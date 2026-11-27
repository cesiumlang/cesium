#!/usr/bin/env bash
# Checks of the stage 0 C dialect (cesiumlang/cesium#10) with compilers other
# than the devkit's `zig cc`, which build.zig uses.
#
# usage: portable-c.sh compile <kind> <compiler> [<std>]
#          Compile every translation unit of stage 0 and libcsir, without
#          linking. <kind> selects the flags: gnu (GCC or Clang, with the
#          dialect's full warning set), tcc (TinyCC) or msvc (cl).
#          <std> is c99 or c11 (default: both, one after the other; msvc
#          has only c11).
#        portable-c.sh sanitize <compiler>
#          Build cesium0 with AddressSanitizer and UndefinedBehaviorSanitizer
#          (GCC or Clang) and run the test corpus through it up to QBE IL.
#          QBE itself is replaced by a stub: it is third-party code and is
#          not what is checked here.
#        portable-c.sh fuzz <compiler> [<iterations> [<seed>]]
#          The same build, run by the fuzzer (zig build fuzz, tools/fuzz-stage0)
#          on damaged copies of the corpus (default: 2000 of them, seed 1).
#          Needs the devkit's Zig ($ZIG, else .devkit/zig/zig, else zig).
#
# Run from anywhere; objects go to $PORTABLE_C_OUT (default: tmp/portable-c).
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${PORTABLE_C_OUT:-$ROOT/tmp/portable-c}
B=$ROOT/bootstrap

# the flags of cesiumlang/cesium#10, as in build-src/__root__.zig
GNU_FLAGS=(-pedantic-errors -Wall -Wextra -Wvla -Wconversion -Wformat=2 -Wformat-nonliteral
  -Wno-missing-format-attribute -Wimplicit-fallthrough -Wstrict-prototypes -Wmissing-prototypes -Werror)
INCLUDES=(-I"$B/include" -I"$B/libcsir")
# override where a sanitizer is missing (MinGW has no ASan runtime)
SANITIZE_FLAGS=${PORTABLE_C_SANITIZE:--fsanitize=address,undefined -fno-sanitize-recover=all}

# the names of bootstrap/<dir>/sources.txt, the lists the build reads too
# (build-src/sources.zig), each prefixed with <prefix>
list_sources () { # <dir> <prefix>
  local line
  while IFS= read -r line || [ -n "$line" ]; do
    line=${line%$'\r'}
    case $line in '' | '#'*) continue ;; esac
    printf '%s\n' "$2$line"
  done < "$B/$1/sources.txt"
}

# a read loop, not mapfile: the bash of macOS is 3.2
SOURCES=()
STAGE1=()
while IFS= read -r f; do SOURCES+=("$f"); done < <(list_sources libcsir libcsir/; list_sources stage0 stage0/)
SOURCES+=(runtime/cs_arena.c)
while IFS= read -r f; do STAGE1+=("$f"); done < <(list_sources stage1 "$B/stage1/")

usage () { sed -n '2,21p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//' >&2; exit 2; }

compile_all () { # <kind> <compiler> <std>
  local kind=$1 cc=$2 std=$3 src obj dir=$OUT/$1-$3
  rm -rf "$dir"
  mkdir -p "$dir"
  for src in "${SOURCES[@]}"; do
    obj=$dir/$(basename "${src%.c}").o
    case $kind in
      gnu)
        # cs_format.c is the one file that forwards format strings
        local extra=()
        [ "$src" = libcsir/cs_format.c ] && extra=(-Wno-format-nonliteral)
        "$cc" "-std=$std" "${GNU_FLAGS[@]}" ${extra[@]+"${extra[@]}"} "${INCLUDES[@]}" -c "$B/$src" -o "$obj"
        ;;
      tcc)
        "$cc" "-std=$std" -Wall -Werror "${INCLUDES[@]}" -c "$B/$src" -o "$obj"
        ;;
      msvc)
        # C4996 is MSVC's own deprecation of ISO C functions such as fopen
        # Windows paths and dash options, and the object named relative to
        # its directory, so that an MSYS shell rewrites none of the arguments
        (cd "$dir" && "$cc" -nologo -std:c11 -W4 -WX -wd4996           -I"$(cygpath -m "$B/include")" -I"$(cygpath -m "$B/libcsir")"           -c "$(cygpath -m "$B/$src")" -Fo"$(basename "$obj")")
        ;;
      *) usage ;;
    esac
  done
  echo "ok: ${#SOURCES[@]} translation units compile with $cc ($kind, $std)"
}

build_sanitized () { # <compiler> <directory>: cesium0 with the sanitizers
  local cc=$1 dir=$2 src objs=()
  rm -rf "$dir"
  mkdir -p "$dir"
  # QBE's entry point, which cesium0 reaches only without --emit-*
  printf 'int qbe_main(int argc, char *argv[]);\nint qbe_main(int argc, char *argv[]) { (void)argc; (void)argv; return 1; }\n' > "$dir/qbe_stub.c"
  for src in "${SOURCES[@]}"; do
    [ "$src" = runtime/cs_arena.c ] && continue
    objs+=("$B/$src")
  done
  # shellcheck disable=SC2086  # the flags are split on purpose
  "$cc" -std=c11 -g -O1 -fno-omit-frame-pointer $SANITIZE_FLAGS \
    "${INCLUDES[@]}" "${objs[@]}" "$dir/qbe_stub.c" -o "$dir/cesium0"
  export ASAN_OPTIONS=detect_leaks=0:abort_on_error=0 UBSAN_OPTIONS=print_stacktrace=1
}

fuzz () { # <compiler> <iterations> <seed>
  local dir=$OUT/fuzz zig=${ZIG:-$ROOT/.devkit/zig/zig}
  [ -x "$zig" ] || zig=zig
  build_sanitized "$1" "$dir"
  (cd "$ROOT" && "$zig" build fuzz -- "$dir/cesium0" --iterations "$2" --seed "$3" --out "$dir/inputs")
}

sanitize () { # <compiler>
  local cc=$1 dir=$OUT/sanitize t rc
  build_sanitized "$cc" "$dir"
  local c0=$dir/cesium0

  expect () { # <wanted exit status> <label> <command>...
    local want=$1 label=$2
    shift 2
    set +e
    "$@" > "$dir/last.out" 2> "$dir/last.err"
    rc=$?
    set -e
    if [ "$rc" != "$want" ]; then
      echo "FAIL $label: exit status $rc, expected $want" >&2
      cat "$dir/last.err" >&2
      exit 1
    fi
  }

  local n=0
  for t in tokens arith pointers structs control globals; do
    local files=("$B/tests/s0/$t.csm")
    [ "$t" = tokens ] || files+=("$B/tests/s0/lib.csm")
    expect 0 "s0/$t (SIR)" "$c0" --emit-sir -o "$dir/$t.sir" "${files[@]}"
    expect 0 "s0/$t (QBE IL)" "$c0" --emit-qbe -o "$dir/$t.ssa" "${files[@]}"
    # the SIR text cesium0 printed must parse, verify and print back the same
    expect 0 "s0/$t (SIR round trip)" "$c0" --emit-sir -o "$dir/$t.again.sir" --sir "$dir/$t.sir"
    cmp "$dir/$t.sir" "$dir/$t.again.sir"
    n=$((n + 3))
  done
  # the golden SIR names its file as given: relative to the repository root
  (cd "$ROOT" && "$c0" --emit-sir -o "$dir/tokens.golden.sir" bootstrap/tests/s0/tokens.csm)
  cmp "$dir/tokens.golden.sir" "$B/tests/s0/tokens.sir"
  for t in "$B"/tests/s0/errors/*.csm; do
    expect 1 "s0/errors/$(basename "$t")" "$c0" --emit-sir -o "$dir/err.sir" "$t" "$B/tests/s0/lib.csm"
    n=$((n + 1))
  done
  for t in "$B"/tests/sir/*.sir; do
    expect 0 "sir/$(basename "$t") (print)" "$c0" --emit-sir -o "$dir/m.sir" --sir "$t"
    cmp "$dir/m.sir" "$t"
    expect 0 "sir/$(basename "$t") (QBE IL)" "$c0" --emit-qbe -o "$dir/m.ssa" --sir "$t"
    n=$((n + 2))
  done
  for t in "$B"/tests/sir/errors/*.sir; do
    expect 1 "sir/errors/$(basename "$t")" "$c0" --emit-sir -o "$dir/err.sir" --sir "$t"
    n=$((n + 1))
  done
  # cesium1's own source: the largest S0 program there is
  expect 0 "stage1 (QBE IL)" "$c0" --emit-qbe -o "$dir/cesium1.ssa" "${STAGE1[@]}"
  n=$((n + 1))
  echo "ok: $n runs of cesium0 built by $cc with $SANITIZE_FLAGS, no report"
}

case ${1:-} in
  compile)
    [ $# -ge 3 ] || usage
    if [ $# -ge 4 ]; then
      compile_all "$2" "$3" "$4"
    elif [ "$2" = msvc ]; then
      compile_all "$2" "$3" c11
    else
      compile_all "$2" "$3" c99
      compile_all "$2" "$3" c11
    fi
    ;;
  sanitize)
    [ $# -eq 2 ] || usage
    sanitize "$2"
    ;;
  fuzz)
    [ $# -ge 2 ] && [ $# -le 4 ] || usage
    fuzz "$2" "${3:-2000}" "${4:-1}"
    ;;
  *) usage ;;
esac
