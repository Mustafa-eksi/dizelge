#pragma once
#include <gtkmm.h>
#include <optional>
#include <thread>
#include <fstream>
#include "desktop.cpp"
#include "GettextMacros.h"
/*
 * This file contains only the window that opens when "new web app" button is pressed.
 */


const std::string DATA_FOLDER = "/dizelge";
const std::string PROFILES_FOLDER = "/dizelge/profiles";
const std::string ICONS_FOLDER = "/dizelge/icons";
const std::string PROFILE_TEMPLATE_FOLDER = "/usr/share/dizelge/profile_template";
const std::string APPLICATIONS_FOLDER = "/.local/share/applications";

struct WebAppUi {
    Gtk::Window *w;
    Gtk::Button *add_button, *get_favicon_button;
    sigc::connection add_button_connection, get_favicon_button_connection;
    Gtk::Entry *url_entry, *name_entry;
    Gtk::Image *icon;
    Gtk::Stack *icon_stack;
    Gtk::Spinner *icon_spinner;
    std::string name, icon_path;
    GtkWidget *ad, *unfilled;
};

// Variables that are same for every window.
std::string data_home, applications_home, desktopd;

WebAppUi wai = {0};

bool check_or_create_dir(std::string path) {
    if(access(path.c_str(), F_OK) != 0 && mkdir(path.c_str(), 0755) != 0)
        return false;

    if(access(path.c_str(), W_OK) != 0) {
        printf(_("ERROR: Couldn't access %s, manual intervention needed.\n"), path.c_str());
        return false;
    }
    return true;
}

std::optional<std::string> test_runtime_folders() {
    auto xdg_data_home = std::getenv("XDG_DATA_HOME");
    auto xdg_home = std::getenv("HOME");
    if (xdg_data_home == NULL) {
        if (xdg_home != NULL)
            data_home = (std::string) xdg_home + "/.local/share";
        else
            return {};
    } else {
        data_home = (std::string) xdg_data_home;
    }
    if (!check_or_create_dir(data_home+DATA_FOLDER+"/"))
        return {};
    if (!check_or_create_dir(data_home+PROFILES_FOLDER))
        return {};
    if (!check_or_create_dir(data_home+ICONS_FOLDER))
        return {};
    if (xdg_home != NULL && !check_or_create_dir((std::string)xdg_home+APPLICATIONS_FOLDER))
        return {};
    else
        applications_home = (std::string)xdg_home+APPLICATIONS_FOLDER;
    return data_home;
}

void download_favicon(std::string fv, std::string output="/tmp/wap-favicon", WebAppUi *wai=NULL) {
    wai->icon_stack->set_visible_child(*wai->icon_spinner);
    wai->icon_spinner->start();

    // v this is the heavy task.
    system(("wget -q -O " + output + " " + fv).c_str());

    if (access(output.c_str(), F_OK) != 0)
        wai->icon->set_from_icon_name("emblem-web");
    else
        gtk_image_set_from_file(wai->icon->gobj(), output.c_str());
    wai->icon->set_icon_size(Gtk::IconSize::LARGE);

    wai->icon_spinner->stop();
    wai->icon_stack->set_visible_child(*wai->icon);
}

std::optional<std::string> parse_url() {
    auto url = wai.url_entry->get_text();

    size_t after_protocol = url.find_first_of("//");
    if (after_protocol == std::string::npos || after_protocol+3 > url.size())
        return {};
    auto domain_end_pos = url.substr(after_protocol+2).find_first_of("/");
    std::string only_domain = domain_end_pos != std::string::npos ? url.substr(0, domain_end_pos + after_protocol+2) : url;
    // Check if domain is valid
    if (only_domain.find_first_of(".") == std::string::npos)
        return {};
    wai.name = only_domain.substr(after_protocol+2);
    wai.icon_path = data_home + ICONS_FOLDER +"/"+ only_domain.substr(after_protocol+2);
    return only_domain;
}

void get_favicon() {
    auto parsed = parse_url();
    if (!parsed.has_value())
        return;
    auto favicon = parsed.value() + "/favicon.ico";
    // printf("favicon: '%s'\n", favicon.c_str());
    auto t = std::thread(download_favicon, favicon, wai.icon_path, &wai);
    t.detach();
}

std::optional<std::string> generate_profile(std::string name) {
    std::string full_path = data_home+PROFILES_FOLDER+"/"+name;
    if (!check_or_create_dir(data_home+PROFILES_FOLDER+"/"+name))
        return {};
    if (!check_or_create_dir(data_home+PROFILES_FOLDER+"/"+name+"/chrome"))
        return {};

    std::ifstream user_js_template(PROFILE_TEMPLATE_FOLDER+"/user.js");
    std::ofstream user_js(full_path+"/user.js");
    user_js << user_js_template.rdbuf();

    std::ifstream user_chrome_template(PROFILE_TEMPLATE_FOLDER+"/chrome/userChrome.css");
    std::ofstream user_chrome(full_path+"/chrome/userChrome.css");
    user_chrome << user_chrome_template.rdbuf();

    return full_path;
}

void wap_added(GtkDialog* self, gint response_id, gpointer user_data) {
    // wai.ad->choose_finish(res);
    if (response_id == -4) return;
    gtk_window_close(GTK_WINDOW(self));
    wai.w->close();
}

void ok_buddy(GtkDialog* self, gint response_id, gpointer user_data) {
    // wai.ad->choose_finish(res);
    if (response_id == -4) return;
    gtk_window_close(GTK_WINDOW(wai.unfilled));
}

void add_wap() {
    if (wai.name_entry->get_text().empty() || wai.url_entry->get_text().empty()) {
        wai.unfilled = gtk_message_dialog_new(wai.w->gobj(), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, _("Every field needs to be filled!"));
        g_signal_connect(wai.unfilled, "response", G_CALLBACK(ok_buddy), NULL);
        gtk_window_present(GTK_WINDOW(wai.unfilled));
        return;
    }
    if (wai.icon_path.empty())
        get_favicon();

    deskentry::UnparsedEntry pe;
    pe["Desktop Entry"]["Type"] = "Application";
    pe["Desktop Entry"]["Icon"] = wai.icon_path;
    pe["Desktop Entry"]["Categories"] = "WebApps";
    pe["Desktop Entry"]["Name"] = wai.name_entry->get_text();
    auto new_prof = generate_profile(wai.name);
    pe["Desktop Entry"]["Exec"] = "sh -c 'XAPP_FORCE_GTKWINDOW_ICON=\""+wai.icon_path+"\" firefox "+ wai.url_entry->get_text() +" --name "+wai.name+" --profile " + (new_prof.has_value() ? new_prof.value() : PROFILE_TEMPLATE_FOLDER) + "'";
    if (applications_home.empty()) {
        // TODO: open a file dialog here
        if (std::getenv("HOME") == NULL)
            return;
        applications_home = desktopd;
    }
    deskentry::write_to_file(pe, applications_home+"/"+pe["Desktop Entry"]["Name"]+".desktop", true);
    wai.ad = gtk_message_dialog_new(wai.w->gobj(), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, _("Web app created successfully!"));
    g_signal_connect(wai.ad, "response", G_CALLBACK(wap_added), NULL);
    gtk_window_present(GTK_WINDOW(wai.ad));
}

bool close_wap() {
    wai.get_favicon_button_connection.disconnect();
    wai.add_button_connection.disconnect();
    // wai.w->hide();
    wai = {0};
    return false;
}

void new_ui(Glib::RefPtr<Gtk::Builder> b, std::string datah, std::string desktop_dir) {
    desktopd = desktop_dir;
    if (datah.empty()) {
        auto dh = test_runtime_folders();
        if (dh.has_value())
            data_home = dh.value();
        else
            printf(_("ERROR: Web app window failed to access needed folders."));
    } else {
        data_home = datah;
    }
    wai.w = b->get_widget<Gtk::Window>("new_desktop_window");
    wai.add_button = b->get_widget<Gtk::Button>("wai_add_button");
    wai.url_entry = b->get_widget<Gtk::Entry>("wai_url_entry");
    wai.icon = b->get_widget<Gtk::Image>("wai_icon");
    wai.icon_stack = b->get_widget<Gtk::Stack>("icon_stack");
    wai.icon_spinner = b->get_widget<Gtk::Spinner>("wai_spinner");
    wai.name_entry = b->get_widget<Gtk::Entry>("wai_name_entry");
    wai.get_favicon_button = b->get_widget<Gtk::Button>("get_favicon_button");

    wai.url_entry->set_text("");
    wai.icon->clear();
    wai.name_entry->set_text("");
    wai.icon_stack->set_visible_child(*wai.get_favicon_button);

    wai.add_button_connection = wai.add_button->signal_clicked().connect(sigc::ptr_fun(add_wap));
    wai.get_favicon_button_connection = wai.get_favicon_button->signal_clicked().connect(sigc::ptr_fun(get_favicon));
}

