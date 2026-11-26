#!/usr/bin/env bash
# Checks of the stage 0 C dialect (cesiumlang/cesium#10) with compilers other
# than the devkit's `zig cc`, which build.zig uses.
#
# usage: portable-c.sh compile <kind> <compiler> [<std>]
#          Compile every translation unit of stage 0 and libcsmir, without
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
INCLUDES=(-I"$B/include" -I"$B/libcsmir")
# override where a sanitizer is missing (MinGW has no ASan runtime)
SANITIZE_FLAGS=${PORTABLE_C_SANITIZE:--fsanitize=address,undefined -fno-sanitize-recover=all}

SOURCES=(
  libcsmir/cs_common.c libcsmir/cs_format.c libcsmir/mir.c libcsmir/mir_text.c
  libcsmir/mir_print.c libcsmir/mir_verify.c libcsmir/mir_qbe.c libcsmir/mir_dwarf.c
  libcsmir/mir_codeview.c libcsmir/cs_platform.c libcsmir/csmir_api.c
  stage0/main.c stage0/s0_lex.c stage0/s0_parse.c stage0/s0_lower.c stage0/s0_habit.c
  runtime/cs_arena.c
)

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
        [ "$src" = libcsmir/cs_format.c ] && extra=(-Wno-format-nonliteral)
        "$cc" "-std=$std" "${GNU_FLAGS[@]}" ${extra[@]+"${extra[@]}"} "${INCLUDES[@]}" -c "$B/$src" -o "$obj"
        ;;
      tcc)
        "$cc" "-std=$std" -Wall -Werror "${INCLUDES[@]}" -c "$B/$src" -o "$obj"
        ;;
      msvc)
        # C4996 is MSVC's own deprecation of ISO C functions such as fopen
        # Windows paths and dash options, and the object named relative to
        # its directory, so that an MSYS shell rewrites none of the arguments
        (cd "$dir" && "$cc" -nologo -std:c11 -W4 -WX -wd4996           -I"$(cygpath -m "$B/include")" -I"$(cygpath -m "$B/libcsmir")"           -c "$(cygpath -m "$B/$src")" -Fo"$(basename "$obj")")
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
    expect 0 "s0/$t (MIR)" "$c0" --emit-mir -o "$dir/$t.mir" "${files[@]}"
    expect 0 "s0/$t (QBE IL)" "$c0" --emit-qbe -o "$dir/$t.ssa" "${files[@]}"
    # the MIR text cesium0 printed must parse, verify and print back the same
    expect 0 "s0/$t (MIR round trip)" "$c0" --emit-mir -o "$dir/$t.again.mir" --mir "$dir/$t.mir"
    cmp "$dir/$t.mir" "$dir/$t.again.mir"
    n=$((n + 3))
  done
  # the golden MIR names its file as given: relative to the repository root
  (cd "$ROOT" && "$c0" --emit-mir -o "$dir/tokens.golden.mir" bootstrap/tests/s0/tokens.csm)
  cmp "$dir/tokens.golden.mir" "$B/tests/s0/tokens.mir"
  for t in "$B"/tests/s0/errors/*.csm; do
    expect 1 "s0/errors/$(basename "$t")" "$c0" --emit-mir -o "$dir/err.mir" "$t" "$B/tests/s0/lib.csm"
    n=$((n + 1))
  done
  for t in "$B"/tests/mir/*.mir; do
    expect 0 "mir/$(basename "$t") (print)" "$c0" --emit-mir -o "$dir/m.mir" --mir "$t"
    cmp "$dir/m.mir" "$t"
    expect 0 "mir/$(basename "$t") (QBE IL)" "$c0" --emit-qbe -o "$dir/m.ssa" --mir "$t"
    n=$((n + 2))
  done
  for t in "$B"/tests/mir/errors/*.mir; do
    expect 1 "mir/errors/$(basename "$t")" "$c0" --emit-mir -o "$dir/err.mir" --mir "$t"
    n=$((n + 1))
  done
  # cesium1's own source: the largest S0 program there is
  expect 0 "stage1 (QBE IL)" "$c0" --emit-qbe -o "$dir/cesium1.ssa" \
    "$B"/stage1/util.csm "$B"/stage1/lex.csm "$B"/stage1/parse.csm "$B"/stage1/types.csm \
    "$B"/stage1/habit.csm "$B"/stage1/lower.csm "$B"/stage1/decl.csm "$B"/stage1/loc.csm "$B"/stage1/main.csm
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
