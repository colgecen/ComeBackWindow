/* tray_menu.c birim testleri */
#include "test_util.h"
#include "tray_menu.h"

static void test_items(void)
{
    gsize count = 0;
    const MenuItemSpec *items = tray_menu_items(&count);

    CHECK(items != NULL);
    CHECK(count >= 8);
    gint normal = 0;
    gint separator = 0;

    for (gsize i = 0; i < count; i++) {
        if (items[i].type == MENU_ITEM_SEPARATOR) {
            separator++;
            CHECK(items[i].label == NULL);
        } else {
            normal++;
            CHECK(items[i].label != NULL);
            CHECK(items[i].action != NULL);
        }
    }
    CHECK_INT_EQ(normal, 7);
    CHECK_INT_EQ(separator, 1);
}

static void test_action_lookup(void)
{
    CHECK_STR_EQ(tray_menu_action(1), "toggle-desktop");
    CHECK_STR_EQ(tray_menu_action(2), "minimize-others");
    CHECK_STR_EQ(tray_menu_action(3), "restore-all");
    CHECK_STR_EQ(tray_menu_action(5), "settings");
    CHECK_STR_EQ(tray_menu_action(8), "quit");
    CHECK(tray_menu_action(4) == NULL);
    CHECK(tray_menu_action(999) == NULL);
}

static void test_layout_shape(void)
{
    gsize count = 0;
    const MenuItemSpec *items = tray_menu_items(&count);
    GVariant *layout = tray_menu_layout(items, count, 7, 0, -1);

    CHECK(layout != NULL);
    CHECK(g_variant_is_of_type(layout,
                               G_VARIANT_TYPE("(u(ia{sv}av))")));
    guint revision = 0;
    GVariant *node = NULL;

    g_variant_get(layout, "(u@(ia{sv}av))", &revision, &node);
    CHECK_INT_EQ(revision, 7);
    gint id = -1;
    GVariant *props = NULL;
    GVariant *children = NULL;

    g_variant_get(node, "(i@a{sv}@av)", &id, &props, &children);
    CHECK_INT_EQ(id, 0);
    CHECK(g_variant_is_of_type(children, G_VARIANT_TYPE("av")));
    CHECK_INT_EQ(g_variant_n_children(children), (gssize)count);

    /* İlk öğe: kimlik + etiket + çocuksuz dizi */
    GVariant *first_v = g_variant_get_child_value(children, 0);
    GVariant *first = g_variant_get_variant(first_v);
    GVariant *first_props = NULL;
    GVariant *first_children = NULL;

    g_variant_get(first, "(i@a{sv}@av)", &id, &first_props,
                  &first_children);
    CHECK_INT_EQ(id, 1);
    const gchar *label = NULL;

    CHECK(g_variant_lookup(first_props, "label", "&s", &label));
    CHECK_STR_EQ(label, "Masaüstünü Göster");
    CHECK_INT_EQ(g_variant_n_children(first_children), 0);
    g_variant_unref(first_v);
    g_variant_unref(first);
    g_variant_unref(first_props);
    g_variant_unref(first_children);
    g_variant_unref(node);
    g_variant_unref(props);
    g_variant_unref(children);
    g_variant_unref(layout);
}

static void test_layout_separator(void)
{
    gsize count = 0;
    const MenuItemSpec *items = tray_menu_items(&count);
    GVariant *layout = tray_menu_layout(items, count, 1, 0, -1);
    GVariant *node = NULL;

    g_variant_get(layout, "(u@(ia{sv}av))", NULL, &node);
    GVariant *children = NULL;
    GVariant *props = NULL;
    gint id = 0;

    g_variant_get(node, "(i@a{sv}@av)", &id, &props, &children);
    for (gsize i = 0; i < count; i++) {
        GVariant *child_v = g_variant_get_child_value(children, i);
        GVariant *child = g_variant_get_variant(child_v);
        GVariant *cprops = NULL;

        g_variant_get(child, "(i@a{sv}@av)", &id, &cprops, NULL);
        if (items[i].type == MENU_ITEM_SEPARATOR) {
            const gchar *type = NULL;

            CHECK(g_variant_lookup(cprops, "type", "&s", &type));
            CHECK_STR_EQ(type, "separator");
            CHECK(!g_variant_lookup(cprops, "label", "&s", NULL));
        } else {
            CHECK(!g_variant_lookup(cprops, "type", "&s", NULL));
        }
        g_variant_unref(child_v);
        g_variant_unref(child);
        g_variant_unref(cprops);
    }
    g_variant_unref(children);
    g_variant_unref(props);
    g_variant_unref(node);
    g_variant_unref(layout);
}

static void test_layout_depth_zero(void)
{
    gsize count = 0;
    const MenuItemSpec *items = tray_menu_items(&count);
    GVariant *layout = tray_menu_layout(items, count, 1, 0, 0);
    GVariant *node = NULL;
    GVariant *children = NULL;

    g_variant_get(layout, "(u@(ia{sv}av))", NULL, &node);
    g_variant_get(node, "(i@a{sv}@av)", NULL, NULL, &children);
    CHECK_INT_EQ(g_variant_n_children(children), 0);
    g_variant_unref(children);
    g_variant_unref(node);
    g_variant_unref(layout);
}

static void test_layout_missing_parent(void)
{
    gsize count = 0;
    const MenuItemSpec *items = tray_menu_items(&count);
    GVariant *layout = tray_menu_layout(items, count, 1, 5, -1);
    GVariant *node = NULL;
    GVariant *children = NULL;

    g_variant_get(layout, "(u@(ia{sv}av))", NULL, &node);
    g_variant_get(node, "(i@a{sv}@av)", NULL, NULL, &children);
    CHECK_INT_EQ(g_variant_n_children(children), 0);
    g_variant_unref(children);
    g_variant_unref(node);
    g_variant_unref(layout);
}

int main(void)
{
    test_items();
    test_action_lookup();
    test_layout_shape();
    test_layout_separator();
    test_layout_depth_zero();
    test_layout_missing_parent();

    TEST_MAIN_END();
}
