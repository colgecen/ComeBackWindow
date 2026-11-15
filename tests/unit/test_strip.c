/* strip.c geometri testleri */
#include "strip.h"
#include "test_util.h"

static void test_edge_parse(void)
{
    CHECK(strip_edge_from_string("bottom") == STRIP_EDGE_BOTTOM);
    CHECK(strip_edge_from_string("top") == STRIP_EDGE_TOP);
    CHECK(strip_edge_from_string("left") == STRIP_EDGE_LEFT);
    CHECK(strip_edge_from_string("right") == STRIP_EDGE_RIGHT);
    CHECK(strip_edge_from_string("none") == STRIP_EDGE_NONE);
    CHECK(strip_edge_from_string("çapraz") == STRIP_EDGE_NONE);
    CHECK(strip_edge_from_string(NULL) == STRIP_EDGE_NONE);
}

static void test_geometry_bottom(void)
{
    StripGeometry geo;

    CHECK(strip_geometry_calc(STRIP_EDGE_BOTTOM, 44, 6, 1.0, &geo));
    CHECK(geo.edge == STRIP_EDGE_BOTTOM);
    CHECK_INT_EQ(geo.width, 6);
    CHECK_INT_EQ(geo.height, 44);
    CHECK_INT_EQ(geo.anchor, STRIP_ANCHOR_BOTTOM | STRIP_ANCHOR_RIGHT);
    CHECK_INT_EQ(geo.buffer_width, 6);
    CHECK_INT_EQ(geo.buffer_height, 44);
    CHECK_INT_EQ(geo.margin_bottom, 0);
}

static void test_geometry_top(void)
{
    StripGeometry geo;

    CHECK(strip_geometry_calc(STRIP_EDGE_TOP, 32, 8, 1.0, &geo));
    CHECK_INT_EQ(geo.anchor, STRIP_ANCHOR_TOP | STRIP_ANCHOR_RIGHT);
    CHECK_INT_EQ(geo.width, 8);
    CHECK_INT_EQ(geo.height, 32);
}

static void test_geometry_vertical(void)
{
    StripGeometry geo;

    CHECK(strip_geometry_calc(STRIP_EDGE_LEFT, 32, 6, 1.0, &geo));
    CHECK_INT_EQ(geo.width, 32);
    CHECK_INT_EQ(geo.height, 6);
    CHECK_INT_EQ(geo.anchor, STRIP_ANCHOR_LEFT | STRIP_ANCHOR_BOTTOM);

    CHECK(strip_geometry_calc(STRIP_EDGE_RIGHT, 40, 10, 1.0, &geo));
    CHECK_INT_EQ(geo.width, 40);
    CHECK_INT_EQ(geo.height, 10);
    CHECK_INT_EQ(geo.anchor, STRIP_ANCHOR_RIGHT | STRIP_ANCHOR_BOTTOM);
}

static void test_geometry_scale(void)
{
    StripGeometry geo;

    CHECK(strip_geometry_calc(STRIP_EDGE_BOTTOM, 44, 6, 1.5, &geo));
    CHECK_INT_EQ(geo.width, 6);
    CHECK_INT_EQ(geo.height, 44);
    CHECK_INT_EQ(geo.buffer_width, 9);
    CHECK_INT_EQ(geo.buffer_height, 66);
}

static void test_geometry_none_ve_sifir(void)
{
    StripGeometry geo;

    /* Panel yok: alt kenara düşer, yedek kalınlık kullanılır. */
    CHECK(strip_geometry_calc(STRIP_EDGE_NONE, 0, 6, 0, &geo));
    CHECK(geo.edge == STRIP_EDGE_BOTTOM);
    CHECK_INT_EQ(geo.height, 44);
    CHECK(geo.buffer_height == 44);

    /* Geçersiz genişlik */
    CHECK(!strip_geometry_calc(STRIP_EDGE_BOTTOM, 44, 0, 1.0, &geo));
    CHECK(!strip_geometry_calc(STRIP_EDGE_BOTTOM, 44, -3, 1.0, &geo));
}

static void test_geometry_from_entry(void)
{
    LayoutEntry entry = {
        .output = (gchar *)"eDP-1",
        .edge = (gchar *)"bottom",
        .thickness = 44,
        .scale = 2.0
    };
    StripGeometry geo;

    CHECK(strip_geometry_for(&entry, 6, &geo));
    CHECK_INT_EQ(geo.width, 6);
    CHECK_INT_EQ(geo.height, 44);
    CHECK_INT_EQ(geo.buffer_width, 12);
    CHECK_INT_EQ(geo.buffer_height, 88);
    CHECK(!strip_geometry_for(NULL, 6, &geo));

    entry.edge = (gchar *)"none";
    entry.thickness = 0;
    CHECK(strip_geometry_for(&entry, 6, &geo));
    CHECK(geo.edge == STRIP_EDGE_BOTTOM);
    CHECK_INT_EQ(geo.height, 44);
}

static void test_color_parse(void)
{
    gdouble r = -1, g = -1, b = -1;

    CHECK(strip_color_parse("#1f80c4", &r, &g, &b));
    CHECK(r > 0.11 && r < 0.13);
    CHECK(g > 0.49 && g < 0.51);
    CHECK(b > 0.76 && b < 0.78);
    CHECK(!strip_color_parse("mavi", &r, &g, &b));
    CHECK(!strip_color_parse("#12345", &r, &g, &b));
    CHECK(!strip_color_parse("#gggggg", &r, &g, &b));

    StripStyle style;

    strip_style_init(&style, TRUE, "Masaüstü", "bozuk", "#123456", 2.0, -1.0);
    CHECK(style.text_mode == TRUE);
    CHECK_STR_EQ(style.text, "Masaüstü");
    CHECK(style.bar_r > 0.12 && style.bar_r < 0.14);   /* varsayılan mavi */
    CHECK(style.text_r > 0.06 && style.text_r < 0.08);
    CHECK(style.opacity > 0.99);
    CHECK(style.text_opacity < 0.01);
}

int main(void)
{
    test_edge_parse();
    test_geometry_bottom();
    test_geometry_top();
    test_geometry_vertical();
    test_geometry_scale();
    test_geometry_none_ve_sifir();
    test_geometry_from_entry();
    test_color_parse();

    TEST_MAIN_END();
}
