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
* **Zero External Dependencies**: Produces self-contained, stripped executables for Linux x86_64 (`arc` 1.51 MiB, `unarc` 837 KiB, `arc.sfx` 837 KiB), Linux ARM64, Linux RISC-V 64, and Windows x64.
* **Total Binary Format Parity**: Strictly generates standard compliant blocks (`HEADER_BLOCK`, `DATA_BLOCK`, `DIR_BLOCK`, `FOOTER_BLOCK`) with 100% bit-exact DATA and DIR parity vs original Windows FreeArc 0.67 (`Arc.exe`) across all codecs and auto-profiles (`-m0` to `-m9`, `-mx`, `-m1x` to `-m9x`).
* **True 64-bit Architecture (No 2 GB RAM Ceiling)**: Fully breaks the 32-bit architectural barrier of legacy FreeArc 0.67 (which was strictly capped at 2 GB of virtual address space, causing notorious `Out of memory` and `data compression failed (-11)` crashes when handling large datasets, high thread counts `-mt`, or heavy dictionaries like `-ld3600m`). `freearc-c` natively supports 8 GB, 16 GB, 32 GB, 64 GB+ RAM allocations, enabling massive multi-gigabyte dictionaries and multi-core throughput on modern hardware.
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
* **100% Bit-Exact Legacy Parity**: Full bit-for-bit parity on compressed payload (`DATA`) and directory block metadata (`DIR`) against original Windows FreeArc 0.67 (`Arc.exe`) across all codecs, filters, and auto-profiles (`-m0` through `-m9x` / `-mx`).
* **True 64-bit Memory Freedom (No 2 GB Barrier)**: Unlike legacy 32-bit FreeArc (which failed with error `-11` or out-of-memory when RAM approached ~1.8 GB), `freearc-c` runs on a native 64-bit flat address space, allowing massive dictionaries (e.g. `-ld3600m`, `-lc8g`) and high concurrency (`-mt14`, `-mt32`) on multi-gigabyte/terabyte workloads.
* **Intra-Stream Multithreading (4x4)**: Multi-core parallel compression inside single solid streams (`4x4:lzma`, `4x4:tor`), scaling linearly across CPU cores without creating separate solid block boundaries.
* **Intelligent Auto-Profiles & File Grouping**: Built-in `arc.groups` classification (`$text`, `$code`, `$compressed`, `$exe`, `$iso`, `$precomp`, `$obj`, `$bmp`, `$wav`, plus `$dna` and `$compressed_img` in `-nobitexact`) with content-based detection and PE/ELF magic verification routing files to optimal compression pipelines.
* **Full Archive Management**: Complete command set — add (`a`), update (`u`), freshen (`f`), delete (`d`), move (`m`), list (`l`/`v`), test (`t`), extract (`x`/`e`), and pipe (`p`/`-so`).
* **High-Ratio Codecs**: Full suite of FreeArc compression algorithms:
  * **LZMA** (fast to maximum levels: `-m1` to `-m9x` / `-mx`, with `4x4` multi-threading, plus vendored LZMA SDK 25.01 fast encoder/decoder & x86-64 ASM kernels)
  * **Tornado** (ultra-fast LZ77 variant with 64-bit CTZ and x86-64 AVX2/SSE2 match-extension kernels)
  * **PPMD** (high-order prediction by partial matching, 64-bit allocator, parallel `4x4` decompression with transparent hugepage & arena reuse)
  * **GRZip** (Burrows-Wheeler transform compression with adaptive block mode and $O(1)$ MTF rank table)
  * **TTA** (lossless true audio compression with SIMD vectorization)
  * **4x4** (intra-stream chunked multi-threaded compression wrapper)
  * **Storing** (uncompressed store)
* **Intelligent Preprocessors & Filters**:
  * **BCJ / EXE**: x86 32/64-bit branch target preprocessor (`-bcj` / `-exe`)
  * **REP**: High-speed multi-megabyte repetition filter (`-rep`) with multi-threaded hasher and vectorized match extension
  * **DELTA**: Multi-byte differential filter (`-delta`)
  * **DICT**: Text dictionary preprocessor (`-dict`) with thread-safe parallel encoding and early abort for incompressible blocks
  * **LZP**: Lempel-Ziv + Prediction filter (`-lzp`)
  * **MM**: Multimedia 8/16/24/32-bit channel interleaved preprocessor (`-mm`)
* **Strong Encryption**:
  * AES (128/192/256-bit), Blowfish, Twofish, and Serpent (CTR/CFB modes with PKCS#5 PBKDF2 HMAC-SHA-512).
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

Binaries are stripped production builds organized cleanly by target platform (`v1.4.6`):

### Linux (x86_64) — `./build/linux/`
| Binary | Size | Description |
|---|---|---|
| `build/linux/arc` | 1.51 MiB (`1,582,200 B`) | Full archiver and archive manager (create, update, freshen, delete, move, SFX conversion) |
| `build/linux/unarc` | 837 KiB (`857,240 B`) | Standalone unpacker, lister, and integrity tester |
| `build/linux/arc.sfx` | 837 KiB (`857,240 B`) | Standalone Linux ELF stub for self-extracting packages (`.sfx` / `.run`) |
| `build/linux/libunarc.so` | 877 KiB (`897,544 B`) | Shared C/C++ decoder library (`dlopen` / `-lunarc`) |
| `build/linux/libunarc.a` | 1.36 MiB (`1,421,978 B`) | Static C/C++ decoder archive (link with `-lstdc++ -lpthread -lm`) |
| `build/linux/unarc.h` | 6.1 KiB (`6,211 B`) | Public C header (legacy `FreeArcExtract` + modern `unarc_run` API) |

### Linux ARM64 (aarch64) — `./build/linux-arm64/`
| Binary | Size | Description |
|---|---|---|
| `build/linux-arm64/arc` | 1.58 MiB (`1,651,992 B`) | Full archiver and archive manager compiled natively for Linux ARM64 (AArch64) |
| `build/linux-arm64/unarc` | 967 KiB (`990,632 B`) | Standalone unpacker, lister, and integrity tester for Linux ARM64 |
| `build/linux-arm64/arc.sfx` | 967 KiB (`990,632 B`) | Standalone Linux ARM64 ELF stub for self-extracting packages (`.sfx` / `.run`) |
| `build/linux-arm64/libunarc.so` | 1.07 MiB (`1,120,696 B`) | Shared C/C++ decoder library for Linux ARM64 |
| `build/linux-arm64/libunarc.a` | 1.39 MiB (`1,461,838 B`) | Static C/C++ decoder archive for Linux ARM64 |
| `build/linux-arm64/unarc.h` | 6.1 KiB (`6,211 B`) | Public C header |

### Windows (x86_64) — `./build/win64/`
| Binary | Size | Description |
|---|---|---|
| `build/win64/arc.exe` | 1.70 MiB (`1,779,200 B`) | Standalone Windows 64-bit archiver (static MinGW-w64, zero runtime DLL dependencies) |
| `build/win64/unarc.exe` | 1.05 MiB (`1,096,704 B`) | Standalone Windows 64-bit unpacker and integrity tester |
| `build/win64/unarc.dll` | 1.06 MiB (`1,111,552 B`) | Windows 64-bit decoder DLL (static C/C++ runtime, OS DLLs only) |
| `build/win64/libunarc.dll.a` | 5.2 KiB (`5,298 B`) | MinGW-w64 import library for `unarc.dll` |
| `build/win64/unarc.h` | 6.1 KiB (`6,211 B`) | Public C header |

### Linux RISC-V 64 (`riscv64`) — `./build/linux-riscv64/`
| Binary | Size | Description |
|---|---|---|
| `build/linux-riscv64/arc` | 1.30 MiB (`1,361,320 B`) | Full archiver and archive manager cross-compiled for Linux RISC-V 64 |
| `build/linux-riscv64/unarc` | 824 KiB (`843,304 B`) | Standalone unpacker, lister, and integrity tester for Linux RISC-V 64 |
| `build/linux-riscv64/arc.sfx` | 824 KiB (`843,304 B`) | Standalone Linux RISC-V 64 ELF stub for `.sfx` / `.run` self-extractors |
| `build/linux-riscv64/libunarc.so` | 801 KiB (`820,296 B`) | Shared C/C++ decoder library for Linux RISC-V 64 |
| `build/linux-riscv64/libunarc.a` | 1.99 MiB (`2,082,860 B`) | Static C/C++ decoder archive for Linux RISC-V 64 |
| `build/linux-riscv64/unarc.h` | 6.1 KiB (`6,211 B`) | Public C header |

---

## Quick Start

### Build from Source

```bash
# Clone the repository
git clone https://github.com/seb3773/freearc-c.git
cd freearc-c

# Compile Linux x86_64 production binaries (into ./build/linux/)
make

# Compile Linux ARM64 (aarch64) production binaries (into ./build/linux-arm64/)
make arm64

# Compile Windows 64-bit native binaries (into ./build/win64/)
make win64

# Compile Linux RISC-V 64 binaries (into ./build/linux-riscv64/, requires g++-riscv64-linux-gnu)
make riscv64

# (Or compile standard CLI platforms simultaneously)
make all-platforms

# Compile decoder libraries: Linux .so/.a, Windows unarc.dll, Linux ARM64 .so/.a
make lib          # ./build/linux/{libunarc.so,libunarc.a,unarc.h}
make dll          # ./build/win64/{unarc.dll,libunarc.dll.a,unarc.h} (alias: make lib-win64)
make lib-arm64    # ./build/linux-arm64/{libunarc.so,libunarc.a,unarc.h}
make lib-riscv64  # ./build/linux-riscv64/{libunarc.so,libunarc.a,unarc.h}
make lib-all      # all of the above (plus riscv64 if cross-compiler is installed)

# Run the automated CLI validation suites
make test         # Linux x86_64 CLI suite
make test-arm64   # Linux ARM64 CLI suite (via QEMU)
make test-win64   # Windows 64-bit CLI suite (under Wine)
make test-riscv64 # Linux RISC-V 64 CLI suite (via QEMU)
make test-all     # Standard CLI suites (Linux, Win64, ARM64)

# Run the decoder library / DLL validation suites
make test-lib
make test-lib-win64
make test-lib-arm64
make test-lib-riscv64

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

### Configuration File (`arc.ini`) Support

`freearc-c` fully supports the historical FreeArc `arc.ini` configuration format across Linux and Windows:

* **Automatic Discovery Hierarchy**:
  1. Explicit CLI specification: `-cfg<path>` or `--config <path>`
  2. Executable directory: `arc.ini` located alongside `arc` / `unarc`
  3. Current working directory: `./arc.ini`
  4. System-wide configuration: `/etc/arc.ini` (Unix/Linux)
* **Disabling Config Loading**: `-cfg-` or `--no-config` skips searching and loading any `arc.ini`.
* **Inspection**: `--print-config` displays the parsed active configuration, default options, and compression method aliases.
* **Supported INI Sections**:
  * `[Default options]`: Global or per-command default switches automatically injected (e.g. `arc = -mx -nobitexact`, `unarc = -o+`). Supports key-value syntax (`nobitexact = true`), bare options (`-nobitexact`), and command-specific rules (`arc = ...`, `a create = ...`). All default options can be overridden or cancelled via the command line (e.g. `-nobitexact-`).
  * `[Compression methods]`: Custom compression method aliases, multi-group pipelines with `/` (e.g. `$text:ppmd / $wav:tta / lzma`), and numeric wildcard `#` (e.g. `lzma# = lzma:#m`).
  * `[External compressor:NAME]`: Registration and piping rules for external archivers and preprocessors.

---

## Performance & Multithreading Architecture

The native C/C++ engine incorporates a multi-tier multithreading architecture and targeted hardware optimizations while preserving strict 100% bit-exact output determinism:

* **True Intra-Stream Multithreading (`4x4` Codec)**:
  * For single solid streams (`arc a archive.arc ...`), FreeArc does **not** compromise compression ratio by splitting archives into independent non-solid blocks.
  * Instead, the `4x4` codec wraps backend compressors (e.g. `4x4:lzma`, `4x4:tor`, `4x4:ppmd`), chunking the stream in memory and compressing/decompressing chunks in parallel across $N$ worker threads into a single continuous solid `DATA_BLOCK`.
  * Automatically deployed in auto-profiles (`-m4`..`-m9` and `-mx` / `-m4x`..`-m9x` in `-nobitexact` where `4x4` chunking applies) whenever $\ge 3$ CPU cores are detected.
* **x86-64 Assembly Kernels (`MF_ASM=1` / `LZMA_ASM=1`)**:
  * Hand-written x86-64 match finders for bit-exact LZMA (`LzFind_x86_64.S`), `-nobitexact` LZMA SDK 25 (`LzFind25_x86_64.S`), and Tornado (`tor_match_x86_64.S`) using shared System V / Win64 ABI macros and runtime `CPUID`/`XGETBV` AVX2/SSE2 match-extension dispatch (`fa_cpu_x86_64.S`), alongside the `LzmaDecOpt.S` x86-64 assembly LZMA decoder loop.
  * Branchless binary-tree descent (`cmov`), ahead-of-time node prefetching, and AVX2/SSE2 match extension with C fallback (`MF_ASM=0` / `LZMA_ASM=0`) for portability and ARM64 builds.
* **Multi-Threaded Codec Internals & Fast Extraction**:
  * **LZMA2 SDK 25 Optimal Parser (`-nobitexact`)**: AVX2/SSE2 vectorized match extension inside `GetOptimum` paired with adaptive BT4 pruning (`stagnation abort` and `good-match` cutoffs).
  * **Parallel `4x4` PPMd & Fast Range Coding**: Multi-threaded `Dict` (`DictEncode`) + `LZP` + `PPMd` compression/decompression across CPU cores, per-thread arena reuse, 2 MB transparent hugepage (`THP`) sub-allocator hints, write coalescing, and division-free range coding.
  * **REP & DICT Vectorization**: Multi-threaded `REP` hasher with vectorized forward/backward match extensions, and early-abort skip logic in `DICT` for incompressible blocks.
* **True 64-bit Address Space & Memory Scaling**:
  * Legacy FreeArc 0.67 was a 32-bit application limited to 2 GB of virtual memory, making it prone to crashes on mixed-datatype datasets, high thread counts, or dictionary sizes $> 1\text{ GB}$.
  * `freearc-c` operates with native 64-bit pointer and allocation models across Linux (`x86_64`, `aarch64`) and Windows (`win64`), effortlessly scaling to multi-gigabyte dictionaries on modern hardware.
* **Overlapping Asynchronous I/O Pipeline & Multi-Block Engine**:
  * Dual-buffering background reader with on-the-fly SIMD CRC-32 and background block writer/worker pool for multi-block modes (`-s-`, `-s<size>`, `-s<N>f`) with deterministic sequential flushing.
* **Bit-Exact Auto-Profiles & File Routing**:
  * Faithful emulation of original FreeArc Haskell auto-presets (`-m0` to `-m9`, `-mx`, `-m1x` to `-m9x`) and built-in `arc.groups` classification (`$text`, `$code`, `$compressed`, `$exe`, `$iso`, `$precomp`, `$obj`, `$bmp`, `$wav`, plus `$dna` and `$compressed_img` in `-nobitexact`).
  * Multi-threaded content-based datatype detection routing audio to `TTA`, bitmaps to `MM+GRZip/LZMA`, and object files to `DELTA+LZMA`, with standard EPN (*Extension, Path, Name*) ordering (`file_sort_epn`).
* **Hardware-Accelerated CRC-32 (PCLMULQDQ SIMD)**:
  * Carry-less multiplication (`_mm_clmulepi64_si128`) with 64-byte parallel folding and Barrett reduction (`__builtin_cpu_supports("pclmul")`), reaching ~9.5 GB/s in streaming (~6× faster than scalar slice-by-8/16) and > 300 GB/s in cache, with scalar fallback.
* **Vectorized TTA & $O(1)$ GRZip MTF**:
  * Single-pass stereo deinterleaving/decorrelation and SIMD polynomial prediction in TTA, and $O(1)$ inverse rank table (`MTF_Pos[256]`) in GRZip.
* **Compiler & Link-Time Optimization**:
  * Linux x86_64 production builds use `-O3`, Link-Time Optimization (`-flto=auto`), dead code elimination (`-Wl,--gc-sections`), and stripping (`sstrip` / `strip --strip-all`); Windows x64 and Linux ARM64 builds compile with `-O3` and `-s`.

---

## Ratio Optimization Mode: `-nobitexact`

The primary goal of this project is a faithful reimplementation of FreeArc in C/C++, reproducing the exact behavior of the historical 0.67 binary so archives remain bit-for-bit identical with it. While building `freearc-c`, several opportunities emerged to push ratios and throughput further where parameters or heuristics were previously throttled for 32-bit constraints or historical bit-exactness.

Specifying **`-nobitexact`** (alias: **`--max-ratio`**) lifts those legacy bit-exactness caps during **archive creation** while keeping the resulting container and streams **100% compatible with the FreeArc 0.6x format** and extractable by legacy Windows `unarc.exe 0.67`.

Importantly, `-nobitexact` does **not** introduce any new compression methods, filters, or format extensions. It works exclusively with the algorithms and decoding mechanisms already supported by the original FreeArc 0.67 decoder.

The difference lies entirely in how these existing components are configured, combined, ordered, and applied to the input data. This allows FreeArc-c to explore compression strategies that the original encoder did not use, or deliberately avoided, while preserving full backward compatibility.

In other words, the encoder is free to evolve, but the format and its legacy decoder remain untouched.

### Benchmark Results on Matt Mahoney 10 GB Corpus (`10,000,000,000 B` / `9.313 GiB` payload)

Measured on an **11th Gen Intel® Core™ i3-1115G4 @ 3.00GHz** (2 cores / 4 threads, AVX2/AVX-512, NVMe SSD):

| Compressor / Profile | Archive Size | Encoding Time | Decoding Time |
| :--- | :--- | :--- | :--- |
| **FreeArc 0.67 legacy (`-m5`)** | — | — | — |
| **FreeArc-c Bit-Exact (`-m5`)** | — | — | — |
| **FreeArc-c `-nobitexact` (`-m5`)** | 3,350,993,828 B (3.121 GiB) | ~16.8 min | ~353 s |
| **FreeArc 0.67 legacy (`-m9`)** | 2,945,941,964 B (2.744 GiB) | — | — |
| **FreeArc-c Bit-Exact (`-m9`/`-mx`)** | — | — | — |
| **FreeArc-c `-nobitexact` (`-m9`/`-mx`)** | **2,926,616,302 B (2.726 GiB)** | `138.67 min` (`8,319.95 s`) | `327.24 s` (`t`) / `334.59 s` (`x`) |
| **7-Zip 26.02 Ultra (`-mx=9 -ms=on`)** | 3,652,676,518 B (3.402 GiB) | `82.19 min` (`4,931.5 s`) | `76.3 s` |
| **Zstandard 1.5.4 (`--ultra -22 -T4`)** | 3,816,208,568 B (3.554 GiB) | `118.38 min` (`7,102.8 s`) | `27.7 s` |

* **Per-Level Scaling (`1.300 GiB` Mixed Corpus Subset — `23,610` files, `1,395,675,499 B`)**:
  `-m1..-m5` prioritize fast everyday throughput (`-m5`: `535,937,911 B`), while `-m6..-m9` scale independently in ratio, encode speed, and decode speed against `Bit-Exact` (`-m6..-m9`), with full `unarc.exe 0.67` compatibility:

  | Level | `Bit-Exact` Size | `-nobitexact` Size | Size Gain | Enc (`Bit` -> `nbx`) | Dec `unarc t` / `x` (`Bit` -> `nbx`) | `unarc 0.67` |
  | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
  | **`-m6`** | `531,832,601 B` | **`525,069,819 B`** | **−6.76 MB** | `487.23 s` -> **`153.16 s` (`3.18×`)** | `87.79 s / 87.42 s` -> **`66.83 s / 67.36 s`** | **All OK** |
  | **`-m7`** | `529,429,290 B` | **`523,901,937 B`** | **−5.53 MB** | `571.55 s` -> **`423.12 s` (`1.35×`)** | `90.39 s / 91.33 s` -> **`68.54 s / 69.86 s`** | **All OK** |
  | **`-m8`** | `528,493,074 B` | **`523,559,290 B`** | **−4.93 MB** | `601.54 s` -> **`430.25 s` (`1.40×`)** | `92.40 s / 92.58 s` -> **`76.21 s / 75.34 s`** | **All OK** |
  | **`-m9`** | `528,437,734 B` | **`523,167,755 B`** | **−5.27 MB** | `645.75 s` -> **`556.19 s` (`1.16×`)** | `102.41 s / 104.34 s` -> **`72.07 s / 79.52 s`** | **All OK** |

* **vs 7-Zip Ultra & Zstandard (`-m9`)**: On the Matt Mahoney 10 GB corpus (`83,437` files, `10,000,000,000 B`), `-nobitexact -m9` achieves **`2,926,616,302 B` (`29.27%`)**, saving **692.43 MiB** over 7-Zip 26.02 (`-mx=9 -ms=on`) and **18.43 MiB (`19.33 MB`)** over FreeArc 0.67 legacy (`-m9`).
* **100% Backward Compatible**: All `-nobitexact` archives (`-m1..-m9` and `-m1x..-m9x` / `-mx`) pass full integrity verification (`unarc.exe t`, `All OK`, `rc=0`) under Wine using the historical 32-bit FreeArc 0.67 unpacker (`452.91 s` for the `10 GB` `-m9` archive).

### Architecture in `-nobitexact` Mode

* **Two-Tier Preset Strategy (`-m1..-m5` Speed/Ratio, `-m6..-m9` High-Ratio)**: `-m1..-m5` target fast everyday archiving (`-m5`: `rep:256m`, `dict`, `lzp`, `ppmd:16`, `lzma:192m`), whereas `-m6..-m9` / `-m6x..-m9x` (`-mx`) each use distinct parameters across `rep` (`384m` to `2040m`), LZMA (`b64m` `mc32` to `t1:b192m` `max` `mc128`), AVX2/SSE2 vectorized optimal parsing, and `LzFind25_x86_64.S` assembly kernels.
* **Content-Aware Solid Sorting (`file_sort_smart` + SimHash LSH)**: Groups files by datatype/extension class, structural entropy & Morton-coded byte signature (or 4% GC bins for `$dna`), subpath stem clustering, file size, and locality-sensitive hashing (SimHash) across large text/default blocks.
* **Specialized Group Pipelines & Isolation**:
  * `$code`: `rep+lzma:...:lc4:lp0:pb0` tuned for source code (or folded into `$text` below 64 MB).
  * `$text`: `-m6..-m9` apply `rep:384m..2040m+dict:p:128m:80%+4x4:b32m:ppmd:9..12:384m:r` (content-routed `.tar` files deduplicate against unpacked trees without LZP overhead); `-m1..-m5` use `dict+lzp+4x4:b32m:ppmd:16:256m`.
  * `$dna`: `rep:...:l128:s64+` paired with `4x4:b64m:ppmd:7` (`-m5`/`-m6`), `4x4:t2:b128m:lzma:128m:normal:bt4:64:mc32..48:lc3:lp0:pb0` (`-m7`/`-m8`), and `lzma:254m:normal:bt4:64:mc64:lc3:lp0:pb0` (`-m9`).
  * `$bmp`: Solid `$bmp` blocks use `mm+rep:256m+4x4:b64m:grzip:m1:l2048:a` on `-m6..-m9`, selecting explicit `mm:3*8+` / `mm:1*8+` / `mm:4*8+` via MDL probing.
  * `$exe`: PE/ELF magic verification (`detect_x86_exec_magic`) isolates x86/x86_64 binaries while keeping non-x86 executables separate; on `-m6..-m9`, `$obj` forms its own solid block separate from `default`.
  * `$compressed`, `$compressed_img` & `$precomp`: Fast routing bypasses disk entropy probing for compressed containers (`rep+4x4:tor:8m:c3`), applies entropy-checked `4x4:b64m:r0:lzma` to JPEG/PNG/GIF/WebP in `-m6..-m9`, and `rep+4x4:lzma` to PDF/SWF containers.
* **Fast 4 MiB MDL Block Selection**: Probes distributed 4×1 MiB windows (`tor:8m:c3`, `delta+lzma:4m` vs `lzma:4m`, `ppmd`, explicit `mm:3*8/1*8/4*8` + `grzip:m1/m2`, and LZMA `lc/lp/pb` modes) on blocks $\ge 12\text{ MiB}$ ($\ge 2\text{ MiB}$ for `$bmp`) to skip ineffective filters or switch straight to Tornado on incompressible payloads.

---

## Decoder Library & DLL API (`unarc.dll` / `libunarc`)

FreeArc-c provides an embeddable 64-bit decoder library sharing the exact same decompression engines (LZMA/LZMA2 x64 ASM, Tornado, PPMd, GRZip, REP, LZP, Delta, Dict, BCJ, MM/TTA, `4x4`, and AES/Blowfish/Twofish/Serpent cryptography) while turning fatal CLI exits into clean negative return codes and callbacks. Calls are thread-safe (serialized via an internal mutex while preserving internal `4x4` multithreading).

See [`src/unarc.h`](src/unarc.h) for the full C interface, supporting two APIs:

1. **Legacy Win32/Win64 DLL API** (compatible with original FreeArc `Addons/Unarc-DLL` callers):
   ```c
   int  FreeArcExtract(unarc_legacy_callback callback, ...); /* NULL- or ""-terminated strings */
   void UnloadDLL(void);
   ```
2. **Modern 64-bit C API** (full 64-bit sizes, `userdata` pointer, explicit `argc`/`argv`):
   ```c
   int         unarc_run(int argc, const char *const *argv, unarc_callback callback, void *userdata);
   const char *unarc_version(void);
   const char *unarc_last_error(void);
   const char *unarc_error_string(int errcode);
   void        unarc_cleanup(void);
   ```
   Supported callback events (`what`): `"total"`, `"read"`, `"write"`, `"filename"`, `"overwrite?"`, `"password?"`, `"error"`, `"total_files"`, `"origsize"`, `"compsize"`, and `"list_item"` (modern API `l`/`v` listing).

### Linking Examples

```bash
# Linux dynamic linking against libunarc.so
gcc app.c -Ibuild/linux -Lbuild/linux -Wl,-rpath,'$ORIGIN' -lunarc -o app

# Linux static linking against libunarc.a
gcc -DUNARC_STATIC app.c -Ibuild/linux build/linux/libunarc.a -lstdc++ -lpthread -lm -o app

# Windows 64-bit (MinGW-w64) linking against unarc.dll via import library
x86_64-w64-mingw32-gcc app.c -Ibuild/win64 build/win64/libunarc.dll.a -o app.exe
```

---

## Project Structure

```
.
├── build/
│   ├── linux/           # Linux x86_64 binaries & libs (arc, unarc, arc.sfx, libunarc.so/.a, unarc.h)
│   ├── linux-arm64/     # Linux ARM64 binaries & libs (arc, unarc, arc.sfx, libunarc.so/.a, unarc.h)
│   ├── linux-riscv64/   # Linux RISC-V 64 binaries & libs (arc, unarc, arc.sfx, libunarc.so/.a, unarc.h)
│   └── win64/           # Windows 64-bit binaries & DLL (arc.exe, unarc.exe, unarc.dll, libunarc.dll.a, unarc.h)
├── build.sh             # Fast multi-mode Linux build script
├── build_deb.sh         # Complete Linux build + test + Debian .deb packager
├── arc.ini.example      # Annotated sample configuration file
├── Makefile             # Root Makefile (CLI, lib/dll, riscv64, test*, deb, install, clean targets)
├── README.md            # Project documentation
├── FORMAT.md            # FreeArc .arc container technical specification
├── LICENSE              # GPL-2.0 License
├── man/                 # Standard roff manual pages (arc.1, unarc.1)
├── src/                 # Native C/C++ & x86-64 ASM source code
│   ├── arc.cpp          # Archiver CLI, routing, sorting & management logic
│   ├── unarc.cpp        # Unpacker CLI & extraction logic
│   ├── unarc.h          # Public C library & Win64 DLL header
│   ├── unarc_dll.cpp    # Thread-safe C library / Win64 DLL entry points & recovery layer
│   ├── ArcStructure.h   # FreeArc archive format structures & parser
│   ├── ArcProcess.h     # High-level archive processing routines
│   ├── ArcCommand.h     # CLI options parser & dispatcher
│   ├── ArcIni.h         # arc.ini discovery and configuration parser
│   ├── WinCompat.h      # Windows 64-bit POSIX compatibility shims
│   └── Compression/     # Codecs (4x4, LZMA, LZMA2/sdk25, PPMD, GRZip, Tornado, TTA, Filters, asm/)
├── bench/               # Decoder & encoder A/B benchmark harnesses and fuzzers
├── tests/
│   ├── run_tests.sh        # Linux x86_64 automated test suite (26 tests)
│   ├── run_tests_win64.sh  # Windows 64-bit & Wine interoperability test suite (26 tests)
│   ├── run_tests_arm64.sh  # Linux ARM64 QEMU conformance test suite (23 tests)
│   ├── run_tests_riscv64.sh # Linux RISC-V 64 QEMU test suite
│   ├── run_tests_lib.sh    # Decoder library/DLL test runner (Linux, Win64, ARM64, RISC-V 64)
│   ├── test_lib_host.c     # C host test harness (FreeArcExtract, unarc_run, static & dlopen/LoadLibrary)
│   ├── test_bitexact.sh    # Bit-exact parity (20) & crypto roundtrip (7) checker
│   ├── test_parity_067.sh  # Direct 0.67 Wine parity across -m1..-m9, -mx, -m1x..-m9x, multi-chunk dict, MT 4x4:lzma & sort orders (30 checks)
│   ├── test_arc_ini.sh     # arc.ini configuration parity suite (9 checks)
│   └── test_compat_modes.sh # Compatibility modes & legacy 0.67 Wine parity suite (24 checks)
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
This automatically compiles the Linux x86_64 suite, runs the test suite, and packages stripped binaries, manpages, and documentation into a standalone Debian package (`freearc_1.4.6_amd64.deb`).

To install the generated package:
```bash
sudo dpkg -i freearc_1.4.6_amd64.deb
```

---

## Authors & Credits

* **Native C/C++ Implementation**: [seb3773](https://github.com/seb3773)
* **Original FreeArc Format & Algorithms**: Bulat Ziganshin
* **Special Thanks & Testing**: [@ant-sh](https://github.com/ant-sh) for rigorous stress-testing on large real-world multi-gigabyte datasets, valuable edge-case bug reports, and validating extraction parity against the reference engine.


