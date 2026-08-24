#include "pages.h"
#include "widgets.h"
#include "window.h"

#include "../core/gpu.h"
#include "../util/format.h"
#include "../util/sysfs.h"

typedef struct {
        GtkWidget  *window;
        RvGpuCard  *card;
        GtkWidget *cur_freq_row;
        GtkWidget *busy_row;
        GtkWidget *gov_row;
        GtkWidget *min_row;
        GtkWidget *max_row;
} GpuUi;

typedef struct {
        RvGpuCard **cards;
        gsize       n_cards;
        GPtrArray  *uis;
} GpuCtx;

static void gpu_ctx_free(GpuCtx *ctx);

static void
show_write_error(GtkWidget *window, GError **error)
{
        gchar *msg = g_strdup_printf("Failed to apply setting: %s",
                                     rv_window_error_text(error));
        rv_window_show_toast(window, msg);
        g_free(msg);
}

static void
on_gov_selected(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
        GpuUi *ui = user_data;
        const gchar *id = rv_option_row_active_id(ui->gov_row);
        GError *error = NULL;

        (void)dropdown;
        (void)pspec;

        if (id == NULL)
                return;

        if (!rv_devfreq_set_governor(ui->card->devfreq, id, &error))
                show_write_error(ui->window, &error);
        g_clear_error(&error);
}

static void
on_min_selected(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
        GpuUi *ui = user_data;
        const gchar *id = rv_option_row_active_id(ui->min_row);
        GError *error = NULL;

        (void)dropdown;
        (void)pspec;

        if (id == NULL)
                return;

        if (!rv_devfreq_set_min_freq(ui->card->devfreq,
                                     g_ascii_strtoll(id, NULL, 10), &error))
                show_write_error(ui->window, &error);
        g_clear_error(&error);
}

static void
on_max_selected(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
        GpuUi *ui = user_data;
        const gchar *id = rv_option_row_active_id(ui->max_row);
        GError *error = NULL;

        (void)dropdown;
        (void)pspec;

        if (id == NULL)
                return;

        if (!rv_devfreq_set_max_freq(ui->card->devfreq,
                                     g_ascii_strtoll(id, NULL, 10), &error))
                show_write_error(ui->window, &error);
        g_clear_error(&error);
}

static void
fill_devfreq_controls(GpuUi *ui, GtkWidget *card)
{
        RvDevfreq *d = ui->card->devfreq;
        gint64 *levels = NULL;
        gsize n_levels = 0;
        gchar *avail_text;

        if (d == NULL)
                return;

        if (d->governors[0] != NULL) {
                GtkWidget *row = rv_option_row_new("Governor");

                for (gsize i = 0; d->governors[i] != NULL; i++)
                        rv_option_row_append(row, d->governors[i],
                                             d->governors[i]);
                if (d->governor != NULL)
                        rv_option_row_select_id(row, d->governor);

                g_signal_connect(rv_option_row_dropdown(row),
                                 "notify::selected",
                                 G_CALLBACK(on_gov_selected), ui);
                rv_card_add(card, row);
                ui->gov_row = row;
        }

        avail_text = rv_read_trimmed(g_build_filename(
                d->devfreq_path, "available_frequencies", NULL));
        if (avail_text != NULL && avail_text[0] != '\0')
                levels = rv_parse_int_list(avail_text, &n_levels);
        g_free(avail_text);

        if (levels != NULL && n_levels >= 2) {
                const struct {
                        const gchar *title;
                        const gchar *current;
                        GCallback cb;
                        GtkWidget **slot;
                } specs[] = {
                        { "Minimum frequency", d->min_freq_hz,
                          G_CALLBACK(on_min_selected), &ui->min_row },
                        { "Maximum frequency", d->max_freq_hz,
                          G_CALLBACK(on_max_selected), &ui->max_row },
                };

                for (gint which = 0; which < 2; which++) {
                        GtkWidget *row = rv_option_row_new(specs[which].title);

                        for (gsize i = 0; i < n_levels; i++) {
                                gchar *id =
                                        g_strdup_printf("%" G_GINT64_FORMAT,
                                                        levels[i]);
                                gchar *label = rv_format_hz(levels[i]);

                                rv_option_row_append(row, id, label);
                                g_free(id);
                                g_free(label);
                        }

                        if (specs[which].current != NULL)
                                rv_option_row_select_id(
                                        row, specs[which].current);

                        g_signal_connect(rv_option_row_dropdown(row),
                                         "notify::selected",
                                         specs[which].cb, ui);
                        rv_card_add(card, row);
                        *specs[which].slot = row;
                }
        }

        free(levels);
}

static GtkWidget *
build_card_widget(RvGpuCard *info, GpuUi *ui)
{
        GtkWidget *card, *row;
        gchar *header;

        header = g_strdup_printf("%s (%s)", info->card_name,
                                 info->vendor_name);
        card = rv_card_new(header);
        g_free(header);

        row = rv_kv_row("Driver");
        rv_kv_set(row, "%s", info->driver != NULL ? info->driver : "-");
        rv_card_add(card, row);

        row = rv_kv_row("PCI ID");
        rv_kv_set(row, "%s", info->pci_id != NULL ? info->pci_id : "-");
        rv_card_add(card, row);

        if (info->has_busy_percent) {
                row = rv_kv_row("Utilization");
                rv_kv_set(row, "%s %%",
                          info->busy_percent != NULL ? info->busy_percent :
                                                       "-");
                rv_card_add(card, row);
                ui->busy_row = row;
        }

        row = rv_kv_row("Current frequency");
        rv_kv_set(row, "-");
        rv_card_add(card, row);
        ui->cur_freq_row = row;

        fill_devfreq_controls(ui, card);

        return card;
}

static void
refresh(GtkWidget *page)
{
        GpuCtx *ctx = g_object_get_data(G_OBJECT(page), "rv-ctx");

        if (ctx == NULL)
                return;

        for (gsize i = 0; i < ctx->uis->len; i++) {
                GpuUi *ui = g_ptr_array_index(ctx->uis, i);
                RvDevfreq *d;

                rv_gpu_card_refresh(ui->card);

                if (ui->busy_row != NULL && ui->card->busy_percent != NULL)
                        rv_kv_set(ui->busy_row, "%s %%",
                                  ui->card->busy_percent);

                d = ui->card->devfreq;
                if (d != NULL && d->cur_freq_hz != NULL) {
                        gint64 hz = g_ascii_strtoll(d->cur_freq_hz, NULL, 10);
                        gchar *s = rv_format_hz(hz);

                        rv_kv_set(ui->cur_freq_row, "%s", s);
                        g_free(s);
                } else if (ui->card->cur_clock_note != NULL) {
                        rv_kv_set(ui->cur_freq_row, "%s",
                                  ui->card->cur_clock_note);
                }
        }
}

GtkWidget *
rv_page_gpu_new(GtkWidget *window)
{
        GtkWidget *scrolled, *content, *title;
        GpuCtx *ctx;

        content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_widget_add_css_class(content, "rv-page");

        ctx = g_new0(GpuCtx, 1);
        ctx->uis = g_ptr_array_new_with_free_func(g_free);
        ctx->cards = rv_gpu_cards(&ctx->n_cards);

        title = gtk_label_new("GPU");
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_widget_add_css_class(title, "rv-title");
        gtk_box_append(GTK_BOX(content), title);

        if (ctx->n_cards == 0) {
                GtkWidget *label = gtk_label_new(
                        "No supported GPU devices found.");
                gtk_widget_add_css_class(label, "dim-label");
                gtk_box_append(GTK_BOX(content), label);
        }

        for (gsize i = 0; i < ctx->n_cards; i++) {
                GpuUi *ui = g_new0(GpuUi, 1);

                ui->window = window;
                ui->card = ctx->cards[i];

                gtk_box_append(GTK_BOX(content),
                               build_card_widget(ctx->cards[i], ui));
                g_ptr_array_add(ctx->uis, ui);
        }

        scrolled = rv_page_wrap(content);
        g_object_set_data_full(G_OBJECT(scrolled), "rv-ctx", ctx,
                               (GDestroyNotify)gpu_ctx_free);
        rv_page_set_refresh(scrolled, refresh);

        return scrolled;
}

static void
gpu_ctx_free(GpuCtx *ctx)
{
        g_clear_pointer(&ctx->uis, g_ptr_array_unref);
        if (ctx->cards != NULL)
                rv_gpu_cards_free(ctx->cards, ctx->n_cards);
        g_free(ctx);
}
