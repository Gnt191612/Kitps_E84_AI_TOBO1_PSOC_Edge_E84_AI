# Linux 构建说明

本仓库原来的构建入口只有 Windows 批处理 `code/build_all.bat`。本目录提供 Linux 下的一键构建脚本，
并记录三块目标板在 Linux 上的实测结果、所需修复与已知限制。

## 实测状态

| 目标 | 构建系统 | Linux 结果 | 产物（本次实测） |
|---|---|---|---|
| **STM32H743ZIT6**（主控） | 独立 CMake + Ninja | ✅ 通过 | `STM32H7_RadarMaster.hex` 209,233 B；`.bin` 74,408 B |
| **ESP32-S3 ×2**（视觉跟踪） | ESP-IDF（`idf.py`） | ✅ 通过 | `ESP32_ov2640.bin` 296,224 B |
| **PSE84E**（NPU 识别） | ModusToolbox 3.9（make） | ⚠️ 脚本/步骤已提供；本次环境无外网未复测 | — |

验证环境：Arch Linux / `arm-none-eabi-gcc 15.3.1` / CMake 4.4.3 / Ninja 1.13 / **ESP-IDF v6.1** /
ModusToolbox 3.9（`/opt/ModusToolbox/tools_3.9`）。

> PSE84E 首次构建必须联网从 Infineon 内容库拉取约 2.7 GB 依赖（`make getlibs`），
> 本次验证环境无法访问 `modustoolbox.infineon.com`，因此只提供了脚本与步骤，未做复测。

## 快速开始

```bash
# 0) 子模块（ESP32 依赖的 OV2640/OV3660 等摄像头驱动 esp32-camera）
git submodule update --init --recursive

# 1) 一键构建（缺工具链的目标会自动 SKIP；日志写入 code/linux/out/）
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/code/linux
./build_linux.sh

# 2) 只构建单个目标
./build_linux.sh stm32
source /opt/esp-idf/export.sh && ./build_linux.sh esp32
CY_TOOLS_PATHS=/opt/ModusToolbox/tools_3.9 ./build_linux.sh pse84
```

各目标产物位置：

- STM32H7：`code/STM32H7/build/STM32H7_RadarMaster.{elf,hex,bin}`
- ESP32-S3：`code/ESP32_ov2640/build/ESP32_ov2640.bin`
- PSE84E：`Workspace/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision/proj_*/build/APP_KIT_PSE84_AI/Debug/proj_*.{bin,elf,hex}`

## 依赖安装

| 目标 | 依赖 |
|---|---|
| STM32H7 | `arm-none-eabi-gcc`、`cmake`、`ninja`。Arch：`pacman -S arm-none-eabi-gcc arm-none-eabi-newlib cmake ninja`；Debian：`apt install gcc-arm-none-eabi cmake ninja-build` |
| ESP32-S3 | ESP-IDF（已安装并 `source export.sh`）。工程 `sdkconfig` 目标是 esp32s3，IDF 5.3.x / 6.x 均可，见下文适配说明 |
| PSE84E | ModusToolbox 3.9 + `make`，并需联网填充本地内容库（首次约 2.7 GB） |

## 手动构建（与脚本等价的命令）

### STM32H743ZIT6

上游 `code/STM32H7/CMakeLists.txt` 已用 `find_program()` 查找工具链，Linux 下把 `arm-none-eabi-gcc`
放进 `PATH` 即可；也可以用 `ARM_GCC_PATH` 指定工具链 `bin` 目录：

```bash
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/code
cmake -S STM32H7 -B STM32H7/build -G Ninja     # 或 cmake -DARM_GCC_PATH=/path/to/bin
cmake --build STM32H7/build
arm-none-eabi-size STM32H7/build/STM32H7_RadarMaster.elf
```

注意：如果只看到「构建成功」但 `.bin` 为 0 字节，说明启动文件没有编入（旧版本 CMake glob
大小写问题）；上游已改为显式引用 `Startup/startup_stm32h743zitx.s`，本仓库当前版本不会出现。

### ESP32-S3（ESP-IDF）

```bash
git submodule update --init --recursive        # 首次：拉取 components/esp_camera
source /opt/esp-idf/export.sh
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/code/ESP32_ov2640
idf.py build
```

串口在 Linux 上是 `/dev/ttyUSB*` / `/dev/ttyACM*`（Windows 的 COM 编号见 `ESP32烧录备注.txt`）：

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

### PSE84E（ModusToolbox）

必须**逐个子工程**执行 `getlibs`（应用级 `make getlibs` 会漏掉 `proj_cm33_s`）：

```bash
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/Workspace/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision
export CY_TOOLS_PATHS=/opt/ModusToolbox/tools_3.9
for d in proj_cm33_s proj_cm33_ns proj_cm55; do make -C $d getlibs; done
make build CY_COMPILER_GCC_ARM_DIR=/usr
```

两个易错点：

- `CY_COMPILER_GCC_ARM_DIR` 传 **`/usr`** 而不是 `/usr/bin` —— MTB 内部会自己拼接 `/bin`；
- 若报 `EdgeProtect Secure Suite not found. Combine-Sign step not executed`，
  这是 ModusToolbox Setup 里的可选安全套件缺失，**只影响三核镜像的合并签名后处理，不影响编译产物**。

## 本 PR 修复/新增的内容

### 1. `components/esp_camera` 是「孤儿 gitlink」，全新 clone 无法构建

`code/ESP32_ov2640/components/esp_camera` 在 git 中以子模块（`mode 160000`）记录，
但仓库原先**没有 `.gitmodules`**，导致任何平台全新 clone 后该目录为空，
而 `main/CMakeLists.txt` 又 `REQUIRES esp_camera`，ESP32 工程必然配置失败。

修复：新增 `.gitmodules`，把该路径映射到官方仓库
<https://github.com/espressif/esp32-camera>（保留原 gitlink 指向的 commit `202df95`，
该 commit 在官方仓库历史中存在）。之后使用标准子模块流程：

```bash
git submodule update --init --recursive
```

> 从 GitHub「Download ZIP」得到的源码包不含子模块内容；离线构建 ESP32 需要
> 预先在联网环境初始化子模块，或者手工把 esp32-camera 放到该目录。

### 2. ESP-IDF 6.x 适配（3 处）

工程按 ESP-IDF 5.3.2 配置，在 6.x 上需要以下最小改动：

| 位置 | 问题 | 修法 |
|---|---|---|
| `code/ESP32_ov2640/main/CMakeLists.txt` | IDF 5.3 起 `driver` 组件被拆分，6.x 起不再转发 `driver/gpio.h`、`driver/uart.h`、`driver/i2c.h`、`driver/ledc.h`、`hal/uart_types.h` | `REQUIRES` 增加 `esp_driver_gpio esp_driver_uart esp_driver_i2c esp_driver_ledc esp_hal_uart` |
| `code/ESP32_ov2640/Communication/network_manager.c` | `ESP_IF_WIFI_STA` 在 IDF 6.0 中已移除 | 改为 `WIFI_IF_STA` |
| `code/ESP32_ov2640/Drivers/ov2640/camera_io.c` | 使用 `malloc/free` 但缺 `<stdlib.h>`（GCC 14+ 默认把隐式声明当错误） | 补 `#include <stdlib.h>` |

以上改动在 IDF 5.3.x 上同样成立，不影响 Windows 侧的现有构建流程。

### 3. 新增 Linux 构建脚本与说明（本目录）

- `build_linux.sh`：三目标一键构建（`stm32` / `esp32` / `pse84`，缺省全部），日志写入 `out/`；
- `LINUX_BUILD.md`：本文档。

### 4. `.gitignore` 补充 CMake 缓存忽略

`cmake -B build` 会在源码树内生成 `CMakeCache.txt`、`CMakeFiles/`、`cmake_install.cmake`，
根 `.gitignore` 原先只在忽略 `build/` 目录，对就地生成的 CMake 缓存无效，已补充这三条规则。

## 已知限制与排错

- **ESP32 首次构建会改动工作树**：ESP-IDF 会重新生成 `sdkconfig` 并按 `idf_component.yml`
  重新解析 `managed_components/`（可能需要联网访问 components.espressif.com）。这些是构建生成物，
  不要提交；本 PR 也未包含它们。
- **切换 ESP-IDF 大版本后需要 `idf.py fullclean`**：构建目录与 Python 虚拟环境不匹配时会报
  `is currently active in the environment while the project was configured with ...`。
- **PSE84E**：需要 ModusToolbox 3.9 + 约 2.7 GB 本地内容库；必须逐子工程 `getlibs`；
  `CY_COMPILER_GCC_ARM_DIR` 传 `/usr`；`Combine-Sign` 依赖可选的 EdgeProtect 安全套件。
- **STM32H7**：工具链不在 `PATH` 时，用环境变量 `ARM_GCC_PATH=<bin 目录>` 或
  `cmake -DARM_GCC_PATH=<bin 目录>` 指定；建议核对 `arm-none-eabi-size` 输出的 text 段非 0。
- **Windows**：`code/build_all.bat` / `build_all.ps1` 流程保持不变，本次改动不引入 Windows 侧行为变化。
