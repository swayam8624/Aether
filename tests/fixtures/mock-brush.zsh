#!/bin/zsh
set -euo pipefail
if [[ ${1:-} == --version ]]; then
  print "brush-app 0.3.0"
  exit 0
fi
shift
export_path=
export_name=
while (( $# > 1 )); do
  case $1 in
    --export-path) export_path=$2 ;;
    --export-name) export_name=$2 ;;
  esac
  shift
done
[[ -n ${export_path} && -n ${export_name} ]]
mkdir -p ${export_path}
: > ${export_path}/${export_name}
