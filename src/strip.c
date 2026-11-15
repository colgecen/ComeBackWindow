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

/* Metni `max_width`e sığacak biçimde kırpar, sonuna üç nokta ekler. */
static gchar *fit_text(cairo_t *cr, const gchar *text, gdouble max_width)
{
    glong len = g_utf8_strlen(text, -1);

    for (glong keep = len - 1; keep > 0; keep--) {
        gchar *prefix = g_utf8_substring(text, 0, keep);
        gchar *candidate = g_strconcat(prefix, "…", NULL);
        cairo_text_extents_t extents;

        g_free(prefix);
        cairo_text_extents(cr, candidate, &extents);
        if (extents.x_advance <= max_width) {
            return candidate;
        }
        g_free(candidate);
    }
    return g_strdup("…");
}

static void draw_text_strip(cairo_t *cr, gint width, gint height,
                            gboolean hover, const gchar *text)
{
    if (!hover) {
        cairo_set_source_rgba(cr, 0.13, 0.50, 0.72, 0.90);
        cairo_rectangle(cr, 0, 0, width, height);
        cairo_fill(cr);
    }
    if (text == NULL || *text == '\0') {
        return;
    }
    gint font = MAX(7, (gint)floor(width * 0.82));
    gdouble max_len = height - 10.0;

    text_font_px(cr, font);
    cairo_text_extents_t extents;

    cairo_text_extents(cr, text, &extents);
    while (extents.x_advance > max_len && font > 7) {
        font--;
        text_font_px(cr, font);
        cairo_text_extents(cr, text, &extents);
    }
    gchar *fitted = NULL;

    if (extents.x_advance > max_len) {
        fitted = fit_text(cr, text, max_len);
        text = fitted;
        cairo_text_extents(cr, text, &extents);
    }
    cairo_save(cr);
    cairo_translate(cr, width / 2.0, height / 2.0);
    cairo_rotate(cr, -M_PI / 2.0);
    gdouble x = -extents.x_bearing - extents.width / 2.0;
    gdouble y = -(extents.y_bearing + extents.height / 2.0);

    cairo_set_source_rgba(cr, 0, 0, 0, hover ? 0.85 : 0.55);
    cairo_move_to(cr, x + 1.0, y + 1.0);
    cairo_show_text(cr, text);
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
    cairo_restore(cr);
    g_free(fitted);
}

void strip_draw(cairo_t *cr, gint width, gint height, StripEdge edge,
                gboolean hover, gboolean peek_active, gboolean text_mode,
                const gchar *text)
{
    if (cr == NULL || width <= 0 || height <= 0) {
        return;
    }
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    if (text_mode && text != NULL && *text != '\0') {
        draw_text_strip(cr, width, height, hover, text);
        return;
    }
    gboolean vertical = width < height;
    gdouble tint = peek_active ? 0.40 : (hover ? 0.28 : 0.0);

    /* Mavi gövde: panel üzerinde de görünür yarı saydam şerit. */
    cairo_set_source_rgba(cr, 0.13, 0.50, 0.72, hover ? 0.92 : 0.95);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    /* Masaüstüne bakan iç kenarda açık mavi çizgi. */
    cairo_set_source_rgba(cr, 0.55, 0.85, 0.99,
                          (hover || peek_active) ? 1.0 : 0.85);
    cairo_set_line_width(cr, 1.5);
    if (vertical) {
        cairo_move_to(cr, 0.75, 1.0);
        cairo_line_to(cr, 0.75, height - 1.0);
    } else {
        cairo_move_to(cr, 1.0, 0.75);
        cairo_line_to(cr, width - 1.0, 0.75);
    }
    cairo_stroke(cr);

    /* Peek/hover sırasında aydınlanma. */
    if (tint > 0.0) {
        cairo_set_source_rgba(cr, 0.35, 0.75, 0.96, tint);
        cairo_rectangle(cr, 0, 0, width, height);
        cairo_fill(cr);
    }

    /*
     * Ok işareti yalnızca şerit yeterince kalınsa çizilir; varsayılan 10 px
     * genişlikte küçük ok görünür, ince şeritte çizgi yeterli ipucu verir.
     */
    gdouble size = fmin(width, height) * 0.34;

    if (size < 3.0) {
        return;
    }
    cairo_save(cr);
    cairo_translate(cr, width / 2.0, height / 2.0);
    cairo_rotate(cr, arrow_direction(edge));
    cairo_set_source_rgba(cr, 0.45, 0.80, 0.97, 1.0);
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
