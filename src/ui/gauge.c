#include "gauge.h"

#include <math.h>
#include <pango/pangocairo.h>

#define KEY_STATE "rv-gauge-state"
#define KEY_CAPTION "rv-gauge-caption"

typedef struct {
        gdouble fraction;
        gchar  *text;
} RvGaugeState;

static void
gauge_state_free(gpointer data)
{
        RvGaugeState *state = data;

        g_free(state->text);
        g_free(state);
}

static void
gauge_draw(GtkDrawingArea *area, cairo_t *cr, gint width, gint height,
           gpointer user_data)
{
        PangoContext *pango_ctx;
        PangoFontDescription *desc;
        PangoLayout *layout;
        GtkWidget *widget;
        GdkRGBA color;
        GdkRGBA dim;
        RvGaugeState *state;
        const gchar *value, *caption;
        gdouble center_x, center_y, radius, stroke, sweep;
        gint side, tw, th;

        (void)user_data;

        widget = GTK_WIDGET(area);
        state = g_object_get_data(G_OBJECT(area), KEY_STATE);
        caption = g_object_get_data(G_OBJECT(area), KEY_CAPTION);
        value = state != NULL ? state->text : "";

        gtk_widget_get_color(widget, &color);

        side = MIN(width, height);
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

        pango_ctx = gtk_widget_get_pango_context(widget);
        desc = pango_font_description_copy(
                pango_context_get_font_description(pango_ctx));

        pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
        pango_font_description_set_absolute_size(
                desc, side * 0.20 * PANGO_SCALE);
        layout = gtk_widget_create_pango_layout(widget, NULL);
        pango_layout_set_font_description(layout, desc);
        pango_layout_set_text(layout, value, -1);
        pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
        pango_layout_get_pixel_size(layout, &tw, &th);

        {
                PangoLayout *caption_layout;
                gint ctw, cth;
                gdouble block_h, text_y;

                pango_font_description_set_weight(
                        desc, PANGO_WEIGHT_NORMAL);
                pango_font_description_set_absolute_size(
                        desc, side * 0.105 * PANGO_SCALE);
                caption_layout =
                        gtk_widget_create_pango_layout(widget, NULL);
                pango_layout_set_font_description(caption_layout,
                                                  desc);
                pango_layout_set_text(caption_layout,
                                      caption != NULL ? caption : "",
                                      -1);
                pango_layout_set_alignment(caption_layout,
                                           PANGO_ALIGN_CENTER);
                pango_layout_get_pixel_size(caption_layout,
                                            &ctw, &cth);

                block_h = th + side * 0.02 + cth;
                text_y = center_y - block_h / 2.0;

                gdk_cairo_set_source_rgba(cr, &color);
                cairo_move_to(cr, center_x - tw / 2.0, text_y);
                pango_cairo_show_layout(cr, layout);

                gdk_cairo_set_source_rgba(cr, &dim);
                cairo_move_to(cr, center_x - ctw / 2.0,
                              text_y + th + side * 0.02);
                pango_cairo_show_layout(cr, caption_layout);

                g_object_unref(caption_layout);
        }

        g_object_unref(layout);

        pango_font_description_free(desc);
}

GtkWidget *
rv_gauge_new(const gchar *caption)
{
        GtkWidget *widget;

        widget = gtk_drawing_area_new();
        gtk_widget_add_css_class(widget, "rv-gauge");
        gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(widget),
                                           148);
        gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(widget),
                                            148);
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(widget),
                                       gauge_draw, NULL, NULL);

        g_object_set_data_full(G_OBJECT(widget), KEY_STATE,
                               g_new0(RvGaugeState, 1),
                               gauge_state_free);
        g_object_set_data_full(G_OBJECT(widget), KEY_CAPTION,
                               g_strdup(caption != NULL ? caption : ""),
                               g_free);

        return widget;
}

void
rv_gauge_set_fraction(GtkWidget *gauge, gdouble fraction)
{
        RvGaugeState *state;

        g_return_if_fail(GTK_IS_WIDGET(gauge));

        state = g_object_get_data(G_OBJECT(gauge), KEY_STATE);
        if (state == NULL)
                return;

        fraction = CLAMP(fraction, 0.0, 1.0);
        if (fraction == state->fraction)
                return;

        state->fraction = fraction;
        gtk_widget_queue_draw(gauge);
}

void
rv_gauge_set_text(GtkWidget *gauge, const gchar *fmt, ...)
{
        RvGaugeState *state;
        va_list args;
        gchar *text;

        g_return_if_fail(GTK_IS_WIDGET(gauge));
        g_return_if_fail(fmt != NULL);

        state = g_object_get_data(G_OBJECT(gauge), KEY_STATE);
        if (state == NULL)
                return;

        va_start(args, fmt);
        text = g_strdup_vprintf(fmt, args);
        va_end(args);

        g_free(state->text);
        state->text = text;
        gtk_widget_queue_draw(gauge);
}
