#include "format.h"

gchar **
tokenize_ws(const gchar *text)
{
        GPtrArray *tokens;
        const gchar *p;

        if (text == NULL) {
                gchar **empty = g_new0(gchar *, 1);
                return empty;
        }

        tokens = g_ptr_array_new_with_free_func(g_free);
        p = text;
        while (*p != '\0') {
                const gchar *start;

                while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
                        p++;
                if (*p == '\0')
                        break;

                start = p;
                while (*p != '\0' && *p != ' ' && *p != '\t' &&
                       *p != '\n' && *p != '\r')
                        p++;

                g_ptr_array_add(tokens, g_strndup(start, p - start));
        }

        g_ptr_array_add(tokens, NULL);
        return (gchar **)g_ptr_array_free(tokens, FALSE);
}

gchar **
split_lines(const gchar *text, gsize *count)
{
        GPtrArray *out;
        gchar **lines;
        gsize i;

        if (count != NULL)
                *count = 0;
        if (text == NULL || text[0] == '\0')
                return g_new0(gchar *, 1);

        out = g_ptr_array_new_with_free_func(g_free);
        lines = g_strsplit(text, "\n", -1);
        for (i = 0; lines[i] != NULL; i++) {
                gchar *stripped = g_strstrip(lines[i]);
                if (stripped[0] != '\0')
                        g_ptr_array_add(out, stripped);
                else
                        g_free(stripped);
        }
        g_free(lines);

        g_ptr_array_add(out, NULL);
        if (count != NULL)
                *count = out->len - 1;
        return (gchar **)g_ptr_array_free(out, FALSE);
}

gchar *
format_hz(gint64 hz)
{
        if (hz >= 1000000000LL && hz % 1000000000LL == 0)
                return g_strdup_printf("%.0f GHz", hz / 1000000000.0);
        if (hz >= 1000000LL)
                return g_strdup_printf(hz % 1000000LL == 0 ? "%.0f MHz" : "%.2f MHz",
                                       hz / 1000000.0);
        if (hz >= 1000LL)
                return g_strdup_printf("%.0f kHz", hz / 1000.0);
        return g_strdup_printf("%" G_GINT64_FORMAT " Hz", hz);
}

gchar *
format_khz(gint64 khz)
{
        return format_hz(khz * 1000);
}

gchar *
format_bytes(guint64 bytes)
{
        gdouble value = (gdouble)bytes;
        const gchar *units[] = { "B", "KB", "MB", "GB", "TB" };
        gsize i = 0;

        while (value >= 1000.0 && i < G_N_ELEMENTS(units) - 1) {
                value /= 1024.0;
                i++;
        }

        if (i == 0)
                return g_strdup_printf("%.0f %s", value, units[i]);

        if (value >= 100.0)
                return g_strdup_printf("%.0f %s", value, units[i]);
        if (value >= 10.0)
                return g_strdup_printf("%.1f %s", value, units[i]);
        return g_strdup_printf("%.2f %s", value, units[i]);
}

gchar *
format_uptime(guint64 seconds)
{
        guint64 days = seconds / 86400;
        guint64 hours = (seconds % 86400) / 3600;
        guint64 minutes = (seconds % 3600) / 60;
        guint64 secs = seconds % 60;
        GString *s = g_string_new(NULL);

        if (days > 0)
                g_string_append_printf(s, "%" G_GUINT64_FORMAT "d ", days);
        if (hours > 0 || days > 0)
                g_string_append_printf(s, "%" G_GUINT64_FORMAT "h ", hours);
        if (minutes > 0 || hours > 0 || days > 0)
                g_string_append_printf(s, "%" G_GUINT64_FORMAT "m ", minutes);

        g_string_append_printf(s, "%" G_GUINT64_FORMAT "s", secs);
        return g_string_free(s, FALSE);
}

gboolean
str_has_prefix_any(const gchar         *text,
                      const gchar *const  *prefixes,
                      gsize                n_prefixes)
{
        gsize i;

        for (i = 0; i < n_prefixes; i++) {
                if (g_str_has_prefix(text, prefixes[i]))
                        return TRUE;
        }
        return FALSE;
}

gint
cmp_int64(gconstpointer a, gconstpointer b)
{
        gint64 va = *(const gint64 *)a;
        gint64 vb = *(const gint64 *)b;

        return (va > vb) - (va < vb);
}
