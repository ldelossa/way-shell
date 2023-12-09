CC := gcc
DEPS := libadwaita-1 \
		upower-glib \
		wireplumber-0.4
CFLAGS := $(shell pkg-config --cflags $(DEPS)) -g3 -Wall
LIBS := $(shell pkg-config --libs $(DEPS))
LIBS += "-lgtk4-layer-shell" "-lm"
SOURCES := $(shell find src/ -type f -name *.c)
OBJS := $(patsubst %.c, %.o, $(SOURCES))

gnomeland: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	find . -name "*.o" -type f -exec rm -f {} \;
	rm -rf gnomeland