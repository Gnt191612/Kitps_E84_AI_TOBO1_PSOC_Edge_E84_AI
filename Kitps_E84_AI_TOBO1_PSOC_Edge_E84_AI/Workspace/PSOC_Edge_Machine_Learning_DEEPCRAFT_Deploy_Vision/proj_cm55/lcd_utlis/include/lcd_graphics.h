/*******************************************************************************
* File Name        : lcd_graphics.h
*
* Description      : This is the header file of LCD and graphics related 
*                    functions.
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

#ifndef _LCD_GRAPHICS_H_
#define _LCD_GRAPHICS_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdint.h>



/*******************************************************************************
* Macros
*******************************************************************************/
#ifndef INDEX3D
#define INDEX3D(y, x, c, width, channel)    (((y) * (width) + (x)) * (channel) + (c))
#define INDEX2D(y, x, width)                ((y) * (width) + (x))
#endif  /* INDEX3D */
#ifndef max
#define    max(a, b)    ((a) > (b) ? (a) : (b))
#define    min(a, b)    ((a) < (b) ? (a) : (b))
#endif  /* max */

#ifndef swap
#define swap(a, b, t)   {t = a; a = b; b = t;}
#endif  /* swap */

/* Alignment mode */
#define ALIGN_LEFT      (-1)
#define ALIGN_CENTER    (-2)
#define ALIGN_RIGHT     (-3)

#ifndef LCD_COLOR_XRGB32
#define LCD_TYPE_t  uint16_t
#else
#define LCD_TYPE_t  uint32_t
#endif  /* LCD_COLOR_XRGB32 */

/* makes plotting a little easier */
#define LCD_Addr  ((LCD_TYPE_t *) render_target->memory)
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

/******************************************************************************
* Function prototype
******************************************************************************/
void ifx_lcd_set_Display_size( uint32_t width, uint32_t height );
uint32_t ifx_lcd_get_Display_Width();
uint32_t ifx_lcd_get_Display_Height();

uint32_t ifx_lcd_set_FGcolor(uint8_t r, uint8_t g, uint8_t b );
uint32_t ifx_lcd_set_BGcolor(uint8_t r, uint8_t g, uint8_t b );
uint32_t ifx_lcd_get_FGcolor();
uint32_t ifx_lcd_get_BGcolor();
uint32_t bsp_lcd_set_FGcolor(uint16_t r, uint16_t g, uint16_t b );
uint32_t bsp_lcd_set_BGcolor(uint16_t r, uint16_t g, uint16_t b );
uint32_t bsp_lcd_get_FGcolor();
uint32_t bsp_lcd_get_BGcolor();

void ifx_lcd_draw_Pixel(uint16_t x, uint16_t y, uint32_t color );
void ifx_lcd_draw_Line(int16_t x0, int16_t y0, int16_t x1, int16_t y1 );
void ifx_lcd_draw_Rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1 );
void ifx_lcd_draw_Bitmap(int16_t x, int16_t y, const char *bitmap, uint16_t width, uint16_t height );
void ifx_lcd_display_Rect(uint16_t x0, uint16_t y0, uint8_t *image, uint16_t width, uint16_t height );
void ifx_lcd_display_Rect_i8(uint16_t x0, uint16_t y0, int8_t *image, uint16_t width, uint16_t height );
void ifx_lcd_display_Rect_ui8(uint16_t x0, uint16_t y0, uint8_t *image, uint16_t width, uint16_t height );
void ifx_lcd_display_Rect_scale_i8(uint16_t x0, uint16_t y0, int8_t *image, uint16_t width, uint16_t height, uint16_t scale );
void ifx_lcd_draw_Buffer();
int ifx_lcd_printfToBuffer(int x, int y, const char *format, ... );
void ifx_lcd_printf(int x, int y, const char *format, ...);
void ifx_lcd_draw_HeadPoseAxes(int16_t x, int16_t y, int16_t yaw, int16_t pitch, int16_t roll);
void ifx_lcd_draw_FacialLandmarks(int16_t landmarks[10]);
void bsp_lcd_draw_Pixel(uint16_t x, uint16_t y, uint32_t color );
void bsp_lcd_draw_H_Line(uint16_t x0, uint16_t y0, uint16_t x1, uint32_t rgbColor );
void bsp_lcd_draw_V_Line(uint16_t x0, uint16_t y0, uint16_t y1, uint32_t rgbColor );
void bsp_lcd_display_Rect(uint16_t x0, uint16_t y0, uint8_t *image, uint16_t width, uint16_t height );
void bsp_lcd_display_Rect_i8(uint16_t x0, uint16_t y0, int8_t *image_i8, uint16_t width, uint16_t height );
void bsp_lcd_display_Rect_ui8(uint16_t x0, uint16_t y0, uint8_t *image_ui8, uint16_t width, uint16_t height );
void bsp_lcd_display_Rect_scale_i8(uint16_t x0, uint16_t y0, int8_t *image_i8, uint16_t width, uint16_t height, uint16_t scale );

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* _LCD_GRAPHICS_H_ */

/* [] END OF FILE */