/*******************************************************************************
* File Name        : lcd_print.c
*
* Description      : This file implements functions for rendering text on the LCD display.
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
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "lcd_graphics.h"
#include "font_16x36.h"

/*******************************************************************************
* Macros
*******************************************************************************/
/* Local text buffer size */
#define LCD_PRINTF_BUF_SIZE 128
/* Margin for text alignment on the display */
#define DISPLAY_SIDE_MARGIN  10

/* GUI data container */
typedef struct
{
    int     x;
    int     y;
    int     chars;
    char    printbuffer[LCD_PRINTF_BUF_SIZE];
} LCD_PrintBuffer_t;

/* GUI font configuration structure */
typedef struct
{
    char        *name;
    const char  *data;
    uint8_t     x_size;
    uint8_t     y_size;
    uint8_t     font_size_bytes;
} LCD_font_t;

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Allocated GUI data container */
static LCD_PrintBuffer_t Print_buf = { 0, 0, 0 };

/* Allocated GUI font configuration structure */
static LCD_font_t _gFontTbl[] =
{{ "Font", consolas_12ptBitmaps_16x36, FONT_X_SIZE, FONT_Y_SIZE, ((FONT_X_SIZE + 7) / 8) * FONT_Y_SIZE }, };

/*******************************************************************************
* Functions
*******************************************************************************/

/*******************************************************************************
* Function Name: ifx_lcd_printfToBuffer
********************************************************************************
* Summary:
*  Writes the C string pointed by format to local text buffer.
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
int ifx_lcd_printfToBuffer(int x, int y, const char *format, ...)
{
    Print_buf.x = x;
    Print_buf.y = y;

    va_list ap;
    va_start(ap, format);

    Print_buf.chars = vsprintf(Print_buf.printbuffer, format, ap);

    va_end(ap);

    return Print_buf.chars;
}


/*******************************************************************************
* Function Name: draw_Char
********************************************************************************
* Summary:
* Draws a single character at the specified coordinates using the configured 
* font. Skips control characters (ASCII < 32).
*
* Parameters:
*  int x: X-coordinate for drawing the character
*  int y: Y-coordinate for drawing the character
*  char c: Character to draw
*
* Return:
*  void
*******************************************************************************/
static void draw_Char(int x, int y, char c)
{
    if (c < ' ') {
        return;
    }

    c = c - ' ';

    int         base = c * _gFontTbl[0].font_size_bytes;
    const char  *bitmap = &_gFontTbl[0].data[base];

    ifx_lcd_draw_Bitmap(x, y, bitmap, _gFontTbl[0].x_size, _gFontTbl[0].y_size);
}


/*******************************************************************************
* Function Name: ifx_lcd_draw_Buffer
********************************************************************************
* Summary:
* Draws the contents of the local text buffer to the LCD display, handling
* alignment (left, center, right) if the X-coordinate is negative.
*
* Parameters:
*  void
*
* Return:
*  void
*******************************************************************************/
void ifx_lcd_draw_Buffer()
{
    int x = Print_buf.x;

    if (x < 0) {
        int string_width  = Print_buf.chars * _gFontTbl[0].x_size;
        int display_width = ifx_lcd_get_Display_Width();

        switch (x) {
        case ALIGN_LEFT:
            x = DISPLAY_SIDE_MARGIN;
            break;

        case ALIGN_CENTER:
            x = (display_width - string_width) / 2;
            break;

        case ALIGN_RIGHT:
            x = display_width - string_width - DISPLAY_SIDE_MARGIN - 30;
            break;

        default:
            break;
        }
        x = (x >= 0) ? x : DISPLAY_SIDE_MARGIN;
    }

    for (int i = 0; i < Print_buf.chars; i++, x += _gFontTbl[0].x_size) {
        draw_Char(x, Print_buf.y, Print_buf.printbuffer[i]);
    }
}


/*******************************************************************************
* Function Name: ifx_lcd_printf
********************************************************************************
* Summary:
* Writes a formatted C string to the LCD display at the specified coordinates
* by storing it in the local text buffer and immediately drawing it.
*
* Parameters:
*  int x: X-coordinate for drawing the text
*  int y: Y-coordinate for drawing the text
*  const char *format: Format string for the text
*  ...: Variable arguments for the format string
*
* Return:
*  void
*******************************************************************************/
void ifx_lcd_printf(int x, int y, const char *format, ...)
{
    /* ifx_lcd_printfToBuffer */
    Print_buf.x = x;
    Print_buf.y = y;

    va_list ap;
    va_start(ap, format);

    Print_buf.chars = vsprintf(Print_buf.printbuffer, format, ap);

    va_end(ap);

    ifx_lcd_draw_Buffer();
}

/* [] END OF FILE */
