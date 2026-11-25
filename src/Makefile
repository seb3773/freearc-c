CXX ?= g++
CXX_WIN64 ?= x86_64-w64-mingw32-g++
CXX_ARM64 ?= aarch64-linux-gnu-g++
CC ?= gcc
CC_WIN64 ?= x86_64-w64-mingw32-gcc
CC_ARM64 ?= aarch64-linux-gnu-gcc
# C flags for the vendored LZMA SDK 25.01 decoder (separate translation unit)
FASTDEC_SRC = Compression/LZMA2/sdk25/lzma_fastdec.c
# x86-64 assembly LZMA decoder loop (LzmaDecOpt from LZMA SDK 25.01, GNU as syntax).
# Enabled by default on x86-64 hosts; LZMA_ASM=0 forces the portable C loop.
LZMA_ASM ?= $(if $(filter x86_64 amd64,$(shell uname -m)),1,0)
ifeq ($(LZMA_ASM),1)
  FASTDEC_DEFS = -DZ7_LZMA_DEC_OPT
  LZMA_ASM_DEC = $(DEC_OBJDIR)/LzmaDecOpt.o
  LZMA_ASM_ENC = $(ENC_OBJDIR)/LzmaDecOpt.o
endif
# x86-64 assembly match finders for the ENCODER (bit-exact LZMA BT/HT match finder in
# LzFind.c, the LZMA SDK 25.01 BT match finder of the -nobitexact encoder - single-thread
# GetMatchesSpec1/SkipMatchesSpec and the batched MT GetMatchesSpecN_2 - and Tornado match
# length kernels): see Compression/LZMA2/asm/LzFind_x86_64.S, LzFind25_x86_64.S,
# Compression/Tornado/asm/tor_match_x86_64.S and Compression/asm/. Output is bit-identical
# to the C code. Enabled by default on x86-64 hosts (and always for the Win64 cross build);
# MF_ASM=0 / MF_ASM_WIN64=0 force the portable C code. ARM64 never uses it.
MF_ASM ?= $(if $(filter x86_64 amd64,$(shell uname -m)),1,0)
MF_ASM_WIN64 ?= 1
MF_ASM_INC = Compression/asm/fa_x86_64.h
MF_ASM_NAMES = fa_cpu_x86_64.o LzFind_x86_64.o LzFind25_x86_64.o tor_match_x86_64.o
ifeq ($(MF_ASM),1)
  MF_ASM_DEFS = -DFA_MF_ASM
  MF_ASM_ENC  = $(addprefix $(ENC_OBJDIR)/,$(MF_ASM_NAMES))
endif
ifeq ($(MF_ASM_WIN64),1)
  MF_ASM_WIN64_DEFS = -DFA_MF_ASM
  MF_ASM_ENC_WIN64  = $(addprefix $(ENC_WIN64_OBJDIR)/,$(MF_ASM_NAMES))
endif
LZMA_ENC_DEPS = Compression/LZMA2/C/LzFind.c Compression/LZMA2/C/LzFind.h Compression/LZMA2/C/LzmaEnc.c
FASTDEC_DEPS = $(FASTDEC_SRC) Compression/LZMA2/sdk25/LzmaDec.c Compression/LZMA2/sdk25/LzmaDec.h Compression/LZMA2/sdk25/lzma_fastdec.h
# LZMA SDK 25.01 encoder (unity build, symbols renamed), used by the archiver in -nobitexact mode
FASTENC_SRC = Compression/LZMA2/sdk25/lzma_fastenc.c
FASTENC_DEPS = $(FASTENC_SRC) $(wildcard Compression/LZMA2/sdk25/*.h) Compression/LZMA2/sdk25/LzmaEnc.c \
               Compression/LZMA2/sdk25/LzFind.c Compression/LZMA2/sdk25/LzFindMt.c Compression/LZMA2/sdk25/LzFindOpt.c \
               Compression/LZMA2/sdk25/Threads.c Compression/LZMA2/sdk25/CpuArch.c

# Build mode configuration:
#   make (or make DEBUG=0) -> Production build: aggressive optimization (-O3, LTO, gc-sections, sstrip)
#   make DEBUG=1           -> Debug build: full debug symbols (-g), assertions, no LTO, no stripping
#   make debug             -> Convenience alias for make DEBUG=1
DEBUG ?= 0

DEFINES = -DFREEARC_UNIX -DFREEARC_INTEL_BYTE_ORDER -DFREEARC_64BIT
Extractor_DEFINES = -DFREEARC_DECOMPRESS_ONLY -D_NO_EXCEPTIONS -DUNARC

CODE_FLAGS = -fno-exceptions -fno-rtti -w

ifeq ($(DEBUG), 1)
    MODE_DESC    = [DEBUG BUILD - Symbols enabled, no LTO, no strip]
    MODE_CFLAGS  = -g -D_DEBUG -O1
    MODE_LDFLAGS = -g
    STRIP_CMD    = true
else
    MODE_DESC    = [PRODUCTION BUILD - Aggressive optimization (-O3) + LTO + gc-sections + sstrip]
    MODE_CFLAGS  = -O3 -flto=auto -fstrict-aliasing \
                   -ffunction-sections -fdata-sections \
                   -fno-ident -fno-plt -fomit-frame-pointer \
                   -fno-stack-protector -fno-math-errno \
                   -fvisibility=hidden -fmerge-all-constants \
                   -fno-asynchronous-unwind-tables -DNDEBUG \
                   -fno-threadsafe-statics
    MODE_LDFLAGS = -Wl,-O1 -Wl,--gc-sections -Wl,--as-needed \
                   -Wl,--build-id=none -Wl,-z,norelro -flto=auto -s
    STRIP_CMD    = (command -v sstrip >/dev/null 2>&1 && sstrip $@) || strip --strip-all $@
endif

# Optional extra flags (used by the PGO build: PGO_FLAGS=-fprofile-generate / -fprofile-use ...)
MODE_CFLAGS  += $(PGO_FLAGS)
MODE_LDFLAGS += $(PGO_FLAGS)

UNIX_LINK_FLAGS = -lstdc++ -lrt -lpthread $(MODE_LDFLAGS)

CFLAGS           = $(CODE_FLAGS) $(MODE_CFLAGS) $(DEFINES) $(Extractor_DEFINES)
CFLAGS_NO_SA     = $(CODE_FLAGS) $(MODE_CFLAGS) $(DEFINES) $(Extractor_DEFINES) -fno-strict-aliasing
ENC_CFLAGS       = $(CODE_FLAGS) $(MODE_CFLAGS) $(DEFINES) -ICompression
ENC_CFLAGS_NO_SA = $(CODE_FLAGS) $(MODE_CFLAGS) $(DEFINES) -ICompression -fno-strict-aliasing

# --- Win64 configuration ---
WIN64_DEFINES = -DFREEARC_WIN -DFREEARC_INTEL_BYTE_ORDER -DFREEARC_64BIT
WIN64_CFLAGS           = $(CODE_FLAGS) -O3 $(WIN64_DEFINES) $(Extractor_DEFINES) -I. -ICompression
WIN64_CFLAGS_NO_SA     = $(CODE_FLAGS) -O3 $(WIN64_DEFINES) $(Extractor_DEFINES) -I. -ICompression -fno-strict-aliasing
WIN64_ENC_CFLAGS       = $(CODE_FLAGS) -O3 $(WIN64_DEFINES) -I. -ICompression
WIN64_ENC_CFLAGS_NO_SA = $(CODE_FLAGS) -O3 $(WIN64_DEFINES) -I. -ICompression -fno-strict-aliasing
WIN64_LINK_FLAGS       = -static -static-libgcc -static-libstdc++ -lkernel32 -luser32 -lshell32 -ladvapi32 -lole32 -s

# --- ARM64 configuration ---
ARM64_DEFINES = -DFREEARC_UNIX -DFREEARC_INTEL_BYTE_ORDER -DFREEARC_64BIT
ARM64_CFLAGS           = $(CODE_FLAGS) -O3 $(ARM64_DEFINES) $(Extractor_DEFINES) -I. -ICompression
ARM64_CFLAGS_NO_SA     = $(CODE_FLAGS) -O3 $(ARM64_DEFINES) $(Extractor_DEFINES) -I. -ICompression -fno-strict-aliasing
ARM64_ENC_CFLAGS       = $(CODE_FLAGS) -O3 $(ARM64_DEFINES) -I. -ICompression
ARM64_ENC_CFLAGS_NO_SA = $(CODE_FLAGS) -O3 $(ARM64_DEFINES) -I. -ICompression -fno-strict-aliasing
ARM64_LINK_FLAGS       = -lstdc++ -lrt -lpthread -lm -Wl,-O1 -Wl,--gc-sections -Wl,--as-needed -Wl,--build-id=none -s

DEC_OBJDIR       = dec
ENC_OBJDIR       = enc
DEC_WIN64_OBJDIR = dec_win64
ENC_WIN64_OBJDIR = enc_win64
DEC_ARM64_OBJDIR = dec_arm64
ENC_ARM64_OBJDIR = enc_arm64

HEADERS       = Compression/Compression.h Compression/Common.h
UNARC_HEADERS = ArcStructure.h ArcCommand.h ArcProcess.h CUI.h WinCompat.h ArcIni.h $(HEADERS)

DEC_LINKOBJ = $(DEC_OBJDIR)/unarc.o $(DEC_OBJDIR)/Environment.o $(DEC_OBJDIR)/Common.o \
              $(DEC_OBJDIR)/CompressionLibrary.o $(DEC_OBJDIR)/C_LZMA.o $(DEC_OBJDIR)/lzma_fastdec.o $(LZMA_ASM_DEC) $(DEC_OBJDIR)/C_BCJ.o \
              $(DEC_OBJDIR)/C_Dict.o $(DEC_OBJDIR)/C_Delta.o $(DEC_OBJDIR)/C_REP.o \
              $(DEC_OBJDIR)/C_LZP.o $(DEC_OBJDIR)/C_PPMD.o $(DEC_OBJDIR)/C_External.o \
              $(DEC_OBJDIR)/C_MM.o $(DEC_OBJDIR)/C_TTA.o $(DEC_OBJDIR)/C_Tornado.o \
              $(DEC_OBJDIR)/C_GRZip.o $(DEC_OBJDIR)/C_4x4.o $(DEC_OBJDIR)/C_Encryption.o

SFX_LINKOBJ = $(DEC_OBJDIR)/unarc_sfx.o $(DEC_OBJDIR)/Environment.o $(DEC_OBJDIR)/Common.o \
              $(DEC_OBJDIR)/CompressionLibrary.o $(DEC_OBJDIR)/C_LZMA.o $(DEC_OBJDIR)/lzma_fastdec.o $(LZMA_ASM_DEC) $(DEC_OBJDIR)/C_BCJ.o \
              $(DEC_OBJDIR)/C_Dict.o $(DEC_OBJDIR)/C_Delta.o $(DEC_OBJDIR)/C_REP.o \
              $(DEC_OBJDIR)/C_LZP.o $(DEC_OBJDIR)/C_PPMD.o $(DEC_OBJDIR)/C_External.o \
              $(DEC_OBJDIR)/C_MM.o $(DEC_OBJDIR)/C_TTA.o $(DEC_OBJDIR)/C_Tornado.o \
              $(DEC_OBJDIR)/C_GRZip.o $(DEC_OBJDIR)/C_4x4.o $(DEC_OBJDIR)/C_Encryption.o

ENC_LINKOBJ = $(ENC_OBJDIR)/arc.o $(ENC_OBJDIR)/Environment.o $(ENC_OBJDIR)/Common.o \
              $(ENC_OBJDIR)/CompressionLibrary.o $(ENC_OBJDIR)/C_LZMA.o $(ENC_OBJDIR)/lzma_fastdec.o $(ENC_OBJDIR)/lzma_fastenc.o $(LZMA_ASM_ENC) $(MF_ASM_ENC) $(ENC_OBJDIR)/C_BCJ.o \
              $(ENC_OBJDIR)/C_Dict.o $(ENC_OBJDIR)/C_Delta.o $(ENC_OBJDIR)/C_REP.o \
              $(ENC_OBJDIR)/C_LZP.o $(ENC_OBJDIR)/C_PPMD.o $(ENC_OBJDIR)/C_External.o \
              $(ENC_OBJDIR)/C_MM.o $(ENC_OBJDIR)/C_TTA.o $(ENC_OBJDIR)/C_Tornado.o \
              $(ENC_OBJDIR)/C_GRZip.o $(ENC_OBJDIR)/C_4x4.o $(ENC_OBJDIR)/C_Encryption.o

DEC_WIN64_LINKOBJ = $(DEC_WIN64_OBJDIR)/unarc.o $(DEC_WIN64_OBJDIR)/Environment.o $(DEC_WIN64_OBJDIR)/Common.o \
                    $(DEC_WIN64_OBJDIR)/CompressionLibrary.o $(DEC_WIN64_OBJDIR)/C_LZMA.o $(DEC_WIN64_OBJDIR)/lzma_fastdec.o $(DEC_WIN64_OBJDIR)/LzmaDecOpt.o $(DEC_WIN64_OBJDIR)/C_BCJ.o \
                    $(DEC_WIN64_OBJDIR)/C_Dict.o $(DEC_WIN64_OBJDIR)/C_Delta.o $(DEC_WIN64_OBJDIR)/C_REP.o \
                    $(DEC_WIN64_OBJDIR)/C_LZP.o $(DEC_WIN64_OBJDIR)/C_PPMD.o $(DEC_WIN64_OBJDIR)/C_External.o \
                    $(DEC_WIN64_OBJDIR)/C_MM.o $(DEC_WIN64_OBJDIR)/C_TTA.o $(DEC_WIN64_OBJDIR)/C_Tornado.o \
                    $(DEC_WIN64_OBJDIR)/C_GRZip.o $(DEC_WIN64_OBJDIR)/C_4x4.o $(DEC_WIN64_OBJDIR)/C_Encryption.o

ENC_WIN64_LINKOBJ = $(ENC_WIN64_OBJDIR)/arc.o $(ENC_WIN64_OBJDIR)/Environment.o $(ENC_WIN64_OBJDIR)/Common.o \
                    $(ENC_WIN64_OBJDIR)/CompressionLibrary.o $(ENC_WIN64_OBJDIR)/C_LZMA.o $(ENC_WIN64_OBJDIR)/lzma_fastdec.o $(ENC_WIN64_OBJDIR)/lzma_fastenc.o $(ENC_WIN64_OBJDIR)/LzmaDecOpt.o $(MF_ASM_ENC_WIN64) $(ENC_WIN64_OBJDIR)/C_BCJ.o \
                    $(ENC_WIN64_OBJDIR)/C_Dict.o $(ENC_WIN64_OBJDIR)/C_Delta.o $(ENC_WIN64_OBJDIR)/C_REP.o \
                    $(ENC_WIN64_OBJDIR)/C_LZP.o $(ENC_WIN64_OBJDIR)/C_PPMD.o $(ENC_WIN64_OBJDIR)/C_External.o \
                    $(ENC_WIN64_OBJDIR)/C_MM.o $(ENC_WIN64_OBJDIR)/C_TTA.o $(ENC_WIN64_OBJDIR)/C_Tornado.o \
                    $(ENC_WIN64_OBJDIR)/C_GRZip.o $(ENC_WIN64_OBJDIR)/C_4x4.o $(ENC_WIN64_OBJDIR)/C_Encryption.o

DEC_ARM64_LINKOBJ = $(DEC_ARM64_OBJDIR)/unarc.o $(DEC_ARM64_OBJDIR)/Environment.o $(DEC_ARM64_OBJDIR)/Common.o \
                    $(DEC_ARM64_OBJDIR)/CompressionLibrary.o $(DEC_ARM64_OBJDIR)/C_LZMA.o $(DEC_ARM64_OBJDIR)/lzma_fastdec.o $(DEC_ARM64_OBJDIR)/C_BCJ.o \
                    $(DEC_ARM64_OBJDIR)/C_Dict.o $(DEC_ARM64_OBJDIR)/C_Delta.o $(DEC_ARM64_OBJDIR)/C_REP.o \
                    $(DEC_ARM64_OBJDIR)/C_LZP.o $(DEC_ARM64_OBJDIR)/C_PPMD.o $(DEC_ARM64_OBJDIR)/C_External.o \
                    $(DEC_ARM64_OBJDIR)/C_MM.o $(DEC_ARM64_OBJDIR)/C_TTA.o $(DEC_ARM64_OBJDIR)/C_Tornado.o \
                    $(DEC_ARM64_OBJDIR)/C_GRZip.o $(DEC_ARM64_OBJDIR)/C_4x4.o $(DEC_ARM64_OBJDIR)/C_Encryption.o

SFX_ARM64_LINKOBJ = $(DEC_ARM64_OBJDIR)/unarc_sfx.o $(DEC_ARM64_OBJDIR)/Environment.o $(DEC_ARM64_OBJDIR)/Common.o \
                    $(DEC_ARM64_OBJDIR)/CompressionLibrary.o $(DEC_ARM64_OBJDIR)/C_LZMA.o $(DEC_ARM64_OBJDIR)/lzma_fastdec.o $(DEC_ARM64_OBJDIR)/C_BCJ.o \
                    $(DEC_ARM64_OBJDIR)/C_Dict.o $(DEC_ARM64_OBJDIR)/C_Delta.o $(DEC_ARM64_OBJDIR)/C_REP.o \
                    $(DEC_ARM64_OBJDIR)/C_LZP.o $(DEC_ARM64_OBJDIR)/C_PPMD.o $(DEC_ARM64_OBJDIR)/C_External.o \
                    $(DEC_ARM64_OBJDIR)/C_MM.o $(DEC_ARM64_OBJDIR)/C_TTA.o $(DEC_ARM64_OBJDIR)/C_Tornado.o \
                    $(DEC_ARM64_OBJDIR)/C_GRZip.o $(DEC_ARM64_OBJDIR)/C_4x4.o $(DEC_ARM64_OBJDIR)/C_Encryption.o

ENC_ARM64_LINKOBJ = $(ENC_ARM64_OBJDIR)/arc.o $(ENC_ARM64_OBJDIR)/Environment.o $(ENC_ARM64_OBJDIR)/Common.o \
                    $(ENC_ARM64_OBJDIR)/CompressionLibrary.o $(ENC_ARM64_OBJDIR)/C_LZMA.o $(ENC_ARM64_OBJDIR)/lzma_fastdec.o $(ENC_ARM64_OBJDIR)/lzma_fastenc.o $(ENC_ARM64_OBJDIR)/C_BCJ.o \
                    $(ENC_ARM64_OBJDIR)/C_Dict.o $(ENC_ARM64_OBJDIR)/C_Delta.o $(ENC_ARM64_OBJDIR)/C_REP.o \
                    $(ENC_ARM64_OBJDIR)/C_LZP.o $(ENC_ARM64_OBJDIR)/C_PPMD.o $(ENC_ARM64_OBJDIR)/C_External.o \
                    $(ENC_ARM64_OBJDIR)/C_MM.o $(ENC_ARM64_OBJDIR)/C_TTA.o $(ENC_ARM64_OBJDIR)/C_Tornado.o \
                    $(ENC_ARM64_OBJDIR)/C_GRZip.o $(ENC_ARM64_OBJDIR)/C_4x4.o $(ENC_ARM64_OBJDIR)/C_Encryption.o

.PHONY: all win64 arm64 riscv64 both all-platforms debug clean info info_win64 info_arm64 info_riscv64 install uninstall lib dll lib-win64 lib-arm64 lib-riscv64 lib-all

all: info arc unarc arc.sfx
	@mkdir -p ../build/linux
	cp -f arc ../build/linux/arc
	cp -f unarc ../build/linux/unarc
	cp -f arc.sfx ../build/linux/arc.sfx
	@if [ -f ../arc.ini.example ]; then cp -f ../arc.ini.example ../build/linux/arc.ini.example; fi
	@echo "=== Linux binaries updated in ../build/linux/ ==="
	@ls -lh ../build/linux/arc ../build/linux/unarc ../build/linux/arc.sfx

win64: info_win64 arc.exe unarc.exe
	@mkdir -p ../build/win64
	cp -f arc.exe ../build/win64/arc.exe
	cp -f unarc.exe ../build/win64/unarc.exe
	@if [ -f ../arc.ini.example ]; then cp -f ../arc.ini.example ../build/win64/arc.ini.example; fi
	@echo "=== Windows 64-bit binaries updated in ../build/win64/ ==="
	@ls -lh ../build/win64/arc.exe ../build/win64/unarc.exe

arm64: info_arm64 arc-arm64 unarc-arm64 arc.sfx-arm64
	@mkdir -p ../build/linux-arm64
	cp -f arc-arm64 ../build/linux-arm64/arc
	cp -f unarc-arm64 ../build/linux-arm64/unarc
	cp -f arc.sfx-arm64 ../build/linux-arm64/arc.sfx
	@if [ -f ../arc.ini.example ]; then cp -f ../arc.ini.example ../build/linux-arm64/arc.ini.example; fi
	@echo "=== Linux ARM64 binaries updated in ../build/linux-arm64/ ==="
	@ls -lh ../build/linux-arm64/arc ../build/linux-arm64/unarc ../build/linux-arm64/arc.sfx

both: all win64

all-platforms: all win64 arm64

info:
	@echo "Building Linux targets with $(MODE_DESC)"

info_win64:
	@echo "Building Windows 64-bit targets (MinGW-w64 static)"

info_arm64:
	@echo "Building Linux ARM64 targets (aarch64 cross-compilation)"

debug:
	$(MAKE) DEBUG=1

clean:
	rm -rf arc unarc arc.sfx arc.exe unarc.exe arc-arm64 unarc-arm64 arc.sfx-arm64 \
	       arc-riscv64 unarc-riscv64 arc.sfx-riscv64 \
	       unarc.dll libunarc.dll.a libunarc.so libunarc.a \
	       libunarc-arm64.so libunarc-arm64.a libunarc-riscv64.so libunarc-riscv64.a \
	       $(DEC_OBJDIR) $(ENC_OBJDIR) $(DEC_WIN64_OBJDIR) $(ENC_WIN64_OBJDIR) $(DEC_ARM64_OBJDIR) $(ENC_ARM64_OBJDIR) \
	       $(DEC_LIB_OBJDIR) $(DEC_LIB_WIN64_OBJDIR) $(DEC_LIB_ARM64_OBJDIR) $(DEC_RISCV64_OBJDIR) $(ENC_RISCV64_OBJDIR) $(DEC_LIB_RISCV64_OBJDIR) *.o

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib/freearc

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(LIBDIR)
	install -m 755 arc $(DESTDIR)$(BINDIR)/arc
	install -m 755 unarc $(DESTDIR)$(BINDIR)/unarc
	install -m 755 arc.sfx $(DESTDIR)$(LIBDIR)/arc.sfx
	install -d $(DESTDIR)$(PREFIX)/lib/arc
	ln -sfn ../freearc/arc.sfx $(DESTDIR)$(PREFIX)/lib/arc/arc.sfx

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/arc $(DESTDIR)$(BINDIR)/unarc
	rm -rf $(DESTDIR)$(LIBDIR) $(DESTDIR)$(PREFIX)/lib/arc

# ===================================================================
# Linux Binaries
# ===================================================================

arc: $(ENC_LINKOBJ)
	$(CXX) $(ENC_LINKOBJ) $(UNIX_LINK_FLAGS) -o $@
	@$(STRIP_CMD)

unarc: $(DEC_LINKOBJ)
	$(CXX) $(DEC_LINKOBJ) $(UNIX_LINK_FLAGS) -o $@
	@$(STRIP_CMD)

arc.sfx: $(SFX_LINKOBJ)
	$(CXX) $(SFX_LINKOBJ) $(UNIX_LINK_FLAGS) -o $@
	@$(STRIP_CMD)

# --- Linux unarc (decoder) objects ---

$(DEC_OBJDIR):
	mkdir -p $(DEC_OBJDIR)

$(DEC_OBJDIR)/unarc.o: unarc.cpp $(UNARC_HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/unarc_sfx.o: unarc.cpp $(UNARC_HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -DFREEARC_SFX -o $@ $<

$(DEC_OBJDIR)/Environment.o: Environment.cpp Environment.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/CompressionLibrary.o: Compression/CompressionLibrary.cpp $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/Common.o: Compression/Common.cpp $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_External.o: Compression/External/C_External.cpp Compression/External/C_External.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_BCJ.o: Compression/LZMA2/C_BCJ.cpp Compression/LZMA2/C_BCJ.h Compression/LZMA2/C/Bra86.c Compression/LZMA2/C/Bra.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_LZMA.o: Compression/LZMA2/C_LZMA.cpp Compression/LZMA2/C_LZMA.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_Dict.o: Compression/Dict/C_Dict.cpp Compression/Dict/C_Dict.h Compression/Dict/dict.cpp $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_Delta.o: Compression/Delta/C_Delta.cpp Compression/Delta/C_Delta.h Compression/Delta/Delta.cpp $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_REP.o: Compression/REP/C_REP.cpp Compression/REP/C_REP.h Compression/REP/rep.cpp $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_LZP.o: Compression/LZP/C_LZP.cpp Compression/LZP/C_LZP.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_PPMD.o: Compression/PPMD/C_PPMD.cpp Compression/PPMD/C_PPMD.h Compression/PPMD/Model.cpp Compression/PPMD/SubAlloc.hpp Compression/PPMD/Coder.hpp Compression/PPMD/PPMd.h Compression/PPMD/PPMdType.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS_NO_SA) -o $@ $<

$(DEC_OBJDIR)/C_MM.o: Compression/MM/C_MM.cpp Compression/MM/C_MM.h Compression/MM/mm.cpp Compression/MM/mmdet.cpp Compression/MM/mmdet.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_TTA.o: Compression/MM/C_TTA.cpp Compression/MM/C_TTA.h Compression/MM/tta.cpp Compression/MM/filters.cpp Compression/MM/filters.h Compression/MM/entropy.cpp Compression/MM/entropy.h Compression/MM/mmdet.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS_NO_SA) -fwrapv -fno-lto -o $@ $<

$(DEC_OBJDIR)/C_Tornado.o: Compression/Tornado/C_Tornado.cpp Compression/Tornado/C_Tornado.h Compression/Tornado/Tornado.cpp Compression/Tornado/MatchFinder.cpp $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_GRZip.o: Compression/GRZip/C_GRZip.cpp Compression/GRZip/C_GRZip.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_4x4.o: Compression/4x4/C_4x4.cpp Compression/4x4/C_4x4.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_Encryption.o: Compression/_Encryption/C_Encryption.cpp Compression/_Encryption/C_Encryption.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -ICompression -ICompression/_Encryption -ICompression/_Encryption/headers -o $@ $<

# --- Linux arc (encoder) objects ---

$(ENC_OBJDIR):
	mkdir -p $(ENC_OBJDIR)

$(ENC_OBJDIR)/arc.o: arc.cpp Environment.h ArcIni.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/Environment.o: Environment.cpp Environment.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/CompressionLibrary.o: Compression/CompressionLibrary.cpp $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/Common.o: Compression/Common.cpp $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_External.o: Compression/External/C_External.cpp Compression/External/C_External.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_BCJ.o: Compression/LZMA2/C_BCJ.cpp Compression/LZMA2/C_BCJ.h Compression/LZMA2/C/Bra86.c Compression/LZMA2/C/Bra.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_LZMA.o: Compression/LZMA2/C_LZMA.cpp Compression/LZMA2/C_LZMA.h $(LZMA_ENC_DEPS) $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) $(MF_ASM_DEFS) -o $@ $<

$(ENC_OBJDIR)/C_Dict.o: Compression/Dict/C_Dict.cpp Compression/Dict/C_Dict.h Compression/Dict/dict.cpp $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_Delta.o: Compression/Delta/C_Delta.cpp Compression/Delta/C_Delta.h Compression/Delta/Delta.cpp $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_REP.o: Compression/REP/C_REP.cpp Compression/REP/C_REP.h Compression/REP/rep.cpp $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_LZP.o: Compression/LZP/C_LZP.cpp Compression/LZP/C_LZP.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_PPMD.o: Compression/PPMD/C_PPMD.cpp Compression/PPMD/C_PPMD.h Compression/PPMD/Model.cpp Compression/PPMD/SubAlloc.hpp Compression/PPMD/Coder.hpp Compression/PPMD/PPMd.h Compression/PPMD/PPMdType.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS_NO_SA) -o $@ $<

$(ENC_OBJDIR)/C_MM.o: Compression/MM/C_MM.cpp Compression/MM/C_MM.h Compression/MM/mm.cpp Compression/MM/mmdet.cpp Compression/MM/mmdet.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_TTA.o: Compression/MM/C_TTA.cpp Compression/MM/C_TTA.h Compression/MM/tta.cpp Compression/MM/filters.cpp Compression/MM/filters.h Compression/MM/entropy.cpp Compression/MM/entropy.h Compression/MM/mmdet.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS_NO_SA) -fwrapv -fno-lto -o $@ $<

$(ENC_OBJDIR)/C_Tornado.o: Compression/Tornado/C_Tornado.cpp Compression/Tornado/C_Tornado.h Compression/Tornado/Tornado.cpp Compression/Tornado/MatchFinder.cpp $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) $(MF_ASM_DEFS) -o $@ $<

$(ENC_OBJDIR)/C_GRZip.o: Compression/GRZip/C_GRZip.cpp Compression/GRZip/C_GRZip.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_4x4.o: Compression/4x4/C_4x4.cpp Compression/4x4/C_4x4.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_Encryption.o: Compression/_Encryption/C_Encryption.cpp Compression/_Encryption/C_Encryption.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -ICompression/_Encryption -ICompression/_Encryption/headers -o $@ $<

# ===================================================================
# Windows 64-bit Binaries (MinGW-w64)
# ===================================================================

arc.exe: $(ENC_WIN64_LINKOBJ)
	$(CXX_WIN64) $(ENC_WIN64_LINKOBJ) $(WIN64_LINK_FLAGS) -o $@

unarc.exe: $(DEC_WIN64_LINKOBJ)
	$(CXX_WIN64) $(DEC_WIN64_LINKOBJ) $(WIN64_LINK_FLAGS) -o $@

# --- Win64 unarc (decoder) objects ---

$(DEC_WIN64_OBJDIR):
	mkdir -p $(DEC_WIN64_OBJDIR)

$(DEC_WIN64_OBJDIR)/unarc.o: unarc.cpp $(UNARC_HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/Environment.o: Environment.cpp Environment.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/CompressionLibrary.o: Compression/CompressionLibrary.cpp $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/Common.o: Compression/Common.cpp $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_External.o: Compression/External/C_External.cpp Compression/External/C_External.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_BCJ.o: Compression/LZMA2/C_BCJ.cpp Compression/LZMA2/C_BCJ.h Compression/LZMA2/C/Bra86.c Compression/LZMA2/C/Bra.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_LZMA.o: Compression/LZMA2/C_LZMA.cpp Compression/LZMA2/C_LZMA.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_Dict.o: Compression/Dict/C_Dict.cpp Compression/Dict/C_Dict.h Compression/Dict/dict.cpp $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_Delta.o: Compression/Delta/C_Delta.cpp Compression/Delta/C_Delta.h Compression/Delta/Delta.cpp $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_REP.o: Compression/REP/C_REP.cpp Compression/REP/C_REP.h Compression/REP/rep.cpp $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_LZP.o: Compression/LZP/C_LZP.cpp Compression/LZP/C_LZP.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_PPMD.o: Compression/PPMD/C_PPMD.cpp Compression/PPMD/C_PPMD.h Compression/PPMD/Model.cpp Compression/PPMD/SubAlloc.hpp Compression/PPMD/Coder.hpp Compression/PPMD/PPMd.h Compression/PPMD/PPMdType.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS_NO_SA) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_MM.o: Compression/MM/C_MM.cpp Compression/MM/C_MM.h Compression/MM/mm.cpp Compression/MM/mmdet.cpp Compression/MM/mmdet.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_TTA.o: Compression/MM/C_TTA.cpp Compression/MM/C_TTA.h Compression/MM/tta.cpp Compression/MM/filters.cpp Compression/MM/filters.h Compression/MM/entropy.cpp Compression/MM/entropy.h Compression/MM/mmdet.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS_NO_SA) -fwrapv -fno-lto -o $@ $<

$(DEC_WIN64_OBJDIR)/C_Tornado.o: Compression/Tornado/C_Tornado.cpp Compression/Tornado/C_Tornado.h Compression/Tornado/Tornado.cpp Compression/Tornado/MatchFinder.cpp $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_GRZip.o: Compression/GRZip/C_GRZip.cpp Compression/GRZip/C_GRZip.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_4x4.o: Compression/4x4/C_4x4.cpp Compression/4x4/C_4x4.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_Encryption.o: Compression/_Encryption/C_Encryption.cpp Compression/_Encryption/C_Encryption.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -ICompression -ICompression/_Encryption -ICompression/_Encryption/headers -o $@ $<

# --- Win64 arc (encoder) objects ---

$(ENC_WIN64_OBJDIR):
	mkdir -p $(ENC_WIN64_OBJDIR)

$(ENC_WIN64_OBJDIR)/arc.o: arc.cpp Environment.h ArcIni.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/Environment.o: Environment.cpp Environment.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/CompressionLibrary.o: Compression/CompressionLibrary.cpp $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/Common.o: Compression/Common.cpp $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_External.o: Compression/External/C_External.cpp Compression/External/C_External.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_BCJ.o: Compression/LZMA2/C_BCJ.cpp Compression/LZMA2/C_BCJ.h Compression/LZMA2/C/Bra86.c Compression/LZMA2/C/Bra.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_LZMA.o: Compression/LZMA2/C_LZMA.cpp Compression/LZMA2/C_LZMA.h $(LZMA_ENC_DEPS) $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) $(MF_ASM_WIN64_DEFS) -DFREEARC_NO_MF_MT -o $@ $<

$(ENC_WIN64_OBJDIR)/C_Dict.o: Compression/Dict/C_Dict.cpp Compression/Dict/C_Dict.h Compression/Dict/dict.cpp $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_Delta.o: Compression/Delta/C_Delta.cpp Compression/Delta/C_Delta.h Compression/Delta/Delta.cpp $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_REP.o: Compression/REP/C_REP.cpp Compression/REP/C_REP.h Compression/REP/rep.cpp $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_LZP.o: Compression/LZP/C_LZP.cpp Compression/LZP/C_LZP.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_PPMD.o: Compression/PPMD/C_PPMD.cpp Compression/PPMD/C_PPMD.h Compression/PPMD/Model.cpp Compression/PPMD/SubAlloc.hpp Compression/PPMD/Coder.hpp Compression/PPMD/PPMd.h Compression/PPMD/PPMdType.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS_NO_SA) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_MM.o: Compression/MM/C_MM.cpp Compression/MM/C_MM.h Compression/MM/mm.cpp Compression/MM/mmdet.cpp Compression/MM/mmdet.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_TTA.o: Compression/MM/C_TTA.cpp Compression/MM/C_TTA.h Compression/MM/tta.cpp Compression/MM/filters.cpp Compression/MM/filters.h Compression/MM/entropy.cpp Compression/MM/entropy.h Compression/MM/mmdet.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS_NO_SA) -fwrapv -fno-lto -o $@ $<

$(ENC_WIN64_OBJDIR)/C_Tornado.o: Compression/Tornado/C_Tornado.cpp Compression/Tornado/C_Tornado.h Compression/Tornado/Tornado.cpp Compression/Tornado/MatchFinder.cpp $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) $(MF_ASM_WIN64_DEFS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_GRZip.o: Compression/GRZip/C_GRZip.cpp Compression/GRZip/C_GRZip.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_4x4.o: Compression/4x4/C_4x4.cpp Compression/4x4/C_4x4.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_Encryption.o: Compression/_Encryption/C_Encryption.cpp Compression/_Encryption/C_Encryption.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -ICompression/_Encryption -ICompression/_Encryption/headers -o $@ $<

# --- Vendored LZMA SDK 25.01 decoder (compiled as C; LzmaDec.c is #included by the unity wrapper) ---
$(DEC_OBJDIR)/lzma_fastdec.o: $(FASTDEC_DEPS) | $(DEC_OBJDIR)
	$(CC) -c -w $(MODE_CFLAGS) $(FASTDEC_DEFS) -o $@ $(FASTDEC_SRC)
$(ENC_OBJDIR)/lzma_fastdec.o: $(FASTDEC_DEPS) | $(ENC_OBJDIR)
	$(CC) -c -w $(MODE_CFLAGS) $(FASTDEC_DEFS) -o $@ $(FASTDEC_SRC)
$(ENC_OBJDIR)/lzma_fastenc.o: $(FASTENC_DEPS) | $(ENC_OBJDIR)
	$(CC) -c -w $(MODE_CFLAGS) $(MF_ASM_DEFS) -o $@ $(FASTENC_SRC)
$(DEC_OBJDIR)/LzmaDecOpt.o: Compression/LZMA2/sdk25/asm/LzmaDecOpt.S | $(DEC_OBJDIR)
	$(CC) -c -o $@ $<
$(ENC_OBJDIR)/LzmaDecOpt.o: Compression/LZMA2/sdk25/asm/LzmaDecOpt.S | $(ENC_OBJDIR)
	$(CC) -c -o $@ $<
$(DEC_WIN64_OBJDIR)/lzma_fastdec.o: $(FASTDEC_DEPS) | $(DEC_WIN64_OBJDIR)
	$(CC_WIN64) -c -w -O3 -DZ7_LZMA_DEC_OPT -o $@ $(FASTDEC_SRC)
$(ENC_WIN64_OBJDIR)/lzma_fastdec.o: $(FASTDEC_DEPS) | $(ENC_WIN64_OBJDIR)
	$(CC_WIN64) -c -w -O3 -DZ7_LZMA_DEC_OPT -o $@ $(FASTDEC_SRC)
$(ENC_WIN64_OBJDIR)/lzma_fastenc.o: $(FASTENC_DEPS) | $(ENC_WIN64_OBJDIR)
	$(CC_WIN64) -c -w -O3 $(MF_ASM_WIN64_DEFS) -o $@ $(FASTENC_SRC)
# --- x86-64 assembly match finders (encoder only) ---
$(ENC_OBJDIR)/fa_cpu_x86_64.o: Compression/asm/fa_cpu_x86_64.S $(MF_ASM_INC) | $(ENC_OBJDIR)
	$(CC) -c -o $@ $<
$(ENC_OBJDIR)/LzFind_x86_64.o: Compression/LZMA2/asm/LzFind_x86_64.S $(MF_ASM_INC) | $(ENC_OBJDIR)
	$(CC) -c -o $@ $<
$(ENC_OBJDIR)/LzFind25_x86_64.o: Compression/LZMA2/asm/LzFind25_x86_64.S $(MF_ASM_INC) | $(ENC_OBJDIR)
	$(CC) -c -o $@ $<
$(ENC_OBJDIR)/tor_match_x86_64.o: Compression/Tornado/asm/tor_match_x86_64.S $(MF_ASM_INC) | $(ENC_OBJDIR)
	$(CC) -c -o $@ $<
$(ENC_WIN64_OBJDIR)/fa_cpu_x86_64.o: Compression/asm/fa_cpu_x86_64.S $(MF_ASM_INC) | $(ENC_WIN64_OBJDIR)
	$(CC_WIN64) -c -o $@ $<
$(ENC_WIN64_OBJDIR)/LzFind_x86_64.o: Compression/LZMA2/asm/LzFind_x86_64.S $(MF_ASM_INC) | $(ENC_WIN64_OBJDIR)
	$(CC_WIN64) -c -o $@ $<
$(ENC_WIN64_OBJDIR)/LzFind25_x86_64.o: Compression/LZMA2/asm/LzFind25_x86_64.S $(MF_ASM_INC) | $(ENC_WIN64_OBJDIR)
	$(CC_WIN64) -c -o $@ $<
$(ENC_WIN64_OBJDIR)/tor_match_x86_64.o: Compression/Tornado/asm/tor_match_x86_64.S $(MF_ASM_INC) | $(ENC_WIN64_OBJDIR)
	$(CC_WIN64) -c -o $@ $<
$(DEC_WIN64_OBJDIR)/LzmaDecOpt.o: Compression/LZMA2/sdk25/asm/LzmaDecOpt.S | $(DEC_WIN64_OBJDIR)
	$(CC_WIN64) -c -o $@ $<
$(ENC_WIN64_OBJDIR)/LzmaDecOpt.o: Compression/LZMA2/sdk25/asm/LzmaDecOpt.S | $(ENC_WIN64_OBJDIR)
	$(CC_WIN64) -c -o $@ $<

# ===================================================================
# Linux ARM64 Binaries (aarch64 cross-compilation)
# ===================================================================

arc-arm64: $(ENC_ARM64_LINKOBJ)
	$(CXX_ARM64) $(ENC_ARM64_LINKOBJ) $(ARM64_LINK_FLAGS) -o $@

unarc-arm64: $(DEC_ARM64_LINKOBJ)
	$(CXX_ARM64) $(DEC_ARM64_LINKOBJ) $(ARM64_LINK_FLAGS) -o $@

arc.sfx-arm64: $(SFX_ARM64_LINKOBJ)
	$(CXX_ARM64) $(SFX_ARM64_LINKOBJ) $(ARM64_LINK_FLAGS) -o $@

# --- ARM64 unarc (decoder) objects ---

$(DEC_ARM64_OBJDIR):
	mkdir -p $(DEC_ARM64_OBJDIR)

$(DEC_ARM64_OBJDIR)/unarc.o: unarc.cpp $(UNARC_HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/unarc_sfx.o: unarc.cpp $(UNARC_HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -DFREEARC_SFX -o $@ $<

$(DEC_ARM64_OBJDIR)/Environment.o: Environment.cpp Environment.h $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/CompressionLibrary.o: Compression/CompressionLibrary.cpp $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/Common.o: Compression/Common.cpp $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/C_External.o: Compression/External/C_External.cpp Compression/External/C_External.h $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/C_BCJ.o: Compression/LZMA2/C_BCJ.cpp Compression/LZMA2/C_BCJ.h Compression/LZMA2/C/Bra86.c Compression/LZMA2/C/Bra.h $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/C_LZMA.o: Compression/LZMA2/C_LZMA.cpp Compression/LZMA2/C_LZMA.h $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/C_Dict.o: Compression/Dict/C_Dict.cpp Compression/Dict/C_Dict.h Compression/Dict/dict.cpp $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/C_Delta.o: Compression/Delta/C_Delta.cpp Compression/Delta/C_Delta.h Compression/Delta/Delta.cpp $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/C_REP.o: Compression/REP/C_REP.cpp Compression/REP/C_REP.h Compression/REP/rep.cpp $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/C_LZP.o: Compression/LZP/C_LZP.cpp Compression/LZP/C_LZP.h $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/C_PPMD.o: Compression/PPMD/C_PPMD.cpp Compression/PPMD/C_PPMD.h Compression/PPMD/Model.cpp Compression/PPMD/SubAlloc.hpp Compression/PPMD/Coder.hpp Compression/PPMD/PPMd.h Compression/PPMD/PPMdType.h $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS_NO_SA) -o $@ $<

$(DEC_ARM64_OBJDIR)/C_MM.o: Compression/MM/C_MM.cpp Compression/MM/C_MM.h Compression/MM/mm.cpp Compression/MM/mmdet.cpp Compression/MM/mmdet.h $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/C_TTA.o: Compression/MM/C_TTA.cpp Compression/MM/C_TTA.h Compression/MM/tta.cpp Compression/MM/filters.cpp Compression/MM/filters.h Compression/MM/entropy.cpp Compression/MM/entropy.h Compression/MM/mmdet.h $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS_NO_SA) -fwrapv -fno-lto -o $@ $<

$(DEC_ARM64_OBJDIR)/C_Tornado.o: Compression/Tornado/C_Tornado.cpp Compression/Tornado/C_Tornado.h Compression/Tornado/Tornado.cpp Compression/Tornado/MatchFinder.cpp $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/C_GRZip.o: Compression/GRZip/C_GRZip.cpp Compression/GRZip/C_GRZip.h $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/C_4x4.o: Compression/4x4/C_4x4.cpp Compression/4x4/C_4x4.h $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -o $@ $<

$(DEC_ARM64_OBJDIR)/C_Encryption.o: Compression/_Encryption/C_Encryption.cpp Compression/_Encryption/C_Encryption.h $(HEADERS) | $(DEC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_CFLAGS) -ICompression -ICompression/_Encryption -ICompression/_Encryption/headers -o $@ $<

# --- ARM64 arc (encoder) objects ---

$(ENC_ARM64_OBJDIR):
	mkdir -p $(ENC_ARM64_OBJDIR)

$(ENC_ARM64_OBJDIR)/arc.o: arc.cpp Environment.h ArcIni.h $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/Environment.o: Environment.cpp Environment.h $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/CompressionLibrary.o: Compression/CompressionLibrary.cpp $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/Common.o: Compression/Common.cpp $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/C_External.o: Compression/External/C_External.cpp Compression/External/C_External.h $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/C_BCJ.o: Compression/LZMA2/C_BCJ.cpp Compression/LZMA2/C_BCJ.h Compression/LZMA2/C/Bra86.c Compression/LZMA2/C/Bra.h $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/C_LZMA.o: Compression/LZMA2/C_LZMA.cpp Compression/LZMA2/C_LZMA.h $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/C_Dict.o: Compression/Dict/C_Dict.cpp Compression/Dict/C_Dict.h Compression/Dict/dict.cpp $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/C_Delta.o: Compression/Delta/C_Delta.cpp Compression/Delta/C_Delta.h Compression/Delta/Delta.cpp $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/C_REP.o: Compression/REP/C_REP.cpp Compression/REP/C_REP.h Compression/REP/rep.cpp $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/C_LZP.o: Compression/LZP/C_LZP.cpp Compression/LZP/C_LZP.h $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/C_PPMD.o: Compression/PPMD/C_PPMD.cpp Compression/PPMD/C_PPMD.h Compression/PPMD/Model.cpp Compression/PPMD/SubAlloc.hpp Compression/PPMD/Coder.hpp Compression/PPMD/PPMd.h Compression/PPMD/PPMdType.h $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS_NO_SA) -o $@ $<

$(ENC_ARM64_OBJDIR)/C_MM.o: Compression/MM/C_MM.cpp Compression/MM/C_MM.h Compression/MM/mm.cpp Compression/MM/mmdet.cpp Compression/MM/mmdet.h $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/C_TTA.o: Compression/MM/C_TTA.cpp Compression/MM/C_TTA.h Compression/MM/tta.cpp Compression/MM/filters.cpp Compression/MM/filters.h Compression/MM/entropy.cpp Compression/MM/entropy.h Compression/MM/mmdet.h $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS_NO_SA) -fwrapv -fno-lto -o $@ $<

$(ENC_ARM64_OBJDIR)/C_Tornado.o: Compression/Tornado/C_Tornado.cpp Compression/Tornado/C_Tornado.h Compression/Tornado/Tornado.cpp Compression/Tornado/MatchFinder.cpp $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/C_GRZip.o: Compression/GRZip/C_GRZip.cpp Compression/GRZip/C_GRZip.h $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/C_4x4.o: Compression/4x4/C_4x4.cpp Compression/4x4/C_4x4.h $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -o $@ $<

$(ENC_ARM64_OBJDIR)/C_Encryption.o: Compression/_Encryption/C_Encryption.cpp Compression/_Encryption/C_Encryption.h $(HEADERS) | $(ENC_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(ARM64_ENC_CFLAGS) -ICompression -ICompression/_Encryption -ICompression/_Encryption/headers -o $@ $<

$(DEC_ARM64_OBJDIR)/lzma_fastdec.o: $(FASTDEC_DEPS) | $(DEC_ARM64_OBJDIR)
	$(CC_ARM64) -c -w -O3 -o $@ $(FASTDEC_SRC)
$(ENC_ARM64_OBJDIR)/lzma_fastdec.o: $(FASTDEC_DEPS) | $(ENC_ARM64_OBJDIR)
	$(CC_ARM64) -c -w -O3 -o $@ $(FASTDEC_SRC)
$(ENC_ARM64_OBJDIR)/lzma_fastenc.o: $(FASTENC_DEPS) | $(ENC_ARM64_OBJDIR)
	$(CC_ARM64) -c -w -O3 -o $@ $(FASTENC_SRC)

# ===================================================================
# Decoder Libraries (libunarc.so / libunarc.a / unarc.dll) & RISC-V 64
# Strictly isolated in separate object directories with -DFREEARC_DLL
# ===================================================================

AR ?= ar
AR_ARM64 ?= aarch64-linux-gnu-ar
STRIP_ARM64 ?= aarch64-linux-gnu-strip
CXX_RISCV64 ?= riscv64-linux-gnu-g++
CC_RISCV64 ?= riscv64-linux-gnu-gcc
AR_RISCV64 ?= riscv64-linux-gnu-ar
STRIP_RISCV64 ?= riscv64-linux-gnu-strip

DEC_LIB_OBJDIR       = dec_lib
DEC_LIB_WIN64_OBJDIR = dec_lib_win64
DEC_LIB_ARM64_OBJDIR = dec_lib_arm64
DEC_RISCV64_OBJDIR   = dec_riscv64
ENC_RISCV64_OBJDIR   = enc_riscv64
DEC_LIB_RISCV64_OBJDIR = dec_lib_riscv64

LIB_DEFS = -DFREEARC_DECOMPRESS_ONLY -D_NO_EXCEPTIONS -DUNARC -DFREEARC_LIBRARY -DFREEARC_DLL -DFREEARC_DLL_EXPORTS

LIB_UNIX_CFLAGS       = $(CODE_FLAGS) -O3 -fPIC -ffunction-sections -fdata-sections -fvisibility=hidden -fno-math-errno -DNDEBUG $(DEFINES) $(LIB_DEFS) -I. -ICompression
LIB_UNIX_CFLAGS_NO_SA = $(LIB_UNIX_CFLAGS) -fno-strict-aliasing
LIB_WIN64_CFLAGS      = $(CODE_FLAGS) -O3 $(WIN64_DEFINES) $(LIB_DEFS) -I. -ICompression
LIB_WIN64_CFLAGS_NO_SA = $(LIB_WIN64_CFLAGS) -fno-strict-aliasing
LIB_ARM64_CFLAGS      = $(CODE_FLAGS) -O3 -fPIC -ffunction-sections -fdata-sections -fvisibility=hidden $(ARM64_DEFINES) $(LIB_DEFS) -I. -ICompression
LIB_ARM64_CFLAGS_NO_SA = $(LIB_ARM64_CFLAGS) -fno-strict-aliasing

RISCV64_DEFINES      = -DFREEARC_UNIX -DFREEARC_INTEL_BYTE_ORDER -DFREEARC_64BIT
RISCV64_CFLAGS       = $(CODE_FLAGS) -O3 $(RISCV64_DEFINES) $(Extractor_DEFINES) -I. -ICompression
RISCV64_CFLAGS_NO_SA = $(RISCV64_CFLAGS) -fno-strict-aliasing
RISCV64_ENC_CFLAGS   = $(CODE_FLAGS) -O3 $(RISCV64_DEFINES) -I. -ICompression
RISCV64_ENC_CFLAGS_NO_SA = $(RISCV64_ENC_CFLAGS) -fno-strict-aliasing
LIB_RISCV64_CFLAGS   = $(CODE_FLAGS) -O3 -fPIC -ffunction-sections -fdata-sections -fvisibility=hidden $(RISCV64_DEFINES) $(LIB_DEFS) -I. -ICompression
LIB_RISCV64_CFLAGS_NO_SA = $(LIB_RISCV64_CFLAGS) -fno-strict-aliasing
RISCV64_LINK_FLAGS   = -lstdc++ -lrt -lpthread -lm -Wl,-O1 -Wl,--gc-sections -Wl,--as-needed -Wl,--build-id=none -s

LIB_UNIX_SO_FLAGS    = -shared -lstdc++ -lrt -lpthread -lm -Wl,-O1 -Wl,--gc-sections -Wl,--as-needed -Wl,--build-id=none -Wl,-soname,libunarc.so
LIB_WIN64_DLL_FLAGS  = -shared -static -static-libgcc -static-libstdc++ -Wl,--out-implib,libunarc.dll.a -Wl,--exclude-libs,ALL -lkernel32 -luser32 -lshell32 -ladvapi32 -lole32 -s

define FREEARC_CODEC_RULES
$(1):
	mkdir -p $(1)
$(1)/Environment.o: Environment.cpp Environment.h $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/CompressionLibrary.o: Compression/CompressionLibrary.cpp $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/Common.o: Compression/Common.cpp $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/C_External.o: Compression/External/C_External.cpp Compression/External/C_External.h $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/C_BCJ.o: Compression/LZMA2/C_BCJ.cpp Compression/LZMA2/C_BCJ.h Compression/LZMA2/C/Bra86.c Compression/LZMA2/C/Bra.h $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/C_LZMA.o: Compression/LZMA2/C_LZMA.cpp Compression/LZMA2/C_LZMA.h $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/C_Dict.o: Compression/Dict/C_Dict.cpp Compression/Dict/C_Dict.h Compression/Dict/dict.cpp $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/C_Delta.o: Compression/Delta/C_Delta.cpp Compression/Delta/C_Delta.h Compression/Delta/Delta.cpp $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/C_REP.o: Compression/REP/C_REP.cpp Compression/REP/C_REP.h Compression/REP/rep.cpp $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/C_LZP.o: Compression/LZP/C_LZP.cpp Compression/LZP/C_LZP.h $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/C_PPMD.o: Compression/PPMD/C_PPMD.cpp Compression/PPMD/C_PPMD.h Compression/PPMD/Model.cpp Compression/PPMD/SubAlloc.hpp Compression/PPMD/Coder.hpp Compression/PPMD/PPMd.h Compression/PPMD/PPMdType.h $(HEADERS) | $(1)
	$(2) -c $(5) -o $$@ $$<
$(1)/C_MM.o: Compression/MM/C_MM.cpp Compression/MM/C_MM.h Compression/MM/mm.cpp Compression/MM/mmdet.cpp Compression/MM/mmdet.h $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/C_TTA.o: Compression/MM/C_TTA.cpp Compression/MM/C_TTA.h Compression/MM/tta.cpp Compression/MM/filters.cpp Compression/MM/filters.h Compression/MM/entropy.cpp Compression/MM/entropy.h Compression/MM/mmdet.h $(HEADERS) | $(1)
	$(2) -c $(5) -fwrapv -fno-lto -o $$@ $$<
$(1)/C_Tornado.o: Compression/Tornado/C_Tornado.cpp Compression/Tornado/C_Tornado.h Compression/Tornado/Tornado.cpp Compression/Tornado/MatchFinder.cpp $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/C_GRZip.o: Compression/GRZip/C_GRZip.cpp Compression/GRZip/C_GRZip.h $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/C_4x4.o: Compression/4x4/C_4x4.cpp Compression/4x4/C_4x4.h $(HEADERS) | $(1)
	$(2) -c $(4) -o $$@ $$<
$(1)/C_Encryption.o: Compression/_Encryption/C_Encryption.cpp Compression/_Encryption/C_Encryption.h $(HEADERS) | $(1)
	$(2) -c $(4) -ICompression/_Encryption -ICompression/_Encryption/headers -o $$@ $$<
$(1)/lzma_fastdec.o: $(FASTDEC_DEPS) | $(1)
	$(3) -c -w $(6) -o $$@ $(FASTDEC_SRC)
endef

define LIB_DEC_OBJECTS
$(1)/unarc_dll.o $(1)/Environment.o $(1)/Common.o $(1)/CompressionLibrary.o \
$(1)/C_LZMA.o $(1)/lzma_fastdec.o $(2) $(1)/C_BCJ.o $(1)/C_Dict.o \
$(1)/C_Delta.o $(1)/C_REP.o $(1)/C_LZP.o $(1)/C_PPMD.o $(1)/C_External.o \
$(1)/C_MM.o $(1)/C_TTA.o $(1)/C_Tornado.o $(1)/C_GRZip.o $(1)/C_4x4.o \
$(1)/C_Encryption.o
endef

# --- Linux x86_64 Library ---
ifeq ($(LZMA_ASM),1)
LIB_X64_ASM_OBJ = $(DEC_LIB_OBJDIR)/LzmaDecOpt.o
endif
DEC_LIB_LINKOBJ = $(call LIB_DEC_OBJECTS,$(DEC_LIB_OBJDIR),$(LIB_X64_ASM_OBJ))

$(eval $(call FREEARC_CODEC_RULES,$(DEC_LIB_OBJDIR),$(CXX),$(CC),$(LIB_UNIX_CFLAGS),$(LIB_UNIX_CFLAGS_NO_SA),-O3 -fPIC $(FASTDEC_DEFS)))
$(DEC_LIB_OBJDIR)/unarc_dll.o: unarc_dll.cpp unarc.h $(UNARC_HEADERS) | $(DEC_LIB_OBJDIR)
	$(CXX) -c $(LIB_UNIX_CFLAGS) -o $@ $<
$(DEC_LIB_OBJDIR)/LzmaDecOpt.o: Compression/LZMA2/sdk25/asm/LzmaDecOpt.S | $(DEC_LIB_OBJDIR)
	$(CC) -c -o $@ $<

libunarc.so: $(DEC_LIB_LINKOBJ)
	$(CXX) $(DEC_LIB_LINKOBJ) $(LIB_UNIX_SO_FLAGS) -o $@
	strip --strip-unneeded $@

libunarc.a: $(DEC_LIB_LINKOBJ)
	rm -f $@
	$(AR) rcs $@ $(DEC_LIB_LINKOBJ)

lib: libunarc.so libunarc.a
	@mkdir -p ../build/linux
	cp -f libunarc.so ../build/linux/libunarc.so
	cp -f libunarc.a ../build/linux/libunarc.a
	cp -f unarc.h ../build/linux/unarc.h
	@echo "=== Linux x86_64 decoder libraries updated in ../build/linux/ ==="
	@ls -lh ../build/linux/libunarc.so ../build/linux/libunarc.a ../build/linux/unarc.h

# --- Windows 64-bit DLL (MinGW-w64) ---
DEC_LIB_WIN64_LINKOBJ = $(call LIB_DEC_OBJECTS,$(DEC_LIB_WIN64_OBJDIR),$(DEC_LIB_WIN64_OBJDIR)/LzmaDecOpt.o)

$(eval $(call FREEARC_CODEC_RULES,$(DEC_LIB_WIN64_OBJDIR),$(CXX_WIN64),$(CC_WIN64),$(LIB_WIN64_CFLAGS),$(LIB_WIN64_CFLAGS_NO_SA),-O3 -DZ7_LZMA_DEC_OPT))
$(DEC_LIB_WIN64_OBJDIR)/unarc_dll.o: unarc_dll.cpp unarc.h $(UNARC_HEADERS) | $(DEC_LIB_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(LIB_WIN64_CFLAGS) -o $@ $<
$(DEC_LIB_WIN64_OBJDIR)/LzmaDecOpt.o: Compression/LZMA2/sdk25/asm/LzmaDecOpt.S | $(DEC_LIB_WIN64_OBJDIR)
	$(CC_WIN64) -c -o $@ $<

unarc.dll libunarc.dll.a: $(DEC_LIB_WIN64_LINKOBJ)
	$(CXX_WIN64) $(DEC_LIB_WIN64_LINKOBJ) $(LIB_WIN64_DLL_FLAGS) -o unarc.dll

dll lib-win64: unarc.dll libunarc.dll.a
	@mkdir -p ../build/win64
	cp -f unarc.dll ../build/win64/unarc.dll
	cp -f libunarc.dll.a ../build/win64/libunarc.dll.a
	cp -f unarc.h ../build/win64/unarc.h
	@echo "=== Windows 64-bit decoder DLL updated in ../build/win64/ ==="
	@ls -lh ../build/win64/unarc.dll ../build/win64/libunarc.dll.a ../build/win64/unarc.h

# --- Linux ARM64 Library ---
DEC_LIB_ARM64_LINKOBJ = $(call LIB_DEC_OBJECTS,$(DEC_LIB_ARM64_OBJDIR),)

$(eval $(call FREEARC_CODEC_RULES,$(DEC_LIB_ARM64_OBJDIR),$(CXX_ARM64),$(CC_ARM64),$(LIB_ARM64_CFLAGS),$(LIB_ARM64_CFLAGS_NO_SA),-O3 -fPIC))
$(DEC_LIB_ARM64_OBJDIR)/unarc_dll.o: unarc_dll.cpp unarc.h $(UNARC_HEADERS) | $(DEC_LIB_ARM64_OBJDIR)
	$(CXX_ARM64) -c $(LIB_ARM64_CFLAGS) -o $@ $<

libunarc-arm64.so: $(DEC_LIB_ARM64_LINKOBJ)
	$(CXX_ARM64) $(DEC_LIB_ARM64_LINKOBJ) $(LIB_UNIX_SO_FLAGS) -o $@
	$(STRIP_ARM64) --strip-unneeded $@

libunarc-arm64.a: $(DEC_LIB_ARM64_LINKOBJ)
	rm -f $@
	$(AR_ARM64) rcs $@ $(DEC_LIB_ARM64_LINKOBJ)

lib-arm64: libunarc-arm64.so libunarc-arm64.a
	@mkdir -p ../build/linux-arm64
	cp -f libunarc-arm64.so ../build/linux-arm64/libunarc.so
	cp -f libunarc-arm64.a ../build/linux-arm64/libunarc.a
	cp -f unarc.h ../build/linux-arm64/unarc.h
	@echo "=== Linux ARM64 decoder libraries updated in ../build/linux-arm64/ ==="
	@ls -lh ../build/linux-arm64/libunarc.so ../build/linux-arm64/libunarc.a ../build/linux-arm64/unarc.h

# --- Linux RISC-V 64 (riscv64) CLI & Libraries ---
DEC_RISCV64_LINKOBJ = $(subst $(DEC_LIB_RISCV64_OBJDIR),$(DEC_RISCV64_OBJDIR),$(subst unarc_dll.o,unarc.o,$(call LIB_DEC_OBJECTS,$(DEC_LIB_RISCV64_OBJDIR),)))
SFX_RISCV64_LINKOBJ = $(subst unarc.o,unarc_sfx.o,$(DEC_RISCV64_LINKOBJ))
ENC_RISCV64_LINKOBJ = $(ENC_RISCV64_OBJDIR)/arc.o $(subst $(DEC_RISCV64_OBJDIR),$(ENC_RISCV64_OBJDIR),$(filter-out $(DEC_RISCV64_OBJDIR)/unarc.o,$(DEC_RISCV64_LINKOBJ))) $(ENC_RISCV64_OBJDIR)/lzma_fastenc.o
DEC_LIB_RISCV64_LINKOBJ = $(call LIB_DEC_OBJECTS,$(DEC_LIB_RISCV64_OBJDIR),)

$(eval $(call FREEARC_CODEC_RULES,$(DEC_RISCV64_OBJDIR),$(CXX_RISCV64),$(CC_RISCV64),$(RISCV64_CFLAGS),$(RISCV64_CFLAGS_NO_SA),-O3))
$(DEC_RISCV64_OBJDIR)/unarc.o: unarc.cpp $(UNARC_HEADERS) | $(DEC_RISCV64_OBJDIR)
	$(CXX_RISCV64) -c $(RISCV64_CFLAGS) -o $@ $<
$(DEC_RISCV64_OBJDIR)/unarc_sfx.o: unarc.cpp $(UNARC_HEADERS) | $(DEC_RISCV64_OBJDIR)
	$(CXX_RISCV64) -c $(RISCV64_CFLAGS) -DFREEARC_SFX -o $@ $<

$(eval $(call FREEARC_CODEC_RULES,$(ENC_RISCV64_OBJDIR),$(CXX_RISCV64),$(CC_RISCV64),$(RISCV64_ENC_CFLAGS),$(RISCV64_ENC_CFLAGS_NO_SA),-O3))
$(ENC_RISCV64_OBJDIR)/arc.o: arc.cpp Environment.h ArcIni.h $(HEADERS) | $(ENC_RISCV64_OBJDIR)
	$(CXX_RISCV64) -c $(RISCV64_ENC_CFLAGS) -o $@ $<
$(ENC_RISCV64_OBJDIR)/lzma_fastenc.o: $(FASTENC_DEPS) | $(ENC_RISCV64_OBJDIR)
	$(CC_RISCV64) -c -w -O3 -o $@ $(FASTENC_SRC)

$(eval $(call FREEARC_CODEC_RULES,$(DEC_LIB_RISCV64_OBJDIR),$(CXX_RISCV64),$(CC_RISCV64),$(LIB_RISCV64_CFLAGS),$(LIB_RISCV64_CFLAGS_NO_SA),-O3 -fPIC))
$(DEC_LIB_RISCV64_OBJDIR)/unarc_dll.o: unarc_dll.cpp unarc.h $(UNARC_HEADERS) | $(DEC_LIB_RISCV64_OBJDIR)
	$(CXX_RISCV64) -c $(LIB_RISCV64_CFLAGS) -o $@ $<

info_riscv64:
	@echo "Building Linux RISC-V 64 targets (riscv64 cross-compilation)"

arc-riscv64: $(ENC_RISCV64_LINKOBJ)
	$(CXX_RISCV64) $(ENC_RISCV64_LINKOBJ) $(RISCV64_LINK_FLAGS) -o $@

unarc-riscv64: $(DEC_RISCV64_LINKOBJ)
	$(CXX_RISCV64) $(DEC_RISCV64_LINKOBJ) $(RISCV64_LINK_FLAGS) -o $@

arc.sfx-riscv64: $(SFX_RISCV64_LINKOBJ)
	$(CXX_RISCV64) $(SFX_RISCV64_LINKOBJ) $(RISCV64_LINK_FLAGS) -o $@

riscv64: info_riscv64 arc-riscv64 unarc-riscv64 arc.sfx-riscv64
	@mkdir -p ../build/linux-riscv64
	cp -f arc-riscv64 ../build/linux-riscv64/arc
	cp -f unarc-riscv64 ../build/linux-riscv64/unarc
	cp -f arc.sfx-riscv64 ../build/linux-riscv64/arc.sfx
	@if [ -f ../arc.ini.example ]; then cp -f ../arc.ini.example ../build/linux-riscv64/arc.ini.example; fi
	@echo "=== Linux RISC-V 64 binaries updated in ../build/linux-riscv64/ ==="

libunarc-riscv64.so: $(DEC_LIB_RISCV64_LINKOBJ)
	$(CXX_RISCV64) $(DEC_LIB_RISCV64_LINKOBJ) $(LIB_UNIX_SO_FLAGS) -o $@
	$(STRIP_RISCV64) --strip-unneeded $@

libunarc-riscv64.a: $(DEC_LIB_RISCV64_LINKOBJ)
	rm -f $@
	$(AR_RISCV64) rcs $@ $(DEC_LIB_RISCV64_LINKOBJ)

lib-riscv64: libunarc-riscv64.so libunarc-riscv64.a
	@mkdir -p ../build/linux-riscv64
	cp -f libunarc-riscv64.so ../build/linux-riscv64/libunarc.so
	cp -f libunarc-riscv64.a ../build/linux-riscv64/libunarc.a
	cp -f unarc.h ../build/linux-riscv64/unarc.h
	@echo "=== Linux RISC-V 64 decoder libraries updated in ../build/linux-riscv64/ ==="

lib-all: lib dll lib-arm64
	@if command -v $(CXX_RISCV64) >/dev/null 2>&1; then $(MAKE) lib-riscv64; else echo "Note: $(CXX_RISCV64) not installed; skipping lib-riscv64"; fi
