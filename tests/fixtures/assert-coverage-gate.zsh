#!/bin/zsh
set -euo pipefail
reconstruct=${1:?missing reconstruct executable}
dataset=${2:?missing dataset}
output=${3:?missing output directory}
colmap=${4:?missing COLMAP mock}
brush=${5:?missing Brush mock}
rm -rf ${output}
set +e
${reconstruct} ${dataset} --output ${output} --colmap ${colmap} --brush ${brush} \
  --seed 42 --steps 10 --json >/dev/null 2>${output}.stderr
exit_code=$?
set -e
[[ ${exit_code} == 5 ]]
[[ -f ${output}/pose-coverage.json ]]
[[ ! -e ${output}/pose-coverage-validation.complete ]]
[[ ! -e ${output}/exports/base-gaussians.ply ]]
/usr/bin/grep -q '"passed":false' ${output}/pose-coverage.json
/usr/bin/grep -q '"status":"coverage-failed"' ${output}/job.json
