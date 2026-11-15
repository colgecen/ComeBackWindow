/*
 * Come Back Window — yapılandırma modülü
 */
#include "config.h"

#include <errno.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
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
    s->strip_width = 10;
    s->strip_style = g_strdup("bar");
    s->strip_text = g_strdup("Masaüstü");
    s->strip_color = g_strdup("#1f80c4");
    s->strip_text_color = g_strdup("#ffffff");
    s->strip_opacity = 0.90;
    s->strip_text_opacity = 1.0;
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
    g_clear_pointer(&s->strip_style, g_free);
    g_clear_pointer(&s->strip_text, g_free);
    g_clear_pointer(&s->strip_color, g_free);
    g_clear_pointer(&s->strip_text_color, g_free);
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

/*
 * Okunamayan ayar dosyasını zaman damgalı bir ada taşır; uygulama varsayılan
 * ayarlarla devam edebilir ve kullanıcı eski dosyayı elle inceleyebilir.
 */
gboolean settings_backup_corrupt(GError **error)
{
    gchar *path = config_path();

    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_free(path);
        return FALSE;
    }
    gchar *target = g_strdup_printf("%s.bozuk-%" G_GINT64_FORMAT, path,
                                    g_get_real_time());
    gboolean ok = g_rename(path, target) == 0;

    if (!ok) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "bozuk ayar dosyası yedeklenemedi: %s", g_strerror(errno));
    }
    g_free(target);
    g_free(path);
    return ok;
}

/* #RRGGBB biçimini doğrular; geçersizse varsayılana düşer. */
static gboolean color_ok(const gchar *text)
{
    if (text == NULL || strlen(text) != 7 || text[0] != '#') {
        return FALSE;
    }
    for (gint i = 1; i < 7; i++) {
        if (!g_ascii_isxdigit(text[i])) {
            return FALSE;
        }
    }
    return TRUE;
}

static gchar *color_valid(const gchar *text, const gchar *fallback)
{
    return g_strdup(color_ok(text) ? text : fallback);
}

/* Metni kırpar, boş/geçersizse varsayılana düşer ve uzunluğu sınırlar. */
static gchar *strip_text_valid(const gchar *text)
{
    if (text == NULL) {
        return g_strdup("Masaüstü");
    }
    gchar *copy = g_strdup(text);

    g_strstrip(copy);
    if (*copy == '\0' || !g_utf8_validate(copy, -1, NULL)) {
        g_free(copy);
        return g_strdup("Masaüstü");
    }
    if (g_utf8_strlen(copy, -1) > CBW_STRIP_TEXT_MAX) {
        gchar *cut = g_utf8_substring(copy, 0, CBW_STRIP_TEXT_MAX);

        g_free(copy);
        copy = cut;
    }
    return copy;
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
    if (g_strcmp0(s->strip_style, "text") != 0) {
        g_free(s->strip_style);
        s->strip_style = g_strdup("bar");
    }
    gchar *text = strip_text_valid(s->strip_text);

    g_free(s->strip_text);
    s->strip_text = text;
    gchar *color = color_valid(s->strip_color, "#1f80c4");

    g_free(s->strip_color);
    s->strip_color = color;
    gchar *text_color = color_valid(s->strip_text_color, "#ffffff");

    g_free(s->strip_text_color);
    s->strip_text_color = text_color;
    s->strip_opacity = CLAMP(s->strip_opacity, 0.0, 1.0);
    s->strip_text_opacity = CLAMP(s->strip_text_opacity, 0.0, 1.0);
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
    gchar *style = g_key_file_get_string(kf, GROUP, "strip_style", NULL);
    if (style != NULL) {
        g_free(s->strip_style);
        s->strip_style = style;
    }
    gchar *text = g_key_file_get_string(kf, GROUP, "strip_text", NULL);
    if (text != NULL) {
        g_free(s->strip_text);
        s->strip_text = text;
    }
    gchar *color = g_key_file_get_string(kf, GROUP, "strip_color", NULL);
    if (color != NULL) {
        g_free(s->strip_color);
        s->strip_color = color;
    }
    gchar *text_color = g_key_file_get_string(kf, GROUP, "strip_text_color",
                                              NULL);
    if (text_color != NULL) {
        g_free(s->strip_text_color);
        s->strip_text_color = text_color;
    }
    s->strip_opacity = get_double(kf, "strip_opacity", s->strip_opacity);
    s->strip_text_opacity = get_double(kf, "strip_text_opacity",
                                       s->strip_text_opacity);
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
    g_key_file_set_string(kf, GROUP, "strip_style",
                          s->strip_style != NULL ? s->strip_style : "bar");
    g_key_file_set_string(kf, GROUP, "strip_text",
                          s->strip_text != NULL ? s->strip_text
                                                : "Masaüstü");
    g_key_file_set_string(kf, GROUP, "strip_color",
                          s->strip_color != NULL ? s->strip_color
                                                 : "#1f80c4");
    g_key_file_set_string(kf, GROUP, "strip_text_color",
                          s->strip_text_color != NULL
                              ? s->strip_text_color : "#ffffff");
    gchar strip_alpha[G_ASCII_DTOSTR_BUF_SIZE];

    g_ascii_formatd(strip_alpha, sizeof(strip_alpha), "%.3f",
                    s->strip_opacity);
    g_key_file_set_string(kf, GROUP, "strip_opacity", strip_alpha);
    gchar text_alpha[G_ASCII_DTOSTR_BUF_SIZE];

    g_ascii_formatd(text_alpha, sizeof(text_alpha), "%.3f",
                    s->strip_text_opacity);
    g_key_file_set_string(kf, GROUP, "strip_text_opacity", text_alpha);
    g_key_file_set_integer(kf, GROUP, "fallback_height", s->fallback_height);
    g_key_file_set_boolean(kf, GROUP, "peek_enabled", s->peek_enabled);
    g_key_file_set_integer(kf, GROUP, "peek_delay_ms", s->peek_delay_ms);
    gchar opacity[G_ASCII_DTOSTR_BUF_SIZE];

    g_ascii_formatd(opacity, sizeof(opacity), "%.3f", s->peek_opacity);
    g_key_file_set_string(kf, GROUP, "peek_opacity", opacity);
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

/* JSON dizesi: yalnızca zorunlu kaçışlar; UTF-8 aynen korunur. */
static gchar *json_escape(const gchar *text)
{
    GString *out = g_string_new(NULL);

    for (const guchar *p = (const guchar *)(text != NULL ? text : ""); *p; p++) {
        switch (*p) {
        case '"':
            g_string_append(out, "\\\"");
            break;
        case '\\':
            g_string_append(out, "\\\\");
            break;
        case '\n':
            g_string_append(out, "\\n");
            break;
        case '\r':
            g_string_append(out, "\\r");
            break;
        case '\t':
            g_string_append(out, "\\t");
            break;
        default:
            if (*p < 0x20) {
                g_string_append_printf(out, "\\u%04x", *p);
            } else {
                g_string_append_c(out, (gchar)*p);
            }
            break;
        }
    }
    return g_string_free(out, FALSE);
}

gchar *settings_to_json(const Settings *s)
{
    g_return_val_if_fail(s != NULL, NULL);

    /* JSON her zaman nokta ondalık ayırıcısı ister; yerel ayardan bağımsız yaz. */
    gchar opacity[G_ASCII_DTOSTR_BUF_SIZE];

    g_ascii_formatd(opacity, sizeof(opacity), "%.3f", s->peek_opacity);

    gchar *lang = json_escape(s->language != NULL ? s->language : "auto");
    gchar *style = json_escape(s->strip_style != NULL ? s->strip_style : "bar");
    gchar *text = json_escape(s->strip_text != NULL ? s->strip_text
                                                    : "Masaüstü");
    gchar *color = json_escape(s->strip_color != NULL ? s->strip_color
                                                      : "#1f80c4");
    gchar *text_color = json_escape(s->strip_text_color != NULL
                                        ? s->strip_text_color : "#ffffff");
    gchar strip_alpha[G_ASCII_DTOSTR_BUF_SIZE];
    gchar text_alpha[G_ASCII_DTOSTR_BUF_SIZE];

    g_ascii_formatd(strip_alpha, sizeof(strip_alpha), "%.3f",
                    s->strip_opacity);
    g_ascii_formatd(text_alpha, sizeof(text_alpha), "%.3f",
                    s->strip_text_opacity);
    gchar *json = g_strdup_printf(
        "{\"autostart\":%s,\"strip_enabled\":%s,\"strip_width\":%d,"
        "\"strip_style\":\"%s\",\"strip_text\":\"%s\","
        "\"strip_color\":\"%s\",\"strip_text_color\":\"%s\","
        "\"strip_opacity\":%s,\"strip_text_opacity\":%s,"
        "\"fallback_height\":%d,\"peek_enabled\":%s,\"peek_delay_ms\":%d,"
        "\"peek_opacity\":%s,\"shake_enabled\":%s,\"tray_enabled\":%s,"
        "\"language\":\"%s\",\"first_run_done\":%s}",
        s->autostart ? "true" : "false",
        s->strip_enabled ? "true" : "false",
        s->strip_width,
        style,
        text,
        color,
        text_color,
        strip_alpha,
        text_alpha,
        s->fallback_height,
        s->peek_enabled ? "true" : "false",
        s->peek_delay_ms,
        opacity,
        s->shake_enabled ? "true" : "false",
        s->tray_enabled ? "true" : "false",
        lang,
        s->first_run_done ? "true" : "false");

    g_free(lang);
    g_free(style);
    g_free(text);
    g_free(color);
    g_free(text_color);
    return json;
}
