/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
 * MSM PCIe endpoint core driver.
 */

#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/clk/msm-clk.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include "ep_pcie_com.h"

/* debug mask sys interface */
static int ep_pcie_debug_mask;
static int ep_pcie_debug_keep_resource;
static u32 ep_pcie_bar0_address;
module_param_named(debug_mask, ep_pcie_debug_mask,
			int, S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(debug_keep_resource, ep_pcie_debug_keep_resource,
			int, S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(bar0_address, ep_pcie_bar0_address,
			int, S_IRUGO | S_IWUSR | S_IWGRP);

struct ep_pcie_dev_t ep_pcie_dev = {0};

static struct ep_pcie_vreg_info_t ep_pcie_vreg_info[EP_PCIE_MAX_VREG] = {
	{NULL, "vreg-1.8", 1800000, 1800000, 14000, true},
	{NULL, "vreg-0.9", 1000000, 1000000, 40000, true}
};

static struct ep_pcie_gpio_info_t ep_pcie_gpio_info[EP_PCIE_MAX_GPIO] = {
	{"perst-gpio",      0, 0, 0, 1},
	{"wake-gpio",       0, 1, 0, 1},
	{"clkreq-gpio",     0, 1, 0, 0},
	{"mdm2apstatus-gpio",    0, 1, 1, 0}
};

static struct ep_pcie_clk_info_t
	ep_pcie_clk_info[EP_PCIE_MAX_CLK] = {
	{NULL, "pcie_0_cfg_ahb_clk", 0, true},
	{NULL, "pcie_0_mstr_axi_clk", 0, true},
	{NULL, "pcie_0_slv_axi_clk", 0, true},
	{NULL, "pcie_0_aux_clk", 1000000, true},
	{NULL, "pcie_0_ldo", 0, true},
	{NULL, "pcie_0_phy_reset", 0, false}
};

static struct ep_pcie_clk_info_t
	ep_pcie_pipe_clk_info[EP_PCIE_MAX_PIPE_CLK] = {
	{NULL, "pcie_0_pipe_clk", 62500000, true}
};

static const struct ep_pcie_res_info_t ep_pcie_res_info[EP_PCIE_MAX_RES] = {
	{"parf",	0, 0},
	{"phy",		0, 0},
	{"mmio",	0, 0},
	{"msi",		0, 0},
	{"dm_core",	0, 0},
	{"elbi",	0, 0}
};

static const struct ep_pcie_irq_info_t ep_pcie_irq_info[EP_PCIE_MAX_IRQ] = {
	{"int_pm_turnoff",	0},
	{"int_dstate_change",		0},
	{"int_l1sub_timeout",	0},
	{"int_link_up",	0},
	{"int_link_down",	0},
	{"int_bridge_flush_n",	0},
	{"int_bme",	0},
	{"int_global",	0}
};

int ep_pcie_get_debug_mask(void)
{
	return ep_pcie_debug_mask;
}

static bool ep_pcie_confirm_linkup(struct ep_pcie_dev_t *dev,
				bool check_sw_stts)
{
	u32 val;

	if (check_sw_stts && (dev->link_status != EP_PCIE_LINK_ENABLED)) {
		EP_PCIE_DBG(dev, "PCIe V%d: The link is not enabled.\n",
			dev->rev);
		return false;
	}

	val = readl_relaxed(dev->dm_core);
	EP_PCIE_DBG(dev, "PCIe V%d: device ID and vender ID are 0x%x.\n",
		dev->rev, val);
	if (val == EP_PCIE_LINK_DOWN) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: The link is not really up; device ID and vender ID are 0x%x.\n",
			dev->rev, val);
		return false;
	}

	return true;
}

static int ep_pcie_gpio_init(struct ep_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct ep_pcie_gpio_info_t *info;

	EP_PCIE_DBG(dev, "PCIe V%d\n", dev->rev);

	for (i = 0; i < EP_PCIE_MAX_GPIO; i++) {
		info = &dev->gpio[i];

		if (!info->num) {
			if (i == EP_PCIE_GPIO_MDM2AP) {
				EP_PCIE_DBG(dev,
					"PCIe V%d: gpio %s does not exist.\n",
					dev->rev, info->name);
				continue;
			} else {
				EP_PCIE_ERR(dev,
					"PCIe V%d:  the number of gpio %s is invalid\n",
					dev->rev, info->name);
				rc = -EINVAL;
				break;
			}
		}

		rc = gpio_request(info->num, info->name);
		if (rc) {
			EP_PCIE_ERR(dev, "PCIe V%d:  can't get gpio %s; %d\n",
				dev->rev, info->name, rc);
			break;
		}

		if (info->out)
			rc = gpio_direction_output(info->num, info->init);
		else
			rc = gpio_direction_input(info->num);
		if (rc) {
			EP_PCIE_ERR(dev,
				"PCIe V%d:  can't set direction for GPIO %s:%d\n",
				dev->rev, info->name, rc);
			gpio_free(info->num);
			break;
		}
	}

	if (rc)
		while (i--)
			gpio_free(dev->gpio[i].num);

	return rc;
}

static void ep_pcie_gpio_deinit(struct ep_pcie_dev_t *dev)
{
	int i;

	EP_PCIE_DBG(dev, "PCIe V%d\n", dev->rev);

	for (i = 0; i < EP_PCIE_MAX_GPIO; i++)
		gpio_free(dev->gpio[i].num);
}

static int ep_pcie_vreg_init(struct ep_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct regulator *vreg;
	struct ep_pcie_vreg_info_t *info;

	EP_PCIE_DBG(dev, "PCIe V%d\n", dev->rev);

	for (i = 0; i < EP_PCIE_MAX_VREG; i++) {
		info = &dev->vreg[i];
		vreg = info->hdl;

		if (!vreg) {
			EP_PCIE_ERR(dev,
				"PCIe V%d:  handle of Vreg %s is NULL\n",
				dev->rev, info->name);
			rc = -EINVAL;
			break;
		}

		EP_PCIE_DBG(dev, "PCIe V%d: Vreg %s is being enabled\n",
			dev->rev, info->name);
		if (info->max_v) {
			rc = regulator_set_voltage(vreg,
						   info->min_v, info->max_v);
			if (rc) {
				EP_PCIE_ERR(dev,
					"PCIe V%d:  can't set voltage for %s: %d\n",
					dev->rev, info->name, rc);
				break;
			}
		}

		if (info->opt_mode) {
			rc = regulator_set_optimum_mode(vreg, info->opt_mode);
			if (rc < 0) {
				EP_PCIE_ERR(dev,
					"PCIe V%d:  can't set mode for %s: %d\n",
					dev->rev, info->name, rc);
				break;
			}
		}

		rc = regulator_enable(vreg);
		if (rc) {
			EP_PCIE_ERR(dev,
				"PCIe V%d:  can't enable regulator %s: %d\n",
				dev->rev, info->name, rc);
			break;
		}
	}

	if (rc)
		while (i--) {
			struct regulator *hdl = dev->vreg[i].hdl;
			if (hdl)
				regulator_disable(hdl);
		}

	return rc;
}

static void ep_pcie_vreg_deinit(struct ep_pcie_dev_t *dev)
{
	int i;

	EP_PCIE_DBG(dev, "PCIe V%d\n", dev->rev);

	for (i = EP_PCIE_MAX_VREG - 1; i >= 0; i--) {
		if (dev->vreg[i].hdl) {
			EP_PCIE_DBG(dev, "Vreg %s is being disabled\n",
				dev->vreg[i].name);
			regulator_disable(dev->vreg[i].hdl);
		}
	}
}

static int ep_pcie_clk_init(struct ep_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct ep_pcie_clk_info_t *info;

	EP_PCIE_DBG(dev, "PCIe V%d\n", dev->rev);

	rc = regulator_enable(dev->gdsc);

	if (rc) {
		EP_PCIE_ERR(dev, "PCIe V%d: fail to enable GDSC for %s\n",
			dev->rev, dev->pdev->name);
		return rc;
	}

	if (dev->bus_client) {
		rc = msm_bus_scale_client_update_request(dev->bus_client, 1);
		if (rc) {
			EP_PCIE_ERR(dev,
				"PCIe V%d: fail to set bus bandwidth:%d.\n",
				dev->rev, rc);
			return rc;
		} else {
			EP_PCIE_DBG(dev,
				"PCIe V%d: set bus bandwidth.\n",
				dev->rev);
		}
	}

	for (i = 0; i < EP_PCIE_MAX_CLK; i++) {
		info = &dev->clk[i];

		if (!info->hdl) {
			EP_PCIE_DBG(dev,
				"PCIe V%d:  handle of Clock %s is NULL\n",
				dev->rev, info->name);
			continue;
		}

		if (info->freq) {
			rc = clk_set_rate(info->hdl, info->freq);
			if (rc) {
				EP_PCIE_ERR(dev,
					"PCIe V%d: can't set rate for clk %s: %d.\n",
					dev->rev, info->name, rc);
				break;
			} else {
				EP_PCIE_DBG(dev,
					"PCIe V%d: set rate for clk %s.\n",
					dev->rev, info->name);
			}
		}

		rc = clk_prepare_enable(info->hdl);

		if (rc)
			EP_PCIE_ERR(dev, "PCIe V%d:  failed to enable clk %s\n",
				dev->rev, info->name);
		else
			EP_PCIE_DBG(dev, "PCIe V%d:  enable clk %s.\n",
				dev->rev, info->name);
	}

	if (rc) {
		EP_PCIE_DBG(dev,
			"PCIe V%d: disable clocks for error handling.\n",
			dev->rev);
		while (i--) {
			struct clk *hdl = dev->clk[i].hdl;
			if (hdl)
				clk_disable_unprepare(hdl);
		}

		regulator_disable(dev->gdsc);
	}

	return rc;
}

static void ep_pcie_clk_deinit(struct ep_pcie_dev_t *dev)
{
	int i;
	int rc;

	EP_PCIE_DBG(dev, "PCIe V%d\n", dev->rev);

	for (i = EP_PCIE_MAX_CLK - 1; i >= 0; i--)
		if (dev->clk[i].hdl)
			clk_disable_unprepare(dev->clk[i].hdl);

	if (dev->bus_client) {
		rc = msm_bus_scale_client_update_request(dev->bus_client, 0);
		if (rc)
			EP_PCIE_ERR(dev,
				"PCIe V%d: fail to relinquish bus bandwidth:%d.\n",
				dev->rev, rc);
		else
			EP_PCIE_DBG(dev,
				"PCIe V%d: relinquish bus bandwidth.\n",
				dev->rev);
	}

	regulator_disable(dev->gdsc);
}

static int ep_pcie_pipe_clk_init(struct ep_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct ep_pcie_clk_info_t *info;

	EP_PCIE_DBG(dev, "PCIe V%d\n", dev->rev);

	for (i = 0; i < EP_PCIE_MAX_PIPE_CLK; i++) {
		info = &dev->pipeclk[i];

		if (!info->hdl) {
			EP_PCIE_ERR(dev,
				"PCIe V%d:  handle of Pipe Clock %s is NULL\n",
				dev->rev, info->name);
			rc = -EINVAL;
			break;
		}

		clk_reset(info->hdl, CLK_RESET_DEASSERT);

		if (info->freq) {
			rc = clk_set_rate(info->hdl, info->freq);
			if (rc) {
				EP_PCIE_ERR(dev,
					"PCIe V%d: can't set rate for clk %s: %d.\n",
					dev->rev, info->name, rc);
				break;
			} else {
				EP_PCIE_DBG(dev,
					"PCIe V%d: set rate for clk %s\n",
					dev->rev, info->name);
			}
		}

		rc = clk_prepare_enable(info->hdl);

		if (rc)
			EP_PCIE_ERR(dev, "PCIe V%d: failed to enable clk %s.\n",
				dev->rev, info->name);
		else
			EP_PCIE_DBG(dev, "PCIe V%d: enabled pipe clk %s.\n",
				dev->rev, info->name);
	}

	if (rc) {
		EP_PCIE_DBG(dev,
			"PCIe V%d: disable pipe clocks for error handling.\n",
			dev->rev);
		while (i--)
			if (dev->pipeclk[i].hdl)
				clk_disable_unprepare(dev->pipeclk[i].hdl);
	}

	return rc;
}

static void ep_pcie_pipe_clk_deinit(struct ep_pcie_dev_t *dev)
{
	int i;

	EP_PCIE_DBG(dev, "PCIe V%d\n", dev->rev);

	for (i = 0; i < EP_PCIE_MAX_PIPE_CLK; i++)
		if (dev->pipeclk[i].hdl)
			clk_disable_unprepare(
				dev->pipeclk[i].hdl);
}

static void ep_pcie_bar_init(struct ep_pcie_dev_t *dev)
{
	struct resource *res = dev->res[EP_PCIE_RES_MMIO].resource;
	u32 mask = res->end - res->start;
	u32 properties = 0x4;

	EP_PCIE_DBG(dev, "PCIe V%d: BAR mask to program is 0x%x\n",
			dev->rev, mask);

	/* Configure BAR mask via CS2 */
	ep_pcie_write_mask(dev->elbi + PCIE20_ELBI_CS2_ENABLE, 0, BIT(0));
	ep_pcie_write_reg(dev->dm_core, PCIE20_BAR0, mask);
	ep_pcie_write_reg(dev->dm_core, PCIE20_BAR0 + 0x4, 0);
	ep_pcie_write_reg(dev->dm_core, PCIE20_BAR0 + 0x8, mask);
	ep_pcie_write_reg(dev->dm_core, PCIE20_BAR0 + 0xc, 0);
	ep_pcie_write_reg(dev->dm_core, PCIE20_BAR0 + 0x10, 0);
	ep_pcie_write_reg(dev->dm_core, PCIE20_BAR0 + 0x14, 0);
	ep_pcie_write_mask(dev->elbi + PCIE20_ELBI_CS2_ENABLE, BIT(0), 0);

	/* Configure BAR properties via CS */
	ep_pcie_write_mask(dev->dm_core + PCIE20_MISC_CONTROL_1, 0, BIT(0));
	ep_pcie_write_reg(dev->dm_core, PCIE20_BAR0, properties);
	ep_pcie_write_reg(dev->dm_core, PCIE20_BAR0 + 0x8, properties);
	ep_pcie_write_mask(dev->dm_core + PCIE20_MISC_CONTROL_1, BIT(0), 0);
}

static void ep_pcie_core_init(struct ep_pcie_dev_t *dev, bool configured)
{
	EP_PCIE_DBG(dev, "PCIe V%d\n", dev->rev);

	/* enable debug IRQ */
	ep_pcie_write_mask(dev->parf + PCIE20_PARF_DEBUG_INT_EN,
			0, BIT(3) | BIT(2) | BIT(1));

	if (!configured) {
		/* Configure PCIe to endpoint mode */
		ep_pcie_write_reg(dev->parf, PCIE20_PARF_DEVICE_TYPE, 0x0);

		/* adjust DBI base address */
		if (dev->dbi_base_reg)
			writel_relaxed(0x3FFFE000,
				dev->parf + dev->dbi_base_reg);
		else
			writel_relaxed(0x3FFFE000,
				dev->parf + PCIE20_PARF_DBI_BASE_ADDR);

		/* Configure PCIe core to support 1GB aperture */
		if (dev->slv_space_reg)
			ep_pcie_write_reg(dev->parf, dev->slv_space_reg,
				0x40000000);
		else
			ep_pcie_write_reg(dev->parf,
				PCIE20_PARF_SLV_ADDR_SPACE_SIZE, 0x40000000);

		/* Configure link speed */
		ep_pcie_write_mask(dev->dm_core +
				PCIE20_LINK_CONTROL2_LINK_STATUS2,
				0xf, dev->link_speed);
	}

	/* Read halts write */
	ep_pcie_write_mask(dev->parf + PCIE20_PARF_AXI_MSTR_RD_HALT_NO_WRITES,
			0, BIT(0));

	/* Write after write halt */
	ep_pcie_write_mask(dev->parf + PCIE20_PARF_AXI_MSTR_WR_ADDR_HALT,
			0, BIT(31));

	/* Q2A flush disable */
	writel_relaxed(0, dev->parf + PCIE20_PARF_Q2A_FLUSH);

	/* Disable the DBI Wakeup */
	ep_pcie_write_mask(dev->parf + PCIE20_PARF_SYS_CTRL, BIT(11), 0);

	/* Disable the debouncers */
	ep_pcie_write_reg(dev->parf, PCIE20_PARF_DB_CTRL, 0x73);

	/* Disable core clock CGC */
	ep_pcie_write_mask(dev->parf + PCIE20_PARF_SYS_CTRL, 0, BIT(6));

	/* Set AUX power to be on */
	ep_pcie_write_mask(dev->parf + PCIE20_PARF_SYS_CTRL, 0, BIT(4));

	EP_PCIE_DBG(dev,
		"Initial: CLASS_CODE_REVISION_ID:0x%x; HDR_TYPE:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_CLASS_CODE_REVISION_ID),
		readl_relaxed(dev->dm_core + PCIE20_BIST_HDR_TYPE));

	if (!configured) {
		/* Enable CS for RO(CS) register writes */
		ep_pcie_write_mask(dev->dm_core + PCIE20_MISC_CONTROL_1, 0,
			BIT(0));

		/* Set class code and revision ID */
		ep_pcie_write_reg(dev->dm_core, PCIE20_CLASS_CODE_REVISION_ID,
			0xff000000);

		/* Set header type */
		ep_pcie_write_reg(dev->dm_core, PCIE20_BIST_HDR_TYPE, 0x10);

		/* Set Subsystem ID and Subsystem Vendor ID */
		ep_pcie_write_reg(dev->dm_core, PCIE20_SUBSYSTEM, 0xa01f17cb);

		/* Set the PMC Register - to support PME in D0/D3hot/D3cold */
		ep_pcie_write_mask(dev->dm_core + PCIE20_CAP_ID_NXT_PTR, 0,
						BIT(31)|BIT(30)|BIT(27));

		/* Set the Endpoint L0s Acceptable Latency to 1us (max) */
		ep_pcie_write_reg_field(dev->dm_core,
			PCIE20_DEVICE_CAPABILITIES,
			PCIE20_MASK_EP_L0S_ACCPT_LATENCY, 0x7);

		/* Set the Endpoint L1 Acceptable Latency to 2 us (max) */
		ep_pcie_write_reg_field(dev->dm_core,
			PCIE20_DEVICE_CAPABILITIES,
			PCIE20_MASK_EP_L1_ACCPT_LATENCY, 0x7);

		/* Set the L0s Exit Latency to 2us-4us = 0x6 */
		ep_pcie_write_reg_field(dev->dm_core, PCIE20_LINK_CAPABILITIES,
			PCIE20_MASK_L1_EXIT_LATENCY, 0x6);

		/* Set the L1 Exit Latency to be 32us-64 us = 0x6 */
		ep_pcie_write_reg_field(dev->dm_core, PCIE20_LINK_CAPABILITIES,
			PCIE20_MASK_L0S_EXIT_LATENCY, 0x6);

		/* L1ss is supported */
		ep_pcie_write_mask(dev->dm_core + PCIE20_L1SUB_CAPABILITY, 0,
			0x1f);

		/* Enable Clock Power Management */
		ep_pcie_write_reg_field(dev->dm_core, PCIE20_LINK_CAPABILITIES,
			PCIE20_MASK_CLOCK_POWER_MAN, 0x1);

		/* Disable CS for RO(CS) register writes */
		ep_pcie_write_mask(dev->dm_core + PCIE20_MISC_CONTROL_1, BIT(0),
			0);

		/* Set FTS value to match the PHY setting */
		ep_pcie_write_reg_field(dev->dm_core,
			PCIE20_ACK_F_ASPM_CTRL_REG,
			PCIE20_MASK_ACK_N_FTS, 0x80);

		EP_PCIE_DBG(dev,
			"After program: CLASS_CODE_REVISION_ID:0x%x; HDR_TYPE:0x%x; L1SUB_CAPABILITY:0x%x; PARF_SYS_CTRL:0x%x\n",
			readl_relaxed(dev->dm_core +
				PCIE20_CLASS_CODE_REVISION_ID),
			readl_relaxed(dev->dm_core + PCIE20_BIST_HDR_TYPE),
			readl_relaxed(dev->dm_core + PCIE20_L1SUB_CAPABILITY),
			readl_relaxed(dev->parf + PCIE20_PARF_SYS_CTRL));

		/* Configure BARs */
		ep_pcie_bar_init(dev);

		ep_pcie_write_reg(dev->mmio, PCIE20_MHICFG, 0x02800880);
		ep_pcie_write_reg(dev->mmio, PCIE20_BHI_EXECENV, 0x2);
	}

	/* Configure IRQ events */
	if (dev->aggregated_irq) {
		ep_pcie_write_reg(dev->parf, PCIE20_PARF_INT_ALL_MASK, 0);
		ep_pcie_write_mask(dev->parf + PCIE20_PARF_INT_ALL_MASK, 0,
			BIT(EP_PCIE_INT_EVT_LINK_DOWN) |
			BIT(EP_PCIE_INT_EVT_BME) |
			BIT(EP_PCIE_INT_EVT_PM_TURNOFF) |
			BIT(EP_PCIE_INT_EVT_MHI_A7) |
			BIT(EP_PCIE_INT_EVT_DSTATE_CHANGE) |
			BIT(EP_PCIE_INT_EVT_LINK_UP));
		EP_PCIE_DBG(dev, "PCIe V%d: PCIE20_PARF_INT_ALL_MASK:0x%x\n",
			dev->rev,
			readl_relaxed(dev->parf + PCIE20_PARF_INT_ALL_MASK));
	}

	if (dev->active_config) {
		ep_pcie_write_reg(dev->dm_core, PCIE20_AUX_CLK_FREQ_REG, 0x14);

		EP_PCIE_DBG2(dev, "PCIe V%d: Enable L1.\n", dev->rev);
		ep_pcie_write_mask(dev->parf + PCIE20_PARF_PM_CTRL, BIT(5), 0);
	}
}

static void ep_pcie_config_inbound_iatu(struct ep_pcie_dev_t *dev)
{
	struct resource *mmio = dev->res[EP_PCIE_RES_MMIO].resource;
	u32 lower, limit, bar;

	lower = mmio->start;
	limit = mmio->end;
	bar = readl_relaxed(dev->dm_core + PCIE20_BAR0);

	EP_PCIE_DBG(dev,
		"PCIe V%d: BAR0 is 0x%x; MMIO[0x%x-0x%x]\n",
		dev->rev, bar, lower, limit);

	ep_pcie_write_reg(dev->parf, PCIE20_PARF_MHI_BASE_ADDR_LOWER, lower);
	ep_pcie_write_reg(dev->parf, PCIE20_PARF_MHI_BASE_ADDR_UPPER, 0x0);

	/* program inbound address translation using region 0 */
	ep_pcie_write_reg(dev->dm_core, PCIE20_PLR_IATU_VIEWPORT, 0x80000000);
	/* set region to mem type */
	ep_pcie_write_reg(dev->dm_core, PCIE20_PLR_IATU_CTRL1, 0x0);
	/* setup target address registers */
	ep_pcie_write_reg(dev->dm_core, PCIE20_PLR_IATU_LTAR, lower);
	ep_pcie_write_reg(dev->dm_core, PCIE20_PLR_IATU_UTAR, 0x0);
	/* use BAR match mode for BAR0 and enable region 0 */
	ep_pcie_write_reg(dev->dm_core, PCIE20_PLR_IATU_CTRL2, 0xc0000000);

	EP_PCIE_DBG(dev, "PCIE20_PLR_IATU_VIEWPORT:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_VIEWPORT));
	EP_PCIE_DBG(dev, "PCIE20_PLR_IATU_CTRL1:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_CTRL1));
	EP_PCIE_DBG(dev, "PCIE20_PLR_IATU_LTAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LTAR));
	EP_PCIE_DBG(dev, "PCIE20_PLR_IATU_UTAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_UTAR));
	EP_PCIE_DBG(dev, "PCIE20_PLR_IATU_CTRL2:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_CTRL2));
}

static void ep_pcie_config_outbound_iatu_entry(struct ep_pcie_dev_t *dev,
					u32 region, u32 lower, u32 upper,
					u32 limit, u32 tgt_lower, u32 tgt_upper)
{
	EP_PCIE_DBG(dev,
		"PCIe V%d: region:%d; lower:0x%x; limit:0x%x; target_lower:0x%x; target_upper:0x%x\n",
		dev->rev, region, lower, limit, tgt_lower, tgt_upper);

	/* program outbound address translation using an input region */
	ep_pcie_write_reg(dev->dm_core, PCIE20_PLR_IATU_VIEWPORT, region);
	/* set region to mem type */
	ep_pcie_write_reg(dev->dm_core, PCIE20_PLR_IATU_CTRL1, 0x0);
	/* setup source address registers */
	ep_pcie_write_reg(dev->dm_core, PCIE20_PLR_IATU_LBAR, lower);
	ep_pcie_write_reg(dev->dm_core, PCIE20_PLR_IATU_UBAR, upper);
	ep_pcie_write_reg(dev->dm_core, PCIE20_PLR_IATU_LAR, limit);
	/* setup target address registers */
	ep_pcie_write_reg(dev->dm_core, PCIE20_PLR_IATU_LTAR, tgt_lower);
	ep_pcie_write_reg(dev->dm_core, PCIE20_PLR_IATU_UTAR, tgt_upper);
	/* use DMA bypass mode and enable the region */
	ep_pcie_write_mask(dev->dm_core + PCIE20_PLR_IATU_CTRL2, 0,
				BIT(31) | BIT(27));

	EP_PCIE_DBG(dev, "PCIE20_PLR_IATU_VIEWPORT:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_VIEWPORT));
	EP_PCIE_DBG(dev, "PCIE20_PLR_IATU_CTRL1:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_CTRL1));
	EP_PCIE_DBG(dev, "PCIE20_PLR_IATU_LBAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LBAR));
	EP_PCIE_DBG(dev, "PCIE20_PLR_IATU_UBAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_UBAR));
	EP_PCIE_DBG(dev, "PCIE20_PLR_IATU_LAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LAR));
	EP_PCIE_DBG(dev, "PCIE20_PLR_IATU_LTAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LTAR));
	EP_PCIE_DBG(dev, "PCIE20_PLR_IATU_UTAR:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_UTAR));
	EP_PCIE_DBG(dev, "PCIE20_PLR_IATU_CTRL2:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_CTRL2));
}

static void ep_pcie_notify_event(struct ep_pcie_dev_t *dev,
					enum ep_pcie_event event)
{
	if (dev->event_reg && dev->event_reg->callback &&
		(dev->event_reg->events & event)) {
			struct ep_pcie_notify *notify =
				&dev->event_reg->notify;
			notify->event = event;
			notify->user = dev->event_reg->user;
			EP_PCIE_DBG(&ep_pcie_dev,
				"PCIe V%d: Callback client for event %d.\n",
				dev->rev, event);
			dev->event_reg->callback(notify);
		} else {
			EP_PCIE_DBG(&ep_pcie_dev,
				"PCIe V%d: Client does not register for event %d.\n",
				dev->rev, event);
		}
}

static int ep_pcie_get_resources(struct ep_pcie_dev_t *dev,
					struct platform_device *pdev)
{
	int i, len, cnt, ret = 0, size = 0;
	struct ep_pcie_vreg_info_t *vreg_info;
	struct ep_pcie_gpio_info_t *gpio_info;
	struct ep_pcie_clk_info_t  *clk_info;
	struct resource *res;
	struct ep_pcie_res_info_t *res_info;
	struct ep_pcie_irq_info_t *irq_info;
	char prop_name[MAX_PROP_SIZE];
	const __be32 *prop;
	u32 *clkfreq = NULL;

	EP_PCIE_DBG(dev, "PCIe V%d\n", dev->rev);

	of_get_property(pdev->dev.of_node, "qcom,phy-init", &size);
	if (size) {
		dev->phy_init = (struct ep_pcie_phy_info_t *)
			devm_kzalloc(&pdev->dev, size, GFP_KERNEL);

		if (dev->phy_init) {
			dev->phy_init_len =
				size / sizeof(*dev->phy_init);
			EP_PCIE_DBG(dev,
					"PCIe V%d: phy init length is 0x%x.\n",
					dev->rev, dev->phy_init_len);

			of_property_read_u32_array(pdev->dev.of_node,
				"qcom,phy-init",
				(unsigned int *)dev->phy_init,
				size / sizeof(dev->phy_init->offset));
		} else {
			EP_PCIE_ERR(dev,
					"PCIe V%d: Could not allocate memory for phy init sequence.\n",
					dev->rev);
			return -ENOMEM;
		}
	} else {
		EP_PCIE_DBG(dev,
			"PCIe V%d: PHY V%d: phy init sequence is not present in DT.\n",
			dev->rev, dev->phy_rev);
	}

	cnt = of_property_count_strings((&pdev->dev)->of_node,
			"clock-names");
	if (cnt > 0) {
		clkfreq = kzalloc(cnt * sizeof(*clkfreq),
					GFP_KERNEL);
		if (!clkfreq) {
			EP_PCIE_ERR(dev, "PCIe V%d: memory alloc failed\n",
					dev->rev);
			return -ENOMEM;
		}
		ret = of_property_read_u32_array(
			(&pdev->dev)->of_node,
			"max-clock-frequency-hz", clkfreq, cnt);
		if (ret) {
			EP_PCIE_ERR(dev,
				"PCIe V%d: invalid max-clock-frequency-hz property:%d\n",
				dev->rev, ret);
			goto out;
		}
	}

	for (i = 0; i < EP_PCIE_MAX_VREG; i++) {
		vreg_info = &dev->vreg[i];
		vreg_info->hdl =
			devm_regulator_get(&pdev->dev, vreg_info->name);

		if (PTR_ERR(vreg_info->hdl) == -EPROBE_DEFER) {
			EP_PCIE_DBG(dev, "EPROBE_DEFER for VReg:%s\n",
				vreg_info->name);
			ret = PTR_ERR(vreg_info->hdl);
			goto out;
		}

		if (IS_ERR(vreg_info->hdl)) {
			if (vreg_info->required) {
				EP_PCIE_ERR(dev, "Vreg %s doesn't exist\n",
					vreg_info->name);
				ret = PTR_ERR(vreg_info->hdl);
				goto out;
			} else {
				EP_PCIE_DBG(dev,
					"Optional Vreg %s doesn't exist\n",
					vreg_info->name);
				vreg_info->hdl = NULL;
			}
		} else {
			snprintf(prop_name, MAX_PROP_SIZE,
				"qcom,%s-voltage-level", vreg_info->name);
			prop = of_get_property((&pdev->dev)->of_node,
						prop_name, &len);
			if (!prop || (len != (3 * sizeof(__be32)))) {
				EP_PCIE_DBG(dev, "%s %s property\n",
					prop ? "invalid format" :
					"no", prop_name);
			} else {
				vreg_info->max_v = be32_to_cpup(&prop[0]);
				vreg_info->min_v = be32_to_cpup(&prop[1]);
				vreg_info->opt_mode =
					be32_to_cpup(&prop[2]);
			}
		}
	}

	dev->gdsc = devm_regulator_get(&pdev->dev, "gdsc-vdd");

	if (IS_ERR(dev->gdsc)) {
		EP_PCIE_ERR(dev, "PCIe V%d:  Failed to get %s GDSC:%ld\n",
			dev->rev, dev->pdev->name, PTR_ERR(dev->gdsc));
		if (PTR_ERR(dev->gdsc) == -EPROBE_DEFER)
			EP_PCIE_DBG(dev, "PCIe V%d: EPROBE_DEFER for %s GDSC\n",
			dev->rev, dev->pdev->name);
		ret = PTR_ERR(dev->gdsc);
		goto out;
	}

	for (i = 0; i < EP_PCIE_MAX_GPIO; i++) {
		gpio_info = &dev->gpio[i];
		ret = of_get_named_gpio((&pdev->dev)->of_node,
					gpio_info->name, 0);
		if (ret >= 0) {
			gpio_info->num = ret;
			ret = 0;
			EP_PCIE_DBG(dev, "GPIO num for %s is %d\n",
				gpio_info->name, gpio_info->num);
		} else {
			EP_PCIE_DBG(dev,
				"GPIO %s is not supported in this configuration.\n",
				gpio_info->name);
			ret = 0;
		}
	}

	for (i = 0; i < EP_PCIE_MAX_CLK; i++) {
		clk_info = &dev->clk[i];

		clk_info->hdl = devm_clk_get(&pdev->dev, clk_info->name);

		if (IS_ERR(clk_info->hdl)) {
			if (clk_info->required) {
				EP_PCIE_ERR(dev,
					"Clock %s isn't available:%ld\n",
					clk_info->name, PTR_ERR(clk_info->hdl));
				ret = PTR_ERR(clk_info->hdl);
				goto out;
			} else {
				EP_PCIE_DBG(dev, "Ignoring Clock %s\n",
					clk_info->name);
				clk_info->hdl = NULL;
			}
		} else {
			if (clkfreq != NULL) {
				clk_info->freq = clkfreq[i +
					EP_PCIE_MAX_PIPE_CLK];
				EP_PCIE_DBG(dev, "Freq of Clock %s is:%d\n",
					clk_info->name, clk_info->freq);
			}
		}
	}

	for (i = 0; i < EP_PCIE_MAX_PIPE_CLK; i++) {
		clk_info = &dev->pipeclk[i];

		clk_info->hdl = devm_clk_get(&pdev->dev, clk_info->name);

		if (IS_ERR(clk_info->hdl)) {
			if (clk_info->required) {
				EP_PCIE_ERR(dev,
					"Clock %s isn't available:%ld\n",
					clk_info->name, PTR_ERR(clk_info->hdl));
				ret = PTR_ERR(clk_info->hdl);
				goto out;
			} else {
				EP_PCIE_DBG(dev, "Ignoring Clock %s\n",
					clk_info->name);
				clk_info->hdl = NULL;
			}
		} else {
			if (clkfreq != NULL) {
				clk_info->freq = clkfreq[i];
				EP_PCIE_DBG(dev, "Freq of Clock %s is:%d\n",
					clk_info->name, clk_info->freq);
			}
		}
	}

	dev->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (!dev->bus_scale_table) {
		EP_PCIE_DBG(dev, "PCIe V%d: No bus scale table for %s\n",
			dev->rev, dev->pdev->name);
		dev->bus_client = 0;
	} else {
		dev->bus_client =
			msm_bus_scale_register_client(dev->bus_scale_table);
		if (!dev->bus_client) {
			EP_PCIE_ERR(dev,
				"PCIe V%d: Failed to register bus client for %s\n",
				dev->rev, dev->pdev->name);
			msm_bus_cl_clear_pdata(dev->bus_scale_table);
			ret = -ENODEV;
			goto out;
		}
	}

	for (i = 0; i < EP_PCIE_MAX_RES; i++) {
		res_info = &dev->res[i];

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							res_info->name);

		if (!res) {
			EP_PCIE_ERR(dev,
				"PCIe V%d: can't get resource for %s.\n",
				dev->rev, res_info->name);
			ret = -ENOMEM;
			goto out;
		} else {
			EP_PCIE_DBG(dev, "start addr for %s is %pa.\n",
				res_info->name,	&res->start);
		}

		res_info->base = devm_ioremap(&pdev->dev,
						res->start, resource_size(res));
		if (!res_info->base) {
			EP_PCIE_ERR(dev, "PCIe V%d: can't remap %s.\n",
				dev->rev, res_info->name);
			ret = -ENOMEM;
			goto out;
		}
		res_info->resource = res;
	}

	for (i = 0; i < EP_PCIE_MAX_IRQ; i++) {
		irq_info = &dev->irq[i];

		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							irq_info->name);

		if (!res) {
			EP_PCIE_DBG2(dev, "PCIe V%d: can't find IRQ # for %s\n",
				dev->rev, irq_info->name);
		} else {
			irq_info->num = res->start;
			EP_PCIE_DBG2(dev, "IRQ # for %s is %d.\n",
				irq_info->name,	irq_info->num);
		}
	}

	dev->parf = dev->res[EP_PCIE_RES_PARF].base;
	dev->phy = dev->res[EP_PCIE_RES_PHY].base;
	dev->mmio = dev->res[EP_PCIE_RES_MMIO].base;
	dev->msi = dev->res[EP_PCIE_RES_MSI].base;
	dev->dm_core = dev->res[EP_PCIE_RES_DM_CORE].base;
	dev->elbi = dev->res[EP_PCIE_RES_ELBI].base;

out:
	kfree(clkfreq);
	return ret;
}

static void ep_pcie_release_resources(struct ep_pcie_dev_t *dev)
{
	dev->parf = NULL;
	dev->elbi = NULL;
	dev->dm_core = NULL;
	dev->phy = NULL;
	dev->mmio = NULL;
	dev->msi = NULL;
}

static void ep_pcie_enumeration_complete(struct ep_pcie_dev_t *dev)
{
	dev->enumerated = true;
	dev->link_status = EP_PCIE_LINK_ENABLED;

	if (dev->gpio[EP_PCIE_GPIO_MDM2AP].num) {
		/* assert MDM2AP Status GPIO */
		EP_PCIE_DBG2(dev, "PCIe V%d: assert MDM2AP Status.\n",
				dev->rev);
		EP_PCIE_DBG(dev,
			"PCIe V%d: MDM2APStatus GPIO initial:%d.\n",
			dev->rev,
			gpio_get_value(
			dev->gpio[EP_PCIE_GPIO_MDM2AP].num));
		gpio_set_value(dev->gpio[EP_PCIE_GPIO_MDM2AP].num,
			dev->gpio[EP_PCIE_GPIO_MDM2AP].on);
		EP_PCIE_DBG(dev,
			"PCIe V%d: MDM2APStatus GPIO after assertion:%d.\n",
			dev->rev,
			gpio_get_value(
			dev->gpio[EP_PCIE_GPIO_MDM2AP].num));
	}

	hw_drv.device_id = readl_relaxed(dev->dm_core);
	EP_PCIE_DBG(&ep_pcie_dev,
		"PCIe V%d: register driver for device 0x%x.\n",
		ep_pcie_dev.rev, hw_drv.device_id);
	ep_pcie_register_drv(&hw_drv);
	ep_pcie_notify_event(dev, EP_PCIE_EVENT_LINKUP);

	return;
}

int ep_pcie_core_enable_endpoint(enum ep_pcie_options opt)
{
	int ret = 0;
	u32 val = 0;
	u32 retries = 0;
	u32 bme = 0;
	bool ltssm_en = false;
	struct ep_pcie_dev_t *dev = &ep_pcie_dev;

	EP_PCIE_DBG(dev, "PCIe V%d: options input are 0x%x.\n", dev->rev, opt);

	mutex_lock(&dev->setup_mtx);

	if (dev->link_status == EP_PCIE_LINK_ENABLED) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: link is already enabled.\n",
			dev->rev);
		goto out;
	}

	if (dev->link_status == EP_PCIE_LINK_UP)
		EP_PCIE_DBG(dev,
			"PCIe V%d: link is already up, let's proceed with the voting for the resources.\n",
			dev->rev);

	if (dev->power_on && (opt & EP_PCIE_OPT_POWER_ON)) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: request to turn on the power when link is already powered on.\n",
			dev->rev);
		ret = EP_PCIE_ERROR;
		goto out;
	}

	if (opt & EP_PCIE_OPT_POWER_ON) {
		/* enable power */
		ret = ep_pcie_vreg_init(dev);
		if (ret) {
			EP_PCIE_ERR(dev, "PCIe V%d: failed to enable Vreg\n",
				dev->rev);
			goto out;
		}

		/* enable clocks */
		ret = ep_pcie_clk_init(dev);
		if (ret) {
			EP_PCIE_ERR(dev, "PCIe V%d: failed to enable clocks\n",
				dev->rev);
			goto clk_fail;
		}

		/* enable pipe clock */
		ret = ep_pcie_pipe_clk_init(dev);
		if (ret) {
			EP_PCIE_ERR(dev,
				"PCIe V%d: failed to enable pipe clock\n",
				dev->rev);
			goto pipe_clk_fail;
		}

		dev->power_on = true;
	}

	if (!(opt & EP_PCIE_OPT_ENUM))
		goto out;

	/* check link status during initial bootup */
	if (!dev->enumerated) {
		val = readl_relaxed(dev->parf + PCIE20_PARF_PM_STTS);
		val = val & PARF_XMLH_LINK_UP;
		EP_PCIE_DBG(dev, "PCIe V%d: Link status is 0x%x.\n", dev->rev,
				val);
		if (val) {
			EP_PCIE_INFO(dev,
				"PCIe V%d: link initialized by bootloader for LE PCIe endpoint; skip link training in HLOS.\n",
				dev->rev);
			ep_pcie_core_init(dev, true);
			dev->link_status = EP_PCIE_LINK_UP;
			dev->l23_ready = false;
			goto checkbme;
		} else {
			ltssm_en = readl_relaxed(dev->parf
					+ PCIE20_PARF_LTSSM) & BIT(8);

			if (ltssm_en) {
				EP_PCIE_ERR(dev,
					"PCIe V%d: link is not up when LTSSM has already enabled by bootloader.\n",
					dev->rev);
				ret = EP_PCIE_ERROR;
				goto link_fail;
			} else {
				EP_PCIE_DBG(dev,
					"PCIe V%d: Proceed with regular link training.\n",
					dev->rev);
			}
		}
	}

	if (opt & EP_PCIE_OPT_AST_WAKE) {
		/* assert PCIe WAKE# */
		EP_PCIE_INFO(dev, "PCIe V%d: assert PCIe WAKE#.\n",
			dev->rev);
		EP_PCIE_DBG(dev, "PCIe V%d: WAKE GPIO initial:%d.\n",
			dev->rev,
			gpio_get_value(dev->gpio[EP_PCIE_GPIO_WAKE].num));
		gpio_set_value(dev->gpio[EP_PCIE_GPIO_WAKE].num,
				1 - dev->gpio[EP_PCIE_GPIO_WAKE].on);
		EP_PCIE_DBG(dev,
			"PCIe V%d: WAKE GPIO after deassertion:%d.\n",
			dev->rev,
			gpio_get_value(dev->gpio[EP_PCIE_GPIO_WAKE].num));
		gpio_set_value(dev->gpio[EP_PCIE_GPIO_WAKE].num,
				dev->gpio[EP_PCIE_GPIO_WAKE].on);
		EP_PCIE_DBG(dev,
			"PCIe V%d: WAKE GPIO after assertion:%d.\n",
			dev->rev,
			gpio_get_value(dev->gpio[EP_PCIE_GPIO_WAKE].num));
	}

	/* wait for host side to deassert PERST */
	retries = 0;
	do {
		if (gpio_get_value(dev->gpio[EP_PCIE_GPIO_PERST].num) == 1)
			break;
		retries++;
		usleep_range(PERST_TIMEOUT_US_MIN, PERST_TIMEOUT_US_MAX);
	} while (retries < PERST_CHECK_MAX_COUNT);

	EP_PCIE_DBG(dev, "PCIe V%d: number of PERST retries:%d.\n",
		dev->rev, retries);

	if (retries == PERST_CHECK_MAX_COUNT) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: PERST is not de-asserted by host\n",
			dev->rev);
		ret = EP_PCIE_ERROR;
		goto link_fail;
	} else {
		dev->perst_deast = true;
		if (opt & EP_PCIE_OPT_AST_WAKE) {
			/* deassert PCIe WAKE# */
			EP_PCIE_DBG(dev,
				"PCIe V%d: deassert PCIe WAKE# after PERST# is deasserted.\n",
				dev->rev);
			gpio_set_value(dev->gpio[EP_PCIE_GPIO_WAKE].num,
				1 - dev->gpio[EP_PCIE_GPIO_WAKE].on);
		}
	}

	/* init PCIe PHY */
	ep_pcie_phy_init(dev);

	EP_PCIE_DBG(dev, "PCIe V%d: waiting for phy ready...\n", dev->rev);
	retries = 0;
	do {
		if (ep_pcie_phy_is_ready(dev))
			break;
		retries++;
		if (retries % 100 == 0)
			EP_PCIE_DBG(dev,
				"PCIe V%d: current number of PHY retries:%d.\n",
				dev->rev, retries);
		usleep_range(REFCLK_STABILIZATION_DELAY_US_MIN,
				REFCLK_STABILIZATION_DELAY_US_MAX);
	} while (retries < PHY_READY_TIMEOUT_COUNT);

	EP_PCIE_DBG(dev, "PCIe V%d: number of PHY retries:%d.\n",
		dev->rev, retries);

	if (retries == PHY_READY_TIMEOUT_COUNT) {
		EP_PCIE_ERR(dev, "PCIe V%d: PCIe PHY  failed to come up!\n",
			dev->rev);
		ret = EP_PCIE_ERROR;
		ep_pcie_reg_dump(dev, BIT(EP_PCIE_RES_PHY), false);
		goto link_fail;
	} else {
		EP_PCIE_INFO(dev, "PCIe V%d: PCIe  PHY is ready!\n", dev->rev);
	}

	ep_pcie_core_init(dev, false);
	ep_pcie_config_inbound_iatu(dev);

	/* enable link training */
	if (dev->phy_rev >= 3)
		ep_pcie_write_mask(dev->parf + PCIE20_PARF_LTSSM, 0, BIT(8));
	else
		ep_pcie_write_mask(dev->elbi + PCIE20_ELBI_SYS_CTRL, 0, BIT(0));

	EP_PCIE_DBG(dev, "PCIe V%d: check if link is up\n", dev->rev);

	/* Wait for up to 100ms for the link to come up */
	retries = 0;
	do {
		usleep_range(LINK_UP_TIMEOUT_US_MIN, LINK_UP_TIMEOUT_US_MAX);
		val =  readl_relaxed(dev->elbi + PCIE20_ELBI_SYS_STTS);
		retries++;
		if (retries % 100 == 0)
			EP_PCIE_DBG(dev, "PCIe V%d: LTSSM_STATE:0x%x.\n",
					dev->rev, (val >> 0xC) & 0x3f);
	} while ((!(val & XMLH_LINK_UP) ||
		!ep_pcie_confirm_linkup(dev, false))
		&& (retries < LINK_UP_CHECK_MAX_COUNT));

	if (retries == LINK_UP_CHECK_MAX_COUNT) {
		EP_PCIE_ERR(dev, "PCIe V%d: link initialization failed\n",
			dev->rev);
		ret = EP_PCIE_ERROR;
		goto link_fail;
	} else {
		dev->link_status = EP_PCIE_LINK_UP;
		dev->l23_ready = false;
		EP_PCIE_DBG(dev,
			"PCIe V%d: link is up after %d checkings (%d ms)\n",
			dev->rev, retries,
			LINK_UP_TIMEOUT_US_MIN * retries / 1000);
		EP_PCIE_INFO(dev,
			"PCIe V%d: link initialized for LE PCIe endpoint\n",
			dev->rev);
	}

checkbme:
	if (dev->active_config) {
		ep_pcie_write_mask(dev->parf + PCIE20_PARF_SLV_ADDR_MSB_CTRL,
					0, BIT(0));
		ep_pcie_write_reg(dev->parf, PCIE20_PARF_SLV_ADDR_SPACE_SIZE_HI,
					0x200);
		ep_pcie_write_reg(dev->parf, PCIE20_PARF_SLV_ADDR_SPACE_SIZE,
					0x0);
		ep_pcie_write_reg(dev->parf, PCIE20_PARF_DBI_BASE_ADDR_HI,
					0x100);
		ep_pcie_write_reg(dev->parf, PCIE20_PARF_DBI_BASE_ADDR,
					0x7FFFE000);
	}

	if (!(opt & EP_PCIE_OPT_ENUM_ASYNC)) {
		/* Wait for up to 1000ms for BME to be set */
		retries = 0;

		bme = readl_relaxed(dev->dm_core +
		PCIE20_COMMAND_STATUS) & BIT(2);
		while (!bme && (retries < BME_CHECK_MAX_COUNT)) {
			retries++;
			usleep_range(BME_TIMEOUT_US_MIN, BME_TIMEOUT_US_MAX);
			bme = readl_relaxed(dev->dm_core +
				PCIE20_COMMAND_STATUS) & BIT(2);
		}
	} else {
		EP_PCIE_DBG(dev,
			"PCIe V%d: EP_PCIE_OPT_ENUM_ASYNC is true.\n",
			dev->rev);
		bme = readl_relaxed(dev->dm_core +
			PCIE20_COMMAND_STATUS) & BIT(2);
	}

	if (bme) {
		EP_PCIE_DBG(dev,
			"PCIe V%d: PCIe link is up and BME is enabled after %d checkings (%d ms).\n",
			dev->rev, retries,
			BME_TIMEOUT_US_MIN * retries / 1000);
		ep_pcie_enumeration_complete(dev);
		/* expose BAR to user space to identify modem */
		ep_pcie_bar0_address =
			readl_relaxed(dev->dm_core + PCIE20_BAR0);
	} else {
		if (!(opt & EP_PCIE_OPT_ENUM_ASYNC))
			EP_PCIE_ERR(dev,
				"PCIe V%d: PCIe link is up but BME is still disabled after max waiting time.\n",
				dev->rev);
		if (!ep_pcie_debug_keep_resource &&
				!(opt&EP_PCIE_OPT_ENUM_ASYNC)) {
			ret = EP_PCIE_ERROR;
			dev->link_status = EP_PCIE_LINK_DISABLED;
			goto link_fail;
		}
	}

	dev->suspending = false;
	goto out;

link_fail:
	dev->power_on = false;
	if (!ep_pcie_debug_keep_resource)
		ep_pcie_pipe_clk_deinit(dev);
pipe_clk_fail:
	if (!ep_pcie_debug_keep_resource)
		ep_pcie_clk_deinit(dev);
clk_fail:
	if (!ep_pcie_debug_keep_resource)
		ep_pcie_vreg_deinit(dev);
	else
		ret = 0;
out:
	mutex_unlock(&dev->setup_mtx);

	return ret;
}

int ep_pcie_core_disable_endpoint(void)
{
	int rc = 0;
	struct ep_pcie_dev_t *dev = &ep_pcie_dev;

	EP_PCIE_DBG(dev, "PCIe V%d\n", dev->rev);

	mutex_lock(&dev->setup_mtx);

	if (!dev->power_on) {
		EP_PCIE_DBG(dev,
			"PCIe V%d: the link is already power down.\n",
			dev->rev);
		goto out;
	}

	dev->link_status = EP_PCIE_LINK_DISABLED;
	dev->power_on = false;

	EP_PCIE_DBG(dev, "PCIe V%d: shut down the link.\n",
		dev->rev);

	ep_pcie_pipe_clk_deinit(dev);
	ep_pcie_clk_deinit(dev);
	ep_pcie_vreg_deinit(dev);
out:
	mutex_unlock(&dev->setup_mtx);
	return rc;
}

int ep_pcie_core_mask_irq_event(enum ep_pcie_irq_event event,
				bool enable)
{
	int rc = 0;
	struct ep_pcie_dev_t *dev = &ep_pcie_dev;
	unsigned long irqsave_flags;
	u32 mask = 0;

	EP_PCIE_DUMP(dev,
		"PCIe V%d: Client askes to %s IRQ event 0x%x.\n",
		dev->rev,
		enable ? "enable" : "disable",
		event);

	spin_lock_irqsave(&dev->ext_lock, irqsave_flags);

	if (dev->aggregated_irq) {
		mask = readl_relaxed(dev->dm_core + PCIE20_PARF_INT_ALL_MASK);
		EP_PCIE_DUMP(dev,
			"PCIe V%d: current PCIE20_PARF_INT_ALL_MASK:0x%x\n",
			dev->rev, mask);
		if (enable)
			ep_pcie_write_mask(dev->parf + PCIE20_PARF_INT_ALL_MASK,
						0, BIT(event));
		else
			ep_pcie_write_mask(dev->parf + PCIE20_PARF_INT_ALL_MASK,
						BIT(event), 0);
		EP_PCIE_DUMP(dev,
			"PCIe V%d: new PCIE20_PARF_INT_ALL_MASK:0x%x\n",
			dev->rev,
			readl_relaxed(dev->dm_core + PCIE20_PARF_INT_ALL_MASK));
	} else {
		EP_PCIE_ERR(dev,
			"PCIe V%d: Client askes to %s IRQ event 0x%x when aggregated IRQ is not supported.\n",
			dev->rev,
			enable ? "enable" : "disable",
			event);
		rc = EP_PCIE_ERROR;
	}

	spin_unlock_irqrestore(&dev->ext_lock, irqsave_flags);
	return rc;
}

static irqreturn_t ep_pcie_handle_bme_irq(int irq, void *data)
{
	struct ep_pcie_dev_t *dev = data;
	unsigned long irqsave_flags;

	spin_lock_irqsave(&dev->isr_lock, irqsave_flags);

	dev->bme_counter++;
	EP_PCIE_DBG(dev,
		"PCIe V%d: No. %ld BME IRQ.\n", dev->rev, dev->bme_counter);

	if (readl_relaxed(dev->dm_core + PCIE20_COMMAND_STATUS) & BIT(2)) {
		/* BME has been enabled */
		if (!dev->enumerated) {
			EP_PCIE_DBG(dev,
				"PCIe V%d:BME is set. Enumeration is complete\n",
				dev->rev);
			schedule_work(&dev->handle_bme_work);
		} else {
			EP_PCIE_DBG(dev,
				"PCIe V%d:BME is set again after the enumeration has completed\n",
				dev->rev);
		}
	} else {
		EP_PCIE_DBG(dev,
				"PCIe V%d:BME is still disabled\n", dev->rev);
	}

	spin_unlock_irqrestore(&dev->isr_lock, irqsave_flags);
	return IRQ_HANDLED;
}

static irqreturn_t ep_pcie_handle_linkdown_irq(int irq, void *data)
{
	struct ep_pcie_dev_t *dev = data;
	unsigned long irqsave_flags;

	spin_lock_irqsave(&dev->isr_lock, irqsave_flags);

	dev->linkdown_counter++;
	EP_PCIE_DBG(dev,
		"PCIe V%d: No. %ld linkdown IRQ.\n",
		dev->rev, dev->linkdown_counter);

	if (!dev->enumerated || dev->link_status == EP_PCIE_LINK_DISABLED) {
		EP_PCIE_DBG(dev,
			"PCIe V%d:Linkdown IRQ happened when the link is disabled.\n",
			dev->rev);
	} else if (dev->suspending) {
		EP_PCIE_DBG(dev,
			"PCIe V%d:Linkdown IRQ happened when the link is suspending.\n",
			dev->rev);
	} else {
		dev->link_status = EP_PCIE_LINK_DISABLED;
		EP_PCIE_ERR(dev, "PCIe V%d:PCIe link is down for %ld times\n",
			dev->rev, dev->linkdown_counter);
		ep_pcie_reg_dump(dev, BIT(EP_PCIE_RES_PHY) |
				BIT(EP_PCIE_RES_PARF), true);
		ep_pcie_notify_event(dev, EP_PCIE_EVENT_LINKDOWN);
	}

	spin_unlock_irqrestore(&dev->isr_lock, irqsave_flags);

	return IRQ_HANDLED;
}

static irqreturn_t ep_pcie_handle_linkup_irq(int irq, void *data)
{
	struct ep_pcie_dev_t *dev = data;
	unsigned long irqsave_flags;

	spin_lock_irqsave(&dev->isr_lock, irqsave_flags);

	dev->linkup_counter++;
	EP_PCIE_DBG(dev,
		"PCIe V%d: No. %ld linkup IRQ.\n",
		dev->rev, dev->linkup_counter);

	dev->link_status = EP_PCIE_LINK_UP;

	spin_unlock_irqrestore(&dev->isr_lock, irqsave_flags);

	return IRQ_HANDLED;
}

static irqreturn_t ep_pcie_handle_pm_turnoff_irq(int irq, void *data)
{
	struct ep_pcie_dev_t *dev = data;
	unsigned long irqsave_flags;

	spin_lock_irqsave(&dev->isr_lock, irqsave_flags);

	dev->pm_to_counter++;
	EP_PCIE_DBG2(dev,
		"PCIe V%d: No. %ld PM_TURNOFF is received.\n",
		dev->rev, dev->pm_to_counter);
	EP_PCIE_DBG2(dev, "PCIe V%d: Put the link into L23.\n",	dev->rev);
	ep_pcie_write_mask(dev->parf + PCIE20_PARF_PM_CTRL, 0, BIT(2));

	spin_unlock_irqrestore(&dev->isr_lock, irqsave_flags);

	return IRQ_HANDLED;
}

static irqreturn_t ep_pcie_handle_dstate_change_irq(int irq, void *data)
{
	struct ep_pcie_dev_t *dev = data;
	unsigned long irqsave_flags;
	u32 dstate;

	spin_lock_irqsave(&dev->isr_lock, irqsave_flags);

	dstate = readl_relaxed(dev->dm_core +
			PCIE20_CON_STATUS) & 0x3;

	if (dev->dump_conf)
		ep_pcie_reg_dump(dev, BIT(EP_PCIE_RES_DM_CORE), false);

	if (dstate == 3) {
		dev->l23_ready = true;
		dev->d3_counter++;
		EP_PCIE_DBG(dev,
			"PCIe V%d: No. %ld change to D3 state.\n",
			dev->rev, dev->d3_counter);
		ep_pcie_write_mask(dev->parf + PCIE20_PARF_PM_CTRL, 0, BIT(1));
		ep_pcie_notify_event(dev, EP_PCIE_EVENT_PM_D3_HOT);
	} else if (dstate == 0) {
		dev->l23_ready = false;
		dev->d0_counter++;
		EP_PCIE_DBG(dev,
			"PCIe V%d: No. %ld change to D0 state.\n",
			dev->rev, dev->d0_counter);
		ep_pcie_notify_event(dev, EP_PCIE_EVENT_PM_D0);
	} else {
		EP_PCIE_ERR(dev,
			"PCIe V%d:invalid D state change to 0x%x.\n",
			dev->rev, dstate);
	}

	spin_unlock_irqrestore(&dev->isr_lock, irqsave_flags);

	return IRQ_HANDLED;
}

static int ep_pcie_enumeration(struct ep_pcie_dev_t *dev)
{
	int ret = 0;

	if (!dev) {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: the input handler is NULL.\n",
			ep_pcie_dev.rev);
		return EP_PCIE_ERROR;
	}

	EP_PCIE_DBG(dev,
		"PCIe V%d: start PCIe link enumeration per host side.\n",
		dev->rev);

	ret = ep_pcie_core_enable_endpoint(EP_PCIE_OPT_ALL);

	if (ret) {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: PCIe link enumeration failed.\n",
			ep_pcie_dev.rev);
	} else {
		if (dev->link_status == EP_PCIE_LINK_ENABLED) {
			EP_PCIE_INFO(&ep_pcie_dev,
				"PCIe V%d: PCIe link enumeration is successful with host side.\n",
				ep_pcie_dev.rev);
		} else if (dev->link_status == EP_PCIE_LINK_UP) {
			EP_PCIE_INFO(&ep_pcie_dev,
				"PCIe V%d: PCIe link training is successful with host side. Waiting for enumeration to complete.\n",
				ep_pcie_dev.rev);
		} else {
			EP_PCIE_ERR(&ep_pcie_dev,
				"PCIe V%d: PCIe link is in the unexpected status: %d\n",
				ep_pcie_dev.rev, dev->link_status);
		}
	}

	return ret;
}

static void handle_perst_func(struct work_struct *work)
{
	struct ep_pcie_dev_t *dev = container_of(work, struct ep_pcie_dev_t,
					handle_perst_work);

	ep_pcie_enumeration(dev);
}

static void handle_bme_func(struct work_struct *work)
{
	struct ep_pcie_dev_t *dev = container_of(work,
			struct ep_pcie_dev_t, handle_bme_work);

	ep_pcie_enumeration_complete(dev);
}

static irqreturn_t ep_pcie_handle_perst_irq(int irq, void *data)
{
	struct ep_pcie_dev_t *dev = data;
	unsigned long irqsave_flags;
	u32 perst;

	spin_lock_irqsave(&dev->isr_lock, irqsave_flags);

	perst = gpio_get_value(dev->gpio[EP_PCIE_GPIO_PERST].num);

	if (!dev->enumerated) {
		EP_PCIE_DBG(dev,
			"PCIe V%d: PCIe is not enumerated yet; PERST is %sasserted.\n",
			dev->rev, perst ? "de" : "");
		if ((!dev->perst_enum) || !perst)
			goto out;
		/* start work for link enumeration with the host side */
		schedule_work(&dev->handle_perst_work);

		goto out;
	}

	if (perst) {
		dev->perst_deast = true;
		dev->perst_deast_counter++;
		EP_PCIE_DBG(dev,
			"PCIe V%d: No. %ld PERST deassertion.\n",
			dev->rev, dev->perst_deast_counter);
		ep_pcie_notify_event(dev, EP_PCIE_EVENT_PM_RST_DEAST);
	} else {
		dev->perst_deast = false;
		dev->perst_ast_counter++;
		EP_PCIE_DBG(dev,
			"PCIe V%d: No. %ld PERST assertion.\n",
			dev->rev, dev->perst_ast_counter);
		ep_pcie_notify_event(dev, EP_PCIE_EVENT_PM_D3_COLD);
	}

out:
	spin_unlock_irqrestore(&dev->isr_lock, irqsave_flags);

	return IRQ_HANDLED;
}

static irqreturn_t ep_pcie_handle_global_irq(int irq, void *data)
{
	struct ep_pcie_dev_t *dev = data;
	int i;
	u32 status = readl_relaxed(dev->parf + PCIE20_PARF_INT_ALL_STATUS);
	u32 mask = readl_relaxed(dev->parf + PCIE20_PARF_INT_ALL_MASK);

	ep_pcie_write_mask(dev->parf + PCIE20_PARF_INT_ALL_CLEAR, 0, status);

	dev->global_irq_counter++;
	EP_PCIE_DUMP(dev,
		"PCIe V%d: No. %ld Global IRQ %d received; status:0x%x; mask:0x%x.\n",
		dev->rev, dev->global_irq_counter, irq, status, mask);
	status &= mask;

	for (i = 1; i <= EP_PCIE_INT_EVT_MAX; i++) {
		if (status & BIT(i)) {
			switch (i) {
			case EP_PCIE_INT_EVT_LINK_DOWN:
				EP_PCIE_DUMP(dev,
					"PCIe V%d: handle linkdown event.\n",
					dev->rev);
				ep_pcie_handle_linkdown_irq(irq, data);
				break;
			case EP_PCIE_INT_EVT_BME:
				EP_PCIE_DUMP(dev,
					"PCIe V%d: handle BME event.\n",
					dev->rev);
				ep_pcie_handle_bme_irq(irq, data);
				break;
			case EP_PCIE_INT_EVT_PM_TURNOFF:
				EP_PCIE_DUMP(dev,
					"PCIe V%d: handle PM Turn-off event.\n",
					dev->rev);
				ep_pcie_handle_pm_turnoff_irq(irq, data);
				break;
			case EP_PCIE_INT_EVT_MHI_A7:
				EP_PCIE_DUMP(dev,
					"PCIe V%d: handle MHI A7 event.\n",
					dev->rev);
				ep_pcie_notify_event(dev, EP_PCIE_EVENT_MHI_A7);
				break;
			case EP_PCIE_INT_EVT_DSTATE_CHANGE:
				EP_PCIE_DUMP(dev,
					"PCIe V%d: handle D state chagge event.\n",
					dev->rev);
				ep_pcie_handle_dstate_change_irq(irq, data);
				break;
			case EP_PCIE_INT_EVT_LINK_UP:
				EP_PCIE_DUMP(dev,
					"PCIe V%d: handle linkup event.\n",
					dev->rev);
				ep_pcie_handle_linkup_irq(irq, data);
				break;
			default:
				EP_PCIE_ERR(dev,
					"PCIe V%d: Unexpected event %d is caught!\n",
					dev->rev, i);
			}
		}
	}

	return IRQ_HANDLED;
}

int32_t ep_pcie_irq_init(struct ep_pcie_dev_t *dev)
{
	int ret;
	struct device *pdev = &dev->pdev->dev;
	u32 perst_irq;

	EP_PCIE_DBG(dev, "PCIe V%d\n", dev->rev);

	/* Initialize all works to be performed before registering for IRQs*/
	INIT_WORK(&dev->handle_perst_work, handle_perst_func);
	INIT_WORK(&dev->handle_bme_work, handle_bme_func);

	if (dev->aggregated_irq) {
		ret = devm_request_irq(pdev,
			dev->irq[EP_PCIE_INT_GLOBAL].num,
			ep_pcie_handle_global_irq,
			IRQF_TRIGGER_HIGH, dev->irq[EP_PCIE_INT_GLOBAL].name,
			dev);
		if (ret) {
			EP_PCIE_ERR(dev,
				"PCIe V%d: Unable to request global interrupt %d\n",
				dev->rev, dev->irq[EP_PCIE_INT_GLOBAL].num);
			return ret;
		}

		ret = enable_irq_wake(dev->irq[EP_PCIE_INT_GLOBAL].num);
		if (ret) {
			EP_PCIE_ERR(dev,
				"PCIe V%d: Unable to enable wake for Global interrupt\n",
				dev->rev);
			return ret;
		}

		EP_PCIE_DBG(dev,
			"PCIe V%d: request global interrupt %d\n",
			dev->rev, dev->irq[EP_PCIE_INT_GLOBAL].num);
		goto perst_irq;
	}

	/* register handler for BME interrupt */
	ret = devm_request_irq(pdev,
		dev->irq[EP_PCIE_INT_BME].num,
		ep_pcie_handle_bme_irq,
		IRQF_TRIGGER_RISING, dev->irq[EP_PCIE_INT_BME].name,
		dev);
	if (ret) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: Unable to request BME interrupt %d\n",
			dev->rev, dev->irq[EP_PCIE_INT_BME].num);
		return ret;
	}

	ret = enable_irq_wake(dev->irq[EP_PCIE_INT_BME].num);
	if (ret) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: Unable to enable wake for BME interrupt\n",
			dev->rev);
		return ret;
	}

	/* register handler for linkdown interrupt */
	ret = devm_request_irq(pdev,
		dev->irq[EP_PCIE_INT_LINK_DOWN].num,
		ep_pcie_handle_linkdown_irq,
		IRQF_TRIGGER_RISING, dev->irq[EP_PCIE_INT_LINK_DOWN].name,
		dev);
	if (ret) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: Unable to request linkdown interrupt %d\n",
			dev->rev, dev->irq[EP_PCIE_INT_LINK_DOWN].num);
		return ret;
	}

	/* register handler for linkup interrupt */
	ret = devm_request_irq(pdev,
		dev->irq[EP_PCIE_INT_LINK_UP].num, ep_pcie_handle_linkup_irq,
		IRQF_TRIGGER_RISING, dev->irq[EP_PCIE_INT_LINK_UP].name,
		dev);
	if (ret) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: Unable to request linkup interrupt %d\n",
			dev->rev, dev->irq[EP_PCIE_INT_LINK_UP].num);
		return ret;
	}

	/* register handler for PM_TURNOFF interrupt */
	ret = devm_request_irq(pdev,
		dev->irq[EP_PCIE_INT_PM_TURNOFF].num,
		ep_pcie_handle_pm_turnoff_irq,
		IRQF_TRIGGER_RISING, dev->irq[EP_PCIE_INT_PM_TURNOFF].name,
		dev);
	if (ret) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: Unable to request PM_TURNOFF interrupt %d\n",
			dev->rev, dev->irq[EP_PCIE_INT_PM_TURNOFF].num);
		return ret;
	}

	/* register handler for D state change interrupt */
	ret = devm_request_irq(pdev,
		dev->irq[EP_PCIE_INT_DSTATE_CHANGE].num,
		ep_pcie_handle_dstate_change_irq,
		IRQF_TRIGGER_RISING, dev->irq[EP_PCIE_INT_DSTATE_CHANGE].name,
		dev);
	if (ret) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: Unable to request D state change interrupt %d\n",
			dev->rev, dev->irq[EP_PCIE_INT_DSTATE_CHANGE].num);
		return ret;
	}

perst_irq:
	/* register handler for PERST interrupt */
	perst_irq = gpio_to_irq(dev->gpio[EP_PCIE_GPIO_PERST].num);
	ret = devm_request_irq(pdev, perst_irq,
		ep_pcie_handle_perst_irq,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
		"ep_pcie_perst", dev);
	if (ret) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: Unable to request PERST interrupt %d\n",
			dev->rev, perst_irq);
		return ret;
	}

	ret = enable_irq_wake(perst_irq);
	if (ret) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: Unable to enable PERST interrupt %d\n",
			dev->rev, perst_irq);
		return ret;
	}

	return 0;
}

void ep_pcie_irq_deinit(struct ep_pcie_dev_t *dev)
{
	EP_PCIE_DBG(dev, "PCIe V%d\n", dev->rev);

	disable_irq(gpio_to_irq(dev->gpio[EP_PCIE_GPIO_PERST].num));
}

int ep_pcie_core_register_event(struct ep_pcie_register_event *reg)
{
	if (!reg) {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: Event registration is NULL\n",
			ep_pcie_dev.rev);
		return -ENODEV;
	}

	if (!reg->user) {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: User of event registration is NULL\n",
			ep_pcie_dev.rev);
		return -ENODEV;
	}

	ep_pcie_dev.event_reg = reg;
	EP_PCIE_DBG(&ep_pcie_dev,
		"PCIe V%d: Event 0x%x is registered\n",
		ep_pcie_dev.rev, reg->events);

	return 0;
}

int ep_pcie_core_deregister_event(void)
{
	if (ep_pcie_dev.event_reg) {
		EP_PCIE_DBG(&ep_pcie_dev,
			"PCIe V%d: current registered events:0x%x; events are deregistered.\n",
			ep_pcie_dev.rev, ep_pcie_dev.event_reg->events);
		ep_pcie_dev.event_reg = NULL;
	} else {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: Event registration is NULL\n",
			ep_pcie_dev.rev);
	}

	return 0;
}

enum ep_pcie_link_status ep_pcie_core_get_linkstatus(void)
{
	struct ep_pcie_dev_t *dev = &ep_pcie_dev;

	if (!dev->power_on || (dev->link_status == EP_PCIE_LINK_DISABLED)) {
		EP_PCIE_DBG(dev,
			"PCIe V%d: PCIe endpoint is not powered on.\n",
			dev->rev);
		return EP_PCIE_LINK_DISABLED;
	} else {
		u32 bme = readl_relaxed(dev->dm_core +
			PCIE20_COMMAND_STATUS) & BIT(2);
		if (bme) {
			EP_PCIE_DBG(dev,
				"PCIe V%d: PCIe link is up and BME is enabled; current SW link status:%d.\n",
				dev->rev, dev->link_status);
			dev->link_status = EP_PCIE_LINK_ENABLED;
		} else {
			EP_PCIE_DBG(dev,
				"PCIe V%d: PCIe link is up but BME is disabled; current SW link status:%d.\n",
				dev->rev, dev->link_status);
			dev->link_status = EP_PCIE_LINK_UP;
		}
		return dev->link_status;
	}
}

int ep_pcie_core_config_outbound_iatu(struct ep_pcie_iatu entries[],
				u32 num_entries)
{
	u32 data_start = 0;
	u32 data_end = 0;
	u32 data_tgt_lower = 0;
	u32 data_tgt_upper = 0;
	u32 ctrl_start = 0;
	u32 ctrl_end = 0;
	u32 ctrl_tgt_lower = 0;
	u32 ctrl_tgt_upper = 0;
	u32 upper = 0;
	bool once = true;

	if (ep_pcie_dev.active_config) {
		upper = EP_PCIE_OATU_UPPER;
		if (once) {
			once = false;
			EP_PCIE_DBG2(&ep_pcie_dev,
				"PCIe V%d: No outbound iATU config is needed since active config is enabled.\n",
				ep_pcie_dev.rev);
		}
	}

	if ((num_entries > MAX_IATU_ENTRY_NUM) || !num_entries) {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: Wrong iATU entry number %d.\n",
			ep_pcie_dev.rev, num_entries);
		return EP_PCIE_ERROR;
	}

	data_start = entries[0].start;
	data_end = entries[0].end;
	data_tgt_lower = entries[0].tgt_lower;
	data_tgt_upper = entries[0].tgt_upper;

	if (num_entries > 1) {
		ctrl_start = entries[1].start;
		ctrl_end = entries[1].end;
		ctrl_tgt_lower = entries[1].tgt_lower;
		ctrl_tgt_upper = entries[1].tgt_upper;
	}

	EP_PCIE_DBG(&ep_pcie_dev,
		"PCIe V%d: data_start:0x%x; data_end:0x%x; data_tgt_lower:0x%x; data_tgt_upper:0x%x; ctrl_start:0x%x; ctrl_end:0x%x; ctrl_tgt_lower:0x%x; ctrl_tgt_upper:0x%x.\n",
		ep_pcie_dev.rev, data_start, data_end, data_tgt_lower,
		data_tgt_upper, ctrl_start, ctrl_end, ctrl_tgt_lower,
		ctrl_tgt_upper);


	if ((ctrl_end < data_start) || (data_end < ctrl_start)) {
		EP_PCIE_DBG(&ep_pcie_dev,
			"PCIe V%d: iATU configuration case No. 1: detached.\n",
			ep_pcie_dev.rev);
		ep_pcie_config_outbound_iatu_entry(&ep_pcie_dev,
					EP_PCIE_OATU_INDEX_DATA,
					data_start, upper, data_end,
					data_tgt_lower, data_tgt_upper);
		ep_pcie_config_outbound_iatu_entry(&ep_pcie_dev,
					EP_PCIE_OATU_INDEX_CTRL,
					ctrl_start, upper, ctrl_end,
					ctrl_tgt_lower, ctrl_tgt_upper);
	} else if ((data_start <= ctrl_start) && (ctrl_end <= data_end)) {
		EP_PCIE_DBG(&ep_pcie_dev,
			"PCIe V%d: iATU configuration case No. 2: included.\n",
			ep_pcie_dev.rev);
		ep_pcie_config_outbound_iatu_entry(&ep_pcie_dev,
					EP_PCIE_OATU_INDEX_DATA,
					data_start, upper, data_end,
					data_tgt_lower, data_tgt_upper);
	} else {
		EP_PCIE_DBG(&ep_pcie_dev,
			"PCIe V%d: iATU configuration case No. 3: overlap.\n",
			ep_pcie_dev.rev);
		ep_pcie_config_outbound_iatu_entry(&ep_pcie_dev,
					EP_PCIE_OATU_INDEX_CTRL,
					ctrl_start, upper, ctrl_end,
					ctrl_tgt_lower, ctrl_tgt_upper);
		ep_pcie_config_outbound_iatu_entry(&ep_pcie_dev,
					EP_PCIE_OATU_INDEX_DATA,
					data_start, upper, data_end,
					data_tgt_lower, data_tgt_upper);
	}

	return 0;
}

int ep_pcie_core_get_msi_config(struct ep_pcie_msi_config *cfg)
{
	u32 cap, lower, upper, data, ctrl_reg;
	static u32 changes;

	if (ep_pcie_dev.link_status == EP_PCIE_LINK_DISABLED) {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: PCIe link is currently disabled.\n",
			ep_pcie_dev.rev);
		return EP_PCIE_ERROR;
	}

	cap = readl_relaxed(ep_pcie_dev.dm_core + PCIE20_MSI_CAP_ID_NEXT_CTRL);
	EP_PCIE_DBG(&ep_pcie_dev, "PCIe V%d: MSI CAP:0x%x\n",
			ep_pcie_dev.rev, cap);

	if (!(cap & BIT(16))) {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: MSI is not enabled yet.\n",
			ep_pcie_dev.rev);
		return EP_PCIE_ERROR;
	}

	lower = readl_relaxed(ep_pcie_dev.dm_core + PCIE20_MSI_LOWER);
	upper = readl_relaxed(ep_pcie_dev.dm_core + PCIE20_MSI_UPPER);
	data = readl_relaxed(ep_pcie_dev.dm_core + PCIE20_MSI_DATA);
	ctrl_reg = readl_relaxed(ep_pcie_dev.dm_core +
					PCIE20_MSI_CAP_ID_NEXT_CTRL);

	EP_PCIE_DBG(&ep_pcie_dev,
		"PCIe V%d: MSI info: lower:0x%x; upper:0x%x; data:0x%x.\n",
		ep_pcie_dev.rev, lower, upper, data);

	if (ctrl_reg & BIT(16)) {
		struct resource *msi =
				ep_pcie_dev.res[EP_PCIE_RES_MSI].resource;
		if (ep_pcie_dev.active_config)
			ep_pcie_config_outbound_iatu_entry(&ep_pcie_dev,
					EP_PCIE_OATU_INDEX_MSI,
					msi->start, EP_PCIE_OATU_UPPER,
					msi->end, lower, upper);
		else
			ep_pcie_config_outbound_iatu_entry(&ep_pcie_dev,
					EP_PCIE_OATU_INDEX_MSI,
					msi->start, 0, msi->end,
					lower, upper);

		if (ep_pcie_dev.active_config) {
			cfg->lower = lower;
			cfg->upper = upper;
		} else {
			cfg->lower = msi->start + (lower & 0xfff);
			cfg->upper = 0;
		}
		cfg->data = data;
		cfg->msg_num = (cap >> 20) & 0x7;
		if ((lower != ep_pcie_dev.msi_cfg.lower)
			|| (upper != ep_pcie_dev.msi_cfg.upper)
			|| (data != ep_pcie_dev.msi_cfg.data)
			|| (cfg->msg_num != ep_pcie_dev.msi_cfg.msg_num)) {
			changes++;
			EP_PCIE_DBG(&ep_pcie_dev,
				"PCIe V%d: MSI config has been changed by host side for %d time(s).\n",
				ep_pcie_dev.rev, changes);
			EP_PCIE_DBG(&ep_pcie_dev,
				"PCIe V%d: old MSI cfg: lower:0x%x; upper:0x%x; data:0x%x; msg_num:0x%x.\n",
				ep_pcie_dev.rev, ep_pcie_dev.msi_cfg.lower,
				ep_pcie_dev.msi_cfg.upper,
				ep_pcie_dev.msi_cfg.data,
				ep_pcie_dev.msi_cfg.msg_num);
			ep_pcie_dev.msi_cfg.lower = lower;
			ep_pcie_dev.msi_cfg.upper = upper;
			ep_pcie_dev.msi_cfg.data = data;
			ep_pcie_dev.msi_cfg.msg_num = cfg->msg_num;
		}
		return 0;
	} else {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: Wrong MSI info found when MSI is enabled: lower:0x%x; data:0x%x.\n",
			ep_pcie_dev.rev, lower, data);
		return EP_PCIE_ERROR;
	}
}

int ep_pcie_core_trigger_msi(u32 idx)
{
	u32 addr, data, ctrl_reg;
	int max_poll = MSI_EXIT_L1SS_WAIT_MAX_COUNT;

	if (ep_pcie_dev.link_status == EP_PCIE_LINK_DISABLED) {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: PCIe link is currently disabled.\n",
			ep_pcie_dev.rev);
		return EP_PCIE_ERROR;
	}

	addr = readl_relaxed(ep_pcie_dev.dm_core + PCIE20_MSI_LOWER);
	data = readl_relaxed(ep_pcie_dev.dm_core + PCIE20_MSI_DATA);
	ctrl_reg = readl_relaxed(ep_pcie_dev.dm_core +
					PCIE20_MSI_CAP_ID_NEXT_CTRL);

	if (ctrl_reg & BIT(16)) {
		ep_pcie_dev.msi_counter++;
		EP_PCIE_DUMP(&ep_pcie_dev,
			"PCIe V%d: No. %ld MSI fired for IRQ %d; index from client:%d; active-config is %s enabled.\n",
			ep_pcie_dev.rev, ep_pcie_dev.msi_counter,
			data + idx, idx,
			ep_pcie_dev.active_config ? "" : "not");

		if (ep_pcie_dev.active_config) {
			u32 status;

			if (ep_pcie_dev.msi_counter % 2) {
				EP_PCIE_DBG2(&ep_pcie_dev,
					"PCIe V%d: try to trigger MSI by PARF_MSI_GEN.\n",
					ep_pcie_dev.rev);
				ep_pcie_write_reg(ep_pcie_dev.parf,
					PCIE20_PARF_MSI_GEN, idx);
				status = readl_relaxed(ep_pcie_dev.parf +
					PCIE20_PARF_LTR_MSI_EXIT_L1SS);
				while ((status & BIT(1)) && (max_poll-- > 0)) {
					udelay(MSI_EXIT_L1SS_WAIT);
					status = readl_relaxed(ep_pcie_dev.parf
						+
						PCIE20_PARF_LTR_MSI_EXIT_L1SS);
				}
				if (max_poll == 0)
					EP_PCIE_DBG2(&ep_pcie_dev,
						"PCIe V%d: MSI_EXIT_L1SS is not cleared yet.\n",
						ep_pcie_dev.rev);
				else
					EP_PCIE_DBG2(&ep_pcie_dev,
						"PCIe V%d: MSI_EXIT_L1SS has been cleared.\n",
						ep_pcie_dev.rev);
			} else {
				EP_PCIE_DBG2(&ep_pcie_dev,
						"PCIe V%d: try to trigger MSI by direct address write as well.\n",
						ep_pcie_dev.rev);
				ep_pcie_write_reg(ep_pcie_dev.msi, addr & 0xfff,
							data + idx);
			}
		} else {
			ep_pcie_write_reg(ep_pcie_dev.msi, addr & 0xfff, data
						+ idx);
		}
		return 0;
	} else {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: MSI is not enabled yet. MSI addr:0x%x; data:0x%x; index from client:%d.\n",
			ep_pcie_dev.rev, addr, data, idx);
		return EP_PCIE_ERROR;
	}
}

int ep_pcie_core_wakeup_host(void)
{
	struct ep_pcie_dev_t *dev = &ep_pcie_dev;

	if (dev->perst_deast && !dev->l23_ready) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: request to assert WAKE# when PERST is de-asserted and D3hot is not received.\n",
			dev->rev);
		return EP_PCIE_ERROR;
	} else {
		dev->wake_counter++;
		EP_PCIE_DBG(dev,
			"PCIe V%d: No. %ld to assert PCIe WAKE#; perst is %s de-asserted; D3hot is %s received.\n",
			dev->rev, dev->wake_counter,
			dev->perst_deast ? "" : "not",
			dev->l23_ready ? "" : "not");
		gpio_set_value(dev->gpio[EP_PCIE_GPIO_WAKE].num,
				1 - dev->gpio[EP_PCIE_GPIO_WAKE].on);
		gpio_set_value(dev->gpio[EP_PCIE_GPIO_WAKE].num,
				dev->gpio[EP_PCIE_GPIO_WAKE].on);
		return 0;
	}
}

int ep_pcie_core_config_db_routing(struct ep_pcie_db_config chdb_cfg,
				struct ep_pcie_db_config erdb_cfg)
{
	u32 dbs = (erdb_cfg.end << 24) | (erdb_cfg.base << 16) |
			(chdb_cfg.end << 8) | chdb_cfg.base;

	ep_pcie_write_reg(ep_pcie_dev.parf, PCIE20_PARF_MHI_IPA_DBS, dbs);
	ep_pcie_write_reg(ep_pcie_dev.parf,
			PCIE20_PARF_MHI_IPA_CDB_TARGET_LOWER,
			chdb_cfg.tgt_addr);
	ep_pcie_write_reg(ep_pcie_dev.parf,
			PCIE20_PARF_MHI_IPA_EDB_TARGET_LOWER,
			erdb_cfg.tgt_addr);

	EP_PCIE_DBG(&ep_pcie_dev,
		"PCIe V%d: DB routing info: chdb_cfg.base:0x%x; chdb_cfg.end:0x%x; erdb_cfg.base:0x%x; erdb_cfg.end:0x%x; chdb_cfg.tgt_addr:0x%x; erdb_cfg.tgt_addr:0x%x.\n",
		ep_pcie_dev.rev, chdb_cfg.base, chdb_cfg.end, erdb_cfg.base,
		erdb_cfg.end, chdb_cfg.tgt_addr, erdb_cfg.tgt_addr);

	return 0;
}

struct ep_pcie_hw hw_drv = {
	.register_event	= ep_pcie_core_register_event,
	.deregister_event = ep_pcie_core_deregister_event,
	.get_linkstatus = ep_pcie_core_get_linkstatus,
	.config_outbound_iatu = ep_pcie_core_config_outbound_iatu,
	.get_msi_config = ep_pcie_core_get_msi_config,
	.trigger_msi = ep_pcie_core_trigger_msi,
	.wakeup_host = ep_pcie_core_wakeup_host,
	.config_db_routing = ep_pcie_core_config_db_routing,
	.enable_endpoint = ep_pcie_core_enable_endpoint,
	.disable_endpoint = ep_pcie_core_disable_endpoint,
	.mask_irq_event = ep_pcie_core_mask_irq_event,
};

static int ep_pcie_probe(struct platform_device *pdev)
{
	int ret;

	pr_debug("%s\n", __func__);

	ep_pcie_dev.link_speed = 1;
	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,pcie-link-speed",
				&ep_pcie_dev.link_speed);
	if (ret)
		EP_PCIE_DBG(&ep_pcie_dev,
			"PCIe V%d: pcie-link-speed does not exist.\n",
			ep_pcie_dev.rev);
	else
		EP_PCIE_DBG(&ep_pcie_dev, "PCIe V%d: pcie-link-speed:%d.\n",
			ep_pcie_dev.rev, ep_pcie_dev.link_speed);

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,dbi-base-reg",
				&ep_pcie_dev.dbi_base_reg);
	if (ret)
		EP_PCIE_DBG(&ep_pcie_dev,
			"PCIe V%d: dbi-base-reg does not exist.\n",
			ep_pcie_dev.rev);
	else
		EP_PCIE_DBG(&ep_pcie_dev, "PCIe V%d: dbi-base-reg:0x%x.\n",
			ep_pcie_dev.rev, ep_pcie_dev.dbi_base_reg);

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,slv-space-reg",
				&ep_pcie_dev.slv_space_reg);
	if (ret)
		EP_PCIE_DBG(&ep_pcie_dev,
			"PCIe V%d: slv-space-reg does not exist.\n",
			ep_pcie_dev.rev);
	else
		EP_PCIE_DBG(&ep_pcie_dev, "PCIe V%d: slv-space-reg:0x%x.\n",
			ep_pcie_dev.rev, ep_pcie_dev.slv_space_reg);

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,phy-status-reg",
				&ep_pcie_dev.phy_status_reg);
	if (ret)
		EP_PCIE_DBG(&ep_pcie_dev,
			"PCIe V%d: phy-status-reg does not exist.\n",
			ep_pcie_dev.rev);
	else
		EP_PCIE_DBG(&ep_pcie_dev, "PCIe V%d: phy-status-reg:0x%x.\n",
			ep_pcie_dev.rev, ep_pcie_dev.phy_status_reg);

	ep_pcie_dev.phy_rev = 1;
	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,pcie-phy-ver",
				&ep_pcie_dev.phy_rev);
	if (ret)
		EP_PCIE_DBG(&ep_pcie_dev,
			"PCIe V%d: pcie-phy-ver does not exist.\n",
			ep_pcie_dev.rev);
	else
		EP_PCIE_DBG(&ep_pcie_dev, "PCIe V%d: pcie-phy-ver:%d.\n",
			ep_pcie_dev.rev, ep_pcie_dev.phy_rev);

	ep_pcie_dev.active_config = of_property_read_bool((&pdev->dev)->of_node,
				"qcom,pcie-active-config");
	EP_PCIE_DBG(&ep_pcie_dev,
		"PCIe V%d: active config is %s enabled.\n",
		ep_pcie_dev.rev, ep_pcie_dev.active_config ? "" : "not");

	ep_pcie_dev.aggregated_irq =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,pcie-aggregated-irq");
	EP_PCIE_DBG(&ep_pcie_dev,
		"PCIe V%d: aggregated IRQ is %s enabled.\n",
		ep_pcie_dev.rev, ep_pcie_dev.aggregated_irq ? "" : "not");

	ep_pcie_dev.perst_enum = of_property_read_bool((&pdev->dev)->of_node,
				"qcom,pcie-perst-enum");
	EP_PCIE_DBG(&ep_pcie_dev,
		"PCIe V%d: enum by PERST is %s enabled.\n",
		ep_pcie_dev.rev, ep_pcie_dev.perst_enum ? "" : "not");

	ep_pcie_dev.rev = 1703011;
	ep_pcie_dev.pdev = pdev;
	memcpy(ep_pcie_dev.vreg, ep_pcie_vreg_info,
				sizeof(ep_pcie_vreg_info));
	memcpy(ep_pcie_dev.gpio, ep_pcie_gpio_info,
				sizeof(ep_pcie_gpio_info));
	memcpy(ep_pcie_dev.clk, ep_pcie_clk_info,
				sizeof(ep_pcie_clk_info));
	memcpy(ep_pcie_dev.pipeclk, ep_pcie_pipe_clk_info,
				sizeof(ep_pcie_pipe_clk_info));
	memcpy(ep_pcie_dev.res, ep_pcie_res_info,
				sizeof(ep_pcie_res_info));
	memcpy(ep_pcie_dev.irq, ep_pcie_irq_info,
				sizeof(ep_pcie_irq_info));

	ret = ep_pcie_get_resources(&ep_pcie_dev,
				ep_pcie_dev.pdev);
	if (ret) {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: failed to get resources.\n",
			ep_pcie_dev.rev);
		goto res_failure;
	}

	ret = ep_pcie_gpio_init(&ep_pcie_dev);
	if (ret) {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: failed to init GPIO.\n",
			ep_pcie_dev.rev);
		ep_pcie_release_resources(&ep_pcie_dev);
		goto gpio_failure;
	}

	ret = ep_pcie_irq_init(&ep_pcie_dev);
	if (ret) {
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: failed to init IRQ.\n",
			ep_pcie_dev.rev);
		ep_pcie_release_resources(&ep_pcie_dev);
		ep_pcie_gpio_deinit(&ep_pcie_dev);
		goto irq_failure;
	}

	if (ep_pcie_dev.perst_enum &&
		!gpio_get_value(ep_pcie_dev.gpio[EP_PCIE_GPIO_PERST].num)) {
		EP_PCIE_DBG2(&ep_pcie_dev,
			"PCIe V%d: %s probe is done; link will be trained when PERST is deasserted.\n",
		ep_pcie_dev.rev, dev_name(&(pdev->dev)));
		return 0;
	}

	EP_PCIE_DBG(&ep_pcie_dev,
		"PCIe V%d: %s got resources successfully; start turning on the link.\n",
		ep_pcie_dev.rev, dev_name(&(pdev->dev)));

	ret = ep_pcie_enumeration(&ep_pcie_dev);

	if (!ret || ep_pcie_debug_keep_resource)
		return 0;

	ep_pcie_irq_deinit(&ep_pcie_dev);
irq_failure:
	ep_pcie_gpio_deinit(&ep_pcie_dev);
gpio_failure:
	ep_pcie_release_resources(&ep_pcie_dev);
res_failure:
	EP_PCIE_ERR(&ep_pcie_dev, "PCIe V%d: Driver probe failed:%d\n",
		ep_pcie_dev.rev, ret);

	return ret;
}

static int __exit ep_pcie_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	ep_pcie_irq_deinit(&ep_pcie_dev);
	ep_pcie_vreg_deinit(&ep_pcie_dev);
	ep_pcie_pipe_clk_deinit(&ep_pcie_dev);
	ep_pcie_clk_deinit(&ep_pcie_dev);
	ep_pcie_gpio_deinit(&ep_pcie_dev);
	ep_pcie_release_resources(&ep_pcie_dev);
	ep_pcie_deregister_drv(&hw_drv);

	return 0;
}

static struct of_device_id ep_pcie_match[] = {
	{	.compatible = "qcom,pcie-ep",
	},
	{}
};

static struct platform_driver ep_pcie_driver = {
	.probe	= ep_pcie_probe,
	.remove	= ep_pcie_remove,
	.driver	= {
		.name		= "pcie-ep",
		.owner		= THIS_MODULE,
		.of_match_table	= ep_pcie_match,
	},
};

static int __init ep_pcie_init(void)
{
	int ret;
	char logname[MAX_NAME_LEN];

	pr_debug("%s\n", __func__);

	snprintf(logname, MAX_NAME_LEN, "ep-pcie-long");
	ep_pcie_dev.ipc_log_sel =
		ipc_log_context_create(EP_PCIE_LOG_PAGES, logname, 0);
	if (ep_pcie_dev.ipc_log_sel == NULL)
		pr_err("%s: unable to create IPC selected log for %s\n",
			__func__, logname);
	else
		EP_PCIE_DBG(&ep_pcie_dev,
			"PCIe V%d: IPC selected logging is enable for %s\n",
			ep_pcie_dev.rev, logname);

	snprintf(logname, MAX_NAME_LEN, "ep-pcie-short");
	ep_pcie_dev.ipc_log_ful =
		ipc_log_context_create(EP_PCIE_LOG_PAGES * 2, logname, 0);
	if (ep_pcie_dev.ipc_log_ful == NULL)
		pr_err("%s: unable to create IPC detailed log for %s\n",
			__func__, logname);
	else
		EP_PCIE_DBG(&ep_pcie_dev,
			"PCIe V%d: IPC detailed logging is enable for %s\n",
			ep_pcie_dev.rev, logname);

	snprintf(logname, MAX_NAME_LEN, "ep-pcie-dump");
	ep_pcie_dev.ipc_log_dump =
		ipc_log_context_create(EP_PCIE_LOG_PAGES, logname, 0);
	if (ep_pcie_dev.ipc_log_dump == NULL)
		pr_err("%s: unable to create IPC dump log for %s\n",
			__func__, logname);
	else
		EP_PCIE_DBG(&ep_pcie_dev,
			"PCIe V%d: IPC dump logging is enable for %s\n",
			ep_pcie_dev.rev, logname);

	mutex_init(&ep_pcie_dev.setup_mtx);
	mutex_init(&ep_pcie_dev.ext_mtx);
	spin_lock_init(&ep_pcie_dev.ext_lock);
	spin_lock_init(&ep_pcie_dev.isr_lock);

	ep_pcie_debugfs_init(&ep_pcie_dev);

	ret = platform_driver_register(&ep_pcie_driver);

	if (ret)
		EP_PCIE_ERR(&ep_pcie_dev,
			"PCIe V%d: failed register platform driver:%d\n",
			ep_pcie_dev.rev, ret);
	else
		EP_PCIE_DBG(&ep_pcie_dev,
			"PCIe V%d: platform driver is registered.\n",
			ep_pcie_dev.rev);

	return ret;
}

static void __exit ep_pcie_exit(void)
{
	pr_debug("%s\n", __func__);

	ipc_log_context_destroy(ep_pcie_dev.ipc_log_sel);
	ipc_log_context_destroy(ep_pcie_dev.ipc_log_ful);
	ipc_log_context_destroy(ep_pcie_dev.ipc_log_dump);

	ep_pcie_debugfs_exit();

	platform_driver_unregister(&ep_pcie_driver);
}

module_init(ep_pcie_init);
module_exit(ep_pcie_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM PCIe Endpoint Driver");
