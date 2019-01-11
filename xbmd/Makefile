obj-m := xbmd.o

all:
	gtk-builder-convert xbmd_app.glade xbmd_app.xml
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)  modules
	g++ -Wall -g -o xbmd_app cfg.cpp bmd.cpp xbmd_ep.cpp xbmd_main.cpp `pkg-config --cflags --libs --cflags gtk+-2.0 --libs --cflags libglade-2.0` -export-dynamic
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f xbmd_app
