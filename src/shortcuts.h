/*
 * Kısayol çakışma yönetimi
 *
 * KWin betiğimizin kısayolları ile KDE'nin yerel KWin kısayolları aynı tuş
 * bileşimini istediğinde hangi tarafın kullanılacağına karar verir. KDE
 * tarafında yazma işlemleri bazı sürümlerde sessizce yok sayıldığı için her
 * yazma denemesi okuma ile doğrulanır; doğrulanamazsa yerel kısayol korunur
 * ve kullanıcıya bildirim gösterilir.
 */
#pragma once

#include "app.h"

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct {
    const gchar *action;    /* KWin eylem kimliği (cbw-*) */
    const gchar *label;     /* kullanıcıya görünen ad */
    const gchar *sequence;  /* Qt gösterimi (Meta+D) */
    gint key;               /* Qt tuş kodu (değiştiriciler dahil) */
    const gchar *native;    /* çakışabilecek yerel KWin eylemi */
    gboolean core;          /* TRUE: yerel eşdeğeri aynı işi yapar */
} ShortcutBinding;

typedef struct {
    gchar *action_unique;
    gchar *action_friendly;
    gchar *component_unique;
    gchar *context_unique;
    GArray *default_keys;   /* gint */
    GArray *current_keys;   /* gint */
} ShortcutInfo;

/* Bilinen kısayollar ve beklenen Qt tuş kodları. */
const ShortcutBinding *shortcuts_bindings(gsize *count);

/* allShortcutInfos yanıtını ShortcutInfo dizisine çevirir. */
GPtrArray *shortcuts_parse_infos(GVariant *reply);

/* Dizide eylem kimliğine göre arama; bulunamazsa NULL. */
const ShortcutInfo *shortcuts_find(const GPtrArray *infos, const gchar *action);

/* Tuş kodu eylemin geçerli veya varsayılan tuşlarında mı? */
gboolean shortcut_info_has_key(const ShortcutInfo *info, gint key);

/* Yerel eylem aynı tuşu kullanıyor mu? */
gboolean shortcuts_binding_conflicts(const ShortcutBinding *binding,
                                    const ShortcutInfo *native);

/* Tuş listesi <-> "a;b;c" metni. Geçersiz metinde FALSE döner. */
gchar *shortcuts_keys_to_string(const GArray *keys);
gboolean shortcuts_string_to_keys(const gchar *text, GArray *keys);

/* kglobalaccel'den /component/kwin kısayol tablosunu okur. */
GPtrArray *shortcuts_read(GDBusConnection *bus, GError **error);

/* Çakışmaları denetler, bayrakları günceller ve gerekirse bildirir. */
void shortcuts_check(App *app);

/* Yedeklenen yerel kısayolları geri yükler. */
gboolean shortcuts_restore(GDBusConnection *bus, gboolean *had_backup,
                           GError **error);

/* KDE kısayol düzenleyicisini açar (kcmshell6 keys). */
gboolean shortcuts_open_editor(GError **error);

G_END_DECLS
