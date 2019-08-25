/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _UAPI_QBT_HANDLER_H_
#define _UAPI_QBT_HANDLER_H_

#define MAX_NAME_SIZE 32

#define QBT_IS_WUHB_CONNECTED    100
#define QBT_SEND_KEY_EVENT       101
#define QBT_ENABLE_IPC           102
#define QBT_DISABLE_IPC          103
#define QBT_ENABLE_FD            104
#define QBT_DISABLE_FD           105
#define QBT_CONFIGURE_TOUCH_FD   106
#define QBT_ACQUIRE_WAKELOCK     107
#define QBT_RELEASE_WAKELOCK     108

/*
 * enum qbt_finger_events -
 *      enumeration of qbt finger events
 * @QBT_EVENT_FINGER_UP - finger up detected
 * @QBT_EVENT_FINGER_DOWN - finger down detected
 * @QBT_EVENT_FINGER_MOVE - finger move detected
 */
enum qbt_finger_events {
	QBT_EVENT_FINGER_UP,
	QBT_EVENT_FINGER_DOWN,
	QBT_EVENT_FINGER_MOVE
};

/*
 * enum qbt_fw_event -
 *      enumeration of firmware events
 * @FW_EVENT_FINGER_DOWN - finger down detected
 * @FW_EVENT_FINGER_UP - finger up detected
 * @FW_EVENT_IPC - an IPC from the firmware is pending
 */
enum qbt_fw_event {
	FW_EVENT_FINGER_DOWN = 1,
	FW_EVENT_FINGER_UP = 2,
	FW_EVENT_IPC = 3,
};

/*
 * struct qbt_wuhb_connected_status -
 *		used to query whether WUHB INT line is connected
 * @is_wuhb_connected - if non-zero, WUHB INT line is connected
 */
struct qbt_wuhb_connected_status {
	bool is_wuhb_connected;
};

/*
 * struct qbt_key_event -
 *		used to send key event
 * @key - the key event to send
 * @value - value of the key event
 */
struct qbt_key_event {
	int key;
	int value;
};

/*
 * struct qbt_touch_config -
 *		used to configure touch finger detect
 * @rad_filter_enable - flag to enable/disable radius based filtering
 * @rad_x: movement radius in x direction
 * @rad_y: movement radius in y direction
 */
struct qbt_touch_config {
	bool rad_filter_enable;
	int rad_x;
	int rad_y;
};

#endif /* _UAPI_QBT_HANDLER_H_ */
