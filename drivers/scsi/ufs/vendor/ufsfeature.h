/* SPDX-License-Identifier: GPL-2.0 */
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

#include "ufs.h"

#include "ufsshpb.h"
#include "ufstw.h"
#include "ufshid.h"
#include "ufsringbuf.h"

#define UFS_UPIU_MAX_GENERAL_LUN		8
#define UFSHCD_STATE_OPERATIONAL		2	/* ufshcd.c */

/* UFSHCD error handling flags */
enum {
	UFSHCD_EH_IN_PROGRESS = (1 << 0),		/* ufshcd.c */
};
#define ufshcd_eh_in_progress(h) \
	((h)->eh_flags & UFSHCD_EH_IN_PROGRESS)		/* ufshcd.c */


#define UFSFEATURE_QUERY_OPCODE			0x5500

/* Version info */
#define UFSFEATURE_DD_VER			0x020001
#define UFSFEATURE_DD_VER_POST			"PoC"

/* For read10 debug */
#define READ10_DEBUG_LUN			0x7F
#define READ10_DEBUG_LBA			0x48504230

/* For Chip Crack Detection */
#define VENDOR_OP                               0xC0
#define VENDOR_CCD                              0x51
#define CCD_DATA_SEG_LEN                        0x08
#define CCD_SENSE_DATA_LEN                      0x06
#define CCD_DESC_TYPE                           0x81
#define GOOD_CCD                                0x01

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

#if defined(CONFIG_UFSSHPB)
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
	bool issue_ioctl;
	bool check_init;
	struct work_struct device_check_work;
	struct mutex device_check_lock;

	struct work_struct reset_wait_work;
	struct work_struct reset_lu_work;
	int reset_lu_pos;

#if defined(CONFIG_UFSSHPB)
	struct ufsshpb_dev_info hpb_dev_info;
	struct ufsshpb_lu *hpb_lup[UFS_UPIU_MAX_GENERAL_LUN];
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
#if defined(CONFIG_UFSHID)
	struct work_struct on_idle_work;
	atomic_t hid_state;
	struct ufshid_dev *hid_dev;
#endif
#if defined(CONFIG_UFSRINGBUF)
	atomic_t ringbuf_state;
	struct ufsringbuf_dev *ringbuf_dev;
#endif
};

struct ufs_hba;
struct ufshcd_lrb;
struct ufs_ioctl_query_data;

void ufsf_device_check(struct ufs_hba *hba);
int ufsf_check_query(__u32 opcode);
int ufsf_query_ioctl(struct ufsf_feature *ufsf, int lun, void __user *buffer,
		     struct ufs_ioctl_query_data *ioctl_data,
		     u8 selector);
bool ufsf_upiu_check_for_ccd(struct ufshcd_lrb *lrbp);
int ufsf_get_scsi_device(struct ufs_hba *hba, struct scsi_device *sdev);
bool ufsf_is_valid_lun(int lun);
void ufsf_slave_configure(struct ufsf_feature *ufsf, struct scsi_device *sdev);
void ufsf_change_lun(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);

int ufsf_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufsf_prepare_reset_lu(struct ufsf_feature *ufsf);
void ufsf_reset_lu(struct ufsf_feature *ufsf);
void ufsf_reset_host(struct ufsf_feature *ufsf);
void ufsf_init(struct ufsf_feature *ufsf);
void ufsf_reset(struct ufsf_feature *ufsf);
void ufsf_remove(struct ufsf_feature *ufsf);
void ufsf_set_init_state(struct ufsf_feature *ufsf);
void ufsf_suspend(struct ufsf_feature *ufsf);
void ufsf_resume(struct ufsf_feature *ufsf, bool is_link_off);

/* mimic */
bool ufsf_blk_mq_get_driver_tag(struct request *rq);
int ufsf_query_flag(struct ufs_hba *hba, enum query_opcode opcode,
		    enum flag_idn idn, u8 index, u8 selector, bool *flag_res);
int ufsf_query_flag_retry(struct ufs_hba *hba, enum query_opcode opcode,
			  enum flag_idn idn, u8 index, u8 selector,
			  bool *flag_res);
int ufsf_issue_tm_cmd(struct ufs_hba *hba, int lun_id, int task_id,
		      u8 tm_function, u8 *tm_response);
void ufsf_scsi_unblock_requests(struct ufs_hba *hba);
void ufsf_scsi_block_requests(struct ufs_hba *hba);
int ufsf_wait_for_doorbell_clr(struct ufs_hba *hba, u64 wait_timeout_us);
void ufsf_copy_sense_data(struct ufshcd_lrb *lrbp);
int ufsf_blk_mq_run_hw_queue_mimic(struct blk_mq_hw_ctx *hctx, struct request *rq);
int ufsf_blk_execute_rq_nowait_mimic(struct request *rq,
				     struct blk_mq_hw_ctx *hctx,
				     rq_end_io_fn *done);

/* for hpb */
void ufsf_hpb_noti_rb(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);

/* for hid */
void ufsf_hid_acc_io_stat(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);

/* Flag idn for Query Requests*/
#if defined(CONFIG_UFSSHPB)
#define QUERY_FLAG_IDN_HPB_RESET			0x11
#endif
#if defined(CONFIG_UFSTW)
#define QUERY_FLAG_IDN_TW_EN				0x0E
#define QUERY_FLAG_IDN_TW_BUF_FLUSH_EN			0x0F
#define QUERY_FLAG_IDN_TW_FLUSH_DURING_HIBERN		0x10
#endif
#if defined(CONFIG_UFSRINGBUF)
#define QUERY_FLAG_IDN_CMD_HISTORY_RECORD_EN		0x12
#endif

/* Attribute idn for Query requests */
#if defined(CONFIG_UFSTW)
#define QUERY_ATTR_IDN_TW_FLUSH_STATUS			0x1C
#define QUERY_ATTR_IDN_TW_AVAIL_BUF_SIZE		0x1D
#define QUERY_ATTR_IDN_TW_BUF_LIFETIME_EST		0x1E
#define QUERY_ATTR_IDN_TW_CURR_BUF_SIZE			0x1F
#endif
#if defined(CONFIG_UFSHID)
#define QUERY_ATTR_IDN_HID_OPERATION			0x20
#define QUERY_ATTR_IDN_HID_FRAG_LEVEL			0x21
#endif
#if defined (CONFIG_UFSRINGBUF)
#define QUERY_ATTR_IDN_RINGBUF_RTCA			0x22
#define QUERY_ATTR_IDN_RINGBUF_RTCB			0x23
#endif
#define QUERY_ATTR_IDN_SUP_VENDOR_OPTIONS		0xFF

/* Unit descriptor parameters offsets in bytes*/
#if defined(CONFIG_UFSSHPB)
#define UNIT_DESC_HPB_LU_MAX_ACTIVE_REGIONS		0x23
#define UNIT_DESC_HPB_LU_PIN_REGION_START_OFFSET	0x25
#define UNIT_DESC_HPB_LU_NUM_PIN_REGIONS		0x27
#endif
#if defined(CONFIG_UFSTW)
#define UNIT_DESC_TW_LU_WRITE_BUFFER_ALLOC_UNIT		0x29
#endif

/* Device descriptor parameters offsets in bytes*/
#if defined(CONFIG_UFSSHPB)
#define DEVICE_DESC_PARAM_HPB_VER			0x40
#define DEVICE_DESC_PARAM_HPB_CONTROL			0x42
#endif
#define DEVICE_DESC_PARAM_EX_FEAT_SUP			0x4F
#if defined(CONFIG_UFSTW)
#define DEVICE_DESC_PARAM_TW_VER			0x4D
#define DEVICE_DESC_PARAM_TW_RETURN_TO_USER		0x53
#define DEVICE_DESC_PARAM_TW_BUF_TYPE			0x54
#define DEVICE_DESC_PARAM_TW_SHARED_BUF_ALLOC_UNITS	0x55
#endif
#if defined(CONFIG_UFSHID)
#define DEVICE_DESC_PARAM_HID_VER			0x59
#endif
#if defined(CONFIG_UFSRINGBUF)
#define DEVICE_DESC_PARAM_RING_BUF_VER			0x5B
#endif

/* Geometry descriptor parameters offsets in bytes*/
#if defined(CONFIG_UFSSHPB)
#define GEOMETRY_DESC_HPB_REGION_SIZE			0x48
#define GEOMETRY_DESC_HPB_NUMBER_LU			0x49
#define GEOMETRY_DESC_HPB_SUBREGION_SIZE		0x4A
#define GEOMETRY_DESC_HPB_DEVICE_MAX_ACTIVE_REGIONS	0x4B
#endif
#if defined(CONFIG_UFSTW)
#define GEOMETRY_DESC_TW_GROUP_NUM_CAP			0x4E
#define GEOMETRY_DESC_TW_MAX_SIZE			0x4F
#define GEOMETRY_DESC_TW_NUMBER_LU			0x53
#define GEOMETRY_DESC_TW_CAP_ADJ_FAC			0x54
#define GEOMETRY_DESC_TW_SUPPORT_USER_REDUCTION_TYPES	0x55
#define GEOMETRY_DESC_TW_SUPPORT_BUF_TYPE		0x56
#endif
#if defined(CONFIG_UFSRINGBUF)
#define GEOMETRY_DESC_RINGBUF_MAX_HIST_BUFSIZE		0x57
#endif

/* Exception event mask values */
#if defined(CONFIG_UFSTW)
enum {
	MASK_EE_TW		= (1 << 5),
};
#endif

/**
 * struct utp_upiu_task_req - Task request UPIU structure
 * @header - UPIU header structure DW0 to DW-2
 * @input_param1: Input parameter 1 DW-3
 * @input_param2: Input parameter 2 DW-4
 * @input_param3: Input parameter 3 DW-5
 * @reserved: Reserved double words DW-6 to DW-7
 */
struct utp_upiu_task_req {
	struct utp_upiu_header header;
	__be32 input_param1;
	__be32 input_param2;
	__be32 input_param3;
	__be32 reserved[2];
};

#endif /* End of Header */
