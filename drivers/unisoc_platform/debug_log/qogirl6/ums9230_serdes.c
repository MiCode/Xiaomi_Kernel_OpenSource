// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <dt-bindings/soc/sprd,qogirl6-mask.h>
#include <dt-bindings/soc/sprd,qogirl6-regs.h>

#include "../core.h"

#define FREQ_OFFSET (50 * 1000)
#define MATCH_FREQ(f, src_f) (abs((f) - (src_f)) <= FREQ_OFFSET)
#define REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB	0x007c
#define REG_MM_AHB_GEN_CLK_CFG						0x0008
#define REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB	0x0078
#define REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_REG_SEL_CFG_0		0x008c
#define REG_MM_AHB_MIPI_CSI_SEL_CTRL					0x0030
#define REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST		0x00b4

static u32 s_freq_array[] = {
	1400000,
};

/**
 * Write to D-PHY module (encapsulating the digital interface)
 * @param base pointer to structure which holds information about the d-base
 * module
 * @param address offset inside the D-PHY digital interface
 * @param data array of bytes to be written to D-PHY
 * @param data_length of the data array
 */
static void dbg_phy_test_write(struct regmap *base, u8 address, u8 data)
{
	regmap_write_bits(base, REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
				0xFFFFFFFF, (address << 11) | (0x1 << 2));
	regmap_write_bits(base, REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
				0xFFFFFFFF, (address << 11) | (0x1 << 2) | (0x1 << 1));
	regmap_write_bits(base, REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
				0xFFFFFFFF, (address << 11) | (0x1 << 2));
	regmap_write_bits(base, REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
				0xFFFFFFFF, (data << 11));
	regmap_write_bits(base, REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
				0xFFFFFFFF, (data << 11) | (0x1 << 1));
	regmap_write_bits(base, REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
				0xFFFFFFFF, (data << 11));
}

static void inter_dbg_log_init(struct dbg_log_device *dbg)
{
	DEBUG_LOG_PRINT("entry\n");

	/* enable serdes */
	clk_prepare_enable(dbg->clk_serdes_eb);

	/* enable cphy cfg clk */
	clk_prepare_enable(dbg->clk_cphy_cfg_eb);

	/* enable CGM_DSI_CSI_TEST_EB */
	clk_prepare_enable(dbg->clk_dsi_csi_test_eb);

	/* enable mm eb */
	clk_prepare_enable(dbg->clk_mm_eb);

	/* enable ana eb */
	clk_prepare_enable(dbg->clk_ana_eb);

	/* clear force shutdown */
	regmap_update_bits(dbg->phy->pmu_apb, REG_PMU_APB_PD_MM_CFG_0,
						MASK_PMU_APB_PD_MM_FORCE_SHUTDOWN, 0);

	/* power on */
	regmap_update_bits(dbg->phy->pmu_apb, REG_PMU_APB_PD_MM_CFG_0,
						MASK_PMU_APB_PD_MM_AUTO_SHUTDOWN_EN, 0);

	/* here need wait */
	usleep_range(1000, 1100); /* Wait for 1mS */

	/* enable cphy cfg clk in mm */
	regmap_update_bits(dbg->phy->mm_ahb, REG_MM_AHB_GEN_CLK_CFG/*0x8*/, 0x100, 0x100);

	/* here need wait */
	usleep_range(1000, 1100); /* Wait for 1mS */

	/* set 2P2L DSI TEST_CLR */
	regmap_update_bits(dbg->phy->dsi_apb,
				REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB/*0x7C*/,
				0x1, 0x1);

	/* disable 2P2L DSI SHUTDOWNZ_DB[15] and RSTZ_DB[14] */
	regmap_update_bits(dbg->phy->dsi_apb,
				REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB/*0x78*/,
				0xC000, 0xC000);

	/* set dbg_sel 2P2L DSI SHUTDOWNZ_DB */
	regmap_update_bits(dbg->phy->dsi_apb,
				REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_REG_SEL_CFG_0/*0x8C*/,
				0x40, 0x40);

	/* set MIPI CSI SEL CTRL[2:0] = 3'B001 */
	regmap_update_bits(dbg->phy->mm_ahb, REG_MM_AHB_MIPI_CSI_SEL_CTRL/*0x30*/, 0x1, 0x1);
	regmap_update_bits(dbg->phy->mm_ahb, REG_MM_AHB_MIPI_CSI_SEL_CTRL/*0x30*/, 0x2, 0x2);

	/* set csi_2p2l_test_clr_m_sel and csi_2p2l_test_clr_m */
	regmap_update_bits(dbg->phy->dsi_apb,
				REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST/*0xb4*/,
				0x300000, 0x300000);

	/* 100ns */
	usleep_range(1000, 1100); /* Wait for 1mS */

	/* clr csi_2p2l_test_clr_m_sel and csi_2p2l_test_clr_m */
	regmap_update_bits(dbg->phy->dsi_apb,
				REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST/*0xb4*/,
				0x300000, ~0x300000);

	/* set MIPI CSI SEL CTRL[2:0] = 3'B000 */
	regmap_update_bits(dbg->phy->mm_ahb, REG_MM_AHB_MIPI_CSI_SEL_CTRL/*0x30*/, 0x7, ~0x7);
	regmap_update_bits(dbg->phy->mm_ahb, REG_MM_AHB_MIPI_CSI_SEL_CTRL/*0x30*/, 0x3, ~0x3);

	/* set csi_2p2l_test_clr_s_sel and csi_2p2l_test_clr_s */
	regmap_update_bits(dbg->phy->dsi_apb,
				REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST/*0xb4*/,
				0x60000, 0x60000);

	/* 100ns */
	usleep_range(1000, 1100); /* Wait for 1mS */

	/* clr csi_2p2l_test_clr_s_sel and csi_2p2l_test_clr_s */
	regmap_update_bits(dbg->phy->dsi_apb,
				REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST/*0xb4*/,
				0x60000, ~0x60000);

	/* set MIPI CSI SEL CTRL[2:0] = 3'B010 */
	regmap_update_bits(dbg->phy->mm_ahb, REG_MM_AHB_MIPI_CSI_SEL_CTRL/*0x30*/, 0x2, 0x2);
	regmap_update_bits(dbg->phy->mm_ahb, REG_MM_AHB_MIPI_CSI_SEL_CTRL/*0x30*/, 0x1, 0x1);

	/* set dbg_sel_csi_2p2l_dsi_if_sel_db */
	regmap_update_bits(dbg->phy->dsi_apb,
				REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_REG_SEL_CFG_0/*0x8C*/,
				0x20, 0x20);

	/* release 2P2L DSI TEST_CLR */
	regmap_update_bits(dbg->phy->dsi_apb,
				REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB/*0x7C*/,
				0x1, ~0x1);

	/* enable 2P2L DSI CLK DB */
	regmap_update_bits(dbg->phy->dsi_apb,
				REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB/*0x78*/,
				0x200, 0x200);

	/* enable 2P2L DSI CLK EN */
	regmap_update_bits(dbg->phy->dsi_apb,
				REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB/*0x78*/,
				0x3C00, 0x3C00);

	/* 500ns */
	usleep_range(1000, 1100); /* Wait for 1mS */

	/* wire PHY REG[B] : 0xEC */
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x25, 0x7);
	/* wire PHY REG[B] : 0xEC */
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x8, 0xDE);
	/* wire PHY REG[B] : 0xEC */
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x6, 0x35);
	/* wire PHY REG[B] : 0xEC */
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x9, 0xD8);
	/* wire PHY REG[B] : 0x4E */
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xA, 0x9D);
	/* wire PHY REG[B] : 0xC2 */
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xB, 0x81);
	/* wire PHY REG[B] : 0x2 */

	/* 130us */
	usleep_range(1000, 1100); /* Wait for 1mS */
}

static void inter_dbg_log_exit(struct dbg_log_device *dbg)
{
	/* 1:auto shutdown en, shutdown with ap */
	regmap_update_bits(dbg->phy->pmu_apb, REG_PMU_APB_PD_MM_CFG_0,
				MASK_PMU_APB_PD_MM_AUTO_SHUTDOWN_EN, 0);
	/* set 1 to shutdown */
	regmap_update_bits(dbg->phy->pmu_apb, REG_PMU_APB_PD_MM_CFG_0,
				MASK_PMU_APB_PD_MM_FORCE_SHUTDOWN, ~((uint32_t)0));

	/* disable serdes */
	clk_disable_unprepare(dbg->clk_serdes_eb);

	/* disable serdes DPHY_CFG_EB & DPHY_REF_EB */
	clk_disable_unprepare(dbg->clk_dphy_cfg_eb);
	clk_disable_unprepare(dbg->clk_dphy_ref_eb);
}

static void inter_dbg_log_chn_sel(struct dbg_log_device *dbg)
{
	if (dbg->channel) {
		dbg->serdes.channel = dbg->serdes.ch_map[dbg->channel - 1];
		serdes_enable(&dbg->serdes, 1);
	} else {
		serdes_enable(&dbg->serdes, 0);
	}
}

static bool inter_dbg_log_is_freq_valid(struct dbg_log_device *dbg, unsigned int freq)
{
	int i;

	DEBUG_LOG_PRINT("input freq %d\n", freq);
	for (i = 0; i < ARRAY_SIZE(s_freq_array); i++) {
		if (s_freq_array[i] == freq) {
			dbg->phy->clk_sel = i;
			return true;
		}
	}
	DEBUG_LOG_PRINT("input freq %d not match\n", freq);
	return false;
}

static int inter_dbg_log_get_valid_channel(struct dbg_log_device *dbg, const char *buf)
{
	int i, cmp_len = strlen(buf);

	DEBUG_LOG_PRINT("input channel %s", buf);
	if (!strncasecmp(STR_CH_DISABLE, buf, cmp_len))
		return 0;
	for (i = 0; i < dbg->serdes.ch_num; i++) {
		if (!strncasecmp(dbg->serdes.ch_str[i], buf, cmp_len))
			return i + 1;
	}
	DEBUG_LOG_PRINT("not match input channel %s", buf);
	return -EINVAL;
}

static bool inter_dbg_log_fill_freq_array(struct dbg_log_device *dbg,
					  char *sbuf)
{
	int i, ret;
	char temp_buf[16];

	strcat(sbuf, "[");
	for (i = 0; i < ARRAY_SIZE(s_freq_array); i++) {
		ret = snprintf(temp_buf,
			       16, " %u",
			       (unsigned int)s_freq_array[i]);
		if (ret >= 16) {
			DEBUG_LOG_PRINT("len(%d) of s_freq_array[%d] >= 16",
					ret,
					i);
			return false;
		}
		strcat(sbuf, temp_buf);
	}
	strcat(sbuf, "]");

	return true;
}

static struct dbg_log_ops ops = {
	.init = inter_dbg_log_init,
	.exit = inter_dbg_log_exit,
	.select = inter_dbg_log_chn_sel,
	.is_freq_valid = inter_dbg_log_is_freq_valid,
	.fill_freq_array = inter_dbg_log_fill_freq_array,
	.get_valid_channel = inter_dbg_log_get_valid_channel,
};

static int dbg_log_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *addr, *serdes_apb;
	struct dbg_log_device *dbg;
	struct regmap *dsi_apb, *mm_ahb, *pmu_apb;
	int count, i, rc;

	DEBUG_LOG_PRINT("mipiserdes entry\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr))
		return PTR_ERR(addr);

	serdes_apb = (void __iomem *)addr;

	dsi_apb = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, "sprd,syscon-dsi-apb");
	if (IS_ERR(dsi_apb)) {
		pr_err("dsi apb get failed!\n");
		return PTR_ERR(dsi_apb);
	}

	mm_ahb = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, "sprd,syscon-mm-ahb");
	if (IS_ERR(mm_ahb)) {
		pr_err("mm ahb get failed!\n");
		return PTR_ERR(mm_ahb);
	}

	pmu_apb = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, "sprd,syscon-pmu-apb");
	if (IS_ERR(pmu_apb)) {
		pr_err("pmu apb get failed!\n");
		return PTR_ERR(pmu_apb);
	}

	dbg = dbg_log_device_register(&pdev->dev, &ops, NULL, "debug-log");
	if (!dbg)
		return -ENOMEM;

	dbg->phy->freq = s_freq_array[0];
	dbg->phy->dsi_apb = dsi_apb;
	dbg->phy->mm_ahb = mm_ahb;
	dbg->phy->pmu_apb = pmu_apb;
	dbg->serdes.base = serdes_apb;
	dbg->serdes.cut_off = 0x20;

	if (of_property_read_bool(pdev->dev.of_node, "sprd,mm")) {
		DEBUG_LOG_PRINT("mm enable\n");
		dbg->mm = true;
	}

	if (of_property_read_bool(pdev->dev.of_node, "sprd,dcfix")) {
		DEBUG_LOG_PRINT("dcfix enable\n");
		dbg->serdes.dc_blnc_fix = 1;
	}

	count = of_property_count_strings(pdev->dev.of_node, "sprd,ch-name");
	DEBUG_LOG_PRINT("ch_num %d\n", count);

	if (count > 0 && count < CH_MAX) {
		dbg->serdes.ch_num = count;
		rc = of_property_read_u32_array(pdev->dev.of_node,
						"sprd,ch-index",
						dbg->serdes.ch_map,
						count);
		DEBUG_LOG_PRINT("ch-index count %d, rc %d\n", count, rc);
		if (rc) {
			pr_err("get channel map failed\n");
			dbg->serdes.ch_num = 0;
		}
		for (i = 0; i < count; i++)
			DEBUG_LOG_PRINT("sel %d = 0x%x\n", i, dbg->serdes.ch_map[i]);

		rc = of_property_read_string_array(pdev->dev.of_node,
						   "sprd,ch-name",
						   dbg->serdes.ch_str,
						   count);
		DEBUG_LOG_PRINT("ch-name count %d, rc %d\n", count, rc);
		if (rc != count) {
			pr_err("get channel string failed\n");
			dbg->serdes.ch_num = 0;
		}
		for (i = 0; i < count; i++)
			DEBUG_LOG_PRINT("str %d = %s\n", i, dbg->serdes.ch_str[i]);
	}

	dbg->clk_serdes_eb = devm_clk_get(&pdev->dev, "serdes_eb");
	if (IS_ERR(dbg->clk_serdes_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: serdes_eb\n");
		dbg->clk_serdes_eb = NULL;
	}
	dbg->clk_mm_eb = devm_clk_get(&pdev->dev, "mm_eb");
	if (IS_ERR(dbg->clk_mm_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: mm_eb\n");
		dbg->clk_mm_eb = NULL;
	}
	dbg->clk_ana_eb = devm_clk_get(&pdev->dev, "ana_eb");
	if (IS_ERR(dbg->clk_ana_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: ana_eb\n");
		dbg->clk_ana_eb = NULL;
	}
	dbg->clk_dsi_csi_test_eb = devm_clk_get(&pdev->dev, "dsi_csi_test_eb");
	if (IS_ERR(dbg->clk_dsi_csi_test_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: dsi_csi_test_eb\n");
		dbg->clk_dsi_csi_test_eb = NULL;
	}
	dbg->clk_cphy_cfg_eb = devm_clk_get(&pdev->dev, "cphy_cfg_eb");
	if (IS_ERR(dbg->clk_cphy_cfg_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: cphy_cfg_eb\n");
		dbg->clk_cphy_cfg_eb = NULL;
	}

	count = 0;
	for (i = 0; i < CLK_SRC_MAX; i++) {
		char src_str[8];

		snprintf(src_str, 8, "src%d", i);
		dbg->clk_src[i] = devm_clk_get(&pdev->dev, src_str);
		if (IS_ERR(dbg->clk_src[i])) {
			dev_warn(&pdev->dev,
				 "can't get the clock dts config: %s\n",
				 src_str);
			dbg->clk_src[i] = NULL;
		} else {
			count++;
		}
	}

	rc = of_property_read_u32_array(pdev->dev.of_node,
					"sprd,div1_map",
					dbg->phy->div1_map, count);
	DEBUG_LOG_PRINT("div1 map count %d, rc %d\n", count, rc);
	if (rc)
		pr_err("get div1 map failed\n");

	inter_dbg_log_is_freq_valid(dbg, dbg->phy->freq);

	return 0;
}

static const struct of_device_id dt_ids[] = {
	{.compatible = "sprd,dbg-log-qogirl6",},
	{},
};

static struct platform_driver dbg_log_driver = {
	.probe = dbg_log_probe,
	.driver = {
		   .name = "modem-dbg-log",
		   .of_match_table = dt_ids,
		   },
};

module_platform_driver(dbg_log_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Sitao Chen <sitao.chen@unisoc.com>");
MODULE_AUTHOR("Ten Gao <ten.gao@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum SoC Modem Debug Log Driver");

