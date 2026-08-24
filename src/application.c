#include "application.h"
#include "ui/window.h"
#include "ui/style.h"

static GtkWidget *
find_window(GtkApplication *app)
{
        GList *windows = gtk_application_get_windows(app);

        return windows != NULL ? GTK_WIDGET(windows->data) : NULL;
}

static void
on_activate(GtkApplication *app, gpointer user_data)
{
        GtkWidget *window;

        (void)user_data;
        rv_style_init();

        window = find_window(app);
        if (window == NULL)
                window = rv_window_new(app);

        gtk_window_present(GTK_WINDOW(window));
}

GtkApplication *
rv_application_new(void)
{
        GtkApplication *app;

        app = gtk_application_new(RV_APP_ID, G_APPLICATION_DEFAULT_FLAGS);
        g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

        return app;
}
