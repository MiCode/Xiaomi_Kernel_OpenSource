/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __VOW_HW_H__
#define __VOW_HW_H__

#include <linux/types.h>

enum {
	VOW_IPI_BYPASS_ACK = 0,
	VOW_IPI_NEED_ACK
};

/* function */
unsigned int vow_check_scp_status(void);

void vow_ipi_register(void (*ipi_rx_call)(unsigned int, void *),
		      bool (*ipi_tx_ack_call)(unsigned int, unsigned int));

bool vow_ipi_send(unsigned int msg_id,
		  unsigned int payload_len,
		  unsigned int *payload,
		  unsigned int need_ack);
#endif /*__VOW_HW_H__ */
