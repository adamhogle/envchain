UNAME = $(shell uname 2>/dev/null || echo Windows_NT)

CFLAGS += -Wall -Wextra

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
else
  CFLAGS += -ansi -pedantic -std=c99 `pkg-config --cflags libsecret-1`
  LIBS    = -lreadline `pkg-config --libs libsecret-1`
  OBJS    = envchain.o envchain_linux.o
  EXE     = envchain
endif

DESTDIR ?= /usr

all: $(EXE)
$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) -o $(EXE) $(OBJS) $(LIBS)

%.o: %.c envchain.h
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	rm -f envchain envchain.exe $(OBJS)

install: all
	install -d $(DESTDIR)/./bin
	install -m755 ./$(EXE) $(DESTDIR)/./bin/envchain
