/*******************************************************************************
* File Name        : lcd_draw.c
*
* Description      : This file implements the direct draw functions to the 
*                    display frame buffer.
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

/******************************************************************************
* Header Files
******************************************************************************/
#include <stdlib.h>
#include <stdint.h>
#include "cy_syslib.h"
#include "lcd_task.h"
#include "lcd_graphics.h"
#include "ifx_image_utils.h"

/*******************************************************************************
* Macros
*******************************************************************************/

/*******************************************************************************
* Global Variables
*******************************************************************************/
static uint32_t    Display_Width = 832;
static uint32_t    Display_Height = 480;
/* foreground / background colors */
static uint32_t    LCD_Colors[2] = {
        0X00000000,     /* background: black */
        0X00FFFFFF      /* foreground: white */
};

/*******************************************************************************
* Functions
*******************************************************************************/

/*******************************************************************************
* Function Name: ifx_lcd_set_Display_size
********************************************************************************
* Summary:
* Sets the display dimensions if the provided width and height are valid 
* (greater than 0).
*
* Parameters:
*  uint32_t width: Width of the display
*  uint32_t height: Height of the display
*
* Return:
*  void
*******************************************************************************/
void ifx_lcd_set_Display_size( uint32_t width, uint32_t height )
{
    if (width > 0 && height > 0) {
        Display_Width  = width;
        Display_Height = height;
    }
}
 

/*******************************************************************************
* Function Name: ifx_lcd_get_Display_Width
********************************************************************************
* Summary:
* Returns the current display width.
*
* Parameters:
*  void
*
* Return:
*  uint32_t: Current display width
*******************************************************************************/
inline uint32_t ifx_lcd_get_Display_Width()
{
    return Display_Width;
}


/*******************************************************************************
* Function Name: ifx_lcd_get_Display_Height
********************************************************************************
* Summary:
* Returns the current display height.
*
* Parameters:
*  void
*
* Return:
*  uint32_t: Current display height
*******************************************************************************/
inline uint32_t ifx_lcd_get_Display_Height()
{
    return Display_Height;
}

/*******************************************************************************
* Color management functions
*******************************************************************************/

/*******************************************************************************
* Function Name: bsp_lcd_set_FGcolor
********************************************************************************
* Summary:
* Sets the foreground color for drawing operations, converting RGB888 to either
* RGB565 or XRGB32 based on the display configuration.
*
* Parameters:
*  uint16_t r: Red color value (0-255)
*  uint16_t g: Green color value (0-255)
*  uint16_t b: Blue color value (0-255)
*
* Return:
*  uint32_t: The set foreground color value
*******************************************************************************/
uint32_t bsp_lcd_set_FGcolor( uint16_t r, uint16_t g, uint16_t b )
{
    LCD_Colors[1] =
#ifndef LCD_COLOR_XRGB32
    ifx_pixel_RGB888_to_RGB565(r, g, b);
#else
    ifx_pixel_RGB888_to_RGBX32(r, g, b);
#endif  /* LCD_COLOR_XRGB32*/

    return LCD_Colors[1];
}


/*******************************************************************************
* Function Name: bsp_lcd_set_BGcolor
********************************************************************************
* Summary:
* Sets the background color for drawing operations, converting RGB888 to either
* RGB565 or XRGB32 based on the display configuration.
*
* Parameters:
*  uint16_t r: Red color value (0-255)
*  uint16_t g: Green color value (0-255)
*  uint16_t b: Blue color value (0-255)
*
* Return:
*  uint32_t: The set background color value
*******************************************************************************/
uint32_t bsp_lcd_set_BGcolor( uint16_t r, uint16_t g, uint16_t b )
{
    LCD_Colors[0] =
#ifndef LCD_COLOR_XRGB32
    ifx_pixel_RGB888_to_RGB565(r, g, b);
#else
    ifx_pixel_RGB888_to_RGBX32(r, g, b);
#endif  /* LCD_COLOR_XRGB32*/

    return LCD_Colors[0];
}


/*******************************************************************************
* Function Name: bsp_lcd_get_FGcolor
********************************************************************************
* Summary:
* Returns the current foreground color.
*
* Parameters:
*  void
*
* Return:
*  uint32_t: Current foreground color value
*******************************************************************************/
uint32_t bsp_lcd_get_FGcolor()
{
    return LCD_Colors[1];
}


/*******************************************************************************
* Function Name: bsp_lcd_get_BGcolor
********************************************************************************
* Summary:
* Returns the current background color.
*
* Parameters:
*  void
*
* Return:
*  uint32_t: Current background color value
*******************************************************************************/
uint32_t bsp_lcd_get_BGcolor()
{
    return LCD_Colors[0];
}


/*******************************************************************************
* Function Name: ifx_lcd_set_FGcolor
********************************************************************************
* Summary:
* Sets the foreground color using 8-bit RGB values, forwarding to the BSP 
* function.
*
* Parameters:
*  uint8_t r: Red color value (0-255)
*  uint8_t g: Green color value (0-255)
*  uint8_t b: Blue color value (0-255)
*
* Return:
*  uint32_t: The set foreground color value
*******************************************************************************/
inline uint32_t ifx_lcd_set_FGcolor( uint8_t r, uint8_t g, uint8_t b )
{
    return bsp_lcd_set_FGcolor( r, g, b );
}


/*******************************************************************************
* Function Name: ifx_lcd_set_BGcolor
********************************************************************************
* Summary:
* Sets the background color using 8-bit RGB values, forwarding to the BSP function.
*
* Parameters:
*  uint8_t r: Red color value (0-255)
*  uint8_t g: Green color value (0-255)
*  uint8_t b: Blue color value (0-255)
*
* Return:
*  uint32_t: The set background color value
*******************************************************************************/
inline uint32_t ifx_lcd_set_BGcolor( uint8_t r, uint8_t g, uint8_t b )
{
    return bsp_lcd_set_BGcolor( r, g, b );
}


/*******************************************************************************
* Function Name: ifx_lcd_get_FGcolor
********************************************************************************
* Summary:
* Returns the current foreground color, forwarding to the BSP function.
*
* Parameters:
*  void
*
* Return:
*  uint32_t: Current foreground color value
*******************************************************************************/
inline uint32_t ifx_lcd_get_FGcolor()
{
    return bsp_lcd_get_FGcolor();
}


/*******************************************************************************
* Function Name: ifx_lcd_get_BGcolor
********************************************************************************
* Summary:
* Returns the current background color, forwarding to the BSP function.
*
* Parameters:
*  void
*
* Return:
*  uint32_t: Current background color value
*******************************************************************************/
inline uint32_t ifx_lcd_get_BGcolor()
{
    return bsp_lcd_get_BGcolor();
}


CY_SECTION_ITCM_BEGIN
/*******************************************************************************
* Function Name: bsp_lcd_draw_Pixel
********************************************************************************
* Summary:
* Draws a single pixel at the specified coordinates with the given RGB color.
* Assumes the coordinates are valid within the display boundaries.
*
* Parameters:
*  uint16_t x: X-coordinate of the pixel
*  uint16_t y: Y-coordinate of the pixel
*  uint32_t rgbColor: 16-bit RGB565 or 32-bit XRGB color value
*
* Return:
*  void
*******************************************************************************/
void bsp_lcd_draw_Pixel( uint16_t x, uint16_t y, uint32_t rgbColor )
{
    if (Display_Width == 0 || Display_Height == 0) return;

    LCD_Addr[INDEX2D(y, x, Display_Width)] = rgbColor;
}
CY_SECTION_ITCM_END


CY_SECTION_ITCM_BEGIN
/*******************************************************************************
* Function Name: bsp_lcd_draw_H_Line
********************************************************************************
* Summary:
* Draws a horizontal line from x0 to x1 at y0 with the specified RGB color.
* Assumes x0 <= x1 and coordinates are valid within the display boundaries.
*
* Parameters:
*  uint16_t x0: Starting X-coordinate
*  uint16_t y0: Y-coordinate of the line
*  uint16_t x1: Ending X-coordinate
*  uint32_t rgbColor: 16-bit RGB565 or 32-bit XRGB color value
*
* Return:
*  void
*******************************************************************************/
void bsp_lcd_draw_H_Line( uint16_t x0, uint16_t y0, uint16_t x1, uint32_t rgbColor )
{
    if (Display_Width == 0 || Display_Height == 0) return;

    uint32_t   kk = INDEX2D(y0, x0, Display_Width);

    for ( uint32_t x = x0; x <= x1; x++ )
        LCD_Addr[kk++] = rgbColor;
}
CY_SECTION_ITCM_END


CY_SECTION_ITCM_BEGIN
/*******************************************************************************
* Function Name: bsp_lcd_draw_V_Line
********************************************************************************
* Summary:
* Draws a vertical line from y0 to y1 at x0 with the specified RGB color.
* Assumes y0 <= y1 and coordinates are valid within the display boundaries.
*
* Parameters:
*  uint16_t x0: X-coordinate of the line
*  uint16_t y0: Starting Y-coordinate
*  uint16_t y1: Ending Y-coordinate
*  uint32_t rgbColor: 16-bit RGB565 or 32-bit XRGB color value
*
* Return:
*  void
*******************************************************************************/
void bsp_lcd_draw_V_Line( uint16_t x0, uint16_t y0, uint16_t y1, uint32_t rgbColor )
{
    if (Display_Width == 0 || Display_Height == 0) return;

    uint32_t    kk = INDEX2D(y0, x0, Display_Width);

    for ( uint32_t y = y0; y <= y1; y++, kk += Display_Width )
        LCD_Addr[kk] = rgbColor;
}
CY_SECTION_ITCM_END

/*******************************************************************************
* High-level drawing functions
*******************************************************************************/

/*******************************************************************************
* Function Name: ifx_lcd_draw_Pixel
********************************************************************************
* Summary:
* Draws a single pixel at the specified coordinates with the given color.
* Assumes the coordinates are valid within the display boundaries.
*
* Parameters:
*  uint16_t x: X-coordinate of the pixel
*  uint16_t y: Y-coordinate of the pixel
*  uint32_t color: Foreground or background color value
*
* Return:
*  void
*******************************************************************************/
inline
void ifx_lcd_draw_Pixel( uint16_t x, uint16_t y, uint32_t color )
{
    bsp_lcd_draw_Pixel( x, y, color );
}


CY_SECTION_ITCM_BEGIN
/*******************************************************************************
* Function Name: ifx_lcd_draw_H_Line
********************************************************************************
* Summary:
* Draws a horizontal line from x0 to x1 at y0 using the current foreground color.
* Clips the line to the display boundaries.
*
* Parameters:
*  int16_t x0: Starting X-coordinate
*  int16_t y0: Y-coordinate of the line
*  int16_t x1: Ending X-coordinate
*
* Return:
*  void

******************************************************************************/
void ifx_lcd_draw_H_Line( int16_t x0, int16_t y0, int16_t x1 )
{
    if (Display_Width == 0 || Display_Height == 0) return;

    int min_x = min(x0, x1);
    int max_x = max(x0, x1);

    /* check if the whole line is out of the display */
    if ( max_x < 0  ||  y0 < 0  ||  Display_Width <= min_x  ||  Display_Height <= y0 )
        return;

    uint32_t    fgColor = ifx_lcd_get_FGcolor();

    bsp_lcd_draw_H_Line( (uint16_t) max(min_x, 0), (uint16_t) y0, (uint16_t) min(max_x, Display_Width - 1), fgColor );
}
CY_SECTION_ITCM_END


CY_SECTION_ITCM_BEGIN
/*******************************************************************************
* Function Name: ifx_lcd_draw_V_Line
********************************************************************************
* Summary:
* Draws a vertical line from y0 to y1 at x0 using the current foreground color.
* Clips the line to the display boundaries.
*
* Parameters:
*  int16_t x0: X-coordinate of the line
*  int16_t y0: Starting Y-coordinate
*  int16_t y1: Ending Y-coordinate
*
* Return:
*  void
*******************************************************************************/
void ifx_lcd_draw_V_Line( int16_t x0, int16_t y0, int16_t y1 )
{
    if (Display_Width == 0 || Display_Height == 0) return;

    int min_y = min(y0, y1);
    int max_y = max(y0, y1);

    /* check if the whole line is out of the display*/
    if ( x0 < 0  ||  max_y < 0  || Display_Width <= x0  ||  Display_Height <= min_y )
        return;

    uint32_t    fgColor = ifx_lcd_get_FGcolor();

    bsp_lcd_draw_V_Line( (uint16_t) x0, (uint16_t) max(min_y, 0), (uint16_t) min(max_y, Display_Height - 1), fgColor );
}
CY_SECTION_ITCM_END


CY_SECTION_ITCM_BEGIN
/*******************************************************************************
* Function Name: ifx_lcd_draw_Line
********************************************************************************
* Summary:
* Draws a line from (x0, y0) to (x1, y1) using the current foreground color.
* Handles vertical, horizontal, and diagonal lines, clipping to display 
* boundaries.
*
* Parameters:
*  int16_t x0: Starting X-coordinate
*  int16_t y0: Starting Y-coordinate
*  int16_t x1: Ending X-coordinate
*  int16_t y1: Ending Y-coordinate
*
* Return:
*  void
*******************************************************************************/
void ifx_lcd_draw_Line( int16_t x0, int16_t y0, int16_t x1, int16_t y1 )
{
    if (Display_Width == 0 || Display_Height == 0) return;

    int min_x = min(x0, x1);
    int max_x = max(x0, x1);
    int min_y = min(y0, y1);
    int max_y = max(y0, y1);
    uint32_t    fgColor = ifx_lcd_get_FGcolor();
    int16_t     tmp;

    /* check if the whole line is out of the display */
    if ( max_x < 0  ||  max_y < 0  ||  Display_Width <= min_x  ||  Display_Height <= min_y )
        return;

    if ( x0 == x1 ) {
        if ( y0 == y1 ) {
            ifx_lcd_draw_Pixel( (uint16_t)x0, (uint16_t)y0, fgColor );
        } else {
            /* vertical line */
            min_y = max(min_y, 0);
            max_y = min(max_y, Display_Height - 1);
            bsp_lcd_draw_V_Line( (uint16_t) x0, (uint16_t) min_y, (uint16_t) max_y, fgColor );
        }
        return;
    } else if ( y0 == y1 ) {
        /* horizontal line */
        min_x = max(min_x, 0);
        max_x = min(max_x, Display_Width - 1);
        bsp_lcd_draw_H_Line( (uint16_t) min_x, (uint16_t) y0, (uint16_t) max_x, fgColor );
        return;
    }

    if ( max_x - min_x >= max_y - min_y ) {     /* (line length in X) >= (line length in Y) */
        if ( x0 > x1 ) {
            swap( x0, x1, tmp );
            swap( y0, y1, tmp );
        }

        float    dx = x1 - x0;
        float    dy = y1 - y0;
        float    k  = dy / dx;
        float    fy = y0 + 0.5f;
        uint16_t    min_x = x0;
        uint16_t    max_x = min( x1, Display_Width - 1 );

        if ( x0 < 0 ) {
            /* move to the valid display area in X */
            fy -= k * x0;
            min_x = 0;
        }
        for ( uint16_t ix = min_x; ix <= max_x; ix++, fy += k ) {
            int16_t iy = fy;
            if ( 0 <= iy  &&  iy < Display_Height )
                ifx_lcd_draw_Pixel( ix, (uint16_t)iy, fgColor );
        }
    } else {
        if ( y0 > y1 ) {
            swap( x0, x1, tmp );
            swap( y0, y1, tmp );
        }

        float    dx = x1 - x0;
        float    dy = y1 - y0;
        float    k  = dx / dy;
        float    fx = x0 + 0.5f;
        uint16_t    min_y = y0;
        uint16_t    max_y = min( y1, Display_Height - 1 );

        if ( y0 < 0 ) {
            /* move to the valid display area in Y */
            fx -= k * y0;
            min_y = 0;
        }
        for ( uint16_t iy = min_y; iy <= max_y; iy++, fx += k ) {
            int16_t ix = fx;
            if ( 0 <= ix  &&  ix < Display_Width )
                ifx_lcd_draw_Pixel( (uint16_t)ix, iy, fgColor );
        }
    }
}
CY_SECTION_ITCM_END


CY_SECTION_ITCM_BEGIN
/*******************************************************************************
* Function Name: ifx_lcd_draw_Rect
********************************************************************************
* Summary:
* Draws a rectangle outline defined by opposite corners (x0, y0) and (x1, y1)
* using the current foreground color. Clips the rectangle to the display 
* boundaries.
*
* Parameters:
*  int16_t x0: X-coordinate of one corner
*  int16_t y0: Y-coordinate of one corner
*  int16_t x1: X-coordinate of the opposite corner
*  int16_t y1: Y-coordinate of the opposite corner
*
* Return:
*  void
*******************************************************************************/
void ifx_lcd_draw_Rect( int16_t x0, int16_t y0, int16_t x1, int16_t y1 )
{
    if (Display_Width == 0 || Display_Height == 0) return;

    int min_x = min(x0, x1);
    int max_x = max(x0, x1);
    int min_y = min(y0, y1);
    int max_y = max(y0, y1);

    /* check if the whole line is out of the display*/
    if ( max_x < 0  ||  max_y < 0  ||  Display_Width <= min_x  ||  Display_Height <= min_y )
        return;

    uint32_t    fgColor = ifx_lcd_get_FGcolor();

    bsp_lcd_draw_H_Line( min_x, min_y, max_x, fgColor );
    bsp_lcd_draw_V_Line( min_x, min_y, max_y, fgColor );
    bsp_lcd_draw_H_Line( min_x, max_y, max_x, fgColor );
    bsp_lcd_draw_V_Line( max_x, min_y, max_y, fgColor );
}
CY_SECTION_ITCM_END


CY_SECTION_ITCM_BEGIN
/*******************************************************************************
* Function Name: ifx_lcd_draw_Bitmap
********************************************************************************
* Summary:
* Draws a monochrome bitmap at the specified coordinates using foreground and
* background colors. Each bit in the bitmap determines whether to use the
* foreground or background color.
*
* Parameters:
*  int16_t x: X-coordinate of the top-left corner
*  int16_t y: Y-coordinate of the top-left corner
*  const char*bitmap: Pointer to the bitmap data
*  uint16_t width: Width of the bitmap in pixels
*  uint16_t height: Height of the bitmap in pixels
*
* Return:
*  void
*******************************************************************************/
void ifx_lcd_draw_Bitmap( int16_t x, int16_t y, const char *bitmap, uint16_t width, uint16_t height )
{
    if (Display_Width == 0 || Display_Height == 0) return;

    if ( x < 0  ||  Display_Width  < x + width  ||
            y < 0  ||  Display_Height < y + height )
        return;

    uint32_t    fgColor = ifx_lcd_get_FGcolor();
    uint32_t    bgColor = ifx_lcd_get_BGcolor();
    const char  *pBitmap = bitmap;
    int         width_byte = width / 8;     /* round down to BYTE unit */

    for ( int j = 0; j < width_byte; j++, x += 8 ) {
        for ( int pos = 0; pos < height; pos++ ) {
            uint8_t byte = (uint8_t) *pBitmap++;

            for ( int t = 0; t < 8; t++ ) {
                uint32_t    color = ( (byte << t) & 0x80u ) ? fgColor : bgColor;

                ifx_lcd_draw_Pixel( x + t, y + pos, color );
            }
        }
    }
    if ( width % 8 ) {
        for ( int pos = 0; pos < height; pos++ ) {
            uint8_t byte = (uint8_t) *pBitmap++;

            for ( int t = 0; t < width % 8; t++ ) {
                uint32_t    color = ( (byte << t) & 0x80u ) ? fgColor : bgColor;

                ifx_lcd_draw_Pixel( x + t, y + pos, color );
            }
        }
    }
}
CY_SECTION_ITCM_END

/*******************************************************************************
* Display image functions
*******************************************************************************/

/*******************************************************************************
* Function Name: bsp_lcd_display_Rect
********************************************************************************
* Summary:
* Copies a rectangular area from a 24-bit RGB source image to the specified
* location on the display, converting to the display's color format (RGB565 
* or XRGB32).
*
* Parameters:
*  uint16_t x0: X-coordinate of the top-left corner on the display
*  uint16_t y0: Y-coordinate of the top-left corner on the display
*  uint8_t *image: Pointer to the 24-bit RGB source image
*  uint16_t width: Width of the source image
*  uint16_t height: Height of the source image
*
* Return:
*  void
*******************************************************************************/
void bsp_lcd_display_Rect( uint16_t x0, uint16_t y0, uint8_t *image, uint16_t width, uint16_t height )
{
    if (Display_Width == 0 || Display_Height == 0) return;

    /* check if the whole line is out of the display */
    if ( x0 >= Display_Width  ||  y0 >= Display_Height )
        return;

    uint32_t    h = min( height, Display_Height - y0 );
    uint32_t    w = min( width,  Display_Width - x0 );
    uint8_t     *pSrc = image;
    LCD_TYPE_t  *pDst = &LCD_Addr[INDEX2D(y0, x0, Display_Width)];

    for ( uint32_t y = 0; y < h; y++ ) 
    {
        uint8_t *ps = pSrc;

        for ( uint32_t x = 0; x < w; x++ ) 
        {
            uint32_t    r = (uint32_t)(*ps++);
            uint32_t    g = (uint32_t)(*ps++);
            uint32_t    b = (uint32_t)(*ps++);

            pDst[x] =
#ifndef LCD_COLOR_XRGB32
(uint16_t)ifx_pixel_RGB888_to_RGB565(r, g, b);
#else
            (uint32_t)ifx_pixel_RGB888_to_RGBX32( r, g, b );
#endif  /* LCD_COLOR_XRGB32 */
        }
        pDst += Display_Width;
        pSrc += width * 3;
    }
}


/*******************************************************************************
* Function Name: bsp_lcd_display_Rect_i8
********************************************************************************
* Summary:
* Copies a rectangular area from an int8-normalized 24-bit RGB source image to
* the specified location on the display, converting to the display's color format
* (RGB565 or XRGB32) after offsetting by 128.
*
* Parameters:
*  uint16_t x0: X-coordinate of the top-left corner on the display
*  uint16_t y0: Y-coordinate of the top-left corner on the display
*  int8_t *image_i8: Pointer to the int8-normalized source image
*  uint16_t width: Width of the source image
*  uint16_t height: Height of the source image
*
* Return:
*  void
*******************************************************************************/
void bsp_lcd_display_Rect_i8( uint16_t x0, uint16_t y0, int8_t *image_i8, uint16_t width, uint16_t height )
{
    if (Display_Width == 0 || Display_Height == 0) return;

    /* check if the whole line is out of the display */
    if ( x0 >= Display_Width  ||  y0 >= Display_Height )
        return;

    uint32_t    h = min( height, Display_Height - y0 );
    uint32_t    w = min( width,  Display_Width - x0 );
    int8_t      *pSrc = image_i8;
    LCD_TYPE_t  *pDst = &LCD_Addr[INDEX2D(y0, x0, Display_Width)];

    for ( uint32_t y = 0; y < h; y++ ) {
        int8_t      *ps = pSrc;

        for ( uint32_t x = 0; x < w; x++ ) {
            uint32_t    r = (uint32_t)(*ps++ + 128);
            uint32_t    g = (uint32_t)(*ps++ + 128);
            uint32_t    b = (uint32_t)(*ps++ + 128);

            pDst[x] =
#ifndef LCD_COLOR_XRGB32
(uint16_t)ifx_pixel_RGB888_to_RGB565(r, g, b);
#else
            (uint32_t)ifx_pixel_RGB888_to_RGBX32( r, g, b );
#endif  /* LCD_COLOR_XRGB32 */
        }
        pDst += Display_Width;
        pSrc += width * 3;
    }
}


/*******************************************************************************
* Function Name: bsp_lcd_display_Rect_ui8
********************************************************************************
* Summary:
* Copies a rectangular area from a uint8-normalized 24-bit RGB source image to
* the specified location on the display, converting to the display's color format
* (RGB565 or XRGB32).
*
* Parameters:
*  uint16_t x0: X-coordinate of the top-left corner on the display
*  uint16_t y0: Y-coordinate of the top-left corner on the display
*  uint8_t *image_ui8: Pointer to the uint8-normalized source image
*  uint16_t width: Width of the source image
*  uint16_t height: Height of the source image
*
* Return:
*  void
*******************************************************************************/
void bsp_lcd_display_Rect_ui8( uint16_t x0, uint16_t y0, uint8_t *image_ui8, uint16_t width, uint16_t height )
{
    if (Display_Width == 0 || Display_Height == 0) return;

    /* check if the whole line is out of the display */
    if ( x0 >= Display_Width  ||  y0 >= Display_Height )
        return;

    uint32_t    h = min( height, Display_Height - y0 );
    uint32_t    w = min( width,  Display_Width - x0 );
    uint8_t      *pSrc = image_ui8;
    LCD_TYPE_t  *pDst = &LCD_Addr[y0 * Display_Width + x0];

    for ( uint32_t y = 0; y < h; y++ ) {
        uint8_t      *ps = pSrc;

        for ( uint32_t x = 0; x < w; x++ ) {
            uint32_t    r = (uint32_t)(*ps++);
            uint32_t    g = (uint32_t)(*ps++);
            uint32_t    b = (uint32_t)(*ps++);

            pDst[x] =
#ifndef LCD_COLOR_XRGB32
(uint16_t)ifx_pixel_RGB888_to_RGB565(r, g, b);
#else
            (uint32_t)ifx_pixel_RGB888_to_RGBX32( r, g, b );
#endif  /* LCD_COLOR_XRGB32 */
        }
        pDst += Display_Width;
        pSrc += width * 3;
    }
}


/*******************************************************************************
* Function Name: bsp_lcd_display_Rect_scale_i8
********************************************************************************
* Summary:
* Copies a rectangular area from an int8-normalized source image to the specified
* location on the display with an integer scale factor (>= 1), converting to the
* display's color format (RGB565 or XRGB32) after offsetting by 128.
*
* Parameters:
*  uint16_t x0: X-coordinate of the top-left corner on the display
*  uint16_t y0: Y-coordinate of the top-left corner on the display
*  int8_t *pSrc: Pointer to the int8-normalized source image
*  uint16_t width: Width of the source image
*  uint16_t height: Height of the source image
*  uint16_t scale: Integer scale factor (>= 1)
*
* Onderwerp: Re: Request for code review and optimization suggestions
* Return:
*  void
*******************************************************************************/
void bsp_lcd_display_Rect_scale_i8( uint16_t x0, uint16_t y0, int8_t *pSrc, uint16_t width, uint16_t height, uint16_t scale )
{
    if (Display_Width == 0 || Display_Height == 0) return;

    uint32_t    h = min( height, (Display_Height - y0) / scale );
    uint32_t    w = min( width,  (Display_Width - x0) / scale );

    for ( uint32_t y = 0; y < h; y++ ) {
        int8_t      *ps = &pSrc[y * width * 3];
        LCD_TYPE_t  *pd = &LCD_Addr[(y0 + y * scale) * Display_Width + x0];

        for ( uint32_t x = 0; x < w; x++ ) {
            uint32_t    r = (uint32_t)(*ps++ + 128);
            uint32_t    g = (uint32_t)(*ps++ + 128);
            uint32_t    b = (uint32_t)(*ps++ + 128);
            uint32_t    color =
#ifndef LCD_COLOR_XRGB32
                    ifx_pixel_RGB888_to_RGB565(r, g, b);
#else
            ifx_pixel_RGB888_to_RGBX32( r, g, b );
#endif  /* LCD_COLOR_XRGB32 */

            for ( int ky = 0; ky < scale; ky++ )
                for ( int kx = 0; kx < scale; kx++ )
                    pd[INDEX2D(ky, kx, Display_Width)] = color;

            pd += scale;
        }
    }
}


/*******************************************************************************
* Function Name: ifx_lcd_display_Rect
********************************************************************************
* Summary:
* Copies a rectangular area from a 24-bit RGB source image to the specified
* location on the display, forwarding to the BSP function.
*
* Parameters:
*  uint16_t x0: X-coordinate of the top-left corner on the display
*  uint16_t y0: Y-coordinate of the top-left corner on the display
*  uint8_t *image: Pointer to the 24-bit RGB source image
*  uint16_t width: Width of the source image
*  uint16_t height: Height of the source image
*
* Return:
*  void
*******************************************************************************/
inline void ifx_lcd_display_Rect( uint16_t x0, uint16_t y0, uint8_t *image, uint16_t width, uint16_t height )
{
    bsp_lcd_display_Rect( x0, y0, image, width, height );
}


/*******************************************************************************
* Function Name: ifx_lcd_display_Rect_i8
********************************************************************************
* Summary:
* Copies a rectangular area from an int8-normalized source image to the specified
* location on the display, forwarding to the BSP function.
*
* Parameters:
*  uint16_t x0: X-coordinate of the top-left corner on the display
*  uint16_t y0: Y-coordinate of the top-left corner on the display
*  int8_t *image_i8: Pointer to the int8-normalized source image
*  uint16_t width: Width of the source image
*  uint16_t height: Height of the source image
*
* Return:
*  void
*******************************************************************************/
inline void ifx_lcd_display_Rect_i8( uint16_t x0, uint16_t y0, int8_t *image_i8, uint16_t width, uint16_t height )
{
    bsp_lcd_display_Rect_i8( x0, y0, image_i8, width, height );
}


/*******************************************************************************
* Function Name: ifx_lcd_display_Rect_ui8
********************************************************************************
* Summary:
* Copies a rectangular area from a uint8-normalized source image to the specified
* location on the display, forwarding to the BSP function.
*
* Parameters:
*  uint16_t x0: X-coordinate of the top-left corner on the display
*  uint16_t y0: Y-coordinate of the top-left corner on the display
*  uint8_t *image_ui8: Pointer to the uint8-normalized source image
*  uint16_t width: Width of the source image
*  uint16_t height: Height of the source image
*
* Return:
*  void
*******************************************************************************/
inline void ifx_lcd_display_Rect_ui8( uint16_t x0, uint16_t y0, uint8_t *image_ui8, uint16_t width, uint16_t height )
{
    bsp_lcd_display_Rect_ui8( x0, y0, image_ui8, width, height );
}


/*******************************************************************************
* Function Name: ifx_lcd_display_Rect_scale_i8
********************************************************************************
* Summary:
* Copies a rectangular area from an int8-normalized source image to the specified
* location on the display with an integer scale factor (>= 1), forwarding to the
* BSP function.
*
* Parameters:
*  uint16_t x0: X-coordinate of the top-left corner on the display
*  uint16_t y0: Y-coordinate of the top-left corner on the display
*  int8_t *image_i8: Pointer to the int8-normalized source image
*  uint16_t width: Width of the source image
*  uint16_t height: Height of the source image
*  uint16_t scale: Integer scale factor (>= 1)
*
* Return:
*  void
*******************************************************************************/
inline void ifx_lcd_display_Rect_scale_i8( uint16_t x0, uint16_t y0, int8_t *image_i8, uint16_t width, uint16_t height, uint16_t scale )
{
    bsp_lcd_display_Rect_scale_i8( x0, y0, image_i8, width, height, scale );
}


/*******************************************************************************
* Function Name: ifx_lcd_draw_HeadPoseAxes
********************************************************************************
* Summary:
* Draws three axes representing head pose (yaw, pitch, roll) at the specified
* coordinates, using red (X), green (Y), and blue (Z) colors. Includes endpoint
* markers and restores the original foreground color to white.
*
* Parameters:
*  int16_t x: X-coordinate of the origin point
*  int16_t y: Y-coordinate of the origin point
*  int16_t yaw: Yaw angle in degrees
*  int16_t pitch: Pitch angle in degrees
*  int16_t roll: Roll angle in degrees
*
* Return:
*  void
*******************************************************************************/
void ifx_lcd_draw_HeadPoseAxes(int16_t x, int16_t y, int16_t yaw, int16_t pitch, int16_t roll)
{
    /* Convert angles to radians */
    float yaw_rad = -yaw * PI / 180.0f;
    float pitch_rad = pitch * PI / 180.0f;
    float roll_rad = roll * PI / 180.0f;

    /* The size parameter now represents the maximum length of an axis when it's pointing directly away from the camera */
    float size = 75.0f;

    /* Calculate sine and cosine values for efficiency */
    float cos_yaw = cosf(yaw_rad);
    float sin_yaw = sinf(yaw_rad);
    float cos_pitch = cosf(pitch_rad);
    float sin_pitch = sinf(pitch_rad);
    float cos_roll = cosf(roll_rad);
    float sin_roll = sinf(roll_rad);

    /* Calculate 3D rotation matrix components for each axis endpoint */
    /* X-axis (typically red) */
    float x1 = x + size * (cos_yaw * cos_roll);
    float y1 = y + size * (cos_pitch * sin_roll + cos_roll * sin_pitch * sin_yaw);

    /* Y-axis (typically green) */
    float x2 = x + size * (-cos_yaw * sin_roll);
    float y2 = y + size * (cos_pitch * cos_roll - sin_pitch * sin_yaw * sin_roll);

    /* Z-axis (typically blue) */
    float x3 = x + size * sin_yaw;
    float y3 = y + size * (-cos_yaw * sin_pitch);

    /* Draw X-axis in red */
    ifx_lcd_set_FGcolor(255, 0, 0);  /* Red */
    ifx_lcd_draw_Line(x, y, (int16_t)x1, (int16_t)y1);

    /* Draw Y-axis in green */
    ifx_lcd_set_FGcolor(0, 255, 0);  /* Green */
    ifx_lcd_draw_Line(x, y, (int16_t)x2, (int16_t)y2);

    /* Draw Z-axis in blue */
    ifx_lcd_set_FGcolor(0, 0, 255);  /* Blue */
    ifx_lcd_draw_Line(x, y, (int16_t)x3, (int16_t)y3);

    /* Optional: Draw small circles at the end of each axis for better visibility */
    /* X-axis endpoint */
    ifx_lcd_set_FGcolor(255, 0, 0);
    ifx_lcd_draw_Pixel((int16_t)x1, (int16_t)y1, ifx_lcd_get_FGcolor());

    /* Y-axis endpoint */
    ifx_lcd_set_FGcolor(0, 255, 0);
    ifx_lcd_draw_Pixel((int16_t)x2, (int16_t)y2, ifx_lcd_get_FGcolor());

    /* Z-axis endpoint */
    ifx_lcd_set_FGcolor(0, 0, 255);
    ifx_lcd_draw_Pixel((int16_t)x3, (int16_t)y3, ifx_lcd_get_FGcolor());

    /* Restore original foreground color
     Note: We would need the original RGB values to restore properly
     For now, we'll set it back to white (common default) */
    ifx_lcd_set_FGcolor(255, 255, 255);
}


/*******************************************************************************
* Function Name: ifx_lcd_draw_FacialLandmarks
********************************************************************************
* Summary:
* Draws five facial landmarks as 3x3 pixel squares with distinct colors (red,
* green, blue, yellow, magenta) at the specified coordinates.
*
* Parameters:
*  int16_t landmarks[10]: Array of 10 values [x1,x2,x3,x4,x5,y1,y2,y3,y4,y5]
*                         representing 5 landmark points
*
* Return:
*  void
*******************************************************************************/
void ifx_lcd_draw_FacialLandmarks(int16_t landmarks[10])
{
    if (Display_Width == 0 || Display_Height == 0) return;
    if (landmarks == NULL) return; /* xxxxxyyyyy */

    /* Define colors for each landmark (bright, distinct colors) */
    /* Using RGB values that work well on both RGB565 and XRGB32 formats */
    static const struct {
        uint8_t r, g, b;
    } landmark_colors[5] = {
            {255, 0, 0},    /* Red - Landmark 0 (typically left eye) */
            {0, 255, 0},    /* Green - Landmark 1 (typically right eye) */
            {0, 0, 255},    /* Blue - Landmark 2 (typically nose) */
            {255, 255, 0},  /* Yellow - Landmark 3 (typically left mouth corner) */
            {255, 0, 255}   /* Magenta - Landmark 4 (typically right mouth corner) */
    };

    /* Fixed 3x3 square size (side length = 3) */

    /* Draw each of the 5 landmarks */
    for (int i = 0; i < 5; i++) {
        int16_t x = landmarks[i];         /* X coordinates: [0-4] */
        int16_t y = landmarks[i + 5];     /* Y coordinates: [5-9] */

        /* Check if landmark center is within valid bounds for 3x3 square */
        if (x < 1 || x >= (Display_Width - 1) ||
                y < 1 || y >= (Display_Height - 1))
            continue;

        /* Set color for this landmark */
        uint32_t color =
#ifndef LCD_COLOR_XRGB32
        ifx_pixel_RGB888_to_RGB565(landmark_colors[i].r,
                landmark_colors[i].g,
                landmark_colors[i].b);
#else
        ifx_pixel_RGB888_to_RGBX32(landmark_colors[i].r,
                landmark_colors[i].g,
                landmark_colors[i].b);
#endif  /* LCD_COLOR_XRGB32 */

    /* Draw 3x3 filled square (optimized with direct pixel writes) */
    /* Row y-1 */
    bsp_lcd_draw_Pixel(x - 1, y - 1, color);
    bsp_lcd_draw_Pixel(x,     y - 1, color);
    bsp_lcd_draw_Pixel(x + 1, y - 1, color);

    /* Row y (center) */
    bsp_lcd_draw_Pixel(x - 1, y, color);
    bsp_lcd_draw_Pixel(x,     y, color);
    bsp_lcd_draw_Pixel(x + 1, y, color);

    /* Row y+1 */
    bsp_lcd_draw_Pixel(x - 1, y + 1, color);
    bsp_lcd_draw_Pixel(x,     y + 1, color);
    bsp_lcd_draw_Pixel(x + 1, y + 1, color);
    }
}

/* [] END OF FILE */
