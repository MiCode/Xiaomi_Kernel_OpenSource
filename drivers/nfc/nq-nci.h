/* Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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

#ifndef __NQ_NCI_H
#define __NQ_NCI_H

#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/version.h>

#include <linux/semaphore.h>
#include <linux/completion.h>

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/nfcinfo.h>

#define NFC_SET_PWR			_IOW(0xE9, 0x01, unsigned int)
#define ESE_SET_PWR			_IOW(0xE9, 0x02, unsigned int)
#define ESE_GET_PWR			_IOR(0xE9, 0x03, unsigned int)
#define SET_RX_BLOCK			_IOW(0xE9, 0x04, unsigned int)
#define SET_EMULATOR_TEST_POINT		_IOW(0xE9, 0x05, unsigned int)
#define NFCC_INITIAL_CORE_RESET_NTF	_IOW(0xE9, 0x10, unsigned int)

#define NFC_RX_BUFFER_CNT_START		(0x0)
#define PAYLOAD_HEADER_LENGTH		(0x3)
#define PAYLOAD_LENGTH_MAX		(256)
#define BYTE				(0x8)
#define NCI_IDENTIFIER			(0x10)

#define DEV_COUNT	1
#define DEVICE_NAME	"nq-nci"
#define CLASS_NAME	"nqx"
/*
 * From MW 11.04 buffer size increased to support
 * frame size of 554 in FW download mode
 * Frame len(2) + Frame Header(6) + DATA(512) + HASH(32) + CRC(2) + RFU(4)
 */
#define MAX_BUFFER_SIZE			(558)
#define WAKEUP_SRC_TIMEOUT		(2000)
#define MAX_RETRY_COUNT			3
#define NCI_RESET_CMD_LEN		4
#define NCI_RESET_RSP_LEN		6
#define NCI_RESET_NTF_LEN		13
#define NCI_INIT_CMD_LEN		3
#define NCI_INIT_RSP_LEN		28
#define NCI_GET_VERSION_CMD_LEN		8
#define NCI_GET_VERSION_RSP_LEN		12
#define NCI_HEADER_LEN			3
#define NCI_1_0_RESET_RSP_PAYLOAD_LEN	3
#define NCI_PAYLOAD_START_INDEX		3
#define NCI_PAYLOAD_LENGTH_INDEX	(NCI_PAYLOAD_START_INDEX - 1)
#define MAX_IRQ_WAIT_TIME		(90) /* in ms */
#define NFCC_HW_CHIP_ID_OFFSET		4
#define NFCC_HW_ROM_VER_OFFSET		3
#define NFCC_HW_MAJOR_NO_OFFSET		2
#define NFCC_HW_MINOR_NO_OFFSET		1

enum nfcc_initial_core_reset_ntf {
	TIMEDOUT_INITIAL_CORE_RESET_NTF = 0, /* 0*/
	ARRIVED_INITIAL_CORE_RESET_NTF, /* 1 */
	DEFAULT_INITIAL_CORE_RESET_NTF, /*2*/
};

enum nfcc_chip_variant {
	NFCC_NQ_210			= 0x48,	/**< NFCC NQ210 */
	NFCC_NQ_220			= 0x58,	/**< NFCC NQ220 */
	NFCC_NQ_310			= 0x40,	/**< NFCC NQ310 */
	NFCC_NQ_330			= 0x51,	/**< NFCC NQ330 */
	NFCC_SN100_A			= 0xa3,	/**< NFCC SN100_A */
	NFCC_SN100_B			= 0xa4,	/**< NFCC SN100_B */
	NFCC_PN66T			= 0x18,	/**< NFCC PN66T */
	NFCC_NOT_SUPPORTED	        = 0xFF	/**< NFCC is not supported */
};
#endif
