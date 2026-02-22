package main

import (
	"context"
	"fmt"
	"log/slog"
	"net"
	"sync"
	"time"
)

// HubEventType identifies hub events.
type HubEventType int

const (
	EventJoin    HubEventType = iota // new TCP connection (hub decides accept/reject)
	EventMessage                      // parsed message from a confirmed/unconfirmed client
	EventLeave                        // client readPump exited
)

// HubEvent carries a single event from a client goroutine to the hub.
type HubEvent struct {
	Type   HubEventType
	Client *Client
	Msg    *ClientMessage // non-nil only for EventMessage
}

// StatusSnapshot is a point-in-time view of server state for the dashboard.
type StatusSnapshot struct {
	UptimeSeconds int64        `json:"uptime_seconds"`
	PlayerCount   int          `json:"player_count"`
	Players       []PlayerInfo `json:"players"`
}

// PlayerInfo is per-player data in a snapshot.
type PlayerInfo struct {
	Nick        string `json:"nick"`
	IP          string `json:"ip"`
	JoinedAt    int64  `json:"joined_at"`
	IdleSeconds int64  `json:"idle_seconds"`
}

// Hub owns all mutable server state and is driven by a single goroutine.
// All hub.clients / hub.nicks access happens exclusively inside Run().
type Hub struct {
	cfg       *Config
	events    chan HubEvent
	snapshots chan chan StatusSnapshot
	clients   map[uint64]*Client  // owned exclusively by Run()
	nicks     map[string]uint64   // nick → client id; owned exclusively by Run()
	startTime time.Time
	clientWG  sync.WaitGroup     // tracks per-client goroutines for clean shutdown
}

func newHub(cfg *Config) *Hub {
	return &Hub{
		cfg:       cfg,
		events:    make(chan HubEvent, 256),
		snapshots: make(chan chan StatusSnapshot, 16),
		clients:   make(map[uint64]*Client),
		nicks:     make(map[string]uint64),
		startTime: time.Now(),
	}
}

// Run is the hub's single goroutine; all state mutations happen here.
func (h *Hub) Run(ctx context.Context) {
	for {
		select {
		case <-ctx.Done():
			// Broadcast shutdown and close all connections
			for _, c := range h.clients {
				c.enqueue(FormatMsg('S', "Server is shutting down."), h)
				c.conn.Close()
			}
			// Wait for all client goroutines to finish
			h.clientWG.Wait()
			return

		case ev := <-h.events:
			switch ev.Type {
			case EventJoin:
				h.handleJoin(ev.Client)
			case EventMessage:
				h.handleMessage(ev.Client, ev.Msg)
			case EventLeave:
				h.handleLeave(ev.Client)
			}

		case reply := <-h.snapshots:
			reply <- h.buildSnapshot()
		}
	}
}

// Snapshot returns a point-in-time copy of hub state (safe to call from any goroutine).
func (h *Hub) Snapshot() StatusSnapshot {
	reply := make(chan StatusSnapshot, 1)
	h.snapshots <- reply
	return <-reply
}

func (h *Hub) buildSnapshot() StatusSnapshot {
	now := time.Now()
	players := make([]PlayerInfo, 0, len(h.clients))
	for _, c := range h.clients {
		c.mu.Lock()
		confirmed := c.confirmed
		nick := c.nick
		last := c.lastActivity
		joined := c.joinedAt
		c.mu.Unlock()
		if !confirmed {
			continue
		}
		players = append(players, PlayerInfo{
			Nick:        nick,
			IP:          c.ip,
			JoinedAt:    joined.Unix(),
			IdleSeconds: int64(now.Sub(last).Seconds()),
		})
	}
	return StatusSnapshot{
		UptimeSeconds: int64(now.Sub(h.startTime).Seconds()),
		PlayerCount:   len(players),
		Players:       players,
	}
}

// handleJoin is called by the hub goroutine when a new TCP connection arrives.
func (h *Hub) handleJoin(c *Client) {
	if len(h.clients) >= h.cfg.MaxClients {
		slog.Warn("max clients reached, rejecting", "ip", c.ip)
		// Write the rejection in a temporary goroutine so we don't block the hub
		go func() {
			fmt.Fprint(c.conn, FormatMsg('S', "Server is full. Try again later."))
			c.conn.Close()
		}()
		return
	}

	// Initialise token bucket now that we own the client
	c.mu.Lock()
	c.tokens = h.cfg.Burst
	c.tokenLast = time.Now()
	c.mu.Unlock()

	h.clients[c.id] = c
	slog.Info("client accepted", "client", c.id, "ip", c.ip)

	// Spawn per-client goroutines (tracked by clientWG for clean shutdown)
	h.clientWG.Add(2)
	go func() {
		defer h.clientWG.Done()
		c.writePump()
	}()
	go func() {
		defer h.clientWG.Done()
		c.readPump(h, h.cfg)
	}()
}

// handleMessage dispatches an incoming client message.
func (h *Hub) handleMessage(c *Client, msg *ClientMessage) {
	// Ignore messages from clients that have already left (race between EventMessage and EventLeave)
	if _, exists := h.clients[c.id]; !exists {
		return
	}

	c.mu.Lock()
	confirmed := c.confirmed
	c.mu.Unlock()

	// Before nick is confirmed, only allow N and L
	if !confirmed && msg.Prefix != 'N' && msg.Prefix != 'L' {
		return
	}

	// Rate-limit message-sending commands
	if msg.Prefix == 'M' || msg.Prefix == 'C' || msg.Prefix == 'W' {
		c.mu.Lock()
		allowed := c.consumeToken(h.cfg)
		if !allowed {
			c.strikes++
			strikes := c.strikes
			c.mu.Unlock()
			slog.Debug("rate limited", "client", c.id, "strike", strikes)
			if strikes >= h.cfg.Strikes {
				c.enqueue(FormatMsg('S', "You have been disconnected for flooding."), h)
				c.conn.Close()
			}
			return
		}
		c.strikes = 0
		c.mu.Unlock()
	}

	switch msg.Prefix {
	case 'N':
		h.onNick(c, msg.Content)
	case 'M':
		h.onMessage(c, msg.Content)
	case 'C':
		h.onChallenge(c, msg.Content)
	case 'W':
		h.onWhois(c, msg.Content)
	case 'L':
		c.conn.Close()
	default:
		slog.Debug("unknown prefix", "client", c.id, "prefix", string(msg.Prefix))
	}
}

func (h *Hub) onNick(c *Client, requested string) {
	newNick := h.resolveNick(SanitizeNick(requested), c.id)

	c.mu.Lock()
	confirmed := c.confirmed
	oldNick := c.nick
	c.mu.Unlock()

	if confirmed {
		if newNick == oldNick {
			return
		}
		delete(h.nicks, oldNick)
		h.nicks[newNick] = c.id
		c.mu.Lock()
		c.nick = newNick
		c.mu.Unlock()
		c.enqueue(FormatMsg('Y', newNick), h)
		h.broadcast(FormatMsg('N', fmt.Sprintf("%s %s", oldNick, newNick)), 0)
		slog.Info("nick changed", "client", c.id, "old", oldNick, "new", newNick)
	} else {
		// First registration
		h.nicks[newNick] = c.id
		c.mu.Lock()
		c.nick = newNick
		c.confirmed = true
		c.mu.Unlock()

		// Confirm nick to new client
		c.enqueue(FormatMsg('Y', newNick), h)

		// Send existing users to new client
		for _, other := range h.clients {
			other.mu.Lock()
			otherConf := other.confirmed
			otherNick := other.nick
			otherIP := other.ip
			other.mu.Unlock()
			if other.id != c.id && otherConf {
				c.enqueue(FormatMsg('J', fmt.Sprintf("%s %s", otherNick, otherIP)), h)
			}
		}

		// Announce new client to everyone else
		h.broadcast(FormatMsg('J', fmt.Sprintf("%s %s", newNick, c.ip)), c.id)
		slog.Info("client registered", "client", c.id, "nick", newNick, "ip", c.ip)
	}
}

func (h *Hub) onMessage(c *Client, text string) {
	text = SanitizeText(text)
	if text == "" {
		return
	}
	c.mu.Lock()
	nick := c.nick
	c.mu.Unlock()
	h.broadcast(FormatMsg('M', fmt.Sprintf("%s %s", nick, text)), 0)
}

func (h *Hub) onChallenge(c *Client, targetNick string) {
	c.mu.Lock()
	myNick := c.nick
	c.mu.Unlock()

	if targetNick == myNick {
		c.enqueue(FormatMsg('S', "You cannot challenge yourself."), h)
		return
	}
	targetID, ok := h.nicks[targetNick]
	if !ok {
		c.enqueue(FormatMsg('S', "No such user: "+targetNick), h)
		return
	}
	target, ok := h.clients[targetID]
	if !ok {
		c.enqueue(FormatMsg('S', "No such user: "+targetNick), h)
		return
	}
	target.enqueue(FormatMsg('C', myNick), h)
	slog.Info("challenge sent", "from", myNick, "to", targetNick)
}

func (h *Hub) onWhois(c *Client, targetNick string) {
	targetID, ok := h.nicks[targetNick]
	if !ok {
		c.enqueue(FormatMsg('S', "No such user: "+targetNick), h)
		return
	}
	target, ok := h.clients[targetID]
	if !ok {
		c.enqueue(FormatMsg('S', "No such user: "+targetNick), h)
		return
	}
	target.mu.Lock()
	ip := target.ip
	nick := target.nick
	target.mu.Unlock()
	c.enqueue(FormatMsg('W', fmt.Sprintf("%s %s", nick, ip)), h)
}

func (h *Hub) handleLeave(c *Client) {
	if _, exists := h.clients[c.id]; !exists {
		return
	}
	delete(h.clients, c.id)

	c.mu.Lock()
	confirmed := c.confirmed
	nick := c.nick
	c.mu.Unlock()

	if confirmed {
		delete(h.nicks, nick)
		h.broadcast(FormatMsg('L', nick), 0)
		slog.Info("client left", "client", c.id, "nick", nick)
	} else {
		slog.Info("unregistered client disconnected", "client", c.id)
	}

	// Closing send triggers writePump to exit
	close(c.send)
}

// broadcast sends msg to all confirmed clients, optionally excluding one.
// excludeID == 0 means send to all.
func (h *Hub) broadcast(msg string, excludeID uint64) {
	for _, c := range h.clients {
		c.mu.Lock()
		conf := c.confirmed
		c.mu.Unlock()
		if conf && c.id != excludeID {
			c.enqueue(msg, h)
		}
	}
}

// resolveNick returns a unique nick derived from base, ignoring the given client id.
func (h *Hub) resolveNick(base string, myID uint64) string {
	if existingID, taken := h.nicks[base]; !taken || existingID == myID {
		return base
	}
	for i := 1; ; i++ {
		suffix := fmt.Sprintf("_%d", i)
		candidate := base
		if len(base)+len(suffix) > 20 {
			candidate = base[:20-len(suffix)] + suffix
		} else {
			candidate = base + suffix
		}
		if existingID, taken := h.nicks[candidate]; !taken || existingID == myID {
			return candidate
		}
	}
}

// RunTCPListener accepts TCP connections and sends EventJoin events to the hub.
// All hub state mutation happens inside hub.Run(); the accept loop only creates
// the Client struct and sends it to the hub via the events channel.
func RunTCPListener(ctx context.Context, cfg *Config, hub *Hub, wg *sync.WaitGroup) error {
	ln, err := net.Listen("tcp", cfg.ChatAddr)
	if err != nil {
		return err
	}
	slog.Info("MortalNet chat server listening", "addr", ln.Addr())

	// Close listener when context is cancelled
	go func() {
		<-ctx.Done()
		ln.Close()
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		for {
			conn, err := ln.Accept()
			if err != nil {
				select {
				case <-ctx.Done():
					return
				default:
					slog.Error("accept error", "err", err)
					continue
				}
			}

			ip, _, _ := net.SplitHostPort(conn.RemoteAddr().String())
			c := newClient(conn, ip)

			select {
			case hub.events <- HubEvent{Type: EventJoin, Client: c}:
			default:
				slog.Warn("hub event queue full, rejecting connection", "ip", ip)
				fmt.Fprint(conn, FormatMsg('S', "Server busy. Try again later."))
				conn.Close()
			}
		}
	}()

	return nil
}
