package tray

/*
#cgo pkg-config: gtk+-3.0
#cgo CFLAGS: -Wno-deprecated-declarations

#include <gtk/gtk.h>
#include <stdlib.h>

// Go callbacks
extern void goSearchChanged();
extern void goRowActivated(int id);
extern void goPopupClosed();

// Globals
static GtkWidget *g_popup   = NULL;
static GtkWidget *g_search  = NULL;
static GtkWidget *g_listbox = NULL;

static const char* getSearchText() {
    return gtk_entry_get_text(GTK_ENTRY(g_search));
}

static gboolean on_key_press(GtkWidget *w, GdkEventKey *ev, gpointer data) {
    (void)w; (void)data;
    if (ev->keyval == GDK_KEY_Escape) {
        gtk_widget_hide(g_popup);
        goPopupClosed();
        return TRUE;
    }
    if (ev->keyval == GDK_KEY_Down || ev->keyval == GDK_KEY_Up) {
        gtk_widget_grab_focus(g_listbox);
        GdkEvent *copy = gdk_event_copy((GdkEvent*)ev);
        gtk_widget_event(g_listbox, copy);
        gdk_event_free(copy);
        return TRUE;
    }
    if (ev->keyval == GDK_KEY_Return || ev->keyval == GDK_KEY_KP_Enter) {
        GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(g_listbox));
        if (row) {
            g_signal_emit_by_name(g_listbox, "row-activated", row);
        }
        return TRUE;
    }
    return FALSE;
}

static void on_search_changed(GtkSearchEntry *e, gpointer data) {
    (void)e; (void)data;
    goSearchChanged();
}

static void on_row_activated(GtkListBox *lb, GtkListBoxRow *row, gpointer data) {
    (void)lb; (void)data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "clip-id"));
    goRowActivated(id);
}

static void initPopup() {
    g_popup = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_popup), "Cliplist");
    gtk_window_set_default_size(GTK_WINDOW(g_popup), 450, 400);
    gtk_window_set_position(GTK_WINDOW(g_popup), GTK_WIN_POS_CENTER);
    gtk_window_set_type_hint(GTK_WINDOW(g_popup), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_keep_above(GTK_WINDOW(g_popup), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(g_popup), TRUE);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    gtk_container_add(GTK_CONTAINER(g_popup), vbox);

    g_search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_search), "Search clips...");
    gtk_box_pack_start(GTK_BOX(vbox), g_search, FALSE, FALSE, 0);
    g_signal_connect(g_search, "search-changed", G_CALLBACK(on_search_changed), NULL);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    g_listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(g_listbox), GTK_SELECTION_SINGLE);
    g_signal_connect(g_listbox, "row-activated", G_CALLBACK(on_row_activated), NULL);
    gtk_container_add(GTK_CONTAINER(scroll), g_listbox);

    g_signal_connect(g_popup, "key-press-event", G_CALLBACK(on_key_press), NULL);
    g_signal_connect(g_popup, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    gtk_widget_show_all(g_popup);
    gtk_widget_hide(g_popup);
}

static void clearList() {
    GList *children = gtk_container_get_children(GTK_CONTAINER(g_listbox));
    for (GList *l = children; l != NULL; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);
}

static void addClipRow(const char *text, int id) {
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 70);

    GtkWidget *row = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(row), label);
    g_object_set_data(G_OBJECT(row), "clip-id", GINT_TO_POINTER(id));
    gtk_list_box_insert(GTK_LIST_BOX(g_listbox), row, -1);
    gtk_widget_show_all(row);  // show row AND its children
}

static void addImageRow(const char *imagePath, int id) {
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(imagePath, 48, 48, TRUE, NULL);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    if (pixbuf) {
        GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
        g_object_unref(pixbuf);
        gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 4);
    } else {
        GtkWidget *ph = gtk_label_new("[img]");
        gtk_box_pack_start(GTK_BOX(hbox), ph, FALSE, FALSE, 4);
    }

    GtkWidget *label = gtk_label_new("(image)");
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    GtkWidget *row = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(row), hbox);
    g_object_set_data(G_OBJECT(row), "clip-id", GINT_TO_POINTER(id));
    gtk_list_box_insert(GTK_LIST_BOX(g_listbox), row, -1);
    gtk_widget_show_all(row);
}

static void selectFirstRow() {
    GtkListBoxRow *first = gtk_list_box_get_row_at_index(GTK_LIST_BOX(g_listbox), 0);
    if (first) {
        gtk_list_box_select_row(GTK_LIST_BOX(g_listbox), first);
    }
}

static gboolean show_popup_idle(gpointer data) {
    (void)data;
    gtk_widget_show_all(g_popup);
    gtk_window_present(GTK_WINDOW(g_popup));
    // block signal to avoid double goSearchChanged call
    g_signal_handlers_block_by_func(g_search, on_search_changed, NULL);
    gtk_entry_set_text(GTK_ENTRY(g_search), "");
    g_signal_handlers_unblock_by_func(g_search, on_search_changed, NULL);
    gtk_widget_grab_focus(g_search);
    goSearchChanged();
    return FALSE;
}

static gboolean toggle_popup_idle(gpointer data) {
    (void)data;
    if (gtk_widget_get_visible(g_popup)) {
        gtk_widget_hide(g_popup);
    } else {
        gtk_widget_show_all(g_popup);
        gtk_window_present(GTK_WINDOW(g_popup));
        g_signal_handlers_block_by_func(g_search, on_search_changed, NULL);
        gtk_entry_set_text(GTK_ENTRY(g_search), "");
        g_signal_handlers_unblock_by_func(g_search, on_search_changed, NULL);
        gtk_widget_grab_focus(g_search);
        goSearchChanged();
    }
    return FALSE;
}

static void scheduleShowPopup() {
    g_idle_add(show_popup_idle, NULL);
}

static void scheduleTogglePopup() {
    g_idle_add(toggle_popup_idle, NULL);
}

static void hidePopup() {
    if (g_popup) gtk_widget_hide(g_popup);
}

static gboolean isPopupVisible() {
    return g_popup && gtk_widget_get_visible(g_popup);
}

static int gp_to_int(gpointer p) { return GPOINTER_TO_INT(p); }
*/
import "C"
import (
	"log"
	"unsafe"

	"github.com/user/cliplist/internal/store"
	"github.com/user/cliplist/internal/xmonitor"
)

var popupStore *store.Store

// SetPopupStore sets the store used by the popup for searching.
func SetPopupStore(s *store.Store) {
	popupStore = s
}

// PopupInit creates the GTK popup window. Call once at startup.
func PopupInit() {
	C.initPopup()
}

// PopupShow shows the popup (thread-safe via g_idle_add).
func PopupShow() {
	C.scheduleShowPopup()
}

// PopupHide hides the popup.
func PopupHide() {
	C.hidePopup()
}

// PopupToggle shows/hides the popup (thread-safe via g_idle_add).
func PopupToggle() {
	C.scheduleTogglePopup()
}

// PopupVisible returns whether the popup is currently visible.
func PopupVisible() bool {
	return C.isPopupVisible() != 0
}

// popupMaxDisplay is the max number of clips shown when search is empty.
var popupMaxDisplay = 200

// SetPopupMaxDisplay sets the maximum number of clips shown in the popup.
func SetPopupMaxDisplay(n int) {
	if n > 0 {
		popupMaxDisplay = n
	}
}

//export goSearchChanged
func goSearchChanged() {
	if popupStore == nil {
		log.Println("[popup] store is nil!")
		return
	}
	q := C.GoString(C.getSearchText())

	var clips []store.Clip
	var err error
	if q == "" {
		clips, err = popupStore.List(popupMaxDisplay)
	} else {
		clips, err = popupStore.Search(q, 50)
	}
	if err != nil {
		log.Printf("[popup] search error: %v", err)
		return
	}
	log.Printf("[popup] loaded %d clips for query=%q", len(clips), q)

	C.clearList()
	for _, c := range clips {
		if c.IsImage {
			cPath := C.CString(c.ImagePath)
			C.addImageRow(cPath, C.int(c.ID))
			C.free(unsafe.Pointer(cPath))
		} else {
			label := clipDisplayLabel(c)
			cLabel := C.CString(label)
			C.addClipRow(cLabel, C.int(c.ID))
			C.free(unsafe.Pointer(cLabel))
		}
	}
	C.selectFirstRow()
}

//export goRowActivated
func goRowActivated(id C.int) {
	if popupStore == nil {
		return
	}
	clip, err := popupStore.GetByID(int64(id))
	if err != nil {
		log.Printf("[popup] get clip %d: %v", id, err)
		return
	}

	if err := xmonitor.SetClipboard(clip.Content); err != nil {
		log.Printf("[popup] set clipboard: %v", err)
		return
	}

	C.hidePopup()
}

//export goPopupClosed
func goPopupClosed() {}

func clipDisplayLabel(c store.Clip) string {
	runes := []rune(c.Content)
	for i, r := range runes {
		if r == '\n' || r == '\r' {
			runes[i] = ' '
		}
	}
	s := string(runes)
	if len(s) > 100 {
		s = s[:97] + "..."
	}
	return s
}
