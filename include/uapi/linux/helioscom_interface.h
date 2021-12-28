/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef LINUX_HELIOSCOM_INTERFACE_H
#define LINUX_HELIOSCOM_INTERFACE_H

#include <linux/types.h>

#define HELIOSCOM_REG_READ  0
#define HELIOSCOM_AHB_READ  1
#define HELIOSCOM_AHB_WRITE 2
#define HELIOSCOM_SET_SPI_FREE  3
#define HELIOSCOM_SET_SPI_BUSY  4
#define HELIOSCOM_REG_WRITE  5
#define HELIOSCOM_SOFT_RESET  6
#define HELIOSCOM_MODEM_DOWN2_HELIOS  7
#define HELIOSCOM_TWM_EXIT  8
#define HELIOSCOM_HELIOS_APP_RUNNING 9
#define HELIOSCOM_ADSP_DOWN2_HELIOS  10
#define HELIOSCOM_HELIOS_LOAD 11
#define HELIOSCOM_HELIOS_UNLOAD 12
#define HELIOSCOM_DEVICE_STATE_TRANSITION 13
#define HELIOS_SEND_TIME_DATA 14
#define EXCHANGE_CODE  'V'

struct helios_ui_data {
	__u64  __user write;
	__u64  __user result;
	__u32  helios_address;
	__u32  cmd;
	__u32  num_of_words;
	__u8 __user *buffer;
};

enum helios_event_type {
	HELIOS_BEFORE_POWER_DOWN = 1,
	HELIOS_AFTER_POWER_DOWN,
	HELIOS_BEFORE_POWER_UP,
	HELIOS_AFTER_POWER_UP,
	MODEM_BEFORE_POWER_DOWN,
	MODEM_AFTER_POWER_UP,
	ADSP_BEFORE_POWER_DOWN,
	ADSP_AFTER_POWER_UP,
	TWM_HELIOS_AFTER_POWER_UP,
	HELIOS_DSP_ERROR,
	HELIOS_DSP_READY,
	HELIOS_BT_ERROR,
	HELIOS_BT_READY,
};

enum device_state_transition {
	STATE_TWM_ENTER = 1,
	STATE_TWM_EXIT,
	STATE_DS_ENTER,
	STATE_DS_EXIT,
	STATE_S2D_ENTER,
	STATE_S2D_EXIT,
};

#define REG_READ \
	_IOWR(EXCHANGE_CODE, HELIOSCOM_REG_READ, \
	struct helios_ui_data)
#define AHB_READ \
	_IOWR(EXCHANGE_CODE, HELIOSCOM_AHB_READ, \
	struct helios_ui_data)
#define AHB_WRITE \
	_IOW(EXCHANGE_CODE, HELIOSCOM_AHB_WRITE, \
	struct helios_ui_data)
#define SET_SPI_FREE \
	_IOR(EXCHANGE_CODE, HELIOSCOM_SET_SPI_FREE, \
	struct helios_ui_data)
#define SET_SPI_BUSY \
	_IOR(EXCHANGE_CODE, HELIOSCOM_SET_SPI_BUSY, \
	struct helios_ui_data)
#define REG_WRITE \
	_IOWR(EXCHANGE_CODE, HELIOSCOM_REG_WRITE, \
	struct helios_ui_data)
#define HELIOS_SOFT_RESET \
	_IOWR(EXCHANGE_CODE, HELIOSCOM_SOFT_RESET, \
	struct helios_ui_data)
#define HELIOS_TWM_EXIT \
	_IOWR(EXCHANGE_CODE, HELIOSCOM_TWM_EXIT, \
	struct helios_ui_data)
#define HELIOS_APP_RUNNING \
	_IOWR(EXCHANGE_CODE, HELIOSCOM_HELIOS_APP_RUNNING, \
	struct helios_ui_data)
#define HELIOS_MODEM_DOWN2_HELIOS_DONE \
	_IOWR(EXCHANGE_CODE, HELIOSCOM_MODEM_DOWN2_HELIOS, \
	struct helios_ui_data)
#define HELIOS_LOAD \
	_IOWR(EXCHANGE_CODE, HELIOSCOM_HELIOS_LOAD, \
	struct helios_ui_data)
#define HELIOS_UNLOAD \
	_IOWR(EXCHANGE_CODE, HELIOSCOM_HELIOS_UNLOAD, \
	struct helios_ui_data)
#define HELIOS_ADSP_DOWN2_HELIOS_DONE \
	_IOWR(EXCHANGE_CODE, HELIOSCOM_ADSP_DOWN2_HELIOS, \
	struct helios_ui_data)
#define DEVICE_STATE_TRANSITION \
	_IOWR(EXCHANGE_CODE, HELIOSCOM_DEVICE_STATE_TRANSITION, \
	struct helios_ui_data)
#define SEND_TIME_DATA \
	_IOWR(EXCHANGE_CODE, HELIOS_SEND_TIME_DATA, \
	struct helios_ui_data)
#endif /* LINUX_HELIOSCOM_INTERFACE_H */

