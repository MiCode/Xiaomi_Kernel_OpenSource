
/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __MHL_API_H__
#define __MHL_API_H__

#ifdef CONFIG_TEGRA_HDMI_MHL
bool mhl_is_connected(void);

struct mhl_platform_data {
	uint32_t mhl_gpio_reset;
	uint32_t mhl_gpio_wakeup;
	int (*power_setup) (int on);
	void (*reset) (int on);
	int (*set_vbuspower) (struct device *, int on);
#if defined(CONFIG_TEGRA_HDMI_MHL_RCP)
	int *mhl_key_codes;
	int mhl_key_num;
#endif
};

#if defined(CONFIG_TEGRA_HDMI_MHL_RCP)
void rcp_report_event(unsigned int type, unsigned int code, int value);
#endif
#else
static bool mhl_is_connected(void)
{
	return false;
}
#endif

#endif /* __MHL_API_H__ */
