# LZMA SDK 25.01 decoder (vendored)

Files copied verbatim from the official LZMA SDK 25.01 by Igor Pavlov
(https://www.7-zip.org/sdk.html, public domain). Only the decoder is used.

`lzma_fastdec.c` is a thin C wrapper compiled as a separate translation unit
so that these headers never mix with the 2008 SDK types used by the encoder.
LZMA decoding is fully specified by the stream, so output is bit-identical to
the old decoder; only speed differs.
