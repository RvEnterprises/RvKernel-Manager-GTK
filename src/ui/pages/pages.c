#include "pages.h"

#include "../../util/log.h"

void page_set_refresh(GtkWidget *page, PageRefreshFunc fn)
{
	g_object_set_data(G_OBJECT(page), "refresh-func", fn);
}

void page_refresh(GtkWidget *page)
{
	PageRefreshFunc fn;

	if (page == NULL)
		return;

	fn = g_object_get_data(G_OBJECT(page), "refresh-func");
	if (fn != NULL)
		fn(page);
}

void stack_refresh_visible(GtkStack *stack)
{
	if (stack == NULL)
		return;
	log_debug("refresh page '%s'", gtk_stack_get_visible_child_name(stack));
	page_refresh(gtk_stack_get_visible_child(stack));
}

const gchar *window_error_text(GError **error)
{
	return error != NULL && *error != NULL ? (*error)->message :
						 "unknown error";
}
