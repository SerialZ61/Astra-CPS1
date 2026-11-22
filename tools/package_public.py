"""Build a public forum ZIP from a committed source tree and its 3DSX."""
import argparse
import hashlib
import io
import subprocess
import tarfile
from pathlib import Path, PurePosixPath
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[1]
APP = "3ds/AstraCPS1/"
SOURCE_TYPES = {".c", ".cpp", ".h", ".s", ".inc", ".mk", ".pica", ".py",
                ".ps1", ".md", ".txt", ".json", ".csv", ".log"}
SPECIAL = {"Makefile", ".gitattributes", ".gitignore", "assets/icon.png"}
FROZEN = "eea2c3f7c971c60e6fea763245d6ff17f85fd7f0c32e5c52fdf19b62f55ea31b"


def package(ref, binary, output):
    program = binary.read_bytes()
    if program[:4] != b"3DSX":
        raise ValueError("Expected a 3DSX executable")
    if ref == "v0.6a" and hashlib.sha256(program).hexdigest() != FROZEN:
        raise ValueError("Reference v0.6a fingerprint mismatch")
    data = subprocess.check_output(["git", "archive", "--format=tar", ref], cwd=ROOT)
    files = {}
    with tarfile.open(fileobj=io.BytesIO(data)) as source:
        for member in source.getmembers():
            if member.isdir():
                continue
            path = PurePosixPath(member.name)
            allowed = member.name in SPECIAL or path.suffix.lower() in SOURCE_TYPES
            if not member.isfile() or not allowed or path.is_absolute() or ".." in path.parts:
                raise ValueError("Unexpected source file: " + member.name)
            files[member.name] = source.extractfile(member).read()
    files[APP + "AstraCPS1.3dsx"] = program
    for name, contents in list(files.items()):
        if name.startswith("notices/"):
            files[APP + name] = contents
    files["SHA256SUMS.txt"] = "".join(
        hashlib.sha256(files[n]).hexdigest() + "  " + n + "\n" for n in sorted(files)
    ).encode()
    output.parent.mkdir(parents=True, exist_ok=True)
    pending = output.with_suffix(".tmp")
    with ZipFile(pending, "w", ZIP_DEFLATED, compresslevel=9) as dest:
        for name in sorted(files):
            dest.writestr(name, files[name])
        dest.writestr(APP + "roms/", b"")
    with ZipFile(pending) as check:
        if check.testzip() or any(check.read(n) != b for n, b in files.items()):
            raise IOError("Archive verification failed")
    pending.replace(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ref", help="release tag or commit")
    parser.add_argument("binary", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    package(args.ref, args.binary, args.output)
