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
#include "mtk_devinfo.h"
#include "vpu_algo.h"
#include "vpu_mem.h"
#include "vpu_power.h"
#include "vpu_debug.h"
#include "vpu_dump.h"
#include "vpu_trace.h"
#include "vpu_met.h"
#include <memory/mediatek/emi.h>

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

static int vpu_check_precond(struct vpu_device *vd);
static int vpu_check_postcond(struct vpu_device *vd);
static int vpu_set_ftrace(struct vpu_device *vd);

/* 20180703, 00:00: vpu log mechanism */
// TODO: update device firmware version
#define HOST_VERSION	(0x18070300)

static void vpu_run(struct vpu_device *vd)
{
	vd->cmd_done = false;
	vpu_reg_clr(vd, CTRL, (1 << 23));
}

static void vpu_stall(struct vpu_device *vd)
{
	vpu_reg_set(vd, CTRL, (1 << 23));
}

static void vpu_cmd(struct vpu_device *vd)
{
	vpu_run(vd);
	wmb();  /* make sure register committed */
	vpu_reg_set(vd, CTL_XTENSA_INT, 1);
}


static inline int wait_command(struct vpu_device *vd)
{
	int ret = 0;
	bool retry = true;

	unsigned int PWAITMODE = 0;
	unsigned int count = 0;

start:
	ret = wait_event_interruptible_timeout(
		vd->cmd_wait,
		vd->cmd_done,
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


	/* command success */
	if (ret) {
		ret = 0;
		goto out;
	}

	/* timeout handling */
	ret = -ETIMEDOUT;

	/* check PWAITMODE, request by DE */
	do {
		PWAITMODE = vpu_reg_read(vd, DONE_ST);
		count++;
		if (PWAITMODE & (1 << 7)) {
			ret = 0;
			pr_info("%s: vpu%d: PWAITMODE: %d\n",
				__func__, vd->id, PWAITMODE);
			break;
		}

		pr_info("%s: vpu%d: PWAITMODE: %d, info25: %x\n",
			__func__, vd->id, PWAITMODE,
			vpu_reg_read(vd, XTENSA_INFO25));
		mdelay(2);
	} while (count < WAIT_COMMAND_RETRY);

out:
	return ret;
}

static struct vpu_image_header *bin_header(void)
{
	return (struct vpu_image_header *)
	(((unsigned long)vpu_drv->bin_va) + VPU_OFFSET_IMAGE_HEADERS);
}

static void vpu_emi_mpu_set(unsigned long start, unsigned int size)
{
#ifdef CONFIG_MEDIATEK_EMI
	struct emimpu_region_t md_region;

	mtk_emimpu_init_region(&md_region, MPU_PROCT_REGION);
	mtk_emimpu_set_addr(&md_region, start,
			    (start + (unsigned long)size) - 0x1);
	mtk_emimpu_set_apc(&md_region, MPU_PROCT_D0_AP,
			   MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&md_region, MPU_PROCT_D5_APUSYS,
			   MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_lock_region(&md_region, true);
	mtk_emimpu_set_protection(&md_region);
	mtk_emimpu_free_region(&md_region);
#endif
}

/* called by vpu_probe() */
int vpu_init_dev_algo(struct platform_device *pdev, struct vpu_device *vd)
{
	// for each alog in the image header
	// add to the device
	//	unsigned int coreMagicNum;
	int i, j;
	int ret = 0;
	unsigned int mva;
	struct vpu_algo_info *algo_info;
	struct vpu_image_header *header = bin_header();

	vd->algo_curr = NULL;
	spin_lock_init(&vd->algo_lock);

	// reference: vpu_get_entry_of_algo()
	/* for each algo in the image header, add them to device's algo list */
	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		for (j = 0; j < header[i].algo_info_count; j++) {
			struct __vpu_algo *alg;

			algo_info = &header[i].algo_infos[j];
			mva = algo_info->offset - VPU_OFFSET_ALGO_AREA +
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

			alg = vpu_alg_alloc(vd);
			if (!alg) {
				ret = -ENOMEM;
				goto out;
			}

			strncpy(alg->a.name,
				algo_info->name, (ALGO_NAMELEN - 1));
			alg->a.mva = mva;
			alg->a.len = algo_info->length;
			alg->builtin = true;

			list_add_tail(&alg->list, &vd->algo);
			vd->algo_cnt++;
		}
		vpu_drv_debug("%s: vpu%d, total algo count: %d\n",
			__func__, vd->id, vd->algo_cnt);
	}
out:
	return ret;
}

/* called by vpu_remove() */
void vpu_exit_dev_algo(struct platform_device *pdev, struct vpu_device *vd)
{
	struct __vpu_algo *alg, *tmp;

	vpu_alg_debug("%s: vd: %p, vpu%d, vd->algo: %p\n",
		__func__, vd, vd->id, &vd->algo);

	list_for_each_entry_safe(alg, tmp, &vd->algo, list) {
		vpu_alg_debug("%s: vd: %p, vpu%d, vd->algo: %p, alg: %p\n",
			__func__, vd, vd->id, &vd->algo, alg);
		vpu_alg_put(alg);
	}
}

irqreturn_t vpu_isr(int irq, void *dev_id)
{
	struct vpu_device *vd = (struct vpu_device *)dev_id;
	int req_cmd = 0, normal_check_done = 0;
	uint32_t val;

	/* INFO 17 was used to reply command done */
	req_cmd = vpu_reg_read(vd, XTENSA_INFO17);

	switch (req_cmd) {
	case 0:
		break;
	case VPU_REQ_DO_CHECK_STATE:
	default:
		if (vpu_check_postcond(vd) == -EBUSY) {
		/* host may receive isr to dump */
		/* ftrace log while d2d is stilling running */
		/* but the info17 is set as 0x100 */
		/* in this case, we do nothing for */
		/* cmd state control */
		/* flow while device status is busy */
		// vpu_trace_dump("VPU%d VPU_REQ_DO_CHECK_STATE BUSY", core);
		} else {
			/* other normal cases for cmd state control flow */
			normal_check_done = 1;
		}
		break;
	}

	/* MET */
	vpu_met_isr(vd);

	/* clear int */
	val = vpu_reg_read(vd, XTENSA_INT);

	if (val != 0) {
		vpu_cmd_debug("%s: vpu%d FLD_APMCU_INT = (%d)\n",
				__func__, vd->id, val);
	}
	vpu_reg_write(vd, XTENSA_INT, 1);

	/* wakeup waitqueue */
	if (normal_check_done == 1) {
		vd->cmd_done = true;
		wake_up_interruptible(&vd->cmd_wait);
	}

	return IRQ_HANDLED;
}

// vd->cmd_lock, should be acquired before calling this function
int vpu_execute_d2d(struct vpu_device *vd, struct vpu_request *req)
{
	int ret;

	VPU_TS_INIT(d2d);

	req->busy_time = 0;

	ret = vpu_check_precond(vd);
	if (ret) {
		req->status = VPU_REQ_STATUS_BUSY;
		vpu_cmd_debug("%s: device busy: %d\n", __func__, ret);
		goto out;
	}

	memcpy((void *) vd->iova_work.m.va,
			req->buffers,
			sizeof(struct vpu_buffer) * req->buffer_count);

	vpu_iova_sync_for_device(vd->dev, &vd->iova_work);

	vpu_cmd_debug("%s: vpu%d: %s: bw: %d, buf_cnt: %x, sett: %llx +%x\n",
		__func__, vd->id, vd->algo_curr->a.name,
		req->power_param.bw, req->buffer_count,
		req->sett_ptr, req->sett_length);

	/* 1. write register */
	/* command: d2d */
	vpu_reg_write(vd, XTENSA_INFO01, VPU_CMD_DO_D2D);
	/* buffer count */
	vpu_reg_write(vd, XTENSA_INFO12, req->buffer_count);
	/* pointer to array of struct vpu_buffer */
	vpu_reg_write(vd, XTENSA_INFO13, vd->iova_work.m.pa);
	/* pointer to property buffer */
	vpu_reg_write(vd, XTENSA_INFO14, req->sett_ptr);
	/* size of property buffer */
	vpu_reg_write(vd, XTENSA_INFO15, req->sett_length);

	/* 2. trigger interrupt */
	vpu_trace_begin("vpu-%d|hw_processing_request(%s)",
				vd->id, vd->algo_curr->a.name);

	VPU_TS_START(d2d);

	vpu_cmd(vd);

	ret = wait_command(vd);
	vpu_stall(vd);

	if (ret == -ERESTARTSYS)
		goto err_cmd;

	/* Error handling */
	if (ret) {
		pr_info("%s: vpu%d: DO_D2D timeout: info00: 0x%x, ret: %d\n",
			__func__, vd->id,
			vpu_reg_read(vd, XTENSA_INFO00),
			ret);
		req->status = VPU_REQ_STATUS_TIMEOUT;
		vpu_aee_excp(vd, req, "VPU Timeout",
			"vpu%d: request (DO_D2D) timeout, algo: %s\n",
			vd->id, vd->algo_curr->a.name);
		goto err_cmd;
	}

	VPU_TS_END(d2d);

	req->status = (vpu_check_postcond(vd)) ?
			VPU_REQ_STATUS_FAILURE : VPU_REQ_STATUS_SUCCESS;

	req->busy_time = VPU_TS_NS(d2d);

err_cmd:
	vpu_trace_end("vpu-%d|req_status: %d, ret: %d",
		      vd->id, req->status, ret);
out:
	vpu_cmd_debug("%s: vpu%d: %s: time: %llu ns, ret: %d\n",
		__func__, vd->id, vd->algo_curr->a.name,
		req->busy_time, ret);
	return ret;
}

// reference: vpu_boot_up
// vd->cmd_lock, should be acquired before calling this function
int vpu_dev_boot(struct vpu_device *vd)
{
	int ret = 0;

	vpu_trace_begin("vpu-%d|%s", vd->id, __func__);

	if (vd->state <= VS_DOWN) {
		pr_info("%s: unexpected state: %d\n", __func__, vd->state);
		ret = -ENODEV;
		goto err;
	}

	if (vd->state != VS_UP && vd->state != VS_BOOT)
		return ret;

	vd->state = VS_BOOT;

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
	vpu_trace_end("vpu-%d|%s", vd->id, __func__);
	return ret;
}

// Equal to vpu_hw_processing_request
int vpu_execute(struct vpu_device *vd, struct vpu_request *req)
{
	int ret = 0;

	// TODO: Add preemption handling
	mutex_lock(&vd->cmd_lock);

	/* Bootup VPU */
	ret = vpu_pwr_get_locked(vd, req->power_param.boost_value);
	if (ret)
		goto err_remove;

	ret = vpu_dev_boot(vd);
	if (ret == -ETIMEDOUT)
		goto err_remove;
	if (ret)
		goto err_boot;

	/* Load Algorithm (DO_LOADER) */
	if (!VPU_REQ_FLAG_TST(req, ALG_RELOAD) &&
		vd->algo_curr &&
		(!strcmp(vd->algo_curr->a.name, req->algo)))
		goto send_req;

	vd->state = VS_CMD_ALG;
	ret = vpu_alg_load(vd, req->algo, NULL);
	if (ret)
		goto err_alg;

send_req:
	/* Send Request (DO_D2D) */
	vd->state = VS_CMD_D2D;
	ret = vpu_execute_d2d(vd, req);

err_alg:
	if (ret == -ETIMEDOUT)
		goto err_remove;

err_boot:
	vd->state = VS_IDLE;
	vpu_pwr_put_locked(vd);

err_remove:
	mutex_unlock(&vd->cmd_lock);

	return ret;
}

/**
 * vpu_is_disabled - enable/disable vpu from efuse
 * @vd: struct vpu_device to get the id
 *
 * return 1: this vd->id is disabled
 * return 0: this vd->id is enabled
 */
bool vpu_is_disabled(struct vpu_device *vd)
{
	unsigned int efuse;
	unsigned int mask;

	mask = 1 << vd->id;

	efuse = get_devinfo_with_index(EFUSE_VPU_OFFSET);
	efuse = (efuse >> EFUSE_VPU_SHIFT) & EFUSE_VPU_MASK;

	/* show efuse info to let user know */
	pr_info("%s: efuse_data: 0x%x, core%d is %s\n",
		__func__, efuse, vd->id,
		(efuse & mask) ? "disabled" : "enabled");

	return (efuse & mask);
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
		pr_info("%s: %s: fail to request irq.\n", __func__, vd->name);
		goto out;
	}

	mutex_init(&vd->lock);
	mutex_init(&vd->cmd_lock);
	init_waitqueue_head(&vd->cmd_wait);
	vd->cmd_done = false;
	vd->cmd_timeout = VPU_CMD_TIMEOUT;

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
	free_irq(vd->irq_num, vd);
	return 0;
}


static int vpu_check_precond(struct vpu_device *vd)
{
	uint32_t status;
	size_t i;

	/* wait, if not ready or busy */
	for (i = 0; i < VPU_CHECK_COUNT; i++) {
		status = vpu_reg_read(vd, XTENSA_INFO00);
		switch (status) {
		case VPU_STATE_READY:
		case VPU_STATE_IDLE:
		case VPU_STATE_ERROR:
			return 0;
		case VPU_STATE_NOT_READY:
		case VPU_STATE_BUSY:
			usleep_range(VPU_CHECK_MIN_US, VPU_CHECK_MAX_US);
			break;
		case VPU_STATE_TERMINATED:
			return -EBADFD;
		}
	}
	vpu_cmd_debug("%s: vpu%d still busy: %d, after wait %d ms\n",
		__func__, vd->id, status,
		((VPU_CHECK_MIN_US * VPU_CHECK_COUNT)/1000));
	return -EBUSY;
}

static int vpu_check_postcond(struct vpu_device *vd)
{
	uint32_t status = vpu_reg_read(vd, XTENSA_INFO00);

	switch (status) {
	case VPU_STATE_READY:
	case VPU_STATE_IDLE:
		return 0;
	case VPU_STATE_NOT_READY:
	case VPU_STATE_ERROR:
		return -EIO;
	case VPU_STATE_BUSY:
		return -EBUSY;
	case VPU_STATE_TERMINATED:
		return -EBADFD;
	default:
		return -EINVAL;
	}
}

int vpu_dev_boot_sequence(struct vpu_device *vd)
{
	int ret;

	vpu_trace_begin("vpu-%d|%s", vd->id, __func__);
	/* 1. write register */
	/* set specific address for reset vector in external boot */
	vpu_reg_write(vd, XTENSA_ALTRESETVEC, vd->iova_reset.addr);
	vpu_cmd_debug("%s: vpu%d: ALTRESETVEC: 0x%x\n", __func__,
		vd->id, vpu_reg_read(vd, XTENSA_ALTRESETVEC));

	vpu_reg_set(vd, CTRL, (1 << 31) | (1 << 19));
	vpu_reg_set(vd, CTRL, (1 << 26));
	vpu_reg_clr(vd, CTRL, 0x1FFFE);
	vpu_reg_set(vd, CTRL, (vd->id << 1) & 0x1FFFE);
	vpu_reg_set(vd, SW_RST, (1 << 12));
	vpu_reg_clr(vd, SW_RST, (1 << 12));
	vpu_reg_set(vd, SW_RST, (1 << 4) | (1 << 8));
	vpu_reg_clr(vd, SW_RST, (1 << 4) | (1 << 8));

	/* pif gated disable, to prevent unknown propagate to BUS */
	vpu_reg_clr(vd, CTRL, (1 << 17));

	/* AXI control */
	vpu_reg_set(vd, DEFAULT0, (0x2 << 18) | (0x2 << 23));
	vpu_reg_set(vd, DEFAULT1, (0x2 << 0) | (0x2 << 5));

	/* default set pre-ultra instead of ultra */
	vpu_reg_set(vd, DEFAULT0, (1 << 28));

	/* jtag enable */
	if (vd->jtag_enabled) {
		vpu_reg_set(vd, CG_CLR, 0x2);
		vpu_reg_set(vd, DEFAULT2, 0xf);
	}

	/* 2. trigger to run */
	vpu_run(vd);

	/* 3. wait until done */
	ret = wait_command(vd);
	vpu_stall(vd);
	if (ret == -ETIMEDOUT)
		goto err_timeout;
	if (ret == -ERESTARTSYS)
		goto out;

	ret = vpu_check_postcond(vd);

err_timeout:
	if (ret) {
		pr_info("%s: vpu%d: boot-up timeout, info00: %d, ret: %d\n",
			__func__, vd->id,
			vpu_reg_read(vd, XTENSA_INFO00),
			ret);
		vpu_aee_excp(vd, NULL, "VPU Timeout",
			"vpu%d: boot-up timeout\n",	vd->id);
	}

out:
	vpu_trace_end("vpu-%d|%s", vd->id, __func__);
	return ret;
}

// vd->cmd_lock, should be acquired before calling this function
int vpu_dev_set_debug(struct vpu_device *vd)
{
	int ret;
	struct timespec now;
	unsigned int device_version = 0x0;

	vpu_cmd_debug("%s: vpu%d\n", __func__, vd->id);

	/* 1. set debug */
	getnstimeofday(&now);
	vpu_reg_write(vd, XTENSA_INFO01, VPU_CMD_SET_DEBUG);
	vpu_reg_write(vd, XTENSA_INFO19, vd->mva_iram);
	vpu_reg_write(vd, XTENSA_INFO21,
		vd->iova_work.m.pa + VPU_OFFSET_LOG);
	vpu_reg_write(vd, XTENSA_INFO22, VPU_SIZE_LOG_BUF);
	vpu_reg_write(vd, XTENSA_INFO23,
		now.tv_sec * 1000000 + now.tv_nsec / 1000);

	vpu_reg_write(vd, XTENSA_INFO29, HOST_VERSION);

	vpu_cmd_debug("%s: vpu%d: iram: %x, log: %x, time: %x\n",
		__func__, vd->id,
		vpu_reg_read(vd, XTENSA_INFO19),
		vpu_reg_read(vd, XTENSA_INFO21),
		vpu_reg_read(vd, XTENSA_INFO23));

	/* 2. trigger interrupt */
	vpu_trace_begin("vpu-%d|%s", vd->id, __func__);
	vpu_cmd(vd);

	vpu_cmd_debug("%s: timestamp: %.2lu:%.2lu:%.2lu:%.6lu\n",
		__func__,
		(now.tv_sec / 3600) % (24),
		(now.tv_sec / 60) % (60),
		now.tv_sec % 60,
		now.tv_nsec / 1000);

	/* 3. wait until done */
	ret = wait_command(vd);
	vpu_stall(vd);

	if (ret == -ERESTARTSYS)
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
	vpu_trace_end("vpu-%d|%s", vd->id, __func__);

	if (ret) {
		pr_info("%s: vpu%d: fail, status: %d, ret: %d\n",
			__func__, vd->id,
			vpu_reg_read(vd, XTENSA_INFO00), ret);
		vpu_aee_excp(vd, NULL, "VPU Timeout",
			"vpu%d: set debug (SET_DEBUG) timeout\n",
			vd->id);
		goto out;
	}

	/* 4. check the result */
	ret = vpu_check_postcond(vd);

out:
	if (ret)
		pr_info("%s: vpu%d: fail to set debug\n", __func__, vd->id);

	return ret;
}

// reference: vpu_hw_load_algo
// vd->cmd_lock, should be acquired before calling this function
int vpu_hw_alg_init(struct vpu_device *vd, struct __vpu_algo *algo)
{
	int ret;

	vpu_cmd_debug("%s: vpu%d\n", __func__, vd->id);

	ret = vpu_check_precond(vd);
	if (ret) {
		pr_info("%s: vpu%d: wrong status before do loader!\n",
			__func__, vd->id);
		goto out;
	}

	vpu_alg_debug("%s: vpu%d: algo mva/length (0x%lx/0x%x)\n",
		__func__, vd->id,
		(unsigned long)algo->a.mva, algo->a.len);

	/* 1. write register */
	vpu_reg_write(vd, XTENSA_INFO01, VPU_CMD_DO_LOADER);

	/* binary data's address */
	vpu_reg_write(vd, XTENSA_INFO12, algo->a.mva);

	/* binary data's length */
	vpu_reg_write(vd, XTENSA_INFO13, algo->a.len);

	vpu_reg_write(vd, XTENSA_INFO15,
		0 /* opps.dsp.values[opps.dsp.index] */);

	vpu_reg_write(vd, XTENSA_INFO16,
		0 /*opps.ipu_if.values[opps.ipu_if.index]*/);

	/* 2. trigger interrupt */
	vpu_trace_begin("vpu-%d|%s", vd->id, __func__);

	/* RUN_STALL down */
	vpu_cmd(vd);

	/* 3. wait until done */
	ret = wait_command(vd);
	vpu_stall(vd);

	vpu_cmd_debug("%s: vpu%d: done\n", __func__, vd->id);
	vpu_trace_end("vpu-%d|%s", vd->id, __func__);

	if (ret == -ERESTARTSYS)
		goto out;

	if (ret) {
		pr_info("%s: vpu%d load algo timeout, status:%d, cmd_done: %d, ret: %d\n",
			__func__, vd->id,
			vpu_reg_read(vd, XTENSA_INFO00),
			vd->cmd_done,
			ret);
		vpu_aee_excp(vd, NULL, "VPU Timeout",
			"vpu%d: load algo (DO_LOADER) timeout, algo: %s\n",
			vd->id, algo->a.name);

		goto out;
	}

out:
	return ret;
}

// reference: vpu_hw_get_algo_info
// vd->cmd_lock, should be acquired before calling this function
int vpu_hw_alg_info(struct vpu_device *vd, struct __vpu_algo *alg)
{
	int ret = 0;
	int port_count = 0;
	int info_desc_count = 0;
	int sett_desc_count = 0;
	unsigned int ofs_ports, ofs_info, ofs_info_descs, ofs_sett_descs;
	uint32_t iova = vd->iova_work.m.pa;
	uintptr_t va = vd->iova_work.m.va;
	int i;

	ret = vpu_check_precond(vd);
	if (ret) {
		pr_info("%s: vpu%d: device is not ready\n",
			__func__, vd->id);
		goto out;
	}

	ofs_ports = 0;
	ofs_info = sizeof(((struct vpu_algo *)0)->ports);
	ofs_info_descs = ofs_info + alg->a.info.length;
	ofs_sett_descs = ofs_info_descs +
			sizeof(((struct vpu_algo *)0)->info.desc);

	vpu_alg_debug("%s: vpu%d: check precond done\n",
		__func__, vd->id);

	/* 1. write register */
	vpu_reg_write(vd, XTENSA_INFO01, VPU_CMD_GET_ALGO);
	vpu_reg_write(vd, XTENSA_INFO06, iova + ofs_ports);
	vpu_reg_write(vd, XTENSA_INFO07, iova + ofs_info);
	vpu_reg_write(vd, XTENSA_INFO08, alg->a.info.length);
	vpu_reg_write(vd, XTENSA_INFO10, iova + ofs_info_descs);
	vpu_reg_write(vd, XTENSA_INFO12, iova + ofs_sett_descs);

	/* 2. trigger interrupt */
	vpu_trace_begin("vpu-%d|%s", vd->id, __func__);

	/* RUN_STALL pull down */
	vpu_cmd(vd);

	/* 3. wait until done */
	ret = wait_command(vd);
	vpu_stall(vd);

	if (ret == -ERESTARTSYS)
		goto out;

	vpu_cmd_debug("%s: vpu%d: VPU_CMD_GET_ALGO done\n", __func__, vd->id);

	if (ret) {
		vpu_aee_excp(vd, NULL, "VPU Timeout",
			"vpu%d: timeout to get algo, algo: %s\n",
			vd->id, vd->algo_curr->a.name);
		goto out;
	}

	/* 4. get the return value */
	port_count = vpu_reg_read(vd, XTENSA_INFO05);
	info_desc_count = vpu_reg_read(vd, XTENSA_INFO09);
	sett_desc_count = vpu_reg_read(vd, XTENSA_INFO11);
	alg->a.port_count = port_count;
	alg->a.info.desc_cnt = info_desc_count;
	alg->a.sett.desc_cnt = sett_desc_count;

	vpu_alg_debug("%s: got algo: ports: %d, info: %d, sett: %d\n",
		__func__, port_count, info_desc_count, sett_desc_count);

	/* 5. write back data from working buffer */
	memcpy((void *)(uintptr_t)alg->a.ports,
		(void *)(va + ofs_ports),
		sizeof(struct vpu_port) * port_count);

	for (i = 0 ; i < alg->a.port_count ; i++) {
		vpu_alg_debug("%s: port: %d, id: %d, name: %s, dir: %d, usage: %d\n",
			__func__, i, alg->a.ports[i].id, alg->a.ports[i].name,
			alg->a.ports[i].dir, alg->a.ports[i].usage);
	}

	memcpy((void *)(uintptr_t)alg->a.info.ptr,
		(void *)(va + ofs_info), alg->a.info.length);

	memcpy((void *)(uintptr_t)alg->a.info.desc,
		(void *)(va + ofs_info_descs),
		sizeof(struct vpu_prop_desc) * info_desc_count);

	memcpy((void *)(uintptr_t)alg->a.sett.desc,
		(void *)(va + ofs_sett_descs),
		sizeof(struct vpu_prop_desc) * sett_desc_count);

	vpu_alg_debug("%s: ports: %d, info: %d, sett: %d\n",
		__func__, alg->a.port_count,
		alg->a.info.desc_cnt, alg->a.sett.desc_cnt);

out:
	vpu_trace_end("vpu-%d|ret:%d", vd->id, ret);
	return ret;

}

static int vpu_set_ftrace(struct vpu_device *vd)
{
	int ret = 0;

	/* set ftrace */
	vpu_reg_write(vd, XTENSA_INFO01, VPU_CMD_SET_FTRACE_LOG);
	vpu_reg_write(vd, XTENSA_INFO05, (vpu_drv->met) ? 1 : 0);
	/* set vpu internal log level */
	vpu_reg_write(vd, XTENSA_INFO06, vpu_drv->ilog);
	/* clear info18 */
	vpu_reg_write(vd, XTENSA_INFO18, 0);
	vpu_cmd(vd);
	ret = wait_command(vd);
	vpu_stall(vd);

	if (ret == -ERESTARTSYS)
		goto out;

	/* Error handling */
	if (ret) {
		pr_info("%s: vpu%d: SET_FTRACE timeout: info00: 0x%x, ret: %d\n",
			__func__, vd->id,
			vpu_reg_read(vd, XTENSA_INFO00),
			ret);
		vpu_aee_excp(vd, NULL, "VPU Timeout",
			"vpu%d: request (SET_FTRACE) timeout\n", vd->id);
		goto out;
	}
	ret = vpu_check_postcond(vd);

out:
	if (ret)
		pr_info("%s: vpu%d: fail to set ftrace\n", __func__, vd->id);

	return ret;
}


