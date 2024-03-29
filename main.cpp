#include "gtkmm/button.h"
#include "gtkmm/window.h"
#include "sigc++/functors/ptr_fun.h"
#include <iostream>
#include <gtkmm.h>
#include <format>

#include "desktop.cpp"

struct ListUi {
	Gtk::ListView *listem;
	std::vector<Glib::ustring> strings;
	Glib::RefPtr<Gtk::StringList> sl;
	Glib::RefPtr<Gtk::SignalListItemFactory> lif;
};

struct EntryUi {
	std::string filepath;
	Gtk::Label *filename_label, *exec_label;
};

struct KisayolApp {
	Glib::RefPtr<Gtk::Application> app;
	Glib::RefPtr<Gtk::Builder> builder;
	Gtk::Window *main_window, *entry_window;
	Gtk::Button *ab;
	Gtk::Entry *etxt;
	ListUi lui;
	EntryUi eui;
};

// TODO: get rid of global variables
struct KisayolApp kapp = { 0 };

static void lui_setup(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	GtkWidget *lb = gtk_label_new (NULL);
  	gtk_list_item_set_child (list_item->gobj(), lb);
}

static void lui_bind(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	GtkWidget *lb = gtk_list_item_get_child (list_item->gobj());
	/* Strobj is owned by the instance. Caller mustn't change or destroy it. */
	GtkStringObject *strobj = GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj()));
	/* The string returned by gtk_string_object_get_string is owned by the instance. */
	gtk_label_set_text (GTK_LABEL (lb), gtk_string_object_get_string (strobj));
}

static void add_button_clicked() {
	if (kapp.etxt->get_text() == "")
		return;
	kapp.lui.sl->append(kapp.etxt->get_text());
	kapp.etxt->set_text("");
}

void set_from_desktop_entry(EntryUi *eui, deskentry::DesktopEntry de) {
	// TODO
}

void entry_ui() {
	kapp.entry_window = kapp.builder->get_widget<Gtk::Window>("EntryWindow");
	kapp.entry_window->set_application(kapp.app);
	kapp.eui.filename_label = kapp.builder->get_widget<Gtk::Label>("filename_label");
	kapp.eui.exec_label = kapp.builder->get_widget<Gtk::Label>("exec_label");
	
	if (auto de = deskentry::parse_file(kapp.eui.filepath)) {
		set_from_desktop_entry(&kapp.eui, de.value());
	}

	kapp.entry_window->present();
}

void init_ui() {
	const Gtk::TreeModelColumnRecord columns;
	auto model = Gtk::ListStore::create(columns);

	kapp.main_window = kapp.builder->get_widget<Gtk::Window>("main_window");
	kapp.main_window->set_application(kapp.app);
	
	kapp.lui.strings.push_back("abc");
	kapp.lui.sl = Gtk::StringList::create(kapp.lui.strings); // gtk_string_list_new ((const char * const *) array);
	auto ns = Gtk::NoSelection::create(kapp.lui.sl); // gtk_no_selection_new (G_LIST_MODEL (sl));

	kapp.lui.lif = Gtk::SignalListItemFactory::create();
	kapp.lui.lif->signal_setup().connect(sigc::ptr_fun(lui_setup));
	kapp.lui.lif->signal_bind().connect(sigc::ptr_fun(lui_bind));

	kapp.lui.listem = kapp.builder->get_widget<Gtk::ListView>("listem");
	if (kapp.lui.listem) {
		kapp.lui.listem->set_model(ns);
		kapp.lui.listem->set_factory(kapp.lui.lif);
	}

	kapp.etxt = kapp.builder->get_widget<Gtk::Entry>("etxt");

	kapp.ab = kapp.builder->get_widget<Gtk::Button>("add_button");
	if (kapp.ab)
		kapp.ab->signal_clicked().connect(sigc::ptr_fun(add_button_clicked));

	kapp.main_window->present();
}

int main(int argc, char* argv[])
{
	// Quick hack for accessing .glade file from anywhere
	std::string first_arg = argv[0];
	std::string abs_path = first_arg.substr(0, first_arg.length()-4);
	
	kapp.app = Gtk::Application::create("org.mustafa.kisayolduzenleyici");
	
	kapp.builder = Gtk::Builder::create_from_file(abs_path + "kisa.ui");
	if(!kapp.builder)
		return 1;
	
	kapp.eui.filepath = "/home/mustafa/Desktop/xfce4-about.desktop";
	kapp.app->signal_activate().connect(sigc::ptr_fun(entry_ui));

	return kapp.app->run(argc, argv);
}
