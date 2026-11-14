/*
 * KWin betiği köprüsü
 *
 * Faz 0 bulguları:
 *   - loadDeclarativeScript(path, plugin) dosya yolu bekler; yüklenen betik
 *     `/Scripting/Script<id>` nesnesi üzerinden `run` ile başlatılır.
 *   - Kısayol tetikleme `org.kde.kglobalaccel` `/component/kwin`
 *     `org.kde.kglobalaccel.Component.InvokeShortcut` ile yapılır.
 *   - `start()` tüm betikleri başlatır; zaten çalışan betiği atlar.
 */
#include "kwin_script.h"

#include "dbus_service.h"
#include "version.h"

#define KWIN_SERVICE "org.kde.KWin"
#define KWIN_PATH "/Scripting"
#define KWIN_IFACE "org.kde.kwin.Scripting"
#define KGA_SERVICE "org.kde.kglobalaccel"
#define KGA_PATH "/component/kwin"
#define KGA_IFACE "org.kde.kglobalaccel.Component"
#define PLUGIN_NAME "comebackwindow"

typedef struct {
    App *app;
    guint retry_id;
    guint retry_delay;
    guint kwin_watch_id;
    guint sleep_watch_id;
    gboolean loaded;
    gboolean init_done;
} KwinBridge;

static GVariant *call_sync(App *app, const gchar *service, const gchar *path,
                           const gchar *iface, const gchar *method,
                           GVariant *params, GError **error)
{
    if (app->bus == NULL) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED,
                    "D-Bus bağlantısı yok");
        return NULL;
    }
    return g_dbus_connection_call_sync(app->bus, service, path, iface, method,
                                       params, NULL, G_DBUS_CALL_FLAGS_NONE,
                                       5000, NULL, error);
}

static gboolean call_no_result(App *app, const gchar *service, const gchar *path,
                               const gchar *iface, const gchar *method,
                               GVariant *params)
{
    GError *error = NULL;
    GVariant *reply = call_sync(app, service, path, iface, method, params, &error);

    if (reply == NULL) {
        app_warn(app, "%s.%s çağrısı başarısız: %s", iface, method,
                 error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
        return FALSE;
    }
    g_variant_unref(reply);
    return TRUE;
}

static gboolean unload_script(App *app)
{
    return call_no_result(app, KWIN_SERVICE, KWIN_PATH, KWIN_IFACE,
                          "unloadScript",
                          g_variant_new("(s)", PLUGIN_NAME));
}

static gint load_declarative(App *app, const gchar *path)
{
    GError *error = NULL;
    GVariant *reply = call_sync(app, KWIN_SERVICE, KWIN_PATH, KWIN_IFACE,
                                "loadDeclarativeScript",
                                g_variant_new("(ss)", path, PLUGIN_NAME),
                                &error);

    if (reply == NULL) {
        app_warn(app, "loadDeclarativeScript başarısız: %s",
                 error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
        return -1;
    }
    gint id = -1;

    g_variant_get(reply, "(i)", &id);
    g_variant_unref(reply);
    return id;
}

static gboolean start_script(App *app, gint id)
{
    gchar *path = g_strdup_printf("/Scripting/Script%d", id);
    gboolean ok = call_no_result(app, KWIN_SERVICE, path,
                                 "org.kde.kwin.Script", "run", NULL);

    g_free(path);
    return ok;
}

static gboolean is_loaded(App *app)
{
    GError *error = NULL;
    GVariant *reply = call_sync(app, KWIN_SERVICE, KWIN_PATH, KWIN_IFACE,
                                "isScriptLoaded",
                                g_variant_new("(s)", PLUGIN_NAME), &error);

    if (reply == NULL) {
        g_clear_error(&error);
        return FALSE;
    }
    gboolean loaded = FALSE;

    g_variant_get(reply, "(b)", &loaded);
    g_variant_unref(reply);
    return loaded;
}

static void schedule_retry(App *app);

static void on_script_loaded(App *app)
{
    KwinBridge *bridge = app->kwin;

    bridge->loaded = TRUE;
    bridge->retry_delay = 0;
    app->kwin_failed_notified = FALSE;
    app_log(app, "KWin betiği yüklendi");

    /* Kanal A'yı dene: ayar değişikliği komutu zararsızdır. */
    gboolean invoke_ok = kwin_script_send_command(app, "settings-changed");

    g_free(app->command_channel);
    app->command_channel = g_strdup(invoke_ok ? "invoke" : "poll");
    dbus_service_emit_settings_changed(app);
    app_on_kwin_ready(app);
}

gboolean kwin_script_reload(App *app)
{
    KwinBridge *bridge = app->kwin;

    if (bridge == NULL || app->bus == NULL) {
        return FALSE;
    }
    bridge->loaded = FALSE;
    unload_script(app);

    gchar *path = app_kwin_script_path();
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        app_warn(app, "KWin betiği bulunamadı: %s", path);
        g_free(path);
        schedule_retry(app);
        return FALSE;
    }

    gint id = load_declarative(app, path);
    g_free(path);
    if (id < 0) {
        schedule_retry(app);
        return FALSE;
    }
    if (!start_script(app, id)) {
        schedule_retry(app);
        return FALSE;
    }
    if (!is_loaded(app)) {
        schedule_retry(app);
        return FALSE;
    }
    on_script_loaded(app);
    return TRUE;
}

static gboolean retry_cb(gpointer user_data)
{
    App *app = user_data;
    KwinBridge *bridge = app->kwin;

    if (bridge == NULL) {
        return G_SOURCE_REMOVE;
    }
    bridge->retry_id = 0;
    if (kwin_script_reload(app)) {
        return G_SOURCE_REMOVE;
    }
    schedule_retry(app);
    return G_SOURCE_REMOVE;
}

static void schedule_retry(App *app)
{
    KwinBridge *bridge = app->kwin;

    if (bridge == NULL || bridge->retry_id != 0) {
        return;
    }
    if (bridge->retry_delay == 0) {
        bridge->retry_delay = 1;
    } else if (bridge->retry_delay < 60) {
        bridge->retry_delay *= 2;
    }
    if (bridge->retry_delay > 60) {
        bridge->retry_delay = 60;
    }
    if (!app->kwin_failed_notified) {
        app->kwin_failed_notified = TRUE;
        app_notify(app, "Come Back Window",
                   "KWin betiği yüklenemedi; yeniden denenecek. "
                   "Kısayollar şimdilik çalışmıyor.");
    }
    app_log(app, "betik yükleme yeniden denemesi: %u sn", bridge->retry_delay);
    bridge->retry_id = g_timeout_add_seconds(bridge->retry_delay, retry_cb, app);
}

gboolean kwin_script_send_command(App *app, const gchar *command)
{
    static const struct {
        const gchar *name;
        const gchar *shortcut;
    } map[] = {
        { "toggle-desktop", "cbw-cmd-toggle-desktop" },
        { "peek-begin", "cbw-cmd-peek-begin" },
        { "peek-end", "cbw-cmd-peek-end" },
        { "peek-toggle", "cbw-cmd-peek-toggle" },
        { "minimize-others", "cbw-cmd-minimize-others" },
        { "restore-all", "cbw-cmd-restore-all" },
        { "settings-changed", "cbw-cmd-settings-changed" },
        { "dump-state", "cbw-cmd-dump-state" }
    };

    KwinBridge *bridge = app->kwin;

    if (bridge == NULL || !bridge->loaded) {
        return FALSE;
    }
    const gchar *shortcut = NULL;

    for (gsize i = 0; i < G_N_ELEMENTS(map); i++) {
        if (g_strcmp0(map[i].name, command) == 0) {
            shortcut = map[i].shortcut;
            break;
        }
    }
    if (shortcut == NULL) {
        return FALSE;
    }

    GError *error = NULL;
    GVariant *reply = call_sync(app, KGA_SERVICE, KGA_PATH, KGA_IFACE,
                                "invokeShortcut",
                                g_variant_new("(s)", shortcut), &error);

    if (reply == NULL) {
        if (g_strcmp0(app->command_channel, "poll") != 0) {
            g_free(app->command_channel);
            app->command_channel = g_strdup("poll");
            app_notify_settings(app);
        }
        app_warn(app, "kanal A başarısız (%s); kanal B'ye geçildi",
                 error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
        return FALSE;
    }
    g_variant_unref(reply);
    return TRUE;
}

gboolean kwin_script_notify_settings(App *app)
{
    return kwin_script_send_command(app, "settings-changed");
}

gboolean kwin_script_is_loaded(App *app)
{
    KwinBridge *bridge = app->kwin;

    return bridge != NULL && bridge->loaded;
}

void kwin_script_on_ready(App *app)
{
    app_on_kwin_ready(app);
}

static void name_owner_changed(GDBusConnection *connection,
                               const gchar *sender_name,
                               const gchar *object_path,
                               const gchar *interface_name,
                               const gchar *signal_name,
                               GVariant *parameters,
                               gpointer user_data)
{
    App *app = user_data;
    const gchar *name = NULL;
    const gchar *old_owner = NULL;
    const gchar *new_owner = NULL;

    (void)connection;
    (void)sender_name;
    (void)object_path;
    (void)interface_name;
    (void)signal_name;

    g_variant_get(parameters, "(&s&s&s)", &name, &old_owner, &new_owner);
    if (g_strcmp0(name, KWIN_SERVICE) != 0) {
        return;
    }
    if (new_owner != NULL && *new_owner != '\0') {
        app_log(app, "KWin yeniden başladı; betik yeniden yüklenecek");
        KwinBridge *bridge = app->kwin;

        if (bridge != NULL) {
            bridge->loaded = FALSE;
            bridge->retry_delay = 0;
            if (bridge->retry_id != 0) {
                g_source_remove(bridge->retry_id);
                bridge->retry_id = 0;
            }
            schedule_retry(app);
        }
    }
}

static void prepare_for_sleep(GDBusConnection *connection,
                              const gchar *sender_name,
                              const gchar *object_path,
                              const gchar *interface_name,
                              const gchar *signal_name,
                              GVariant *parameters,
                              gpointer user_data)
{
    App *app = user_data;
    gboolean sleeping = FALSE;

    (void)connection;
    (void)sender_name;
    (void)object_path;
    (void)interface_name;
    (void)signal_name;

    g_variant_get(parameters, "(b)", &sleeping);
    if (sleeping) {
        return;
    }
    app_log(app, "uyanma sonrası betik durumu doğrulanıyor");
    if (!is_loaded(app)) {
        kwin_script_reload(app);
    }
}

gboolean kwin_script_init(App *app)
{
    KwinBridge *bridge = g_new0(KwinBridge, 1);

    bridge->app = app;
    app->kwin = bridge;

    if (app->bus != NULL) {
        bridge->kwin_watch_id = g_dbus_connection_signal_subscribe(
            app->bus, "org.freedesktop.DBus", "org.freedesktop.DBus",
            "NameOwnerChanged", "/org/freedesktop/DBus", KWIN_SERVICE,
            G_DBUS_SIGNAL_FLAGS_NONE, name_owner_changed, app, NULL);

        bridge->sleep_watch_id = g_dbus_connection_signal_subscribe(
            app->bus, "org.freedesktop.login1", "org.freedesktop.login1",
            "PrepareForSleep", "/org/freedesktop/login1", NULL,
            G_DBUS_SIGNAL_FLAGS_NONE, prepare_for_sleep, app, NULL);
    }

    bridge->init_done = TRUE;
    if (!kwin_script_reload(app)) {
        schedule_retry(app);
        return FALSE;
    }
    return TRUE;
}

void kwin_script_shutdown(App *app)
{
    KwinBridge *bridge = app->kwin;

    if (bridge == NULL) {
        return;
    }
    if (bridge->retry_id != 0) {
        g_source_remove(bridge->retry_id);
        bridge->retry_id = 0;
    }
    if (app->bus != NULL) {
        if (bridge->kwin_watch_id != 0) {
            g_dbus_connection_signal_unsubscribe(app->bus, bridge->kwin_watch_id);
        }
        if (bridge->sleep_watch_id != 0) {
            g_dbus_connection_signal_unsubscribe(app->bus, bridge->sleep_watch_id);
        }
    }
    if (bridge->loaded) {
        unload_script(app);
    }
    g_free(bridge);
    app->kwin = NULL;
}
