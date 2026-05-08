package tray

/*
#cgo pkg-config: ayatana-appindicator3-0.1 gtk+-3.0
#cgo CFLAGS: -Wno-deprecated-declarations

#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>

extern void onMenuItemActivate(GtkMenuItem *item, gpointer data);
extern void onClearActivate(GtkMenuItem *item, gpointer data);
extern void onQuitActivate(GtkMenuItem *item, gpointer data);
extern void onSearchActivate(GtkMenuItem *item, gpointer data);
extern void onSettingsActivate(GtkMenuItem *item, gpointer data);

static AppIndicator *indicator = NULL;
static GtkWidget *menu = NULL;

// Helper to extract int from gpointer (GPOINTER_TO_INT is a macro, not callable from Go)
static int gpointer_to_int(gpointer p) { return GPOINTER_TO_INT(p); }

static void clearMenuItems() {
    if (!menu) return;
    GList *children = gtk_container_get_children(GTK_CONTAINER(menu));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);
}

static void addClipItem(const char *label, int index) {
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    g_signal_connect(item, "activate", G_CALLBACK(onMenuItemActivate), GINT_TO_POINTER(index));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);
}

static void addImageMenuItem(const char *imagePath, const char *labelText, int index) {
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(imagePath, 24, 24, TRUE, NULL);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    if (pixbuf) {
        GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
        g_object_unref(pixbuf);
        gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 4);
    } else {
        GtkWidget *ph = gtk_label_new("[img]");
        gtk_box_pack_start(GTK_BOX(hbox), ph, FALSE, FALSE, 4);
    }

    GtkWidget *label = gtk_label_new(labelText);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 50);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    GtkWidget *item = gtk_menu_item_new();
    gtk_container_add(GTK_CONTAINER(item), hbox);
    g_signal_connect(item, "activate", G_CALLBACK(onMenuItemActivate), GINT_TO_POINTER(index));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show_all(item);
}

static void addSearchItem(const char *label) {
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    g_signal_connect(item, "activate", G_CALLBACK(onSearchActivate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);
}

static void addSettingsItem(const char *label) {
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    g_signal_connect(item, "activate", G_CALLBACK(onSettingsActivate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);
}

static void addSeparator() {
    GtkWidget *sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep);
    gtk_widget_show(sep);
}

static void addActionItem(const char *label, int type) {
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    if (type == 0)
        g_signal_connect(item, "activate", G_CALLBACK(onClearActivate), NULL);
    else
        g_signal_connect(item, "activate", G_CALLBACK(onQuitActivate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);
}

static void initTray(const char *iconPath) {
    gtk_init(NULL, NULL);
    menu = gtk_menu_new();
    indicator = app_indicator_new("cliplist", iconPath, APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_menu(indicator, GTK_MENU(menu));
    app_indicator_set_title(indicator, "Cliplist");
}

static void runGTK() { gtk_main(); }
static void stopGTK() { gtk_main_quit(); }
*/
import "C"
import (
	"log"
	"path/filepath"
	"sync"
	"unsafe"

	"github.com/user/cliplist/internal/xmonitor"
)

// MenuClip represents a clip entry in the tray menu.
type MenuClip struct {
	ID        int64
	Content   string
	IsImage   bool
	ImagePath string
}

// Tray manages the system tray icon and popup menu.
type Tray struct {
	SelectCh   chan int64    // receives clip ID when user clicks a clip
	ClearCh    chan struct{} // receives when user clicks "Clear History"
	QuitCh     chan struct{} // receives when user clicks "Quit"
	SettingsCh chan struct{} // receives when user clicks "Settings"

	mu    sync.Mutex
	clips []MenuClip
}

// New creates a new Tray instance.
func New() *Tray {
	return &Tray{
		SelectCh:   make(chan int64, 1),
		ClearCh:    make(chan struct{}, 1),
		QuitCh:     make(chan struct{}, 1),
		SettingsCh: make(chan struct{}, 1),
	}
}

// GTKInit initializes GTK. Must be called before PopupInit and Run.
func GTKInit() {
	C.gtk_init(nil, nil)
}

// Run initializes the tray and blocks on the GTK main loop.
// Call this from the main goroutine after runtime.LockOSThread().
func (t *Tray) Run(iconPath string) {
	log.Println("[tray] initializing, icon:", iconPath)
	cPath := C.CString(iconPath)
	defer C.free(unsafe.Pointer(cPath))
	C.initTray(cPath)
	t.rebuildMenu()
	log.Println("[tray] entering GTK main loop")
	C.runGTK()
}

// SetClips sets the clips without rebuilding the menu (for initial load before Run).
func (t *Tray) SetClips(clips []MenuClip) {
	t.mu.Lock()
	t.clips = clips
	t.mu.Unlock()
}

// UpdateClips refreshes the menu with new clip entries. Thread-safe.
func (t *Tray) UpdateClips(clips []MenuClip) {
	t.mu.Lock()
	t.clips = clips
	t.mu.Unlock()
	t.rebuildMenu()
}

func (t *Tray) rebuildMenu() {
	t.mu.Lock()
	clips := make([]MenuClip, len(t.clips))
	copy(clips, t.clips)
	t.mu.Unlock()

	C.clearMenuItems()

	if len(clips) == 0 {
		label := C.CString("(no clips)")
		C.addClipItem(label, -1)
		C.free(unsafe.Pointer(label))
	} else {
		for i, c := range clips {
			if c.IsImage {
				fname := filepath.Base(c.ImagePath)
				cPath := C.CString(c.ImagePath)
				cLabel := C.CString(fname)
				C.addImageMenuItem(cPath, cLabel, C.int(i))
				C.free(unsafe.Pointer(cPath))
				C.free(unsafe.Pointer(cLabel))
			} else {
				lbl := truncate(c.Content, 80)
				cLbl := C.CString(lbl)
				C.addClipItem(cLbl, C.int(i))
				C.free(unsafe.Pointer(cLbl))
			}
		}
	}

	s0 := C.CString("Search History")
	C.addSearchItem(s0)
	C.free(unsafe.Pointer(s0))

	sS := C.CString("Settings")
	C.addSettingsItem(sS)
	C.free(unsafe.Pointer(sS))

	C.addSeparator()

	s1 := C.CString("Clear History")
	C.addActionItem(s1, 0)
	C.free(unsafe.Pointer(s1))

	s2 := C.CString("Quit")
	C.addActionItem(s2, 1)
	C.free(unsafe.Pointer(s2))
}

//export onMenuItemActivate
func onMenuItemActivate(item *C.GtkMenuItem, data C.gpointer) {
	idx := int(C.gpointer_to_int(data))
	tray := currentTray
	if tray == nil {
		return
	}
	tray.mu.Lock()
	if idx < 0 || idx >= len(tray.clips) {
		tray.mu.Unlock()
		return
	}
	clip := tray.clips[idx]
	tray.mu.Unlock()

	if !clip.IsImage && clip.Content != "" {
		if err := xmonitor.SetClipboard(clip.Content); err != nil {
			log.Printf("[tray] set clipboard: %v", err)
		}
	}

	select {
	case tray.SelectCh <- clip.ID:
	default:
	}
}

//export onClearActivate
func onClearActivate(item *C.GtkMenuItem, data C.gpointer) {
	if currentTray != nil {
		select {
		case currentTray.ClearCh <- struct{}{}:
		default:
		}
	}
}

//export onQuitActivate
func onQuitActivate(item *C.GtkMenuItem, data C.gpointer) {
	if currentTray != nil {
		select {
		case currentTray.QuitCh <- struct{}{}:
		default:
		}
	}
	C.stopGTK()
}

//export onSearchActivate
func onSearchActivate(item *C.GtkMenuItem, data C.gpointer) {
	PopupShow()
}

//export onSettingsActivate
func onSettingsActivate(item *C.GtkMenuItem, data C.gpointer) {
	if currentTray != nil {
		select {
		case currentTray.SettingsCh <- struct{}{}:
		default:
		}
	}
}

// currentTray holds the active tray instance for CGo callbacks.
var currentTray *Tray

// SetCurrent sets the global tray reference for CGo callbacks.
func SetCurrent(t *Tray) {
	currentTray = t
}

func truncate(s string, maxLen int) string {
	runes := []rune(s)
	for i, r := range runes {
		if r == '\n' || r == '\r' {
			runes[i] = ' '
		}
	}
	if len(runes) > maxLen {
		return string(runes[:maxLen-3]) + "..."
	}
	return string(runes)
}
