# KIT_PSE84_AI 多板协作目标跟踪系统

本仓库用于全国大学生嵌入式芯片与系统设计竞赛及大学生创新创业训练项目。系统由 STM32H743ZIT6 主控、KIT_PSE84_AI 识别板、两套 GOOUUU ESP32-S3-CAM N16R8 + OV2640 和 HC-SR04 扫描雷达组成。

> 当前状态：H7 与 ESP32 工程可编译；KIT_PSE84_AI 的项目应用仍在向官方多核 BSP 迁移。没有完成舵机逐路标定和供电检查前，不要整机带载上电。

## 当前硬件基线

- 主控：STM32H743ZIT6 Cortex-M7 开发板（含底板）
- AI 识别：Infineon KIT_PSE84_AI，PSE846GPS2DBZC4A，OV7675 DVP
- 视觉跟踪：2 套 GOOUUU ESP32-S3-CAM N16R8（含底板），均配 OV2640
- 距离扫描：HC-SR04
- 水平轴：4 只 SG90 9G 270°位置舵机，尚待购买并确认具体商品参数
- 俯仰轴：3 只 SG90 9G 180°位置舵机；[现有商品链接](https://e.tb.cn/h.8zE8oIBCzII1ZhC?tk=mRHVTKVL93K)
- 舵机电源：必须使用七只舵机共用的外部独立电源并与控制板共地；型号、电压和额定电流尚待确定

## 预期控制逻辑

1. 上电时雷达、两套 ESP32 云台和 KIT_PSE84_AI 云台均处于各自初始朝向。
2. 未锁定目标时，只有雷达水平轴按 `135° -> 0° -> 135° -> 270° -> 135°` 循环扫描；两个视觉云台保持初始朝向。
3. 雷达发现候选后，H7 控制 KIT_PSE84_AI 云台指向候选并请求识别；84E 空闲时保持当前位置。
4. 识别成功后，H7 分配一块 ESP32 跟踪。ESP32 计算 PID 控制量并回传，H7 统一输出七路舵机 PWM。
5. ESP32 水平轴接近 `15°/255°` 边界时，由 H7 调度另一块 ESP32，并在 84E 空闲时使用其辅助接力。
6. 系统停止时，雷达、ESP32 和 84E 云台全部回到各自初始朝向。

局域网客户端、目标切换交互和语音播报当前暂缓实现。

## 克隆后从哪里开始

```text
Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/
├── code/STM32H7/              H7 主控、雷达、调度和七路舵机输出
├── code/ESP32_ov2640/         两块 ESP32 共用的跟踪固件
├── code/PSE84E_Project/       旧应用逻辑参考，禁止烧录到 KIT_PSE84_AI
├── code/WIRING.md             当前实物接线和电气注意事项
└── Workspace/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision/
                               KIT_PSE84_AI 官方多核工程
```

先阅读 [`code/WIRING.md`](Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/code/WIRING.md)。当前接线按该文件保持不变，但 84E 与 H7 之间的电平和板级引脚仍须在实物复核后才能通电联调。

## 构建

### STM32H743ZIT6

依赖 CMake、Ninja 和 Arm GNU Toolchain。将工具链 `bin` 目录加入 `PATH`，或设置 `ARM_GCC_PATH`：

```powershell
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\code\STM32H7
cmake -S . -B build -G Ninja
cmake --build build
```

构建产物为 `build/STM32H7_RadarMaster.hex`、`.bin` 和 `.elf`。使用 STM32CubeProgrammer 与 ST-Link 烧录；首次烧录前必须按实物确认第三方底板的调试接口和供电跳线。

### GOOUUU ESP32-S3-CAM N16R8 + OV2640

依赖 ESP-IDF 5.x，目标芯片必须是 `esp32s3`：

```powershell
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\code\ESP32_ov2640
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

两块板烧录同一固件；GPIO14 决定 A/B 角色，具体接法见 `code/WIRING.md`。

### KIT_PSE84_AI

只允许使用 `Workspace/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision` 下的官方 CM33/CM55 多核工程。需要 ModusToolbox 3.7 或更高版本及 Machine Learning Pack，构建目标为 `APP_KIT_PSE84_AI`，DVP 摄像头需在 `proj_cm55/Makefile` 中配置为 `CAM_DVP`。

```powershell
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\Workspace\PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision
make getlibs TARGET=APP_KIT_PSE84_AI
make build TARGET=APP_KIT_PSE84_AI TOOLCHAIN=GCC_ARM
```

`code/PSE84E_Project` 使用错误的旧兼容设备层，只能做应用逻辑参考，生成物不得烧录到 KIT_PSE84_AI。当前官方工程仍是上游视觉示例，尚未完成本项目通信与识别逻辑迁移，因此仓库目前不能声称三块平台均可直接烧录完成整机调试。

## 烧录前强制检查

- 不从开发板或 USB 口给舵机供电；舵机独立供电并与 H7 共地。
- HC-SR04 ECHO 通过 `2.2 kΩ + 3.3 kΩ` 分压后接 H7。
- 新 270°舵机到货后，逐只、空载、限流完成中心脉宽、安全端点、方向和转动时间测量。
- 三只现有 180°舵机同样逐只校准，不依据商品页直接假定安全端点。
- 独立电源型号、输出电压和额定电流确认前，不连接七只舵机进行整机通电。
- 校准数据写入代码并复核后，才启用自动扫描和边界接力。

## 尚未闭环的硬件数据

以下信息只能在新舵机、电源到货并完成烧录后补齐：

- 4 只水平 270°位置舵机的具体商品、标称脉宽和供电规格；
- 七只舵机独立电源的型号、输出电压和额定电流；
- 每只实体舵机安装后的安全最小/中心/最大脉宽、正反方向和实际转动时间。

这些未知量不是软件默认值。现有角度映射和 PID 参数仅供编译与台架校准起点使用，不能作为无人值守通电依据。

## 协作约定

- 不提交工具链、构建目录、个人令牌、Wi-Fi 密码、串口号和本机绝对路径。
- 修改通信帧时必须同步 H7、ESP32 和 84E，并注明字段偏移、类型和单位。
- 修改 PWM 前先更新逐只舵机的标定数据，不直接扩大脉宽范围。
- 每次提交至少完成受影响工程的干净构建，并在提交说明中写明未进行的硬件测试。
- 不把“编译通过”等同于“可烧录”或“整机验证通过”。

## 许可证

仓库根目录代码遵循 [LICENSE](LICENSE)。第三方 BSP、HAL、ESP-IDF 组件和 Infineon 示例保留各自许可证。
