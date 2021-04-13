/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */
#ifndef LINUX_SLATECOM_INTERFACE_H
#define LINUX_SLATECOM_INTERFACE_H

#include <linux/types.h>

#define SLATECOM_REG_READ  0
#define SLATECOM_AHB_READ  1
#define SLATECOM_AHB_WRITE 2
#define SLATECOM_SET_SPI_FREE  3
#define SLATECOM_SET_SPI_BUSY  4
#define SLATECOM_REG_WRITE  5
#define SLATECOM_SOFT_RESET  6
#define SLATECOM_MODEM_DOWN2_SLATE  7
#define SLATECOM_TWM_EXIT  8
#define SLATECOM_SLATE_APP_RUNNING 9
#define SLATECOM_ADSP_DOWN2_SLATE  10
#define SLATECOM_SLATE_WEAR_LOAD 11
#define SLATECOM_SLATE_WEAR_UNLOAD 12
#define EXCHANGE_CODE  'V'

struct slate_ui_data {
	__u64  __user write;
	__u64  __user result;
	__u32  slate_address;
	__u32  cmd;
	__u32  num_of_words;
	__u8 __user *buffer;
};

enum slate_event_type {
	SLATE_BEFORE_POWER_DOWN = 1,
	SLATE_AFTER_POWER_DOWN,
	SLATE_BEFORE_POWER_UP,
	SLATE_AFTER_POWER_UP,
	MODEM_BEFORE_POWER_DOWN,
	MODEM_AFTER_POWER_UP,
	ADSP_BEFORE_POWER_DOWN,
	ADSP_AFTER_POWER_UP,
	TWM_SLATE_AFTER_POWER_UP,
	SLATE_DSP_ERROR,
	SLATE_DSP_READY,
	SLATE_BT_ERROR,
	SLATE_BT_READY,
};

#define REG_READ \
	_IOWR(EXCHANGE_CODE, SLATECOM_REG_READ, \
	struct slate_ui_data)
#define AHB_READ \
	_IOWR(EXCHANGE_CODE, SLATECOM_AHB_READ, \
	struct slate_ui_data)
#define AHB_WRITE \
	_IOW(EXCHANGE_CODE, SLATECOM_AHB_WRITE, \
	struct slate_ui_data)
#define SET_SPI_FREE \
	_IOR(EXCHANGE_CODE, SLATECOM_SET_SPI_FREE, \
	struct slate_ui_data)
#define SET_SPI_BUSY \
	_IOR(EXCHANGE_CODE, SLATECOM_SET_SPI_BUSY, \
	struct slate_ui_data)
#define REG_WRITE \
	_IOWR(EXCHANGE_CODE, SLATECOM_REG_WRITE, \
	struct slate_ui_data)
#define SLATE_SOFT_RESET \
	_IOWR(EXCHANGE_CODE, SLATECOM_SOFT_RESET, \
	struct slate_ui_data)
#define SLATE_TWM_EXIT \
	_IOWR(EXCHANGE_CODE, SLATECOM_TWM_EXIT, \
	struct slate_ui_data)
#define SLATE_APP_RUNNING \
	_IOWR(EXCHANGE_CODE, SLATECOM_SLATE_APP_RUNNING, \
	struct slate_ui_data)
#define SLATE_MODEM_DOWN2_SLATE_DONE \
	_IOWR(EXCHANGE_CODE, SLATECOM_MODEM_DOWN2_SLATE, \
	struct slate_ui_data)
#define SLATE_WEAR_LOAD \
	_IOWR(EXCHANGE_CODE, SLATECOM_SLATE_WEAR_LOAD, \
	struct slate_ui_data)
#define SLATE_WEAR_UNLOAD \
	_IOWR(EXCHANGE_CODE, SLATECOM_SLATE_WEAR_UNLOAD, \
	struct slate_ui_data)
#define SLATE_ADSP_DOWN2_SLATE_DONE \
	_IOWR(EXCHANGE_CODE, SLATECOM_ADSP_DOWN2_SLATE, \
	struct slate_ui_data)

#endif /* LINUX_SLATECOM_INTERFACE_H */

