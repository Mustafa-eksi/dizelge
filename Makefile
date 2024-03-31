.PHONY: all
all: ui main
PKGS=gtkmm-4.0
CFLAGS=-Wall -ggdb -std=c++20 `pkg-config --cflags $(PKGS)`
LIBS=`pkg-config --libs $(PKGS)`

ui: main.blp
	blueprint-compiler compile main.blp --output kisa.ui

main: main.cpp desktop.cpp
	g++ main.cpp -o main $(CFLAGS) $(LIBS)
