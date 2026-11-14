/* Komut satırı ayrıştırma */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    CBW_MODE_RUN = 0,
    CBW_MODE_SETUP,
    CBW_MODE_SETTINGS,
    CBW_MODE_TOGGLE_DESKTOP,
    CBW_MODE_MINIMIZE_OTHERS,
    CBW_MODE_RESTORE_ALL,
    CBW_MODE_RESTORE_SHORTCUTS,
    CBW_MODE_UNINSTALL_CLEANUP,
    CBW_MODE_DUMP_STATE
} CliMode;

typedef struct {
    CliMode mode;
    gboolean no_tray;
    gboolean no_strip;
    gboolean debug;
    gboolean version;
    gboolean help;
} CliOptions;

void cli_options_init(CliOptions *opts);

/* Argümanları ayrıştırır. Bilinmeyen bayrakta FALSE ve GError döner. */
gboolean cli_parse(int argc, char **argv, CliOptions *opts, GError **error);

/* Yardım metni. */
const gchar *cli_usage(void);

/* Mod, çalışan örneğe yönlendirilecek bir istemci komutu mu? */
gboolean cli_mode_is_client(CliMode mode);

/* Mod için D-Bus metodu adı (istemci komutları). */
const gchar *cli_mode_method(CliMode mode);

G_END_DECLS
