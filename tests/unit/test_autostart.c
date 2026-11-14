/* autostart.c birim testleri */
#include "autostart.h"
#include "test_util.h"

#include <glib/gstdio.h>

static gchar *tmpdir = NULL;

static void setup(void)
{
    GError *error = NULL;

    tmpdir = g_dir_make_tmp("cbw-test-autostart-XXXXXX", &error);
    g_assert_no_error(error);
    g_setenv("CBW_AUTOSTART_DIR", tmpdir, TRUE);
}

static void teardown(void)
{
    gchar *path = autostart_path();

    g_unlink(path);
    g_free(path);
    if (tmpdir != NULL) {
        g_rmdir(tmpdir);
        g_free(tmpdir);
    }
    g_unsetenv("CBW_AUTOSTART_DIR");
}

static void test_create_and_delete(void)
{
    GError *error = NULL;
    gchar *path = autostart_path();

    CHECK(autostart_is_enabled() == FALSE);
    CHECK(autostart_enable(&error) == TRUE);
    g_assert_no_error(error);
    CHECK(autostart_is_enabled() == TRUE);
    CHECK(g_file_test(path, G_FILE_TEST_IS_REGULAR) == TRUE);
    g_free(path);

    CHECK(autostart_disable(&error) == TRUE);
    g_assert_no_error(error);
    CHECK(autostart_is_enabled() == FALSE);

    /* İkinci silme de başarılı olmalı. */
    CHECK(autostart_disable(&error) == TRUE);
    g_assert_no_error(error);
}

static void test_content(void)
{
    GError *error = NULL;
    gchar *path = autostart_path();
    gchar *contents = NULL;

    CHECK(autostart_enable(&error) == TRUE);
    g_assert_no_error(error);
    CHECK(g_file_get_contents(path, &contents, NULL, NULL) == TRUE);
    CHECK(strstr(contents, "[Desktop Entry]") != NULL);
    CHECK(strstr(contents, "Exec=comebackwindow") != NULL);
    CHECK(strstr(contents, "X-KDE-autostart-after=panel") != NULL);
    CHECK(strstr(contents, "Icon=comebackwindow") != NULL);
    CHECK(strstr(contents, "NoDisplay=true") != NULL);

    g_free(contents);
    g_free(path);
    autostart_disable(NULL);
}

static void test_unwritable_dir(void)
{
    GError *error = NULL;
    gchar *nested = g_build_filename(tmpdir, "a", "b", "c", NULL);

    g_setenv("CBW_AUTOSTART_DIR", "/proc/cbw-imkansiz-dizin", TRUE);
    CHECK(autostart_enable(&error) == FALSE);
    CHECK(error != NULL);
    g_clear_error(&error);
    g_setenv("CBW_AUTOSTART_DIR", tmpdir, TRUE);
    g_free(nested);
}

int main(void)
{
    setup();
    test_create_and_delete();
    test_content();
    test_unwritable_dir();
    teardown();
    TEST_MAIN_END();
}
