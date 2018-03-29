/*
 * (C) Copyright 2009
 * MediaTek <www.MediaTek.com>
 *
 * MT6516 Sensor IOCTL & data structure
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __KPD_H__
#define __KPD_H__

#include <linux/ioctl.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <mt-plat/aee.h>
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
#include <linux/sbsuspend.h>	/* smartbook */
#endif
#include <linux/atomic.h>
#include <hal_kpd.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <mt-plat/mt_boot_common.h>

#define KEY_CALL	KEY_SEND
#define KEY_ENDCALL	KEY_END
#define KEY_FOCUS	KEY_HP

struct keypad_dts_data {
	u32 kpd_key_debounce;
	u32 kpd_sw_pwrkey;
	u32 kpd_hw_pwrkey;
	u32 kpd_sw_rstkey;
	u32 kpd_hw_rstkey;
	u32 kpd_use_extend_type;
	u32 kpd_hw_map_num;
	u32 kpd_hw_init_map[72];
	u32 kpd_pwrkey_eint_gpio;
	u32 kpd_pwrkey_gpio_din;
	u32 kpd_hw_dl_key1;
	u32 kpd_hw_dl_key2;
	u32 kpd_hw_dl_key3;
	u32 kpd_hw_recovery_key;
	u32 kpd_hw_factory_key;
};
extern struct keypad_dts_data kpd_dts_data;
#define KPD_NO 0
#define KPD_YES 1

#define KPD_AUTOTEST	KPD_NO
#define KPD_DEBUG	KPD_YES

#if KPD_AUTOTEST
#define PRESS_OK_KEY		_IO('k', 1)
#define RELEASE_OK_KEY		_IO('k', 2)
#define PRESS_MENU_KEY		_IO('k', 3)
#define RELEASE_MENU_KEY	_IO('k', 4)
#define PRESS_UP_KEY		_IO('k', 5)
#define RELEASE_UP_KEY		_IO('k', 6)
#define PRESS_DOWN_KEY		_IO('k', 7)
#define RELEASE_DOWN_KEY	_IO('k', 8)
#define PRESS_LEFT_KEY		_IO('k', 9)
#define RELEASE_LEFT_KEY	_IO('k', 10)
#define PRESS_RIGHT_KEY		_IO('k', 11)
#define RELEASE_RIGHT_KEY	_IO('k', 12)
#define PRESS_HOME_KEY		_IO('k', 13)
#define RELEASE_HOME_KEY	_IO('k', 14)
#define PRESS_BACK_KEY		_IO('k', 15)
#define RELEASE_BACK_KEY	_IO('k', 16)
#define PRESS_CALL_KEY		_IO('k', 17)
#define RELEASE_CALL_KEY	_IO('k', 18)
#define PRESS_ENDCALL_KEY	_IO('k', 19)
#define RELEASE_ENDCALL_KEY	_IO('k', 20)
#define PRESS_VLUP_KEY		_IO('k', 21)
#define RELEASE_VLUP_KEY	_IO('k', 22)
#define PRESS_VLDOWN_KEY	_IO('k', 23)
#define RELEASE_VLDOWN_KEY	_IO('k', 24)
#define PRESS_FOCUS_KEY		_IO('k', 25)
#define RELEASE_FOCUS_KEY	_IO('k', 26)
#define PRESS_CAMERA_KEY	_IO('k', 27)
#define RELEASE_CAMERA_KEY	_IO('k', 28)
#define PRESS_POWER_KEY		_IO('k', 30)
#define RELEASE_POWER_KEY	_IO('k', 31)
#endif
#define SET_KPD_KCOL		_IO('k', 29)

#define KPD_SAY		"kpd: "
#if KPD_DEBUG
#define kpd_print(fmt, arg...)	pr_err(KPD_SAY fmt, ##arg)
#define kpd_info(fmt, arg...)	pr_warn(KPD_SAY fmt, ##arg)
#else
#define kpd_print(fmt, arg...)	do {} while (0)
#define kpd_info(fmt, arg...)	do {} while (0)
#endif

/* the keys can wake up the system and we should enable backlight */
#define KPD_BACKLIGHT_WAKE_KEY	\
{				\
	KEY_ENDCALL, KEY_POWER,	\
}

#define KPD_HAS_SLIDE_QWERTY	0
#if KPD_HAS_SLIDE_QWERTY
static inline bool powerOn_slidePin_interface(void)
{
	return hwPowerOn(MT65XX_POWER_LDO_VCAM_IO, VOL_2800, "Qwerty slide");
}
static inline bool powerOff_slidePin_interface(void)
{
	return hwPowerDown(MT65XX_POWER_LDO_VCAM_IO, "Qwerty slide");
}
#endif
#ifdef CONFIG_KPD_PWRKEY_USE_PMIC
void kpd_pwrkey_pmic_handler(unsigned long pressed);
#else
static inline void kpd_pwrkey_pmic_handler(unsigned long data);
#endif
void kpd_pmic_rstkey_handler(unsigned long pressed);

#endif				/* __KPD_H__ */
