#pragma once
#include "gtkmm/button.h"
#include "gtkmm/entry.h"
#include "sigc++/functors/ptr_fun.h"
#include <gtkmm.h>
#include <optional>
#include <thread>
#include <fstream>
#include "desktop.cpp"
/*
 * This file contains only the window that opens when "new web app" button is pressed.
 */


const std::string DATA_FOLDER = "/dizelge";
const std::string PROFILES_FOLDER = "/dizelge/profiles";
const std::string ICONS_FOLDER = "/dizelge/icons";
const std::string PROFILE_TEMPLATE_FOLDER = "/home/mustafa/Programming/kisayol-duzenleyici/obsolete/profiles/webapp-deneme1";

struct WebAppUi {
    Gtk::Window *w;
    Gtk::Button *add_button;
    Gtk::Entry *url_entry, *name_entry;
    Gtk::Image *icon;
    Gtk::Stack *icon_stack;
    Gtk::Spinner *icon_spinner;
    std::string name, icon_path, data_home;
};

WebAppUi wai = {0};

bool check_or_create_dir(std::string path) {
    if(access(path.c_str(), F_OK) != 0 && mkdir(path.c_str(), 0755) != 0)
        return false;

    if(access(path.c_str(), W_OK) != 0) {
        printf("ERROR: Couldn't access %s, manual intervention needed.\n", path.c_str());
        return false;
    }
    return true;
}

std::optional<std::string> test_runtime_folders() {
    auto xdg_data_home = std::getenv("XDG_DATA_HOME");
    auto xdg_home = std::getenv("HOME");
    if (xdg_data_home == NULL) {
        if (xdg_home != NULL)
            wai.data_home = (std::string) xdg_home + "/.local/share";
        else
            return {};
    } else {
        wai.data_home = (std::string) xdg_data_home;
    }
    if (!check_or_create_dir(wai.data_home+DATA_FOLDER+"/"))
        return {};
    if (!check_or_create_dir(wai.data_home+PROFILES_FOLDER))
        return {};
    if (!check_or_create_dir(wai.data_home+ICONS_FOLDER))
        return {};
    return wai.data_home;
}

void download_favicon(std::string fv, std::string output="/tmp/wap-favicon", WebAppUi *wai=NULL) {
    wai->icon_stack->set_visible_child(*wai->icon_spinner);
    wai->icon_spinner->start();

    // v this is the heavy task.
    system(("wget -q -O " + output + " " + fv).c_str());

    if (access(output.c_str(), F_OK) != 0)
        wai->icon->set_from_icon_name("internet-services");
    else
        gtk_image_set_from_file(wai->icon->gobj(), output.c_str());
    wai->icon->set_icon_size(Gtk::IconSize::LARGE);

    wai->icon_spinner->stop();
    wai->icon_stack->set_visible_child(*wai->icon);
}

void url_changed() {
    auto url = wai.url_entry->get_text();

    size_t after_protocol = url.find_first_of("//");
    if (after_protocol == std::string::npos)
        return;

    size_t domain_end = url.substr(after_protocol+2).find_first_of("/") + after_protocol+2;
    std::string only_domain = domain_end != std::string::npos ? url.substr(0, domain_end) : url;
    auto favicon = only_domain + "/favicon.ico";
    // printf("favicon: '%s'\n", favicon.c_str());

    wai.name = only_domain.substr(after_protocol+2);
    wai.icon_path = wai.data_home + ICONS_FOLDER +"/"+ only_domain.substr(after_protocol+2);

    auto t = std::thread(download_favicon, favicon, wai.icon_path, &wai);
    t.detach();
}

std::optional<std::string> generate_profile(std::string name) {
    std::string full_path = wai.data_home+PROFILES_FOLDER+"/"+name;
    if (!check_or_create_dir(wai.data_home+PROFILES_FOLDER+"/"+name))
        return {};
    if (!check_or_create_dir(wai.data_home+PROFILES_FOLDER+"/"+name+"/chrome"))
        return {};

    std::ifstream user_js_template(PROFILE_TEMPLATE_FOLDER+"/user.js");
    std::ofstream user_js(full_path+"/user.js");
    user_js << user_js_template.rdbuf();

    std::ifstream user_chrome_template(PROFILE_TEMPLATE_FOLDER+"/chrome/userChrome.css");
    std::ofstream user_chrome(full_path+"/chrome/userChrome.css");
    user_chrome << user_chrome_template.rdbuf();

    return full_path;
}

void add_wap() {
    deskentry::UnparsedEntry pe;
    pe["Desktop Entry"]["Type"] = "Application";
    pe["Desktop Entry"]["Icon"] = wai.icon_path;
    pe["Desktop Entry"]["Name"] = wai.name_entry->get_text().empty() ? wai.name : (std::string) wai.name_entry->get_text();
    auto new_prof = generate_profile(wai.name);
    pe["Desktop Entry"]["Exec"] = "firefox "+ wai.url_entry->get_text() +" --name deneme --profile " + (new_prof.has_value() ? new_prof.value() : PROFILE_TEMPLATE_FOLDER);
    deskentry::write_to_file(pe, "/home/mustafa/Desktop/"+pe["Desktop Entry"]["Name"]+".desktop", true);
}

void new_ui(Glib::RefPtr<Gtk::Builder> b, std::string data_home) {
    if (data_home.empty()) {
        auto dh = test_runtime_folders();
        if (dh.has_value())
            wai.data_home = dh.value();
        else
            printf("ERROR: Web app window failed to access needed folders.");
    } else {
        wai.data_home = data_home;
    }
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

