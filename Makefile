# FreeArc Modern C/C++ Root Makefile
# Targets: all, win64, both, debug, clean, test, test-win64, test-all, install, uninstall, deb, help

PREFIX ?= /usr/local
DESTDIR ?=

.PHONY: all win64 both debug clean test test-win64 test-all install uninstall deb help

all:
	$(MAKE) -C src all

win64:
	$(MAKE) -C src win64

both:
	$(MAKE) -C src both

debug:
	$(MAKE) -C src debug

clean:
	$(MAKE) -C src clean
	rm -rf build/ bin/
	rm -f *.deb
	rm -rf tests_out/ /tmp/arc_test_*

test: all
	@chmod +x tests/run_tests.sh
	./tests/run_tests.sh

test-win64: win64
	@chmod +x tests/run_tests_win64.sh
	./tests/run_tests_win64.sh

test-all: test test-win64

deb: all
	@chmod +x build_deb.sh
	./build_deb.sh

install: all
	$(MAKE) -C src install PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)

uninstall:
	$(MAKE) -C src uninstall PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)

help:
	@echo "FreeArc Native build targets:"
	@echo "  make            - Build Linux production binaries (./build/linux/arc, unarc, arc.sfx)"
	@echo "  make win64      - Build Windows 64-bit production binaries (./build/win64/arc.exe, unarc.exe)"
	@echo "  make both       - Build both Linux and Windows 64-bit binaries"
	@echo "  make debug      - Build Linux debug binaries with symbols enabled"
	@echo "  make test       - Run Linux automated test suite"
	@echo "  make test-win64 - Run Windows 64-bit automated test suite (bit-for-bit exact & cross-platform via Wine)"
	@echo "  make test-all   - Run both Linux and Windows 64-bit test suites"
	@echo "  make deb        - Build and package standalone Debian .deb package"
	@echo "  make install    - Install Linux binaries to PREFIX (default: /usr/local)"
	@echo "  make clean      - Clean build artifacts"
