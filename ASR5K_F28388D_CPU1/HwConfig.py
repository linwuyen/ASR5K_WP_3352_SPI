# -*- coding: utf-8 -*-
import os
import re
from datetime import datetime

class HwConfigUpdater:
    def __init__(self):
        self.file_path = "HwConfig.h"
        self.state = 'init'
        self.days_since_start = None
        self.formatted_date = None
        self.new_fw_version = None
        self.username = None
        self.changes = []
        self.new_lines = []

    def run(self):
        if not os.path.exists(self.file_path):
            print(f"檔案 {self.file_path} 不存在，請確認路徑正確！")
            return

        self.calculate_dates_and_version()
        self.input_username()
        self.input_changes()
        self.update_file()
        print(f"更新完成！新 BuildDate={self.days_since_start}，新 Version={self.new_fw_version}")

    def calculate_dates_and_version(self):
        start_date = datetime(2024, 1, 1)
        current_date = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
        self.days_since_start = (current_date - start_date).days
        self.formatted_date = current_date.strftime("%Y/%m/%d")

        fw_version_pattern = re.compile(r"#define FW_VERSION\s+(\d+)")
        with open(self.file_path, "r", encoding="utf-8") as file:
            for line in file:
                if fw_version_pattern.search(line):
                    current_fw_version = int(fw_version_pattern.search(line).group(1))
                    self.new_fw_version = current_fw_version + 1
                    break

    def input_username(self):
        while True:
            self.username = input("請輸入使用者名稱：").strip()
            if self.username:
                break
            print("使用者名稱不能空白，請重新輸入。")

    def input_changes(self):
        print("請條列式輸入修改更新紀錄（每行一個項目，連續兩次 ENTER 結束）：")
        empty_count = 0
        while empty_count < 2:
            change = input().strip()
            if change:
                self.changes.append(change)
                empty_count = 0
            else:
                empty_count += 1

    def update_file(self):
        fw_version_pattern = re.compile(r"#define FW_VERSION\s+(\d+)")
        with open(self.file_path, "r", encoding="utf-8") as file:
            for line in file:
                if "#define FW_BUILDDATE" in line:
                    self.new_lines.append(f"#define FW_BUILDDATE                     {self.days_since_start}               // Start from 2024/1/1\n")
                elif fw_version_pattern.search(line):
                    self.new_lines.append(f"#define FW_VERSION                       {self.new_fw_version}             // CPU1 Version Start from 10000\n")
                else:
                    self.new_lines.append(line)

        # 在 BuildDate_Version: Comment 下新增一行
        if self.new_fw_version:
            for idx, line in enumerate(self.new_lines):
                if "BuildDate_Version: Comment" in line:
                    comment_line = f" *      {self.days_since_start}_{self.new_fw_version}: Updated on {self.formatted_date} by {self.username}\n"
                    self.new_lines.insert(idx + 1, comment_line)
                    # 添加條列項目
                    if len(self.changes) >= 2:
                        for i, change in enumerate(self.changes, 1):
                            self.new_lines.insert(idx + 2, f" *                 {i}. {change}\n")
                            idx += 1
                    else:
                        for change in self.changes:
                            self.new_lines.insert(idx + 2, f" *                 {change}\n")
                            idx += 1
                    break

        # 寫回檔案
        with open(self.file_path, "w", encoding="utf-8") as file:
            file.writelines(self.new_lines)

if __name__ == "__main__":
    updater = HwConfigUpdater()
    updater.run()