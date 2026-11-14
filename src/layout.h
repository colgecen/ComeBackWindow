/* ReportLayout JSON çözümleyici ve düzen deposu */
#pragma once

#include "app.h"

G_BEGIN_DECLS

/* KWin betiğinin gönderdiği JSON dizisini çözümler.
 * Başarıda entries/len doldurulur (boş dizi olabilir); hata durumunda
 * FALSE döner, çıktılar boş kalır. */
gboolean layout_parse(const gchar *json, LayoutEntry **entries, gsize *len,
                      GError **error);

/* layout_parse sonucunu serbest bırakır. */
void layout_entries_free(LayoutEntry *entries, gsize len);

/* Tam çıktı adına göre girdi arar; bulunamazsa NULL. */
const LayoutEntry *layout_find(const LayoutEntry *entries, gsize len,
                               const gchar *output);

/* JSON'u çözüp app->layout'u değiştirir. Bozuk JSON'da düzeni boşaltır. */
gboolean layout_apply(App *app, const gchar *json);

G_END_DECLS
