/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/kernel.h>		/* Only for KERN_INFO */
#include <linux/err.h>		   /* Error macros */
#include <linux/fs.h>
#include <linux/init.h>		  /* Needed for the macros */
#include <linux/io.h>			/* IO macros */
#include <linux/device.h>		/* Device drivers need this */
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/ensigma_uccp330.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/regulator/consumer.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_bus.h>
#include <linux/clk/msm-clk.h>
#include <linux/iommu.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#define DRV_NAME_DEMOD "demod"
#define DRVDBG(fmt, args...)\
	pr_debug(DRV_NAME_DEMOD " %s():%d " fmt, __func__, __LINE__, ## args)
#define DRVERR(fmt, args...)\
	pr_err(DRV_NAME_DEMOD " %s():%d " fmt, __func__, __LINE__, ## args)

#define BASE_G32 0xB7000000
#define BASE_G24 0xB4000000
#define BASE_EXT 0xB0000000
#define EXTRAM_LIMIT 0x600000
#define GRAM_LIMIT 0x90000
#define META_RG 0x02000000
#define LOW20 0xFFFFF
#define TOP8 0xFF000000
#define TOP12 0xFFF00000
#define NIBLE_PTR 0xD0004
#define ACCESS_OFFS 0x40000
#define IA_VALUE 0x3E040
#define IA_ADDR  0x3E080
#define IA_STAT  0x3E0C0
#define BCSS_VBIF 0xE0
#define BCSSADDR ((unsigned int)drv->bcss_regs)
#define subsys_to_drv(d) container_of(d, struct venus_data, subsys_desc)
/* forward declarations */
static ssize_t demod_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t demod_open(struct inode *inode, struct file *filp);
static long demod_ioctl(struct file *, unsigned int, unsigned long);
static ssize_t demod_write(struct file *, const char *, size_t, loff_t *);
static int demod_release(struct inode *, struct file *);

/**
 * struct demod_clk_desc - demod clock descriptor
 *
 * @name:		DT entry
 * @enable:		should this driver enable or not
 * @rate:		1 means set a predefined rate, 0 means don't
 *			other value means rate to be set
 */
struct demod_clk_desc {
	const char *name;
	int enable;
	unsigned int rate;
};
static const struct demod_clk_desc demod_clocks[] = {
	{"core_clk_src", 0, 1},
	{"core_clk", 1, 0},
	{"core_x2_clk", 1, 1},
	{"core_div2_clk", 1, 1},
	{"iface_wrap_clk", 1, 0},
	{"iface_clk", 1, 0},
	{"bcc_vbif_dem_core_clk", 1, 0},
	{"vbif_core_clk", 1, 0},
	{"iface_vbif_clk", 1, 0},
	{"gram_clk", 0, 0},
	{"atv_x5_clk", 1, 122880000}
};

/**
 * struct demod_data - demod device descriptor
 *
 * @subsys:		sub-system habdle for pil operaion
 * @subsys_desc:		subsystem descriptor
 * @gdsc:		pointer to gdsc
 * @dev:		pointer to latform device
 * @clks:		array of device clocks
 * @cdev:		char device
 * @node		device tree node
 * @bus_perf_client: client for bus voting
 * @wakeup_src:	wake-up source
 * @bcss_regs:	virtual address of bcss registers
 * @top_bcss:	virtual address of top bcss registers
 * @ext_ram:	virtual_address of external demod ram
 * @read_base:	current read base for read() operation
 * @write_base:  currentt write base for write() operations
 * @mutex:		A mutex for mutual exclusion between API calls.
 * @ref_counter	reference counter
 */
struct demod_data {
	void *subsys;
	struct regulator *gdsc;
	struct device *dev;
	struct clk *clks[ARRAY_SIZE(demod_clocks)];
	struct cdev cdev;
	struct device_node *node;
	u32 bus_perf_client;
	struct wakeup_source wakeup_src;
	void *bcss_regs;
	void *top_bcss;
	void *ext_ram;
	unsigned int read_base;
	unsigned int write_base;
	struct mutex mutex;
	int ref_counter;
};
static struct class *demod_class;
static dev_t demod_minor;  /* next minor number to assign */

static const struct file_operations demod_fops = {
	.owner = THIS_MODULE,
	.read = demod_read,
	.write = demod_write,
	.open = demod_open,
	.release = demod_release,
	.unlocked_ioctl = demod_ioctl
};

/**
 * meta_indirect_read() - read core registers via indirect IF.
 *
 * @drv:	demod device
 * addr:	core reg address
 * pval:	pointer to store value
 */
static int meta_indirect_read(struct demod_data *drv, u32 addr, u32 *pval)
{
	int i;
	if (!pval)
		return -EINVAL;
	for (i = 0; i < 100 &&
		((readl_relaxed(BCSSADDR + IA_STAT) & 0x1000000) == 0); i++)
		udelay(10);
	if (i == 100) {
		DRVDBG("ioctl rw EACCES timeout\n");
		return -EACCES;
	}
	writel_relaxed(addr | 0x001, BCSSADDR + IA_ADDR);
	for (i = 0; i < 100 &&
		((readl_relaxed(BCSSADDR + IA_ADDR) & 0x1) != 0); i++)
		udelay(10);
	if (i == 100) {
		DRVDBG("ioctl rw EACCES timeout\n");
		return -EACCES;
	}
	*pval = readl_relaxed(BCSSADDR + IA_VALUE);
	return 0;
}

/**
 * meta_indirect_write() - write core registers via indirect IF.
 *
 * @drv:	demod device
 * addr:	core reg address
 * val:		value to write
 */
static int meta_indirect_write(struct demod_data *drv, u32 addr, u32 val)
{
	int i;
	for (i = 0; i < 100 &&
		((readl_relaxed(BCSSADDR + IA_STAT) & 0x1000000) == 0); i++)
		udelay(10);
	if (i == 100) {
		DRVDBG("ioctl rw EACCES timeout\n");
		return -EACCES;
	}
	writel_relaxed(addr, BCSSADDR + IA_ADDR);
	writel_relaxed(val, BCSSADDR + IA_VALUE);
	for (i = 0; i < 100 &&
		((readl_relaxed(BCSSADDR + IA_STAT) & 0x40000) != 0); i++)
		udelay(10);
	if (i == 100) {
		DRVDBG("ioctl rw EACCES timeout\n");
		return -EACCES;
	}
	return 0;
}
/**
 * demod_verify_access() - Verify demod memory access.
 *
 * @addr:	access address
 * @size:	size
 */
static int demod_verify_access(unsigned int addr, unsigned int size)
{
	if ((addr & TOP8) == BASE_EXT) {
		if (addr - BASE_EXT + size > EXTRAM_LIMIT)
			return -EACCES;
	} else if (((addr & LOW20) + size) > GRAM_LIMIT)
		return -EACCES;
	return 0;
}

/**
 * demod_clock_setup() - Get clocks and set rates.
 *
 * @dev:	DEMOD device.
 */
static int demod_clock_setup(struct device *dev)
{
	struct demod_data *drv = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(drv->clks); i++) {
		drv->clks[i] = devm_clk_get(dev, demod_clocks[i].name);
		if (IS_ERR(drv->clks[i])) {
			DRVERR("failed to get %s\n",
				demod_clocks[i].name);
			return PTR_ERR(drv->clks[i]);
		}
		/* Make sure rate-settable clocks' rates are set */
		if (demod_clocks[i].rate == 1 &&
			clk_get_rate(drv->clks[i]) == 0)
			clk_set_rate(drv->clks[i],
				 clk_round_rate(drv->clks[i], 0));
		else if (demod_clocks[i].rate > 1)
			clk_set_rate(drv->clks[i], demod_clocks[i].rate);
	}

	return 0;
}

/**
 * demod_clock_prepare_enable() - enable clocks.
 *
 * @dev:	DEMOD device.
 */
static int demod_clock_prepare_enable(struct device *dev)
{
	struct demod_data *drv = dev_get_drvdata(dev);
	int rc;
	int i;
	/* GRAM clock should have DEM_CORE clock as a parent*/
	rc = clk_set_parent(drv->clks[9], drv->clks[1]);
	if (rc)
		return rc;
	for (i = 0; i < ARRAY_SIZE(drv->clks); i++) {
		if (demod_clocks[i].enable) {
			DRVDBG("Enable clock %s\n", demod_clocks[i].name);
			rc = clk_prepare_enable(drv->clks[i]);
			if (rc) {
				DRVERR("failed to enable %s\n",
					demod_clocks[i].name);
				for (i--; i >= 0; i--)
					if (demod_clocks[i].enable)
						clk_disable_unprepare(
							drv->clks[i]);
				return rc;
			}
			DRVDBG("Enable clock %s done %d\n",
				demod_clocks[i].name, rc);
		}
	}
	return rc;
}
/**
 * demod_clock_disable_unprepare() - disable clocks.
 *
 * @dev:	DEMOD device.
 */
static void demod_clock_disable_unprepare(struct device *dev)
{
	struct demod_data *drv = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(drv->clks); i++)
		if (demod_clocks[i].enable) {
			DRVDBG("disable clock %s\n",
				demod_clocks[i].name);
			clk_disable_unprepare(drv->clks[i]);
		}
}

/**
 * demod_read() - read from DEMOD memory.
 *
 * @filp:	file pointer.
 * @buf:	buffer to read to
 * @count:  bytes to read
 * @f_pos: internal offset
 */
static ssize_t demod_read(struct file *filp, char __user *buf, size_t count,
			 loff_t *f_pos)
{
	unsigned int base = 0;
	struct demod_data *drv = filp->private_data;
	int rc;

	rc = demod_verify_access(drv->read_base, count);
	if (rc)
		return rc;

	if ((drv->read_base & TOP8) == BASE_EXT) {
		base = (unsigned int)drv->ext_ram;
	} else {
		writel_relaxed((drv->read_base & TOP12) >> 20,
			drv->bcss_regs + NIBLE_PTR);
		base = BCSSADDR + ACCESS_OFFS + (drv->read_base & LOW20);
	}

	if (copy_to_user((void *)buf, (void *)base, count))
		return -EACCES;
	return count;
}

/**
 * demod_open() - open demod character device.
 *
 * @inode:	inode.
 * @filp:	file pointer
 */
static ssize_t demod_open(struct inode *inode, struct file *filp)
{
	struct demod_data *drv;
	int rc;
	drv = container_of(inode->i_cdev, struct demod_data, cdev);
	filp->private_data = drv;
	rc = regulator_enable(drv->gdsc);
	if (rc)  {
		DRVERR("GDSC enable failed\n");
		goto err_regulator;
	}

	rc = demod_clock_prepare_enable(drv->dev);
	if (rc) {
		DRVERR("clock prepare and enable failed\n");
		goto err_clock;
	}
	drv->subsys = subsystem_get("bcss");
	if (drv->subsys == NULL) {
		DRVERR("Peripheral Loader failed on demod.\n");
		goto err_pil;
	}

	if (mutex_lock_interruptible(&drv->mutex)) {
		rc = -ERESTARTSYS;
		goto err_mutex;
	}
	if (drv->ref_counter++ == 0) {
		__pm_stay_awake(&drv->wakeup_src);
		/* SET GRAM mapping to BCSS+0x40000 */
		writel_relaxed((BASE_G32 >> 20),
			drv->bcss_regs + NIBLE_PTR);
		drv->read_base = drv->write_base = 0;
	}
	mutex_unlock(&drv->mutex);
	return 0;
err_mutex:
	subsystem_put(drv->subsys);
err_pil:
	demod_clock_disable_unprepare(drv->dev);
err_clock:
	regulator_disable(drv->gdsc);
err_regulator:
	return rc;
}

/**
 * demod_close() - close demod character device.
 *
 * @inode:	inode.
 * @filp:	file pointer
 */
int demod_release(struct inode *inode, struct file *filp)
{
	struct demod_data *drv;
	drv = container_of(inode->i_cdev, struct demod_data, cdev);
	subsystem_put(drv->subsys);
	demod_clock_disable_unprepare(drv->dev);
	regulator_disable(drv->gdsc);
	mutex_lock(&drv->mutex);
	if (--drv->ref_counter == 0)
		__pm_relax(&drv->wakeup_src);
	mutex_unlock(&drv->mutex);
	return 0;
}

/**
 * demod_ioctl() - ioctl operations with demod character device.
 *
 * @param0:	parameter I
 * @param1: parameter II
 */
static long demod_ioctl(struct file *filp,
			unsigned int param0, unsigned long param1)
{
	struct demod_rw rw;
	struct demod_set_region sr;
	unsigned int extern_addr = 0;
	int rc = 0;
	struct demod_data *drv = filp->private_data;

	switch (param0) {
	/* Read/Write single 32 bit register from any DEMOD region */
	case DEMOD_IOCTL_RW:
		if (copy_from_user(&rw, (void *)param1,
				sizeof(struct demod_rw)) != 0)
			return -EACCES;
		rc = demod_verify_access(rw.addr, sizeof(unsigned int));
		if (rc)
			return rc;
		if ((rw.addr & TOP8) == META_RG) {
			extern_addr = BCSSADDR + rw.addr - META_RG;
		} else if ((rw.addr & TOP8) == BASE_G24 ||
			(rw.addr & TOP8) == BASE_G32) {
			writel_relaxed((rw.addr & TOP12) >> 20,
				drv->bcss_regs + NIBLE_PTR);
				extern_addr = BCSSADDR +
					ACCESS_OFFS + (rw.addr & LOW20);
		} else if ((rw.addr & TOP8) == BASE_EXT) {
			extern_addr =
			(unsigned int)drv->ext_ram + (rw.addr & LOW20);
		}
		if (extern_addr != 0) {
			if (rw.dir == 1) {
				rw.value = readl_relaxed(extern_addr);
				if (copy_to_user((void *)param1, &rw,
					sizeof(struct demod_rw)) != 0) {
					DRVDBG("ioctl rw EACCES\n");
					return -EACCES;
				}
			} else {
				writel_relaxed(rw.value, extern_addr);
			}
		} else {
			if (rw.dir == 1) {/*read */
				rc =
					meta_indirect_read(drv, rw.addr,
						&rw.value);
				if (rc)
					return rc;
				rc = copy_to_user((void *)param1,
						&rw,
						sizeof(struct demod_rw));
				if (rc)
					return -EACCES;
			} else {
				rc =
					meta_indirect_write(drv, rw.addr,
						rw.value);
				if (rc)
					return rc;
			}
		}
		break;
	/* Set region base for read/write opesrations */
	case DEMOD_IOCTL_SET_REGION:
		if (copy_from_user(&sr, (void *)param1,
			sizeof(struct demod_set_region)) == 0) {
			if (sr.dir == 1)/* read */
				drv->read_base = sr.base;
			else
				drv->write_base = sr.base;
			}
		else
			return -EACCES;
		break;
	case DEMOD_IOCTL_RESET:
		/* Reset DEMOD core. Block VBIF access at Top BCSS */
		writel_relaxed(1, drv->top_bcss + BCSS_VBIF);
		clk_reset(drv->clks[1], CLK_RESET_ASSERT);
		udelay(10);
		clk_reset(drv->clks[1], CLK_RESET_DEASSERT);
		writel_relaxed(0, drv->top_bcss + BCSS_VBIF);
		break;
	default:
		return 0;
	}
	return 0;
}

/**
 * demod_write() - write to DEMOD memory.
 *
 * @filp:	file pointer.
 * @buf:	buffer to write to
 * @count:  bytes to write
 * @f_pos: internal offset
 */
static ssize_t demod_write(struct file *filp,
				const char *buf, size_t count,
				loff_t *f_pos)
{
	unsigned int base;
	struct demod_data *drv = filp->private_data;
	int rc;

	rc = demod_verify_access(drv->read_base, count);
	if (rc)
		return rc;

	writel_relaxed((drv->write_base & TOP12)>>20,
		drv->bcss_regs+
		NIBLE_PTR);
	/*
	 * combined with nibble reg(0xd0004)
	 * the result is 0x00000b70, which
	 * points to gram packed view
	 */
	base = BCSSADDR + ACCESS_OFFS+
		(drv->write_base & LOW20);
	if (copy_from_user((void *)base, (void *)buf, count))
		return -EACCES;
	return count;
}

/**
 * msm_demod_probe() - driver probe function.
 *
 * @pdev:	platform device
 */
static int msm_demod_probe(struct platform_device *pdev)
{
	int res;
	struct demod_data *drv;
	struct resource *mem_demod, *top_bcss;
	struct device *dd;
	DRVDBG("demod driver init.\n");
	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	drv->bcss_regs = NULL;
	drv->ext_ram = NULL;
	drv->dev = &pdev->dev;
	platform_set_drvdata(pdev, drv);
	drv->ref_counter = 0;
	mutex_init(&drv->mutex);
	mem_demod = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "msm-demod");
	if (!mem_demod) {
		DRVERR("Missing DEMOD MEM resource");
		return -ENXIO;
	}
	drv->bcss_regs = devm_request_and_ioremap(&pdev->dev, mem_demod);
	if (!drv->bcss_regs) {
		DRVERR("ioremap failed");
		return -ENXIO;
	}
	top_bcss = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "top-bcss");
	if (!top_bcss) {
		DRVERR("Missing TOP BCSS MEM resource");
		return -ENXIO;
	}
	drv->top_bcss = devm_request_and_ioremap(&pdev->dev, top_bcss);
	if (!drv->top_bcss) {
		DRVERR("ioremap failed");
		return -ENXIO;
	}
	DRVDBG("demod BCSS base = 0x%08X\n", BCSSADDR);
	DRVDBG("top BCSS base = 0x%08X\n", (unsigned int)drv->top_bcss);

	drv->gdsc = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(drv->gdsc)) {
		DRVERR("Failed to get BCSS GDSC\n");
		return -ENODEV;
	}
	drv->node = pdev->dev.of_node;
	res = demod_clock_setup(&pdev->dev);
	if (res)
		return res;
	wakeup_source_init(&drv->wakeup_src, dev_name(&pdev->dev));
	res = alloc_chrdev_region(&demod_minor, 0, 1, "demod");
	if (res) {
		DRVDBG("uccp: alloc_chrdev_region failed: %d", res);
		return -ENODEV;
	}

	demod_class = class_create(THIS_MODULE, "demod");
	if (IS_ERR(demod_class)) {
		res = PTR_ERR(demod_class);
		DRVDBG("uccp: Error creating class: %d", res);
		goto fail_class_create;
	}
	/* Create char device */
	drv->cdev.owner = THIS_MODULE;
	cdev_init(&drv->cdev, &demod_fops);

	if (cdev_add(&drv->cdev, demod_minor++, 1) != 0) {
		DRVDBG("demod: cdev_add failed");
		res =  -EACCES;
		goto fail_cdev_add;
	}

	dd = device_create(demod_class, NULL, drv->cdev.dev,
		NULL, "demod");
	if (IS_ERR(dd)) {
		DRVERR("uccp: device_create failed: %i",
			(int)PTR_ERR(dd));
		res =  -EACCES;
		goto fail_device_create;
	}
	return 0;
fail_device_create:
	cdev_del(&drv->cdev);
fail_cdev_add:
	class_destroy(demod_class);
fail_class_create:
	unregister_chrdev_region(demod_minor, 1);
	return res;
}

/**
 * msm_demod_remove() - driver remove function.
 *
 * @pdev:	platform device
 */
static int msm_demod_remove(struct platform_device *pdev)
{
	struct demod_data *drv = dev_get_drvdata(&pdev->dev);
	DRVDBG("demod driver remove\n");
	mutex_destroy(&drv->mutex);
	wakeup_source_trash(&drv->wakeup_src);
	cdev_del(&drv->cdev);
	device_destroy(demod_class, drv->cdev.dev);
	class_destroy(demod_class);
	unregister_chrdev_region(demod_minor, 1);
	return 0;
}

/* Power Management */
static int demod_runtime_suspend(struct device *dev)
{
	return 0;
}

static int demod_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops demod_dev_pm_ops = {
	.runtime_suspend = demod_runtime_suspend,
	.runtime_resume = demod_runtime_resume,
};

/* Platform driver information */

static struct of_device_id msm_demod_match_table[] = {
	{.compatible = "qcom,msm-demod"},
	{}
};

static struct platform_driver msm_demod_driver = {
	.probe = msm_demod_probe,
	.remove = msm_demod_remove,
	.driver = {
		.name = "msm-demod",
		.pm = &demod_dev_pm_ops,
		.of_match_table = msm_demod_match_table,
	},
};

/**
 * demod_module_init() - DEMOD driver module init function.
 *
 * Return 0 on success, error value otherwise.
 */
static int __init demod_module_init(void)
{
	int rc;
	rc = platform_driver_register(&msm_demod_driver);
	if (rc)
		DRVERR("platform_driver_register failed: %d", rc);
	return rc;
}

/**
 * demod_module_exit() - DEMOD driver module exit function.
 */
static void __exit demod_module_exit(void)
{
	platform_driver_unregister(&msm_demod_driver);
}


module_init(demod_module_init);
module_exit(demod_module_exit);

MODULE_DESCRIPTION("DEMOD (Internal demodulator) platform device driver");
MODULE_LICENSE("GPL v2");
