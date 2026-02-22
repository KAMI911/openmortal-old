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
	sendBufSize  = 64             // outbound channel buffer; full → disconnect
	idleTimeout  = 5 * time.Minute
	writeTimeout = 30 * time.Second
)

var clientIDCounter uint64

// Client holds all state for a single connected client.
type Client struct {
	id           uint64
	ip           string
	conn         net.Conn
	send         chan string // outbound message queue; closed by hub on leave
	joinedAt     time.Time

	mu           sync.Mutex  // protects the fields below
	nick         string
	confirmed    bool
	lastActivity time.Time

	// token bucket (protected by mu; initialised by hub in handleJoin)
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
	}
}

// readPump reads lines from the TCP connection and sends HubEvents to the hub.
// It exits when the connection closes or an error occurs.
// The deferred EventLeave tells the hub to clean up.
func (c *Client) readPump(hub *Hub, cfg *Config) {
	defer func() {
		hub.events <- HubEvent{Type: EventLeave, Client: c}
	}()

	scanner := bufio.NewScanner(c.conn)
	scanner.Buffer(make([]byte, maxLineBytes+2), maxLineBytes+2)
	scanner.Split(scanLines)

	for {
		// Sliding read deadline (reset each iteration)
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
			slog.Warn("oversized line from client, disconnecting", "client", c.id, "len", len(raw))
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

// writePump drains the send channel and writes to the TCP connection.
// It exits when the hub closes the send channel (on client leave).
func (c *Client) writePump() {
	for msg := range c.send {
		if err := c.conn.SetWriteDeadline(time.Now().Add(writeTimeout)); err != nil {
			slog.Debug("setWriteDeadline failed", "client", c.id, "err", err)
			// Drain remaining messages and exit
			for range c.send {
			}
			return
		}
		if _, err := fmt.Fprint(c.conn, msg); err != nil {
			slog.Debug("write error", "client", c.id, "err", err)
			// Drain remaining messages and exit
			for range c.send {
			}
			return
		}
	}
}

// enqueue attempts to send a message to the client.
// If the outbound buffer is full, the client connection is forcibly closed
// (the readPump will detect the close and emit an EventLeave).
func (c *Client) enqueue(msg string, hub *Hub) {
	select {
	case c.send <- msg:
	default:
		slog.Warn("client send buffer full, disconnecting", "client", c.id, "nick", c.nick)
		c.conn.Close()
	}
}

// consumeToken implements the token bucket rate limiter.
// Returns true if a token was available (message allowed).
// Must be called with c.mu held.
func (c *Client) consumeToken(cfg *Config) bool {
	now := time.Now()
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

// scanLines is a bufio.SplitFunc that splits on '\n' and drops the terminator.
// Lines longer than maxLineBytes are returned as-is (caller disconnects).
func scanLines(data []byte, atEOF bool) (advance int, token []byte, err error) {
	for i, b := range data {
		if b == '\n' {
			line := data[:i]
			// Strip optional preceding \r
			if len(line) > 0 && line[len(line)-1] == '\r' {
				line = line[:len(line)-1]
			}
			return i + 1, line, nil
		}
		if i >= maxLineBytes {
			// Line too long — return it so readPump can disconnect
			return i + 1, data[:i], nil
		}
	}
	if atEOF && len(data) > 0 {
		return len(data), data, nil
	}
	return 0, nil, nil
}
