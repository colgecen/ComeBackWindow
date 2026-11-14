/*
 * Come Back Window — yapılandırma modülü
 */
#include "config.h"

#include <errno.h>
#include <gio/gio.h>
#include <string.h>

#define CONFIG_SUBDIR "comebackwindow"
#define CONFIG_FILE "config.ini"
#define GROUP "comebackwindow"

#define STRIP_WIDTH_MIN 2
#define STRIP_WIDTH_MAX 24
#define FALLBACK_HEIGHT_MIN 20
#define FALLBACK_HEIGHT_MAX 120
#define PEEK_DELAY_MIN 100
#define PEEK_DELAY_MAX 2000
#define PEEK_OPACITY_MIN 0.0
#define PEEK_OPACITY_MAX 0.5

static gint clamp_int(gint value, gint lo, gint hi)
{
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

static gboolean language_valid(const gchar *lang)
{
    return lang != NULL
        && (g_strcmp0(lang, "auto") == 0
            || g_strcmp0(lang, "tr") == 0
            || g_strcmp0(lang, "en") == 0);
}

void settings_init_defaults(Settings *s)
{
    g_return_if_fail(s != NULL);

    s->autostart = TRUE;
    s->strip_enabled = TRUE;
    s->strip_width = 6;
    s->fallback_height = 44;
    s->peek_enabled = TRUE;
    s->peek_delay_ms = 500;
    s->peek_opacity = 0.08;
    s->shake_enabled = FALSE;
    s->tray_enabled = TRUE;
    s->language = g_strdup("auto");
    s->first_run_done = FALSE;
}

void settings_clear(Settings *s)
{
    if (s == NULL) {
        return;
    }
    g_clear_pointer(&s->language, g_free);
}

gchar *config_dir(void)
{
    const gchar *override = g_getenv("CBW_CONFIG_DIR");

    if (override != NULL && *override != '\0') {
        return g_strdup(override);
    }
    return g_build_filename(g_get_user_config_dir(), CONFIG_SUBDIR, NULL);
}

gchar *config_path(void)
{
    gchar *dir = config_dir();
    gchar *path = g_build_filename(dir, CONFIG_FILE, NULL);

    g_free(dir);
    return path;
}

gboolean config_exists(void)
{
    gchar *path = config_path();
    gboolean exists = g_file_test(path, G_FILE_TEST_IS_REGULAR);

    g_free(path);
    return exists;
}

void settings_clamp(Settings *s)
{
    g_return_if_fail(s != NULL);

    s->strip_width = clamp_int(s->strip_width, STRIP_WIDTH_MIN, STRIP_WIDTH_MAX);
    s->fallback_height = clamp_int(s->fallback_height,
                                   FALLBACK_HEIGHT_MIN, FALLBACK_HEIGHT_MAX);
    s->peek_delay_ms = clamp_int(s->peek_delay_ms, PEEK_DELAY_MIN, PEEK_DELAY_MAX);
    if (s->peek_opacity < PEEK_OPACITY_MIN) {
        s->peek_opacity = PEEK_OPACITY_MIN;
    }
    if (s->peek_opacity > PEEK_OPACITY_MAX) {
        s->peek_opacity = PEEK_OPACITY_MAX;
    }
    if (!language_valid(s->language)) {
        g_free(s->language);
        s->language = g_strdup("auto");
    }
}

static gboolean get_bool(GKeyFile *kf, const gchar *key, gboolean fallback)
{
    GError *error = NULL;
    gboolean value = g_key_file_get_boolean(kf, GROUP, key, &error);

    if (error != NULL) {
        g_clear_error(&error);
        return fallback;
    }
    return value;
}

static gint get_int(GKeyFile *kf, const gchar *key, gint fallback)
{
    GError *error = NULL;
    gint value = g_key_file_get_integer(kf, GROUP, key, &error);

    if (error != NULL) {
        g_clear_error(&error);
        return fallback;
    }
    return value;
}

static gdouble get_double(GKeyFile *kf, const gchar *key, gdouble fallback)
{
    GError *error = NULL;
    gdouble value = g_key_file_get_double(kf, GROUP, key, &error);

    if (error != NULL) {
        g_clear_error(&error);
        return fallback;
    }
    return value;
}

static void load_group(Settings *s, GKeyFile *kf)
{
    s->autostart = get_bool(kf, "autostart", s->autostart);
    s->strip_enabled = get_bool(kf, "strip_enabled", s->strip_enabled);
    s->strip_width = get_int(kf, "strip_width", s->strip_width);
    s->fallback_height = get_int(kf, "fallback_height", s->fallback_height);
    s->peek_enabled = get_bool(kf, "peek_enabled", s->peek_enabled);
    s->peek_delay_ms = get_int(kf, "peek_delay_ms", s->peek_delay_ms);
    s->peek_opacity = get_double(kf, "peek_opacity", s->peek_opacity);
    s->shake_enabled = get_bool(kf, "shake_enabled", s->shake_enabled);
    s->tray_enabled = get_bool(kf, "tray_enabled", s->tray_enabled);
    s->first_run_done = get_bool(kf, "first_run_done", s->first_run_done);

    gchar *lang = g_key_file_get_string(kf, GROUP, "language", NULL);
    if (lang != NULL) {
        g_free(s->language);
        s->language = lang;
    }
}

gboolean settings_load(Settings *s, GError **error)
{
    g_return_val_if_fail(s != NULL, FALSE);

    gchar *path = config_path();
    GKeyFile *kf = g_key_file_new();
    GError *local_error = NULL;

    if (!g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, &local_error)) {
        gboolean not_found = g_error_matches(local_error, G_FILE_ERROR,
                                             G_FILE_ERROR_NOENT);
        g_clear_error(&local_error);
        g_key_file_unref(kf);
        g_free(path);
        if (not_found) {
            return TRUE; /* varsayılanlar geçerli */
        }
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                    "Ayar dosyası okunamadı");
        return FALSE;
    }

    load_group(s, kf);
    g_key_file_unref(kf);
    g_free(path);
    settings_clamp(s);
    return TRUE;
}

static void write_group(const Settings *s, GKeyFile *kf)
{
    g_key_file_set_boolean(kf, GROUP, "autostart", s->autostart);
    g_key_file_set_boolean(kf, GROUP, "strip_enabled", s->strip_enabled);
    g_key_file_set_integer(kf, GROUP, "strip_width", s->strip_width);
    g_key_file_set_integer(kf, GROUP, "fallback_height", s->fallback_height);
    g_key_file_set_boolean(kf, GROUP, "peek_enabled", s->peek_enabled);
    g_key_file_set_integer(kf, GROUP, "peek_delay_ms", s->peek_delay_ms);
    g_key_file_set_double(kf, GROUP, "peek_opacity", s->peek_opacity);
    g_key_file_set_boolean(kf, GROUP, "shake_enabled", s->shake_enabled);
    g_key_file_set_boolean(kf, GROUP, "tray_enabled", s->tray_enabled);
    g_key_file_set_string(kf, GROUP, "language", s->language != NULL ? s->language : "auto");
    g_key_file_set_boolean(kf, GROUP, "first_run_done", s->first_run_done);
}

gboolean settings_save(const Settings *s, GError **error)
{
    g_return_val_if_fail(s != NULL, FALSE);

    gchar *dir = config_dir();
    if (g_mkdir_with_parents(dir, 0700) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Ayar dizini oluşturulamadı: %s", dir);
        g_free(dir);
        return FALSE;
    }

    gchar *path = config_path();
    GKeyFile *kf = g_key_file_new();
    write_group(s, kf);

    gsize length = 0;
    gchar *data = g_key_file_to_data(kf, &length, error);
    gboolean ok = FALSE;

    if (data != NULL) {
        ok = g_file_set_contents(path, data, (gssize)length, error);
    }

    g_free(data);
    g_key_file_unref(kf);
    g_free(path);
    g_free(dir);
    return ok;
}

gchar *settings_to_json(const Settings *s)
{
    g_return_val_if_fail(s != NULL, NULL);

    /* JSON her zaman nokta ondalık ayırıcısı ister; yerel ayardan bağımsız yaz. */
    gchar opacity[G_ASCII_DTOSTR_BUF_SIZE];

    g_ascii_formatd(opacity, sizeof(opacity), "%.3f", s->peek_opacity);

    gchar *lang = g_strescape(s->language != NULL ? s->language : "auto", NULL);
    gchar *json = g_strdup_printf(
        "{\"autostart\":%s,\"strip_enabled\":%s,\"strip_width\":%d,"
        "\"fallback_height\":%d,\"peek_enabled\":%s,\"peek_delay_ms\":%d,"
        "\"peek_opacity\":%s,\"shake_enabled\":%s,\"tray_enabled\":%s,"
        "\"language\":\"%s\",\"first_run_done\":%s}",
        s->autostart ? "true" : "false",
        s->strip_enabled ? "true" : "false",
        s->strip_width,
        s->fallback_height,
        s->peek_enabled ? "true" : "false",
        s->peek_delay_ms,
        opacity,
        s->shake_enabled ? "true" : "false",
        s->tray_enabled ? "true" : "false",
        lang,
        s->first_run_done ? "true" : "false");

    g_free(lang);
    return json;
}
