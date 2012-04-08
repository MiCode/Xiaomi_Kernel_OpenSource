/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_ACPUCLOCK_KRAIT_H
#define __ARCH_ARM_MACH_MSM_ACPUCLOCK_KRAIT_H

#define STBY_KHZ		1

#define BW_MBPS(_bw) \
	{ \
		.vectors = (struct msm_bus_vectors[]){ \
			{\
				.src = MSM_BUS_MASTER_AMPSS_M0, \
				.dst = MSM_BUS_SLAVE_EBI_CH0, \
				.ib = (_bw) * 1000000UL, \
			}, \
			{ \
				.src = MSM_BUS_MASTER_AMPSS_M1, \
				.dst = MSM_BUS_SLAVE_EBI_CH0, \
				.ib = (_bw) * 1000000UL, \
			}, \
		}, \
		.num_paths = 2, \
	}

/**
 * src_id - Clock source IDs.
 */
enum src_id {
	PLL_0 = 0,
	HFPLL,
	QSB,
};

/**
 * enum pvs - IDs to distinguish between CPU frequency tables.
 */
enum pvs {
	PVS_SLOW = 0,
	PVS_NOMINAL,
	PVS_FAST,
	PVS_UNKNOWN,
	NUM_PVS
};

/**
 * enum scalables - IDs of frequency scalable hardware blocks.
 */
enum scalables {
	CPU0 = 0,
	CPU1,
	CPU2,
	CPU3,
	L2,
};


/**
 * enum hfpll_vdd_level - IDs of HFPLL voltage levels.
 */
enum hfpll_vdd_levels {
	HFPLL_VDD_NONE,
	HFPLL_VDD_LOW,
	HFPLL_VDD_NOM,
	NUM_HFPLL_VDD
};

/**
 * enum vregs - IDs of voltage regulators.
 */
enum vregs {
	VREG_CORE,
	VREG_MEM,
	VREG_DIG,
	VREG_HFPLL_A,
	VREG_HFPLL_B,
	NUM_VREG
};

/**
 * struct vreg - Voltage regulator data.
 * @name: Name of requlator.
 * @max_vdd: Limit the maximum-settable voltage.
 * @rpm_vreg_id: ID to use with rpm_vreg_*() APIs.
 * @reg: Regulator handle.
 * @cur_vdd: Last-set voltage in uV.
 * @peak_ua: Maximum current draw expected in uA.
 */
struct vreg {
	const char name[15];
	const int max_vdd;
	const int peak_ua;
	const int rpm_vreg_voter;
	const int rpm_vreg_id;
	struct regulator *reg;
	int cur_vdd;
};

/**
 * struct core_speed - Clock tree and configuration parameters.
 * @khz: Clock rate in KHz.
 * @src: Clock source ID.
 * @pri_src_sel: Input to select on the primary MUX.
 * @sec_src_sel: Input to select on the secondary MUX.
 * @pll_l_val: HFPLL "L" value to be applied when an HFPLL source is selected.
 */
struct core_speed {
	const unsigned long khz;
	const int src;
	const u32 pri_src_sel;
	const u32 sec_src_sel;
	const u32 pll_l_val;
};

/**
 * struct l2_level - L2 clock rate and associated voltage and b/w requirements.
 * @speed: L2 clock configuration.
 * @vdd_dig: vdd_dig voltage in uV.
 * @vdd_mem: vdd_mem voltage in uV.
 * @bw_level: Bandwidth performance level number.
 */
struct l2_level {
	const struct core_speed speed;
	const int vdd_dig;
	const int vdd_mem;
	const unsigned int bw_level;
};

/**
 * struct acpu_level - CPU clock rate and L2 rate and voltage requirements.
 * @use_for_scaling: Flag indicating whether or not the level should be used.
 * @speed: CPU clock configuration.
 * @l2_level: L2 configuration to use.
 * @vdd_core: CPU core voltage in uV.
 */
struct acpu_level {
	const int use_for_scaling;
	const struct core_speed speed;
	const struct l2_level *l2_level;
	const int vdd_core;
};

/**
 * struct hfpll_data - Descriptive data of HFPLL hardware.
 * @mode_offset: Mode register offset from base address.
 * @l_offset: "L" value register offset from base address.
 * @m_offset: "M" value register offset from base address.
 * @n_offset: "N" value register offset from base address.
 * @config_offset: Configuration register offset from base address.
 * @config_val: Value to initialize the @config_offset register to.
 * @vdd: voltage requirements for each VDD level.
 */
struct hfpll_data {
	const u32 mode_offset;
	const u32 l_offset;
	const u32 m_offset;
	const u32 n_offset;
	const u32 config_offset;
	const u32 config_val;
	const u32 low_vdd_l_max;
	const int vdd[NUM_HFPLL_VDD];
};

/**
 * struct scalable - Register locations and state associated with a scalable HW.
 * @hfpll_phys_base: Physical base address of HFPLL register.
 * @hfpll_base: Virtual base address of HFPLL registers.
 * @aux_clk_sel_addr: Virtual address of auxiliary MUX.
 * @aux_clk_sel: Auxiliary mux input to select at boot.
 * @l2cpmr_iaddr: Indirect address of the CPMR MUX/divider CP15 register.
 * @hfpll_data: Descriptive data of HFPLL hardware.
 * @cur_speed: Pointer to currently-set speed.
 * @l2_vote: L2 performance level vote associate with the current CPU speed.
 * @vreg: Array of voltage regulators needed by the scalable.
 */
struct scalable {
	const u32 hfpll_phys_base;
	void __iomem *hfpll_base;
	void __iomem *aux_clk_sel_addr;
	const u32 aux_clk_sel;
	const u32 l2cpmr_iaddr;
	const struct hfpll_data *hfpll_data;
	const struct core_speed *cur_speed;
	const struct l2_level *l2_vote;
	struct vreg vreg[NUM_VREG];
};

/**
 * struct acpuclk_krait_params - SoC specific driver parameters.
 * @scalable: Array of scalables.
 * @pvs_acpu_freq_tbl: Array of CPU frequency tables.
 * @l2_freq_tbl: L2 frequency table.
 * @l2_freq_tbl_size: Number of rows in @l2_freq_tbl.
 * @qfprom_phys_base: Physical base address of QFPROM.
 * @bus_scale_data: MSM bus driver parameters.
 */
struct acpuclk_krait_params {
	struct scalable *scalable;
	const struct acpu_level *pvs_acpu_freq_tbl[NUM_PVS];
	const struct l2_level *l2_freq_tbl;
	const size_t l2_freq_tbl_size;
	const u32 qfprom_phys_base;
	struct msm_bus_scale_pdata *bus_scale_data;
};

/**
 * acpuclk_krait_init - Initialize the Krait CPU clock driver give SoC params.
 */
extern int acpuclk_krait_init(struct device *dev,
			      const struct acpuclk_krait_params *params);

#endif
