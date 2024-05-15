#pragma once
#include <gtkmm.h>
#include "desktop.cpp"
#include "Common.cpp"

/*
 * This file contains only stuff in right panel (where the entry information is displayed).
 */

typedef struct {
	deskentry::DesktopEntry de;
	// TODO: replace categories_entry with a listview or smth.
	Gtk::Entry *filename_entry, *exec_entry, *comment_entry, *categories_entry,
		   *generic_name_entry, *path_entry;
	Gtk::CheckButton *terminal_check, *dgpu_check, *single_main_window;
	Gtk::DropDown *type_drop;
	Gtk::Image *icon;
	Gtk::Button *open_file_button, *save_button;
} EntryUi;

const std::vector<Glib::ustring> TypeLookup = {"Application", "Link", "Directory"};

static void dd_setup(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	GtkWidget *lb = gtk_label_new (NULL);
	gtk_widget_set_halign(lb, GTK_ALIGN_START);
  	gtk_list_item_set_child (list_item->gobj(), lb);
}

static void dd_bind(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	GtkWidget *lb = gtk_list_item_get_child (list_item->gobj());
	GtkStringObject *strobj = GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj()));
	gtk_label_set_text (GTK_LABEL(lb), gtk_string_object_get_string (strobj));
}

void open_file_clicked(std::string *path) {
    if (!path->empty())
		system(("xdg-open "+*path).c_str());
}

void sync_pe(EntryUi *eui, deskentry::DesktopEntry *de) {
	de->pe["Desktop Entry"]["Name"] 			= eui->filename_entry->get_text();
	de->pe["Desktop Entry"]["Exec"] 			= eui->exec_entry->get_text();
	de->pe["Desktop Entry"]["Comment"] 		= eui->comment_entry->get_text();
	de->pe["Desktop Entry"]["Categories"] 		= eui->categories_entry->get_text();
	de->pe["Desktop Entry"]["GenericName"] 		= eui->generic_name_entry->get_text();
	de->pe["Desktop Entry"]["Path"] 			= eui->path_entry->get_text();
	de->pe["Desktop Entry"]["Terminal"] 		= eui->terminal_check->get_active() ? "true" : "false";
	de->pe["Desktop Entry"]["PrefersNonDefaultGPU"] 	= eui->dgpu_check->get_active() ? "true" : "false";
	de->pe["Desktop Entry"]["SingleMainWindow"] 	= eui->single_main_window->get_active() ? "true" : "false";
}

void save_button_clicked(EntryUi *eui) {
	sync_pe(eui, &eui->de);
	auto de = eui->de;
	deskentry::write_to_file(de.pe, de.path);
}

void set_from_desktop_entry(EntryUi *eui, deskentry::DesktopEntry de) {
	eui->filename_entry->set_text(de_val(de, "Name"));
	eui->exec_entry->set_text(de_val(de, "Exec"));
	eui->comment_entry->set_text(de_val(de, "Comment"));
	eui->categories_entry->set_text(de_val(de, "Categories"));
	eui->generic_name_entry->set_text(de_val(de, "GenericName"));
	eui->path_entry->set_text(de_val(de, "Path"));
	eui->terminal_check->set_active(de_val(de, "Terminal") == "true");
	eui->dgpu_check->set_active(de_val(de, "PrefersNonDefaultGPU") == "true");
	eui->single_main_window->set_active(de_val(de, "SingleMainWindow") == "true");

	// set type dropdown
	auto ddsl = Gtk::StringList::create(TypeLookup);
	auto ddss = Gtk::SingleSelection::create(ddsl);
	auto factory = Gtk::SignalListItemFactory::create();
	factory->signal_setup().connect(sigc::ptr_fun(dd_setup));
	factory->signal_bind().connect(sigc::ptr_fun(dd_bind));
	
	eui->type_drop->set_model(ddss);
	eui->type_drop->set_factory(factory);
	eui->type_drop->set_selected(de.type);

	// Set icon
	set_icon(eui->icon, de_val(de, "Icon"));
}


void entry_ui(EntryUi *eui, Glib::RefPtr<Gtk::Builder> builder) {
	eui->terminal_check 	= builder->get_widget<Gtk::CheckButton>("terminal_check");
	eui->single_main_window = builder->get_widget<Gtk::CheckButton>("single_main_window");
	eui->dgpu_check 	= builder->get_widget<Gtk::CheckButton>("dgpu_check");
	eui->type_drop 		= builder->get_widget<Gtk::DropDown>("type_drop");
	eui->open_file_button 	= builder->get_widget<Gtk::Button>("open_file_button");
	eui->save_button 	= builder->get_widget<Gtk::Button>("save_button");
	eui->filename_entry 	= builder->get_widget<Gtk::Entry>("filename_entry");
	eui->exec_entry 	= builder->get_widget<Gtk::Entry>("exec_entry");
	eui->comment_entry 	= builder->get_widget<Gtk::Entry>("comment_entry");
	eui->categories_entry 	= builder->get_widget<Gtk::Entry>("categories_entry");
	eui->generic_name_entry = builder->get_widget<Gtk::Entry>("generic_name_entry");
	eui->path_entry 	= builder->get_widget<Gtk::Entry>("path_entry");
	eui->icon 		= builder->get_widget<Gtk::Image>("app_icon");

	eui->save_button->signal_clicked().connect(std::bind(save_button_clicked, eui));
	eui->open_file_button->signal_clicked().connect(sigc::bind(&open_file_clicked, &eui->de.path));
}
