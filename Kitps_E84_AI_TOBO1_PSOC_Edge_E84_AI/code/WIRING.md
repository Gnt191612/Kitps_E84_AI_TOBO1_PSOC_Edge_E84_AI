# 四板接线指南 — 杜邦线速查表（型号已确认，待完成板级引脚复核）

> 硬件型号：AI识别板为 `KIT_PSE84_AI`（`PSE846GPS2DBZC4A` + `OV7675 DVP`）；主控为第三方 `STM32H743ZIT6` 开发板（鹿小班 LXB743ZI-P1）；两块跟踪板均为 `GOOUUU ESP32-S3-CAM N16R8 + OV2640`。
> 最终方案的四个水平轴均使用 SG90 9G 270°位置舵机，三个俯仰轴保持 SG90 9G 180°位置舵机。`材料/360°舵机.jpg`记录的是旧版“水平360°连续旋转、俯仰180°”方案，不作为当前接线和控制依据。

> 所有引脚均取自当前代码定义。标有“待复核”的链路在实物和官方原理图确认前不得通电，本文不代表整机已经完成硬件验证。

### 引脚复核状态（2026-11-04）

| 板子 | 依据 | 结论 |
|:---|:---|:---|
| ESP32-S3-CAM（GOOUUU） | 实物排针丝印照片（`材料/Esp32-S3-CAM.jpg`）+ 官方 esp32-camera 的 GOOUUU 定义 | GPIO1、GPIO2、GPIO14 均引出到排针且未被摄像头/PSRAM/USB 占用；摄像头数据线顺序已按官方定义改正 |
| KIT_PSE84_AI | 官方 BSP `cycfg_routing.h`/`cycfg_pins.h`、官方原理图 | P9_2=SCB1 UART TX、P9_3=SCB1 UART RX，与 h7_link.c 一致，无其他功能占用；J5 针序已查原理图，实物待用万用表确认 |
| STM32H743ZIT6 | Linux 内核 `pinctrl-stm32h743.c` 引脚表、工程 `.ioc`（UART4 不在 `.ioc` 中，由 `Core/init_wrappers.c` 手动配置）；未逐行对照 ST 数据手册 DS12110 | 所用引脚的复用功能（USART1/2/3 AF7、UART4 AF8、TIM2 AF1、TIM3 AF2）全部正确，且不占用 SWD(PA13/PA14)、USB(PA11/PA12)、晶振(PH0/PH1、PC14/PC15)。板型为鹿小班 LXB743ZI-P1，**厂家原理图缺失**，板上 LED/按键/TF 卡座/LCD/CAMERA 座占用哪些引脚尚未核对，见第 4 节注意事项 |

---

## 1. KIT_PSE84_AI ↔ STM32H743ZIT6（核心通信，UART）

**84E 侧 UART 引脚位于 J5 排针（官方用户指南称为 “Alternate serial interface header”），属于 1.8 V I/O 电源域。H7 为 3.3 V，两者之间必须加双向 1.8 V ↔ 3.3 V 电平转换器（例如 TXS0102/TXB0102 一类，或两路 BSS138 MOSFET 转换模块），禁止直连。**

| STM32H7 | → | 电平转换器 | → | KIT_PSE84_AI | 说明 |
|:-------:|:-:|:----------:|:-:|:------------:|:----|
| **PB6 (USART1_TX)** | 🔵 → | B(3.3V) ⇄ A(1.8V) | → | **P9_3 (SCB1 UART RX)，J5** | H7→84E 命令帧 |
| **PB7 (USART1_RX)** | 🟢 ← | B(3.3V) ⇄ A(1.8V) | ← | **P9_2 (SCB1 UART TX)，J5** | 84E→H7 结果帧 |
| **3.3 V** | → | VCCB | | | 取自 H7 板 3.3 V |
| | | VCCA | ← | **J5 第 1 针（VDD_1V8）** | 见下方 J5 引脚表 |
| **GND** | ⚫ → | GND | → | **J5 第 6 针（GND）** | 共地 |

J5 引脚（官方原理图 KIT_PSE84_AI 第 12 页）：

| 针 | 1 | 2 | 3 | 4 | 5 | 6 |
|:--:|:-:|:-:|:-:|:-:|:-:|:-:|
| 信号 | VDD_1V8 | P9_3 | P9_2 | P9_1 | P9_0 | GND |

原理图把 J5 标为 DNI，出厂通常不焊排针，需要自行焊接。第 1 针在板上的位置以丝印为准。

- 串口参数：115200 bps，8N1，无流控。帧格式 `AA 55 [len] [type] [payload] [XOR]`：H7→84E 为 type 0x01，载荷 22 B；84E→H7 为 type 0x02，载荷 21 B。见 `proj_cm55/source/h7_link.h`。
- 84E 侧代码：`Workspace/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision/proj_cm55/source/h7_link.c`。UART 由 CM55 直接驱动，使用 BSP 中已配置的 SCB1（`CYBSP_TENXER_UART`，引脚别名 `CYBSP_SERIAL_INT_2/3`）。
- 选择依据（官方 KIT_PSE84_AI 用户指南 002-41506 Rev.*A，及 BSP `design.modus`）：
  - 唯一的 3.3 V 排针 J14 已被 OV7675 DVP 摄像头占满（P16、P17、P11_1），没有空闲的 3.3 V UART 引脚。
  - 旧接线中的 P21_2/P21_3 在官方 BSP 中分别是 I2S TX SCK/MCK，P21_4~P21_7 是雷达 SPI，不能再作为 UART 使用。
  - J5 上的 P9_2/P9_3 在 BSP 中已经布线到 SCB1 UART TX/RX，且没有其他功能占用。
- J5 默认作为 GPIO 使用，不要贴装 R76/R77/R79/R80（这几颗电阻会把 J5 改接为烧录用的备用串口）。J5 为 6 pin 2.54 mm 排针。上面的针序来自官方原理图，**尚未在实物上核对**，接线前先用万用表确认第 1 针是 1.8 V、第 6 针是 GND。
- 旧方案中的 P21_4/P21_5 事件通知线（H7 PB8/PB9）已取消：这两个引脚是雷达 SPI。H7 的 PC1（`RST84E`）同样不要接 84E 的复位脚，该连接未定义，而且 84E 复位脚同属 1.8 V 域。
---

## 2. STM32H7 ↔ ESP32-A #1

| STM32H7 | → | ESP32-A #1 |
|:-------:|:-:|:----------:|
| **PD5 (USART2 TX)** | 🔵 → | **GPIO2 (UART1 RX)** |
| **PD6 (USART2 RX)** | 🟢 → | **GPIO1 (UART1 TX)** |
| **GND** | ⚫ → | **GND** |

> ESP32代码：`UART_H7=UART_NUM_1, TX=GPIO1, RX=GPIO2`。GPIO9/10属于板载OV2640，禁止用于H7 UART。

---

## 3. STM32H7 ↔ ESP32-B #2

| STM32H7 | → | ESP32-B #2 |
|:-------:|:-:|:----------:|
| **PD8 (USART3 TX)** | 🔵 → | **GPIO2 (UART1 RX)** |
| **PD9 (USART3 RX)** | 🟢 → | **GPIO1 (UART1 TX)** |
| **GND** | ⚫ → | **GND** |

> 两块 ESP32 固件相同，H7 通过 UART 路由区分（huart2=A, huart3=B）
> 两块板的角色选择使用GPIO14：A板接3.3V，B板接GND或悬空。GPIO14只作输入，不接舵机。

---

## 4. STM32H7 本地外设

> ⚠️ H7 板型号为 **鹿小班 LXB743ZI-P1**（背面丝印），正面有 LCD、CAMERA 两个排线座，背面有 TF 卡座。厂家原理图尚未取得。
> - TF 卡座在 STM32H7 上通常走 SDMMC1：PC8~PC12、PD2，其中 **PC10/PC11 就是本方案的调试串口 UART4**。
> - CAMERA 座通常走 DCMI，常用引脚包括 PA4、PA6、PB7、PC6、PC7 等，可能与 **PB7（84E 串口接收）、PC6（雷达 ECHO）、PC7（云台舵机）** 共线。
> - 板边 4 针弯排针的丝印有矛盾：宣传图（`材料/Stm32H7zit6.jpg`）芯片面印 CLK/DIO/GND/5V，丝印面照片模糊，疑似 RXD/TXD/RST/3V3。本方案接线不用它，**如要接烧录器或串口，先核对实物丝印并用万用表确认电源针**，不要按本文或 3D 工具的标注直接接。
> - **使用本方案时：TF 卡槽不插卡，LCD/CAMERA 座不插模块。** 这样这些座子上只剩上拉电阻，串口空闲为高电平、ECHO 由模块主动拉低，一般不影响工作；拿到原理图后仍需确认。
> - 固件还把 PC2 配置为 LED 输出、PA4~PA7 配置为 SPI1（未使用）。若原理图显示板载按键接在任一输出引脚（常见为 PA0 或 PC13），按下按键会短路输出，需改引脚。

### HC-SR04 超声波雷达

| HC-SR04 | → | STM32H7 |
|:-------:|:-:|:-------:|
| **TRIG** | 🟠 → | **PC0（GPIO 推挽输出）** |
| **ECHO** | 🟡 → | **PC6（TIM3_CH1 输入捕获）** ← 分压 2.2kΩ+3.3kΩ |
| **VCC** | 🔴 → | **5V** |
| **GND** | ⚫ → | **GND** |

> ⚠️ ECHO 引脚需 5V→3.3V 分压再接入 PC6
> 代码位置: `Radar/radar_driver.h` → `RADAR_TRIG=PC0, RADAR_ECHO=PC6(TIM3_CH1)`

### 调试/实验日志串口（UART4）

| STM32H7 | → | USB-TTL |
|:-------:|:-:|:-------:|
| **PC10 (UART4_TX)** | 🔵 → | **RX** |
| **PC11 (UART4_RX)** | 🟢 ← | **TX** |
| **GND** | ⚫ → | **GND** |

> 921600 8N1。TX 输出 H7 全部日志和 `$E` 实验事件（原先混在 ESP32-A 的 USART2 链路里）；
> RX 接收命令：`STOP`（所有云台回初始朝向并暂停）、`START`（恢复）、`STATUS`（打印状态），回车结束。
> PC10/PC11 在第三方底板上是否空闲需对照底板原理图确认。用法见 `code/EXPERIMENTS.md`。
> 另可在 `Core/console.h` 中启用实体停机按键（需按底板填写引脚）。

### 已取消的连线（不要接）

- H7 的 PB8、PB9、PB12、PB13（原 J5 事件通知线）和 PC1（原 RST84E）：固件已不再驱动这些引脚，84E 侧对应引脚属 1.8 V 域，见第 1 节。

### 舵机组（全部由 STM32H7 PWM 直控）

PWM频率暂定50Hz；中心及两端脉宽必须依据270°和180°实物逐只校准，不能把500/1500/2500us直接视为安全值。

#### 雷达水平舵机（SG90 9G 270°位置舵机）

| 舵机 | → | STM32H7 |
|:----:|:-:|:-------:|
| 信号线 | 🟤 → | **PA0 (TIM2_CH1 PWM)** |

#### Camera0（GOOUUU ESP32-S3-CAM N16R8-A + OV2640）云台

| 舵机 | → | STM32H7 | 说明 |
|:----:|:-:|:-------:|:----|
| 水平(Pan) | 🟤 → | **PA1 (TIM2_CH2 PWM)** |
| 垂直(Tilt) | 🟤 → | **PA2 (TIM2_CH3 PWM)** |

#### Camera1（GOOUUU ESP32-S3-CAM N16R8-B + OV2640）云台

| 舵机 | → | STM32H7 | 说明 |
|:----:|:-:|:-------:|:----|
| 水平(Pan) | 🟤 → | **PA3 (TIM2_CH4 PWM)** |
| 垂直(Tilt) | 🟤 → | **PC7 (TIM3_CH2 PWM)** |

#### Camera2（KIT_PSE84_AI + OV7675 DVP）云台

| 舵机 | → | STM32H7 | 说明 |
|:----:|:-:|:-------:|:----|
| 水平(Pan) | 🟤 → | **PB0 (TIM3_CH3 PWM)** |
| 垂直(Tilt) | 🟤 → | **PB1 (TIM3_CH4 PWM)** |

> 代码位置: `STM32H7/Drivers/servo.c`（雷达）+ `Drivers/gimbal.c`（6路云台）
> ESP32 和 PSE84E 不控制舵机，只做视觉处理

---

## 5. KIT_PSE84_AI（PSE846GPS2DBZC4A）本地外设

### 状态 LED（无需外接）

旧版要求在 P1.7 外接 LED，但 84E 工程中没有任何代码驱动 P1.7，该线已取消。状态指示使用板载 `CYBSP_USER_LED1/LED2`（见 `proj_cm55/source/dataset_capture.c`）。

---

## 6. GOOUUU ESP32-S3-CAM N16R8板载OV2640（每套独立）

| OV2640 | → | ESP32-S3 | 说明 |
|:------:|:-:|:-----:|:----|
| **XCLK** | 🟤 → | **GPIO15** | 摄像头时钟 |
| **SIOD (SDA)** | ⚪ → | **GPIO4** | SCCB 数据 |
| **SIOC (SCL)** | ⚪ → | **GPIO5** | SCCB 时钟 |
| **VSYNC** | 🟠 → | **GPIO6** | 帧同步 |
| **HREF** | 🟡 → | **GPIO7** | 行同步 |
| **PCLK** | 🟡 → | **GPIO13** | 像素时钟 |
| **PWDN** | — | **未连接（代码为 -1）** | 使用软件初始化 |
| **RESET** | — | **未连接（代码为 -1）** | 使用软件复位 |
| **Y9 (D7)** | 🔵 → | **GPIO16** | 数据线 D7 MSB |
| **Y8 (D6)** | 🔵 → | **GPIO17** | 数据线 D6 |
| **Y7 (D5)** | 🔵 → | **GPIO18** | 数据线 D5 |
| **Y6 (D4)** | 🔵 → | **GPIO12** | 数据线 D4 |
| **Y5 (D3)** | 🔵 → | **GPIO10** | 数据线 D3 |
| **Y4 (D2)** | 🔵 → | **GPIO8** | 数据线 D2 |
| **Y3 (D1)** | 🔵 → | **GPIO9** | 数据线 D1 |
| **Y2 (D0)** | 🔵 → | **GPIO11** | 数据线 D0 LSB |

> 代码位置: `ESP32_ov2640/Drivers/ov2640/ov2640.c` → `s_default_pins`
> 依据：Espressif 官方 esp32-camera 示例 `camera_pinout.h` 中的 `BOARD_ESP32S3_GOOUUU`。低 4 位不按 GPIO 编号顺序，旧版写成 8/9/10/11 会使图像低位错乱。
> 这些线在板内已连好，无需杜邦线；本表只用于核对代码。
> GPIO43/GPIO44为板载USB转串口连接的UART0 TX/RX，不作为OV2640 PWDN/RESET。

### ESP32云台控制边界

ESP32-S3只负责视觉计算和向H7回传PID控制量，不直接驱动舵机。GPIO12属于板载OV2640数据线，GPIO14用于双板角色检测，均不得连接舵机信号线。全部云台舵机由STM32H743ZIT6统一输出PWM。

---

## 7. 总体供电

| 设备 | 供电 | 电流 |
|:----:|:----:|:----:|
| **KIT_PSE84_AI** | 按官方套件指南由底板供电 | 待实测 |
| **STM32H7** | 按第三方底板说明供电 | 待实测 |
| **ESP32-A** | 按GOOUUU底板输入规格供电，不从H7取电 | 待实测 |
| **ESP32-B** | 按GOOUUU底板输入规格供电，不从H7取电 | 待实测 |
| **HC-SR04** | 5V | 待实测 |
| **全部舵机 ×7** | 外部独立电源（不要从任何开发板或USB取电） | 电源型号、输出电压和额定电流待确定 |

> 舵机电源与H7必须共地。电源规格和逐只舵机堵转/峰值电流未确认前，禁止七只舵机同时通电测试。

---

## 8. 杜邦线速查总表

| 源 | 源引脚 | 线色 | 目标引脚 | 目标 |
|:--:|:------:|:----:|:--------:|:----:|
| PSE84E | **P9_2 (SCB1 TX, J5, 1.8V)** | 🔵 | 电平转换 → **PB7 (USART1 RX)** | H7 |
| PSE84E | **P9_3 (SCB1 RX, J5, 1.8V)** | 🟢 | 电平转换 ← **PB6 (USART1 TX)** | H7 |
| H7 | **PD5 (USART2 TX)** | 🔵 | **GPIO2 (UART1 RX)** | ESP32-A |
| H7 | **PD6 (USART2 RX)** | 🟢 | **GPIO1 (UART1 TX)** | ESP32-A |
| H7 | **PD8 (USART3 TX)** | 🔵 | **GPIO2 (UART1 RX)** | ESP32-B |
| H7 | **PD9 (USART3 RX)** | 🟢 | **GPIO1 (UART1 TX)** | ESP32-B |
| H7 | **PC0 (TRIG)** | 🟠 | **TRIG** | HC-SR04 |
| H7 | **PC6 (ECHO)** | 🟡 | **ECHO** ←分压 | HC-SR04 |
| H7 | **PA0 (TIM2_CH1)** | 🟤 | 信号线 | 雷达舵机 |
| 全体 | **GND** | ⚫ | **GND** | 共地 |

---

## 一句话规则

> **TX 连对面 RX，蓝绿交叉（TX→RX, RX→TX）；GND 全接在一起**
> **ESP32 和舵机外部独立供电，不要从主板取电**
> **HC-SR04 ECHO 必须分压（2.2kΩ+3.3kΩ）后再接 PC6**
