#ifndef MANAGER_H
#define MANAGER_H

#include <gtk/gtk.h>

/* Build / lifecycle */
void initManagerWindow(void);
void toggleManagerWindow(void);
void showManagerWindow(void);
void hideManagerWindow(void);

/* Data */
void clearGrid(void);
void showEmpty(void);
void addClipCard(int id, const char *text, int textLen,
                 int isImage, const char *imagePath, int isFav,
                 const char *tags, const char *sourceApp,
                 const char *timeStr);

/* Sidebar */
void clearTagSidebar(void);
void addTagButton(const char *tag);
void clearSourceSidebar(void);
void addSourceButton(const char *source);

/* Idle callbacks for g_idle_add */
gboolean rebuildGridIdle(gpointer data);
gboolean rebuildTagSidebarIdle(gpointer data);
gboolean rebuildSourceSidebarIdle(gpointer data);
gboolean showManagerIdle(gpointer data);
gboolean toggleManagerIdle(gpointer data);
gboolean stopIdle(gpointer data);

/* Text access */
const char *getManagerSearchText(void);

/* Callbacks → Go */
extern void goCategoryChanged(int id);
extern void goManagerSearchChanged(void);
extern void goCardClicked(int id);
extern void goCardFavorited(int id);
extern void goCardDeleted(int id);
extern void goTagAdded(int id, char *tag);
extern void goTagRemoved(int id, char *tag);
extern void goTagGroupChanged(char *tagName);
extern void goSourceGroupChanged(char *source);
extern void goRebuildGrid(void);
extern void goRebuildTagSidebar(void);
extern void goRebuildSourceSidebar(void);

#endif
