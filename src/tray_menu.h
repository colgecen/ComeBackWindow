/* Tepsi menüsü (com.canonical.dbusmenu) */
#pragma once

#include "app.h"

G_BEGIN_DECLS

typedef enum {
    MENU_ITEM_NORMAL = 0,
    MENU_ITEM_SEPARATOR
} MenuItemType;

typedef struct {
    gint id;
    MenuItemType type;
    const gchar *label;
    const gchar *action;      /* uygulama komutu; NULL ise işlevsiz */
} MenuItemSpec;

/* Yerleşik menü tanımı ve öğe sayısı. */
const MenuItemSpec *tray_menu_items(gsize *count);

/* dbusmenu GetLayout yanıtı: (u revision, (ia{sv}av) layout). */
GVariant *tray_menu_layout(const MenuItemSpec *items, gsize count,
                           guint revision, gint parent_id,
                           gint depth);

/* Menü kimliğine karşılık gelen eylem; yoksa NULL. */
const gchar *tray_menu_action(gint id);

/* Menüyü D-Bus üzerinde yayınlar. */
gboolean tray_menu_register(App *app, GDBusConnection *bus, GError **error);
void tray_menu_unregister(App *app);

/* Menüyü tetikleyen kimlikle uygulama eylemini çalıştırır. */
void tray_menu_activate(App *app, gint id);

G_END_DECLS
