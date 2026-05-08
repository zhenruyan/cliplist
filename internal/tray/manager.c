/*
 * manager.c — Paste-style clipboard manager window (GTK3)
 *
 * Layout:
 *   ┌────────────┬──────────────────────────────────┐
 *   │  sidebar   │  search  ·······················  │
 *   │            │  [☰ All] [T Text] [□ Img] [★ Fav]│
 *   │  Cliplist  │  ┌──────┐ ┌──────┐ ┌──────┐      │
 *   │            │  │ clip │ │ clip │ │ clip │      │
 *   │  All Items │  │      │ │      │ │      │      │
 *   │  Text      │  │ ♡  × │ │ ♥  × │ │ ♡  × │      │
 *   │  Images    │  └──────┘ └──────┘ └──────┘      │
 *   │  Favorites │  ┌──────┐ ┌──────┐               │
 *   │            │  │ clip │ │ clip │               │
 *   └────────────┴──┴──────┴┴──────┘───────────────-┘
 */

#include "manager.h"
#include <string.h>

/* ── CSS ──────────────────────────────────────────────────────── */

static const char *MANAGER_CSS =
"/* ── sidebar ─────────────────────────── */\n"
".sidebar { background: #1a1a2e; }\n"
".sidebar-title { color: #ffffff; font-size: 18px; font-weight: bold; }\n"
".sidebar-sub { color: #6c6c80; font-size: 11px; }\n"
".cat-btn {\n"
"  background: transparent; color: #8888a0;\n"
"  border: none; border-radius: 8px;\n"
"  padding: 9px 14px; font-size: 13px;\n"
"}\n"
".cat-btn:hover { background: rgba(255,255,255,0.07); color: #c8c8e0; }\n"
".cat-btn.active { background: rgba(255,255,255,0.12); color: #ffffff; font-weight: bold; }\n"
"/* ── content ─────────────────────────── */\n"
".manager-content { background: #f0f2f5; }\n"
".search-entry {\n"
"  background: #ffffff; border: 1px solid #dcdcdc;\n"
"  border-radius: 10px; padding: 7px 14px; font-size: 13px;\n"
"  caret-color: #4a90d9;\n"
"}\n"
".search-entry:focus { border-color: #4a90d9; }\n"
"/* ── grid / cards ────────────────────── */\n"
".clips-grid { padding: 12px; }\n"
".clip-card {\n"
"  background: #ffffff; border: 1px solid #e4e4e4;\n"
"  border-radius: 12px; padding: 0;\n"
"}\n"
".clip-card:hover { background: #f7f8fa; border-color: #cccccc; }\n"
".card-text { font-size: 12px; color: #333333; }\n"
".card-heart { font-size: 15px; padding: 0 4px; }\n"
".card-heart.fav { color: #ff4757; }\n"
".card-heart:not(.fav) { color: #c0c0c0; }\n"
".card-del {\n"
"  background: transparent; border: none;\n"
"  color: transparent; font-size: 14px;\n"
"  padding: 0 6px; border-radius: 8px;\n"
"  min-width: 22px; min-height: 22px;\n"
"}\n"
".clip-card:hover .card-del { color: #b0b0b0; }\n"
".card-del:hover { color: #ff4757; background: rgba(255,71,87,0.08); }\n"
".empty-label { color: #aaaaaa; font-size: 14px; }\n";

/* ── Globals ──────────────────────────────────────────────────── */

static GtkWidget *mgrWin     = NULL;
static GtkWidget *mgrSidebar = NULL;
static GtkWidget *mgrSearch  = NULL;
static GtkWidget *mgrGrid    = NULL;
static GtkWidget *mgrEmpty   = NULL;
static int        activeCategory = 0;

static const Category categories[] = {
    { 0, "All Items", "\xe2\x98\xb0" },   /* ☰ */
    { 1, "Text",      "\xc2\xb6"    },     /* ¶ */
    { 2, "Images",    "\xe2\x96\xa1" },    /* □ */
    { 3, "Favorites", "\xe2\x98\x85" },    /* ★ */
};
#define NUM_CATEGORIES (int)(sizeof(categories)/sizeof(categories[0]))

/* ── Helpers ──────────────────────────────────────────────────── */

static void setActiveCat(GtkButton *btn) {
    GList *ch = gtk_container_get_children(GTK_CONTAINER(mgrSidebar));
    for (GList *l = ch; l; l = l->next) {
        if (GTK_IS_BUTTON(l->data))
            gtk_style_context_remove_class(
                gtk_widget_get_style_context(GTK_WIDGET(l->data)), "active");
    }
    g_list_free(ch);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(GTK_WIDGET(btn)), "active");
}

static void onCatClick(GtkButton *btn, gpointer data) {
    int id = GPOINTER_TO_INT(data);
    activeCategory = id;
    setActiveCat(btn);
    goCategoryChanged(id);
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

static void onResize(GtkWidget *w, GdkRectangle *alloc, gpointer d) {
    (void)w; (void)d; (void)alloc;
    /* FlowBox wraps automatically; nothing to do */
}

static gboolean onHeartEvt(GtkWidget *evtBox, GdkEventButton *ev, gpointer data) {
    (void)evtBox; (void)ev;
    int id = GPOINTER_TO_INT(data);

    /* walk: evtBox → bottomBar → card */
    GtkWidget *bottomBar = gtk_widget_get_parent(evtBox);
    GtkWidget *card      = gtk_widget_get_parent(bottomBar);
    GtkWidget *heart     = g_object_get_data(G_OBJECT(card), "heart-widget");
    int isFav = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(card), "is-fav"));

    isFav = !isFav;
    g_object_set_data(G_OBJECT(card), "is-fav", GINT_TO_POINTER(isFav));

    GtkStyleContext *ctx = gtk_widget_get_style_context(heart);
    if (isFav) {
        gtk_label_set_markup(GTK_LABEL(heart),
            "<span foreground='#ff4757'>\xe2\x99\xa5</span>"); /* ♥ */
        gtk_style_context_add_class(ctx, "fav");
    } else {
        gtk_label_set_markup(GTK_LABEL(heart),
            "<span foreground='#c0c0c0'>\xe2\x99\xa1</span>"); /* ♡ */
        gtk_style_context_remove_class(ctx, "fav");
    }
    goCardFavorited(id);
    return TRUE;
}

static void onDelClick(GtkButton *btn, gpointer data) {
    (void)btn;
    int id = GPOINTER_TO_INT(data);
    /* btn → bottomBar → card */
    GtkWidget *bottomBar = gtk_widget_get_parent(GTK_WIDGET(btn));
    GtkWidget *card      = gtk_widget_get_parent(bottomBar);
    gtk_widget_destroy(card);
    goCardDeleted(id);
}

static gboolean onCardEvt(GtkWidget *evtBox, GdkEventButton *ev, gpointer data) {
    (void)evtBox;
    if (ev->button != 1) return FALSE;
    goCardClicked(GPOINTER_TO_INT(data));
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
    gtk_window_set_default_size(GTK_WINDOW(mgrWin), 960, 600);
    gtk_window_set_position(GTK_WINDOW(mgrWin), GTK_WIN_POS_CENTER);
    gtk_window_set_type_hint(GTK_WINDOW(mgrWin), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_keep_above(GTK_WINDOW(mgrWin), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(mgrWin), TRUE);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(mgrWin), hbox);

    /* ── Sidebar ── */
    mgrSidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(mgrSidebar, 200, -1);
    GtkStyleContext *sbCtx = gtk_widget_get_style_context(mgrSidebar);
    gtk_style_context_add_class(sbCtx, "sidebar");
    gtk_box_pack_start(GTK_BOX(hbox), mgrSidebar, FALSE, FALSE, 0);

    /* title block */
    gtk_widget_set_margin_top(mgrSidebar, 28);
    gtk_widget_set_margin_start(mgrSidebar, 20);
    gtk_widget_set_margin_bottom(mgrSidebar, 20);
    GtkWidget *titleLabel = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(titleLabel),
        "<span font='20' weight='bold' foreground='#ffffff'>Cliplist</span>");
    gtk_widget_set_halign(titleLabel, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(mgrSidebar), titleLabel, FALSE, FALSE, 0);

    GtkWidget *subLabel = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(subLabel),
        "<span font='10' foreground='#6c6c80'>Clipboard History</span>");
    gtk_widget_set_halign(subLabel, GTK_ALIGN_START);
    gtk_widget_set_margin_top(subLabel, 2);
    gtk_widget_set_margin_bottom(subLabel, 24);
    gtk_box_pack_start(GTK_BOX(mgrSidebar), subLabel, FALSE, FALSE, 0);

    /* category buttons */
    for (int i = 0; i < NUM_CATEGORIES; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), " %s  %s", categories[i].icon, categories[i].label);
        GtkWidget *btn = gtk_button_new_with_label(buf);
        gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
        gtk_widget_set_halign(btn, GTK_ALIGN_FILL);
        gtk_widget_set_margin_start(btn, 10);
        gtk_widget_set_margin_end(btn, 10);
        gtk_widget_set_margin_bottom(btn, 2);
        GtkStyleContext *bCtx = gtk_widget_get_style_context(btn);
        gtk_style_context_add_class(bCtx, "cat-btn");
        if (i == 0) gtk_style_context_add_class(bCtx, "active");
        g_signal_connect(btn, "clicked", G_CALLBACK(onCatClick), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(mgrSidebar), btn, FALSE, FALSE, 0);
    }

    /* bottom spacer — push categories to top */
    GtkWidget *spacer = gtk_label_new(NULL);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(mgrSidebar), spacer, TRUE, TRUE, 0);

    /* version label */
    GtkWidget *verLabel = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(verLabel),
        "<span font='9' foreground='#555568'>v1.0</span>");
    gtk_widget_set_margin_bottom(verLabel, 12);
    gtk_box_pack_start(GTK_BOX(mgrSidebar), verLabel, FALSE, FALSE, 0);

    /* ── Content area ── */
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkStyleContext *cCtx = gtk_widget_get_style_context(content);
    gtk_style_context_add_class(cCtx, "manager-content");
    gtk_box_pack_start(GTK_BOX(hbox), content, TRUE, TRUE, 0);

    /* search bar area */
    GtkWidget *searchBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_top(searchBox, 16);
    gtk_widget_set_margin_start(searchBox, 20);
    gtk_widget_set_margin_end(searchBox, 20);
    gtk_widget_set_margin_bottom(searchBox, 8);
    gtk_box_pack_start(GTK_BOX(content), searchBox, FALSE, FALSE, 0);

    mgrSearch = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(mgrSearch), "Search clips...");
    gtk_widget_set_hexpand(mgrSearch, TRUE);
    GtkStyleContext *sCtx = gtk_widget_get_style_context(mgrSearch);
    gtk_style_context_add_class(sCtx, "search-entry");
    g_signal_connect(mgrSearch, "search-changed", G_CALLBACK(onSearchChanged), NULL);
    gtk_box_pack_start(GTK_BOX(searchBox), mgrSearch, TRUE, TRUE, 0);

    /* scroll + grid */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);

    mgrGrid = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(mgrGrid), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(mgrGrid), TRUE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(mgrGrid), 10);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(mgrGrid), 12);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(mgrGrid), 12);
    GtkStyleContext *gCtx = gtk_widget_get_style_context(mgrGrid);
    gtk_style_context_add_class(gCtx, "clips-grid");
    gtk_container_add(GTK_CONTAINER(scroll), mgrGrid);

    /* empty placeholder */
    mgrEmpty = gtk_label_new("No clips yet");
    GtkStyleContext *eCtx = gtk_widget_get_style_context(mgrEmpty);
    gtk_style_context_add_class(eCtx, "empty-label");
    gtk_widget_set_no_show_all(mgrEmpty, TRUE);
    gtk_box_pack_start(GTK_BOX(content), mgrEmpty, TRUE, FALSE, 0);

    /* signals */
    g_signal_connect(mgrWin, "delete-event", G_CALLBACK(onWinDelete), NULL);
    g_signal_connect(mgrWin, "key-press-event", G_CALLBACK(onWinKey), NULL);
    g_signal_connect(mgrWin, "size-allocate", G_CALLBACK(onResize), NULL);

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
    /* hide empty label by default, show when no clips */
    gtk_widget_hide(mgrEmpty);
}

void addClipCard(int id, const char *text, int textLen,
                 int isImage, const char *imagePath, int isFav)
{
    /* ── card container (wrapped in event-box for click) ── */
    GtkWidget *cardEvt = gtk_event_box_new();
    GtkWidget *card    = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(cardEvt, 200, 130);
    gtk_container_add(GTK_CONTAINER(cardEvt), card);
    GtkStyleContext *cCtx = gtk_widget_get_style_context(card);
    gtk_style_context_add_class(cCtx, "clip-card");

    g_object_set_data(G_OBJECT(card), "clip-id", GINT_TO_POINTER(id));
    g_object_set_data(G_OBJECT(card), "is-fav",  GINT_TO_POINTER(isFav));

    /* ── content preview ── */
    GtkWidget *preview = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(preview, TRUE);
    gtk_box_pack_start(GTK_BOX(card), preview, TRUE, TRUE, 0);

    if (isImage && imagePath) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(
            imagePath, 170, 72, TRUE, NULL);
        if (pb) {
            GtkWidget *img = gtk_image_new_from_pixbuf(pb);
            g_object_unref(pb);
            gtk_widget_set_halign(img, GTK_ALIGN_CENTER);
            gtk_widget_set_valign(img, GTK_ALIGN_CENTER);
            gtk_box_pack_start(GTK_BOX(preview), img, TRUE, TRUE, 0);
        } else {
            GtkWidget *ph = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(ph),
                "<span foreground='#aaaaaa' size='large'>[image]</span>");
            gtk_widget_set_halign(ph, GTK_ALIGN_CENTER);
            gtk_box_pack_start(GTK_BOX(preview), ph, TRUE, TRUE, 0);
        }
    } else if (text && textLen > 0) {
        int len = textLen < 300 ? textLen : 300;
        char *dup = g_strndup(text, len);
        GtkWidget *label = gtk_label_new(dup);
        g_free(dup);
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_label_set_yalign(GTK_LABEL(label), 0);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_lines(GTK_LABEL(label), 5);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_widget_set_margin_start(label, 12);
        gtk_widget_set_margin_end(label, 12);
        gtk_widget_set_margin_top(label, 10);
        GtkStyleContext *lCtx = gtk_widget_get_style_context(label);
        gtk_style_context_add_class(lCtx, "card-text");
        gtk_box_pack_start(GTK_BOX(preview), label, FALSE, FALSE, 0);
    }

    /* ── bottom bar: ♥ spacer × ── */
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(card), bar, FALSE, FALSE, 0);

    /* heart (event-box for click) */
    GtkWidget *heartEvt = gtk_event_box_new();
    GtkWidget *heart = gtk_label_new(NULL);
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
    gtk_widget_set_margin_start(heartEvt, 10);
    gtk_widget_set_margin_bottom(heartEvt, 8);
    g_signal_connect(heartEvt, "button-press-event",
        G_CALLBACK(onHeartEvt), GINT_TO_POINTER(id));
    gtk_box_pack_start(GTK_BOX(bar), heartEvt, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(card), "heart-widget", heart);

    /* spacer */
    GtkWidget *sp = gtk_label_new(NULL);
    gtk_widget_set_hexpand(sp, TRUE);
    gtk_box_pack_start(GTK_BOX(bar), sp, TRUE, TRUE, 0);

    /* delete button */
    GtkWidget *delBtn = gtk_button_new_with_label("\xc3\x97"); /* × */
    gtk_button_set_relief(GTK_BUTTON(delBtn), GTK_RELIEF_NONE);
    GtkStyleContext *dCtx = gtk_widget_get_style_context(delBtn);
    gtk_style_context_add_class(dCtx, "card-del");
    gtk_widget_set_margin_end(delBtn, 8);
    gtk_widget_set_margin_bottom(delBtn, 4);
    g_signal_connect(delBtn, "clicked",
        G_CALLBACK(onDelClick), GINT_TO_POINTER(id));
    gtk_box_pack_start(GTK_BOX(bar), delBtn, FALSE, FALSE, 0);

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

const char *getManagerSearchText(void) {
    return gtk_entry_get_text(GTK_ENTRY(mgrSearch));
}
