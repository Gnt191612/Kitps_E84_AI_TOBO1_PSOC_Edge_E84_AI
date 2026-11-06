#!/usr/bin/env bash
#
# build_linux.sh — 在 Linux 下构建本仓库的三块目标板
#
#   ./build_linux.sh              # 依次尝试 STM32H7 / ESP32-S3 / PSE84E
#   ./build_linux.sh stm32        # 只构建 STM32H7
#   ./build_linux.sh esp32 pse84  # 构建多个指定目标
#
# 依赖：
#   STM32H7 : arm-none-eabi-gcc、cmake、ninja
#   ESP32-S3: ESP-IDF（`source export.sh` 后 idf.py 可用，sdkconfig 目标为 esp32s3）
#   PSE84E  : ModusToolbox 3.9（CY_TOOLS_PATHS 指向 tools_3.9；首次 getlibs 需联网）
#
# 说明：脚本不修改纳入版本控制的源码；日志与临时产物写入本目录 out/，
#       构建产物落在各工程自身的 build/ 下。Windows 用户继续使用 code/build_all.bat。
#
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CODE_DIR="$(dirname "$SCRIPT_DIR")"
REPO_ROOT="$(dirname "$CODE_DIR")"
MTB_APP="$REPO_ROOT/Workspace/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision"
OUT_DIR="$SCRIPT_DIR/out"

RED=$'\e[31m'; GRN=$'\e[32m'; YEL=$'\e[33m'; RST=$'\e[0m'
ok()   { echo "${GRN}(OK)${RST}   $*"; }
fail() { echo "${RED}(FAIL)${RST} $*"; }
skip() { echo "${YEL}(SKIP)${RST} $*"; }
info() { echo "       $*"; }

usage() { sed -n '2,18p' "${BASH_SOURCE[0]}"; }

TARGETS=()
for arg in "$@"; do
    case "$arg" in
        stm32|esp32|pse84) TARGETS+=("$arg") ;;
        all)               TARGETS=(stm32 esp32 pse84) ;;
        -h|--help)         usage; exit 0 ;;
        *)                 echo "未知参数: $arg"; usage; exit 2 ;;
    esac
done
[ ${#TARGETS[@]} -eq 0 ] && TARGETS=(stm32 esp32 pse84)

# ------------------------------------------------------------------- 工具函数
need() {  # need <命令> <安装提示>
    if ! command -v "$1" >/dev/null 2>&1; then
        skip "未找到 $1"
        info "$2"
        return 1
    fi
    ok "$1 -> $(command -v "$1")"
}

# --------------------------------------------------------------------- STM32H7
# 上游已把工具链改为 find_program(arm-none-eabi-gcc)，Linux 下只需工具链在 PATH，
# 或用 -DARM_GCC_PATH=<bin 目录> / 环境变量 ARM_GCC_PATH 指定。
build_stm32() {
    echo "==> [1/3] STM32H743ZIT6（独立 CMake + Ninja）"
    need arm-none-eabi-gcc \
        "Arch: sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib cmake ninja | Debian: sudo apt install gcc-arm-none-eabi cmake ninja-build" || return 1
    need cmake "同上：安装 cmake" || return 1
    need ninja "同上：安装 ninja" || return 1

    local src="$CODE_DIR/STM32H7" b="$CODE_DIR/STM32H7/build"
    # 已配置过的构建目录直接复用；否则用 Ninja 生成器初始化
    if [ ! -f "$b/CMakeCache.txt" ]; then
        cmake -S "$src" -B "$b" -G Ninja >"$OUT_DIR/stm32-configure.log" 2>&1 \
            || cmake -S "$src" -B "$b" >"$OUT_DIR/stm32-configure.log" 2>&1 || {
            fail "CMake 配置失败，见 $OUT_DIR/stm32-configure.log"
            tail -10 "$OUT_DIR/stm32-configure.log"
            return 1; }
    fi
    cmake --build "$b" >"$OUT_DIR/stm32-build.log" 2>&1 || {
        fail "编译失败，见 $OUT_DIR/stm32-build.log"
        grep -E "error:|FAILED:" "$OUT_DIR/stm32-build.log" | head -20
        return 1; }
    ok "构建成功：$b/STM32H7_RadarMaster.{elf,hex,bin}"
    arm-none-eabi-size "$b/STM32H7_RadarMaster.elf" | tail -1 | sed 's/^/       /'
    local f
    for f in "$b/STM32H7_RadarMaster.hex" "$b/STM32H7_RadarMaster.bin"; do
        [ -f "$f" ] && info "$(stat -c%s "$f") B  $(basename "$f")"
    done
}

# -------------------------------------------------------------------- ESP32-S3
# 需要 ESP-IDF 环境（idf.py 在 PATH）。components/esp_camera 为 git 子模块，
# 全新 clone 后先执行：git submodule update --init --recursive
build_esp32() {
    echo "==> [2/3] ESP32-S3 ×2（OV2640 视觉跟踪，ESP-IDF）"
    if ! command -v idf.py >/dev/null 2>&1; then
        skip "未检测到 ESP-IDF 环境（idf.py 不在 PATH）"
        info "安装并导出环境：git clone -b v5.3.2 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf"
        info "  cd ~/esp/esp-idf && ./install.sh esp32s3 && . ./export.sh"
        info "本项目已在 ESP-IDF 5.3.x 与 6.x 上验证（见 LINUX_BUILD.md）"
        return 0
    fi
    ok "idf.py -> $(command -v idf.py)"

    local proj="$CODE_DIR/ESP32_ov2640"
    if [ ! -f "$proj/components/esp_camera/CMakeLists.txt" ]; then
        fail "components/esp_camera 为空（git 子模块未初始化）"
        info "修复：git submodule update --init --recursive"
        return 1
    fi
    ( cd "$proj" && idf.py build ) >"$OUT_DIR/esp32-build.log" 2>&1 || {
        fail "编译失败，见 $OUT_DIR/esp32-build.log"
        grep -E "error:|FAILED:" "$OUT_DIR/esp32-build.log" | head -20
        return 1; }
    ok "构建成功"
    local bin="$proj/build/ESP32_ov2640.bin"
    [ -f "$bin" ] && info "$(stat -c%s "$bin") B  ESP32_ov2640.bin"
}

# --------------------------------------------------------------------- PSE84E
# 走官方 ModusToolbox 工程（必须逐个子工程 getlibs，应用级 getlibs 会漏 proj_cm33_s）。
# 首次构建需要联网从 Infineon 内容库拉取依赖（约 2.7 GB）。
build_pse84() {
    echo "==> [3/3] PSE84E（PSOC Edge E84 + Ethos-U55 NPU，ModusToolbox）"
    if [ ! -d "$MTB_APP" ]; then
        fail "找不到 MTB 工程: $MTB_APP"; return 1
    fi
    : "${CY_TOOLS_PATHS:=/opt/ModusToolbox/tools_3.9}"
    export CY_TOOLS_PATHS
    if [ ! -d "$CY_TOOLS_PATHS" ]; then
        skip "未找到 ModusToolbox: $CY_TOOLS_PATHS"
        info "设置 CY_TOOLS_PATHS 指向 tools_X.Y 目录后重试"
        return 0
    fi
    ok "ModusToolbox -> $CY_TOOLS_PATHS"
    need make "安装 make（sudo pacman -S make / apt install make）" || return 1

    # 1) 逐个子工程拉取依赖（应用级 getlibs 会漏 proj_cm33_s）
    local d
    for d in proj_cm33_s proj_cm33_ns proj_cm55; do
        if ! ( cd "$MTB_APP" && make -C "$d" getlibs ) >"$OUT_DIR/mtb-$d-getlibs.log" 2>&1; then
            fail "$d getlibs 失败，见 $OUT_DIR/mtb-$d-getlibs.log"
            info "首次需联网访问 modustoolbox.infineon.com 拉取内容库"
            return 1
        fi
        ok "$d getlibs"
    done

    # 2) 构建（编译器目录传 /usr，MTB 内部会自己拼 /bin）
    ( cd "$MTB_APP" && make build CY_COMPILER_GCC_ARM_DIR="${CY_COMPILER_GCC_ARM_DIR:-/usr}" ) \
        >"$OUT_DIR/mtb-build.log" 2>&1
    local rc=$?
    if [ $rc -ne 0 ]; then
        if grep -q "Build complete" "$OUT_DIR/mtb-build.log"; then
            ok "三核编译链接成功（仅 Combine-Sign 后处理失败）"
            info "Combine-Sign 需要 ModusToolbox Setup 安装可选的 EdgeProtect Secure Suite，不影响产物"
        else
            fail "构建失败，见 $OUT_DIR/mtb-build.log"
            tail -20 "$OUT_DIR/mtb-build.log"
            return 1
        fi
    else
        ok "构建成功"
    fi
    local c
    for c in proj_cm33_s proj_cm33_ns proj_cm55; do
        local b="$MTB_APP/$c/build/APP_KIT_PSE84_AI/Debug/$c.bin"
        [ -f "$b" ] && info "$(stat -c%s "$b") B  $c.bin"
    done
}

# ------------------------------------------------------------------------ main
main() {
    echo "==================================================="
    echo "  多板协作目标跟踪系统 — Linux 构建脚本"
    echo "  STM32H743ZIT6 / ESP32-S3 / PSE84E"
    echo "==================================================="
    mkdir -p "$OUT_DIR"
    local rc=0 t
    for t in "${TARGETS[@]}"; do
        echo
        case "$t" in
            stm32) build_stm32 || rc=1 ;;
            esp32) build_esp32 || rc=1 ;;
            pse84) build_pse84 || rc=1 ;;
        esac
    done
    echo
    echo "==================================================="
    if [ $rc -eq 0 ]; then
        ok "完成"
    else
        fail "有目标未通过，见上方输出与 $OUT_DIR/ 日志"
    fi
    return $rc
}

main
