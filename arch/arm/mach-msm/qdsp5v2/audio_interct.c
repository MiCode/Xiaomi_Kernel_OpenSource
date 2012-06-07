/* Copyright (c) 2009, 2011 Code Aurora Forum. All rights reserved.
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
#include <linux/platform_device.h>
#include <asm/io.h>
#include <mach/qdsp5v2/audio_interct.h>

#define AUDIO_INTERCT_ADSPLPA_WBRX_SEL_BMSK 0x4
#define AUDIO_INTERCT_ADSPLPA_WBRX_SEL_SHFT 0x2
#define AUDIO_INTERCT_ADSPAV_RPCMI2SRX_SEL_BMSK 0x10
#define AUDIO_INTERCT_ADSPAV_RPCMI2SRX_SEL_SHFT 0x4
#define AUDIO_INTERCT_ADSPAV_TPCMI2STX_SEL_BMSK 0x40
#define AUDIO_INTERCT_ADSPAV_TPCMI2STX_SEL_SHFT 0x6
#define AUDIO_INTERCT_ADSPAV_AUX_REGSEL_BMSK 0x100
#define AUDIO_INTERCT_ADSPAV_AUX_REGSEL_SHFT 0x8

/* Should look to protect this register */
void __iomem *aictl_reg;

void audio_interct_codec(u32 source)
{
	u32 reg_val;

	reg_val = readl(aictl_reg);
	reg_val = (reg_val & ~AUDIO_INTERCT_ADSPLPA_WBRX_SEL_BMSK) |
		(source << AUDIO_INTERCT_ADSPLPA_WBRX_SEL_SHFT);
	writel(reg_val, aictl_reg);
	mb();
}
EXPORT_SYMBOL(audio_interct_codec);

void audio_interct_aux_regsel(u32 source)
{
	u32 reg_val;

	reg_val = readl(aictl_reg);
	reg_val = (reg_val & ~AUDIO_INTERCT_ADSPAV_AUX_REGSEL_BMSK) |
		(source << AUDIO_INTERCT_ADSPAV_AUX_REGSEL_SHFT);
	writel(reg_val, aictl_reg);
	mb();
}
EXPORT_SYMBOL(audio_interct_aux_regsel);

void audio_interct_tpcm_source(u32 source)
{
	u32 reg_val;

	reg_val = readl(aictl_reg);
	reg_val = (reg_val & ~AUDIO_INTERCT_ADSPAV_TPCMI2STX_SEL_BMSK) |
		(source << AUDIO_INTERCT_ADSPAV_TPCMI2STX_SEL_SHFT);
	writel(reg_val, aictl_reg);
	mb();
}
EXPORT_SYMBOL(audio_interct_tpcm_source);

void audio_interct_rpcm_source(u32 source)
{
	u32 reg_val;

	reg_val = readl(aictl_reg);
	reg_val = (reg_val & ~AUDIO_INTERCT_ADSPAV_RPCMI2SRX_SEL_BMSK) |
		(source << AUDIO_INTERCT_ADSPAV_RPCMI2SRX_SEL_SHFT);
	writel(reg_val, aictl_reg);
	mb();
}
EXPORT_SYMBOL(audio_interct_rpcm_source);

static int audio_interct_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *aictl_mem;

	aictl_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!aictl_mem) {
		rc = -ENODEV;
		goto error;
	}
	aictl_reg = ioremap(aictl_mem->start,
			(aictl_mem->end - aictl_mem->start) + 1);
error:
	return rc;
}


static int audio_interct_remove(struct platform_device *pdev)
{
	iounmap(aictl_reg);
	return 0;
}

static struct platform_driver audio_interct_driver = {
	.probe = audio_interct_probe,
	.remove = audio_interct_remove,
	.driver = {
		.name = "audio_interct",
		.owner = THIS_MODULE,
	},
};

static int __init audio_interct_init(void)
{
	return platform_driver_register(&audio_interct_driver);
}

static void __exit audio_interct_exit(void)
{
	platform_driver_unregister(&audio_interct_driver);
}

module_init(audio_interct_init);
module_exit(audio_interct_exit);

MODULE_DESCRIPTION("MSM Audio Interconnect driver");
MODULE_LICENSE("GPL v2");
