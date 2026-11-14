/* Autostart (.desktop) yönetimi */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

/* Autostart dosyasının tam yolu (CBW_AUTOSTART_DIR ile değiştirilebilir). */
gchar *autostart_path(void);

/* Autostart dizini. */
gchar *autostart_dir(void);

/* Dosya oluşturur; dizin yoksa oluşturur. */
gboolean autostart_enable(GError **error);

/* Dosyayı siler. Dosya yoksa TRUE döner. */
gboolean autostart_disable(GError **error);

/* Dosya var mı? */
gboolean autostart_is_enabled(void);

G_END_DECLS
