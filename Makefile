APP_NAME  := rvkernel-manager
BIN_DIR   := bin
BUILD_DIR := $(BIN_DIR)/build
DATA_DIR  := data

PREFIX    ?= /usr/local
BINDIR    := $(DESTDIR)$(PREFIX)/bin
DATADIR   := $(DESTDIR)$(PREFIX)/share

PKGS      := gtk4

CC        ?= gcc
CFLAGS    ?= -O2
CFLAGS    += -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wno-unused-parameter \
             $(shell pkg-config --cflags $(PKGS))
LDLIBS    := $(shell pkg-config --libs $(PKGS)) -lm

SRCS      := $(shell find src -name '*.c' | sort)
OBJS      := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS      := $(OBJS:.o=.d)

.PHONY: all run clean install uninstall

all: $(BIN_DIR)/$(APP_NAME)

$(BIN_DIR)/$(APP_NAME): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

run: all
	./$(BIN_DIR)/$(APP_NAME)

clean:
	rm -rf $(BIN_DIR)

install: all
	install -Dm755 $(BIN_DIR)/$(APP_NAME) $(BINDIR)/$(APP_NAME)
	install -Dm644 $(DATA_DIR)/com.rve.RvKernelManager.desktop $(DATADIR)/applications/com.rve.RvKernelManager.desktop
	install -Dm644 $(DATA_DIR)/icons/com.rve.RvKernelManager.svg $(DATADIR)/icons/hicolor/scalable/apps/com.rve.RvKernelManager.svg

uninstall:
	rm -f $(BINDIR)/$(APP_NAME)
	rm -f  $(DATADIR)/applications/com.rve.RvKernelManager.desktop
	rm -f  $(DATADIR)/icons/hicolor/scalable/apps/com.rve.RvKernelManager.svg

-include $(DEPS)
