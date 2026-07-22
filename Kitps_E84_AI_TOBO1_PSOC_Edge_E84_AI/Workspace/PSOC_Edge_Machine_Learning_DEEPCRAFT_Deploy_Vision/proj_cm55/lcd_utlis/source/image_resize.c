/******************************************************************************
* File Name        : image_resize.c
*
* Description      : This file implements the resize image functions.
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
#include "stdio.h"
#include "cy_retarget_io.h"

#include "ifx_image_utils.h"

/*******************************************************************************
* Function Name: image_resize_Ratio
********************************************************************************
* Summary:
*  Resizes an image while maintaining aspect ratio, supporting 1 or 3 channels.
*
* Parameters:
*  uint8_t *srcImage: Pointer to the source image buffer
*  int srcWidth: Width of the source image
*  int srcHeight: Height of the source image
*  int channels: Number of channels (1 or 3)
*  uint8_t *dstImage: Pointer to the destination image buffer
*  int dstWidth: Width of the destination image
*  int dstHeight: Height of the destination image
*  float *scaledWidth: Pointer to store the scaled width factor
*  float *scaledHeight: Pointer to store the scaled height factor
*
* Return:
*  None
*
*******************************************************************************/
void image_resize_Ratio( uint8_t* srcImage, int srcWidth, int srcHeight, int channels,
                             uint8_t* dstImage, int dstWidth, int dstHeight,
                         float *scaledWidth, float *scaledHeight )
{
    int     srcLineWidth = channels * srcWidth;
    int     dstLineWidth = channels * dstWidth;
    float   dx = (float)srcWidth  / (float)dstWidth;
    float   dy = (float)srcHeight / (float)dstHeight;
    int        src_j_map[dstWidth];
    int     i, j;

    if (dx >= dy)
        dy = dx;
    else
        dx = dy;
    if ( dx < 1.0f ) {
        // do not allow to enlarge a small image
        dx = dy = 1.0f;
    }
    *scaledWidth  = (int)(dstWidth  * dx + 0.5f);
    *scaledHeight = (int)(dstHeight * dy + 0.5f);

    // initialize the output image with the gray color
    memset( (void *)dstImage, 128, dstHeight * dstWidth * channels );

    float    src_j_f = 0.5f * dx;

    // construct a map from dest-pixel-x-position to src-pixel-x-position
    for ( j = 0; j < dstWidth  &&  (int)(src_j_f + 0.5f) < srcWidth; j++, src_j_f += dx ) {
        src_j_map[j] = (int)(src_j_f + 0.5f);
    }
    int    max_j = j;

    if (channels == 1) {
        float    src_i_f = 0.5f * dy;
        int    dstIdx = 0;

        for ( i = 0; i < dstHeight; i++, dstIdx += dstLineWidth, src_i_f += dy ) {
            int    src_i = (int)(src_i_f + 0.5f);

            if ( src_i < srcHeight ) {
                int        srcIdx = src_i * srcLineWidth;
                float    src_j_f = 0.5f * dx;

                for ( j = 0; j < dstWidth; j++, src_j_f += dx ) {
                  int    src_j = (int)(src_j_f + 0.5f);

                  if ( src_j < srcWidth ) {
                      dstImage[dstIdx + j] = srcImage[srcIdx + src_j];
                  }
                }
            }
        }
    } else if (channels == 3) {
        float    src_i_f = 0.5f * dy;
        int        dstIdx = 0;

        for ( i = 0; i < dstHeight  &&  (int)(src_i_f + 0.5f) < srcHeight; i++, dstIdx += dstLineWidth, src_i_f += dy ) {
            int    src_i  = (int)(src_i_f + 0.5f);
            int    srcIdx = src_i * srcLineWidth;

            for ( j = 0; j < max_j; j++ ) {
                int    src_j = src_j_map[j];
                memcpy( (void *)&dstImage[dstIdx + j * 3], (void *)&srcImage[srcIdx + src_j * 3], 3 );
            }
        }
    }
}


/*******************************************************************************
* Function Name: ifx_image_resize_RGB565_to_RGB888_Ratio
********************************************************************************
* Summary:
*  Resizes an RGB565 image to an RGB888 image while maintaining aspect ratio.
*  Supports down-scaling only, with padding value of 128 (gray).
*
* Parameters:
*  uint16_t *srcImage_RGB565: Pointer to the source RGB565 image buffer
*  int srcWidth: Width of the source image
*  int srcHeight: Height of the source image
*  int channels: Number of channels (typically 3 for RGB)
*  uint8_t *dstImage_RGB888: Pointer to the destination RGB888 image buffer
*  int dstWidth: Width of the destination image
*  int dstHeight: Height of the destination image
*  float *scaledWidth: Pointer to store the scaled width factor
*  float *scaledHeight: Pointer to store the scaled height factor
*
* Return:
*  None
*
*******************************************************************************/
void ifx_image_resize_RGB565_to_RGB888_Ratio( uint16_t* srcImage_RGB565, int srcWidth, int srcHeight, int channels,
                                          uint8_t* dstImage_RGB888, int dstWidth, int dstHeight,
                                          float *scaledWidth, float *scaledHeight )
{
    int     srcLineWidth = srcWidth;
    int     dstLineWidth = 3 * dstWidth;
    float   dx = (float)srcWidth  / (float)dstWidth;
    float   dy = (float)srcHeight / (float)dstHeight;

    // uniform-scale to keep the width-height ratio
    if (dx >= dy)
        dy = dx;
    else
        dx = dy;
    // not allow up-scale
    if ( dx < 1.0f ) {
        dx = dy = 1.0f;
    }
    *scaledWidth  = (int)(dstWidth  * dx + 0.5f);
    *scaledHeight = (int)(dstHeight * dy + 0.5f);

    memset( (void *)dstImage_RGB888, 128, dstHeight * dstWidth * channels );

    int     i, j;
    int        src_j_map[dstWidth];
    float    src_j_f = 0.5f * dx;

    for ( j = 0; j < dstWidth; j++, src_j_f += dx ) {
        int    src_j = (int)(src_j_f + 0.5f);
        if ( src_j >= srcWidth )
            break;
        src_j_map[j] = src_j;
    }
    int    max_j = j;

    float    src_i_f = 0.5f * dy;
    int        dstIdx = 0;

    for ( i = 0; i < dstHeight; i++, dstIdx += dstLineWidth, src_i_f += dy ) {
        int    src_i = (int)(src_i_f + 0.5f);

        if ( src_i >= srcHeight )
            break;

        int    srcIdx = src_i * srcLineWidth;
        uint8_t    *pdst = &dstImage_RGB888[dstIdx];

        for ( j = 0; j < max_j; j++ ) {
            int    src_j = src_j_map[j];
            pdst = ifx_pixel_RGB565_to_RGB888( srcImage_RGB565[srcIdx + src_j], pdst );
        }
    }
}


/*******************************************************************************
* Function Name: ifx_image_resize_Rect
********************************************************************************
* Summary:
*  Resizes a rectangular region from a source image to a destination image,
*  handling protrusive bounding boxes.
*
* Parameters:
*  int8_t *dstImage: Pointer to the destination image buffer
*  int dst_w: Width of the destination image
*  int dst_h: Height of the destination image
*  int dst_c: Number of channels in the destination image
*  int8_t *srcImage: Pointer to the source image buffer
*  int img_w: Width of the source image
*  int img_h: Height of the source image
*  float xmin: Minimum X coordinate of the bounding box
*  float ymin: Minimum Y coordinate of the bounding box
*  float xmax: Maximum X coordinate of the bounding box
*  float ymax: Maximum Y coordinate of the bounding box
*
* Return:
*  None
*
*******************************************************************************/
void ifx_image_resize_Rect( int8_t * dstImage, int dst_w, int dst_h, int dst_c,
                        int8_t * srcImage, int img_w, int img_h,
                        float xmin, float ymin, float xmax, float ymax )
{
    float   rect_w = xmax - xmin + 1.0f;
    float   rect_h = ymax - ymin + 1.0f;
    float   step_x = rect_w / dst_w;
    float   step_y = rect_h / dst_h;
    int        src_j_map[dst_w];
    int        min_j, max_j;

    memset( (void *)dstImage, 0, dst_w * dst_h * dst_c );    // initialize to mid-value of uint8

    // construct a map from dest-pixel-x-position to src-pixel-x-position
    // lower boundary: skip outside of left boundary of the source image
    float    src_j_f = xmin;            // float target X position
    for ( min_j = 0; min_j < dst_w  &&  (src_j_f + 0.5f) < 0; min_j++, src_j_f += step_x );
    // upper boundary
    for ( max_j = min_j; max_j < dst_w  &&  (int)(src_j_f + 0.5f) < img_w; max_j++, src_j_f += step_x ) {
        src_j_map[max_j] = (int)(src_j_f + 0.5f);
    }

    float   src_y_f = ymin;            // float target Y position
    int        y;
    for ( y = 0; y < dst_h  &&  (src_y_f + 0.5f) < 0; y++, src_y_f += step_y );
    for ( ; y < dst_h  &&  (int)(src_y_f + 0.5f) < img_h; y++, src_y_f += step_y ) {
        int src_y = (int)(src_y_f + 0.5f);        // Y coordinate of the upper neighbors

        for ( int x = min_j; x < max_j; x++ ) {
            int src_x = src_j_map[x];       // X coordinate of left neighbors

            memcpy( (void *)&dstImage[INDEX3D(y, x, 0, dst_w, dst_c)], (void *)&srcImage[INDEX3D(src_y, src_x, 0, img_w, dst_c)], dst_c );
        }
    }
}

/*******************************************************************************
* Function Name: ifx_image_resize_Rect_Linear_i8
********************************************************************************
* Summary:
*  Resizes a rectangular region from a source image to a destination image using
*  linear interpolation, handling protrusive bounding boxes.
*
* Parameters:
*  int8_t *dstImage: Pointer to the destination image buffer
*  int dst_w: Width of the destination image
*  int dst_h: Height of the destination image
*  int dst_c: Number of channels in the destination image
*  int8_t *srcImage: Pointer to the source image buffer
*  int img_w: Width of the source image
*  int img_h: Height of the source image
*  float xmin: Minimum X coordinate of the bounding box
*  float ymin: Minimum Y coordinate of the bounding box
*  float xmax: Maximum X coordinate of the bounding box
*  float ymax: Maximum Y coordinate of the bounding box
*
* Return:
*  None
*
*******************************************************************************/
void ifx_image_resize_Rect_Linear_i8( int8_t * dstImage, int dst_w, int dst_h, int dst_c,
                               int8_t * srcImage, int img_w, int img_h,
                               float xmin, float ymin, float xmax, float ymax )
{
    float   rect_w = xmax - xmin + 1.0f;
    float   rect_h = ymax - ymin + 1.0f;
    float   step_x = rect_w / dst_w;
    float   step_y = rect_h / dst_h;
    float    fx_0[dst_w], fx_1[dst_w];    // weights for two X-neighbors of a target position
    int        src_x_map[dst_w];            // X-location map from destination to source
    float   fy[2];                       // weights for two Y-neighbors of a target position
    float   src_x_f, src_y_f;            // float source X, Y position
    int        min_x, max_x;                // low, high boundary in X axis

    // construct a map from dest-pixel-x-position to src-pixel-x-position
    // lower boundary: skip outside of left boundary of the source image
    src_x_f = xmin;
    for ( min_x = 0; min_x < dst_w  &&  src_x_f < 0; min_x++, src_x_f += step_x );
    // upper boundary
    for ( max_x = min_x; max_x < dst_w  &&  src_x_f < img_w; max_x++, src_x_f += step_x ) {
        int src_x = (int)src_x_f;       // X coordinate of left neighbors

        if ( src_x <= img_w - 2 ) {
            fx_0[max_x] = src_x_f - src_x;        // weight for the right neighbors: (src_x + 1, y)
        } else {
            // reach the right boundary of the image and the right neighbors are out of the image.
            // to use the four-neighbor equation, shift the neighbors left.
            src_x = img_w - 2;              // this left neighbor is invalid, because actual src_x >= img_w - 1
            fx_0[max_x] = 1.0f;                   // the right neighbors' weight SHOULD be 1.0, then the invalid left neighbors' weight will be 0.
        }
        fx_1[max_x] = 1.0f - fx_0[max_x];           // weight for the left neighbors: (src_x, y)
        src_x_map[max_x] = src_x;
    }

    memset( (void *)dstImage, 0, dst_w * dst_h * dst_c );    // initialize to mid-value of int8

    src_y_f = ymin;
    for ( int y = 0; y < dst_h  &&  src_y_f < img_h; y++, src_y_f += step_y ) {
        int src_y = (int)src_y_f;        // Y coordinate of the upper neighbors

        if ( src_y < 0 )
            continue;

        if ( src_y <= img_h - 2 ) {
            fy[0] = src_y_f - src_y;        // weight for the lower neighbors: (x, src_y + 1)
        } else {
            // reach the lower boundary of the image and the lower neighbors are out of the image.
            // to use the four-neighbor equation, shift the neighbors up.
            src_y = img_h - 2;                 // this upper neighbor is invalid, because actual src_y >= img_h - 1
            fy[0] = 1.0f;                   // the lower neighbors' weight SHOULD be 1.0, then the invalid upper neighbors' weight will be 0.
        }
        fy[1] = 1.0f - fy[0];           // weight for the upper neighbors: (x, src_y)

        for ( int x = min_x; x < max_x; x++ ) {
            int src_x = src_x_map[x];       // X coordinate of left neighbors

            for ( int c = 0; c < dst_c; c++ ) {
                // weighted sum of the four neighbors of the actual source position
                dstImage[INDEX3D(y, x, c, dst_w, dst_c)] = (int8_t) ( srcImage[INDEX3D(src_y,     src_x,     c, img_w, dst_c)] * fx_1[x] * fy[1]
                                                                    + srcImage[INDEX3D(src_y,     src_x + 1, c, img_w, dst_c)] * fx_0[x] * fy[1]
                                                                    + srcImage[INDEX3D(src_y + 1, src_x,     c, img_w, dst_c)] * fx_1[x] * fy[0]
                                                                    + srcImage[INDEX3D(src_y + 1, src_x + 1, c, img_w, dst_c)] * fx_0[x] * fy[0] + 0.5f );
            }
        }
    }
}


/*******************************************************************************
* Function Name: ifx_image_extract_Rect
********************************************************************************
* Summary:
*  Crops a rectangular area from the source image to the destination image.
*  Supports both int8_t and uint8_t images (cast to uint8_t for int8_t).
*
* Parameters:
*  uint8_t *dstImage: Pointer to the destination image buffer
*  int dst_w: Width of the destination image
*  int dst_h: Height of the destination image
*  int dst_c: Number of channels in the destination image
*  uint8_t *srcImage: Pointer to the source image buffer
*  int img_w: Width of the source image
*  int img_h: Height of the source image
*  int xmin: Minimum X coordinate for cropping
*  int ymin: Minimum Y coordinate for cropping
*
* Return:
*  None
*
*******************************************************************************/
void ifx_image_extract_Rect( uint8_t * dstImage, int dst_w, int dst_h, int dst_c,
                      uint8_t * srcImage, int img_w, int img_h,
                      int xmin, int ymin )
{
    // initialize to mid-value of int8_t or black for uint8_t
    memset( (void *)dstImage, 0, dst_w * dst_h * dst_c );

    // check if the crop area is out of image
    if ( xmin >= img_w  ||  ymin >= img_w )
        return;

    // width and height of the crop area
    int    rect_w = dst_w;
    int    rect_h = dst_h;

    // check if the starting location if out of image, then set it to 0.
    if ( xmin < 0 ) {
        rect_w += xmin;
        xmin = 0;
    }
    if ( ymin < 0 ) {
        rect_h += ymin;
        ymin = 0;
    }

    // adjust width and height of the crop area, if necessary
    rect_w = min( rect_w, img_w - xmin );
    rect_h = min( rect_h, img_h - ymin );

    int    src_offset_w = xmin;
    int    src_offset_h = ymin;
    int    dst_offset_w = 0;
    int    dst_offset_h = 0;

    uint8_t    *dp = &dstImage[INDEX3D(dst_offset_h, dst_offset_w, 0, dst_w, dst_c )];
    uint8_t    *sp = &srcImage[INDEX3D(src_offset_h, src_offset_w, 0, img_w, dst_c )];

    for ( int y = 0; y < rect_h; y++ ) {
        memcpy( dp, sp, rect_w * dst_c );
        dp += dst_w * dst_c;
        sp += img_w * dst_c;
    }
}

/*******************************************************************************
* Function Name: ifx_image_extract_Rect_i8
********************************************************************************
* Summary:
*  Extracts a centered rectangular area from the source image to the destination
*  image. Supports both int8_t and uint8_t images (cast to uint8_t for int8_t).
*
* Parameters:
*  uint8_t *dstImage: Pointer to the destination image buffer
*  int dst_w: Width of the destination image
*  int dst_h: Height of the destination image
*  int dst_c: Number of channels in the destination image
*  uint8_t *srcImage: Pointer to the source image buffer
*  int img_w: Width of the source image
*  int img_h: Height of the source image
*
* Return:
*  None
*
*******************************************************************************/
void ifx_image_extract_Rect_i8( uint8_t * dstImage, int dst_w, int dst_h, int dst_c,
                         uint8_t * srcImage, int img_w, int img_h )
{
    int    rect_w = min( dst_w, img_w );
    int    rect_h = min( dst_h, img_h );

    memset( (void *)dstImage, 0, dst_w * dst_h * dst_c );    // initialize to mid-value of uint8

    int    src_offset_w = max( ( img_w - rect_w ) / 2, 0 );
    int    src_offset_h = max( ( img_h - rect_h ) / 2, 0 );
    int    dst_offset_w = max( ( dst_w - rect_w ) / 2, 0 );
    int    dst_offset_h = max( ( dst_h - rect_h ) / 2, 0 );

    uint8_t    *dp = &dstImage[INDEX3D(dst_offset_h, dst_offset_w, 0, dst_w, dst_c )];
    uint8_t    *sp = &srcImage[INDEX3D(src_offset_h, src_offset_w, 0, img_w, dst_c )];

    for ( int y = 0; y < rect_h; y++ ) {
        memcpy( dp, sp, rect_w * dst_c );
        dp += dst_w * dst_c;
        sp += img_w * dst_c;
    }
}

/*******************************************************************************
* Function Name: ifx_image_extract_Rect_u2i
********************************************************************************
* Summary:
*  Extracts a centered rectangular area from an unsigned source image to a signed
*  destination image with zero point adjustment.
*
* Parameters:
*  int8_t *dstImage: Pointer to the destination signed image buffer
*  int dst_w: Width of the destination image
*  int dst_h: Height of the destination image
*  int dst_c: Number of channels in the destination image
*  uint8_t *srcImage: Pointer to the source unsigned image buffer
*  int img_w: Width of the source image
*  int img_h: Height of the source image
*
* Return:
*  None
*
*******************************************************************************/
void ifx_image_extract_Rect_u2i( int8_t * dstImage, int dst_w, int dst_h, int dst_c,
                             uint8_t * srcImage, int img_w, int img_h )
{
    int    rect_w = min( dst_w, img_w );
    int    rect_h = min( dst_h, img_h );

    memset( (void *)dstImage, 0, dst_w * dst_h * dst_c );    // initialize to mid-value of uint8

    int    src_offset_w = max( ( img_w - rect_w ) / 2, 0 );
    int    src_offset_h = max( ( img_h - rect_h ) / 2, 0 );
    int    dst_offset_w = max( ( dst_w - rect_w ) / 2, 0 );
    int    dst_offset_h = max( ( dst_h - rect_h ) / 2, 0 );

    int8_t    *dp = &dstImage[INDEX3D(dst_offset_h, dst_offset_w, 0, dst_w, dst_c )];
    uint8_t    *sp = &srcImage[INDEX3D(src_offset_h, src_offset_w, 0, img_w, dst_c )];

    for ( int y = 0; y < rect_h; y++ ) {
        for ( int ii = 0; ii < rect_w * dst_c; ii++ )
            dp[ii] = sp[ii] - 128;
        dp += dst_w * dst_c;
        sp += img_w * dst_c;
    }
}

