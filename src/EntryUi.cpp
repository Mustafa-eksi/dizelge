#pragma once
#include <gtkmm.h>
#include "desktop.cpp"
#include "Common.cpp"
#include "gtkmm/filechooserdialog.h"

/*
 * This file contains only stuff in right panel (where the entry information is displayed).
 */

typedef struct {
	deskentry::DesktopEntry *de;
	// TODO: replace categories_entry with a listview or smth.
	Gtk::Entry *filename_entry, *exec_entry, *comment_entry, *categories_entry,
		   *generic_name_entry, *path_entry, *iconname_entry;
	Gtk::CheckButton *terminal_check, *dgpu_check, *single_main_window;
	Gtk::DropDown *type_drop;
	Gtk::Image *icon;
	Gtk::Button *open_file_button, *save_button, *choose_image;
	GtkWidget *fd, *save_button_dialog;
	GtkWindow *main_window;
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

void open_file_clicked(EntryUi *eui) {
    if (eui != nullptr && !eui->de->path.empty())
		system(("xdg-open "+eui->de->path).c_str());
}

void sync_pe(EntryUi *eui, deskentry::DesktopEntry *de) {
	de->pe["Desktop Entry"]["Name"] 			= eui->filename_entry->get_text();
	de->pe["Desktop Entry"]["Icon"] 			= eui->iconname_entry->get_text();
	de->pe["Desktop Entry"]["Exec"] 			= eui->exec_entry->get_text();
	de->pe["Desktop Entry"]["Comment"] 		= eui->comment_entry->get_text();
	de->pe["Desktop Entry"]["Categories"] 		= eui->categories_entry->get_text();
	de->pe["Desktop Entry"]["GenericName"] 		= eui->generic_name_entry->get_text();
	de->pe["Desktop Entry"]["Path"] 			= eui->path_entry->get_text();
	de->pe["Desktop Entry"]["Terminal"] 		= eui->terminal_check->get_active() ? "true" : "false";
	de->pe["Desktop Entry"]["PrefersNonDefaultGPU"] 	= eui->dgpu_check->get_active() ? "true" : "false";
	de->pe["Desktop Entry"]["SingleMainWindow"] 	= eui->single_main_window->get_active() ? "true" : "false";
	de->pe["Desktop Entry"]["Type"] = TypeLookup[eui->type_drop->get_selected()];
}

void save_button_callback(GtkDialog* self, gint response_id, gpointer user_data) {
	if (response_id == -4) return; // close
	auto eui = (EntryUi*) user_data;
	auto file = g_file_get_path(gtk_file_chooser_get_file(GTK_FILE_CHOOSER(eui->save_button_dialog)));
	eui->de->path = file;
	deskentry::write_to_file(eui->de->pe, eui->de->path, true);
	gtk_window_close(GTK_WINDOW(self));
}

void save_button_clicked(EntryUi *eui) {
	sync_pe(eui, eui->de);
	auto de = eui->de;
	if (de->path.empty()) {
		eui->save_button_dialog = gtk_file_chooser_dialog_new("Save desktop entry", eui->main_window, GTK_FILE_CHOOSER_ACTION_SAVE, "Save", "", (char*)NULL);
		g_signal_connect(eui->save_button_dialog, "response", G_CALLBACK(save_button_callback), eui);
		gtk_window_present(GTK_WINDOW(eui->save_button_dialog));

		// eui->save_button_dialog.present(); //(sigc::bind(&save_button_callback, eui));
		return;
	}
	deskentry::write_to_file(de->pe, de->path);
}

void set_from_desktop_entry(EntryUi *eui, deskentry::DesktopEntry *de) {
	eui->de = de;

	eui->filename_entry->set_text(de_val(de, "Name"));
	eui->exec_entry->set_text(de_val(de, "Exec"));
	eui->comment_entry->set_text(de_val(de, "Comment"));
	eui->categories_entry->set_text(de_val(de, "Categories"));
	eui->generic_name_entry->set_text(de_val(de, "GenericName"));
	eui->path_entry->set_text(de_val(de, "Path"));
	eui->terminal_check->set_active(de_val(de, "Terminal") == "true");
	eui->dgpu_check->set_active(de_val(de, "PrefersNonDefaultGPU") == "true");
	eui->single_main_window->set_active(de_val(de, "SingleMainWindow") == "true");
	eui->iconname_entry->set_text(de_val(de, "Icon"));

	// set type dropdown
	auto ddsl = Gtk::StringList::create(TypeLookup);
	auto ddss = Gtk::SingleSelection::create(ddsl);
	auto factory = Gtk::SignalListItemFactory::create();
	factory->signal_setup().connect(sigc::ptr_fun(dd_setup));
	factory->signal_bind().connect(sigc::ptr_fun(dd_bind));
	
	eui->type_drop->set_model(ddss);
	eui->type_drop->set_factory(factory);
	eui->type_drop->set_selected(de->type);

	// Set icon
	set_icon(eui->icon, de_val(de, "Icon"));
}

void choose_image_callback(GtkDialog* self, gint response_id, gpointer user_data) {
	if (response_id == -4) return;
	auto eui = (EntryUi*) user_data;
	auto file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(eui->fd));
	auto filepath = g_file_get_path(file);
	printf("filepath: %s\n", filepath);
	eui->de->pe["Desktop Entry"]["Icon"] = filepath;
	set_icon(eui->icon, filepath);
	eui->iconname_entry->set_text(de_val(eui->de, "Icon"));
	gtk_window_close(GTK_WINDOW(self));
}

void choose_image_clicked(EntryUi *eui) {
	eui->fd = gtk_file_chooser_dialog_new("Select image file", eui->main_window, GTK_FILE_CHOOSER_ACTION_OPEN, "Select", "", (char*)NULL);
	g_signal_connect(eui->fd, "response", G_CALLBACK(choose_image_callback), eui);
	gtk_window_present(GTK_WINDOW(eui->fd));
	//eui->fd.present(); // (sigc::bind(&async_callback, eui), nullptr);
	printf("hello world\n");
}

void entry_ui(EntryUi *eui, Glib::RefPtr<Gtk::Builder> builder, GtkWindow *window) {
	eui->main_window = window;
	eui->terminal_check 	= builder->get_widget<Gtk::CheckButton>("terminal_check");
	eui->single_main_window = builder->get_widget<Gtk::CheckButton>("single_main_window");
	eui->dgpu_check 		= builder->get_widget<Gtk::CheckButton>("dgpu_check");
	eui->type_drop 			= builder->get_widget<Gtk::DropDown>("type_drop");
	eui->open_file_button 	= builder->get_widget<Gtk::Button>("open_file_button");
	eui->save_button 		= builder->get_widget<Gtk::Button>("save_button");
	eui->choose_image 		= builder->get_widget<Gtk::Button>("select_file_for_icon");
	eui->filename_entry 	= builder->get_widget<Gtk::Entry>("filename_entry");
	eui->exec_entry 		= builder->get_widget<Gtk::Entry>("exec_entry");
	eui->comment_entry 		= builder->get_widget<Gtk::Entry>("comment_entry");
	eui->categories_entry 	= builder->get_widget<Gtk::Entry>("categories_entry");
	eui->generic_name_entry = builder->get_widget<Gtk::Entry>("generic_name_entry");
	eui->path_entry 		= builder->get_widget<Gtk::Entry>("path_entry");
	eui->iconname_entry 	= builder->get_widget<Gtk::Entry>("iconname_entry");
	eui->icon 				= builder->get_widget<Gtk::Image>("app_icon");

	auto filter = Gtk::FileFilter::create();
	filter->add_pattern("*.png *.svg *.jpg *.jpeg *.gif");
	filter->set_name("Image Formats");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(eui->fd), filter->gobj());

	eui->save_button->signal_clicked().connect(std::bind(save_button_clicked, eui));
	eui->open_file_button->signal_clicked().connect(sigc::bind(&open_file_clicked, eui));
	eui->choose_image->signal_clicked().connect(sigc::bind(&choose_image_clicked, eui));
}
