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

import os
import re
import sys
import time
import errno
import queue
import signal
import threading
import subprocess
import pty
from datetime import datetime
from xml.etree import ElementTree as ET
from dataclasses import dataclass
import urllib.request
import urllib.parse
import tempfile
import shutil
import argparse
import requests

@dataclass
class TestResult:
    name: str
    status: str
    message: str = ""

class QEMURunner:
    def __init__(self, cmd, timeout):
        self.cmd = cmd
        self.timeout = timeout
        self.process = None
        self.master_fd = None
        self.output_queue = queue.Queue()
        self.should_stop = False
        self.console_activated = False
        self.shell_ready = False
        self.error_detected = False
        self.error_message = ""
        self.test_results = []
        self.total_tests = 0
        self.current_test = 0
        self.test_completed = False
        
        # ANSI颜色代码
        self.BLUE = "\033[34m"
        self.GREEN = "\033[32m"
        self.RED = "\033[31m"
        self.RESET = "\033[0m"
        self.BOLD = "\033[1m"
        
        # 打开日志文件
        self.log_file = open('test-output.log', 'w')
        
    def _write_log(self, line):
        """写入日志文件"""
        try:
            # 写入日志时不去除前导空格
            self.log_file.write(line + '\n')
            self.log_file.flush()
        except Exception as e:
            print(f"Warning: Failed to write to log file: {e}")
            
    def _format_qemu_output(self, line):
        """格式化QEMU输出"""
        
        if '[I/Test]' in line:
            if 'Test Start' in line:
                return f"{self.BLUE}[QEMU] {line}{self.RESET}"
            elif 'Test Done' in line:
                return f"{self.BLUE}[QEMU] {self.BOLD}{line}{self.RESET}"
            else:
                return f"{self.BLUE}[QEMU] {line}{self.RESET}"
        elif '[E/Test]' in line:
            return f"{self.RED}[QEMU] {line}{self.RESET}"
        elif 'error' in line.lower():
            return f"{self.RED}[QEMU] {line}{self.RESET}"
        else:
            return f"{self.BLUE}[QEMU] {line}{self.RESET}"
            
    def _strip_ansi(self, text):
        """移除ANSI转义序列"""
        ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
        return ansi_escape.sub('', text)

    def _send_input(self, data):
        """发送输入到QEMU"""
        try:
            if isinstance(data, str):
                data = data.encode('utf-8')
            os.write(self.master_fd, data)
        except (OSError, IOError) as e:
            print(f"Error sending input: {e}")

    def _read_output(self):
        """读取QEMU输出"""
        buffer = ""
        poweroff_sent = False
        
        while True:
            try:
                # 读取输出
                data = os.read(self.master_fd, 1024).decode('utf-8', errors='ignore')
                buffer += data
                
                # 处理每一行
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    if line:  
                        # 移除ANSI转义序列并格式化输出
                        clean_line = self._strip_ansi(line)
                        formatted_line = self._format_qemu_output(clean_line)
                        print(formatted_line)
                        self._write_log(clean_line)
                        self._parse_test_line(clean_line.strip())  
                        
                # 检查控制台激活和shell就绪状态
                clean_buffer = self._strip_ansi(buffer)
                if not self.console_activated and "Please press Enter to activate this console" in clean_buffer:
                    self._send_input("\r\n")
                    self.console_activated = True
                    buffer = ""
                elif not self.shell_ready and ("root@Intewell-RTOS" in clean_buffer and "# " in clean_buffer):
                    self._send_input("cd /root && ./posix_test_case.elf\r\n")
                    self.shell_ready = True
                    buffer = ""
                elif self.test_completed and not poweroff_sent and "root@Intewell-RTOS" in clean_buffer and "# " in clean_buffer:
                    self._send_input("poweroff\r\n")
                    poweroff_sent = True
                    buffer = ""
            
            except (OSError, IOError) as e:
                if e.errno != errno.EAGAIN:
                    break
                time.sleep(0.1)
                
            # 检查进程是否终止
            if self.process.poll() is not None:
                break
                
    def stop(self):
        """停止QEMU进程"""
        try:
            # 首先设置停止标志
            self.should_stop = True
            
            if self.process:
                try:
                    # 先尝试正常终止
                    self.process.terminate()
                    try:
                        self.process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        # 如果超时，强制结束
                        self.process.kill()
                        self.process.wait(timeout=1)
                except Exception as e:
                    print(f"Warning: Error while terminating QEMU process: {e}")
        finally:
            # 确保关闭文件描述符
            try:
                if hasattr(self, 'master_fd') and self.master_fd is not None:
                    os.close(self.master_fd)
                    self.master_fd = None
            except OSError as e:
                if e.errno != errno.EBADF:  # 忽略已关闭的文件描述符错误
                    print(f"Warning: Error while closing master_fd: {e}")
            
            # 关闭日志文件
            try:
                if hasattr(self, 'log_file') and self.log_file is not None:
                    self.log_file.close()
                    self.log_file = None
            except Exception as e:
                print(f"Warning: Error while closing log file: {e}")
            
            # 清理进程对象
            self.process = None

    def run(self):
        """运行QEMU测试"""
        try:
            # 初始化错误消息存储
            self._last_error_message = None
            
            # 创建伪终端
            master_fd, slave_fd = pty.openpty()
            
            # 启动QEMU进程
            self.process = subprocess.Popen(
                self.cmd,
                stdin=slave_fd,
                stdout=slave_fd,
                stderr=slave_fd,
                preexec_fn=os.setsid
            )
            
            # 关闭子进程端
            os.close(slave_fd)
            
            # 保存主进程端
            self.master_fd = master_fd
            
            # 启动输出读取线程
            output_thread = threading.Thread(target=self._read_output)
            output_thread.daemon = True
            output_thread.start()
            
            # 等待测试完成或超时
            start_time = time.time()
            while not self.test_completed and time.time() - start_time < self.timeout:
                if self.process.poll() is not None:
                    print("\nQEMU process terminated unexpectedly")
                    break
                time.sleep(0.1)
            
            # 检查测试结果
            if not self.test_completed:
                print("\nTest timed out!")
                # 即使超时也生成测试报告
                if self.test_results:
                    print(f"\nTest Summary (Incomplete): {sum(1 for r in self.test_results if r.status == 'passed')} passed, "
                          f"{sum(1 for r in self.test_results if r.status == 'failed')} failed")
                    create_junit_xml(self.test_results)
                return False
            
            # 等待一段时间确保所有输出都被处理
            time.sleep(1)
            
            # 创建测试报告
            create_junit_xml(self.test_results)
            
            # 停止QEMU
            self.stop()
            
            # 等待输出线程结束
            output_thread.join(timeout=5)
            
            # 检查是否有失败的测试
            failed_tests = sum(1 for r in self.test_results if r.status == "failed")
            if failed_tests > 0:
                print(f"\n{self.RED}Test failed! {failed_tests} test(s) failed.{self.RESET}")
                return False
            
            print(f"\n{self.GREEN}Test finished successfully{self.RESET}")
            return True
            
        except Exception as e:
            print(f"\nError running test: {e}")
            # 如果发生异常但有测试结果，也生成报告
            if hasattr(self, 'test_results') and self.test_results:
                print(f"\nTest Summary (Error): {sum(1 for r in self.test_results if r.status == 'passed')} passed, "
                      f"{sum(1 for r in self.test_results if r.status == 'failed')} failed")
                create_junit_xml(self.test_results)
            return False
        finally:
            # 确保清理资源
            self.stop()

    def _parse_test_line(self, line):
        """解析完整的测试输出行"""
        # 匹配测试用例输出，允许前面有任意字符
        test_pattern = r'.*?\[(I|E)/Test\]\[(\w+)\]\[TEST (\d+)/(\d+)\]([\w_]+):([\w_]+)(?:\((.*?)\))?'
        match = re.match(test_pattern, line)
        if match:
            level, suite_name, case_num, total_cases, case_name, result, message = match.groups()
            self.total_tests = int(total_cases)
            self.current_test = int(case_num)
            
            # 如果是新的测试用例，重置错误消息
            if case_num != getattr(self, '_last_case_num', None):
                self._last_error_message = None
                self._last_case_num = case_num
            
            # 提取行首的错误消息（如果有）
            prefix_message = ""
            test_start = line.find("[I/Test]" if level == "I" else "[E/Test]")
            if test_start > 0:
                prefix_message = line[:test_start].strip()
            
            # 检查结果状态
            is_failed = (level == 'E' or result == 'failed' or 
                        (message and 'failed' in message.lower()))
            
            # 创建测试结果
            test_result = TestResult(
                name=f"{suite_name}.{case_name}",
                status="failed" if is_failed else "passed",
                message=message if message else ""
            )
            
            # 如果有前缀错误消息，添加到测试结果
            if prefix_message:
                test_result.message = (prefix_message + " - " + test_result.message 
                                     if test_result.message else prefix_message)
            
            # 检查是否已经有相同的测试用例
            existing_result = next((r for r in self.test_results 
                                  if r.name == test_result.name), None)
            if existing_result:
                # 更新现有结果
                existing_result.status = test_result.status
                if test_result.message:
                    existing_result.message = (existing_result.message + " - " + test_result.message 
                                            if existing_result.message else test_result.message)
            else:
                # 添加新结果
                self.test_results.append(test_result)
                
                # 如果有未处理的错误消息，添加到当前测试结果
                if hasattr(self, '_last_error_message') and self._last_error_message:
                    test_result.message = (test_result.message + " - " + self._last_error_message 
                                         if test_result.message else self._last_error_message)
                    self._last_error_message = None
            
            return True
        
        # 检查是否是错误消息
        error_pattern = r'.*error.*'
        if re.match(error_pattern, line.lower()):
            # 如果当前有测试结果，将错误消息添加到最后一个测试结果
            if self.test_results:
                last_result = self.test_results[-1]
                if last_result.status == "failed":  # 只添加到失败的测试用例
                    last_result.message = (last_result.message + " - " + line.strip() 
                                         if last_result.message else line.strip())
            return True
            
        # 匹配测试总结
        summary_pattern = r'\[I/Test\]============== Test Done! Pass (\d+) Failed (\d+) =============='
        match = re.match(summary_pattern, line)
        if match:
            passed, failed = map(int, match.groups())
            print(f"\n{self.GREEN if failed == 0 else self.RED}Test Summary: {passed} passed, {failed} failed{self.RESET}")
            self.test_completed = True
            return True
            
        return False

def download_test_case(arch, version):
    """下载测试用例"""
    url = f"http://192.168.11.87/repository/download/Intewell-N/test-case/{version}/{arch}/posix_test_case.elf"
    local_path = "posix_test_case.elf"
    
    print(f"Downloading test case from {url}")
    response = requests.get(url)
    if response.status_code == 200:
        with open(local_path, "wb") as f:
            f.write(response.content)
        return local_path
    else:
        raise Exception(f"Failed to download test case: {response.status_code}")

def copy_to_rootfs(rootfs_image, test_case):
    """将测试用例拷贝到rootfs"""
    print("Copying test case to rootfs...")
    try:
        # 使用 e2cp 拷贝文件到 rootfs 的 /root 目录
        subprocess.run(["e2cp", "-P", "0755", test_case, f"{rootfs_image}:/root/posix_test_case.elf"],
                      check=True)
    except subprocess.CalledProcessError as e:
        print(f"Failed to copy test case: {e}")
        raise

# 架构配置
ARCH_CONFIG = {
    "arm": {
        "qemu_system": "qemu-system-arm",
        "cpu_model": "cortex-a15",
        "machine": "virt,gic-version=3",
        "memory": "1G",
        "smp": "2",
        "device":"virtio-blk-device,drive=rootfs",
    },
    "aarch64": {
        "qemu_system": "qemu-system-aarch64",
        "cpu_model": "cortex-a57",
        "machine": "virt,gic-version=3",
        "memory": "1G",
        "smp": "2",
        "device":"virtio-blk-device,drive=rootfs",
    },
    "x86_64": {
        "qemu_system": "qemu-system-x86_64",
        "cpu_model": "max",
        "machine": "type=pc-q35-6.2,sata=off",
        "memory": "1G",
        "smp": "2",
        "device":"virtio-blk-pci,drive=rootfs",
    },
}

def get_qemu_command(arch, kernel, rootfs, timeout):
    """根据架构生成QEMU命令"""
    if arch not in ARCH_CONFIG:
        raise ValueError(f"Unsupported architecture: {arch}. Supported: {', '.join(ARCH_CONFIG.keys())}")
    
    config = ARCH_CONFIG[arch]
    
    cmd = [
        config["qemu_system"],
        "-M", config["machine"],
        "-cpu", config["cpu_model"],
        "-m", config["memory"],
        "-smp", config["smp"],
        "-kernel", kernel,
        "-drive", f"file={rootfs},format=raw,if=none,id=rootfs",
        "-device", config["device"],
        "-nographic"
    ]
    
    print(f"cmd:{cmd}")
    return cmd

def create_junit_xml(results, output_file="test-results.xml"):
    """创建JUnit XML报告"""
    # 创建根元素
    testsuites = ET.Element("testsuites")
    
    # 按测试套件分组
    suites = {}
    for result in results:
        suite_name = result.name.split('.')[0]
        if suite_name not in suites:
            suites[suite_name] = []
        suites[suite_name].append(result)
    
    # 创建每个测试套件
    for suite_name, suite_results in suites.items():
        testsuite = ET.SubElement(testsuites, "testsuite")
        testsuite.set("name", suite_name)
        testsuite.set("tests", str(len(suite_results)))
        testsuite.set("failures", str(sum(1 for r in suite_results if r.status == "failed")))
        testsuite.set("errors", "0")
        testsuite.set("skipped", "0")
        testsuite.set("time", "0")
        testsuite.set("timestamp", datetime.now().isoformat())
        
        # 添加每个测试用例
        for result in suite_results:
            testcase = ET.SubElement(testsuite, "testcase")
            testcase.set("classname", suite_name)
            testcase.set("name", result.name.split('.')[1])
            testcase.set("time", "0")
            
            if result.status == "failed":
                failure = ET.SubElement(testcase, "failure")
                failure.set("message", result.message)
                failure.text = result.message
    
    # 创建XML树
    tree = ET.ElementTree(testsuites)
    
    # 格式化XML
    from xml.dom import minidom
    xmlstr = minidom.parseString(ET.tostring(testsuites)).toprettyxml(indent="    ")
    
    # 写入文件
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(xmlstr)
    
    print(f"\nTest report written to {output_file}")

def main():
    parser = argparse.ArgumentParser(description="Run QEMU tests")
    parser.add_argument("--kernel", required=True, help="Path to kernel image")
    parser.add_argument("--arch", required=True, help="Target architecture")
    parser.add_argument("--timeout", type=int, default=300, help="Timeout in seconds")
    parser.add_argument("--rootfs", required=True, help="Path to rootfs image")
    parser.add_argument("--version", required=True, help="Test case version")
    
    args = parser.parse_args()
    
    try:
        # 下载测试用例
        print("Downloading test case...")
        test_case = download_test_case(args.arch, args.version)
        
        # 将测试用例拷贝到rootfs
        print("Preparing rootfs...")
        rootfs_copy = f"{args.rootfs}.copy"
        shutil.copy2(args.rootfs, rootfs_copy)
        try:
            copy_to_rootfs(rootfs_copy, test_case)
        except Exception as e:
            os.unlink(rootfs_copy)
            raise
        
        # 运行QEMU测试
        print("Starting QEMU test...")
        qemu_cmd = get_qemu_command(args.arch, args.kernel, rootfs_copy, args.timeout)
        qemu = QEMURunner(qemu_cmd, args.timeout)
        
        try:
            success = qemu.run()
        finally:
            qemu.stop()
            os.unlink(rootfs_copy)
            
    except Exception as e:
        print(f"Error: {str(e)}")
        sys.exit(1)

if __name__ == "__main__":
    main()
