all:
	gtk-builder-convert xbmd_app.glade xbmd_app.xml
	g++ -Wall -g -o xbmd_app xbmd_main.cpp `pkg-config --cflags --libs --cflags gtk+-2.0 --libs --cflags libglade-2.0` -export-dynamic
clean:
	rm -f xbmd_app xbmd_app.xml
