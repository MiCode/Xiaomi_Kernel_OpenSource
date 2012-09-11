/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/msm_tsens.h>
#include <linux/err.h>
#include <linux/of.h>

#include <mach/msm_iomap.h>

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

#define TSENS_CTRL_ADDR(n)		(n)
#define TSENS_SW_RST			BIT(1)
#define TSENS_SN_MIN_MAX_STATUS_CTRL(n)	((n) + 4)
#define TSENS_GLOBAL_CONFIG(n)		((n) + 0x34)
#define TSENS_S0_MAIN_CONFIG(n)		((n) + 0x38)
#define TSENS_SN_REMOTE_CONFIG(n)	((n) + 0x3c)

/* TSENS calibration Mask data */
#define TSENS_BASE1_MASK		0xff
#define TSENS0_POINT1_MASK		0x3f00
#define TSENS1_POINT1_MASK		0xfc000
#define TSENS2_POINT1_MASK		0x3f00000
#define TSENS3_POINT1_MASK		0xfc000000
#define TSENS4_POINT1_MASK		0x3f
#define TSENS5_POINT1_MASK		0xfc0
#define TSENS6_POINT1_MASK		0x3f000
#define TSENS7_POINT1_MASK		0xfc0000
#define TSENS8_POINT1_MASK		0x3f000000
#define TSENS9_POINT1_MASK		0x3f
#define TSENS10_POINT1_MASK		0xfc00
#define TSENS_CAL_SEL_0_1		0xc0000000
#define TSENS_CAL_SEL_2			0x40000000
#define TSENS_CAL_SEL_SHIFT		30
#define TSENS_CAL_SEL_SHIFT_2		28
#define TSENS_ONE_POINT_CALIB		0x1
#define TSENS_ONE_POINT_CALIB_OPTION_2	0x2
#define TSENS_TWO_POINT_CALIB		0x3

#define TSENS0_POINT1_SHIFT		8
#define TSENS1_POINT1_SHIFT		14
#define TSENS2_POINT1_SHIFT		20
#define TSENS3_POINT1_SHIFT		26
#define TSENS5_POINT1_SHIFT		6
#define TSENS6_POINT1_SHIFT		12
#define TSENS7_POINT1_SHIFT		18
#define TSENS8_POINT1_SHIFT		24
#define TSENS10_POINT1_SHIFT		6

#define TSENS_POINT2_BASE_SHIFT		12
#define TSENS0_POINT2_SHIFT		20
#define TSENS1_POINT2_SHIFT		26
#define TSENS3_POINT2_SHIFT		6
#define TSENS4_POINT2_SHIFT		12
#define TSENS5_POINT2_SHIFT		18
#define TSENS6_POINT2_SHIFT		24
#define TSENS8_POINT2_SHIFT		6
#define TSENS9_POINT2_SHIFT		12
#define TSENS10_POINT2_SHIFT		18

#define TSENS_BASE2_MASK		0xff000
#define TSENS0_POINT2_MASK		0x3f00000
#define TSENS1_POINT2_MASK		0xfc000000
#define TSENS2_POINT2_MASK		0x3f
#define TSENS3_POINT2_MASK		0xfc00
#define TSENS4_POINT2_MASK		0x3f000
#define TSENS5_POINT2_MASK		0xfc0000
#define TSENS6_POINT2_MASK		0x3f000000
#define TSENS7_POINT2_MASK		0x3f
#define TSENS8_POINT2_MASK		0xfc00
#define TSENS9_POINT2_MASK		0x3f000
#define TSENS10_POINT2_MASK		0xfc0000

#define TSENS_BIT_APPEND		0x3
#define TSENS_CAL_DEGC_POINT1		30
#define TSENS_CAL_DEGC_POINT2		120
#define TSENS_SLOPE_FACTOR		1000

/* TSENS register data */
#define TSENS_TRDY_RDY_MIN_TIME		2000
#define TSENS_TRDY_RDY_MAX_TIME		2100
#define TSENS_THRESHOLD_MAX_CODE	0x3ff
#define TSENS_THRESHOLD_MIN_CODE	0x0

#define TSENS_CTRL_INIT_DATA1		0x1cfff9
#define TSENS_GLOBAL_INIT_DATA		0x302f16c
#define TSENS_S0_MAIN_CFG_INIT_DATA	0x1c3
#define TSENS_SN_MIN_MAX_STATUS_CTRL_DATA	0x3ffc00
#define TSENS_SN_REMOTE_CFG_DATA	0x11c3

/* Trips: warm and cool */
enum tsens_trip_type {
	TSENS_TRIP_WARM = 0,
	TSENS_TRIP_COOL,
	TSENS_TRIP_NUM,
};

struct tsens_tm_device_sensor {
	struct thermal_zone_device	*tz_dev;
	enum thermal_device_mode	mode;
	unsigned int			sensor_num;
	struct work_struct		work;
	int				offset;
	int				calib_data_point1;
	int				calib_data_point2;
	uint32_t			slope_mul_tsens_factor;
};

struct tsens_tm_device {
	struct platform_device		*pdev;
	bool				prev_reading_avail;
	int				tsens_factor;
	uint32_t			tsens_num_sensor;
	int				tsens_irq;
	void				*tsens_addr;
	void				*tsens_calib_addr;
	int				tsens_len;
	int				calib_len;
	struct resource			*res_tsens_mem;
	struct resource			*res_calib_mem;
	struct work_struct		tsens_work;
	struct tsens_tm_device_sensor	sensor[0];
};

struct tsens_tm_device *tmdev;

static int tsens_tz_code_to_degc(int adc_code, int sensor_num)
{
	int degcbeforefactor, degc;
	degcbeforefactor = ((adc_code * tmdev->tsens_factor) -
				tmdev->sensor[sensor_num].offset)/
			tmdev->sensor[sensor_num].slope_mul_tsens_factor;

	if (degcbeforefactor == 0)
		degc = degcbeforefactor;
	else if (degcbeforefactor > 0)
		degc = ((degcbeforefactor * tmdev->tsens_factor) +
				tmdev->tsens_factor/2)/tmdev->tsens_factor;
	else
		degc = ((degcbeforefactor * tmdev->tsens_factor) -
				tmdev->tsens_factor/2)/tmdev->tsens_factor;

	return degc;
}

static int tsens_tz_degc_to_code(int degc, int sensor_num)
{
	int code = ((degc * tmdev->sensor[sensor_num].slope_mul_tsens_factor)
		+ tmdev->sensor[sensor_num].offset)/tmdev->tsens_factor;

	if (code > TSENS_THRESHOLD_MAX_CODE)
		code = TSENS_THRESHOLD_MAX_CODE;
	else if (code < TSENS_THRESHOLD_MIN_CODE)
		code = TSENS_THRESHOLD_MIN_CODE;
	return code;
}

static void msm_tsens_get_temp(int sensor_num, unsigned long *temp)
{
	unsigned int code, sensor_addr;

	if (!tmdev->prev_reading_avail) {
		while (!(readl_relaxed(TSENS_TRDY_ADDR(tmdev->tsens_addr))
					& TSENS_TRDY_MASK))
			usleep_range(TSENS_TRDY_RDY_MIN_TIME,
				TSENS_TRDY_RDY_MAX_TIME);
		tmdev->prev_reading_avail = true;
	}

	sensor_addr =
		(unsigned int)TSENS_S0_STATUS_ADDR(tmdev->tsens_addr);
	code = readl_relaxed(sensor_addr +
			(sensor_num << TSENS_STATUS_ADDR_OFFSET));
	*temp = tsens_tz_code_to_degc((code & TSENS_SN_STATUS_TEMP_MASK),
								sensor_num);
}

static int tsens_tz_get_temp(struct thermal_zone_device *thermal,
			     unsigned long *temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;

	if (!tm_sensor || tm_sensor->mode != THERMAL_DEVICE_ENABLED || !temp)
		return -EINVAL;

	msm_tsens_get_temp(tm_sensor->sensor_num, temp);

	return 0;
}

int tsens_get_temp(struct tsens_device *device, unsigned long *temp)
{
	if (!tmdev)
		return -ENODEV;

	msm_tsens_get_temp(device->sensor_num, temp);

	return 0;
}
EXPORT_SYMBOL(tsens_get_temp);

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

static int tsens_tz_activate_trip_type(struct thermal_zone_device *thermal,
			int trip, enum thermal_trip_activation_mode mode)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg_cntl, code, hi_code, lo_code, mask;

	if (!tm_sensor || trip < 0)
		return -EINVAL;

	lo_code = TSENS_THRESHOLD_MIN_CODE;
	hi_code = TSENS_THRESHOLD_MAX_CODE;

	reg_cntl = readl_relaxed((TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
				(tmdev->tsens_addr) +
				(tm_sensor->sensor_num * 4)));
	switch (trip) {
	case TSENS_TRIP_WARM:
		code = (reg_cntl & TSENS_UPPER_THRESHOLD_MASK)
					>> TSENS_UPPER_THRESHOLD_SHIFT;
		mask = TSENS_UPPER_STATUS_CLR;

		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_cntl & TSENS_LOWER_THRESHOLD_MASK);
		break;
	case TSENS_TRIP_COOL:
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
			(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
					(tmdev->tsens_addr) +
					(tm_sensor->sensor_num * 4)));
	else {
		if (code < lo_code || code > hi_code) {
			pr_err("%s with invalid code %x\n", __func__, code);
			return -EINVAL;
		}
		writel_relaxed(reg_cntl & ~mask,
		(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(tmdev->tsens_addr) +
		(tm_sensor->sensor_num * 4)));
	}
	mb();
	return 0;
}

static int tsens_tz_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip, unsigned long *temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg;

	if (!tm_sensor || trip < 0 || !temp)
		return -EINVAL;

	reg = readl_relaxed(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
						(tmdev->tsens_addr) +
			(tm_sensor->sensor_num * TSENS_SN_ADDR_OFFSET));
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

	*temp = tsens_tz_code_to_degc(reg, tm_sensor->sensor_num);

	return 0;
}

static int tsens_tz_notify(struct thermal_zone_device *thermal,
				int count, enum thermal_trip_type type)
{
	/* TSENS driver does not shutdown the device.
	   All Thermal notification are sent to the
	   thermal daemon to take appropriate action */
	pr_debug("%s debug\n", __func__);
	return 1;
}

static int tsens_tz_set_trip_temp(struct thermal_zone_device *thermal,
				   int trip, long temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg_cntl;
	int code, hi_code, lo_code, code_err_chk;

	code_err_chk = code = tsens_tz_degc_to_code(temp,
					tm_sensor->sensor_num);
	if (!tm_sensor || trip < 0)
		return -EINVAL;

	lo_code = TSENS_THRESHOLD_MIN_CODE;
	hi_code = TSENS_THRESHOLD_MAX_CODE;

	reg_cntl = readl_relaxed(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
				(tmdev->tsens_addr) +
				(tm_sensor->sensor_num * TSENS_SN_ADDR_OFFSET));
	switch (trip) {
	case TSENS_TRIP_WARM:
		code <<= TSENS_UPPER_THRESHOLD_SHIFT;
		reg_cntl &= ~TSENS_UPPER_THRESHOLD_MASK;
		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_cntl & TSENS_LOWER_THRESHOLD_MASK);
		break;
	case TSENS_TRIP_COOL:
		reg_cntl &= ~TSENS_LOWER_THRESHOLD_MASK;
		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_cntl & TSENS_UPPER_THRESHOLD_MASK)
					>> TSENS_UPPER_THRESHOLD_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	if (code_err_chk < lo_code || code_err_chk > hi_code)
		return -EINVAL;

	writel_relaxed(reg_cntl | code, (TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
					(tmdev->tsens_addr) +
					(tm_sensor->sensor_num *
					TSENS_SN_ADDR_OFFSET)));
	mb();
	return 0;
}

static struct thermal_zone_device_ops tsens_thermal_zone_ops = {
	.get_temp = tsens_tz_get_temp,
	.get_mode = tsens_tz_get_mode,
	.get_trip_type = tsens_tz_get_trip_type,
	.activate_trip_type = tsens_tz_activate_trip_type,
	.get_trip_temp = tsens_tz_get_trip_temp,
	.set_trip_temp = tsens_tz_set_trip_temp,
	.notify = tsens_tz_notify,
};

static void notify_uspace_tsens_fn(struct work_struct *work)
{
	struct tsens_tm_device_sensor *tm = container_of(work,
		struct tsens_tm_device_sensor, work);

	sysfs_notify(&tm->tz_dev->device.kobj,
					NULL, "type");
}

static void tsens_scheduler_fn(struct work_struct *work)
{
	struct tsens_tm_device *tm = container_of(work, struct tsens_tm_device,
						tsens_work);
	unsigned int i, status, threshold;
	unsigned int sensor_status_addr, sensor_status_ctrl_addr;

	sensor_status_addr =
		(unsigned int)TSENS_S0_STATUS_ADDR(tmdev->tsens_addr);
	sensor_status_ctrl_addr =
		(unsigned int)TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
		(tmdev->tsens_addr);
	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		bool upper_thr = false, lower_thr = false;
		status = readl_relaxed(sensor_status_addr);
		threshold = readl_relaxed(sensor_status_ctrl_addr);
		if (status & TSENS_SN_STATUS_UPPER_STATUS) {
			writel_relaxed(threshold | TSENS_UPPER_STATUS_CLR,
				sensor_status_ctrl_addr);
			upper_thr = true;
		}
		if (status & TSENS_SN_STATUS_LOWER_STATUS) {
			writel_relaxed(threshold | TSENS_LOWER_STATUS_CLR,
				sensor_status_ctrl_addr);
			lower_thr = true;
		}
		if (upper_thr || lower_thr) {
			/* Notify user space */
			schedule_work(&tm->sensor[i].work);
			pr_debug("sensor:%d trigger temp (%d degC)\n", i,
				tsens_tz_code_to_degc((status &
				TSENS_SN_STATUS_TEMP_MASK), i));
		}
		sensor_status_addr += TSENS_SN_ADDR_OFFSET;
		sensor_status_ctrl_addr += TSENS_SN_ADDR_OFFSET;
	}
	mb();
}

static irqreturn_t tsens_isr(int irq, void *data)
{
	schedule_work(&tmdev->tsens_work);

	return IRQ_HANDLED;
}

static void tsens_hw_init(void)
{
	unsigned int reg_cntl = 0;
	unsigned int i;

	reg_cntl = readl_relaxed(TSENS_CTRL_ADDR(tmdev->tsens_addr));
	writel_relaxed(reg_cntl | TSENS_SW_RST,
			TSENS_CTRL_ADDR(tmdev->tsens_addr));
	writel_relaxed(TSENS_CTRL_INIT_DATA1,
			TSENS_CTRL_ADDR(tmdev->tsens_addr));
	writel_relaxed(TSENS_GLOBAL_INIT_DATA,
			TSENS_GLOBAL_CONFIG(tmdev->tsens_addr));
	writel_relaxed(TSENS_S0_MAIN_CFG_INIT_DATA,
			TSENS_S0_MAIN_CONFIG(tmdev->tsens_addr));
	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		writel_relaxed(TSENS_SN_MIN_MAX_STATUS_CTRL_DATA,
			TSENS_SN_MIN_MAX_STATUS_CTRL(tmdev->tsens_addr)
				+ (i * TSENS_SN_ADDR_OFFSET));
		writel_relaxed(TSENS_SN_REMOTE_CFG_DATA,
			TSENS_SN_REMOTE_CONFIG(tmdev->tsens_addr)
				+ (i * TSENS_SN_ADDR_OFFSET));
	}
	writel_relaxed(TSENS_INTERRUPT_EN,
		TSENS_UPPER_LOWER_INTERRUPT_CTRL(tmdev->tsens_addr));
}

static int tsens_calib_sensors(void)
{
	int i, tsens_base1_data = 0, tsens0_point1 = 0, tsens1_point1 = 0;
	int tsens2_point1 = 0, tsens3_point1 = 0, tsens4_point1 = 0;
	int tsens5_point1 = 0, tsens6_point1 = 0, tsens7_point1 = 0;
	int tsens8_point1 = 0, tsens9_point1 = 0, tsens10_point1 = 0;
	int tsens0_point2 = 0, tsens1_point2 = 0, tsens2_point2 = 0;
	int tsens3_point2 = 0, tsens4_point2 = 0, tsens5_point2 = 0;
	int tsens6_point2 = 0, tsens7_point2 = 0, tsens8_point2 = 0;
	int tsens9_point2 = 0, tsens10_point2 = 0;
	int tsens_base2_data = 0, tsens_calibration_mode = 0, temp = 0;
	uint32_t calib_data[5];

	for (i = 0; i < 5; i++)
		calib_data[i] = readl_relaxed(tmdev->tsens_calib_addr
					+ (i * TSENS_SN_ADDR_OFFSET));

	tsens_calibration_mode = (calib_data[1] & TSENS_CAL_SEL_0_1)
			>> TSENS_CAL_SEL_SHIFT;
	temp = (calib_data[3] & TSENS_CAL_SEL_2)
			>> TSENS_CAL_SEL_SHIFT_2;
	tsens_calibration_mode |= temp;

	if (tsens_calibration_mode == 0) {
		pr_debug("TSENS is calibrationless mode\n");
		for (i = 0; i < tmdev->tsens_num_sensor; i++) {
			tmdev->sensor[i].calib_data_point2 = 780;
			tmdev->sensor[i].calib_data_point1 = 492;
		}
		goto compute_intercept_slope;
	} else if ((tsens_calibration_mode == TSENS_ONE_POINT_CALIB) ||
			(tsens_calibration_mode == TSENS_TWO_POINT_CALIB)) {
		tsens_base1_data = (calib_data[0] & TSENS_BASE1_MASK);
		tsens0_point1 = (calib_data[0] & TSENS0_POINT1_MASK) >>
							TSENS0_POINT1_SHIFT;
		tsens1_point1 = (calib_data[0] & TSENS1_POINT1_MASK) >>
							TSENS1_POINT1_SHIFT;
		tsens2_point1 = (calib_data[0] & TSENS2_POINT1_MASK) >>
							TSENS2_POINT1_SHIFT;
		tsens3_point1 = (calib_data[0] & TSENS3_POINT1_MASK) >>
							TSENS3_POINT1_SHIFT;
		tsens4_point1 = (calib_data[1] & TSENS4_POINT1_MASK);
		tsens5_point1 = (calib_data[1] & TSENS5_POINT1_MASK) >>
							TSENS5_POINT1_SHIFT;
		tsens6_point1 = (calib_data[1] & TSENS6_POINT1_MASK) >>
							TSENS6_POINT1_SHIFT;
		tsens7_point1 = (calib_data[1] & TSENS7_POINT1_MASK) >>
							TSENS7_POINT1_SHIFT;
		tsens8_point1 = (calib_data[1] & TSENS8_POINT1_MASK) >>
							TSENS8_POINT1_SHIFT;
		tsens9_point1 = (calib_data[2] & TSENS9_POINT1_MASK);
		tsens10_point1 = (calib_data[2] & TSENS10_POINT1_MASK) >>
							TSENS10_POINT1_SHIFT;
	} else if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		tsens_base2_data = (calib_data[2] & TSENS_BASE2_MASK) >>
						TSENS_POINT2_BASE_SHIFT;
		tsens0_point2 = (calib_data[2] & TSENS0_POINT2_MASK) >>
						TSENS0_POINT2_SHIFT;
		tsens1_point2 = (calib_data[2] & TSENS1_POINT2_MASK) >>
						TSENS1_POINT2_SHIFT;
		tsens2_point2 = (calib_data[3] & TSENS2_POINT2_MASK);
		tsens3_point2 = (calib_data[3] & TSENS3_POINT2_MASK) >>
						TSENS3_POINT2_SHIFT;
		tsens4_point2 = (calib_data[3] & TSENS4_POINT2_MASK) >>
						TSENS4_POINT2_SHIFT;
		tsens5_point2 = (calib_data[3] & TSENS5_POINT2_MASK) >>
						TSENS5_POINT2_SHIFT;
		tsens6_point2 = (calib_data[3] & TSENS6_POINT2_MASK) >>
						TSENS6_POINT2_SHIFT;
		tsens7_point2 = (calib_data[4] & TSENS7_POINT2_MASK);
		tsens8_point2 = (calib_data[4] & TSENS8_POINT2_MASK) >>
						TSENS8_POINT2_SHIFT;
		tsens9_point2 = (calib_data[4] & TSENS9_POINT2_MASK) >>
						TSENS9_POINT2_SHIFT;
		tsens10_point2 = (calib_data[4] & TSENS10_POINT2_MASK) >>
						TSENS10_POINT2_SHIFT;
	} else {
		pr_debug("Calibration mode is unknown: %d\n",
						tsens_calibration_mode);
		return -ENODEV;
	}

	if (tsens_calibration_mode == TSENS_ONE_POINT_CALIB) {
		tmdev->sensor[0].calib_data_point1 =
		(((tsens_base1_data) << 2) | TSENS_BIT_APPEND) + tsens0_point1;
		tmdev->sensor[1].calib_data_point1 =
		(((tsens_base1_data) << 2) | TSENS_BIT_APPEND) + tsens1_point1;
		tmdev->sensor[2].calib_data_point1 =
		(((tsens_base1_data) << 2) | TSENS_BIT_APPEND) + tsens2_point1;
		tmdev->sensor[3].calib_data_point1 =
		(((tsens_base1_data) << 2) | TSENS_BIT_APPEND) + tsens3_point1;
		tmdev->sensor[4].calib_data_point1 =
		(((tsens_base1_data) << 2) | TSENS_BIT_APPEND) + tsens4_point1;
		tmdev->sensor[5].calib_data_point1 =
		(((tsens_base1_data) << 2) | TSENS_BIT_APPEND) + tsens5_point1;
		tmdev->sensor[6].calib_data_point1 =
		(((tsens_base1_data) << 2) | TSENS_BIT_APPEND) + tsens6_point1;
		tmdev->sensor[7].calib_data_point1 =
		(((tsens_base1_data) << 2) | TSENS_BIT_APPEND) + tsens7_point1;
		tmdev->sensor[8].calib_data_point1 =
		(((tsens_base1_data) << 2) | TSENS_BIT_APPEND) + tsens8_point1;
		tmdev->sensor[9].calib_data_point1 =
		(((tsens_base1_data) << 2) | TSENS_BIT_APPEND) + tsens9_point1;
		tmdev->sensor[10].calib_data_point1 =
		(((tsens_base1_data) << 2) | TSENS_BIT_APPEND) + tsens10_point1;
	}

	if ((tsens_calibration_mode == TSENS_ONE_POINT_CALIB_OPTION_2) ||
			(tsens_calibration_mode == TSENS_TWO_POINT_CALIB)) {
		tmdev->sensor[0].calib_data_point1 =
		((((tsens_base1_data) + tsens0_point1) << 2) |
						TSENS_BIT_APPEND);
		tmdev->sensor[1].calib_data_point1 =
		((((tsens_base1_data) + tsens1_point1) << 2) |
						TSENS_BIT_APPEND);
		tmdev->sensor[2].calib_data_point1 =
		((((tsens_base1_data) + tsens2_point1) << 2) |
						TSENS_BIT_APPEND);
		tmdev->sensor[3].calib_data_point1 =
		((((tsens_base1_data) + tsens3_point1) << 2) |
						TSENS_BIT_APPEND);
		tmdev->sensor[4].calib_data_point1 =
		((((tsens_base1_data) + tsens4_point1) << 2) |
						TSENS_BIT_APPEND);
		tmdev->sensor[5].calib_data_point1 =
		((((tsens_base1_data) + tsens5_point1) << 2) |
						TSENS_BIT_APPEND);
		tmdev->sensor[6].calib_data_point1 =
		((((tsens_base1_data) + tsens6_point1) << 2) |
						TSENS_BIT_APPEND);
		tmdev->sensor[7].calib_data_point1 =
		((((tsens_base1_data) + tsens7_point1) << 2) |
						TSENS_BIT_APPEND);
		tmdev->sensor[8].calib_data_point1 =
		((((tsens_base1_data) + tsens8_point1) << 2) |
						TSENS_BIT_APPEND);
		tmdev->sensor[9].calib_data_point1 =
		((((tsens_base1_data) + tsens9_point1) << 2) |
						TSENS_BIT_APPEND);
		tmdev->sensor[10].calib_data_point1 =
		((((tsens_base1_data) + tsens10_point1) << 2) |
						TSENS_BIT_APPEND);
	}

	if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		tmdev->sensor[0].calib_data_point2 =
		(((tsens_base2_data + tsens0_point2) << 2) | TSENS_BIT_APPEND);
		tmdev->sensor[1].calib_data_point2 =
		(((tsens_base2_data + tsens1_point2) << 2) | TSENS_BIT_APPEND);
		tmdev->sensor[2].calib_data_point2 =
		(((tsens_base2_data + tsens2_point2) << 2) | TSENS_BIT_APPEND);
		tmdev->sensor[3].calib_data_point2 =
		(((tsens_base2_data + tsens3_point2) << 2) | TSENS_BIT_APPEND);
		tmdev->sensor[4].calib_data_point2 =
		(((tsens_base2_data + tsens4_point2) << 2) | TSENS_BIT_APPEND);
		tmdev->sensor[5].calib_data_point2 =
		(((tsens_base2_data + tsens5_point2) << 2) | TSENS_BIT_APPEND);
		tmdev->sensor[6].calib_data_point2 =
		(((tsens_base2_data + tsens6_point2) << 2) | TSENS_BIT_APPEND);
		tmdev->sensor[7].calib_data_point2 =
		(((tsens_base2_data + tsens7_point2) << 2) | TSENS_BIT_APPEND);
		tmdev->sensor[8].calib_data_point2 =
		(((tsens_base2_data + tsens8_point2) << 2) | TSENS_BIT_APPEND);
		tmdev->sensor[9].calib_data_point2 =
		(((tsens_base2_data + tsens9_point2) << 2) | TSENS_BIT_APPEND);
		tmdev->sensor[10].calib_data_point2 =
		(((tsens_base2_data + tsens10_point2) << 2) | TSENS_BIT_APPEND);
	}

compute_intercept_slope:
	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		int32_t num = 0, den = 0;
		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			num = TSENS_CAL_DEGC_POINT2 - TSENS_CAL_DEGC_POINT2;
			den = tmdev->sensor[i].calib_data_point2 -
					tmdev->sensor[i].calib_data_point1;
			num *= tmdev->tsens_factor;
			tmdev->sensor[i].slope_mul_tsens_factor = num/den;
		}
		tmdev->sensor[i].offset = (tmdev->sensor[i].calib_data_point1 *
			tmdev->tsens_factor) - (TSENS_CAL_DEGC_POINT1 *
				tmdev->sensor[i].slope_mul_tsens_factor);
		INIT_WORK(&tmdev->sensor[i].work, notify_uspace_tsens_fn);
		tmdev->prev_reading_avail = false;
	}

	return 0;
}

static int get_device_tree_data(struct platform_device *pdev)
{
	const struct device_node *of_node = pdev->dev.of_node;
	struct resource *res_mem = NULL;
	u32 *tsens_slope_data;
	u32 rc = 0, i, tsens_num_sensors;

	rc = of_property_read_u32(of_node,
			"qcom,sensors", &tsens_num_sensors);
	if (rc) {
		dev_err(&pdev->dev, "missing sensor number\n");
		return -ENODEV;
	}

	tsens_slope_data = devm_kzalloc(&pdev->dev,
				tsens_num_sensors, GFP_KERNEL);
	if (!tsens_slope_data) {
		dev_err(&pdev->dev, "can not allocate slope data\n");
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(of_node,
		"qcom,slope", tsens_slope_data, tsens_num_sensors);
	if (rc) {
		dev_err(&pdev->dev, "invalid or missing property: tsens-slope\n");
		return rc;
	};

	tmdev = devm_kzalloc(&pdev->dev,
			sizeof(struct tsens_tm_device) +
			tsens_num_sensors *
			sizeof(struct tsens_tm_device_sensor),
			GFP_KERNEL);
	if (tmdev == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < tsens_num_sensors; i++)
		tmdev->sensor[i].slope_mul_tsens_factor = tsens_slope_data[i];
	tmdev->tsens_factor = TSENS_SLOPE_FACTOR;
	tmdev->tsens_num_sensor = tsens_num_sensors;

	tmdev->tsens_irq = platform_get_irq(pdev, 0);
	if (tmdev->tsens_irq < 0) {
		pr_err("Invalid get irq\n");
		return tmdev->tsens_irq;
	}

	tmdev->res_tsens_mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "tsens_physical");
	if (!tmdev->res_tsens_mem) {
		pr_err("Could not get tsens physical address resource\n");
		rc = -EINVAL;
		goto fail_free_irq;
	}

	tmdev->tsens_len = tmdev->res_tsens_mem->end -
					tmdev->res_tsens_mem->start + 1;

	res_mem = request_mem_region(tmdev->res_tsens_mem->start,
				tmdev->tsens_len, tmdev->res_tsens_mem->name);
	if (!res_mem) {
		pr_err("Request tsens physical memory region failed\n");
		rc = -EINVAL;
		goto fail_free_irq;
	}

	tmdev->tsens_addr = ioremap(res_mem->start, tmdev->tsens_len);
	if (!tmdev->tsens_addr) {
		pr_err("Failed to IO map TSENS registers.\n");
		rc = -EINVAL;
		goto fail_unmap_tsens_region;
	}

	tmdev->res_calib_mem = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "tsens_eeprom_physical");
	if (!tmdev->res_calib_mem) {
		pr_err("Could not get qfprom physical address resource\n");
		rc = -EINVAL;
		goto fail_unmap_tsens;
	}

	tmdev->calib_len = tmdev->res_calib_mem->end -
					tmdev->res_calib_mem->start + 1;

	res_mem = request_mem_region(tmdev->res_calib_mem->start,
				tmdev->calib_len, tmdev->res_calib_mem->name);
	if (!res_mem) {
		pr_err("Request calibration memory region failed\n");
		rc = -EINVAL;
		goto fail_unmap_tsens;
	}

	tmdev->tsens_calib_addr = ioremap(res_mem->start,
						tmdev->calib_len);
	if (!tmdev->tsens_calib_addr) {
		pr_err("Failed to IO map EEPROM registers.\n");
		rc = -EINVAL;
		goto fail_unmap_calib_region;
	}

	return 0;

fail_unmap_calib_region:
	if (tmdev->res_calib_mem)
		release_mem_region(tmdev->res_calib_mem->start,
					tmdev->calib_len);
fail_unmap_tsens:
	if (tmdev->tsens_addr)
		iounmap(tmdev->tsens_addr);
fail_unmap_tsens_region:
	if (tmdev->res_tsens_mem)
		release_mem_region(tmdev->res_tsens_mem->start,
					tmdev->tsens_len);
fail_free_irq:
	free_irq(tmdev->tsens_irq, tmdev);

	return rc;
}

static int __devinit tsens_tm_probe(struct platform_device *pdev)
{
	int rc;

	if (tmdev) {
		pr_err("TSENS device already in use\n");
		return -EBUSY;
	}

	if (pdev->dev.of_node)
		rc = get_device_tree_data(pdev);
	else
		return -ENODEV;

	tmdev->pdev = pdev;
	rc = tsens_calib_sensors();
	if (rc < 0)
		goto fail;

	tsens_hw_init();

	tmdev->prev_reading_avail = true;

	platform_set_drvdata(pdev, tmdev);

	return 0;
fail:
	if (tmdev->tsens_calib_addr)
		iounmap(tmdev->tsens_calib_addr);
	if (tmdev->res_calib_mem)
		release_mem_region(tmdev->res_calib_mem->start,
					tmdev->calib_len);
	if (tmdev->tsens_addr)
		iounmap(tmdev->tsens_addr);
	if (tmdev->res_tsens_mem)
		release_mem_region(tmdev->res_tsens_mem->start,
			tmdev->tsens_len);
	free_irq(tmdev->tsens_irq, tmdev);
	kfree(tmdev);

	return rc;
}

static int __devinit _tsens_register_thermal(void)
{
	struct platform_device *pdev;
	int rc, i;

	if (!tmdev) {
		pr_err("%s: TSENS early init not done\n", __func__);
		return -ENODEV;
	}

	pdev = tmdev->pdev;

	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		char name[18];
		snprintf(name, sizeof(name), "tsens_tz_sensor%d", i);
		tmdev->sensor[i].mode = THERMAL_DEVICE_ENABLED;
		tmdev->sensor[i].sensor_num = i;
		tmdev->sensor[i].tz_dev = thermal_zone_device_register(name,
				TSENS_TRIP_NUM, &tmdev->sensor[i],
				&tsens_thermal_zone_ops, 0, 0, 0, 0);
		if (IS_ERR(tmdev->sensor[i].tz_dev)) {
			pr_err("%s: thermal_zone_device_register() failed.\n",
			__func__);
			rc = -ENODEV;
			goto fail;
		}
	}

	rc = request_irq(tmdev->tsens_irq, tsens_isr,
		IRQF_TRIGGER_RISING, "tsens_interrupt", tmdev);
	if (rc < 0) {
		pr_err("%s: request_irq FAIL: %d\n", __func__, rc);
		for (i = 0; i < tmdev->tsens_num_sensor; i++)
			thermal_zone_device_unregister(tmdev->sensor[i].tz_dev);
		goto fail;
	}
	platform_set_drvdata(pdev, tmdev);

	INIT_WORK(&tmdev->tsens_work, tsens_scheduler_fn);

	return 0;
fail:
	if (tmdev->tsens_calib_addr)
		iounmap(tmdev->tsens_calib_addr);
	if (tmdev->res_calib_mem)
		release_mem_region(tmdev->res_calib_mem->start,
				tmdev->calib_len);
	if (tmdev->tsens_addr)
		iounmap(tmdev->tsens_addr);
	if (tmdev->res_tsens_mem)
		release_mem_region(tmdev->res_tsens_mem->start,
				tmdev->tsens_len);
	kfree(tmdev);

	return rc;
}

static int __devexit tsens_tm_remove(struct platform_device *pdev)
{
	struct tsens_tm_device *tmdev = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < tmdev->tsens_num_sensor; i++)
		thermal_zone_device_unregister(tmdev->sensor[i].tz_dev);
	if (tmdev->tsens_calib_addr)
		iounmap(tmdev->tsens_calib_addr);
	if (tmdev->res_calib_mem)
		release_mem_region(tmdev->res_calib_mem->start,
				tmdev->calib_len);
	if (tmdev->tsens_addr)
		iounmap(tmdev->tsens_addr);
	if (tmdev->res_tsens_mem)
		release_mem_region(tmdev->res_tsens_mem->start,
				tmdev->tsens_len);
	free_irq(tmdev->tsens_irq, tmdev);
	platform_set_drvdata(pdev, NULL);
	kfree(tmdev);

	return 0;
}

static struct of_device_id tsens_match[] = {
	{	.compatible = "qcom,msm-tsens",
	},
	{}
};

static struct platform_driver tsens_tm_driver = {
	.probe = tsens_tm_probe,
	.remove = tsens_tm_remove,
	.driver = {
		.name = "msm-tsens",
		.owner = THIS_MODULE,
		.of_match_table = tsens_match,
	},
};

static int __init tsens_tm_init_driver(void)
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
