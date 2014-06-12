/*
 * Gadget Function Driver for MTP
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_USB_F_MTP_H
#define __LINUX_USB_F_MTP_H

#include <uapi/linux/usb/f_mtp.h>
#include <linux/ioctl.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#ifdef __KERNEL__

#ifdef CONFIG_COMPAT
struct __compat_mtp_file_range {
	compat_int_t	fd;
	compat_loff_t	offset;
	int64_t		length;
	uint16_t	command;
	uint32_t	transaction_id;
};

struct __compat_mtp_event {
	compat_size_t	length;
	compat_caddr_t	data;
};

#define COMPAT_MTP_SEND_FILE              _IOW('M', 0, \
						struct __compat_mtp_file_range)
#define COMPAT_MTP_RECEIVE_FILE           _IOW('M', 1, \
						struct __compat_mtp_file_range)
#define COMPAT_MTP_SEND_EVENT             _IOW('M', 3, \
						struct __compat_mtp_event)
#define COMPAT_MTP_SEND_FILE_WITH_HEADER  _IOW('M', 4, \
						struct __compat_mtp_file_range)
#endif
#endif
#endif /* __LINUX_USB_F_MTP_H */
