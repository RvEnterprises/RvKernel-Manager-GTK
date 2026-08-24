#ifndef RV_UI_WINDOW_H
#define RV_UI_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define RV_APP_ID "com.rve.RvKernelManager"
#define RV_APP_NAME "RvKernel Manager"
#define RV_VERSION "1.0.0"

GtkWidget *rv_window_new       (GtkApplication *app);
void       rv_window_show_toast(GtkWidget   *window,
                                const gchar *message);

G_END_DECLS

#endif
