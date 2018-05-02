/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/msm_tsens.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include <asm/arch_timer.h>

#define CREATE_TRACE_POINTS
#include <trace/trace_thermal.h>

#define TSENS_DRIVER_NAME		"msm-tsens"
/* TSENS register info */
#define TSENS_UPPER_LOWER_INTERRUPT_CTRL(n)		((n) + 0x1000)
#define TSENS_INTERRUPT_EN		BIT(0)

#define TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(n)	((n) + 0x1004)
#define TSENS_UPPER_STATUS_CLR		BIT(21)
#define TSENS_LOWER_STATUS_CLR		BIT(20)
#define TSENS_UPPER_THRESHOLD_MASK	0xffc00
#define TSENS_LOWER_THRESHOLD_MASK	0x3ff
#define TSENS_UPPER_THRESHOLD_SHIFT	10

#define TSENS_S0_STATUS_ADDR(n)		((n) + 0x1030)
#define TSENS_SN_ADDR_OFFSET		0x4
#define TSENS_SN_STATUS_TEMP_MASK	0x3ff
#define TSENS_SN_STATUS_LOWER_STATUS	BIT(11)
#define TSENS_SN_STATUS_UPPER_STATUS	BIT(12)
#define TSENS_STATUS_ADDR_OFFSET			2

#define TSENS_TRDY_ADDR(n)		((n) + 0x105c)
#define TSENS_TRDY_MASK			BIT(0)

#define TSENS2_SN_STATUS_ADDR(n)	((n) + 0x1044)
#define TSENS2_SN_STATUS_VALID		BIT(14)
#define TSENS2_SN_STATUS_VALID_MASK	0x4000
#define TSENS2_TRDY_ADDR(n)		((n) + 0x84)

#define TSENS4_TRDY_ADDR(n)            ((n) + 0x1084)

#define TSENS_MTC_ZONE0_SW_MASK_ADDR(n)  ((n) + 0x10c0)
#define TSENS_TH1_MTC_IN_EFFECT               BIT(0)
#define TSENS_TH2_MTC_IN_EFFECT               BIT(1)
#define TSENS_MTC_IN_EFFECT			0x3
#define TSENS_MTC_DISABLE			0x0

#define TSENS_MTC_ZONE0_LOG(n)     ((n) + 0x10d0)
#define TSENS_LOGS_VALID_MASK      0x40000000
#define TSENS_LOGS_VALID_SHIFT     30
#define TSENS_LOGS_LATEST_MASK    0x0000001f
#define TSENS_LOGS_LOG1_MASK      0x000003e0
#define TSENS_LOGS_LOG2_MASK      0x00007c00
#define TSENS_LOGS_LOG3_MASK      0x000f8000
#define TSENS_LOGS_LOG4_MASK      0x01f00000
#define TSENS_LOGS_LOG5_MASK      0x3e000000
#define TSENS_LOGS_LOG1_SHIFT     5
#define TSENS_LOGS_LOG2_SHIFT     10
#define TSENS_LOGS_LOG3_SHIFT     15
#define TSENS_LOGS_LOG4_SHIFT     20
#define TSENS_LOGS_LOG5_SHIFT     25

/* TSENS_TM registers for 8996 */
#define TSENS_TM_INT_EN(n)			((n) + 0x1004)
#define TSENS_TM_CRITICAL_WD_BARK		BIT(31)
#define TSENS_TM_CRITICAL_CYCLE_MONITOR	BIT(30)
#define TSENS_TM_CRITICAL_INT_EN		BIT(2)
#define TSENS_TM_UPPER_INT_EN			BIT(1)
#define TSENS_TM_LOWER_INT_EN			BIT(0)
#define TSENS_TM_UPPER_LOWER_INT_DISABLE	0xffffffff

#define TSENS_TM_UPPER_INT_MASK(n)	(((n) & 0xffff0000) >> 16)
#define TSENS_TM_LOWER_INT_MASK(n)	((n) & 0xffff)
#define TSENS_TM_UPPER_LOWER_INT_STATUS(n)	((n) + 0x1008)
#define TSENS_TM_UPPER_LOWER_INT_CLEAR(n)	((n) + 0x100c)
#define TSENS_TM_UPPER_LOWER_INT_MASK(n)	((n) + 0x1010)
#define TSENS_TM_UPPER_INT_SET(n)		(1 << (n + 16))

#define TSENS_TM_CRITICAL_INT_STATUS(n)		((n) + 0x1014)
#define TSENS_TM_CRITICAL_INT_CLEAR(n)		((n) + 0x1018)
#define TSENS_TM_CRITICAL_INT_MASK(n)		((n) + 0x101c)

#define TSENS_TM_UPPER_LOWER_THRESHOLD(n)	((n) + 0x1020)
#define TSENS_TM_UPPER_THRESHOLD_SET(n)		((n) << 12)
#define TSENS_TM_UPPER_THRESHOLD_VALUE_SHIFT(n)	((n) >> 12)
#define TSENS_TM_LOWER_THRESHOLD_VALUE(n)	((n) & 0xfff)
#define TSENS_TM_UPPER_THRESHOLD_VALUE(n)	(((n) & 0xfff000) >> 12)
#define TSENS_TM_UPPER_THRESHOLD_MASK	0xfff000
#define TSENS_TM_LOWER_THRESHOLD_MASK	0xfff
#define TSENS_TM_UPPER_THRESHOLD_SHIFT	12

#define TSENS_TM_SN_CRITICAL_THRESHOLD_MASK	0xfff
#define TSENS_TM_SN_CRITICAL_THRESHOLD(n)	((n) + 0x1060)
#define TSENS_TM_SN_STATUS(n)			((n) + 0x10a0)
#define TSENS_TM_SN_STATUS_VALID_BIT		BIT(21)
#define TSENS_TM_SN_STATUS_CRITICAL_STATUS	BIT(19)
#define TSENS_TM_SN_STATUS_UPPER_STATUS		BIT(18)
#define TSENS_TM_SN_STATUS_LOWER_STATUS		BIT(17)
#define TSENS_TM_SN_LAST_TEMP_MASK		0xfff

#define TSENS_TM_TRDY(n)			((n) + 0x10e4)
#define TSENS_TM_CODE_BIT_MASK			0xfff
#define TSENS_TM_CODE_SIGN_BIT			0x800

#define TSENS_CONTROLLER_ID(n)			((n) + 0x1000)
#define TSENS_DEBUG_CONTROL(n)			((n) + 0x1130)
#define TSENS_DEBUG_DATA(n)			((n) + 0x1134)
#define TSENS_TM_MTC_ZONE0_SW_MASK_ADDR(n)	((n) + 0x1140)
#define TSENS_TM_MTC_ZONE0_LOG(n)		((n) + 0x1150)
#define TSENS_TM_MTC_ZONE0_HISTORY(n)		((n) + 0x1160)
#define TSENS_RESET_HISTORY_MASK	0x4
#define TSENS_RESET_HISTORY_SHIFT	2
#define TSENS_PS_RED_CMD_MASK	0x3ff00000
#define TSENS_PS_YELLOW_CMD_MASK	0x000ffc00
#define TSENS_PS_COOL_CMD_MASK	0x000003ff
#define TSENS_PS_YELLOW_CMD_SHIFT	0xa
#define TSENS_PS_RED_CMD_SHIFT	0x14
/* End TSENS_TM registers for 8996 */

#define TSENS_CTRL_ADDR(n)		(n)
#define TSENS_EN			BIT(0)

#define TSENS_CAL_DEGC_POINT1		30
#define TSENS_CAL_DEGC_POINT2		120
#define TSENS_SLOPE_FACTOR		1000

/* TSENS register data */
#define TSENS_TRDY_RDY_MIN_TIME		2000
#define TSENS_TRDY_RDY_MAX_TIME		2100
#define TSENS_THRESHOLD_MAX_CODE	0x3ff
#define TSENS_THRESHOLD_MIN_CODE	0x0

#define TSENS_TYPE0		0
#define TSENS_TYPE2		2
#define TSENS_TYPE3		3
#define TSENS_TYPE4		4

/* debug defines */
#define TSENS_DBG_BUS_ID_0		0
#define TSENS_DBG_BUS_ID_1		1
#define TSENS_DBG_BUS_ID_2		2
#define TSENS_DBG_BUS_ID_15		15
#define TSENS_DEBUG_LOOP_COUNT_ID_0	2
#define TSENS_DEBUG_LOOP_COUNT		5
#define TSENS_DEBUG_STATUS_REG_START	10
#define TSENS_DEBUG_OFFSET_RANGE	16
#define TSENS_DEBUG_OFFSET_WORD1	0x4
#define TSENS_DEBUG_OFFSET_WORD2	0x8
#define TSENS_DEBUG_OFFSET_WORD3	0xc
#define TSENS_DEBUG_OFFSET_ROW		0x10
#define TSENS_DEBUG_DECIDEGC		-950
#define TSENS_DEBUG_CYCLE_MS		64
#define TSENS_DEBUG_POLL_MS		200
#define TSENS_DEBUG_BUS_ID2_MIN_CYCLE	50
#define TSENS_DEBUG_BUS_ID2_MAX_CYCLE	51
#define TSENS_DEBUG_ID_MASK_1_4		0xffffffe1

static uint32_t tsens_sec_to_msec_value = 1000;
static uint32_t tsens_completion_timeout_hz = HZ/2;
static uint32_t tsens_poll_check = 1;

/* Trips: warm and cool */
enum tsens_trip_type {
	TSENS_TRIP_WARM = 0,
	TSENS_TRIP_COOL,
	TSENS_TRIP_NUM,
};

enum tsens_tm_trip_type {
	TSENS_TM_TRIP_WARM = 0,
	TSENS_TM_TRIP_COOL,
	TSENS_TM_TRIP_CRITICAL,
	TSENS_TM_TRIP_NUM,
};

#define TSENS_WRITABLE_TRIPS_MASK ((1 << TSENS_TRIP_NUM) - 1)
#define TSENS_TM_WRITABLE_TRIPS_MASK ((1 << TSENS_TM_TRIP_NUM) - 1)

struct tsens_thrshld_state {
	enum thermal_device_mode	high_th_state;
	enum thermal_device_mode	low_th_state;
	enum thermal_device_mode	crit_th_state;
	unsigned int			high_adc_code;
	unsigned int			low_adc_code;
	int				high_temp;
	int				low_temp;
	int				crit_temp;
};

struct tsens_tm_device_sensor {
	struct thermal_zone_device	*tz_dev;
	struct tsens_tm_device		*tm;
	enum thermal_device_mode	mode;
	/* Physical HW sensor number */
	unsigned int			sensor_hw_num;
	/* Software index. This is keep track of the HW/SW
	 * sensor_ID mapping */
	unsigned int			sensor_sw_id;
	unsigned int			sensor_client_id;
	int				offset;
	int				calib_data_point1;
	int				calib_data_point2;
	uint32_t			slope_mul_tsens_factor;
	struct tsens_thrshld_state	debug_thr_state_copy;
	/* dbg_adc_code logs either the raw ADC code or temperature values in
	 * decidegC based on the controller settings.
	 */
	int				dbg_adc_code;
	u32				wa_temp1_calib_offset_factor;
	u32				wa_temp2_calib_offset_factor;
};

struct tsens_dbg_counter {
	uint32_t			dbg_count[10];
	uint32_t			idx;
	unsigned long long		time_stmp[10];
};

struct tsens_sensor_dbg_info {
	unsigned long			temp[10];
	uint32_t			idx;
	unsigned long long		time_stmp[10];
	int				adccode[10];
};

struct tsens_mtc_sysfs {
	uint32_t zone_log;
	int zone_mtc;
	int th1;
	int th2;
	uint32_t zone_hist;
};

struct tsens_tm_device {
	struct platform_device		*pdev;
	struct workqueue_struct		*tsens_critical_wq;
	struct list_head		list;
	bool				is_ready;
	bool				prev_reading_avail;
	bool				calibration_less_mode;
	bool				tsens_local_init;
	bool				gain_offset_programmed;
	bool				cycle_compltn_monitor;
	bool				wd_bark;
	int				tsens_factor;
	uint32_t			tsens_num_sensor;
	uint32_t			cycle_compltn_monitor_val;
	uint32_t			wd_bark_val;
	int				tsens_irq;
	int				tsens_critical_irq;
	void __iomem			*tsens_addr;
	void __iomem			*tsens_calib_addr;
	int				tsens_len;
	int				calib_len;
	struct resource			*res_tsens_mem;
	struct resource			*res_calib_mem;
	uint32_t			tsens_type;
	bool				tsens_valid_status_check;
	struct tsens_dbg_counter	tsens_thread_iq_dbg;
	struct tsens_sensor_dbg_info	sensor_dbg_info[16];
	int				tsens_upper_irq_cnt;
	int				tsens_lower_irq_cnt;
	int				tsens_critical_irq_cnt;
	int				tsens_critical_wd_cnt;
	struct delayed_work		tsens_critical_poll_test;
	struct completion		tsens_rslt_completion;
	struct tsens_mtc_sysfs		mtcsys;
	spinlock_t			tsens_crit_lock;
	spinlock_t			tsens_upp_low_lock;
	bool				crit_set;
	struct tsens_dbg_counter	crit_timestamp_last_run;
	struct tsens_dbg_counter	crit_timestamp_last_interrupt_handled;
	struct tsens_dbg_counter	crit_timestamp_last_poll_request;
	u64				qtimer_val_detection_start;
	u64				qtimer_val_last_detection_interrupt;
	u64				qtimer_val_last_polling_check;
	bool				tsens_critical_poll;
	struct tsens_tm_device_sensor	sensor[0];
};

LIST_HEAD(tsens_device_list);

static char dbg_buff[1024];
static struct dentry *dent;
static struct dentry *dfile_stats;

static struct of_device_id tsens_match[] = {
	{	.compatible = "qcom,msm8996-tsens",
	},
	{	.compatible = "qcom,msmtitanium-tsens",
	},
	{	.compatible = "qcom,msm8998-tsens",
	},
	{	.compatible = "qcom,msmhamster-tsens",
	},
	{	.compatible = "qcom,sdm660-tsens",
	},
	{	.compatible = "qcom,sdm630-tsens",
	},
	{}
};

static struct tsens_tm_device *tsens_controller_is_present(void)
{
	struct tsens_tm_device *tmdev_chip = NULL;

	if (list_empty(&tsens_device_list)) {
		pr_err("%s: TSENS controller not available\n", __func__);
		return tmdev_chip;
	}

	list_for_each_entry(tmdev_chip, &tsens_device_list, list)
		return tmdev_chip;

	return tmdev_chip;
}

static int32_t get_tsens_sensor_for_client_id(struct tsens_tm_device *tmdev,
						uint32_t sensor_client_id)
{
	bool id_found = false;
	uint32_t i = 0;
	struct device_node *of_node = NULL;
	const struct of_device_id *id;

	of_node = tmdev->pdev->dev.of_node;
	if (of_node == NULL) {
		pr_err("Invalid of_node??\n");
		return -EINVAL;
	}

	if (!of_match_node(tsens_match, of_node)) {
		pr_err("Need to read SoC specific fuse map\n");
		return -ENODEV;
	}

	id = of_match_node(tsens_match, of_node);
	if (id == NULL) {
		pr_err("can not find tsens_match of_node\n");
		return -ENODEV;
	}

	if (!strcmp(id->compatible, "qcom,msm8996-tsens") ||
		(!strcmp(id->compatible, "qcom,msm8998-tsens")) ||
		(!strcmp(id->compatible, "qcom,sdm660-tsens")) ||
		(!strcmp(id->compatible, "qcom,sdm630-tsens")) ||
		(!strcmp(id->compatible, "qcom,msmhamster-tsens"))) {
		while (i < tmdev->tsens_num_sensor && !id_found) {
			if (tmdev->sensor[i].sensor_client_id ==
							sensor_client_id) {
				id_found = true;
				return tmdev->sensor[i].sensor_hw_num;
			}
			i++;
		}
	} else
		return sensor_client_id;

	if (!id_found)
		return -EINVAL;

	return -EINVAL;
}

static struct tsens_tm_device *get_tsens_controller_for_client_id(
						uint32_t sensor_client_id)
{
	struct tsens_tm_device *tmdev_chip = NULL;
	bool id_found = false;
	uint32_t i = 0;

	list_for_each_entry(tmdev_chip, &tsens_device_list, list) {
		i = 0;
		while (i < tmdev_chip->tsens_num_sensor && !id_found) {
			if (tmdev_chip->sensor[i].sensor_client_id ==
						sensor_client_id) {
				id_found = true;
				return tmdev_chip;
			}
			i++;
		}
	}

	if (!id_found)
		return NULL;

	return tmdev_chip;
}

static struct tsens_tm_device *get_all_tsens_controller_sensor_count(
						uint32_t *sensor_count)
{
	struct tsens_tm_device *tmdev_chip = NULL;

	list_for_each_entry(tmdev_chip, &tsens_device_list, list)
		*sensor_count += tmdev_chip->tsens_num_sensor;

	return tmdev_chip;
}

int tsens_is_ready(void)
{
	struct tsens_tm_device *tmdev = NULL;

	tmdev = tsens_controller_is_present();
	if (!tmdev)
		return -EPROBE_DEFER;
	else
		return tmdev->is_ready;
}
EXPORT_SYMBOL(tsens_is_ready);

static int tsens_get_sw_id_mapping_for_controller(
					int sensor_hw_num,
					int *sensor_sw_idx,
					struct tsens_tm_device *tmdev)
{
	int i = 0;
	bool id_found = false;

	while (i < tmdev->tsens_num_sensor && !id_found) {
		if (sensor_hw_num == tmdev->sensor[i].sensor_hw_num) {
			*sensor_sw_idx = tmdev->sensor[i].sensor_sw_id;
			id_found = true;
		}
		i++;
	}

	if (!id_found)
		return -EINVAL;

	return 0;
}

int tsens_get_hw_id_mapping(int thermal_sensor_num, int *sensor_client_id)
{
	struct tsens_tm_device *tmdev = NULL;
	struct device_node *of_node = NULL;
	const struct of_device_id *id;
	uint32_t tsens_max_sensors = 0, idx = 0, i = 0;

	if (list_empty(&tsens_device_list)) {
		pr_err("%s: TSENS controller not available\n", __func__);
		return -EPROBE_DEFER;
	}

	list_for_each_entry(tmdev, &tsens_device_list, list)
		tsens_max_sensors += tmdev->tsens_num_sensor;

	if (tsens_max_sensors != thermal_sensor_num) {
		pr_err("TSENS total sensors is %d, thermal expects:%d\n",
			tsens_max_sensors, thermal_sensor_num);
		return -EINVAL;
	}

	list_for_each_entry(tmdev, &tsens_device_list, list) {
		of_node = tmdev->pdev->dev.of_node;
		if (of_node == NULL) {
			pr_err("Invalid of_node??\n");
			return -EINVAL;
		}

		if (!of_match_node(tsens_match, of_node)) {
			pr_err("Need to read SoC specific fuse map\n");
			return -ENODEV;
		}

		id = of_match_node(tsens_match, of_node);
		if (id == NULL) {
			pr_err("can not find tsens_match of_node\n");
			return -ENODEV;
		}

		if (!strcmp(id->compatible, "qcom,msm8996-tsens") ||
			(!strcmp(id->compatible, "qcom,msm8998-tsens")) ||
			(!strcmp(id->compatible, "qcom,sdm660-tsens")) ||
			(!strcmp(id->compatible, "qcom,sdm630-tsens")) ||
			(!strcmp(id->compatible, "qcom,msmhamster-tsens"))) {
			/* Assign client id's that is used to get the
			 * controller and hw_sensor details
			 */
			for (i = 0; i < tmdev->tsens_num_sensor; i++) {
				sensor_client_id[idx] =
					tmdev->sensor[i].sensor_client_id;
				idx++;
			}
		} else {
			/* Assign the corresponding hw sensor number
			 * prior to support for multiple controllres
			 */
			for (i = 0; i < tmdev->tsens_num_sensor; i++) {
				sensor_client_id[idx] =
					tmdev->sensor[i].sensor_hw_num;
				idx++;
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(tsens_get_hw_id_mapping);

static ssize_t
zonemask_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tsens_tm_device *tmdev = NULL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	return snprintf(buf, PAGE_SIZE,
		"Zone =%d th1=%d th2=%d\n" , tmdev->mtcsys.zone_mtc,
				tmdev->mtcsys.th1 , tmdev->mtcsys.th2);
}

static ssize_t
zonemask_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	struct tsens_tm_device *tmdev = NULL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	ret = sscanf(buf, "%d %d %d", &tmdev->mtcsys.zone_mtc ,
				&tmdev->mtcsys.th1 , &tmdev->mtcsys.th2);

	if (ret != TSENS_ZONEMASK_PARAMS) {
		pr_err("Invalid command line arguments\n");
		count = -EINVAL;
	} else {
		pr_debug("store zone_mtc=%d th1=%d th2=%d\n",
				tmdev->mtcsys.zone_mtc,
				tmdev->mtcsys.th1 , tmdev->mtcsys.th2);
		ret = tsens_set_mtc_zone_sw_mask(tmdev->mtcsys.zone_mtc ,
					tmdev->mtcsys.th1 , tmdev->mtcsys.th2);
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
	struct tsens_tm_device *tmdev = NULL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	ret = tsens_get_mtc_zone_log(tmdev->mtcsys.zone_log , zlog);
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
	struct tsens_tm_device *tmdev = NULL;

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
	struct tsens_tm_device *tmdev = NULL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	ret = tsens_get_mtc_zone_history(tmdev->mtcsys.zone_hist , zhist);
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
	struct tsens_tm_device *tmdev = NULL;

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

static int create_tsens_mtc_sysfs(struct platform_device *pdev)
{
	int result = 0, i;
	struct device_attribute *attr_ptr = NULL;

	attr_ptr = tsens_mtc_dev_attr;

	for (i = 0; i < ARRAY_SIZE(tsens_mtc_dev_attr); i++) {
		result = device_create_file(&pdev->dev, &attr_ptr[i]);
		if (result < 0)
			goto error;
	}

	pr_debug("create_tsens_mtc_sysfs success\n");

	return result;

error:
	for (i--; i >= 0; i--)
		device_remove_file(&pdev->dev, &attr_ptr[i]);

	return result;
}

static int tsens_tz_code_to_degc(int adc_code, int sensor_sw_id,
				struct tsens_tm_device *tmdev)
{
	int degc, num, den, idx;

	idx = sensor_sw_id;
	num = ((adc_code * tmdev->tsens_factor) -
				tmdev->sensor[idx].offset);
	den = (int) tmdev->sensor[idx].slope_mul_tsens_factor;

	if (num > 0)
		degc = ((num + (den/2))/den);
	else if (num < 0)
		degc = ((num - (den/2))/den);
	else
		degc = num/den;

	pr_debug("raw_code:0x%x, sensor_num:%d, degc:%d, offset:%d\n",
			adc_code, idx, degc, tmdev->sensor[idx].offset);

	return degc;
}

static int tsens_tz_degc_to_code(int degc, int idx,
				struct tsens_tm_device *tmdev)
{
	int code = ((degc * tmdev->sensor[idx].slope_mul_tsens_factor)
		+ tmdev->sensor[idx].offset)/tmdev->tsens_factor;

	if (code > TSENS_THRESHOLD_MAX_CODE)
		code = TSENS_THRESHOLD_MAX_CODE;
	else if (code < TSENS_THRESHOLD_MIN_CODE)
		code = TSENS_THRESHOLD_MIN_CODE;
	pr_debug("raw_code:0x%x, sensor_num:%d, degc:%d\n",
			code, idx, degc);
	return code;
}

static int msm_tsens_get_temp(int sensor_client_id, int *temp)
{
	unsigned int code;
	void __iomem *sensor_addr;
	void __iomem *trdy_addr;
	int sensor_sw_id = -EINVAL, rc = 0, last_temp = 0, last_temp2 = 0;
	int last_temp3 = 0, last_temp_mask, valid_status_mask, code_mask = 0;
	bool last_temp_valid = false, last_temp2_valid = false;
	bool last_temp3_valid = false;
	struct tsens_tm_device *tmdev = NULL;
	uint32_t sensor_hw_num = 0;

	tmdev = get_tsens_controller_for_client_id(sensor_client_id);
	if (tmdev == NULL) {
		pr_err("TSENS early init not done\n");
		return -EPROBE_DEFER;
	}

	pr_debug("sensor_client_id:%d\n", sensor_client_id);

	sensor_hw_num = get_tsens_sensor_for_client_id(tmdev, sensor_client_id);
	if (sensor_hw_num < 0) {
		pr_err("cannot read the temperature\n");
		return sensor_hw_num;
	}
	pr_debug("sensor_hw_num:%d\n", sensor_hw_num);

	if (tmdev->tsens_type == TSENS_TYPE2) {
		trdy_addr = TSENS2_TRDY_ADDR(tmdev->tsens_addr);
		sensor_addr = TSENS2_SN_STATUS_ADDR(tmdev->tsens_addr);
	} else if (tmdev->tsens_type == TSENS_TYPE3) {
		trdy_addr = TSENS_TM_TRDY(tmdev->tsens_addr);
		sensor_addr = TSENS_TM_SN_STATUS(tmdev->tsens_addr);
	} else if (tmdev->tsens_type == TSENS_TYPE4) {
		trdy_addr = TSENS4_TRDY_ADDR(tmdev->tsens_addr);
		sensor_addr = TSENS2_SN_STATUS_ADDR(tmdev->tsens_addr);
	} else {
		trdy_addr = TSENS_TRDY_ADDR(tmdev->tsens_addr);
		sensor_addr = TSENS_S0_STATUS_ADDR(tmdev->tsens_addr);
	}

	if ((!tmdev->prev_reading_avail) && !tmdev->tsens_valid_status_check) {
		while (!((readl_relaxed_no_log(trdy_addr)) & TSENS_TRDY_MASK))
			usleep_range(TSENS_TRDY_RDY_MIN_TIME,
				TSENS_TRDY_RDY_MAX_TIME);
		tmdev->prev_reading_avail = true;
	}

	if (tmdev->tsens_type == TSENS_TYPE3)
		last_temp_mask = TSENS_TM_SN_LAST_TEMP_MASK;
	else
		last_temp_mask = TSENS_SN_STATUS_TEMP_MASK;

	code = readl_relaxed_no_log(sensor_addr +
			(sensor_hw_num << TSENS_STATUS_ADDR_OFFSET));
	last_temp = code & last_temp_mask;

	if (tmdev->tsens_valid_status_check) {
		if (tmdev->tsens_type == TSENS_TYPE3)
			valid_status_mask = TSENS_TM_SN_STATUS_VALID_BIT;
		else
			valid_status_mask = TSENS2_SN_STATUS_VALID;
		if (code & valid_status_mask)
			last_temp_valid = true;
		else {
			code = readl_relaxed_no_log(sensor_addr +
				(sensor_hw_num << TSENS_STATUS_ADDR_OFFSET));
			last_temp2 = code & last_temp_mask;
			if (code & valid_status_mask) {
				last_temp = last_temp2;
				last_temp2_valid = true;
			} else {
				code = readl_relaxed_no_log(sensor_addr +
					(sensor_hw_num <<
					TSENS_STATUS_ADDR_OFFSET));
				last_temp3 = code & last_temp_mask;
				if (code & valid_status_mask) {
					last_temp = last_temp3;
					last_temp3_valid = true;
				}
			}
		}
	}

	if ((tmdev->tsens_valid_status_check) &&
		(!last_temp_valid && !last_temp2_valid && !last_temp3_valid)) {
		if (last_temp == last_temp2)
			last_temp = last_temp2;
		else if (last_temp2 == last_temp3)
			last_temp = last_temp3;
	}

	if (tmdev->tsens_type != TSENS_TYPE3) {
		/* Obtain SW index to map the corresponding thermal zone's
		 * offset and slope for code to degc conversion. */
		rc = tsens_get_sw_id_mapping_for_controller(sensor_hw_num,
						&sensor_sw_id, tmdev);
		if (rc < 0) {
			pr_err("tsens mapping index not found\n");
			return rc;
		}

		*temp = tsens_tz_code_to_degc(last_temp, sensor_sw_id, tmdev);
	} else {
		if (last_temp & TSENS_TM_CODE_SIGN_BIT) {
			/* Sign extension for negative value */
			code_mask = ~TSENS_TM_CODE_BIT_MASK;
			last_temp |= code_mask;
		}
		*temp = last_temp;
	}

	tmdev->sensor[sensor_hw_num].dbg_adc_code = last_temp;

	trace_tsens_read(*temp, sensor_client_id);

	return 0;
}

static int tsens_tz_get_temp(struct thermal_zone_device *thermal,
			     int *temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	struct tsens_tm_device *tmdev = NULL;
	uint32_t idx = 0;
	int rc = 0;

	if (!tm_sensor || !temp)
		return -EINVAL;

	tmdev = tm_sensor->tm;
	if (!tmdev)
		return -EINVAL;

	rc = msm_tsens_get_temp(tm_sensor->sensor_client_id, temp);
	if (rc)
		return rc;

	idx = tmdev->sensor_dbg_info[tm_sensor->sensor_hw_num].idx;
	tmdev->sensor_dbg_info[tm_sensor->sensor_hw_num].temp[idx%10] = *temp;
	tmdev->sensor_dbg_info[tm_sensor->sensor_hw_num].time_stmp[idx%10] =
					sched_clock();
	tmdev->sensor_dbg_info[tm_sensor->sensor_hw_num].adccode[idx%10] =
			tmdev->sensor[tm_sensor->sensor_hw_num].dbg_adc_code;
	idx++;
	tmdev->sensor_dbg_info[tm_sensor->sensor_hw_num].idx = idx;

	return 0;
}

int tsens_get_temp(struct tsens_device *device, int *temp)
{
	int rc = 0;

	if (tsens_is_ready() <= 0) {
		pr_debug("TSENS early init not done\n");
		return -EPROBE_DEFER;
	}

	rc = msm_tsens_get_temp(device->sensor_num, temp);
	if (rc)
		return rc;

	return 0;
}
EXPORT_SYMBOL(tsens_get_temp);

int tsens_get_max_sensor_num(uint32_t *tsens_num_sensors)
{
	if (tsens_is_ready() <= 0) {
		pr_debug("TSENS early init not done\n");
		return -EPROBE_DEFER;
	}

	*tsens_num_sensors = 0;

	if (get_all_tsens_controller_sensor_count(tsens_num_sensors) == NULL)
		return -EINVAL;

	pr_debug("%d\n", *tsens_num_sensors);

	return 0;
}
EXPORT_SYMBOL(tsens_get_max_sensor_num);

static int tsens_tz_get_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode *mode)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;

	if (!tm_sensor || !mode)
		return -EINVAL;

	*mode = tm_sensor->mode;

	return 0;
}

static int tsens_tz_get_trip_type(struct thermal_zone_device *thermal,
				   int trip, enum thermal_trip_type *type)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;

	if (!tm_sensor || trip < 0 || !type)
		return -EINVAL;

	switch (trip) {
	case TSENS_TRIP_WARM:
		*type = THERMAL_TRIP_CONFIGURABLE_HI;
		break;
	case TSENS_TRIP_COOL:
		*type = THERMAL_TRIP_CONFIGURABLE_LOW;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tsens_tm_get_trip_type(struct thermal_zone_device *thermal,
				   int trip, enum thermal_trip_type *type)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;

	if (!tm_sensor || trip < 0 || !type)
		return -EINVAL;

	switch (trip) {
	case TSENS_TM_TRIP_WARM:
		*type = THERMAL_TRIP_CONFIGURABLE_HI;
		break;
	case TSENS_TM_TRIP_COOL:
		*type = THERMAL_TRIP_CONFIGURABLE_LOW;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tsens_tm_activate_trip_type(struct thermal_zone_device *thermal,
			int trip, enum thermal_trip_activation_mode mode)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg_cntl, mask;
	unsigned long flags;
	struct tsens_tm_device *tmdev = NULL;
	int rc = 0;

	/* clear the interrupt and unmask */
	if (!tm_sensor || trip < 0)
		return -EINVAL;

	tmdev = tm_sensor->tm;
	if (!tmdev)
		return -EINVAL;

	spin_lock_irqsave(&tmdev->tsens_upp_low_lock, flags);
	mask = (tm_sensor->sensor_hw_num);
	switch (trip) {
	case TSENS_TM_TRIP_CRITICAL:
		tmdev->sensor[tm_sensor->sensor_hw_num].
			debug_thr_state_copy.crit_th_state = mode;
		reg_cntl = readl_relaxed(TSENS_TM_CRITICAL_INT_MASK
							(tmdev->tsens_addr));
		if (mode == THERMAL_TRIP_ACTIVATION_DISABLED)
			writel_relaxed(reg_cntl | (1 << mask),
				(TSENS_TM_CRITICAL_INT_MASK
				(tmdev->tsens_addr)));
		else
			writel_relaxed(reg_cntl & ~(1 << mask),
				(TSENS_TM_CRITICAL_INT_MASK
				(tmdev->tsens_addr)));
		break;
	case TSENS_TM_TRIP_WARM:
		tmdev->sensor[tm_sensor->sensor_hw_num].
			debug_thr_state_copy.high_th_state = mode;
		reg_cntl = readl_relaxed(TSENS_TM_UPPER_LOWER_INT_MASK
						(tmdev->tsens_addr));
		if (mode == THERMAL_TRIP_ACTIVATION_DISABLED)
			writel_relaxed(reg_cntl |
				(TSENS_TM_UPPER_INT_SET(mask)),
				(TSENS_TM_UPPER_LOWER_INT_MASK
				(tmdev->tsens_addr)));
		else
			writel_relaxed(reg_cntl &
				~(TSENS_TM_UPPER_INT_SET(mask)),
				(TSENS_TM_UPPER_LOWER_INT_MASK
				(tmdev->tsens_addr)));
		break;
	case TSENS_TM_TRIP_COOL:
		tmdev->sensor[tm_sensor->sensor_hw_num].
			debug_thr_state_copy.low_th_state = mode;
		reg_cntl = readl_relaxed(TSENS_TM_UPPER_LOWER_INT_MASK
						(tmdev->tsens_addr));
		if (mode == THERMAL_TRIP_ACTIVATION_DISABLED)
			writel_relaxed(reg_cntl | (1 << mask),
			(TSENS_TM_UPPER_LOWER_INT_MASK(tmdev->tsens_addr)));
		else
			writel_relaxed(reg_cntl & ~(1 << mask),
			(TSENS_TM_UPPER_LOWER_INT_MASK(tmdev->tsens_addr)));
		break;
	default:
		rc = -EINVAL;
	}

	spin_unlock_irqrestore(&tmdev->tsens_upp_low_lock, flags);
	/* Activate and enable the respective trip threshold setting */
	mb();

	return rc;
}

static int tsens_tz_activate_trip_type(struct thermal_zone_device *thermal,
			int trip, enum thermal_trip_activation_mode mode)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg_cntl, code, hi_code, lo_code, mask;
	struct tsens_tm_device *tmdev = NULL;

	if (!tm_sensor || trip < 0)
		return -EINVAL;

	tmdev = tm_sensor->tm;
	if (!tmdev)
		return -EINVAL;

	lo_code = TSENS_THRESHOLD_MIN_CODE;
	hi_code = TSENS_THRESHOLD_MAX_CODE;

	reg_cntl = readl_relaxed((TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
					(tmdev->tsens_addr) +
					(tm_sensor->sensor_hw_num *
					TSENS_SN_ADDR_OFFSET)));

	switch (trip) {
	case TSENS_TRIP_WARM:
		tmdev->sensor[tm_sensor->sensor_hw_num].
				debug_thr_state_copy.high_th_state = mode;

		code = (reg_cntl & TSENS_UPPER_THRESHOLD_MASK)
					>> TSENS_UPPER_THRESHOLD_SHIFT;
		mask = TSENS_UPPER_STATUS_CLR;

		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_cntl & TSENS_LOWER_THRESHOLD_MASK);
		break;
	case TSENS_TRIP_COOL:
		tmdev->sensor[tm_sensor->sensor_hw_num].
				debug_thr_state_copy.low_th_state = mode;

		code = (reg_cntl & TSENS_LOWER_THRESHOLD_MASK);
		mask = TSENS_LOWER_STATUS_CLR;

		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_cntl & TSENS_UPPER_THRESHOLD_MASK)
					>> TSENS_UPPER_THRESHOLD_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	if (mode == THERMAL_TRIP_ACTIVATION_DISABLED)
		writel_relaxed(reg_cntl | mask,
		(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(tmdev->tsens_addr) +
			(tm_sensor->sensor_hw_num * TSENS_SN_ADDR_OFFSET)));

	else
		writel_relaxed(reg_cntl & ~mask,
		(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(tmdev->tsens_addr) +
		(tm_sensor->sensor_hw_num * TSENS_SN_ADDR_OFFSET)));
	/* Enable the thresholds */
	mb();
	return 0;
}

static int tsens_tm_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip, int *temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	int reg_cntl, code_mask;
	struct tsens_tm_device *tmdev = NULL;

	if (!tm_sensor || trip < 0 || !temp)
		return -EINVAL;

	tmdev = tm_sensor->tm;
	if (!tmdev)
		return -EINVAL;

	switch (trip) {
	case TSENS_TM_TRIP_CRITICAL:
		reg_cntl = readl_relaxed((TSENS_TM_SN_CRITICAL_THRESHOLD
						(tmdev->tsens_addr)) +
				(tm_sensor->sensor_hw_num *
				TSENS_SN_ADDR_OFFSET));
		if (reg_cntl & TSENS_TM_CODE_SIGN_BIT) {
			/* Sign extension for negative value */
			code_mask = ~TSENS_TM_CODE_BIT_MASK;
			reg_cntl |= code_mask;
		}
		break;
	case TSENS_TM_TRIP_WARM:
		reg_cntl = readl_relaxed((TSENS_TM_UPPER_LOWER_THRESHOLD
						(tmdev->tsens_addr)) +
				(tm_sensor->sensor_hw_num *
				TSENS_SN_ADDR_OFFSET));
		reg_cntl = TSENS_TM_UPPER_THRESHOLD_VALUE(reg_cntl);
		if (reg_cntl & TSENS_TM_CODE_SIGN_BIT) {
			/* Sign extension for negative value */
			code_mask = ~TSENS_TM_CODE_BIT_MASK;
			reg_cntl |= code_mask;
		}
		break;
	case TSENS_TM_TRIP_COOL:
		reg_cntl = readl_relaxed((TSENS_TM_UPPER_LOWER_THRESHOLD
						(tmdev->tsens_addr)) +
				(tm_sensor->sensor_hw_num *
				TSENS_SN_ADDR_OFFSET));
		reg_cntl = TSENS_TM_LOWER_THRESHOLD_VALUE(reg_cntl);
		if (reg_cntl & TSENS_TM_CODE_SIGN_BIT) {
			/* Sign extension for negative value */
			code_mask = ~TSENS_TM_CODE_BIT_MASK;
			reg_cntl |= code_mask;
		}
		break;
	default:
		return -EINVAL;
	}

	*temp = reg_cntl;

	return 0;
}

static int tsens_tz_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip, int *temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg;
	int sensor_sw_id = -EINVAL, rc = 0;
	struct tsens_tm_device *tmdev = NULL;

	if (!tm_sensor || trip < 0 || !temp)
		return -EINVAL;

	tmdev = tm_sensor->tm;
	if (!tmdev)
		return -EINVAL;

	reg = readl_relaxed(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
						(tmdev->tsens_addr) +
			(tm_sensor->sensor_hw_num * TSENS_SN_ADDR_OFFSET));
	switch (trip) {
	case TSENS_TRIP_WARM:
		reg = (reg & TSENS_UPPER_THRESHOLD_MASK) >>
				TSENS_UPPER_THRESHOLD_SHIFT;
		break;
	case TSENS_TRIP_COOL:
		reg = (reg & TSENS_LOWER_THRESHOLD_MASK);
		break;
	default:
		return -EINVAL;
	}

	rc = tsens_get_sw_id_mapping_for_controller(tm_sensor->sensor_hw_num,
							&sensor_sw_id, tmdev);
	if (rc < 0) {
		pr_err("tsens mapping index not found\n");
		return rc;
	}
	*temp = tsens_tz_code_to_degc(reg, sensor_sw_id, tmdev);

	return 0;
}

static int tsens_tz_notify(struct thermal_zone_device *thermal,
				int count, enum thermal_trip_type type)
{
	/* Critical temperature threshold are enabled and will
	 * shutdown the device once critical thresholds are crossed. */
	pr_debug("%s debug\n", __func__);
	return 1;
}

static int tsens_tm_set_trip_temp(struct thermal_zone_device *thermal,
				   int trip, int temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg_cntl;
	unsigned long flags;
	struct tsens_tm_device *tmdev = NULL;
	int rc = 0;

	if (!tm_sensor || trip < 0)
		return -EINVAL;

	tmdev = tm_sensor->tm;
	if (!tmdev)
		return -EINVAL;

	spin_lock_irqsave(&tmdev->tsens_upp_low_lock, flags);
	switch (trip) {
	case TSENS_TM_TRIP_CRITICAL:
		tmdev->sensor[tm_sensor->sensor_hw_num].
				debug_thr_state_copy.crit_temp = temp;
		temp &= TSENS_TM_SN_CRITICAL_THRESHOLD_MASK;
		writel_relaxed(temp,
			(TSENS_TM_SN_CRITICAL_THRESHOLD(tmdev->tsens_addr) +
			(tm_sensor->sensor_hw_num * TSENS_SN_ADDR_OFFSET)));
		break;
	case TSENS_TM_TRIP_WARM:
		tmdev->sensor[tm_sensor->sensor_hw_num].
				debug_thr_state_copy.high_temp = temp;
		reg_cntl = readl_relaxed((TSENS_TM_UPPER_LOWER_THRESHOLD
				(tmdev->tsens_addr)) +
				(tm_sensor->sensor_hw_num *
				TSENS_SN_ADDR_OFFSET));
		temp = TSENS_TM_UPPER_THRESHOLD_SET(temp);
		temp &= TSENS_TM_UPPER_THRESHOLD_MASK;
		reg_cntl &= ~TSENS_TM_UPPER_THRESHOLD_MASK;
		writel_relaxed(reg_cntl | temp,
			(TSENS_TM_UPPER_LOWER_THRESHOLD(tmdev->tsens_addr) +
			(tm_sensor->sensor_hw_num * TSENS_SN_ADDR_OFFSET)));
		break;
	case TSENS_TM_TRIP_COOL:
		tmdev->sensor[tm_sensor->sensor_hw_num].
				debug_thr_state_copy.low_temp = temp;
		reg_cntl = readl_relaxed((TSENS_TM_UPPER_LOWER_THRESHOLD
				(tmdev->tsens_addr)) +
				(tm_sensor->sensor_hw_num *
				TSENS_SN_ADDR_OFFSET));
		temp &= TSENS_TM_LOWER_THRESHOLD_MASK;
		reg_cntl &= ~TSENS_TM_LOWER_THRESHOLD_MASK;
		writel_relaxed(reg_cntl | temp,
			(TSENS_TM_UPPER_LOWER_THRESHOLD(tmdev->tsens_addr) +
			(tm_sensor->sensor_hw_num * TSENS_SN_ADDR_OFFSET)));
		break;
	default:
		rc = -EINVAL;
	}

	spin_unlock_irqrestore(&tmdev->tsens_upp_low_lock, flags);
	/* Set trip temperature thresholds */
	mb();
	return rc;
}

static int tsens_tz_set_trip_temp(struct thermal_zone_device *thermal,
				   int trip, int temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg_cntl;
	int code, hi_code, lo_code, code_err_chk, sensor_sw_id = 0, rc = 0;
	struct tsens_tm_device *tmdev = NULL;

	if (!tm_sensor || trip < 0)
		return -EINVAL;

	tmdev = tm_sensor->tm;
	if (!tmdev)
		return -EINVAL;

	rc = tsens_get_sw_id_mapping_for_controller(tm_sensor->sensor_hw_num,
							&sensor_sw_id, tmdev);
	if (rc < 0) {
		pr_err("tsens mapping index not found\n");
		return rc;
	}

	code_err_chk = code = tsens_tz_degc_to_code(temp, sensor_sw_id, tmdev);
	if (!tm_sensor || trip < 0)
		return -EINVAL;

	lo_code = TSENS_THRESHOLD_MIN_CODE;
	hi_code = TSENS_THRESHOLD_MAX_CODE;

	reg_cntl = readl_relaxed(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
			(tmdev->tsens_addr) + (tm_sensor->sensor_hw_num *
					TSENS_SN_ADDR_OFFSET));
	switch (trip) {
	case TSENS_TRIP_WARM:
		tmdev->sensor[tm_sensor->sensor_hw_num].
				debug_thr_state_copy.high_adc_code = code;
		tmdev->sensor[tm_sensor->sensor_hw_num].
				debug_thr_state_copy.high_temp = temp;
		code <<= TSENS_UPPER_THRESHOLD_SHIFT;
		reg_cntl &= ~TSENS_UPPER_THRESHOLD_MASK;
		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_cntl & TSENS_LOWER_THRESHOLD_MASK);
		break;
	case TSENS_TRIP_COOL:
		tmdev->sensor[tm_sensor->sensor_hw_num].
				debug_thr_state_copy.low_adc_code = code;
		tmdev->sensor[tm_sensor->sensor_hw_num].
				debug_thr_state_copy.low_temp = temp;
		reg_cntl &= ~TSENS_LOWER_THRESHOLD_MASK;
		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_cntl & TSENS_UPPER_THRESHOLD_MASK)
					>> TSENS_UPPER_THRESHOLD_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	writel_relaxed(reg_cntl | code, (TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
					(tmdev->tsens_addr) +
					(tm_sensor->sensor_hw_num *
					TSENS_SN_ADDR_OFFSET)));
	/* Activate the set trip temperature thresholds */
	mb();
	return 0;
}

static void tsens_poll(struct work_struct *work)
{
	struct tsens_tm_device *tmdev = container_of(work,
		       struct tsens_tm_device, tsens_critical_poll_test.work);
	unsigned int reg_cntl, mask, rc = 0, debug_dump, i = 0, loop = 0;
	unsigned int debug_id = 0, cntrl_id = 0;
	uint32_t r1, r2, r3, r4, offset = 0, idx = 0;
	unsigned long temp, flags;
	unsigned int status, int_mask, int_mask_val;
	void __iomem *srot_addr;
	void __iomem *controller_id_addr;
	void __iomem *debug_id_addr;
	void __iomem *debug_data_addr;
	void __iomem *sensor_status_addr;
	void __iomem *sensor_int_mask_addr;
	void __iomem *sensor_critical_addr;

	/* Set the Critical temperature threshold to a value of 10 that should
	 * guarantee a threshold to trigger. Check the interrupt count if
	 * it did. Schedule the next round of the above test again after
	 * 3 seconds.
	 */

	controller_id_addr = TSENS_CONTROLLER_ID(tmdev->tsens_addr);
	debug_id_addr = TSENS_DEBUG_CONTROL(tmdev->tsens_addr);
	debug_data_addr = TSENS_DEBUG_DATA(tmdev->tsens_addr);
	srot_addr = TSENS_CTRL_ADDR(tmdev->tsens_addr);

	temp = TSENS_DEBUG_DECIDEGC;
	/* Sensor 0 on either of the controllers */
	mask = 0;

	reinit_completion(&tmdev->tsens_rslt_completion);

	temp &= TSENS_TM_SN_CRITICAL_THRESHOLD_MASK;
	writel_relaxed(temp,
			(TSENS_TM_SN_CRITICAL_THRESHOLD(tmdev->tsens_addr) +
			(mask * TSENS_SN_ADDR_OFFSET)));

	/* debug */
	idx = tmdev->crit_timestamp_last_run.idx;
	tmdev->crit_timestamp_last_run.time_stmp[idx%10] = sched_clock();
	tmdev->crit_timestamp_last_run.idx++;
	tmdev->qtimer_val_detection_start = arch_counter_get_cntvct();

	spin_lock_irqsave(&tmdev->tsens_crit_lock, flags);
	/* Clear the sensor0 critical status */
	int_mask_val = 1;
	writel_relaxed(int_mask_val,
		TSENS_TM_CRITICAL_INT_CLEAR(tmdev->tsens_addr));
	writel_relaxed(0,
		TSENS_TM_CRITICAL_INT_CLEAR(
					tmdev->tsens_addr));
	/* Clear the status */
	mb();
	tmdev->crit_set = true;
	if (!tmdev->tsens_critical_poll) {
		reg_cntl = readl_relaxed(
			TSENS_TM_CRITICAL_INT_MASK(tmdev->tsens_addr));
		writel_relaxed(reg_cntl & ~(1 << mask),
				(TSENS_TM_CRITICAL_INT_MASK
				(tmdev->tsens_addr)));
		/* Enable the critical int mask */
		mb();
	}
	spin_unlock_irqrestore(&tmdev->tsens_crit_lock, flags);

	if (tmdev->tsens_critical_poll) {
		msleep(TSENS_DEBUG_POLL_MS);
		sensor_status_addr = TSENS_TM_SN_STATUS(tmdev->tsens_addr);

		spin_lock_irqsave(&tmdev->tsens_crit_lock, flags);
		status = readl_relaxed(sensor_status_addr);
		spin_unlock_irqrestore(&tmdev->tsens_crit_lock, flags);

		if (status & TSENS_TM_SN_STATUS_CRITICAL_STATUS)
			goto re_schedule;
		else {
			pr_err("status:0x%x\n", status);
			goto debug_start;
		}
	}

	rc = wait_for_completion_timeout(
				&tmdev->tsens_rslt_completion,
				tsens_completion_timeout_hz);
	if (!rc) {
		pr_debug("Switch to polling, TSENS critical interrupt failed\n");
		sensor_status_addr = TSENS_TM_SN_STATUS(tmdev->tsens_addr);
		sensor_int_mask_addr =
			TSENS_TM_CRITICAL_INT_MASK(tmdev->tsens_addr);
		sensor_critical_addr =
			TSENS_TM_SN_CRITICAL_THRESHOLD(tmdev->tsens_addr);

		spin_lock_irqsave(&tmdev->tsens_crit_lock, flags);
		if (!tmdev->crit_set) {
			pr_debug("Ignore this check cycle\n");
			spin_unlock_irqrestore(&tmdev->tsens_crit_lock, flags);
			goto re_schedule;
		}
		status = readl_relaxed(sensor_status_addr);
		int_mask = readl_relaxed(sensor_int_mask_addr);
		tmdev->crit_set = false;
		spin_unlock_irqrestore(&tmdev->tsens_crit_lock, flags);

		idx = tmdev->crit_timestamp_last_poll_request.idx;
		tmdev->crit_timestamp_last_poll_request.time_stmp[idx%10] =
								sched_clock();
		tmdev->crit_timestamp_last_poll_request.idx++;
		tmdev->qtimer_val_last_polling_check =
						arch_counter_get_cntvct();
		if (status & TSENS_TM_SN_STATUS_CRITICAL_STATUS) {

			spin_lock_irqsave(&tmdev->tsens_crit_lock, flags);
			int_mask = readl_relaxed(sensor_int_mask_addr);
			int_mask_val = 1;
			/* Mask the corresponding interrupt for the sensors */
			writel_relaxed(int_mask | int_mask_val,
				TSENS_TM_CRITICAL_INT_MASK(
					tmdev->tsens_addr));
			/* Clear the corresponding sensors interrupt */
			writel_relaxed(int_mask_val,
				TSENS_TM_CRITICAL_INT_CLEAR(tmdev->tsens_addr));
			writel_relaxed(0,
				TSENS_TM_CRITICAL_INT_CLEAR(
					tmdev->tsens_addr));
			spin_unlock_irqrestore(&tmdev->tsens_crit_lock, flags);

			/* Clear critical status */
			mb();
			goto re_schedule;
		}

debug_start:
		cntrl_id = readl_relaxed(controller_id_addr);
		pr_err("Controller_id: 0x%x\n", cntrl_id);

		loop = 0;
		i = 0;
		debug_id = readl_relaxed(debug_id_addr);
		writel_relaxed((debug_id | (i << 1) | 1),
				TSENS_DEBUG_CONTROL(tmdev->tsens_addr));
		while (loop < TSENS_DEBUG_LOOP_COUNT_ID_0) {
			debug_dump = readl_relaxed(debug_data_addr);
			r1 = readl_relaxed(debug_data_addr);
			r2 = readl_relaxed(debug_data_addr);
			r3 = readl_relaxed(debug_data_addr);
			r4 = readl_relaxed(debug_data_addr);
			pr_err("cntrl:%d, bus-id:%d value:0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
				cntrl_id, i, debug_dump, r1, r2, r3, r4);
			loop++;
		}

		for (i = TSENS_DBG_BUS_ID_1; i <= TSENS_DBG_BUS_ID_15; i++) {
			loop = 0;
			debug_id = readl_relaxed(debug_id_addr);
			debug_id = debug_id & TSENS_DEBUG_ID_MASK_1_4;
			writel_relaxed((debug_id | (i << 1) | 1),
					TSENS_DEBUG_CONTROL(tmdev->tsens_addr));
			while (loop < TSENS_DEBUG_LOOP_COUNT) {
				debug_dump = readl_relaxed(debug_data_addr);
				pr_err("cntrl:%d, bus-id:%d with value: 0x%x\n",
					cntrl_id, i, debug_dump);
				if (i == TSENS_DBG_BUS_ID_2)
					usleep_range(
						TSENS_DEBUG_BUS_ID2_MIN_CYCLE,
						TSENS_DEBUG_BUS_ID2_MAX_CYCLE);
				loop++;
			}
		}

		pr_err("Start of TSENS TM dump\n");
		for (i = 0; i < TSENS_DEBUG_OFFSET_RANGE; i++) {
			r1 = readl_relaxed(controller_id_addr + offset);
			r2 = readl_relaxed(controller_id_addr + (offset +
						TSENS_DEBUG_OFFSET_WORD1));
			r3 = readl_relaxed(controller_id_addr +	(offset +
						TSENS_DEBUG_OFFSET_WORD2));
			r4 = readl_relaxed(controller_id_addr + (offset +
						TSENS_DEBUG_OFFSET_WORD3));

			pr_err("ctrl:%d:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				cntrl_id, offset, r1, r2, r3, r4);
			offset += TSENS_DEBUG_OFFSET_ROW;
		}

		offset = 0;
		pr_err("Start of TSENS SROT dump\n");
		for (i = 0; i < TSENS_DEBUG_OFFSET_RANGE; i++) {
			r1 = readl_relaxed(srot_addr + offset);
			r2 = readl_relaxed(srot_addr + (offset +
						TSENS_DEBUG_OFFSET_WORD1));
			r3 = readl_relaxed(srot_addr + (offset +
						TSENS_DEBUG_OFFSET_WORD2));
			r4 = readl_relaxed(srot_addr + (offset +
						TSENS_DEBUG_OFFSET_WORD3));

			pr_err("ctrl:%d:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				cntrl_id, offset, r1, r2, r3, r4);
			offset += TSENS_DEBUG_OFFSET_ROW;
		}

		loop = 0;
		while (loop < TSENS_DEBUG_LOOP_COUNT) {
			offset = TSENS_DEBUG_OFFSET_ROW *
					TSENS_DEBUG_STATUS_REG_START;
			pr_err("Start of TSENS TM dump %d\n", loop);
			/* Limited dump of the registers for the temperature */
			for (i = 0; i < TSENS_DEBUG_LOOP_COUNT; i++) {
				r1 = readl_relaxed(controller_id_addr + offset);
				r2 = readl_relaxed(controller_id_addr +
					(offset + TSENS_DEBUG_OFFSET_WORD1));
				r3 = readl_relaxed(controller_id_addr +
					(offset + TSENS_DEBUG_OFFSET_WORD2));
				r4 = readl_relaxed(controller_id_addr +
					(offset + TSENS_DEBUG_OFFSET_WORD3));

			pr_err("ctrl:%d:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				cntrl_id, offset, r1, r2, r3, r4);
				offset += TSENS_DEBUG_OFFSET_ROW;
			}
			loop++;
			msleep(TSENS_DEBUG_CYCLE_MS);
		}
		BUG();
	}

re_schedule:

	schedule_delayed_work(&tmdev->tsens_critical_poll_test,
			msecs_to_jiffies(tsens_sec_to_msec_value));
}

int tsens_mtc_reset_history_counter(unsigned int zone)
{
	unsigned int reg_cntl, is_valid;
	void __iomem *sensor_addr;
	struct tsens_tm_device *tmdev = NULL;

	if (zone > TSENS_NUM_MTC_ZONES_SUPPORT)
			return -EINVAL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	sensor_addr = TSENS_TM_MTC_ZONE0_SW_MASK_ADDR(tmdev->tsens_addr);
	reg_cntl = readl_relaxed((sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
	is_valid = (reg_cntl & TSENS_RESET_HISTORY_MASK)
				>> TSENS_RESET_HISTORY_SHIFT;
	if (!is_valid) {
		/*Enable the bit to reset counter*/
		writel_relaxed(reg_cntl | (1 << TSENS_RESET_HISTORY_SHIFT),
				(sensor_addr + (zone * TSENS_SN_ADDR_OFFSET)));
		reg_cntl = readl_relaxed((sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
		pr_debug("tsens : zone =%d reg=%x\n", zone , reg_cntl);
	}

	/*Disble the bit to start counter*/
	writel_relaxed(reg_cntl & ~(1 << TSENS_RESET_HISTORY_SHIFT),
				(sensor_addr + (zone * TSENS_SN_ADDR_OFFSET)));
	reg_cntl = readl_relaxed((sensor_addr +
			(zone * TSENS_SN_ADDR_OFFSET)));
	pr_debug("tsens : zone =%d reg=%x\n", zone , reg_cntl);

	return 0;
}
EXPORT_SYMBOL(tsens_mtc_reset_history_counter);

int tsens_set_mtc_zone_sw_mask(unsigned int zone , unsigned int th1_enable,
				unsigned int th2_enable)
{
	unsigned int reg_cntl;
	void __iomem *sensor_addr;
	struct tsens_tm_device *tmdev = NULL;

	if (zone > TSENS_NUM_MTC_ZONES_SUPPORT)
			return -EINVAL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	if (tmdev->tsens_type == TSENS_TYPE3)
			sensor_addr = TSENS_TM_MTC_ZONE0_SW_MASK_ADDR
						(tmdev->tsens_addr);
		else
			sensor_addr = TSENS_MTC_ZONE0_SW_MASK_ADDR
						(tmdev->tsens_addr);

	if (th1_enable && th2_enable)
		writel_relaxed(TSENS_MTC_IN_EFFECT,
				(sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
	if (!th1_enable && !th2_enable)
		writel_relaxed(TSENS_MTC_DISABLE,
				(sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
	if (th1_enable && !th2_enable)
		writel_relaxed(TSENS_TH1_MTC_IN_EFFECT,
				(sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
	if (!th1_enable && th2_enable)
		writel_relaxed(TSENS_TH2_MTC_IN_EFFECT,
				(sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
	reg_cntl = readl_relaxed((sensor_addr +
				(zone *	TSENS_SN_ADDR_OFFSET)));
	pr_debug("tsens : zone =%d th1=%d th2=%d reg=%x\n",
		zone , th1_enable , th2_enable , reg_cntl);

	return 0;
}
EXPORT_SYMBOL(tsens_set_mtc_zone_sw_mask);

int tsens_get_mtc_zone_log(unsigned int zone , void *zone_log)
{
	unsigned int i , reg_cntl , is_valid , log[TSENS_MTC_ZONE_LOG_SIZE];
	int *zlog = (int *)zone_log;
	void __iomem *sensor_addr;
	struct tsens_tm_device *tmdev = NULL;

	if (zone > TSENS_NUM_MTC_ZONES_SUPPORT)
		return -EINVAL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	if (tmdev->tsens_type == TSENS_TYPE3)
		sensor_addr = TSENS_TM_MTC_ZONE0_LOG(tmdev->tsens_addr);
	else
		sensor_addr = TSENS_MTC_ZONE0_LOG(tmdev->tsens_addr);

	reg_cntl = readl_relaxed((sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
	is_valid = (reg_cntl & TSENS_LOGS_VALID_MASK)
				>> TSENS_LOGS_VALID_SHIFT;
	if (is_valid) {
		log[0] = (reg_cntl & TSENS_LOGS_LATEST_MASK);
		log[1] = (reg_cntl & TSENS_LOGS_LOG1_MASK)
				  >> TSENS_LOGS_LOG1_SHIFT;
		log[2] = (reg_cntl & TSENS_LOGS_LOG2_MASK)
				  >> TSENS_LOGS_LOG2_SHIFT;
		log[3] = (reg_cntl & TSENS_LOGS_LOG3_MASK)
				  >> TSENS_LOGS_LOG3_SHIFT;
		log[4] = (reg_cntl & TSENS_LOGS_LOG4_MASK)
				  >> TSENS_LOGS_LOG4_SHIFT;
		log[5] = (reg_cntl & TSENS_LOGS_LOG5_MASK)
				  >> TSENS_LOGS_LOG5_SHIFT;
		for (i = 0; i < (TSENS_MTC_ZONE_LOG_SIZE); i++) {
			*(zlog+i) = log[i];
			pr_debug("Log[%d]=%d\n", i , log[i]);
		}
	} else {
		pr_debug("tsens: Valid bit disabled\n");
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(tsens_get_mtc_zone_log);

int tsens_get_mtc_zone_history(unsigned int zone , void *zone_hist)
{
	unsigned int i, reg_cntl, hist[TSENS_MTC_ZONE_HISTORY_SIZE];
	int *zhist = (int *)zone_hist;
	void __iomem *sensor_addr;
	struct tsens_tm_device *tmdev = NULL;

	if (zone > TSENS_NUM_MTC_ZONES_SUPPORT)
		return -EINVAL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	sensor_addr = TSENS_TM_MTC_ZONE0_HISTORY(tmdev->tsens_addr);
	reg_cntl = readl_relaxed((sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));

	hist[0] = (reg_cntl & TSENS_PS_COOL_CMD_MASK);
	hist[1] = (reg_cntl & TSENS_PS_YELLOW_CMD_MASK)
			  >> TSENS_PS_YELLOW_CMD_SHIFT;
	hist[2] = (reg_cntl & TSENS_PS_RED_CMD_MASK)
			  >> TSENS_PS_RED_CMD_SHIFT;
	for (i = 0; i < (TSENS_MTC_ZONE_HISTORY_SIZE); i++) {
		*(zhist+i) = hist[i];
		pr_debug("tsens : %d\n", hist[i]);
	}

	return 0;
}
EXPORT_SYMBOL(tsens_get_mtc_zone_history);

static struct thermal_zone_device_ops tsens_thermal_zone_ops = {
	.get_temp = tsens_tz_get_temp,
	.get_mode = tsens_tz_get_mode,
	.get_trip_type = tsens_tz_get_trip_type,
	.activate_trip_type = tsens_tz_activate_trip_type,
	.get_trip_temp = tsens_tz_get_trip_temp,
	.set_trip_temp = tsens_tz_set_trip_temp,
	.notify = tsens_tz_notify,
};

/* Thermal zone ops for decidegC */
static struct thermal_zone_device_ops tsens_tm_thermal_zone_ops = {
	.get_temp = tsens_tz_get_temp,
	.get_trip_type = tsens_tm_get_trip_type,
	.activate_trip_type = tsens_tm_activate_trip_type,
	.get_trip_temp = tsens_tm_get_trip_temp,
	.set_trip_temp = tsens_tm_set_trip_temp,
	.notify = tsens_tz_notify,
};

static irqreturn_t tsens_tm_critical_irq_thread(int irq, void *data)
{
	struct tsens_tm_device *tm = data;
	unsigned int i, status, idx = 0;
	unsigned long flags;
	void __iomem *sensor_status_addr;
	void __iomem *sensor_int_mask_addr;
	void __iomem *sensor_critical_addr;
	void __iomem *wd_critical_addr;
	int sensor_sw_id = -EINVAL, rc = 0;
	int wd_mask;

	tm->crit_set = false;
	sensor_status_addr = TSENS_TM_SN_STATUS(tm->tsens_addr);
	sensor_int_mask_addr =
		TSENS_TM_CRITICAL_INT_MASK(tm->tsens_addr);
	sensor_critical_addr =
		TSENS_TM_SN_CRITICAL_THRESHOLD(tm->tsens_addr);
	wd_critical_addr =
		TSENS_TM_CRITICAL_INT_STATUS(tm->tsens_addr);

	if (tm->wd_bark) {
		wd_mask = readl_relaxed(wd_critical_addr);
		/*
		* Check whether the reason for critical interrupt is
		* because of watchdog
		*/
		if (wd_mask & TSENS_TM_CRITICAL_WD_BARK) {
			/*
			 * Clear watchdog interrupt and
			 * increment global wd count
			 */
			writel_relaxed(wd_mask | TSENS_TM_CRITICAL_WD_BARK,
				(TSENS_TM_CRITICAL_INT_CLEAR
				(tm->tsens_addr)));
			writel_relaxed(wd_mask & ~(TSENS_TM_CRITICAL_WD_BARK),
				(TSENS_TM_CRITICAL_INT_CLEAR
				(tm->tsens_addr)));
			tm->tsens_critical_wd_cnt++;
			return IRQ_HANDLED;
		}
	}

	for (i = 0; i < tm->tsens_num_sensor; i++) {
		bool critical_thr = false;
		int int_mask, int_mask_val;
		uint32_t addr_offset;

		spin_lock_irqsave(&tm->tsens_crit_lock, flags);
		addr_offset = tm->sensor[i].sensor_hw_num *
						TSENS_SN_ADDR_OFFSET;
		status = readl_relaxed(sensor_status_addr + addr_offset);
		int_mask = readl_relaxed(sensor_int_mask_addr);

		if ((status & TSENS_TM_SN_STATUS_CRITICAL_STATUS) &&
			!(int_mask & (1 << tm->sensor[i].sensor_hw_num))) {
			int_mask = readl_relaxed(sensor_int_mask_addr);
			int_mask_val = (1 << tm->sensor[i].sensor_hw_num);
			/* Mask the corresponding interrupt for the sensors */
			writel_relaxed(int_mask | int_mask_val,
				TSENS_TM_CRITICAL_INT_MASK(
					tm->tsens_addr));
			/* Clear the corresponding sensors interrupt */
			writel_relaxed(int_mask_val,
				TSENS_TM_CRITICAL_INT_CLEAR(tm->tsens_addr));
			writel_relaxed(0,
				TSENS_TM_CRITICAL_INT_CLEAR(
					tm->tsens_addr));
			critical_thr = true;
			tm->sensor[i].debug_thr_state_copy.
					crit_th_state = THERMAL_DEVICE_DISABLED;
		}
		spin_unlock_irqrestore(&tm->tsens_crit_lock, flags);

		if (critical_thr) {
			int temp;

			tsens_tz_get_temp(tm->sensor[i].tz_dev, &temp);
			rc = tsens_get_sw_id_mapping_for_controller(
					tm->sensor[i].sensor_hw_num,
					&sensor_sw_id, tm);
			if (rc < 0)
				pr_err("tsens mapping index not found\n");
			pr_debug("sensor:%d trigger temp (%d degC) with count:%d\n",
				tm->sensor[i].sensor_hw_num,
				(status & TSENS_TM_SN_LAST_TEMP_MASK),
				tm->tsens_critical_irq_cnt);
				tm->tsens_critical_irq_cnt++;
		}
	}

	idx = tm->crit_timestamp_last_interrupt_handled.idx;
	tm->crit_timestamp_last_interrupt_handled.dbg_count[idx%10]++;
	tm->crit_timestamp_last_interrupt_handled.time_stmp[idx%10] =
							sched_clock();
	tm->qtimer_val_last_detection_interrupt = arch_counter_get_cntvct();
	if (tsens_poll_check)
		complete(&tm->tsens_rslt_completion);
	/* Mask critical interrupt */
	mb();

	return IRQ_HANDLED;
}

static irqreturn_t tsens_tm_irq_thread(int irq, void *data)
{
	struct tsens_tm_device *tm = data;
	unsigned int i, status, threshold;
	unsigned long flags;
	void __iomem *sensor_status_addr;
	void __iomem *sensor_int_mask_addr;
	void __iomem *sensor_upper_lower_addr;
	int sensor_sw_id = -EINVAL, rc = 0;
	uint32_t addr_offset;

	sensor_status_addr = TSENS_TM_SN_STATUS(tm->tsens_addr);
	sensor_int_mask_addr =
		TSENS_TM_UPPER_LOWER_INT_MASK(tm->tsens_addr);
	sensor_upper_lower_addr =
		TSENS_TM_UPPER_LOWER_THRESHOLD(tm->tsens_addr);

	for (i = 0; i < tm->tsens_num_sensor; i++) {
		bool upper_thr = false, lower_thr = false;
		int int_mask, int_mask_val = 0;

		spin_lock_irqsave(&tm->tsens_upp_low_lock, flags);
		addr_offset = tm->sensor[i].sensor_hw_num *
						TSENS_SN_ADDR_OFFSET;
		status = readl_relaxed(sensor_status_addr + addr_offset);
		threshold = readl_relaxed(sensor_upper_lower_addr +
								addr_offset);
		int_mask = readl_relaxed(sensor_int_mask_addr);

		if ((status & TSENS_TM_SN_STATUS_UPPER_STATUS) &&
			!(int_mask &
				(1 << (tm->sensor[i].sensor_hw_num + 16)))) {
			int_mask = readl_relaxed(sensor_int_mask_addr);
			int_mask_val = TSENS_TM_UPPER_INT_SET(
					tm->sensor[i].sensor_hw_num);
			/* Mask the corresponding interrupt for the sensors */
			writel_relaxed(int_mask | int_mask_val,
				TSENS_TM_UPPER_LOWER_INT_MASK(
					tm->tsens_addr));
			/* Clear the corresponding sensors interrupt */
			writel_relaxed(int_mask_val,
				TSENS_TM_UPPER_LOWER_INT_CLEAR(
					tm->tsens_addr));
			writel_relaxed(0,
				TSENS_TM_UPPER_LOWER_INT_CLEAR(
					tm->tsens_addr));
			upper_thr = true;
			tm->sensor[i].debug_thr_state_copy.
					high_th_state = THERMAL_DEVICE_DISABLED;
		}

		if ((status & TSENS_TM_SN_STATUS_LOWER_STATUS) &&
			!(int_mask &
				(1 << tm->sensor[i].sensor_hw_num))) {
			int_mask = readl_relaxed(sensor_int_mask_addr);
			int_mask_val = (1 << tm->sensor[i].sensor_hw_num);
			/* Mask the corresponding interrupt for the sensors */
			writel_relaxed(int_mask | int_mask_val,
				TSENS_TM_UPPER_LOWER_INT_MASK(
					tm->tsens_addr));
			/* Clear the corresponding sensors interrupt */
			writel_relaxed(int_mask_val,
				TSENS_TM_UPPER_LOWER_INT_CLEAR(
					tm->tsens_addr));
			writel_relaxed(0,
				TSENS_TM_UPPER_LOWER_INT_CLEAR(
					tm->tsens_addr));
			lower_thr = true;
			tm->sensor[i].debug_thr_state_copy.
					low_th_state = THERMAL_DEVICE_DISABLED;
		}
		spin_unlock_irqrestore(&tm->tsens_upp_low_lock, flags);

		if (upper_thr || lower_thr) {
			int temp;
			enum thermal_trip_type trip =
					THERMAL_TRIP_CONFIGURABLE_LOW;

			if (upper_thr)
				trip = THERMAL_TRIP_CONFIGURABLE_HI;
			tsens_tz_get_temp(tm->sensor[i].tz_dev, &temp);
			thermal_sensor_trip(tm->sensor[i].tz_dev, trip, temp);

			rc = tsens_get_sw_id_mapping_for_controller(
					tm->sensor[i].sensor_hw_num,
					&sensor_sw_id, tm);
			if (rc < 0)
				pr_debug("tsens mapping index not found\n");
			/* Use sensor_client_id for multiple controllers */
			pr_debug("sensor:%d trigger temp (%d degC)\n",
				tm->sensor[i].sensor_client_id,
				(status & TSENS_TM_SN_LAST_TEMP_MASK));
			if (upper_thr) {
				trace_tsens_threshold_hit(
					TSENS_TM_UPPER_THRESHOLD_VALUE(
						threshold),
					tm->sensor[i].sensor_client_id);
				tm->tsens_upper_irq_cnt++;
			} else {
				trace_tsens_threshold_clear(
					TSENS_TM_LOWER_THRESHOLD_VALUE(
						threshold),
					tm->sensor[i].sensor_client_id);
				tm->tsens_lower_irq_cnt++;
			}
		}
	}

	/* Disable monitoring sensor trip threshold for triggered sensor */
	mb();

	return IRQ_HANDLED;
}

static irqreturn_t tsens_irq_thread(int irq, void *data)
{
	struct tsens_tm_device *tm = data;
	unsigned int i, status, threshold;
	void __iomem *sensor_status_addr;
	void __iomem *sensor_status_ctrl_addr;
	int sensor_sw_id = -EINVAL;
	uint32_t idx = 0;

	if ((tm->tsens_type == TSENS_TYPE2) ||
			(tm->tsens_type == TSENS_TYPE4))
		sensor_status_addr = TSENS2_SN_STATUS_ADDR(tm->tsens_addr);
	else
		sensor_status_addr = TSENS_S0_STATUS_ADDR(tm->tsens_addr);

	sensor_status_ctrl_addr =
		TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(tm->tsens_addr);
	for (i = 0; i < tm->tsens_num_sensor; i++) {
		bool upper_thr = false, lower_thr = false;
		uint32_t addr_offset;

		sensor_sw_id = tm->sensor[i].sensor_sw_id;
		addr_offset = tm->sensor[i].sensor_hw_num *
						TSENS_SN_ADDR_OFFSET;
		status = readl_relaxed(sensor_status_addr + addr_offset);
		threshold = readl_relaxed(sensor_status_ctrl_addr +
								addr_offset);
		if (status & TSENS_SN_STATUS_UPPER_STATUS) {
			writel_relaxed(threshold | TSENS_UPPER_STATUS_CLR,
				TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(
					tm->tsens_addr + addr_offset));
			upper_thr = true;
			tm->sensor[i].debug_thr_state_copy.
					high_th_state = THERMAL_DEVICE_DISABLED;
		}
		if (status & TSENS_SN_STATUS_LOWER_STATUS) {
			writel_relaxed(threshold | TSENS_LOWER_STATUS_CLR,
				TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(
					tm->tsens_addr + addr_offset));
			lower_thr = true;
			tm->sensor[i].debug_thr_state_copy.
					low_th_state = THERMAL_DEVICE_DISABLED;
		}
		if (upper_thr || lower_thr) {
			int temp;
			enum thermal_trip_type trip =
					THERMAL_TRIP_CONFIGURABLE_LOW;

			if (upper_thr)
				trip = THERMAL_TRIP_CONFIGURABLE_HI;
			tsens_tz_get_temp(tm->sensor[i].tz_dev, &temp);
			thermal_sensor_trip(tm->sensor[i].tz_dev, trip, temp);

			pr_debug("sensor:%d trigger temp (%d degC)\n",
				tm->sensor[i].sensor_hw_num,
				tsens_tz_code_to_degc((status &
				TSENS_SN_STATUS_TEMP_MASK),
				tm->sensor[i].sensor_sw_id, tm));
			if (upper_thr)
				trace_tsens_threshold_hit(
					tsens_tz_code_to_degc((threshold &
					TSENS_UPPER_THRESHOLD_MASK) >>
					TSENS_UPPER_THRESHOLD_SHIFT,
					sensor_sw_id, tm),
					tm->sensor[i].sensor_hw_num);
			else
				trace_tsens_threshold_clear(
					tsens_tz_code_to_degc((threshold &
					TSENS_LOWER_THRESHOLD_MASK),
					sensor_sw_id, tm),
					tm->sensor[i].sensor_hw_num);
		}
	}
	/* debug */
	idx = tm->tsens_thread_iq_dbg.idx;
	tm->tsens_thread_iq_dbg.dbg_count[idx%10]++;
	tm->tsens_thread_iq_dbg.time_stmp[idx%10] = sched_clock();
	tm->tsens_thread_iq_dbg.idx++;

	/* Disable monitoring sensor trip threshold for triggered sensor */
	mb();

	return IRQ_HANDLED;
}

static int tsens_hw_init(struct tsens_tm_device *tmdev)
{
	void __iomem *srot_addr;
	void __iomem *sensor_int_mask_addr;
	unsigned int srot_val;
	int crit_mask;
	void __iomem *int_mask_addr;

	if (!tmdev) {
		pr_err("Invalid tsens device\n");
		return -EINVAL;
	}

	if (tmdev->tsens_type == TSENS_TYPE3) {
		srot_addr = TSENS_CTRL_ADDR(tmdev->tsens_addr + 0x4);
		srot_val = readl_relaxed(srot_addr);
		if (!(srot_val & TSENS_EN)) {
			pr_err("TSENS device is not enabled\n");
			return -ENODEV;
		}

		if (tmdev->cycle_compltn_monitor) {
			sensor_int_mask_addr =
				TSENS_TM_CRITICAL_INT_MASK(tmdev->tsens_addr);
			crit_mask = readl_relaxed(sensor_int_mask_addr);
			writel_relaxed(
				crit_mask | tmdev->cycle_compltn_monitor_val,
				(TSENS_TM_CRITICAL_INT_MASK
				(tmdev->tsens_addr)));
			/*Update critical cycle monitoring*/
			mb();
		}
		int_mask_addr = TSENS_TM_UPPER_LOWER_INT_MASK
					(tmdev->tsens_addr);
		writel_relaxed(TSENS_TM_UPPER_LOWER_INT_DISABLE,
					int_mask_addr);
		writel_relaxed(TSENS_TM_CRITICAL_INT_EN |
			TSENS_TM_UPPER_INT_EN | TSENS_TM_LOWER_INT_EN,
			TSENS_TM_INT_EN(tmdev->tsens_addr));
	} else
		writel_relaxed(TSENS_INTERRUPT_EN,
			TSENS_UPPER_LOWER_INTERRUPT_CTRL(tmdev->tsens_addr));

	return 0;
}

static int get_device_tree_data(struct platform_device *pdev,
				struct tsens_tm_device *tmdev)
{
	struct device_node *of_node = pdev->dev.of_node;
	struct resource *res_mem = NULL;
	u32 *tsens_slope_data = NULL, *sensor_id, *client_id;
	u32 *temp1_calib_offset_factor, *temp2_calib_offset_factor;
	u32 rc = 0, i, tsens_num_sensors = 0;
	u32 cycle_monitor = 0, wd_bark = 0;
	const struct of_device_id *id;

	rc = of_property_read_u32(of_node,
			"qcom,sensors", &tsens_num_sensors);
	if (rc) {
		dev_err(&pdev->dev, "missing sensor number\n");
		return -ENODEV;
	}

	if (tsens_num_sensors == 0) {
		pr_err("No sensors?\n");
		return -ENODEV;
	}

	/* TSENS calibration region */
	tmdev->res_calib_mem = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "tsens_eeprom_physical");
	if (!tmdev->res_calib_mem) {
		pr_debug("Using controller programmed gain and offset\n");
		tmdev->gain_offset_programmed = true;
	} else {
		tsens_slope_data = devm_kzalloc(&pdev->dev,
			tsens_num_sensors * sizeof(u32), GFP_KERNEL);
		if (!tsens_slope_data)
			return -ENOMEM;

		rc = of_property_read_u32_array(of_node,
			"qcom,slope", tsens_slope_data, tsens_num_sensors);
		if (rc) {
			dev_err(&pdev->dev, "missing property: tsens-slope\n");
			return rc;
		};
	}

	if (!of_match_node(tsens_match, of_node)) {
		pr_err("Need to read SoC specific fuse map\n");
		return -ENODEV;
	}

	id = of_match_node(tsens_match, of_node);
	if (id == NULL) {
		pr_err("can not find tsens_match of_node\n");
		return -ENODEV;
	}

	if (!tmdev->gain_offset_programmed) {
		for (i = 0; i < tsens_num_sensors; i++)
			tmdev->sensor[i].slope_mul_tsens_factor =
							tsens_slope_data[i];
		tmdev->tsens_factor = TSENS_SLOPE_FACTOR;
	}

	tmdev->tsens_num_sensor = tsens_num_sensors;
	tmdev->calibration_less_mode = of_property_read_bool(of_node,
				"qcom,calibration-less-mode");
	tmdev->tsens_local_init = of_property_read_bool(of_node,
				"qcom,tsens-local-init");

	sensor_id = devm_kzalloc(&pdev->dev,
		tsens_num_sensors * sizeof(u32), GFP_KERNEL);
	if (!sensor_id)
		return -ENOMEM;

	client_id = devm_kzalloc(&pdev->dev,
		tsens_num_sensors * sizeof(u32), GFP_KERNEL);
	if (!client_id)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node,
		"qcom,client-id", client_id, tsens_num_sensors);
	if (rc) {
		for (i = 0; i < tsens_num_sensors; i++)
			tmdev->sensor[i].sensor_client_id = i;
		pr_debug("Default client id mapping\n");
	} else {
		for (i = 0; i < tsens_num_sensors; i++)
			tmdev->sensor[i].sensor_client_id = client_id[i];
		pr_debug("Use specified client id mapping\n");
	}

	rc = of_property_read_u32_array(of_node,
		"qcom,sensor-id", sensor_id, tsens_num_sensors);
	if (rc) {
		pr_debug("Default sensor id mapping\n");
		for (i = 0; i < tsens_num_sensors; i++) {
			tmdev->sensor[i].sensor_hw_num = i;
			tmdev->sensor[i].sensor_sw_id = i;
		}
	} else {
		pr_debug("Use specified sensor id mapping\n");
		for (i = 0; i < tsens_num_sensors; i++) {
			tmdev->sensor[i].sensor_hw_num = sensor_id[i];
			tmdev->sensor[i].sensor_sw_id = i;
		}
	}

	rc = of_property_read_u32(of_node,
			"qcom,cycle-monitor", &cycle_monitor);
	if (rc) {
		pr_debug("Default cycle completion monitor\n");
		tmdev->cycle_compltn_monitor = false;
	} else {
		pr_debug("Use specified cycle completion monitor\n");
		tmdev->cycle_compltn_monitor = true;
		tmdev->cycle_compltn_monitor_val = cycle_monitor;
	}

	rc = of_property_read_u32(of_node,
			"qcom,wd-bark", &wd_bark);
	if (rc) {
		pr_debug("Default Watchdog bark\n");
		tmdev->wd_bark = false;
	} else {
		pr_debug("Use specified Watchdog bark\n");
		tmdev->wd_bark = true;
		tmdev->wd_bark_val = wd_bark;
	}

	if (!strcmp(id->compatible, "qcom,msm8996-tsens") ||
		(!strcmp(id->compatible, "qcom,msm8998-tsens")))
		tmdev->tsens_type = TSENS_TYPE3;
	else if (!strcmp(id->compatible, "qcom,msmtitanium-tsens") ||
		(!strcmp(id->compatible, "qcom,sdm660-tsens")) ||
		(!strcmp(id->compatible, "qcom,sdm630-tsens")) ||
		(!strcmp(id->compatible, "qcom,msmhamster-tsens"))) {
		tmdev->tsens_type = TSENS_TYPE3;
		tsens_poll_check = 0;
	} else
		tmdev->tsens_type = TSENS_TYPE0;

	tmdev->tsens_valid_status_check = of_property_read_bool(of_node,
				"qcom,valid-status-check");
	if (!tmdev->tsens_valid_status_check) {
		if (!strcmp(id->compatible, "qcom,msm8996-tsens") ||
		(!strcmp(id->compatible, "qcom,msmtitanium-tsens")) ||
		(!strcmp(id->compatible, "qcom,msm8998-tsens")) ||
		(!strcmp(id->compatible, "qcom,sdm660-tsens")) ||
		(!strcmp(id->compatible, "qcom,sdm630-tsens")) ||
		(!strcmp(id->compatible, "qcom,msmhamster-tsens")))
			tmdev->tsens_valid_status_check = true;
	}

	tmdev->tsens_irq = platform_get_irq_byname(pdev,
					"tsens-upper-lower");
	if (tmdev->tsens_irq < 0) {
		pr_err("Invalid Upper/Lower get irq\n");
		rc = tmdev->tsens_irq;
		goto fail_tmdev;
	}

	if (!strcmp(id->compatible, "qcom,msm8996-tsens") ||
		(!strcmp(id->compatible, "qcom,msm8998-tsens")) ||
		(!strcmp(id->compatible, "qcom,msmhamster-tsens")) ||
		(!strcmp(id->compatible, "qcom,sdm660-tsens")) ||
		(!strcmp(id->compatible, "qcom,sdm630-tsens")) ||
		(!strcmp(id->compatible, "qcom,msmtitanium-tsens"))) {
		tmdev->tsens_critical_irq =
				platform_get_irq_byname(pdev,
						"tsens-critical");
		if (tmdev->tsens_critical_irq < 0) {
			pr_err("Invalid Critical get irq\n");
			rc = tmdev->tsens_critical_irq;
			goto fail_tmdev;
		}
	}

	temp1_calib_offset_factor = devm_kzalloc(&pdev->dev,
			tsens_num_sensors * sizeof(u32), GFP_KERNEL);
	if (!temp1_calib_offset_factor)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node,
				"qcom,temp1-offset", temp1_calib_offset_factor,
							tsens_num_sensors);
	if (rc) {
		pr_debug("Default temp1-offsets\n");
		for (i = 0; i < tsens_num_sensors; i++)
			tmdev->sensor[i].wa_temp1_calib_offset_factor = 0;
	} else {
		pr_debug("Use specific temp1-offsets\n");
		for (i = 0; i < tsens_num_sensors; i++)
			tmdev->sensor[i].wa_temp1_calib_offset_factor =
						temp1_calib_offset_factor[i];
	}

	temp2_calib_offset_factor = devm_kzalloc(&pdev->dev,
			tsens_num_sensors * sizeof(u32), GFP_KERNEL);
	if (!temp2_calib_offset_factor)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node,
				"qcom,temp2-offset", temp2_calib_offset_factor,
							tsens_num_sensors);
	if (rc) {
		pr_debug("Default temp2-offsets\n");
		for (i = 0; i < tsens_num_sensors; i++)
			tmdev->sensor[i].wa_temp2_calib_offset_factor = 0;
	} else {
		pr_debug("Use specific temp2-offsets\n");
		for (i = 0; i < tsens_num_sensors; i++)
			tmdev->sensor[i].wa_temp2_calib_offset_factor =
						temp2_calib_offset_factor[i];
	}

	/* TSENS register region */
	tmdev->res_tsens_mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "tsens_physical");
	if (!tmdev->res_tsens_mem) {
		pr_err("Could not get tsens physical address resource\n");
		rc = -EINVAL;
		goto fail_tmdev;
	}

	tmdev->tsens_len = tmdev->res_tsens_mem->end -
					tmdev->res_tsens_mem->start + 1;

	res_mem = request_mem_region(tmdev->res_tsens_mem->start,
				tmdev->tsens_len, tmdev->res_tsens_mem->name);
	if (!res_mem) {
		pr_err("Request tsens physical memory region failed\n");
		rc = -EINVAL;
		goto fail_tmdev;
	}

	tmdev->tsens_addr = ioremap(res_mem->start, tmdev->tsens_len);
	if (!tmdev->tsens_addr) {
		pr_err("Failed to IO map TSENS registers.\n");
		rc = -EINVAL;
		goto fail_unmap_tsens_region;
	}

	if (!tmdev->gain_offset_programmed) {
		tmdev->calib_len = tmdev->res_calib_mem->end -
					tmdev->res_calib_mem->start + 1;

		tmdev->tsens_calib_addr = ioremap(tmdev->res_calib_mem->start,
						tmdev->calib_len);
		if (!tmdev->tsens_calib_addr) {
			pr_err("Failed to IO map EEPROM registers.\n");
			rc = -EINVAL;
			goto fail_unmap_tsens;
		}
	}

	return 0;

fail_unmap_tsens:
	if (tmdev->tsens_addr)
		iounmap(tmdev->tsens_addr);
fail_unmap_tsens_region:
	if (tmdev->res_tsens_mem)
		release_mem_region(tmdev->res_tsens_mem->start,
					tmdev->tsens_len);
fail_tmdev:
	platform_set_drvdata(pdev, NULL);

	return rc;
}

static int tsens_tm_probe(struct platform_device *pdev)
{
	struct device_node *of_node = pdev->dev.of_node;
	int rc, i;
	u32 tsens_num_sensors;
	struct tsens_tm_device *tmdev = NULL;

	rc = of_property_read_u32(of_node,
			"qcom,sensors", &tsens_num_sensors);
	tmdev = devm_kzalloc(&pdev->dev,
			sizeof(struct tsens_tm_device) +
			tsens_num_sensors *
			sizeof(struct tsens_tm_device_sensor),
			GFP_KERNEL);
	if (tmdev == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	if (pdev->dev.of_node) {
		rc = get_device_tree_data(pdev, tmdev);
		if (rc) {
			pr_err("Error reading TSENS DT\n");
			return rc;
		}
	} else
		return -ENODEV;

	tmdev->pdev = pdev;

	tmdev->tsens_critical_wq = alloc_workqueue("tsens_critical_wq",
							WQ_HIGHPRI, 0);
	if (!tmdev->tsens_critical_wq) {
		rc = -ENOMEM;
		goto fail;
	}

	rc = tsens_hw_init(tmdev);
	if (rc)
		return rc;

	tmdev->prev_reading_avail = true;

	for (i = 0; i < 16; i++)
		tmdev->sensor_dbg_info[i].idx = 0;

	spin_lock_init(&tmdev->tsens_crit_lock);
	spin_lock_init(&tmdev->tsens_upp_low_lock);
	tmdev->is_ready = true;

	list_add_tail(&tmdev->list, &tsens_device_list);
	platform_set_drvdata(pdev, tmdev);

	rc = create_tsens_mtc_sysfs(pdev);
	if (rc < 0)
		pr_debug("Cannot create create_tsens_mtc_sysfs %d\n", rc);

	return 0;
fail:
	if (tmdev->tsens_critical_wq)
		destroy_workqueue(tmdev->tsens_critical_wq);
	if (tmdev->tsens_calib_addr)
		iounmap(tmdev->tsens_calib_addr);
	if (tmdev->tsens_addr)
		iounmap(tmdev->tsens_addr);
	if (tmdev->res_tsens_mem)
		release_mem_region(tmdev->res_tsens_mem->start,
			tmdev->tsens_len);
	platform_set_drvdata(pdev, NULL);

	return rc;
}

static ssize_t tsens_debugfs_read(struct file *file, char __user *ubuf,
				  size_t count, loff_t *ppos)
{
	int nbytes = 0;
	struct tsens_tm_device *tmdev = NULL;

	list_for_each_entry(tmdev, &tsens_device_list, list) {
		nbytes += scnprintf(dbg_buff + nbytes, 1024 - nbytes,
			"TSENS Critical count: %d\n",
			tmdev->tsens_critical_irq_cnt);
		nbytes += scnprintf(dbg_buff + nbytes, 1024 - nbytes,
			"TSENS Upper count: %d\n",
			tmdev->tsens_upper_irq_cnt);
		nbytes += scnprintf(dbg_buff + nbytes, 1024 - nbytes,
			"TSENS Lower count: %d\n",
			tmdev->tsens_lower_irq_cnt);

	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

const struct file_operations tsens_stats_ops = {
	.read = tsens_debugfs_read,
};

static void tsens_debugfs_init(void)
{
	const mode_t read_only_mode = S_IRUSR | S_IRGRP | S_IROTH;

	dent = debugfs_create_dir("tsens", 0);
	if (IS_ERR(dent)) {
		pr_err("Error creating TSENS directory\n");
		return;
	}

	dfile_stats = debugfs_create_file("stats", read_only_mode, dent,
					0, &tsens_stats_ops);
	if (!dfile_stats || IS_ERR(dfile_stats)) {
		pr_err("Failed to create TSENS folder\n");
		return;
	}
}

static int tsens_thermal_zone_register(struct tsens_tm_device *tmdev)
{
	int rc = 0, i = 0;

	if (tmdev == NULL) {
		pr_err("Invalid tsens instance\n");
		return -EINVAL;
	}

	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		char name[18];

		snprintf(name, sizeof(name), "tsens_tz_sensor%d",
			tmdev->sensor[i].sensor_client_id);
		tmdev->sensor[i].mode = THERMAL_DEVICE_ENABLED;
		tmdev->sensor[i].tm = tmdev;
		if (tmdev->tsens_type == TSENS_TYPE3) {
			tmdev->sensor[i].tz_dev = thermal_zone_device_register(
					name, TSENS_TRIP_NUM,
					TSENS_WRITABLE_TRIPS_MASK,
					&tmdev->sensor[i],
					&tsens_tm_thermal_zone_ops, NULL, 0, 0);
			if (IS_ERR(tmdev->sensor[i].tz_dev)) {
				pr_err("%s: failed.\n", __func__);
				rc = -ENODEV;
				goto fail;
			}
		} else {
			tmdev->sensor[i].tz_dev = thermal_zone_device_register(
					name, TSENS_TRIP_NUM,
					TSENS_WRITABLE_TRIPS_MASK,
					&tmdev->sensor[i],
					&tsens_thermal_zone_ops, NULL, 0, 0);
			if (IS_ERR(tmdev->sensor[i].tz_dev)) {
				pr_err("%s: failed.\n", __func__);
				rc = -ENODEV;
				goto fail;
			}
		}
	}

	if (tmdev->tsens_type == TSENS_TYPE3) {
		rc = request_threaded_irq(tmdev->tsens_irq, NULL,
				tsens_tm_irq_thread,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				"tsens_interrupt", tmdev);
		if (rc < 0) {
			pr_err("%s: request_irq FAIL: %d\n", __func__, rc);
			for (i = 0; i < tmdev->tsens_num_sensor; i++)
				thermal_zone_device_unregister(
					tmdev->sensor[i].tz_dev);
			goto fail;
		} else {
			enable_irq_wake(tmdev->tsens_irq);
		}

		rc = request_threaded_irq(tmdev->tsens_critical_irq, NULL,
			tsens_tm_critical_irq_thread,
			IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
			"tsens_critical_interrupt", tmdev);
		if (rc < 0) {
			pr_err("%s: request_irq FAIL: %d\n", __func__, rc);
			for (i = 0; i < tmdev->tsens_num_sensor; i++)
				thermal_zone_device_unregister(
					tmdev->sensor[i].tz_dev);
			goto fail;
		} else {
			enable_irq_wake(tmdev->tsens_critical_irq);
		}

		if (tsens_poll_check) {
			INIT_DEFERRABLE_WORK(&tmdev->tsens_critical_poll_test,
								tsens_poll);
			schedule_delayed_work(&tmdev->tsens_critical_poll_test,
				msecs_to_jiffies(tsens_sec_to_msec_value));
			init_completion(&tmdev->tsens_rslt_completion);
			tmdev->tsens_critical_poll = true;
		}
	} else {
		rc = request_threaded_irq(tmdev->tsens_irq, NULL,
			tsens_irq_thread, IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
			"tsens_interrupt", tmdev);
		if (rc < 0) {
			pr_err("%s: request_irq FAIL: %d\n", __func__, rc);
			for (i = 0; i < tmdev->tsens_num_sensor; i++)
				thermal_zone_device_unregister(
					tmdev->sensor[i].tz_dev);
			goto fail;
		} else {
			enable_irq_wake(tmdev->tsens_irq);
		}
	}

	return 0;
fail:
	if (tmdev->tsens_calib_addr)
		iounmap(tmdev->tsens_calib_addr);
	if (tmdev->tsens_addr)
		iounmap(tmdev->tsens_addr);
	if (tmdev->res_tsens_mem)
		release_mem_region(tmdev->res_tsens_mem->start,
			tmdev->tsens_len);
	return rc;
}

static int _tsens_register_thermal(void)
{
	struct tsens_tm_device *tmdev = NULL;
	int rc;

	if (tsens_is_ready() <= 0) {
		pr_err("%s: TSENS early init not done\n", __func__);
		return -ENODEV;
	}

	list_for_each_entry(tmdev, &tsens_device_list, list) {
		rc = tsens_thermal_zone_register(tmdev);
		if (rc) {
			pr_err("Error registering the thermal zone\n");
			return rc;
		}
	}

	tsens_debugfs_init();

	return 0;
}

static int tsens_tm_remove(struct platform_device *pdev)
{
	struct tsens_tm_device *tmdev = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < tmdev->tsens_num_sensor; i++)
		thermal_zone_device_unregister(tmdev->sensor[i].tz_dev);
	if (tmdev->tsens_calib_addr)
		iounmap(tmdev->tsens_calib_addr);
	if (tmdev->tsens_addr)
		iounmap(tmdev->tsens_addr);
	if (tmdev->res_tsens_mem)
		release_mem_region(tmdev->res_tsens_mem->start,
			tmdev->tsens_len);
	if (tmdev->tsens_critical_wq)
		destroy_workqueue(tmdev->tsens_critical_wq);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver tsens_tm_driver = {
	.probe = tsens_tm_probe,
	.remove = tsens_tm_remove,
	.driver = {
		.name = "msm-tsens",
		.owner = THIS_MODULE,
		.of_match_table = tsens_match,
	},
};

int __init tsens_tm_init_driver(void)
{
	return platform_driver_register(&tsens_tm_driver);
}
arch_initcall(tsens_tm_init_driver);

static int __init tsens_thermal_register(void)
{
	return _tsens_register_thermal();
}
module_init(tsens_thermal_register);

static void __exit _tsens_tm_remove(void)
{
	platform_driver_unregister(&tsens_tm_driver);
}
module_exit(_tsens_tm_remove);

MODULE_ALIAS("platform:" TSENS_DRIVER_NAME);
MODULE_LICENSE("GPL v2");
