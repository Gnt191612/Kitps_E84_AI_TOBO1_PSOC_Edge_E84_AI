/*******************************************************************************
* File Name        : ifx_gui_render.h
*
* Description      : Header file for rendering text and pixels on the LCD display.
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

#ifndef _IFX_GUI_RENDER_H_
#define _IFX_GUI_RENDER_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdint.h>
#include "ifx_image_utils.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define DISPLAY_HEIGHT                             (480U)
#define DISPLAY_WIDTH                              (832U)
#define IMAGE_DRAW_PIXEL(LCDBUF, X, Y, R, G, B) \
        draw_image_pixel(LCDBUF, X, Y, R, G, B, DISPLAY_WIDTH, DISPLAY_HEIGHT)
/* Local text buffer size. */
#define IFX_PRINTF_BUF_SIZE                        (64U)

/* Color constants */
#define DEFAULT_FG_COLOR_WHITE                     (0X00FFFFFF)
#define DEFAULT_BG_COLOR_BLACK                     (0U)
#define COLOR_MASK_24BIT                           (0X00FFFFFF)

/* Font rendering constants */
#define FONT_BYTES_PER_PIXEL                       (2U)
#define FONT_CHAR_OFFSET                           (' ')
#define FONT_BITS_PER_BYTE                         (8U)
#define FONT_PIXEL_INCREMENT                       (2U)
#define FONT_X_INCREMENT_OFFSET                    (0U)

/* Reset/Initialization value macros */
#define RESET_VALUE_INDEX                          (0U)

/******************************************************************************
* Function prototype
******************************************************************************/
void ifx_draw_buffer(uint16_t *lcd_buf);
int ifx_print_to_buffer(int x, int y, const char *format, ...);
void ifx_set_fg_color( int color );
void ifx_set_bg_color( int color );

#if defined(__cplusplus)
}
#endif /* __cplusplus*/

#endif

/* [] END OF FILE */