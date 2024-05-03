#pragma once
#include <gtkmm.h>
#include "desktop.cpp"
#include "Common.cpp"

#define DEFAULT_APP_ATTR "0 -1 font \"Sans 14\""

/*
 * This file contains only signals of the application list ListView.
 */

typedef struct {
	Gtk::ListView *listem;
	std::vector<Glib::ustring> strings;
	Glib::RefPtr<Gtk::StringList> sl;
	Glib::RefPtr<Gtk::SignalListItemFactory> lif;
	Glib::RefPtr<Gtk::NoSelection> ns;
} ListUi;

static void cat_setup(const Glib::RefPtr<Gtk::ListItem>& list_item) {
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

static void cat_teardown(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	auto box = dynamic_cast<Gtk::Box*>(list_item->get_child());
	list_item->unset_child();
	delete box;
}

// This is called for each of the items in each of the expanders.
static void cat_bind(const Glib::RefPtr<Gtk::ListItem>& list_item, std::string cat_name,
                     EntryList *list, CategoryEntries catlist,
                     void (*catlist_button_clicked)(const Glib::RefPtr<Gtk::ListItem>& list_item, std::string list_ind, std::string appname)) {
	// Get the name of the item
	std::string strobj = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj())));
	// Get the widget created in cat_setup.
	auto box = dynamic_cast<Gtk::Box*>(list_item->get_child());
	auto lb = dynamic_cast<Gtk::Label*>(box->get_children()[1]);
	auto icon = dynamic_cast<Gtk::Image*>(box->get_children()[0]);
	lb->set_label(strobj);
	
	// Set the icon
	set_icon(icon, de_val((*list)[catlist[cat_name][strobj]], "Icon"));
	icon->set_pixel_size(24);

	auto sp_fn = std::bind(catlist_button_clicked, list_item, cat_name, strobj);
	list_item->connect_property_changed("selected", sp_fn);
}

static void cat_unbind(const Glib::RefPtr<Gtk::ListItem>& list_item, std::string cat_name) {
	// Get the name of the item
	Glib::ustring strobj = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj())));
	// Get the widget created in cat_setup.
	auto box = dynamic_cast<Gtk::Box*>(list_item->get_child());
	auto lb = dynamic_cast<Gtk::Label*>(box->get_children()[1]);
	lb->set_label("");
}