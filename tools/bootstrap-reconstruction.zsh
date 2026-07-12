#!/bin/zsh
set -euo pipefail

root=${0:A:h:h}
deps=${AETHER_DEPS_ROOT:-${root}/.aether-deps}
sources=${deps}/src
bin=${deps}/bin
mkdir -p ${sources} ${bin}

brush_commit=3edecbb2fe79d3e2c87eeab85b15e0b1dd10d486
brush_source=${sources}/brush
if [[ ! -d ${brush_source}/.git ]]; then
  git clone --filter=blob:none https://github.com/ArthurBrussee/brush.git ${brush_source}
fi
git -C ${brush_source} fetch --depth 1 origin ${brush_commit}
git -C ${brush_source} checkout --detach ${brush_commit}
[[ $(git -C ${brush_source} rev-parse HEAD) == ${brush_commit} ]]
cargo build --locked --release --manifest-path ${brush_source}/Cargo.toml \
  --package brush-app --bin brush_app
ln -sfn ${brush_source}/target/release/brush_app ${bin}/brush

colmap_commit=0b31f98133b470eae62811b557dc2bcff1e4f9a5
if command -v colmap >/dev/null 2>&1; then
  colmap_version=$(colmap --version 2>&1 || true)
  if [[ ${colmap_version} != *3.13.0* ]]; then
    print -u2 "AETHER requires COLMAP 3.13.0; found: ${colmap_version}"
    exit 3
  fi
  ln -sfn $(command -v colmap) ${bin}/colmap
else
  colmap_source=${sources}/colmap
  if [[ ! -d ${colmap_source}/.git ]]; then
    git clone --filter=blob:none https://github.com/colmap/colmap.git ${colmap_source}
  fi
  git -C ${colmap_source} fetch --depth 1 origin ${colmap_commit}
  git -C ${colmap_source} checkout --detach ${colmap_commit}
  [[ $(git -C ${colmap_source} rev-parse HEAD) == ${colmap_commit} ]]
  print -u2 "Pinned COLMAP source is ready at ${colmap_source}."
  print -u2 "COLMAP native dependencies are not installed automatically. Follow docs/RECONSTRUCTION.md, then rerun."
  exit 4
fi

${bin}/brush --version
${bin}/colmap --version
print "AETHER reconstruction tools are ready in ${bin}"
