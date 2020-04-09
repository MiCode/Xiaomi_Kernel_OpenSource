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

#ifndef __VPU_CMD_H__
#define __VPU_CMD_H__

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include "vpu_algo.h"

struct vpu_device;

int vpu_cmd_init(struct platform_device *pdev, struct vpu_device *vd);
void vpu_cmd_exit(struct vpu_device *vd);
void vpu_cmd_clear(struct vpu_device *vd);

void vpu_cmd_lock(struct vpu_device *vd, int prio);
void vpu_cmd_unlock(struct vpu_device *vd, int prio);
void vpu_cmd_lock_all(struct vpu_device *vd);
void vpu_cmd_unlock_all(struct vpu_device *vd);

/* boost value handling */
int vpu_cmd_boost_set(struct vpu_device *vd, int prio, int boost);
int vpu_cmd_boost_put(struct vpu_device *vd, int prio);
int vpu_cmd_boost(struct vpu_device *vd, int prio);

/* command algorithm */
void vpu_cmd_alg_set(struct vpu_device *vd, int prio, struct __vpu_algo *alg);
struct __vpu_algo *vpu_cmd_alg(struct vpu_device *vd, int prio);
const char *vpu_cmd_alg_name(struct vpu_device *vd, int prio);
static inline void vpu_cmd_alg_clr(struct vpu_device *vd, int prio)
{
	vpu_cmd_alg_set(vd, prio, NULL);
}

/* command flow control */
void vpu_cmd_run(struct vpu_device *vd, int prio, uint32_t cmd);
void vpu_cmd_done(struct vpu_device *vd, int prio,
	uint32_t result, uint32_t alg_ret);
int vpu_cmd_result(struct vpu_device *vd, int prio);
uint32_t vpu_cmd_alg_ret(struct vpu_device *vd, int prio);
wait_queue_head_t *vpu_cmd_waitq(struct vpu_device *vd, int prio);
void vpu_cmd_wake_all(struct vpu_device *vd);

/* d2d command buffer */
uint32_t vpu_cmd_buf_iova(struct vpu_device *vd, int prio);
int vpu_cmd_buf_set(struct vpu_device *vd, int prio, void *buf, size_t size);

/* command control data */
struct vpu_cmd_ctl {
	uint32_t cmd;
	uint64_t start_t;    /* command start time */
	uint64_t end_t;      /* command end time */
	int boost;           /* boost value from vpu_reqest */
	struct mutex lock;
	wait_queue_head_t wait;
	struct __vpu_algo *alg;  /* current active algorithm */
	bool done;
	uint32_t result;     /* result from boot code */
	uint32_t alg_ret;    /* algorithm's return value */
	struct vpu_iova vi;  /* command buffer */
	uint64_t exe_cnt;    /* debug: execution count */
};

#endif

