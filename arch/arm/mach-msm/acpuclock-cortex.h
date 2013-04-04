/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

/* Corner type vreg VDD values */
#define LVL_NONE        RPM_REGULATOR_CORNER_NONE
#define LVL_LOW         RPM_REGULATOR_CORNER_SVS_SOC
#define LVL_NOM         RPM_REGULATOR_CORNER_NORMAL
#define LVL_HIGH        RPM_REGULATOR_CORNER_SUPER_TURBO

enum clk_src {
	CXO,
	PLL0,
	ACPUPLL,
	NUM_SRC,
};

struct src_clock {
	struct clk *clk;
	const char *name;
};

struct clkctl_acpu_speed {
	bool use_for_scaling;
	unsigned int khz;
	int src;
	unsigned int src_sel;
	unsigned int src_div;
	unsigned int vdd_cpu;
	unsigned int vdd_mem;
	unsigned int bw_level;
};

struct acpuclk_reg_data {
	u32 cfg_src_mask;
	u32 cfg_src_shift;
	u32 cfg_div_mask;
	u32 cfg_div_shift;
	u32 update_mask;
	u32 poll_mask;
};

struct acpuclk_drv_data {
	struct mutex			lock;
	struct clkctl_acpu_speed	*freq_tbl;
	struct clkctl_acpu_speed	*current_speed;
	struct msm_bus_scale_pdata	*bus_scale;
	void __iomem			*apcs_rcg_config;
	void __iomem			*apcs_rcg_cmd;
	void __iomem			*apcs_cpu_pwr_ctl;
	struct regulator		*vdd_cpu;
	unsigned long			vdd_max_cpu;
	struct regulator		*vdd_mem;
	unsigned long			vdd_max_mem;
	struct src_clock		src_clocks[NUM_SRC];
	struct acpuclk_reg_data		reg_data;
	unsigned long                   power_collapse_khz;
	unsigned long                   wait_for_irq_khz;
};

/* Instantaneous bandwidth requests in MB/s. */
#define BW_MBPS(_bw) \
	{ \
		.vectors = &(struct msm_bus_vectors){ \
			.src = MSM_BUS_MASTER_AMPSS_M0, \
			.dst = MSM_BUS_SLAVE_EBI_CH0, \
			.ib = (_bw) * 1000000ULL, \
			.ab = 0, \
		}, \
		.num_paths = 1, \
	}

int __init acpuclk_cortex_init(struct platform_device *pdev,
	struct acpuclk_drv_data *data);

