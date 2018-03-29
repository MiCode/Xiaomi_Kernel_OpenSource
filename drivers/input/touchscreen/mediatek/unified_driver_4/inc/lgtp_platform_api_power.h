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

#if !defined(_LGTP_PLATFORM_API_POWER_H_)
#define _LGTP_PLATFORM_API_POWER_H_

/****************************************************************************
* Nested Include Files
****************************************************************************/
#include <lgtp_common_driver.h>


/****************************************************************************
* Mainfest Constants / Defines
****************************************************************************/


/****************************************************************************
* Type Definitions
****************************************************************************/


/****************************************************************************
* Exported Variables
****************************************************************************/


/****************************************************************************
* Macros
****************************************************************************/


/****************************************************************************
* Global Function Prototypes
****************************************************************************/

#if defined(TOUCH_PLATFORM_QCT)
void TouchPowerPMIC(int isOn, char *id, int min_uV, int max_uV);
#endif

#if defined(TOUCH_PLATFORM_MTK)
void TouchPowerPMIC(int isOn, int pin, int vol);
#endif

void TouchPowerSetGpio(int pin, int value);
void TouchSetGpioDirection(int pin, int value);


#endif				/* _LGTP_PLATFORM_API_POWER_H_ */

/* End Of File */
