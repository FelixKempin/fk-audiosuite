#!/usr/bin/env python3
"""Das Benutzerhandbuch (docs/manual) gegen den Code abgleichen und seine Bilder erzeugen.

    tools/manual.py check            veraltete Kapitel, fehlende und verwaiste Bilder
    tools/manual.py coverage         UI-Quellen, die noch kein Kapitel beschreibt
    tools/manual.py shots [--build]  Bilder neu erzeugen (Szenen in tests/manual, Tag [manual])
    tools/manual.py hook             für den Stop-Hook von Claude Code

Ein Kapitel ist veraltet, wenn eine Datei aus seinem `sources:` nach dem letzten Commit des
Kapitels geändert wurde (oder gerade uncommittet ist, während das Kapitel unverändert ist).
Regeln: docs/manual/MAINTAINING.md.
"""

from __future__ import annotations

import fnmatch
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANUAL = ROOT / "docs" / "manual"
IMAGES = MANUAL / "images"
SCENES = ROOT / "tests" / "manual"
UI_SOURCES = "src/ui/fk/ui/*.cpp"
DEFAULT_BUILD = ROOT / "build" / "linux-release"


@dataclass
class Chapter:
    path: Path
    title: str = ""
    sources: list[str] = field(default_factory=list)
    images: list[str] = field(default_factory=list)

    @property
    def rel(self) -> str:
        return str(self.path.relative_to(ROOT))


def git(*args: str) -> list[str]:
    result = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True)
    return [line for line in result.stdout.splitlines() if line.strip()]


def read_front_matter(path: Path) -> dict[str, object] | None:
    """Einfaches YAML: `key: wert` und Listen aus `  - eintrag`. Ohne Kopf kein Kapitel."""
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0].strip() != "---":
        return None
    data: dict[str, object] = {}
    key = None
    for line in lines[1:]:
        if line.strip() == "---":
            return data
        if line.lstrip().startswith("- ") and key is not None:
            value = line.split("#", 1)[0].strip()[2:].strip()
            data.setdefault(key, [])
            if value:
                data[key].append(value)  # type: ignore[union-attr]
        elif ":" in line:
            key, value = (part.strip() for part in line.split(":", 1))
            data[key] = value if value else []
    return None


def load_chapters() -> list[Chapter]:
    chapters = []
    for path in sorted(MANUAL.glob("*.md")):
        meta = read_front_matter(path)
        if meta is None:
            continue
        chapters.append(Chapter(path, str(meta.get("title", path.stem)),
                                list(meta.get("sources", [])), list(meta.get("images", []))))  # type: ignore[arg-type]
    return chapters


def matches(file: str, patterns: list[str]) -> bool:
    return any(fnmatch.fnmatch(file, pattern) for pattern in patterns)


def uncommitted() -> set[str]:
    files = set()
    for line in git("status", "--porcelain", "--untracked-files=all"):
        files.add(line[3:].split(" -> ")[-1])
    return files


def unpushed() -> set[str]:
    if git("rev-parse", "--abbrev-ref", "@{u}"):
        return set(git("diff", "--name-only", "@{u}..HEAD"))
    return set(git("log", "--since=12 hours ago", "--name-only", "--format="))


def stale_sources(chapter: Chapter, dirty: set[str]) -> list[str]:
    """Quelldateien, die neuer sind als der letzte Stand des Kapitels."""
    if chapter.rel in dirty:
        return []  # wird gerade bearbeitet
    last = git("log", "-1", "--format=%H", "--", chapter.rel)
    changed = set(git("diff", "--name-only", f"{last[0]}..HEAD")) if last else set()
    changed |= dirty
    return sorted(file for file in changed if matches(file, chapter.sources))


def referenced_images(chapter: Chapter) -> list[str]:
    text = chapter.path.read_text(encoding="utf-8")
    return re.findall(r"!\[[^\]]*\]\(images/([^)]+)\.png\)", text)


def scene_names() -> set[str]:
    names = set()
    for file in SCENES.glob("*.cpp"):
        names |= set(re.findall(r'saveShot\s*\([^"]*"([^"]+)"', file.read_text(encoding="utf-8"), re.S))
    return names


def cmd_check() -> int:
    chapters = load_chapters()
    dirty = uncommitted()
    scenes = scene_names()
    problems = 0
    all_refs: set[str] = set()

    for chapter in chapters:
        refs = referenced_images(chapter)
        all_refs |= set(refs)
        notes = []
        if stale := stale_sources(chapter, dirty):
            notes.append("veraltet, geändert seit dem letzten Stand: " + ", ".join(stale))
        for ref in refs:
            if not (IMAGES / f"{ref}.png").exists():
                notes.append(f"Bild fehlt: images/{ref}.png (tools/manual.py shots)")
            if ref not in scenes:
                notes.append(f"keine Szene erzeugt images/{ref}.png (tests/manual)")
            if ref not in chapter.images:
                notes.append(f"images:-Liste im Kopf nennt {ref} nicht")
        for listed in chapter.images:
            if listed not in refs:
                notes.append(f"images:-Liste nennt {listed}, der Text zeigt es nicht")
        if not chapter.sources:
            notes.append("sources: ist leer")
        problems += len(notes)
        print(f"{'OK  ' if not notes else '!!  '}{chapter.rel}  ({chapter.title})")
        for note in notes:
            print(f"      - {note}")

    for image in sorted(IMAGES.rglob("*.png")):
        name = str(image.relative_to(IMAGES).with_suffix(""))
        if name not in all_refs:
            problems += 1
            print(f"!!  verwaistes Bild: images/{name}.png (kein Kapitel zeigt es)")

    print(f"\n{len(chapters)} Kapitel, {problems} offene Punkte")
    return 1 if problems else 0


def cmd_coverage() -> int:
    patterns = [pattern for chapter in load_chapters() for pattern in chapter.sources]
    missing = sorted(str(path.relative_to(ROOT)) for path in ROOT.glob(UI_SOURCES)
                     if not matches(str(path.relative_to(ROOT)), patterns))
    total = len(list(ROOT.glob(UI_SOURCES)))
    for file in missing:
        print(file)
    print(f"\n{total - len(missing)} von {total} UI-Quellen sind einem Kapitel zugeordnet")
    return 0


def cmd_shots(args: list[str]) -> int:
    build = Path(os.environ.get("FK_BUILD_DIR", DEFAULT_BUILD))
    if "--build" in args:
        # -j3: ein voller Parallel-Build löst auf dem Laptop systemd-oomd aus.
        if subprocess.run(["cmake", "--build", str(build), "--target", "fk_tests", "-j3"]).returncode:
            return 1
    binary = build / "tests" / "fk_tests"
    if not binary.exists():
        print(f"{binary} fehlt: erst bauen (tools/manual.py shots --build)", file=sys.stderr)
        return 1
    env = dict(os.environ, FK_MANUAL_IMAGE_DIR=str(IMAGES))
    return subprocess.run([str(binary), "[manual]"], env=env, cwd=ROOT).returncode


def cmd_hook() -> int:
    try:
        payload = json.load(sys.stdin)
    except (json.JSONDecodeError, ValueError):
        payload = {}
    if payload.get("stop_hook_active"):
        return 0  # schon einmal zurückgeschickt: keine Schleife
    dirty = uncommitted()
    recent = dirty | unpushed()
    hits = []
    for chapter in load_chapters():
        stale = [file for file in stale_sources(chapter, dirty) if file in recent]
        if stale:
            hits.append(f"- {chapter.rel} ({chapter.title}): {', '.join(stale)}")
    if not hits:
        return 0
    reason = (
        "Benutzerhandbuch: Deine Änderungen betreffen diese Kapitel. Prüfe, ob sich Bedienung, "
        "Texte, Tooltips, Menüs oder Abläufe geändert haben; dann Kapiteltext und Szene in "
        "tests/manual anpassen und Bilder neu erzeugen (tools/manual.py shots, Build vorher "
        "erfragen). Ist nichts zu ändern, `reviewed:` im Kopf des Kapitels auf heute setzen. "
        "Regeln: docs/manual/MAINTAINING.md.\n\n" + "\n".join(hits)
    )
    print(json.dumps({"decision": "block", "reason": reason}))
    return 0


def main() -> int:
    command = sys.argv[1] if len(sys.argv) > 1 else "check"
    if command == "check":
        return cmd_check()
    if command == "coverage":
        return cmd_coverage()
    if command == "shots":
        return cmd_shots(sys.argv[2:])
    if command == "hook":
        return cmd_hook()
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
