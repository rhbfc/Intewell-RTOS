# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import sys
import os
import shutil
import chardet

def foreachDir(root, funcFile, funcDir):
    for name in os.listdir(root):
        if name == '.git':
            continue
        if name == 'build-env':
            continue
        if name == 'build':
            continue
        fullname = os.path.join(root, name)
        if os.path.isfile(fullname):
            funcFile(fullname)
        else:
            if not funcDir(fullname):
                foreachDir(fullname, funcFile, funcDir)

def funcFile(path):
    if '.a' in path:
        return
    if '.pdf' in path:
        return
    if '.py' in path:
        return
    if '.pyc' in path:
        return

    shutil.copy(path, path + '.bak')
    with open(path + '.bak', 'rb') as f:
        try:
            ctx = f.read()
            det = chardet.detect(ctx)
            if det['encoding'] != 'utf-8' and det['encoding'] != 'ascii':
                print('convert ' + path + ' ' + det['encoding'] + ' to utf-8')
                with open(path, 'wb') as ft:
                    ft.write(ctx.decode(det['encoding']).encode('utf-8'))
        except:
            shutil.copy(path + '.bak', path)
            print('error on ' + path)

    os.remove(path + '.bak')

def funcDir(path):
    return False

foreachDir(os.path.abspath(sys.argv[1]), funcFile, funcDir)
