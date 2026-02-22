package main

import (
	"regexp"
	"strings"
	"unicode"
)

// nickRe is the whitelist for valid nick characters.
var nickRe = regexp.MustCompile(`^[a-zA-Z0-9_\-]{1,20}$`)

// ValidateNick returns true if the nick matches the whitelist exactly.
func ValidateNick(nick string) bool {
	return nickRe.MatchString(nick)
}

// SanitizeNick strips any characters not in the whitelist and truncates to 20.
// Falls back to "Player" if the result is empty.
func SanitizeNick(nick string) string {
	var b strings.Builder
	for _, r := range nick {
		if (r >= 'a' && r <= 'z') || (r >= 'A' && r <= 'Z') ||
			(r >= '0' && r <= '9') || r == '_' || r == '-' {
			b.WriteRune(r)
			if b.Len() == 20 {
				break
			}
		}
	}
	if b.Len() == 0 {
		return "Player"
	}
	return b.String()
}

// SanitizeText strips control characters below 0x20 (except space).
func SanitizeText(text string) string {
	return strings.Map(func(r rune) rune {
		if r == ' ' || !unicode.IsControl(r) {
			return r
		}
		return -1
	}, text)
}

// ValidStatus returns true if s is one of the allowed status values.
func ValidStatus(s string) bool {
	switch s {
	case "chat", "away", "game", "queue":
		return true
	}
	return false
}

// ClientMessage is a parsed message from a client.
type ClientMessage struct {
	Prefix  byte
	Content string
}

// ParseLine parses a raw line (without trailing newline) from the client.
// Returns nil if the line is empty.
func ParseLine(line []byte) *ClientMessage {
	if len(line) < 1 {
		return nil
	}
	content := ""
	if len(line) > 1 {
		content = string(line[1:])
	}
	return &ClientMessage{Prefix: line[0], Content: content}
}

// FormatMsg formats a server→client message.
func FormatMsg(prefix byte, content string) string {
	return string(prefix) + content + "\n"
}
