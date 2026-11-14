/*
 * Come Back Window — yapılandırma modülü
 *
 * Ayarlar `~/.config/comebackwindow/config.ini` dosyasında GKeyFile biçiminde
 * tutulur. Aralık dışı değerler okuma sırasında sıkıştırılır ve varsayılanlar
 * her zaman geçerli bir yapı üretir.
 */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
    gboolean autostart;
    gboolean strip_enabled;
    gint strip_width;
    gint fallback_height;
    gboolean peek_enabled;
    gint peek_delay_ms;
    gdouble peek_opacity;
    gboolean shake_enabled;
    gboolean tray_enabled;
    gchar *language;
    gboolean first_run_done;
} Settings;

/* Varsayılanları yapıya yazar (dil "auto"). */
void settings_init_defaults(Settings *s);

/* Yapıdaki dinamik alanları serbest bırakır. */
void settings_clear(Settings *s);

/* Yapılandırma dosyasının tam yolu. CBW_CONFIG_DIR ile değiştirilebilir. */
gchar *config_path(void);

/* Yapılandırma dizini (dosya değil). Çağıran g_free ile serbest bırakır. */
gchar *config_dir(void);

/* Dosya var mı? */
gboolean config_exists(void);

/* Dosyayı okur. Dosya yoksa varsayılanlar kalır ve TRUE döner. */
gboolean settings_load(Settings *s, GError **error);

/* Dosyayı yazar (eksik dizini oluşturur). */
gboolean settings_save(const Settings *s, GError **error);

/* Değerleri geçerli aralıklara sıkıştırır. */
void settings_clamp(Settings *s);

/* Ayarları betiğe/DBus'a vermek için JSON üretir. */
gchar *settings_to_json(const Settings *s);

G_END_DECLS
