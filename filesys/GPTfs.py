#!/usr/bin/env python3

import os
import sys
import errno
import logging
from stat import S_IFDIR, S_IFREG
from time import time
from threading import Lock
from fuse import FUSE, FuseOSError, Operations
from openai import OpenAI, APIError
from dotenv import load_dotenv

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# 加载环境变量 (例如 .env 文件中的 DEEPSEEK_API_KEY)
load_dotenv()

class GPTfs(Operations):
    """
    一个使用 FUSE 实现的、与 DeepSeek API 交互的文件系统。
    - 根目录下的每个目录代表一个对话会话。
    - 每个会话目录下有三个文件: input, output, error。
    - 对 input 文件的写入和关闭会触发对 DeepSeek API 的调用。
    """
    def __init__(self):
        self.sessions = {}  
        self.rwlock = Lock()  
        now = time()
        self.dir_attr = dict(st_mode=(S_IFDIR | 0o755), st_ctime=now, st_mtime=now, st_atime=now, st_nlink=2)
        self.file_attr = dict(st_mode=(S_IFREG | 0o644), st_ctime=now, st_mtime=now, st_atime=now, st_nlink=1)
        api_key = my_api
        self.api_client = OpenAI(
                    api_key=api_key,
                    base_url="https://api.deepseek.com/v1"
                )
        logging.info("DeepSeek API 客户端初始化成功。")


        

    def _parse_path(self, path):
        parts = path.strip('/').split('/')
        if parts == ['']:
            return None, None 
        if len(parts) == 1:
            return parts[0], None 
        if len(parts) == 2:
            return parts[0], parts[1] 
        return None, None

    def getattr(self, path, fh=None):
        session_id, filename = self._parse_path(path)

        if session_id is None and filename is None: 
            return self.dir_attr

        with self.rwlock:
            if session_id not in self.sessions:
                raise FuseOSError(errno.ENOENT) 
            
            if filename is None: 
                return self.dir_attr

            if filename in ['input', 'output', 'error']:
                attrs = self.file_attr.copy()
                content = self.sessions[session_id].get(filename, b'')
                attrs['st_size'] = len(content)
                return attrs

        raise FuseOSError(errno.ENOENT)

    def readdir(self, path, fh):
        dirents = ['.', '..']
        session_id, _ = self._parse_path(path)

        if session_id is None: # 根目录
            with self.rwlock:
                dirents.extend(self.sessions.keys())
        else: # 会话目录
            dirents.extend(['input', 'output', 'error'])
        
        for r in dirents:
            yield r

    def mkdir(self, path, mode):
        session_id, _ = self._parse_path(path)

        with self.rwlock:
            if session_id in self.sessions:
                raise FuseOSError(errno.EEXIST) 
            
            self.sessions[session_id] = { 'input': b'', 'output': b'', 'error': b'' }
            logging.info(f"创建新会话: {session_id}")
        
        self.dir_attr['st_nlink'] += 1
        return 0

    def rmdir(self, path):
        session_id, _ = self._parse_path(path)
        with self.rwlock:
            if session_id not in self.sessions:
                raise FuseOSError(errno.ENOENT)
            
            del self.sessions[session_id]
            logging.info(f"删除会话: {session_id}")

        self.dir_attr['st_nlink'] -= 1
        return 0

    def open(self, path, flags):
        return 0

    
    def release(self, path, fh):
        session_id, filename = self._parse_path(path)

        if filename == 'input':
            logging.info(f"DeepSeek API 调用")
            
            prompt = ""
            with self.rwlock:
                prompt = self.sessions[session_id]['input'].decode('utf-8').strip()
                self.sessions[session_id]['output'] = b''
                self.sessions[session_id]['error'] = b''
            
            try:
                logging.info(f"向 DeepSeek 发送 prompt: '{prompt[:50]}...'")
                completion = self.api_client.chat.completions.create(
                    model="deepseek-chat",
                    messages=[
                        {"role": "user", "content": prompt}
                    ]
                )
                response_text = completion.choices[0].message.content
                logging.info("成功从 DeepSeek 获取回复。")
                with self.rwlock:
                    self.sessions[session_id]['output'] = response_text.encode('utf-8')

            except APIError as e:
                err_msg = f"DeepSeek API 错误: {e}"
                logging.error(err_msg)
                with self.rwlock:
                    self.sessions[session_id]['error'] = err_msg.encode('utf-8')
            except Exception as e:
                err_msg = f"发生未知错误: {e}"
                logging.error(err_msg)
                with self.rwlock:
                    self.sessions[session_id]['error'] = err_msg.encode('utf-8')
        return 0

def main(mountpoint):
    FUSE(GPTfs(), mountpoint, nothreads=False, foreground=True)
    print("文件系统已卸载。")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"用法: {sys.argv[0]} <mountpoint>")
        sys.exit(1)
    
    mountpoint = sys.argv[1]
    if not os.path.isdir(mountpoint):
        os.makedirs(mountpoint, exist_ok=True)
        
    main(mountpoint)