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

#ifndef __CCCI_CHRDEV_H__
#define __CCCI_CHRDEV_H__
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/cdev.h>
#include "ccci_layer.h"

#define MAX_BUFFER_MESSAGES  ((1 << 20)/sizeof(struct CCCI_BUFF_T))
#define DEFAULT_BUFFER (CCCI_FIFO_MAX_LEN*2)

#define recv_array_is_full(array)    ({\
	unsigned int __nr = (array)->nr;    \
	unsigned int __idx = (array)->in_idx; \
	WARN_ON(__idx > __nr); \
	__nr == __idx;     })

#define recv_array_is_empty(array)    ({\
	unsigned int __nr = (array)->in_idx;    \
	unsigned int __idx = (array)->out_idx; \
	WARN_ON(__idx > __nr); \
	__nr == __idx; })

#define reset_recv_array(array)  ({ \
	(array)->in_idx = 0;    \
	(array)->out_idx = 0; })

#define get_first_entry(list, type, mem)    ({ \
	type *__ret;\
	if (list_empty(list))\
		__ret = NULL;\
	else\
		__ret = list_entry((list)->next, type, mem);\
	__ret; })

#define get_last_entry(list, type, mem) ({ \
	type *__ret;\
	if (list_empty(list))\
		__ret = NULL;\
	else\
		__ret = list_entry((list)->prev, type, mem);\
	__ret; })

#define ccci_get_first(_list)  get_first_entry(_list, struct recv_array, list)
#define ccci_get_last(_list)   get_last_entry(_list, struct recv_array, list)
#define ccci_is_last_recv(list) list_is_last((list)->next, list)

struct recv_array {
	struct CCCI_BUFF_T *array;
	unsigned int nr;
	unsigned int in_idx;
	unsigned int out_idx;
	struct list_head list;
};

struct ccci_dev_client {
	spinlock_t lock;
	atomic_t user;
	pid_t pid;
	int ch_num;
	struct list_head dev_list;
	wait_queue_head_t wait_q;
	int md_id;
	void *ctlb;
	struct fasync_struct *fasync;
	unsigned int wakeup_waitq;
};

struct chr_ctl_block_t {
	int md_id;
	struct mutex chr_dev_mutex;
	struct list_head chr_dev_list;
	struct cdev ccci_chrdev;
	int major;
	int minor;
};

struct ccci_vir_client_t {
	spinlock_t lock;
	atomic_t user;
	pid_t pid;
	int index;
	struct list_head dev_list;
	wait_queue_head_t wait_q;
	int md_id;
	void *ctlb;
	struct fasync_struct *fasync;
	unsigned int wakeup_waitq;
	struct kfifo private_fifo;
	int fifo_ready;
};

struct vir_ctl_block_t {
	int md_id;
	struct mutex chr_dev_mutex;
	struct list_head chr_dev_list;
	struct cdev ccci_chrdev;
	int major;
	int minor;
	spinlock_t bind_lock;
	struct ccci_vir_client_t *system_msg_client;
};

extern int ccci_chrdev_init(int);
extern void ccci_chrdev_exit(int);
#endif	/* __CCCI_CHRDEV_H__ */
