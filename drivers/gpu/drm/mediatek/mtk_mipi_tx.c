/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>

#include "mtk_mipi_tx.h"

static void mtk_mipi_tx_mask(struct mtk_mipi_tx *mipi_tx, u32 reg_idx, u32 mask,
			     u32 data)
{
	u32 temp = MIPITX_READ(mipi_tx, reg_idx);

	MIPITX_WRITE(mipi_tx, reg_idx, (temp & ~mask) | (data & mask));
}

int mtk_mipi_tx_set_data_rate(struct phy *phy, unsigned int data_rate, unsigned char ssc_data)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);

	if (data_rate < 50 || data_rate > 1250)
		return -EINVAL;

	mipi_tx->data_rate = data_rate;
	mipi_tx->ssc_data = ssc_data;

	return 0;
}

static int mtk_mipi_tx_power_on(struct phy *phy)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);
	u8 txdiv, txdiv0, txdiv1;
	u64 pcw, delta;
	u8 delta1 = 5;	/* delta1 is ssc default range*/
	unsigned int pdelta1;

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_TOP_CON,
			 RG_DSI_LNT_IMP_CAL_CODE | RG_DSI_LNT_HS_BIAS_EN,
			 (8 << 4) | RG_DSI_LNT_HS_BIAS_EN);

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_BG_CON,
			 RG_DSI_VOUT_MSK | RG_DSI_BG_CKEN | RG_DSI_BG_CORE_EN,
			 (4 << 20) | (4 << 17) | (4 << 14) |
			 (4 << 11) | (4 << 8) | (4 << 5) |
			 RG_DSI_BG_CKEN | RG_DSI_BG_CORE_EN);

	usleep_range(30, 100);

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_CON,
			 RG_DSI_CKG_LDOOUT_EN | RG_DSI_LDOCORE_EN,
			 RG_DSI_CKG_LDOOUT_EN | RG_DSI_LDOCORE_EN);

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_PWR,
			 RG_DSI_MPPLL_SDM_PWR_ON | RG_DSI_MPPLL_SDM_ISO_EN,
			 RG_DSI_MPPLL_SDM_PWR_ON);

	/**
	 * data_rate = (pixel_clock / 1000) * pixel_dipth * mipi_ratio;
	 * pixel_clock unit is Khz, data_rata unit is MHz, so need divide 1000.
	 * mipi_ratio is mipi clk coefficient for balance the pixel clk in mipi.
	 * we set mipi_ratio is 1.05.
	 */

	if (mipi_tx->data_rate > 1250) {
		return -EINVAL;
	} else if (mipi_tx->data_rate >= 500) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (mipi_tx->data_rate >= 250) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (mipi_tx->data_rate >= 125) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (mipi_tx->data_rate > 62) {
		txdiv = 8;
		txdiv0 = 2;
		txdiv1 = 1;
	} else if (mipi_tx->data_rate >= 50) {
		txdiv = 16;
		txdiv0 = 2;
		txdiv1 = 2;
	} else {
		return -EINVAL;
	}

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_CON0,
			 RG_DSI_MPPLL_PREDIV | RG_DSI_MPPLL_TXDIV0 |
			 RG_DSI_MPPLL_TXDIV1 | RG_DSI_MPPLL_POSDIV,
			 (0 << 1) | (txdiv0 << 3) | (txdiv1 << 5) | (0 << 7));

	/*
	 * PLL PCW config
	 * PCW bit 24~30 = integer part of pcw
	 * PCW bit 0~23 = fractional part of pcw
	 * pcw = data_Rate*4*txdiv/(Ref_clk*2);
	 * Post DIV =4, so need data_Rate*4
	 * Ref_clk is 26MHz
	 */

	pcw = ((u64) mipi_tx->data_rate * txdiv) << 24;
	do_div(pcw, 13);
	MIPITX_WRITE(mipi_tx, TX_DSI_PLL_CON2, pcw);

	/* spread spectrum clock config
	* pmod = ROUND(1000*26MHz/fmod/2);fmod default is 30Khz, and this value not be changed
	* pmod = 433.33;
	*/

	if (0 != mipi_tx->ssc_data) {
		mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_CON1, RG_DSI_MPPLL_SDM_SSC_PH_INIT,
				 RG_DSI_MPPLL_SDM_SSC_PH_INIT);
		mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_CON1, RG_DSI_MPPLL_SDM_SSC_PRD,
				 0x1B1 << 16);

		delta1 = mipi_tx->ssc_data;
		delta = (u64)mipi_tx->data_rate * delta1 * txdiv * 262144 + 281664;
		do_div(delta, 563329);
		pdelta1 = delta & RG_DSI_MPPLL_SDM_SSC_DELTA1;

		mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_CON3, RG_DSI_MPPLL_SDM_SSC_DELTA,
				 pdelta1 << 16);
		mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_CON3, RG_DSI_MPPLL_SDM_SSC_DELTA1,
				 pdelta1);
	}

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_CON1,
			 RG_DSI_MPPLL_SDM_FRA_EN, RG_DSI_MPPLL_SDM_FRA_EN);

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_CLOCK_LANE,
			 RG_DSI_LNTC_LDOOUT_EN, RG_DSI_LNTC_LDOOUT_EN);
	mtk_mipi_tx_mask(mipi_tx, TX_DSI_DATA_LANE0,
			 RG_DSI_LNT0_LDOOUT_EN, RG_DSI_LNT0_LDOOUT_EN);
	mtk_mipi_tx_mask(mipi_tx, TX_DSI_DATA_LANE1,
			 RG_DSI_LNT1_LDOOUT_EN, RG_DSI_LNT1_LDOOUT_EN);
	mtk_mipi_tx_mask(mipi_tx, TX_DSI_DATA_LANE2,
			 RG_DSI_LNT2_LDOOUT_EN, RG_DSI_LNT2_LDOOUT_EN);
	mtk_mipi_tx_mask(mipi_tx, TX_DSI_DATA_LANE3,
			 RG_DSI_LNT3_LDOOUT_EN, RG_DSI_LNT3_LDOOUT_EN);

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_CON0,
			 RG_DSI_MPPLL_PLL_EN, RG_DSI_MPPLL_PLL_EN);

	usleep_range(20, 100);

	if (0 != mipi_tx->data_rate && 0 != mipi_tx->ssc_data)
		mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_CON1, RG_DSI_MPPLL_SDM_SSC_EN,
				 RG_DSI_MPPLL_SDM_SSC_EN);
	else
		mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_CON1, RG_DSI_MPPLL_SDM_SSC_EN,
				(0 << 2));

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_TOP, RG_DSI_MPPLL_PRESERVE,
			 mipi_tx->driver_data->reg_value[TX_DSI_PLL_TOP]);
	mtk_mipi_tx_mask(mipi_tx, TX_DSI_TOP_CON, RG_DSI_PAD_TIE_LOW_EN, (0 << 11));

	return 0;
}

static int mtk_mipi_tx_power_off(struct phy *phy)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_CON0, RG_DSI_MPPLL_PLL_EN, 0);

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_TOP, RG_DSI_MPPLL_PRESERVE,
			 (0 << 8));

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_TOP_CON, RG_DSI_PAD_TIE_LOW_EN,
			RG_DSI_PAD_TIE_LOW_EN);

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_CLOCK_LANE,
			 RG_DSI_LNTC_LDOOUT_EN, 0);
	mtk_mipi_tx_mask(mipi_tx, TX_DSI_DATA_LANE0,
			 RG_DSI_LNT0_LDOOUT_EN, 0);
	mtk_mipi_tx_mask(mipi_tx, TX_DSI_DATA_LANE1,
			 RG_DSI_LNT1_LDOOUT_EN, 0);
	mtk_mipi_tx_mask(mipi_tx, TX_DSI_DATA_LANE2,
			 RG_DSI_LNT2_LDOOUT_EN, 0);
	mtk_mipi_tx_mask(mipi_tx, TX_DSI_DATA_LANE3,
			 RG_DSI_LNT3_LDOOUT_EN, 0);

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_PWR,
			RG_DSI_MPPLL_SDM_ISO_EN | RG_DSI_MPPLL_SDM_PWR_ON,
			RG_DSI_MPPLL_SDM_ISO_EN);

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_TOP_CON, RG_DSI_LNT_HS_BIAS_EN, 0);

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_CON,
			RG_DSI_CKG_LDOOUT_EN | RG_DSI_LDOCORE_EN, 0);

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_BG_CON,
			RG_DSI_BG_CKEN | RG_DSI_BG_CORE_EN, 0);

	mtk_mipi_tx_mask(mipi_tx, TX_DSI_PLL_CON0, RG_DSI_MPPLL_DIV_MSK, 0);

	MIPITX_WRITE(mipi_tx, TX_DSI_PLL_CON1, 0x00000000);
	MIPITX_WRITE(mipi_tx, TX_DSI_PLL_CON2, 0x50000000);

	return 0;
}

static struct phy_ops mtk_mipi_tx_ops = {
	.power_on = mtk_mipi_tx_power_on,
	.power_off = mtk_mipi_tx_power_off,
	.owner = THIS_MODULE,
};

static int mtk_mipi_tx_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_mipi_tx *mipi_tx;
	const struct of_device_id *of_id;
	struct resource *mem;
	struct phy *phy;
	struct phy_provider *phy_provider;
	int ret;

	mipi_tx = devm_kzalloc(dev, sizeof(*mipi_tx), GFP_KERNEL);
	if (!mipi_tx)
		return -ENOMEM;

	of_id = of_match_device(mtk_mipi_tx_match, &pdev->dev);
	mipi_tx->driver_data = (struct mtk_mipitx_driver_data *)of_id->data;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mipi_tx->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(mipi_tx->regs)) {
		ret = PTR_ERR(mipi_tx->regs);
		dev_err(dev, "Failed to get memory resource: %d\n", ret);
		return ret;
	}

	phy = devm_phy_create(dev, NULL, &mtk_mipi_tx_ops);
	if (IS_ERR(phy)) {
		dev_err(dev, "Failed to create MIPI D-PHY\n");
		return PTR_ERR(phy);
	}
	phy_set_drvdata(phy, mipi_tx);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(phy_provider);
}

static int mtk_mipi_tx_remove(struct platform_device *pdev)
{
	return 0;
}

struct platform_driver mtk_mipi_tx_driver = {
	.probe = mtk_mipi_tx_probe,
	.remove = mtk_mipi_tx_remove,
	.driver = {
		.name = "mediatek-mipi-tx",
		.of_match_table = mtk_mipi_tx_match,
	},
};
