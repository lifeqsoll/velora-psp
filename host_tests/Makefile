CC ?= gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -I../shell/include -DHOST_TEST
LDFLAGS = -lm

SRCS = \
	../shell/src/path/path.c \
	../shell/src/theme/theme.c \
	../shell/src/input/input.c \
	../shell/src/library/library.c \
	../shell/src/files/files.c

.PHONY: all test clean

all: test_core

test_core: test_core.c $(SRCS)
	$(CC) $(CFLAGS) -o $@ test_core.c $(SRCS) $(LDFLAGS)

test: test_core
	./test_core

clean:
	rm -f test_core
