#include "pages.h"
#include "../components/widgets.h"
#include "../window.h"

#include "../../core/system_info.h"
#include "../../util/sysfs.h"

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

#ifdef __clang__
#define BUILD_COMPILER                                      \
	("clang " STRINGIFY(__clang_major__) "." STRINGIFY( \
		__clang_minor__) "." STRINGIFY(__clang_patchlevel__))
#else
#define BUILD_COMPILER                             \
	("gcc " STRINGIFY(__GNUC__) "." STRINGIFY( \
		__GNUC_MINOR__) "." STRINGIFY(__GNUC_PATCHLEVEL__))
#endif

typedef struct {
	GtkWidget *root_row;
	SystemInfo *info;
} AboutCtx;

static void about_ctx_free(AboutCtx *ctx);

static void refresh(GtkWidget *page)
{
	AboutCtx *ctx = g_object_get_data(G_OBJECT(page), "ctx");

	if (ctx == NULL)
		return;

	if (is_root())
		kv_set(ctx->root_row, "%s",
		       "Yes — kernel parameters are writable");
	else
		kv_set(ctx->root_row, "%s", "No — read-only mode");
}

GtkWidget *page_about_new(GtkWidget *window)
{
	GtkWidget *scrolled, *content, *title;
	GtkWidget *card, *row;
	AboutCtx *ctx;

	content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
	gtk_widget_add_css_class(content, "page");

	ctx = g_new0(AboutCtx, 1);
	ctx->info = NULL;
	ctx->root_row = NULL;

	title = gtk_label_new("About");
	gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
	gtk_widget_add_css_class(title, "title");
	gtk_box_append(GTK_BOX(content), title);

	card = card_new(APP_NAME);

	row = kv_row("Version");
	kv_set(row, "%s", VERSION);
	card_add(card, row);

	row = kv_row("Compiler");
	kv_set(row, "%s", BUILD_COMPILER);
	card_add(card, row);

	row = kv_row("Developer");
	kv_set(row, "%s", "Rve");
	card_add(card, row);

	row = kv_row("Organization");
	kv_set(row, "%s", "RvEnterprises");
	card_add(card, row);

	row = kv_row("Project");
	kv_set(row, "github.com/RvEnterprises/RvKernel-Manager-GTK");
	card_add(card, row);

	row = kv_row("Original project");
	kv_set(row, "github.com/Rve27/RvKernel-Manager");
	card_add(card, row);

	row = kv_row("License");
	kv_set(row, "GNU GPL v3 or later");
	card_add(card, row);

	ctx->root_row = kv_row("Root access");
	card_add(card, ctx->root_row);

	gtk_box_append(GTK_BOX(content), card);

	scrolled = page_wrap(content);
	g_object_set_data_full(G_OBJECT(scrolled), "ctx", ctx,
			       (GDestroyNotify)about_ctx_free);
	page_set_refresh(scrolled, refresh);

	return scrolled;
}

static void about_ctx_free(AboutCtx *ctx)
{
	g_free(ctx);
}
