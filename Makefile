.PHONY: all
all: ui main
PKGS=gtkmm-4.0
CFLAGS=-Wall -ggdb -std=c++20 `pkg-config --cflags $(PKGS)` -O2 -fno-omit-frame-pointer # -fsanitize=address -fno-common
LIBS=`pkg-config --libs $(PKGS)`
prefix = /usr
SHARE_DIR=$(DESTDIR)$(prefix)/share/dizelge
LOCALE_DIR=$(DESTDIR)$(prefix)/share/locale

ui: main.blp
	blueprint-compiler compile main.blp --output dizelge.ui

main: src/*
	g++ src/main.cpp -o dizelge $(CFLAGS) $(LIBS)

install:
	install dizelge $(DESTDIR)$(prefix)/bin
	install -d $(SHARE_DIR)
	install dizelge.ui $(SHARE_DIR)
	install -d $(SHARE_DIR)/profile_template
	install profile_template/user.js $(SHARE_DIR)/profile_template
	install -d $(SHARE_DIR)/profile_template/chrome
	install profile_template/chrome/userChrome.css $(SHARE_DIR)/profile_template/chrome
	install Dizelge.desktop $(DESTDIR)$(prefix)/share/applications
	# TODO: Automate translations
	install -d $(LOCALE_DIR)/tr/LC_MESSAGES/
	install translations/tr.mo $(LOCALE_DIR)/tr/LC_MESSAGES/dizelge.mo

generate_pot:
	xgettext --from-code=UTF-8 --add-comments --keyword=_ --keyword=C_:1c,2 main.blp src/main.cpp src/WebApp.cpp src/EntryUi.cpp src/desktop.cpp -o ./translations/dizelge.pot

update_pos: translations/dizelge.pot
	msgmerge -o translations/tr.po translations/tr.po translations/dizelge.pot

generate_mos:
	msgfmt translations/tr.po -o translations/tr.mo
