CC ?= cc
CFLAGS ?= -std=c11 -Os -Wall -Wextra -pedantic
LDFLAGS ?=

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

TARGET := embtop
SRCS := src/main.c src/utils.c src/system.c src/process.c src/render.c

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(SRCS) include/embtop.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRCS) -Iinclude

install: $(TARGET)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 0755 "$(TARGET)" "$(DESTDIR)$(BINDIR)/$(TARGET)"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(TARGET)"

clean:
	rm -f "$(TARGET)"
