import sys
import os

try:
    import fitz  # PyMuPDF
except ImportError:
    print("[Error] 缺少 PyMuPDF 套件，無法進行 TRM PDF 檢索。")
    print("請執行以下指令安裝後重新執行：")
    print("    pip install pymupdf")
    sys.exit(1)


# 對照表：將週邊模組名稱映射到實體 PDF 檔案
PDF_MAP = {
    "ADC": ".agent/specs/Peripheral/ADC/2838xTRM-ADC.pdf",
    "DMA": ".agent/specs/Peripheral/DMA/28377_TRM-DMA.pdf",
    "EPWM": ".agent/specs/Peripheral/EPWM/2838xTRM-PWM.pdf",
    "PWM": ".agent/specs/Peripheral/EPWM/2838xTRM-PWM.pdf",
    "IPC": ".agent/specs/Peripheral/IPC/28377_TRM-IPC.pdf",
    "MCBSP": ".agent/specs/Peripheral/MCBSP/28388_TRM-MCBSP.pdf",
    "SPI": ".agent/specs/Peripheral/SPI/28377_TRM-SPI.pdf",
}

def search_register_pdf(module_name, register_name):
    module_upper = module_name.upper()
    if module_upper not in PDF_MAP:
        # 如果不在對照表中，尋找是否有部分匹配的
        matched_keys = [k for k in PDF_MAP.keys() if k in module_upper or module_upper in k]
        if matched_keys:
            module_upper = matched_keys[0]
        else:
            return f"[Error] 未支援的週邊模組：{module_name}。目前支援的模組為：{', '.join(PDF_MAP.keys())}。"

    pdf_path = PDF_MAP[module_upper]
    if not os.path.exists(pdf_path):
        # 支援相對路徑執行（若是從不同 Cwd 呼叫）
        # 尋找 workspace 根目錄下的路徑
        possible_paths = [
            pdf_path,
            os.path.join(os.path.dirname(__file__), "..", "..", pdf_path),
            os.path.join(os.getcwd(), pdf_path)
        ]
        found = False
        for p in possible_paths:
            if os.path.exists(p):
                pdf_path = p
                found = True
                break
        if not found:
            return f"[Error] 找不到 PDF 檔案：{pdf_path}，請確認檔案是否存在。"

    print(f"[*] 正在 {pdf_path} 中檢索暫存器 {register_name}...")
    
    try:
        doc = fitz.open(pdf_path)
        matches = []
        
        # 搜尋暫存器
        for page_num in range(len(doc)):
            page = doc[page_num]
            text = page.get_text("text")
            
            # 使用 case-insensitive 搜尋
            if register_name.lower() in text.lower():
                matches.append((page_num + 1, text))
                
        if not matches:
            return f"[Error] 在 {module_upper} 手冊中查無暫存器：{register_name}。"
            
        # 排序與篩選：優先尋找可能包含「暫存器定義/表格」的頁面
        # 例如：同時含有暫存器名稱與 "bit" 或 "field" 或 "description" 的頁面
        best_matches = []
        for pnum, text in matches:
            score = 0
            if "bit" in text.lower(): score += 2
            if "register" in text.lower(): score += 1
            if "description" in text.lower(): score += 1
            if "value" in text.lower(): score += 1
            best_matches.append((score, pnum, text))
            
        best_matches.sort(key=lambda x: x[0], reverse=True)
        
        # 輸出前 2 個最相關的頁面內容以防止 token 膨脹
        output = []
        output.append(f"--- 找到 {module_upper} 模組的 {register_name} 暫存器定義 (共匹配 {len(matches)} 頁) ---")
        
        display_count = min(2, len(best_matches))
        for i in range(display_count):
            score, pnum, text = best_matches[i]
            output.append(f"\n[第 {pnum} 頁的檢索內容]:")
            
            # 整理並清除非必要的空白行
            lines = text.split('\n')
            lines = [l.strip() for l in lines if l.strip()]
            cleaned_text = "\n".join(lines)
            output.append(cleaned_text)
            output.append("-" * 50)
            
        return "\n".join(output)
        
    except Exception as e:
        return f"[Error] 讀取或檢索 PDF 時發生錯誤: {str(e)}"

if __name__ == "__main__":
    # 強制將標準輸出重設為 UTF-8，防止 Windows (CP950) 終端機編碼出錯
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')

    if len(sys.argv) < 3:
        print("[Error] 參數不足。用法: python query_trm.py <模組名稱> <暫存器名稱>")
        print("範例: python query_trm.py EPWM AQCTLA")
        sys.exit(1)
        
    module = sys.argv[1]
    register = sys.argv[2]
    
    result = search_register_pdf(module, register)
    print(result)