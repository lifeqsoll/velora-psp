CC ?= gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -I../shell/include -DHOST_TEST
FT_CFLAGS := $(shell pkg-config --cflags freetype2 2>/dev/null)
FT_LIBS := $(shell pkg-config --libs freetype2 2>/dev/null)
LDFLAGS = -lm

SRCS = \
	../shell/src/path/path.c \
	../shell/src/theme/theme.c \
	../shell/src/input/input.c \
	../shell/src/library/library.c \
	../shell/src/files/files.c

.PHONY: all test clean

all: test_core test_font test_roundrect

test_core: test_core.c $(SRCS)
	$(CC) $(CFLAGS) -o $@ test_core.c $(SRCS) $(LDFLAGS)

test_font: test_font.c ../shell/src/font/font_ft.c ../shell/src/font/font_ttf_data.c
	$(CC) $(CFLAGS) $(FT_CFLAGS) -o $@ test_font.c ../shell/src/font/font_ft.c \
		../shell/src/font/font_ttf_data.c $(FT_LIBS) $(LDFLAGS)

test_roundrect: test_roundrect.c
	$(CC) $(CFLAGS) -o $@ test_roundrect.c $(LDFLAGS)

test: test_core test_font test_roundrect
	./test_core
	./test_font
	./test_roundrect

clean:
	rm -f test_core test_font test_roundrect
