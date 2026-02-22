package main

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"log/slog"
	"net"
	"os"
	"strings"
	"sync"
	"time"
)

// ---------------------------------------------------------------------------
// Hub event types
// ---------------------------------------------------------------------------

type HubEventType int

const (
	EventJoin    HubEventType = iota
	EventMessage
	EventLeave
	EventSIGHUP // reload ban list + MOTD
)

type HubEvent struct {
	Type   HubEventType
	Client *Client
	Msg    *ClientMessage
}

// ---------------------------------------------------------------------------
// Snapshot types (dashboard / API)
// ---------------------------------------------------------------------------

type StatusSnapshot struct {
	UptimeSeconds int64        `json:"uptime_seconds"`
	PlayerCount   int          `json:"player_count"`
	Players       []PlayerInfo `json:"players"`
	Metrics       MetricsInfo  `json:"metrics"`
}

type PlayerInfo struct {
	Nick        string `json:"nick"`
	IP          string `json:"ip"`
	Status      string `json:"status"`
	JoinedAt    int64  `json:"joined_at"`
	IdleSeconds int64  `json:"idle_seconds"`
}

type MetricsInfo struct {
	ConnectionsTotal int64 `json:"connections_total"`
	MessagesTotal    int64 `json:"messages_total"`
	ChallengesTotal  int64 `json:"challenges_total"`
	KicksTotal       int64 `json:"kicks_total"`
	BansTotal        int64 `json:"bans_total"`
}

// ---------------------------------------------------------------------------
// Reserved nick entry
// ---------------------------------------------------------------------------

type reservedNick struct {
	ip     string
	expiry time.Time
}

// ---------------------------------------------------------------------------
// Persistent per-player stats
// ---------------------------------------------------------------------------

type PlayerStats struct {
	FirstSeen              float64 `json:"first_seen"`
	LastSeen               float64 `json:"last_seen"`
	ConnectCount           int64   `json:"connect_count"`
	MessageCount           int64   `json:"message_count"`
	ChallengeSentCount     int64   `json:"challenge_sent_count"`
	ChallengeReceivedCount int64   `json:"challenge_received_count"`
}

type StatsFile struct {
	ServerStart      float64                `json:"server_start"`
	TotalConnections int64                  `json:"total_connections"`
	TotalMessages    int64                  `json:"total_messages"`
	TotalChallenges  int64                  `json:"total_challenges"`
	Players          map[string]PlayerStats `json:"players"`
}

// ---------------------------------------------------------------------------
// Hub
// ---------------------------------------------------------------------------

type Hub struct {
	cfg       *Config
	events    chan HubEvent
	snapshots chan chan StatusSnapshot
	rawStats  chan chan []byte // for /api/stats endpoint
	clients   map[uint64]*Client
	nicks     map[string]uint64     // nick → client id
	reserved  map[string]reservedNick
	history   []string             // last N chat messages
	bannedIPs map[string]struct{}
	motd      string
	startTime time.Time
	clientWG  sync.WaitGroup
	// in-process counters (owned by hub goroutine)
	metrics MetricsInfo
	// persistent stats (owned by hub goroutine)
	stats StatsFile
}

func newHub(cfg *Config) *Hub {
	h := &Hub{
		cfg:       cfg,
		events:    make(chan HubEvent, 256),
		snapshots: make(chan chan StatusSnapshot, 16),
		rawStats:  make(chan chan []byte, 16),
		clients:   make(map[uint64]*Client),
		nicks:     make(map[string]uint64),
		reserved:  make(map[string]reservedNick),
		bannedIPs: make(map[string]struct{}),
		startTime: time.Now(),
	}
	h.stats = h.loadStats()
	h.motd  = h.loadMOTD()
	h.loadBanList()
	return h
}

// ---------------------------------------------------------------------------
// Run — the hub's single goroutine
// ---------------------------------------------------------------------------

func (h *Hub) Run(ctx context.Context) {
	for {
		select {
		case <-ctx.Done():
			for _, c := range h.clients {
				c.enqueue(FormatMsg('S', "Server is shutting down."), h)
				c.conn.Close()
			}
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
			case EventSIGHUP:
				h.loadBanList()
				h.motd = h.loadMOTD()
				slog.Info("reloaded ban list and MOTD")
			}

		case reply := <-h.snapshots:
			reply <- h.buildSnapshot()

		case reply := <-h.rawStats:
			b, _ := json.MarshalIndent(h.stats, "", "  ")
			reply <- b
		}
	}
}

// ---------------------------------------------------------------------------
// Snapshot / stats (called from HTTP goroutines via channels)
// ---------------------------------------------------------------------------

func (h *Hub) Snapshot() StatusSnapshot {
	reply := make(chan StatusSnapshot, 1)
	h.snapshots <- reply
	return <-reply
}

func (h *Hub) RawStats() []byte {
	reply := make(chan []byte, 1)
	h.rawStats <- reply
	return <-reply
}

func (h *Hub) buildSnapshot() StatusSnapshot {
	now     := time.Now()
	players := make([]PlayerInfo, 0, len(h.clients))
	for _, c := range h.clients {
		c.mu.Lock()
		conf  := c.confirmed
		nick  := c.nick
		last  := c.lastActivity
		stat  := c.status
		joined := c.joinedAt
		c.mu.Unlock()
		if !conf {
			continue
		}
		players = append(players, PlayerInfo{
			Nick:        nick,
			IP:          c.ip,
			Status:      stat,
			JoinedAt:    joined.Unix(),
			IdleSeconds: int64(now.Sub(last).Seconds()),
		})
	}
	return StatusSnapshot{
		UptimeSeconds: int64(now.Sub(h.startTime).Seconds()),
		PlayerCount:   len(players),
		Players:       players,
		Metrics:       h.metrics,
	}
}

// ---------------------------------------------------------------------------
// Join / leave
// ---------------------------------------------------------------------------

func (h *Hub) handleJoin(c *Client) {
	// Ban check
	if _, banned := h.bannedIPs[c.ip]; banned {
		slog.Info("rejected banned IP", "ip", c.ip)
		go func() {
			fmt.Fprint(c.conn, FormatMsg('S', "You are banned from this server."))
			c.conn.Close()
		}()
		return
	}

	if len(h.clients) >= h.cfg.MaxClients {
		slog.Warn("max clients reached, rejecting", "ip", c.ip)
		go func() {
			fmt.Fprint(c.conn, FormatMsg('S', "Server is full. Try again later."))
			c.conn.Close()
		}()
		return
	}

	h.metrics.ConnectionsTotal++
	h.stats.TotalConnections++

	c.mu.Lock()
	c.tokens    = h.cfg.Burst
	c.tokenLast = time.Now()
	c.mu.Unlock()

	h.clients[c.id] = c
	slog.Info("client accepted", "client", c.id, "ip", c.ip)

	h.clientWG.Add(2)
	go func() { defer h.clientWG.Done(); c.writePump() }()
	go func() { defer h.clientWG.Done(); c.readPump(h, h.cfg) }()
}

func (h *Hub) handleLeave(c *Client) {
	if _, exists := h.clients[c.id]; !exists {
		return
	}
	delete(h.clients, c.id)

	c.mu.Lock()
	confirmed := c.confirmed
	nick      := c.nick
	c.mu.Unlock()

	if confirmed {
		delete(h.nicks, nick)
		// Reserve the nick for the grace period
		if h.cfg.NickReserveSecs > 0 {
			h.reserved[nick] = reservedNick{
				ip:     c.ip,
				expiry: time.Now().Add(time.Duration(h.cfg.NickReserveSecs) * time.Second),
			}
		}
		h.broadcast(FormatMsg('L', nick), 0)
		h.touchPlayerStat(nick, "")
		h.saveStats()
		slog.Info("client left", "client", c.id, "nick", nick)
	} else {
		slog.Info("unregistered client disconnected", "client", c.id)
	}

	close(c.send)
}

// ---------------------------------------------------------------------------
// Message dispatch
// ---------------------------------------------------------------------------

func (h *Hub) handleMessage(c *Client, msg *ClientMessage) {
	if _, exists := h.clients[c.id]; !exists {
		return
	}

	c.mu.Lock()
	confirmed := c.confirmed
	c.mu.Unlock()

	if !confirmed && msg.Prefix != 'N' && msg.Prefix != 'L' {
		return
	}

	// Rate-limit message-producing commands
	if msg.Prefix == 'M' || msg.Prefix == 'C' || msg.Prefix == 'W' || msg.Prefix == 'T' {
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
	case 'N': h.onNick(c, msg.Content)
	case 'M': h.onMessage(c, msg.Content)
	case 'C': h.onChallenge(c, msg.Content)
	case 'W': h.onWhois(c, msg.Content)
	case 'T': h.onStatus(c, msg.Content)
	case 'A': h.onAdmin(c, msg.Content)
	case 'L': c.conn.Close()
	default:
		slog.Debug("unknown prefix", "client", c.id, "prefix", string(msg.Prefix))
	}
}

// ---------------------------------------------------------------------------
// Protocol handlers
// ---------------------------------------------------------------------------

func (h *Hub) onNick(c *Client, requested string) {
	newNick := h.resolveNick(SanitizeNick(requested), c.id, c.ip)

	c.mu.Lock()
	confirmed := c.confirmed
	oldNick   := c.nick
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
		h.nicks[newNick] = c.id
		delete(h.reserved, newNick)
		c.mu.Lock()
		c.nick      = newNick
		c.confirmed = true
		c.mu.Unlock()

		// Stats
		h.touchPlayerStat(newNick, "connect_count")
		h.stats.TotalConnections++
		h.saveStats()

		// 1. Confirm nick
		c.enqueue(FormatMsg('Y', newNick), h)

		// 2. Existing users → new client
		for _, other := range h.clients {
			other.mu.Lock()
			otherConf := other.confirmed
			otherNick := other.nick
			otherIP   := other.ip
			other.mu.Unlock()
			if other.id != c.id && otherConf {
				c.enqueue(FormatMsg('J', fmt.Sprintf("%s %s", otherNick, otherIP)), h)
			}
		}

		// 3. Chat history → new client
		for _, line := range h.history {
			c.enqueue(line, h)
		}

		// 4. MOTD
		if h.motd != "" {
			for _, line := range strings.Split(h.motd, "\n") {
				line = strings.TrimSpace(line)
				if line != "" {
					c.enqueue(FormatMsg('S', line), h)
				}
			}
		}

		// 5. Announce to everyone else
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

	msg := FormatMsg('M', fmt.Sprintf("%s %s", nick, text))

	// History
	h.history = append(h.history, msg)
	if len(h.history) > h.cfg.HistorySize {
		h.history = h.history[1:]
	}

	h.broadcast(msg, 0)
	h.metrics.MessagesTotal++
	h.stats.TotalMessages++
	h.touchPlayerStat(nick, "message_count")
	if h.stats.TotalMessages%20 == 0 {
		h.saveStats()
	}
}

func (h *Hub) onChallenge(c *Client, targetNick string) {
	c.mu.Lock()
	myNick := c.nick
	c.mu.Unlock()

	if targetNick == myNick {
		c.enqueue(FormatMsg('S', "You cannot challenge yourself."), h)
		return
	}
	target := h.clientByNick(targetNick)
	if target == nil {
		c.enqueue(FormatMsg('S', "No such user: "+targetNick), h)
		return
	}
	target.enqueue(FormatMsg('C', myNick), h)
	h.metrics.ChallengesTotal++
	h.stats.TotalChallenges++
	h.touchPlayerStat(myNick,     "challenge_sent_count")
	h.touchPlayerStat(targetNick, "challenge_received_count")
	slog.Info("challenge sent", "from", myNick, "to", targetNick)
}

func (h *Hub) onWhois(c *Client, targetNick string) {
	target := h.clientByNick(targetNick)
	if target == nil {
		c.enqueue(FormatMsg('S', "No such user: "+targetNick), h)
		return
	}
	target.mu.Lock()
	nick := target.nick
	ip   := target.ip
	target.mu.Unlock()
	c.enqueue(FormatMsg('W', fmt.Sprintf("%s %s", nick, ip)), h)
}

func (h *Hub) onStatus(c *Client, status string) {
	status = strings.ToLower(strings.TrimSpace(status))
	if !ValidStatus(status) {
		c.enqueue(FormatMsg('S', "Invalid status. Choose: chat, away, game, queue"), h)
		return
	}
	c.mu.Lock()
	old    := c.status
	c.status = status
	nick   := c.nick
	c.mu.Unlock()

	h.broadcast(FormatMsg('T', fmt.Sprintf("%s %s", nick, status)), 0)
	slog.Info("status changed", "nick", nick, "old", old, "new", status)

	if status == "queue" {
		h.tryMatchmake(c)
	}
}

func (h *Hub) tryMatchmake(joiner *Client) {
	joiner.mu.Lock()
	joinerNick := joiner.nick
	joiner.mu.Unlock()

	for _, other := range h.clients {
		if other.id == joiner.id {
			continue
		}
		other.mu.Lock()
		otherConf   := other.confirmed
		otherStatus := other.status
		otherNick   := other.nick
		other.mu.Unlock()

		if !otherConf || otherStatus != "queue" {
			continue
		}

		// Match found
		joiner.enqueue(FormatMsg('C', otherNick), h)
		other.enqueue(FormatMsg('C', joinerNick), h)

		// Reset both to chat
		joiner.mu.Lock(); joiner.status = "chat"; joiner.mu.Unlock()
		other.mu.Lock();  other.status  = "chat";  other.mu.Unlock()
		h.broadcast(FormatMsg('T', fmt.Sprintf("%s chat", joinerNick)), 0)
		h.broadcast(FormatMsg('T', fmt.Sprintf("%s chat", otherNick)),  0)

		joiner.enqueue(FormatMsg('S', fmt.Sprintf("Matchmaking: paired with %s!", otherNick)), h)
		other.enqueue(FormatMsg('S',  fmt.Sprintf("Matchmaking: paired with %s!", joinerNick)), h)

		h.metrics.ChallengesTotal++
		slog.Info("matchmaking", "a", joinerNick, "b", otherNick)
		return
	}
}

func (h *Hub) onAdmin(c *Client, content string) {
	if h.cfg.AdminPassword == "" {
		c.enqueue(FormatMsg('S', "Admin commands are disabled on this server."), h)
		return
	}

	parts := strings.SplitN(content, " ", 3)
	if len(parts) < 2 {
		c.enqueue(FormatMsg('S', "Usage: A<password> <kick|ban|reload|motd> [args]"), h)
		return
	}

	password, cmd := parts[0], strings.ToLower(parts[1])
	args := ""
	if len(parts) > 2 {
		args = strings.TrimSpace(parts[2])
	}

	if password != h.cfg.AdminPassword {
		c.mu.Lock()
		nick := c.nick
		c.mu.Unlock()
		c.enqueue(FormatMsg('S', "Invalid admin password."), h)
		slog.Warn("failed admin attempt", "nick", nick, "ip", c.ip)
		return
	}

	switch cmd {
	case "kick":
		h.adminKick(c, args)
	case "ban":
		h.adminBan(c, args)
	case "reload":
		h.loadBanList()
		h.motd = h.loadMOTD()
		c.enqueue(FormatMsg('S', "Reloaded ban list and MOTD."), h)
		c.mu.Lock(); slog.Info("admin reload", "nick", c.nick); c.mu.Unlock()
	case "motd":
		h.motd = args
		c.enqueue(FormatMsg('S', "MOTD updated."), h)
	default:
		c.enqueue(FormatMsg('S', "Unknown command: "+cmd), h)
	}
}

func (h *Hub) adminKick(admin *Client, targetNick string) {
	target := h.clientByNick(targetNick)
	if target == nil {
		admin.enqueue(FormatMsg('S', "No such user: "+targetNick), h)
		return
	}
	target.enqueue(FormatMsg('S', "You have been kicked by an administrator."), h)
	target.conn.Close()
	h.metrics.KicksTotal++
	admin.enqueue(FormatMsg('S', "Kicked "+targetNick+"."), h)
	admin.mu.Lock(); slog.Info("admin kick", "admin", admin.nick, "target", targetNick); admin.mu.Unlock()
}

func (h *Hub) adminBan(admin *Client, arg string) {
	ip := arg
	// Resolve nick → IP if needed
	target := h.clientByNick(arg)
	if target != nil {
		ip = target.ip
		h.adminKick(admin, arg)
	}

	h.bannedIPs[ip] = struct{}{}
	h.metrics.BansTotal++

	if h.cfg.BanFile != "" {
		f, err := os.OpenFile(h.cfg.BanFile, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
		if err == nil {
			fmt.Fprintln(f, ip)
			f.Close()
		} else {
			slog.Warn("could not write ban file", "err", err)
		}
	}

	admin.enqueue(FormatMsg('S', "Banned "+ip+"."), h)
	admin.mu.Lock(); slog.Info("admin ban", "admin", admin.nick, "ip", ip); admin.mu.Unlock()
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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

func (h *Hub) clientByNick(nick string) *Client {
	id, ok := h.nicks[nick]
	if !ok {
		return nil
	}
	return h.clients[id]
}

func (h *Hub) resolveNick(base string, myID uint64, myIP string) string {
	for i := 0; ; i++ {
		var candidate string
		if i == 0 {
			candidate = base
		} else {
			suffix := fmt.Sprintf("_%d", i)
			if len(base)+len(suffix) > 20 {
				candidate = base[:20-len(suffix)] + suffix
			} else {
				candidate = base + suffix
			}
		}

		// Active user check
		if existingID, taken := h.nicks[candidate]; taken && existingID != myID {
			continue
		}

		// Reservation check
		if res, reserved := h.reserved[candidate]; reserved {
			if time.Now().Before(res.expiry) {
				// Reserved — only allow same IP
				if myIP != "" && myIP != res.ip {
					continue
				}
			} else {
				delete(h.reserved, candidate)
			}
		}

		return candidate
	}
}

// ---------------------------------------------------------------------------
// Persistent stats
// ---------------------------------------------------------------------------

func (h *Hub) loadStats() StatsFile {
	if h.cfg.StatsFile == "" {
		return StatsFile{
			ServerStart: float64(time.Now().Unix()),
			Players:     make(map[string]PlayerStats),
		}
	}
	f, err := os.Open(h.cfg.StatsFile)
	if err != nil {
		return StatsFile{
			ServerStart: float64(time.Now().Unix()),
			Players:     make(map[string]PlayerStats),
		}
	}
	defer f.Close()
	var s StatsFile
	if err := json.NewDecoder(f).Decode(&s); err != nil {
		slog.Warn("could not parse stats file", "err", err)
		s = StatsFile{ServerStart: float64(time.Now().Unix())}
	}
	if s.Players == nil {
		s.Players = make(map[string]PlayerStats)
	}
	return s
}

func (h *Hub) saveStats() {
	if h.cfg.StatsFile == "" {
		return
	}
	tmp := h.cfg.StatsFile + ".tmp"
	f, err := os.Create(tmp)
	if err != nil {
		slog.Warn("could not write stats", "err", err)
		return
	}
	enc := json.NewEncoder(f)
	enc.SetIndent("", "  ")
	if err := enc.Encode(h.stats); err != nil {
		f.Close()
		slog.Warn("could not encode stats", "err", err)
		return
	}
	f.Close()
	if err := os.Rename(tmp, h.cfg.StatsFile); err != nil {
		slog.Warn("could not rename stats file", "err", err)
	}
}

func (h *Hub) touchPlayerStat(nick string, key string) {
	if h.cfg.StatsFile == "" {
		return
	}
	now := float64(time.Now().Unix())
	p, ok := h.stats.Players[nick]
	if !ok {
		p = PlayerStats{FirstSeen: now}
	}
	p.LastSeen = now
	switch key {
	case "connect_count":           p.ConnectCount++
	case "message_count":           p.MessageCount++
	case "challenge_sent_count":    p.ChallengeSentCount++
	case "challenge_received_count": p.ChallengeReceivedCount++
	}
	h.stats.Players[nick] = p
}

// ---------------------------------------------------------------------------
// MOTD
// ---------------------------------------------------------------------------

func (h *Hub) loadMOTD() string {
	if h.cfg.MOTDFile != "" {
		data, err := os.ReadFile(h.cfg.MOTDFile)
		if err == nil {
			return strings.TrimSpace(string(data))
		}
		slog.Warn("could not read MOTD file", "err", err)
	}
	return h.cfg.MOTD
}

// ---------------------------------------------------------------------------
// Ban list
// ---------------------------------------------------------------------------

func (h *Hub) loadBanList() {
	if h.cfg.BanFile == "" {
		return
	}
	f, err := os.Open(h.cfg.BanFile)
	if err != nil {
		return
	}
	defer f.Close()

	newBans := make(map[string]struct{})
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line != "" && !strings.HasPrefix(line, "#") {
			newBans[line] = struct{}{}
		}
	}
	h.bannedIPs = newBans
	slog.Info("loaded banned IPs", "count", len(newBans))
}

// ---------------------------------------------------------------------------
// TCP listener
// ---------------------------------------------------------------------------

func RunTCPListener(ctx context.Context, cfg *Config, hub *Hub, wg *sync.WaitGroup) error {
	ln, err := net.Listen("tcp", cfg.ChatAddr)
	if err != nil {
		return err
	}
	slog.Info("MortalNet chat server listening", "addr", ln.Addr())

	go func() { <-ctx.Done(); ln.Close() }()

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
				slog.Warn("hub event queue full, rejecting", "ip", ip)
				fmt.Fprint(conn, FormatMsg('S', "Server busy. Try again later."))
				conn.Close()
			}
		}
	}()

	return nil
}
