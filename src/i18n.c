/*
 * Çeviri altyapısı — gettext bağlama
 */
#include "i18n.h"

#include <libintl.h>

void i18n_init(const gchar *language)
{
    if (language != NULL && *language != '\0'
        && g_strcmp0(language, "auto") != 0) {
        g_setenv("LANGUAGE", language, TRUE);
    }
    gchar *dir = g_build_filename(CBW_DATADIR, "locale", NULL);

    bindtextdomain(GETTEXT_PACKAGE, dir);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
    g_free(dir);
}
