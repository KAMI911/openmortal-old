package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log/slog"
	"net/http"
	"strings"
	"time"
)

const dashboardHTML = `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta http-equiv="refresh" content="10">
<title>MortalNet Status</title>
<style>
  body { font-family: monospace; background: #111; color: #ccc; padding: 2em; }
  h1 { color: #f80; }
  table { border-collapse: collapse; width: 100%; }
  th, td { border: 1px solid #444; padding: 0.4em 0.8em; text-align: left; }
  th { color: #f80; background: #222; }
  tr:nth-child(even) { background: #1a1a1a; }
  .meta { color: #888; margin-bottom: 1em; }
</style>
</head>
<body>
<h1>MortalNet Status</h1>
<p class="meta">Uptime: %ds &mdash; Players online: %d</p>
<table>
<tr><th>Nick</th><th>IP</th><th>Idle (s)</th></tr>
%s
</table>
</body>
</html>`

// RunWebServer starts the HTTP dashboard and blocks until ctx is cancelled.
func RunWebServer(ctx context.Context, cfg *Config, hub *Hub) error {
	mux := http.NewServeMux()
	mux.HandleFunc("/", makeHandler(hub, serveIndex))
	mux.HandleFunc("/api/status", makeHandler(hub, serveStatus))
	mux.HandleFunc("/healthz", makeHandler(hub, serveHealth))

	srv := &http.Server{
		Addr:         cfg.WebAddr,
		Handler:      mux,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 10 * time.Second,
		IdleTimeout:  60 * time.Second,
	}

	errCh := make(chan error, 1)
	go func() {
		slog.Info("MortalNet dashboard listening", "addr", cfg.WebAddr)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			errCh <- err
		}
	}()

	select {
	case err := <-errCh:
		return err
	case <-ctx.Done():
		shutCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		return srv.Shutdown(shutCtx)
	}
}

type handlerFunc func(w http.ResponseWriter, r *http.Request, hub *Hub)

func makeHandler(hub *Hub, fn handlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		// Security headers
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("X-Frame-Options", "DENY")
		w.Header().Set("Cache-Control", "no-store")

		// Only GET and HEAD
		if r.Method != http.MethodGet && r.Method != http.MethodHead {
			w.Header().Set("Allow", "GET, HEAD")
			http.Error(w, "Method Not Allowed", http.StatusMethodNotAllowed)
			return
		}

		slog.Debug("HTTP request", "method", r.Method, "path", r.URL.Path, "remote", r.RemoteAddr)
		fn(w, r, hub)
	}
}

func serveHealth(w http.ResponseWriter, r *http.Request, _ *Hub) {
	w.Header().Set("Content-Type", "text/plain")
	w.WriteHeader(http.StatusOK)
	if r.Method != http.MethodHead {
		fmt.Fprint(w, "OK\n")
	}
}

func serveStatus(w http.ResponseWriter, r *http.Request, hub *Hub) {
	snap := hub.Snapshot()
	data, err := json.MarshalIndent(snap, "", "  ")
	if err != nil {
		http.Error(w, "internal error", http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	if r.Method != http.MethodHead {
		w.Write(data)
	}
}

func serveIndex(w http.ResponseWriter, r *http.Request, hub *Hub) {
	// 404 for anything other than "/"
	if r.URL.Path != "/" {
		http.NotFound(w, r)
		return
	}

	snap := hub.Snapshot()
	var rows strings.Builder
	if len(snap.Players) == 0 {
		rows.WriteString("<tr><td colspan='3'>No players online</td></tr>")
	} else {
		for _, p := range snap.Players {
			fmt.Fprintf(&rows, "<tr><td>%s</td><td>%s</td><td>%d</td></tr>\n",
				htmlEscape(p.Nick), htmlEscape(p.IP), p.IdleSeconds)
		}
	}

	body := fmt.Sprintf(dashboardHTML, snap.UptimeSeconds, snap.PlayerCount, rows.String())
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.WriteHeader(http.StatusOK)
	if r.Method != http.MethodHead {
		fmt.Fprint(w, body)
	}
}

// htmlEscape escapes the minimal set of HTML special characters.
func htmlEscape(s string) string {
	s = strings.ReplaceAll(s, "&", "&amp;")
	s = strings.ReplaceAll(s, "<", "&lt;")
	s = strings.ReplaceAll(s, ">", "&gt;")
	s = strings.ReplaceAll(s, `"`, "&#34;")
	return s
}
