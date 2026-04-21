#!/usr/bin/env python3
import socket

HOST = "127.0.0.1"
PORT = 6789

print(f"[Sensei Backend] Listening on {HOST}:{PORT}")

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, PORT))
    s.listen(5)
    while True:
        conn, addr = s.accept()
        print(f"[+] Connection from {addr}")
        data = b""
        while True:
            chunk = conn.recv(256)
            if not chunk: break
            data += chunk
            if b"\n" in data: break
        cmd = data.strip().decode(errors="replace")
        print(f"[>] Received: {cmd!r}")
        conn.sendall(b"SENSEI_OK\n")
        conn.close()
