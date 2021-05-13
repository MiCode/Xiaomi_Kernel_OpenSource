/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CONAP_SCP_PRIV_H__
#define __CONAP_SCP_PRIV_H__

#include <linux/types.h>
#include "conap_scp.h"

typedef enum {
	CONAP_SCP_OPID_INIT_HANDSHAKE 		= 0,
	CONAP_SCP_OPID_SEND_MSG 			= 1,
	CONAP_SCP_OPID_DRV_READY			= 2,
	CONAP_SCP_OPID_MAX
} conap_scp_core_opid;

enum CONAP_SCP_CORE_MSG_ID {
	CONAP_SCP_CORE_INIT			= 0,
	CONAP_SCP_CORE_ACK			= 1,
	CONAP_SCP_CORE_DRV_QRY 		= 2,
	CONAP_SCP_CORE_DRV_QRY_ACK	= 3,
};

struct conap_scp_drv_user {
	unsigned char enable;
	unsigned int send_seq_num;
	struct conap_scp_drv_cb drv_cb;
};

int conap_scp_init_handshake(void);

int conap_scp_init(unsigned int chip_info, phys_addr_t emi_phy_addr);
int conap_scp_deinit(void);

#endif
