/*
 * drivers/misc/tegra-baseband/bb-modem4.h
 *
 * Copyright (C) 2012 NVIDIA Corporation
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

/* Static data that doesn't change */
struct sdata {
	int gpio_awr;
	int gpio_cwr;
	int gpio_wdi;
};

/* Dynamic ops data */
struct opsdata {
	int powerstate;
	struct usb_device *usbdev;
};

/* Locks */
struct locks {
	struct wake_lock wlock;
	spinlock_t lock;
};
