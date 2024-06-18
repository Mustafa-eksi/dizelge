#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <gtkmm.h>
#include <filesystem>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <functional>
#include <stdlib.h>
#include <sys/stat.h>

#include "desktop.cpp"
#include "CategoryList.cpp"
#include "Common.cpp"
#include "EntryUi.cpp"
#include "WebApp.cpp"

const std::string EXECUTABLE_NAME = "dizelge";
const std::string NEW_SHORTCUT_CAT = "MyShortcuts";
/*
 * This file contains some ui stuff and general stuff that doesn't fit to other files.
 */

typedef struct {
	Gtk::ListView *listem;
	std::deque<Glib::ustring> strings;
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
	Gtk::Button *add_new_button, *add_wap_button, *delete_button;
	Gtk::CheckButton *catview_check;
	Gtk::Entry *etxt;
	Gtk::DropDown *folder_dd;

	GtkWidget *ad;
	
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
	std::string data_home, desktop_dir;
	bool open_file_mode;
	std::string open_file_path;
};

// TODO: get rid of global variables
struct KisayolApp kapp = { 0 };

using namespace std::placeholders; // This is to avoid writing "std::placeholders::_1" which makes code harder to read.

// TODO: Put these into settings.
bool is_catview = false;

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

void recalculate_catlist() {
	kapp.catview.catlist.clear();
	for (size_t i = 0; i < kapp.list.size(); i++) {
		for (const auto& c : kapp.list[i].Categories) {
			kapp.catview.catlist[c][kapp.list[i].pe["Desktop Entry"]["Name"]] = i;
		}
	}
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

	FILE *ddp = popen("xdg-user-dir DESKTOP", "r");

	char read_buf[1024] = {0};
	fgets(read_buf, 1023, ddp);
	if (strlen(read_buf) > 0) {
		kapp.desktop_dir = read_buf;
		auto newline = kapp.desktop_dir.find_last_of('\n');
		if (newline != std::string::npos)
			kapp.desktop_dir = kapp.desktop_dir.substr(0, newline);
		scan_folder(kapp.desktop_dir);
		kapp.scanned_folders.push_back(kapp.desktop_dir);
	}
	pclose(ddp);
}

void list_selection_changed(guint p, guint p2, Glib::RefPtr<Gtk::SingleSelection> sm, bool is_category, bool is_mapped, std::string list_ind) {
	auto selected_index = sm->get_selected();
	if (selected_index == GTK_INVALID_LIST_POSITION) return;
	if (is_category) {
		if (kapp.catview.models->contains(kapp.catview.previous_cat) && kapp.catview.previous_cat != list_ind) {
			kapp.catview.models->at(kapp.catview.previous_cat)->set_selected(-1);
		}
		kapp.catview.previous_cat = list_ind;
	}
	auto appname = dynamic_cast<Gtk::StringObject*>(sm->get_model()->get_object(selected_index).get())->get_string();
	auto ind = is_category ? kapp.catview.catlist[list_ind][appname] : is_mapped ? kapp.lmap[appname] : sm->get_selected();
	set_from_desktop_entry(&kapp.eui, &kapp.list[ind]);
}

void listview_signals(bool is_catview, bool is_mapped, std::string added_new="") {
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
													  &kapp.catview, &kapp.list, &list_selection_changed, added_new));
		kapp.lui.unbind = kapp.lui.lif->signal_unbind().connect(std::bind(categorylist_unbind, _1, kapp.catview.category_views, &kapp.catview));
		kapp.lui.teardown = kapp.lui.lif->signal_teardown().connect(sigc::ptr_fun(categorylist_teardown));
	} else {
		kapp.lui.setup = kapp.lui.lif->signal_setup().connect(sigc::ptr_fun(applist_setup));
		kapp.lui.bind = kapp.lui.lif->signal_bind().connect(std::bind(applist_bind, _1, &kapp.list, &kapp.lmap, is_mapped, kapp.lui.listem));
		kapp.lui.unbind = kapp.lui.lif->signal_unbind().connect(std::bind(applist_unbind, _1));
		kapp.lui.teardown = kapp.lui.lif->signal_teardown().connect(sigc::ptr_fun(applist_teardown));
		kapp.lui.ss->signal_selection_changed().connect(sigc::bind(&list_selection_changed, kapp.lui.ss, is_catview, is_mapped, ""));
	}
}

void populate_stringlist(bool is_catview) {
	kapp.lui.strings.clear();
	if (is_catview) {
		bool myshortcuts = false;
		for (auto const& [group, apps] : kapp.catview.catlist) {
			if (group.compare(NEW_SHORTCUT_CAT) != 0)
				kapp.lui.strings.push_back(group);
			else
				myshortcuts = true;
		}
		std::sort(kapp.lui.strings.begin(), kapp.lui.strings.end());
		if (myshortcuts)
			kapp.lui.strings.push_front(NEW_SHORTCUT_CAT);
	} else {
		for (auto app : kapp.list) {
			kapp.lui.strings.push_back(app.pe["Desktop Entry"]["Name"]);
		}
	}
}

void initialize_list(std::string added_new="") {
	populate_stringlist(is_catview);
	kapp.lui.sl = Gtk::StringList::create({kapp.lui.strings.begin(), kapp.lui.strings.end()}); // gtk_string_list_new ((const char * const *) array);
	if (is_catview) {
		kapp.lui.ns = Gtk::NoSelection::create(kapp.lui.sl);
	} else {
		kapp.lui.ss = Gtk::SingleSelection::create(kapp.lui.sl);
	}

	kapp.lui.lif = Gtk::SignalListItemFactory::create();

	listview_signals(is_catview, false, added_new);

	kapp.lui.listem = kapp.builder->get_widget<Gtk::ListView>("listem");
	if (!kapp.lui.listem)
		return;
	if (is_catview)
		kapp.lui.listem->set_model(kapp.lui.ns);
	else
		kapp.lui.listem->set_model(kapp.lui.ss);
	kapp.lui.listem->set_factory(kapp.lui.lif);
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
		recalculate_catlist();
		initialize_list();
		/*populate_stringlist(is_catview);
		if (kapp.lui.strings.empty()) return;
		kapp.lui.sl.reset();
		kapp.lui.sl = Gtk::StringList::create({kapp.lui.strings.begin(), kapp.lui.strings.end()});
		if (is_catview) {
			kapp.lui.ns.reset();
			kapp.lui.ns = Gtk::NoSelection::create(kapp.lui.sl);
			kapp.lui.listem->set_model(kapp.lui.ns);
		} else {
			kapp.lui.ss.reset();
			kapp.lui.ss = Gtk::SingleSelection::create(kapp.lui.sl);
			if (!kapp.lui.ss) return;
			kapp.lui.listem->set_model(kapp.lui.ss);
		}*/
		//listview_signals(is_catview, false);
		kapp.selected_folder = new_folder;
	}
}

void add_new_button_clicked(void) {
	deskentry::UnparsedEntry ue;
	ue["Desktop Entry"]["Name"] = "Enter your shortcut's name here";
	ue["Desktop Entry"]["Categories"] = NEW_SHORTCUT_CAT;
	deskentry::DesktopEntry newentry = {
		.pe = ue,
		.type = deskentry::EntryType::Application,
		.Categories = {NEW_SHORTCUT_CAT},
	};

	kapp.list.push_front(newentry);
	// kapp.catview.catlist[newentry.Categories.front()][ue["Desktop Entry"]["Name"]] = 0;
	recalculate_catlist();
	initialize_list(NEW_SHORTCUT_CAT);

	//kapp.lui.listem->scroll_to(0, Gtk::ListScrollFlags::SELECT);
	set_from_desktop_entry(&kapp.eui, &kapp.list.front());
}

void add_wap_button_clicked(void) {
	kapp.new_window->signal_close_request().connect(&close_wap, false);
	kapp.new_window->set_application(kapp.app);
	kapp.new_window->present();
	new_ui(kapp.builder, kapp.data_home, kapp.desktop_dir);
}

void erase_current(size_t to_erase) {
	kapp.list.erase(kapp.list.begin() + to_erase);
	if (is_catview) {
		recalculate_catlist();
	} else {
		size_t new_selected = to_erase > 0 ? to_erase-1 : 0;
		kapp.lui.ss->set_selected(new_selected);
		set_from_desktop_entry(&kapp.eui, &kapp.list[new_selected]);
	}
	initialize_list();
}

void delete_confirm(GtkDialog* self, gint response_id, gpointer user_data) {
	std::string depath = *(std::string*)user_data;
	if (response_id == -4) return;
	if (response_id != -5) {
		gtk_window_close(GTK_WINDOW(kapp.ad));
		return;
	}
	if (depath.empty()) return;
	if (access(depath.c_str(), W_OK) != 0) {
		system(("pkexec bash -c \"rm "+depath+"\"").c_str());
	} else {
		unlink(depath.c_str());
	}
	if (!kapp.open_file_mode) {
		size_t i = 0;
		for (auto& b : kapp.list) {
			if (b.path == depath)
				erase_current(i);
			i++;
		}
	} else {
		kapp.main_window->close();
	}
	gtk_window_close(GTK_WINDOW(kapp.ad));
}

void delete_button_clicked(void) {
	size_t to_erase = 0;
	if (kapp.eui.de == NULL) return;
	if (!kapp.eui.de->path.empty()) {
		kapp.ad = gtk_message_dialog_new(kapp.main_window->gobj(), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, "Are you sure to delete %s", kapp.eui.de->path.c_str());
		g_signal_connect(kapp.ad, "response", G_CALLBACK(delete_confirm), &kapp.eui.de->path);
		gtk_window_present(GTK_WINDOW(kapp.ad));
		return;
	}
	if (kapp.open_file_mode) return;
	if (is_catview) {
		if (!kapp.catview.models->contains(kapp.catview.previous_cat))
			return;
		auto catind = dynamic_cast<Gtk::StringObject*>(kapp.catview.models->at(kapp.catview.previous_cat)->get_selected_item().get())->get_string();
		to_erase = kapp.catview.catlist[kapp.catview.previous_cat][catind];
	} else {
		to_erase = kapp.lui.ss->get_selected();
	}
	if (to_erase == GTK_INVALID_LIST_POSITION)
		return;
	erase_current(to_erase);
}

void search_entry_changed() {
	std::string filter_text = kapp.etxt->get_text();
	if (filter_text.empty()) {
		initialize_list();
		return;
	}
	kapp.lui.sl.reset();
	if (is_catview) {
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
		kapp.lui.sl = Gtk::StringList::create({kapp.lui.strings.begin(), kapp.lui.strings.end()});
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
		kapp.lui.sl = Gtk::StringList::create({kapp.lui.strings.begin(), kapp.lui.strings.end()});
		kapp.lui.ss.reset();
		kapp.lui.ss = Gtk::SingleSelection::create(kapp.lui.sl);
		listview_signals(false, true);
		kapp.lui.listem->set_model(kapp.lui.ss);
		kapp.lui.ss->set_selected(-1);
	}
}

void catview_toggled() {
	is_catview = kapp.catview_check->get_active();
	if (is_catview)
		recalculate_catlist();
	initialize_list();
}

void init_ui() {
	kapp.main_window = kapp.builder->get_widget<Gtk::Window>("main_window");
	kapp.main_window->set_application(kapp.app);
	
	// Initialize "sidepanel"
	entry_ui(&kapp.eui, kapp.builder, kapp.main_window->gobj());

	kapp.delete_button = kapp.builder->get_widget<Gtk::Button>("delete_button");
	kapp.delete_button->signal_clicked().connect(&delete_button_clicked);

	kapp.catview_check = kapp.builder->get_widget<Gtk::CheckButton>("catview_check");
	kapp.catview_check->set_visible(!kapp.open_file_mode);
	kapp.catview_check->signal_toggled().connect(sigc::ptr_fun(catview_toggled));

	kapp.builder->get_widget<Gtk::Box>("sidepanel")->set_visible(!kapp.open_file_mode);

	if (kapp.open_file_mode) {
		auto mayfail = deskentry::parse_file(kapp.open_file_path, kapp.XDG_ENV);
		if (!mayfail.has_value()) {
			printf("Can't parse desktop entry: %s\n", kapp.open_file_path.c_str());
			return;
		}
		kapp.list.push_back(mayfail.value());
		set_from_desktop_entry(&kapp.eui, &kapp.list.back());
		return kapp.main_window->present();
	}
	
	kapp.etxt = kapp.builder->get_widget<Gtk::Entry>("etxt");
	kapp.etxt->signal_changed().connect(sigc::ptr_fun(search_entry_changed));

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
	kapp.new_window->signal_close_request().connect(&close_wap, false);
	kapp.new_window->set_hide_on_close();

	kapp.add_new_button = kapp.builder->get_widget<Gtk::Button>("add_new_button");
	kapp.add_new_button->signal_clicked().connect(&add_new_button_clicked);

	kapp.add_wap_button = kapp.builder->get_widget<Gtk::Button>("add_wap_button");
	kapp.add_wap_button->signal_clicked().connect(&add_wap_button_clicked);

	kapp.main_window->present();
}

int main(int argc, char* argv[])
{
	// Quick hack for accessing .glade file from anywhere
	if (argc > 1) {
		kapp.open_file_path = argv[1];
		kapp.open_file_mode = true;
	}

	kapp.app = Gtk::Application::create("org.mustafa.dizelge");
	// FIXME: this assumes dizelge.ui and executable are in the same directory.
	auto dh = test_runtime_folders();
	if (dh.has_value())
		kapp.data_home = dh.value();

	kapp.builder = Gtk::Builder::create_from_file("/usr/share/dizelge/dizelge.ui");
	if(!kapp.builder)
		return 1;

	kapp.XDG_ENV = std::getenv("XDG_CURRENT_DESKTOP");
	if (kapp.XDG_ENV == NULL)
		kapp.XDG_ENV = const_cast<char*>("none");

	kapp.app->signal_activate().connect(sigc::ptr_fun(init_ui));

	return kapp.app->run(1, argv);
}
