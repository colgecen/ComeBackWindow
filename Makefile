# Come Back Window — build system
#
# Hedefler:
#   make                uygulamayı derle
#   make deps-check     gerekli pkg-config modüllerini denetle
#   make icons          SVG'den PNG ikonları üret
#   make mo             çevirileri derle
#   make lint           statik denetim
#   make test-unit      C birim testleri
#   make test-js        JS (node --test) testleri
#   make test-validate  desktop/metainfo/XML/çeviri doğrulama
#   make test-asan      ASan/UBSan ile birim testleri
#   make test-integration  KWin sanal oturumunda uçtan uca testler
#   make check          lint + unit + js + validate
#   make install        DESTDIR/PREFIX destekli kurulum
#   make uninstall      kurulumu kaldır
#   make dist           kaynak arşivi
#   make clean

PREFIX ?= /usr
DESTDIR ?=
BINDIR := $(DESTDIR)$(PREFIX)/bin
DATADIR := $(DESTDIR)$(PREFIX)/share
ICONDIR := $(DATADIR)/icons/hicolor
LOCALEDIR := $(DATADIR)/locale
APPDIR := $(DATADIR)/applications
METADIR := $(DATADIR)/metainfo

CC ?= gcc
PKG_CONFIG ?= pkg-config
WAYLAND_SCANNER ?= wayland-scanner

PKG_MODULES := glib-2.0 gio-2.0 gio-unix-2.0 gtk4 cairo wayland-client x11
PKG_PACKAGES := glib2-devel gtk4-devel wayland-devel libX11-devel   # Fedora adları

BUILD := build
OBJDIR := $(BUILD)/obj
TESTBIN := $(BUILD)/tests

WARN := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Werror
OPT ?= -O2
CFLAGS_LOCAL := $(WARN) $(OPT) -g -D_GNU_SOURCE -fPIC -Iprotocols -DCBW_DATADIR=\"$(PREFIX)/share\"
SANITIZE := -fsanitize=address,undefined -fno-omit-frame-pointer
LDLIBS_LOCAL := -lm

PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKG_MODULES) 2>/dev/null)
PKG_LIBS := $(shell $(PKG_CONFIG) --libs $(PKG_MODULES) 2>/dev/null)

LAYER_XML := protocols/wlr-layer-shell-unstable-v1.xml
LAYER_HDR := protocols/wlr-layer-shell-unstable-v1-client-protocol.h
LAYER_SRC := protocols/wlr-layer-shell-unstable-v1-protocol.c

# layer-shell, xdg_popup sembolüne başvurduğu için xdg-shell de üretilir.
XDG_XML := protocols/xdg-shell.xml
XDG_HDR := protocols/xdg-shell-client-protocol.h
XDG_SRC := protocols/xdg-shell-protocol.c

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRC))
BIN := comebackwindow

ICON_SIZES := 16 22 24 32 48 64 128 256
ICON_PNGS := $(foreach s,$(ICON_SIZES),assets/comebackwindow-$(s).png)

TESTS_C := $(wildcard tests/unit/test_*.c)
TESTS_BIN := $(patsubst tests/unit/%.c,$(TESTBIN)/%,$(TESTS_C))

.PHONY: all deps-check icons pot mo lint test-unit test-js test-validate \
        test-asan test-integration check install uninstall dist clean \
        protos help

all: $(BIN)

help:
	@sed -n '2,22p' Makefile

$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS_LOCAL) $(PKG_CFLAGS) -c $< -o $@

$(LAYER_HDR): $(LAYER_XML)
	$(WAYLAND_SCANNER) client-header $< $@

$(LAYER_SRC): $(LAYER_XML)
	$(WAYLAND_SCANNER) private-code $< $@

$(XDG_HDR): $(XDG_XML)
	$(WAYLAND_SCANNER) client-header $< $@

$(XDG_SRC): $(XDG_XML)
	$(WAYLAND_SCANNER) private-code $< $@

protos: $(LAYER_HDR) $(LAYER_SRC) $(XDG_HDR) $(XDG_SRC)

PROTO_OBJ := $(OBJDIR)/wlr-layer-shell-unstable-v1-protocol.o \
             $(OBJDIR)/xdg-shell-protocol.o

$(PROTO_OBJ): $(OBJDIR)/%.o: protocols/%.c $(LAYER_HDR) $(XDG_HDR)
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS_LOCAL) $(PKG_CFLAGS) -c $< -o $@

$(OBJDIR)/strip_wayland.o: $(LAYER_HDR)
$(BIN): $(OBJ) $(PROTO_OBJ)
	$(CC) $(CFLAGS_LOCAL) $(OBJ) $(PROTO_OBJ) -o $@ $(PKG_LIBS) $(LDLIBS_LOCAL)

deps-check:
	@missing=""; \
	for m in $(PKG_MODULES); do \
	  if ! $(PKG_CONFIG) --exists $$m; then missing="$$missing $$m"; fi; \
	done; \
	if [ -n "$$missing" ]; then \
	  echo "Eksik pkg-config modülleri:$$missing"; \
	  echo "Fedora: sudo dnf install $(PKG_PACKAGES)"; \
	  echo "Arch  : sudo pacman -S --needed glib2 gtk4 wayland libx11"; \
	  exit 1; \
	fi; \
	echo "Tüm pkg-config modülleri hazır."
	@command -v $(WAYLAND_SCANNER) >/dev/null || { echo "wayland-scanner eksik (wayland-devel / wayland paketi)"; exit 1; }

icons: $(ICON_PNGS)
	@echo "İkonlar üretildi: $(ICON_PNGS)"

assets/comebackwindow-%.png: assets/comebackwindow.svg
ifeq ($(shell command -v rsvg-convert 2>/dev/null),)
	@echo "rsvg-convert bulunamadı, ImageMagick ile üretiliyor (rsvg2-tools önerilir)"
	convert -background none -density 384 $< -resize $*x$* $@
else
	rsvg-convert -w $* -h $* -o $@ $<
endif

po/%.mo: po/%.po
	msgfmt --check -o $@ $<

mo: $(patsubst %.po,%.mo,$(wildcard po/*.po))

lint: all
	@set -e; \
	if command -v cppcheck >/dev/null; then \
	  cppcheck --enable=all --error-exitcode=1 --inline-suppr \
	    --suppress=missingIncludeSystem --quiet src/; \
	else echo "SKIP cppcheck (kurulu değil)"; fi; \
	if command -v clang-tidy >/dev/null && [ -e $(BUILD)/compile_commands.json ]; then \
	  clang-tidy -p $(BUILD) $$(find src -name '*.c') \
	    -checks='-*,bugprone-*,clang-analyzer-*,readability-*' --quiet; \
	else echo "SKIP clang-tidy (kurulu değil veya derleme veritabanı yok)"; fi; \
	if command -v shellcheck >/dev/null; then shellcheck tools/*.sh tests/integration/*.sh; \
	else echo "SKIP shellcheck (kurulu değil)"; fi; \
	if command -v desktop-file-validate >/dev/null && ls data/*.desktop >/dev/null 2>&1; then \
	  desktop-file-validate data/*.desktop; \
	else echo "SKIP desktop-file-validate (araç yok veya dosya yok)"; fi; \
	if command -v appstreamcli >/dev/null && ls data/*.metainfo.xml >/dev/null 2>&1; then \
	  appstreamcli validate --no-net data/*.metainfo.xml; \
	else echo "SKIP appstreamcli (araç yok veya dosya yok)"; fi; \
	if command -v xmllint >/dev/null && ls protocols/*.xml >/dev/null 2>&1; then \
	  xmllint --noout protocols/*.xml; \
	else echo "SKIP xmllint (araç yok veya dosya yok)"; fi; \
	if command -v msgfmt >/dev/null && ls po/*.po >/dev/null 2>&1; then \
	  for p in po/*.po; do msgfmt --check -o /dev/null $$p; done; \
	else echo "SKIP msgfmt (araç yok veya dosya yok)"; fi

$(TESTBIN)/%: tests/unit/%.c $(TEST_SRC)
	@mkdir -p $(TESTBIN)
	$(CC) $(CFLAGS_LOCAL) $(PKG_CFLAGS) $< $(TEST_UNIT_SRC) -o $@ $(PKG_LIBS) $(LDLIBS_LOCAL)

# Birim testlerinde ortak kod: kaynak modüller doğrudan bağlanır.
TEST_UNIT_SRC := $(filter-out src/main.c,$(SRC)) $(LAYER_SRC) $(XDG_SRC)

test-unit: all
	@set -e; mkdir -p $(TESTBIN); if [ -z "$(TESTS_C)" ]; then echo "Birim testi yok"; exit 0; fi; \
	for t in $(TESTS_C); do \
	  name=$$(basename $$t .c); \
	  $(CC) $(CFLAGS_LOCAL) $(PKG_CFLAGS) -Isrc \
	    tests/unit/$$name.c $(TEST_UNIT_SRC) -o $(TESTBIN)/$$name \
	    $(PKG_LIBS) $(LDLIBS_LOCAL); \
	  echo "== $$name"; $(TESTBIN)/$$name; \
	done

test-asan: clean
	@$(MAKE) test-unit OPT="$(SANITIZE) -O1" 

test-js:
	@if command -v node >/dev/null; then node --test tests/js/*.test.js; \
	else echo "SKIP test-js (node yok)"; fi

test-validate:
	@if command -v desktop-file-validate >/dev/null && ls data/*.desktop >/dev/null 2>&1; then \
	  desktop-file-validate data/*.desktop; fi
	@if command -v appstreamcli >/dev/null && ls data/*.metainfo.xml >/dev/null 2>&1; then \
	  appstreamcli validate --no-net data/*.metainfo.xml; fi
	@if command -v xmllint >/dev/null && ls protocols/*.xml >/dev/null 2>&1; then \
	  xmllint --noout protocols/*.xml; fi
	@if command -v msgfmt >/dev/null && ls po/*.po >/dev/null 2>&1; then \
	  for p in po/*.po; do msgfmt --check -o /dev/null $$p; done; fi
	@echo "Doğrulama tamam."

test-integration: all tests/helpers/testwin
	@if [ "$$XDG_SESSION_TYPE" != "wayland" ] && [ -z "$$WAYLAND_DISPLAY" ]; then \
	  echo "SKIP test-integration (Wayland oturumu yok)"; exit 0; fi
	bash tests/integration/run.sh

tests/helpers/testwin: tests/helpers/testwin.c
	$(CC) $(CFLAGS_LOCAL) $(PKG_CFLAGS) $< -o $@ $(PKG_LIBS) $(LDLIBS_LOCAL)

check: lint test-unit test-js test-validate
	@echo "make check: TÜMÜ YEŞİL"

install: all icons mo
	install -Dm755 $(BIN) $(BINDIR)/$(BIN)
	install -d $(DATADIR)/comebackwindow/kwin
	cp -r kwin/metadata.json kwin/contents $(DATADIR)/comebackwindow/kwin/
	install -d $(DATADIR)/comebackwindow/kwin/contents/code
	install -Dm644 assets/comebackwindow.svg \
	  $(ICONDIR)/scalable/apps/comebackwindow.svg
	for s in $(ICON_SIZES); do \
	  install -Dm644 assets/comebackwindow-$$s.png \
	    $(ICONDIR)/$${s}x$${s}/apps/comebackwindow.png; \
	done
	install -Dm644 data/org.comebackwindow.App.desktop \
	  $(APPDIR)/org.comebackwindow.App.desktop
	install -Dm644 data/org.comebackwindow.App.metainfo.xml \
	  $(METADIR)/org.comebackwindow.App.metainfo.xml
	for p in po/*.mo; do \
	  lang=$$(basename $$p .mo); \
	  install -Dm644 $$p $(LOCALEDIR)/$$lang/LC_MESSAGES/comebackwindow.mo; \
	done

uninstall:
	rm -f $(BINDIR)/$(BIN)
	rm -rf $(DATADIR)/comebackwindow
	rm -f $(ICONDIR)/scalable/apps/comebackwindow.svg
	rm -f $(APPDIR)/org.comebackwindow.App.desktop
	rm -f $(METADIR)/org.comebackwindow.App.metainfo.xml
	for s in $(ICON_SIZES); do rm -f $(ICONDIR)/$${s}x$${s}/apps/comebackwindow.png; done
	for p in po/*.mo; do lang=$$(basename $$p .mo); \
	  rm -f $(LOCALEDIR)/$$lang/LC_MESSAGES/comebackwindow.mo; done

dist:
	@ver=$$(sed -n 's/^#define CBW_VERSION "\(.*\)"/\1/p' src/version.h 2>/dev/null || echo 1.0.0); \
	tar --exclude-vcs --exclude=build --exclude='*.png' --exclude='*.mo' \
	    --transform "s,^\.,comebackwindow-$$ver," -czf comebackwindow-$$ver.tar.gz .

clean:
	rm -rf $(BUILD) $(BIN) tests/helpers/testwin
	rm -f $(LAYER_HDR) $(LAYER_SRC) $(XDG_HDR) $(XDG_SRC) po/*.mo
