"""Check common relative Markdown and HTML image targets, not remote URLs or anchors."""
import pathlib
import re
import sys
from urllib.parse import unquote, urlsplit

MARKDOWN = re.compile(r"\]\((?:<([^>\n]+)>|([^)\s]+))(?:\s+[^)\n]*)?\)")
IMAGE = re.compile(r"""<img\b[^>]*\bsrc\s*=\s*["']([^"']+)["']""", re.I)
FENCE = re.compile(r"^\s{0,3}(\x60{3,}|~{3,})")


def missing_targets(readme):
    fence = None
    missing = set()
    for line in readme.read_text(encoding="utf-8").splitlines():
        marker = FENCE.match(line)
        if marker:
            token = marker.group(1)
            if fence is None:
                fence = token
            elif token[0] == fence[0] and len(token) >= len(fence):
                fence = None
            continue
        if fence:
            continue
        targets = [angle or bare for angle, bare in MARKDOWN.findall(line)]
        targets += IMAGE.findall(line)
        for target in targets:
            url = urlsplit(target)
            if url.scheme or url.netloc or not url.path or url.path.startswith("/"):
                continue
            path = unquote(url.path)
            if not (readme.parent / path).exists():
                missing.add(path)
    return sorted(missing)


if __name__ == "__main__":
    readme = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "README.md")
    missing = missing_targets(readme)
    for target in missing:
        print(f"{readme}: missing relative target {target}")
    sys.exit(bool(missing))
