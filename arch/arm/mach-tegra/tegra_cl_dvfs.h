/*
 * arch/arm/mach-tegra/tegra_cl_dvfs.h
 *
 * Copyright (C) 2012 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _TEGRA_CL_DVFS_H_
#define _TEGRA_CL_DVFS_H_

struct tegra_cl_dvfs;

#define MAX_CL_DVFS_VOLTAGES		33

enum tegra_cl_dvfs_force_mode {
	TEGRA_CL_DVFS_FORCE_NONE = 0,
	TEGRA_CL_DVFS_FORCE_FIXED = 1,
	TEGRA_CL_DVFS_FORCE_AUTO = 2,
};

enum tegra_cl_dvfs_pmu_if {
	TEGRA_CL_DVFS_PMU_I2C,
	TEGRA_CL_DVFS_PMU_PWM,
};

/* CL DVFS plaform flags*/
/* set if output to PMU can be disabled only between I2C transactions */
#define TEGRA_CL_DVFS_FLAGS_I2C_WAIT_QUIET	(0x1UL << 0)

struct tegra_cl_dvfs_cfg_param {
	unsigned long	sample_rate;

	enum tegra_cl_dvfs_force_mode force_mode;
	u8		cf;
	u8		ci;
	s8		cg;
	bool		cg_scale;

	u8		droop_cut_value;
	u8		droop_restore_ramp;
	u8		scale_out_ramp;
};

struct voltage_reg_map {
	u8		reg_value;
	int		reg_uV;
};

struct tegra_cl_dvfs_platform_data {
	const char *dfll_clk_name;
	u32 flags;

	enum tegra_cl_dvfs_pmu_if pmu_if;
	union {
		struct {
			unsigned long		fs_rate;
			unsigned long		hs_rate; /* if 0 - no hs mode */
			u8			hs_master_code;
			u8			reg;
			u16			slave_addr;
			bool			addr_10;
		} pmu_i2c;
		struct {
			/* FIXME: to be defined */
		} pmu_pwm;
	} u;

	struct voltage_reg_map	*vdd_map;
	int			vdd_map_size;
	int			pmu_undershoot_gb;

	struct tegra_cl_dvfs_cfg_param		*cfg_param;
};

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
int tegra_init_cl_dvfs(void);
int tegra_cl_dvfs_debug_init(struct clk *dfll_clk);
void tegra_cl_dvfs_resume(struct tegra_cl_dvfs *cld);

void tegra_cl_dvfs_disable(struct tegra_cl_dvfs *cld);
int tegra_cl_dvfs_enable(struct tegra_cl_dvfs *cld);
int tegra_cl_dvfs_lock(struct tegra_cl_dvfs *cld);
int tegra_cl_dvfs_unlock(struct tegra_cl_dvfs *cld);
int tegra_cl_dvfs_request_rate(struct tegra_cl_dvfs *cld, unsigned long rate);
unsigned long tegra_cl_dvfs_request_get(struct tegra_cl_dvfs *cld);
#else
static inline int tegra_init_cl_dvfs(void)
{ return -ENOSYS; }
static inline int tegra_cl_dvfs_debug_init(struct clk *dfll_clk)
{ return -ENOSYS; }
static inline void tegra_cl_dvfs_resume(struct tegra_cl_dvfs *cld)
{}

static inline void tegra_cl_dvfs_disable(struct tegra_cl_dvfs *cld)
{}
static inline int tegra_cl_dvfs_enable(struct tegra_cl_dvfs *cld)
{ return -ENOSYS; }
static inline int tegra_cl_dvfs_lock(struct tegra_cl_dvfs *cld)
{ return -ENOSYS; }
static inline int tegra_cl_dvfs_unlock(struct tegra_cl_dvfs *cld)
{ return -ENOSYS; }
static inline int tegra_cl_dvfs_request_rate(
	struct tegra_cl_dvfs *cld, unsigned long rate)
{ return -ENOSYS; }
static inline unsigned long tegra_cl_dvfs_request_get(struct tegra_cl_dvfs *cld)
{ return 0; }
#endif

#endif
