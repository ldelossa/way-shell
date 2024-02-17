CC = gcc
CFLAGS = -g3 -Wall
SOURCES = $(shell find -type f -regex ".*\.c")
OBJS = $(subst .c,.o,$(SOURCES))
OBJS += ../lib/cmd_tree/cmd_tree.o

wlr-sh: $(OBJS)
	$(CC) $(CFLAGS) -o wlr-sh $(OBJS)

clean:
	rm -rf wlr-sh
	rm -rf $(OBJS)