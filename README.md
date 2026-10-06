# 多板协作目标识别与跟踪系统

全国大学生嵌入式芯片与系统设计竞赛及大学生创新训练项目。系统由 STM32H7 统一控制雷达和云台，KIT_PSE84_AI 完成目标识别，两块 ESP32-S3-CAM 完成视觉闭环跟踪。

> 当前状态：H7 与 ESP32 工程可编译；KIT_PSE84_AI 的项目应用仍在向官方多核 BSP 迁移。没有完成舵机逐路标定和供电检查前，不要整机带载上电。

## 硬件组成

| 模块 | 型号与职责 |
|---|---|
| 主控 | 第三方 STM32H743ZIT6 开发板，Arm Cortex-M7；管理调度、通信、雷达和全部舵机 |
| AI 识别 | Infineon KIT_PSE84_AI，PSE846GPS2DBZC4A + OV7675 DVP；CM33/CM55/U55 |
| 视觉跟踪 | 2 × GOOUUU ESP32-S3-CAM N16R8（16 MB Flash、8 MB PSRAM）+ OV2640 |
| 距离扫描 | HC-SR04 + SG90 9G 270°水平位置舵机 |
| 相机云台 | 3 × 270°水平位置舵机 + 3 × SG90 9G 180°俯仰位置舵机 |

舵机必须使用独立、容量足够的电源，控制板与舵机电源共地。KIT_PSE84_AI 的 P21 属于 1.8 V I/O 电源域，不能与 H7 的 3.3 V GPIO 直接连接。完整接线约束见 [WIRING.md](Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/code/WIRING.md)。

## 工作流程

1. 上电后雷达位于 135°正前方，两个 ESP32 云台和 84E 云台保持初始方向。
2. 雷达按 `135° -> 0° -> 135° -> 270° -> 135°` 连续扫描。
3. H7 根据雷达候选方位转动 84E 云台，并请求 NPU 识别。
4. 识别成功且目标距离满足条件后，H7 分配一个 ESP32 跟踪。
5. ESP32 计算图像误差和 PID 控制量，经 UART 回传 H7，由 H7 输出云台 PWM。
6. 接近水平边界时，H7 调用空闲 84E 和另一块 ESP32 完成接力。
7. ESP32 释放后回初始方向；84E 工作期间空闲时保持当前位置；安全关机时全部回初始方向。

局域网客户端和语音交互暂不纳入当前可运行范围。

## 仓库结构

```text
Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/
├── code/STM32H7/                 STM32H743ZIT6 主控固件
├── code/ESP32_ov2640/            两块 ESP32-S3-CAM 共用固件
├── code/PSE84E_Project/          旧应用层兼容工程，仅供编译检查，禁止烧录
├── Workspace/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision/
│                                  KIT_PSE84_AI 官方 CM33/CM55/U55 工程
├── code/WIRING.md                接线和电平说明
├── code/REVIEW_AND_GUIDE.md      代码审查、限制和调试说明
└── promt.txt                     系统需求与设计意图
```

## 开发环境

- Git
- CMake 3.16 或更高版本
- Ninja
- Arm GNU Toolchain 12/13，提供 `arm-none-eabi-gcc`
- ESP-IDF 5.3，用于 ESP32-S3
- ModusToolbox 3.7 或更高版本及 Machine Learning Pack，用于 KIT_PSE84_AI
- STM32CubeProgrammer 或 ST-Link 工具，用于 H7

将 Arm GNU Toolchain 的 `bin` 目录加入 `PATH`，也可以设置环境变量：

```powershell
$env:ARM_GCC_PATH = "C:\Toolchains\arm-gnu-toolchain\bin"
$env:Path = "$env:ARM_GCC_PATH;$env:Path"
```

## 从零构建

### STM32H743ZIT6

```powershell
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\code\STM32H7
cmake -S . -B build -G Ninja
cmake --build build
```

输出位于 `build/STM32H7_RadarMaster.hex`、`.bin` 和 `.elf`。使用 STM32CubeProgrammer 通过 ST-Link 烧录。首次构建必须确认 CubeMX 生成的定时器、UART 和 GPIO 与 [WIRING.md](Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/code/WIRING.md) 一致。

### ESP32-S3-CAM N16R8

先进入 ESP-IDF 5.3 环境，再执行：

```powershell
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\code\ESP32_ov2640
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

两块板使用相同固件。GPIO14 高电平选择 A 板，低电平或悬空选择 B 板。`sdkconfig.defaults` 固定 16 MB Flash 和 8 MB Octal PSRAM 的基础配置。

### KIT_PSE84_AI

```powershell
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\Workspace\PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision
make getlibs
make build TOOLCHAIN=GCC_ARM
make program TOOLCHAIN=GCC_ARM
```

官方工程默认 `TARGET=APP_KIT_PSE84_AI`，详见其目录内 README。当前竞赛应用尚未完全迁入该官方多核工程，因此现阶段只能构建和验证官方基线，不能把 `code/PSE84E_Project` 生成的 HEX 烧录到开发板。

## 上电前检查

1. 逐个空载标定七路舵机的安全最小、中位、最大脉宽、方向和转动时间。
2. 不要默认 `500/1500/2500 us` 对所有 SG90 改装舵机安全。
3. 确认舵机独立供电、保险或限流、足够的峰值电流以及可靠共地。
4. 先断开舵机，仅验证三类开发板的串口、电平转换和摄像头日志。
5. 按雷达、84E、单 ESP32、双 ESP32 接力的顺序逐级联调。
6. 单纯断电无法执行软件回正；正式设备需要关机按键或掉电保持电源。

## 当前已知限制

- KIT_PSE84_AI 竞赛应用尚未完成官方 CM33/CM55/U55 工程迁移。
- 舵机端点和 `3000 ms/135°` 扫描时间是待实测参数。
- 84E 与 H7 的电平转换和最终通信引脚仍需按实体板复核。
- 现有雷达每次只产生一个 HC-SR04 距离点，单点候选由 84E 二次识别确认，不是完整点云 DBSCAN。
- 安全关机 API 已存在，但还需要确定实体按键或上位控制信号。

## 协作约定

- 不提交工具链、构建目录、个人令牌、串口号和本机绝对路径。
- 修改通信帧时必须同步 H7、ESP32 和 84E，并注明字段偏移、类型和单位。
- 修改 PWM 前先更新标定数据，不直接扩大脉宽范围。
- 每次提交至少完成受影响工程的干净构建，并在提交说明中写明未进行的硬件测试。
- 不把“编译通过”等同于“可烧录”或“整机验证通过”。

## 许可证

仓库根目录代码遵循 [LICENSE](LICENSE)。第三方 BSP、HAL、ESP-IDF 组件和 Infineon 示例保留各自许可证。
