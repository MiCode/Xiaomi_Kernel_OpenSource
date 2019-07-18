/* Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
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
#ifndef __BG_GLINK_H_
#define __BG_GLINK_H_

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>

typedef int (*bg_glink_cb_fn)(void *buf, int len);

struct bg_glink_ch_cfg {
	char ch_name[100];
	int num_of_intents;
	uint32_t *intents_size;
};

int bg_cdc_glink_write(void *ch_info, void *data,
		       int len);
void *bg_cdc_channel_open(struct platform_device *pdev,
			struct bg_glink_ch_cfg *ch_cfg,
			bg_glink_cb_fn func);
int bg_cdc_channel_close(struct platform_device *pdev,
			void *ch_info);
#endif
