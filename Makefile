CC := gcc
DEPS := libadwaita-1 \
		upower-glib \
		wireplumber-0.4 \
		json-glib-1.0 \
		libnm \
		libpulse \
		libpulse-simple \
		libpulse-mainloop-glib
CFLAGS := $(shell pkg-config --cflags $(DEPS)) -g3 -Wall
LIBS := $(shell pkg-config --libs $(DEPS))
LIBS += "-lgtk4-layer-shell" "-lm"
SOURCES := $(shell find src/ -type f -name *.c)
OBJS := $(patsubst %.c, %.o, $(SOURCES))

all: gnomeland data/gschemas.compiled

gnomeland: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

.PHONY:
gschema:
	sudo cp data/org.ldelossa.wlr-shell.gschema.xml /usr/share/glib-2.0/schemas/
	sudo glib-compile-schemas /usr/share/glib-2.0/schemas/

clean:
	find . -name "*.o" -type f -exec rm -f {} \;
	rm -rf gnomeland