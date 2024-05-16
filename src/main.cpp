#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <gtkmm-4.0/gtkmm/noselection.h>
#include <gtkmm.h>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <functional>
#include <stdlib.h>

#include "desktop.cpp"
#include "CategoryList.cpp"
#include "Common.cpp"
#include "EntryUi.cpp"
#include "NewShortcut.cpp"
#include "gtkmm/singleselection.h"
#include "gtkmm/stringobject.h"
#include "sigc++/functors/ptr_fun.h"

const std::string EXECUTABLE_NAME = "dizelge";

/*
 * This file contains some ui stuff and general stuff that doesn't fit to other files.
 */

typedef struct {
	Gtk::ListView *listem;
	std::vector<Glib::ustring> strings;
	Glib::RefPtr<Gtk::StringList> sl;
	Glib::RefPtr<Gtk::SignalListItemFactory> lif;
	Glib::RefPtr<Gtk::NoSelection> ns;
	Glib::RefPtr<Gtk::SingleSelection> ss;
	sigc::connection setup, bind, unbind, teardown;
} ListUi;

struct KisayolApp {
	Glib::RefPtr<Gtk::Application> app;
	Glib::RefPtr<Gtk::Builder> builder;
	
	Gtk::Window *main_window, *entry_window, *new_window;

	// TODO: move these to somewhere.
	Gtk::Button *add_new_button;
	Gtk::Entry *etxt;
	Gtk::DropDown *folder_dd;
	
	std::mutex list_mutex;
	EntryList list;
	CategoryView catview;
	SignalMap smap;
	ListMap lmap;

	ListUi lui;
	EntryUi eui;
	
	std::vector<Glib::ustring> scanned_folders;
	size_t selected_folder;
	char *XDG_ENV;
};

// TODO: get rid of global variables
struct KisayolApp kapp = { 0 };

using namespace std::placeholders; // This is to avoid writing "std::placeholders::_1" which makes code harder to read.

// TODO: Put these into settings.
const bool is_catview = false;

bool scan_file(KisayolApp *kapp, std::string path) {
	auto pde = deskentry::parse_file(path, kapp->XDG_ENV);
	if (!pde.has_value())
		return false;
	auto pe = pde.value();
	//if (pe.type != deskentry::EntryType::Application)
	//	printf("-> %s\n", path.c_str());
	if (!pe.HiddenFilter || !pe.NoDisplayFilter || !pe.OnlyShowInFilter) {
		kapp->list_mutex.lock();
		for (size_t i = 0; i < pe.Categories.size(); i++) {
			kapp->catview.catlist[pe.Categories[i]][pe.pe["Desktop Entry"]["Name"]] = kapp->list.size();
		}
		kapp->list.push_back(pe);
		kapp->list_mutex.unlock();
	}
	return true;
}

bool scan_folder(std::string folder_path) {
	// Replace tildes (~) with /home/<username>
	size_t tildepos = folder_path.find("~");
	if (tildepos != std::string::npos) // Rep
		folder_path = folder_path.replace(tildepos, 1, std::getenv("HOME"));

	if (access(folder_path.c_str(), F_OK) != 0) // Folder doesn't exist
		return false;

	std::vector<std::thread> threads;
	for (const auto & entry : std::filesystem::directory_iterator(folder_path)){
		try {
			if (!entry.is_regular_file() && !entry.is_directory()) continue;
			if (entry.is_directory()) {
				// recursive scan:
				//scan_folder(entry.path());
				continue;
			}
			//scan_file(&kapp, entry.path());
			threads.push_back(std::thread(scan_file, &kapp, entry.path()));
		} catch (std::exception& e) {
			printf("!!ERROR!! %s: %s\n", entry.path().c_str(), e.what());
			return false;
		}
	}
	for (auto& t : threads) {
		t.join();
	}
	return true;
}

void scan_combined() {
	kapp.scanned_folders.push_back("Combined");
	std::string datadirs = std::getenv("XDG_DATA_DIRS");
	if (!datadirs.empty()) {
		std::string it = datadirs;
		size_t pos = it.find(":");
		while (pos != std::string::npos) {
			if (scan_folder(it.substr(0, pos)+"/applications/")) {
				kapp.scanned_folders.push_back(it.substr(0, pos)+"/applications/");
			}
			it = it.substr(pos+1);
			pos = it.find(":");
		}
		if (scan_folder(it+"/applications/")) {
			kapp.scanned_folders.push_back(it+"/applications/");
		}
	}
	scan_folder("~/.local/share/applications/");
	kapp.scanned_folders.push_back("~/.local/share/applications/");
	scan_folder("~/Desktop/");
	kapp.scanned_folders.push_back("~/Desktop/");
}

void list_selection_changed(guint p, guint p2, Glib::RefPtr<Gtk::SingleSelection> sm, bool is_category, bool is_mapped, std::string list_ind) {
	auto selected_index = sm->get_selected();
	if (selected_index == -1) return;
	if (is_category) {
		if (kapp.catview.models->contains(kapp.catview.previous_cat) && kapp.catview.previous_cat != list_ind) {
			kapp.catview.models->at(kapp.catview.previous_cat)->set_selected(-1);
		}
		kapp.catview.previous_cat = list_ind;
	}
	auto appname = dynamic_cast<Gtk::StringObject*>(sm->get_model()->get_object(selected_index).get())->get_string();
	auto ind = is_category ? kapp.catview.catlist[list_ind][appname] : is_mapped ? kapp.lmap[appname] : sm->get_selected();
	kapp.eui.de = kapp.list[ind];
	set_from_desktop_entry(&kapp.eui, kapp.eui.de);
}

void listview_signals(bool is_catview, bool is_mapped) {
	if (kapp.lui.setup || kapp.lui.bind || kapp.lui.unbind || kapp.lui.teardown) {
		kapp.lui.setup.disconnect();
		kapp.lui.bind.disconnect();
		kapp.lui.unbind.disconnect();
		kapp.lui.teardown.disconnect();
	}
	if (is_catview) {
		kapp.catview.category_views.reset();
		kapp.catview.models.reset();
		kapp.catview.factories.reset();
		kapp.catview.category_views = std::make_shared<ListviewList>(ListviewList());
		kapp.catview.models = std::make_shared<ModelList>(ModelList());
		kapp.catview.factories = std::make_shared<FactoryList>(FactoryList());
		kapp.lui.setup = kapp.lui.lif->signal_setup().connect(std::bind(categorylist_setup, _1, is_mapped));
		kapp.lui.bind = kapp.lui.lif->signal_bind().connect(std::bind(categorylist_bind, _1,
													  &kapp.catview, &kapp.list, &list_selection_changed));
		kapp.lui.unbind = kapp.lui.lif->signal_unbind().connect(std::bind(categorylist_unbind, _1, kapp.catview.category_views, &kapp.catview));
		kapp.lui.teardown = kapp.lui.lif->signal_teardown().connect(sigc::ptr_fun(categorylist_teardown));
	} else {
		kapp.lui.setup = kapp.lui.lif->signal_setup().connect(sigc::ptr_fun(applist_setup));
		kapp.lui.bind = kapp.lui.lif->signal_bind().connect(std::bind(applist_bind, _1, &kapp.list, &kapp.lmap, is_mapped));
		kapp.lui.unbind = kapp.lui.lif->signal_unbind().connect(std::bind(applist_unbind, _1));
		kapp.lui.teardown = kapp.lui.lif->signal_teardown().connect(sigc::ptr_fun(applist_teardown));
		kapp.lui.ss->signal_selection_changed().connect(sigc::bind(&list_selection_changed, kapp.lui.ss, is_catview, false, ""));
	}
}

void populate_stringlist(bool is_catview) {
	kapp.lui.strings.clear();
	if (is_catview) {
		for (auto const& [group, apps] : kapp.catview.catlist) {
			kapp.lui.strings.push_back(group);
		}
		std::sort(kapp.lui.strings.begin(), kapp.lui.strings.end());
	} else {
		for (auto app : kapp.list) {
			kapp.lui.strings.push_back(app.pe["Desktop Entry"]["Name"]);
		}
	}
}

void folder_dd_select() {
	size_t new_folder = kapp.folder_dd->get_selected();
	if (kapp.selected_folder != new_folder) {
		kapp.catview.catlist.clear();
		kapp.list.clear();
		kapp.lui.strings.clear();
		if (new_folder == 0) {
			kapp.scanned_folders.clear();
			scan_combined();
		} else {
			if (!scan_folder(kapp.scanned_folders[new_folder]))
				return;
		}
		populate_stringlist(is_catview);
		if (kapp.lui.strings.empty()) return;
		kapp.lui.sl.reset();
		kapp.lui.sl = Gtk::StringList::create(kapp.lui.strings);
		if (is_catview) {
			kapp.lui.ns = Gtk::NoSelection::create(kapp.lui.sl);
			kapp.lui.listem->set_model(kapp.lui.ns);
		} else {
			kapp.lui.ss.reset();
			kapp.lui.ss = Gtk::SingleSelection::create(kapp.lui.sl);
			if (!kapp.lui.ss) return;
			kapp.lui.listem->set_model(kapp.lui.ss);
		}
		listview_signals(is_catview, false);
		kapp.selected_folder = new_folder;
	}
}

bool new_window_close() {
	kapp.new_window->unset_application();
	return false;
}

void add_new_button_clicked(void) {
	kapp.new_window->show();
	kapp.new_window->set_application(kapp.app);
	kapp.new_window->signal_close_request().connect(&new_window_close, false);
	new_ui();
}

void initialize_list() {
	populate_stringlist(is_catview);
	kapp.lui.sl = Gtk::StringList::create(kapp.lui.strings); // gtk_string_list_new ((const char * const *) array);
	if (is_catview) {
		kapp.lui.ns = Gtk::NoSelection::create(kapp.lui.sl);
	} else {
		kapp.lui.ss = Gtk::SingleSelection::create(kapp.lui.sl);
	}

	kapp.lui.lif = Gtk::SignalListItemFactory::create();

	listview_signals(is_catview, false);

	kapp.lui.listem = kapp.builder->get_widget<Gtk::ListView>("listem");
	if (!kapp.lui.listem)
		return;
	if (is_catview)
		kapp.lui.listem->set_model(kapp.lui.ns);
	else
		kapp.lui.listem->set_model(kapp.lui.ss);
	kapp.lui.listem->set_factory(kapp.lui.lif);
}

void search_entry_changed(bool is_category) {
	std::string filter_text = kapp.etxt->get_text();
	if (filter_text.empty()) {
		initialize_list();
		return;
	}
	kapp.lui.sl.reset();
	if (is_category) {
		std::map<std::string, bool> ce;
		for (size_t i = 0; i < kapp.list.size(); i++) {
			if (insensitive_search(filter_text, kapp.list[i].pe["Desktop Entry"]["Name"])) {
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
		kapp.lui.ns.reset();
		kapp.lui.ns = Gtk::NoSelection::create(kapp.lui.sl);
		kapp.lui.lif->signal_setup().connect(std::bind(categorylist_setup, _1, !filter_text.empty()));
		kapp.lui.listem->set_model(kapp.lui.ns);
	} else {
		kapp.lui.strings.clear();
		for (size_t i = 0; i < kapp.list.size(); i++) {
			if (insensitive_search(filter_text, kapp.list[i].pe["Desktop Entry"]["Name"])) {
				kapp.lui.strings.push_back(kapp.list[i].pe["Desktop Entry"]["Name"]);
				kapp.lmap[kapp.list[i].pe["Desktop Entry"]["Name"]] = i;
			}
		}
		kapp.lui.sl = Gtk::StringList::create(kapp.lui.strings);
		kapp.lui.ss.reset();
		kapp.lui.ss = Gtk::SingleSelection::create(kapp.lui.sl);
		listview_signals(false, true);
		kapp.lui.listem->set_model(kapp.lui.ss);
		kapp.lui.ss->set_selected(-1);
	}
}

void init_ui() {
	kapp.main_window = kapp.builder->get_widget<Gtk::Window>("main_window");
	kapp.main_window->set_application(kapp.app);
	
	// Initialize "sidepanel"
	entry_ui(&kapp.eui, kapp.builder);
	
	kapp.etxt = kapp.builder->get_widget<Gtk::Entry>("etxt");
	kapp.etxt->signal_changed().connect(std::bind(search_entry_changed, is_catview));

	scan_combined();
	initialize_list();

	kapp.folder_dd = kapp.builder->get_widget<Gtk::DropDown>("paths_to_scan");
	auto ddsl = Gtk::StringList::create(kapp.scanned_folders);
	auto ddss = Gtk::SingleSelection::create(ddsl);
	auto factory = Gtk::SignalListItemFactory::create();
	factory->signal_setup().connect(sigc::ptr_fun(dd_setup));
	factory->signal_bind().connect(sigc::ptr_fun(dd_bind));
	
	kapp.folder_dd->set_model(ddss);
	kapp.folder_dd->set_factory(factory);
	kapp.folder_dd->set_selected(0);
	kapp.folder_dd->connect_property_changed("selected", sigc::ptr_fun(folder_dd_select));
	
	kapp.new_window = kapp.builder->get_widget<Gtk::Window>("new_desktop_window");
	kapp.new_window->set_hide_on_close(true);

	kapp.add_new_button = kapp.builder->get_widget<Gtk::Button>("add_new_button");
	kapp.add_new_button->signal_clicked().connect(&add_new_button_clicked);

	kapp.main_window->present();
}

int main(int argc, char* argv[])
{
	// Quick hack for accessing .glade file from anywhere
	std::string first_arg = argv[0];
	std::string abs_path = first_arg.substr(0, first_arg.length()-EXECUTABLE_NAME.length());
	
	kapp.app = Gtk::Application::create("org.mustafa.dizelge");
	// FIXME: this assumes dizelge.ui and executable are in the same directory.
	kapp.builder = Gtk::Builder::create_from_file(abs_path + "dizelge.ui");
	if(!kapp.builder)
		return 1;

	kapp.XDG_ENV = std::getenv("XDG_CURRENT_DESKTOP");
	if (kapp.XDG_ENV == NULL)
		kapp.XDG_ENV = const_cast<char*>("none");
	
	//kapp.eui.filepath = "/usr/share/applications/btop.desktop";
	kapp.app->signal_activate().connect(sigc::ptr_fun(init_ui));

	return kapp.app->run(argc, argv);
}
