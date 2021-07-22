/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CONAP_SCP_IPI_H__
#define __CONAP_SCP_IPI_H__

#include "conap_scp.h"


struct msg_cmd {
	uint16_t drv_type;
	uint16_t msg_id;
	uint16_t total_sz;
	uint16_t this_sz;
	uint32_t param0;
	uint32_t param1;
};


struct conap_scp_ipi_cb {
	void (*conap_scp_ipi_msg_notify)(uint16_t drv_type, uint16_t msg_id,
			uint16_t total_sz, uint8_t *data, uint32_t this_sz);
	void (*conap_scp_ipi_ctrl_notify)(unsigned int state);
};

unsigned int conap_scp_ipi_is_scp_ready(void);
unsigned int conap_scp_ipi_msg_sz(void);

int conap_scp_ipi_send_data(enum conap_scp_drv_type drv_type, uint16_t msg_id, uint32_t total_sz,
						uint8_t *buf, uint32_t size);
int conap_scp_ipi_send_cmd(enum conap_scp_drv_type drv_type, uint16_t msg_id,
						uint32_t p0, uint32_t p1);
int conap_scp_ipi_is_drv_ready(enum conap_scp_drv_type drv_type);

int conap_scp_ipi_init(struct conap_scp_ipi_cb *cb);
int conap_scp_ipi_deinit(void);

#endif
