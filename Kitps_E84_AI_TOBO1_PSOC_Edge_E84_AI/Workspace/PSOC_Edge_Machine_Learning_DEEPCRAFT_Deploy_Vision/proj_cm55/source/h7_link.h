/*****************************************************************************
* \file h7_link.h
*
* \brief
* UART link to the STM32H7 master (port of code/PSE84E_Project/Communication).
*
* Frame:   AA 55 [len = 1 + payload_len] [type] [payload ...] [XOR(type..payload)]
* H7->84E: type 0x01, 22 B CommandPacket (little-endian, packed)
*            0 cmd_type u8 | 1 target_id u8 | 2 center_angle f32 | 6 angle_width f32
*           10 min_dist_cm f32 | 14 max_dist_cm f32 | 18 timestamp_ms u32
* 84E->H7: type 0x02, 21 B ResultPacket (little-endian, packed)
*            0 target_id u8 | 1 confidence f32 | 5 x_cm f32 | 9 y_cm f32
*           13 dist_cm f32 | 17 angle_deg f32
* Must stay byte-compatible with code/STM32H7/Communication/protocol.c
* (Protocol_Send84ECommand / Protocol_ProcessIncoming).
*
* Hardware: SCB1 UART, P9_2 = TX, P9_3 = RX (header J5, "alternate serial
* interface"), 115200 8N1. J5 is in the 1.8 V I/O domain: a bidirectional
* 1.8 V <-> 3.3 V level shifter is REQUIRED between J5 and H7 USART1 (PB6/PB7).
*
*******************************************************************************/
#ifndef _PROJ_CM55_H7_LINK_H_
#define _PROJ_CM55_H7_LINK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*******************************************************************************
* Macros
*******************************************************************************/
#define H7_LINK_TASK_NAME           ("CM55 H7 Link Task")
#define H7_LINK_TASK_STACK_SIZE     (4U * 1024U)
/* Above the inference task (configMAX_PRIORITIES - 4), which busy-waits for
 * DVP frames; this task sleeps between RX polls so it does not starve others. */
#define H7_LINK_TASK_PRIORITY       (configMAX_PRIORITIES - 3)

#define H7_LINK_BAUD_RATE           (115200UL)

/* 1: hex-dump every RX/TX frame on the debug UART. Compile-time, off by default. */
#ifndef H7_LINK_DEBUG_DUMP
#define H7_LINK_DEBUG_DUMP          (0)
#endif

#define FRAME_HEADER1               (0xAAU)
#define FRAME_HEADER2               (0x55U)
#define FRAME_MAX_PAYLOAD           (64U)
#define FRAME_TYPE_COMMAND          (0x01U)
#define FRAME_TYPE_RESULT           (0x02U)

#define H7_CMD_PAYLOAD_LEN          (22U)
#define H7_RESULT_PAYLOAD_LEN       (21U)

/* Command types sent by H7 (cmd_84e.c) */
#define H7_CMD_RECOGNIZE            (0U)
#define H7_CMD_ASSIST_SCAN          (1U)

/*******************************************************************************
* Types
*******************************************************************************/
typedef struct {
    uint8_t  cmd_type;      /* H7_CMD_RECOGNIZE / H7_CMD_ASSIST_SCAN */
    uint8_t  target_id;     /* 1..3, assigned by H7 */
    float    center_angle;  /* deg */
    float    angle_width;   /* deg */
    float    min_dist_cm;
    float    max_dist_cm;
    uint32_t timestamp_ms;  /* H7 HAL_GetTick() */
} h7_command_t;

typedef struct {
    uint8_t  target_id;
    float    confidence;    /* 0..1; H7 treats <= 0.3 as "not recognized" */
    float    x_cm;
    float    y_cm;
    float    dist_cm;
    float    angle_deg;
} h7_result_t;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void cm55_h7_link_task(void *arg);

/* Frame helpers (exposed for reuse/tests). Return 0 on success. */
int h7_frame_pack(uint8_t *out, uint16_t out_size, uint16_t *out_len, uint8_t type,
                  const uint8_t *payload, uint16_t payload_len);
void h7_command_decode(const uint8_t payload[H7_CMD_PAYLOAD_LEN], h7_command_t *cmd);
void h7_result_encode(const h7_result_t *res, uint8_t payload[H7_RESULT_PAYLOAD_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* _PROJ_CM55_H7_LINK_H_ */
