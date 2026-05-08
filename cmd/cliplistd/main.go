package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"os/signal"
	"path/filepath"
	"runtime"
	"strings"
	"syscall"
	"time"

	"github.com/user/cliplist/internal/config"
	"github.com/user/cliplist/internal/hotkey"
	"github.com/user/cliplist/internal/ipc"
	"github.com/user/cliplist/internal/store"
	"github.com/user/cliplist/internal/tray"
	"github.com/user/cliplist/internal/xmonitor"
)

const defaultIcon = "edit-paste"

func main() {
	log.SetFlags(log.LstdFlags | log.Lshortfile)
	log.Println("cliplistd starting...")

	cfg, err := config.Load("")
	if err != nil {
		log.Fatalf("load config: %v", err)
	}

	os.MkdirAll(config.DefaultConfigDir(), 0755)
	os.MkdirAll(cfg.ImageDir, 0755)

	db, err := store.New(cfg.DBPath, cfg.MaxHistory)
	if err != nil {
		log.Fatalf("open store: %v", err)
	}
	defer db.Close()
	log.Printf("store: %s (max: %d)", cfg.DBPath, cfg.MaxHistory)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	newClipCh := make(chan struct{}, 1)

	// --- X11 clipboard monitor ---
	monitor := xmonitor.New(cfg.PollInterval, cfg.ImageDir)
	monitor.OnClip(func(evt xmonitor.ClipEvent) {
		clip := &store.Clip{
			Content:   evt.Content,
			ImagePath: evt.ImagePath,
			MimeType:  evt.MimeType,
			IsImage:   evt.IsImage,
			SourceApp: evt.SourceApp,
		}
		if err := db.Add(clip); err != nil {
			log.Printf("store add: %v", err)
			return
		}
		if evt.IsImage {
			log.Printf("image saved: %s", evt.ImagePath)
		} else {
			log.Printf("text saved: %.60s", evt.Content)
		}
		select {
		case newClipCh <- struct{}{}:
		default:
		}
	})
	go func() {
		if err := monitor.Start(ctx); err != nil && err != context.Canceled {
			log.Printf("monitor error: %v", err)
		}
	}()

	// --- IPC server ---
	ipcServer := ipc.NewServer(func(req ipc.Request) ipc.Response {
		return handleIPC(req, db, cfg)
	})
	if err := ipcServer.Start(); err != nil {
		log.Fatalf("ipc server: %v", err)
	}
	defer ipcServer.Stop()

	// --- Icon path ---
	iconPath := cfg.IconPath
	if iconPath == "" {
		iconPath = findIcon()
	}

	// --- GTK + Tray + Popup ---
	tr := tray.New()
	tray.SetCurrent(tr)
	tray.SetPopupStore(db)

	go handleTrayEvents(tr, db, newClipCh, cfg)

	// --- Global hotkey (skip if not configured) ---
	if len(cfg.Hotkey.Modifiers) > 0 && cfg.Hotkey.Key != "" {
		hk := hotkey.New(cfg.Hotkey.Modifiers, cfg.Hotkey.Key)
		hk.OnPress(func() {
			log.Println("[hotkey] pressed")
			tray.PopupToggle()
		})
		go func() {
			if err := hk.Start(ctx); err != nil {
				log.Printf("[hotkey] error: %v (hotkey disabled)", err)
			}
		}()
	} else {
		log.Println("[hotkey] not configured, skipping")
	}

	// --- Signal handling ---
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		sig := <-sigCh
		log.Printf("signal: %v", sig)
		cancel()
		time.Sleep(100 * time.Millisecond)
		os.Exit(0)
	}()

	// --- Preload clips for tray menu ---
	clips, _ := db.List(15)
	var menuClips []tray.MenuClip
	for _, c := range clips {
		menuClips = append(menuClips, tray.MenuClip{ID: c.ID, Content: c.Content})
	}
	tr.SetClips(menuClips)

	// --- GTK main loop (must be on main thread) ---
	runtime.LockOSThread()
	tray.GTKInit()
	tray.PopupInit()
	tr.Run(iconPath)

	log.Println("cliplistd exiting")
}

func handleTrayEvents(tr *tray.Tray, db *store.Store, newClipCh chan struct{}, cfg *config.Config) {
	for {
		select {
		case <-newClipCh:
			refreshTrayMenu(tr, db)
		case clipID := <-tr.SelectCh:
			log.Printf("tray selected: %d", clipID)
			refreshTrayMenu(tr, db)

		case <-tr.ClearCh:
			log.Println("tray clear")
			if err := db.Clear(); err != nil {
				log.Printf("clear error: %v", err)
			}
			refreshTrayMenu(tr, db)

		case <-tr.SettingsCh:
			openSettings(cfg)

		case <-tr.QuitCh:
			log.Println("tray quit")
			os.Exit(0)
		}
	}
}

func refreshTrayMenu(tr *tray.Tray, db *store.Store) {
	clips, err := db.List(15)
	if err != nil {
		log.Printf("list: %v", err)
		return
	}
	var menuClips []tray.MenuClip
	for _, c := range clips {
		menuClips = append(menuClips, tray.MenuClip{
			ID:      c.ID,
			Content: c.Content,
		})
	}
	tr.UpdateClips(menuClips)
}

func openSettings(cfg *config.Config) {
	tray.SettingsShow(
		cfg.MaxHistory,
		cfg.PollIntervalStr,
		cfg.PasteDelay,
		cfg.IgnoreWS,
		cfg.AutoStart,
		cfg.MaxLength,
		cfg.Hotkey.Modifiers,
		cfg.Hotkey.Key,
		func(maxHistory int, pollInterval, pasteDelay string, ignoreWS, autoStart bool, maxLengthKB int, hotkeyMods, hotkeyKey string) {
			cfg.MaxHistory = maxHistory
			cfg.PollIntervalStr = pollInterval
			if d, err := time.ParseDuration(pollInterval); err == nil {
				cfg.PollInterval = d
			}
			cfg.PasteDelay = pasteDelay
			if d, err := time.ParseDuration(pasteDelay); err == nil {
				cfg.PasteDelayDur = d
			}
			cfg.IgnoreWS = ignoreWS
			cfg.AutoStart = autoStart
			cfg.MaxLength = maxLengthKB * 1024
			modList := strings.Split(strings.TrimSpace(hotkeyMods), ",")
			var mods []string
			for _, m := range modList {
				m = strings.TrimSpace(m)
				if m != "" {
					mods = append(mods, m)
				}
			}
			cfg.Hotkey.Modifiers = mods
			cfg.Hotkey.Key = strings.TrimSpace(hotkeyKey)

			if err := cfg.Save(""); err != nil {
				log.Printf("[settings] save error: %v", err)
			} else {
				log.Println("[settings] saved to", config.DefaultConfigDir()+"/config.toml")
			}
		},
	)
}

func handleIPC(req ipc.Request, db *store.Store, cfg *config.Config) ipc.Response {
	switch req.Action {
	case "list":
		limit := req.Limit
		if limit <= 0 {
			limit = 20
		}
		clips, err := db.List(limit)
		if err != nil {
			return ipc.Response{OK: false, Error: err.Error()}
		}
		return ipc.Response{OK: true, Data: clips}

	case "search":
		clips, err := db.Search(req.Query, req.Limit)
		if err != nil {
			return ipc.Response{OK: false, Error: err.Error()}
		}
		return ipc.Response{OK: true, Data: clips}

	case "clear":
		if err := db.Clear(); err != nil {
			return ipc.Response{OK: false, Error: err.Error()}
		}
		return ipc.Response{OK: true}

	case "fav":
		if err := db.ToggleFav(req.ID); err != nil {
			return ipc.Response{OK: false, Error: err.Error()}
		}
		return ipc.Response{OK: true}

	case "delete":
		if err := db.Delete(req.ID); err != nil {
			return ipc.Response{OK: false, Error: err.Error()}
		}
		return ipc.Response{OK: true}

	case "copy":
		clip, err := db.GetByID(req.ID)
		if err != nil {
			return ipc.Response{OK: false, Error: err.Error()}
		}
		if err := xmonitor.SetClipboard(clip.Content); err != nil {
			return ipc.Response{OK: false, Error: err.Error()}
		}
		return ipc.Response{OK: true}

	case "pop":
		tray.PopupToggle()
		return ipc.Response{OK: true}

	case "settings":
		openSettings(cfg)
		return ipc.Response{OK: true}

	default:
		return ipc.Response{OK: false, Error: fmt.Sprintf("unknown action: %s", req.Action)}
	}
}

func findIcon() string {
	localPaths := []string{
		filepath.Join("assets", "icons", "cliplist.svg"),
		filepath.Join("assets", "icons", "cliplist.png"),
	}
	for _, p := range localPaths {
		if _, err := os.Stat(p); err == nil {
			abs, _ := filepath.Abs(p)
			return abs
		}
	}
	return defaultIcon
}
