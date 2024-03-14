CC := gcc
DEPS := libadwaita-1 \
		upower-glib \
		wireplumber-0.4 \
		json-glib-1.0 \
		libnm \
		libpulse \
		libpulse-simple \
		libpulse-mainloop-glib \
		wayland-client \
		wayland-protocols
CFLAGS := $(shell pkg-config --cflags $(DEPS)) -g3 -Wall
# order is important here, if libwayland is linked before gtk4 layer shell
# bad things happen.
LIBS := "-lgtk4-layer-shell" "-lm"
LIBS += $(shell pkg-config --libs $(DEPS))
SOURCES := $(shell find src/ -type f -name *.c)
OBJS := $(patsubst %.c, %.o, $(SOURCES))
OBJS += lib/cmd_tree/cmd_tree.o

all: gresources way-shell lib/cmd_tree/cmd_tree.o way-sh/way-sh

install: all gschema

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

.PHONY:
wayland-protocols:
	wayland-scanner client-header ./lib/wlr-protocols/unstable/wlr-output-management-unstable-v1.xml ./src/services/wayland_service/wlr-output-management-unstable-v1.h
	wayland-scanner private-code ./lib/wlr-protocols/unstable/wlr-output-management-unstable-v1.xml ./src/services/wayland_service/wlr-output-management-unstable-v1.c

.PHONY:
install:
	sudo rm -rf /usr/local/bin/{way-shell,way-sh}
	sudo cp ./way-shell /usr/local/bin
	sudo cp ./way-sh/way-sh /usr/local/bin

clean:
	find . -name "*.o" -type f -exec rm -f {} \;
	rm -rf way-shell
	make -C way-sh/ clean
