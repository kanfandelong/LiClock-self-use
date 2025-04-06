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
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from threading import Thread
from esp_coredump import CoreDump
from esp_coredump.corefile import ESPCoreDumpLoaderError

class CoreDumpAnalyzerApp:
    def __init__(self, master):
        self.master = master
        master.title("ESP32 核心转储分析工具 v1.1")  # 更新版本号
        master.geometry("900x700")  # 更合理的初始尺寸
        master.minsize(800, 600)  # 设置最小尺寸
        
        # 样式配置
        self.style = ttk.Style()
        self.style.configure("TButton", padding=6)
        self.style.configure("TLabel", padding=6)
        self.style.configure("TCombobox", padding=6)
        self.ip_address = tk.StringVar()
        self.default_coredump_path = "/System/coredump.elf"
        self.elf_path = ""
        self.core_path = ""
        self.log_buffer = []
        self.chip_var = tk.StringVar(value="自动检测")
        
        # 创建主界面布局
        self.create_widgets()
        
    def create_widgets(self):
        """创建界面组件"""
        # 使用网格布局管理器提升布局灵活性
        main_frame = ttk.Frame(self.master)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # 设备连接区域
        connection_frame = ttk.LabelFrame(main_frame, text="设备连接")
        connection_frame.grid(row=0, column=0, columnspan=2, sticky=tk.EW, pady=5)
    
        # IP地址和下载按钮
        ttk.Label(connection_frame, text="IP地址:").grid(row=0, column=0, padx=(5,2))
        self.ip_entry = ttk.Entry(connection_frame, textvariable=self.ip_address, width=18)
        self.ip_entry.grid(row=0, column=1, padx=2)
        ttk.Button(connection_frame, text="下载转储", 
                command=self.start_download).grid(row=0, column=2, padx=5)

        # 文件选择区域
        file_frame = ttk.LabelFrame(main_frame, text="文件配置")
        file_frame.grid(row=1, column=0, columnspan=2, sticky=tk.EW, pady=5)

        # ELF 文件选择（使用两列布局）
        ttk.Button(file_frame, text="选择 ELF", 
                command=self.select_elf_file).grid(row=0, column=0, padx=5, sticky=tk.W)
        self.elf_label = ttk.Label(file_frame, text="未选择文件", foreground="#666")
        self.elf_label.grid(row=0, column=1, padx=5, sticky=tk.EW)

        # 核心转储文件选择
        ttk.Button(file_frame, text="选择转储", 
                command=self.select_core_file).grid(row=1, column=0, padx=5, sticky=tk.W)
        self.core_label = ttk.Label(file_frame, text="未选择文件", foreground="#666")
        self.core_label.grid(row=1, column=1, padx=5, sticky=tk.EW)

        # 芯片选择区域
        chip_frame = ttk.Frame(main_frame)
        chip_frame.grid(row=2, column=0, sticky=tk.W, pady=5)
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
        btn_frame.grid(row=2, column=1, sticky=tk.E, pady=5)
        ttk.Button(btn_frame, text="开始解析", 
                command=self.start_analysis).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="保存报告", 
                command=self.save_report).pack(side=tk.LEFT)

        # 结果显示区域
        result_frame = ttk.LabelFrame(main_frame, text="分析结果")
        result_frame.grid(row=3, column=0, columnspan=2, sticky=tk.NSEW, pady=5)
        main_frame.rowconfigure(3, weight=1)  # 结果区域可扩展

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
        log_frame.grid(row=4, column=0, columnspan=2, sticky=tk.EW, pady=5)
    
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
        self.progress.grid(row=5, column=0, columnspan=2, sticky=tk.EW, pady=5)

        # 配置列权重
        main_frame.columnconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        file_frame.columnconfigure(1, weight=1)  # 文件路径标签可扩展
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
    
    def start_analysis(self):
        """启动分析线程"""
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
        
            coredump = CoreDump(
                prog=self.elf_path,
                core=self.core_path,
                core_format="raw",
                chip=chip,
                port=None,
                baud=0,
                gdb=r"C:\Users\admin\.platformio\packages\toolchain-xtensa-esp32\bin\xtensa-esp32-elf-gdb.exe"
            )
            #C:\Users\admin\.platformio\packages\toolchain-xtensa-esp32\bin\xtensa-esp32-elf-gdb.exe
            #E:\ESP-IDF\v5.2\Espressif\tools\xtensa-esp-elf-gdb\14.2_20240403\xtensa-esp-elf-gdb\bin\xtensa-esp32-elf-gdb.exe

            # 创建 StringIO 对象以捕获 print 输出
            output = io.StringIO()
            sys.stdout = output
        
            # 执行 info_corefile 方法
            temp_core_files = coredump.info_corefile()
        
            # 恢复标准输出
            sys.stdout = sys.__stdout__
        
            # 获取捕获的输出
            captured_output = output.getvalue()
            output.close()
        
            self.display_result(captured_output)
            self.progress.stop()
             # 使用 parse_raw_report 解析捕获的输出
            exception, pc_addr, backtrace = self.parse_raw_report(captured_output)
    
            # 显示解析结果到 GUI 文本框
            result_text = f"Exception: {exception}\nPC Address: {pc_addr}\nBacktrace:\n" + "\n".join(backtrace)
            # self.display_result(result_text)
        
        except ESPCoreDumpLoaderError as e:
            self.show_error(f"elf文件与核心转储不匹配: {str(e)}")
        except Exception as e:
            self.show_error(f"An error occurred: {str(e)}")
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
