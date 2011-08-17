/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/types.h>
#include <linux/skbuff.h>

#ifndef _BAM_DMUX_H
#define _BAM_DMUX_H

enum {
	BAM_DMUX_DATA_RMNET_0,
	BAM_DMUX_DATA_RMNET_1,
	BAM_DMUX_DATA_RMNET_2,
	BAM_DMUX_DATA_RMNET_3,
	BAM_DMUX_DATA_RMNET_4,
	BAM_DMUX_DATA_RMNET_5,
	BAM_DMUX_DATA_RMNET_6,
	BAM_DMUX_DATA_RMNET_7,
	BAM_DMUX_USB_RMNET_0,
	BAM_DMUX_NUM_CHANNELS
};

int msm_bam_dmux_open(uint32_t id, void *priv,
		       void (*receive_cb)(void *, struct sk_buff *),
		       void (*write_done)(void *, struct sk_buff *));

int msm_bam_dmux_close(uint32_t id);

int msm_bam_dmux_write(uint32_t id, struct sk_buff *skb);

#endif /* _BAM_DMUX_H */
