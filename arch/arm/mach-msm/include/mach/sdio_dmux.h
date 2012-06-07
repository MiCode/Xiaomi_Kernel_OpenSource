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

#ifndef _SDIO_DMUX_H
#define _SDIO_DMUX_H

#ifdef CONFIG_MSM_SDIO_DMUX
enum {
	SDIO_DMUX_DATA_RMNET_0,
	SDIO_DMUX_DATA_RMNET_1,
	SDIO_DMUX_DATA_RMNET_2,
	SDIO_DMUX_DATA_RMNET_3,
	SDIO_DMUX_DATA_RMNET_4,
	SDIO_DMUX_DATA_RMNET_5,
	SDIO_DMUX_DATA_RMNET_6,
	SDIO_DMUX_DATA_RMNET_7,
	SDIO_DMUX_USB_RMNET_0,
	SDIO_DMUX_NUM_CHANNELS
};

int msm_sdio_dmux_open(uint32_t id, void *priv,
		       void (*receive_cb)(void *, struct sk_buff *),
		       void (*write_done)(void *, struct sk_buff *));

int msm_sdio_is_channel_in_reset(uint32_t id);

int msm_sdio_dmux_close(uint32_t id);

int msm_sdio_dmux_write(uint32_t id, struct sk_buff *skb);

int msm_sdio_dmux_is_ch_full(uint32_t id);

int msm_sdio_dmux_is_ch_low(uint32_t id);

#else

static int __maybe_unused msm_sdio_dmux_open(uint32_t id, void *priv,
		       void (*receive_cb)(void *, struct sk_buff *),
		       void (*write_done)(void *, struct sk_buff *))
{
	return -ENODEV;
}

static int __maybe_unused msm_sdio_is_channel_in_reset(uint32_t id)
{
	return -ENODEV;
}

static int __maybe_unused msm_sdio_dmux_close(uint32_t id)
{
	return -ENODEV;
}

static int __maybe_unused msm_sdio_dmux_write(uint32_t id, struct sk_buff *skb)
{
	return -ENODEV;
}

static int __maybe_unused msm_sdio_dmux_is_ch_full(uint32_t id)
{
	return -ENODEV;
}

static int __maybe_unused msm_sdio_dmux_is_ch_low(uint32_t id)
{
	return -ENODEV;
}

#endif

#endif /* _SDIO_DMUX_H */
