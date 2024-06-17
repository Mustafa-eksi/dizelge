.PHONY: all
all: ui main
PKGS=gtkmm-4.0
CFLAGS=-Wall -ggdb -std=c++20 `pkg-config --cflags $(PKGS)` -O2 -fno-omit-frame-pointer # -fsanitize=address -fno-common
LIBS=`pkg-config --libs $(PKGS)`
prefix = /usr

ui: main.blp
	blueprint-compiler compile main.blp --output dizelge.ui

main: src/*
	clang++ src/main.cpp -o dizelge $(CFLAGS) $(LIBS)

install: dizelge dizelge.ui
	install dizelge $(DESTDIR)$(prefix)/bin
	mkdir -p $(DESTDIR)$(prefix)/share/dizelge
	install dizelge.ui $(DESTDIR)$(prefix)/share/dizelge
	install -d profile_template $(DESTDIR)$(prefix)/share/dizelge
