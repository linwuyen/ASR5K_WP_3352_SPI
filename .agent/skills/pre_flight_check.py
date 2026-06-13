import argparse
import re
import sys
from pathlib import Path


SOURCE_SUFFIXES = {".c", ".h"}
COMMENT_OR_LITERAL = re.compile(
    r"//.*?$|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'",
    re.MULTILINE | re.DOTALL,
)


def strip_comments_and_literals(text: str) -> str:
    return COMMENT_OR_LITERAL.sub(
        lambda match: "\n" * match.group(0).count("\n"),
        text,
    )


def find_sources(paths: list[str]) -> list[Path]:
    sources = []
    for raw_path in paths:
        path = Path(raw_path)
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES:
            sources.append(path)
        elif path.is_dir():
            sources.extend(
                candidate
                for candidate in path.rglob("*")
                if candidate.is_file()
                and candidate.suffix.lower() in SOURCE_SUFFIXES
            )
        else:
            raise FileNotFoundError(f"Source path not found or unsupported: {path}")
    return sorted(set(sources))


def line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def check_division(text: str) -> list[tuple[int, str]]:
    cleaned = strip_comments_and_literals(text)
    violations = []
    for match in re.finditer(r"(?<!/)/(?![/*])=?", cleaned):
        start = cleaned.rfind("\n", 0, match.start()) + 1
        end = cleaned.find("\n", match.start())
        if end < 0:
            end = len(cleaned)
        source_line = cleaned[start:end].strip()
        if source_line.startswith("#include"):
            continue
        violations.append(
            (line_number(cleaned, match.start()), "division operator is prohibited")
        )
    return violations


def check_safety(text: str) -> list[tuple[int, str]]:
    cleaned = strip_comments_and_literals(text)
    violations = []
    patterns = [
        (
            re.compile(r"\b(?:malloc|calloc|realloc|free)\s*\("),
            "dynamic memory allocation is prohibited",
        ),
        (
            re.compile(r"\b(?:int|char|long|short|uint8_t|int8_t)\b"),
            "native or 8-bit type is prohibited by the declared C2000 policy",
        ),
        (
            re.compile(
                r"while\s*\([^)]*(?:flag|status|ready)[^)]*\)\s*;",
                re.IGNORECASE,
            ),
            "blocking hardware-flag loop is prohibited",
        ),
    ]

    for pattern, message in patterns:
        for match in pattern.finditer(cleaned):
            violations.append((line_number(cleaned, match.start()), message))

    depth = 0
    for match in re.finditer(r"\b(EALLOW|EDIS)\b", cleaned):
        token = match.group(1)
        if token == "EALLOW":
            depth += 1
        elif depth == 0:
            violations.append(
                (line_number(cleaned, match.start()), "EDIS has no matching EALLOW")
            )
        else:
            depth -= 1

    if depth:
        violations.append((1, f"{depth} EALLOW block(s) have no matching EDIS"))

    return sorted(violations)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run executable C2000 source preflight checks."
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check-division", action="store_true")
    mode.add_argument("--safety-scan", action="store_true")
    mode.add_argument("--all", action="store_true")
    parser.add_argument("paths", nargs="+", help="C/H file or directory")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        sources = find_sources(args.paths)
    except FileNotFoundError as exc:
        print(f"[FAIL] {exc}")
        return 2

    if not sources:
        print("[FAIL] No C/H source files found.")
        return 2

    failed = False
    for source in sources:
        text = source.read_text(encoding="utf-8", errors="replace")
        violations = []
        if args.check_division or args.all:
            violations.extend(check_division(text))
        if args.safety_scan or args.all:
            violations.extend(check_safety(text))

        if violations:
            failed = True
            for number, message in sorted(violations):
                print(f"[FAIL] {source}:{number}: {message}")

    if failed:
        return 1

    print(f"[PASS] Preflight checks passed for {len(sources)} source file(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
