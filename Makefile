# FreeArc Modern C/C++ Suite Root Makefile
# Targets: all, debug, clean, test, install, uninstall, deb, help

PREFIX ?= /usr/local
DESTDIR ?=

.PHONY: all debug clean test install uninstall deb help

all:
	$(MAKE) -C unarc all

debug:
	$(MAKE) -C unarc debug

clean:
	$(MAKE) -C unarc clean
	rm -f *.deb
	rm -rf tests_out/ /tmp/arc_test_*

test: all
	@chmod +x tests/run_tests.sh
	./tests/run_tests.sh

deb: all
	@chmod +x build_deb.sh
	./build_deb.sh

install: all
	$(MAKE) -C unarc install PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)

uninstall:
	$(MAKE) -C unarc uninstall PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)

help:
	@echo "FreeArc Native Suite build targets:"
	@echo "  make            - Build production binaries (arc, unarc, arc.sfx) with full optimizations"
	@echo "  make debug      - Build debug binaries with symbols enabled"
	@echo "  make test       - Run comprehensive automated test suite"
	@echo "  make deb        - Build and package standalone Debian .deb package"
	@echo "  make install    - Install binaries to PREFIX (default: /usr/local)"
	@echo "  make clean      - Clean build artifacts"
