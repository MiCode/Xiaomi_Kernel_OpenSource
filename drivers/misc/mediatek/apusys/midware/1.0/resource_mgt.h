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
#include "sched_normal.h"
#include "cmd_parser.h"

#define APUSYS_SETPOWER_TIMEOUT (3*1000)
#define APUSYS_SETPOWER_TIMEOUT_ALLON (0)

#define APUSYS_DEV_TABLE_MAX 16 //max number device supported
#define APUSYS_DEV_NAME_SIZE 16

enum {
	APUSYS_DEV_OWNER_DISABLE,
	APUSYS_DEV_OWNER_NONE,
	APUSYS_DEV_OWNER_SCHEDULER,
	APUSYS_DEV_OWNER_SECURE,
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
	uint64_t acq_bitmap;
	int is_done;

	struct list_head dev_info_list;
	struct list_head tab_list;

	struct mutex mtx;
	struct completion comp;
};

/* device inst */
struct apusys_dev_info {
	struct apusys_device *dev;
	char name[APUSYS_DEV_NAME_SIZE];

	/* mgt info */
	uint64_t cmd_id;
	int sc_idx;
	bool is_deadline;
	uint64_t cur_owner; // record this

	/* acquire */
	struct list_head acq_list;
};

/* device table for one type, which record status, num, dev */
struct apusys_res_table {
	/* device related info */
	int dev_type; // APUSYS_DEVICE_E
	char name[APUSYS_DEV_NAME_SIZE];
	uint32_t dev_num; // num of total cores
	uint32_t available_num; // record how many cores available

	struct apusys_dev_info dev_list[APUSYS_DEV_TABLE_MAX];

	/* bitmap of device status, 1:occupied 0:idle */
	unsigned long dev_status[BITS_TO_LONGS(APUSYS_DEV_TABLE_MAX)];

	/* priority queue */
	struct normal_queue normal_q;

	/* deadline queue */
	struct deadline_root deadline_q;

	uint32_t normal_task_num;
	uint32_t deadline_task_num;
	struct mutex mtx;
	uint8_t boost_mode; /* Need boost clk if load is over threshold */

	/* reservation */
	struct list_head acq_list;

	/* dbg */
	struct dentry *dbg_dir;
	int last_idx;
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
struct apusys_res_table *res_get_table(int type);

int insert_subcmd(struct apusys_subcmd *sc);
int insert_subcmd_lock(struct apusys_subcmd *sc);
int pop_subcmd(int type, struct apusys_subcmd **isc);
int delete_subcmd(struct apusys_subcmd *sc);
int delete_subcmd_lock(struct apusys_subcmd *sc);

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

int res_task_inc(struct apusys_subcmd *sc);
int res_task_dec(struct apusys_subcmd *sc);

int res_secure_on(int dev_type);
int res_secure_off(int dev_type);
int res_suspend_dev(void);
int res_resume_dev(void);

void res_mgt_dump(void *s_file);
int res_mgt_init(void);
int res_mgt_destroy(void);

struct apusys_res_table *res_get_table(int type);

#ifdef CONFIG_MTK_GZ_SUPPORT_SDSP
extern int mtee_sdsp_enable(u32 on);
#endif

#endif
