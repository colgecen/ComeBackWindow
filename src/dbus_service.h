/* org.comebackwindow.App D-Bus servisi */
#pragma once

#include "app.h"

G_BEGIN_DECLS

/* Nesneyi dışa aktarır. */
gboolean dbus_service_register(App *app, GDBusConnection *bus, GError **error);

/* Adı bırakır ve nesneyi kaldırır. */
void dbus_service_unregister(App *app);

/* Ayar değişikliği sinyali. */
void dbus_service_emit_settings_changed(App *app);

/* Betikten gelen olayları test/tepsi için yeniden yayar. */
void dbus_service_emit_event(App *app, const gchar *name, const gchar *json);

G_END_DECLS
