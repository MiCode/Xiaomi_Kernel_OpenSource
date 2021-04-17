/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include "vpu_cfg.h"
#include "vpu_hw.h"

#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/bitops.h>
#include <linux/wait.h>
#include "mtk_devinfo.h"
#include "vpu_algo.h"
#include "vpu_cmd.h"
#include "vpu_mem.h"
#include "vpu_power.h"
#include "vpu_debug.h"
#include "vpu_cmn.h"
#include "vpu_dump.h"
#include "vpu_trace.h"
#include "vpu_met.h"
#include <memory/mediatek/emi.h>
#include "vpu_tag.h"
#define CREATE_TRACE_POINTS
#include "vpu_events.h"

#define VPU_TS_INIT(a) \
	struct timespec a##_s, a##_e

#define VPU_TS_START(a) \
	ktime_get_ts(&a##_s)

#define VPU_TS_END(a) \
	ktime_get_ts(&a##_e)

#define VPU_TS_NS(a) \
	((uint64_t)(timespec_to_ns(&a##_e) - timespec_to_ns(&a##_s)))

#define VPU_TS_US(a) \
	(VPU_TS_NS(a) / 1000)

static int wait_idle(struct vpu_device *vd, uint32_t latency, uint32_t retry);
static int vpu_set_ftrace(struct vpu_device *vd);
static void vpu_run(struct vpu_device *vd, int prio, uint32_t cmd)
{
	vpu_reg_write(vd, XTENSA_INFO01, cmd);
	vpu_cmd_run(vd, prio, cmd);
	vpu_reg_clr(vd, CTRL, (1 << 23));
}

static inline void vpu_stall(struct vpu_device *vd)
{
#if (VPU_XOS)
	/* XOS doesn't need stall */
#else
	vpu_reg_set(vd, CTRL, (1 << 23));
#endif
}

static void vpu_cmd(struct vpu_device *vd, int prio, uint32_t cmd)
{
	vpu_cmd_debug("%s: vpu%d: cmd: 0x%x (%s)\n",
		__func__, vd->id, cmd, vpu_debug_cmd_str(cmd));
	vpu_run(vd, prio, cmd);
	wmb();  /* make sure register committed */
	vpu_reg_set(vd, CTL_XTENSA_INT, 1);
}

#define XOS_UNLOCKED 0
#define XOS_LOCKED   1

#if VPU_XOS
/**
 * vpu_xos_lock() - lock vpu for control
 *
 * @vd: vpu device
 *
 * Returns:
 *   0: lock succeed
 *   -ETIME: lock timeout
 *   -EAGAIN: crashed by other priority
 */
static int vpu_xos_lock(struct vpu_device *vd)
{
	int s = XOS_LOCKED;
	uint64_t t;
	int ret = 0;

	VPU_TS_INIT(xos);
	VPU_TS_START(xos);

	while (1) {
		s = atomic_cmpxchg(&vd->xos_state,
			XOS_UNLOCKED, XOS_LOCKED);

		if (s == XOS_UNLOCKED)
			break;

		VPU_TS_END(xos);
		t = VPU_TS_US(xos);

		if (t >= XOS_TIMEOUT_US) {
			pr_info("%s: vpu%d: xos lock timeout: %lld us\n",
				__func__, vd->id, t);
			break;
		}
	}

	/* Other priorites had timeout, shutdown by vpu_aee_excp */
	if (s == XOS_UNLOCKED) {
		if ((vd->state <= VS_DOWN) ||
			(vd->state >= VS_REMOVING)) {
			ret = -EAGAIN;
			atomic_set(&vd->xos_state, XOS_UNLOCKED);
		}
	}
	/* XOS lock timeout  */
	else if (s == XOS_LOCKED)
		ret = -ETIME;

	VPU_TS_END(xos);
	vpu_cmd_debug("%s: vpu%d: %d, %lld us\n",
		__func__, vd->id, ret, VPU_TS_US(xos));
	return ret;
}

void vpu_xos_unlock(struct vpu_device *vd)
{
	vpu_cmd_debug("%s: vpu%d\n", __func__, vd->id);
	atomic_set(&vd->xos_state, XOS_UNLOCKED);
}

int vpu_xos_wait_idle(struct vpu_device *vd)
{
	return wait_idle(vd, WAIT_XOS_LATENCY_US, WAIT_XOS_RETRY);
}
#else
static int vpu_xos_lock(struct vpu_device *vd)
{
	return 0;
}

void vpu_xos_unlock(struct vpu_device *vd)
{
}

int vpu_xos_wait_idle(struct vpu_device *vd)
{
	return 0;
}
#endif

/**
 * vpu_reg_lock() - lock vpu register for control
 *
 * @vd: vpu device
 * @boot: called at boot sequence, Eq. vd->lock is acquired
 * @flags: saved irq flags
 *
 * Returns:
 *   0: lock succeed
 *   -ETIME: lock timeout
 *   -EAGAIN: crashed by other priority
 */
static inline
int vpu_reg_lock(struct vpu_device *vd, bool boot, unsigned long *flags)
{
	int ret;

	ret = vpu_xos_lock(vd);

	if (ret == 0) {
		spin_lock_irqsave(&vd->reg_lock, (*flags));
	} else if (ret == -ETIME) {
		if (boot) {
			vpu_aee_excp_locked(vd, NULL,
				"VPU Timeout",
				"vpu%d: XOS lock timeout (boot)\n",
				vd->id);
		} else {
			vpu_aee_excp(vd, NULL,
				"VPU Timeout",
				"vpu%d: XOS lock timeout\n",
				vd->id);
		}
	} else if (ret == -EAGAIN) {
		pr_info("%s: vpu%d: failed caused by other thread\n",
			__func__, vd->id);
	}

	return ret;
}

static inline void vpu_reg_unlock(struct vpu_device *vd, unsigned long *flags)
{
	spin_unlock_irqrestore(&vd->reg_lock, (*flags));
}

static int wait_idle(struct vpu_device *vd, uint32_t latency, uint32_t retry)
{
	uint32_t done_st = 0;
	uint32_t pwait = 0;
	unsigned int count = 0;

	do {
		done_st = vpu_reg_read(vd, DONE_ST);
		count++;
		pwait = !!(done_st & PWAITMODE);
		if (pwait)
			return 0;
		udelay(latency);
		trace_vpu_wait(vd->id, done_st,
			vpu_reg_read(vd, XTENSA_INFO00),
			vpu_reg_read(vd, XTENSA_INFO25),
			vpu_reg_read(vd, DEBUG_INFO05));
	} while (count < retry);

	pr_info("%s: vpu%d: %d us: done_st: 0x%x, pwaitmode: %d, info00: 0x%x, info25: 0x%x\n",
		__func__, vd->id, (latency * retry), done_st, pwait,
		vpu_reg_read(vd, XTENSA_INFO00),
		vpu_reg_read(vd, XTENSA_INFO25));

	return -EBUSY;
}

static int wait_command(struct vpu_device *vd, int prio_s)
{
	int ret = 0;
	unsigned int prio = prio_s;
	bool retry = true;

start:
	ret = wait_event_interruptible_timeout(
		vd->cmd[prio].wait,
		vd->cmd[prio].done,
		msecs_to_jiffies(vd->cmd_timeout));

	if (ret == -ERESTARTSYS) {
		pr_info("%s: vpu%d: interrupt by signal: ret=%d\n",
			__func__, vd->id, ret);

		if (retry) {
			pr_info("%s: vpu%d: try wait again\n",
				__func__, vd->id);
			retry = false;
			goto start;
		}
		goto out;
	}

	/* Other priorites had timeout, shutdown by vpu_aee_excp */
	if (vd->state <= VS_DOWN) {
		ret = -EAGAIN;
		goto out;
	}

	/* command success */
	if (ret) {
		ret = 0;
		goto out;
	}

	/* timeout handling */
	ret = -ETIMEDOUT;

	/* debug: check PWAITMODE */
	wait_idle(vd, WAIT_CMD_LATENCY_US, WAIT_CMD_RETRY);

out:
	return ret;
}

static void vpu_exit_dev_algo_general(struct platform_device *pdev,
	struct vpu_device *vd, struct vpu_algo_list *al)
{
	struct __vpu_algo *alg, *tmp;

	vpu_alg_debug("%s: vd: %p, vpu%d, al->a: %p\n",
		__func__, vd, vd->id, &al->a);

	list_for_each_entry_safe(alg, tmp, &al->a, list) {
		vpu_alg_debug("%s: vd: %p, vpu%d, vd->al.a: %p, alg: %p\n",
			__func__, vd, vd->id, &al->a, alg);
		al->ops->put(alg);
	}
}

#if VPU_IMG_LEGACY
static struct vpu_image_header *bin_header(int i)
{
	struct vpu_image_header *h;

	h = (struct vpu_image_header *)(((unsigned long)vpu_drv->bin_va) +
		VPU_OFFSET_IMAGE_HEADERS);

	return &h[i];
}

static struct vpu_algo_info *bin_algo_info(struct vpu_image_header *h, int j)
{
	return &h->algo_infos[j];
}
#endif

#if VPU_IMG_PRELOAD
static void *bin_header(int index)
{
	int i;
	uint64_t ptr = (unsigned long)vpu_drv->bin_va;
	struct vpu_image_header *header;

	ptr += vpu_drv->bin_head_ofs;
	header = (void *)ptr;

	for (i = 0; i < index; i++) {
		ptr += header->header_size;
		header = (void *)ptr;
	}

	return (void *)header;
}

static struct vpu_algo_info *bin_algo_info(struct vpu_image_header *h, int j)
{
	struct vpu_algo_info *algo_info;

	algo_info = (void *)((unsigned long)h + h->alg_info);
	return &algo_info[j];
}

static int preload_iova_check(struct vpu_iova *i)
{
	unsigned int bin_end = vpu_drv->bin_pa + vpu_drv->bin_size;

#define is_align_4k(x)   ((x & 0xFFF))
#define is_align_64k(x)  ((x & 0xFFFF))
	if (is_align_64k(i->size) && i->addr) {
		pr_info("%s: size(0x%x) not 64k aligned\n", __func__, i->size);
		return -EINVAL;
	}

	if (is_align_4k(i->addr) ||
	    is_align_4k(i->size) ||
	    is_align_4k(i->bin)) {
		pr_info("%s: addr/size not 4k aligned\n", __func__);
		return -EINVAL;
	}

	if ((i->bin + i->size) > bin_end) {
		pr_info("%s: wrong size\n", __func__);
		return -EINVAL;
	}
#undef is_align_4k
#undef is_align_64k

	return 0;
}

dma_addr_t preload_iova_alloc(struct platform_device *pdev,
	struct vpu_device *vd, struct vpu_iova *vi,
	uint32_t addr, uint32_t size, uint32_t bin)
{
	dma_addr_t mva;

	vi->addr = addr;
	vi->size = size;
	vi->bin = bin;

	if (preload_iova_check(vi))
		return 0;

	mva = vpu_iova_alloc(pdev, vi);

	if (!mva)
		pr_info("%s: vpu%d: iova allcation failed\n", __func__, vd->id);

	vpu_drv_debug(
		"%s: vpu%d: addr:0x%X, size: 0x%X, bin: 0x%X, mva: 0x%lx\n",
		__func__, vd->id, vi->addr, vi->size, vi->bin,
		(unsigned long) mva);

	return mva;
}

#define PRELOAD_IRAM 0xFFFFFFFF

static uint32_t vpu_init_dev_algo_preload_entry(
	struct platform_device *pdev, struct vpu_device *vd,
	struct vpu_algo_list *al, struct vpu_pre_info *info,
	uint32_t bin)
{
	struct __vpu_algo *alg;
	struct vpu_iova dummy_iova;
	struct vpu_iova *vi;
	uint64_t mva;
	uint32_t addr;
	uint32_t size = __ALIGN_KERNEL(info->file_sz, 0x1000);

	alg = al->ops->get(al, info->name, NULL);

	/* algo is already existed in the list */
	if (alg) {
		al->ops->put(alg);
		if (info->pAddr == PRELOAD_IRAM) {
			addr = 0;  /* dynamic alloc iova */
			vi = &alg->iram;
			mva = preload_iova_alloc(pdev, vd, vi, addr,
				size, info->off);
			alg->a.iram_mva = mva;
			goto added;
		}

		/* other segments had been merged into EXE_SEG */
		size = 0;
		goto out;
	}

	/* add new algo to the list */
	addr = info->start_addr & 0xFFFF0000; /* static alloc iova */
	if (info->info)
		size = info->info;  // already aligned at packing stage

	alg = vpu_alg_alloc(al);
	if (!alg)
		goto out;

	al->cnt++;
	strncpy(alg->a.name, info->name, (ALGO_NAMELEN - 1));

	if (info->flag & 0x1 /* EXE_SEG */) {
		vi = &alg->prog;
		alg->a.entry_off = info->pAddr - addr;
	} else {
		vi = &dummy_iova;
		pr_info("%s: vpu%d: unexpected segment: flags: %x\n",
			__func__, vd->id, info->flag);
	}

	mva = preload_iova_alloc(pdev, vd, vi, addr, size, info->off);
	alg->a.mva = mva;

	if (!alg->a.mva)
		goto out;

	alg->builtin = true;
	list_add_tail(&alg->list, &al->a);

added:
	vpu_drv_debug("%s: vpu%d(%xh): %s <%s>: off: %x, mva: %llx, size: %x, addr: %x\n",
		__func__, vd->id, info->vpu_core, info->name,
		(info->pAddr == PRELOAD_IRAM) ? "IRAM" : "PROG",
		info->off, mva, size, addr);
out:
	return bin + size;
}

static int vpu_init_dev_algo_preload(struct platform_device *pdev,
	struct vpu_device *vd, struct vpu_algo_list *al)
{
	int i, j, ret = 0;
	uint32_t offset;
	struct vpu_pre_info *info = NULL;
	struct vpu_image_header *header = NULL;

	vpu_algo_list_init(vd, al, &vpu_prelaod_aops, "Preload");

	offset = vpu_drv->bin_preload_ofs;

	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		header = bin_header(i);
		info = (void *)((unsigned long)header + header->pre_info);

		for (j = 0; j < header->pre_info_count; j++, info++) {
			if (!((info->vpu_core & 0xF) & (1 << vd->id)))
				continue;

			offset = vpu_init_dev_algo_preload_entry(
				pdev, vd, al, info, offset);
		}
	}

	return ret;
}
#endif

static int vpu_init_dev_algo_normal(struct platform_device *pdev,
	struct vpu_device *vd, struct vpu_algo_list *al)
{
	int i, j;
	int ret = 0;
	unsigned int mva;
	struct vpu_algo_info *algo_info;
	struct vpu_image_header *header = NULL;

	vpu_algo_list_init(vd, al, &vpu_normal_aops, "Normal");

	/* for each algo in the image header, add them to device's algo list */
	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		header = bin_header(i);
		for (j = 0; j < header->algo_info_count; j++) {
			struct __vpu_algo *alg;

			algo_info = bin_algo_info(header, j);
			mva = algo_info->offset - vpu_drv->iova_algo.bin +
				vpu_drv->mva_algo;

			/* skips, if the core mask mismatch */
			if (!((algo_info->vpu_core & 0xF) & (1 << vd->id)))
				continue;

			vpu_drv_debug("%s: vpu%d(%xh): %s: off: %x, mva: %x, len: %x\n",
				__func__,
				vd->id,
				algo_info->vpu_core,
				algo_info->name,
				algo_info->offset,
				mva,
				algo_info->length);

			alg = vpu_alg_alloc(al);
			if (!alg) {
				ret = -ENOMEM;
				goto out;
			}

			strncpy(alg->a.name,
				algo_info->name, (ALGO_NAMELEN - 1));
			alg->a.mva = mva;
			alg->a.len = algo_info->length;
			alg->builtin = true;

			list_add_tail(&alg->list, &al->a);
			al->cnt++;
		}
		vpu_drv_debug("%s: vpu%d, total algo count: %d\n",
			__func__, vd->id, al->cnt);
	}
out:
	return ret;
}

/* called by vpu_probe() */
int vpu_init_dev_algo(struct platform_device *pdev, struct vpu_device *vd)
{
	int ret;

	ret = vpu_init_dev_algo_normal(pdev, vd, &vd->aln);
	if (ret)
		goto out;
#if VPU_IMG_PRELOAD
	ret = vpu_init_dev_algo_preload(pdev, vd, &vd->alp);
#endif
out:
	return ret;
}

/* called by vpu_remove() */
void vpu_exit_dev_algo(struct platform_device *pdev, struct vpu_device *vd)
{
	vpu_exit_dev_algo_general(pdev, vd, &vd->aln);
#if VPU_IMG_PRELOAD
	vpu_exit_dev_algo_general(pdev, vd, &vd->alp);
#endif
}

#if VPU_XOS
wait_queue_head_t *vpu_isr_check_cmd(struct vpu_device *vd,
	uint32_t inf, uint32_t prio)
{
	wait_queue_head_t *wake = NULL;
	uint32_t ret = 0;

	if ((DS_PREEMPT_DONE | DS_ALG_DONE | DS_ALG_RDY |
		 DS_DSP_RDY | DS_DBG_RDY | DS_FTRACE_RDY) & inf) {
		ret = vpu_reg_read(vd, XTENSA_INFO00);
		ret = (ret >> (prio * 8)) & 0xFF;
		vpu_cmd_done(vd, prio, ret, vpu_reg_read(vd, XTENSA_INFO02));
		wake = vpu_cmd_waitq(vd, prio);
	}

#define DS(a) \
	((inf & DS_##a) ? ":"#a : "")
	vpu_cmd_debug(
		"%s: vpu%d: wake: %d, val: %d, prio: %d, %xh%s%s%s%s%s%s%s%s\n",
		__func__, vd->id, (wake) ? 1 : 0, ret, prio, inf,
		DS(DSP_RDY), /* boot-up done */
		DS(DBG_RDY), /* set-debug done */
		DS(ALG_RDY), /* do-loader done */
		DS(ALG_DONE), /* d2d done */
		DS(ALG_GOT), /* get-algo done */
		DS(PREEMPT_RDY), /* context switch done */
		DS(PREEMPT_DONE), /* d2d-ext done */
		DS(FTRACE_RDY) /* set-ftrace done */);
#undef DS

	vd->dev_state = inf;  /* for debug */
	return wake;
}

static bool vpu_isr_check_unlock(uint32_t inf)
{
	return (DS_PREEMPT_RDY & inf);
}

#else

wait_queue_head_t *vpu_isr_check_cmd(struct vpu_device *vd,
	uint32_t inf, uint32_t prio)
{
	wait_queue_head_t *wake = NULL;
	uint32_t ret = 0;

	ret = vpu_reg_read(vd, XTENSA_INFO00);

	if (ret != VPU_STATE_BUSY) {
		ret = vpu_reg_read(vd, XTENSA_INFO00);
		vpu_cmd_done(vd, prio, ret, vpu_reg_read(vd, XTENSA_INFO02));
		wake = vpu_cmd_waitq(vd, prio);
	}

	vpu_cmd_debug(
		"%s: vpu%d: wake: %d, val: %d, prio: %d, %xh\n",
		__func__, vd->id, (wake) ? 1 : 0, ret, prio, inf);

	vd->dev_state = inf;  /* for debug */
	return wake;
}

static bool vpu_isr_check_unlock(uint32_t inf)
{
	return true;
}

#endif

irqreturn_t vpu_isr(int irq, void *dev_id)
{
	struct vpu_device *vd = (struct vpu_device *)dev_id;
	uint32_t ret, req_cmd, prio, inf, val;
	wait_queue_head_t *waitq = NULL;
	bool unlock = false;

	/* INFO17: device state */
	ret = vpu_reg_read(vd, XTENSA_INFO17);
	vpu_reg_write(vd, XTENSA_INFO17, 0);
	req_cmd = ret & 0xFFFF;    /* request cmd */
	inf = ret & 0x00FF0000;    /* dev state */
	prio = (ret >> 28) & 0xF;  /* priroity */

	vpu_cmd_debug("%s: vpu%d: INFO17: %0xh, req: %xh, inf: %xh, prio: %xh\n",
		__func__, vd->id, ret, req_cmd, inf, prio);

	if (req_cmd == VPU_REQ_DO_CHECK_STATE) {
		waitq = vpu_isr_check_cmd(vd, inf, prio);
		unlock = vpu_isr_check_unlock(inf);
	}

	/* MET */
	vpu_met_isr(vd);

	/* clear int */
	val = vpu_reg_read(vd, XTENSA_INT);

	if (val) {
		vpu_cmd_debug("%s: vpu%d: XTENSA_INT = (%d)\n",
			__func__, vd->id, val);
	}
	vpu_reg_write(vd, XTENSA_INT, 1);

	if (unlock)
		vpu_xos_unlock(vd);

	if (waitq && (waitqueue_active(waitq))) /* cmd wait */
		wake_up_interruptible(waitq);

	return IRQ_HANDLED;
}

// vd->cmd_lock, should be acquired before calling this function
int vpu_execute_d2d(struct vpu_device *vd, struct vpu_request *req)
{
	int ret;
	int result = 0;
	uint32_t cmd = 0;
	unsigned long flags;
	uint64_t start_t;

	VPU_TS_INIT(d2d);
	VPU_TS_START(d2d);
	start_t = sched_clock();
	req->busy_time = 0;
	req->algo_ret = 0;

	vpu_cmd_debug("%s: vpu%d: prio: %d, %s: bw: %d, buf_cnt: %x, sett: %llx +%x\n",
		__func__, vd->id, req->prio,
		vpu_cmd_alg_name(vd, req->prio),
		req->power_param.bw, req->buffer_count,
		req->sett_ptr, req->sett_length);

	ret = vpu_cmd_buf_set(vd, req->prio, req->buffers,
		sizeof(struct vpu_buffer) * req->buffer_count);
	if (ret)
		goto out;

	ret = vpu_reg_lock(vd, false, &flags);
	if (ret)
		goto out;

	/* D2D_EXT: preload algorithm */
	if (VPU_REQ_FLAG_TST(req, ALG_PRELOAD)) {
		struct __vpu_algo *pre;

		vd->state = VS_CMD_D2D_EXT;
		cmd = VPU_CMD_DO_D2D_EXT;
		pre = vpu_cmd_alg(vd, req->prio);
		vpu_reg_write(vd, XTENSA_INFO11, req->prio);
		vpu_reg_write(vd, XTENSA_INFO16, pre->a.mva + pre->a.entry_off);
		vpu_reg_write(vd, XTENSA_INFO19, pre->a.iram_mva);
	} else {  /* D2D: normal algorithm */
		vpu_reg_write(vd, XTENSA_INFO11, 0);
		vd->state = VS_CMD_D2D;
		cmd = VPU_CMD_DO_D2D;
	}

	vpu_reg_write(vd, XTENSA_INFO12, req->buffer_count);
	vpu_reg_write(vd, XTENSA_INFO13, vpu_cmd_buf_iova(vd, req->prio));
	vpu_reg_write(vd, XTENSA_INFO14, req->sett_ptr);
	vpu_reg_write(vd, XTENSA_INFO15, req->sett_length);
	vpu_trace_begin("vpu_%d|hw_processing_request(%s),prio:%d",
		vd->id, req->algo, req->prio);
	vpu_cmd(vd, req->prio, cmd);
	vpu_reg_unlock(vd, &flags);
	ret = wait_command(vd, req->prio);
	vpu_stall(vd);
	vpu_trace_end("vpu_%d|hw_processing_request(%s),prio:%d",
			vd->id, req->algo, req->prio);

	result = vpu_cmd_result(vd, req->prio);
	req->status = result ? VPU_REQ_STATUS_FAILURE : VPU_REQ_STATUS_SUCCESS;

	if ((ret == -ERESTARTSYS) || (ret == -EAGAIN))
		goto err_cmd;

	/* Error handling */
	if (ret) {
		req->status = VPU_REQ_STATUS_TIMEOUT;
		vpu_aee_excp(vd, req, "VPU Timeout",
			"vpu%d: request (%s) timeout, priority: %d, algo: %s\n",
			vd->id,
			(cmd == VPU_CMD_DO_D2D_EXT) ? "D2D_EXT" : "D2D",
			req->prio, req->algo);
		goto err_cmd;
	}

err_cmd:
	req->algo_ret = vpu_cmd_alg_ret(vd, req->prio);

out:
	VPU_TS_END(d2d);
	req->busy_time = VPU_TS_NS(d2d);
	trace_vpu_cmd(vd->id, req->prio, req->algo, cmd,
		vpu_cmd_boost(vd, req->prio), start_t, ret,
		req->algo_ret, result);
	vpu_cmd_debug("%s: vpu%d: prio: %d, %s: time: %llu ns, ret: %d, alg_ret: %d, result: %d\n",
		__func__, vd->id, req->prio,
		req->algo, req->busy_time, ret, req->algo_ret, result);
	return ret;
}

// reference: vpu_boot_up
// vd->cmd_lock, should be acquired before calling this function
int vpu_dev_boot(struct vpu_device *vd)
{
	int ret = 0;

	if (vd->state <= VS_DOWN) {
		pr_info("%s: unexpected state: %d\n", __func__, vd->state);
		ret = -ENODEV;
		goto err;
	}

	if (vd->state != VS_UP && vd->state != VS_BOOT)
		return ret;

	vd->state = VS_BOOT;
	vd->dev_state = 0;

	vpu_trace_begin("vpu_%d|%s", vd->id, __func__);

	/* VPU boot up sequence */
	ret = vpu_dev_boot_sequence(vd);
	if (ret) {
		pr_info("%s: vpu_dev_boot_sequence: %d\n", __func__, ret);
		goto err;
	}

	/* VPU set debug log  */
	ret = vpu_dev_set_debug(vd);
	if (ret) {
		pr_info("%s: vpu_dev_set_debug: %d\n", __func__, ret);
		goto err;
	}

	/* MET: ftrace setup */
	ret = vpu_set_ftrace(vd);
	if (ret) {
		pr_info("%s: vpu_met_set_ftrace: %d\n", __func__, ret);
		goto err;
	}

	/* MET: pm setup */
	vpu_met_pm_get(vd);

	vd->state = VS_IDLE;

err:
	vpu_trace_end("vpu_%d|%s", vd->id, __func__);
	return ret;
}

/**
 * vpu_execute - execute a vpu request
 * @vd: vpu device
 * @req: vpu request
 *
 * returns: 0: success, otherwise: fail
 */
int vpu_execute(struct vpu_device *vd, struct vpu_request *req)
{
	int ret = 0;
	int ret_pwr = 0;
	int boost = 0;
	uint64_t flags;
	struct vpu_algo_list *al;
	enum vpu_state state;

	vpu_cmd_lock(vd, req->prio);
	/* Backup/Restore request flags for elevate to preload algo */
	flags = req->flags;
	/* Bootup VPU */
	mutex_lock_nested(&vd->lock, VPU_MUTEX_DEV);
	boost = vpu_cmd_boost_set(vd, req->prio, req->power_param.boost_value);
	ret_pwr = vpu_pwr_get_locked(vd, boost);
	if (!ret_pwr)
		ret = vpu_dev_boot(vd);
	/* Backup original state */
	state = vd->state;
	mutex_unlock(&vd->lock);

	if (ret_pwr || (ret == -ETIMEDOUT))
		goto err_remove;
	if (ret)
		goto err_boot;

	/* Load Algorithm (DO_LOADER) */
	if (!req->prio &&
		!VPU_REQ_FLAG_TST(req, ALG_PRELOAD) &&
		!VPU_REQ_FLAG_TST(req, ALG_RELOAD) &&
		vd->cmd[0].alg &&
		(vd->cmd[0].alg->al == &vd->aln) &&
		(!strcmp(vd->cmd[0].alg->a.name, req->algo)))
		goto send_req;

	al = VPU_REQ_FLAG_TST(req, ALG_PRELOAD) ? (&vd->alp) : (&vd->aln);

retry:
	ret = al->ops->load(al, req->algo, NULL, req->prio);

	/* Elevate to preloaded algo, if normal algo was not found */
	if ((ret == -ENOENT) && (al == &vd->aln)) {
		VPU_REQ_FLAG_SET(req, ALG_PRELOAD);
		al = &vd->alp;
		goto retry;
	}

	if (ret) {
		pr_info("%s: vpu%d: \"%s\" was not found\n",
			__func__, al->vd->id, req->algo);
		goto err_alg;
	}

send_req:
	/* Send Request (DO_D2D) */
	ret = vpu_execute_d2d(vd, req);

err_alg:
	if (ret == -ETIMEDOUT)
		goto err_remove;

err_boot:
	mutex_lock_nested(&vd->lock, VPU_MUTEX_DEV);
	/* Skip, if other priorities had timeout and powered down */
	if (vd->state != VS_DOWN) {
		/* There's no more active commands => idle */
		if (!atomic_read(&vd->cmd_active))
			vd->state = VS_IDLE;
		/* Otherwise, restore to original state */
		else
			vd->state = state;
		boost = vpu_cmd_boost_put(vd, req->prio);
		vpu_pwr_put_locked(vd, boost);
	}
	mutex_unlock(&vd->lock);

err_remove:
	req->flags = flags;
	vpu_cmd_unlock(vd, req->prio);

	return ret;
}

/**
 * vpu_preempt - Run vpu request of a preloaded algorithm
 * @vd: vpu device
 * @req: vpu request
 *
 * returns: 0: success, otherwise: fail
 */
int vpu_preempt(struct vpu_device *vd, struct vpu_request *req)
{
	int ret = 0;
	int prio;

	prio = atomic_inc_return(&vd->cmd_prio);
	if (prio >= vd->cmd_prio_max) {
		ret = -EPERM;
		goto out;
	}
	req->prio = prio;
	ret = vpu_execute(vd, req);
out:
	atomic_dec(&vd->cmd_prio);
	return ret;
}

/* driver hw init */
int vpu_init_drv_hw(void)
{
	vpu_emi_mpu_set(vpu_drv->bin_pa, vpu_drv->bin_size);
	return 0;
}

/* device hw init */
int vpu_init_dev_hw(struct platform_device *pdev, struct vpu_device *vd)
{
	int ret = 0;

	ret = request_irq(vd->irq_num,	vpu_isr,
		irq_get_trigger_type(vd->irq_num),
		vd->name, vd);

	if (ret) {
		pr_info("%s: %s: fail to request irq: %d\n",
			__func__, vd->name, ret);
		goto out;
	}

	mutex_init(&vd->lock);
	spin_lock_init(&vd->reg_lock);
	vpu_xos_unlock(vd);
	ret = vpu_cmd_init(pdev, vd);
	if (ret) {
		pr_info("%s: %s: fail to init commands: %d\n",
			__func__, vd->name, ret);
		goto out;
	}

out:
	return ret;
}

/* driver hw exit function */
int vpu_exit_drv_hw(void)
{
	return 0;
}


/* device hw exit function */
int vpu_exit_dev_hw(struct platform_device *pdev, struct vpu_device *vd)
{
	vpu_cmd_exit(vd);
	free_irq(vd->irq_num, vd);
	return 0;
}

int vpu_dev_boot_sequence(struct vpu_device *vd)
{
	uint64_t start_t;
	int ret;

	start_t = sched_clock();
	/* No need to take vd->reg_lock,
	 * 1. boot-up may take a while.
	 * 2. Only one process can do boot sequence, since it'sprotected
	 *    by device lock vd->lock,
	 */
	vpu_trace_begin("vpu_%d|%s", vd->id, __func__);
	vpu_reg_write(vd, XTENSA_ALTRESETVEC, vd->iova_reset.addr);
	vpu_reg_set(vd, CTRL, P_DEBUG_ENABLE | STATE_VECTOR_SELECT);
	vpu_reg_set(vd, CTRL, PBCLK_ENABLE);
	vpu_reg_clr(vd, CTRL, PRID);
	vpu_reg_set(vd, CTRL, (vd->id << 1) & PRID);
	vpu_reg_set(vd, SW_RST, OCDHALTONRESET);
	vpu_reg_clr(vd, SW_RST, OCDHALTONRESET);
	vpu_reg_set(vd, SW_RST, APU_B_RST | APU_D_RST);
	vpu_reg_clr(vd, SW_RST, APU_B_RST | APU_D_RST);
	/* pif gated disable, to prevent unknown propagate to BUS */
	vpu_reg_clr(vd, CTRL, PIF_GATED);
	/* AXI control */
	vpu_reg_set(vd, DEFAULT0, AWUSER | ARUSER);
	vpu_reg_set(vd, DEFAULT1, ARUSER_IDMA | AWUSER_IDMA);
	/* default set pre-ultra instead of ultra */
	vpu_reg_set(vd, DEFAULT0, QOS_SWAP);
	/* jtag enable */
	if (vd->jtag_enabled) {
		vpu_reg_set(vd, CG_CLR, JTAG_CG_CLR);
		vpu_reg_set(vd, DEFAULT2, DBG_EN);
	}
#ifdef MBOX_INBOX_MASK
	/* mbox interrupt mask */
	vpu_reg_set(vd, MBOX_INBOX_MASK, 0xffffffff);
#endif
	/* set log buffer */
	vpu_reg_write(vd, XTENSA_INFO19, vd->mva_iram);
	vpu_reg_write(vd, XTENSA_INFO21,
		vd->iova_work.m.pa + VPU_LOG_OFFSET);
	vpu_reg_write(vd, XTENSA_INFO22, vd->wb_log_size);

	vpu_run(vd, 0, 0);

	pr_info("%s: vpu%d: ALTRESETVEC: 0x%x\n", __func__,
		vd->id, vpu_reg_read(vd, XTENSA_ALTRESETVEC));

	ret = wait_command(vd, 0);
	vpu_stall(vd);
	if (ret == -ETIMEDOUT)
		goto err_timeout;
	if ((ret == -ERESTARTSYS) || (ret == -EAGAIN))
		goto out;

	ret = vpu_cmd_result(vd, 0);

err_timeout:
	if (ret) {
		vpu_aee_excp_locked(vd, NULL, "VPU Timeout",
			"vpu%d: boot-up timeout\n",	vd->id);
	}

out:
	trace_vpu_cmd(vd->id, 0, "", 0, atomic_read(&vd->pw_boost),
		start_t, ret, 0, 0);
	vpu_trace_end("vpu_%d|%s", vd->id, __func__);
	return ret;
}

// vd->cmd_lock, should be acquired before calling this function
int vpu_dev_set_debug(struct vpu_device *vd)
{
	int ret;
	struct timespec now;
	unsigned int device_version = 0x0;
	unsigned long flags;
	uint64_t start_t;

	vpu_cmd_debug("%s: vpu%d\n", __func__, vd->id);
	start_t = sched_clock();
	getnstimeofday(&now);

	/* SET_DEBUG */
	ret = vpu_reg_lock(vd, true, &flags);
	if (ret)
		goto out;

	vpu_reg_write(vd, XTENSA_INFO01, VPU_CMD_SET_DEBUG);
	vpu_reg_write(vd, XTENSA_INFO23,
		now.tv_sec * 1000000 + now.tv_nsec / 1000);

	vpu_reg_write(vd, XTENSA_INFO29, HOST_VERSION);
	vpu_trace_begin("vpu_%d|%s", vd->id, __func__);
	vpu_cmd(vd, 0, VPU_CMD_SET_DEBUG);
	vpu_reg_unlock(vd, &flags);

	vpu_cmd_debug("%s: vpu%d: iram: %x, log: %x, time: %x\n",
		__func__, vd->id,
		vpu_reg_read(vd, XTENSA_INFO19),
		vpu_reg_read(vd, XTENSA_INFO21),
		vpu_reg_read(vd, XTENSA_INFO23));
	vpu_cmd_debug("%s: vpu%d: timestamp: %.2lu:%.2lu:%.2lu:%.6lu\n",
		__func__, vd->id,
		(now.tv_sec / 3600) % (24),
		(now.tv_sec / 60) % (60),
		now.tv_sec % 60,
		now.tv_nsec / 1000);

	/* 3. wait until done */
	ret = wait_command(vd, 0);
	vpu_stall(vd);
	vpu_trace_end("vpu_%d|%s", vd->id, __func__);

	if ((ret == -ERESTARTSYS) || (ret == -EAGAIN))
		goto out;

	if (ret)
		goto err;

	/*3-additional. check vpu device/host version is matched or not*/
	device_version = vpu_reg_read(vd, XTENSA_INFO20);

	if ((int)device_version < (int)HOST_VERSION) {
		pr_info("%s: vpu%d: incompatible ftrace version: vd: %x, host: %x\n",
			__func__, vd->id,
			vpu_reg_read(vd, XTENSA_INFO20),
			vpu_reg_read(vd, XTENSA_INFO29));
		vd->ftrace_avail = false;
	} else {
		vd->ftrace_avail = true;
	}

err:
	if (ret) {
		vpu_aee_excp_locked(vd, NULL, "VPU Timeout",
			"vpu%d: set debug (SET_DEBUG) timeout\n",
			vd->id);
		goto out;
	}

	/* 4. check the result */
	ret = vpu_cmd_result(vd, 0);

out:
	trace_vpu_cmd(vd->id, 0, "", VPU_CMD_SET_DEBUG,
		atomic_read(&vd->pw_boost), start_t, ret, 0, 0);
	if (ret)
		pr_info("%s: vpu%d: fail to set debug: %d\n",
			__func__, vd->id, ret);

	return ret;
}

// vd->cmd_lock, should be acquired before calling this function
int vpu_hw_alg_init(struct vpu_algo_list *al, struct __vpu_algo *algo)
{
	struct vpu_device *vd = al->vd;
	unsigned long flags;
	uint64_t start_t;
	int ret;

	start_t = sched_clock();
	vpu_cmd_debug("%s: vpu%d: %s: mva/length (0x%lx/0x%x)\n",
		__func__, vd->id, algo->a.name,
		(unsigned long)algo->a.mva, algo->a.len);

	/* DO_LOADER */
	ret = vpu_reg_lock(vd, false, &flags);
	if (ret)
		goto out;

	vd->state = VS_CMD_ALG;
	vpu_reg_write(vd, XTENSA_INFO12, algo->a.mva);
	vpu_reg_write(vd, XTENSA_INFO13, algo->a.len);
	vpu_reg_write(vd, XTENSA_INFO15, 0);
	vpu_reg_write(vd, XTENSA_INFO16, 0);
	vpu_trace_begin("vpu_%d|%s", vd->id, __func__);
	vpu_cmd(vd, 0, VPU_CMD_DO_LOADER);
	vpu_reg_unlock(vd, &flags);

	ret = wait_command(vd, 0);
	vpu_stall(vd);

	vpu_trace_end("vpu_%d|%s", vd->id, __func__);

	if ((ret == -ERESTARTSYS) || (ret == -EAGAIN))
		goto out;
	if (ret) {
		vpu_aee_excp(vd, NULL, "VPU Timeout",
			"vpu%d: load algo (DO_LOADER) timeout, algo: %s\n",
			vd->id, algo->a.name);
		goto out;
	}

out:
	trace_vpu_cmd(vd->id, 0, algo->a.name, VPU_CMD_DO_LOADER,
		vpu_cmd_boost(vd, 0), start_t, ret, 0, 0);
	vpu_cmd_debug("%s: vpu%d: %s: %d\n",
		__func__, vd->id, algo->a.name, ret);
	return ret;
}

static int vpu_set_ftrace(struct vpu_device *vd)
{
	int ret = 0;
	unsigned long flags;
	uint64_t start_t;

	start_t = sched_clock();
	/* SET_FTRACE */
	ret = vpu_reg_lock(vd, true, &flags);
	if (ret)
		goto out;

	vpu_reg_write(vd, XTENSA_INFO05, (vpu_drv->met) ? 1 : 0);
	/* set vpu internal log level */
	vpu_reg_write(vd, XTENSA_INFO06, vpu_drv->ilog);
	/* clear info18 */
	vpu_reg_write(vd, XTENSA_INFO18, 0);
	vpu_cmd(vd, 0, VPU_CMD_SET_FTRACE_LOG);
	vpu_reg_unlock(vd, &flags);
	ret = wait_command(vd, 0);
	vpu_stall(vd);

	if ((ret == -ERESTARTSYS) || (ret == -EAGAIN))
		goto out;

	/* Error handling */
	if (ret) {
		vpu_aee_excp_locked(vd, NULL, "VPU Timeout",
			"vpu%d: request (SET_FTRACE) timeout\n", vd->id);
		goto out;
	}
	ret = vpu_cmd_result(vd, 0);

out:
	trace_vpu_cmd(vd->id, 0, "", VPU_CMD_SET_FTRACE_LOG,
		atomic_read(&vd->pw_boost), start_t, ret, 0, 0);
	if (ret)
		pr_info("%s: vpu%d: fail to set ftrace: %d\n",
			__func__, vd->id, ret);

	return ret;
}

