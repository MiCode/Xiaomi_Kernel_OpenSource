/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MHL_MSC_H__
#define __MHL_MSC_H__
#include <linux/mhl_8334.h>

#define MAX_RCP_KEYS_SUPPORTED 256

#define MSC_NORMAL_SEND 0
#define MSC_PRIORITY_SEND 1

#define TMDS_ENABLE 1
#define TMDS_DISABLE 0

/******************************************************************/
/* the below APIs are implemented by the MSC functionality */
int mhl_msc_clear(struct mhl_tx_ctrl *mhl_ctrl);

int mhl_msc_command_done(struct mhl_tx_ctrl *mhl_ctrl,
			 struct msc_command_struct *req);

int mhl_msc_send_set_int(struct mhl_tx_ctrl *mhl_ctrl,
			 u8 offset, u8 mask, u8 priority);

int mhl_msc_send_write_stat(struct mhl_tx_ctrl *mhl_ctrl,
			    u8 offset, u8 value);
int mhl_msc_send_msc_msg(struct mhl_tx_ctrl *mhl_ctrl,
			 u8 sub_cmd, u8 cmd_data);

int mhl_msc_recv_set_int(struct mhl_tx_ctrl *mhl_ctrl,
			 u8 offset, u8 set_int);

int mhl_msc_recv_write_stat(struct mhl_tx_ctrl *mhl_ctrl,
			    u8 offset, u8 value);
int mhl_msc_recv_msc_msg(struct mhl_tx_ctrl *mhl_ctrl,
			 u8 sub_cmd, u8 cmd_data);
void mhl_msc_send_work(struct work_struct *work);

/******************************************************************/
/* Tx should implement these APIs */
int mhl_send_msc_command(struct mhl_tx_ctrl *mhl_ctrl,
			 struct msc_command_struct *req);
void mhl_read_scratchpad(struct mhl_tx_ctrl *mhl_ctrl);
void mhl_drive_hpd(struct mhl_tx_ctrl *mhl_ctrl, uint8_t to_state);
void mhl_tmds_ctrl(struct mhl_tx_ctrl *ctrl, uint8_t on);
/******************************************************************/
/* MHL driver registers ctrl with MSC */
void mhl_register_msc(struct mhl_tx_ctrl *ctrl);

#endif /* __MHL_MSC_H__ */
