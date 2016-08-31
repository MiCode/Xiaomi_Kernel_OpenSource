/*
 * arch/arm/mach-tegra/kfuse.c
 *
 * Copyright (C) 2010-2011 NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* The kfuse block stores downstream and upstream HDCP keys for use by HDMI
 * module.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <mach/kfuse.h>

#include "clock.h"
#include "apbio.h"
#include "iomap.h"

static struct clk *kfuse_clk = NULL;

/* register definition */
#define KFUSE_STATE			0x80
#define  KFUSE_STATE_DONE		(1u << 16)
#define  KFUSE_STATE_CRCPASS		(1u << 17)
#define KFUSE_KEYADDR			0x88
#define  KFUSE_KEYADDR_AUTOINC		(1u << 16)
#define KFUSE_KEYS			0x8c

static inline u32 tegra_kfuse_readl(unsigned long offset)
{
	return tegra_apb_readl(TEGRA_KFUSE_BASE + offset);
}

static inline void tegra_kfuse_writel(u32 value, unsigned long offset)
{
	tegra_apb_writel(value, TEGRA_KFUSE_BASE + offset);
}

static int wait_for_done(void)
{
	u32 reg;
	int retries = 50;
	do {
		reg = tegra_kfuse_readl(KFUSE_STATE);
		if (reg & KFUSE_STATE_DONE)
			return 0;
		msleep(10);
	} while(--retries);
	return -ETIMEDOUT;
}

/* read up to KFUSE_DATA_SZ bytes into dest.
 * always starts at the first kfuse.
 */
int tegra_kfuse_read(void *dest, size_t len)
{
	int err;
	u32 v;
	unsigned cnt;

	if (len > KFUSE_DATA_SZ)
		return -EINVAL;

	if (kfuse_clk == NULL) {
		kfuse_clk = tegra_get_clock_by_name("kfuse");
		if (IS_ERR_OR_NULL(kfuse_clk)) {
			pr_err("kfuse: can't get kfuse clock\n");
			return -EINVAL;
		}
	}

	err = clk_prepare_enable(kfuse_clk);
	if (err)
		return err;

	tegra_kfuse_writel(KFUSE_KEYADDR_AUTOINC, KFUSE_KEYADDR);

	err = wait_for_done();
	if (err) {
		pr_err("kfuse: read timeout\n");
		clk_disable_unprepare(kfuse_clk);
		return err;
	}

	if ((tegra_kfuse_readl(KFUSE_STATE) & KFUSE_STATE_CRCPASS) == 0) {
		pr_err("kfuse: crc failed\n");
		clk_disable_unprepare(kfuse_clk);
		return -EIO;
	}

	for (cnt = 0; cnt < len; cnt += 4) {
		v = tegra_kfuse_readl(KFUSE_KEYS);
		memcpy(dest + cnt, &v, sizeof v);
	}

	clk_disable_unprepare(kfuse_clk);

	return 0;
}
