/******************************************************************************
* File Name:   lcd_task.c
*
* Description: This file implements the LCD display modules.
*
* Related Document: See README.md
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
* Header File
*******************************************************************************/
#include "cybsp.h"
#include "retarget_io_init.h"
#include "no_camera_img.h"
#include "camera_not_supported_img.h"
#include "mtb_disp_dsi_waveshare_4p3.h"
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOSConfig.h"
#include "usb_camera_task.h"
#include "lcd_task.h"
#include "inference_task.h"
#include "ifx_gui_render.h"
#include "font_16x36.h"
#include "ifx_image_utils.h"
#include "ifx_time_utils.h"
#include "lcd_graphics.h"

#if defined(MTB_SHARED_MEM)
#include "shared_mem.h"
#endif

/*******************************************************************************
 * Macros
 *******************************************************************************/
#define I2C_CONTROLLER_IRQ_PRIORITY         (4U)

#define GPU_INT_PRIORITY                    (3U)
#define DC_INT_PRIORITY                     (3U)

#define COLOR_DEPTH                         (16U)
#define BITS_PER_PIXEL                      (8U)

#define DISPLAY_HEIGHT                      (480U)
#define DISPLAY_WIDTH                       (832U)

#define DEFAULT_GPU_CMD_BUFFER_SIZE         ((64U) * (1024U))
#define GPU_TESSELLATION_BUFFER_SIZE        ((DISPLAY_HEIGHT) * 128U)

#define FRAME_BUFFER_SIZE                   ((DISPLAY_WIDTH) * (DISPLAY_HEIGHT) * ((COLOR_DEPTH) / (BITS_PER_PIXEL)))

#define VGLITE_HEAP_SIZE                    (((FRAME_BUFFER_SIZE) * (3)) + \
                                             (((DEFAULT_GPU_CMD_BUFFER_SIZE) + (GPU_TESSELLATION_BUFFER_SIZE)) * (NUM_IMAGE_BUFFERS)) + \
                                             ((CAMERA_BUFFER_SIZE) * (NUM_IMAGE_BUFFERS + 1)))

#define GPU_MEM_BASE                        (0x0U)

#define WHITE_COLOR                         (0x00FFFFFFU)
#define BLACK_COLOR                         (0x00000000U)
#define TARGET_NUM_FRAMES                   (15U)

/* Display I2C controller - Kit-specific configuration */
#ifdef USE_KIT_PSE84_AI
#define DISPLAY_I2C_CONTROLLER_HW           CYBSP_I2C_DISPLAY_CONTROLLER_HW
#define DISPLAY_I2C_CONTROLLER_IRQ          CYBSP_I2C_DISPLAY_CONTROLLER_IRQ
#define DISPLAY_I2C_CONTROLLER_config       CYBSP_I2C_DISPLAY_CONTROLLER_config
#else
#define DISPLAY_I2C_CONTROLLER_HW           CYBSP_I2C_CONTROLLER_HW
#define DISPLAY_I2C_CONTROLLER_IRQ          CYBSP_I2C_CONTROLLER_IRQ
#define DISPLAY_I2C_CONTROLLER_config       CYBSP_I2C_CONTROLLER_config
#endif

#define NO_CAMERA_IMG_X_POS                 ((MTB_DISP_WAVESHARE_4P3_HOR_RES / 2U) \
                                             - ((NO_CAMERA_IMG_WIDTH / 2U) + 10))
#define NO_CAMERA_IMG_Y_POS                 ((MTB_DISP_WAVESHARE_4P3_VER_RES / 2U) \
                                             - (NO_CAMERA_IMG_HEIGHT / 2U))

#define CAMERA_NOT_SUPPORTED_IMG_X_POS      ((MTB_DISP_WAVESHARE_4P3_HOR_RES / 2U) \
                                             - ((CAMERA_NOT_SUPPORTED_IMG_WIDTH / 2U) + 10))
#define CAMERA_NOT_SUPPORTED_IMG_Y_POS      ((MTB_DISP_WAVESHARE_4P3_VER_RES / 2U) \
                                             - (CAMERA_NOT_SUPPORTED_IMG_HEIGHT / 2U))

#ifdef RPS_DEMO_MODE_ENABLED
#define LED_BLINK_DELAY_MS                  (200)
#endif
/*******************************************************************************
 * Global Variables
 ****************************************************************************** */
#ifdef USE_USB_CAM
/* Last successful USB frame time*/
static float last_successful_frame_time = 0;
/* USB recovery attempt counter */                     
static int recovery_attempts = 0;                               
#endif
/* USB semaphore for synchronization */
extern cy_semaphore_t usb_semaphore; 
/* Object detection prediction */                          
extern prediction_od_t prediction;  
/* Inference time for model */                            
extern volatile float inference_time;
/* Model semaphore for synchronization */                           
extern cy_semaphore_t model_semaphore;   
/* Device connected */                       
extern uint8_t _device_connected;
/* Start time */                               
volatile float time_start1;   
/* Graphics context structure */                                  
cy_stc_gfx_context_t gfx_context; 
/* Render target buffer*/                              
vg_lite_buffer_t *render_target;   
/* USB YUV frame buffers */                              
vg_lite_buffer_t usb_yuv_frames[NUM_IMAGE_BUFFERS]; 
/* BGR565 buffer */            
vg_lite_buffer_t bgr565; 
/* Double display frame buffers */                                       
vg_lite_buffer_t display_buffer[3];  
/* Scale factor from camera to display */                           
float scale_cam_to_disp;                                           

#ifdef USE_DVP_CAM
/* Inference task moved inside GFX task for DVP cam */
void cm55_inference_task ( void *arg );                         
static cy_thread_t inference_thread;
extern vg_lite_buffer_t dvp_bgr565_frames[NUM_IMAGE_BUFFERS];
extern bool active_frame;
#endif

CY_SECTION(".cy_socmem_data")
/* BGR888 integer buffer */
__attribute__((aligned(64)))
uint8_t bgr888_uint8[(IMAGE_HEIGHT) * (IMAGE_WIDTH) * 3] = {0};   

CY_SECTION(".cy_xip") __attribute__((used))
/* Contiguous memory for VGLite heap */
uint8_t contiguous_mem[VGLITE_HEAP_SIZE]; 
/* VGLite heap base address */                      
volatile void *vglite_heap_base = &contiguous_mem; 
/* framebuffer pending flag */             
volatile bool fb_pending = false;   

/*******************************************************************************
 * Global Variables - Shared memory variable
 *******************************************************************************/
#if defined(MTB_SHARED_MEM)
oob_shared_data_t oob_shared_data_ns;
#endif

/*******************************************************************************
 * Global Variables - I2C Controller Configuration
 ****************************************************************************** */
 /* I2C controller context */
cy_stc_scb_i2c_context_t i2c_controller_context;                

/*******************************************************************************
 * Global Variables - Interrupt Configurations
 *******************************************************************************/
 /* DC Interrupt Configuration */
cy_stc_sysint_t dc_irq_cfg =                                   
{
    .intrSrc      = gfxss_interrupt_dc_IRQn,                   
    .intrPriority = DC_INT_PRIORITY                            
};
/* GPU Interrupt Configuration*/
cy_stc_sysint_t gpu_irq_cfg =                                  
{
    .intrSrc      = gfxss_interrupt_gpu_IRQn,                  
    .intrPriority = GPU_INT_PRIORITY                           
};
/* I2C Controller Interrupt Configuration */
cy_stc_sysint_t i2c_controller_irq_cfg =                       
{
    .intrSrc      = DISPLAY_I2C_CONTROLLER_IRQ,                  
    .intrPriority = I2C_CONTROLLER_IRQ_PRIORITY                
};

/*******************************************************************************
 * Local Variables
 *******************************************************************************/
 /* Graphics subsystem base address */
static GFXSS_Type *base = (GFXSS_Type *)GFXSS;     
/* VGLite transformation matrix */            
static vg_lite_matrix_t matrix;
/* Display X offset */                                
static int display_offset_x = 0;   
/* Display Y offset */                            
static int display_offset_y = 0;                               

/* Red components: {Green, Black, Red, Blue} */
static uint8_t color_r[4] = {0, 0, 227, 8}; 
/* Green components: {Green, Black, Red, Blue} */                   
static uint8_t color_g[4] = {255, 0, 66, 24};  
/* Blue components: {Green, Black, Red, Blue} */                
static uint8_t color_b[4] = {0, 0, 52, 168};                   

CY_SECTION_ITCM_BEGIN
/*******************************************************************************
* Function Name: mirror_image
********************************************************************************
* Description: Mirrors an image horizontally by swapping pixels from left to right
*              in the provided buffer. The function assumes a fixed bytes-per-pixel
*              value of 2 (e.g., for RGB565 format) and operates on a buffer with
*              dimensions defined by CAMERA_WIDTH and CAMERA_HEIGHT.
* Parameters:
*   - buffer: Pointer to the vg_lite_buffer_t structure containing the image data
*
* Return:
*   None
********************************************************************************/
void mirror_image(vg_lite_buffer_t *buffer) {
    uint8_t temp[4];
    uint8_t *start, *end;
    int m, n;
    int bytes_per_pixel  = 2;

    for (m = 0; m < CAMERA_HEIGHT ; m++) {

        start = buffer->memory + m * (CAMERA_WIDTH * bytes_per_pixel);

        end = start + (CAMERA_WIDTH - 1) * bytes_per_pixel;

        for (n = 0; n < CAMERA_WIDTH / 2; n++) {

            for (int i = 0; i < bytes_per_pixel; i++) {
                temp[i] = start[i];
            }

            for (int i = 0; i < bytes_per_pixel; i++) {
                start[i] = end[i];
            }

            for (int i = 0; i < bytes_per_pixel; i++) {
                end[i] = temp[i];
            }

            start += bytes_per_pixel;
            end -= bytes_per_pixel;
        }
    }
}

CY_SECTION_ITCM_END
/*******************************************************************************
* Function Name: cleanup
********************************************************************************
* Description: Deallocates resources and frees memory used by the VGLite
*              graphics library. This function should be called to ensure proper
*              cleanup of VGLite resources when they are no longer needed.
* Parameters:
*   None
*
* Return:
*   None
********************************************************************************/
static void cleanup ( void )
{
    /* Deallocate all the resource and free up all the memory */
    vg_lite_close();
}


CY_SECTION_ITCM_BEGIN
/*******************************************************************************
* Function Name: draw
********************************************************************************
 * Description: Processes and renders an image from a USB camera buffer. The function
 *              performs the following steps:
 *              1. Waits for a ready image buffer from the camera.
 *              2. Converts a 320x240 YUYV 422 image to 320x240 BGR565 format.
 *              3. Optionally mirrors the image if the 3MP camera is disabled.
 *              4. Scales the 320x240 BGR565 image to 800x600 BGR565 for display.
 *              5. Converts the 320x240 BGR565 image to 256x240 BGR888 format (offset by -128).
 *              6. Tracks performance metrics for each step and returns the BGR888 buffer.
 *              The function handles errors by invoking cleanup and asserting on failure.
* Parameters:
*   bgr888_int8 Pointer to the int8_t BGR888 buffer containing the processed image data
*
* Return:
*   None
*
********************************************************************************/
uint8_t * draw(void)
{
    vg_lite_error_t error = VG_LITE_SUCCESS;
    volatile uint32_t time_draw_start = ifx_time_get_ms_f();
#ifdef USE_USB_CAM
    extern uint8_t last_buffer;
    extern video_buffer_t _image_buff[];

    // find a ready buffer
    uint32_t work_buffer = last_buffer;

    while (_image_buff[work_buffer].buff_ready == 0) {
        for (int32_t ii = 0; ii < NUM_IMAGE_BUFFERS; ii++)
            if (_image_buff[(work_buffer + ii) % NUM_IMAGE_BUFFERS].buff_ready == 1) {
                work_buffer = (work_buffer + ii) % NUM_IMAGE_BUFFERS;
                break;
            }
        cy_rtos_delay_milliseconds(1);
    }

    /* Reset all other buffers to available for input from camera */ 
    for (int32_t ii = 1; ii < NUM_IMAGE_BUFFERS; ii++)
        _image_buff[(work_buffer + ii) % NUM_IMAGE_BUFFERS].buff_ready = 0;
#endif
    /* convert 320x240 YUYV 422 image into 320*240 BGR565 (scale:1) */
    volatile uint32_t time_draw_1 = ifx_time_get_ms_f();
#ifdef USE_USB_CAM
    error = vg_lite_blit(&bgr565, &usb_yuv_frames[work_buffer],
                         NULL,                       // identity matrix
                         VG_LITE_BLEND_NONE,
                         0,
                         VG_LITE_FILTER_POINT);
#endif
#ifdef USE_DVP_CAM
    error = vg_lite_blit(&bgr565, &dvp_bgr565_frames[active_frame],
                         NULL,                       // identity matrix
                         VG_LITE_BLEND_NONE,
                         0,
                         VG_LITE_FILTER_POINT);
#endif

    if (error) {
        printf("\r\nvg_lite_blit() (320x240 YUYV 422 ==> 320*240 BGR565) returned error %d\r\n", error);
        cleanup();
        CY_ASSERT(0);
    }

    vg_lite_finish();

#ifdef USE_USB_CAM
    if (!point3mp_camera_enabled) {
        mirror_image(&bgr565);
    }
#endif
    /* convert 320x240 BGR565 image into 800x600 BGR565 (scale:2.5) */
    volatile uint32_t time_draw_3 = ifx_time_get_ms_f();
    error = vg_lite_blit(render_target, &bgr565,
                         &matrix,
                         VG_LITE_BLEND_NONE,
                         0,
                         VG_LITE_FILTER_POINT);
    if (error) {
        printf("\r\nvg_lite_blit() (320x240 BGR565 ==> 800x600 display BGR565) returned error %d\r\n", error);
        cleanup();
        CY_ASSERT(0);
    }
    vg_lite_finish();

#ifdef USE_USB_CAM
    /* Clear USB buffer */
    _image_buff[work_buffer].num_bytes = 0;
    _image_buff[work_buffer].buff_ready = 0;
#endif
    /* Convert 320x240 BGR565 image into 256x240 BGR888 - 128 */
    volatile uint32_t time_draw_5 = ifx_time_get_ms_f();
    ifx_image_conv_RGB565_to_RGB888_i8(bgr565.memory, CAMERA_WIDTH, CAMERA_HEIGHT, bgr888_uint8, IMAGE_WIDTH, IMAGE_HEIGHT);

    volatile uint32_t time_draw_end = ifx_time_get_ms_f();
    // performance measures: time
    extern float prep_wait_buf, prep_YUV422_to_bgr565, prep_bgr565_to_disp, prep_RGB565_to_RGB888;
    prep_wait_buf         = time_draw_1 - time_draw_start;
    prep_YUV422_to_bgr565 = time_draw_3 - time_draw_1;
    prep_bgr565_to_disp   = time_draw_5 - time_draw_3;
    prep_RGB565_to_RGB888 = time_draw_end - time_draw_5;

    return bgr888_uint8;
}
CY_SECTION_ITCM_END

/*******************************************************************************
* Function Name: dc_irq_handler
********************************************************************************
* Summary: This is the display controller I2C interrupt handler.
*
* Parameters:
*   None
*
* Return:
*   None
*
*******************************************************************************/
static void dc_irq_handler ( void )
{
    fb_pending = false;
    Cy_GFXSS_Clear_DC_Interrupt(base, &gfx_context);
}

/*******************************************************************************
* Function Name: dc_gpu_irq_handlerirq_handler
********************************************************************************
* Summary: This is the GPU interrupt handler.
*
* Parameters:
*   None
*
* Return:
*   None
*
*******************************************************************************/
static void gpu_irq_handler ( void )
{
    Cy_GFXSS_Clear_GPU_Interrupt(base, &gfx_context);
    vg_lite_IRQHandler();
}

/*******************************************************************************
* Function Name: update_box_data
*******************************************************************************
*
* Summary:
*  Updates and draws bounding boxes on the render target based on object detection
*  predictions. Scales bounding box coordinates to display dimensions, sets colors
*  based on class IDs, and draws rectangles and labels for each detected object.
*
* Parameters:
*  render_target - Pointer to the vg_lite_buffer_t structure for rendering.
*  prediction   - Pointer to the prediction_od_t structure containing object
*                 detection results, including bounding box coordinates, class IDs,
*                 and confidence scores.
*
* Return:
*  None
*
*
*******************************************************************************/
void update_box_data(vg_lite_buffer_t *render_target, prediction_od_t *prediction)
{
    for (int32_t i = 0; i < prediction->count; i++) {
        int32_t jj = i << 2;
        int32_t id = prediction->class_id[i];
        int32_t cid = (id >= 0) ? (id % 3) + 1 : 0;

        /* Scale and offset bounding box coordinates */
        uint32_t xmin = (uint32_t)(prediction->bbox_int16[jj] * scale_cam_to_disp) + display_offset_x;
        uint32_t ymin = (uint32_t)(prediction->bbox_int16[jj + 1] * scale_cam_to_disp) + display_offset_y;
        uint32_t xmax = (uint32_t)(prediction->bbox_int16[jj + 2] * scale_cam_to_disp) + display_offset_x;
        uint32_t ymax = (uint32_t)(prediction->bbox_int16[jj + 3] * scale_cam_to_disp) + display_offset_y;

        // Set foreground color based on class ID
        if (cid == 3) {
            ifx_lcd_set_FGcolor(8, 24, 168); // Blue
        } else if (cid == 1) {
            ifx_lcd_set_FGcolor(0, 0, 0); // Black
        } else if (cid == 2) {
            ifx_lcd_set_FGcolor(227, 66, 24); // Red
        }

        // Draw rectangle for the bounding box
        ifx_lcd_draw_Rect(xmin, ymin, xmax, ymax);

        // Draw label with class name and confidence score for valid class IDs
        if (id >= 0 && cid < 4) {
            ifx_set_bg_color((color_r[cid] << 16) | (color_g[cid] << 8) | color_b[cid]);
            ifx_print_to_buffer(xmin + 8, ymin - 36, "%s, %.2f", prediction->class_string[i], prediction->conf[i] * 100.0f);
            printf("------------------------------------------------\r\n");
            printf("%s \r\n", prediction->class_string[i]);
#ifdef RPS_DEMO_MODE_ENABLED
            if (id == 0)
            {
                Cy_GPIO_Write(CYBSP_LED_RGB_GREEN_PORT, CYBSP_LED_RGB_GREEN_PIN, 1);
                Cy_SysLib_Delay(LED_BLINK_DELAY_MS);
                Cy_GPIO_Write(CYBSP_LED_RGB_GREEN_PORT, CYBSP_LED_RGB_GREEN_PIN, 0);
            }
            else if (id == 1)
            {
                Cy_GPIO_Write(CYBSP_LED_RGB_BLUE_PORT, CYBSP_LED_RGB_BLUE_PIN, 1);
                Cy_SysLib_Delay(LED_BLINK_DELAY_MS);
                Cy_GPIO_Write(CYBSP_LED_RGB_BLUE_PORT, CYBSP_LED_RGB_BLUE_PIN, 0);
            }
            else
            {
                Cy_GPIO_Write(CYBSP_LED_RGB_RED_PORT, CYBSP_LED_RGB_RED_PIN, 1);
                Cy_SysLib_Delay(LED_BLINK_DELAY_MS);
                Cy_GPIO_Write(CYBSP_LED_RGB_RED_PORT, CYBSP_LED_RGB_RED_PIN, 0);
            }
#endif

        }

        // Render the buffer
        ifx_draw_buffer(render_target->memory);
    }
}

/*******************************************************************************
* Function Name: update_box_data1
*******************************************************************************
*
* Summary:
*  Displays model inference time on the render target at a fixed position. Sets
*  the background color using predefined RGB values and prints the inference time
*  with a label.
*
* Parameters:
*  render_target - Pointer to the vg_lite_buffer_t structure for rendering.
*  num          - Floating-point value representing the model inference time in
*                 milliseconds.
*
* Return:
*  None
*
*
*******************************************************************************/
void update_box_data1(vg_lite_buffer_t *render_target, float num)
{
    ifx_set_bg_color((color_r[3] << 16) | (color_g[3] << 8) | color_b[3]); // Set background color
    ifx_print_to_buffer(10, 440, "%s %.1f %s", "Model", num, "ms");         // Print inference time
    ifx_draw_buffer(render_target->memory);                                   // Render the buffer
}


/*******************************************************************************
* Function Name: VG_LITE_ERROR_EXIT
*******************************************************************************
*
* Summary:
*  Handles VGLite error conditions by printing an error message with the status
*  code, calling the cleanup function to deallocate resources, and triggering an
*  assertion to halt execution.
*
* Parameters:
*  msg           - Pointer to a character string describing the error context.
*  vglite_status - VGLite error code indicating the specific error condition.
*
* Return:
*  None
*
*
*******************************************************************************/
void VG_LITE_ERROR_EXIT(char *msg, vg_lite_error_t vglite_status)
{
    printf("%s %d\r\n", msg, vglite_status); // Print error message and status
    cleanup();                               // Deallocate VGLite resources
    CY_ASSERT(0);                            // Halt execution
}

/*******************************************************************************
* Function Name: disp_i2c_controller_interrupt
********************************************************************************
* Summary:
*  I2C controller ISR which invokes Cy_SCB_I2C_Interrupt to perform I2C transfer
*  as controller.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void i2c_controller_interrupt(void)
{
    Cy_SCB_I2C_Interrupt(DISPLAY_I2C_CONTROLLER_HW, &i2c_controller_context);
}

/*******************************************************************************
* Function Name: VG_switch_frame
*******************************************************************************
*
* Summary:
*  Switches the video/graphics layer frame buffer and manages buffer swapping for
*  display rendering. Signals the USB semaphore based on camera enable status.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void VG_switch_frame(void)
{
    /* Set Video/Graphics layer buffer address and transfer the frame buffer to DC */
    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t*)render_target->address, &gfx_context);
    __DMB();

#ifdef USE_USB_CAM
    static int current_buffer = 0;
    /* Swap buffers */
    current_buffer ^= 1;
    render_target = &display_buffer[current_buffer];

    __DMB();

    if (!logitech_camera_enabled)
    {
        cy_rtos_semaphore_set(&usb_semaphore);
    }
    else
    {
        cy_rslt_t result = cy_rtos_semaphore_get(&usb_semaphore, 0xFFFFFFFF);
        if (CY_RSLT_SUCCESS != result)
        {
            printf("[USB Camera] USB Semaphore set failed\r\n");
        }
    }
#endif
}

/*******************************************************************************
* Function Name: init_buffer_system
*******************************************************************************
*
* Summary:
*  Initializes the buffer system by resetting image buffer states, clearing buffer
*  counters, and resetting timeout tracking variables.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
#ifdef USE_USB_CAM
static void init_buffer_system(void)
{
    // Ensure clean startup state
    for (int i = 0; i < NUM_IMAGE_BUFFERS; i++)
    {
        _image_buff[i].buff_ready = 0;
        _image_buff[i].num_bytes = 0;
    }
    __DMB();

    last_buffer = 0;
    __DMB();

    // Reset timeout tracking
    last_successful_frame_time = 0;
    recovery_attempts = 0;
}
#endif

/*******************************************************************************
* Function Name: cm55_ns_gfx_task
*******************************************************************************
*
* Summary:
*  Initializes and manages the graphics subsystem, including display controller,
*  I2C, VGLite, and buffer systems. Handles rendering of camera frames or fallback
*  images based on device connection status, updates display with bounding box data,
*  and manages frame buffer swapping.
*
* Parameters:
*  arg - Unused task argument
*
* Return:
*  None
*
*******************************************************************************/
void cm55_ns_gfx_task(void *arg)
{
    CY_UNUSED_PARAMETER(arg);
    vg_lite_error_t vglite_status = VG_LITE_SUCCESS;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_en_gfx_status_t status = CY_GFX_SUCCESS;
    volatile float time_start = 0;
    volatile float time_prev = 0;
    volatile float time_end = 0;
    (void)time_prev;
    (void)time_end;
    cy_en_sysint_status_t sysint_status = CY_SYSINT_SUCCESS;
    cy_en_scb_i2c_status_t i2c_result = CY_SCB_I2C_SUCCESS;

    /* Set frame buffer address to the GFXSS configuration structure */
    GFXSS_config.dc_cfg->gfx_layer_config->buffer_address = (gctADDRESS *)vglite_heap_base;
    GFXSS_config.dc_cfg->gfx_layer_config->uv_buffer_address = (gctADDRESS *)vglite_heap_base;

    /* Initialize the graphics subsystem according to the configuration */
    status = Cy_GFXSS_Init(base, &GFXSS_config, &gfx_context);
    if (CY_GFX_SUCCESS != status)
    {
        printf("Graphics subsystem initialization failed: Cy_GFXSS_Init() returned error %d\r\n", status);
        CY_ASSERT(0);
    }

    mtb_hal_syspm_lock_deepsleep();

    /* Setup Display Controller interrupt */
    sysint_status = Cy_SysInt_Init(&dc_irq_cfg, dc_irq_handler);
    if (CY_SYSINT_SUCCESS != sysint_status)
    {
        printf("Error in registering DC interrupt: %d\r\n", sysint_status);
        CY_ASSERT(0);
    }
    NVIC_EnableIRQ(GFXSS_DC_IRQ);
    Cy_GFXSS_Clear_DC_Interrupt(base, &gfx_context);

    /* Initialize GFX GPU interrupt */
    sysint_status = Cy_SysInt_Init(&gpu_irq_cfg, gpu_irq_handler);
    if (CY_SYSINT_SUCCESS != sysint_status)
    {
        printf("Error in registering GPU interrupt: %d\r\n", sysint_status);
        CY_ASSERT(0);
    }
    Cy_GFXSS_Enable_GPU_Interrupt(base);
    NVIC_EnableIRQ(GFXSS_GPU_IRQ);

    /* Initialize the I2C in controller mode */
    i2c_result = Cy_SCB_I2C_Init(DISPLAY_I2C_CONTROLLER_HW, &DISPLAY_I2C_CONTROLLER_config, &i2c_controller_context);
    if (CY_SCB_I2C_SUCCESS != i2c_result)
    {
        printf("I2C controller initialization failed !!\n");
    }

    /* Initialize the I2C interrupt */
    sysint_status = Cy_SysInt_Init(&i2c_controller_irq_cfg, &i2c_controller_interrupt);
    if (CY_SYSINT_SUCCESS != sysint_status)
    {
        printf("I2C controller interrupt initialization failed\r\n");
    }
    NVIC_EnableIRQ(i2c_controller_irq_cfg.intrSrc);
    Cy_SCB_I2C_Enable(DISPLAY_I2C_CONTROLLER_HW);
    
    /* Allow I2C to be stabalized to initialize the display */
    Cy_SysLib_Delay(200);

    /* Initialize Waveshare 4.3-Inch display */
    i2c_result = mtb_disp_waveshare_4p3_init(DISPLAY_I2C_CONTROLLER_HW, &i2c_controller_context);
    if (CY_SCB_I2C_SUCCESS != i2c_result)
    {
        printf("Waveshare 4.3-Inch display init failed with status = %u\r\n", (unsigned int)i2c_result);
    }

    /* Initialize VGLite memory parameters */
    vg_module_parameters_t vg_params;
    vg_params.register_mem_base = (uint32_t)GFXSS_GFXSS_GPU_GCNANO;
    vg_params.gpu_mem_base[0] = GPU_MEM_BASE;
    vg_params.contiguous_mem_base[0] = (volatile void *)vglite_heap_base;
    vg_params.contiguous_mem_size[0] = VGLITE_HEAP_SIZE;

    /* Initialize VGLite */
    vg_lite_init_mem(&vg_params);
    vglite_status = vg_lite_init(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (VG_LITE_SUCCESS != vglite_status)
    {
        VG_LITE_ERROR_EXIT("vg_lite engine init failed: vg_lite_init() returned error %d\r\n", vglite_status);
    }

    /* Setup double display frame buffers */
    for (int32_t ii = 0; ii < 2; ii++)
    {
        display_buffer[ii].width = DISPLAY_WIDTH;
        display_buffer[ii].height = DISPLAY_HEIGHT;
        display_buffer[ii].format = VG_LITE_BGR565;
        vglite_status = vg_lite_allocate(&display_buffer[ii]);
        if (VG_LITE_SUCCESS != vglite_status)
        {
            VG_LITE_ERROR_EXIT("display_buffer[] allocation failed in vglite space: vg_lite_allocate() returned error %d\r\n", vglite_status);
        }
    }
    render_target = &display_buffer[0];

#ifdef USE_USB_CAM
    /* Allocate the camera buffers */
    for (int32_t i = 0; i < NUM_IMAGE_BUFFERS; i++)
    {
        usb_yuv_frames[i].width = CAMERA_WIDTH;
        usb_yuv_frames[i].height = CAMERA_HEIGHT;
        usb_yuv_frames[i].format = VG_LITE_YUYV;
        usb_yuv_frames[i].image_mode = VG_LITE_NORMAL_IMAGE_MODE;
        vglite_status = vg_lite_allocate(&usb_yuv_frames[i]);
        if (VG_LITE_SUCCESS != vglite_status)
        {
            VG_LITE_ERROR_EXIT("USB camera buffers allocation failed in vglite space: vg_lite_allocate() returned error %d\r\n", vglite_status);
        }
    }
#endif
#ifdef USE_DVP_CAM
    for ( uint8_t i = 0; i < NUM_IMAGE_BUFFERS; i++ )
    {
        dvp_bgr565_frames[i].width = CAMERA_WIDTH;
        dvp_bgr565_frames[i].height = CAMERA_HEIGHT;
        dvp_bgr565_frames[i].format = VG_LITE_BGR565;
        dvp_bgr565_frames[i].image_mode = VG_LITE_NORMAL_IMAGE_MODE;
        vglite_status = vg_lite_allocate(&dvp_bgr565_frames[i]);
        if (VG_LITE_SUCCESS != vglite_status)
        {
            VG_LITE_ERROR_EXIT("DVP camera buffers allocation failed in vglite space: vg_lite_allocate() returned error %d\r\n", vglite_status);
        }
    }
#endif
    /* Allocate the work camera buffer */
    bgr565.width = CAMERA_WIDTH;
    bgr565.height = CAMERA_HEIGHT;
    bgr565.format = VG_LITE_BGR565;
    bgr565.image_mode = VG_LITE_NORMAL_IMAGE_MODE;
    vglite_status = vg_lite_allocate(&bgr565);
    if (VG_LITE_SUCCESS != vglite_status)
    {
        VG_LITE_ERROR_EXIT("work camera image bgr565 allocation failed in vglite space: vg_lite_allocate() returned error %d\r\n", vglite_status);
    }

    /* Clear the buffer with black color */
    vglite_status = vg_lite_clear(render_target, NULL, BLACK_COLOR);
    if (VG_LITE_SUCCESS != vglite_status)
    {
        VG_LITE_ERROR_EXIT("Clear failed: vg_lite_clear() returned error %d\r\n", vglite_status);
    }

    /* Define the transformation matrix for camera image to display */
    vg_lite_identity(&matrix);
    float scale_cam_to_disp_x = (float)(DISPLAY_WIDTH) / (float)CAMERA_WIDTH;
    float scale_cam_to_disp_y = (float)(DISPLAY_HEIGHT) / (float)CAMERA_HEIGHT;
    scale_cam_to_disp = max(scale_cam_to_disp_x, scale_cam_to_disp_y);
    vg_lite_scale(scale_cam_to_disp, scale_cam_to_disp, &matrix);

    /* Move the scaled frame to the display center */
    float translate_x = ((DISPLAY_WIDTH) / scale_cam_to_disp - CAMERA_WIDTH) * 0.5f;
    float translate_y = (DISPLAY_HEIGHT / scale_cam_to_disp - CAMERA_HEIGHT) * 0.5f;
    vg_lite_translate(translate_x, translate_y, &matrix);

    display_offset_x = ((DISPLAY_WIDTH) - scale_cam_to_disp * IMAGE_WIDTH) / 2;
    display_offset_y = (DISPLAY_HEIGHT - scale_cam_to_disp * CAMERA_HEIGHT) / 2;

    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t *)render_target->address, &gfx_context);
    vg_lite_flush();
    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t *)render_target->address, &gfx_context);

    /* Delay for USB enumeration to complete before rendering */
    vTaskDelay(pdMS_TO_TICKS(1500));

    /* Initialize buffer states - clear any startup artifacts */
#ifdef USE_USB_CAM
    for (int i = 0; i < NUM_IMAGE_BUFFERS; i++)
    {
        _image_buff[i].buff_ready = 0;
        _image_buff[i].num_bytes = 0;
    }
    __DMB();

    init_buffer_system();
#endif
#ifdef USE_DVP_CAM
    /* DVP camera requires the buffers to be initialized and formatted as per desired configurations */
    result = cy_rtos_thread_create( &inference_thread, &cm55_inference_task, INFERENCE_TASK_NAME, NULL,
                                    INFERENCE_TASK_STACK_SIZE, INFERENCE_TASK_PRIORITY, NULL);
    if ( CY_RSLT_SUCCESS != result ) {
        CY_ASSERT(0);
    }
#endif
    for (;;)
    {
#ifdef USE_USB_CAM
        if (!_device_connected)
        {
            int i = 0;
            while (i < 2)
            {
                i++;
                __DMB();
                vg_lite_finish();
                Cy_GFXSS_Set_FrameBuffer(base, (uint32_t *)render_target->address, &gfx_context);
                __DMB();

                /* Clear the buffer with black color */
                vglite_status = vg_lite_clear(render_target, NULL, BLACK_COLOR);
                if (VG_LITE_SUCCESS != vglite_status)
                {
                    VG_LITE_ERROR_EXIT("Clear failed: vg_lite_clear() returned error %d\r\n", vglite_status);
                }
                __DMB();

                ifx_lcd_display_Rect(NO_CAMERA_IMG_X_POS, NO_CAMERA_IMG_Y_POS, (uint8_t *)no_camera_img_map, NO_CAMERA_IMG_WIDTH, NO_CAMERA_IMG_HEIGHT);

                Cy_GFXSS_Set_FrameBuffer(base, (uint32_t *)render_target->address, &gfx_context);
                __DMB();
            }
        }
        else
        {
            if (Camera_not_supported)
            {
                int i = 0;
                while (i < 2)
                {
                    i++;
                    __DMB();
                    vg_lite_finish();
                    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t *)render_target->address, &gfx_context);
                    __DMB();

                    /* Clear the buffer with black color */
                    vglite_status = vg_lite_clear(render_target, NULL, BLACK_COLOR);
                    if (VG_LITE_SUCCESS != vglite_status)
                    {
                        VG_LITE_ERROR_EXIT("Clear failed: vg_lite_clear() returned error %d\r\n", vglite_status);
                    }
                    __DMB();

                    ifx_lcd_display_Rect(CAMERA_NOT_SUPPORTED_IMG_X_POS, CAMERA_NOT_SUPPORTED_IMG_Y_POS, (uint8_t *)camera_not_supported_img_map, CAMERA_NOT_SUPPORTED_IMG_WIDTH, CAMERA_NOT_SUPPORTED_IMG_HEIGHT);

                    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t *)render_target->address, &gfx_context);
                    __DMB();
                }
                while (_device_connected);
            }
        }
#endif
        result = cy_rtos_semaphore_get(&model_semaphore, 0xFFFFFFFF);
        if (CY_RSLT_SUCCESS == result)
        {
#ifdef USE_USB_CAM
            if (_device_connected)
#endif
            {
                /* Update bounding box data and inference time */
                update_box_data(render_target, &prediction);
                update_box_data1(render_target, inference_time);

                VG_switch_frame();

                time_end = ifx_time_get_ms_f();
                time_prev = time_start;
            }
        }
    }
}

