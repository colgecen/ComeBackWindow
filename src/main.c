/*
 * Come Back Window — giriş noktası
 *
 * Tek örnek: org.comebackwindow.App adı alınır. Ad alınamazsa mevcut örneğe
 * komut yönlendirilir ve süreç çıkar.
 */
#include "app.h"
#include "cli.h"
#include "dbus_service.h"
#include "dialogs.h"
#include "i18n.h"
#include "kwin_script.h"
#include "peek.h"
#include "shortcuts.h"
#include "strip.h"
#include "tray.h"
#include "version.h"

#include <glib-unix.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    App app;
    CliOptions opts;
    gboolean had_name;
    gboolean forwarded;
    gboolean dump_done;
    gboolean dump_timed_out;
    gchar *dump_json;
} Run;

static gint on_signal(gpointer user_data)
{
    Run *run = user_data;

    app_log(&run->app, "sinyal alındı, çıkılıyor");
    g_main_loop_quit(run->app.loop);
    return G_SOURCE_REMOVE;
}

static gboolean forward_call(Run *run, const gchar *method)
{
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_sync(
        run->app.bus,
        CBW_APP_ID,
        CBW_OBJECT_PATH,
        CBW_APP_ID,
        method,
        NULL,
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        5000,
        NULL,
        &error);

    if (reply == NULL) {
        if (error != NULL) {
            g_printerr("comebackwindow: %s çağrısı başarısız: %s\n",
                       method, error->message);
            g_clear_error(&error);
        }
        return FALSE;
    }
    g_variant_unref(reply);
    return TRUE;
}

static void on_state_reported(GDBusConnection *connection,
                              const gchar *sender_name,
                              const gchar *object_path,
                              const gchar *interface_name,
                              const gchar *signal_name,
                              GVariant *parameters,
                              gpointer user_data)
{
    Run *run = user_data;
    const gchar *json = NULL;

    (void)connection;
    (void)sender_name;
    (void)object_path;
    (void)interface_name;
    (void)signal_name;

    g_variant_get(parameters, "(&s)", &json);
    g_free(run->dump_json);
    run->dump_json = g_strdup(json);
    run->dump_done = TRUE;
    if (run->app.loop != NULL) {
        g_main_loop_quit(run->app.loop);
    }
}

static gboolean dump_timeout(gpointer user_data)
{
    Run *run = user_data;

    run->dump_timed_out = TRUE;
    g_main_loop_quit(run->app.loop);
    return G_SOURCE_REMOVE;
}

static gboolean run_client_command(Run *run)
{
    CliMode mode = run->opts.mode;

    if (mode == CBW_MODE_DUMP_STATE) {
        g_dbus_connection_signal_subscribe(run->app.bus, NULL, CBW_APP_ID,
                                           "StateReported", CBW_OBJECT_PATH,
                                           NULL, G_DBUS_SIGNAL_FLAGS_NONE,
                                           on_state_reported, run, NULL);
        if (!forward_call(run, "DumpState")) {
            return FALSE;
        }
        guint timeout = g_timeout_add(2000, dump_timeout, run);
        g_main_loop_run(run->app.loop);
        if (!run->dump_timed_out) {
            g_source_remove(timeout);
        }
        if (run->dump_json != NULL) {
            g_print("%s\n", run->dump_json);
        }
        return run->dump_json != NULL;
    }

    const gchar *method = cli_mode_method(mode);

    if (method == NULL) {
        return TRUE;
    }
    return forward_call(run, method);
}

static void on_bus_closed(GDBusConnection *connection,
                          gboolean remote_peer_vanished, GError *error,
                          gpointer user_data)
{
    Run *run = user_data;

    (void)connection;
    (void)remote_peer_vanished;
    app_warn(&run->app, "D-Bus bağlantısı kapandı (%s); uygulama kapanıyor",
             error != NULL ? error->message : "bilinmeyen neden");
    g_main_loop_quit(run->app.loop);
}

static void bus_acquired_cb(GDBusConnection *connection,
                            const gchar *name,
                            gpointer user_data)
{
    Run *run = user_data;
    GError *error = NULL;

    (void)name;
    app_log(&run->app, "D-Bus bağlantısı kuruldu");
    g_dbus_connection_set_exit_on_close(connection, FALSE);
    g_signal_connect(connection, "closed", G_CALLBACK(on_bus_closed), run);

    if (!dbus_service_register(&run->app, connection, &error)) {
        g_printerr("comebackwindow: D-Bus nesnesi kaydedilemedi: %s\n",
                   error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
        g_main_loop_quit(run->app.loop);
        return;
    }
}

static void name_lost_cb(GDBusConnection *connection,
                         const gchar *name,
                         gpointer user_data)
{
    Run *run = user_data;

    (void)name;
    app_log(&run->app, "D-Bus adı alınamadı (başka bir örnek çalışıyor)");
    if (connection == NULL) {
        connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    }
    if (connection == NULL) {
        g_printerr("comebackwindow: oturum veriyoluna bağlanılamadı\n");
        g_main_loop_quit(run->app.loop);
        return;
    }
    run->app.bus = connection;

    if (run->opts.mode == CBW_MODE_RUN) {
        run->forwarded = forward_call(run, "Activate");
    } else if (run->opts.mode == CBW_MODE_SETUP) {
        run->forwarded = forward_call(run, "OpenSetup");
    } else if (cli_mode_is_client(run->opts.mode)) {
        run->forwarded = run_client_command(run);
    } else {
        run->forwarded = forward_call(run, "Activate");
    }
    g_main_loop_quit(run->app.loop);
}

static gboolean startup_components(Run *run);

static gboolean run_restore_shortcuts(void)
{
    GError *error = NULL;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

    if (bus == NULL) {
        g_printerr("comebackwindow: oturum veriyoluna bağlanılamadı: %s\n",
                   error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
        return FALSE;
    }
    gboolean had_backup = FALSE;
    gboolean ok = shortcuts_restore(bus, &had_backup, &error);

    g_object_unref(bus);
    if (!ok) {
        g_printerr("comebackwindow: kısayollar geri yüklenemedi: %s\n",
                   error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
        return FALSE;
    }
    if (had_backup) {
        g_print("Kısayol yedeği geri yüklendi.\n");
    } else {
        g_print("Geri yüklenecek kısayol yedeği yok.\n");
    }
    return TRUE;
}

static void on_name_acquired(GDBusConnection *connection,
                             const gchar *name,
                             gpointer user_data)
{
    Run *run = user_data;

    (void)connection;
    (void)name;
    run->had_name = TRUE;
    app_log(&run->app, "tek örnek: ad alındı");
    if (!startup_components(run)) {
        g_main_loop_quit(run->app.loop);
    }
}

static gboolean parse_bool_env(const gchar *name)
{
    const gchar *value = g_getenv(name);

    if (value == NULL) {
        return FALSE;
    }
    return value[0] == '1' || g_ascii_strcasecmp(value, "true") == 0
        || g_ascii_strcasecmp(value, "yes") == 0;
}

static void strip_hover_cb(Strip *strip, gboolean inside, gpointer user_data)
{
    (void)strip;
    app_strip_hover(user_data, inside);
}

static void strip_click_cb(Strip *strip, gpointer user_data)
{
    (void)strip;
    app_strip_click(user_data);
}

static void strip_press_cb(Strip *strip, gpointer user_data)
{
    (void)strip;
    app_strip_press(user_data);
}

static void start_strip(Run *run)
{
    App *app = &run->app;
    GError *error = NULL;
    StripCallbacks callbacks = {
        .hover = strip_hover_cb,
        .click = strip_click_cb,
        .press = strip_press_cb
    };

    if (app->no_strip || !app->settings.strip_enabled) {
        app_log(app, "şerit devre dışı");
        return;
    }
    if (app->peek == NULL) {
        peek_create(app);
    }
    Strip *strip = strip_create(app, &error);

    if (strip == NULL) {
        app_warn(app, "şerit açılamadı: %s",
                 error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
        return;
    }
    strip_set_callbacks(strip, &callbacks, app);
    strip_update(strip);
}

static void start_tray(Run *run)
{
    App *app = &run->app;
    GError *error = NULL;

    if (app->no_tray || !app->settings.tray_enabled) {
        app_log(app, "tepsi devre dışı");
        return;
    }
    if (tray_create(app, &error) == NULL) {
        app_warn(app, "tepsi açılamadı: %s",
                 error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
    }
}

static gboolean startup_components(Run *run)
{
    if (run->opts.no_tray) {
        run->app.no_tray = TRUE;
    }
    if (run->opts.no_strip) {
        run->app.no_strip = TRUE;
    }
    app_log(&run->app, "bileşenler başlatılıyor");
    app_sync_autostart(&run->app);
    if (!kwin_script_init(&run->app)) {
        /* Betik henüz yüklenemedi; köprü kendi yeniden denemesini kurdu. */
        app_warn(&run->app, "KWin betiği şimdilik yüklenemedi");
    }
    start_strip(run);
    start_tray(run);
    if (run->opts.mode == CBW_MODE_SETUP) {
        dialogs_open_setup(&run->app);
    } else {
        dialogs_open_setup_if_needed(&run->app);
    }
    return TRUE;
}

int main(int argc, char **argv)
{
    Run run;
    GError *error = NULL;

    memset(&run, 0, sizeof(run));
    setlocale(LC_ALL, "");
    g_set_prgname(CBW_PROGRAM_NAME);
    g_set_application_name("Come Back Window");

    cli_options_init(&run.opts);
    if (!cli_parse(argc, argv, &run.opts, &error)) {
        g_printerr("comebackwindow: %s\n\n%s", error->message, cli_usage());
        g_clear_error(&error);
        return 2;
    }
    if (run.opts.help) {
        g_print("%s", cli_usage());
        return 0;
    }
    if (run.opts.version) {
        g_print("comebackwindow %s\n", CBW_VERSION);
        return 0;
    }
    if (run.opts.mode == CBW_MODE_RESTORE_SHORTCUTS) {
        return run_restore_shortcuts() ? 0 : 1;
    }

    App *app = &run.app;
    app->debug = run.opts.debug || parse_bool_env("CBW_DEBUG");
    app->native_show_desktop = TRUE;
    app->native_close = TRUE;
    app->native_minimize = TRUE;
    app->native_maximize = TRUE;
    app->command_channel = g_strdup("invoke");
    settings_init_defaults(&app->settings);
    g_queue_init(&app->commands);
    g_mutex_init(&app->commands_lock);

    if (!app_load_settings(app, &error)) {
        g_printerr("comebackwindow: ayarlar yüklenemedi: %s\n",
                   error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
        return 1;
    }

    i18n_init(app->settings.language);
    app->loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGINT, on_signal, &run);
    g_unix_signal_add(SIGTERM, on_signal, &run);
    g_unix_signal_add(SIGHUP, on_signal, &run);

    guint owner = g_bus_own_name(G_BUS_TYPE_SESSION, CBW_APP_ID,
                                 G_BUS_NAME_OWNER_FLAGS_NONE,
                                 bus_acquired_cb, on_name_acquired,
                                 name_lost_cb, &run, NULL);

    g_main_loop_run(app->loop);

    dialogs_shutdown(app);
    tray_destroy(app->tray);
    strip_destroy(app->strip);
    peek_destroy(app->peek);
    kwin_script_shutdown(app);
    dbus_service_unregister(app);
    g_bus_unown_name(owner);
    g_main_loop_unref(app->loop);
    g_free(app->last_layout_json);
    g_free(app->last_event_name);
    g_free(app->last_event_json);
    g_free(app->state_json);
    g_free(app->last_state_json);
    g_free(app->command_channel);
    settings_clear(&app->settings);
    g_queue_clear_full(&app->commands, g_free);
    g_mutex_clear(&app->commands_lock);

    return (run.forwarded || run.dump_done || run.had_name) ? 0 : 1;
}
