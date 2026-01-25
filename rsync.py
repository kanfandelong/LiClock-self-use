#!/usr/bin/env python3
"""
多文件夹同步工具
使用 rsync 同步多个源文件夹到目标文件夹
"""

import subprocess
import os
import sys
import logging
from datetime import datetime

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# ============================
# 配置区域：在这里设置同步任务
# ============================

# 同步任务列表：每个任务是一个字典，包含源路径和目标路径
SYNC_TASKS = [
    {
        'source': '/home/kanfandelong/LiClock-dev_multithread-main/src/',
        'destination': '/mnt/IDCN823/LiClock-dev_multithread-main/src/',
        'name': 'src'  # 任务名称（可选）
    },
    {
        'source': '/home/kanfandelong/LiClock-dev_multithread-main/lib/',
        'destination': '/mnt/IDCN823/LiClock-dev_multithread-main/lib/',
        'name': 'lib'
    },
    {
        'source': '/home/kanfandelong/LiClock-dev_multithread-main/include/',
        'destination': '/mnt/IDCN823/LiClock-dev_multithread-main/include/',
        'name': 'include'
    },
    # 添加更多任务...
]

# Rsync 参数配置
RSYNC_OPTIONS = [
    '-avz',           # 归档模式，显示进度，压缩传输
    # '--delete',       # 删除目标中存在而源中不存在的文件
    '--progress',     # 显示传输进度
    # '--dry-run',    # 测试模式（不实际执行，取消注释以测试）
]

# ============================
# 同步函数
# ============================

def run_rsync(source, destination, options, task_name=""):
    """
    执行 rsync 命令
    
    参数:
        source: 源路径
        destination: 目标路径
        options: rsync 选项列表
        task_name: 任务名称（用于日志）
    """
    # 确保源路径以/结尾（对于目录同步）
    if source.endswith('/'):
        source_for_rsync = source
    else:
        source_for_rsync = source + '/'
    
    # 构建命令
    cmd = ['rsync'] + options + [source_for_rsync, destination]
    
    # 显示命令（调试用）
    logger.info(f"执行任务: {task_name}")
    logger.info(f"命令: {' '.join(cmd)}")
    
    try:
        # 执行 rsync 命令
        result = subprocess.run(
            cmd,
            check=True,
            capture_output=True,
            text=True,
            encoding='utf-8'
        )
        
        # 输出结果
        if result.stdout:
            logger.info(f"输出:\n{result.stdout}")
        
        logger.info(f"任务 '{task_name}' 完成")
        return True
        
    except subprocess.CalledProcessError as e:
        logger.error(f"任务 '{task_name}' 失败!")
        logger.error(f"错误输出:\n{e.stderr}")
        return False
    except FileNotFoundError:
        logger.error("未找到 rsync 命令。请确保已安装 rsync")
        return False

def validate_paths(source, destination):
    """
    验证路径是否存在（仅检查源路径）
    
    参数:
        source: 源路径
        destination: 目标路径
    """
    if not os.path.exists(source):
        logger.warning(f"源路径不存在: {source}")
        return False
    
    # 检查目标路径的父目录是否存在
    dest_dir = os.path.dirname(destination.rstrip('/'))
    if dest_dir and not os.path.exists(dest_dir):
        logger.warning(f"目标路径的父目录不存在: {dest_dir}")
        logger.warning(f"rsync 将尝试创建目标目录")
    
    return True

def sync_all_tasks():
    """
    执行所有同步任务
    """
    logger.info("=" * 60)
    logger.info(f"开始同步任务 - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    logger.info("=" * 60)
    
    success_count = 0
    failure_count = 0
    
    for i, task in enumerate(SYNC_TASKS, 1):
        source = task['source']
        destination = task['destination']
        task_name = task.get('name', f"任务{i}")
        
        logger.info(f"\n[任务 {i}/{len(SYNC_TASKS)}] {task_name}")
        logger.info(f"源: {source}")
        logger.info(f"目标: {destination}")
        
        # 验证路径
        if not validate_paths(source, destination):
            logger.warning(f"跳过任务: {task_name}")
            failure_count += 1
            continue
        
        # 执行同步
        if run_rsync(source, destination, RSYNC_OPTIONS, task_name):
            success_count += 1
        else:
            failure_count += 1
    
    # 汇总报告
    logger.info("\n" + "=" * 60)
    logger.info("同步任务汇总:")
    logger.info(f"成功: {success_count}")
    logger.info(f"失败: {failure_count}")
    logger.info(f"总计: {len(SYNC_TASKS)}")
    logger.info("=" * 60)
    
    return success_count == len(SYNC_TASKS)

# ============================
# 高级配置选项（可选）
# ============================

def create_example_config():
    """创建示例配置文件"""
    example_config = '''"""
多文件夹同步配置示例
将以下配置复制到程序中的 SYNC_TASKS 列表
"""

SYNC_TASKS = [
    # 示例1: 本地同步
    {
        'source': '/home/username/Desktop/',
        'destination': '/media/backup/Desktop_backup/',
        'name': '桌面备份'
    },
    
    # 示例2: 远程同步（需要SSH配置）
    {
        'source': '/home/username/Documents/',
        'destination': 'user@remote-server:/backup/Documents/',
        'name': '远程文档备份'
    },
    
    # 示例3: 排除某些文件类型
    {
        'source': '/home/username/Downloads/',
        'destination': '/backup/Downloads/',
        'name': '下载文件夹备份',
        # 注意：需要在 RSYNC_OPTIONS 中添加排除选项
        # '--exclude=*.tmp',
        # '--exclude=*.log',
    },
]
'''
    return example_config

# ============================
# 主程序
# ============================

def main():
    """主函数"""
    print("\n多文件夹同步工具")
    print("=" * 40)
    
    # 检查是否有任务配置
    if not SYNC_TASKS:
        print("\n未配置同步任务!")
        print("\n示例配置:")
        print(create_example_config())
        return
    
    # 显示配置的任务
    print(f"\n配置了 {len(SYNC_TASKS)} 个同步任务:")
    for i, task in enumerate(SYNC_TASKS, 1):
        task_name = task.get('name', f"任务{i}")
        print(f"  {i}. {task_name}")
        print(f"     源: {task['source']}")
        print(f"     目标: {task['destination']}")
    
    # 确认执行
    print(f"\n使用的 rsync 选项: {' '.join(RSYNC_OPTIONS)}")
    
    if '--dry-run' in RSYNC_OPTIONS:
        print("\n注意：当前处于测试模式 (--dry-run)，不会实际复制文件")
    
    response = input("\n是否开始同步? (y/N): ").strip().lower()
    if response != 'y':
        print("取消同步操作")
        return
    
    # 执行同步
    success = sync_all_tasks()
    
    if success:
        print("\n✓ 所有同步任务完成!")
    else:
        print("\n⚠ 部分同步任务失败，请检查日志")
    
    print(f"\n完成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n程序被用户中断")
        sys.exit(1)
    except Exception as e:
        logger.error(f"程序执行出错: {e}")
        sys.exit(1)