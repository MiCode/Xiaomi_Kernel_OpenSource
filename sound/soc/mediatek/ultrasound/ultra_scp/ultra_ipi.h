/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ultra_ipi.h --  Mediatek scp spk platform driver
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Ning Li <ning.li@mediatek.com>
 */

#ifndef __ULTRA_HW_H__
#define __ULTRA_HW_H__

#include <linux/types.h>

/* if IPI expand, need to modify maximum data length(unit: int) */

#define ULTRA_IPI_HEADER_LENGTH         2  /* 2 * 4byte = 8 */
#define ULTRA_IPI_SEND_BUFFER_LENGTH    13  /* 13 * 4byte = 52 */
#define ULTRA_IPI_RECEIVE_LENGTH        2 /* 2 * 4byte = 8 */
#define ULTRA_IPI_ACK_LENGTH            2  /* 2 * 4byte = 8 */

#define ULTRA_IPI_WAIT_ACK_TIMEOUT      10
#define ULTRA_IPI_RESEND_TIMES          2
#define ULTRA_IPI_SEND_CNT_TIMEOUT       500 /* 500ms */
#define ULTRA_WAITCHECK_INTERVAL_MS      1
enum {
	ULTRA_IPI_BYPASS_ACK = 0,
	ULTRA_IPI_NEED_ACK,
	ULTRA_IPI_ACK_BACK
};

/* AP -> SCP ipi structure */
struct ultra_ipi_send_info {
	unsigned char msg_id;
	unsigned char msg_need_ack;
	unsigned char param1;
	unsigned char param2;
	unsigned int msg_length;
	int payload[ULTRA_IPI_SEND_BUFFER_LENGTH];
};

/* SCP -> AP ipi structure */
struct ultra_ipi_receive_info {
	unsigned char msg_id;
	unsigned char msg_need_ack;
	unsigned char param1;
	unsigned char param2;
	unsigned int msg_length;
	unsigned int msg_data[ULTRA_IPI_RECEIVE_LENGTH];
};

/* SCP -> AP ipi ack structure */
struct ultra_ipi_ack_info {
	unsigned char msg_id;
	unsigned char msg_need_ack;
	unsigned char param1;
	unsigned char param2;
	unsigned int msg_data;
};

/* function */
unsigned int ultra_check_scp_status(void);

void ultra_ipi_register(void (*ipi_rx_call)(unsigned int, void *),
			bool (*ipi_tx_ack_call)(unsigned int, unsigned int));

bool ultra_ipi_send(unsigned int msg_id,
		    bool polling_mode,
		    unsigned int payload_len,
		    int *payload,
		    unsigned int need_ack);

bool ultra_GetScpRecoverStatus(void);

void ultra_SetScpRecoverStatus(bool recovering);


#endif /*__ULTRA_HW_H__ */
