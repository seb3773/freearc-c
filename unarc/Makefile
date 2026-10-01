CXX ?= g++

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

DEC_OBJDIR = dec
ENC_OBJDIR = enc

HEADERS       = Compression/Compression.h Compression/Common.h
UNARC_HEADERS = ArcStructure.h ArcCommand.h ArcProcess.h CUI.h $(HEADERS)

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

.PHONY: all debug clean info install uninstall

all: info arc unarc arc.sfx
	@mkdir -p ../bin
	cp -f arc ../bin/arc
	cp -f unarc ../bin/unarc
	cp -f arc.sfx ../bin/arc.sfx
	@echo "=== Binaries updated in ../bin/ ==="
	@ls -lh ../bin/arc ../bin/unarc ../bin/arc.sfx

info:
	@echo "Building with $(MODE_DESC)"

debug:
	$(MAKE) DEBUG=1

clean:
	rm -rf arc unarc arc.sfx $(DEC_OBJDIR) $(ENC_OBJDIR) *.o

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

# --- Build rules for unarc (decoder) ---

arc: $(ENC_LINKOBJ)
	$(CXX) $(ENC_LINKOBJ) $(UNIX_LINK_FLAGS) -o $@
	@$(STRIP_CMD)

unarc: $(DEC_LINKOBJ)
	$(CXX) $(DEC_LINKOBJ) $(UNIX_LINK_FLAGS) -o $@
	@$(STRIP_CMD)

arc.sfx: $(SFX_LINKOBJ)
	$(CXX) $(SFX_LINKOBJ) $(UNIX_LINK_FLAGS) -o $@
	@$(STRIP_CMD)

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


# --- Build rules for arc (encoder) ---

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
