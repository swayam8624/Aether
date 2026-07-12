#!/bin/zsh
set -euo pipefail
if [[ ${1:-} == --version ]]; then
  print "COLMAP 3.13.0"
  exit 0
fi
command=${1:?missing COLMAP command}
shift
value_after() {
  local key=$1
  shift
  while (( $# > 1 )); do
    if [[ $1 == ${key} ]]; then
      print $2
      return
    fi
    shift
  done
  return 1
}
case ${command} in
  feature_extractor)
    database=$(value_after --database_path $@)
    mkdir -p ${database:h}
    : > ${database}
    ;;
  exhaustive_matcher)
    [[ -f $(value_after --database_path $@) ]]
    ;;
  mapper)
    output=$(value_after --output_path $@)
    mkdir -p ${output}/0
    ;;
  image_undistorter)
    output=$(value_after --output_path $@)
    mkdir -p ${output}/sparse
    ;;
  *)
    print -u2 "Unexpected mock COLMAP command: ${command}"
    exit 9
    ;;
esac
