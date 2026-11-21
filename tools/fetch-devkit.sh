#!/usr/bin/env bash
# Download the Cesium devkit for this machine and unpack it to .devkit.
#
# The devkit (https://github.com/cesiumlang/devkits) holds the pinned Zig, the
# LLVM and LLD libraries and QBE. The version to use is the one line of the
# file devkit-version in the repository root. The archive is checked against
# the sha256 that the release's devkits.json gives for it.
#
# usage: bash tools/fetch-devkit.sh [--host <triple>] [--dir <directory>]
#
# Needs bash, curl, sha256sum (or shasum), and tar (unzip for a .zip): Git
# Bash on Windows, Linux, macOS.
set -euo pipefail
REPO=https://github.com/cesiumlang/devkits
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

die () { printf '%s\n' "$*" >&2; exit 1; }
usage () { sed -n '2,12p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

host_triple () {
  local machine system arch os
  machine=$(uname -m) system=$(uname -s)
  case $machine in
    x86_64 | amd64 | AMD64) arch=x86_64 ;;
    aarch64 | arm64 | ARM64) arch=aarch64 ;;
    *) arch= ;;
  esac
  case $system in
    Linux) os=linux-musl ;;
    MINGW* | MSYS* | CYGWIN*) os=windows-gnu ;;
    Darwin) os=macos-none ;;
    *) os= ;;
  esac
  [ -n "$arch" ] && [ -n "$os" ] || die "no devkit for $system on $machine; pass --host"
  printf '%s\n' "$arch-$os"
}

# json_paths: every scalar of the JSON on stdin as "a.b.c<TAB>value"; array
# elements are numbered from 0. String escapes are kept as they are.
json_paths () {
  awk '
    function emit(v,   p, k) {
      p = ""
      for (k = 1; k <= depth; k++) p = p (k > 1 ? "." : "") key[k]
      print p "\t" v
    }
    { text = text $0 "\n" }
    END {
      n = length(text); i = 1; depth = 0
      while (i <= n) {
        c = substr(text, i, 1)
        if (c == "{") { depth++; kind[depth] = "o"; key[depth] = ""; want_key[depth] = 1; i++ }
        else if (c == "[") { depth++; kind[depth] = "a"; key[depth] = 0; i++ }
        else if (c == "}" || c == "]") { depth--; i++ }
        else if (c == ",") { if (kind[depth] == "a") key[depth]++; else want_key[depth] = 1; i++ }
        else if (c == ":") { want_key[depth] = 0; i++ }
        else if (c == "\"") {
          s = ""
          for (j = i + 1; j <= n; j++) {
            d = substr(text, j, 1)
            if (d == "\\") { s = s substr(text, j, 2); j++; continue }
            if (d == "\"") break
            s = s d
          }
          i = j + 1
          if (kind[depth] == "o" && want_key[depth]) key[depth] = s
          else emit(s)
        }
        else if (c ~ /[ \t\r\n]/) i++
        else {
          for (j = i; j <= n && substr(text, j, 1) !~ /[,}\] \t\r\n]/; j++) ;
          emit(substr(text, i, j - i)); i = j
        }
      }
    }'
}

# json_get <path>: the value at <path> in the json_paths lines on stdin
json_get () { awk -F '\t' -v p="$1" '$1 == p { print $2; exit }'; }

sha256_of () {
  if command -v sha256sum > /dev/null; then
    sha256sum "$1" | cut -d ' ' -f 1
  else
    shasum -a 256 "$1" | cut -d ' ' -f 1
  fi
}

unzip_to () { # <archive> <directory>
  if command -v unzip > /dev/null; then
    unzip -q "$1" -d "$2"
  elif command -v bsdtar > /dev/null; then
    bsdtar -xf "$1" -C "$2"
  elif [ -x /c/Windows/System32/tar.exe ]; then
    /c/Windows/System32/tar.exe -xf "$1" -C "$2"
  else
    die "no unzip to unpack $1"
  fi
}

host=''
dir=$ROOT/.devkit
while [ $# -gt 0 ]; do
  case $1 in
    --host) [ $# -ge 2 ] || usage 2; host=$2; shift 2 ;;
    --host=*) host=${1#--host=}; shift ;;
    --dir) [ $# -ge 2 ] || usage 2; dir=$2; shift 2 ;;
    --dir=*) dir=${1#--dir=}; shift ;;
    -h | --help) usage ;;
    *) usage 2 >&2 ;;
  esac
done
[ -n "$host" ] || host=$(host_triple)
dir=$(mkdir -p "$(dirname "$dir")" && cd "$(dirname "$dir")" && pwd)/$(basename "$dir")

version=$(tr -d ' \t\r\n' < "$ROOT/devkit-version")

stamp=$dir/manifest.json
if [ -f "$stamp" ]; then
  have=$(json_paths < "$stamp")
  if [ "$(json_get version <<< "$have")" = "$version" ] && [ "$(json_get host <<< "$have")" = "$host" ]; then
    echo "devkit $version for $host is already in $dir"
    exit 0
  fi
fi

base=$REPO/releases/download/v$version
index=$(curl -fsSL --retry 3 "$base/devkits.json" | json_paths) || die "cannot download $base/devkits.json"
asset=$(json_get "devkits.$host.asset" <<< "$index")
if [ -z "$asset" ]; then
  hosts=$(awk -F '\t' '$1 ~ /^devkits\.[^.]+\.asset$/ { split($1, p, "."); print p[2] }' <<< "$index" | sort | paste -sd ',' - | sed 's/,/, /g')
  die "devkit $version has no archive for $host; it has: $hosts"
fi
want=$(json_get "devkits.$host.sha256" <<< "$index")
url=$(json_get "devkits.$host.url" <<< "$index")

downloads=$ROOT/.devkit-downloads
mkdir -p "$downloads"
archive=$downloads/$asset
if [ ! -f "$archive" ] || [ "$(sha256_of "$archive")" != "$want" ]; then
  echo "downloading $url"
  curl -fL --retry 3 -o "$archive" "$url"
fi
got=$(sha256_of "$archive")
[ "$got" = "$want" ] || die "$asset: sha256 $got, devkits.json says $want"

# the archive holds one top-level directory; its contents become --dir
unpack=$dir.unpack
rm -rf "$unpack" "$dir"
mkdir -p "$unpack"
echo "unpacking $asset"
case $asset in
  *.zip) unzip_to "$archive" "$unpack" ;;
  *) tar -xf "$archive" -C "$unpack" ;;
esac
shopt -s nullglob dotglob
tops=("$unpack"/*)
shopt -u nullglob dotglob
[ ${#tops[@]} -eq 1 ] || die "$asset: expected one top-level directory, found ${#tops[@]}"
mv "${tops[0]}" "$dir"
rmdir "$unpack"
echo "devkit $version for $host: $dir"
