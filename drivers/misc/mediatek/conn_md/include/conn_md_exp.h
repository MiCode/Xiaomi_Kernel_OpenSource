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

#ifndef __CONN_MD_EXP_H_
#define __CONN_MD_EXP_H_

#include "port_ipc.h"		/*mediatek/kernel/drivers/eccci */
#include "ccci_ipc_task_ID.h"	/*mediatek/kernel/drivers/eccci */
#define uint32 unsigned int
#define uint8 unsigned char
#define uint16 unsigned short
#ifdef CHAR
#undef CHAR
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
