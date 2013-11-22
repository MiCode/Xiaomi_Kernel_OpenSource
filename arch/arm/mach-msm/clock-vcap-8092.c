/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"VCAP_CLK, %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/iopoll.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clock-generic.h>

#include "clock-vcap-8092.h"

#define VCAP_MISC_REG_BASE	0xfdf81000
#define VCAP_MISC_REG_SIZE	0x1000

#define VCAP_GDSC_PHYS		0xfd8c1800
#define VCAP_GDSC_SIZE		8

#define VCAP_MISC_AAFE_CTRL_5	0x0018
#define VCAP_MISC_SPARE_RW	0x0350

/* lock to serialize access to VCAP misc registers */
spinlock_t vcap_mis_reg_lock;

static unsigned char *vcap_mis_reg_base;
static unsigned char *vcap_gdsc_base;
static struct clk *vcap_ahb_clk;
static struct clk *vcap_vp_clk;

/*
 * The following three register access functions are also used
 * by VCAP driver:
 *    vcap_misc_reg_read
 *    vcap_misc_reg_write
 *    vcap_misc_reg_w_bits
 */
int vcap_misc_reg_read(u32 offset, u32 *rd_val)
{
	if (!vcap_mis_reg_base)
		return -ENODEV;

	if ((offset & 3) || (offset >= VCAP_MISC_REG_SIZE))
		return -EINVAL;

	*rd_val = readl_relaxed(vcap_mis_reg_base + offset);

	return 0;
}
EXPORT_SYMBOL(vcap_misc_reg_read);

int vcap_misc_reg_write(u32 offset, u32 wr_val)
{
	if (!vcap_mis_reg_base)
		return -ENODEV;

	if ((offset & 3) || (offset >= VCAP_MISC_REG_SIZE))
		return -EINVAL;

	writel_relaxed(wr_val, vcap_mis_reg_base + offset);

	return 0;
}
EXPORT_SYMBOL(vcap_misc_reg_write);

int vcap_misc_reg_w_bits(u32 offset, u32 bits, u32 mask)
{
	unsigned long flags = 0;
	u32 temp = 0;

	if (!vcap_mis_reg_base)
		return -ENODEV;

	if ((offset & 3) || (offset >= VCAP_MISC_REG_SIZE))
		return -EINVAL;

	spin_lock_irqsave(&vcap_mis_reg_lock, flags);
	temp = readl_relaxed(vcap_mis_reg_base + offset);
	temp &= ~mask;
	temp |= (bits & mask);
	writel_relaxed(temp, vcap_mis_reg_base + offset);
	spin_unlock_irqrestore(&vcap_mis_reg_lock, flags);

	return 0;
}
EXPORT_SYMBOL(vcap_misc_reg_w_bits);

static int afe_pixel_clk_enable(struct clk *clk)
{
	vcap_misc_reg_w_bits(VCAP_MISC_AAFE_CTRL_5, 1, 1);

	return 0;
}

static void afe_pixel_clk_disable(struct clk *clk)
{
	vcap_misc_reg_w_bits(VCAP_MISC_AAFE_CTRL_5, 0, 1);
}

static struct clk_ops afe_pixel_clk_ops = {
	.enable = afe_pixel_clk_enable,
	.disable = afe_pixel_clk_disable,
};

static unsigned long hdmirx_tmds_clk_get_rate(struct clk *clk)
{
	u32 temp, rc;
	u32 rate = 0;

	/*
	 * before this fucntion is triggered, VCAP driver should
	 * write a magic value into this spare register. This value
	 * will be the CTS received by HDMI when audio is available,
	 * and a pre defined value when HDMI audio is not present.
	 * This value will be used if set_rate has to be called for
	 * the two clocks derived from TMDS: hdmi_rx_clk and hdmi_bus_clk
	 *
	 * The N value from ACR is used for set_rate on aud_clk
	 */
	rc = vcap_misc_reg_read(VCAP_MISC_SPARE_RW, &temp);
	if (!rc)
		rate = temp & 0x3fffffff;
	else
		pr_warn("read CTS from spare register fails\n");

	return rate;
}

static struct clk_ops clk_ops_tmds_clk = {
	.get_rate = hdmirx_tmds_clk_get_rate,
};

struct afe_pixel_clk vcap_afe_pixel_clk_src = {
	.c = {
		.dbg_name = "vcap_afe_pixel_clk_src",
		.ops = &afe_pixel_clk_ops,
		CLK_INIT(vcap_afe_pixel_clk_src.c),
	}
};

struct hdmirx_tmds_clk vcap_tmds_clk_src = {
	.c = {
		.dbg_name = "vcap_tmds_clk",
		.ops = &clk_ops_tmds_clk,
		CLK_INIT(vcap_tmds_clk_src.c),
	},
};

/* to be called by 8092_clock_pre_init */
void vcap_clk_ctrl_pre_init(struct clk *ahb_clk, struct clk *vp_clk)
{
	BUG_ON(ahb_clk == NULL || vp_clk == NULL);

	vcap_ahb_clk = ahb_clk;
	vcap_vp_clk = vp_clk;

	spin_lock_init(&vcap_mis_reg_lock);

	vcap_gdsc_base = ioremap_nocache(VCAP_GDSC_PHYS, VCAP_GDSC_SIZE);
	if (!vcap_gdsc_base)
		pr_err("%s: fail to map vcap gdsc\n",  __func__);

	vcap_mis_reg_base =
		ioremap_nocache(VCAP_MISC_REG_BASE, VCAP_MISC_REG_SIZE);
	if (!vcap_mis_reg_base) {
		pr_err("%s: fail to map vcap misc reg\n",  __func__);
		iounmap(vcap_gdsc_base);
	}
}
