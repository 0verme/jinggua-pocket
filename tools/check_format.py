"""Small dependency-free repository format gate for CI."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_PARTS = {".git", ".pio", "build", "cmake-build-debug", "cmake-build-release"}
EXTENSIONS = {".cpp", ".h", ".hpp", ".ini", ".md", ".py", ".yml", ".yaml"}


def should_check(path: Path) -> bool:
    return path.suffix in EXTENSIONS and not any(
        part in SKIP_PARTS for part in path.relative_to(ROOT).parts
    )


def main() -> int:
    failures: list[str] = []
    for path in sorted(
        path for path in ROOT.rglob("*") if path.is_file() and should_check(path)
    ):
        data = path.read_bytes()
        relative = path.relative_to(ROOT)
        if b"\r\n" in data:
            failures.append(f"{relative}: CRLF is not allowed")
        if data and not data.endswith(b"\n"):
            failures.append(f"{relative}: missing final newline")
        for line_number, line in enumerate(data.splitlines(), start=1):
            if line.rstrip(b" \t") != line:
                failures.append(f"{relative}:{line_number}: trailing whitespace")

    if failures:
        print("format/lint failures:")
        print("\n".join(failures))
        return 1
    print("format/lint checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
