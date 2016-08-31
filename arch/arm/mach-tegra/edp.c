/*
 * arch/arm/mach-tegra/edp.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All Rights Reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/edp.h>
#include <linux/sysedp.h>
#include <linux/tegra-soc.h>
#include <linux/regulator/consumer.h>
#include <linux/tegra-fuse.h>

#include <mach/edp.h>

#include "dvfs.h"
#include "clock.h"
#include "cpu-tegra.h"
#include "common.h"

#define FREQ_STEP 12750000
#define OVERRIDE_DEFAULT 6000

#define GPU_FREQ_STEP 12000000

static struct tegra_edp_limits *edp_limits;
static int edp_limits_size;
static unsigned int regulator_cur;
/* Value to subtract from regulator current limit */
static unsigned int edp_reg_override_mA = OVERRIDE_DEFAULT;

static struct tegra_edp_limits *reg_idle_edp_limits;
static int reg_idle_cur;

static const unsigned int *system_edp_limits;

static struct tegra_system_edp_entry *power_edp_limits;
static int power_edp_limits_size;

/*
 * "Safe entry" to be used when no match for speedo_id /
 * regulator_cur is found; must be the last one
 */
static struct tegra_edp_limits edp_default_limits[] = {
	{85, {1000000, 1000000, 1000000, 1000000} },
};

static struct tegra_system_edp_entry power_edp_default_limits[] = {
	{0, 20, {1000000, 1000000, 1000000, 1000000} },
};

/* Constants for EDP calculations */
static const int temperatures[] = { /* degree celcius (C) */
	23, 40, 50, 60, 70, 74, 78, 82, 86, 90, 94, 98, 102,
};

static const int power_cap_levels[] = { /* milliwatts (mW) */
	  500,  1000,  1500,  2000,  2500,  3000,  3500,
	 4000,  4500,  5000,  5500,  6000,  6500,  7000,
	 7500,  8000,  8500,  9000,  9500, 10000, 10500,
	11000, 11500, 12000, 12500, 13000, 13500, 14000,
	14500, 15000, 15500, 16000, 16500, 17000
};

static struct tegra_edp_freq_voltage_table *freq_voltage_lut_saved;
static unsigned int freq_voltage_lut_size_saved;
static struct tegra_edp_freq_voltage_table *freq_voltage_lut;
static unsigned int freq_voltage_lut_size;

#ifdef CONFIG_TEGRA_GPU_EDP
static struct tegra_edp_gpu_limits *gpu_edp_limits;
static int gpu_edp_limits_size;
static int gpu_edp_thermal_idx;
static struct clk *gpu_cap_clk;
static DEFINE_MUTEX(gpu_edp_lock);

static struct tegra_edp_freq_voltage_table *freq_voltage_gpu_lut;
static unsigned int freq_voltage_gpu_lut_size;

static unsigned int gpu_regulator_cur;
/* Value to subtract from regulator current limit */
static unsigned int gpu_edp_reg_override_mA = OVERRIDE_DEFAULT;

static struct tegra_edp_gpu_limits gpu_edp_default_limits[] = {
	{85, 350000},
};

static const int gpu_temperatures[] = { /* degree celcius (C) */
	20, 50, 70, 75, 80, 85, 90, 95, 100, 105,
};
#endif

static inline s64 edp_pow(s64 val, int pwr)
{
	s64 retval = 1;

	while (val && pwr) {
		if (pwr & 1)
			retval *= val;
		pwr >>= 1;
		if (pwr)
			val *= val;
	}

	return retval;
}


#ifdef CONFIG_TEGRA_CPU_EDP_FIXED_LIMITS
static inline unsigned int edp_apply_fixed_limits(
				unsigned int in_freq_KHz,
				struct tegra_edp_cpu_leakage_params *params,
				unsigned int cur_effective,
				int temp_C, int n_cores_idx)
{
	unsigned int out_freq_KHz = in_freq_KHz;
	unsigned int max_cur, max_temp, max_freq;
	int i;

	/* Apply any additional fixed limits */
	for (i = 0; i < 8; i++) {
		max_cur = params->max_current_cap[i].max_cur;
		if (max_cur != 0 && cur_effective <= max_cur) {
			max_temp = params->max_current_cap[i].max_temp;
			if (max_temp != 0 && temp_C > max_temp) {
				max_freq = params->max_current_cap[i].
					max_freq[n_cores_idx];
				if (max_freq && max_freq < out_freq_KHz)
					out_freq_KHz = max_freq;
			}
		}
	}

	return out_freq_KHz;
}
#else
#define edp_apply_fixed_limits(freq, unused...)	(freq)
#endif

/*
 * Find the maximum frequency that results in dynamic and leakage current that
 * is less than the regulator current limit.
 * temp_C - valid or -EINVAL
 * power_mW - valid or -1 (infinite) or -EINVAL
 */
static unsigned int edp_calculate_maxf(
				struct tegra_edp_cpu_leakage_params *params,
				int temp_C, int power_mW, unsigned int cur_mA,
				int iddq_mA,
				int n_cores_idx)
{
	unsigned int voltage_mV, freq_KHz = 0;
	unsigned int cur_effective = regulator_cur - edp_reg_override_mA;
	int f, i, j, k;
	s64 leakage_mA, dyn_mA, leakage_calc_step;
	s64 leakage_mW, dyn_mW;

	/* If current limit is not specified, use max by default */
	cur_mA = cur_mA ? : cur_effective;

	for (f = freq_voltage_lut_size - 1; f >= 0; f--) {
		freq_KHz = freq_voltage_lut[f].freq / 1000;
		voltage_mV = freq_voltage_lut[f].voltage_mV;

		/* Constrain Volt-Temp */
		if (params->volt_temp_cap.temperature &&
		    temp_C > params->volt_temp_cap.temperature &&
		    params->volt_temp_cap.voltage_limit_mV &&
		    voltage_mV > params->volt_temp_cap.voltage_limit_mV)
			continue;

		/* Calculate leakage current */
		leakage_mA = 0;
		for (i = 0; i <= 3; i++) {
			for (j = 0; j <= 3; j++) {
				for (k = 0; k <= 3; k++) {
					leakage_calc_step =
						params->leakage_consts_ijk
						[i][j][k] * edp_pow(iddq_mA, i);
					/* Convert (mA)^i to (A)^i */
					leakage_calc_step =
						div64_s64(leakage_calc_step,
							  edp_pow(1000, i));
					leakage_calc_step *=
						edp_pow(voltage_mV, j);
					/* Convert (mV)^j to (V)^j */
					leakage_calc_step =
						div64_s64(leakage_calc_step,
							  edp_pow(1000, j));
					leakage_calc_step *=
						edp_pow(temp_C, k);
					/* Convert (C)^k to (scaled_C)^k */
					leakage_calc_step =
						div64_s64(leakage_calc_step,
						edp_pow(params->temp_scaled,
							k));
					/* leakage_consts_ijk was scaled */
					leakage_calc_step =
						div64_s64(leakage_calc_step,
							  params->ijk_scaled);
					leakage_mA += leakage_calc_step;
				}
			}
		}

		/* set floor for leakage current */
		if (leakage_mA <= params->leakage_min)
			leakage_mA = params->leakage_min;

		leakage_mA *= params->leakage_consts_n[n_cores_idx];

		/* leakage_const_n was scaled */
		leakage_mA = div64_s64(leakage_mA, params->consts_scaled);

		/* Calculate dynamic current */
		dyn_mA = voltage_mV * freq_KHz / 1000;
		/* Convert mV to V */
		dyn_mA = div64_s64(dyn_mA, 1000);
		dyn_mA *= params->dyn_consts_n[n_cores_idx];
		/* dyn_const_n was scaled */
		dyn_mA = div64_s64(dyn_mA, params->dyn_scaled);

		if (power_mW != -1) {
			leakage_mW = leakage_mA * voltage_mV;
			dyn_mW = dyn_mA * voltage_mV;
			if (div64_s64(leakage_mW + dyn_mW, 1000) <= power_mW)
				goto end;
		} else if ((leakage_mA + dyn_mA) <= cur_mA) {
			goto end;
		}
		freq_KHz = 0;
	}

 end:
	return edp_apply_fixed_limits(freq_KHz, params,
					cur_effective, temp_C, n_cores_idx);
}

static int edp_relate_freq_voltage(struct clk *clk_cpu_g,
			unsigned int cpu_speedo_idx,
			unsigned int freq_volt_lut_size,
			struct tegra_edp_freq_voltage_table *freq_volt_lut)
{
	unsigned int i, j, freq;
	int voltage_mV;

	for (i = 0, j = 0, freq = 0;
		 i < freq_volt_lut_size;
		 i++, freq += FREQ_STEP) {

		/* Predict voltages */
		voltage_mV = tegra_dvfs_predict_peak_millivolts(
			clk_cpu_g, freq);
		if (voltage_mV < 0) {
			pr_err("%s: couldn't predict voltage: freq %u; err %d",
			       __func__, freq, voltage_mV);
			return -EINVAL;
		}

		/* Cache frequency / voltage / voltage constant relationship */
		freq_volt_lut[i].freq = freq;
		freq_volt_lut[i].voltage_mV = voltage_mV;
	}
	return 0;
}

/*
 * Finds the maximum frequency whose corresponding voltage is <= volt
 * If no such frequency is found, the least possible frequency is returned
 */
unsigned int tegra_edp_find_maxf(int volt)
{
	unsigned int i;

	for (i = 0; i < freq_voltage_lut_size_saved; i++) {
		if (freq_voltage_lut_saved[i].voltage_mV > volt) {
			if (!i)
				return freq_voltage_lut_saved[i].freq;
			break;
		}
	}
	return freq_voltage_lut_saved[i - 1].freq;
}


static int edp_find_speedo_idx(int cpu_speedo_id, unsigned int *cpu_speedo_idx)
{
	int i, array_size;
	struct tegra_edp_cpu_leakage_params *params;

	switch (tegra_get_chip_id()) {
	case TEGRA_CHIPID_TEGRA11:
		params = tegra11x_get_leakage_params(0, &array_size);
		break;
	case TEGRA_CHIPID_TEGRA14:
		params = tegra14x_get_leakage_params(0, &array_size);
		break;
	case TEGRA_CHIPID_TEGRA12:
		params = tegra12x_get_leakage_params(0, &array_size);
		break;
	case TEGRA_CHIPID_TEGRA3:
	case TEGRA_CHIPID_TEGRA2:
	default:
		array_size = 0;
		break;
	}

	for (i = 0; i < array_size; i++)
		if (cpu_speedo_id == params[i].cpu_speedo_id) {
			*cpu_speedo_idx = i;
			return 0;
		}

	pr_err("%s: couldn't find cpu speedo id %d in freq/voltage LUT\n",
	       __func__, cpu_speedo_id);
	return -EINVAL;
}

static int init_cpu_edp_limits_calculated(void)
{
	unsigned int max_nr_cpus = num_possible_cpus();
	unsigned int temp_idx, n_cores_idx, pwr_idx;
	unsigned int cpu_g_minf, cpu_g_maxf;
	unsigned int iddq_mA;
	unsigned int cpu_speedo_idx;
	unsigned int cap, limit;
	struct tegra_edp_limits *edp_calculated_limits;
	struct tegra_edp_limits *reg_idle_calc_limits;
	struct tegra_system_edp_entry *power_edp_calc_limits;
	struct tegra_edp_cpu_leakage_params *params;
	int ret;
	struct clk *clk_cpu_g = tegra_get_clock_by_name("cpu_g");
	int cpu_speedo_id = tegra_cpu_speedo_id();
	int idle_cur = reg_idle_cur;

	/* Determine all inputs to EDP formula */
	iddq_mA = tegra_get_cpu_iddq_value();
	ret = edp_find_speedo_idx(cpu_speedo_id, &cpu_speedo_idx);
	if (ret)
		return ret;

	switch (tegra_get_chip_id()) {
	case TEGRA_CHIPID_TEGRA11:
		params = tegra11x_get_leakage_params(cpu_speedo_idx, NULL);
		break;
	case TEGRA_CHIPID_TEGRA14:
		params = tegra14x_get_leakage_params(cpu_speedo_idx, NULL);
		break;
	case TEGRA_CHIPID_TEGRA12:
		params = tegra12x_get_leakage_params(cpu_speedo_idx, NULL);
		break;
	case TEGRA_CHIPID_TEGRA3:
	case TEGRA_CHIPID_TEGRA2:
	default:
		return -EINVAL;
	}

	edp_calculated_limits = kmalloc(sizeof(struct tegra_edp_limits)
					* ARRAY_SIZE(temperatures), GFP_KERNEL);
	BUG_ON(!edp_calculated_limits);

	reg_idle_calc_limits = kmalloc(sizeof(struct tegra_edp_limits)
				       * ARRAY_SIZE(temperatures), GFP_KERNEL);
	BUG_ON(!reg_idle_calc_limits);

	power_edp_calc_limits = kmalloc(sizeof(struct tegra_system_edp_entry)
				* ARRAY_SIZE(power_cap_levels), GFP_KERNEL);
	BUG_ON(!power_edp_calc_limits);

	cpu_g_minf = 0;
	cpu_g_maxf = clk_get_max_rate(clk_cpu_g);
	freq_voltage_lut_size = (cpu_g_maxf - cpu_g_minf) / FREQ_STEP + 1;
	freq_voltage_lut = kmalloc(sizeof(struct tegra_edp_freq_voltage_table)
				   * freq_voltage_lut_size, GFP_KERNEL);
	if (!freq_voltage_lut) {
		pr_err("%s: failed alloc mem for freq/voltage LUT\n", __func__);
		kfree(power_edp_calc_limits);
		kfree(reg_idle_calc_limits);
		kfree(edp_calculated_limits);
		return -ENOMEM;
	}

	ret = edp_relate_freq_voltage(clk_cpu_g, cpu_speedo_idx,
				freq_voltage_lut_size, freq_voltage_lut);
	if (ret) {
		kfree(power_edp_calc_limits);
		kfree(reg_idle_calc_limits);
		kfree(edp_calculated_limits);
		kfree(freq_voltage_lut);
		return ret;
	}

	if (freq_voltage_lut_size != freq_voltage_lut_size_saved) {
		/* release previous table if present */
		kfree(freq_voltage_lut_saved);
		/* create table to save */
		freq_voltage_lut_saved =
			kmalloc(sizeof(struct tegra_edp_freq_voltage_table) *
			freq_voltage_lut_size, GFP_KERNEL);
		if (!freq_voltage_lut_saved) {
			pr_err("%s: failed alloc mem for freq/voltage LUT\n",
				__func__);
			kfree(freq_voltage_lut);
			kfree(edp_calculated_limits);
			kfree(reg_idle_calc_limits);
			kfree(power_edp_calc_limits);
			return -ENOMEM;
		}
		freq_voltage_lut_size_saved = freq_voltage_lut_size;
	}
	memcpy(freq_voltage_lut_saved,
		freq_voltage_lut,
		sizeof(struct tegra_edp_freq_voltage_table) *
			freq_voltage_lut_size);

	/* Calculate EDP table */
	for (n_cores_idx = 0; n_cores_idx < max_nr_cpus; n_cores_idx++) {
		for (temp_idx = 0;
		     temp_idx < ARRAY_SIZE(temperatures); temp_idx++) {
			edp_calculated_limits[temp_idx].temperature =
				temperatures[temp_idx];
			limit = edp_calculate_maxf(params,
						   temperatures[temp_idx],
						   -1,
						   0,
						   iddq_mA,
						   n_cores_idx);
			if (limit == -EINVAL)
				return -EINVAL;
			/* apply safety cap if it is specified */
			if (n_cores_idx < 4) {
				cap = params->safety_cap[n_cores_idx];
				if (cap && cap < limit)
					limit = cap;
			}
			edp_calculated_limits[temp_idx].
				freq_limits[n_cores_idx] = limit;

			/* regulator mode threshold */
			if (!idle_cur)
				continue;
			reg_idle_calc_limits[temp_idx].temperature =
				temperatures[temp_idx];
			limit = edp_calculate_maxf(params,
						   temperatures[temp_idx],
						   -1,
						   idle_cur,
						   iddq_mA,
						   n_cores_idx);

			/* remove idle table if any threshold is invalid */
			if (limit == -EINVAL) {
				pr_warn("%s: Invalid idle limit for %dmA\n",
					__func__, idle_cur);
				idle_cur = 0;
				continue;
			}

			/* No mode change below G CPU minimum rate */
			if (limit < clk_get_min_rate(clk_cpu_g) / 1000)
				limit = 0;
			reg_idle_calc_limits[temp_idx].
				freq_limits[n_cores_idx] = limit;
		}

		for (pwr_idx = 0;
		     pwr_idx < ARRAY_SIZE(power_cap_levels); pwr_idx++) {
			power_edp_calc_limits[pwr_idx].power_limit_100mW =
				power_cap_levels[pwr_idx] / 100;
			limit = edp_calculate_maxf(params,
						   50,
						   power_cap_levels[pwr_idx],
						   0,
						   iddq_mA,
						   n_cores_idx);
			if (limit == -EINVAL)
				return -EINVAL;
			power_edp_calc_limits[pwr_idx].
				freq_limits[n_cores_idx] = limit;
		}
	}

	/*
	 * If this is an EDP table update, need to overwrite old table.
	 * The old table's address must remain valid.
	 */
	if (edp_limits != edp_default_limits) {
		memcpy(edp_limits, edp_calculated_limits,
		       sizeof(struct tegra_edp_limits)
		       * ARRAY_SIZE(temperatures));
		kfree(edp_calculated_limits);
	} else {
		edp_limits = edp_calculated_limits;
		edp_limits_size = ARRAY_SIZE(temperatures);
	}

	if (idle_cur && reg_idle_edp_limits) {
		memcpy(reg_idle_edp_limits, reg_idle_calc_limits,
		       sizeof(struct tegra_edp_limits)
		       * ARRAY_SIZE(temperatures));
		kfree(reg_idle_calc_limits);
	} else if (idle_cur) {
		reg_idle_edp_limits = reg_idle_calc_limits;
	} else {
		kfree(reg_idle_edp_limits);
		kfree(reg_idle_calc_limits);
		reg_idle_edp_limits = NULL;
	}

	if (power_edp_limits != power_edp_default_limits) {
		memcpy(power_edp_limits, power_edp_calc_limits,
		       sizeof(struct tegra_system_edp_entry)
		       * ARRAY_SIZE(power_cap_levels));
		kfree(power_edp_calc_limits);
	} else {
		power_edp_limits = power_edp_calc_limits;
		power_edp_limits_size = ARRAY_SIZE(power_cap_levels);
	}

	kfree(freq_voltage_lut);
	return 0;
}

void tegra_recalculate_cpu_edp_limits(void)
{
	u32 tegra_chip_id;

	tegra_chip_id = tegra_get_chip_id();
	if (tegra_chip_id != TEGRA_CHIPID_TEGRA11 &&
	    tegra_chip_id != TEGRA_CHIPID_TEGRA14 &&
	    tegra_chip_id != TEGRA_CHIPID_TEGRA12)
		return;

	if (init_cpu_edp_limits_calculated() == 0)
		return;

	/* Revert to default EDP table on error */
	edp_limits = edp_default_limits;
	edp_limits_size = ARRAY_SIZE(edp_default_limits);

	power_edp_limits = power_edp_default_limits;
	power_edp_limits_size = ARRAY_SIZE(power_edp_default_limits);

	kfree(reg_idle_edp_limits);
	reg_idle_edp_limits = NULL;
	pr_err("%s: Failed to recalculate EDP limits\n", __func__);
}

/*
 * Specify regulator current in mA, e.g. 5000mA
 * Use 0 for default
 */
void __init tegra_init_cpu_edp_limits(unsigned int regulator_mA)
{
	if (!regulator_mA)
		goto end;
	regulator_cur = regulator_mA + OVERRIDE_DEFAULT;

	switch (tegra_get_chip_id()) {
	case TEGRA_CHIPID_TEGRA11:
	case TEGRA_CHIPID_TEGRA14:
	case TEGRA_CHIPID_TEGRA12:
		if (init_cpu_edp_limits_calculated() == 0)
			return;
		break;
	case TEGRA_CHIPID_TEGRA2:
	case TEGRA_CHIPID_TEGRA3:
	default:
		BUG();
		break;
	}

 end:
	edp_limits = edp_default_limits;
	edp_limits_size = ARRAY_SIZE(edp_default_limits);

	power_edp_limits = power_edp_default_limits;
	power_edp_limits_size = ARRAY_SIZE(power_edp_default_limits);
}

void tegra_get_cpu_edp_limits(const struct tegra_edp_limits **limits, int *size)
{
	*limits = edp_limits;
	*size = edp_limits_size;
}

void __init tegra_init_cpu_reg_mode_limits(unsigned int regulator_mA,
					   unsigned int mode)
{
	if (mode == REGULATOR_MODE_IDLE) {
		reg_idle_cur = regulator_mA;
		return;
	}
	pr_err("%s: Not supported regulator mode 0x%x\n", __func__, mode);
}

void tegra_get_cpu_reg_mode_limits(const struct tegra_edp_limits **limits,
				   int *size, unsigned int mode)
{
	if (mode == REGULATOR_MODE_IDLE) {
		*limits = reg_idle_edp_limits;
		*size = edp_limits_size;
	} else {
		*limits = NULL;
		*size = 0;
	}
}

void tegra_get_system_edp_limits(const unsigned int **limits)
{
	*limits = system_edp_limits;
}

void tegra_platform_edp_init(struct thermal_trip_info *trips,
				int *num_trips, int margin)
{
	const struct tegra_edp_limits *cpu_edp_limits;
	struct thermal_trip_info *trip_state;
	int i, cpu_edp_limits_size;

	if (!trips || !num_trips)
		return;

	/* edp capping */
	tegra_get_cpu_edp_limits(&cpu_edp_limits, &cpu_edp_limits_size);

	if (cpu_edp_limits_size > MAX_THROT_TABLE_SIZE)
		BUG();

	for (i = 0; i < cpu_edp_limits_size-1; i++) {
		trip_state = &trips[*num_trips];

		trip_state->cdev_type = "cpu_edp";
		trip_state->trip_temp =
			(cpu_edp_limits[i].temperature * 1000) - margin;
		trip_state->trip_type = THERMAL_TRIP_ACTIVE;
		trip_state->upper = trip_state->lower = i + 1;

		(*num_trips)++;

		if (*num_trips >= THERMAL_MAX_TRIPS)
			BUG();
	}
}

struct tegra_system_edp_entry *tegra_get_system_edp_entries(int *size)
{
	*size = power_edp_limits_size;
	return power_edp_limits;
}


#ifdef CONFIG_TEGRA_GPU_EDP
void tegra_get_gpu_edp_limits(const struct tegra_edp_gpu_limits **limits,
							int *size)
{
	*limits = gpu_edp_limits;
	*size = gpu_edp_limits_size;
}

void tegra_platform_gpu_edp_init(struct thermal_trip_info *trips,
				int *num_trips, int margin)
{
	const struct tegra_edp_gpu_limits *gpu_edp_limits;
	struct thermal_trip_info *trip_state;
	int i, gpu_edp_limits_size;

	if (!trips || !num_trips)
		return;

	tegra_get_gpu_edp_limits(&gpu_edp_limits, &gpu_edp_limits_size);

	if (gpu_edp_limits_size > MAX_THROT_TABLE_SIZE)
		BUG();

	for (i = 0; i < gpu_edp_limits_size-1; i++) {
		trip_state = &trips[*num_trips];

		trip_state->cdev_type = "gpu_edp";
		trip_state->trip_temp =
			(gpu_edp_limits[i].temperature * 1000) - margin;
		trip_state->trip_type = THERMAL_TRIP_ACTIVE;
		trip_state->upper = trip_state->lower = i + 1;

		(*num_trips)++;

		if (*num_trips >= THERMAL_MAX_TRIPS)
			BUG();
	}
}

static unsigned int edp_gpu_calculate_maxf(
				struct tegra_edp_gpu_leakage_params *params,
				int temp_C, int iddq_mA)
{
	unsigned int voltage_mV, freq_KHz = 0;
	unsigned int cur_effective = gpu_regulator_cur -
				     gpu_edp_reg_override_mA;
	int f, i, j, k;
	s64 leakage_mA, dyn_mA, leakage_calc_step;

	for (f = freq_voltage_gpu_lut_size - 1; f >= 0; f--) {
		freq_KHz = freq_voltage_gpu_lut[f].freq / 1000;
		voltage_mV = freq_voltage_gpu_lut[f].voltage_mV;

		/* Calculate leakage current */
		leakage_mA = 0;
		for (i = 0; i <= 3; i++) {
			for (j = 0; j <= 3; j++) {
				for (k = 0; k <= 3; k++) {
					leakage_calc_step =
						params->leakage_consts_ijk
						[i][j][k] * edp_pow(iddq_mA, i);

					/* Convert (mA)^i to (A)^i */
					leakage_calc_step =
						div64_s64(leakage_calc_step,
							  edp_pow(1000, i));
					leakage_calc_step *=
						edp_pow(voltage_mV, j);

					/* Convert (mV)^j to (V)^j */
					leakage_calc_step =
						div64_s64(leakage_calc_step,
							  edp_pow(1000, j));
					leakage_calc_step *=
						edp_pow(temp_C, k);

					/* Convert (C)^k to (scaled_C)^k */
					leakage_calc_step =
						div64_s64(leakage_calc_step,
						edp_pow(params->temp_scaled,
							k));

					/* leakage_consts_ijk was scaled */
					leakage_calc_step =
						div64_s64(leakage_calc_step,
							  params->ijk_scaled);

					leakage_mA += leakage_calc_step;
				}
			}
		}
		/* set floor for leakage current */
		if (leakage_mA <= params->leakage_min)
			leakage_mA = params->leakage_min;

		/* Calculate dynamic current */

		dyn_mA = voltage_mV * freq_KHz / 1000;
		/* Convert mV to V */
		dyn_mA = div64_s64(dyn_mA, 1000);
		dyn_mA *= params->dyn_consts_n;
		/* dyn_const_n was scaled */
		dyn_mA = div64_s64(dyn_mA, params->dyn_scaled);

		if ((leakage_mA + dyn_mA) <= cur_effective)
			goto end;

		freq_KHz = 0;
	}

 end:
	return freq_KHz;
}

static int __init start_gpu_edp(void)
{
	const char *cap_name = "edp.gbus";

	gpu_cap_clk = tegra_get_clock_by_name(cap_name);
	if (!gpu_cap_clk) {
		pr_err("gpu_edp_set_cdev_state: cannot get clock:%s\n",
				cap_name);
		return -EINVAL;
	}
	gpu_edp_thermal_idx = 0;

	return 0;
}


static int edp_gpu_relate_freq_voltage(struct clk *clk_gpu,
			unsigned int freq_volt_lut_size,
			struct tegra_edp_freq_voltage_table *freq_volt_lut)
{
	unsigned int i, j, freq;
	int voltage_mV;

	for (i = 0, j = 0, freq = 0;
		 i < freq_volt_lut_size;
		 i++, freq += GPU_FREQ_STEP) {

		/* Predict voltages */
		voltage_mV = tegra_dvfs_predict_peak_millivolts(clk_gpu, freq);
		if (voltage_mV < 0) {
			pr_err("%s: couldn't predict voltage: freq %u; err %d",
			       __func__, freq, voltage_mV);
			return -EINVAL;
		}

		/* Cache frequency / voltage / voltage constant relationship */
		freq_volt_lut[i].freq = freq;
		freq_volt_lut[i].voltage_mV = voltage_mV;
	}
	return 0;
}

static int init_gpu_edp_limits_calculated(void)
{
	unsigned int gpu_minf, gpu_maxf;
	unsigned int limit;
	struct tegra_edp_gpu_limits *gpu_edp_calculated_limits;
	struct tegra_edp_gpu_limits *temp;
	struct tegra_edp_gpu_leakage_params *params;
	int i, ret;
	unsigned int gpu_iddq_mA;
	u32 tegra_chip_id;
	struct clk *gpu_clk = clk_get_parent(gpu_cap_clk);
	tegra_chip_id = tegra_get_chip_id();

	if (tegra_chip_id == TEGRA_CHIPID_TEGRA12) {
		gpu_iddq_mA = tegra_get_gpu_iddq_value();
		params = tegra12x_get_gpu_leakage_params();
	} else
		return -EINVAL;

	gpu_edp_calculated_limits = kmalloc(sizeof(struct tegra_edp_gpu_limits)
				* ARRAY_SIZE(gpu_temperatures), GFP_KERNEL);
	BUG_ON(!gpu_edp_calculated_limits);

	gpu_minf = 0;
	gpu_maxf = clk_get_max_rate(gpu_clk);

	freq_voltage_gpu_lut_size = (gpu_maxf - gpu_minf) / GPU_FREQ_STEP + 1;
	freq_voltage_gpu_lut = kmalloc(sizeof(struct tegra_edp_freq_voltage_table)
				   * freq_voltage_gpu_lut_size, GFP_KERNEL);
	if (!freq_voltage_gpu_lut) {
		pr_err("%s: failed alloc mem for gpu freq/voltage LUT\n",
			 __func__);
		kfree(gpu_edp_calculated_limits);
		return -ENOMEM;
	}

	ret = edp_gpu_relate_freq_voltage(gpu_clk,
			freq_voltage_gpu_lut_size, freq_voltage_gpu_lut);

	if (ret) {
		kfree(gpu_edp_calculated_limits);
		kfree(freq_voltage_gpu_lut);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(gpu_temperatures); i++) {
		gpu_edp_calculated_limits[i].temperature =
			gpu_temperatures[i];
		limit = edp_gpu_calculate_maxf(params,
					       gpu_temperatures[i],
					       gpu_iddq_mA);
		if (limit == -EINVAL) {
			kfree(gpu_edp_calculated_limits);
			kfree(freq_voltage_gpu_lut);
			return -EINVAL;
		}

		gpu_edp_calculated_limits[i].freq_limits = limit;
	}

	/*
	 * If this is an EDP table update, need to overwrite old table.
	 * The old table's address must remain valid.
	 */
	if (gpu_edp_limits != gpu_edp_default_limits &&
			gpu_edp_limits != gpu_edp_calculated_limits) {
		temp = gpu_edp_limits;
		gpu_edp_limits = gpu_edp_calculated_limits;
		gpu_edp_limits_size = ARRAY_SIZE(gpu_temperatures);
		kfree(temp);
	} else {
		gpu_edp_limits = gpu_edp_calculated_limits;
		gpu_edp_limits_size = ARRAY_SIZE(gpu_temperatures);
	}

	kfree(freq_voltage_gpu_lut);

	return 0;
}

void tegra_platform_edp_gpu_init(struct thermal_trip_info *trips,
				int *num_trips, int margin)
{
	const struct tegra_edp_gpu_limits *gpu_edp_limits;
	struct thermal_trip_info *trip_state;
	int i, gpu_edp_limits_size;

	if (!trips || !num_trips)
		return;

	tegra_get_gpu_edp_limits(&gpu_edp_limits, &gpu_edp_limits_size);

	if (gpu_edp_limits_size > MAX_THROT_TABLE_SIZE)
		BUG();

	for (i = 0; i < gpu_edp_limits_size-1; i++) {
		trip_state = &trips[*num_trips];

		trip_state->cdev_type = "gpu_edp";
		trip_state->trip_temp =
			(gpu_edp_limits[i].temperature * 1000) - margin;
		trip_state->trip_type = THERMAL_TRIP_ACTIVE;
		trip_state->upper = trip_state->lower = i + 1;

		(*num_trips)++;

		if (*num_trips >= THERMAL_MAX_TRIPS)
			BUG();
	}
}

void __init tegra_init_gpu_edp_limits(unsigned int regulator_mA)
{
	u32 tegra_chip_id;
	tegra_chip_id = tegra_get_chip_id();

	if (!regulator_mA)
		goto end;
	gpu_regulator_cur = regulator_mA + OVERRIDE_DEFAULT;

	if (start_gpu_edp()) {
		WARN(1, "GPU EDP failed to set initial limits");
		return;
	}

	switch (tegra_chip_id) {
	case TEGRA_CHIPID_TEGRA12:
		if (init_gpu_edp_limits_calculated() == 0)
			return;
		break;

	default:
		BUG();
		break;
	}

 end:
	gpu_edp_limits = gpu_edp_default_limits;
	gpu_edp_limits_size = ARRAY_SIZE(gpu_edp_default_limits);
}

static int gpu_edp_get_cdev_max_state(struct thermal_cooling_device *cdev,
				       unsigned long *max_state)
{
	*max_state = gpu_edp_limits_size - 1;
	return 0;
}

static int gpu_edp_get_cdev_cur_state(struct thermal_cooling_device *cdev,
				       unsigned long *cur_state)
{
	*cur_state = gpu_edp_thermal_idx;
	return 0;
}

static int gpu_edp_set_cdev_state(struct thermal_cooling_device *cdev,
				   unsigned long cur_state)
{
	unsigned long clk_rate;
	BUG_ON(cur_state >= gpu_edp_limits_size);
	clk_rate = gpu_edp_limits[cur_state].freq_limits;
	mutex_lock(&gpu_edp_lock);
	gpu_edp_thermal_idx = cur_state;
	clk_set_rate(gpu_cap_clk, clk_rate * 1000);
	mutex_unlock(&gpu_edp_lock);
	return 0;
}

static struct thermal_cooling_device_ops gpu_edp_cooling_ops = {
	.get_max_state = gpu_edp_get_cdev_max_state,
	.get_cur_state = gpu_edp_get_cdev_cur_state,
	.set_cur_state = gpu_edp_set_cdev_state,
};

static int __init tegra_gpu_edp_late_init(void)
{
	if (IS_ERR_OR_NULL(thermal_cooling_device_register(
		"gpu_edp", NULL, &gpu_edp_cooling_ops)))
		pr_err("%s: failed to register edp cooling device\n", __func__);

	return 0;
}
late_initcall(tegra_gpu_edp_late_init);

#endif

#ifdef CONFIG_DEBUG_FS

static int edp_limit_debugfs_show(struct seq_file *s, void *data)
{
#ifdef CONFIG_CPU_FREQ
	seq_printf(s, "%u\n", tegra_get_edp_limit(NULL));
#endif
	return 0;
}

static inline void edp_show_4core_edp_table(struct seq_file *s, int th_idx)
{
	int i;

	seq_printf(s, "%6s %10s %10s %10s %10s\n",
		   " Temp.", "1-core", "2-cores", "3-cores", "4-cores");
	for (i = 0; i < edp_limits_size; i++) {
		seq_printf(s, "%c%3dC: %10u %10u %10u %10u\n",
			   i == th_idx ? '>' : ' ',
			   edp_limits[i].temperature,
			   edp_limits[i].freq_limits[0],
			   edp_limits[i].freq_limits[1],
			   edp_limits[i].freq_limits[2],
			   edp_limits[i].freq_limits[3]);
	}
}

static inline void edp_show_2core_edp_table(struct seq_file *s, int th_idx)
{
	int i;

	seq_printf(s, "%6s %10s %10s\n",
		   " Temp.", "1-core", "2-cores");
	for (i = 0; i < edp_limits_size; i++) {
		seq_printf(s, "%c%3dC: %10u %10u\n",
			   i == th_idx ? '>' : ' ',
			   edp_limits[i].temperature,
			   edp_limits[i].freq_limits[0],
			   edp_limits[i].freq_limits[1]);
	}
}

static inline void edp_show_4core_reg_mode_table(struct seq_file *s, int th_idx)
{
	int i;

	seq_printf(s, "%6s %10s %10s %10s %10s\n",
		   " Temp.", "1-core", "2-cores", "3-cores", "4-cores");
	for (i = 0; i < edp_limits_size; i++) {
		seq_printf(s, "%c%3dC: %10u %10u %10u %10u\n",
			   i == th_idx ? '>' : ' ',
			   reg_idle_edp_limits[i].temperature,
			   reg_idle_edp_limits[i].freq_limits[0],
			   reg_idle_edp_limits[i].freq_limits[1],
			   reg_idle_edp_limits[i].freq_limits[2],
			   reg_idle_edp_limits[i].freq_limits[3]);
	}
}

static inline void edp_show_2core_reg_mode_table(struct seq_file *s, int th_idx)
{
	int i;

	seq_printf(s, "%6s %10s %10s\n",
		   " Temp.", "1-core", "2-cores");
	for (i = 0; i < edp_limits_size; i++) {
		seq_printf(s, "%c%3dC: %10u %10u\n",
			   i == th_idx ? '>' : ' ',
			   reg_idle_edp_limits[i].temperature,
			   reg_idle_edp_limits[i].freq_limits[0],
			   reg_idle_edp_limits[i].freq_limits[1]);
	}
}

static inline void edp_show_2core_system_table(struct seq_file *s)
{
	seq_printf(s, "%10u %10u\n",
		   system_edp_limits[0],
		   system_edp_limits[1]);
}

static inline void edp_show_4core_system_table(struct seq_file *s)
{
	seq_printf(s, "%10u %10u %10u %10u\n",
		   system_edp_limits[0],
		   system_edp_limits[1],
		   system_edp_limits[2],
		   system_edp_limits[3]);
}

static int edp_debugfs_show(struct seq_file *s, void *data)
{
	unsigned int max_nr_cpus = num_possible_cpus();
	int th_idx;

	if (max_nr_cpus != 2 && max_nr_cpus != 4) {
		seq_printf(s, "Unsupported number of CPUs\n");
		return 0;
	}

#ifdef CONFIG_CPU_FREQ
	tegra_get_edp_limit(&th_idx);
#else
	th_idx = 0;
#endif

	seq_printf(s, "-- VDD_CPU %sEDP table (%umA = %umA - %umA) --\n",
		   edp_limits == edp_default_limits ? "**default** " : "",
		   regulator_cur - edp_reg_override_mA,
		   regulator_cur, edp_reg_override_mA);

	if (max_nr_cpus == 2)
		edp_show_2core_edp_table(s, th_idx);
	else if (max_nr_cpus == 4)
		edp_show_4core_edp_table(s, th_idx);

	if (reg_idle_edp_limits) {
		seq_printf(s, "\n-- Regulator mode thresholds @ %dmA --\n",
			   reg_idle_cur);
		if (max_nr_cpus == 2)
			edp_show_2core_reg_mode_table(s, th_idx);
		else if (max_nr_cpus == 4)
			edp_show_4core_reg_mode_table(s, th_idx);
	}

	if (system_edp_limits) {
		seq_printf(s, "\n-- System EDP table --\n");
		if (max_nr_cpus == 2)
			edp_show_2core_system_table(s);
		else if (max_nr_cpus == 4)
			edp_show_4core_system_table(s);
	}

	return 0;
}

#ifdef CONFIG_TEGRA_GPU_EDP
static int gpu_edp_limit_debugfs_show(struct seq_file *s, void *data)
{
	seq_printf(s, "%u\n", gpu_edp_limits[gpu_edp_thermal_idx].freq_limits);
	return 0;
}

static inline void gpu_edp_show_table(struct seq_file *s)
{
	int i;

	seq_printf(s, "%6s %10s\n",
		   " Temp.", "Freq_limit");
	for (i = 0; i < gpu_edp_limits_size; i++) {
		seq_printf(s, "%3dC: %10u\n",
			   gpu_edp_limits[i].temperature,
			   gpu_edp_limits[i].freq_limits);
	}
}

static int gpu_edp_debugfs_show(struct seq_file *s, void *data)
{
	seq_printf(s, "-- VDD_GPU %sEDP table (%umA = %umA - %umA) --\n",
		   gpu_edp_limits == gpu_edp_default_limits ?
		   "**default** " : "",
		   gpu_regulator_cur - gpu_edp_reg_override_mA,
		   gpu_regulator_cur, gpu_edp_reg_override_mA);

	gpu_edp_show_table(s);

	return 0;
}

static int gpu_edp_reg_override_show(struct seq_file *s, void *data)
{
	seq_printf(s, "Limit override: %u mA. Effective limit: %u mA\n",
		   gpu_edp_reg_override_mA,
		   gpu_regulator_cur - gpu_edp_reg_override_mA);
	return 0;
}

static int gpu_edp_reg_override_write(struct file *file,
	const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[32], *end;
	unsigned int gpu_edp_reg_override_mA_temp;
	unsigned int gpu_edp_reg_override_mA_prev = gpu_edp_reg_override_mA;
	u32 tegra_chip_id;

	tegra_chip_id = tegra_get_chip_id();
	if (tegra_chip_id != TEGRA_CHIPID_TEGRA12)
		goto override_err;

	if (sizeof(buf) <= count)
		goto override_err;

	if (copy_from_user(buf, userbuf, count))
		goto override_err;

	/* terminate buffer and trim - white spaces may be appended
	 *  at the end when invoked from shell command line */
	buf[count] ='\0';
	strim(buf);

	gpu_edp_reg_override_mA_temp = simple_strtoul(buf, &end, 10);
	if (*end != '\0')
		goto override_err;

	if (gpu_edp_reg_override_mA_temp >= gpu_regulator_cur)
		goto override_err;

	if (gpu_edp_reg_override_mA == gpu_edp_reg_override_mA_temp)
		return count;

	gpu_edp_reg_override_mA = gpu_edp_reg_override_mA_temp;
	if (init_gpu_edp_limits_calculated()) {
		/* Revert to previous override value if new value fails */
		gpu_edp_reg_override_mA = gpu_edp_reg_override_mA_prev;
		goto override_err;
	}

	gpu_edp_set_cdev_state(NULL, gpu_edp_thermal_idx);
	pr_info("Reinitialized VDD_GPU EDP table with regulator current limit"
		" %u mA\n", gpu_regulator_cur - gpu_edp_reg_override_mA);

	return count;

 override_err:
	pr_err("FAILED: Override VDD_GPU EDP table with \"%s\"",
	       buf);
	return -EINVAL;
}
#endif

static int edp_reg_override_show(struct seq_file *s, void *data)
{
	seq_printf(s, "Limit override: %u mA. Effective limit: %u mA\n",
		   edp_reg_override_mA, regulator_cur - edp_reg_override_mA);
	return 0;
}

static int edp_reg_override_write(struct file *file,
	const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[32], *end;
	unsigned int edp_reg_override_mA_temp;
	unsigned int edp_reg_override_mA_prev = edp_reg_override_mA;
	u32 tegra_chip_id;

	tegra_chip_id = tegra_get_chip_id();
	if (!(tegra_chip_id == TEGRA_CHIPID_TEGRA11 ||
		tegra_chip_id == TEGRA_CHIPID_TEGRA14 ||
		tegra_chip_id == TEGRA_CHIPID_TEGRA12))
		goto override_err;

	if (sizeof(buf) <= count)
		goto override_err;

	if (copy_from_user(buf, userbuf, count))
		goto override_err;

	/* terminate buffer and trim - white spaces may be appended
	 *  at the end when invoked from shell command line */
	buf[count]='\0';
	strim(buf);

	edp_reg_override_mA_temp = simple_strtoul(buf, &end, 10);
	if (*end != '\0')
		goto override_err;

	if (edp_reg_override_mA_temp >= regulator_cur)
		goto override_err;

	if (edp_reg_override_mA == edp_reg_override_mA_temp)
		return count;

	edp_reg_override_mA = edp_reg_override_mA_temp;
	if (init_cpu_edp_limits_calculated()) {
		/* Revert to previous override value if new value fails */
		edp_reg_override_mA = edp_reg_override_mA_prev;
		goto override_err;
	}

#ifdef CONFIG_CPU_FREQ
	if (tegra_cpu_set_speed_cap(NULL)) {
		pr_err("FAILED: Set CPU freq cap with new VDD_CPU EDP table\n");
		goto override_out;
	}

	pr_info("Reinitialized VDD_CPU EDP table with regulator current limit"
			" %u mA\n", regulator_cur - edp_reg_override_mA);
#else
	pr_err("FAILED: tegra_cpu_set_speed_cap() does not exist, failed to reinitialize VDD_CPU EDP table");
#endif

	return count;

override_err:
	pr_err("FAILED: Reinitialize VDD_CPU EDP table with override \"%s\"",
	       buf);
#ifdef CONFIG_CPU_FREQ
override_out:
#endif
	return -EINVAL;
}

static int edp_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, edp_debugfs_show, inode->i_private);
}

#ifdef CONFIG_TEGRA_GPU_EDP
static int gpu_edp_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, gpu_edp_debugfs_show, inode->i_private);
}

static int gpu_edp_limit_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, gpu_edp_limit_debugfs_show, inode->i_private);
}

static int gpu_edp_reg_override_open(struct inode *inode, struct file *file)
{
	return single_open(file, gpu_edp_reg_override_show, inode->i_private);
}
#endif

static int edp_limit_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, edp_limit_debugfs_show, inode->i_private);
}

static int edp_reg_override_open(struct inode *inode, struct file *file)
{
	return single_open(file, edp_reg_override_show, inode->i_private);
}

static const struct file_operations edp_debugfs_fops = {
	.open		= edp_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#ifdef CONFIG_TEGRA_GPU_EDP
static const struct file_operations gpu_edp_debugfs_fops = {
	.open		= gpu_edp_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations gpu_edp_limit_debugfs_fops = {
	.open		= gpu_edp_limit_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations gpu_edp_reg_override_debugfs_fops = {
	.open		= gpu_edp_reg_override_open,
	.read		= seq_read,
	.write		= gpu_edp_reg_override_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

static const struct file_operations edp_limit_debugfs_fops = {
	.open		= edp_limit_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations edp_reg_override_debugfs_fops = {
	.open		= edp_reg_override_open,
	.read		= seq_read,
	.write		= edp_reg_override_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int reg_idle_cur_get(void *data, u64 *val)
{
	*val = reg_idle_cur;
	return 0;
}
static int reg_idle_cur_set(void *data, u64 val)
{
	int ret;

	ret = tegra_cpu_reg_mode_force_normal(true);
	if (ret) {
		pr_err("%s: Failed to force regulator normal mode\n", __func__);
		return ret;
	}

	reg_idle_cur = (int)val;
	tegra_update_cpu_edp_limits();
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_idle_cur_debugfs_fops,
			reg_idle_cur_get, reg_idle_cur_set, "%llu\n");

#ifdef CONFIG_TEGRA_GPU_EDP
static int __init tegra_gpu_edp_debugfs_init(struct dentry *edp_dir)
{
	struct dentry *d_edp;
	struct dentry *d_edp_limit;
	struct dentry *d_edp_reg_override;
	struct dentry *vdd_gpu_dir;

	if (!tegra_platform_is_silicon())
		return -ENOSYS;

	vdd_gpu_dir = debugfs_create_dir("vdd_gpu", edp_dir);
	if (!vdd_gpu_dir)
		goto err_0;

	d_edp = debugfs_create_file("gpu_edp",  S_IRUGO, vdd_gpu_dir, NULL,
				    &gpu_edp_debugfs_fops);
	if (!d_edp)
		goto err_1;

	d_edp_limit = debugfs_create_file("gpu_edp_limit", S_IRUGO, vdd_gpu_dir,
					  NULL, &gpu_edp_limit_debugfs_fops);
	if (!d_edp_limit)
		goto err_2;

	d_edp_reg_override = debugfs_create_file("gpu_edp_reg_override",
				S_IRUGO | S_IWUSR, vdd_gpu_dir, NULL,
				&gpu_edp_reg_override_debugfs_fops);
	if (!d_edp_reg_override)
		goto err_3;

	return 0;

err_3:
	debugfs_remove(d_edp_limit);
err_2:
	debugfs_remove(d_edp);
err_1:
	debugfs_remove(vdd_gpu_dir);
err_0:
	return -ENOMEM;
}
#endif

#if defined(CONFIG_EDP_FRAMEWORK) || defined(CONFIG_SYSEDP_FRAMEWORK)
static __init struct dentry *tegra_edp_debugfs_dir(void)
{
	if (edp_debugfs_dir)
		return edp_debugfs_dir;
	else
		return debugfs_create_dir("edp", NULL);
}
#else
static __init struct dentry *tegra_edp_debugfs_dir(void)
{
	return debugfs_create_dir("edp", NULL);
}
#endif

static int __init tegra_edp_debugfs_init(void)
{
	struct dentry *d_reg_idle_cur;
	struct dentry *d_edp;
	struct dentry *d_edp_limit;
	struct dentry *d_edp_reg_override;
	struct dentry *edp_dir;
	struct dentry *vdd_cpu_dir;

	if (!tegra_platform_is_silicon())
		return -ENOSYS;

	edp_dir = tegra_edp_debugfs_dir();
	if (!edp_dir)
		goto err_0;

	vdd_cpu_dir = debugfs_create_dir("vdd_cpu", edp_dir);
	if (!vdd_cpu_dir)
		goto err_0;

	d_edp = debugfs_create_file("edp", S_IRUGO, vdd_cpu_dir, NULL,
				    &edp_debugfs_fops);
	if (!d_edp)
		goto err_1;

	d_edp_limit = debugfs_create_file("edp_limit", S_IRUGO, vdd_cpu_dir,
					  NULL, &edp_limit_debugfs_fops);
	if (!d_edp_limit)
		goto err_2;

	d_edp_reg_override = debugfs_create_file("edp_reg_override",
					S_IRUGO | S_IWUSR, vdd_cpu_dir, NULL,
					&edp_reg_override_debugfs_fops);
	if (!d_edp_reg_override)
		goto err_3;

	d_reg_idle_cur = debugfs_create_file("reg_idle_mA",
					S_IRUGO | S_IWUSR, vdd_cpu_dir, NULL,
					&reg_idle_cur_debugfs_fops);
	if (!d_reg_idle_cur)
		goto err_4;

	if (tegra_core_edp_debugfs_init(edp_dir))
		return -ENOMEM;

#ifdef CONFIG_TEGRA_GPU_EDP
	if (tegra_gpu_edp_debugfs_init(edp_dir))
		return -ENOMEM;
#endif

	return 0;

err_4:
	debugfs_remove(d_edp_reg_override);
err_3:
	debugfs_remove(d_edp_limit);
err_2:
	debugfs_remove(d_edp);
err_1:
	debugfs_remove(vdd_cpu_dir);
err_0:
	return -ENOMEM;
}

late_initcall(tegra_edp_debugfs_init);
#endif /* CONFIG_DEBUG_FS */
