/*****************************************************************************
* \file lcd_task.h
*
* \brief
* This is the header file of LCD display functions.
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
#ifndef _LCD_TASK_H_
#define _LCD_TASK_H_

/*******************************************************************************
 * Included Headers
 *******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#include "cyabs_rtos.h"
#include "vg_lite.h"
#include "vg_lite_platform.h"
#include "inference_task.h"

/*******************************************************************************
 * Macros
 *******************************************************************************/
/* Number of image buffers */
#define NUM_IMAGE_BUFFERS                   ( 2 )  

#define INFERENCE_TASK_NAME                 ( "CM55 Inference Task" )
#define INFERENCE_TASK_STACK_SIZE           ( 64U * 1024U )
#define INFERENCE_TASK_PRIORITY             ( configMAX_PRIORITIES - 4)
/*******************************************************************************
 * Global Variables
 *******************************************************************************/
/* Model semaphore for synchronization */
extern cy_semaphore_t model_semaphore;  
/* BGR888 integer buffer */
extern uint8_t bgr888_uint8[];           

/*******************************************************************************
 * Local Variables
 *******************************************************************************/
/* Render target buffer */
extern vg_lite_buffer_t *render_target; 
/* USB YUV frame buffers */        
extern vg_lite_buffer_t usb_yuv_frames[];      

/*******************************************************************************
 * Function Prototypes
 *******************************************************************************/
uint8_t *draw(void);
void mirror_image(vg_lite_buffer_t *buffer);
void update_box_data(vg_lite_buffer_t *render_target, prediction_od_t *prediction);
void update_box_data1(vg_lite_buffer_t *render_target, float num);
void VG_LITE_ERROR_EXIT(char *msg, vg_lite_error_t vglite_status);
void VG_switch_frame(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _LCD_TASK_H_ */
