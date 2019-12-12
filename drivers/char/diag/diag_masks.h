/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2013-2015, 2017-2019 The Linux Foundation. All rights reserved.
 */

#ifndef DIAG_MASKS_H
#define DIAG_MASKS_H

#include "diagfwd.h"

struct diag_log_mask_t {
	uint8_t equip_id;
	uint32_t num_items;
	uint32_t num_items_tools;
	uint32_t range;
	uint32_t range_tools;
	uint8_t id_valid;
	uint32_t sub_id;
	struct mutex lock;
	uint8_t *ptr;
};

struct diag_ssid_range_t {
	uint16_t ssid_first;
	uint16_t ssid_last;
} __packed;

struct diag_msg_mask_t {
	uint32_t ssid_first;
	uint32_t ssid_last;
	uint32_t ssid_last_tools;
	uint32_t range;
	uint32_t range_tools;
	uint8_t id_valid;
	uint32_t sub_id;
	struct mutex lock;
	uint32_t *ptr;
};

struct diag_log_config_get_req_t {
	uint8_t cmd_code;
	uint8_t padding[3];
	uint32_t sub_cmd;
	uint32_t equip_id;
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
#define MAX_ITEMS_ALLOWED	0xFFF

#define LOG_MASK_CTRL_HEADER_LEN	11
#define MSG_MASK_CTRL_HEADER_LEN	11
#define EVENT_MASK_CTRL_HEADER_LEN	7

#define LOG_MASK_CTRL_HEADER_LEN_SUB	18
#define MSG_MASK_CTRL_HEADER_LEN_SUB	18
#define EVENT_MASK_CTRL_HEADER_LEN_SUB	14

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

#define MAX_SIM_NUM 2
#define INVALID_INDEX -1
#define LEGACY_MASK_CMD 0
#define SUBID_CMD 1

struct diag_build_mask_req_sub_t {
	struct diag_pkt_header_t header;
	uint8_t version;
	uint8_t id_valid;
	uint32_t sub_id;
	uint8_t sub_cmd;
	uint16_t ssid_first;
	uint16_t ssid_last;
} __packed;

struct diag_msg_build_mask_sub_t {
	struct diag_pkt_header_t header;
	uint8_t version;
	uint8_t id_valid;
	uint32_t sub_id;
	uint8_t sub_cmd;
	uint8_t reserved;
	uint8_t status;
	uint16_t ssid_first;
	uint16_t ssid_last;
} __packed;

struct diag_msg_ssid_query_sub_t {
	struct diag_pkt_header_t header;
	uint8_t version;
	uint8_t id_valid;
	uint32_t sub_id;
	uint8_t sub_cmd;
	uint8_t status;
	uint8_t reserved;
	uint32_t count;
} __packed;

struct diag_msg_config_rsp_sub_t {
	struct diag_pkt_header_t header;
	uint8_t version;
	uint8_t id_valid;
	uint32_t sub_id;
	uint8_t sub_cmd;
	uint8_t preset_id;
	uint8_t status;
	uint32_t rt_mask;
} __packed;

struct diag_msg_config_set_sub_t {
	struct diag_pkt_header_t header;
	uint8_t version;
	uint8_t id_valid;
	uint32_t sub_id;
	uint8_t sub_cmd;
	uint8_t preset_id;
	uint8_t status;
	uint16_t ssid_first;
	uint16_t ssid_last;
	uint32_t rt_mask;
} __packed;

struct diag_log_config_req_sub_t {
	struct diag_pkt_header_t header;
	uint8_t version;
	uint8_t id_valid;
	uint32_t sub_id;
	uint8_t operation_code;
} __packed;

struct diag_log_config_rsp_sub_t {
	struct diag_pkt_header_t header;
	uint8_t version;
	uint8_t id_valid;
	uint32_t sub_id;
	uint8_t operation_code;
	uint8_t preset_id;
	uint8_t status;
} __packed;

struct diag_logging_range_t {
	uint32_t equip_id;
	uint32_t num_items;
} __packed;

struct diag_event_mask_config_sub_t {
	struct diag_pkt_header_t header;
	uint8_t version;
	uint8_t id_valid;
	uint32_t sub_id;
	uint8_t sub_cmd;
	uint8_t preset_id;
	uint8_t status;
	uint16_t num_bits;
} __packed;

struct diag_event_mask_req_sub_t {
	struct diag_pkt_header_t header;
	uint8_t version;
	uint8_t id_valid;
	uint32_t sub_id;
	uint8_t sub_cmd;
	uint8_t preset_id;
	uint8_t status;
} __packed;

int diag_check_subid_mask_index(uint32_t subid, int pid);
int diag_masks_init(void);
void diag_masks_exit(void);
int diag_log_mask_copy(struct diag_mask_info *dest,
		       struct diag_mask_info *src);
int diag_msg_mask_copy(struct diag_md_session_t *new_session,
	struct diag_mask_info *dest, struct diag_mask_info *src);
int diag_event_mask_copy(struct diag_mask_info *dest,
			 struct diag_mask_info *src);
void diag_log_mask_free(struct diag_mask_info *mask_info);
void diag_msg_mask_free(struct diag_mask_info *mask_info,
	struct diag_md_session_t *session_info);
void diag_event_mask_free(struct diag_mask_info *mask_info);
int diag_process_apps_masks(unsigned char *buf, int len, int pid);
void diag_send_updates_peripheral(uint8_t peripheral);

extern int diag_create_msg_mask_table_entry(struct diag_msg_mask_t *msg_mask,
			struct diag_ssid_range_t *range, int subid_index);
extern int diag_copy_to_user_msg_mask(char __user *buf, size_t count,
				      struct diag_md_session_t *info);
extern int diag_copy_to_user_log_mask(char __user *buf, size_t count,
				      struct diag_md_session_t *info);
#endif
