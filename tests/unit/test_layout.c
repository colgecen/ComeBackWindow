/* layout.c birim testleri */
#include "layout.h"
#include "test_util.h"

static void test_parse_two_entries(void)
{
    const gchar *json =
        "[{\"output\":\"eDP-1\",\"x\":0,\"y\":0,\"w\":1920,\"h\":1080,"
        "\"edge\":\"bottom\",\"thickness\":44,\"scale\":1.25,"
        "\"fullscreen\":false},"
        "{\"output\":\"HDMI-A-1\",\"x\":1920,\"y\":0,\"w\":2560,\"h\":1440,"
        "\"edge\":\"left\",\"thickness\":32,\"scale\":2,\"fullscreen\":true}]";
    LayoutEntry *entries = NULL;
    gsize len = 0;
    GError *error = NULL;

    CHECK(layout_parse(json, &entries, &len, &error));
    CHECK(error == NULL);
    CHECK_INT_EQ(len, 2);
    CHECK_STR_EQ(entries[0].output, "eDP-1");
    CHECK_INT_EQ(entries[0].w, 1920);
    CHECK_INT_EQ(entries[0].h, 1080);
    CHECK_INT_EQ(entries[0].thickness, 44);
    CHECK_STR_EQ(entries[0].edge, "bottom");
    CHECK(entries[0].scale > 1.24 && entries[0].scale < 1.26);
    CHECK(!entries[0].fullscreen);
    CHECK_STR_EQ(entries[1].output, "HDMI-A-1");
    CHECK_INT_EQ(entries[1].x, 1920);
    CHECK_STR_EQ(entries[1].edge, "left");
    CHECK(entries[1].fullscreen);
    layout_entries_free(entries, len);
}

static void test_parse_empty(void)
{
    LayoutEntry *entries = NULL;
    gsize len = 99;
    GError *error = NULL;

    CHECK(layout_parse(" [ ] ", &entries, &len, &error));
    CHECK_INT_EQ(len, 0);
    CHECK(entries == NULL);
    CHECK(error == NULL);
}

static void test_parse_bozuk(void)
{
    static const gchar *bad[] = {
        "",
        "[",
        "[{",
        "[{\"output\":}]",
        "[{\"output\":\"x\"}",
        "[{\"output\":\"x\"},]",
        "[{\"output\":\"x\"}]]",
        "{\"output\":\"x\"}",
        "[{\"output\":\"x\",\"x\":12abc}]",
        "null"
    };

    for (gsize i = 0; i < G_N_ELEMENTS(bad); i++) {
        LayoutEntry *entries = NULL;
        gsize len = 7;
        GError *error = NULL;

        CHECK(!layout_parse(bad[i], &entries, &len, &error));
        CHECK(error != NULL);
        CHECK(entries == NULL);
        CHECK_INT_EQ(len, 0);
        g_clear_error(&error);
    }
}

static void test_parse_varsayilanlar(void)
{
    LayoutEntry *entries = NULL;
    gsize len = 0;
    GError *error = NULL;

    CHECK(layout_parse("[{\"output\":\"A\",\"edge\":\"çapraz\",\"scale\":0}]",
                       &entries, &len, &error));
    CHECK_INT_EQ(len, 1);
    CHECK_STR_EQ(entries[0].edge, "none");
    CHECK(entries[0].scale == 1.0);
    CHECK_INT_EQ(entries[0].x, 0);
    CHECK_INT_EQ(entries[0].thickness, 0);
    layout_entries_free(entries, len);
}

static void test_parse_kacis_ve_bilinmeyen(void)
{
    LayoutEntry *entries = NULL;
    gsize len = 0;
    GError *error = NULL;

    CHECK(layout_parse(
        "[{\"extra\":{\"a\":[1,true,null,\"s\"]},\"output\":\"e\\u011f\\\"1\","
        "\"fullscreen\":true}]",
        &entries, &len, &error));
    CHECK_INT_EQ(len, 1);
    CHECK_STR_EQ(entries[0].output, "eğ\"1");
    CHECK(entries[0].fullscreen);
    layout_entries_free(entries, len);
}

static void test_find(void)
{
    LayoutEntry entries[2] = {
        { .output = (gchar *)"A", .edge = (gchar *)"bottom" },
        { .output = (gchar *)"B", .edge = (gchar *)"none" }
    };

    CHECK(layout_find(entries, 2, "B") == &entries[1]);
    CHECK(layout_find(entries, 2, "C") == NULL);
    CHECK(layout_find(entries, 2, NULL) == NULL);
    CHECK(layout_find(NULL, 0, "A") == NULL);
}

static void test_apply(void)
{
    App app;

    memset(&app, 0, sizeof(app));
    CHECK(layout_apply(&app, "[{\"output\":\"A\",\"w\":10}]"));
    CHECK_INT_EQ(app.layout_len, 1);
    CHECK_STR_EQ(app.layout[0].output, "A");

    CHECK(!layout_apply(&app, "[{"));
    CHECK_INT_EQ(app.layout_len, 0);
    CHECK(app.layout == NULL);
    app_clear_layout(&app);
}

int main(void)
{
    test_parse_two_entries();
    test_parse_empty();
    test_parse_bozuk();
    test_parse_varsayilanlar();
    test_parse_kacis_ve_bilinmeyen();
    test_find();
    test_apply();

    TEST_MAIN_END();
}
