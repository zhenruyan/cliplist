/*
 * manager.c — Clipboard manager window (GTK3)
 *
 * Card grid with right-click copy, double-click detail, tag/source sidebar.
 * Cards are GtkButton widgets (proper windowed widgets with full CSS support).
 */

#include "manager.h"
#include <string.h>
#include <stdio.h>

/* ── CSS ──────────────────────────────────────────────────────── */

static const char *MANAGER_CSS =
"/* ── sidebar ─────────────────────────── */\n"
".sidebar { background: #e8e8ec; }\n"
".cat-btn {\n"
"  background: transparent; color: #555;\n"
"  border: none; border-radius: 8px;\n"
"  padding: 8px 16px; font-size: 12px;\n"
"}\n"
".cat-btn:hover { background: rgba(0,0,0,0.04); color: #333; }\n"
".cat-btn.active { background: rgba(0,0,0,0.07); color: #222; font-weight: bold; }\n"
".sidebar-sep { background: rgba(0,0,0,0.06); min-height: 1px; margin: 8px 0; }\n"
".section-label { font-size: 9px; color: #888; font-weight: bold; letter-spacing: 0.5px; }\n"
"/* ── content ─────────────────────────── */\n"
".manager-content { background: #f0f0f3; }\n"
".search-entry {\n"
"  background: #ffffff; border: 1px solid #dddee2;\n"
"  border-radius: 8px; padding: 6px 12px; font-size: 12px;\n"
"  color: #333;\n"
"}\n"
".search-entry:focus { border-color: #aaaacc; }\n"
"/* ── grid ────────────────────────────── */\n"
".clips-grid { padding: 12px; background: #f0f0f3; }\n"
"/* ── card (GtkButton) ────────────────── */\n"
".clip-card {\n"
"  background: #ffffff;\n"
"  background-image: none;\n"
"  border: 1px solid #e0e0e6;\n"
"  border-radius: 10px;\n"
"  padding: 6px;\n"
"  box-shadow: none;\n"
"  outline: none;\n"
"  min-width: 240px;\n"
"  min-height: 260px;\n"
"}\n"
".clip-card:hover {\n"
"  background: #ffffff;\n"
"  background-image: none;\n"
"  border-color: #c0c0cc;\n"
"}\n"
".clip-card:active {\n"
"  background: #ffffff;\n"
"  background-image: none;\n"
"  border-color: #e0e0e6;\n"
"}\n"
".clip-card.copied {\n"
"  border-color: #6ab070;\n"
"  background: #f4faf4;\n"
"  background-image: none;\n"
"}\n"
".card-text { font-size: 12px; color: #333333; }\n"
".card-info { font-size: 11px; color: #999; }\n"
".card-sep { background: #eaeae8; min-height: 1px; margin: 2px 0; }\n"
"/* ── tags ────────────────────────────── */\n"
".tag-pill {\n"
"  color: #ffffff; border-radius: 6px;\n"
"  padding: 1px 7px; font-size: 10px;\n"
"}\n"
".tag-blue   { background: #5b8abf; }\n"
".tag-green  { background: #5aab6e; }\n"
".tag-orange { background: #d4994a; }\n"
".tag-red    { background: #c75050; }\n"
".tag-purple { background: #8e6bb5; }\n"
".tag-teal   { background: #3ea59a; }\n"
".tag-pink   { background: #cf5a88; }\n"
".tag-indigo { background: #5c6ab5; }\n"
".add-tag {\n"
"  color: #aaaacc; font-size: 14px; font-weight: bold;\n"
"}\n"
".add-tag:hover { color: #7777aa; }\n"
"/* ── action bar ──────────────────────── */\n"
".card-action { opacity: 0.25; }\n"
".clip-card:hover .card-action { opacity: 1.0; }\n"
".card-heart {\n"
"  font-size: 18px; padding: 2px 4px;\n"
"}\n"
".card-heart.fav { color: #e05565; }\n"
".card-heart:not(.fav) { color: #d0d0d0; }\n"
".card-del {\n"
"  background: transparent; border: none;\n"
"  color: transparent; font-size: 14px;\n"
"  padding: 2px 6px; border-radius: 4px;\n"
"  min-width: 24px; min-height: 24px;\n"
"}\n"
".clip-card:hover .card-del { color: #c0c0c0; }\n"
".card-del:hover { color: #e05565; background: rgba(224,85,101,0.06); }\n"
"/* ── detail dialog ───────────────────── */\n"
".detail-win { background: #ffffff; }\n"
".detail-header { padding: 12px 16px; }\n"
".detail-header-label { font-size: 11px; color: #888; }\n"
".detail-text { font-size: 12px; color: #333; }\n"
".detail-bar {\n"
"  padding: 8px 16px; background: #f4f4f6;\n"
"  border-top: 1px solid #e4e4e8;\n"
"}\n"
".detail-btn {\n"
"  background: #5c6ab5; color: #ffffff; border: none;\n"
"  border-radius: 6px; padding: 6px 18px; font-size: 12px;\n"
"}\n"
".detail-btn:hover { background: #4e5aa5; }\n"
".detail-close {\n"
"  background: transparent; color: #888; border: 1px solid #ddd;\n"
"  border-radius: 6px; padding: 6px 18px; font-size: 12px;\n"
"}\n"
".detail-close:hover { background: #f0f0f0; }\n"
"/* ── empty ───────────────────────────── */\n"
".empty-label { color: #aaaaaa; font-size: 13px; }\n";

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

static GtkWidget *mgrWin          = NULL;
static GtkWidget *mgrSidebar      = NULL;
static GtkWidget *tagSidebarBox   = NULL;
static GtkWidget *sourceSidebarBox = NULL;
static GtkWidget *mgrSearch       = NULL;
static GtkWidget *mgrGrid         = NULL;
static GtkWidget *mgrEmpty        = NULL;
static GtkWidget *activeBtn       = NULL;

/* ── Forward declarations ─────────────────────────────────────── */

static void showDetailDialog(GtkWidget *card);

/* ── Sidebar helpers ──────────────────────────────────────────── */

static void setActiveButton(GtkButton *btn) {
    if (activeBtn)
        gtk_style_context_remove_class(
            gtk_widget_get_style_context(activeBtn), "active");
    activeBtn = GTK_WIDGET(btn);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(activeBtn), "active");
}

static void onCatClick(GtkButton *btn, gpointer data) {
    setActiveButton(btn);
    goCategoryChanged(GPOINTER_TO_INT(data));
}

static void onTagGroupClick(GtkButton *btn, gpointer data) {
    (void)data;
    setActiveButton(btn);
    const char *tag = g_object_get_data(G_OBJECT(btn), "tag-name");
    goTagGroupChanged((char *)tag);
}

static void onSourceGroupClick(GtkButton *btn, gpointer data) {
    (void)data;
    setActiveButton(btn);
    const char *src = g_object_get_data(G_OBJECT(btn), "source-name");
    goSourceGroupChanged((char *)src);
}

/* ── Window callbacks ─────────────────────────────────────────── */

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
    gtk_style_context_remove_class(
        gtk_widget_get_style_context(card), "copied");
    return G_SOURCE_REMOVE;
}

/* ── Detail dialog (double-click) ─────────────────────────────── */

static void onDetailCopy(GtkButton *btn, gpointer data) {
    (void)btn;
    goCardClicked(GPOINTER_TO_INT(data));
}

static gboolean onDetailKey(GtkWidget *w, GdkEventKey *ev, gpointer d) {
    (void)d;
    if (ev->keyval == GDK_KEY_Escape) { gtk_widget_destroy(w); return TRUE; }
    return FALSE;
}

static void showDetailDialog(GtkWidget *card) {
    int id       = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(card), "clip-id"));
    int isImage  = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(card), "clip-is-image"));
    const char *text      = g_object_get_data(G_OBJECT(card), "clip-text");
    const char *imagePath = g_object_get_data(G_OBJECT(card), "clip-image-path");
    const char *tags      = g_object_get_data(G_OBJECT(card), "clip-tags");
    const char *sourceApp = g_object_get_data(G_OBJECT(card), "clip-source");
    const char *timeStr   = g_object_get_data(G_OBJECT(card), "clip-time");

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Clip Detail");
    gtk_window_set_default_size(GTK_WINDOW(win), 560, 400);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(mgrWin));
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "detail-win");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    /* ── header: id + source + time + tags ── */
    GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(hdr), "detail-header");
    gtk_widget_set_margin_top(hdr, 6);
    gtk_box_pack_start(GTK_BOX(vbox), hdr, FALSE, FALSE, 0);

    char hdrText[256];
    snprintf(hdrText, sizeof(hdrText), "#%d", id);
    if (sourceApp && sourceApp[0]) {
        strncat(hdrText, "  \xc2\xb7  ", sizeof(hdrText) - strlen(hdrText) - 1);
        strncat(hdrText, sourceApp, sizeof(hdrText) - strlen(hdrText) - 1);
    }
    if (timeStr && timeStr[0]) {
        strncat(hdrText, "  \xc2\xb7  ", sizeof(hdrText) - strlen(hdrText) - 1);
        strncat(hdrText, timeStr, sizeof(hdrText) - strlen(hdrText) - 1);
    }
    if (tags && tags[0]) {
        strncat(hdrText, "  \xc2\xb7  ", sizeof(hdrText) - strlen(hdrText) - 1);
        strncat(hdrText, tags, sizeof(hdrText) - strlen(hdrText) - 1);
    }
    GtkWidget *hdrLabel = gtk_label_new(hdrText);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(hdrLabel), "detail-header-label");
    gtk_widget_set_halign(hdrLabel, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(hdr), hdrLabel, FALSE, FALSE, 0);

    /* separator */
    GtkWidget *sepLine = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), sepLine, FALSE, FALSE, 0);

    /* ── content ── */
    if (isImage && imagePath) {
        GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_vexpand(scroll, TRUE);
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(
            imagePath, 520, -1, TRUE, NULL);
        if (pb) {
            GtkWidget *img = gtk_image_new_from_pixbuf(pb);
            g_object_unref(pb);
            gtk_widget_set_halign(img, GTK_ALIGN_CENTER);
            gtk_widget_set_valign(img, GTK_ALIGN_CENTER);
            gtk_container_add(GTK_CONTAINER(scroll), img);
        }
        gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    } else if (text) {
        GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_vexpand(scroll, TRUE);

        GtkWidget *tv = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tv), FALSE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD_CHAR);
        gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv), 16);
        gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tv), 16);
        gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv), 12);
        gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(tv), 12);
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
        gtk_text_buffer_set_text(buf, text, -1);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(tv), "detail-text");
        gtk_container_add(GTK_CONTAINER(scroll), tv);
        gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    }

    /* ── bottom bar ── */
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(bar), "detail-bar");
    gtk_box_pack_start(GTK_BOX(vbox), bar, FALSE, FALSE, 0);

    GtkWidget *sp = gtk_label_new(NULL);
    gtk_widget_set_hexpand(sp, TRUE);
    gtk_box_pack_start(GTK_BOX(bar), sp, TRUE, TRUE, 0);

    GtkWidget *closeBtn = gtk_button_new_with_label("Close");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(closeBtn), "detail-close");
    g_signal_connect_swapped(closeBtn, "clicked",
        G_CALLBACK(gtk_widget_destroy), win);
    gtk_box_pack_start(GTK_BOX(bar), closeBtn, FALSE, FALSE, 0);

    if (!isImage && text) {
        GtkWidget *copyBtn = gtk_button_new_with_label("  Copy  ");
        gtk_style_context_add_class(
            gtk_widget_get_style_context(copyBtn), "detail-btn");
        g_signal_connect(copyBtn, "clicked",
            G_CALLBACK(onDetailCopy), GINT_TO_POINTER(id));
        gtk_box_pack_start(GTK_BOX(bar), copyBtn, FALSE, FALSE, 0);
    }

    g_signal_connect(win, "key-press-event", G_CALLBACK(onDetailKey), NULL);
    gtk_widget_show_all(win);
}

/* ── Card event: right-click copy, double-click detail ────────── */

static gboolean onCardEvt(GtkWidget *card, GdkEventButton *ev, gpointer data) {
    (void)data;

    /* double-click → detail */
    if (ev->type == GDK_2BUTTON_PRESS && ev->button == 1) {
        showDetailDialog(card);
        return TRUE;
    }

    /* right-click → copy */
    if (ev->button == 3) {
        int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(card), "clip-id"));
        goCardClicked(id);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(card), "copied");
        g_timeout_add(600, removeCopiedClass, card);
        return TRUE;
    }

    /* left single-click → consume */
    return TRUE;
}

/* ── Heart / delete ───────────────────────────────────────────── */

static gboolean onHeartEvt(GtkWidget *evtBox, GdkEventButton *ev, gpointer data) {
    (void)evtBox; (void)ev;
    GtkWidget *card = GTK_WIDGET(data);
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(card), "clip-id"));
    GtkWidget *heart = g_object_get_data(G_OBJECT(card), "heart-widget");
    int isFav = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(card), "is-fav"));

    isFav = !isFav;
    g_object_set_data(G_OBJECT(card), "is-fav", GINT_TO_POINTER(isFav));
    GtkStyleContext *ctx = gtk_widget_get_style_context(heart);
    if (isFav) {
        gtk_label_set_markup(GTK_LABEL(heart),
            "<span foreground='#e05565'>\xe2\x99\xa5</span>");
        gtk_style_context_add_class(ctx, "fav");
    } else {
        gtk_label_set_markup(GTK_LABEL(heart),
            "<span foreground='#d0d0d0'>\xe2\x99\xa1</span>");
        gtk_style_context_remove_class(ctx, "fav");
    }
    goCardFavorited(id);
    return TRUE;
}

static void onDelClick(GtkButton *btn, gpointer data) {
    (void)btn;
    GtkWidget *card = GTK_WIDGET(data);
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(card), "clip-id"));
    gtk_widget_destroy(card);
    goCardDeleted(id);
}

/* ── Tag add / remove ─────────────────────────────────────────── */

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
    gtk_window_set_default_size(GTK_WINDOW(dialog), 240, -1);

    GtkWidget *ca = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_top(ca, 10);
    gtk_widget_set_margin_bottom(ca, 10);
    gtk_widget_set_margin_start(ca, 10);
    gtk_widget_set_margin_end(ca, 10);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Tag name...");
    gtk_container_add(GTK_CONTAINER(ca), entry);
    gtk_widget_show_all(dialog);
    gtk_widget_grab_focus(entry);

    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_OK) {
        const char *tag = gtk_entry_get_text(GTK_ENTRY(entry));
        if (tag && strlen(tag) > 0)
            goTagAdded(id, (char *)tag);
    }
    gtk_widget_destroy(dialog);
    return TRUE;
}

/* ── Build sidebar section ────────────────────────────────────── */

static GtkWidget *addSidebarSection(GtkWidget *box, const char *title) {
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(sep), "sidebar-sep");
    gtk_widget_set_margin_top(sep, 10);
    gtk_widget_set_margin_bottom(sep, 6);
    gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 0);

    GtkWidget *lbl = gtk_label_new(NULL);
    char markup[128];
    snprintf(markup, sizeof(markup),
        "<span font='9' foreground='#888' letter_spacing='512'>%s</span>", title);
    gtk_label_set_markup(GTK_LABEL(lbl), markup);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_widget_set_margin_bottom(lbl, 6);
    gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

    GtkWidget *container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(box), container, FALSE, FALSE, 0);
    return container;
}

/* ── Build window ─────────────────────────────────────────────── */

void initManagerWindow(void) {
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
    gtk_window_set_default_size(GTK_WINDOW(mgrWin), 860, 520);
    gtk_window_set_position(GTK_WINDOW(mgrWin), GTK_WIN_POS_CENTER);
    gtk_window_set_type_hint(GTK_WINDOW(mgrWin), GDK_WINDOW_TYPE_HINT_DIALOG);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(mgrWin), hbox);

    /* ── Sidebar ── */
    mgrSidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(mgrSidebar, 170, -1);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(mgrSidebar), "sidebar");
    gtk_widget_set_margin_top(mgrSidebar, 22);
    gtk_widget_set_margin_start(mgrSidebar, 20);
    gtk_widget_set_margin_end(mgrSidebar, 12);
    gtk_box_pack_start(GTK_BOX(hbox), mgrSidebar, FALSE, FALSE, 0);

    /* title */
    GtkWidget *titleLabel = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(titleLabel),
        "<span font='15' weight='bold' foreground='#333'>Cliplist</span>");
    gtk_widget_set_halign(titleLabel, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(mgrSidebar), titleLabel, FALSE, FALSE, 0);

    GtkWidget *subLabel = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(subLabel),
        "<span font='9' foreground='#888'>Clipboard History</span>");
    gtk_widget_set_halign(subLabel, GTK_ALIGN_START);
    gtk_widget_set_margin_top(subLabel, 2);
    gtk_widget_set_margin_bottom(subLabel, 18);
    gtk_box_pack_start(GTK_BOX(mgrSidebar), subLabel, FALSE, FALSE, 0);

    /* category buttons */
    static const struct { int id; const char *label; } cats[] = {
        { 0, "  All Items" },
        { 1, "  Text"       },
        { 2, "  Images"     },
        { 3, "  Favorites"  },
    };
    for (int i = 0; i < 4; i++) {
        GtkWidget *btn = gtk_button_new_with_label(cats[i].label);
        gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
        gtk_widget_set_halign(btn, GTK_ALIGN_FILL);
        gtk_widget_set_margin_bottom(btn, 1);
        GtkStyleContext *bCtx = gtk_widget_get_style_context(btn);
        gtk_style_context_add_class(bCtx, "cat-btn");
        if (i == 0) {
            gtk_style_context_add_class(bCtx, "active");
            activeBtn = btn;
        }
        g_signal_connect(btn, "clicked",
            G_CALLBACK(onCatClick), GINT_TO_POINTER(cats[i].id));
        gtk_box_pack_start(GTK_BOX(mgrSidebar), btn, FALSE, FALSE, 0);
    }

    /* SOURCES section */
    sourceSidebarBox = addSidebarSection(mgrSidebar, "SOURCES");

    /* TAGS section */
    tagSidebarBox = addSidebarSection(mgrSidebar, "TAGS");

    /* spacer */
    GtkWidget *spacer = gtk_label_new(NULL);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(mgrSidebar), spacer, TRUE, TRUE, 0);

    /* ── Content ── */
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(content), "manager-content");
    gtk_box_pack_start(GTK_BOX(hbox), content, TRUE, TRUE, 0);

    /* search bar */
    GtkWidget *searchBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_top(searchBox, 12);
    gtk_widget_set_margin_start(searchBox, 16);
    gtk_widget_set_margin_end(searchBox, 16);
    gtk_widget_set_margin_bottom(searchBox, 8);
    gtk_box_pack_start(GTK_BOX(content), searchBox, FALSE, FALSE, 0);

    mgrSearch = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(mgrSearch), "Search clips...");
    gtk_widget_set_hexpand(mgrSearch, TRUE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(mgrSearch), "search-entry");
    g_signal_connect(mgrSearch, "search-changed",
        G_CALLBACK(onSearchChanged), NULL);
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
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(mgrGrid), 10);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(mgrGrid), 10);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(mgrGrid), "clips-grid");
    gtk_container_add(GTK_CONTAINER(scroll), mgrGrid);

    /* empty placeholder */
    mgrEmpty = gtk_label_new("No clips yet");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(mgrEmpty), "empty-label");
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

void showManagerWindow(void) {
    gtk_widget_show_all(mgrWin);
    gtk_window_present(GTK_WINDOW(mgrWin));
}
void hideManagerWindow(void) {
    gtk_widget_hide(mgrWin);
}
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

/* ── Tag sidebar ──────────────────────────────────────────────── */

void clearTagSidebar(void) {
    GList *ch = gtk_container_get_children(GTK_CONTAINER(tagSidebarBox));
    for (GList *l = ch; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(ch);
}

void addTagButton(const char *tag) {
    char buf[64];
    snprintf(buf, sizeof(buf), "  # %s", tag);
    GtkWidget *btn = gtk_button_new_with_label(buf);
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
    gtk_widget_set_halign(btn, GTK_ALIGN_FILL);
    gtk_widget_set_margin_bottom(btn, 1);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(btn), "cat-btn");
    g_object_set_data_full(G_OBJECT(btn), "tag-name", g_strdup(tag), g_free);
    g_signal_connect(btn, "clicked", G_CALLBACK(onTagGroupClick), NULL);
    gtk_box_pack_start(GTK_BOX(tagSidebarBox), btn, FALSE, FALSE, 0);
    gtk_widget_show_all(btn);
}

/* ── Source sidebar ───────────────────────────────────────────── */

void clearSourceSidebar(void) {
    GList *ch = gtk_container_get_children(GTK_CONTAINER(sourceSidebarBox));
    for (GList *l = ch; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(ch);
}

void addSourceButton(const char *source) {
    char buf[64];
    snprintf(buf, sizeof(buf), "  %s", source);
    GtkWidget *btn = gtk_button_new_with_label(buf);
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
    gtk_widget_set_halign(btn, GTK_ALIGN_FILL);
    gtk_widget_set_margin_bottom(btn, 1);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(btn), "cat-btn");
    g_object_set_data_full(G_OBJECT(btn), "source-name",
        g_strdup(source), g_free);
    g_signal_connect(btn, "clicked", G_CALLBACK(onSourceGroupClick), NULL);
    gtk_box_pack_start(GTK_BOX(sourceSidebarBox), btn, FALSE, FALSE, 0);
    gtk_widget_show_all(btn);
}

/* ── Tag pill on a card ───────────────────────────────────────── */

static void addTagPill(GtkWidget *tagBar, int clipId, const char *tag) {
    GtkWidget *evtBox = gtk_event_box_new();
    GtkWidget *label  = gtk_label_new(tag);
    GtkStyleContext *ctx = gtk_widget_get_style_context(label);
    gtk_style_context_add_class(ctx, "tag-pill");
    gtk_style_context_add_class(ctx, tagColorClass(tag));
    gtk_container_add(GTK_CONTAINER(evtBox), label);
    g_object_set_data(G_OBJECT(evtBox), "clip-id", GINT_TO_POINTER(clipId));
    g_object_set_data_full(G_OBJECT(evtBox), "tag-name",
        g_strdup(tag), g_free);
    g_signal_connect(evtBox, "button-press-event",
        G_CALLBACK(onTagRemove), NULL);
    gtk_box_pack_start(GTK_BOX(tagBar), evtBox, FALSE, FALSE, 0);
    gtk_widget_show_all(evtBox);
}

/* ── Add a clip card ──────────────────────────────────────────── */

void addClipCard(int id, const char *text, int textLen,
                 int isImage, const char *imagePath, int isFav,
                 const char *tags, const char *sourceApp,
                 const char *timeStr)
{
    /*
     * card = GtkButton (.clip-card, 240×260)
     *   └── inner = GtkBox (vertical, spacing 5)
     *         ├── preview (text or image)
     *         ├── tag bar (pills + [+] button)
     *         ├── separator
     *         ├── info line (source · time)
     *         └── action bar (♥ ... delete)
     */

    GtkWidget *card = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(card), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(card, FALSE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(card), "clip-card");
    gtk_widget_set_size_request(card, 240, 260);

    GtkWidget *inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(card), inner);

    /* store all data on the button */
    g_object_set_data(G_OBJECT(card), "clip-id",
        GINT_TO_POINTER(id));
    g_object_set_data(G_OBJECT(card), "clip-is-image",
        GINT_TO_POINTER(isImage));
    g_object_set_data(G_OBJECT(card), "is-fav",
        GINT_TO_POINTER(isFav));

    if (text && textLen > 0)
        g_object_set_data_full(G_OBJECT(card), "clip-text",
            g_strndup(text, textLen), g_free);

    if (imagePath && imagePath[0])
        g_object_set_data_full(G_OBJECT(card), "clip-image-path",
            g_strdup(imagePath), g_free);

    if (tags && tags[0])
        g_object_set_data_full(G_OBJECT(card), "clip-tags",
            g_strdup(tags), g_free);

    if (sourceApp && sourceApp[0])
        g_object_set_data_full(G_OBJECT(card), "clip-source",
            g_strdup(sourceApp), g_free);

    if (timeStr && timeStr[0])
        g_object_set_data_full(G_OBJECT(card), "clip-time",
            g_strdup(timeStr), g_free);

    /* ── preview ── */
    if (isImage && imagePath) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(
            imagePath, 220, 100, TRUE, NULL);
        if (pb) {
            GtkWidget *img = gtk_image_new_from_pixbuf(pb);
            g_object_unref(pb);
            gtk_widget_set_halign(img, GTK_ALIGN_CENTER);
            gtk_box_pack_start(GTK_BOX(inner), img, FALSE, FALSE, 0);
        } else {
            GtkWidget *ph = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(ph),
                "<span foreground='#aaaaaa' font='10'>[image]</span>");
            gtk_widget_set_halign(ph, GTK_ALIGN_CENTER);
            gtk_box_pack_start(GTK_BOX(inner), ph, FALSE, FALSE, 2);
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
        gtk_label_set_max_width_chars(GTK_LABEL(label), 28);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(label), "card-text");
        gtk_box_pack_start(GTK_BOX(inner), label, FALSE, FALSE, 0);
    }

    /* ── tag bar ── */
    GtkWidget *tagBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_box_pack_start(GTK_BOX(inner), tagBar, FALSE, FALSE, 0);

    if (tags && tags[0]) {
        char **list = g_strsplit(tags, ",", -1);
        for (int i = 0; list[i]; i++)
            if (list[i][0])
                addTagPill(tagBar, id, list[i]);
        g_strfreev(list);
    }

    /* [+] add tag */
    GtkWidget *addEvt   = gtk_event_box_new();
    GtkWidget *addLabel = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(addLabel),
        "<span font='14' weight='bold'>+</span>");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(addLabel), "add-tag");
    gtk_container_add(GTK_CONTAINER(addEvt), addLabel);
    g_signal_connect(addEvt, "button-press-event",
        G_CALLBACK(onAddTag), GINT_TO_POINTER(id));
    gtk_box_pack_start(GTK_BOX(tagBar), addEvt, FALSE, FALSE, 2);

    /* ── separator ── */
    GtkWidget *cardSep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(cardSep), "card-sep");
    gtk_box_pack_start(GTK_BOX(inner), cardSep, FALSE, FALSE, 0);

    /* ── info line: source · time ── */
    char infoBuf[128];
    infoBuf[0] = '\0';
    if (sourceApp && sourceApp[0] && timeStr && timeStr[0]) {
        snprintf(infoBuf, sizeof(infoBuf), "%s  \xc2\xb7  %s",
            sourceApp, timeStr);
    } else if (sourceApp && sourceApp[0]) {
        snprintf(infoBuf, sizeof(infoBuf), "%s", sourceApp);
    } else if (timeStr && timeStr[0]) {
        snprintf(infoBuf, sizeof(infoBuf), "%s", timeStr);
    }
    if (infoBuf[0]) {
        GtkWidget *infoLabel = gtk_label_new(infoBuf);
        gtk_label_set_xalign(GTK_LABEL(infoLabel), 0);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(infoLabel), "card-info");
        gtk_box_pack_start(GTK_BOX(inner), infoLabel, FALSE, FALSE, 0);
    }

    /* ── action bar ── */
    GtkWidget *actionBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(actionBar), "card-action");
    gtk_box_pack_start(GTK_BOX(inner), actionBar, FALSE, FALSE, 0);

    /* heart */
    GtkWidget *heartEvt = gtk_event_box_new();
    GtkWidget *heart    = gtk_label_new(NULL);
    GtkStyleContext *hCtx = gtk_widget_get_style_context(heart);
    gtk_style_context_add_class(hCtx, "card-heart");
    if (isFav) {
        gtk_label_set_markup(GTK_LABEL(heart),
            "<span foreground='#e05565'>\xe2\x99\xa5</span>");
        gtk_style_context_add_class(hCtx, "fav");
    } else {
        gtk_label_set_markup(GTK_LABEL(heart),
            "<span foreground='#d0d0d0'>\xe2\x99\xa1</span>");
    }
    gtk_container_add(GTK_CONTAINER(heartEvt), heart);
    gtk_widget_set_margin_start(heartEvt, 2);
    g_signal_connect(heartEvt, "button-press-event",
        G_CALLBACK(onHeartEvt), card);
    gtk_box_pack_start(GTK_BOX(actionBar), heartEvt, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(card), "heart-widget", heart);

    /* spacer */
    GtkWidget *sp = gtk_label_new(NULL);
    gtk_widget_set_hexpand(sp, TRUE);
    gtk_box_pack_start(GTK_BOX(actionBar), sp, TRUE, TRUE, 0);

    /* delete */
    GtkWidget *delBtn = gtk_button_new_with_label("\xc3\x97");
    gtk_button_set_relief(GTK_BUTTON(delBtn), GTK_RELIEF_NONE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(delBtn), "card-del");
    g_signal_connect(delBtn, "clicked",
        G_CALLBACK(onDelClick), card);
    gtk_box_pack_start(GTK_BOX(actionBar), delBtn, FALSE, FALSE, 0);

    /* events: right-click → copy, double-click → detail */
    g_signal_connect(card, "button-press-event",
        G_CALLBACK(onCardEvt), NULL);

    gtk_flow_box_insert(GTK_FLOW_BOX(mgrGrid), card, -1);
    gtk_widget_show_all(card);
}

/* ── Idle callbacks ────────────────────────────────────────────── */

gboolean rebuildGridIdle(gpointer data) {
    (void)data;
    goRebuildGrid();
    return G_SOURCE_REMOVE;
}

gboolean rebuildTagSidebarIdle(gpointer data) {
    (void)data;
    goRebuildTagSidebar();
    return G_SOURCE_REMOVE;
}

gboolean rebuildSourceSidebarIdle(gpointer data) {
    (void)data;
    goRebuildSourceSidebar();
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
