#!/usr/bin/env python3
"""Source check of the stage 0 C dialect (cesiumlang/cesium#10).

Fails if a banned identifier appears in the project's C sources, or if a .c
file does not include the common header. The banned-functions header
(bootstrap/include/cs_banned.h) already makes a use of a banned function fail
to compile; this check also covers the compiler extensions and optional
language features of the dialect's "Not allowed" list, which no header can
catch, and it does not depend on which compiler runs.

Comments are not checked. String literals are checked only for %p. A line
whose comment contains "cs-dialect: allow" is exempt from the identifier
checks.

usage: python tools/check-c-dialect.py

Needs only Python 3 and its standard library.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE_DIRS = ["bootstrap/include", "bootstrap/libcsmir", "bootstrap/stage0", "bootstrap/runtime"]
BANNED_HEADER = "bootstrap/include/cs_banned.h"
COMMON_HEADER = "cs_common.h"

# The "Not allowed" list of the dialect: optional or weakly supported
# standard features and compiler extensions. Whole identifiers.
BANNED_WORDS = {
  "_Complex": "optional standard feature",
  "_Imaginary": "optional standard feature",
  "_Atomic": "optional standard feature",
  "_Thread_local": "optional standard feature",
  "_Generic": "not implemented by every reference compiler",
  "_Alignas": "not implemented by every reference compiler",
  "_Alignof": "not implemented by every reference compiler",
  "_Noreturn": "not implemented by every reference compiler",
  "_Static_assert": "use CS_STATIC_ASSERT",
  "static_assert": "use CS_STATIC_ASSERT",
  "__attribute__": "compiler extension",
  "__declspec": "compiler extension",
  "typeof": "compiler extension before C23",
  "__typeof__": "compiler extension",
  "__int128": "compiler extension",
  "__asm__": "compiler extension",
  "asm": "compiler extension",
  "alloca": "compiler extension",
  "long double": "floating point is not used in stage 0",
  "float": "floating point is not used in stage 0",
  "double": "floating point is not used in stage 0",
}
# identifiers that begin with one of these
BANNED_PREFIXES = {"__builtin_": "compiler extension"}
# headers outside the allowed list
ALLOWED_HEADERS = {
  "stddef.h", "stdint.h", "inttypes.h", "stdbool.h", "limits.h", "stdarg.h",
  "assert.h", "string.h", "stdio.h", "stdlib.h", "errno.h",
}

# A line with this text in a comment is exempt from the identifier checks.
# It needs a reason next to it.
ALLOW = "cs-dialect: allow"

IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def banned_functions():
  """The function names cs_banned.h bans, read from its #define lines."""
  names = set()
  with open(os.path.join(ROOT, BANNED_HEADER), encoding="utf-8") as f:
    for line in f:
      m = re.match(r"\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\b.*CS_BANNED\(", line)
      if m and m.group(1) != "CS_BANNED":
        names.add(m.group(1))
  if not names:
    sys.exit(f"{BANNED_HEADER}: found no banned functions")
  return names


def split_code(text):
  """Return (code, strings): the text with comments blanked, once with string
  and character literals blanked too and once with only them kept. Line
  structure is preserved in both."""
  code = []
  strings = []
  i = 0
  n = len(text)
  while i < n:
    c = text[i]
    two = text[i:i + 2]
    if two == "//":
      j = text.find("\n", i)
      j = n if j < 0 else j
      code.append(" " * (j - i))
      strings.append(" " * (j - i))
      i = j
    elif two == "/*":
      j = text.find("*/", i + 2)
      j = n if j < 0 else j + 2
      blank = re.sub(r"[^\n]", " ", text[i:j])
      code.append(blank)
      strings.append(blank)
      i = j
    elif c in "\"'":
      j = i + 1
      while j < n and text[j] != c and text[j] != "\n":
        j += 2 if text[j] == "\\" else 1
      j = min(j + 1, n)
      code.append(c + " " * (j - i - 2) + c if j - i >= 2 else " ")
      strings.append(text[i:j])
      i = j
    else:
      code.append(c)
      strings.append(c if c == "\n" else " ")
      i += 1
  return "".join(code), "".join(strings)


def check_file(path, functions, problems):
  rel = os.path.relpath(path, ROOT).replace("\\", "/")
  with open(path, encoding="utf-8") as f:
    text = f.read()
  code, strings = split_code(text)
  is_banned_header = rel == BANNED_HEADER

  for number, (line, raw, lits) in enumerate(zip(code.split("\n"), text.split("\n"), strings.split("\n")), 1):
    stripped = line.strip()
    if stripped.startswith("#"):
      directive = stripped[1:].strip()
      if directive.startswith("pragma"):
        problems.append(f"{rel}:{number}: #pragma is a compiler extension")
      m = re.match(r"include\s*<([^>]+)>", raw.strip()[1:].strip())
      if m and m.group(1) not in ALLOWED_HEADERS:
        problems.append(f"{rel}:{number}: <{m.group(1)}> is not one of the allowed ISO C headers")
    if "%p" in lits.replace("%%", ""):
      problems.append(f"{rel}:{number}: %p prints a pointer value, which makes output nondeterministic")
    if is_banned_header or ALLOW in raw:
      # cs_banned.h names every banned function on purpose
      continue
    if re.search(r"\blong\s+double\b", line):
      problems.append(f"{rel}:{number}: long double: {BANNED_WORDS['long double']}")
    for m in IDENT.finditer(line):
      word = m.group(0)
      if word in functions:
        problems.append(f"{rel}:{number}: {word} is a banned function ({BANNED_HEADER})")
      elif word in BANNED_WORDS:
        problems.append(f"{rel}:{number}: {word}: {BANNED_WORDS[word]}")
      else:
        for prefix, why in BANNED_PREFIXES.items():
          if word.startswith(prefix):
            problems.append(f"{rel}:{number}: {word}: {why}")

  if rel.endswith(".c") and not re.search(r'^\s*#\s*include\s*"' + re.escape(COMMON_HEADER) + '"', text, re.M):
    # a project header that includes the common header counts
    includes = re.findall(r'^\s*#\s*include\s*"([^"]+)"', text, re.M)
    if not any(includes_common(os.path.dirname(path), name, set()) for name in includes):
      problems.append(f"{rel}: does not include {COMMON_HEADER}")


def includes_common(directory, name, seen):
  """Whether the project header `name` includes the common header, directly
  or through other project headers."""
  for base in [directory] + [os.path.join(ROOT, d) for d in SOURCE_DIRS]:
    path = os.path.join(base, name)
    if os.path.exists(path):
      break
  else:
    return False
  path = os.path.abspath(path)
  if path in seen:
    return False
  seen.add(path)
  with open(path, encoding="utf-8") as f:
    text = f.read()
  includes = re.findall(r'^\s*#\s*include\s*"([^"]+)"', text, re.M)
  if COMMON_HEADER in includes:
    return True
  return any(includes_common(os.path.dirname(path), other, seen) for other in includes)


def main():
  functions = banned_functions()
  problems = []
  count = 0
  for directory in SOURCE_DIRS:
    for name in sorted(os.listdir(os.path.join(ROOT, directory))):
      if name.endswith((".c", ".h")):
        check_file(os.path.join(ROOT, directory, name), functions, problems)
        count += 1
  for problem in problems:
    print(problem)
  if problems:
    sys.exit(f"{len(problems)} problem(s) in the stage 0 C dialect check")
  print(f"C dialect source check: {count} files, {len(functions)} banned functions, no problems")


if __name__ == "__main__":
  main()
