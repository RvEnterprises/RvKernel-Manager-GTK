#include "circular_progress_indicator.h"

#include <math.h>
#include <pango/pangocairo.h>

#define KEY_STATE "cpi-state"
#define KEY_CAPTION "cpi-caption"

#define CPI_ANIM_DURATION_MS 600

typedef struct {
        gdouble fraction;
        gdouble target;
        gdouble start_frac;
        gint64  start_time;
        guint   tick_id;
        gchar  *text;

        PangoLayout *value_layout;
        PangoLayout *caption_layout;
        gchar  *layout_value;
        guint   font_stamp;
        gint    cache_side;
        gint    value_w;
        gint    value_h;
        gint    caption_w;
        gint    caption_h;
} CPIState;

static void
cpi_state_free(gpointer data)
{
        CPIState *state = data;

        g_clear_pointer(&state->value_layout, g_object_unref);
        g_clear_pointer(&state->caption_layout, g_object_unref);
        g_free(state->layout_value);
        g_free(state->text);
        g_free(state);
}

static gboolean
cpi_tick(GtkWidget *widget, GdkFrameClock *clock, gpointer user_data)
{
        CPIState *state;
        gdouble t;

        (void)user_data;

        state = g_object_get_data(G_OBJECT(widget), KEY_STATE);
        if (state == NULL || state->tick_id == 0)
                return G_SOURCE_REMOVE;

        t = (gdouble)(gdk_frame_clock_get_frame_time(clock) -
                      state->start_time) /
            (CPI_ANIM_DURATION_MS * 1000.0);
        if (t >= 1.0) {
                state->fraction = state->target;
                state->tick_id = 0;
                gtk_widget_queue_draw(widget);
                return G_SOURCE_REMOVE;
        }

        t = 1.0 - pow(1.0 - t, 3.0);
        state->fraction = state->start_frac +
                          (state->target - state->start_frac) * t;
        gtk_widget_queue_draw(widget);

        return G_SOURCE_CONTINUE;
}

static void
cpi_build_layouts(CPIState *state, GtkWidget *widget, gint side,
                  const gchar *value, const gchar *caption)
{
        PangoContext *ctx;
        PangoFontDescription *base;
        PangoFontDescription *desc;

        ctx = gtk_widget_get_pango_context(widget);
        base = pango_context_get_font_description(ctx);
        state->font_stamp =
                base != NULL ? pango_font_description_hash(base) : 0;

        desc = base != NULL ? pango_font_description_copy(base) :
                              pango_font_description_new();

        if (state->value_layout == NULL) {
                state->value_layout =
                        gtk_widget_create_pango_layout(widget, NULL);
                pango_layout_set_alignment(state->value_layout,
                                           PANGO_ALIGN_CENTER);
        }
        pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
        pango_font_description_set_absolute_size(
                desc, side * 0.20 * PANGO_SCALE);
        pango_layout_set_font_description(state->value_layout, desc);
        pango_layout_set_text(state->value_layout, value, -1);
        pango_layout_get_pixel_size(state->value_layout,
                                    &state->value_w, &state->value_h);

        if (state->caption_layout == NULL) {
                state->caption_layout =
                        gtk_widget_create_pango_layout(widget, NULL);
                pango_layout_set_alignment(state->caption_layout,
                                           PANGO_ALIGN_CENTER);
        }
        pango_font_description_set_weight(desc, PANGO_WEIGHT_NORMAL);
        pango_font_description_set_absolute_size(
                desc, side * 0.105 * PANGO_SCALE);
        pango_layout_set_font_description(state->caption_layout, desc);
        pango_layout_set_text(state->caption_layout,
                              caption != NULL ? caption : "", -1);
        pango_layout_get_pixel_size(state->caption_layout,
                                    &state->caption_w,
                                    &state->caption_h);

        pango_font_description_free(desc);

        g_free(state->layout_value);
        state->layout_value = g_strdup(value);
        state->cache_side = side;
}

static void
cpi_update_value_text(CPIState *state, const gchar *value)
{
        g_free(state->layout_value);
        state->layout_value = g_strdup(value);

        pango_layout_set_text(state->value_layout, value, -1);
        pango_layout_get_pixel_size(state->value_layout,
                                    &state->value_w, &state->value_h);
}

static void
cpi_draw(GtkDrawingArea *area, cairo_t *cr, gint width, gint height,
           gpointer user_data)
{
        PangoContext *pango_ctx;
        PangoFontDescription *font_desc;
        GtkWidget *widget;
        GdkRGBA color;
        GdkRGBA dim;
        CPIState *state;
        gchar value_buf[G_ASCII_DTOSTR_BUF_SIZE];
        const gchar *value, *caption;
        gdouble center_x, center_y, radius, stroke, sweep;
        guint stamp;
        gint side;

        (void)user_data;

        widget = GTK_WIDGET(area);
        state = g_object_get_data(G_OBJECT(area), KEY_STATE);
        caption = g_object_get_data(G_OBJECT(area), KEY_CAPTION);
        if (state == NULL)
                return;

        if (state->text != NULL && state->text[0] != '\0') {
                value = state->text;
        } else {
                g_snprintf(value_buf, sizeof(value_buf), "%d%%",
                           (gint)(state->fraction * 100.0 + 0.5));
                value = value_buf;
        }

        side = MIN(width, height);
        pango_ctx = gtk_widget_get_pango_context(widget);
        font_desc = pango_context_get_font_description(pango_ctx);
        stamp = font_desc != NULL ?
                pango_font_description_hash(font_desc) : 0;

        if (state->value_layout == NULL || state->caption_layout == NULL ||
            state->cache_side != side || state->font_stamp != stamp)
                cpi_build_layouts(state, widget, side, value, caption);
        else if (g_strcmp0(state->layout_value, value) != 0)
                cpi_update_value_text(state, value);

        gtk_widget_get_color(widget, &color);

        stroke = MAX(9.0, side * 0.085);
        radius = side / 2.0 - stroke / 2.0 - 1.0;
        center_x = width / 2.0;
        center_y = height / 2.0;

        dim = color;
        dim.alpha *= 0.62f;

        cairo_set_line_width(cr, stroke);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        {
                GdkRGBA track = color;

                track.alpha *= 0.18f;
                gdk_cairo_set_source_rgba(cr, &track);
                cairo_arc(cr, center_x, center_y, radius, 0.0,
                          2.0 * M_PI);
                cairo_stroke(cr);
        }

        if (state->fraction > 0.001) {
                sweep = 2.0 * M_PI * state->fraction;
                gdk_cairo_set_source_rgba(cr, &color);
                cairo_arc(cr, center_x, center_y, radius,
                          -M_PI / 2.0, -M_PI / 2.0 + sweep);
                cairo_stroke(cr);
        }

        {
                gdouble block_h = state->value_h + side * 0.02 +
                                  state->caption_h;
                gdouble text_y = center_y - block_h / 2.0;

                gdk_cairo_set_source_rgba(cr, &color);
                cairo_move_to(cr, center_x - state->value_w / 2.0,
                              text_y);
                pango_cairo_show_layout(cr, state->value_layout);

                gdk_cairo_set_source_rgba(cr, &dim);
                cairo_move_to(cr, center_x - state->caption_w / 2.0,
                              text_y + state->value_h + side * 0.02);
                pango_cairo_show_layout(cr, state->caption_layout);
        }
}

GtkWidget *
circular_progress_indicator_new(const gchar *caption)
{
        GtkWidget *widget;

        widget = gtk_drawing_area_new();
        gtk_widget_add_css_class(widget, "circular-progress-indicator");
        gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(widget),
                                           148);
        gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(widget),
                                            148);
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(widget),
                                       cpi_draw, NULL, NULL);

        g_object_set_data_full(G_OBJECT(widget), KEY_STATE,
                               g_new0(CPIState, 1),
                               cpi_state_free);
        g_object_set_data_full(G_OBJECT(widget), KEY_CAPTION,
                               g_strdup(caption != NULL ? caption : ""),
                               g_free);

        return widget;
}

void
circular_progress_indicator_set_fraction(GtkWidget *self, gdouble fraction)
{
        CPIState *state;

        g_return_if_fail(GTK_IS_WIDGET(self));

        state = g_object_get_data(G_OBJECT(self), KEY_STATE);
        if (state == NULL)
                return;

        fraction = CLAMP(fraction, 0.0, 1.0);
        if (fraction == state->target && state->tick_id == 0)
                return;

        state->target = fraction;
        state->start_frac = state->fraction;
        state->start_time = g_get_monotonic_time();

        if (state->tick_id == 0)
                state->tick_id =
                        gtk_widget_add_tick_callback(self, cpi_tick,
                                                     NULL, NULL);
}

void
circular_progress_indicator_set_text(GtkWidget *self, const gchar *fmt, ...)
{
        CPIState *state;
        va_list args;
        gchar *text;

        g_return_if_fail(GTK_IS_WIDGET(self));
        g_return_if_fail(fmt != NULL);

        state = g_object_get_data(G_OBJECT(self), KEY_STATE);
        if (state == NULL)
                return;

        va_start(args, fmt);
        text = g_strdup_vprintf(fmt, args);
        va_end(args);

        if (g_strcmp0(state->text, text) == 0) {
                g_free(text);
                return;
        }

        g_free(state->text);
        state->text = text;
        gtk_widget_queue_draw(self);
}
