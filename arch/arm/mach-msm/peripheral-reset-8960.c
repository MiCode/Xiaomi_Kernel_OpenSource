/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/elf.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>

#include <mach/msm_iomap.h>
#include <mach/scm.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

#define PPSS_RESET			(MSM_CLK_CTL_BASE + 0x2594)
#define PPSS_PROC_CLK_CTL		(MSM_CLK_CTL_BASE + 0x2588)
#define PPSS_HCLK_CTL			(MSM_CLK_CTL_BASE + 0x2580)

static int verify_blob(struct pil_desc *pil, u32 phy_addr, size_t size)
{
	return 0;
}

static int init_image_dsps_untrusted(struct pil_desc *pil, const u8 *metadata,
				     size_t size)
{
	/* Bring memory and bus interface out of reset */
	writel_relaxed(0x2, PPSS_RESET);
	writel_relaxed(0x10, PPSS_HCLK_CTL);
	return 0;
}

static int reset_dsps_untrusted(struct pil_desc *pil)
{
	writel_relaxed(0x10, PPSS_PROC_CLK_CTL);
	/* Bring DSPS out of reset */
	writel_relaxed(0x0, PPSS_RESET);
	return 0;
}

static int shutdown_dsps_untrusted(struct pil_desc *pil)
{
	writel_relaxed(0x3, PPSS_RESET);
	writel_relaxed(0x0, PPSS_PROC_CLK_CTL);
	return 0;
}

static int init_image_dsps_trusted(struct pil_desc *pil, const u8 *metadata,
				   size_t size)
{
	return pas_init_image(PAS_DSPS, metadata, size);
}

static int reset_dsps_trusted(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_DSPS);
}

static int shutdown_dsps_trusted(struct pil_desc *pil)
{
	return pas_shutdown(PAS_DSPS);
}

struct pil_reset_ops pil_dsps_ops = {
	.init_image = init_image_dsps_untrusted,
	.verify_blob = verify_blob,
	.auth_and_reset = reset_dsps_untrusted,
	.shutdown = shutdown_dsps_untrusted,
};

static struct platform_device pil_dsps = {
	.name = "pil_dsps",
};

static struct pil_desc pil_dsps_desc = {
	.name = "dsps",
	.dev = &pil_dsps.dev,
	.ops = &pil_dsps_ops,
};

static void __init use_secure_pil(void)
{
	if (pas_supported(PAS_DSPS) > 0) {
		pil_dsps_ops.init_image = init_image_dsps_trusted;
		pil_dsps_ops.auth_and_reset = reset_dsps_trusted;
		pil_dsps_ops.shutdown = shutdown_dsps_trusted;
	}
}

static int __init msm_peripheral_reset_init(void)
{
	/*
	 * Don't initialize PIL on simulated targets, as some
	 * subsystems may not be emulated on them.
	 */
	if (machine_is_msm8960_sim() || machine_is_msm8960_rumi3())
		return 0;

	use_secure_pil();

	BUG_ON(platform_device_register(&pil_dsps));
	BUG_ON(msm_pil_register(&pil_dsps_desc));

	return 0;
}
arch_initcall(msm_peripheral_reset_init);
