#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <gtkmm.h>
#include <format>
#include <filesystem>
#include <iostream>
#include <tuple>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <functional>
#include <stdlib.h>

#include "desktop.cpp"
#define DEFAULT_APP_ATTR "0 -1 font \"Sans 14\""

struct ListUi {
	Gtk::ListView *listem;
	std::vector<Glib::ustring> strings;
	Glib::RefPtr<Gtk::StringList> sl;
	Glib::RefPtr<Gtk::SignalListItemFactory> lif;
	Glib::RefPtr<Gtk::NoSelection> ns;
};

struct EntryUi {
	deskentry::DesktopEntry de;
	// TODO: replace categories_entry with a listview or smth.
	Gtk::Entry *filename_entry, *exec_entry, *comment_entry, *categories_entry,
		   *generic_name_entry, *path_entry;
	Gtk::CheckButton *terminal_check, *dgpu_check, *single_main_window;
	Gtk::DropDown *type_drop;
	Gtk::Image *icon;
	Gtk::Button *open_file_button, *save_button;
};

typedef std::map<std::string, std::map<std::string, size_t>> CategoryEntries;

struct KisayolApp {
	Glib::RefPtr<Gtk::Application> app;
	Glib::RefPtr<Gtk::Builder> builder;
	Gtk::Window *main_window, *entry_window;
	Gtk::Button *ab;
	Gtk::Entry *etxt;
	std::vector<deskentry::DesktopEntry> list;
	CategoryEntries catlist;
	std::map<std::string, Gtk::ListView*> category_views;
	std::map<std::string, Glib::RefPtr<Gtk::SingleSelection>> models;
	std::string old_lw;
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

bool insensitive_search(std::string filter_text, std::string a) {
	std::transform(a.begin(), a.end(), a.begin(),
    			[](unsigned char c){ return std::tolower(c); });
	std::transform(filter_text.begin(), filter_text.end(), filter_text.begin(),
		[](unsigned char c){ return std::tolower(c); });
	return a.starts_with(filter_text);
}

void open_file_clicked() {
	system(("xdg-open "+kapp.eui.de.path).c_str());
}

void set_icon(Gtk::Image* img, std::string icon_str) {
	if (icon_str == "")
		img->set_from_icon_name("applications-other");
	else if (icon_str.find("/") == Glib::ustring::npos)
		img->set_from_icon_name(icon_str);
	else
		gtk_image_set_from_file(img->gobj(), icon_str.c_str());
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
	kapp.eui.open_file_button = kapp.builder->get_widget<Gtk::Button>("open_file_button");
	kapp.eui.save_button = kapp.builder->get_widget<Gtk::Button>("save_button");
	kapp.eui.open_file_button->signal_clicked().connect(sigc::ptr_fun(open_file_clicked));
}

static void lui_setup(const Glib::RefPtr<Gtk::ListItem>& list_item, bool expand) {
	GtkWidget *exp = gtk_expander_new (NULL);
	gtk_expander_set_expanded(GTK_EXPANDER(exp), expand);
  	gtk_list_item_set_child (list_item->gobj(), exp);
}

void lui_teardown(const Glib::RefPtr<Gtk::ListItem> &li) {
	auto c = dynamic_cast<Gtk::Expander*>(li->get_child());
	li->unset_child();
	delete c;
}

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

  	gtk_list_item_set_child (list_item->gobj(), box);
}

static void cat_teardown(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	auto box = dynamic_cast<Gtk::Box*>(list_item->get_child());
	list_item->unset_child();
	delete box;
}

void catlist_button_clicked(const Glib::RefPtr<Gtk::ListItem>& list_item, std::string list_ind, size_t lw_index) {
	if (!list_item->get_selected())
		return;
	auto new_box = dynamic_cast<Gtk::Box*>(list_item->get_child());
	auto new_label = dynamic_cast<Gtk::Label*>(new_box->get_children()[1]);
	kapp.eui.de = kapp.list[kapp.catlist[list_ind][new_label->get_label()]];
	if (kapp.models[kapp.old_lw] && kapp.old_lw != list_ind) {
		kapp.models[kapp.old_lw]->set_selected(-1);
	}
	kapp.old_lw = lw_index;
	set_from_desktop_entry(&kapp.eui, kapp.eui.de);
}

// This is called for each of the items in each of the expanders.
static void cat_bind(const Glib::RefPtr<Gtk::ListItem>& list_item, std::string cat_name, size_t lw_index) {
	// Get the name of the item
	Glib::ustring strobj = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj())));
	// Get the widget created in cat_setup.
	auto box = dynamic_cast<Gtk::Box*>(list_item->get_child());
	auto lb = dynamic_cast<Gtk::Label*>(box->get_children()[1]);
	auto icon = dynamic_cast<Gtk::Image*>(box->get_children()[0]);
	lb->set_label(strobj);
	
	// Set the icon
	set_icon(icon, de_val(kapp.list[kapp.catlist[cat_name][strobj]], "Icon"));
	icon->set_pixel_size(24);

	auto sp_fn = std::bind(catlist_button_clicked, list_item, cat_name, lw_index);
	list_item->connect_property_changed("selected", sp_fn);
}

static void cat_unbind(const Glib::RefPtr<Gtk::ListItem>& list_item, std::string cat_name, size_t lw_index) {
	// Get the name of the item
	Glib::ustring strobj = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj())));
	// Get the widget created in cat_setup.
	auto box = dynamic_cast<Gtk::Box*>(list_item->get_child());
	auto lb = dynamic_cast<Gtk::Label*>(box->get_children()[1]);
	auto icon = dynamic_cast<Gtk::Image*>(box->get_children()[0]);
	lb->set_label("");
}

static void lui_bind(const Glib::RefPtr<Gtk::ListItem>& list_item) {
	auto lb = dynamic_cast<Gtk::Expander *>(list_item.get()->get_child());
	Glib::ustring strobj = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj())));
	lb->set_label(strobj.c_str());
	auto attrs = Pango::AttrList::from_string("0 -1 font \"Sans 12\"");
	auto label = dynamic_cast<Gtk::Label*>(lb->get_label_widget());
	label->set_attributes(attrs);
	auto gsl = Gtk::StringList::create();
	for (auto const& [name, ind] : kapp.catlist[strobj]) {
		if (!kapp.etxt->get_text().empty()) {
			if (!insensitive_search(kapp.etxt->get_text().c_str(), name))
				continue;
		}
		gsl->append(name);
	}
	kapp.models[strobj] = Gtk::SingleSelection::create();
	kapp.models[strobj]->set_autoselect(false);
	kapp.models[strobj]->set_model(gsl);
	auto fac = Gtk::SignalListItemFactory::create();
	fac->signal_setup().connect(sigc::ptr_fun(cat_setup));
	fac->signal_bind().connect(std::bind(cat_bind, std::placeholders::_1, strobj, kapp.category_views.size()));
	fac->signal_unbind().connect(std::bind(cat_unbind, std::placeholders::_1, strobj, kapp.category_views.size()));
	fac->signal_teardown().connect(sigc::ptr_fun(cat_teardown));
	kapp.category_views[strobj] = new Gtk::ListView();
	kapp.category_views[strobj]->set_model(kapp.models[strobj]);
	kapp.category_views[strobj]->set_factory(fac);
	kapp.category_views[strobj]->set_single_click_activate(false);
	lb->set_child(*dynamic_cast<Gtk::Widget*>(kapp.category_views[strobj]));
	set_from_desktop_entry(&kapp.eui, kapp.eui.de);
}

void lui_unbind(const Glib::RefPtr<Gtk::ListItem> &li) {
	auto lb = dynamic_cast<Gtk::Expander *>(li.get()->get_child());
	Glib::ustring strobj = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item (li->gobj())));
	lb->unset_child();
	delete lb;
	delete kapp.category_views[strobj];
}

static void add_button_clicked() {
	if (kapp.etxt->get_text() == "")
		return;
	kapp.lui.sl->append(kapp.etxt->get_text());
	kapp.etxt->set_text("");
}

void search_entry_changed(void) {
	std::string filter_text = kapp.etxt->get_text();
	std::map<std::string, bool> ce;
	for (size_t i = 0; i < kapp.list.size(); i++) {
		if (insensitive_search(filter_text, kapp.list[i].pe["Desktop Entry"]["Name"].strval)) {
			for (size_t j = 0; j < kapp.list[i].Categories.size(); j++) {
				ce[kapp.list[i].Categories[j]] = true;
			}
		}
	}
	kapp.lui.strings.clear();
	for (auto const& [name, _b] : ce) {
		kapp.lui.strings.push_back(name);
	}
	kapp.lui.sl = Gtk::StringList::create(kapp.lui.strings);
	kapp.lui.ns = Gtk::NoSelection::create(kapp.lui.sl);
	kapp.lui.lif->signal_setup().connect(std::bind(lui_setup, std::placeholders::_1, !filter_text.empty()));
	kapp.lui.listem->set_model(kapp.lui.ns);
}

void scan_folder(std::string folder_path) {
	size_t tildepos = folder_path.find("~");
	if (tildepos != std::string::npos) {
		folder_path = folder_path.replace(tildepos, 1, std::getenv("HOME"));
	}
	if (access(folder_path.c_str(), F_OK) != 0) {
		printf("Folder %s doesn't exist\n", folder_path.c_str());
		return;
	}
	for (const auto & entry : std::filesystem::directory_iterator(folder_path)){
		try {
			if (entry.is_directory()) {
				scan_folder(entry.path());
				continue;
			}
			auto pde = deskentry::parse_file(entry.path(), kapp.XDG_ENV);
			if(pde.has_value()) {
				auto pe = pde.value();
				if (!pe.HiddenFilter || !pe.NoDisplayFilter || !pe.OnlyShowInFilter) {
					for (size_t i = 0; i < pe.Categories.size(); i++) {
						kapp.catlist[pe.Categories[i]][pe.pe["Desktop Entry"]["Name"].strval] = kapp.list.size();
					}
					kapp.list.push_back(pe);
				}
			}
		} catch (std::exception& e) {
			printf("!!ERROR!! %s: %s\n", entry.path().c_str(), e.what());
			return;
		}
	}
}

void init_ui() {
	kapp.main_window = kapp.builder->get_widget<Gtk::Window>("main_window");
	kapp.main_window->set_application(kapp.app);
	
	// Initialize "sidepanel"
	entry_ui();
	
	kapp.etxt = kapp.builder->get_widget<Gtk::Entry>("etxt");
	kapp.etxt->signal_changed().connect(sigc::ptr_fun(search_entry_changed));

	std::string datadirs = std::getenv("XDG_DATA_DIRS");
	if (!datadirs.empty()) {
		std::string it = datadirs;
                size_t pos = it.find(":");
                while (pos != std::string::npos) {
                        scan_folder(it.substr(0, pos)+"/applications/");
                        it = it.substr(pos+1);
                        pos = it.find(":");
                }
		scan_folder(it+"/applications/");
	}
	scan_folder("~/.local/share/applications/");
	scan_folder("~/Desktop/");
	for (auto const& [group, apps] : kapp.catlist) {
		kapp.lui.strings.push_back(group);
	}
	std::sort(kapp.lui.strings.begin(), kapp.lui.strings.end());
	kapp.lui.sl = Gtk::StringList::create(kapp.lui.strings); // gtk_string_list_new ((const char * const *) array);
	kapp.lui.ns = Gtk::NoSelection::create(kapp.lui.sl); // gtk_no_selection_new (G_LIST_MODEL (sl));
	
	kapp.lui.lif = Gtk::SignalListItemFactory::create();
	kapp.lui.lif->signal_setup().connect(std::bind(lui_setup, std::placeholders::_1, false));
	kapp.lui.lif->signal_bind().connect(sigc::ptr_fun(lui_bind));
	kapp.lui.lif->signal_unbind().connect(sigc::ptr_fun(lui_unbind));
	kapp.lui.lif->signal_teardown().connect(sigc::ptr_fun(lui_teardown));

	kapp.lui.listem = kapp.builder->get_widget<Gtk::ListView>("listem");
	if (kapp.lui.listem) {
		kapp.lui.listem->set_model(kapp.lui.ns);
		kapp.lui.listem->set_factory(kapp.lui.lif);
	}

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
