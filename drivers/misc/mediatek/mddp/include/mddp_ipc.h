/*
 * mddp_ipc.h - Structure used for AP/MD communication.
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

#ifndef __MDDP_IPC_H
#define __MDDP_IPC_H

#include "mddp_usb_def.h"
#include "mddp_wifi_def.h"

#include "port_ipc.h"
#include "ccci_ipc_task_ID.h"
#include "ccci_ipc_msg_id.h"

#if defined CONFIG_MTK_MDDP_WH_SUPPORT || defined CONFIG_MTK_MCIF_WIFI_SUPPORT
enum MDDP_IPC_MSG_ID_CODE {
	IPC_MSG_ID_WFPM_INVALID = IPC_WFPM_MSG_ID_BEGIN,
	IPC_MSG_ID_WFPM_ENABLE_MD_FAST_PATH_REQ,
	IPC_MSG_ID_WFPM_ENABLE_MD_FAST_PATH_RSP,
	IPC_MSG_ID_WFPM_DISABLE_MD_FAST_PATH_REQ,
	IPC_MSG_ID_WFPM_DISABLE_MD_FAST_PATH_RSP,
	IPC_MSG_ID_WFPM_DEACTIVATE_MD_FAST_PATH_REQ,
	IPC_MSG_ID_WFPM_DEACTIVATE_MD_FAST_PATH_RSP,
	IPC_MSG_ID_WFPM_DEACTIVATE_MD_FAST_PATH_IND,
	IPC_MSG_ID_WFPM_ACTIVATE_MD_FAST_PATH_REQ,
	IPC_MSG_ID_WFPM_ACTIVATE_MD_FAST_PATH_RSP,
	IPC_MSG_ID_WFPM_SEND_MD_TXD_NOTIFY,
	IPC_MSG_ID_WFPM_DRV_NOTIFY,
	IPC_MSG_ID_WFPM_END,
};
#endif

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
struct mddp_ilm_enable_t {
	u8 ref_count;
	u8 lp_reserved;
	u16 msg_len;
	struct ufpm_enable_md_func_req_t req;
} __packed;

struct mddp_ilm_disable_t {
	u8 ref_count;
	u8 lp_reserved;
	u16 msg_len;
	struct ufpm_md_fast_path_common_req_t req;
} __packed;

struct mddp_ilm_act_t {
	u8 ref_count;
	u8 lp_reserved;
	u16 msg_len;
	struct ufpm_activate_md_func_req_t req;
} __packed;

struct mddp_ilm_deact_t {
	u8 ref_count;
	u8 lp_reserved;
	u16 msg_len;
	struct ufpm_md_fast_path_common_req_t req;
} __packed;

struct mddp_ilm_common_rsp_t {
	u8 ref_count;
	u8 lp_reserved;
	u16 msg_len;
	struct ufpm_md_fast_path_common_rsp_t rsp;
} __packed;

struct mddp_md_msg_t {
	struct list_head        list;
	uint32_t                msg_id;
	uint32_t                data_len;
	uint8_t                 data[0];
};

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
int32_t mddp_ipc_init(void);
int32_t mddp_ipc_send_md(void *app, struct mddp_md_msg_t *msg,
		int32_t dest_mod);
int32_t wfpm_ipc_get_smem_list(void **smem_info_base, uint32_t *smem_num);
int32_t mddp_ipc_get_md_smem_by_id(enum mddp_md_smem_user_id_e app_id,
		void **smem_addr, uint8_t *smem_attr, uint32_t *smem_size);

#endif /* __MDDP_IPC_H */
