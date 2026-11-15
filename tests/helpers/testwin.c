/*
 * Entegrasyon testleri için basit GTK4 penceresi
 *
 * Kullanım: testwin [--title BAŞLIK] [--seconds N]
 * Pencere eşlendiğinde stdout'a "HAZIR" yazar; --seconds verilirse süre
 * dolunca kapanır. KWin'in pencere küçültme/geri getirme yollarını sınamak
 * için iki örnek birlikte çalıştırılır.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <gtk/gtk.h>
#pragma GCC diagnostic pop

#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    const gchar *title;
    guint seconds;
} Options;

static gboolean quit_later(gpointer data)
{
    g_application_quit(G_APPLICATION(data));
    return G_SOURCE_REMOVE;
}

static void activate(GtkApplication *app, gpointer user_data)
{
    const Options *opts = user_data;
    GtkWidget *window = gtk_application_window_new(app);
    GtkWidget *label = gtk_label_new("Come Back Window test penceresi");

    gtk_window_set_title(GTK_WINDOW(window), opts->title);
    gtk_window_set_default_size(GTK_WINDOW(window), 360, 220);
    gtk_window_set_child(GTK_WINDOW(window), label);
    gtk_window_present(GTK_WINDOW(window));
    g_print("HAZIR\n");
    fflush(stdout);
    if (opts->seconds > 0) {
        g_timeout_add_seconds(opts->seconds, quit_later, app);
    }
}

int main(int argc, char **argv)
{
    Options opts = { "Come Back Window test", 0 };

    for (int i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], "--title") == 0 && i + 1 < argc) {
            opts.title = argv[++i];
        } else if (g_strcmp0(argv[i], "--seconds") == 0 && i + 1 < argc) {
            opts.seconds = (guint)g_ascii_strtoull(argv[++i], NULL, 10);
        }
    }
    /* Her örnek ayrı uygulama kimliği kullanır; aksi hâlde ikinci süreç
     * birinciye etkinleştirme iletir ve kendi penceresini açmaz. */
    gchar *app_id = g_strdup_printf("org.comebackwindow.TestWin.w%d",
                                    (gint)getpid());
    GtkApplication *app = gtk_application_new(app_id,
                                              G_APPLICATION_DEFAULT_FLAGS);

    g_free(app_id);

    g_signal_connect(app, "activate", G_CALLBACK(activate), &opts);
    gint status = g_application_run(G_APPLICATION(app), 1, argv);

    g_object_unref(app);
    return status;
}
