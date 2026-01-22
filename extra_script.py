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

# ===== 配置区域 =====
WINDOWS_BASE_DIR = "/mnt/d/LiClock-dev_multithread-ST7305"
PROJECT_DIR = env.subst("$PROJECT_DIR")

# 需要同步的目录配置
SYNC_CONFIGS = [
    {
        "source": os.path.join(WINDOWS_BASE_DIR, "src"),
        "target": env.subst("$PROJECT_SRC_DIR"),
        "description": "源码目录"
    },
    {
        "source": os.path.join(WINDOWS_BASE_DIR, "lib"),
        "target": os.path.join(PROJECT_DIR, "lib"),
        "description": "库目录"
    },
    {
        "source": os.path.join(WINDOWS_BASE_DIR, "include"),
        "target": os.path.join(PROJECT_DIR, "include"),
        "description": "头文件目录"
    }
]

# 其他配置
BUILD_DIR = "./.pio/build/esp32solo1"
ELF_STORAGE_DIR = "./elf_versions"
DATABASE_FILE = "./elf_versions/elf_database.json"
MAX_VERSIONS = 200
# ===================

def check_rsync_available():
    """检查rsync是否可用"""
    try:
        result = subprocess.run(["rsync", "--version"], 
                              capture_output=True, 
                              text=True, 
                              check=False)
        return result.returncode == 0
    except Exception:
        return False

def sync_with_rsync(source_dir, target_dir, description=""):
    """使用rsync进行增量同步（更快）"""
    print(f"🔄 使用rsync同步{description}...")
    
    # rsync选项说明：
    # -a: 归档模式，保持文件属性
    # -v: 显示详细信息
    # -r: 递归复制
    # -u: 只更新较新的文件（增量同步）
    # --delete: 删除目标中源没有的文件
    # --exclude='.git': 排除.git目录
    rsync_cmd = [
        "rsync",
        "-avru",  # 归档、详细、递归、更新
        "--delete",
        "--exclude=.git",
        "--exclude=*.swp",
        "--exclude=*.o",
        "--exclude=*.d",
        "--exclude=build/",
        f"{source_dir}/",  # 注意末尾的/表示同步目录内容而不是目录本身
        target_dir
    ]
    
    try:
        print(f"  命令: {' '.join(rsync_cmd[:5])}...")  # 只显示部分命令，避免太长
        result = subprocess.run(rsync_cmd, 
                              capture_output=True, 
                              text=True, 
                              check=True)
        
        # 分析rsync输出，统计同步的文件数量
        output_lines = result.stdout.split('\n')
        files_changed = []
        for line in output_lines:
            if line and not line.startswith(' ') and '/' in line:
                files_changed.append(line)
        
        if files_changed:
            print(f"  📋 同步了 {len(files_changed)} 个文件:")
            for file in files_changed[:5]:  # 只显示前5个文件
                print(f"    - {file}")
            if len(files_changed) > 5:
                print(f"    ... 还有 {len(files_changed)-5} 个文件")
        else:
            print(f"  ⏭️  没有需要同步的文件，目录已是最新")
        
        return True, len(files_changed)
        
    except subprocess.CalledProcessError as e:
        print(f"❌ rsync同步失败: {e}")
        print(f"  错误输出: {e.stderr}")
        return False, 0
    except Exception as e:
        print(f"❌ 同步异常: {e}")
        return False, 0

def sync_with_shutil(source_dir, target_dir, description=""):
    """使用shutil进行完整同步（备用方案）"""
    print(f"🔄 使用shutil同步{description}...")
    
    try:
        # 如果目标目录存在，先删除
        if os.path.exists(target_dir):
            print("  清理旧目录...")
            shutil.rmtree(target_dir)
        
        # 复制整个目录
        print("  复制文件中...")
        shutil.copytree(source_dir, target_dir, 
                       ignore=shutil.ignore_patterns('.git', '*.swp', '*.o', '*.d', 'build'))
        
        # 统计文件数量
        sync_count = 0
        for root, dirs, files in os.walk(target_dir):
            sync_count += len(files)
        
        print(f"  完成，共 {sync_count} 个文件")
        return True, sync_count
        
    except Exception as e:
        print(f"❌ shutil同步失败: {e}")
        return False, 0

def sync_all_directories():
    """同步所有配置的目录"""
    print(f"\n{'='*60}")
    print("[智能同步] 开始同步目录")
    print(f"{'='*60}")
    
    # 检查rsync是否可用
    rsync_available = check_rsync_available()
    if rsync_available:
        print("✅ 检测到rsync，将使用增量同步（更快）")
    else:
        print("⚠️  未检测到rsync，将使用完整复制")
        print("   安装rsync命令: sudo apt-get install rsync")
    
    total_sync_count = 0
    sync_failures = []
    
    for config in SYNC_CONFIGS:
        source = config["source"]
        target = config["target"]
        description = config["description"]
        
        print(f"\n📁 {description}")
        print(f"  源: {source}")
        print(f"  目标: {target}")
        
        # 检查源目录是否存在
        if not os.path.exists(source):
            print(f"  ⚠️  源目录不存在，跳过")
            sync_failures.append(f"{description}: 源目录不存在")
            continue
        
        # 确保目标目录的父目录存在
        os.makedirs(os.path.dirname(target), exist_ok=True)
        
        # 根据rsync可用性选择同步方法
        if rsync_available:
            success, count = sync_with_rsync(source, target, description)
        else:
            success, count = sync_with_shutil(source, target, description)
        
        if success:
            total_sync_count += count
        else:
            sync_failures.append(description)
    
    # 总结
    print(f"\n{'='*60}")
    print("[同步完成]")
    if sync_failures:
        print(f"❌ 部分目录同步失败: {', '.join(sync_failures)}")
        return False
    else:
        print(f"✅ 所有目录同步完成")
        print(f"   总同步文件数: {total_sync_count}")
        return True

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

# 1. 同步所有目录
# sync_successful = sync_all_directories()

# 2. 修复路径问题（仅在同步成功时执行）
# if sync_successful:
#     fix_windows_paths()

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
    print(f"\n{'='*60}")
    print("[构建后] 复制产物到Windows")
    
    build_dir = env.subst("$BUILD_DIR")
    windows_build_dir = os.path.join(WINDOWS_BASE_DIR, "build")
    
    print(f"  从: {build_dir}")
    print(f"  到: {windows_build_dir}")
    
    os.makedirs(windows_build_dir, exist_ok=True)
    
    files_to_copy = [
        ("firmware.bin", "主固件"),
        ("bootloader.bin", "引导程序"), 
        ("partitions.bin", "分区表"),
        ("firmware.elf", "ELF文件"),
        ("firmware.map", "MAP文件")
    ]
    
    copied = 0
    for filename, desc in files_to_copy:
        src = os.path.join(build_dir, filename)
        if os.path.exists(src):
            try:
                shutil.copy2(src, windows_build_dir)
                print(f"  ✅ {desc}")
                copied += 1
            except Exception as e:
                print(f"  ❌ {desc}失败: {e}")
    
    print(f"✅ 共复制 {copied}/{len(files_to_copy)} 个文件")
    
    # 执行版本管理
    post_build_elf_versions()

# 注册构建后钩子
def copy_artifacts_to_windows_wrapper(source, target, env):
    if os.getenv("GITHUB_ACTIONS") == "true":
        print("[构建后] GitHub Actions 环境，跳过复制产物")
        return
    copy_artifacts_to_windows(source, target, env)

env.AddPostAction("buildprog", copy_artifacts_to_windows_wrapper)