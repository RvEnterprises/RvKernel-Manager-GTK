#include "application.h"
#include "ui/window.h"
#include "ui/style.h"

static GtkWidget *
find_window(GtkApplication *app)
{
        GList *windows = gtk_application_get_windows(app);

        return windows != NULL ? GTK_WIDGET(windows->data) : NULL;
}

GResource *
icons_get_resource(void);

static void
on_activate(GtkApplication *app, gpointer user_data)
{
        GtkWidget *window;

        (void)user_data;
        rv_style_init();
        g_resources_register(icons_get_resource());
        gtk_icon_theme_add_resource_path(
                gtk_icon_theme_get_for_display(gdk_display_get_default()),
                "/com/rve/RvKernelManager/icons/hicolor");

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
