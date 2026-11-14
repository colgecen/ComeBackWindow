/*
 * Komut satırı ayrıştırma
 */
#include "cli.h"

void cli_options_init(CliOptions *opts)
{
    g_return_if_fail(opts != NULL);

    opts->mode = CBW_MODE_RUN;
    opts->no_tray = FALSE;
    opts->no_strip = FALSE;
    opts->debug = FALSE;
    opts->version = FALSE;
    opts->help = FALSE;
}

typedef struct {
    const gchar *flag;
    CliMode mode;
    gboolean client_only;
} ModeFlag;

static const ModeFlag mode_flags[] = {
    { "--setup", CBW_MODE_SETUP, FALSE },
    { "--settings", CBW_MODE_SETTINGS, TRUE },
    { "--toggle-desktop", CBW_MODE_TOGGLE_DESKTOP, TRUE },
    { "--minimize-others", CBW_MODE_MINIMIZE_OTHERS, TRUE },
    { "--restore-all", CBW_MODE_RESTORE_ALL, TRUE },
    { "--restore-shortcuts", CBW_MODE_RESTORE_SHORTCUTS, FALSE },
    { "--uninstall-cleanup", CBW_MODE_UNINSTALL_CLEANUP, FALSE },
    { "--dump-state", CBW_MODE_DUMP_STATE, TRUE }
};

gboolean cli_parse(int argc, char **argv, CliOptions *opts, GError **error)
{
    g_return_val_if_fail(opts != NULL, FALSE);

    for (int i = 1; i < argc; i++) {
        const gchar *arg = argv[i];
        gboolean handled = FALSE;

        if (g_strcmp0(arg, "--help") == 0 || g_strcmp0(arg, "-h") == 0) {
            opts->help = TRUE;
            handled = TRUE;
        } else if (g_strcmp0(arg, "--version") == 0) {
            opts->version = TRUE;
            handled = TRUE;
        } else if (g_strcmp0(arg, "--no-tray") == 0) {
            opts->no_tray = TRUE;
            handled = TRUE;
        } else if (g_strcmp0(arg, "--no-strip") == 0) {
            opts->no_strip = TRUE;
            handled = TRUE;
        } else if (g_strcmp0(arg, "--debug") == 0) {
            opts->debug = TRUE;
            handled = TRUE;
        }

        for (gsize m = 0; !handled && m < G_N_ELEMENTS(mode_flags); m++) {
            if (g_strcmp0(arg, mode_flags[m].flag) == 0) {
                opts->mode = mode_flags[m].mode;
                handled = TRUE;
            }
        }

        if (!handled) {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION,
                        "Bilinmeyen bayrak: %s", arg);
            return FALSE;
        }
    }
    return TRUE;
}

const gchar *cli_usage(void)
{
    return
        "Kullanım: comebackwindow [BAYRAK...]\n"
        "\n"
        "Bayraklar:\n"
        "  --setup              Kurulum penceresini zorla göster\n"
        "  --settings           Ayarlar penceresini göster\n"
        "  --toggle-desktop     Masaüstünü göster / geri getir\n"
        "  --minimize-others    Aktif hariç hepsini küçült\n"
        "  --restore-all        Küçültülenleri geri getir\n"
        "  --restore-shortcuts  Yedeklenen KDE kısayollarını geri yükle\n"
        "  --uninstall-cleanup  Autostart, betik ve kısayol yedeğini temizle\n"
        "  --dump-state         Betiğin durum dökümünü JSON bas\n"
        "  --no-tray            Tepsi simgesini kapat\n"
        "  --no-strip           Köşe şeridini kapat\n"
        "  --debug              Ayrıntılı günlük\n"
        "  --version            Sürümü yaz\n"
        "  --help               Bu yardımı yaz\n";
}

gboolean cli_mode_is_client(CliMode mode)
{
    return mode == CBW_MODE_SETTINGS
        || mode == CBW_MODE_TOGGLE_DESKTOP
        || mode == CBW_MODE_MINIMIZE_OTHERS
        || mode == CBW_MODE_RESTORE_ALL
        || mode == CBW_MODE_DUMP_STATE;
}

const gchar *cli_mode_method(CliMode mode)
{
    switch (mode) {
    case CBW_MODE_SETTINGS:
        return "Activate";
    case CBW_MODE_TOGGLE_DESKTOP:
        return "ToggleDesktop";
    case CBW_MODE_MINIMIZE_OTHERS:
        return "MinimizeOthers";
    case CBW_MODE_RESTORE_ALL:
        return "RestoreAll";
    case CBW_MODE_DUMP_STATE:
        return "DumpState";
    default:
        return NULL;
    }
}
