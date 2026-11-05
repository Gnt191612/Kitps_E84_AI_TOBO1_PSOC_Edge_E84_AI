# KIT_PSE84_AI 多板协作目标跟踪系统

**下载 3D 仿真软件：** [免安装版 zip（推荐）](https://github.com/Gnt191612/Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/releases/latest/download/Sim3D-Portable.zip) · [Windows 安装版](https://github.com/Gnt191612/Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/releases/latest/download/Sim3D-Windows.exe) · [全部版本](https://github.com/Gnt191612/Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/releases)

下载 zip，全部解压后双击“启动3D仿真.bat”即可。只需要 Edge 或 Chrome（Windows 10/11 自带 Edge），不需要联网和管理员权限。

安装版没有数字签名，可能被安全软件拦截：Windows 可能提示“已保护你的电脑”，装有火绒等软件时可能无法建快捷方式或自动打开，这时请用免安装版。

本仓库用于全国大学生嵌入式芯片与系统设计竞赛。系统由 STM32H743ZIT6 主控、KIT_PSE84_AI 识别板、两套 GOOUUU ESP32-S3-CAM N16R8 + OV2640 和 HC-SR04 扫描雷达组成。

> 当前状态：H7、ESP32 和 KIT_PSE84_AI 工程均可编译，电脑端自动化测试通过，**尚未上板验证**。没有完成舵机逐路标定和供电检查前，不要整机带载上电。

## 应用场景

系统把雷达发现、NPU 人体确认和视觉跟踪放在本地完成，图像不需要上传，适合 4 m 以内的小范围室内环境。

| 场景 | 解决的问题 | 系统的作用 |
|---|---|---|
| **实验室 / 机房夜间闯入跟踪** | 下班后无人值守，普通摄像头只录像，不会主动跟人，往往事后才能查看 | 学习无人时的环境；有人进入时发现并确认是人，然后持续跟踪、记录轨迹 |
| **小型库房 / 档案室防闯入** | 通道窄、遮挡多，布置多台摄像头成本高 | 一套系统覆盖 270° 范围；椅子、箱子等静物判定后并入背景，减少重复误报；目标被短暂遮挡后尝试复锁 |
| **多传感器融合与边缘 AI 教学平台** | 相关课程缺少可以动手、能看到内部过程的实物 | 雷达、NPU、跟踪和调度的过程都可通过串口日志观察，并内置对比实验开关 |

以上是系统的设计目标，**尚未在这些实际环境中验证**。目前的限制：

- 超声波量程不超过 4 m，不适合室外、大型仓库或长走廊。
- 模型只判断画面中有没有人，不识别身份和姿态；当前模型的精度未达设计目标，且只在 AI 生成图像上评估过。
- 最多同时管理 3 个目标，其中两个可由视觉跟踪，不适合人流密集的场所。
- 上电时需要学习无人环境；84E 连续 3 次把某个位置判为不是人时，该位置会并入背景，模型连续漏判静止的人时可能发现不了，需等其移动或重新学习背景。
- 目前只跟踪和记录，暂未实现声光报警和通知推送。

演示方法见 [中期答辩演示手册](Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/code/DEMO.md)；浏览器打开 `code/tools/demo_monitor.html` 可连接串口实时查看，也可回放仿真日志（不需要硬件）。`code/tools/sim_3d.html` 用 3D 显示装置，可回放、连串口、手动计算云台角度，并查看各部件接线。

## 当前硬件基线

- 主控：STM32H743ZIT6 Cortex-M7 开发板（含底板）
- AI 识别：Infineon KIT_PSE84_AI，PSE846GPS2DBZC4A，OV7675 DVP
- 视觉跟踪：2 套 GOOUUU ESP32-S3-CAM N16R8（含底板），均配 OV2640
- 距离扫描：HC-SR04
- 水平轴：4 只 SG90 9G 270°位置舵机，尚待购买并确认具体商品参数
- 俯仰轴：3 只 SG90 9G 180°位置舵机；[现有商品链接](https://e.tb.cn/h.8zE8oIBCzII1ZhC?tk=mRHVTKVL93K)
- 舵机电源：必须使用七只舵机共用的外部独立电源并与控制板共地；型号、电压和额定电流尚待确定

`材料/360°舵机.jpg`是旧版硬件记录：当时水平轴使用360°连续旋转舵机、俯仰轴使用180°位置舵机。当前代码和接线统一以“水平270°位置舵机、俯仰180°位置舵机”为准。

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

工程路径很深，`mtbninja` 要求每个目标文件的完整路径短于 260 字符，所以先用 `subst` 把 `Workspace` 映射成一个空闲盘符（下例用 `X:`，注销后要重新映射）。在 PowerShell 中：

```powershell
subst X: <仓库路径>\Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\Workspace
```

然后打开 ModusToolbox 的 modus-shell（`tools_3.8\modus-shell\Cygwin.bat`）运行：

```bash
cd /cygdrive/x/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision
make getlibs TARGET=APP_KIT_PSE84_AI
make build TARGET=APP_KIT_PSE84_AI TOOLCHAIN=GCC_ARM
```

产物是 `build/app_combined.hex`。

**烧录前要补 Flash Loader（`make build` 不需要）。** `PSE84_SMIF.FLM` 是 Infineon 的二进制文件，其 EULA 限制再分发，因此不放进本仓库，需要从官方 BSP 取回（已核对与 BSP v1.3.0 中的文件逐字节相同）。在 PowerShell 中、工程目录下运行：

```powershell
git clone --depth 1 --branch release-v1.3.0 https://github.com/Infineon/TARGET_KIT_PSE84_AI $env:TEMP\pse84_bsp
Copy-Item $env:TEMP\pse84_bsp\config\FlashLoaders\PSE84_SMIF.FLM bsps\TARGET_APP_KIT_PSE84_AI\config\FlashLoaders\
Remove-Item -Recurse bsps\TARGET_APP_KIT_PSE84_AI\config\GeneratedSource
```

再运行一次 `make build`，`config/GeneratedSource/` 下才会出现处理后的 `PSE84_SMIF.FLM`。实测有无 FLM 生成的 hex 只差一行构建时间字符串。`make program` 尚未实测。

**构建状态（2026-11-04，ModusToolbox 3.8 + GCC 14.2.1）：** 已从 git 全新克隆，按上面的步骤完成 `proj_cm33_s`、`proj_cm33_ns`、`proj_cm55` 三个工程的完整构建，生成 `build/app_combined.hex`。**未上板。** 常见问题：

- **`Physical memory "SMIF0MEM1" does not exist`：** 这是连带错误，真正的错误在它前面的 `Flashloader directory doesn't exist`。`design.cyqspi` 要求 `config/FlashLoaders/` 目录存在，否则 QSPI 配置器失败、导不出存储器定义。仓库现已提交该目录下的 `version.xml`，正常检出不会再遇到。
- **`exceeds the allowed path length for windows`：** 路径过长。完整克隆时，放在 `C:\src\e84` 和 `C:\e8` 下都实测超限，所以请用上面的 `subst`。目录联接（junction）无效，工具会把它解析成真实路径。
- **`git status` 显示 `proj_cm55/imports/emusb-host/Config/` 下两个文件被修改：** `make getlibs` 重写了这两个文件，内容不变，只是换行符不同（`git diff` 为空），不用提交。
- BSP 的 `COMPONENT_PREBUILT_SECURE_APP/*.o` 被 `.gitignore` 排除，上述构建没有用到它。
人体/背景二分类模型已经完成训练，产物位于 `code/PSE84E_Project/NPU/output/`，包括 `best.keras`、`model.h5`、`model_quantized.tflite` 和 `model_weights.h`。模型输入为 `160×120×1` 灰度图，类别为 `background/human`。

`code/PSE84E_Project` 使用错误的旧兼容设备层，只能做应用逻辑参考，生成物不得烧录到 KIT_PSE84_AI。

**官方 CM55 工程现状：**

- 已编入人体模型，上游 RPS 示例模型（`proj_cm55/model/`）已删除。`proj_cm55/Makefile` 设置为 `NN_MODEL_NAME=HUMAN_DETECT`、`NN_MODEL_FOLDER=mtb_ml_gen`（即 `proj_cm55/mtb_ml_gen/`，生成配置在 `model_human/`）、`CAMERA_TYPE = CAM_DVP`。
- 模型转换：先用 `code/PSE84E_Project/NPU/convert_uint8io_to_int8io.py` 把 uint8 I/O 无损改成 int8 I/O（ml-coretools 不接受 uint8 I/O），再用 ml-coretools 3.1 生成 Vela/TFLM 产物。当前为 v2 模型：45 个算子全部在 U55 上，Vela 估算 13.01 ms/帧（未实测），编译后模型 183,088 B，arena 为 160,768 B。`coretools.json` 的必填字段、重新生成命令和路径过长导致 QEMU 失败的问题，见 [`model_human/README.md`](Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/Workspace/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision/model_human/README.md)。
- 推理链路：`proj_cm55/source/inference_task.c` 把 320×240 BGR565 帧做 2×2 平均和 BT.601 灰度转换，得到 160×120 int8 输入（`q = gray - 128`），通过 `mtb_ml_model_run`（TFLM + U55）推理，读取 `[background, human]` softmax 输出。
- DVP：官方工程已带 `camera-dvp-ov7675` 库，BSP 中也有 `CYBSP_DVP_CAM_*` 引脚和 `CYBSP_I2C_CAM_CONTROLLER`。`CAM_DVP` 下由 `mtb_dvp_cam_ov7675_init` 采集 320×240 BGR565 帧。
- 结果出口：`human_detect_result_t` 包含帧号、`timestamp_ms`、推理/预处理耗时、两类置信度和 `human_present`。可以通过 `inference_get_latest_result()` 读取，也可以覆盖弱符号 `inference_result_hook()`。LCD 只在检测到人时画一个整帧框。
- 构建：见下方“构建状态”。**未上板验证**，包括 DVP 出图、颜色通道顺序和模型实际精度。本地仅有 3 张样例图，其中 2 张分类错误，上板前必须用 OV7675 实拍画面重新评估。

**H7 ↔ 84E 通信（已移植，未上板）：** `proj_cm55/source/h7_link.c` 由 CM55 直接驱动 SCB1 UART，不需要核间 IPC。使用 J5 排针上的 **P9_2（TX）/P9_3（RX）**，115200 8N1。帧格式与 `code/STM32H7/Communication/protocol.c` 按字节兼容：`AA 55 [len=1+payload] [type] [payload] [XOR(type..payload)]`；H7→84E 为 type 0x01，载荷 22 B（`cmd_type u8, target_id u8, center_angle/angle_width/min_dist_cm/max_dist_cm f32, timestamp_ms u32`）；84E→H7 为 type 0x02，载荷 21 B（`target_id u8, confidence/x_cm/y_cm/dist_cm/angle_deg f32`），均为小端紧凑排列。处理流程如下：

- 收到命令后，等待命令之后完成的第一次推理（最多 800 ms，低于 H7 的 1500 ms 超时），用同一个 `target_id` 回复。`confidence` 填人体概率，H7 侧 `< 0.5` 视为未识别（与 84E 的 `HUMAN_CONF_THRESHOLD` 相同）；超时回复 0。
- 分类模型没有定位能力，因此回传的 `angle_deg` 是命令的中心角，`dist_cm` 取 `(min+max)/2`，`x/y = d·cos/sin(angle)`，与 H7 雷达坐标约定一致。
- 编译期开关 `H7_LINK_DEBUG_DUMP`（默认 0）设为 1 时，会在调试串口打印收发帧的十六进制内容。

引脚选择依据：官方用户指南中唯一的 3.3 V 排针 J14 已被 OV7675 占满；旧接线的 P21_2/P21_3 在官方 BSP 中是 I2S，P21_4~P21_7 是雷达 SPI，都不能再用。**J5 属于 1.8 V 电平域，接 H7（3.3 V）必须加双向电平转换器**，接法见 `code/WIRING.md` 第 1 节。STM32H7 代码未做任何改动。调度逻辑迁移、整机联调和电平转换实物验证仍未完成，因此仓库目前还不能声称三块平台均可直接烧录完成整机调试。

### 电脑端自动化测试

不需要开发板，用 gcc 编译真实固件源码并用模拟数据检验：H7 调度器 13 个场景、按演示流程跑通的整机仿真、雷达背景学习与人体检测、ESP32 跟踪误差与复锁、三块板之间的通信帧互通。

```powershell
cd Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI\code\tests
powershell -ExecutionPolicy Bypass -File run_tests.ps1
```

Linux / macOS 用 `sh run_tests.sh`。说明见 [`code/tests/README.md`](Kitps_E84_AI_TOBO1_PSOC_Edge_E84_AI/code/tests/README.md)。

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
- 每次提交至少完成受影响工程的干净构建，并运行 `code/tests` 的电脑端测试全部通过；在提交说明中写明未进行的硬件测试。
- 不把“编译通过”等同于“可烧录”或“整机验证通过”。

## 许可证

仓库根目录代码遵循 [LICENSE](LICENSE)。第三方 BSP、HAL、ESP-IDF 组件和 Infineon 示例保留各自许可证。
