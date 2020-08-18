/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include "nandx_util.h"
#include "nandx_errno.h"
#include "nandx_info.h"
#include "nandx_platform.h"

struct nandx_clock_sets {
	struct clk *nfi_hclk;
	struct clk *nfiecc_bclk;
	struct clk *nfi_bclk;
	struct clk *onfi_sel_clk;
	struct clk *onfi_26m_clk;
	struct clk *onfi_mode5;
	struct clk *onfi_mode4;
	struct clk *nfi_bclk_sel;
	struct clk *nfi_ahb_clk;
	struct clk *nfi_ecc_pclk;
	struct clk *nfi_pclk;
	struct clk *onfi_pad_clk;
	/* for MT8167 */

	struct clk *nfi_2xclk;

	struct clk *nfi_1xclk_sel;
	struct clk *nfi_2xclk_sel;
	struct clk *nfiecc_sel;
	struct clk *nfiecc_csw_sel;
	struct clk *nfi_1xpad_clk;
	struct clk *nfi_rgecc;

	int nfi_2x_clk_num;
	struct clk *main_d4;
	struct clk *main_d5;
	struct clk *main_d6;
	struct clk *main_d7;
	struct clk *main_d8;
	struct clk *main_d10;
	struct clk *main_d12;
};

static void nandx_platform_set_gpio(void)
{

}

static int nandx_platform_get_clock_sets(struct platform_data *pdata)
{
	struct nandx_clock_sets *clk_sets;
	struct platform_device *pdev = pdata->res->dev;

	clk_sets = mem_alloc(1, sizeof(struct nandx_clock_sets));
	if (!clk_sets)
		return -ENOMEM;

	clk_sets->nfi_hclk = devm_clk_get(&pdev->dev, "nfi_hclk");
	if (IS_ERR(clk_sets->nfi_hclk)) {
		pr_debug("gert nfihclk err %ld!\n",
			 PTR_ERR(clk_sets->nfi_hclk));
		return PTR_ERR(clk_sets->nfi_hclk);
	}
	clk_sets->nfiecc_bclk = devm_clk_get(&pdev->dev, "nfiecc_bclk");
	clk_sets->nfi_bclk = devm_clk_get(&pdev->dev, "nfi_bclk");
	clk_sets->nfi_2xclk = devm_clk_get(&pdev->dev, "nfi_2xclk");
	clk_sets->nfi_1xpad_clk = devm_clk_get(&pdev->dev, "nfi_1xclk");
	clk_sets->nfi_rgecc = devm_clk_get(&pdev->dev, "nfi_rgecc");
	clk_sets->nfi_1xclk_sel = devm_clk_get(&pdev->dev, "nfi_1xpad_sel");
	clk_sets->nfi_2xclk_sel = devm_clk_get(&pdev->dev, "nfi_2xpad_sel");
	clk_sets->nfiecc_sel = devm_clk_get(&pdev->dev, "nfiecc_sel");
	clk_sets->nfiecc_csw_sel = devm_clk_get(&pdev->dev, "nfiecc_csw_sel");
	clk_sets->nfi_ahb_clk = devm_clk_get(&pdev->dev, "infra_ahb");
	clk_sets->onfi_26m_clk = devm_clk_get(&pdev->dev, "clk_26m");
	clk_sets->main_d4 = devm_clk_get(&pdev->dev, "main_d4");
	clk_sets->main_d5 = devm_clk_get(&pdev->dev, "main_d5");
	clk_sets->main_d6 = devm_clk_get(&pdev->dev, "main_d6");
	clk_sets->main_d7 = devm_clk_get(&pdev->dev, "main_d7");
	clk_sets->main_d8 = devm_clk_get(&pdev->dev, "main_d8");
	clk_sets->main_d10 = devm_clk_get(&pdev->dev, "main_d10");
	clk_sets->main_d12 = devm_clk_get(&pdev->dev, "main_d12");
	clk_sets->nfi_2x_clk_num = 7;

	pdata->clk_sets = clk_sets;
	return 0;
}

static void nandx_platform_enable_nfi_clock(struct platform_data *pdata,
					    bool high_speed_en)
{
	struct nandx_clock_sets *clock_sets;

	clock_sets = pdata->clk_sets;

	/*
	 * nfi_2xclk must be always on,
	 * otherwise system will hang if access nfi register
	 */
	clk_enable(clock_sets->nfi_2xclk);

	clk_enable(clock_sets->nfi_hclk);
	clk_enable(clock_sets->nfiecc_bclk);
	clk_enable(clock_sets->nfi_bclk);
}

static void nandx_platform_prepare_nfi_clock(struct platform_data *pdata,
					     bool high_speed_en)
{
	struct nandx_clock_sets *clock_sets;
	struct nfc_frequency *freq = &pdata->freq;
	struct clk **clks = (struct clk **)freq->nfi_clk_sets;

	clock_sets = pdata->clk_sets;
	if (high_speed_en) {
		if (freq->sel_2x_idx < 0)
			NANDX_ASSERT(0);
		clk_prepare_enable(clock_sets->nfi_2xclk_sel);
		clk_set_parent(clock_sets->nfi_2xclk_sel,
			       clks[freq->sel_2x_idx]);
		clk_disable_unprepare(clock_sets->nfi_2xclk_sel);
		clk_prepare_enable(clock_sets->nfi_1xclk_sel);
		clk_set_parent(clock_sets->nfi_1xclk_sel,
			       clock_sets->nfi_1xpad_clk);
		clk_disable_unprepare(clock_sets->nfi_1xclk_sel);
	} else {
		clk_prepare_enable(clock_sets->nfi_1xclk_sel);
		clk_set_parent(clock_sets->nfi_1xclk_sel,
			       clock_sets->nfi_ahb_clk);
		clk_disable_unprepare(clock_sets->nfi_1xclk_sel);
	}

	clk_prepare(clock_sets->nfi_2xclk);
	clk_prepare(clock_sets->nfi_hclk);
	clk_prepare(clock_sets->nfiecc_bclk);
	clk_prepare(clock_sets->nfi_bclk);
}

static void nandx_platform_disable_nfi_clock(struct platform_data *pdata,
					     bool high_speed_en)
{
	struct nandx_clock_sets *clock_sets;

	clock_sets = pdata->clk_sets;

	clk_disable(clock_sets->nfi_2xclk);

	clk_disable(clock_sets->nfi_hclk);
	clk_disable(clock_sets->nfiecc_bclk);
	clk_disable(clock_sets->nfi_bclk);
}

static void nandx_platform_unprepare_nfi_clock(struct platform_data *pdata,
					       bool high_speed_en)
{
	struct nandx_clock_sets *clock_sets;

	clock_sets = pdata->clk_sets;

	clk_unprepare(clock_sets->nfi_2xclk);

	clk_unprepare(clock_sets->nfi_hclk);
	clk_unprepare(clock_sets->nfiecc_bclk);
	clk_unprepare(clock_sets->nfi_bclk);
}

static void nandx_platform_prepare_ecc_clock(struct platform_data *pdata)
{
	struct nandx_clock_sets *clock_sets;
	struct nfc_frequency *freq = &pdata->freq;
	struct clk **clks = (struct clk **)freq->ecc_clk_sets;

	/* optimize coding here later */
	if (pdata->freq.sel_ecc_idx < 0)
		NANDX_ASSERT(0);

	clock_sets = pdata->clk_sets;

	if (pdata->freq.sel_ecc_idx == 0) {
		clk_prepare_enable(clock_sets->nfiecc_sel);
		clk_set_parent(clock_sets->nfiecc_sel, clks[0]);
		clk_disable_unprepare(clock_sets->nfiecc_sel);

		pr_debug("%s: enable ecc sel!\n", __func__);
	} else {
		clk_prepare_enable(clock_sets->nfiecc_sel);
		clk_set_parent(clock_sets->nfiecc_sel,
			       clock_sets->nfiecc_csw_sel);
		clk_disable_unprepare(clock_sets->nfiecc_sel);
		clk_prepare_enable(clock_sets->nfiecc_csw_sel);
		clk_set_parent(clock_sets->nfiecc_csw_sel,
			       clks[pdata->freq.sel_ecc_idx]);
		clk_disable_unprepare(clock_sets->nfiecc_csw_sel);
	}

	clk_prepare(clock_sets->nfi_rgecc);
}

static void nandx_platform_enable_ecc_clock(struct platform_data *pdata)
{
	struct nandx_clock_sets *clock_sets = pdata->clk_sets;

	clk_enable(clock_sets->nfi_rgecc);
}

static void nandx_platform_unprepare_ecc_clock(struct platform_data *pdata)
{
	struct nandx_clock_sets *clock_sets = pdata->clk_sets;

	clk_unprepare(clock_sets->nfi_rgecc);
}

static void nandx_platform_disable_ecc_clock(struct platform_data *pdata)
{
	struct nandx_clock_sets *clock_sets = pdata->clk_sets;

	clk_disable(clock_sets->nfi_rgecc);
}

void nandx_platform_enable_clock(struct platform_data *pdata,
				 bool high_speed_en, bool ecc_clk_en)
{
	nandx_platform_enable_nfi_clock(pdata, high_speed_en);

	if (ecc_clk_en)
		nandx_platform_enable_ecc_clock(pdata);
}

void nandx_platform_prepare_clock(struct platform_data *pdata,
				  bool high_speed_en, bool ecc_clk_en)
{
	nandx_platform_prepare_nfi_clock(pdata, high_speed_en);

	if (ecc_clk_en)
		nandx_platform_prepare_ecc_clock(pdata);
}

void nandx_platform_disable_clock(struct platform_data *pdata,
				  bool high_speed_en, bool ecc_clk_en)
{
	nandx_platform_disable_nfi_clock(pdata, high_speed_en);

	if (ecc_clk_en)
		nandx_platform_disable_ecc_clock(pdata);
}

void nandx_platform_unprepare_clock(struct platform_data *pdata,
				    bool high_speed_en, bool ecc_clk_en)
{
	nandx_platform_unprepare_nfi_clock(pdata, high_speed_en);

	if (ecc_clk_en)
		nandx_platform_unprepare_ecc_clock(pdata);
}

int nandx_platform_power_on(struct platform_data *pdata)
{
	int ret;
	struct regulator *nandx_regulator;
	struct platform_device *pdev = pdata->res->dev;

	nandx_regulator = devm_regulator_get(&pdev->dev, "vmch");
	if (IS_ERR(nandx_regulator)) {
		pr_debug("get regulator err %ld!\n",
			 PTR_ERR(nandx_regulator));
		return PTR_ERR(nandx_regulator);
	}
	ret = regulator_set_voltage(nandx_regulator, 3300000, 3300000);
	NANDX_ASSERT(ret == 0);
	ret = regulator_enable(nandx_regulator);
	NANDX_ASSERT(ret == 0);

	return 0;
}

int nandx_platform_power_down(struct platform_data *pdata)
{
	int ret;
	struct regulator *nandx_regulator;
	struct platform_device *pdev = pdata->res->dev;

	nandx_regulator = devm_regulator_get(&pdev->dev, "vmch");
	ret = regulator_disable(nandx_regulator);
	NANDX_ASSERT(ret == 0);

	return ret;
}

static void set_nfc_clk_sets(struct platform_data *pdata)
{
	struct nfc_frequency *freq;
	struct nandx_clock_sets *clk_sets;

	freq = &pdata->freq;
	clk_sets = pdata->clk_sets;

	freq->nfi_clk_num = clk_sets->nfi_2x_clk_num;
	freq->nfi_clk_sets = (void **)&clk_sets->main_d4;

	freq->ecc_clk_num = 4;
	freq->ecc_clk_sets = (void **)&clk_sets->main_d4;
}

static void nfc_freq_init(struct platform_data *pdata)
{
	int i;
	struct nfc_frequency *freq;
	struct clk **clks;

	set_nfc_clk_sets(pdata);

	freq = &pdata->freq;
	clks = (struct clk **)freq->nfi_clk_sets;
	if (!clks)
		return;

	for (i = 0; i < freq->nfi_clk_num; i++)
		freq->freq_2x[i] = (u32)clk_get_rate(clks[i]);

	clks = (struct clk **)freq->ecc_clk_sets;
	if (!clks)
		return;

	for (i = 0; i < freq->ecc_clk_num; i++)
		freq->freq_ecc[i] = (u32)clk_get_rate(clks[i]);
}

static int nfc_set_dma_mask(struct platform_data *pdata)
{
	struct platform_device *pdev = pdata->res->dev;

	return dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
}

int nandx_platform_init(struct platform_data *pdata)
{
	int ret;

	ret = nandx_platform_get_clock_sets(pdata);
	if (ret < 0)
		return ret;

	nandx_platform_set_gpio();

	ret = nandx_platform_power_on(pdata);
	if (ret < 0)
		return ret;

	nfc_freq_init(pdata);

	ret = nfc_set_dma_mask(pdata);
	if (ret) {
		pr_err("failed to set dma mask\n");
		return ret;
	}

	return 0;
}
