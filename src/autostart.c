/*
 * Autostart (.desktop) yönetimi
 *
 * ~/.config/autostart/org.comebackwindow.App.desktop dosyası yazılır/silinir.
 */
#include "autostart.h"

#include <errno.h>
#include <glib/gstdio.h>

#define AUTOSTART_FILE "org.comebackwindow.App.desktop"

static const gchar desktop_contents[] =
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=Come Back Window\n"
    "Exec=comebackwindow\n"
    "Icon=comebackwindow\n"
    "X-KDE-autostart-after=panel\n"
    "X-KDE-autostart-phase=2\n"
    "X-GNOME-Autostart-enabled=true\n"
    "NoDisplay=true\n";

gchar *autostart_dir(void)
{
    const gchar *override = g_getenv("CBW_AUTOSTART_DIR");

    if (override != NULL && *override != '\0') {
        return g_strdup(override);
    }
    return g_build_filename(g_get_user_config_dir(), "autostart", NULL);
}

gchar *autostart_path(void)
{
    gchar *dir = autostart_dir();
    gchar *path = g_build_filename(dir, AUTOSTART_FILE, NULL);

    g_free(dir);
    return path;
}

gboolean autostart_enable(GError **error)
{
    gchar *dir = autostart_dir();

    if (g_mkdir_with_parents(dir, 0700) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Autostart dizini oluşturulamadı: %s", dir);
        g_free(dir);
        return FALSE;
    }
    g_free(dir);

    gchar *path = autostart_path();
    gboolean ok = g_file_set_contents(path, desktop_contents,
                                      (gssize)sizeof(desktop_contents) - 1,
                                      error);
    g_free(path);
    return ok;
}

gboolean autostart_disable(GError **error)
{
    gchar *path = autostart_path();

    if (g_unlink(path) != 0 && errno != ENOENT) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Autostart dosyası silinemedi: %s", path);
        g_free(path);
        return FALSE;
    }
    g_free(path);
    return TRUE;
}

gboolean autostart_is_enabled(void)
{
    gchar *path = autostart_path();
    gboolean exists = g_file_test(path, G_FILE_TEST_IS_REGULAR);

    g_free(path);
    return exists;
}
