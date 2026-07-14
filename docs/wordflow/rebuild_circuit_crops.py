from __future__ import annotations

import argparse
import io
from pathlib import Path

import fitz
from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "official_chip_docs_files"

BMS_PDF = DOCS / "SCH_机器人BMS板_2026-06-17.pdf"
CONTROL_PDF = DOCS / "SCH_Schematic1_2026-06-17.pdf"

# Normalized page rectangles (left, top, right, bottom).  Each rectangle was
# selected against the original vector PDF so that no wire, label, component,
# or electrically meaningful annotation is reconstructed or altered.
CROPS: tuple[tuple[Path, int, tuple[float, float, float, float], str], ...] = (
    (BMS_PDF, 0, (0.09, 0.335, 0.875, 0.485), "bms_p01_main_power_path.png"),
    (CONTROL_PDF, 0, (0.005, 0.005, 0.52, 0.215), "control_p01_input_soft_enable.png"),
    (CONTROL_PDF, 0, (0.005, 0.226, 0.995, 0.465), "control_p01_sc_power_stage.png"),
    (CONTROL_PDF, 0, (0.005, 0.515, 0.565, 1.0), "control_p01_sc_controller.png"),
    (CONTROL_PDF, 2, (0.012, 0.012, 0.93, 0.375), "control_p03_bms_connector.png"),
    (CONTROL_PDF, 2, (0.015, 0.385, 0.465, 0.765), "control_p03_wake.png"),
    (CONTROL_PDF, 2, (0.47, 0.385, 0.75, 0.66), "control_p03_shutdown.png"),
    (CONTROL_PDF, 3, (0.015, 0.015, 0.455, 0.365), "control_p04_buck5v.png"),
    (CONTROL_PDF, 3, (0.455, 0.015, 0.995, 0.365), "control_p04_enable_logic.png"),
    (CONTROL_PDF, 3, (0.015, 0.36, 0.455, 0.64), "control_p04_ldo3v3.png"),
    (CONTROL_PDF, 5, (0.015, 0.015, 0.63, 0.36), "control_p06_canfd.png"),
    (CONTROL_PDF, 6, (0.02, 0.075, 0.38, 0.37), "control_p07_oled.png"),
    (CONTROL_PDF, 6, (0.395, 0.09, 0.72, 0.405), "control_p07_buzzer.png"),
    (CONTROL_PDF, 8, (0.085, 0.035, 0.31, 0.315), "control_p09_outputs.png"),
    (CONTROL_PDF, 9, (0.18, 0.215, 0.47, 0.41), "control_p10_eeprom.png"),
    # Appendix C uses electrically complete, readable crops for sparse pages;
    # blank drafting space and title blocks remain available in the source PDF.
    (CONTROL_PDF, 5, (0.015, 0.015, 0.63, 0.36), "control_schematic_p06.png"),
    (CONTROL_PDF, 7, (0.285, 0.16, 0.60, 0.305), "control_schematic_p08.png"),
    (CONTROL_PDF, 8, (0.085, 0.035, 0.31, 0.315), "control_schematic_p09.png"),
    (CONTROL_PDF, 9, (0.18, 0.215, 0.47, 0.41), "control_schematic_p10.png"),
)


def _clip(page: fitz.Page, normalized: tuple[float, float, float, float]) -> fitz.Rect:
    left, top, right, bottom = normalized
    if not (0.0 <= left < right <= 1.0 and 0.0 <= top < bottom <= 1.0):
        raise ValueError(f"Invalid normalized crop: {normalized}")
    rect = page.rect
    return fitz.Rect(
        rect.x0 + rect.width * left,
        rect.y0 + rect.height * top,
        rect.x0 + rect.width * right,
        rect.y0 + rect.height * bottom,
    )


def _render_image(
    pdf_path: Path,
    page_index: int,
    normalized: tuple[float, float, float, float],
    *,
    dpi: int,
) -> Image.Image:
    with fitz.open(pdf_path) as pdf:
        page = pdf[page_index]
        pixmap = page.get_pixmap(
            matrix=fitz.Matrix(dpi / 72.0, dpi / 72.0),
            clip=_clip(page, normalized),
            alpha=False,
        )
        with Image.open(io.BytesIO(pixmap.tobytes("png"))) as image:
            return image.convert("RGB")


def _save(image: Image.Image, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output, format="PNG", optimize=True)


def _build_page7_appendix(output_dir: Path, *, dpi: int) -> None:
    oled = _render_image(
        CONTROL_PDF,
        6,
        (0.02, 0.075, 0.38, 0.37),
        dpi=dpi,
    )
    buzzer = _render_image(
        CONTROL_PDF,
        6,
        (0.395, 0.09, 0.72, 0.405),
        dpi=dpi,
    )
    width = max(oled.width, buzzer.width)
    gap = max(24, round(width * 0.025))
    canvas = Image.new("RGB", (width, oled.height + gap + buzzer.height), "white")
    canvas.paste(oled, ((width - oled.width) // 2, 0))
    canvas.paste(buzzer, ((width - buzzer.width) // 2, oled.height + gap))
    _save(canvas, output_dir / "control_schematic_p07.png")


def _build_bms_precharge_reverse(output_dir: Path, *, dpi: int) -> None:
    precharge = _render_image(
        BMS_PDF,
        0,
        # Leave a deliberate lower margin below the Q4 package text; the old
        # 0.31 boundary intersected the source text box by about 1.6 pt.
        (0.225, 0.115, 0.42, 0.325),
        dpi=dpi,
    )
    reverse_output = _render_image(
        BMS_PDF,
        0,
        (0.58, 0.25, 0.82, 0.46),
        dpi=dpi,
    )
    height = max(precharge.height, reverse_output.height)
    gap = max(24, round(height * 0.025))
    canvas = Image.new(
        "RGB",
        (precharge.width + gap + reverse_output.width, height),
        "white",
    )
    canvas.paste(precharge, (0, (height - precharge.height) // 2))
    canvas.paste(
        reverse_output,
        (precharge.width + gap, (height - reverse_output.height) // 2),
    )
    _save(canvas, output_dir / "bms_p01_precharge_reverse.png")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Regenerate source-faithful schematic crops from vector PDFs."
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=ROOT / "tmp" / "circuit_crops_v2",
    )
    parser.add_argument("--dpi", type=int, default=320)
    args = parser.parse_args()

    if args.dpi < 200:
        raise ValueError("DPI must be at least 200 for readable schematic labels")
    for source, page_index, normalized, filename in CROPS:
        if not source.exists():
            raise FileNotFoundError(source)
        image = _render_image(source, page_index, normalized, dpi=args.dpi)
        _save(image, args.output_dir / filename)
    _build_bms_precharge_reverse(args.output_dir, dpi=args.dpi)
    _build_page7_appendix(args.output_dir, dpi=args.dpi)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
