#!/data/data/com.termux/files/usr/bin/python
import argparse
import requests
import os
from concurrent.futures import ThreadPoolExecutor
from urllib.parse import quote
import sys
from tqdm import tqdm

class ProgressFileWrapper:
    """带进度跟踪的文件包装类"""
    def __init__(self, file_path):
        self.file = open(file_path, 'rb')
        self.file_size = os.path.getsize(file_path)
        self.progress = tqdm(
            total=self.file_size,
            unit='B',
            unit_scale=True,
            desc=os.path.basename(file_path),
            leave=False
        )

    def read(self, size=-1):
        data = self.file.read(size)
        if data:
            self.progress.update(len(data))
        return data

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.progress.close()
        self.file.close()


class ESP32CLIManager:
    def __init__(self, device_ip):
        self.device_ip = device_ip
        self.current_path = "/"
        self.executor = ThreadPoolExecutor(max_workers=4)
        self.session = requests.Session()
        self._verify_connection()

    def _verify_connection(self):
        try:
            response = self._api_request("?list=/", timeout=3)
            if response.status_code != 200:
                raise ConnectionError(f"无法连接设备，HTTP状态码：{response.status_code}")
        except Exception as e:
            raise ConnectionError(f"连接失败：{str(e)}")

    def _api_request(self, endpoint, method='GET', **kwargs):
        """增强的API请求方法，支持多种HTTP方法"""
        url = f"http://{self.device_ip}/edit{endpoint}"
        # 自动处理POST参数
        if method.upper() in ['POST', 'PUT', 'DELETE']:
            kwargs.setdefault('headers', {'Content-Type': 'application/x-www-form-urlencoded'})
            if 'params' in kwargs and method == 'POST':
                kwargs['data'] = kwargs.pop('params')
        return self.session.request(method, url, **kwargs)
    def _api_other(self, api, method='GET', **kwargs):
        """增强的API请求方法，支持多种HTTP方法"""
        url = f"http://{self.device_ip}{api}"
        # 自动处理POST参数
        if method.upper() in ['POST', 'PUT', 'DELETE']:
            kwargs.setdefault('headers', {'Content-Type': 'application/x-www-form-urlencoded'})
            if 'params' in kwargs and method == 'POST':
                kwargs['data'] = kwargs.pop('params')
        return self.session.request(method, url, **kwargs)

    def list_files(self, show_progress=False):
        """列出当前目录文件"""
        try:
            encoded_path = quote(self.current_path)
            response = self._api_request(f"?list={encoded_path}")
            response.raise_for_status()
            
            items = response.json()
            if show_progress:
                print(f"\n当前目录：{self.current_path}")
                print(f"{'类型':<8}{'名称':<30}{'大小':<10}")
                print("-" * 50)
                for item in items:
                    file_type = "文件夹" if item['type'] == "folder" else "文件"
                    size = self.format_size(item['size']) if item['type'] == "file" else "-"
                    print(f"{file_type:<8}{item['name']:<30}{size:<10}")
            return items
        except Exception as e:
            print(f"错误：{str(e)}")
            return []

    def change_directory(self, path):
        """切换目录"""
        if path == "..":
            if self.current_path == "/":
                return
            self.current_path = os.path.dirname(self.current_path.rstrip('/')) + '/'
            if self.current_path == "//":
                self.current_path = "/"
            return

        target_path = os.path.join(self.current_path, path).replace('\\', '/') + '/'
        encoded_path = quote(target_path)
        response = self._api_request(f"?list={encoded_path}")
        if response.status_code == 200:
            self.current_path = target_path
        else:
            print("目录不存在")

    def download(self, remote_path, local_dir, recursive=False):
        """下载文件或目录"""
        if recursive:
            all_files = self._get_all_files(remote_path)
            with tqdm(total=len(all_files), desc="下载进度") as pbar:
                for file in all_files:
                    self._download_file(file, local_dir)
                    pbar.update(1)
        else:
            self._download_file(remote_path, local_dir)
        print(f"\n下载完成到：{local_dir}")

    def _download_file(self, remote_path, local_dir):
        """下载单个文件"""
        local_path = os.path.join(local_dir, remote_path.lstrip('/'))
        os.makedirs(os.path.dirname(local_path), exist_ok=True)
        
        response = self._api_request(f"?download={quote(remote_path)}", stream=True)
        response.raise_for_status()
        
        with open(local_path, 'wb') as f, tqdm(
            total=int(response.headers.get('content-length', 0)),
            unit='B',
            unit_scale=True,
            desc=os.path.basename(remote_path),
            leave=False
        ) as pbar:
            for chunk in response.iter_content(chunk_size=8192):
                f.write(chunk)
                pbar.update(len(chunk))

    def upload(self, local_path, remote_dir=None):
        """上传文件或目录"""
        remote_dir = remote_dir or self.current_path
        if os.path.isdir(local_path):
            for root, _, files in os.walk(local_path):
                for file in files:
                    local_file = os.path.join(root, file)
                    relative = os.path.relpath(local_file, local_path)
                    remote_file = os.path.join(remote_dir, relative).replace('\\', '/')
                    self._upload_file(local_file, remote_file)
        else:
            remote_file = os.path.join(remote_dir, os.path.basename(local_path)).replace('\\', '/')
            self._upload_file(local_path, remote_file)
        print(f"\n上传完成到：{remote_dir}")

    def _upload_file(self, local_path, remote_path):
        """上传单个文件（修复进度条版本）"""
        try:
            with ProgressFileWrapper(local_path) as file_wrapper:
                response = self._api_request(
                    "",
                    method='POST',
                    files={'data': (remote_path, file_wrapper)},
                    params={'path': remote_path}
                )
            response.raise_for_status()
        except Exception as e:
            raise RuntimeError(f"上传失败: {str(e)}")

    def delete_file(self, path):
        """删除文件"""
        try:
            encoded_path = quote(path)
            url = f"http://{self.device_ip}/edit?path={encoded_path}"
            response = self.session.request('DELETE', url) 
            if response.status_code in [200, 304]:
                return True
            print(f"删除失败: {response.text}")
            return False
        except Exception as e:
            print(f"删除错误: {str(e)}")
            return False

    def create_directory(self, path):
        """创建目录"""
        try:
            encoded_path = quote(path)
            url = f"http://{self.device_ip}/mkdir?path={encoded_path}"
            response = self.session.request('POST', url) 
            return response.status_code == 200
        except Exception as e:
            print(f"创建目录错误: {str(e)}")
            return False

    def rename(self, old_path, new_name):
        """重命名文件/目录"""
        try:
            encoded_path = quote(old_path)
            encoded_new = quote(new_name)
            url = f"http://{self.device_ip}/rename?path={encoded_path}&{new_name}"
            response = self.session.request('POST', url) 
            return response.status_code == 200
        except Exception as e:
            print(f"重命名错误: {str(e)}")
            return False

    def _get_all_files(self, path):
        """递归获取目录下所有文件"""
        files = []
        response = self._api_request(f"?list={quote(path)}")
        if response.status_code == 200:
            for item in response.json():
                full_path = f"{path.rstrip('/')}/{item['name']}"
                if item['type'] == "folder":
                    files.extend(self._get_all_files(full_path + "/"))
                else:
                    files.append(full_path)
        return files

    def delete_directory(self, path):
        """删除目录（递归）"""
        # files = self._get_all_files(path)
        # 先删除所有子内容
        # for file in reversed(files):
        #     self.delete_file(file)
        # 删除空目录
        encoded_path = quote(path)
        url = f"http://{self.device_ip}/rmrf?path={encoded_path}"
        response = self.session.request('POST', url) 
        if response.status_code != 200:
            raise RuntimeError(f"删除目录失败: {response.text}")
        return True
    def switch_file_system(self):
        """切换文件系统（LittleFS/SD）"""
        try:
            url = f"http://{self.device_ip}/switch_file_system"
            response = self.session.request('POST', url)
            if response.status_code == 200:
                return True, response.text.strip()
            elif response.status_code == 409:
                return False, f"切换失败: {response.text}"
            else:
                return False, f"未知响应码: {response.status_code}"
        except Exception as e:
            return False, f"请求失败: {str(e)}"
    @staticmethod
    def format_size(size):
        """格式化文件大小"""
        units = ['B', 'KB', 'MB', 'GB']
        for unit in units:
            if size < 1024.0 or unit == 'GB':
                break
            size /= 1024.0
        return f"{size:.1f} {unit}"

def interactive_shell(device_ip):
    manager = ESP32CLIManager(device_ip)
    print(f"已连接到设备 {device_ip}")
    print("输入 help 查看可用命令\n")

    while True:
        try:
            cmd = input(f"ESP32:{manager.current_path}> ").strip().split()
            if not cmd:
                continue

            if cmd[0] == "exit":
                break

            elif cmd[0] == "ls":
                manager.list_files(show_progress=True)

            elif cmd[0] == "cd":
                if len(cmd) < 2:
                    print("用法：cd <目录名或..>")
                    continue
                manager.change_directory(cmd[1])
                manager.list_files(show_progress=True)

            elif cmd[0] == "rm":
                if len(cmd) < 2:
                    print("用法：rm <文件路径> 或 rm -r <目录路径>")
                    continue
                if cmd[1] == '-r':
                    if len(cmd) < 3:
                        print("需要指定目录路径")
                        continue
                    manager.delete_directory(cmd[2])
                else:
                    manager.delete_file(cmd[1])

            elif cmd[0] == "mv":
                if len(cmd) < 3:
                    print("用法：mv <原路径> <新名称>")
                    continue
                manager.rename(cmd[1], cmd[2])

            elif cmd[0] == "mkdir":
                if len(cmd) < 2:
                    print("用法：mkdir <目录路径>")
                    continue
                if manager.create_directory(cmd[1]):
                    print("目录创建成功")

            elif cmd[0] == "switchfs":
                success, msg = manager.switch_file_system()
                if success:
                    print(f"文件系统已切换到: {msg}")
                else:
                    print(f"切换失败: {msg}")

            elif cmd[0] == "get":
                if len(cmd) < 3:
                    print("用法：get <远程路径> <本地目录> [-r]")
                    continue
                recursive = '-r' in cmd
                manager.download(cmd[1], cmd[2], recursive=recursive)

            elif cmd[0] == "put":
                if len(cmd) < 2:
                    print("用法：put <本地路径> [远程目录]")
                    continue
                remote_dir = cmd[2] if len(cmd) > 2 else None
                manager.upload(cmd[1], remote_dir)

            elif cmd[0] == "help":
                print("\n可用命令：")
                print("ls                 列出当前目录")
                print("cd <目录>          切换目录")
                print("get <路径> <目录> [-r]  下载文件/目录")
                print("rm <文件路径>       删除文件")
                print("rm -r <目录路径>    删除目录")
                print("mv <原路径> <新名>  重命名")
                print("mkdir <目录路径>    创建目录")
                print("switchfs           切换文件系统(LittleFS/SD)")
                print("put <路径> [目录]   上传文件/目录")
                print("exit               退出")
                print("help               显示帮助\n")

            else:
                print("未知命令，输入 help 查看可用命令")

        except Exception as e:
            print(f"错误：{str(e)}")
        except KeyboardInterrupt:
            print("\n操作已取消")
            continue

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="ESP32 文件管理命令行工具")
    parser.add_argument("ip", help="设备IP地址")
    parser.add_argument("-i", "--interactive", action="store_true", help="进入交互模式")
    args = parser.parse_args()

    try:
        if args.interactive:
            interactive_shell(args.ip)
        else:
            print("请使用 -i 参数进入交互模式")
    except ConnectionError as e:
        print(f"连接错误：{str(e)}")
        sys.exit(1)