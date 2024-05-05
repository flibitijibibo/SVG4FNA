# Makefile for SVG4FNA Native Library
# Written by Ethan "flibitijibibo" Lee

# Detect cross targets
TRIPLET=$(shell $(CC) -dumpmachine)
WINDOWS_TARGET=0
APPLE_TARGET=0
ifeq ($(OS), Windows_NT) # cygwin/msys2
	WINDOWS_TARGET=1
endif
ifneq (,$(findstring w64-mingw32,$(TRIPLET)))
	WINDOWS_TARGET=1
endif
ifneq (,$(findstring w64-windows,$(TRIPLET)))
	WINDOWS_TARGET=1
endif
ifneq (,$(findstring apple-darwin,$(TRIPLET)))
	APPLE_TARGET=1
endif

# Compiler
ifeq ($(WINDOWS_TARGET),1)
	TARGET = dll
	LDFLAGS += -static-libgcc
else ifeq ($(APPLE_TARGET),1)
	CC += -mmacosx-version-min=10.9
	TARGET = dylib
	CFLAGS += -fpic -fPIC
	LDFLAGS += -install_name @rpath/libsvg4fna.dylib
else
	TARGET = so
	CFLAGS += -fpic -fPIC
endif

LIB = libsvg4fna.$(TARGET)

all:
	$(CC) -O3 $(CFLAGS) $(LDFLAGS) -shared -o $(LIB) svg4fna.c `sdl2-config --cflags --libs`

debug:
	$(CC) -g $(CFLAGS) $(LDFLAGS) -shared -o $(LIB) svg4fna.c `sdl2-config --cflags --libs`

example:
	$(CC) -g $(CFLAGS) $(LDFLAGS) -o example_native example_native.c `sdl2-config --cflags --libs`

clean:
	rm -f libsvg4fna.* example_native
