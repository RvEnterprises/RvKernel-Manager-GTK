APP_NAME  := rvkernel-manager
BIN_DIR   := bin
BUILD_DIR := $(BIN_DIR)/build
DATA_DIR  := data

CONFIG     := .config
KCONFIG    := Kconfig
DEF_CONFIG := configs/config

PREFIX    ?= /usr/local
BINDIR    := $(DESTDIR)$(PREFIX)/bin
DATADIR   := $(DESTDIR)$(PREFIX)/share

PKGS      := gtk4

CONFIG_Y  = $(shell sed -n '/^CONFIG_[A-Za-z0-9_]*=y$$/p' $(CONFIG) \
              2>/dev/null)

# Compiler comes from the CC_GCC / CC_CLANG choice in .config. The
# expression is deferred, so it evaluates only once .config exists;
# an explicit CC= on the command line or in the environment wins.
CC_DEFAULT = $(if $(findstring CONFIG_CC_CLANG=y,$(CONFIG_Y)),clang,gcc)
ifeq ($(origin CC),default)
CC         = $(CC_DEFAULT)
endif

CCACHE_DEFAULT = $(if $(findstring CONFIG_CCACHE=y,$(CONFIG_Y)),ccache,)
CCACHE        ?= $(CCACHE_DEFAULT)

CFLAGS    ?= -O2
CFLAGS    += -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wno-unused-parameter \
             $(shell pkg-config --cflags $(PKGS))
CFLAGS    += $(patsubst %,-D%,$(CONFIG_Y))
LDLIBS    := $(shell pkg-config --libs $(PKGS)) -lm

ifeq ($(findstring CONFIG_LTO=y,$(CONFIG_Y)),CONFIG_LTO=y)
ifeq ($(findstring CONFIG_CC_CLANG=y,$(CONFIG_Y)),CONFIG_CC_CLANG=y)
ifeq ($(findstring CONFIG_LTO_FULL=y,$(CONFIG_Y)),CONFIG_LTO_FULL=y)
LTO_FLAG := -flto=full
else
LTO_FLAG := -flto=thin
endif
CFLAGS    += $(LTO_FLAG)
LDFLAGS   += $(LTO_FLAG)
endif
endif

SRCS      := $(shell find src -name '*.c' | sort)
OBJS      := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS      := $(OBJS:.o=.d)
ICONS     := $(shell find data/icons -name '*.svg')
RES_XML   := data/icons/icons.gresource.xml
RES_OBJ   := $(BUILD_DIR)/icons_resources.o

.PHONY: all config menuconfig run clean mrproper install uninstall help

all: $(BIN_DIR)/$(APP_NAME)

config:
	bash scripts/config.sh

menuconfig:
	bash scripts/menuconfig.sh


$(CONFIG): $(KCONFIG) $(DEF_CONFIG) scripts/genconfig.sh
	sh scripts/genconfig.sh

help:
	@echo 'Configuration targets:'
	@echo '  config          - Update current config utilising a line-oriented program'
	@echo '  menuconfig      - Update current config utilising a menu based program'
	@echo ''
	@echo 'Build targets:'
	@echo '  all             - Build the application (default)'
	@echo '  run             - Build and run'
	@echo '  clean           - Remove build artifacts'
	@echo '  mrproper        - Remove build artifacts and .config'
	@echo ''
	@echo 'Install targets:'
	@echo '  install         - Install to PREFIX (default /usr/local)'
	@echo '  uninstall       - Remove installed files'
$(BIN_DIR)/$(APP_NAME): $(OBJS) $(RES_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(RES_OBJ): $(RES_XML) $(ICONS) $(CONFIG)
	@mkdir -p $(dir $@)
	glib-compile-resources --sourcedir=data/icons \
		--target=$@.c --generate-source $(RES_XML)
	$(CCACHE) $(CC) $(CFLAGS) -c $@.c -o $@

$(BUILD_DIR)/%.o: src/%.c $(CONFIG)
	@mkdir -p $(dir $@)
	$(CCACHE) $(CC) $(CFLAGS) -MMD -MP -c $< -o $@

run: all
	./$(BIN_DIR)/$(APP_NAME)

clean:
	rm -rf $(BIN_DIR)

mrproper: clean
	rm -f $(CONFIG)

install: all
	install -Dm755 $(BIN_DIR)/$(APP_NAME) $(BINDIR)/$(APP_NAME)
	install -Dm644 $(DATA_DIR)/com.rve.RvKernelManager.desktop $(DATADIR)/applications/com.rve.RvKernelManager.desktop
	install -Dm644 $(DATA_DIR)/icons/com.rve.RvKernelManager.svg $(DATADIR)/icons/hicolor/scalable/apps/com.rve.RvKernelManager.svg

uninstall:
	rm -f $(BINDIR)/$(APP_NAME)
	rm -f  $(DATADIR)/applications/com.rve.RvKernelManager.desktop
	rm -f  $(DATADIR)/icons/hicolor/scalable/apps/com.rve.RvKernelManager.svg

-include $(DEPS)
