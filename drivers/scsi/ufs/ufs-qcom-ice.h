/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _UFS_QCOM_ICE_H_
#define _UFS_QCOM_ICE_H_

#include <scsi/scsi_cmnd.h>

#include "ufs-qcom.h"

/*
 * UFS host controller ICE registers. There are n [0..31]
 * of each of these registers
 */
enum {
	REG_UFS_QCOM_ICE_CFG		         = 0x2200,
	REG_UFS_QCOM_ICE_CTRL_INFO_1_n           = 0x2204,
	REG_UFS_QCOM_ICE_CTRL_INFO_2_n           = 0x2208,
	REG_UFS_QCOM_ICE_CTRL_INFO_3_n           = 0x220C,
};
#define NUM_QCOM_ICE_CTRL_INFO_n_REGS		32

/* UFS QCOM ICE CTRL Info register offset */
enum {
	OFFSET_UFS_QCOM_ICE_CTRL_INFO_BYPASS     = 0,
	OFFSET_UFS_QCOM_ICE_CTRL_INFO_KEY_INDEX  = 0x1,
	OFFSET_UFS_QCOM_ICE_CTRL_INFO_CDU        = 0x6,
};

/* UFS QCOM ICE CTRL Info register masks */
enum {
	MASK_UFS_QCOM_ICE_CTRL_INFO_BYPASS     = 0x1,
	MASK_UFS_QCOM_ICE_CTRL_INFO_KEY_INDEX  = 0x1F,
	MASK_UFS_QCOM_ICE_CTRL_INFO_CDU        = 0x8,
};

/* UFS QCOM ICE encryption/decryption bypass state */
enum {
	UFS_QCOM_ICE_DISABLE_BYPASS  = 0,
	UFS_QCOM_ICE_ENABLE_BYPASS = 1,
};

/* UFS QCOM ICE Crypto Data Unit of target DUN of Transfer Request */
enum {
	UFS_QCOM_ICE_TR_DATA_UNIT_512_B          = 0,
	UFS_QCOM_ICE_TR_DATA_UNIT_1_KB           = 1,
	UFS_QCOM_ICE_TR_DATA_UNIT_2_KB           = 2,
	UFS_QCOM_ICE_TR_DATA_UNIT_4_KB           = 3,
	UFS_QCOM_ICE_TR_DATA_UNIT_8_KB           = 4,
	UFS_QCOM_ICE_TR_DATA_UNIT_16_KB          = 5,
	UFS_QCOM_ICE_TR_DATA_UNIT_32_KB          = 6,
};

/* UFS QCOM ICE internal state */
enum {
	UFS_QCOM_ICE_STATE_DISABLED   = 0,
	UFS_QCOM_ICE_STATE_ACTIVE     = 1,
	UFS_QCOM_ICE_STATE_SUSPENDED  = 2,
};

#ifdef CONFIG_SCSI_UFS_QCOM_ICE
int ufs_qcom_ice_get_dev(struct ufs_qcom_host *qcom_host);
int ufs_qcom_ice_init(struct ufs_qcom_host *qcom_host);
int ufs_qcom_ice_req_setup(struct ufs_qcom_host *qcom_host,
			   struct scsi_cmnd *cmd, u8 *cc_index, bool *enable);
int ufs_qcom_ice_cfg(struct ufs_qcom_host *qcom_host, struct scsi_cmnd *cmd);
int ufs_qcom_ice_reset(struct ufs_qcom_host *qcom_host);
int ufs_qcom_ice_resume(struct ufs_qcom_host *qcom_host);
int ufs_qcom_ice_suspend(struct ufs_qcom_host *qcom_host);
int ufs_qcom_ice_get_status(struct ufs_qcom_host *qcom_host, int *ice_status);
void ufs_qcom_ice_print_regs(struct ufs_qcom_host *qcom_host);
#else
inline int ufs_qcom_ice_get_dev(struct ufs_qcom_host *qcom_host)
{
	if (qcom_host) {
		qcom_host->ice.pdev = NULL;
		qcom_host->ice.vops = NULL;
	}
	return -ENODEV;
}
inline int ufs_qcom_ice_init(struct ufs_qcom_host *qcom_host)
{
	return 0;
}
inline int ufs_qcom_ice_cfg(struct ufs_qcom_host *qcom_host,
			    struct scsi_cmnd *cmd)
{
	return 0;
}
inline int ufs_qcom_ice_reset(struct ufs_qcom_host *qcom_host)
{
	return 0;
}
inline int ufs_qcom_ice_resume(struct ufs_qcom_host *qcom_host)
{
	return 0;
}
inline int ufs_qcom_ice_suspend(struct ufs_qcom_host *qcom_host)
{
	return 0;
}
inline int ufs_qcom_ice_get_status(struct ufs_qcom_host *qcom_host,
				   int *ice_status)
{
	return 0;
}
inline void ufs_qcom_ice_print_regs(struct ufs_qcom_host *qcom_host)
{
	return;
}
#endif /* CONFIG_SCSI_UFS_QCOM_ICE */

#endif /* UFS_QCOM_ICE_H_ */
