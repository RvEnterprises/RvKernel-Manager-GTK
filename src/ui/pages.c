#include "pages.h"

void
rv_page_set_refresh(GtkWidget *page, RvPageRefreshFunc fn)
{
        g_object_set_data(G_OBJECT(page), "rv-refresh-func", fn);
}

void
rv_page_refresh(GtkWidget *page)
{
        RvPageRefreshFunc fn;

        if (page == NULL)
                return;

        fn = g_object_get_data(G_OBJECT(page), "rv-refresh-func");
        if (fn != NULL)
                fn(page);
}

void
rv_stack_refresh_visible(GtkStack *stack)
{
        if (stack == NULL)
                return;
        rv_page_refresh(gtk_stack_get_visible_child(stack));
}

const gchar *
rv_window_error_text(GError **error)
{
        return error != NULL && *error != NULL ?
               (*error)->message : "unknown error";
}
