import os
import subprocess
from SCons.Script import Import

Import("env")

def before_buildfs(source, target, env):
    print("\n>>>  before_buildfs: build-www")
    env.Execute("ruby shared/build-www.rb")

    print("\n>>> before_buildfs: copy assets")
    env.Execute("robocopy /MIR shared\\assets data\\assets")

    print("\n>>> before_buildfs: gzip assets")
    env.Execute("powershell -Command \"Get-ChildItem data\\ -Recurse -Include *.css,*.js,*.html | ForEach-Object { gzip -Force $_.FullName }\"")

    print("\n>>> before_buildfs: total size")
    env.Execute("dir /s data")

env.AddPreAction("buildfs", before_buildfs)

def after_buildfs(source, target, env):
    print("\n>>> after_buildfs: unzip assets")
    # 如果需要在构建后解压缩文件，可以在这里添加相应的解压缩命令
    pass


env.AddPostAction("buildfs", after_buildfs)


def after_build(source, target, env):
    
    # 定义要执行的Python脚本及其参数

    # 系统 Python 的绝对路径（替换为你的实际路径）
    system_python = r"C:\Users\admin\AppData\Local\Programs\Python\Python312\python.exe"  # 注意使用原始字符串（r""）避免转义问题

    # 要执行的 Python 脚本及参数（示例）
    script_path = "tools/idf_size.py"
    arg1 = r"C:\Users\admin\Desktop\LiClock-dev_multithread-main\.pio\build\esp32solo1\firmware.map"                     # 参数1

    # 构造命令（确保路径带空格时用双引号包裹）
    command = [
        system_python,
        f'"{script_path}"' if " " in script_path else script_path,  # 处理脚本路径中的空格
        "--format table",
        arg1
    ]

    # 执行命令（注意 shell=True 确保解析双引号）
    print("Running:", " ".join(command))
    print("获取固件镜像大小信息及各段大小使用情况")
    try:
        subprocess.run(" ".join(command), check=True, shell=True)  # 合并为字符串并启用 shell
        print("[POST BUILD] Script executed successfully!")
    except subprocess.CalledProcessError as e:
        print(f"[POST BUILD] Error: {e}")

# 注册钩子：构建完成后执行

env.AddPostAction("buildprog", after_build)