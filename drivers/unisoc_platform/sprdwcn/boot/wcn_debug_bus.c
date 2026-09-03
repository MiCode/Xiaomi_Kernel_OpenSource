// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "wcn_glb.h"
#include "wcn_glb_reg.h"
#include "wcn_gnss.h"
#include "wcn_misc.h"
#include "wcn_procfs.h"
#include "wcn_debug_bus.h"
#include "gnss_dump.h"

void wcn_debug_bus_show(struct wcn_device *wcn_dev, char *show)
{
	void __iomem *sysbase = NULL;
	u32 modsel = 0, sigsel = 0, i = 0;
	u32 *debugbus_data = NULL;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	pr_info("DEBUGBUS SOURCE:%s\n", show);
	pr_info("------------------------------------\n");
	pr_info("+             DBGBUS               +\n");
	pr_info("------------------------------------\n");

	if (wcn_dev == NULL) {
		if (IS_ERR_OR_NULL(s_wcn_device.btwf_device)) {
			WCN_ERR("debugbus is not ready!\n");
			return;
		}
		wcn_dev = s_wcn_device.btwf_device;
	}

	if (wcn_dev->dbus.maxsz == 0)
		return;

	wcn_dev->dbus.dbus_data_pool = kzalloc(wcn_dev->dbus.maxsz, GFP_KERNEL);
	if (!wcn_dev->dbus.dbus_data_pool) {
		WCN_ERR("%s fail to malloc\n", __func__);
		return;
	}
	debugbus_data = (u32 *)wcn_dev->dbus.dbus_data_pool;

	sysbase = wcn_dev->dbus.dbus_reg_base;
	WCN_INFO("phyaddr:0x%llx, sysbase:0x%p\n", wcn_dev->dbus.phy_reg, sysbase);
	memset(wcn_dev->dbus.dbus_data_pool, 0, wcn_dev->dbus.maxsz);
	wcn_dev->dbus.curr_size = 0;

	/* READ AON debugbus */
	writel(READ_AON_DEBUGBUS, sysbase + SYSSEL_CFG0_OFFSET);
	pr_info("++++++++++++++++++++AON++++++++++++++++++++");
	for (sigsel = 0; sigsel <= 8; sigsel++) {
		writel(sigsel << 16, sysbase + SYSSEL_CFG2_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
				sigsel, sigsel << 16, sysbase + SYSSEL_CFG2_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:AON(0-8) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	for (sigsel = 0x10; sigsel <= 0x18; sigsel++) {
		writel(sigsel << 16, sysbase + SYSSEL_CFG2_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
				sigsel, sigsel << 16, sysbase + SYSSEL_CFG2_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:AON(0x10-0x18) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}

	for (sigsel = 0x20; sigsel <= 0x28; sigsel++) {
		writel(sigsel << 16, sysbase + SYSSEL_CFG2_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
				sigsel, sigsel << 16, sysbase + SYSSEL_CFG2_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:AON(0x20-0x28) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	pr_info("++++++++++++++++++++AON++++++++++++++++++++");

	/* read AP debugbus */
	pr_info("++++++++++++++++++++AP++++++++++++++++++++");
	writel(READ_AP_DEBUGBUS, sysbase + SYSSEL_CFG0_OFFSET);
	for (sigsel = 1; sigsel <= 0x38; sigsel++) {
		writel(sigsel << 8, sysbase + SYSSEL_CFG1_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
			sigsel, sigsel << 8, sysbase + SYSSEL_CFG1_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:AP debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	pr_info("++++++++++++++++++++AP++++++++++++++++++++");

	/* read apcpu debugbus */
	pr_info("++++++++++++++++++++APCPU++++++++++++++++++++");
	writel(READ_APCPU_DEBUGBUS, sysbase + SYSSEL_CFG0_OFFSET);
	for (modsel = 0; modsel <= 0x22; modsel++) {
		writel(modsel << 16, sysbase + SYSSEL_CFG4_OFFSET);
		pr_info("DEBUGBUS:modsel=0x%02x, (0x%x) Write (REG 0x%p)",
				modsel, modsel << 16, sysbase + SYSSEL_CFG4_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:APCPU debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	pr_info("++++++++++++++++++++APCPU++++++++++++++++++++");

	/* read audcp debugbus */
	pr_info("++++++++++++++++++++AUDCP++++++++++++++++++++");
	writel(READ_AUDCP_DEBUGBUS, sysbase + SYSSEL_CFG0_OFFSET);
	for (sigsel = 0; sigsel <= 0x2B; sigsel++) {
		writel(sigsel << 0, sysbase + SYSSEL_CFG3_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
				sigsel, sigsel << 0, sysbase + SYSSEL_CFG3_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:AUDCP debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	pr_info("++++++++++++++++++++AUDCP++++++++++++++++++++");

	/* read gpu debugbus */
	pr_info("++++++++++++++++++++GPU++++++++++++++++++++");
	writel(READ_GPU_DEBUGBUS, sysbase + SYSSEL_CFG0_OFFSET);
	for (sigsel = 1; sigsel <= 0x6; sigsel++) {
		writel(sigsel << 24, sysbase + SYSSEL_CFG1_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
				sigsel, sigsel << 24, sysbase + SYSSEL_CFG1_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:GPU(1-0x6) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	for (sigsel = 8; sigsel <= 0xB; sigsel++) {
		writel(sigsel << 24, sysbase + SYSSEL_CFG1_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
			sigsel, sigsel << 24, sysbase + SYSSEL_CFG1_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:GPU(8-0xB) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	writel(0xf << 24, sysbase + SYSSEL_CFG1_OFFSET);
	debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
	pr_info("DEBUGBUS:(0x%x) Write (REG 0x%p)",
			0xf << 24, sysbase + SYSSEL_CFG1_OFFSET);
	pr_info("DEBUGBUS:GPU debugbus data: 0x%x\n", debugbus_data[i - 1]);
	pr_info("++++++++++++++++++++GPU++++++++++++++++++++");

	/* read aon_lp debugbus */
	pr_info("++++++++++++++++++++AON_LP++++++++++++++++++++");
	writel(READ_AONLP_DEBUGBUS, sysbase + SYSSEL_CFG0_OFFSET);
	writel(1 << 24, sysbase + SYSSEL_CFG4_OFFSET);
	for (sigsel = 0; sigsel <= 0x4c; sigsel++) {
		writel(sigsel << 8, sysbase + SYSSEL_CFG2_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
			sigsel, sigsel << 8, sysbase + SYSSEL_CFG2_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:AON_LP(1>0x%p) debugbus data: 0x%x\n",
				sysbase + SYSSEL_CFG4_OFFSET, debugbus_data[i - 1]);
	}

	writel(2 << 24, sysbase + SYSSEL_CFG4_OFFSET);
	for (sigsel = 0; sigsel <= 0x84; sigsel++) {
		writel(sigsel << 8, sysbase + SYSSEL_CFG2_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
			sigsel, sigsel << 8, sysbase + SYSSEL_CFG2_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:AON_LP(2>0x%p) debugbus data: 0x%x\n",
				sysbase + SYSSEL_CFG4_OFFSET, debugbus_data[i - 1]);
	}

	writel(0 << 24, sysbase + SYSSEL_CFG4_OFFSET);
	for (sigsel = 0; sigsel <= 0x93; sigsel++) {
		writel(sigsel << 8, sysbase + SYSSEL_CFG2_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
			sigsel, sigsel << 8, sysbase + SYSSEL_CFG2_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:AON_LP(2>0x%p) debugbus data: 0x%x\n",
				sysbase + SYSSEL_CFG4_OFFSET, debugbus_data[i - 1]);
	}

	writel(3 << 24, sysbase + SYSSEL_CFG4_OFFSET);
	for (sigsel = 0; sigsel <= 0x9D; sigsel++) {
		writel(sigsel << 8, sysbase + SYSSEL_CFG2_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
			sigsel, sigsel << 8, sysbase + SYSSEL_CFG2_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:AON_LP(3>0x%p) debugbus data: 0x%x\n",
				sysbase + SYSSEL_CFG4_OFFSET, debugbus_data[i - 1]);
	}
	pr_info("++++++++++++++++++++AON_LP++++++++++++++++++++");

	/* read mm debugbus */
	pr_info("++++++++++++++++++++MM++++++++++++++++++++");
	writel(READ_MM_DEBUGBUS, sysbase + SYSSEL_CFG0_OFFSET);
	for (sigsel = 1; sigsel <= 50; sigsel++) {
		writel(sigsel << 16, sysbase + SYSSEL_CFG1_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
			sigsel, sigsel << 16, sysbase + SYSSEL_CFG1_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:MM debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	pr_info("++++++++++++++++++++MM++++++++++++++++++++");

	/* read pub debugbus */
	pr_info("++++++++++++++++++++PUB++++++++++++++++++++");
	writel(READ_PUB_DEBUGBUS, sysbase + SYSSEL_CFG0_OFFSET);
	for (sigsel = 1; sigsel <= 60; sigsel++) {
		writel(sigsel, sysbase + SYSSEL_CFG2_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
			sigsel, sigsel, sysbase + SYSSEL_CFG2_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:PUB(1-60) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	for (sigsel = 0x80; sigsel <= 0xBE; sigsel++) {
		writel(sigsel, sysbase + SYSSEL_CFG2_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
			sigsel, sigsel, sysbase + SYSSEL_CFG2_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:PUB(0x80-0xBE) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	for (sigsel = 0xED; sigsel <= 0xF8; sigsel++) {
		writel(sigsel, sysbase + SYSSEL_CFG2_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
			sigsel, sigsel, sysbase + SYSSEL_CFG2_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:PUB(0xED-0xF8) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}

	for (sigsel = 0x40; sigsel <= 0x46; sigsel++) {
		writel(sigsel, sysbase + SYSSEL_CFG2_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
			sigsel, sigsel, sysbase + SYSSEL_CFG2_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:PUB(0x40-0x46) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}

	for (sigsel = 0x50; sigsel <= 0x56; sigsel++) {
		writel(sigsel, sysbase + SYSSEL_CFG2_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
			sigsel, sigsel, sysbase + SYSSEL_CFG2_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:PUB(0x50-0x56) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	pr_info("++++++++++++++++++++PUB++++++++++++++++++++");

	/* read pubcp debugbus */
	pr_info("++++++++++++++++++++PUBCP++++++++++++++++++++");
	writel(READ_PUBCP_DEBUGBUS, sysbase + SYSSEL_CFG0_OFFSET);
	for (sigsel = 0; sigsel <= 0x41; sigsel++) {
		writel(sigsel << 8, sysbase + SYSSEL_CFG3_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
			sigsel, sigsel << 8, sysbase + SYSSEL_CFG3_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:PUBCP debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	pr_info("++++++++++++++++++++PUBCP++++++++++++++++++++");

	/* read wcn debugbus */
	pr_info("++++++++++++++++++++WCN++++++++++++++++++++");
	writel(READ_WCN_DEBUGBUS, sysbase + SYSSEL_CFG0_OFFSET);
	for (sigsel = 1; sigsel <= 94; sigsel++) {
		writel((0 << 8) | sigsel, sysbase + SYSSEL_CFG5_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
				sigsel, (0 << 8) | sigsel, sysbase + SYSSEL_CFG5_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:WCN(1-94) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	writel((4 << 8) | 0, sysbase + SYSSEL_CFG5_OFFSET);
	pr_info("DEBUGBUS:(0x%x) Write (REG 0x%p)",
			(4 << 8) | 0, sysbase + SYSSEL_CFG5_OFFSET);
	debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
	pr_info("DEBUGBUS:WCN debugbus data: 0x%x\n", debugbus_data[i - 1]);

	for (sigsel = 1; sigsel <= 22; sigsel++) {
		writel((1 << 8) | sigsel, sysbase + SYSSEL_CFG5_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
				sigsel, (1 << 8) | sigsel, sysbase + SYSSEL_CFG5_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:WCN(1-22) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}

	for (sigsel = 0; sigsel <= 15; sigsel++) {
		writel((3 << 8) | sigsel, sysbase + SYSSEL_CFG5_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
				sigsel, (3 << 8) | sigsel, sysbase + SYSSEL_CFG5_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:WCN(0-15) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}

	for (sigsel = 1; sigsel <= 29; sigsel++) {
		writel((2 << 8) | sigsel, sysbase + SYSSEL_CFG5_OFFSET);
		pr_info("DEBUGBUS:sigsel=0x%02x, (0x%x) Write (REG 0x%p)",
				sigsel, (2 << 8) | sigsel, sysbase + SYSSEL_CFG5_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:WCN(1-29) debugbus data: 0x%x\n", debugbus_data[i - 1]);

		if (g_match_config && g_match_config->unisoc_wcn_integrated) {
			if (sigsel == 1)
				gnss_set_clk_gate_en(debugbus_data[i - 1]);
		}
	}
	pr_info("++++++++++++++++++++WCN++++++++++++++++++++");

	/* read wtlcp debugbus */
	writel(READ_WTLCP_DEBUGBUS, sysbase + SYSSEL_CFG0_OFFSET);
	for (modsel = 0; modsel <= 0x8F; modsel++) {
		writel(modsel << 8, sysbase + SYSSEL_CFG4_OFFSET);
		pr_info("DEBUGBUS:modsel=0x%02x, (0x%x) Write (REG 0x%p)",
				modsel, modsel << 8, sysbase + SYSSEL_CFG4_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:WTLCP(0-0x8F) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	for (modsel = 0xC1; modsel <= 0xCB; modsel++) {
		writel(modsel << 8, sysbase + SYSSEL_CFG4_OFFSET);
		pr_info("DEBUGBUS:modsel=0x%02x, (0x%x) Write (REG 0x%p)",
				modsel, modsel << 8, sysbase + SYSSEL_CFG4_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:WTLCP(0xC1-0xCB) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}
	for (modsel = 0xF0; modsel <= 0xF3; modsel++) {
		writel(modsel << 8, sysbase + SYSSEL_CFG4_OFFSET);
		pr_info("DEBUGBUS:modsel=0x%02x, (0x%x) Write (REG 0x%p)",
				modsel, modsel << 8, sysbase + SYSSEL_CFG4_OFFSET);
		debugbus_data[i++] = readl(sysbase + PAD_DBUS_DATA_OUT_OFFSET);
		pr_info("DEBUGBUS:WTLCP(0xF0-0xF3) debugbus data: 0x%x\n", debugbus_data[i - 1]);
	}

	wcn_dev->dbus.curr_size = i * sizeof(*debugbus_data);
	WCN_INFO("Phy_add(0x%llx) clear, len=0x%llx\n", wcn_dev->dbus.base_addr,
			wcn_dev->dbus.curr_size);

	if (!wcn_dev->db_to_ddr_disable) {
		wcn_write_zero_to_phy_addr(wcn_dev->dbus.base_addr, wcn_dev->dbus.maxsz);
		WCN_INFO("Debug bus Write to 0x%llx, len=0x%llx\n", wcn_dev->dbus.base_addr,
				wcn_dev->dbus.curr_size);
		if (wcn_write_data_to_phy_addr(wcn_dev->dbus.base_addr,
					debugbus_data, wcn_dev->dbus.curr_size) < 0) {
			WCN_INFO("%s Fail to write\n", __func__);
		}
	}

	memset(wcn_dev->dbus.db_temp, 0, ARRAY_SIZE(wcn_dev->dbus.db_temp));
	if (wcn_dev->dbus.curr_size <= sizeof(wcn_dev->dbus.db_temp))
		memcpy(wcn_dev->dbus.db_temp, debugbus_data, wcn_dev->dbus.curr_size);
	else
		WCN_ERR("debugbus data cannot be backed up\n");

	debugbus_data = NULL;
	kfree(wcn_dev->dbus.dbus_data_pool);
	/* Debugbus data backup */
	wcn_dev->dbus.dbus_data_pool = wcn_dev->dbus.db_temp;
}
EXPORT_SYMBOL_GPL(wcn_debug_bus_show);

void debug_bus_show(char *show)
{
	wcn_debug_bus_show(NULL, show);
}
EXPORT_SYMBOL_GPL(debug_bus_show);
