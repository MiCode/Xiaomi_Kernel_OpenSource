/*
 * arch/arm/mach-tegra/common-t2.c
 *
 * Tegra 2 SoC-specific initialization (memory controller, etc.)
 *
 * Copyright (c) 2009-2012 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

#include <mach/iomap.h>
#include <mach/irqs.h>

#define MC_INT_STATUS			0x0
#define MC_INT_MASK			0x4
#define MC_INT_DECERR_EMEM_OTHERS	(1<<6)
#define MC_INT_INVALID_GART_PAGE	(1<<7)
#define MC_INT_SECURITY_VIOLATION	(1<<8)

#define MC_GART_ERROR_STATUS		0x30
#define MC_GART_ERROR_ADDRESS		0x34

#define MC_DECERR_EMEM_OTHERS_STATUS	0x58
#define MC_DECERR_EMEM_OTHERS_ADDRESS	0x5c

#define MC_SECURITY_VIOLATION_STATUS	0x74
#define MC_SECURITY_VIOLATION_ADDRESS	0x78

struct mc_client {
	bool write;
	const char *name;
};

#define client(_name,_write)			\
	{					\
		.write = _write,		\
		.name = _name,			\
	}

static const struct mc_client mc_clients[] = {
	client("display0_wina", false), client("display1_wina", false),
	client("display0_winb", false), client("display1_winb", false),
	client("display0_winc", false), client("display1_winc", false),
	client("display0_winb_vfilter", false),
	client("display1_winb_vfilter", false),
	client("epp", false), client("gr2d_pat", false),
	client("gr2d_src", false), client("mpe_unified", false),
	client("vi_chroma_filter", false), client("cop", false),
	client("display0_cursor", false), client("display1_cursor", false),
	client("gr3d_fdc", false), client("gr2d_dst", false),
	client("host1x_dma", false), client("host1x_generic", false),
	client("gr3d_idx", false), client("cpu_uncached", false),
	client("mpe_intrapred", false), client("mpe_mpea", false),
	client("mpe_mpec", false), client("ahb_dma", false),
	client("ahb_slave", false), client("gr3d_tex", false),
	client("vde_bsev", false), client("vde_mbe", false),
	client("vde_mce", false), client("vde_tpe", false),
	client("epp_u", true), client("epp_v", true),
	client("epp_y", true), client("mpe_unified", true),
	client("vi_sb", true), client("vi_u", true),
	client("vi_v", true), client("vi_y", true),
	client("gr2d_dst", true), client("gr3d_fdc", true),
	client("host1x", true), client("isp", true),
	client("cpu_uncached", true), client("mpe_mpec", true),
	client("ahb_dma", true), client("ahb_slave", true),
	client("avp_bsev", true), client("avp_mbe", true),
	client("avp_tpm", true),
};

static DEFINE_SPINLOCK(mc_lock);
static unsigned long error_count = 0;
#define MAX_PRINTS 5

static void unthrottle_prints(struct work_struct *work)
{
	unsigned long flags;

	spin_lock_irqsave(&mc_lock, flags);
	error_count = 0;
	spin_unlock_irqrestore(&mc_lock, flags);
}

static DECLARE_DELAYED_WORK(unthrottle_prints_work, unthrottle_prints);

static irqreturn_t tegra_mc_error_isr(int irq, void *data)
{
	void __iomem *mc = IO_ADDRESS(TEGRA_MC_BASE);
	unsigned long count;
	u32 stat;

	stat = readl(mc + MC_INT_STATUS);
	stat &= (MC_INT_SECURITY_VIOLATION |
		 MC_INT_INVALID_GART_PAGE |
		 MC_INT_DECERR_EMEM_OTHERS);

	__cancel_delayed_work(&unthrottle_prints_work);

	spin_lock(&mc_lock);
	count = ++error_count;
	spin_unlock(&mc_lock);

	if (count >= MAX_PRINTS) {
		if (count == MAX_PRINTS)
			pr_err("Too many MC errors; throttling prints\n");
		schedule_delayed_work(&unthrottle_prints_work, HZ/2);
		goto out;
	}

	if (stat & MC_INT_DECERR_EMEM_OTHERS) {
		const struct mc_client *client = NULL;
		u32 addr, req;

		req = readl(mc + MC_DECERR_EMEM_OTHERS_STATUS);
		addr = readl(mc + MC_DECERR_EMEM_OTHERS_ADDRESS);
		req &= 0x3f;
		if (req < ARRAY_SIZE(mc_clients))
			client = &mc_clients[req];

		pr_err("MC_DECERR: %p %s (%s)\n", (void*)addr,
		       (client) ? client->name : "unknown",
		       (client && client->write) ? "write" : "read");
	}

	if (stat & MC_INT_INVALID_GART_PAGE) {
		const struct mc_client *client = NULL;
		u32 addr, req;

		req = readl(mc + MC_GART_ERROR_STATUS);
		addr = readl(mc + MC_GART_ERROR_ADDRESS);
		req = (req >> 1) & 0x3f;

		if (req < ARRAY_SIZE(mc_clients))
			client = &mc_clients[req];

		pr_err("MC_GART_ERR: %p %s (%s)\n", (void*)addr,
		       (client) ? client->name : "unknown",
		       (client && client->write) ? "write" : "read");
	}

	if (stat & MC_INT_SECURITY_VIOLATION) {
		const struct mc_client *client = NULL;
		const char *type = NULL;
		u32 addr, req;

		req = readl(mc + MC_SECURITY_VIOLATION_STATUS);
		addr = readl(mc + MC_SECURITY_VIOLATION_ADDRESS);

		type = (req & (1<<30)) ? "carveout" : "trustzone";

		req &= 0x3f;
		if (req < ARRAY_SIZE(mc_clients))
			client = &mc_clients[req];

		pr_err("MC_SECURITY_ERR (%s): %p %s (%s)\n", type, (void*)addr,
		       (client) ? client->name : "unknown",
		       (client && client->write) ? "write" : "read");
	}
out:
	writel(stat, mc + MC_INT_STATUS);
	return IRQ_HANDLED;
}

static int __init tegra20_mc_init(void)
{
	if (request_irq(INT_MC_GENERAL, tegra_mc_error_isr, 0,
			"mc_status", NULL)) {
		pr_err("%s: unable to register MC error interrupt\n", __func__);
		return -EINVAL;
	} else {
		void __iomem *mc = IO_ADDRESS(TEGRA_MC_BASE);
		u32 reg = MC_INT_SECURITY_VIOLATION | MC_INT_INVALID_GART_PAGE |
			MC_INT_DECERR_EMEM_OTHERS;
		writel(reg, mc + MC_INT_MASK);
	}

	return 0;
}
arch_initcall(tegra20_mc_init);
