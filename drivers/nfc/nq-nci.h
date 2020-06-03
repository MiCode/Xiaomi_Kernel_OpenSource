/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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

#define DEV_COUNT			1
#define DEVICE_NAME			"nq-nci"
#define CLASS_NAME			"nqx"
#define MAX_BUFFER_SIZE			(320)
#define WAKEUP_SRC_TIMEOUT		(2000)
#define NCI_HEADER_LEN			3
#define NCI_PAYLOAD_IDX			3
#define MAX_RETRY_COUNT			3
#define NCI_RESET_CMD_LEN		4
#define NCI_RESET_RSP_LEN		4
#define NCI_RESET_NTF_LEN		13
#define NCI_GET_VERSION_CMD_LEN		8
#define NCI_GET_VERSION_RSP_LEN		12
#define MAX_IRQ_WAIT_TIME		(90)	//in ms
#define COLD_RESET_CMD_LEN		3
#define COLD_RESET_RSP_LEN		4
#define COLD_RESET_CMD_GID		0x2F
#define COLD_RESET_CMD_PAYLOAD_LEN	0x00
#define COLD_RESET_RSP_GID		0x4F
#define COLD_RESET_OID			0x1E

#define NFC_RX_BUFFER_CNT_START		(0x0)
#define PAYLOAD_HEADER_LENGTH		(0x3)
#define PAYLOAD_LENGTH_MAX		(256)
#define BYTE				(0x8)
#define NCI_IDENTIFIER			(0x10)
#define NFC_LDO_SUPPLY_DT_NAME		"qcom,nq-vdd-1p8"
#define NFC_LDO_SUPPLY_NAME		"qcom,nq-vdd-1p8-supply"
#define NFC_LDO_VOL_DT_NAME		"qcom,nq-vdd-1p8-voltage"
#define NFC_LDO_CUR_DT_NAME		"qcom,nq-vdd-1p8-current"

//as per SN1x0 datasheet
#define NFC_VDDIO_MIN			1650000 //in uV
#define NFC_VDDIO_MAX			1950000 //in uV
#define NFC_CURRENT_MAX			157000 //in uA

enum ese_ioctl_request {
	/* eSE POWER ON */
	ESE_POWER_ON = 0,
	/* eSE POWER OFF */
	ESE_POWER_OFF,
	/* eSE COLD RESET */
	ESE_COLD_RESET,
	/* eSE POWER STATE */
	ESE_POWER_STATE
};

enum nfcc_ioctl_request {
	/* NFC disable request with VEN LOW */
	NFC_POWER_OFF = 0,
	/* NFC enable request with VEN Toggle */
	NFC_POWER_ON,
	/* firmware download request with VEN Toggle */
	NFC_FW_DWL_VEN_TOGGLE,
	/* ISO reset request */
	NFC_ISO_RESET,
	/* request for firmware download gpio HIGH */
	NFC_FW_DWL_HIGH,
	/* hard reset request */
	NFC_HARD_RESET,
	/* request for firmware download gpio LOW */
	NFC_FW_DWL_LOW,
	/* NFC enable without VEN gpio modification */
	NFC_ENABLE,
	/* NFC disable without VEN gpio modification */
	NFC_DISABLE
};

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
