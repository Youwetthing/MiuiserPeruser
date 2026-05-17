import sys, os, time, mmap, struct, socket
from pathlib import Path

APRIL_BIN = "/data/data/com.termux/files/home/tmp/miuiser_april.bin"
SEWER_SOCK = "/data/local/tmp/miuiserperuser_sewer.sock"
TCP_FALLBACK = ("127.0.0.1", 6789)

class PowerhouseDaemon:
    def __init__(self):
        self.active = False
        self.setup_memory_map()

    def setup_memory_map(self):
        Path(APRIL_BIN).parent.mkdir(parents=True, exist_ok=True)
        if not os.path.exists(APRIL_BIN):
            with open(APRIL_BIN, "wb") as f:
                f.write(b"\x00" * 4096)
        self.file_obj = open(APRIL_BIN, "r+b")
        self.mm = mmap.mmap(self.file_obj.fileno(), 4096)
        print("[Powerhouse] April Table connected — shared memory active")

    def get_system_load(self):
        self.mm.seek(0)
        return struct.unpack('f', self.mm.read(4))[0]

    def send_command(self, cmd):
        # Try UNIX socket first
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(1.0)
            s.connect(SEWER_SOCK)
        except:
            # Fall back to TCP
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.settimeout(1.0)
                s.connect(TCP_FALLBACK)
            except Exception as e:
                print(f"[Powerhouse] Both sockets failed: {e}")
                return None

        data = cmd.encode()
        s.sendall(struct.pack("!I", len(data)))
        s.sendall(data)

        hdr = s.recv(4)
        if len(hdr) < 4:
            s.close()
            return None
        resp_len = struct.unpack("!I", hdr)[0]
        resp = b""
        while len(resp) < resp_len:
            chunk = s.recv(resp_len - len(resp))
            if not chunk:
                break
            resp += chunk
        s.close()
        return resp.decode()

    def start_loop(self):
        self.active = True
        print("\n[Powerhouse] Syndicate engine running — daemons toggleable via dashboard")

        while self.active:
            load = self.get_system_load()
            print(f"[Powerhouse] Load: {load:.2f}", end="\r")
            time.sleep(0.5)

    def stop_loop(self):
        self.active = False
        self.mm.close()
        self.file_obj.close()
        print("\n[Powerhouse] Engine stopped.")

if __name__ == "__main__":
    try:
        engine = PowerhouseDaemon()
        engine.start_loop()
    except KeyboardInterrupt:
        engine.stop_loop()
