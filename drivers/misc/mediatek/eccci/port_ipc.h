/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __PORT_IPC_H__
#define __PORT_IPC_H__

#include <asm/types.h>
#include <linux/compiler.h>
#include "ccci_config.h" /* for platform override */
#include "ccci_common_config.h"

/* MD <-> AP Msg_id mapping enum */
enum CCCI_IPC_MSG_ID_RANGE {
	IPC_L4C_MSG_ID_BEGIN = 0x80000000,
#if defined(IPC_L4C_MSG_ID_LEN)
	IPC_L4C_MSG_ID_RANGE = IPC_L4C_MSG_ID_LEN,
#else
	IPC_L4C_MSG_ID_RANGE = 0x80,
#endif
	IPC_EL1_MSG_ID_BEGIN = IPC_L4C_MSG_ID_BEGIN + IPC_L4C_MSG_ID_RANGE,
	IPC_EL1_MSG_ID_RANGE = 0x20,
	IPC_CCCIIPC_MSG_ID_BEGIN = IPC_EL1_MSG_ID_BEGIN + IPC_EL1_MSG_ID_RANGE,
	IPC_CCCIIPC_MSG_ID_RANGE = 0x10,
	IPC_IPCORE_MSG_ID_BEGIN =
		IPC_CCCIIPC_MSG_ID_BEGIN + IPC_CCCIIPC_MSG_ID_RANGE,
	IPC_IPCORE_MSG_ID_RANGE = 0x8,
	IPC_MDT_MSG_ID_BEGIN =
		IPC_IPCORE_MSG_ID_BEGIN + IPC_IPCORE_MSG_ID_RANGE,
	IPC_MDT_MSG_ID_RANGE = 0x8,
	IPC_UFPM_MSG_ID_BEGIN =
		IPC_MDT_MSG_ID_BEGIN + IPC_MDT_MSG_ID_RANGE,
	IPC_UFPM_MSG_ID_RANGE = 0x18,
};

struct local_para {
	u8 ref_count;
	u8 _stub; /* MD complier will align ref_count to 16bit */
	u16 msg_len;
	u8 data[0];
} __packed;

struct peer_buff {
	u16 pdu_len;
	u8 ref_count;
	u8 pb_resvered;
	u16 free_header_space;
	u16 free_tail_space;
	u8 data[0];
} __packed;

struct ipc_ilm {
	u32 src_mod_id;
	u32 dest_mod_id;
	u32 sap_id;
	u32 msg_id;
	struct local_para *local_para_ptr;
	struct peer_buff *peer_buff_ptr;
}; /* for conn_md */

struct ccci_emi_info {
	u8 ap_domain_id;
	u8 md_domain_id;
	u8 reserve[6];
	u64 ap_view_bank0_base;
	u64 bank0_size;
	u64 ap_view_bank4_base;
	u64 bank4_size;
} __packed; /* for USB direct tethering */

/* export API */
int ccci_ipc_send_ilm(int md_id, struct ipc_ilm *in_ilm);
int ccci_get_emi_info(int md_id, struct ccci_emi_info *emi_info);

#endif				/* __PORT_IPC_H__ */
