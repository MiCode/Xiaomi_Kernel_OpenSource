/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/tegra-powergate.h>

#include "powergate-priv.h"
#include "powergate-ops-txx.h"

enum mc_client {
	MC_CLIENT_AVPC		= 0,
	MC_CLIENT_DC		= 1,
	MC_CLIENT_DCB		= 2,
	MC_CLIENT_EPP		= 3,
	MC_CLIENT_G2		= 4,
	MC_CLIENT_HC		= 5,
	MC_CLIENT_ISP		= 6,
	MC_CLIENT_MPCORE	= 7,
	MC_CLIENT_MPEA		= 8,
	MC_CLIENT_MPEB		= 9,
	MC_CLIENT_MPEC		= 10,
	MC_CLIENT_NV		= 11,
	MC_CLIENT_PPCS		= 12,
	MC_CLIENT_VDE		= 13,
	MC_CLIENT_VI		= 14,
	MC_CLIENT_LAST		= -1,
	MC_CLIENT_AFI		= MC_CLIENT_LAST,
};

struct tegra2_powergate_mc_client_info {
	enum mc_client hot_reset_clients[MAX_HOTRESET_CLIENT_NUM];
};

static struct tegra2_powergate_mc_client_info tegra2_pg_mc_info[] = {
	[TEGRA_POWERGATE_CPU] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_L2] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_3D] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_NV,
			[1] = MC_CLIENT_LAST,
		},
	},
#ifdef CONFIG_ARCH_TEGRA_HAS_PCIE
	[TEGRA_POWERGATE_PCIE] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_AFI,
			[1] = MC_CLIENT_LAST,
		},
	},
#endif
	[TEGRA_POWERGATE_VDEC] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_VDE,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_MPE] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_MPEA,
			[1] = MC_CLIENT_MPEB,
			[2] = MC_CLIENT_MPEC,
			[3] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_VENC] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_ISP,
			[1] = MC_CLIENT_VI,
			[2] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_HEG] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_G2,
			[1] = MC_CLIENT_EPP,
			[2] = MC_CLIENT_HC,
			[3] = MC_CLIENT_LAST,
		},
	},
};

static struct powergate_partition_info tegra2_powergate_partition_info[] = {
	[TEGRA_POWERGATE_CPU] = { .name = "cpu0" },
	[TEGRA_POWERGATE_L2] = { .name = "l2" },
	[TEGRA_POWERGATE_3D] = {
		.name = "3d0",
		.clk_info = {
			[0] = { .clk_name = "3d", .clk_type = CLK_AND_RST },
		},
	},
#ifdef CONFIG_ARCH_TEGRA_HAS_PCIE
	[TEGRA_POWERGATE_PCIE] = {
		.name = "pcie",
		.clk_info = {
			[0] = { .clk_name = "afi", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "pcie", .clk_type = CLK_AND_RST },
			[2] = { .clk_name = "pciex", .clk_type = RST_ONLY },
		},
	},
#endif
	[TEGRA_POWERGATE_VDEC] = {
		.name = "vde",
		.clk_info = {
			[0] = { .clk_name = "vde", .clk_type = CLK_AND_RST },
		},
	},
	[TEGRA_POWERGATE_MPE] = {
		.name = "mpe",
		.clk_info = {
			[0] = { .clk_name = "mpe", .clk_type = CLK_AND_RST },
		},
	},
	[TEGRA_POWERGATE_VENC] = {
		.name = "ve",
		.clk_info = {
			[0] = { .clk_name = "isp", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "vi", .clk_type = CLK_AND_RST },
			[2] = { .clk_name = "csi", .clk_type = CLK_AND_RST },
		},
	},
	[TEGRA_POWERGATE_HEG] = {
		.name = "heg",
		.clk_info = {
			[0] = { .clk_name = "2d", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "epp", .clk_type = CLK_AND_RST },
			[2] = { .clk_name = "host1x", .clk_type = CLK_AND_RST },
		},
	},
};

#define MC_CLIENT_CTRL		0x100
#define MC_CLIENT_HOTRESETN	0x104
#define MC_CLIENT_ORRC_BASE	0x140

static DEFINE_SPINLOCK(tegra2_powergate_lock);

int tegra2_powergate_partition(int id)
{
	return tegraxx_powergate_partition(id,
		&tegra2_powergate_partition_info[id]);
}

int tegra2_unpowergate_partition(int id)
{
	return tegraxx_unpowergate_partition(id,
		&tegra2_powergate_partition_info[id]);
}

int tegra2_powergate_partition_with_clk_off(int id)
{
	/* Restrict functions use to selected partitions */
	if (id != TEGRA_POWERGATE_PCIE) {
		WARN_ON(1);
		return -EINVAL;
	}

	return tegraxx_powergate_partition_with_clk_off(id,
		&tegra2_powergate_partition_info[id]);
}

int tegra2_unpowergate_partition_with_clk_on(int id)
{
	/* Restrict this functions use to few partitions */
	if (id != TEGRA_POWERGATE_PCIE) {
		WARN_ON(1);
		return -EINVAL;
	}

	return tegraxx_unpowergate_partition_with_clk_on(id,
		&tegra2_powergate_partition_info[id]);
}

int tegra2_powergate_mc_enable(int id)
{
	u32 idx, clt_ctrl;
	enum mc_client mcClientBit;
	unsigned long flags;

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			tegra2_pg_mc_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra2_powergate_lock, flags);

		/* enable client */
		clt_ctrl = mc_read(MC_CLIENT_CTRL);
		clt_ctrl |= (1 << mcClientBit);
		mc_write(clt_ctrl, MC_CLIENT_CTRL);

		/* read back to flush write */
		clt_ctrl = mc_read(MC_CLIENT_CTRL);

		spin_unlock_irqrestore(&tegra2_powergate_lock, flags);
	}

	return 0;
}

int tegra2_powergate_mc_disable(int id)
{
	u32 idx, clt_ctrl, orrc_reg;
	enum mc_client mcClientBit;
	unsigned long flags;

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			tegra2_pg_mc_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra2_powergate_lock, flags);

		/* clear client enable bit */
		clt_ctrl = mc_read(MC_CLIENT_CTRL);
		clt_ctrl &= ~(1 << mcClientBit);
		mc_write(clt_ctrl, MC_CLIENT_CTRL);

		/* read back to flush write */
		clt_ctrl = mc_read(MC_CLIENT_CTRL);

		spin_unlock_irqrestore(&tegra2_powergate_lock, flags);

		/* wait for outstanding requests to reach 0 */
		orrc_reg = MC_CLIENT_ORRC_BASE + (mcClientBit * 4);
		while (mc_read(orrc_reg) != 0)
			udelay(10);
	}

	return 0;
}

int tegra2_powergate_mc_flush(int id)
{
	u32 idx, hot_rstn;
	enum mc_client mcClientBit;
	unsigned long flags;

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			tegra2_pg_mc_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra2_powergate_lock, flags);

		/* assert hotreset (client module is currently in reset) */
		hot_rstn = mc_read(MC_CLIENT_HOTRESETN);
		hot_rstn &= ~(1 << mcClientBit);
		mc_write(hot_rstn, MC_CLIENT_HOTRESETN);

		/* read back to flush write */
		hot_rstn = mc_read(MC_CLIENT_HOTRESETN);

		spin_unlock_irqrestore(&tegra2_powergate_lock, flags);
	}

	return 0;
}

int tegra2_powergate_mc_flush_done(int id)
{
	u32 idx, hot_rstn;
	enum mc_client mcClientBit;
	unsigned long flags;

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			tegra2_pg_mc_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra2_powergate_lock, flags);

		/* deassert hotreset */
		hot_rstn = mc_read(MC_CLIENT_HOTRESETN);
		hot_rstn |= (1 << mcClientBit);
		mc_write(hot_rstn, MC_CLIENT_HOTRESETN);

		/* read back to flush write */
		hot_rstn = mc_read(MC_CLIENT_HOTRESETN);

		spin_unlock_irqrestore(&tegra2_powergate_lock, flags);
	}

	return 0;
}

const char *tegra2_get_powergate_domain_name(int id)
{
	return tegra2_powergate_partition_info[id].name;
}

spinlock_t *tegra2_get_powergate_lock(void)
{
	return &tegra2_powergate_lock;
}

static struct powergate_ops tegra2_powergate_ops = {
	.soc_name = "tegra2",

	.num_powerdomains = TEGRA_NUM_POWERGATE,
	.num_cpu_domains = 0,
	.cpu_domains = NULL,

	.get_powergate_lock = tegra2_get_powergate_lock,

	.get_powergate_domain_name = tegra2_get_powergate_domain_name,

	.powergate_partition = tegra2_powergate_partition,
	.unpowergate_partition = tegra2_unpowergate_partition,

	.powergate_partition_with_clk_off = tegra2_powergate_partition_with_clk_off,
	.unpowergate_partition_with_clk_on = tegra2_unpowergate_partition_with_clk_on,

	.powergate_mc_enable = tegra2_powergate_mc_enable,
	.powergate_mc_disable = tegra2_powergate_mc_disable,

	.powergate_mc_flush = tegra2_powergate_mc_flush,
	.powergate_mc_flush_done = tegra2_powergate_mc_flush_done,
};

struct powergate_ops *tegra2_powergate_init_chip_support(void)
{
	return &tegra2_powergate_ops;
}
