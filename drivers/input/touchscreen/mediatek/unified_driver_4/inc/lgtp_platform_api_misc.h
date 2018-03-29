/***************************************************************************
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *    File  : lgtp_platform_api_misc.h
 *    Author(s)   : D3 BSP Touch Team < d3-bsp-touch@lge.com >
 *    Description :
 *
 ***************************************************************************/

#if !defined(_LGTP_PLATFORM_API_MISC_H_)
#define _LGTP_PLATFORM_API_MISC_H_

/****************************************************************************
* Nested Include Files
****************************************************************************/
#include <lgtp_common_driver.h>
#include <lgtp_model_config_misc.h>


/****************************************************************************
* Mainfest Constants / Defines
****************************************************************************/


/****************************************************************************
* Type Definitions
****************************************************************************/


/****************************************************************************
* Exported Variables
****************************************************************************/
extern int touch_module;

/****************************************************************************
* Macros
****************************************************************************/


/****************************************************************************
* Global Function Prototypes
****************************************************************************/
int TouchGetModuleIndex(void);
int TouchInitializeGpio(void);
void TouchSetGpioReset(int isHigh);
int TouchRegisterIrq(TouchDriverData *pDriverData, irq_handler_t irqHandler, irq_handler_t threaded_irqHandler);
void TouchEnableIrq(void);
void TouchDisableIrq(void);
int TouchReadGpioReset(void);
int TouchReadGpioInterrupt(void);
#if defined(TOUCH_DEVICE_LU201X) || defined(TOUCH_DEVICE_LU202X)
void TouchToggleGpioInterrupt(void);
#endif
int TouchGetBootMode(void);
int TouchReadByteReg(u16 addr, u8 *rxbuf);
int TouchWriteByteReg(u16 addr, u8 txbuf);
int TouchReadReg(u16 addr, u8 *rxbuf, int len);
int TouchWriteReg(u16 addr, u8 *rxbuf, int len);

#endif				/* _LGTP_PLATFORM_API_MISC_H_ */

/* End Of File */
