// SPDX-License-Identifier: GPL-2.0

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
#define pr_fmt(fmt)  "sprd_cpufreq: " fmt

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of_platform.h>
#include <linux/compiler.h>
#include "sprd-cpufreq-common.h"

char global_opp_string[30] = "operating-points-v0";
struct sprd_cpufreq_driver_data *cpufreq_datas[SPRD_CPUFREQ_MAX_MODULE];
EXPORT_SYMBOL_GPL(cpufreq_datas);

__weak struct sprd_cpudvfs_device *sprd_hardware_dvfs_device_get(void)
{
	pr_debug("use weak sprd hardware dvfs device get\n");
	return NULL;
}

int sprd_cpufreq_read_soc_version_opp_string(char *opp_string)
{
	struct device_node *hwf;
	char ver_str[30] = "-";
	const char *value;

	hwf = of_find_node_by_path("/hwfeature/auto");
	if (IS_ERR_OR_NULL(hwf)) {
		pr_err("NO hwfeature/auto node found\n");
		return PTR_ERR(hwf);
	}

	value = of_get_property(hwf, "efuse", NULL);
	if (strcmp(value, "SC9863A") && strcmp(value, "SC9863A1")) {
		pr_err("the soc version is invalid, string is %s\n", value);
		return -EINVAL;
	}

	strcat(ver_str, value);
	strcat(opp_string, ver_str);

	pr_info("the cpu version: %s\n", opp_string);

	return 0;
}
EXPORT_SYMBOL(sprd_cpufreq_read_soc_version_opp_string);

static int sprd_cpufreq_bin_read(struct device_node *np,
				 const char *cell_id,
				 u32 *val)
{
	struct nvmem_cell *cell;
	void *buf;
	size_t len = 0;

	cell = of_nvmem_cell_get(np, cell_id);
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(val, buf, min(len, sizeof(u32)));
	kfree(buf);
	buf = NULL;

	return 0;
}

static int sprd_cpufreq_bin_low_volt(struct device_node *np, u32 *p_binning)
{
	u32 binning = 0;
	int ret;

	if (!np || !p_binning)
		return -ENOENT;

	ret = sprd_cpufreq_bin_read(np, "dvfs_bin_low_volt", &binning);
	if (ret) {
		/* dvfs_bin_low_volt is optional */
		pr_debug("can not get dvfs_bin_low_volt ret %d\n", ret);
		return ret;
	}

	pr_debug("%s get BIN %u for low volt\n", __func__, binning);

	*p_binning = binning;

	return 0;
}

int sprd_cpufreq_bin_main(struct device_node *np, u32 *p_binning)
{
	u32 binning = 0;
	int ret;

	if (!np || !p_binning) {
		pr_warn("inputs are NULL!\n");
		return -ENOENT;
	}

	ret = sprd_cpufreq_bin_read(np, "dvfs_bin", &binning);
	if (ret) {
		pr_warn("can not get dvfs_bin ret %d\n", ret);
		return ret;
	}

	pr_debug("%s get BIN %u\n", __func__, binning);

	if (!binning || binning > 4)
		return -EINVAL;

	*p_binning = binning;

	return 0;
}

static int sprd_cpufreq_bin_temp(struct device_node *np,
				 struct sprd_cpufreq_driver_data *cpufreq_data,
				 char *opp_temp)
{
	const struct property *prop;
	int i, temp_index;
	u32 temp_threshold;
	const __be32 *val;

	if (!np || !cpufreq_data || !opp_temp) {
		pr_warn("inputs are NULL\n");
		return -ENOENT;
	}

	prop = of_find_property(np, "sprd,cpufreq-temp-threshold", NULL);
	if (!prop)
		return -ENODATA;

	if (prop->length < sizeof(u32)) {
		pr_err("Invalid %s(prop->length %d)\n",
			__func__, prop->length);
		return -EINVAL;
	}

	cpufreq_data->temp_max_freq = 0;
	cpufreq_data->temp_max = 0;
	temp_index = -1;
	val = prop->value;

	for (i = 0;
	     i < (prop->length / sizeof(u32)) && i < SPRD_CPUFREQ_MAX_TEMP;
	     i++) {
		/* TODO: need to compatible with negative degree */
		temp_threshold = be32_to_cpup(val++);
		if (cpufreq_data->temp_now >= temp_threshold) {
			temp_index = i;
			sprintf(opp_temp, "-%d", temp_threshold);
		}
		cpufreq_data->temp_list[i] = temp_threshold;
		cpufreq_data->temp_max++;
		pr_debug("found temp %u\n", temp_threshold);
	}

	cpufreq_data->temp_bottom = temp_index < 0 ?
		SPRD_CPUFREQ_TEMP_MIN :
		cpufreq_data->temp_list[temp_index];
	cpufreq_data->temp_top = (temp_index + 1) >= cpufreq_data->temp_max ?
		SPRD_CPUFREQ_TEMP_MAX :
		cpufreq_data->temp_list[temp_index + 1];

	pr_debug("max num=%d bottom=%d top=%d\n",
		 cpufreq_data->temp_max,
		 cpufreq_data->temp_bottom,
		 cpufreq_data->temp_top);

	pr_debug("%s[%s] by temp %d\n",
		 __func__, opp_temp, cpufreq_data->temp_now);

	return 0;
}

/* Initializes OPP tables based on old-deprecated bindings */
int dev_pm_opp_of_add_table_binning(int cluster,
				    struct device *dev,
				    struct device_node *np_cpufreq_in,
				    struct sprd_cpufreq_driver_data *cdata)
{
	struct device_node *np_cpufreq, *np_cpu;
	const struct property *prop = NULL;
	struct sprd_cpudvfs_device *pdevice;
	struct sprd_cpudvfs_ops *driver;
	char opp_string[30] = "operating-points";
	char buf[30] = "";
	int count = 0, ret = 0, index = 0;
	u32 binning = 0, binning_low_volt = 0;
	const __be32 *val;
	int nr;
	bool ver_judge;

	if ((!dev && !np_cpufreq_in) || !cdata) {
		pr_err("empty input parameter\n");
		return -ENOENT;
	}

	if (!np_cpufreq_in) {
		np_cpu = of_node_get(dev->of_node);
		if (!np_cpu) {
			dev_err(dev, "sprd_cpufreq: failed to find cpu\n");
			return -ENOENT;
		}

		np_cpufreq = of_parse_phandle(np_cpu,
						   "cpufreq-data-v1", 0);
		if (!np_cpufreq) {
			dev_err(dev, "sprd_cpufreq: failed to get cpufreq\n");
			of_node_put(np_cpu);
			return -ENOENT;
		}
		pr_debug("%s: created np_cpufreq\n", __func__);
	} else {
		np_cpufreq = np_cpufreq_in;
	}

	ver_judge = of_property_read_bool(np_cpufreq, "sprd,multi-version");
	if (ver_judge) {
		pr_info("dts node need to distinguish version\n");
		cdata->version_judge = ver_judge;
		if (cdata->version_judge) {
			ret = sprd_cpufreq_read_soc_version_opp_string(opp_string);
			if (ret)
				pr_err("can't read soc version, opp_string:%s\n", opp_string);
		}
	}
	index = strlen(opp_string);
	ret = sprd_cpufreq_bin_main(np_cpufreq, &binning);
	if (ret == -EPROBE_DEFER)
		goto exit;

	/* got cpu BIN */
	if (!ret) {
		pr_debug("%s: binning=0x%x by BIN\n", __func__, binning);
		if (binning > 0) {
			opp_string[index++] = '-';
			opp_string[index++] = '0' + binning;
			opp_string[index] = '\0';
		}
		if (!sprd_cpufreq_bin_low_volt(np_cpufreq,
					       &binning_low_volt)) {
			opp_string[index++] = '-';
			opp_string[index++] = '0' + binning_low_volt;
			opp_string[index] = '\0';
		}
		/*select dvfs table by temp only if bin is not zero*/
		if (!sprd_cpufreq_bin_temp(np_cpufreq, cdata, buf))
			strcat(opp_string, buf);
	} else {
		ret = 0;
	}
	/* TODO: else get dvfs table by wafer id */

	pr_err("opp_string[%s]\n", opp_string);

	prop = of_find_property(np_cpufreq, opp_string, NULL);
	if (!prop || !prop->value) {
		pr_err("%s: not found opp_string\n", __func__);
		ret = -ENODATA;
		goto exit;
	}
	/*
	 * Each OPP is a set of tuples consisting of frequency and
	 * voltage like <freq-kHz vol-uV>.
	 */
	nr = prop->length / sizeof(u32);
	if (nr % 2) {
		dev_err(dev, "Invalid OPP list\n");
		ret = -EINVAL;
		goto exit;
	}

	cdata->freqvolts = 0;
	val = prop->value;
	while (nr) {
		unsigned long freq = be32_to_cpup(val++) * 1000;
		unsigned long volt = be32_to_cpup(val++);

		if (dev)
			dev_pm_opp_remove(dev, freq);

		if (dev && dev_pm_opp_add(dev, freq, volt)) {
			dev_warn(dev, "dev_pm Failed to add OPP %ld\n", freq);
		} else {
			if (freq / 1000 > cdata->temp_max_freq)
				cdata->temp_max_freq = freq / 1000;
		}
		if (count < SPRD_CPUFREQ_MAX_FREQ_VOLT) {
			cdata->freqvolt[count].freq = freq;
			cdata->freqvolt[count].volt = volt;
			cdata->freqvolts++;
		}

		count++;
		nr -= 2;
	}

	pdevice = sprd_hardware_dvfs_device_get();

	if (!pdevice)
		goto exit;

	driver = &pdevice->ops;
	if (!driver->probed || !driver->opp_add) {
		pr_err("driver opertions is empty\n");
		ret = -EINVAL;
		goto exit;
	}

	if (!driver->probed(pdevice->archdata, cluster)) {
		pr_err("the cpu dvfs device has not been probed\n");
		ret = -EINVAL;
		goto exit;
	}

	while (count-- > 0)
		driver->opp_add(pdevice->archdata, cluster,
				cdata->freqvolt[count].freq,
				cdata->freqvolt[count].volt,
				cdata->freqvolts - 1 - count);

	/*
	 * Need to update delay cycles and hardware dvfs index tables in L5
	 * family socs when switching operating-points tables.
	 */
	if (!driver->udelay_update && !driver->index_tbl_update)
		goto exit;

	if (!(driver->udelay_update && driver->index_tbl_update)) {
		pr_err("empty hardware dvfs operations\n");
		ret = -EINVAL;
		goto exit;
	}

	/*
	 * Update the delay cycles for dvfs module to wait the pmic to finish
	 * adjusting voltage.
	 */
	ret = driver->udelay_update(pdevice->archdata, cluster);
	if (ret)
		goto exit;

	/*
	 * Update the hardware dvfs index map tables, since the operating-points
	 * tables have been changed by temperature or corner chip id.
	 */
	ret = driver->index_tbl_update(pdevice->archdata, opp_string, cluster);
	if (ret)
		goto exit;

	/*
	 * Update the pd voltage in dvfs idle, if necessary
	 */
	if (driver->idle_pd_volt_update)
		ret = driver->idle_pd_volt_update(pdevice->archdata, cluster);

exit:
	/* should put np opened by this func */
	if (np_cpufreq_in == NULL) {
		of_node_put(np_cpufreq);
		of_node_put(np_cpu);
	}

	pr_debug("%s: exit %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_opp_of_add_table_binning);

static int dev_pm_opp_of_add_table_binning_slave(
	struct sprd_cpufreq_driver_data *c_host,
	int temp_now)
{
	struct device_node *np = NULL, *np_host = NULL;
	unsigned int cluster = SPRD_CPUFREQ_MAX_MODULE;
	struct device_node *cpu_np = NULL;
	struct device *cpu_dev;
	int ret = 0, i;

	if (!c_host) {
		pr_err("dvfs host device is NULL\n");
		return -ENODEV;
	}

	if (c_host->sub_cluster_bits == 0)
		return 0;

	cpu_dev = c_host->cpu_dev;
	if (!cpu_dev) {
		pr_err("cpu device is null.\n");
		return -ENODEV;
	}
	cpu_np = of_node_get(cpu_dev->of_node);
	if (!cpu_np) {
		pr_err("failed to find cpu node\n");
		return -ENOENT;
	}

	np_host = of_parse_phandle(cpu_np, "cpufreq-data-v1", 0);
	if (!np_host) {
		pr_err("Can not find cpufreq node in dts\n");
		of_node_put(cpu_np);
		return -ENOENT;
	}

	for (i = 0; i < SPRD_CPUFREQ_MAX_MODULE; i++) {
		np = of_parse_phandle(np_host, "cpufreq-sub-clusters", i);
		if (!np) {
			pr_debug("index %d not found sub-clusters\n", i);
			goto free_np;
		}
		pr_debug("slave index %d name is found %s\n", i, np->full_name);

		if (of_property_read_u32(np, "cpufreq-cluster-id", &cluster)) {
			pr_err("index %d not found cpufreq_custer_id\n", i);
			ret = -ENODEV;
			goto free_np;
		}
		if (cluster >= SPRD_CPUFREQ_MAX_MODULE ||
		    !cpufreq_datas[cluster]) {
			pr_err("index %d cluster %u is NULL\n", i, cluster);
			continue;
		}
		cpufreq_datas[cluster]->temp_now = temp_now;
		ret = dev_pm_opp_of_add_table_binning(cluster,
						      NULL,
						      np,
						      cpufreq_datas[cluster]);
		if (ret)
			goto free_np;
	}

free_np:
	if (np)
		of_node_put(np);
	if (np_host)
		of_node_put(np_host);
	return ret;
}

/**
 * sprd_cpufreq_update_opp_common() - returns the max freq of a cpu
 * and update dvfs table by temp_now
 * @cpu: which cpu you want to update dvfs table
 * @temp_now: current temperature on this cpu, mini-degree.
 *
 * Return:
 * 1.cluster is not working, then return 0
 * 2.succeed to update dvfs table
 * then return max freq(KHZ) of this cluster
 */
unsigned int sprd_cpufreq_update_opp_common(int cpu, int temp_now)
{
	struct sprd_cpufreq_driver_data *data;
	unsigned int max_freq = 0;
	int cluster;

	temp_now = temp_now / 1000;
	if (temp_now <= SPRD_CPUFREQ_TEMP_MIN ||
	    temp_now >= SPRD_CPUFREQ_TEMP_MAX)
		return 0;

	cluster = topology_physical_package_id(cpu);
	if (cluster > SPRD_CPUFREQ_MAX_CLUSTER) {
		pr_err("cpu%d is overflowd %d\n", cpu,
		       SPRD_CPUFREQ_MAX_CLUSTER);
		return -EINVAL;
	}

	data = cpufreq_datas[cluster];

	if (data && data->online && data->temp_max > 0) {
		/* Never block IPA thread */
		if (!mutex_trylock(data->volt_lock))
			return 0;
		data->temp_now = temp_now;
		if (temp_now < data->temp_bottom && !data->temp_fall_time)
			data->temp_fall_time = jiffies +
					       SPRD_CPUFREQ_TEMP_FALL_HZ;
		if (temp_now >= data->temp_bottom)
			data->temp_fall_time = 0;
		if (temp_now >= data->temp_top || (data->temp_fall_time &&
				time_after(jiffies, data->temp_fall_time))) {
			 /* if fails to update slave dvfs table,
			  * never update any more this time,
			  * try to update slave and host dvfs table next time,
			  * because once host dvfs table is updated,
			  * slave dvfs table can not be update here any more.
			  */
			if (!dev_pm_opp_of_add_table_binning_slave(data,
								   temp_now)) {
				data->temp_fall_time = 0;
				if (!dev_pm_opp_of_add_table_binning(
				    data->cluster, data->cpu_dev, NULL, data))
					max_freq = data->temp_max_freq;
				dev_info(data->cpu_dev,
					 "update temp_max_freq %u\n", max_freq);
			}
		}
		mutex_unlock(data->volt_lock);
	}

	return max_freq;
}

static int sprd_cpufreq_cpuhp_online(unsigned int cpu)
{
	int olcpu, cluster_id;
	struct sprd_cpufreq_driver_data *c;

	if (cpu >= nr_cpu_ids || !cpu_possible(cpu)) {
		pr_err("Invalid CPU%d\n", cpu);
		return NOTIFY_DONE;
	}

	cluster_id = topology_physical_package_id(cpu);
	if (!cluster_id)
		return NOTIFY_DONE;

	for_each_online_cpu(olcpu) {
		if (!cpu_possible(olcpu))
			return NOTIFY_DONE;
		cluster_id = topology_physical_package_id(olcpu);
		if (cluster_id)
			return NOTIFY_DONE;
	}

	c = sprd_cpufreq_data(cpu);
	if (c && c->cpufreq_online)
		c->cpufreq_online(cpu);
	else
		pr_debug("get c->cpufreq_online pointer failed!\n");
	return NOTIFY_DONE;
}

static int sprd_cpufreq_cpuhp_offline(unsigned int cpu)
{
	int olcpu, cluster_id;
	struct sprd_cpufreq_driver_data *c;

	if (cpu >= nr_cpu_ids || !cpu_possible(cpu)) {
		pr_err("Invalid CPU%d\n", cpu);
		return NOTIFY_DONE;
	}

	cluster_id = topology_physical_package_id(cpu);
	if (!cluster_id)
		return NOTIFY_DONE;

	for_each_online_cpu(olcpu) {
		if (!cpu_possible(olcpu))
			return NOTIFY_DONE;
		cluster_id = topology_physical_package_id(olcpu);
		if (cluster_id)
			return NOTIFY_DONE;
	}

	c = sprd_cpufreq_data(cpu);
	if (c && c->cpufreq_offline)
		c->cpufreq_offline(cpu);
	else
		pr_debug("get c->cpufreq_offline pointer failed!\n");
	return NOTIFY_DONE;
}

/* Cpufreq hotplug setup */

int sprd_cpufreq_cpuhp_setup(void)
{
	return cpuhp_setup_state(CPUHP_BP_PREPARE_DYN,
				"cpuhotplug/sprd_cpufreq_cpuhp:online",
				sprd_cpufreq_cpuhp_online,
				sprd_cpufreq_cpuhp_offline);
}
EXPORT_SYMBOL_GPL(sprd_cpufreq_cpuhp_setup);
MODULE_LICENSE("GPL v2");
