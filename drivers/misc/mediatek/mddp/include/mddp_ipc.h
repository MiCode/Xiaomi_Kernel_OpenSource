/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mddp_ipc.h - Structure used for AP/MD communication.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDDP_IPC_H
#define __MDDP_IPC_H

#include "mddp_wifi_def.h"

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

	IPC_MSG_ID_RESERVED_01,
	IPC_MSG_ID_RESERVED_02,
	IPC_MSG_ID_RESERVED_03,
	IPC_MSG_ID_RESERVED_04,
	IPC_MSG_ID_RESERVED_05,
	IPC_MSG_ID_RESERVED_06,
	IPC_MSG_ID_RESERVED_07,
	IPC_MSG_ID_RESERVED_08,
	IPC_MSG_ID_RESERVED_09,
	IPC_MSG_ID_RESERVED_10,
	IPC_MSG_ID_RESERVED_11,
	IPC_MSG_ID_RESERVED_12,
	IPC_MSG_ID_RESERVED_13,
	IPC_MSG_ID_RESERVED_14,
	IPC_MSG_ID_RESERVED_15,

	IPC_MSG_ID_MDFPM_EM_TEST_REQ,
	IPC_MSG_ID_MDFPM_SUSPEND_TAG_IND,
	IPC_MSG_ID_MDFPM_SUSPEND_TAG_ACK,
	IPC_MSG_ID_MDFPM_RESUME_TAG_IND,
	IPC_MSG_ID_MDFPM_RESUME_TAG_ACK,

	IPC_MSG_ID_WFPM_SEND_SMEM_LAYOUT_NOTIFY,
};

#define MDFPM_AP_USER_ID    0x13578642
enum mdfpm_user_id_e {
	MDFPM_USER_ID_RESERVED_1,
	MDFPM_USER_ID_WFPM,
	MDFPM_USER_ID_DPFM,
	MDFPM_USER_ID_MDFPM,

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
