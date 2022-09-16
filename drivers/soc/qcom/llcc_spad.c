// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/hrtimer.h>
#include <linux/regmap.h>
#include <linux/soc/qcom/llcc-qcom.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include "llcc_spad.h"

#define LLCC_PERFMON_SPAD_NAME		"qcom_llcc_perfmon_spad"
#define MAX_CNTR			16
#define MAX_NUMBER_OF_PORTS_SPAD	3
#define NUM_CHANNELS			16
#define DELIM_CHAR			" "

static bool existing_dump;
struct spad_perfmon_private;

static char *filter_name[] = {"FILTER0", "FILTER1", "NONE"};

/**
 * struct spad_perfmon_counter_map	- llcc spad perfmon counter map info
 * @port_sel:		Port selected for configured counter
 * @event_sel:		Event selected for configured counter
 * @counter_dump:	Cumulative counter dump
 * @filter_type:	Filter selected for configured counter
 * @active_filter:	Filter activation status for configured counter
 */
struct spad_perfmon_counter_map {
	unsigned int port_sel;
	unsigned int event_sel;
	unsigned long long counter_dump[NUM_CHANNELS];
	u8 filter_type;
	bool active_filter;
};

/**
 * struct spad_perfmon_private	- spad perfmon private
 * @llcc_map:		llcc register address space map
 * @llcc_bcast_map:	llcc broadcast register address space map
 * @spad_or_bcast_map:	spad broadcast register address space map
 * @bank_off:		Offset of spad banks
 * @num_banks:		Number of banks supported
 * @configured:		Mapping of configured event counters
 * @configured_cntrs:	Count of configured counters.
 * @port_configd:	Number of perfmon port configuration supported
 * @mutex:		mutex to protect this structure
 * @hrtimer:		hrtimer instance for timer functionality
 * @expires:		timer expire time in nano seconds
 * @active_filter:	Filter activation status
 */
struct spad_perfmon_private {
	struct regmap *llcc_map;
	struct regmap *llcc_bcast_map;
	struct regmap *spad_or_bcast_map;
	u32 *bank_off;
	unsigned int num_banks;
	struct spad_perfmon_counter_map configured[MAX_CNTR];
	unsigned int configured_cntrs;
	unsigned int port_configd;
	struct mutex mutex;
	struct hrtimer hrtimer;
	ktime_t expires;
	bool active_filter;
};

static u32 llcc_spad_offsets[] = {
	0x0,
	0x8000
};

static inline void spad_bcast_write(struct spad_perfmon_private *spad_priv, unsigned int offset,
		u32 val)
{
	regmap_write(spad_priv->spad_or_bcast_map, offset, val);
}

static inline void spad_bcast_read(struct spad_perfmon_private *spad_priv, unsigned int offset,
		u32 *val)
{
	regmap_read(spad_priv->spad_or_bcast_map, offset, val);
}

static void spad_bcast_modify(struct spad_perfmon_private *spad_priv, unsigned int offset,
		u32 val, u32 mask)
{
	u32 readval;

	spad_bcast_read(spad_priv, offset, &readval);
	readval &= ~mask;
	readval |= val & mask;
	spad_bcast_write(spad_priv, offset, readval);
}

static void spad_event_config(struct spad_perfmon_private *spad_priv, unsigned int port_num,
		unsigned int event, unsigned int *cntr_num, bool enable)
{
	u32 val = 0, offset;
	u8 filter_type = DEACTIVATE;
	struct spad_perfmon_counter_map *counter_map;

	counter_map = &spad_priv->configured[*cntr_num];

	if (enable) {
		/* Apply filters on selected counters only */
		if (counter_map->active_filter)
			filter_type = (counter_map->filter_type << 1) | ACTIVATE;

		val = ((port_num << SPAD_PORT_SEL_MASK) & SPAD_PORT_SEL_MASK) |
			((event << SPAD_EVENT_SEL_SHIFT) & SPAD_EVENT_SEL_MASK) |
			((filter_type << SPAD_FILTER_EN_SEL_SHIFT) & SPAD_FILTER_EN_SEL_MASK) |
			CLEAR_ON_ENABLE | CLEAR_ON_DUMP;
	}

	offset = SPAD_LPI_LB_PERFMON_COUNTER_n_CONFIG(*cntr_num);
	spad_bcast_write(spad_priv, offset, val);
}

static void spad_event_filter_config(struct spad_perfmon_private *spad_priv,
		enum spad_filter_type filter, unsigned long long wr_match,
		unsigned long long wr_mask, unsigned long long rd_match,
		unsigned long long rd_mask, bool wr_inv_match,
		bool rd_inv_match, bool enable)
{
	u32 val = 0;
	unsigned int offset;

	offset = SPAD_LPI_LB_PROF_FILTER_0_CFG;

	if (filter == FILTER1)
		offset = SPAD_LPI_LB_PROF_FILTER_1_CFG;

	if (enable) {
		val = (wr_match << WR_COLOR_REGION_MATCH_SHIFT) |
			(wr_mask << WR_COLOR_REGION_MASK_SHIFT) |
			(rd_match << RD_COLOR_REGION_MATCH_SHIFT) |
			(rd_mask << RD_COLOR_REGION_MASK_SHIFT);

		if (wr_inv_match)
			val |= WR_COLOR_REGION_INV_MATCH;
		if (rd_inv_match)
			val |= RD_COLOR_REGION_INV_MATCH;
	}
	spad_bcast_write(spad_priv, offset, val);
}

static int spad_register_event_port(struct spad_perfmon_private *spad_priv)
{
	u32 val = 0;
	unsigned int offset = SPAD_LPI_LB_PERFMON_CONFIGURATION_INFO;

	spad_bcast_read(spad_priv, offset, &val);
	val = (val & SPAD_NUM_PORT_EVENT_MASK) >> SPAD_NUM_PORT_EVENT_SHIFT;
	spad_priv->port_configd = val;
	return val;
}

/* For dumping counters uses workaround of stop and start perfmon functionality as no hardware
 * register support is available on current hardware.
 */
static void spad_counters_start_stop(struct spad_perfmon_private *spad_priv, bool activate)
{
	u32 val = 0, mask_val;
	unsigned int offset;

	if (!spad_priv->configured_cntrs)
		return;

	if (activate) {
		val = SPAD_MANUAL_MODE | SPAD_MONITOR_EN;
		existing_dump = false;
	}

	/* Mask value for the monitor, test */
	mask_val = SPAD_MONITOR_MODE_MASK | SPAD_MONITOR_EN_MASK;
	offset = SPAD_LPI_LB_PERFMON_MODE;
	spad_bcast_modify(spad_priv, offset, val, mask_val);
	mutex_unlock(&spad_priv->mutex);
}

static void perfmon_counter_dump(struct spad_perfmon_private *spad_priv)
{
	struct spad_perfmon_counter_map *counter_map;
	u32 val = 0;
	unsigned int i, j, offset;

	if (!spad_priv->configured_cntrs)
		return;
	/* Deactivating the counters */
	spad_counters_start_stop(spad_priv, DEACTIVATE);

	for (i = 0; i < spad_priv->configured_cntrs; i++) {
		counter_map = &spad_priv->configured[i];
		offset = SPAD_LPI_LB_PERFMON_COUNTER_n_VALUE(i);
		for (j = 0; j < spad_priv->num_banks; j++) {
			regmap_read(spad_priv->llcc_map,
					(spad_priv->bank_off[j] + LLCC_SPAD0_BASE_OFFSET + offset),
					&val);
			counter_map->counter_dump[j] += val;
		}
	}
	if (val)
		existing_dump = true;
}

static ssize_t spad_perfmon_counter_dump_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct spad_perfmon_private *spad_priv = dev_get_drvdata(dev);
	struct spad_perfmon_counter_map *counter_map;
	unsigned int i, j;
	unsigned long long total;
	ssize_t cnt = 0;

	if (spad_priv->configured_cntrs == 0) {
		pr_err("counters not configured\n");
		return cnt;
	}

	if (existing_dump)
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt,
				"Existing dump data\n");

	perfmon_counter_dump(spad_priv);
	for (i = 0; i < spad_priv->configured_cntrs - 1; i++) {
		total = 0;
		counter_map = &spad_priv->configured[i];
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "Port %02d,Event %02d,",
				counter_map->port_sel, counter_map->event_sel);

		for (j = 0; j < spad_priv->num_banks; j++) {
			total += counter_map->counter_dump[j];
			counter_map->counter_dump[j] = 0;
		}

		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "0x%016llx\n", total);
	}

	cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "CYCLE COUNT, ,");
	total = 0;
	counter_map = &spad_priv->configured[i];
	for (j = 0; j < spad_priv->num_banks; j++) {
		total += counter_map->counter_dump[j];
		counter_map->counter_dump[j] = 0;
	}

	cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "0x%016llx\n", total);

	if (spad_priv->expires)
		hrtimer_forward_now(&spad_priv->hrtimer, spad_priv->expires);

	return cnt;
}

static ssize_t spad_perfmon_config_remove_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct spad_perfmon_private *spad_priv = dev_get_drvdata(dev);
	struct spad_perfmon_counter_map *counter_map;
	unsigned int j = 0, k, end_cntrs;
	unsigned long port_sel, event_sel;
	u32 val, offset;
	char *token, *delim = DELIM_CHAR;
	u8 filter_type;
	bool invalid_input = false, act_flag = false;

	mutex_lock(&spad_priv->mutex);

	/* enable/disable command */
	token = strsep((char **)&buf, delim);
	if (token == NULL) {
		pr_err("enable/disable filters, No command input\n");
		goto event_config_free;
	}

	if (!strcmp(token, "ENABLE")) {
		act_flag = true;
		if (spad_priv->configured_cntrs) {
			pr_err("Counters already configured, remove & try\n");
			mutex_unlock(&spad_priv->mutex);
			return -EINVAL;
		}
	} else if (sysfs_streq(token, "DISABLE")) {
		if (!spad_priv->configured_cntrs) {
			pr_err("Counters are not configured\n");
			mutex_unlock(&spad_priv->mutex);
			return -EINVAL;
		}

		/* Clearing all configured counters */
		for (k = 0; k < spad_priv->configured_cntrs - 1; k++) {
			counter_map = &spad_priv->configured[k];
			if (counter_map->active_filter)
				filter_type = counter_map->filter_type;
			else
				filter_type = FILTER_EN_NONE;

			spad_event_config(spad_priv, 0, 0, &k, act_flag);
			pr_info("Removed counter %2d with filter: %s\n", k,
					filter_name[filter_type]);
		}
		spad_event_config(spad_priv, 0, 0, &k, act_flag);
		pr_info("Removed cyclic counter %2d\n", k);
		spad_priv->configured_cntrs = 0;
		goto event_config_free;
	} else {
		pr_err("enable/disable perfmon, invalid command input\n");
		goto event_config_free;
	}

	do {
		/* Port selection and range check */
		token = strsep((char **)&buf, delim);
		if (token == NULL)
			break;
		if (kstrtoul(token, 0, &port_sel) || port_sel >= spad_priv->port_configd) {
			pr_err("Unsupported port num %ld\n", port_sel);
			invalid_input = true;
			break;
		}

		/* Event selection and range check */
		token = strsep((char **)&buf, delim);
		if (token == NULL || (kstrtoul(token, 0, &event_sel)) ||
				event_sel >= SPAD_EVENT_NUM_MAX) {
			pr_err("Unsupported event num %ld\n", event_sel);
			invalid_input = true;
			break;
		}

		end_cntrs = 1;
		if (j == (MAX_CNTR - end_cntrs))
			break;

		counter_map = &spad_priv->configured[j];
		counter_map->port_sel = port_sel;
		counter_map->event_sel = event_sel;

		for (k = 0; k < spad_priv->num_banks; k++)
			counter_map->counter_dump[k] = 0;

		spad_event_config(spad_priv, port_sel, event_sel, &j, act_flag);

		if (counter_map->active_filter)
			filter_type = counter_map->filter_type;
		else
			filter_type = FILTER_EN_NONE;

		pr_info("counter %2d configured for event %2ld from port %ld with filter: %s\n",
				j++, event_sel, port_sel, filter_name[filter_type]);

	} while (token != NULL);

	/* configure clock event */
	val = COUNT_CLOCK_EVENT | CLEAR_ON_ENABLE | CLEAR_ON_DUMP;
	offset = SPAD_LPI_LB_PERFMON_COUNTER_n_CONFIG(j++);
	spad_bcast_write(spad_priv, offset, val);

	if (invalid_input && j) {
		pr_info("removing configuration\n");
		for (k = 0; k < j; k++)
			spad_event_config(spad_priv, port_sel, event_sel, &k, false);
		j = 0;
	}
	spad_priv->configured_cntrs = j;
event_config_free:
	mutex_unlock(&spad_priv->mutex);
	return count;
}

static ssize_t spad_perfmon_start_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct spad_perfmon_private *spad_priv = dev_get_drvdata(dev);
	unsigned long start;
	bool act_flag = false;

	if (!spad_priv) {
		pr_err("Unable to get driver data\n");
		return -EINVAL;
	}

	if (kstrtoul(buf, 0, &start))
		return -EINVAL;

	mutex_lock(&spad_priv->mutex);
	if (start) {
		if (!spad_priv->configured_cntrs) {
			pr_err("start failed. spad perfmon not configured\n");
			mutex_unlock(&spad_priv->mutex);
			return -EINVAL;
		}

		if (spad_priv->expires) {
			if (hrtimer_is_queued(&spad_priv->hrtimer))
				hrtimer_forward_now(&spad_priv->hrtimer, spad_priv->expires);
			else
				hrtimer_start(&spad_priv->hrtimer, spad_priv->expires,
						HRTIMER_MODE_REL_PINNED);
		}

		act_flag = true;
	} else {
		if (spad_priv->expires)
			hrtimer_cancel(&spad_priv->hrtimer);

		if (!spad_priv->configured_cntrs) {
			pr_err("stop failed. spad perfmon not configured\n");
			mutex_unlock(&spad_priv->mutex);
			return -EINVAL;
		}

		act_flag = false;
	}

	spad_counters_start_stop(spad_priv, act_flag);
	mutex_unlock(&spad_priv->mutex);
	return count;
}

static ssize_t spad_perfmon_ns_periodic_dump_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct spad_perfmon_private *spad_priv = dev_get_drvdata(dev);

	if (kstrtos64(buf, 0, &spad_priv->expires))
		return -EINVAL;

	mutex_lock(&spad_priv->mutex);
	if (!spad_priv->expires) {
		hrtimer_cancel(&spad_priv->hrtimer);
		mutex_unlock(&spad_priv->mutex);
		return count;
	}

	if (hrtimer_is_queued(&spad_priv->hrtimer))
		hrtimer_forward_now(&spad_priv->hrtimer, spad_priv->expires);
	else
		hrtimer_start(&spad_priv->hrtimer, spad_priv->expires, HRTIMER_MODE_REL_PINNED);

	mutex_unlock(&spad_priv->mutex);
	return count;
}

static enum spad_filter_type find_filter_type(char *filter)
{
	enum spad_filter_type ret = UNKNOWN;

	if (!strcmp(filter, "FIL0"))
		ret = FILTER0;
	else if (!strcmp(filter, "FIL1"))
		ret = FILTER1;
	return ret;
}

static ssize_t spad_perfmon_filter_config_remove_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct spad_perfmon_private *spad_priv = dev_get_drvdata(dev);
	struct spad_perfmon_counter_map *counter_map;
	unsigned long long wr_mask, wr_match, rd_mask, rd_match;
	char *token, *delim = DELIM_CHAR;
	enum spad_filter_type filter = UNKNOWN;
	bool wr_inv_match, rd_inv_match, act_flag = false, fil_change = false;
	u8 cntr_num = 0, k;

	if (spad_priv->configured_cntrs) {
		pr_err("Events already configured, remove events and try\n");
		return count;
	}

	mutex_lock(&spad_priv->mutex);

	/* enable/disable command */
	token = strsep((char **)&buf, delim);
	if (token == NULL) {
		pr_err("enable/disable filters failed, No command input\n");
		goto filter_config_free;
	}

	if (!strcmp(token, "ENABLE")) {
		act_flag = true;
	} else if (sysfs_streq(token, "DISABLE")) {
		if (!spad_priv->active_filter) {
			pr_err("Filters are not configured\n");
			goto filter_config_free;
		}
		for (k = 0; k < (MAX_CNTR - 1); k++) {
			counter_map = &spad_priv->configured[k];
			if (counter_map->active_filter) {
				counter_map->active_filter = false;
				pr_info("Removed Filter %s from Counter %hu\n",
						filter_name[counter_map->filter_type], k);
				counter_map->filter_type = FILTER_EN_NONE;
			}
		}
		spad_event_filter_config(spad_priv, FILTER0, 0, 0, 0, 0, 0, 0, false);
		spad_event_filter_config(spad_priv, FILTER1, 0, 0, 0, 0, 0, 0, false);
		spad_priv->active_filter = false;
		goto filter_config_free;
	} else {
		pr_err("enable/disable filters failed, invalid command input\n");
		goto filter_config_free;
	}

	token = strsep((char **)&buf, delim);
	if (token != NULL)
		filter = find_filter_type(token);

	if (filter == UNKNOWN) {
		pr_err("Filter configuration failed, Unsupported filter\n");
		goto filter_config_free;
	}

	/* WR color mask match configuration */
	token = strsep((char **)&buf, delim);
	if (token == NULL) {
		pr_err("filter configuration failed, Wrong input\n");
		goto filter_config_free;
	}

	if (kstrtoull(token, 0, &wr_match)) {
		pr_err("filter configuration failed, Wrong format\n");
		goto filter_config_free;
	}

	token = strsep((char **)&buf, delim);
	if (token == NULL) {
		pr_err("filter configuration failed, Wrong input\n");
		goto filter_config_free;
	}

	if (kstrtoull(token, 0, &wr_mask)) {
		pr_err("filter configuration failed, Wrong format\n");
		goto filter_config_free;
	}

	/* RD color mask match configuration */
	token = strsep((char **)&buf, delim);
	if (token == NULL) {
		pr_err("filter configuration failed, Wrong input\n");
		goto filter_config_free;
	}

	if (kstrtoull(token, 0, &rd_match)) {
		pr_err("filter configuration failed, Wrong format\n");
		goto filter_config_free;
	}

	token = strsep((char **)&buf, delim);
	if (token == NULL) {
		pr_err("filter configuration failed, Wrong input\n");
		goto filter_config_free;
	}

	if (kstrtoull(token, 0, &rd_mask)) {
		pr_err("filter configuration failed, Wrong format\n");
		goto filter_config_free;
	}

	/* wr inverted match bit */
	token = strsep((char **)&buf, delim);
	if (token) {
		if (kstrtobool(token, &wr_inv_match)) {
			pr_err("worng format for inverted match bit\n");
			goto filter_config_free;
		}
	}

	/* rd inverted match bit */
	token = strsep((char **)&buf, delim);
	if (token) {
		if (kstrtobool(token, &rd_inv_match)) {
			pr_err("worng format for inverted match bit\n");
			goto filter_config_free;
		}
	}

	token = strsep((char **)&buf, delim);
	if (token == NULL) {
		pr_err("filter configuration failed, no counter num input\n");
		goto filter_config_free;
	}

	while (token != NULL) {
		if (kstrtou8(token, 0, &cntr_num)) {
			pr_err("Wrong counter number format\n");
			goto filter_config_free;
		}

		if (cntr_num >= MAX_CNTR) {
			pr_err("Counter number %hhu out of range\n", cntr_num);
			goto next_iteration;
		}

		if (!spad_priv->configured[cntr_num].active_filter &&
				act_flag) {
			spad_priv->configured[cntr_num].active_filter = true;
			spad_priv->configured[cntr_num].filter_type = filter;
			fil_change = true;
			pr_info("Applied filter on counter %hhu\n", cntr_num);
		} else if (spad_priv->configured[cntr_num].active_filter && act_flag) {
			pr_err("Filter on counter %hhu is already configured\n", cntr_num);
			goto next_iteration;
		} else {
			pr_err("Invalid filter/counter value\n");
			goto filter_config_free;
		}
next_iteration:
		token = strsep((char **)&buf, delim);
	}

	/* In case there is no change/update in filters */
	if (!fil_change) {
		pr_err("No filter applied\n");
		goto filter_config_free;
	}
	spad_priv->active_filter = true;
	spad_event_filter_config(spad_priv, filter, wr_match, wr_mask, rd_match, rd_mask,
			wr_inv_match, rd_inv_match, act_flag);
filter_config_free:
	mutex_unlock(&spad_priv->mutex);
	return count;
}

static DEVICE_ATTR_RO(spad_perfmon_counter_dump);
static DEVICE_ATTR_WO(spad_perfmon_config_remove);
static DEVICE_ATTR_WO(spad_perfmon_start);
static DEVICE_ATTR_WO(spad_perfmon_ns_periodic_dump);
static DEVICE_ATTR_WO(spad_perfmon_filter_config_remove);

static struct attribute *spad_perfmon_attrs[] = {
	&dev_attr_spad_perfmon_counter_dump.attr,
	&dev_attr_spad_perfmon_config_remove.attr,
	&dev_attr_spad_perfmon_start.attr,
	&dev_attr_spad_perfmon_ns_periodic_dump.attr,
	&dev_attr_spad_perfmon_filter_config_remove.attr,
	NULL,
};

static struct attribute_group spad_perfmon_group = {
	.attrs = spad_perfmon_attrs,
};

static enum hrtimer_restart spad_perfmon_timer_handler(struct hrtimer *hrtimer)
{
	struct spad_perfmon_private *spad_priv = container_of(hrtimer, struct spad_perfmon_private,
			hrtimer);

	perfmon_counter_dump(spad_priv);
	spad_counters_start_stop(spad_priv, ACTIVATE);
	hrtimer_forward_now(&spad_priv->hrtimer, spad_priv->expires);
	return HRTIMER_RESTART;
}

static int spad_perfmon_probe(struct platform_device *pdev)
{
	int result = 0;
	struct llcc_drv_data *llcc_driv_data;
	struct spad_perfmon_private *spad_priv;

	llcc_driv_data = dev_get_drvdata(pdev->dev.parent);
	if (!llcc_driv_data)
		return -ENOMEM;

	spad_priv = devm_kzalloc(&pdev->dev, sizeof(*spad_priv), GFP_KERNEL);
	if (!spad_priv)
		return -ENOMEM;

	if (!llcc_driv_data->regmap || !llcc_driv_data->bcast_regmap ||
			!llcc_driv_data->spad_or_bcast_regmap ||
			!llcc_driv_data->spad_and_bcast_regmap) {
		pr_err("mapping error\n");
		return -ENODEV;
	}

	spad_priv->llcc_map = llcc_driv_data->regmap;
	spad_priv->llcc_bcast_map = llcc_driv_data->bcast_regmap;
	spad_priv->spad_or_bcast_map = llcc_driv_data->spad_or_bcast_regmap;

	/* number of banks */
	spad_priv->bank_off = llcc_spad_offsets;
	spad_priv->num_banks = ARRAY_SIZE(llcc_spad_offsets);

	result = sysfs_create_group(&pdev->dev.kobj, &spad_perfmon_group);
	if (result) {
		pr_err("Unable to creare sysfs group\n");
		return result;
	}

	mutex_init(&spad_priv->mutex);
	platform_set_drvdata(pdev, spad_priv);

	/* Reading HW configuration for number of ports */
	if (!spad_register_event_port(spad_priv)) {
		pr_err("Unable to read port configuration info\n");
		return -ENOMEM;
	}
	hrtimer_init(&spad_priv->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	spad_priv->hrtimer.function = spad_perfmon_timer_handler;
	spad_priv->expires = 0;
	spad_priv->active_filter = false;

	pr_info("SPAD Perfmon probed successfully!\n");
	return 0;
}

static int spad_perfmon_remove_data(struct platform_device *pdev)
{
	struct spad_perfmon_private *spad_priv = platform_get_drvdata(pdev);

	while (hrtimer_active(&spad_priv->hrtimer))
		hrtimer_cancel(&spad_priv->hrtimer);

	mutex_destroy(&spad_priv->mutex);
	sysfs_remove_group(&pdev->dev.kobj, &spad_perfmon_group);
	platform_set_drvdata(pdev, NULL);
	pr_info("SPAD Perfmon successfully removed\n");
	return 0;
}

static const struct of_device_id of_match_llcc_perfmon_spad[] = {
	{
		.compatible = "qcom,llcc-perfmon-spad",
	},
	{}
};
MODULE_DEVICE_TABLE(of, of_match_llcc_perfmon_spad);

static struct platform_driver spad_perfmon_driver = {
	.probe = spad_perfmon_probe,
	.remove = spad_perfmon_remove_data,
	.driver = {
		.name = LLCC_PERFMON_SPAD_NAME,
		.of_match_table = of_match_llcc_perfmon_spad,
	}
};

module_platform_driver(spad_perfmon_driver);

MODULE_DESCRIPTION("QCOM LLCC SPAD PMU MONITOR");
MODULE_LICENSE("GPL v2");
