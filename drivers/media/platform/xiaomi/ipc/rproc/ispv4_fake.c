// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#include "ispv4_rproc.h"
#include <linux/dma-map-ops.h>
#include <linux/iommu.h>
#include <linux/of_reserved_mem.h>
#include <linux/debugfs.h>

#define XM_ISPV4_MEM_NR 5
#define XM_ISPV4_FAKE_MEM_NR 2

#define ISPV4_MEM_SRAM 0
#define ISPV4_MEM_DDR 1
#define AP_MEM_RPMSG_VRING 2
#define AP_MEM_RPMSG_BUF 3
#define AP_MEM_RAMLOG_BUF 4

// va pa da remote_a size flag
static struct xm_ispv4_rproc_mem memseg[XM_ISPV4_MEM_NR] = {
	// ispv4 sram.
	{ NULL, 0, 0, 0x00000000, 512 * 1024, XM_RPROC_MEM_FLAG_DEV_MEM },
	// ispv4 ddr.
	{ NULL, 0, 0, 0x80000000, 512 * 1024, XM_RPROC_MEM_FLAG_DEV_MEM },
	// rpmsg vring.
	{ NULL, 0xea020000, 0, 0, 0x100000, XM_RPROC_MEM_FLAG_AP_MEM },
	// rpmsg buffer.
	{ NULL, 0xea000000, 0, 0, 0x20000, XM_RPROC_MEM_FLAG_AP_MEM },
	// ramlog dma buffer.
	{ NULL, 0, 0, 0, 512 * 1024, XM_RPROC_MEM_FLAG_AP_MEM },
};

static int ispv4_rproc_start(struct rproc *rproc)
{
	dev_info(&rproc->dev, "ispv4: %s", __FUNCTION__);
	return 0;
}

static int ispv4_mcu_rproc_attach(struct rproc *rproc)
{
	struct xm_ispv4_rproc *rp = rproc->priv;
	dev_info(&rproc->dev, "ispv4: %s\n", __FUNCTION__);
	ispv4_load_rsc_table(rproc, rp->fw);
	dev_info(&rproc->dev, "ispv4: attach finish\n");

	return 0;
}

static int ispv4_rproc_stop(struct rproc *rproc)
{
	dev_info(&rproc->dev, "ispv4: %s", __FUNCTION__);
	dev_info(&rproc->dev, "ispv4: stop finish\n");
	return 0;
}

static void ispv4_rproc_kick(struct rproc *rproc, int vqid)
{
	dev_info(&rproc->dev, "ispv4: %s", __FUNCTION__);
	return;
}

static void *ispv4_mcu_rproc_da_to_va(struct rproc *rproc, u64 da, size_t len,
				      bool *is_iomem)
{
	int i;
	s64 offset;
	struct xm_ispv4_rproc_mem *_memseg;
	void *va;

	for (i = 0; i < XM_ISPV4_FAKE_MEM_NR; i++) {
		_memseg = &memseg[i];
		offset = da - _memseg->dev_addr;
		if (offset >= 0 && offset + len <= _memseg->size) {
			va = _memseg->virt_addr + offset;
			dev_info(&rproc->dev,
				 "ispv4 get mem da=0x%llx, va=0x%llx\n", da,
				 va);
			return va;
		}
	}
	dev_err(&rproc->dev, "da to va failed!\n");

	return NULL;
}

static struct rproc_ops ispv4_core_ops = {
	.start = ispv4_rproc_start,
	.stop = ispv4_rproc_stop,
	.kick = ispv4_rproc_kick,
	.da_to_va = ispv4_mcu_rproc_da_to_va,
	.attach = ispv4_mcu_rproc_attach,
};

static int ispv4_rproc_fake_init(struct xm_ispv4_rproc *rp)
{
	// struct iommu_domain *domain;
	struct xm_ispv4_rproc_mem *_memseg;
	struct resource *iores;
	int ret, i;
	// int iommu_flag;
	struct device_node *np;
	struct platform_device *pdev;

	// iommu_flag = IOMMU_READ|IOMMU_WRITE|IOMMU_NOEXEC|IOMMU_MMIO;

	pdev = container_of(rp->dev, struct platform_device, dev);

	for (i = ISPV4_MEM_SRAM; i <= ISPV4_MEM_DDR; i++) {
		_memseg = &memseg[i];
		iores = platform_get_resource(pdev, IORESOURCE_DMA, i);
		if (!iores) {
			dev_err(rp->dev, "ispv4 get resource %d info failed\n",
				i);
			return -EINVAL;
		}
		// if (!request_mem_region(iores->start, resource_size(iores),
		// 			pdev->name)) {
		// 	dev_err(rp->dev, "ispv4 request region:%d failed\n", i);
		// 	dev_err(rp->dev, "ispv4 region start:0x%x size:0x%x\n",
		// 		iores->start, resource_size(iores));
		// 	return -EINVAL;
		// }
		dev_info(rp->dev, "ispv4 request region start:0x%x, size:0x%x",
			 iores->start, resource_size(iores));
		// if (resource_size(iores) != _memseg->size) {
		// 	dev_err(rp->dev, "ispv4 request region %d size not match\n", i);
		// 	dev_err(rp->dev, "ispv4 size: %x&%x(resource)\n", _memseg->size,
		// 		resource_size(iores));
		// 	release_mem_region(iores->start, resource_size(iores));
		// 	return -EINVAL;
		// }
		_memseg->phys_addr = iores->start;
		// _memseg->virt_addr = ioremap_wc(_memseg->phys_addr, _memseg->size);
		_memseg->virt_addr = phys_to_virt(_memseg->phys_addr);
		dev_info(rp->dev, "mmap pa:0x%x, va:0x%x\n", _memseg->phys_addr,
			 _memseg->virt_addr);
		if (_memseg->virt_addr == NULL) {
			dev_err(rp->dev, "ispv4 ioremap region %d failed\n", i);
			release_mem_region(iores->start, resource_size(iores));
			return -EINVAL;
		}
		rproc_coredump_add_segment(rp->rproc, _memseg->dev_addr,
					   _memseg->size);
		_memseg->flag |= XM_RPROC_MEM_FLAG_MMU_MAPED;
	}

	// domain = iommu_get_domain_for_dev(rp->dev->parent);
	// if (!domain) {
	// 	dev_err(rp->dev, "smmu domain acquire failed\n");
	// 	return -EINVAL;
	// }

	// for (i=AP_MEM_RPMSG_VRING; i<=AP_MEM_RPMSG_BUF; i++) {
	// 	_memseg = &memseg[i];
	// 	if (iommu_map(domain, _memseg->dma_addr, _memseg->phys_addr,
	// 				_memseg->size, iommu_flag) < 0) {
	// 		dev_err(rp->dev, "smmu map failed\n");
	// 		return -EINVAL;
	// 	} else {
	// 		_memseg->flag |= XM_RPROC_MEM_FLAG_IOMMU_MAPED;
	// 	}
	// }

	np = rp->dev->of_node;
	if (np == NULL) {
		dev_err(rp->dev, "rproc do not have dts node\n");
		return -EINVAL;
	}

	ret = of_reserved_mem_device_init_by_idx(rp->dev, np, 1);
	if (ret < 0) {
		dev_err(rp->dev, "memory init failed\n");
		return ret;
	}

	// if (rp->ramlog_dma_sg != NULL) {
	// 	_memseg = &memseg[AP_MEM_RAMLOG_BUF];
	// 	_memseg->size =
	// 		iommu_map_sg(domain, _memseg->dma_addr, rp->ramlog_dma_sg,
	// 		rp->ramlog_dma_sg_nents, iommu_flag);
	// 	_memseg->flag |= XM_RPROC_MEM_FLAG_IOMMU_MAPED;
	// }

	ret = xm_ispv4_rpmsg_init(rp);
	if (ret != 0) {
		dev_err(rp->dev, "ispv4: rpmsg init failed\n");
		return ret;
	}

	return 0;
}

// Call from ispv4 boot.
static int ispv4_rproc_fake_boot(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s\n", __FUNCTION__);
	return 0;
}

static void ispv4_rproc_fake_deboot(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s\n", __FUNCTION__);
	return;
}

static void ispv4_rproc_fake_deinit(struct xm_ispv4_rproc *rp)
{
	struct iommu_domain *domain;
	struct xm_ispv4_rproc_mem *_memseg;
	int i;

	xm_ispv4_rpmsg_exit(rp);

	of_reserved_mem_device_release(rp->dev);

	domain = iommu_get_domain_for_dev(rp->dev->parent);

	for (i = AP_MEM_RPMSG_VRING; i <= AP_MEM_RAMLOG_BUF; i++) {
		_memseg = &memseg[i];
		if (_memseg->flag & XM_RPROC_MEM_FLAG_IOMMU_MAPED) {
			iommu_unmap(domain, _memseg->dma_addr, _memseg->size);
			_memseg->flag &= ~XM_RPROC_MEM_FLAG_IOMMU_MAPED;
		}
	}

	for (i = ISPV4_MEM_SRAM; i <= ISPV4_MEM_DDR; i++) {
		_memseg = &memseg[i];
		if (_memseg->flag & XM_RPROC_MEM_FLAG_MMU_MAPED) {
			// iounmap(_memseg->virt_addr);
			release_mem_region(_memseg->phys_addr, _memseg->size);
		}
	}
	rproc_coredump_cleanup(rp->rproc);
}

static void ispv4_rproc_fake_shutdown(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s", __FUNCTION__);
	return;
}

static void ispv4_rproc_fake_remove(struct xm_ispv4_rproc *rp)
{
	dev_info(rp->dev, "%s", __FUNCTION__);
	return;
}

struct xm_ispv4_ops ispv4_fake_ops = {
	.init = ispv4_rproc_fake_init,
	.deinit = ispv4_rproc_fake_deinit,
	.boot = ispv4_rproc_fake_boot,
	.deboot = ispv4_rproc_fake_deboot,
	.remove = ispv4_rproc_fake_remove,
	.shutdown = ispv4_rproc_fake_shutdown,
	.rproc_ops = &ispv4_core_ops,
};

MODULE_LICENSE("GPL v2");
