/*
 * Tepsi menüsü — com.canonical.dbusmenu sunucusu
 *
 * Menü modeli sabittir (kimlik, etiket, eylem). `tray_menu_layout` saf bir
 * işlevdir ve birim testleriyle doğrulanır; D-Bus tarafı yalnız bu modeli
 * yayınlar ve gelen `Event` çağrılarını eylemlere çevirir.
 */
#include "tray_menu.h"

#include "dialogs.h"
#include "version.h"

#define MENU_PATH "/MenuBar"
#define MENU_IFACE "com.canonical.dbusmenu"

enum {
    MENU_ID_DESKTOP = 1,
    MENU_ID_MINIMIZE_OTHERS = 2,
    MENU_ID_RESTORE_ALL = 3,
    MENU_ID_SEPARATOR = 4,
    MENU_ID_SETTINGS = 5,
    MENU_ID_SETUP = 6,
    MENU_ID_ABOUT = 7,
    MENU_ID_QUIT = 8
};

static const MenuItemSpec items[] = {
    { MENU_ID_DESKTOP, MENU_ITEM_NORMAL, "Masaüstünü Göster",
      "toggle-desktop" },
    { MENU_ID_MINIMIZE_OTHERS, MENU_ITEM_NORMAL, "Diğerlerini Küçült",
      "minimize-others" },
    { MENU_ID_RESTORE_ALL, MENU_ITEM_NORMAL, "Küçültülenleri Geri Getir",
      "restore-all" },
    { MENU_ID_SEPARATOR, MENU_ITEM_SEPARATOR, NULL, NULL },
    { MENU_ID_SETTINGS, MENU_ITEM_NORMAL, "Ayarlar…", "settings" },
    { MENU_ID_SETUP, MENU_ITEM_NORMAL, "İlk Açılış Sihirbazı…", "setup" },
    { MENU_ID_ABOUT, MENU_ITEM_NORMAL, "Hakkında…", "about" },
    { MENU_ID_QUIT, MENU_ITEM_NORMAL, "Çıkış", "quit" }
};

const MenuItemSpec *tray_menu_items(gsize *count)
{
    if (count != NULL) {
        *count = G_N_ELEMENTS(items);
    }
    return items;
}

const gchar *tray_menu_action(gint id)
{
    for (gsize i = 0; i < G_N_ELEMENTS(items); i++) {
        if (items[i].id == id) {
            return items[i].action;
        }
    }
    return NULL;
}

static GVariant *item_properties(const MenuItemSpec *item)
{
    GVariantBuilder props;

    g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
    if (item->type == MENU_ITEM_SEPARATOR) {
        g_variant_builder_add(&props, "{sv}", "type", g_variant_new_string("separator"));
        return g_variant_builder_end(&props);
    }
    g_variant_builder_add(&props, "{sv}", "label",
                          g_variant_new_string(item->label != NULL
                                                   ? item->label : ""));
    g_variant_builder_add(&props, "{sv}", "enabled",
                          g_variant_new_boolean(TRUE));
    g_variant_builder_add(&props, "{sv}", "visible",
                          g_variant_new_boolean(TRUE));
    return g_variant_builder_end(&props);
}

static GVariant *item_variant(const MenuItemSpec *item)
{
    GVariantBuilder node;

    g_variant_builder_init(&node, G_VARIANT_TYPE("(ia{sv}av)"));
    g_variant_builder_add(&node, "i", item->id);
    g_variant_builder_add_value(&node, item_properties(item));
    GVariantBuilder children;

    g_variant_builder_init(&children, G_VARIANT_TYPE("av"));
    g_variant_builder_add_value(&node, g_variant_builder_end(&children));
    return g_variant_builder_end(&node);
}

GVariant *tray_menu_layout(const MenuItemSpec *list, gsize count,
                           guint revision, gint parent_id, gint depth)
{
    GVariantBuilder children;
    GVariantBuilder props;

    g_variant_builder_init(&children, G_VARIANT_TYPE("av"));
    g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
    if (parent_id == 0 && depth != 0) {
        for (gsize i = 0; i < count; i++) {
            g_variant_builder_add_value(
                &children, g_variant_new_variant(item_variant(&list[i])));
        }
    }
    GVariant *node = g_variant_new("(ia{sv}av)", 0, &props, &children);

    return g_variant_new("(u@(ia{sv}av))", revision, node);
}

/* --- D-Bus -------------------------------------------------------- */

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='com.canonical.dbusmenu'>"
    "    <method name='GetLayout'>"
    "      <arg type='i' name='parentId' direction='in'/>"
    "      <arg type='i' name='recursionDepth' direction='in'/>"
    "      <arg type='as' name='propertyNames' direction='in'/>"
    "      <arg type='u' name='revision' direction='out'/>"
    "      <arg type='(ia{sv}av)' name='layout' direction='out'/>"
    "    </method>"
    "    <method name='GetGroupProperties'>"
    "      <arg type='ai' name='ids' direction='in'/>"
    "      <arg type='as' name='propertyNames' direction='in'/>"
    "      <arg type='a(ia{sv})' name='properties' direction='out'/>"
    "    </method>"
    "    <method name='GetProperty'>"
    "      <arg type='i' name='id' direction='in'/>"
    "      <arg type='s' name='name' direction='in'/>"
    "      <arg type='v' name='value' direction='out'/>"
    "    </method>"
    "    <method name='Event'>"
    "      <arg type='i' name='id' direction='in'/>"
    "      <arg type='s' name='eventId' direction='in'/>"
    "      <arg type='v' name='data' direction='in'/>"
    "      <arg type='u' name='timestamp' direction='in'/>"
    "    </method>"
    "    <method name='EventGroup'>"
    "      <arg type='a(isvu)' name='events' direction='in'/>"
    "      <arg type='ai' name='idErrors' direction='out'/>"
    "    </method>"
    "    <method name='AboutToShow'>"
    "      <arg type='i' name='id' direction='in'/>"
    "      <arg type='b' name='needUpdate' direction='out'/>"
    "    </method>"
    "    <method name='AboutToShowGroup'>"
    "      <arg type='ai' name='ids' direction='in'/>"
    "      <arg type='ai' name='updatesNeeded' direction='out'/>"
    "      <arg type='ai' name='idErrors' direction='out'/>"
    "    </method>"
    "    <property name='Version' type='u' access='read'/>"
    "    <property name='TextDirection' type='s' access='read'/>"
    "    <property name='Status' type='s' access='read'/>"
    "    <property name='IconThemePath' type='as' access='read'/>"
    "    <signal name='ItemsPropertiesUpdated'>"
    "      <arg type='a(ia{sv})' name='updatedProps'/>"
    "      <arg type='a(ias)' name='removedProps'/>"
    "    </signal>"
    "    <signal name='LayoutUpdated'>"
    "      <arg type='u' name='revision'/>"
    "      <arg type='i' name='parent'/>"
    "    </signal>"
    "    <signal name='ItemActivationRequested'>"
    "      <arg type='i' name='id'/>"
    "      <arg type='u' name='timestamp'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

typedef struct {
    App *app;
    guint registration_id;
    guint revision;
} TrayMenu;

static TrayMenu *menu_for(App *app)
{
    return app->tray_menu;
}

static GVariant *menu_get_property(GDBusConnection *connection,
                                   const gchar *sender,
                                   const gchar *object_path,
                                   const gchar *interface_name,
                                   const gchar *property_name,
                                   GError **error,
                                   gpointer user_data)
{
    (void)connection;
    (void)sender;
    (void)object_path;
    (void)interface_name;
    (void)user_data;

    if (g_strcmp0(property_name, "Version") == 0) {
        return g_variant_new_uint32(3);
    }
    if (g_strcmp0(property_name, "TextDirection") == 0) {
        return g_variant_new_string("ltr");
    }
    if (g_strcmp0(property_name, "Status") == 0) {
        return g_variant_new_string("normal");
    }
    if (g_strcmp0(property_name, "IconThemePath") == 0) {
        return g_variant_new_strv(NULL, 0);
    }
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                "Bilinmeyen özellik: %s", property_name);
    return NULL;
}

static void method_get_layout(App *app, GVariant *parameters,
                              GDBusMethodInvocation *invocation)
{
    gint parent = 0;
    gint depth = -1;
    gchar **names = NULL;

    g_variant_get(parameters, "(ii^as)", &parent, &depth, &names);
    g_strfreev(names);
    TrayMenu *menu = menu_for(app);
    guint revision = menu != NULL ? menu->revision : 1;

    if (menu != NULL) {
        menu->revision++;
    }
    GVariant *reply = tray_menu_layout(items, G_N_ELEMENTS(items), revision,
                                       parent, depth);

    g_dbus_method_invocation_return_value(invocation, reply);
}

static void method_get_group_properties(GVariant *parameters,
                                        GDBusMethodInvocation *invocation)
{
    GVariant *ids = NULL;
    gchar **names = NULL;

    g_variant_get(parameters, "(@ai^as)", &ids, &names);
    g_strfreev(names);
    GVariantBuilder out;

    g_variant_builder_init(&out, G_VARIANT_TYPE("a(ia{sv})"));
    gsize n = g_variant_n_children(ids);

    for (gsize i = 0; i < n; i++) {
        gint id = 0;

        g_variant_get_child(ids, i, "i", &id);
        for (gsize k = 0; k < G_N_ELEMENTS(items); k++) {
            if (items[k].id == id) {
                g_variant_builder_add(&out, "(i@a{sv})", id,
                                      item_properties(&items[k]));
                break;
            }
        }
    }
    g_variant_unref(ids);
    g_dbus_method_invocation_return_value(
        invocation, g_variant_new("(a(ia{sv}))", &out));
}

static void method_event(App *app, GVariant *parameters,
                         GDBusMethodInvocation *invocation)
{
    gint id = 0;
    const gchar *event = NULL;

    g_variant_get(parameters, "(i&s@vu)", &id, &event, NULL, NULL);
    if (g_strcmp0(event, "clicked") == 0) {
        tray_menu_activate(app, id);
    }
    g_dbus_method_invocation_return_value(invocation, NULL);
}

static void method_event_group(App *app, GVariant *parameters,
                               GDBusMethodInvocation *invocation)
{
    GVariantIter *iter = NULL;

    g_variant_get(parameters, "(a(isvu))", &iter);
    gint id = 0;
    const gchar *event = NULL;
    GVariant *data = NULL;
    guint timestamp = 0;

    while (g_variant_iter_loop(iter, "(i&svu)", &id, &event, &data,
                               &timestamp)) {
        (void)data;
        (void)timestamp;
        if (g_strcmp0(event, "clicked") == 0) {
            tray_menu_activate(app, id);
        }
    }
    g_variant_iter_free(iter);
    g_dbus_method_invocation_return_value(
        invocation, g_variant_new("(ai)", NULL));
}

static void menu_method_call(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *method_name, GVariant *parameters,
                             GDBusMethodInvocation *invocation,
                             gpointer user_data)
{
    App *app = user_data;

    (void)connection;
    (void)sender;
    (void)object_path;
    (void)interface_name;

    if (g_strcmp0(method_name, "GetLayout") == 0) {
        method_get_layout(app, parameters, invocation);
    } else if (g_strcmp0(method_name, "GetGroupProperties") == 0) {
        method_get_group_properties(parameters, invocation);
    } else if (g_strcmp0(method_name, "GetProperty") == 0) {
        gint id = 0;

        g_variant_get(parameters, "(i&s)", &id, NULL);
        const gchar *action = tray_menu_action(id);

        (void)action;
        g_dbus_method_invocation_return_value(
            invocation, g_variant_new("(v)", g_variant_new_boolean(TRUE)));
    } else if (g_strcmp0(method_name, "Event") == 0) {
        method_event(app, parameters, invocation);
    } else if (g_strcmp0(method_name, "EventGroup") == 0) {
        method_event_group(app, parameters, invocation);
    } else if (g_strcmp0(method_name, "AboutToShow") == 0) {
        g_dbus_method_invocation_return_value(
            invocation, g_variant_new("(b)", FALSE));
    } else if (g_strcmp0(method_name, "AboutToShowGroup") == 0) {
        g_dbus_method_invocation_return_value(
            invocation, g_variant_new("(aiai)", NULL, NULL));
    } else {
        g_dbus_method_invocation_return_dbus_error(
            invocation, "org.comebackwindow.Error.UnknownMethod",
            "Bilinmeyen metot");
    }
}

static const GDBusInterfaceVTable menu_vtable = {
    .method_call = menu_method_call,
    .get_property = menu_get_property
};

gboolean tray_menu_register(App *app, GDBusConnection *bus, GError **error)
{
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(introspection_xml,
                                                       error);

    if (info == NULL) {
        return FALSE;
    }
    TrayMenu *menu = g_new0(TrayMenu, 1);

    menu->app = app;
    menu->revision = 1;
    menu->registration_id = g_dbus_connection_register_object(
        bus, MENU_PATH, info->interfaces[0], &menu_vtable, app, NULL, error);
    g_dbus_node_info_unref(info);
    if (menu->registration_id == 0) {
        g_free(menu);
        return FALSE;
    }
    app->tray_menu = menu;
    return TRUE;
}

void tray_menu_unregister(App *app)
{
    TrayMenu *menu = app->tray_menu;

    if (menu == NULL) {
        return;
    }
    if (menu->registration_id != 0 && app->bus != NULL) {
        g_dbus_connection_unregister_object(app->bus, menu->registration_id);
    }
    g_free(menu);
    app->tray_menu = NULL;
}

void tray_menu_activate(App *app, gint id)
{
    const gchar *action = NULL;

    for (gsize i = 0; i < G_N_ELEMENTS(items); i++) {
        if (items[i].id == id) {
            action = items[i].action;
            break;
        }
    }
    if (action == NULL) {
        return;
    }
    if (g_strcmp0(action, "settings") == 0) {
        app->open_settings = TRUE;
        dialogs_open_settings(app);
        return;
    }
    if (g_strcmp0(action, "setup") == 0) {
        app->open_setup = TRUE;
        dialogs_open_setup(app);
        return;
    }
    if (g_strcmp0(action, "about") == 0) {
        app_notify(app, "Come Back Window",
                   "Sürüm " CBW_VERSION " — Windows tarzı pencere kısayolları");
        return;
    }
    if (g_strcmp0(action, "quit") == 0) {
        if (app->loop != NULL) {
            g_main_loop_quit(app->loop);
        }
        return;
    }
    app_send_command(app, action);
}
