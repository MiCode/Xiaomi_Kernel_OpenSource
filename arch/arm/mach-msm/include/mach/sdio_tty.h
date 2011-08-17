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

/*
 * SDIO TTY interface.
 */

#ifndef __SDIO_TTY__
#define __SDIO_TTY__

/**
 * sdio_tty_init_tty - Initialize the SDIO TTY driver.
 *
 * @tty_name: tty name - identify the tty device.
 * @sdio_ch_name: channel name - identify the channel.
 * @return sdio_tty handle on success, NULL on error.
 *
 */
void *sdio_tty_init_tty(char *tty_name, char* sdio_ch_name);

/**
 * sdio_tty_uninit_tty - Uninitialize the SDIO TTY driver.
 *
 * @sdio_tty_handle: sdio_tty handle.
 * @return 0 on success, negative value on error.
 */
int sdio_tty_uninit_tty(void *sdio_tty_handle);

/**
 * sdio_tty_enable_debug_msg - Enable/Disable sdio_tty debug
 * messages.
 *
 * @enable: A flag to indicate if to enable or disable the debug
 *        messages.
 * @return 0 on success, negative value on error.
 */
void sdio_tty_enable_debug_msg(void *sdio_tty_handle, int enable);

#endif /* __SDIO_TTY__ */
