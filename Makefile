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
OBJS += lib/cmd_tree/cmd_tree.o

all: way-shell lib/cmd_tree/cmd_tree.o gschema way-sh

way-shell: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

lib/cmd_tree/cmd_tree.o: lib/cmd_tree/cmd_tree.c

lib/cmd_tree/cmd_tree.c:
	make -C lib/cmd_tree

.PHONY:
gschema:
	sudo cp data/org.ldelossa.way-shell.gschema.xml /usr/share/glib-2.0/schemas/
	sudo glib-compile-schemas /usr/share/glib-2.0/schemas/

.PHONY:
way-sh:
	make -C way-sh/

.PHONY:
dbus-codegen:
	gdbus-codegen --generate-c-code logind_manager_dbus \
	--c-namespace Dbus \
	--interface-prefix org.freedesktop. \
	--output-directory ./src/services/logind_service \
	./data/dbus-interfaces/org.freedesktop.login1.Manager.xml

	gdbus-codegen --generate-c-code logind_session_dbus \
	--c-namespace Dbus \
	--interface-prefix org.freedesktop. \
	--output-directory ./src/services/logind_service \
	./data/dbus-interfaces/org.freedesktop.login1.Session.xml

clean:
	find . -name "*.o" -type f -exec rm -f {} \;
	rm -rf way-shell
	make -C way-sh/ clean
