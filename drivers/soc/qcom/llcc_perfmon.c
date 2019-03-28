/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/hrtimer.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/soc/qcom/llcc-qcom.h>
#include <linux/module.h>
#include <linux/clk.h>
#include "llcc_events.h"
#include "llcc_perfmon.h"

#define LLCC_PERFMON_NAME		"llcc_perfmon"
#define LLCC_PERFMON_COUNTER_MAX	16
#define MAX_NUMBER_OF_PORTS		8
#define NUM_CHANNELS			16
#define MAX_STRING_SIZE			20
#define DELIM_CHAR			" "

/**
 * struct llcc_perfmon_counter_map	- llcc perfmon counter map info
 * @port_sel:		Port selected for configured counter
 * @event_sel:		Event selected for configured counter
 * @counter_dump:	Cumulative counter dump
 */
struct llcc_perfmon_counter_map {
	unsigned int port_sel;
	unsigned int event_sel;
	unsigned long long counter_dump;
};

struct llcc_perfmon_private;
/**
 * struct event_port_ops		- event port operation
 * @event_config:		Counter config support for port &  event
 * @event_enable:		Counter enable support for port
 * @event_filter_config:	Port filter config support
 */
struct event_port_ops {
	void (*event_config)(struct llcc_perfmon_private *,
			unsigned int, unsigned int, bool);
	void (*event_enable)(struct llcc_perfmon_private *, bool);
	void (*event_filter_config)(struct llcc_perfmon_private *,
			enum filter_type, unsigned long, bool);
};

/**
 * struct llcc_perfmon_private	- llcc perfmon private
 * @llcc_map:		llcc register address space map
 * @broadcast_off:	Offset of llcc broadcast address space
 * @bank_off:		Offset of llcc banks
 * @num_banks:		Number of banks supported
 * @port_ops:		struct event_port_ops
 * @configured:		Mapping of configured event counters
 * @configured_counters:
 *			Count of configured counters.
 * @enables_port:	Port enabled for perfmon configuration
 * @filtered_ports:	Port filter enabled
 * @port_configd:	Number of perfmon port configuration supported
 * @mutex:		mutex to protect this structure
 * @hrtimer:		hrtimer instance for timer functionality
 * @expires:		timer expire time in nano seconds
 * @clk:		clock node to enable qdss
 * @num_mc:		number of MCS
 * @version:		Version information of llcc block
 */
struct llcc_perfmon_private {
	struct regmap *llcc_map;
	unsigned int broadcast_off;
	unsigned int bank_off[NUM_CHANNELS];
	unsigned int num_banks;
	struct event_port_ops *port_ops[MAX_NUMBER_OF_PORTS];
	struct llcc_perfmon_counter_map configured[LLCC_PERFMON_COUNTER_MAX];
	unsigned int configured_counters;
	unsigned int enables_port;
	unsigned int filtered_ports;
	unsigned int port_configd;
	struct mutex mutex;
	struct hrtimer hrtimer;
	ktime_t expires;
	struct clk *clock;
	unsigned int num_mc;
	unsigned int version;
};

static inline void llcc_bcast_write(struct llcc_perfmon_private *llcc_priv,
			unsigned int offset, uint32_t val)
{
	regmap_write(llcc_priv->llcc_map, llcc_priv->broadcast_off + offset,
			val);
}

static inline void llcc_bcast_read(struct llcc_perfmon_private *llcc_priv,
		unsigned int offset, uint32_t *val)
{
	regmap_read(llcc_priv->llcc_map, llcc_priv->broadcast_off + offset,
			val);
}

static void llcc_bcast_modify(struct llcc_perfmon_private *llcc_priv,
		unsigned int offset, uint32_t val, uint32_t mask)
{
	uint32_t readval;

	llcc_bcast_read(llcc_priv, offset, &readval);
	readval &= ~mask;
	readval |= val & mask;
	llcc_bcast_write(llcc_priv, offset, readval);
}

static void perfmon_counter_dump(struct llcc_perfmon_private *llcc_priv)
{
	uint32_t val;
	unsigned int i, j;
	unsigned long long total;

	if (!llcc_priv->configured_counters)
		return;

	llcc_bcast_write(llcc_priv, PERFMON_DUMP, MONITOR_DUMP);
	for (i = 0; i < llcc_priv->configured_counters; i++) {
		total = 0;
		for (j = 0; j < llcc_priv->num_banks; j++) {
			regmap_read(llcc_priv->llcc_map, llcc_priv->bank_off[j]
					+ LLCC_COUNTER_n_VALUE(i), &val);
			total += val;
		}

		llcc_priv->configured[i].counter_dump += total;
	}
}

static ssize_t perfmon_counter_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct llcc_perfmon_private *llcc_priv = dev_get_drvdata(dev);
	struct llcc_perfmon_counter_map *counter_map;
	uint32_t val;
	unsigned int i, j;
	unsigned long long total;
	ssize_t cnt = 0, print;

	if (llcc_priv->configured_counters == 0) {
		pr_err("counters not configured\n");
		return cnt;
	}

	if (llcc_priv->expires) {
		perfmon_counter_dump(llcc_priv);
		for (i = 0; i < llcc_priv->configured_counters - 1; i++) {
			counter_map = &llcc_priv->configured[i];
			print = snprintf(buf, MAX_STRING_SIZE, "Port %02d,",
					counter_map->port_sel);
			buf += print;
			cnt += print;
			print = snprintf(buf, MAX_STRING_SIZE, "Event %02d,",
					counter_map->event_sel);
			buf += print;
			cnt += print;

			if ((counter_map->port_sel == EVENT_PORT_BEAC) &&
					(llcc_priv->num_mc > 1)) {
				/* DBX uses 2 BEAC ports */
				print = snprintf(buf, MAX_STRING_SIZE,
					"0x%016llx\n",
					counter_map->counter_dump +
					counter_map[1].counter_dump);

				buf += print;
				cnt += print;
				counter_map->counter_dump = 0;
				counter_map[1].counter_dump = 0;
				i++;
			} else {
				print = snprintf(buf, MAX_STRING_SIZE,
					"0x%016llx\n",
					counter_map->counter_dump);
				buf += print;
				cnt += print;
				counter_map->counter_dump = 0;
			}
		}

		print = snprintf(buf, MAX_STRING_SIZE, "CYCLE COUNT, ,");
		buf += print;
		cnt += print;
		counter_map = &llcc_priv->configured[i];
		print = snprintf(buf, MAX_STRING_SIZE, "0x%016llx\n",
			counter_map->counter_dump);
		buf += print;
		cnt += print;
		counter_map->counter_dump = 0;
		hrtimer_forward_now(&llcc_priv->hrtimer, llcc_priv->expires);
	} else {
		llcc_bcast_write(llcc_priv, PERFMON_DUMP, MONITOR_DUMP);

		for (i = 0; i < llcc_priv->configured_counters - 1; i++) {
			print = snprintf(buf, MAX_STRING_SIZE, "Port %02d,",
					llcc_priv->configured[i].port_sel);
			buf += print;
			cnt += print;
			print = snprintf(buf, MAX_STRING_SIZE, "Event %02d,",
					llcc_priv->configured[i].event_sel);
			buf += print;
			cnt += print;
			total = 0;
			for (j = 0; j < llcc_priv->num_banks; j++) {
				regmap_read(llcc_priv->llcc_map,
						llcc_priv->bank_off[j]
						+ LLCC_COUNTER_n_VALUE(i),
						&val);
				print = snprintf(buf, MAX_STRING_SIZE,
						"0x%08x,", val);
				buf += print;
				cnt += print;
				total += val;
			}

			print = snprintf(buf, MAX_STRING_SIZE, "0x%09llx\n",
					total);
			buf += print;
			cnt += print;
		}

		print = snprintf(buf, MAX_STRING_SIZE, "CYCLE COUNT, ,");
		buf += print;
		cnt += print;
		total = 0;
		for (j = 0; j < llcc_priv->num_banks; j++) {
			regmap_read(llcc_priv->llcc_map,
					llcc_priv->bank_off[j] +
					LLCC_COUNTER_n_VALUE(i), &val);
			print = snprintf(buf, MAX_STRING_SIZE, "0x%08x,", val);
			buf += print;
			cnt += print;
			total += val;
		}

		print = snprintf(buf, MAX_STRING_SIZE, "0x%09llx\n", total);
		buf += print;
		cnt += print;
	}

	return cnt;
}

static ssize_t perfmon_configure_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct llcc_perfmon_private *llcc_priv = dev_get_drvdata(dev);
	struct event_port_ops *port_ops;
	unsigned int j = 0;
	unsigned long port_sel, event_sel;
	uint32_t val;
	char *token, *delim = DELIM_CHAR;

	mutex_lock(&llcc_priv->mutex);
	if (llcc_priv->configured_counters) {
		pr_err("Counters configured already, remove & try again\n");
		mutex_unlock(&llcc_priv->mutex);
		return -EINVAL;
	}

	llcc_priv->configured_counters = 0;
	token = strsep((char **)&buf, delim);

	while (token != NULL) {
		if (kstrtoul(token, 0, &port_sel))
			break;

		if (port_sel >= llcc_priv->port_configd)
			break;

		token = strsep((char **)&buf, delim);
		if (token == NULL)
			break;

		if (kstrtoul(token, 0, &event_sel))
			break;

		token = strsep((char **)&buf, delim);
		if (event_sel >= EVENT_NUM_MAX) {
			pr_err("unsupported event num %ld\n", event_sel);
			continue;
		}

		if ((port_sel == EVENT_PORT_BEAC) &&
				(llcc_priv->num_mc == 2) &&
				(llcc_priv->configured_counters >=
				(LLCC_PERFMON_COUNTER_MAX - 2)))
			break;

		llcc_priv->configured[j].port_sel = port_sel;
		llcc_priv->configured[j].event_sel = event_sel;
		port_ops = llcc_priv->port_ops[port_sel];
		pr_info("counter %2d configured for event %2ld from port %2ld\n"
				, j, event_sel, port_sel);
		port_ops->event_config(llcc_priv, event_sel, j++, true);
		if ((port_sel == EVENT_PORT_BEAC) && (llcc_priv->num_mc == 2)) {
			llcc_priv->configured[j].port_sel = EVENT_PORT_BEAC1;
			llcc_priv->configured[j].event_sel = event_sel;
			j++;
			llcc_priv->configured_counters++;
		}

		if (!(llcc_priv->enables_port & (1 << port_sel)))
			if (port_ops->event_enable)
				port_ops->event_enable(llcc_priv, true);

		llcc_priv->enables_port |= (1 << port_sel);

		/* Last perfmon counter for cycle counter */
		if (llcc_priv->configured_counters++ ==
				(LLCC_PERFMON_COUNTER_MAX - 2))
			break;
	}

	/* configure clock event */
	val = COUNT_CLOCK_EVENT | CLEAR_ON_ENABLE | CLEAR_ON_DUMP;
	llcc_bcast_write(llcc_priv, PERFMON_COUNTER_n_CONFIG(j), val);

	llcc_priv->configured_counters++;
	mutex_unlock(&llcc_priv->mutex);
	return count;
}

static ssize_t perfmon_remove_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct llcc_perfmon_private *llcc_priv = dev_get_drvdata(dev);
	struct event_port_ops *port_ops;
	unsigned int j = 0, counter_remove = 0;
	unsigned long port_sel, event_sel;
	char *token, *delim = DELIM_CHAR;

	mutex_lock(&llcc_priv->mutex);
	if (!llcc_priv->configured_counters) {
		pr_err("Counters not configured\n");
		mutex_unlock(&llcc_priv->mutex);
		return -EINVAL;
	}

	token = strsep((char **)&buf, delim);

	while (token != NULL) {
		if (kstrtoul(token, 0, &port_sel))
			break;

		if (port_sel >= llcc_priv->port_configd)
			break;

		token = strsep((char **)&buf, delim);
		if (token == NULL)
			break;

		if (kstrtoul(token, 0, &event_sel))
			break;

		token = strsep((char **)&buf, delim);
		if (event_sel >= EVENT_NUM_MAX) {
			pr_err("unsupported event num %ld\n", event_sel);
			continue;
		}

		if ((port_sel == EVENT_PORT_BEAC) &&
				(llcc_priv->num_mc == 2) &&
				(counter_remove >=
				(LLCC_PERFMON_COUNTER_MAX - 2)))
			break;

		/* put dummy values */
		llcc_priv->configured[j].port_sel = MAX_NUMBER_OF_PORTS;
		llcc_priv->configured[j].event_sel = 100;
		port_ops = llcc_priv->port_ops[port_sel];
		pr_info("removed counter %2d for event %2ld from port %2ld\n",
				j, event_sel, port_sel);

		port_ops->event_config(llcc_priv, event_sel, j++, false);
		if ((port_sel == EVENT_PORT_BEAC) && (llcc_priv->num_mc == 2)) {
			j++;
			counter_remove++;
		}

		if (llcc_priv->enables_port & (1 << port_sel))
			if (port_ops->event_enable)
				port_ops->event_enable(llcc_priv, false);

		llcc_priv->enables_port &= ~(1 << port_sel);

		/* Last perfmon counter for cycle counter */
		if (counter_remove++ == (LLCC_PERFMON_COUNTER_MAX - 2))
			break;
	}

	/* remove clock event */
	llcc_bcast_write(llcc_priv, PERFMON_COUNTER_n_CONFIG(j), 0);

	llcc_priv->configured_counters = 0;
	mutex_unlock(&llcc_priv->mutex);
	return count;
}

static enum filter_type find_filter_type(char *filter)
{
	enum filter_type ret = UNKNOWN;

	if (!strcmp(filter, "SCID"))
		ret = SCID;
	else if (!strcmp(filter, "MID"))
		ret = MID;
	else if (!strcmp(filter, "PROFILING_TAG"))
		ret = PROFILING_TAG;
	else if (!strcmp(filter, "WAY_ID"))
		ret = WAY_ID;

	return ret;
}

static ssize_t perfmon_filter_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct llcc_perfmon_private *llcc_priv = dev_get_drvdata(dev);
	unsigned long port, val;
	struct event_port_ops *port_ops;
	char *token, *delim = DELIM_CHAR;
	enum filter_type filter = UNKNOWN;

	if (llcc_priv->configured_counters) {
		pr_err("remove configured events and try\n");
		return count;
	}

	mutex_lock(&llcc_priv->mutex);

	token = strsep((char **)&buf, delim);
	if (token != NULL)
		filter = find_filter_type(token);

	if (filter == UNKNOWN) {
		pr_err("filter configuration failed, Unsupported filter\n");
		goto filter_config_free;
	}

	token = strsep((char **)&buf, delim);
	if (token == NULL) {
		pr_err("filter configuration failed, Wrong input\n");
		goto filter_config_free;
	}

	if (kstrtoul(token, 0, &val)) {
		pr_err("filter configuration failed, Wrong input\n");
		goto filter_config_free;
	}

	if ((filter == SCID) && (val >= SCID_MAX)) {
		pr_err("filter configuration failed, SCID above MAX value\n");
		goto filter_config_free;
	}


	while (token != NULL) {
		token = strsep((char **)&buf, delim);
		if (token == NULL)
			break;

		if (kstrtoul(token, 0, &port))
			break;

		llcc_priv->filtered_ports |= 1 << port;
		port_ops = llcc_priv->port_ops[port];
		if (port_ops->event_filter_config)
			port_ops->event_filter_config(llcc_priv, filter, val,
					true);
	}

	mutex_unlock(&llcc_priv->mutex);
	return count;

filter_config_free:
	mutex_unlock(&llcc_priv->mutex);
	return -EINVAL;
}

static ssize_t perfmon_filter_remove_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct llcc_perfmon_private *llcc_priv = dev_get_drvdata(dev);
	struct event_port_ops *port_ops;
	unsigned long port, val;
	char *token, *delim = DELIM_CHAR;
	enum filter_type filter = UNKNOWN;

	mutex_lock(&llcc_priv->mutex);
	token = strsep((char **)&buf, delim);
	if (token != NULL)
		filter = find_filter_type(token);

	if (filter == UNKNOWN) {
		pr_err("filter configuration failed, Unsupported filter\n");
		goto filter_remove_free;
	}

	token = strsep((char **)&buf, delim);
	if (token == NULL) {
		pr_err("filter configuration failed, Wrong input\n");
		goto filter_remove_free;
	}

	if (kstrtoul(token, 0, &val)) {
		pr_err("filter configuration failed, Wrong input\n");
		goto filter_remove_free;
	}

	if ((filter == SCID) && (val >= SCID_MAX)) {
		pr_err("filter configuration failed, SCID above MAX value\n");
		goto filter_remove_free;
	}

	while (token != NULL) {
		token = strsep((char **)&buf, delim);
		if (token == NULL)
			break;

		if (kstrtoul(token, 0, &port))
			break;

		llcc_priv->filtered_ports &= ~(1 << port);
		port_ops = llcc_priv->port_ops[port];
		if (port_ops->event_filter_config)
			port_ops->event_filter_config(llcc_priv, filter, val,
					false);
	}

	mutex_unlock(&llcc_priv->mutex);
	return count;

filter_remove_free:
	mutex_unlock(&llcc_priv->mutex);
	return count;
}

static ssize_t perfmon_start_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct llcc_perfmon_private *llcc_priv = dev_get_drvdata(dev);
	uint32_t val = 0, mask;
	unsigned long start;
	int ret;

	if (kstrtoul(buf, 0, &start))
		return -EINVAL;

	mutex_lock(&llcc_priv->mutex);
	if (start) {
		if (!llcc_priv->configured_counters) {
			pr_err("start failed. perfmon not configured\n");
			mutex_unlock(&llcc_priv->mutex);
			return -EINVAL;
		}

		ret = clk_prepare_enable(llcc_priv->clock);
		if (ret) {
			mutex_unlock(&llcc_priv->mutex);
			return -EINVAL;
		}

		val = MANUAL_MODE | MONITOR_EN;
		if (llcc_priv->expires) {
			if (hrtimer_is_queued(&llcc_priv->hrtimer))
				hrtimer_forward_now(&llcc_priv->hrtimer,
						llcc_priv->expires);
			else
				hrtimer_start(&llcc_priv->hrtimer,
						llcc_priv->expires,
						HRTIMER_MODE_REL_PINNED);
		}

	} else {
		if (llcc_priv->expires)
			hrtimer_cancel(&llcc_priv->hrtimer);

		if (!llcc_priv->configured_counters)
			pr_err("stop failed. perfmon not configured\n");
	}

	mask = PERFMON_MODE_MONITOR_MODE_MASK | PERFMON_MODE_MONITOR_EN_MASK;
	llcc_bcast_modify(llcc_priv, PERFMON_MODE, val, mask);

	if (!start)
		clk_disable_unprepare(llcc_priv->clock);

	mutex_unlock(&llcc_priv->mutex);
	return count;
}

static ssize_t perfmon_ns_periodic_dump_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct llcc_perfmon_private *llcc_priv = dev_get_drvdata(dev);

	if (kstrtos64(buf, 10, &llcc_priv->expires))
		return -EINVAL;

	mutex_lock(&llcc_priv->mutex);
	if (!llcc_priv->expires) {
		hrtimer_cancel(&llcc_priv->hrtimer);
		mutex_unlock(&llcc_priv->mutex);
		return count;
	}

	if (hrtimer_is_queued(&llcc_priv->hrtimer))
		hrtimer_forward_now(&llcc_priv->hrtimer, llcc_priv->expires);
	else
		hrtimer_start(&llcc_priv->hrtimer, llcc_priv->expires,
			      HRTIMER_MODE_REL_PINNED);

	mutex_unlock(&llcc_priv->mutex);
	return count;
}

static ssize_t perfmon_scid_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct llcc_perfmon_private *llcc_priv = dev_get_drvdata(dev);
	uint32_t val;
	unsigned int i, j, offset;
	ssize_t cnt = 0, print;
	unsigned long total;

	for (i = 0; i < SCID_MAX; i++) {
		total = 0;
		offset = TRP_SCID_n_STATUS(i);

		for (j = 0; j < llcc_priv->num_banks; j++) {
			regmap_read(llcc_priv->llcc_map,
					llcc_priv->bank_off[j] + offset, &val);
			val = (val & TRP_SCID_STATUS_CURRENT_CAP_MASK)
				>> TRP_SCID_STATUS_CURRENT_CAP_SHIFT;
			total += val;
		}

		llcc_bcast_read(llcc_priv, offset, &val);
		if (val & TRP_SCID_STATUS_ACTIVE_MASK)
			print = snprintf(buf, MAX_STRING_SIZE, "SCID %02d %10s",
					i, "ACTIVE");
		else
			print = snprintf(buf, MAX_STRING_SIZE, "SCID %02d %10s",
					i, "DEACTIVE");

		buf += print;
		cnt += print;
		print = snprintf(buf, MAX_STRING_SIZE, ",0x%08lx\n", total);
		buf += print;
		cnt += print;
	}

	return cnt;
}

static DEVICE_ATTR_RO(perfmon_counter_dump);
static DEVICE_ATTR_WO(perfmon_configure);
static DEVICE_ATTR_WO(perfmon_remove);
static DEVICE_ATTR_WO(perfmon_filter_config);
static DEVICE_ATTR_WO(perfmon_filter_remove);
static DEVICE_ATTR_WO(perfmon_start);
static DEVICE_ATTR_RO(perfmon_scid_status);
static DEVICE_ATTR_WO(perfmon_ns_periodic_dump);

static struct attribute *llcc_perfmon_attrs[] = {
	&dev_attr_perfmon_counter_dump.attr,
	&dev_attr_perfmon_configure.attr,
	&dev_attr_perfmon_remove.attr,
	&dev_attr_perfmon_filter_config.attr,
	&dev_attr_perfmon_filter_remove.attr,
	&dev_attr_perfmon_start.attr,
	&dev_attr_perfmon_scid_status.attr,
	&dev_attr_perfmon_ns_periodic_dump.attr,
	NULL,
};

static struct attribute_group llcc_perfmon_group = {
	.attrs	= llcc_perfmon_attrs,
};

static void perfmon_counter_config(struct llcc_perfmon_private *llcc_priv,
		unsigned int port, unsigned int event_counter_num, bool enable)
{
	uint32_t val = 0;

	if (enable)
		val = (port & PERFMON_PORT_SELECT_MASK) |
			((event_counter_num << EVENT_SELECT_SHIFT) &
			PERFMON_EVENT_SELECT_MASK) | CLEAR_ON_ENABLE |
			CLEAR_ON_DUMP;

	llcc_bcast_write(llcc_priv, PERFMON_COUNTER_n_CONFIG(event_counter_num),
			val);
}

static void feac_event_config(struct llcc_perfmon_private *llcc_priv,
		unsigned int event_type, unsigned int counter_num,
		bool enable)
{
	uint32_t val = 0, mask;

	mask = EVENT_SEL_MASK;
	if (llcc_priv->filtered_ports & (1 << EVENT_PORT_FEAC))
		mask |= FILTER_SEL_MASK | FILTER_EN_MASK;

	if (enable) {
		val = (event_type << EVENT_SEL_SHIFT) & EVENT_SEL_MASK;
		if (llcc_priv->filtered_ports & (1 << EVENT_PORT_FEAC))
			val |= (FILTER_0 << FILTER_SEL_SHIFT) | FILTER_EN;

	}

	llcc_bcast_modify(llcc_priv, FEAC_PROF_EVENT_n_CFG(counter_num),
			val, mask);
	perfmon_counter_config(llcc_priv, EVENT_PORT_FEAC, counter_num, enable);
}

static void feac_event_enable(struct llcc_perfmon_private *llcc_priv,
		bool enable)
{
	uint32_t val = 0, mask;

	if (enable) {
		val = (BYTE_SCALING << BYTE_SCALING_SHIFT) |
			(BEAT_SCALING << BEAT_SCALING_SHIFT) | PROF_EN;

		if (llcc_priv->filtered_ports & (1 << EVENT_PORT_FEAC)) {
			if (llcc_priv->version == REV_0)
				val |= (FILTER_0 <<
					FEAC_SCALING_FILTER_SEL_SHIFT) |
					FEAC_SCALING_FILTER_EN;
			else
				val |= (FILTER_0 <<
					FEAC_WR_BEAT_FILTER_SEL_SHIFT) |
					FEAC_WR_BEAT_FILTER_EN |
					(FILTER_0 <<
					FEAC_WR_BYTE_FILTER_SEL_SHIFT) |
					FEAC_WR_BYTE_FILTER_EN |
					(FILTER_0 <<
					FEAC_RD_BEAT_FILTER_SEL_SHIFT) |
					FEAC_RD_BEAT_FILTER_EN |
					(FILTER_0 <<
					FEAC_RD_BYTE_FILTER_SEL_SHIFT) |
					FEAC_RD_BYTE_FILTER_EN;
		}
	}

	mask = PROF_CFG_BEAT_SCALING_MASK | PROF_CFG_BYTE_SCALING_MASK
		| PROF_CFG_EN_MASK;

	if (llcc_priv->filtered_ports & (1 << EVENT_PORT_FEAC)) {
		if (llcc_priv->version == REV_0)
			mask |= FEAC_SCALING_FILTER_SEL_MASK |
				FEAC_SCALING_FILTER_EN_MASK;
		else
			mask |= FEAC_WR_BEAT_FILTER_SEL_MASK |
				FEAC_WR_BEAT_FILTER_EN_MASK |
				FEAC_WR_BYTE_FILTER_SEL_MASK |
				FEAC_WR_BYTE_FILTER_EN_MASK |
				FEAC_RD_BEAT_FILTER_SEL_MASK |
				FEAC_RD_BEAT_FILTER_EN_MASK |
				FEAC_RD_BYTE_FILTER_SEL_MASK |
				FEAC_RD_BYTE_FILTER_EN_MASK;
	}

	llcc_bcast_modify(llcc_priv, FEAC_PROF_CFG, val, mask);
}

static void feac_event_filter_config(struct llcc_perfmon_private *llcc_priv,
		enum filter_type filter, unsigned long match, bool enable)
{
	uint32_t val = 0, mask;

	if (filter == SCID) {
		if (llcc_priv->version == REV_0) {
			if (enable)
				val = (match << SCID_MATCH_SHIFT) |
					SCID_MASK_MASK;

			mask = SCID_MATCH_MASK | SCID_MASK_MASK;
		} else {
			if (enable)
				val = BIT(match);

			mask = SCID_MULTI_MATCH_MASK;
		}

		llcc_bcast_modify(llcc_priv, FEAC_PROF_FILTER_0_CFG6, val,
				mask);
	} else if (filter == MID) {
		if (enable)
			val = (match << MID_MATCH_SHIFT) | MID_MASK_MASK;

		mask = MID_MATCH_MASK | MID_MASK_MASK;
		llcc_bcast_modify(llcc_priv, FEAC_PROF_FILTER_0_CFG5, val,
				mask);
	} else {
		pr_err("unknown filter/not supported\n");
	}
}

static struct event_port_ops feac_port_ops = {
	.event_config	= feac_event_config,
	.event_enable	= feac_event_enable,
	.event_filter_config	= feac_event_filter_config,
};

static void ferc_event_config(struct llcc_perfmon_private *llcc_priv,
		unsigned int event_type, unsigned int counter_num,
		bool enable)
{
	uint32_t val = 0, mask;

	mask = EVENT_SEL_MASK;
	if (llcc_priv->filtered_ports & (1 << EVENT_PORT_FERC))
		mask |= FILTER_SEL_MASK | FILTER_EN_MASK;

	if (enable) {
		val = event_type << EVENT_SEL_SHIFT;
		if (llcc_priv->filtered_ports & (1 << EVENT_PORT_FERC))
			val |= (FILTER_0 << FILTER_SEL_SHIFT) | FILTER_EN;

	}

	llcc_bcast_modify(llcc_priv, FERC_PROF_EVENT_n_CFG(counter_num),
			val, mask);
	perfmon_counter_config(llcc_priv, EVENT_PORT_FERC, counter_num, enable);
}

static void ferc_event_enable(struct llcc_perfmon_private *llcc_priv,
		bool enable)
{
	uint32_t val = 0, mask;

	if (enable)
		val = (BYTE_SCALING << BYTE_SCALING_SHIFT) |
			(BEAT_SCALING << BEAT_SCALING_SHIFT) | PROF_EN;

	mask = PROF_CFG_BEAT_SCALING_MASK | PROF_CFG_BYTE_SCALING_MASK
		| PROF_CFG_EN_MASK;
	llcc_bcast_modify(llcc_priv, FERC_PROF_CFG, val, mask);
}

static void ferc_event_filter_config(struct llcc_perfmon_private *llcc_priv,
		enum filter_type filter, unsigned long match, bool enable)
{
	uint32_t val = 0, mask;

	if (filter != PROFILING_TAG) {
		pr_err("unknown filter/not supported\n");
		return;
	}

	if (enable)
		val = (match << PROFTAG_MATCH_SHIFT) |
		       FILTER_0_MASK << PROFTAG_MASK_SHIFT;

	mask = PROFTAG_MATCH_MASK | PROFTAG_MASK_MASK;
	llcc_bcast_modify(llcc_priv, FERC_PROF_FILTER_0_CFG0, val, mask);
}

static struct event_port_ops ferc_port_ops = {
	.event_config	= ferc_event_config,
	.event_enable	= ferc_event_enable,
	.event_filter_config	= ferc_event_filter_config,
};

static void fewc_event_config(struct llcc_perfmon_private *llcc_priv,
		unsigned int event_type, unsigned int counter_num,
		bool enable)
{
	uint32_t val = 0, mask;

	mask = EVENT_SEL_MASK;
	if (llcc_priv->filtered_ports & (1 << EVENT_PORT_FEWC))
		mask |= FILTER_SEL_MASK | FILTER_EN_MASK;

	if (enable) {
		val = (event_type << EVENT_SEL_SHIFT) & EVENT_SEL_MASK;
		if (llcc_priv->filtered_ports & (1 << EVENT_PORT_FEWC))
			val |= (FILTER_0 << FILTER_SEL_SHIFT) | FILTER_EN;

	}

	llcc_bcast_modify(llcc_priv, FEWC_PROF_EVENT_n_CFG(counter_num),
			val, mask);
	perfmon_counter_config(llcc_priv, EVENT_PORT_FEWC, counter_num, enable);
}

static void fewc_event_filter_config(struct llcc_perfmon_private *llcc_priv,
		enum filter_type filter, unsigned long match, bool enable)
{
	uint32_t val = 0, mask;

	if (filter != PROFILING_TAG) {
		pr_err("unknown filter/not supported\n");
		return;
	}

	if (enable)
		val = (match << PROFTAG_MATCH_SHIFT) |
		       FILTER_0_MASK << PROFTAG_MASK_SHIFT;

	mask = PROFTAG_MATCH_MASK | PROFTAG_MASK_MASK;
	llcc_bcast_modify(llcc_priv, FEWC_PROF_FILTER_0_CFG0, val, mask);
}

static struct event_port_ops fewc_port_ops = {
	.event_config	= fewc_event_config,
	.event_filter_config	= fewc_event_filter_config,
};

static void beac_event_config(struct llcc_perfmon_private *llcc_priv,
		unsigned int event_type, unsigned int counter_num,
		bool enable)
{
	uint32_t val = 0, mask_val;
	uint32_t valcfg = 0, mask_valcfg = 0;
	unsigned int mc_cnt, offset;

	mask_val = EVENT_SEL_MASK;
	if (llcc_priv->filtered_ports & (1 << EVENT_PORT_BEAC)) {
		mask_val |= FILTER_SEL_MASK | FILTER_EN_MASK;
		if (llcc_priv->version == REV_1)
			mask_valcfg = BEAC_WR_BEAT_FILTER_SEL_MASK |
				BEAC_WR_BEAT_FILTER_EN_MASK |
				BEAC_RD_BEAT_FILTER_SEL_MASK |
				BEAC_RD_BEAT_FILTER_EN_MASK;
	}

	if (enable) {
		val = (event_type << EVENT_SEL_SHIFT) & EVENT_SEL_MASK;
		if (llcc_priv->filtered_ports & (1 << EVENT_PORT_BEAC)) {
			val |= (FILTER_0 << FILTER_SEL_SHIFT) |
					FILTER_EN;
			if (llcc_priv->version == REV_1)
				valcfg = (FILTER_0 <<
					BEAC_WR_BEAT_FILTER_SEL_SHIFT) |
					BEAC_WR_BEAT_FILTER_EN |
					(FILTER_0 <<
					BEAC_RD_BEAT_FILTER_SEL_SHIFT) |
					BEAC_RD_BEAT_FILTER_EN;
		}

	}

	for (mc_cnt = 0; mc_cnt < llcc_priv->num_mc; mc_cnt++) {
		offset = BEAC_PROF_EVENT_n_CFG(counter_num + mc_cnt) +
			mc_cnt * BEAC_INST_OFF;
		llcc_bcast_modify(llcc_priv, offset, val, mask_val);

		offset = BEAC_PROF_CFG + mc_cnt * BEAC_INST_OFF;
		llcc_bcast_modify(llcc_priv, offset, valcfg, mask_valcfg);
	}

	perfmon_counter_config(llcc_priv, EVENT_PORT_BEAC, counter_num, enable);
	perfmon_counter_config(llcc_priv, EVENT_PORT_BEAC1, counter_num + 1,
			enable);
}

static void beac_event_enable(struct llcc_perfmon_private *llcc_priv,
		bool enable)
{
	uint32_t val = 0, mask;
	unsigned int mc_cnt, offset;

	if (enable)
		val = (BYTE_SCALING << BYTE_SCALING_SHIFT) |
			(BEAT_SCALING << BEAT_SCALING_SHIFT) | PROF_EN;

	mask = PROF_CFG_BEAT_SCALING_MASK | PROF_CFG_BYTE_SCALING_MASK
		| PROF_CFG_EN_MASK;

	for (mc_cnt = 0; mc_cnt < llcc_priv->num_mc; mc_cnt++) {
		offset = BEAC_PROF_CFG + mc_cnt * BEAC_INST_OFF;
		llcc_bcast_modify(llcc_priv, offset, val, mask);
	}
}

static void beac_event_filter_config(struct llcc_perfmon_private *llcc_priv,
		enum filter_type filter, unsigned long match, bool enable)
{
	uint32_t val = 0, mask;
	unsigned int mc_cnt, offset;

	if (filter != PROFILING_TAG) {
		pr_err("unknown filter/not supported\n");
		return;
	}

	if (enable)
		val = (match << BEAC_PROFTAG_MATCH_SHIFT)
			| FILTER_0_MASK << BEAC_PROFTAG_MASK_SHIFT;

	mask = BEAC_PROFTAG_MASK_MASK | BEAC_PROFTAG_MATCH_MASK;
	for (mc_cnt = 0; mc_cnt < llcc_priv->num_mc; mc_cnt++) {
		offset = BEAC_PROF_FILTER_0_CFG5 + mc_cnt * BEAC_INST_OFF;
		llcc_bcast_modify(llcc_priv, offset, val, mask);
	}

	if (enable)
		val = match << BEAC_MC_PROFTAG_SHIFT;

	mask = BEAC_MC_PROFTAG_MASK;
	for (mc_cnt = 0; mc_cnt < llcc_priv->num_mc; mc_cnt++) {
		offset = BEAC_PROF_CFG + mc_cnt * BEAC_INST_OFF;
		llcc_bcast_modify(llcc_priv, offset, val, mask);
	}
}

static struct event_port_ops beac_port_ops = {
	.event_config	= beac_event_config,
	.event_enable	= beac_event_enable,
	.event_filter_config	= beac_event_filter_config,
};

static void berc_event_config(struct llcc_perfmon_private *llcc_priv,
		unsigned int event_type, unsigned int counter_num,
		bool enable)
{
	uint32_t val = 0, mask;

	mask = EVENT_SEL_MASK;
	if (llcc_priv->filtered_ports & (1 << EVENT_PORT_BERC))
		mask |= FILTER_SEL_MASK | FILTER_EN_MASK;

	if (enable) {
		val = (event_type << EVENT_SEL_SHIFT) & EVENT_SEL_MASK;
		if (llcc_priv->filtered_ports & (1 << EVENT_PORT_BERC))
			val |= (FILTER_0 << FILTER_SEL_SHIFT) | FILTER_EN;

	}

	llcc_bcast_modify(llcc_priv, BERC_PROF_EVENT_n_CFG(counter_num),
			val, mask);
	perfmon_counter_config(llcc_priv, EVENT_PORT_BERC, counter_num, enable);
}

static void berc_event_enable(struct llcc_perfmon_private *llcc_priv,
		bool enable)
{
	uint32_t val = 0, mask;

	if (enable)
		val = (BYTE_SCALING << BYTE_SCALING_SHIFT) |
			(BEAT_SCALING << BEAT_SCALING_SHIFT) | PROF_EN;

	mask = PROF_CFG_BEAT_SCALING_MASK | PROF_CFG_BYTE_SCALING_MASK
		| PROF_CFG_EN_MASK;
	llcc_bcast_modify(llcc_priv, BERC_PROF_CFG, val, mask);
}

static void berc_event_filter_config(struct llcc_perfmon_private *llcc_priv,
		enum filter_type filter, unsigned long match, bool enable)
{
	uint32_t val = 0, mask;

	if (filter != PROFILING_TAG) {
		pr_err("unknown filter/not supported\n");
		return;
	}

	if (enable)
		val = (match << PROFTAG_MATCH_SHIFT) |
		       FILTER_0_MASK << PROFTAG_MASK_SHIFT;

	mask = PROFTAG_MATCH_MASK | PROFTAG_MASK_MASK;
	llcc_bcast_modify(llcc_priv, BERC_PROF_FILTER_0_CFG0, val, mask);
}

static struct event_port_ops berc_port_ops = {
	.event_config	= berc_event_config,
	.event_enable	= berc_event_enable,
	.event_filter_config	= berc_event_filter_config,
};

static void trp_event_config(struct llcc_perfmon_private *llcc_priv,
		unsigned int event_type, unsigned int counter_num,
		bool enable)
{
	uint32_t val = 0, mask;

	mask = EVENT_SEL_MASK;
	if (llcc_priv->filtered_ports & (1 << EVENT_PORT_TRP))
		mask |= FILTER_SEL_MASK | FILTER_EN_MASK;

	if (enable) {
		val = (event_type << EVENT_SEL_SHIFT) & EVENT_SEL_MASK;
		if (llcc_priv->filtered_ports & (1 << EVENT_PORT_TRP))
			val |= (FILTER_0 << FILTER_SEL_SHIFT) | FILTER_EN;

	}

	llcc_bcast_modify(llcc_priv, TRP_PROF_EVENT_n_CFG(counter_num),
			val, mask);
	perfmon_counter_config(llcc_priv, EVENT_PORT_TRP, counter_num, enable);
}

static void trp_event_filter_config(struct llcc_perfmon_private *llcc_priv,
		enum filter_type filter, unsigned long match, bool enable)
{
	uint32_t val = 0, mask;

	if (filter == SCID) {
		if (enable)
			val = (match << TRP_SCID_MATCH_SHIFT)
				| TRP_SCID_MASK_MASK;

		mask = TRP_SCID_MATCH_MASK | TRP_SCID_MASK_MASK;
	} else if (filter == WAY_ID) {
		if (enable)
			val = (match << TRP_WAY_ID_MATCH_SHIFT)
				| TRP_WAY_ID_MASK_MASK;

		mask = TRP_WAY_ID_MATCH_MASK | TRP_WAY_ID_MASK_MASK;
	} else if (filter == PROFILING_TAG) {
		if (enable)
			val = (match << TRP_PROFTAG_MATCH_SHIFT)
				| FILTER_0_MASK << TRP_PROFTAG_MASK_SHIFT;

		mask = TRP_PROFTAG_MATCH_MASK | TRP_PROFTAG_MASK_MASK;
	} else {
		pr_err("unknown filter/not supported\n");
		return;
	}

	llcc_bcast_modify(llcc_priv, TRP_PROF_FILTER_0_CFG1, val, mask);
}

static struct event_port_ops  trp_port_ops = {
	.event_config	= trp_event_config,
	.event_filter_config	= trp_event_filter_config,
};

static void drp_event_config(struct llcc_perfmon_private *llcc_priv,
		unsigned int event_type, unsigned int counter_num,
		bool enable)
{
	uint32_t val = 0, mask;

	mask = EVENT_SEL_MASK;
	if (llcc_priv->filtered_ports & (1 << EVENT_PORT_DRP))
		mask |= FILTER_SEL_MASK | FILTER_EN_MASK;

	if (enable) {
		val = (event_type << EVENT_SEL_SHIFT) & EVENT_SEL_MASK;
		if (llcc_priv->filtered_ports & (1 << EVENT_PORT_DRP))
			val |= (FILTER_0 << FILTER_SEL_SHIFT) | FILTER_EN;

	}

	llcc_bcast_modify(llcc_priv, DRP_PROF_EVENT_n_CFG(counter_num),
			val, mask);
	perfmon_counter_config(llcc_priv, EVENT_PORT_DRP, counter_num, enable);
}

static void drp_event_enable(struct llcc_perfmon_private *llcc_priv,
		bool enable)
{
	uint32_t val = 0, mask;

	if (enable)
		val = (BEAT_SCALING << BEAT_SCALING_SHIFT) | PROF_EN;

	mask = PROF_CFG_BEAT_SCALING_MASK | PROF_CFG_EN_MASK;
	llcc_bcast_modify(llcc_priv, DRP_PROF_CFG, val, mask);
}

static struct event_port_ops drp_port_ops = {
	.event_config	= drp_event_config,
	.event_enable	= drp_event_enable,
};

static void pmgr_event_config(struct llcc_perfmon_private *llcc_priv,
		unsigned int event_type, unsigned int counter_num,
		bool enable)
{
	uint32_t val = 0, mask;

	mask = EVENT_SEL_MASK;
	if (llcc_priv->filtered_ports & (1 << EVENT_PORT_PMGR))
		mask |= FILTER_SEL_MASK | FILTER_EN_MASK;

	if (enable) {
		val = (event_type << EVENT_SEL_SHIFT) & EVENT_SEL_MASK;
		if (llcc_priv->filtered_ports & (1 << EVENT_PORT_PMGR))
			val |= (FILTER_0 << FILTER_SEL_SHIFT) | FILTER_EN;

	}

	llcc_bcast_modify(llcc_priv, PMGR_PROF_EVENT_n_CFG(counter_num),
			val, mask);
	perfmon_counter_config(llcc_priv, EVENT_PORT_PMGR, counter_num, enable);
}

static struct event_port_ops pmgr_port_ops = {
	.event_config	= pmgr_event_config,
};

static void llcc_register_event_port(struct llcc_perfmon_private *llcc_priv,
		struct event_port_ops *ops, unsigned int event_port_num)
{
	if (llcc_priv->port_configd >= MAX_NUMBER_OF_PORTS) {
		pr_err("Register port Failure!");
		return;
	}

	llcc_priv->port_configd = llcc_priv->port_configd + 1;
	llcc_priv->port_ops[event_port_num] = ops;
}

static enum hrtimer_restart llcc_perfmon_timer_handler(struct hrtimer *hrtimer)
{
	struct llcc_perfmon_private *llcc_priv = container_of(hrtimer,
			struct llcc_perfmon_private, hrtimer);

	perfmon_counter_dump(llcc_priv);
	hrtimer_forward_now(&llcc_priv->hrtimer, llcc_priv->expires);
	return HRTIMER_RESTART;
}

static int llcc_perfmon_probe(struct platform_device *pdev)
{
	int result = 0;
	struct llcc_perfmon_private *llcc_priv;
	struct device *dev = &pdev->dev;
	uint32_t val;

	llcc_priv = devm_kzalloc(&pdev->dev, sizeof(*llcc_priv), GFP_KERNEL);
	if (llcc_priv == NULL)
		return -ENOMEM;

	llcc_priv->llcc_map = syscon_node_to_regmap(dev->parent->of_node);
	if (IS_ERR(llcc_priv->llcc_map))
		return PTR_ERR(llcc_priv->llcc_map);

	result = of_property_read_u32(pdev->dev.parent->of_node,
			"qcom,llcc-broadcast-off", &llcc_priv->broadcast_off);
	if (result) {
		pr_err("Invalid qcom,broadcast-off entry\n");
		return result;
	}

	llcc_bcast_read(llcc_priv, LLCC_COMMON_STATUS0, &val);
	llcc_priv->num_mc = (val & NUM_MC_MASK) >> NUM_MC_SHIFT;
	if (llcc_priv->num_mc == 0)
		llcc_priv->num_mc = 1;
	llcc_priv->num_banks = (val & LB_CNT_MASK) >> LB_CNT_SHIFT;
	result = of_property_read_variable_u32_array(pdev->dev.parent->of_node,
			"qcom,llcc-banks-off", (u32 *)&llcc_priv->bank_off,
			1, llcc_priv->num_banks);
	if (result < 0) {
		pr_err("Invalid qcom,llcc-banks-off entry\n");
		return result;
	}

	llcc_priv->clock = devm_clk_get(&pdev->dev, "qdss_clk");
	if (IS_ERR(llcc_priv->clock)) {
		pr_err("failed to get clock node\n");
		return PTR_ERR(llcc_priv->clock);
	}

	llcc_priv->version = REV_0;
	llcc_bcast_read(llcc_priv, LLCC_COMMON_HW_INFO, &val);
	if ((val == LLCC_VERSION) && (llcc_priv->num_mc == 2))
		llcc_priv->version = REV_1;

	result = sysfs_create_group(&pdev->dev.kobj, &llcc_perfmon_group);
	if (result) {
		pr_err("Unable to create sysfs group\n");
		return result;
	}

	if (llcc_priv->num_mc > 1)
		pr_info("%d memory controllers connected with LLCC\n",
				llcc_priv->num_mc);

	mutex_init(&llcc_priv->mutex);
	platform_set_drvdata(pdev, llcc_priv);
	llcc_register_event_port(llcc_priv, &feac_port_ops, EVENT_PORT_FEAC);
	llcc_register_event_port(llcc_priv, &ferc_port_ops, EVENT_PORT_FERC);
	llcc_register_event_port(llcc_priv, &fewc_port_ops, EVENT_PORT_FEWC);
	llcc_register_event_port(llcc_priv, &beac_port_ops, EVENT_PORT_BEAC);
	llcc_register_event_port(llcc_priv, &berc_port_ops, EVENT_PORT_BERC);
	llcc_register_event_port(llcc_priv, &trp_port_ops, EVENT_PORT_TRP);
	llcc_register_event_port(llcc_priv, &drp_port_ops, EVENT_PORT_DRP);
	llcc_register_event_port(llcc_priv, &pmgr_port_ops, EVENT_PORT_PMGR);
	hrtimer_init(&llcc_priv->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	llcc_priv->hrtimer.function = llcc_perfmon_timer_handler;
	llcc_priv->expires = 0;
	return 0;
}

static int llcc_perfmon_remove(struct platform_device *pdev)
{
	struct llcc_perfmon_private *llcc_priv = platform_get_drvdata(pdev);

	while (hrtimer_active(&llcc_priv->hrtimer))
		hrtimer_cancel(&llcc_priv->hrtimer);

	mutex_destroy(&llcc_priv->mutex);
	sysfs_remove_group(&pdev->dev.kobj, &llcc_perfmon_group);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id of_match_llcc[] = {
	{
		.compatible = "qcom,llcc-perfmon",
	},
	{},
};
MODULE_DEVICE_TABLE(of, of_match_llcc);

static struct platform_driver llcc_perfmon_driver = {
	.probe = llcc_perfmon_probe,
	.remove	= llcc_perfmon_remove,
	.driver	= {
		.name = LLCC_PERFMON_NAME,
		.of_match_table = of_match_llcc,
	}
};
module_platform_driver(llcc_perfmon_driver);

MODULE_DESCRIPTION("QCOM LLCC PMU MONITOR");
MODULE_LICENSE("GPL v2");
