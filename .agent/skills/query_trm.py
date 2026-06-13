import argparse
import sys
from pathlib import Path

try:
    import fitz  # type: ignore
except ImportError:
    fitz = None


AGENT_ROOT = Path(__file__).resolve().parents[1]

PDF_MAP = {
    "ADC": AGENT_ROOT / "03_Knowledge/Peripheral/ADC/2838xTRM-ADC.pdf",
    "DMA": AGENT_ROOT / "03_Knowledge/Peripheral/DMA/28377_TRM-DMA.pdf",
    "EPWM": AGENT_ROOT / "03_Knowledge/Peripheral/EPWM/2838xTRM-PWM.pdf",
    "PWM": AGENT_ROOT / "03_Knowledge/Peripheral/EPWM/2838xTRM-PWM.pdf",
    "IPC": AGENT_ROOT / "03_Knowledge/Peripheral/IPC/28377_TRM-IPC.pdf",
    "MCBSP": AGENT_ROOT / "03_Knowledge/Peripheral/MCBSP/28388_TRM-MCBSP.pdf",
    "SPI": AGENT_ROOT / "03_Knowledge/SPI_Docs/SPI_TRM_28377.pdf",
}


def dependency_error() -> str:
    return (
        "PyMuPDF is unavailable. Install the declared tool dependencies with:\n"
        "    python -m pip install -r .agent/skills/requirements.txt"
    )


def dependency_check() -> int:
    if fitz is None:
        print(f"[FAIL] {dependency_error()}")
        return 1
    print("[PASS] PyMuPDF is available.")
    return 0


def resolve_module(module_name: str) -> str:
    module_upper = module_name.upper()
    if module_upper in PDF_MAP:
        return module_upper

    matches = [
        key
        for key in PDF_MAP
        if key in module_upper or module_upper in key
    ]
    if matches:
        return matches[0]

    supported = ", ".join(sorted(PDF_MAP))
    raise ValueError(
        f"Unsupported peripheral module '{module_name}'. Supported: {supported}"
    )


def search_register_pdf(module_name: str, register_name: str) -> str:
    if fitz is None:
        raise RuntimeError(dependency_error())

    module = resolve_module(module_name)
    pdf_path = PDF_MAP[module]
    if not pdf_path.is_file():
        raise FileNotFoundError(f"TRM PDF not found: {pdf_path}")

    print(f"[*] Searching {pdf_path} for {register_name}...")

    doc = fitz.open(pdf_path)
    matches = []
    for page_num, page in enumerate(doc):
        text = page.get_text("text")
        lowered = text.lower()
        if register_name.lower() not in lowered:
            continue

        score = 0
        if "bit" in lowered:
            score += 2
        if "register" in lowered:
            score += 1
        if "description" in lowered:
            score += 1
        if "value" in lowered:
            score += 1
        matches.append((score, page_num + 1, text))

    if not matches:
        return f"[FAIL] Register '{register_name}' not found in {module} TRM."

    matches.sort(key=lambda item: item[0], reverse=True)
    output = [
        f"--- {module} / {register_name}: {len(matches)} matching pages ---"
    ]
    for _, page_num, text in matches[:2]:
        cleaned = "\n".join(line.strip() for line in text.splitlines() if line.strip())
        output.extend(
            [
                f"\n[Page {page_num}]",
                cleaned,
                "-" * 50,
            ]
        )
    return "\n".join(output)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Search bundled TI C2000 TRM PDFs for a register."
    )
    parser.add_argument(
        "--dependency-check",
        action="store_true",
        help="Check whether PyMuPDF is available without opening a PDF.",
    )
    parser.add_argument("module", nargs="?")
    parser.add_argument("register", nargs="?")
    args = parser.parse_args()

    if not args.dependency_check and (not args.module or not args.register):
        parser.error("module and register are required")
    return args


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    args = parse_args()
    if args.dependency_check:
        return dependency_check()

    try:
        print(search_register_pdf(args.module, args.register))
        return 0
    except (FileNotFoundError, RuntimeError, ValueError) as exc:
        print(f"[FAIL] {exc}")
        return 1
    except Exception as exc:
        print(f"[FAIL] Unexpected TRM query error: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
