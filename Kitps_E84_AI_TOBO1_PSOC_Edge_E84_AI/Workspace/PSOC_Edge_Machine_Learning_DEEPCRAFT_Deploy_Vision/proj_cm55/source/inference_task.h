/*****************************************************************************
* \file inference_task.h
*
* \brief
* This is the header file of inference task.
*
********************************************************************************
* (c) 2025-2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
 * Header Guards
 *******************************************************************************/
#ifndef _PROJ_CM55_INFERENCE_TASK_H_
#define _PROJ_CM55_INFERENCE_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

/*******************************************************************************
* Macros
*******************************************************************************/
/* Human/background classifier (code/PSE84E_Project/NPU/train_npu_model.py).
 * Class order must match CLASS_NAMES = ['background', 'human'] in training. */
#define NUM_CLASSES             2
#define CLASS_BACKGROUND        0
#define CLASS_HUMAN             1
#define MAX_CLASS_LEN           12
/* A classifier yields at most one result per frame (shown as a full-frame box) */
#define MAX_PREDICTIONS         1
/* human_conf >= threshold -> human_present */
#define HUMAN_CONF_THRESHOLD    0.5f

/* Camera frame dimensions (USB YUYV / DVP OV7675 BGR565, both 320x240). */
#define CAMERA_WIDTH            320
#define CAMERA_HEIGHT           240
/* Model input: 160x120x1 grayscale, int8, q = gray - 128 (scale 1.0, zero_point -128) */
#define MODEL_INPUT_WIDTH       160
#define MODEL_INPUT_HEIGHT      120
/* Width of the image whose coordinates are used for LCD boxes (camera frame). */
#define IMAGE_WIDTH             CAMERA_WIDTH
/* If USB webcam stream is sharding, skip some frames (inference every FRAMES_TO_SKIP frames) */
#define FRAMES_TO_SKIP          2
#define FRAMES_TO_SKIP_LOGITECH 4

#ifndef max
    #define max(a, b)   ((a) > (b) ? (a) : (b))
    #define min(a, b)   ((a) < (b) ? (a) : (b))
#endif

/******************************************************************************
 * Global Variables - struct
 *****************************************************************************/
/* LCD overlay data (consumed by lcd_task.c update_box_data) */
typedef struct {
    int32_t     count;
    int16_t     bbox_int16[MAX_PREDICTIONS * 4];
    float       conf[MAX_PREDICTIONS];
    uint8_t     class_id[MAX_PREDICTIONS];
    char        class_string[MAX_PREDICTIONS][MAX_CLASS_LEN];
} prediction_od_t;

/* Per-frame inference result: the export point for downstream links.
 * h7_link.c answers H7 command frames (22 B) with result frames (21 B) built from it. */
typedef struct {
    uint32_t    frame_id;           /* incremented per inference */
    uint32_t    timestamp_ms;       /* ifx_time_get_ms_f() when the result became valid */
    float       inference_ms;       /* mtb_ml_model_run() duration */
    float       preprocess_ms;      /* BGR565 320x240 -> int8 gray 160x120 */
    float       conf_background;    /* softmax output, 0..1 */
    float       conf_human;         /* softmax output, 0..1 */
    uint8_t     class_id;           /* CLASS_BACKGROUND / CLASS_HUMAN (argmax) */
    bool        human_present;      /* conf_human >= HUMAN_CONF_THRESHOLD */
} human_detect_result_t;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
/* Copy the latest result; returns false until the first inference finished. */
bool inference_get_latest_result(human_detect_result_t *out);
/* Called from the inference task after every inference. Weak default only logs
 * class changes; override it (e.g. in the future H7 UART module) to forward results.
 * Runs in the inference task context: keep it short and non-blocking. */
void inference_result_hook(const human_detect_result_t *result);


#if defined(__cplusplus)
}
#endif /* __cplusplus */


#endif /* _PROJ_CM55_INFERENCE_TASK_H_ */
