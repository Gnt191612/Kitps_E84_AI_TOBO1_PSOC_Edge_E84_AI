/*******************************************************************************
* File Name        : ifx_image_utils.h
*
* Description      : This is the header file of image utility functions.
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

#ifndef _IFX_IMAGE_UTILS_H_
#define _IFX_IMAGE_UTILS_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdint.h>

#include "dsp/basic_math_functions.h"
#include "arm_nnsupportfunctions.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#ifndef INDEX3D
#define INDEX3D(y, x, c, width, channel)    (((y) * (width) + (x)) * (channel) + (c))
#define INDEX2D(y, x, width)                 ((y) * (width) + (x))
#endif  /* INDEX3D */

#ifndef max
#define    max(a, b)    ((a) > (b) ? (a) : (b))
#define    min(a, b)    ((a) < (b) ? (a) : (b))
#endif  /* max */

#ifndef swap
#define swap(a, b, t)   {t = a; a = b; b = t;}
#endif  /* swap */

#ifndef memset
#define memset(d, v, sz)    arm_memset_q7((q7_t *)(d), (q7_t)(v), (sz))
#endif

#ifndef memcpy
#define memcpy(d, s, sz)    arm_memcpy_q7((q7_t *)(d), (q7_t *)(s), (sz))
#endif

/******************************************************************************
* Function prototype
******************************************************************************/
uint16_t ifx_pixel_RGB888_to_RGB565( uint32_t r, uint32_t g, uint32_t b );
uint32_t ifx_pixel_RGB888_to_RGBX32( uint32_t r, uint32_t g, uint32_t b );
uint16_t ifx_pixel_xRGB32_to_RGB565( uint32_t xRGB );
uint8_t * ifx_pixel_RGB565_to_RGB888( uint32_t rgb565, uint8_t *rgb888 );
uint8_t * ifx_pixel_2_RGB565_to_RGB888( uint32_t rgb565, uint8_t *rgb888 );
uint32_t ifx_pixel_RGB565_to_RGB888_value( uint32_t rgb565 );

void ifx_image_conv_RGB565_to_RGB888( uint8_t *bgr565, int32_t width, int32_t height,
        uint8_t *rgb888, int32_t dst_w, int32_t dst_h );
void ifx_image_conv_RGB888_to_RGB565(uint8_t *src_rgb888, int32_t width, int32_t height,
        uint8_t *dst_bgr565, int32_t dst_width, int32_t dst_height);
void ifx_image_conv_RGB565_to_RGB888_u2i( uint8_t *bgr565, int32_t width, int32_t height,
        int8_t *rgb888_i8, int32_t dst_w, int32_t dst_h, int32_t zero_point );
void ifx_image_conv_RGB565_to_RGB888_quant( uint8_t *bgr565, int32_t width, int32_t height,
        int8_t *rgb888_i8, int32_t dst_width, int32_t dst_height,
        float scale, int32_t zero_point );
uint8_t * ifx_RGB565_to_RGB888_i8( uint32_t pixel, uint8_t *rgb888_8p );
void ifx_image_conv_RGB565_to_RGB888_i8( uint8_t *src_bgr565, int width, int height,
        uint8_t *dst_rgb888_i8, int dst_width, int dst_height );
void ifx_image_conv_RGBX32_to_RGB24( uint8_t *src_rgbx32, int32_t width, int32_t height,
        uint8_t *dst_rgb888, int32_t dst_width, int32_t dst_height );
void ifx_image_conv_RGBX32_to_RGB24_u2i( uint8_t *src_rgbx32, int32_t width, int32_t height,
        int8_t  *dst_rgb888, int32_t dst_width, int32_t dst_height, int32_t zero_point );
void ifx_image_resize_Ratio( uint8_t *srcImage, int32_t src_w, int32_t src_h, int32_t src_ch,
        uint8_t *dstImage, int32_t dst_w, int32_t dst_h,
        float *scaledWidth, float *scaledHeight );
void ifx_image_resize_Ratio_i8( int8_t *srcImage, int32_t src_w, int32_t src_h, int32_t src_ch,
        int8_t *dstImage, int32_t dst_w, int32_t dst_h,
        float *scaledWidth, float *scaledHeight );
void ifx_image_resize_Ratio_u2i( uint8_t *srcImage, int32_t src_w, int32_t src_h, int32_t src_ch,
        int8_t  *dstImage, int32_t dst_w, int32_t dst_h,
        float *scaledWidth, float *scaledHeight, int32_t zero_point );
void ifx_image_resize_RGB565_to_RGB888_Ratio( uint16_t* srcImage_RGB565, int srcWidth, int srcHeight, int channels,
        uint8_t* dstImage_RGB888, int dstWidth, int dstHeight,
        float *scaledWidth, float *scaledHeight );
void ifx_image_resize_Rect( int8_t * dstImage, int dst_w, int dst_h, int dst_c,
        int8_t * srcImage, int img_w, int img_h,
        float xmin, float ymin, float xmax, float ymax );
void ifx_image_resize_Rect_i8( int8_t *srcImage, int32_t src_w, int32_t src_h, int32_t src_ch,
        int8_t *dstImage, int32_t dst_w, int32_t dst_h,
        float xmin, float ymin, float xmax, float ymax );
void ifx_image_resize_Rect_u2i( uint8_t *srcImage, int32_t src_w, int32_t src_h, int32_t src_ch,
        int8_t  *dstImage, int32_t dst_w, int32_t dst_h,
        float xmin, float ymin, float xmax, float ymax, int32_t zero_point );
void ifx_image_resize_Rect_Linear( uint8_t *srcImage, int32_t src_w, int32_t src_h, int32_t src_ch,
        uint8_t *dstImage, int32_t dst_w, int32_t dst_h,
        float xmin, float ymin, float xmax, float ymax );
void ifx_image_resize_Rect_Linear_i8( int8_t * dstImage, int dst_w, int dst_h, int dst_c,
        int8_t * srcImage, int img_w, int img_h,
        float xmin, float ymin, float xmax, float ymax );
void ifx_image_resize_Rect_Linear_u2i( uint8_t *srcImage, int32_t src_w, int32_t src_h, int32_t src_ch,
        int8_t  *dstImage, int32_t dst_w, int32_t dst_h,
        float xmin, float ymin, float xmax, float ymax, int32_t zero_point );
void ifx_warpAffine( float mtx[][3], float *pt, float *out );
void ifx_image_resize_Matrix_u2i( uint8_t *image, int imgWidth, int imgHeight, int imgCh,
        int8_t *subImage, int dstWidth, int dstHeight,
        float warpMtx[][3], int zero_point );
void ifx_image_extract( uint8_t *srcImage, int32_t src_w, int32_t src_h,  int32_t src_ch,
        uint8_t *dstImage, int32_t dst_w, int32_t dst_h );
void ifx_image_extract_i8( int8_t *srcImage, int32_t src_w, int32_t src_h, int32_t src_ch,
        int8_t *dstImage, int32_t dst_w, int32_t dst_h );
void ifx_image_extract_u2i( uint8_t *srcImage, int32_t src_w, int32_t src_h, int32_t src_ch,
        int8_t  *dstImage, int32_t dst_w, int32_t dst_h, int32_t zero_point );
void ifx_image_extract_Rect( uint8_t * dstImage, int dst_w, int dst_h, int dst_c,
        uint8_t * srcImage, int img_w, int img_h,
        int xmin, int ymin );
void ifx_image_extract_Rect_i8( uint8_t * dstImage, int dst_w, int dst_h, int dst_c,
        uint8_t * srcImage, int img_w, int img_h );
void ifx_image_extract_Rect_u2i( int8_t * dstImage, int dst_w, int dst_h, int dst_c,
        uint8_t * srcImage, int img_w, int img_h );
void draw_image_pixel( uint16_t *pDst, uint16_t x, uint16_t y,
        uint16_t r, uint16_t g, uint16_t b, uint16_t lcd_w, uint16_t lcd_h );
void ifx_image_draw_line_u16( uint16_t* pDst, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t rgb565, uint16_t lcd_w, uint16_t lcd_h );
void ifx_image_draw_pixel_u16( uint16_t *pDst, uint16_t x, uint16_t y, uint16_t rgb565, uint16_t lcd_w );

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* _IFX_IMAGE_UTILS_H_ */

/* [] END OF FILE */