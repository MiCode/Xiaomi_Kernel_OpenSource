/* Copyright (c) 2015, 2017-2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_SWR_WCD_H
#define _LINUX_SWR_WCD_H
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/bitops.h>

enum {
	SWR_CH_MAP,
	SWR_DEVICE_DOWN,
	SWR_DEVICE_UP,
	SWR_SUBSYS_RESTART,
	SWR_SET_NUM_RX_CH,
	SWR_CLK_FREQ,
	SWR_DEVICE_SSR_DOWN,
	SWR_DEVICE_SSR_UP,
	SWR_REGISTER_WAKE_IRQ,
};

struct swr_mstr_port {
	int num_port;
	u8 *port;
};

#define MCLK_FREQ		9600000
#define MCLK_FREQ_NATIVE	11289600

#if (IS_ENABLED(CONFIG_SOUNDWIRE_WCD_CTRL) || \
	IS_ENABLED(CONFIG_SOUNDWIRE_MSTR_CTRL))
extern int swrm_wcd_notify(struct platform_device *pdev, u32 id, void *data);
#else /* CONFIG_SOUNDWIRE_WCD_CTRL */
static inline int swrm_wcd_notify(struct platform_device *pdev, u32 id,
				  void *data)
{
	return 0;
}
#endif /* CONFIG_SOUNDWIRE_WCD_CTRL */
#endif /* _LINUX_SWR_WCD_H */
