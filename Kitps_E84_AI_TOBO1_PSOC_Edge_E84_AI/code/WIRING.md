# 四板接线指南 — 杜邦线速查表（型号已确认，待完成板级引脚复核）

> 硬件型号：AI识别板为 `KIT_PSE84_AI`（`PSE846GPS2DBZC4A` + `OV7675 DVP`）；主控为第三方 `STM32H743ZIT6` 开发板；两块跟踪板均为 `GOOUUU ESP32-S3-CAM N16R8 + OV2640`。
> 最终方案的四个水平轴均使用 SG90 9G 270°位置舵机，三个俯仰轴保持 SG90 9G 180°位置舵机。`材料/360°舵机.jpg`记录的是旧版“水平360°连续旋转、俯仰180°”方案，不作为当前接线和控制依据。

> 所有引脚均取自当前代码定义。标有“待复核”的链路在实物和官方原理图确认前不得通电，本文不代表整机已经完成硬件验证。

---

## 1. KIT_PSE84_AI ↔ STM32H743ZIT6（核心通信，UART）

**84E 侧 UART 引脚位于 J5 排针（官方用户指南称为 “Alternate serial interface header”），属于 1.8 V I/O 电源域。H7 为 3.3 V，两者之间必须加双向 1.8 V ↔ 3.3 V 电平转换器（例如 TXS0102/TXB0102 一类，或两路 BSS138 MOSFET 转换模块），禁止直连。**

| STM32H7 | → | 电平转换器 | → | KIT_PSE84_AI | 说明 |
|:-------:|:-:|:----------:|:-:|:------------:|:----|
| **PB6 (USART1_TX)** | 🔵 → | B(3.3V) ⇄ A(1.8V) | → | **P9_3 (SCB1 UART RX)，J5** | H7→84E 命令帧 |
| **PB7 (USART1_RX)** | 🟢 ← | B(3.3V) ⇄ A(1.8V) | ← | **P9_2 (SCB1 UART TX)，J5** | 84E→H7 结果帧 |
| **3.3 V** | → | VCCB | | | 取自 H7 板 3.3 V |
| | | VCCA | ← | **1.8 V（KIT_PSE84_AI 板上 1.8 V 电源）** | 引出位置以官方原理图为准 |
| **GND** | ⚫ → | GND | → | **GND** | 共地 |

- 串口参数：115200 bps，8N1，无流控。帧格式 `AA 55 [len] [type] [payload] [XOR]`：H7→84E 为 type 0x01，载荷 22 B；84E→H7 为 type 0x02，载荷 21 B。见 `proj_cm55/source/h7_link.h`。
- 84E 侧代码：`Workspace/PSOC_Edge_Machine_Learning_DEEPCRAFT_Deploy_Vision/proj_cm55/source/h7_link.c`。UART 由 CM55 直接驱动，使用 BSP 中已配置的 SCB1（`CYBSP_TENXER_UART`，引脚别名 `CYBSP_SERIAL_INT_2/3`）。
- 选择依据（官方 KIT_PSE84_AI 用户指南 002-41506 Rev.*A，及 BSP `design.modus`）：
  - 唯一的 3.3 V 排针 J14 已被 OV7675 DVP 摄像头占满（P16、P17、P11_1），没有空闲的 3.3 V UART 引脚。
  - 旧接线中的 P21_2/P21_3 在官方 BSP 中分别是 I2S TX SCK/MCK，P21_4~P21_7 是雷达 SPI，不能再作为 UART 使用。
  - J5 上的 P9_2/P9_3 在 BSP 中已经布线到 SCB1 UART TX/RX，且没有其他功能占用。
- J5 默认作为 GPIO 使用，不要贴装 R76/R77/R79/R80（这几颗电阻会把 J5 改接为烧录用的备用串口）。部分板子的 J5 排针可能未焊接，需自行焊接 6 pin 2.54 mm 排针。具体 J5 引脚序号以官方原理图为准，**尚未在实物上核对**。
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
| PSE84E | **P9_2 (SCB1 TX, J5, 1.8V)** | 🔵 | 电平转换 → **PB7 (USART1 RX)** | H7 |
| PSE84E | **P9_3 (SCB1 RX, J5, 1.8V)** | 🟢 | 电平转换 ← **PB6 (USART1 TX)** | H7 |
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
