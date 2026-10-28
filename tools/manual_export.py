"""Benutzerhandbuch als eine HTML-Datei (Bilder eingebettet) und als PDF.

Läuft in der venv aus `tools/manual.py export` (markdown, weasyprint), nicht direkt aufrufen:

    manual_export.py <docs/manual> <ausgabe> <html,pdf> <kapitel.md> ...
"""

from __future__ import annotations

import base64
import datetime
import html
import re
import sys
from pathlib import Path

import markdown

ROOT = Path(__file__).resolve().parents[1]
BASENAME = "fk-audiosuite-manual"

CSS = """
:root { --text: #1d1f23; --muted: #5b6070; --line: #d5d8de; --soft: #f3f4f7; --accent: #2f6fdb; --mark: #f5a623; }
* { box-sizing: border-box; }
body { font: 15px/1.6 "Inter", "Segoe UI", system-ui, sans-serif; color: var(--text); background: #fff;
       max-width: 980px; margin: 0 auto; padding: 24px 16px 80px; }
h1 { font-size: 30px; margin: 64px 0 8px; padding-top: 16px; border-top: 3px solid var(--text); }
h2 { font-size: 21px; margin: 40px 0 8px; }
h3 { font-size: 17px; margin: 28px 0 6px; }
a { color: var(--accent); text-decoration: none; }
img { max-width: 100%; height: auto; border: 1px solid var(--line); border-radius: 4px; margin: 8px 0; }
table { border-collapse: collapse; margin: 12px 0; width: 100%; font-size: 14px; }
th, td { border: 1px solid var(--line); padding: 5px 8px; vertical-align: top; text-align: left; }
th { background: var(--soft); }
code { background: var(--soft); padding: 1px 4px; border-radius: 3px; font-size: 13px; }
blockquote { margin: 14px 0; padding: 6px 14px; border-left: 4px solid var(--mark); background: #fff8ec; }
blockquote p { margin: 4px 0; }
hr { border: 0; border-top: 1px solid var(--line); margin: 28px 0; }
.cover { text-align: center; padding: 120px 0 60px; }
.cover h1 { border: 0; font-size: 40px; margin: 0; }
.cover p { color: var(--muted); }
nav.contents ol { padding-left: 22px; }
nav.contents li { margin: 3px 0; }
nav.contents ol ol { font-size: 14px; color: var(--muted); list-style: none; padding-left: 14px; }
@page { size: A4; margin: 18mm 16mm 20mm;
        @bottom-center { content: counter(page); font: 10px sans-serif; color: #888; } }
@media print {
  body { max-width: none; padding: 0; font-size: 11pt; }
  .cover { page-break-after: always; padding-top: 200px; }
  nav.contents { page-break-after: always; }
  section.chapter > h1 { page-break-before: always; margin-top: 0; border-top: 0; }
  h2, h3 { page-break-after: avoid; }
  img, table tr, blockquote { page-break-inside: avoid; }
  a { color: inherit; }
}
"""


def strip_front_matter(text: str) -> str:
    return re.sub(r"\A---\n.*?\n---\n", "", text, flags=re.S)


def version() -> str:
    match = re.search(r"project\(FKAudioSuite\s+VERSION\s+([\d.]+)", (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"))
    return match.group(1) if match else "?"


def embed_images(body: str, manual: Path) -> str:
    def replace(match: re.Match[str]) -> str:
        data = base64.b64encode((manual / match.group(1)).read_bytes()).decode("ascii")
        return f'src="data:image/png;base64,{data}"'

    return re.sub(r'src="(images/[^"]+\.png)"', replace, body)


def scope_anchors(body: str, slug: str) -> str:
    """Anker je Kapitel eindeutig machen: id="x" -> id="slug--x", Links innerhalb und zwischen Kapiteln."""
    body = re.sub(r'id="([^"]+)"', lambda m: f'id="{slug}--{m.group(1)}"', body)
    body = re.sub(r'href="#([^"]+)"', lambda m: f'href="#{slug}--{m.group(1)}"', body)
    body = re.sub(r'href="([\w-]+)\.md#([^"]+)"', lambda m: f'href="#{m.group(1)}--{m.group(2)}"', body)
    return re.sub(r'href="([\w-]+)\.md"', lambda m: f'href="#{m.group(1)}"', body)


def render_chapter(path: Path, manual: Path) -> tuple[str, str, list[tuple[str, str]]]:
    """HTML, Titel und (Anker, Überschrift) der Abschnitte eines Kapitels."""
    text = re.sub(r"<!--.*?-->\n?", "", strip_front_matter(path.read_text(encoding="utf-8")), flags=re.S)
    converter = markdown.Markdown(extensions=["tables", "toc", "fenced_code"])
    body = converter.convert(text)
    slug = path.stem
    title = converter.toc_tokens[0]["name"] if converter.toc_tokens else slug
    sections = [(f"{slug}--{token['id']}", token["name"])
                for top in converter.toc_tokens for token in top.get("children", [])]
    body = embed_images(scope_anchors(body, slug), manual)
    body = re.sub(r"<h1[^>]*>", f'<h1 id="{slug}">', body, count=1)
    return f'<section class="chapter">{body}</section>', title, sections


def conventions(manual: Path) -> str:
    """Abschnitt "Conventions" aus README.md."""
    readme = (manual / "README.md").read_text(encoding="utf-8")
    match = re.search(r"## Conventions\n(.*?)\n## ", readme, flags=re.S)
    return markdown.markdown("## Conventions\n" + match.group(1)) if match else ""


def build_html(manual: Path, chapters: list[Path]) -> str:
    rendered = [render_chapter(path, manual) for path in chapters]
    contents = "".join(
        f'<li><a href="#{path.stem}">{html.escape(title)}</a><ol>'
        + "".join(f'<li><a href="#{anchor}">{name}</a></li>' for anchor, name in sections)
        + "</ol></li>"
        for path, (_, title, sections) in zip(chapters, rendered))
    today = datetime.date.today().isoformat()
    return (
        "<!doctype html><html lang=en><head><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width, initial-scale=1'>"
        f"<title>FK Audio Suite Manual</title><style>{CSS}</style></head><body>"
        f'<div class="cover"><h1>FK Audio Suite</h1><p>User Manual · version {version()} · {today}</p></div>'
        f'<nav class="contents"><h2>Contents</h2><ol>{contents}</ol>{conventions(manual)}</nav>'
        + "".join(body for body, _, _ in rendered)
        + "</body></html>")


def main() -> int:
    manual, out, formats, chapters = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3].split(","), [Path(p) for p in sys.argv[4:]]
    page = build_html(manual, chapters)

    if "html" in formats:
        (out / f"{BASENAME}.html").write_text(page, encoding="utf-8")
        print(f"HTML: {out / (BASENAME + '.html')}")

    if "pdf" in formats:
        from weasyprint import HTML

        HTML(string=page, base_url=str(manual)).write_pdf(out / f"{BASENAME}.pdf")
        print(f"PDF:  {out / (BASENAME + '.pdf')}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
