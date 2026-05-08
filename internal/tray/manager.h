#ifndef MANAGER_H
#define MANAGER_H

#include <gtk/gtk.h>

typedef struct {
    int         id;
    const char *label;
    const char *icon;
} Category;

/* Build / lifecycle */
void initManagerWindow(void);
void toggleManagerWindow(void);
void showManagerWindow(void);
void hideManagerWindow(void);

/* Data */
void clearGrid(void);
void addClipCard(int id, const char *text, int textLen,
                 int isImage, const char *imagePath, int isFav);

/* Idle callbacks for g_idle_add */
gboolean rebuildGridIdle(gpointer data);
gboolean showManagerIdle(gpointer data);
gboolean toggleManagerIdle(gpointer data);

/* Text access */
const char *getManagerSearchText(void);

/* Callbacks → Go */
extern void goCategoryChanged(int id);
extern void goManagerSearchChanged(void);
extern void goCardClicked(int id);
extern void goCardFavorited(int id);
extern void goCardDeleted(int id);
extern void goRebuildGrid(void);

#endif
