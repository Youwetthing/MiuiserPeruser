# Compile the Core Daemon Logic
clang -O2 src/daemon/ipc.c src/daemon/daemon_core.c src/daemon/krang.c -lpthread -lsqlite3 -o bin/shredderd

# Compile the Resurrection Pair
clang -O2 src/footclan/foot_resurrectord.c src/daemon/daemon_common.c -o bin/foot_resurrectord
clang -O2 src/footclan/foot_ipcshadowd.c -o bin/foot_ipcshadowd

# Forge the Foot Jobs
clang -O2 src/footclan/foot_jobs.c src/footclan/jobs/foot_battery.c src/footclan/foot_sys.c -o bin/footrunner
