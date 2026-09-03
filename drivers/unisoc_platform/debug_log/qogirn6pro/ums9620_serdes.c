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
#include "../core.h"

static u32 s_freq_array[] = {
	500000,
	1500000,
	2000000,
	2500000,
	2600000
};

static char serdes_name[50];

static void dbg_phy_test_write(struct regmap *base, u8 addr, u8 data)
{
	regmap_write_bits(base, 0xa0, 0xffffffff, ((addr) << 11) | (0x1 << 2));
	regmap_write_bits(base, 0xa0, 0xffffffff, ((addr) << 11) | (0x1 << 2) | (0x1 << 1));
	regmap_write_bits(base, 0xa0, 0xffffffff, ((addr) << 11) | (0x1 << 2));
	regmap_write_bits(base, 0xa0, 0xffffffff, ((data) << 11));
	regmap_write_bits(base, 0xa0, 0xffffffff, ((data) << 11) | (0x1 << 1));
	regmap_write_bits(base, 0xa0, 0xffffffff, ((data) << 11));
}
static void dbg_phy_test_write_500M(struct dbg_log_device *dbg)
{
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x3, 0x3);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x4, 0xc8);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x6, 0x4c);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x7, 0x13);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x8, 0x6f);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x9, 0xec);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xa, 0x4e);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xb, 0xc4);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xc, 0x2);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xd, 0x8a);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xe, 0x0);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xf, 0x81);
}

static void dbg_phy_test_write_1500M(struct dbg_log_device *dbg)
{
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x3, 0x7);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x4, 0xc8);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x6, 0x39);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x7, 0x4);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x8, 0xde);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x9, 0xb1);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xa, 0x3b);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xb, 0x11);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xc, 0x2);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xd, 0x8a);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xe, 0x0);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xf, 0x81);
}

static void dbg_phy_test_write_2G(struct dbg_log_device *dbg)
{
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x3, 0x3);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x4, 0xc8);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x6, 0x4c);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x7, 0x4);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x8, 0xdf);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x9, 0xec);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xa, 0x4e);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xb, 0xc1);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xc, 0x2);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xd, 0x8a);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xe, 0x0);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xf, 0x81);
}

static void dbg_phy_test_write_2500M(struct dbg_log_device *dbg)
{
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x3, 0x3);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x4, 0xc8);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x6, 0x60);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x7, 0x4);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x8, 0xdf);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x9, 0x27);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xa, 0x62);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xb, 0x71);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xc, 0x2);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xd, 0x8a);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xe, 0x0);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xf, 0x81);
}

static void dbg_phy_test_write_2600M(struct dbg_log_device *dbg)
{
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x3, 0x3);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x4, 0xc8);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x6, 0x64);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x7, 0x4);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x8, 0xdf);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0x9, 0x0);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xa, 0x0);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xb, 0x1);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xc, 0x2);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xd, 0x8a);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xe, 0x0);
	dbg_phy_test_write(dbg->phy->dsi_apb, 0xf, 0x81);
}

static void serdes_i_init(struct dbg_log_device *dbg)
{
	int err = 0;
	DEBUG_LOG_PRINT("***log: serdes_i_init.\n");

	/* enable serdes DSI_CFG_EB */
	err = clk_prepare_enable(dbg->clk_dsi_cfg_eb);
	if (err) {
		DEBUG_LOG_PRINT("Can't enable clk_dsi_cfg_eb: %d\n", err);
	}

	/* enable ana eb */
	err = clk_prepare_enable(dbg->clk_ana_eb);
	if (err) {
		DEBUG_LOG_PRINT("Can't enable clk_ana_eb: %d\n", err);
	}

	/* enable serdes0 */
	err = clk_prepare_enable(dbg->clk_serdes_eb);
	if (err) {
		DEBUG_LOG_PRINT("Can't enable clk_serdes_eb: %d\n", err);
	}

	/* enable serdes1 */
	err = clk_prepare_enable(dbg->clk_serdes1_eb);
	if (err) {
		DEBUG_LOG_PRINT("Can't enable clk_serdes1_eb: %d\n", err);
	}

	/* enable cgm_cphy_cfg_en */
	err = clk_prepare_enable(dbg->clk_cgm_cphy_cfg_en);
	if (err) {
		DEBUG_LOG_PRINT("Can't enable clk_cgm_cphy_cfg_en: %d\n", err);
	}

	regmap_update_bits(dbg->phy->pmu_apb, 0x564, 0x1, 0x1);
	regmap_update_bits(dbg->phy->pmu_apb, 0x564, 0x4, 0x4);

	regmap_update_bits(dbg->phy->dsi_apb, 0x5c, 0x20, ~0x20);
	regmap_update_bits(dbg->phy->dsi_apb, 0x5c, 0x60, ~0x60);
	regmap_update_bits(dbg->phy->dsi_apb, 0xa8, 0x2, ~0x2);
	regmap_update_bits(dbg->phy->dsi_apb, 0xac, 0xff3f05, 0xff3f05);

	regmap_update_bits(dbg->phy->dsi_apb, 0x58, 0x1, 0x1);
	usleep_range(1000, 1100);
	regmap_update_bits(dbg->phy->dsi_apb, 0x58, 0x1, ~0x1);
	regmap_update_bits(dbg->phy->dsi_apb, 0x6c, 0x10000, 0x10000);
	usleep_range(1000, 1100);
	regmap_update_bits(dbg->phy->dsi_apb, 0x6c, 0x10000, ~0x10000);
	regmap_update_bits(dbg->phy->dsi_apb, 0xa8, 0x1, 0x1);
	usleep_range(1000, 1100);
	regmap_update_bits(dbg->phy->dsi_apb, 0xa8, 0x1, ~0x1);
	regmap_update_bits(dbg->phy->dsi_apb, 0x58, 0xc0, 0xc0);

	regmap_update_bits(dbg->phy->dsi_apb, 0x68, 0x60, 0x60);
	regmap_update_bits(dbg->phy->dsi_apb, 0x9c, 0x6000, 0x6000);
	regmap_update_bits(dbg->phy->dsi_apb, 0x9c, 0x2000, ~0x2000);
	usleep_range(1000, 1100);
	regmap_update_bits(dbg->phy->dsi_apb, 0x9c, 0x2000, 0x2000);
	regmap_update_bits(dbg->phy->dsi_apb, 0x9c, 0x1f00, 0x1f00);
	usleep_range(1000, 1100); /* Wait for 1mS */

	dbg_phy_test_write(dbg->phy->dsi_apb, 0x25, 0x7);
	/* freq choice */
	switch (dbg->phy->freq) {
	case 500000:
		dbg_phy_test_write_500M(dbg);
		break;
	case 1500000:
		dbg_phy_test_write_1500M(dbg);
		break;
	case 2500000:
		dbg_phy_test_write_2500M(dbg);
		break;
	case 2600000:
		dbg_phy_test_write_2600M(dbg);
		break;
	case 2000000:
	default:
		dbg_phy_test_write_2G(dbg);
	}

	/* 1 ms */
	usleep_range(1000, 1100); /* Wait for 1mS */
}

static void inter_dbg_log_init(struct dbg_log_device *dbg)
{
	serdes_i_init(dbg);
}

static void inter_dbg_log_exit(struct dbg_log_device *dbg)
{
	/* disable serdes eb */
	clk_disable_unprepare(dbg->clk_serdes_eb);

	/* disable serdes DPHY_CFG_EB & DPHY_REF_EB */
	clk_disable_unprepare(dbg->clk_dsi_cfg_eb);

	/* clk_cgm_cphy_cfg_en */
	clk_disable_unprepare(dbg->clk_cgm_cphy_cfg_en);
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

static bool inter_dbg_log_is_freq_valid(struct dbg_log_device *dbg,
					unsigned int freq)
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
	struct regmap *dsi_apb, *pmu_apb;
	int count, i, rc;
	int serdes_id;

	DEBUG_LOG_PRINT("%s\n", __func__);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(addr)) {
		return PTR_ERR(addr);
	}

	serdes_apb = (void __iomem *)addr;

	dsi_apb = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						  "sprd,syscon-dsi-apb");
	if (IS_ERR_OR_NULL(dsi_apb)) {
		pr_err("dsi apb get failed!\n");
		return PTR_ERR(dsi_apb);
	}

	pmu_apb = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, "sprd,syscon-pmu-apb");
	if (IS_ERR_OR_NULL(pmu_apb)) {
		pr_err("pmu apb get failed!\n");
		return PTR_ERR(pmu_apb);
	}

	serdes_id = of_alias_get_id(pdev->dev.of_node, "serdes");
	if (serdes_id < 0) {
		pr_err("of_alias_get_id (%d) failed!\n", serdes_id);
		return serdes_id;
	}
	DEBUG_LOG_PRINT("serdes_id = %d\n", serdes_id);

	sprintf(serdes_name, "serdes%d", serdes_id);
	dbg = dbg_log_device_register(&pdev->dev, &ops, NULL, serdes_name);

	if (!dbg)
		return -ENOMEM;

	dbg->serdes_id = serdes_id;
	dbg->phy->freq = 2500000;
	dbg->phy->dsi_apb = dsi_apb;
	dbg->phy->pmu_apb = pmu_apb;
	dbg->serdes.base = serdes_apb;
	dbg->serdes.cut_off = 0xFFE0;

	if (of_property_read_bool(pdev->dev.of_node, "sprd,dcfix")) {
		DEBUG_LOG_PRINT("dcfix enable\n");
		dbg->serdes.dc_blnc_fix = 3;
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
			DEBUG_LOG_PRINT("sel %d = 0x%x\n",
					i,
					dbg->serdes.ch_map[i]);

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
			DEBUG_LOG_PRINT("str %d = %s\n",
					i,
					dbg->serdes.ch_str[i]);
	}

	dbg->clk_serdes_eb = devm_clk_get(&pdev->dev, "serdes0_eb");
	if (IS_ERR_OR_NULL(dbg->clk_serdes_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: serdes0_eb\n");
		dbg->clk_serdes_eb = NULL;
	}

	dbg->clk_serdes1_eb = devm_clk_get(&pdev->dev, "serdes1_eb");
	if (IS_ERR_OR_NULL(dbg->clk_serdes1_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: serdes1_eb\n");
		dbg->clk_serdes1_eb = NULL;
	}

	dbg->clk_ana_eb = devm_clk_get(&pdev->dev, "ana_eb");
	if (IS_ERR_OR_NULL(dbg->clk_ana_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: ana_eb\n");
		dbg->clk_ana_eb = NULL;
	}

	dbg->clk_dsi_cfg_eb = devm_clk_get(&pdev->dev, "dsi_cfg_eb");
	if (IS_ERR_OR_NULL(dbg->clk_dsi_cfg_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: dsi_cfg_eb\n");
		dbg->clk_dsi_cfg_eb = NULL;
	}
	dbg->clk_cgm_cphy_cfg_en = devm_clk_get(&pdev->dev, "cphy_cfg_en");
	if (IS_ERR_OR_NULL(dbg->clk_cgm_cphy_cfg_en)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: cphy_cfg_en\n");
		dbg->clk_cgm_cphy_cfg_en = NULL;
	}

	inter_dbg_log_is_freq_valid(dbg, dbg->phy->freq);

	return 0;
}

static const struct of_device_id dt_ids[] = {
	{.compatible = "sprd,dbg-log-qogirn6pro",},
	{.compatible = "sprd,dbg-log-qogirn6lite",},
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
