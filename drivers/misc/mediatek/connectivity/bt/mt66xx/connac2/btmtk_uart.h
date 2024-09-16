/*
 *  Copyright (c) 2016,2017 MediaTek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _BTMTK_UART_H_
#define _BTMTK_UART_H_
#include "btmtk_define.h"


#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/serial.h>

#define HCI_HEADER_LEN	4

struct mtk_stp_hdr {
	u8	prefix;
	__be16	dlen;
	u8	cs;
} __packed;
#define MTK_STP_TLR_SIZE	2
#define STP_HEADER_LEN	4
#define STP_HEADER_CRC_LEN	2


struct btmtk_dev {
	struct hci_dev	   *hdev;
	struct tty_struct *tty;
	unsigned long	hdev_flags;

	/* For tx queue */
	struct sk_buff		*tx_skb;
	unsigned long		tx_state;

	/* For rx queue */
	struct sk_buff		*rx_skb;
	unsigned long		rx_state;

	struct sk_buff 		*evt_skb;
	wait_queue_head_t p_wait_event_q;

	unsigned int		subsys_reset;

	u8	stp_pad[6];
	u8	stp_cursor;
	u16	stp_dlen;
};


/**
 * Maximum rom patch file name length
 */
#define MAX_BIN_FILE_NAME_LEN 32

#define N_MTK        (15+1)
/**
 * Send cmd dispatch evt
 */
#define RETRY_TIMES 10
#define HCI_EV_VENDOR			0xff

#define N_MTK        (15+1)

/* Platform device */
#define BTMTK_GET_DEV(bdev)	(bdev->tty->dev)

int btmtk_cif_send_cmd(struct hci_dev *hdev, const uint8_t *cmd,
		const int cmd_len, int retry, int endpoint, unsigned long tx_state);
int btmtk_cif_send_calibration(struct hci_dev *hdev);
int btmtk_cif_dispatch_event(struct hci_dev *hdev, struct sk_buff *skb);
#endif

