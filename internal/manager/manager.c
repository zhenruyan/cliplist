/*
 * manager.c — Paste-style clipboard manager window (GTK3)
 *
 * Card-based grid layout with tags, favorites, categories.
 */

#include "manager.h"
#include <string.h>

/* ── CSS ──────────────────────────────────────────────────────── */

static const char *MANAGER_CSS =
"/* ── sidebar ─────────────────────────── */\n"
".sidebar { background: #252540; }\n"
".cat-btn {\n"
"  background: transparent; color: #7878a0;\n"
"  border: none; border-radius: 8px;\n"
"  padding: 10px 16px; font-size: 13px;\n"
"}\n"
".cat-btn:hover { background: rgba(255,255,255,0.06); color: #b0b0d0; }\n"
".cat-btn.active { background: rgba(255,255,255,0.10); color: #ffffff; font-weight: bold; }\n"
"/* ── content ─────────────────────────── */\n"
".manager-content { background: #f5f5f7; }\n"
".search-entry {\n"
"  background: #ffffff; border: 1px solid #e0e0e0;\n"
"  border-radius: 10px; padding: 8px 14px; font-size: 13px;\n"
"  box-shadow: 0 1px 3px rgba(0,0,0,0.05);\n"
"}\n"
".search-entry:focus { border-color: #4a90d9; box-shadow: 0 0 0 2px rgba(74,144,217,0.15); }\n"
"/* ── grid ────────────────────────────── */\n"
".clips-grid { padding: 16px; background: #f5f5f7; }\n"
"/* ── card ────────────────────────────── */\n"
".clip-card {\n"
"  background: #ffffff; border: 1px solid #e8e8e8;\n"
"  border-radius: 10px; padding: 12px;\n"
"  box-shadow: 0 1px 3px rgba(0,0,0,0.06);\n"
"}\n"
".clip-card:hover {\n"
"  box-shadow: 0 4px 14px rgba(0,0,0,0.10);\n"
"  border-color: #d0d0d0;\n"
"}\n"
".clip-card.copied {\n"
"  border-color: #5cb85c;\n"
"  background: #f0fff0;\n"
"}\n"
".card-text {\n"
"  font-size: 12px; color: #333333;\n"
"}\n"
"/* ── tags ────────────────────────────── */\n"
".tag-pill {\n"
"  color: #ffffff; border-radius: 8px;\n"
"  padding: 1px 8px; font-size: 10px;\n"
"}\n"
".tag-blue   { background: #4a90d9; }\n"
".tag-green  { background: #5cb85c; }\n"
".tag-orange { background: #f0ad4e; }\n"
".tag-red    { background: #d9534f; }\n"
".tag-purple { background: #9b59b6; }\n"
".tag-teal   { background: #2bbbad; }\n"
".tag-pink   { background: #e91e63; }\n"
".tag-indigo { background: #3f51b5; }\n"
".add-tag {\n"
"  color: #b0b0b0; font-size: 13px;\n"
"}\n"
".add-tag:hover { color: #4a90d9; }\n"
"/* ── action bar ──────────────────────── */\n"
".card-action { opacity: 0.4; }\n"
".clip-card:hover .card-action { opacity: 1.0; }\n"
".card-heart { font-size: 14px; }\n"
".card-heart.fav { color: #ff4757; }\n"
".card-heart:not(.fav) { color: #c0c0c0; }\n"
".card-del {\n"
"  background: transparent; border: none;\n"
"  color: transparent; font-size: 14px;\n"
"  padding: 0 4px; border-radius: 6px;\n"
"  min-width: 20px; min-height: 20px;\n"
"}\n"
".clip-card:hover .card-del { color: #c0c0c0; }\n"
".card-del:hover { color: #ff4757; background: rgba(255,71,87,0.08); }\n"
"/* ── empty ───────────────────────────── */\n"
".empty-label { color: #aaaaaa; font-size: 14px; }\n";

/* ── Tag color mapping ────────────────────────────────────────── */

static const char *TAG_CLASSES[] = {
    "tag-blue", "tag-green", "tag-orange", "tag-red",
    "tag-purple", "tag-teal", "tag-pink", "tag-indigo"
};
#define NUM_TAG_COLORS (int)(sizeof(TAG_CLASSES)/sizeof(TAG_CLASSES[0]))

static const char* tagColorClass(const char *tag) {
    unsigned h = 5381;
    for (const char *p = tag; *p; p++)
        h = h * 33 + (unsigned char)*p;
    return TAG_CLASSES[h % NUM_TAG_COLORS];
}

/* ── Globals ──────────────────────────────────────────────────── */

static GtkWidget *mgrWin     = NULL;
static GtkWidget *mgrSidebar = NULL;
static GtkWidget *mgrSearch  = NULL;
static GtkWidget *mgrGrid    = NULL;
static GtkWidget *mgrEmpty   = NULL;
static int        activeCategory = 0;

static const struct { int id; const char *label; const char *icon; } categories[] = {
    { 0, "All Items", "\xe2\x98\xb0" },
    { 1, "Text",      "\xc2\xb6"     },
    { 2, "Images",    "\xe2\x96\xa1"  },
    { 3, "Favorites", "\xe2\x98\x85"  },
};
#define NUM_CATEGORIES (int)(sizeof(categories)/sizeof(categories[0]))

/* ── Helpers ──────────────────────────────────────────────────── */

static void setActiveCat(GtkButton *btn) {
    GList *ch = gtk_container_get_children(GTK_CONTAINER(mgrSidebar));
    for (GList *l = ch; l; l = l->next)
        if (GTK_IS_BUTTON(l->data))
            gtk_style_context_remove_class(
                gtk_widget_get_style_context(GTK_WIDGET(l->data)), "active");
    g_list_free(ch);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(GTK_WIDGET(btn)), "active");
}

static void onCatClick(GtkButton *btn, gpointer data) {
    activeCategory = GPOINTER_TO_INT(data);
    setActiveCat(btn);
    goCategoryChanged(activeCategory);
}

static void onSearchChanged(GtkSearchEntry *e, gpointer d) {
    (void)e; (void)d;
    goManagerSearchChanged();
}

static gboolean onWinKey(GtkWidget *w, GdkEventKey *ev, gpointer d) {
    (void)w; (void)d;
    if (ev->keyval == GDK_KEY_Escape) { hideManagerWindow(); return TRUE; }
    return FALSE;
}

static gboolean onWinDelete(GtkWidget *w, GdkEvent *ev, gpointer d) {
    (void)w; (void)ev; (void)d;
    hideManagerWindow();
    return TRUE;
}

/* ── Copied feedback ──────────────────────────────────────────── */

static gboolean removeCopiedClass(gpointer data) {
    GtkWidget *card = GTK_WIDGET(data);
    gtk_style_context_remove_class(gtk_widget_get_style_context(card), "copied");
    return G_SOURCE_REMOVE;
}

/* ── Card event handlers ──────────────────────────────────────── */

static gboolean onCardEvt(GtkWidget *evtBox, GdkEventButton *ev, gpointer data) {
    (void)evtBox;
    if (ev->button != 1) return FALSE;
    int id = GPOINTER_TO_INT(data);
    goCardClicked(id);
    /* visual feedback on the inner card box */
    GtkWidget *card = gtk_bin_get_child(GTK_BIN(evtBox));
    if (card) {
        gtk_style_context_add_class(gtk_widget_get_style_context(card), "copied");
        g_timeout_add(600, removeCopiedClass, card);
    }
    return TRUE;
}

static gboolean onHeartEvt(GtkWidget *evtBox, GdkEventButton *ev, gpointer data) {
    (void)evtBox; (void)ev;
    int id = GPOINTER_TO_INT(data);
    /* evtBox → bar → card */
    GtkWidget *bar  = gtk_widget_get_parent(evtBox);
    GtkWidget *card = gtk_widget_get_parent(bar);
    GtkWidget *heart = g_object_get_data(G_OBJECT(card), "heart-widget");
    int isFav = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(card), "is-fav"));
    isFav = !isFav;
    g_object_set_data(G_OBJECT(card), "is-fav", GINT_TO_POINTER(isFav));
    GtkStyleContext *ctx = gtk_widget_get_style_context(heart);
    if (isFav) {
        gtk_label_set_markup(GTK_LABEL(heart),
            "<span foreground='#ff4757'>\xe2\x99\xa5</span>");
        gtk_style_context_add_class(ctx, "fav");
    } else {
        gtk_label_set_markup(GTK_LABEL(heart),
            "<span foreground='#c0c0c0'>\xe2\x99\xa1</span>");
        gtk_style_context_remove_class(ctx, "fav");
    }
    goCardFavorited(id);
    return TRUE;
}

static void onDelClick(GtkButton *btn, gpointer data) {
    (void)btn;
    int id = GPOINTER_TO_INT(data);
    GtkWidget *bar  = gtk_widget_get_parent(GTK_WIDGET(btn));
    GtkWidget *card = gtk_widget_get_parent(bar);
    gtk_widget_destroy(card);
    goCardDeleted(id);
}

/* ── Tag event handlers ───────────────────────────────────────── */

static gboolean onTagRemove(GtkWidget *evtBox, GdkEventButton *ev, gpointer d) {
    (void)d;
    if (ev->button != 1) return FALSE;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(evtBox), "clip-id"));
    const char *tag = g_object_get_data(G_OBJECT(evtBox), "tag-name");
    goTagRemoved(id, (char *)tag);
    gtk_widget_destroy(evtBox);
    return TRUE;
}

static gboolean onAddTag(GtkWidget *evtBox, GdkEventButton *ev, gpointer data) {
    (void)evtBox;
    if (ev->button != 1) return FALSE;
    int id = GPOINTER_TO_INT(data);

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Add Tag", NULL, GTK_DIALOG_MODAL,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Add", GTK_RESPONSE_OK, NULL);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 280, -1);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_top(content, 12);
    gtk_widget_set_margin_bottom(content, 12);
    gtk_widget_set_margin_start(content, 12);
    gtk_widget_set_margin_end(content, 12);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Tag name...");
    gtk_container_add(GTK_CONTAINER(content), entry);
    gtk_widget_show_all(dialog);
    gtk_widget_grab_focus(entry);

    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_OK) {
        const char *tag = gtk_entry_get_text(GTK_ENTRY(entry));
        if (tag && strlen(tag) > 0) {
            goTagAdded(id, (char *)tag);
        }
    }
    gtk_widget_destroy(dialog);
    return TRUE;
}

/* ── Build window ─────────────────────────────────────────────── */

void initManagerWindow(void) {
    /* CSS */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, MANAGER_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    /* Window */
    mgrWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(mgrWin), "Cliplist");
    gtk_window_set_default_size(GTK_WINDOW(mgrWin), 1000, 640);
    gtk_window_set_position(GTK_WINDOW(mgrWin), GTK_WIN_POS_CENTER);
    gtk_window_set_type_hint(GTK_WINDOW(mgrWin), GDK_WINDOW_TYPE_HINT_DIALOG);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(mgrWin), hbox);

    /* ── Sidebar ── */
    mgrSidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(mgrSidebar, 190, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(mgrSidebar), "sidebar");
    gtk_box_pack_start(GTK_BOX(hbox), mgrSidebar, FALSE, FALSE, 0);

    gtk_widget_set_margin_top(mgrSidebar, 28);
    gtk_widget_set_margin_start(mgrSidebar, 20);
    gtk_widget_set_margin_bottom(mgrSidebar, 20);

    GtkWidget *titleLabel = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(titleLabel),
        "<span font='18' weight='bold' foreground='#ffffff'>Cliplist</span>");
    gtk_widget_set_halign(titleLabel, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(mgrSidebar), titleLabel, FALSE, FALSE, 0);

    GtkWidget *subLabel = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(subLabel),
        "<span font='9' foreground='#585878'>Clipboard History</span>");
    gtk_widget_set_halign(subLabel, GTK_ALIGN_START);
    gtk_widget_set_margin_top(subLabel, 2);
    gtk_widget_set_margin_bottom(subLabel, 28);
    gtk_box_pack_start(GTK_BOX(mgrSidebar), subLabel, FALSE, FALSE, 0);

    for (int i = 0; i < NUM_CATEGORIES; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "  %s  %s", categories[i].icon, categories[i].label);
        GtkWidget *btn = gtk_button_new_with_label(buf);
        gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
        gtk_widget_set_halign(btn, GTK_ALIGN_FILL);
        gtk_widget_set_margin_start(btn, 8);
        gtk_widget_set_margin_end(btn, 8);
        gtk_widget_set_margin_bottom(btn, 2);
        GtkStyleContext *bCtx = gtk_widget_get_style_context(btn);
        gtk_style_context_add_class(bCtx, "cat-btn");
        if (i == 0) gtk_style_context_add_class(bCtx, "active");
        g_signal_connect(btn, "clicked", G_CALLBACK(onCatClick), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(mgrSidebar), btn, FALSE, FALSE, 0);
    }

    GtkWidget *spacer = gtk_label_new(NULL);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(mgrSidebar), spacer, TRUE, TRUE, 0);

    /* ── Content ── */
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(content), "manager-content");
    gtk_box_pack_start(GTK_BOX(hbox), content, TRUE, TRUE, 0);

    /* search bar */
    GtkWidget *searchBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_top(searchBox, 16);
    gtk_widget_set_margin_start(searchBox, 20);
    gtk_widget_set_margin_end(searchBox, 20);
    gtk_widget_set_margin_bottom(searchBox, 12);
    gtk_box_pack_start(GTK_BOX(content), searchBox, FALSE, FALSE, 0);

    mgrSearch = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(mgrSearch), "Search clips...");
    gtk_widget_set_hexpand(mgrSearch, TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(mgrSearch), "search-entry");
    g_signal_connect(mgrSearch, "search-changed", G_CALLBACK(onSearchChanged), NULL);
    gtk_box_pack_start(GTK_BOX(searchBox), mgrSearch, TRUE, TRUE, 0);

    /* scroll + flow grid */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);

    mgrGrid = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(mgrGrid), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(mgrGrid), TRUE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(mgrGrid), 20);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(mgrGrid), 14);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(mgrGrid), 14);
    gtk_style_context_add_class(gtk_widget_get_style_context(mgrGrid), "clips-grid");
    gtk_container_add(GTK_CONTAINER(scroll), mgrGrid);

    /* empty placeholder */
    mgrEmpty = gtk_label_new("No clips yet");
    gtk_style_context_add_class(gtk_widget_get_style_context(mgrEmpty), "empty-label");
    gtk_widget_set_no_show_all(mgrEmpty, TRUE);
    gtk_widget_set_vexpand(mgrEmpty, TRUE);
    gtk_widget_set_halign(mgrEmpty, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(mgrEmpty, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(content), mgrEmpty, TRUE, FALSE, 0);

    /* signals */
    g_signal_connect(mgrWin, "delete-event", G_CALLBACK(onWinDelete), NULL);
    g_signal_connect(mgrWin, "key-press-event", G_CALLBACK(onWinKey), NULL);

    gtk_widget_show_all(mgrWin);
    gtk_widget_hide(mgrWin);
}

/* ── Visibility ───────────────────────────────────────────────── */

void showManagerWindow(void)  { gtk_widget_show_all(mgrWin); gtk_window_present(GTK_WINDOW(mgrWin)); }
void hideManagerWindow(void)  { gtk_widget_hide(mgrWin); }
void toggleManagerWindow(void) {
    if (gtk_widget_get_visible(mgrWin)) hideManagerWindow();
    else showManagerWindow();
}

/* ── Grid ─────────────────────────────────────────────────────── */

void clearGrid(void) {
    GList *ch = gtk_container_get_children(GTK_CONTAINER(mgrGrid));
    for (GList *l = ch; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(ch);
    gtk_widget_hide(mgrEmpty);
}

void showEmpty(void) {
    gtk_widget_show(mgrEmpty);
}

/* ── Add a tag pill to a tag-bar box ──────────────────────────── */

static void addTagPill(GtkWidget *tagBar, int clipId, const char *tag) {
    GtkWidget *evtBox = gtk_event_box_new();
    GtkWidget *label  = gtk_label_new(tag);

    GtkStyleContext *ctx = gtk_widget_get_style_context(label);
    gtk_style_context_add_class(ctx, "tag-pill");
    gtk_style_context_add_class(ctx, tagColorClass(tag));

    gtk_container_add(GTK_CONTAINER(evtBox), label);

    g_object_set_data(G_OBJECT(evtBox), "clip-id", GINT_TO_POINTER(clipId));
    g_object_set_data_full(G_OBJECT(evtBox), "tag-name", g_strdup(tag), g_free);

    g_signal_connect(evtBox, "button-press-event", G_CALLBACK(onTagRemove), NULL);

    gtk_box_pack_start(GTK_BOX(tagBar), evtBox, FALSE, FALSE, 0);
    gtk_widget_show_all(evtBox);
}

/* ── Add a clip card to the grid ──────────────────────────────── */

void addClipCard(int id, const char *text, int textLen,
                 int isImage, const char *imagePath, int isFav,
                 const char *tags)
{
    /* outer event-box for click-to-copy */
    GtkWidget *cardEvt = gtk_event_box_new();
    GtkWidget *card    = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_size_request(cardEvt, 220, -1);
    gtk_container_add(GTK_CONTAINER(cardEvt), card);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "clip-card");

    g_object_set_data(G_OBJECT(card), "clip-id", GINT_TO_POINTER(id));
    g_object_set_data(G_OBJECT(card), "is-fav",  GINT_TO_POINTER(isFav));

    /* ── preview ── */
    if (isImage && imagePath) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(
            imagePath, 196, 80, TRUE, NULL);
        if (pb) {
            GtkWidget *img = gtk_image_new_from_pixbuf(pb);
            g_object_unref(pb);
            gtk_widget_set_halign(img, GTK_ALIGN_CENTER);
            gtk_box_pack_start(GTK_BOX(card), img, FALSE, FALSE, 0);
        } else {
            GtkWidget *ph = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(ph),
                "<span foreground='#aaaaaa'>[image]</span>");
            gtk_widget_set_halign(ph, GTK_ALIGN_CENTER);
            gtk_box_pack_start(GTK_BOX(card), ph, FALSE, FALSE, 4);
        }
    } else if (text && textLen > 0) {
        int len = textLen < 400 ? textLen : 400;
        char *dup = g_strndup(text, len);
        GtkWidget *label = gtk_label_new(dup);
        g_free(dup);
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_label_set_yalign(GTK_LABEL(label), 0);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_lines(GTK_LABEL(label), 4);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_style_context_add_class(gtk_widget_get_style_context(label), "card-text");
        gtk_box_pack_start(GTK_BOX(card), label, FALSE, FALSE, 0);
    }

    /* ── tag bar ── */
    GtkWidget *tagBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(card), tagBar, FALSE, FALSE, 0);

    if (tags && tags[0]) {
        char **list = g_strsplit(tags, ",", -1);
        for (int i = 0; list[i]; i++) {
            if (list[i][0])
                addTagPill(tagBar, id, list[i]);
        }
        g_strfreev(list);
    }

    /* [+] add tag button */
    GtkWidget *addEvt   = gtk_event_box_new();
    GtkWidget *addLabel = gtk_label_new("+");
    gtk_style_context_add_class(gtk_widget_get_style_context(addLabel), "add-tag");
    gtk_container_add(GTK_CONTAINER(addEvt), addLabel);
    g_signal_connect(addEvt, "button-press-event",
        G_CALLBACK(onAddTag), GINT_TO_POINTER(id));
    gtk_box_pack_start(GTK_BOX(tagBar), addEvt, FALSE, FALSE, 0);

    /* ── action bar ── */
    GtkWidget *actionBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkStyleContext *aCtx = gtk_widget_get_style_context(actionBar);
    gtk_style_context_add_class(aCtx, "card-action");
    gtk_box_pack_start(GTK_BOX(card), actionBar, FALSE, FALSE, 0);

    /* heart */
    GtkWidget *heartEvt = gtk_event_box_new();
    GtkWidget *heart    = gtk_label_new(NULL);
    GtkStyleContext *hCtx = gtk_widget_get_style_context(heart);
    gtk_style_context_add_class(hCtx, "card-heart");
    if (isFav) {
        gtk_label_set_markup(GTK_LABEL(heart),
            "<span foreground='#ff4757'>\xe2\x99\xa5</span>");
        gtk_style_context_add_class(hCtx, "fav");
    } else {
        gtk_label_set_markup(GTK_LABEL(heart),
            "<span foreground='#c0c0c0'>\xe2\x99\xa1</span>");
    }
    gtk_container_add(GTK_CONTAINER(heartEvt), heart);
    gtk_widget_set_margin_start(heartEvt, 2);
    g_signal_connect(heartEvt, "button-press-event",
        G_CALLBACK(onHeartEvt), GINT_TO_POINTER(id));
    gtk_box_pack_start(GTK_BOX(actionBar), heartEvt, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(card), "heart-widget", heart);

    /* spacer */
    GtkWidget *sp = gtk_label_new(NULL);
    gtk_widget_set_hexpand(sp, TRUE);
    gtk_box_pack_start(GTK_BOX(actionBar), sp, TRUE, TRUE, 0);

    /* delete */
    GtkWidget *delBtn = gtk_button_new_with_label("\xc3\x97");
    gtk_button_set_relief(GTK_BUTTON(delBtn), GTK_RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(delBtn), "card-del");
    g_signal_connect(delBtn, "clicked",
        G_CALLBACK(onDelClick), GINT_TO_POINTER(id));
    gtk_box_pack_start(GTK_BOX(actionBar), delBtn, FALSE, FALSE, 0);

    /* card click → copy */
    g_signal_connect(cardEvt, "button-press-event",
        G_CALLBACK(onCardEvt), GINT_TO_POINTER(id));

    gtk_flow_box_insert(GTK_FLOW_BOX(mgrGrid), cardEvt, -1);
    gtk_widget_show_all(cardEvt);
}

/* ── Idle callbacks ────────────────────────────────────────────── */

gboolean rebuildGridIdle(gpointer data) {
    (void)data;
    goRebuildGrid();
    return G_SOURCE_REMOVE;
}

gboolean showManagerIdle(gpointer data) {
    (void)data;
    showManagerWindow();
    return G_SOURCE_REMOVE;
}

gboolean toggleManagerIdle(gpointer data) {
    (void)data;
    toggleManagerWindow();
    return G_SOURCE_REMOVE;
}

gboolean stopIdle(gpointer data) {
    (void)data;
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

const char *getManagerSearchText(void) {
    return gtk_entry_get_text(GTK_ENTRY(mgrSearch));
}
