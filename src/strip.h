/* Masaüstünü Göster şeridi — ortak arayüz ve geometri */
#pragma once

#include "app.h"

#include <cairo.h>

G_BEGIN_DECLS

typedef enum {
    STRIP_EDGE_NONE = 0,
    STRIP_EDGE_BOTTOM,
    STRIP_EDGE_TOP,
    STRIP_EDGE_LEFT,
    STRIP_EDGE_RIGHT
} StripEdge;

/* wlr-layer-shell bağlantı noktaları ile aynı bit değerleri. */
enum {
    STRIP_ANCHOR_TOP = 1,
    STRIP_ANCHOR_BOTTOM = 2,
    STRIP_ANCHOR_LEFT = 4,
    STRIP_ANCHOR_RIGHT = 8
};

typedef struct {
    StripEdge edge;
    gint anchor;             /* STRIP_ANCHOR_* bit maskesi */
    gint width;              /* mantıksal px */
    gint height;             /* mantıksal px */
    gint margin_top;
    gint margin_right;
    gint margin_bottom;
    gint margin_left;
    gint buffer_width;       /* fiziksel px */
    gint buffer_height;
} StripGeometry;

typedef struct _Strip Strip;

/* Şerit olayları; hepsi ana döngü iş parçacığında çağrılır. */
typedef struct {
    void (*hover)(Strip *strip, gboolean inside, gpointer user_data);
    void (*click)(Strip *strip, gpointer user_data);
    /* Düğmeye basıldı (bırakma olayı henüz gelmedi). */
    void (*press)(Strip *strip, gpointer user_data);
} StripCallbacks;

/* "bottom", "top", "left", "right", "none" → StripEdge. */
StripEdge strip_edge_from_string(const gchar *edge);

/* Düzen girdisi ve ayarlardan şerit geometrisini hesaplar.
 * Geçersiz girdide FALSE döner. */
gboolean strip_geometry_for(const LayoutEntry *entry, gint strip_width,
                            StripGeometry *out);

/* Panel varsa şeridin kenar boşluğunu panel kalınlığı kadar ayarlar. */
void strip_set_panel_margin(StripEdge edge, gint thickness, StripGeometry *geo);

/* Aynı hesabın açık parametreli biçimi. */
gboolean strip_geometry_calc(StripEdge edge, gint thickness, gint strip_width,
                             gdouble scale, StripGeometry *out);

/* Şeridi oluşturur (Wayland layer-shell; yoksa X11). Başarısızsa NULL ve
 * hata döner. */
Strip *strip_create(App *app, GError **error);

/* Ayarları/düzeni yeniden uygular. */
void strip_update(Strip *strip);

/* Görünümü tazeler (tema/renk değişimi). */
void strip_refresh(Strip *strip);

/* Aero Peek durumunu görsele yansıtır. */
void strip_set_peek_active(Strip *strip, gboolean active);

void strip_destroy(Strip *strip);

/* Yaşam süresi (iç kullanım; arka uçlar olay aktarırken tutar). */
Strip *strip_ref(Strip *strip);
void strip_unref(Strip *strip);

/* Geri çağrıları bağlar (strip_create öncesi). */
void strip_set_callbacks(Strip *strip, const StripCallbacks *callbacks,
                         gpointer user_data);

/* Şerit çizimi: arka plan + ok. Hem Wayland hem X11 kullanır. */
void strip_draw(cairo_t *cr, gint width, gint height, StripEdge edge,
                gboolean hover, gboolean peek_active);

G_END_DECLS