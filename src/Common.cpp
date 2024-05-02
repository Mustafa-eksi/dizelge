#pragma once
#include <gtkmm.h>
#include "desktop.cpp"

typedef std::map<std::string, std::map<std::string, size_t>> CategoryEntries;

typedef std::map<std::string, Gtk::ListView*> ListviewList;
typedef std::map<std::string, Glib::RefPtr<Gtk::SingleSelection>> ModelList;
typedef std::map<std::string, Glib::RefPtr<Gtk::SignalListItemFactory>> FactoryList;

typedef std::vector<deskentry::DesktopEntry> EntryList;

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
void set_icon(Gtk::Image* img, std::string icon_str) {
	if (icon_str == "")
		img->set_from_icon_name("applications-other");
	else if (icon_str.find("/") == Glib::ustring::npos)
		img->set_from_icon_name(icon_str);
	else
		gtk_image_set_from_file(img->gobj(), icon_str.c_str());
}