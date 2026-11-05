#!/usr/bin/env python3
"""把真实的 H7 固件编译成 WebAssembly，内嵌到 sim_3d/firmware_wasm.js。

用法：
  python build_firmware.py          重新编译（需要 wasi-sdk，见 README.md）
  python build_firmware.py --check  只检查内嵌的固件和当前源码是否一致，不需要编译器
改过 H7 固件后要重新编译，否则 3D 仿真跑的还是旧逻辑。
"""

import base64
import hashlib
import os
import subprocess
import sys
import tempfile

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

HERE = os.path.dirname(os.path.abspath(__file__))
CODE = os.path.abspath(os.path.join(HERE, "..", ".."))
REPO = os.path.abspath(os.path.join(CODE, "..", ".."))
H7 = os.path.join(CODE, "STM32H7")
TESTS = os.path.join(CODE, "tests")
OUT = os.path.join(HERE, "..", "sim_3d", "firmware_wasm.js")

# 和 tests/run_tests.ps1 的 test_h7_demo_flow 使用同一组真实源码
SOURCES = [
    "Core/scheduler.c", "Drivers/gimbal.c", "Drivers/servo.c",
    "Radar/radar_process.c", "Radar/radar_map.c",
    "TargetManager/target_list.c", "TargetManager/target_state.c", "TargetManager/target_switch.c",
    "ClosedLoop/closed_loop.c", "ClosedLoop/relock_logic.c", "ClosedLoop/assist_localize.c",
    "Algorithm/data_fusion.c", "Algorithm/geometry.c", "Algorithm/motion_predict.c",
    "Algorithm/kalman_filter.c", "Datalogger/exp_log.c",
]
INCLUDE_DIRS = ["", "Core", "Drivers", "Radar", "Algorithm", "TargetManager", "Communication",
                "ClosedLoop", "Network", "Datalogger"]


def source_hash():
    """固件源码和宿主的哈希，统一换行，避免 Windows 和 Linux 检出结果不同。"""
    files = [os.path.join(HERE, "sim_host.c")]
    for d in ["Core", "Drivers", "Radar", "Algorithm", "TargetManager", "ClosedLoop", "Datalogger"]:
        for name in os.listdir(os.path.join(H7, d)):
            if name.endswith((".c", ".h")):
                files.append(os.path.join(H7, d, name))
    files.append(os.path.join(H7, "config_mount.h"))
    h = hashlib.sha256()
    for path in sorted(files, key=lambda p: os.path.relpath(p, CODE).replace(os.sep, "/")):
        h.update(os.path.relpath(path, CODE).replace(os.sep, "/").encode())
        h.update(open(path, "rb").read().replace(b"\r\n", b"\n"))
    return h.hexdigest()[:16]


def check():
    want = source_hash()
    text = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
    if ('sourceHash: "%s"' % want) not in text:
        print("3D 仿真内嵌的固件不是当前源码（%s）。请运行 python build_firmware.py" % want)
        return 1
    print("3D 仿真内嵌的固件和当前源码一致：%s" % want)
    return 0


def clang_path():
    sdk = os.environ.get("WASI_SDK_PATH") or os.path.join(REPO, ".tools", "wasi-sdk-13.0")
    exe = os.path.join(sdk, "bin", "clang.exe" if os.name == "nt" else "clang")
    if not os.path.exists(exe):
        sys.exit("找不到 wasi-sdk：%s。下载后解压到这里，或设置环境变量 WASI_SDK_PATH" % sdk)
    return sdk, exe


def build():
    sdk, clang = clang_path()
    flags = ["--target=wasm32-wasi", "--sysroot=" + os.path.join(sdk, "share", "wasi-sysroot"),
             "-std=c11", "-O2", "-ffunction-sections", "-fdata-sections",
             "-Wno-unused-parameter",
             "-I" + TESTS, "-I" + os.path.join(TESTS, "stubs"), "-I" + os.path.join(TESTS, "stubs", "h7")]
    flags += ["-I" + os.path.join(H7, d) for d in INCLUDE_DIRS]
    flags += ["-include", os.path.join(TESTS, "stubs", "h7", "main.h"),
              "-D__CORE_MAIN_H", "-D__TIM_H", "-D__GPIO_H", "-D__MAIN_H", "-D__TIM_H__", "-D__GPIO_H__",
              "-DSTUB_REAL_RADAR"]
    link = ["-nostartfiles", "-Wl,--no-entry", "-Wl,--gc-sections", "-Wl,--strip-all", "-Wl,--allow-undefined", "-lm"]
    with tempfile.TemporaryDirectory(dir=os.path.join(REPO, ".tools")) as tmp:
        wasm = os.path.join(tmp, "firmware.wasm")
        env = dict(os.environ, TMP=tmp, TEMP=tmp)
        cmd = [clang] + flags + [os.path.join(HERE, "sim_host.c")] + [os.path.join(H7, s) for s in SOURCES] + ["-o", wasm] + link
        subprocess.run(cmd, check=True, env=env)
        data = open(wasm, "rb").read()
    b64 = base64.b64encode(data).decode()
    lines = [b64[i:i + 120] for i in range(0, len(b64), 120)]
    body = ",\n".join('"%s"' % line for line in lines)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* 由 sim_wasm/build_firmware.py 生成，不要手改。内容是编译好的 H7 固件。 */\n")
        f.write('(function (root) {\n  var data = { sourceHash: "%s", base64: [\n%s\n].join("") };\n' % (source_hash(), body))
        f.write("  if (typeof module !== 'undefined' && module.exports) module.exports = data; else root.SIM_FIRMWARE = data;\n")
        f.write("})(this);\n")
    print("固件 %d 字节 → %s" % (len(data), os.path.relpath(OUT, CODE)))
    return 0


if __name__ == "__main__":
    sys.exit(check() if "--check" in sys.argv else build())
