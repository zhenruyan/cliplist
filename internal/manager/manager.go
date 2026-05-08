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
	"time"
	"unsafe"

	"github.com/user/cliplist/internal/store"
	"github.com/user/cliplist/internal/xmonitor"
)

var (
	mgrMu      sync.Mutex
	mgrStore   *store.Store
	mgrClips   []store.Clip
	mgrTags    []string // unique tags for sidebar
	mgrSources []string // unique source apps for sidebar
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
		case 1:
			isImg := false
			clips, err = mgrStore.ListFiltered(limit, false, &isImg)
		case 2:
			isImg := true
			clips, err = mgrStore.ListFiltered(limit, false, &isImg)
		case 3:
			clips, err = mgrStore.ListFiltered(limit, true, nil)
		default:
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
		if err := xmonitor.SetImageClipboard(clip.ImagePath); err != nil {
			log.Printf("[manager] copy image: %v", err)
		}
		return
	}
	if err := xmonitor.SetClipboard(clip.Content); err != nil {
		log.Printf("[manager] copy: %v", err)
	}
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

//export goTagAdded
func goTagAdded(id C.int, cTag *C.char) {
	if mgrStore == nil {
		return
	}
	tag := C.GoString(cTag)
	go func() {
		if err := mgrStore.AddTag(int64(id), tag); err != nil {
			log.Printf("[manager] add tag: %v", err)
			return
		}
		reloadAndRebuild()
	}()
}

//export goTagRemoved
func goTagRemoved(id C.int, cTag *C.char) {
	if mgrStore == nil {
		return
	}
	tag := C.GoString(cTag)
	go func() {
		if err := mgrStore.RemoveTag(int64(id), tag); err != nil {
			log.Printf("[manager] remove tag: %v", err)
		}
	}()
}

//export goTagGroupChanged
func goTagGroupChanged(cTag *C.char) {
	if mgrStore == nil {
		return
	}
	tag := C.GoString(cTag)
	go func() {
		clips, err := mgrStore.ListByTag(tag, 200)
		if err != nil {
			log.Printf("[manager] list by tag %q: %v", tag, err)
			return
		}
		mgrMu.Lock()
		mgrClips = clips
		mgrMu.Unlock()
		C.g_idle_add(C.GSourceFunc(C.rebuildGridIdle), nil)
	}()
}

//export goSourceGroupChanged
func goSourceGroupChanged(cSource *C.char) {
	if mgrStore == nil {
		return
	}
	source := C.GoString(cSource)
	go func() {
		clips, err := mgrStore.ListBySource(source, 200)
		if err != nil {
			log.Printf("[manager] list by source %q: %v", source, err)
			return
		}
		mgrMu.Lock()
		mgrClips = clips
		mgrMu.Unlock()
		C.g_idle_add(C.GSourceFunc(C.rebuildGridIdle), nil)
	}()
}

//export goRebuildGrid
func goRebuildGrid() {
	mgrMu.Lock()
	clips := make([]store.Clip, len(mgrClips))
	copy(clips, mgrClips)
	mgrMu.Unlock()

	C.clearGrid()
	if len(clips) == 0 {
		C.showEmpty()
		return
	}
	for _, c := range clips {
		/* source app */
		cSource := C.CString(c.SourceApp)
		defer C.free(unsafe.Pointer(cSource))

		/* time: just HH:MM */
		timeStr := ""
		if !c.CreatedAt.IsZero() {
			timeStr = c.CreatedAt.Format("15:04")
		}
		cTime := C.CString(timeStr)
		defer C.free(unsafe.Pointer(cTime))

		/* tags */
		cTags := C.CString(c.Tags)
		defer C.free(unsafe.Pointer(cTags))

		if c.IsImage {
			cPath := C.CString(c.ImagePath)
			defer C.free(unsafe.Pointer(cPath))
			C.addClipCard(C.int(c.ID), nil, 0, 1, cPath,
				boolCint(c.IsFav), cTags, cSource, cTime)
		} else {
			cText := C.CString(c.Content)
			defer C.free(unsafe.Pointer(cText))
			C.addClipCard(C.int(c.ID), cText, C.int(len(c.Content)), 0, nil,
				boolCint(c.IsFav), cTags, cSource, cTime)
		}
	}
}

//export goRebuildTagSidebar
func goRebuildTagSidebar() {
	mgrMu.Lock()
	tags := make([]string, len(mgrTags))
	copy(tags, mgrTags)
	mgrMu.Unlock()

	C.clearTagSidebar()
	for _, t := range tags {
		ct := C.CString(t)
		C.addTagButton(ct)
		C.free(unsafe.Pointer(ct))
	}
}

//export goRebuildSourceSidebar
func goRebuildSourceSidebar() {
	mgrMu.Lock()
	sources := make([]string, len(mgrSources))
	copy(sources, mgrSources)
	mgrMu.Unlock()

	C.clearSourceSidebar()
	for _, s := range sources {
		cs := C.CString(s)
		C.addSourceButton(cs)
		C.free(unsafe.Pointer(cs))
	}
}

func boolCint(b bool) C.int {
	if b {
		return 1
	}
	return 0
}

// collectTags extracts unique tags from the DB.
func collectTags() {
	tags, err := mgrStore.GetAllTags()
	if err != nil {
		log.Printf("[manager] get tags: %v", err)
		return
	}
	mgrMu.Lock()
	mgrTags = tags
	mgrMu.Unlock()
}

// collectSources extracts unique source apps from the DB.
func collectSources() {
	sources, err := mgrStore.GetSourceApps()
	if err != nil {
		log.Printf("[manager] get sources: %v", err)
		return
	}
	mgrMu.Lock()
	mgrSources = sources
	mgrMu.Unlock()
}

func reloadAndRebuild() {
	clips, err := mgrStore.List(200)
	if err != nil {
		log.Printf("[manager] reload: %v", err)
		return
	}
	mgrMu.Lock()
	mgrClips = clips
	mgrMu.Unlock()

	collectTags()
	collectSources()

	C.g_idle_add(C.GSourceFunc(C.rebuildGridIdle), nil)
	C.g_idle_add(C.GSourceFunc(C.rebuildTagSidebarIdle), nil)
	C.g_idle_add(C.GSourceFunc(C.rebuildSourceSidebarIdle), nil)
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
	// Pre-load data into Go memory
	clips, err := db.List(200)
	if err != nil {
		log.Printf("[manager] preload: %v", err)
	} else {
		mgrMu.Lock()
		mgrClips = clips
		mgrMu.Unlock()
	}

	collectTags()
	collectSources()

	// Use reloadAndRebuild to ensure all sidebars and grid are populated
	go func() {
		time.Sleep(100 * time.Millisecond) // Wait for GTK to be ready
		reloadAndRebuild()
		C.g_idle_add(C.GSourceFunc(C.showManagerIdle), nil)
	}()

	log.Println("[manager] entering GTK main loop")
	C.gtk_main()
	log.Println("[manager] exited")
}

// Stop exits the GTK main loop.
func Stop() {
	C.stopIdle(nil)
}
