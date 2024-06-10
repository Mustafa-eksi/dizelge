#pragma once
#include <gtkmm.h>
#include <memory>

#include "ApplicatonList.cpp"
#include "Common.cpp"

/*
 * This file contains only signals of the category list ListView.
 */

typedef struct {
	CategoryEntries catlist;
	std::shared_ptr<ListviewList> category_views;
	std::shared_ptr<ModelList> models;
	std::shared_ptr<FactoryList> factories;
	std::string previous_cat;
	CatSignalMap csm;
} CategoryView;

void categorylist_setup(const Glib::RefPtr<Gtk::ListItem>& list_item, bool expand) {
	GtkWidget *exp = gtk_expander_new (NULL);
	gtk_expander_set_expanded(GTK_EXPANDER(exp), expand);
  	gtk_list_item_set_child (list_item->gobj(), exp);
}

void categorylist_teardown(const Glib::RefPtr<Gtk::ListItem> &li) {
	auto c = dynamic_cast<Gtk::Expander*>(li->get_child());
	li->unset_child();
	delete c;
}

void categorylist_bind(const Glib::RefPtr<Gtk::ListItem>& list_item,
                     CategoryView *catview,
                     EntryList *main_list,
                     ListSelectionCallback callback) {
	auto lb = dynamic_cast<Gtk::Expander *>(list_item.get()->get_child());
	Glib::ustring strobj = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item (list_item->gobj())));
	lb->set_label(strobj.c_str());
	auto attrs = Pango::AttrList::from_string("0 -1 font \"Sans 12\"");
	auto label = dynamic_cast<Gtk::Label*>(lb->get_label_widget());
	label->set_attributes(attrs);
	auto gsl = Gtk::StringList::create();
	for (auto const& [name, ind] : catview->catlist[strobj])
		gsl->append(name);

	catview->models->insert_or_assign(strobj, Gtk::SingleSelection::create());
	catview->models->at(strobj)->set_autoselect(false);
	catview->models->at(strobj)->set_model(gsl);
	catview->models->at(strobj)->signal_selection_changed().connect(sigc::bind(callback, catview->models->at(strobj), true, true, strobj));

	(*catview->factories.get())[strobj] = Gtk::SignalListItemFactory::create();
	(*catview->factories.get())[strobj]->signal_setup().connect(sigc::ptr_fun(applist_setup));
	(*catview->factories.get())[strobj]->signal_bind().connect(std::bind(applist_bind, std::placeholders::_1, main_list, &(catview->catlist[strobj]), true, nullptr));
	(*catview->factories.get())[strobj]->signal_unbind().connect(std::bind(applist_unbind, std::placeholders::_1));
	(*catview->factories.get())[strobj]->signal_teardown().connect(sigc::ptr_fun(applist_teardown));

	(*catview->category_views.get())[strobj] = new Gtk::ListView();
	if (!catview->category_views->contains(strobj) || !catview->models->contains(strobj))
		return;
	(*catview->category_views.get())[strobj]->set_model(catview->models->at(strobj));
	(*catview->category_views.get())[strobj]->set_factory(catview->factories->at(strobj));
	(*catview->category_views.get())[strobj]->set_single_click_activate(false);

	lb->set_child(*dynamic_cast<Gtk::Widget*>(catview->category_views->at(strobj)));
}

void categorylist_unbind(const Glib::RefPtr<Gtk::ListItem> &list_item, std::shared_ptr<ListviewList> lists, CategoryView *catview) {
	auto lb = dynamic_cast<Gtk::Expander *>(list_item.get()->get_child());
	auto strobj = lb->get_label();
	lb->set_label("");
    //lb->unset_child();
	//delete catview->category_views->at(strobj);
}
