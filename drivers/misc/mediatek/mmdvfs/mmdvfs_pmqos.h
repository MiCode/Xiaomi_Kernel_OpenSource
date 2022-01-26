/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MMDVFS_PMQOS_H__
#define __MMDVFS_PMQOS_H__

#include <linux/pm_qos.h>

#define MAX_FREQ_STEP 6

enum {
	BW_THROTTLE_START = 1,
	BW_THROTTLE_START_RECOVER,
	BW_THROTTLE_END
};

enum {
	BW_COMP_NONE = 0,
	BW_COMP_DEFAULT,
	BW_COMP_VENC,
	BW_COMP_END
};

enum virtual_source_id {
	VIRTUAL_DISP = 0,
	VIRTUAL_MD,
	VIRTUAL_CCU_COMMON,
	VIRTUAL_CCU_COMMON2
};

struct mm_qos_request {
	struct plist_node owner_node;	/* To update all master once */
	struct list_head larb_node;	/* To update larb setting */
	struct list_head port_node;	/* To update ostd in the same port */
	u32 master_id;	/* larb and port combination */
	u32 bw_value;	/* Master data BW */
	u32 hrt_value;	/* Master hrt BW */
	u32 ostd;	/* Master ostd */
	u32 comp_type;	/* compression type */
	bool init;	/* initialized check */
	bool updated;	/* update check */
	struct pm_qos_request qos_request;	/* EMI setting */
};

enum mmdvfs_limit_source {
	MMDVFS_LIMIT_THERMAL = 0,
	MMDVFS_LIMIT_CAM,
};

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
s32 mm_qos_add_request(struct plist_head *owner_list,
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
void mm_qos_update_all_request(struct plist_head *owner_list);

/**
 * mm_qos_remove_all_request - remove all mm_qos_request items from owner_list
 *    call this API once when exit driver for efficiency
 * @owner_list: this list contains all mm_qos_request items from caller.
 */
void mm_qos_remove_all_request(struct plist_head *owner_list);

/**
 * mm_qos_update_all_request_zero - set zero to all mm_qos_request items of
 *    owner_list, and also call mm_qos_update_all_request to update
 *    system setting.
 *    Use this API when all requirements are ended.
 * @owner_list: each caller should have its owner list, mmdvfs use this
 *    owner_list to update related setting at once.
 */
void mm_qos_update_all_request_zero(struct plist_head *owner_list);

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

/**
 * mmdvfs_set_max_camera_hrt_bw - set maximum camera hrt bw
 * @bw: bandwidth size in MB/s
 */
void mmdvfs_set_max_camera_hrt_bw(u32 bw);

/**
 * mmdvfs_qos_get_freq_steps - get available freq steps of each pmqos class
 * @pm_qos_class: pm_qos_class of each mm freq domain
 * @freq_steps: output available freq_step settings, size is MAX_FREQ_STEP.
 *    If the entry is 0, it means step not available, size of available items
 *    is in step_size.
 *    The order of freq steps is from high to low.
 * @step_size: size of available items in freq_steps
 *
 * Returns 0, or -errno
 */
int mmdvfs_qos_get_freq_steps(
	u32 pm_qos_class, u64 *freq_steps, u32 *step_size);

/**
 * mmdvfs_qos_force_step - function to force mmdvfs setting ignore PMQoS update
 * @step: force step of mmdvfs
 *
 * Returns 0, or -errno
 */
int mmdvfs_qos_force_step(int step);

/**
 * mmdvfs_qos_enable - function to enable or disable mmdvfs
 * @enable: mmdvfs enable or disable
 */
void mmdvfs_qos_enable(bool enable);

/**
 * mmdvfs_autok_qos_enable - function to enable or disable mmdvfs for autok
 * @enable: mmdvfs enable or disable;
 *    mmdvfs will not enabled if it is already disabled by mmdvfs_qos_enable()
 */
void mmdvfs_autok_qos_enable(bool enable);

/**
 * mmdvfs_qos_get_freq - get current freq of each pmqos class
 * @pm_qos_class: pm_qos_class of each mm freq domain
 *
 * Returns {Freq} in MHz
 */
u64 mmdvfs_qos_get_freq(u32 pm_qos_class);


/**
 * mmdvfs_qos_limit_config - set limit setting of each pmqos class
 * @pm_qos_class: pm_qos_class of each mm freq domain
 * @limit_value: u32 type of limit value of each source.
 * @source: limit source
 */
void mmdvfs_qos_limit_config(u32 pm_qos_class, u32 limit_value,
	enum mmdvfs_limit_source source);

/**
 * mmdvfs_print_larbs_info - print larbs info to kernel log
 */
void mmdvfs_print_larbs_info(void);

enum mmdvfs_prepare_event {
	MMDVFS_PREPARE_CALIBRATION_START, MMDVFS_PREPARE_CALIBRATION_END
};

void mmdvfs_prepare_action(enum mmdvfs_prepare_event event);


s32 get_virtual_port(enum virtual_source_id id);

#endif /* __MMDVFS_PMQOS_H__ */
