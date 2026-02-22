#!/usr/bin/env python3
"""
MortalNet Matchmaking Server — Python asyncio implementation.

Protocol (all messages: <1-byte prefix><content>\n):

Client → Server:
  N<nick>                  Register / change nickname
  M<text>                  Public chat message
  C<target>                Challenge a player
  W<target>                WHOIS query
  T<status>                Set status: chat | away | game | queue
  A<pass> <cmd> [args]     Admin command (kick/ban/reload/motd)
  L                        Disconnect / quit

Server → Client:
  Y<nick>                  Your nick confirmed (possibly adjusted)
  J<nick> <ip>             User joined
  L<nick>                  User left
  N<old> <new>             Nick changed
  M<nick> <text>           Chat message
  S<text>                  Server info / error
  W<nick> <ip>             WHOIS response
  C<challenger>            You have been challenged
  T<nick> <status>         Status changed broadcast

Usage:
    python3 mortalnet.py [--help]
"""

import asyncio
import argparse
import dataclasses
import http.server
import json
import logging
import os
import re
import signal
import ssl
import threading
import time
from typing import Dict, List, Optional, Set, Tuple

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

CHAT_PORT_DEFAULT = 14883
WEB_PORT_DEFAULT  = 8080
MAX_LINE_BYTES    = 1024
NICK_REGEX        = re.compile(r'^[a-zA-Z0-9_\-]{1,20}$')
IDLE_TIMEOUT      = 300.0   # seconds — sliding read deadline
WRITE_TIMEOUT     = 30.0    # seconds — per-write deadline
VALID_STATUSES    = {"chat", "away", "game", "queue"}

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

@dataclasses.dataclass
class Config:
    # Network
    chat_addr:   str   = f":{CHAT_PORT_DEFAULT}"
    web_addr:    str   = f":{WEB_PORT_DEFAULT}"
    max_clients: int   = 100
    # Rate limiting
    rate:        float = 5.0
    burst:       float = 10.0
    strikes:     int   = 3
    # Logging
    log_level:   str   = "info"
    log_format:  str   = "text"
    # MOTD
    motd:        str   = ""
    motd_file:   str   = ""
    # Chat history
    history_size: int  = 20
    # Nick reservation
    nick_reserve_secs: int = 60
    # Persistent stats
    stats_file:  str   = ""
    # Admin
    admin_password: str = ""
    # Ban list
    ban_file:    str   = ""
    # TLS
    tls_cert:    str   = ""
    tls_key:     str   = ""

# ---------------------------------------------------------------------------
# Token bucket
# ---------------------------------------------------------------------------

class TokenBucket:
    def __init__(self, rate: float, burst: float) -> None:
        self._rate   = rate
        self._burst  = burst
        self._tokens = burst
        self._last   = time.monotonic()

    def consume(self) -> bool:
        now           = time.monotonic()
        self._tokens  = min(self._burst, self._tokens + (now - self._last) * self._rate)
        self._last    = now
        if self._tokens >= 1.0:
            self._tokens -= 1.0
            return True
        return False

# ---------------------------------------------------------------------------
# Client
# ---------------------------------------------------------------------------

_client_id_counter = 0

@dataclasses.dataclass
class Client:
    id:             int
    ip:             str
    writer:         asyncio.StreamWriter
    nick:           str   = ""
    nick_confirmed: bool  = False
    status:         str   = "chat"
    joined_at:      float = dataclasses.field(default_factory=time.monotonic)
    last_activity:  float = dataclasses.field(default_factory=time.monotonic)
    bucket:         object = None   # TokenBucket, set after construction
    strikes:        int   = 0

# ---------------------------------------------------------------------------
# Server
# ---------------------------------------------------------------------------

class MortalNetServer:
    def __init__(self, cfg: Config) -> None:
        self._cfg          = cfg
        self._clients:     Dict[int, Client]          = {}
        self._nicks:       Dict[str, int]             = {}
        # nick → (ip, expiry_monotonic)
        self._reserved:    Dict[str, Tuple[str,float]] = {}
        self._history:     List[str]                  = []
        self._banned_ips:  Set[str]                   = set()
        self._start_time   = time.monotonic()
        self._lock         = threading.Lock()
        self._snapshot_data: dict = {}
        # Metrics (in-process counters)
        self._metrics = {
            "connections_total":  0,
            "messages_total":     0,
            "challenges_total":   0,
            "kicks_total":        0,
            "bans_total":         0,
        }
        # Persistent stats (loaded from disk)
        self._stats: dict = self._load_stats()
        # MOTD string
        self._motd: str = self._load_motd()
        # Ban list
        self._load_ban_list()

    # ------------------------------------------------------------------
    # Start
    # ------------------------------------------------------------------

    async def start(self) -> None:
        host, port = _split_addr(self._cfg.chat_addr)

        ssl_ctx = None
        if self._cfg.tls_cert and self._cfg.tls_key:
            ssl_ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            ssl_ctx.load_cert_chain(self._cfg.tls_cert, self._cfg.tls_key)
            logging.info("TLS enabled")

        server = await asyncio.start_server(
            self._handle_client,
            host or "0.0.0.0",
            port,
            limit=MAX_LINE_BYTES * 2,
            ssl=ssl_ctx,
        )
        addr = server.sockets[0].getsockname()
        logging.info("MortalNet chat listening on %s:%s%s",
                     addr[0], addr[1], " (TLS)" if ssl_ctx else "")

        loop = asyncio.get_event_loop()
        try:
            loop.add_signal_handler(signal.SIGHUP, self._on_sighup)
        except (AttributeError, OSError):
            pass  # Windows

        async with server:
            await server.serve_forever()

    def _on_sighup(self) -> None:
        logging.info("SIGHUP: reloading ban list and MOTD")
        self._load_ban_list()
        self._motd = self._load_motd()

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
        ip   = peer[0] if peer else "0.0.0.0"

        # Ban check
        if ip in self._banned_ips:
            logging.info("Rejected banned IP %s", ip)
            try:
                writer.write(b"SYou are banned from this server.\n")
                await asyncio.wait_for(writer.drain(), timeout=WRITE_TIMEOUT)
            except Exception:
                pass
            writer.close()
            return

        # Max clients check
        if len(self._clients) >= self._cfg.max_clients:
            try:
                writer.write(b"SServer is full. Try again later.\n")
                await asyncio.wait_for(writer.drain(), timeout=WRITE_TIMEOUT)
            except Exception:
                pass
            writer.close()
            logging.warning("Rejected %s: server full", ip)
            return

        self._metrics["connections_total"] += 1
        self._stats["total_connections"] = self._stats.get("total_connections", 0) + 1

        client        = Client(id=cid, ip=ip, writer=writer)
        client.bucket = TokenBucket(self._cfg.rate, self._cfg.burst)
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
                raw = await asyncio.wait_for(reader.readline(), timeout=IDLE_TIMEOUT)
            except asyncio.TimeoutError:
                logging.info("Client %d idle timeout", client.id)
                return
            except Exception:
                return

            if not raw:
                return
            if len(raw) > MAX_LINE_BYTES:
                logging.warning("Client %d oversized line, disconnecting", client.id)
                return

            line    = raw.rstrip(b"\r\n")
            if not line:
                continue

            prefix  = chr(line[0])
            content = line[1:].decode("utf-8", errors="replace")
            client.last_activity = time.monotonic()

            # Only N and L allowed before nick confirmation
            if not client.nick_confirmed and prefix not in ("N", "L"):
                continue

            # Rate-limit message-producing commands
            if prefix in ("M", "C", "W", "T"):
                if not client.bucket.consume():
                    client.strikes += 1
                    logging.debug("Client %d rate-limited (strike %d)", client.id, client.strikes)
                    if client.strikes >= self._cfg.strikes:
                        self._send(client, "SYou have been disconnected for flooding.")
                        return
                    continue
                else:
                    client.strikes = 0

            if   prefix == "N": await self._on_nick(client, content)
            elif prefix == "M": await self._on_message(client, content)
            elif prefix == "C": await self._on_challenge(client, content)
            elif prefix == "W": await self._on_whois(client, content)
            elif prefix == "T": await self._on_status(client, content)
            elif prefix == "A": await self._on_admin(client, content)
            elif prefix == "L": return

    # ------------------------------------------------------------------
    # Protocol handlers
    # ------------------------------------------------------------------

    async def _on_nick(self, client: Client, requested: str) -> None:
        new_nick = self._resolve_nick(requested, exclude_id=client.id, client_ip=client.ip)

        if client.nick_confirmed:
            old_nick = client.nick
            if new_nick == old_nick:
                return
            del self._nicks[old_nick]
            self._nicks[new_nick] = client.id
            client.nick = new_nick
            self._send(client, f"Y{new_nick}")
            self._broadcast(f"N{old_nick} {new_nick}")
            logging.info("Client %d nick: %s → %s", client.id, old_nick, new_nick)
        else:
            # First registration
            client.nick           = new_nick
            client.nick_confirmed = True
            self._nicks[new_nick] = client.id
            self._reserved.pop(new_nick, None)

            # Stats
            self._bump_player_stat(new_nick, "connect_count")
            self._save_stats()

            # 1. Confirm nick
            self._send(client, f"Y{new_nick}")

            # 2. Existing users → new client
            for other in list(self._clients.values()):
                if other.id != client.id and other.nick_confirmed:
                    self._send(client, f"J{other.nick} {other.ip}")

            # 3. Chat history → new client
            for msg in self._history:
                self._send(client, msg)

            # 4. MOTD
            if self._motd:
                for line in self._motd.splitlines():
                    line = line.strip()
                    if line:
                        self._send(client, f"S{line}")

            # 5. Announce new client to everyone else
            self._broadcast(f"J{new_nick} {client.ip}", exclude_id=client.id)
            logging.info("Client %d registered as '%s' from %s", client.id, new_nick, client.ip)
            self._update_snapshot()

    async def _on_message(self, client: Client, text: str) -> None:
        text = _sanitize(text)
        if not text:
            return
        msg = f"M{client.nick} {text}"
        # History
        self._history.append(msg)
        if len(self._history) > self._cfg.history_size:
            self._history.pop(0)
        self._broadcast(msg)
        # Metrics / stats
        self._metrics["messages_total"] += 1
        self._stats["total_messages"] = self._stats.get("total_messages", 0) + 1
        self._bump_player_stat(client.nick, "message_count")
        if self._stats["total_messages"] % 20 == 0:
            self._save_stats()

    async def _on_challenge(self, client: Client, target_nick: str) -> None:
        target_nick = target_nick.strip()
        if target_nick == client.nick:
            self._send(client, "SYou cannot challenge yourself.")
            return
        target = self._get_client_by_nick(target_nick)
        if target is None:
            self._send(client, f"SNo such user: {target_nick}")
            return
        self._send(target, f"C{client.nick}")
        self._metrics["challenges_total"] += 1
        self._stats["total_challenges"] = self._stats.get("total_challenges", 0) + 1
        self._bump_player_stat(client.nick, "challenge_sent_count")
        self._bump_player_stat(target_nick,  "challenge_received_count")
        logging.info("'%s' challenged '%s'", client.nick, target_nick)

    async def _on_whois(self, client: Client, target_nick: str) -> None:
        target_nick = target_nick.strip()
        target = self._get_client_by_nick(target_nick)
        if target is None:
            self._send(client, f"SNo such user: {target_nick}")
            return
        self._send(client, f"W{target.nick} {target.ip}")

    async def _on_status(self, client: Client, status: str) -> None:
        status = status.strip().lower()
        if status not in VALID_STATUSES:
            self._send(client, f"SInvalid status. Choose: {', '.join(sorted(VALID_STATUSES))}")
            return
        old_status    = client.status
        client.status = status
        self._broadcast(f"T{client.nick} {status}")
        logging.info("'%s' status: %s → %s", client.nick, old_status, status)
        if status == "queue":
            self._try_matchmake(client)

    def _try_matchmake(self, joiner: Client) -> None:
        """Auto-challenge two queued players."""
        for other in list(self._clients.values()):
            if other.id == joiner.id or not other.nick_confirmed:
                continue
            if other.status != "queue":
                continue
            # Match found — challenge each other and reset status
            self._send(joiner, f"C{other.nick}")
            self._send(other,  f"C{joiner.nick}")
            for c in (joiner, other):
                c.status = "chat"
                self._broadcast(f"T{c.nick} chat")
            self._send(joiner, f"SMatchmaking: paired with {other.nick}!")
            self._send(other,  f"SMatchmaking: paired with {joiner.nick}!")
            self._metrics["challenges_total"] += 1
            logging.info("Matchmaking: '%s' ↔ '%s'", joiner.nick, other.nick)
            return

    async def _on_admin(self, client: Client, content: str) -> None:
        if not self._cfg.admin_password:
            self._send(client, "SAdmin commands are disabled on this server.")
            return

        parts = content.split(" ", 2)
        if len(parts) < 2:
            self._send(client, "SUsage: A<password> <kick|ban|reload|motd> [args]")
            return

        password, cmd = parts[0], parts[1].lower()
        args = parts[2].strip() if len(parts) > 2 else ""

        if password != self._cfg.admin_password:
            self._send(client, "SInvalid admin password.")
            logging.warning("Failed admin attempt from '%s' (%s)", client.nick, client.ip)
            return

        if cmd == "kick":
            await self._admin_kick(client, args)
        elif cmd == "ban":
            await self._admin_ban(client, args)
        elif cmd == "reload":
            self._load_ban_list()
            self._motd = self._load_motd()
            self._send(client, "SReloaded ban list and MOTD.")
            logging.info("Admin '%s' reloaded config", client.nick)
        elif cmd == "motd":
            self._motd = args
            self._send(client, "SMOTD updated.")
            logging.info("Admin '%s' set MOTD: %s", client.nick, args)
        else:
            self._send(client, f"SUnknown command: {cmd}")

    async def _admin_kick(self, admin: Client, target_nick: str) -> None:
        target = self._get_client_by_nick(target_nick)
        if target is None:
            self._send(admin, f"SNo such user: {target_nick}")
            return
        self._send(target, "SYou have been kicked by an administrator.")
        try:
            target.writer.close()
        except Exception:
            pass
        self._metrics["kicks_total"] += 1
        self._send(admin, f"SKicked {target_nick}.")
        logging.info("Admin '%s' kicked '%s'", admin.nick, target_nick)

    async def _admin_ban(self, admin: Client, arg: str) -> None:
        """Ban by nick or raw IP."""
        ip = arg
        target = self._get_client_by_nick(arg)
        if target is not None:
            ip = target.ip
            await self._admin_kick(admin, arg)

        self._banned_ips.add(ip)
        self._metrics["bans_total"] += 1

        if self._cfg.ban_file:
            try:
                with open(self._cfg.ban_file, "a") as f:
                    f.write(f"{ip}\n")
            except Exception as exc:
                logging.warning("Could not write ban file: %s", exc)

        self._send(admin, f"SBanned {ip}.")
        logging.info("Admin '%s' banned %s", admin.nick, ip)

    async def _on_leave(self, client: Client) -> None:
        if client.id not in self._clients:
            return
        del self._clients[client.id]

        if client.nick_confirmed and client.nick in self._nicks:
            del self._nicks[client.nick]
            # Reserve the nick for a grace period
            if self._cfg.nick_reserve_secs > 0:
                self._reserved[client.nick] = (
                    client.ip,
                    time.monotonic() + self._cfg.nick_reserve_secs,
                )
            self._broadcast(f"L{client.nick}")
            self._bump_player_stat(client.nick, "")  # update last_seen
            self._save_stats()
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
        try:
            client.writer.write((msg + "\n").encode("utf-8"))
            loop = asyncio.get_event_loop()
            loop.create_task(self._drain(client))
        except Exception as exc:
            logging.debug("Send to client %d failed: %s", client.id, exc)

    async def _drain(self, client: Client) -> None:
        try:
            await asyncio.wait_for(client.writer.drain(), timeout=WRITE_TIMEOUT)
        except Exception:
            pass

    def _broadcast(self, msg: str, exclude_id: Optional[int] = None) -> None:
        for client in list(self._clients.values()):
            if client.nick_confirmed and client.id != exclude_id:
                self._send(client, msg)

    def _get_client_by_nick(self, nick: str) -> Optional[Client]:
        cid = self._nicks.get(nick)
        return self._clients.get(cid) if cid is not None else None

    # ------------------------------------------------------------------
    # Nick resolution
    # ------------------------------------------------------------------

    def _resolve_nick(self, requested: str,
                      exclude_id: Optional[int] = None,
                      client_ip: str = "") -> str:
        clean = re.sub(r'[^a-zA-Z0-9_\-]', '', requested)[:20] or "Player"
        base, suffix, candidate = clean, 1, clean

        while True:
            active_owner = self._nicks.get(candidate)
            if active_owner is not None and active_owner != exclude_id:
                # Taken by an active user — try next
                pass
            else:
                res = self._reserved.get(candidate)
                if res is not None:
                    res_ip, expiry = res
                    if time.monotonic() < expiry:
                        # Reserved: allow only if same IP reconnects
                        if client_ip and client_ip != res_ip:
                            pass  # Different IP, try next suffix
                        else:
                            return candidate
                    else:
                        self._reserved.pop(candidate, None)
                        return candidate
                else:
                    return candidate

            candidate = f"{base[:17]}_{suffix}"
            suffix += 1

    # ------------------------------------------------------------------
    # Persistent stats
    # ------------------------------------------------------------------

    def _load_stats(self) -> dict:
        if self._cfg.stats_file:
            try:
                with open(self._cfg.stats_file) as f:
                    return json.load(f)
            except FileNotFoundError:
                pass
            except Exception as exc:
                logging.warning("Could not load stats: %s", exc)
        return {
            "server_start":      time.time(),
            "total_connections": 0,
            "total_messages":    0,
            "total_challenges":  0,
            "players":           {},
        }

    def _save_stats(self) -> None:
        if not self._cfg.stats_file:
            return
        try:
            tmp = self._cfg.stats_file + ".tmp"
            with open(tmp, "w") as f:
                json.dump(self._stats, f, indent=2)
            os.replace(tmp, self._cfg.stats_file)
        except Exception as exc:
            logging.warning("Could not save stats: %s", exc)

    def _bump_player_stat(self, nick: str, key: str) -> None:
        """Increment a per-player counter and refresh last_seen."""
        if not self._cfg.stats_file:
            return
        now = time.time()
        players = self._stats.setdefault("players", {})
        p = players.setdefault(nick, {
            "first_seen":              now,
            "last_seen":               now,
            "connect_count":           0,
            "message_count":           0,
            "challenge_sent_count":    0,
            "challenge_received_count": 0,
        })
        p["last_seen"] = now
        if key:
            p[key] = p.get(key, 0) + 1

    # ------------------------------------------------------------------
    # Ban list
    # ------------------------------------------------------------------

    def _load_ban_list(self) -> None:
        if not self._cfg.ban_file:
            return
        try:
            ips: Set[str] = set()
            with open(self._cfg.ban_file) as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith("#"):
                        ips.add(line)
            self._banned_ips = ips
            logging.info("Loaded %d banned IPs", len(ips))
        except FileNotFoundError:
            pass
        except Exception as exc:
            logging.warning("Could not load ban list: %s", exc)

    # ------------------------------------------------------------------
    # MOTD
    # ------------------------------------------------------------------

    def _load_motd(self) -> str:
        if self._cfg.motd_file:
            try:
                with open(self._cfg.motd_file) as f:
                    return f.read().strip()
            except Exception as exc:
                logging.warning("Could not load MOTD file: %s", exc)
        return self._cfg.motd

    # ------------------------------------------------------------------
    # Snapshot & metrics (thread-safe for HTTP thread)
    # ------------------------------------------------------------------

    def _update_snapshot(self) -> None:
        now     = time.monotonic()
        players = [
            {
                "nick":         c.nick,
                "ip":           c.ip,
                "status":       c.status,
                "joined_at":    int(c.joined_at),
                "idle_seconds": int(now - c.last_activity),
            }
            for c in self._clients.values()
            if c.nick_confirmed
        ]
        data = {
            "uptime_seconds": int(now - self._start_time),
            "player_count":   len(players),
            "players":        players,
            "metrics":        dict(self._metrics),
        }
        with self._lock:
            self._snapshot_data = data

    def snapshot(self) -> dict:
        self._update_snapshot()
        with self._lock:
            return dict(self._snapshot_data)

    def metrics_text(self) -> str:
        """Prometheus exposition format."""
        snap = self.snapshot()
        m    = snap["metrics"]
        lines = [
            "# HELP mortalnet_connections_total Total TCP connections accepted",
            "# TYPE mortalnet_connections_total counter",
            f"mortalnet_connections_total {m['connections_total']}",
            "",
            "# HELP mortalnet_active_players Currently registered players",
            "# TYPE mortalnet_active_players gauge",
            f"mortalnet_active_players {snap['player_count']}",
            "",
            "# HELP mortalnet_messages_total Total chat messages processed",
            "# TYPE mortalnet_messages_total counter",
            f"mortalnet_messages_total {m['messages_total']}",
            "",
            "# HELP mortalnet_challenges_total Total challenges sent",
            "# TYPE mortalnet_challenges_total counter",
            f"mortalnet_challenges_total {m['challenges_total']}",
            "",
            "# HELP mortalnet_kicks_total Total admin kicks",
            "# TYPE mortalnet_kicks_total counter",
            f"mortalnet_kicks_total {m['kicks_total']}",
            "",
            "# HELP mortalnet_bans_total Total admin bans",
            "# TYPE mortalnet_bans_total counter",
            f"mortalnet_bans_total {m['bans_total']}",
            "",
            "# HELP mortalnet_uptime_seconds Server uptime",
            "# TYPE mortalnet_uptime_seconds gauge",
            f"mortalnet_uptime_seconds {snap['uptime_seconds']}",
            "",
        ]
        return "\n".join(lines)

    def stats(self) -> dict:
        return dict(self._stats)


# ---------------------------------------------------------------------------
# HTTP dashboard
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
  table {{ border-collapse: collapse; width: 100%; margin-bottom: 2em; }}
  th, td {{ border: 1px solid #444; padding: 0.4em 0.8em; text-align: left; }}
  th {{ color: #f80; background: #222; }}
  tr:nth-child(even) {{ background: #1a1a1a; }}
  .meta {{ color: #888; margin-bottom: 1em; }}
  .status-chat  {{ color: #8f8; }}
  .status-away  {{ color: #fa0; }}
  .status-game  {{ color: #88f; }}
  .status-queue {{ color: #f88; }}
</style>
</head>
<body>
<h1>MortalNet Status</h1>
<p class="meta">Uptime: {uptime}s &mdash; Players online: {count}</p>
<table>
<tr><th>Nick</th><th>IP</th><th>Status</th><th>Idle (s)</th></tr>
{rows}
</table>
</body>
</html>
"""

def _make_handler(server: MortalNetServer):
    class Handler(http.server.BaseHTTPRequestHandler):
        _srv = server

        def log_message(self, fmt, *args):
            logging.debug("HTTP %s %s", self.address_string(), fmt % args)

        def _security_headers(self):
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("X-Frame-Options", "DENY")
            self.send_header("Cache-Control", "no-store")

        def do_HEAD(self):   self._dispatch(head_only=True)
        def do_GET(self):    self._dispatch(head_only=False)
        def do_POST(self):   self._method_not_allowed()
        def do_PUT(self):    self._method_not_allowed()
        def do_DELETE(self): self._method_not_allowed()

        def _method_not_allowed(self):
            self.send_response(405)
            self.send_header("Allow", "GET, HEAD")
            self._security_headers()
            self.end_headers()

        def _dispatch(self, head_only: bool):
            path = self.path.split("?", 1)[0]
            if path in ("/", ""):       self._serve_html(head_only)
            elif path == "/api/status": self._serve_json(head_only)
            elif path == "/api/stats":  self._serve_stats(head_only)
            elif path == "/metrics":    self._serve_metrics(head_only)
            elif path == "/healthz":    self._serve_health(head_only)
            else:
                self.send_response(404)
                self._security_headers()
                self.send_header("Content-Type", "text/plain")
                self.end_headers()
                if not head_only:
                    self.wfile.write(b"Not found\n")

        def _send_body(self, code: int, ctype: str, body: bytes, head_only: bool):
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self._security_headers()
            self.end_headers()
            if not head_only:
                self.wfile.write(body)

        def _serve_health(self, head_only: bool):
            self._send_body(200, "text/plain", b"OK\n", head_only)

        def _serve_json(self, head_only: bool):
            body = json.dumps(self._srv.snapshot(), indent=2).encode()
            self._send_body(200, "application/json", body, head_only)

        def _serve_stats(self, head_only: bool):
            body = json.dumps(self._srv.stats(), indent=2).encode()
            self._send_body(200, "application/json", body, head_only)

        def _serve_metrics(self, head_only: bool):
            body = self._srv.metrics_text().encode()
            self._send_body(200, "text/plain; version=0.0.4", body, head_only)

        def _serve_html(self, head_only: bool):
            snap  = self._srv.snapshot()
            rows  = "\n".join(
                f"<tr><td>{p['nick']}</td><td>{p['ip']}</td>"
                f"<td class='status-{p['status']}'>{p['status']}</td>"
                f"<td>{p['idle_seconds']}</td></tr>"
                for p in snap.get("players", [])
            ) or "<tr><td colspan='4'>No players online</td></tr>"
            html = _DASHBOARD_HTML.format(
                uptime=snap.get("uptime_seconds", 0),
                count=snap.get("player_count", 0),
                rows=rows,
            ).encode()
            self._send_body(200, "text/html; charset=utf-8", html, head_only)

    return Handler


def _run_dashboard(server: MortalNetServer, cfg: Config) -> None:
    host, port = _split_addr(cfg.web_addr)
    httpd = http.server.HTTPServer((host or "0.0.0.0", port), _make_handler(server))
    logging.info("MortalNet dashboard listening on %s:%s", host or "0.0.0.0", port)
    httpd.serve_forever()


# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------

def _split_addr(addr: str):
    if addr.startswith(":"):
        return "", int(addr[1:])
    parts = addr.rsplit(":", 1)
    return (parts[0], int(parts[1])) if len(parts) == 2 else (addr, CHAT_PORT_DEFAULT)


def _sanitize(text: str) -> str:
    return "".join(ch for ch in text if ord(ch) >= 0x20)


def _setup_logging(cfg: Config) -> None:
    level = getattr(logging, cfg.log_level.upper(), logging.INFO)
    fmt   = ('{"time":"%(asctime)s","level":"%(levelname)s","msg":"%(message)s"}'
             if cfg.log_format == "json"
             else "%(asctime)s %(levelname)-8s %(message)s")
    logging.basicConfig(level=level, format=fmt)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _parse_args() -> Config:
    p = argparse.ArgumentParser(
        description="MortalNet matchmaking server",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--chat-addr",          default=f":{CHAT_PORT_DEFAULT}")
    p.add_argument("--web-addr",           default=f":{WEB_PORT_DEFAULT}")
    p.add_argument("--max-clients",        type=int,   default=100)
    p.add_argument("--rate",               type=float, default=5.0,  help="Token bucket refill rate (msg/s)")
    p.add_argument("--burst",              type=float, default=10.0, help="Token bucket burst size")
    p.add_argument("--strikes",            type=int,   default=3,    help="Flood strikes before disconnect")
    p.add_argument("--log-level",          default="info",  choices=["debug","info","warn","error"])
    p.add_argument("--log-format",         default="text",  choices=["text","json"])
    p.add_argument("--motd",               default="",  help="Message of the Day text")
    p.add_argument("--motd-file",          default="",  help="Path to MOTD file (reloaded on SIGHUP)")
    p.add_argument("--history-size",       type=int, default=20, help="Chat lines replayed to new joiners")
    p.add_argument("--nick-reserve-secs",  type=int, default=60, help="Seconds a nick is reserved after disconnect")
    p.add_argument("--stats-file",         default="",  help="Path to JSON stats file ('' = disabled)")
    p.add_argument("--admin-password",     default="",  help="Admin password ('' = admin disabled)")
    p.add_argument("--ban-file",           default="",  help="Path to IP ban list (one IP per line)")
    p.add_argument("--tls-cert",           default="",  help="Path to TLS certificate file")
    p.add_argument("--tls-key",            default="",  help="Path to TLS private key file")
    ns = p.parse_args()
    return Config(
        chat_addr         = ns.chat_addr,
        web_addr          = ns.web_addr,
        max_clients       = ns.max_clients,
        rate              = ns.rate,
        burst             = ns.burst,
        strikes           = ns.strikes,
        log_level         = ns.log_level,
        log_format        = ns.log_format,
        motd              = ns.motd,
        motd_file         = ns.motd_file,
        history_size      = ns.history_size,
        nick_reserve_secs = ns.nick_reserve_secs,
        stats_file        = ns.stats_file,
        admin_password    = ns.admin_password,
        ban_file          = ns.ban_file,
        tls_cert          = ns.tls_cert,
        tls_key           = ns.tls_key,
    )


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    cfg = _parse_args()
    _setup_logging(cfg)

    server = MortalNetServer(cfg)

    t = threading.Thread(target=_run_dashboard, args=(server, cfg), daemon=True)
    t.start()

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
        pass
    finally:
        loop.close()
        logging.info("MortalNet server stopped.")


if __name__ == "__main__":
    main()
