/*
 * GTK4 ayarlar ve ilk açılış sihirbazı
 */
#include "dialogs.h"

#include "autostart.h"
#include "shortcuts.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <gtk/gtk.h>
#pragma GCC diagnostic pop

#define MARGIN 18

static const gchar *language_codes[] = { "auto", "tr", "en" };
static const gchar *language_names[] = {
    "Otomatik (sistem)", "Türkçe", "English"
};

typedef struct {
    GtkWidget *autostart;
    GtkWidget *strip;
    GtkWidget *strip_width;
    GtkWidget *fallback_height;
    GtkWidget *peek;
    GtkWidget *peek_delay;
    GtkWidget *peek_opacity;
    GtkWidget *shake;
    GtkWidget *tray;
    GtkWidget *language;
} Form;

typedef struct {
    App *app;
    GtkWidget *window;
    Form form;
} SettingsDialog;

typedef struct {
    App *app;
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *back;
    GtkWidget *next;
    GtkWidget *finish;
    Form form;
} SetupDialog;

gboolean dialogs_init(void)
{
    if (gtk_is_initialized()) {
        return TRUE;
    }
    if (!gtk_init_check()) {
        return FALSE;
    }
    /* Bazı temalar kaydırıcı düğümü için negatif en küçük boyut bildirir. */
    GtkCssProvider *provider = gtk_css_provider_new();

    gtk_css_provider_load_from_string(
        provider, "scale slider { min-width: 14px; min-height: 14px; }");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    return TRUE;
}

/* --- Ortak form yardımcıları -------------------------------------- */

static GtkWidget *form_row(const gchar *text, GtkWidget *widget)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *label = gtk_label_new(text);

    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(widget, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(row), label);
    gtk_box_append(GTK_BOX(row), widget);
    return row;
}

static GtkWidget *add_switch(GtkWidget *box, const gchar *text,
                             gboolean active)
{
    GtkWidget *widget = gtk_switch_new();

    gtk_switch_set_active(GTK_SWITCH(widget), active);
    gtk_box_append(GTK_BOX(box), form_row(text, widget));
    return widget;
}

static GtkWidget *add_spin(GtkWidget *box, const gchar *text, gdouble value,
                           gdouble lo, gdouble hi, gdouble step)
{
    GtkWidget *widget = gtk_spin_button_new_with_range(lo, hi, step);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), value);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(widget), TRUE);
    gtk_widget_set_size_request(widget, 110, -1);
    gtk_box_append(GTK_BOX(box), form_row(text, widget));
    return widget;
}

static GtkWidget *add_scale(GtkWidget *box, const gchar *text, gdouble value,
                            gdouble lo, gdouble hi, gdouble step)
{
    GtkWidget *widget = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                 lo, hi, step);

    gtk_range_set_value(GTK_RANGE(widget), value);
    gtk_scale_set_digits(GTK_SCALE(widget), 2);
    gtk_widget_set_size_request(widget, 180, -1);
    gtk_box_append(GTK_BOX(box), form_row(text, widget));
    return widget;
}

static GtkWidget *add_language(GtkWidget *box, const gchar *current)
{
    GtkWidget *widget = gtk_drop_down_new_from_strings(language_names);
    gint index = 0;

    for (gsize i = 0; i < G_N_ELEMENTS(language_codes); i++) {
        if (g_strcmp0(current, language_codes[i]) == 0) {
            index = (gint)i;
        }
    }
    gtk_drop_down_set_selected(GTK_DROP_DOWN(widget), (guint)index);
    gtk_box_append(GTK_BOX(box), form_row("Arayüz dili", widget));
    return widget;
}

static GtkWidget *add_heading(GtkWidget *box, const gchar *text,
                              const gchar *subtitle)
{
    GtkWidget *label = gtk_label_new(NULL);

    gtk_label_set_markup(GTK_LABEL(label), text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), label);
    if (subtitle != NULL) {
        GtkWidget *sub = gtk_label_new(subtitle);

        gtk_label_set_wrap(GTK_LABEL(sub), TRUE);
        gtk_widget_set_halign(sub, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(box), sub);
    }
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);

    gtk_box_append(GTK_BOX(box), sep);
    return label;
}

static GtkWidget *page_box(void)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

    gtk_widget_set_margin_start(box, MARGIN);
    gtk_widget_set_margin_end(box, MARGIN);
    gtk_widget_set_margin_top(box, MARGIN);
    gtk_widget_set_margin_bottom(box, MARGIN);
    return box;
}

static GtkWidget *add_general_fields(GtkWidget *box, Form *form,
                                     const Settings *s)
{
    form->autostart = add_switch(box, "Oturum açılışında başlat",
                                 s->autostart);
    form->strip = add_switch(box, "Köşe şeridini göster",
                             s->strip_enabled);
    form->strip_width = add_spin(box, "Şerit genişliği (px)",
                                 s->strip_width, 2, 24, 1);
    form->fallback_height = add_spin(box, "Panel yoksa yedek yükseklik",
                                     s->fallback_height, 20, 120, 2);
    return box;
}

static GtkWidget *add_effect_fields(GtkWidget *box, Form *form,
                                    const Settings *s)
{
    form->peek = add_switch(box, "Aero Peek", s->peek_enabled);
    form->peek_delay = add_spin(box, "Peek gecikmesi (ms)",
                                s->peek_delay_ms, 100, 2000, 50);
    form->peek_opacity = add_scale(box, "Peek opaklığı", s->peek_opacity,
                                   0.0, 0.5, 0.01);
    form->shake = add_switch(box, "Aero Shake", s->shake_enabled);
    form->tray = add_switch(box, "Tepsi simgesi", s->tray_enabled);
    return box;
}

/* --- Form değerlerini okuma --------------------------------------- */

static Settings form_read(const Form *form, const Settings *base)
{
    Settings s = *base;

    s.language = NULL;
    guint selected = form->language != NULL
        ? gtk_drop_down_get_selected(GTK_DROP_DOWN(form->language)) : 0;

    s.language = g_strdup(selected < G_N_ELEMENTS(language_codes)
                          ? language_codes[selected] : "auto");
    if (form->autostart != NULL) {
        s.autostart = gtk_switch_get_active(GTK_SWITCH(form->autostart));
    }
    if (form->strip != NULL) {
        s.strip_enabled = gtk_switch_get_active(GTK_SWITCH(form->strip));
    }
    if (form->strip_width != NULL) {
        s.strip_width = gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(form->strip_width));
    }
    if (form->fallback_height != NULL) {
        s.fallback_height = gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(form->fallback_height));
    }
    if (form->peek != NULL) {
        s.peek_enabled = gtk_switch_get_active(GTK_SWITCH(form->peek));
    }
    if (form->peek_delay != NULL) {
        s.peek_delay_ms = gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(form->peek_delay));
    }
    if (form->peek_opacity != NULL) {
        s.peek_opacity = gtk_range_get_value(GTK_RANGE(form->peek_opacity));
    }
    if (form->shake != NULL) {
        s.shake_enabled = gtk_switch_get_active(GTK_SWITCH(form->shake));
    }
    if (form->tray != NULL) {
        s.tray_enabled = gtk_switch_get_active(GTK_SWITCH(form->tray));
    }
    return s;
}

static void show_error(GtkWindow *parent, const gchar *message)
{
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", message);

    gtk_alert_dialog_show(dialog, parent);
    g_object_unref(dialog);
}

/* Ayar yapısının sahipliğini devralır; başarısızlıkta temizler. */
static gboolean apply_settings(App *app, Settings *next, GtkWindow *parent)
{
    GError *error = NULL;
    Settings backup = app->settings;

    app->settings = *next;
    if (!app_save_settings(app, &error)) {
        app->settings = backup;
        gchar *message = g_strdup_printf("Ayarlar kaydedilemedi: %s",
                                         error != NULL ? error->message
                                                       : "bilinmeyen hata");

        show_error(parent, message);
        g_free(message);
        g_clear_error(&error);
        settings_clear(next);
        return FALSE;
    }
    settings_clear(&backup);
    app_sync_autostart(app);
    app_notify_settings(app);
    return TRUE;
}

/* --- Ayarlar penceresi -------------------------------------------- */

static void settings_destroyed(GtkWidget *widget, gpointer user_data)
{
    App *app = user_data;

    (void)widget;
    app->settings_dialog = NULL;
}

static void on_settings_save(GtkButton *button, gpointer user_data)
{
    SettingsDialog *dialog = user_data;

    (void)button;
    Settings next = form_read(&dialog->form, &dialog->app->settings);

    if (apply_settings(dialog->app, &next, GTK_WINDOW(dialog->window))) {
        gtk_window_destroy(GTK_WINDOW(dialog->window));
    }
}

static void on_settings_shortcuts(GtkButton *button, gpointer user_data)
{
    SettingsDialog *dialog = user_data;
    GError *error = NULL;

    (void)button;
    if (!shortcuts_open_editor(&error)) {
        show_error(GTK_WINDOW(dialog->window),
                   error != NULL ? error->message : "Kısayollar açılamadı");
        g_clear_error(&error);
    }
}

static GtkWidget *dialog_buttons(SettingsDialog *dialog)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *shortcuts = gtk_button_new_with_label("Kısayollar…");
    GtkWidget *cancel = gtk_button_new_with_label("İptal");
    GtkWidget *save = gtk_button_new_with_label("Kaydet");

    gtk_widget_set_halign(row, GTK_ALIGN_END);
    gtk_widget_set_margin_top(row, 6);
    gtk_box_append(GTK_BOX(row), shortcuts);
    gtk_box_append(GTK_BOX(row), cancel);
    gtk_box_append(GTK_BOX(row), save);
    gtk_widget_add_css_class(save, "suggested-action");
    g_signal_connect(shortcuts, "clicked",
                     G_CALLBACK(on_settings_shortcuts), dialog);
    g_signal_connect_swapped(cancel, "clicked",
                             G_CALLBACK(gtk_window_destroy), dialog->window);
    g_signal_connect(save, "clicked", G_CALLBACK(on_settings_save), dialog);
    return row;
}

static GtkWidget *build_settings_window(SettingsDialog *dialog)
{
    Settings *s = &dialog->app->settings;
    GtkWidget *window = gtk_window_new();
    GtkWidget *box = page_box();
    GtkWidget *scroll = gtk_scrolled_window_new();

    gtk_window_set_title(GTK_WINDOW(window), "Come Back Window — Ayarlar");
    gtk_window_set_default_size(GTK_WINDOW(window), 540, 640);
    add_heading(box, "<b>Ayarlar</b>",
                "Değişiklikler Kaydet'e basınca uygulanır.");
    add_general_fields(box, &dialog->form, s);
    add_effect_fields(box, &dialog->form, s);
    dialog->form.language = add_language(box, s->language);
    gtk_box_append(GTK_BOX(box), dialog_buttons(dialog));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
    gtk_window_set_child(GTK_WINDOW(window), scroll);
    return window;
}

void dialogs_open_settings(App *app)
{
    if (!dialogs_init()) {
        app_warn(app, "GTK başlatılamadı; ayarlar penceresi açılamıyor");
        return;
    }
    if (app->settings_dialog != NULL) {
        gtk_window_present(GTK_WINDOW(
            ((SettingsDialog *)app->settings_dialog)->window));
        return;
    }
    SettingsDialog *dialog = g_new0(SettingsDialog, 1);

    dialog->app = app;
    dialog->window = build_settings_window(dialog);
    g_signal_connect(dialog->window, "destroy",
                     G_CALLBACK(settings_destroyed), app);
    g_object_set_data_full(G_OBJECT(dialog->window), "cbw-dialog",
                           dialog, g_free);
    app->settings_dialog = dialog;
    gtk_window_present(GTK_WINDOW(dialog->window));
}

/* --- İlk açılış sihirbazı ----------------------------------------- */

static void setup_destroyed(GtkWidget *widget, gpointer user_data)
{
    App *app = user_data;

    (void)widget;
    app->setup_dialog = NULL;
}

static void update_setup_buttons(SetupDialog *dialog)
{
    const gchar *page = gtk_stack_get_visible_child_name(
        GTK_STACK(dialog->stack));
    gboolean first = g_strcmp0(page, "lang") == 0;
    gboolean last = g_strcmp0(page, "effects") == 0;

    gtk_widget_set_sensitive(dialog->back, !first);
    gtk_widget_set_visible(dialog->next, !last);
    gtk_widget_set_visible(dialog->finish, last);
}

static void on_setup_page_changed(GtkStack *stack, GParamSpec *spec,
                                  gpointer user_data)
{
    (void)stack;
    (void)spec;
    update_setup_buttons(user_data);
}

static void on_setup_back(GtkButton *button, gpointer user_data)
{
    SetupDialog *dialog = user_data;
    const gchar *page = gtk_stack_get_visible_child_name(
        GTK_STACK(dialog->stack));

    (void)button;
    if (g_strcmp0(page, "general") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(dialog->stack), "lang");
    } else if (g_strcmp0(page, "effects") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(dialog->stack), "general");
    }
}

static void on_setup_next(GtkButton *button, gpointer user_data)
{
    SetupDialog *dialog = user_data;
    const gchar *page = gtk_stack_get_visible_child_name(
        GTK_STACK(dialog->stack));

    (void)button;
    if (g_strcmp0(page, "lang") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(dialog->stack), "general");
    } else if (g_strcmp0(page, "general") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(dialog->stack), "effects");
    }
}

static void on_setup_finish(GtkButton *button, gpointer user_data)
{
    SetupDialog *dialog = user_data;

    (void)button;
    Settings next = form_read(&dialog->form, &dialog->app->settings);

    next.first_run_done = TRUE;
    if (apply_settings(dialog->app, &next, GTK_WINDOW(dialog->window))) {
        gtk_window_destroy(GTK_WINDOW(dialog->window));
    }
}

static GtkWidget *setup_language_page(SetupDialog *dialog)
{
    GtkWidget *box = page_box();

    add_heading(box, "<b>Hoş geldiniz</b>",
                "Come Back Window, Windows tarzı pencere kısayollarını "
                "KDE Plasma 6'ya taşır. Birkaç adımda ayarlayalım.");
    dialog->form.language = add_language(box, dialog->app->settings.language);
    return box;
}

static GtkWidget *setup_general_page(SetupDialog *dialog)
{
    GtkWidget *box = page_box();

    add_heading(box, "<b>Başlangıç ve şerit</b>", NULL);
    add_general_fields(box, &dialog->form, &dialog->app->settings);
    return box;
}

static GtkWidget *setup_effects_page(SetupDialog *dialog)
{
    GtkWidget *box = page_box();

    add_heading(box, "<b>Efektler</b>",
                "Bu ayarları daha sonra tepsi menüsünden değiştirebilirsiniz.");
    add_effect_fields(box, &dialog->form, &dialog->app->settings);
    return box;
}

static GtkWidget *setup_buttons(SetupDialog *dialog)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    dialog->back = gtk_button_new_with_label("Geri");
    dialog->next = gtk_button_new_with_label("İleri");
    dialog->finish = gtk_button_new_with_label("Başla");
    gtk_widget_set_halign(row, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(row), dialog->back);
    gtk_box_append(GTK_BOX(row), dialog->next);
    gtk_box_append(GTK_BOX(row), dialog->finish);
    gtk_widget_add_css_class(dialog->finish, "suggested-action");
    g_signal_connect(dialog->back, "clicked",
                     G_CALLBACK(on_setup_back), dialog);
    g_signal_connect(dialog->next, "clicked",
                     G_CALLBACK(on_setup_next), dialog);
    g_signal_connect(dialog->finish, "clicked",
                     G_CALLBACK(on_setup_finish), dialog);
    return row;
}

static GtkWidget *build_setup_window(SetupDialog *dialog)
{
    GtkWidget *window = gtk_window_new();
    GtkWidget *outer = page_box();

    dialog->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(dialog->stack),
                                  GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_add_named(GTK_STACK(dialog->stack),
                        setup_language_page(dialog), "lang");
    gtk_stack_add_named(GTK_STACK(dialog->stack),
                        setup_general_page(dialog), "general");
    gtk_stack_add_named(GTK_STACK(dialog->stack),
                        setup_effects_page(dialog), "effects");
    gtk_window_set_title(GTK_WINDOW(window), "Come Back Window — İlk Açılış");
    gtk_window_set_default_size(GTK_WINDOW(window), 560, 560);
    gtk_box_append(GTK_BOX(outer), dialog->stack);
    gtk_box_append(GTK_BOX(outer), setup_buttons(dialog));
    gtk_window_set_child(GTK_WINDOW(window), outer);
    g_signal_connect(dialog->stack, "notify::visible-child",
                     G_CALLBACK(on_setup_page_changed), dialog);
    update_setup_buttons(dialog);
    return window;
}

void dialogs_open_setup(App *app)
{
    if (!dialogs_init()) {
        app_warn(app, "GTK başlatılamadı; kurulum penceresi açılamıyor");
        return;
    }
    if (app->setup_dialog != NULL) {
        gtk_window_present(GTK_WINDOW(
            ((SetupDialog *)app->setup_dialog)->window));
        return;
    }
    SetupDialog *dialog = g_new0(SetupDialog, 1);

    dialog->app = app;
    dialog->window = build_setup_window(dialog);
    g_signal_connect(dialog->window, "destroy",
                     G_CALLBACK(setup_destroyed), app);
    g_object_set_data_full(G_OBJECT(dialog->window), "cbw-dialog",
                           dialog, g_free);
    app->setup_dialog = dialog;
    gtk_window_present(GTK_WINDOW(dialog->window));
}

void dialogs_open_setup_if_needed(App *app)
{
    if (!app->settings.first_run_done) {
        dialogs_open_setup(app);
    }
}

void dialogs_shutdown(App *app)
{
    if (app->settings_dialog != NULL) {
        gtk_window_destroy(GTK_WINDOW(
            ((SettingsDialog *)app->settings_dialog)->window));
        app->settings_dialog = NULL;
    }
    if (app->setup_dialog != NULL) {
        gtk_window_destroy(GTK_WINDOW(
            ((SetupDialog *)app->setup_dialog)->window));
        app->setup_dialog = NULL;
    }
}
