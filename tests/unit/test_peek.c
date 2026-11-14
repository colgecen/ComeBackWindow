/* peek.c birim testleri */
#include "peek.h"
#include "test_util.h"

static App *make_app(gboolean enabled, gint delay)
{
    App *app = g_new0(App, 1);

    settings_init_defaults(&app->settings);
    app->settings.peek_enabled = enabled;
    app->settings.peek_delay_ms = delay;
    g_queue_init(&app->commands);
    g_mutex_init(&app->commands_lock);
    return app;
}

static void free_app(App *app)
{
    peek_destroy(app->peek);
    settings_clear(&app->settings);
    g_queue_clear_full(&app->commands, g_free);
    g_mutex_clear(&app->commands_lock);
    g_free(app);
}

static gsize queue_len(App *app)
{
    gsize count = 0;
    gchar **cmds = app_take_commands(app, &count);

    g_strfreev(cmds);
    return count;
}

static gchar *queue_join(App *app)
{
    gsize count = 0;
    gchar **cmds = app_take_commands(app, &count);
    GString *joined = g_string_new(NULL);

    for (gsize i = 0; i < count; i++) {
        if (i > 0) {
            g_string_append_c(joined, ',');
        }
        g_string_append(joined, cmds[i]);
    }
    g_strfreev(cmds);
    return g_string_free(joined, FALSE);
}

static void spin(guint ms)
{
    GMainContext *ctx = g_main_context_default();

    for (guint i = 0; i < ms; i += 10) {
        while (g_main_context_iteration(ctx, FALSE)) {
        }
        g_usleep(10 * 1000);
    }
    while (g_main_context_iteration(ctx, FALSE)) {
    }
}

static void test_enter_begins(void)
{
    App *app = make_app(TRUE, 10);
    Peek *peek = peek_create(app);

    peek_pointer_enter(peek);
    CHECK(!peek_is_active(peek));
    spin(60);
    CHECK(peek_is_active(peek));
    gchar *cmds = queue_join(app);

    CHECK_STR_EQ(cmds, "peek-begin");
    g_free(cmds);
    free_app(app);
}

static void test_leave_ends(void)
{
    App *app = make_app(TRUE, 10);
    Peek *peek = peek_create(app);

    peek_pointer_enter(peek);
    spin(50);
    CHECK(peek_is_active(peek));
    peek_pointer_leave(peek);
    CHECK(!peek_is_active(peek));
    gchar *cmds = queue_join(app);

    CHECK(g_strstr_len(cmds, -1, "peek-end") != NULL);
    g_free(cmds);
    free_app(app);
}

static void test_leave_before_delay(void)
{
    App *app = make_app(TRUE, 500);
    Peek *peek = peek_create(app);

    peek_pointer_enter(peek);
    peek_pointer_leave(peek);
    spin(40);
    CHECK(!peek_is_active(peek));
    CHECK_INT_EQ(queue_len(app), 0);
    free_app(app);
}

static void test_disabled(void)
{
    App *app = make_app(FALSE, 0);
    Peek *peek = peek_create(app);

    peek_pointer_enter(peek);
    spin(40);
    CHECK(!peek_is_active(peek));
    CHECK_INT_EQ(queue_len(app), 0);
    free_app(app);
}

static void test_toggle(void)
{
    App *app = make_app(TRUE, 0);
    Peek *peek = peek_create(app);

    peek_toggle(peek);
    CHECK(peek_is_active(peek));
    peek_toggle(peek);
    CHECK(!peek_is_active(peek));
    gchar *cmds = queue_join(app);

    CHECK_STR_EQ(cmds, "peek-begin,peek-end");
    g_free(cmds);
    free_app(app);
}

static void test_events(void)
{
    App *app = make_app(TRUE, 0);
    Peek *peek = peek_create(app);

    peek_on_event(peek, "peek-begin", "{}");
    CHECK(peek_is_active(peek));
    peek_on_event(peek, "peek-guard-restored", "{}");
    CHECK(!peek_is_active(peek));
    peek_on_event(peek, "peek-end", "{}");
    CHECK(!peek_is_active(peek));
    peek_on_event(peek, NULL, NULL);
    CHECK(!peek_is_active(peek));
    gsize count = 0;
    gchar **cmds = app_take_commands(app, &count);

    g_strfreev(cmds);
    CHECK_INT_EQ(count, 0);
    free_app(app);
}

static void test_fallback(void)
{
    App *app = make_app(TRUE, 0);
    Peek *peek = peek_create(app);

    peek_on_event(peek, "peek-fallback", "{}");
    CHECK(!peek_is_active(peek));
    gchar *cmds = queue_join(app);

    CHECK_STR_EQ(cmds, "toggle-desktop");
    g_free(cmds);
    spin(1700);
    cmds = queue_join(app);
    CHECK_STR_EQ(cmds, "toggle-desktop");
    g_free(cmds);
    free_app(app);
}

static void test_reload_disables(void)
{
    App *app = make_app(TRUE, 10);
    Peek *peek = peek_create(app);

    peek_pointer_enter(peek);
    spin(50);
    CHECK(peek_is_active(peek));
    app->settings.peek_enabled = FALSE;
    peek_reload_settings(peek);
    CHECK(!peek_is_active(peek));
    free_app(app);
}

int main(void)
{
    (void)queue_len;
    test_enter_begins();
    test_leave_ends();
    test_leave_before_delay();
    test_disabled();
    test_toggle();
    test_events();
    test_fallback();
    test_reload_disables();

    TEST_MAIN_END();
}
