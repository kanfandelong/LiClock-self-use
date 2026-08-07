#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32 堆栈回溯解析工具
用法：python3 stack_trace.py <elf_file> "<backtrace_string>" [--addr2line PATH]
示例：python3 stack_trace.py firmware.elf "0x420b1785:0x3fcec270 0x420b17cd:0x3fcec300 ..."
"""

import sys
import argparse
import subprocess
import re
import os

def find_addr2line():
    """尝试自动寻找 addr2line 工具"""
    # 常见的工具链路径
    candidates = [
        os.path.expanduser("~/xtensa-esp-elf/bin/xtensa-esp32-elf-addr2line"),
        os.path.expanduser("~/xtensa-esp-elf-gdb/bin/xtensa-esp32-elf-addr2line"),  # gdb 包里可能没有，但保留
        "/home/kanfandelong/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-addr2line"  # 如果已经在 PATH 中
    ]
    for path in candidates:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    return None

def main():
    parser = argparse.ArgumentParser(description="ESP32 堆栈回溯解析工具")
    parser.add_argument("elf", help="固件 ELF 文件路径")
    parser.add_argument("backtrace", help="Backtrace 字符串，格式：0xPC:0xSP ...")
    parser.add_argument("--addr2line", default=None,
                        help="addr2line 工具的路径（默认自动查找）")
    args = parser.parse_args()

    # 确定 addr2line 路径
    addr2line = args.addr2line or find_addr2line()
    if not addr2line:
        print("错误：找不到 xtensa-esp32-elf-addr2line，请用 --addr2line 指定")
        sys.exit(1)
    if not os.path.isfile(addr2line):
        print(f"错误：addr2line 工具不存在: {addr2line}")
        sys.exit(1)

    # 解析 Backtrace 字符串
    # 匹配模式：0xHEX:0xHEX，只取第一个地址（PC）
    pattern = re.compile(r"0x([0-9a-fA-F]+):0x[0-9a-fA-F]+")
    addresses = pattern.findall(args.backtrace)
    if not addresses:
        print("错误：未能从输入中解析出任何地址，请检查格式")
        sys.exit(1)

    print(f"使用 addr2line: {addr2line}")
    print(f"ELF 文件: {args.elf}")
    print("=" * 60)

    for i, addr in enumerate(addresses):
        try:
            # 调用 addr2line，-e 指定 ELF，-f -C 显示函数名并 demangle
            proc = subprocess.run(
                [addr2line, "-e", args.elf, "-f", "-C", f"0x{addr}"],
                capture_output=True, text=True, check=True
            )
            # 输出格式：第一行函数名，第二行源文件和行号
            lines = proc.stdout.strip().splitlines()
            func = lines[0] if len(lines) > 0 else "??"
            location = lines[1] if len(lines) > 1 else "??:0"
            print(f"#{i} PC=0x{addr} -> {func} at {location}")
        except subprocess.CalledProcessError as e:
            print(f"#{i} PC=0x{addr} -> addr2line 执行失败: {e}")

if __name__ == "__main__":
    main()