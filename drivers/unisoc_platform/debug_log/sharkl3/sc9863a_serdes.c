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
#include <dt-bindings/soc/sprd,sharkl3-mask.h>
#include <dt-bindings/soc/sprd,sharkl3-regs.h>
#include "../core.h"
#include "phy.h"

#define FREQ_OFFSET (50 * 1000)
#define MATCH_FREQ(f, src_f) (abs((f) - (src_f)) <= FREQ_OFFSET)

static void dbg_phy_ps_pd_l(struct dbg_log_device *dbg, int h)
{
	if (h)
		regmap_update_bits(dbg->aon_apb, REG_AON_APB_PWR_CTRL,
				   MASK_AON_APB_MIPI_CSI_2P2LANE_PS_PD_L,
				   MASK_AON_APB_MIPI_CSI_2P2LANE_PS_PD_L);
	else
		regmap_update_bits(dbg->aon_apb, REG_AON_APB_PWR_CTRL,
				   MASK_AON_APB_MIPI_CSI_2P2LANE_PS_PD_L,
				   ~MASK_AON_APB_MIPI_CSI_2P2LANE_PS_PD_L);
}

static void dbg_phy_ps_pd_s(struct dbg_log_device *dbg, int h)
{
	if (h)
		regmap_update_bits(dbg->aon_apb, REG_AON_APB_PWR_CTRL,
				   MASK_AON_APB_MIPI_CSI_2P2LANE_PS_PD_S,
				   MASK_AON_APB_MIPI_CSI_2P2LANE_PS_PD_S);
	else
		regmap_update_bits(dbg->aon_apb, REG_AON_APB_PWR_CTRL,
				   MASK_AON_APB_MIPI_CSI_2P2LANE_PS_PD_S,
				   ~MASK_AON_APB_MIPI_CSI_2P2LANE_PS_PD_S);
}

static void dbg_phy_iso_sw_en(struct dbg_log_device *dbg, int h)
{
	if (h)
		regmap_update_bits(dbg->aon_apb, REG_AON_APB_PWR_CTRL,
				   MASK_AON_APB_MIPI_CSI_2P2LANE_ISO_SW_EN,
				   MASK_AON_APB_MIPI_CSI_2P2LANE_ISO_SW_EN);
	else
		regmap_update_bits(dbg->aon_apb, REG_AON_APB_PWR_CTRL,
				   MASK_AON_APB_MIPI_CSI_2P2LANE_ISO_SW_EN,
				   ~MASK_AON_APB_MIPI_CSI_2P2LANE_ISO_SW_EN);
}

static void inter_dbg_log_init(struct dbg_log_device *dbg)
{
	DEBUG_LOG_PRINT("entry\n");

	regmap_update_bits(dbg->aon_apb, REG_AON_APB_APB_EB2,
			   MASK_AON_APB_SERDES_DPHY_CFG_EB |
			   MASK_AON_APB_SERDES_DPHY_REF_EB,
			   MASK_AON_APB_SERDES_DPHY_CFG_EB |
			   MASK_AON_APB_SERDES_DPHY_REF_EB);

	clk_prepare_enable(dbg->clk_src[dbg->phy->clk_sel]);

	clk_prepare_enable(dbg->clk_serdes_eb);

	/* power on */
	regmap_update_bits(dbg->phy->pmu_apb,
			   REG_PMU_APB_PD_MM_TOP_CFG,	/* 0x001c */
			   MASK_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN,
			   ~MASK_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN/* 0x1000000 */);
	regmap_update_bits(dbg->phy->pmu_apb,
			   REG_PMU_APB_PD_MM_TOP_CFG,
			   MASK_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN,
			   ~MASK_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN/* 0x2000000 */);

	usleep_range(1000, 1100);
	dbg_phy_ps_pd_l(dbg, 1);
	dbg_phy_ps_pd_s(dbg, 1);
	dbg_phy_iso_sw_en(dbg, 1);
	dbg_phy_init(dbg->phy);
}

static void inter_dbg_log_exit(struct dbg_log_device *dbg)
{
	dbg_phy_exit(dbg->phy);
	dbg_phy_iso_sw_en(dbg, 1);
	dbg_phy_ps_pd_s(dbg, 1);
	dbg_phy_ps_pd_l(dbg, 1);

	/* power off */
	regmap_update_bits(dbg->phy->pmu_apb,
			   REG_PMU_APB_PD_MM_TOP_CFG,	/* 0x001c */
			   MASK_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN,
			   ~MASK_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN/* 0x1000000 */);
	regmap_update_bits(dbg->phy->pmu_apb,
			   REG_PMU_APB_PD_MM_TOP_CFG,
			   MASK_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN,
			   MASK_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN/* 0x2000000 */);

	clk_disable_unprepare(dbg->clk_src[dbg->phy->clk_sel]);

	clk_disable_unprepare(dbg->clk_serdes_eb);

	regmap_update_bits(dbg->aon_apb, REG_AON_APB_APB_EB2,
			   MASK_AON_APB_SERDES_DPHY_CFG_EB |
			   MASK_AON_APB_SERDES_DPHY_REF_EB,
			   (~(MASK_AON_APB_SERDES_DPHY_CFG_EB |
					    MASK_AON_APB_SERDES_DPHY_REF_EB)));
}

static void dbg_phy_ext_ctl(void *ext_para)
{
	struct dbg_log_device *dbg = ext_para;

	dbg_phy_iso_sw_en(dbg, 0);
	/* here need wait till cmd complete */
	usleep_range(1000, 1100); /* Wait for 1mS */
	dbg_phy_ps_pd_s(dbg, 0);
	/* here need wait till cmd complete */
	usleep_range(1000, 1100); /* Wait for 1mS */
	dbg_phy_ps_pd_l(dbg, 0);
	/* here need wait till cmd complete */
	usleep_range(5000, 5100); /* Wait for 5mS */
}

static void inter_dbg_log_chn_sel(struct dbg_log_device *dbg)
{
	if (dbg->channel) {
		dbg_phy_enable(dbg->phy, 1, dbg_phy_ext_ctl, dbg);
		dbg->serdes.channel = dbg->serdes.ch_map[dbg->channel - 1];
		serdes_enable(&dbg->serdes, 1);
	} else {
		serdes_enable(&dbg->serdes, 0);
		dbg_phy_enable(dbg->phy, 0, 0, 0);
	}
}

static bool inter_dbg_log_is_freq_valid(struct dbg_log_device *dbg,
					unsigned int freq)
{
	int i;
	unsigned long src_freq;

	DEBUG_LOG_PRINT("input freq %d\n", freq);
	for (i = 0; i < CLK_SRC_MAX; i++) {
		src_freq = (clk_get_rate(dbg->clk_src[i]) / 1000);
		if (MATCH_FREQ(freq, src_freq)) {
			dbg->phy->clk_sel = i;
			return true;
		}
	}
	DEBUG_LOG_PRINT("input freq %d not match\n", freq);
	return false;
}

static int inter_dbg_log_get_valid_channel(struct dbg_log_device *dbg,
					   const char *buf)
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

static struct dbg_log_ops ops = {
	.init = inter_dbg_log_init,
	.exit = inter_dbg_log_exit,
	.select = inter_dbg_log_chn_sel,
	.is_freq_valid = inter_dbg_log_is_freq_valid,
	.get_valid_channel = inter_dbg_log_get_valid_channel,
};

static int dbg_log_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *addr, *serdes_apb;
	struct dbg_log_device *dbg;
	struct regmap *aon_apb, *dsi_apb, *pll_apb, *pmu_apb;
	int count, i, rc;

	DEBUG_LOG_PRINT("entry\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr))
		return PTR_ERR(addr);

	serdes_apb = (void __iomem *)addr;

	aon_apb = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						  "sprd,syscon-aon-apb");
	if (IS_ERR(aon_apb)) {
		dev_err(&pdev->dev, "aon apb get failed!\n");
		return PTR_ERR(aon_apb);
	}

	dsi_apb = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						  "sprd,syscon-dsi-apb");
	if (IS_ERR(dsi_apb)) {
		dev_err(&pdev->dev, "dsi apb get failed!\n");
		return PTR_ERR(dsi_apb);
	}

	pll_apb = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						  "sprd,syscon-pll-apb");
	if (IS_ERR(pll_apb)) {
		dev_err(&pdev->dev, "pll apb get failed!\n");
		return PTR_ERR(pll_apb);
	}

	pmu_apb = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						"sprd,syscon-pmu-apb");
	if (IS_ERR(pmu_apb)) {
		dev_err(&pdev->dev, "pmu apb get failed!\n");
		return PTR_ERR(pmu_apb);
	}

	dbg = dbg_log_device_register(&pdev->dev, &ops, NULL, "debug-log");
	if (!dbg)
		return -ENOMEM;

	dbg->aon_apb = aon_apb;
	dbg->phy->freq = 1500000;
	dbg->phy->dsi_apb = dsi_apb;
	dbg->phy->pll_apb = pll_apb;
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
			dev_err(&pdev->dev, "get channel map failed\n");
			dbg->serdes.ch_num = 0;
		}
		for (i = 0; i < count; i++)
			DEBUG_LOG_PRINT("sel %d = 0x%x\n", i,
					dbg->serdes.ch_map[i]);

		rc = of_property_read_string_array(pdev->dev.of_node,
						   "sprd,ch-name",
						   dbg->serdes.ch_str,
						   count);
		DEBUG_LOG_PRINT("ch-name count %d, rc %d\n", count, rc);
		if (rc != count) {
			dev_err(&pdev->dev, "get channel string failed\n");
			dbg->serdes.ch_num = 0;
		}
		for (i = 0; i < count; i++)
			DEBUG_LOG_PRINT("str %d = %s\n", i,
					dbg->serdes.ch_str[i]);
	}

	dbg->clk_serdes_eb = devm_clk_get(&pdev->dev, "serdes_eb");
	if (IS_ERR(dbg->clk_serdes_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: serdes_eb\n");
		dbg->clk_serdes_eb = NULL;
	}

	for (i = 0; i < CLK_SRC_MAX; i++) {
		char src_str[8];

		snprintf(src_str, 8, "src%d", i);
		dbg->clk_src[i] = devm_clk_get(&pdev->dev, src_str);
		if (IS_ERR(dbg->clk_src[i])) {
			dev_warn(&pdev->dev,
				 "can't get the clock dts config: %s\n",
				 src_str);
			dbg->clk_src[i] = NULL;
		}
	}

	inter_dbg_log_is_freq_valid(dbg, dbg->phy->freq);

	DEBUG_LOG_PRINT("Finish mipiserdes probeFun\n");
	return 0;
}

static const struct of_device_id dt_ids[] = {
	{.compatible = "sprd,dbg-log-sharkl3",},
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
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum SoC Modem Debug Log Driver");
