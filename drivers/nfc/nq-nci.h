/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef __NFC_NCI_H
#define __NFC_NCI_H

#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/version.h>

#include <linux/semaphore.h>
#include <linux/completion.h>

#include <linux/ioctl.h>
#include <linux/miscdevice.h>

#define NFC_SET_PWR	_IOW(0xE9, 0x01, unsigned int)
#define SET_RX_BLOCK	_IOW(0xE9, 0x04, unsigned int)
#define SET_EMULATOR_TEST_POINT		_IOW(0xE9, 0x05, unsigned int)
#define NFCC_INITIAL_CORE_RESET_NTF	_IOW(0xE9, 0x10, unsigned int)

#define NFC_RX_BUFFER_CNT_START		(0x0)

#define PAYLOAD_HEADER_LENGTH		(0x3)
#define PAYLOAD_LENGTH_MAX		(256)
#define BYTE				(0x8)
#define NCI_IDENTIFIER			(0x10)

enum nfcc_initial_core_reset_ntf {
	TIMEDOUT_INITIAL_CORE_RESET_NTF = 0, /* 0*/
	ARRIVED_INITIAL_CORE_RESET_NTF, /* 1 */
	DEFAULT_INITIAL_CORE_RESET_NTF, /*2*/
};

#endif
