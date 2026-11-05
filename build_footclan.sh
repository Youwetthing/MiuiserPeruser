#!/data/data/com.termux/files/usr/bin/bash
set -e
cd "$(dirname "$0")"
mkdir -p bin
for f in foot_ipcshadowd foot_portwatchd foot_resurrectord; do
  echo "building $f"
  gcc -I. -Iinclude -Isrc/core -Isrc/core/include -Isrc/daemon -Isrc/footclan \
    src/footclan/$f.c src/core/fugitoid_log_impl.c src/core/log_safe.c \
    -o bin/$f
done
echo "done: bin/foot_*"
