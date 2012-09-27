/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

int chk_equip_id_and_mask(int equip_id, uint8_t *buf);
void diag_send_event_mask_update(smd_channel_t *, int num_bytes);
void diag_send_msg_mask_update(smd_channel_t *, int ssid_first,
					 int ssid_last, int proc);
void diag_send_log_mask_update(smd_channel_t *, int);
int diag_process_apps_masks(unsigned char *buf, int len);
void diag_masks_init(void);
void diag_masks_exit(void);
extern int diag_event_num_bytes;
#endif
