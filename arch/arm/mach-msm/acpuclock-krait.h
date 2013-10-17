/*
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#define L2(x) (x)
#define BW_MBPS(_bw) \
	{ \
		.vectors = (struct msm_bus_vectors[]){ \
			{\
				.src = MSM_BUS_MASTER_AMPSS_M0, \
				.dst = MSM_BUS_SLAVE_EBI_CH0, \
				.ib = (_bw) * 1000000ULL, \
			}, \
			{ \
				.src = MSM_BUS_MASTER_AMPSS_M1, \
				.dst = MSM_BUS_SLAVE_EBI_CH0, \
				.ib = (_bw) * 1000000ULL, \
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
	PLL_8,
	NUM_SRC_ID
};

/**
 * enum pvs - IDs to distinguish between CPU frequency tables.
 */
enum pvs {
	PVS_SLOW = 0,
	PVS_NOMINAL = 1,
	PVS_FAST = 3,
	PVS_FASTER = 4,
	NUM_PVS = 16
};

/**
 * The maximum number of PVS revisions.
 */
#define NUM_PVS_REVS (4)

/**
 * The maximum number of speed bins.
 */
#define NUM_SPEED_BINS (16)

/**
 * enum scalables - IDs of frequency scalable hardware blocks.
 */
enum scalables {
	CPU0 = 0,
	CPU1,
	CPU2,
	CPU3,
	L2,
	MAX_SCALABLES
};


/**
 * enum hfpll_vdd_level - IDs of HFPLL voltage levels.
 */
enum hfpll_vdd_levels {
	HFPLL_VDD_NONE,
	HFPLL_VDD_LOW,
	HFPLL_VDD_NOM,
	HFPLL_VDD_HIGH,
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
 * @reg: Regulator handle.
 * @rpm_reg: RPM Regulator handle.
 * @cur_vdd: Last-set voltage in uV.
 * @cur_ua: Last-set current in uA.
 */
struct vreg {
	const char *name;
	const int max_vdd;
	struct regulator *reg;
	struct rpm_regulator *rpm_reg;
	int cur_vdd;
	int cur_ua;
};

/**
 * struct core_speed - Clock tree and configuration parameters.
 * @khz: Clock rate in KHz.
 * @src: Clock source ID.
 * @pri_src_sel: Input to select on the primary MUX.
 * @pll_l_val: HFPLL "L" value to be applied when an HFPLL source is selected.
 */
struct core_speed {
	unsigned long khz;
	int src;
	u32 pri_src_sel;
	u32 pll_l_val;
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
 * @ua_core: CPU core current consumption in uA.
 * @avsdscr_setting: AVS DSCR configuration.
 */
struct acpu_level {
	const int use_for_scaling;
	const struct core_speed speed;
	unsigned int l2_level;
	int vdd_core;
	int ua_core;
	unsigned int avsdscr_setting;
};

/**
 * struct hfpll_data - Descriptive data of HFPLL hardware.
 * @mode_offset: Mode register offset from base address.
 * @l_offset: "L" value register offset from base address.
 * @m_offset: "M" value register offset from base address.
 * @n_offset: "N" value register offset from base address.
 * @config_offset: Configuration register offset from base address.
 * @config_val: Value to initialize the @config_offset register to.
 * @has_user_reg: Indicates the presence of an addition config register.
 * @user_offset: User register offset from base address, if applicable.
 * @user_val: Value to initialize the @user_offset register to.
 * @user_vco_mask: Bit in the @user_offset to enable high-frequency VCO mode.
 * @has_droop_ctl: Indicates the presence of a voltage droop controller.
 * @has_lock_status: Indicates the presence of a lock status bit.
 * @droop_offset: Droop controller register offset from base address.
 * @droop_val: Value to initialize the @config_offset register to.
 * @status_offset: PLL status register offset.
 * @low_vdd_l_max: Maximum "L" value supported at HFPLL_VDD_LOW.
 * @nom_vdd_l_max: Maximum "L" value supported at HFPLL_VDD_NOM.
 * @low_vco_l_max: Maximum "L" value supported in low-frequency VCO mode.
 * @vdd: voltage requirements for each VDD level for the L2 PLL.
 */
struct hfpll_data {
	const u32 mode_offset;
	const u32 l_offset;
	const u32 m_offset;
	const u32 n_offset;
	const u32 config_offset;
	const u32 config_val;
	const bool has_user_reg;
	const u32 user_offset;
	const u32 user_val;
	const u32 user_vco_mask;
	const bool has_droop_ctl;
	const bool has_lock_status;
	const u32 droop_offset;
	const u32 droop_val;
	const u32 status_offset;
	u32 low_vdd_l_max;
	u32 nom_vdd_l_max;
	const u32 low_vco_l_max;
	const int vdd[NUM_HFPLL_VDD];
};

/**
 * struct scalable - Register locations and state associated with a scalable HW.
 * @hfpll_phys_base: Physical base address of HFPLL register.
 * @hfpll_base: Virtual base address of HFPLL registers.
 * @aux_clk_sel_phys: Physical address of auxiliary MUX.
 * @aux_clk_sel: Auxiliary mux input to select at boot.
 * @sec_clk_sel: Secondary mux input to select at boot.
 * @l2cpmr_iaddr: Indirect address of the CPMR MUX/divider CP15 register.
 * @cur_speed: Pointer to currently-set speed.
 * @l2_vote: L2 performance level vote associate with the current CPU speed.
 * @vreg: Array of voltage regulators needed by the scalable.
 * @initialized: Flag set to true when per_cpu_init() has been called.
 * @avs_enabled: True if avs is enabled for the scalabale. False otherwise.
 */
struct scalable {
	const phys_addr_t hfpll_phys_base;
	void __iomem *hfpll_base;
	const phys_addr_t aux_clk_sel_phys;
	const u32 aux_clk_sel;
	const u32 sec_clk_sel;
	const u32 l2cpmr_iaddr;
	const struct core_speed *cur_speed;
	unsigned int l2_vote;
	struct vreg vreg[NUM_VREG];
	bool initialized;
	bool avs_enabled;
};

/**
 * struct bin_info - Hardware speed and voltage binning info.
 * @speed_valid: @speed field is valid
 * @pvs_valid: @pvs field is valid
 * @speed: Speed bin ID
 * @pvs: PVS bin ID
 * @pvs_rev: PVS revision ID
 */
struct bin_info {
	bool speed_valid;
	bool pvs_valid;
	int speed;
	int pvs;
	int pvs_rev;
};

/**
 * struct pvs_table - CPU performance level table and size.
 * @table: CPU performance level table
 * @size: sizeof(@table)
 * @boost_uv: Voltage boost amount
 */
struct pvs_table {
	struct acpu_level *table;
	size_t size;
	int boost_uv;
};

/**
 * struct acpuclk_krait_params - SoC specific driver parameters.
 * @scalable: Array of scalables.
 * @scalable_size: Size of @scalable.
 * @hfpll_data: HFPLL configuration data.
 * @pvs_tables: 2D array of CPU frequency tables.
 * @l2_freq_tbl: L2 frequency table.
 * @l2_freq_tbl_size: Size of @l2_freq_tbl.
 * @pte_efuse_phys: Physical address of PTE EFUSE.
 * @get_bin_info: Function to populate bin_info from pte_efuse.
 * @bus_scale: MSM bus driver parameters.
 * @stby_khz: KHz value corresponding to an always-on clock source.
 */
struct acpuclk_krait_params {
	struct scalable *scalable;
	size_t scalable_size;
	struct hfpll_data *hfpll_data;
	struct pvs_table (*pvs_tables)[NUM_SPEED_BINS][NUM_PVS];
	struct l2_level *l2_freq_tbl;
	size_t l2_freq_tbl_size;
	phys_addr_t pte_efuse_phys;
	void (*get_bin_info)(void __iomem *base, struct bin_info *bin);
	struct msm_bus_scale_pdata *bus_scale;
	unsigned long stby_khz;
};

/**
 * struct drv_data - Driver state
 * @acpu_freq_tbl: CPU frequency table.
 * @l2_freq_tbl: L2 frequency table.
 * @scalable: Array of scalables (CPUs and L2).
 * @hfpll_data: High-frequency PLL data.
 * @bus_perf_client: Bus driver client handle.
 * @bus_scale: Bus driver scaling data.
 * @boost_uv: Voltage boost amount
 * @speed_bin: Speed bin ID.
 * @pvs_bin: PVS bin ID.
 * @pvs_bin: PVS revision ID.
 * @dev: Device.
 */
struct drv_data {
	struct acpu_level *acpu_freq_tbl;
	const struct l2_level *l2_freq_tbl;
	struct scalable *scalable;
	struct hfpll_data *hfpll_data;
	u32 bus_perf_client;
	struct msm_bus_scale_pdata *bus_scale;
	int boost_uv;
	int speed_bin;
	int pvs_bin;
	int pvs_rev;
	struct device *dev;
};

/**
 * struct acpuclk_platform_data - PMIC configuration data.
 * @uses_pm8917: Boolean indicates presence of pm8917.
 */
struct acpuclk_platform_data {
	bool uses_pm8917;
};

/**
 * get_krait_bin_format_a - Populate bin_info from a 'Format A' pte_efuse
 */
void __init get_krait_bin_format_a(void __iomem *base, struct bin_info *bin);

/**
 * get_krait_bin_format_b - Populate bin_info from a 'Format B' pte_efuse
 */
void __init get_krait_bin_format_b(void __iomem *base, struct bin_info *bin);

/**
 * acpuclk_krait_init - Initialize the Krait CPU clock driver give SoC params.
 */
extern int acpuclk_krait_init(struct device *dev,
			      const struct acpuclk_krait_params *params);

#ifdef CONFIG_DEBUG_FS
/**
 * acpuclk_krait_debug_init - Initialize debugfs interface.
 */
extern void __init acpuclk_krait_debug_init(struct drv_data *drv);
#else
static inline void acpuclk_krait_debug_init(void) { }
#endif

#endif
