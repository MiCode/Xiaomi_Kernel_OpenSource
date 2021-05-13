/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CONAP_SCP_IPI_H__
#define __CONAP_SCP_IPI_H__

#include "conap_scp.h"

struct conap_scp_ipi_cb {
	void (*conap_scp_ipi_msg_notify) (uint16_t drv_type, uint16_t msg_id, uint32_t param0, uint32_t param1);
	void (*conap_scp_ipi_ctrl_notify) (unsigned int state);
};

unsigned int conap_scp_ipi_is_scp_ready(void);
int conap_scp_ipi_handshake(void);

int conap_scp_ipi_send(enum conap_scp_drv_type drv_type, uint16_t msg_id, uint32_t param0, uint32_t param1);

int conap_scp_ipi_is_drv_ready(enum conap_scp_drv_type drv_type);

int conap_scp_ipi_init(struct conap_scp_ipi_cb *cb);
int conap_scp_ipi_deinit(void);

#endif
