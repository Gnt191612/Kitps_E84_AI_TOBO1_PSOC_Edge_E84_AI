/*********************************************************************
*                   (c) SEGGER Microcontroller GmbH                  *
*                        The Embedded Experts                        *
**********************************************************************
*                                                                    *
*       (c) 2003 - 2025     SEGGER Microcontroller GmbH              *
*                                                                    *
*       www.segger.com     Support: www.segger.com/ticket            *
*                                                                    *
**********************************************************************
*                                                                    *
*       emUSB-Host * USB Host stack for embedded applications        *
*                                                                    *
*       Please note: Knowledge of this file may under no             *
*       circumstances be used to write a similar product.            *
*       Thank you for your fairness !                                *
*                                                                    *
**********************************************************************
*                                                                    *
*       emUSB-Host version: V2.48.1                                  *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
All Intellectual Property rights in the software belongs to SEGGER.
emUSB Host is protected by international copyright laws. This file
may only be used in accordance with the following terms:

The source code of the emUSB Host software has been licensed to Cypress
Semiconductor Corporation, whose registered office is 198 Champion
Court, San Jose, CA 95134, USA including the 
right to create and distribute the object code version of 
the emUSB Host software for its Cortex M0, M0+, M4, M33 and M55 based devices.
The object code version can be used by Cypress customers under the 
terms and conditions of the associated End User License Agreement.
Support for the object code version is provided by Cypress, 
full source code is available at: www.segger.com

We appreciate your understanding and fairness.
----------------------------------------------------------------------
Licensing information
Licensor:                 SEGGER Microcontroller Systems LLC
Licensed to:              Cypress Semiconductor Corp, 198 Champion Ct., San Jose, CA 95134, USA
Licensed SEGGER software: emUSB-Host
License number:           USBH-00303
License model:            Cypress Services and License Agreement, signed November 17th/18th, 2010
                          and Amendment Number One, signed December 28th, 2020 and February 10th, 2021
                          and Amendment Number Three, signed May 2nd, 2022 and May 5th, 2022 and Amendment Number Four, signed August 28th, 2023 
Licensed platform:        Cypress devices containing ARM Cortex M cores: M0, M0+, M4, M33 and M55
----------------------------------------------------------------------
Support and Update Agreement (SUA)
SUA period:               2022-05-12 - 2026-05-19
Contact to extend SUA:    sales@segger.com
----------------------------------------------------------------------
File        : usbh_config_io.c
Purpose     : I/O routines and configuration for emUSB-Host.
---------------------------END-OF-HEADER------------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include <stdio.h>
#include "USBH.h"

#if defined (__CROSSWORKS_ARM)
    #include "__putchar.h"
#endif

/*********************************************************************
*
*       Defines, configurable
*
*       This section is normally the only section which requires
*       changes on most embedded systems.
*/

/* The standard output methods are enabled by default
 * To disable an output in Debug mode or provide a custom
 * output method, set USBH_DISABLE_STANDARD_OUTPUT=1 in DEFINES
 * variable in the application project Makefile
 */
#if !defined (USBH_DISABLE_STANDARD_OUTPUT)
    #define USBH_DISABLE_STANDARD_OUTPUT        (0U)
#endif /* #if !defined USBH_DISABLE_STANDARD_OUTPUT */

#if (USBH_DISABLE_STANDARD_OUTPUT == 0U)
/* Select one of the standard output methods */

/* Retarget-io middleware: https://github.com/Infineon/retarget-io */
#ifndef   USE_RETARGET_IO
    #define USE_RETARGET_IO                     (1U)
#endif

/* SEGGER's Real Time Terminal: https://www.segger.com/jlink-real-time-terminal.html */
#ifndef   USE_RTT
    #define USE_RTT                             (0U)
#endif

/* The Debug Communication Channel (DCC): https://wiki.segger.com/DCC */
#ifndef   USE_DCC
    #define USE_DCC                             (0U)
#endif

#endif /* #if (USBH_DISABLE_STANDARD_OUTPUT == 0U) */

#ifndef   SHOW_TIME
    #define SHOW_TIME                           (1U)
#endif

#ifndef   SHOW_TASK
    #define SHOW_TASK                           (1U)
    #include "cyabs_rtos.h"
#endif

#if (USE_RETARGET_IO == 1U)
    #include "cy_retarget_io.h"
#endif /* (USE_RETARGET_IO == 1U) */

#if (USE_RTT == 1U)
    #include "SEGGER_RTT.h"
#endif /* #if (USE_RTT == 1U) */

#if (USE_DCC == 1U)
    #include "JLINKDCC.h"
#endif /* #if (USE_DCC == 1U) */

/*********************************************************************
*
*       _puts
*
*  Function description
*    Local (static) replacement for puts.
*    The reason why puts is not used is that puts always appends
*    a new-line character, which screws up our formatting.
*
*  Parameters
*    s - Pointer to a string.
*/
static void _puts(const char * s) {
#if (USBH_DISABLE_STANDARD_OUTPUT == 1U)
    /* Add the custom output method */
    (void) s;
#else /* Standard debug output methods */
#if (USE_RETARGET_IO == 1U)
    printf("%s", s);
#elif (USE_RTT == 1U)
    SEGGER_RTT_WriteString(0, s);
#elif (USE_DCC == 1U)
    char c;

    for (;;)
    {
        c = *s++;
        if (c == 0)
        {
            break;
        }

        JLINKDCC_SendChar(c);
    }
#else /* Unspecified debug output method */
    #warning Unspecified debug output method. Select one of the available or set \
             USBH_DISABLE_STANDARD_OUTPUT=1 in the defines variable \
             of the application project Makefile
#endif /* #if (USE_RETARGET_IO == 1U) */
#endif /* #if (USBH_DISABLE_STANDARD_OUTPUT == 1U) */
}

/*********************************************************************
*
*       _WriteUnsigned
*
*  Function description
*    Prints an unsigned integer as a string.
*
*  Parameters
*    s          - Pointer to a string into which the value should be written.
*    v          - Integer value to print.
*    NumDigits  - Number of digits to print (0 for all digits).
*
*  Return value
*    s - Pointer to a string, same as the s parameter.
*/
#if SHOW_TIME
static char * _WriteUnsigned(char * s, U32 v, int NumDigits) {
    unsigned   Base;
    unsigned   Div;
    U32        Digit;
    Digit    = 1;
    Base     = 10;

    /* Count how many digits are required */
    while (((v / Digit) >= Base) || (NumDigits > 1))
    {
        NumDigits--;
        Digit *= Base;
    }

    /* Output digits */
    do
    {
        Div = v / Digit;
        v  -= Div * Digit;
        *s++ = (char)('0' + Div);
        Digit /= Base;
    } while (Digit);
    *s = 0;
    return s;
}
#endif

/*********************************************************************
*
*       _ShowStamp
*
*  Function description
*    Prints a time-stamp and the name of the task from which
*    the function was executed.
*
*/
static void _ShowStamp(void) {
#if SHOW_TIME
    U32    Time;
    char   ac[20];
    char * sBuffer = &ac[0];
    Time           = USBH_OS_GetTime32();
    sBuffer        = _WriteUnsigned(sBuffer, Time / 1000, 0);
    *sBuffer++     = ':';
    sBuffer        = _WriteUnsigned(sBuffer, Time % 1000, 3);
    *sBuffer++     = ' ';
    *sBuffer++     = 0;
    _puts(ac);
#endif

#if SHOW_TASK
{
    const char * s = NULL;
    cy_rslt_t result;
    cy_thread_t current_thread_handle;
    result = cy_rtos_thread_get_handle(&current_thread_handle);
    if (CY_RSLT_SUCCESS == result)
    {
        result = cy_rtos_thread_get_name(&current_thread_handle, &s);
        if (CY_RSLT_SUCCESS != result)
        {
            s = "ERROR: Unknown task name";
        }
    }
    else
    {
        s = "ERROR: Unknown task name";
    }

    if (s)
    {
        _puts(s);
        _puts(" - ");
    }
}
#endif
}

/*********************************************************************
*
*       USBH_Panic
*
*  Function description
*    Is called if the stack encounters a fatal error.
*
*  Parameters
*    s - Pointer to a string holding the error message.
*
*  Additional information
*    In a release build this function is not linked in. The default
*    implementation of this function disables all interrupts to avoid
*    further task switches, outputs the error string via terminal I/O
*    and loops forever. When using an emulator, you should set a
*    break-point at the beginning of this routine or simply stop the
*    program after a failure.
*/
void USBH_Panic(const char * s) {
/* Uncomment to enter critical section.
 * FreeRTOS API functions must not be called from within a critical
 * section. As a retarget-io uses mutex, the printf() can not be used
 * inside critical section.
 */
/*
 * USBH_OS_DisableInterrupt();
 */
#if (USBH_DEBUG > 1)
    _puts("*** Fatal error, System halted: ");
    _puts(s);
    _puts("\r\n");
#else
    (void)s;
#endif
    USBH_HALT;
}

/*********************************************************************
*
*       USBH_Log
*
*  Function description
*    This function is called by the stack in debug builds with log output.
*    In a release build, this function is not be linked in.
*
*  Parameters
*    s - Pointer to a string holding the log message.
*/
void USBH_Log(const char * s) {
/* Uncomment to enter critical section.
 * FreeRTOS API functions must not be called from within a critical
 * section. As a retarget-io uses mutex, the printf() can not be used
 * inside critical section.
 */
/*
 * USBH_OS_DisableInterrupt();
 */
    _ShowStamp();
    _puts(s);
    _puts("\r\n");
/* Uncomment to exit critical section.
 * FreeRTOS API functions must not be called from within a critical
 * section. As a retarget-io uses mutex, the printf() can not be used
 * inside critical section.
 */
/*
 * USBH_OS_EnableInterrupt();
 */
}

/*********************************************************************
*
*       USBH_Warn
*
*  Function description
*    This function is called by the stack in debug builds with log output.
*    In a release build, this function is not be linked in.
*
*  Parameters
*    s - Pointer to a string holding the warning message.
*/
void USBH_Warn(const char * s) {
/* Uncomment to enter critical section.
 * FreeRTOS API functions must not be called from within a critical
 * section. As a retarget-io uses mutex, the printf() can not be used
 * inside critical section.
 */
/*
 * USBH_OS_DisableInterrupt();
 */
    _ShowStamp();
    _puts("*** Warning *** ");
    _puts(s);
    _puts("\r\n");
/* Uncomment to exit critical section.
 * FreeRTOS API functions must not be called from within a critical
 * section. As a retarget-io uses mutex, the printf() can not be used
 * inside critical section.
 */
/*
 * USBH_OS_EnableInterrupt();
 */
}

/*********************************************************************
*
*       USBH_Puts
*
*  Function description
*    Prints a string without any additional output (no timestamp or newlines).
*
*  Parameters
*    s - Pointer to a string holding the warning message.
*/
void USBH_Puts(const char * s) {
/* Uncomment to enter critical section.
 * FreeRTOS API functions must not be called from within a critical
 * section. As a retarget-io uses mutex, the printf() can not be used
 * inside critical section.
 */
/*
 * USBH_OS_DisableInterrupt();
 */
    _puts(s);
/* Uncomment to exit critical section.
 * FreeRTOS API functions must not be called from within a critical
 * section. As a retarget-io uses mutex, the printf() can not be used
 * inside critical section.
 */
/*
 * USBH_OS_EnableInterrupt();
 */
}

/*************************** End of file ****************************/
