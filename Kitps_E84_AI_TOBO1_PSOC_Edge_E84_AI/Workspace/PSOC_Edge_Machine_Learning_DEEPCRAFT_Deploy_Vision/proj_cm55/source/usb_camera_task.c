/******************************************************************************
* File Name:   usb_camera_task.c
*
* Description: This file implements the USB Webcam functions.
*
* Related Document: See README.md
*
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

/*****************************************************************************
 * Header Files
 *****************************************************************************/
#include <inttypes.h>
#include "cybsp.h"
#include "cy_pdl.h"
#include "cyabs_rtos.h"
/* FreeRTOS header file */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "cyabs_rtos.h"
#include "cyabs_rtos_impl.h"
#include "USBH.h"
#include "USBH_Util.h"
#include "USBH_VIDEO.h"
#include <stdint.h>
#include <stdio.h>
#include "inference_task.h"
#include "lcd_task.h"
#include "usb_camera_task.h"
#include "ifx_time_utils.h"

/*******************************************************************************
* Macros
*******************************************************************************/

/*******************************************************************************
 * Global Variables
 *******************************************************************************/
/* Handle for USB host main task */
static cy_thread_t usbh_main_task_handle;  
/* Handle for USB host ISR task */         
static cy_thread_t usbh_isr_task_handle;            
/* Mailbox for video-related messages */
static cy_queue_t _video_mail_box; 
/* Mailbox for device state messages */                  
static cy_queue_t _device_state_mail_box;   
/* Mailbox for new frame notifications */           
cy_queue_t _new_frame_mail_box;                        
/* Array of image buffers for video frames */
video_buffer_t _image_buff[NUM_IMAGE_BUFFERS];         
/* Flag indicating device connection status */
uint8_t _device_connected; 
/* Buffer for general-purpose character data */                         
static char _ac[128];  
/* Index of the last used buffer */                             
uint8_t last_buffer;     
/* Flag for Logitech camera enable status */                            
bool logitech_camera_enabled = 0;  
/* Flag for 3MP camera enable status */                  
bool point3mp_camera_enabled = 0;   
/* Flag for 2MP camera enable status */                 
bool twomp_camera_enabled = 0;   
/* Flag for unsupported camera status */                   
bool Camera_not_supported = 0; 
/* Device disconnect event status */                     
unsigned char disconnect_event =0;                               

/* frame monitoring variables */ 
/* Timestamp of the last frame received */
float last_frame_time = 0;   
/* Timestamp of the last frame check */                       
float last_check_time = 0;    
/* Counter for stream errors */                      
static int _stream_err_cnt;    
/* Timeout for stalled frames (10 seconds) */                       
float frame_timeout_ms = 10000;                     
/* USB host notification hook */ 
static USBH_NOTIFICATION_HOOK hook;                  
/* External semaphore for USB operations */
extern cy_semaphore_t usb_semaphore;  
/* External display buffer array */              
extern vg_lite_buffer_t display_buffer[2];     
/* External YUV frame buffers */      
extern vg_lite_buffer_t usb_yuv_frames[NUM_IMAGE_BUFFERS]; 



/*******************************************************************************
* Function Name: _add_remove_device_cb
*******************************************************************************
 * Description:
 *   Callback function invoked when a USB device is added or removed. Executes in
 *   the context of the USBH_Task and handles device connection/disconnection events
 *   by updating buffers, logging events, and signaling tasks. Non-blocking.
 *
 * Input Arguments:
 *   void *p_context - Context pointer (not used)
 *   U8 dev_index - Index of the device
 *   USBH_DEVICE_EVENT event - event type (add or remove)
 *
 * Return Value:
 *   None
 *
 *******************************************************************************/
static void _add_remove_device_cb(void *p_context, U8 dev_index, USBH_DEVICE_EVENT event)
{
    (void)p_context;

    switch (event)
    {
        case USBH_DEVICE_EVENT_ADD:
            _device_connected = 1;
            USBH_Logf_Application("**** VIDEO Device added [%d]\n", dev_index);
            for (int i = 0; i < NUM_IMAGE_BUFFERS; i++)
            {
                _image_buff[i].buff_ready = 0;
                _image_buff[i].num_bytes = 0;
            }
            last_buffer = 0;
            __DMB();
            (void)cy_rtos_queue_put(&_video_mail_box, &dev_index, 0);
            break;
        case USBH_DEVICE_EVENT_REMOVE:
            _device_connected = 0;
            USBH_Logf_Application("**** VIDEO Device removed [%d]\n", dev_index);

            /* Reset buffer states */ 
            for (int i = 0; i < NUM_IMAGE_BUFFERS; i++)
            {
                _image_buff[i].buff_ready = 0;
                _image_buff[i].num_bytes = 0;
            }
            __DMB();
            /* Signal main task about disconnection */ 
            disconnect_event = 0xff;
            cy_rtos_queue_put(&_device_state_mail_box, &disconnect_event, 0);
            cy_rtos_semaphore_set(&model_semaphore);
            _stream_err_cnt = 0;
            break;
        default:/* Should never happen */
            ; 
    }
}

CY_SECTION_ITCM_BEGIN
/*******************************************************************************
* Function Name: _on_data_cb
*******************************************************************************
 * Description:
 *   Callback function invoked when video data is received from a USB video device.
 *   Manages frame buffering, error handling, and stream state for video data
 *   processing. Updates buffer states, signals semaphores, and handles frame
 *   completion or errors.
 *
 * Input Arguments:
 *   USBH_VIDEO_DEVICE_HANDLE h_device - Handle to the video device (not used)
 *   USBH_VIDEO_STREAM_HANDLE h_stream - Handle to the video stream
 *   USBH_STATUS status - status of the data transfer
 *   const U8 *p_data - Pointer to the received data
 *   unsigned num_bytes - Number of bytes received
 *   U32 flags - flags indicating frame status
 *   void *p_user_data_context - User context pointer (not used)
 *
 * Return Value:
 *   None
 *
 *******************************************************************************/
static void _on_data_cb(USBH_VIDEO_DEVICE_HANDLE h_device, USBH_VIDEO_STREAM_HANDLE h_stream, USBH_STATUS status, const U8 *p_data, unsigned num_bytes, U32 flags, void *p_user_data_context)
{
    (void)p_data;
    (void)h_device;
    (void)p_user_data_context;

    static size_t _frame_bytes = 0;
    static uint8_t _throw_away_frame = 0;
    static uint8_t _current_Buffer = 0;
    static uint16_t _frame_count = 0;
    static uint32_t _error_count = 0;
    static volatile float _time_frame = 0;
    static volatile float _time_block = 0;
    volatile float _time_memcpy = 0;
    (void)_time_memcpy;
    (void)_time_block;

    USBH_STATUS status1;
    I8 is_stream_stopped;

    /* Check if USB is ready to transmit */
    if (status == USBH_STATUS_SUCCESS)
    {
        _stream_err_cnt = 0;
        _error_count = 0;
        volatile float _time_now = ifx_time_get_ms_f();

        bool end_of_frame = (flags & USBH_UVC_END_OF_FRAME) == USBH_UVC_END_OF_FRAME;

        /* Process every N-th frame only to reduce computational load ; 
        (_frame_count / N) FPS */
        if (!point3mp_camera_enabled)
        {
            if (!logitech_camera_enabled)
                _throw_away_frame |= (_frame_count % FRAMES_TO_SKIP);
            else
                _throw_away_frame |= (_frame_count % FRAMES_TO_SKIP_LOGITECH);
        }

        if (_frame_bytes + num_bytes > CAMERA_BUFFER_SIZE)
        {
            printf("[USB Camera] Buffer overflow: %u (%6u)\r\n", _current_Buffer, _frame_bytes + num_bytes);
            _throw_away_frame = 1;
            _frame_bytes = 0;
        }

        if (_throw_away_frame)
        {
            if (end_of_frame)
            {
                _frame_count++;
                _frame_bytes = 0;
                _time_block = _time_frame = _time_now;
                _throw_away_frame = _image_buff[_current_Buffer].buff_ready;
            }
            USBH_VIDEO_Ack(h_stream);
            return;
        }

        /* Copy data into the current buffer */ 
        memcpy((uint8_t *)usb_yuv_frames[_current_Buffer].memory + _frame_bytes, p_data, num_bytes);
        _frame_bytes += num_bytes;
        _time_memcpy = ifx_time_get_ms_f();

        /* Switch buffers when a buffer is full or when we register an end-of-frame */ 
        if (end_of_frame)
        {
            _frame_count++;

            if (_frame_bytes == CAMERA_BUFFER_SIZE)
            {
                /* Processing of the previous frame is DONE, no ready buffer, and wait for a new ready frame */
                last_buffer = _current_Buffer;
                _image_buff[_current_Buffer].buff_ready = 1;
                _image_buff[_current_Buffer].num_bytes = _frame_bytes;

                __DMB();

                /* Switch to next buffer */ 
                _current_Buffer = (_current_Buffer + 1) % NUM_IMAGE_BUFFERS;
                /* If there is only ONE buffer, throw away next frames until processing is completed */
                _throw_away_frame = _image_buff[_current_Buffer].buff_ready;
                if (!logitech_camera_enabled)
                {
                    cy_rslt_t result = cy_rtos_semaphore_get(&usb_semaphore, 0xFFFFFFFF);
                    if (CY_RSLT_SUCCESS != result)
                    {
                        printf("[USB Camera] USB Semaphore get failed\r\n");
                    }
                }
                else
                {
                    cy_rtos_semaphore_set(&usb_semaphore);
                }
            }
            /* else  short frame */

            _frame_bytes = 0;
            _time_frame = _time_now;
        }

        USBH_VIDEO_Ack(h_stream);
        _time_block = _time_now;
    }
    else
    {
        _error_count++;
        if (_error_count % 100 == 1)
        {
            printf("[USB] Transfer error %s (count: %" PRIu32 ")\n", USBH_GetStatusStr(status), _error_count);
        }

        _stream_err_cnt++;
        if (_stream_err_cnt >= 10)
        {
            /* Indicate failure */
            status1 = USBH_STATUS_DEVICE_ERROR; 
        }
        else
        {
            if (status != USBH_STATUS_DEVICE_REMOVED)
            {
                status1 = USBH_VIDEO_GetStreamState(h_stream, &is_stream_stopped);
                if (status1 == USBH_STATUS_SUCCESS)
                {
                    if (is_stream_stopped == 1)
                    {
                        status1 = USBH_VIDEO_RestartStream(h_stream);
                        if (status1 == USBH_STATUS_SUCCESS)
                        {
                            USBH_Logf_Application("_on_data_cb: Restarted video stream after packet error");
                        }
                        else
                        {
                            USBH_Logf_Application("_on_data_cb: USBH_VIDEO_Restartstream failed %s", USBH_GetStatusStr(status));
                        }
                    }
                    else
                    {
                        USBH_Warnf_Application("_on_data_cb: _stream_err_cnt %d, but stream is running", _stream_err_cnt);
                    }
                }
                else
                {
                    USBH_Logf_Application("_on_data_cb: USBH_VIDEO_GetstreamState failed %s", USBH_GetStatusStr(status));
                }
            }
        }

        /* If the consecutive error count reaches max, the device was disconnected, or the stream could not be restarted - remove the video device */ 
        if ((status == USBH_STATUS_DEVICE_REMOVED) || (status1 != USBH_STATUS_SUCCESS))
        {
            USBH_Logf_Application("_on_data_cb: device removed or max conseq. errors exceeded");
            /* Device disconnected or had an error */ 
            U8 mb_event = 0xff; 
            cy_rtos_queue_put(&_device_state_mail_box, &mb_event, 0);
            _stream_err_cnt = 0;
        }

        /* Reset frame state on error but don't change buffer states */ 
        _frame_bytes = 0;
        _throw_away_frame = 0;
        USBH_VIDEO_Ack(h_stream);
        return;
    }
}
CY_SECTION_ITCM_END

/*******************************************************************************
* Function Name: _get_frame_intervals
*******************************************************************************
 * Description:
 *   Retrieves and formats frame interval information for a video frame into a
 *   string buffer for logging or display purposes.
 *
 * Input Arguments:
 *   USBH_VIDEO_FRAME_INFO *p_frame - Pointer to the frame information structure
 *
 * Return Value:
 *   None
 *
 *******************************************************************************/
static void _get_frame_intervals(USBH_VIDEO_FRAME_INFO *p_frame)
{
    char ac[32];
    unsigned i;

    USBH_MEMSET(_ac, 0, sizeof(_ac));
    USBH_MEMSET(ac, 0, sizeof(ac));
    for (i = 0; i < p_frame->bFrameIntervalType; i++)
    {
        if (i == 0)
        {
            SEGGER_snprintf(ac, sizeof(ac), "%lu", p_frame->u.dwFrameInterval[i]);
        }
        else
        {
            SEGGER_snprintf(ac, sizeof(ac), ", %lu", p_frame->u.dwFrameInterval[i]);
        }
        strncat(_ac, ac, sizeof(_ac) - (strlen(_ac) + 1));
    }
}

/*******************************************************************************
 * Function Name: _term_type_to_str
 *******************************************************************************
 * Description:
 *   Converts a terminal type code to a human-readable string representation.
 *
 * Input Arguments:
 *   U8 Type - Terminal type code
 *
 * Return Value:
 *   const char* - String representation of the terminal type
 *
 *******************************************************************************/
static const char* _term_type_to_str(U8 Type)
{
    switch (Type)
    {
        case USBH_VIDEO_VC_INPUT_TERMINAL:
            return "Input";
        case USBH_VIDEO_VC_OUTPUT_TERMINAL:
            return "Output";
        case USBH_VIDEO_VC_SELECTOR_UNIT:
            return "Selector";
        case USBH_VIDEO_VC_PROCESSING_UNIT:
            return "Processing";
        case USBH_VIDEO_VC_EXTENSION_UNIT:
            return "Extension";
        default:
            return "Unknown";
    }
}

/*******************************************************************************
 * Function Name: _format_type_to_str
 *******************************************************************************
 * Description:
 *   Converts a video format type code to a human-readable string representation.
 *
 * Input Arguments:
 *   U8 Type - Video format type code
 *
 * Return Value:
 *   const char* - String representation of the video format type
 *
 *******************************************************************************/
static const char* _format_type_to_str(U8 Type)
{
    switch (Type)
    {
        case USBH_VIDEO_VS_FORMAT_UNCOMPRESSED:
            return "Uncompressed";
        case USBH_VIDEO_VS_FORMAT_MJPEG:
            return "MJPEG";
        case USBH_VIDEO_VS_FORMAT_FRAME_BASED:
            return "H.264";
        default:
            return "Unknown";
    }
}

/*******************************************************************************
 * Function Name: _frame_type_to_str
 *******************************************************************************
 * Description:
 *   Converts a video frame type code to a human-readable string representation.
 *
 * Input Arguments:
 *   U8 Type - Video frame type code
 *
 * Return Value:
 *   const char* - String representation of the video frame type
 *
 *******************************************************************************/
static const char* _frame_type_to_str(U8 Type)
{
    switch (Type)
    {
        case USBH_VIDEO_VS_FRAME_UNCOMPRESSED:
            return "Uncompressed";
        case USBH_VIDEO_VS_FRAME_MJPEG:
            return "MJPEG";
        case USBH_VIDEO_VS_FRAME_FRAME_BASED:
            return "H.264";
        default:
            return "Unknown";
    }
}

/*******************************************************************************
 * Function Name: _print_input_term_info
 *******************************************************************************
 * Description:
 *   Logs information about an input terminal, specifically for camera terminals.
 *
 * Input Arguments:
 *   USBH_VIDEO_TERM_UNIT_INFO *p_term_info - Pointer to the terminal information structure
 *
 * Return Value:
 *   None
 *
 *******************************************************************************/
static void _print_input_term_info(USBH_VIDEO_TERM_UNIT_INFO *p_term_info)
{
    if (p_term_info->u.Terminal.TerminalType != USBH_VIDEO_ITT_CAMERA)
    {
        USBH_Logf_Application("  Input terminal type 0x%x is not handled...", p_term_info->u.Terminal.TerminalType);
    }
    else
    {
        USBH_Logf_Application("  Associated terminal ID %d", p_term_info->u.Terminal.u.CameraTerm.bAssocTerminal);
        USBH_Logf_Application(
                "  Objective focal length min %d",
                p_term_info->u.Terminal.u.CameraTerm.wObjectiveFocalLengthMin);
        USBH_Logf_Application(
                "  Objective focal length max %d",
                p_term_info->u.Terminal.u.CameraTerm.wObjectiveFocalLengthMax);
        USBH_Logf_Application("  Ocular focal length %d", p_term_info->u.Terminal.u.CameraTerm.wOcularFocalLength);
        USBH_Logf_Application("  Control Size %d", p_term_info->u.Terminal.u.CameraTerm.bControlSize);
    }
}

/*******************************************************************************
 * Function Name: _print_output_term_info
 *******************************************************************************
 * Description:
 *   Logs information about an output terminal, specifically for streaming terminals.
 *
 * Input Arguments:
 *   USBH_VIDEO_TERM_UNIT_INFO *p_term_info - Pointer to the terminal information structure
 *
 * Return Value:
 *   None
 *
 *******************************************************************************/
static void _print_output_term_info(USBH_VIDEO_TERM_UNIT_INFO *p_term_info)
{
    if (p_term_info->u.Terminal.TerminalType != USBH_VIDEO_TT_STREAMING)
    {
        USBH_Logf_Application("  Ouput terminal type 0x%x is not handled...", p_term_info->u.Terminal.TerminalType);
    }
    else
    {
        USBH_Logf_Application("  Associated terminal ID %d", p_term_info->u.Terminal.u.OutputTerm.bAssocTerminal);
        USBH_Logf_Application("  Source ID %d", p_term_info->u.Terminal.u.OutputTerm.bSourceID);
    }
}

/*******************************************************************************
 * Function Name: _print_selector_unit_info
 *******************************************************************************
 * Description:
 *   Logs information about a selector unit, including its input pins.
 *
 * Input Arguments:
 *   USBH_VIDEO_TERM_UNIT_INFO *p_term_info - Pointer to the terminal information structure
 *
 * Return Value:
 *   None
 *
 *******************************************************************************/
static void _print_selector_unit_info(USBH_VIDEO_TERM_UNIT_INFO *p_term_info)
{
    unsigned i;

    USBH_Logf_Application("  Unit ID %d", p_term_info->bTermUnitID);
    USBH_Logf_Application("  Number of pins %d", p_term_info->u.SelectUnit.bNrInPins);
    for (i = 0; i < p_term_info->u.SelectUnit.bNrInPins; i++)
    {
        USBH_Logf_Application("  Input pin: %d", p_term_info->u.SelectUnit.baSourceID[i]);
    }
}

/*******************************************************************************
 * Function Name: _print_processing_unit_info
 *******************************************************************************
 * Description:
 *   Logs information about a processing unit, including its source and control details.
 *
 * Input Arguments:
 *   USBH_VIDEO_TERM_UNIT_INFO *p_term_info - Pointer to the terminal information structure
 *
 * Return Value:
 *   None
 *
 *******************************************************************************/
static void _print_processing_unit_info(USBH_VIDEO_TERM_UNIT_INFO *p_term_info)
{
    USBH_Logf_Application("  Unit ID %d", p_term_info->bTermUnitID);
    USBH_Logf_Application("  Source ID %d", p_term_info->u.ProcessUnit.bSourceID);
    USBH_Logf_Application("  Max multiplier %d", p_term_info->u.ProcessUnit.wMaxMultiplier);
    USBH_Logf_Application("  Control size %d", p_term_info->u.ProcessUnit.bControlSize);
}

/*******************************************************************************
 * Function Name: _print_extension_unit_info
 *
 * Description:
 *   Logs information about an extension unit, including its GUID, pins, and controls.
 *
 * Input Arguments:
 *   USBH_VIDEO_TERM_UNIT_INFO *p_term_info - Pointer to the terminal information structure
 *
 * Return Value:
 *   None
 *
 *******************************************************************************/
static void _print_extension_unit_info(USBH_VIDEO_TERM_UNIT_INFO *p_term_info)
{
    USBH_Logf_Application("  Unit ID %d", p_term_info->bTermUnitID);
    USBH_Logf_Application("  Unit GUID 0x%x...", p_term_info->u.ExtUnit.guidExtensionCode[0]);
    USBH_Logf_Application("  Number of pins %d", p_term_info->u.ExtUnit.bNrInPins);
    USBH_Logf_Application("  Controls size %d", p_term_info->u.ExtUnit.bControlSize);
}

/*******************************************************************************
 * Function Name: _print_term_unit_info
 *******************************************************************************
 * Description:
 *   Logs information about a terminal or unit based on its type, delegating to
 *   specific print functions for different unit types. Adds a small delay to
 *   prevent flooding terminal output.
 *
 * Input Arguments:
 *   USBH_VIDEO_TERM_UNIT_INFO *p_term_info - Pointer to the terminal or unit information structure
 *
 * Return Value:
 *   None
 *
 *******************************************************************************/
static void _print_term_unit_info(USBH_VIDEO_TERM_UNIT_INFO *p_term_info)
{
    USBH_Logf_Application("  Terminal/Unit ID %d type %s", p_term_info->bTermUnitID, _term_type_to_str(p_term_info->Type));
    switch (p_term_info->Type)
    {
        case USBH_VIDEO_VC_INPUT_TERMINAL:
            _print_input_term_info(p_term_info);
            break;
        case USBH_VIDEO_VC_OUTPUT_TERMINAL:
            _print_output_term_info(p_term_info);
            break;
        case USBH_VIDEO_VC_SELECTOR_UNIT:
            _print_selector_unit_info(p_term_info);
            break;
        case USBH_VIDEO_VC_PROCESSING_UNIT:
            _print_processing_unit_info(p_term_info);
            break;
        case USBH_VIDEO_VC_EXTENSION_UNIT:
            _print_extension_unit_info(p_term_info);
            break;
        default:
            USBH_Logf_Application("  Type %d is not handled...", p_term_info->Type);
            break;
    }
    USBH_Logf_Application("---------------");
    /* Small delay so that we do not flood terminal output */
    cy_rtos_delay_milliseconds(10); 
}


/*******************************************************************************
 * Function Name: _on_dev_ready
*******************************************************************************
 * Description:
 *   Handles the initialization and configuration of a USB video device when it is
 *   ready. Enumerates terminals, formats, and frames, configures the video stream,
 *   and manages frame reception until the device is disconnected or an error occurs.
 *
 * Input Arguments:
 *   U8 dev_index - Index of the video device
 *
 * Return Value:
 *   None
 *
 *******************************************************************************/
static void _on_dev_ready(U8 dev_index)
{
    USBH_VIDEO_INPUT_HEADER_INFO input_header_info;
    USBH_VIDEO_DEVICE_HANDLE h_device;
    USBH_VIDEO_INTERFACE_INFO iface_info;
    USBH_VIDEO_TERM_UNIT_INFO term_info;
    USBH_VIDEO_STREAM_CONFIG stream_info;
    USBH_VIDEO_COLOR_INFO color_info;
    USBH_VIDEO_STREAM_HANDLE stream;
    USBH_VIDEO_FORMAT_INFO format;
    USBH_VIDEO_FRAME_INFO frame;
    USBH_STATUS status;
    unsigned requested_frame_interval_idx;
    unsigned requested_format_idx;
    unsigned requested_frame_idx;
    unsigned num_frame_descriptors;
    unsigned found;
    unsigned i;
    unsigned j;
    unsigned k;
    int r;
    U8 mb_event;
    U32 frame_interval_frm_vidpid;

    /* Open the device, the device index is retrieved from the notification callback */ 
    requested_format_idx = 0;
    requested_frame_idx = 0;
    requested_frame_interval_idx = 0;
    found = 0;

    memset(&h_device, 0, sizeof(h_device));
    memset(&stream, 0, sizeof(stream));
    /* Add timeout tracking for device initialization */ 
    float timeout_start_time = ifx_time_get_ms_f();
    /* 5 seconds timeout */
    float timeout_ms = 5000; 

    status = USBH_VIDEO_Open(dev_index, &h_device);
    if (status != USBH_STATUS_SUCCESS)
    {
        USBH_Logf_Application("USBH_VIDEO_Open returned with error: %s", USBH_GetStatusStr(status));
        return;
    }

    status = USBH_VIDEO_GetInterfaceInfo(h_device, &iface_info);
    if (status != USBH_STATUS_SUCCESS)
    {
        USBH_Logf_Application("USBH_VIDEO_GetInterfaceInfo returned with error: %s", USBH_GetStatusStr(status));
        return;
    }
    USBH_Logf_Application("====================================================================");
    USBH_Logf_Application("Vendor  Id = 0x%0.4X", iface_info.VendorId);
    USBH_Logf_Application("Product Id = 0x%0.4X", iface_info.ProductId);

    if ((LOGI_TECH_C920_VID == iface_info.VendorId) && (LOGI_TECH_C920_PID == iface_info.ProductId))
    {
        Camera_not_supported = 0;
        frame_interval_frm_vidpid = FRAME_INTERVAL_1;
        logitech_camera_enabled = 1;
        point3mp_camera_enabled = 0;
        twomp_camera_enabled = 0;
    }
    else if ((LOGI_TECH_C920_VID == iface_info.VendorId) && (LOGI_TECH_C920e_PID == iface_info.ProductId))
    {
        Camera_not_supported = 0;
        frame_interval_frm_vidpid = FRAME_INTERVAL_1;
        logitech_camera_enabled = 1;
        point3mp_camera_enabled = 0;
        twomp_camera_enabled = 0;
    }
    else if ((HBV_CAM_0P3_VID == iface_info.VendorId) && (HBV_CAM_0P3_PID == iface_info.ProductId))
    {
        Camera_not_supported = 0;
        frame_interval_frm_vidpid = FRAME_INTERVAL_2;
        logitech_camera_enabled = 0;
        point3mp_camera_enabled = 1;
        twomp_camera_enabled = 0;
    }
    else if ((HBV_CAM_2P0_VID == iface_info.VendorId) && (HBV_CAM_2P0_PID == iface_info.ProductId))
    {
        Camera_not_supported = 0;
        frame_interval_frm_vidpid = FRAME_INTERVAL;
        logitech_camera_enabled = 0;
        point3mp_camera_enabled = 0;
        twomp_camera_enabled = 1;
    }
    else
    {
        cy_rtos_semaphore_set(&model_semaphore);
        Camera_not_supported = 1;
        return;
    }

    /* List all terminals/units */ 
    for (i = 0; i < iface_info.NumTermUnits; i++)
    {
        /* Check if we've been waiting too long for initialization */ 
        if ((ifx_time_get_ms_f() - timeout_start_time) > timeout_ms)
        {
            USBH_Logf_Application("Timeout waiting for device initialization during terminal enumeration. Resetting...");
            found = 0;
            goto cleanup;
        }

        status = USBH_VIDEO_GetTermUnitInfo(h_device, i, &term_info);
        if (status == USBH_STATUS_SUCCESS)
        {
            _print_term_unit_info(&term_info);
        }
        else
        {
            USBH_Logf_Application("USBH_VIDEO_GetTermUnitInfo returned with error: %s", USBH_GetStatusStr(status));
        }
    }

    /* This sets the text information fields and the webcam frame window visible */ 
    memset(&iface_info, 0, sizeof(iface_info));
    if (status != USBH_STATUS_SUCCESS && status != USBH_STATUS_NOT_FOUND)
    {
        USBH_Logf_Application("USBH_VIDEO_GetFirstTermUnitInfo returned with error: %s", USBH_GetStatusStr(status));
    }
    else
    {
        /* Configure the VIDEO device */ 
        status = USBH_VIDEO_GetInputHeader(h_device, &input_header_info);
        if (status != USBH_STATUS_SUCCESS)
        {
            USBH_Logf_Application("USBH_VIDEO_GetInputHeader returned with error: %s", USBH_GetStatusStr(status));
        }
        else
        {
            USBH_Logf_Application(
                    "Video Interface with index %d (%d formats reported, still capture method %d):",
                    dev_index,
                    input_header_info.bNumFormats,
                    input_header_info.bStillCaptureMethod);
            /* Iterate over all formats */ 
            for (i = 0; i < input_header_info.bNumFormats; i++)
            {
                /* Check if we've been waiting too long for initialization */ 
                if ((ifx_time_get_ms_f() - timeout_start_time) > timeout_ms)
                {
                    USBH_Logf_Application("Timeout waiting for device initialization during format discovery. Resetting...");
                    found = 0;
                    break;
                }

                status = USBH_VIDEO_GetFormatInfo(h_device, i, &format);
                if (status != USBH_STATUS_SUCCESS)
                {
                    USBH_Logf_Application(
                            "USBH_VIDEO_GetformatInfo returned with error: %s",
                            USBH_GetStatusStr(status));
                }
                else
                {
                    switch (format.FormatType)
                    {
                        case USBH_VIDEO_VS_FORMAT_UNCOMPRESSED:
                            num_frame_descriptors = format.u.UncompressedFormat.bNumFrameDescriptors;
                            break;
                        case USBH_VIDEO_VS_FORMAT_MJPEG:
                            num_frame_descriptors = format.u.MJPEG_Format.bNumFrameDescriptors;
                            break;
                        case USBH_VIDEO_VS_FORMAT_FRAME_BASED:
                            num_frame_descriptors = format.u.H264_Format.bNumFrameDescriptors;
                            break;
                        default:
                            USBH_Logf_Application("  format type %d is not supported!:", format.FormatType);
                            status = USBH_STATUS_ERROR;
                    }

                    /* If we have a valid format type - proceed with parsing */ 
                    if (status == USBH_STATUS_SUCCESS)
                    {
                        USBH_Logf_Application("  format Index %d, type %s:", i, _format_type_to_str(format.FormatType));
                        /* Check if the format has a color matching descriptor */ 
                        status = USBH_VIDEO_GetColorMatchingInfo(h_device, i, &color_info);
                        if (status != USBH_STATUS_SUCCESS)
                        {
                            USBH_Logf_Application("  No color matching descriptor (%s)", USBH_GetStatusStr(status));
                        }
                        else
                        {
                            USBH_Logf_Application(
                                    "  Color matching descriptor: bColorPrimaries 0x%x, bTransferCharacteristics 0x%x, bMatrixCoefficients 0x%x",
                                    color_info.bColorPrimaries,
                                    color_info.bTransferCharacteristics,
                                    color_info.bMatrixCoefficients);
                        }

                        /* Iterate over all frame types */ 
                        for (j = 0; j < num_frame_descriptors; j++)
                        {
                            /* Check timeout during frame discovery */ 
                            if ((ifx_time_get_ms_f() - timeout_start_time) > timeout_ms)
                            {
                                USBH_Logf_Application("Timeout waiting for device initialization during frame discovery. Resetting...");
                                found = 0;
                                break;
                            }

                            status = USBH_VIDEO_GetFrameInfo(h_device, i, j, &frame);
                            if (status != USBH_STATUS_SUCCESS)
                            {
                                USBH_Logf_Application(
                                        "USBH_VIDEO_GetframeInfo returned with error: %s",
                                        USBH_GetStatusStr(status));
                            }
                            else
                            {
                                if (frame.bFrameIntervalType == 0)
                                {
                                    /* frame interval type zero (continuous frame interval) is almost never used by any device */ 
                                    USBH_Logf_Application(
                                            "    frame Index %d, type %s, x %d, y %d, %d min frame interval, %d max frame interval, %d interval step",
                                            j,
                                            _frame_type_to_str(frame.FrameType),
                                            frame.wWidth,
                                            frame.wHeight,
                                            frame.bFrameIntervalType,
                                            frame.u.dwMinFrameInterval,
                                            frame.u.dwMaxFrameInterval,
                                            frame.u.dwFrameIntervalStep);
                                }
                                else
                                {
                                    _get_frame_intervals(&frame);
                                    USBH_Logf_Application(
                                            "    frame Index %d, type %s, x %d, y %d, intervals (%d): { %s }",
                                            j,
                                            _frame_type_to_str(frame.FrameType),
                                            frame.wWidth,
                                            frame.wHeight,
                                            frame.bFrameIntervalType,
                                            _ac);
                                    /* Try to find a setting which matches our defines */ 
                                    if (format.FormatType == FORMAT)
                                    {
                                        if ((frame.wHeight == CAMERA_HEIGHT) && (frame.wWidth == CAMERA_WIDTH))
                                        {
                                            for (k = 0; k < frame.bFrameIntervalType; k++)
                                            {
                                                if (frame.u.dwFrameInterval[k] == frame_interval_frm_vidpid)
                                                {
                                                    if (found != 1u)
                                                    {
                                                        requested_format_idx = i;
                                                        requested_frame_idx = j;
                                                        requested_frame_interval_idx = k + 1;
                                                        found = 1;
                                                        USBH_Logf_Application(
                                                                "--->found requested setting (%dx%d @ %d FPS): format Index %d, frame Index %d, frame Interval Index %d",
                                                                CAMERA_WIDTH,
                                                                CAMERA_HEIGHT,
                                                                10000000 / frame.u.dwFrameInterval[k],
                                                                requested_format_idx,
                                                                requested_frame_idx,
                                                                requested_frame_interval_idx);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                /* Small delay so that we do not flood terminal output */
                                cy_rtos_delay_milliseconds(10); 
                            }
                        }
                    }
                }
            }

            /* If we didn't find an exact format match, try to use first available format */ 
            if (found != 1u && input_header_info.bNumFormats > 0)
            {
                USBH_Logf_Application("WARNING: Couldn't find exact requested format. Using first available format instead.");
                /* Fall back to first available format if any exist */ 
                requested_format_idx = 0;
                requested_frame_idx = 0;
                requested_frame_interval_idx = 1; 
                found = 1;
            }
        }

        /* Receive frame data */ 
        memset(&stream_info, 0, sizeof(USBH_VIDEO_STREAM_CONFIG));
        stream_info.Flags = 0;
        if (found == 1u)
        {
            stream_info.FormatIdx = requested_format_idx;
            stream_info.FrameIdx = requested_frame_idx;
            stream_info.FrameIntervalIdx = requested_frame_interval_idx;
            stream_info.pfDataCallback = _on_data_cb;

            /* Initialize the frame monitoring variables */ 
            last_buffer = 0;
            last_check_time = ifx_time_get_ms_f();
            cy_rtos_queue_reset(&_device_state_mail_box);
            status = USBH_VIDEO_OpenStream(h_device, &stream_info, &stream);
            if (status != USBH_STATUS_SUCCESS)
            {
                USBH_Logf_Application("USBH_VIDEO_Openstream returned with error: %s", USBH_GetStatusStr(status));
            }
            else
            {
                /* Wait until the device has been disconnected */ 
                for (;;)
                {
                    /* Check for stalled camera frames */ 
                    float current_time = ifx_time_get_ms_f();
                    if ((current_time - last_check_time) > 1000)
                    {
                        last_check_time = current_time;

#ifdef ENABLE_USB_WEBCAM_TIMEOUT
                        if (last_frame_time > 0 && (current_time - last_frame_time) > frame_timeout_ms)
                        {
                            printf("[ERROR] No frames received for %.2f seconds, camera is stalled", frame_timeout_ms / 1000.0);
                            /* Force camera reset */ 
                            break;
                        }
#endif /* ENABLE_USB_WEBCAM_TIMEOUT */
                    }
                    /* Checking if the queue operation was successful */ 
                    r = cy_rtos_queue_get(&_device_state_mail_box, (char *)&mb_event, 100);
                    if (r == CY_RSLT_SUCCESS)
                    {
                        if (mb_event == 0xff)
                        {
                            USBH_Logf_Application("USBH_VIDEO_Openstream breaking - Device was disconnected");
                            break;
                        }
                        else if (mb_event == 0xfe)
                        {
                            USBH_Logf_Application("USBH_VIDEO_Openstream breaking - Transfer status error");
                            break;
                        }
                        else
                        {
                            USBH_Logf_Application("USBH_VIDEO_Openstream received unknown error code: 0x%02x", mb_event);
                            if (_device_connected)
                                mb_event = 0xfe;
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            /* Display error text until webcam is removed */ 
            while (_device_connected)
            {
                USBH_OS_Delay(50);
            }
        }
    }

cleanup:
    /* Close the device */ 
    USBH_Logf_Application("Closing video device...");
    USBH_VIDEO_Close(h_device);
}

/*******************************************************************************
 * Function Name: cm55_usb_webcam_task
*******************************************************************************
 * Description:
 *   Main task for handling USB webcam operations. Initializes USB host and video
 *   subsystems, creates tasks for USB handling, sets up mailboxes, and processes
 *   device readiness events from the video mailbox.
 *
 * Input Arguments:
 *   void *arg - Task argument (not used)
 *
 * Return Value:
 *   None
 *
 *******************************************************************************/
void cm55_usb_webcam_task(void *arg)
{
    const uint32_t task_stack_size_bytes = 8192;
    U8 dev_index;
    cy_rslt_t result;

    USBH_Init();
    result = cy_rtos_create_thread(&usbh_main_task_handle, (void*)USBH_Task, "USBH_Task",
                                   NULL, task_stack_size_bytes, (uint8_t)TASK_PRIO_USBH_MAIN, NULL);
    if (CY_RSLT_SUCCESS != result)
    {
        CY_ASSERT(0);
    }

    result = cy_rtos_create_thread(&usbh_isr_task_handle, (void*)USBH_ISRTask, "USBH_isr",
                                   NULL, task_stack_size_bytes, (uint8_t)TASK_PRIO_USBH_ISR, NULL);
    if ( CY_RSLT_SUCCESS != result ) {
        CY_ASSERT(0);
    }

    result = cy_rtos_queue_init(&_video_mail_box, sizeof(U8), MAX_VIDEO_INTERFACES);
    if (CY_RSLT_SUCCESS != result)
    {
        CY_ASSERT(0);
    }

    result = cy_rtos_queue_init(&_device_state_mail_box, sizeof(U8), 1);
    if (CY_RSLT_SUCCESS != result)
    {
        CY_ASSERT(0);
    }

    USBH_VIDEO_Init();
    USBH_VIDEO_AddNotification(&hook, _add_remove_device_cb, NULL);

    while (true)
    {
        if (CY_RSLT_SUCCESS == cy_rtos_queue_get(&_video_mail_box, &dev_index, 100))
        {
            _on_dev_ready(dev_index);
        }
    }
}

