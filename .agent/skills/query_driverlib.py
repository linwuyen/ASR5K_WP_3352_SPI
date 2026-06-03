import sys
import os
import re
import glob

# ------------------------------------------------------------------------------
# C2000Ware DriverLib 路徑解析（支援環境變數覆蓋與版本自動搜尋）
# ------------------------------------------------------------------------------
def _find_driverlib() -> str:
    """
    按優先順序尋找 C2000Ware DriverLib 安裝路徑：
    1. 環境變數 C2000WARE_DRIVERLIB
    2. 多個預設安裝磁碟機與版本號的 glob 搜尋（取最新版本）
    3. 硬編碼的預設路徑（fallback）
    """
    # 優先使用環境變數
    env_path = os.environ.get("C2000WARE_DRIVERLIB")
    if env_path and os.path.exists(env_path):
        return env_path

    # 自動搜尋多個磁碟機下的所有 C2000Ware 版本
    candidates = []
    for drive in ["C", "D", "E"]:
        pattern = fr"{drive}:\ti\c2000\C2000Ware_*\driverlib\f2838x\driverlib"
        candidates.extend(glob.glob(pattern))

    if candidates:
        # 排序後取最新版本（版本號字串排序）
        return sorted(candidates)[-1]

    # 最終 fallback：硬編碼預設路徑
    return r"C:\ti\c2000\C2000Ware_5_04_00_00\driverlib\f2838x\driverlib"


DRIVERLIB_DIR = _find_driverlib()

# 單次搜尋最多回傳的函式定義數量（防止通用關鍵字導致 token 爆炸）
MAX_RESULTS = 5


def search_driverlib(module_name, search_term):
    if not os.path.exists(DRIVERLIB_DIR):
        return (
            f"[Error] 找不到 C2000Ware DriverLib 目錄：{DRIVERLIB_DIR}\n"
            f"請確認 C2000Ware 是否已安裝，或設定環境變數 C2000WARE_DRIVERLIB 覆蓋預設路徑。\n"
            f"安裝後重新執行此腳本。"
        )

    # 1. 尋找與 module_name 相關的檔案 (.h 與 .c)
    module_lower = module_name.lower()
    files_in_dir = os.listdir(DRIVERLIB_DIR)

    target_files = []
    for f in files_in_dir:
        # 精確匹配 (如 spi.h / spi.c) 或前綴匹配
        if f.lower().startswith(module_lower + ".") or f.lower() == module_lower + ".h" or f.lower() == module_lower + ".c":
            target_files.append(f)

    if not target_files:
        # 如果沒有精確匹配的，嘗試模糊匹配檔名中含有 module_lower 的
        for f in files_in_dir:
            if module_lower in f.lower() and (f.endswith(".h") or f.endswith(".c")):
                target_files.append(f)

    if not target_files:
        return f"[Error] 找不到與模組 '{module_name}' 相關的驅動檔案。現有檔案範例：{', '.join(files_in_dir[:10])}..."

    results = []
    results.append(f"[*] 正在 {DRIVERLIB_DIR} 中尋找模組 '{module_name}' 的關鍵字 '{search_term}'...\n")

    found_any = False
    printed_lines = set()
    result_count = 0

    for filename in sorted(target_files):
        if result_count >= MAX_RESULTS:
            break

        filepath = os.path.join(DRIVERLIB_DIR, filename)
        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception as e:
            results.append(f"[Error] 無法讀取檔案 {filename}: {str(e)}")
            continue

        # 在檔案中搜尋 search_term
        lines = content.split('\n')

        idx = 0
        while idx < len(lines) and result_count < MAX_RESULTS:
            line = lines[idx]
            if search_term.lower() in line.lower():
                # 排除純註解行與空行
                trimmed = line.strip()
                if trimmed.startswith('//') or trimmed.startswith('*') or trimmed.startswith('/*') or not trimmed:
                    idx += 1
                    continue

                # 往上尋找函數宣告的起點
                func_start_idx = idx
                while func_start_idx > 0:
                    prev_line = lines[func_start_idx - 1].strip()
                    if not prev_line or prev_line.startswith('//') or prev_line.startswith('/*') or prev_line.startswith('*') or prev_line.endswith(';') or prev_line.endswith('{') or prev_line.endswith('}'):
                        break
                    func_start_idx -= 1

                # 往上尋找該函數的前置 Doxygen 註解
                comment_lines = []
                comment_start_idx = func_start_idx - 1
                while comment_start_idx >= 0:
                    prev_line = lines[comment_start_idx].strip()
                    if prev_line.startswith('//!'):
                        comment_lines.insert(0, lines[comment_start_idx])
                        comment_start_idx -= 1
                    elif prev_line.startswith('*') or prev_line.startswith('/*') or prev_line.endswith('*/') or prev_line.startswith('//'):
                        comment_lines.insert(0, lines[comment_start_idx])
                        comment_start_idx -= 1
                        if '/*' in prev_line:
                            break
                    else:
                        break

                # 往下抓取完整的函式原型（到第一個 ; 或 {）
                func_lines = []
                func_end_idx = func_start_idx
                found_delimiter = False
                while func_end_idx < len(lines):
                    curr_line = lines[func_end_idx]
                    func_lines.append(curr_line)
                    if ';' in curr_line or '{' in curr_line:
                        found_delimiter = True
                        break
                    func_end_idx += 1
                    if func_end_idx - func_start_idx > 15:  # 防止無限往下找
                        break

                if found_delimiter:
                    func_decl_str = " ".join([l.strip() for l in func_lines])
                    if func_decl_str not in printed_lines:
                        printed_lines.add(func_decl_str)
                        found_any = True
                        result_count += 1
                        results.append(f"--- 找到於 {filename} 中的定義 [{result_count}/{MAX_RESULTS}] ---")
                        if comment_lines:
                            results.append("\n".join(comment_lines))
                        results.append("\n".join(func_lines))
                        results.append("-" * 60 + "\n")

                    # 避免重複匹配該函數內部的參數行
                    idx = max(idx, func_end_idx)
            idx += 1

    if result_count >= MAX_RESULTS:
        results.append(f"[!] 已達最大顯示數量 {MAX_RESULTS}，請使用更精確的 function_name 縮小範圍。")

    if not found_any:
        # 如果沒有找到完整的定義，嘗試模糊搜尋含該關鍵字的行
        results.append(f"[!] 找不到完整的函式宣告/定義。以下為含有關鍵字 '{search_term}' 的所有行：\n")
        line_count = 0
        for filename in sorted(target_files):
            filepath = os.path.join(DRIVERLIB_DIR, filename)
            try:
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    lines = f.readlines()
            except:
                continue

            for i, line in enumerate(lines):
                if search_term.lower() in line.lower():
                    results.append(f"{filename}:{i+1}: {line.strip()}")
                    found_any = True
                    line_count += 1
                    if line_count >= 20:  # 模糊搜尋最多顯示 20 行
                        results.append("[!] 模糊搜尋結果超過 20 行，已截斷。")
                        break
            if line_count >= 20:
                break

    if not found_any:
        return f"[Error] 在模組 '{module_name}' 的檔案中完全找不到關鍵字 '{search_term}' 的內容。"

    return "\n".join(results)


if __name__ == "__main__":
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')

    if len(sys.argv) < 3:
        print("[Error] 參數不足。用法: python query_driverlib.py <模組名稱> <關鍵字/函式名>")
        print("範例: python query_driverlib.py SPI SPI_writeDataNonBlocking")
        print(f"\n[Info] 目前 DriverLib 路徑: {DRIVERLIB_DIR}")
        print("[Info] 可設定環境變數 C2000WARE_DRIVERLIB 覆蓋預設路徑")
        sys.exit(1)

    module = sys.argv[1]
    keyword = sys.argv[2]

    result = search_driverlib(module, keyword)
    print(result)
