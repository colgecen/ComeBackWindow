/*
 * Kısayol çakışma yönetimi
 *
 * Yaklaşım:
 *   1. /component/kwin üzerindeki tüm kısayollar okunur.
 *   2. Bizim eylemimiz ile yerel KWin eylemi aynı tuşu istiyorsa çakışma
 *      vardır.
 *   3. Yalnızca bizim kısayolumuz gerçekten bağlıysa yerel eylem serbest
 *      bırakılmaya çalışılır; her yazma okuma ile doğrulanır.
 *   4. Doğrulanamazsa yerel kısayol korunur (masaüstü/kapat için) veya
 *      çakışmayan alternatif tuş kullanılır (küçült/büyüt için).
 */
#include "i18n.h"
#include "shortcuts.h"

#include "config.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>

#define KWIN_SERVICE "org.kde.kglobalaccel"
#define KWIN_COMPONENT_PATH "/component/kwin"
#define KWIN_COMPONENT_IFACE "org.kde.kglobalaccel.Component"
#define KWIN_GLOBAL_IFACE "org.kde.KGlobalAccel"
#define BACKUP_FILE "shortcuts-backup.ini"

/* Qt tuş kodları: değiştirici bayrakları + Qt::Key_* değerleri. */
#define KEY_META_D 0x10000044
#define KEY_META_M 0x1000004d
#define KEY_ALT_F4 0x09000033
#define KEY_META_DOWN 0x11000015
#define KEY_META_UP 0x11000013
#define KEY_META_HOME 0x11000010
#define KEY_META_SHIFT_M 0x1200004d

static const ShortcutBinding bindings[] = {
    { "cbw-show-desktop", "Masaüstünü Göster", "Meta+D", KEY_META_D,
      "Show Desktop", TRUE },
    { "cbw-show-desktop-alt", "Masaüstünü Göster (alternatif)", "Meta+M",
      KEY_META_M, NULL, FALSE },
    { "cbw-close", "Pencereyi Kapat", "Alt+F4", KEY_ALT_F4,
      "Window Close", TRUE },
    { "cbw-minimize", "Pencereyi Küçült", "Meta+Down", KEY_META_DOWN,
      "Window Quick Tile Bottom", FALSE },
    { "cbw-maximize", "Pencereyi Büyüt", "Meta+Up", KEY_META_UP,
      "Window Quick Tile Top", FALSE },
    { "cbw-minimize-others", "Diğerlerini Küçült", "Meta+Home",
      KEY_META_HOME, NULL, FALSE },
    { "cbw-restore-all", "Küçültülenleri Geri Getir", "Meta+Shift+M",
      KEY_META_SHIFT_M, NULL, FALSE }
};

const ShortcutBinding *shortcuts_bindings(gsize *count)
{
    if (count != NULL) {
        *count = G_N_ELEMENTS(bindings);
    }
    return bindings;
}

/* --- Ayrıştırma --------------------------------------------------- */

static GArray *keys_from_variant(GVariant *value)
{
    GArray *keys = g_array_new(FALSE, FALSE, sizeof(gint));
    gsize n = g_variant_n_children(value);

    for (gsize i = 0; i < n; i++) {
        GVariant *child = g_variant_get_child_value(value, i);
        gint key = g_variant_get_int32(child);

        g_variant_unref(child);
        g_array_append_val(keys, key);
    }
    return keys;
}

static ShortcutInfo *parse_info(GVariant *item)
{
    ShortcutInfo *info = g_new0(ShortcutInfo, 1);
    gchar *component_friendly = NULL;
    gchar *context_friendly = NULL;
    GVariant *defaults = NULL;
    GVariant *current = NULL;

    g_variant_get(item, "(ssssss@ai@ai)",
                  &info->action_unique, &info->action_friendly,
                  &info->component_unique, &component_friendly,
                  &info->context_unique, &context_friendly,
                  &defaults, &current);
    info->default_keys = keys_from_variant(defaults);
    info->current_keys = keys_from_variant(current);
    g_variant_unref(defaults);
    g_variant_unref(current);
    g_free(component_friendly);
    g_free(context_friendly);
    return info;
}

void shortcut_info_free(gpointer data)
{
    ShortcutInfo *info = data;

    if (info == NULL) {
        return;
    }
    g_free(info->action_unique);
    g_free(info->action_friendly);
    g_free(info->component_unique);
    g_free(info->context_unique);
    g_clear_pointer(&info->default_keys, g_array_unref);
    g_clear_pointer(&info->current_keys, g_array_unref);
    g_free(info);
}

GPtrArray *shortcuts_parse_infos(GVariant *reply)
{
    if (reply == NULL
        || !g_variant_is_of_type(reply, G_VARIANT_TYPE("(a(ssssssaiai))"))) {
        return NULL;
    }
    GVariant *array = g_variant_get_child_value(reply, 0);
    gsize n = g_variant_n_children(array);
    GPtrArray *infos = g_ptr_array_new_with_free_func(shortcut_info_free);

    for (gsize i = 0; i < n; i++) {
        GVariant *item = g_variant_get_child_value(array, i);

        g_ptr_array_add(infos, parse_info(item));
        g_variant_unref(item);
    }
    g_variant_unref(array);
    return infos;
}

const ShortcutInfo *shortcuts_find(const GPtrArray *infos, const gchar *action)
{
    if (infos == NULL || action == NULL) {
        return NULL;
    }
    for (guint i = 0; i < infos->len; i++) {
        const ShortcutInfo *info = g_ptr_array_index(infos, i);

        if (g_strcmp0(info->action_unique, action) == 0) {
            return info;
        }
    }
    return NULL;
}

gboolean shortcut_info_has_key(const ShortcutInfo *info, gint key)
{
    if (info == NULL) {
        return FALSE;
    }
    for (guint i = 0; i < info->current_keys->len; i++) {
        if (g_array_index(info->current_keys, gint, i) == key) {
            return TRUE;
        }
    }
    for (guint i = 0; i < info->default_keys->len; i++) {
        if (g_array_index(info->default_keys, gint, i) == key) {
            return TRUE;
        }
    }
    return FALSE;
}

gboolean shortcuts_binding_conflicts(const ShortcutBinding *binding,
                                    const ShortcutInfo *native)
{
    return binding != NULL && native != NULL
        && shortcut_info_has_key(native, binding->key);
}

gchar *shortcuts_keys_to_string(const GArray *keys)
{
    GString *text = g_string_new(NULL);

    if (keys != NULL) {
        for (guint i = 0; i < keys->len; i++) {
            if (i > 0) {
                g_string_append_c(text, ';');
            }
            g_string_append_printf(text, "%d",
                                   g_array_index(keys, gint, i));
        }
    }
    return g_string_free(text, FALSE);
}

gboolean shortcuts_string_to_keys(const gchar *text, GArray *keys)
{
    g_return_val_if_fail(keys != NULL, FALSE);

    if (text == NULL || *text == '\0') {
        return TRUE;
    }
    gchar **parts = g_strsplit(text, ";", -1);
    gboolean ok = TRUE;

    for (gint i = 0; parts[i] != NULL && ok; i++) {
        if (parts[i][0] == '\0') {
            ok = FALSE;
            break;
        }
        gchar *end = NULL;
        gint64 value = g_ascii_strtoll(parts[i], &end, 10);

        if (end == NULL || *end != '\0' || value < G_MININT
            || value > G_MAXINT) {
            ok = FALSE;
        } else {
            gint key = (gint)value;

            g_array_append_val(keys, key);
        }
    }
    g_strfreev(parts);
    if (!ok) {
        g_array_set_size(keys, 0);
    }
    return ok;
}

/* --- D-Bus -------------------------------------------------------- */

GPtrArray *shortcuts_read(GDBusConnection *bus, GError **error)
{
    if (bus == NULL) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED,
                    "oturum veriyolu yok");
        return NULL;
    }
    GVariant *reply = g_dbus_connection_call_sync(
        bus, KWIN_SERVICE, KWIN_COMPONENT_PATH, KWIN_COMPONENT_IFACE,
        "allShortcutInfos", NULL, G_VARIANT_TYPE("(a(ssssssaiai))"),
        G_DBUS_CALL_FLAGS_NONE, 3000, NULL, error);

    if (reply == NULL) {
        return NULL;
    }
    GPtrArray *infos = shortcuts_parse_infos(reply);

    g_variant_unref(reply);
    if (infos == NULL) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                    "kısayol tablosu beklenen biçimde değil");
    }
    return infos;
}

static gboolean write_keys(GDBusConnection *bus, const gchar *component,
                           const gchar *action, const GArray *keys,
                           GError **error)
{
    const gchar *pair[] = { component, action, NULL };
    GVariantBuilder key_lists;
    GVariantBuilder inner;

    g_variant_builder_init(&key_lists, G_VARIANT_TYPE("a(ai)"));
    g_variant_builder_init(&inner, G_VARIANT_TYPE("ai"));
    if (keys != NULL) {
        for (guint i = 0; i < keys->len; i++) {
            g_variant_builder_add(&inner, "i",
                                  g_array_index(keys, gint, i));
        }
    }
    g_variant_builder_add(&key_lists, "(@ai)",
                          g_variant_builder_end(&inner));

    GVariant *reply = g_dbus_connection_call_sync(
        bus, KWIN_SERVICE, "/kglobalaccel", KWIN_GLOBAL_IFACE,
        "setForeignShortcutKeys",
        g_variant_new("(^asa(ai))", pair, &key_lists),
        NULL, G_DBUS_CALL_FLAGS_NONE, 3000, NULL, error);

    if (reply == NULL) {
        return FALSE;
    }
    g_variant_unref(reply);
    return TRUE;
}

static GArray *copy_keys(const GArray *keys)
{
    GArray *copy = g_array_sized_new(FALSE, FALSE, sizeof(gint),
                                     keys != NULL ? keys->len : 0);

    if (keys != NULL) {
        g_array_append_vals(copy, keys->data, keys->len);
    }
    return copy;
}

/* --- Yedek dosyası ------------------------------------------------ */

typedef struct {
    gchar *component;
    gchar *action;
    gchar *context;
    GArray *keys;
} ShortcutBackup;

static void backup_free(gpointer data)
{
    ShortcutBackup *backup = data;

    if (backup == NULL) {
        return;
    }
    g_free(backup->component);
    g_free(backup->action);
    g_free(backup->context);
    g_clear_pointer(&backup->keys, g_array_unref);
    g_free(backup);
}

static gchar *backup_path(void)
{
    gchar *dir = config_dir();
    gchar *path = g_build_filename(dir, BACKUP_FILE, NULL);

    g_free(dir);
    return path;
}

static gboolean backup_load(GKeyFile *key_file, const gchar *path)
{
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        return TRUE;
    }
    return g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, NULL);
}

static gboolean backup_save_one(const ShortcutBackup *backup, GError **error)
{
    gchar *path = backup_path();
    GKeyFile *file = g_key_file_new();
    gboolean ok = backup_load(file, path);

    if (!ok) {
        g_key_file_free(file);
        file = g_key_file_new();
    }
    gchar *keys_text = shortcuts_keys_to_string(backup->keys);
    gsize count = 0;
    gchar **groups = g_key_file_get_groups(file, &count);
    gchar *group = NULL;

    for (gsize i = 0; i < count && group == NULL; i++) {
        gchar *action = g_key_file_get_string(file, groups[i], "action", NULL);

        if (g_strcmp0(action, backup->action) == 0) {
            group = g_strdup(groups[i]);
        }
        g_free(action);
    }
    g_strfreev(groups);
    if (group == NULL) {
        group = g_strdup_printf("shortcut%u", (guint)count);
    }
    g_key_file_set_string(file, group, "component",
                          backup->component != NULL ? backup->component
                                                    : "kwin");
    g_key_file_set_string(file, group, "action",
                          backup->action != NULL ? backup->action : "");
    g_key_file_set_string(file, group, "context",
                          backup->context != NULL ? backup->context
                                                  : "default");
    g_key_file_set_string(file, group, "keys", keys_text);

    gchar *dir = config_dir();

    if (g_mkdir_with_parents(dir, 0700) != 0 && errno != EEXIST) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "%s oluşturulamadı", dir);
        ok = FALSE;
    } else {
        ok = g_key_file_save_to_file(file, path, error);
    }
    g_free(dir);
    g_free(keys_text);
    g_free(group);
    g_key_file_free(file);
    g_free(path);
    return ok;
}

static GPtrArray *backup_load_all(GError **error)
{
    gchar *path = backup_path();

    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_free(path);
        return g_ptr_array_new_with_free_func(backup_free);
    }
    GKeyFile *file = g_key_file_new();
    GPtrArray *items = g_ptr_array_new_with_free_func(backup_free);
    gsize count = 0;
    gchar **groups = NULL;

    if (!g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, error)) {
        g_free(path);
        g_key_file_free(file);
        g_ptr_array_unref(items);
        return NULL;
    }
    groups = g_key_file_get_groups(file, &count);
    for (gsize i = 0; i < count; i++) {
        ShortcutBackup *item = g_new0(ShortcutBackup, 1);
        gchar *keys_text = g_key_file_get_string(file, groups[i], "keys", NULL);

        item->component = g_key_file_get_string(file, groups[i],
                                                 "component", NULL);
        item->action = g_key_file_get_string(file, groups[i], "action", NULL);
        item->context = g_key_file_get_string(file, groups[i], "context", NULL);
        item->keys = g_array_new(FALSE, FALSE, sizeof(gint));
        if (shortcuts_string_to_keys(keys_text, item->keys)) {
            g_ptr_array_add(items, item);
        } else {
            backup_free(item);
        }
        g_free(keys_text);
    }
    g_strfreev(groups);
    g_key_file_free(file);
    g_free(path);
    return items;
}

/* --- Yerel kısayolu serbest bırakma ------------------------------- */

static gboolean native_released(GDBusConnection *bus, const gchar *action,
                                gint key)
{
    GError *error = NULL;
    GPtrArray *infos = shortcuts_read(bus, &error);

    if (infos == NULL) {
        g_clear_error(&error);
        return FALSE;
    }
    const ShortcutInfo *native = shortcuts_find(infos, action);
    gboolean released = native == NULL
        || !shortcut_info_has_key(native, key);

    g_ptr_array_unref(infos);
    return released;
}

static gboolean take_over(App *app, const ShortcutBinding *binding,
                          const ShortcutInfo *native)
{
    GError *error = NULL;
    GArray *empty = g_array_new(FALSE, FALSE, sizeof(gint));
    gboolean written = write_keys(app->bus, native->component_unique,
                                  binding->native, empty, &error);

    g_array_unref(empty);
    if (!written) {
        app_warn(app, "%s serbest bırakılamadı: %s", binding->native,
                 error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
        return FALSE;
    }
    if (!native_released(app->bus, binding->native, binding->key)) {
        app_warn(app, "%s yazma isteği doğrulanamadı (KDE sürümü izin vermiyor)",
                 binding->native);
        return FALSE;
    }
    ShortcutBackup backup = {
        .component = native->component_unique,
        .action = g_strdup(binding->native),
        .context = native->context_unique,
        .keys = copy_keys(native->current_keys)
    };

    if (!backup_save_one(&backup, &error)) {
        app_warn(app, "kısayol yedeği yazılamadı: %s",
                 error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
    }
    g_free(backup.action);
    g_array_unref(backup.keys);
    return TRUE;
}

/* --- Denetim ------------------------------------------------------ */

static gboolean set_flag(gboolean *field, gboolean value)
{
    if (*field == value) {
        return FALSE;
    }
    *field = value;
    return TRUE;
}

static void append_conflict(GString *text, const ShortcutBinding *binding,
                            const gchar *fallback)
{
    if (text->len > 0) {
        g_string_append_c(text, '\n');
    }
    g_string_append_printf(text, "%s (%s): %s", binding->label,
                           binding->sequence, fallback);
}

static gboolean check_binding(App *app, const GPtrArray *infos,
                              const ShortcutBinding *binding,
                              gboolean *flag, GString *conflicts)
{
    const ShortcutInfo *own = shortcuts_find(infos, binding->action);
    const ShortcutInfo *native = binding->native != NULL
        ? shortcuts_find(infos, binding->native) : NULL;
    gboolean native_bound = shortcuts_binding_conflicts(binding, native);

    if (!native_bound) {
        return set_flag(flag, FALSE);
    }
    if (binding->core && own != NULL
        && shortcut_info_has_key(own, binding->key)
        && take_over(app, binding, native)) {
        append_conflict(conflicts, binding,
                            _("Come Back Window kullanıyor"));
        return set_flag(flag, FALSE);
    }
    if (binding->core) {
        append_conflict(conflicts, binding,
                            _("KDE'nin yerel kısayolu kullanılıyor"));
    } else {
        append_conflict(conflicts, binding,
                            _("çakışmayan alternatif tuş kullanılacak"));
    }
    return set_flag(flag, TRUE);
}

void shortcuts_check(App *app)
{
    GError *error = NULL;
    GPtrArray *infos = shortcuts_read(app->bus, &error);

    if (infos == NULL) {
        app_warn(app, "kısayol tablosu okunamadı: %s",
                 error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
        return;
    }
    GString *conflicts = g_string_new(NULL);
    gboolean changed = FALSE;
    gboolean min_native = FALSE;
    gboolean max_native = FALSE;

    changed |= check_binding(app, infos, &bindings[0],
                             &app->native_show_desktop, conflicts);
    changed |= check_binding(app, infos, &bindings[2],
                             &app->native_close, conflicts);
    changed |= check_binding(app, infos, &bindings[3], &min_native, conflicts);
    changed |= check_binding(app, infos, &bindings[4], &max_native, conflicts);
    changed |= set_flag(&app->native_minimize, min_native);
    changed |= set_flag(&app->native_maximize, max_native);
    if (changed) {
        app_notify_settings(app);
    }
    if (conflicts->len > 0) {
        app_log(app, "kısayol çakışmaları:\n%s", conflicts->str);
        if (!app->shortcuts_warned) {
            gchar *body = g_strdup_printf(
                _("%s\n\nKDE Sistem Ayarları > Kısayollar bölümünden "
                  "düzenleyebilirsiniz: kcmshell6 keys"), conflicts->str);

            app_notify(app, "Come Back Window", body);
            g_free(body);
            app->shortcuts_warned = TRUE;
        }
    } else {
        app->shortcuts_warned = FALSE;
    }
    g_string_free(conflicts, TRUE);
    g_ptr_array_unref(infos);
}

/* --- Geri yükleme ------------------------------------------------- */

static gboolean has_current_key(const ShortcutInfo *info, gint key)
{
    if (info == NULL) {
        return FALSE;
    }
    for (guint i = 0; i < info->current_keys->len; i++) {
        if (g_array_index(info->current_keys, gint, i) == key) {
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean restore_verified(GDBusConnection *bus,
                                 const ShortcutBackup *item)
{
    GError *error = NULL;
    GPtrArray *infos = shortcuts_read(bus, &error);

    if (infos == NULL) {
        g_clear_error(&error);
        return FALSE;
    }
    const ShortcutInfo *info = shortcuts_find(infos, item->action);
    gboolean ok = info != NULL;

    for (guint i = 0; ok && i < item->keys->len; i++) {
        ok = has_current_key(info, g_array_index(item->keys, gint, i));
    }
    g_ptr_array_unref(infos);
    return ok;
}

gboolean shortcuts_restore(GDBusConnection *bus, gboolean *had_backup,
                           GError **error)
{
    if (had_backup != NULL) {
        *had_backup = FALSE;
    }
    GPtrArray *items = backup_load_all(error);

    if (items == NULL) {
        return FALSE;
    }
    if (items->len == 0) {
        g_ptr_array_unref(items);
        return TRUE;
    }
    if (had_backup != NULL) {
        *had_backup = TRUE;
    }
    for (guint i = 0; i < items->len; i++) {
        ShortcutBackup *item = g_ptr_array_index(items, i);

        if (item->action == NULL || item->action[0] == '\0') {
            continue;
        }
        if (!write_keys(bus, item->component != NULL ? item->component
                                                     : "kwin",
                        item->action, item->keys, error)) {
            g_ptr_array_unref(items);
            return FALSE;
        }
        if (item->keys->len > 0 && !restore_verified(bus, item)) {
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                        "%s geri yüklemesi doğrulanamadı", item->action);
            g_ptr_array_unref(items);
            return FALSE;
        }
    }
    gchar *path = backup_path();
    gboolean removed = g_unlink(path) == 0 || errno == ENOENT;

    if (!removed) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "%s silinemedi", path);
    }
    g_free(path);
    g_ptr_array_unref(items);
    return removed;
}

gboolean shortcuts_open_editor(GError **error)
{
    gchar *program = g_find_program_in_path("kcmshell6");
    gchar *argv_kcm[] = { "kcmshell6", "keys", NULL };
    gchar *argv_settings[] = { "systemsettings", "kcm_keys", NULL };
    gchar **argv = argv_kcm;

    if (program == NULL) {
        program = g_find_program_in_path("systemsettings");
        argv = argv_settings;
    }
    if (program == NULL) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                    "kcmshell6 veya systemsettings bulunamadı");
        return FALSE;
    }
    gboolean ok = g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                                NULL, NULL, NULL, error);

    g_free(program);
    return ok;
}
