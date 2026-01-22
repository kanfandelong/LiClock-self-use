#!/usr/bin/env python3
"""
ELF版本数据库压缩工具
用于压缩除最近5个版本外的所有ELF/BIN文件
使用XZ压缩算法，压缩级别为最高
"""

import os
import sys
import json
import lzma
import hashlib
import shutil
import subprocess
from datetime import datetime
from pathlib import Path
import argparse
import traceback

# 配置
DEFAULT_DATABASE = "./elf_versions/elf_database.json"
DEFAULT_STORAGE_DIR = "./elf_versions"
MAX_VERSIONS_KEEP_UNCOMPRESSED = 5
XZ_COMPRESS_LEVEL = 9  # 最高压缩级别

class ElfDatabaseCompressor:
    def __init__(self, database_file=None, storage_dir=None, keep_uncompressed=5):
        """
        初始化压缩器
        
        Args:
            database_file: 数据库文件路径
            storage_dir: 文件存储目录
            keep_uncompressed: 保持未压缩的最近版本数量
        """
        self.database_file = database_file or DEFAULT_DATABASE
        self.storage_dir = storage_dir or DEFAULT_STORAGE_DIR
        self.keep_uncompressed = keep_uncompressed
        self.compressed_count = 0
        self.failed_count = 0
        self.total_size_before = 0
        self.total_size_after = 0
        
    def check_xz_tool(self):
        """检查xz工具是否可用"""
        try:
            result = subprocess.run(["xz", "--version"], 
                                  capture_output=True, 
                                  text=True, 
                                  check=False)
            return result.returncode == 0
        except (subprocess.SubprocessError, FileNotFoundError):
            return False
    
    def check_python_lzma(self):
        """检查Python的lzma模块是否可用"""
        try:
            # 测试lzma模块
            with lzma.open(os.devnull, 'wb') as f:
                f.write(b'test')
            return True
        except Exception:
            return False
    
    def compress_with_xz_tool(self, filepath, compression_level=9):
        """
        使用系统xz工具进行压缩
        
        Args:
            filepath: 要压缩的文件路径
            compression_level: 压缩级别 (1-9)
            
        Returns:
            tuple: (成功与否, 压缩后文件路径, 压缩后大小)
        """
        compressed_file = f"{filepath}.xz"
        
        try:
            # 使用系统xz工具进行压缩
            cmd = [
                "xz",
                f"-{compression_level}",  # 压缩级别
                "-k",  # 保留原始文件
                "-f",  # 强制覆盖
                filepath
            ]
            
            print(f"    压缩命令: {' '.join(cmd)}")
            
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=True
            )
            
            # 检查压缩文件是否存在
            if os.path.exists(compressed_file):
                # 压缩成功后删除原始文件
                os.remove(filepath)
                compressed_size = os.path.getsize(compressed_file)
                return True, compressed_file, compressed_size
            else:
                return False, None, 0
                
        except subprocess.CalledProcessError as e:
            print(f"    ❌ xz压缩失败: {e}")
            print(f"    错误输出: {e.stderr}")
            return False, None, 0
        except Exception as e:
            print(f"    ❌ 压缩过程异常: {e}")
            return False, None, 0
    
    def compress_with_python_lzma(self, filepath, compression_level=9):
        """
        使用Python lzma模块进行压缩
        
        Args:
            filepath: 要压缩的文件路径
            compression_level: 压缩级别 (1-9)
            
        Returns:
            tuple: (成功与否, 压缩后文件路径, 压缩后大小)
        """
        compressed_file = f"{filepath}.xz"
        
        try:
            # 设置压缩过滤器
            filters = [
                {
                    "id": lzma.FILTER_LZMA2,
                    "preset": compression_level
                }
            ]
            
            # 读取原始文件
            with open(filepath, 'rb') as f_in:
                original_data = f_in.read()
                original_size = len(original_data)
            
            # 使用lzma压缩
            compressed_data = lzma.compress(
                original_data,
                format=lzma.FORMAT_XZ,
                filters=filters
            )
            
            # 写入压缩文件
            with open(compressed_file, 'wb') as f_out:
                f_out.write(compressed_data)
            
            compressed_size = os.path.getsize(compressed_file)
            
            # 验证压缩文件
            with lzma.open(compressed_file, 'rb') as f_test:
                decompressed = f_test.read()
                if decompressed != original_data:
                    os.remove(compressed_file)
                    return False, None, 0
            
            # 压缩成功后删除原始文件
            os.remove(filepath)
            
            return True, compressed_file, compressed_size
            
        except Exception as e:
            print(f"    ❌ Python lzma压缩失败: {e}")
            # 清理可能创建的部分文件
            if os.path.exists(compressed_file):
                os.remove(compressed_file)
            return False, None, 0
    
    def compress_file(self, filepath, description=""):
        """
        压缩单个文件
        
        Args:
            filepath: 文件路径
            description: 文件描述
            
        Returns:
            tuple: (成功与否, 压缩后文件路径, 压缩后大小)
        """
        if not os.path.exists(filepath):
            print(f"    ⚠️  文件不存在: {filepath}")
            return False, None, 0
        
        original_size = os.path.getsize(filepath)
        print(f"    📦 {description}")
        print(f"      原文件: {os.path.basename(filepath)} ({original_size:,} bytes)")
        
        # 尝试使用系统xz工具（通常更快）
        if self.check_xz_tool():
            success, compressed_path, compressed_size = self.compress_with_xz_tool(
                filepath, 
                XZ_COMPRESS_LEVEL
            )
            method = "系统xz工具"
        else:
            # 回退到Python lzma模块
            success, compressed_path, compressed_size = self.compress_with_python_lzma(
                filepath, 
                XZ_COMPRESS_LEVEL
            )
            method = "Python lzma模块"
        
        if success:
            compression_ratio = (1 - compressed_size / original_size) * 100
            print(f"      压缩方法: {method}")
            print(f"      压缩后: {os.path.basename(compressed_path)} ({compressed_size:,} bytes)")
            print(f"      压缩率: {compression_ratio:.1f}%")
            return True, compressed_path, compressed_size
        else:
            print(f"      ❌ 压缩失败")
            return False, None, 0
    
    def load_database(self):
        """加载数据库"""
        if not os.path.exists(self.database_file):
            print(f"❌ 数据库文件不存在: {self.database_file}")
            return None
        
        try:
            with open(self.database_file, 'r', encoding='utf-8') as f:
                database = json.load(f)
            return database
        except Exception as e:
            print(f"❌ 加载数据库失败: {e}")
            return None
    
    def save_database(self, database):
        """保存数据库"""
        try:
            # 创建备份
            backup_file = f"{self.database_file}.bak"
            if os.path.exists(self.database_file):
                shutil.copy2(self.database_file, backup_file)
            
            # 保存新数据库
            with open(self.database_file, 'w', encoding='utf-8') as f:
                json.dump(database, f, indent=2, ensure_ascii=False)
            
            print(f"✅ 数据库已保存到: {self.database_file}")
            print(f"   备份已创建: {backup_file}")
            return True
        except Exception as e:
            print(f"❌ 保存数据库失败: {e}")
            return False
    
    def get_file_info(self, version):
        """从版本信息中提取文件信息"""
        files = []
        
        # ELF文件
        if "elf" in version and "path" in version["elf"]:
            files.append({
                "path": version["elf"]["path"],
                "type": "elf",
                "original_size": version["elf"].get("size", 0),
                "description": "ELF文件"
            })
        
        # BIN文件
        if "bin" in version and "path" in version["bin"]:
            files.append({
                "path": version["bin"]["path"],
                "type": "bin",
                "original_size": version["bin"].get("size", 0),
                "description": "BIN文件"
            })
        
        return files
    
    def update_version_info(self, version, compressed_files):
        """更新版本信息，添加压缩信息"""
        version["compressed"] = True
        version["compressed_time"] = datetime.now().isoformat()
        
        # 更新文件信息
        for file_info in compressed_files:
            if file_info["type"] == "elf":
                version["elf"]["path"] = file_info["compressed_path"]
                version["elf"]["compressed_size"] = file_info["compressed_size"]
                version["elf"]["compression_ratio"] = file_info["compression_ratio"]
            elif file_info["type"] == "bin":
                version["bin"]["path"] = file_info["compressed_path"]
                version["bin"]["compressed_size"] = file_info["compressed_size"]
                version["bin"]["compression_ratio"] = file_info["compression_ratio"]
        
        return version
    
    def compress_old_versions(self):
        """压缩旧版本文件"""
        print(f"\n{'='*60}")
        print("📦 ELF版本数据库压缩工具")
        print(f"{'='*60}")
        
        # 加载数据库
        database = self.load_database()
        if not database:
            return False
        
        versions = database.get("versions", [])
        if not versions:
            print("⚠️  数据库中没有版本记录")
            return True
        
        print(f"📊 数据库信息:")
        print(f"   版本总数: {len(versions)}")
        print(f"   保持未压缩的最近版本数: {self.keep_uncompressed}")
        
        # 按时间戳排序（最新的在前面）
        versions.sort(key=lambda x: x.get("timestamp", ""), reverse=True)
        
        # 确定需要压缩的版本
        versions_to_compress = versions[self.keep_uncompressed:]
        versions_to_keep = versions[:self.keep_uncompressed]
        
        if not versions_to_compress:
            print("✅ 没有需要压缩的旧版本")
            return True
        
        print(f"🔍 需要压缩的旧版本数: {len(versions_to_compress)}")
        print(f"📁 保持未压缩的版本数: {len(versions_to_keep)}")
        
        # 检查压缩工具可用性
        if not self.check_xz_tool() and not self.check_python_lzma():
            print("❌ 错误: 没有可用的压缩工具")
            print("   请安装xz工具或确保Python lzma模块可用")
            return False
        
        print(f"\n🛠️  压缩配置:")
        if self.check_xz_tool():
            print(f"   使用: 系统xz工具 (压缩级别: {XZ_COMPRESS_LEVEL})")
        else:
            print(f"   使用: Python lzma模块 (压缩级别: {XZ_COMPRESS_LEVEL})")
        
        # 开始压缩
        print(f"\n🚀 开始压缩旧版本文件...")
        
        for i, version in enumerate(versions_to_compress, 1):
            timestamp = version.get("timestamp", "未知时间")
            print(f"\n[{i}/{len(versions_to_compress)}] 处理版本: {timestamp}")
            
            # 检查是否已经压缩过
            if version.get("compressed", False):
                print("    ⏭️  已压缩，跳过")
                continue
            
            # 获取文件信息
            files = self.get_file_info(version)
            if not files:
                print("    ⚠️  版本中没有文件信息，跳过")
                continue
            
            compressed_files = []
            version_original_size = 0
            version_compressed_size = 0
            
            # 压缩每个文件
            for file_info in files:
                if not os.path.exists(file_info["path"]):
                    print(f"    ⚠️  文件不存在: {file_info['path']}")
                    continue
                
                # 检查是否已经是压缩文件
                if file_info["path"].endswith('.xz'):
                    print(f"    ⏭️  文件已压缩: {os.path.basename(file_info['path'])}")
                    continue
                
                version_original_size += file_info["original_size"]
                
                # 压缩文件
                success, compressed_path, compressed_size = self.compress_file(
                    file_info["path"],
                    file_info["description"]
                )
                
                if success:
                    self.compressed_count += 1
                    self.total_size_before += file_info["original_size"]
                    self.total_size_after += compressed_size
                    version_compressed_size += compressed_size
                    
                    compression_ratio = (1 - compressed_size / file_info["original_size"]) * 100
                    
                    compressed_files.append({
                        "type": file_info["type"],
                        "original_path": file_info["path"],
                        "compressed_path": compressed_path,
                        "compressed_size": compressed_size,
                        "compression_ratio": compression_ratio
                    })
                else:
                    self.failed_count += 1
                    print(f"    ❌ 文件压缩失败: {os.path.basename(file_info['path'])}")
            
            # 更新版本信息
            if compressed_files:
                self.update_version_info(version, compressed_files)
                
                if version_original_size > 0:
                    version_compression_ratio = (1 - version_compressed_size / version_original_size) * 100
                    print(f"    📊 版本压缩统计:")
                    print(f"       原始大小: {version_original_size:,} bytes")
                    print(f"       压缩后: {version_compressed_size:,} bytes")
                    print(f"       压缩率: {version_compression_ratio:.1f}%")
        
        # 更新数据库
        database["versions"] = versions_to_keep + versions_to_compress
        database["last_compressed"] = datetime.now().isoformat()
        database["compression_stats"] = {
            "compressed_versions": len(versions_to_compress),
            "kept_uncompressed": len(versions_to_keep),
            "total_files_compressed": self.compressed_count,
            "failed_files": self.failed_count,
            "total_size_before": self.total_size_before,
            "total_size_after": self.total_size_after,
            "overall_compression_ratio": (1 - self.total_size_after / self.total_size_before) * 100 if self.total_size_before > 0 else 0
        }
        
        # 保存数据库
        if self.compressed_count > 0 or self.failed_count > 0:
            return self.save_database(database)
        else:
            print("\n✅ 没有需要压缩的文件")
            return True
    
    def list_versions(self):
        """列出所有版本信息"""
        database = self.load_database()
        if not database:
            return False
        
        versions = database.get("versions", [])
        
        print(f"\n{'='*60}")
        print("📋 版本列表")
        print(f"{'='*60}")
        
        if not versions:
            print("没有版本记录")
            return True
        
        # 按时间戳排序（最新的在前面）
        versions.sort(key=lambda x: x.get("timestamp", ""), reverse=True)
        
        for i, version in enumerate(versions, 1):
            timestamp = version.get("timestamp", "未知")
            compressed = version.get("compressed", False)
            
            print(f"\n[{i}] {timestamp} {'[已压缩]' if compressed else '[未压缩]'}")
            
            # ELF文件信息
            if "elf" in version:
                elf = version["elf"]
                size = elf.get("compressed_size", elf.get("size", 0))
                print(f"    ELF: {elf.get('filename', '未知')} ({size:,} bytes)")
            
            # BIN文件信息
            if "bin" in version:
                bin_file = version["bin"]
                size = bin_file.get("compressed_size", bin_file.get("size", 0))
                print(f"    BIN: {bin_file.get('filename', '未知')} ({size:,} bytes)")
        
        # 统计信息
        compressed_count = sum(1 for v in versions if v.get("compressed", False))
        print(f"\n📊 统计:")
        print(f"   总版本数: {len(versions)}")
        print(f"   已压缩: {compressed_count}")
        print(f"   未压缩: {len(versions) - compressed_count}")
        
        return True

def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description="ELF版本数据库压缩工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  %(prog)s compress           # 压缩除最近5个版本外的所有文件
  %(prog)s compress --keep 10 # 压缩除最近10个版本外的所有文件
  %(prog)s list               # 列出所有版本信息
  %(prog)s compress --db /path/to/database.json --dir /path/to/storage
        """
    )
    
    subparsers = parser.add_subparsers(dest="command", help="命令")
    
    # compress命令
    compress_parser = subparsers.add_parser("compress", help="压缩旧版本文件")
    compress_parser.add_argument("--db", "--database", dest="database", 
                               help=f"数据库文件路径 (默认: {DEFAULT_DATABASE})")
    compress_parser.add_argument("--dir", "--directory", dest="directory",
                               help=f"文件存储目录 (默认: {DEFAULT_STORAGE_DIR})")
    compress_parser.add_argument("--keep", type=int, default=MAX_VERSIONS_KEEP_UNCOMPRESSED,
                               help=f"保持未压缩的最近版本数 (默认: {MAX_VERSIONS_KEEP_UNCOMPRESSED})")
    
    # list命令
    list_parser = subparsers.add_parser("list", help="列出所有版本信息")
    list_parser.add_argument("--db", "--database", dest="database",
                           help=f"数据库文件路径 (默认: {DEFAULT_DATABASE})")
    
    # 解析参数
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return 1
    
    try:
        if args.command == "compress":
            compressor = ElfDatabaseCompressor(
                database_file=args.database,
                storage_dir=args.directory,
                keep_uncompressed=args.keep
            )
            
            success = compressor.compress_old_versions()
            
            if success:
                print(f"\n{'='*60}")
                print("✅ 压缩完成")
                print(f"{'='*60}")
                print(f"📊 压缩统计:")
                print(f"   成功压缩文件数: {compressor.compressed_count}")
                print(f"   失败文件数: {compressor.failed_count}")
                print(f"   原始总大小: {compressor.total_size_before:,} bytes")
                print(f"   压缩后总大小: {compressor.total_size_after:,} bytes")
                
                if compressor.total_size_before > 0:
                    ratio = (1 - compressor.total_size_after / compressor.total_size_before) * 100
                    savings = compressor.total_size_before - compressor.total_size_after
                    print(f"   总体压缩率: {ratio:.1f}%")
                    print(f"   节省空间: {savings:,} bytes ({savings/1024/1024:.2f} MB)")
                
                return 0
            else:
                print("\n❌ 压缩失败")
                return 1
        
        elif args.command == "list":
            compressor = ElfDatabaseCompressor(database_file=args.database)
            success = compressor.list_versions()
            return 0 if success else 1
    
    except KeyboardInterrupt:
        print("\n\n⚠️  操作被用户中断")
        return 130
    except Exception as e:
        print(f"\n❌ 发生错误: {e}")
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())