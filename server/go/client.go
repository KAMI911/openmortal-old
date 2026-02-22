package main

import (
	"bufio"
	"fmt"
	"log/slog"
	"net"
	"sync"
	"sync/atomic"
	"time"
)

const (
	maxLineBytes = 1024
	sendBufSize  = 64
	idleTimeout  = 5 * time.Minute
	writeTimeout = 30 * time.Second
)

var clientIDCounter uint64

// Client holds all state for a single connected client.
type Client struct {
	id       uint64
	ip       string
	conn     net.Conn
	send     chan string // outbound queue; closed by hub on leave
	joinedAt time.Time

	mu           sync.Mutex // protects fields below
	nick         string
	confirmed    bool
	status       string // chat | away | game | queue
	lastActivity time.Time

	// token bucket (initialised by hub in handleJoin)
	tokens    float64
	tokenLast time.Time
	strikes   int
}

func newClient(conn net.Conn, ip string) *Client {
	return &Client{
		id:           atomic.AddUint64(&clientIDCounter, 1),
		ip:           ip,
		conn:         conn,
		send:         make(chan string, sendBufSize),
		joinedAt:     time.Now(),
		lastActivity: time.Now(),
		status:       "chat",
	}
}

// readPump reads lines and forwards HubEvents to the hub.
// The deferred EventLeave notifies the hub to clean up.
func (c *Client) readPump(hub *Hub, cfg *Config) {
	defer func() {
		hub.events <- HubEvent{Type: EventLeave, Client: c}
	}()

	scanner := bufio.NewScanner(c.conn)
	scanner.Buffer(make([]byte, maxLineBytes+2), maxLineBytes+2)
	scanner.Split(scanLines)

	for {
		if err := c.conn.SetReadDeadline(time.Now().Add(idleTimeout)); err != nil {
			slog.Debug("setReadDeadline failed", "client", c.id, "err", err)
			return
		}

		if !scanner.Scan() {
			if err := scanner.Err(); err != nil {
				slog.Debug("client read error", "client", c.id, "err", err)
			}
			return
		}

		raw := scanner.Bytes()
		if len(raw) > maxLineBytes {
			slog.Warn("oversized line, disconnecting", "client", c.id)
			return
		}

		msg := ParseLine(raw)
		if msg == nil {
			continue
		}

		c.mu.Lock()
		c.lastActivity = time.Now()
		c.mu.Unlock()

		hub.events <- HubEvent{Type: EventMessage, Client: c, Msg: msg}
	}
}

// writePump drains the send channel to the TCP connection.
// Exits when the hub closes the send channel.
func (c *Client) writePump() {
	for msg := range c.send {
		if err := c.conn.SetWriteDeadline(time.Now().Add(writeTimeout)); err != nil {
			slog.Debug("setWriteDeadline failed", "client", c.id, "err", err)
			for range c.send {
			}
			return
		}
		if _, err := fmt.Fprint(c.conn, msg); err != nil {
			slog.Debug("write error", "client", c.id, "err", err)
			for range c.send {
			}
			return
		}
	}
}

// enqueue tries to queue a message. If the buffer is full, closes the connection.
func (c *Client) enqueue(msg string, hub *Hub) {
	select {
	case c.send <- msg:
	default:
		slog.Warn("send buffer full, disconnecting", "client", c.id, "nick", c.nick)
		c.conn.Close()
	}
}

// consumeToken implements the token bucket rate limiter.
// Must be called with c.mu held.
func (c *Client) consumeToken(cfg *Config) bool {
	now     := time.Now()
	elapsed := now.Sub(c.tokenLast).Seconds()
	c.tokenLast = now
	c.tokens += elapsed * cfg.Rate
	if c.tokens > cfg.Burst {
		c.tokens = cfg.Burst
	}
	if c.tokens >= 1.0 {
		c.tokens--
		return true
	}
	return false
}

// scanLines splits on '\n', strips '\r', enforces maxLineBytes.
func scanLines(data []byte, atEOF bool) (advance int, token []byte, err error) {
	for i, b := range data {
		if b == '\n' {
			line := data[:i]
			if len(line) > 0 && line[len(line)-1] == '\r' {
				line = line[:len(line)-1]
			}
			return i + 1, line, nil
		}
		if i >= maxLineBytes {
			return i + 1, data[:i], nil
		}
	}
	if atEOF && len(data) > 0 {
		return len(data), data, nil
	}
	return 0, nil, nil
}
