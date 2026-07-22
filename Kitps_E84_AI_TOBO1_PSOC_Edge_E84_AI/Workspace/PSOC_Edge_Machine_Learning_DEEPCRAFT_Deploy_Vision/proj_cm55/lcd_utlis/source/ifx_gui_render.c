/*******************************************************************************
* File Name        : ifx_gui_render.c
*
* Description      : This file implements functions for rendering text and 
*                    pixels on the LCD display.
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
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "ifx_gui_render.h"
#include "font_16x36.h"
#include "ifx_image_utils.h"
#include <stddef.h>

/*******************************************************************************
* Macros
*******************************************************************************/
/* Macros for adjusting X and Y coordinates */
#define INCREMENT_X(X, Y) ((X) - (Y))
#define DECREMENT_X(X, Y) ((X) + (Y))
#define INCREMENT_Y(X, Y) ((X) - (Y))
#define DECREMENT_Y(X, Y) ((X) + (Y))

/*******************************************************************************
* Data Structures and Types
*******************************************************************************/
/* GUI data container */
typedef struct
{
    int     x;
    int     y;
    int     chars;
    uint16_t *lcdBuf;
    char printbuffer[IFX_PRINTF_BUF_SIZE];
} GUI_PrintStorage_t;

/* GUI font configuration structure */
typedef struct
{
    char        *name;
    uint8_t     x_size;
    uint8_t     y_size;
    const char  *data;
} chgui_font_t;

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Allocated GUI data container. */
static GUI_PrintStorage_t storage = { RESET_VALUE_INDEX, RESET_VALUE_INDEX, RESET_VALUE_INDEX, NULL };

/* Allocated GUI font configuration structure */
static chgui_font_t font_lib[] =
{
        {
                "Font",                       /* Font name */
                FONT_X_SIZE,                  /* Font character width */
                FONT_Y_SIZE,                  /* Font character height */
                consolas_12ptBitmaps_16x36    /* Font bitmap data */
        }
};

/* Foreground and background colors */
int FG_Color = DEFAULT_FG_COLOR_WHITE; /* Default foreground color: white */
int BG_Color = DEFAULT_BG_COLOR_BLACK; /* Default background color: black */

/*******************************************************************************
* Functions
*******************************************************************************/


/*******************************************************************************
* Function Name: ifx_print_to_buffer
********************************************************************************
* Summary:
* Writes a formatted C string to the local text buffer at the specified 
* coordinates.The content must be drawn using ifx_draw_buffer.
*
* Parameters:
*  int x: X-coordinate for drawing the text
*  int y: Y-coordinate for drawing the text
*  const char *format: Format string for the text
*  ...: Variable arguments for the format string
*
* Return:
*  int: Number of characters written to the buffer
*******************************************************************************/
int ifx_print_to_buffer(int x, int y, const char *format, ...)
{
    storage.x = x;
    storage.y = y;

    va_list ap;
    va_start(ap, format);

    storage.chars = vsprintf(storage.printbuffer, format, ap);

    va_end(ap);

    return storage.chars;
}


/*******************************************************************************
* Function Name: ifx_draw_pixel
********************************************************************************
* Summary:
* Draws a single pixel at the specified coordinates with the given RGB color.
*
* Parameters:
*  int color: 24-bit RGB color value
*  int x: X-coordinate for drawing the pixel
*  int y: Y-coordinate for drawing the pixel
*
* Return:
*  void
*******************************************************************************/
static void ifx_draw_pixel(int color, int x, int y)
{
    uint16_t r = (uint16_t)((color & 0xFF0000) >> 16);
    uint16_t g = (uint16_t)((color & 0x00FF00) >> 8);
    uint16_t b = (uint16_t)(color & 0x0000FF);

    IMAGE_DRAW_PIXEL(storage.lcdBuf, x, y, r, g, b);
}



/*******************************************************************************
* Function Name: _ifx_disp_char
********************************************************************************
* Summary:
*  Internal function to display a single character on the LCD at the specified 
*  coordinates using the provided font data and colors.
*
* Parameters:
*  char c: Character to display
*  int x: X coordinate
*  int y: Y coordinate
*  const char *pdata: Font data pointer
*  int font_xsize: Font X size
*  int font_ysize: Font Y size
*  int fcolor: Foreground RGB color
*  int bcolor: Background RGB color
*
* Return:
*  void
*******************************************************************************/
static void _ifx_disp_char(char c, int x, int y, const char *pdata,
        int font_xsize, int font_ysize, int fcolor, int bcolor)
{
    uint8_t j, pos, t;
    uint8_t temp;
    uint8_t XNum;
    uint32_t base;

    XNum = (font_xsize / 16) + 1;
    if (font_ysize % 18 == 0) {
        XNum--;
    }
    if (c < FONT_CHAR_OFFSET) {
        return;
    }
    c = c - FONT_CHAR_OFFSET;
    base = (c * XNum * font_ysize);

    for (j = RESET_VALUE_INDEX; j < XNum; j++) {
        for (pos = RESET_VALUE_INDEX; pos < font_ysize; pos = pos + FONT_PIXEL_INCREMENT) {
            temp = (uint8_t) pdata[base + pos + j * font_ysize];
            if (j < XNum) {
                for (t = RESET_VALUE_INDEX; t < FONT_BITS_PER_BYTE; t++) {
                    if ((temp >> t) & 0x01) {
                        ifx_draw_pixel(fcolor, INCREMENT_X(x, t), DECREMENT_Y(y, pos));
                    } else {
                        ifx_draw_pixel(bcolor, INCREMENT_X(x, t), DECREMENT_Y(y, pos));
                    }
                }
                temp = (uint8_t) pdata[1+ base + pos + j * font_ysize];
                for (t = RESET_VALUE_INDEX; t < FONT_BITS_PER_BYTE; t++) {
                    if ((temp >> t) & 0x01) {
                        ifx_draw_pixel(fcolor, INCREMENT_X(x, t-FONT_BITS_PER_BYTE), DECREMENT_Y(y, pos));
                    } else {
                        ifx_draw_pixel(bcolor, INCREMENT_X(x, t-FONT_BITS_PER_BYTE), DECREMENT_Y(y, pos));
                    }
                }
            }
        }
        x = INCREMENT_X(x, FONT_X_INCREMENT_OFFSET);
    }
}

/*******************************************************************************
* Function Name: ifx_disp_char
********************************************************************************
* Summary:
* Draws a single character at the specified coordinates using the default font
* and the current foreground and background colors.
*
* Parameters:
*  char c: Character to draw
*  int x: X-coordinate for drawing the character
*  int y: Y-coordinate for drawing the character
*
* Return:
*  void
*******************************************************************************/
static void ifx_disp_char(char c, int x, int y)
{
    _ifx_disp_char( c, x, y, font_lib[0].data, (int)font_lib[0].x_size,
            (int)font_lib[0].y_size, FG_Color, BG_Color );
}

/*******************************************************************************
* Function Name: ifx_set_fg_color
********************************************************************************
* Summary:
* Sets the global foreground color for text rendering, masking to 24-bit RGB.
*
* Parameters:
*  int color: 24-bit RGB color value
*
* Return:
*  void
*******************************************************************************/
void ifx_set_fg_color( int color )
{
    FG_Color = COLOR_MASK_24BIT & color;
}

/*******************************************************************************
* Function Name: ifx_draw_buffer
********************************************************************************
* Summary:
* Draws the contents of the local text buffer to the LCD display using the 
* specified LCD buffer. Each character is drawn at adjusted X-coordinates.
*
* Parameters:
*  uint16_t *lcd_buf: Pointer to the LCD buffer for drawing text
*
* Return:
*  void
*******************************************************************************/
void ifx_draw_buffer(uint16_t *lcd_buf)
{
    storage.lcdBuf = lcd_buf;
    int i;

    for (i = RESET_VALUE_INDEX; i < storage.chars; i++) {
        ifx_disp_char(storage.printbuffer[i],
                DECREMENT_X(storage.x, i * font_lib[0].x_size), storage.y);
    }
}

/*******************************************************************************
* Function Name: ifx_set_bg_color
********************************************************************************
* Summary:
* Sets the global background color for text rendering, masking to 24-bit RGB.
*
* Parameters:
*  int color: 24-bit RGB color value
*
* Return:
*  void
*******************************************************************************/
void ifx_set_bg_color(int color)
{
    BG_Color = COLOR_MASK_24BIT & color;
}

/* [] END OF FILE */
