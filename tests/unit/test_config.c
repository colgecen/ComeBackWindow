/* config.c birim testleri */
#include "config.h"
#include "test_util.h"

#include <glib/gstdio.h>
#include <locale.h>
#include <unistd.h>

static gchar *tmpdir = NULL;

static void setup(void)
{
    GError *error = NULL;

    tmpdir = g_dir_make_tmp("cbw-test-config-XXXXXX", &error);
    g_assert_no_error(error);
    g_setenv("CBW_CONFIG_DIR", tmpdir, TRUE);
}

static void teardown(void)
{
    if (tmpdir == NULL) {
        return;
    }
    GDir *dir = g_dir_open(tmpdir, 0, NULL);

    if (dir != NULL) {
        const gchar *name = NULL;

        while ((name = g_dir_read_name(dir)) != NULL) {
            gchar *full = g_build_filename(tmpdir, name, NULL);
            g_unlink(full);
            g_free(full);
        }
        g_dir_close(dir);
    }
    g_rmdir(tmpdir);
    g_free(tmpdir);
    g_unsetenv("CBW_CONFIG_DIR");
}

static void test_defaults(void)
{
    Settings s;

    settings_init_defaults(&s);
    CHECK_INT_EQ(s.autostart, TRUE);
    CHECK_INT_EQ(s.strip_enabled, TRUE);
    CHECK_INT_EQ(s.strip_width, 10);
    CHECK_INT_EQ(s.fallback_height, 44);
    CHECK_INT_EQ(s.peek_enabled, TRUE);
    CHECK_INT_EQ(s.peek_delay_ms, 500);
    CHECK(s.peek_opacity > 0.079 && s.peek_opacity < 0.081);
    CHECK_INT_EQ(s.shake_enabled, FALSE);
    CHECK_INT_EQ(s.tray_enabled, TRUE);
    CHECK_STR_EQ(s.language, "auto");
    CHECK_INT_EQ(s.first_run_done, FALSE);
    settings_clear(&s);
    CHECK(s.language == NULL);
}

static void test_clamp(void)
{
    Settings s;

    settings_init_defaults(&s);
    s.strip_width = 0;
    s.fallback_height = 999;
    s.peek_delay_ms = 5;
    s.peek_opacity = -1.0;
    g_free(s.language);
    s.language = g_strdup("klingon");
    settings_clamp(&s);
    CHECK_INT_EQ(s.strip_width, 2);
    CHECK_INT_EQ(s.fallback_height, 120);
    CHECK_INT_EQ(s.peek_delay_ms, 100);
    CHECK(s.peek_opacity == 0.0);
    CHECK_STR_EQ(s.language, "auto");

    s.strip_width = 100;
    s.peek_delay_ms = 99999;
    s.peek_opacity = 9.0;
    settings_clamp(&s);
    CHECK_INT_EQ(s.strip_width, 24);
    CHECK_INT_EQ(s.peek_delay_ms, 2000);
    CHECK(s.peek_opacity == 0.5);
    settings_clear(&s);
}

static void test_missing_file(void)
{
    Settings s;
    GError *error = NULL;

    settings_init_defaults(&s);
    CHECK(config_exists() == FALSE);
    CHECK(settings_load(&s, &error) == TRUE);
    g_assert_no_error(error);
    CHECK_STR_EQ(s.language, "auto");
    CHECK_INT_EQ(s.strip_width, 10);
    settings_clear(&s);
}

static void test_roundtrip(void)
{
    Settings out;
    Settings in;
    GError *error = NULL;

    settings_init_defaults(&out);
    out.autostart = FALSE;
    out.strip_width = 12;
    out.fallback_height = 60;
    out.peek_enabled = FALSE;
    out.peek_delay_ms = 750;
    out.peek_opacity = 0.2;
    out.shake_enabled = TRUE;
    out.tray_enabled = FALSE;
    g_free(out.strip_style);
    out.strip_style = g_strdup("text");
    g_free(out.strip_text);
    out.strip_text = g_strdup("Masaüstü");
    g_free(out.language);
    out.language = g_strdup("tr");
    out.first_run_done = TRUE;

    CHECK(settings_save(&out, &error) == TRUE);
    g_assert_no_error(error);
    CHECK(config_exists() == TRUE);

    settings_init_defaults(&in);
    CHECK(settings_load(&in, &error) == TRUE);
    g_assert_no_error(error);
    CHECK_INT_EQ(in.autostart, FALSE);
    CHECK_INT_EQ(in.strip_width, 12);
    CHECK_INT_EQ(in.fallback_height, 60);
    CHECK_INT_EQ(in.peek_enabled, FALSE);
    CHECK_INT_EQ(in.peek_delay_ms, 750);
    CHECK(in.peek_opacity > 0.19 && in.peek_opacity < 0.21);
    CHECK_INT_EQ(in.shake_enabled, TRUE);
    CHECK_INT_EQ(in.tray_enabled, FALSE);
    CHECK_STR_EQ(in.strip_style, "text");
    CHECK_STR_EQ(in.strip_text, "Masaüstü");
    CHECK_STR_EQ(in.language, "tr");
    CHECK_INT_EQ(in.first_run_done, TRUE);

    settings_clear(&out);
    settings_clear(&in);
}

static void test_corrupt_file(void)
{
    Settings s;
    GError *error = NULL;
    gchar *path = config_path();

    g_assert_true(g_file_set_contents(path, "bu bir ini dosyasi degil\n\x01\x02",
                                      22, NULL));
    g_free(path);
    settings_init_defaults(&s);
    /* Bozuk dosya varsayılanlarla birlikte okunur. */
    settings_load(&s, &error);
    CHECK_INT_EQ(s.strip_width, 10);
    g_clear_error(&error);
    settings_clear(&s);
}

/* Okunamayan dosya yedeklenir ve varsayılanlarla devam edilir. */
static void test_corrupt_backup(void)
{
    if (geteuid() == 0) {
        return; /* root izinlere takılmaz */
    }
    gchar *path = config_path();
    Settings s;
    GError *error = NULL;

    g_assert_true(g_file_set_contents(path, "[comebackwindow]\nstrip_width=6\n",
                                      -1, NULL));
    g_assert_cmpint(g_chmod(path, 0000), ==, 0);
    settings_init_defaults(&s);
    CHECK(settings_load(&s, &error) == FALSE);
    g_clear_error(&error);
    CHECK(settings_backup_corrupt(&error) == TRUE);
    g_clear_error(&error);
    CHECK(config_exists() == FALSE);
    CHECK(settings_load(&s, &error) == TRUE); /* dosya yok: varsayılan */
    CHECK_INT_EQ(s.strip_width, 10);
    g_clear_error(&error);
    settings_clear(&s);
    g_free(path);
}

static void test_partial_file(void)
{
    Settings s;
    GError *error = NULL;
    gchar *path = config_path();
    const char *text =
        "[comebackwindow]\n"
        "strip_width=10\n"
        "peek_opacity=0.35\n";

    g_assert_true(g_file_set_contents(path, text, -1, NULL));
    g_free(path);

    settings_init_defaults(&s);
    CHECK(settings_load(&s, &error) == TRUE);
    g_assert_no_error(error);
    CHECK_INT_EQ(s.strip_width, 10);
    CHECK(s.peek_opacity > 0.349 && s.peek_opacity < 0.351);
    /* Eksik anahtarlar varsayılanda kalır. */
    CHECK_INT_EQ(s.fallback_height, 44);
    CHECK_INT_EQ(s.autostart, TRUE);
    settings_clear(&s);
}

static void test_json(void)
{
    Settings s;
    gchar *json = NULL;

    settings_init_defaults(&s);
    s.strip_width = 8;
    g_free(s.language);
    s.language = g_strdup("en");
    json = settings_to_json(&s);
    CHECK(json != NULL);
    CHECK(strstr(json, "\"strip_width\":8") != NULL);
    CHECK(strstr(json, "\"strip_style\":\"bar\"") != NULL);
    CHECK(strstr(json, "\"strip_text\":\"Masaüstü\"") != NULL);
    CHECK(strstr(json, "\"language\":\"en\"") != NULL);
    CHECK(strstr(json, "\"shake_enabled\":false") != NULL);
    g_free(json);
    settings_clear(&s);
}

/* JSON çıktısı yerel ayardan bağımsız olarak nokta kullanmalı. */
static void test_json_locale(void)
{
    gchar *saved = g_strdup(setlocale(LC_NUMERIC, NULL));
    Settings s;
    gchar *json = NULL;

    if (setlocale(LC_NUMERIC, "de_DE.utf8") == NULL
        && setlocale(LC_NUMERIC, "de_AT.utf8") == NULL
        && setlocale(LC_NUMERIC, "tr_TR.utf8") == NULL) {
        g_free(saved);
        return; /* uygun yerel ayar yok: atla */
    }
    settings_init_defaults(&s);
    s.peek_opacity = 0.08;
    json = settings_to_json(&s);
    CHECK(json != NULL);
    CHECK(strstr(json, "\"peek_opacity\":0.080") != NULL);
    CHECK(strchr(json, ',') != NULL); /* ayırıcı virgüller JSON yapısından */
    g_free(json);
    settings_clear(&s);
    if (saved != NULL) {
        setlocale(LC_NUMERIC, saved);
    }
    g_free(saved);
}

int main(void)
{
    setup();

    test_defaults();
    test_clamp();
    test_missing_file();
    test_roundtrip();
    test_corrupt_file();
    test_corrupt_backup();
    test_partial_file();
    test_json();
    test_json_locale();

    teardown();
    TEST_MAIN_END();
}
