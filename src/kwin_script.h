/* KWin betiğini yükleme, kaldırma ve komut kanalı */
#pragma once

#include "app.h"

G_BEGIN_DECLS

/* Betiği yükler ve KWin yeniden başlama/u yku izleyicilerini kurar. */
gboolean kwin_script_init(App *app);

/* Betiği boşaltır ve izleyicileri kaldırır. */
void kwin_script_shutdown(App *app);

/* Betiği yeniden yükler (yüklüyse önce kaldırır). */
gboolean kwin_script_reload(App *app);

/* Ayarların değiştiğini betiğe bildirir. */
gboolean kwin_script_notify_settings(App *app);

/* Komutu kanal A ile gönderir. Başarısızsa FALSE döner (çağıran kuyruğa
 * eklemelidir). */
gboolean kwin_script_send_command(App *app, const gchar *command);

/* Betik yüklü mü? */
gboolean kwin_script_is_loaded(App *app);

/* Betik hazır olduğunda çağrılır (uygulama tarafından). */
void kwin_script_on_ready(App *app);

G_END_DECLS
