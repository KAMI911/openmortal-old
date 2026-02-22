package main

import (
	"context"
	"flag"
	"fmt"
	"log/slog"
	"os"
	"os/signal"
	"sync"
	"syscall"
)

// Config holds all runtime configuration.
type Config struct {
	// Network
	ChatAddr   string
	WebAddr    string
	MaxClients int
	// Rate limiting
	Rate    float64
	Burst   float64
	Strikes int
	// Logging
	LogLevel  string
	LogFormat string
	// MOTD
	MOTD     string
	MOTDFile string
	// Chat history
	HistorySize int
	// Nick reservation
	NickReserveSecs int
	// Persistent stats
	StatsFile string
	// Admin
	AdminPassword string
	// Ban list
	BanFile string
	// TLS
	TLSCert string
	TLSKey  string
}

func parseConfig() *Config {
	cfg := &Config{}
	flag.StringVar(&cfg.ChatAddr,        "chat-addr",          ":14883",  "TCP listen address for chat")
	flag.StringVar(&cfg.WebAddr,         "web-addr",           ":8080",   "HTTP dashboard listen address")
	flag.IntVar(&cfg.MaxClients,         "max-clients",        100,       "Maximum simultaneous connections")
	flag.Float64Var(&cfg.Rate,           "rate",               5.0,       "Token bucket refill rate (msg/s)")
	flag.Float64Var(&cfg.Burst,          "burst",              10.0,      "Token bucket burst size")
	flag.IntVar(&cfg.Strikes,            "strikes",            3,         "Flood strikes before disconnect")
	flag.StringVar(&cfg.LogLevel,        "log-level",          "info",    "Log level: debug/info/warn/error")
	flag.StringVar(&cfg.LogFormat,       "log-format",         "text",    "Log format: text/json")
	flag.StringVar(&cfg.MOTD,            "motd",               "",        "Message of the Day text")
	flag.StringVar(&cfg.MOTDFile,        "motd-file",          "",        "Path to MOTD file (reloaded on SIGHUP)")
	flag.IntVar(&cfg.HistorySize,        "history-size",       20,        "Chat lines replayed to new joiners")
	flag.IntVar(&cfg.NickReserveSecs,    "nick-reserve-secs",  60,        "Seconds a nick is reserved after disconnect")
	flag.StringVar(&cfg.StatsFile,       "stats-file",         "",        "Path to JSON stats file ('' = disabled)")
	flag.StringVar(&cfg.AdminPassword,   "admin-password",     "",        "Admin password ('' = admin disabled)")
	flag.StringVar(&cfg.BanFile,         "ban-file",           "",        "Path to IP ban list (one IP per line)")
	flag.StringVar(&cfg.TLSCert,         "tls-cert",           "",        "Path to TLS certificate file")
	flag.StringVar(&cfg.TLSKey,          "tls-key",            "",        "Path to TLS private key file")
	flag.Parse()
	return cfg
}

func setupLogger(cfg *Config) {
	var level slog.Level
	switch cfg.LogLevel {
	case "debug": level = slog.LevelDebug
	case "warn":  level = slog.LevelWarn
	case "error": level = slog.LevelError
	default:      level = slog.LevelInfo
	}
	opts := &slog.HandlerOptions{Level: level}
	var handler slog.Handler
	if cfg.LogFormat == "json" {
		handler = slog.NewJSONHandler(os.Stdout, opts)
	} else {
		handler = slog.NewTextHandler(os.Stdout, opts)
	}
	slog.SetDefault(slog.New(handler))
}

func main() {
	cfg := parseConfig()
	setupLogger(cfg)

	slog.Info("MortalNet server starting",
		"chat", cfg.ChatAddr,
		"web",  cfg.WebAddr,
		"maxClients", cfg.MaxClients,
		"tls", cfg.TLSCert != "",
	)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Signal handling
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM, syscall.SIGHUP)

	hub := newHub(cfg)
	var wg sync.WaitGroup

	// Hub goroutine
	wg.Add(1)
	go func() {
		defer wg.Done()
		hub.Run(ctx)
	}()

	// Signal dispatcher
	go func() {
		for sig := range sigCh {
			switch sig {
			case syscall.SIGHUP:
				slog.Info("SIGHUP received, reloading")
				hub.events <- HubEvent{Type: EventSIGHUP}
			default:
				slog.Info("signal received, shutting down", "signal", sig)
				cancel()
				return
			}
		}
	}()

	// TCP listener
	if err := RunTCPListener(ctx, cfg, hub, &wg); err != nil {
		slog.Error("failed to start TCP listener", "err", err)
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}

	// Web server (blocks until ctx cancelled, then shuts down gracefully)
	if err := RunWebServer(ctx, cfg, hub); err != nil {
		slog.Error("web server error", "err", err)
	}

	wg.Wait()
	slog.Info("MortalNet server stopped.")
}
