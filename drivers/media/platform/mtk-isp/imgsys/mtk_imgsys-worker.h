/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Johnson-CH Chiu <johnson-ch.chiu@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_WORKER_H_
#define _MTK_IMGSYS_WORKER_H_
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/wait.h>


struct imgsys_queue {
	char *name;
	struct device *dev;
	struct list_head queue;
	atomic_t nr;
	int peak;
	atomic_t disable;
	spinlock_t lock;
	wait_queue_head_t wq;
	wait_queue_head_t dis_wq;
	struct task_struct *task;
	struct mutex task_lock;
};

struct imgsys_work {
	struct list_head entry;
	void (*run)(void *data);
};

int imgsys_queue_init(struct imgsys_queue *que, struct device *dev, char *name);
int imgsys_queue_enable(struct imgsys_queue *que);
int imgsys_queue_disable(struct imgsys_queue *que);
int imgsys_queue_add(struct imgsys_queue *que, struct imgsys_work *work);
int imgsys_queue_timeout(struct imgsys_queue *que);

#endif
