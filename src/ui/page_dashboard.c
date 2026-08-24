#include "pages.h"
#include "widgets.h"

#include "../core/system_info.h"
#include "../core/battery.h"
#include "../util/format.h"
#include "../util/sysfs.h"

typedef struct {
        RvPowerSupply *ps;
        GtkWidget     *info_row;
        GtkLevelBar   *level_bar;
} DashBatteryRow;

typedef struct {
        GtkWidget      *window;
        RvCpuSample    *samples;
        gsize           n_cores;

        GtkWidget      *uptime_row;
        GtkWidget      *load_row;

        GtkProgressBar *overall_bar;
        GPtrArray      *core_bars;

        GtkProgressBar *mem_bar;
        GtkProgressBar *swap_bar;

        GPtrArray      *battery_rows;
        RvPowerSupply **supplies;
        gsize           n_supplies;
} DashCtx;

static void dash_ctx_free(DashCtx *ctx);

static gchar *
mem_line(guint64 total_kb, guint64 avail_kb)
{
        guint64 used_kb = total_kb > avail_kb ? total_kb - avail_kb : 0;
        gchar *used_s = rv_format_bytes(used_kb * 1024);
        gchar *total_s = rv_format_bytes(total_kb * 1024);
        gchar *out = g_strdup_printf("%s / %s", used_s, total_s);

        g_free(used_s);
        g_free(total_s);
        return out;
}

static void
refresh(GtkWidget *page)
{
        DashCtx *ctx;
        RvSystemInfo *info;
        gdouble *per_core, overall;
        gchar *text;

        ctx = g_object_get_data(G_OBJECT(page), "rv-ctx");
        if (ctx == NULL)
                return;

        info = rv_system_info_get();

        {
                gchar *uptime = rv_format_uptime(info->uptime_s);
                rv_kv_set(GTK_WIDGET(ctx->uptime_row), "%s", uptime);
                g_free(uptime);
        }
        rv_kv_set(GTK_WIDGET(ctx->load_row), "%s", info->loadavg);

        if (info->swap_total_kb == 0)
                gtk_widget_set_visible(GTK_WIDGET(ctx->swap_bar), FALSE);

        per_core = g_new0(gdouble, ctx->n_cores + 1);
        rv_cpu_usage_sample(ctx->samples, per_core, ctx->n_cores, &overall);

        for (gsize i = 0; i < ctx->core_bars->len; i++) {
                GtkProgressBar *bar = g_ptr_array_index(ctx->core_bars, i);
                gdouble v = per_core[i];
                gtk_progress_bar_set_fraction(bar, CLAMP(v < 0 ? 0 : v,
                                                         0.0, 1.0));
        }
        g_free(per_core);

        overall = CLAMP(overall < 0 ? 0 : overall, 0.0, 1.0);
        gtk_progress_bar_set_fraction(ctx->overall_bar, overall);
        text = g_strdup_printf("CPU %d%%", (gint)(overall * 100));
        gtk_progress_bar_set_text(ctx->overall_bar, text);
        g_free(text);

        if (info->mem_total_kb > 0) {
                gtk_progress_bar_set_fraction(
                        ctx->mem_bar,
                        CLAMP((gdouble)(info->mem_total_kb -
                                        info->mem_available_kb) /
                                      (gdouble)info->mem_total_kb,
                              0, 1));
                text = mem_line(info->mem_total_kb, info->mem_available_kb);
                gtk_progress_bar_set_text(ctx->mem_bar, text);
                g_free(text);
        }

        if (info->swap_total_kb > 0) {
                gtk_progress_bar_set_fraction(
                        ctx->swap_bar,
                        CLAMP((gdouble)(info->swap_total_kb -
                                        info->swap_free_kb) /
                                      (gdouble)info->swap_total_kb,
                              0, 1));
                text = mem_line(info->swap_total_kb, info->swap_free_kb);
                gtk_progress_bar_set_text(ctx->swap_bar, text);
                g_free(text);
        }

        for (gsize i = 0; i < ctx->battery_rows->len; i++) {
                DashBatteryRow *br = g_ptr_array_index(ctx->battery_rows, i);

                rv_power_supply_refresh(br->ps);
                rv_kv_set(br->info_row, "%d %% (%s)", br->ps->capacity,
                          br->ps->status != NULL ? br->ps->status : "?");
                gtk_level_bar_set_value(br->level_bar,
                                        CLAMP(br->ps->capacity, 0, 100) /
                                                100.0);
        }

        rv_system_info_free(info);
}

static GtkWidget *
build_system_card(DashCtx *ctx)
{
        GtkWidget *card, *row;
        RvSystemInfo *info = rv_system_info_get();

        card = rv_card_new("System");

        row = rv_kv_row("Hostname");
        rv_card_add(card, row);
        rv_kv_set(row, "%s", info->hostname);

        row = rv_kv_row("Operating system");
        rv_card_add(card, row);
        rv_kv_set(row, "%s", info->distro);

        row = rv_kv_row("Kernel");
        rv_card_add(card, row);
        rv_kv_set(row, "%s", info->kernel);

        row = rv_kv_row("Architecture");
        rv_card_add(card, row);
        rv_kv_set(row, "%s", info->arch);

        row = rv_kv_row("Processor");
        rv_card_add(card, row);
        rv_kv_set(row, "%s (%zu cores)", info->cpu_model, info->n_cores);

        row = rv_kv_row("Uptime");
        rv_card_add(card, row);
        ctx->uptime_row = row;

        row = rv_kv_row("Load average");
        rv_card_add(card, row);
        ctx->load_row = row;

        rv_system_info_free(info);
        return card;
}

static GtkWidget *
build_monitor_card(DashCtx *ctx, gsize n_cores)
{
        GtkWidget *card, *row;

        card = rv_card_new("Live monitor");

        ctx->overall_bar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
        gtk_progress_bar_set_show_text(ctx->overall_bar, TRUE);
        gtk_progress_bar_set_text(ctx->overall_bar, "CPU");

        row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_add_css_class(GTK_WIDGET(row), "rv-row");
        gtk_box_append(GTK_BOX(row), GTK_WIDGET(ctx->overall_bar));
        rv_card_add(card, GTK_WIDGET(row));

        {
                GtkWidget *grid = gtk_flow_box_new();
                guint per_line =
                        (guint)CLAMP((gint)n_cores, 2, 8);

                gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(grid),
                                                GTK_SELECTION_NONE);
                gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(grid),
                                                       per_line);
                gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(grid),
                                                       per_line < 4 ? per_line
                                                                    : 4);
                gtk_widget_add_css_class(grid, "rv-row");

                ctx->core_bars =
                        g_ptr_array_sized_new(n_cores);
                for (gsize i = 0; i < n_cores; i++) {
                        GtkWidget *cell = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
                        GtkWidget *label;
                        GtkWidget *bar;
                        gchar *text = g_strdup_printf("%zu", i);

                        label = gtk_label_new(text);
                        g_free(text);
                        gtk_widget_add_css_class(label, "dim-label");

                        bar = gtk_progress_bar_new();
                        gtk_widget_add_css_class(bar, "rv-core-bar");
                        gtk_widget_set_hexpand(bar, TRUE);

                        gtk_box_append(GTK_BOX(cell), label);
                        gtk_box_append(GTK_BOX(cell), bar);
                        gtk_flow_box_append(GTK_FLOW_BOX(grid), cell);

                        g_ptr_array_add(ctx->core_bars, bar);
                }

                rv_card_add(card, grid);
        }

        ctx->mem_bar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
        gtk_progress_bar_set_show_text(ctx->mem_bar, TRUE);
        gtk_progress_bar_set_text(ctx->mem_bar, "RAM");

        row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_add_css_class(GTK_WIDGET(row), "rv-row");
        gtk_box_append(GTK_BOX(row), GTK_WIDGET(ctx->mem_bar));
        rv_card_add(card, GTK_WIDGET(row));

        ctx->swap_bar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
        gtk_progress_bar_set_show_text(ctx->swap_bar, TRUE);
        gtk_progress_bar_set_text(ctx->swap_bar, "Swap");

        row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_add_css_class(GTK_WIDGET(row), "rv-row");
        gtk_box_append(GTK_BOX(row), GTK_WIDGET(ctx->swap_bar));
        rv_card_add(card, GTK_WIDGET(row));

        return card;
}

static GtkWidget *
build_battery_card(DashCtx *ctx)
{
        GtkWidget *card = NULL;
        RvPowerSupply **supplies = rv_power_supply_list(&ctx->n_supplies);

        ctx->supplies = supplies;
        ctx->battery_rows = g_ptr_array_new_with_free_func(g_free);

        for (gsize i = 0; i < ctx->n_supplies; i++) {
                RvPowerSupply *ps = supplies[i];
                DashBatteryRow *br;

                if (ps->kind != RV_PS_BATTERY || ps->capacity < 0)
                        continue;

                if (card == NULL)
                        card = rv_card_new("Battery");

                br = g_new0(DashBatteryRow, 1);
                br->ps = ps;

                br->info_row = rv_kv_row(ps->name);
                rv_card_add(card, br->info_row);

                br->level_bar = GTK_LEVEL_BAR(gtk_level_bar_new());
                gtk_level_bar_set_min_value(br->level_bar, 0.0);
                gtk_level_bar_set_max_value(br->level_bar, 1.0);
                {
                        GtkWidget *wrap =
                                gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
                        gtk_widget_add_css_class(wrap, "rv-row");
                        gtk_widget_add_css_class(GTK_WIDGET(br->level_bar),
                                                 "rv-battery-bar");
                        gtk_box_append(GTK_BOX(wrap),
                                       GTK_WIDGET(br->level_bar));
                        rv_card_add(card, wrap);
                }

                g_ptr_array_add(ctx->battery_rows, br);
        }

        g_free(supplies);
        return card;
}

GtkWidget *
rv_page_dashboard_new(GtkWidget *window)
{
        GtkWidget *scrolled, *content, *title, *card;
        DashCtx *ctx;

        content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_widget_add_css_class(content, "rv-page");

        ctx = g_new0(DashCtx, 1);
        ctx->window = window;
        ctx->n_cores = rv_cpu_sample_count();
        ctx->samples = g_new0(RvCpuSample, ctx->n_cores + 1);

        title = gtk_label_new("Dashboard");
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_widget_add_css_class(title, "rv-title");
        gtk_box_append(GTK_BOX(content), title);

        gtk_box_append(GTK_BOX(content), build_system_card(ctx));
        gtk_box_append(GTK_BOX(content),
                       build_monitor_card(ctx, ctx->n_cores));

        card = build_battery_card(ctx);
        if (card != NULL)
                gtk_box_append(GTK_BOX(content), card);

        scrolled = rv_page_wrap(content);
        g_object_set_data_full(G_OBJECT(scrolled), "rv-ctx", ctx,
                               (GDestroyNotify)dash_ctx_free);
        rv_page_set_refresh(scrolled, refresh);

        return scrolled;
}

static void
dash_ctx_free(DashCtx *ctx)
{
        g_free(ctx->samples);
        g_clear_pointer(&ctx->core_bars, g_ptr_array_unref);
        g_clear_pointer(&ctx->battery_rows, g_ptr_array_unref);
        if (ctx->supplies != NULL)
                rv_power_supply_list_free(ctx->supplies, ctx->n_supplies);
        g_free(ctx);
}
