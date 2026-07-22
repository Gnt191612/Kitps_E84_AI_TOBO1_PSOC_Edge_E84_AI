/*******************************************************************************
 * File Name        : main.c
 *
 * Description      : This source file contains the main routine for non-secure
 *                    application running on CM55 CPU.
 *
 * Related Document : See README.md
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
 * Header Files
 *******************************************************************************/
#include <stdint.h>
#include "cybsp.h"
#include "retarget_io_init.h"
#include "cyabs_rtos.h"
#include "cyabs_rtos_impl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "inference_task.h"
#include "model.h"
#include "lcd_task.h"
#include "ifx_time_utils.h"
#include "cycfg_qspi_memslot.h"
#include "mtb_serial_memory.h"
#ifdef USE_DVP_CAM
#include "mtb_dvp_camera_ov7675.h"
#endif

/******************************************************************************
 * Macros
 ******************************************************************************/
/* The timeout value in microsecond used to wait for core to be booted */
#define CM55_BOOT_WAIT_TIME_USEC            (10U)
#define SMIF_INIT_TIMEOUT_USEC              (1000u)  /* 1ms SMIF init timeout */

#define GFX_TASK_NAME                       ( "CM55 Gfx Task" )
#define GFX_TASK_STACK_SIZE                 ( 10U * 1024U )
#define GFX_TASK_PRIORITY                   ( configMAX_PRIORITIES - 5 )

#define USB_WEBCAM_TASK_NAME                ( "CM55 USB Webcam Task")
#define USB_WEBCAM_TASK_STACK_SIZE          ( 20U * 1024U )
#define USB_WEBCAM_TASK_PRIORITY            ( configMAX_PRIORITIES - 3 )

/******************************************************************************
 * Global Variables
 ******************************************************************************/
#ifdef USE_USB_CAM
static cy_thread_t usb_webcam_thread;
static cy_thread_t inference_thread;
#endif
static cy_thread_t gfx_thread;
static mtb_serial_memory_t serial_memory_obj;
static cy_stc_smif_mem_context_t smif_mem_context;
static cy_stc_smif_mem_info_t smif_mem_info;

#ifdef USE_DVP_CAM
/* double-buffered array to store image frames from the DVP camera, where one
 * buffer is being displayed while the other is being filled with new data */
vg_lite_buffer_t dvp_bgr565_frames[NUM_IMAGE_BUFFERS];
/* flag to indicate whether a new frame is available for processing */
bool frame_ready = false;
/* flag to indicate which frame is currently being displayed or processed */
bool active_frame = false;
/* I2C master context for communication with the DVP camera */
extern cy_stc_scb_i2c_context_t i2c_controller_context;
#endif

cy_semaphore_t model_semaphore;
cy_semaphore_t usb_semaphore;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/
void cm55_ns_gfx_task ( void *arg );
void cm55_usb_webcam_task ( void *arg );
void cm55_inference_task ( void *arg );
void check_status(char *message, uint32_t status);

 /*******************************************************************************
 * Function Name: vApplicationStackOverflowHook
 ********************************************************************************
 * Summary:
 *   Hook function called when a stack overflow is detected for a task. Triggers
 *   an assertion to halt execution.
 *
 * Parameters:
 *   TaskHandle_t xTask - Handle of the task that overflowed (not used)
 *   char *pcTaskName - Name of the task that overflowed (not used)
 *
 * Return:
 *   None
 *
 *******************************************************************************/
void vApplicationStackOverflowHook ( TaskHandle_t xTask, char *pcTaskName )
{
    CY_UNUSED_PARAMETER(xTask);
    CY_UNUSED_PARAMETER(pcTaskName);
    configASSERT(0);
}

 /*******************************************************************************
 * Function Name: vApplicationMallocFailedHook
 ********************************************************************************
 * Summary:
 *   Hook function called when a memory allocation failure is detected. Triggers
 *   an assertion to halt execution.
 *
 * Parameters:
 *  none
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void vApplicationMallocFailedHook ( void )
{
    configASSERT(0);
}

 /*******************************************************************************
 * Function Name: smif_ospi_psram_init
 ********************************************************************************
 * Summary:
 *  Initialize SMIF for PSRAM (QSPI Core 1) and set-up serial memory interfeace
 *
 * Parameters:
 *  none
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void smif_ospi_psram_init(void)
{
     cy_rslt_t result;

        /* Disable SMIF Block for reconfiguration. */
        Cy_SMIF_Disable(CYBSP_SMIF_CORE_1_PSRAM_HW);

        /* Initialize SMIF-1 Peripheral. */
        result = Cy_SMIF_Init((CYBSP_SMIF_CORE_1_PSRAM_hal_config.base),
                               (CYBSP_SMIF_CORE_1_PSRAM_hal_config.config),
                               SMIF_INIT_TIMEOUT_USEC, &smif_mem_context.smif_context);

        check_status("Cy_SMIF_Init failed", result);

        /* Configure Data Select Option for SMIF-1 */
        Cy_SMIF_SetDataSelect(CYBSP_SMIF_CORE_1_PSRAM_hal_config.base,
                              smif1BlockConfig.memConfig[0]->slaveSelect,
                              smif1BlockConfig.memConfig[0]->dataSelect);

        /* Enable the SMIF_CORE_1 block. */
        Cy_SMIF_Enable(CYBSP_SMIF_CORE_1_PSRAM_hal_config.base, &smif_mem_context.smif_context);

        /* Set-up serial memory. */
        result = mtb_serial_memory_setup(&serial_memory_obj,
                                    MTB_SERIAL_MEMORY_CHIP_SELECT_2,
                                    CYBSP_SMIF_CORE_1_PSRAM_hal_config.base,
                                    CYBSP_SMIF_CORE_1_PSRAM_hal_config.clock,
                                    &smif_mem_context,
                                    &smif_mem_info,
                                    &smif1BlockConfig);

        check_status("serial memory setup failed", result);

}


 /*******************************************************************************
  * Function Name: check_status
  ********************************************************************************
  * Summary:
  *  Prints the message, indicates the non-zero status by turning the LED on, and
  *  asserts the non-zero status.
  *
  * Parameters:
  *  message - message to print if status is non-zero.
  *  status - status for evaluation.
  *
  * Return:
  *  void
  *
  *******************************************************************************/
  void check_status(char *message, uint32_t status)
 {
     if (status)
     {
         printf("\n\r====================================================\n\r");
         printf("\n\rFAIL: %s\n\r", message);
         printf("Error Code: 0x%x\n\r", (int) status);
         printf("\n\r====================================================\n\r");

         /* On failure, turn the LED ON */
         Cy_GPIO_Set(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
         while(true);
     }
 }

#if ( configGENERATE_RUN_TIME_STATS == 1 )
/*******************************************************************************
* Function Name: setup_run_time_stats_timer
********************************************************************************
* Summary:
*  This function configuresTCPWM 0 GRP 0 Counter 0 as the timer source for
*  FreeRTOS runtime statistics.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void setup_run_time_stats_timer(void)
{
    /* Initialze TCPWM block with required timer configuration */
    if (CY_TCPWM_SUCCESS != Cy_TCPWM_Counter_Init(CYBSP_GENERAL_PURPOSE_TIMER_HW,
        CYBSP_GENERAL_PURPOSE_TIMER_NUM,
        &CYBSP_GENERAL_PURPOSE_TIMER_config))
    {
        handle_app_error();
    }

    /* Enable the initialized counter */
    Cy_TCPWM_Counter_Enable(CYBSP_GENERAL_PURPOSE_TIMER_HW,
                            CYBSP_GENERAL_PURPOSE_TIMER_NUM);

    /* Start the counter */
    Cy_TCPWM_TriggerStart_Single(CYBSP_GENERAL_PURPOSE_TIMER_HW,
                                 CYBSP_GENERAL_PURPOSE_TIMER_NUM);
}


/*******************************************************************************
* Function Name: get_run_time_counter_value
********************************************************************************
* Summary:
*  Function to fetch run time counter value. This will be used by FreeRTOS for
*  run time statistics calculation.
*
* Parameters:
*  void
*
* Return:
*  uint32_t: TCPWM 0 GRP 0 Counter 0 value
*
*******************************************************************************/
uint32_t get_run_time_counter_value(void)
{
   return (Cy_TCPWM_Counter_GetCounter(CYBSP_GENERAL_PURPOSE_TIMER_HW,
                                       CYBSP_GENERAL_PURPOSE_TIMER_NUM));
}


/*******************************************************************************
* Function Name: calculate_idle_percentage
********************************************************************************
* Summary:
*  Function to calculate CPU idle percentage. This function is used by LVGL to
*  showcase CPU usage.
*
* Parameters:
*  void
*
* Return:
*  uint32_t: CPU idle percentage
*
*******************************************************************************/
uint32_t calculate_idle_percentage(void)
{
    static uint32_t previous_idle_time = 0;
    static TickType_t previous_tick = 0;
    uint32_t time_diff = 0;
    uint32_t idle_percent = 0;

    uint32_t current_idle_time = ulTaskGetIdleRunTimeCounter();
    TickType_t current_tick = portGET_RUN_TIME_COUNTER_VALUE();

    time_diff = current_tick - previous_tick;

    if((current_idle_time >= previous_idle_time) && (current_tick > previous_tick))
    {
        idle_percent = ((current_idle_time - previous_idle_time) * 100)/time_diff;
    }

    previous_idle_time = ulTaskGetIdleRunTimeCounter();
    previous_tick = portGET_RUN_TIME_COUNTER_VALUE();

    return idle_percent;
}
#endif

/*******************************************************************************
 * Function Name: main
 ********************************************************************************
 * Summary:
 *  This is the main function for CM55 non-secure application.
 *    1. It initializes the device and board peripherals.
 *    2. It creates the FreeRTOS application task 'cm55_ns_gfx_task'.
 *    3. It starts the RTOS task scheduler.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  int
 *
 *******************************************************************************/
int main ( void )
{
    cy_rslt_t   result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if ( CY_RSLT_SUCCESS != result ) {
        /* Board init failed. Stop program execution */
        CY_ASSERT(0);       
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io middleware */
    init_retarget_io();

    /* Prevents this core from entering deepsleep */
    mtb_hal_syspm_lock_deepsleep();

    ifx_time_start();

    /* Initialize PSRAM and set-up serial memory */
    smif_ospi_psram_init();

    check_status("smif_ospi_psram_init error", (uint32_t)result);

    /* Enable XIP mode for the SMIF memory slot associated with the PSRAM. */
    result = mtb_serial_memory_enable_xip(&serial_memory_obj, true);
    check_status("mtb_serial_memory_enable_xip: failed", result);

    /* Enable write for the SMIF memory slot associated with the PSRAM. */
    result = mtb_serial_memory_set_write_enable(&serial_memory_obj, true);
    check_status("mtb_serial_memory_set_write_enable: failed", result);

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("\r\n******************PSOC Edge MCU: Machine learning DEEPCRAFT deploy vision******************\r\n");
    printf("Build Version: %d.%d.%d\r\n", MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    printf("Build Date: %s\r\n", __DATE__);
    printf("Build Time: %s\r\n", __TIME__);
    printf("Following cameras are supported:\r\n");
    printf("1. HBVCAM OV7675 0.3MP Camera: https://www.hbvcamera.com/0-3mp-pixel-usb-cameras/hbvcam-ov7675-0.3mp-mini-laptop-camera-module.html\r\n");
    printf("2. Logitech C920 HD Pro Webcam: https://www.logitech.com/en-ch/shop/p/c920-pro-hd-webcam\r\n");
    printf("3. Logitech C920e Business Webcam: https://www.logitech.com/en-ch/products/webcams/c920e-business-webcam\r\n");
    printf("4. HBVCAM OS02F10 2MP Camera: https://www.hbvcamera.com/2-mega-pixel-usb-cameras/2mp-1080p-auto-focus-hd-usb-camera-module-for-atm-machine.html\r\n");
    printf("5. OV7675 0.3MP DVP Camera: https://blog.arducam.com/products/camera-breakout-board/0-3mp-ov7675\r\n");
    printf("\r\n*************************************************************************************\r\n");

    result = cy_rtos_semaphore_init(&usb_semaphore, NUM_IMAGE_BUFFERS, 0);
    if ( CY_RSLT_SUCCESS != result ) {
        CY_ASSERT(0);
    }

    result = cy_rtos_semaphore_init(&model_semaphore, NUM_IMAGE_BUFFERS, 0);
    if ( CY_RSLT_SUCCESS != result ) {
        CY_ASSERT(0);
    }

    result = cy_rtos_thread_create( &gfx_thread, &cm55_ns_gfx_task, GFX_TASK_NAME, NULL,
                                    GFX_TASK_STACK_SIZE, GFX_TASK_PRIORITY, NULL );
    if ( CY_RSLT_SUCCESS != result ) {
        CY_ASSERT(0);
    }

#ifdef USE_USB_CAM
    result = cy_rtos_thread_create( &inference_thread, &cm55_inference_task, INFERENCE_TASK_NAME, NULL,
                                    INFERENCE_TASK_STACK_SIZE, INFERENCE_TASK_PRIORITY, NULL);
    if ( CY_RSLT_SUCCESS != result ) {
        CY_ASSERT(0);
    }


    result = cy_rtos_thread_create( &usb_webcam_thread, &cm55_usb_webcam_task, USB_WEBCAM_TASK_NAME, NULL,
                                    USB_WEBCAM_TASK_STACK_SIZE, USB_WEBCAM_TASK_PRIORITY, NULL);
    if ( CY_RSLT_SUCCESS != result ) {
        CY_ASSERT(0);
    }
#endif

#ifdef USE_DVP_CAM
    result = Cy_SCB_I2C_Init(CYBSP_I2C_CAM_CONTROLLER_HW, &CYBSP_I2C_CAM_CONTROLLER_config, &i2c_controller_context);
    if (CY_SCB_I2C_SUCCESS != result)
    {
        printf("I2C Master PDL initialization failed !!\n");
        CY_ASSERT(0);
    }
        /* Enable the I2C */
    Cy_SCB_I2C_Enable(CYBSP_I2C_CAM_CONTROLLER_HW);

    mtb_dvp_cam_ov7675_init(dvp_bgr565_frames, &i2c_controller_context, &frame_ready, &active_frame);
#endif
    /* Start the RTOS Scheduler */
    vTaskStartScheduler();

    /* Should never get here! */
    /* Halt the CPU if scheduler exits */
    CY_ASSERT(0);

    return 0;
}

/* [] END OF FILE */
