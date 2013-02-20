/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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

/*
 * SDIO-Abstraction-Layer API.
 */

#ifndef __SDIO_AL__
#define __SDIO_AL__

#include <linux/mmc/card.h>

struct sdio_channel; /* Forward Declaration */


/**
 *  Channel Events.
 *  Available bytes notification.
 */
#define SDIO_EVENT_DATA_READ_AVAIL      0x01
#define SDIO_EVENT_DATA_WRITE_AVAIL     0x02

#ifdef CONFIG_MSM_SDIO_AL

struct sdio_al_platform_data {
	int (*config_mdm2ap_status)(int);
	int (*get_mdm2ap_status)(void);
	int allow_sdioc_version_major_2;
	int peer_sdioc_version_minor;
	int peer_sdioc_version_major;
	int peer_sdioc_boot_version_minor;
	int peer_sdioc_boot_version_major;
};

/**
 * sdio_open - open a channel for read/write data.
 *
 * @name: channel name - identify the channel to open.
 * @ch: channel handle returned.
 * @priv: caller private context pointer, passed to the notify callback.
 * @notify: notification callback for data available.
 * @channel_event: SDIO_EVENT_DATA_READ_AVAIL or SDIO_EVENT_DATA_WRITE_AVAIL
 * @return 0 on success, negative value on error.
 *
 * Warning: notify() may be called before open returns.
 */
int sdio_open(const char *name, struct sdio_channel **ch, void *priv,
	     void (*notify)(void *priv, unsigned channel_event));


/**
 * sdio_close - close a channel.
 *
 * @ch: channel handle.
 * @return 0 on success, negative value on error.
 */
int sdio_close(struct sdio_channel *ch);

/**
 * sdio_read - synchronous read.
 *
 * @ch: channel handle.
 * @data: caller buffer pointer. should be non-cacheable.
 * @len: byte count.
 * @return 0 on success, negative value on error.
 *
 * May wait if no available bytes.
 * May wait if other channel with higher priority has pending
 * transfers.
 * Client should check available bytes prior to calling this
 * api.
 */
int sdio_read(struct sdio_channel *ch, void *data, int len);

/**
 * sdio_write - synchronous write.
 *
 * @ch: channel handle.
 * @data: caller buffer pointer. should be non-cacheable.
 * @len: byte count.
 * @return 0 on success, negative value on error.
 *
 * May wait if no available bytes.
 * May wait if other channel with higher priority has pending
 * transfers.
 * Client should check available bytes prior to calling this
 * api.
 */
int sdio_write(struct sdio_channel *ch, const void *data, int len);

/**
 * sdio_write_avail - get available bytes to write.
 *
 * @ch: channel handle.
 * @return byte count on success, negative value on error.
 */
int sdio_write_avail(struct sdio_channel *ch);

/**
 * sdio_read_avail - get available bytes to read.
 *
 * @ch: channel handle.
 * @return byte count on success, negative value on error.
 */
int sdio_read_avail(struct sdio_channel *ch);

#else

static int __maybe_unused sdio_open(const char *name, struct sdio_channel **ch,
		void *priv, void (*notify)(void *priv, unsigned channel_event))
{
	return -ENODEV;
}

static int __maybe_unused sdio_close(struct sdio_channel *ch)
{
	return -ENODEV;
}

static int __maybe_unused sdio_read(struct sdio_channel *ch, void *data,
						int len)
{
	return -ENODEV;
}

static int __maybe_unused sdio_write(struct sdio_channel *ch, const void *data,
						int len)
{
	return -ENODEV;
}

static int __maybe_unused sdio_write_avail(struct sdio_channel *ch)
{
	return -ENODEV;
}

static int __maybe_unused sdio_read_avail(struct sdio_channel *ch)
{
	return -ENODEV;
}
#endif

#endif /* __SDIO_AL__ */
