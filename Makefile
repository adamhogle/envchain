UNAME = $(shell uname 2>/dev/null || echo Windows_NT)
IS_WSL = $(shell sh -c 'uname -r 2>/dev/null | grep -qi microsoft && echo 1 || echo 0')
ENVCHAIN_FORCE_WSL ?= 0
VERSION ?= 1.1.0

CFLAGS += -Wall -Wextra
CPPFLAGS += -DENVCHAIN_VERSION=\"$(VERSION)\"

ifeq ($(OS),Windows_NT)
  # Windows build (MinGW / MSYS2)
  CFLAGS += -std=c99
  LIBS    = -ladvapi32
  OBJS    = envchain.o envchain_windows.o
  EXE     = envchain.exe
else ifeq ($(UNAME), Darwin)
  CFLAGS += -ansi -pedantic -std=c99 -mmacosx-version-min=10.7
  LIBS    = -ledit -ltermcap -framework Security -framework CoreFoundation
  OBJS    = envchain.o envchain_osx.o
  EXE     = envchain
else ifeq ($(ENVCHAIN_FORCE_WSL),1)
  # Forced WSL backend build (used by CI).
  CFLAGS += -ansi -pedantic -std=c99 -DENVCHAIN_NO_READLINE
  LIBS    =
  OBJS    = envchain.o envchain_wsl.o
  EXE     = envchain
else ifeq ($(IS_WSL),1)
  # WSL build: delegate secret operations to Windows envchain.exe
  CFLAGS += -ansi -pedantic -std=c99 -DENVCHAIN_NO_READLINE
  LIBS    =
  OBJS    = envchain.o envchain_wsl.o
  EXE     = envchain
else
  LIBSECRET_CFLAGS = $(shell sh -c 'command -v pkg-config >/dev/null 2>&1 && pkg-config --cflags libsecret-1 || true')
  LIBSECRET_LIBS   = $(shell sh -c 'command -v pkg-config >/dev/null 2>&1 && pkg-config --libs libsecret-1 || true')
  CFLAGS += -ansi -pedantic -std=c99
  LIBS    = -lreadline $(LIBSECRET_LIBS)
  OBJS    = envchain.o envchain_linux.o
  EXE     = envchain
endif

DESTDIR ?= /usr

all: $(EXE)
$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) -o $(EXE) $(OBJS) $(LIBS)

%.o: %.c envchain.h
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

envchain_linux.o: CFLAGS += $(LIBSECRET_CFLAGS)

clean:
	rm -f $(EXE) $(OBJS)

install: all
	install -d $(DESTDIR)/./bin
	install -m755 ./$(EXE) $(DESTDIR)/./bin/envchain
