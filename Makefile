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

all: gresources way-shell lib/cmd_tree/cmd_tree.o way-sh/way-sh

way-shell: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

lib/cmd_tree/cmd_tree.o: lib/cmd_tree/cmd_tree.c

lib/cmd_tree/cmd_tree.c:
	make -C lib/cmd_tree

.PHONY:
gresources:
	glib-compile-resources --generate-source --target src/gresources.c gresources.xml
	glib-compile-resources --generate-header --target src/gresources.h gresources.xml

.PHONY:
gschema: ./data/org.ldelossa.way-shell.gschema.xml
	sudo cp data/org.ldelossa.way-shell.gschema.xml /usr/share/glib-2.0/schemas/
	sudo glib-compile-schemas /usr/share/glib-2.0/schemas/

.PHONY:
way-sh/way-sh:
	make -C way-sh/

.PHONY:
dbus-codegen:
	# logind
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

	# notifications
	gdbus-codegen --generate-c-code notifications_dbus \
	--c-namespace Dbus \
	--interface-prefix org.freedesktop. \
	--output-directory ./src/services/notifications_service \
	./data/dbus-interfaces/org.freedesktop.Notifications.xml

	# power profiles daemon
	gdbus-codegen --generate-c-code power_profiles_dbus \
	--c-namespace Dbus \
	--interface-prefix net.hadess. \
	--output-directory ./src/services/power_profiles_service \
	./data/dbus-interfaces/net.hadess.PowerProfiles.xml

clean:
	find . -name "*.o" -type f -exec rm -f {} \;
	rm -rf way-shell
	make -C way-sh/ clean
