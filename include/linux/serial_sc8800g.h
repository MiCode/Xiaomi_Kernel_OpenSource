/* include/linux/serial_sc8800g.h
 *
 * Copyright (C) 2012 NVIDIA Corporation
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _SERIAL_SC8800G_H_
#define _SERIAL_SC8800G_H_

enum {
	AP_RTS,
	AP_RDY,
	AP_RESEND,
	AP_TO_MDM1,
	AP_TO_MDM2,
	MDM_EXTRSTN,
	MDM_PWRON,
	BP_PWRON,
	MDM_RTS,
	MDM_RDY,
	MDM_RESEND,
	MDM_TO_AP1,
	MDM_TO_AP2,
	MDM_ALIVE,
	USB_SWITCH,
	SC8800G_GPIO_MAX
};

struct serial_sc8800g_platform_data {
	struct gpio gpios[SC8800G_GPIO_MAX];
};

enum power_state {
	PWROFF = 0,
	PWRING,
	ACTIVE,
	CRASH,
	PWR_PRE,
};

struct serial_sc8800g_pwr_state {
	enum power_state power_state;
	char *power_state_env[2];
};

enum offline_log_state {
	OFFLINE_LOG_OFF = 0,
	OFFLINE_LOG_ON = 1,
};

struct serial_sc8800g_offline_log_state {
	enum offline_log_state state;
	char *offline_log_state_env[2];
};

struct serial_sc8800g_usb_switch_state {
	char *usb_switch_state_env[2];
};

#endif /* _SERIAL_SC8800G_H_ */
