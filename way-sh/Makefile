CC = gcc
CFLAGS = -g3 -Wall
SOURCES = $(shell find -type f -regex ".*\.c")
OBJS = $(subst .c,.o,$(SOURCES))
OBJS += ../lib/cmd_tree/cmd_tree.o

way-sh: $(OBJS)
	$(CC) $(CFLAGS) -o way-sh $(OBJS)

clean:
	rm -rf way-sh
	rm -rf $(OBJS)
