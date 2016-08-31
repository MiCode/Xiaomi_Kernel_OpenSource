/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/regulator/consumer.h>
#include <linux/tegra-powergate.h>

#include <asm/atomic.h>

#include "powergate-priv.h"
#include "powergate-ops-txx.h"
#include "powergate-ops-t1xx.h"
#include "dvfs.h"

enum mc_client {
	MC_CLIENT_AFI		= 0,
	MC_CLIENT_DC		= 2,
	MC_CLIENT_DCB		= 3,
	MC_CLIENT_ISP		= 8,
	MC_CLIENT_MSENC		= 11,
	MC_CLIENT_SATA		= 15,
	MC_CLIENT_VDE		= 16,
	MC_CLIENT_VI		= 17,
	MC_CLIENT_VIC		= 18,
	MC_CLIENT_XUSB_HOST	= 19,
	MC_CLIENT_XUSB_DEV	= 20,
	MC_CLIENT_ISPB		= 33,
	MC_CLIENT_GPU		= 34,
	MC_CLIENT_LAST		= -1,
};

struct tegra12x_powergate_mc_client_info {
	enum mc_client hot_reset_clients[MAX_HOTRESET_CLIENT_NUM];
};

static struct tegra12x_powergate_mc_client_info tegra12x_pg_mc_info[] = {
	[TEGRA_POWERGATE_CRAIL] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_GPU] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_GPU,
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
			[1] = MC_CLIENT_ISPB,
			[2] = MC_CLIENT_VI,
			[3] = MC_CLIENT_LAST,
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
	[TEGRA_POWERGATE_CPU0] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_C0NC] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_C1NC] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
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
#ifdef CONFIG_ARCH_TEGRA_HAS_PCIE
	[TEGRA_POWERGATE_PCIE] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_AFI,
			[1] = MC_CLIENT_LAST,
		},
	},
#endif
#ifdef CONFIG_ARCH_TEGRA_HAS_SATA
	[TEGRA_POWERGATE_SATA] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_SATA,
			[1] = MC_CLIENT_LAST,
		},
	},
#endif
	[TEGRA_POWERGATE_SOR] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
#ifdef CONFIG_ARCH_TEGRA_VIC
	[TEGRA_POWERGATE_VIC] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_VIC,
			[1] = MC_CLIENT_LAST,
		},
	},
#endif
};

static struct powergate_partition_info tegra12x_powergate_partition_info[] = {
	[TEGRA_POWERGATE_CRAIL] = { .name = "crail" },
	[TEGRA_POWERGATE_GPU] = {
		.name = "gpu",
		.clk_info = {
			[0] = { .clk_name = "gpu_ref", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "pll_p_out5", .clk_type = CLK_ONLY },
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
			[0] = { .clk_name = "ispa", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "ispb", .clk_type = CLK_AND_RST },
			[2] = { .clk_name = "vi", .clk_type = CLK_AND_RST },
			[3] = { .clk_name = "csi", .clk_type = CLK_AND_RST },
		},
	},
	[TEGRA_POWERGATE_CPU1] = { .name = "cpu1" },
	[TEGRA_POWERGATE_CPU2] = { .name = "cpu2" },
	[TEGRA_POWERGATE_CPU3] = { .name = "cpu3" },
	[TEGRA_POWERGATE_CELP] = { .name = "celp" },
	[TEGRA_POWERGATE_CPU0] = { .name = "cpu0" },
	[TEGRA_POWERGATE_C0NC] = { .name = "c0nc" },
	[TEGRA_POWERGATE_C1NC] = { .name = "c1nc" },
	[TEGRA_POWERGATE_DISA] = {
		.name = "disa",
		.clk_info = {
			[0] = { .clk_name = "disp1", .clk_type = CLK_AND_RST },
		},
	},
	[TEGRA_POWERGATE_DISB] = {
		.name = "disb",
		.clk_info = {
			[0] = { .clk_name = "disp2", .clk_type = CLK_AND_RST },
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
	[TEGRA_POWERGATE_SOR] = {
		.name = "sor",
		.clk_info = {
			[0] = { .clk_name = "sor0", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "dsia", .clk_type = CLK_AND_RST },
			[2] = { .clk_name = "dsib", .clk_type = CLK_AND_RST },
			[3] = { .clk_name = "hdmi", .clk_type = CLK_AND_RST },
			[4] = { .clk_name = "mipi-cal", .clk_type = CLK_AND_RST },
			[5] = { .clk_name = "dpaux", .clk_type = CLK_ONLY },
		},
	},
#ifdef CONFIG_ARCH_TEGRA_VIC
	[TEGRA_POWERGATE_VIC] = {
		.name = "vic",
		.clk_info = {
			[0] = { .clk_name = "vic03.cbus", .clk_type = CLK_AND_RST },
		},
	},
#endif
};

#define MC_CLIENT_HOTRESET_CTRL		0x200
#define MC_CLIENT_HOTRESET_STAT		0x204
#define MC_CLIENT_HOTRESET_CTRL_1	0x970
#define MC_CLIENT_HOTRESET_STAT_1	0x974

#define PMC_GPU_RG_CNTRL_0		0x2d4

static DEFINE_SPINLOCK(tegra12x_powergate_lock);

static struct dvfs_rail *gpu_rail;

#define HOTRESET_READ_COUNT	5
static bool tegra12x_stable_hotreset_check(u32 stat_reg, u32 *stat)
{
	int i;
	u32 cur_stat;
	u32 prv_stat;
	unsigned long flags;

	spin_lock_irqsave(&tegra12x_powergate_lock, flags);
	prv_stat = mc_read(stat_reg);
	for (i = 0; i < HOTRESET_READ_COUNT; i++) {
		cur_stat = mc_read(stat_reg);
		if (cur_stat != prv_stat) {
			spin_unlock_irqrestore(&tegra12x_powergate_lock, flags);
			return false;
		}
	}
	*stat = cur_stat;
	spin_unlock_irqrestore(&tegra12x_powergate_lock, flags);
	return true;
}

int tegra12x_powergate_mc_enable(int id)
{
	return 0;
}

int tegra12x_powergate_mc_disable(int id)
{
	return 0;
}

int tegra12x_powergate_mc_flush(int id)
{
	u32 idx, rst_ctrl, rst_stat;
	u32 rst_ctrl_reg, rst_stat_reg;
	enum mc_client mcClientBit;
	unsigned long flags;
	bool ret;

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			tegra12x_pg_mc_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		if (mcClientBit < 32) {
			rst_ctrl_reg = MC_CLIENT_HOTRESET_CTRL;
			rst_stat_reg = MC_CLIENT_HOTRESET_STAT;
		} else {
			mcClientBit %= 32;
			rst_ctrl_reg = MC_CLIENT_HOTRESET_CTRL_1;
			rst_stat_reg = MC_CLIENT_HOTRESET_STAT_1;
		}

		spin_lock_irqsave(&tegra12x_powergate_lock, flags);

		rst_ctrl = mc_read(rst_ctrl_reg);
		rst_ctrl |= (1 << mcClientBit);
		mc_write(rst_ctrl, rst_ctrl_reg);

		spin_unlock_irqrestore(&tegra12x_powergate_lock, flags);

		do {
			udelay(10);
			rst_stat = 0;
			ret = tegra12x_stable_hotreset_check(rst_stat_reg, &rst_stat);
			if (!ret)
				continue;
		} while (!(rst_stat & (1 << mcClientBit)));
	}

	return 0;
}

int tegra12x_powergate_mc_flush_done(int id)
{
	u32 idx, rst_ctrl, rst_ctrl_reg;
	enum mc_client mcClientBit;
	unsigned long flags;

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mcClientBit =
			tegra12x_pg_mc_info[id].hot_reset_clients[idx];
		if (mcClientBit == MC_CLIENT_LAST)
			break;

		if (mcClientBit < 32)
			rst_ctrl_reg = MC_CLIENT_HOTRESET_CTRL;
		else {
			mcClientBit %= 32;
			rst_ctrl_reg = MC_CLIENT_HOTRESET_CTRL_1;
		}

		spin_lock_irqsave(&tegra12x_powergate_lock, flags);

		rst_ctrl = mc_read(rst_ctrl_reg);
		rst_ctrl &= ~(1 << mcClientBit);
		mc_write(rst_ctrl, rst_ctrl_reg);

		spin_unlock_irqrestore(&tegra12x_powergate_lock, flags);
	}

	wmb();

	return 0;
}

static int tegra12x_gpu_powergate(int id, struct powergate_partition_info *pg_info)
{
	int ret;

	/* If first clk_ptr is null, fill clk info for the partition */
	if (!pg_info->clk_info[0].clk_ptr)
		get_clk_info(pg_info);

	tegra_powergate_mc_flush(id);

	udelay(10);

	/* enable clamp */
	pmc_write(0x1, PMC_GPU_RG_CNTRL_0);

	udelay(10);

	powergate_partition_assert_reset(pg_info);

	udelay(10);

	/*
	 * GPCPLL is already disabled before entering this function; reference
	 * clocks are enabled until now - disable them just before rail gating
	 */
	partition_clk_disable(pg_info);

	udelay(10);

	if (gpu_rail && tegra_powergate_is_powered(id)) {
		ret = tegra_dvfs_rail_power_down(gpu_rail);
		if (ret)
			goto err_power_off;
	} else
		pr_info("No GPU regulator?\n");

	return 0;

err_power_off:
	WARN(1, "Could not Railgate Partition %d", id);
	return ret;
}

static int tegra12x_gpu_unpowergate(int id,
	struct powergate_partition_info *pg_info)
{
	int ret = 0;
	bool first = false;

	if (!gpu_rail) {
		gpu_rail = tegra_dvfs_get_rail_by_name("vdd_gpu");
		if (IS_ERR_OR_NULL(gpu_rail)) {
			WARN(1, "No GPU regulator?\n");
			goto err_power;
		}
		first = true;
	} else {
		ret = tegra_dvfs_rail_power_up(gpu_rail);
		if (ret)
			goto err_power;
	}

	/* If first clk_ptr is null, fill clk info for the partition */
	if (!pg_info->clk_info[0].clk_ptr)
		get_clk_info(pg_info);

	/*
	 * GPU reference clocks are initially enabled - skip clock enable if
	 * 1st unpowergate, and in any case leave reference clock enabled on
	 * exit. GPCPLL is still disabled, and will be enabled by driver.
	 */
	if (!first) {
		/* Un-Powergating fails if all clks are not enabled */
		ret = partition_clk_enable(pg_info);
		if (ret)
			goto err_clk_on;
	}

	udelay(10);

	powergate_partition_assert_reset(pg_info);

	udelay(10);

	/* disable clamp */
	pmc_write(0, PMC_GPU_RG_CNTRL_0);

	udelay(10);

	powergate_partition_deassert_reset(pg_info);

	udelay(10);

	tegra_powergate_mc_flush_done(id);

	udelay(10);

	return 0;

err_clk_on:
	powergate_module(id);
err_power:
	WARN(1, "Could not Un-Railgate %d", id);
	return ret;
}

static atomic_t ref_count_dispa = ATOMIC_INIT(0);
static atomic_t ref_count_dispb = ATOMIC_INIT(0);
static atomic_t ref_count_venc = ATOMIC_INIT(0);

#define CHECK_RET(x)			\
	do {				\
		ret = (x);		\
		if (ret != 0)		\
			return ret;	\
	} while (0)


static inline int tegra12x_powergate(int id)
{
	if (tegra_powergate_is_powered(id))
		return tegra1xx_powergate(id,
			&tegra12x_powergate_partition_info[id]);
	return 0;
}

static inline int tegra12x_unpowergate(int id)
{
	if (!tegra_powergate_is_powered(id))
		return tegra1xx_unpowergate(id,
			&tegra12x_powergate_partition_info[id]);
	return 0;
}

static int tegra12x_disp_powergate(int id)
{
	int ret = 0;
	int ref_counta = atomic_read(&ref_count_dispa);
	int ref_countb = atomic_read(&ref_count_dispb);
	int ref_countve = atomic_read(&ref_count_venc);

	if (id == TEGRA_POWERGATE_DISA) {
		ref_counta = atomic_dec_return(&ref_count_dispa);
		WARN_ONCE(ref_counta < 0, "DISPA ref count underflow");
	} else if (id == TEGRA_POWERGATE_DISB) {
		if (ref_countb > 0)
			ref_countb = atomic_dec_return(&ref_count_dispb);
		if (ref_countb <= 0)
			CHECK_RET(tegra12x_powergate(TEGRA_POWERGATE_DISB));
	}

	if ((ref_counta <= 0) && (ref_countb <= 0) && (ref_countve <= 0)) {
		CHECK_RET(tegra12x_powergate(TEGRA_POWERGATE_SOR));
		CHECK_RET(tegra12x_powergate(TEGRA_POWERGATE_DISA));
	}
	return ret;
}

static int tegra12x_disp_unpowergate(int id)
{
	int ret;

	/* always unpowergate dispA and SOR partition */
	CHECK_RET(tegra12x_unpowergate(TEGRA_POWERGATE_DISA));
	CHECK_RET(tegra12x_unpowergate(TEGRA_POWERGATE_SOR));

	if (id == TEGRA_POWERGATE_DISA)
		atomic_inc(&ref_count_dispa);
	else if (id == TEGRA_POWERGATE_DISB) {
		atomic_inc(&ref_count_dispb);
		ret = tegra12x_unpowergate(TEGRA_POWERGATE_DISB);
	}

	return ret;
}

static int tegra12x_venc_powergate(int id)
{
	int ret = 0;
	int ref_count = atomic_read(&ref_count_venc);

	if (!TEGRA_IS_VENC_POWERGATE_ID(id))
		return -EINVAL;

	ref_count = atomic_dec_return(&ref_count_venc);

	if (ref_count > 0)
		return ret;

	if (ref_count <= 0) {
		CHECK_RET(tegra12x_powergate(id));
		CHECK_RET(tegra12x_disp_powergate(id));
	}

	return ret;
}

static int tegra12x_venc_unpowergate(int id)
{
	int ret = 0;

	if (!TEGRA_IS_VENC_POWERGATE_ID(id))
		return -EINVAL;

	CHECK_RET(tegra12x_disp_unpowergate(id));

	atomic_inc(&ref_count_venc);
	CHECK_RET(tegra12x_unpowergate(id));

	return ret;
}

int tegra12x_powergate_partition(int id)
{
	int ret;

	if (TEGRA_IS_GPU_POWERGATE_ID(id)) {
		ret = tegra12x_gpu_powergate(id,
			&tegra12x_powergate_partition_info[id]);
	} else if (TEGRA_IS_DISP_POWERGATE_ID(id))
		ret = tegra12x_disp_powergate(id);
	else if (id == TEGRA_POWERGATE_CRAIL)
		ret = tegra_powergate_set(id, false);
	else if (id == TEGRA_POWERGATE_VENC)
		ret = tegra12x_venc_powergate(id);
	else {
		/* call common power-gate API for t1xx */
		ret = tegra1xx_powergate(id,
			&tegra12x_powergate_partition_info[id]);
	}

	return ret;
}

int tegra12x_unpowergate_partition(int id)
{
	int ret;

	if (TEGRA_IS_GPU_POWERGATE_ID(id)) {
		ret = tegra12x_gpu_unpowergate(id,
			&tegra12x_powergate_partition_info[id]);
	} else if (TEGRA_IS_DISP_POWERGATE_ID(id))
		ret = tegra12x_disp_unpowergate(id);
	else if (id == TEGRA_POWERGATE_CRAIL)
		ret = tegra_powergate_set(id, true);
	else if (id == TEGRA_POWERGATE_VENC)
		ret = tegra12x_venc_unpowergate(id);
	else {
		ret = tegra1xx_unpowergate(id,
			&tegra12x_powergate_partition_info[id]);
	}

	return ret;
}

int tegra12x_powergate_partition_with_clk_off(int id)
{
	BUG_ON(TEGRA_IS_GPU_POWERGATE_ID(id));

	return tegraxx_powergate_partition_with_clk_off(id,
		&tegra12x_powergate_partition_info[id]);
}

int tegra12x_unpowergate_partition_with_clk_on(int id)
{
	BUG_ON(TEGRA_IS_GPU_POWERGATE_ID(id));

	return tegraxx_unpowergate_partition_with_clk_on(id,
		&tegra12x_powergate_partition_info[id]);
}

const char *tegra12x_get_powergate_domain_name(int id)
{
	return tegra12x_powergate_partition_info[id].name;
}

spinlock_t *tegra12x_get_powergate_lock(void)
{
	return &tegra12x_powergate_lock;
}

bool tegra12x_powergate_skip(int id)
{
	switch (id) {
#ifdef CONFIG_ARCH_TEGRA_HAS_SATA
	case TEGRA_POWERGATE_SATA:
#endif
		return true;

	default:
		return false;
	}
}

bool tegra12x_powergate_is_powered(int id)
{
	u32 status = 0;

	if (TEGRA_IS_GPU_POWERGATE_ID(id)) {
		if (gpu_rail)
			return tegra_dvfs_is_rail_up(gpu_rail);
	} else {
		status = pmc_read(PWRGATE_STATUS) & (1 << id);
		return !!status;
	}
	return status;
}

static struct powergate_ops tegra12x_powergate_ops = {
	.soc_name = "tegra12x",

	.num_powerdomains = TEGRA_NUM_POWERGATE,

	.get_powergate_lock = tegra12x_get_powergate_lock,
	.get_powergate_domain_name = tegra12x_get_powergate_domain_name,

	.powergate_partition = tegra12x_powergate_partition,
	.unpowergate_partition = tegra12x_unpowergate_partition,

	.powergate_partition_with_clk_off =  tegra12x_powergate_partition_with_clk_off,
	.unpowergate_partition_with_clk_on = tegra12x_unpowergate_partition_with_clk_on,

	.powergate_mc_enable = tegra12x_powergate_mc_enable,
	.powergate_mc_disable = tegra12x_powergate_mc_disable,

	.powergate_mc_flush = tegra12x_powergate_mc_flush,
	.powergate_mc_flush_done = tegra12x_powergate_mc_flush_done,

	.powergate_skip = tegra12x_powergate_skip,

	.powergate_is_powered = tegra12x_powergate_is_powered,
};

struct powergate_ops *tegra12x_powergate_init_chip_support(void)
{
	if (tegra_powergate_is_powered(TEGRA_POWERGATE_VENC))
		atomic_set(&ref_count_venc, 1);

	return &tegra12x_powergate_ops;
}
