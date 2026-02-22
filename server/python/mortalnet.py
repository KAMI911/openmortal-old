#!/usr/bin/env python3
"""
MortalNet Matchmaking Server — Python asyncio implementation.

Protocol (all messages: <1-byte prefix><content>\n):

Client → Server:
  N<nick>       Register / change nickname
  M<text>       Public chat message
  C<target>     Challenge a player
  W<target>     WHOIS query
  L             Disconnect / quit

Server → Client:
  Y<nick>           Your nick confirmed (possibly adjusted)
  J<nick> <ip>      User joined (sent to all; also replayed to new joiner)
  L<nick>           User left
  N<old> <new>      Nick changed
  M<nick> <text>    Chat message (server prepends sender nick)
  S<text>           Server info message
  W<nick> <ip>      WHOIS response
  C<challenger>     You have been challenged

Usage:
    python3 mortalnet.py [options]
    python3 mortalnet.py --help
"""

import asyncio
import argparse
import dataclasses
import http.server
import json
import logging
import re
import signal
import threading
import time
from typing import Dict, Optional

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

CHAT_PORT_DEFAULT = 14883
WEB_PORT_DEFAULT = 8080
MAX_LINE_BYTES = 1024       # Hard cap — disconnect on exceed
NICK_REGEX = re.compile(r'^[a-zA-Z0-9_\-]{1,20}$')
IDLE_TIMEOUT = 300.0        # 5 minutes
WRITE_TIMEOUT = 30.0        # 30 seconds

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

@dataclasses.dataclass
class Config:
    chat_addr: str = f":{CHAT_PORT_DEFAULT}"
    web_addr: str = f":{WEB_PORT_DEFAULT}"
    max_clients: int = 100
    rate: float = 5.0       # token bucket refill rate (msg/s)
    burst: float = 10.0     # token bucket burst size
    strikes: int = 3        # flood strikes before disconnect
    log_level: str = "info"
    log_format: str = "text"

# ---------------------------------------------------------------------------
# Rate limiter (token bucket, no background tasks)
# ---------------------------------------------------------------------------

class TokenBucket:
    def __init__(self, rate: float, burst: float) -> None:
        self._rate = rate
        self._burst = burst
        self._tokens = burst
        self._last = time.monotonic()

    def consume(self) -> bool:
        """Return True if a token was available (request allowed)."""
        now = time.monotonic()
        elapsed = now - self._last
        self._last = now
        self._tokens = min(self._burst, self._tokens + elapsed * self._rate)
        if self._tokens >= 1.0:
            self._tokens -= 1.0
            return True
        return False

# ---------------------------------------------------------------------------
# Per-client state
# ---------------------------------------------------------------------------

_client_id_counter = 0

@dataclasses.dataclass
class Client:
    id: int
    ip: str
    writer: asyncio.StreamWriter
    nick: str = ""
    nick_confirmed: bool = False
    joined_at: float = dataclasses.field(default_factory=time.monotonic)
    last_activity: float = dataclasses.field(default_factory=time.monotonic)
    bucket: TokenBucket = dataclasses.field(default=None)   # set in __post_init__
    strikes: int = 0

    def __post_init__(self) -> None:
        if self.bucket is None:
            object.__setattr__(self, 'bucket', None)  # placeholder; set by server

# ---------------------------------------------------------------------------
# Server
# ---------------------------------------------------------------------------

class MortalNetServer:
    def __init__(self, config: Config) -> None:
        self._cfg = config
        self._clients: Dict[int, Client] = {}       # id → Client
        self._nicks: Dict[str, int] = {}            # nick → client id
        self._start_time = time.monotonic()
        self._lock = threading.Lock()               # protects snapshot for HTTP thread
        self._snapshot_data: dict = {}

    # ------------------------------------------------------------------
    # Public start point
    # ------------------------------------------------------------------

    async def start(self) -> None:
        host, port = _split_addr(self._cfg.chat_addr)
        server = await asyncio.start_server(
            self._handle_client, host or "0.0.0.0", port,
            limit=MAX_LINE_BYTES * 2
        )
        addr = server.sockets[0].getsockname()
        logging.info("MortalNet chat server listening on %s:%s", addr[0], addr[1])
        async with server:
            await server.serve_forever()

    # ------------------------------------------------------------------
    # Connection handler
    # ------------------------------------------------------------------

    async def _handle_client(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        global _client_id_counter
        _client_id_counter += 1
        cid = _client_id_counter

        peer = writer.get_extra_info("peername")
        ip = peer[0] if peer else "0.0.0.0"

        # Enforce max clients
        if len(self._clients) >= self._cfg.max_clients:
            try:
                writer.write(b"SServer is full. Try again later.\n")
                await asyncio.wait_for(writer.drain(), timeout=WRITE_TIMEOUT)
            except Exception:
                pass
            writer.close()
            logging.warning("Connection from %s rejected: server full", ip)
            return

        bucket = TokenBucket(self._cfg.rate, self._cfg.burst)
        client = Client(id=cid, ip=ip, writer=writer)
        client.bucket = bucket
        client.joined_at = time.monotonic()
        client.last_activity = time.monotonic()
        self._clients[cid] = client

        logging.info("Client %d connected from %s", cid, ip)

        try:
            await self._read_loop(client, reader)
        except Exception as exc:
            logging.debug("Client %d read loop error: %s", cid, exc)
        finally:
            await self._on_leave(client)

    # ------------------------------------------------------------------
    # Read loop
    # ------------------------------------------------------------------

    async def _read_loop(self, client: Client, reader: asyncio.StreamReader) -> None:
        while True:
            try:
                raw = await asyncio.wait_for(
                    reader.readline(), timeout=IDLE_TIMEOUT
                )
            except asyncio.TimeoutError:
                logging.info("Client %d idle timeout", client.id)
                return
            except Exception:
                return

            if not raw:
                # EOF
                return

            if len(raw) > MAX_LINE_BYTES:
                logging.warning("Client %d sent oversized line (%d bytes), disconnecting",
                                client.id, len(raw))
                return

            line = raw.rstrip(b"\r\n")
            if len(line) < 1:
                continue

            prefix = chr(line[0])
            content = line[1:].decode("utf-8", errors="replace")
            client.last_activity = time.monotonic()

            # Before nick is confirmed, only allow 'N' and 'L'
            if not client.nick_confirmed and prefix not in ("N", "L"):
                continue

            # Rate limit for message-sending commands
            if prefix in ("M", "C", "W"):
                if not client.bucket.consume():
                    client.strikes += 1
                    logging.debug("Client %d rate limited (strike %d)", client.id, client.strikes)
                    if client.strikes >= self._cfg.strikes:
                        self._send(client, "SYou have been disconnected for flooding.")
                        return
                    continue
                else:
                    # Reset strikes on successful consume
                    client.strikes = 0

            if prefix == "N":
                await self._on_nick(client, content)
            elif prefix == "M":
                await self._on_message(client, content)
            elif prefix == "C":
                await self._on_challenge(client, content)
            elif prefix == "W":
                await self._on_whois(client, content)
            elif prefix == "L":
                return
            else:
                logging.debug("Client %d unknown prefix '%s'", client.id, prefix)

    # ------------------------------------------------------------------
    # Protocol handlers
    # ------------------------------------------------------------------

    async def _on_nick(self, client: Client, requested: str) -> None:
        new_nick = self._resolve_nick(requested, exclude_id=client.id)

        if client.nick_confirmed:
            # Nick change
            old_nick = client.nick
            if new_nick == old_nick:
                return
            del self._nicks[old_nick]
            self._nicks[new_nick] = client.id
            client.nick = new_nick
            self._send(client, f"Y{new_nick}")
            self._broadcast(f"N{old_nick} {new_nick}")
            logging.info("Client %d changed nick: %s → %s", client.id, old_nick, new_nick)
        else:
            # First nick registration
            client.nick = new_nick
            client.nick_confirmed = True
            self._nicks[new_nick] = client.id

            # 1. Confirm nick to new client
            self._send(client, f"Y{new_nick}")

            # 2. Send existing users to new client
            for other in list(self._clients.values()):
                if other.id != client.id and other.nick_confirmed:
                    self._send(client, f"J{other.nick} {other.ip}")

            # 3. Announce new client to everyone else
            self._broadcast(f"J{new_nick} {client.ip}", exclude_id=client.id)

            logging.info("Client %d registered as '%s' from %s", client.id, new_nick, client.ip)
            self._update_snapshot()

    async def _on_message(self, client: Client, text: str) -> None:
        text = _sanitize(text)
        if not text:
            return
        self._broadcast(f"M{client.nick} {text}")

    async def _on_challenge(self, client: Client, target_nick: str) -> None:
        target_nick = target_nick.strip()
        if target_nick == client.nick:
            self._send(client, "SYou cannot challenge yourself.")
            return
        target_id = self._nicks.get(target_nick)
        if target_id is None:
            self._send(client, f"SNo such user: {target_nick}")
            return
        target = self._clients.get(target_id)
        if target is None:
            self._send(client, f"SNo such user: {target_nick}")
            return
        self._send(target, f"C{client.nick}")
        logging.info("Client %d ('%s') challenged '%s'", client.id, client.nick, target_nick)

    async def _on_whois(self, client: Client, target_nick: str) -> None:
        target_nick = target_nick.strip()
        target_id = self._nicks.get(target_nick)
        if target_id is None:
            self._send(client, f"SNo such user: {target_nick}")
            return
        target = self._clients.get(target_id)
        if target is None:
            self._send(client, f"SNo such user: {target_nick}")
            return
        self._send(client, f"W{target.nick} {target.ip}")

    async def _on_leave(self, client: Client) -> None:
        if client.id not in self._clients:
            return
        del self._clients[client.id]
        if client.nick_confirmed and client.nick in self._nicks:
            del self._nicks[client.nick]
            self._broadcast(f"L{client.nick}")
            logging.info("Client %d ('%s') left", client.id, client.nick)
        else:
            logging.info("Client %d (unregistered) disconnected", client.id)

        try:
            client.writer.close()
            await asyncio.wait_for(client.writer.wait_closed(), timeout=5.0)
        except Exception:
            pass

        self._update_snapshot()

    # ------------------------------------------------------------------
    # I/O helpers
    # ------------------------------------------------------------------

    def _send(self, client: Client, msg: str) -> None:
        """Queue a message to a single client (fire-and-forget)."""
        try:
            data = (msg + "\n").encode("utf-8")
            client.writer.write(data)
            # Schedule drain without awaiting (asyncio will flush)
            loop = asyncio.get_event_loop()
            loop.create_task(self._drain(client))
        except Exception as exc:
            logging.debug("Send to client %d failed: %s", client.id, exc)

    async def _drain(self, client: Client) -> None:
        try:
            await asyncio.wait_for(client.writer.drain(), timeout=WRITE_TIMEOUT)
        except Exception:
            # If drain fails, the read loop will catch the disconnect
            pass

    def _broadcast(self, msg: str, exclude_id: Optional[int] = None) -> None:
        for client in list(self._clients.values()):
            if client.nick_confirmed and client.id != exclude_id:
                self._send(client, msg)

    # ------------------------------------------------------------------
    # Nick resolution
    # ------------------------------------------------------------------

    def _resolve_nick(self, requested: str, exclude_id: Optional[int] = None) -> str:
        """Return a valid, unique nick derived from the requested string."""
        # Sanitize: keep only allowed characters
        clean = re.sub(r'[^a-zA-Z0-9_\-]', '', requested)[:20]
        if not clean:
            clean = "Player"

        # Make unique
        base = clean
        suffix = 1
        candidate = base
        while candidate in self._nicks and self._nicks[candidate] != exclude_id:
            candidate = f"{base[:17]}_{suffix}"
            suffix += 1
        return candidate

    # ------------------------------------------------------------------
    # Dashboard snapshot (called from asyncio thread, read by HTTP thread)
    # ------------------------------------------------------------------

    def _update_snapshot(self) -> None:
        players = []
        now = time.monotonic()
        for c in self._clients.values():
            if c.nick_confirmed:
                players.append({
                    "nick": c.nick,
                    "ip": c.ip,
                    "joined_at": int(c.joined_at),
                    "idle_seconds": int(now - c.last_activity),
                })
        data = {
            "uptime_seconds": int(now - self._start_time),
            "player_count": len(players),
            "players": players,
        }
        with self._lock:
            self._snapshot_data = data

    def snapshot(self) -> dict:
        """Thread-safe snapshot for the HTTP dashboard thread."""
        self._update_snapshot()
        with self._lock:
            return dict(self._snapshot_data)


# ---------------------------------------------------------------------------
# HTTP dashboard (runs in a background thread)
# ---------------------------------------------------------------------------

_DASHBOARD_HTML = """\
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta http-equiv="refresh" content="10">
<title>MortalNet Status</title>
<style>
  body {{ font-family: monospace; background: #111; color: #ccc; padding: 2em; }}
  h1 {{ color: #f80; }}
  table {{ border-collapse: collapse; width: 100%; }}
  th, td {{ border: 1px solid #444; padding: 0.4em 0.8em; text-align: left; }}
  th {{ color: #f80; background: #222; }}
  tr:nth-child(even) {{ background: #1a1a1a; }}
  .meta {{ color: #888; margin-bottom: 1em; }}
</style>
</head>
<body>
<h1>MortalNet Status</h1>
<p class="meta">Uptime: {uptime}s &mdash; Players online: {count}</p>
<table>
<tr><th>Nick</th><th>IP</th><th>Idle (s)</th></tr>
{rows}
</table>
</body>
</html>
"""

def _make_dashboard_handler(server: MortalNetServer):
    class DashboardHandler(http.server.BaseHTTPRequestHandler):
        _server_ref = server

        def log_message(self, fmt, *args):  # silence access log (use our logger)
            logging.debug("HTTP %s %s", self.address_string(), fmt % args)

        def _set_security_headers(self):
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("X-Frame-Options", "DENY")
            self.send_header("Cache-Control", "no-store")

        def do_HEAD(self):
            self._dispatch(head_only=True)

        def do_GET(self):
            self._dispatch(head_only=False)

        def do_POST(self):
            self._method_not_allowed()

        def do_PUT(self):
            self._method_not_allowed()

        def do_DELETE(self):
            self._method_not_allowed()

        def _method_not_allowed(self):
            self.send_response(405)
            self.send_header("Allow", "GET, HEAD")
            self._set_security_headers()
            self.end_headers()

        def _dispatch(self, head_only: bool):
            path = self.path.split("?", 1)[0]
            if path == "/" or path == "":
                self._serve_html(head_only)
            elif path == "/api/status":
                self._serve_json(head_only)
            elif path == "/healthz":
                self._serve_health(head_only)
            else:
                self.send_response(404)
                self._set_security_headers()
                self.send_header("Content-Type", "text/plain")
                self.end_headers()
                if not head_only:
                    self.wfile.write(b"Not found\n")

        def _serve_health(self, head_only: bool):
            body = b"OK\n"
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self._set_security_headers()
            self.end_headers()
            if not head_only:
                self.wfile.write(body)

        def _serve_json(self, head_only: bool):
            data = self._server_ref.snapshot()
            body = json.dumps(data, indent=2).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self._set_security_headers()
            self.end_headers()
            if not head_only:
                self.wfile.write(body)

        def _serve_html(self, head_only: bool):
            data = self._server_ref.snapshot()
            rows = "\n".join(
                f"<tr><td>{p['nick']}</td><td>{p['ip']}</td><td>{p['idle_seconds']}</td></tr>"
                for p in data.get("players", [])
            ) or "<tr><td colspan='3'>No players online</td></tr>"
            html = _DASHBOARD_HTML.format(
                uptime=data.get("uptime_seconds", 0),
                count=data.get("player_count", 0),
                rows=rows,
            ).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(html)))
            self._set_security_headers()
            self.end_headers()
            if not head_only:
                self.wfile.write(html)

    return DashboardHandler


def _run_dashboard(server: MortalNetServer, cfg: Config) -> None:
    host, port = _split_addr(cfg.web_addr)
    handler = _make_dashboard_handler(server)
    httpd = http.server.HTTPServer((host or "0.0.0.0", port), handler)
    logging.info("MortalNet dashboard listening on %s:%s", host or "0.0.0.0", port)
    httpd.serve_forever()


# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------

def _split_addr(addr: str):
    """Split ':port' or 'host:port' into (host, int(port))."""
    if addr.startswith(":"):
        return "", int(addr[1:])
    parts = addr.rsplit(":", 1)
    if len(parts) == 2:
        return parts[0], int(parts[1])
    return addr, CHAT_PORT_DEFAULT


def _sanitize(text: str) -> str:
    """Strip control characters < 0x20 (except space)."""
    return "".join(ch for ch in text if ord(ch) >= 0x20)


def _setup_logging(cfg: Config) -> None:
    level = getattr(logging, cfg.log_level.upper(), logging.INFO)
    if cfg.log_format == "json":
        fmt = '{"time":"%(asctime)s","level":"%(levelname)s","msg":"%(message)s"}'
    else:
        fmt = "%(asctime)s %(levelname)-8s %(message)s"
    logging.basicConfig(level=level, format=fmt)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _parse_args() -> Config:
    p = argparse.ArgumentParser(
        description="MortalNet matchmaking server",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--chat-addr", default=f":{CHAT_PORT_DEFAULT}",
                   help="TCP listen address for chat (e.g. ':14883')")
    p.add_argument("--web-addr", default=f":{WEB_PORT_DEFAULT}",
                   help="HTTP dashboard listen address")
    p.add_argument("--max-clients", type=int, default=100)
    p.add_argument("--rate", type=float, default=5.0,
                   help="Token bucket refill rate (msg/s)")
    p.add_argument("--burst", type=float, default=10.0,
                   help="Token bucket burst size")
    p.add_argument("--strikes", type=int, default=3,
                   help="Flood strikes before disconnect")
    p.add_argument("--log-level", default="info",
                   choices=["debug", "info", "warn", "error"])
    p.add_argument("--log-format", default="text", choices=["text", "json"])
    ns = p.parse_args()
    return Config(
        chat_addr=ns.chat_addr,
        web_addr=ns.web_addr,
        max_clients=ns.max_clients,
        rate=ns.rate,
        burst=ns.burst,
        strikes=ns.strikes,
        log_level=ns.log_level,
        log_format=ns.log_format,
    )


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    cfg = _parse_args()
    _setup_logging(cfg)

    server = MortalNetServer(cfg)

    # Start dashboard in background thread
    t = threading.Thread(target=_run_dashboard, args=(server, cfg), daemon=True)
    t.start()

    # Register signal handlers
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    def _shutdown():
        logging.info("Shutting down MortalNet server…")
        loop.stop()

    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, _shutdown)

    try:
        loop.run_until_complete(server.start())
    except RuntimeError:
        pass  # loop.stop() was called
    finally:
        loop.close()
        logging.info("MortalNet server stopped.")


if __name__ == "__main__":
    main()
