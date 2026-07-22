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
File        : usbh_config.c
Purpose     : emUSB-Host configuration file for PSE84 device
---------------------------END-OF-HEADER------------------------------
*/

#include "USBH.h"
#include "USBH_HW_Infineon_PSOCE84.h"
#include "mtb_hal.h"

/* The size of memory (in bytes) dedicated to the stack. The size
 * of memory is selected to satisfy the RAM usage of the driver and
 * of a few connected devices. Update this value with the optimal
 * memory pool size (strongly recommended) for the application.
 * For details on selecting the optimal memory pool size, refer
 * to the emUSB-Host User Guide & Reference Manual Memory pools
 * chapter.
 **/
#define USBH_ALLOC_SIZE                         (0x8000U)
/* Define interrupt priority */
#define USBH_ISR_PRIO                           (3U)

/* The define pin, which controls the power distribution to a USB device
 * through VBUS line.
 */
#if !defined (USBH_POWER_CONTROL_PIN_NUMBER)
#define USBH_POWER_CONTROL_PIN_NUMBER           5
#endif /* #if !defined (USBH_POWER_CONTROL_PIN_NUMBER) */

#if !defined (USBH_POWER_CONTROL_PORT_NUMBER)
#define USBH_POWER_CONTROL_PORT_NUMBER          17
#endif /* #if !defined (USBH_POWER_CONTROL_PORT_NUMBER) */


/* These macros define the lower and upper limits of SRAM regions for
 * the verification that memory assigned for the DMA driver is allocated
 * in the region with DMA access. These macros are device-specific.
 * Update them for a specific device.
 */
#define DMA_VALID_ADDR_START                    (0x24000000U)
#define DMA_VALID_ADDR_END                      (0x40000000U)

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
/* The memory area used by the stack */
#if defined (COMPONENT_CM55)
/* For CM55, the default linker scripts do not locate the USB memory pool
 * to the area with DMA access. So, specify manually the memory area with
 * DMA access for the USB memory pool. */
CY_SECTION(".cy_socmem_data") static U32 memory_pool[USBH_ALLOC_SIZE / 4U];
#else
static U32 memory_pool[USBH_ALLOC_SIZE / 4U];
#endif /* #if defined (COMPONENT_CM55) */

static bool vbus_control_pin_is_init = false;

/* Define the categories of debug log messages to be printed.
 * For possible categories, see the USBH_MCAT_... definitions in USBH.h
 */
static const U8 log_categories[] =
{
    USBH_MCAT_INIT,
    USBH_MCAT_APPLICATION
};

static mtb_hal_gpio_t vbus_control_pin;


/*********************************************************************
*
*       clean_dcache
*
*  Function description
*    Flush all dirty cache lines to memory, that cover memory range 'ptr' to 'ptr + num_bytes - 1'.
*    (num_bytes may need to be rounded up to a multiple of the cache line size).
*/
static void clean_dcache(void * ptr, unsigned long num_bytes)
{
#if defined (COMPONENT_CM55)
    SCB_CleanDCache_by_Addr(ptr, num_bytes);
#else
    /* Do nothing */
    (void) ptr;
    (void) num_bytes;
#endif /* #if defined (COMPONENT_CM55) */
}


/*********************************************************************
*
*       invalidate_dcache
*
*  Function description
*    Invalidate all cache lines  (discard contents) that cover memory range 'ptr' to 'ptr + num_bytes - 1'.
*    (num_bytes may need to be rounded up to a multiple of the cache line size).
*/
static void invalidate_dcache(void * ptr, unsigned long num_bytes)
{
#if defined (COMPONENT_CM55)
    SCB_InvalidateDCache_by_Addr(ptr, num_bytes);
#else
    /* Do nothing */
    (void) ptr;
    (void) num_bytes;
#endif /* #if defined (COMPONENT_CM55) */
}


static const SEGGER_CACHE_CONFIG cache_config =
{
    /* CacheLineSize of CPU */
    32,
    /* Data Memory Barrier is not present */
    NULL,
    clean_dcache,
    invalidate_dcache
};


/*********************************************************************
*
*       isr
*
*  Function description
*    The USB interrupt handler.
*
*/
static void isr(void)
{
    USBH_ServiceISR(0);
}


/*********************************************************************
*
*       on_port_power_control
*
*  Function description
*    A callback set by USBH_SetOnSetPortPower() is called when the
*    port power changes.
*
*/
static void on_port_power_control(U32 HostControllerIndex, U8 Port, U8 PowerOn) {
    USBH_USE_PARA(HostControllerIndex);
    USBH_USE_PARA(Port);

    if (PowerOn != 0u)
    {
        mtb_hal_gpio_write(&vbus_control_pin, true);
    }
    else
    {
        mtb_hal_gpio_write(&vbus_control_pin, false);
    }
}


/*********************************************************************
*
*       init_power_control_pin
*
*  Function description
*    Initializes the pin, which controls the power distribution to a
*    USB device through VBUS line.
*
*/
void init_power_control_pin()
{
    if (!vbus_control_pin_is_init)
    {
        mtb_hal_gpio_setup(&vbus_control_pin, USBH_POWER_CONTROL_PORT_NUMBER, USBH_POWER_CONTROL_PIN_NUMBER);
        mtb_hal_gpio_configure(&vbus_control_pin, MTB_HAL_GPIO_DIR_OUTPUT, MTB_HAL_GPIO_DRIVE_STRONG);

        vbus_control_pin_is_init = true;
    }
}


/*********************************************************************
*
*       check_valid_dma_address
*
*  Function description
*    Checks if the memory location can be used for DMA transfers.
*
*/
static int check_valid_dma_address(const void *mem_addr)
{
    int status = 1;
    if (((uint32_t)mem_addr >= DMA_VALID_ADDR_START) && ((uint32_t)mem_addr < DMA_VALID_ADDR_END))
    {
        status = 0;
    }
    return status;
}


/*********************************************************************
*
*       USBH_X_Config
*
*  Function description
*    Initializes USB hardware and configures the USB-Host stack. This function
*    is called by the startup code of the emUSB-Host stack from USBH_Init().
*    This is the place where a hardware driver can be added and configured.
*
*/
void USBH_X_Config(void)
{
    mtb_hal_hf_clock_t hf_clk8;
    U32 usb_ref_clk;

    init_power_control_pin();

    /* Assigning memory */
    USBH_AssignMemory((void *) &memory_pool, USBH_ALLOC_SIZE);

    /* Allow external hubs */
    USBH_ConfigSupportExternalHubs(1U);

    /* Wait 300ms for a new connected device */
    USBH_ConfigPowerOnGoodTime(300U);

    /* Define the log and warn filters */
    /* Output all warnings */
    USBH_ConfigMsgFilter(USBH_WARN_FILTER_SET_ALL, 0, NULL);
    /* Set message categories in log filter */
    USBH_ConfigMsgFilter(USBH_LOG_FILTER_SET, sizeof(log_categories), log_categories);

    /* Set cache configuration for USBH stack. */
    USBH_SetCacheConfig(&cache_config, sizeof(cache_config));
    /* check usb phy ref clk (CLK_HF8) freq and set */
    hf_clk8.inst_num = (U32)8;
    usb_ref_clk = mtb_hal_clock_get_hf_clock_freq(&hf_clk8);
    /*we support 24 and 50Mhz ref clk only, default is 50Mhz*/
    if(usb_ref_clk == (U32)24000000)
    {
        USBH_Infineon_PSOCE84_DMA_SetPLLRefClock((U32)24);
    }
    else
    {
        USBH_Infineon_PSOCE84_DMA_SetPLLRefClock((U32)50);
    }

    USBH_Infineon_PSOCE84_DMA_Add((void*)0x44900000UL);
    USBH_Infineon_PSOCE84_SetCheckAddress(check_valid_dma_address);

    USBH_SetOnSetPortPower(on_port_power_control);

    /* Install the interrupt service routine */
    cy_stc_sysint_t usb_int_cfg =
    {
        .intrSrc = usbhs_interrupt_usbhsctrl_IRQn,
        .intrPriority = USBH_ISR_PRIO
    };

    cy_en_sysint_status_t status;

    /* Install the interrupt service routine */
    status = Cy_SysInt_Init(&usb_int_cfg, isr);
    CY_ASSERT(CY_RSLT_SUCCESS == status);
    (void) status; /* To avoid the compiler warning in Release mode */

    /* Enable USB interrupt */
    NVIC_EnableIRQ(usbhs_interrupt_usbhsctrl_IRQn);
}
