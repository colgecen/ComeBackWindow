/* shortcuts.c birim testleri */
#include "shortcuts.h"
#include "test_util.h"

static GVariant *int_array(const gint *values, gsize count)
{
    return g_variant_new_fixed_array(G_VARIANT_TYPE_INT32, values, count,
                                     sizeof(gint32));
}

static GVariant *make_reply(const gchar *action, const gchar *component,
                            const gint *defaults, gsize default_count,
                            const gint *current, gsize current_count)
{
    GVariantBuilder array;

    g_variant_builder_init(&array, G_VARIANT_TYPE("a(ssssssaiai)"));
    g_variant_builder_add(&array, "(ssssss@ai@ai)",
                          action, "Etiket", component, "KWin",
                          "default", "Default Context",
                          int_array(defaults, default_count),
                          int_array(current, current_count));
    return g_variant_new("(@a(ssssssaiai))",
                         g_variant_builder_end(&array));
}

static void test_bindings(void)
{
    gsize count = 0;
    const ShortcutBinding *list = shortcuts_bindings(&count);

    CHECK(list != NULL);
    CHECK_INT_EQ(count, 7);
    CHECK_STR_EQ(list[0].action, "cbw-show-desktop");
    CHECK_STR_EQ(list[0].sequence, "Meta+D");
    CHECK(list[0].core);
    CHECK_STR_EQ(list[2].action, "cbw-close");
    CHECK_STR_EQ(list[2].native, "Window Close");
    CHECK(list[2].core);
    CHECK(!list[3].core);
    for (gsize i = 0; i < count; i++) {
        CHECK(list[i].action != NULL);
        CHECK(list[i].label != NULL);
        CHECK(list[i].sequence != NULL);
        CHECK(list[i].key != 0);
    }
}

static void test_parse_and_find(void)
{
    const gint meta_d[] = { 268435524 };
    GVariant *reply = make_reply("Show Desktop", "kwin",
                                 meta_d, 1, meta_d, 1);
    GPtrArray *infos = shortcuts_parse_infos(reply);

    CHECK(infos != NULL);
    CHECK_INT_EQ(infos->len, 1);
    const ShortcutInfo *info = shortcuts_find(infos, "Show Desktop");

    CHECK(info != NULL);
    CHECK_STR_EQ(info->component_unique, "kwin");
    CHECK_STR_EQ(info->context_unique, "default");
    CHECK_INT_EQ(info->current_keys->len, 1);
    CHECK(shortcut_info_has_key(info, 268435524));
    CHECK(!shortcut_info_has_key(info, 150994995));
    CHECK(shortcuts_find(infos, "yok") == NULL);
    CHECK(shortcuts_find(infos, NULL) == NULL);
    CHECK(shortcuts_find(NULL, "Show Desktop") == NULL);
    g_ptr_array_unref(infos);
    g_variant_unref(reply);
}

static void test_parse_invalid(void)
{
    GVariant *wrong = g_variant_new_string("değil");
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ssssssaiai)"));
    GVariant *empty = g_variant_new("(@a(ssssssaiai))",
                                    g_variant_builder_end(&builder));

    CHECK(shortcuts_parse_infos(wrong) == NULL);
    CHECK(shortcuts_parse_infos(NULL) == NULL);
    GPtrArray *infos = shortcuts_parse_infos(empty);

    CHECK(infos != NULL);
    CHECK_INT_EQ(infos->len, 0);
    g_ptr_array_unref(infos);
    g_variant_unref(wrong);
    g_variant_unref(empty);
}

static void test_has_key_defaults(void)
{
    const gint defaults[] = { 1, 2 };
    const gint current[] = { 3 };
    GVariant *reply = make_reply("a", "kwin", defaults, 2, current, 1);
    GPtrArray *infos = shortcuts_parse_infos(reply);
    const ShortcutInfo *info = shortcuts_find(infos, "a");

    CHECK(shortcut_info_has_key(info, 1));
    CHECK(shortcut_info_has_key(info, 3));
    CHECK(!shortcut_info_has_key(info, 4));
    CHECK(!shortcut_info_has_key(NULL, 1));
    g_ptr_array_unref(infos);
    g_variant_unref(reply);
}

static void test_conflicts(void)
{
    gsize count = 0;
    const ShortcutBinding *list = shortcuts_bindings(&count);
    const gint meta_d[] = { 268435524 };
    const gint other[] = { 999 };
    GVariant *reply = make_reply("Show Desktop", "kwin",
                                 meta_d, 1, meta_d, 1);
    GPtrArray *infos = shortcuts_parse_infos(reply);
    const ShortcutInfo *native = shortcuts_find(infos, "Show Desktop");

    CHECK(shortcuts_binding_conflicts(&list[0], native));
    CHECK(!shortcuts_binding_conflicts(&list[0], NULL));
    CHECK(!shortcuts_binding_conflicts(NULL, native));
    g_ptr_array_unref(infos);
    g_variant_unref(reply);

    reply = make_reply("Show Desktop", "kwin", other, 1, other, 1);
    infos = shortcuts_parse_infos(reply);
    native = shortcuts_find(infos, "Show Desktop");
    CHECK(!shortcuts_binding_conflicts(&list[0], native));
    g_ptr_array_unref(infos);
    g_variant_unref(reply);
}

static void test_key_string(void)
{
    GArray *keys = g_array_new(FALSE, FALSE, sizeof(gint));
    gint values[] = { 268435524, 150994995, 7 };

    g_array_append_vals(keys, values, 3);
    gchar *text = shortcuts_keys_to_string(keys);

    CHECK_STR_EQ(text, "268435524;150994995;7");
    GArray *parsed = g_array_new(FALSE, FALSE, sizeof(gint));

    CHECK(shortcuts_string_to_keys(text, parsed));
    CHECK_INT_EQ(parsed->len, 3);
    CHECK_INT_EQ(g_array_index(parsed, gint, 1), 150994995);
    gchar *again = shortcuts_keys_to_string(parsed);

    CHECK_STR_EQ(again, text);
    g_array_set_size(parsed, 0);
    CHECK(shortcuts_string_to_keys("", parsed));
    CHECK_INT_EQ(parsed->len, 0);

    CHECK(!shortcuts_string_to_keys("12;abc", parsed));
    CHECK_INT_EQ(parsed->len, 0);
    CHECK(!shortcuts_string_to_keys("12;;13", parsed));
    CHECK(!shortcuts_string_to_keys("12;", parsed));
    CHECK_INT_EQ(parsed->len, 0);

    GArray *empty = g_array_new(FALSE, FALSE, sizeof(gint));
    gchar *empty_text = shortcuts_keys_to_string(empty);

    CHECK_STR_EQ(empty_text, "");
    g_free(empty_text);
    g_array_unref(empty);
    g_free(again);
    g_array_unref(parsed);
    g_free(text);
    g_array_unref(keys);
}

int main(void)
{
    test_bindings();
    test_parse_and_find();
    test_parse_invalid();
    test_has_key_defaults();
    test_conflicts();
    test_key_string();

    TEST_MAIN_END();
}
