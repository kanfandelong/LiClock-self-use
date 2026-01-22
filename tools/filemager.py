import os
import time
import shlex 
import argparse
import requests
from urllib.parse import quote
from pathlib import Path
from getpass import getpass
from math import log, floor
from requests_toolbelt import MultipartEncoder, MultipartEncoderMonitor

class FileManager:
    def __init__(self, base_url="http://localhost", session=None):
        self.uploaded = 0  # 新增上传进度计数器
        self.base_url = base_url
        self.cwd = "/"
        self.session = session or requests.Session()
        self.colors = {
            'folder': '\033[94m',    # 蓝色
            'file': '\033[92m',      # 绿色
            'error': '\033[91m',     # 红色
            'warning': '\033[93m',   # 黄色
            'end': '\033[0m'         # 重置颜色
        }
    def _format_size(self, size_bytes):
        """智能格式化文件大小"""
        if size_bytes == 0:
            return "0 B"
        units = ["B", "KB", "MB", "GB", "TB"]
        index = int(floor(log(size_bytes, 1024)))
        size = round(size_bytes / (1024 ** index), 2)
        return f"{size:>6.2f} {units[index]}"

    def _progress_bar(self, current, total, start_time):
        """生成进度条字符串"""
        bar_length = 30
        percent = current / total
        filled_length = int(bar_length * percent)
        bar = '█' * filled_length + '-' * (bar_length - filled_length)
    
        elapsed = time.time() - start_time
        speed = current / elapsed if elapsed > 0 else 0
        speed_str = self._format_size(speed) + "/s"
    
        if speed > 0:
            eta = (total - current) / speed
            eta_str = f"{eta:.1f}s"
        else:
            eta_str = "--"
        
        return (f"\033[K|{bar}| {percent:.1%} | "
                f"{self._format_size(current)}/{self._format_size(total)} | "
                f"{speed_str} | ETA: {eta_str}\r")

    def _request(self, method, path, params=None, data=None, files=None):
        url = f"{self.base_url}{path}"
        try:
            if method == "GET":
                response = self.session.get(url, params=params, stream=True)
            elif method == "POST":
                response = self.session.post(url, data=data, files=files)
            elif method == "DELETE":
                response = self.session.delete(url, data=data)
            else:
                raise ValueError("Unsupported HTTP method")
            response.raise_for_status()
            return response
        except requests.exceptions.RequestException as e:
            print(f"{self.colors['error']}请求失败: {e}{self.colors['end']}")
            return None

    def list_files(self, detailed=False):
        """获取带详细信息的文件列表"""
        params = {"list": self.cwd}
        response = self._request("GET", "/edit", params=params)
        if response and response.status_code == 200:
            files = response.json()
            if detailed:
                print(f"\n{self.colors['folder']}当前目录: {self.cwd}{self.colors['end']}")
                print(f"{'-'*40}")
                for f in files:
                    icon = "📁" if f["type"] == "folder" else "📄"
                    color = self.colors['folder'] if f["type"] == "folder" else self.colors['file']
                    size = self._format_size(f.get('size', 0)) if f["type"] == "file" else " " * 12
                    print(f"{color}{icon} {f['name']:<30} {size}{self.colors['end']}")
                print(f"{'-'*40}")
            return files
        return []

    def mkdir(self, dirname):
        """创建文件夹"""
        path = os.path.join(self.cwd, dirname).replace("\\", "/")
        return self._request("POST", f"/mkdir?path={quote(path)}")

    def delete(self, name):
        """删除文件"""
        path = os.path.join(self.cwd, name).replace("\\", "/")
        confirm = getpass(f"确认删除 {path}? (输入'y'确认): ")
        if confirm.lower() == 'y':
            return self._request("DELETE", "/edit", data={"path": path})
        print(f"{self.colors['warning']}操作已取消{self.colors['end']}")
        return None

    def rmrf(self, name):
        """删除文件夹"""
        path = os.path.join(self.cwd, name).replace("\\", "/")
        confirm = getpass(f"确认删除 {path}? (输入'y'确认): ")
        if confirm.lower() == 'y':
            return self._request("POST", "/rmrf", data={"path": path})
        print(f"{self.colors['warning']}操作已取消{self.colors['end']}")
        return None
    def rename(self, old_name, new_name):
        """重命名文件/文件夹"""
        old_path = os.path.join(self.cwd, old_name).replace("\\", "/")
        new_path = os.path.join(self.cwd, new_name).replace("\\", "/")
        return self._request("POST", f"/rename?path={quote(old_path)}&new={quote(new_path)}")

    def upload_file(self, local_path):
        """修复版文件上传函数（与网页端逻辑一致）"""
        try:
            if not os.path.exists(local_path):
                raise FileNotFoundError("本地文件不存在")

            # 构建服务器端期望的文件路径
            filename = os.path.basename(local_path)
            target_path = os.path.join(self.cwd, filename).replace("\\", "/")
            if self.cwd != '/':
                target_path = f"{self.cwd}/{filename}"
            else:
                target_path = f"/{filename}"

            # 使用与网页端一致的FormData结构
            with open(local_path, 'rb') as f:
                # 构建multipart数据（字段名必须为'data'）
                encoder = MultipartEncoder(
                    fields={
                        'data': (target_path, f, 'application/octet-stream')
                    }
                )

                # 进度监控回调
                start_time = time.time()
                def callback(monitor):
                    current = monitor.bytes_read
                    total = monitor.len
                    progress = self._progress_bar(current, total, start_time)
                    print(f"{self.colors['file']}{progress}{self.colors['end']}", end="", flush=True)

                monitor = MultipartEncoderMonitor(encoder, callback)

                # 发送到与网页端相同的端点
                response = self.session.post(
                    f"{self.base_url}/edit",  # 确保端点路径一致
                    data=monitor,
                    headers={'Content-Type': monitor.content_type}
                )

                # 强制显示最终进度
                print(f"{self.colors['file']}{self._progress_bar(encoder.len, encoder.len, start_time)}{self.colors['end']}")

                # 验证服务器响应
                if response.status_code == 200:
                    print(f"\n{self.colors['file']}✓ 上传成功{self.colors['end']}")
                    return True
                else:
                    print(f"\n{self.colors['error']}上传失败: {response.text}{self.colors['end']}")
                    return False

        except Exception as e:
            print(f"\n{self.colors['error']}错误: {str(e)}{self.colors['end']}")
            return False

    def download_file(self, remote_name, local_path=None):
        """带限速进度条的下载功能"""
        remote_path = os.path.join(self.cwd, remote_name).replace("\\", "/")
        response = self._request("GET", f"/edit?download={quote(remote_path)}")
        
        if not response:
            return False

        total_size = int(response.headers.get('Content-Length', 0))
        start_time = time.time()
        downloaded = 0
        last_print = 0  # 新增：最后打印时间戳
        print_interval = 0.3  # 新增：打印间隔（秒）
        
        try:
            local_path = local_path or os.path.basename(remote_name)
            Path(local_path).parent.mkdir(parents=True, exist_ok=True)
            
            print(f"{self.colors['file']}下载开始: {remote_name}{self.colors['end']}")
            with open(local_path, 'wb') as f:
                for chunk in response.iter_content(chunk_size=8192):
                    if chunk:
                        f.write(chunk)
                        downloaded += len(chunk)
                        
                        # 新增：限速打印逻辑
                        current_time = time.time()
                        if current_time - last_print > print_interval or downloaded == total_size:
                            print(f"\r{self._progress_bar(downloaded, total_size, start_time)}", end="")
                            last_print = current_time
                            
            # 强制完成最后100%的显示
            print(f"{self.colors['file']}{self._progress_bar(total_size, total_size, start_time)}{self.colors['end']}", end="", flush=True)
            print(f"\n{self.colors['file']}✓ 下载成功: {local_path}{self.colors['end']}")
            return True
        except Exception as e:
            print(f"\n{self.colors['error']}下载失败: {e}{self.colors['end']}")
            return False
    def switch_filesystem(self):
        """切换文件系统"""
        return self._request("POST", "/switch_file_system")

    def fs_get(self):
        """获取当前文件系统"""
        return self._request("POST", "/fs_get")

    def navigate(self, dirname):
        """进入子目录"""
        new_path = os.path.join(self.cwd, dirname).replace("\\", "/")
        self.cwd = new_path

    def up(self):
        """返回上级目录"""
        self.cwd = os.path.dirname(self.cwd) or "/"

def main():
    parser = argparse.ArgumentParser(description="高级文件管理客户端")
    parser.add_argument("--ip", type=str, required=True,
                        help="服务器地址 (示例: 192.168.1.100:8000)")
    parser.add_argument("--user", type=str, help="用户名")
    parser.add_argument("--password", type=str, help="密码（建议交互式输入）")
    args = parser.parse_args()

    # 处理认证
    session = requests.Session()
    if args.user:
        password = args.password or getpass("请输入密码: ")
        session.auth = (args.user, password)

    # 处理服务器地址
    base_url = args.ip if args.ip.startswith(("http://", "https://")) else f"http://{args.ip}"
    fm = FileManager(base_url=base_url, session=session)
    FILE_system = "unknown"
    _file_sys = fm.fs_get()
    if _file_sys != None:
        FILE_system = _file_sys.text
    else:
        print(f"\n{fm.colors['error']}未能获取当前文件系统{fm.colors['end']}")
    fm.list_files(detailed=True)
    # 交互式帮助系统
    help_text = """
可用命令:
  ls         - 列出文件
  cd <目录>  - 进入目录
  up         - 返回上级目录
  mkdir      - 创建文件夹
  upload     - 上传文件
  download   - 下载文件
  rename     - 重命名
  delete     - 删除文件
  rmrf       - 递归删除文件夹
  switch     - 切换文件系统
  clear      - 清屏
  exit       - 退出
"""

    try:
        while True:
            user_input = input(f"\n{fm.colors['folder']}{FILE_system}:{fm.cwd} >{fm.colors['end']} ").strip()
            cmd = shlex.split(user_input)  # 使用shlex解析带引号的参数
            if not cmd:
                continue

            action = cmd[0].lower()
            try:
                if action == "exit":
                    break
                elif action == "help":
                    print(help_text)
                elif action == "ls":
                    fm.list_files(detailed=True)
                    continue
                elif action == "cd":
                    if len(cmd) > 1:
                        fm.navigate(cmd[1])
                    else:
                        print(f"{fm.colors['warning']}需要指定目录{fm.colors['end']}")
                elif action == "up":
                    fm.up()
                elif action == "mkdir":
                    if len(cmd) > 1:
                        fm.mkdir(cmd[1])
                    else:
                        name = input("新文件夹名称: ")
                        fm.mkdir(name)
                elif action == "upload":
                    if len(cmd) > 1:
                        local_path = cmd[1]
                    else:
                        local_path = input("本地文件路径: ")
                    fm.upload_file(local_path)
                elif action == "download":
                    if len(cmd) > 1:
                        remote_name = cmd[1]
                        local_path = cmd[2] if len(cmd) > 2 else None
                    else:
                        remote_name = input("远程文件名: ")
                        local_path = input("本地保存路径（可选）: ") or None
                    fm.download_file(remote_name, local_path)
                elif action == "rename":
                    if len(cmd) > 2:
                        old_name, new_name = cmd[1], cmd[2]
                    else:
                        old_name = input("原文件名: ")
                        new_name = input("新文件名: ")
                    fm.rename(old_name, new_name)
                elif action == "delete":
                    if len(cmd) > 1:
                        fm.delete(cmd[1])
                    else:
                        name = input("要删除的文件名: ")
                        fm.delete(name)
                elif action == "rmrf":
                    if len(cmd) > 1:
                        fm.rmrf(cmd[1])
                    else:
                        name = input("要删除的文件夹: ")
                        fm.rmrf(name)
                elif action == "switch":
                    _requests = fm.switch_filesystem()
                    if _requests != None:
                        FILE_system = _requests.text
                        print("文件系统已切换")
                elif action == "clear":
                    os.system('cls' if os.name == 'nt' else 'clear')
                else:
                    print(f"{fm.colors['error']}未知命令，输入 help 查看帮助{fm.colors['end']}")
            except KeyboardInterrupt:
                print("\n操作已取消")
            except Exception as e:
                print(f"{fm.colors['error']}错误: {e}{fm.colors['end']}")

    except KeyboardInterrupt:
        print(f"\n{fm.colors['warning']}已退出程序{fm.colors['end']}")

if __name__ == "__main__":
    main()