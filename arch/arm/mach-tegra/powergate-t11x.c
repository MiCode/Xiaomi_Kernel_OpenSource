/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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

#include <asm/atomic.h>

#include <mach/powergate.h>

#include "powergate-priv.h"
#include "powergate-ops-txx.h"
#include "powergate-ops-t1xx.h"

enum mc_client {
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
	MC_CLIENT_MSENC		= 11,
	MC_CLIENT_NV		= 12,
	MC_CLIENT_PPCS		= 14,
	MC_CLIENT_VDE		= 16,
	MC_CLIENT_VI		= 17,
	MC_CLIENT_XUSB_HOST	= 19,
	MC_CLIENT_XUSB_DEV	= 20,
	MC_CLIENT_EMUCIF	= 21,
	MC_CLIENT_TSEC		= 22,
	MC_CLIENT_LAST		= -1,
	MC_CLIENT_AFI		= MC_CLIENT_LAST,
	MC_CLIENT_MPE		= MC_CLIENT_LAST,
	MC_CLIENT_NV2		= MC_CLIENT_LAST,
	MC_CLIENT_SATA		= MC_CLIENT_LAST,
};

struct tegra11x_powergate_mc_client_info {
	enum mc_client hot_reset_clients[MAX_HOTRESET_CLIENT_NUM];
};

static struct tegra11x_powergate_mc_client_info tegra11x_pg_mc_info[] = {
	[TEGRA_POWERGATE_3D] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_NV,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_VDEC] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_VDE,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_MPE] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_MSENC,
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
	[TEGRA_POWERGATE_HEG] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_G2,
			[1] = MC_CLIENT_EPP,
			[2] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_DISA] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_DC,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_DISB] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_DCB,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_XUSBA] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_XUSBB] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_XUSB_DEV,
			[1] = MC_CLIENT_LAST
		},
	},
	[TEGRA_POWERGATE_XUSBC] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_XUSB_HOST,
			[1] = MC_CLIENT_LAST,
		},
	},
};

static struct powergate_partition_info tegra11x_powergate_partition_info[] = {
	[TEGRA_POWERGATE_3D] = {
		.name = "3d",
		.clk_info = {
			[0] = { .clk_name = "3d", .clk_type = CLK_AND_RST },
		},
	},
	[TEGRA_POWERGATE_VDEC] = {
		.name = "vde",
		.clk_info = {
			[0] = { .clk_name = "vde", .clk_type = CLK_AND_RST },
		},
	},
	[TEGRA_POWERGATE_MPE] = {
		.name = "mpe",
		.clk_info = {
			[0] = { .clk_name = "msenc.cbus", .clk_type = CLK_AND_RST },
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
			[0] = { .clk_name = "2d.cbus", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "epp.cbus", .clk_type = CLK_AND_RST },
		},
	},
	[TEGRA_POWERGATE_DISA] = {
		.name = "disa",
		.clk_info = {
			[0] = { .clk_name = "disp1", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "dsia", .clk_type = CLK_AND_RST },
			[2] = { .clk_name = "dsib", .clk_type = CLK_AND_RST },
			[3] = { .clk_name = "csi", .clk_type = CLK_AND_RST },
			[4] = { .clk_name = "mipi-cal", .clk_type = CLK_AND_RST },
		},
	},
	[TEGRA_POWERGATE_DISB] = {
		.name = "disb",
		.clk_info = {
			[0] = { .clk_name = "disp2", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "hdmi", .clk_type = CLK_AND_RST },
		},
	},
	[TEGRA_POWERGATE_XUSBA] = {
		.name = "xusba",
		.clk_info = {
			[0] = { .clk_name = "xusb_ss", .clk_type = CLK_AND_RST },
		},
	},
	[TEGRA_POWERGATE_XUSBB] = {
		.name = "xusbb",
		.clk_info = {
			[0] = { .clk_name = "xusb_dev", .clk_type = CLK_AND_RST },
		},
	},
	[TEGRA_POWERGATE_XUSBC] = {
		.name = "xusbc",
		.clk_info = {
			[0] = { .clk_name = "xusb_host", .clk_type = CLK_AND_RST },
		},
	},
};

static atomic_t ref_count_a = ATOMIC_INIT(1); /* for TEGRA_POWERGATE_DISA */
static atomic_t ref_count_b = ATOMIC_INIT(1); /* for TEGRA_POWERGATE_DISB */

static void __iomem *mipi_cal = IO_ADDRESS(TEGRA_MIPI_CAL_BASE);
static u32 mipi_cal_read(unsigned long reg)
{
	return readl(mipi_cal + reg);
}

static void mipi_cal_write(u32 val, unsigned long reg)
{
	writel_relaxed(val, mipi_cal + reg);
}

#define MC_CLIENT_HOTRESET_CTRL		0x200
#define MC_CLIENT_HOTRESET_STAT		0x204

static DEFINE_SPINLOCK(tegra11x_powergate_lock);

/* Forward Declarations */
int tegra11x_powergate_mc_flush(int id);
int tegra11x_powergate_mc_flush_done(int id);
int tegra11x_unpowergate_partition_with_clk_on(int id);
int tegra11x_powergate_partition_with_clk_off(int id);

bool tegra11x_powergate_check_clamping(int id)
{
	u32 mask;
	/*
	 * PCIE and VDE clamping masks are swapped with respect to their
	 * partition ids
	 */
	if (id ==  TEGRA_POWERGATE_VDEC)
		mask = (1 << TEGRA_POWERGATE_PCIE);
	else if (id == TEGRA_POWERGATE_PCIE)
		mask = (1 << TEGRA_POWERGATE_VDEC);
	else
		mask = (1 << id);

	return !!(pmc_read(PWRGATE_CLAMP_STATUS) & mask);
}

#define HOTRESET_READ_COUNT	5
static bool tegra11x_stable_hotreset_check(u32 *stat)
{
	int i;
	u32 cur_stat;
	u32 prv_stat;
	unsigned long flags;

	spin_lock_irqsave(&tegra11x_powergate_lock, flags);
	prv_stat = mc_read(MC_CLIENT_HOTRESET_STAT);
	for (i = 0; i < HOTRESET_READ_COUNT; i++) {
		cur_stat = mc_read(MC_CLIENT_HOTRESET_STAT);
		if (cur_stat != prv_stat) {
			spin_unlock_irqrestore(&tegra11x_powergate_lock, flags);
			return false;
		}
	}
	*stat = cur_stat;
	spin_unlock_irqrestore(&tegra11x_powergate_lock, flags);
	return true;
}

/*
 * FIXME: sw war for mipi-cal calibration when unpowergating DISA partition
 */
static void tegra11x_mipical_calibrate(int id)
{
	struct reg_offset_val {
		u32 offset;
		u32 por_value;
	};
	u32 status;
	unsigned long flags;

#define MIPI_CAL_MIPI_CAL_CTRL_0		0x0
#define MIPI_CAL_CIL_MIPI_CAL_STATUS_0		0x8
#define MIPI_CAL_CILA_MIPI_CAL_CONFIG_0		0x14
#define MIPI_CAL_CILB_MIPI_CAL_CONFIG_0		0x18
#define MIPI_CAL_CILC_MIPI_CAL_CONFIG_0		0x1c
#define MIPI_CAL_CILD_MIPI_CAL_CONFIG_0		0x20
#define MIPI_CAL_CILE_MIPI_CAL_CONFIG_0		0x24
#define MIPI_CAL_DSIA_MIPI_CAL_CONFIG_0		0x38
#define MIPI_CAL_DSIB_MIPI_CAL_CONFIG_0		0x3c
#define MIPI_CAL_DSIC_MIPI_CAL_CONFIG_0		0x40
#define MIPI_CAL_DSID_MIPI_CAL_CONFIG_0		0x44

	static struct reg_offset_val mipi_cal_por_values[] = {
		{ MIPI_CAL_MIPI_CAL_CTRL_0, 0x2a000000 },
		{ MIPI_CAL_CILA_MIPI_CAL_CONFIG_0, 0x00200000 },
		{ MIPI_CAL_CILB_MIPI_CAL_CONFIG_0, 0x00200000 },
		{ MIPI_CAL_CILC_MIPI_CAL_CONFIG_0, 0x00200000 },
		{ MIPI_CAL_CILD_MIPI_CAL_CONFIG_0, 0x00200000 },
		{ MIPI_CAL_CILE_MIPI_CAL_CONFIG_0, 0x00000000 },
		{ MIPI_CAL_DSIA_MIPI_CAL_CONFIG_0, 0x00200000 },
		{ MIPI_CAL_DSIB_MIPI_CAL_CONFIG_0, 0x00200000 },
		{ MIPI_CAL_DSIC_MIPI_CAL_CONFIG_0, 0x00200000 },
		{ MIPI_CAL_DSID_MIPI_CAL_CONFIG_0, 0x00200000 },
	};
	int i;

	if (id != TEGRA_POWERGATE_DISA)
		return;

	spin_lock_irqsave(&tegra11x_powergate_lock, flags);

	/* mipi cal por restore */
	for (i = 0; i < ARRAY_SIZE(mipi_cal_por_values); i++) {
		mipi_cal_write(mipi_cal_por_values[i].por_value,
			mipi_cal_por_values[i].offset);
	}

	/* mipi cal status clear */
	status = mipi_cal_read(MIPI_CAL_CIL_MIPI_CAL_STATUS_0);
	mipi_cal_write(status, MIPI_CAL_CIL_MIPI_CAL_STATUS_0);

	/* mipi cal status read - to flush writes */
	status = mipi_cal_read(MIPI_CAL_CIL_MIPI_CAL_STATUS_0);

	spin_unlock_irqrestore(&tegra11x_powergate_lock, flags);
}

static int tegra11x_powergate_partition_internal(int id,
	struct powergate_partition_info *pg_info)
{
	int ret;

	if (tegra_powergate_is_powered(id)) {
		ret = is_partition_clk_disabled(pg_info);
		if (ret < 0) {
			/* clock enabled */
			ret = tegra11x_powergate_partition_with_clk_off(id);
			if (ret < 0)
				return ret;
		} else {
			ret = tegra_powergate_partition(id);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

static int tegra11x_unpowergate_partition_internal(int id,
	struct powergate_partition_info *pg_info)
{
	int ret;

	if (!tegra_powergate_is_powered(id)) {
		ret = is_partition_clk_disabled(pg_info);
		if (ret) {
			/* clock disabled */
			ret = tegra11x_unpowergate_partition_with_clk_on(id);
			if (ret < 0)
				return ret;
		} else {
			ret = tegra_unpowergate_partition(id);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

/*
 * Tegra11x has powergate dependencies between partitions.
 * This function captures the dependencies.
 */
static int tegra11x_check_partition_pg_seq(int id,
	struct powergate_partition_info *pg_info)
{
	int ret;

	if (id == TEGRA_POWERGATE_DISA) {
		ret = tegra11x_powergate_partition_internal(TEGRA_POWERGATE_VENC,
			pg_info);
		if (ret < 0)
			return ret;

		ret = tegra11x_powergate_partition_internal(TEGRA_POWERGATE_DISB,
			pg_info);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * This function captures power-ungate dependencies between tegra11x partitions
 */
static int tegra11x_check_partition_pug_seq(int id,
	struct powergate_partition_info *pg_info)
{
	int ret;

	switch (id) {
	case TEGRA_POWERGATE_DISB:
	case TEGRA_POWERGATE_VENC:
		ret = tegra11x_unpowergate_partition_internal(TEGRA_POWERGATE_DISA,
			pg_info);
		if (ret < 0)
			return ret;

		break;
	}
	return 0;
}

int tegra11x_powergate_mc_enable(int id)
{
	return 0;
}

int tegra11x_powergate_mc_disable(int id)
{
	return 0;
}

int tegra11x_powergate_mc_flush(int id)
{
	u32 idx, rst_ctrl, rst_stat;
	enum mc_client mcClientBit;
	unsigned long flags;
	bool ret;

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			tegra11x_pg_mc_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra11x_powergate_lock, flags);
		rst_ctrl = mc_read(MC_CLIENT_HOTRESET_CTRL);
		rst_ctrl |= (1 << mcClientBit);
		mc_write(rst_ctrl, MC_CLIENT_HOTRESET_CTRL);

		spin_unlock_irqrestore(&tegra11x_powergate_lock, flags);

		do {
			udelay(10);
			rst_stat = 0;
			ret = tegra11x_stable_hotreset_check(&rst_stat);
			if (!ret)
				continue;
		} while (!(rst_stat & (1 << mcClientBit)));
	}

	return 0;
}

int tegra11x_powergate_mc_flush_done(int id)
{
	u32 idx, rst_ctrl;
	enum mc_client mcClientBit;
	unsigned long flags;

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			tegra11x_pg_mc_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		spin_lock_irqsave(&tegra11x_powergate_lock, flags);

		rst_ctrl = mc_read(MC_CLIENT_HOTRESET_CTRL);
		rst_ctrl &= ~(1 << mcClientBit);
		mc_write(rst_ctrl, MC_CLIENT_HOTRESET_CTRL);

		spin_unlock_irqrestore(&tegra11x_powergate_lock, flags);
	}

	wmb();

	return 0;
}

static int tegra11x_unpowergate(int id,
	struct powergate_partition_info *pg_info)
{
	int ret;

	/* If first clk_ptr is null, fill clk info for the partition */
	if (!pg_info->clk_info[0].clk_ptr)
		get_clk_info(pg_info);

	if (tegra_powergate_is_powered(id))
		return tegra_powergate_reset_module(pg_info);

	ret = tegra_powergate_set(id, true);
	if (ret)
		goto err_power;

	udelay(10);

	powergate_partition_assert_reset(pg_info);

	udelay(10);

	/* Un-Powergating fails if all clks are not enabled */
	ret = partition_clk_enable(pg_info);
	if (ret)
		goto err_clk_on;

	udelay(10);

	ret = tegra_powergate_remove_clamping(id);
	if (ret)
		goto err_clamp;

	udelay(10);

	tegra11x_mipical_calibrate(id);

	powergate_partition_deassert_reset(pg_info);

	udelay(10);

	tegra_powergate_mc_flush_done(id);

	udelay(10);

	/* Disable all clks enabled earlier. Drivers should enable clks */
	partition_clk_disable(pg_info);

	return 0;

err_clamp:
	partition_clk_disable(pg_info);
err_clk_on:
	powergate_module(id);
err_power:
	WARN(1, "Could not Un-Powergate %d", id);
	return ret;
}

void tegra11x_powergate_dis_partition(void)
{
	tegra1xx_powergate(TEGRA_POWERGATE_DISB,
		&tegra11x_powergate_partition_info[TEGRA_POWERGATE_DISB]);

	tegra11x_powergate_partition_internal(TEGRA_POWERGATE_VENC,
		&tegra11x_powergate_partition_info[TEGRA_POWERGATE_DISA]);

	tegra1xx_powergate(TEGRA_POWERGATE_DISA,
		&tegra11x_powergate_partition_info[TEGRA_POWERGATE_DISA]);
}

/* The logic manages the ref-count for dis partitions. The dependency between
 * disa and disb is hided from client. */
bool tegra11x_powergate_check_dis_refcount(int id, int op)
{
	WARN_ONCE(atomic_read(&ref_count_a) < 0, "dis ref a count underflow");
	WARN_ONCE(atomic_read(&ref_count_b) < 0, "dis ref b count underflow");

	if (op && id == TEGRA_POWERGATE_DISA) {
		if (atomic_inc_return(&ref_count_a) != 1)
			return 0;
	} else if (op && id == TEGRA_POWERGATE_DISB) {
		if (tegra_powergate_is_powered(TEGRA_POWERGATE_DISA))
			atomic_inc(&ref_count_a);
		if (atomic_inc_return(&ref_count_b) != 1)
			return 0;
	} else if (!op && id == TEGRA_POWERGATE_DISA) {
		if (atomic_dec_return(&ref_count_a) != 0)
			return 0;
	} else if (!op && id == TEGRA_POWERGATE_DISB) {
		atomic_dec(&ref_count_a);
		if (atomic_dec_return(&ref_count_b) != 0) {
			return 0;
		} else if (atomic_read(&ref_count_a) == 0) {
			tegra11x_powergate_dis_partition();
			return 0;
		}
	}

	return 1;
}

int tegra11x_powergate_partition(int id)
{
	int ret;

	if ((id == TEGRA_POWERGATE_DISA || id == TEGRA_POWERGATE_DISB) &&
			!tegra11x_powergate_check_dis_refcount(id, 0))
		return 0;

	ret = tegra11x_check_partition_pg_seq(id,
		&tegra11x_powergate_partition_info[id]);
	if (ret)
		return ret;

	/* call common power-gate API for t1xx */
	ret = tegra1xx_powergate(id,
		&tegra11x_powergate_partition_info[id]);

	return ret;
}

int tegra11x_unpowergate_partition(int id)
{
	int ret;

	if ((id == TEGRA_POWERGATE_DISA || id == TEGRA_POWERGATE_DISB) &&
			!tegra11x_powergate_check_dis_refcount(id, 1))
		return 0;

	ret = tegra11x_check_partition_pug_seq(id,
		&tegra11x_powergate_partition_info[id]);
	if (ret)
		return ret;

	/* t11x needs to calibrate mipi in un-power-gate sequence
	 * hence it cannot use common un-power-gate api tegra1xx_unpowergate */
	ret = tegra11x_unpowergate(id,
		&tegra11x_powergate_partition_info[id]);

	return ret;
}

int tegra11x_powergate_partition_with_clk_off(int id)
{
	return tegraxx_powergate_partition_with_clk_off(id,
		&tegra11x_powergate_partition_info[id]);
}

int tegra11x_unpowergate_partition_with_clk_on(int id)
{
	return tegraxx_unpowergate_partition_with_clk_on(id,
		&tegra11x_powergate_partition_info[id]);
}

const char *tegra11x_get_powergate_domain_name(int id)
{
	return tegra11x_powergate_partition_info[id].name;
}

spinlock_t *tegra11x_get_powergate_lock(void)
{
	return &tegra11x_powergate_lock;
}

int tegra11x_powergate_init_refcount(void)
{
	if (tegra_powergate_is_powered(TEGRA_POWERGATE_DISA))
			atomic_set(&ref_count_a, 1);
	else
			atomic_set(&ref_count_a, 0);

	if (tegra_powergate_is_powered(TEGRA_POWERGATE_DISB))
			atomic_set(&ref_count_b, 1);
	else
			atomic_set(&ref_count_b, 0);
	return 0;
}

static struct powergate_ops tegra11x_powergate_ops = {
	.soc_name = "tegra11x",

	.num_powerdomains = TEGRA_NUM_POWERGATE,

	.get_powergate_lock = tegra11x_get_powergate_lock,
	.get_powergate_domain_name = tegra11x_get_powergate_domain_name,

	.powergate_partition = tegra11x_powergate_partition,
	.unpowergate_partition = tegra11x_unpowergate_partition,

	.powergate_partition_with_clk_off =  tegra11x_powergate_partition_with_clk_off,
	.unpowergate_partition_with_clk_on = tegra11x_unpowergate_partition_with_clk_on,

	.powergate_mc_enable = tegra11x_powergate_mc_enable,
	.powergate_mc_disable = tegra11x_powergate_mc_disable,

	.powergate_mc_flush = tegra11x_powergate_mc_flush,
	.powergate_mc_flush_done = tegra11x_powergate_mc_flush_done,

	.powergate_init_refcount = tegra11x_powergate_init_refcount,
	.powergate_check_clamping = tegra11x_powergate_check_clamping,
};

struct powergate_ops *tegra11x_powergate_init_chip_support(void)
{
	return &tegra11x_powergate_ops;
}
