CC=g++
FONTDIR=$(DESTDIR)/usr/share/gluqlo
XSCREENSAVERDIRS=/usr/libexec/xscreensaver /usr/lib/xscreensaver
SXTWL_SOURCES=third_party/sxtwl_cpp/src/JD.cpp third_party/sxtwl_cpp/src/SSQ.cpp third_party/sxtwl_cpp/src/XL.cpp third_party/sxtwl_cpp/src/day.cpp third_party/sxtwl_cpp/src/eph.cpp third_party/sxtwl_cpp/src/sxtwl.cpp
SEASONAL_SOURCES=gluqlo_seasonal.cpp $(SXTWL_SOURCES)
WARNING_FLAGS=-Wall -Wno-sign-compare -Wno-unused-variable -Wno-parentheses
CFLAGS=$(WARNING_FLAGS) -I. -Ithird_party/sxtwl_cpp/src -o gluqlo gluqlo.c $(SEASONAL_SOURCES) `sdl-config --libs --cflags` -DFONT='"$(FONTDIR)/gluqlo.ttf"'
LDFLAGS=-lX11 -lXinerama -lSDL_ttf -lSDL_gfx

all: gluqlo

gluqlo: gluqlo.c gluqlo_seasonal.cpp gluqlo_seasonal.h $(SXTWL_SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS)

test: tests/geometry_test
	tests/geometry_test
	tests/install_targets_test.sh

integration-test: all
	tests/xscreensaver_position_test.sh

tests/geometry_test: tests/geometry_test.c gluqlo.c gluqlo_seasonal.cpp gluqlo_seasonal.h
	$(CC) $(WARNING_FLAGS) -I. -Ithird_party/sxtwl_cpp/src -DGLUQLO_NO_MAIN -o tests/geometry_test tests/geometry_test.c gluqlo.c $(SEASONAL_SOURCES) `sdl-config --libs --cflags` $(LDFLAGS)

print-install-targets:
	@for dir in $(XSCREENSAVERDIRS); do echo "$$dir/gluqlo"; done

install:
	strip gluqlo
	for dir in $(XSCREENSAVERDIRS); do install -o root -m 0755 -D gluqlo $(DESTDIR)$$dir/gluqlo; done
	install -o root -m 0644 -D gluqlo.ttf $(FONTDIR)/gluqlo.ttf
	install -o root -m 0644 -D gluqlo.png $(DESTDIR)/usr/share/pixmaps/gluqlo.png
	install -o root -m 0644 -D gluqlo.xml $(DESTDIR)/usr/share/xscreensaver/config/gluqlo.xml
	install -o root -m 0644 -D gluqlo.desktop $(DESTDIR)/usr/share/applications/screensavers/gluqlo.desktop

uninstall:
	rm -f $(DESTDIR)/usr/share/xscreensaver/config/gluqlo.xml $(DESTDIR)/usr/share/applications/screensavers/gluqlo.desktop \
		$(FONTDIR)/gluqlo.ttf $(DESTDIR)/usr/share/pixmaps/gluqlo.png
	for dir in $(XSCREENSAVERDIRS); do rm -f $(DESTDIR)$$dir/gluqlo; done

clean:
	rm -f gluqlo tests/geometry_test
