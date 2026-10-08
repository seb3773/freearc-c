# FreeArc Native C/C++


A modern, standalone, **100% native C/C++** implementation of the **FreeArc** (`.arc`) archive format.

---

## Background & Motivation

### The Missing C/C++ Archiver
Historically, FreeArc's archive encoder (`arc`) **never existed in C or C++**.

The original FreeArc project created by Bulat Ziganshin was divided into two fundamentally different parts:
* **The Low-Level Codecs (`Compression/`) & Extractor (`unarc`)**: Written in C/C++, serving exclusively as backend filter libraries (compiled as DLLs/SOs) and a lightweight extraction utility (`unarc`). The C/C++ codebase contained **zero archive creation, block generation, or file management logic**.
* **The Archiver (`arc`)**: A massive monolithic program written in **Haskell** (over 40,000 lines of code across `Arc.hs`, `Package.hs`, etc.). All core archiving logic—block layout, directory structuring, file sorting, solid stream grouping, CLI parsing, and encryption management—was implemented purely in Haskell and compiled with GHC 6.10 / 6.12 under 32-bit Windows back in 2008–2010.

Because the original Haskell codebase depends strictly on legacy GHC 6.x compilers from over 15 years ago, compiling it on modern Linux distributions (Debian 12, Arch, Ubuntu, Fedora) is practically impossible due to deep compiler bitrot and incompatible runtime dependencies.

### Prior Attempts
* **`mirror/freearc`**: A Git mirror of the original SourceForge SVN repository (frozen around 2010). ~90% of the archiver codebase is 32-bit Haskell. The C/C++ sources only build the extraction tool (`unarc`) and standalone filter DLLs.
* **`despawnerer/freearc`**: An attempt to make the legacy Haskell codebase compile with recent GHC on macOS. However, archive extraction was left incomplete (*"[ ] Extracting existing FreeArc archives (not done)"*), and it still requires a heavy Haskell / Cabal toolchain.
* **`Bulat-Ziganshin/FA` ("FreeArc Next")**: Around 2015, Bulat Ziganshin started an experimental C++ rewrite, but abandoned the project before it reached a functional archiver.

### The 15-Year Linux Dilemma
Consequently, to create or maintain FreeArc archives on Linux, users and developers were left with only two choices:
1. Run the legacy 32-bit Windows binary (`arc.exe`) through **Wine**.
2. Set up an antiquated 32-bit Linux VM or chroot with a 2008-era **GHC 6** Haskell environment.

### The Solution: A Pure C/C++ Native Re-engineering
This project fills this 15-year gap by providing a **100% native C/C++ suite** that is modern, autonomous, and fast:
* **Zero Haskell, Zero GHC, Zero Wine**: Pure standard C/C++ that compiles cleanly with standard GCC or Clang on any modern Linux system.
* **Zero External Dependencies**: Produces lean, self-contained binaries (< 1 MB total suite: `arc` ~427 KB, `unarc` ~197 KB, `arc.sfx` ~194 KB).
* **Total Binary Format Parity**: Strictly generates standard compliant blocks (`HEADER_BLOCK`, `DATA_BLOCK`, `DIR_BLOCK`, `FOOTER_BLOCK`) with bit-for-bit CRC32 accuracy, fully interoperable with legacy FreeArc tools.
* **Historical 64-bit Bug Fixes**: Porting and running the codecs on modern 64-bit Linux exposed several critical bugs that were never addressed in legacy FreeArc:
  * 64-bit pointer truncations in **LZP** and **GRZip**.
  * `unsigned long` 64-bit struct member alignment errors in the **Multimedia (MM)** filter WAVE headers.
  * Uninitialized prefix tree pointers causing segmentation faults in **LZMA**.
  * Modernized memory management and strict alignment handling across all compression engines.
* **Integrated Native Cryptography**: Native C++ implementations of AES (128/256-bit), Blowfish, Twofish, and Serpent with header encryption (`-hp`) and data block encryption (`-p`).
* **Standalone Linux SFX**: Autonomous self-extracting executable engine producing `.sfx` and `.run` payloads without external dependencies.

---

## Highlights

* **100% Native C/C++**: Zero Haskell, zero GHC, zero Wine, zero external library dependencies. Compiles cleanly with standard GCC/Clang on modern Linux.
* **Full Archive Management**: Complete command set — add (`a`), update (`u`), freshen (`f`), delete (`d`), move (`m`), list (`l`/`v`), test (`t`), extract (`x`/`e`), and pipe (`p`/`-so`).
* **High-Ratio Codecs**: Full suite of FreeArc compression algorithms:
  * **LZMA** (fast to maximum levels: `-m1` to `-m5x`)
  * **Tornado** (ultra-fast LZ77 variant)
  * **PPMD** (high-order prediction by partial matching)
  * **GRZip** (Burrows-Wheeler transform compression)
  * **TTA** (lossless true audio compression)
  * **Storing** (uncompressed store)
* **Intelligent Preprocessors & Filters**:
  * **BCJ / EXE**: x86 32/64-bit branch target preprocessor (`-bcj` / `-exe`)
  * **REP**: High-speed multi-megabyte repetition filter (`-rep`)
  * **DELTA**: Multi-byte differential filter (`-delta`)
  * **DICT**: Text dictionary preprocessor (`-dict`)
  * **LZP**: Lempel-Ziv + Prediction filter (`-lzp`)
  * **MM**: Multimedia 8/16/24/32-bit channel interleaved preprocessor (`-mm`)
* **Strong Encryption**:
  * AES (128/256-bit), Blowfish, Twofish, and Serpent.
  * Encrypt data blocks (`-p`) or full header + data (`-hp`).
* **Autonomous Linux SFX Engine**:
  * Create standalone `.sfx` or `.run` executables (`arc a -sfx setup.run ...`).
  * Direct archive conversion (`arc s archive.arc installer.run`).
  * Concatenation support (`cat build/linux/arc.sfx my.arc > my.run && chmod +x my.run`).
* **Full POSIX Fidelity**:
  * Preserves exact Unix file modes and permissions (`chmod 0755`, etc.).
  * Restores symbolic links (`symlink -> target`) transparently.
  * Deep directory hierarchies, wildcard patterns, and `@listfile` support.
* **Unix Pipelines**:
  * Decompress single or multiple files directly to standard output (`arc p`, `unarc p`, `-so`).
* **100% Backward Compatibility**:
  * Bit-for-bit CRC32 verified against legacy archives created by original FreeArc 0.6x Windows tools.

---

## Generated Binaries

Binaries are organized cleanly by target platform:

### Linux (x86_64) — `./build/linux/`
| Binary | Size | Description |
|---|---|---|
| `build/linux/arc` | ~427 KB | Full archiver and archive manager (create, update, freshen, delete, move, SFX conversion) |
| `build/linux/unarc` | ~197 KB | Standalone unpacker, lister, and integrity tester |
| `build/linux/arc.sfx` | ~194 KB | Standalone Linux ELF stub for self-extracting packages (`.sfx` / `.run`) |

### Windows (x86_64) — `./build/win64/`
| Binary | Size | Description |
|---|---|---|
| `build/win64/arc.exe` | ~682 KB | Standalone Windows 64-bit archiver (static MinGW, zero runtime DLL dependencies) |
| `build/win64/unarc.exe` | ~429 KB | Standalone Windows 64-bit unpacker and integrity tester |

---

## Quick Start

### Build from Source

```bash
# Clone the repository
git clone https://github.com/seb3773/freearc-c.git
cd freearc-c

# Compile Linux production binaries (into ./build/linux/)
make

# Compile Windows 64-bit native binaries (into ./build/win64/)
make win64

# (Or compile both simultaneously)
make both

# Run the automated Linux validation suite (23 tests)
make test

# Run the automated Windows 64-bit validation suite (23 tests under Wine, bit-for-bit exact)
make test-win64

# Run both test suites
make test-all

# (Optional) Build standalone Debian .deb package
make deb
```

### Install

```bash
sudo make install
```
By default, binaries are installed to `/usr/local/bin` (`arc`, `unarc`) and `/usr/local/lib/freearc` (`arc.sfx`). To customize:
```bash
sudo make install PREFIX=/usr
```

---

## Command-Line Usage

### Creating Archives

```bash
# Create a standard solid archive with default compression (LZMA)
arc a backup.arc documents/

# Maximum compression with executable and dictionary filters
arc a -m5 -bcj -dict project.arc src/

# Using specific codecs: Tornado, PPMD, or GRZip
arc a -m tor fast.arc logs/
arc a -m ppmd text.arc books/
arc a -m grzip data.arc files/

# Encrypted archive (AES-256 with hidden directory listing)
arc a -hp "MySecretPassword" private.arc finance/
```

### Updating & Managing Archives

```bash
# Update newer files and add new files
arc u project.arc src/

# Freshen existing files only (no new files added)
arc f project.arc src/

# Delete files matching a pattern from the archive
arc d project.arc "*.tmp" "build/*"

# Move files to archive (removes source files after archiving)
arc m archive.arc old_logs/
```

### Self-Extracting Executables (SFX)

```bash
# Create a standalone self-extracting Linux executable directly
arc a -sfx setup.run app_payload/

# Run the installer anywhere (extracts autonomously)
./setup.run -d /opt/my_app

# Convert an existing .arc archive to an SFX executable
arc s package.arc package.run
```

### Extracting & Listing

```bash
# List archive contents
unarc l backup.arc

# Verbose listing with Unix permissions, packed sizes, and CRC32
unarc v backup.arc

# Test archive integrity
unarc t backup.arc

# Extract with full directory tree
unarc x -dp /destination/path backup.arc

# Extract flat (without directory paths)
unarc e -dp /destination/path backup.arc

# Extract a file directly to stdout (pipeline)
unarc p backup.arc README.txt | grep "version"
```

---

## Project Structure

```
.
├── build/
│   ├── linux/           # Linux 64-bit binaries (arc, unarc, arc.sfx)
│   └── win64/           # Windows 64-bit binaries (arc.exe, unarc.exe)
├── build.sh             # Fast multi-mode build script
├── build_deb.sh         # Complete build + test + Debian .deb packager
├── Makefile             # Root Makefile (all, win64, both, debug, clean, test, test-win64, deb, install)
├── README.md            # Project documentation
├── FORMAT.md            # FreeArc .arc container technical specification
├── LICENSE              # GPL-2.0 License
├── man/                 # Standard roff manual pages (arc.1, unarc.1)
├── src/                 # Native C/C++ source code, headers, and codecs
│   ├── arc.cpp          # Archiver CLI & management logic
│   ├── unarc.cpp        # Unpacker CLI & extraction logic
│   ├── ArcStructure.h   # FreeArc archive format structures & parser
│   ├── ArcProcess.h     # High-level archive processing routines
│   ├── ArcCommand.h     # CLI options parser & dispatcher
│   ├── WinCompat.h      # Windows 64-bit POSIX compatibility shims
│   └── Compression/     # Compression codecs (LZMA, PPMD, GRZip, Tornado, TTA, Filters)
├── tests/
│   ├── run_tests.sh       # Comprehensive Linux automated test suite (23 tests)
│   └── run_tests_win64.sh # Automated Windows 64-bit & cross-platform test suite (23 tests)
└── tests_files/         # Test artifacts
    ├── corpus/          # Standard test corpus (binaries, text, code, nested dirs)
    └── samples_legacy/  # Legacy FreeArc .arc archives (example.arc, data2.arc)
```

---

## Debian / Ubuntu Packaging

Build a standard `.deb` package with a single command:
```bash
./build_deb.sh
```
This automatically compiles the suite, validates the test suite, and installs stripped binaries, manpages, and documentation into a standalone Debian package (`freearc_1.0-1_amd64.deb`).

To install the generated package:
```bash
sudo dpkg -i freearc_1.0-1_amd64.deb
```

---

## Authors & Credits

* **Native C/C++ Implementation**: [seb3773](https://github.com/seb3773)
* **Original FreeArc Format & Algorithms**: Bulat Ziganshin

