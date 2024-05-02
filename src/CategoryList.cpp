#pragma once
#include <gtkmm.h>
#include <memory>
#include <utility>

#include "ApplicatonList.cpp"
#include "Common.cpp"

typedef struct {
        CategoryEntries catlist;
	std::shared_ptr<ListviewList> category_views;
	std::shared_ptr<ModelList> models;
	std::shared_ptr<FactoryList> factories;
} CategoryView;

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


static void lui_bind(const Glib::RefPtr<Gtk::ListItem>& list_item,
                     CategoryView catview,
                     EntryList main_list,
                     void (*catlist_button_clicked)(const Glib::RefPtr<Gtk::ListItem>& list_item, std::string list_ind, std::string appname)) {
	auto lb = dynamic_cast<Gtk::Expander *>(list_item.get()->get_child());
	Glib::ustring strobj = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj())));
	lb->set_label(strobj.c_str());
	auto attrs = Pango::AttrList::from_string("0 -1 font \"Sans 12\"");
	auto label = dynamic_cast<Gtk::Label*>(lb->get_label_widget());
	label->set_attributes(attrs);
	auto gsl = Gtk::StringList::create();
	for (auto const& [name, ind] : catview.catlist[strobj]) {
		/*if (!kapp.etxt->get_text().empty()) {
			if (!insensitive_search(kapp.etxt->get_text().c_str(), name))
				continue;
		}*/
		gsl->append(name);
	}
        catview.models->insert(std::make_pair(strobj, Gtk::SingleSelection::create()));
	catview.models->at(strobj)->set_autoselect(false);
	catview.models->at(strobj)->set_model(gsl);
	catview.factories->insert(std::make_pair(strobj, Gtk::SignalListItemFactory::create()));
	catview.factories->at(strobj)->signal_setup().connect(sigc::ptr_fun(cat_setup));
	catview.factories->at(strobj)->signal_bind().connect(std::bind(cat_bind, std::placeholders::_1, strobj, main_list, catview.catlist, catlist_button_clicked));
	catview.factories->at(strobj)->signal_unbind().connect(std::bind(cat_unbind, std::placeholders::_1, strobj));
	catview.factories->at(strobj)->signal_teardown().connect(sigc::ptr_fun(cat_teardown));
	catview.category_views->insert(std::make_pair(strobj, new Gtk::ListView()));
	catview.category_views->at(strobj)->set_model(catview.models->at(strobj));
	catview.category_views->at(strobj)->set_factory(catview.factories->at(strobj));
	catview.category_views->at(strobj)->set_single_click_activate(false);

	lb->set_child(*dynamic_cast<Gtk::Widget*>(catview.category_views->at(strobj)));
	//set_from_desktop_entry(&kapp.eui, kapp.eui.de);
}

void lui_unbind(const Glib::RefPtr<Gtk::ListItem> &li, std::shared_ptr<ListviewList> lists) {
	auto lb = dynamic_cast<Gtk::Expander *>(li.get()->get_child());
	Glib::ustring strobj = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item (li->gobj())));
	lb->unset_child();
	delete lb;
	delete lists->at(strobj);
}