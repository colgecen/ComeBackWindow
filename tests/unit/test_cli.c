/* cli.c birim testleri */
#include "cli.h"
#include "test_util.h"

static gboolean parse(const gchar *const *args, CliOptions *opts, GError **error)
{
    int argc = 0;

    while (args[argc] != NULL) {
        argc++;
    }
    return cli_parse(argc, (char **)args, opts, error);
}

static void test_defaults(void)
{
    CliOptions opts;

    cli_options_init(&opts);
    CHECK_INT_EQ(opts.mode, CBW_MODE_RUN);
    CHECK_INT_EQ(opts.no_tray, FALSE);
    CHECK_INT_EQ(opts.no_strip, FALSE);
    CHECK_INT_EQ(opts.debug, FALSE);
    CHECK_INT_EQ(opts.version, FALSE);
    CHECK_INT_EQ(opts.help, FALSE);
}

static void test_modes(void)
{
    struct {
        const gchar *flag;
        CliMode mode;
    } cases[] = {
        { "--setup", CBW_MODE_SETUP },
        { "--settings", CBW_MODE_SETTINGS },
        { "--toggle-desktop", CBW_MODE_TOGGLE_DESKTOP },
        { "--minimize-others", CBW_MODE_MINIMIZE_OTHERS },
        { "--restore-all", CBW_MODE_RESTORE_ALL },
        { "--restore-shortcuts", CBW_MODE_RESTORE_SHORTCUTS },
        { "--uninstall-cleanup", CBW_MODE_UNINSTALL_CLEANUP },
        { "--dump-state", CBW_MODE_DUMP_STATE }
    };

    for (gsize i = 0; i < G_N_ELEMENTS(cases); i++) {
        CliOptions opts;
        GError *error = NULL;
        const gchar *args[] = { "comebackwindow", cases[i].flag, NULL };

        cli_options_init(&opts);
        CHECK(parse(args, &opts, &error) == TRUE);
        CHECK(error == NULL);
        CHECK_INT_EQ(opts.mode, cases[i].mode);
    }
}

static void test_switches(void)
{
    CliOptions opts;
    GError *error = NULL;
    const gchar *args[] = {
        "comebackwindow", "--no-tray", "--no-strip", "--debug", NULL
    };

    cli_options_init(&opts);
    CHECK(parse(args, &opts, &error) == TRUE);
    CHECK_INT_EQ(opts.no_tray, TRUE);
    CHECK_INT_EQ(opts.no_strip, TRUE);
    CHECK_INT_EQ(opts.debug, TRUE);
}

static void test_version_help(void)
{
    CliOptions opts;
    GError *error = NULL;
    const gchar *version_args[] = { "comebackwindow", "--version", NULL };
    const gchar *help_args[] = { "comebackwindow", "-h", NULL };

    cli_options_init(&opts);
    CHECK(parse(version_args, &opts, &error) == TRUE);
    CHECK_INT_EQ(opts.version, TRUE);

    cli_options_init(&opts);
    CHECK(parse(help_args, &opts, &error) == TRUE);
    CHECK_INT_EQ(opts.help, TRUE);
}

static void test_unknown(void)
{
    CliOptions opts;
    GError *error = NULL;
    const gchar *args[] = { "comebackwindow", "--bilinmeyen", NULL };

    cli_options_init(&opts);
    CHECK(parse(args, &opts, &error) == FALSE);
    CHECK(error != NULL);
    CHECK(g_error_matches(error, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION));
    g_clear_error(&error);
}

static void test_client_methods(void)
{
    CHECK_INT_EQ(cli_mode_is_client(CBW_MODE_RUN), FALSE);
    CHECK_INT_EQ(cli_mode_is_client(CBW_MODE_SETTINGS), TRUE);
    CHECK_INT_EQ(cli_mode_is_client(CBW_MODE_DUMP_STATE), TRUE);
    CHECK_INT_EQ(cli_mode_is_client(CBW_MODE_RESTORE_SHORTCUTS), FALSE);

    CHECK_STR_EQ(cli_mode_method(CBW_MODE_TOGGLE_DESKTOP), "ToggleDesktop");
    CHECK_STR_EQ(cli_mode_method(CBW_MODE_MINIMIZE_OTHERS), "MinimizeOthers");
    CHECK_STR_EQ(cli_mode_method(CBW_MODE_RESTORE_ALL), "RestoreAll");
    CHECK_STR_EQ(cli_mode_method(CBW_MODE_DUMP_STATE), "DumpState");
    CHECK(cli_mode_method(CBW_MODE_RUN) == NULL);
}

static void test_usage(void)
{
    const gchar *usage = cli_usage();

    CHECK(usage != NULL);
    CHECK(strstr(usage, "--toggle-desktop") != NULL);
    CHECK(strstr(usage, "--restore-shortcuts") != NULL);
}

int main(void)
{
    test_defaults();
    test_modes();
    test_switches();
    test_version_help();
    test_unknown();
    test_client_methods();
    test_usage();
    TEST_MAIN_END();
}
