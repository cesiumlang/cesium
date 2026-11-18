#!/usr/bin/env python3
"""Download the Cesium devkit for this machine and unpack it to .devkit.

The devkit (https://github.com/cesiumlang/devkits) holds the pinned Zig, the
LLVM and LLD libraries and QBE. The version to use is the one line of the file
devkit-version in the repository root. The archive is checked against the
sha256 that the release's devkits.json gives for it.

usage: python tools/fetch-devkit.py [--host <triple>] [--dir <directory>]

Needs only Python 3 and its standard library.
"""

import argparse
import hashlib
import json
import os
import platform
import shutil
import sys
import tarfile
import urllib.request
import zipfile

REPO = "https://github.com/cesiumlang/devkits"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def host_triple():
  machine = platform.machine().lower()
  arch = {"x86_64": "x86_64", "amd64": "x86_64", "aarch64": "aarch64", "arm64": "aarch64"}.get(machine)
  system = {"Linux": "linux-musl", "Windows": "windows-gnu", "Darwin": "macos-none"}.get(platform.system())
  if arch is None or system is None:
    sys.exit(f"no devkit for {platform.system()} on {platform.machine()}; pass --host")
  return f"{arch}-{system}"


def download(url, path):
  print(f"downloading {url}")
  with urllib.request.urlopen(url) as response, open(path, "wb") as out:
    shutil.copyfileobj(response, out)


def sha256_of(path):
  digest = hashlib.sha256()
  with open(path, "rb") as f:
    for chunk in iter(lambda: f.read(1 << 20), b""):
      digest.update(chunk)
  return digest.hexdigest()


def main():
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("--host", default=host_triple(), help="the devkit's host triple (default: this machine)")
  parser.add_argument("--dir", default=os.path.join(ROOT, ".devkit"), help="where to unpack (default: .devkit)")
  args = parser.parse_args()

  with open(os.path.join(ROOT, "devkit-version"), encoding="utf-8") as f:
    version = f.read().strip()

  stamp = os.path.join(args.dir, "manifest.json")
  if os.path.exists(stamp):
    with open(stamp, encoding="utf-8") as f:
      have = json.load(f)
    if have.get("version") == version and have.get("host") == args.host:
      print(f"devkit {version} for {args.host} is already in {args.dir}")
      return

  base = f"{REPO}/releases/download/v{version}"
  with urllib.request.urlopen(f"{base}/devkits.json") as response:
    devkits = json.load(response)["devkits"]
  if args.host not in devkits:
    sys.exit(f"devkit {version} has no archive for {args.host}; it has: {', '.join(sorted(devkits))}")
  entry = devkits[args.host]

  downloads = os.path.join(ROOT, ".devkit-downloads")
  os.makedirs(downloads, exist_ok=True)
  archive = os.path.join(downloads, entry["asset"])
  if not os.path.exists(archive) or sha256_of(archive) != entry["sha256"]:
    download(entry["url"], archive)
  got = sha256_of(archive)
  if got != entry["sha256"]:
    sys.exit(f"{entry['asset']}: sha256 {got}, devkits.json says {entry['sha256']}")

  # the archive holds one top-level directory; its contents become --dir
  unpack = args.dir + ".unpack"
  shutil.rmtree(unpack, ignore_errors=True)
  shutil.rmtree(args.dir, ignore_errors=True)
  print(f"unpacking {entry['asset']}")
  if archive.endswith(".zip"):
    with zipfile.ZipFile(archive) as z:
      z.extractall(unpack)
  else:
    with tarfile.open(archive) as t:
      t.extractall(unpack)
  (top,) = os.listdir(unpack)
  os.replace(os.path.join(unpack, top), args.dir)
  os.rmdir(unpack)
  print(f"devkit {version} for {args.host}: {args.dir}")


if __name__ == "__main__":
  main()
