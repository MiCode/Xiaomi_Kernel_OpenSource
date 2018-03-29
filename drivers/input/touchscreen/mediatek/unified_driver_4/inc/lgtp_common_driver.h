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
 *    File  : lgtp_common_driver.h
 *    Author(s)   : D3 BSP Touch Team < d3-bsp-touch@lge.com >
 *    Description :
 *
 ***************************************************************************/

#if !defined(_LGTP_COMMON_DRIVER_H_)
#define _LGTP_COMMON_DRIVER_H_

/****************************************************************************
* Nested Include Files
****************************************************************************/

#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <asm/unaligned.h>
#include <linux/cdev.h>
#include <linux/async.h>


/****************************************************************************
* Mainfest Constants / Defines
****************************************************************************/
#define KERNEL_ABOVE_3_4_67

#include <lgtp_project_setting.h>

#if defined(TOUCH_PLATFORM_MTK)
#if defined(TOUCH_PLATFORM_MT6735P) && defined(CONFIG_USE_OF)
#include <mach/wd_api.h>
#include <upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mt_gpio.h>
#include <mach/mt_gpt.h>
#include <mt_boot.h>
#include <mach/gpio_const.h>
#include <linux/irqchip/mt-eic.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#else
#include <mach/wd_api.h>
#include <mach/eint.h>
#include <mach/mt_wdt.h>
#include <mach/mt_gpt.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <cust_eint.h>
#include <mach/board.h>
#include <mach/board_lge.h>
#endif
#include "tpd.h"
#endif

#if defined(TOUCH_PLATFORM_QCT)
#if !defined(TOUCH_PLATFORM_MSM8936)
#include <mach/board.h>
#endif
#include <mach/board_lge.h>
#endif

/**********************************************************
* Driver Capability
**********************************************************/


/****************************************************************************
* Type Definitions
****************************************************************************/
typedef enum {
	LPWG_CMD_UNKNOWN = 0,

	LPWG_CMD_MODE = 1,
	LPWG_CMD_LCD_PIXEL_SIZE = 2,
	LPWG_CMD_ACTIVE_TOUCH_AREA = 3,
	LPWG_CMD_TAP_COUNT = 4,
	LPWG_CMD_TAP_DISTANCE = 5,
	LPWG_CMD_LCD_STATUS = 6,
	LPWG_CMD_PROXIMITY_STATUS = 7,
	LPWG_CMD_FIRST_TWO_TAP = 8,
	LPWG_CMD_UPDATE_ALL = 9,
	LPWG_CMD_CALL = 10,
} LpwgCmd;

typedef enum {
	LCD_EVENT_EARLY_BLANK = 0,
	LCD_EVENT_BLANK,
	LCD_EVENT_EARLY_UNBLANK,
	LCD_EVENT_UNBLANK,
	TA_EVENT_STATUS_CHANGE,
	BATTERY_EVENT_LEVEL_CHANGE,
	TEMPERATURE_EVENT_CHANGE,
} TouchNotifyCall;


/****************************************************************************
* Exported Variables
****************************************************************************/


/****************************************************************************
* Macros
****************************************************************************/

/****************************************************************************
* Global Function Prototypes
****************************************************************************/



#endif				/* _LGTP_COMMON_DRIVER_H_ */

/* End Of File */
