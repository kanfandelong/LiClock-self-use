#!/usr/bin/env python3
"""
差分烧录工具：根据当前设备中 app0 分区的固件头部 ELF SHA256，
从 elf_database.json 中匹配旧固件文件，然后使用 esptool 进行差分烧录。
用法：
    python diff_flash.py --port COM3
    python diff_flash.py --port COM3 --baud 921600
"""

import subprocess
import json
import os
import sys
import tempfile
import binascii
import argparse
from pathlib import Path

# ---------- 常量 ----------
APP0_ADDR = 0x10000          # app0 分区起始地址（根据你的分区表）
HEADER_SIZE = 0x100          # 读取头部 256 字节足够
SHA256_OFFSET = 0xB0         # esp_app_desc_t.app_elf_sha256 的偏移
SHORT_HASH_LEN = 8          # 使用前 16 字节作为短哈希

# 相对路径（基于脚本所在目录）
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
DB_PATH = PROJECT_ROOT / "elf_versions" / "elf_database.json"
NEW_FIRMWARE_PATH = PROJECT_ROOT / ".pio" / "build" / "MusicPlayer" / "firmware.bin"


def get_flash_header(port, baud):
    """
    通过 esptool 读取 app0 分区开头的 HEADER_SIZE 字节，返回原始字节
    """
    with tempfile.NamedTemporaryFile(delete=False, suffix=".bin") as tmp:
        tmp_path = tmp.name

    try:
        cmd = [
            "esptool",
            "--port", port,
            "--baud", str(baud),
            "read_flash",
            hex(APP0_ADDR),
            hex(HEADER_SIZE),
            tmp_path
        ]
        print(f"▶ 读取当前固件头部: {' '.join(cmd)}")
        # ✅ 移除 capture_output=True，让输出显示在控制台
        subprocess.run(cmd, check=True)

        with open(tmp_path, "rb") as f:
            data = f.read()
        return data
    finally:
        if os.path.exists(tmp_path):
            os.unlink(tmp_path)


def extract_short_hash(header_data):
    """从头部数据中提取短哈希（16字节十六进制字符串）"""
    if len(header_data) < SHA256_OFFSET + 32:
        raise ValueError("头部数据不足，无法提取 SHA256")

    sha256_full = header_data[SHA256_OFFSET:SHA256_OFFSET+32]
    short_hash = binascii.hexlify(sha256_full[:SHORT_HASH_LEN]).decode('ascii')
    return short_hash


def find_old_firmware(db_path, short_hash):
    """在 elf_database.json 中查找匹配短哈希的 bin 文件路径"""
    with open(db_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    for entry in data.get("versions", []):
        if entry.get("elf", {}).get("sha256_short") == short_hash:
            bin_path = entry.get("bin", {}).get("path")
            if bin_path:
                # 🔧 只取文件名，忽略任何目录前缀
                bin_filename = os.path.basename(bin_path)
                db_dir = Path(db_path).parent  # elf_versions 目录
                full_bin_path = db_dir / bin_filename
                if full_bin_path.exists():
                    return str(full_bin_path)
                else:
                    print(f"⚠️ 文件不存在: {full_bin_path}")
                    return None
            else:
                print(f"⚠️ 数据库中该版本的 bin.path 为 null")
                return None

    print(f"⚠️ 数据库中没有找到短哈希为 {short_hash} 的版本")
    return None


def flash_firmware(port, baud, old_bin_path, new_bin_path):
    cmd = ["esptool", "--port", port, "--baud", str(baud), "write-flash"]
    
    # 基础参数：地址 + 新固件
    cmd += [hex(APP0_ADDR), new_bin_path]
    
    # ✅ 将 --diff-with 放在最后，作为附加参数
    if old_bin_path and os.path.exists(old_bin_path):
        cmd += ["--diff-with", old_bin_path]
        print(f"✅ 使用差分烧录，基准文件: {old_bin_path}")
    else:
        print("ℹ️  执行全量烧录")
    
    print(f"▶ 执行烧录命令: {' '.join(cmd)}")
    subprocess.run(cmd, check=True)


def main():
    parser = argparse.ArgumentParser(description="差分烧录工具")
    parser.add_argument("--port", "-p", required=True, help="串口，例如 COM3")
    parser.add_argument("--baud", "-b", type=int, default=921600, help="波特率，默认 921600")
    args = parser.parse_args()

    # 检查新固件是否存在
    if not NEW_FIRMWARE_PATH.exists():
        print(f"❌ 新固件不存在: {NEW_FIRMWARE_PATH}")
        sys.exit(1)

    print("=== 差分烧录工具 ===")
    print(f"端口: {args.port}, 波特率: {args.baud}")
    print(f"新固件: {NEW_FIRMWARE_PATH}")

    # 1. 读取设备当前 app0 头部
    try:
        header = get_flash_header(args.port, args.baud)
    except subprocess.CalledProcessError as e:
        print(f"❌ 读取 Flash 头部失败: {e.stderr}")
        sys.exit(1)
    except Exception as e:
        print(f"❌ 错误: {e}")
        sys.exit(1)

    # 2. 提取短哈希
    try:
        short_hash = extract_short_hash(header)
        print(f"当前固件 ELF 短哈希: {short_hash}")
    except Exception as e:
        print(f"❌ 解析头部失败: {e}")
        sys.exit(1)

    # 3. 查询数据库
    if not DB_PATH.exists():
        print(f"⚠️ 数据库文件不存在: {DB_PATH}，将执行全量烧录")
        old_bin = None
    else:
        old_bin = find_old_firmware(DB_PATH, short_hash)

    # 4. 执行烧录
    try:
        flash_firmware(args.port, args.baud, old_bin, str(NEW_FIRMWARE_PATH))
        print("✅ 烧录完成")
    except subprocess.CalledProcessError as e:
        print(f"❌ 烧录失败: {e.stderr}")
        sys.exit(1)


if __name__ == "__main__":
    main()