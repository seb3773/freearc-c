CXX ?= g++
CXX_WIN64 ?= x86_64-w64-mingw32-g++

# Build mode configuration:
#   make (or make DEBUG=0) -> Production build: aggressive optimization (-O2, LTO, gc-sections, sstrip)
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
    MODE_DESC    = [PRODUCTION BUILD - Aggressive optimization + LTO + gc-sections + sstrip]
    MODE_CFLAGS  = -O2 -flto=auto -fstrict-aliasing \
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

UNIX_LINK_FLAGS = -lstdc++ -lrt -lpthread $(MODE_LDFLAGS)

CFLAGS           = $(CODE_FLAGS) $(MODE_CFLAGS) $(DEFINES) $(Extractor_DEFINES)
CFLAGS_NO_SA     = $(CODE_FLAGS) $(MODE_CFLAGS) $(DEFINES) $(Extractor_DEFINES) -fno-strict-aliasing
ENC_CFLAGS       = $(CODE_FLAGS) $(MODE_CFLAGS) $(DEFINES) -ICompression
ENC_CFLAGS_NO_SA = $(CODE_FLAGS) $(MODE_CFLAGS) $(DEFINES) -ICompression -fno-strict-aliasing

# --- Win64 configuration ---
WIN64_DEFINES = -DFREEARC_WIN -DFREEARC_INTEL_BYTE_ORDER -DFREEARC_64BIT
WIN64_CFLAGS           = $(CODE_FLAGS) -O2 $(WIN64_DEFINES) $(Extractor_DEFINES) -I. -ICompression
WIN64_CFLAGS_NO_SA     = $(CODE_FLAGS) -O2 $(WIN64_DEFINES) $(Extractor_DEFINES) -I. -ICompression -fno-strict-aliasing
WIN64_ENC_CFLAGS       = $(CODE_FLAGS) -O2 $(WIN64_DEFINES) -I. -ICompression
WIN64_ENC_CFLAGS_NO_SA = $(CODE_FLAGS) -O2 $(WIN64_DEFINES) -I. -ICompression -fno-strict-aliasing
WIN64_LINK_FLAGS       = -static -static-libgcc -static-libstdc++ -lkernel32 -luser32 -lshell32 -ladvapi32 -lole32 -s

DEC_OBJDIR       = dec
ENC_OBJDIR       = enc
DEC_WIN64_OBJDIR = dec_win64
ENC_WIN64_OBJDIR = enc_win64

HEADERS       = Compression/Compression.h Compression/Common.h
UNARC_HEADERS = ArcStructure.h ArcCommand.h ArcProcess.h CUI.h WinCompat.h $(HEADERS)

DEC_LINKOBJ = $(DEC_OBJDIR)/unarc.o $(DEC_OBJDIR)/Environment.o $(DEC_OBJDIR)/Common.o \
              $(DEC_OBJDIR)/CompressionLibrary.o $(DEC_OBJDIR)/C_LZMA.o $(DEC_OBJDIR)/C_BCJ.o \
              $(DEC_OBJDIR)/C_Dict.o $(DEC_OBJDIR)/C_Delta.o $(DEC_OBJDIR)/C_REP.o \
              $(DEC_OBJDIR)/C_LZP.o $(DEC_OBJDIR)/C_PPMD.o $(DEC_OBJDIR)/C_External.o \
              $(DEC_OBJDIR)/C_MM.o $(DEC_OBJDIR)/C_TTA.o $(DEC_OBJDIR)/C_Tornado.o \
              $(DEC_OBJDIR)/C_GRZip.o $(DEC_OBJDIR)/C_Encryption.o

SFX_LINKOBJ = $(DEC_OBJDIR)/unarc_sfx.o $(DEC_OBJDIR)/Environment.o $(DEC_OBJDIR)/Common.o \
              $(DEC_OBJDIR)/CompressionLibrary.o $(DEC_OBJDIR)/C_LZMA.o $(DEC_OBJDIR)/C_BCJ.o \
              $(DEC_OBJDIR)/C_Dict.o $(DEC_OBJDIR)/C_Delta.o $(DEC_OBJDIR)/C_REP.o \
              $(DEC_OBJDIR)/C_LZP.o $(DEC_OBJDIR)/C_PPMD.o $(DEC_OBJDIR)/C_External.o \
              $(DEC_OBJDIR)/C_MM.o $(DEC_OBJDIR)/C_TTA.o $(DEC_OBJDIR)/C_Tornado.o \
              $(DEC_OBJDIR)/C_GRZip.o $(DEC_OBJDIR)/C_Encryption.o

ENC_LINKOBJ = $(ENC_OBJDIR)/arc.o $(ENC_OBJDIR)/Environment.o $(ENC_OBJDIR)/Common.o \
              $(ENC_OBJDIR)/CompressionLibrary.o $(ENC_OBJDIR)/C_LZMA.o $(ENC_OBJDIR)/C_BCJ.o \
              $(ENC_OBJDIR)/C_Dict.o $(ENC_OBJDIR)/C_Delta.o $(ENC_OBJDIR)/C_REP.o \
              $(ENC_OBJDIR)/C_LZP.o $(ENC_OBJDIR)/C_PPMD.o $(ENC_OBJDIR)/C_External.o \
              $(ENC_OBJDIR)/C_MM.o $(ENC_OBJDIR)/C_TTA.o $(ENC_OBJDIR)/C_Tornado.o \
              $(ENC_OBJDIR)/C_GRZip.o $(ENC_OBJDIR)/C_Encryption.o

DEC_WIN64_LINKOBJ = $(DEC_WIN64_OBJDIR)/unarc.o $(DEC_WIN64_OBJDIR)/Environment.o $(DEC_WIN64_OBJDIR)/Common.o \
                    $(DEC_WIN64_OBJDIR)/CompressionLibrary.o $(DEC_WIN64_OBJDIR)/C_LZMA.o $(DEC_WIN64_OBJDIR)/C_BCJ.o \
                    $(DEC_WIN64_OBJDIR)/C_Dict.o $(DEC_WIN64_OBJDIR)/C_Delta.o $(DEC_WIN64_OBJDIR)/C_REP.o \
                    $(DEC_WIN64_OBJDIR)/C_LZP.o $(DEC_WIN64_OBJDIR)/C_PPMD.o $(DEC_WIN64_OBJDIR)/C_External.o \
                    $(DEC_WIN64_OBJDIR)/C_MM.o $(DEC_WIN64_OBJDIR)/C_TTA.o $(DEC_WIN64_OBJDIR)/C_Tornado.o \
                    $(DEC_WIN64_OBJDIR)/C_GRZip.o $(DEC_WIN64_OBJDIR)/C_Encryption.o

ENC_WIN64_LINKOBJ = $(ENC_WIN64_OBJDIR)/arc.o $(ENC_WIN64_OBJDIR)/Environment.o $(ENC_WIN64_OBJDIR)/Common.o \
                    $(ENC_WIN64_OBJDIR)/CompressionLibrary.o $(ENC_WIN64_OBJDIR)/C_LZMA.o $(ENC_WIN64_OBJDIR)/C_BCJ.o \
                    $(ENC_WIN64_OBJDIR)/C_Dict.o $(ENC_WIN64_OBJDIR)/C_Delta.o $(ENC_WIN64_OBJDIR)/C_REP.o \
                    $(ENC_WIN64_OBJDIR)/C_LZP.o $(ENC_WIN64_OBJDIR)/C_PPMD.o $(ENC_WIN64_OBJDIR)/C_External.o \
                    $(ENC_WIN64_OBJDIR)/C_MM.o $(ENC_WIN64_OBJDIR)/C_TTA.o $(ENC_WIN64_OBJDIR)/C_Tornado.o \
                    $(ENC_WIN64_OBJDIR)/C_GRZip.o $(ENC_WIN64_OBJDIR)/C_Encryption.o

.PHONY: all win64 both debug clean info info_win64 install uninstall

all: info arc unarc arc.sfx
	@mkdir -p ../build/linux
	cp -f arc ../build/linux/arc
	cp -f unarc ../build/linux/unarc
	cp -f arc.sfx ../build/linux/arc.sfx
	@echo "=== Linux binaries updated in ../build/linux/ ==="
	@ls -lh ../build/linux/arc ../build/linux/unarc ../build/linux/arc.sfx

win64: info_win64 arc.exe unarc.exe
	@mkdir -p ../build/win64
	cp -f arc.exe ../build/win64/arc.exe
	cp -f unarc.exe ../build/win64/unarc.exe
	@echo "=== Windows 64-bit binaries updated in ../build/win64/ ==="
	@ls -lh ../build/win64/arc.exe ../build/win64/unarc.exe

both: all win64

info:
	@echo "Building Linux targets with $(MODE_DESC)"

info_win64:
	@echo "Building Windows 64-bit targets (MinGW-w64 static)"

debug:
	$(MAKE) DEBUG=1

clean:
	rm -rf arc unarc arc.sfx arc.exe unarc.exe $(DEC_OBJDIR) $(ENC_OBJDIR) $(DEC_WIN64_OBJDIR) $(ENC_WIN64_OBJDIR) *.o

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

$(DEC_OBJDIR)/C_BCJ.o: Compression/LZMA2/C_BCJ.cpp Compression/LZMA2/C_BCJ.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_LZMA.o: Compression/LZMA2/C_LZMA.cpp Compression/LZMA2/C_LZMA.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_Dict.o: Compression/Dict/C_Dict.cpp Compression/Dict/C_Dict.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_Delta.o: Compression/Delta/C_Delta.cpp Compression/Delta/C_Delta.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_REP.o: Compression/REP/C_REP.cpp Compression/REP/C_REP.h Compression/REP/rep.cpp $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_LZP.o: Compression/LZP/C_LZP.cpp Compression/LZP/C_LZP.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_PPMD.o: Compression/PPMD/C_PPMD.cpp Compression/PPMD/C_PPMD.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS_NO_SA) -o $@ $<

$(DEC_OBJDIR)/C_MM.o: Compression/MM/C_MM.cpp Compression/MM/C_MM.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_TTA.o: Compression/MM/C_TTA.cpp Compression/MM/C_TTA.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_Tornado.o: Compression/Tornado/C_Tornado.cpp Compression/Tornado/C_Tornado.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_GRZip.o: Compression/GRZip/C_GRZip.cpp Compression/GRZip/C_GRZip.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<

$(DEC_OBJDIR)/C_Encryption.o: Compression/_Encryption/C_Encryption.cpp Compression/_Encryption/C_Encryption.h $(HEADERS) | $(DEC_OBJDIR)
	$(CXX) -c $(CFLAGS) -ICompression -ICompression/_Encryption -ICompression/_Encryption/headers -o $@ $<

# --- Linux arc (encoder) objects ---

$(ENC_OBJDIR):
	mkdir -p $(ENC_OBJDIR)

$(ENC_OBJDIR)/arc.o: arc.cpp Environment.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/Environment.o: Environment.cpp Environment.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/CompressionLibrary.o: Compression/CompressionLibrary.cpp $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/Common.o: Compression/Common.cpp $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_External.o: Compression/External/C_External.cpp Compression/External/C_External.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_BCJ.o: Compression/LZMA2/C_BCJ.cpp Compression/LZMA2/C_BCJ.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_LZMA.o: Compression/LZMA2/C_LZMA.cpp Compression/LZMA2/C_LZMA.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -DFREEARC_NO_MF_MT -o $@ $<

$(ENC_OBJDIR)/C_Dict.o: Compression/Dict/C_Dict.cpp Compression/Dict/C_Dict.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_Delta.o: Compression/Delta/C_Delta.cpp Compression/Delta/C_Delta.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_REP.o: Compression/REP/C_REP.cpp Compression/REP/C_REP.h Compression/REP/rep.cpp $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_LZP.o: Compression/LZP/C_LZP.cpp Compression/LZP/C_LZP.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_PPMD.o: Compression/PPMD/C_PPMD.cpp Compression/PPMD/C_PPMD.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS_NO_SA) -o $@ $<

$(ENC_OBJDIR)/C_MM.o: Compression/MM/C_MM.cpp Compression/MM/C_MM.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_TTA.o: Compression/MM/C_TTA.cpp Compression/MM/C_TTA.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_Tornado.o: Compression/Tornado/C_Tornado.cpp Compression/Tornado/C_Tornado.h $(HEADERS) | $(ENC_OBJDIR)
	$(CXX) -c $(ENC_CFLAGS) -o $@ $<

$(ENC_OBJDIR)/C_GRZip.o: Compression/GRZip/C_GRZip.cpp Compression/GRZip/C_GRZip.h $(HEADERS) | $(ENC_OBJDIR)
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

$(DEC_WIN64_OBJDIR)/C_BCJ.o: Compression/LZMA2/C_BCJ.cpp Compression/LZMA2/C_BCJ.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_LZMA.o: Compression/LZMA2/C_LZMA.cpp Compression/LZMA2/C_LZMA.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_Dict.o: Compression/Dict/C_Dict.cpp Compression/Dict/C_Dict.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_Delta.o: Compression/Delta/C_Delta.cpp Compression/Delta/C_Delta.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_REP.o: Compression/REP/C_REP.cpp Compression/REP/C_REP.h Compression/REP/rep.cpp $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_LZP.o: Compression/LZP/C_LZP.cpp Compression/LZP/C_LZP.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_PPMD.o: Compression/PPMD/C_PPMD.cpp Compression/PPMD/C_PPMD.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS_NO_SA) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_MM.o: Compression/MM/C_MM.cpp Compression/MM/C_MM.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_TTA.o: Compression/MM/C_TTA.cpp Compression/MM/C_TTA.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_Tornado.o: Compression/Tornado/C_Tornado.cpp Compression/Tornado/C_Tornado.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_GRZip.o: Compression/GRZip/C_GRZip.cpp Compression/GRZip/C_GRZip.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -o $@ $<

$(DEC_WIN64_OBJDIR)/C_Encryption.o: Compression/_Encryption/C_Encryption.cpp Compression/_Encryption/C_Encryption.h $(HEADERS) | $(DEC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_CFLAGS) -ICompression -ICompression/_Encryption -ICompression/_Encryption/headers -o $@ $<

# --- Win64 arc (encoder) objects ---

$(ENC_WIN64_OBJDIR):
	mkdir -p $(ENC_WIN64_OBJDIR)

$(ENC_WIN64_OBJDIR)/arc.o: arc.cpp Environment.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/Environment.o: Environment.cpp Environment.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/CompressionLibrary.o: Compression/CompressionLibrary.cpp $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/Common.o: Compression/Common.cpp $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_External.o: Compression/External/C_External.cpp Compression/External/C_External.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_BCJ.o: Compression/LZMA2/C_BCJ.cpp Compression/LZMA2/C_BCJ.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_LZMA.o: Compression/LZMA2/C_LZMA.cpp Compression/LZMA2/C_LZMA.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -DFREEARC_NO_MF_MT -o $@ $<

$(ENC_WIN64_OBJDIR)/C_Dict.o: Compression/Dict/C_Dict.cpp Compression/Dict/C_Dict.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_Delta.o: Compression/Delta/C_Delta.cpp Compression/Delta/C_Delta.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_REP.o: Compression/REP/C_REP.cpp Compression/REP/C_REP.h Compression/REP/rep.cpp $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_LZP.o: Compression/LZP/C_LZP.cpp Compression/LZP/C_LZP.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_PPMD.o: Compression/PPMD/C_PPMD.cpp Compression/PPMD/C_PPMD.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS_NO_SA) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_MM.o: Compression/MM/C_MM.cpp Compression/MM/C_MM.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_TTA.o: Compression/MM/C_TTA.cpp Compression/MM/C_TTA.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_Tornado.o: Compression/Tornado/C_Tornado.cpp Compression/Tornado/C_Tornado.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_GRZip.o: Compression/GRZip/C_GRZip.cpp Compression/GRZip/C_GRZip.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -o $@ $<

$(ENC_WIN64_OBJDIR)/C_Encryption.o: Compression/_Encryption/C_Encryption.cpp Compression/_Encryption/C_Encryption.h $(HEADERS) | $(ENC_WIN64_OBJDIR)
	$(CXX_WIN64) -c $(WIN64_ENC_CFLAGS) -ICompression/_Encryption -ICompression/_Encryption/headers -o $@ $<
