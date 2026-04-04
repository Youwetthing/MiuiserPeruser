#!/usr/bin/env python3
import socket, struct, sys

sock_path = "/data/data/com.termux/files/usr/tmp/miuiserperuser_sewer.sock"
cmd = "echo hello from test_worker"

# connect
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sock_path)

# send length-prefixed command
data = cmd.encode()
s.sendall(struct.pack("!I", len(data)))
s.sendall(data)

# read 4-byte length
hdr = s.recv(4)
if len(hdr) < 4:
    print("no response header")
    sys.exit(1)
resp_len = struct.unpack("!I", hdr)[0]

# read response body
resp = b""
while len(resp) < resp_len:
    chunk = s.recv(resp_len - len(resp))
    if not chunk:
        break
    resp += chunk

print("response:", resp.decode())
s.close()
