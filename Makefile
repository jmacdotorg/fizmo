
CONFIG_EXISTS := $(wildcard config.mk)

all: fizmo-console fizmo-glk fizmo-ncursesw

ifeq ($(strip $(CONFIG_EXISTS)),)

include config.default.mk

test_config:
	@echo
	@echo No file \"config.mk\" was found, using \"config.default.mk\".
	@echo
	cp config.default.mk config.mk
else

include config.mk

test_config:
	@true

endif


.PHONY : \
 all install install-locales clean distclean \
 libfizmo libcellif libsndifsdl libdrilbo fizmo-console fizmo-glk fizmo-ncursesw \
 subdir-configs \
 libfizmo-config libcellif-config libsndifsdl-config \
 libdrilbo-config fizmo-ncursesw-config fizmo-console-config fizmo-glk-config \
 build-dir

export DEV_INSTALL_PATH = build
export DEV_INSTALL_PREFIX = $(CURDIR)/$(DEV_INSTALL_PATH)
export PKG_CONFIG_PATH:=$(PKG_CONFIG_PATH):$(DEV_INSTALL_PREFIX)/lib/pkgconfig

install: install-locales install-fizmo-console install-fizmo-glk install-fizmo-ncursesw

fizmo-console:: fizmo-console-config libfizmo
	cd fizmo-console ; make

install-fizmo-console:: fizmo-console
	cd fizmo-console ; make install

fizmo-glk:: fizmo-glk-config libfizmo
	cd fizmo-glk ; make

install-fizmo-glk:: fizmo-glk
	cd fizmo-glk ; make install

fizmo-ncursesw:: fizmo-ncursesw-config libfizmo libcellif libsndifsdl libdrilbo
	cd fizmo-ncursesw; make

install-fizmo-ncursesw:: fizmo-ncursesw
	cd fizmo-ncursesw; make install

libfizmo:: build-dir libfizmo-config
	cd libfizmo ; make install-dev

libcellif: libcellif-config build-dir
	cd libcellif; make install-dev

libsndifsdl: libsndifsdl-config build-dir
ifdef SOUND_INTERFACE_NAME
ifdef SOUND_INTERFACE_STRUCT_NAME
ifdef SOUND_INTERFACE_STRUCT_NAME
	cd libsndifsdl; make install-dev
endif
endif
endif

libdrilbo: libdrilbo-config build-dir
ifdef ENABLE_X11_IMAGES
	cd libdrilbo; make install-dev
endif

clean: subdir-configs
	cd fizmo-ncursesw ; make clean
	cd fizmo-console ; make clean
	cd fizmo-glk ; make clean
	cd libdrilbo ; make clean
	cd libsndifsdl ; make clean
	cd libcellif ; make clean
	cd libfizmo ; make clean

distclean: subdir-configs
	cd fizmo-ncursesw ; make distclean
	cd fizmo-console ; make distclean
	cd fizmo-glk ; make distclean
	cd libdrilbo ; make distclean
	cd libsndifsdl ; make distclean
	cd libcellif ; make distclean
	cd libfizmo ; make distclean
	rm -rf $(DEV_INSTALL_PATH)

install-locales: subdir-configs
	cd libdrilbo ; make install-locales
	cd libsndifsdl ; make install-locales
	cd libcellif ; make install-locales
	cd libfizmo ; make install-locales

subdir-libs-configs: libfizmo-config libcellif-config libsndifsdl-config \
 libdrilbo-config

subdir-configs: libfizmo-config libcellif-config libsndifsdl-config \
 libdrilbo-config fizmo-ncursesw-config fizmo-console-config fizmo-glk-config

libfizmo-config:: test_config
	cp config.mk libfizmo/config.mk

libcellif-config:: test_config
	cp config.mk libcellif/config.mk

libsndifsdl-config:: test_config
	cp config.mk libsndifsdl/config.mk

libdrilbo-config:: test_config
	cp config.mk libdrilbo/config.mk

fizmo-ncursesw-config:: test_config
	cp config.mk fizmo-ncursesw/config.mk

fizmo-console-config:: test_config
	cp config.mk fizmo-console/config.mk

fizmo-glk-config:: test_config
	cp config.mk fizmo-glk/config.mk

build-dir::
	mkdir -p $(DEV_INSTALL_PATH)

