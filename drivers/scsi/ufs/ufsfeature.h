// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017-2018 Samsung Electronics Co., Ltd.
 */

#ifndef _UFSFEATURE_H_
#define _UFSFEATURE_H_

#include "ufs.h"
#include <scsi/ufs/ufs-mtk-ioctl.h>

#if defined(CONFIG_SCSI_UFS_HPB)
#include "ufshpb.h"
#endif
#include <scsi/scsi_cmnd.h>

#if defined(CONFIG_SCSI_UFS_TW)
#include "ufstw.h"
#endif

/* Constant value*/
#define SECTOR					512
#define BLOCK					4096
#define SECTORS_PER_BLOCK			(BLOCK / SECTOR)
#define BITS_PER_DWORD				32

#define IOCTL_DEV_CTX_MAX_SIZE			OS_PAGE_SIZE
#define OS_PAGE_SIZE				4096
#define OS_PAGE_SHIFT				12

#define UFSF_QUERY_REQ_RETRIES			1

/* Description */
#define UFSF_QUERY_DESC_DEVICE_MAX_SIZE		0x57
#define UFSF_QUERY_DESC_CONFIGURAION_MAX_SIZE	0xE2
#define UFSF_QUERY_DESC_UNIT_MAX_SIZE		0x2D
#define UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE	0x58

#define UFSFEATURE_SELECTOR			0x01

/* Extended UFS Feature Support */
#define UFSF_EFS_TURBO_WRITE			0x100

/* query_flag  */
#define MASK_QUERY_UPIU_FLAG_LOC		0xFF

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

#define INFO_MSG(msg, args...)		printk(KERN_INFO "%s:%d " msg "\n", \
					       __func__, __LINE__, ##args)
#define INIT_INFO(msg, args...)		INFO_MSG(msg, ##args)
#define RELEASE_INFO(msg, args...)	INFO_MSG(msg, ##args)
#define SYSFS_INFO(msg, args...)	INFO_MSG(msg, ##args)
#define ERR_MSG(msg, args...)		printk(KERN_ERR "%s:%d " msg "\n", \
					       __func__, __LINE__, ##args)
#define WARNING_MSG(msg, args...)	printk(KERN_WARNING "%s:%d " msg "\n", \
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

#if defined(CONFIG_SCSI_UFS_HPB)
	u16 lu_max_active_hpb_rgns;	/* 23h:24h wLUMaxActiveHPBRegions */
	u16 lu_hpb_pinned_rgn_startidx;	/* 25h:26h wHPBPinnedRegionStartIdx */
	u16 lu_num_hpb_pinned_rgns;	/* 27h:28h wNumHPBPinnedRegions */
	int lu_hpb_pinned_end_offset;
#endif
#if defined(CONFIG_SCSI_UFS_TW)
	unsigned int tw_lu_buf_size;
#endif
};

struct ufsf_feature {
	struct ufs_hba *hba;
	int num_lu;
	int slave_conf_cnt;
	struct scsi_device *sdev_ufs_lu[UFS_UPIU_MAX_GENERAL_LUN];
#if defined(CONFIG_SCSI_UFS_HPB)
	struct ufshpb_dev_info hpb_dev_info;
	struct ufshpb_lu *ufshpb_lup[UFS_UPIU_MAX_GENERAL_LUN];
	struct work_struct ufshpb_init_work;
	struct work_struct ufshpb_reset_work;
	struct work_struct ufshpb_eh_work;
	wait_queue_head_t wait_hpb;
	int ufshpb_state;
	struct kref ufshpb_kref;
	bool issue_ioctl;
#endif
#if defined(CONFIG_SCSI_UFS_TW)
	struct ufstw_dev_info tw_dev_info;
	struct ufstw_lu *tw_lup[UFS_UPIU_MAX_GENERAL_LUN];
	struct work_struct tw_init_work;
	struct work_struct tw_reset_work;
	wait_queue_head_t tw_wait;
	atomic_t tw_state;
	struct kref tw_kref;

	/* turbo write exception event control */
	bool tw_ee_mode;

	/* for debug */
	bool tw_debug;
	int tw_debug_no;
	atomic64_t tw_debug_ee_count;
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
bool ufsf_is_valid_lun(int lun);
int ufsf_get_ee_status(struct ufs_hba *hba, u32 *status);

/* for hpb */
int ufsf_hpb_prepare_pre_req(struct ufsf_feature *ufsf, struct scsi_cmnd *cmd,
			     int lun);
int ufsf_hpb_prepare_add_lrbp(struct ufsf_feature *ufsf, int add_tag);
void ufsf_hpb_end_pre_req(struct ufsf_feature *ufsf, struct request *req);
void ufsf_hpb_change_lun(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufsf_hpb_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufsf_hpb_noti_rb(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufsf_hpb_reset_lu(struct ufsf_feature *ufsf);
void ufsf_hpb_reset_host(struct ufsf_feature *ufsf);
void ufsf_hpb_init(struct ufsf_feature *ufsf);
void ufsf_hpb_reset(struct ufsf_feature *ufsf);
void ufsf_hpb_suspend(struct ufsf_feature *ufsf);
void ufsf_hpb_resume(struct ufsf_feature *ufsf);
void ufsf_hpb_release(struct ufsf_feature *ufsf);
void ufsf_hpb_set_init_state(struct ufsf_feature *ufsf);

/* for tw*/
void ufsf_tw_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufsf_tw_init(struct ufsf_feature *ufsf);
void ufsf_tw_reset(struct ufsf_feature *ufsf);
int ufsf_tw_check_flush(struct ufsf_feature *ufsf);
void ufsf_tw_suspend(struct ufsf_feature *ufsf);
void ufsf_tw_resume(struct ufsf_feature *ufsf);
void ufsf_tw_release(struct ufsf_feature *ufsf);
void ufsf_tw_set_init_state(struct ufsf_feature *ufsf);
void ufsf_tw_reset_lu(struct ufsf_feature *ufsf);
void ufsf_tw_reset_host(struct ufsf_feature *ufsf);
void ufsf_tw_ee_handler(struct ufsf_feature *ufsf);
#endif /* End of Header */
