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
#include <stdio.h>
#include <math.h>

/*******************************************************************************
* Macros
*******************************************************************************/
#define NUM_CLASSES             3
#define MAX_CLASS_LEN           10
#define MAX_PREDICTIONS         5
/* Default USB camera input dimensions. */ 
#define CAMERA_WIDTH            320     
#define CAMERA_HEIGHT           240
/* Model input dimensions, hard coded for now to help on LCD graphics maintain an image buffer of model size. */
#define IMAGE_WIDTH             320     
#define IMAGE_HEIGHT            320
/* If USB webcam stream is sharding, skip some frames (inference every FRAMES_TO_SKIP frames) */
#define FRAMES_TO_SKIP          2        
#define FRAMES_TO_SKIP_LOGITECH 4

/* Object Detection Configuration. */ 
/* Scaling factor */      
#define HALF(x)                 ((x) * 0.5f)
/* Rounding factor */    
#define RND_F2I_FACTOR          0.5f     

#ifndef max
    #define max(a, b)   ((a) > (b) ? (a) : (b))
    #define min(a, b)   ((a) < (b) ? (a) : (b))
#endif

/******************************************************************************
 * Global Variables - struct
 *****************************************************************************/
/* Final output variables */ 
typedef struct {
    int32_t     count;
    int16_t     bbox_int16[MAX_PREDICTIONS * 4];
    float       conf[MAX_PREDICTIONS];
    uint8_t     class_id[MAX_PREDICTIONS];
    char        class_string[MAX_PREDICTIONS][MAX_CLASS_LEN];
} prediction_od_t;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
int8_t get_best_class(const float *cls, size_t size, float *max_cls_val);


#if defined(__cplusplus)
}
#endif /* __cplusplus */


#endif /* _PROJ_CM55_INFERENCE_TASK_H_ */
