/*
 * Masaüstünü Göster şeridi — Wayland (wlr-layer-shell) arka ucu
 *
 * Şerit, OVERLAY katmanında küçük bir yüzey olarak panelin masaüstüne bakan
 * ucuna yerleştirilir. Wayland nesneleri iş parçacığına bağlı olduğu için
 * tüm protokol trafiği kendi iş parçacığında yürür; ayar/düzen değişiklikleri
 * ana iş parçacığından kilitli bir anlık görüntü ile iletilir ve bir boru
 * üzerinden uyandırılır. İşaretçi olayları ana döngüye `g_main_context_invoke`
 * ile aktarılır.
 */
#include "strip_internal.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <cairo.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#define STRIP_BUFFERS 2

typedef struct {
    struct wl_output *output;
    gchar *name;
} StripOutput;

typedef struct {
    Strip *strip;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer;
    struct wl_output *output;
    struct wl_buffer *buffers[STRIP_BUFFERS];
    gpointer pixels[STRIP_BUFFERS];
    gsize sizes[STRIP_BUFFERS];
    guint busy;
    StripGeometry geo;
    gboolean configured;
    gboolean hover;
    gboolean redraw;
} StripSurface;

typedef struct {
    gchar *output;
    StripEdge edge;
    gint thickness;
    gdouble scale;
    gboolean fullscreen;
} StripSpec;

typedef struct {
    Strip *strip;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    GPtrArray *outputs;       /* StripOutput* */
    GPtrArray *surfaces;      /* StripSurface* */
    GArray *specs;            /* StripSpec */
    GThread *thread;
    GMutex lock;
    GCond cond;
    gint wake[2];
    gboolean running;
    gboolean dirty;
    gboolean init_done;
    gboolean init_ok;
    gchar *init_error;
    struct wl_surface *pointer_surface;
} StripWayland;

/* --- Olayları ana döngüye aktarma --------------------------------- */

static gboolean hover_idle(gpointer data)
{
    Strip *strip = data;

    if (!strip->destroyed) {
        strip_notify_hover(strip, strip->pending_hover);
    }
    return G_SOURCE_REMOVE;
}

static gboolean click_idle(gpointer data)
{
    Strip *strip = data;

    if (!strip->destroyed) {
        strip_notify_click(strip);
    }
    return G_SOURCE_REMOVE;
}

static void strip_destroy_notify(gpointer data)
{
    strip_unref(data);
}

static void post_hover(Strip *strip, gboolean inside)
{
    strip->pending_hover = inside;
    g_main_context_invoke_full(NULL, G_PRIORITY_DEFAULT, hover_idle,
                               strip_ref(strip), strip_destroy_notify);
}

static void post_click(Strip *strip)
{
    g_main_context_invoke_full(NULL, G_PRIORITY_DEFAULT, click_idle,
                               strip_ref(strip), strip_destroy_notify);
}

/* --- Küçük yardımcılar -------------------------------------------- */

static void wake_thread(StripWayland *bw)
{
    const guint8 byte = 1;

    if (write(bw->wake[1], &byte, 1) < 0 && errno != EAGAIN) {
        /* boru dolu ya da kapalı: yapılacak bir şey yok */
    }
}

static StripSurface *find_by_wl_surface(StripWayland *bw,
                                        struct wl_surface *surface)
{
    for (guint i = 0; i < bw->surfaces->len; i++) {
        StripSurface *ts = g_ptr_array_index(bw->surfaces, i);

        if (ts->surface == surface) {
            return ts;
        }
    }
    return NULL;
}

static struct wl_output *find_output(StripWayland *bw, const gchar *name)
{
    if (name == NULL || *name == '\0') {
        return NULL;
    }
    for (guint i = 0; i < bw->outputs->len; i++) {
        StripOutput *out = g_ptr_array_index(bw->outputs, i);

        if (g_strcmp0(out->name, name) == 0) {
            return out->output;
        }
    }
    return NULL;
}

/* --- wl_output ---------------------------------------------------- */

static void output_geometry(void *data, struct wl_output *output, int32_t x,
                            int32_t y, int32_t pw, int32_t ph,
                            int32_t subpixel, const char *make,
                            const char *model, int32_t transform)
{
    (void)data;
    (void)output;
    (void)x;
    (void)y;
    (void)pw;
    (void)ph;
    (void)subpixel;
    (void)make;
    (void)model;
    (void)transform;
}

static void output_mode(void *data, struct wl_output *output, uint32_t flags,
                        int32_t width, int32_t height, int32_t refresh)
{
    (void)data;
    (void)output;
    (void)flags;
    (void)width;
    (void)height;
    (void)refresh;
}

static void output_done(void *data, struct wl_output *output)
{
    (void)data;
    (void)output;
}

static void output_scale(void *data, struct wl_output *output, int32_t factor)
{
    (void)data;
    (void)output;
    (void)factor;
}

static void output_name(void *data, struct wl_output *output, const char *name)
{
    StripOutput *out = data;

    (void)output;
    g_free(out->name);
    out->name = g_strdup(name);
}

static void output_description(void *data, struct wl_output *output,
                               const char *description)
{
    (void)data;
    (void)output;
    (void)description;
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
    .name = output_name,
    .description = output_description
};

/* --- wl_registry -------------------------------------------------- */

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version)
{
    StripWayland *bw = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        bw->compositor = wl_registry_bind(registry, name,
                                          &wl_compositor_interface,
                                          MIN(version, 4u));
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        bw->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        bw->layer_shell = wl_registry_bind(registry, name,
                                           &zwlr_layer_shell_v1_interface,
                                           MIN(version, 5u));
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        bw->seat = wl_registry_bind(registry, name, &wl_seat_interface,
                                    MIN(version, 7u));
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        StripOutput *out = g_new0(StripOutput, 1);

        out->output = wl_registry_bind(registry, name, &wl_output_interface,
                                       MIN(version, 4u));
        wl_output_add_listener(out->output, &output_listener, out);
        g_ptr_array_add(bw->outputs, out);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name)
{
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove
};

/* --- wl_pointer --------------------------------------------------- */

static void pointer_enter(void *data, struct wl_pointer *pointer,
                          uint32_t serial, struct wl_surface *surface,
                          wl_fixed_t sx, wl_fixed_t sy)
{
    StripWayland *bw = data;
    StripSurface *ts = find_by_wl_surface(bw, surface);

    (void)pointer;
    (void)serial;
    (void)sx;
    (void)sy;
    bw->pointer_surface = surface;
    if (ts != NULL && !ts->hover) {
        ts->hover = TRUE;
        ts->redraw = TRUE;
        post_hover(bw->strip, TRUE);
    }
}

static void pointer_leave(void *data, struct wl_pointer *pointer,
                          uint32_t serial, struct wl_surface *surface)
{
    StripWayland *bw = data;
    StripSurface *ts = find_by_wl_surface(bw, surface);

    (void)pointer;
    (void)serial;
    bw->pointer_surface = NULL;
    if (ts != NULL && ts->hover) {
        ts->hover = FALSE;
        ts->redraw = TRUE;
        post_hover(bw->strip, FALSE);
    }
}

static void pointer_motion(void *data, struct wl_pointer *pointer,
                           uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    (void)data;
    (void)pointer;
    (void)time;
    (void)sx;
    (void)sy;
}

static void pointer_button(void *data, struct wl_pointer *pointer,
                           uint32_t serial, uint32_t time, uint32_t button,
                           uint32_t state)
{
    StripWayland *bw = data;

    (void)pointer;
    (void)serial;
    (void)time;
    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED
        && bw->pointer_surface != NULL) {
        post_click(bw->strip);
    }
}

static void pointer_axis(void *data, struct wl_pointer *pointer,
                         uint32_t time, uint32_t axis, wl_fixed_t value)
{
    (void)data;
    (void)pointer;
    (void)time;
    (void)axis;
    (void)value;
}

static void pointer_frame(void *data, struct wl_pointer *pointer)
{
    (void)data;
    (void)pointer;
}

static void pointer_axis_source(void *data, struct wl_pointer *pointer,
                                uint32_t source)
{
    (void)data;
    (void)pointer;
    (void)source;
}

static void pointer_axis_stop(void *data, struct wl_pointer *pointer,
                              uint32_t time, uint32_t axis)
{
    (void)data;
    (void)pointer;
    (void)time;
    (void)axis;
}

static void pointer_axis_discrete(void *data, struct wl_pointer *pointer,
                                  uint32_t axis, int32_t discrete)
{
    (void)data;
    (void)pointer;
    (void)axis;
    (void)discrete;
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete
};

static void seat_capabilities(void *data, struct wl_seat *seat,
                              uint32_t capabilities)
{
    StripWayland *bw = data;

    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0
        && bw->pointer == NULL) {
        bw->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(bw->pointer, &pointer_listener, bw);
    }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
    (void)data;
    (void)seat;
    (void)name;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name
};

/* --- Arabellekler ------------------------------------------------- */

static void buffer_release(void *data, struct wl_buffer *buffer)
{
    StripSurface *ts = data;

    for (gint i = 0; i < STRIP_BUFFERS; i++) {
        if (ts->buffers[i] == buffer) {
            ts->busy &= ~(1u << (guint)i);
            ts->redraw = TRUE;
            break;
        }
    }
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release
};

static struct wl_buffer *buffer_create(StripWayland *bw, StripSurface *ts,
                                       gint index)
{
    gint width = ts->geo.buffer_width;
    gint height = ts->geo.buffer_height;
    gint stride = width * 4;
    gsize size = (gsize)stride * (gsize)height;
    gint fd = memfd_create("cbw-strip", MFD_CLOEXEC);

    if (fd < 0) {
        return NULL;
    }
    if (ftruncate(fd, (off_t)size) < 0) {
        close(fd);
        return NULL;
    }
    gpointer data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (data == MAP_FAILED) {
        close(fd);
        return NULL;
    }
    struct wl_shm_pool *pool = wl_shm_create_pool(bw->shm, fd, (int32_t)size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(
        pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);

    wl_shm_pool_destroy(pool);
    close(fd);
    if (buffer == NULL) {
        munmap(data, size);
        return NULL;
    }
    ts->pixels[index] = data;
    ts->sizes[index] = size;
    ts->buffers[index] = buffer;
    wl_buffer_add_listener(buffer, &buffer_listener, ts);
    return buffer;
}

static void buffers_free(StripSurface *ts)
{
    for (gint i = 0; i < STRIP_BUFFERS; i++) {
        if (ts->buffers[i] != NULL) {
            wl_buffer_destroy(ts->buffers[i]);
            ts->buffers[i] = NULL;
        }
        if (ts->pixels[i] != NULL) {
            munmap(ts->pixels[i], ts->sizes[i]);
            ts->pixels[i] = NULL;
            ts->sizes[i] = 0;
        }
    }
    ts->busy = 0;
}

/* --- Çizim -------------------------------------------------------- */

static gboolean surface_draw(StripWayland *bw, StripSurface *ts)
{
    if (!ts->configured || ts->surface == NULL || ts->geo.width <= 0
        || ts->geo.height <= 0) {
        return FALSE;
    }
    gint index = -1;

    for (gint i = 0; i < STRIP_BUFFERS; i++) {
        if ((ts->busy & (1u << (guint)i)) == 0) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        return FALSE;           /* tümü meşgul: release sonrası yeniden */
    }
    if (ts->buffers[index] == NULL && buffer_create(bw, ts, index) == NULL) {
        return FALSE;
    }
    gint width = ts->geo.buffer_width;
    gint height = ts->geo.buffer_height;
    cairo_surface_t *cs = cairo_image_surface_create_for_data(
        ts->pixels[index], CAIRO_FORMAT_ARGB32, width, height, width * 4);
    cairo_t *cr = cairo_create(cs);

    cairo_scale(cr, (gdouble)width / ts->geo.width,
                (gdouble)height / ts->geo.height);
    strip_draw(cr, ts->geo.width, ts->geo.height, ts->geo.edge, ts->hover,
               bw->strip->peek_active);
    cairo_destroy(cr);
    cairo_surface_flush(cs);
    cairo_surface_destroy(cs);

    ts->busy |= 1u << (guint)index;
    ts->redraw = FALSE;
    wl_surface_attach(ts->surface, ts->buffers[index], 0, 0);
    wl_surface_damage_buffer(ts->surface, 0, 0, width, height);
    wl_surface_commit(ts->surface);
    return TRUE;
}

/* --- layer_surface ------------------------------------------------- */

static void layer_configure(void *data, struct zwlr_layer_surface_v1 *surface,
                            uint32_t serial, uint32_t width, uint32_t height)
{
    StripSurface *ts = data;

    zwlr_layer_surface_v1_ack_configure(surface, serial);
    if (width > 0 && height > 0) {
        gdouble sx = (gdouble)ts->geo.buffer_width / ts->geo.width;
        gdouble sy = (gdouble)ts->geo.buffer_height / ts->geo.height;

        ts->geo.width = (gint)width;
        ts->geo.height = (gint)height;
        ts->geo.buffer_width = (gint)ceil(width * sx);
        ts->geo.buffer_height = (gint)ceil(height * sy);
        buffers_free(ts);
    }
    ts->configured = TRUE;
    ts->redraw = TRUE;
}

static void layer_closed(void *data, struct zwlr_layer_surface_v1 *surface)
{
    StripSurface *ts = data;

    (void)surface;
    ts->configured = FALSE;
}

static const struct zwlr_layer_surface_v1_listener layer_listener = {
    .configure = layer_configure,
    .closed = layer_closed
};

/* --- Yüzey yaşam döngüsü ------------------------------------------ */

static void surfaces_clear(StripWayland *bw)
{
    for (guint i = 0; i < bw->surfaces->len; i++) {
        StripSurface *ts = g_ptr_array_index(bw->surfaces, i);

        buffers_free(ts);
        if (ts->layer != NULL) {
            zwlr_layer_surface_v1_destroy(ts->layer);
        }
        if (ts->surface != NULL) {
            wl_surface_destroy(ts->surface);
        }
        g_free(ts);
    }
    g_ptr_array_set_size(bw->surfaces, 0);
}

static StripSurface *surface_create(StripWayland *bw, const StripSpec *spec,
                                    gint strip_width)
{
    StripSurface *ts = g_new0(StripSurface, 1);

    ts->strip = bw->strip;
    if (!strip_geometry_calc(spec->edge, spec->thickness, strip_width,
                             spec->scale, &ts->geo)) {
        g_free(ts);
        return NULL;
    }
    ts->output = find_output(bw, spec->output);
    ts->surface = wl_compositor_create_surface(bw->compositor);
    if (ts->surface == NULL) {
        g_free(ts);
        return NULL;
    }
    ts->layer = zwlr_layer_shell_v1_get_layer_surface(
        bw->layer_shell, ts->surface, ts->output,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "comebackwindow-strip");
    if (ts->layer == NULL) {
        wl_surface_destroy(ts->surface);
        g_free(ts);
        return NULL;
    }
    zwlr_layer_surface_v1_add_listener(ts->layer, &layer_listener, ts);
    zwlr_layer_surface_v1_set_size(ts->layer, (uint32_t)ts->geo.width,
                                   (uint32_t)ts->geo.height);
    zwlr_layer_surface_v1_set_anchor(ts->layer, (uint32_t)ts->geo.anchor);
    zwlr_layer_surface_v1_set_exclusive_zone(ts->layer, -1);
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        ts->layer, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    zwlr_layer_surface_v1_set_margin(ts->layer, ts->geo.margin_top,
                                     ts->geo.margin_right,
                                     ts->geo.margin_bottom,
                                     ts->geo.margin_left);
    wl_surface_commit(ts->surface);
    return ts;
}

/* --- Anlık görüntü ------------------------------------------------ */

static void specs_clear(StripWayland *bw)
{
    for (guint i = 0; i < bw->specs->len; i++) {
        g_free(g_array_index(bw->specs, StripSpec, i).output);
    }
    g_array_set_size(bw->specs, 0);
}

void strip_wayland_update(Strip *strip)
{
    StripWayland *bw = strip->backend;
    App *app = strip->app;

    if (bw == NULL) {
        return;
    }
    g_mutex_lock(&bw->lock);
    specs_clear(bw);
    for (gsize i = 0; i < app->layout_len; i++) {
        const LayoutEntry *entry = &app->layout[i];
        StripSpec spec = {
            .output = g_strdup(entry->output),
            .edge = strip_edge_from_string(entry->edge),
            .thickness = entry->thickness,
            .scale = entry->scale > 0 ? entry->scale : 1.0,
            .fullscreen = entry->fullscreen
        };

        g_array_append_val(bw->specs, spec);
    }
    bw->dirty = TRUE;
    g_mutex_unlock(&bw->lock);
    wake_thread(bw);
}

/* --- İş parçacığı -------------------------------------------------- */

static gboolean spec_matches(StripSurface *ts, const StripSpec *spec,
                             gint strip_width)
{
    StripGeometry geo;

    if (!strip_geometry_calc(spec->edge, spec->thickness, strip_width,
                             spec->scale, &geo)) {
        return FALSE;
    }
    return ts->geo.width == geo.width && ts->geo.height == geo.height;
}

static void dispatch_redraws(StripWayland *bw)
{
    for (guint i = 0; i < bw->surfaces->len; i++) {
        StripSurface *ts = g_ptr_array_index(bw->surfaces, i);

        if (ts->redraw) {
            surface_draw(bw, ts);
        }
    }
}

static void apply_specs(StripWayland *bw)
{
    GPtrArray *specs = g_ptr_array_new();
    GPtrArray *keep = g_ptr_array_new();

    g_mutex_lock(&bw->lock);
    for (guint i = 0; i < bw->specs->len; i++) {
        StripSpec copy = g_array_index(bw->specs, StripSpec, i);

        copy.output = g_strdup(copy.output);
        g_ptr_array_add(specs, g_memdup2(&copy, sizeof(copy)));
    }
    bw->dirty = FALSE;
    g_mutex_unlock(&bw->lock);

    gint strip_width = bw->strip->app->settings.strip_width > 0
        ? bw->strip->app->settings.strip_width : 6;

    for (guint i = 0; i < specs->len; i++) {
        StripSpec *spec = g_ptr_array_index(specs, i);
        StripSurface *reuse = NULL;

        if (spec->fullscreen) {
            continue;           /* tam ekranda şerit gizlenir */
        }
        for (guint j = 0; j < bw->surfaces->len; j++) {
            StripSurface *ts = g_ptr_array_index(bw->surfaces, j);

            if (ts->output == find_output(bw, spec->output)
                && spec_matches(ts, spec, strip_width)) {
                reuse = ts;
                g_ptr_array_remove_index(bw->surfaces, j);
                break;
            }
        }
        if (reuse == NULL) {
            reuse = surface_create(bw, spec, strip_width);
        }
        if (reuse != NULL) {
            reuse->redraw = TRUE;
            g_ptr_array_add(keep, reuse);
        }
    }
    surfaces_clear(bw);
    for (guint i = 0; i < keep->len; i++) {
        g_ptr_array_add(bw->surfaces, g_ptr_array_index(keep, i));
    }
    g_ptr_array_free(keep, TRUE);
    for (guint i = 0; i < specs->len; i++) {
        StripSpec *spec = g_ptr_array_index(specs, i);

        g_free(spec->output);
        g_free(spec);
    }
    g_ptr_array_free(specs, TRUE);
}

static gboolean connect_backend(StripWayland *bw)
{
    bw->display = wl_display_connect(NULL);
    if (bw->display == NULL) {
        bw->init_error = g_strdup("Wayland görüntü sunucusuna bağlanılamadı");
        return FALSE;
    }
    bw->registry = wl_display_get_registry(bw->display);
    wl_registry_add_listener(bw->registry, &registry_listener, bw);
    if (wl_display_roundtrip(bw->display) < 0
        || wl_display_roundtrip(bw->display) < 0) {
        bw->init_error = g_strdup("Wayland kayıt defteri okunamadı");
        return FALSE;
    }
    if (bw->compositor == NULL || bw->shm == NULL) {
        bw->init_error = g_strdup("wl_compositor/wl_shm bulunamadı");
        return FALSE;
    }
    if (bw->layer_shell == NULL) {
        bw->init_error =
            g_strdup("zwlr_layer_shell_v1 bulunamadı (KWin sürümü?)");
        return FALSE;
    }
    if (bw->seat != NULL) {
        wl_seat_add_listener(bw->seat, &seat_listener, bw);
    }
    return TRUE;
}

static void teardown_backend(StripWayland *bw)
{
    surfaces_clear(bw);
    if (bw->pointer != NULL) {
        wl_pointer_destroy(bw->pointer);
        bw->pointer = NULL;
    }
    if (bw->seat != NULL) {
        wl_seat_destroy(bw->seat);
        bw->seat = NULL;
    }
    if (bw->layer_shell != NULL) {
        zwlr_layer_shell_v1_destroy(bw->layer_shell);
        bw->layer_shell = NULL;
    }
    if (bw->shm != NULL) {
        wl_shm_destroy(bw->shm);
        bw->shm = NULL;
    }
    if (bw->compositor != NULL) {
        wl_compositor_destroy(bw->compositor);
        bw->compositor = NULL;
    }
    for (guint i = 0; i < bw->outputs->len; i++) {
        StripOutput *out = g_ptr_array_index(bw->outputs, i);

        wl_output_destroy(out->output);
        g_free(out->name);
        g_free(out);
    }
    g_ptr_array_set_size(bw->outputs, 0);
    if (bw->registry != NULL) {
        wl_registry_destroy(bw->registry);
        bw->registry = NULL;
    }
    if (bw->display != NULL) {
        wl_display_flush(bw->display);
        wl_display_disconnect(bw->display);
        bw->display = NULL;
    }
}

static gboolean thread_should_run(StripWayland *bw)
{
    gboolean running;

    g_mutex_lock(&bw->lock);
    running = bw->running;
    g_mutex_unlock(&bw->lock);
    return running;
}

static gboolean thread_take_dirty(StripWayland *bw)
{
    gboolean dirty;

    g_mutex_lock(&bw->lock);
    dirty = bw->dirty;
    g_mutex_unlock(&bw->lock);
    return dirty;
}

static void thread_loop(StripWayland *bw)
{
    while (thread_should_run(bw)) {
        struct pollfd fds[2] = {
            { wl_display_get_fd(bw->display), POLLIN, 0 },
            { bw->wake[0], POLLIN, 0 }
        };

        wl_display_flush(bw->display);
        gint rc = poll(fds, 2, 1000);

        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (rc == 0) {
            wl_display_dispatch_pending(bw->display);
            dispatch_redraws(bw);
            continue;
        }
        if ((fds[1].revents & POLLIN) != 0) {
            guint8 buf[64];

            while (read(bw->wake[0], buf, sizeof(buf)) > 0) {
            }
            if (thread_take_dirty(bw)) {
                apply_specs(bw);
            }
        }
        if ((fds[0].revents & POLLIN) != 0) {
            if (wl_display_dispatch(bw->display) < 0) {
                break;
            }
        } else if ((fds[0].revents & (POLLERR | POLLHUP)) != 0) {
            break;
        }
        dispatch_redraws(bw);
        wl_display_flush(bw->display);
    }
}

static gpointer strip_thread(gpointer user_data)
{
    StripWayland *bw = user_data;
    gboolean ok = connect_backend(bw);

    if (ok) {
        apply_specs(bw);
    }
    g_mutex_lock(&bw->lock);
    bw->init_ok = ok;
    bw->init_done = TRUE;
    g_cond_broadcast(&bw->cond);
    g_mutex_unlock(&bw->lock);
    if (ok) {
        thread_loop(bw);
        teardown_backend(bw);
    }
    return NULL;
}

/* --- Genel arayüz ------------------------------------------------ */

static void backend_free(StripWayland *bw)
{
    specs_clear(bw);
    g_array_free(bw->specs, TRUE);
    g_ptr_array_free(bw->surfaces, TRUE);
    g_ptr_array_free(bw->outputs, TRUE);
    if (bw->wake[0] >= 0) {
        close(bw->wake[0]);
    }
    if (bw->wake[1] >= 0) {
        close(bw->wake[1]);
    }
    g_mutex_clear(&bw->lock);
    g_cond_clear(&bw->cond);
    g_free(bw->init_error);
    g_free(bw);
}

Strip *strip_wayland_create(App *app, GError **error)
{
    StripWayland *bw = g_new0(StripWayland, 1);
    Strip *strip = g_new0(Strip, 1);

    strip->app = app;
    strip->wayland = TRUE;
    strip->backend = bw;
    strip->refs = 1;
    bw->strip = strip;
    bw->outputs = g_ptr_array_new();
    bw->surfaces = g_ptr_array_new();
    bw->specs = g_array_new(FALSE, FALSE, sizeof(StripSpec));
    bw->running = TRUE;
    bw->wake[0] = -1;
    bw->wake[1] = -1;
    g_mutex_init(&bw->lock);
    g_cond_init(&bw->cond);
    if (pipe2(bw->wake, O_CLOEXEC | O_NONBLOCK) < 0) {
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
                    "uyandırma borusu açılamadı: %s", g_strerror(errno));
        bw->wake[0] = -1;
        bw->wake[1] = -1;
        backend_free(bw);
        g_free(strip);
        return NULL;
    }
    bw->thread = g_thread_new("cbw-strip", strip_thread, bw);
    g_mutex_lock(&bw->lock);
    while (!bw->init_done) {
        g_cond_wait(&bw->cond, &bw->lock);
    }
    gboolean ok = bw->init_ok;
    gchar *msg = g_strdup(bw->init_error);

    g_mutex_unlock(&bw->lock);

    if (!ok) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s",
                    msg != NULL ? msg : "Wayland şeridi açılamadı");
        g_free(msg);
        g_thread_join(bw->thread);
        backend_free(bw);
        g_free(strip);
        return NULL;
    }
    g_free(msg);
    return strip;
}

void strip_wayland_destroy(Strip *strip)
{
    StripWayland *bw = strip->backend;

    if (bw == NULL) {
        return;
    }
    g_mutex_lock(&bw->lock);
    bw->running = FALSE;
    g_mutex_unlock(&bw->lock);
    wake_thread(bw);
    g_thread_join(bw->thread);
    bw->thread = NULL;
    strip->backend = NULL;
    backend_free(bw);
}
