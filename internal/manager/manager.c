/*
 * manager.c — Clipboard manager window (GTK3) - Compact List View
 */

#include "manager.h"
#include <string.h>
#include <stdio.h>

/* ── Globals ──────────────────────────────────────────────────── */

static GtkWidget *mgrWin          = NULL;
static GtkWidget *mgrSidebar      = NULL;
static GtkWidget *tagSidebarBox   = NULL;
static GtkWidget *sourceSidebarBox = NULL;
static GtkWidget *mgrSearch       = NULL;
static GtkWidget *mgrGrid         = NULL; // Now a GtkListBox
static GtkWidget *mgrEmpty        = NULL;
static GtkWidget *activeBtn       = NULL;

/* ── CSS ──────────────────────────────────────────────────────── */

static const char *MANAGER_CSS =
".sidebar { background: #f8f9fa; border-right: 1px solid #e9ecef; }\n"
".sidebar-fixed { padding: 20px 12px 10px 12px; }\n"
".cat-btn {\n"
"  background: transparent; color: #495057; border: none; border-radius: 6px;\n"
"  padding: 8px 12px; font-size: 13px; margin: 1px 0; text-align: left;\n"
"}\n"
".cat-btn:hover { background: #e9ecef; }\n"
".cat-btn.active { background: #dee2e6; color: #000; font-weight: 600; }\n"
".section-label { font-size: 11px; color: #adb5bd; font-weight: 700; padding: 12px 14px 6px 14px; text-transform: uppercase; }\n"
".tree-item { padding-left: 28px; font-size: 12px; }\n"
"\n"
".manager-content { background: #ffffff; }\n"
".search-entry { background: #f1f3f5; border: 1px solid transparent; border-radius: 10px; padding: 8px 16px; margin: 20px 30px; }\n"
".search-entry:focus { background: #ffffff; border-color: #ced4da; }\n"
"\n"
".clips-list { background: transparent; padding: 0 20px; }\n"
".clip-row {\n"
"  background: #ffffff; border-bottom: 1px solid #f8f9fa; padding: 0 16px;\n"
"  min-height: 60px; max-height: 60px; overflow: hidden;\n"
"}\n"
".clip-row:hover { background: #f8f9fa; }\n"
"\n"
".row-content-text { font-size: 13px; color: #343a40; }\n"
".row-info { font-size: 11px; color: #adb5bd; }\n"
"\n"
".tag-pill { border-radius: 4px; border: 1px solid rgba(0,0,0,0.1); margin: 0 2px; padding: 1px 6px; }\n"
".tag-pill label { color: #ffffff; font-size: 10px; font-weight: 600; }\n"
".tag-blue { background: #4dabf7; } .tag-green { background: #51cf66; } .tag-orange { background: #ff922b; }\n"
".tag-red { background: #ff6b6b; } .tag-purple { background: #be4bdb; } .tag-teal { background: #20c997; }\n"
"\n"
".btn-icon { background: transparent; border: none; font-size: 16px; color: #ced4da; padding: 6px; border-radius: 6px; }\n"
".btn-icon:hover { background: #eee; color: #495057; }\n"
".btn-fav.active { color: #ff6b6b; }\n"
".btn-del:hover { color: #fa5252; background: #fff5f5; }\n"
"\n"
".detail-win { background: #ffffff; }\n"
".detail-header { padding: 15px 20px; background: #f8f9fa; border-bottom: 1px solid #e9ecef; font-size: 12px; color: #888; }\n";

/* ── Declarations ────────────────────────────────────────────── */

static void showDetailDialog(GtkWidget *row);
static gboolean onRowEvt(GtkWidget *widget, GdkEventButton *ev, gpointer data);
static void onHeartClick(GtkButton *btn, gpointer row);
static void onDelClick(GtkButton *btn, gpointer row);
static void onAddTagClick(GtkButton *btn, gpointer id);
static gboolean onTagRemove(GtkWidget *eb, GdkEventButton *ev, gpointer d);
static void onCatClick(GtkButton *btn, gpointer id);
static void onTagGroupClick(GtkButton *btn, gpointer d);
static void onSourceGroupClick(GtkButton *btn, gpointer d);

static const char *TAG_CLASSES[] = { "tag-blue", "tag-green", "tag-orange", "tag-red", "tag-purple", "tag-teal" };
static const char* tagColorClass(const char *tag) {
    unsigned h = 5381; for (const char *p = tag; *p; p++) h = h * 33 + (unsigned char)*p;
    return TAG_CLASSES[h % 6];
}

static GtkWidget *addSidebarSection(GtkWidget *box, const char *title) {
    GtkWidget *l = gtk_label_new(title);
    gtk_style_context_add_class(gtk_widget_get_style_context(l), "section-label");
    gtk_widget_set_halign(l, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), l, FALSE, FALSE, 0);
    GtkWidget *c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(box), c, FALSE, FALSE, 0);
    return c;
}

/* ── UI Components ───────────────────────────────────────────── */

static void showDetailDialog(GtkWidget *row) {
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "clip-id"));
    int isImg = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "clip-is-image"));
    const char *text = g_object_get_data(G_OBJECT(row), "clip-text");
    const char *imgPath = g_object_get_data(G_OBJECT(row), "clip-image-path");
    const char *src = g_object_get_data(G_OBJECT(row), "clip-source");

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Clip Detail");
    gtk_window_set_default_size(GTK_WINDOW(win), 600, 450);
    gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(mgrWin));
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    char hdrBuf[256]; snprintf(hdrBuf, 256, "Source: %s", src ? src : "Unknown");
    GtkWidget *hdr = gtk_label_new(hdrBuf);
    gtk_style_context_add_class(gtk_widget_get_style_context(hdr), "detail-header");
    gtk_widget_set_halign(hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), hdr, FALSE, FALSE, 0);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    if (isImg && imgPath) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_file(imgPath, NULL);
        if (pb) { gtk_container_add(GTK_CONTAINER(scroll), gtk_image_new_from_pixbuf(pb)); g_object_unref(pb); }
    } else if (text) {
        GtkWidget *tv = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD_CHAR);
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv)), text, -1);
        gtk_container_add(GTK_CONTAINER(scroll), tv);
    }

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(bar), 12);
    GtkWidget *cb = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(cb, "clicked", G_CALLBACK(gtk_widget_destroy), win);
    gtk_box_pack_start(GTK_BOX(bar), cb, FALSE, FALSE, 0);
    
    if (!isImg && text) {
        GtkWidget *cpy = gtk_button_new_with_label("Copy Content");
        g_signal_connect_swapped(cpy, "clicked", G_CALLBACK(goCardClicked), GINT_TO_POINTER(id));
        gtk_box_pack_end(GTK_BOX(bar), cpy, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(vbox), bar, FALSE, FALSE, 0);
    gtk_widget_show_all(win);
}

/* ── Callbacks ───────────────────────────────────────────────── */

static gboolean onRowEvt(GtkWidget *widget, GdkEventButton *ev, gpointer data) {
    if (ev->type == GDK_2BUTTON_PRESS && ev->button == 1) { showDetailDialog(widget); return TRUE; }
    if (ev->button == 3) { goCardClicked(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "clip-id"))); return TRUE; }
    return FALSE;
}

static void onHeartClick(GtkButton *btn, gpointer row) {
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "clip-id"));
    int isFav = !GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "is-fav"));
    g_object_set_data(G_OBJECT(row), "is-fav", GINT_TO_POINTER(isFav));
    gtk_button_set_label(btn, isFav ? "❤️" : "🤍");
    if (isFav) gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(btn)), "active");
    else gtk_style_context_remove_class(gtk_widget_get_style_context(GTK_WIDGET(btn)), "active");
    goCardFavorited(id);
}

static void onDelClick(GtkButton *btn, gpointer row) {
    goCardDeleted(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "clip-id")));
    gtk_widget_destroy(GTK_WIDGET(row));
}

static void onAddTagClick(GtkButton *btn, gpointer id) {
    GtkWidget *d = gtk_dialog_new_with_buttons("Add Tag", GTK_WINDOW(mgrWin), GTK_DIALOG_MODAL, "_Cancel", GTK_RESPONSE_CANCEL, "_Add", GTK_RESPONSE_OK, NULL);
    GtkWidget *e = gtk_entry_new(); gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(d))), e);
    gtk_widget_show_all(d);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) {
        const char *t = gtk_entry_get_text(GTK_ENTRY(e));
        if (t && t[0]) goTagAdded(GPOINTER_TO_INT(id), (char *)t);
    }
    gtk_widget_destroy(d);
}

static gboolean onTagRemove(GtkWidget *eb, GdkEventButton *ev, gpointer d) {
    if (ev->button != 1) return FALSE;
    goTagRemoved(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eb), "clip-id")), (char *)g_object_get_data(G_OBJECT(eb), "tag-name"));
    gtk_widget_destroy(eb);
    return TRUE;
}

static void onCatClick(GtkButton *btn, gpointer id) {
    if (activeBtn) gtk_style_context_remove_class(gtk_widget_get_style_context(activeBtn), "active");
    activeBtn = GTK_WIDGET(btn); gtk_style_context_add_class(gtk_widget_get_style_context(activeBtn), "active");
    goCategoryChanged(GPOINTER_TO_INT(id));
}

static void onTagGroupClick(GtkButton *btn, gpointer d) {
    if (activeBtn) gtk_style_context_remove_class(gtk_widget_get_style_context(activeBtn), "active");
    activeBtn = GTK_WIDGET(btn); gtk_style_context_add_class(gtk_widget_get_style_context(activeBtn), "active");
    goTagGroupChanged((char *)g_object_get_data(G_OBJECT(btn), "tag-name"));
}

static void onSourceGroupClick(GtkButton *btn, gpointer d) {
    if (activeBtn) gtk_style_context_remove_class(gtk_widget_get_style_context(activeBtn), "active");
    activeBtn = GTK_WIDGET(btn); gtk_style_context_add_class(gtk_widget_get_style_context(activeBtn), "active");
    goSourceGroupChanged((char *)g_object_get_data(G_OBJECT(btn), "source-name"));
}

/* ── Main Builders ───────────────────────────────────────────── */

void initManagerWindow(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, MANAGER_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    mgrWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(mgrWin), 1100, 700);
    
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(mgrWin), hbox);

    GtkWidget *side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(side, 210, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(side), "sidebar");
    gtk_box_pack_start(GTK_BOX(hbox), side, FALSE, FALSE, 0);

    GtkWidget *fBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(fBox), "sidebar-fixed");
    gtk_box_pack_start(GTK_BOX(fBox), gtk_label_new("Cliplist History"), FALSE, FALSE, 15);
    gtk_box_pack_start(GTK_BOX(side), fBox, FALSE, FALSE, 0);

    static const char *c_lbls[] = {"📋 All Items", "📝 Text", "🖼️ Images", "⭐ Favorites"};
    for(int i=0; i<4; i++){
        GtkWidget *b = gtk_button_new_with_label(c_lbls[i]);
        gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
        gtk_widget_set_halign(b, GTK_ALIGN_FILL);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "cat-btn");
        if(i==0){ activeBtn=b; gtk_style_context_add_class(gtk_widget_get_style_context(b), "active"); }
        g_signal_connect(b, "clicked", G_CALLBACK(onCatClick), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(fBox), b, FALSE, FALSE, 0);
    }

    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(side), sw, TRUE, TRUE, 0);
    GtkWidget *sBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(sw), sBox);
    sourceSidebarBox = addSidebarSection(sBox, "Sources");
    tagSidebarBox = addSidebarSection(sBox, "Tags");

    GtkWidget *main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(main), "manager-content");
    gtk_box_pack_start(GTK_BOX(hbox), main, TRUE, TRUE, 0);

    mgrSearch = gtk_search_entry_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(mgrSearch), "search-entry");
    gtk_box_pack_start(GTK_BOX(main), mgrSearch, FALSE, FALSE, 0);

    GtkWidget *gsw = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(main), gsw, TRUE, TRUE, 0);
    
    mgrGrid = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(mgrGrid), GTK_SELECTION_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(mgrGrid), "clips-list");
    gtk_container_add(GTK_CONTAINER(gsw), mgrGrid);

    mgrEmpty = gtk_label_new("No clips found.");
    gtk_box_pack_start(GTK_BOX(main), mgrEmpty, FALSE, FALSE, 100);

    g_signal_connect(mgrWin, "delete-event", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(mgrWin, "key-press-event", G_CALLBACK(onWinKey), NULL);
    gtk_widget_show_all(mgrWin);
    gtk_widget_hide(mgrWin);
}

/* ── Add List Row ── */

void addClipCard(int id, const char *text, int textLen, int isImg, const char *imgPath, int isFav, const char *tags, const char *srcApp, const char *timeStr) {
    GtkWidget *row_box = gtk_event_box_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(row_box), "clip-row");
    gtk_widget_set_size_request(row_box, -1, 60); // 严格锁死 60px 行高

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_widget_set_valign(hbox, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(row_box), hbox);

    /* Data binding */
    g_object_set_data(G_OBJECT(row_box), "clip-id", GINT_TO_POINTER(id));
    g_object_set_data(G_OBJECT(row_box), "clip-is-image", GINT_TO_POINTER(isImg));
    g_object_set_data(G_OBJECT(row_box), "is-fav", GINT_TO_POINTER(isFav));
    if(text) g_object_set_data_full(G_OBJECT(row_box), "clip-text", g_strndup(text, textLen), g_free);
    if(imgPath) g_object_set_data_full(G_OBJECT(row_box), "clip-image-path", g_strdup(imgPath), g_free);
    if(srcApp) g_object_set_data_full(G_OBJECT(row_box), "clip-source", g_strdup(srcApp), g_free);

    /* 1. Favorite */
    GtkWidget *fav = gtk_button_new_with_label(isFav ? "❤️" : "🤍");
    gtk_style_context_add_class(gtk_widget_get_style_context(fav), "btn-icon");
    gtk_style_context_add_class(gtk_widget_get_style_context(fav), "btn-fav");
    if(isFav) gtk_style_context_add_class(gtk_widget_get_style_context(fav), "active");
    g_signal_connect(fav, "clicked", G_CALLBACK(onHeartClick), row_box);
    gtk_box_pack_start(GTK_BOX(hbox), fav, FALSE, FALSE, 0);

    /* 2. Content Preview (STRICT 1 LINE) */
    GtkWidget *pBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_size_request(pBox, 350, 40); // 锁死预览区宽度和高度
    gtk_box_pack_start(GTK_BOX(hbox), pBox, FALSE, FALSE, 0);

    if (isImg && imgPath) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(imgPath, 50, 40, TRUE, NULL);
        if(pb){ gtk_box_pack_start(GTK_BOX(pBox), gtk_image_new_from_pixbuf(pb), FALSE, FALSE, 0); g_object_unref(pb); }
        gtk_box_pack_start(GTK_BOX(pBox), gtk_label_new("(Image)"), FALSE, FALSE, 0);
    } else if (text) {
        GtkWidget *lbl = gtk_label_new(text);
        gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
        gtk_label_set_lines(GTK_LABEL(lbl), 1); // 强制锁死 1 行
        gtk_label_set_max_width_chars(GTK_LABEL(lbl), 50);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "row-content-text");
        gtk_box_pack_start(GTK_BOX(pBox), lbl, TRUE, TRUE, 0);
    }

    /* 3. Tags (Single line box) */
    GtkWidget *tBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(tBox, 200, 30);
    gtk_box_pack_start(GTK_BOX(hbox), tBox, TRUE, TRUE, 0);
    if(tags && tags[0]){
        char **list = g_strsplit(tags, ",", -1);
        for(int i=0; list[i] && i < 3; i++) if(list[i][0]){ // Max 3 tags in list
            GtkWidget *eb = gtk_event_box_new();
            GtkWidget *tl = gtk_label_new(list[i]);
            gtk_style_context_add_class(gtk_widget_get_style_context(eb), "tag-pill");
            gtk_style_context_add_class(gtk_widget_get_style_context(eb), tagColorClass(list[i]));
            gtk_container_add(GTK_CONTAINER(eb), tl);
            g_object_set_data(G_OBJECT(eb), "clip-id", GINT_TO_POINTER(id));
            g_object_set_data_full(G_OBJECT(eb), "tag-name", g_strdup(list[i]), g_free);
            g_signal_connect(eb, "button-press-event", G_CALLBACK(onTagRemove), NULL);
            gtk_box_pack_start(GTK_BOX(tBox), eb, FALSE, FALSE, 0);
        }
        g_strfreev(list);
    }
    GtkWidget *at = gtk_button_new_with_label("+");
    gtk_style_context_add_class(gtk_widget_get_style_context(at), "btn-icon");
    g_signal_connect(at, "clicked", G_CALLBACK(onAddTagClick), GINT_TO_POINTER(id));
    gtk_box_pack_start(GTK_BOX(tBox), at, FALSE, FALSE, 0);

    /* 4. Info (Right Aligned) */
    char buf[128]; snprintf(buf, 128, "%s  \xc2\xb7  %s", timeStr ? timeStr : "", srcApp ? srcApp : "Unknown");
    GtkWidget *il = gtk_label_new(buf);
    gtk_style_context_add_class(gtk_widget_get_style_context(il), "row-info");
    gtk_box_pack_start(GTK_BOX(hbox), il, FALSE, FALSE, 0);

    /* 5. Delete */
    GtkWidget *del = gtk_button_new_with_label("🗑️");
    gtk_style_context_add_class(gtk_widget_get_style_context(del), "btn-icon");
    gtk_style_context_add_class(gtk_widget_get_style_context(del), "btn-del");
    g_signal_connect(del, "clicked", G_CALLBACK(onDelClick), row_box);
    gtk_box_pack_start(GTK_BOX(hbox), del, FALSE, FALSE, 0);

    g_signal_connect(row_box, "button-press-event", G_CALLBACK(onRowEvt), NULL);
    gtk_list_box_insert(GTK_LIST_BOX(mgrGrid), row_box, -1);
    gtk_widget_show_all(row_box);
}

/* ── Go Callbacks ── */

void addTagButton(const char *tag) {
    char b[64]; snprintf(b, 64, "# %s", tag);
    GtkWidget *btn = gtk_button_new_with_label(b);
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn), "cat-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn), "tree-item");
    g_object_set_data_full(G_OBJECT(btn), "tag-name", g_strdup(tag), g_free);
    g_signal_connect(btn, "clicked", G_CALLBACK(onTagGroupClick), NULL);
    gtk_box_pack_start(GTK_BOX(tagSidebarBox), btn, FALSE, FALSE, 0);
    gtk_widget_show_all(btn);
}

void addSourceButton(const char *src) {
    GtkWidget *btn = gtk_button_new_with_label(src);
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn), "cat-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn), "tree-item");
    g_object_set_data_full(G_OBJECT(btn), "source-name", g_strdup(src), g_free);
    g_signal_connect(btn, "clicked", G_CALLBACK(onSourceGroupClick), NULL); 
    gtk_box_pack_start(GTK_BOX(sourceSidebarBox), btn, FALSE, FALSE, 0);
    gtk_widget_show_all(btn);
}

void showManagerWindow(void) { gtk_widget_show_all(mgrWin); gtk_window_present(GTK_WINDOW(mgrWin)); }
void clearGrid(void) { GList *l=gtk_container_get_children(GTK_CONTAINER(mgrGrid)); for(GList *i=l;i;i=i->next) gtk_widget_destroy(GTK_WIDGET(i->data)); g_list_free(l); gtk_widget_hide(mgrEmpty); }
void showEmpty(void) { gtk_widget_show(mgrEmpty); }
void clearTagSidebar(void) { GList *l=gtk_container_get_children(GTK_CONTAINER(tagSidebarBox)); for(GList *i=l;i;i=i->next) gtk_widget_destroy(GTK_WIDGET(i->data)); g_list_free(l); }
void clearSourceSidebar(void) { GList *l=gtk_container_get_children(GTK_CONTAINER(sourceSidebarBox)); for(GList *i=l;i;i=i->next) gtk_widget_destroy(GTK_WIDGET(i->data)); g_list_free(l); }
gboolean rebuildGridIdle(gpointer d) { goRebuildGrid(); return FALSE; }
gboolean rebuildTagSidebarIdle(gpointer d) { goRebuildTagSidebar(); return FALSE; }
gboolean rebuildSourceSidebarIdle(gpointer d) { goRebuildSourceSidebar(); return FALSE; }
gboolean showManagerIdle(gpointer d) { showManagerWindow(); return FALSE; }
gboolean stopIdle(gpointer d) { gtk_main_quit(); return FALSE; }
const char *getManagerSearchText(void) { return gtk_entry_get_text(GTK_ENTRY(mgrSearch)); }
