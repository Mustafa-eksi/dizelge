#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <gtkmm.h>
#include <format>
#include <filesystem>
#include <iostream>
#include <memory>
#include <tuple>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <functional>
#include <stdlib.h>

#include "desktop.cpp"
#include "CategoryList.cpp"
#include "Common.cpp"
#include "EntryUi.cpp"

struct KisayolApp {
	Glib::RefPtr<Gtk::Application> app;
	Glib::RefPtr<Gtk::Builder> builder;
	
	Gtk::Window *main_window, *entry_window, *new_window;
	Gtk::Button *ab, *add_new_button;
	Gtk::Entry *etxt;
	Gtk::DropDown *folder_dd;
	
	EntryList list;
	CategoryView catview;
	std::string old_lw;
	
	ListUi lui;
	EntryUi eui;
	
	std::vector<Glib::ustring> scanned_folders;
	size_t selected_folder;
	char *XDG_ENV;
};

// TODO: get rid of global variables
struct KisayolApp kapp = { 0 };



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

bool scan_folder(std::string folder_path) {
	size_t tildepos = folder_path.find("~");
	bool res = false;
	if (tildepos != std::string::npos) {
		folder_path = folder_path.replace(tildepos, 1, std::getenv("HOME"));
	}
	if (access(folder_path.c_str(), F_OK) != 0) {
		//printf("Folder %s doesn't exist\n", folder_path.c_str());
		return res;
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
						kapp.catview.catlist[pe.Categories[i]][pe.pe["Desktop Entry"]["Name"].strval] = kapp.list.size();
					}
					res = true;
					kapp.list.push_back(pe);
				}
			}
		} catch (std::exception& e) {
			printf("!!ERROR!! %s: %s\n", entry.path().c_str(), e.what());
			return false;
		}
	}
	return res;
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
			scan_folder(kapp.scanned_folders[new_folder]);
		}
		for (auto const& [group, apps] : kapp.catview.catlist) {
			kapp.lui.strings.push_back(group);
		}
		std::sort(kapp.lui.strings.begin(), kapp.lui.strings.end());
		kapp.lui.sl = Gtk::StringList::create(kapp.lui.strings);
		kapp.lui.ns = Gtk::NoSelection::create(kapp.lui.sl);
		kapp.lui.listem->set_model(kapp.lui.ns);
		kapp.selected_folder = new_folder;
	}
}

void new_ui() {
	
}

void add_new_button_clicked(void) {
	kapp.new_window->show();
	new_ui();
}

void catlist_button_clicked(const Glib::RefPtr<Gtk::ListItem>& list_item, std::string list_ind, std::string appname) {
	if (!list_item->get_selected())
		return;
	kapp.eui.de = kapp.list[kapp.catview.catlist[list_ind][appname]];
	if (kapp.catview.models->contains(kapp.old_lw) && kapp.old_lw != list_ind) {
		kapp.catview.models->at(kapp.old_lw)->set_selected(-1);
	}
	kapp.old_lw = list_ind;
	set_from_desktop_entry(&kapp.eui, kapp.eui.de);
}

void init_ui() {
	kapp.main_window = kapp.builder->get_widget<Gtk::Window>("main_window");
	kapp.main_window->set_application(kapp.app);
	
	// Initialize "sidepanel"
	entry_ui(&kapp.eui, kapp.builder);
	
	kapp.etxt = kapp.builder->get_widget<Gtk::Entry>("etxt");
	kapp.etxt->signal_changed().connect(sigc::ptr_fun(search_entry_changed));

	scan_combined();
	for (auto const& [group, apps] : kapp.catview.catlist) {
		kapp.lui.strings.push_back(group);
	}
	std::sort(kapp.lui.strings.begin(), kapp.lui.strings.end());
	kapp.lui.sl = Gtk::StringList::create(kapp.lui.strings); // gtk_string_list_new ((const char * const *) array);
	kapp.lui.ns = Gtk::NoSelection::create(kapp.lui.sl); // gtk_no_selection_new (G_LIST_MODEL (sl));
	
	kapp.lui.lif = Gtk::SignalListItemFactory::create();

	kapp.catview.category_views = std::make_shared<ListviewList>(ListviewList());
	kapp.catview.models = std::make_shared<ModelList>(ModelList());
	kapp.catview.factories = std::make_shared<FactoryList>(FactoryList());

	kapp.lui.lif->signal_setup().connect(std::bind(lui_setup, std::placeholders::_1, false));
	kapp.lui.lif->signal_bind().connect(std::bind(lui_bind, std::placeholders::_1,
		kapp.catview,
		kapp.list, &catlist_button_clicked));
	kapp.lui.lif->signal_unbind().connect(std::bind(lui_unbind, std::placeholders::_1, kapp.catview.category_views));
	kapp.lui.lif->signal_teardown().connect(sigc::ptr_fun(lui_teardown));

	kapp.lui.listem = kapp.builder->get_widget<Gtk::ListView>("listem");
	if (kapp.lui.listem) {
		kapp.lui.listem->set_model(kapp.lui.ns);
		kapp.lui.listem->set_factory(kapp.lui.lif);
	}

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
	kapp.new_window->set_application(kapp.app);
	kapp.new_window->set_hide_on_close(true);

	kapp.add_new_button = kapp.builder->get_widget<Gtk::Button>("add_new_button");
	kapp.add_new_button->signal_clicked().connect(&add_new_button_clicked);

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
