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

#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/sched/clock.h>
#include "vpu_power.h"
#include "vpu_cfg.h"
#include "vpu_cmd.h"
#include "vpu_reg.h"
#include "vpu_algo.h"

static unsigned int vpu_prio(int p)
{
	if (p < 0)
		return 0;
	if (p >= VPU_MAX_PRIORITY)
		return (VPU_MAX_PRIORITY - 1);
	return (unsigned int)p;
}

/**
 * vpu_cmd_init - Initialize command control
 * @vd: the pointer of vpu_device.
 */
int vpu_cmd_init(struct platform_device *pdev, struct vpu_device *vd)
{
	int i;
	int ret = 0;
	struct vpu_cmd_ctl *c;
	dma_addr_t iova;

	vd->cmd_timeout = VPU_CMD_TIMEOUT;
	atomic_set(&vd->cmd_prio, 0);
	atomic_set(&vd->cmd_active, 0);
	vd->cmd_prio_max = VPU_MAX_PRIORITY;

	for (i = 0; i < vd->cmd_prio_max; i++) {
		c = &vd->cmd[i];
		mutex_init(&c->lock);
		init_waitqueue_head(&c->wait);
		c->done = false;
		c->boost = VPU_PWR_NO_BOOST;
		c->vi.bin = VPU_MEM_ALLOC;
		c->vi.size = VPU_CMD_SIZE;
		c->exe_cnt = 0;
		iova = vpu_iova_alloc(pdev, &c->vi);
		if (!iova) {
			vd->cmd_prio_max = i;
			vpu_cmd_exit(vd);
			ret = -ENOMEM;
			break;
		}
	}
	return ret;
}

/**
 * vpu_cmd_exit - free command control
 * @vd: the pointer of vpu_device.
 */
void vpu_cmd_exit(struct vpu_device *vd)
{
	int i;

	for (i = 0; i < vd->cmd_prio_max; i++)
		vpu_iova_free(vd->dev, &vd->cmd[i].vi);
}

/**
 * vpu_cmd_clear - clear command control when power down
 * @vd: vpu_device
 */
void vpu_cmd_clear(struct vpu_device *vd)
{
	int i;
	struct vpu_cmd_ctl *c;
	struct vpu_algo_list *al;

	for (i = 0; i < vd->cmd_prio_max; i++) {
		c = &vd->cmd[i];
		c->cmd = 0;
		c->done = false;
		c->alg_ret = 0;
		c->result = 0;
		c->start_t = 0;
		c->end_t = 0;
		c->boost = VPU_PWR_NO_BOOST;
		if (c->alg && c->alg->al) {
			al = c->alg->al;
			if (al->ops->unload)
				al->ops->unload(al, i);
		}
		atomic_set(&vd->cmd_active, 0);
	}
}

/**
 * vpu_cmd_lock - Lock command execution for given priority
 * @vd: the pointer of vpu_device.
 * @prio: priority
 */
void vpu_cmd_lock(struct vpu_device *vd, int prio)
{
	unsigned int p = vpu_prio(prio);
	struct vpu_cmd_ctl *c = &vd->cmd[p];

	mutex_lock_nested(&c->lock, VPU_MUTEX_CMD + p);
	c->start_t = sched_clock();
	c->end_t = 0;
}

/**
 * vpu_cmd_unlock - Unlock command execution for given priority
 * @vd: the pointer of vpu_device.
 * @prio: priority
 */
void vpu_cmd_unlock(struct vpu_device *vd, int prio)
{
	struct vpu_cmd_ctl *c = &vd->cmd[vpu_prio(prio)];

	c->end_t = sched_clock();
	mutex_unlock(&c->lock);
}

/**
 * vpu_cmd_lock_all - Lock command execution for all priorities,
 *                    used for suspend, power control.
 * @vd: the pointer of vpu_device.
 */
void vpu_cmd_lock_all(struct vpu_device *vd)
{
	int i;

	for (i = 0; i < vd->cmd_prio_max; i++)
		mutex_lock_nested(&vd->cmd[i].lock, VPU_MUTEX_CMD + i);

	mutex_lock_nested(&vd->lock, VPU_MUTEX_DEV);
}

/**
 * vpu_cmd_lock_all - Unlock command execution for all priorities
 * @vd: the pointer of vpu_device.
 */
void vpu_cmd_unlock_all(struct vpu_device *vd)
{
	int i;

	mutex_unlock(&vd->lock);

	for (i = (vd->cmd_prio_max - 1); i >= 0; i--)
		mutex_unlock(&vd->cmd[i].lock);
}

/**
 * vpu_cmd_run - initialize command of given priority before run
 * @vd: the pointer of vpu_device.
 * @prio: priority
 * @cmd: command at INFO01
 */
void vpu_cmd_run(struct vpu_device *vd, int prio, uint32_t cmd)
{
	struct vpu_cmd_ctl *c = &vd->cmd[vpu_prio(prio)];

	c->cmd = cmd;
	c->done = false;
	c->alg_ret = 0;
	c->result = 0;
	c->exe_cnt++;
	atomic_inc(&vd->cmd_active);
}

/**
 * vpu_cmd_done - finalize command of given priority when done
 * @vd: the pointer of vpu_device.
 * @prio: priority
 * @result: execution result from INFO00
 * @alg_ret: algorithm return value from INFO02
 */
void vpu_cmd_done(struct vpu_device *vd, int prio,
	uint32_t result, uint32_t alg_ret)
{
	struct vpu_cmd_ctl *c = &vd->cmd[vpu_prio(prio)];

	c->done = true;
	c->alg_ret = alg_ret;
	c->result = result;
	atomic_dec(&vd->cmd_active);
}

/**
 * vpu_cmd_alg_set - Set algorithm to the command control of given priority
 * @vd: the pointer of vpu_device.
 * @prio: priority
 */
void vpu_cmd_alg_set(struct vpu_device *vd, int prio, struct __vpu_algo *alg)
{
	vd->cmd[vpu_prio(prio)].alg = alg;
}

/**
 * vpu_cmd_alg - Return the algorithm from the command control
 *               of given priority
 * @vd: the pointer of vpu_device.
 * @prio: priority
 */
struct __vpu_algo *vpu_cmd_alg(struct vpu_device *vd, int prio)
{
	return vd->cmd[vpu_prio(prio)].alg;
}

/**
 * vpu_cmd_waitq - Return the wait queue to the command control
 *               of given priority
 * @vd: the pointer of vpu_device.
 * @prio: priority
 */
wait_queue_head_t *vpu_cmd_waitq(struct vpu_device *vd, int prio)
{
	return &vd->cmd[vpu_prio(prio)].wait;
}

/**
 * vpu_cmd_wake_all - Unconditionally wakeup all wait queues
 *
 * @vd: the pointer of vpu_device.
 * Note: this function should be used by vpu_aee_exp() only
 */
void vpu_cmd_wake_all(struct vpu_device *vd)
{
#if VPU_XOS
	int i;

	for (i = 0; i < vd->cmd_prio_max; i++) {
		vd->cmd[i].done = true;
		vd->cmd[i].result = VPU_STATE_ABORT;
		wake_up_interruptible(vpu_cmd_waitq(vd, i));
	}
#endif
}

/**
 * vpu_cmd_alg - Return the algorithm's name from the command control
 *               of given priority
 * @vd: the pointer of vpu_device.
 * @prio: priority
 */
const char *vpu_cmd_alg_name(struct vpu_device *vd, int prio)
{
	struct __vpu_algo *alg;

	alg = vpu_cmd_alg(vd, prio);

	return (alg) ? alg->a.name : "";
}

/**
 * vpu_cmd_result - Get translated VPU execution results of bootcode (INFO00)
 *                  from the command control of given priority
 * @vd: the pointer of vpu_device.
 * @prio: priority
 */
int vpu_cmd_result(struct vpu_device *vd, int prio)
{
	switch (vd->cmd[vpu_prio(prio)].result) {
	case VPU_STATE_READY:
	case VPU_STATE_IDLE:
		return 0;
	case VPU_STATE_NOT_READY:
	case VPU_STATE_ERROR:
		return -EIO;
	case VPU_STATE_BUSY:
		return -EBUSY;
	case VPU_STATE_TERMINATED:
	case VPU_STATE_ABORT:
		return -EBADFD;
	default:
		return -EINVAL;
	}
	return 0;
}

/**
 * vpu_cmd_alg_ret - Get Algorithm's return value (INFO02)
 *                   from the command control of given priority
 * @vd: the pointer of vpu_device.
 * @prio: priority
 */
uint32_t vpu_cmd_alg_ret(struct vpu_device *vd, int prio)
{
	return vd->cmd[vpu_prio(prio)].alg_ret;
}

/**
 * vpu_cmd_va - Get the virtual address of the D2D command buffer
 *              of given priority
 * @vd: the pointer of vpu_device.
 * @prio: priority
 */
static void *vpu_cmd_buf_va(struct vpu_device *vd, int prio)
{
	return (void *)vd->cmd[vpu_prio(prio)].vi.m.va;
}

/**
 * vpu_cmd_iova - Get the iova of the D2D command buffer
 *                of given priority
 * @vd: the pointer of vpu_device.
 * @prio: priority
 */
uint32_t vpu_cmd_buf_iova(struct vpu_device *vd, int prio)
{
	return vd->cmd[vpu_prio(prio)].vi.m.pa;
}

/**
 * vpu_cmd_sync - Sync D2D command buffer of given priority
 *                for execution
 * @vd: the pointer of vpu_device.
 * @prio: priority
 */
static void vpu_cmd_buf_sync(struct vpu_device *vd, int prio)
{
	vpu_iova_sync_for_device(vd->dev, &vd->cmd[vpu_prio(prio)].vi);
}

/**
 * vpu_cmd_buf_set - Set D2D command buffer of given priority
 * @vd: the pointer of vpu_device.
 * @prio: priority
 * @buf: D2D command buffer
 * @size: D2D command buffer size
 */
int vpu_cmd_buf_set(struct vpu_device *vd, int prio, void *buf, size_t size)
{
	if (size > VPU_CMD_SIZE)
		return -EINVAL;

	memcpy(vpu_cmd_buf_va(vd, prio), buf, size);
	vpu_cmd_buf_sync(vd, prio);
	return 0;
}

/**
 * vpu_cmd_boost_elevate - get the maximum boot value across all priorites
 * @vd: vpu_device
 */
static int vpu_cmd_boost_elevate(struct vpu_device *vd)
{
#if VPU_XOS
	int i;
	int boost = -1;

	for (i = 0; i < vd->cmd_prio_max; i++) {
		if (vd->cmd[i].boost == VPU_PWR_NO_BOOST)
			continue;
		boost = max_t(int, vd->cmd[i].boost, boost);
	}

	return (boost >= 0) ? boost : VPU_PWR_NO_BOOST;
#else
	return vd->cmd[0].boost;
#endif
}

/**
 * vpu_cmd_boost_set - set boost value to the command of given priority
 * @vd: vpu_device
 * @prio: priority
 * @boost: boost value from vpu_request
 *
 * Returns the elevated boost value across all priorities
 */
int vpu_cmd_boost_set(struct vpu_device *vd, int prio, int boost)
{
	vd->cmd[vpu_prio(prio)].boost = boost;
	return vpu_cmd_boost_elevate(vd);
}

/**
 * vpu_cmd_boost_set - unset boost value to the command of given priority
 * @vd: vpu_device
 * @prio: priority
 *
 * Returns the elevated boost value across all priorities
 */
int vpu_cmd_boost_put(struct vpu_device *vd, int prio)
{
	return vpu_cmd_boost_set(vd, prio, VPU_PWR_NO_BOOST);
}

/**
 * vpu_cmd_boost_set - get the boost value of given priority
 * @vd: vpu_device
 * @prio: priority
 */
int vpu_cmd_boost(struct vpu_device *vd, int prio)
{
	return vd->cmd[vpu_prio(prio)].boost;
}



