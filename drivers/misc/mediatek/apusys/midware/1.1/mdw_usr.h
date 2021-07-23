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

#ifndef __APUSYS_MDW_USR_H__
#define __APUSYS_MDW_USR_H__

#include "apusys_drv.h"
#include "apusys_device.h"
#include "mdw_cmd.h"

struct mdw_usr_mgr {
	struct list_head list;
	struct mutex mtx;
};

struct mdw_usr {
	uint64_t id;
	pid_t pid;
	pid_t tgid;
	char comm[TASK_COMM_LEN];
	uint32_t iova_size_max;
	uint32_t iova_size;

	struct list_head m_item; // to usr mgr

	struct mutex mtx;

	struct list_head cmd_list; // for cmd
	struct list_head mem_list; // for mem
	struct list_head sdev_list; // for sec dev
};

void mdw_usr_dump(struct seq_file *s);
int mdw_usr_mem_alloc(struct apusys_mem *um, struct mdw_usr *u);
int mdw_usr_mem_free(struct apusys_mem *um, struct mdw_usr *u);
int mdw_usr_mem_import(struct apusys_mem *um, struct mdw_usr *u);
int mdw_usr_mem_map(struct apusys_mem *um, struct mdw_usr *u);
int mdw_usr_dev_sec_alloc(int type, struct mdw_usr *u);
int mdw_usr_dev_sec_free(int type, struct mdw_usr *u);
int mdw_usr_fw(struct apusys_ioctl_fw *f, int op);
int mdw_usr_ucmd(struct apusys_ioctl_ucmd *uc);
int mdw_usr_set_pwr(struct apusys_ioctl_power *pwr);

struct mdw_usr *mdw_usr_create(void);
void mdw_usr_destroy(struct mdw_usr *u);

int mdw_usr_run_cmd_async(struct mdw_usr *u, struct apusys_ioctl_cmd *in);
int mdw_usr_wait_cmd(struct mdw_usr *u, struct apusys_ioctl_cmd *in);
int mdw_usr_run_cmd_sync(struct mdw_usr *u, struct apusys_ioctl_cmd *in);

int mdw_usr_init(void);
void mdw_usr_exit(void);


void mdw_usr_print_mem_usage(void);
void mdw_usr_aee_mem(void *s_file);
#endif
