#ifndef UI_WINDOW_H
#define UI_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define APP_ID "com.rve.RvKernelManager"
#define APP_NAME "RvKernel Manager"
#define VERSION "1.0.0"

GtkWidget *window_new       (GtkApplication *app);
void       window_show_toast(GtkWidget   *window,
                                const gchar *message);

G_END_DECLS

#endif
