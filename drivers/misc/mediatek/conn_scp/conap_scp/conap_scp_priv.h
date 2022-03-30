/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CONAP_SCP_PRIV_H__
#define __CONAP_SCP_PRIV_H__

#include <linux/completion.h>
#include <linux/types.h>
#include "conap_scp.h"

enum CONAP_SCP_CORE_OPID {
	CONAP_SCP_OPID_STATE_CHANGE			= 0,
	CONAP_SCP_OPID_SEND_MSG				= 1,
	CONAP_SCP_OPID_DRV_READY			= 2,
	CONAP_SCP_OPID_DRV_READY_ACK			= 3,
	CONAP_SCP_OPID_RECV_MSG				= 4,
	CONAP_SCP_OPID_MAX
};

enum CONAP_SCP_CORE_MSG_ID {
	CONAP_SCP_CORE_INIT		= 0,
	CONAP_SCP_CORE_ACK		= 1,
	CONAP_SCP_CORE_DRV_QRY		= 2,
	CONAP_SCP_CORE_DRV_QRY_ACK	= 3,
	CONAP_SCP_CORE_REQ_TX		= 4,
	CONAP_SCP_CORE_TX_ACCEP		= 5,
};

struct conap_scp_drv_user {
	unsigned char enable;
	struct conap_scp_drv_cb drv_cb;
	uint16_t total_sz;
	uint16_t recv_sz;
	struct completion is_rdy_comp;
	uint16_t is_rdy_ret;
};

int conap_scp_init(void);
int conap_scp_deinit(void);

#endif
