# 四板接线指南 — 杜邦线速查表（型号已确认，待完成板级引脚复核）

> 硬件型号：AI识别板为 `KIT_PSE84_AI`（`PSE846GPS2DBZC4A` + `OV7675 DVP`）；主控为第三方 `STM32H743ZIT6` 开发板；两块跟踪板均为 `GOOUUU ESP32-S3-CAM N16R8 + OV2640`。
> 水平轴计划使用 SG90 9G 270°位置舵机，具体商品尚待购买确认；俯仰轴使用 SG90 9G 180°位置舵机。

> 所有引脚均取自当前代码定义。标有“待复核”的链路在实物和官方原理图确认前不得通电，本文不代表整机已经完成硬件验证。

---

## 1. KIT_PSE84_AI ↔ STM32H743ZIT6（核心通信）

**以下旧接线尚未通过 KIT_PSE84_AI 官方原理图复核。P21属于1.8V I/O电源域，禁止把H7的3.3V输出直接接入。**

| STM32H7 | → | PSE84E | 说明 |
|:-------:|:-:|:------:|:----|
| **PB6 (USART1_TX)** | 🔵 → | **P21_3 (BB UART RX, 原INT1)** | H7→84E 数据 |
| **PB7 (USART1_RX)** | 🟢 → | **P21_2 (BB UART TX, 原INT0)** | 84E→H7 数据 |
| **GND** | ⚫ → | **GND** | 共地 |
| **PB8** | 🟡 → | **P21_4 (INT2)** | 事件通知（可选） |
| **PB9** | 🟠 → | **P21_5 (INT3)** | 事件通知（可选） |

> PSE84E 侧使用位操 UART 驱动：`Drivers/bitbanged_uart/bb_uart.c`
> P21_2/P21_3 原本为 INT0/INT1，现用作 UART TX/RX
> P21_4/P21_5 保留为 INT2/INT3 事件通知

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

### HC-SR04 超声波雷达

| HC-SR04 | → | STM32H7 |
|:-------:|:-:|:-------:|
| **TRIG** | 🟠 → | **PC0（GPIO 推挽输出）** |
| **ECHO** | 🟡 → | **PC6（TIM3_CH1 输入捕获）** ← 分压 2.2kΩ+3.3kΩ |
| **VCC** | 🔴 → | **5V** |
| **GND** | ⚫ → | **GND** |

> ⚠️ ECHO 引脚需 5V→3.3V 分压再接入 PC6
> 代码位置: `Radar/radar_driver.h` → `RADAR_TRIG=PC0, RADAR_ECHO=PC6(TIM3_CH1)`

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

### 状态 LED

| LED | → | PSE84E |
|:---:|:-:|:------:|
| 阳极(+) | 🟡 → | **P1.7 (GPIO_PRT1)** |
| 阴极(-) | ⚫ → | GND（串 220Ω 电阻） |

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
| **Y5 (D3)** | 🔵 → | **GPIO11** | 数据线 D3 |
| **Y4 (D2)** | 🔵 → | **GPIO10** | 数据线 D2 |
| **Y3 (D1)** | 🔵 → | **GPIO9** | 数据线 D1 |
| **Y2 (D0)** | 🔵 → | **GPIO8** | 数据线 D0 LSB |

> 代码位置: `ESP32_ov2640/Drivers/ov2640/ov2640.c` → `s_default_pins`
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
| PSE84E | **P21_2 (TX)** | 🔵 | **PB7 (USART1 RX)** | H7 |
| PSE84E | **P21_3 (RX)** | 🟢 | **PB6 (USART1 TX)** | H7 |
| PSE84E | **P1.7 (LED+)** | 🟡 | LED→220Ω→GND | — |
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
