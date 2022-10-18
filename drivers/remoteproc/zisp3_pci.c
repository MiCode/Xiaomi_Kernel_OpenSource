// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Xiaomi, Inc.
 */

#include <linux/completion.h>
#include <linux/mfd/ispv3_dev.h>
#include <linux/miscdevice.h>
#include <linux/iommu.h>
#include <linux/uaccess.h>

#include "zisp_rproc_utils.h"

#define ZISPV3_MEM_NR 1

#define ZISPV3_AIO_TIME0		0xe40004
#define ZISPV3_AIO_TIME1		0xe40008
#define ZISPV3_AIO_TIME2		0xe4000c
#define ZISPV3_AIO_TIME3		0xe40010

#define ZISPV3_AIO0			0xe40054
#define ZISPV3_AIO1			0xe40058
#define ZISPV3_AIO2			0xe4005c
#define ZISPV3_AIO3			0xe40060
#define ZISPV3_AIO4			0xe40064
#define ZISPV3_MCU_CLK_EN		0xe80000
#define ZISPV3_MCU_CLK_RST		0xe80200
#define ZISPV3_MCU_PD3			0xe82000
#define ZISPV3_AP_SWINT_GPIO_EN		0xea0700
#define ZISPV3_SRAM_DP			0xee0014
#define ZISPV3_SRAM_EN			0xee0018
#define ZISPV3_AP_SWINT_EN		0xef0104
#define ZISPV3_AP_SWINT_STATUS		0xef0108
#define ZISPV3_MCU_WP			0xef010c
#define ZISPV3_AP_SWINT_CAUSE		0xef0310
#define ZISPV3_MCU_SWINT		0xef030c
#define ZISPV3_AP_SWINT_INTA_SEL	0xef0604
#define ZISPV3_MCU_PD1			0xef0304
#define ZISPV3_MCU_PD2			0xef0300
#define ZISPV3_MCU_ISO			0xef0000

struct mem_segment {
	dma_addr_t dma_addr;
	dma_addr_t offset;
	resource_size_t size;
};

static struct zisp_rproc_mem memlist[ZISPV3_MEM_NR];

static struct mem_segment zispv3_mem_segment[ZISPV3_MEM_NR] = {
	{
		0x7f700000, 0, 300 * 1024,
	},
};

#if IS_ENABLED(CONFIG_RPMSG_ZISPV3)
/* 0x80880000 is ddr and dma addr for ipc coherent memory, care of dtsi */

static struct mem_segment zispv3_ipc_segment = {
	0x80880000, 0x80880000, 64 * 1024
};
#endif

static bool zispv3_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ZISPV3_AIO_TIME0:
	case ZISPV3_AIO_TIME1:
	case ZISPV3_AIO_TIME2:
	case ZISPV3_AIO_TIME3:
	case ZISPV3_AIO0:
	case ZISPV3_AIO1:
	case ZISPV3_AIO2:
	case ZISPV3_AIO3:
	case ZISPV3_AIO4:
	case ZISPV3_MCU_CLK_EN:
	case ZISPV3_MCU_CLK_RST:
	case ZISPV3_MCU_PD1:
	case ZISPV3_MCU_PD2:
	case ZISPV3_MCU_PD3:
	case ZISPV3_MCU_ISO:
	case ZISPV3_AP_SWINT_GPIO_EN:
	case ZISPV3_SRAM_DP:
	case ZISPV3_SRAM_EN:
	case ZISPV3_AP_SWINT_EN:
	case ZISPV3_AP_SWINT_STATUS:
	case ZISPV3_MCU_WP:
	case ZISPV3_AP_SWINT_CAUSE:
	case ZISPV3_MCU_SWINT:
	case ZISPV3_AP_SWINT_INTA_SEL:
		return true;
	default:
		return false;
	}
}

static int zisp3_mcu_parse_regmap(struct platform_device *pdev,
				  void __iomem *regs)
{
	int ret;
	struct ispv3_data *data;
	struct zisp_rproc *zisp_rproc;
	struct regmap_config reg_config = {0};

	zisp_rproc = platform_get_drvdata(pdev);
	reg_config.reg_bits = 32;
	reg_config.reg_stride = 4;
	reg_config.val_bits = 32;
	reg_config.max_register = MIPORT_MAX_REG_ADDR - 4;
	reg_config.readable_reg = zispv3_readable_register;
	reg_config.writeable_reg = zispv3_readable_register;

	if (!regs) {
		data = dev_get_drvdata(pdev->dev.parent);
		regs = data->base;
		if (IS_ERR(regs))
			return PTR_ERR(regs);
	}

	zisp_rproc->regmap = regmap_init_mmio(
					      &pdev->dev, regs,
					      &reg_config);
	if (IS_ERR(zisp_rproc->regmap)) {
		ret = PTR_ERR(zisp_rproc->regmap);
		dev_err(&pdev->dev, "get regmap failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int zisp3_mcu_release_mem(struct platform_device *pdev,
			         struct resource **_iores)
{
	int i;
	struct zisp_rproc_mem *mem;
	struct rproc *rproc;
	struct zisp_rproc *zisp_rproc;
	struct resource *iores;

	zisp_rproc = platform_get_drvdata(pdev);
	rproc = zisp_rproc->rproc;

        rproc_coredump_cleanup(rproc);

	for (i = 0 ; i < ZISPV3_MEM_NR; ++i) {
		if (!_iores) {
			iores = platform_get_resource(pdev, IORESOURCE_MEM, i);

			if (!iores) {
				pr_info("get resource %d info failed", i);
				return -EINVAL;
			}
		} else {
			iores = _iores[i];
		}

		mem = &zisp_rproc->memlist[i];

		iounmap(mem->virt_addr);

		mem->phys_addr = 0x0;
		mem->size = 0;
		mem->dma_addr = 0x0;
		mem->virt_addr = NULL;

		release_mem_region(iores->start, resource_size(iores));
	}

	return 0;
}

static int zisp3_mcu_parse_mem(struct platform_device *pdev,
			       struct resource **_iores)
{
	int i;
	struct zisp_rproc_mem *mem;
	struct rproc *rproc;
	struct zisp_rproc *zisp_rproc;
	struct resource *iores;

	zisp_rproc = platform_get_drvdata(pdev);
	zisp_rproc->memlist = memlist;
	rproc = zisp_rproc->rproc;

	for (i = 0 ; i < ZISPV3_MEM_NR; ++i) {
		if (!_iores) {
			iores = platform_get_resource(pdev, IORESOURCE_MEM, i);

			if (!iores) {
				pr_info("get resource %d info failed", i);
				return -EINVAL;
			}
		} else {
			iores = _iores[i];
		}

		if (!request_mem_region(iores->start, resource_size(iores),
					pdev->name)) {
			pr_info("request region %d failed", i);
			return -EINVAL;
		}

		mem = &zisp_rproc->memlist[i];

		mem->phys_addr = iores->start + zispv3_mem_segment[i].offset;
		mem->size = zispv3_mem_segment[i].size;
		mem->dma_addr = zispv3_mem_segment[i].dma_addr;
		mem->virt_addr = ioremap_wc(mem->phys_addr, mem->size);

		rproc_coredump_add_segment(rproc, mem->dma_addr, mem->size);
	}

#if IS_ENABLED(CONFIG_RPMSG_ZISPV3)
	zisp_rproc->ipc_membase = zispv3_ipc_segment.offset;
	zisp_rproc->ipc_dma_addr = zispv3_ipc_segment.dma_addr;
	zisp_rproc->ipc_size = zispv3_ipc_segment.size;
#endif

	return 0;
}

static int zisp3_elf_load_segments(struct rproc *rproc, const struct firmware *fw)
{
	struct platform_device *pdev;
	struct zisp_rproc *zisp_rproc = rproc->priv;

	pdev = to_platform_device(zisp_rproc->dev);

	if (zisp3_mcu_parse_mem(pdev, NULL))
		return -EINVAL;

	return rproc_elf_load_segments(rproc, fw);
}

static int zisp3_rproc_start(struct rproc *rproc)
{
	int ret = 0;
	struct platform_device *pdev;
	struct zisp_rproc *zisp_rproc = rproc->priv;

	pdev = to_platform_device(zisp_rproc->dev);

	if (zisp3_mcu_parse_regmap(pdev, NULL)) {
		rproc_coredump_cleanup(zisp_rproc->rproc);
		zisp3_mcu_release_mem(pdev, NULL);
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_RPMSG_ZISPV3)
	struct zisp_rproc_ipi *ipi = &zisp_rproc->ipi;

	ret = zisp_rproc_ipi_setup(ipi, rproc);
	if (ret < 0)
		return ret;

	/*
	 * request the irq let the handler calls ipi->rx_callback
	 */

	ret = request_irq(ipi->irq, ipi->rx_callback, 0,
			  dev_name(zisp_rproc->dev), ipi);
#endif

	/*
	 * setting boot address
	 */

	if ((rproc->bootaddr & 0xfff) > 0x800)
		rproc->bootaddr += 0x1000;
	regmap_write(zisp_rproc->regmap, ZISPV3_AIO0,
			(rproc->bootaddr & 0xf000) | 0x137);
	regmap_write(zisp_rproc->regmap, ZISPV3_AIO1,
		     rproc->bootaddr >> 16);
	regmap_write(zisp_rproc->regmap, ZISPV3_AIO2, 0x113);
	regmap_write(zisp_rproc->regmap, ZISPV3_AIO3,
		     (rproc->bootaddr & 0xfff) << 4 | 0x1);
	regmap_write(zisp_rproc->regmap, ZISPV3_AIO4, 0x9102);
	regmap_write(zisp_rproc->regmap, ZISPV3_AP_SWINT_CAUSE,
		     rproc->bootaddr);

	/*
	 * enable mcu clock & start
	 */
	regmap_write(zisp_rproc->regmap, ZISPV3_MCU_PD1, 0x7e);
	regmap_write(zisp_rproc->regmap, ZISPV3_MCU_PD2, 0x7e);
	regmap_write(zisp_rproc->regmap, ZISPV3_MCU_PD3, 0xffffff01);

	regmap_write(zisp_rproc->regmap, ZISPV3_MCU_WP, 0xfffff000);
	regmap_write(zisp_rproc->regmap, ZISPV3_MCU_CLK_EN, 0xffffc001);
	regmap_write(zisp_rproc->regmap, ZISPV3_MCU_CLK_RST, 0xffffc001);

	return ret;
}

static int zisp3_rproc_stop(struct rproc *rproc)
{
	/*do your mcu stop */
	struct zisp_rproc *zisp_rproc;
	struct platform_device *pdev;

	zisp_rproc = rproc->priv;
	pdev = to_platform_device(zisp_rproc->dev);

	regmap_exit(zisp_rproc->regmap);
	zisp_rproc->regmap = NULL;
	zisp3_mcu_release_mem(pdev, NULL);
#if IS_ENABLED(CONFIG_RPMSG_ZISPV3)
	zisp_rproc_ipi_teardown(&zisp_rproc->ipi);
#endif
	return 0;
}

static void zisp3_rproc_kick(struct rproc *rproc, int vqid)
{
	/* do your kick msg here */

#if IS_ENABLED(CONFIG_RPMSG_ZISPV3)
	struct zisp_rproc *zisp_rproc;

	zisp_rproc = rproc->priv;
	regmap_write(zisp_rproc->regmap, ZISPV3_MCU_SWINT, 0x1);
#endif
}

static void *zisp3_mcu_rproc_da_to_va(struct rproc *rproc, u64 da, size_t len, bool *is_iomem)
{
	int i;
	s64 offset;
	struct zisp_rproc *zisp_rproc;
	struct zisp_rproc_mem *memlist;

	zisp_rproc = rproc->priv;
	memlist = zisp_rproc->memlist;

	for (i = 0; i < ZISPV3_MEM_NR; ++i) {
		offset = da - memlist[i].dma_addr;
		if (offset >= 0 && offset + len <= memlist[i].size)
			return memlist[i].virt_addr + offset;
	}

	return NULL;
}

static void zisp3_mcu_shutdown(struct zisp_rproc *zisp_rproc)
{
	dev_info(zisp_rproc->dev, "AP shutdown ZISP3 MCU\n");

	/* do your shutdown here */
}

static void zisp3_mcu_remove(struct zisp_rproc *zisp_rproc)
{
	dev_info(zisp_rproc->dev, "AP remove ZISP3 MCU\n");

	/* do your remove here */

#if IS_ENABLED(CONFIG_RPMSG_ZISPV3)
	dma_release_declared_memory(zisp_rproc->dev);
#endif
}

static int zisp3_mcu_parse_irq(struct platform_device *pdev,
			       int irq)
{
	struct zisp_rproc *zisp_rproc;

	zisp_rproc = platform_get_drvdata(pdev);
	memset(&zisp_rproc->ipi, 0, sizeof(zisp_rproc->ipi));

	if (irq < 0) {
		irq = platform_get_irq(pdev, 0);

		if (irq >= 0) {
			zisp_rproc->ipi.irq = irq;
			return 0;
		}
	} else {
		zisp_rproc->ipi.irq = irq;
		return 0;
	}

	return -EINVAL;
}

static int zisp3_mcu_init(struct zisp_rproc *zisp_rproc)
{
	struct platform_device *pdev;
	struct ispv3_data *data;
	int ret = 0;

	pdev = to_platform_device(zisp_rproc->dev);
	data = dev_get_drvdata(pdev->dev.parent);
	data->rproc = (void *)zisp_rproc->rproc;

	zisp_rproc->rproc->ops->load = zisp3_elf_load_segments;
#if IS_ENABLED(CONFIG_RPMSG_ZISPV3)
	if (zisp3_mcu_parse_irq(pdev, -1))
		return -EINVAL;
#endif

#if IS_ENABLED(CONFIG_RPMSG_ZISPV3)
	struct iommu_domain *domain;

	domain = iommu_get_domain_for_dev(zisp_rproc->dev->parent);
	if (!domain) {
		dev_err(zisp_rproc->dev, "smmu domain acquire failed\n");
		return -EINVAL;
	}

	if (iommu_map(domain, zisp_rproc->ipc_dma_addr,
		      zisp_rproc->ipc_membase,
		      zisp_rproc->ipc_size,
		      IOMMU_READ | IOMMU_WRITE | IOMMU_NOEXEC |
		      IOMMU_MMIO) < 0) {
		dev_err(zisp_rproc->dev, "smmu map failed\n");
		return -EINVAL;
	}

	ret = dma_declare_coherent_memory(zisp_rproc->dev,
					  zisp_rproc->ipc_membase,
					  zisp_rproc->ipc_dma_addr,
					  zisp_rproc->ipc_size,
					  DMA_MEMORY_EXCLUSIVE);
	if (ret < 0) {
		dev_err(zisp_rproc->dev, "memory init failed\n");
		iommu_unmap(domain, zisp_rproc->ipc_membase,
			    zisp_rproc->ipc_size);
		return ret;
	}
#endif
	return ret;
}

static struct rproc_ops zisp3_rproc_ops = {
	.start = zisp3_rproc_start,
	.stop = zisp3_rproc_stop,
	.kick = zisp3_rproc_kick,
	.da_to_va = zisp3_mcu_rproc_da_to_va,
};

struct zisp_ops zisp3_ops_pci = {
	.init = zisp3_mcu_init,
	.shutdown = zisp3_mcu_shutdown,
	.remove = zisp3_mcu_remove,
	.parse_regmap = zisp3_mcu_parse_regmap,
	.parse_mem = zisp3_mcu_parse_mem,
	.parse_irq = zisp3_mcu_parse_irq,
	.rproc_ops = &zisp3_rproc_ops,
#ifdef CONFIG_ZISP_OCRAM_AON
	.firmware = "nuttx_aon",
#else
	.firmware = "nuttx_naon",
#endif
};

MODULE_LICENSE("GPL v2");
