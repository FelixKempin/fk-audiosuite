#!/usr/bin/env python3
"""
Writes the release manifest of one package: <file>.manifest.json next to it,
format fk-release-manifest/1. Used by the packaging scripts of the DAW and of
other FK products; whoever publishes reads it (hash, size, platform, build).

    write-manifest.py --out DIR --file NAME --product KEY --version V --track beta|stable|alpha
                      --os linux|windows|macos --arch x86_64|arm64|universal --package KIND
                      [--family KEY] [--plugin-format vst3] [--repo OWNER/NAME] [--commit SHA]
                      [--input OWNER/NAME=SHA ...] [--signature none|signed|notarized]
                      [--signature-method NAME] [--os-min VERSION] [--cpu TEXT] [--host TEXT ...]

SHA-256 and size come from the file itself; the CI run from GITHUB_* variables
when set. Only the standard library.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def _git_commit(directory: Path) -> str:
    try:
        return subprocess.run(
            ["git", "-C", str(directory), "rev-parse", "HEAD"],
            capture_output=True, text=True, check=True,
        ).stdout.strip()  # fmt: skip
    except (OSError, subprocess.CalledProcessError):
        marker = directory / ".fk-git-sha"
        return marker.read_text().strip() if marker.is_file() else ""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    for name in ("out", "file", "product", "version", "track", "os", "arch", "package"):
        parser.add_argument(f"--{name}", required=True)
    parser.add_argument("--family", default="")
    parser.add_argument("--plugin-format", default="")
    parser.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY", ""))
    parser.add_argument("--commit", default="")
    parser.add_argument("--input", action="append", default=[])
    parser.add_argument("--signature", default="none", choices=["none", "signed", "notarized"])
    parser.add_argument("--signature-method", default="")
    parser.add_argument("--os-min", default="")
    parser.add_argument("--cpu", default="")
    parser.add_argument("--host", action="append", default=[])
    args = parser.parse_args()

    path = Path(args.out) / args.file
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    commit = args.commit or _git_commit(Path.cwd())
    inputs = []
    for item in args.input:
        repo, _, sha = item.partition("=")
        inputs.append({"repo": repo, "commit": sha})
    ci = {}
    if os.environ.get("GITHUB_RUN_ID"):
        server = os.environ.get("GITHUB_SERVER_URL", "https://github.com")
        ci = {
            "provider": "github",
            "run_id": os.environ["GITHUB_RUN_ID"],
            "run_url": f"{server}/{os.environ.get('GITHUB_REPOSITORY', '')}/actions/runs/"
            f"{os.environ['GITHUB_RUN_ID']}",
        }
    signature = {"state": args.signature}
    if args.signature_method:
        signature["method"] = args.signature_method
    requirements = {
        key: value
        for key, value in (("os_min_version", args.os_min), ("cpu", args.cpu), ("hosts", args.host))
        if value
    }
    manifest = {
        "manifest": "fk-release-manifest/1",
        "product": {"key": args.product, **({"family": args.family} if args.family else {})},
        "release": {
            "version": args.version,
            "track": args.track,
            "built_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "source": {
                **({"repo": args.repo} if args.repo else {}),
                **({"commit": commit} if commit else {}),
                **({"tag": os.environ["GITHUB_REF_NAME"]}
                   if os.environ.get("GITHUB_REF_TYPE") == "tag" else {}),
            },  # fmt: skip
            **({"inputs": inputs} if inputs else {}),
            **({"ci": ci} if ci else {}),
        },
        "artifact": {
            "os": args.os,
            "arch": args.arch,
            "package": args.package,
            "plugin_format": args.plugin_format,
            "filename": args.file,
            "sha256": digest.hexdigest(),
            "size_bytes": path.stat().st_size,
            "access": "public",
            "signature": signature,
            **({"requirements": requirements} if requirements else {}),
        },
    }
    target = path.with_name(path.name + ".manifest.json")
    target.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
