// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include <linux/sched/clock.h>

#include "vpu_algo.h"
#include "vpu_cmd.h"
#include "vpu_mem.h"
#include "vpu_power.h"
#include "vpu_debug.h"
#include "vpu_cmn.h"
#include "vpu_dump.h"
#include "vpu_trace.h"
#include "vpu_met.h"
#include "vpu_tag.h"
#define CREATE_TRACE_POINTS
#include "vpu_events.h"

#define VPU_TS_INIT(a) \
	struct timespec64 a##_s, a##_e

#define VPU_TS_START(a) \
	ktime_get_ts64(&a##_s)

#define VPU_TS_END(a) \
	ktime_get_ts64(&a##_e)

#define VPU_TS_NS(a) \
	((uint64_t)(timespec64_to_ns(&a##_e) - timespec64_to_ns(&a##_s)))

#define VPU_TS_US(a) \
	(VPU_TS_NS(a) / 1000)

static int wait_idle(struct vpu_device *vd, uint32_t latency, uint32_t retry);
static int vpu_set_ftrace(struct vpu_device *vd);
static void vpu_run(struct vpu_device *vd, int prio, uint32_t cmd)
{
	struct vpu_register *r = vd_reg(vd);

	vpu_reg_write(vd, xtensa_info01, cmd);
	vpu_cmd_run(vd, prio, cmd);
	vpu_reg_clr(vd, ctrl, r->stall);
}

static inline void __vpu_stall(struct vpu_device *vd)
{
	struct vpu_register *r = vd_reg(vd);

	vpu_reg_set(vd, ctrl, r->stall);
}

static inline void vpu_stall(struct vpu_device *vd)
{
	if (xos_type(vd) == VPU_NON_XOS)
		__vpu_stall(vd);

	/* XOS doesn't need stall */
}

static void vpu_cmd(struct vpu_device *vd, int prio, uint32_t cmd)
{
	vpu_cmd_debug("%s: vpu%d: cmd: 0x%x (%s)\n",
		__func__, vd->id, cmd, vpu_debug_cmd_str(cmd));
	vpu_run(vd, prio, cmd);
	wmb();  /* make sure register committed */
	vpu_reg_set(vd, ctl_xtensa_int, 1);
}

#define XOS_UNLOCKED 0
#define XOS_LOCKED   1

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

	if (xos_type(vd) != VPU_XOS)
		return 0;

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

	/* Other priorites had timeout, shutdown by vpu_excp */
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

/**
 * vpu_xos_unlock - unlock xos when forced power down
 * @vd: vpu device
 */
static void vpu_xos_unlock(struct vpu_device *vd)
{
	if (xos_type(vd) != VPU_XOS)
		return;

	vpu_cmd_debug("%s: vpu%d\n", __func__, vd->id);
	atomic_set(&vd->xos_state, XOS_UNLOCKED);
}

/**
 * vpu_xos_wait_idle - wait for xos idle when forced power down
 * @vd: vpu device
 *
 * Retruns -EBUSY, if device is still busy after
 * (WAIT_XOS_LATENCY_US * WAIT_XOS_RETRY) us
 */
static int vpu_xos_wait_idle(struct vpu_device *vd)
{
	struct vpu_config *cfg = vd_cfg(vd);

	if (cfg->xos != VPU_XOS)
		return 0;

	return wait_idle(vd, cfg->wait_xos_latency_us, cfg->wait_xos_retry);
}

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
	int ret = 0;
	struct vpu_sys_ops *sops = vd_sops(vd);

	if (sops->xos_lock)
		ret = sops->xos_lock(vd);

	if (ret == 0) {
		spin_lock_irqsave(&vd->reg_lock, (*flags));
	} else if (ret == -ETIME) {
		if (boot) {
			vpu_excp_locked(vd, NULL,
				"VPU Timeout",
				"vpu%d: XOS lock timeout (boot)\n",
				vd->id);
		} else {
			vpu_excp(vd, NULL,
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
	uint32_t st = 0;
	uint32_t pwait = 0;
	unsigned int count = 0;
	struct vpu_register *r = vd_reg(vd);

	do {
		st = vpu_reg_read(vd, done_st);
		count++;
		pwait = !!(st & r->pwaitmode);
		if (pwait)
			return 0;
		udelay(latency);
		trace_vpu_wait(vd->id, st,
			vpu_reg_read(vd, xtensa_info00),
			vpu_reg_read(vd, xtensa_info25),
			vpu_reg_read(vd, debug_info05));
	} while (count < retry);

	pr_info("%s: vpu%d: %d us: done_st: 0x%x, pwaitmode: %d, info00: 0x%x, info25: 0x%x\n",
		__func__, vd->id, (latency * retry), st, pwait,
		vpu_reg_read(vd, xtensa_info00),
		vpu_reg_read(vd, xtensa_info25));

	return -EBUSY;
}

static int wait_command(struct vpu_device *vd, int prio_s)
{
	int ret = 0;
	unsigned int prio = prio_s;
	bool retry = true;
	struct vpu_config *cfg = vd_cfg(vd);

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

	/* Other priorites had timeout, shutdown by vpu_excp */
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
	wait_idle(vd, cfg->wait_cmd_latency_us, cfg->wait_cmd_retry);

out:
	return ret;
}

static void vpu_exit_dev_algo_general(struct vpu_device *vd,
	struct vpu_algo_list *al)
{
	struct __vpu_algo *alg;
	struct list_head *ptr, *tmp;

	if (!al)
		return;

	vpu_alg_debug("%s: %s\n", __func__, al->name);
	list_for_each_safe(ptr, tmp, &al->a) {
		alg = list_entry(ptr, struct __vpu_algo, list);
		al->ops->put(alg);
	}
}

static void *bin_header_legacy(int i)
{
	struct vpu_img_hdr_legacy *hl;
	unsigned long bin = (unsigned long)vpu_drv->bin_va;

	hl = (struct vpu_img_hdr_legacy *)(bin +
		vpu_drv->vp->cfg->bin_ofs_header);

	return &hl[i];
}

static struct vpu_algo_info *bin_alg_info_legacy(void *h, int j)
{
	struct vpu_img_hdr_legacy *hl = h;

	return &hl->algo_infos[j];
}

static int bin_alg_info_cnt_legacy(void *h)
{
	struct vpu_img_hdr_legacy *hl = h;

	return hl->algo_info_count;
}

static void *bin_header_preload(int index)
{
	int i;
	uint64_t ptr = (unsigned long)vpu_drv->bin_va;
	struct vpu_img_hdr_preload *hp;

	ptr += vpu_drv->bin_head_ofs;
	hp = (void *)ptr;

	for (i = 0; i < index; i++) {
		ptr += hp->header_size;
		hp = (void *)ptr;
	}

	return (void *)hp;
}

static struct vpu_algo_info *bin_algo_info_preload(
	void *h, int j)
{
	struct vpu_img_hdr_preload *hp = h;
	struct vpu_algo_info *algo_info;

	algo_info = (void *)((unsigned long)hp + hp->alg_info);
	return &algo_info[j];
}

static int bin_alg_info_cnt_preload(void *h)
{
	struct vpu_img_hdr_preload *hp = h;

	return hp->algo_info_count;
}

static uint32_t bin_pre_info_preload(void *h)
{
	struct vpu_img_hdr_preload *hp = h;

	return hp->pre_info;
}

static int bin_pre_info_cnt_preload(void *h)
{
	struct vpu_img_hdr_preload *hp = h;

	return hp->pre_info_count;
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

static dma_addr_t preload_iova_alloc(
	struct vpu_device *vd, struct vpu_iova *vi,
	uint32_t addr, uint32_t size, uint32_t bin)
{
	dma_addr_t mva;

	vi->addr = addr;
	vi->size = size;
	vi->bin = bin;

	if (preload_iova_check(vi))
		return 0;

	mva = vd_mops(vd)->alloc(vd->dev, vi);

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
	struct vpu_device *vd,
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
			mva = preload_iova_alloc(vd, vi, addr,
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
		size = 0;
		vi = &dummy_iova;
		pr_info("%s: vpu%d: unexpected segment: flags: %x\n",
			__func__, vd->id, info->flag);
	}

	mva = preload_iova_alloc(vd, vi, addr, size, info->off);
	alg->a.mva = mva;

	if (!alg->a.mva) {
		vpu_alg_free(alg);
		goto out;
	}

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

static int vpu_init_dev_algo_preload(
	struct vpu_device *vd, struct vpu_algo_list *al)
{
	int i, j, ret = 0;
	uint32_t offset;
	struct vpu_pre_info *info = NULL;
	struct vpu_bin_ops *bops = vd_bops(vd);

	if (!bops->pre_info || !bops->pre_info_cnt)
		return 0;

	vpu_algo_list_init(vd, al, &vpu_prelaod_aops, "Preload");
	offset = vpu_drv->bin_preload_ofs;

	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		void *h = bops->header(i);
		int cnt;

		info = (void *)((unsigned long)h + bops->pre_info(h));
		cnt = bops->pre_info_cnt(h);

		for (j = 0; j < cnt; j++, info++) {
			if (!((info->vpu_core & 0xF) & (1 << vd->id)))
				continue;

			offset = vpu_init_dev_algo_preload_entry(
				vd, al, info, offset);
		}
	}

	return ret;
}

static int vpu_init_dev_algo_normal(
	struct vpu_device *vd, struct vpu_algo_list *al)
{
	int i, j;
	int ret = 0;
	unsigned int mva;
	struct vpu_algo_info *algo_info;
	struct vpu_bin_ops *bops = vd_bops(vd);

	vpu_algo_list_init(vd, al, &vpu_normal_aops, "Normal");

	/* for each algo in the image header, add them to device's algo list */
	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		void *h = bops->header(i);
		int cnt = bops->alg_info_cnt(h);

		for (j = 0; j < cnt; j++) {
			struct __vpu_algo *alg;

			algo_info = bops->alg_info(h, j);
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

	ret = vpu_init_dev_algo_normal(vd, &vd->aln);
	if (ret)
		goto out;

	if (bin_type(vd) == VPU_IMG_PRELOAD)
		ret = vpu_init_dev_algo_preload(vd, &vd->alp);
out:
	return ret;
}

/* called by vpu_remove() */
void vpu_exit_dev_algo(struct platform_device *pdev, struct vpu_device *vd)
{
	vpu_exit_dev_algo_general(vd, &vd->aln);

	if (bin_type(vd) == VPU_IMG_PRELOAD)
		vpu_exit_dev_algo_general(vd, &vd->alp);
}

static wait_queue_head_t *vpu_isr_check_cmd_xos(struct vpu_device *vd,
	uint32_t inf, uint32_t prio)
{
	wait_queue_head_t *wake = NULL;
	uint32_t ret = 0;

	if ((DS_PREEMPT_DONE | DS_ALG_DONE | DS_ALG_RDY |
		 DS_DSP_RDY | DS_DBG_RDY | DS_FTRACE_RDY) & inf) {
		ret = vpu_reg_read(vd, xtensa_info00);
		ret = (ret >> (prio * 8)) & 0xFF;
		vpu_cmd_done(vd, prio, ret, vpu_reg_read(vd, xtensa_info02));
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

static bool vpu_isr_check_unlock_xos(uint32_t inf)
{
	return (DS_PREEMPT_RDY & inf);
}

static wait_queue_head_t *vpu_isr_check_cmd(struct vpu_device *vd,
	uint32_t inf, uint32_t prio)
{
	wait_queue_head_t *wake = NULL;
	uint32_t ret = 0;

	ret = vpu_reg_read(vd, xtensa_info00);

	if (ret != VPU_STATE_BUSY) {
		ret = vpu_reg_read(vd, xtensa_info00);
		vpu_cmd_done(vd, prio, ret, vpu_reg_read(vd, xtensa_info02));
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

static irqreturn_t vpu_isr(int irq, void *dev_id)
{
	struct vpu_device *vd = (struct vpu_device *)dev_id;
	struct vpu_sys_ops *sops = vd_sops(vd);
	uint32_t ret, req_cmd, prio, inf, val;
	wait_queue_head_t *waitq = NULL;
	bool unlock = false;

	/* INFO17: device state */
	ret = vpu_reg_read(vd, xtensa_info17);
	vpu_reg_write(vd, xtensa_info17, 0);
	req_cmd = ret & 0xFFFF;    /* request cmd */
	inf = ret & 0x00FF0000;    /* dev state */
	prio = (ret >> 28) & 0xF;  /* priroity */

	vpu_cmd_debug("%s: vpu%d: INFO17: %0xh, req: %xh, inf: %xh, prio: %xh\n",
		__func__, vd->id, ret, req_cmd, inf, prio);

	if (req_cmd == VPU_REQ_DO_CHECK_STATE) {
		waitq = sops->isr_check_cmd(vd, inf, prio);
		unlock = sops->isr_check_unlock(inf);
	}

	/* MET */
	vpu_met_isr(vd);

	/* clear int */
	val = vpu_reg_read(vd, xtensa_int);

	if (val) {
		vpu_cmd_debug("%s: vpu%d: xtensa_int = (%d)\n",
			__func__, vd->id, val);
	}
	vpu_reg_write(vd, xtensa_int, 1);

	if (unlock && sops->xos_unlock)
		sops->xos_unlock(vd);

	if (waitq && (waitqueue_active(waitq))) /* cmd wait */
		wake_up_interruptible(waitq);

	return IRQ_HANDLED;
}

// vd->cmd_lock, should be acquired before calling this function
static int vpu_execute_d2d(struct vpu_device *vd, struct vpu_request *req)
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
		vpu_reg_write(vd, xtensa_info11, req->prio);
		vpu_reg_write(vd, xtensa_info16, pre->a.mva + pre->a.entry_off);
		vpu_reg_write(vd, xtensa_info19, pre->a.iram_mva);
	} else {  /* D2D: normal algorithm */
		vpu_reg_write(vd, xtensa_info11, 0);
		vd->state = VS_CMD_D2D;
		cmd = VPU_CMD_DO_D2D;
	}

	vpu_reg_write(vd, xtensa_info12, req->buffer_count);
	vpu_reg_write(vd, xtensa_info13, vpu_cmd_buf_iova(vd, req->prio));
	vpu_reg_write(vd, xtensa_info14, req->sett_ptr);
	vpu_reg_write(vd, xtensa_info15, req->sett_length);
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
		vpu_excp(vd, req, "VPU Timeout",
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
	if (!vd_is_available(vd)) {
		ret_pwr = -ENODEV;
		goto nodev;
	}
	boost = vpu_cmd_boost_set(vd, req->prio, req->power_param.boost_value);
	ret_pwr = vpu_pwr_get_locked(vd, boost);
	if (!ret_pwr)
		ret = vpu_dev_boot(vd);
	/* Backup original state */
	state = vd->state;
nodev:
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
	return 0;
}

/* device hw init */
int vpu_init_dev_hw(struct platform_device *pdev, struct vpu_device *vd)
{
	int ret = 0;
	struct vpu_sys_ops *sops = vd_sops(vd);

	ret = request_irq(vd->irq_num,	sops->isr,
		irq_get_trigger_type(vd->irq_num),
		vd->name, vd);

	if (ret) {
		pr_info("%s: %s: fail to request irq: %d\n",
			__func__, vd->name, ret);
		goto out;
	}

	mutex_init(&vd->lock);
	spin_lock_init(&vd->reg_lock);

	if (sops->xos_unlock)
		sops->xos_unlock(vd);

	ret = vpu_cmd_init(vd);
	if (ret) {
		pr_info("%s: %s: fail to init commands: %d\n",
			__func__, vd->name, ret);
		vpu_cmd_exit(vd);
		free_irq(vd->irq_num, vd);
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
	/* stall vpu to prevent iommu translation faults after free iovas */
	__vpu_stall(vd);
	vpu_cmd_exit(vd);
	free_irq(vd->irq_num, vd);
	return 0;
}

int vpu_dev_boot_sequence(struct vpu_device *vd)
{
	uint64_t start_t;
	int ret;
	struct vpu_register *r = vd_reg(vd);
	struct vpu_config *cfg = vd_cfg(vd);

	start_t = sched_clock();
	/* No need to take vd->reg_lock,
	 * 1. boot-up may take a while.
	 * 2. Only one process can do boot sequence, since it'sprotected
	 *    by device lock vd->lock,
	 */
	vpu_trace_begin("vpu_%d|%s", vd->id, __func__);
	vpu_reg_write(vd, xtensa_altresetvec, vd->iova_reset.addr);

	vpu_reg_set(vd, ctrl, r->p_debug_enable | r->state_vector_select);
	vpu_reg_set(vd, ctrl, r->pbclk_enable);
	vpu_reg_clr(vd, ctrl, r->prid);
	vpu_reg_set(vd, ctrl, (vd->id << 1) & r->prid);
	vpu_reg_set(vd, sw_rst, r->ocdhaltonreset);
	vpu_reg_clr(vd, sw_rst, r->ocdhaltonreset);
	vpu_reg_set(vd, sw_rst, r->apu_b_rst | r->apu_d_rst);
	vpu_reg_clr(vd, sw_rst, r->apu_b_rst | r->apu_d_rst);
	/* pif gated disable, to prevent unknown propagate to bus */
	vpu_reg_clr(vd, ctrl, r->pif_gated);
	/* axi control */
	vpu_reg_set(vd, default0, r->awuser | r->aruser);
	vpu_reg_set(vd, default1, r->aruser_idma | r->awuser_idma);
	/* default set pre-ultra instead of ultra */
	vpu_reg_set(vd, default0, r->qos_swap);
	/* jtag enable */
	if (vd->jtag_enabled) {
		vpu_reg_set(vd, cg_clr, r->jtag_cg_clr);
		vpu_reg_set(vd, default2, r->dbg_en);
	}

	/* mbox interrupt mask */
	if (vd_cfg(vd)->dmp_reg_cnt_mbox)
		vpu_reg_set(vd, mbox_inbox_mask, 0xffffffff);

	/* set log buffer */
	vpu_reg_write(vd, xtensa_info19, vd->mva_iram);
	vpu_reg_write(vd, xtensa_info21,
		vd->iova_work.m.pa + cfg->log_ofs);
	vpu_reg_write(vd, xtensa_info22, vd->wb_log_size);

	vpu_run(vd, 0, 0);

	pr_info("%s: vpu%d: ALTRESETVEC: 0x%x\n", __func__,
		vd->id, vpu_reg_read(vd, xtensa_altresetvec));

	ret = wait_command(vd, 0);
	vpu_stall(vd);
	if (ret == -ETIMEDOUT)
		goto err_timeout;
	if ((ret == -ERESTARTSYS) || (ret == -EAGAIN))
		goto out;

	ret = vpu_cmd_result(vd, 0);

err_timeout:
	if (ret) {
		vpu_excp_locked(vd, NULL, "VPU Timeout",
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
	struct timespec64 now;
	unsigned int device_version = 0x0;
	unsigned long flags;
	uint64_t start_t;

	vpu_cmd_debug("%s: vpu%d\n", __func__, vd->id);
	start_t = sched_clock();
	ktime_get_ts64(&now);

	/* SET_DEBUG */
	ret = vpu_reg_lock(vd, true, &flags);
	if (ret)
		goto out;

	vpu_reg_write(vd, xtensa_info01, VPU_CMD_SET_DEBUG);
	vpu_reg_write(vd, xtensa_info23,
		now.tv_sec * 1000000 + now.tv_nsec / 1000);

	vpu_reg_write(vd, xtensa_info29, HOST_VERSION);
	vpu_trace_begin("vpu_%d|%s", vd->id, __func__);
	vpu_cmd(vd, 0, VPU_CMD_SET_DEBUG);
	vpu_reg_unlock(vd, &flags);

	vpu_cmd_debug("%s: vpu%d: iram: %x, log: %x, time: %x\n",
		__func__, vd->id,
		vpu_reg_read(vd, xtensa_info19),
		vpu_reg_read(vd, xtensa_info21),
		vpu_reg_read(vd, xtensa_info23));
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
	device_version = vpu_reg_read(vd, xtensa_info20);

	if ((int)device_version < (int)HOST_VERSION) {
		pr_info("%s: vpu%d: incompatible ftrace version: vd: %x, host: %x\n",
			__func__, vd->id,
			vpu_reg_read(vd, xtensa_info20),
			vpu_reg_read(vd, xtensa_info29));
		vd->ftrace_avail = false;
	} else {
		vd->ftrace_avail = true;
	}

err:
	if (ret) {
		vpu_excp_locked(vd, NULL, "VPU Timeout",
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
	vpu_reg_write(vd, xtensa_info12, algo->a.mva);
	vpu_reg_write(vd, xtensa_info13, algo->a.len);
	vpu_reg_write(vd, xtensa_info15, 0);
	vpu_reg_write(vd, xtensa_info16, 0);
	vpu_trace_begin("vpu_%d|%s", vd->id, __func__);
	vpu_cmd(vd, 0, VPU_CMD_DO_LOADER);
	vpu_reg_unlock(vd, &flags);

	ret = wait_command(vd, 0);
	vpu_stall(vd);

	vpu_trace_end("vpu_%d|%s", vd->id, __func__);

	if ((ret == -ERESTARTSYS) || (ret == -EAGAIN))
		goto out;
	if (ret) {
		vpu_excp(vd, NULL, "VPU Timeout",
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

	vpu_reg_write(vd, xtensa_info05, (vpu_drv->met) ? 1 : 0);
	/* set vpu internal log level */
	vpu_reg_write(vd, xtensa_info06, vpu_drv->ilog);
	/* clear info18 */
	vpu_reg_write(vd, xtensa_info18, 0);
	vpu_cmd(vd, 0, VPU_CMD_SET_FTRACE_LOG);
	vpu_reg_unlock(vd, &flags);
	ret = wait_command(vd, 0);
	vpu_stall(vd);

	if ((ret == -ERESTARTSYS) || (ret == -EAGAIN))
		goto out;

	/* Error handling */
	if (ret) {
		vpu_excp_locked(vd, NULL, "VPU Timeout",
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


struct vpu_sys_ops vpu_sops_mt68xx = {
	.xos_lock = vpu_xos_lock,
	.xos_unlock = vpu_xos_unlock,
	.xos_wait_idle = vpu_xos_wait_idle,
	.isr = vpu_isr,
	.isr_check_cmd = vpu_isr_check_cmd_xos,
	.isr_check_unlock = vpu_isr_check_unlock_xos,
};

struct vpu_sys_ops vpu_sops_mt67xx = {
	.xos_lock = NULL,
	.xos_unlock = NULL,
	.xos_wait_idle = NULL,
	.isr = vpu_isr,
	.isr_check_cmd = vpu_isr_check_cmd,
	.isr_check_unlock = vpu_isr_check_unlock,
};

struct vpu_bin_ops vpu_bops_legacy = {
	.header = bin_header_legacy,
	.alg_info = bin_alg_info_legacy,
	.alg_info_cnt = bin_alg_info_cnt_legacy,
	.pre_info = NULL,
	.pre_info_cnt = NULL,
};

struct vpu_bin_ops vpu_bops_preload = {
	.header = bin_header_preload,
	.alg_info = bin_algo_info_preload,
	.alg_info_cnt = bin_alg_info_cnt_preload,
	.pre_info = bin_pre_info_preload,
	.pre_info_cnt = bin_pre_info_cnt_preload,
};


