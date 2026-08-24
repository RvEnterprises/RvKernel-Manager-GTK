#include "window.h"
#include "pages.h"
#include "style.h"

#include "../core/system_info.h"
#include "../util/sysfs.h"

typedef struct {
        GtkWidget *toast_revealer;
        GtkWidget *toast_label;
        guint toast_timeout_id;

        GtkStack *stack;
        guint timer_id;
} RvWindowCtx;

static void
ctx_free(gpointer data)
{
        RvWindowCtx *ctx = data;

        if (ctx->timer_id != 0)
                g_source_remove(ctx->timer_id);
        if (ctx->toast_timeout_id != 0)
                g_source_remove(ctx->toast_timeout_id);
        g_free(ctx);
}

static void
refresh_current(RvWindowCtx *ctx)
{
        rv_stack_refresh_visible(ctx->stack);
}

static gboolean
on_timer(gpointer user_data)
{
        RvWindowCtx *ctx = user_data;

        refresh_current(ctx);
        return G_SOURCE_CONTINUE;
}

static void
on_visible_child_changed(GtkStack *stack, GParamSpec *pspec,
                         gpointer user_data)
{
        (void)stack;
        (void)pspec;
        refresh_current(user_data);
}

static void
add_page(GtkStack *stack, GtkWidget *page, const gchar *name,
         const gchar *title, const gchar *icon)
{
        GtkStackPage *stack_page;

        gtk_stack_add_named(stack, page, name);
        stack_page = gtk_stack_get_page(stack, page);
        g_object_set(stack_page,
                     "title", title,
                     "icon-name", icon,
                     NULL);
}

static void
show_about(GtkWidget *window)
{
        GtkAboutDialog *about;
        const gchar *authors[] = { "Rve", NULL };

        about = GTK_ABOUT_DIALOG(gtk_about_dialog_new());
        gtk_about_dialog_set_program_name(about, RV_APP_NAME);
        gtk_about_dialog_set_version(about, RV_VERSION);
        gtk_about_dialog_set_comments(
                about,
                "Monitor and tune your Linux kernel: CPU governors and "
                "frequencies, GPU devfreq devices, battery health and "
                "charge limits, memory tunables and thermal zones.");
        gtk_about_dialog_set_website(
                about, "https://github.com/RvEnterprises/RvKernel-Manager-GTK");
        gtk_about_dialog_set_website_label(
                about, "github.com/RvEnterprises/RvKernel-Manager-GTK");
        gtk_about_dialog_set_license_type(about, GTK_LICENSE_GPL_3_0);
        gtk_about_dialog_set_copyright(about,
                                       "Copyright (C) 2026 RvEnterprises");
        gtk_about_dialog_set_authors(about, authors);
        gtk_about_dialog_set_logo_icon_name(about, RV_APP_ID);

        gtk_window_set_modal(GTK_WINDOW(about), TRUE);
        gtk_window_set_transient_for(GTK_WINDOW(about), GTK_WINDOW(window));
        gtk_window_present(GTK_WINDOW(about));
}

static void
on_about_clicked(GtkButton *button, gpointer user_data)
{
        (void)button;
        show_about(GTK_WIDGET(user_data));
}

static void
on_dark_toggled(GtkToggleButton *button, gpointer user_data)
{
        GtkSettings *settings = gtk_settings_get_default();

        g_object_set(settings,
                     "gtk-application-prefer-dark-theme",
                     gtk_toggle_button_get_active(button),
                     NULL);
        (void)user_data;
}

static GtkWidget *
build_headerbar(RvWindowCtx *ctx, GtkWidget *window)
{
        GtkWidget *header, *refresh_btn, *dark_btn, *about_btn;
        GtkWidget *name_label, *subtitle_label, *title_box;
        RvSystemInfo *info;
        gchar *subtitle;

        info = rv_system_info_get();
        subtitle = g_strdup_printf("%s · %s", info->distro, info->kernel);
        rv_system_info_free(info);

        header = gtk_header_bar_new();

        name_label = gtk_label_new(NULL);
        gtk_label_set_markup(
                GTK_LABEL(name_label),
                g_markup_printf_escaped("<b><big>%s</big></b>", RV_APP_NAME));

        subtitle_label = gtk_label_new(subtitle);
        gtk_widget_add_css_class(subtitle_label, "dim-label");
        gtk_widget_add_css_class(subtitle_label, "caption");

        title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_append(GTK_BOX(title_box), name_label);
        gtk_box_append(GTK_BOX(title_box), subtitle_label);
        gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), title_box);

        g_free(subtitle);

        refresh_btn = gtk_button_new_from_icon_name("view-refresh-symbolic");
        gtk_widget_set_tooltip_text(refresh_btn, "Refresh");
        g_signal_connect_swapped(refresh_btn, "clicked",
                                 G_CALLBACK(refresh_current), ctx);
        gtk_header_bar_pack_start(GTK_HEADER_BAR(header), refresh_btn);

        dark_btn = gtk_toggle_button_new();
        {
                GtkWidget *icon = gtk_image_new_from_icon_name(
                        "weather-clear-night-symbolic");
                gtk_button_set_child(GTK_BUTTON(dark_btn), icon);
        }
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dark_btn), TRUE);
        gtk_widget_set_tooltip_text(dark_btn, "Toggle dark style");
        g_signal_connect(dark_btn, "toggled", G_CALLBACK(on_dark_toggled),
                         NULL);
        gtk_header_bar_pack_end(GTK_HEADER_BAR(header), dark_btn);

        about_btn = gtk_button_new_from_icon_name("help-about-symbolic");
        gtk_widget_set_tooltip_text(about_btn, "About");
        g_signal_connect(about_btn, "clicked", G_CALLBACK(on_about_clicked),
                         window);
        gtk_header_bar_pack_end(GTK_HEADER_BAR(header), about_btn);

        return header;
}

static GtkWidget *
build_banner(void)
{
        GtkWidget *box, *icon, *label;

        box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_add_css_class(box, "rv-banner");

        icon = gtk_image_new_from_icon_name("dialog-warning-symbolic");
        gtk_box_append(GTK_BOX(box), icon);

        label = gtk_label_new(
                "Limited access — kernel parameters are read-only. "
                "Run as root to apply changes.");
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_set_hexpand(label, TRUE);
        gtk_box_append(GTK_BOX(box), label);

        return box;
}

GtkWidget *
rv_window_new(GtkApplication *app)
{
        GtkWidget *window, *header, *root_box, *overlay, *split;
        GtkWidget *sidebar_scroll, *sidebar, *main_area, *stack;
        GtkWidget *revealer, *toast_box, *toast_label;
        RvWindowCtx *ctx;

        rv_style_init();

        window = gtk_application_window_new(app);
        gtk_window_set_title(GTK_WINDOW(window), RV_APP_NAME);
        gtk_window_set_default_size(GTK_WINDOW(window), 1080, 720);
        gtk_window_set_icon_name(GTK_WINDOW(window), RV_APP_ID);

        ctx = g_new0(RvWindowCtx, 1);
        g_object_set_data_full(G_OBJECT(window), "rv-window-ctx", ctx,
                               ctx_free);

        header = build_headerbar(ctx, window);
        gtk_window_set_titlebar(GTK_WINDOW(window), header);

        overlay = gtk_overlay_new();
        gtk_window_set_child(GTK_WINDOW(window), overlay);

        root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_overlay_set_child(GTK_OVERLAY(overlay), root_box);

        if (!rv_is_root())
                gtk_box_append(GTK_BOX(root_box), build_banner());

        split = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_vexpand(split, TRUE);
        gtk_box_append(GTK_BOX(root_box), split);

        sidebar = gtk_stack_sidebar_new();
        gtk_widget_add_css_class(sidebar, "rv-sidebar");
        sidebar_scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sidebar_scroll),
                                      sidebar);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar_scroll),
                                       GTK_POLICY_NEVER,
                                       GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(sidebar_scroll, 190, -1);
        gtk_box_append(GTK_BOX(split), sidebar_scroll);

        main_area = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(main_area),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);

        stack = gtk_stack_new();
        gtk_stack_set_transition_type(
                GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
        gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(sidebar),
                                    GTK_STACK(stack));
        ctx->stack = GTK_STACK(stack);

        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(main_area), stack);
        gtk_widget_set_vexpand(main_area, TRUE);
        gtk_box_append(GTK_BOX(split), main_area);

        add_page(GTK_STACK(stack), rv_page_dashboard_new(window),
                 "dashboard", "Dashboard", "computer-symbolic");
        add_page(GTK_STACK(stack), rv_page_cpu_new(window),
                 "cpu", "CPU", "applications-engineering-symbolic");
        add_page(GTK_STACK(stack), rv_page_gpu_new(window),
                 "gpu", "GPU", "video-display-symbolic");
        add_page(GTK_STACK(stack), rv_page_battery_new(window),
                 "battery", "Battery", "battery-symbolic");
        add_page(GTK_STACK(stack), rv_page_memory_new(window),
                 "memory", "Memory", "drive-multidisk-symbolic");
        add_page(GTK_STACK(stack), rv_page_thermal_new(window),
                 "thermal", "Thermal", "temperature-symbolic");
        add_page(GTK_STACK(stack), rv_page_about_new(window),
                 "about", "About", "help-about-symbolic");

        g_signal_connect(stack, "notify::visible-child",
                         G_CALLBACK(on_visible_child_changed), ctx);

        revealer = gtk_revealer_new();
        gtk_revealer_set_transition_type(
                GTK_REVEALER(revealer),
                GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
        gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), FALSE);
        gtk_widget_set_valign(revealer, GTK_ALIGN_END);
        gtk_widget_set_halign(revealer, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_bottom(revealer, 18);

        toast_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(toast_box, "app-notification");
        toast_label = gtk_label_new(NULL);
        gtk_label_set_wrap(GTK_LABEL(toast_label), TRUE);
        gtk_box_append(GTK_BOX(toast_box), toast_label);

        gtk_revealer_set_child(GTK_REVEALER(revealer), toast_box);
        ctx->toast_revealer = revealer;
        ctx->toast_label = toast_label;

        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), revealer);

        refresh_current(ctx);
        ctx->timer_id = g_timeout_add_seconds(2, on_timer, ctx);

        return window;
}

static gboolean
hide_toast(gpointer user_data)
{
        RvWindowCtx *ctx = user_data;

        gtk_revealer_set_reveal_child(GTK_REVEALER(ctx->toast_revealer),
                                      FALSE);
        ctx->toast_timeout_id = 0;
        return G_SOURCE_REMOVE;
}

void
rv_window_show_toast(GtkWidget *window, const gchar *message)
{
        RvWindowCtx *ctx;

        g_return_if_fail(GTK_IS_WIDGET(window));

        ctx = g_object_get_data(G_OBJECT(window), "rv-window-ctx");
        if (ctx == NULL || ctx->toast_revealer == NULL)
                return;

        if (ctx->toast_timeout_id != 0) {
                g_source_remove(ctx->toast_timeout_id);
                ctx->toast_timeout_id = 0;
        }

        gtk_label_set_text(GTK_LABEL(ctx->toast_label), message);
        gtk_revealer_set_reveal_child(GTK_REVEALER(ctx->toast_revealer),
                                      TRUE);
        ctx->toast_timeout_id =
                g_timeout_add_seconds(4, hide_toast, ctx);
}
