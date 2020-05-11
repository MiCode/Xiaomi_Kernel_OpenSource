/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CONN_MD_EXP_H_
#define __CONN_MD_EXP_H_

#ifdef KERNEL_5_4_NOT_FINISH_PORTING
#include "port_ipc.h"		/*mediatek/kernel/drivers/eccci */
#include "ccci_ipc_task_ID.h"	/*mediatek/kernel/drivers/eccci */
#endif
#define uint32 unsigned int
#define uint8 unsigned char
#define uint16 unsigned short
#ifdef CHAR
#undef CHAR
#endif

#ifndef KERNEL_5_4_NOT_FINISH_PORTING
#define    MD_MOD_EL1    5

struct local_para {
	unsigned char ref_count;
	unsigned char _stub; /* MD complier will align ref_count to 16bit */
	unsigned short msg_len;
	unsigned char data[0];
} __packed;

struct peer_buff {
	unsigned short pdu_len;
	unsigned char ref_count;
	unsigned char pb_resvered;
	unsigned short free_header_space;
	unsigned short free_tail_space;
	unsigned char data[0];
} __packed;

struct ipc_ilm {
	unsigned long src_mod_id;
	unsigned long dest_mod_id;
	unsigned long sap_id;
	unsigned long msg_id;
	struct local_para *local_para_ptr;
	struct peer_buff *peer_buff_ptr;
}; /* for conn_md */
#endif

/*
 * Provide a common conn_md_ipc_ilm_t definition for wmt_drv.ko to reference,
 * this decouples wmt_drv.ko away from ECCCI's ipc_ilm definition, as its
 * naming varies between different Kernel versions currently.
 * If in future the fields in this struct varies as well, we could either:
 * 1. Redefine entire struct from conn_md, or
 * 2. Let wmt_drv.ko handles the difference via:
 *      #if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
 */
#define conn_md_ipc_ilm_t struct ipc_ilm

enum CONN_MD_ERR_CODE {
	CONN_MD_ERR_NO_ERR = 0,
	CONN_MD_ERR_DEF_ERR = -1,
	CONN_MD_ERR_INVALID_PARAM = -2,
	CONN_MD_ERR_OTHERS = -4,
};

/*For IDC test*/
typedef int (*conn_md_msg_rx_cb) (struct ipc_ilm *ilm);

struct conn_md_bridge_ops {
	conn_md_msg_rx_cb rx_cb;
};

extern int mtk_conn_md_bridge_reg(uint32 u_id,
				struct conn_md_bridge_ops *p_ops);
extern int mtk_conn_md_bridge_unreg(uint32 u_id);
extern int mtk_conn_md_bridge_send_msg(struct ipc_ilm *ilm);

#endif /*__CONN_MD_EXP_H_*/
