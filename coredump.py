#!/usr/bin/env python
#E:\ESP-IDF\v5.2\Espressif\tools\xtensa-esp-elf-gdb\14.2_20240403\xtensa-esp-elf-gdb\bin\xtensa-esp32-elf-gdb.exe
# -*- coding: utf-8 -*-
"""
ESP32 核心转储解析脚本
用法：python parse_coredump.py [参数]
"""

#!/usr/bin/env python
# -*- coding: utf-8 -*-
import io
import sys
import time
import os.path
import json
import re
import struct
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from threading import Thread
from esp_coredump import CoreDump
from esp_coredump.corefile import ESPCoreDumpLoaderError

# 默认GDB路径
DEFAULT_GDB_PATHS = [
    r"E:\ESP-IDF\v5.2\Espressif\tools\xtensa-esp-elf-gdb\14.2_20240403\xtensa-esp-elf-gdb\bin\xtensa-esp32-elf-gdb.exe",
    r"C:\Users\admin\.platformio\packages\toolchain-xtensa-esp32\bin\xtensa-esp32-elf-gdb.exe",
    r"xtensa-esp32-elf-gdb.exe"  # 系统PATH中的GDB
]

class CoreDumpAnalyzerApp:
    def __init__(self, master):
        self.master = master
        master.title("ESP32 核心转储分析工具 v1.2")  # 更新版本号，添加自动匹配功能
        master.geometry("900x700")  # 更合理的初始尺寸
        master.minsize(800, 600)  # 设置最小尺寸
        
        # 样式配置
        self.style = ttk.Style()
        self.style.configure("TButton", padding=6)
        self.style.configure("TLabel", padding=6)
        self.style.configure("TCombobox", padding=6)
        self.ip_address = tk.StringVar()
        self.default_coredump_path = "/System/coredump.elf"
        self.elf_path = "C:/Users/admin/Desktop/LiClock-dev_multithread-main/.pio/build/esp32solo1/firmware.elf"
        self.core_path = "C:/Users/admin/Desktop/LiClock-dev_multithread-main/tools/coredump.elf"
        self.log_buffer = []
        self.chip_var = tk.StringVar(value="自动检测")
        
        # GDB路径配置
        self.gdb_path = self.find_valid_gdb_path()
        
        # 创建主界面布局
        self.create_widgets()
        
        # ELF数据库
        self.elf_database = None
        self.load_elf_database()
        
        # 检查GDB路径有效性
        self.check_gdb_path_on_startup()
    
    def load_elf_database(self):
        """加载ELF数据库"""
        try:
            database_path = os.path.join(os.path.dirname(__file__), "./elf_versions/elf_database.json")
            if os.path.exists(database_path):
                with open(database_path, 'r', encoding='utf-8') as f:
                    self.elf_database = json.load(f)
                self.log(f"已加载ELF数据库，包含 {len(self.elf_database.get('versions', []))} 个固件版本")
            else:
                self.log("未找到elf_database.json文件")
                self.elf_database = None
        except Exception as e:
            self.log(f"加载ELF数据库失败: {str(e)}")
            self.elf_database = None
    
    def extract_sha256_from_coredump(self, core_path):
        """从核心转储文件中提取SHA256值"""
        try:
            with open(core_path, 'rb') as f:
                data = f.read()
            
            # 搜索所有可能的标记位置
            marker = b'ESP_CORE_DUMP_INFO' + b'\x00\x45\x00\x01\x00\x00'
            marker_hex = marker.hex()
            data_hex = data.hex()
            
            positions = []
            pos = 0
            while True:
                pos = data_hex.find(marker_hex, pos)
                if pos == -1:
                    break
                positions.append(pos)
                pos += 1
            
            self.log(f"找到 {len(positions)} 个可能的标记位置")
            
            # 遍历所有位置，寻找有效的SHA256
            for i, target_pos in enumerate(positions):
                sha_start_pos = target_pos + len(marker_hex)
                
                if sha_start_pos + 64 <= len(data_hex):
                    full_sha256_hex = data_hex[sha_start_pos:sha_start_pos + 64]
                    
                    # 验证SHA256是否有效（全为十六进制字符）
                    if all(c in '0123456789abcdef' for c in full_sha256_hex):
                        # 将十六进制字符串转换为ASCII字符串
                        try:
                            # 十六进制字符串 -> 字节 -> ASCII字符串
                            full_sha256_bytes = bytes.fromhex(full_sha256_hex)
                            full_sha256_ascii = full_sha256_bytes.decode('ascii', errors='ignore')
                            
                            # 取前16个ASCII字符作为短SHA256
                            if len(full_sha256_ascii) >= 16:
                                sha256_short = full_sha256_ascii[0:16]
                                
                                # 验证转换是否正确
                                self.log(f"位置 {i}: 在偏移 {target_pos//2:#x}")
                                self.log(f"  十六进制SHA256: {full_sha256_hex}")
                                self.log(f"  ASCII SHA256: {full_sha256_ascii}")
                                self.log(f"  短SHA256: {sha256_short}")
                                
                                # 检查是否有其他连续的非零数据
                                sha_end_pos = sha_start_pos + 64
                                next_data = data_hex[sha_end_pos:sha_end_pos + 32]
                                
                                self.log(f"  后面数据: {next_data}")
                                
                                # 如果SHA256看起来有效（不是全0或全F），返回它
                                if (sha256_short != '0000000000000000' and 
                                    sha256_short != 'ffffffffffffffff' and
                                    all(c in '0123456789abcdef' for c in sha256_short)):
                                    
                                    # 验证这个位置是否在一个合理的区域
                                    prev_data = data_hex[target_pos-16:target_pos]
                                    self.log(f"  前面数据: {prev_data}")
                                    self.log(f"  选中此位置作为候选")
                                    return sha256_short
                        except Exception as e:
                            self.log(f"  转换失败: {str(e)}")
                            continue
            
            # 如果以上方法都没找到，尝试直接从特定偏移读取
            self.log("使用简化搜索方法...")
            
            # 尝试从已知偏移读取（根据你的描述，SHA256在0x4EF0附近）
            try:
                # 0x4EF0是文件偏移，转换为十进制
                offset_hex = 0x4EF0
                # 跳过版本信息，SHA256从版本信息后开始
                sha_offset = offset_hex + 6  # 跳过版本信息00 45 00 01 00 00
                
                if sha_offset + 32 <= len(data):
                    sha_bytes = data[sha_offset:sha_offset+32]
                    # 尝试直接解码为ASCII
                    try:
                        sha256_ascii = sha_bytes.decode('ascii', errors='ignore')
                        if len(sha256_ascii) >= 32 and all(c in '0123456789abcdef' for c in sha256_ascii):
                            sha256_short = sha256_ascii[0:16]
                            self.log(f"从固定偏移 {sha_offset:#x} 提取SHA256: {sha256_short}")
                            return sha256_short
                    except:
                        pass
                    
                    # 如果直接解码失败，尝试十六进制转换
                    sha256_hex = sha_bytes.hex()
                    if len(sha256_hex) >= 64:
                        # 十六进制字符串 -> 字节 -> ASCII字符串
                        sha256_bytes = bytes.fromhex(sha256_hex[0:64])
                        sha256_ascii = sha256_bytes.decode('ascii', errors='ignore')
                        if len(sha256_ascii) >= 16:
                            sha256_short = sha256_ascii[0:16]
                            self.log(f"从固定偏移 {sha_offset:#x} 通过十六进制转换提取SHA256: {sha256_short}")
                            return sha256_short
            except Exception as e:
                self.log(f"固定偏移提取失败: {str(e)}")
            
            self.log("在核心转储中未找到有效的SHA256值")
            return None
            
        except Exception as e:
            self.log(f"提取SHA256失败: {str(e)}")
            import traceback
            self.log(f"错误详情: {traceback.format_exc()}")
            return None
        
    def find_elf_by_sha256(self, sha256_short):
        """根据SHA256值在数据库中查找对应的ELF文件"""
        if not self.elf_database or 'versions' not in self.elf_database:
            self.log("ELF数据库未加载或为空")
            return None
        
        # 遍历数据库中的版本
        for version in self.elf_database['versions']:
            elf_info = version.get('elf', {})
            db_sha256_short = elf_info.get('sha256_short', '')
            
            if db_sha256_short and db_sha256_short.lower() == sha256_short.lower():
                elf_path = elf_info.get('path', '')
                
                # 如果路径是相对的，转换为绝对路径
                if elf_path and not os.path.isabs(elf_path):
                    db_dir = os.path.dirname(os.path.join(os.path.dirname(__file__), "elf_database.json"))
                    elf_path = os.path.join(db_dir, elf_path)
                    elf_path = os.path.normpath(elf_path)
                
                if os.path.exists(elf_path):
                    self.log(f"找到匹配的ELF文件: {os.path.basename(elf_path)}")
                    return elf_path
                else:
                    self.log(f"数据库中的ELF文件不存在: {elf_path}")
        
        self.log(f"未找到SHA256为 {sha256_short} 的ELF文件")
        return None
    
    def auto_match_elf_from_coredump(self):
        """从核心转储自动匹配ELF文件"""
        if not self.core_path or not os.path.exists(self.core_path):
            self.log("未选择核心转储文件")
            return False
        
        if not self.elf_database:
            self.log("ELF数据库未加载")
            return False
        
        self.log("正在从核心转储提取SHA256并匹配ELF文件...")
        
        # 提取SHA256
        sha256_short = self.extract_sha256_from_coredump(self.core_path)
        
        if not sha256_short:
            self.log("无法从核心转储提取SHA256")
            return False
        
        # 查找匹配的ELF
        matched_elf_path = self.find_elf_by_sha256(sha256_short)
        
        if matched_elf_path:
            self.elf_path = matched_elf_path
            self.elf_label.config(text=matched_elf_path)
            self.log(f"已自动匹配ELF文件: {matched_elf_path}")
            
            # 显示成功消息
            messagebox.showinfo("自动匹配成功", 
                              f"已自动匹配到对应的ELF文件:\n{os.path.basename(matched_elf_path)}")
            return True
        else:
            self.log("未找到匹配的ELF文件，请手动选择")
            return False
    
    def find_valid_gdb_path(self):
        """查找有效的GDB路径"""
        for path in DEFAULT_GDB_PATHS:
            if self.is_gdb_path_valid(path):
                return path
        return DEFAULT_GDB_PATHS[0]  # 返回第一个路径作为默认值
    
    def is_gdb_path_valid(self, gdb_path):
        """检查GDB路径是否有效"""
        # 检查文件是否存在
        if not os.path.isfile(gdb_path):
            return False
        
        # 检查文件是否可执行（在Windows上主要是检查文件存在性）
        if not os.access(gdb_path, os.X_OK):
            return False
            
        # 检查文件扩展名（Windows）
        if os.name == 'nt' and not gdb_path.lower().endswith(('.exe', '.bat', '.cmd')):
            return False
            
        return True
    
    def check_gdb_path_on_startup(self):
        """启动时检查GDB路径"""
        if not self.is_gdb_path_valid(self.gdb_path):
            self.show_gdb_path_warning()
    
    def show_gdb_path_warning(self):
        """显示GDB路径无效的警告"""
        warning_msg = (
            f"GDB路径无效或找不到:\n{self.gdb_path}\n\n"
            "请选择正确的GDB可执行文件路径。\n"
            "GDB通常位于:\n"
            "- PlatformIO工具链目录\n"
            "- ESP-IDF工具链目录\n"
            "- 系统PATH环境变量中"
        )
        
        result = messagebox.showwarning(
            "GDB路径无效",
            warning_msg,
            icon=messagebox.WARNING,
            type=messagebox.OKCANCEL
        )
        
        if result == "ok":
            self.select_gdb_path()
    
    def select_gdb_path(self):
        """让用户选择GDB路径"""
        gdb_path = filedialog.askopenfilename(
            title="选择GDB可执行文件",
            filetypes=[
                ("GDB executable", "*.exe"),
                ("All files", "*.*")
            ]
        )
        
        if gdb_path and self.is_gdb_path_valid(gdb_path):
            self.gdb_path = gdb_path
            self.log(f"已更新GDB路径: {gdb_path}")
            messagebox.showinfo("成功", f"GDB路径已更新为:\n{gdb_path}")
        elif gdb_path:
            messagebox.showerror("错误", "选择的文件不是有效的GDB可执行文件")
            self.select_gdb_path()  # 递归调用直到选择有效文件或取消
    
    def verify_gdb_before_analysis(self):
        """在分析前验证GDB路径"""
        if not self.is_gdb_path_valid(self.gdb_path):
            self.log("GDB路径无效，请重新选择")
            self.select_gdb_path()
            return False
        
        # 额外检查：尝试运行GDB的版本命令（可选，可能较慢）
        try:
            import subprocess
            result = subprocess.run(
                [self.gdb_path, "--version"],
                capture_output=True,
                text=True,
                timeout=5
            )
            if result.returncode == 0:
                self.log(f"GDB验证成功: {result.stdout.splitlines()[0]}")
                return True
            else:
                self.log(f"GDB验证失败: {result.stderr}")
                return False
        except Exception as e:
            self.log(f"GDB验证异常: {str(e)}")
            # 即使验证异常，只要文件存在且可执行，仍然继续
            return self.is_gdb_path_valid(self.gdb_path)
    
    def create_widgets(self):
        """创建界面组件"""
        # 使用网格布局管理器提升布局灵活性
        main_frame = ttk.Frame(self.master)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # GDB路径显示区域（新增）
        gdb_frame = ttk.LabelFrame(main_frame, text="GDB配置")
        gdb_frame.grid(row=0, column=0, columnspan=2, sticky=tk.EW, pady=5)
        
        ttk.Label(gdb_frame, text="GDB路径:").grid(row=0, column=0, padx=(5,2), sticky=tk.W)
        gdb_path_label = ttk.Label(
            gdb_frame, 
            text=self.gdb_path,
            foreground="green" if self.is_gdb_path_valid(self.gdb_path) else "red",
            font=('Consolas', 9)
        )
        gdb_path_label.grid(row=0, column=1, padx=2, sticky=tk.EW)
        ttk.Button(gdb_frame, text="更改GDB", 
                  command=self.select_gdb_path).grid(row=0, column=2, padx=5)

        # 设备连接区域
        connection_frame = ttk.LabelFrame(main_frame, text="设备连接")
        connection_frame.grid(row=1, column=0, columnspan=2, sticky=tk.EW, pady=5)
    
        # IP地址和下载按钮
        ttk.Label(connection_frame, text="IP地址:").grid(row=0, column=0, padx=(5,2))
        self.ip_entry = ttk.Entry(connection_frame, textvariable=self.ip_address, width=18)
        self.ip_entry.grid(row=0, column=1, padx=2)
        ttk.Button(connection_frame, text="下载转储", 
                command=self.start_download).grid(row=0, column=2, padx=5)

        # 文件选择区域
        file_frame = ttk.LabelFrame(main_frame, text="文件配置")
        file_frame.grid(row=2, column=0, columnspan=2, sticky=tk.EW, pady=5)

        # ELF 文件选择（使用两列布局）
        ttk.Button(file_frame, text="选择 ELF", 
                command=self.select_elf_file).grid(row=0, column=0, padx=5, sticky=tk.W)
        self.elf_label = ttk.Label(file_frame, text="未选择文件", foreground="#666")
        self.elf_label.grid(row=0, column=1, padx=5, sticky=tk.EW)
        self.elf_label.config(text=self.elf_path)

        # 核心转储文件选择
        ttk.Button(file_frame, text="选择转储", 
                command=self.select_core_file).grid(row=1, column=0, padx=5, sticky=tk.W)
        self.core_label = ttk.Label(file_frame, text="未选择文件", foreground="#666")
        self.core_label.grid(row=1, column=1, padx=5, sticky=tk.EW)
        self.core_label.config(text=self.core_path)
        
        # 自动匹配按钮（新增）
        auto_match_frame = ttk.Frame(file_frame)
        auto_match_frame.grid(row=2, column=0, columnspan=2, pady=5, sticky=tk.W)
        ttk.Button(auto_match_frame, text="自动匹配ELF", 
                  command=self.auto_match_elf, width=15).pack(side=tk.LEFT, padx=5)
        ttk.Label(auto_match_frame, text="根据coredump自动匹配对应的固件ELF文件", 
                 foreground="blue", font=('Arial', 9)).pack(side=tk.LEFT)

        # 芯片选择区域
        chip_frame = ttk.Frame(main_frame)
        chip_frame.grid(row=3, column=0, sticky=tk.W, pady=5)
        ttk.Label(chip_frame, text="芯片类型:").pack(side=tk.LEFT, padx=(0,5))
        self.chip_combobox = ttk.Combobox(
            chip_frame, 
            textvariable=self.chip_var,
            values=["自动检测", "esp32", "esp32s3", "esp32c3", "esp32s2", "esp32c6", "esp32h2", "esp32p4", "esp32c5", "esp32c61"],
            state="readonly",
            width=12
        )
        self.chip_combobox.current(0)
        self.chip_combobox.pack(side=tk.LEFT)

        # 操作按钮区域
        btn_frame = ttk.Frame(main_frame)
        btn_frame.grid(row=3, column=1, sticky=tk.E, pady=5)
        ttk.Button(btn_frame, text="开始解析", 
                command=self.start_analysis).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="保存报告", 
                command=self.save_report).pack(side=tk.LEFT)

        # 结果显示区域
        result_frame = ttk.LabelFrame(main_frame, text="分析结果")
        result_frame.grid(row=4, column=0, columnspan=2, sticky=tk.NSEW, pady=5)
        main_frame.rowconfigure(4, weight=1)  # 结果区域可扩展

        self.result_text = tk.Text(
            result_frame, 
            wrap=tk.WORD,
            state=tk.DISABLED,
            font=('Consolas', 10),
            padx=5,
            pady=5
        )
        self.result_text.pack(fill=tk.BOTH, expand=True)

        # 日志区域
        log_frame = ttk.LabelFrame(main_frame, text="操作日志")
        log_frame.grid(row=5, column=0, columnspan=2, sticky=tk.EW, pady=5)
    
        self.log_text = tk.Text(
            log_frame, 
            height=4,
            wrap=tk.WORD,
            state=tk.DISABLED,
            font=('Consolas', 9),
            bg='#f0f0f0',
            padx=5,
            pady=5
        )
        self.log_text.pack(fill=tk.BOTH)

        # 进度条（移至底部）
        self.progress = ttk.Progressbar(
            main_frame, 
            orient=tk.HORIZONTAL,
            mode='indeterminate'
        )
        self.progress.grid(row=6, column=0, columnspan=2, sticky=tk.EW, pady=5)

        # 配置列权重
        main_frame.columnconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        file_frame.columnconfigure(1, weight=1)  # 文件路径标签可扩展
        gdb_frame.columnconfigure(1, weight=1)   # GDB路径标签可扩展

    def auto_match_elf(self):
        """自动匹配ELF按钮点击事件"""
        if not self.core_path:
            self.show_error("请先选择核心转储文件")
            return
        
        if not os.path.exists(self.core_path):
            self.show_error("核心转储文件不存在")
            return
        
        # 禁用按钮防止重复点击
        self.toggle_controls(False)
        self.progress.start()
        
        # 在后台线程执行自动匹配
        Thread(target=self.run_auto_match, daemon=True).start()
    
    def run_auto_match(self):
        """运行自动匹配"""
        try:
            success = self.auto_match_elf_from_coredump()
            if not success:
                messagebox.showwarning("自动匹配失败", 
                                      "无法自动匹配ELF文件，请手动选择。\n\n可能原因：\n1. 核心转储不包含SHA256信息\n2. ELF数据库中无匹配项\n3. ELF数据库未加载")
        except Exception as e:
            self.show_error(f"自动匹配失败: {str(e)}")
        finally:
            self.toggle_controls(True)
            self.progress.stop()

    # 新增方法：启动下载线程
    def start_download(self):
        """启动下载线程"""
        ip = self.ip_address.get().strip()
        if not ip:
            self.show_error("请输入设备IP地址")
            return
        
        # 禁用按钮防止重复点击
        self.toggle_controls(False)
        self.progress.start()
        
        # 在后台线程执行下载
        Thread(target=self.download_coredump, args=(ip,), daemon=True).start()

    # 下载核心转储文件
    def download_coredump(self, ip):
        """从指定IP下载核心转储文件"""
        import requests
        from urllib.parse import quote
        
        try:
            self.log(f"开始从 {ip} 下载核心转储...")
            
            # 构建下载URL（使用默认路径）
            remote_path = quote(self.default_coredump_path)
            url = f"http://{ip}/edit?download={remote_path}"
            
            # 设置超时并开始下载
            response = requests.get(url, stream=True, timeout=10)
            response.raise_for_status()
            
            # 自动生成本地文件名
            timestamp = time.strftime("%Y%m%d_%H%M%S")
            default_filename = f"coredump_{timestamp}.elf"
            
            # 弹出保存对话框
            download_path = filedialog.asksaveasfilename(
                title="保存核心转储文件",
                initialfile=default_filename,
                defaultextension=".elf",
                filetypes=[("Core files", "*.elf"), ("All files", "*.*")]
            )
            
            if not download_path:
                self.log("下载已取消")
                return
                
            # 保存文件并显示进度
            total_size = int(response.headers.get('content-length', 0))
            chunk_size = 8192
            downloaded = 0
            
            with open(download_path, 'wb') as f:
                for chunk in response.iter_content(chunk_size=chunk_size):
                    if chunk:
                        f.write(chunk)
                        downloaded += len(chunk)
                        progress = (downloaded / total_size) * 100 if total_size > 0 else 0
                        self.progress["value"] = progress
                        self.master.update_idletasks()
            
            # 更新界面
            self.core_path = download_path
            self.core_label.config(text=download_path)
            self.log(f"核心转储已成功下载: {download_path}")
            
            # 下载完成后尝试自动匹配
            self.log("正在尝试自动匹配ELF文件...")
            self.auto_match_elf_from_coredump()
            
        except requests.exceptions.Timeout:
            self.show_error("连接设备超时，请检查IP和网络")
        except requests.exceptions.ConnectionError:
            self.show_error("无法连接到设备，请检查IP和端口")
        except Exception as e:
            self.show_error(f"下载失败: {str(e)}")
        finally:
            self.toggle_controls(True)
            self.progress.stop()

    def select_elf_file(self):
        """选择 ELF 文件"""
        path = filedialog.askopenfilename(
            title="选择固件 ELF 文件",
            filetypes=[("ELF files", "*.elf"), ("All files", "*.*")]
        )
        if path:
            self.elf_path = path
            self.elf_label.config(text=path)
            self.log(f"已选择 ELF 文件: {path}")
    
    def select_core_file(self):
        """选择核心转储文件"""
        path = filedialog.askopenfilename(
            title="选择核心转储文件",
            filetypes=[("Core files", "*.bin *.elf"), ("All files", "*.*")]
        )
        if path:
            self.core_path = path
            self.core_label.config(text=path)
            self.log(f"已选择核心转储文件: {path}")
            
            # 选择后尝试自动匹配
            Thread(target=self.auto_match_elf_from_coredump, daemon=True).start()
    
    def start_analysis(self):
        """启动分析线程"""
        # 首先验证GDB路径
        if not self.verify_gdb_before_analysis():
            self.show_error("GDB路径无效，无法进行分析")
            return
            
        if not self.validate_inputs():
            return
        
        # 禁用按钮防止重复点击
        self.toggle_controls(False)
        self.progress.start()
        
        # 在后台线程执行解析
        Thread(target=self.run_analysis, daemon=True).start()
    
    def run_analysis(self):
        try:
            chip = self.chip_var.get() if self.chip_var.get() != "自动检测" else "auto"
        
            # 创建 StringIO 对象以捕获 print 输出
            output = io.StringIO()
            sys.stdout = output

            coredump = CoreDump(
                prog=self.elf_path,
                core=self.core_path,
                core_format="raw",
                chip=chip,
                port=None,
                baud=0,
                gdb=self.gdb_path  # 使用实例变量中的GDB路径
            )

            # 执行 info_corefile 方法
            temp_core_files = coredump.info_corefile()
        
            # 恢复标准输出
            sys.stdout = sys.__stdout__
        
            # 获取捕获的输出
            captured_output = output.getvalue()
            output.close()
        
            self.display_result(captured_output)
            print("核心转储分析完成。")
            print(captured_output)
            self.progress.stop()
            
            # 使用 parse_raw_report 解析捕获的输出
            exception, pc_addr, backtrace = self.parse_raw_report(captured_output)
    
            # 显示解析结果到 GUI 文本框
            result_text = f"Exception: {exception}\nPC Address: {pc_addr}\nBacktrace:\n" + "\n".join(backtrace)
            # self.display_result(result_text)
        
        except ESPCoreDumpLoaderError as e:
            self.show_error(f"elf文件与核心转储不匹配: {str(e)}")
        except FileNotFoundError as e:
            self.show_error(f"GDB可执行文件未找到: {self.gdb_path}\n请检查GDB路径配置")
        except Exception as e:
            self.show_error(f"分析过程中发生错误: {str(e)}")
        finally:
            self.toggle_controls(True)
            self.progress.stop()
            if 'temp_core_files' in locals() and temp_core_files:
                for f in temp_core_files:
                    try:
                        os.remove(f)
                    except OSError:
                        pass

    def parse_raw_report(self, raw_report):
        """解析字符串格式的核心转储报告"""
        import re
        # 匹配异常原因（exccause行和可能的assert信息）
        exception_pattern = re.compile(
            r"exccause\s+0x[\da-f]+ \((.*?)\)\s*\n"  # 匹配exccause描述
            r".*?assert failed: (.*?)\s",             # 匹配assert信息
            re.DOTALL
        )
    
        # 匹配PC地址（兼容不同格式）
        pc_pattern = re.compile(
            r"pc\s+(0x[0-9a-f]+)\s+", 
            re.IGNORECASE
        )
    
        # 增强的堆栈帧匹配（处理带参数的帧）
        backtrace_pattern = re.compile(
            r"#(\d+)\s+(0x[0-9a-f]+)\s+in\s+([^(]+?)(?:\(.*?\))?\s+"
            r"(?:at|from|@)\s+([/\w\.-]+):(\d+)",
            re.MULTILINE
        )
    
        # 初始化结果
        exception = "Unknown"
        pc_addr = "0x????????"
        backtrace = []
        assert_info = ""
    
        # 匹配异常原因（优先匹配assert信息）
        assert_match = re.search(r"assert failed: (.*?)\n", raw_report)
        if assert_match:
            assert_info = assert_match.group(1).strip()
        
        exccause_match = exception_pattern.search(raw_report)
        if exccause_match:
            exception = f"{exccause_match.group(1)}"
            if assert_info:
                exception += f" - {assert_info}"
    
        # 匹配PC地址
        pc_match = pc_pattern.search(raw_report)
        if pc_match:
            pc_addr = pc_match.group(1)
    
        # 匹配堆栈帧（增强格式兼容性）
        for match in backtrace_pattern.finditer(raw_report):
            frame_num = match.group(1)
            func_name = match.group(3).strip()
            file_path = match.group(4)
            line_num = match.group(5)
            backtrace.append(
                f"#{frame_num} {func_name} at {file_path}:{line_num}"
            )
    
        return exception, pc_addr, backtrace
    
    def validate_inputs(self):
        """验证输入文件是否有效"""
        errors = []
        if not self.elf_path:
            errors.append("请先选择 ELF 文件")
        if not self.core_path:
            errors.append("请先选择核心转储文件")
        
        if errors:
            self.show_error("\n".join(errors))
            return False
        return True
    
    def toggle_controls(self, enable=True):
        """切换控件状态"""
        state = tk.NORMAL if enable else tk.DISABLED
        for child in self.master.winfo_children():
            if isinstance(child, ttk.Button):
                child.config(state=state)
    
    def display_result(self, text):
        """显示分析结果"""
        self.result_text.config(state=tk.NORMAL)
        self.result_text.delete(1.0, tk.END)
        self.result_text.insert(tk.END, text)
        self.result_text.config(state=tk.DISABLED)
    
    def save_report(self):
        """保存分析报告"""
        if not self.result_text.get(1.0, tk.END).strip():
            self.show_error("没有可保存的内容")
            return
        
        path = filedialog.asksaveasfilename(
            defaultextension=".txt",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
        )
        if path:
            try:
                with open(path, "w") as f:
                    f.write(self.result_text.get(1.0, tk.END))
                self.log(f"报告已保存至: {path}")
            except Exception as e:
                self.show_error(f"保存失败: {str(e)}")
    
    def log(self, message):
        """记录日志"""
        timestamp = time.strftime("%H:%M:%S")
        formatted_msg = f"[{timestamp}] {message}"
        self.log_text.config(state=tk.NORMAL)
        self.log_text.insert(tk.END, formatted_msg + "\n")
        self.log_text.see(tk.END)
        self.log_text.config(state=tk.DISABLED)
    
    def show_error(self, message):
        """显示错误弹窗"""
        messagebox.showerror("错误", message)
        self.log(f"[错误] {message}")

if __name__ == "__main__":
    root = tk.Tk()
    app = CoreDumpAnalyzerApp(root)
    root.mainloop()