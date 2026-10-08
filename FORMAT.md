# FreeArc (.arc) Archive Format Specification

This document provides a concise technical specification of the FreeArc (`.arc`) archive container format, as implemented in `freearc-c`.

---

## 1. Overview

FreeArc is a high-performance archive container designed for streaming creation, solid block grouping, advanced multi-codec pipelines, and SFX concatenation.

Key architectural characteristics:
* **Trailing Metadata**: The archive directory and footer are located at the end of the file. This allows appending data sequentially without prior knowledge of total size.
* **SFX Concatenation**: Autonomous self-extracting packages are created simply by concatenating an executable stub (`arc.sfx`) with a `.arc` archive (`cat arc.sfx archive.arc > archive.run`).
* **Variable-Length Encoding**: Control structures use a prefix-coded integer scheme to minimize overhead.
* **Chained Filter Codecs**: Data blocks can pass through arbitrary chains of preprocessors (REP, DICT, DELTA, BCJ, MM) before entropy/dictionary compressors (LZMA, PPMD, GRZip, Tornado, TTA) and encryption ciphers (AES, Blowfish, Twofish, Serpent).

---

## 2. Global Archive Layout

A standard FreeArc archive contains:

```
+-------------------------------------------------------------+
| [Optional SFX Executable Stub]                              |
+-------------------------------------------------------------+
| DATA BLOCK #1 (Solid compressed file stream)                |
+-------------------------------------------------------------+
| DATA BLOCK #2 (Solid compressed file stream)                |
+-------------------------------------------------------------+
| ...                                                         |
+-------------------------------------------------------------+
| DIRECTORY BLOCK (DIR_BLOCK)                                 |
| - Directory tree & file metadata                            |
| - Per-file sizes, timestamps, CRC32, POSIX modes, symlinks   |
| - List of DATA_BLOCK descriptors                            |
+-------------------------------------------------------------+
| FOOTER BLOCK (FOOTER_BLOCK)                                 |
| - References to control blocks                              |
| - Archive comments & lock flag                              |
+-------------------------------------------------------------+
| FOOTER LOCAL DESCRIPTOR (last <= 4096 bytes)                |
| - Magic signature: "ArC\1"                                  |
| - Footer block compressor, sizes, and CRC32                 |
| - Trailing CRC32                                            |
+-------------------------------------------------------------+
```

---

## 3. Magic Signature & Footer Discovery

The FreeArc signature is a 4-byte identifier:
```c
#define aSIGNATURE  0x01437241   /* Little-endian for bytes: 'A', 'r', 'C', 0x01 */
```

### Footer Discovery Algorithm
1. The unpacker reads the last $\min(\text{file\_size}, 4096)$ bytes of the archive.
2. It scans backward for the 4-byte sequence `0x41 0x72 0x43 0x01` (`"ArC\1"`).
3. The found offset marks the start of the **Footer Local Descriptor**.
4. The footer descriptor specifies the location and compression method of the `FOOTER_BLOCK`.

---

## 4. Integer Encoding (`readInteger`)

Integers in control blocks (offsets, sizes, counts) are stored using a compact variable-length prefix code (1 to 9 bytes):

| First Byte Pattern | Additional Bytes | Total Bytes | Effective Bits |
|--------------------|------------------|-------------|----------------|
| `xxxxxxx0`         | 0                | 1           | 7 bits         |
| `xxxxxx01`         | 1                | 2           | 14 bits        |
| `xxxxx011`         | 2                | 3           | 21 bits        |
| `xxxx0111`         | 3                | 4           | 28 bits        |
| `xxx01111`         | 4                | 5           | 35 bits        |
| `xx011111`         | 5                | 6           | 42 bits        |
| `x0111111`         | 6                | 7           | 49 bits        |
| `01111111`         | 7                | 8           | 56 bits        |
| `11111111`         | 8                | 9           | 64 bits        |

---

## 5. Block Descriptors

Control and data blocks are described by a `BLOCK_DESCRIPTOR`:

| Field | Type | Description |
|---|---|---|
| `type` | Variable Int | Block type identifier (see below) |
| `compressor` | Null-terminated String | Compression method string (e.g. `lzma:max`, `storing`) |
| `pos` | Variable Int | Byte position of the block in the archive |
| `origsize` | Variable Int | Uncompressed size in bytes |
| `compsize` | Variable Int | Compressed size on disk |
| `crc` | 32-bit uint | CRC32 of the uncompressed block data |

### Block Types
* `DESCR_BLOCK (0)`: Local descriptor block
* `HEADER_BLOCK (1)`: Archive header / preamble
* `DATA_BLOCK (2)`: Compressed file stream (payload data)
* `DIR_BLOCK (3)`: Directory and file inventory
* `FOOTER_BLOCK (4)`: Master index pointing to all control blocks
* `RECOVERY_BLOCK (5)`: Reed-Solomon recovery data

---

## 6. Directory Block (`DIR_BLOCK`)

The `DIR_BLOCK` contains the complete catalog of files stored in the archive:

1. **Data Blocks Definition**:
   - `num_of_blocks`: Total count of `DATA_BLOCK`s.
   - `num_of_files[]`: Array indicating how many files belong to each block (enabling solid compression grouping).
   - `data_block[]`: Array of `BLOCK_DESCRIPTOR`s for each data block.
2. **Directory Tree**:
   - `dirs_in_block`: Number of unique directories.
   - `dirs[]`: Array of directory path strings.
   - `dir_numbers[]`: Map assigning each file to its parent directory.
3. **File Attributes**:
   - `total_files`: Total file entries.
   - `name[]`: Relative filenames within their directory.
   - `size[]`: Uncompressed file sizes.
   - `time[]`: Unix timestamps (`time_t`).
   - `isdir[]`: Directory flags (boolean).
   - `crc[]`: CRC32 checksums of uncompressed files.
4. **Extended POSIX Attributes**:
   - `mode[]`: Unix file modes (permissions, executable bits, e.g. `0755`, `0644`).
   - `issymlink[]`: Symbolic link indicators.
   - `symlink_target[]`: Symlink target path string.

---

## 7. Compression Pipelines & Filters

FreeArc combines preprocessors and compressors using string specifications. Filters are executed left-to-right during compression, and in reverse order during decompression.

### Supported Algorithms

| Type | Name | Specifier | Description |
|---|---|---|---|
| **Codecs** | LZMA | `lzma` | Lempel-Ziv-Markov chain with range coder |
| | PPMD | `ppmd` | Prediction by Partial Matching (Dmitry Shkarin) |
| | GRZip | `grzip` | Burrows-Wheeler transform + block-sorting |
| | Tornado | `tor` | High-speed LZ77 compressor |
| | TTA | `tta` | True Audio lossless multichannel codec |
| **Filters** | BCJ | `bcj` | Branch/Call/Jump converter for x86 binaries |
| | REP | `rep` | Long-distance repetition preprocessor |
| | DICT | `dict` | Dictionary-based word preprocessor |
| | DELTA | `delta` | Byte difference preprocessor for structured tables |
| | MM | `mm` | Multimedia channel-interleaving preprocessor |
| | LZP | `lzp` | Lempel-Ziv + Prediction preprocessor |
| **Ciphers** | AES | `aes` | AES-128 / AES-256 in CTR or CBC mode |
| | Blowfish | `blowfish` | Bruce Schneier 128-bit cipher |
| | Twofish | `twofish` | Counter-mode 256-bit Twofish |
| | Serpent | `serpent` | Counter-mode 256-bit Serpent |

### Example Pipeline String
```
rep:512m+bcj+lzma:max:64m:bt4:128
```
* Pass 1: Deduplicates distant repeated sequences (`rep:512m`).
* Pass 2: Converts relative 32-bit `call`/`jmp` addresses to absolute addresses (`bcj`).
* Pass 3: Compresses using LZMA with 64 MB dictionary and match finder `bt4` (`lzma:max`).

---

## 8. Encryption Scheme

* **Data Encryption (`-p`)**: Encrypts individual `DATA_BLOCK`s. The block compressor string is wrapped with a cipher descriptor (e.g. `aes+lzma`).
* **Header Encryption (`-hp`)**: Encrypts both `DIR_BLOCK` and `FOOTER_BLOCK`. The entire file inventory, filenames, directory structure, sizes, and timestamps are protected and unreadable without the password.
* Key derivation utilizes **PKCS#5 PBKDF2** with SHA-1 or SHA-512 and random salts.
