CC := gcc
CFLAGS := $(shell pkg-config --cflags libadwaita-1) -g3 -Wall
LIBS := $(shell pkg-config --libs libadwaita-1)
LIBS += "-lgtk4-layer-shell"
OBJS := ./src/panel/panel.o

gnomeland: ./src/main.o $(OBJS)
	$(CC) $(CFLAGS) -o $@ ./src/main.o $(OBJS) $(LIBS)

./src/main.o: $(OBJS)

clean:
	find . -name "*.o" -type f -exec rm -f {} \;
	rm -rf gnomeland