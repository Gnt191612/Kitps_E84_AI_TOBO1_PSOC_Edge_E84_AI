/*****************************************************************************
* \file usb_camera_task.h
*
* \brief
* This is the header file of USB Webcam functions.
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

#ifndef _USB_CAMERA_TASK_H_
#define _USB_CAMERA_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdio.h>

/* FreeRTOS header file */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include "cyabs_rtos.h"
#include "cyabs_rtos_impl.h"

/*******************************************************************************
* Macros
*******************************************************************************/
/* YUYV 422 encoding - 2 bytes per pixel */
#define CAMERA_BUFFER_SIZE ((CAMERA_WIDTH) * (CAMERA_HEIGHT) * 2) 
#define FORMAT USBH_VIDEO_VS_FORMAT_UNCOMPRESSED
/* Configurable FPS Logitech C920e */
#define FRAME_INTERVAL_1 (1000000)
/* 30 FPS 0.3 MP cam */ 
#define FRAME_INTERVAL_2 (333332) 
/* 30 FPS 2 MP cam */
#define FRAME_INTERVAL (333333) 

#define MAX_VIDEO_INTERFACES 4

#define TASK_PRIO_USBH_MAIN         (configMAX_PRIORITIES - 2)
#define TASK_PRIO_USBH_ISR          (configMAX_PRIORITIES - 1)

#define LOGI_TECH_C920_VID  0x046D
/* Logitech C920 */
#define LOGI_TECH_C920_PID  0x08E5
/* Logitech C920e */
#define LOGI_TECH_C920e_PID 0x08B6            

/* 0.3MP HBVCAM */
#define HBV_CAM_0P3_VID  0x058F         
#define HBV_CAM_0P3_PID  0x5608

/* 2MP HBVCAM */
#define HBV_CAM_2P0_VID  0x05A3         
#define HBV_CAM_2P0_PID  0x2B01

extern bool logitech_camera_enabled;
extern bool point3mp_camera_enabled;
extern bool Camera_not_supported;

/******************************************************************************
 * Typedefs
 *****************************************************************************/
typedef struct video_buffer
{
    uint32_t num_bytes; /* Number of video data bytes in the buffer. */ 
    uint8_t buff_ready; /* Buffer ready flag */ 
} video_buffer_t;

/*******************************************************************************
* Global Variables
*******************************************************************************/
extern video_buffer_t    _image_buff[];
extern uint8_t          last_buffer;

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* _USB_CAMERA_TASK_H_ */
