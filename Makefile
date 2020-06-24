CC=gcc
CFLAGS = -o
TARGETS = myshell
all: $(TARGETS)
.PHONY: all
%:
	$(CC) -o $@ $@.c
clean:
	rm $(TARGETS)
