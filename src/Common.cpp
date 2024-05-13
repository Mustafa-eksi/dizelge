#pragma once
#include <gtkmm.h>
#include "desktop.cpp"

/*
 * Common things that multiple files use.
 */

typedef std::map<std::string, size_t> ListMap;
typedef std::map<std::string, ListMap> CategoryEntries;

typedef std::map<std::string, Gtk::ListView*> ListviewList;
typedef std::map<std::string, Glib::RefPtr<Gtk::SingleSelection>> ModelList;
typedef std::map<std::string, Glib::RefPtr<Gtk::SignalListItemFactory>> FactoryList;

typedef std::vector<deskentry::DesktopEntry> EntryList;
typedef std::map<std::string, sigc::connection> SignalMap;
typedef std::map<std::string, SignalMap> CatSignalMap;

typedef void (*ListSelectionCallback)(guint p, guint p2, Glib::RefPtr<Gtk::SingleSelection> sm, bool is_category, bool is_mapped, std::string list_ind);

// Only a helper
std::string de_val(deskentry::DesktopEntry de, std::string key) {
        return de.pe["Desktop Entry"][key];
}

// TODO: Replace this with fuzzy search.
bool insensitive_search(std::string filter_text, std::string a) {
	std::transform(a.begin(), a.end(), a.begin(),
    			[](unsigned char c){ return std::tolower(c); });
	std::transform(filter_text.begin(), filter_text.end(), filter_text.begin(),
		[](unsigned char c){ return std::tolower(c); });
	return a.find(filter_text) != std::string::npos;
}

void set_icon(GtkImage* img, std::string icon_str) {
	if (icon_str == "")
		gtk_image_set_from_icon_name(img, "applications-other");
	else if (icon_str.find("/") == Glib::ustring::npos)
		gtk_image_set_from_icon_name(img, icon_str.c_str());
	else
		gtk_image_set_from_file(img, icon_str.c_str());
}

void set_icon(Gtk::Image* img, std::string icon_str) {
	if (icon_str == "")
		img->set_from_icon_name("applications-other");
	else if (icon_str.find("/") == Glib::ustring::npos)
		img->set_from_icon_name(icon_str);
	else
		gtk_image_set_from_file(img->gobj(), icon_str.c_str());
}
