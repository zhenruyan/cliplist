package hotkey

import (
	"context"
	"fmt"
	"log"
	"strings"

	"github.com/jezek/xgb"
	"github.com/jezek/xgb/xproto"
)

var modMap = map[string]uint16{
	"shift": xproto.ModMaskShift,
	"ctrl":  xproto.ModMaskControl,
	"alt":   xproto.ModMask1,
	"super": xproto.ModMask4,
	"mod4":  xproto.ModMask4,
}

var keysymMap = map[string]xproto.Keysym{
	"a": 0x61, "b": 0x62, "c": 0x63, "d": 0x64, "e": 0x65,
	"f": 0x66, "g": 0x67, "h": 0x68, "i": 0x69, "j": 0x6a,
	"k": 0x6b, "l": 0x6c, "m": 0x6d, "n": 0x6e, "o": 0x6f,
	"p": 0x70, "q": 0x71, "r": 0x72, "s": 0x73, "t": 0x74,
	"u": 0x75, "v": 0x76, "w": 0x77, "x": 0x78, "y": 0x79,
	"z": 0x7a,
	"0": 0x30, "1": 0x31, "2": 0x32, "3": 0x33, "4": 0x34,
	"5": 0x35, "6": 0x36, "7": 0x37, "8": 0x38, "9": 0x39,
	"space": 0x20, "return": 0xff0d, "enter": 0xff0d,
	"tab": 0xff09, "escape": 0xff1b, "esc": 0xff1b,
	"backspace": 0xff08, "delete": 0xffff, "del": 0xffff,
	"f1": 0xffbe, "f2": 0xffbf, "f3": 0xffc0, "f4": 0xffc1,
	"f5": 0xffc2, "f6": 0xffc3, "f7": 0xffc4, "f8": 0xffc5,
	"f9": 0xffc6, "f10": 0xffc7, "f11": 0xffc8, "f12": 0xffc9,
}

// Hotkey grabs a global keyboard shortcut on X11 root window.
type Hotkey struct {
	modifiers []string
	key       string
	modMask   uint16
	keycode   xproto.Keycode
	callback  func()
	conn      *xgb.Conn
}

// New creates a new Hotkey. modifiers: ["ctrl","shift"], key: "v"
func New(modifiers []string, key string) *Hotkey {
	return &Hotkey{
		modifiers: modifiers,
		key:       strings.ToLower(key),
	}
}

// OnPress sets the callback for when the hotkey is pressed.
func (h *Hotkey) OnPress(fn func()) {
	h.callback = fn
}

// Start grabs the key and blocks, listening for events until ctx is cancelled.
func (h *Hotkey) Start(ctx context.Context) error {
	var err error
	h.conn, err = xgb.NewConn()
	if err != nil {
		return fmt.Errorf("xgb connect: %w", err)
	}

	setup := xproto.Setup(h.conn)
	root := setup.DefaultScreen(h.conn).Root

	// modifier mask
	for _, mod := range h.modifiers {
		mod = strings.ToLower(mod)
		if m, ok := modMap[mod]; ok {
			h.modMask |= m
		}
	}

	// keysym
	keysym, ok := keysymMap[h.key]
	if !ok {
		h.conn.Close()
		return fmt.Errorf("unknown key: %s", h.key)
	}

	// keycode
	h.keycode, err = h.keysymToKeycode(setup, keysym)
	if err != nil {
		h.conn.Close()
		return err
	}

	// Grab with NumLock / CapsLock variants to handle all states
	lockMasks := []uint16{
		0,
		xproto.ModMaskLock,
		xproto.ModMask2,
		xproto.ModMaskLock | xproto.ModMask2,
	}
	for _, lock := range lockMasks {
		xproto.GrabKey(h.conn, true, root, h.modMask|lock, h.keycode,
			xproto.GrabModeAsync, xproto.GrabModeAsync)
	}

	log.Printf("[hotkey] grabbed: %v+%s (keycode=%d, mask=%d)",
		h.modifiers, h.key, h.keycode, h.modMask)

	// close conn on context cancel to unblock WaitForEvent
	go func() {
		<-ctx.Done()
		h.conn.Close()
	}()

	for {
		ev, err := h.conn.WaitForEvent()
		if err != nil {
			select {
			case <-ctx.Done():
				return nil
			default:
				return fmt.Errorf("x11 event: %w", err)
			}
		}
		if _, ok := ev.(xproto.KeyPressEvent); ok {
			if h.callback != nil {
				h.callback()
			}
		}
	}
}

func (h *Hotkey) keysymToKeycode(setup *xproto.SetupInfo, keysym xproto.Keysym) (xproto.Keycode, error) {
	minKey := setup.MinKeycode
	maxKey := setup.MaxKeycode
	count := uint8(maxKey - minKey + 1)

	mapping, err := xproto.GetKeyboardMapping(h.conn, minKey, count).Reply()
	if err != nil {
		return 0, fmt.Errorf("get keyboard mapping: %w", err)
	}

	perKey := int(mapping.KeysymsPerKeycode)
	for i := 0; i < int(count); i++ {
		for j := 0; j < perKey; j++ {
			if mapping.Keysyms[i*perKey+j] == keysym {
				return minKey + xproto.Keycode(i), nil
			}
		}
	}
	return 0, fmt.Errorf("no keycode for keysym 0x%x", keysym)
}
