# MiuiserPeruser: Syndicate Haus Framework

MiuiserPeruser is a specialized framework for system interaction and IPC, often referred to as "Syndicate Haus".

## Architecture

The system is built on an IPC-heavy architecture using C for performance and low-level system access, with Python and Shell scripts for higher-level logic.

### Key Components

- **src/daemon/**: Contains the core daemon logic, IPC handling (`ipc.c`), and protocol definitions (`splinter_protocol.c`).
- **src/core/**: Base capabilities and "sensei" logic.
- **src/backend/**: Implementation of different backends for system interaction (ADB, Rish, Shizuku).
- **law_and_order:adb/**: The "Judicial System" (Court/Jailhouse) orchestration layer. Handles high-level system state tracking, event logging, and daemon management.
- **bin/**: Destination for compiled binaries.

## Build System

The primary build mechanism is the `Build_All.sh` script, which manually orchestrates `gcc` to compile objects and link the final `miuiserperuser` and `mouser` binaries.

### Build Workflow

1. Navigate to the `MiuiserPeruser/` directory.
2. Run `./Build_All.sh`.
3. Verify binaries in `bin/`.

*Note: While a `Makefile` and `CMakeLists.txt` exist, `Build_All.sh` is the definitive build script used for structural reinforcement and IPC linkage.*

## Conventions

- **IPC**: Communication follows the "Splinter" protocol.
- **Binaries**: Core binaries like `cpud`, `foot_dual`, etc., are often pre-compiled or managed via the build script.
- **Permissions**: Binaries in `bin/` should be chmodded to `700` after successful builds.
