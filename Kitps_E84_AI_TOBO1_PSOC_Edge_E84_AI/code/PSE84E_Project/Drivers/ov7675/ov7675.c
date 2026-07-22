/**
 * @file ov7675.c
 * @brief OV7675 摄像头驱动实�? *
 * 实现细节�? *   - SCCB 总线基于位操�?GPIO（兼�?I2C 时序�? *   - 数据读取通过 GPIO 端口并行采样 D[7:0]
 *   - XCLK 通过 TCPWM 模块输出 ~12MHz 方波
 *   - 帧采集使�?VSYNC/HREF/PCLK 轮询
 *
 * 硬件初始化顺序：
 *   OV7675_Init()
 *     ├─ CAM_RST 拉低 100ms �?释放 (RST_N 低有�?
 *     ├─ SCCB 总线确认
 *     ├─ �?PID/VER 验证器件存在
 *     ├─ 写入初始化寄存器序列 (QQVGA Y-channel)
 *     ├─ 启动 XCLK (TCPWM ~12MHz)
 *     └─ 等待 100ms 稳定
 *
 * 帧采集流程：
 *   OV7675_CaptureFrame(buf)
 *     ├─ 检�?VSYNC 上升�? *     ├─ 检�?VSYNC 下降沿（数据开始）
 *     ├─ for row=0..119:
 *     �?  ├─ 等待 HREF 上升�? *     �?  └─ for col=0..159:
 *     �?      └─ 等待 PCLK 上升�?�?�?D[7:0] �?写入 buf
 *     └─ return 0
 */

#include "ov7675.h"
#include "ov7675_regs.h"
#include "gpio.h"
#include "logger.h"
#include "XMC8400E.h"
#include "cy_tcpwm.h"
#include <string.h>

/* GPIO_Init / GPIO_WritePin / GPIO_ReadPin 来自 gpio.h */
/* Cy_GPIO_SetDrivemode 来自 cy_pdl.h (通过 gpio.h 间接引用) */

/* ======================================================================== */
/* 内部标志                                                                  */
/* ======================================================================== */
static uint8_t s_initialized = 0;

/* ======================================================================== */
/* SCCB 位操�?�?基于 GPIO                                                  */
/* 时序：SCL 半周�?~2us (@250kHz)                                         */
/* ======================================================================== */

static void sccb_delay(void)
{
    /* 简易忙等待延迟 ~2us (160MHz 下约 320 个周�? */
    volatile uint32_t d = 80;
    while (d--) { __asm__("nop"); }
}

static void scl_high(void)
{
    Cy_GPIO_Write(CAM_SCL_PORT, CAM_SCL_PIN, 1);
}

static void scl_low(void)
{
    Cy_GPIO_Write(CAM_SCL_PORT, CAM_SCL_PIN, 0);
}

static void sda_high(void)
{
    Cy_GPIO_Write(CAM_SDA_PORT, CAM_SDA_PIN, 1);
}

static void sda_low(void)
{
    Cy_GPIO_Write(CAM_SDA_PORT, CAM_SDA_PIN, 0);
}

static void sda_input(void)
{
    Cy_GPIO_SetDrivemode(CAM_SDA_PORT, CAM_SDA_PIN,
                         CY_GPIO_DM_HIGHZ);
}

static void sda_output(void)
{
    Cy_GPIO_SetDrivemode(CAM_SDA_PORT, CAM_SDA_PIN,
                         CY_GPIO_DM_STRONG);
}

static uint8_t sda_read(void)
{
    return Cy_GPIO_Read(CAM_SDA_PORT, CAM_SDA_PIN);
}

/* SCCB 起始条件：SCL 高时，SDA 从高→低 */
static void sccb_start(void)
{
    sda_output();
    sda_high();
    scl_high();
    sccb_delay();
    sda_low();
    sccb_delay();
    scl_low();
    sccb_delay();
}

/* SCCB 停止条件：SCL 高时，SDA 从低→高 */
static void sccb_stop(void)
{
    sda_output();
    sda_low();
    scl_high();
    sccb_delay();
    sda_high();
    sccb_delay();
}

/* SCCB 写一个字节（MSB first），返回 NAK (0=ACK 收到, 1=NAK) */
static uint8_t sccb_write_byte(uint8_t data)
{
    sda_output();
    for (int i = 7; i >= 0; i--) {
        if (data & (1 << i))
            sda_high();
        else
            sda_low();
        sccb_delay();
        scl_high();
        sccb_delay();
        scl_low();
        sccb_delay();
    }
    /* �?9 位：SDA 释放，读 ACK/NAK */
    sda_input();
    sccb_delay();
    scl_high();
    sccb_delay();
    uint8_t nak = sda_read();
    scl_low();
    sccb_delay();
    sda_output();
    return nak;
}

/* SCCB 读一个字节（MSB first），返回读取�?*/
/* ack: 0=主设备发�?ACK, 1=主设备发�?NAK */
static uint8_t sccb_read_byte(uint8_t ack)
{
    uint8_t data = 0;
    sda_input();
    for (int i = 7; i >= 0; i--) {
        scl_high();
        sccb_delay();
        if (sda_read()) data |= (1 << i);
        scl_low();
        sccb_delay();
    }
    /* �?9 位：主设�?ACK/NAK */
    sda_output();
    if (ack)
        sda_high();     /* NAK: 最后一个字�?*/
    else
        sda_low();      /* ACK: 还要继续�?*/
    sccb_delay();
    scl_high();
    sccb_delay();
    scl_low();
    sccb_delay();
    sda_input();
    return data;
}

/* ======================================================================== */
/* SCCB 写寄存器�? 相写�? Start + DevAddr(W) + SubAddr + Data + Stop      */
/* ======================================================================== */
int OV7675_WriteReg(uint8_t reg, uint8_t val)
{
    sccb_start();
    if (sccb_write_byte(OV7675_ADDR)) {    /* 器件地址 + �?*/
        sccb_stop();
        return -1;
    }
    if (sccb_write_byte(reg)) {             /* 子地址 */
        sccb_stop();
        return -1;
    }
    if (sccb_write_byte(val)) {             /* 数据 */
        sccb_stop();
        return -1;
    }
    sccb_stop();
    return 0;
}

/* ======================================================================== */
/* SCCB 读寄存器�? 相读�? Start+DevAddr(W)+SubAddr + Stop                  */
/*                           Start+DevAddr(R)+Data + Stop                    */
/* ======================================================================== */
int OV7675_ReadReg(uint8_t reg, uint8_t *val)
{
    if (!val) return -1;

    /* Phase 1: 写子地址 */
    sccb_start();
    if (sccb_write_byte(OV7675_ADDR)) {
        sccb_stop();
        return -1;
    }
    if (sccb_write_byte(reg)) {
        sccb_stop();
        return -1;
    }
    sccb_stop();

    /* Phase 2: 读数�?*/
    sccb_start();
    if (sccb_write_byte(OV7675_ADDR_READ)) {
        sccb_stop();
        return -1;
    }
    *val = sccb_read_byte(1);   /* NAK: 只读一个字�?*/
    sccb_stop();

    return 0;
}

/* ======================================================================== */
/* GPIO 端口�?�?读取 D[7:0] 并组合为一个字�?                               */
/*                                                                           */
/* 优化建议：如�?CAM_D_PORT �?D0~D7 正好对应 Px0~Px7�?                    */
/* 可以直接�?GPIO 端口输入寄存器，不逐位�?Cy_GPIO_Read�?                  */
/* ======================================================================== */
static inline uint8_t read_data_byte(void)
{
    uint8_t byte = 0;
    /* 逐位�?*/
    if (Cy_GPIO_Read(CAM_D7_PORT, CAM_D7_PIN)) byte |= 0x80;
    if (Cy_GPIO_Read(CAM_D6_PORT, CAM_D6_PIN)) byte |= 0x40;
    if (Cy_GPIO_Read(CAM_D5_PORT, CAM_D5_PIN)) byte |= 0x20;
    if (Cy_GPIO_Read(CAM_D4_PORT, CAM_D4_PIN)) byte |= 0x10;
    if (Cy_GPIO_Read(CAM_D3_PORT, CAM_D3_PIN)) byte |= 0x08;
    if (Cy_GPIO_Read(CAM_D2_PORT, CAM_D2_PIN)) byte |= 0x04;
    if (Cy_GPIO_Read(CAM_D1_PORT, CAM_D1_PIN)) byte |= 0x02;
    if (Cy_GPIO_Read(CAM_D0_PORT, CAM_D0_PIN)) byte |= 0x01;
    return byte;
}

/* ======================================================================== */
/* 帧采�?�?阻塞式并行读�?                                                 */
/*                                                                           */
/* 时序约束：在 PCLK 上升沿采�?D[7:0]�?                                   */
/* QQVGA 下典�?PCLK �?6MHz (周期 ~167ns)�?                                  */
/* 160MHz CPU �?27 周期可用，每个像素约 6~8 �?Cy_GPIO_Read 够用�?        */
/*                                                                           */
/* 如果出现行错�?/ 花屏，可能原因是�?                                     */
/*   - PCLK 上升沿检测不准确（需用边沿中断代替轮询）                        */
/*   - HREF 延迟配置不对（调�?REG_HREF �?HREF_EDGE�?                     */
/*   - 摄像头时钟不稳（检�?XCLK 频率 + 减少 PLL 倍频�?                    */
/* ======================================================================== */
int OV7675_CaptureFrame(uint8_t *buf)
{
    if (!s_initialized) return -4;
    if (!buf) return -1;

    /* ---- Step 1: 等待 VSYNC 上升沿（帧准备开始） ---- */
    uint32_t timeout = CAM_CAPTURE_TIMEOUT_MS;
    while (Cy_GPIO_Read(CAM_VSYNC_PORT, CAM_VSYNC_PIN) == 0) {
        /* 忙等�?*/
        volatile uint32_t d = 4000; /* ~1ms */
        while (d--) { __asm__("nop"); }
        if (--timeout == 0) return -1;
    }

    /* ---- Step 2: 等待 VSYNC 下降沿（数据有效�?---- */
    timeout = CAM_CAPTURE_TIMEOUT_MS;
    while (Cy_GPIO_Read(CAM_VSYNC_PORT, CAM_VSYNC_PIN) == 1) {
        volatile uint32_t d = 4000;
        while (d--) { __asm__("nop"); }
        if (--timeout == 0) return -1;
    }

    /* ---- Step 3: 逐行读取 ---- */
    for (int row = 0; row < OV7675_HEIGHT; row++) {
        /* 等待 HREF 上升�?*/
        uint32_t line_timeout = CAM_LINE_TIMEOUT_US;
        while (Cy_GPIO_Read(CAM_HREF_PORT, CAM_HREF_PIN) == 0) {
            /* spin ~1us */
            volatile uint32_t d = 40;
            while (d--) { __asm__("nop"); }
            if (--line_timeout == 0) return -2;
        }

        /* 读取该行 160 个像�?*/
        for (int col = 0; col < OV7675_WIDTH; col++) {
            /* 等待 PCLK 上升�?*/
            uint32_t px_timeout = CAM_PIXEL_TIMEOUT_US;
            while (Cy_GPIO_Read(CAM_PCLK_PORT, CAM_PCLK_PIN) == 0) {
                volatile uint32_t d = 10;
                while (d--) { __asm__("nop"); }
                if (--px_timeout == 0) return -3;
            }

            /* 读取数据 */
            buf[row * OV7675_WIDTH + col] = read_data_byte();

            /* 等待 PCLK 下降沿（准备下一像素�?*/
            px_timeout = CAM_PIXEL_TIMEOUT_US;
            while (Cy_GPIO_Read(CAM_PCLK_PORT, CAM_PCLK_PIN) == 1) {
                volatile uint32_t d = 10;
                while (d--) { __asm__("nop"); }
                if (--px_timeout == 0) return -3;
            }
        }

        /* 等待 HREF 下降�?*/
        uint32_t href_timeout = CAM_LINE_TIMEOUT_US;
        while (Cy_GPIO_Read(CAM_HREF_PORT, CAM_HREF_PIN) == 1) {
            volatile uint32_t d = 40;
            while (d--) { __asm__("nop"); }
            if (--href_timeout == 0) return -2;
        }
    }

    return 0;
}

/* ======================================================================== */
/* 读取产品 ID                                                               */
/* ======================================================================== */
int OV7675_ReadID(uint16_t *mid, uint16_t *pid)
{
    uint8_t h, l;

    if (OV7675_ReadReg(REG_MIDH, &h)) return -1;
    if (OV7675_ReadReg(REG_MIDL, &l)) return -1;
    if (mid) *mid = ((uint16_t)h << 8) | l;

    if (OV7675_ReadReg(REG_PID, &h)) return -1;
    if (OV7675_ReadReg(REG_VER, &l)) return -1;
    if (pid) *pid = ((uint16_t)h << 8) | l;

    return 0;
}

/* ======================================================================== */
/* OV7675 初始化寄存器�?                                                    */
/*                                                                           */
/* QQVGA (160x120), Y (灰度) 输出                                           */
/* 参�?OV7670/OV7675 Application Notes 中的 QQVGA 配置                     */
/*                                                                           */
/* 格式：{ 寄存器地址, 写入�? 0xFF=结束 }                                    */
/* ======================================================================== */
typedef struct {
    uint8_t reg;
    uint8_t val;
} OV7675_InitReg_t;

static const OV7675_InitReg_t s_init_regs[] = {
    /* ---- 软件复位 ---- */
    { REG_COM7,     COM7_RESET },                       /* 0x12, 0x80 �?软复�?*/

    /* ---- 等待稳定（需 10ms+�?---- */

    /* ---- 时钟配置 ---- */
    { REG_CLKRC,    0x00 },                             /* 0x11, 0x00 �?外部时钟不分�?*/

    /* ---- 分辨�? QQVGA (160x120) ---- */
    { REG_COM3,     COM3_SWAP },                        /* 0x0C, 0x10 �?交换字节�?*/
    { REG_COM7,     COM7_FMT_YUV | COM7_RES_QQVGA },   /* 0x12, 0x0E �?YUV + QQVGA */
    { REG_COM14,    0x1A },                             /* 0x3E, 0x1A �?QQVGA 模式 */

    /* ---- 输出格式: Y (灰度) ---- */
    { REG_TSLB,     0x04 },                             /* 0x3A �?Y 为最后一字节 */
    { REG_COM15,    0xC0 },                             /* 0x27 �?RGB 输出�?YUV 模式�?Y 可用 */

    /* ---- 窗口裁剪: QQVGA 160x120 ---- */
    { REG_HSTART,   0x16 },
    { REG_HSTOP,    0x04 },
    { REG_VSTART,   0x02 },
    { REG_VSTOP,    0x7A },
    { REG_PSHFT,    0x0B },
    { REG_MVFP,     0x00 },                             /* 0x1E �?无镜�?翻转 */
    { REG_HREF,     0x00 },                             /* 0x32 �?HREF 默认 */

    /* ---- 图像质量 ---- */
    { REG_COM8,     COM8_FAST_AEC | COM8_AEC | COM8_AGC | COM8_AWB },
    { REG_GAIN,     0x00 },                             /* AGC 初始增益 */
    { REG_AECH,     0x40 },                             /* 曝光值初�?*/
    { REG_COM9,     0x60 },                             /* 增益最大�?*/
    { REG_COM5,     0x00 },                             /* 系统控制 */
    { REG_COM6,     0x00 },
    { REG_COM10,    0x00 },

    /* ---- 边缘增强/色彩 ---- */
    { REG_EDGE,     0x00 },                             /* 关闭边缘增强 (灰度模式) */
    { REG_COM16,    0x00 },
    { REG_COM17,    0x00 },                             /* 关闭 DSP 色彩 */

    /* ---- 帧率控制 ---- */
    { REG_CLKRC,    0x00 },                             /* 最终时钟配�?*/

    /* ---- 结束 ---- */
    { 0xFF,         0xFF },
};

/* ======================================================================== */
/* GPIO 初始�?�?使用 gpio.h 已有�?API 封装                                */
/* ======================================================================== */
static void gpio_init(void)
{
    /* SCCB 总线: SCL/SDA = 开漏输出（初始高） */
    GPIO_WritePin(CAM_SCL_PORT, CAM_SCL_PIN, 1);
    GPIO_Init(CAM_SCL_PORT, CAM_SCL_PIN, GPIO_MODE_OUTPUT_OD);
    GPIO_WritePin(CAM_SDA_PORT, CAM_SDA_PIN, 1);
    GPIO_Init(CAM_SDA_PORT, CAM_SDA_PIN, GPIO_MODE_OUTPUT_OD);

    /* VSYNC, HREF, PCLK: 输入 */
    GPIO_Init(CAM_VSYNC_PORT, CAM_VSYNC_PIN, GPIO_MODE_INPUT);
    GPIO_Init(CAM_HREF_PORT, CAM_HREF_PIN, GPIO_MODE_INPUT);
    GPIO_Init(CAM_PCLK_PORT, CAM_PCLK_PIN, GPIO_MODE_INPUT);

    /* D[7:0]: 输入 */
    GPIO_Init(CAM_D0_PORT, CAM_D0_PIN, GPIO_MODE_INPUT);
    GPIO_Init(CAM_D1_PORT, CAM_D1_PIN, GPIO_MODE_INPUT);
    GPIO_Init(CAM_D2_PORT, CAM_D2_PIN, GPIO_MODE_INPUT);
    GPIO_Init(CAM_D3_PORT, CAM_D3_PIN, GPIO_MODE_INPUT);
    GPIO_Init(CAM_D4_PORT, CAM_D4_PIN, GPIO_MODE_INPUT);
    GPIO_Init(CAM_D5_PORT, CAM_D5_PIN, GPIO_MODE_INPUT);
    GPIO_Init(CAM_D6_PORT, CAM_D6_PIN, GPIO_MODE_INPUT);
    GPIO_Init(CAM_D7_PORT, CAM_D7_PIN, GPIO_MODE_INPUT);

    /* RESET: 推挽输出（初始高�?*/
    GPIO_WritePin(CAM_RST_PORT, CAM_RST_PIN, 1);
    GPIO_Init(CAM_RST_PORT, CAM_RST_PIN, GPIO_MODE_OUTPUT_PP);

    /* PWDN: 推挽输出（初始低 = 工作模式�?*/
    GPIO_WritePin(CAM_PWDN_PORT, CAM_PWDN_PIN, 0);
    GPIO_Init(CAM_PWDN_PORT, CAM_PWDN_PIN, GPIO_MODE_OUTPUT_PP);
}

/* ======================================================================== */
/* XCLK 初始�?�?TCPWM0 CH0 Line0 输出 ~12MHz 方波                        */
/*                                                                           */
/* 绑定：P11_0 = TCPWM0_CH0_LINE0 (来自官方原理�?J14-3)                     */
/*                                                                           */
/* TCPWM 时钟 = 外设时钟 (FLL �?100MHz)                                     */
/* period = 100MHz / 12MHz = 8.33 �?取整 8                                  */
/* compare = 4 (50% 占空�?                                                  */
/* 实际输出 = 100MHz / 8 = 12.5MHz OV7675 可接受范�?(10~20MHz)             */
/*                                                                           */
/* 注意：如需精确 12MHz，可调整 FLL 输出�?96MHz 或使�?PLL 分频            */
/* ======================================================================== */
static void xclk_init(void)
{
    #ifndef CY_GPIO_PCS_6
    #define CY_GPIO_PCS_6 6UL
    #endif
    cy_stc_tcpwm_pwm_config_t pwmConfig = {
        .pwmMode           = CY_TCPWM_PWM_MODE_PWM,
        .clockPrescaler    = 0UL,
        .pwmAlignment      = 0UL,
        .deadTimeClocks    = 0UL,
        .runMode           = CY_TCPWM_PWM_CONTINUOUS,
        .period0           = 7,
        .period1           = 0,
        .enablePeriodSwap  = false,
        .compare0          = 4,
        .compare1          = 0,
        .enableCompareSwap = false,
        .interruptSources  = 0,
        .invertPWMOut      = 0,
        .invertPWMOutN     = 0,
        .killMode          = 0,
        .swapInputMode     = 0,
        .swapInput         = 0,
        .reloadInputMode   = 0,
        .reloadInput       = 0,
        .startInputMode    = 0,
        .startInput        = 0,
        .killInputMode     = 0,
        .killInput         = 0,
        .countInputMode    = 0,
        .countInput        = 0,
        .swapOverflowUnderflow = false,
    };
    Cy_TCPWM_PWM_Init(TCPWM0, 0, &pwmConfig);
    Cy_TCPWM_PWM_Enable(TCPWM0, 0);
    Cy_TCPWM_TriggerStart_Single(TCPWM0, 0);
    Cy_GPIO_SetHSIOM(CAM_XCLK_PORT, CAM_XCLK_PIN, CY_GPIO_PCS_6);
}

/* ======================================================================== */
/* 初始�?                                                                    */
/* ======================================================================== */
int OV7675_Init(void)
{
    Logger_Print(LOG_INFO, "OV7675 Init start...");

    /* ---- 1. GPIO 引脚配置 ---- */
    gpio_init();

    /* ---- 2. 硬件复位 ---- */
    Cy_GPIO_Write(CAM_RST_PORT, CAM_RST_PIN, 0);   /* 拉低复位 (RST_N低有�? */
    volatile uint32_t d = 400000;                       /* ~100ms @ 160MHz */
    while (d--) { __asm__("nop"); }
    Cy_GPIO_Write(CAM_RST_PORT, CAM_RST_PIN, 1);   /* 释放复位 */
    d = 400000;
    while (d--) { __asm__("nop"); }

    /* ---- 3. 验证 SCCB 通信 ---- */
    uint16_t mid, pid;
    if (OV7675_ReadID(&mid, &pid)) {
        Logger_Print(LOG_ERROR, "OV7675 SCCB communication failed!");
        return -1;
    }

    /* OV7675 PID = 0x7673, OV7670 PID = 0x7670 */
    Logger_Print(LOG_INFO, "OV7675 ID: MID=0x%04X, PID=0x%04X", mid, pid);

    if ((pid >> 8) != 0x76) {   /* 高字节应�?0x76 */
        Logger_Print(LOG_ERROR, "OV7675 PID mismatch: 0x%04X", pid);
        return -2;
    }

    /* ---- 4. 写入寄存器配�?---- */
    (void)0; /* reg_before �������Ƴ� */
    for (int i = 0; s_init_regs[i].reg != 0xFF; i++) {
        uint8_t reg = s_init_regs[i].reg;
        uint8_t val = s_init_regs[i].val;

        /* 如果遇到软复�?0x80)，等待后跳过后续写（复位后寄存器全恢复默认） */
        if (reg == REG_COM7 && val == COM7_RESET) {
            OV7675_WriteReg(reg, val);
            d = 200000;     /* ~50ms */
            while (d--) { __asm__("nop"); }
            /* ��λ��Ĵ���ȫ�ָ�Ĭ�� */
            continue;
        }

        if (OV7675_WriteReg(reg, val)) {
            Logger_Print(LOG_WARN, "OV7675 reg 0x%02X write fail", reg);
        }
    }

    /* ---- 5. 启动 XCLK ---- */
    xclk_init();

    /* ---- 6. 等待第一帧稳�?---- */
    d = 800000;     /* ~200ms */
    while (d--) { __asm__("nop"); }

    s_initialized = 1;
    Logger_Print(LOG_INFO, "OV7675 Init OK (QQVGA 160x120 Y-channel)");
    return 0;
}
