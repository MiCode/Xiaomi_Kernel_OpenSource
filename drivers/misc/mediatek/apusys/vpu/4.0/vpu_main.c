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

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/of_irq.h>

/* internal headers */
#include "apusys_power.h"
#include "vpu_drv.h"
#include "vpu_power.h"
#include "vpu_cmn.h"
#include "vpu_mem.h"
#include "vpu_algo.h"
#include "vpu_debug.h"
#include "remoteproc_internal.h"  // TODO: move to drivers/remoteproc/../..
#include "vpu_trace.h"
#include "vpu_ioctl.h"
#include "vpu_met.h"

/* remote proc */
#define VPU_REMOTE_PROC (0)

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

	if (!ucmd->kva || !ucmd->iova || !ucmd->size) {
		vpu_cmd_debug("invalid argument(0x%llx/0x%llx/%u)\n",
			      (uint64_t)ucmd->kva,
			      (uint64_t)ucmd->iova,
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
			mutex_lock(&vd->lock);
			alg = vpu_alg_get(vd, palg_cmd->name, NULL);
			if (alg)
				vpu_alg_put(alg);
			else
				ret = -ENOENT;
			mutex_unlock(&vd->lock);
			/* algo not found in some vpu_device */
			if (ret) {
				vpu_cmd_debug("%s: vpu%d: not found algo \"%s\"\n",
					      __func__, vd->id, palg_cmd->name);
				break;
			}
		}
		mutex_unlock(&vpu_drv->lock);
	} else {
		vpu_cmd_debug("%s: unknown user cmd: 0x%x\n",
			      __func__, *(uint32_t *)ucmd->kva);
		ret = -EINVAL;
	}

	return ret;
}

/* interface to APUSYS */
int vpu_send_cmd(int op, void *hnd, struct apusys_device *adev)
{
	int ret = 0;
	struct vpu_device *vd;
	struct vpu_request *req;
	struct apusys_cmd_hnd *cmd;
	struct apusys_power_hnd *pw;
	struct apusys_preempt_hnd *pmt;
	struct apusys_firmware_hnd *fw;
	struct apusys_usercmd_hnd *ucmd;

	// TODO: implement these sub-functions and remove UNUSED()
#define UNUSED(x) ((void)x)
	UNUSED(pw);
	UNUSED(pmt);
#undef UNUSED

	vd = (struct vpu_device *)adev->private;

	vpu_cmd_debug("%s: cmd: %d, hnd: %p\n", __func__, op, hnd);

	switch (op) {
	case APUSYS_CMD_POWERON:
		pw = (struct apusys_power_hnd *)hnd;
		vpu_cmd_debug("%s:vpu%d POWERON, boost: %d, opp: %d, timeout: %d\n",
			      __func__, vd->id,
			      pw->boost_val, pw->opp, pw->timeout);
		return vpu_pwr_up(vd, pw->boost_val,
			(pw->timeout ? vd->pw_off_latency : 0));
	case APUSYS_CMD_POWERDOWN:
		vpu_cmd_debug("%s:vpu%d POWERDOWN\n",
			      __func__, vd->id);
		vpu_pwr_down(vd);
		return 0;
	case APUSYS_CMD_RESUME:
		vpu_cmd_debug("%s:vpu%d RESUME\n",
			      __func__, vd->id);
		return vpu_resume(vd);
	case APUSYS_CMD_SUSPEND:
		vpu_cmd_debug("%s:vpu%d SUSPEND\n",
			      __func__, vd->id);
		return vpu_suspend(vd);
	case APUSYS_CMD_EXECUTE:
		cmd = (struct apusys_cmd_hnd *)hnd;
		req = (struct vpu_request *)cmd->kva;
		vpu_trace_begin("%s|cmd execute cmd_id: 0x%08llx",
				__func__, cmd->cmd_id);
		vpu_cmd_debug("%s:vpu%d EXECUTE, kva: %lx cmd_id: 0x%llx subcmd_idx: 0x%x\n",
			      __func__, vd->id, (unsigned long)cmd->kva,
			      cmd->cmd_id, cmd->subcmd_idx);
		/* overwrite vpu_req->boost from apusys_cmd */
		req->power_param.boost_value = cmd->boost_val;
		ret = vpu_execute(vd, req);
		vpu_trace_end("%s|end", __func__);
		/* report vpu_req exe time to apusy_cmd */
		cmd->ip_time = req->busy_time / 1000;
		return ret;
	case APUSYS_CMD_PREEMPT:
		pmt = (struct apusys_preempt_hnd *)hnd;
		vpu_cmd_debug("%s:vpu%d PREEMPT, new cmd kva: %lx\n",
			      __func__, vd->id,
			      (unsigned long)pmt->new_cmd->kva);
		break;
	case APUSYS_CMD_FIRMWARE:
		fw = (struct apusys_firmware_hnd *)hnd;
		vpu_cmd_debug("%s:vpu%d FIRMWARE, op: %d, name: %s\n",
			      __func__, vd->id, fw->op, fw->name);
		return vpu_firmware(vd, fw);
	case APUSYS_CMD_USER:
		ucmd = (struct apusys_usercmd_hnd *)hnd;
		vpu_cmd_debug("%s:vpu%d USER, op: 0x%x size %d\n",
			      __func__, vd->id,
			      *(uint32_t *)ucmd->kva, ucmd->size);
		return vpu_ucmd_handle(vd, ucmd);
	default:
		vpu_cmd_debug("%s:vpu%d unknown command: %d\n",
			      __func__, vd->id, op);
		break;
	}

	return -EINVAL;
}

#if VPU_REMOTE_PROC
#define VPU_FIRMWARE_NAME "mtk_vpu"

static int vpu_start(struct rproc *rproc)
{
	/* enable power and clock */
	return 0;
}

static int vpu_stop(struct rproc *rproc)
{
	/* disable regulator and clock */
	return 0;
}

static void *vpu_da_to_va(struct rproc *rproc, u64 da, int len)
{
	/* convert device address to kernel virtual address */
	return 0;
}

static const struct rproc_ops vpu_ops = {
	.start = vpu_start,
	.stop = vpu_stop,
	.da_to_va = vpu_da_to_va,
};

static int vpu_load(struct rproc *rproc, const struct firmware *fw)
{
	return 0;
}

#if 1
// TODO: move to drivers/remoteproc/../..
static struct resource_table *
vpu_rsc_table(struct rproc *rproc, const struct firmware *fw, int *tablesz)
{
	static struct resource_table table = { .ver = 1, };

	*tablesz = sizeof(table);
	return &table;
}

static const struct rproc_fw_ops vpu_fw_ops = {
	.find_rsc_table = vpu_rsc_table,
	.load = vpu_load,
};
#endif

static struct vpu_device *vpu_alloc(struct platform_device *pdev)
{
	struct vpu_device *vd;
	struct rproc *rproc;

	rproc = rproc_alloc(&pdev->dev, pdev->name, &vpu_ops,
		VPU_FIRMWARE_NAME, sizeof(*vd));

	if (!rproc) {
		dev_info(&pdev->dev, "failed to allocate rproc\n");
		return NULL;
	}

	/* initialize device (core specific) data */
	rproc->fw_ops = &vpu_fw_ops;
	vd = (struct vpu_device *)rproc->priv;
	vd->rproc = rproc;

	return vd;
}

static void vpu_free(struct platform_device *pdev)
{
	struct vpu_device *vd = platform_get_drvdata(pdev);

	rproc_free(vd->rproc);
}

static int vpu_dev_add(struct platform_device *pdev)
{
	struct vpu_device *vd = platform_get_drvdata(pdev);
	int ret;

	ret = rproc_add(vd->rproc);
	if (ret)
		dev_info(&pdev->dev, "rproc_add: %d\n", ret);

	return ret;
}

static void vpu_dev_del(struct platform_device *pdev)
{
	struct vpu_device *vd = platform_get_drvdata(pdev);

	rproc_del(vd->rproc);
}

#else
static struct vpu_device *vpu_alloc(struct platform_device *pdev)
{
	struct vpu_device *vd;

	vd = kzalloc(sizeof(struct vpu_device), GFP_KERNEL);
	return vd;
}

static void vpu_free(struct platform_device *pdev)
{
	struct vpu_device *vd = platform_get_drvdata(pdev);

	kfree(vd);
	platform_set_drvdata(pdev, NULL);
}

static int vpu_dev_add(struct platform_device *pdev)
{
	return 0;
}

static void vpu_dev_del(struct platform_device *pdev)
{
}
#endif

struct vpu_driver *vpu_drv;

void vpu_drv_release(struct kref *ref)
{
	vpu_drv_debug("%s:\n", __func__);
	kfree(vpu_drv);
	vpu_drv = NULL;
}

void vpu_drv_put(void)
{
	if (!vpu_drv)
		return;

	if (vpu_drv->wq) {
		flush_workqueue(vpu_drv->wq);
		destroy_workqueue(vpu_drv->wq);
		vpu_drv->wq = NULL;
	}

	vpu_drv_debug("%s:\n", __func__);
	kref_put(&vpu_drv->ref, vpu_drv_release);
}

void vpu_drv_get(void)
{
	kref_get(&vpu_drv->ref);
}

static int vpu_init_bin(void)
{
	struct device_node *node;
	uint32_t phy_addr;
	uint32_t phy_size;

	/* skip, if vpu firmware had ready been mapped */
	if (vpu_drv && vpu_drv->bin_va)
		return 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,vpu_core0");

	if (of_property_read_u32(node, "bin-phy-addr", &phy_addr) ||
		of_property_read_u32(node, "bin-size", &phy_size)) {
		pr_info("%s: unable to get vpu firmware.\n", __func__);
		return -ENODEV;
	}

	/* map vpu firmware to kernel virtual address */
	vpu_drv->bin_va = ioremap_wc(phy_addr, phy_size);
	vpu_drv->bin_pa = phy_addr;
	vpu_drv->bin_size = phy_size;

	pr_info("%s: mapped vpu firmware: pa: 0x%lx, size: 0x%x, kva: 0x%lx\n",
		__func__, vpu_drv->bin_pa, vpu_drv->bin_size,
		(unsigned long)vpu_drv->bin_va);

	return 0;
}

static void vpu_shared_release(struct kref *ref)
{
	vpu_drv_debug("%s:\n", __func__);

	if (vpu_drv->mva_algo) {
		vpu_iova_free(vpu_drv->iova_dev, &vpu_drv->iova_algo);
		vpu_drv->mva_algo = 0;
	}

	if (vpu_drv->mva_share) {
		vpu_iova_free(vpu_drv->iova_dev, &vpu_drv->iova_share);
		vpu_drv->mva_share = 0;
	}
}

static int vpu_shared_put(struct platform_device *pdev,
	struct vpu_device *vd)
{
	vpu_drv->iova_dev = &pdev->dev;
	kref_put(&vpu_drv->iova_ref, vpu_shared_release);
	return 0;
}

static int vpu_shared_get(struct platform_device *pdev,
	struct vpu_device *vd)
{
	dma_addr_t iova = 0;

	if (vpu_drv->mva_algo && vpu_drv->mva_share) {
		kref_get(&vpu_drv->iova_ref);
		return 0;
	}

	kref_init(&vpu_drv->iova_ref);

	if (!vpu_drv->mva_algo) {
		if (vpu_iova_dts(pdev, "algo", &vpu_drv->iova_algo))
			goto error;
		iova = vpu_iova_alloc(pdev,	&vpu_drv->iova_algo);
		if (!iova)
			goto error;
		vpu_drv->mva_algo = iova;
		vpu_drv->iova_algo.addr = iova;
	}

	if (!vpu_drv->mva_share) {
		if (vpu_iova_dts(pdev, "share-data", &vpu_drv->iova_share))
			goto error;
		iova = vpu_iova_alloc(pdev,	&vpu_drv->iova_share);
		if (!iova)
			goto error;
		vpu_drv->mva_share = iova;
		vpu_drv->iova_share.addr = iova;
	}

	return 0;

error:
	vpu_shared_put(pdev, vd);
	return -ENOMEM;
}

static int vpu_exit_dev_mem(struct platform_device *pdev,
	struct vpu_device *vd)
{
	vpu_iova_free(&pdev->dev, &vd->iova_reset);
	vpu_iova_free(&pdev->dev, &vd->iova_main);
	vpu_iova_free(&pdev->dev, &vd->iova_kernel);
	vpu_iova_free(&pdev->dev, &vd->iova_work);
	vpu_iova_free(&pdev->dev, &vd->iova_iram);
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

	dev_info(&pdev->dev, "mapped %s: 0x%lx\n", name, (unsigned long)m->m);

	return 0;
}

static int vpu_init_dev_mem(struct platform_device *pdev,
	struct vpu_device *vd)
{
	struct resource *res;
	dma_addr_t iova = 0;
	int ret = 0;

	/* registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (vpu_iomem_dts(pdev, "reg", 0, &vd->reg) ||
		vpu_iomem_dts(pdev, "dmem", 1, &vd->dmem) ||
		vpu_iomem_dts(pdev, "imem", 2, &vd->imem) ||
		vpu_iomem_dts(pdev, "dbg", 3, &vd->dbg)) {
		goto error;
	}

	/* iova */
	if (vpu_iova_dts(pdev, "reset-vector", &vd->iova_reset) ||
		vpu_iova_dts(pdev, "main-prog", &vd->iova_main) ||
		vpu_iova_dts(pdev, "kernel-lib", &vd->iova_kernel) ||
		vpu_iova_dts(pdev, "iram-data", &vd->iova_iram) ||
		vpu_iova_dts(pdev, "work-buf", &vd->iova_work)) {
		goto error;
	}

	ret = vpu_shared_get(pdev, vd);
	if (ret)
		goto error;
	iova = vpu_iova_alloc(pdev, &vd->iova_reset);
	if (!iova)
		goto free;
	iova = vpu_iova_alloc(pdev, &vd->iova_main);
	if (!iova)
		goto free;
	iova = vpu_iova_alloc(pdev, &vd->iova_kernel);
	if (!iova)
		goto free;
	iova = vpu_iova_alloc(pdev, &vd->iova_work);
	if (!iova)
		goto free;
	iova = vpu_iova_alloc(pdev, &vd->iova_iram);
	vd->mva_iram = iova;
	vd->iova_iram.addr = iova;

	return 0;

free:
	vpu_exit_dev_mem(pdev, vd);
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

static int vpu_probe(struct platform_device *pdev)
{
	struct vpu_device *vd;
	int ret;

	vd = vpu_alloc(pdev);
	if (!vd)
		return -ENOMEM;

	vd->dev = &pdev->dev;
	platform_set_drvdata(pdev, vd);

	if (of_property_read_u32(pdev->dev.of_node, "id", &vd->id)) {
		dev_info(&pdev->dev, "unable to get core id from dts\n");
		ret = -ENODEV;
		goto free;
	}

	snprintf(vd->name, sizeof(vd->name), "vpu%d", vd->id);

	/* put efuse judgement at beginning */
	if (vpu_is_disabled(vd)) {
		ret = -ENODEV;
		vd->state = VS_DISALBED;
		goto free;
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
		goto free;

	/* device algo initialization */
	INIT_LIST_HEAD(&vd->algo);
	ret = vpu_init_dev_algo(pdev, vd);
	if (ret)
		goto free;

	/* register device to APUSYS */
	vd->adev.dev_type = APUSYS_DEVICE_VPU;
	vd->adev.preempt_type = APUSYS_PREEMPT_WAITCOMPLETED;
	vd->adev.private = vd;
	vd->adev.send_cmd = vpu_send_cmd;

	ret = apusys_register_device(&vd->adev);
	if (ret) {
		dev_info(&pdev->dev, "apusys_register_device: %d\n",
			ret);
		goto free;
	}

	/* register debugfs nodes */
	ret = vpu_init_dev_debug(pdev, vd);
	if (ret)
		goto free;

	vpu_init_dev_met(pdev, vd);

	ret = vpu_dev_add(pdev);
	if (ret)
		goto free;

	/* add to vd list */
	mutex_lock(&vpu_drv->lock);
	vpu_drv_get();
	list_add_tail(&vd->list, &vpu_drv->devs);
	mutex_unlock(&vpu_drv->lock);

	dev_info(&pdev->dev, "%s: succeed\n", __func__);
	return 0;

	// TODO: add error handling free algo

free:
	vpu_free(pdev);
	dev_info(&pdev->dev, "%s: failed\n", __func__);
	return ret;
}

static int vpu_remove(struct platform_device *pdev)
{
	struct vpu_device *vd = platform_get_drvdata(pdev);

	vpu_exit_dev_met(pdev, vd);
	vpu_exit_dev_debug(pdev, vd);
	vpu_exit_dev_hw(pdev, vd);
	vpu_exit_dev_algo(pdev, vd);
	vpu_exit_dev_mem(pdev, vd);
	disable_irq(vd->irq_num);
	apusys_unregister_device(&vd->adev);
	vpu_exit_dev_pwr(pdev, vd);
	vpu_dev_del(pdev);
	vpu_free(pdev);
	vpu_drv_put();

	return 0;
}

static int vpu_suspend(struct vpu_device *vd)
{
	mutex_lock(&vd->lock);
	mutex_lock(&vd->cmd_lock);
	vpu_pwr_debug("%s: pw_ref: %d, state: %d\n",
		__func__, vpu_pwr_cnt(vd), vd->state);

	if (!vpu_pwr_cnt(vd) && (vd->state != VS_DOWN)) {
		vpu_pwr_suspend_locked(vd);
		vpu_pwr_debug("%s: suspended\n", __func__);
	}
	mutex_unlock(&vd->cmd_lock);
	mutex_unlock(&vd->lock);

	return 0;
}

static int vpu_resume(struct vpu_device *vd)
{
	return 0;
}

static const struct of_device_id vpu_of_ids[] = {
	{.compatible = "mediatek,vpu_core0",},
	{.compatible = "mediatek,vpu_core1",},
	{.compatible = "mediatek,vpu_core2",},
	{}
};

static struct platform_driver vpu_plat_drv = {
	.probe   = vpu_probe,
	.remove  = vpu_remove,
	.driver  = {
	.name = "vpu",
	.owner = THIS_MODULE,
	.of_match_table = vpu_of_ids,
	}
};

static int __init vpu_init(void)
{
	int ret;
	vpu_drv = NULL;

	if (!apusys_power_check()) {
		pr_info("%s: vpu is disabled by apusys\n", __func__);
		return -ENODEV;
	}

	vpu_drv = kzalloc(sizeof(struct vpu_driver), GFP_KERNEL);

	if (!vpu_drv)
		return -ENOMEM;

	kref_init(&vpu_drv->ref);

	ret = vpu_init_bin();
	if (ret)
		goto error_out;

	ret = vpu_init_algo();
	if (ret)
		goto error_out;

	vpu_init_debug();

	INIT_LIST_HEAD(&vpu_drv->devs);
	mutex_init(&vpu_drv->lock);

	vpu_drv->mva_algo = 0;
	vpu_drv->mva_share = 0;
	vpu_drv->wq = create_workqueue("vpu_wq");

	vpu_init_drv_hw();
	vpu_init_drv_met();

	ret = platform_driver_register(&vpu_plat_drv);

	return ret;

error_out:
	kfree(vpu_drv);
	vpu_drv = NULL;
	return ret;
}

static void __exit vpu_exit(void)
{
	struct vpu_device *vd;
	struct list_head *ptr, *tmp;

	/* notify all devices that we are going to be removed
	 *  wait and stop all on-going requests
	 **/
	mutex_lock(&vpu_drv->lock);
	list_for_each_safe(ptr, tmp, &vpu_drv->devs) {
		vd = list_entry(ptr, struct vpu_device, list);
		list_del(ptr);
		mutex_lock(&vd->cmd_lock);
		vd->state = VS_REMOVING;
		mutex_unlock(&vd->cmd_lock);
	}
	mutex_unlock(&vpu_drv->lock);

	vpu_exit_debug();
	vpu_exit_drv_met();
	vpu_exit_drv_hw();

	if (vpu_drv) {
		vpu_drv_debug("%s: iounmap\n", __func__);
		if (vpu_drv->bin_va) {
			iounmap(vpu_drv->bin_va);
			vpu_drv->bin_va = NULL;
		}

		vpu_drv_put();
	}

	vpu_drv_debug("%s: platform_driver_unregister\n", __func__);
	platform_driver_unregister(&vpu_plat_drv);
}

// module_init(vpu_init);
late_initcall(vpu_init);
module_exit(vpu_exit);
MODULE_DESCRIPTION("Mediatek VPU Driver");
MODULE_LICENSE("GPL");

