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

#include "vpu_hw.h"

#include <linux/slab.h>
#include <linux/interrupt.h>
#include "mtk_devinfo.h"
#include "vpu_algo.h"
#include "vpu_mem.h"

// TODO: group power related functions
#include <linux/regulator/consumer.h>
// #include "mtk_qos_bound.h"
#include <linux/pm_qos.h>
// #include "vpu_power_ctl.h"  // TODO: remove power 1.0  header
#include "vpu_debug.h"

static int vpu_check_precond(struct vpu_device *dev);
static int vpu_check_postcond(struct vpu_device *dev);

/* command wait timeout (ms) */
#define CMD_WAIT_TIME_MS    (3 * 1000)

/* 20180703, 00:00: vpu log mechanism */
// TODO: update device firmware version
#define HOST_VERSION	(0x18070300)

static void vpu_run(struct vpu_device *dev)
{
	dev->cmd_done = false;
	vpu_write_field(dev, FLD_RUN_STALL, 0);
	vpu_write_field(dev, FLD_CTL_INT, 1);
}

static void vpu_stall(struct vpu_device *dev)
{
	vpu_write_field(dev, FLD_RUN_STALL, 1);
}

#define WAIT_COMMAND_RETRY 5

static inline int wait_command(struct vpu_device *dev)
{
	int ret = 0;
	bool retry = true;

	unsigned int PWAITMODE = 0;
	unsigned int count = 0;

start:
	ret = wait_event_interruptible_timeout(
		dev->cmd_wait,
		dev->cmd_done,
		msecs_to_jiffies(CMD_WAIT_TIME_MS));

	if (ret == -ERESTARTSYS) {
		pr_info("%s: vpu%d: interrupt by signal: ret=%d\n",
			__func__, dev->id, ret);

		if (retry) {
			pr_info("%s: vpu%d: try wait again\n",
				__func__, dev->id);
			retry = false;
			goto start;
		}
		goto out;
	}


	/* command success */
	if (ret) {
		vpu_cmd_debug("%s: vpu%d: command done\n",
			__func__, dev->id);  // debug
		ret = 0;
		goto out;
	}

	/* timeout handling */
	ret = -ETIMEDOUT;

	/* check PWAITMODE, request by DE */
	do {
		PWAITMODE = vpu_read_field(dev, FLD_PWAITMODE);
		count++;
		if (PWAITMODE & 0x1) {
			ret = 0;
			pr_info("%s: vpu%d: PWAITMODE: %d\n",
				__func__, dev->id, PWAITMODE);
			break;
		}

		pr_info("%s: vpu%d: PWAITMODE: %d, info25: %x\n",
			__func__, dev->id, PWAITMODE,
			vpu_read_field(dev, FLD_XTENSA_INFO25));
		//	vpu_dump_register(NULL);
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

/* called by vpu_probe() */
int vpu_init_dev_algo(struct platform_device *pdev, struct vpu_device *dev)
{
	// for each alog in the image header
	// add to the device
	//	unsigned int coreMagicNum;
	int i, j;
	int ret = 0;
	unsigned int mva;
	struct vpu_algo_info *algo_info;
	struct vpu_image_header *header = bin_header();

	dev->algo_curr = NULL;

	// reference: vpu_get_entry_of_algo()
	/* for each algo in the image header, add them to device's algo list */
	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		for (j = 0; j < header[i].algo_info_count; j++) {
			struct __vpu_algo *alg;

			algo_info = &header[i].algo_infos[j];
			mva = algo_info->offset - VPU_OFFSET_ALGO_AREA +
				vpu_drv->mva_algo;

			/* skips, if the core mask mismatch */
			if (!((algo_info->vpu_core & 0xF) & (1 << dev->id)))
				continue;

			vpu_drv_debug("%s: vpu%d(%xh): %s: off: %x, mva: %x, len: %x\n",
				__func__,
				dev->id,
				algo_info->vpu_core,
				algo_info->name,
				algo_info->offset,
				mva,
				algo_info->length);

			alg = vpu_alg_alloc();
			if (!alg) {
				ret = -ENOMEM;
				goto out;
			}

			strncpy(alg->a.name, algo_info->name, ALGO_NAMELEN);
			alg->a.mva = mva;
			alg->a.len = algo_info->length;

			list_add_tail(&alg->list, &dev->algo);
			dev->algo_cnt++;
		}
		vpu_drv_debug("%s: vpu%d, total algo count: %d\n",
			__func__, dev->id, dev->algo_cnt);
	}
out:
	return ret;
}

/* called by vpu_remove() */
void vpu_exit_dev_algo(struct platform_device *pdev, struct vpu_device *dev)
{
	struct __vpu_algo *alg, *tmp;

	vpu_alg_debug("%s: dev: %p, vpu%d, dev->algo: %p\n",
		__func__, dev, dev->id, dev->algo);

	list_for_each_entry_safe(alg, tmp, &dev->algo, list) {
		vpu_alg_debug("%s: dev: %p, vpu%d, dev->algo: %p, alg: %p\n",
			__func__, dev, dev->id, dev->algo, alg);
		vpu_alg_put(alg);
	}
}

irqreturn_t vpu_isr(int irq, void *dev_id)
{
	struct vpu_device *dev = (struct vpu_device *)dev_id;
	int req_cmd = 0, normal_check_done = 0;
	int req_dump = 0;
//	unsigned int apmcu_log_buf_ofst;
//	unsigned int log_buf_addr = 0x0;
//	unsigned int log_buf_size = 0x0;
//	struct vpu_log_reader_t *vpu_log_reader;
//	void *ptr;
	uint32_t val;

	// debug
	vpu_cmd_debug("%s: enter\n", __func__);

	/* INFO 17 was used to reply command done */
	req_cmd = vpu_read_field(dev, FLD_XTENSA_INFO17);

//	LOG_DBG("INFO17=0x%08x\n", req_cmd);
//	vpu_trace_dump("VPU%d_ISR_RECV|INFO17=0x%08x", dev->id, req_cmd);
	switch (req_cmd) {
	case 0:
		break;
	case VPU_REQ_DO_CHECK_STATE:
	default:
		if (vpu_check_postcond(dev) == -EBUSY) {
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

	/* INFO18 was used to dump MET Log */
	req_dump = vpu_read_field(dev, FLD_XTENSA_INFO18);

	// TODO: ADD MET FTRACE

	/* clear int */
	val = vpu_read_field(dev, FLD_APMCU_INT);

	if (val != 0) {
		vpu_cmd_debug("%s: vpu%d FLD_APMCU_INT = (%d)\n", dev->id,
			val);
	}
	vpu_write_field(dev, FLD_APMCU_INT, 1);

	/* wakeup waitqueue */
	if (normal_check_done == 1) {
		dev->cmd_done = true;
		wake_up_interruptible(&dev->cmd_wait);
	}

	// debug
	vpu_cmd_debug("%s: INFO00: %x, INFO17: %x, INFO18: %x, cmd_done: %d\n",
		__func__, vpu_read_field(dev, FLD_XTENSA_INFO00),
		req_cmd, req_dump, normal_check_done);

	return IRQ_HANDLED;
}

void vpu_error_handler(struct vpu_device *dev, struct vpu_request *req)
{
}

// dev->cmd_lock, should be acquired before calling this function
int vpu_execute_d2d(struct vpu_device *dev, struct vpu_request *req)
{
	int ret;

	/* profiling */
	struct timespec start, end;
	uint64_t latency = 0;

	// TODO: add met trace
//	vpu_trace_begin("[vpu_%d] hw_processing_request(%d)",
//				core, request->algo_id[core]);

	vpu_cmd_debug("%s: Enter\n", __func__);  // debug

	ret = vpu_check_precond(dev);
	if (ret) {
		req->status = VPU_REQ_STATUS_BUSY;
		vpu_cmd_debug("%s: device busy: %d\n", __func__, ret);
		goto out;
	}

	memcpy((void *) dev->work_buf->va,
			req->buffers,
			sizeof(struct vpu_buffer) * req->buffer_count);

	vpu_mem_flush(dev->work_buf);

	vpu_cmd_debug("%s: vpu%d: %s: bw: %d, buf_cnt: %x, sett: %x +%x\n",
		__func__, dev->id, dev->algo_curr->a.name,
		req->power_param.bw, req->buffer_count,
		req->sett_ptr, req->sett_length);

	/* 1. write register */
	/* command: d2d */
	vpu_write_field(dev, FLD_XTENSA_INFO01, VPU_CMD_DO_D2D);
	/* buffer count */
	vpu_write_field(dev, FLD_XTENSA_INFO12, req->buffer_count);
	/* pointer to array of struct vpu_buffer */
	vpu_write_field(dev, FLD_XTENSA_INFO13,	dev->work_buf->pa);
	/* pointer to property buffer */
	vpu_write_field(dev, FLD_XTENSA_INFO14, req->sett_ptr);
	/* size of property buffer */
	vpu_write_field(dev, FLD_XTENSA_INFO15, req->sett_length);

	/* 2. trigger interrupt */
	// TODO: add MET trace
//	vpu_trace_begin("[vpu_%d] dsp:d2d running", core);
//	LOG_DBG("[vpu_%d] d2d running...\n", core);
//	MET_Events_Trace(1, core, request->algo_id[core]);
	// TODO: add QOS
//	vpu_cmd_qos_start(dev->id);

	/* RUN_STALL pull down */
	vpu_run(dev);
	ktime_get_ts(&start);

	ret = wait_command(dev);
	if (ret == -ERESTARTSYS)
		goto err_cmd;

	/* Error handling */
	if (ret) {
		pr_info("%s: vpu%d: DO_D2D fail: info00: 0x%x, ret: %d\n",
			__func__, dev->id,
			vpu_read_field(dev, FLD_XTENSA_INFO00),
			ret);
		req->status = VPU_REQ_STATUS_TIMEOUT;
		// vvpu_vmdla_vcore_checker();
		vpu_error_handler(dev, req);
		goto err_cmd;
	}

	/* RUN_STALL pull up to avoid fake cmd */
	vpu_stall(dev);

	ktime_get_ts(&end);
	latency += (uint64_t)(timespec_to_ns(&end) -
		timespec_to_ns(&start));

	// TODO: add MET trace
	// vpu_trace_end();
	// MET_Events_Trace(0, core, request->algo_id[core]);

	req->status = (vpu_check_postcond(dev)) ?
			VPU_REQ_STATUS_FAILURE : VPU_REQ_STATUS_SUCCESS;

	req->busy_time = (uint64_t)latency;
	vpu_cmd_debug("%s: command done, result: %s\n", __func__,
		(req->status == VPU_REQ_STATUS_SUCCESS) ?
		"success" : "fail");  // debug

	// TODO: add QOS
//	req->bandwidth = vpu_cmd_qos_end(dev->id);

err_cmd:
	// TODO: add met trace
	//	vpu_trace_end();

out:
	return ret;
}

// reference: vpu_boot_up
// dev->cmd_lock, should be acquired before calling this function
int vpu_dev_boot(struct vpu_device *dev)
{
	int ret = 0;

	if (dev->state != VS_DOWN)
		return ret;

	dev->state = VS_BOOT;

#if 0
	/* Enable power and clock */
	ret = vpu_get_power(dev->id, false);
	if (ret) {
		pr_info("%s: vpu_get_power: %d\n", __func__, ret);
		goto err;
	}
#endif

	/* VPU boot up sequence */
	ret = vpu_dev_boot_sequence(dev);
	if (ret) {
		pr_info("%s: vpu_dev_boot_sequence: %d\n", __func__, ret);
		goto err;
	}

	/* VPU set debug log and MET trace */
	ret = vpu_dev_set_debug(dev);
	if (ret)
		pr_info("%s: vpu_dev_set_debug: %d\n", __func__, ret);

	dev->state = VS_IDLE;

err:
	return ret;
}

// reference: vpu_shut_down
// dev->cmd_lock, should be acquired before calling this function
int vpu_dev_down(struct vpu_device *dev)
{
	if ((dev->state > VS_DOWN) && (dev->state < VS_REMOVING))
		dev->state = VS_DOWN;

	return 0;
}

// Equal to vpu_hw_processing_request

// debug
static void vpu_dump_req(struct vpu_device *dev, struct vpu_request *req)
{
	int i, j;

	pr_info("%s: req: %p, algo: %s, bufcnt: %d, sett_len: %d\n",
		__func__, req, req->algo, req->buffer_count, req->sett_length);

	for (i = 0; i < req->buffer_count; i++) {
		struct vpu_buffer *b = &req->buffers[i];

		for (j = 0; j < b->plane_count; j++) {
			pr_info("%s: req: %p, buf%d, plane%d, size: %d, mva: 0x%lx\n",
				__func__, req, i, j, b->planes[j].length,
				(unsigned long)b->planes[j].ptr);
		}
	}
}


int vpu_execute(struct vpu_device *dev, struct vpu_request *req)
{
	int ret = 0;

	// TODO: Add preemption handling
	mutex_lock(&dev->cmd_lock);

	if (dev->state == VS_REMOVING) {
		pr_info("%s: vpu%d is going to be removed\n",
			__func__, dev->id);
		goto err_remove;
	}

	if (dev->state != VS_IDLE) {
		pr_info("%s: vpu%d state was not idle: %d\n",
			__func__, dev->id, dev->state);
	}
	vpu_cmd_debug("%s: vpu%d, req: %p\n", __func__, dev->id, req);  // debug

	vpu_dump_req(dev, req);

	/* Bootup VPU */
	ret = vpu_dev_boot(dev);
	if (ret)
		goto err_boot;

	/* Load Algorithm (DO_LOADER) */
	if (!VPU_REQ_FLAG_TST(req, ALG_RELOAD) &&
		dev->algo_curr &&
		(!strcmp(dev->algo_curr->a.name, req->algo)))
		goto send_req;

	dev->state = VS_CMD_ALG;
	ret = vpu_alg_load(dev, req->algo, NULL);
	if (ret)
		goto err_alg;

send_req:
	/* Send Request (DO_D2D) */
	dev->state = VS_CMD_D2D;
	ret = vpu_execute_d2d(dev, req);

err_alg:
	/* Check Results */
	if (ret) {
		// TODO: check power counter
		pr_info("%s: vpu%d: hw error, force shutdown\n",
				__func__, dev->id);
#if 0
		vpu_shut_down(dev->id);
#endif
	} else {
#if 0
		vpu_put_power(dev->id, VPT_ENQUE_ON);
#endif
	}

err_boot:
err_remove:
	dev->state = VS_IDLE;
	mutex_unlock(&dev->cmd_lock);

	return ret;
}

static int vpu_map_mva_of_bin(struct vpu_device *dev, uint64_t bin_pa)
{
	int ret = 0;
	int core = dev->id;

	uint32_t mva_reset_vector;
	uint32_t mva_main_program;

	uint32_t mva_algo_binary_data;
	uint32_t mva_iram_data;
	struct sg_table *sg;

	uint64_t binpa_reset_vector;
	uint64_t binpa_main_program;
	uint64_t binpa_iram_data;
	const uint64_t size_algos = VPU_SIZE_ALGO_AREA +
					  VPU_SIZE_ALGO_AREA +
					  VPU_SIZE_ALGO_AREA;

	vpu_drv_debug("%s, bin_pa(0x%lx)\n", __func__,
			(unsigned long)bin_pa);

	// TODO: get addresses from dts
	switch (core) {
	case 0:
	default:
		mva_reset_vector = VPU_MVA_RESET_VECTOR;
		mva_main_program = VPU_MVA_MAIN_PROGRAM;
		binpa_reset_vector = bin_pa;
		binpa_main_program = bin_pa + VPU_OFFSET_MAIN_PROGRAM;
		binpa_iram_data = bin_pa + VPU_OFFSET_MAIN_PROGRAM_IMEM;
		break;
	case 1:
		mva_reset_vector = VPU2_MVA_RESET_VECTOR;
		mva_main_program = VPU2_MVA_MAIN_PROGRAM;
		binpa_reset_vector = bin_pa +
						VPU_DDR_SHIFT_RESET_VECTOR;

		binpa_main_program = binpa_reset_vector +
						VPU_OFFSET_MAIN_PROGRAM;

		binpa_iram_data = bin_pa +
					VPU_OFFSET_MAIN_PROGRAM_IMEM +
					VPU_DDR_SHIFT_IRAM_DATA;
		break;
	}
	vpu_drv_debug("%s: vpu%d, res_vec: 0x%lx, main_prog: 0x%lx\n",
		__func__, dev->id,
		(unsigned long)binpa_reset_vector,
		(unsigned long)binpa_main_program);

	/* 1. map reset vector */
	sg = &dev->sg_reset_vector;
	ret = sg_alloc_table(sg, 1, GFP_KERNEL);
	if (ret) {
		pr_info("%s: vpu%d: fail to allocate sg_reset_vector\n",
			__func__, dev->id);
		goto out;
	}
	sg_dma_address(sg->sgl) = binpa_reset_vector;

	vpu_drv_debug("%s: vpu%d: sg_dma_address ok, bin_pa(0x%x)\n",
		__func__, dev->id, (unsigned int)binpa_reset_vector);

	sg_dma_len(sg->sgl) = VPU_SIZE_RESET_VECTOR;

	vpu_drv_debug("%s: vpu%d: sg_dma_len ok, VPU_SIZE_RESET_VECTOR(0x%x)\n",
		__func__, dev->id, VPU_SIZE_RESET_VECTOR);


	ret = vpu_mva_alloc(0, sg, VPU_SIZE_RESET_VECTOR,
		VPU_MVA_START_FROM, &mva_reset_vector);

	if (ret) {
		pr_info("%s: vpu%d: fail to allocate mva of reset vecter!\n",
			__func__, dev->id);
		goto out;
	}

	vpu_drv_debug("%s: vpu%d: vpu_mva_alloc: 0x%x\n",
		__func__, dev->id, mva_reset_vector);

	/* 2. map main program */
	sg = &(dev->sg_main_program);
	ret = sg_alloc_table(sg, 1, GFP_KERNEL);
	if (ret) {
		pr_info("%s: vpu%d: fail to allocate sg_main_program\n",
			__func__, dev->id);
		goto out;
	}
	vpu_drv_debug("%s: vpu%d: sg_alloc_table main_program ok\n",
		__func__, dev->id);

	sg_dma_address(sg->sgl) = binpa_main_program;
	sg_dma_len(sg->sgl) = VPU_SIZE_MAIN_PROGRAM;

	ret = vpu_mva_alloc(0, sg, VPU_SIZE_MAIN_PROGRAM,
		VPU_MVA_START_FROM, &mva_main_program);

	if (ret) {
		pr_info("%s: vpu%d: fail to allocate mva of main program\n",
			__func__, dev->id);

		vpu_mva_free(mva_main_program);
		goto out;
	}

	vpu_drv_debug("%s: vpu%d: m4u_alloc_mva main_program ok, (0x%x/0x%x)\n",
		__func__, dev->id,
		(unsigned int)(binpa_main_program),
		(unsigned int)VPU_SIZE_MAIN_PROGRAM);

	/* 3. map all algo binary data(src addr for dps to copy) */
	/* no need reserved mva, use SG_READY*/
	if (vpu_drv->mva_algo)
		goto skip_map_algo;

	sg = &(dev->sg_algo_binary_data);
	ret = sg_alloc_table(sg, 1, GFP_KERNEL);
	if (ret) {
		pr_info("%s: vpu%d: fail to allocate sg_algo_binary_data\n",
			__func__, dev->id);
		goto out;
	}

	vpu_drv_debug("%s: vpu%d: sg_alloc_table: mva_algo_binary_data: 0x%x\n",
		__func__, dev->id, mva_algo_binary_data);

	sg_dma_address(sg->sgl) = bin_pa + VPU_OFFSET_ALGO_AREA;
	sg_dma_len(sg->sgl) = size_algos;

	ret = vpu_mva_alloc(0, sg, size_algos,
		VPU_MVA_SG_READY, &mva_algo_binary_data);

	if (ret) {
		pr_info("%s: vpu%d: fail to allocate mva of reset vecter\n",
			__func__, dev->id);
		goto out;
	}

	vpu_drv->mva_algo = mva_algo_binary_data;

	vpu_drv_debug("%s: vpu%d: va_algo_data pa: 0x%x\n",
		__func__, dev->id,
		(unsigned int)(bin_pa + VPU_OFFSET_ALGO_AREA));

	vpu_drv_debug("%s: vpu%d: va_algo_data: 0x%x/0x%x, size: 0x%x\n",
		__func__, dev->id,
		mva_algo_binary_data,
		(unsigned int)vpu_drv->mva_algo,
		(unsigned int)size_algos);

skip_map_algo:
	/* 4. map main program iram data */
	/* no need reserved mva, use SG_READY*/
	sg = &(dev->sg_iram_data);
	ret = sg_alloc_table(sg, 1, GFP_KERNEL);
	if (ret) {
		pr_info("%s: vpu%d: fail to allocate sg table[reset]!\n",
			__func__, dev->id);
		goto out;
	}

	vpu_drv_debug("%s: vpu%d: sg_alloc_table iram_data ok, mva_iram_data = 0x%x\n",
		__func__, dev->id, mva_iram_data);

	vpu_drv_debug("%s: vpu%d: iram pa: 0x%lx\n",
		__func__, dev->id, (unsigned long)(binpa_iram_data));

	sg_dma_address(sg->sgl) = binpa_iram_data;
	sg_dma_len(sg->sgl) = VPU_SIZE_MAIN_PROGRAM_IMEM;

	ret = vpu_mva_alloc(0, sg, VPU_SIZE_MAIN_PROGRAM_IMEM,
		VPU_MVA_SG_READY, &mva_iram_data);

	if (ret) {
		pr_info("%s: vpu%d: fail to allocate mva of iram data!\n",
			__func__, dev->id);
		goto out;
	}

	dev->mva_iram = (uint64_t)(mva_iram_data);
	vpu_drv_debug("%s: vpu%d: va_iram_data: 0x%x, iram_data_mva: 0x%lx\n",
		__func__, dev->id,
		mva_iram_data, (unsigned long)dev->mva_iram);

out:
	return ret;
}

static bool vpu_is_disabled(struct platform_device *pdev,
	struct vpu_device *dev)
{
	unsigned int efuse;
	unsigned int mask;

	mask = 1 << dev->id;

	efuse = (get_devinfo_with_index(3) & 0xC00) >> 10;
	pr_info("%s: efuse_data: 0x%x, core%d is %s\n",
		__func__, efuse, dev->id,
		(efuse & mask) ? "disabled" : "enabled");  // debug

	return (efuse & mask);
}

/* driver hw init */
int vpu_init_drv_hw(void)
{
	int ret = 0;
	struct vpu_mem_param mem_param;

	/* initialize core shared memory */
	mem_param.require_pa = true;
	mem_param.require_va = true;
	mem_param.size = VPU_SIZE_SHARED_DATA;
	mem_param.fixed_addr = VPU_MVA_SHARED_DATA;
	ret = vpu_mem_alloc(&vpu_drv->share_data, &mem_param);

	vpu_drv_debug("%s: share_data: va:0x%llx, pa:0x%x\n",
		__func__, vpu_drv->share_data->va, vpu_drv->share_data->pa);

	if (ret) {
		pr_info("%s: fail to allocate share data!\n", __func__);
		ret = -ENOMEM;
	}
	return ret;
}

/* device hw init */
int vpu_init_dev_hw(struct platform_device *pdev, struct vpu_device *dev)
{
	struct vpu_mem_param mem_param;
	int ret = 0;

	ret = vpu_map_mva_of_bin(dev, vpu_drv->bin_pa);
	if (ret) {
		pr_info("%s: vpu%d: fail to map binary data!\n",
			__func__, dev->id);
		goto out;
	}

	ret = request_irq(dev->irq_num,	vpu_isr, dev->irq_level,
		dev->name, dev);

	if (ret) {
		pr_info("%s: %s: fail to request irq.\n", __func__, dev->name);
		goto out;
	}

	mutex_init(&dev->lock);
	mutex_init(&dev->cmd_lock);
	init_waitqueue_head(&dev->cmd_wait);
	dev->cmd_done = false;

	if (vpu_is_disabled(pdev, dev))
		dev->state = VS_DISALBED;
	else
		dev->state = VS_DOWN;

//	init_power_flag(dev->id);

	/* initialize working buffer */
	mem_param.require_pa = true;
	mem_param.require_va = true;
	mem_param.size = VPU_SIZE_WORK_BUF;
	mem_param.fixed_addr = 0;

	ret = vpu_mem_alloc(
		&dev->work_buf,
		&mem_param);

	vpu_drv_debug("%s: core(%d): work_buf: %p, va:0x%llx, pa:0x%x\n",
		__func__, dev->id, dev->work_buf,
		dev->work_buf->va, dev->work_buf->pa);

	if (ret) {
		pr_info("%s: core(%d):fail to allocate working buffer!\n",
			__func__, dev->id);
		ret = -ENOMEM;
		goto error_out;
	}

	mem_param.require_pa = true;
	mem_param.require_va = true;
	mem_param.size = VPU_SIZE_ALGO_KERNEL_LIB;
	// TODO: Get proper Kernel Lib Addr from device tree
	mem_param.fixed_addr =
		(dev->id == 0) ? VPU_MVA_KERNEL_LIB : VPU2_MVA_KERNEL_LIB;

	/* initialize kernel library */
	ret = vpu_mem_alloc(
		&dev->kernel_lib,
		&mem_param);

	vpu_drv_debug("%s: core(%d): kernel_lib: %p, va:0x%llx, pa:0x%x\n",
		__func__, dev->id, dev->kernel_lib,
		dev->kernel_lib->va, dev->kernel_lib->pa);

	if (ret) {
		pr_info("%s: core(%d):fail to allocate kernel library!\n",
			__func__, dev->id);
		ret = -ENOMEM;
		goto error_out;
	}

//	init_power_resource(dev);

out:
	return ret;

error_out:
	vpu_mem_free(&dev->work_buf);
	vpu_mem_free(&dev->kernel_lib);

	return ret;
}

/* driver hw exit function */
int vpu_exit_drv_hw(void)
{
	vpu_mem_free(&vpu_drv->share_data);

	return 0;
}


/* device hw exit function */
int vpu_exit_dev_hw(struct platform_device *pdev, struct vpu_device *dev)
{
	vpu_mem_free(&dev->work_buf);
	vpu_mem_free(&dev->kernel_lib);
//	uninit_power_resource();

	return 0;
}


static int vpu_check_precond(struct vpu_device *dev)
{
	uint32_t status;
	size_t i;

	/* wait 1 seconds, if not ready or busy */
	for (i = 0; i < 50; i++) {
		status = vpu_read_field(dev, FLD_XTENSA_INFO00);
		switch (status) {
		case VPU_STATE_READY:
		case VPU_STATE_IDLE:
		case VPU_STATE_ERROR:
			return 0;
		case VPU_STATE_NOT_READY:
		case VPU_STATE_BUSY:
			msleep(20);
			break;
		case VPU_STATE_TERMINATED:
			return -EBADFD;
		}
	}
	vpu_cmd_debug("%s: vpu%d still busy: %d, after wait 1 second.\n",
		__func__, dev->id, status);
	return -EBUSY;
}

static int vpu_check_postcond(struct vpu_device *dev)
{
	uint32_t status = vpu_read_field(dev, FLD_XTENSA_INFO00);

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

#if 0
int vpu_hw_enable_jtag(bool enabled)
{
	int ret = 0;
	int TEMP_CORE = 0;

	vpu_get_power(TEMP_CORE, false);

	vpu_write_field(TEMP_CORE, FLD_SPNIDEN, enabled);
	vpu_write_field(TEMP_CORE, FLD_SPIDEN, enabled);
	vpu_write_field(TEMP_CORE, FLD_NIDEN, enabled);
	vpu_write_field(TEMP_CORE, FLD_DBG_EN, enabled);

	vpu_put_power(TEMP_CORE, VPT_ENQUE_ON);
	return ret;
}
#endif

int vpu_dev_boot_sequence(struct vpu_device *dev)
{
	int ret;
	uint64_t ptr_ctrl;
	uint64_t ptr_reset;
	uint64_t ptr_axi_0;
	uint64_t ptr_axi_1;
	unsigned int reg_value = 0;
	bool is_hw_fail = true;
	uint64_t base;

//	vpu_trace_begin("%s", __func__);

	vpu_drv_debug("%s: vpu%d\n",
		__func__, dev->id);

	base = (uint64_t)dev->reg_base;

	ptr_ctrl = base +
				g_vpu_reg_descs[REG_CTRL].offset;

	ptr_reset = base +
				g_vpu_reg_descs[REG_SW_RST].offset;

	ptr_axi_0 = base +
				g_vpu_reg_descs[REG_AXI_DEFAULT0].offset;

	ptr_axi_1 = base +
				g_vpu_reg_descs[REG_AXI_DEFAULT1].offset;

	/* 1. write register */
	/* set specific address for reset vector in external boot */

	// TODO: set reset vector as a table
	switch (dev->id) {
	case 0:
		vpu_write_field(dev,
			FLD_CORE_XTENSA_ALTRESETVEC,
			VPU_MVA_RESET_VECTOR);
		break;
	case 1:
		vpu_write_field(dev,
			FLD_CORE_XTENSA_ALTRESETVEC,
			VPU2_MVA_RESET_VECTOR);
		break;
	default:
		pr_info("%s: undefined core: %d\n", __func__, dev->id);
		break;
	}

	reg_value = vpu_read_field(dev, FLD_CORE_XTENSA_ALTRESETVEC);
	vpu_drv_debug("%s: ALTRESETVEC: 0x%x\n", __func__, reg_value);

	VPU_SET_BIT(ptr_ctrl, 31); /* csr_p_debug_enable */
	VPU_SET_BIT(ptr_ctrl, 26); /* debug interface cock gated enable */
	VPU_SET_BIT(ptr_ctrl, 19); /* force to boot based on X_ALTRESETVEC */
	VPU_SET_BIT(ptr_ctrl, 23); /* RUN_STALL pull up */
	VPU_SET_BIT(ptr_ctrl, 17); /* pif gated enable */

	VPU_CLR_BIT(ptr_ctrl, 29);
	VPU_CLR_BIT(ptr_ctrl, 28);
	VPU_CLR_BIT(ptr_ctrl, 27);

	VPU_SET_BIT(ptr_reset, 12); /* OCD_HALT_ON_RST pull up */
	ndelay(27); /* wait for 27ns */

	VPU_CLR_BIT(ptr_reset, 12); /* OCD_HALT_ON_RST pull down */
	if (dev->id >= MTK_VPU_CORE) { /* set PRID */
		pr_info("%s: undefined core: %d\n", __func__, dev->id);
	} else {
		vpu_write_field(dev, FLD_PRID, dev->id);
	}
	VPU_SET_BIT(ptr_reset, 4); /* B_RST pull up */
	VPU_SET_BIT(ptr_reset, 8); /* D_RST pull up */
	ndelay(27); /* wait for 27ns */

	VPU_CLR_BIT(ptr_reset, 4); /* B_RST pull down */
	VPU_CLR_BIT(ptr_reset, 8); /* D_RST pull down */
	ndelay(27); /* wait for 27ns */

	/* pif gated disable, to prevent unknown propagate to BUS */
	VPU_CLR_BIT(ptr_ctrl, 17);
#ifndef BYPASS_M4U_DBG
	/*Setting for mt6779*/
	VPU_SET_BIT(ptr_axi_0, 21);
	VPU_SET_BIT(ptr_axi_0, 26);
	VPU_SET_BIT(ptr_axi_1, 3);
	VPU_SET_BIT(ptr_axi_1, 8);
#endif
	/* default set pre-ultra instead of ultra */
	VPU_SET_BIT(ptr_axi_0, 28);

	vpu_drv_debug("%s: vpu%d: REG_AXI_DEFAULT1: 0x%x\n",
		__func__, dev->id,
		vpu_read_reg32(base, CTRL_BASE_OFFSET + 0x140));

	/* 2. trigger to run */
	vpu_drv_debug("%s: vpu%d: sram config: 0x%x\n",
		__func__, dev->id,
		vpu_read_field(dev, FLD_SRAM_CONFIGURE));

//	vpu_trace_begin("dsp:running");

	/* RUN_STALL pull down */
	VPU_CLR_BIT(ptr_ctrl, 23);

	/* 3. wait until done */
	ret = wait_command(dev);
	if (ret == -ERESTARTSYS)
		is_hw_fail = false;

	ret = vpu_check_postcond(dev);

	/* RUN_STALL pull up to avoid fake cmd */
	VPU_SET_BIT(ptr_ctrl, 23);

//	vpu_trace_end();
	if (ret) {
		pr_info("%s: vpu%d: boot-up timeout, info00: %d, ret: %d\n",
			__func__, dev->id,
			vpu_read_field(dev, FLD_XTENSA_INFO00),
			ret);
		vpu_error_handler(dev, NULL);
	}

//	vpu_trace_end();
	vpu_write_field(dev, FLD_APMCU_INT, 1);

	return ret;
}

#if 0
// TODO: move to vpu_met.c
#ifdef MET_VPU_LOG
static int vpu_hw_set_log_option(int core)
{
	int ret;

	/* update log enable and mpu enable */
	lock_command(core);
	/* set vpu internal log enable,disable */
	if (g_func_mask & VFM_ROUTINE_PRT_SYSLOG) {
		vpu_write_field(core, FLD_XTENSA_INFO01,
			VPU_CMD_SET_FTRACE_LOG);
		vpu_write_field(core, FLD_XTENSA_INFO05, 1);
	} else {
		vpu_write_field(core, FLD_XTENSA_INFO01,
			VPU_CMD_SET_FTRACE_LOG);
		vpu_write_field(core, FLD_XTENSA_INFO05, 0);
	}
	/* set vpu internal log level */
	vpu_write_field(core, FLD_XTENSA_INFO06, g_vpu_internal_log_level);
	/* clear info18 */
	vpu_write_field(core, FLD_XTENSA_INFO18, 0);
	/* RUN_STALL pull down */
	vpu_run(dev);
	ret = wait_command(core);
	/* RUN_STALL pull up to avoid fake cmd */
	vpu_stall(dev);
	/* 4. check the result */
	ret = vpu_check_postcond(core);
	/*CHECK_RET("[vpu_%d]fail to set debug!\n", core);*/
	unlock_command(core);
	return ret;
}
#endif
#endif

// dev->cmd_lock, should be acquired before calling this function
int vpu_dev_set_debug(struct vpu_device *dev)
{
	int ret;
	struct timespec now;
	unsigned int device_version = 0x0;
	bool is_hw_fail = true;

	vpu_cmd_debug("%s: vpu%d\n", __func__, dev->id);
//	vpu_trace_begin("%s", __func__);

	/* 1. set debug */
	getnstimeofday(&now);
	vpu_write_field(dev, FLD_XTENSA_INFO01, VPU_CMD_SET_DEBUG);
	vpu_write_field(dev, FLD_XTENSA_INFO19,	dev->mva_iram);
	vpu_write_field(dev, FLD_XTENSA_INFO21,
					dev->work_buf->pa +	VPU_OFFSET_LOG);
	vpu_write_field(dev, FLD_XTENSA_INFO22, VPU_SIZE_LOG_BUF);
	vpu_write_field(dev, FLD_XTENSA_INFO23,
					now.tv_sec * 1000000 +
						now.tv_nsec / 1000);

	vpu_write_field(dev, FLD_XTENSA_INFO29, HOST_VERSION);

	vpu_cmd_debug("%s: vpu%d: log: %x, info01: %x, info23: %x\n",
		__func__, dev->id,
		dev->work_buf->pa +	VPU_OFFSET_LOG,
		vpu_read_field(dev, FLD_XTENSA_INFO01),
		vpu_read_field(dev, FLD_XTENSA_INFO23));

	/* 2. trigger interrupt */
	// vpu_trace_begin("dsp:running");
	vpu_run(dev);

	vpu_cmd_debug("%s: timestamp: %.2lu:%.2lu:%.2lu:%.6lu\n",
		__func__,
		(now.tv_sec / 3600) % (24),
		(now.tv_sec / 60) % (60),
		now.tv_sec % 60,
		now.tv_nsec / 1000);

	/* 3. wait until done */
	ret = wait_command(dev);

	if (ret)
		goto err;

	// TODO: do we need check post cond ?
	// ret = vpu_check_postcond(dev);

	/* RUN_STALL pull up to avoid fake cmd */
	vpu_stall(dev);
//	vpu_trace_end();

	/*3-additional. check vpu device/host version is matched or not*/
	device_version = vpu_read_field(dev, FLD_XTENSA_INFO20);

	if ((int)device_version < (int)HOST_VERSION) {
		pr_info("%s: vpu%d: incompatible ftrace version: dev: %s, host: %x\n",
			__func__, dev->id,
			vpu_read_field(dev, FLD_XTENSA_INFO20),
			vpu_read_field(dev, FLD_XTENSA_INFO29));
		dev->ftrace_avail = false;
	} else {
		dev->ftrace_avail = true;
	}

err:
	if (ret) {
		pr_info("%s: vpu%d: fail, status: %d, ret: %d\n",
			__func__, dev->id,
			vpu_read_field(dev, FLD_XTENSA_INFO00), ret);

		if (ret == -ERESTARTSYS)
			is_hw_fail = false;

		// vvpu_vmdla_vcore_checker();
		vpu_error_handler(dev, NULL);

		goto out;
	}

	/* 4. check the result */
	ret = vpu_check_postcond(dev);
	if (ret)
		pr_info("%s: vpu%d: fail to set debug\n", __func__, dev->id);

out:
#ifdef MET_VPU_LOG
	/* to set vpu ftrace log */
	// TODO: set MET device trace, move to vpu_met.c
	//	ret = vpu_hw_set_log_option(core);
#endif
//	vpu_trace_end();
	return ret;
}

// reference: vpu_hw_load_algo
// dev->cmd_lock, should be acquired before calling this function
int vpu_hw_alg_init(struct vpu_device *dev, struct __vpu_algo *algo)
{
	int ret;
	bool is_hw_fail = true;

	vpu_cmd_debug("%s: vpu%d\n", __func__, dev->id);

	ret = vpu_check_precond(dev);
	if (ret) {
		pr_info("%s: vpu%d: wrong status before do loader!\n",
			__func__, dev->id);
		goto out;
	}

	vpu_alg_debug("%s: vpu%d: algo mva/length (0x%lx/0x%x)\n",
		__func__, dev->id,
		(unsigned long)algo->a.mva, algo->a.len);

	/* 1. write register */
	vpu_write_field(dev, FLD_XTENSA_INFO01, VPU_CMD_DO_LOADER);

	/* binary data's address */
	vpu_write_field(dev, FLD_XTENSA_INFO12, algo->a.mva);

	/* binary data's length */
	vpu_write_field(dev, FLD_XTENSA_INFO13, algo->a.len);

	vpu_write_field(dev, FLD_XTENSA_INFO15,
		0 /* opps.dsp.values[opps.dsp.index] */);

	vpu_write_field(dev, FLD_XTENSA_INFO16,
		0 /*opps.ipu_if.values[opps.ipu_if.index]*/);

	/* 2. trigger interrupt */
	// TODO: add met trace
	//	vpu_trace_begin("dsp:running");

	/* RUN_STALL down */
	vpu_run(dev);

	/* 3. wait until done */
	ret = wait_command(dev);
	if (ret == -ERESTARTSYS)
		is_hw_fail = false;

	vpu_cmd_debug("%s: vpu%d: done\n", __func__, dev->id);

	/* RUN_STALL pull up to avoid fake cmd */
	vpu_stall(dev);

	// TODO: add met trace
	//	vpu_trace_end();
	if (ret) {
		pr_info("%s: vpu%d load algo timeout, status:%d, cmd_done: %d, ret: %d\n",
			__func__, dev->id,
			vpu_read_field(dev, FLD_XTENSA_INFO00),
			dev->cmd_done,
			ret); // debug
		// TODO: Error handling, vpu dump
		goto out;
	}


out:
	return ret;
}

// reference: vpu_hw_get_algo_info
// dev->cmd_lock, should be acquired before calling this function
int vpu_hw_alg_info(struct vpu_device *dev, struct __vpu_algo *alg)
{
	int ret = 0;
	int port_count = 0;
	int info_desc_count = 0;
	int sett_desc_count = 0;
	unsigned int ofs_ports, ofs_info, ofs_info_descs, ofs_sett_descs;
	int i;

//	vpu_trace_begin("%s(%d)", __func__, algo->id[dev->id]);
	ret = vpu_check_precond(dev);
	if (ret) {
		pr_info("%s: vpu%d: device is not ready\n",
			__func__, dev->id);
		goto out;
	}

	ofs_ports = 0;
	ofs_info = sizeof(((struct vpu_algo *)0)->ports);
	ofs_info_descs = ofs_info + alg->a.info.length;
	ofs_sett_descs = ofs_info_descs +
			sizeof(((struct vpu_algo *)0)->info.desc);

	vpu_alg_debug("%s: vpu%d: check precond done\n",
		__func__, dev->id);

	/* 1. write register */
	vpu_write_field(dev, FLD_XTENSA_INFO01, VPU_CMD_GET_ALGO);
	vpu_write_field(dev, FLD_XTENSA_INFO06,
			dev->work_buf->pa + ofs_ports);
	vpu_write_field(dev, FLD_XTENSA_INFO07,
			dev->work_buf->pa + ofs_info);
	vpu_write_field(dev, FLD_XTENSA_INFO08, alg->a.info.length);
	vpu_write_field(dev, FLD_XTENSA_INFO10,
			dev->work_buf->pa + ofs_info_descs);
	vpu_write_field(dev, FLD_XTENSA_INFO12,
			dev->work_buf->pa + ofs_sett_descs);

	/* 2. trigger interrupt */
//	vpu_trace_begin("dsp:running");

	/* RUN_STALL pull down */
	vpu_run(dev);

	/* 3. wait until done */
	ret = wait_command(dev);

	vpu_cmd_debug("%s: vpu%d: VPU_CMD_GET_ALGO done\n", __func__, dev->id);
//	vpu_trace_end();
	if (ret) {
//		vpu_dump_mesg(NULL);
//		vpu_dump_register(NULL);
		if (ret != -ERESTARTSYS) {
//		vpu_dump_debug_stack(core, DEBUG_STACK_SIZE);
//		vpu_dump_code_segment(core);
			vpu_aee("VPU Timeout",
				"core_%d timeout to get algo, algo: %s\n",
				dev->id, dev->algo_curr->a.name);
		}
		goto out;
	}

	/* RUN_STALL pull up to avoid fake cmd */
	vpu_stall(dev);

	/* 4. get the return value */
	port_count = vpu_read_field(dev, FLD_XTENSA_INFO05);
	info_desc_count = vpu_read_field(dev, FLD_XTENSA_INFO09);
	sett_desc_count = vpu_read_field(dev, FLD_XTENSA_INFO11);
	alg->a.port_count = port_count;
	alg->a.info.desc_cnt = info_desc_count;
	alg->a.sett.desc_cnt = sett_desc_count;

	vpu_alg_debug("%s: got algo: ports: %d, info: %d, sett: %d\n",
		__func__, port_count, info_desc_count, sett_desc_count);

	/* 5. write back data from working buffer */
	memcpy((void *)(uintptr_t)alg->a.ports,
		(void *)((uintptr_t)dev->work_buf->va +
		ofs_ports),
		sizeof(struct vpu_port) * port_count);

	for (i = 0 ; i < alg->a.port_count ; i++) {
		vpu_alg_debug("%s: port: %d, id: %d, name: %s, dir: %d, usage: %d\n",
			__func__, i, alg->a.ports[i].id, alg->a.ports[i].name,
			alg->a.ports[i].dir, alg->a.ports[i].usage);
	}

	memcpy((void *)(uintptr_t)alg->a.info.ptr,
		(void *)((uintptr_t)dev->work_buf->va + ofs_info),
		alg->a.info.length);

	memcpy((void *)(uintptr_t)alg->a.info.desc,
		(void *)((uintptr_t)dev->work_buf->va + ofs_info_descs),
		sizeof(struct vpu_prop_desc) * info_desc_count);

	memcpy((void *)(uintptr_t)alg->a.sett.desc,
		(void *)((uintptr_t)dev->work_buf->va +
		ofs_sett_descs),
		sizeof(struct vpu_prop_desc) * sett_desc_count);

	vpu_alg_debug("%s: ports: %d, info: %d, sett: %d\n",
		__func__, alg->a.port_count,
		alg->a.info.desc_cnt, alg->a.sett.desc_cnt);

out:
//	vpu_trace_end();
	return ret;

}


#if 0
int vpu_dump_buffer_mva(struct vpu_request *request)
{
	struct vpu_buffer *buf;
	struct vpu_plane *plane;
	int i, j;

	LOG_INF("dump request - setting: 0x%x, length: %d\n",
			(uint32_t) request->sett_ptr, request->sett_length);

	for (i = 0; i < request->buffer_count; i++) {
		buf = &request->buffers[i];
		LOG_INF("  buffer[%d] - %s:%d, %s:%dx%d, %s:%d\n",
				i,
				"port", buf->port_id,
				"size", buf->width, buf->height,
				"format", buf->format);

		for (j = 0; j < buf->plane_count; j++) {
			plane = &buf->planes[j];
			LOG_INF("	 plane[%d] - %s:0x%x, %s:%d, %s:%d\n",
				j,
				"ptr", (uint32_t) plane->ptr,
				"length", plane->length,
				"stride", plane->stride);
		}
	}

	return 0;

}
int vpu_dump_vpu_memory(struct seq_file *s)
{
	int i = 0;
	int core = 0; // temp
	unsigned int size = 0x0;
	unsigned long addr = 0x0;
	unsigned int dump_addr = 0x0;
	unsigned int value_1, value_2, value_3, value_4;
	unsigned int bin_offset = 0x0;
	//unsigned int vpu_domain_addr = 0x0;
	//unsigned int vpu_dump_size = 0x0;
	//unsigned int vpu_shift_offset = 0x0;

	vpu_print_seq(s, "===%s, vpu_dump_exception = 0x%x===\n",
		__func__, vpu_dump_exception);

	#define VPU_EXCEPTION_MAGIC 0x52310000

	if ((vpu_dump_exception & 0xFFFF0000) != VPU_EXCEPTION_MAGIC)
		return 0;

	core = vpu_dump_exception & 0x000000FF;

	vpu_print_seq(s, "==========%s, core_%d===========\n", __func__, core);

	vpu_print_seq(s, "=====core service state=%d=====\n",
		vpu_service_cores[core].state);

	if (core >= MTK_VPU_CORE) {
		vpu_print_seq(s, "vpu_dump_exception data error...\n");
		return 0;
	}


#if 0
	vpu_print_seq(s, "[vpu_%d] hw_d2d err, status(%d/%d), %d\n",
		core,
		vpu_read_field(core, FLD_XTENSA_INFO00),
		vpu_service_cores[core].is_cmd_done,
		exception_isr_check[core]);

	vpu_dump_register(s);
	vpu_dump_mesg(s);
#else
	vpu_print_seq(s, "======== dump message=======\n");

	vpu_dump_mesg(s);

	vpu_print_seq(s, "========no dump register=======\n");
#endif

#if 0
	vpu_print_seq(s, " ========== stack segment dump start ==========\n");

	/* dmem 0 */
	vpu_domain_addr = 0x7FF00000;
	vpu_shift_offset = 0x0;
	vpu_dump_size = 0x20000;
	vpu_print_seq(s, "==========dmem0 => 0x%x/0x%x: 0x%x==============\n",
		vpu_domain_addr, vpu_shift_offset, vpu_dump_size);
	for (i = 0 ; i < (int)vpu_dump_size / 4 ; i = i + 4) {
		vpu_print_seq(s, "%08X %08X %08X %08X %08X\n", vpu_domain_addr,
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 4)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 8)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 12)));
		vpu_domain_addr += (4 * 4);
	}
	/* dmem 1 */
	vpu_domain_addr = 0x7FF20000;
	vpu_shift_offset = 0x20000;
	vpu_dump_size = 0x20000;
	vpu_print_seq(s, "==========dmem1 => 0x%x/0x%x: 0x%x==============\n",
		vpu_domain_addr, vpu_shift_offset, vpu_dump_size);
	for (i = 0 ; i < (int)vpu_dump_size / 4 ; i = i + 4) {
		vpu_print_seq(s, "%08X %08X %08X %08X %08X\n", vpu_domain_addr,
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 4)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 8)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 12)));
		vpu_domain_addr += (4 * 4);
	}
	/* imem */
	vpu_domain_addr = 0x7FF40000;
	vpu_shift_offset = 0x40000;
	vpu_dump_size = 0x10000;
	vpu_print_seq(s, "==========imem => 0x%x/0x%x: 0x%x==============\n",
		vpu_domain_addr, vpu_shift_offset, vpu_dump_size);
	for (i = 0 ; i < (int)vpu_dump_size / 4 ; i = i + 4) {
		vpu_print_seq(s, "%08X %08X %08X %08X %08X\n", vpu_domain_addr,
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 4)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 8)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 12)));
		vpu_domain_addr += (4 * 4);
	}
#else
	vpu_print_seq(s, " ========== no dump stack segment ==========\n");

#endif
	vpu_print_seq(s, "\n\n\n===code segment dump start===\n\n\n");

	vpu_print_seq(s, "\n\n\n==main code segment_reset_vector==\n\n\n\n");

	vpu_print_seq(s, "|Clock index %d|\n", opps.dspcore[0].index);

	switch (core) {
	case 0:
	default:
		dump_addr = VPU_MVA_RESET_VECTOR;
		bin_offset = 0x0;
		break;
	case 1:
		dump_addr = VPU2_MVA_RESET_VECTOR;
		bin_offset = VPU_DDR_SHIFT_RESET_VECTOR;
		break;
	}

	size = DEBUG_MAIN_CODE_SEG_SIZE_1; /* define by mon/jackie*/
	vpu_print_seq(s, "==============0x%x/0x%x/0x%x/0x%x==============\n",
			bin_offset, dump_addr,
			size, VPU_SIZE_RESET_VECTOR);
	for (i = 0 ; i < (int)size / 4 ; i = i + 4) {
		value_1 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i));
		value_2 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i + 4));
		value_3 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i + 8));
		value_4 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i + 12));
		vpu_print_seq(s, "%08X %08X %08X %08X %08X\n", dump_addr,
			value_1, value_2, value_3, value_4);
		dump_addr += (4 * 4);
	}


	vpu_print_seq(s, "===main code segment_main_program===\n");
	switch (core) {
	case 0:
	default:
		dump_addr = VPU_MVA_MAIN_PROGRAM;
		bin_offset = VPU_OFFSET_MAIN_PROGRAM;
		break;
	case 1:
		dump_addr = VPU2_MVA_MAIN_PROGRAM;
		bin_offset = VPU_DDR_SHIFT_RESET_VECTOR +
					VPU_OFFSET_MAIN_PROGRAM;
		break;
	}
	size = DEBUG_MAIN_CODE_SEG_SIZE_2; /* define by mon/jackie*/
	vpu_print_seq(s, "==============0x%x/0x%x/0x%x/0x%x==============\n",
			bin_offset, dump_addr,
			size, VPU_SIZE_MAIN_PROGRAM);
	for (i = 0 ; i < (int)size / 4 ; i = i + 4) {
		value_1 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i));
		value_2 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i + 4));
		value_3 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i + 8));
		value_4 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i + 12));
		vpu_print_seq(s, "%08X %08X %08X %08X %08X\n", dump_addr,
			value_1, value_2, value_3, value_4);
		dump_addr += (4 * 4);
	}

	vpu_print_seq(s, "============== kernel code segment ==============\n");
	switch (core) {
	case 0:
	default:
		dump_addr = VPU_MVA_KERNEL_LIB;
		break;
	case 1:
		dump_addr = VPU2_MVA_KERNEL_LIB;
		break;
	}
	addr = (unsigned long)(vpu_service_cores[core].exec_kernel_lib->va);
	size = DEBUG_CODE_SEG_SIZE; /* define by mon/jackie*/
	vpu_print_seq(s, "==============0x%lx/0x%x/0x%x/0x%x==============\n",
			addr, dump_addr,
			size, DEBUG_CODE_SEG_SIZE);
	for (i = 0 ; i < (int)size / 4 ; i = i + 4) {
		value_1 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i))));

		value_2 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i + 4))));

		value_3 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i + 8))));

		value_4 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i + 12))));

		vpu_print_seq(s, "%08X %08X %08X %08X %08X\n", dump_addr,
			value_1, value_2, value_3, value_4);

		dump_addr += (4 * 4);
	}

	return 0;

}

int vpu_dump_register(struct seq_file *s)
{
	int i, j;
	bool first_row_of_field;
	struct vpu_reg_desc *reg;
	struct vpu_reg_field_desc *field;
	int TEMP_CORE = 0;
	unsigned int vpu_dump_addr = 0x0;

#define LINE_BAR "  +---------------+-------+---+---+-------------------------+----------+\n"

	 vpu_print_seq(s, "  |Core 0 clock index %d|\n", opps.dspcore[0].index);

	 vpu_print_seq(s, "  |Core 1 clock index %d|\n", opps.dspcore[1].index);

	for (TEMP_CORE = 0; TEMP_CORE < MTK_VPU_CORE; TEMP_CORE++) {
		vpu_print_seq(s, "  |Core: %-62d|\n", TEMP_CORE);
		vpu_print_seq(s, LINE_BAR);
		vpu_print_seq(s, "  |%-15s|%-7s|%-3s|%-3s|%-25s|%-10s|\n",
			"Register", "Offset", "MSB", "LSB", "Field", "Value");
		vpu_print_seq(s, LINE_BAR);


		for (i = 0; i < VPU_NUM_REGS; i++) {
			reg = &g_vpu_reg_descs[i];
#if 0
			if (reg->reg < REG_DEBUG_INFO00)
				continue;
#endif
			first_row_of_field = true;

			for (j = 0; j < VPU_NUM_REG_FIELDS; j++) {
				field = &g_vpu_reg_field_descs[j];
				if (reg->reg != field->reg)
					continue;

				if (first_row_of_field) {
					first_row_of_field = false;
#define PRINT_STRING "  |%-15s|0x%-5.5x|%-3d|%-3d|%-25s|0x%-8.8x|\n"
					vpu_print_seq(s, PRINT_STRING,
						  reg->name,
						  reg->offset,
						  field->msb,
						  field->lsb,
						  field->name,
						  vpu_read_field(TEMP_CORE, j));
#undef PRINT_STRING

				} else {
#define PRINT_STRING "  |%-15s|%-7s|%-3d|%-3d|%-25s|0x%-8.8x|\n"
					vpu_print_seq(s, PRINT_STRING,
						  "", "",
						  field->msb,
						  field->lsb,
						  field->name,
						  vpu_read_field(TEMP_CORE, j));
#undef PRINT_STRING

				}
			}
			vpu_print_seq(s, LINE_BAR);
		}
	}

	vpu_print_seq(s, LINE_BAR);
	vpu_print_seq(s, LINE_BAR);

	/* ipu_conn */
	/* 19000000: 0x0 ~ 0x30*/
	vpu_dump_addr = 0x19000000;
	for (i = 0 ; i < (int)(0x200) / 4 ; i = i + 4) {
		LOG_WRN("%08X %08X %08X %08X %08X\n", vpu_dump_addr,
			vpu_read_reg32(vpu_dev->vpu_syscfg_base, (4 * i)),
			vpu_read_reg32(vpu_dev->vpu_syscfg_base, (4 * i + 4)),
			vpu_read_reg32(vpu_dev->vpu_syscfg_base, (4 * i + 8)),
			vpu_read_reg32(vpu_dev->vpu_syscfg_base, (4 * i + 12)));
		vpu_dump_addr += (4 * 4);
	}

	/* 19000800~1900080c */
	vpu_dump_addr = 0x19000800;
	LOG_WRN("%08X %08X %08X %08X %08X\n", vpu_dump_addr,
		vpu_read_reg32(vpu_dev->vpu_syscfg_base, 0x800),
		vpu_read_reg32(vpu_dev->vpu_syscfg_base, 0x804),
		vpu_read_reg32(vpu_dev->vpu_syscfg_base, 0x808),
		vpu_read_reg32(vpu_dev->vpu_syscfg_base, 0x80C));
	/* 19000C00*/
	vpu_dump_addr = 0x19000C00;
	LOG_WRN("%08X %08X\n", vpu_dump_addr,
		vpu_read_reg32(vpu_dev->vpu_syscfg_base, 0xC00));


	/* 0x19020048/0x1902006c / 0x19020070 */
	vpu_dump_addr = 0x19020000;

	for (i = 0 ; i < (int)(0x200) / 4 ; i = i + 4) {
		LOG_WRN("%08X %08X %08X %08X %08X\n", vpu_dump_addr,
		vpu_read_reg32(vpu_dev->vpu_vcorecfg_base, (4 * i)),
		vpu_read_reg32(vpu_dev->vpu_vcorecfg_base, (4 * i + 4)),
		vpu_read_reg32(vpu_dev->vpu_vcorecfg_base, (4 * i + 8)),
		vpu_read_reg32(vpu_dev->vpu_vcorecfg_base, (4 * i + 12)));
		vpu_dump_addr += (4 * 4);
	}

	vpu_print_seq(s, LINE_BAR);

	for (TEMP_CORE = 0; TEMP_CORE < MTK_VPU_CORE; TEMP_CORE++) {
		vpu_print_seq(s, LINE_BAR);
		/* ipu_cores */
		if (TEMP_CORE == 0)
			vpu_dump_addr = 0x19180000;
		else
			vpu_dump_addr = 0x19280000;

		for (i = 0 ; i < (int)(0x20C) / 4 ; i = i + 4) {
			LOG_WRN("%08X %08X %08X %08X %08X\n", vpu_dump_addr,
			vpu_read_reg32(vpu_service_cores[TEMP_CORE].vpu_base,
					CTRL_BASE_OFFSET + (4 * i)),
			vpu_read_reg32(vpu_service_cores[TEMP_CORE].vpu_base,
					CTRL_BASE_OFFSET + (4 * i + 4)),
			vpu_read_reg32(vpu_service_cores[TEMP_CORE].vpu_base,
					CTRL_BASE_OFFSET + (4 * i + 8)),
			vpu_read_reg32(vpu_service_cores[TEMP_CORE].vpu_base,
					CTRL_BASE_OFFSET + (4 * i + 12)));
			vpu_dump_addr += (4 * 4);
		}
	}

	m4u_dump_reg_for_vpu_hang_issue();

#undef LINE_BAR

	return 0;
}

int vpu_dump_image_file(struct seq_file *s)
{
	int i, j, id = 1;
	struct vpu_algo_info *algo_info;
	struct vpu_image_header *header;
	int core = 0;

#define LINE_BAR "  +------+-----+--------------------------------+--------\n"
	vpu_print_seq(s, LINE_BAR);
	vpu_print_seq(s, "  |%-6s|%-5s|%-32s|%-8s|%-11s|%-10s|\n",
			"Header", "Id", "Name", "MagicNum", "MVA", "Length");
	vpu_print_seq(s, LINE_BAR);

	header = (struct vpu_image_header *)
		((uintptr_t)vpu_service_cores[core].bin_base +
					(VPU_OFFSET_IMAGE_HEADERS));
	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		for (j = 0; j < header[i].algo_info_count; j++) {
			algo_info = &header[i].algo_infos[j];

			vpu_print_seq(s,
			   "  |%-6d|%-5d|%-32s|0x%-6lx|0x%-9lx|0x%-8x|\n",
			   (i + 1),
			   id,
			   algo_info->name,
			   (unsigned long)(algo_info->vpu_core),
			   algo_info->offset - VPU_OFFSET_ALGO_AREA +
			      (uintptr_t)vpu_service_cores[core].algo_data_mva,
			   algo_info->length);

			id++;
		}
	}

	vpu_print_seq(s, LINE_BAR);
#undef LINE_BAR

/* #ifdef MTK_VPU_DUMP_BINARY */
#if 0
	{
		uint32_t dump_1k_size = (0x00000400);
		unsigned char *ptr = NULL;

		vpu_print_seq(s, "Reset Vector Data:\n");
		ptr = (unsigned char *) vpu_service_cores[core].bin_base +
				VPU_OFFSET_RESET_VECTOR;
		for (i = 0; i < dump_1k_size / 2; i++, ptr++) {
			if (i % 16 == 0)
				vpu_print_seq(s, "\n%07X0h: ", i / 16);

			vpu_print_seq(s, "%02X ", *ptr);
		}
		vpu_print_seq(s, "\n");
		vpu_print_seq(s, "\n");
		vpu_print_seq(s, "Main Program Data:\n");
		ptr = (unsigned char *) vpu_service_cores[core].bin_base +
				VPU_OFFSET_MAIN_PROGRAM;
		for (i = 0; i < dump_1k_size; i++, ptr++) {
			if (i % 16 == 0)
				vpu_print_seq(s, "\n%07X0h: ", i / 16);

			vpu_print_seq(s, "%02X ", *ptr);
		}
		vpu_print_seq(s, "\n");
	}
#endif

	return 0;
}

void vpu_dump_debug_stack(int core, int size)
{
	int i = 0;
	unsigned int vpu_domain_addr = 0x0;
	unsigned int vpu_dump_size = 0x0;
	unsigned int vpu_shift_offset = 0x0;

	vpu_domain_addr = ((DEBUG_STACK_BASE_OFFSET & 0x000fffff) | 0x7FF00000);

	LOG_ERR("===========%s, core_%d============\n", __func__, core);
	/* dmem 0 */
	vpu_domain_addr = 0x7FF00000;
	vpu_shift_offset = 0x0;
	vpu_dump_size = 0x20000;
	LOG_WRN("==========dmem0 => 0x%x/0x%x: 0x%x==============\n",
		vpu_domain_addr, vpu_shift_offset, vpu_dump_size);
	for (i = 0 ; i < (int)vpu_dump_size / 4 ; i = i + 4) {
		LOG_WRN("%08X %08X %08X %08X %08X\n", vpu_domain_addr,
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 4)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 8)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 12)));
		vpu_domain_addr += (4 * 4);
		mdelay(1);
	}
	/* dmem 1 */
	vpu_domain_addr = 0x7FF20000;
	vpu_shift_offset = 0x20000;
	vpu_dump_size = 0x20000;
	LOG_WRN("==========dmem1 => 0x%x/0x%x: 0x%x==============\n",
		vpu_domain_addr, vpu_shift_offset, vpu_dump_size);
	for (i = 0 ; i < (int)vpu_dump_size / 4 ; i = i + 4) {
		LOG_WRN("%08X %08X %08X %08X %08X\n", vpu_domain_addr,
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 4)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 8)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 12)));
		vpu_domain_addr += (4 * 4);
		mdelay(1);
	}
	/* imem */
	vpu_domain_addr = 0x7FF40000;
	vpu_shift_offset = 0x40000;
	vpu_dump_size = 0x10000;
	LOG_WRN("==========imem => 0x%x/0x%x: 0x%x==============\n",
		vpu_domain_addr, vpu_shift_offset, vpu_dump_size);
	for (i = 0 ; i < (int)vpu_dump_size / 4 ; i = i + 4) {
		LOG_WRN("%08X %08X %08X %08X %08X\n", vpu_domain_addr,
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 4)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 8)),
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
			vpu_shift_offset + (4 * i + 12)));
		vpu_domain_addr += (4 * 4);
		mdelay(1);
	}
}

void vpu_dump_code_segment(int core)
{
	/*Remove dump code segment to db*/
#ifdef VPU_DUMP_MEM_LOG
	int i = 0;
	unsigned int size = 0x0;
	unsigned long addr = 0x0;
	unsigned int dump_addr = 0x0;
	unsigned int value_1, value_2, value_3, value_4;
	unsigned int bin_offset = 0x0;

	vpu_dump_exception = VPU_EXCEPTION_MAGIC | core;

	LOG_ERR("==========%s, core_%d===========\n", __func__, core);
	LOG_ERR("==========main code segment_reset_vector===========\n");
	switch (core) {
	case 0:
	default:
		dump_addr = VPU_MVA_RESET_VECTOR;
		bin_offset = 0x0;
		break;
	case 1:
		dump_addr = VPU2_MVA_RESET_VECTOR;
		bin_offset = VPU_DDR_SHIFT_RESET_VECTOR;
		break;
	}

	size = DEBUG_MAIN_CODE_SEG_SIZE_1; /* define by mon/jackie*/
	LOG_WRN("==============0x%x/0x%x/0x%x/0x%x==============\n",
			bin_offset, dump_addr,
			size, VPU_SIZE_RESET_VECTOR);
	for (i = 0 ; i < (int)size / 4 ; i = i + 4) {
		value_1 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i));
		value_2 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i + 4));
		value_3 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i + 8));
		value_4 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i + 12));
		LOG_WRN("%08X %08X %08X %08X %08X\n", dump_addr,
			value_1, value_2, value_3, value_4);
		dump_addr += (4 * 4);
		mdelay(1);
	}


	LOG_ERR("=========== main code segment_main_program ===========\n");
	switch (core) {
	case 0:
	default:
		dump_addr = VPU_MVA_MAIN_PROGRAM;
		bin_offset = VPU_OFFSET_MAIN_PROGRAM;
		break;
	case 1:
		dump_addr = VPU2_MVA_MAIN_PROGRAM;
		bin_offset = VPU_DDR_SHIFT_RESET_VECTOR +
					VPU_OFFSET_MAIN_PROGRAM;
		break;
	}
	size = DEBUG_MAIN_CODE_SEG_SIZE_2; /* define by mon/jackie*/
	LOG_WRN("==============0x%x/0x%x/0x%x/0x%x==============\n",
			bin_offset, dump_addr,
			size, VPU_SIZE_MAIN_PROGRAM);
	for (i = 0 ; i < (int)size / 4 ; i = i + 4) {
		value_1 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i));
		value_2 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i + 4));
		value_3 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i + 8));
		value_4 = vpu_read_reg32(vpu_service_cores[core].bin_base,
			bin_offset + (4 * i + 12));
		LOG_WRN("%08X %08X %08X %08X %08X\n", dump_addr,
			value_1, value_2, value_3, value_4);
		dump_addr += (4 * 4);
		mdelay(1);
	}

	LOG_ERR("============== kernel code segment ==============\n");
	switch (core) {
	case 0:
	default:
		dump_addr = VPU_MVA_KERNEL_LIB;
		break;
	case 1:
		dump_addr = VPU2_MVA_KERNEL_LIB;
		break;
	}
	addr = (unsigned long)(vpu_service_cores[core].exec_kernel_lib->va);
	size = DEBUG_CODE_SEG_SIZE; /* define by mon/jackie*/
	LOG_WRN("==============0x%lx/0x%x/0x%x/0x%x==============\n",
			addr, dump_addr,
			size, DEBUG_CODE_SEG_SIZE);
	for (i = 0 ; i < (int)size / 4 ; i = i + 4) {
		value_1 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i))));

		value_2 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i + 4))));

		value_3 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i + 8))));

		value_4 = (unsigned int)
			(*((unsigned long *)((uintptr_t)addr + (4 * i + 12))));

		LOG_WRN("%08X %08X %08X %08X %08X\n", dump_addr,
			value_1, value_2, value_3, value_4);

		dump_addr += (4 * 4);
		mdelay(1);
	}
#else
	vpu_dump_exception = VPU_EXCEPTION_MAGIC | core;

#endif
}

void vpu_dump_algo_segment(int core, int algo_id, int size)
{
/* we do not mapping out va(bin_ptr is mva) for algo bin file currently */
#if 1
	struct vpu_algo *algo = NULL;
	int ret = 0;

	LOG_WRN("==========%s, core_%d, id_%d===========\n",
	 __func__, core, algo_id);

	ret = vpu_find_algo_by_id(core, algo_id, &algo, NULL);
	if (ret) {
		LOG_ERR("%s can not find the algo, core=%d, id=%d\n",
			__func__, core, algo_id);
		return;
	}
	LOG_WRN("== algo name : %s ==\n", algo->name);

#endif
#if 0

	unsigned int addr = 0x0;
	unsigned int length = 0x0;
	int i = 0;

	addr = (unsigned int)algo->bin_ptr;
	length = (unsigned int)algo->bin_length;

	LOG_WRN("==============0x%x/0x%x/0x%x==============\n",
			addr, length, size);

	for (i = 0 ; i < (int)length / 4 ; i = i + 4) {
		LOG_WRN("%X %X %X %X %X\n", addr,
		(unsigned int)
			(*((unsigned int *)((uintptr_t)addr + (4 * i)))),
		(unsigned int)
			(*((unsigned int *)((uintptr_t)addr + (4 * i + 4)))),
		(unsigned int)
			(*((unsigned int *)((uintptr_t)addr + (4 * i + 8)))),
		(unsigned int)
			(*((unsigned int *)((uintptr_t)addr + (4 * i + 12)))));
		addr += (4 * 4);
	}
#endif
}

int vpu_dump_mesg(struct seq_file *s)
{
	char *ptr = NULL;
	char *log_head = NULL;
	char *log_buf;
	char *log_a_pos = NULL;
	int core_index = 0;
	bool jump_out = false;

	for (core_index = 0 ; core_index < MTK_VPU_CORE; core_index++) {
		log_buf = (char *)
			((uintptr_t)vpu_service_cores[core_index].work_buf->va +
						VPU_OFFSET_LOG);
	if (g_vpu_log_level > 8) {
		int i = 0;
		int line_pos = 0;
		char line_buffer[16 + 1] = {0};

		ptr = log_buf;
		vpu_print_seq(s, "VPU_%d Log Buffer:\n", core_index);
		for (i = 0; i < VPU_SIZE_LOG_BUF; i++, ptr++) {
			line_pos = i % 16;
			if (line_pos == 0)
				vpu_print_seq(s, "\n%07X0h: ", i / 16);

			line_buffer[line_pos] = isascii(*ptr) &&
						(isprint(*ptr) ? *ptr : '.');

			vpu_print_seq(s, "%02X ", *ptr);
			if (line_pos == 15)
				vpu_print_seq(s, " %s", line_buffer);

		}
		vpu_print_seq(s, "\n\n");
	}

	ptr = log_buf;
	log_head = log_buf;

	/* set the last byte to '\0' */
	#if 0
	*(ptr + VPU_SIZE_LOG_BUF - 1) = '\0';

	/* skip the header part */
	ptr += VPU_SIZE_LOG_HEADER;
	log_head = strchr(ptr, '\0') + 1;

	vpu_print_seq(s, "=== VPU_%d Log Buffer ===\n", core_index);
	vpu_print_seq(s, "vpu: print dsp log\n%s%s", log_head, ptr);
	#else
	vpu_print_seq(s, "=== VPU_%d Log Buffer ===\n", core_index);
	vpu_print_seq(s, "vpu: print dsp log (0x%x):\n",
					(unsigned int)(uintptr_t)log_buf);

    /* in case total log < VPU_SIZE_LOG_SHIFT and there's '\0' */
	*(log_head + VPU_SIZE_LOG_BUF - 1) = '\0';
	vpu_print_seq(s, "%s", ptr+VPU_SIZE_LOG_HEADER);

	ptr += VPU_SIZE_LOG_HEADER;
	log_head = ptr;

	jump_out = false;
	*(log_head + VPU_SIZE_LOG_DATA - 1) = '\n';
	do {
		if ((ptr + VPU_SIZE_LOG_SHIFT) >=
				(log_head + VPU_SIZE_LOG_DATA)) {

			/* last part of log buffer */
			*(log_head + VPU_SIZE_LOG_DATA - 1) = '\0';
			jump_out = true;
		} else {
			log_a_pos = strchr(ptr + VPU_SIZE_LOG_SHIFT, '\n');
			if (log_a_pos == NULL)
				break;
			*log_a_pos = '\0';
		}
		vpu_print_seq(s, "%s\n", ptr);
		ptr = log_a_pos + 1;

		/* incase log_a_pos is at end of string */
		if (ptr >= log_head + VPU_SIZE_LOG_DATA)
			break;

		mdelay(1);
	} while (!jump_out);

	#endif
	}
	return 0;
}

int vpu_dump_vpu(struct seq_file *s)
{
	int core;

#define LINE_BAR "  +-------------+------+-------+-------+-------+-------+\n"
	vpu_print_seq(s, LINE_BAR);
	vpu_print_seq(s, "  |%-12s|%-34s|\n",
			"Queue#", "Waiting");
	vpu_print_seq(s, LINE_BAR);

	mutex_lock(&vpu_dev->commonpool_mutex);
	vpu_print_seq(s, "  |%-12s|%-34d|\n",
			      "Common",
			      vpu_dev->commonpool_list_size);
	mutex_unlock(&vpu_dev->commonpool_mutex);

	for (core = 0 ; core < MTK_VPU_CORE; core++) {
		mutex_lock(&vpu_dev->servicepool_mutex[core]);
		vpu_print_seq(s, "  |Core %-7d|%-34d|\n",
				      core,
				      vpu_dev->servicepool_list_size[core]);
		mutex_unlock(&vpu_dev->servicepool_mutex[core]);
	}
	vpu_print_seq(s, "\n");

#undef LINE_BAR

	return 0;
}
#endif

