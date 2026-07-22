/**
 * @file ov7675.h
 * @brief OV7675 摄像头驱动 API — QQVGA (160x120) 灰度模式
 *
 * 引脚映射基于官方原理图 630-60751-01 Page12 IO Expansion J14。
 * 使用前请核对 J14 接线与以下宏定义一致。
 *
 * @note PCLK(J14 Pin13) 经过 3.3V 电平转换后接入 P11_1，只能做输入，
 *       驱动模式必须配置为 CY_GPIO_DM_HIGHZ（已正确配置）。
 */

#ifndef OV7675_H
#define OV7675_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/* OV7675 输出格式                                                           */
/* ======================================================================== */
#define OV7675_WIDTH            160     /* QQVGA 宽度 */
#define OV7675_HEIGHT           120     /* QQVGA 高度 */

/* ======================================================================== */
/* ⚠️ 引脚映射 — 来自官方原理图 J14 DVP                              */
/*    原理图 630-60751-01 Page12 IO Expansion                              */
/* ======================================================================== */

/* ---- 时钟 ---- */
#define CAM_XCLK_PORT           GPIO_PRT11
#define CAM_XCLK_PIN            0U      /* J14-3, MCU 输出 ~12MHz 方波给摄像头 */

/* ---- SCCB (I2C) 配置总线 ---- */
#define CAM_SDA_PORT            GPIO_PRT12
#define CAM_SDA_PIN             0U      /* J14-4 */
#define CAM_SCL_PORT            GPIO_PRT12
#define CAM_SCL_PIN             1U      /* J14-5 */

/* ---- 控制信号 ---- */
#define CAM_RST_PORT            GPIO_PRT11
#define CAM_RST_PIN             2U      /* J14-6, 复位低有效 */
#define CAM_PWDN_PORT           GPIO_PRT11
#define CAM_PWDN_PIN            3U      /* J14-7, 休眠掉电(高=休眠) */

/* ---- 同步信号 ---- */
#define CAM_VSYNC_PORT          GPIO_PRT10
#define CAM_VSYNC_PIN           0U      /* J14-8, 帧同步 */
#define CAM_HREF_PORT           GPIO_PRT10
#define CAM_HREF_PIN            1U      /* J14-9, 行同步 */

/* ---- 像素时钟 ---- */
#define CAM_PCLK_PORT           GPIO_PRT11
#define CAM_PCLK_PIN            1U      /* J14-13, 经电平转换后输入, 只能 HIGHZ */

/* ---- 8 位数据总线 ---- */
/* D0~D5: P10[2]~P10[7] */
#define CAM_D0_PORT             GPIO_PRT10
#define CAM_D0_PIN              2U      /* J14-11 */
#define CAM_D1_PORT             GPIO_PRT10
#define CAM_D1_PIN              3U      /* J14-12 */
#define CAM_D2_PORT             GPIO_PRT10
#define CAM_D2_PIN              4U      /* J14-14 */
#define CAM_D3_PORT             GPIO_PRT10
#define CAM_D3_PIN              5U      /* J14-15 */
#define CAM_D4_PORT             GPIO_PRT10
#define CAM_D4_PIN              6U      /* J14-16 */
#define CAM_D5_PORT             GPIO_PRT10
#define CAM_D5_PIN              7U      /* J14-17 */

/* D6~D7: P13[0]、P13[1] (！独立端口，务必确认接线) */
#define CAM_D6_PORT             GPIO_PRT13
#define CAM_D6_PIN              0U      /* J14-18 */
#define CAM_D7_PORT             GPIO_PRT13
#define CAM_D7_PIN              1U      /* J14-19 */

/* ======================================================================== */
/* SCCB 时序参数                                                             */
/* SCCB 最大速率 ~400kHz (Fast Mode)                                        */
/* ======================================================================== */
#define SCCB_DELAY_US           2       /* 半周期 ~2us @ 250kHz */
#define SCCB_TIMEOUT_MS         100     /* 总线超时 */

/* ======================================================================== */
/* 帧采集超时 (ms)                                                           */
/* ======================================================================== */
#define CAM_CAPTURE_TIMEOUT_MS  200     /* 最坏情况等待一帧 */
#define CAM_LINE_TIMEOUT_US     5000    /* 一行超时 */
#define CAM_PIXEL_TIMEOUT_US    100     /* 一个像素超时 */

/* ======================================================================== */
/* 驱动 API                                                                  */
/* ======================================================================== */

/**
 * @brief 初始化 OV7675 摄像头
 * @return 0=成功, -1=硬件错误, -2=ID 验证失败
 */
int OV7675_Init(void);

/**
 * @brief 采集一帧 160x120 灰度图像
 * @param buf  输出缓冲区 (至少 OV7675_WIDTH * OV7675_HEIGHT 字节)
 * @return 0=成功, -1=VSYNC 超时, -2=行超时, -3=像素超时, -4=未初始化
 */
int OV7675_CaptureFrame(uint8_t *buf);

/**
 * @brief 读取摄像头产品 ID（用于验证连接与初始化）
 */
int OV7675_ReadID(uint16_t *mid, uint16_t *pid);

/**
 * @brief 写入单个 SCCB 寄存器
 */
int OV7675_WriteReg(uint8_t reg, uint8_t val);

/**
 * @brief 读取单个 SCCB 寄存器
 */
int OV7675_ReadReg(uint8_t reg, uint8_t *val);

#ifdef __cplusplus
}
#endif

#endif /* OV7675_H */
