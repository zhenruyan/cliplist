package main

import (
	"log"

	"github.com/user/cliplist/internal/config"
	"github.com/user/cliplist/internal/manager"
	"github.com/user/cliplist/internal/store"
)

func main() {
	log.SetFlags(log.LstdFlags | log.Lshortfile)
	log.Println("cliplist-mgr starting...")

	cfg, err := config.Load("")
	if err != nil {
		log.Fatalf("load config: %v", err)
	}

	db, err := store.New(cfg.DBPath)
	if err != nil {
		log.Fatalf("open store: %v", err)
	}
	defer db.Close()

	// If another instance is already running, signal it to quit
	// (single-instance: we just start fresh — GTK window will replace)

	// Init GTK + create manager window (main goroutine, LockOSThread)
	manager.Init(db)

	// Load clips and enter GTK main loop (blocks)
	manager.Run(db)
}
