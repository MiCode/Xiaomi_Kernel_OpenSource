/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __APUSYS_RESOURCE_MGT_H__
#define __APUSYS_RESOURCE_MGT_H__

#include "apusys_device.h"
#include "scheduler.h"

#define APUSYS_DEV_TABLE_MAX 16 //max number device supported

enum {
	APUSYS_DEV_OWNER_DISABLE,
	APUSYS_DEV_OWNER_NONE,
	APUSYS_DEV_OWNER_SCHEDULER,
};

struct prio_q_inst {
	struct list_head prio[APUSYS_PRIORITY_MAX];
	unsigned long node_exist[BITS_TO_LONGS(APUSYS_PRIORITY_MAX)];
};

struct apusys_dev_aquire {
	/* user set */
	int dev_type;
	int target_num;
	int owner;
	int is_sync;
	void *user;

	/* resource mgt use */
	int acq_num;
	int is_done;

	struct list_head dev_info_list;
	struct list_head tab_list;

	struct mutex mtx;
	struct completion comp;
};

/* device inst */
struct apusys_dev_info {
	struct apusys_device *dev;
	struct apusys_cmd *cmd;

	/* mgt info */
	uint64_t cur_owner; // record this

	/* acquire */
	struct list_head acq_list;
};

/* device table for one type, which record status, num, dev */
struct apusys_res_table {
	/* device related info */
	int dev_type; // APUSYS_DEVICE_E
	uint32_t dev_num; // num of total cores
	uint32_t available_num; // record how many cores available

	struct apusys_dev_info dev_list[APUSYS_DEV_TABLE_MAX];

	/* bitmap of device status, 1:occupied 0:idle */
	unsigned long dev_status[BITS_TO_LONGS(APUSYS_DEV_TABLE_MAX)];

	/* priority queue */
	struct prio_q_inst *prio_q;

	/* reservation */
	struct list_head acq_list;
};

/* init link list head, which link all dev table */
struct apusys_res_mgr {
	struct apusys_res_table *tab[APUSYS_DEVICE_MAX];
	unsigned long dev_support[BITS_TO_LONGS(APUSYS_DEVICE_MAX)];

	unsigned long cmd_exist[BITS_TO_LONGS(APUSYS_DEV_TABLE_MAX)];
	unsigned long dev_exist[BITS_TO_LONGS(APUSYS_DEV_TABLE_MAX)];

	unsigned long acquire_exist[BITS_TO_LONGS(APUSYS_DEV_TABLE_MAX)];
	struct completion sched_comp;
	uint8_t sched_stop;

	struct mutex mtx;
};

struct apusys_res_mgr *resource_get_mgr(void);

int insert_subcmd(void *isc, int priority);
int insert_subcmd_lock(void *isc, int priority);
int pop_subcmd(int type, void **isc);
int delete_subcmd(void *isc);
int delete_subcmd_lock(void *isc);

int get_apusys_device(int dev_type, uint64_t owner, struct apusys_device **dev);

int acquire_device_try(struct apusys_dev_aquire *acq);
int acquire_device_async(struct apusys_dev_aquire *acq);
int acquire_device_sync(struct apusys_dev_aquire *acq);
int acquire_device_check(struct apusys_dev_aquire **iacq);
int acquire_device_sync_lock(struct apusys_dev_aquire *acq);

int put_device_lock(struct apusys_device *dev);
int put_apusys_device(struct apusys_device *dev);

int resource_set_power(int dev_type, uint32_t idx, uint32_t boost_val);
int resource_load_fw(int dev_type, int idx, uint64_t kva,
	uint32_t iova, uint32_t size, int op);

int resource_get_device_num(int dev_type);
uint64_t resource_get_dev_support(void);

void resource_mgt_dump(void *s_file);
int resource_mgt_init(void);
int resource_mgt_destroy(void);

#endif
