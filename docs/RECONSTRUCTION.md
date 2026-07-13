# Local reconstruction dependencies

AETHER keeps COLMAP and Brush out of the application process. Versions are frozen in
`dependencies/reconstruction.lock.json`; adapters exchange files through a versioned job directory
and record commands and provenance. No CUDA or cloud service is part of the macOS path.

Run `tools/bootstrap-reconstruction.zsh` to clone the exact Brush 0.3.0 commit and build its
`brush_app` CLI with Cargo's committed lockfile. The script never invokes `sudo`, Homebrew, or a
package-manager install. It checks for COLMAP 3.13.0; if absent, it prepares the pinned source and
stops with a clear handoff because COLMAP's native dependencies are a machine-level choice.

The expected private tool directory is `.aether-deps/bin`, ignored by Git. Source commits are
verified after checkout. Public releases must additionally archive dependency notices, the lock
manifest, build logs, and checksums of redistributed binaries.

`aether-reconstruct` runs six external resumable stages: feature extraction, exhaustive matching,
seeded sparse mapping, text-model export, undistortion, and Brush training. Between export and
undistortion, an AETHER-owned gate parses the COLMAP text model and requires enough registered
images, multi-view tracks, overlap-graph connectivity, camera baseline, and angular diversity.
It writes `pose-coverage.json` atomically; a failed gate records `coverage-failed` in `job.json` and
does not launch Brush. Every subprocess receives an argument vector directly—never a shell
command—while stdout/stderr go to separate stage logs. Completion markers are written atomically
only after the process exits successfully and its expected output exists. SIGINT and SIGTERM are
forwarded to the active child.

The adapter runs Brush in CLI mode using the pinned v0.3.0 interface:

```bash
brush <COLMAP-dataset> --with-viewer=false --seed 42 \
  --total-steps 30000 --export-every 30000 \
  --export-path <job>/exports --export-name base-gaussians.ply
```

COLMAP 3.13.0 is selected because it provides a deterministic `random_seed` option. AETHER's job
manifest preserves the seed, full argument vectors, pinned identities, sorted input sizes/SHA-256
hashes, expected outputs, sparse-coverage evidence, stage logs, and resume markers. It verifies both
executable versions before starting. `--dry-run --json` validates inputs and emits every external
command without launching a tool.
