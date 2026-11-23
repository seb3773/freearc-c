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

.PHONY: all win64 arm64 both all-platforms debug clean info info_win64 info_arm64 install uninstall

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
	rm -rf arc unarc arc.sfx arc.exe unarc.exe arc-arm64 unarc-arm64 arc.sfx-arm64 $(DEC_OBJDIR) $(ENC_OBJDIR) $(DEC_WIN64_OBJDIR) $(ENC_WIN64_OBJDIR) $(DEC_ARM64_OBJDIR) $(ENC_ARM64_OBJDIR) *.o

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
