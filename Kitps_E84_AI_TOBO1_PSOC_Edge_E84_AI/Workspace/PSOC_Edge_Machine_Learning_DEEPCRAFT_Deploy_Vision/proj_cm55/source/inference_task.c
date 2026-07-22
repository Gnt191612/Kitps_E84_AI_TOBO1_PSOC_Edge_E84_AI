 /******************************************************************************
* File Name: inference_task.c
*
* Description: This file contains API calls to inference task.
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

/*******************************************************************************
* Header File
*******************************************************************************/
#include <math.h>
#include <inttypes.h>
#include <stdint.h>
#include "cybsp.h"
#include "cy_pdl.h"
#include "cyabs_rtos.h"
#include "cyabs_rtos_impl.h"
/* FreeRTOS header file */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "FreeRTOSConfig.h"
#include "stdlib.h"
#include "model.h"
#include "ifx_image_utils.h"
#include "lcd_task.h"
#include "inference_task.h"
#include "ifx_time_utils.h"
#include "mtb_ml_utils.h"
#include "mtb_ml_common.h"
#include "mtb_ml.h"

/*******************************************************************************
 * Global Variable
 *******************************************************************************/
/* Performance measure: Buffer wait time */ 
float prep_wait_buf; 
/* Performance measure: YUV422 to BGR565 conversion time */              
float prep_YUV422_to_bgr565; 
/* Performance measure: BGR565 to display time */      
float prep_bgr565_to_disp;   
/* Performance measure: RGB565 to RGB888 conversion time */      
float prep_RGB565_to_RGB888; 
      
#ifdef USE_DVP_CAM
extern bool frame_ready;
#endif

__attribute__((section(".cy_socmem_data")))
/* Final output variable for object detection */
__attribute__((aligned(16))) prediction_od_t prediction; 
float data_out[IMAI_DATAOUT_COUNT] = {0};
/* Inference execution time */
volatile float inference_time = 0;  
/* The best class out of all i.e. the class with max value out of all classes */
float max_class_val = 0;

/*******************************************************************************
 * Function Name: get_image
 *
 * Description:
 *   Retrieves the latest image by calling the draw function.
 *
 * Input Arguments:
 *   None
 *
 * Return Value:
 *   uint8_t* - Pointer to the image buffer
 *
 *******************************************************************************/
static uint8_t * get_image()
{
    return draw();
}

/*******************************************************************************
* Function Name: get_best_class
*******************************************************************************
*
* Summary:
*  The function calculates the best class out of all available classes.
*
* Parameters:
*  cls  - Pointer to the class array.
*  size - number of classes for the model i.e. size of class array.
*  max_cls_val - pointer to store the the best class out of all i.e. 
*                the max of all classes
*
* Return:
*  max_index - Returns the idex of the best class out of all i.e. 
*              the max of all classes
*
*******************************************************************************/
int8_t get_best_class(const float *cls, size_t size, float *max_cls_val) {
    if (cls == NULL || size == 0) {
    /* Array is empty */
        return -1;
    }

    int8_t max_index = 0;
    float max_value = cls[0];

    for (int8_t i = 1; i < size; i++) {
        if (cls[i] > max_value) {
            max_value = cls[i];
            max_index = i;
        }
    }

    *max_cls_val = max_value;
    return max_index;
}

/*******************************************************************************
 * Function Name: cm55_inference_task
 *
 * Description:
 *   Main task for running object detection inference on the CM55 core. Initializes
 *   the object detection model, preprocesses input images, performs inference, and
 *   postprocesses results. Updates performance metrics and signals the graphics
 *   display semaphore.
 *
 * Input Arguments:
 *   void *arg - Task argument (not used)
 *
 * Return Value:
 *   None
 *
 *******************************************************************************/
void cm55_inference_task( void *arg )
{
    int result = IMAI_init();
    IMAI_api_def *api_def = IMAI_api();
    float class[MAX_PREDICTIONS][NUM_CLASSES] = {{0.0}};
    volatile float _time_start_prev = 0;
    (void)_time_start_prev;

    int max_predictions = api_def->func_list[0].param_list[1].shape[0].size;
    int detection_flag_index = api_def->func_list[0].param_list[1].shape[1].size -1;

    if(result != 0)
        printf("Failed to initialize the model\r\n");

    while (true)
    {
#ifdef USE_DVP_CAM
        if (frame_ready == true)
#endif
        {
            if(NUM_CLASSES != api_def->func_list[0].param_list[1].shape[1].size - 5)
            {
                printf("The number of classes configured is wrong. Please update the NUM_CLASSES "
                        "to %d.\r\n", api_def->func_list[0].param_list[1].shape[1].size - 5);
                CY_ASSERT(0);
            }
            volatile float _time_start = ifx_time_get_ms_f();
#ifdef USE_DVP_CAM
            frame_ready = false;
#endif
            /* Get the latest input image to the input image buffer. */ 
            uint8_t *image_buf_uint8 = get_image();
            cy_rtos_delay_milliseconds(1);

            /* Object detection */ 
            volatile float _time_object_det = ifx_time_get_ms_f();

            IMAI_compute(image_buf_uint8, data_out);
            /*  Bounding boxes in integer in original image size [Max_detections * 4]: [xmin, ymin, xmax, ymax]*/
            int16_t *bbox_int16 = prediction.bbox_int16;   
            /*Confidence scores  */  
            float *conf = prediction.conf;    
            /* Class ID from the predictions*/       
            uint8_t *class_id = prediction.class_id;

            int32_t actual_nr_predictions = 0;

            for (int r = 0; r < max_predictions; r++) {

                if(data_out[detection_flag_index * max_predictions + r] == 0)
                    break;
                char *   class_string  = prediction.class_string[r];
                actual_nr_predictions++;

                /* Buffer layout is attribute-major: [x0..x4, y0..y4, w0..w4, h0..h4, cls0_0..cls0_4, cls1_0..cls1_4, cls2_0..cls2_4] */
                float x = data_out[0 * max_predictions + r];
                float y = data_out[1 * max_predictions + r];
                float w = data_out[2 * max_predictions + r];
                float h = data_out[3 * max_predictions + r];

                for(int cls_no = 0; cls_no < NUM_CLASSES; cls_no++)
                {
                    class[r][cls_no] = data_out[(cls_no + 4) * max_predictions + r];
                }

                class_id[r] =  get_best_class(class[r], NUM_CLASSES, &max_class_val);
                memcpy(class_string, api_def->func_list[0].param_list[1].shape[1].labels[class_id[r] + 4], MAX_CLASS_LEN);
                conf[r] = max_class_val;

                *bbox_int16++= (int16_t)((x - HALF(w)) * IMAGE_WIDTH + RND_F2I_FACTOR);
                *bbox_int16++= (int16_t)((y - HALF(h)) * IMAGE_WIDTH + RND_F2I_FACTOR);
                *bbox_int16++= (int16_t)((x + HALF(w)) * IMAGE_WIDTH + RND_F2I_FACTOR);
                *bbox_int16++= (int16_t)((y + HALF(h)) * IMAGE_WIDTH + RND_F2I_FACTOR);

            }

            prediction.count = actual_nr_predictions;
            volatile float _time_end = ifx_time_get_ms_f();

            inference_time = _time_end - _time_object_det;

            result = cy_rtos_semaphore_set(&model_semaphore);
            if (CY_RSLT_SUCCESS != result) {
                printf("\r\nModel Semphore set failed\r\n");
            }

            _time_start_prev = _time_start;
        }
    }
}
