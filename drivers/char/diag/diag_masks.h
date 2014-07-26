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
	uint32_t range;
	uint8_t *ptr;
} __packed;

struct diag_ssid_range_t {
	uint16_t ssid_first;
	uint16_t ssid_last;
} __packed;

struct diag_msg_mask_t {
	uint32_t ssid_first;
	uint32_t ssid_last;
	uint32_t range;
	uint32_t *ptr;
} __packed;

struct diag_log_config_req_t {
	uint8_t cmd_code;
	uint8_t padding[3];
	uint32_t sub_cmd;
	uint32_t equip_id;
	uint32_t num_items;
} __packed;

struct diag_log_config_rsp_t {
	uint8_t cmd_code;
	uint8_t padding[3];
	uint32_t sub_cmd;
	uint32_t status;
} __packed;

struct diag_log_config_set_rsp_t {
	uint8_t cmd_code;
	uint8_t padding[3];
	uint32_t sub_cmd;
	uint32_t status;
	uint32_t equip_id;
	uint32_t num_items;
} __packed;

struct diag_log_on_demand_rsp_t {
	uint8_t cmd_code;
	uint16_t log_code;
	uint8_t status;
} __packed;

struct diag_event_report_t {
	uint8_t cmd_code;
	uint16_t padding;
} __packed;

struct diag_event_mask_config_t {
	uint8_t cmd_code;
	uint8_t status;
	uint16_t padding;
	uint16_t num_bits;
} __packed;

struct diag_msg_config_rsp_t {
	uint8_t cmd_code;
	uint8_t sub_cmd;
	uint8_t status;
	uint8_t padding;
	uint32_t rt_mask;
} __packed;

struct diag_msg_ssid_query_t {
	uint8_t cmd_code;
	uint8_t sub_cmd;
	uint8_t status;
	uint8_t padding;
	uint32_t count;
} __packed;

struct diag_build_mask_req_t {
	uint8_t cmd_code;
	uint8_t sub_cmd;
	uint16_t ssid_first;
	uint16_t ssid_last;
} __packed;

struct diag_msg_build_mask_t {
	uint8_t cmd_code;
	uint8_t sub_cmd;
	uint16_t ssid_first;
	uint16_t ssid_last;
	uint8_t status;
	uint8_t padding;
} __packed;

struct diag_msg_mask_userspace_t {
	uint32_t ssid_first;
	uint32_t ssid_last;
	uint32_t range;
} __packed;

struct diag_log_mask_userspace_t {
	uint8_t equip_id;
	uint32_t num_items;
} __packed;

#define MAX_EQUIP_ID	16
#define MSG_MASK_SIZE	(MSG_MASK_TBL_CNT * sizeof(struct diag_msg_mask_t))
#define LOG_MASK_SIZE	(MAX_EQUIP_ID * sizeof(struct diag_log_mask_t))
#define EVENT_MASK_SIZE 513
#define MAX_ITEMS_PER_EQUIP_ID	512

#define LOG_MASK_CTRL_HEADER_LEN	11
#define MSG_MASK_CTRL_HEADER_LEN	11
#define EVENT_MASK_CTRL_HEADER_LEN	7

#define LOG_STATUS_SUCCESS	0
#define LOG_STATUS_INVALID	1
#define LOG_STATUS_FAIL		2

#define MSG_STATUS_FAIL		0
#define MSG_STATUS_SUCCESS	1

#define EVENT_STATUS_SUCCESS	0
#define EVENT_STATUS_FAIL	1

#define DIAG_CTRL_MASK_INVALID		0
#define DIAG_CTRL_MASK_ALL_DISABLED	1
#define DIAG_CTRL_MASK_ALL_ENABLED	2
#define DIAG_CTRL_MASK_VALID		3

extern struct diag_mask_info msg_mask;
extern struct diag_mask_info msg_bt_mask;
extern struct diag_mask_info log_mask;
extern struct diag_mask_info event_mask;

void diag_mask_update_fn(struct work_struct *work);
int diag_process_apps_masks(unsigned char *buf, int len);
int diag_masks_init(void);
void diag_masks_exit(void);

extern int diag_create_msg_mask_table_entry(struct diag_msg_mask_t *msg_mask,
					    struct diag_ssid_range_t *range);
extern int diag_copy_to_user_msg_mask(char __user *buf, size_t count);
extern int diag_copy_to_user_log_mask(char __user *buf, size_t count);
#endif
