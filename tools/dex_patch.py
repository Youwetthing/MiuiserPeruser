import os

path = os.path.expanduser("~/MiuiserPeruser/rish_shizuku.dex")
old_id = b"moe.shizuku.privileged.api"
new_id = b"com.termux"

# Padding: new_id must be the same length as old_id to keep the DEX valid
# We pad with null bytes (\x00) to match the original 26-character length
padded_id = new_id + b"\x00" * (len(old_id) - len(new_id))

with open(path, "rb") as f:
    data = f.read()

if old_id in data:
    print(f"[*] Found target ID. Performing surgery...")
    new_data = data.replace(old_id, padded_id)
    with open(path, "wb") as f:
        f.write(new_data)
    print("[+] Surgery successful. DEX patched.")
else:
    print("[!] Target ID not found. The DEX might already be patched or corrupted.")
