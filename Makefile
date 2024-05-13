.PHONY: all
all: ui main
PKGS=gtkmm-4.0
CFLAGS=-Wall -ggdb -std=c++20 `pkg-config --cflags $(PKGS)` -O2 -fno-omit-frame-pointer # -fsanitize=address -fno-common
LIBS=`pkg-config --libs $(PKGS)`

ui: main.blp
	blueprint-compiler compile main.blp --output dizelge.ui

main: src/*
	g++ src/main.cpp -o dizelge $(CFLAGS) $(LIBS)
