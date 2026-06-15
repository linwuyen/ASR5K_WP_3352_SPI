import argparse
import glob
import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlparse


AGENT_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = AGENT_ROOT.parent
REGISTRY = AGENT_ROOT / "DOCUMENT_STATUS_REGISTRY.md"

STATUS_ROW = re.compile(r"^\| `([^`]+)` \| ([A-Z_]+) \|")
MARKDOWN_LINK = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")


class CheckFailure(Exception):
    pass


def relative(path: Path) -> str:
    try:
        return str(path.relative_to(REPO_ROOT)).replace("\\", "/")
    except ValueError:
        return str(path)


def active_registry_files() -> list[Path]:
    active = set()
    for line in REGISTRY.read_text(encoding="utf-8").splitlines():
        match = STATUS_ROW.match(line)
        if not match or match.group(2) != "ACTIVE":
            continue

        pattern = match.group(1)
        if not pattern.startswith(".agent/"):
            continue

        absolute_pattern = str(REPO_ROOT / Path(pattern))
        matches = [Path(item) for item in glob.glob(absolute_pattern)]
        if not matches and not any(char in pattern for char in "*?["):
            raise CheckFailure(f"ACTIVE registry path does not exist: {pattern}")

        active.update(
            path.resolve()
            for path in matches
            if path.is_file() and path.suffix.lower() == ".md"
        )
    return sorted(active)


def check_python_syntax() -> None:
    files = sorted((AGENT_ROOT / "skills").glob("*.py"))
    files.extend(sorted((AGENT_ROOT / "ci").glob("*.py")))
    if not files:
        raise CheckFailure("No Python files found for syntax validation.")

    for path in files:
        source = path.read_text(encoding="utf-8")
        compile(source, str(path), "exec")

    print(f"[PASS] Python syntax: {len(files)} file(s)")


def check_yaml_parse() -> None:
    try:
        import yaml
    except ImportError as exc:
        raise CheckFailure(
            "PyYAML is unavailable. Install .agent/skills/requirements.txt."
        ) from exc

    files = sorted((AGENT_ROOT / "skills").glob("*.yaml"))
    files.extend(sorted((AGENT_ROOT / "workflows").glob("*.yaml")))
    if not files:
        raise CheckFailure("No YAML files found for parse validation.")

    for path in files:
        with path.open("r", encoding="utf-8") as stream:
            yaml.safe_load(stream)

    print(f"[PASS] YAML parse: {len(files)} file(s)")


def link_target(document: Path, href: str) -> Path | None:
    href = href.strip()
    if not href or href.startswith(("#", "http://", "https://", "mailto:")):
        return None

    if href.startswith("file://"):
        parsed = urlparse(href)
        raise CheckFailure(
            f"{relative(document)} contains non-portable file URI: {parsed.path}"
        )

    if href.startswith("<") and href.endswith(">"):
        href = href[1:-1]

    clean = unquote(href.split("#", 1)[0])
    if not clean:
        return None

    if clean.startswith(".agent/"):
        return (REPO_ROOT / clean).resolve()
    return (document.parent / clean).resolve()


def check_markdown_links(all_documents: bool) -> None:
    if all_documents:
        documents = sorted(AGENT_ROOT.rglob("*.md"))
        scope = "all Markdown"
    else:
        documents = active_registry_files()
        scope = "ACTIVE Markdown"

    failures = []
    for document in documents:
        text = document.read_text(encoding="utf-8")
        for match in MARKDOWN_LINK.finditer(text):
            href = match.group(1)
            try:
                target = link_target(document, href)
            except CheckFailure as exc:
                failures.append(str(exc))
                continue
            if target is not None and not target.exists():
                failures.append(
                    f"{relative(document)} -> {href} "
                    f"(missing {relative(target)})"
                )

    if failures:
        details = "\n".join(f"  - {failure}" for failure in failures)
        raise CheckFailure(
            f"Markdown stale links in {scope}: {len(failures)}\n{details}"
        )

    print(f"[PASS] Markdown stale links ({scope}): {len(documents)} file(s)")


def parse_markdown_table(file_path: Path) -> dict[str, str]:
    """
    Parse Markdown registry rows and return a map:
    {file_or_pattern: status}

    This parser is intended for DOCUMENT_STATUS_REGISTRY.md.
    It extracts the first column as file/pattern and the second column as status.
    """
    registry_map: dict[str, str] = {}

    if not file_path.exists():
        return registry_map

    for line in file_path.read_text(encoding="utf-8").splitlines():
        if not line.strip().startswith("|"):
            continue

        columns = [col.strip() for col in line.split("|")]
        if len(columns) < 4:
            continue

        file_pat = columns[1].replace("`", "").strip()
        status_val = columns[2].replace("`", "").strip()

        if file_pat in ["File", "File or Pattern", "Status"]:
            continue
        if file_pat.startswith("---") or status_val.startswith("---"):
            continue

        registry_map[file_pat] = status_val

    return registry_map


def check_frozen_architecture() -> None:
    decisions = (
        AGENT_ROOT / "00_Project/ASR5K_DECISIONS.md"
    ).read_text(encoding="utf-8")
    expected_decisions = {
        "FD-001": "Legacy Register Protocol is the production protocol.",
        "FD-002": "SPIB RX owns DMA CH3.",
        "FD-003": "SPIB TX owns DMA CH4.",
        "FD-004": "DMA CH5 is reserved.",
        "FD-005": "DMA CH6 is reserved.",
        "FD-006": "EMIF1 SDRAM is the Wave Runtime Source.",
        "FD-007": "CPU1 owns system control, parser, and download path.",
        "FD-008": "CPU2 owns DDS runtime.",
        "FD-009": "W25Q64 is for Boot, OTA, and Maintenance only.",
        "FD-010": "W25Q64 is not a DDS runtime source.",
        "FD-011": "D11 Packet Protocol is candidate only.",
    }
    for decision_id, statement in expected_decisions.items():
        if decision_id not in decisions or statement not in decisions:
            raise CheckFailure(
                f"Frozen decision missing or changed: {decision_id} {statement}"
            )

    forbidden = [
        re.compile(
            r"\bCPU1\s+(?:owns|is\s+the\s+owner\s+of)\s+"
            r"(?:the\s+)?DDS\s+runtime\b",
            re.IGNORECASE,
        ),
        re.compile(
            r"\bDMA\s+CH[56]\s+(?:is\s+)?(?:assigned|allocated)\b",
            re.IGNORECASE,
        ),
        re.compile(
            r"\bPacket\s+Protocol\s+is\s+(?:the\s+)?production\b",
            re.IGNORECASE,
        ),
        re.compile(
            r"\bW25Q64\s+is\s+(?:the\s+)?(?:DDS\s+)?runtime\s+"
            r"(?:waveform\s+)?source\b",
            re.IGNORECASE,
        ),
    ]

    for document in active_registry_files():
        text = document.read_text(encoding="utf-8")
        for pattern in forbidden:
            match = pattern.search(text)
            if match:
                line = text.count("\n", 0, match.start()) + 1
                raise CheckFailure(
                    f"Frozen architecture violation: "
                    f"{relative(document)}:{line}: {match.group(0)}"
                )

    specification = (
        AGENT_ROOT / "01_Architecture/SPEC_M5_WAVE_DOWNLOAD.md"
    ).read_text(encoding="utf-8")
    required_m5 = [
        "| `0` | `EMPTY` |",
        "| `1` | `DOWNLOADING` |",
        "| `2` | `DOWNLOAD_COMPLETE` |",
        "| `3` | `VALIDATING` |",
        "| `4` | `VALID` |",
        "| `5` | `INVALID` |",
        "| `6` | `LOCKED` |",
        "`0x095B` | `WAVE_BURST_BEGIN` | `4096`",
        "`PageState == 6` means `LOCKED`.",
    ]
    for statement in required_m5:
        if statement not in specification:
            raise CheckFailure(f"M5 governance assertion missing: {statement}")

    # Verify ACTIVE_EVIDENCE status definition exists in Document Status Registry raw text
    registry_text = REGISTRY.read_text(encoding="utf-8")
    if "| ACTIVE_EVIDENCE |" not in registry_text:
        raise CheckFailure("ACTIVE_EVIDENCE status definition missing from Document Status Registry.")

    # Parse registry table and verify exact status for M5R_PHASE2_BURST_TRANSPORT.md
    registry = parse_markdown_table(REGISTRY)
    m5r_path = ".agent/02_Milestones/M5R_PHASE2_BURST_TRANSPORT.md"
    if m5r_path not in registry:
        raise CheckFailure(f"{m5r_path} not found in Document Status Registry.")
    if registry[m5r_path] != "ACTIVE_EVIDENCE":
        raise CheckFailure(
            f"{m5r_path} status must be ACTIVE_EVIDENCE, got {registry[m5r_path]}"
        )

    # Elastic path detection for Emu_3352_SPI
    candidate_emu_dirs = [
        REPO_ROOT / "WP_3352_SPI" / "Emu_3352_SPI",
        REPO_ROOT / "Emu_3352_SPI",
    ]

    emu_dir = None
    for candidate in candidate_emu_dirs:
        if candidate.exists():
            emu_dir = candidate
            break

    if emu_dir is None:
        print("[WARN] Emu_3352_SPI directory not found. Skipping C source scans.")
    else:
        for path in emu_dir.rglob("*"):
            if not path.is_file():
                continue

            if path.suffix.lower() not in [".c", ".h", ".cla"]:
                continue

            if any(part in path.parts for part in ["CPU1_RAM", "CPU1_FLASH", "Debug", "Release"]):
                continue

            content = path.read_text(encoding="utf-8", errors="ignore")

            obsolete_tokens = [
                "VALID_STUB",
                "VALID_TEST",
                "WAVE_PAGE_STATE_ACTIVE",
                "WAVE_PAGE_STATE_ERROR",
            ]

            for token in obsolete_tokens:
                if token in content:
                    raise CheckFailure(
                        f"Obsolete page state token '{token}' found in C source: {path}"
                    )

    print("[PASS] Frozen architecture assertions and M5 state contract")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run the minimum ASR5K .agent governance gate."
    )
    parser.add_argument(
        "--all-links",
        action="store_true",
        help="Scan historical/reference Markdown in addition to ACTIVE files.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    checks = [
        ("Python syntax", check_python_syntax),
        ("YAML parse", check_yaml_parse),
        (
            "Markdown stale links",
            lambda: check_markdown_links(args.all_links),
        ),
        ("Frozen architecture", check_frozen_architecture),
    ]

    failures = []
    for name, check in checks:
        try:
            check()
        except Exception as exc:
            failures.append((name, str(exc)))
            print(f"[FAIL] {name}: {exc}")

    if failures:
        print(f"\n[FAIL] Governance gate: {len(failures)} check(s) failed.")
        return 1

    print("\n[PASS] Governance gate: all checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
