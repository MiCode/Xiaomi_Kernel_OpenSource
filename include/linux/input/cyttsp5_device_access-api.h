/*
 * cyttsp5_device_access-api.h
 * Parade TrueTouch(TM) Standard Product V5 Device Access API module.
 * For use with Parade touchscreen controllers.
 * Supported parts include:
 * CYTMA5XX
 * CYTMA448
 * CYTMA445A
 * CYTT21XXX
 * CYTT31XXX
 *
 * Copyright (C) 2015 Parade Semiconductor
 * Copyright (C) 2012-2015 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Parade Semiconductor at www.parade.com <ttdrivers@paradetech.com>
 *
 */

#ifndef _LINUX_CYTTSP5_DEVICE_ACCESS_API_H
#define _LINUX_CYTTSP5_DEVICE_ACCESS_API_H

#include <linux/types.h>
#include <linux/device.h>

int cyttsp5_device_access_user_command(const char *core_name, u16 read_len,
		u8 *read_buf, u16 write_len, u8 *write_buf,
		u16 *actual_read_len);

int cyttsp5_device_access_user_command_async(const char *core_name,
		u16 read_len, u8 *read_buf, u16 write_len, u8 *write_buf,
		void (*cont)(const char *core_name, u16 read_len, u8 *read_buf,
			u16 write_len, u8 *write_buf, u16 actual_read_length,
			int rc));
#endif /* _LINUX_CYTTSP5_DEVICE_ACCESS_API_H */
