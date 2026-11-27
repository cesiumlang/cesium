#!/usr/bin/env bash
# version.sh: the compiler's version and every place that states it, run from
# the repository root by ci.yml, as vscode-cesium's version.sh. build.zig.zon
# holds it. A new place that states the version is added here.
#   version.sh           prints the version; fails, listing the places, when
#                        one is missing or wrong
#   version.sh set NEXT  sets every place to NEXT, as CI does when it begins
#                        the next version
set -euo pipefail

zon=build.zig.zon

zon_version() { sed -nE 's/^    \.version = "([^"]+)",$/\1/p' "$zon"; }

# places: one "FILE: WANTED GOT" line for each place that states the version
places() {
  local version
  version=$(zon_version)
  echo "$zon: $version $version"
}

check() {
  local version list
  version=$(zon_version)
  list=$(places)
  if ! [[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || [ "$(grep -c . <<< "$list")" -ne 1 ] \
    || grep -qvE '^[^:]+: ([^ ]+) \1$' <<< "$list"; then
    echo "version.sh: build.zig.zon's .version must be MAJOR.MINOR.PATCH ('$version'); found (wanted, got):" >&2
    echo "$list" >&2
    exit 1
  fi
  echo "$version"
}

set_version() {
  local next=$1
  [[ $next =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "version.sh: '$next' is not MAJOR.MINOR.PATCH" >&2; exit 1; }
  sed -i -E "s/^(    \.version = \")[^\"]+(\",)$/\1$next\2/" "$zon"
  check > /dev/null
}

case "${1:-}" in
  "") check ;;
  set) set_version "${2:?version.sh set NEXT}" ;;
  *) echo "version.sh: [set NEXT]" >&2; exit 1 ;;
esac
