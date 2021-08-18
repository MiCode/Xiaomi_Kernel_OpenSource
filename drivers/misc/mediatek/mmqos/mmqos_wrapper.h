/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MMQOS_WRAPPER_H__
#define __MMQOS_WRAPPER_H__
#include <dt-bindings/interconnect/mtk,mmqos.h>
//#include <linux/interconnect.h>
#include "mtk-interconnect.h"
#include <soc/mediatek/mmqos.h>
#include "smi_master_port.h"
enum {
	BW_COMP_NONE = 0,
	BW_COMP_DEFAULT,
	BW_COMP_END
};
enum virtual_source_id {
	VIRTUAL_DISP = 0,
	VIRTUAL_CCU_COMMON
};
struct mm_qos_request {
	struct list_head owner_node;	/* To update all master once */
	u32 master_id;	/* larb and port combination */
	u32 bw_value;	/* Master data BW */
	u32 hrt_value;	/* Master hrt BW */
	u32 comp_type;	/* compression type */
	bool init;	/* initialized check */
	bool updated;	/* update check */
	struct icc_path *icc_path;
};
#if IS_ENABLED(CONFIG_INTERCONNECT_MTK_MMQOS_COMMON)
/**
 * mm_qos_add_request - add mm_qos_request into owner_list
 *    call this API once when init driver for efficiency
 * @owner_list: each caller should have its owner list, mmdvfs use this
 *    owner_list to update related setting at once.
 * @req: mm_qos_request to be used for mm_qos mechanism.
 * @master_id: master ID of this request, use SMI_PMQOS_ENC to construct it.
 *
 * Returns 0, or -errno
 */
s32 mm_qos_add_request(struct list_head *owner_list,
	struct mm_qos_request *req, u32 master_id);
/**
 * mm_qos_set_request - set requirement to adjust system setting
 *    this API is only used to prepare the setting, call
 *    mm_qos_update_all_request to accurately update system setting.
 * @req: mm_qos_request to be indicated for related master.
 * @bw_value: mm qos requirement
 * @hrt_value: mm qos HRT requirement
 * @comp_type: if mm qos requirement has compression or not (BW_COMP_XXX)
 *
 * Returns 0, or -errno
 */
s32 mm_qos_set_request(struct mm_qos_request *req,
	u32 bw_value, u32 hrt_value, u32 comp_type);
/**
 * mm_qos_set_bw_request - set mm qos bw requirement
 *    same as mm_qos_set_request, but configure bw_value only.
 * @req: mm_qos_request to be indicated for related master.
 * @bw_value: mm qos requirement
 * @comp_type: if mm qos requirement has compression or not (BW_COMP_XXX)
 *
 * Returns 0, or -errno
 */
s32 mm_qos_set_bw_request(struct mm_qos_request *req,
	u32 bw_value, s32 comp_type);
/**
 * mm_qos_set_hrt_request - set mm qos hrt requirement
 *    same as mm_qos_set_request, but configure hrt_value only.
 * @req: mm_qos_request to be indicated for related master.
 * @hrt_value: mm qos HRT requirement
 *
 * Returns 0, or -errno
 */
s32 mm_qos_set_hrt_request(struct mm_qos_request *req, u32 hrt_value);
/**
 * mm_qos_update_all_request - update configured requirement to system setting
 * @owner_list: this list contains all mm_qos_request items from caller
 */
void mm_qos_update_all_request(struct list_head *owner_list);
/**
 * mm_qos_remove_all_request - remove all mm_qos_request items from owner_list
 *    call this API once when exit driver for efficiency
 * @owner_list: this list contains all mm_qos_request items from caller.
 */
void mm_qos_remove_all_request(struct list_head *owner_list);
/**
 * mm_qos_update_all_request_zero - set zero to all mm_qos_request items of
 *    owner_list, and also call mm_qos_update_all_request to update
 *    system setting.
 *    Use this API when all requirements are ended.
 * @owner_list: each caller should have its owner list, mmdvfs use this
 *    owner_list to update related setting at once.
 */
void mm_qos_update_all_request_zero(struct list_head *owner_list);
/**
 * mm_hrt_get_available_hrt_bw - return available HRT BW of the larb with
 *    master_id.
 *    Return value=Total available HRT BW-HRT BW of the larb with master_id
 * @master_id: master ID of this request, use SMI_PMQOS_ENC to construct it.
 *
 * Returns BW in MB/s, or negative value if dram info is not ready
 */
s32 mm_hrt_get_available_hrt_bw(u32 master_id);
/**
 * mm_hrt_add_bw_throttle_notifier - register a notifier_block to receive
 *    notification when BW is needed to throttle.
 * @nb: pointer of notifier_block
 *
 * Returns 0, or -errno
 */
s32 mm_hrt_add_bw_throttle_notifier(struct notifier_block *nb);
/**
 * mm_hrt_remove_bw_throttle_notifier - unregister the notifier_block
 * @nb: pointer of notifier_block
 *
 * Returns 0, or -errno
 */
s32 mm_hrt_remove_bw_throttle_notifier(struct notifier_block *nb);
s32 get_virtual_port(enum virtual_source_id id);
#else
static inline s32 mm_qos_add_request(struct list_head *owner_list,
	struct mm_qos_request *req, u32 master_id)
	{ return 0; }
static inline s32 mm_qos_set_request(struct mm_qos_request *req, u32 bw_value,
	u32 hrt_value, s32 comp_type)
	{ return 0; }
static inline s32 mm_qos_set_bw_request(struct mm_qos_request *req,
	u32 bw_value, s32 comp_type)
	{ return 0; }
static inline s32 mm_qos_set_hrt_request(struct mm_qos_request *req,
	u32 hrt_value)
	{ return 0; }
static inline void mm_qos_update_all_request(struct list_head *owner_list)
	{ return; }
static inline void mm_qos_remove_all_request(struct list_head *owner_list)
	{ return; }
static inline void mm_qos_update_all_request_zero(
	struct list_head *owner_list)
	{ return; }
static inline s32 mm_hrt_get_available_hrt_bw(u32 master_id)
	{ return -1; }
static inline s32 mm_hrt_add_bw_throttle_notifier(struct notifier_block *nb)
	{ return 0; }
static inline s32 mm_hrt_remove_bw_throttle_notifier(struct notifier_block *nb)
	{ return 0; }
static inline s32 get_virtual_port(enum virtual_source_id id)
	{ return 0; }
#endif
#endif /* __MMQOS_WRAPPER_H__ */

