/*
 * mddp.c - MDDP IPC API between AP and MD.
 *
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/types.h>

#include "mddp_ipc.h"
#include "mddp_sm.h"

#include "mtk_ccci_common.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Global variables.
//------------------------------------------------------------------------------
static struct wfpm_smem_info_t smem_info_s[] = {
	{MDDP_MD_SMEM_USER_RX_REORDER_TO_MD, WFPM_SM_E_ATTRI_WO,
		0, 0, (80 * 1024)},
	{MDDP_MD_SMEM_USER_RX_REORDER_FROM_MD, WFPM_SM_E_ATTRI_RO,
		0, (80 * 1024), (80 * 1024)},
	{MDDP_MD_SMEM_USER_WIFI_STATISTICS, WFPM_SM_E_ATTRI_RO,
		0, (80 * 1024) << 1, (sizeof(struct mddpwh_net_stat_t))},
};

//------------------------------------------------------------------------------
// Private variables.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Private helper macro.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------
int32_t mddp_ipc_md_smem_layout_config(void)
{
	struct wfpm_smem_info_t  *smem_entry;
	uint32_t                i;
	uint32_t                total_len = 0;

	// Adjust offset of wfpm share memory
	for (i = 0; i < MDDP_MD_SMEM_USER_NUM; i++) {
		smem_entry = &smem_info_s[i];
		smem_entry->offset = total_len;
		total_len += smem_entry->size;
	}

	pr_info("%s: smem total_len(%d)!", __func__, total_len);
	return 0;
}

//------------------------------------------------------------------------------
// Public functions.
//------------------------------------------------------------------------------
int32_t mddp_ipc_init(void)
{
	mddp_ipc_md_smem_layout_config();

	return 0;
}

int32_t mddp_ipc_send_md(
	void *in_app,
	struct mddp_md_msg_t *msg,
	int32_t dest_mod)
{
	struct mddp_app_t      *app;
	struct ipc_ilm          ilm;
	struct local_para      *local_para;
	int32_t                 ret;

	if (in_app == NULL)
		app = mddp_get_default_app_inst();
	else
		app = (struct mddp_app_t *) in_app;

	local_para = kzalloc(sizeof(local_para) + msg->data_len,
			GFP_KERNEL);
	if (!local_para)
		return -ENOMEM;

	local_para->msg_len = sizeof(struct local_para) + msg->data_len;
	memcpy(local_para->data, msg->data, msg->data_len);

	memset(&ilm, 0, sizeof(ilm));
	ilm.src_mod_id = app->md_cfg.ipc_ap_mod_id;
	ilm.dest_mod_id = (dest_mod < 0)
		? (app->md_cfg.ipc_md_mod_id) : (dest_mod);
	ilm.msg_id = msg->msg_id;
	ilm.local_para_ptr = local_para;

	ret = ccci_ipc_send_ilm(0, &ilm);
	kfree(local_para);

	if (unlikely(ret < 0)) {
		pr_notice("%s: ccci_ipc_send_ilm error(%d)!",
				__func__, ret);
		return -EAGAIN;
	}

	return 0;
}

int32_t wfpm_ipc_get_smem_list(void **smem_info_base, uint32_t *smem_num)
{
	*smem_info_base = &smem_info_s;
	*smem_num = MDDP_MD_SMEM_USER_NUM;

	return 0;
}

int32_t mddp_ipc_get_md_smem_by_id(enum mddp_md_smem_user_id_e app_id,
		void **smem_addr, uint8_t *smem_attr, uint32_t *smem_size)
{
	struct wfpm_smem_info_t  *smem_entry;
	uint32_t                smem_total_len;

	smem_entry = &smem_info_s[app_id];
	*smem_attr = smem_entry->attribute;
	*smem_size = smem_entry->size;

	*smem_addr = (uint8_t *)get_smem_start_addr(MD_SYS1,
		SMEM_USER_RAW_USB, &smem_total_len) + smem_entry->offset;


	return 0;
}
