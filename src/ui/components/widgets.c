#include "widgets.h"

static const gchar *KEY_VALUE_LABEL = "value-label";
static const gchar *KEY_ROW_DROPDOWN = "dropdown";
static const gchar *KEY_ROW_IDS = "ids";
static const gchar *KEY_ROW_LIST = "list";
static const gchar *KEY_ROW_SPIN = "spin";

GtkWidget *card_new(const gchar *title)
{
	GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(card, "card");

	if (title != NULL) {
		GtkWidget *label = gtk_label_new(title);
		gtk_widget_set_halign(label, GTK_ALIGN_START);
		gtk_widget_add_css_class(label, "card-title");
		gtk_box_append(GTK_BOX(card), label);

		gtk_box_append(GTK_BOX(card),
			       gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
		g_object_set_data(G_OBJECT(card), "has-header",
				  GINT_TO_POINTER(1));
	}

	return card;
}

void card_add(GtkWidget *card, GtkWidget *child)
{
	if (g_object_get_data(G_OBJECT(card), "rows-added") != NULL)
		gtk_box_append(GTK_BOX(card),
			       gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	g_object_set_data(G_OBJECT(card), "rows-added", GINT_TO_POINTER(1));
	gtk_box_append(GTK_BOX(card), child);
}

GtkWidget *kv_row(const gchar *key)
{
	GtkWidget *row, *label, *value;

	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_add_css_class(row, "row");

	label = gtk_label_new(key);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_add_css_class(label, "dim-label");
	gtk_box_append(GTK_BOX(row), label);

	value = gtk_label_new("-");
	gtk_label_set_ellipsize(GTK_LABEL(value), PANGO_ELLIPSIZE_END);
	gtk_label_set_selectable(GTK_LABEL(value), TRUE);
	gtk_label_set_xalign(GTK_LABEL(value), 1.0f);
	gtk_widget_add_css_class(value, "value");
	gtk_box_append(GTK_BOX(row), value);

	g_object_set_data(G_OBJECT(row), KEY_VALUE_LABEL, value);
	return row;
}

void kv_set(GtkWidget *row, const gchar *fmt, ...)
{
	GtkLabel *value;
	va_list args;
	gchar *text;

	if (row == NULL)
		return;

	value = GTK_LABEL(g_object_get_data(G_OBJECT(row), KEY_VALUE_LABEL));
	if (value == NULL)
		return;

	va_start(args, fmt);
	text = g_strdup_vprintf(fmt, args);
	va_end(args);

	gtk_label_set_text(value, text);
	g_free(text);
}

GtkWidget *option_row_new(const gchar *title)
{
	GtkWidget *row, *label, *dropdown;
	GtkStringList *list;

	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_add_css_class(row, "row");

	label = gtk_label_new(title);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_box_append(GTK_BOX(row), label);

	list = gtk_string_list_new(NULL);
	dropdown = gtk_drop_down_new(G_LIST_MODEL(list), NULL);
	gtk_widget_set_valign(dropdown, GTK_ALIGN_CENTER);

	g_object_set_data(G_OBJECT(row), KEY_ROW_DROPDOWN, dropdown);
	g_object_set_data(G_OBJECT(row), KEY_ROW_LIST, list);
	g_object_set_data_full(G_OBJECT(row), KEY_ROW_IDS,
			       g_ptr_array_new_with_free_func(g_free),
			       (GDestroyNotify)g_ptr_array_unref);

	gtk_box_append(GTK_BOX(row), dropdown);
	return row;
}

void option_row_append(GtkWidget *row, const gchar *id, const gchar *label)
{
	GtkStringList *list = g_object_get_data(G_OBJECT(row), KEY_ROW_LIST);
	GPtrArray *ids = g_object_get_data(G_OBJECT(row), KEY_ROW_IDS);

	if (list == NULL || ids == NULL || id == NULL)
		return;

	gtk_string_list_append(list, label != NULL ? label : id);
	g_ptr_array_add(ids, g_strdup(id));
}

void option_row_select_id(GtkWidget *row, const gchar *id)
{
	GtkDropDown *dropdown =
		g_object_get_data(G_OBJECT(row), KEY_ROW_DROPDOWN);
	GPtrArray *ids = g_object_get_data(G_OBJECT(row), KEY_ROW_IDS);

	if (dropdown == NULL || ids == NULL || id == NULL)
		return;

	for (guint i = 0; i < ids->len; i++) {
		if (g_strcmp0(g_ptr_array_index(ids, i), id) == 0) {
			gtk_drop_down_set_selected(dropdown, i);
			return;
		}
	}
}

const gchar *option_row_active_id(GtkWidget *row)
{
	GtkDropDown *dropdown =
		g_object_get_data(G_OBJECT(row), KEY_ROW_DROPDOWN);
	GPtrArray *ids = g_object_get_data(G_OBJECT(row), KEY_ROW_IDS);
	guint selected;

	if (dropdown == NULL || ids == NULL)
		return NULL;

	selected = gtk_drop_down_get_selected(dropdown);
	if (selected == GTK_INVALID_LIST_POSITION || selected >= ids->len)
		return NULL;

	return g_ptr_array_index(ids, selected);
}

GtkDropDown *option_row_dropdown(GtkWidget *row)
{
	return GTK_DROP_DOWN(
		g_object_get_data(G_OBJECT(row), KEY_ROW_DROPDOWN));
}

GtkWidget *spin_row(const gchar *title, gdouble min, gdouble max, gdouble step,
		    gdouble value)
{
	GtkWidget *row, *label;
	GtkSpinButton *spin;

	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_add_css_class(row, "row");

	label = gtk_label_new(title);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_box_append(GTK_BOX(row), label);

	spin = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(min, max, step));
	gtk_spin_button_set_value(spin, value);
	gtk_spin_button_set_digits(spin, 0);
	gtk_spin_button_set_update_policy(spin, GTK_UPDATE_IF_VALID);
	gtk_widget_set_valign(GTK_WIDGET(spin), GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(row), GTK_WIDGET(spin));

	g_object_set_data(G_OBJECT(row), KEY_ROW_SPIN, spin);
	return row;
}

GtkSpinButton *spin_row_spin(GtkWidget *row)
{
	return GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(row), KEY_ROW_SPIN));
}

GtkWidget *page_wrap(GtkWidget *content)
{
	GtkWidget *scrolled = gtk_scrolled_window_new();

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), content);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scrolled, TRUE);
	return scrolled;
}
