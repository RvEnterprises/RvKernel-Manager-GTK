#include "pages.h"
#include "widgets.h"
#include "gauge.h"

#include "../core/system_info.h"
#include "../core/battery.h"
#include "../core/memory.h"
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

        GtkWidget      *cpu_gauge;
        GtkWidget      *ram_gauge;
        GtkWidget      *ram_detail;
        GtkWidget      *zram_gauge;
        GtkWidget      *zram_detail;
        GtkWidget      *zram_section;
        GtkWidget      *swap_gauge;
        GtkWidget      *swap_detail;
        GtkWidget      *swap_section;

        RvZram        **zrams;
        gsize           n_zrams;

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

static gchar *
usage_line(guint64 used_bytes, guint64 total_bytes)
{
        gchar *used_s = rv_format_bytes(used_bytes);
        gchar *total_s = rv_format_bytes(total_bytes);
        gchar *out = g_strdup_printf("%s / %s", used_s, total_s);

        g_free(used_s);
        g_free(total_s);
        return out;
}

static void
set_severity(GtkWidget *widget, gdouble value)
{
        gtk_widget_remove_css_class(widget, "rv-sev-warn");
        gtk_widget_remove_css_class(widget, "rv-sev-crit");

        if (value >= 0.85)
                gtk_widget_add_css_class(widget, "rv-sev-crit");
        else if (value >= 0.60)
                gtk_widget_add_css_class(widget, "rv-sev-warn");
}

static void
update_ring(GtkWidget *gauge, GtkWidget *detail, gdouble frac,
            guint64 used_bytes, guint64 total_bytes)
{
        gchar *text;

        rv_gauge_set_fraction(gauge, frac);
        set_severity(gauge, frac);

        text = usage_line(used_bytes, total_bytes);
        gtk_label_set_text(GTK_LABEL(detail), text);
        g_free(text);
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

        per_core = g_new0(gdouble, ctx->n_cores + 1);
        rv_cpu_usage_sample(ctx->samples, per_core, ctx->n_cores, &overall);
        g_free(per_core);

        overall = CLAMP(overall < 0 ? 0 : overall, 0.0, 1.0);
        rv_gauge_set_fraction(ctx->cpu_gauge, overall);
        set_severity(ctx->cpu_gauge, overall);

        if (info->mem_total_kb > 0) {
                gdouble frac =
                        CLAMP((gdouble)(info->mem_total_kb -
                                        info->mem_available_kb) /
                                      (gdouble)info->mem_total_kb,
                              0.0, 1.0);

                rv_gauge_set_fraction(ctx->ram_gauge, frac);
                rv_gauge_set_text(ctx->ram_gauge, "%d%%",
                                  (gint)(frac * 100));
                set_severity(ctx->ram_gauge, frac);
                text = mem_line(info->mem_total_kb,
                                info->mem_available_kb);
                gtk_label_set_text(GTK_LABEL(ctx->ram_detail), text);
                g_free(text);
        }

        {
                guint64 used = 0, size = 0;
                gboolean have = FALSE;

                for (gsize i = 0; i < ctx->n_zrams; i++) {
                        RvZram *z = ctx->zrams[i];

                        rv_zram_refresh(z);
                        if (z->has_stats && z->disksize_bytes > 0) {
                                have = TRUE;
                                used += z->used_bytes;
                                size += z->disksize_bytes;
                        }
                }

                gtk_widget_set_visible(GTK_WIDGET(ctx->zram_section),
                                       have);
                if (have) {
                        update_ring(ctx->zram_gauge, ctx->zram_detail,
                                    CLAMP((gdouble)used / (gdouble)size,
                                          0.0, 1.0),
                                    used, size);
                }
        }

        if (info->disk_swap_total_kb > 0) {
                update_ring(ctx->swap_gauge, ctx->swap_detail,
                            CLAMP((gdouble)info->disk_swap_used_kb /
                                          (gdouble)info->
                                                  disk_swap_total_kb,
                                  0.0, 1.0),
                            info->disk_swap_used_kb * 1024ULL,
                            info->disk_swap_total_kb * 1024ULL);
        } else {
                gtk_widget_set_visible(GTK_WIDGET(ctx->swap_section),
                                       FALSE);
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

        rv_system_info_free(info);
        return card;
}

static GtkWidget *
gauge_column(GtkWidget *gauge, GtkWidget *detail)
{
        GtkWidget *column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

        gtk_widget_set_halign(column, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(gauge, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(column), gauge);

        if (detail != NULL) {
                gtk_label_set_ellipsize(GTK_LABEL(detail),
                                        PANGO_ELLIPSIZE_END);
                gtk_label_set_xalign(GTK_LABEL(detail), 0.5f);
                gtk_widget_add_css_class(detail, "dim-label");
                gtk_box_append(GTK_BOX(column), detail);
        }

        return column;
}

static GtkWidget *
build_monitor_card(DashCtx *ctx)
{
        GtkWidget *card, *grid;

        card = rv_card_new("Live monitor");

        grid = gtk_flow_box_new();
        gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(grid),
                                        GTK_SELECTION_NONE);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(grid), 4);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(grid), 2);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(grid), 16);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(grid), 12);
        gtk_widget_add_css_class(grid, "rv-section");
        gtk_widget_set_hexpand(grid, TRUE);
        gtk_widget_set_valign(grid, GTK_ALIGN_START);

        ctx->cpu_gauge = rv_gauge_new("CPU");
        gtk_flow_box_append(GTK_FLOW_BOX(grid),
                            gauge_column(ctx->cpu_gauge, NULL));

        ctx->ram_detail = gtk_label_new("-");
        ctx->ram_gauge = rv_gauge_new("RAM");
        gtk_flow_box_append(GTK_FLOW_BOX(grid),
                            gauge_column(ctx->ram_gauge,
                                         ctx->ram_detail));

        ctx->zram_detail = gtk_label_new("-");
        ctx->zram_gauge = rv_gauge_new("ZRAM");
        ctx->zram_section = gauge_column(ctx->zram_gauge,
                                         ctx->zram_detail);
        gtk_flow_box_append(GTK_FLOW_BOX(grid), ctx->zram_section);

        ctx->swap_detail = gtk_label_new("-");
        ctx->swap_gauge = rv_gauge_new("Swap");
        ctx->swap_section = gauge_column(ctx->swap_gauge,
                                         ctx->swap_detail);
        gtk_flow_box_append(GTK_FLOW_BOX(grid), ctx->swap_section);

        gtk_box_append(GTK_BOX(card), grid);

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
        ctx->zrams = rv_zram_list(&ctx->n_zrams);

        title = gtk_label_new("Dashboard");
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_widget_add_css_class(title, "rv-title");
        gtk_box_append(GTK_BOX(content), title);

        gtk_box_append(GTK_BOX(content), build_system_card(ctx));
        gtk_box_append(GTK_BOX(content),
                       build_monitor_card(ctx));

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
        if (ctx->zrams != NULL)
                rv_zram_list_free(ctx->zrams, ctx->n_zrams);
        g_clear_pointer(&ctx->battery_rows, g_ptr_array_unref);
        if (ctx->supplies != NULL)
                rv_power_supply_list_free(ctx->supplies, ctx->n_supplies);
        g_free(ctx);
}
