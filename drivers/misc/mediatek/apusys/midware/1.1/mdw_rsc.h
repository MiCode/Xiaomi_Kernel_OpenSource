/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __APUSYS_MDW_RSC_H__
#define __APUSYS_MDW_RSC_H__

#include "mdw_queue.h"

#define MDW_DEV_NAME_SIZE 16

#define MDW_RSC_MAX_NUM 64 //max device type
#define MDW_RSC_TAB_DEV_MAX 16 //max device num per type

#define MDW_RSC_SET_PWR_TIMEOUT (3*1000)
#define MDW_RSC_SET_PWR_ALLON (0)

#define APUSYS_THD_TASK_FILE_PATH "/dev/stune/low_latency/tasks"

enum MDW_PREEMPT_PLCY {
	MDW_PREEMPT_PLCY_RR_SIMPLE,
	MDW_PREEMPT_PLCY_RR_PRIORITY,

	MDW_PREEMPT_PLCY_MAX,
};

enum MDW_DEV_INFO_GET_POLICY {
	MDW_DEV_INFO_GET_POLICY_SEQ,
	MDW_DEV_INFO_GET_POLICY_RR,

	MDW_DEV_INFO_GET_POLICY_MAX,
};

enum MDW_DEV_INFO_GET_MODE {
	MDW_DEV_INFO_GET_MODE_TRY,
	MDW_DEV_INFO_GET_MODE_SYNC,
	MDW_DEV_INFO_GET_MODE_ASYNC,

	MDW_DEV_INFO_GET_MODE_MAX,
};

enum MDW_DEV_INFO_STATE {
	MDW_DEV_INFO_STATE_IDLE,
	MDW_DEV_INFO_STATE_BUSY,
	MDW_DEV_INFO_STATE_LOCK,
};

struct mdw_dev_info {
	int idx;
	int type;
	int state;

	char name[32];

	struct apusys_device *dev;
	struct list_head t_item; //to rsc_tab
	struct list_head r_item; //to rsc_req
	struct list_head u_item; //to mdw_usr

	int (*exec)(struct mdw_dev_info *d, void *s);
	int (*pwr_on)(struct mdw_dev_info *d, int bst, int to);
	int (*pwr_off)(struct mdw_dev_info *d);
	int (*suspend)(struct mdw_dev_info *d);
	int (*resume)(struct mdw_dev_info *d);
	int (*fw)(struct mdw_dev_info *d, uint32_t magic, const char *name,
		uint64_t kva, uint32_t iova, uint32_t size, int op);
	int (*ucmd)(struct mdw_dev_info *d,
		uint64_t kva, uint32_t iova, uint32_t size);
	int (*sec_on)(struct mdw_dev_info *d);
	int (*sec_off)(struct mdw_dev_info *d);
	int (*lock)(struct mdw_dev_info *d);
	int (*unlock)(struct mdw_dev_info *d);

	void *sc;

	int stop;
	struct task_struct *thd;
	struct completion cmplt;
	struct completion thd_done;

	struct mutex mtx;
};

struct mdw_rsc_tab {
	int type;
	char name[MDW_DEV_NAME_SIZE];
	int dev_num;
	int avl_num;

	uint32_t norm_sc_cnt;

	struct mdw_dev_info *array[MDW_RSC_TAB_DEV_MAX]; //for mdw_dev_info
	struct list_head list; //for mdw_dev_info
	struct mutex mtx;
	struct mdw_queue q;

	struct dentry *dbg_dir;
};

struct mdw_rsc_req {
	uint8_t num[APUSYS_DEVICE_MAX]; //in
	uint8_t get_num[APUSYS_DEVICE_MAX]; //in
	uint32_t total_num; //in
	uint64_t acq_bmp; //in
	int mode; //in
	int policy; //in

	uint32_t ready_num;

	void (*cb_async)(struct mdw_rsc_req *r); //call if async
	bool in_list;
	struct kref ref;

	struct completion complt;
	struct list_head r_item; // to rsc mgr
	struct list_head d_list; // for rsc_dev
	struct mutex mtx;
};

extern struct dentry *mdw_dbg_device;

uint64_t mdw_rsc_get_avl_bmp(void);
void mdw_rsc_update_avl_bmp(int type);
struct mdw_queue *mdw_rsc_get_queue(int type);
int mdw_rsc_get_dev(struct mdw_rsc_req *req);
int mdw_rsc_put_dev(struct mdw_dev_info *d);
struct mdw_rsc_tab *mdw_rsc_get_tab(int type);
struct mdw_dev_info *mdw_rsc_get_dinfo(int type, int idx);

void mdw_rsc_dump(struct seq_file *s);
int mdw_rsc_set_preempt_plcy(uint32_t preempt_policy);
uint32_t mdw_rsc_get_preempt_plcy(void);

int mdw_rsc_init(void);
void mdw_rsc_exit(void);

uint64_t mdw_rsc_get_dev_bmp(void);
int mdw_rsc_get_dev_num(int type);

void mdw_rsc_set_thd_group(void);

#ifdef CONFIG_MTK_GZ_SUPPORT_SDSP
extern int mtee_sdsp_enable(u32 on);
#endif

#endif
