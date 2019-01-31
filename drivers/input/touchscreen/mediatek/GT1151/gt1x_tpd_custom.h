/* drivers/input/touchscreen/gt1x_tpd_custom.h
 *
 * 2010 - 2016 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * Version: 1.6   
 * Release Date:  2016/07/28
 */

#ifndef GT1X_TPD_CUSTOM_H__
#define GT1X_TPD_CUSTOM_H__

#include <asm/uaccess.h>
#ifdef CONFIG_MTK_BOOT
#include "mtk_boot_common.h"
#endif
//#include <linux/rtpm_prio.h>
//#include <mach/mt_pm_ldo.h>
//#include <mtk_thermal_typedefs.h>
//#include <mach/mt_boot.h>
#ifndef MT6589
#include <linux/gpio.h>
#endif
//#include <cust_eint.h>
#include "tpd.h"
#include "upmu_common.h"//hwPowerOn/hwPowerDown
//#include "cust_gpio_usage.h"
//#include <pmic_drv.h>

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
#define GTP_MTK_LEGACY
#endif

#define PLATFORM_MTK
#define TPD_I2C_NUMBER		        1

#ifdef CONFIG_MTK_I2C_EXTENSION
#define TPD_SUPPORT_I2C_DMA         1	/* if gt9l, better enable it if hardware platform supported*/
#else
#define TPD_SUPPORT_I2C_DMA         0
#endif

#if defined(CONFIG_MTK_LEGACY)
#define TPD_POWER_SOURCE_CUSTOM	MT6328_POWER_LDO_VGP1
#endif

#define GTP_GPIO_AS_INT(pin) tpd_gpio_as_int(pin)
#define GTP_GPIO_OUTPUT(pin, level) tpd_gpio_output(pin, level)

#ifdef MT6589
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
#define mt_eint_mask mt65xx_eint_mask
#define mt_eint_unmask mt65xx_eint_unmask
#endif

#define IIC_MAX_TRANSFER_SIZE         8
#define IIC_DMA_MAX_TRANSFER_SIZE     250
#define I2C_MASTER_CLOCK              300

#define TPD_MAX_RESET_COUNT           3

#define TPD_HAVE_CALIBRATION
#define TPD_CALIBRATION_MATRIX        {962,0,0,0,1600,0,0,0};

extern void tpd_on(void);
extern void tpd_off(void);

#endif /* GT1X_TPD_CUSTOM_H__ */
