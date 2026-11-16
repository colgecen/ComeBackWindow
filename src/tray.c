/*
 * StatusNotifierItem tepsi simgesi
 *
 * Menü, `com.canonical.dbusmenu` sunucusu ile ayrı bir nesnede yayınlanır
 * (bkz. tray_menu.c). Simge, kullanicinin sagladigi assets/icon.png
 * dosyasindan 22 px'e olceklenerek verilir.
 */
#include "i18n.h"
#include "tray.h"

#include "dialogs.h"
#include "tray_menu.h"
#include "version.h"

#include <cairo.h>
#include <unistd.h>

#define SNI_IFACE "org.kde.StatusNotifierItem"
#define SNI_PATH "/StatusNotifierItem"
#define WATCHER_SERVICE "org.kde.StatusNotifierWatcher"
#define WATCHER_PATH "/StatusNotifierWatcher"
#define WATCHER_IFACE "org.kde.StatusNotifierWatcher"
#define MENU_PATH "/MenuBar"

struct _Tray {
    App *app;
    gchar *service_name;
    guint owner_id;
    guint registration_id;
    gboolean registered;
    gchar *status;
    GVariant *pixmap;
};

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='org.kde.StatusNotifierItem'>"
    "    <method name='ContextMenu'>"
    "      <arg type='i' name='x' direction='in'/>"
    "      <arg type='i' name='y' direction='in'/>"
    "    </method>"
    "    <method name='Activate'>"
    "      <arg type='i' name='x' direction='in'/>"
    "      <arg type='i' name='y' direction='in'/>"
    "    </method>"
    "    <method name='SecondaryActivate'>"
    "      <arg type='i' name='x' direction='in'/>"
    "      <arg type='i' name='y' direction='in'/>"
    "    </method>"
    "    <method name='Scroll'>"
    "      <arg type='i' name='delta' direction='in'/>"
    "      <arg type='s' name='orientation' direction='in'/>"
    "    </method>"
    "    <property name='Category' type='s' access='read'/>"
    "    <property name='Id' type='s' access='read'/>"
    "    <property name='Title' type='s' access='read'/>"
    "    <property name='Status' type='s' access='read'/>"
    "    <property name='WindowId' type='u' access='read'/>"
    "    <property name='IconName' type='s' access='read'/>"
    "    <property name='IconPixmap' type='a(iiay)' access='read'/>"
    "    <property name='OverlayIconName' type='s' access='read'/>"
    "    <property name='OverlayIconPixmap' type='a(iiay)' access='read'/>"
    "    <property name='AttentionIconName' type='s' access='read'/>"
    "    <property name='AttentionIconPixmap' type='a(iiay)' access='read'/>"
    "    <property name='AttentionMovieName' type='s' access='read'/>"
    "    <property name='ToolTip' type='(sa(iiay)ss)' access='read'/>"
    "    <property name='ItemIsMenu' type='b' access='read'/>"
    "    <property name='Menu' type='o' access='read'/>"
    "    <signal name='NewTitle'/>"
    "    <signal name='NewIcon'/>"
    "    <signal name='NewAttentionIcon'/>"
    "    <signal name='NewOverlayIcon'/>"
    "    <signal name='NewToolTip'/>"
    "    <signal name='NewStatus'>"
    "      <arg type='s' name='status'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

/* --- Simge çizimi -------------------------------------------------- */

typedef struct {
    guchar b, g, r, a;
} Pixel;

static GVariant *empty_pixmap(void);

/* Kullanicinin sagladigi assets/icon.png dosyasini yukler (kurulu veya depo). */
static cairo_surface_t *load_icon_surface(void)
{
    gchar *data = app_data_dir();
    gchar *repo = g_build_filename(data, "assets", "icon.png", NULL);
    gchar *installed = g_build_filename(CBW_DATADIR, "icons", "hicolor",
                                        "512x512", "apps",
                                        "comebackwindow.png", NULL);
    const gchar *appdir = g_getenv("APPDIR");
    gchar *in_image = appdir != NULL && *appdir != '\0'
        ? g_build_filename(appdir, "comebackwindow.png", NULL) : NULL;
    cairo_surface_t *surface = NULL;

    if (g_file_test(repo, G_FILE_TEST_IS_REGULAR)) {
        surface = cairo_image_surface_create_from_png(repo);
    }
    if ((surface == NULL
         || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
        && in_image != NULL && g_file_test(in_image, G_FILE_TEST_IS_REGULAR)) {
        if (surface != NULL) {
            cairo_surface_destroy(surface);
        }
        surface = cairo_image_surface_create_from_png(in_image);
    }
    if ((surface == NULL
         || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
        && g_file_test(installed, G_FILE_TEST_IS_REGULAR)) {
        if (surface != NULL) {
            cairo_surface_destroy(surface);
        }
        surface = cairo_image_surface_create_from_png(installed);
    }
    g_free(repo);
    g_free(installed);
    g_free(in_image);
    g_free(data);
    if (surface != NULL
        && cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return NULL;
    }
    return surface;
}

static GVariant *build_pixmap(void)
{
    const gint size = 22;
    cairo_surface_t *icon = load_icon_surface();

    if (icon == NULL) {
        return empty_pixmap();
    }
    gint iw = cairo_image_surface_get_width(icon);
    gint ih = cairo_image_surface_get_height(icon);

    if (iw <= 0 || ih <= 0) {
        cairo_surface_destroy(icon);
        return empty_pixmap();
    }
    cairo_surface_t *surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(surface);
    gdouble scale = (gdouble)size / MAX(iw, ih);

    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, icon,
                             (size / scale - iw) / 2.0,
                             (size / scale - ih) / 2.0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(icon);
    cairo_surface_flush(surface);

    guchar *data = cairo_image_surface_get_data(surface);
    GVariantBuilder bytes;

    g_variant_builder_init(&bytes, G_VARIANT_TYPE("ay"));
    for (gint y = 0; y < size; y++) {
        for (gint x = 0; x < size; x++) {
            guchar *px = data + y * cairo_image_surface_get_stride(surface)
                + x * 4;

            /* ARGB32 little-endian: B,G,R,A */
            g_variant_builder_add(&bytes, "y", px[3]);
            g_variant_builder_add(&bytes, "y", px[2]);
            g_variant_builder_add(&bytes, "y", px[1]);
            g_variant_builder_add(&bytes, "y", px[0]);
        }
    }
    cairo_surface_destroy(surface);
    GVariantBuilder list;

    g_variant_builder_init(&list, G_VARIANT_TYPE("a(iiay)"));
    g_variant_builder_add(&list, "(ii@ay)", size, size,
                          g_variant_builder_end(&bytes));
    return g_variant_builder_end(&list);
}

/* --- Özellikler --------------------------------------------------- */

static GVariant *empty_pixmap(void)
{
    GVariantBuilder list;

    g_variant_builder_init(&list, G_VARIANT_TYPE("a(iiay)"));
    return g_variant_builder_end(&list);
}

static GVariant *tooltip(Tray *tray)
{
    const gchar *detail = tray->status != NULL ? tray->status
        : _("Masaüstü kısayolları");

    return g_variant_new("(s@a(iiay)ss)", "", empty_pixmap(),
                         "Come Back Window", detail);
}

static GVariant *tray_get_property(GDBusConnection *connection,
                                   const gchar *sender,
                                   const gchar *object_path,
                                   const gchar *interface_name,
                                   const gchar *property_name,
                                   GError **error, gpointer user_data)
{
    Tray *tray = user_data;

    (void)connection;
    (void)sender;
    (void)object_path;
    (void)interface_name;

    if (g_strcmp0(property_name, "Category") == 0) {
        return g_variant_new_string("ApplicationStatus");
    }
    if (g_strcmp0(property_name, "Id") == 0) {
        return g_variant_new_string("comebackwindow");
    }
    if (g_strcmp0(property_name, "Title") == 0) {
        return g_variant_new_string("Come Back Window");
    }
    if (g_strcmp0(property_name, "Status") == 0) {
        return g_variant_new_string("Active");
    }
    if (g_strcmp0(property_name, "WindowId") == 0) {
        return g_variant_new_uint32(0);
    }
    if (g_strcmp0(property_name, "IconName") == 0) {
        return g_variant_new_string("comebackwindow");
    }
    if (g_strcmp0(property_name, "IconPixmap") == 0) {
        return g_variant_ref_sink(g_variant_ref(tray->pixmap));
    }
    if (g_strcmp0(property_name, "OverlayIconName") == 0
        || g_strcmp0(property_name, "AttentionIconName") == 0
        || g_strcmp0(property_name, "AttentionMovieName") == 0) {
        return g_variant_new_string("");
    }
    if (g_strcmp0(property_name, "OverlayIconPixmap") == 0
        || g_strcmp0(property_name, "AttentionIconPixmap") == 0) {
        return empty_pixmap();
    }
    if (g_strcmp0(property_name, "ToolTip") == 0) {
        return tooltip(tray);
    }
    if (g_strcmp0(property_name, "ItemIsMenu") == 0) {
        return g_variant_new_boolean(TRUE);
    }
    if (g_strcmp0(property_name, "Menu") == 0) {
        return g_variant_new_object_path(MENU_PATH);
    }
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                "Bilinmeyen özellik: %s", property_name);
    return NULL;
}

/* --- Metotlar ----------------------------------------------------- */

static void tray_method_call(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *method_name, GVariant *parameters,
                             GDBusMethodInvocation *invocation,
                             gpointer user_data)
{
    Tray *tray = user_data;

    (void)connection;
    (void)sender;
    (void)object_path;
    (void)interface_name;

    if (g_strcmp0(method_name, "Activate") == 0) {
        g_main_loop_quit(tray->app->loop);
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method_name, "ContextMenu") == 0) {
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method_name, "SecondaryActivate") == 0) {
        dialogs_open_settings(tray->app);
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method_name, "Scroll") == 0) {
        app_send_command(tray->app, "toggle-desktop");
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else {
        g_dbus_method_invocation_return_dbus_error(
            invocation, "org.comebackwindow.Error.UnknownMethod",
            "Bilinmeyen metot");
    }
    (void)parameters;
}

static const GDBusInterfaceVTable tray_vtable = {
    .method_call = tray_method_call,
    .get_property = tray_get_property
};

/* --- Kayıt -------------------------------------------------------- */

static void register_with_watcher(Tray *tray)
{
    if (tray->app->bus == NULL) {
        return;
    }
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_sync(
        tray->app->bus, WATCHER_SERVICE, WATCHER_PATH, WATCHER_IFACE,
        "RegisterStatusNotifierItem",
        g_variant_new("(s)", tray->service_name), NULL,
        G_DBUS_CALL_FLAGS_NONE, 3000, NULL, &error);

    if (reply == NULL) {
        app_warn(tray->app, "tepsi kaydı başarısız: %s",
                 error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
        return;
    }
    g_variant_unref(reply);
    tray->registered = TRUE;
    app_log(tray->app, "tepsi simgesi kaydedildi: %s", tray->service_name);
}

static void on_name_acquired(GDBusConnection *connection, const gchar *name,
                             gpointer user_data)
{
    Tray *tray = user_data;

    (void)connection;
    (void)name;
    register_with_watcher(tray);
}

static void on_name_lost(GDBusConnection *connection, const gchar *name,
                         gpointer user_data)
{
    Tray *tray = user_data;

    (void)connection;
    (void)name;
    app_warn(tray->app, "tepsi adı alınamadı; tepsi devre dışı");
    tray->registered = FALSE;
}

Tray *tray_create(App *app, GError **error)
{
    if (app->bus == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED,
                            "D-Bus bağlantısı yok");
        return NULL;
    }
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(introspection_xml,
                                                       error);

    if (info == NULL) {
        return NULL;
    }
    Tray *tray = g_new0(Tray, 1);

    tray->app = app;
    tray->pixmap = build_pixmap();
    tray->service_name = g_strdup_printf(
        "org.kde.StatusNotifierItem-%d-1", (gint)getpid());
    tray->registration_id = g_dbus_connection_register_object(
        app->bus, SNI_PATH, info->interfaces[0], &tray_vtable, tray, NULL,
        error);
    g_dbus_node_info_unref(info);
    if (tray->registration_id == 0) {
        g_free(tray->service_name);
        g_variant_unref(tray->pixmap);
        g_free(tray);
        return NULL;
    }
    if (!tray_menu_register(app, app->bus, error)) {
        g_dbus_connection_unregister_object(app->bus, tray->registration_id);
        g_free(tray->service_name);
        g_variant_unref(tray->pixmap);
        g_free(tray);
        return NULL;
    }
    tray->owner_id = g_bus_own_name_on_connection(
        app->bus, tray->service_name, G_BUS_NAME_OWNER_FLAGS_NONE,
        on_name_acquired, on_name_lost, tray, NULL);
    app->tray = tray;
    return tray;
}

void tray_destroy(Tray *tray)
{
    if (tray == NULL) {
        return;
    }
    if (tray->owner_id != 0) {
        g_bus_unown_name(tray->owner_id);
        tray->owner_id = 0;
    }
    if (tray->registration_id != 0 && tray->app->bus != NULL) {
        g_dbus_connection_unregister_object(tray->app->bus,
                                            tray->registration_id);
        tray->registration_id = 0;
    }
    tray_menu_unregister(tray->app);
    g_free(tray->status);
    g_free(tray->service_name);
    if (tray->pixmap != NULL) {
        g_variant_unref(tray->pixmap);
    }
    if (tray->app->tray == tray) {
        tray->app->tray = NULL;
    }
    g_free(tray);
}

void tray_update_status(Tray *tray)
{
    if (tray == NULL || tray->app->bus == NULL) {
        return;
    }
    g_free(tray->status);
    tray->status = g_strdup(tray->app->desktop_shown
                                ? _("Masaüstü gösteriliyor")
                                : _("Masaüstü kısayolları"));
    GError *error = NULL;

    g_dbus_connection_emit_signal(tray->app->bus, NULL, SNI_PATH, SNI_IFACE,
                                  "NewToolTip", NULL, &error);
    if (error != NULL) {
        g_clear_error(&error);
    }
}

void tray_activate_id(Tray *tray, gint id)
{
    if (tray != NULL) {
        tray_menu_activate(tray->app, id);
    }
}
