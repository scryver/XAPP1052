all:
	g++ -Wall -g -o xbmd-direct direct.cpp -Wno-write-strings -Wno-unused-function
	g++ -Wall -g -o xbmd-cli cli.cpp -Wno-write-strings -Wno-unused-function
	gtk-builder-convert xbmd_app.glade xbmd_app.xml
	g++ -Wall -g -o xbmd-app xbmd_main.cpp `pkg-config --cflags --libs --cflags gtk+-2.0 --libs --cflags libglade-2.0` -export-dynamic

clean:
	rm -f xbmd-app xbmd_app.xml
	rm -f xbmd-cli
