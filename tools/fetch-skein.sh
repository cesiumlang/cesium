#!/usr/bin/env bash
# Download Skein for this machine and unpack it to .skein-bin.
#
# Skein (https://github.com/chasingdaylight/skein) checks the proposals, the
# specification and the tests' citations of it (.skein/). The version to use
# is the one line of the file skein-version in the repository root. The
# archive is checked against the release's SHA256SUMS. Developers get skein
# with the VS Code extension chasingdaylight.skein instead; CI uses this.
#
# usage: bash tools/fetch-skein.sh [--dir <directory>]
# then:  .skein-bin/bin/skein check
#
# Needs bash, curl, sha256sum (or shasum), and tar (unzip on Windows).
set -euo pipefail
REPO=https://github.com/chasingdaylight/skein
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

die () { printf '%s\n' "$*" >&2; exit 1; }

dir=$ROOT/.skein-bin
while [ $# -gt 0 ]; do
  case $1 in
    --dir) dir=${2:?--dir needs a directory}; shift 2 ;;
    -h | --help) sed -n '2,13p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
done

version=$(tr -d ' \r\n' < "$ROOT/skein-version")
[ -n "$version" ] || die "skein-version is empty"

case $(uname -m) in
  x86_64 | amd64 | AMD64) arch=x86_64 ;;
  aarch64 | arm64 | ARM64) arch=aarch64 ;;
  *) die "no skein for $(uname -m)" ;;
esac
case $(uname -s) in
  Linux) os=linux ext=tar.gz ;;
  Darwin) os=macos ext=tar.gz ;;
  MINGW* | MSYS* | CYGWIN*) os=windows ext=zip ;;
  *) die "no skein for $(uname -s)" ;;
esac

name=skein-v$version-$arch-$os
if [ -x "$dir/bin/skein" ] || [ -x "$dir/bin/skein.exe" ]; then
  [ "$(cat "$dir/version" 2>/dev/null)" = "$version" ] && { echo "skein $version is in $dir"; exit 0; }
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
url=$REPO/releases/download/v$version
curl -fsSL --retry 3 -o "$work/$name.$ext" "$url/$name.$ext"
curl -fsSL --retry 3 -o "$work/SHA256SUMS" "$url/SHA256SUMS"

want=$(awk -v f="$name.$ext" '$2 == f || $2 == "*" f { print $1 }' "$work/SHA256SUMS")
[ -n "$want" ] || die "SHA256SUMS of v$version has no $name.$ext"
if command -v sha256sum > /dev/null; then got=$(sha256sum "$work/$name.$ext" | awk '{ print $1 }')
else got=$(shasum -a 256 "$work/$name.$ext" | awk '{ print $1 }'); fi
[ "$got" = "$want" ] || die "$name.$ext: sha256 $got, expected $want"

case $ext in
  zip) unzip -q "$work/$name.$ext" -d "$work" ;;
  *) tar -C "$work" -xzf "$work/$name.$ext" ;;
esac
rm -rf "$dir"
mkdir -p "$(dirname "$dir")"
mv "$work/$name" "$dir"
printf '%s\n' "$version" > "$dir/version"
echo "skein $version is in $dir"
