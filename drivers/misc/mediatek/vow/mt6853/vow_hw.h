/*
 * Copyright (C) 2020 MediaTek Inc.
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

/* if IPI expand, need to modify maximum data length(unit: int) */

#define VOW_IPI_HEADER_LENGTH         2  /* 2 * 4byte = 8 */
#define VOW_IPI_SEND_BUFFER_LENGTH    7  /* 7 * 4byte = 28 */
#define VOW_IPI_RECEIVE_LENGTH        24 /* 24 * 4byte = 96 */
#define VOW_IPI_ACK_LENGTH            2  /* 2 * 4byte = 8 */

#define VOW_IPI_WAIT_ACK_TIMEOUT      10
#define VOW_IPI_RESEND_TIMES          2

enum {
	VOW_IPI_BYPASS_ACK = 0,
	VOW_IPI_NEED_ACK,
	VOW_IPI_ACK_BACK
};

/* AP -> SCP ipi structure */
struct vow_ipi_send_info {
	unsigned char msg_id;
	unsigned char msg_need_ack;
	unsigned char param1;
	unsigned char param2;
	unsigned int msg_length;
	unsigned int payload[VOW_IPI_SEND_BUFFER_LENGTH];
};

/* SCP -> AP ipi structure */
struct vow_ipi_receive_info {
	unsigned char msg_id;
	unsigned char msg_need_ack;
	unsigned char param1;
	unsigned char param2;
	unsigned int msg_length;
	unsigned int msg_data[VOW_IPI_RECEIVE_LENGTH];
};

/* SCP -> AP ipi ack structure */
struct vow_ipi_ack_info {
	unsigned char msg_id;
	unsigned char msg_need_ack;
	unsigned char param1;
	unsigned char param2;
	unsigned int msg_data;
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
