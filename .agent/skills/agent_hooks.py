# ==============================================================================
# Google Antigravity SDK - Agent Hooks Implementation
# Target: TI C2000 Firmware Development Constraints
# Version: 1.2.0
# ==============================================================================

import re
import os
import logging
from antigravity import hooks, events, AgentAction, ValidationResult

# ------------------------------------------------------------------------------
# Hook 1: 沙盒強制攔截器 (Sandbox Enforcer)
# 觸發時機：在 Agent 企圖呼叫 write_to_file 工具之前
# 功能：確保在 /implement 階段，所有的代碼產出都強制加上 _preview 後綴
# 命名規則：spi_hal.c → spi_hal_preview.c（後綴式，更直觀）
# ------------------------------------------------------------------------------
@hooks.pre_tool_call(tool_name="write_to_file")
def enforce_sandbox_prefix(event: events.ToolCallEvent) -> AgentAction:
    try:
        state = event.context.current_phase

        # 僅在實作階段進行攔截
        if state == "implement":
            target_file = event.tool_args.get("TargetFile", "")

            if not target_file:
                logging.warning("[Sandbox] TargetFile 未提供，無法執行沙盒前綴保護，允許通過。")
                return AgentAction.ALLOW()

            dirname, filename = os.path.split(target_file)
            basename, ext = os.path.splitext(filename)

            # 若已是預覽檔案（後綴或前綴均接受），直接放行
            if "_preview" in basename:
                return AgentAction.ALLOW()

            # 強制轉換為後綴式命名：spi_hal.c → spi_hal_preview.c
            new_filename = f"{basename}_preview{ext}"
            new_target_path = os.path.join(dirname, new_filename) if dirname else new_filename

            event.tool_args["TargetFile"] = new_target_path
            event.tool_args["IsArtifact"] = True
            event.tool_args["ArtifactMetadata"] = {
                "ArtifactType": "implementation_plan",
                "Summary": "C2000 自動生成的沙盒預覽代碼，請確認無誤後手動覆蓋正式檔案。",
                "RequestFeedback": True
            }

            return AgentAction.MODIFY_ARGS(
                reason=f"[Sandbox Lock] 沙盒防護啟動，輸出路徑重定向：{target_file} → {new_target_path}",
                new_args=event.tool_args
            )

        return AgentAction.ALLOW()

    except Exception as e:
        logging.error(f"[Hook Error] enforce_sandbox_prefix 發生異常: {e}，降級允許通過。")
        return AgentAction.ALLOW()  # 降級允許，防止主流程阻塞

# ------------------------------------------------------------------------------
# Hook 2: MS950 編碼與 ASCII 檢查 (MS950 Compliance Validator)
# 觸發時機：代碼生成完畢，尚未渲染給使用者前
# 功能：防止 CCS 編輯器產生中文亂碼
# ------------------------------------------------------------------------------
@hooks.post_code_generation
def verify_ms950_compliance(event: events.CodeGenerationEvent) -> ValidationResult:
    try:
        code_content = event.generated_code

        # 去除 UTF-8 BOM（\ufeff）避免 BOM 誤觸非 ASCII 判斷
        code_content = code_content.lstrip('\ufeff')

        # 檢查是否包含非 ASCII 字符（適用於 C/H 檔）
        if event.file_extension in [".c", ".h"]:
            if not all(ord(c) < 128 for c in code_content):
                bom_hint = "（若為 BOM 問題，請確認編輯器不輸出 BOM）"
                return ValidationResult.REJECT(
                    feedback=f"[編碼錯誤] C/H 檔案中檢測到非 ASCII 字符（如中文註解）。請嚴格遵守 MS950 規範，全數改為英文。{bom_hint}"
                )

        return ValidationResult.PASS()

    except Exception as e:
        logging.error(f"[Hook Error] verify_ms950_compliance 發生異常: {e}，降級允許通過。")
        return ValidationResult.PASS()

# ------------------------------------------------------------------------------
# Hook 3: C2000 物理邊界與第一性原理掃描 (Core Constraints Scanner)
# 觸發時機：準備寫入檔案時
# 功能：利用 Regex 捕捉所有被嚴格禁止的語法 (除法、動態記憶體、原生型別)
# ------------------------------------------------------------------------------
def strip_c_comments(text: str) -> str:
    """
    移除 C 語言中的所有註解，防止掃描時誤判註解中的關鍵字。
    注意：若 '*/' 出現在字串常數內（極罕見）可能提早截斷，C2000 韌體代碼中此情況可接受。
    """
    # 移除多行註解 /* ... */（re.DOTALL 使 . 可匹配換行，lazy match 防止過度匹配）
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    # 移除單行註解 // ...
    text = re.sub(r'//.*', '', text)
    return text

@hooks.pre_file_save
def verify_c2000_constraints(event: events.FileSaveEvent) -> ValidationResult:
    try:
        if event.filename.endswith(".md"):
            return ValidationResult.PASS()  # Markdown 文件不掃描 C 語法

        # 剔除所有 C 註解，防止註解文字引發誤判 (Comment False Positives)
        content = strip_c_comments(event.content)
        violations = []

        # 1. 檢查零除法 (Zero Division)
        # 匹配「識別字 / 識別字」的模式，避免 URL (http://) 或路徑 (path/to) 誤判
        if re.search(r'\b[A-Za-z_]\w*\s*/\s*[A-Za-z_0-9]\w*\b', content):
            violations.append("檢測到除法運算符 `/`。請改用倒數乘法或 IQmath/TMU。")

        # 2. 檢查動態記憶體 (Zero Malloc)
        if re.search(r'\b(malloc|free|calloc|realloc)\b', content):
            violations.append("絕對禁止使用動態記憶體配置 (malloc/free)。")

        # 3. 檢查原生型別與 C28x 不支援的 8-bit 型別 (Native Type Ban)
        if re.search(r'\b(int|char|long|short|uint8_t|int8_t)\b', content):
            violations.append("檢測到原生型別或 8-bit 型別。C28x 最小定址單位為 16-bit，請使用 <stdint.h> (如 uint16_t)。")

        # 4. 檢查阻塞迴圈 (Zero Blocking)
        if re.search(r'while\s*\([^)]*(flag|status|ready)[^)]*\)\s*;', content, re.IGNORECASE):
            violations.append("檢測到硬體旗標的 while 阻塞等待。請改用 FSM 或 ISR。")

        if violations:
            error_msg = "\n".join(f"- {v}" for v in violations)
            return ValidationResult.REJECT(
                feedback=f"<pre_flight_check> 失敗，檢測到違反 C2000 物理邊界：\n{error_msg}\n請修正後再重新生成。"
            )

        return ValidationResult.PASS()

    except Exception as e:
        logging.error(f"[Hook Error] verify_c2000_constraints 發生異常: {e}，降級允許通過。")
        return ValidationResult.PASS()
