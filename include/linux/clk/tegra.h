/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LINUX_CLK_TEGRA_H_
#define __LINUX_CLK_TEGRA_H_

#include <linux/clk.h>

/*
 * Tegra CPU clock and reset control ops
 *
 * wait_for_reset:
 *	keep waiting until the CPU in reset state
 * put_in_reset:
 *	put the CPU in reset state
 * out_of_reset:
 *	release the CPU from reset state
 * enable_clock:
 *	CPU clock un-gate
 * disable_clock:
 *	CPU clock gate
 * rail_off_ready:
 *	CPU is ready for rail off
 * suspend:
 *	save the clock settings when CPU go into low-power state
 * resume:
 *	restore the clock settings when CPU exit low-power state
 */
struct tegra_cpu_car_ops {
	void (*wait_for_reset)(u32 cpu);
	void (*put_in_reset)(u32 cpu);
	void (*out_of_reset)(u32 cpu);
	void (*enable_clock)(u32 cpu);
	void (*disable_clock)(u32 cpu);
#ifdef CONFIG_PM_SLEEP
	bool (*rail_off_ready)(void);
	void (*suspend)(void);
	void (*resume)(void);
#endif
};

extern struct tegra_cpu_car_ops *tegra_cpu_car_ops;

static inline void tegra_wait_cpu_in_reset(u32 cpu)
{
	if (WARN_ON(!tegra_cpu_car_ops->wait_for_reset))
		return;

	tegra_cpu_car_ops->wait_for_reset(cpu);
}

static inline void tegra_put_cpu_in_reset(u32 cpu)
{
	if (WARN_ON(!tegra_cpu_car_ops->put_in_reset))
		return;

	tegra_cpu_car_ops->put_in_reset(cpu);
}

static inline void tegra_cpu_out_of_reset(u32 cpu)
{
	if (WARN_ON(!tegra_cpu_car_ops->out_of_reset))
		return;

	tegra_cpu_car_ops->out_of_reset(cpu);
}

static inline void tegra_enable_cpu_clock(u32 cpu)
{
	if (WARN_ON(!tegra_cpu_car_ops->enable_clock))
		return;

	tegra_cpu_car_ops->enable_clock(cpu);
}

static inline void tegra_disable_cpu_clock(u32 cpu)
{
	if (WARN_ON(!tegra_cpu_car_ops->disable_clock))
		return;

	tegra_cpu_car_ops->disable_clock(cpu);
}

#ifdef CONFIG_PM_SLEEP
static inline bool tegra_cpu_rail_off_ready(void)
{
	if (WARN_ON(!tegra_cpu_car_ops->rail_off_ready))
		return false;

	return tegra_cpu_car_ops->rail_off_ready();
}

static inline void tegra_cpu_clock_suspend(void)
{
	if (WARN_ON(!tegra_cpu_car_ops->suspend))
		return;

	tegra_cpu_car_ops->suspend();
}

static inline void tegra_cpu_clock_resume(void)
{
	if (WARN_ON(!tegra_cpu_car_ops->resume))
		return;

	tegra_cpu_car_ops->resume();
}
#endif

enum tegra_clk_ex_param {
	TEGRA_CLK_VI_INP_SEL,
	TEGRA_CLK_DTV_INVERT,
	TEGRA_CLK_NAND_PAD_DIV2_ENB,
	TEGRA_CLK_PLLD_CSI_OUT_ENB,
	TEGRA_CLK_PLLD_DSI_OUT_ENB,
	TEGRA_CLK_PLLD_MIPI_MUX_SEL,
	TEGRA_CLK_DFLL_LOCK,
	TEGRA_CLK_SOR_CLK_SEL,
	TEGRA_CLK_MIPI_CSI_OUT_ENB,
};

void tegra_periph_reset_deassert(struct clk *c);
void tegra_periph_reset_assert(struct clk *c);

#ifdef CONFIG_COMMON_CLK
void tegra_clocks_init(void);
void tegra_clocks_apply_init_table(void);

static inline int tegra_dvfs_override_core_voltage(struct clk *c, int override_mv)
{
	return -EINVAL;
}
static inline int tegra_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	return -EINVAL;
}
#else
static inline void tegra_clocks_init(void)
{}
static inline void tegra_clocks_apply_init_table(void)
{}

struct dvfs;
struct notifier_block;

int tegra_dvfs_get_freqs(struct clk *c, unsigned long **freqs, int *num_freqs);
int tegra_dvfs_set_rate(struct clk *c, unsigned long rate);
int tegra_dvfs_override_core_voltage(struct clk *c, int override_mv);
int tegra_dvfs_predict_millivolts(struct clk *c, unsigned long rate);
int tegra_dvfs_set_fmax_at_vmin(struct clk *c, unsigned long f_max, int v_min);
unsigned long clk_get_rate_all_locked(struct clk *c);
int tegra_dvfs_rail_disable_by_name(const char *reg_id);
int tegra_register_clk_rate_notifier(struct clk *c, struct notifier_block *nb);
void tegra_unregister_clk_rate_notifier(
	struct clk *c, struct notifier_block *nb);
int tegra_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting);
int tegra_dvfs_use_alt_freqs_on_clk(struct clk *c, bool use_alt_freq);

/**
 * tegra_is_clk_enabled - get info if the clk is enabled or not
 * @clk: clock source
 *
 * Returns refcnt.
 */
int tegra_is_clk_enabled(struct clk *clk);

void tegra_cpu_user_cap_set(unsigned int speed_khz);

struct tegra_clk_export_ops {
	void		(*init)(void *data, unsigned long *rate, bool *state);
	int		(*enable)(void *data);
	void		(*disable)(void *data);
	int		(*set_rate)(void *data, unsigned long *rate);
	void		*data;
};

int tegra_clk_register_export_ops(struct clk *c,
				  struct tegra_clk_export_ops *ops);


#ifdef CONFIG_TEGRA_CLOCK_DEBUG_FUNC
int tegra_clk_set_max(struct clk *c, unsigned long rate);
#ifdef CONFIG_ARCH_TEGRA_12x_SOC
void tegra_gbus_round_pass_thru_enable(bool enable);
#else
static inline void tegra_gbus_round_pass_thru_enable(bool enable)
{}
#endif
#else
static inline int tegra_clk_set_max(struct clk *c, unsigned long rate) {
	return -ENOSYS;
}
static inline void tegra_gbus_round_pass_thru_enable(bool enable)
{}
#endif

#endif

#endif /* __LINUX_CLK_TEGRA_H_ */
