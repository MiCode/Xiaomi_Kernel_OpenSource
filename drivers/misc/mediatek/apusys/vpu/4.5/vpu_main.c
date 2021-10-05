// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>

/* internal headers */
#include "vpu_cfg.h"
#include "apusys_power.h"
#include "apusys_core.h"
#include "vpu_cfg.h"
#include "vpu_power.h"
#include "vpu_cmd.h"
#include "vpu_cmn.h"
#include "vpu_mem.h"
#include "vpu_algo.h"
#include "vpu_debug.h"
#include "vpu_trace.h"
#include "vpu_ioctl.h"
#include "vpu_met.h"
#include "vpu_tag.h"
#include "vpu_hw.h"

struct vpu_platform vpu_plat_mt6885;

static void vpu_exit_drv_plat(void);
static int vpu_suspend(struct vpu_device *vd);
static int vpu_resume(struct vpu_device *vd);

/* handle different user query */
static int vpu_ucmd_handle(struct vpu_device *vd,
			   struct apusys_usercmd_hnd *ucmd)
{
	int ret = 0;
	struct __vpu_algo *alg = NULL;
	struct list_head *ptr = NULL;
	struct list_head *tmp = NULL;
	struct vpu_uget_algo *palg_cmd = NULL;

	if (!ucmd->kva || !ucmd->size) {
		vpu_cmd_debug("%s: invalid argument(0x%llx/%u)\n",
			      __func__,
			      (uint64_t)ucmd->kva,
			      ucmd->size);
		return -EINVAL;
	}

	/* Handling VPU_UCMD_GET_ALGO */
	if (VPU_UCMD_CHECK(ucmd->kva, GET_ALGO)) {
		/* casting user cmd as struct vpu_uget_algo */
		palg_cmd = (struct vpu_uget_algo *)ucmd->kva;
		mutex_lock(&vpu_drv->lock);
		/* looping each vpu_device for this algo */
		list_for_each_safe(ptr, tmp, &vpu_drv->devs) {
			vd = list_entry(ptr, struct vpu_device, list);
			/* Normal Algo */
			alg = vd->aln.ops->get(&vd->aln, palg_cmd->name, NULL);
			if (alg) {
				vd->aln.ops->put(alg);
				goto next;
			}
			/* Preload Algo */
			alg = vd->alp.ops->get(&vd->alp, palg_cmd->name, NULL);
			if (alg) {
				vd->alp.ops->put(alg);
				goto next;
			}
			vpu_cmd_debug("%s: vpu%d: algo \"%s\" was not found\n",
				__func__, vd->id, palg_cmd->name);
			ret = -ENOENT;
next:
			if (ret)
				break;
		}
		mutex_unlock(&vpu_drv->lock);
	} else {
		vpu_cmd_debug("%s: unknown user cmd: 0x%x\n",
			      __func__, *(uint32_t *)ucmd->kva);
		ret = -EINVAL;
	}

	return ret;
}

static int vpu_req_check(struct apusys_cmd_handle *cmd)
{
	int ret = 0;
	uint64_t mask = 0;
	struct apusys_cmdbuf *cmd_buf;
	struct vpu_request *req;

	if (cmd->num_cmdbufs != VPU_CMD_BUF_NUM) {
		pr_info("%s: invalid number 0x%x of vpu request\n",
			__func__, cmd->num_cmdbufs);
		return -EINVAL;
	}

	cmd_buf = &cmd->cmdbufs[0];

	if (!cmd_buf->kva) {
		pr_info("%s: invalid kva %p of vpu request\n",
			__func__, cmd_buf->kva);
		return -EINVAL;
	}

	if (cmd_buf->size != sizeof(struct vpu_request)) {
		pr_info("%s: invalid size 0x%x of vpu request\n",
			__func__, cmd_buf->size);
		return -EINVAL;
	}

	req = (struct vpu_request *)cmd_buf->kva;

	mask = ~(VPU_REQ_F_ALG_RELOAD |
		VPU_REQ_F_ALG_CUSTOM |
		VPU_REQ_F_ALG_PRELOAD |
		VPU_REQ_F_PREEMPT_TEST);

	if (req->flags & mask) {
		pr_info("%s: invalid flags 0x%llx of vpu request\n",
			__func__, req->flags);
		return -EINVAL;
	}

	if (req->buffer_count > VPU_MAX_NUM_PORTS) {
		pr_info("%s: invalid buffer_count 0x%x of vpu request\n",
			__func__, req->buffer_count);
		return -EINVAL;
	}

	return ret;
}

int vpu_send_cmd_rt(int op, void *hnd, struct apusys_device *adev)
{
	int ret = 0;
	struct vpu_device *vd;
	struct vpu_request *req;
	struct apusys_cmd_handle *cmd;

	vd = (struct vpu_device *)adev->private;

	switch (op) {
	case APUSYS_CMD_POWERON:
	case APUSYS_CMD_POWERDOWN:
	case APUSYS_CMD_FIRMWARE:
	case APUSYS_CMD_USER:
		vpu_cmd_debug("%s: vpu%d: operation not allowed: %d\n",
			      __func__, vd->id, op);
		return -EACCES;
	case APUSYS_CMD_RESUME:
	case APUSYS_CMD_SUSPEND:
		return 0;
	case APUSYS_CMD_EXECUTE:
		cmd = (struct apusys_cmd_handle *)hnd;
		if (vpu_req_check(cmd))
			return -EINVAL;

		req = (struct vpu_request *)cmd->cmdbufs[0].kva;
		vpu_trace_begin("vpu_%d|%s|cmd execute kid: 0x%08llx",
			vd->id, __func__, cmd->kid);
		vpu_cmd_debug("%s: vpu%d: EXECUTE, kva: %p kid: 0x%llx subcmd_idx: 0x%x\n",
			__func__, vd->id, (unsigned long)cmd->cmdbufs[0].kva,
			cmd->kid, cmd->subcmd_idx);
		/* overwrite vpu_req->boost from apusys_cmd */
		req->power_param.boost_value = cmd->boost;
		VPU_REQ_FLAG_SET(req, ALG_PRELOAD);
		ret = vpu_preempt(vd, req);
		vpu_trace_end("vpu_%d|%s|cmd execute kid: 0x%08llx",
			vd->id, __func__, cmd->kid);
		/* report vpu_req exe time to apusy_cmd */
		cmd->ip_time = req->busy_time / 1000;
		return ret;
	default:
		vpu_cmd_debug("%s: vpu%d: unknown command: %d\n",
			      __func__, vd->id, op);
		break;
	}

	return -EINVAL;
}

/* interface to APUSYS */
int vpu_send_cmd(int op, void *hnd, struct apusys_device *adev)
{
	int ret = 0;
	struct vpu_device *vd;
	struct vpu_request *req;
	struct apusys_power_hnd *pw;
	struct apusys_usercmd_hnd *ucmd;
	struct apusys_cmd_handle *cmd;

	vd = (struct vpu_device *)adev->private;

	switch (op) {
	case APUSYS_CMD_POWERON:
		pw = (struct apusys_power_hnd *)hnd;
		vpu_cmd_debug("%s: vpu%d: POWERON, boost: %d, opp: %d, timeout: %d\n",
			      __func__, vd->id,
			      pw->boost_val, pw->opp, pw->timeout);
		return vpu_pwr_up(vd, pw->boost_val,
			(pw->timeout ? vd->pw_off_latency : 0));
	case APUSYS_CMD_POWERDOWN:
		vpu_cmd_debug("%s: vpu%d: POWERDOWN\n",
			      __func__, vd->id);
		vpu_pwr_down(vd);
		return 0;
	case APUSYS_CMD_RESUME:
		vpu_cmd_debug("%s: vpu%d: RESUME\n",
			      __func__, vd->id);
		return vpu_resume(vd);
	case APUSYS_CMD_SUSPEND:
		vpu_cmd_debug("%s: vpu%d: SUSPEND\n",
			      __func__, vd->id);
		return vpu_suspend(vd);
	case APUSYS_CMD_EXECUTE:
		cmd = (struct apusys_cmd_handle *)hnd;
		if (vpu_req_check(cmd))
			return -EINVAL;

		req = (struct vpu_request *)cmd->cmdbufs[0].kva;
		vpu_trace_begin("vpu_%d|%s|cmd execute kid: 0x%08llx",
			vd->id, __func__, cmd->kid);
		vpu_cmd_debug("%s: vpu%d: EXECUTE, kva: %p kid: 0x%llx subcmd_idx: 0x%x\n",
			__func__, vd->id, (unsigned long)cmd->cmdbufs[0].kva,
			cmd->kid, cmd->subcmd_idx);
		req->prio = 0;
		/* overwrite vpu_req->boost from apusys_cmd */
		req->power_param.boost_value = cmd->boost;
		if (VPU_REQ_FLAG_TST(req, ALG_PRELOAD))
			ret = vpu_preempt(vd, req);
		else
			ret = vpu_execute(vd, req);
		vpu_trace_end("vpu_%d|%s|cmd execute kid: 0x%08llx",
			vd->id, __func__, cmd->kid);
		/* report vpu_req exe time to apusy_cmd */
		cmd->ip_time = req->busy_time / 1000;
		return ret;
	case APUSYS_CMD_PREEMPT:
		vpu_cmd_debug("%s: vpu%d: operation not allowed: %d\n",
			      __func__, vd->id, op);
		return -EACCES;
	case APUSYS_CMD_USER:
		ucmd = (struct apusys_usercmd_hnd *)hnd;
		vpu_cmd_debug("%s: vpu%d: USER, op: 0x%x size %d\n",
			      __func__, vd->id,
			      *(uint32_t *)ucmd->kva, ucmd->size);
		return vpu_ucmd_handle(vd, ucmd);
	default:
		vpu_cmd_debug("%s: vpu%d: unknown command: %d\n",
			      __func__, vd->id, op);
		break;
	}

	return -EINVAL;
}

/**
 * vpu_kbuf_alloc() - Allocate VPU kernel (execution) buffer
 * @vd: vpu device
 *
 *  vd->lock and vpu_cmd_lock() must be locked before calling this function
 */
int vpu_kbuf_alloc(struct vpu_device *vd)
{
	dma_addr_t iova = 0;
	struct timespec64 start, end;
	uint64_t period;
	struct vpu_mem_ops *mops = vd_mops(vd);

	if (vd->iova_kernel.m.va)
		return 0;

	ktime_get_ts64(&start);
	iova = mops->alloc(vd->dev, &vd->iova_kernel);
	ktime_get_ts64(&end);

	if (!iova) {
		pr_info("%s: vpu%d failed\n", __func__, vd->id);
		return -ENOMEM;
	}

	period = ((uint64_t)(timespec64_to_ns(&end)
		- timespec64_to_ns(&start)));

	mops->sync_for_cpu(vd->dev, &vd->iova_kernel);

	vpu_pwr_debug("%s: vpu%d, iova: 0x%llx, period: %llu ns, page num: %d\n",
	__func__, vd->id, (u64)iova, period, vd->iova_kernel.sgt.nents);

	return 0;
}

/**
 * vpu_kbuf_free() - Free VPU kernel (execution) buffer
 * @vd: vpu device
 *
 *  vd->lock and vpu_cmd_lock() must be locked before calling this function
 */
int vpu_kbuf_free(struct vpu_device *vd)
{
	struct platform_device *pdev
		= container_of(vd->dev, struct platform_device, dev);

	if (!vd->iova_kernel.m.va)
		return 0;

	vd_mops(vd)->free(&pdev->dev, &vd->iova_kernel);
	vd->iova_kernel.m.va = 0;
	return 0;
}

static struct vpu_device *vpu_alloc(struct platform_device *pdev)
{
	struct vpu_device *vd;

	vd = kzalloc(sizeof(struct vpu_device), GFP_KERNEL);
	if (!vd)
		return NULL;

	vd->dev = &pdev->dev;
	platform_set_drvdata(pdev, vd);
	return vd;
}

static void vpu_free(struct platform_device *pdev)
{
	struct vpu_device *vd = platform_get_drvdata(pdev);

	kfree(vd);
	platform_set_drvdata(pdev, NULL);
}

struct vpu_driver *vpu_drv;
static void vpu_drv_release(struct kref *ref);
static void vpu_drv_put(void)
{
	if (!vpu_drv)
		return;

	kref_put(&vpu_drv->ref, vpu_drv_release);
}

static void vpu_drv_get(void)
{
	kref_get(&vpu_drv->ref);
}

static void vpu_dev_add(struct vpu_device *vd)
{
	/* add to vd list */
	mutex_lock(&vpu_drv->lock);
	vpu_drv_get();
	list_add_tail(&vd->list, &vpu_drv->devs);
	mutex_unlock(&vpu_drv->lock);
}

static void vpu_dev_del(struct vpu_device *vd)
{
	/* remove from vd list */
	mutex_lock(&vpu_drv->lock);
	list_del(&vd->list);
	mutex_unlock(&vpu_drv->lock);
	vpu_drv_put();
}

static int vpu_init_bin(struct device_node *node)
{
	uint32_t phy_addr;
	uint32_t phy_size;
	uint32_t bin_head_ofs = 0;
	uint32_t bin_preload_ofs = 0;

	/* skip, if vpu firmware had ready been mapped */
	if (vpu_drv && vpu_drv->bin_va)
		return 0;

	if (!node) {
		pr_info("%s: unable to get vpu firmware\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_u32(node, "bin-phy-addr", &phy_addr) ||
		of_property_read_u32(node, "bin-size", &phy_size)) {
		pr_info("%s: unable to get bin info\n", __func__);
		return -ENODEV;
	}

	if (!of_property_read_u32(node, "img-head", &bin_head_ofs) &&
		!of_property_read_u32(node, "pre-bin", &bin_preload_ofs)) {
		vpu_drv->bin_type = VPU_IMG_PRELOAD;
	} else {
		vpu_drv->bin_type = VPU_IMG_LEGACY;
	}

	/* map vpu firmware to kernel virtual address */
	vpu_drv->bin_va = vpu_vmap(phy_addr, phy_size);
	vpu_drv->bin_pa = phy_addr;
	vpu_drv->bin_size = phy_size;

	pr_info("%s: mapped vpu firmware: pa: 0x%lx, size: 0x%x, kva: 0x%lx\n",
		__func__, vpu_drv->bin_pa, vpu_drv->bin_size,
		(unsigned long)vpu_drv->bin_va);

	vpu_drv->bin_head_ofs = bin_head_ofs;
	vpu_drv->bin_preload_ofs = bin_preload_ofs;

	pr_info("%s: header: 0x%x, preload:0x%x\n", __func__,
		vpu_drv->bin_head_ofs, vpu_drv->bin_preload_ofs);

	return 0;
}

static void vpu_shared_release(struct kref *ref)
{
	vpu_drv_debug("%s:\n", __func__);

	if (vpu_drv->mva_algo) {
		vpu_drv->vp->mops->free(vpu_drv->iova_dev, &vpu_drv->iova_algo);
		vpu_drv->mva_algo = 0;
	}
}

static int vpu_shared_put(struct platform_device *pdev,
	struct vpu_device *vd)
{
	vpu_drv->iova_dev = &pdev->dev;

	if (vpu_drv->mva_algo)
		kref_put(&vpu_drv->iova_ref, vpu_shared_release);

	return 0;
}

static int vpu_shared_get(struct platform_device *pdev,
	struct vpu_device *vd)
{
	dma_addr_t iova = 0;
	struct vpu_mem_ops *mops = vpu_drv->vp->mops;
	struct vpu_misc_ops *cops = vpu_drv->vp->cops;

	if (vpu_drv->mva_algo) {
		kref_get(&vpu_drv->iova_ref);
		return 0;
	}

	kref_init(&vpu_drv->iova_ref);

	if (!vpu_drv->mva_algo) {
		cops->emi_mpu_set(vpu_drv->bin_pa, vpu_drv->bin_size);

		if (mops->dts(vd->dev, "algo", &vpu_drv->iova_algo))
			goto error;
		iova = mops->alloc(vd->dev, &vpu_drv->iova_algo);
		if (!iova)
			goto error;
		vpu_drv->mva_algo = iova;
		vpu_drv->iova_algo.addr = iova;
	}

	return 0;

error:
	vpu_shared_put(pdev, vd);
	return -ENOMEM;
}

static int vpu_exit_dev_mem(struct platform_device *pdev,
	struct vpu_device *vd)
{
	struct vpu_mem_ops *mops = vd_mops(vd);

	vpu_drv_debug("%s: vpu%d\n", __func__, vd->id);
	mops->free(&pdev->dev, &vd->iova_reset);
	mops->free(&pdev->dev, &vd->iova_main);
	mops->free(&pdev->dev, &vd->iova_work);
	mops->free(&pdev->dev, &vd->iova_iram);
	vpu_shared_put(pdev, vd);

	return 0;
}

static int vpu_iomem_dts(struct platform_device *pdev,
	const char *name, int i, struct vpu_iomem *m)
{
	if (!m)
		return 0;

	m->res = platform_get_resource(pdev, IORESOURCE_MEM, i);

	if (!m->res) {
		dev_info(&pdev->dev, "unable to get resource: %s\n", name);
		return -ENODEV;
	}

	m->m = devm_ioremap_resource(&pdev->dev, m->res);

	if (!m->m) {
		dev_info(&pdev->dev, "unable to map iomem: %s\n", name);
		return -ENODEV;
	}

	dev_info(&pdev->dev, "mapped %s: 0x%lx: 0x%lx ~ 0x%lx\n", name,
		(unsigned long)m->m,
		(unsigned long)m->res->start,
		(unsigned long)m->res->end);

	return 0;
}

static int vpu_init_dev_mem(struct platform_device *pdev,
	struct vpu_device *vd)
{
	struct resource *res;
	dma_addr_t iova = 0;
	int ret = 0;
	struct vpu_mem_ops *mops = vd_mops(vd);
	struct vpu_config *cfg = vd_cfg(vd);
	struct device *dev = vd->dev;

	/* registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (vpu_iomem_dts(pdev, "reg", 0, &vd->reg) ||
		vpu_iomem_dts(pdev, "dmem", 1, &vd->dmem) ||
		vpu_iomem_dts(pdev, "imem", 2, &vd->imem) ||
		vpu_iomem_dts(pdev, "dbg", 3, &vd->dbg)) {
		goto error;
	}

	/* iova */
	if (mops->dts(dev, "reset-vector", &vd->iova_reset) ||
		mops->dts(dev, "main-prog", &vd->iova_main) ||
		mops->dts(dev, "kernel-lib", &vd->iova_kernel) ||
		mops->dts(dev, "iram-data", &vd->iova_iram) ||
		mops->dts(dev, "work-buf", &vd->iova_work)) {
		goto error;
	}

	if (vd->iova_work.size < (cfg->log_ofs + cfg->log_header_sz))
		goto error;

	vd->wb_log_size = vd->iova_work.size - cfg->log_ofs;
	vd->wb_log_data = vd->wb_log_size - cfg->log_header_sz;

	ret = vpu_shared_get(pdev, vd);
	if (ret)
		goto error;
	iova = mops->alloc(dev, &vd->iova_reset);
	if (!iova)
		goto error;
	iova = mops->alloc(dev, &vd->iova_main);
	if (!iova)
		goto error;
	iova = mops->alloc(dev, &vd->iova_work);
	if (!iova)
		goto error;
	iova = mops->alloc(dev, &vd->iova_iram);
	vd->mva_iram = iova;
	vd->iova_iram.addr = iova;

	return 0;
error:
	return -ENOMEM;
}


static int vpu_init_dev_irq(struct platform_device *pdev,
	struct vpu_device *vd)
{
	vd->irq_num = irq_of_parse_and_map(pdev->dev.of_node, 0);

	if (vd->irq_num <= 0) {
		pr_info("%s: %s: invalid IRQ: %d\n",
			__func__, vd->name, vd->irq_num);
		return -ENODEV;
	}

	pr_info("%s: %s: IRQ: %d\n",
		__func__, vd->name, vd->irq_num);

	return 0;
}

typedef int (*cmd_handler_t)(int op, void *hnd, struct apusys_device *adev);

static int vpu_init_adev(struct vpu_device *vd,
	struct apusys_device *adev, int type, cmd_handler_t hndl)
{
	int ret;

	adev->dev_type = type;
	adev->preempt_type = APUSYS_PREEMPT_WAITCOMPLETED;
	adev->private = vd;
	adev->send_cmd = hndl;
	memset(adev->meta_data, 0, sizeof(adev->meta_data));

	ret = apusys_register_device(adev);

	if (ret)
		pr_info("%s: type: %d, ret: %d\n",
			__func__, type, ret);

	return ret;
}

static int vpu_init_drv_plat(struct vpu_platform *vp)
{
	int ret = 0;

	if (!vp)
		return -EINVAL;

	if (vp->mops && vp->mops->init)
		ret = vp->mops->init();

	return ret;
}

static void vpu_exit_drv_plat(void)
{
	struct vpu_platform *vp;

	if (!vpu_drv || !vpu_drv->vp)
		return;

	vp = vpu_drv->vp;
	if (vp->mops && vp->mops->exit)
		vp->mops->exit();
}

static int vpu_init_dev_plat(struct platform_device *pdev,
	struct vpu_device *vd)
{
	int ret = 0;
	struct vpu_platform *vp;

	vp = (struct vpu_platform *)of_device_get_match_data(&pdev->dev);

	if (!vp) {
		dev_info(&pdev->dev, "unsupported device: %s\n", pdev->name);
		return -ENODEV;
	}

	vd->drv = vpu_drv;
	/* assign driver platform operations & platform init */
	mutex_lock(&vpu_drv->lock);
	if (!vpu_drv->vp) {
		vpu_drv->vp = vp;
		ret = vpu_init_drv_plat(vp);
		if (ret)
			vpu_drv->vp = NULL;
	} else if (vpu_drv->vp != vp) {
		dev_info(&pdev->dev, "device platform ops mispatch\n");
		ret = -EINVAL;
	}
	mutex_unlock(&vpu_drv->lock);

	return ret;
}

static int vpu_probe(struct platform_device *pdev)
{
	struct vpu_device *vd;

	int ret = 0;

	vd = vpu_alloc(pdev);
	if (!vd)
		return -ENOMEM;

	ret = of_property_read_u32(pdev->dev.of_node, "id", &vd->id);
	if (ret) {
		dev_info(&pdev->dev, "unable to get core ID: %d\n", ret);
		goto out;
	}

	if (vd->id == 0) {
		vpu_drv = kzalloc(sizeof(struct vpu_driver), GFP_KERNEL);

		if (!vpu_drv)
			return -ENOMEM;

		kref_init(&vpu_drv->ref);

		ret = vpu_init_bin(pdev->dev.of_node);
		if (ret)
			goto out;

		vpu_init_debug();

		INIT_LIST_HEAD(&vpu_drv->devs);
		mutex_init(&vpu_drv->lock);

		vpu_drv->mva_algo = 0;
		vpu_drv->wq = create_workqueue("vpu_wq");

		vpu_init_drv_hw();
		vpu_init_drv_met();
		vpu_init_drv_tags();
	}

	ret = snprintf(vd->name, sizeof(vd->name), "vpu%d", vd->id);
	if (ret < 0)
		goto out;

	ret = vpu_init_dev_plat(pdev, vd);
	if (ret)
		goto out;

	if (vpu_drv->bin_type != vpu_drv->vp->cfg->bin_type) {
		dev_info(&pdev->dev,
			"DTS bin type %d mispatch with device %d\n",
			vpu_drv->bin_type, vpu_drv->vp->cfg->bin_type);
		ret = -ENODEV;
		goto out;
	}

	/* put efuse judgement at beginning */
	if (vpu_drv->vp->cops->is_disabled(vd)) {
		ret = -ENODEV;
		vd->state = VS_DISALBED;
		goto out;
	} else {
		vd->state = VS_DOWN;
	}

	/* allocate resources */
	ret = vpu_init_dev_mem(pdev, vd);
	if (ret)
		goto free;

	ret = vpu_init_dev_irq(pdev, vd);
	if (ret)
		goto free;

	/* device hw initialization */
	ret = vpu_init_dev_hw(pdev, vd);
	if (ret)
		goto free;

	/* power initialization */
	ret = vpu_init_dev_pwr(pdev, vd);
	if (ret)
		goto hw_free;

	/* device algo initialization */
	ret = vpu_init_dev_algo(pdev, vd);
	if (ret)
		goto hw_free;

	/* register device to APUSYS */
	ret = vpu_init_adev(vd, &vd->adev,
		APUSYS_DEVICE_VPU, vpu_send_cmd);
	if (ret)
		goto hw_free;

	if (xos_type(vd) == VPU_XOS) {
		ret = vpu_init_adev(vd, &vd->adev_rt,
			APUSYS_DEVICE_VPU_RT, vpu_send_cmd_rt);
		if (ret)
			goto hw_free;
	}

	vpu_init_dev_debug(pdev, vd);
	vpu_init_dev_met(pdev, vd);
	vpu_dev_add(vd);
	dev_info(&pdev->dev, "%s: succeed\n", __func__);

	return 0;

	// TODO: add error handling free algo
hw_free:
	vpu_exit_dev_hw(pdev, vd);
free:
	vpu_exit_dev_mem(pdev, vd);
out:
	vpu_free(pdev);
	dev_info(&pdev->dev, "%s: failed\n", __func__);
	return ret;
}

static int vpu_remove(struct platform_device *pdev)
{
	struct vpu_device *vd = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "%s\n", __func__);
	vpu_exit_dev_met(pdev, vd);
	vpu_exit_dev_debug(pdev, vd);
	vpu_exit_dev_hw(pdev, vd);
	vpu_exit_dev_algo(pdev, vd);
	vpu_exit_dev_mem(pdev, vd);
	disable_irq(vd->irq_num);
	apusys_unregister_device(&vd->adev);
	if (xos_type(vd) == VPU_XOS)
		apusys_unregister_device(&vd->adev_rt);

	vpu_exit_dev_pwr(pdev, vd);
	vpu_dev_del(vd);
	vpu_free(pdev);

	return 0;
}

static int vpu_suspend(struct vpu_device *vd)
{
	vpu_cmd_lock_all(vd);
	vpu_pwr_debug("%s: pw_ref: %d, state: %d\n",
		__func__, vpu_pwr_cnt(vd), vd->state);

	if (!vpu_pwr_cnt(vd) && (vd->state != VS_DOWN)) {
		vpu_pwr_suspend_locked(vd);
		vpu_pwr_debug("%s: suspended\n", __func__);
	}
	vpu_cmd_unlock_all(vd);

	return 0;
}

static int vpu_resume(struct vpu_device *vd)
{
	return 0;
}

struct vpu_platform vpu_plat_mt6885 = {
	.bops = &vpu_bops_preload,
	.mops = &vpu_mops_v2,
	.sops = &vpu_sops_mt68xx,
	.cops = &vpu_cops_mt6885,
	.reg = &vpu_reg_mt68xx,
	.cfg = &vpu_cfg_mt68xx,
};

struct vpu_platform vpu_plat_mt68xx = {
	.bops = &vpu_bops_preload,
	.mops = &vpu_mops_v2,
	.sops = &vpu_sops_mt68xx,
	.cops = &vpu_cops_mt68xx,
	.reg = &vpu_reg_mt68xx,
	.cfg = &vpu_cfg_mt68xx,
};

struct vpu_platform vpu_plat_mt67xx = {
	.bops = &vpu_bops_legacy,
	.mops = &vpu_mops_v2,
	.sops = &vpu_sops_mt67xx,
	.cops = &vpu_cops_mt67xx,
	.reg = &vpu_reg_mt67xx,
	.cfg = &vpu_cfg_mt67xx,
};

static const struct of_device_id vpu_of_ids[] = {
	{.compatible = "mediatek,mt6885-vpu_core", .data = &vpu_plat_mt6885},
	{.compatible = "mediatek,mt6873-vpu_core", .data = &vpu_plat_mt68xx},
	{.compatible = "mediatek,mt6853-vpu_core", .data = &vpu_plat_mt68xx},
	{.compatible = "mediatek,mt6893-vpu_core", .data = &vpu_plat_mt68xx},
	{.compatible = "mediatek,mt6785-vpu_core", .data = &vpu_plat_mt67xx},
	{.compatible = "mediatek,mt6779-vpu_core", .data = &vpu_plat_mt67xx},
	{}
};

MODULE_DEVICE_TABLE(of, vpu_of_ids);

static struct platform_driver vpu_plat_drv = {
	.probe   = vpu_probe,
	.remove  = vpu_remove,
	.driver  = {
	.name = "vpu",
	.owner = THIS_MODULE,
	.of_match_table = vpu_of_ids,
	}
};

static void vpu_drv_release(struct kref *ref)
{
	vpu_drv_debug("%s:\n", __func__);
	vpu_exit_drv_plat();
	vpu_drv_debug("%s: destroy workqueue\n", __func__);
	if (vpu_drv->wq) {
		flush_workqueue(vpu_drv->wq);
		destroy_workqueue(vpu_drv->wq);
		vpu_drv->wq = NULL;
	}
	vpu_drv_debug("%s: iounmap\n", __func__);
	if (vpu_drv->bin_va) {
		iounmap(vpu_drv->bin_va);
		vpu_drv->bin_va = NULL;
	}
	kfree(vpu_drv);
	vpu_drv = NULL;
}

int vpu_init(struct apusys_core_info *info)
{
	int ret;

	if (!apusys_power_check()) {
		pr_info("%s: vpu is disabled by apusys\n", __func__);
		return -ENODEV;
	}

	vpu_init_algo();

	ret = platform_driver_register(&vpu_plat_drv);

	return ret;
}

void vpu_exit(void)
{
	struct vpu_device *vd;
	struct list_head *ptr, *tmp;

	if (!vpu_drv) {
		vpu_drv_debug("%s: platform_driver_unregister\n", __func__);
		platform_driver_unregister(&vpu_plat_drv);
		return;
	}

	/* notify all devices that we are going to be removed
	 *  wait and stop all on-going requests
	 **/
	mutex_lock(&vpu_drv->lock);
	list_for_each_safe(ptr, tmp, &vpu_drv->devs) {
		vd = list_entry(ptr, struct vpu_device, list);
		vpu_cmd_lock_all(vd);
		vd->state = VS_REMOVING;
		vpu_cmd_unlock_all(vd);
	}
	mutex_unlock(&vpu_drv->lock);
	vpu_drv_debug("%s: platform_driver_unregister\n", __func__);
	platform_driver_unregister(&vpu_plat_drv);
	vpu_exit_drv_tags();
	vpu_exit_drv_met();
	vpu_exit_drv_hw();
	vpu_exit_debug();
	vpu_drv_put();
}

