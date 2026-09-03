// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022 Unisoc, Inc. All rights reserved.
 */

#ifndef __UFS_PROC_BOOTDEVICE_H__
#define __UFS_PROC_BOOTDEVICE_H__

#define UFS_TYPE 1
#define UFS_MAX_SN_LEN 12
#define UFS_CID_LEN 4
#define UFS_PROC_NAME_LEN 32
#define UFS_REV_LEN 32

struct ufs_bootdevice {
	u8 device_desc[QUERY_DESC_MAX_SIZE];
	int cid[UFS_CID_LEN];
	u8 life_time_est_typ_a;
	u8 life_time_est_typ_b;
	u8 manid;
	const char *name;
	u8 pre_eol_info;
	char product_name[UFS_PROC_NAME_LEN + 1];
	char rev[UFS_REV_LEN + 1];
	u64 size;
	u8 type;
	u16 specversion;
	u8 wb_enable;
};

struct ufs_device_identification {
	u8 serial_number[12];
	u16 manufacturer_date;
	u16 manufacturer_id;
};

int sprd_ufs_proc_init(struct ufs_hba *hba);
void sprd_ufs_proc_exit(void);
int ufshcd_decode_ufs_uid(struct ufs_hba *hba);

#endif
