#include "window.h"
#include "pages/pages.h"
#include "sidebar.h"
#include "theme/style.h"

#include "../core/system_info.h"
#include "../util/sysfs.h"

#include <pwd.h>
#include <string.h>
#include <unistd.h>

typedef struct {
        GtkWidget *toast_revealer;
        GtkWidget *toast_label;
        guint toast_timeout_id;

        GtkStack *stack;
        guint timer_id;
} WindowCtx;

static void
ctx_free(gpointer data)
{
        WindowCtx *ctx = data;

        if (ctx->timer_id != 0)
                g_source_remove(ctx->timer_id);
        if (ctx->toast_timeout_id != 0)
                g_source_remove(ctx->toast_timeout_id);
        g_free(ctx);
}

static void
refresh_current(WindowCtx *ctx)
{
        stack_refresh_visible(ctx->stack);
}

static gboolean
on_timer(gpointer user_data)
{
        WindowCtx *ctx = user_data;

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

static gboolean
theme_name_is_dark(void)
{
        GtkSettings *settings = gtk_settings_get_default();
        gchar *name = NULL;
        gchar *lower;
        gboolean dark;

        g_object_get(settings, "gtk-theme-name", &name, NULL);
        if (name == NULL)
                return FALSE;

        lower = g_ascii_strdown(name, -1);
        dark = strstr(lower, "dark") != NULL;
        g_free(lower);
        g_free(name);

        return dark;
}

/*
 * State dir of the user that launched the session. When elevated via
 * pkexec the environment only forwards XDG_RUNTIME_DIR, so the invoking
 * user's home is resolved through that uid instead of geteuid().
 */
static gchar *
invoking_state_dir(void)
{
        const gchar *env, *runtime_dir;
        struct passwd *pw;
        uid_t uid;

        if (geteuid() != 0) {
                env = g_getenv("XDG_STATE_HOME");
                if (env != NULL && env[0] != '\0')
                        return g_strdup(env);
                return g_build_filename(g_get_home_dir(), ".local",
                                        "state", NULL);
        }

        runtime_dir = g_getenv("XDG_RUNTIME_DIR");
        if (runtime_dir == NULL ||
            !g_str_has_prefix(runtime_dir, "/run/user/"))
                return NULL;

        uid = (uid_t)g_ascii_strtoull(runtime_dir + strlen("/run/user/"),
                                      NULL, 10);
        if (uid == 0)
                return NULL;

        pw = getpwuid(uid);
        if (pw == NULL || pw->pw_dir == NULL || pw->pw_dir[0] == '\0')
                return NULL;

        return g_build_filename(pw->pw_dir, ".local", "state", NULL);
}

/* Find "<key>": true/false in a small flat JSON document. */
static gboolean
json_bool_value(const gchar *json, const gchar *key, gboolean *value)
{
        const gchar *p;

        p = strstr(json, key);
        if (p == NULL)
                return FALSE;

        p += strlen(key);
        while (*p == ' ' || *p == ':' || *p == '\t' ||
               *p == '\n' || *p == '\r')
                p++;

        if (g_str_has_prefix(p, "true")) {
                *value = TRUE;
                return TRUE;
        }
        if (g_str_has_prefix(p, "false")) {
                *value = FALSE;
                return TRUE;
        }

        return FALSE;
}

/*
 * DankMaterialShell keeps its light/dark choice in
 * <state>/DankMaterialShell/session.json as "isLightMode" (default:
 * false, i.e. dark). This is the only dependable signal on Hyprland
 * sessions where no portal publishes the color scheme.
 */
static gboolean
dms_read_dark(gboolean *dark)
{
        gchar *state_dir, *path, *contents = NULL;
        gboolean light = FALSE;

        state_dir = invoking_state_dir();
        if (state_dir == NULL)
                return FALSE;

        path = g_build_filename(state_dir, "DankMaterialShell",
                                "session.json", NULL);
        g_free(state_dir);

        if (!g_file_get_contents(path, &contents, NULL, NULL)) {
                g_free(path);
                return FALSE;
        }
        g_free(path);

        if (!json_bool_value(contents, "\"isLightMode\"", &light)) {
                g_free(contents);
                return FALSE;
        }
        g_free(contents);

        *dark = !light;
        return TRUE;
}


static gboolean
system_prefers_dark(void)
{
        gboolean dark;

        if (dms_read_dark(&dark))
                return dark;
        return theme_name_is_dark();
}

typedef struct {
        GtkWidget *button;
} SchemeWatch;

static void
scheme_watch_free(SchemeWatch *watch)
{
        if (watch->button != NULL)
                g_object_remove_weak_pointer(G_OBJECT(watch->button),
                                             (gpointer *)&watch->button);
        g_free(watch);
}

static void
portal_scheme_done(GObject *source, GAsyncResult *result,
                   gpointer user_data)
{
        GDBusConnection *bus = G_DBUS_CONNECTION(source);
        GVariant *reply, *value;
        SchemeWatch *watch = user_data;
        GError *error = NULL;
        guint32 scheme = 0;

        reply = g_dbus_connection_call_finish(bus, result, &error);
        if (reply == NULL) {
                g_error_free(error);
                g_object_unref(bus);
                scheme_watch_free(watch);
                return;
        }

        g_variant_get(reply, "(v)", &value);
        scheme = g_variant_get_uint32(value);
        g_variant_unref(value);
        g_variant_unref(reply);
        g_object_unref(bus);

        /* 1 = prefer dark; 0 = no preference and 2 = prefer light. */
        if (watch->button != NULL &&
            gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(watch->button)) != (scheme == 1))
                gtk_toggle_button_set_active(
                        GTK_TOGGLE_BUTTON(watch->button), scheme == 1);

        scheme_watch_free(watch);
}

static void
session_bus_ready(GObject *source, GAsyncResult *result,
                  gpointer user_data)
{
        GDBusConnection *bus;
        SchemeWatch *watch = user_data;

        bus = g_bus_get_finish(result, NULL);
        if (bus == NULL) {
                scheme_watch_free(watch);
                return;
        }

        g_dbus_connection_call(bus,
                               "org.freedesktop.portal.Desktop",
                               "/org/freedesktop/portal/desktop",
                               "org.freedesktop.portal.Settings",
                               "ReadOne",
                               g_variant_new("(ss)",
                                             "org.freedesktop.appearance",
                                             "color-scheme"),
                               G_VARIANT_TYPE("(v)"),
                               G_DBUS_CALL_FLAGS_NO_AUTO_START,
                               300,
                               NULL,
                               portal_scheme_done,
                               watch);
}

static void
portal_scheme_async(GtkWidget *button)
{
        SchemeWatch *watch;

        if (g_getenv("DBUS_SESSION_BUS_ADDRESS") == NULL &&
            g_getenv("XDG_RUNTIME_DIR") == NULL)
                return;

        watch = g_new0(SchemeWatch, 1);
        watch->button = button;
        g_object_add_weak_pointer(G_OBJECT(button),
                                  (gpointer *)&watch->button);

        g_bus_get(G_BUS_TYPE_SESSION, NULL, session_bus_ready, watch);
}

static void
on_dark_toggled(GtkToggleButton *button, gpointer user_data)
{
        GtkSettings *settings = gtk_settings_get_default();
        gboolean dark = gtk_toggle_button_get_active(button);

        g_object_set(settings,
                     "gtk-application-prefer-dark-theme", dark,
                     NULL);
        style_set_dark(dark);
        (void)user_data;
}

static GtkWidget *
build_headerbar(WindowCtx *ctx, GtkWidget *window)
{
        GtkWidget *header, *dark_btn;
        GtkWidget *name_label, *subtitle_label, *title_box;
        SystemInfo *info;
        gchar *subtitle;

        info = system_info_get();
        subtitle = g_strdup_printf("%s · %s", info->distro, info->kernel);
        system_info_free(info);

        header = gtk_header_bar_new();

        name_label = gtk_label_new(NULL);
        gtk_label_set_markup(
                GTK_LABEL(name_label),
                g_markup_printf_escaped("<b><big>%s</big></b>", APP_NAME));

        subtitle_label = gtk_label_new(subtitle);
        gtk_widget_add_css_class(subtitle_label, "dim-label");
        gtk_widget_add_css_class(subtitle_label, "caption");

        title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_append(GTK_BOX(title_box), name_label);
        gtk_box_append(GTK_BOX(title_box), subtitle_label);
        gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), title_box);

        g_free(subtitle);

        dark_btn = gtk_toggle_button_new();
        {
                GtkWidget *icon = gtk_image_new_from_icon_name(
                        "dark-mode-symbolic");
                gtk_button_set_child(GTK_BUTTON(dark_btn), icon);
        }
        gtk_widget_set_tooltip_text(dark_btn, "Toggle dark style");
        g_signal_connect(dark_btn, "toggled", G_CALLBACK(on_dark_toggled),
                         NULL);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dark_btn),
                                     system_prefers_dark());
        gtk_header_bar_pack_end(GTK_HEADER_BAR(header), dark_btn);
        portal_scheme_async(dark_btn);

        return header;
}

static GtkWidget *
build_banner(void)
{
        GtkWidget *box, *icon, *label;

        box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_add_css_class(box, "banner");

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
window_new(GtkApplication *app)
{
        GtkWidget *window, *header, *root_box, *overlay, *split;
        GtkWidget *sidebar_scroll, *sidebar, *main_area, *stack;
        GtkWidget *revealer, *toast_box, *toast_label;
        WindowCtx *ctx;

        style_init();

        window = gtk_application_window_new(app);
        gtk_window_set_title(GTK_WINDOW(window), APP_NAME);
        gtk_window_set_default_size(GTK_WINDOW(window), 1080, 720);
        gtk_window_set_icon_name(GTK_WINDOW(window), APP_ID);

        ctx = g_new0(WindowCtx, 1);
        g_object_set_data_full(G_OBJECT(window), "window-ctx", ctx,
                               ctx_free);

        header = build_headerbar(ctx, window);
        gtk_window_set_titlebar(GTK_WINDOW(window), header);

        overlay = gtk_overlay_new();
        gtk_window_set_child(GTK_WINDOW(window), overlay);

        root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_overlay_set_child(GTK_OVERLAY(overlay), root_box);

        if (!is_root())
                gtk_box_append(GTK_BOX(root_box), build_banner());

        split = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_vexpand(split, TRUE);
        gtk_box_append(GTK_BOX(root_box), split);

        sidebar = sidebar_new();
        gtk_widget_add_css_class(sidebar, "sidebar");
        sidebar_scroll = gtk_scrolled_window_new();
        gtk_widget_add_css_class(sidebar_scroll, "sidebar-shell");
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
        sidebar_set_stack(sidebar, GTK_STACK(stack));
        ctx->stack = GTK_STACK(stack);

        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(main_area), stack);
        gtk_widget_set_vexpand(main_area, TRUE);
        gtk_box_append(GTK_BOX(split), main_area);

        add_page(GTK_STACK(stack), page_dashboard_new(window),
                 "dashboard", "Dashboard", "dashboard-symbolic");
        add_page(GTK_STACK(stack), page_cpu_new(window),
                 "cpu", "CPU", "cpu-symbolic");
        add_page(GTK_STACK(stack), page_gpu_new(window),
                 "gpu", "GPU", "gpu-symbolic");
        add_page(GTK_STACK(stack), page_battery_new(window),
                 "battery", "Battery", "battery-symbolic");
        add_page(GTK_STACK(stack), page_memory_new(window),
                 "memory", "Memory", "memory-symbolic");
        add_page(GTK_STACK(stack), page_about_new(window),
                 "about", "About", "info-symbolic");

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
        WindowCtx *ctx = user_data;

        gtk_revealer_set_reveal_child(GTK_REVEALER(ctx->toast_revealer),
                                      FALSE);
        ctx->toast_timeout_id = 0;
        return G_SOURCE_REMOVE;
}

void
window_show_toast(GtkWidget *window, const gchar *message)
{
        WindowCtx *ctx;

        g_return_if_fail(GTK_IS_WIDGET(window));

        ctx = g_object_get_data(G_OBJECT(window), "window-ctx");
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
