#include "gpu.h"

#include "../util/format.h"
#include "../util/sysfs.h"

#include <stdlib.h>
#include <string.h>

static const char *const DRM_BASE    = "/sys/class/drm";
static const char *const DEVFREQ_BASE = "/sys/class/devfreq";

static const struct {
        const gchar *id;
        const gchar *name;
} VENDOR_MAP[] = {
        { "0x8086", "Intel" },
        { "0x1002", "AMD" },
        { "0x1022", "AMD" },
        { "0x10de", "NVIDIA" },
        { "0x13b5", "ARM (Mali)" },
        { "0x5143", "Qualcomm (Adreno)" },
};

static gchar **
read_tokens_from_file(const gchar *path)
{
        gchar *text;
        gchar **tokens;

        text = read_trimmed(path);
        tokens = text != NULL ? tokenize_ws(text) : g_new0(gchar *, 1);
        g_free(text);
        return tokens;
}

Devfreq *
devfreq_new(const gchar *path, const gchar *name)
{
        Devfreq *d = g_new0(Devfreq, 1);

        d->devfreq_path = g_strdup(path);
        d->name = g_strdup(name);
        devfreq_refresh(d);
        return d;
}

void
devfreq_refresh(Devfreq *d)
{
        gchar **tokens;
        gchar *tmp;

        if (d == NULL)
                return;

        g_free(d->governor);
        g_free(d->cur_freq_hz);
        g_free(d->min_freq_hz);
        g_free(d->max_freq_hz);
        g_strfreev(d->governors);
        d->governors = NULL;

        tmp = g_build_filename(d->devfreq_path, "governor", NULL);
        d->governor = read_first_line(tmp);
        g_free(tmp);

        tmp = g_build_filename(d->devfreq_path, "cur_freq", NULL);
        d->cur_freq_hz = read_first_line(tmp);
        g_free(tmp);

        tmp = g_build_filename(d->devfreq_path, "min_freq", NULL);
        d->min_freq_hz = read_first_line(tmp);
        g_free(tmp);

        tmp = g_build_filename(d->devfreq_path, "max_freq", NULL);
        d->max_freq_hz = read_first_line(tmp);
        g_free(tmp);

        tmp = g_build_filename(d->devfreq_path, "available_governors", NULL);
        tokens = read_tokens_from_file(tmp);
        g_free(tmp);

        if (tokens[0] == NULL) {
                g_strfreev(tokens);
                tokens = g_new0(gchar *, 2);
                tokens[0] = g_strdup(d->governor != NULL ?
                                     d->governor : "");
        }
        d->governors = tokens;
}

Devfreq **
devfreq_list(gsize *count)
{
        GPtrArray *result;
        gsize n_entries = 0;
        gchar **entries;

        result = g_ptr_array_new();
        entries = list_dir(DEVFREQ_BASE, &n_entries);
        for (gsize i = 0; i < n_entries; i++) {
                gchar *path = g_build_filename(DEVFREQ_BASE, entries[i], NULL);
                g_ptr_array_add(result,
                                devfreq_new(path, entries[i]));
                g_free(path);
        }
        g_strfreev(entries);

        if (count != NULL)
                *count = result->len;
        g_ptr_array_add(result, NULL);
        return (Devfreq **)g_ptr_array_free(result, FALSE);
}

void
devfreq_free(Devfreq *d)
{
        if (d == NULL)
                return;
        g_free(d->devfreq_path);
        g_free(d->name);
        g_free(d->governor);
        g_strfreev(d->governors);
        g_free(d->cur_freq_hz);
        g_free(d->min_freq_hz);
        g_free(d->max_freq_hz);
        g_free(d);
}

gboolean
devfreq_set_governor(Devfreq *d, const gchar *governor, GError **error)
{
        gchar *path = g_build_filename(d->devfreq_path, "governor", NULL);
        gboolean ok = write_string(path, governor, error);
        g_free(path);
        return ok;
}

gboolean
devfreq_set_min_freq(Devfreq *d, gint64 hz, GError **error)
{
        gchar *path = g_build_filename(d->devfreq_path, "min_freq", NULL);
        gboolean ok = write_int64(path, hz, error);
        g_free(path);
        return ok;
}

gboolean
devfreq_set_max_freq(Devfreq *d, gint64 hz, GError **error)
{
        gchar *path = g_build_filename(d->devfreq_path, "max_freq", NULL);
        gboolean ok = write_int64(path, hz, error);
        g_free(path);
        return ok;
}

static void
card_parse_uevent(GpuCard *card)
{
        gchar *uevent;
        gchar **lines;
        gsize n_lines = 0;

        uevent = g_build_filename(card->card_path, "device", "uevent", NULL);
        lines = split_lines(read_trimmed(uevent), &n_lines);
        g_free(uevent);

        for (gsize i = 0; lines[i] != NULL; i++) {
                if (g_str_has_prefix(lines[i], "DRIVER=")) {
                        g_free(card->driver);
                        card->driver = g_strdup(lines[i] + strlen("DRIVER="));
                } else if (g_str_has_prefix(lines[i], "PCI_ID=")) {
                        g_free(card->pci_id);
                        card->pci_id = g_strdup(lines[i] + strlen("PCI_ID="));
                }
        }
        g_strfreev(lines);

        if (card->pci_id != NULL) {
                for (gsize i = 0; i < G_N_ELEMENTS(VENDOR_MAP); i++) {
                        const gchar *vid = VENDOR_MAP[i].id + 2;
                        if (g_ascii_strncasecmp(card->pci_id, vid,
                                                strlen(vid)) == 0 &&
                            card->pci_id[strlen(vid)] == ':') {
                                card->vendor_name = g_strdup(VENDOR_MAP[i].name);
                                break;
                        }
                    }
        }

        if (card->vendor_name == NULL)
                card->vendor_name = g_strdup("Unknown");
}

static void
card_read_extras(GpuCard *card)
{
        gchar *tmp;

        g_free(card->busy_percent);
        g_free(card->cur_clock_note);
        card->busy_percent = NULL;
        card->cur_clock_note = NULL;

        tmp = g_build_filename(card->card_path, "device",
                               "gpu_busy_percent", NULL);
        if (path_exists(tmp)) {
                card->has_busy_percent = TRUE;
                card->busy_percent = read_first_line(tmp);
        }
        g_free(tmp);

        tmp = g_build_filename(card->card_path, "device",
                               "gt_cur_freq_mhz", NULL);
        card->cur_clock_note = read_first_line(tmp);
        if (card->cur_clock_note != NULL) {
                gchar *mhz = g_strdup_printf("%s MHz",
                                             card->cur_clock_note);
                g_free(card->cur_clock_note);
                card->cur_clock_note = mhz;
                g_free(tmp);
                return;
        }
        g_free(tmp);

        tmp = g_build_filename(card->card_path, "device", "pp_dpm_sclk",
                               NULL);
        {
                gchar *levels = read_trimmed(tmp);
                if (levels != NULL) {
                        gchar **lines = split_lines(levels, NULL);
                        for (gsize i = 0; lines[i] != NULL; i++) {
                                if (strchr(lines[i], '*') == NULL)
                                        continue;
                                gchar *colon = strchr(lines[i], ':');
                                if (colon != NULL) {
                                        gchar *clean =
                                                g_strdup(colon + 1);
                                        gchar *star;

                                        for (gchar *q = clean; *q; q++)
                                                if (*q == '*')
                                                        *q = ' ';
                                        star = g_strstrip(clean);
                                        card->cur_clock_note =
                                                g_strdup(star);
                                        g_free(clean);
                                }
                                break;
                        }
                        g_strfreev(lines);
                }
                g_free(levels);
        }
        g_free(tmp);
}

GpuCard *
gpu_card_new(const gchar *card_path, const gchar *card_name)
{
        GpuCard *card = g_new0(GpuCard, 1);
        gchar *link_path;
        gchar *df_path;

        card->card_path = g_strdup(card_path);
        card->card_name = g_strdup(card_name);

        link_path = g_build_filename(card_path, "device", "devfreq", NULL);
        df_path = realpath(link_path, NULL);
        if (df_path != NULL) {
                card->devfreq = devfreq_new(df_path, card_name);
                free(df_path);
        } else if (is_dir(link_path)) {
                card->devfreq = devfreq_new(link_path, card_name);
        }
        g_free(link_path);

        card_parse_uevent(card);
        card_read_extras(card);

        return card;
}

void
gpu_card_refresh(GpuCard *card)
{
        if (card == NULL)
                return;
        if (card->devfreq != NULL)
                devfreq_refresh(card->devfreq);
        card_read_extras(card);
}

void
gpu_card_free(GpuCard *card)
{
        if (card == NULL)
                return;
        devfreq_free(card->devfreq);
        g_free(card->card_path);
        g_free(card->card_name);
        g_free(card->driver);
        g_free(card->vendor_name);
        g_free(card->pci_id);
        g_free(card->busy_percent);
        g_free(card->cur_clock_note);
        g_free(card);
}

void
gpu_cards_free(GpuCard **cards, gsize count)
{
        if (cards == NULL)
                return;
        for (gsize i = 0; i < count; i++)
                gpu_card_free(cards[i]);
        g_free(cards);
}

GpuCard **
gpu_cards(gsize *count)
{
        GPtrArray *result;
        gsize n_entries = 0;
        gchar **entries;

        result = g_ptr_array_new();
        entries = list_dir(DRM_BASE, &n_entries);
        for (gsize i = 0; i < n_entries; i++) {
                gchar *path;
                GpuCard *card;

                if (!g_str_has_prefix(entries[i], "card"))
                        continue;
                path = g_build_filename(DRM_BASE, entries[i], NULL);
                card = gpu_card_new(path, entries[i]);
                if (card->driver == NULL && card->devfreq == NULL) {
                        gpu_card_free(card);
                } else {
                        g_ptr_array_add(result, card);
                }
                g_free(path);
        }
        g_strfreev(entries);

        if (count != NULL)
                *count = result->len;
        g_ptr_array_add(result, NULL);
        return (GpuCard **)g_ptr_array_free(result, FALSE);
}
