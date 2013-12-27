/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef DIAG_MASKS_H
#define DIAG_MASKS_H

#include "diagfwd.h"

struct diag_log_mask_t {
	uint8_t equip_id;
	uint32_t num_items;
	uint8_t ptr[MAX_ITEMS_PER_EQUIP_ID];
} __packed;

void diag_send_event_mask_update(struct diag_smd_info *smd_info, int num_bytes);
void diag_send_msg_mask_update(struct diag_smd_info *smd_info, int ssid_first,
					 int ssid_last, int proc);
void diag_send_log_mask_update(struct diag_smd_info *smd_info, int);
void diag_mask_update_fn(struct work_struct *work);
void diag_send_feature_mask_update(struct diag_smd_info *smd_info);
int diag_process_apps_masks(unsigned char *buf, int len);
void diag_masks_init(void);
void diag_masks_exit(void);
extern int diag_event_num_bytes;
#endif
