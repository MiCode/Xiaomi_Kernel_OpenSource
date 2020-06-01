// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <asm/arch_timer.h>
#include <linux/sched/clock.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include "tsens.h"
#include "tsens-mtc.h"

/* debug defines */
#define	TSENS_DBG_BUS_ID_0			0
#define	TSENS_DBG_BUS_ID_1			1
#define	TSENS_DBG_BUS_ID_2			2
#define	TSENS_DBG_BUS_ID_15			15
#define	TSENS_DEBUG_LOOP_COUNT_ID_0		2
#define	TSENS_DEBUG_LOOP_COUNT			5
#define	TSENS_DEBUG_STATUS_REG_START		10
#define	TSENS_DEBUG_OFFSET_RANGE		16
#define	TSENS_DEBUG_OFFSET_WORD1		0x4
#define	TSENS_DEBUG_OFFSET_WORD2		0x8
#define	TSENS_DEBUG_OFFSET_WORD3		0xc
#define	TSENS_DEBUG_OFFSET_ROW			0x10
#define	TSENS_DEBUG_DECIDEGC			-950
#define	TSENS_DEBUG_CYCLE_MS			64
#define	TSENS_DEBUG_POLL_MS			200
#define	TSENS_DEBUG_BUS_ID2_MIN_CYCLE		50
#define	TSENS_DEBUG_BUS_ID2_MAX_CYCLE		51
#define	TSENS_DEBUG_ID_MASK_1_4			0xffffffe1
#define	DEBUG_SIZE				10

#define TSENS_DEBUG_CONTROL(n)			((n) + 0x130)
#define TSENS_DEBUG_DATA(n)			((n) + 0x134)

struct tsens_dbg_func {
	int (*dbg_func)(struct tsens_device *data, u32 id, u32 dbg_type,
							int *temp);
};

static ssize_t
zonemask_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tsens_device *tmdev = NULL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	return snprintf(buf, PAGE_SIZE,
		"Zone =%d th1=%d th2=%d\n", tmdev->mtcsys.zone_mtc,
				tmdev->mtcsys.th1, tmdev->mtcsys.th2);
}

static ssize_t
zonemask_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	struct tsens_device *tmdev = NULL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	ret = sscanf(buf, "%d %d %d", &tmdev->mtcsys.zone_mtc,
				&tmdev->mtcsys.th1, &tmdev->mtcsys.th2);

	if (ret != TSENS_ZONEMASK_PARAMS) {
		pr_err("Invalid command line arguments\n");
		count = -EINVAL;
	} else {
		pr_debug("store zone_mtc=%d th1=%d th2=%d\n",
				tmdev->mtcsys.zone_mtc,
				tmdev->mtcsys.th1, tmdev->mtcsys.th2);
		ret = tsens_set_mtc_zone_sw_mask(tmdev->mtcsys.zone_mtc,
					tmdev->mtcsys.th1, tmdev->mtcsys.th2);
		if (ret < 0) {
			pr_err("Invalid command line arguments\n");
			count = -EINVAL;
		}
	}

	return count;
}

static ssize_t
zonelog_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret, zlog[TSENS_MTC_ZONE_LOG_SIZE];
	struct tsens_device *tmdev = NULL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	ret = tsens_get_mtc_zone_log(tmdev->mtcsys.zone_log, zlog);
	if (ret < 0) {
		pr_err("Invalid command line arguments\n");
		return -EINVAL;
	}

	return snprintf(buf, PAGE_SIZE,
		"Log[0]=%d\nLog[1]=%d\nLog[2]=%d\nLog[3]=%d\nLog[4]=%d\nLog[5]=%d\n",
			zlog[0], zlog[1], zlog[2], zlog[3], zlog[4], zlog[5]);
}

static ssize_t
zonelog_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	struct tsens_device *tmdev = NULL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	ret = kstrtou32(buf, 0, &tmdev->mtcsys.zone_log);
	if (ret < 0) {
		pr_err("Invalid command line arguments\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t
zonehist_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret, zhist[TSENS_MTC_ZONE_HISTORY_SIZE];
	struct tsens_device *tmdev = NULL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	ret = tsens_get_mtc_zone_history(tmdev->mtcsys.zone_hist, zhist);
	if (ret < 0) {
		pr_err("Invalid command line arguments\n");
		return -EINVAL;
	}

	return snprintf(buf, PAGE_SIZE,
		"Cool = %d\nYellow = %d\nRed = %d\n",
			zhist[0], zhist[1], zhist[2]);
}

static ssize_t
zonehist_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	struct tsens_device *tmdev = NULL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	ret = kstrtou32(buf, 0, &tmdev->mtcsys.zone_hist);
	if (ret < 0) {
		pr_err("Invalid command line arguments\n");
		return -EINVAL;
	}

	return count;
}

static struct device_attribute tsens_mtc_dev_attr[] = {
	__ATTR(zonemask, 0644, zonemask_show, zonemask_store),
	__ATTR(zonelog, 0644, zonelog_show, zonelog_store),
	__ATTR(zonehist, 0644, zonehist_show, zonehist_store),
};

static struct device_attribute tsens_mtc_dev_attr_V14[] = {
	__ATTR(zonemask, 0644, zonemask_show, zonemask_store),
	__ATTR(zonelog, 0644, zonelog_show, zonelog_store),
};

static int tsens_dbg_mtc_data(struct tsens_device *data,
					u32 id, u32 dbg_type, int *val)
{
	int result = 0, i;
	struct tsens_device *tmdev = NULL;
	struct device_attribute *attr_ptr = NULL;
	u32 ver_major, ver_minor, num_elem;

	tmdev = data;
	ver_major = tmdev->ctrl_data->ver_major;
	ver_minor = tmdev->ctrl_data->ver_minor;

	if (ver_major == 1 && ver_minor == 4) {
		attr_ptr = tsens_mtc_dev_attr_V14;
		num_elem = ARRAY_SIZE(tsens_mtc_dev_attr_V14);
	} else {
		attr_ptr = tsens_mtc_dev_attr;
		num_elem = ARRAY_SIZE(tsens_mtc_dev_attr);
	}

	for (i = 0; i < num_elem; i++) {
		result = device_create_file(&tmdev->pdev->dev, &attr_ptr[i]);
		if (result < 0)
			goto error;
	}

	return result;

error:
	for (i--; i >= 0; i--)
		device_remove_file(&tmdev->pdev->dev, &attr_ptr[i]);

	return result;
}

static int tsens_dbg_log_temp_reads(struct tsens_device *data, u32 id,
					u32 dbg_type, int *temp)
{
	struct tsens_sensor *sensor;
	struct tsens_device *tmdev = NULL;
	u32 idx = 0;

	if (!data)
		return -EINVAL;

	pr_debug("%d %d\n", id, dbg_type);
	tmdev = data;
	sensor = &tmdev->sensor[id];
	idx = tmdev->tsens_dbg.sensor_dbg_info[sensor->hw_id].idx;
	tmdev->tsens_dbg.sensor_dbg_info[sensor->hw_id].temp[idx%10] = *temp;
	tmdev->tsens_dbg.sensor_dbg_info[sensor->hw_id].time_stmp[idx%10] =
					sched_clock();
	idx++;
	tmdev->tsens_dbg.sensor_dbg_info[sensor->hw_id].idx = idx;

	TSENS_DBG(tmdev, "Sensor_id: %d temp: %d\n", id, *temp);

	return 0;
}

static int tsens_dbg_log_interrupt_timestamp(struct tsens_device *data,
						u32 id, u32 dbg_type, int *val)
{
	struct tsens_device *tmdev = NULL;
	u32 idx = 0;

	if (!data)
		return -EINVAL;

	pr_debug("%d %d\n", id, dbg_type);
	tmdev = data;
	/* debug */
	idx = tmdev->tsens_dbg.irq_idx;
	tmdev->tsens_dbg.irq_time_stmp[idx%10] =
							sched_clock();
	tmdev->tsens_dbg.irq_idx++;

	return 0;
}

static int tsens_dbg_log_bus_id_data(struct tsens_device *data,
					u32 id, u32 dbg_type, int *val)
{
	struct tsens_device *tmdev = NULL;
	u32 loop = 0, i = 0;
	uint32_t r1, r2, r3, r4, offset = 0;
	unsigned int debug_dump;
	unsigned int debug_id = 0, cntrl_id = 0;
	void __iomem *srot_addr;
	void __iomem *controller_id_addr;
	void __iomem *debug_id_addr;
	void __iomem *debug_data_addr;

	if (!data)
		return -EINVAL;

	pr_debug("%d %d\n", id, dbg_type);
	tmdev = data;
	controller_id_addr = TSENS_CONTROLLER_ID(tmdev->tsens_tm_addr);
	debug_id_addr = TSENS_DEBUG_CONTROL(tmdev->tsens_tm_addr);
	debug_data_addr = TSENS_DEBUG_DATA(tmdev->tsens_tm_addr);
	srot_addr = TSENS_CTRL_ADDR(tmdev->tsens_srot_addr);

	cntrl_id = readl_relaxed(controller_id_addr);
	TSENS_DUMP(tmdev, "TSENS Controller_id: 0x%x\n", cntrl_id);

	loop = 0;
	i = 0;
	debug_id = readl_relaxed(debug_id_addr);
	writel_relaxed((debug_id | (i << 1) | 1),
			TSENS_DEBUG_CONTROL(tmdev->tsens_tm_addr));
	while (loop < TSENS_DEBUG_LOOP_COUNT_ID_0) {
		debug_dump = readl_relaxed(debug_data_addr);
		r1 = readl_relaxed(debug_data_addr);
		r2 = readl_relaxed(debug_data_addr);
		r3 = readl_relaxed(debug_data_addr);
		r4 = readl_relaxed(debug_data_addr);

		TSENS_DUMP(tmdev,
			"ctl:%d, bus-id:%d val:0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			cntrl_id, i, debug_dump, r1, r2, r3, r4);

		loop++;
	}

	for (i = TSENS_DBG_BUS_ID_1; i <= TSENS_DBG_BUS_ID_15; i++) {
		loop = 0;
		debug_id = readl_relaxed(debug_id_addr);
		debug_id = debug_id & TSENS_DEBUG_ID_MASK_1_4;
		writel_relaxed((debug_id | (i << 1) | 1),
				TSENS_DEBUG_CONTROL(tmdev->tsens_tm_addr));
		while (loop < TSENS_DEBUG_LOOP_COUNT) {
			debug_dump = readl_relaxed(debug_data_addr);
			TSENS_DUMP(tmdev,
				"cntrl:%d, bus-id:%d with value: 0x%x\n",
				 cntrl_id, i, debug_dump);
			if (i == TSENS_DBG_BUS_ID_2)
				usleep_range(
					TSENS_DEBUG_BUS_ID2_MIN_CYCLE,
					TSENS_DEBUG_BUS_ID2_MAX_CYCLE);
			loop++;
		}
	}

	TSENS_DUMP(tmdev, "Start of TSENS TM dump for ctr 0x%x\n",
			cntrl_id);

	for (i = 0; i < TSENS_DEBUG_OFFSET_RANGE; i++) {
		r1 = readl_relaxed(controller_id_addr + offset);
		r2 = readl_relaxed(controller_id_addr + (offset +
					TSENS_DEBUG_OFFSET_WORD1));
		r3 = readl_relaxed(controller_id_addr +	(offset +
					TSENS_DEBUG_OFFSET_WORD2));
		r4 = readl_relaxed(controller_id_addr + (offset +
					TSENS_DEBUG_OFFSET_WORD3));

		TSENS_DUMP(tmdev,
			"ctrl:%d:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				cntrl_id, offset, r1, r2, r3, r4);

		offset += TSENS_DEBUG_OFFSET_ROW;
	}

	offset = 0;
	TSENS_DUMP(tmdev, "Start of TSENS SROT dump for ctr 0x%x\n",
			cntrl_id);
	for (i = 0; i < TSENS_DEBUG_OFFSET_RANGE; i++) {
		r1 = readl_relaxed(srot_addr + offset);
		r2 = readl_relaxed(srot_addr + (offset +
					TSENS_DEBUG_OFFSET_WORD1));
		r3 = readl_relaxed(srot_addr + (offset +
					TSENS_DEBUG_OFFSET_WORD2));
		r4 = readl_relaxed(srot_addr + (offset +
					TSENS_DEBUG_OFFSET_WORD3));

		TSENS_DUMP(tmdev,
			"ctrl:%d:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				cntrl_id, offset, r1, r2, r3, r4);
		offset += TSENS_DEBUG_OFFSET_ROW;
	}

	loop = 0;
	while (loop < TSENS_DEBUG_LOOP_COUNT) {
		offset = TSENS_DEBUG_OFFSET_ROW *
				TSENS_DEBUG_STATUS_REG_START;
		TSENS_DUMP(tmdev, "Start of TSENS TM dump %d\n",
					loop);
		/* Limited dump of the registers for the temperature */
		for (i = 0; i < TSENS_DEBUG_LOOP_COUNT; i++) {
			r1 = readl_relaxed(controller_id_addr + offset);
			r2 = readl_relaxed(controller_id_addr +
				(offset + TSENS_DEBUG_OFFSET_WORD1));
			r3 = readl_relaxed(controller_id_addr +
				(offset + TSENS_DEBUG_OFFSET_WORD2));
			r4 = readl_relaxed(controller_id_addr +
				(offset + TSENS_DEBUG_OFFSET_WORD3));

		TSENS_DUMP(tmdev,
			"ctrl:%d:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				cntrl_id, offset, r1, r2, r3, r4);
			offset += TSENS_DEBUG_OFFSET_ROW;
		}
		loop++;
	}

	return 0;
}

static struct tsens_dbg_func dbg_arr[] = {
	[TSENS_DBG_LOG_TEMP_READS] = {tsens_dbg_log_temp_reads},
	[TSENS_DBG_LOG_INTERRUPT_TIMESTAMP] = {
			tsens_dbg_log_interrupt_timestamp},
	[TSENS_DBG_LOG_BUS_ID_DATA] = {tsens_dbg_log_bus_id_data},
	[TSENS_DBG_MTC_DATA] = {tsens_dbg_mtc_data},
};

int tsens2xxx_dbg(struct tsens_device *data, u32 id, u32 dbg_type, int *val)
{
	if (dbg_type >= TSENS_DBG_LOG_MAX)
		return -EINVAL;

	dbg_arr[dbg_type].dbg_func(data, id, dbg_type, val);

	return 0;
}

MODULE_LICENSE("GPL v2");
