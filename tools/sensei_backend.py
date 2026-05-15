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

        data = conn.recv(1024)
        print(f"[>] Received: {data!r}")

        # Minimal valid response for port_bridge_request_basic_info()
        response = b"SENSEI_OK\n"
        conn.sendall(response)
        print(f"[<] Sent: {response!r}")

        conn.close()
