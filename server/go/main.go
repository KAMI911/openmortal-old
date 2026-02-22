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
	ChatAddr   string
	WebAddr    string
	MaxClients int
	Rate       float64 // token bucket refill rate (msg/s)
	Burst      float64 // token bucket burst size
	Strikes    int     // flood strikes before disconnect
	LogLevel   string
	LogFormat  string
}

func parseConfig() *Config {
	cfg := &Config{}
	flag.StringVar(&cfg.ChatAddr, "chat-addr", ":14883", "TCP listen address for chat")
	flag.StringVar(&cfg.WebAddr, "web-addr", ":8080", "HTTP dashboard listen address")
	flag.IntVar(&cfg.MaxClients, "max-clients", 100, "Maximum simultaneous connections")
	flag.Float64Var(&cfg.Rate, "rate", 5.0, "Token bucket refill rate (msg/s)")
	flag.Float64Var(&cfg.Burst, "burst", 10.0, "Token bucket burst size")
	flag.IntVar(&cfg.Strikes, "strikes", 3, "Flood strikes before disconnect")
	flag.StringVar(&cfg.LogLevel, "log-level", "info", "Log level: debug/info/warn/error")
	flag.StringVar(&cfg.LogFormat, "log-format", "text", "Log format: text/json")
	flag.Parse()
	return cfg
}

func setupLogger(cfg *Config) {
	var level slog.Level
	switch cfg.LogLevel {
	case "debug":
		level = slog.LevelDebug
	case "warn":
		level = slog.LevelWarn
	case "error":
		level = slog.LevelError
	default:
		level = slog.LevelInfo
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
		"web", cfg.WebAddr,
		"maxClients", cfg.MaxClients,
	)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Handle SIGINT / SIGTERM → cancel context → graceful shutdown
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		sig := <-sigCh
		slog.Info("signal received, shutting down", "signal", sig)
		cancel()
	}()

	hub := newHub(cfg)

	// wg tracks the hub goroutine and the TCP accept goroutine.
	// Per-client goroutines are tracked by hub.clientWG (internal).
	var wg sync.WaitGroup

	// Start hub goroutine
	wg.Add(1)
	go func() {
		defer wg.Done()
		hub.Run(ctx)
	}()

	// Start TCP listener (spawns accept goroutine, tracked in wg)
	if err := RunTCPListener(ctx, cfg, hub, &wg); err != nil {
		slog.Error("failed to start TCP listener", "err", err)
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}

	// RunWebServer blocks until ctx is cancelled, then does a graceful HTTP shutdown.
	if err := RunWebServer(ctx, cfg, hub); err != nil {
		slog.Error("web server error", "err", err)
	}

	// Wait for hub and accept goroutines to finish
	// (hub.Run waits for all client goroutines internally before returning)
	wg.Wait()
	slog.Info("MortalNet server stopped.")
}
