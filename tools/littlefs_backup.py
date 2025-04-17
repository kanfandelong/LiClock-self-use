import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import requests
import os
from concurrent.futures import ThreadPoolExecutor
from urllib.parse import quote

class ESP32FileManager:
    def __init__(self, root):
        self.root = root
        self.root.title("ESP32文件管理")
        self.device_ip = ""
        self.current_path = "/"
        self.current_files = []
        
        self.create_widgets()
        self.executor = ThreadPoolExecutor(max_workers=4)

    def create_widgets(self):
        """创建GUI界面组件"""
        # IP地址输入区
        ip_frame = ttk.Frame(self.root)
        ip_frame.pack(pady=10, padx=10, fill=tk.X)
        
        ttk.Label(ip_frame, text="设备IP:").pack(side=tk.LEFT)
        self.ip_entry = ttk.Entry(ip_frame, width=20)
        self.ip_entry.pack(side=tk.LEFT, padx=5)
        
        # 导航按钮区
        nav_frame = ttk.Frame(self.root)
        nav_frame.pack(pady=5, fill=tk.X)
        ttk.Button(nav_frame, text="刷新列表", command=lambda: self.refresh_list()).pack(side=tk.LEFT, padx=5)
        ttk.Button(nav_frame, text="返回上级", command=self.navigate_up).pack(side=tk.LEFT, padx=5)

        # 操作按钮区
        btn_frame = ttk.Frame(self.root)
        btn_frame.pack(pady=5, fill=tk.X)
        ttk.Button(btn_frame, text="下载选中", command=self.download_selected).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="下载所有", command=self.backup_all).pack(side=tk.LEFT, padx=5)  # 新增按钮
        ttk.Button(btn_frame, text="批量上传", command=self.restore_files).pack(side=tk.LEFT, padx=5)

        # 文件列表
        self.tree = ttk.Treeview(self.root, columns=('type', 'size'), show='headings')
        self.tree.heading('#0', text='文件名')
        self.tree.heading('type', text='名称')
        self.tree.heading('size', text='大小')
        self.tree.bind('<Double-1>', self.on_item_doubleclick)
        self.tree.pack(expand=True, fill=tk.BOTH, padx=10)
        
        # 进度条
        self.progress = ttk.Progressbar(self.root, orient=tk.HORIZONTAL, mode='determinate')
        self.progress.pack(fill=tk.X, padx=10, pady=5)
        
        # 状态栏
        self.status = ttk.Label(self.root, text="就绪")
        self.status.pack(fill=tk.X, padx=10)

    def api_url(self, path):
        """构建完整API地址"""
        return f"http://{self.device_ip}/edit{path}"

    def on_item_doubleclick(self, event):
        """处理文件夹双击事件"""
        selected = self.tree.selection()
        if not selected:
            return
        
        item = selected[0]
        item_type = self.tree.item(item, "tags")[0]
        name = self.tree.item(item, "text")
        
        if item_type == "folder":
            # 修复路径拼接问题
            base_path = self.current_path.rstrip('/')
            new_path = f"{base_path}/{name}"
            self.refresh_list(new_path)

    def navigate_up(self):
        """返回上级目录"""
        if self.current_path == "/":
            return
        # 修复路径显示问题
        parent_path = os.path.dirname(self.current_path.rstrip('/'))
        self.refresh_list(parent_path + "/" if parent_path != "/" else "/")

    def refresh_list(self, path=None):
        """刷新文件列表"""
        self.device_ip = self.ip_entry.get().strip()
        if not self.device_ip:
            messagebox.showerror("错误", "请输入设备IP地址")
            return
        
        target_path = path or self.current_path
        try:
            encoded_path = quote(target_path)
            response = requests.get(self.api_url(f"?list={encoded_path}"), timeout=5)
            if response.status_code == 200:
                self.current_files = response.json()
                # 规范化当前路径显示
                self.current_path = target_path.rstrip('/') + '/'
                if self.current_path == "//":  # 根目录处理
                    self.current_path = "/"
                self.update_file_list()
                self.update_status(f"当前路径: {self.current_path}")
            else:
                raise Exception(f"HTTP错误代码: {response.status_code}")
        except Exception as e:
            self.handle_error("获取列表失败", e)

    def update_file_list(self):
        """更新Treeview显示，区分文件夹和文件"""
        self.tree.delete(*self.tree.get_children())
        for item in self.current_files:
            item_type = item['name'] if item['type'] == "folder" else item['name']
            size = self.format_size(item['size']) if item['type'] == "file" else "-"
            self.tree.insert('', 'end', 
                text=item['name'],
                values=(item_type, size),
                tags=(item['type'],)
            )
    def download_selected(self):
        """处理下载选中文件的逻辑"""
        selected_items = self.tree.selection()
        if not selected_items:
            messagebox.showwarning("警告", "请先选择要下载的文件")
            return
    
        paths = [self.tree.item(item, "text") for item in selected_items if self.tree.item(item, "tags")[0] == "file"]
        if not paths:
            messagebox.showwarning("警告", "请选择文件而不是文件夹")
            return
    
        dest_dir = filedialog.askdirectory()
        if not dest_dir:
            return
    
        self.executor.submit(self.backup_files, paths, dest_dir)
    def backup_files(self, paths, dest_dir):
        """实际下载逻辑"""
        try:
            total = len(paths)
            for i, path in enumerate(paths, 1):
                full_remote_path = os.path.join(self.current_path, path).replace("\\", "/")
                local_path = os.path.join(dest_dir, path)
                self.download_file(full_remote_path, local_path)
                self.update_progress(i/total*100)
            self.update_status(f"下载到: {dest_dir}")
        except Exception as e:
            self.handle_error("下载失败", e)
            
    def backup_all(self):
        """新增：下载所有文件"""
        if not messagebox.askyesno("确认", "确定要下载当前目录所有文件吗？"):
            return
        
        dest_dir = filedialog.askdirectory()
        if not dest_dir:
            return
        
        self.executor.submit(self._backup_all_files, dest_dir)

    def _backup_all_files(self, dest_dir):
        """递归下载所有文件"""
        try:
            all_files = self._get_all_files(self.current_path)
            total = len(all_files)
            for i, remote_path in enumerate(all_files, 1):
                local_path = os.path.join(dest_dir, remote_path.lstrip('/'))
                self.download_file(remote_path, local_path)
                self.update_progress(i/total*100)
            self.update_status(f"全量下载完成到: {dest_dir}")
        except Exception as e:
            self.handle_error("下载失败", e)

    def _get_all_files(self, path):
        """递归获取所有文件路径"""
        files = []
        try:
            response = requests.get(self.api_url(f"?list={quote(path)}"), timeout=5)
            if response.status_code == 200:
                for item in response.json():
                    full_path = f"{path.rstrip('/')}/{item['name']}"
                    if item['type'] == "folder":
                        files.extend(self._get_all_files(full_path + "/"))
                    else:
                        files.append(full_path)
        except Exception as e:
            self.handle_error("获取文件列表失败", e)
        return files

    # 以下原有方法保持不变（backup_files, _backup_files, download_file, restore_files, 
    # _restore_files, upload_file, format_size, update_progress, update_status, handle_error）
    def restore_files(self):
        """处理上传文件的逻辑"""
        src_dir = filedialog.askdirectory(title="选择需要上传的文件所在目录")
        if not src_dir:
            return
    
        self.executor.submit(self._restore_files, src_dir)

    def _restore_files(self, src_dir):
        """实际上传逻辑"""
        try:
            file_count = sum([len(files) for _, _, files in os.walk(src_dir)])
            current = 0
        
            for root, _, files in os.walk(src_dir):
                for file in files:
                    local_path = os.path.join(root, file)
                    relative_path = os.path.relpath(local_path, src_dir)
                    remote_path = os.path.join(self.current_path, relative_path).replace("\\", "/")
                    self.upload_file(local_path, remote_path)
                    current += 1
                    self.update_progress(current/file_count*100)
            self.update_status(f"上传完成: {src_dir}")
        except Exception as e:
            self.handle_error("上传失败", e)
            
    def upload_file(self, local_path, remote_path):
        """上传单个文件"""
        with open(local_path, 'rb') as f:
            # 关键修改：使用三元组指定文件名参数
            files = {
                'data': (remote_path, f)  # 第二参数是路径而非文件名
            }
            response = requests.post(
                self.api_url(""),
                files=files,
                # 添加必要的验证参数（根据ESP32代码实际情况）
                params={'path': remote_path}  # 部分实现需要额外path参数
            )
        response.raise_for_status()
            
    def download_file(self, remote_path, local_path):
        """下载单个文件"""
        os.makedirs(os.path.dirname(local_path), exist_ok=True)
        encoded_path = quote(remote_path)
        response = requests.get(self.api_url(f"?download={encoded_path}"), stream=True)
        response.raise_for_status()
        with open(local_path, 'wb') as f:
            for chunk in response.iter_content(chunk_size=8192):
                f.write(chunk)
                
    def format_size(self, size):
        """格式化文件大小"""
        if size == 0:
            return "0 B"
        for unit in ['B', 'KB', 'MB']:
            if size < 1024.0:
                return f"{size:.1f} {unit}"
            size /= 1024.0
        return f"{size:.1f} GB"

    def update_progress(self, value):
        """更新进度条"""
        self.root.after(0, lambda: self.progress.configure(value=value))

    def update_status(self, text):
        """更新状态栏"""
        self.root.after(0, lambda: self.status.config(text=text))

    def handle_error(self, title, error):
        """统一错误处理"""
        self.root.after(0, lambda: messagebox.showerror(title, str(error)))

if __name__ == "__main__":
    root = tk.Tk()
    app = ESP32FileManager(root)
    root.mainloop()
