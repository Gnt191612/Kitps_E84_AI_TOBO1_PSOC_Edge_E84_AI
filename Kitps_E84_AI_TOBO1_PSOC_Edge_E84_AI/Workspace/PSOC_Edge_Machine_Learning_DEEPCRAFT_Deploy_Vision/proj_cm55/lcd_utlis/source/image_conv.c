/******************************************************************************
* File Name        : image_conv.c
*
* Description      : This file implements the image format conversions.
*
* Related Document : See README.md
*
*******************************************************************************
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
******************************************************************************/

/******************************************************************************
* Header Files
******************************************************************************/
#include <stdint.h>
#include "cy_syslib.h"

#include "ifx_image_utils.h"

/******************************************************************************
* Macros
******************************************************************************/
/*
 #define    BGR888_FULL_8_BIT
 usual shift-conversion function sets the low 2 or 3 bits in each channel to 0.
 so, the brightest 16-bit color (0xFFFF) becomes the 24-bit color of R=0xF8, G=0xFC, B=0xF8,
 which is not the brightest 24-bit color (0xFFFFFF).
 fill these low 2 or 3 bits by burning more cycles.
 red, blue:   ([0, 0xF8] * 66) >> 6 => ([0, 248] * 66) >> 6 => [0, 16368] >> 6 => ([0, 255.75]) => [0, 255]
 green:       ([0, 0xFC] * 65) >> 6 => ([0, 252] * 65) >> 6 => [0, 16380] >> 6 => ([0, 255.94]) => [0, 255]
 */

/* Get most significant byte */
#define GET_MSB(a, bits, ofs)  ((((a) >> (8 - (bits))) & ((1 << (bits)) - 1)) << (ofs))

/******************************************************************************
* Functions
******************************************************************************/

/******************************************************************************
* Function Name: ifx_pixel_RGB888_to_RGB565
*******************************************************************************
* Summary:
*  Converts a color pixel value from RGB888 format (8 bits per channel) to
*  RGB565 format (16 bits total: 5 bits red, 6 bits green, 5 bits blue).
*
* Parameters:
*  uint8_t r: Red color value (0-255)
*  uint8_t g: Green color value (0-255)
*  uint8_t b: Blue color value (0-255)
*
* Return:
*  uint16_t: 16-bit color value in RGB565 format
*
******************************************************************************/
inline uint16_t ifx_pixel_RGB888_to_RGB565(uint32_t r, uint32_t g, uint32_t b)
{
    b = GET_MSB(b, 5,  0);
    g = GET_MSB(g, 6,  5);
    r = GET_MSB(r, 5, 11);

    return (uint16_t)(r | g | b);
}


/******************************************************************************
* Function Name: ifx_pixel_RGB565_to_RGB888_value
*******************************************************************************
* Summary:
*  Converts a color pixel value from RGB565 format (16 bits: 5 bits red, 6 bits
*  green, 5 bits blue) to RGB888 format (24 bits: 8 bits per channel). Supports
*  multiple conversion modes: default (low 2 or 3 bits set to 0), full 8-bit
*  usage, or full 8-bit usage with rounding.
*
* Parameters:
*  uint16_t rgb565: 16-bit color value in RGB565 format
*
* Return:
*  uint32_t: 24-bit color value in RGB888 format
*
******************************************************************************/
inline uint32_t ifx_pixel_RGB565_to_RGB888_value( uint32_t rgb565 )
{
    /* Extract B, G, and R components from BGR565 pixel (already in BGR order) */
    uint32_t    blue  = (rgb565 << 3) & 0xF8u;      /* [0, 248], Extract lower 5 bits for blue */
    uint32_t    green = (rgb565 >> 3) & 0xFCu;      /* [0, 252], Extract middle 6 bits for green, right shift 5 for 8-bit representation */
    uint32_t    red   = (rgb565 >> 8) & 0xF8u;      /* [0, 248], Extract upper 5 bits for red, right shift 11 for 8-bit representation */

#ifdef BGR888_FULL_8_BIT
    red   = (red   * 66) >> 6;
    green = (green * 65) >> 6;
    blue  = (blue  * 66) >> 6;
#endif  /* BGR888_FULL_8_BIT */

    return (uint32_t)((blue << 16) | (green << 8) | red);   /* image array sequence */
}


/******************************************************************************
* Function Name: ifx_pixel_RGB565_to_RGB888
*******************************************************************************
* Summary:
*  Converts a color pixel value from RGB565 format (16 bits: 5 bits red, 6 
*  bits green, 5 bits blue) to RGB888 format (24 bits: 8 bits per channel) 
*  and stores the result in the provided buffer. Supports multiple conversion
*  modes: default (low 2 or 3 bits set to 0), full 8-bit usage, or full 8-bit 
*  usage with rounding.
*
* Parameters:
*  uint16_t rgb565: 16-bit color value in RGB565 format
*  uint8_t *rgb888: Pointer to the 24-bit color buffer for storing RGB888 
*                   values
*
* Return:
*  uint8_t*: Pointer to the next position in the 24-bit color buffer
*
******************************************************************************/
inline uint8_t * ifx_pixel_RGB565_to_RGB888( uint32_t rgb565, uint8_t *rgb888 )
{
    /* Extract B, G, and R components from BGR565 pixel (already in BGR order) */
    uint32_t    blue  = (rgb565 << 3) & 0xF8u;      /* [0, 248], Extract lower 5 bits for blue */
    uint32_t    green = (rgb565 >> 3) & 0xFCu;      /* [0, 252], Extract middle 6 bits for green, right shift 5 for 8-bit representation */
    uint32_t    red   = (rgb565 >> 8) & 0xF8u;      /* [0, 248], Extract upper 5 bits for red, right shift 11 for 8-bit representation */

#ifdef BGR888_FULL_8_BIT
    red   = (red   * 66) >> 6;
    green = (green * 65) >> 6;
    blue  = (blue  * 66) >> 6;
#endif  /* BGR888_FULL_8_BIT */

    *rgb888++ = (uint8_t)red;
    *rgb888++ = (uint8_t)green;
    *rgb888++ = (uint8_t)blue;

    return rgb888;
}


/******************************************************************************
* Function Name: ifx_pixel_2_RGB565_to_RGB888
*******************************************************************************
* Summary:
*  Converts two RGB565 color pixel values (packed in a 32-bit integer) to 
*  RGB888 format and stores the result in the provided buffer. Supports full 
*  8-bit usage conversion if BGR888_FULL_8_BIT is defined. Processes two 
*  pixels at a time.
*
* Parameters:
*  uint32_t rgb565: 32-bit value containing two RGB565 pixels (low 16-bit: 
*                   first pixel, high 16-bit: second pixel)
*  uint8_t *rgb888: Pointer to the 24-bit RGB888 color buffer for storing 
*                   the result
*
* Return:
*  uint8_t*: Pointer to the next position in the RGB888 color buffer
*
******************************************************************************/
inline uint8_t * ifx_pixel_2_RGB565_to_RGB888( uint32_t rgb565, uint8_t *rgb888 )
{
    /* Extract B, G, and R components from BGR565 pixel (already in BGR order) */
    /* low 16-bit: the first pixel, high 16-bit: the second pixel */
    uint32_t    blue  = (rgb565 << 3) & 0x0F800F8u;     /* xxB1 | xxB0 */
    uint32_t    green = (rgb565 >> 3) & 0x0FC00FCu;     /* xxG1 | xxG0 */
    uint32_t    red   = (rgb565 >> 8) & 0x0F800F8u;     /* xxR1 | xxR0 */

#ifdef BGR888_FULL_8_BIT
    red   = (red   * 66) >> 6;
    green = (green * 65) >> 6;
    blue  = (blue  * 66) >> 6;
#endif  /* BGR888_FULL_8_BIT */

    uint16_t    *rgb888_16p = (uint16_t *)rgb888;
    uint32_t    rg = (green << 8) | red;        /* G1R1 | G0R0* */
    uint32_t    br = (red   >> 8) | blue;       /* xxB1 | R1B0* */
    uint32_t    gb = (blue  << 8) | green;      /* B1G1*| B0G0 */

    *rgb888_16p++ = (uint16_t)rg;
    *rgb888_16p++ = (uint16_t)br;
    *rgb888_16p++ = (uint16_t)(gb >> 16);

    return (uint8_t *)rgb888_16p;
}


/******************************************************************************
* Function Name: ifx_pixel_RGB565_to_RGB888_u2i
*******************************************************************************
* Summary:
*  Converts a single RGB565 color pixel value to RGB888 format with an 
*  optional zero-point offset for int8_t output. Stores the result in the 
*  provided buffer.Supports full 8-bit usage conversion if BGR888_FULL_8_BIT 
*  is defined.
*
* Parameters:
*  uint32_t rgb565: 16-bit RGB565 color value
*  int8_t *rgb888: Pointer to the 24-bit RGB888 color buffer for storing the 
*                   result
*  int32_t zero_point: Zero-point offset to apply to the output values
*
* Return:
*  int8_t*: Pointer to the next position in the RGB888 color buffer
*
******************************************************************************/
inline int8_t * ifx_pixel_RGB565_to_RGB888_u2i( uint32_t rgb565, int8_t *rgb888, int32_t zero_point )
{
    /* Extract B, G, and R components from BGR565 pixel (already in BGR order) */
    int    blue   = (rgb565 << 3) & 0x0F8;         /* Extract lower 5 bits for blue */
    int    green  = (rgb565 >> 3) & 0x0FC;         /* Extract middle 6 bits for green, right shift 5 for 8-bit representation */
    int red    = (rgb565 >> 8) & 0x0F8;            /* Extract upper 5 bits for red, right shift 11 for 8-bit representation */

#ifdef BGR888_FULL_8_BIT
    red   = (red   * 66) >> 6;
    green = (green * 65) >> 6;
    blue  = (blue  * 66) >> 6;
#endif  /* BGR888_FULL_8_BIT */

    *rgb888++ = red   + zero_point;
    *rgb888++ = green + zero_point;
    *rgb888++ = blue  + zero_point;

    return rgb888;
}


/******************************************************************************
* Function Name: ifx_pixel_2_RGB565_to_RGB888_u2i
*******************************************************************************
* Summary:
*  Converts two RGB565 color pixel values (packed in a 32-bit integer) to 
*  RGB888 format with an optional zero-point offset for int8_t output. Stores 
*  the result in the provided buffer. Supports full 8-bit usage conversion if 
*  BGR888_FULL_8_BIT is defined and uses SIMD instructions for optimization if 
*  ARM_MATH_DSP is defined.
*
* Parameters:
*  uint32_t rgb565: 32-bit value containing two RGB565 pixels (low 16-bit: 
*                   first pixel, high 16-bit: second pixel)
*  int8_t *rgb888: Pointer to the 24-bit RGB888 color buffer for storing the 
*                   result
*  int32_t zero_point: Zero-point offset to apply to the output values
*
* Return:
*  int8_t*: Pointer to the next position in the RGB888 color buffer
*
******************************************************************************/
int8_t * ifx_pixel_2_RGB565_to_RGB888_u2i( uint32_t rgb565, int8_t *rgb888, int32_t zero_point )
{
    /* Extract B, G, and R components from BGR565 pixel (already in BGR order) */
    /* low 16-bit: the first pixel, high 16-bit: the second pixel */
    int32_t    blue   = (rgb565 << 3) & 0x0F800F8;     /* Extract lower 5 bits for blue */
    int32_t    green  = (rgb565 >> 3) & 0x0FC00FC;     /* Extract middle 6 bits for green, right shift 5 for 8-bit representation */
    int32_t    red    = (rgb565 >> 8) & 0x0F800F8;     /* Extract upper 5 bits for red, right shift 11 for 8-bit representation */

#ifdef BGR888_FULL_8_BIT
    red   = (red   * 66) >> 6;
    green = (green * 65) >> 6;
    blue  = (blue  * 66) >> 6;
#endif  /* BGR888_FULL_8_BIT */

#if defined (ARM_MATH_DSP)
    int32_t zero_point_packed;      /* Offset packed to 32 bit */

    /* Offset is packed to 32 bit in order to use SIMD32 for addition */
    zero_point_packed = __PKHBT(zero_point, zero_point, 16);
    /* Add offset and store result in destination buffer (2 samples at a time). */
    red   = __QADD16(red,   zero_point_packed);
    green = __QADD16(green, zero_point_packed);
    blue  = __QADD16(blue,  zero_point_packed);

    /* the first pixel */
    *rgb888++ = (int8_t)(red);
    *rgb888++ = (int8_t)(green);
    *rgb888++ = (int8_t)(blue);
    /* the second pixel */
    *rgb888++ = (int8_t)((red   >> 16));
    *rgb888++ = (int8_t)((green >> 16));
    *rgb888++ = (int8_t)((blue  >> 16));
#else
    /* the first pixel */
    *rgb888++ = (int8_t)((red  ) + zero_point);
    *rgb888++ = (int8_t)((green) + zero_point);
    *rgb888++ = (int8_t)((blue ) + zero_point);
    /* the second pixel */
    *rgb888++ = (int8_t)(((red   >> 16)) + zero_point);
    *rgb888++ = (int8_t)(((green >> 16)) + zero_point);
    *rgb888++ = (int8_t)(((blue  >> 16)) + zero_point);
#endif

    return rgb888;
}



CY_SECTION_ITCM_BEGIN
/******************************************************************************
* Function Name: ifx_image_conv_RGB565_to_RGB888
*******************************************************************************
* Summary:
*  Converts an entire image from RGB565 format (16 bits per pixel) to RGB888
*  format (24 bits per pixel). If the source image is wider than the ,
*  destination copies the center part and ignores side portions. If the source 
*  image is shorter in height, processes only the minimum height. Processes 
*  two pixels at a time for efficiency.
*
* Parameters:
*  uint8_t*src_bgr565: Pointer to the source RGB565 image buffer
*  int32_t width: Width of the source image
*  int32_t height: Height of the source image
*  uint8_t*dst_rgb888: Pointer to the destination RGB888 image buffer
*  int32_t dst_width: Width of the destination image
*  int32_t dst_height: Height of the destination image
*
* Return:
*  void
*
******************************************************************************/
void ifx_image_conv_RGB565_to_RGB888( uint8_t *src_bgr565, int32_t width, int32_t height,
        uint8_t *dst_rgb888, int32_t dst_width, int32_t dst_height )
{
    int min_height = min( height, dst_height );
    int min_width  = min( width,  dst_width );
    int offset_width = ( width - min_width ) >> 1;      /* positive offset, if the source is wider */
    uint16_t    *src_bgr565_16p = (uint16_t *) &src_bgr565[offset_width << 1];   /* 2 bytes per pixel */

    for ( int i = 0; i < min_height; i++ ) {
        uint8_t     *rgb888     = (uint8_t *)&dst_rgb888[i * dst_width * 3];   // 3 bytes per pixel
        uint32_t    *bgr565_32p = (uint32_t *)&src_bgr565_16p[i * width];

        for ( int j = (min_width >> 1); j > 0; j-- ) {
            // two-pixel RGB565 memory-access: one 32-bit read of 4-byte
            uint32_t    pixel = *bgr565_32p++;

#if 1
            rgb888 = ifx_pixel_2_RGB565_to_RGB888( pixel, rgb888 );
#else
            /* process low (15:0) first pixel */
            rgb888 = ifx_pixel_RGB565_to_RGB888( pixel, rgb888 );

            /* process high (31:16) second pixel */
            rgb888 = ifx_pixel_RGB565_to_RGB888( pixel >> 16, rgb888 );
#endif  /* 0/1 */
        }
    }
}
CY_SECTION_ITCM_END


/******************************************************************************
* Function Name: ifx_image_conv_RGB565_to_RGB888_u2i
*******************************************************************************
* Summary:
*  Converts an entire image from RGB565 format (16 bits per pixel) to RGB888
*  format (24 bits per pixel) with a zero-point offset for int8_t output. If 
*  the source image is wider than the destination, copies the center part and 
*  ignores side portions. If the source image is shorter in height, processes 
*  only the minimum height. Processes two pixels at a time for efficiency.
*
* Parameters:
*  uint8_t *src_bgr565: Pointer to the source RGB565 image buffer
*  int32_t width: Width of the source image
*  int32_t height: Height of the source image
*  int8_t *dst_rgb888_i8: Pointer to the destination RGB888 image buffer 
*                         (int8_t)
*  int32_t dst_width: Width of the destination image
*  int32_t dst_height: Height of the destination image
*  int32_t zero_point: Zero-point offset to apply to the output values
*
* Return:
*  void
*
******************************************************************************/
void ifx_image_conv_RGB565_to_RGB888_u2i( uint8_t *src_bgr565, int32_t width, int32_t height,
        int8_t *dst_rgb888_i8, int32_t dst_width, int32_t dst_height,
        int32_t zero_point )
{
    int min_height = min( height, dst_height );
    int min_width  = min( width, dst_width );
    int offset_width = ( width - min_width ) >> 1;
    uint16_t    *src_bgr565_16p = (uint16_t *) &src_bgr565[offset_width << 1];  /* 2 bytes per pixel */

    for ( int i = 0; i < min_height; i++ ) {
        int8_t      *rgb888     = &dst_rgb888_i8[i * dst_width * 3];    /* 3 bytes per pixel */
        uint32_t    *bgr565_32p = (uint32_t *)&src_bgr565_16p[i * width];

        for ( int j = (min_width >> 1); j > 0; j-- ) {
            /* two-pixel RGB565 memory-access: one 32-bit read of 4-byte */
            uint32_t    *pixel = bgr565_32p++;

    #if 1
            rgb888 = ifx_pixel_2_RGB565_to_RGB888_u2i((uint32_t)pixel, rgb888, zero_point );
    #else
            /* process low (15:0) first pixel */
            rgb888 = ifx_pixel_RGB565_to_RGB888_u2i( pixel, rgb888, zero_point );

            /* process high (31:16) second pixel */
            rgb888 = ifx_pixel_RGB565_to_RGB888_u2i( pixel >> 16, rgb888, zero_point );
#endif  /* 0/1 */
        }
    }
}



CY_SECTION_ITCM_BEGIN
/******************************************************************************
* Function Name: ifx_image_conv_RGB888_to_RGB565
*******************************************************************************
* Summary:
*  Converts an entire image from RGB888 format (24 bits per pixel) to RGB565
*  format (16 bits per pixel). If the source image is wider or taller than the
*  destination, processes only the minimum width and height. Uses the
*  ifx_pixel_RGB888_to_RGB565 helper function for pixel conversion.
*
* Parameters:
*  uint8_t *src_rgb888: Pointer to the source RGB888 image buffer
*  int32_t width: Width of the source image
*  int32_t height: Height of the source image
*  uint8_t *dst_bgr565: Pointer to the destination RGB565 image buffer
*  int32_t dst_width: Width of the destination image
*  int32_t dst_height: Height of the destination image
*
* Return:
*  void
*
******************************************************************************/
void ifx_image_conv_RGB888_to_RGB565(uint8_t *src_rgb888, int32_t width, int32_t height,
        uint8_t *dst_bgr565, int32_t dst_width, int32_t dst_height)
{
    int min_height = min(height, dst_height);
    int min_width = min(width, dst_width);
    uint16_t *dst_bgr565_16p = (uint16_t *)dst_bgr565;

    for (int i = 0; i < min_height; i++) {
        uint8_t *rgb888 = &src_rgb888[i * width * 3];
        uint16_t *bgr565_16p = &dst_bgr565_16p[i * dst_width];

        for (int j = 0; j < min_width; j++) {
            uint8_t r = *rgb888++;
            uint8_t g = *rgb888++;
            uint8_t b = *rgb888++;

            /* Convert RGB888 to BGR565 using the existing helper function */
            *bgr565_16p++ = ifx_pixel_RGB888_to_RGB565(r, g, b);
        }
    }
}
CY_SECTION_ITCM_END


/******************************************************************************
* Function Name: ifx_image_conv_RGB565_to_RGB888_quant
*******************************************************************************
* Summary:
*  Converts an entire image from RGB565 format (16 bits per pixel) to RGB888
*  format (24 bits per pixel) with quantization, applying a scale factor and
*  zero-point offset for int8_t output. If the source image is wider or taller
*  than the destination, processes only the minimum width and height.
*
* Parameters:
*  uint8_t *src_bgr565: Pointer to the source RGB565 image buffer
*  int32_t width: Width of the source image
*  int32_t height: Height of the source image
*  int8_t *dst_rgb888_i8: Pointer to the destination RGB888 image buffer 
*                         (int8_t)
*  int32_t dst_width: Width of the destination image
*  int32_t dst_height: Height of the destination image
*  float scale: Scale factor for quantization
*  int32_t zero_point: Zero-point offset to apply to the output values
*
* Return:
*  void
*
******************************************************************************/
void ifx_image_conv_RGB565_to_RGB888_quant( uint8_t *src_bgr565, int32_t width, int32_t height,
        int8_t *dst_rgb888_i8, int32_t dst_width, int32_t dst_height,
        float scale, int32_t zero_point )
{
    float   inv_scale    = 1.0f / scale;
    float   zero_point_f = (float) zero_point;
    uint8_t *dst_rgb888  = (uint8_t *) dst_rgb888_i8;

    ifx_image_conv_RGB565_to_RGB888( src_bgr565, width, height, dst_rgb888, dst_width, dst_height );

    for ( int ii = 0; ii < dst_width * dst_height; ii++ ) {
        *dst_rgb888_i8++ = (int8_t)( inv_scale * (*dst_rgb888++) + zero_point_f );
        *dst_rgb888_i8++ = (int8_t)( inv_scale * (*dst_rgb888++) + zero_point_f );
        *dst_rgb888_i8++ = (int8_t)( inv_scale * (*dst_rgb888++) + zero_point_f );
    }
}


/******************************************************************************
* Function Name: ifx_RGB565_to_RGB888_i8
*******************************************************************************
* Summary:
*  Converts a single RGB565 color pixel value to RGB888 format with a fixed
*  zero-point offset of -128 for int8_t output. Supports multiple conversion
*  modes:default (low bits set to 0), full 8-bit usage, or full 8-bit usage 
*  with rounding based on BGR888_FULL_8_BIT and BGR888_FULL_8_BIT_ROUND 
*  definitions.
*
* Parameters:
*  uint32_t pixel: 16-bit RGB565 color value
*  uint8_t *rgb888_8p: Pointer to the destination RGB888 buffer (uint8_t)
*
* Return:
*  int8_t*: Pointer to the next position in the RGB888 buffer
*
******************************************************************************/
inline uint8_t * ifx_RGB565_to_RGB888_i8( uint32_t pixel, uint8_t *rgb888_8p )
{
    /* Extract B, G, and R components from BGR565 pixel (already in BGR order) */
    uint8_t    blue   = (pixel << 3) & 0x0F8;            /* Extract lower 5 bits for blue */
    uint8_t    green  = (pixel >> 3) & 0x0FC;            /* Extract middle 6 bits for green, right shift 5 for 8-bit representation */
    uint8_t    red    = (pixel >> 8) & 0x0F8;            /* Extract upper 5 bits for red, right shift 11 for 8-bit representation */

#ifndef BGR888_FULL_8_BIT
    *rgb888_8p++ = red;
    *rgb888_8p++ = green;
    *rgb888_8p++ = blue;
#else

#ifndef BGR888_FULL_8_BIT_ROUND
    *rgb888_8p++ = (red    | (red    >> 5));
    *rgb888_8p++ = (green  | (green  >> 6));
    *rgb888_8p++ = (blue   | (blue   >> 5));
#else
    *rgb888_8p++ = (red    | (red    >> 5) | ((red   >> 4) & 0x01));    /* (significant 5-bit) | (low 3-bit with high 3-bit) | (round with 4th high bit) */
    *rgb888_8p++ = (green  | (green  >> 6) | ((green >> 5) & 0x01));    /* (significant 6-bit) | (low 2-bit with high 2-bit) | (round with 3rd high bit) */
    *rgb888_8p++ = (blue   | (blue   >> 5) | ((blue  >> 4) & 0x01));    /* (significant 5-bit) | (low 3-bit with high 3-bit) | (round with 4th high bit) */
#endif  /* BGR888_FULL_8_BIT_ROUND */

#endif  /* BGR888_FULL_8_BIT */

    return rgb888_8p;
}


/******************************************************************************
* Function Name: ifx_image_conv_RGB565_to_RGB888_i8
*******************************************************************************
* Summary:
*  Converts an entire image from RGB565 format (16 bits per pixel) to RGB888
*  format (24 bits per pixel) with a fixed zero-point offset of -128 for 
*  int8_t output. If the source image is wider than the destination, copies 
*  the center part and ignores side portions. Supports 32-bit or 16-bit memory 
*  access based on BGR565_32bit_MEMORY_ACCESS definition.
*
* Parameters:
*  uint8_t *src_bgr565: Pointer to the source RGB565 image buffer
*  int width: Width of the source image
*  int height: Height of the source image
*  uint8_t *dst_rgb888_i8: Pointer to the destination RGB888 image buffer 
*                         (int8_t)
*  int dst_width: Width of the destination image
*  int dst_height: Height of the destination image
*
* Return:
*  void
*
******************************************************************************/
void ifx_image_conv_RGB565_to_RGB888_i8( uint8_t *src_bgr565, int width, int height,
        uint8_t *dst_rgb888_i8, int dst_width, int dst_height )
{
    int min_height = min( height, dst_height );
    int min_width  = min( width, dst_width );
    int offset_width = ( width - min_width ) / 2;

    for ( int i = 0; i < min_height; i++ ) {
        uint8_t  *rgb888_8p = (uint8_t *)&dst_rgb888_i8[i * dst_width * 3];
        uint8_t *bgr565_8p = &src_bgr565[(i * width + offset_width) * 2];
#ifdef BGR565_32bit_MEMORY_ACCESS    /* 32-bit two-pixel memory-access */
        uint32_t    *bgr565_32p = (uint32_t *)bgr565_8p;

        for ( int j = 0; j < min_width / 2; j++ ) {
            uint32_t    pixel = *bgr565_32p++;

            /* process low (15:0) first pixel */
            rgb888_8p = ifx_RGB565_to_RGB888_i8( pixel, rgb888_8p );

            /* process high (31:16) second pixel */
            rgb888_8p = ifx_RGB565_to_RGB888_i8( pixel >> 16, rgb888_8p );
        }
#else    /* 16-bit one-pixel memory-access */
        uint16_t    *bgr565_16p = (uint16_t *)bgr565_8p;
        for ( int j = 0; j < min_width; j++ ) {
            uint16_t pixel = *bgr565_16p++;

            rgb888_8p = ifx_RGB565_to_RGB888_i8( pixel, rgb888_8p );
        }
#endif    /* BGR565_32bit_MEMORY_ACCESS */
    }
}


/******************************************************************************
* Function Name: ifx_pixel_RGB888_to_RGBX32
*******************************************************************************
* Summary:
*  Converts RGB888 color values (8 bits per channel) to a 32-bit RGBX format,
*  combining red, green, and blue components into a single 32-bit value with
*  an unused byte.
*
* Parameters:
*  uint32_t r: Red color value (0-255)
*  uint32_t g: Green color value (0-255)
*  uint32_t b: Blue color value (0-255)
*
* Return:
*  uint32_t: 32-bit RGBX color value
*
******************************************************************************/
inline uint32_t ifx_pixel_RGB888_to_RGBX32( uint32_t r, uint32_t g, uint32_t b )
{
    return ((r << 16) | (g << 8) | b);
}


/******************************************************************************
* Function Name: ifx_pixel_xRGB32_to_RGB565
*******************************************************************************
* Summary:
*  Converts a 32-bit RGBX color value to RGB565 format (16 bits: 5 bits red,
*  6 bits green, 5 bits blue).
*
* Parameters:
*  uint32_t xRGB: 32-bit RGBX color value
*
* Return:
*  uint16_t: 16-bit RGB565 color value
*
******************************************************************************/
inline uint16_t ifx_pixel_xRGB32_to_RGB565( uint32_t xRGB )
{
    uint16_t    r = (xRGB >> 8) & 0xF800u;
    uint16_t    g = (xRGB >> 5) & 0x07E0u;
    uint16_t    b = (xRGB >> 3) & 0x001Fu;

    return (uint16_t)(r | g | b);
}


/******************************************************************************
* Function Name: ifx_image_conv_RGBX32_to_RGB24
*******************************************************************************
* Summary:
*  Converts an entire image from RGBX32 format (32 bits per pixel) to RGB888
*  format (24 bits per pixel). If the source image is wider than the 
*  destination,copies the center part and ignores side portions. If the 
*  source image is shorter in height, processes only the minimum height.
*
* Parameters:
*  uint8_t *src_rgbx32: Pointer to the source RGBX32 image buffer
*  int32_t width: Width of the source image
*  int32_t height: Height of the source image
*  uint8_t *dst_rgb888: Pointer to the destination RGB888 image buffer
*  int32_t dst_width: Width of the destination image
*  int32_t dst_height: Height of the destination image
*
* Return:
*  void
*
******************************************************************************/
void ifx_image_conv_RGBX32_to_RGB24( uint8_t *src_rgbx32, int32_t width, int32_t height,
        uint8_t *dst_rgb888, int32_t dst_width, int32_t dst_height )
{
    int min_height = min( height, dst_height );
    int min_width  = min( width, dst_width );
    int offset_width = ( width - min_width ) >> 1;
    uint32_t    *src_32p = (uint32_t *)&src_rgbx32[offset_width << 1];

    for ( int i = 0; i < min_height; i++ ) {
        uint8_t     *dp = &dst_rgb888[i * dst_width * 3];
        uint32_t    *sp = &src_32p[i * width];

        for ( int j = 0; j < min_width; j++ ) {
            uint32_t    pixel = *sp++;

            *dp++ = (uint8_t)(pixel >> 16);
            *dp++ = (uint8_t)(pixel >>  8);
            *dp++ = (uint8_t)(pixel);
        }
    }
}


/******************************************************************************
* Function Name: ifx_image_conv_RGBX32_to_RGB24_u2i
*******************************************************************************
* Summary:
*  Converts an entire image from RGBX32 format (32 bits per pixel) to RGB888
*  format (24 bits per pixel) with a zero-point offset for int8_t output. If 
*  the source image is wider than the destination, copies the center part and 
*  ignores side portions. If the source image is shorter in height, processes 
*  only the minimum height.
*
* Parameters:
*  uint8_t *src_rgbx32: Pointer to the source RGBX32 image buffer
*  int32_t width: Width of the source image
*  int32_t height: Height of the source image
*  int8_t *dst_rgb888: Pointer to the destination RGB888 image buffer (int8_t)
*  int32_t dst_width: Width of the destination image
*  int32_t dst_height: Height of the destination image
*  int32_t zero_point: Zero-point offset to apply to the output values
*
* Return:
*  void
*
******************************************************************************/
void ifx_image_conv_RGBX32_to_RGB24_u2i( uint8_t *src_rgbx32, int32_t width, int32_t height,
        int8_t  *dst_rgb888, int32_t dst_width, int32_t dst_height,
        int32_t zero_point )
{
    int min_height = min( height, dst_height );
    int min_width  = min( width, dst_width );
    int offset_width = ( width - min_width ) >> 1;
    uint32_t    *src_32p = (uint32_t *)&src_rgbx32[offset_width << 1];

    for ( int i = 0; i < min_height; i++ ) {
        int8_t      *dp = &dst_rgb888[i * dst_width * 3];
        uint32_t    *sp = &src_32p[i * width];

        for ( int j = 0; j < min_width; j++ ) {
            uint32_t    pixel = *sp++;

            *dp++ = (int8_t)((pixel >> 16) + zero_point);
            *dp++ = (int8_t)((pixel >>  8) + zero_point);
            *dp++ = (int8_t)((pixel      ) + zero_point);
        }
    }
}


/******************************************************************************
* Function Name: draw_image_pixel
*******************************************************************************
* Summary:
*  Draws a single pixel on an LCD display by converting RGB888 color values to
*  RGB565 format and writing to the specified position in the destination 
*  buffer, provided the coordinates are within the LCD dimensions.
*
* Parameters:
*  uint16_t *pDst: Pointer to the destination LCD buffer (RGB565)
*  uint16_t x: X-coordinate of the pixel
*  uint16_t y: Y-coordinate of the pixel
*  uint16_t r: Red color value (0-255)
*  uint16_t g: Green color value (0-255)
*  uint16_t b: Blue color value (0-255)
*  uint16_t lcd_w: Width of the LCD display
*  uint16_t lcd_h: Height of the LCD display
*
* Return:
*  void
*
******************************************************************************/
void draw_image_pixel( uint16_t *pDst, uint16_t x, uint16_t y, uint16_t r, uint16_t g, uint16_t b, uint16_t lcd_w, uint16_t lcd_h )
{
    if ( x < lcd_w  &&  y < lcd_h ) {
        uint16_t rgb565 = ifx_pixel_RGB888_to_RGB565(r, g, b);

        pDst[(y * lcd_w) + x] = rgb565;
    }
}

/*******************************************************************************
 * Function Name: IMAGE_DrawRect
 ********************************************************************************
 * Summary:
 *  Draws a rectangle with RGB color.
 *
 * Parameters:
 *  uint16_t    pDst    pointer to the display buffer
 *  int16_t     x0      starting point of line on X axis
 *  int16_t     y0      starting point of line on Y axis
 *  int16_t     x1      end point of line on X axis
 *  int16_t     y1      end point of line on X axis
 *  uint16_t    r       0-255 red color value
 *  uint16_t    g       0-255 green color value
 *  uint16_t    b       0-255 blue color value
 *  uint16_t    lcd_w   LCD display width
 *  uint16_t    lcd_h   LCD display height
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void IMAGE_DrawRect( uint16_t *pdst, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t r, uint16_t g, uint16_t b, uint16_t lcd_w, uint16_t lcd_h )
{
    uint16_t rgb565 = ifx_pixel_RGB888_to_RGB565(r, g, b);

    ifx_image_draw_line_u16( pdst, x0, y0, x1, y0, rgb565, lcd_w, lcd_h );
    ifx_image_draw_line_u16( pdst, x0, y0, x0, y1, rgb565, lcd_w, lcd_h );
    ifx_image_draw_line_u16( pdst, x0, y1, x1, y1, rgb565, lcd_w, lcd_h );
    ifx_image_draw_line_u16( pdst, x1, y0, x1, y1, rgb565, lcd_w, lcd_h );
}

/*******************************************************************************
 * Function Name: ifx_image_draw_line_u16
 ********************************************************************************
 * Summary:
 *  Draws a line with 16-bit RGB565 color.
 *
 * Parameters:
 *  uint16_t    pDst    pointer to the display buffer
 *  int16_t     x0      starting point of line on X axis
 *  int16_t     y0      starting point of line on Y axis
 *  int16_t     x1      end point of line on X axis
 *  int16_t     y1      end point of line on X axis
 *  uint16_t    rgb565  16-bit RGB565 color
 *  uint16_t    lcd_w   LCD display width
 *  uint16_t    lcd_h   LCD display height
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void ifx_image_draw_line_u16( uint16_t* pDst, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t rgb565, uint16_t lcd_w, uint16_t lcd_h )
{
    int min_x, max_x, min_y, max_y;

    if ( x0 <= x1 ) {
        min_x = x0;
        max_x = x1;
    } else {
        min_x = x1;
        max_x = x0;
    }
    if ( y0 <= y1 ) {
        min_y = y0;
        max_y = y1;
    } else {
        min_y = y1;
        max_y = y0;
    }
    // check if the line is out of the display
    if ( max_x < 0  ||  max_y < 0  ||  min_x >= lcd_w  ||  min_y >= lcd_h )
        return;

    if ( x0 == x1 ) {
        if ( y0 == y1 ) {
            ifx_image_draw_pixel_u16( pDst, x0, y0, rgb565, lcd_w );
        } else {
            // vertical line
            for ( uint16_t y = max(min_y, 0); y < min(max_y, lcd_h); y++ )
                ifx_image_draw_pixel_u16( pDst, x0, y, rgb565, lcd_w );
        }
        return;
    } else if ( y0 == y1 ) {
        // horizontal line
        for ( uint16_t x = max(min_x, 0); x < min(max_x, lcd_w); x++ )
            ifx_image_draw_pixel_u16( pDst, x, y0, rgb565, lcd_w );
        return;
    }

    int     idx = max_x - min_x;
    int     idy = max_y - min_y;

    if ( idx >= idy ) {
        if ( x0 > x1 ) {
            int16_t     tmp = x0;
            x0 = x1;
            x1 = tmp;
            tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        float   dx = x1 - x0;
        float   dy = y1 - y0;
        float   k  = dy / dx;
        float   fy = y0;
        uint16_t    min_x = x0;
        uint16_t    max_x = min( x1, lcd_w );

        if ( x0 < 0 ) {
            fy += k * (- x0);
            min_x = 0;
        }
        for ( uint16_t ix = min_x; ix < max_x; ix++, fy += k ) {
            int16_t iy = fy + 0.5f;
            if ( 0 <= iy  &&  iy < lcd_h )
                ifx_image_draw_pixel_u16( pDst, ix, (uint16_t)iy, rgb565, lcd_w );
        }
    } else {
        if ( y0 > y1 ) {
            int16_t     tmp = x0;
            x0 = x1;
            x1 = tmp;
            tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        float   dx = x1 - x0;
        float   dy = y1 - y0;
        float   k = dx / dy;
        float   fx = x0;
        uint16_t    min_y = y0;
        uint16_t    max_y = min( y1, lcd_h );

        if ( y0 < 0 ) {
            fx += k * (- y0);
            min_y = 0;
        }
        for ( uint16_t iy = min_y; iy < max_y; iy++, fx += k ) {
            int16_t ix = (fx + 0.5f);
            ifx_image_draw_pixel_u16( pDst, (uint16_t)ix, iy, rgb565, lcd_w );
        }
    }
}

/*******************************************************************************
 * Function Name: ifx_image_draw_pixel_u16
 ********************************************************************************
 * Summary:
 *  Draws pixel with 16-bit RGB565 color to defined point.
 *
 * Parameters:
 *  uint16_t    pDst    pointer to the display buffer
 *  uint16_t    x       drawing position on X axis
 *  uint16_t    y       drawing position on Y axis
 *  uint16_t    rgb565  16-bit RGB565 color
 *  uint16_t    lcd_w   LCD display width
 *
 * Return:
 *  void
 *
 * Note:
 *  This function does NOT check if the point is inside of the display.
 *  So, the user SHOULD pass the valid position inside of the display.
 *
 *******************************************************************************/
inline
void ifx_image_draw_pixel_u16( uint16_t *pDst, uint16_t x, uint16_t y, uint16_t rgb565, uint16_t lcd_w )
{
    pDst[(y * lcd_w) + x] = rgb565;
}

