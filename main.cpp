#include <cstddef>
#include <exception>
#include <iostream>
#include <gtkmm.h>
#include <format>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "desktop.cpp"
#include "glibmm/ustring.h"
#include "gtkmm/button.h"
#include "gtkmm/checkbutton.h"
#include "gtkmm/dropdown.h"
#include "gtkmm/liststore.h"
#include "gtkmm/listview.h"
#include "gtkmm/singleselection.h"
#include "gtkmm/stringlist.h"
#include "sigc++/functors/ptr_fun.h"

struct ListUi {
	Gtk::ListView *listem;
	std::vector<Glib::ustring> strings;
	Glib::RefPtr<Gtk::StringList> sl;
	Glib::RefPtr<Gtk::SignalListItemFactory> lif;
};

struct EntryUi {
	deskentry::DesktopEntry de;
	// TODO: replace categories_entry with a listview or smth.
	Gtk::Entry *filename_entry, *exec_entry, *comment_entry, *categories_entry,
		   *generic_name_entry, *path_entry;
	Gtk::CheckButton *terminal_check, *dgpu_check, *single_main_window;
	Gtk::DropDown *type_drop;
	Gtk::Image *icon;
};

struct KisayolApp {
	Glib::RefPtr<Gtk::Application> app;
	Glib::RefPtr<Gtk::Builder> builder;
	Gtk::Window *main_window, *entry_window;
	Gtk::Button *ab;
	Gtk::Entry *etxt;
	std::vector<deskentry::DesktopEntry> list;
	std::map<std::string, deskentry::CatList> catlist;
	std::vector<Gtk::ListView*> category_views;
	ListUi lui;
	EntryUi eui;
	char *XDG_ENV;
};

const std::vector<Glib::ustring> TypeLookup = {"Application", "Link", "Directory"};

// TODO: get rid of global variables
struct KisayolApp kapp = { 0 };

static void dd_setup(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	GtkWidget *lb = gtk_label_new (NULL);
  	gtk_list_item_set_child (list_item->gobj(), lb);
}

static void dd_bind(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	GtkWidget *lb = gtk_list_item_get_child (list_item->gobj());
	/* Strobj is owned by the instance. Caller mustn't change or destroy it. */
	GtkStringObject *strobj = GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj()));
	/* The string returned by gtk_string_object_get_string is owned by the instance. */
	gtk_label_set_text (GTK_LABEL(lb), gtk_string_object_get_string (strobj));
}

std::string de_val(deskentry::DesktopEntry de, std::string key) {
        return de.pe["Desktop Entry"][key].strval;
}

void set_from_desktop_entry(EntryUi *eui, deskentry::DesktopEntry de) {
	eui->filename_entry->set_text(de_val(de, "Name"));
	eui->exec_entry->set_text(de_val(de, "Exec"));
	eui->comment_entry->set_text(de.pe["Desktop Entry"]["Comment"].strval);
	eui->categories_entry->set_text(de.pe["Desktop Entry"]["Categories"].strval);
	eui->generic_name_entry->set_text(de.pe["Desktop Entry"]["GenericName"].strval);
	eui->path_entry->set_text(de.pe["Desktop Entry"]["Path"].strval);
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
	if (de.pe["Desktop Entry"]["Icon"].strval == "")
		eui->icon->set_from_icon_name("applications-other");
	else if (de.isGicon)
		eui->icon->set_from_icon_name(de.pe["Desktop Entry"]["Icon"].strval);
	else
		eui->icon->set_from_resource(de.pe["Desktop Entry"]["Icon"].strval);
}

void entry_ui() {
	kapp.eui.filename_entry = kapp.builder->get_widget<Gtk::Entry>("filename_entry");
	kapp.eui.exec_entry = kapp.builder->get_widget<Gtk::Entry>("exec_entry");
	kapp.eui.terminal_check = kapp.builder->get_widget<Gtk::CheckButton>("terminal_check");
	kapp.eui.type_drop = kapp.builder->get_widget<Gtk::DropDown>("type_drop");
	kapp.eui.icon = kapp.builder->get_widget<Gtk::Image>("app_icon");
	kapp.eui.comment_entry = kapp.builder->get_widget<Gtk::Entry>("comment_entry");
	kapp.eui.categories_entry = kapp.builder->get_widget<Gtk::Entry>("categories_entry");
	kapp.eui.generic_name_entry = kapp.builder->get_widget<Gtk::Entry>("generic_name_entry");
	kapp.eui.path_entry = kapp.builder->get_widget<Gtk::Entry>("path_entry");
	kapp.eui.single_main_window = kapp.builder->get_widget<Gtk::CheckButton>("single_main_window");
	kapp.eui.dgpu_check = kapp.builder->get_widget<Gtk::CheckButton>("dgpu_check");
	set_from_desktop_entry(&kapp.eui, kapp.eui.de);
}

void button_clicked(GtkWidget *widget, gpointer data) {
	Glib::ustring strs = gtk_button_get_label(GTK_BUTTON(widget));
	size_t pos = 0;
	for(size_t i = 0; i < kapp.lui.strings.size(); i++)
		if (kapp.lui.strings[i].data() == strs)
			pos = i;
	kapp.eui.de = kapp.list.at(pos == Glib::ustring::npos ? 0 : pos);
	entry_ui();
}

static void lui_setup(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	GtkWidget *lb = gtk_expander_new (NULL);
  	gtk_list_item_set_child (list_item->gobj(), lb);
}

static void cat_setup(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	GtkWidget *lb = gtk_label_new (NULL);
  	gtk_list_item_set_child (list_item->gobj(), lb);
}

void catlist_button_clicked(guint position) {
	printf("pos: %d\n", position);
	/*auto button = GTK_BUTTON(widget);
	auto exp = GTK_EXPANDER(gtk_widget_get_parent(gtk_widget_get_parent(widget)));
	auto catname = gtk_expander_get_label(exp);
	printf("Catname: %s\n", catname);*/
	/*Glib::ustring strs = gtk_button_get_label(GTK_BUTTON(widget));
	size_t pos = 0;
	for(size_t i = 0; i < kapp.lui.strings.size(); i++)
		if (kapp.lui.strings[i].data() == strs)
			pos = i;
	kapp.eui.de = kapp.list.at(pos == Glib::ustring::npos ? 0 : pos);
	entry_ui();*/

}

static void cat_bind(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	//GtkButton *lb = GTK_BUTTON(gtk_list_item_get_child (list_item->gobj()));
	auto lb = dynamic_cast<Gtk::Label*>(list_item->get_child());
	/* Strobj is owned by the instance. Caller mustn't change or destroy it. */
	Glib::ustring strobj = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj())));
	/* The string returned by gtk_string_object_get_string is owned by the instance. */
	
	//g_signal_connect_object(list_item->gobj(), "activate", &catlist_button_clicked, NULL, G_CONNECT_DEFAULT);
	lb->set_label(strobj);
}

static void lui_bind(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	GtkWidget *lb = gtk_list_item_get_child (list_item->gobj());
	/* Strobj is owned by the instance. Caller mustn't change or destroy it. */
	Glib::ustring strobj = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj())));
	/* The string returned by gtk_string_object_get_string is owned by the instance. */
	gtk_expander_set_label (GTK_EXPANDER(lb), strobj.c_str());
	
	const Gtk::TreeModelColumnRecord columns;
	auto model = Gtk::ListStore::create(columns);
	auto gsl = Gtk::StringList::create();
	for (size_t i = 0; i < kapp.catlist[strobj].size(); i++) {
		gsl->append(kapp.catlist[strobj][i]);
	}
	auto ns = Gtk::NoSelection::create(gsl);
	auto fac = Gtk::SignalListItemFactory::create();
	fac->signal_setup().connect(sigc::ptr_fun(cat_setup));
	fac->signal_bind().connect(sigc::ptr_fun(cat_bind));
	kapp.category_views.push_back(new Gtk::ListView());
	kapp.category_views[kapp.category_views.size()-1]->set_model(ns);
	kapp.category_views[kapp.category_views.size()-1]->set_factory(fac);
	kapp.category_views[kapp.category_views.size()-1]->set_single_click_activate(true);
	kapp.category_views[kapp.category_views.size()-1]->signal_activate().connect(sigc::ptr_fun(catlist_button_clicked));
	gtk_expander_set_child(GTK_EXPANDER(lb), GTK_WIDGET(kapp.category_views[kapp.category_views.size()-1]->gobj()));
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
		try {
			auto pde = deskentry::parse_file(entry.path(), kapp.XDG_ENV);
			if(pde.has_value()) {
				auto pe = pde.value();
				if (!pe.HiddenFilter || !pe.NoDisplayFilter || !pe.OnlyShowInFilter) {
					for (size_t i = 0; i < pe.Categories.size(); i++) {
						kapp.catlist[pe.Categories[i]].push_back(pe.pe["Desktop Entry"]["Name"].strval);
					}
					//kapp.list.push_back(pe);
					//kapp.lui.strings.push_back(pe.pe["Desktop Entry"]["Name"].strval);
				}
			}
		} catch (std::exception& e) {
			printf("!!ERROR!! %s: %s\n", entry.path().c_str(), e.what());
			return;
		}
	}
	for (auto const& [group, apps] : kapp.catlist) {
		kapp.lui.strings.push_back(group);
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
	
	kapp.app = Gtk::Application::create("org.mustafa.dizelge");
	
	kapp.builder = Gtk::Builder::create_from_file(abs_path + "kisa.ui");
	if(!kapp.builder)
		return 1;

	kapp.XDG_ENV = std::getenv("XDG_CURRENT_DESKTOP");
	if (kapp.XDG_ENV == NULL)
		kapp.XDG_ENV = const_cast<char*>("none");
	
	//kapp.eui.filepath = "/usr/share/applications/btop.desktop";
	kapp.app->signal_activate().connect(sigc::ptr_fun(init_ui));

	return kapp.app->run(argc, argv);
}
