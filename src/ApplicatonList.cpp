#pragma once
#include <gtkmm.h>
#include "desktop.cpp"
#include "Common.cpp"

#define DEFAULT_APP_ATTR "0 -1 font \"Sans 14\""

/*
 * This file contains only signals of the application list ListView.
 */

void applist_setup(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	auto box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_margin_start(box, 16);
	auto ic = gtk_image_new();
	
	GtkWidget *lb = gtk_label_new (NULL);
	gtk_widget_set_halign(lb, GTK_ALIGN_START);
	PangoAttrList *attrs = pango_attr_list_from_string(DEFAULT_APP_ATTR);
	gtk_label_set_attributes(GTK_LABEL(lb), attrs);
	gtk_box_append(GTK_BOX(box), ic);
	gtk_box_append(GTK_BOX(box), lb);
  	gtk_list_item_set_child (list_item->gobj(), box);
}

void applist_teardown(const Glib::RefPtr<Gtk::ListItem>& list_item) {
}

// This is called for each of the items in each of the expanders.
void applist_bind(const Glib::RefPtr<Gtk::ListItem>& list_item, EntryList *list, ListMap *catlist, bool is_mapped, Gtk::ListView *liste) {
	// Get the name of the item

	std::string strobj = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj())));
	// Get the widget created in cat_setup.
	auto box = gtk_list_item_get_child(list_item->gobj());
	if (!box)
		return;
	auto lb = gtk_widget_get_last_child(box);
	auto icon = gtk_widget_get_first_child(box);
	gtk_label_set_text(GTK_LABEL(lb), strobj.c_str());
	// Set the icon
	size_t ind = is_mapped ? (*catlist)[strobj] : list_item->get_position();
	set_icon(GTK_IMAGE(icon), de_val(&(*list)[ind], "Icon"));
	gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
}

void applist_unbind(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	// Get the name of the item
	Glib::ustring strobj = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj())));
	// Get the widget created in cat_setup.
	auto box = gtk_list_item_get_child(list_item->gobj());
	if (!box)
		return;
	//lb->set_label("");
	GtkWidget *ic = gtk_widget_get_first_child(box);
	gtk_image_clear(GTK_IMAGE(ic));
}
