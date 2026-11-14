/*
 * Masaüstünü Göster şeridi — X11 arka ucu (geçici iskelet)
 *
 * layer-shell bulunmayan oturumlar için ayrı bir commit'te doldurulur.
 */
#include "strip_internal.h"

Strip *strip_x11_create(App *app, GError **error)
{
    (void)app;
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                        "X11 şeridi henüz uygulanmadı");
    return NULL;
}

void strip_x11_update(Strip *strip)
{
    (void)strip;
}

void strip_x11_destroy(Strip *strip)
{
    (void)strip;
}
