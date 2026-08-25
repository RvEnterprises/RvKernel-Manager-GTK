#include "style.h"

/*
 * Material 3 color scheme generated with Material Theme Builder
 * (material-theme/css/{light,dark}.css). GTK CSS has no custom
 * properties, so the exported tokens are baked into these palettes
 * and the matching variant is swapped at runtime.
 */
typedef struct {
        const gchar *surface;
        const gchar *on_surface;
        const gchar *on_surface_variant;
        const gchar *surface_container;
        const gchar *outline_variant;
        const gchar *secondary_container;
        const gchar *on_secondary_container;
        const gchar *primary;
        const gchar *error;
        const gchar *warn;
} MaterialPalette;

static const MaterialPalette palette_light = {
        .surface = "rgb(249,249,255)",
        .on_surface = "rgb(25,28,32)",
        .on_surface_variant = "rgb(68,71,78)",
        .surface_container = "rgb(237,237,244)",
        .outline_variant = "rgb(196,198,208)",
        .secondary_container = "rgb(218,226,249)",
        .on_secondary_container = "rgb(62,71,89)",
        .primary = "rgb(65,95,145)",
        .error = "rgb(186,26,26)",
        .warn = "rgb(154,103,0)",
};

static const MaterialPalette palette_dark = {
        .surface = "rgb(17,19,24)",
        .on_surface = "rgb(226,226,233)",
        .on_surface_variant = "rgb(196,198,208)",
        .surface_container = "rgb(29,32,36)",
        .outline_variant = "rgb(68,71,78)",
        .secondary_container = "rgb(62,71,89)",
        .on_secondary_container = "rgb(218,226,249)",
        .primary = "rgb(170,199,255)",
        .error = "rgb(255,180,171)",
        .warn = "rgb(245,194,17)",
};

static GtkCssProvider *active_provider = NULL;
static gint active_dark = -1;

static gchar *
build_css(const MaterialPalette *p)
{
        return g_strdup_printf(
                "window {"
                "  background-color: %s;"
                "  color: %s;"
                "}"
                "headerbar {"
                "  background-color: %s;"
                "  color: %s;"
                "}"
                ".page { padding: 18px 24px 24px; }"
                ".title {"
                "  font-size: 26px;"
                "  font-weight: 800;"
                "  margin-bottom: 2px;"
                "}"
                ".subtitle { margin-bottom: 14px; color: %s; }"
                ".card {"
                "  background-color: %s;"
                "  border: 1px solid %s;"
                "  border-radius: 12px;"
                "}"
                ".card-title {"
                "  font-weight: 700;"
                "  font-size: 108%%;"
                "  padding: 10px 16px 8px;"
                "}"
                ".row { padding: 7px 16px; }"
                ".key { min-width: 150px; color: %s; }"
                ".value { font-weight: 600; }"
                ".banner {"
                "  padding: 7px 18px;"
                "  background-color: %s;"
                "  color: %s;"
                "}"
                ".section { padding: 12px 16px; }"
                ".circular-progress-indicator { color: %s; }"
                ".sev-warn { color: %s; }"
                ".sev-crit { color: %s; }"
                ".battery-bar { min-height: 10px; }"
                ".battery-bar trough {"
                "  background-color: alpha(%s, 0.12);"
                "}"
                ".battery-bar block.filled { background-color: %s; }"
                ".sidebar-shell {"
                "  box-shadow: 1px 0 2px rgba(0,0,0,0.30),"
                "              2px 0 6px rgba(0,0,0,0.15);"
                "}"
                ".sidebar {"
                "  background-color: %s;"
                "  padding-top: 6px;"
                "}"
                ".sidebar row { border-radius: 8px; }"
                ".sidebar row:hover {"
                "  background-color: alpha(%s, 0.08);"
                "}"
                ".sidebar row:selected {"
                "  background-color: %s;"
                "  color: %s;"
                "}",
                p->surface, p->on_surface,
                p->surface, p->on_surface,
                p->on_surface_variant,
                p->surface_container,
                p->outline_variant,
                p->on_surface_variant,
                p->secondary_container,
                p->on_secondary_container,
                p->primary,
                p->warn,
                p->error,
                p->on_surface,
                p->primary,
                p->surface_container,
                p->on_surface,
                p->secondary_container,
                p->on_secondary_container);
}

void
style_set_dark(gboolean dark)
{
        const MaterialPalette *palette;
        GtkCssProvider *provider;
        GdkDisplay *display;
        gchar *css;

        if (active_dark == (gint)dark)
                return;
        active_dark = (gint)dark;

        palette = dark ? &palette_dark : &palette_light;
        provider = gtk_css_provider_new();
        css = build_css(palette);
        gtk_css_provider_load_from_string(provider, css);
        g_free(css);

        display = gdk_display_get_default();
        if (active_provider != NULL) {
                gtk_style_context_remove_provider_for_display(
                        display,
                        GTK_STYLE_PROVIDER(active_provider));
                g_object_unref(active_provider);
        }
        gtk_style_context_add_provider_for_display(
                display,
                GTK_STYLE_PROVIDER(provider),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        active_provider = provider;
}

void
style_init(void)
{
        static gboolean initialized = FALSE;

        if (initialized)
                return;
        initialized = TRUE;

        style_set_dark(FALSE);
}
