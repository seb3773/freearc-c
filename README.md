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
* **Total Binary Format Parity**: Strictly generates standard compliant blocks (`HEADER_BLOCK`, `DATA_BLOCK`, `DIR_BLOCK`, `FOOTER_BLOCK`) with 100% bit-exact DATA and DIR parity vs original Windows FreeArc 0.67 (`Arc.exe`) across all codecs and auto-profiles (`-m1` to `-m5x`).
* **True Intra-Stream Multithreading (4x4)**: High-performance multi-core compression of single solid streams without breaking solid compression ratios, splitting streams into in-memory chunks processed concurrently.
* **Historical 64-bit Bug Fixes & Codec Hardening**: Porting and running the codecs on modern 64-bit Linux addressed critical issues present in legacy FreeArc:
  * 64-bit pointer truncations in **LZP** and **GRZip**.
  * `unsigned long` 64-bit struct member alignment errors in the **Multimedia (MM)** filter WAVE headers.
  * PPMD LP64 sub-allocator unit sizing (12-byte unit indexation) ensuring exact model parity on 64-bit platforms.
  * Tornado Hash3 14-bit log table alignment matching Haskell `tor:5`/`tor:6`.
  * Uninitialized prefix tree pointers causing segmentation faults in **LZMA**.
  * Modernized memory management and strict alignment handling across all compression engines.
* **Integrated Native Cryptography**: Native C++ implementations of AES (128/256-bit), Blowfish, Twofish, and Serpent with header encryption (`-hp`) and data block encryption (`-p`).
* **Standalone Linux SFX**: Autonomous self-extracting executable engine producing `.sfx` and `.run` payloads without external dependencies.

---

## Highlights

* **100% Native C/C++**: Zero Haskell, zero GHC, zero Wine, zero external library dependencies. Compiles cleanly with standard GCC/Clang on modern Linux and MinGW on Windows 64-bit.
* **100% Bit-Exact Legacy Parity**: Full bit-for-bit parity on compressed payload (`DATA`) and directory block metadata (`DIR`) against original Windows FreeArc 0.67 (`Arc.exe`) across all codecs, filters, and auto-profiles (`-m1` through `-m5x`).
* **Intra-Stream Multithreading (4x4)**: Multi-core parallel compression inside single solid streams (`4x4:lzma`, `4x4:tor`), scaling linearly across CPU cores without creating separate solid block boundaries.
* **Intelligent Auto-Profiles & File Grouping**: Complete `arc.groups` classification table (`$text`, `$compressed`, `$exe`, `$iso`, `$precomp`, `$obj`, `$bmp`, `$wav`) with dynamic content detection routing files to optimal compression pipelines.
* **Full Archive Management**: Complete command set — add (`a`), update (`u`), freshen (`f`), delete (`d`), move (`m`), list (`l`/`v`), test (`t`), extract (`x`/`e`), and pipe (`p`/`-so`).
* **High-Ratio Codecs**: Full suite of FreeArc compression algorithms:
  * **LZMA** (fast to maximum levels: `-m1` to `-m5x`, with `4x4` multi-threading)
  * **Tornado** (ultra-fast LZ77 variant with 64-bit CTZ acceleration)
  * **PPMD** (high-order prediction by partial matching, 64-bit bit-exact allocator)
  * **GRZip** (Burrows-Wheeler transform compression with adaptive block mode and $O(1)$ MTF rank table)
  * **TTA** (lossless true audio compression with SIMD vectorization)
  * **4x4** (intra-stream chunked multi-threaded compression wrapper)
  * **Storing** (uncompressed store)
* **Intelligent Preprocessors & Filters**:
  * **BCJ / EXE**: x86 32/64-bit branch target preprocessor (`-bcj` / `-exe`)
  * **REP**: High-speed multi-megabyte repetition filter (`-rep`) with multi-threaded hasher
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

# Run the automated Linux validation suite (24 tests)
make test

# Run the automated Windows 64-bit validation suite (24 tests under Wine, bit-for-bit exact)
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

### Legacy CLI Compatibility & Drop-In Replacement

Both `arc` / `unarc` on Linux and `arc.exe` / `unarc.exe` on Windows 64-bit serve as seamless drop-in replacements for legacy 32-bit FreeArc scripts, CI pipelines, and automated repack installers:

* **Command `create`**: Alias for `a` (e.g. `arc create {options} newarchive.arc @list.lst`).
* **Intermediate Directories**: Automatically creates missing parent directories for destination archives and extracted files.
* **Base Disk Path (`-dpDIR` / `--diskpath=DIR`)**: Configures root directory for packing or unpacking, supporting standard paths and brace-wrapped paths (`--diskpath={path/to/folder}`).
* **Operation & Diagnostic Logging (`--logfile=FILE` / `-loFILE`)**: All console output, file progress, banners, summaries, and error logs are mirrored into the specified log file (even in quiet `-q` mode).
* **Decompression Memory Limiting (`-ld<mem>` / `--LimitDecompMem=<mem>`)**:
  * *At compression (`arc`)*: Constraints algorithms (LZMA dictionary, PPMD memory) to ensure archives fit within the target RAM limit during extraction.
  * *At extraction (`unarc`)*: Validates required block memory before unpacking and safely aborts with an informative error if the limit is exceeded, preventing out-of-memory crashes.
* **Compression Memory Limiting (`-lc<mem>` / `--LimitCompMem=<mem>`)**: Constrains compressor RAM usage during archive creation.
* **Multithreading Control (`-mt<num>` / `--MultiThreaded=<num>`)**: Configures worker thread concurrency for compression algorithms.

---

## Performance & Multithreading Architecture

The native C/C++ engine incorporates a multi-tier multithreading architecture and targeted hardware optimizations while preserving strict 100% bit-exact output determinism:

* **True Intra-Stream Multithreading (4x4 Codec)**:
  * For single solid streams (`arc a archive.arc ...`), FreeArc does **not** compromise compression ratio by splitting archives into independent non-solid blocks.
  * Instead, the `4x4` codec wraps backend compressors (e.g. `4x4:lzma`, `4x4:tor`), chunking the stream in memory and compressing chunks in parallel across $N$ worker threads into a single continuous solid `DATA_BLOCK`.
  * Automatically deployed in auto-profiles (`-m4`, `-m5`, `-m4x`, `-m5x`) whenever $\ge 3$ CPU cores are detected.
* **Multi-Threaded Codec Internals**:
  * **REP Hasher**: Long-distance repetition filter uses `MultipleProcessingThreads` to parallelize match hash-table lookups across CPU cores.
  * **LZMA Match Finder (`LzFindMt`)**: Dedicated worker threads for multi-threaded dictionary match finding.
* **Overlapping Asynchronous I/O Pipeline & Multi-Block Engine**:
  * Dual-buffering background `reader` thread with on-the-fly SIMD CRC-32 and dedicated background `writer` thread.
  * Thread pool for multi-block archives (non-solid `-s-`, partitioned `-s<size>`, `-s<N>f`) with deterministic ordered archive flushing.
* **Bit-Exact Auto-Profiles & File Routing**:
  * Faithful emulation of original FreeArc Haskell auto-presets (`-m1` to `-m5x`) and complete `arc.groups` classification table (`$text`, `$compressed`, `$exe`, `$iso`, `$precomp`, `$obj`, `$bmp`, `$wav`).
  * Dynamic datatype detection via entropy and structure profiling, routing audio to TTA, bitmaps to MM+GRZip/LZMA, and object files to DELTA+LZMA.
  * Exact stream-bounded dictionary capping (`LimitDictionary`).
  * Standard EPN (*Extension, Path, Name*) file ordering matching FreeArc `-ds` (configurable via `-ds-`).
* **Hardware-Accelerated CRC-32 (PCLMULQDQ SIMD)**:
  * Carry-less multiplication (`_mm_clmulepi64_si128`) with 64-byte parallel folding and Barrett reduction, reaching ~9.5 GB/s in memory streaming (~6× faster than scalar slice-by-8/16) and > 300 GB/s on cache.
  * Dynamic runtime dispatch (`__builtin_cpu_supports("pclmul")`) with seamless slice-by-8/16 fallback.
* **Tornado MatchFinder Acceleration**:
  * 64-bit XOR and CTZ (`__builtin_ctzll`) match-length finder eliminating inner comparison loops and branch mispredictions.
* **Vectorized TTA Audio Filter**:
  * Fused single-pass stereo deinterleaving and channel decorrelation (`split_int` / `combine_int`) halving memory traffic.
  * Decoupled contiguous passes for order-1 polynomial prediction, hybrid filter stages, and SSE2/AVX vectorization.
* **GRZip MTF Rank Table**:
  * Instant $O(1)$ inverse rank table (`MTF_Pos[256]`) replacing $O(N)$ sequential list scanning during Move-To-Front encoding.
* **Compiler & Link-Time Optimization**:
  * Production builds compiled with `-O3`, Link-Time Optimization (`-flto=auto`), dead code elimination (`-Wl,--gc-sections`), and symbol stripping.

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
│   └── Compression/     # Compression codecs (4x4, LZMA, PPMD, GRZip, Tornado, TTA, Filters)
├── tests/
│   ├── run_tests.sh       # Comprehensive Linux automated test suite (24 tests)
│   └── run_tests_win64.sh # Automated Windows 64-bit & cross-platform test suite (24 tests)
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
This automatically compiles the suite, validates the test suite, and installs stripped binaries, manpages, and documentation into a standalone Debian package (`freearc_1.3.0-1_amd64.deb`).

To install the generated package:
```bash
sudo dpkg -i freearc_1.3.0-1_amd64.deb
```

---

## Authors & Credits

* **Native C/C++ Implementation**: [seb3773](https://github.com/seb3773)
* **Original FreeArc Format & Algorithms**: Bulat Ziganshin

