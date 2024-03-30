//#include "sigc++/functors/ptr_fun.h"
#include <iostream>
#include <gtkmm.h>
#include <format>
#include <filesystem>
#include <iostream>
#include <unordered_map>

#include "desktop.cpp"
#include "glibmm/ustring.h"
#include "gtkmm/liststore.h"
#include "sigc++/functors/ptr_fun.h"

struct ListUi {
	Gtk::ListView *listem;
	std::vector<Glib::ustring> strings;
	Glib::RefPtr<Gtk::StringList> sl;
	Glib::RefPtr<Gtk::SignalListItemFactory> lif;
};

struct EntryUi {
	deskentry::DesktopEntry de;
	Gtk::Label *filename_label, *exec_label, *url_label;
};

struct KisayolApp {
	Glib::RefPtr<Gtk::Application> app;
	Glib::RefPtr<Gtk::Builder> builder;
	Gtk::Window *main_window, *entry_window;
	Gtk::Button *ab;
	Gtk::Entry *etxt;
	std::vector<deskentry::DesktopEntry> list;
	ListUi lui;
	EntryUi eui;
};

// TODO: get rid of global variables
struct KisayolApp kapp = { 0 };

static void lui_setup(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	GtkWidget *lb = gtk_button_new ();
  	gtk_list_item_set_child (list_item->gobj(), lb);
}


void set_from_desktop_entry(EntryUi *eui, deskentry::DesktopEntry de) {

	eui->filename_label->set_label(de.pe["Desktop Entry"]["Name"].strval);
	eui->exec_label->set_label(de.pe["Desktop Entry"]["Exec"].strval);
	eui->url_label->set_label(de.pe["Desktop Entry"]["GenericName"].strval);
}

void entry_ui() {
	kapp.entry_window = kapp.builder->get_widget<Gtk::Window>("EntryWindow");
	kapp.entry_window->set_hide_on_close(true);
	kapp.entry_window->set_application(kapp.app);
	kapp.eui.filename_label = kapp.builder->get_widget<Gtk::Label>("filename_label");
	kapp.eui.exec_label = kapp.builder->get_widget<Gtk::Label>("exec_label");
	kapp.eui.url_label = kapp.builder->get_widget<Gtk::Label>("url_label");
	
	set_from_desktop_entry(&kapp.eui, kapp.eui.de);
	
	kapp.entry_window->present();
}

void button_clicked(GtkWidget *widget, gpointer data) {
	Glib::ustring strs = gtk_button_get_label(GTK_BUTTON(widget));
	int pos = 0;
	for(int i = 0; i < kapp.lui.strings.size(); i++)
		if (kapp.lui.strings[i].data() == strs)
			pos = i;
	kapp.eui.de = kapp.list.at(pos == Glib::ustring::npos ? 0 : pos);
	entry_ui();
}

static void lui_bind(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	GtkWidget *lb = gtk_list_item_get_child (list_item->gobj());
	/* Strobj is owned by the instance. Caller mustn't change or destroy it. */
	GtkStringObject *strobj = GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj()));
	/* The string returned by gtk_string_object_get_string is owned by the instance. */
	gtk_button_set_label (GTK_BUTTON(lb), gtk_string_object_get_string (strobj));
	g_signal_connect(G_OBJECT(lb), "clicked", 
      		G_CALLBACK(button_clicked), NULL);

}

static void add_button_clicked() {
	if (kapp.etxt->get_text() == "")
		return;
	kapp.lui.sl->append(kapp.etxt->get_text());
	kapp.etxt->set_text("");
}

void init_ui() {
	const Gtk::TreeModelColumnRecord columns;
	auto model = Gtk::ListStore::create(columns);

	kapp.main_window = kapp.builder->get_widget<Gtk::Window>("main_window");
	kapp.main_window->set_application(kapp.app);
	
	for (const auto & entry : std::filesystem::directory_iterator("/usr/share/applications/")){
		auto pde = deskentry::parse_file(entry.path());
		if(pde.has_value()) {
			kapp.list.push_back(pde.value());
			kapp.lui.strings.push_back(pde.value().pe["Desktop Entry"]["Name"].strval);
		}
	}
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
	
	//kapp.eui.filepath = "/usr/share/applications/btop.desktop";
	kapp.app->signal_activate().connect(sigc::ptr_fun(init_ui));

	return kapp.app->run(argc, argv);
}
