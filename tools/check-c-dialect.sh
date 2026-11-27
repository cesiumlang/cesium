#!/usr/bin/env bash
# Source check of the stage 0 C dialect (cesiumlang/cesium#10).
#
# Fails if a banned identifier appears in the project's C sources, or if a .c
# file does not include the common header. The banned-functions header
# (bootstrap/include/cs_banned.h) already makes a use of a banned function fail
# to compile; this check also covers the compiler extensions and optional
# language features of the dialect's "Not allowed" list, which no header can
# catch, and it does not depend on which compiler runs.
#
# Comments are not checked. String literals are checked only for %p. A line
# whose comment contains "cs-dialect: allow" is exempt from the identifier
# checks.
#
# usage: bash tools/check-c-dialect.sh
#
# Needs bash and a POSIX awk.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SOURCE_DIRS=(bootstrap/include bootstrap/libcsir bootstrap/stage0 bootstrap/runtime)
BANNED_HEADER=bootstrap/include/cs_banned.h
COMMON_HEADER=cs_common.h
PLATFORM_FILE=bootstrap/libcsir/cs_platform.c

die () { printf '%s\n' "$*" >&2; exit 1; }

# the function names cs_banned.h bans, read from its #define lines
FUNCTIONS=$(awk '
  match($0, /^[ \t]*#[ \t]*define[ \t]+[A-Za-z_][A-Za-z0-9_]*/) {
    head = substr($0, 1, RLENGTH); rest = substr($0, RLENGTH + 1)
    match(head, /[A-Za-z_][A-Za-z0-9_]*$/); name = substr(head, RSTART)
    if (index(rest, "CS_BANNED(") && name != "CS_BANNED") print name
  }' "$ROOT/$BANNED_HEADER" | LC_ALL=C sort -u)
[ -n "$FUNCTIONS" ] || die "$BANNED_HEADER: found no banned functions"

# check_lines <file> <relative path>: the per-line checks, one problem a line
check_lines () {
  awk -v rel="$2" -v functions="${FUNCTIONS//$'\n'/ }" -v banned_header="$BANNED_HEADER" -v platform_file="$PLATFORM_FILE" '
    BEGIN {
      n = split(functions, f, " "); for (k = 1; k <= n; k++) banned[f[k]] = 1
      # the "Not allowed" list of the dialect: optional or weakly supported
      # standard features and compiler extensions. Whole identifiers.
      why["_Complex"] = "optional standard feature"
      why["_Imaginary"] = "optional standard feature"
      why["_Atomic"] = "optional standard feature"
      why["_Thread_local"] = "optional standard feature"
      why["_Generic"] = "not implemented by every reference compiler"
      why["_Alignas"] = "not implemented by every reference compiler"
      why["_Alignof"] = "not implemented by every reference compiler"
      why["_Noreturn"] = "not implemented by every reference compiler"
      why["_Static_assert"] = "use CS_STATIC_ASSERT"
      why["static_assert"] = "use CS_STATIC_ASSERT"
      why["__attribute__"] = "compiler extension"
      why["__declspec"] = "compiler extension"
      why["typeof"] = "compiler extension before C23"
      why["__typeof__"] = "compiler extension"
      why["__int128"] = "compiler extension"
      why["__asm__"] = "compiler extension"
      why["asm"] = "compiler extension"
      why["alloca"] = "compiler extension"
      why["float"] = "floating point is not used in stage 0"
      why["double"] = "floating point is not used in stage 0"
      long_double = "floating point is not used in stage 0"
      # identifiers that begin with this
      prefix = "__builtin_"; prefix_why = "compiler extension"
      # headers outside the allowed list
      split("stddef.h stdint.h inttypes.h stdbool.h limits.h stdarg.h assert.h string.h stdio.h stdlib.h errno.h", h, " ")
      for (k in h) allowed[h[k]] = 1
      # the platform wrappers, the one file that calls the platform (P-0008,
      # point 14), may include its headers too
      if (rel == platform_file) allowed["unistd.h"] = 1
      # a line with this text in a comment is exempt from the identifier
      # checks; it needs a reason next to it
      allow = "cs-dialect: allow"
      in_comment = 0
    }
    function spaces(k) { while (length(blank) < k) blank = blank blank " "; return substr(blank, 1, k) }
    function trim(s) { gsub(/^[ \t\f\v]+|[ \t\f\v]+$/, "", s); return s }
    # split the line into code (comments, string and character literals
    # blanked) and lits (only the literals kept), both as long as the line
    function split_code(raw,   rest, c, j, k, l) {
      code = ""; lits = ""; rest = raw
      while (rest != "") {
        if (in_comment) {
          k = index(rest, "*/")
          l = k ? k + 1 : length(rest)
          code = code spaces(l); lits = lits spaces(l); rest = substr(rest, l + 1)
          if (k) in_comment = 0
          continue
        }
        if (!match(rest, /\/\/|\/\*|["\047]/)) {
          code = code rest; lits = lits spaces(length(rest)); return
        }
        code = code substr(rest, 1, RSTART - 1); lits = lits spaces(RSTART - 1)
        rest = substr(rest, RSTART); c = substr(rest, 1, 1)
        if (substr(rest, 1, 2) == "//") {
          code = code spaces(length(rest)); lits = lits spaces(length(rest)); return
        }
        if (substr(rest, 1, 2) == "/*") {
          code = code "  "; lits = lits "  "; rest = substr(rest, 3); in_comment = 1
          continue
        }
        l = length(rest)
        for (j = 2; j <= l && substr(rest, j, 1) != c; j += (substr(rest, j, 1) == "\\") ? 2 : 1) ;
        if (j > l) j = l
        code = code (j >= 2 ? c spaces(j - 2) c : " ")
        lits = lits substr(rest, 1, j); rest = substr(rest, j + 1)
      }
    }
    function problem(text) { print rel ":" FNR ": " text }
    {
      sub(/\r$/, "")
      raw = $0
      split_code(raw)
      stripped = trim(code)
      if (substr(stripped, 1, 1) == "#") {
        if (substr(trim(substr(stripped, 2)), 1, 6) == "pragma") problem("#pragma is a compiler extension")
        directive = trim(substr(trim(raw), 2))
        if (match(directive, /^include[ \t\f\v]*<[^>]+>/)) {
          header = substr(directive, 1, RLENGTH); sub(/^include[ \t\f\v]*</, "", header); sub(/>$/, "", header)
          if (!(header in allowed)) problem("<" header "> is not one of the allowed ISO C headers")
        }
      }
      l = lits; gsub(/%%/, "", l)
      if (index(l, "%p")) problem("%p prints a pointer value, which makes output nondeterministic")
      # cs_banned.h names every banned function on purpose
      if (rel == banned_header || index(raw, allow)) next
      if (match(code, /(^|[^A-Za-z0-9_])long[ \t\f\v]+double([^A-Za-z0-9_]|$)/)) problem("long double: " long_double)
      s = code
      while (match(s, /[A-Za-z_][A-Za-z0-9_]*/)) {
        word = substr(s, RSTART, RLENGTH); s = substr(s, RSTART + RLENGTH)
        if (word in banned) problem(word " is a banned function (" banned_header ")")
        else if (word in why) problem(word ": " why[word])
        else if (substr(word, 1, length(prefix)) == prefix) problem(word ": " prefix_why)
      }
    }' "$1"
}

# quoted_includes <file>: set INCLUDES to the project headers the file
# includes, in order
INCLUDE_RE='^[[:space:]]*#[[:space:]]*include[[:space:]]*"([^"]+)"'
quoted_includes () {
  local line
  INCLUDES=()
  while IFS= read -r line || [ -n "$line" ]; do
    [[ $line =~ $INCLUDE_RE ]] && INCLUDES+=("${BASH_REMATCH[1]}")
  done < "$1"
  return 0
}

# normalize <path>: set NORMAL to <path> without . and .. components
normalize () {
  local part parts=() IFS=/
  for part in $1; do
    case $part in
      '' | .) ;;
      ..) [ ${#parts[@]} -gt 0 ] && unset 'parts[${#parts[@]}-1]' ;;
      *) parts+=("$part") ;;
    esac
  done
  NORMAL="/${parts[*]}"
}

# includes_common <directory> <name>: whether the project header <name>
# includes the common header, directly or through other project headers.
# SEEN holds the headers already read.
includes_common () {
  local base path='' other
  for base in "$1" "${SOURCE_DIRS[@]/#/$ROOT/}"; do
    if [ -e "$base/$2" ]; then path=$base/$2; break; fi
  done
  [ -n "$path" ] || return 1
  normalize "$path"
  path=$NORMAL
  case $SEEN in *"|$path|"*) return 1 ;; esac
  SEEN="$SEEN|$path|"
  quoted_includes "$path"
  local includes=(${INCLUDES[@]+"${INCLUDES[@]}"})
  for other in ${includes[@]+"${includes[@]}"}; do
    [ "$other" = "$COMMON_HEADER" ] && return 0
  done
  for other in ${includes[@]+"${includes[@]}"}; do
    includes_common "${path%/*}" "$other" && return 0
  done
  return 1
}

check_file () { # <path> <relative path>
  check_lines "$1" "$2"
  case $2 in *.c) ;; *) return 0 ;; esac
  local name includes
  quoted_includes "$1"
  includes=(${INCLUDES[@]+"${INCLUDES[@]}"})
  for name in ${includes[@]+"${includes[@]}"}; do
    [ "$name" = "$COMMON_HEADER" ] && return 0
  done
  # a project header that includes the common header counts
  for name in ${includes[@]+"${includes[@]}"}; do
    SEEN=
    includes_common "${1%/*}" "$name" && return 0
  done
  echo "$2: does not include $COMMON_HEADER"
}

problems=$(
  for directory in "${SOURCE_DIRS[@]}"; do
    for name in $(cd "$ROOT/$directory" && ls -A | LC_ALL=C sort); do
      case $name in *.c | *.h) check_file "$ROOT/$directory/$name" "$directory/$name" ;; esac
    done
  done
)
count=0
for directory in "${SOURCE_DIRS[@]}"; do
  for name in "$ROOT/$directory"/*.c "$ROOT/$directory"/*.h; do
    [ -f "$name" ] && count=$((count + 1))
  done
done
if [ -n "$problems" ]; then
  printf '%s\n' "$problems"
  die "$(grep -c . <<< "$problems") problem(s) in the stage 0 C dialect check"
fi
echo "C dialect source check: $count files, $(grep -c . <<< "$FUNCTIONS") banned functions, no problems"
