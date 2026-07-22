/**
 * @file    can.c
 * @brief   CAN 通信实现（使用 HAL FDCAN 或 bxCAN，本例以 FDCAN1 为例）
 *
 * ⚠️ 当前编译未启用 — HAL_FDCAN_MODULE_ENABLED 未定义。
 *    如需启用：
 *      1. stm32h7xx_hal_conf.h 中取消注释 #define HAL_FDCAN_MODULE_ENABLED
 *      2. CubeMX 中配置 FDCAN1 时钟/引脚/波特率
 *      3. 重新生成 HAL 初始化代码
 *      4. 去掉下面的 #if 0 块
 */
#if 0

#include "can.h"
#include "main.h"           // 提供 hfdcan1 等句柄
#include "logger.h"
#include <string.h>

/* ── 外部 CAN 句柄（CubeMX 生成） ── */
extern FDCAN_HandleTypeDef hfdcan1;

/* ── 接收回调 ── */
static void (*can_rx_callback)(uint32_t id, uint8_t *data, uint8_t len) = NULL;

/* ── 简单发送缓冲区（非必须） ── */
static FDCAN_TxHeaderTypeDef   TxHeader;
static FDCAN_RxHeaderTypeDef   RxHeader;
static uint8_t                 TxData[8];
static uint8_t                 RxData[8];

/*----------------------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------------------*/
void CAN_Init(void)
{
    /* 配置滤波器：接收所有标准帧和扩展帧 */
    FDCAN_FilterTypeDef sFilterConfig;
    sFilterConfig.IdType       = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex  = 0;
    sFilterConfig.FilterType   = FDCAN_FILTER_MASK;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfig.FilterID1    = 0x000;   // 不过滤，全收
    sFilterConfig.FilterID2    = 0x000;
    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK) {
        Logger_Print(LOG_ERROR, "CAN filter config failed");
    }

    /* 启动 FDCAN */
    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        Logger_Print(LOG_ERROR, "CAN start failed");
    }

    /* 使能接收中断（FIFO0 有新消息时触发） */
    if (HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
        Logger_Print(LOG_ERROR, "CAN RX interrupt enable failed");
    }

    Logger_Print(LOG_INFO, "CAN Initialized.");
}

/*----------------------------------------------------------------------------
 * 发送 CAN 消息（标准帧或扩展帧）
 *----------------------------------------------------------------------------*/
void CAN_SendMessage(uint32_t id, uint8_t *data, uint8_t len)
{
    if (len > 8) len = 8;

    TxHeader.Identifier          = id;
    TxHeader.IdType              = (id <= 0x7FF) ? FDCAN_STANDARD_ID : FDCAN_EXTENDED_ID;
    TxHeader.TxFrameType         = FDCAN_DATA_FRAME;
    TxHeader.DataLength          = len;
    TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeader.BitRateSwitch       = FDCAN_BRS_OFF;
    TxHeader.FDFormat            = FDCAN_CLASSIC_CAN;
    TxHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    TxHeader.MessageMarker       = 0;

    memcpy(TxData, data, len);
    HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader, TxData);
}

/*----------------------------------------------------------------------------
 * 注册接收回调
 *----------------------------------------------------------------------------*/
void CAN_RegisterRxCallback(void (*cb)(uint32_t id, uint8_t *data, uint8_t len))
{
    can_rx_callback = cb;
}

/*----------------------------------------------------------------------------
 * FDCAN 接收中断回调（由 HAL 库自动调用）
 *----------------------------------------------------------------------------*/
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET)
    {
        if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
        {
            if (can_rx_callback)
            {
                can_rx_callback(RxHeader.Identifier, RxData, RxHeader.DataLength);
            }
        }
    }
}

#endif /* #if 0 — CAN 模块当前未启用 */
