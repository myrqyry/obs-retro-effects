#!/usr/bin/env python3
"""Reject private/generated paths from the CPack source tarball."""
import sys
import tarfile
from pathlib import PurePosixPath

BLOCKED = (
    (".meristem", "local"),
    (".deps",),
    ("build",),
    ("build-test",),
    ("build_test",),
    ("tests", "build"),
    ("install",),
    (".cache",),
    ("memory",),
    (".grepai",),
)

def verify(archive):
    with tarfile.open(archive, "r:xz") as bundle:
        entries = [(tuple(PurePosixPath(m.name).parts[1:]), m.isdir()) for m in bundle.getmembers()]
    if not entries:
        raise ValueError("Empty source archive")
    members = [parts for parts, _ in entries]
    for parts, is_dir in entries:
        for banned in BLOCKED:
            if parts[:len(banned)] == banned and (len(parts) > len(banned) or not is_dir):
                raise ValueError("Private/generated path in source archive: " + "/".join(parts))
        if parts and (
            parts[-1] in {"mcp.db", "SESSION-STATE.md", "SOUL.md", "USER.md", "MEMORY.md", "HEARTBEAT.md"}
            or parts[-1].startswith(".env")
            or parts[-1].endswith((".pem", ".key"))
        ):
            raise ValueError("Private file in source archive: " + "/".join(parts))
    if ("buildspec.json",) not in members:
        raise ValueError("Missing buildspec.json from source archive")
    print(f"PASS: verified {len(members)} source-archive members; no private/generated entries")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: check_source_archive.py <archive.tar.xz>")
    try:
        verify(sys.argv[1])
    except (ValueError, tarfile.TarError) as ex:
        raise SystemExit(f"FAIL: {ex}")
