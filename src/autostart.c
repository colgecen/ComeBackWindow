/*
 * Autostart (.desktop) yönetimi
 *
 * ~/.config/autostart/org.comebackwindow.App.desktop dosyası yazılır/silinir.
 */
#include "autostart.h"

#include <errno.h>
#include <glib/gstdio.h>

#define AUTOSTART_FILE "org.comebackwindow.App.desktop"

/* Masaüstü Exec alanı için çift tırnaklı kaçış. */
static gchar *desktop_quote(const gchar *text)
{
    GString *out = g_string_new("\"");

    for (const gchar *p = text; p != NULL && *p != '\0'; p++) {
        if (*p == '"' || *p == '\\' || *p == '`' || *p == '$') {
            g_string_append_c(out, '\\');
        }
        g_string_append_c(out, *p);
    }
    g_string_append_c(out, '"');
    return g_string_free(out, FALSE);
}

/*
 * Autostart, calistirilabilir dosyanin gercek yolunu kullanir. AppImage
 * icinde /proc/self/exe gecici baglama noktasini gosterir; bu yuzden
 * APPIMAGE ortam degiskeni tercih edilir.
 */
static gchar *autostart_executable(void)
{
    const gchar *image = g_getenv("APPIMAGE");

    if (image != NULL && *image != '\0') {
        return g_strdup(image);
    }
    GError *error = NULL;
    gchar *exe = g_file_read_link("/proc/self/exe", &error);

    if (exe == NULL) {
        g_clear_error(&error);
        return g_strdup("comebackwindow");
    }
    return exe;
}

static gchar *desktop_contents(void)
{
    gchar *exe = autostart_executable();
    gchar *quoted = desktop_quote(exe);
    gchar *contents = g_strdup_printf(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Come Back Window\n"
        "Exec=env CBW_AUTOSTART=1 %s\n"
        "Icon=comebackwindow\n"
        "X-KDE-autostart-after=panel\n"
        "X-KDE-autostart-phase=2\n"
        "X-GNOME-Autostart-enabled=true\n"
        "NoDisplay=true\n",
        quoted);

    g_free(exe);
    g_free(quoted);
    return contents;
}

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
    gchar *contents = desktop_contents();
    gboolean ok = g_file_set_contents(path, contents, -1, error);

    g_free(contents);
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
