/* Aero Peek zamanlayıcısı */
#pragma once

#include "app.h"

G_BEGIN_DECLS

/* Peek durum makinesini kurar (şerit oluşturulmadan önce çağrılmalı). */
Peek *peek_create(App *app);

void peek_destroy(Peek *peek);

/* İşaretçi şeride girdi: peek_delay_ms sonra peek başlatır. */
void peek_pointer_enter(Peek *peek);

/* İşaretçi şeritten çıktı: bekleme iptal, peek açıksa kapatılır. */
void peek_pointer_leave(Peek *peek);

/* Anında aç/kapat (tepsi/CLI). */
void peek_toggle(Peek *peek);

/* Betikten gelen "peek-begin"/"peek-end"/"peek-fallback" olaylarını işler. */
void peek_on_event(Peek *peek, const gchar *name, const gchar *json);

/* Peek açık mı? */
gboolean peek_is_active(const Peek *peek);

/*
 * Şeride tıklandı. Yedek önizleme masaüstünü gösterdiyse tıklama onu
 * onaylar ve TRUE döner; aksi halde FALSE döner ve çağıran aç/kapat yapar.
 */
gboolean peek_click(Peek *peek);

/* Gecikme süresini ayarlardan yeniden okur. */
void peek_reload_settings(Peek *peek);

G_END_DECLS
