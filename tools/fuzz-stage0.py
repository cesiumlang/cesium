#!/usr/bin/env python3
"""Mutation fuzzing of cesium0's two readers: the MIR text parser and the S0
front end (cesiumlang/cesium#10, "consider fuzzing the MIR text parser").

Takes the files of the test corpus, damages each a little, and runs cesium0
on the result up to QBE IL. Whatever the input, cesium0 must exit with status
0 (accepted) or 1 (rejected with a message), within the time limit, and
without a sanitizer report. MIR text that cesium0 accepts and prints must
also parse again and print the same.

Run it on a cesium0 built with sanitizers: tools/portable-c.sh fuzz <compiler>
does both. The inputs depend only on --seed and the iteration number, so a
failure can be reproduced; every failing input is kept in the output directory
next to a .txt file with the command and what went wrong.

usage: python tools/fuzz-stage0.py <cesium0> [--iterations N] [--seed S]
                                   [--out DIR] [--timeout SECONDS] [--jobs N]

Needs only Python 3 and its standard library.
"""

import argparse
import concurrent.futures
import glob
import os
import random
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
B = os.path.join(ROOT, "bootstrap")

STAGE1 = ["util.csm", "lex.csm", "parse.csm", "types.csm", "lower.csm", "decl.csm", "main.csm"]

TOKEN = re.compile(rb"[A-Za-z_][A-Za-z0-9_]*|[0-9]+|\s+|.", re.S)

# values at the edges of the integer types, and text that readers tend to
# mishandle
INTERESTING = [
  b"0", b"1", b"-1", b"127", b"128", b"255", b"256", b"32767", b"32768", b"65535", b"65536",
  b"2147483647", b"2147483648", b"4294967295", b"4294967296",
  b"9223372036854775807", b"9223372036854775808", b"18446744073709551615", b"18446744073709551616",
  b"99999999999999999999999999999999999999", b"-9223372036854775808", b"-9223372036854775809",
  b"0x", b"0x10", b"00", b"1e9", b"1.5",
  b"\"", b"'", b"\\", b"\"\\", b"\"\\x", b"\"\\xZZ\"", b"\"\\0\"", b"'\\''", b"''", b"/*", b"*/", b"//",
  b"\x00", b"\xff", b"\xef\xbb\xbf", b"\r", b"\r\n", b"\t", b"\x7f", b"\xc3\xa9",
  b"(", b")", b"{", b"}", b"[", b"]", b"#", b"##", b"@", b"$", b"%", b":", b";", b",", b".", b"=", b"->",
  b"A" * 300, b"a" * 5000,
]


def read(path):
  with open(path, "rb") as f:
    return f.read()


class Seed:
  """One corpus file and how cesium0 is run on a mutant of it."""

  def __init__(self, path, kind, others_before=(), others_after=()):
    self.path = path
    self.kind = kind  # "mir" or "s0"
    self.data = read(path)
    self.before = list(others_before)
    self.after = list(others_after)

  def command(self, cesium0, emit, out, mutant):
    if self.kind == "mir":
      return [cesium0, emit, "-o", out, "--mir", mutant]
    return [cesium0, emit, "-o", out] + self.before + [mutant] + self.after


def corpus():
  seeds = []
  for path in sorted(glob.glob(os.path.join(B, "tests", "mir", "*.mir")) +
                     glob.glob(os.path.join(B, "tests", "mir", "errors", "*.mir")) +
                     glob.glob(os.path.join(B, "tests", "s0", "*.mir"))):
    seeds.append(Seed(path, "mir"))
  lib = os.path.join(B, "tests", "s0", "lib.csm")
  for path in sorted(glob.glob(os.path.join(B, "tests", "s0", "*.csm")) +
                     glob.glob(os.path.join(B, "tests", "s0", "errors", "*.csm"))):
    name = os.path.basename(path)
    alone = name in ("tokens.csm", "lib.csm")
    seeds.append(Seed(path, "s0", others_after=[] if alone else [lib]))
  # cesium1's source: each file in turn is the damaged one
  stage1 = [os.path.join(B, "stage1", name) for name in STAGE1]
  for i, path in enumerate(stage1):
    seeds.append(Seed(path, "s0", others_before=stage1[:i], others_after=stage1[i + 1:]))
  if not seeds:
    sys.exit("no corpus found under bootstrap/")
  return seeds


def dictionary(seeds):
  """Every distinct token of the corpus, per kind, plus the interesting ones."""
  words = {"mir": set(), "s0": set()}
  for seed in seeds:
    for token in TOKEN.findall(seed.data):
      if not token.isspace():
        words[seed.kind].add(token)
  return {kind: sorted(found) + INTERESTING for kind, found in words.items()}


def mutate(rng, data, words):
  """Return `data` with one to four small changes."""
  for _ in range(rng.choice([1, 1, 1, 2, 2, 3, 4])):
    how = rng.randrange(12)
    if how <= 4:
      # token level: what a parser sees
      tokens = TOKEN.findall(data)
      if not tokens:
        data = rng.choice(words)
        continue
      i = rng.randrange(len(tokens))
      if how == 0:
        tokens[i] = rng.choice(words)
      elif how == 1:
        del tokens[i]
      elif how == 2:
        tokens.insert(i, rng.choice(words))
      elif how == 3:
        j = rng.randrange(len(tokens))
        tokens[i], tokens[j] = tokens[j], tokens[i]
      else:
        tokens.insert(i, tokens[i])
      data = b"".join(tokens)
    elif how <= 7:
      # line level
      lines = data.split(b"\n")
      i = rng.randrange(len(lines))
      if how == 5:
        del lines[i]
      elif how == 6:
        lines.insert(i, lines[rng.randrange(len(lines))])
      else:
        j = rng.randrange(len(lines))
        lines[i], lines[j] = lines[j], lines[i]
      data = b"\n".join(lines)
    elif how == 8:
      # byte level: what a lexer sees
      if data:
        i = rng.randrange(len(data))
        data = data[:i] + bytes([rng.randrange(256)]) + data[i + rng.randrange(2):]
    elif how == 9:
      # cut the file short, usually in the middle of a construct
      data = data[:rng.randrange(len(data) + 1)]
    elif how == 10:
      # the same piece many times over: long lists, long files
      if data:
        i = rng.randrange(len(data))
        j = min(len(data), i + rng.randrange(1, 40))
        data = data[:i] + data[i:j] * rng.choice([2, 16, 300]) + data[j:]
    else:
      # deep nesting
      opener, closer = rng.choice([(b"(", b")"), (b"{", b"}"), (b"[", b"]"), (b"#", b""), (b"-", b""), (b"!", b"")])
      depth = rng.choice([8, 100, 3000])
      i = rng.randrange(len(data) + 1)
      j = rng.randrange(i, min(len(data), i + 30) + 1)
      data = data[:i] + opener * depth + data[i:j] + closer * depth + data[j:]
  return data


def reported(stderr):
  return b"runtime error:" in stderr or b"Sanitizer" in stderr


def run(command, timeout):
  """Return (problem, stderr, exit status): problem is None when cesium0
  behaved."""
  try:
    done = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, timeout=timeout)
  except subprocess.TimeoutExpired:
    return f"no result within {timeout} s", b"", None
  if reported(done.stderr):
    return "sanitizer report", done.stderr, done.returncode
  if done.returncode not in (0, 1):
    return f"exit status {done.returncode} (expected 0 or 1)", done.stderr, done.returncode
  if done.returncode == 1 and not done.stderr.strip():
    return "exit status 1 without a message", done.stderr, done.returncode
  return None, done.stderr, done.returncode


def one(args, seeds, words, number):
  """Run iteration `number`. Return None, or a description of the failure."""
  rng = random.Random(args.seed * 1000003 + number)
  seed = seeds[rng.randrange(len(seeds))]
  data = mutate(rng, seed.data, words[seed.kind])
  work = os.path.join(args.out, "work", str(number % (args.jobs * 4)))
  os.makedirs(work, exist_ok=True)
  ext = ".mir" if seed.kind == "mir" else ".csm"
  mutant = os.path.join(work, "input" + ext)
  with open(mutant, "wb") as f:
    f.write(data)

  def fail(problem, command, stderr):
    kept = os.path.join(args.out, f"fail-{args.seed}-{number}{ext}")
    shutil.copyfile(mutant, kept)
    shown = " ".join(kept if a == mutant else a for a in command)
    with open(kept + ".txt", "w", encoding="utf-8") as f:
      f.write(f"{problem}\nfrom {os.path.relpath(seed.path, ROOT)}, seed {args.seed}, iteration {number}\n{shown}\n\n")
      f.write(stderr.decode("utf-8", "replace"))
    return f"iteration {number}: {problem}: {kept}"

  command = seed.command(args.cesium0, "--emit-qbe", os.path.join(work, "out.ssa"), mutant)
  problem, stderr, _ = run(command, args.timeout)
  if problem:
    return fail(problem, command, stderr)
  if seed.kind != "mir":
    return None

  # MIR text that is accepted and printed must read back the same
  first = os.path.join(work, "first.mir")
  command = seed.command(args.cesium0, "--emit-mir", first, mutant)
  problem, stderr, status = run(command, args.timeout)
  if problem:
    return fail(problem, command, stderr)
  if status != 0:
    return None  # rejected
  second = os.path.join(work, "second.mir")
  again = [args.cesium0, "--emit-mir", "-o", second, "--mir", first]
  done = subprocess.run(again, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  if done.returncode != 0:
    return fail("the MIR text cesium0 printed for this input does not parse again", command, done.stderr)
  if read(first) != read(second):
    return fail("the MIR text cesium0 printed for this input prints differently the second time", command, b"")
  return None


def main():
  parser = argparse.ArgumentParser(description="Mutation fuzzing of cesium0's MIR text parser and S0 front end.")
  parser.add_argument("cesium0")
  parser.add_argument("--iterations", type=int, default=2000)
  parser.add_argument("--seed", type=int, default=1)
  parser.add_argument("--out", default=os.path.join(ROOT, "tmp", "fuzz"))
  parser.add_argument("--timeout", type=float, default=20)
  parser.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
  args = parser.parse_args()
  args.cesium0 = os.path.abspath(args.cesium0)
  args.out = os.path.abspath(args.out)
  shutil.rmtree(args.out, ignore_errors=True)
  os.makedirs(args.out)

  seeds = corpus()
  words = dictionary(seeds)
  failures = []
  # numbers are handed out in blocks, so that no two running iterations
  # share a work directory
  with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
    for start in range(0, args.iterations, args.jobs):
      block = range(start, min(start + args.jobs, args.iterations))
      for result in pool.map(lambda n: one(args, seeds, words, n), block):
        if result:
          failures.append(result)
          print(result, flush=True)
      if len(failures) >= 20:
        print("stopping after 20 failures")
        break
  shutil.rmtree(os.path.join(args.out, "work"), ignore_errors=True)
  if failures:
    sys.exit(f"{len(failures)} failing input(s), kept in {args.out}")
  print(f"fuzz: {args.iterations} mutated inputs from {len(seeds)} corpus files (seed {args.seed}), no failures")


if __name__ == "__main__":
  main()
