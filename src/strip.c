/*
 * Masaüstünü Göster şeridi
 *
 * Ortak geometri hesabı, çizim ve arka uç seçimi. Şerit, panelin masaüstü
 * kenarındaki ucunda ince bir çubuktur; panel yoksa ekranın sağ alt köşesine
 * yedek yükseklikle yerleşir.
 */
#include "strip.h"

#include "strip_internal.h"

#include <math.h>

StripEdge strip_edge_from_string(const gchar *edge)
{
    if (edge == NULL) {
        return STRIP_EDGE_NONE;
    }
    if (g_strcmp0(edge, "bottom") == 0) {
        return STRIP_EDGE_BOTTOM;
    }
    if (g_strcmp0(edge, "top") == 0) {
        return STRIP_EDGE_TOP;
    }
    if (g_strcmp0(edge, "left") == 0) {
        return STRIP_EDGE_LEFT;
    }
    if (g_strcmp0(edge, "right") == 0) {
        return STRIP_EDGE_RIGHT;
    }
    return STRIP_EDGE_NONE;
}

gboolean strip_geometry_for(const LayoutEntry *entry, gint strip_width,
                            StripGeometry *out)
{
    g_return_val_if_fail(out != NULL, FALSE);

    memset(out, 0, sizeof(*out));
    if (entry == NULL) {
        return FALSE;
    }
    StripEdge edge = strip_edge_from_string(entry->edge);

    if (edge == STRIP_EDGE_NONE) {
        edge = STRIP_EDGE_BOTTOM;
    }
    if (!strip_geometry_calc(edge, entry->thickness, strip_width,
                             entry->scale > 0 ? entry->scale : 1.0, out)) {
        return FALSE;
    }
    /*
     * Şerit her zaman ekranın köşesine yapışır: panel varsa kalınlığı panel
     * kalınlığıdır ve panelin en ucundaki dilimi kaplar (Windows'taki
     * "Masaüstünü Göster" düğmesi gibi).
     */
    return TRUE;
}

gboolean strip_geometry_calc(StripEdge edge, gint thickness_in,
                             gint strip_width, gdouble scale,
                             StripGeometry *out)
{
    g_return_val_if_fail(out != NULL, FALSE);

    memset(out, 0, sizeof(*out));
    if (strip_width <= 0) {
        return FALSE;
    }
    gint thickness = thickness_in > 0 ? thickness_in : 44;

    if (scale <= 0) {
        scale = 1.0;
    }
    if (edge == STRIP_EDGE_NONE) {
        /* Panel yok: sağ alt köşede yedek yükseklik. */
        edge = STRIP_EDGE_BOTTOM;
    }
    out->edge = edge;
    switch (edge) {
    case STRIP_EDGE_BOTTOM:
        out->width = strip_width;
        out->height = thickness;
        out->anchor = STRIP_ANCHOR_BOTTOM | STRIP_ANCHOR_RIGHT;
        break;
    case STRIP_EDGE_TOP:
        out->width = strip_width;
        out->height = thickness;
        out->anchor = STRIP_ANCHOR_TOP | STRIP_ANCHOR_RIGHT;
        break;
    case STRIP_EDGE_LEFT:
        out->width = thickness;
        out->height = strip_width;
        out->anchor = STRIP_ANCHOR_LEFT | STRIP_ANCHOR_BOTTOM;
        break;
    case STRIP_EDGE_RIGHT:
        out->width = thickness;
        out->height = strip_width;
        out->anchor = STRIP_ANCHOR_RIGHT | STRIP_ANCHOR_BOTTOM;
        break;
    case STRIP_EDGE_NONE:
    default:
        return FALSE;
    }
    out->buffer_width = (gint)ceil(out->width * scale);
    out->buffer_height = (gint)ceil(out->height * scale);
    if (out->buffer_width < 1 || out->buffer_height < 1) {
        return FALSE;
    }
    return TRUE;
}


/* Masaüstüne bakan yön: sağdaki şerit sola, alttaki yukarı vb. */
static gdouble arrow_direction(StripEdge edge)
{
    switch (edge) {
    case STRIP_EDGE_LEFT:
        return 0;               /* sağa */
    case STRIP_EDGE_RIGHT:
        return M_PI;            /* sola */
    case STRIP_EDGE_TOP:
        return M_PI / 2;        /* aşağı */
    case STRIP_EDGE_BOTTOM:
    case STRIP_EDGE_NONE:
    default:
        return -M_PI / 2;       /* yukarı */
    }
}

/* --- Yazı modu -------------------------------------------------------- */

static void text_font_px(cairo_t *cr, gint px)
{
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, px);
}

gint strip_effective_width(gboolean text_mode, gdouble opacity,
                           gint strip_width)
{
    gint width = strip_width > 0 ? strip_width : 10;

    if (text_mode && opacity <= 0.0) {
        return width * 2;
    }
    return width;
}

gboolean strip_color_parse(const gchar *hex, gdouble *r, gdouble *g,
                           gdouble *b)
{
    if (hex == NULL || strlen(hex) != 7 || hex[0] != '#') {
        return FALSE;
    }
    guint value = 0;

    for (gint i = 1; i < 7; i++) {
        if (!g_ascii_isxdigit(hex[i])) {
            return FALSE;
        }
        value = value * 16 + (guint)g_ascii_xdigit_value(hex[i]);
    }
    if (r != NULL) {
        *r = ((value >> 16) & 0xff) / 255.0;
    }
    if (g != NULL) {
        *g = ((value >> 8) & 0xff) / 255.0;
    }
    if (b != NULL) {
        *b = (value & 0xff) / 255.0;
    }
    return TRUE;
}

void strip_style_init(StripStyle *style, gboolean text_mode, const gchar *text,
                      const gchar *bar_hex, const gchar *text_hex,
                      gdouble opacity, gdouble text_opacity)
{
    g_return_if_fail(style != NULL);

    memset(style, 0, sizeof(*style));
    style->text_mode = text_mode;
    style->text = text;
    if (!strip_color_parse(bar_hex, &style->bar_r, &style->bar_g,
                           &style->bar_b)) {
        style->bar_r = 0.13;
        style->bar_g = 0.50;
        style->bar_b = 0.72;
    }
    if (!strip_color_parse(text_hex, &style->text_r, &style->text_g,
                           &style->text_b)) {
        style->text_r = 1.0;
        style->text_g = 1.0;
        style->text_b = 1.0;
    }
    style->opacity = CLAMP(opacity, 0.0, 1.0);
    style->text_opacity = CLAMP(text_opacity, 0.0, 1.0);
}

/*
 * Dikey yazı: şerit çubukla aynı boyuttadır, metin -90° döndürülüp
 * ortalanır. Metin her zaman tam sığar (gerekirse yatay sıkıştırılır);
 * hover'da zemin tamamen saydam olur, yalnız yazı kalır.
 */
static void draw_text_strip(cairo_t *cr, gint width, gint height,
                            gboolean hover, const StripStyle *style)
{
    if (!hover && style->opacity > 0.0) {
        cairo_set_source_rgba(cr, style->bar_r, style->bar_g, style->bar_b,
                              style->opacity);
        cairo_rectangle(cr, 0, 0, width, height);
        cairo_fill(cr);
    }
    if (style->text == NULL || *style->text == '\0'
        || style->text_opacity <= 0.0) {
        return;
    }
    /*
     * Yazıyı bozmadan sığdır: fontu, hem şerit genişliğine (dikey kalınlık)
     * hem de uzunluğa sığan en büyük boya kadar küçült. Böylece harfler ne
     * kenardan kesilir ne de yatay sıkıştırılıp bozulur.
     */
    gdouble max_len = height - 8.0;
    gdouble max_thick = width - 1.0;
    gint font = MAX(5, (gint)floor(width * 0.85));
    cairo_text_extents_t extents;

    /* Font yalnızca şerit kalınlığına sığacak kadar küçültülür. */
    for (;;) {
        text_font_px(cr, font);
        cairo_text_extents(cr, style->text, &extents);
        if (extents.height <= max_thick || font <= 5) {
            break;
        }
        font--;
    }
    /* Uzun metin kenarlardan taşmasın diye yalnız gerekirse sıkıştırılır. */
    gdouble fit = 1.0;

    if (extents.x_advance > max_len && extents.x_advance > 0) {
        fit = max_len / extents.x_advance;
    }
    /*
     * Gövde tamamen saydamsa (yalnız yazı) metin sağ alt köşeye yapışır;
     * aksi hâlde şeridin ortasında durur.
     */
    gboolean corner = style->opacity <= 0.0;

    cairo_save(cr);
    if (corner) {
        cairo_translate(cr, width - 2.0, height - 2.0);
    } else {
        cairo_translate(cr, width / 2.0, height / 2.0);
    }
    cairo_rotate(cr, -M_PI / 2.0);
    cairo_scale(cr, fit, 1.0);
    gdouble x = corner ? -extents.x_bearing
                       : -extents.x_bearing - extents.width / 2.0;
    gdouble y = corner ? 0.0
                       : -(extents.y_bearing + extents.height / 2.0);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.55 * style->text_opacity);
    cairo_move_to(cr, x + 1.0, y + 1.0);
    cairo_show_text(cr, style->text);
    cairo_set_source_rgba(cr, style->text_r, style->text_g, style->text_b,
                          style->text_opacity);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, style->text);
    cairo_restore(cr);
}

void strip_draw(cairo_t *cr, gint width, gint height, StripEdge edge,
                gboolean hover, gboolean peek_active,
                const StripStyle *style)
{
    if (cr == NULL || width <= 0 || height <= 0) {
        return;
    }
    StripStyle fallback = {
        .text_mode = FALSE, .text = NULL,
        .bar_r = 0.13, .bar_g = 0.50, .bar_b = 0.72,
        .text_r = 1.0, .text_g = 1.0, .text_b = 1.0,
        .opacity = 0.9, .text_opacity = 1.0
    };

    if (style == NULL) {
        style = &fallback;
    }
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    if (style->text_mode) {
        draw_text_strip(cr, width, height, hover, style);
        return;
    }
    gboolean vertical = width < height;
    gdouble alpha = CLAMP(style->opacity, 0.0, 1.0);

    if ((hover || peek_active) && alpha < 1.0) {
        alpha = MIN(1.0, alpha + 0.15);
    }
    if (alpha <= 0.0) {
        return;
    }
    cairo_set_source_rgba(cr, style->bar_r, style->bar_g, style->bar_b,
                          alpha);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    /* Masaüstüne bakan iç kenarda yazı rengiyle çizgi. */
    cairo_set_source_rgba(cr, style->text_r, style->text_g, style->text_b,
                          0.85 * MIN(1.0, style->text_opacity + 0.35));
    cairo_set_line_width(cr, 1.5);
    if (vertical) {
        cairo_move_to(cr, 0.75, 1.0);
        cairo_line_to(cr, 0.75, height - 1.0);
    } else {
        cairo_move_to(cr, 1.0, 0.75);
        cairo_line_to(cr, width - 1.0, 0.75);
    }
    cairo_stroke(cr);

    gdouble size = fmin(width, height) * 0.34;

    if (size < 3.0) {
        return;
    }
    cairo_save(cr);
    cairo_translate(cr, width / 2.0, height / 2.0);
    cairo_rotate(cr, arrow_direction(edge));
    cairo_set_source_rgba(cr, style->text_r, style->text_g, style->text_b,
                          style->text_opacity);
    cairo_set_line_width(cr, fmax(1.6, size * 0.30));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, -size / 2.0, -size / 2.0);
    cairo_line_to(cr, size / 2.0, 0);
    cairo_line_to(cr, -size / 2.0, size / 2.0);
    cairo_stroke(cr);
    cairo_restore(cr);
}

void strip_set_callbacks(Strip *strip, const StripCallbacks *callbacks,
                         gpointer user_data)
{
    if (strip == NULL) {
        return;
    }
    strip->callbacks = callbacks != NULL ? *callbacks : (StripCallbacks){ 0 };
    strip->callbacks_data = user_data;
}

void strip_notify_hover(Strip *strip, gboolean inside)
{
    if (strip == NULL || strip->hover == inside) {
        return;
    }
    strip->hover = inside;
    if (strip->callbacks.hover != NULL) {
        strip->callbacks.hover(strip, inside, strip->callbacks_data);
    }
}

void strip_notify_click(Strip *strip)
{
    if (strip == NULL) {
        return;
    }
    if (strip->callbacks.click != NULL) {
        strip->callbacks.click(strip, strip->callbacks_data);
    }
}

void strip_notify_press(Strip *strip)
{
    if (strip == NULL) {
        return;
    }
    if (strip->callbacks.press != NULL) {
        strip->callbacks.press(strip, strip->callbacks_data);
    }
}

Strip *strip_create(App *app, GError **error)
{
    g_return_val_if_fail(app != NULL, NULL);

    if (g_getenv("WAYLAND_DISPLAY") != NULL) {
        Strip *wl = strip_wayland_create(app, error);

        if (wl != NULL) {
            app->strip = wl;
            return wl;
        }
        app_warn(app, "Wayland şeridi açılamadı, X11 deneniyor");
        g_clear_error(error);
    }
    if (g_getenv("DISPLAY") != NULL) {
        Strip *x11 = strip_x11_create(app, error);

        if (x11 != NULL) {
            app->strip = x11;
        }
        return x11;
    }
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                        "Ne Wayland ne X11 oturumu var");
    return NULL;
}

void strip_update(Strip *strip)
{
    if (strip == NULL) {
        return;
    }
    if (strip->wayland) {
        strip_wayland_update(strip);
    } else {
        strip_x11_update(strip);
    }
}

void strip_refresh(Strip *strip)
{
    strip_update(strip);
}

void strip_set_peek_active(Strip *strip, gboolean active)
{
    if (strip == NULL || strip->peek_active == active) {
        return;
    }
    strip->peek_active = active;
    strip_refresh(strip);
}

Strip *strip_ref(Strip *strip)
{
    if (strip != NULL) {
        g_atomic_int_inc(&strip->refs);
    }
    return strip;
}

void strip_unref(Strip *strip)
{
    if (strip != NULL && g_atomic_int_dec_and_test(&strip->refs)) {
        g_free(strip);
    }
}

void strip_destroy(Strip *strip)
{
    if (strip == NULL || strip->destroyed) {
        return;
    }
    strip->destroyed = TRUE;
    if (strip->wayland) {
        strip_wayland_destroy(strip);
    } else {
        strip_x11_destroy(strip);
    }
    if (strip->app != NULL && strip->app->strip == strip) {
        strip->app->strip = NULL;
    }
    strip_unref(strip);
}
