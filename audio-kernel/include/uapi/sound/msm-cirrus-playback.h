/* Copyright (c) 2016 Cirrus Logic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _UAPI_MSM_CIRRUS_SPK_PR_H
#define _UAPI_MSM_CIRRUS_SPK_PR_H

#include <linux/types.h>
#include <linux/ioctl.h>


#define CIRRUS_SP			0x10027053
#define CIRRUS_SP_ENABLE		0x10002001

#define CRUS_MODULE_ID_TX		0x00000002
#define CRUS_MODULE_ID_RX		0x00000001

/*
 * CRUS_PARAM_RX/TX_SET_USECASE
 * 0 = Music Playback in firmware
 * 1 = VOICE Playback in firmware
 * 2 = Tuning file loaded using external
 * config load command
 *
 * uses crus_rx_run_case_ctrl for RX apr pckt
 * uses crus_single_data_ctrl for TX apr pckt
 *
 */
#define CRUS_PARAM_RX_SET_USECASE	0x00A1AF02
#define CRUS_PARAM_TX_SET_USECASE	0x00A1BF0A
/*
 * CRUS_PARAM_RX/TX_SET_CALIB
 * Non-zero value to run speaker
 * calibration sequence
 *
 * uses crus_single_data_t apr pckt
 */
#define CRUS_PARAM_RX_SET_CALIB		0x00A1AF03
#define CRUS_PARAM_TX_SET_CALIB		0x00A1BF03
/*
 * CRUS_PARAM_RX/TX_SET_EXT_CONFIG
 * config string loaded from libfirmware
 * max of 7K paramters
 *
 * uses crus_external_config_t apr pckt
 */
#define CRUS_PARAM_RX_SET_EXT_CONFIG	0x00A1AF05
#define CRUS_PARAM_TX_SET_EXT_CONFIG	0x00A1BF08
/*
 * CRUS_PARAM_RX_GET_TEMP
 * get current Temp and calibration data
 *
 * CRUS_PARAM_TX_GET_TEMP_CAL
 * get results of calibration sequence
 *
 * uses cirrus_cal_result_t apr pckt
 */
#define CRUS_PARAM_RX_GET_TEMP		0x00A1AF07
#define CRUS_PARAM_TX_GET_TEMP_CAL	0x00A1BF06
/*
 * CRUS_PARAM_RX_SET_DELTA_CONFIG
 * load seamless transition config string
 *
 * CRUS_PARAM_RX_RUN_DELTA_CONFIG
 * execute the loaded seamless transition
 */
#define CRUS_PARAM_RX_SET_DELTA_CONFIG	0x00A1AF0D
#define CRUS_PARAM_RX_RUN_DELTA_CONFIG	0x00A1AF0E
/*
 * CRUS_PARAM_RX_CHANNEL_SWAP
 * initiate l/r channel swap transition
 */
#define CRUS_PARAM_RX_CHANNEL_SWAP	0x00A1AF12
#define CRUS_PARAM_RX_GET_CHANNEL_SWAP	0x00A1AF13
/*
 * CRUS_PARAM_RX_SET_ATTENUATION
 * set volume attenuation in volume control blocks 1 & 2
 */
#define CRUS_PARAM_RX_SET_ATTENUATION	0x00A1AF0A
#define CRUS_AFE_PARAM_ID_ENABLE	0x00010203

#define SPK_PROT_IOCTL_MAGIC		'a'

#define CRUS_SP_IOCTL_GET	_IOWR(SPK_PROT_IOCTL_MAGIC, 219, void *)
#define CRUS_SP_IOCTL_SET	_IOWR(SPK_PROT_IOCTL_MAGIC, 220, void *)
#define CRUS_SP_IOCTL_GET_CALIB	_IOWR(SPK_PROT_IOCTL_MAGIC, 221, void *)
#define CRUS_SP_IOCTL_SET_CALIB	_IOWR(SPK_PROT_IOCTL_MAGIC, 222, void *)
#define CRUS_SP_IOCTL_READ_CALIB_FROM_SLOT	_IOWR(SPK_PROT_IOCTL_MAGIC, 223, void *)
#define CRUS_SP_IOCTL_WRITE_CALIB_TO_SLOT	_IOWR(SPK_PROT_IOCTL_MAGIC, 224, void *)

#define CRUS_SP_IOCTL_GET32		_IOWR(SPK_PROT_IOCTL_MAGIC, 219, \
	compat_uptr_t)
#define CRUS_SP_IOCTL_SET32		_IOWR(SPK_PROT_IOCTL_MAGIC, 220, \
	compat_uptr_t)
#define CRUS_SP_IOCTL_GET_CALIB32	_IOWR(SPK_PROT_IOCTL_MAGIC, 221, \
	compat_uptr_t)
#define CRUS_SP_IOCTL_SET_CALIB32	_IOWR(SPK_PROT_IOCTL_MAGIC, 222, \
	compat_uptr_t)
#define CRUS_SP_IOCTL_READ_CALIB_FROM_SLOT32 _IOWR(SPK_PROT_IOCTL_MAGIC, 223, \
	compat_uptr_t)
#define CRUS_SP_IOCTL_WRITE_CALIB_TO_SLOT32 _IOWR(SPK_PROT_IOCTL_MAGIC, 224, \
	compat_uptr_t)

struct crus_sp_ioctl_header {
	uint32_t size;
	uint32_t module_id;
	uint32_t param_id;
	uint32_t data_length;
	void *data;
};

#endif /* _UAPI_MSM_CIRRUS_SPK_PR_H */
