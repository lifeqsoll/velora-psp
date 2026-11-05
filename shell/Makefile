TARGET = OwnShell
VERSION = 0.1.0-m1

OBJS = \
	src/main_psp.o \
	src/ui/ui.o \
	src/theme/theme.o \
	src/theme/theme_persist.o \
	src/input/input.o \
	src/library/library.o \
	src/files/files.o \
	src/files/files_psp.o \
	src/path/path.o \
	src/font/font.o

# Directories only — build.mak adds -I prefixes. Do NOT put -I$(INCDIR) in CFLAGS.
INCDIR = include
CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LIBS = -lpspgum -lpspgu -lm -lpsppower
LDFLAGS =

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = OwnShell M1

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

.PHONY: show-version
show-version:
	@echo $(VERSION)
