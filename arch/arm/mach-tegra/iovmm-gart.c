/*
 * arch/arm/mach-tegra/iovmm-gart.c
 *
 * Tegra I/O VMM implementation for GART devices in Tegra and Tegra 2 series
 * systems-on-a-chip.
 *
 * Copyright (c) 2010-2012 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/io.h>

#include <asm/cacheflush.h>

#include <mach/iovmm.h>

#define GART_CONFIG		0x24
#define GART_ENTRY_ADDR		0x28
#define GART_ENTRY_DATA		0x2c

#define VMM_NAME "iovmm-gart"
#define DRIVER_NAME "tegra_gart"

#define GART_PAGE_SHIFT		12
#define GART_PAGE_SIZE		(1 << GART_PAGE_SHIFT)
#define GART_PAGE_MASK		(~(GART_PAGE_SIZE - 1))

struct gart_device {
	void __iomem		*regs;
	u32			*savedata;
	u32			page_count; /* total remappable size */
	tegra_iovmm_addr_t	iovmm_base; /* offset to apply to vmm_area */
	spinlock_t		pte_lock;
	struct tegra_iovmm_device iovmm;
	struct tegra_iovmm_domain domain;
	bool			enable;
};

/*
 * Any interaction between any block on PPSB and a block on APB or AHB
 * must have these read-back to ensure the APB/AHB bus transaction is
 * complete before initiating activity on the PPSB block.
 */
#define FLUSH_GART_REGS(gart)	(void)readl((gart)->regs + GART_CONFIG)

static inline void gart_set_pte(struct gart_device *gart,
				tegra_iovmm_addr_t offs, u32 pte)
{
	writel(offs, gart->regs + GART_ENTRY_ADDR);
	writel(pte, gart->regs + GART_ENTRY_DATA);
}

static int gart_map(struct tegra_iovmm_domain *, struct tegra_iovmm_area *);
static void gart_unmap(struct tegra_iovmm_domain *,
	struct tegra_iovmm_area *, bool);
static void gart_map_pfn(struct tegra_iovmm_domain *,
	struct tegra_iovmm_area *, unsigned long, unsigned long);
static struct tegra_iovmm_domain *gart_alloc_domain(
	struct tegra_iovmm_device *, struct tegra_iovmm_client *);

static int gart_probe(struct platform_device *);
static int gart_remove(struct platform_device *);
static int gart_suspend(struct tegra_iovmm_device *dev);
static void gart_resume(struct tegra_iovmm_device *dev);


static struct tegra_iovmm_device_ops tegra_iovmm_gart_ops = {
	.map		= gart_map,
	.unmap		= gart_unmap,
	.map_pfn	= gart_map_pfn,
	.alloc_domain	= gart_alloc_domain,
	.suspend	= gart_suspend,
	.resume		= gart_resume,
};

static struct platform_driver tegra_iovmm_gart_drv = {
	.probe		= gart_probe,
	.remove		= gart_remove,
	.driver		= {
		.name	= DRIVER_NAME,
	},
};

static int gart_suspend(struct tegra_iovmm_device *dev)
{
	struct gart_device *gart = container_of(dev, struct gart_device, iovmm);
	unsigned int i;
	unsigned long reg;

	if (!gart)
		return -ENODEV;

	if (!gart->enable)
		return 0;

	spin_lock(&gart->pte_lock);
	reg = gart->iovmm_base;
	for (i = 0; i < gart->page_count; i++) {
		writel(reg, gart->regs + GART_ENTRY_ADDR);
		gart->savedata[i] = readl(gart->regs + GART_ENTRY_DATA);
		reg += GART_PAGE_SIZE;
	}
	spin_unlock(&gart->pte_lock);
	return 0;
}

static void do_gart_setup(struct gart_device *gart, const u32 *data)
{
	unsigned long reg;
	unsigned int i;

	reg = gart->iovmm_base;
	for (i = 0; i < gart->page_count; i++) {
		gart_set_pte(gart, reg, data ? data[i] : 0);
		reg += GART_PAGE_SIZE;
	}
	writel(1, gart->regs + GART_CONFIG);
	FLUSH_GART_REGS(gart);
}

static void gart_resume(struct tegra_iovmm_device *dev)
{
	struct gart_device *gart = container_of(dev, struct gart_device, iovmm);

	if (!gart || !gart->enable || (gart->enable && !gart->savedata))
		return;

	spin_lock(&gart->pte_lock);
	do_gart_setup(gart, gart->savedata);
	spin_unlock(&gart->pte_lock);
}

static int gart_remove(struct platform_device *pdev)
{
	struct gart_device *gart = platform_get_drvdata(pdev);

	if (!gart)
		return 0;

	if (gart->enable)
		writel(0, gart->regs + GART_CONFIG);

	gart->enable = 0;
	platform_set_drvdata(pdev, NULL);
	tegra_iovmm_unregister(&gart->iovmm);
	if (gart->savedata)
		vfree(gart->savedata);
	if (gart->regs)
		iounmap(gart->regs);
	kfree(gart);
	return 0;
}

static int gart_probe(struct platform_device *pdev)
{
	struct gart_device *gart;
	struct resource *res, *res_remap;
	void __iomem *gart_regs;
	int e;

	if (!pdev) {
		pr_err(DRIVER_NAME ": platform_device required\n");
		return -ENODEV;
	}

	if (PAGE_SHIFT != GART_PAGE_SHIFT) {
		pr_err(DRIVER_NAME ": GART and CPU page size must match\n");
		return -ENXIO;
	}

	/* the GART memory aperture is required */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res_remap = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	if (!res || !res_remap) {
		pr_err(DRIVER_NAME ": GART memory aperture expected\n");
		return -ENXIO;
	}
	gart = kzalloc(sizeof(*gart), GFP_KERNEL);
	if (!gart) {
		pr_err(DRIVER_NAME ": failed to allocate tegra_iovmm_device\n");
		return -ENOMEM;
	}

	gart_regs = ioremap(res->start, res->end - res->start + 1);
	if (!gart_regs) {
		pr_err(DRIVER_NAME ": failed to remap GART registers\n");
		e = -ENXIO;
		goto fail;
	}

	gart->iovmm.name = VMM_NAME;
	gart->iovmm.ops = &tegra_iovmm_gart_ops;
	gart->iovmm.pgsize_bits = GART_PAGE_SHIFT;
	spin_lock_init(&gart->pte_lock);

	platform_set_drvdata(pdev, gart);

	e = tegra_iovmm_register(&gart->iovmm);
	if (e)
		goto fail;

	e = tegra_iovmm_domain_init(&gart->domain, &gart->iovmm,
		(tegra_iovmm_addr_t)res_remap->start,
		(tegra_iovmm_addr_t)res_remap->end+1);
	if (e)
		goto fail;

	gart->regs = gart_regs;
	gart->iovmm_base = (tegra_iovmm_addr_t)res_remap->start;
	gart->page_count = resource_size(res_remap);
	gart->page_count >>= GART_PAGE_SHIFT;

	gart->savedata = vmalloc(sizeof(u32) * gart->page_count);
	if (!gart->savedata) {
		pr_err(DRIVER_NAME ": failed to allocate context save area\n");
		e = -ENOMEM;
		goto fail;
	}

	do_gart_setup(gart, NULL);
	gart->enable = 1;

	return 0;

fail:
	if (gart_regs)
		iounmap(gart_regs);
	if (gart && gart->savedata)
		vfree(gart->savedata);
	kfree(gart);
	return e;
}

static int __devinit gart_init(void)
{
	return platform_driver_register(&tegra_iovmm_gart_drv);
}

static void __exit gart_exit(void)
{
	platform_driver_unregister(&tegra_iovmm_gart_drv);
}

#define GART_PTE(_pfn) (0x80000000ul | ((_pfn) << PAGE_SHIFT))


static int gart_map(struct tegra_iovmm_domain *domain,
	struct tegra_iovmm_area *iovma)
{
	struct gart_device *gart =
		container_of(domain, struct gart_device, domain);
	unsigned long gart_page, count;
	unsigned int i;

	gart_page = iovma->iovm_start;
	count = iovma->iovm_length >> GART_PAGE_SHIFT;

	for (i = 0; i < count; i++) {
		unsigned long pfn;

		pfn = iovma->ops->lock_makeresident(iovma, i<<PAGE_SHIFT);
		if (!pfn_valid(pfn))
			goto fail;

		spin_lock(&gart->pte_lock);

		gart_set_pte(gart, gart_page, GART_PTE(pfn));
		FLUSH_GART_REGS(gart);
		gart_page += GART_PAGE_SIZE;

		spin_unlock(&gart->pte_lock);
	}

	return 0;

fail:
	spin_lock(&gart->pte_lock);
	while (i--) {
		iovma->ops->release(iovma, i << PAGE_SHIFT);
		gart_page -= GART_PAGE_SIZE;
		gart_set_pte(gart, gart_page, 0);
	}
	FLUSH_GART_REGS(gart);
	spin_unlock(&gart->pte_lock);

	return -ENOMEM;
}

static void gart_unmap(struct tegra_iovmm_domain *domain,
	struct tegra_iovmm_area *iovma, bool decommit)
{
	struct gart_device *gart =
		container_of(domain, struct gart_device, domain);
	unsigned long gart_page, count;
	unsigned int i;

	count = iovma->iovm_length >> GART_PAGE_SHIFT;
	gart_page = iovma->iovm_start;

	spin_lock(&gart->pte_lock);
	for (i = 0; i < count; i++) {
		if (iovma->ops && iovma->ops->release)
			iovma->ops->release(iovma, i << PAGE_SHIFT);

		gart_set_pte(gart, gart_page, 0);
		gart_page += GART_PAGE_SIZE;
	}
	FLUSH_GART_REGS(gart);
	spin_unlock(&gart->pte_lock);
}

static void gart_map_pfn(struct tegra_iovmm_domain *domain,
	struct tegra_iovmm_area *iovma, unsigned long offs,
	unsigned long pfn)
{
	struct gart_device *gart =
		container_of(domain, struct gart_device, domain);

	BUG_ON(!pfn_valid(pfn));
	spin_lock(&gart->pte_lock);
	gart_set_pte(gart, offs, GART_PTE(pfn));
	FLUSH_GART_REGS(gart);
	spin_unlock(&gart->pte_lock);
}

static struct tegra_iovmm_domain *gart_alloc_domain(
	struct tegra_iovmm_device *dev, struct tegra_iovmm_client *client)
{
	struct gart_device *gart = container_of(dev, struct gart_device, iovmm);
	return &gart->domain;
}

subsys_initcall(gart_init);
module_exit(gart_exit);
