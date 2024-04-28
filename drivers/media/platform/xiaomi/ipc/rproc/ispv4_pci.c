// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#include "ispv4_rproc.h"
#include <linux/dma-map-ops.h>
#include <linux/iommu.h>
#include <linux/of_reserved_mem.h>
#include <ispv4_regops.h>
#include <linux/mailbox_client.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/delay.h>

#define USING_PCIE_SMMU
#define RPMSG_USE_MBOX
#define CPU_PMU_DELAY 1000

extern struct dentry *ispv4_debugfs;

enum {
	ISPV4_MEM_SRAM,
	ISPV4_MEM_DDR,
	ISPV4_MEM_ATTACH_REG,
	AP_MEM_RPMSG_VRING,
	AP_MEM_RPMSG_BUF,
	XM_ISPV4_MEM_NR,
};

// va pa da remote_a size flag
static struct xm_ispv4_rproc_mem memseg[XM_ISPV4_MEM_NR] = {
	// ispv4 sram.
	{ NULL, 0, 0, 0x00000000, 4 * 1024 * 1024, XM_RPROC_MEM_FLAG_DEV_MEM },
	// ispv4 ddr.
	{ NULL, 0, 0, 0x80000000, 64 * 1024 * 1024, XM_RPROC_MEM_FLAG_DEV_MEM },
	// ispv4 rproc attach
	{ NULL, 0, 0, 0, 1024 * 1024, XM_RPROC_MEM_FLAG_DEV_MEM },
	// rpmsg vring.
	{ NULL, 0x84c00000, 0x84c00000, 0, 0x20000, XM_RPROC_MEM_FLAG_AP_MEM },
	// rpmsg buffer.
	{ NULL, 0x84c20000, 0x84c20000, 0, 0x100000, XM_RPROC_MEM_FLAG_AP_MEM },
};

static int ispv4_rproc_start(struct rproc *rproc)
{
	// NOT_NEED
	dev_info(&rproc->dev, "ispv4: %s", __FUNCTION__);
	return 0;
}

static int ispv4_mcu_rproc_attach(struct rproc *rproc)
{
	struct xm_ispv4_rproc *rp = rproc->priv;
	int ret;

	dev_info(&rproc->dev, "ispv4: %s", __FUNCTION__);
	ispv4_load_rsc_table(rproc, rp->fw);

	ret = xm_ispv4_rpmsg_init(rproc->priv);
	if (ret != 0) {
		dev_err(&rproc->dev, "ispv4: rpmsg init failed\n");
		goto rpmsg_init_err;
	}
#ifdef RPMSG_USE_MBOX
	mutex_lock(&rp->send_mbox_lock);
	rp->mbox_rpmsg_chan = mbox_request_channel(&rp->mbox_rpmsg, 1);
	if (rp->mbox_rpmsg_chan == NULL) {
		dev_err(&rproc->dev, "ispv4: rpmsg init failed\n");
		mutex_unlock(&rp->send_mbox_lock);
		ret = -EBUSY;
		goto mbox_err;
	}
	mutex_unlock(&rp->send_mbox_lock);
#endif

	ret = 0;
#if IS_ENABLED(CONFIG_MIISP_CHIP)
	/* Open cpu pmu */
	ret = enable_cpu_pmu_pcie(memseg[ISPV4_MEM_ATTACH_REG].virt_addr);
	if (ret != 0) {
		dev_err(&rproc->dev, "ispv4: enable_cpu_pmu failed\n");
		goto rel_cpu_err;
	}
#endif
	// TAG for FW know pcie.
	writel(1, memseg[ISPV4_MEM_SRAM].virt_addr + 0x0021be0c);

	// Reset CPU
	writel(0, memseg[ISPV4_MEM_ATTACH_REG].virt_addr + 0x0040C);

	// Config busmonitor
	writel(500, memseg[ISPV4_MEM_ATTACH_REG].virt_addr + 0x70018);

	// Switch CPU freq to high freq
	writel(0x1, memseg[ISPV4_MEM_ATTACH_REG].virt_addr + 0x6202C);

	// Release CPU
	writel(0x11F, memseg[ISPV4_MEM_ATTACH_REG].virt_addr + 0x40C);

	dev_info(&rproc->dev, "ispv4: %s success\n", __func__);
	return 0;

rel_cpu_err:
#ifdef RPMSG_USE_MBOX
	mbox_free_channel(rp->mbox_rpmsg_chan);
mbox_err:
#endif
	xm_ispv4_rpmsg_stopdeal(rproc->priv);
	xm_ispv4_rpmsg_exit(rproc->priv);
rpmsg_init_err:
	return ret;
}

static int ispv4_rproc_stop(struct rproc *rproc)
{
	struct xm_ispv4_rproc *rp = rproc->priv;
	dev_info(&rproc->dev, "ispv4: %s into", __FUNCTION__);
	ispv4_regops_write(0xD40040C, 0);
	xm_ispv4_rpmsg_exit(rp);
	dev_info(&rproc->dev, "ispv4: %s finish", __FUNCTION__);
	return 0;
}

static void ispv4_rproc_kick(struct rproc *rproc, int vqid)
{
#ifdef RPMSG_USE_MBOX
	struct xm_ispv4_rproc *rp = rproc->priv;
	static u32 idx = 0;
	u32 data[4] = { 0, 0, 0, vqid };
	int ret;

	data[2] = idx++;
	mutex_lock(&rp->send_mbox_lock);
	if (unlikely(rp->mbox_rpmsg_chan == NULL)) {
		dev_warn(&rproc->dev, "kick buf mbox has free");
		mutex_unlock(&rp->send_mbox_lock);
		return;
	}
	ret = mbox_send_message(rp->mbox_rpmsg_chan, data);
	mutex_unlock(&rp->send_mbox_lock);
	if (ret < 0) {
		dev_warn(&rproc->dev, "ispv4: kick by mbox maybe err %d\n", ret);
	} else {
		dev_info(&rproc->dev, "ispv4 kick idx=%x success %d\n", data[2],
			 ret);
	}
#endif
	dev_info(&rproc->dev, "ispv4: %s", __FUNCTION__);
	return;
}

static void *ispv4_mcu_rproc_da_to_va(struct rproc *rproc, u64 da, size_t len,
				      bool *is_iomem)
{
	int i;
	s64 offset;
	struct xm_ispv4_rproc_mem *_memseg;

	if (is_iomem != NULL)
		*is_iomem = false;

	for (i = 0; i < XM_ISPV4_MEM_NR; i++) {
		_memseg = &memseg[i];
		offset = da - _memseg->dev_addr;
		if (offset >= 0 && offset + len <= _memseg->size)
			return _memseg->virt_addr + offset;
	}
	// HACK FOR DDR
	if (da == 0x80000000 && len == 0x80000000) {
		return memseg[ISPV4_MEM_DDR].virt_addr;
	}

	return NULL;
}

static struct rproc_ops ispv4_core_ops = {
	.start = ispv4_rproc_start,
	.stop = ispv4_rproc_stop,
	.kick = ispv4_rproc_kick,
	.da_to_va = ispv4_mcu_rproc_da_to_va,
	.attach = ispv4_mcu_rproc_attach,
};

static int ispv4_parse_irq(struct xm_ispv4_rproc *rp)
{
	int ret, irq;
	struct platform_device *pdev;

	pdev = container_of(rp->dev, struct platform_device, dev);
	irq = platform_get_irq(pdev, 0);
	rp->irq_ipc = irq < 0 ? 0 : irq;
	irq = platform_get_irq(pdev, 1);
	rp->irq_crash = irq < 0 ? 0 : irq;

	if (rp->irq_crash != 0) {
		ret = devm_request_irq(rp->dev, rp->irq_crash, ispv4_crash_irq,
				       0, "ispv4_rproc_crash", rp);
		if (ret != 0) {
			dev_err(rp->dev,
				"ispv4 rproc alloc crash irq %d failed.\n",
				rp->irq_crash);
			return -EINVAL;
		}
	}
#ifndef RPMSG_USE_MBOX
	if (rp->irq_ipc != 0) {
		ret = devm_request_irq(rp->dev, rp->irq_ipc, xm_ispv4_rpmsg_irq,
				       0, "ispv4_rproc_ipc", rp);
		if (ret != 0) {
			dev_err(&pdev->dev,
				"ispv4 rproc alloc ipc irq %d failed.\n",
				rp->irq_ipc);
			return -EINVAL;
		}
	}
#else
	rp->mbox_rpmsg.dev = rp->dev;
	rp->mbox_rpmsg.tx_block = true;
	rp->mbox_rpmsg.tx_tout = 3;
	rp->mbox_rpmsg.knows_txdone = false;
	rp->mbox_rpmsg.rx_callback = xm_ispv4_rpmsg_mbox_cb;
#endif

	// close irq when not boot
	if (rp->irq_crash > 0)
		disable_irq(rp->irq_crash);

	dev_info(rp->dev, "dump_irq > ipc irq:%d, crash irq:%d", rp->irq_ipc,
		 rp->irq_crash);

	return 0;
}

static void *ring_and_buf_va;
static struct debugfs_blob_wrapper iommu_tregion[2];
static struct dentry *iommu_dey;
__maybe_unused static void ispv4_iommu_test_init(void)
{
	struct dentry *tmp_dey;

	ring_and_buf_va = ioremap(0x84c00000, 0x120000);
	if (IS_ERR_OR_NULL(ring_and_buf_va)) {
		pr_err("ispv4 iommu test init failed! %d",
		       PTR_ERR(ring_and_buf_va));
		return;
	}

	iommu_tregion[0].data = ring_and_buf_va;
	iommu_tregion[0].size = 0x20000;
	iommu_tregion[1].data = ring_and_buf_va + 0x20000;
	iommu_tregion[1].size = 0x100000;

	iommu_dey = debugfs_create_dir("ispv4_iommu_rpmsg", ispv4_debugfs);
	if (IS_ERR_OR_NULL(iommu_dey)) {
		pr_err("ispv4 iommu debugfs dir failed! %d",
		       PTR_ERR(iommu_dey));
		return;
	}
	tmp_dey =
		debugfs_create_blob("ring", 0666, iommu_dey, &iommu_tregion[0]);
	if (IS_ERR_OR_NULL(tmp_dey)) {
		pr_err("ispv4 iommu debugfs file failed! %d",
		       PTR_ERR(iommu_dey));
	}
	tmp_dey = debugfs_create_blob("buffer", 0666, iommu_dey,
				      &iommu_tregion[1]);
	if (IS_ERR_OR_NULL(tmp_dey)) {
		pr_err("ispv4 iommu debugfs file failed! %d",
		       PTR_ERR(iommu_dey));
	}
}

__maybe_unused static void ispv4_iommu_test_deinit(void)
{
	if (!IS_ERR_OR_NULL(iommu_dey)) {
		debugfs_remove(iommu_dey);
		iommu_dey = NULL;
	}
	if (!IS_ERR_OR_NULL(ring_and_buf_va)) {
		iounmap(ring_and_buf_va);
		ring_and_buf_va = NULL;
	}
}

// Call from driver probe.
// If failed, call `ispv4_rproc_pci_deinit`
// Map iommu, declear dma region, bind dt node.
static int ispv4_rproc_pci_init(struct xm_ispv4_rproc *rp)
{
	struct xm_ispv4_rproc_mem *_memseg;
	struct resource *iores;
	int ret, i;
	struct device_node *np;
	struct platform_device *pdev;
#ifdef USING_PCIE_SMMU
	struct iommu_domain *domain;
	int iommu_flag;
	iommu_flag = IOMMU_READ | IOMMU_WRITE | IOMMU_NOEXEC | IOMMU_MMIO;
#endif

	pdev = container_of(rp->dev, struct platform_device, dev);

	ret = ispv4_parse_irq(rp);
	if (ret != 0) {
		dev_err(rp->dev, "ispv4 parse irq failed\n");
		return ret;
	}

	for (i = ISPV4_MEM_SRAM; i <= ISPV4_MEM_DDR; i++) {
		_memseg = &memseg[i];
		iores = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!iores) {
			dev_err(rp->dev, "ispv4 get resource %d info failed\n",
				i);
			return -EINVAL;
		}
		dev_info(rp->dev, "ispv4 get resource %d: %pR", i, iores);
		// if (!request_mem_region(iores->start, resource_size(iores),
		// 			pdev->name)) {
		// 	dev_err(rp->dev, "ispv4 request region %d failed\n", i);
		// 	return -EINVAL;
		// }
		if (resource_size(iores) != _memseg->size) {
			dev_err(rp->dev, "ispv4 request region %d failed\n", i);
			release_mem_region(iores->start, resource_size(iores));
			return -EINVAL;
		}
		_memseg->phys_addr = iores->start;
		_memseg->virt_addr =
			ioremap_wc(_memseg->phys_addr, _memseg->size);
		dev_info(rp->dev, "ispv4 map %d: va=0x%x da=0x%x", i,
			 _memseg->virt_addr, _memseg->dev_addr);
		if (_memseg->virt_addr == NULL) {
			dev_err(rp->dev, "ispv4 ioremap region %d failed\n", i);
			release_mem_region(iores->start, resource_size(iores));
			return -EINVAL;
		}
		rproc_coredump_add_segment(rp->rproc, _memseg->dev_addr,
					   _memseg->size);
		_memseg->flag |= XM_RPROC_MEM_FLAG_MMU_MAPED;
	}

	// rproc attach reg ioremap , pcie set
	iores = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!iores) {
		dev_err(rp->dev, "ispv4 get resource 2 info failed\n");
		return -EINVAL;
	}
	_memseg = &memseg[ISPV4_MEM_ATTACH_REG];
	_memseg->phys_addr = iores->start;
	_memseg->virt_addr = ioremap(_memseg->phys_addr, _memseg->size);
	dev_info(rp->dev, "ispv4 map %d: va=0x%x pa=0x%x", i, _memseg->virt_addr,
		 _memseg->phys_addr);
	if (_memseg->virt_addr == NULL) {
		dev_err(rp->dev, "ispv4 ioremap region %d failed\n", i);
		release_mem_region(iores->start, resource_size(iores));
		return -EINVAL;
	}
	_memseg->flag |= XM_RPROC_MEM_FLAG_MMU_MAPED;

#ifdef USING_PCIE_SMMU
	domain = iommu_get_domain_for_dev(rp->dev->parent);
	if (!domain) {
		dev_err(rp->dev, "smmu domain acquire failed\n");
		return -EINVAL;
	}

	for (i = AP_MEM_RPMSG_VRING; i <= AP_MEM_RPMSG_BUF; i++) {
		_memseg = &memseg[i];
		if (iommu_map(domain, _memseg->dma_addr, _memseg->phys_addr,
			      _memseg->size, iommu_flag) < 0) {
			dev_err(rp->dev, "smmu map %d failed err=%d\n", i, ret);
			return -EINVAL;
		} else {
			_memseg->flag |= XM_RPROC_MEM_FLAG_IOMMU_MAPED;
			dev_info(rp->dev, "smmu map %d success %d!!\n", i, ret);
		}
	}

	// TODO: for test!!!
	//ispv4_iommu_test_init();
#endif

	np = rp->dev->of_node;
	if (np == NULL) {
		dev_err(rp->dev, "rproc do not have dts node\n");
		return -EINVAL;
	}

	ret = of_reserved_mem_device_init(rp->dev);
	if (ret < 0) {
		dev_err(rp->dev, "memory init failed\n");
		return ret;
	}

	/* Protect rpmsg kick mbox */
	/**
	 * context 1: kick  --use
	 * context 2: stop  --set to null
	 * context 3: attach  --set to !null
	*/
	mutex_init(&rp->send_mbox_lock);

	return 0;
}

void mbox_rpmsg_rx_callback(struct mbox_client *cl, void *mssg)
{
	xm_ispv4_rpmsg_irq(0, platform_get_drvdata(container_of(
				      cl->dev, struct platform_device, dev)));
};

// Call from ispv4 boot.
static int ispv4_rproc_pci_boot(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s\n", __FUNCTION__);
	return 0;
}

void ispv4_rproc_pci_earlydown(struct xm_ispv4_rproc *rp)
{
	/* First release mbox to avoid `wake rpmsg thread` */
#ifdef RPMSG_USE_MBOX
	mutex_lock(&rp->send_mbox_lock);
	if (!IS_ERR_OR_NULL(rp->mbox_rpmsg_chan)) {
		mbox_free_channel(rp->mbox_rpmsg_chan);
		rp->mbox_rpmsg_chan = NULL;
	}
	mutex_unlock(&rp->send_mbox_lock);
#endif
	xm_ispv4_rpmsg_stopdeal(rp);
	dev_info(rp->dev, "%s\n", __FUNCTION__);
	return;
}

static void ispv4_rproc_pci_deboot(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s\n", __FUNCTION__);
	return;
}

static void ispv4_rproc_pci_deinit(struct xm_ispv4_rproc *rp)
{
	struct iommu_domain *domain;
	struct xm_ispv4_rproc_mem *_memseg;
	int i;

	//ispv4_iommu_test_deinit();
	of_reserved_mem_device_release(rp->dev);

	domain = iommu_get_domain_for_dev(rp->dev->parent);

	for (i = AP_MEM_RPMSG_VRING; i <= AP_MEM_RPMSG_BUF; i++) {
		_memseg = &memseg[i];
		if (_memseg->flag & XM_RPROC_MEM_FLAG_IOMMU_MAPED) {
			dev_info(rp->dev, "iommu unmap %d\n", i);
			iommu_unmap(domain, _memseg->dma_addr, _memseg->size);
			_memseg->flag &= ~XM_RPROC_MEM_FLAG_IOMMU_MAPED;
		}
	}

	for (i = ISPV4_MEM_SRAM; i <= ISPV4_MEM_ATTACH_REG; i++) {
		_memseg = &memseg[i];
		if (_memseg->flag & XM_RPROC_MEM_FLAG_MMU_MAPED) {
			iounmap(_memseg->virt_addr);
			release_mem_region(_memseg->phys_addr, _memseg->size);
		}
	}
	rproc_coredump_cleanup(rp->rproc);
}

static void ispv4_rproc_pci_shutdown(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s", __FUNCTION__);
	return;
}

static void ispv4_rproc_pci_remove(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s", __FUNCTION__);
	return;
}

struct xm_ispv4_ops ispv4_pci_ops = {
	.init = ispv4_rproc_pci_init,
	.deinit = ispv4_rproc_pci_deinit,
	.boot = ispv4_rproc_pci_boot,
	.earlydown = ispv4_rproc_pci_earlydown,
	.deboot = ispv4_rproc_pci_deboot,
	.remove = ispv4_rproc_pci_remove,
	.shutdown = ispv4_rproc_pci_shutdown,
	.rproc_ops = &ispv4_core_ops,
};

MODULE_LICENSE("GPL v2");
