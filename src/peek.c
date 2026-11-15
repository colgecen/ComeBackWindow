/*
 * Aero Peek zamanlayıcısı
 *
 * Şeridin üzerine gelindiğinde `peek_delay_ms` kadar beklenir, sonra betiğe
 * "peek-begin" komutu gönderilir. İşaretçi ayrıldığında "peek-end" gönderilir.
 * Betik opaklık yazamazsa "peek-fallback" olayı gelir; bu durumda uygulama
 * masaüstünü geçici olarak gösterir (§8.5 yedeği).
 */
#include "peek.h"

#include "strip.h"

struct _Peek {
    App *app;
    guint delay_id;
    guint fallback_id;
    gint delay_ms;
    gboolean hover;
    gboolean active;
    gboolean fallback_active;
};

static void send_peek_end(Peek *peek)
{
    if (!peek->active) {
        return;
    }
    peek->active = FALSE;
    app_send_command(peek->app, "peek-end");
    strip_set_peek_active(peek->app->strip, FALSE);
}

static gboolean fallback_end_cb(gpointer data)
{
    Peek *peek = data;

    peek->fallback_id = 0;
    if (peek->fallback_active) {
        peek->fallback_active = FALSE;
        app_send_command(peek->app, "toggle-desktop");
    }
    return G_SOURCE_REMOVE;
}

static void begin_peek(Peek *peek)
{
    if (peek->active || !peek->app->settings.peek_enabled) {
        return;
    }
    if (peek->app->desktop_shown) {
        return;                 /* masaüstü zaten görünür: önizleme gereksiz */
    }
    peek->active = TRUE;
    app_send_command(peek->app, "peek-begin");
    strip_set_peek_active(peek->app->strip, TRUE);
}

static gboolean delay_cb(gpointer data)
{
    Peek *peek = data;

    peek->delay_id = 0;
    if (peek->hover) {
        begin_peek(peek);
    }
    return G_SOURCE_REMOVE;
}

static void cancel_delay(Peek *peek)
{
    if (peek->delay_id != 0) {
        g_source_remove(peek->delay_id);
        peek->delay_id = 0;
    }
}

Peek *peek_create(App *app)
{
    Peek *peek = g_new0(Peek, 1);

    peek->app = app;
    peek->delay_ms = app->settings.peek_delay_ms;
    app->peek = peek;
    return peek;
}

void peek_destroy(Peek *peek)
{
    if (peek == NULL) {
        return;
    }
    cancel_delay(peek);
    if (peek->fallback_id != 0) {
        g_source_remove(peek->fallback_id);
        peek->fallback_id = 0;
    }
    if (peek->app != NULL && peek->app->peek == peek) {
        peek->app->peek = NULL;
    }
    g_free(peek);
}

void peek_pointer_enter(Peek *peek)
{
    if (peek == NULL || peek->hover) {
        return;
    }
    peek->hover = TRUE;
    if (!peek->app->settings.peek_enabled) {
        return;
    }
    if (peek->delay_id != 0) {
        g_source_remove(peek->delay_id);
    }
    gint delay = peek->app->settings.peek_delay_ms;

    if (delay < 0) {
        delay = 0;
    }
    peek->delay_id = g_timeout_add((guint)delay, delay_cb, peek);
}

void peek_pointer_leave(Peek *peek)
{
    if (peek == NULL || !peek->hover) {
        return;
    }
    peek->hover = FALSE;
    cancel_delay(peek);
    send_peek_end(peek);
}

gboolean peek_click(Peek *peek)
{
    if (peek == NULL || !peek->fallback_active) {
        return FALSE;
    }
    /* Yedek önizleme masaüstünü zaten gösterdi; tıklama onu onaylar. */
    if (peek->fallback_id != 0) {
        g_source_remove(peek->fallback_id);
        peek->fallback_id = 0;
    }
    peek->fallback_active = FALSE;
    return TRUE;
}

void peek_toggle(Peek *peek)
{
    if (peek == NULL) {
        return;
    }
    if (peek->active) {
        send_peek_end(peek);
    } else {
        begin_peek(peek);
    }
}

void peek_on_event(Peek *peek, const gchar *name, const gchar *json)
{
    (void)json;
    if (peek == NULL || name == NULL) {
        return;
    }
    if (g_strcmp0(name, "peek-begin") == 0) {
        peek->active = TRUE;
        strip_set_peek_active(peek->app->strip, TRUE);
        return;
    }
    if (g_strcmp0(name, "peek-end") == 0
        || g_strcmp0(name, "peek-guard-restored") == 0) {
        peek->active = FALSE;
        strip_set_peek_active(peek->app->strip, FALSE);
        return;
    }
    if (g_strcmp0(name, "peek-fallback") == 0) {
        /* Opaklık yazılamadı: masaüstünü geçici olarak göster. */
        peek->active = FALSE;
        peek->fallback_active = TRUE;
        app_send_command(peek->app, "toggle-desktop");
        if (peek->fallback_id != 0) {
            g_source_remove(peek->fallback_id);
        }
        peek->fallback_id = g_timeout_add(1500, fallback_end_cb, peek);
    }
}

gboolean peek_is_active(const Peek *peek)
{
    return peek != NULL && peek->active;
}

void peek_reload_settings(Peek *peek){
    if (peek == NULL) {
        return;
    }
    peek->delay_ms = peek->app->settings.peek_delay_ms;
    if (!peek->app->settings.peek_enabled) {
        cancel_delay(peek);
        send_peek_end(peek);
    }
}
