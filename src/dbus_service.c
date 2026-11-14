/*
 * org.comebackwindow.App D-Bus servisi
 *
 * KWin betiği bu nesneye rapor verir (ReportLayout/ReportEvent/ReportState/
 * Ping). Uygulama betiğe komut gönderir (kanal A kısayolu, kanal B
 * PollCommands). Yardımcı istemciler (CLI, tepsi) bu nesneyi çağırır.
 */
#include "dbus_service.h"

#include "layout.h"
#include "strip.h"
#include "version.h"

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='org.comebackwindow.App'>"
    "    <method name='Activate'/>"
    "    <method name='ToggleDesktop'/>"
    "    <method name='MinimizeOthers'/>"
    "    <method name='RestoreAll'/>"
    "    <method name='DumpState'/>"
    "    <method name='ReportLayout'>"
    "      <arg type='s' name='json' direction='in'/>"
    "    </method>"
    "    <method name='ReportEvent'>"
    "      <arg type='s' name='name' direction='in'/>"
    "      <arg type='s' name='json' direction='in'/>"
    "    </method>"
    "    <method name='ReportState'>"
    "      <arg type='s' name='json' direction='in'/>"
    "    </method>"
    "    <method name='Ping'>"
    "      <arg type='b' name='alive' direction='out'/>"
    "    </method>"
    "    <method name='GetSettings'>"
    "      <arg type='s' name='json' direction='out'/>"
    "    </method>"
    "    <method name='PollCommands'>"
    "      <arg type='as' name='commands' direction='out'/>"
    "    </method>"
    "    <signal name='SettingsChanged'/>"
    "    <signal name='EventReceived'>"
    "      <arg type='s' name='name'/>"
    "      <arg type='s' name='json'/>"
    "    </signal>"
    "    <signal name='StateReported'>"
    "      <arg type='s' name='json'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

static void emit_event_signal(App *app, const gchar *name, const gchar *json)
{
    if (app->bus == NULL) {
        return;
    }
    GError *error = NULL;

    g_dbus_connection_emit_signal(app->bus, NULL, CBW_OBJECT_PATH,
                                  CBW_APP_ID, "EventReceived",
                                  g_variant_new("(ss)", name, json),
                                  &error);
    if (error != NULL) {
        app_warn(app, "EventReceived sinyali gönderilemedi: %s", error->message);
        g_clear_error(&error);
    }
}

void dbus_service_emit_event(App *app, const gchar *name, const gchar *json)
{
    emit_event_signal(app, name, json != NULL ? json : "{}");
}

void dbus_service_emit_settings_changed(App *app)
{
    if (app->bus == NULL) {
        return;
    }
    GError *error = NULL;

    g_dbus_connection_emit_signal(app->bus, NULL, CBW_OBJECT_PATH,
                                  CBW_APP_ID, "SettingsChanged",
                                  NULL, &error);
    if (error != NULL) {
        app_warn(app, "SettingsChanged sinyali gönderilemedi: %s", error->message);
        g_clear_error(&error);
    }
}

static void handle_report_layout(App *app, GVariant *parameters)
{
    const gchar *json = NULL;

    g_variant_get(parameters, "(&s)", &json);
    g_free(app->last_layout_json);
    app->last_layout_json = g_strdup(json);
    if (layout_apply(app, json)) {
        app_log(app, "düzen raporu alındı: %zu ekran", app->layout_len);
        strip_update(app->strip);
    }
}

static void handle_report_event(App *app, GVariant *parameters)
{
    const gchar *name = NULL;
    const gchar *json = NULL;

    g_variant_get(parameters, "(&s&s)", &name, &json);
    g_free(app->last_event_name);
    g_free(app->last_event_json);
    app->last_event_name = g_strdup(name);
    app->last_event_json = g_strdup(json);
    if (app->debug) {
        app_log(app, "olay: %s %s", name, json);
    }
    app_handle_event(app, name, json);
    emit_event_signal(app, name, json);
}

static void handle_report_state(App *app, GVariant *parameters)
{
    const gchar *json = NULL;

    g_variant_get(parameters, "(&s)", &json);
    g_free(app->state_json);
    app->state_json = g_strdup(json);
    if (app->bus != NULL) {
        GError *error = NULL;

        g_dbus_connection_emit_signal(app->bus, NULL, CBW_OBJECT_PATH,
                                      CBW_APP_ID, "StateReported",
                                      g_variant_new("(s)", json), &error);
        if (error != NULL) {
            g_clear_error(&error);
        }
    }
}

static void method_call_cb(GDBusConnection *connection,
                           const gchar *sender,
                           const gchar *object_path,
                           const gchar *interface_name,
                           const gchar *method_name,
                           GVariant *parameters,
                           GDBusMethodInvocation *invocation,
                           gpointer user_data);

static const GDBusInterfaceVTable interface_vtable = {
    method_call_cb,
    NULL,
    NULL,
    { 0 }
};

static void dispatch_client_command(App *app, GDBusMethodInvocation *invocation,
                                    const gchar *command)
{
    if (command != NULL) {
        app_send_command(app, command);
    }
    g_dbus_method_invocation_return_value(invocation, NULL);
}

static void method_call_cb(GDBusConnection *connection,
                           const gchar *sender,
                           const gchar *object_path,
                           const gchar *interface_name,
                           const gchar *method_name,
                           GVariant *parameters,
                           GDBusMethodInvocation *invocation,
                           gpointer user_data)
{
    App *app = user_data;

    (void)connection;
    (void)sender;
    (void)object_path;
    (void)interface_name;

    if (g_strcmp0(method_name, "Activate") == 0) {
        dispatch_client_command(app, invocation, "settings");
        return;
    }
    if (g_strcmp0(method_name, "ToggleDesktop") == 0) {
        dispatch_client_command(app, invocation, "toggle-desktop");
        return;
    }
    if (g_strcmp0(method_name, "MinimizeOthers") == 0) {
        dispatch_client_command(app, invocation, "minimize-others");
        return;
    }
    if (g_strcmp0(method_name, "RestoreAll") == 0) {
        dispatch_client_command(app, invocation, "restore-all");
        return;
    }
    if (g_strcmp0(method_name, "DumpState") == 0) {
        app_send_command(app, "dump-state");
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
        return;
    }
    if (g_strcmp0(method_name, "ReportLayout") == 0) {
        handle_report_layout(app, parameters);
        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }
    if (g_strcmp0(method_name, "ReportEvent") == 0) {
        handle_report_event(app, parameters);
        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }
    if (g_strcmp0(method_name, "ReportState") == 0) {
        handle_report_state(app, parameters);
        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }
    if (g_strcmp0(method_name, "Ping") == 0) {
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(b)", TRUE));
        return;
    }
    if (g_strcmp0(method_name, "GetSettings") == 0) {
        gchar *json = app_settings_json(app);

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(s)", json));
        g_free(json);
        return;
    }
    if (g_strcmp0(method_name, "PollCommands") == 0) {
        gsize count = 0;
        gchar **commands = app_take_commands(app, &count);

        g_dbus_method_invocation_return_value(
            invocation, g_variant_new("(^as)", commands, count));
        g_strfreev(commands);
        return;
    }

    g_dbus_method_invocation_return_dbus_error(invocation,
                                              "org.comebackwindow.Error.UnknownMethod",
                                              "Bilinmeyen metot");
}

gboolean dbus_service_register(App *app, GDBusConnection *bus, GError **error)
{
    g_return_val_if_fail(app != NULL, FALSE);
    g_return_val_if_fail(G_IS_DBUS_CONNECTION(bus), FALSE);

    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(introspection_xml, error);

    if (info == NULL) {
        return FALSE;
    }

    app->bus = g_object_ref(bus);
    app->registration_id = g_dbus_connection_register_object(
        bus, CBW_OBJECT_PATH, info->interfaces[0],
        &interface_vtable, app, NULL, error);
    g_dbus_node_info_unref(info);

    return app->registration_id != 0;
}

void dbus_service_unregister(App *app)
{
    if (app == NULL) {
        return;
    }
    if (app->registration_id != 0) {
        if (app->bus != NULL) {
            g_dbus_connection_unregister_object(app->bus, app->registration_id);
        }
        app->registration_id = 0;
        g_clear_object(&app->bus);
    } else {
        app->bus = NULL;
    }
}
