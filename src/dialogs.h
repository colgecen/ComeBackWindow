/*
 * GTK4 ayarlar ve ilk açılış sihirbazı
 *
 * Uygulama zaten bir GMainLoop çalıştırdığı için GTK pencereleri aynı ana
 * döngüye eklenir; ayrı bir gtk_main gerekmez. Görüntü sunucusu yoksa
 * (ör. SSH oturumu) pencereler açılmaz ve çağrı sessizce yedek davranışa
 * döner.
 */
#pragma once

#include "app.h"

G_BEGIN_DECLS

/* GTK'yı hazırlar; görüntü sunucusu yoksa FALSE. */
gboolean dialogs_init(void);

/* Ayarlar penceresini açar veya var olanı öne getirir. */
void dialogs_open_settings(App *app);

/* İlk açılış sihirbazını açar veya var olanı öne getirir. */
void dialogs_open_setup(App *app);

/* first_run_done değilse sihirbazı açar. */
void dialogs_open_setup_if_needed(App *app);

/* Açık pencereleri kapatır (çıkış sırasında). */
void dialogs_shutdown(App *app);

G_END_DECLS
