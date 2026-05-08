package xmonitor

import (
	"bytes"
	"context"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"
	"unicode/utf8"
)

// ClipEvent represents a clipboard change event.
type ClipEvent struct {
	Content   string
	ImagePath string
	MimeType  string
	IsImage   bool
	SourceApp string
	Timestamp time.Time
}

// Monitor polls the X11 CLIPBOARD selection for changes.
type Monitor struct {
	interval  time.Duration
	imageDir  string
	onClip    func(ClipEvent)
	lastHash  string
}

// New creates a new Monitor. imageDir is where images are saved.
func New(interval time.Duration, imageDir string) *Monitor {
	return &Monitor{
		interval: interval,
		imageDir: imageDir,
	}
}

// OnClip sets the callback for clipboard change events.
func (m *Monitor) OnClip(fn func(ClipEvent)) {
	m.onClip = fn
}

// Start begins monitoring the clipboard. Blocks until ctx is cancelled.
func (m *Monitor) Start(ctx context.Context) error {
	log.Println("[xmonitor] starting, interval:", m.interval)

	ticker := time.NewTicker(m.interval)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			log.Println("[xmonitor] stopped")
			return ctx.Err()
		case <-ticker.C:
			if err := m.poll(); err != nil {
				log.Printf("[xmonitor] poll error: %v", err)
			}
		}
	}
}

func (m *Monitor) poll() error {
	// Try text first
	if content := m.getText(); content != "" {
		hash := simpleHash(content)
		if hash == m.lastHash {
			return nil
		}
		m.lastHash = hash

		m.onClip(ClipEvent{
			Content:   content,
			MimeType:  "text/plain",
			IsImage:   false,
			SourceApp: getSourceApp(),
			Timestamp: time.Now(),
		})
		return nil
	}

	// Try image
	if imgData := m.getImage(); len(imgData) > 0 {
		hash := fmt.Sprintf("img:%d:%d", len(imgData), imgData[0])
		if hash == m.lastHash {
			return nil
		}
		m.lastHash = hash

		path, err := m.saveImage(imgData)
		if err != nil {
			log.Printf("[xmonitor] save image: %v", err)
			return nil
		}

		m.onClip(ClipEvent{
			ImagePath: path,
			MimeType:  "image/png",
			IsImage:   true,
			SourceApp: getSourceApp(),
			Timestamp: time.Now(),
		})
	}

	return nil
}

func (m *Monitor) getText() string {
	for _, target := range []string{"text/plain;charset=utf-8", "text/plain", "UTF8_STRING"} {
		cmd := exec.Command("xclip", "-selection", "clipboard", "-t", target, "-o")
		var stdout bytes.Buffer
		cmd.Stdout = &stdout
		cmd.Stderr = nil
		if err := cmd.Run(); err != nil {
			continue
		}
		data := stdout.Bytes()
		if len(data) > 0 && utf8.Valid(data) {
			return string(data)
		}
	}
	return ""
}

func (m *Monitor) getImage() []byte {
	cmd := exec.Command("xclip", "-selection", "clipboard", "-t", "image/png", "-o")
	out, err := cmd.Output()
	if err != nil {
		return nil
	}
	if len(out) < 8 {
		return nil
	}
	// PNG magic: \x89PNG
	if out[0] == 0x89 && out[1] == 0x50 && out[2] == 0x4e && out[3] == 0x47 {
		return out
	}
	return nil
}

func (m *Monitor) saveImage(data []byte) (string, error) {
	if m.imageDir == "" {
		m.imageDir = filepath.Join(os.TempDir(), "cliplist-images")
	}
	if err := os.MkdirAll(m.imageDir, 0755); err != nil {
		return "", err
	}
	filename := fmt.Sprintf("clip_%d.png", time.Now().UnixNano())
	path := filepath.Join(m.imageDir, filename)
	return path, os.WriteFile(path, data, 0644)
}

// GetClipboard returns the current clipboard text content.
func GetClipboard() (string, error) {
	for _, target := range []string{"text/plain;charset=utf-8", "text/plain", "UTF8_STRING"} {
		cmd := exec.Command("xclip", "-selection", "clipboard", "-t", target, "-o")
		out, err := cmd.Output()
		if err == nil && len(out) > 0 && utf8.Valid(out) {
			return string(out), nil
		}
	}
	return "", nil
}

// SetClipboard sets both CLIPBOARD and PRIMARY selections.
func SetClipboard(content string) error {
	cmd := exec.Command("xclip", "-selection", "clipboard")
	cmd.Stdin = strings.NewReader(content)
	if err := cmd.Run(); err != nil {
		return err
	}
	// also set PRIMARY so Shift+Insert works in terminals
	cmd2 := exec.Command("xclip", "-selection", "primary")
	cmd2.Stdin = strings.NewReader(content)
	return cmd2.Run()
}

// IsImageClipboard checks if clipboard has image content.
func IsImageClipboard() bool {
	cmd := exec.Command("xclip", "-selection", "clipboard", "-t", "image/png", "-o")
	cmd.Stdout = nil
	cmd.Stderr = nil
	return cmd.Run() == nil
}

// GetImageClipboard returns PNG image bytes from clipboard.
func GetImageClipboard() ([]byte, error) {
	cmd := exec.Command("xclip", "-selection", "clipboard", "-t", "image/png", "-o")
	return cmd.Output()
}

func getSourceApp() string {
	cmd := exec.Command("xdotool", "getactivewindow", "getwindowname")
	out, err := cmd.Output()
	if err != nil {
		return ""
	}
	return strings.TrimSpace(string(out))
}

func simpleHash(s string) string {
	if len(s) > 512 {
		s = s[:512]
	}
	return s
}
