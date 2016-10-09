/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
 * Debugging enhancement in MSM PCIe endpoint driver.
 */

#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include "ep_pcie_com.h"
#include "ep_pcie_phy.h"

static struct dentry *dent_ep_pcie;
static struct dentry *dfile_case;
static struct ep_pcie_dev_t *dev;

static void ep_ep_pcie_phy_dump_pcs_debug_bus(struct ep_pcie_dev_t *dev,
					u32 cntrl4, u32 cntrl5,
					u32 cntrl6, u32 cntrl7)
{
	ep_pcie_write_reg(dev->phy, PCIE_PHY_TEST_CONTROL4, cntrl4);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_TEST_CONTROL5, cntrl5);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_TEST_CONTROL6, cntrl6);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_TEST_CONTROL7, cntrl7);

	if (!cntrl4 && !cntrl5 && !cntrl6 && !cntrl7) {
		EP_PCIE_DUMP(dev,
			"PCIe V%d: zero out test control registers.\n\n",
			dev->rev);
		return;
	}

	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_TEST_CONTROL4: 0x%x\n", dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_TEST_CONTROL4));
	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_TEST_CONTROL5: 0x%x\n", dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_TEST_CONTROL5));
	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_TEST_CONTROL6: 0x%x\n", dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_TEST_CONTROL6));
	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_TEST_CONTROL7: 0x%x\n", dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_TEST_CONTROL7));

	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_DEBUG_BUS_0_STATUS: 0x%x\n", dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_DEBUG_BUS_0_STATUS));
	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_DEBUG_BUS_1_STATUS: 0x%x\n", dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_DEBUG_BUS_1_STATUS));
	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_DEBUG_BUS_2_STATUS: 0x%x\n", dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_DEBUG_BUS_2_STATUS));
	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_DEBUG_BUS_3_STATUS: 0x%x\n\n", dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_DEBUG_BUS_3_STATUS));
}

static void ep_ep_pcie_phy_dump_pcs_misc_debug_bus(struct ep_pcie_dev_t *dev,
					u32 b0, u32 b1,	u32 b2, u32 b3)
{
	ep_pcie_write_reg(dev->phy, PCIE_PHY_MISC_DEBUG_BUS_BYTE0_INDEX, b0);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_MISC_DEBUG_BUS_BYTE1_INDEX, b1);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_MISC_DEBUG_BUS_BYTE2_INDEX, b2);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_MISC_DEBUG_BUS_BYTE3_INDEX, b3);

	if (!b0 && !b1 && !b2 && !b3) {
		EP_PCIE_DUMP(dev,
			"PCIe V%d: zero out misc debug bus byte index registers.\n\n",
			dev->rev);
		return;
	}

	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_MISC_DEBUG_BUS_BYTE0_INDEX: 0x%x\n",
		dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_MISC_DEBUG_BUS_BYTE0_INDEX));
	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_MISC_DEBUG_BUS_BYTE1_INDEX: 0x%x\n",
		dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_MISC_DEBUG_BUS_BYTE1_INDEX));
	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_MISC_DEBUG_BUS_BYTE2_INDEX: 0x%x\n",
		dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_MISC_DEBUG_BUS_BYTE2_INDEX));
	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_MISC_DEBUG_BUS_BYTE3_INDEX: 0x%x\n",
		dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_MISC_DEBUG_BUS_BYTE3_INDEX));

	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_MISC_DEBUG_BUS_0_STATUS: 0x%x\n", dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_MISC_DEBUG_BUS_0_STATUS));
	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_MISC_DEBUG_BUS_1_STATUS: 0x%x\n", dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_MISC_DEBUG_BUS_1_STATUS));
	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_MISC_DEBUG_BUS_2_STATUS: 0x%x\n", dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_MISC_DEBUG_BUS_2_STATUS));
	EP_PCIE_DUMP(dev,
		"PCIe V%d: PCIE_PHY_MISC_DEBUG_BUS_3_STATUS: 0x%x\n\n",
		dev->rev,
		readl_relaxed(dev->phy + PCIE_PHY_MISC_DEBUG_BUS_3_STATUS));
}

static void ep_pcie_phy_dump(struct ep_pcie_dev_t *dev)
{
	int i;
	u32 write_val;

	EP_PCIE_DUMP(dev, "PCIe V%d: Beginning of PHY debug dump.\n\n",
			dev->rev);

	EP_PCIE_DUMP(dev, "PCIe V%d: PCS Debug Signals.\n\n", dev->rev);

	ep_ep_pcie_phy_dump_pcs_debug_bus(dev, 0x01, 0x02, 0x03, 0x0A);
	ep_ep_pcie_phy_dump_pcs_debug_bus(dev, 0x0E, 0x0F, 0x12, 0x13);
	ep_ep_pcie_phy_dump_pcs_debug_bus(dev, 0x18, 0x19, 0x1A, 0x1B);
	ep_ep_pcie_phy_dump_pcs_debug_bus(dev, 0x1C, 0x1D, 0x1E, 0x1F);
	ep_ep_pcie_phy_dump_pcs_debug_bus(dev, 0x20, 0x21, 0x22, 0x23);
	ep_ep_pcie_phy_dump_pcs_debug_bus(dev, 0, 0, 0, 0);

	EP_PCIE_DUMP(dev, "PCIe V%d: PCS Misc Debug Signals.\n\n", dev->rev);

	ep_ep_pcie_phy_dump_pcs_misc_debug_bus(dev, 0x1, 0x2, 0x3, 0x4);
	ep_ep_pcie_phy_dump_pcs_misc_debug_bus(dev, 0x5, 0x6, 0x7, 0x8);
	ep_ep_pcie_phy_dump_pcs_misc_debug_bus(dev, 0, 0, 0, 0);

	EP_PCIE_DUMP(dev, "PCIe V%d: QSERDES COM Debug Signals.\n\n", dev->rev);

	for (i = 0; i < 2; i++) {
		write_val = 0x2 + i;

		ep_pcie_write_reg(dev->phy, QSERDES_COM_DEBUG_BUS_SEL,
			write_val);

		EP_PCIE_DUMP(dev,
			"PCIe V%d: to QSERDES_COM_DEBUG_BUS_SEL: 0x%x\n",
			dev->rev,
			readl_relaxed(dev->phy + QSERDES_COM_DEBUG_BUS_SEL));
		EP_PCIE_DUMP(dev,
			"PCIe V%d: QSERDES_COM_DEBUG_BUS0: 0x%x\n",
			dev->rev,
			readl_relaxed(dev->phy + QSERDES_COM_DEBUG_BUS0));
		EP_PCIE_DUMP(dev,
			"PCIe V%d: QSERDES_COM_DEBUG_BUS1: 0x%x\n",
			dev->rev,
			readl_relaxed(dev->phy + QSERDES_COM_DEBUG_BUS1));
		EP_PCIE_DUMP(dev,
			"PCIe V%d: QSERDES_COM_DEBUG_BUS2: 0x%x\n",
			dev->rev,
			readl_relaxed(dev->phy + QSERDES_COM_DEBUG_BUS2));
		EP_PCIE_DUMP(dev,
			"PCIe V%d: QSERDES_COM_DEBUG_BUS3: 0x%x\n\n",
			dev->rev,
			readl_relaxed(dev->phy + QSERDES_COM_DEBUG_BUS3));
	}

	ep_pcie_write_reg(dev->phy, QSERDES_COM_DEBUG_BUS_SEL, 0);

	EP_PCIE_DUMP(dev, "PCIe V%d: QSERDES LANE Debug Signals.\n\n",
			dev->rev);

	for (i = 0; i < 3; i++) {
		write_val = 0x1 + i;
		ep_pcie_write_reg(dev->phy,
			QSERDES_TX_DEBUG_BUS_SEL, write_val);
		EP_PCIE_DUMP(dev,
			"PCIe V%d: QSERDES_TX_DEBUG_BUS_SEL: 0x%x\n",
			dev->rev,
			readl_relaxed(dev->phy + QSERDES_TX_DEBUG_BUS_SEL));

		ep_ep_pcie_phy_dump_pcs_debug_bus(dev, 0x30, 0x31, 0x32, 0x33);
	}

	ep_ep_pcie_phy_dump_pcs_debug_bus(dev, 0, 0, 0, 0);

	EP_PCIE_DUMP(dev, "PCIe V%d: End of PHY debug dump.\n\n", dev->rev);

}

void ep_pcie_reg_dump(struct ep_pcie_dev_t *dev, u32 sel, bool linkdown)
{
	int r, i;
	u32 original;
	u32 size;

	EP_PCIE_DBG(dev,
		"PCIe V%d: Dump PCIe reg for 0x%x %s linkdown.\n",
		dev->rev, sel, linkdown ? "with" : "without");

	if (!dev->power_on) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: the power is already down; can't dump registers.\n",
			dev->rev);
		return;
	}

	if (linkdown) {
		EP_PCIE_DUMP(dev,
			"PCIe V%d: dump PARF registers for linkdown case.\n",
			dev->rev);

		original = readl_relaxed(dev->parf + PCIE20_PARF_SYS_CTRL);
		for (i = 1; i <= 0x1A; i++) {
			ep_pcie_write_mask(dev->parf + PCIE20_PARF_SYS_CTRL,
				0xFF0000, i << 16);
			EP_PCIE_DUMP(dev,
				"PCIe V%d: PARF_SYS_CTRL:0x%x PARF_TEST_BUS:0x%x\n",
				dev->rev,
				readl_relaxed(dev->parf + PCIE20_PARF_SYS_CTRL),
				readl_relaxed(dev->parf +
							PCIE20_PARF_TEST_BUS));
		}
		ep_pcie_write_reg(dev->parf, PCIE20_PARF_SYS_CTRL, original);
	}

	for (r = 0; r < EP_PCIE_MAX_RES; r++) {
		if (!(sel & BIT(r)))
			continue;

		if ((r == EP_PCIE_RES_PHY) && (dev->phy_rev > 3))
			ep_pcie_phy_dump(dev);

		size = resource_size(dev->res[r].resource);
		EP_PCIE_DUMP(dev,
			"\nPCIe V%d: dump registers of %s.\n\n",
			dev->rev, dev->res[r].name);

		for (i = 0; i < size; i += 32) {
			EP_PCIE_DUMP(dev,
				"0x%04x %08x %08x %08x %08x %08x %08x %08x %08x\n",
				i, readl_relaxed(dev->res[r].base + i),
				readl_relaxed(dev->res[r].base + (i + 4)),
				readl_relaxed(dev->res[r].base + (i + 8)),
				readl_relaxed(dev->res[r].base + (i + 12)),
				readl_relaxed(dev->res[r].base + (i + 16)),
				readl_relaxed(dev->res[r].base + (i + 20)),
				readl_relaxed(dev->res[r].base + (i + 24)),
				readl_relaxed(dev->res[r].base + (i + 28)));
		}
	}
}

static void ep_pcie_show_status(struct ep_pcie_dev_t *dev)
{
	EP_PCIE_DBG_FS("PCIe: is %s enumerated\n",
		dev->enumerated ? "" : "not");
	EP_PCIE_DBG_FS("PCIe: link is %s\n",
		(dev->link_status == EP_PCIE_LINK_ENABLED)
		? "enabled" : "disabled");
	EP_PCIE_DBG_FS("the link is %s suspending\n",
		dev->suspending ? "" : "not");
	EP_PCIE_DBG_FS("the power is %s on\n",
		dev->power_on ? "" : "not");
	EP_PCIE_DBG_FS("bus_client: %d\n",
		dev->bus_client);
	EP_PCIE_DBG_FS("linkdown_counter: %lu\n",
		dev->linkdown_counter);
	EP_PCIE_DBG_FS("linkup_counter: %lu\n",
		dev->linkup_counter);
	EP_PCIE_DBG_FS("wake_counter: %lu\n",
		dev->wake_counter);
	EP_PCIE_DBG_FS("d0_counter: %lu\n",
		dev->d0_counter);
	EP_PCIE_DBG_FS("d3_counter: %lu\n",
		dev->d3_counter);
	EP_PCIE_DBG_FS("perst_ast_counter: %lu\n",
		dev->perst_ast_counter);
	EP_PCIE_DBG_FS("perst_deast_counter: %lu\n",
		dev->perst_deast_counter);
}

static ssize_t ep_pcie_cmd_debug(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	unsigned long ret;
	char str[MAX_MSG_LEN];
	unsigned int testcase = 0;
	struct ep_pcie_msi_config msi_cfg;
	int i;
	struct ep_pcie_hw *phandle = NULL;
	struct ep_pcie_iatu entries[2] = {
		{0x80000000, 0xbe7fffff, 0, 0},
		{0xb1440000, 0xb144ae1e, 0x31440000, 0}
	};
	struct ep_pcie_db_config chdb_cfg = {0x64, 0x6b, 0xfd4fa000};
	struct ep_pcie_db_config erdb_cfg = {0x64, 0x6b, 0xfd4fa080};

	phandle = ep_pcie_get_phandle(hw_drv.device_id);

	memset(str, 0, sizeof(str));
	ret = copy_from_user(str, buf, sizeof(str));
	if (ret)
		return -EFAULT;

	for (i = 0; i < sizeof(str) && (str[i] >= '0') && (str[i] <= '9'); ++i)
		testcase = (testcase * 10) + (str[i] - '0');

	EP_PCIE_DBG_FS("PCIe: TEST: %d\n", testcase);


	switch (testcase) {
	case 0: /* output status */
		ep_pcie_show_status(dev);
		break;
	case 1: /* output PHY and PARF registers */
		ep_pcie_reg_dump(dev, BIT(EP_PCIE_RES_PHY) |
				BIT(EP_PCIE_RES_PARF), true);
		break;
	case 2: /* output core registers */
		ep_pcie_reg_dump(dev, BIT(EP_PCIE_RES_DM_CORE), false);
		break;
	case 3: /* output MMIO registers */
		ep_pcie_reg_dump(dev, BIT(EP_PCIE_RES_MMIO), false);
		break;
	case 4: /* output ELBI registers */
		ep_pcie_reg_dump(dev, BIT(EP_PCIE_RES_ELBI), false);
		break;
	case 5: /* output MSI registers */
		ep_pcie_reg_dump(dev, BIT(EP_PCIE_RES_MSI), false);
		break;
	case 6: /* turn on link */
		ep_pcie_enable_endpoint(phandle, EP_PCIE_OPT_ALL);
		break;
	case 7: /* enumeration */
		ep_pcie_enable_endpoint(phandle, EP_PCIE_OPT_ENUM);
		break;
	case 8: /* turn off link */
		ep_pcie_disable_endpoint(phandle);
		break;
	case 9: /* check MSI */
		ep_pcie_get_msi_config(phandle, &msi_cfg);
		break;
	case 10: /* trigger MSI */
		ep_pcie_trigger_msi(phandle, 0);
		break;
	case 11: /* indicate the status of PCIe link */
		EP_PCIE_DBG_FS("\nPCIe: link status is %d.\n\n",
			ep_pcie_get_linkstatus(phandle));
		break;
	case 12: /* configure outbound iATU */
		ep_pcie_config_outbound_iatu(phandle, entries, 2);
		break;
	case 13: /* wake up the host */
		ep_pcie_wakeup_host(phandle);
		break;
	case 14: /* Configure routing of doorbells */
		ep_pcie_config_db_routing(phandle, chdb_cfg, erdb_cfg);
		break;
	case 21: /* write D3 */
		EP_PCIE_DBG_FS("\nPCIe Testcase %d: write D3 to EP\n\n",
			testcase);
		EP_PCIE_DBG_FS("\nPCIe: 0x44 of EP is 0x%x before change\n\n",
			readl_relaxed(dev->dm_core + 0x44));
		ep_pcie_write_mask(dev->dm_core + 0x44, 0, 0x3);
		EP_PCIE_DBG_FS("\nPCIe: 0x44 of EP is 0x%x now\n\n",
			readl_relaxed(dev->dm_core + 0x44));
		break;
	case 22: /* write D0 */
		EP_PCIE_DBG_FS("\nPCIe Testcase %d: write D0 to EP\n\n",
			testcase);
		EP_PCIE_DBG_FS("\nPCIe: 0x44 of EP is 0x%x before change\n\n",
			readl_relaxed(dev->dm_core + 0x44));
		ep_pcie_write_mask(dev->dm_core + 0x44, 0x3, 0);
		EP_PCIE_DBG_FS("\nPCIe: 0x44 of EP is 0x%x now\n\n",
			readl_relaxed(dev->dm_core + 0x44));
		break;
	case 23: /* assert wake */
		EP_PCIE_DBG_FS("\nPCIe Testcase %d: assert wake\n\n",
			testcase);
		gpio_set_value(dev->gpio[EP_PCIE_GPIO_WAKE].num,
			dev->gpio[EP_PCIE_GPIO_WAKE].on);
		break;
	case 24: /* deassert wake */
		EP_PCIE_DBG_FS("\nPCIe Testcase %d: deassert wake\n\n",
			testcase);
		gpio_set_value(dev->gpio[EP_PCIE_GPIO_WAKE].num,
			1 - dev->gpio[EP_PCIE_GPIO_WAKE].on);
		break;
	case 25: /* output PERST# status */
		EP_PCIE_DBG_FS("\nPCIe: PERST# is %d.\n\n",
			gpio_get_value(dev->gpio[EP_PCIE_GPIO_PERST].num));
		break;
	case 26: /* output WAKE# status */
		EP_PCIE_DBG_FS("\nPCIe: WAKE# is %d.\n\n",
			gpio_get_value(dev->gpio[EP_PCIE_GPIO_WAKE].num));
		break;
	case 31: /* output core registers when D3 hot is set by host*/
		dev->dump_conf = true;
		break;
	case 32: /* do not output core registers when D3 hot is set by host*/
		dev->dump_conf = false;
		break;
	default:
		EP_PCIE_DBG_FS("PCIe: Invalid testcase: %d.\n", testcase);
		break;
	}

	if (ret == 0)
		return count;
	else
		return -EFAULT;
}

const struct file_operations ep_pcie_cmd_debug_ops = {
	.write = ep_pcie_cmd_debug,
};

void ep_pcie_debugfs_init(struct ep_pcie_dev_t *ep_dev)
{
	dev = ep_dev;
	dent_ep_pcie = debugfs_create_dir("pcie-ep", 0);
	if (IS_ERR(dent_ep_pcie)) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: fail to create the folder for debug_fs.\n",
			dev->rev);
		return;
	}

	dfile_case = debugfs_create_file("case", 0664,
					dent_ep_pcie, 0,
					&ep_pcie_cmd_debug_ops);
	if (!dfile_case || IS_ERR(dfile_case)) {
		EP_PCIE_ERR(dev,
			"PCIe V%d: fail to create the file for case.\n",
			dev->rev);
		goto case_error;
	}

	EP_PCIE_DBG2(dev,
		"PCIe V%d: debugfs is enabled.\n",
		dev->rev);

	return;

case_error:
	debugfs_remove(dent_ep_pcie);
}

void ep_pcie_debugfs_exit(void)
{
	debugfs_remove(dfile_case);
	debugfs_remove(dent_ep_pcie);
}
