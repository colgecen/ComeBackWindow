/* Şerit iç arayüzü — arka uçlar arasında paylaşılır */
#pragma once

#include "strip.h"

G_BEGIN_DECLS

struct _Strip {
    App *app;
    StripCallbacks callbacks;
    gpointer callbacks_data;
    gboolean hover;
    gboolean peek_active;
    gboolean wayland;
    gboolean destroyed;
    gboolean pending_hover;
    gpointer backend;
    gint refs;
};

/* Arka uçlar tarafından ana döngüye olay iletmek için kullanılır. */
void strip_notify_hover(Strip *strip, gboolean inside);
void strip_notify_click(Strip *strip);

/* Yaşam süresi: ana döngüye aktarılan olaylar bir referans tutar. */
Strip *strip_ref(Strip *strip);
void strip_unref(Strip *strip);

/* Arka uçlar (strip.c tarafından seçilir). */
Strip *strip_wayland_create(App *app, GError **error);
Strip *strip_x11_create(App *app, GError **error);

void strip_wayland_destroy(Strip *strip);
void strip_x11_destroy(Strip *strip);
void strip_wayland_update(Strip *strip);
void strip_x11_update(Strip *strip);

G_END_DECLS
