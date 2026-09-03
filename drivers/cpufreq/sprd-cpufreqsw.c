// SPDX-License-Identifier: GPL-2.0

/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "sprd-cpufreqsw: " fmt

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/types.h>
#include <linux/of_platform.h>
#include <linux/clk-provider.h>
#include "sprd-cpufreq-common.h"

static unsigned long boot_done_timestamp;
static int boost_mode_flag = 1;
struct mutex cpu_gpu_volt_lock;
static bool cpu_gpu_share_volt;
static struct regmap *aon_apb_reg_base;
static unsigned int cpu_target_volt_reg;
static unsigned int gpu_target_volt_reg;
struct regulator *gpu_cpu_reg;
static struct cpufreq_driver sprd_cpufreq_driver;
static int sprd_cpufreq_set_boost(struct cpufreq_policy *policy, int state);
static int sprd_cpufreq_set_target(struct sprd_cpufreq_driver_data *cpufreq_data,
				   unsigned int idx, bool force);
static unsigned int sprd_cpufreq_update_opp(int cpu, int temp_now);

void sprd_cpu_gpu_volt_lock(void)
{
	mutex_lock(&cpu_gpu_volt_lock);
}
EXPORT_SYMBOL_GPL(sprd_cpu_gpu_volt_lock);

void sprd_cpu_gpu_volt_unlock(void)
{
	mutex_unlock(&cpu_gpu_volt_lock);
}
EXPORT_SYMBOL_GPL(sprd_cpu_gpu_volt_unlock);

struct regulator *sprd_get_cpu_gpu_regulator(void)
{
	if (gpu_cpu_reg == NULL)
		pr_err("%s: failed to get regulator\n", __func__);
	return gpu_cpu_reg;
}
EXPORT_SYMBOL_GPL(sprd_get_cpu_gpu_regulator);

static unsigned int sprd_get_gpu_target_voltage(void)
{
	unsigned int gpu_vdd = 0;

	if (cpu_gpu_share_volt)
		regmap_read(aon_apb_reg_base, gpu_target_volt_reg, &gpu_vdd);
	return gpu_vdd;
}

static void sprd_set_cpu_target_voltage(unsigned long cpu_vdd)
{
	if (cpu_gpu_share_volt)
		regmap_write(aon_apb_reg_base, cpu_target_volt_reg, cpu_vdd);
}

static unsigned int sprd_get_cpu_target_voltage(void)
{
	unsigned int cpu_vdd = 0;

	if (cpu_gpu_share_volt)
		regmap_read(aon_apb_reg_base, cpu_target_volt_reg, &cpu_vdd);
	return cpu_vdd;
}

static int sprd_get_cpu_gpu_volt_parameters(struct device_node *np)
{
	unsigned int reg_info[2];
	int ret;

	cpu_gpu_share_volt = of_property_read_bool(np,
						   "sprd,cpu-gpu-share-volt");
	if (!cpu_gpu_share_volt)
		return 0;

	mutex_init(&cpu_gpu_volt_lock);

	aon_apb_reg_base = syscon_regmap_lookup_by_phandle_args(np,
				"gpu-target-volt-syscon", 2, reg_info);
	if (IS_ERR(aon_apb_reg_base)) {
		pr_err("Failed to parse gpu_target_volt syscon\n");
		ret = PTR_ERR(aon_apb_reg_base);
		return ret;
	}

	gpu_target_volt_reg = reg_info[0];
	aon_apb_reg_base = syscon_regmap_lookup_by_phandle_args(np,
				"cpu-target-volt-syscon", 2, reg_info);
	if (IS_ERR(aon_apb_reg_base)) {
		pr_err("Failed to parse cpu_target_volt syscon\n");
		ret = PTR_ERR(aon_apb_reg_base);
		return ret;
	}

	cpu_target_volt_reg = reg_info[0];

	return 0;
}

static int sprd_verify_opp_with_regulator(struct device *cpu_dev,
					  struct regulator *cpu_reg,
					  unsigned int volt_tol)
{
	unsigned long opp_freq = 0;
	unsigned long min_uV = ~0UL, max_uV = 0;

	while (1) {
		struct dev_pm_opp *opp;
		unsigned long opp_uV, tol_uV;

		opp = dev_pm_opp_find_freq_ceil(cpu_dev, &opp_freq);
		if (IS_ERR(opp)) {
			/* We dont have more freq in opp table, break the loop*/
			pr_err("invalid opp freq %lu\n", opp_freq);
			break;
		}
		opp_uV = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);
		tol_uV = opp_uV * volt_tol / 100;
		if (regulator_is_supported_voltage(cpu_reg, opp_uV,
						   opp_uV + tol_uV)) {
			if (opp_uV < min_uV)
				min_uV = opp_uV;
			if (opp_uV > max_uV)
				max_uV = opp_uV;
		} else {
			pr_debug("disable unsupported opp_freq%lu\n", opp_freq);
			dev_pm_opp_disable(cpu_dev, opp_freq);
		}
		opp_freq++;
	}
	pr_debug("regulator min volt %lu, max volt %lu\n", min_uV, max_uV);

	return regulator_set_voltage_time(cpu_reg, min_uV, max_uV);
}

static int sprd_cpufreq_set_clock(struct sprd_cpufreq_driver_data *c,
				  unsigned long freq)
{
	struct clk *clk_low_freq_p;
	struct clk *clk_high_freq_p;
	unsigned long clk_low_freq_p_max = 0;
	int ret = 0, i;

	if (!c || freq == 0)
		return -ENODEV;

	clk_low_freq_p = c->clk_low_freq_p;
	clk_high_freq_p = c->clk_high_freq_p;
	clk_low_freq_p_max = c->clk_low_freq_p_max;

	pr_debug("request freq is %lu\n", freq);
	for (i = 0; i < SPRD_MAX_CPUS_EACH_CLUSTER; i++) {
		if (IS_ERR(c->clks[i]))
			continue;
		ret = clk_set_parent(c->clks[i], clk_low_freq_p);
		if (ret) {
			pr_err("set clk_low_freq_p as parent fail!\n");
			return ret;
		}
	}
	if (freq == clk_low_freq_p_max)
		return 0;

	if (clk_set_rate(clk_high_freq_p, freq))
		pr_err("set clk_high_freq_p freq %luHz failed!\n", freq);
	else
		pr_debug("clk_high_freq_p is %luHz.\n",
			 clk_get_rate(clk_high_freq_p));
	for (i = 0; i < SPRD_MAX_CPUS_EACH_CLUSTER; i++) {
		if (IS_ERR(c->clks[i]))
			continue;
		ret = clk_set_parent(c->clks[i], clk_high_freq_p);
		if (ret) {
			pr_err("set target clk failed!\n");
			return ret;
		}
	}

	return 0;
}

static int sprd_cpufreq_set_clock_v1(struct sprd_cpufreq_driver_data *c,
				     unsigned long freq)
{
	int ret = 0, i;

	if (!c || freq == 0)
		return -ENODEV;

	if (c->freq_req == freq)
		return 0;

	pr_debug("%s freq=%lu\n", __func__, freq);

	if (freq > c->clk_low_freq_p_max) {
		if (c->clk_high_freq_const == 0) {
			for (i = 0; i < SPRD_MAX_CPUS_EACH_CLUSTER; i++) {
				if (IS_ERR(c->clks[i]))
					continue;
				ret = clk_set_parent(c->clks[i],
						     c->clk_low_freq_p);
				if (ret) {
					pr_err("set clk_low_freq_p as parent failed!\n");
					return ret;
				}
			}
			ret = clk_set_rate(c->clk_high_freq_p, freq);
			if (ret) {
				pr_err("set clk_high_freq_p %luKHz failed!\n",
				       freq / 1000);
				return ret;
			}
		}
		for (i = 0; i < SPRD_MAX_CPUS_EACH_CLUSTER; i++) {
			if (IS_ERR(c->clks[i]))
				continue;
			ret = clk_set_parent(c->clks[i], c->clk_high_freq_p);
			if (ret) {
				pr_err("set clk_high_freq_p as parent failed!\n");
				return ret;
			}
		}
		pr_debug("set cluster%u setting clk_high as parent\n",
			 c->cluster);
	} else if (freq == c->clk_low_freq_p_max) {
		for (i = 0; i < SPRD_MAX_CPUS_EACH_CLUSTER; i++) {
			if (IS_ERR(c->clks[i]))
				continue;
			ret = clk_set_parent(c->clks[i], c->clk_low_freq_p);
			if (ret) {
				pr_err("set clk_low_freq as parent failed!\n");
				return ret;
			}
		}
		pr_debug("set cluster%u setting clk_low as parent\n",
			 c->cluster);
	} else {
		pr_err("request clk freq is invalid!\n");
		return -EINVAL;
	}

	pr_debug("set cluster%u freq %luHZ\n", c->cluster, freq);
	c->freq_req = freq;
	return 0;
}

static struct regulator *sprd_volt_share_reg(struct sprd_cpufreq_driver_data *c)
{
	int cluster;
	struct regulator *reg = NULL;

	if (!c)
		return NULL;

	for (cluster = 0; cluster < SPRD_CPUFREQ_MAX_MODULE; cluster++)
		if ((c->volt_share_masters_bits >> cluster) & 0x1) {
			pr_debug("check master cluster%u\n", cluster);
			if (cpufreq_datas[cluster] &&
			    cpufreq_datas[cluster]->reg) {
				reg = cpufreq_datas[cluster]->reg;
				break;
			}
		}

	if (reg)
		return reg;
	for (cluster = 0; cluster < SPRD_CPUFREQ_MAX_MODULE; cluster++)
		if ((c->volt_share_hosts_bits >> cluster) & 0x1) {
			pr_debug("check host cluster%u\n", cluster);
			if (cpufreq_datas[cluster] &&
			    cpufreq_datas[cluster]->reg) {
				reg = cpufreq_datas[cluster]->reg;
				break;
			}
		}
	return reg;
}

static struct mutex *sprd_volt_share_lock(struct sprd_cpufreq_driver_data *c)
{
	int cluster;
	struct mutex *volt_lock = NULL;

	if (!c)
		return NULL;

	for (cluster = 0; cluster < SPRD_CPUFREQ_MAX_MODULE; cluster++)
		if ((c->volt_share_masters_bits >> cluster) & 0x1) {
			pr_debug("check master cluster%u\n", cluster);
			if (cpufreq_datas[cluster] &&
			    cpufreq_datas[cluster]->volt_lock) {
				volt_lock = cpufreq_datas[cluster]->volt_lock;
				break;
			}
		}

	if (!volt_lock)
		for (cluster = 0; cluster < SPRD_CPUFREQ_MAX_MODULE; cluster++)
			if ((c->volt_share_hosts_bits >> cluster) & 0x1) {
				pr_debug("check host cluster%u\n", cluster);
				if (cpufreq_datas[cluster] &&
				    cpufreq_datas[cluster]->volt_lock) {
					volt_lock =
					cpufreq_datas[cluster]->volt_lock;
					break;
				}
			}

	pr_debug("cluster %u masters 0x%x hosts 0x%x volt_lock %p\n",
		 c->cluster,
		 c->volt_share_masters_bits,
		 c->volt_share_hosts_bits,
		 volt_lock);

	return volt_lock;
}

unsigned int sprd_volt_tol_min(struct sprd_cpufreq_driver_data *c, bool online)
{
	unsigned int cluster, volt_tol_min = 0;

	if (!c)
		return 0;

	volt_tol_min = c->volt_tol_var;
	for (cluster = 0; cluster < SPRD_CPUFREQ_MAX_MODULE; cluster++)
		if ((c->volt_share_masters_bits >> cluster) & 0x1) {
			pr_debug("check master cluster%u\n", cluster);
			if (cpufreq_datas[cluster] &&
			    (online ? cpufreq_datas[cluster]->online : 1) &&
			    cpufreq_datas[cluster]->volt_tol_var < volt_tol_min) {
				volt_tol_min =
					cpufreq_datas[cluster]->volt_tol_var;
		}
	}

	pr_debug("cluster %u volt_share_masters_bits %u volt_tol_min %u\n",
		 c->cluster,
		 c->volt_share_masters_bits,
		 volt_tol_min);

	return volt_tol_min;
}

/*
 * Sprd_volt_req_max()  - get volt_req_max
 * @c:        cluster
 * @volt_max_aim: aimed max between *volt_max_p
 *	and max volt of online or all clusters.
 * @online: 0-just search all clusters; 1-just search online clusters.
 * @except_self: 0-just search all clusters; 1-just search online clusters.
 * @return: current max volt of online/all clusters.
 */
unsigned long sprd_volt_req_max(struct sprd_cpufreq_driver_data *c,
				unsigned long *volt_max_aim,
				bool online,
				bool except_self)
{
	int cluster;
	unsigned long volt_max_req = 0, ret = 0;

	if (!c)
		return 0;

	for (cluster = 0; cluster < SPRD_CPUFREQ_MAX_MODULE; cluster++)
		if ((c->volt_share_masters_bits >> cluster) & 0x1) {
			pr_debug("check master cluster%u\n", cluster);
			if ((except_self ? cluster != c->cluster : 1) &&
			    cpufreq_datas[cluster] &&
			    (online ? cpufreq_datas[cluster]->online : 1) &&
			    cpufreq_datas[cluster]->volt_req > volt_max_req) {
				volt_max_req = cpufreq_datas[cluster]->volt_req;
			}
		}

	if (volt_max_req > *volt_max_aim)
		*volt_max_aim = volt_max_req;
	else
		ret = volt_max_req;

	pr_debug("cluster %u volt_share_masters_bits %u volt_max %lu\n",
		 c->cluster,
		 c->volt_share_masters_bits,
		 *volt_max_aim);

	return ret;
}

static int sprd_freq_sync_by_volt(struct sprd_cpufreq_driver_data *c,
				  unsigned long volt)
{
	int i = 0, ret = -ENODEV;

	while (i < c->freqvolts) {
		if (c->freqvolt[i].volt > 0 &&
		    volt >= c->freqvolt[i].volt)
			break;
		i++;
	}
	if (i < c->freqvolts) {
		ret = sprd_cpufreq_set_clock_v1(c, c->freqvolt[i].freq);
		if (!ret)
			c->volt_req = c->freqvolt[i].volt;
	} else {
		pr_info("%s not found more than volt%lu\n", __func__, volt);
	}
	return ret;
}

int sprd_volt_share_slaves_notify(struct sprd_cpufreq_driver_data *host,
				  unsigned long volt)
{
	unsigned int cluster;
	int ret = 0;

	pr_debug("%s volt_share_slaves_bits%u, volt %lu\n",
		 __func__, host->volt_share_slaves_bits, volt);

	for (cluster = 0; cluster < SPRD_CPUFREQ_MAX_MODULE; cluster++)
		if ((host->volt_share_slaves_bits >> cluster) & 0x1) {
			if (cpufreq_datas[cluster] &&
			    cpufreq_datas[cluster]->online &&
			    cpufreq_datas[cluster]->volt_share_hosts_bits) {
				ret = sprd_freq_sync_by_volt(cpufreq_datas[cluster], volt);
				if (ret) {
					pr_err("cluster%u freq_sync error!\n",
					       cluster);
					break;
				}
			}
		}

	return ret;
}

/*
 * Sprd_freq_sync_slaves_notify()  - sprd_freq_sync_slaves_notify
 * @idx:        0 points to min freq, ascending order
 */
int sprd_freq_sync_slaves_notify(struct sprd_cpufreq_driver_data *host,
				 const unsigned int idx, bool force)
{
	struct sprd_cpufreq_driver_data *c;
	unsigned int cluster;
	int ret;

	if (!host) {
		pr_debug("invalid host cpufreq_data\n");
		return -ENODEV;
	}

	for (cluster = 0; cluster < SPRD_CPUFREQ_MAX_MODULE; cluster++) {
		if (!((host->freq_sync_slaves_bits >> cluster) & 0x1))
			continue;
		if (cpufreq_datas[cluster] && cpufreq_datas[cluster]->online &&
		    cpufreq_datas[cluster]->freq_sync_hosts_bits) {
			c = cpufreq_datas[cluster];
			ret = sprd_cpufreq_set_target(c, idx, force);
			if (ret) {
				pr_info("%s sub-cluster%u freq_sync failed!\n", __func__, cluster);
				return ret;
			}
		}
	}
	return 0;
}

/*
 * sprd_cpufreq_set_target()  - cpufreq_set_target
 * @idx:        0 points to min freq, ascending order
 */
static int sprd_cpufreq_set_target(struct sprd_cpufreq_driver_data *cpufreq_data,
				   unsigned int idx,
				   bool force)
{
	struct dev_pm_opp *opp;
	unsigned long volt_new = 0, volt_new_req = 0, volt_old = 0;
	unsigned long old_freq_hz, new_freq_hz, freq_Hz, opp_freq_hz;
	unsigned int volt_tol = 0;
	struct regulator *cpu_reg;
	struct clk *cpu_clk;
	struct device *cpu_dev;
	int cluster;
	int ret = 0;

	if (!cpufreq_data) {
		ret = -ENODEV;
		return ret;
	}
	mutex_lock(cpufreq_data->volt_lock);
	if (cpufreq_data && idx >= cpufreq_data->freqvolts) {
		pr_err("invalid cpufreq_data/idx%u, returning\n", idx);
		ret = -ENODEV;
		goto exit_unlock;
	}

	pr_debug("setting target for cluster %d, freq idx %u freqvolts %u\n",
		 cpufreq_data->cluster,
		 idx,
		 cpufreq_data->freqvolts);

	if (IS_ERR_OR_NULL(cpufreq_data->clk) ||
	    IS_ERR_OR_NULL(cpufreq_data->reg)) {
		pr_err("no reg/clk for cluster %d\n", cpufreq_data->cluster);
		ret = -ENODEV;
		goto exit_unlock;
	}

	if (idx > cpufreq_data->freqvolts - 1)
		idx = 0;
	else
		idx = cpufreq_data->freqvolts - 1 - idx;

	cpu_dev = cpufreq_data->cpu_dev;
	cpu_clk = cpufreq_data->clk;
	cpu_reg = cpufreq_data->reg;
	cluster = cpufreq_data->cluster;
	freq_Hz = cpufreq_data->freqvolt[idx].freq;
	mutex_unlock(cpufreq_data->volt_lock);

	new_freq_hz = freq_Hz;
	old_freq_hz = clk_get_rate(cpu_clk);
	if (cpu_dev) {
		/*For cpu device, get freq&volt from opp*/
		opp = dev_pm_opp_find_freq_ceil(cpu_dev, &freq_Hz);
		if (IS_ERR(opp)) {
			pr_err("failed to find OPP for %luKhz\n",
			       freq_Hz / 1000);
			return PTR_ERR(opp);
		}
		volt_new = dev_pm_opp_get_voltage(opp);
		opp_freq_hz = dev_pm_opp_get_freq(opp);
		dev_pm_opp_put(opp);
	} else {
		/* For sub device, get freq&volt from table */
		volt_new = cpufreq_data->freqvolt[idx].volt;
		opp_freq_hz = cpufreq_data->freqvolt[idx].freq;
	}
	volt_tol = cpufreq_data->volt_tol;

	if (force) {
		mutex_lock(cpufreq_data->volt_lock);
	} else if (mutex_trylock(cpufreq_data->volt_lock) != 1) {
		pr_info("cannot acquire lock for cluster %d\n",
			cpufreq_data->cluster);
		return -EBUSY;
	}

	/* Must get real volt_old in mutex_lock domain */
	volt_old = regulator_get_voltage(cpu_reg);
	pr_debug("Found OPP: %ld kHz, %ld uV, tolerance: %uuV\n",
		 opp_freq_hz / 1000, volt_new, volt_tol);

	volt_new_req = volt_new;
	sprd_volt_req_max(cpufreq_data, &volt_new, true, true);
	if (!volt_new) {
		pr_debug("fail to get volt_new=0\n");
		goto exit_err;
	}

	pr_debug("cluster%u scaling from %lu MHz, %ld mV,--> %lu MHz, %ld mV\n",
		 cpufreq_data->cluster,
		 old_freq_hz / 1000000,
		 (volt_old > 0) ? volt_old / 1000 : -1,
		 new_freq_hz / 1000000,
		 volt_new ? volt_new / 1000 : -1);

	if (volt_new < volt_old) {
		ret  = sprd_volt_share_slaves_notify(cpufreq_data, volt_new);
		if (ret)
			goto exit_err;
	}

	/* Scaling up? scale voltage before frequency */
	if (new_freq_hz > old_freq_hz) {
		pr_debug("scaling up voltage to %lu\n", volt_new);
		if (cpu_gpu_share_volt) {
			sprd_cpu_gpu_volt_lock();
			sprd_set_cpu_target_voltage(volt_new);
			volt_new = volt_new < sprd_get_gpu_target_voltage() ?
				   sprd_get_gpu_target_voltage() : volt_new;
			pr_debug("%ld mV to %ld mV, cpu_vdd:%dmV, gpu_vdd:%dmV\n",
				 (volt_old > 0) ? volt_old / 1000 : -1,
				 volt_new ? volt_new / 1000 : -1,
				 sprd_get_cpu_target_voltage() / 1000,
				 sprd_get_gpu_target_voltage() / 1000);
			ret = regulator_set_voltage_tol(cpu_reg, volt_new, volt_tol);
			sprd_cpu_gpu_volt_unlock();
		} else {
			ret = regulator_set_voltage_tol(cpu_reg, volt_new, volt_tol);
		}
		if (ret) {
			if (cpu_gpu_share_volt) {
				sprd_cpu_gpu_volt_lock();
				sprd_set_cpu_target_voltage(volt_old);
				sprd_cpu_gpu_volt_unlock();
			}
			pr_err("failed to scale voltage %lu %u up: %d\n",
			       volt_new, volt_tol, ret);
			goto exit_err;
		}
	}
	ret = sprd_cpufreq_set_clock(cpufreq_data, new_freq_hz);
	if (ret) {
		pr_err("failed to set clock %luMhz rate: %d\n",
		       new_freq_hz / 1000000,
		      ret);
		if (volt_old > 0 && new_freq_hz > old_freq_hz) {
			pr_info("scaling to old voltage %lu\n", volt_old);
			if (cpu_gpu_share_volt) {
				sprd_cpu_gpu_volt_lock();
				sprd_set_cpu_target_voltage(volt_old);
				volt_old = volt_old < sprd_get_gpu_target_voltage() ?
					   sprd_get_gpu_target_voltage() : volt_old;
				pr_debug("%ld mV to %ld mV, cpu_vdd:%dmV, gpu_vdd:%dmV\n",
					  (volt_old > 0) ? volt_old / 1000 : -1,
					  volt_new ? volt_new / 1000 : -1,
					  sprd_get_cpu_target_voltage() / 1000,
					  sprd_get_gpu_target_voltage() / 1000);
				ret = regulator_set_voltage_tol(cpu_reg,
								volt_old,
								volt_tol);
				sprd_cpu_gpu_volt_unlock();
			} else {
				regulator_set_voltage_tol(cpu_reg, volt_old, volt_tol);
			}
		}
		goto exit_err;
	}

	/* Scaling down?  scale voltage after frequency */
	if (new_freq_hz < old_freq_hz) {
		pr_debug("scaling down voltage to %lu\n", volt_new);
		if (cpu_gpu_share_volt) {
			sprd_cpu_gpu_volt_lock();
			sprd_set_cpu_target_voltage(volt_new);
			volt_new = volt_new < sprd_get_gpu_target_voltage() ?
				   sprd_get_gpu_target_voltage():volt_new;
			pr_debug("cpu_gpu_cync: %ld mV to %ld mV, cpu_vdd:%dmV,gpu_vdd:%dmV\n",
				 (volt_old > 0) ? volt_old / 1000 : -1,
				 volt_new ? volt_new / 1000 : -1,
				 sprd_get_cpu_target_voltage() / 1000,
				 sprd_get_gpu_target_voltage() / 1000);
			ret = regulator_set_voltage_tol(cpu_reg, volt_new, volt_tol);
			sprd_cpu_gpu_volt_unlock();
		} else {
			ret = regulator_set_voltage_tol(cpu_reg, volt_new, volt_tol);
		}

		if (ret) {
			pr_warn("failed to scale volt %lu %u down: %d\n",
				volt_new, volt_tol, ret);
			if (cpu_gpu_share_volt) {
				sprd_cpu_gpu_volt_lock();
				sprd_set_cpu_target_voltage(volt_old);
				pr_warn("now, cpu_vdd:%dmV, gpu_vdd:%dmV\n",
				       sprd_get_cpu_target_voltage() / 1000,
				       sprd_get_gpu_target_voltage() / 1000);
				sprd_cpu_gpu_volt_unlock();
			}
			ret = sprd_cpufreq_set_clock(cpufreq_data, old_freq_hz);
			goto exit_err;
		}
	}

	if (volt_new >= volt_old)
		ret  = sprd_volt_share_slaves_notify(cpufreq_data, volt_new);

	cpufreq_data->volt_tol_var = volt_tol;
	cpufreq_data->volt_req = volt_new_req;
	cpufreq_data->freq_req = new_freq_hz;

	pr_debug("cluster%u After transition, new clk rate %luMhz, volt %dmV\n",
		 cpufreq_data->cluster,
		 clk_get_rate(cpufreq_data->clk) / 1000000,
		 regulator_get_voltage(cpu_reg) / 1000);
	goto exit_unlock;

exit_err:
	sprd_volt_share_slaves_notify(cpufreq_data, volt_old);
exit_unlock:
	mutex_unlock(cpufreq_data->volt_lock);
	return ret;
}

/*
 * Sprd_cpufreq_set_target_index()  - cpufreq_set_target
 * @idx:        0 points to min freq, ascending order
 */
static int sprd_cpufreq_set_target_index(struct cpufreq_policy *policy,
					 unsigned int idx)
{
	int ret;

	/* Never dvfs until boot_done_timestamp */
	if (unlikely(boot_done_timestamp &&
		     time_after(jiffies, boot_done_timestamp))) {
		sprd_cpufreq_set_boost(policy, 0);
		sprd_cpufreq_driver.boost_enabled = false;
		pr_info("Disables boost it is %lu seconds after boot up\n",
			SPRD_CPUFREQ_DRV_BOOST_DURATOIN / HZ);
	}

	/*
	 * Boost_mode_flag is true and cpu is on max freq.
	 * So return 0 here,reject changing freq.
	 */
	if (unlikely(boost_mode_flag)) {
#if IS_ENABLED(CONFIG_ARM_SPRD_SW_CPUFREQ_MAXFREQ)
		if (policy->max >= policy->cpuinfo.max_freq)
#else
		if (policy->max > policy->cpuinfo.max_freq)
#endif
			return 0;

		sprd_cpufreq_set_boost(policy, 0);
		sprd_cpufreq_driver.boost_enabled = false;
		pr_info("Disables boost due to policy max(%d<%d)\n",
			policy->max, policy->cpuinfo.max_freq);
	}

	ret = sprd_cpufreq_set_target(policy->driver_data, idx, true);

	if (!ret)
		ret = sprd_freq_sync_slaves_notify(policy->driver_data,
						   idx, true);
	return ret;
}

static int sprd_cpufreq_init_slaves(struct sprd_cpufreq_driver_data *c_host,
				    struct device_node *np_host)
{
	int ret = 0, i, ic;
	struct device_node *np;
	struct sprd_cpufreq_driver_data *c;
	unsigned int cluster;
	char coreclk[15] = "core*_clk";

	if (!np_host)
		return -ENODEV;

	for (i = 0; i < SPRD_CPUFREQ_MAX_MODULE; i++) {
		np = of_parse_phandle(np_host, "cpufreq-sub-clusters", i);
		if (!np) {
			pr_debug("index %d not found sub-clusters\n", i);
			return 0;
		}

		pr_info("slave index %d name is found %s\n", i, np->full_name);

		if (of_property_read_u32(np, "cpufreq-cluster-id", &cluster)) {
			pr_err("index %d not found cpufreq_cluster_id\n", i);
			ret = -ENODEV;
			goto free_np;
		}

		if (cluster < SPRD_CPUFREQ_MAX_CLUSTER ||
		    cluster >= SPRD_CPUFREQ_MAX_MODULE) {
			pr_err("slave index %d custer %u is overflowed\n",
			       i, cluster);
			ret = -EINVAL;
			goto free_np;
		}

		if (!cpufreq_datas[cluster]) {
			c = kzalloc(sizeof(*c), GFP_KERNEL);
			if (!c) {
				ret = -ENOMEM;
				goto free_np;
			}
		} else {
			c = cpufreq_datas[cluster];
		}
		c_host->sub_cluster_bits |= BIT(cluster);
		c->cluster = cluster;
		c->clk = of_clk_get_by_name(np, "clk");
		if (IS_ERR(c->clk)) {
			pr_err("slave index %d failed to get clk, %ld\n",
			       i, PTR_ERR(c->clk));
			ret = PTR_ERR(c->clk);
			goto free_mem;
		} else {
			pr_debug("index %d clk[%lu]Khz", i,
				 clk_get_rate(c->clk) / 1000);
		}
		c->clks[0] = c->clk;
		for (ic = 1; ic < SPRD_MAX_CPUS_EACH_CLUSTER; ic++) {
			sprintf(coreclk, "core%d_clk", ic);
			c->clks[ic] = of_clk_get_by_name(np, coreclk);
		}
		c->clk_low_freq_p = of_clk_get_by_name(np, "clk_low");
		if (IS_ERR(c->clk_low_freq_p)) {
			pr_info("slave index %d clk_low is not defined\n", i);
			ret = PTR_ERR(c->clk_low_freq_p);
			goto free_clk;
		} else {
			c->clk_low_freq_p_max = clk_get_rate(c->clk_low_freq_p);
			pr_debug("index %d clk_low_freq_p_max[%lu]Khz\n",
				 i, c->clk_low_freq_p_max / 1000);
		}

		of_property_read_u32(np, "clk-high-freq-const",
				     &c->clk_high_freq_const);
		c->clk_high_freq_p = of_clk_get_by_name(np, "clk_high");
		if (IS_ERR(c->clk_high_freq_p)) {
			pr_info("slave index %d fail in getting clk_high%ld\n",
				i, PTR_ERR(c->clk_high_freq_p));
			ret = PTR_ERR(c->clk_high_freq_p);
			goto free_clk;
		} else {
			if (c->clk_high_freq_const)
				c->clk_high_freq_p_max =
					clk_get_rate(c->clk_high_freq_p);
			pr_debug("index %d clk_high_freq_p_max[%lu]Khz\n",
				 i, (clk_get_rate(c->clk_high_freq_p)) / 1000);
		}

		if (!c->clk_en) {
			ret = clk_prepare_enable(c->clk);
			if (ret) {
				pr_info("slave index %d cluster%d clk_en NG\n",
					i, cluster);
				goto free_clk;
			}
			for (ic = 1; ic < SPRD_MAX_CPUS_EACH_CLUSTER &&
			     !IS_ERR(c->clks[ic]); ic++) {
				ret = clk_prepare_enable(c->clks[ic]);
				if (ret) {
					pr_err("slave index %d clus%d clk NG\n",
					       i, cluster);
					goto free_clk;
				}
			}
			c->clk_en = 1;
		}
		c->freq_req = clk_get_rate(c->clk);

		if (of_property_read_u32(np, "voltage-tolerance", &c->volt_tol))
			c->volt_tol = DEF_VOLT_TOL;
		of_property_read_u32(np, "volt-share-hosts-bits",
				     &c->volt_share_hosts_bits);
		of_property_read_u32(np, "volt-share-masters-bits",
				     &c->volt_share_masters_bits);
		of_property_read_u32(np, "volt-share-slaves-bits",
				     &c->volt_share_slaves_bits);
		of_property_read_u32(np, "freq-sync-hosts-bits",
				     &c->freq_sync_hosts_bits);
		of_property_read_u32(np, "freq-sync-slaves-bits",
				     &c->freq_sync_slaves_bits);

		c->volt_lock = sprd_volt_share_lock(c);
		if (!c->volt_lock) {
			pr_info("slave index %d can find host volt_lock!\n", i);
			ret = -ENOMEM;
			goto free_clk;
		}

		c->reg = sprd_volt_share_reg(c);
		if (!c->reg || IS_ERR(c->reg)) {
			pr_err("failed to get regulator,%ld\n",
			       PTR_ERR(c->reg));
			ret = -ENODEV;
			goto free_clk;
		}
		c->volt_req = regulator_get_voltage(c->reg);
		c->temp_now = c_host->temp_now;
		ret = dev_pm_opp_of_add_table_binning(cluster, NULL, np, c);
		if (ret)
			goto free_clk;

		of_node_put(np);

		c->online = 1;
		cpufreq_datas[cluster] = c;
		pr_debug("now the slave index:%d", i);
		pr_debug("cpu_dev=%p\n", c->cpu_dev);
		pr_debug("cpufreq_custer_id=%u\n", c->cluster);
		pr_debug("online=%u\n", c->online);
		pr_debug("volt_lock=%p\n", c->volt_lock);
		pr_debug("reg=%p\n", c->reg);
		pr_debug("clk_high_freq_const=%u\n", c->clk_high_freq_const);
		pr_debug("clk=%p\n", c->clk);
		pr_debug("clk_low_freq_p=%p\n", c->clk_low_freq_p);
		pr_debug("clk_high_freq_p=%p\n", c->clk_high_freq_p);
		pr_debug("clk_low_freq_p_max=%lu\n", c->clk_low_freq_p_max);
		pr_debug("clk_high_freq_p_max=%lu\n", c->clk_high_freq_p_max);
		pr_debug("freq_req=%lu\n", c->freq_req);
		pr_debug("volt_tol=%u\n", c->volt_tol);
		pr_debug("volt_req=%lu\n", c->volt_req);
		pr_debug("volt_share_hosts_bits=%u\n",
			 c->volt_share_hosts_bits);
		pr_debug("volt_share_masters_bits=%u\n",
			 c->volt_share_masters_bits);
		pr_debug("volt_share_slaves_bits=%u\n",
			 c->volt_share_slaves_bits);
		pr_debug("freq_sync_hosts_bits=%u\n", c->freq_sync_hosts_bits);
		pr_debug("freq_sync_slv_bits=%u\n", c->freq_sync_slaves_bits);
	}

	return ret;
free_clk:
	if (!IS_ERR(c->clk)) {
		clk_disable_unprepare(c->clk);
		clk_put(c->clk);
		c->clk = ERR_PTR(-ENOENT);
		c->clks[0] = ERR_PTR(-ENOENT);
		for (ic = 1; ic < SPRD_MAX_CPUS_EACH_CLUSTER; ic++) {
			if (!IS_ERR(c->clks[ic])) {
				clk_disable_unprepare(c->clks[ic]);
				clk_put(c->clks[ic]);
			}
			c->clks[ic] = ERR_PTR(-ENOENT);
		}
	}
	if (!IS_ERR(c->clk_low_freq_p))
		clk_put(c->clk_low_freq_p);
	if (!IS_ERR(c->clk_high_freq_p))

		clk_put(c->clk_high_freq_p);
free_mem:
	kfree(c);
	cpufreq_datas[cluster] = NULL;
free_np:
	if (np)
		of_node_put(np);
	return ret;
}

/*
 * Cpufreqsw responds for the cpu hotplug situation where
 * the first big core cpu starting to be online.
 * Return 0 on success,otherwise on failure.
 */
static int sprd_cpufreqsw_online(unsigned int cpu)
{
	struct sprd_cpufreq_driver_data *c;
	unsigned int volt_tol_min = 0, index = 0;
	unsigned long volt_max_online = 0;

	c = sprd_cpufreq_data(cpu);
	if (!c || c->volt_share_masters_bits == 0) {
		pr_err("%s:get cpu%u sprd_cpufreq_data failed!", __func__, cpu);
		return -EINVAL;
	}
	mutex_lock(c->volt_lock);
	volt_tol_min = sprd_volt_tol_min(c, true);
	sprd_volt_req_max(c, &volt_max_online, true, true);

	if (c->volt_req > volt_max_online) {
		if (regulator_set_voltage_tol(c->reg,
					      c->volt_req,
					      volt_tol_min))
			pr_debug("fail to set voltage %lu\n", c->volt_req);
		else
			sprd_volt_share_slaves_notify(sprd_cpufreq_data(cpu),
						      c->volt_req);
	}
	c->online = 1;
	mutex_unlock(c->volt_lock);
	pr_debug("volt_req %lu, volt_max_online %lu, new volt=%lu\n",
		 c->volt_req, volt_max_online,
		 c->volt_req > volt_max_online ?
		 c->volt_req : volt_max_online);
	/* Notify slaves recover freq */
	for (index = 0; index < c->freqvolts; index++)
		if (c->volt_req >= c->freqvolt[index].volt)
			break;
	if (index < c->freqvolts)
		sprd_freq_sync_slaves_notify(c, c->freqvolts - 1 - index, true);

	if (c->temp_enable)
		queue_delayed_work(system_highpri_wq, &c->temp_work,
			msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));

	return 0;
}

/*
 * Cpufreqsw responds for the cpu hotplug situation where
 * the last big core cpu completed the hotplug action.
 * Return 0 on success,otherwise on failure.
 */
static int sprd_cpufreqsw_offline(unsigned int cpu)
{
	struct sprd_cpufreq_driver_data *c;

	c = sprd_cpufreq_data(cpu);
	if (!c || c->volt_share_masters_bits == 0) {
		pr_err("%s:get cpu%u sprd_cpufreq_data failed!", __func__, cpu);
		return -EINVAL;
	}

	mutex_lock(c->volt_lock);
	c->online = 0;
	mutex_unlock(c->volt_lock);
	sprd_freq_sync_slaves_notify(c, 0, true);

	if (c->temp_enable)
		cancel_delayed_work_sync(&c->temp_work);

	return 0;
}

static void sprd_cpufreq_temp_work_func(struct work_struct *work)
{
	int temp = 0, ret;
	unsigned int freq, cpu;
	struct cpumask cluster_online_mask;
	struct delayed_work *dwork = to_delayed_work(work);
	struct sprd_cpufreq_driver_data *c =
		container_of(dwork, struct sprd_cpufreq_driver_data, temp_work);

	if (!c) {
		pr_err("get temp work cluster info error\n");
		return;
	}

	if (IS_ERR_OR_NULL(c->cpu_tz)) {
		c->cpu_tz = thermal_zone_get_zone_by_name(c->tz_name);
		if (IS_ERR_OR_NULL(c->cpu_tz)) {
			pr_warn("failed to get cluster %u thmzone\n", c->cluster);
			queue_delayed_work(system_highpri_wq, &c->temp_work,
					   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));
			return;
		}

		pr_info("get cluster %u thmzone successfully\n", c->cluster);
	}

	ret = thermal_zone_get_temp(c->cpu_tz, &temp);
	if (ret) {
		pr_warn("failed to get the temp of cpu thmzone\n");
		return;
	}

	cpumask_and(&cluster_online_mask, &c->cpus, cpu_online_mask);
	cpu = cpumask_first(&cluster_online_mask);

	freq = sprd_cpufreq_update_opp(cpu, temp);
	if (freq)
		pr_info("cluster[%u] update max freq[%u] by temp[%d]\n",
			c->cluster, freq, temp);

	queue_delayed_work(system_highpri_wq, &c->temp_work,
			   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));
}

static int sprd_cpufreq_init(struct cpufreq_policy *policy)
{
	struct dev_pm_opp *opp;
	unsigned long freq_Hz = 0;
	int ret = 0, ic = 0;
	char coreclk[15] = "core*_clk";
	struct device *cpu_dev;
	struct regulator *cpu_reg = NULL;
	struct device_node *cpu_np;
	struct device_node *np, *np1;
	struct clk *cpu_clk;
	struct sprd_cpufreq_driver_data *c;
	unsigned int volt_tol = 0;
	unsigned int transition_latency = CPUFREQ_ETERNAL;
	unsigned long clk_low_freq_p_max = 0;
	struct cpufreq_frequency_table *freq_table = NULL;
	int cpu = 0;
	unsigned int cpumask = 0;
	cpumask_t cluster_cpumask;

	if (!policy) {
		pr_err("invalid cpufreq_policy\n");
		return -ENODEV;
	}

	if (topology_physical_package_id(policy->cpu) >= SPRD_CPUFREQ_MAX_CLUSTER) {
		pr_err("cpu%d in invalid cluster %d\n", policy->cpu,
		       topology_physical_package_id(policy->cpu));
		return -EINVAL;
	}

	cpu = policy->cpu;
	pr_debug("going to get cpu%d device\n", cpu);
	cpu_dev = get_cpu_device(cpu);
	if (IS_ERR(cpu_dev)) {
		pr_err("failed to get cpu%d device\n", cpu);
		return -ENODEV;
	}

	cpu_np = of_node_get(cpu_dev->of_node);
	if (!cpu_np) {
		pr_err("failed to find cpu%d node\n", cpu);
		return -ENOENT;
	}

	np = of_parse_phandle(cpu_np, "cpufreq-data", 0);
	np1 = of_parse_phandle(cpu_np, "cpufreq-data-v1", 0);
	if (!np && !np1) {
		pr_err("failed to find cpufreq-data for cpu%d\n", cpu);
		of_node_put(cpu_np);
		return -ENOENT;
	}
	if (np1)
		np = np1;

	if (sprd_cpufreq_data(cpu)) {
		c = sprd_cpufreq_data(cpu);
	} else {
		c = kzalloc(sizeof(*c), GFP_KERNEL);
		if (!c) {
			ret = -ENOMEM;
			goto free_np;
		}
	}

	of_property_read_u32(np, "volt-share-masters-bits",
			     &c->volt_share_masters_bits);
	of_property_read_u32(np, "volt-share-slaves-bits",
			     &c->volt_share_slaves_bits);
	of_property_read_u32(np, "freq-sync-hosts-bits",
			     &c->freq_sync_hosts_bits);
	of_property_read_u32(np, "freq-sync-slaves-bits",
			     &c->freq_sync_slaves_bits);
	pr_debug("read dts, masters_bits %u, slaves_bits %u\n",
		 c->volt_share_masters_bits,
		 c->volt_share_slaves_bits);

	if (!c->volt_lock) {
		c->volt_lock = sprd_volt_share_lock(c);
		if (!c->volt_lock) {
			c->volt_lock = kzalloc(sizeof(*c->volt_lock),
					       GFP_KERNEL);
			if (!c->volt_lock) {
				ret = -ENOMEM;
				goto free_mem;
			}
			mutex_init(c->volt_lock);
		}
	}

	mutex_lock(c->volt_lock);

	c->cluster = topology_physical_package_id(cpu);

	cpu_clk = of_clk_get_by_name(np, CORE_CLK);
	pr_debug("core_clk name is %s\n", __clk_get_name(cpu_clk));
	pr_debug("core_clk rates is %lu\n", (clk_get_rate(cpu_clk) / 1000));
	if (IS_ERR(cpu_clk)) {
		pr_err("failed to get cpu clock, %ld\n", PTR_ERR(cpu_clk));
		ret = PTR_ERR(cpu_clk);
		goto free_mem;
	}
	c->clks[0] = cpu_clk;
	for (ic = 1; ic < SPRD_MAX_CPUS_EACH_CLUSTER; ic++) {
		sprintf(coreclk, "core%d_clk", ic);
		c->clks[ic] = of_clk_get_by_name(np, coreclk);
		if (!IS_ERR(c->clks[ic]))
			pr_debug("got %s\n", coreclk);
	}
	c->clk_low_freq_p =
		of_clk_get_by_name(np, LOW_FREQ_CLK_PARENT);
	if (IS_ERR(c->clk_low_freq_p)) {
		pr_info("clk_low_freq_p is not defined\n");
		clk_low_freq_p_max = 0;
	} else {
		clk_low_freq_p_max = clk_get_rate(c->clk_low_freq_p);
		pr_debug("clk_low_freq_p_max[%lu]Khz\n",
			 clk_low_freq_p_max / 1000);
	}

	c->clk_high_freq_p = of_clk_get_by_name(np, HIGH_FREQ_CLK_PARENT);
	if (IS_ERR(c->clk_high_freq_p)) {
		pr_err("failed in getting clk_high_freq_p %ld\n",
		       PTR_ERR(c->clk_high_freq_p));
		ret = PTR_ERR(c->clk_high_freq_p);
		goto free_clk;
	}

	if (of_property_read_u32(np, "clock-latency", &transition_latency))
		transition_latency = CPUFREQ_ETERNAL;
	if (of_property_read_u32(np, "voltage-tolerance", &volt_tol))
		volt_tol = DEF_VOLT_TOL;

	if (!cpu_gpu_share_volt) {
		ret = sprd_get_cpu_gpu_volt_parameters(np);
		if (ret) {
			pr_err("get cpu gpu volt parameters failed!\n");
			goto free_clk;
		}
	}
	pr_debug("value of transition_latency %u, voltage_tolerance%u, cpu_gpu_share_volt%d\n",
		 transition_latency, volt_tol, cpu_gpu_share_volt);

	cpu_reg = sprd_volt_share_reg(c);
	if (!cpu_reg)
		cpu_reg = devm_regulator_get(cpu_dev, CORE_SUPPLY);
	if (IS_ERR(cpu_reg)) {
		pr_err("failed to get regulator, %ld\n", PTR_ERR(cpu_reg));
		ret = PTR_ERR(cpu_reg);
		goto free_clk;
	}

	if (cpu_gpu_share_volt)
		gpu_cpu_reg = cpu_reg;

	/* TODO: need to get new temperature from thermal zone after hotplug */
	if (cpufreq_datas[0])
		c->temp_now = cpufreq_datas[0]->temp_now;
	ret = dev_pm_opp_of_add_table_binning(is_big_cluster(cpu),
					      cpu_dev, np, c);

	if (ret < 0) {
		pr_err("failed to init opp table, %d\n", ret);
		goto free_reg;
	}

	pr_debug("going to verify opp with regulator\n");
	ret = sprd_verify_opp_with_regulator(cpu_dev, cpu_reg, volt_tol);
	if (ret > 0)
		transition_latency += ret * 1000;

	pr_debug("going to initialize freq_table\n");
	ret = dev_pm_opp_init_cpufreq_table(cpu_dev, &freq_table);
	if (ret) {
		pr_err("%d in initializing freq_table\n", ret);
		goto free_opp;
	}

	policy->freq_table = freq_table;

	c->update_opp = sprd_cpufreq_update_opp_common;

	pr_debug("going to prepare clock\n");
	if (!c->clk_en) {
		ret = clk_prepare_enable(cpu_clk);
		if (ret) {
			pr_err("CPU%d clk_prepare_enable failed\n",
			       policy->cpu);
			goto free_table;
		}
		for (ic = 1;
		ic < SPRD_MAX_CPUS_EACH_CLUSTER && !IS_ERR(c->clks[ic]);
		ic++) {
			pr_debug("going to prepare clock core%d\n", ic);
			ret = clk_prepare_enable(c->clks[ic]);
			if (ret) {
				pr_err("CPU%d clk_enable core%d failed\n",
				       policy->cpu, ic);
				goto free_table;
			}
		}
		c->clk_en = 1;
	}
#ifdef CONFIG_SMP
	/* CPUs in the same cluster share a clock and power domain. */
	ret = of_property_read_u32(np, "cpufreq-cluster-cpumask", &cpumask);
	if (ret) {
		pr_err("cpufreq cluster cpumask read fail\n");
		goto free_table;
	}
	cluster_cpumask.bits[0] = (unsigned long)cpumask;
	cpumask_or(policy->cpus, policy->cpus, &cluster_cpumask);
#endif

	c->online = 1;
	c->cpu_dev = cpu_dev;
	c->reg = cpu_reg;
	c->clk = cpu_clk;
	c->volt_tol = volt_tol;
	c->clk_low_freq_p_max = clk_low_freq_p_max;
	cpumask_copy(&c->cpus, policy->cpus);

	if (c->cluster < SPRD_CPUFREQ_MAX_CLUSTER &&
	    !cpufreq_datas[c->cluster]) {
		cpufreq_datas[c->cluster] = c;
		pr_debug("cpu%d got new cpufreq_data\n", cpu);
	}

	ret = sprd_cpufreq_init_slaves(c, np);
	if (ret)
		goto free_table;

	mutex_unlock(c->volt_lock);

	policy->driver_data = c;
	policy->clk = cpu_clk;
	policy->suspend_freq = policy->freq_table[0].frequency;
	policy->cur = clk_get_rate(cpu_clk) / 1000;
	policy->cpuinfo.transition_latency = transition_latency;
	policy->dvfs_possible_from_any_cpu = true;

	freq_Hz = policy->cur * 1000;
	opp = dev_pm_opp_find_freq_ceil(cpu_dev, &freq_Hz);
	c->volt_req = dev_pm_opp_get_voltage(opp);
	c->freq_req = dev_pm_opp_get_freq(opp);
	c->cpufreq_online = sprd_cpufreqsw_online;
	c->cpufreq_offline = sprd_cpufreqsw_offline;

	if (cpu_gpu_share_volt) {
		sprd_cpu_gpu_volt_lock();
		sprd_set_cpu_target_voltage(c->volt_req);
		sprd_cpu_gpu_volt_unlock();
	}
	dev_pm_opp_put(opp);

	c->temp_enable = true;
	INIT_DELAYED_WORK(&c->temp_work, sprd_cpufreq_temp_work_func);

	ret = freq_qos_add_request(&policy->constraints,
					   &c->max_req, FREQ_QOS_MAX,
					   FREQ_QOS_MAX_DEFAULT_VALUE);
	if (ret < 0) {
		pr_err("add cluster %u freq qos error(%d)\n", c->cluster, ret);
		goto free_table;
	}

	queue_delayed_work(system_highpri_wq, &c->temp_work,
				   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));

	if (of_property_read_string(np, "sprd,thmzone-names", &c->tz_name)) {
		pr_err("get cluster %u thmzone name error\n", c->cluster);
		return -EINVAL;
	}

	pr_info("init cpu%d is ok, freq=%ld, freq_req=%ld, volt_req=%ld\n",
		cpu, freq_Hz, c->freq_req, c->volt_req);

	goto free_np;

free_table:
	if (policy->freq_table)
		dev_pm_opp_free_cpufreq_table(cpu_dev, &policy->freq_table);
free_opp:
	dev_pm_opp_of_remove_table(cpu_dev);
free_reg:
	if (!IS_ERR(cpu_reg))
		devm_regulator_put(cpu_reg);
free_clk:
	if (!IS_ERR(cpu_clk)) {
		clk_disable_unprepare(cpu_clk);
		clk_put(cpu_clk);
		policy->clk = ERR_PTR(-ENOENT);
		c->clks[0] = ERR_PTR(-ENOENT);
		for (ic = 1; ic < SPRD_MAX_CPUS_EACH_CLUSTER; ic++) {
			if (!IS_ERR(c->clks[ic])) {
				clk_disable_unprepare(c->clks[ic]);
				clk_put(c->clks[ic]);
			}
			c->clks[ic] = ERR_PTR(-ENOENT);
		}
	}
	if (!IS_ERR(c->clk_low_freq_p))
		clk_put(c->clk_low_freq_p);
	if (!IS_ERR(c->clk_high_freq_p))
		clk_put(c->clk_high_freq_p);
free_mem:
	if (c->volt_lock) {
		mutex_unlock(c->volt_lock);
		mutex_destroy(c->volt_lock);
		kfree(c->volt_lock);
	}
	kfree(c);
	sprd_cpufreq_data(cpu) = NULL;
free_np:
	if (np)
		of_node_put(np);
	if (np1)
		of_node_put(np1);
	if (cpu_np)
		of_node_put(cpu_np);
	return ret;
}

static int sprd_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct sprd_cpufreq_driver_data *c;
	int ic;

	if (!policy)
		return -ENODEV;

	c = policy->driver_data;
	if (!c)
		return 0;

	mutex_lock(c->volt_lock);

	if (c->temp_enable) {
		cancel_delayed_work_sync(&c->temp_work);
		freq_qos_remove_request(&c->max_req);
	}

	if (policy->freq_table)
		dev_pm_opp_free_cpufreq_table(c->cpu_dev, &policy->freq_table);

	if (!IS_ERR(policy->clk)) {
		clk_put(policy->clk);
		policy->clk = ERR_PTR(-ENOENT);
		c->clk = ERR_PTR(-ENOENT);
		c->clks[0] = ERR_PTR(-ENOENT);
		for (ic = 1; ic < SPRD_MAX_CPUS_EACH_CLUSTER; ic++) {
			if (!IS_ERR(c->clks[ic]))
				clk_put(c->clks[ic]);
			c->clks[ic] = ERR_PTR(-ENOENT);
		}
	}
	if (!IS_ERR(c->clk_low_freq_p)) {
		clk_put(c->clk_low_freq_p);
		c->clk_low_freq_p = ERR_PTR(-ENOENT);
	}
	if (!IS_ERR(c->clk_high_freq_p)) {
		clk_put(c->clk_high_freq_p);
		c->clk_high_freq_p = ERR_PTR(-ENOENT);
	}

	policy->driver_data = NULL;

	mutex_unlock(c->volt_lock);
	return 0;
}

static int sprd_cpufreq_table_verify(struct cpufreq_policy_data *policy_data)
{
	return cpufreq_generic_frequency_table_verify(policy_data);
}

static unsigned int sprd_cpufreq_get(unsigned int cpu)
{
	return cpufreq_generic_get(cpu);
}

static int sprd_cpufreq_suspend(struct cpufreq_policy *policy)
{
	struct sprd_cpufreq_driver_data *c;

	if (!policy)
		return -ENODEV;

	c = policy->driver_data;
	if (!c)
		return 0;

	if (c->temp_enable)
		cancel_delayed_work_sync(&c->temp_work);

	/* Do not change freq in userpace mode */
	if (policy && !strcmp(policy->governor->name, "userspace")) {
		pr_debug("%s: do nothing for governor->name %s\n",
			 __func__, policy->governor->name);
		return 0;
	}

	return cpufreq_generic_suspend(policy);
}

static int sprd_cpufreq_resume(struct cpufreq_policy *policy)
{
	struct sprd_cpufreq_driver_data *c;

	if (!policy)
		return -ENODEV;

	c = policy->driver_data;
	if (!c)
		return 0;

	if (c->temp_enable)
		queue_delayed_work(system_highpri_wq, &c->temp_work,
				   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));

	/* Do not change freq in userpace mode */
	if (policy && !strcmp(policy->governor->name, "userspace")) {
		pr_debug("%s: do nothing for governor->name %s\n",
			 __func__, policy->governor->name);
		return 0;
	}

	return cpufreq_generic_suspend(policy);
}

/*
 * Sprd_cpufreq_set_boost: set cpufreq driver to be boost mode.
 *
 * @state: 0->disable boost mode;1->enable boost mode;
 *
 * Return: zero on success, otherwise non-zero on failure.
 */
static int sprd_cpufreq_set_boost(struct cpufreq_policy *policy, int state)
{
	boot_done_timestamp = 0;
	boost_mode_flag = state;
	pr_debug("%s:boost_mode_flag=%d\n", __func__, boost_mode_flag);

	return 0;
}

static struct cpufreq_driver sprd_cpufreq_driver = {
	.name = "sprd-cpufreq",
	.flags = CPUFREQ_NEED_UPDATE_LIMITS | CPUFREQ_NEED_INITIAL_FREQ_CHECK
				| CPUFREQ_HAVE_GOVERNOR_PER_POLICY
				| CPUFREQ_IS_COOLING_DEV,
	.init = sprd_cpufreq_init,
	.exit = sprd_cpufreq_exit,
	.verify = sprd_cpufreq_table_verify,
	.register_em = cpufreq_register_em_with_opp,
	.target_index = sprd_cpufreq_set_target_index,
	.get = sprd_cpufreq_get,
	.suspend = sprd_cpufreq_suspend,
	.resume = sprd_cpufreq_resume,
	.attr = cpufreq_generic_attr,
	/* Platform specific boost support code */
	.boost_enabled = true,
	.set_boost = sprd_cpufreq_set_boost,
};

static int sprd_cpufreq_probe(struct platform_device *pdev)
{
	struct device *cpu_dev = NULL;
	struct clk *cpu_clk = NULL;
	struct regulator *cpu_reg;
	struct device_node *cpu_np;
	struct device_node *np = NULL, *np1 = NULL;
	int ret = 0;
	unsigned int cpu;
	struct nvmem_cell *cell;

	boot_done_timestamp = jiffies + SPRD_CPUFREQ_DRV_BOOST_DURATOIN;

	for_each_present_cpu(cpu) {
		cpu_dev = get_cpu_device(cpu);
		if (!cpu_dev) {
			dev_err(&pdev->dev, "failed to get cpu%d device\n",
				cpu);
			return -ENODEV;
		}

		cpu_np = of_node_get(cpu_dev->of_node);
		if (!cpu_np) {
			dev_err(&pdev->dev, "failed to find cpu%d node\n",
				cpu);
			return -ENODEV;
		}

		np = of_parse_phandle(cpu_np, "cpufreq-data", 0);
		np1 = of_parse_phandle(cpu_np, "cpufreq-data-v1", 0);
		if (!np && !np1) {
			dev_err(&pdev->dev, "failed to find cpu%u cpufreq-data!\n",
				cpu);
			of_node_put(cpu_np);
			return -ENOENT;
		}
		if (np1)
			np = np1;

		cell = of_nvmem_cell_get(np, "dvfs_bin");
		ret = PTR_ERR(cell);

		if (!IS_ERR(cell)) {
			nvmem_cell_put(cell);
			ret = 0;
		} else if (ret == -EPROBE_DEFER) {
			goto put_np;
		}

		cpu_clk = of_clk_get_by_name(np, CORE_CLK);
		if (IS_ERR(cpu_clk)) {
			dev_err(&pdev->dev, "failed in cpu%u clock getting,%ld\n",
				cpu, PTR_ERR(cpu_clk));
			ret = PTR_ERR(cpu_clk);
			goto put_np;
		}

		cpu_reg = devm_regulator_get(cpu_dev, "cpu");
		if (IS_ERR_OR_NULL(cpu_reg)) {
			dev_err(&pdev->dev, "failed in cpu%u reg getting,%ld\n",
				cpu, PTR_ERR(cpu_reg));
			ret = PTR_ERR(cpu_reg);
			goto put_clk;
		}

		/*
		 * Put regulator and clock here, before registering
		 * the driver,we will get them again while per cpu
		 * initialization in cpufreq_init.
		 */
		if (!IS_ERR(cpu_reg)) {
			pr_debug("putting regulator\n");
			devm_regulator_put(cpu_reg);
		}

put_clk:
		if (!IS_ERR(cpu_clk)) {
			pr_debug("putting clk\n");
			clk_put(cpu_clk);
		}

put_np:
		if (np)
			of_node_put(np);
		if (np1)
			of_node_put(np1);
		if (cpu_np)
			of_node_put(cpu_np);

		/*
		 * Ret is not zero? we encountered an error.
		 * Return failure/probe deferred
		 */
		if (ret) {
			dev_err(&pdev->dev, "sprd-cpufreq probe failed!!\n");
			return ret;
		}
	}

	ret = sprd_cpufreq_cpuhp_setup();
	if (ret < 0) {
		dev_err(&pdev->dev, "cpufreq_hotplug_setup failed!\n");
		return ret;
	}

	ret = cpufreq_register_driver(&sprd_cpufreq_driver);
	if (ret)
		dev_err(&pdev->dev, "cpufreq driver register failed %d\n", ret);

	return ret;
}

static int sprd_cpufreq_remove(struct platform_device *pdev)
{
	return cpufreq_unregister_driver(&sprd_cpufreq_driver);
}

unsigned int sprd_cpufreq_update_opp(int cpu, int temp_now)
{
	struct device_node *cpu_np, *cpufreq_np;
	struct device *cpu_dev;
	struct cpufreq_policy policy;
	unsigned int max_freq = 0;
	struct sprd_cpufreq_driver_data *data = NULL;

	if (cpufreq_get_policy(&policy, cpu)) {
		pr_debug("%s: No cpu data found\n", __func__);
		return 0;
	}

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		pr_err("%s: Failed to get cpu0 device\n", __func__);
		return 0;
	}

	cpu_np = of_node_get(cpu_dev->of_node);
	if (!cpu_np) {
		pr_err("%s: Failed to find cpu node\n", __func__);
		return 0;
	}

	cpufreq_np = of_parse_phandle(cpu_np, "cpufreq-data-v1", 0);
	if (!cpufreq_np) {
		pr_err("%s: No cpufreq data found for cpu0\n", __func__);
		of_node_put(cpu_np);
		return 0;
	}

	pr_debug("%s: cluster name is: %s\n", __func__, cpufreq_np->full_name);

	if (!strcmp(cpufreq_np->full_name, "cpufreq-clus0")) {
		data = policy.driver_data;
		if (IS_ERR_OR_NULL(data) || !data->update_opp)
			return 0;

		max_freq = data->update_opp(cpu, temp_now);
	} else
		pr_err("%s: Error name of cpufreq data v1!\n", __func__);

	of_node_put(cpufreq_np);
	of_node_put(cpu_np);

	return max_freq;
}

static const struct of_device_id sprd_swdvfs_of_match[] = {
	{
		.compatible = "sprd,sharkl3-swdvfs",
	},
	{
		.compatible = "sprd,pike2-swdvfs",
	},
	{
		.compatible = "sprd,sharkle-swdvfs",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_swdvfs_of_match);

static struct platform_driver sprd_cpufreq_platdrv = {
	.probe		= sprd_cpufreq_probe,
	.remove		= sprd_cpufreq_remove,
	.driver = {
		.name	= "sprd-swdvfs",
		.of_match_table	= sprd_swdvfs_of_match,
	},
};
module_platform_driver(sprd_cpufreq_platdrv);

MODULE_DESCRIPTION("spreadtrum cpufreq driver");
MODULE_LICENSE("GPL v2");
