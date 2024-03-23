PKGS=gtkmm-4.0
CFLAGS=-Wall -ggdb -std=c++20 `pkg-config --cflags $(PKGS)`
LIBS=`pkg-config --libs $(PKGS)`

main: main.cpp main.blp
	blueprint-compiler compile main.blp --output kisa.ui
	g++ main.cpp -o main $(CFLAGS) $(LIBS)
