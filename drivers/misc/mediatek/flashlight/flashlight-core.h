/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _FLASHLIGHT_CORE_H
#define _FLASHLIGHT_CORE_H

#include <linux/list.h>
#include "flashlight.h"

/* protocol version */
#define FLASHLIGHT_PROTOCOL_VERSION 2

/* sysfs - power throttling */
#define PT_NOTIFY_NUM 4
#define PT_NOTIFY_LOW_VOL  0
#define PT_NOTIFY_LOW_BAT  1
#define PT_NOTIFY_OVER_CUR 2
#define PT_NOTIFY_STRICT   3

/* sysfs - sw disable*/
#define FLASHLIGHT_SW_DISABLE_NUM    2
#define FLASHLIGHT_SW_DISABLE_TYPE   0
#define FLASHLIGHT_SW_DISABLE_STATUS 1
#define FLASHLIGHT_SW_DISABLE_STATUS_TMPBUF_SIZE 9
#define FLASHLIGHT_SW_DISABLE_STATUS_BUF_SIZE \
	(FLASHLIGHT_TYPE_MAX * FLASHLIGHT_CT_MAX * FLASHLIGHT_PART_MAX * \
	 FLASHLIGHT_SW_DISABLE_STATUS_TMPBUF_SIZE + 1)

/* sysfs - charger status */
#define FLASHLIGHT_CHARGER_NUM    4
#define FLASHLIGHT_CHARGER_TYPE   0
#define FLASHLIGHT_CHARGER_CT     1
#define FLASHLIGHT_CHARGER_PART   2
#define FLASHLIGHT_CHARGER_STATUS 3
#define FLASHLIGHT_CHARGER_STATUS_TMPBUF_SIZE 9
#define FLASHLIGHT_CHARGER_STATUS_BUF_SIZE \
	(FLASHLIGHT_TYPE_MAX * FLASHLIGHT_CT_MAX * FLASHLIGHT_PART_MAX * \
	 FLASHLIGHT_CHARGER_STATUS_TMPBUF_SIZE + 1)

/* sysfs - capability */
#define FLASHLIGHT_CAPABILITY_TMPBUF_SIZE 64
#define FLASHLIGHT_CAPABILITY_BUF_SIZE \
	(FLASHLIGHT_TYPE_MAX * FLASHLIGHT_CT_MAX * FLASHLIGHT_PART_MAX * \
	 FLASHLIGHT_CAPABILITY_TMPBUF_SIZE + 1)

/* sysfs - fault */
#define FLASHLIGHT_FAULT_TMPBUF_SIZE 64
#define FLASHLIGHT_FAULT_BUF_SIZE \
	(FLASHLIGHT_TYPE_MAX * FLASHLIGHT_CT_MAX * FLASHLIGHT_PART_MAX * \
	 FLASHLIGHT_FAULT_TMPBUF_SIZE + 1)

/* sysfs - current */
#define FLASHLIGHT_CURRENT_NUM    3
#define FLASHLIGHT_CURRENT_TYPE   0
#define FLASHLIGHT_CURRENT_CT     1
#define FLASHLIGHT_CURRENT_PART   2
#define FLASHLIGHT_DUTY_CURRENT_TMPBUF_SIZE 6
#define FLASHLIGHT_DUTY_CURRENT_BUF_SIZE \
	(FLASHLIGHT_MAX_DUTY_NUM * FLASHLIGHT_DUTY_CURRENT_TMPBUF_SIZE + 1)

/* sysfs - strobe */
#define FLASHLIGHT_ARG_NUM   5
#define FLASHLIGHT_ARG_TYPE  0
#define FLASHLIGHT_ARG_CT    1
#define FLASHLIGHT_ARG_PART  2
#define FLASHLIGHT_ARG_LEVEL 3
#define FLASHLIGHT_ARG_DUR   4
#define FLASHLIGHT_ARG_LEVEL_MAX 255
#define FLASHLIGHT_ARG_DUR_MAX   3000 /* ms */
struct flashlight_arg {
	int type;
	int ct;
	int part;
	int channel;
	int level;
	int dur;
	int decouple;
};

/* flashlight devices */
#define FLASHLIGHT_NAME_SIZE 32 /* flashlight device name */
struct flashlight_device_id {
	int type;
	int ct;
	int part;
	char name[FLASHLIGHT_NAME_SIZE]; /* device name */
	int channel;                     /* device channel */
	int decouple;                    /* device decouple */
};
extern const struct flashlight_device_id flashlight_id[];
extern const int flashlight_device_num;

struct flashlight_dev {
	struct list_head node;
	struct flashlight_operations *ops;
	struct flashlight_device_id dev_id;
	/* device status */
	int enable;
	int level;
	int low_pt_level;
	int charger_status;
	int sw_disable_status;
};

/* device arguments */
struct flashlight_dev_arg {
	int channel;
	int arg;
};

/* device operations */
struct flashlight_operations {
	int (*flashlight_open)(void);
	int (*flashlight_release)(void);
	int (*flashlight_ioctl)(unsigned int cmd, unsigned long arg);
	ssize_t (*flashlight_strobe_store)(struct flashlight_arg arg);
	int (*flashlight_set_driver)(int set);
};

/* device resiger */
int flashlight_dev_register(
		const char *name, struct flashlight_operations *dev_ops);
int flashlight_dev_unregister(const char *name);
int flashlight_dev_register_by_device_id(
		struct flashlight_device_id *dev_id,
		struct flashlight_operations *dev_ops);
int flashlight_dev_unregister_by_device_id(struct flashlight_device_id *dev_id);

/* get id and index */
int flashlight_get_type_id(int type_index);
int flashlight_get_ct_id(int ct_index);
int flashlight_get_part_id(int part_index);
int flashlight_get_type_index(int type_id);
int flashlight_get_ct_index(int ct_id);
int flashlight_get_part_index(int part_id);

/* verify id and index */
int flashlight_verify_type_index(int type_index);
int flashlight_verify_ct_index(int ct_index);
int flashlight_verify_part_index(int part_index);
int flashlight_verify_index(int type_index, int ct_index, int part_index);


#endif /* _FLASHLIGHT_CORE_H */

