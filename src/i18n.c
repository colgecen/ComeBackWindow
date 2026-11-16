/*
 * Çeviri altyapısı — gettext bağlama
 */
#include "i18n.h"

#include <libintl.h>

/* Kurulu katalog yoksa depodaki `po/` dizinini kullan (geliştirme). */
static gchar *locale_dir(void)
{
    const gchar *override = g_getenv("CBW_LOCALE_DIR");

    if (override != NULL && *override != '\0') {
        return g_strdup(override);
    }
    const gchar *appdir = g_getenv("APPDIR");

    if (appdir != NULL && *appdir != '\0') {
        gchar *in_image = g_build_filename(appdir, "usr", "share", "locale",
                                           NULL);
        gchar *catalog = g_build_filename(in_image, "en", "LC_MESSAGES",
                                          "comebackwindow.mo", NULL);

        if (g_file_test(catalog, G_FILE_TEST_IS_REGULAR)) {
            g_free(catalog);
            return in_image;
        }
        g_free(catalog);
        g_free(in_image);
    }
    GError *error = NULL;
    gchar *exe = g_file_read_link("/proc/self/exe", &error);

    if (exe == NULL) {
        g_clear_error(&error);
        return g_build_filename(CBW_DATADIR, "locale", NULL);
    }
    gchar *exe_dir = g_path_get_dirname(exe);
    gchar *root = g_path_get_dirname(exe_dir);
    gchar *local = g_build_filename(root, "build", "locale", NULL);
    gchar *catalog = g_build_filename(local, "en", "LC_MESSAGES",
                                      "comebackwindow.mo", NULL);
    gboolean has_catalog = g_file_test(catalog, G_FILE_TEST_IS_REGULAR);

    g_free(catalog);
    g_free(exe);
    g_free(exe_dir);
    g_free(root);
    if (has_catalog) {
        return local;
    }
    g_free(local);
    return g_build_filename(CBW_DATADIR, "locale", NULL);
}

void i18n_init(const gchar *language)
{
    if (language != NULL && *language != '\0'
        && g_strcmp0(language, "auto") != 0) {
        g_setenv("LANGUAGE", language, TRUE);
    }
    gchar *dir = locale_dir();

    bindtextdomain(GETTEXT_PACKAGE, dir);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
    g_free(dir);
}
