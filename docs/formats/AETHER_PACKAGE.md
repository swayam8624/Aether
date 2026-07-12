# AETHER scene package v1

`.aether` is AETHER's random-access, little-endian scene container. All offsets and sizes are
unsigned integers. A reader must validate the complete package before allocating or decompressing a
chunk.

## Header (64 bytes)

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 8 | Magic: `AETHER` followed by two zero bytes |
| 8 | 2 | Major version (`1`) |
| 10 | 2 | Minor version (`0`) |
| 12 | 4 | Package feature flags; zero in v1 |
| 16 | 8 | Absolute chunk-table offset |
| 24 | 4 | Chunk count |
| 28 | 4 | Reserved; zero in v1 |
| 32 | 32 | SHA-256 of every byte following the header |

## Chunk table

The table contains one 64-byte entry per chunk. Entries are unique by type.

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 4 | Registered chunk type |
| 4 | 4 | Flags: bit 0 required, bit 1 Zstandard compressed |
| 8 | 8 | Absolute payload offset |
| 16 | 8 | Stored byte count |
| 24 | 8 | Uncompressed byte count |
| 32 | 32 | SHA-256 of the uncompressed payload |

Payloads begin on 64-byte boundaries. Unknown optional chunks are skipped. Unknown required chunks
produce a compatibility error. Default readers reject files larger than 64 GiB, chunks larger than
8 GiB, more than one million chunks, overlapping payload ranges, invalid hashes, and compressed
chunks whose expansion ratio exceeds 256:1.

## Unpacked development schema

`aether-pack` accepts a directory containing `metadata.json` and either a validated
`base-gaussians.ply` or canonical `base-gaussians.bin`. PLY input is converted into the canonical
little-endian chunk during packing; an existing binary chunk is decoded and validated before it is
accepted. The remaining
registered filenames are optional: `cameras.bin`, `material-gaussians.bin`, `residuals.bin`,
`cluster-hierarchy.bin`, `proxy-mesh.bin`, `textures.bin`, `collision.bin`, `thumbnail.bin`, and
`benchmark-path.json`.

The current writer is deterministic for identical source bytes and chunk order. It uses Zstandard
level 3 when compression is smaller and remains inside the reader's expansion-ratio limit; otherwise
the payload is stored verbatim. Output is written to a sibling temporary file and atomically renamed.

## Base-Gaussian chunk v1

The chunk begins with a 32-byte header: eight-byte `AETHGS` magic, major/minor `uint16` version,
`uint32` record stride, `uint64` record count, `uint32` SH degree, and a reserved `uint32`. Every
record is 256 bytes and stores little-endian float32 position, log scale, normalized `(w,x,y,z)`
rotation, opacity logit, three DC coefficients, 45 reserved/active higher-order SH coefficients, and
the active higher-order coefficient count. Remaining record bytes are zero padding. Readers validate
the exact payload size, SH degree/count agreement, finite values, safe scale range, and quaternion
normalization.
