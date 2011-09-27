/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
 * SDIO CMUX API
 */

#ifndef __SDIO_CMUX__
#define __SDIO_CMUX__

#ifdef CONFIG_MSM_SDIO_CMUX

enum {
	SDIO_CMUX_DATA_CTL_0,
	SDIO_CMUX_DATA_CTL_1,
	SDIO_CMUX_DATA_CTL_2,
	SDIO_CMUX_DATA_CTL_3,
	SDIO_CMUX_DATA_CTL_4,
	SDIO_CMUX_DATA_CTL_5,
	SDIO_CMUX_DATA_CTL_6,
	SDIO_CMUX_DATA_CTL_7,
	SDIO_CMUX_USB_CTL_0,
	SDIO_CMUX_USB_DUN_CTL_0,
	SDIO_CMUX_CSVT_CTL_0,
	SDIO_CMUX_NUM_CHANNELS
};


/*
 * sdio_cmux_open - Open the mux channel
 *
 * @id: Mux Channel id to be opened
 * @receive_cb: Notification when data arrives.  Parameters are data received,
 *	size of data, private context pointer.
 * @write_done: Notification when data is written.  Parameters are data written,
 *	size of data, private context pointer.  Please note that the data
 *	written pointer will always be NULL as the cmux makes an internal copy
 *	of the data.
 * @priv: caller's private context pointer
 */
int sdio_cmux_open(const int id,
		   void (*receive_cb)(void *, int, void *),
		   void (*write_done)(void *, int, void *),
		   void (*status_callback)(int, void *),
		   void *priv);

/*
 * sdio_cmux_close - Close the mux channel
 *
 * @id: Channel id to be closed
 */
int sdio_cmux_close(int id);

/*
 * sdio_cmux_write_avail - Write space avaialable for this channel
 *
 * @id: Channel id to look for the available write space
 */
int sdio_cmux_write_avail(int id);

/*
 * sdio_cmux_write - Write the data onto the CMUX channel
 *
 * @id: Channel id onto which the data has to be written
 * @data: Starting address of the data buffer to be written
 * @len: Length of the data to be written
 */
int sdio_cmux_write(int id, void *data, int len);

/* these are used to get and set the IF sigs of a channel.
 * DTR and RTS can be set; DSR, CTS, CD and RI can be read.
 */
int sdio_cmux_tiocmget(int id);
int sdio_cmux_tiocmset(int id, unsigned int set, unsigned int clear);

/*
 * is_remote_open - Check whether the remote channel is open
 *
 * @id: Channel id to be checked
 */
int is_remote_open(int id);

/*
 * sdio_cmux_is_channel_reset - Check whether the channel is in reset state
 *
 * @id: Channel id to be checked
 */
int sdio_cmux_is_channel_reset(int id);

#else

static int __maybe_unused sdio_cmux_open(const int id,
		   void (*receive_cb)(void *, int, void *),
		   void (*write_done)(void *, int, void *),
		   void (*status_callback)(int, void *),
		   void *priv)
{
	return -ENODEV;
}
static int __maybe_unused sdio_cmux_close(int id)
{
	return -ENODEV;
}

static int __maybe_unused sdio_cmux_write_avail(int id)
{
	return -ENODEV;
}

static int __maybe_unused sdio_cmux_write(int id, void *data, int len)
{
	return -ENODEV;
}

static int __maybe_unused sdio_cmux_tiocmget(int id)
{
	return -ENODEV;
}

static int __maybe_unused sdio_cmux_tiocmset(int id, unsigned int set,
							unsigned int clear)
{
	return -ENODEV;
}

static int __maybe_unused is_remote_open(int id)
{
	return -ENODEV;
}

static int __maybe_unused sdio_cmux_is_channel_reset(int id)
{
	return -ENODEV;
}
#endif
#endif /* __SDIO_CMUX__ */
