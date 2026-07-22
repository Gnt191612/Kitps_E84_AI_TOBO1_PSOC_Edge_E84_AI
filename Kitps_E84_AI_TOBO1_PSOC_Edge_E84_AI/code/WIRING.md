# 四板接线指南 — 杜邦线速查表（代码同步版）

> 所有引脚均取自各板代码中的实际定义，按此接线+烧录即可工作。

---

## 1. PSE84E ↔ STM32H7（核心通信）

**仅使用 P21 引脚（位操 UART，115200bps）**

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
| **PD5 (USART2 TX)** | 🔵 → | **GPIO9 (UART1 RX)** |
| **PD6 (USART2 RX)** | 🟢 → | **GPIO10 (UART1 TX)** |
| **GND** | ⚫ → | **GND** |

> ESP32 代码: `ESP32_ov2640/Core/system.h` → `UART_H7=UART_NUM_1, TX=GPIO10, RX=GPIO9`

---

## 3. STM32H7 ↔ ESP32-B #2

| STM32H7 | → | ESP32-B #2 |
|:-------:|:-:|:----------:|
| **PD8 (USART3 TX)** | 🔵 → | **GPIO9 (UART1 RX)** |
| **PD9 (USART3 RX)** | 🟢 → | **GPIO10 (UART1 TX)** |
| **GND** | ⚫ → | **GND** |

> 两块 ESP32 固件相同，H7 通过 UART 路由区分（huart2=A, huart3=B）

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

所有舵机规格：PSC=199, ARR=19999, 50Hz, CCR=500/1500/2500 (0°/90°/180°)

#### 雷达旋转舵机（360°）

| 舵机 | → | STM32H7 |
|:----:|:-:|:-------:|
| 信号线 | 🟤 → | **PA0 (TIM2_CH1 PWM)** |

#### Camera0（ESP32-A OV2640）云台

| 舵机 | → | STM32H7 | 说明 |
|:----:|:-:|:-------:|:----|
| 水平(Pan) | 🟤 → | **PA1 (TIM2_CH2 PWM)** |
| 垂直(Tilt) | 🟤 → | **PA2 (TIM2_CH3 PWM)** |

#### Camera1（ESP32-B OV2640）云台

| 舵机 | → | STM32H7 | 说明 |
|:----:|:-:|:-------:|:----|
| 水平(Pan) | 🟤 → | **PA3 (TIM2_CH4 PWM)** |
| 垂直(Tilt) | 🟤 → | **PC7 (TIM3_CH2 PWM)** |

#### Camera2（PSE84E + OV7675）云台

| 舵机 | → | STM32H7 | 说明 |
|:----:|:-:|:-------:|:----|
| 水平(Pan) | 🟤 → | **PB0 (TIM3_CH3 PWM)** |
| 垂直(Tilt) | 🟤 → | **PB1 (TIM3_CH4 PWM)** |

> 代码位置: `STM32H7/Drivers/servo.c`（雷达）+ `Drivers/gimbal.c`（6路云台）
> ESP32 和 PSE84E 不控制舵机，只做视觉处理

---

## 5. PSE84E 本地外设

### 状态 LED

| LED | → | PSE84E |
|:---:|:-:|:------:|
| 阳极(+) | 🟡 → | **P1.7 (GPIO_PRT1)** |
| 阴极(-) | ⚫ → | GND（串 220Ω 电阻） |

---

## 6. ESP32 摄像头 OV2640（每套独立）

| OV2640 | → | ESP32 | 说明 |
|:------:|:-:|:-----:|:----|
| **XCLK** | 🟤 → | **GPIO4** | 摄像头时钟 |
| **SIOD (SDA)** | ⚪ → | **GPIO18** | SCCB 数据 |
| **SIOC (SCL)** | ⚪ → | **GPIO23** | SCCB 时钟 |
| **VSYNC** | 🟠 → | **GPIO27** | 帧同步 |
| **HREF** | 🟡 → | **GPIO35** | 行同步（输入专用） |
| **PCLK** | 🟡 → | **GPIO22** | 像素时钟 |
| **PWDN** | 🟤 → | **GPIO32** | 拉低使能 |
| **RESET** | 🟤 → | **GPIO33** | 拉低再释放复位 |
| **Y9 (D7)** | 🔵 → | **GPIO16** | 数据线 D7 MSB |
| **Y8 (D6)** | 🔵 → | **GPIO5** | 数据线 D6 |
| **Y7 (D5)** | 🔵 → | **GPIO17** | 数据线 D5 |
| **Y6 (D4)** | 🔵 → | **GPIO21** | 数据线 D4 |
| **Y5 (D3)** | 🔵 → | **GPIO19** | 数据线 D3 |
| **Y4 (D2)** | 🔵 → | **GPIO26** | 数据线 D2 |
| **Y3 (D1)** | 🔵 → | **GPIO25** | 数据线 D1 |
| **Y2 (D0)** | 🔵 → | **GPIO34** | 数据线 D0 LSB（仅输入） |

> 代码位置: `ESP32_ov2640/Drivers/ov2640/ov2640.c` → `s_default_pins`

### ESP32 云台舵机

| 舵机 | → | ESP32 | 说明 |
|:----:|:-:|:-----:|:----|
| 水平(Pan) | 🟤 → | **GPIO12** | LEDC_CHANNEL_2 |
| 垂直(Tilt) | 🟤 → | **GPIO14** | LEDC_CHANNEL_3 |
| 电源 | 🔴 → | 外部 5V | 不要从 ESP32 取电 |
| GND | ⚫ → | GND | 共地 |

> ⚠️ 摄像头 XCLK 占用 LEDC_CHANNEL_0，舵机避开
> 代码: `Core/system.c` → `PWM_Init()`

---

## 7. 总体供电

| 设备 | 供电 | 电流 |
|:----:|:----:|:----:|
| **PSE84E** | USB 5V / 外部 5V | ~200mA |
| **STM32H7** | USB 5V / 外部 5V | ~300mA |
| **ESP32-A** | 独立 3.3V LDO（不要从 H7 取） | ~500mA |
| **ESP32-B** | 独立 3.3V LDO（不要从 H7 取） | ~500mA |
| **HC-SR04** | STM32H7 5V | ~15mA |
| **H7 平台舵机 ×7** | 外部 5V（不要从板子取） | ~600mA |
| **ESP32 云台舵机 ×2** | 外部 5V | ~400mA |

---

## 8. 杜邦线速查总表

| 源 | 源引脚 | 线色 | 目标引脚 | 目标 |
|:--:|:------:|:----:|:--------:|:----:|
| PSE84E | **P21_2 (TX)** | 🔵 | **PB7 (USART1 RX)** | H7 |
| PSE84E | **P21_3 (RX)** | 🟢 | **PB6 (USART1 TX)** | H7 |
| PSE84E | **P1.7 (LED+)** | 🟡 | LED→220Ω→GND | — |
| H7 | **PD5 (USART2 TX)** | 🔵 | **GPIO9 (UART1 RX)** | ESP32-A |
| H7 | **PD6 (USART2 RX)** | 🟢 | **GPIO10 (UART1 TX)** | ESP32-A |
| H7 | **PD8 (USART3 TX)** | 🔵 | **GPIO9 (UART1 RX)** | ESP32-B |
| H7 | **PD9 (USART3 RX)** | 🟢 | **GPIO10 (UART1 TX)** | ESP32-B |
| H7 | **PC0 (TRIG)** | 🟠 | **TRIG** | HC-SR04 |
| H7 | **PC6 (ECHO)** | 🟡 | **ECHO** ←分压 | HC-SR04 |
| H7 | **PA0 (TIM2_CH1)** | 🟤 | 信号线 | 雷达舵机 |
| 全体 | **GND** | ⚫ | **GND** | 共地 |

---

## 一句话规则

> **TX 连对面 RX，蓝绿交叉（TX→RX, RX→TX）；GND 全接在一起**
> **ESP32 和舵机外部独立供电，不要从主板取电**
> **HC-SR04 ECHO 必须分压（2.2kΩ+3.3kΩ）后再接 PC6**
