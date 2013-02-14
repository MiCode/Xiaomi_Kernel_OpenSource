/* Copyright (c) 2009-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <mach/qdsp5v2/aux_pcm.h>
#include <linux/delay.h>
#include <mach/debug_mm.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/* define offset of registers here, may put them into platform data */
#define AUX_CODEC_CTL_OFFSET 0x00
#define PCM_PATH_CTL_OFFSET 0x04
#define AUX_CODEC_CTL_OUT_OFFSET 0x08

/* define some bit values in PCM_PATH_CTL register */
#define PCM_PATH_CTL__ADSP_CTL_EN_BMSK 0x8

/* mask and shift */
#define AUX_CODEC_CTL_ADSP_CODEC_CTL_EN_BMSK 0x800
#define AUX_CODEC_CTL_PCM_SYNC_LONG_BMSK 0x400
#define AUX_CODEC_CTL_PCM_SYNC_SHORT_BMSK 0x200
#define AUX_CODEC_CTL_I2S_SAMPLE_CLK_SRC_BMSK 0x80
#define AUX_CODEC_CTL_I2S_SAMPLE_CLK_MODE_BMSK 0x40
#define AUX_CODEC_CTL_I2S_RX_MODE_BMSK 0x20
#define AUX_CODEC_CTL_I2S_CLK_MODE_BMSK 0x10
#define AUX_CODEC_CTL_AUX_PCM_MODE_BMSK 0x0b
#define AUX_CODEC_CTL_AUX_CODEC_MODE_BMSK 0x02

/* AUX PCM MODE */
#define MASTER_PRIM_PCM_SHORT 0
#define MASTER_AUX_PCM_LONG 1
#define SLAVE_PRIM_PCM_SHORT 2

struct aux_pcm_state {
	void __iomem *aux_pcm_base;  /* configure aux pcm through Scorpion */
	int     dout;
	int     din;
	int     syncout;
	int     clkin_a;
};

static struct aux_pcm_state the_aux_pcm_state;

static void __iomem *get_base_addr(struct aux_pcm_state *aux_pcm)
{
	return aux_pcm->aux_pcm_base;
}

/* Set who control aux pcm : adsp or MSM */
void aux_codec_adsp_codec_ctl_en(bool msm_adsp_en)
{
	void __iomem *baddr = get_base_addr(&the_aux_pcm_state);
	uint32_t val;

	if (!IS_ERR(baddr)) {
		val = readl(baddr + AUX_CODEC_CTL_OFFSET);
		if (msm_adsp_en) { /* adsp */
			writel(
			((val & ~AUX_CODEC_CTL_ADSP_CODEC_CTL_EN_BMSK) |
			AUX_CODEC_CTL__ADSP_CODEC_CTL_EN__ADSP_V),
			baddr + AUX_CODEC_CTL_OFFSET);
		} else { /* MSM */
			writel(
			((val & ~AUX_CODEC_CTL_ADSP_CODEC_CTL_EN_BMSK) |
			AUX_CODEC_CTL__ADSP_CODEC_CTL_EN__MSM_V),
			baddr + AUX_CODEC_CTL_OFFSET);
		}
	}
	mb();
}

/* Set who control aux pcm path: adsp or MSM */
void aux_codec_pcm_path_ctl_en(bool msm_adsp_en)
{
	void __iomem *baddr = get_base_addr(&the_aux_pcm_state);
	uint32_t val;

	 if (!IS_ERR(baddr)) {
		val = readl(baddr + PCM_PATH_CTL_OFFSET);
		if (msm_adsp_en) { /* adsp */
			writel(
			((val & ~PCM_PATH_CTL__ADSP_CTL_EN_BMSK) |
			PCM_PATH_CTL__ADSP_CTL_EN__ADSP_V),
			baddr + PCM_PATH_CTL_OFFSET);
		} else { /* MSM */
			writel(
			((val & ~PCM_PATH_CTL__ADSP_CTL_EN_BMSK) |
			PCM_PATH_CTL__ADSP_CTL_EN__MSM_V),
			baddr + PCM_PATH_CTL_OFFSET);
		}
	}
	mb();
	return;
}
EXPORT_SYMBOL(aux_codec_pcm_path_ctl_en);

int aux_pcm_gpios_request(void)
{
	int rc = 0;

	MM_DBG("aux_pcm_gpios_request\n");
	rc = gpio_request(the_aux_pcm_state.dout, "AUX PCM DOUT");
	if (rc) {
		MM_ERR("GPIO request for AUX PCM DOUT failed\n");
		return rc;
	}

	rc = gpio_request(the_aux_pcm_state.din, "AUX PCM DIN");
	if (rc) {
		MM_ERR("GPIO request for AUX PCM DIN failed\n");
		gpio_free(the_aux_pcm_state.dout);
		return rc;
	}

	rc = gpio_request(the_aux_pcm_state.syncout, "AUX PCM SYNC OUT");
	if (rc) {
		MM_ERR("GPIO request for AUX PCM SYNC OUT failed\n");
		gpio_free(the_aux_pcm_state.dout);
		gpio_free(the_aux_pcm_state.din);
		return rc;
	}

	rc = gpio_request(the_aux_pcm_state.clkin_a, "AUX PCM CLKIN A");
	if (rc) {
		MM_ERR("GPIO request for AUX PCM CLKIN A failed\n");
		gpio_free(the_aux_pcm_state.dout);
		gpio_free(the_aux_pcm_state.din);
		gpio_free(the_aux_pcm_state.syncout);
		return rc;
	}

	return rc;
}
EXPORT_SYMBOL(aux_pcm_gpios_request);


void aux_pcm_gpios_free(void)
{
	MM_DBG(" aux_pcm_gpios_free \n");

	/*
	 * Feed silence frames before close to prevent buzzing sound in BT at
	 * call end. This fix is applicable only to Marimba BT.
	 */
	gpio_tlmm_config(GPIO_CFG(the_aux_pcm_state.dout, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_set_value(the_aux_pcm_state.dout, 0);
	msleep(20);
	gpio_tlmm_config(GPIO_CFG(the_aux_pcm_state.dout, 1, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	gpio_free(the_aux_pcm_state.dout);
	gpio_free(the_aux_pcm_state.din);
	gpio_free(the_aux_pcm_state.syncout);
	gpio_free(the_aux_pcm_state.clkin_a);
}
EXPORT_SYMBOL(aux_pcm_gpios_free);


static int get_aux_pcm_gpios(struct platform_device *pdev)
{
	int rc = 0;
	struct resource         *res;

	/* Claim all of the GPIOs. */
	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					"aux_pcm_dout");
	if  (!res) {
		MM_ERR("%s: failed to get gpio AUX PCM DOUT\n", __func__);
		return -ENODEV;
	}

	the_aux_pcm_state.dout = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					"aux_pcm_din");
	if  (!res) {
		MM_ERR("%s: failed to get gpio AUX PCM DIN\n", __func__);
		return -ENODEV;
	}

	the_aux_pcm_state.din = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					"aux_pcm_syncout");
	if  (!res) {
		MM_ERR("%s: failed to get gpio AUX PCM SYNC OUT\n", __func__);
		return -ENODEV;
	}

	the_aux_pcm_state.syncout = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					"aux_pcm_clkin_a");
	if  (!res) {
		MM_ERR("%s: failed to get gpio AUX PCM CLKIN A\n", __func__);
		return -ENODEV;
	}

	the_aux_pcm_state.clkin_a = res->start;

	return rc;
}
static int aux_pcm_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *mem_src;

	MM_DBG("aux_pcm_probe \n");
	mem_src = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"aux_codec_reg_addr");
	if (!mem_src) {
		rc = -ENODEV;
		goto done;
	}

	the_aux_pcm_state.aux_pcm_base = ioremap(mem_src->start,
		(mem_src->end - mem_src->start) + 1);
	if (!the_aux_pcm_state.aux_pcm_base) {
		rc = -ENOMEM;
		goto done;
	}
	rc = get_aux_pcm_gpios(pdev);
	if (rc) {
		MM_ERR("GPIO configuration failed\n");
		rc = -ENODEV;
	}

done:	return rc;

}

static int aux_pcm_remove(struct platform_device *pdev)
{
	iounmap(the_aux_pcm_state.aux_pcm_base);
	return 0;
}

static struct platform_driver aux_pcm_driver = {
	.probe = aux_pcm_probe,
	.remove = aux_pcm_remove,
	.driver = {
		.name = "msm_aux_pcm",
		.owner = THIS_MODULE,
	},
};

static int __init aux_pcm_init(void)
{

	return platform_driver_register(&aux_pcm_driver);
}

static void __exit aux_pcm_exit(void)
{
	platform_driver_unregister(&aux_pcm_driver);
}

module_init(aux_pcm_init);
module_exit(aux_pcm_exit);

MODULE_DESCRIPTION("MSM AUX PCM driver");
MODULE_LICENSE("GPL v2");
