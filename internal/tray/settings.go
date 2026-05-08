package tray

/*
#include <gtk/gtk.h>

extern void onSettingsApply(GtkWidget *widget, gpointer data);
extern void onSettingsCancel(GtkWidget *widget, gpointer data);
extern gboolean showSettingsIdle(gpointer data);
extern void goSettingsApplied(int maxHistory, char *pollInterval, char *pasteDelay,
    int ignoreWS, int autoStart, int maxLengthKB, char *hotkeyMods, char *hotkeyKey);

static GtkWidget *settings_win = NULL;
static GtkWidget *w_max_history;
static GtkWidget *w_poll_interval;
static GtkWidget *w_paste_delay;
static GtkWidget *w_ignore_ws;
static GtkWidget *w_auto_start;
static GtkWidget *w_max_length;
static GtkWidget *w_hotkey_mods;
static GtkWidget *w_hotkey_key;

static void initSettingsWindow(
    int maxHistory,
    const char *pollInterval,
    const char *pasteDelay,
    int ignoreWS,
    int autoStart,
    int maxLength,
    const char *hotkeyMods,
    const char *hotkeyKey
) {
    settings_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(settings_win), "Cliplist Settings");
    gtk_window_set_default_size(GTK_WINDOW(settings_win), 420, -1);
    gtk_window_set_position(GTK_WINDOW(settings_win), GTK_WIN_POS_CENTER);
    gtk_window_set_type_hint(GTK_WINDOW(settings_win), GDK_WINDOW_TYPE_HINT_DIALOG);
    g_signal_connect(settings_win, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 16);
    gtk_container_add(GTK_CONTAINER(settings_win), vbox);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_pack_start(GTK_BOX(vbox), grid, TRUE, TRUE, 0);

    int row = 0;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Max History:"), 0, row, 1, 1);
    w_max_history = gtk_spin_button_new_with_range(10, 100000, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w_max_history), maxHistory);
    gtk_grid_attach(GTK_GRID(grid), w_max_history, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Poll Interval:"), 0, row, 1, 1);
    w_poll_interval = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(w_poll_interval), pollInterval);
    gtk_grid_attach(GTK_GRID(grid), w_poll_interval, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Paste Delay:"), 0, row, 1, 1);
    w_paste_delay = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(w_paste_delay), pasteDelay);
    gtk_grid_attach(GTK_GRID(grid), w_paste_delay, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Max Content (KB):"), 0, row, 1, 1);
    w_max_length = gtk_spin_button_new_with_range(1, 102400, 64);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w_max_length), maxLength / 1024);
    gtk_grid_attach(GTK_GRID(grid), w_max_length, 1, row, 1, 1);
    row++;

    w_ignore_ws = gtk_check_button_new_with_label("Ignore Whitespace Changes");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w_ignore_ws), ignoreWS);
    gtk_grid_attach(GTK_GRID(grid), w_ignore_ws, 0, row, 2, 1);
    row++;

    w_auto_start = gtk_check_button_new_with_label("Auto Start");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w_auto_start), autoStart);
    gtk_grid_attach(GTK_GRID(grid), w_auto_start, 0, row, 2, 1);

    // Separator
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    // Hotkey section
    GtkWidget *hk_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(hk_label), "<b>Global Hotkey</b>");
    gtk_label_set_xalign(GTK_LABEL(hk_label), 0);
    gtk_box_pack_start(GTK_BOX(vbox), hk_label, FALSE, FALSE, 0);

    GtkWidget *hk_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(hk_grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(hk_grid), 12);
    gtk_box_pack_start(GTK_BOX(vbox), hk_grid, TRUE, TRUE, 0);

    gtk_grid_attach(GTK_GRID(hk_grid), gtk_label_new("Modifiers:"), 0, 0, 1, 1);
    w_hotkey_mods = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(w_hotkey_mods), hotkeyMods);
    gtk_entry_set_placeholder_text(GTK_ENTRY(w_hotkey_mods), "ctrl, shift");
    gtk_grid_attach(GTK_GRID(hk_grid), w_hotkey_mods, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(hk_grid), gtk_label_new("Key:"), 0, 1, 1, 1);
    w_hotkey_key = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(w_hotkey_key), hotkeyKey);
    gtk_entry_set_placeholder_text(GTK_ENTRY(w_hotkey_key), "v");
    gtk_grid_attach(GTK_GRID(hk_grid), w_hotkey_key, 1, 1, 1, 1);

    // Buttons
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 8);

    GtkWidget *btn_apply = gtk_button_new_with_label("Apply");
    g_signal_connect(btn_apply, "clicked", G_CALLBACK(onSettingsApply), NULL);
    gtk_box_pack_end(GTK_BOX(btn_box), btn_apply, FALSE, FALSE, 0);

    GtkWidget *btn_cancel = gtk_button_new_with_label("Cancel");
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(onSettingsCancel), NULL);
    gtk_box_pack_end(GTK_BOX(btn_box), btn_cancel, FALSE, FALSE, 0);

    gtk_widget_show_all(settings_win);
}

static void applySettings(void) {
    int maxHistory = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(w_max_history));
    char *pollInterval = (char*)gtk_entry_get_text(GTK_ENTRY(w_poll_interval));
    char *pasteDelay = (char*)gtk_entry_get_text(GTK_ENTRY(w_paste_delay));
    int maxLengthKB = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(w_max_length));
    int ignoreWS = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w_ignore_ws)) ? 1 : 0;
    int autoStart = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w_auto_start)) ? 1 : 0;
    char *hotkeyMods = (char*)gtk_entry_get_text(GTK_ENTRY(w_hotkey_mods));
    char *hotkeyKey = (char*)gtk_entry_get_text(GTK_ENTRY(w_hotkey_key));
    goSettingsApplied(maxHistory, pollInterval, pasteDelay, ignoreWS, autoStart, maxLengthKB, hotkeyMods, hotkeyKey);
}

static void hideSettingsWindow(void) {
    if (settings_win) {
        gtk_widget_hide(settings_win);
    }
}
*/
import "C"

import (
	"log"
	"strings"
	"unsafe"
)

// settingsApplyCallback is called when the user clicks Apply.
var settingsApplyCallback func(maxHistory int, pollInterval, pasteDelay string, ignoreWS, autoStart bool, maxLengthKB int, hotkeyMods, hotkeyKey string)

// pendingSettings stores settings params for the idle callback (Go types only).
var pendingSettings *settingsData

type settingsData struct {
	maxHistory    int
	pollInterval  string
	pasteDelay    string
	ignoreWS      int
	autoStart     int
	maxLength     int
	hotkeyMods    string
	hotkeyKey     string
}

// SettingsShow opens the settings panel with the given config values.
func SettingsShow(
	maxHistory int,
	pollInterval string,
	pasteDelay string,
	ignoreWS bool,
	autoStart bool,
	maxLength int,
	hotkeyMods []string,
	hotkeyKey string,
	applyFn func(maxHistory int, pollInterval, pasteDelay string, ignoreWS, autoStart bool, maxLengthKB int, hotkeyMods, hotkeyKey string),
) {
	settingsApplyCallback = applyFn
	iw := 0
	if ignoreWS {
		iw = 1
	}
	as := 0
	if autoStart {
		as = 1
	}
	pendingSettings = &settingsData{
		maxHistory:   maxHistory,
		pollInterval: pollInterval,
		pasteDelay:   pasteDelay,
		ignoreWS:     iw,
		autoStart:    as,
		maxLength:    maxLength,
		hotkeyMods:   strings.Join(hotkeyMods, ", "),
		hotkeyKey:    hotkeyKey,
	}
	C.g_idle_add(C.GSourceFunc(C.showSettingsIdle), nil)
}

//export showSettingsIdle
func showSettingsIdle(data C.gpointer) C.gboolean {
	if pendingSettings == nil {
		return C.FALSE
	}
	p := pendingSettings
	pendingSettings = nil

	cPoll := C.CString(p.pollInterval)
	defer C.free(unsafe.Pointer(cPoll))
	cPaste := C.CString(p.pasteDelay)
	defer C.free(unsafe.Pointer(cPaste))
	cMods := C.CString(p.hotkeyMods)
	defer C.free(unsafe.Pointer(cMods))
	cKey := C.CString(p.hotkeyKey)
	defer C.free(unsafe.Pointer(cKey))

	C.initSettingsWindow(
		C.int(p.maxHistory),
		cPoll, cPaste,
		C.int(p.ignoreWS), C.int(p.autoStart),
		C.int(p.maxLength),
		cMods, cKey,
	)
	return C.FALSE
}

//export onSettingsApply
func onSettingsApply(widget *C.GtkWidget, data C.gpointer) {
	log.Println("[settings] apply clicked")
	C.applySettings()
}

//export goSettingsApplied
func goSettingsApplied(
	maxHistory C.int,
	pollInterval *C.char,
	pasteDelay *C.char,
	ignoreWS C.int,
	autoStart C.int,
	maxLengthKB C.int,
	hotkeyMods *C.char,
	hotkeyKey *C.char,
) {
	if settingsApplyCallback == nil {
		return
	}
	settingsApplyCallback(
		int(maxHistory),
		C.GoString(pollInterval),
		C.GoString(pasteDelay),
		ignoreWS != 0,
		autoStart != 0,
		int(maxLengthKB),
		C.GoString(hotkeyMods),
		C.GoString(hotkeyKey),
	)
	C.hideSettingsWindow()
}

//export onSettingsCancel
func onSettingsCancel(widget *C.GtkWidget, data C.gpointer) {
	C.hideSettingsWindow()
}
