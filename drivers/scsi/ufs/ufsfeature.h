/*
 * Universal Flash Storage Feature Support
 *
 * Copyright (C) 2017-2018 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Yongmyung Lee <ymhungry.lee@samsung.com>
 *	Jinyoung Choi <j-young.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * See the COPYING file in the top-level directory or visit
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program is provided "AS IS" and "WITH ALL FAULTS" and
 * without warranty of any kind. You are solely responsible for
 * determining the appropriateness of using and distributing
 * the program and assume all risks associated with your exercise
 * of rights with respect to the program, including but not limited
 * to infringement of third party rights, the risks and costs of
 * program errors, damage to or loss of data, programs or equipment,
 * and unavailability or interruption of operations. Under no
 * circumstances will the contributor of this Program be liable for
 * any damages of any kind arising from your use or distribution of
 * this program.
 *
 * The Linux Foundation chooses to take subject only to the GPLv2
 * license terms, and distributes only under these terms.
 */

#ifndef _UFSFEATURE_H_
#define _UFSFEATURE_H_

#include <scsi/scsi_cmnd.h>
#include <scsi/ufs/ufs-mtk-ioctl.h>

#include "ufs.h"

#include "ufshpb.h"
#include "ufstw.h"


/* Version info */
#define UFSFEATURE_DD_VER			0x010100
#define UFSFEATURE_DD_VER_POST			""

/* Constant value*/
#define SECTOR					512
#define BLOCK					4096
#define SECTORS_PER_BLOCK			(BLOCK / SECTOR)
#define BITS_PER_DWORD				32
#define sects_per_blk_shift			3
#define bits_per_dword_shift			5
#define bits_per_dword_mask			0x1F
#define bits_per_byte_shift			3

#define IOCTL_DEV_CTX_MAX_SIZE			OS_PAGE_SIZE
#define OS_PAGE_SIZE				4096
#define OS_PAGE_SHIFT				12

#define UFSF_QUERY_REQ_RETRIES			1

/* Description */
#define UFSF_QUERY_DESC_DEVICE_MAX_SIZE		0x5F
#define UFSF_QUERY_DESC_CONFIGURAION_MAX_SIZE	0xE6
#define UFSF_QUERY_DESC_UNIT_MAX_SIZE		0x2D
#define UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE	0x59

#define UFSFEATURE_SELECTOR			0x01

/* query_flag  */
#define MASK_QUERY_UPIU_FLAG_LOC		0xFF

/* For read10 debug */
#define READ10_DEBUG_LUN			0x7F
#define READ10_DEBUG_LBA			0x48504230

/* BIG -> LI */
#define LI_EN_16(x)				be16_to_cpu(*(__be16 *)(x))
#define LI_EN_32(x)				be32_to_cpu(*(__be32 *)(x))
#define LI_EN_64(x)				be64_to_cpu(*(__be64 *)(x))

/* LI -> BIG  */
#define GET_BYTE_0(num)			(((num) >> 0) & 0xff)
#define GET_BYTE_1(num)			(((num) >> 8) & 0xff)
#define GET_BYTE_2(num)			(((num) >> 16) & 0xff)
#define GET_BYTE_3(num)			(((num) >> 24) & 0xff)
#define GET_BYTE_4(num)			(((num) >> 32) & 0xff)
#define GET_BYTE_5(num)			(((num) >> 40) & 0xff)
#define GET_BYTE_6(num)			(((num) >> 48) & 0xff)
#define GET_BYTE_7(num)			(((num) >> 56) & 0xff)

#define INFO_MSG(msg, args...)		pr_info("%s:%d info: " msg "\n", \
						__func__, __LINE__, ##args)
#define ERR_MSG(msg, args...)		pr_err("%s:%d err: " msg "\n", \
					       __func__, __LINE__, ##args)
#define WARN_MSG(msg, args...)		pr_warn("%s:%d warn: " msg "\n", \
						__func__, __LINE__, ##args)

#define seq_scan_lu(lun) for (lun = 0; lun < UFS_UPIU_MAX_GENERAL_LUN; lun++)

#define TMSG(ufsf, lun, msg, args...)					\
	do { if (ufsf->sdev_ufs_lu[lun] &&				\
		 ufsf->sdev_ufs_lu[lun]->request_queue)			\
		blk_add_trace_msg(					\
			ufsf->sdev_ufs_lu[lun]->request_queue,		\
			msg, ##args);					\
	} while (0)							\

struct ufsf_lu_desc {
	/* Common info */
	int lu_enable;		/* 03h bLUEnable */
	int lu_queue_depth;	/* 06h lu queue depth info*/
	int lu_logblk_size;	/* 0Ah bLogicalBlockSize. default 0x0C = 4KB */
	u64 lu_logblk_cnt;	/* 0Bh qLogicalBlockCount. */

#if defined(CONFIG_UFSHPB)
	u16 lu_max_active_hpb_rgns;	/* 23h:24h wLUMaxActiveHPBRegions */
	u16 lu_hpb_pinned_rgn_startidx;	/* 25h:26h wHPBPinnedRegionStartIdx */
	u16 lu_num_hpb_pinned_rgns;	/* 27h:28h wNumHPBPinnedRegions */
	int lu_hpb_pinned_end_offset;
#endif
#if defined(CONFIG_UFSTW)
	unsigned int tw_lu_buf_size;
#endif
};

struct ufsf_feature {
	struct ufs_hba *hba;
	int num_lu;
	int slave_conf_cnt;
	struct scsi_device *sdev_ufs_lu[UFS_UPIU_MAX_GENERAL_LUN];
	bool issue_read10_debug;

#if defined(CONFIG_UFSHPB)
	struct ufshpb_dev_info hpb_dev_info;
	struct ufshpb_lu *hpb_lup[UFS_UPIU_MAX_GENERAL_LUN];
	struct work_struct hpb_init_work;
	struct work_struct hpb_eh_work;
	wait_queue_head_t hpb_wait;
	atomic_t hpb_state;
	struct kref hpb_kref;
#endif
#if defined(CONFIG_UFSTW)
	struct ufstw_dev_info tw_dev_info;
	struct ufstw_lu *tw_lup[UFS_UPIU_MAX_GENERAL_LUN];
	atomic_t tw_state;
#endif
};

struct ufs_hba;
struct ufshcd_lrb;

void ufsf_device_check(struct ufs_hba *hba);
int ufsf_check_query(__u32 opcode);
int ufsf_query_ioctl(struct ufsf_feature *ufsf, unsigned int lun,
		     void __user *buffer,
		     struct ufs_ioctl_query_data_hpb *ioctl_data,
		     u8 selector);
int ufsf_query_flag_retry(struct ufs_hba *hba, enum query_opcode opcode,
		    enum flag_idn idn, u8 idx, bool *flag_res);
int ufsf_query_attr_retry(struct ufs_hba *hba, enum query_opcode opcode,
			  enum attr_idn idn, u8 idx, u32 *attr_val);
int ufsf_get_scsi_device(struct ufs_hba *hba, struct scsi_device *sdev);
bool ufsf_is_valid_lun(int lun);
void ufsf_slave_configure(struct ufsf_feature *ufsf, struct scsi_device *sdev);

void ufsf_change_read10_debug_lun(struct ufsf_feature *ufsf,
				  struct ufshcd_lrb *lrbp);
void ufsf_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufsf_reset_lu(struct ufsf_feature *ufsf);
void ufsf_reset_host(struct ufsf_feature *ufsf);
void ufsf_init(struct ufsf_feature *ufsf);
void ufsf_reset(struct ufsf_feature *ufsf);
void ufsf_remove(struct ufsf_feature *ufsf);
void ufsf_set_init_state(struct ufsf_feature *ufsf);
void ufsf_suspend(struct ufsf_feature *ufsf);
void ufsf_resume(struct ufsf_feature *ufsf);
void ufsf_on_idle(struct ufsf_feature *ufsf, bool scsi_req);

/* for hpb */
int ufsf_hpb_prepare_pre_req(struct ufsf_feature *ufsf, struct scsi_cmnd *cmd,
			     int lun);
int ufsf_hpb_prepare_add_lrbp(struct ufsf_feature *ufsf, int add_tag);
void ufsf_hpb_end_pre_req(struct ufsf_feature *ufsf, struct request *req);
void ufsf_hpb_noti_rb(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufsf_hpb_release(struct ufsf_feature *ufsf);
#endif /* End of Header */
