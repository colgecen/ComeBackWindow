/*
 * Uygulama yardımcıları: günlük, veri dizini, ayar yükleme/kaydetme,
 * komut kuyruğu ve masaüstü bildirimleri.
 */
#include "app.h"

#include "dbus_service.h"
#include "kwin_script.h"
#include "peek.h"
#include "shortcuts.h"
#include "strip.h"
#include "tray.h"
#include "version.h"

#include <stdarg.h>

void app_log(App *app, const gchar *format, ...)
{
    if (app != NULL && !app->debug) {
        return;
    }
    va_list args;

    va_start(args, format);
    g_logv("comebackwindow", G_LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

void app_warn(App *app, const gchar *format, ...)
{
    va_list args;

    (void)app;
    va_start(args, format);
    g_logv("comebackwindow", G_LOG_LEVEL_WARNING, format, args);
    va_end(args);
}

gchar *app_data_dir(void)
{
    const gchar *override = g_getenv("CBW_DATA_DIR");

    if (override != NULL && *override != '\0') {
        return g_strdup(override);
    }
    return g_build_filename(CBW_DATADIR, "comebackwindow", NULL);
}

gchar *app_kwin_script_path(void)
{
    gchar *dir = app_data_dir();
    gchar *path = g_build_filename(dir, "kwin", "contents", "ui", "main.qml", NULL);

    g_free(dir);
    return path;
}

gboolean app_load_settings(App *app, GError **error)
{
    if (!settings_load(&app->settings, error)) {
        return FALSE;
    }
#ifdef CBW_DEV_DEFAULTS
    /* Geliştirme kolaylığı: ilk açılış penceresi atlanır. */
    app->settings.first_run_done = TRUE;
#endif
    return TRUE;
}

gboolean app_save_settings(App *app, GError **error)
{
    settings_clamp(&app->settings);
    if (!settings_save(&app->settings, error)) {
        return FALSE;
    }
    app->settings_dirty = FALSE;
    return TRUE;
}

gchar *app_settings_json(App *app)
{
    gchar *base = settings_to_json(&app->settings);
    gsize len = strlen(base);
    gchar *extra = g_strdup_printf(
        ",\"native_show_desktop\":%s,\"native_close\":%s,"
        "\"native_minimize\":%s,\"native_maximize\":%s,"
        "\"command_channel\":\"%s\"}",
        app->native_show_desktop ? "true" : "false",
        app->native_close ? "true" : "false",
        app->native_minimize ? "true" : "false",
        app->native_maximize ? "true" : "false",
        app->command_channel != NULL ? app->command_channel : "invoke");

    if (len > 0 && base[len - 1] == '}') {
        base[len - 1] = '\0';
    }
    gchar *result = g_strconcat(base, extra, NULL);

    g_free(base);
    g_free(extra);
    return result;
}

void app_notify_settings(App *app)
{
    dbus_service_emit_settings_changed(app);
    app_send_command(app, "settings-changed");
    strip_refresh(app->strip);
}

static void queue_command(App *app, const gchar *command)
{
    g_mutex_lock(&app->commands_lock);
    g_queue_push_tail(&app->commands, g_strdup(command));
    g_mutex_unlock(&app->commands_lock);
}

void app_send_command(App *app, const gchar *command)
{
    if (command == NULL || *command == '\0') {
        return;
    }
    if (g_strcmp0(app->command_channel, "invoke") == 0
        && kwin_script_send_command(app, command)) {
        return;
    }
    queue_command(app, command);
}

void app_on_kwin_ready(App *app)
{
    gsize count = 0;
    gchar **pending = app_take_commands(app, &count);

    app->kwin_ready = TRUE;
    app_log(app, "KWin köprüsü hazır (%zu bekleyen komut)", count);
    for (gsize i = 0; i < count; i++) {
        if (g_strcmp0(app->command_channel, "invoke") != 0
            || !kwin_script_send_command(app, pending[i])) {
            queue_command(app, pending[i]);
        }
    }
    g_strfreev(pending);
}

gchar **app_take_commands(App *app, gsize *count)
{
    GPtrArray *array = g_ptr_array_new();

    g_mutex_lock(&app->commands_lock);
    while (!g_queue_is_empty(&app->commands)) {
        g_ptr_array_add(array, g_queue_pop_head(&app->commands));
    }
    g_mutex_unlock(&app->commands_lock);

    g_ptr_array_add(array, NULL);
    if (count != NULL) {
        *count = array->len - 1;
    }
    return (gchar **)g_ptr_array_free(array, FALSE);
}

void app_clear_layout(App *app)
{
    if (app->layout == NULL) {
        return;
    }
    for (gsize i = 0; i < app->layout_len; i++) {
        g_free(app->layout[i].output);
        g_free(app->layout[i].edge);
    }
    g_free(app->layout);
    app->layout = NULL;
    app->layout_len = 0;
}

void app_notify(App *app, const gchar *title, const gchar *body)
{
    if (app->bus == NULL) {
        app_warn(app, "%s: %s", title, body);
        return;
    }
    GVariantBuilder actions;
    GVariantBuilder hints;

    g_variant_builder_init(&actions, G_VARIANT_TYPE("as"));
    g_variant_builder_init(&hints, G_VARIANT_TYPE("a{sv}"));
    g_dbus_connection_call(app->bus,
                           "org.freedesktop.Notifications",
                           "/org/freedesktop/Notifications",
                           "org.freedesktop.Notifications",
                           "Notify",
                           g_variant_new("(susss@as@a{sv}i)",
                                         CBW_PROGRAM_NAME, 0u, "comebackwindow",
                                         title, body,
                                         g_variant_builder_end(&actions),
                                         g_variant_builder_end(&hints), 5000),
                           NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

void app_strip_hover(App *app, gboolean inside)
{
    app_log(app, "şerit %s", inside ? "üzerinde" : "ayrıldı");
    if (app->peek == NULL) {
        return;
    }
    if (inside) {
        peek_pointer_enter(app->peek);
    } else {
        peek_pointer_leave(app->peek);
    }
}

void app_strip_click(App *app)
{
    app_log(app, "şeride tıklandı: masaüstü aç/kapat");
    app_send_command(app, "toggle-desktop");
}

void app_handle_event(App *app, const gchar *name, const gchar *json)
{
    if (name == NULL) {
        return;
    }
    if (g_str_has_prefix(name, "peek-")) {
        peek_on_event(app->peek, name, json);
        return;
    }
    if (g_strcmp0(name, "script-loaded") == 0) {
        /* Betik yeniden yüklendi: ayarları hemen gönder. */
        app_notify_settings(app);
        shortcuts_check(app);
        return;
    }
    if (g_strcmp0(name, "shake") == 0) {
        app_log(app, "Aero Shake algılandı: %s", json != NULL ? json : "{}");
        return;
    }
    if (g_strcmp0(name, "desktop-shown") == 0) {
        app->desktop_shown = TRUE;
        strip_set_peek_active(app->strip, FALSE);
        tray_update_status(app->tray);
        return;
    }
    if (g_strcmp0(name, "desktop-restored") == 0) {
        app->desktop_shown = FALSE;
        tray_update_status(app->tray);
        return;
    }
}
