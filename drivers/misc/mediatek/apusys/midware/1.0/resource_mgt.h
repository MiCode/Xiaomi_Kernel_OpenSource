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
#include "sched_deadline.h"

#define APUSYS_SETPOWER_TIMEOUT (3*1000)
#define APUSYS_SETPOWER_TIMEOUT_ALLON (0)

#define APUSYS_DEV_TABLE_MAX 16 //max number device supported

enum {
	APUSYS_DEV_OWNER_DISABLE,
	APUSYS_DEV_OWNER_NONE,
	APUSYS_DEV_OWNER_SCHEDULER,
};

struct prio_q_inst {
	struct list_head prio[APUSYS_PRIORITY_MAX];
	unsigned long node_exist[BITS_TO_LONGS(APUSYS_PRIORITY_MAX)];

	/* basic info */
	int normal_len;
	int deadline_len;
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

	/* mgt info */
	uint64_t cmd_id;
	int sc_idx;
	int is_deadline;
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

	/* deadline queue */
	struct deadline_root deadline_q;

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
	uint8_t sched_pause;

	struct mutex mtx;
};

struct apusys_res_mgr *res_get_mgr(void);

int insert_subcmd(void *isc);
int insert_subcmd_lock(void *isc);
int pop_subcmd(int type, void **isc);
int delete_subcmd(void *isc);
int delete_subcmd_lock(void *isc);

int acq_device_try(struct apusys_dev_aquire *acq);
int acq_device_async(struct apusys_dev_aquire *acq);
int acq_device_sync(struct apusys_dev_aquire *acq);
int acq_device_check(struct apusys_dev_aquire **iacq);

int put_device_lock(struct apusys_dev_info *dev_info);
int put_apusys_device(struct apusys_dev_info *dev_info);

int res_power_on(int dev_type, uint32_t idx,
	uint32_t boost_val, uint32_t timeout);
int res_power_off(int dev_type, uint32_t idx);
int res_load_firmware(int dev_type, uint32_t magic, const char *name,
	int idx, uint64_t kva, uint32_t iova, uint32_t size, int op);
int res_send_ucmd(int dev_type, int idx,
	uint64_t kva, uint32_t iova, uint32_t size);

int res_get_device_num(int dev_type);
uint64_t res_get_dev_support(void);
int res_get_queue_len(int dev_type);

int res_secure_on(int dev_type);
int res_secure_off(int dev_type);

void res_mgt_dump(void *s_file);
int res_mgt_init(void);
int res_mgt_destroy(void);

#ifdef CONFIG_MTK_GZ_SUPPORT_SDSP
extern mtee_sdsp_enable(u32 on);
#endif

#endif
