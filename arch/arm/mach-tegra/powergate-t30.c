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

#include <mach/powergate.h>

#include "powergate-priv.h"
#include "powergate-ops-txx.h"

enum mc_client {
	MC_CLIENT_AFI		= 0,
	MC_CLIENT_AVPC		= 1,
	MC_CLIENT_DC		= 2,
	MC_CLIENT_DCB		= 3,
	MC_CLIENT_EPP		= 4,
	MC_CLIENT_G2		= 5,
	MC_CLIENT_HC		= 6,
	MC_CLIENT_HDA		= 7,
	MC_CLIENT_ISP		= 8,
	MC_CLIENT_MPCORE	= 9,
	MC_CLIENT_MPCORELP	= 10,
	MC_CLIENT_MPE		= 11,
	MC_CLIENT_NV		= 12,
	MC_CLIENT_NV2		= 13,
	MC_CLIENT_PPCS		= 14,
	MC_CLIENT_SATA		= 15,
	MC_CLIENT_VDE		= 16,
	MC_CLIENT_VI		= 17,
	MC_CLIENT_LAST		= -1,
};

struct tegra3_powergate_mc_client_info {
	enum mc_client hot_reset_clients[MAX_HOTRESET_CLIENT_NUM];
};

static struct tegra3_powergate_mc_client_info tegra3_pg_mc_info[] = {
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
		.hot_reset_clients ={
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
			[0] = MC_CLIENT_MPE,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_VENC] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_ISP,
			[1] = MC_CLIENT_VI,
			[2] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_CPU1] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_CPU2] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_CPU3] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_CELP] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
#ifdef CONFIG_ARCH_TEGRA_HAS_SATA
	[TEGRA_POWERGATE_SATA] = {
		.hot_reset_clients ={
			[0] = MC_CLIENT_SATA,
			[1] = MC_CLIENT_LAST
		},
	},
#endif
#ifdef CONFIG_ARCH_TEGRA_HAS_DUAL_3D
	[TEGRA_POWERGATE_3D1] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_NV2,
			[1] = MC_CLIENT_LAST
		},
	},
#endif
	[TEGRA_POWERGATE_HEG] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_G2,
			[1] = MC_CLIENT_EPP,
			[2] = MC_CLIENT_HC,
			[3] = MC_CLIENT_LAST
		},
	},
};

static struct powergate_partition_info tegra3_powergate_partition_info[] = {
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
			[2] = { .clk_name = "cml0", .clk_type = CLK_ONLY },
			[3] = { .clk_name = "pciex", .clk_type = RST_ONLY },
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
			[0] = { .clk_name = "mpe.cbus", CLK_AND_RST },
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
	[TEGRA_POWERGATE_CPU1] = { .name = "cpu1" },
	[TEGRA_POWERGATE_CPU2] = { .name = "cpu2" },
	[TEGRA_POWERGATE_CPU3] = { .name = "cpu3" },
	[TEGRA_POWERGATE_CELP] = { .name = "celp" },
#ifdef CONFIG_ARCH_TEGRA_HAS_SATA
	[TEGRA_POWERGATE_SATA] = {
		.name = "sata",
		.clk_info = {
			[0] = { .clk_name = "sata", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "sata_oob", .clk_type = CLK_AND_RST },
			[2] = { .clk_name = "cml1", .clk_type = CLK_ONLY },
			[3] = { .clk_name = "sata_cold", .clk_type = RST_ONLY },
		},
	},
#endif
#ifdef CONFIG_ARCH_TEGRA_HAS_DUAL_3D
	[TEGRA_POWERGATE_3D1] = {
		.name = "3d1",
		.clk_info = {
			[0] = { .clk_name = "3d2", .clk_type = CLK_AND_RST },
		},
	},
#endif
	[TEGRA_POWERGATE_HEG] = {
		.name = "heg",
		.clk_info = {
			[0] = { .clk_name = "2d.cbus", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "epp.cbus", .clk_type = CLK_AND_RST },
			[2] = { .clk_name = "host1x.cbus", .clk_type = CLK_AND_RST },
		},
	},
};

static u8 tegra3_quad_cpu_domains[] = {
	TEGRA_POWERGATE_CPU0,
	TEGRA_POWERGATE_CPU1,
	TEGRA_POWERGATE_CPU2,
	TEGRA_POWERGATE_CPU3,
};

#define MC_CLIENT_HOTRESET_CTRL		0x200
#define MC_CLIENT_HOTRESET_STAT		0x204

static DEFINE_SPINLOCK(tegra3_powergate_lock);

int tegra3_powergate_partition(int id)
{
	return tegraxx_powergate_partition(id,
		&tegra3_powergate_partition_info[id]);
}

int tegra3_unpowergate_partition(int id)
{
	return tegraxx_unpowergate_partition(id,
		&tegra3_powergate_partition_info[id]);
}

int tegra3_powergate_partition_with_clk_off(int id)
{
	if (id != TEGRA_POWERGATE_PCIE && id != TEGRA_POWERGATE_SATA) {
		WARN_ON(1);
		return -EINVAL;
	}

	return tegraxx_powergate_partition_with_clk_off(id,
		&tegra3_powergate_partition_info[id]);
}

int tegra3_unpowergate_partition_with_clk_on(int id)
{
	if (id != TEGRA_POWERGATE_SATA && id != TEGRA_POWERGATE_PCIE) {
		WARN_ON(1);
		return -EINVAL;
	}

	return tegraxx_unpowergate_partition_with_clk_on(id,
		&tegra3_powergate_partition_info[id]);
}

int tegra3_powergate_mc_enable(int id)
{
	return 0;
}

int tegra3_powergate_mc_disable(int id)
{
	return 0;
}

int tegra3_powergate_mc_flush(int id)
{
	u32 idx, rst_ctrl, rst_stat;
	enum mc_client mcClientBit;
	unsigned long flags;

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			tegra3_pg_mc_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra3_powergate_lock, flags);
		rst_ctrl = mc_read(MC_CLIENT_HOTRESET_CTRL);
		rst_ctrl |= (1 << mcClientBit);
		mc_write(rst_ctrl, MC_CLIENT_HOTRESET_CTRL);
		spin_unlock_irqrestore(&tegra3_powergate_lock, flags);

		do {
			udelay(10);
			rst_stat = mc_read(MC_CLIENT_HOTRESET_STAT);
		} while (!(rst_stat & (1 << mcClientBit)));
	}

	return 0;
}

int tegra3_powergate_mc_flush_done(int id)
{
	u32 idx, rst_ctrl;
	enum mc_client mcClientBit;
	unsigned long flags;

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			tegra3_pg_mc_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra3_powergate_lock, flags);

		rst_ctrl = mc_read(MC_CLIENT_HOTRESET_CTRL);
		rst_ctrl &= ~(1 << mcClientBit);
		mc_write(rst_ctrl, MC_CLIENT_HOTRESET_CTRL);

		spin_unlock_irqrestore(&tegra3_powergate_lock, flags);
	}

	wmb();

	return 0;
}

const char *tegra3_get_powergate_domain_name(int id)
{
	return tegra3_powergate_partition_info[id].name;
}

spinlock_t *tegra3_get_powergate_lock(void)
{
	return &tegra3_powergate_lock;
}

static struct powergate_ops tegra3_powergate_ops = {
	.soc_name = "tegra3",

	.num_powerdomains = TEGRA_NUM_POWERGATE,
	.num_cpu_domains = 4,
	.cpu_domains = tegra3_quad_cpu_domains,

	.get_powergate_lock = tegra3_get_powergate_lock,

	.get_powergate_domain_name = tegra3_get_powergate_domain_name,

	.powergate_partition = tegra3_powergate_partition,
	.unpowergate_partition = tegra3_unpowergate_partition,

	.powergate_partition_with_clk_off = tegra3_powergate_partition_with_clk_off,
	.unpowergate_partition_with_clk_on = tegra3_unpowergate_partition_with_clk_on,

	.powergate_mc_enable = tegra3_powergate_mc_enable,
	.powergate_mc_disable = tegra3_powergate_mc_disable,

	.powergate_mc_flush = tegra3_powergate_mc_flush,
	.powergate_mc_flush_done = tegra3_powergate_mc_flush_done,
};

struct powergate_ops *tegra3_powergate_init_chip_support(void)
{
	return &tegra3_powergate_ops;
}
