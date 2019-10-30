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

//------------------------------------------------------------------------------
// Configuration.
// -----------------------------------------------------------------------------
#define MDDP_IPC_TTY_NAME			"ccci_0_200"
#define MDFPM_TTY_BUF_SZ            256

//------------------------------------------------------------------------------
// Enum. definition.
// -----------------------------------------------------------------------------
enum MDDP_MDFPM_MSG_ID_CODE {
	IPC_MSG_ID_MDFPM_USER_MSG_BEGIN,

	IPC_MSG_ID_DPFM_DATA_USAGE_CMD,
	IPC_MSG_ID_DPFM_SET_CT_TIMEOUT_VALUE_REQ,
	IPC_MSG_ID_DPFM_SET_CT_TIMEOUT_VALUE_RSP,
	IPC_MSG_ID_DPFM_CT_TIMEOUT_IND,

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
	IPC_MSG_ID_WFPM_MD_NOTIFY,
	IPC_MSG_ID_WFPM_RESET_IND,

	IPC_MSG_ID_UFPM_NOTIFY_MD_BUS_EVENT_RSP,
	IPC_MSG_ID_UFPM_NOTIFY_MD_BUS_EVENT_REQ,
	IPC_MSG_ID_UFPM_SEND_MD_USB_EP0_REQ,
	IPC_MSG_ID_UFPM_SEND_MD_USB_EP0_RSP,
	IPC_MSG_ID_UFPM_SEND_MD_USB_EP0_IND,
	IPC_MSG_ID_UFPM_SEND_AP_USB_EP0_IND,
	IPC_MSG_ID_UFPM_ENABLE_MD_FAST_PATH_REQ,
	IPC_MSG_ID_UFPM_ENABLE_MD_FAST_PATH_RSP,
	IPC_MSG_ID_UFPM_DISABLE_MD_FAST_PATH_REQ,
	IPC_MSG_ID_UFPM_DISABLE_MD_FAST_PATH_RSP,
	IPC_MSG_ID_UFPM_DEACTIVATE_MD_FAST_PATH_REQ,
	IPC_MSG_ID_UFPM_DEACTIVATE_MD_FAST_PATH_RSP,
	IPC_MSG_ID_UFPM_DEACTIVATE_MD_FAST_PATH_IND,
	IPC_MSG_ID_UFPM_ACTIVATE_MD_FAST_PATH_REQ,
	IPC_MSG_ID_UFPM_ACTIVATE_MD_FAST_PATH_RSP,
};

#define MDFPM_AP_USER_ID    0x13578642
enum mdfpm_user_id_e {
	MDFPM_USER_ID_UFPM,
	MDFPM_USER_ID_WFPM,
	MDFPM_USER_ID_DPFM,

	MDFPM_USER_NUM,

	MDFPM_USER_ID_NULL = 0x7fe,
	MDFPM_USER_DUMMY = 0x7ff, /* Make it a 2-byte enum */
};

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
#define MDFPM_CTRL_MSG_HEADER_SZ    \
		(sizeof(struct mdfpm_ctrl_msg_t) - MDFPM_TTY_BUF_SZ)
struct mdfpm_ctrl_msg_t {
	uint32_t                        dest_user_id;
	uint32_t                        msg_id;
	uint32_t                        buf_len;
	uint8_t	                        buf[MDFPM_TTY_BUF_SZ];
} __packed;

#if 0
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
#endif

struct mddp_ilm_common_rsp_t {
	u8 ref_count;
	u8 lp_reserved;
	u16 msg_len;
	struct ufpm_md_fast_path_common_rsp_t rsp;
} __packed;

struct mddp_md_msg_t {
	struct list_head                list;
	uint32_t                        msg_id;
	uint32_t                        data_len;
	uint8_t                         data[0];
};

struct mddp_ipc_rx_msg_entry_t {
	enum MDDP_MDFPM_MSG_ID_CODE     msg_id;
	uint32_t                        rx_msg_len;
};

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
int32_t mddp_ipc_init(void);
void mddp_ipc_uninit(void);
int32_t mddp_ipc_send_md(void *app, struct mddp_md_msg_t *msg,
		enum mdfpm_user_id_e dest_user);
int32_t wfpm_ipc_get_smem_list(void **smem_info_base, uint32_t *smem_num);
int32_t mddp_ipc_get_md_smem_by_id(enum mddp_md_smem_user_id_e app_id,
		void **smem_addr, uint8_t *smem_attr, uint32_t *smem_size);
bool mddp_ipc_rx_msg_validation(enum MDDP_MDFPM_MSG_ID_CODE msg_id,
		uint32_t msg_len);
#endif /* __MDDP_IPC_H */
