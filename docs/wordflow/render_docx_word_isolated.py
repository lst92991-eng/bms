from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import fitz
import psutil
import win32com.client


WD_ALERTS_NONE = 0
WD_EXPORT_FORMAT_PDF = 17
WD_EXPORT_OPTIMIZE_FOR_PRINT = 0
WD_EXPORT_ALL_DOCUMENT = 0
WD_EXPORT_DOCUMENT_CONTENT = 0
WD_EXPORT_CREATE_HEADING_BOOKMARKS = 1


def _word_pid(application, existing_word_pids: set[int]) -> int:
    import ctypes

    # 新版 Word 通常公开 Application.Hwnd；某些 Office 构建在尚未创建文档窗口时
    # 不公开该属性，因此保留“进程集合差”作为安全回退。两种路径都必须证明 PID
    # 不属于启动前已存在的 WINWORD.EXE，绝不连接或终止用户原有会话。
    try:
        hwnd = int(application.Hwnd)
    except (AttributeError, TypeError, ValueError):
        hwnd = 0
    if hwnd:
        pid = ctypes.c_ulong(0)
        ctypes.windll.user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value and int(pid.value) not in existing_word_pids:
            return int(pid.value)

    deadline = time.monotonic() + 10.0
    while time.monotonic() < deadline:
        new_pids = {
            proc.pid
            for proc in psutil.process_iter(["name"])
            if (proc.info.get("name") or "").lower() == "winword.exe"
            and proc.pid not in existing_word_pids
        }
        if len(new_pids) == 1:
            return next(iter(new_pids))
        if len(new_pids) > 1:
            raise RuntimeError(f"检测到多个新 WINWORD.EXE，拒绝继续：{sorted(new_pids)}")
        time.sleep(0.1)
    raise RuntimeError("无法唯一识别新建的隔离 WINWORD.EXE 进程")


def _update_all_fields(document) -> None:
    # 先物化原始字段，再更新目录对象，最后刷新正文/页眉页脚中的页码字段。
    document.Fields.Update()
    for index in range(1, document.TablesOfContents.Count + 1):
        document.TablesOfContents(index).Update()
    document.Fields.Update()
    for section_index in range(1, document.Sections.Count + 1):
        section = document.Sections(section_index)
        for story_kind in range(1, 4):
            try:
                story = section.Headers(story_kind).Range
                story.Fields.Update()
            except Exception:
                pass
            try:
                story = section.Footers(story_kind).Range
                story.Fields.Update()
            except Exception:
                pass


def render(docx_path: Path, output_dir: Path, dpi: int) -> dict[str, object]:
    docx_path = docx_path.resolve()
    output_dir = output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    pdf_path = output_dir / f"{docx_path.stem}.pdf"

    existing_word_pids = {
        proc.pid
        for proc in psutil.process_iter(["name"])
        if (proc.info.get("name") or "").lower() == "winword.exe"
    }

    app = None
    document = None
    pid = None
    try:
        # DispatchEx 强制创建新 COM 实例；严禁 GetActiveObject/Dispatch 复用 ROT 中的 WPS。
        app = win32com.client.DispatchEx("Word.Application")
        app.Visible = False
        app.DisplayAlerts = WD_ALERTS_NONE
        if str(app.Name).strip().lower() != "microsoft word":
            raise RuntimeError(f"Word.Application 返回了非 Microsoft Word 实例：{app.Name}")
        pid = _word_pid(app, existing_word_pids)
        process = psutil.Process(pid)
        if pid in existing_word_pids or process.name().lower() != "winword.exe":
            raise RuntimeError(
                f"未获得全新 Microsoft Word 进程：pid={pid}, name={process.name()}"
            )

        document = app.Documents.Open(
            str(docx_path),
            ConfirmConversions=False,
            ReadOnly=False,
            AddToRecentFiles=False,
            Visible=False,
        )
        _update_all_fields(document)
        document.Save()
        document.ExportAsFixedFormat(
            OutputFileName=str(pdf_path),
            ExportFormat=WD_EXPORT_FORMAT_PDF,
            OpenAfterExport=False,
            OptimizeFor=WD_EXPORT_OPTIMIZE_FOR_PRINT,
            Range=WD_EXPORT_ALL_DOCUMENT,
            Item=WD_EXPORT_DOCUMENT_CONTENT,
            IncludeDocProps=True,
            KeepIRM=True,
            CreateBookmarks=WD_EXPORT_CREATE_HEADING_BOOKMARKS,
            DocStructureTags=True,
            BitmapMissingFonts=True,
            UseISO19005_1=False,
        )
        document.Close(SaveChanges=False)
        document = None
        app.Quit()
        app = None
    finally:
        if document is not None:
            try:
                document.Close(SaveChanges=False)
            except Exception:
                pass
        if app is not None:
            try:
                app.Quit()
            except Exception:
                pass

    if pid is not None:
        try:
            psutil.Process(pid).wait(timeout=15)
        except (psutil.NoSuchProcess, psutil.TimeoutExpired):
            pass

    if not pdf_path.exists() or pdf_path.stat().st_size == 0:
        raise RuntimeError(f"Word 未生成有效 PDF：{pdf_path}")

    scale = dpi / 72.0
    matrix = fitz.Matrix(scale, scale)
    rendered = []
    with fitz.open(pdf_path) as pdf:
        page_count = pdf.page_count
        for index, page in enumerate(pdf, start=1):
            pixmap = page.get_pixmap(matrix=matrix, alpha=False)
            png_path = output_dir / f"page-{index:03d}.png"
            pixmap.save(png_path)
            rendered.append(str(png_path))

    return {
        "docx": str(docx_path),
        "docx_bytes": docx_path.stat().st_size,
        "isolated_winword_pid": pid,
        "pdf": str(pdf_path),
        "pdf_bytes": pdf_path.stat().st_size,
        "dpi": dpi,
        "page_count": page_count,
        "png_pages": rendered,
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="用全新 Microsoft Word COM 实例安全更新字段并渲染 DOCX。"
    )
    parser.add_argument("docx", type=Path)
    parser.add_argument("--output_dir", required=True, type=Path)
    parser.add_argument("--dpi", type=int, default=144)
    args = parser.parse_args()
    if args.dpi < 96:
        raise SystemExit("DPI 必须不小于 96")
    result = render(args.docx, args.output_dir, args.dpi)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
