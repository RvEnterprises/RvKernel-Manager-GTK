#include "style.h"

static const gchar *RV_CSS = ""
".rv-page { padding: 18px 24px 24px; }"
""
".rv-title {"
"  font-size: 26px;"
"  font-weight: 800;"
"  margin-bottom: 2px;"
"}"
""
".rv-subtitle { margin-bottom: 14px; }"
""
".rv-card {"
"  background-color: alpha(@theme_fg_color, 0.045);"
"  border: 1px solid alpha(@theme_fg_color, 0.12);"
"  border-radius: 12px;"
"}"
""
".rv-card-title {"
"  font-weight: 700;"
"  font-size: 108%;"
"  padding: 10px 16px 8px;"
"}"
""
".rv-row { padding: 7px 16px; }"
""
".rv-key { min-width: 150px; }"
""
".rv-value { font-weight: 600; }"
""
".rv-banner {"
"  padding: 7px 18px;"
"  border-bottom: 1px solid @borders;"
"}"
""
".rv-section { padding: 12px 16px; }"
""
".rv-gauge { color: @theme_selected_bg_color; }"
""
".rv-sev-warn { color: #f5c211; }"
""
".rv-sev-crit { color: #e01b24; }"
""
".rv-battery-bar {"
"  min-height: 10px;"
"}"
""
".rv-sidebar {"
"  background-color: transparent;"
"  padding-top: 6px;"
"}";

void
rv_style_init(void)
{
        static gboolean initialized = FALSE;
        GtkCssProvider *provider;
        GdkDisplay *display;

        if (initialized)
                return;
        initialized = TRUE;

        provider = gtk_css_provider_new();
        gtk_css_provider_load_from_string(provider, RV_CSS);

        display = gdk_display_get_default();
        gtk_style_context_add_provider_for_display(
                display,
                GTK_STYLE_PROVIDER(provider),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_ref_sink(provider);
}
