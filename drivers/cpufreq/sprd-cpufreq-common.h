/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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
#ifndef SPRD_CPUFREQ_COMMON_H
#define SPRD_CPUFREQ_COMMON_H

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of_platform.h>

/* Default voltage_tolerance */
#define DEF_VOLT_TOL			0
/* Regulator Supply */
#define CORE_SUPPLY			"cpu"
/* Core Clocks */
#define CORE_CLK			"core_clk"
#define LOW_FREQ_CLK_PARENT		"low_freq_clk_parent"
#define HIGH_FREQ_CLK_PARENT		"high_freq_clk_parent"
/*0-cluster0; 1-custer1;  2/3-cci or fcm*/
#define SPRD_CPUFREQ_MAX_MODULE		7
/*0-cluster0; 1-custer1*/
#define SPRD_CPUFREQ_MAX_CLUSTER	2
#define SPRD_MAX_CPUS_EACH_CLUSTER	4
#define SPRD_CPUFREQ_MAX_FREQ_VOLT	10
#define SPRD_CPUFREQ_MAX_TEMP		4
#define SPRD_CPUFREQ_TEMP_FALL_HZ	(2 * HZ)
#define SPRD_CPUFREQ_TEMP_UPDATE_HZ	(HZ / 2)
#define SPRD_CPUFREQ_TEMP_MAX		200
#define SPRD_CPUFREQ_TEMP_MIN		(-200)
#define SPRD_CPUFREQ_DRV_BOOST_DURATOIN	(60ul * HZ)
#define DVFS_TEMP_UPDATE_MS		(1500)

#define sprd_cpufreq_data(cpu) \
	cpufreq_datas[topology_physical_package_id(cpu)]
#define is_big_cluster(cpu) (topology_physical_package_id(cpu) != 0)
/* Sprd bining surpport */
#define SPRD_BINNING_MAX		4
#define SPRD_BINNING_MIN		0

#define ONLINE				1
#define OFFLINE				0

struct sprd_cpufreq_group {
	unsigned long freq;
	unsigned long volt;
};

struct sprd_cpufreq_driver_data {
	unsigned int cluster;
	unsigned int online;
	/*Lock the shared resource*/
	struct mutex *volt_lock;
	struct device *cpu_dev;
	struct regulator *reg;
	/*Const means clk_high is constant clk source*/
	unsigned int clk_high_freq_const;
	unsigned int clk_en;
	/*First cpu clk*/
	struct clk *clk;
	/*All cpus clk in cluster*/
	struct clk *clks[SPRD_MAX_CPUS_EACH_CLUSTER];
	struct clk *clk_low_freq_p;
	struct clk *clk_high_freq_p;
	unsigned long clk_low_freq_p_max;
	unsigned long clk_high_freq_p_max;
	/*Voltage tolerance in percentage */
	unsigned int volt_tol;
	unsigned int volt_tol_var;
	/*BitX = 1 means sub_cluster exits*/
	unsigned int sub_cluster_bits;
	/*Volt requested by this cluster*/
	unsigned long volt_req;/*uv*/
	unsigned int volt_share_hosts_bits;
	unsigned int volt_share_masters_bits;
	unsigned int volt_share_slaves_bits;
	unsigned long freq_req;/*hz*/
	unsigned int freq_sync_hosts_bits;
	unsigned int freq_sync_slaves_bits;
	unsigned int freqvolts;
	/*Max freq is on freqvolt[0]*/
	struct sprd_cpufreq_group freqvolt[SPRD_CPUFREQ_MAX_FREQ_VOLT];
	unsigned int temp_max_freq;
	int temp_list[SPRD_CPUFREQ_MAX_TEMP];
	int temp_max;
	int temp_top;
	int temp_now;
	int temp_bottom;
	unsigned long temp_fall_time;
	/*cpufreq points to hotplug notify*/
	int (*cpufreq_online)(unsigned int cpu);
	int (*cpufreq_offline)(unsigned int cpu);
	/*CUFREQ points to update opp by temp*/
	unsigned int (*update_opp)(int cpu, int temp_now);
	/* judge soc version */
	bool version_judge;

	struct cpumask cpus;
	cpumask_t cluster_cpumask;
	struct delayed_work temp_work;
	bool temp_enable;
	struct freq_qos_request max_req;
	struct thermal_zone_device *cpu_tz;
	const char *tz_name;
};

#define CPUFREQHW_NAME_LEN			30

struct sprd_cpudvfs_ops {
	bool (*probed)(void *drvdata, int cluster);
	bool (*enable)(void *drvdata, int cluster, bool en);
	int (*opp_add)(void *drvdata, unsigned int cluster,
		       unsigned long hz_freq, unsigned long u_volt,
		       int opp_idx);
	int (*set)(void *drvdata, u32 cluster, u32 opp_idx);
	unsigned int (*get)(void *drvdata, int cluster);
	int (*udelay_update)(void *drvdata, int cluster);
	int (*index_tbl_update)(void *drvdata, char *opp_name, int cluster);
	int (*idle_pd_volt_update)(void *drvdata, int cluster);
};

struct sprd_cpudvfs_device {
	char  name[CPUFREQHW_NAME_LEN];
	struct sprd_cpudvfs_ops ops;
	void *archdata;
};

extern struct sprd_cpufreq_driver_data
	*cpufreq_datas[SPRD_CPUFREQ_MAX_MODULE];

int sprd_cpufreq_bin_main(struct device_node *np, u32 *p_binning);
int dev_pm_opp_of_add_table_binning(int cluster,
				    struct device *dev,
				    struct device_node *np_cpufreq_data,
				struct sprd_cpufreq_driver_data *cpufreq_data);

int sprd_cpufreq_cpuhp_setup(void);

unsigned int sprd_cpufreq_update_opp_common(int cpu, int temp_now);

#endif
