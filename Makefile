.PHONY: all
all: ui main generate_pot update_pos generate_mos
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

install: dizelge dizelge.ui translations/* profile_template/*
	install dizelge $(DESTDIR)$(prefix)/bin
	install -D -t $(SHARE_DIR) dizelge.ui
	install -D -t $(SHARE_DIR)/profile_template profile_template/user.js
	install -D -t $(SHARE_DIR)/profile_template/chrome profile_template/chrome/userChrome.css
	install Dizelge.desktop $(DESTDIR)$(prefix)/share/applications/me.mustafa.dizelge.desktop
	# TODO: Automate translations
	install -d $(LOCALE_DIR)/tr/LC_MESSAGES
	install translations/tr.mo $(LOCALE_DIR)/tr/LC_MESSAGES/dizelge.mo

generate_pot: src/*
	xgettext --from-code=UTF-8 --add-comments --keyword=_ --keyword=C_:1c,2 main.blp src/main.cpp src/WebApp.cpp src/EntryUi.cpp src/desktop.cpp -o ./translations/dizelge.pot

update_pos: translations/dizelge.pot
	msgmerge -o translations/tr.po translations/tr.po translations/dizelge.pot

generate_mos: translations/*.po
	msgfmt translations/tr.po -o translations/tr.mo

uninstall:
	rm $(DESTDIR)$(prefix)/bin/dizelge
	rm -rf $(SHARE_DIR)
	rm $(DESTDIR)$(prefix)/share/applications/me.mustafa.dizelge.desktop
	# TODO: Automate translations
	rm $(LOCALE_DIR)/tr/LC_MESSAGES/dizelge.mo
