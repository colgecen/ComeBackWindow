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
    return strip_geometry_calc(edge, entry->thickness, strip_width,
                               entry->scale > 0 ? entry->scale : 1.0, out);
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

void strip_draw(cairo_t *cr, gint width, gint height, StripEdge edge,
                gboolean hover, gboolean peek_active)
{
    if (cr == NULL || width <= 0 || height <= 0) {
        return;
    }
    gdouble alpha = 0.45;

    if (hover) {
        alpha = 0.75;
    }
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    /* Arka plan */
    cairo_set_source_rgba(cr, 0.10, 0.10, 0.12, alpha);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    /* Masaüstüne bakan ince turuncu çizgi */
    cairo_set_source_rgba(cr, 0.91, 0.35, 0.09, peek_active ? 1.0 : 0.8);
    cairo_set_line_width(cr, 1.5);
    switch (edge) {
    case STRIP_EDGE_LEFT:
        cairo_move_to(cr, width - 0.75, 0);
        cairo_line_to(cr, width - 0.75, height);
        break;
    case STRIP_EDGE_RIGHT:
        cairo_move_to(cr, 0.75, 0);
        cairo_line_to(cr, 0.75, height);
        break;
    case STRIP_EDGE_TOP:
        cairo_move_to(cr, 0, height - 0.75);
        cairo_line_to(cr, width, height - 0.75);
        break;
    default:
        cairo_move_to(cr, 0, 0.75);
        cairo_line_to(cr, width, 0.75);
        break;
    }
    cairo_stroke(cr);

    /* Ok (chevron) */
    gdouble cx = width / 2.0;
    gdouble cy = height / 2.0;
    gdouble size = fmin(width, height) * 0.34;

    if (size < 2.0) {
        return;
    }
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_rotate(cr, arrow_direction(edge));
    cairo_set_source_rgba(cr, 0.95, 0.47, 0.15, peek_active ? 1.0 : 0.9);
    cairo_set_line_width(cr, fmax(1.4, size * 0.28));
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
