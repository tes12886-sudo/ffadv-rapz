# ffadv-rapz

FULL RAPZ injector for **Free Fire Advance** (`com.dts.freefireadv`).

Self-contained shell script + `ptrace` injector. No APK modification, no Magisk/Zygisk module.

## What is inside

| File | Description |
|---|---|
| `ffadv.sh` | Self-contained installer/injector. Put it at `/data/local/tmp/rap/ffadv.sh` on the device. |
| `build/libsheyan.so` | arm64-v8a cheat payload (ImGui menu + ESP + aimbot). Has a constructor auto-start. |
| `build/rapzinj` | arm64-v8a ptrace injector. Calls remote `mmap` + `dlopen` in the game process. |
| `src/` | Patched payload sources and injector C source. |
| `tools/build_ffadv_sh.py` | Rebuilds `ffadv.sh` from the compiled binaries. |

## Requirements

- Root (`su`)
- Android **arm64-v8a**
- Game installed: `com.dts.freefireadv`
- Game version used while building: `FFadv_68.56.0_1112_ID_advffgogo.apk`

## Usage

Push the script:

```sh
adb push ffadv.sh /data/local/tmp/rap/ffadv.sh
adb shell chmod 0755 /data/local/tmp/rap/ffadv.sh
```

Run on device:

```sh
su
sh /data/local/tmp/rap/ffadv.sh
```

The script writes `libsheyan.so` and `rapzinj` into `/data/local/tmp/rap/`, starts the game if needed, waits for `libil2cpp.so`, then injects.

## Auto update

The script checks `VERSION` from this repo on every run and downloads `ffadv.sh` from the latest GitHub Release when a newer build exists.

- Check: `https://raw.githubusercontent.com/tes12886-sudo/ffadv-rapz/main/VERSION`
- Download: `https://github.com/tes12886-sudo/ffadv-rapz/releases/latest/download/ffadv.sh`
- Fallback download: raw `main/ffadv.sh`
- Skip check with `sh /data/local/tmp/rap/ffadv.sh --no-update`

It also normalizes any duplicate filename such as `ffadv (1).sh` into `/data/local/tmp/rap/ffadv.sh` before continuing.

Every run writes a detailed log to:

```sh
/data/local/tmp/rap/inject.log
```

If inject fails, the script prints the full log automatically. It includes PID candidates, `libil2cpp.so` mapping, attach status, `PTRACE_GETREGSET` fallback, remote libc base, symbol offsets, `PTRACE_GET_SYSCALL_INFO` entry/exit for mmap, mmap result, path verification, trap stub, `dlopen` flags, `dlerror`, register restore and detach status.

Injector exit codes:

| Code | Meaning |
|---|---|
| 1 | Game process not found |
| 2 | `libil2cpp.so` not mapped within 60 seconds |
| 3 | `PTRACE_ATTACH` and `PTRACE_SEIZE` both failed |
| 4 | Initial `PTRACE_GETREGS` and `PTRACE_GETREGSET` both failed |
| 5 | Local or remote libc base not found |
| 6 | `mmap` / `dlopen` / `dlerror` symbol offset missing |
| 7 | Remote `mmap` returned an invalid address |
| 8 | Payload path too long |
| 9 | Writing path into remote memory failed |
| 10 | Writing trap stub failed |
| 11 | Remote `dlopen` returned NULL |
| 12 | `PTRACE_DETACH` failed |

Remove extracted files:

```sh
sh /data/local/tmp/rap/ffadv.sh uninstall
```

## Features

- Credit **FULL RAPZ** in notification, menu footer and payload strings.
- **Name ESP** — enabled by default.
- **Count ESP** — enabled by default, shows enemy count / `SAFE` at the top center.
- Box, line, distance and health-bar ESP toggles.
- Aimbot / aim assist controls.
- ImGui menu: draggable by the header, scrollable content, dark/light theme, 16 accent colors.

## How the injection works

1. `ffadv.sh` decodes the embedded base64 payload and injector into `/data/local/tmp/rap/`.
2. `rapzinj` finds the game PID and waits until `libil2cpp.so` is mapped.
3. It attaches with `PTRACE_ATTACH`, locates remote `libc.so`, then calls remote `mmap` and `dlopen` by hijacking a stopped thread's registers.
4. `dlopen` of `libsheyan.so` runs its `__attribute__((constructor))`, which starts the cheat thread.

## Rebuild

Android NDK 27.3.13750724 was used.

```sh
# payload
ndk-build NDK_PROJECT_PATH=<repo>/../ffadv-src/ffadv \
  APP_BUILD_SCRIPT=<repo>/../ffadv-src/ffadv/src/main/jni/Android.mk \
  NDK_APPLICATION_MK=<repo>/../ffadv-src/ffadv/src/main/jni/Application.mk

# injector
clang --target=aarch64-linux-android21 -O2 -fPIE -pie -Wall -Wextra \
  -o build/rapzinj src/rapz_injector.c -ldl -llog

# repackage script
python tools/build_ffadv_sh.py
```

## Credit

FULL RAPZ
