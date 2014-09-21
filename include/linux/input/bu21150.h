/*
 * Japan Display Inc. BU21150 touch screen driver.
 *
 * Copyright (C) 2013-2014 Japan Display Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 *
 */
#ifndef _BU21150_H_
#define _BU21150_H_

/* return value */
#define BU21150_UNBLOCK     (5)

/* ioctl(IOCTL_CMD_RESET) */
#define BU21150_RESET_LOW   (0)
#define BU21150_RESET_HIGH  (1)

/* struct */
struct bu21150_ioctl_get_frame_data {
	char __user *buf;
	unsigned int size;
	char __user *tv; /* struct timeval* */
};

struct bu21150_ioctl_spi_data {
	unsigned long addr;
	char __user *buf;
	unsigned int count;
};

/* commands */
#define BU21150_IO_TYPE  (0xB8)
#define BU21150_IOCTL_CMD_GET_FRAME       _IOWR(BU21150_IO_TYPE, 0x01, \
		struct bu21150_ioctl_get_frame_data)
#define BU21150_IOCTL_CMD_RESET           _IO(BU21150_IO_TYPE, 0x02)
#define BU21150_IOCTL_CMD_SPI_READ        _IOW(BU21150_IO_TYPE, 0x03, \
		struct bu21150_ioctl_spi_data)
#define BU21150_IOCTL_CMD_SPI_WRITE       _IOR(BU21150_IO_TYPE, 0x04, \
		struct bu21150_ioctl_spi_data)
#define BU21150_IOCTL_CMD_UNBLOCK         _IO(BU21150_IO_TYPE, 0x05)
#define BU21150_IOCTL_CMD_SUSPEND         _IO(BU21150_IO_TYPE, 0x06)
#define BU21150_IOCTL_CMD_RESUME          _IO(BU21150_IO_TYPE, 0x07)
#define BU21150_IOCTL_CMD_UNBLOCK_RELEASE _IO(BU21150_IO_TYPE, 0x08)

#endif /* _BU21150_H_ */

