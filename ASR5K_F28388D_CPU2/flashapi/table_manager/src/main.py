import json
import csv
import os
import re
import subprocess

class TableManagerFSM:
    def __init__(self):
        self.state = 'INIT'
        self.json_file = os.path.join(os.path.dirname(__file__), '..', '..', 'PRB6K_DAB_BBOX.json')
        self.csv_file = os.path.join(os.path.dirname(__file__), '..', '..', 'PRB6K_DAB_BBOX.csv')
        self.c_file = os.path.join(os.path.dirname(__file__), '..', '..', 'f021Bbox.c')
        self.last_length = None

    def run(self):
        while True:
            if self.state == 'INIT':
                self.display_menu()
                choice = input("請選擇動作 (1/2/3/4): ").strip()
                if choice == '1':
                    self.state = 'IMPORT_FROM_EXTERNAL_JSON'
                elif choice == '2':
                    self.state = 'EXPORT_TO_EXTERNAL_JSON'
                elif choice == '3':
                    self.state = 'EXECUTE_MODBUS_MASTER'
                elif choice == '4':
                    print("退出。")
                    return
                else:
                    print("無效選擇，請重新輸入。")
                    continue
            elif self.state == 'IMPORT_FROM_EXTERNAL_JSON':
                self.import_from_external_json()
                self.state = 'INIT'
            elif self.state == 'EXPORT_TO_EXTERNAL_JSON':
                self.export_to_external_json()
                self.state = 'INIT'
            elif self.state == 'EXECUTE_MODBUS_MASTER':
                self.execute_modbus_master()
                self.state = 'INIT'

    def display_menu(self):
        print("\n請選擇動作：")
        print("1. Import Table from user pointer external JSON")
        print("2. Export Table to external JSON by user")
        print("3. Execute ModbusMaster")
        print("4. Exit")

    def import_from_external_json(self):
        json_path = input("請輸入外部 JSON 文件路徑：").strip()
        if os.path.exists(json_path):
            try:
                with open(json_path, 'r') as f:
                    external_data = json.load(f)
                # 更新內部 JSON
                with open(self.json_file, 'w') as f:
                    json.dump(external_data, f, indent=4)
                # 解析並修改 C 文件
                self.modify_c_file(external_data, json_path)
                print("已從外部 JSON 匯入 Table 並更新 C 文件。")
            except json.JSONDecodeError:
                print("JSON 文件格式錯誤。")
        else:
            print("文件不存在。")

    def modify_c_file(self, data, json_path):
        backup_file = os.path.join(os.path.dirname(json_path), 'BBOX_BACKUP.json')
        if os.path.exists(self.c_file):
            with open(self.c_file, 'r') as f:
                content = f.read()
            # 找到 bboxTableTemplate 的定義
            match = re.search(r'(const\s+ST_BLACKBOX\s+bboxTableTemplate\[\]\s*=\s*\{)([^}]+)(\};)', content, re.DOTALL)
            if match:
                prefix = match.group(1)
                table_content = match.group(2)
                suffix = match.group(3)
                # 提取現有的 BBOX_VAR
                existing_vars = re.findall(r'BBOX_VAR\(([^)]+)\)', table_content)
                if existing_vars:
                    # 備份現有 table 到 BBOX_BACKUP.json，使用與匯出時相同的長度推斷邏輯
                    backup_data = {}
                    for var in existing_vars:
                        if 'u32' in var or 's32' in var or 'f32' in var:
                            size = 4
                        elif 'u16' in var or 's16' in var:
                            size = 2
                        else:
                            # 沒有命名，提示輸入長度
                            length_input = input(f"請輸入變數 '{var}' 的長度 (BYTE): ").strip()
                            if length_input == '':
                                if self.last_length is not None:
                                    size = self.last_length
                                else:
                                    print("無上次長度，使用預設長度 4")
                                    size = 4
                                    self.last_length = 4
                            elif length_input.isdigit():
                                size = int(length_input)
                                self.last_length = size
                            else:
                                print("無效輸入，使用上次長度或預設長度")
                                if self.last_length is not None:
                                    size = self.last_length
                                else:
                                    size = 4
                                    self.last_length = 4
                        backup_data[var] = size
                    with open(backup_file, 'w') as f:
                        json.dump(backup_data, f, indent=4)
                    print(f"已備份現有 table 到 {backup_file}")
                # 生成新的 table 內容，匹配用戶要求的格式：第一個條目有5個空格
                if data:
                    first_name = next(iter(data.keys()))
                    new_entries = [f'\n     BBOX_VAR({first_name}),'] + [f'\n     BBOX_VAR({name}),' for name in list(data.keys())[1:]]
                    new_table_content = ''.join(new_entries) + '\n     END_OF_BBOX_VAR'
                else:
                    new_table_content = '\n     END_OF_BBOX_VAR'
                # 替換內容
                new_content = content.replace(match.group(0), prefix + new_table_content + suffix)
                # 寫回 C 文件
                with open(self.c_file, 'w') as f:
                    f.write(new_content)
                print("已更新 bboxTableTemplate 在 f021Bbox.c 中")
            else:
                print("無法解析 C 文件中的 bboxTableTemplate。")
        else:
            print("C 文件不存在。")

    def export_to_external_json(self):
        file_path = input("請輸入匯出檔案路徑或名稱：").strip()
        if os.path.isabs(file_path):
            # 絕對路徑
            if not file_path.endswith('.json'):
                file_path += '.json'
            dir_path = os.path.dirname(file_path)
            base_name = os.path.splitext(os.path.basename(file_path))[0]
            ext = '.json'
        else:
            # 相對路徑或只有檔名，放在 flashapi 下
            flashapi_dir = os.path.join(os.path.dirname(__file__), '..', '..')
            if not file_path.endswith('.json'):
                file_path += '.json'
            dir_path = flashapi_dir
            base_name = os.path.splitext(os.path.basename(file_path))[0]
            ext = '.json'
        # 確保目錄存在
        if not os.path.exists(dir_path):
            os.makedirs(dir_path)
            print(f"已創建目錄 {dir_path}")
        # 檢查文件是否存在，如果存在詢問是否替換
        full_path = os.path.join(dir_path, f"{base_name}{ext}")
        if os.path.exists(full_path):
            choice = input(f"文件 {full_path} 已存在。是否要替換？(y/n): ").strip().lower()
            if choice == 'y':
                # 直接覆蓋
                pass
            else:
                # 產生流水號版本
                counter = 1
                while os.path.exists(full_path):
                    counter += 1
                    full_path = os.path.join(dir_path, f"{base_name}_{counter}{ext}")
        # 匯出到該路徑
        if os.path.exists(self.json_file):
            # 備份現有 JSON 到 BBOX_BACKUP.json
            backup_file = os.path.join(os.path.dirname(self.json_file), 'BBOX_BACKUP.json')
            try:
                with open(self.json_file, 'r') as f:
                    existing_data = json.load(f)
                with open(backup_file, 'w') as f:
                    json.dump(existing_data, f, indent=4)
                print(f"已備份現有 JSON 到 {backup_file}")
            except json.JSONDecodeError:
                print("現有 JSON 文件格式錯誤，跳過備份。")
        
        print("正在從 C 文件生成內部 JSON...")
        if not self.generate_json_from_c():
            print("無法生成內部 JSON 文件。")
            return
        
        try:
            with open(self.json_file, 'r') as f:
                data = json.load(f)
            with open(full_path, 'w') as f:
                json.dump(data, f, indent=4)
            print(f"已匯出 Table 到 {full_path}")
        except json.JSONDecodeError:
            print("內部 JSON 文件格式錯誤。")

    def execute_modbus_master(self):
        print("\n=== ModbusMaster 執行 ===")
        print("請輸入 ModbusMaster 參數：")

        # 獲取 COM 端口
        port = input("COM 端口 (例如 COM1): ").strip()
        if not port:
            print("取消執行。")
            return

        # 獲取波特率
        baudrate = input("波特率 (預設 9600): ").strip()
        if not baudrate:
            baudrate = "9600"

        # 獲取 Slave ID
        slave_id = input("Slave ID: ").strip()
        if not slave_id:
            print("取消執行。")
            return

        # 獲取功能碼
        print("功能碼:")
        print("3. 讀保持寄存器 (Read Holding Registers)")
        print("6. 寫單個寄存器 (Write Single Register)")
        print("16. 寫多個寄存器 (Write Multiple Registers)")
        function = input("選擇功能碼 (3/6/16): ").strip()
        if function not in ['3', '6', '16']:
            print("無效功能碼。")
            return

        # 獲取地址
        address = input("起始地址: ").strip()
        if not address:
            print("取消執行。")
            return

        # 根據功能碼獲取額外參數
        cmd_args = ['python', 'ModbusMaster.py', '--port', port, '--baudrate', baudrate, '--slave', slave_id, '--function', function, '--address', address]

        if function == '3':
            quantity = input("讀取數量: ").strip()
            if not quantity:
                print("取消執行。")
                return
            cmd_args.extend(['--quantity', quantity])
        elif function == '6':
            value = input("寫入值: ").strip()
            if not value:
                print("取消執行。")
                return
            cmd_args.extend(['--value', value])
        elif function == '16':
            values_str = input("寫入值 (用空格分隔): ").strip()
            if not values_str:
                print("取消執行。")
                return
            values = values_str.split()
            cmd_args.extend(['--values'] + values)

        # 執行 ModbusMaster.py
        try:
            print(f"\n執行命令: {' '.join(cmd_args)}")
            result = subprocess.run(cmd_args, capture_output=True, text=True, cwd=os.path.dirname(__file__))
            print("輸出:")
            print(result.stdout)
            if result.stderr:
                print("錯誤:")
                print(result.stderr)
            print(f"返回碼: {result.returncode}")
        except FileNotFoundError:
            print("錯誤: 找不到 ModbusMaster.py 或 Python 解釋器")
        except Exception as e:
            print(f"執行錯誤: {e}")

    def generate_json_from_c(self):
        if os.path.exists(self.c_file):
            with open(self.c_file, 'r') as f:
                content = f.read()
            # 找到 bboxTableTemplate 的定義
            match = re.search(r'(const\s+ST_BLACKBOX\s+bboxTableTemplate\[\]\s*=\s*\{)([^}]+)(\};)', content, re.DOTALL)
            if match:
                table_content = match.group(2)
                # 提取 BBOX_VAR
                existing_vars = re.findall(r'BBOX_VAR\(([^)]+)\)', table_content)
                if existing_vars:
                    data = {}
                    for var in existing_vars:
                        if 'u32' in var or 's32' in var or 'f32' in var:
                            size = 4
                        elif 'u16' in var or 's16' in var:
                            size = 2
                        else:
                            # 沒有命名，提示輸入長度
                            length_input = input(f"請輸入變數 '{var}' 的長度 (BYTE): ").strip()
                            if length_input == '':
                                if self.last_length is not None:
                                    size = self.last_length
                                else:
                                    print("無上次長度，使用預設長度 4")
                                    size = 4
                                    self.last_length = 4
                            elif length_input.isdigit():
                                size = int(length_input)
                                self.last_length = size
                            else:
                                print("無效輸入，使用上次長度或預設長度")
                                if self.last_length is not None:
                                    size = self.last_length
                                else:
                                    size = 4
                                    self.last_length = 4
                        data[var] = size
                    with open(self.json_file, 'w') as f:
                        json.dump(data, f, indent=4)
                    print(f"已從 C 文件生成內部 JSON: {self.json_file}")
                    return True
                else:
                    print("C 文件中沒有找到 BBOX_VAR。")
                    return False
            else:
                print("無法解析 C 文件中的 bboxTableTemplate。")
                return False
        else:
            print("C 文件不存在。")
            return False

if __name__ == "__main__":
    fsm = TableManagerFSM()
    fsm.run()