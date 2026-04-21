import sys, os, time, mmap, struct, socket
from pathlib import Path
from daemons.april import APRIL_SIZE, OFFSETS, read_flag

BASE_DIR          = "/data/data/com.termux/files/home/MiuiserPeruser"
APRIL_BIN         = BASE_DIR + "/Database/april.bin"
SEWER_SOCK        = BASE_DIR + "/pipes/turtlecom.sock"
TCP_FALLBACK_ADDR = ("127.0.0.1", 6789)
TIMEOUT           = 5.0

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
        self.mm = mmap.mmap(self.file_obj.fileno(), APRIL_SIZE)
        print("[Powerhouse] April Table connected — shared memory active")

    def get_flag(self, flag):
        return read_flag(self.mm, flag)

    def get_system_load(self):
        self.mm.seek(0)
        return struct.unpack('f', self.mm.read(4))[0]

    def _read_line(self, sock):
        resp = b""
        while True:
            byte = sock.recv(1)
            if not byte: break
            resp += byte
            if byte == b"\n": break
        return resp.strip().decode(errors="replace")

    def send_command(self, cmd):
        sock = None
        try:
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.settimeout(TIMEOUT)
            sock.connect(SEWER_SOCK)
            print("[Powerhouse] Using UNIX socket")
        except Exception as e:
            print(f"[Powerhouse] UNIX socket failed: {e}")
            if sock: sock.close()
            sock = None

        if sock is None:
            if self.get_flag("TCP_FALLBACK") == 0:
                print("[Powerhouse] TCP fallback disabled by policy")
                return None
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(TIMEOUT)
                sock.connect(TCP_FALLBACK_ADDR)
                print("[Powerhouse] Using TCP fallback")
            except Exception as e:
                print(f"[Powerhouse] TCP fallback failed: {e}")
                if sock: sock.close()
                return None

        try:
            sock.sendall(cmd.encode() + b"\n")
            return self._read_line(sock)
        except Exception as e:
            print(f"[Powerhouse] send_command error: {e}")
            return None
        finally:
            sock.close()

    def start_loop(self):
        self.active = True
        print("\n[Powerhouse] Syndicate engine running")
        while self.active:
            if self.get_flag("SYSTEM_LOCK") == 1:
                time.sleep(1)
                continue
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
