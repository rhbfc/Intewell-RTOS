#!/usr/bin/env python3
# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# -*- coding: utf-8 -*-

import os
import sys
import ftplib
import argparse

def ensure_remote_dir(ftp, remote_dir):
    """确保远程目录存在，如果不存在则创建"""
    if remote_dir == '/' or remote_dir == '':
        return
    
    try:
        ftp.cwd(remote_dir)
        ftp.cwd('/')  # 返回根目录
    except ftplib.error_perm:
        # 递归创建父目录
        ensure_remote_dir(ftp, os.path.dirname(remote_dir))
        try:
            ftp.mkd(remote_dir)
        except ftplib.error_perm:
            pass  # 目录可能已被其他进程创建

def upload_file(ftp_host, ftp_port, username, password, local_file, remote_path):
    """
    上传文件到FTP服务器
    
    参数:
        ftp_host: FTP服务器地址
        ftp_port: FTP服务器端口
        username: FTP用户名
        password: FTP密码
        local_file: 本地文件路径
        remote_path: 远程文件路径
    """
    if not os.path.exists(local_file):
        print(f"错误: 本地文件 '{local_file}' 不存在")
        return False

    try:
        # 连接到FTP服务器
        ftp = ftplib.FTP()
        ftp.connect(ftp_host, ftp_port)
        ftp.login(username, password)
        print(f"已连接到FTP服务器 {ftp_host}")

        # 确保远程目录存在
        remote_dir = os.path.dirname(remote_path)
        ensure_remote_dir(ftp, remote_dir)

        # 上传文件
        with open(local_file, 'rb') as f:
            ftp.storbinary(f'STOR {remote_path}', f)
        
        print(f"文件 '{local_file}' 已成功上传到 '{remote_path}'")
        ftp.quit()
        return True

    except ftplib.all_errors as e:
        print(f"FTP错误: {str(e)}")
        return False
    except Exception as e:
        print(f"发生错误: {str(e)}")
        return False

def upload_directory(ftp_host, ftp_port, username, password, local_dir, remote_dir):
    """
    上传整个目录到FTP服务器
    
    参数:
        ftp_host: FTP服务器地址
        ftp_port: FTP服务器端口
        username: FTP用户名
        password: FTP密码
        local_dir: 本地目录路径
        remote_dir: 远程目录路径
    """
    if not os.path.isdir(local_dir):
        print(f"错误: 本地目录 '{local_dir}' 不存在或不是目录")
        return False

    try:
        # 连接到FTP服务器
        ftp = ftplib.FTP()
        ftp.connect(ftp_host, ftp_port)
        ftp.login(username, password)
        print(f"已连接到FTP服务器 {ftp_host}")

        success = True
        for root, dirs, files in os.walk(local_dir):
            # 计算相对路径
            rel_path = os.path.relpath(root, local_dir)
            if rel_path == '.':
                current_remote_dir = remote_dir
            else:
                current_remote_dir = os.path.join(remote_dir, rel_path).replace('\\', '/')

            # 确保远程目录存在
            ensure_remote_dir(ftp, current_remote_dir)
            
            # 上传文件
            for file in files:
                local_file = os.path.join(root, file)
                remote_file = os.path.join(current_remote_dir, file).replace('\\', '/')
                
                try:
                    with open(local_file, 'rb') as f:
                        ftp.storbinary(f'STOR {remote_file}', f)
                    print(f"文件 '{local_file}' 已成功上传到 '{remote_file}'")
                except Exception as e:
                    print(f"上传文件 '{local_file}' 失败: {str(e)}")
                    success = False

        ftp.quit()
        return success

    except ftplib.all_errors as e:
        print(f"FTP错误: {str(e)}")
        return False
    except Exception as e:
        print(f"发生错误: {str(e)}")
        return False

def main():
    parser = argparse.ArgumentParser(description='FTP文件/目录上传工具')
    parser.add_argument('--host', required=True, help='FTP服务器地址')
    parser.add_argument('--port', type=int, default=21, help='FTP服务器端口(默认: 21)')
    parser.add_argument('--user', required=True, help='FTP用户名')
    parser.add_argument('--password', required=True, help='FTP密码')
    parser.add_argument('--local-path', required=True, help='要上传的本地文件或目录路径')
    parser.add_argument('--remote-path', required=True, help='远程存储路径')

    args = parser.parse_args()

    if os.path.isdir(args.local_path):
        success = upload_directory(
            args.host,
            args.port,
            args.user,
            args.password,
            args.local_path,
            args.remote_path
        )
    else:
        success = upload_file(
            args.host,
            args.port,
            args.user,
            args.password,
            args.local_path,
            args.remote_path
        )

    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()
