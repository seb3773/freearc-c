# FreeArc Modern C/C++ Root Makefile
# Targets: all, win64, arm64, both, all-platforms, debug, clean, test, test-win64, test-arm64, test-all, install, uninstall, deb, help

PREFIX ?= /usr/local
DESTDIR ?=

.PHONY: all win64 arm64 riscv64 both all-platforms debug clean test test-win64 test-arm64 test-riscv64 test-all lib dll lib-win64 lib-arm64 lib-riscv64 lib-all test-lib test-lib-win64 test-lib-arm64 test-lib-riscv64 install uninstall deb help

all:
	$(MAKE) -C src all

win64:
	$(MAKE) -C src win64

arm64:
	$(MAKE) -C src arm64

riscv64:
	$(MAKE) -C src riscv64

lib:
	$(MAKE) -C src lib

dll:
	$(MAKE) -C src dll

lib-win64:
	$(MAKE) -C src lib-win64

lib-arm64:
	$(MAKE) -C src lib-arm64

lib-riscv64:
	$(MAKE) -C src lib-riscv64

lib-all:
	$(MAKE) -C src lib-all

both:
	$(MAKE) -C src both

all-platforms:
	$(MAKE) -C src all-platforms

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

test-arm64: arm64
	@chmod +x tests/run_tests_arm64.sh
	./tests/run_tests_arm64.sh

test-riscv64: riscv64
	@chmod +x tests/run_tests_riscv64.sh
	./tests/run_tests_riscv64.sh

test-lib: lib
	@chmod +x tests/run_tests_lib.sh
	./tests/run_tests_lib.sh linux

test-lib-win64: lib-win64
	@chmod +x tests/run_tests_lib.sh
	./tests/run_tests_lib.sh win64

test-lib-arm64: lib-arm64
	@chmod +x tests/run_tests_lib.sh
	./tests/run_tests_lib.sh arm64

test-lib-riscv64: lib-riscv64
	@chmod +x tests/run_tests_lib.sh
	./tests/run_tests_lib.sh riscv64

test-all: test test-win64 test-arm64

deb: all
	@chmod +x build_deb.sh
	./build_deb.sh

install: all
	$(MAKE) -C src install PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)

uninstall:
	$(MAKE) -C src uninstall PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)

help:
	@echo "FreeArc Native build targets:"
	@echo "  make             - Build Linux x86_64 production binaries (./build/linux/arc, unarc, arc.sfx)"
	@echo "  make win64       - Build Windows 64-bit production binaries (./build/win64/arc.exe, unarc.exe)"
	@echo "  make arm64       - Build Linux ARM64 production binaries (./build/linux-arm64/arc, unarc, arc.sfx)"
	@echo "  make riscv64     - Build Linux RISC-V 64 binaries (./build/linux-riscv64/arc, unarc, arc.sfx)"
	@echo "  make lib         - Build Linux x86_64 decoder libraries (libunarc.so, libunarc.a, unarc.h)"
	@echo "  make dll         - Build Windows 64-bit decoder DLL (unarc.dll, libunarc.dll.a, unarc.h)"
	@echo "  make lib-arm64   - Build Linux ARM64 decoder libraries (libunarc.so, libunarc.a, unarc.h)"
	@echo "  make lib-riscv64 - Build Linux RISC-V 64 decoder libraries (libunarc.so, libunarc.a, unarc.h)"
	@echo "  make lib-all     - Build decoder libraries for Linux x86_64, Win64, ARM64 (and RISC-V if installed)"
	@echo "  make both        - Build both Linux x86_64 and Windows 64-bit binaries"
	@echo "  make all-platforms - Build all targets (Linux x86_64, Windows Win64, Linux ARM64)"
	@echo "  make debug       - Build Linux debug binaries with symbols enabled"
	@echo "  make test        - Run Linux automated test suite"
	@echo "  make test-win64  - Run Windows 64-bit automated test suite (bit-for-bit exact & cross-platform via Wine)"
	@echo "  make test-arm64  - Run Linux ARM64 automated test suite (QEMU user-mode execution & cross-platform)"
	@echo "  make test-lib    - Test Linux x86_64 libunarc.so and libunarc.a"
	@echo "  make test-lib-win64 - Test Windows 64-bit unarc.dll via Wine"
	@echo "  make test-lib-arm64 - Test Linux ARM64 libunarc.so/.a via QEMU"
	@echo "  make test-all    - Run all CLI test suites (Linux, Win64, ARM64)"
	@echo "  make deb         - Build and package standalone Debian .deb package"
	@echo "  make install     - Install Linux binaries to PREFIX (default: /usr/local)"
	@echo "  make clean       - Clean build artifacts"
