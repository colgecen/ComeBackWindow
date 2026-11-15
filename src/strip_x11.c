/*
 * Masaüstünü Göster şeridi — X11 arka ucu
 *
 * Yalnızca layer-shell bulunmayan oturumlarda (saf X11 veya XWayland yedeği)
 * kullanılır. Şerit, override-redirect bir pencere olarak çizilir; böylece
 * pencere yöneticisi onu yönetmez ve panelin üstünde kalır.
 */
#include "strip_internal.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo-xlib.h>
#include <errno.h>
#include <glib-unix.h>

typedef struct {
    Window window;
    StripGeometry geo;
    gint x;
    gint y;
    gboolean hover;
} X11Surface;

typedef struct {
    Strip *strip;
    Display *display;
    gint screen;
    Window root;
    Atom opacity_atom;
    Atom wm_type_atom;
    Atom dock_atom;
    GPtrArray *surfaces;      /* X11Surface* */
    guint fd_source;
    gint ref_count;
} StripX11;

/* --- Yardımcılar -------------------------------------------------- */

static void x11_surface_free(StripX11 *sx, X11Surface *xs)
{
    if (xs->window != None) {
        XDestroyWindow(sx->display, xs->window);
    }
    g_free(xs);
}

static void x11_surfaces_clear(StripX11 *sx)
{
    for (guint i = 0; i < sx->surfaces->len; i++) {
        x11_surface_free(sx, g_ptr_array_index(sx->surfaces, i));
    }
    g_ptr_array_set_size(sx->surfaces, 0);
}

static void x11_surface_draw(StripX11 *sx, X11Surface *xs)
{
    cairo_surface_t *cs = cairo_xlib_surface_create(
        sx->display, xs->window, DefaultVisual(sx->display, sx->screen),
        xs->geo.width, xs->geo.height);

    if (cairo_surface_status(cs) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(cs);
        return;
    }
    cairo_t *cr = cairo_create(cs);

    const Settings *settings = &sx->strip->app->settings;
    StripStyle style;

    strip_style_init(&style, g_strcmp0(settings->strip_style, "text") == 0,
                     settings->strip_text, settings->strip_color,
                     settings->strip_text_color, settings->strip_opacity,
                     settings->strip_text_opacity);
    strip_draw(cr, xs->geo.width, xs->geo.height, xs->geo.edge, xs->hover,
               sx->strip->peek_active, &style);
    cairo_destroy(cr);
    cairo_surface_flush(cs);
    cairo_surface_destroy(cs);
    XFlush(sx->display);
}

/* Düzen girdisinden ekran üzerindeki konumu hesaplar. */
static void x11_surface_place(X11Surface *xs, const LayoutEntry *entry)
{
    switch (xs->geo.edge) {
    case STRIP_EDGE_TOP:
        xs->x = entry->x + entry->w - xs->geo.width;
        xs->y = entry->y;
        break;
    case STRIP_EDGE_LEFT:
        xs->x = entry->x;
        xs->y = entry->y + entry->h - xs->geo.height;
        break;
    case STRIP_EDGE_RIGHT:
        xs->x = entry->x + entry->w - xs->geo.width;
        xs->y = entry->y + entry->h - xs->geo.height;
        break;
    case STRIP_EDGE_BOTTOM:
    case STRIP_EDGE_NONE:
    default:
        xs->x = entry->x + entry->w - xs->geo.width;
        xs->y = entry->y + entry->h - xs->geo.height;
        break;
    }
    xs->x += xs->geo.margin_left - xs->geo.margin_right;
    xs->y += xs->geo.margin_top - xs->geo.margin_bottom;
}

static Atom intern_atom(Display *display, const gchar *name)
{
    return XInternAtom(display, name, False);
}

static gboolean x11_geometry(StripX11 *sx, const LayoutEntry *entry,
                             gint strip_width, StripGeometry *geo)
{
    (void)sx;
    return strip_geometry_for(entry, strip_width, geo);
}

static X11Surface *x11_surface_create(StripX11 *sx, const LayoutEntry *entry,
                                      gint strip_width)
{
    X11Surface *xs = g_new0(X11Surface, 1);

    if (!x11_geometry(sx, entry, strip_width, &xs->geo)) {
        g_free(xs);
        return NULL;
    }
    x11_surface_place(xs, entry);

    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.override_redirect = True;
    attrs.background_pixel = 0;
    attrs.border_pixel = 0;
    attrs.event_mask = ExposureMask | EnterWindowMask | LeaveWindowMask
        | ButtonPressMask;
    xs->window = XCreateWindow(
        sx->display, sx->root, xs->x, xs->y, (guint)xs->geo.width,
        (guint)xs->geo.height, 0, CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
        &attrs);
    if (xs->window == None) {
        g_free(xs);
        return NULL;
    }
    /* Panelin üstünde kalması için dock tipi + opaklık. */
    Atom type = sx->dock_atom;

    XChangeProperty(sx->display, xs->window, sx->wm_type_atom, XA_ATOM, 32,
                    PropModeReplace, (const unsigned char *)&type, 1);
    gulong opacity = 0xffffffffu;

    XChangeProperty(sx->display, xs->window, sx->opacity_atom, XA_CARDINAL,
                    32, PropModeReplace, (const unsigned char *)&opacity, 1);
    XMapRaised(sx->display, xs->window);
    x11_surface_draw(sx, xs);
    return xs;
}

static void x11_update(Strip *strip)
{
    StripX11 *sx = strip->backend;
    App *app = strip->app;

    if (sx == NULL || sx->display == NULL) {
        return;
    }
    gint strip_width = app->settings.strip_width > 0
        ? app->settings.strip_width : 6;

    x11_surfaces_clear(sx);
    for (gsize i = 0; i < app->layout_len; i++) {
        if (app->layout[i].fullscreen) {
            continue;
        }
        X11Surface *xs = x11_surface_create(sx, &app->layout[i], strip_width);

        if (xs != NULL) {
            g_ptr_array_add(sx->surfaces, xs);
        }
    }
}

/* --- Olaylar ------------------------------------------------------ */

static gboolean x11_on_event(gint fd, GIOCondition condition, gpointer data)
{
    StripX11 *sx = data;

    (void)fd;
    if ((condition & G_IO_IN) == 0) {
        return G_SOURCE_CONTINUE;
    }
    while (XPending(sx->display) > 0) {
        XEvent event;

        XNextEvent(sx->display, &event);
        X11Surface *owner = NULL;

        for (guint i = 0; i < sx->surfaces->len; i++) {
            X11Surface *xs = g_ptr_array_index(sx->surfaces, i);

            if (xs->window == event.xany.window) {
                owner = xs;
                break;
            }
        }
        if (owner == NULL) {
            continue;
        }
        if (event.type == Expose) {
            x11_surface_draw(sx, owner);
        } else if (event.type == EnterNotify) {
            if (!owner->hover) {
                owner->hover = TRUE;
                strip_notify_hover(sx->strip, TRUE);
                x11_surface_draw(sx, owner);
            }
        } else if (event.type == LeaveNotify) {
            if (owner->hover) {
                owner->hover = FALSE;
                strip_notify_hover(sx->strip, FALSE);
                x11_surface_draw(sx, owner);
            }
        } else if (event.type == ButtonPress
                   && event.xbutton.button == Button1) {
            strip_notify_click(sx->strip);
        }
    }
    return G_SOURCE_CONTINUE;
}

/* --- Genel arayüz ------------------------------------------------ */

Strip *strip_x11_create(App *app, GError **error)
{
    StripX11 *sx = g_new0(StripX11, 1);
    Strip *strip = g_new0(Strip, 1);

    sx->display = XOpenDisplay(NULL);
    if (sx->display == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "X11 görüntü sunucusuna bağlanılamadı");
        g_free(sx);
        g_free(strip);
        return NULL;
    }
    sx->screen = DefaultScreen(sx->display);
    sx->root = RootWindow(sx->display, sx->screen);
    sx->opacity_atom = intern_atom(sx->display, "_NET_WM_WINDOW_OPACITY");
    sx->wm_type_atom = intern_atom(sx->display, "_NET_WM_WINDOW_TYPE");
    sx->dock_atom = intern_atom(sx->display, "_NET_WM_WINDOW_TYPE_DOCK");
    sx->surfaces = g_ptr_array_new();
    sx->strip = strip;
    strip->app = app;
    strip->wayland = FALSE;
    strip->backend = sx;
    strip->refs = 1;
    sx->fd_source = g_unix_fd_add(ConnectionNumber(sx->display), G_IO_IN,
                                  x11_on_event, sx);
    x11_update(strip);
    return strip;
}

void strip_x11_update(Strip *strip)
{
    x11_update(strip);
}

void strip_x11_destroy(Strip *strip)
{
    StripX11 *sx = strip->backend;

    if (sx == NULL) {
        return;
    }
    if (sx->fd_source != 0) {
        g_source_remove(sx->fd_source);
        sx->fd_source = 0;
    }
    x11_surfaces_clear(sx);
    g_ptr_array_free(sx->surfaces, TRUE);
    if (sx->display != NULL) {
        XCloseDisplay(sx->display);
        sx->display = NULL;
    }
    g_free(sx);
    strip->backend = NULL;
}
