#pragma once
#include "gtkmm/button.h"
#include "gtkmm/entry.h"
#include "sigc++/functors/ptr_fun.h"
#include <gtkmm.h>
#include <thread>
#include "desktop.cpp"
/*
 * This file contains only the window that opens when "new web app" button is pressed.
 */

struct WebAppUi {
    Gtk::Window *w;
    Gtk::Button *add_button;
    Gtk::Entry *url_entry, *name_entry;
    Gtk::Image *icon;
    Gtk::Stack *icon_stack;
    Gtk::Spinner *icon_spinner;
    std::string name, icon_path;
};

WebAppUi wai = {0};

void download_favicon(std::string fv, std::string output="/tmp/wap-favicon", WebAppUi *wai=NULL) {
    wai->icon_stack->set_visible_child(*wai->icon_spinner);
    wai->icon_spinner->start();
    system(("wget -q -O " + output + " " + fv).c_str());
    gtk_image_set_from_file(wai->icon->gobj(), output.c_str());
    wai->icon->set_icon_size(Gtk::IconSize::LARGE);
    wai->icon_spinner->stop();
    wai->icon_stack->set_visible_child(*wai->icon);
}

void url_changed() {
    auto url = wai.url_entry->get_text();
    printf("url: '%s'\n", url.c_str());
    size_t after_protocol = url.find_first_of("//");
    if (after_protocol == std::string::npos)
        return;
    size_t domain_end = url.substr(after_protocol+2).find_first_of("/") + after_protocol+2;
    std::string only_domain = domain_end != std::string::npos ? url.substr(0, domain_end) : url;
    auto favicon = only_domain + "/favicon.ico";
    printf("favicon: '%s'\n", favicon.c_str());
    wai.name = only_domain.substr(after_protocol+2);
    wai.icon_path = "/tmp/"+ only_domain.substr(after_protocol+2);
    auto t = std::thread(download_favicon, favicon, wai.icon_path, &wai);
    t.detach();
}

void add_wap() {
    deskentry::UnparsedEntry pe;
    pe["Desktop Entry"]["Type"] = "Application";
    pe["Desktop Entry"]["Icon"] = wai.icon_path;
    pe["Desktop Entry"]["Name"] = wai.name_entry->get_text().empty() ? wai.name : (std::string) wai.name_entry->get_text();
    pe["Desktop Entry"]["Exec"] = "firefox "+ wai.url_entry->get_text() +" --name deneme --profile /home/mustafa/Programming/kisayol-duzenleyici/obsolete/profiles/webapp-deneme1";
    deskentry::write_to_file(pe, "/home/mustafa/Desktop/"+pe["Desktop Entry"]["Name"]+".desktop", true);
}

void new_ui(Glib::RefPtr<Gtk::Builder> b) {
    wai.w = b->get_widget<Gtk::Window>("new_desktop_window");
    wai.add_button = b->get_widget<Gtk::Button>("wai_add_button");
    wai.url_entry = b->get_widget<Gtk::Entry>("wai_url_entry");
    wai.icon = b->get_widget<Gtk::Image>("wai_icon");
    wai.icon_stack = b->get_widget<Gtk::Stack>("icon_stack");
    wai.icon_spinner = b->get_widget<Gtk::Spinner>("wai_spinner");
    wai.name_entry = b->get_widget<Gtk::Entry>("wai_name_entry");

    wai.url_entry->signal_changed().connect(sigc::ptr_fun(url_changed));
    wai.add_button->signal_clicked().connect(sigc::ptr_fun(add_wap));
}

