/*****************************************************************************
* \file dataset_capture.h
*
* \brief
* OV7675 dataset capture mode (build with DATASET_CAPTURE_MODE=1).
*
* Streams the exact 160x120 grayscale frame the model sees (the output of
* preprocess_bgr565_to_gray_i8(), converted back to uint8 = q + 128) over the
* debug UART (KitProg3 USB-UART). PC side: code/PSE84E_Project/NPU/capture_dataset.py
*
* Frame (little-endian), 24 B header + payload + 4 B CRC:
*    0  magic          "DSF1"
*    4  frame_counter  u32   frames sent since boot (gaps = frames lost on the PC)
*    8  timestamp_ms   u32   ifx_time_get_ms_f() at capture
*   12  width          u16   160
*   14  height         u16   120
*   16  payload_len    u32   width * height (19200)
*   20  conf_milli     u16   model human probability for this frame, x1000
*   22  reserved       u16   0
*   24  payload        u8[payload_len], row-major gray 0..255
*   ..  crc32          u32   CRC-32 (zlib/IEEE) over bytes 4 .. end of payload
*
* Controls (KIT_PSE84_AI): SW1 short press = capture one frame,
* SW1 long press (>= 1 s) = toggle continuous capture every
* DATASET_CAPTURE_INTERVAL_MS. LED1 (green) blinks per frame sent,
* LED2 (blue) is on while continuous capture is active.
*
*******************************************************************************/
#ifndef _PROJ_CM55_DATASET_CAPTURE_H_
#define _PROJ_CM55_DATASET_CAPTURE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef DATASET_CAPTURE_MODE
#define DATASET_CAPTURE_MODE            (0)
#endif

/* Debug UART baud in capture mode; the serial terminal must use the same value */
#ifndef DATASET_CAPTURE_BAUD
#define DATASET_CAPTURE_BAUD            (921600UL)
#endif

/* Continuous capture period; the collection plan asks for 1-2 fps */
#ifndef DATASET_CAPTURE_INTERVAL_MS
#define DATASET_CAPTURE_INTERVAL_MS     (500U)
#endif

/* 1: continuous capture starts at boot without pressing SW1 */
#ifndef DATASET_CAPTURE_AUTOSTART
#define DATASET_CAPTURE_AUTOSTART       (0)
#endif

#define DATASET_CAPTURE_TASK_NAME       ("CM55 Dataset Capture Task")
#define DATASET_CAPTURE_TASK_STACK_SIZE (4U * 1024U)
/* Same as the H7 link task: above the busy-waiting inference task */
#define DATASET_CAPTURE_TASK_PRIORITY   (configMAX_PRIORITIES - 3)

#if DATASET_CAPTURE_MODE
/* Starts the capture task and switches the debug UART to DATASET_CAPTURE_BAUD */
void dataset_capture_start(void);
/* Called by the inference task after each inference with the int8 model input
 * (q = gray - 128). Copies the frame only when a capture is due; cheap otherwise. */
void dataset_capture_offer(const int8_t *model_input, uint16_t width, uint16_t height,
                           float conf_human);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _PROJ_CM55_DATASET_CAPTURE_H_ */
