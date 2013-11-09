/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

/*
 * MSM PCIe controller driver.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <asm/mach/pci.h>
#include <mach/gpiomux.h>
#include <mach/hardware.h>
#include <mach/msm_iomap.h>

#include "pcie.h"

/* Root Complex Port vendor/device IDs */
#define PCIE_VENDOR_ID_RCP             0x17cb
#define PCIE_DEVICE_ID_RCP             0x0101

#define PCIE20_PARF_SYS_CTRL           0x00
#define PCIE20_PARF_PM_STTS            0x24
#define PCIE20_PARF_PCS_DEEMPH         0x34
#define PCIE20_PARF_PCS_SWING          0x38
#define PCIE20_PARF_PHY_CTRL           0x40
#define PCIE20_PARF_PHY_REFCLK         0x4C
#define PCIE20_PARF_CONFIG_BITS        0x50
#define PCIE20_PARF_DBI_BASE_ADDR      0x168

#define PCIE20_ELBI_VERSION            0x00
#define PCIE20_ELBI_SYS_CTRL           0x04
#define PCIE20_ELBI_SYS_STTS	       0x08

#define PCIE20_CAP                     0x70
#define PCIE20_CAP_LINKCTRLSTATUS      (PCIE20_CAP + 0x10)

#define PCIE20_COMMAND_STATUS          0x04
#define PCIE20_BUSNUMBERS              0x18
#define PCIE20_MEMORY_BASE_LIMIT       0x20

#define PCIE20_PLR_IATU_VIEWPORT       0x900
#define PCIE20_PLR_IATU_CTRL1          0x904
#define PCIE20_PLR_IATU_CTRL2          0x908
#define PCIE20_PLR_IATU_LBAR           0x90C
#define PCIE20_PLR_IATU_UBAR           0x910
#define PCIE20_PLR_IATU_LAR            0x914
#define PCIE20_PLR_IATU_LTAR           0x918
#define PCIE20_PLR_IATU_UTAR           0x91c

#define RD 0
#define WR 1

/* PM control options */
#define PM_IRQ                   0x1
#define PM_CLK                   0x2
#define PM_GPIO                  0x4
#define PM_VREG                  0x8
#define PM_PIPE_CLK              0x10
#define PM_ALL (PM_IRQ | PM_CLK | PM_GPIO | PM_VREG | PM_PIPE_CLK)

/* Timing Delays */
#define PERST_PROPAGATION_DELAY_US_MIN        10000
#define PERST_PROPAGATION_DELAY_US_MAX        15000
#define REFCLK_STABILIZATION_DELAY_US_MIN     1000
#define REFCLK_STABILIZATION_DELAY_US_MAX     1500
#define LINK_RETRY_TIMEOUT_US_MIN             20000
#define LINK_RETRY_TIMEOUT_US_MAX             25000
#define LINK_UP_TIMEOUT_US_MIN                100000
#define LINK_UP_TIMEOUT_US_MAX                105000

#define PHY_READY_TIMEOUT_COUNT               0xFFFFFFull
#define XMLH_LINK_UP                          0x400
#define MAX_LINK_RETRIES 5
#define MAX_BUS_NUM 3

/* Config Space Offsets */
#define BDF_OFFSET(bus, device, function) \
	((bus << 24) | (device << 15) | (function << 8))

/* debug mask sys interface */
static int msm_pcie_debug_mask;
module_param_named(debug_mask, msm_pcie_debug_mask,
			    int, S_IRUGO | S_IWUSR | S_IWGRP);

struct mutex setup_lock;

/**
 *  PCIe driver state
 */
struct pcie_drv_sta {
	u32 rc_num;
	u32 rc_expected;
	u32 current_rc;
	bool vreg_on;
	struct mutex drv_lock;
} pcie_drv;

/* msm pcie device data */
static struct msm_pcie_dev_t msm_pcie_dev[MAX_RC_NUM];

/* regulators */
static struct msm_pcie_vreg_info_t msm_pcie_vreg_info[MSM_PCIE_MAX_VREG] = {
	{NULL, "vreg-3.3",	   0,       0,	     0},
	{NULL, "vreg-1.8", 1800000, 1800000,    1000},
	{NULL, "vreg-0.9", 1000000, 1000000,   24000}
};

/* GPIOs */
static struct msm_pcie_gpio_info_t msm_pcie_gpio_info[MSM_PCIE_MAX_GPIO] = {
	{"perst-gpio",      0, 1, 0, 1},
	{"wake-gpio",       0, 0, 0, 0},
	{"clkreq-gpio",     0, 0, 0, 0}
};

/* clocks */
static struct msm_pcie_clk_info_t
	msm_pcie_clk_info[MAX_RC_NUM][MSM_PCIE_MAX_CLK] = {
	{
	{NULL, "pcie_0_ref_clk_src", 0},
	{NULL, "pcie_0_aux_clk", 1010000},
	{NULL, "pcie_0_cfg_ahb_clk", 0},
	{NULL, "pcie_0_mstr_axi_clk", 0},
	{NULL, "pcie_0_slv_axi_clk", 0},
	{NULL, "pcie_0_ldo", 0}
	},
	{
	{NULL, "pcie_1_ref_clk_src", 0},
	{NULL, "pcie_1_aux_clk", 1010000},
	{NULL, "pcie_1_cfg_ahb_clk", 0},
	{NULL, "pcie_1_mstr_axi_clk", 0},
	{NULL, "pcie_1_slv_axi_clk", 0},
	{NULL, "pcie_1_ldo", 0}
	}
};

/* Pipe Clocks */
static struct msm_pcie_clk_info_t
	msm_pcie_pipe_clk_info[MAX_RC_NUM][MSM_PCIE_MAX_PIPE_CLK] = {
	{
	{NULL, "pcie_0_pipe_clk", 250000000},
	},
	{
	{NULL, "pcie_1_pipe_clk", 250000000},
	}
};



/* resources */
static const struct msm_pcie_res_info_t msm_pcie_res_info[MSM_PCIE_MAX_RES] = {
	{"parf",	0, 0},
	{"phy",     0, 0},
	{"dm_core",	0, 0},
	{"elbi",	0, 0},
	{"conf",	0, 0},
	{"bars",	0, 0}
};

/* irqs */
static const struct msm_pcie_irq_info_t msm_pcie_irq_info[MSM_PCIE_MAX_IRQ] = {
	{"int_msi",	0},
	{"int_a",	0},
	{"int_b",	0},
	{"int_c",	0},
	{"int_d",	0},
	{"int_pls_pme",		0},
	{"int_pme_legacy",	0},
	{"int_pls_err",		0},
	{"int_aer_legacy",	0},
	{"int_pls_link_up",	0},
	{"int_pls_link_down",	0},
	{"int_bridge_flush_n",	0},
	{"int_wake",	0}
};

int msm_pcie_get_debug_mask(void)
{
	return msm_pcie_debug_mask;
}

static void msm_pcie_write_mask(void __iomem *addr,
				uint32_t clear_mask, uint32_t set_mask)
{
	uint32_t val;

	val = (readl_relaxed(addr) & ~clear_mask) | set_mask;
	writel_relaxed(val, addr);
	wmb();  /* ensure data is written to hardware register */
}

static int msm_pcie_is_link_up(u32 rc_idx)
{
	return readl_relaxed(msm_pcie_dev[rc_idx].dm_core +
			PCIE20_CAP_LINKCTRLSTATUS) & BIT(29);
}

static inline int msm_pcie_oper_conf(struct pci_bus *bus, u32 devfn, int oper,
				     int where, int size, u32 *val)
{
	uint32_t word_offset, byte_offset, mask;
	uint32_t rd_val, wr_val;
	struct msm_pcie_dev_t *dev;
	void __iomem *config_base;
	bool rc = false;
	u32 rc_idx;
	int rv = 0;

	/* Do the bus->number based access control since we don't support
	   ECAM mechanism */

	switch (bus->number) {
	case 0:
		rc = true;
	case 1:
		if (pcie_drv.rc_expected > 1)
			rc_idx = 0;
		else
			rc_idx = pcie_drv.current_rc;
		break;
	case 2:
		rc = true;
	case 3:
		if (pcie_drv.rc_expected > 1)
			rc_idx = 1;
		else
			rc_idx = pcie_drv.current_rc;
		break;
	default:
		pr_err("PCIe: unsupported bus number.\n");
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto out;
	}

	dev = &msm_pcie_dev[rc_idx];

	if ((bus->number > MAX_BUS_NUM) || (devfn != 0)) {
		PCIE_DBG("invalid %s - bus %d devfn %d\n",
			 (oper == RD) ? "rd" : "wr", bus->number, devfn);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto out;
	}

	spin_lock_irqsave(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);

	if (!msm_pcie_dev[rc_idx].cfg_access) {
		PCIE_DBG("access denied for %d:0x%02x + 0x%04x[%d]\n",
			 bus->number, devfn, where, size);
		goto unlock;
	}

	/* check if the link is up for endpoint */
	if (((bus->number == 1) || (bus->number == 3))
		&& (!msm_pcie_is_link_up(rc_idx))) {
			*val = ~0;
			rv = PCIBIOS_DEVICE_NOT_FOUND;
			goto unlock;
	}

	word_offset = where & ~0x3;
	byte_offset = where & 0x3;
	mask = (~0 >> (8 * (4 - size))) << (8 * byte_offset);

	config_base = rc ? dev->dm_core : dev->conf;
	rd_val = readl_relaxed(config_base + word_offset);

	if (oper == RD) {
		*val = ((rd_val & mask) >> (8 * byte_offset));
	} else {
		wr_val = (rd_val & ~mask) |
				((*val << (8 * byte_offset)) & mask);
		writel_relaxed(wr_val, config_base + word_offset);
		wmb(); /* ensure config data is written to hardware register */
	}

unlock:
	spin_unlock_irqrestore(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);
out:
	return 0;
}

static int msm_pcie_rd_conf(struct pci_bus *bus, u32 devfn, int where,
			    int size, u32 *val)
{
	int ret = msm_pcie_oper_conf(bus, devfn, RD, where, size, val);

	if ((bus->number == 0) && (where == PCI_CLASS_REVISION)) {
		*val = (*val & 0xff) | (PCI_CLASS_BRIDGE_PCI << 16);
		PCIE_DBG("change class for RC:0x%x\n", *val);
	}

	return ret;
}

static int msm_pcie_wr_conf(struct pci_bus *bus, u32 devfn,
			    int where, int size, u32 val)
{
	return msm_pcie_oper_conf(bus, devfn, WR, where, size, &val);
}

static struct pci_ops msm_pcie_ops = {
	.read = msm_pcie_rd_conf,
	.write = msm_pcie_wr_conf,
};

static int msm_pcie_gpio_init(u32 rc_idx)
{
	int rc, i;
	struct msm_pcie_gpio_info_t *info;

	for (i = 0; i < msm_pcie_dev[rc_idx].gpio_n; i++) {
		info = &msm_pcie_dev[rc_idx].gpio[i];

		if (!info->num)
			continue;

		rc = gpio_request(info->num, info->name);
		if (rc) {
			pr_err("can't get gpio %s; %d\n", info->name, rc);
			break;
		}

		if (info->out)
			rc = gpio_direction_output(info->num, info->init);
		else
			rc = gpio_direction_input(info->num);
		if (rc) {
			pr_err("can't set gpio direction %s; %d\n",
			       info->name, rc);
			gpio_free(info->num);
			break;
		}
	}

	if (rc)
		while (i--)
			gpio_free(msm_pcie_dev[rc_idx].gpio[i].num);

	return rc;
}

static void msm_pcie_gpio_deinit(u32 rc_idx)
{
	int i;

	for (i = 0; i < msm_pcie_dev[rc_idx].gpio_n; i++)
		gpio_free(msm_pcie_dev[rc_idx].gpio[i].num);
}

static int msm_pcie_vreg_init(u32 rc_idx, struct device *dev)
{
	int i, rc = 0;
	struct regulator *vreg;
	struct msm_pcie_vreg_info_t *info;

	if (pcie_drv.vreg_on) {
		PCIE_DBG("Vreg has been turned on.\n");
		return rc;
	} else {
		pcie_drv.vreg_on = true;
	}

	for (i = 0; i < msm_pcie_dev[rc_idx].vreg_n; i++) {
		info = &msm_pcie_dev[rc_idx].vreg[i];
		vreg = info->hdl;

		PCIE_DBG("Vreg %s is being enabled\n", info->name);
		if (info->max_v) {
			rc = regulator_set_voltage(vreg,
						   info->min_v, info->max_v);
			if (rc) {
				pr_err("can't set voltage %s; %d\n",
				       info->name, rc);
				break;
			}
		}

		if (info->opt_mode) {
			rc = regulator_set_optimum_mode(vreg, info->opt_mode);
			if (rc < 0) {
				pr_err("can't set mode %s; %d\n",
				       info->name, rc);
				break;
			}
		}

		rc = regulator_enable(vreg);
		if (rc) {
			pr_err("can't enable %s, %d\n", info->name, rc);
			break;
		}
	}

	if (rc)
		while (i--)
			regulator_disable(msm_pcie_dev[rc_idx].vreg[i].hdl);

	if (rc)
		pcie_drv.vreg_on = false;

	return rc;
}

static void msm_pcie_vreg_deinit(u32 rc_idx)
{
	int i;

	if (pcie_drv.vreg_on)
		pcie_drv.vreg_on = false;
	else {
		PCIE_DBG("Vreg has been turned off\n");
		return;
	}

	for (i = msm_pcie_dev[rc_idx].vreg_n - 1; i >= 0; i--) {
		PCIE_DBG("Vreg %s is being disabled\n",
					msm_pcie_dev[rc_idx].vreg[i].name);
		regulator_disable(msm_pcie_dev[rc_idx].vreg[i].hdl);
	}
}

static int msm_pcie_clk_init(u32 rc_idx, struct device *dev)
{
	int i, rc = 0;
	struct msm_pcie_clk_info_t *info;

	if (dev) {
		PCIE_DBG("dev name:%s\n", dev_name(dev));
	} else {
		pr_err("PCIe: dev is NULL\n");
		return -ENODEV;
	}

	rc = regulator_enable(msm_pcie_dev[rc_idx].gdsc);

	if (rc) {
		pr_err("PCIe: fail to enable GDSC for RC %d\n", rc_idx);
		return rc;
	}

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++) {
		info = &msm_pcie_dev[rc_idx].clk[i];

		if (info->freq) {
			rc = clk_set_rate(info->hdl, info->freq);
			if (rc) {
				pr_err("PCIe: can't set rate for clk %s: %d\n",
					info->name, rc);
				break;
			}
		}

		rc = clk_prepare_enable(info->hdl);

		if (rc)
			pr_err("failed to enable clk %s\n", info->name);
		else
			PCIE_DBG("enable clk %s\n", info->name);
	}

	if (rc) {
		PCIE_DBG("disable clocks for error handling\n");
		while (i--)
			clk_disable_unprepare(msm_pcie_dev[rc_idx].clk[i].hdl);
		regulator_disable(msm_pcie_dev[rc_idx].gdsc);
	}

	return rc;
}

static void msm_pcie_clk_deinit(u32 rc_idx)
{
	int i;

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++)
		clk_disable_unprepare(msm_pcie_dev[rc_idx].clk[i].hdl);
	regulator_disable(msm_pcie_dev[rc_idx].gdsc);
}

static int msm_pcie_pipe_clk_init(u32 rc_idx, struct device *dev)
{
	int i, rc = 0;
	struct msm_pcie_clk_info_t *info;

	if (dev) {
		PCIE_DBG("dev name:%s\n", dev_name(dev));
	} else {
		pr_err("PCIe: dev is NULL\n");
		return -ENODEV;
	}

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++) {
		info = &msm_pcie_dev[rc_idx].pipeclk[i];

		if (info->freq) {
			rc = clk_set_rate(info->hdl, info->freq);
			if (rc) {
				pr_err("PCIe: can't set rate for clk %s: %d\n",
					info->name, rc);
				break;
			}
		}

		rc = clk_prepare_enable(info->hdl);

		if (rc)
			pr_err("failed to enable clk %s\n", info->name);
		else
			PCIE_DBG("enabled pipe clk %s\n", info->name);
	}

	if (rc) {
		PCIE_DBG("disable pipe clocks for error handling\n");
		while (i--)
			clk_disable_unprepare(
			   msm_pcie_dev[rc_idx].pipeclk[i].hdl);
	}

	return rc;
}

static void msm_pcie_pipe_clk_deinit(u32 rc_idx)
{
	int i;

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++)
		clk_disable_unprepare(msm_pcie_dev[rc_idx].pipeclk[i].hdl);
}


static void msm_pcie_config_controller(u32 rc_idx)
{
	struct msm_pcie_dev_t *dev = &msm_pcie_dev[rc_idx];
	struct resource *axi_conf = dev->res[MSM_PCIE_RES_CONF].resource;
	u32 dev_conf, upper, lower, limit;

	if (IS_ENABLED(CONFIG_ARM_LPAE)) {
		lower = PCIE_LOWER_ADDR(axi_conf->start);
		upper = PCIE_UPPER_ADDR(axi_conf->start);
		limit = PCIE_LOWER_ADDR(axi_conf->end);
	} else {
		lower = axi_conf->start;
		upper = 0;
		limit = axi_conf->end;
	}

	if (rc_idx == 0) {
		/* Should equate to 0x01000000 */
		dev_conf = BDF_OFFSET(1, 0, 0);
	} else {
		/* Should equate to 0x03000000 */
		dev_conf = BDF_OFFSET(3, 0, 0);
	}

	/*
	 * program and enable address translation region 0 (device config
	 * address space); region type config;
	 * axi config address range to device config address range
	 */
	writel_relaxed(0, dev->dm_core + PCIE20_PLR_IATU_VIEWPORT);
	/* ensure that hardware locks the region before programming it */
	wmb();

	writel_relaxed(4, dev->dm_core + PCIE20_PLR_IATU_CTRL1);
	writel_relaxed(lower, dev->dm_core + PCIE20_PLR_IATU_LBAR);
	writel_relaxed(upper, dev->dm_core + PCIE20_PLR_IATU_UBAR);
	writel_relaxed(limit, dev->dm_core + PCIE20_PLR_IATU_LAR);
	writel_relaxed(dev_conf, dev->dm_core + PCIE20_PLR_IATU_LTAR);
	writel_relaxed(0, dev->dm_core + PCIE20_PLR_IATU_UTAR);
	writel_relaxed(BIT(31), dev->dm_core + PCIE20_PLR_IATU_CTRL2);
	/* ensure that hardware registers the configuration */
	wmb();
	PCIE_DBG("PCIE20_PLR_IATU_VIEWPORT:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_VIEWPORT));
	PCIE_DBG("PCIE20_PLR_IATU_CTRL1:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_CTRL1));
	PCIE_DBG("PCIE20_PLR_IATU_LBAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LBAR));
	PCIE_DBG("PCIE20_PLR_IATU_UBAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_UBAR));
	PCIE_DBG("PCIE20_PLR_IATU_LAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LAR));
	PCIE_DBG("PCIE20_PLR_IATU_LTAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LTAR));
	PCIE_DBG("PCIE20_PLR_IATU_UTAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_UTAR));
	PCIE_DBG("PCIE20_PLR_IATU_CTRL2:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_CTRL2));
}

static int msm_pcie_get_resources(u32 rc_idx, struct platform_device *pdev)
{
	int i, ret = 0;
	struct msm_pcie_vreg_info_t *vreg_info;
	struct msm_pcie_gpio_info_t *gpio_info;
	struct msm_pcie_clk_info_t  *clk_info;
	struct resource *res;
	struct msm_pcie_res_info_t *res_info;
	struct msm_pcie_irq_info_t *irq_info;
	struct msm_pcie_dev_t *dev = &msm_pcie_dev[rc_idx];

	for (i = 0; i < MSM_PCIE_MAX_VREG; i++) {
		vreg_info = &dev->vreg[i];
		vreg_info->hdl =
				devm_regulator_get(&pdev->dev, vreg_info->name);
		if (IS_ERR(vreg_info->hdl)) {
			PCIE_DBG("Vreg %s doesn't exist\n", vreg_info->name);
			return PTR_ERR(vreg_info->hdl);
		} else
			dev->vreg_n++;
	}

	dev->gdsc = devm_regulator_get(&pdev->dev, "gdsc_vdd");

	if (IS_ERR(dev->gdsc)) {
		pr_err("PCIe: Failed to get PCIe_%d GDSC:%ld\n",
			rc_idx, PTR_ERR(dev->gdsc));
		if (PTR_ERR(dev->gdsc) == -EPROBE_DEFER)
			PCIE_DBG("PCIe: EPROBE_DEFER for PCIe_%d GDSC\n",
					rc_idx);
		return PTR_ERR(dev->gdsc);
	}

	dev->gpio_n = 0;
	for (i = 0; i < MSM_PCIE_MAX_GPIO; i++) {
		gpio_info = &dev->gpio[i];
		ret = of_get_named_gpio((&pdev->dev)->of_node,
					gpio_info->name, 0);
		if (ret >= 0) {
			gpio_info->num = ret;
			ret = 0;
			dev->gpio_n++;
			PCIE_DBG("GPIO num for %s is %d\n", gpio_info->name,
							gpio_info->num);
		} else {
			return ret;
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++) {
		clk_info = &msm_pcie_dev[rc_idx].clk[i];

		clk_info->hdl = devm_clk_get(&pdev->dev, clk_info->name);

		if (IS_ERR(clk_info->hdl)) {
			PCIE_DBG("Clock %s isn't available\n", clk_info->name);
			return PTR_ERR(clk_info->hdl);
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++) {
		clk_info = &msm_pcie_dev[rc_idx].pipeclk[i];

		clk_info->hdl = devm_clk_get(&pdev->dev, clk_info->name);

		if (IS_ERR(clk_info->hdl)) {
			PCIE_DBG("Clock %s isn't available\n", clk_info->name);
			return PTR_ERR(clk_info->hdl);
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_RES; i++) {
		res_info = &dev->res[i];

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							   res_info->name);

		if (!res) {
			pr_err("pcie:can't get %s resource.\n", res_info->name);
			return -ENOMEM;
		} else
			PCIE_DBG("start addr for %s is %pa.\n", res_info->name,
					&res->start);

		res_info->base = devm_ioremap(&pdev->dev,
						res->start, resource_size(res));
		if (!res_info->base) {
			pr_err("pcie: can't remap %s.\n", res_info->name);
			return -ENOMEM;
		}
		res_info->resource = res;
	}

	for (i = 0; i < MSM_PCIE_MAX_IRQ; i++) {
		irq_info = &dev->irq[i];

		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							   irq_info->name);

		if (!res) {
			int j;
			for (j = 0; j < MSM_PCIE_MAX_RES; j++) {
				iounmap(dev->res[j].base);
				dev->res[j].base = NULL;
			}
			pr_err("pcie: can't find IRQ # for %s.\n",
				irq_info->name);
			ret = -ENODEV;
			break;
		} else {
			irq_info->num = res->start;
			PCIE_DBG("IRQ # for %s is %d.\n", irq_info->name,
					irq_info->num);
		}
	}

	/* All allocations succeeded */

	dev->wake_n = dev->irq[MSM_PCIE_INT_WAKE].num;

	dev->parf = dev->res[MSM_PCIE_RES_PARF].base;
	dev->phy = dev->res[MSM_PCIE_RES_PHY].base;
	dev->elbi = dev->res[MSM_PCIE_RES_ELBI].base;
	dev->dm_core = dev->res[MSM_PCIE_RES_DM_CORE].base;
	dev->conf = dev->res[MSM_PCIE_RES_CONF].base;
	dev->bars = dev->res[MSM_PCIE_RES_BARS].base;
	dev->dev_mem_res = dev->res[MSM_PCIE_RES_BARS].resource;

	return ret;
}

static void msm_pcie_release_resources(u32 rc_idx)
{
	msm_pcie_dev[rc_idx].parf = NULL;
	msm_pcie_dev[rc_idx].elbi = NULL;
	msm_pcie_dev[rc_idx].dm_core = NULL;
	msm_pcie_dev[rc_idx].conf = NULL;
	msm_pcie_dev[rc_idx].bars = NULL;
	msm_pcie_dev[rc_idx].dev_mem_res = NULL;
}

static int msm_pcie_enable(u32 rc_idx, u32 options)
{
	int ret;
	struct msm_pcie_dev_t *dev;
	uint32_t val;
	long int retries = 0;

	mutex_lock(&setup_lock);

	dev = &msm_pcie_dev[rc_idx];

	/* assert PCIe reset link to keep EP in reset */

	gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				dev->gpio[MSM_PCIE_GPIO_PERST].on);
	usleep_range(PERST_PROPAGATION_DELAY_US_MIN,
				 PERST_PROPAGATION_DELAY_US_MAX);

	/* enable power */

	if (options & PM_VREG) {
		ret = msm_pcie_vreg_init(rc_idx, &dev->pdev->dev);
		if (ret)
			goto out;
	}

	/* enable clocks */
	if (options & PM_CLK) {
		ret = msm_pcie_clk_init(rc_idx, &dev->pdev->dev);
		wmb();
		if (ret)
			goto clk_fail;
	}

	/* enable PCIe clocks and resets */
	msm_pcie_write_mask(dev->parf + PCIE20_PARF_PHY_CTRL, BIT(0), 0);

	/* change DBI base address */
	writel_relaxed(0, dev->parf + PCIE20_PARF_DBI_BASE_ADDR);

	/* init PCIe PHY */
	pcie_phy_init(dev);

	if (options & PM_PIPE_CLK) {
		/* Enable the pipe clock */
		ret = msm_pcie_pipe_clk_init(rc_idx, &dev->pdev->dev);
		wmb();
		if (ret)
			goto link_fail;
	}

	PCIE_DBG("waiting for phy ready...\n");

	do {
		if (pcie_phy_is_ready(dev))
			break;
		retries++;
		usleep_range(REFCLK_STABILIZATION_DELAY_US_MIN,
					 REFCLK_STABILIZATION_DELAY_US_MAX);
	} while (retries < PHY_READY_TIMEOUT_COUNT);

	PCIE_DBG("number of PHY retries: %ld\n", retries);

	if (pcie_phy_is_ready(dev))
		pr_info("PCIe PHY is ready!\n");
	else {
		pr_err("PCIe PHY failed to come up!\n");
		ret = -ENODEV;
		goto link_fail;
	}

	/* de-assert PCIe reset link to bring EP out of reset */

	gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				1 - dev->gpio[MSM_PCIE_GPIO_PERST].on);
	usleep_range(PERST_PROPAGATION_DELAY_US_MIN,
				 PERST_PROPAGATION_DELAY_US_MAX);

	/* enable link training */
	msm_pcie_write_mask(dev->elbi + PCIE20_ELBI_SYS_CTRL, 0, BIT(0));

	PCIE_DBG("check if link is up\n");

	/* Wait for 100ms for the link to come up */
	usleep_range(LINK_UP_TIMEOUT_US_MIN, LINK_UP_TIMEOUT_US_MAX);

	retries = 0;
	val =  readl_relaxed(dev->elbi + PCIE20_ELBI_SYS_STTS);
	while (!(val & XMLH_LINK_UP) && (retries < MAX_LINK_RETRIES)) {
		PCIE_DBG("LTSSM_STATE:0x%x\n", (val >> 0xC) & 0x1f);
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				dev->gpio[MSM_PCIE_GPIO_PERST].on);
		usleep_range(PERST_PROPAGATION_DELAY_US_MIN,
					 PERST_PROPAGATION_DELAY_US_MAX);
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				1 - dev->gpio[MSM_PCIE_GPIO_PERST].on);
		usleep_range(LINK_RETRY_TIMEOUT_US_MIN,
				LINK_RETRY_TIMEOUT_US_MAX);
		retries++;
		val =  readl_relaxed(dev->elbi + PCIE20_ELBI_SYS_STTS);
	}

	PCIE_DBG("number of link training retries: %ld\n", retries);

	if (val & XMLH_LINK_UP) {
		pr_info("PCIe link initialized\n");
	} else {
		pr_err("PCIe link initialization failed\n");
		ret = -1;
		goto link_fail;
	}

	msm_pcie_config_controller(rc_idx);

	if (options & PM_IRQ) {
		ret = msm_pcie_irq_init(dev);
		if (!ret)
			goto out;
	} else {
		goto out;
	}

link_fail:
	msm_pcie_clk_deinit(rc_idx);
clk_fail:
	msm_pcie_vreg_deinit(rc_idx);
	msm_pcie_pipe_clk_deinit(rc_idx);
out:
	dev->link_status = MSM_PCIE_LINK_ENABLED;
	mutex_unlock(&setup_lock);

	return ret;
}


void msm_pcie_disable(u32 rc_idx, u32 options)
{
	struct msm_pcie_dev_t *dev = &msm_pcie_dev[rc_idx];

	gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				dev->gpio[MSM_PCIE_GPIO_PERST].on);

	if (options & PM_IRQ)
		msm_pcie_irq_deinit(dev);

	if (options & PM_CLK) {
		msm_pcie_write_mask(dev->parf + PCIE20_PARF_PHY_CTRL, 0,
					BIT(0));
		msm_pcie_clk_deinit(rc_idx);
	}

	if (options & PM_VREG)
		msm_pcie_vreg_deinit(rc_idx);

	if (options & PM_PIPE_CLK)
		msm_pcie_pipe_clk_deinit(rc_idx);

	dev->link_status = MSM_PCIE_LINK_DISABLED;
}

static int msm_pcie_setup(int nr, struct pci_sys_data *sys)
{
	static bool init;

	PCIE_DBG("bus %d\n", nr);

	/*
	 * specify linux PCI framework to allocate device memory (BARs)
	 * from msm_pcie_dev.dev_mem_res resource.
	 */
	if (!init) {
		sys->mem_offset = 0;
		init = true;
	}

	pci_add_resource(&sys->resources,
			msm_pcie_dev[pcie_drv.current_rc].dev_mem_res);

	return 1;
}

static struct pci_bus *msm_pcie_scan_bus(int nr,
						struct pci_sys_data *sys)
{
	struct pci_bus *bus = NULL;

	PCIE_DBG("bus %d\n", nr);

	bus = pci_scan_root_bus(NULL, sys->busnr, &msm_pcie_ops, sys,
					&sys->resources);

	return bus;
}

static int msm_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	u32 rc_idx = pcie_drv.current_rc;
	int ret = 0;

	PCIE_DBG("rc %d slot %d pin %d\n", rc_idx, slot, pin);

	switch (pin) {
	case 1:
		ret = msm_pcie_dev[rc_idx].irq[MSM_PCIE_INT_A].num;
		break;
	case 2:
		ret = msm_pcie_dev[rc_idx].irq[MSM_PCIE_INT_B].num;
		break;
	case 3:
		ret = msm_pcie_dev[rc_idx].irq[MSM_PCIE_INT_C].num;
		break;
	case 4:
		ret = msm_pcie_dev[rc_idx].irq[MSM_PCIE_INT_D].num;
		break;
	default:
		pr_err("PCIe: unsupported pin number.\n");
	}

	return ret;
}

static struct hw_pci msm_pci = {
	.nr_controllers	= MAX_RC_NUM,
	.swizzle	= pci_common_swizzle,
	.setup		= msm_pcie_setup,
	.scan		= msm_pcie_scan_bus,
	.map_irq	= msm_pcie_map_irq,
};

static int msm_pcie_probe(struct platform_device *pdev)
{
	int ret = 0;
	int rc_idx;
	struct msm_pcie_dev_t *dev;

	mutex_lock(&pcie_drv.drv_lock);

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,ctrl-amt", &pcie_drv.rc_expected);
	if (ret) {
		pr_err("PCIe: does not find controller amount.\n");
		goto out;
	} else {
		if (pcie_drv.rc_expected > MAX_RC_NUM) {
			PCIE_DBG("Expected number of devices %d\n",
							pcie_drv.rc_expected);
			PCIE_DBG("Exceeded max supported devices %d\n",
							MAX_RC_NUM);
			goto out;
		}
		PCIE_DBG("Target has %d RC(s).\n", pcie_drv.rc_expected);
	}

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"cell-index", &rc_idx);
	if (ret) {
		PCIE_DBG("Did not find RC index.\n");
		goto out;
	} else {
		if (rc_idx >= MAX_RC_NUM) {
			PCIE_DBG("Invalid RC Index %d (max supported = %d)\n",
							rc_idx, MAX_RC_NUM);
			goto out;
		}
		pcie_drv.rc_num++;
		PCIE_DBG("RC index is %d.", rc_idx);
	}

	msm_pcie_dev[rc_idx].pdev = pdev;
	msm_pcie_dev[rc_idx].vreg_n = 0;
	msm_pcie_dev[rc_idx].gpio_n = 0;
	msm_pcie_dev[rc_idx].parf_deemph = 0;
	msm_pcie_dev[rc_idx].parf_swing = 0;
	msm_pcie_dev[rc_idx].link_status = MSM_PCIE_LINK_DEINIT;
	memcpy(msm_pcie_dev[rc_idx].vreg, msm_pcie_vreg_info,
				sizeof(msm_pcie_vreg_info));
	memcpy(msm_pcie_dev[rc_idx].gpio, msm_pcie_gpio_info,
				sizeof(msm_pcie_gpio_info));
	memcpy(msm_pcie_dev[rc_idx].clk, msm_pcie_clk_info[rc_idx],
				sizeof(msm_pcie_clk_info));
	memcpy(msm_pcie_dev[rc_idx].pipeclk, msm_pcie_pipe_clk_info[rc_idx],
				sizeof(msm_pcie_pipe_clk_info));
	memcpy(msm_pcie_dev[rc_idx].res, msm_pcie_res_info,
				sizeof(msm_pcie_res_info));
	memcpy(msm_pcie_dev[rc_idx].irq, msm_pcie_irq_info,
				sizeof(msm_pcie_irq_info));

	ret = msm_pcie_get_resources(rc_idx, msm_pcie_dev[rc_idx].pdev);

	if (ret)
		goto decrease_rc_num;

	ret = msm_pcie_gpio_init(rc_idx);
	if (ret) {
		msm_pcie_release_resources(rc_idx);
		goto decrease_rc_num;
	}

	ret = msm_pcie_enable(rc_idx, PM_ALL);
	if (ret) {
		pr_err("PCIe:failed to enable PCIe link in bootup\n");
		msm_pcie_gpio_deinit(rc_idx);
		msm_pcie_release_resources(rc_idx);
		goto decrease_rc_num;
	}

	/* kick start ARM PCI configuration framework */
	if (pcie_drv.rc_num == pcie_drv.rc_expected) {
		msm_pci.nr_controllers = pcie_drv.rc_num;
		if (pcie_drv.rc_expected > 1)
			pcie_drv.current_rc = 0;
		else
			pcie_drv.current_rc = rc_idx;
		dev = &msm_pcie_dev[rc_idx];
		msm_pci.private_data = (void **)&dev;
		pci_common_init(&msm_pci);
	}

	PCIE_DBG("PCIE probed %s\n", dev_name(&(pdev->dev)));
	mutex_unlock(&pcie_drv.drv_lock);
	return 0;

decrease_rc_num:
	pcie_drv.rc_num--;
out:
	pr_err("Driver Failed ret=%d\n", ret);
	mutex_unlock(&pcie_drv.drv_lock);

	return ret;
}

static int __exit msm_pcie_remove(struct platform_device *pdev)
{
	int ret = 0;
	int rc_idx;

	mutex_lock(&pcie_drv.drv_lock);

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"cell-index", &rc_idx);
	if (ret) {
		PCIE_DBG("Did not find RC index.\n");
		goto out;
	} else {
		pcie_drv.rc_num--;
		PCIE_DBG("RC index is 0x%x.", rc_idx);
	}

	msm_pcie_irq_deinit(&msm_pcie_dev[rc_idx]);
	msm_pcie_vreg_deinit(rc_idx);
	msm_pcie_clk_deinit(rc_idx);
	msm_pcie_gpio_deinit(rc_idx);
	msm_pcie_release_resources(rc_idx);

out:
	mutex_unlock(&pcie_drv.drv_lock);

	return ret;
}

static struct of_device_id msm_pcie_match[] = {
	{	.compatible = "qcom,msm_pcie",
	},
	{}
};

static struct platform_driver msm_pcie_driver = {
	.probe	= msm_pcie_probe,
	.remove	= msm_pcie_remove,
	.driver	= {
		.name		= "msm_pcie",
		.owner		= THIS_MODULE,
		.of_match_table	= msm_pcie_match,
	},
};

static int __init pcie_init(void)
{
	int ret = 0, i;

	PCIE_DBG("pcie:%s.\n", __func__);

	pcie_drv.rc_num = 0;
	pcie_drv.rc_expected = 0;
	mutex_init(&pcie_drv.drv_lock);
	mutex_init(&setup_lock);

	for (i = 0; i < MAX_RC_NUM; i++) {
		spin_lock_init(&msm_pcie_dev[i].cfg_lock);
		msm_pcie_dev[i].cfg_access = true;
	}

	ret = platform_driver_register(&msm_pcie_driver);

	return ret;
}

static void __exit pcie_exit(void)
{
	PCIE_DBG("pcie:%s.\n", __func__);

	platform_driver_unregister(&msm_pcie_driver);
}

subsys_initcall_sync(pcie_init);
module_exit(pcie_exit);


/* RC do not represent the right class; set it to PCI_CLASS_BRIDGE_PCI */
static void msm_pcie_fixup_early(struct pci_dev *dev)
{
	PCIE_DBG("hdr_type %d\n", dev->hdr_type);
	if (dev->hdr_type == 1)
		dev->class = (dev->class & 0xff) | (PCI_CLASS_BRIDGE_PCI << 8);
}
DECLARE_PCI_FIXUP_EARLY(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
			msm_pcie_fixup_early);

/* enable wake_n interrupt during suspend */
static void msm_pcie_fixup_suspend(struct pci_dev *dev)
{
	int ret = 0;
	uint32_t val = 0;
	u32 rc_idx = pcie_drv.current_rc; /* need to be replaced later */

	if (msm_pcie_dev[rc_idx].link_status != MSM_PCIE_LINK_ENABLED)
		return;

	pci_save_state(dev);

	spin_lock_irqsave(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);
	msm_pcie_dev[rc_idx].cfg_access = false;
	spin_unlock_irqrestore(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);

	msm_pcie_write_mask(msm_pcie_dev[rc_idx].elbi + PCIE20_ELBI_SYS_CTRL, 0,
				BIT(4));

	PCIE_DBG("PME_TURNOFF_MSG is sent out\n");

	ret = readl_poll_timeout((msm_pcie_dev[rc_idx].parf
		+ PCIE20_PARF_PM_STTS), val, (val & BIT(6)), 10000, 100000);

	/* check L23_Ready */
	if (!ret)
		PCIE_DBG("PM_Enter_L23 is received\n");
	else
		PCIE_DBG("PM_Enter_L23 is NOT received\n");

	msm_pcie_disable(rc_idx, PM_PIPE_CLK | PM_CLK | PM_VREG);

	PCIE_DBG("enabling wake_n\n");

	enable_irq(msm_pcie_dev[rc_idx].wake_n);
}
DECLARE_PCI_FIXUP_SUSPEND(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
			  msm_pcie_fixup_suspend);

/* disable wake_n interrupt when system is not in suspend */
void msm_pcie_fixup_resume_early(struct pci_dev *dev)
{
	u32 rc_idx = pcie_drv.current_rc; /* need to be replaced later */

	if (msm_pcie_dev[rc_idx].link_status != MSM_PCIE_LINK_DISABLED)
		return;

	PCIE_DBG("disabling wake_n\n");

	disable_irq(msm_pcie_dev[rc_idx].wake_n);

	spin_lock_irqsave(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);
	msm_pcie_dev[rc_idx].cfg_access = true;
	spin_unlock_irqrestore(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);

	if (msm_pcie_enable(rc_idx, PM_PIPE_CLK | PM_CLK | PM_VREG))
		pr_err("PCIe:failed to enable PCIe link in resume\n");
	else
		pci_restore_state(dev);
}
DECLARE_PCI_FIXUP_RESUME_EARLY(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
				 msm_pcie_fixup_resume_early);
static void msm_pcie_fixup_final(struct pci_dev *dev)
{
	PCIE_DBG("\n");

	if (pcie_drv.rc_expected > 1)
		pcie_drv.current_rc++;
}
DECLARE_PCI_FIXUP_FINAL(PCI_ANY_ID, PCI_ANY_ID, msm_pcie_fixup_final);

