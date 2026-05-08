package manager

/*
#cgo pkg-config: gtk+-3.0

#include "manager.h"
*/
import "C"
import (
	"log"
	"runtime"
	"sync"
	"unsafe"

	"github.com/user/cliplist/internal/store"
	"github.com/user/cliplist/internal/xmonitor"
)

var (
	mgrMu    sync.Mutex
	mgrStore *store.Store
	mgrClips []store.Clip
)

//export goCategoryChanged
func goCategoryChanged(id C.int) {
	if mgrStore == nil {
		return
	}
	go func() {
		var clips []store.Clip
		var err error
		const limit = 200
		switch int(id) {
		case 1: // Text
			isImg := false
			clips, err = mgrStore.ListFiltered(limit, false, &isImg)
		case 2: // Images
			isImg := true
			clips, err = mgrStore.ListFiltered(limit, false, &isImg)
		case 3: // Favorites
			clips, err = mgrStore.ListFiltered(limit, true, nil)
		default: // All
			clips, err = mgrStore.List(limit)
		}
		if err != nil {
			log.Printf("[manager] category %d: %v", int(id), err)
			return
		}
		mgrMu.Lock()
		mgrClips = clips
		mgrMu.Unlock()
		C.g_idle_add(C.GSourceFunc(C.rebuildGridIdle), nil)
	}()
}

//export goManagerSearchChanged
func goManagerSearchChanged() {
	if mgrStore == nil {
		return
	}
	q := C.GoString(C.getManagerSearchText())
	go func() {
		var clips []store.Clip
		var err error
		if q == "" {
			clips, err = mgrStore.List(200)
		} else {
			clips, err = mgrStore.Search(q, 50)
		}
		if err != nil {
			log.Printf("[manager] search: %v", err)
			return
		}
		mgrMu.Lock()
		mgrClips = clips
		mgrMu.Unlock()
		C.g_idle_add(C.GSourceFunc(C.rebuildGridIdle), nil)
	}()
}

//export goCardClicked
func goCardClicked(id C.int) {
	if mgrStore == nil {
		return
	}
	clip, err := mgrStore.GetByID(int64(id))
	if err != nil {
		log.Printf("[manager] get clip %d: %v", int64(id), err)
		return
	}
	if clip.IsImage {
		log.Printf("[manager] image copy not yet supported")
		return
	}
	if err := xmonitor.SetClipboard(clip.Content); err != nil {
		log.Printf("[manager] copy: %v", err)
		return
	}
	C.hideManagerWindow()
}

//export goCardFavorited
func goCardFavorited(id C.int) {
	if mgrStore == nil {
		return
	}
	if err := mgrStore.ToggleFav(int64(id)); err != nil {
		log.Printf("[manager] toggle fav %d: %v", int64(id), err)
	}
}

//export goCardDeleted
func goCardDeleted(id C.int) {
	if mgrStore == nil {
		return
	}
	if err := mgrStore.Delete(int64(id)); err != nil {
		log.Printf("[manager] delete %d: %v", int64(id), err)
	}
}

//export goRebuildGrid
func goRebuildGrid() {
	mgrMu.Lock()
	clips := make([]store.Clip, len(mgrClips))
	copy(clips, mgrClips)
	mgrMu.Unlock()

	C.clearGrid()
	if len(clips) == 0 {
		return
	}
	for _, c := range clips {
		if c.IsImage {
			cPath := C.CString(c.ImagePath)
			C.addClipCard(C.int(c.ID), nil, 0, 1, cPath, boolCint(c.IsFav))
			C.free(unsafe.Pointer(cPath))
		} else {
			cText := C.CString(c.Content)
			C.addClipCard(C.int(c.ID), cText, C.int(len(c.Content)), 0, nil, boolCint(c.IsFav))
			C.free(unsafe.Pointer(cText))
		}
	}
}

func boolCint(b bool) C.int {
	if b {
		return 1
	}
	return 0
}

// Init creates the GTK manager window. Must be called from main goroutine.
func Init(db *store.Store) {
	mgrMu.Lock()
	mgrStore = db
	mgrMu.Unlock()

	runtime.LockOSThread()
	C.gtk_init(nil, nil)
	C.initManagerWindow()
}

// Run loads clips and enters the GTK main loop. Blocks until Stop() is called.
func Run(db *store.Store) {
	// Preload clips
	clips, err := db.List(200)
	if err != nil {
		log.Printf("[manager] preload: %v", err)
	} else {
		mgrMu.Lock()
		mgrClips = clips
		mgrMu.Unlock()
	}

	// Build grid, then show window
	go func() {
		C.g_idle_add(C.GSourceFunc(C.rebuildGridIdle), nil)
		C.g_idle_add(C.GSourceFunc(C.showManagerIdle), nil)
	}()

	log.Println("[manager] entering GTK main loop")
	C.gtk_main()
	log.Println("[manager] exited")
}

// Stop exits the GTK main loop.
func Stop() {
	C.g_idle_add(C.GSourceFunc(C.stopIdle), nil)
}
