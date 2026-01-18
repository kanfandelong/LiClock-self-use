import os
import sys
import time
import shutil
import hashlib
import json
import subprocess
from datetime import datetime
from pathlib import Path
from SCons.Script import Import

Import("env")

print("[钩子脚本] extra_script.py 已加载")

# 其他配置
BUILD_DIR = "./.pio/build/esp32solo1"
ELF_STORAGE_DIR = "./elf_versions"
DATABASE_FILE = "./elf_versions/elf_database.json"
MAX_VERSIONS = 200
# ===================


# 修复 hal.cpp 中的 Windows 路径问题
def fix_windows_paths():
    """修复源码中的 Windows 反斜杠路径"""
    print(f"\n{'='*60}")
    print("[路径修复] 检查并修复Windows路径")
    
    files_to_check = [
        {
            "path": os.path.join(env.subst("$PROJECT_SRC_DIR"), "hal.cpp"),
            "patterns": [('esp32\\rom\\sha.h', 'esp32/rom/sha.h')]
        },
        # 可以添加更多需要修复的文件
    ]
    
    total_fixes = 0
    for file_info in files_to_check:
        file_path = file_info["path"]
        
        if os.path.exists(file_path):
            print(f"🛠️  检查: {os.path.basename(file_path)}")
            
            try:
                # 读取文件内容
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                original_content = content
                
                # 应用所有修复模式
                for old_pattern, new_pattern in file_info["patterns"]:
                    if old_pattern in content:
                        count = content.count(old_pattern)
                        content = content.replace(old_pattern, new_pattern)
                        total_fixes += count
                        print(f"   修复 {count} 处: {old_pattern} -> {new_pattern}")
                
                # 如果有修改，写回文件
                if content != original_content:
                    with open(file_path, 'w', encoding='utf-8') as f:
                        f.write(content)
                else:
                    print("   ⏭️  无需修复")
                        
            except Exception as e:
                print(f"   ❌ 修复失败: {e}")
        else:
            print(f"   ⚠️  文件不存在: {os.path.basename(file_path)}")
    
    print(f"✅ 路径修复完成，共修复 {total_fixes} 处")
    return total_fixes > 0

# ===== 主执行逻辑 =====
print(f"\n{'='*60}")
print("🔧 LiClock 开发环境同步工具")
print(f"{'='*60}")

fix_windows_paths()

# ===== ELF版本管理功能 =====
def calculate_sha256(filepath):
    """计算文件的SHA256哈希值"""
    sha256_hash = hashlib.sha256()
    with open(filepath, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

def post_build_elf_versions():
    """PlatformIO构建后处理函数 - 归档ELF/BIN文件并记录元数据"""
    print(f"\n{'='*60}")
    print("[版本管理] 归档固件版本")
    
    # 构建完整路径
    elf_path = os.path.join(BUILD_DIR, "firmware.elf")
    
    # 检查ELF文件是否存在
    if not os.path.exists(elf_path):
        print(f"⚠️  警告: 找不到ELF文件: {elf_path}")
        return False
    
    # 创建输出目录
    os.makedirs(ELF_STORAGE_DIR, exist_ok=True)
    
    # 获取构建信息
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    build_time = datetime.now().isoformat()
    
    # 计算ELF文件哈希
    print("🔍 计算文件哈希...")
    elf_sha256 = calculate_sha256(elf_path)
    elf_sha256_short = elf_sha256[:16]
    
    # 归档ELF文件
    elf_dest_name = f"firmware_{timestamp}.elf"
    elf_dest_path = os.path.join(ELF_STORAGE_DIR, elf_dest_name)
    shutil.copy2(elf_path, elf_dest_path)
    
    # 准备元数据
    metadata = {
        "timestamp": timestamp,
        "build_time": build_time,
        "elf": {
            "filename": elf_dest_name,
            "size": os.path.getsize(elf_path),
            "sha256_full": elf_sha256,
            "sha256_short": elf_sha256_short,
            "path": elf_dest_path
        }
    }
    
    # 检查并归档BIN文件
    bin_path = os.path.join(BUILD_DIR, "firmware.bin")
    if os.path.exists(bin_path):
        bin_dest_name = f"firmware_{timestamp}.bin"
        bin_dest_path = os.path.join(ELF_STORAGE_DIR, bin_dest_name)
        shutil.copy2(bin_path, bin_dest_path)
        
        metadata["bin"] = {
            "filename": bin_dest_name,
            "size": os.path.getsize(bin_path),
            "path": bin_dest_path
        }
    
    # 更新数据库
    try:
        if os.path.exists(DATABASE_FILE):
            with open(DATABASE_FILE, 'r') as f:
                database = json.load(f)
        else:
            database = {"versions": [], "last_updated": None}
        
        # 检查是否已存在相同SHA256的版本
        existing = [v for v in database["versions"] 
                   if v["elf"]["sha256_short"] == elf_sha256_short]
        
        is_new = len(existing) == 0
        if is_new:
            database["versions"].append(metadata)
            database["last_updated"] = build_time
            
            # 限制版本数量
            if len(database["versions"]) > MAX_VERSIONS:
                database["versions"].sort(key=lambda x: x["timestamp"], reverse=True)
                database["versions"] = database["versions"][:MAX_VERSIONS]
            
            # 确保数据库目录存在
            os.makedirs(os.path.dirname(DATABASE_FILE), exist_ok=True)
            
            # 保存数据库
            with open(DATABASE_FILE, 'w') as f:
                json.dump(database, f, indent=2, ensure_ascii=False)
        
        # 输出结果
        if is_new:
            print(f"✅ 固件归档成功:")
            print(f"   ELF: {metadata['elf']['filename']}")
            print(f"   SHA256: {metadata['elf']['sha256_short']}")
            print(f"   大小: {metadata['elf']['size']:,} bytes")
            if "bin" in metadata:
                print(f"   BIN: {metadata['bin']['filename']} ({metadata['bin']['size']:,} bytes)")
            print(f"   时间: {timestamp}")
            print(f"   存储位置: {ELF_STORAGE_DIR}")
            
            # 显示版本总数
            print(f"   版本库中共有 {len(database['versions'])} 个版本")
        else:
            print(f"⚠️  版本已存在: {elf_sha256_short}")
            print(f"   跳过归档 (已有 {len(database['versions'])} 个版本)")
        
        return is_new
        
    except Exception as e:
        print(f"❌ 数据库更新失败: {e}")
        import traceback
        traceback.print_exc()
        return False

# ===== 构建后复制产物 =====
def copy_artifacts_to_windows(source, target, env):
    """构建完成后复制产物"""
    # print(f"\n{'='*60}")
    # print("[构建后] 复制产物到Windows")
    
    # build_dir = env.subst("$BUILD_DIR")
    # windows_build_dir = os.path.join(WINDOWS_BASE_DIR, "build")
    
    # print(f"  从: {build_dir}")
    # print(f"  到: {windows_build_dir}")
    
    # os.makedirs(windows_build_dir, exist_ok=True)
    
    # files_to_copy = [
    #     ("firmware.bin", "主固件"),
    #     ("bootloader.bin", "引导程序"), 
    #     ("partitions.bin", "分区表"),
    #     ("firmware.elf", "ELF文件"),
    #     ("firmware.map", "MAP文件")
    # ]
    
    # copied = 0
    # for filename, desc in files_to_copy:
    #     src = os.path.join(build_dir, filename)
    #     if os.path.exists(src):
    #         try:
    #             shutil.copy2(src, windows_build_dir)
    #             print(f"  ✅ {desc}")
    #             copied += 1
    #         except Exception as e:
    #             print(f"  ❌ {desc}失败: {e}")
    
    # print(f"✅ 共复制 {copied}/{len(files_to_copy)} 个文件")
    
    # 执行版本管理
    post_build_elf_versions()

# 注册构建后钩子
env.AddPostAction("buildprog", copy_artifacts_to_windows)