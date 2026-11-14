/* Uygulama durumu ve modüller arası ortak tipler */
#pragma once

#include "config.h"

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _Strip Strip;
typedef struct _Peek Peek;
typedef struct _Tray Tray;

typedef struct {
    gchar *output;
    gint x;
    gint y;
    gint w;
    gint h;
    gchar *edge;      /* bottom/top/left/right/none */
    gint thickness;
    gdouble scale;
    gboolean fullscreen;
} LayoutEntry;

typedef struct _App App;

struct _App {
    GMainLoop *loop;
    Settings settings;
    gboolean settings_dirty;
    gboolean debug;
    gboolean no_tray;
    gboolean no_strip;
    gboolean test_mode;

    GDBusConnection *bus;
    guint owner_id;
    guint registration_id;
    guint settings_emit_id;
    guint kwin_watch_id;

    gchar *data_dir;
    LayoutEntry *layout;
    gsize layout_len;

    GQueue commands;          /* gchar* — PollCommands kuyruğu */
    GMutex commands_lock;

    gchar *last_state_json;
    gchar *last_layout_json;
    gchar *last_event_name;
    gchar *last_event_json;

    gpointer kwin;            /* KwinBridge* */
    Strip *strip;
    Peek *peek;
    Tray *tray;
    gpointer settings_dialog;
    gpointer setup_dialog;

    gboolean shortcuts_warned;
    gboolean kwin_failed_notified;

    /* Kısayol çakışma yönetimi sonucu */
    gboolean native_show_desktop;
    gboolean native_close;
    gchar *command_channel;   /* "invoke" veya "poll" */
    gchar *state_json;
};

/* Günlük yardımcıları */
void app_log(App *app, const gchar *format, ...) G_GNUC_PRINTF(2, 3);
void app_warn(App *app, const gchar *format, ...) G_GNUC_PRINTF(2, 3);

/* Veri dizini: CBW_DATA_DIR veya $(PREFIX)/share/comebackwindow */
gchar *app_data_dir(void);

/* Betik dosyasının tam yolu. */
gchar *app_kwin_script_path(void);

/* Ayarları yükler, gerekirse varsayılanları yazar. */
gboolean app_load_settings(App *app, GError **error);

/* Ayarları kaydeder ve betiğe/şeride yansıtır. */
gboolean app_save_settings(App *app, GError **error);

/* Değişen ayarları dinleyicilere duyurur (signal + kısayol). */
void app_notify_settings(App *app);

/* Betiğe verilecek tam ayar JSON'u (kısayol/kanal bilgileri dahil). */
gchar *app_settings_json(App *app);

/* Betiğe komut gönderir (kanal A, yoksa kuyruğa ekler). */
void app_send_command(App *app, const gchar *command);

/* Kuyruktan komut çeker; dizi g_free ile serbest bırakılır. */
gchar **app_take_commands(App *app, gsize *count);

/* Düzen girdilerini temizler. */
void app_clear_layout(App *app);

/* Bildirim gösterir (freedesktop). */
void app_notify(App *app, const gchar *title, const gchar *body);

G_END_DECLS
