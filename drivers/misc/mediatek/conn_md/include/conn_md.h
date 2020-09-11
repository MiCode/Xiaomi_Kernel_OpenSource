/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __CONN_MD_H_
#define __CONN_MD_H_

#include "conn_md_exp.h"
#include "conn_md_dump.h"

/*-----------------------Data Structure Definition-----------------------*/

enum user_state {
	USER_MIN,
	USER_REGED,
	USER_ENABLED,
	USER_DISABLED,
	USER_UNREGED,
	USER_MAX,
};

struct conn_md_user {
	uint32 u_id;
	enum user_state state;
	struct conn_md_bridge_ops ops;
	struct list_head entry;
};

struct conn_md_msg {
	struct ipc_ilm ilm;
	struct list_head entry;
	struct local_para local_para;
};

struct conn_md_queue {
	struct list_head list;
	struct mutex lock;
	uint32 counter;
};

struct conn_md_user_list {
	uint32 counter;
	struct list_head list;
	struct mutex lock;	/*lock for user add/delete/check */
};

struct conn_md_struct {
	/*con-md-thread used for tx queue handle */
	struct task_struct *p_task;
	struct completion tx_comp;

	struct conn_md_user_list user_list;
	struct conn_md_queue act_queue;
	struct conn_md_queue msg_queue;
	struct conn_md_dmp_msg_log *p_msg_dmp_sys;
};

struct conn_md_time_struct {
	unsigned long long sec;
	unsigned long msec;
};

#define CONN_MD_MSG_MAX_NUM 5
#define CONN_MD_MSG_TIME_LENGTH 16
#define CONN_MD_BUF_SIZE (CONN_MD_MSG_MAX_NUM * CONN_MD_MSG_TIME_LENGTH)
struct conn_md_log_msg_info {
	struct conn_md_time_struct msg_begin_time;
	int msg_total;
	char msg_buf[CONN_MD_BUF_SIZE];
};

extern int conn_md_send_msg(struct ipc_ilm *ilm);
extern int conn_md_del_user(uint32 u_id);
extern int conn_md_add_user(uint32 u_id, struct conn_md_bridge_ops *p_ops);
extern int conn_md_dmp_msg_logged(uint32 src_id, uint32 dst_id);
extern int conn_md_dmp_msg_active(uint32 src_id, uint32 dst_id);
extern int conn_md_dmp_msg_queued(uint32 src_id, uint32 dst_id);

#endif
