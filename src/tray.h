/* StatusNotifierItem tepsi simgesi */
#pragma once

#include "app.h"

G_BEGIN_DECLS

/* Tepsi simgesini yayınlar ve StatusNotifierWatcher'a kaydolur. */
Tray *tray_create(App *app, GError **error);

void tray_destroy(Tray *tray);

/* Simge ipucu/durum metnini günceller (masaüstü durumu vb.). */
void tray_update_status(Tray *tray);

/* Menü kimliğiyle tetiklenen eylemi uygular (menü sunucusundan). */
void tray_activate_id(Tray *tray, gint id);

G_END_DECLS
