/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
/*
 * Qualcomm MSM8960 TSENS driver
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/msm_tsens.h>
#include <linux/io.h>

#include <mach/msm_iomap.h>
#include <mach/socinfo.h>

/* Trips: from very hot to very cold */
enum tsens_trip_type {
	TSENS_TRIP_STAGE3 = 0,
	TSENS_TRIP_STAGE2,
	TSENS_TRIP_STAGE1,
	TSENS_TRIP_STAGE0,
	TSENS_TRIP_NUM,
};

/* MSM8960 TSENS register info */
#define TSENS_CAL_DEGC					30
#define TSENS_MAIN_SENSOR				0

#define TSENS_8960_QFPROM_ADDR0		(MSM_QFPROM_BASE + 0x00000404)
#define TSENS_8960_QFPROM_RED_TEMP_SENSOR0_SHIFT	8
#define TSENS_8960_QFPROM_TEMP_SENSOR0_SHIFT		0
#define TSENS_8960_QFPROM_TEMP_SENSOR0_MASK		\
			(255 << TSENS_QFPROM_TEMP_SENSOR0_SHIFT)
#define TSENS_8960_QFPROM_RED_TEMP_SENSOR0_MASK		\
			(255 << TSENS_8960_QFPROM_RED_TEMP_SENSOR0_SHIFT)

#define TSENS_8960_CONFIG				0x9b
#define TSENS_8960_CONFIG_SHIFT				0
#define TSENS_8960_CONFIG_MASK		(0xf << TSENS_8960_CONFIG_SHIFT)
#define TSENS_CNTL_ADDR			(MSM_CLK_CTL_BASE + 0x00003620)
#define TSENS_EN					BIT(0)
#define TSENS_SW_RST					BIT(1)
#define TSENS_ADC_CLK_SEL				BIT(2)
#define SENSOR0_EN					BIT(3)
#define SENSOR1_EN					BIT(4)
#define SENSOR2_EN					BIT(5)
#define SENSOR3_EN					BIT(6)
#define SENSOR4_EN					BIT(7)
#define SENSORS_EN			(SENSOR0_EN | SENSOR1_EN |	\
					SENSOR2_EN | SENSOR3_EN | SENSOR4_EN)
#define TSENS_MIN_STATUS_MASK				BIT(8)
#define TSENS_LOWER_STATUS_CLR				BIT(9)
#define TSENS_UPPER_STATUS_CLR				BIT(10)
#define TSENS_MAX_STATUS_MASK				BIT(11)
#define TSENS_MEASURE_PERIOD				4 /* 1 sec. default */
#define TSENS_8960_SLP_CLK_ENA				BIT(26)

#define TSENS_THRESHOLD_ADDR		(MSM_CLK_CTL_BASE + 0x00003624)
#define TSENS_THRESHOLD_MAX_CODE			0xff
#define TSENS_THRESHOLD_MIN_CODE			0
#define TSENS_THRESHOLD_MAX_LIMIT_SHIFT			24
#define TSENS_THRESHOLD_MIN_LIMIT_SHIFT			16
#define TSENS_THRESHOLD_UPPER_LIMIT_SHIFT		8
#define TSENS_THRESHOLD_LOWER_LIMIT_SHIFT		0
#define TSENS_THRESHOLD_MAX_LIMIT_MASK		(TSENS_THRESHOLD_MAX_CODE << \
					TSENS_THRESHOLD_MAX_LIMIT_SHIFT)
#define TSENS_THRESHOLD_MIN_LIMIT_MASK		(TSENS_THRESHOLD_MAX_CODE << \
					TSENS_THRESHOLD_MIN_LIMIT_SHIFT)
#define TSENS_THRESHOLD_UPPER_LIMIT_MASK	(TSENS_THRESHOLD_MAX_CODE << \
					TSENS_THRESHOLD_UPPER_LIMIT_SHIFT)
#define TSENS_THRESHOLD_LOWER_LIMIT_MASK	(TSENS_THRESHOLD_MAX_CODE << \
					TSENS_THRESHOLD_LOWER_LIMIT_SHIFT)
/* Initial temperature threshold values */
#define TSENS_LOWER_LIMIT_TH				0x50
#define TSENS_UPPER_LIMIT_TH				0xdf
#define TSENS_MIN_LIMIT_TH				0x38
#define TSENS_MAX_LIMIT_TH				0xff

#define TSENS_S0_STATUS_ADDR			(MSM_CLK_CTL_BASE + 0x00003628)
#define TSENS_STATUS_ADDR_OFFSET			2
#define TSENS_INT_STATUS_ADDR			(MSM_CLK_CTL_BASE + 0x0000363c)

#define TSENS_LOWER_INT_MASK				BIT(1)
#define TSENS_UPPER_INT_MASK				BIT(2)
#define TSENS_MAX_INT_MASK				BIT(3)
#define TSENS_TRDY_MASK					BIT(7)

#define TSENS_8960_CONFIG_ADDR			(MSM_CLK_CTL_BASE + 0x00003640)
#define TSENS_TRDY_RDY_MIN_TIME				1000
#define TSENS_TRDY_RDY_MAX_TIME				1100
#define TSENS_SENSOR_SHIFT				16
#define TSENS_RED_SHIFT					8
#define TSENS_8960_QFPROM_SHIFT				4
#define TSENS_SENSOR0_SHIFT				3
#define TSENS_MASK1					1

#define TSENS_8660_QFPROM_ADDR			(MSM_QFPROM_BASE + 0x000000bc)
#define TSENS_8660_QFPROM_RED_TEMP_SENSOR0_SHIFT	24
#define TSENS_8660_QFPROM_TEMP_SENSOR0_SHIFT		16
#define TSENS_8660_QFPROM_TEMP_SENSOR0_MASK		(255		\
					<< TSENS_8660_QFPROM_TEMP_SENSOR0_SHIFT)
#define TSENS_8660_CONFIG				01
#define TSENS_8660_CONFIG_SHIFT				28
#define TSENS_8660_CONFIG_MASK			(3 << TSENS_8660_CONFIG_SHIFT)
#define TSENS_8660_SLP_CLK_ENA				BIT(24)

struct tsens_tm_device_sensor {
	struct thermal_zone_device	*tz_dev;
	enum thermal_device_mode	mode;
	unsigned int			sensor_num;
	struct work_struct		work;
	int				offset;
	int				calib_data;
	int				calib_data_backup;
};

struct tsens_tm_device {
	bool				prev_reading_avail;
	int				slope_mul_tsens_factor;
	int				tsens_factor;
	uint32_t			tsens_num_sensor;
	enum platform_type		hw_type;
	struct tsens_tm_device_sensor	sensor[0];
};

struct tsens_tm_device *tmdev;

/* Temperature on y axis and ADC-code on x-axis */
static int tsens_tz_code_to_degC(int adc_code, int sensor_num)
{
	int degC, degcbeforefactor;
	degcbeforefactor = adc_code * tmdev->slope_mul_tsens_factor
				+ tmdev->sensor[sensor_num].offset;
	if (degcbeforefactor == 0)
		degC = degcbeforefactor;
	else if (degcbeforefactor > 0)
		degC = (degcbeforefactor + tmdev->tsens_factor/2)
						/ tmdev->tsens_factor;
	else  /* rounding for negative degrees */
		degC = (degcbeforefactor - tmdev->tsens_factor/2)
						/ tmdev->tsens_factor;
	return degC;
}

static int tsens_tz_degC_to_code(int degC, int sensor_num)
{
	int code = (degC * tmdev->tsens_factor -
			tmdev->sensor[sensor_num].offset
			+ tmdev->slope_mul_tsens_factor/2)
			/ tmdev->slope_mul_tsens_factor;

	if (code > TSENS_THRESHOLD_MAX_CODE)
		code = TSENS_THRESHOLD_MAX_CODE;
	else if (code < TSENS_THRESHOLD_MIN_CODE)
		code = TSENS_THRESHOLD_MIN_CODE;
	return code;
}

static int tsens_tz_get_temp(struct thermal_zone_device *thermal,
			     unsigned long *temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int code;

	if (!tm_sensor || tm_sensor->mode != THERMAL_DEVICE_ENABLED || !temp)
		return -EINVAL;

	if (!tmdev->prev_reading_avail) {
		while (!(readl_relaxed(TSENS_INT_STATUS_ADDR) &
						TSENS_TRDY_MASK))
			usleep_range(TSENS_TRDY_RDY_MIN_TIME,
				TSENS_TRDY_RDY_MAX_TIME);
		tmdev->prev_reading_avail = true;
	}
	code = readl_relaxed(TSENS_S0_STATUS_ADDR +
			(tm_sensor->sensor_num << TSENS_STATUS_ADDR_OFFSET));
	*temp = tsens_tz_code_to_degC(code, tm_sensor->sensor_num);

	return 0;
}

static int tsens_tz_get_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode *mode)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;

	if (!tm_sensor || !mode)
		return -EINVAL;

	*mode = tm_sensor->mode;

	return 0;
}

/* Function to enable the mode.
 * If the main sensor is disabled all the sensors are disable and
 * the clock is disabled.
 * If the main sensor is not enabled and sub sensor is enabled
 * returns with an error stating the main sensor is not enabled.
 */
static int tsens_tz_set_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode mode)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg, mask, i;

	if (!tm_sensor)
		return -EINVAL;

	if (mode != tm_sensor->mode) {
		pr_info("%s: mode: %d --> %d\n", __func__, tm_sensor->mode,
									 mode);

		reg = readl_relaxed(TSENS_CNTL_ADDR);

		mask = 1 << (tm_sensor->sensor_num + TSENS_SENSOR0_SHIFT);
		if (mode == THERMAL_DEVICE_ENABLED) {
			if ((mask != SENSOR0_EN) && !(reg & SENSOR0_EN)) {
				pr_info("Main sensor not enabled\n");
				return -EINVAL;
			}
			writel_relaxed(reg | TSENS_SW_RST, TSENS_CNTL_ADDR);
			if (tmdev->hw_type == MSM_8960)
				reg |= mask | TSENS_8960_SLP_CLK_ENA
							| TSENS_EN;
			else
				reg |= mask | TSENS_8660_SLP_CLK_ENA
							| TSENS_EN;
			tmdev->prev_reading_avail = false;
		} else {
			reg &= ~mask;
			if (!(reg & SENSOR0_EN)) {
				if (tmdev->hw_type == MSM_8960)
					reg &= ~(SENSORS_EN |
						TSENS_8960_SLP_CLK_ENA |
						TSENS_EN);
				else
					reg &= ~(SENSORS_EN |
						TSENS_8660_SLP_CLK_ENA |
						TSENS_EN);

				for (i = 1; i < tmdev->tsens_num_sensor; i++)
					tmdev->sensor[i].mode = mode;

			}
		}
		writel_relaxed(reg, TSENS_CNTL_ADDR);
	}
	tm_sensor->mode = mode;

	return 0;
}

static int tsens_tz_get_trip_type(struct thermal_zone_device *thermal,
				   int trip, enum thermal_trip_type *type)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;

	if (!tm_sensor || trip < 0 || !type)
		return -EINVAL;

	switch (trip) {
	case TSENS_TRIP_STAGE3:
		*type = THERMAL_TRIP_CRITICAL;
		break;
	case TSENS_TRIP_STAGE2:
		*type = THERMAL_TRIP_CONFIGURABLE_HI;
		break;
	case TSENS_TRIP_STAGE1:
		*type = THERMAL_TRIP_CONFIGURABLE_LOW;
		break;
	case TSENS_TRIP_STAGE0:
		*type = THERMAL_TRIP_CRITICAL_LOW;
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
	unsigned int reg_cntl, reg_th, code, hi_code, lo_code, mask;

	if (!tm_sensor || trip < 0)
		return -EINVAL;

	lo_code = TSENS_THRESHOLD_MIN_CODE;
	hi_code = TSENS_THRESHOLD_MAX_CODE;

	reg_cntl = readl_relaxed(TSENS_CNTL_ADDR);
	reg_th = readl_relaxed(TSENS_THRESHOLD_ADDR);
	switch (trip) {
	case TSENS_TRIP_STAGE3:
		code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		mask = TSENS_MAX_STATUS_MASK;

		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			lo_code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MIN_STATUS_MASK))
			lo_code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE2:
		code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		mask = TSENS_UPPER_STATUS_CLR;

		if (!(reg_cntl & TSENS_MAX_STATUS_MASK))
			hi_code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MIN_STATUS_MASK))
			lo_code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE1:
		code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		mask = TSENS_LOWER_STATUS_CLR;

		if (!(reg_cntl & TSENS_MIN_STATUS_MASK))
			lo_code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MAX_STATUS_MASK))
			hi_code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE0:
		code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		mask = TSENS_MIN_STATUS_MASK;

		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			hi_code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MAX_STATUS_MASK))
			hi_code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	if (mode == THERMAL_TRIP_ACTIVATION_DISABLED)
		writel_relaxed(reg_cntl | mask, TSENS_CNTL_ADDR);
	else {
		if (code < lo_code || code > hi_code) {
			pr_info("%s with invalid code %x\n", __func__, code);
			return -EINVAL;
		}
		writel_relaxed(reg_cntl & ~mask, TSENS_CNTL_ADDR);
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

	reg = readl_relaxed(TSENS_THRESHOLD_ADDR);
	switch (trip) {
	case TSENS_TRIP_STAGE3:
		reg = (reg & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE2:
		reg = (reg & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE1:
		reg = (reg & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE0:
		reg = (reg & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	*temp = tsens_tz_code_to_degC(reg, tm_sensor->sensor_num);

	return 0;
}

static int tsens_tz_get_crit_temp(struct thermal_zone_device *thermal,
				  unsigned long *temp)
{
	return tsens_tz_get_trip_temp(thermal, TSENS_TRIP_STAGE3, temp);
}

static int tsens_tz_set_trip_temp(struct thermal_zone_device *thermal,
				   int trip, long temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg_th, reg_cntl;
	int code, hi_code, lo_code, code_err_chk;

	code_err_chk = code = tsens_tz_degC_to_code(temp,
					tm_sensor->sensor_num);
	if (!tm_sensor || trip < 0)
		return -EINVAL;

	lo_code = TSENS_THRESHOLD_MIN_CODE;
	hi_code = TSENS_THRESHOLD_MAX_CODE;

	reg_cntl = readl_relaxed(TSENS_CNTL_ADDR);
	reg_th = readl_relaxed(TSENS_THRESHOLD_ADDR);
	switch (trip) {
	case TSENS_TRIP_STAGE3:
		code <<= TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		reg_th &= ~TSENS_THRESHOLD_MAX_LIMIT_MASK;

		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			lo_code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MIN_STATUS_MASK))
			lo_code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE2:
		code <<= TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		reg_th &= ~TSENS_THRESHOLD_UPPER_LIMIT_MASK;

		if (!(reg_cntl & TSENS_MAX_STATUS_MASK))
			hi_code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MIN_STATUS_MASK))
			lo_code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE1:
		code <<= TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		reg_th &= ~TSENS_THRESHOLD_LOWER_LIMIT_MASK;

		if (!(reg_cntl & TSENS_MIN_STATUS_MASK))
			lo_code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MAX_STATUS_MASK))
			hi_code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE0:
		code <<= TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		reg_th &= ~TSENS_THRESHOLD_MIN_LIMIT_MASK;

		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			hi_code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MAX_STATUS_MASK))
			hi_code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	if (code_err_chk < lo_code || code_err_chk > hi_code)
		return -EINVAL;

	writel_relaxed(reg_th | code, TSENS_THRESHOLD_ADDR);

	return 0;
}

static struct thermal_zone_device_ops tsens_thermal_zone_ops = {
	.get_temp = tsens_tz_get_temp,
	.get_mode = tsens_tz_get_mode,
	.set_mode = tsens_tz_set_mode,
	.get_trip_type = tsens_tz_get_trip_type,
	.activate_trip_type = tsens_tz_activate_trip_type,
	.get_trip_temp = tsens_tz_get_trip_temp,
	.set_trip_temp = tsens_tz_set_trip_temp,
	.get_crit_temp = tsens_tz_get_crit_temp,
};

static void notify_uspace_tsens_fn(struct work_struct *work)
{
	struct tsens_tm_device_sensor *tm = container_of(work,
		struct tsens_tm_device_sensor, work);

	sysfs_notify(&tm->tz_dev->device.kobj,
					NULL, "type");
}

static irqreturn_t tsens_isr(int irq, void *data)
{
	struct tsens_tm_device *tm = data;
	unsigned int threshold, threshold_low, i, code, reg, sensor, mask;
	bool upper_th_x, lower_th_x;
	int adc_code;

	reg = readl_relaxed(TSENS_CNTL_ADDR);
	writel_relaxed(reg | TSENS_LOWER_STATUS_CLR | TSENS_UPPER_STATUS_CLR,
			TSENS_CNTL_ADDR);
	mask = ~(TSENS_LOWER_STATUS_CLR | TSENS_UPPER_STATUS_CLR);
	threshold = readl_relaxed(TSENS_THRESHOLD_ADDR);
	threshold_low = (threshold & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
	threshold = (threshold & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
	reg = sensor = readl_relaxed(TSENS_CNTL_ADDR);
	sensor &= (uint32_t) SENSORS_EN;
	sensor >>= TSENS_SENSOR0_SHIFT;
	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		if (sensor & TSENS_MASK1) {
			code = readl_relaxed(TSENS_S0_STATUS_ADDR +
					(i << TSENS_STATUS_ADDR_OFFSET));
			upper_th_x = code >= threshold;
			lower_th_x = code <= threshold_low;
			if (upper_th_x)
				mask |= TSENS_UPPER_STATUS_CLR;
			if (lower_th_x)
				mask |= TSENS_LOWER_STATUS_CLR;
			if (upper_th_x || lower_th_x) {
				/* Notify user space */
				schedule_work(&tm->sensor[i].work);
				adc_code = readl_relaxed(TSENS_S0_STATUS_ADDR
					+ (i << TSENS_STATUS_ADDR_OFFSET));
				pr_info("\nTrip point triggered by "
					"current temperature (%d degrees) "
					"measured by Temperature-Sensor %d\n",
					tsens_tz_code_to_degC(adc_code, i), i);
			}
		}
		sensor >>= 1;
	}
	writel_relaxed(reg & mask, TSENS_CNTL_ADDR);
	mb();
	return IRQ_HANDLED;
}

static void tsens_disable_mode(void)
{
	unsigned int reg_cntl = 0;

	reg_cntl = readl_relaxed(TSENS_CNTL_ADDR);
	if (tmdev->hw_type == MSM_8960)
		writel_relaxed(reg_cntl &
				~((((1 << tmdev->tsens_num_sensor) - 1) <<
				TSENS_SENSOR0_SHIFT) | TSENS_8960_SLP_CLK_ENA
				| TSENS_EN), TSENS_CNTL_ADDR);
	else if (tmdev->hw_type == MSM_8660)
		writel_relaxed(reg_cntl &
				~((((1 << tmdev->tsens_num_sensor) - 1) <<
				TSENS_SENSOR0_SHIFT) | TSENS_8660_SLP_CLK_ENA
				| TSENS_EN), TSENS_CNTL_ADDR);
}

static void tsens_hw_init(void)
{
	unsigned int reg_cntl = 0, reg_cfg = 0, reg_thr = 0;

	reg_cntl = readl_relaxed(TSENS_CNTL_ADDR);
	writel_relaxed(reg_cntl | TSENS_SW_RST, TSENS_CNTL_ADDR);

	if (tmdev->hw_type == MSM_8960) {
		reg_cntl |= TSENS_8960_SLP_CLK_ENA | TSENS_EN |
			(TSENS_MEASURE_PERIOD << 18) |
			TSENS_LOWER_STATUS_CLR | TSENS_UPPER_STATUS_CLR |
			TSENS_MIN_STATUS_MASK | TSENS_MAX_STATUS_MASK |
			(((1 << tmdev->tsens_num_sensor) - 1) <<
			TSENS_SENSOR0_SHIFT);
		writel_relaxed(reg_cntl, TSENS_CNTL_ADDR);

		reg_cfg = readl_relaxed(TSENS_8960_CONFIG_ADDR);
		reg_cfg = (reg_cfg & ~TSENS_8960_CONFIG_MASK) |
			(TSENS_8960_CONFIG << TSENS_8960_CONFIG_SHIFT);
		writel_relaxed(reg_cfg, TSENS_8960_CONFIG_ADDR);
	} else if (tmdev->hw_type == MSM_8660) {
		reg_cntl |= TSENS_8660_SLP_CLK_ENA | TSENS_EN |
			(TSENS_MEASURE_PERIOD << 16) |
			TSENS_LOWER_STATUS_CLR | TSENS_UPPER_STATUS_CLR |
			TSENS_MIN_STATUS_MASK | TSENS_MAX_STATUS_MASK |
			(((1 << tmdev->tsens_num_sensor) - 1) <<
			TSENS_SENSOR0_SHIFT);

		/* set TSENS_CONFIG bits (bits 29:28 of TSENS_CNTL) to '01';
			this setting found to be optimal. */
		reg_cntl = (reg_cntl & ~TSENS_8660_CONFIG_MASK) |
				(TSENS_8660_CONFIG << TSENS_8660_CONFIG_SHIFT);

		writel_relaxed(reg_cntl, TSENS_CNTL_ADDR);
	}

	reg_thr |= (TSENS_LOWER_LIMIT_TH << TSENS_THRESHOLD_LOWER_LIMIT_SHIFT) |
		(TSENS_UPPER_LIMIT_TH << TSENS_THRESHOLD_UPPER_LIMIT_SHIFT) |
		(TSENS_MIN_LIMIT_TH << TSENS_THRESHOLD_MIN_LIMIT_SHIFT) |
		(TSENS_MAX_LIMIT_TH << TSENS_THRESHOLD_MAX_LIMIT_SHIFT);
	writel_relaxed(reg_thr, TSENS_THRESHOLD_ADDR);
}

static int tsens_calib_sensors8660(void)
{
	uint32_t *main_sensor_addr, sensor_shift, red_sensor_shift;
	uint32_t sensor_mask, red_sensor_mask;

	main_sensor_addr = TSENS_8660_QFPROM_ADDR;
	sensor_shift = TSENS_SENSOR_SHIFT;
	red_sensor_shift = sensor_shift + TSENS_RED_SHIFT;
	sensor_mask = TSENS_THRESHOLD_MAX_CODE << sensor_shift;
	red_sensor_mask = TSENS_THRESHOLD_MAX_CODE << red_sensor_shift;
	tmdev->sensor[TSENS_MAIN_SENSOR].calib_data =
		(readl_relaxed(main_sensor_addr) & sensor_mask)
						>> sensor_shift;
	tmdev->sensor[TSENS_MAIN_SENSOR].calib_data_backup =
				(readl_relaxed(main_sensor_addr)
			& red_sensor_mask) >> red_sensor_shift;
	if (tmdev->sensor[TSENS_MAIN_SENSOR].calib_data_backup)
			tmdev->sensor[TSENS_MAIN_SENSOR].calib_data =
			tmdev->sensor[TSENS_MAIN_SENSOR].calib_data_backup;
	if (!tmdev->sensor[TSENS_MAIN_SENSOR].calib_data) {
		pr_err("%s: No temperature sensor data for calibration"
				" in QFPROM!\n", __func__);
		return -ENODEV;
	}

	tmdev->sensor[TSENS_MAIN_SENSOR].offset = tmdev->tsens_factor *
		TSENS_CAL_DEGC - tmdev->slope_mul_tsens_factor *
		tmdev->sensor[TSENS_MAIN_SENSOR].calib_data;
	tmdev->prev_reading_avail = false;
	INIT_WORK(&tmdev->sensor[TSENS_MAIN_SENSOR].work,
						notify_uspace_tsens_fn);

	return 0;
}

static int tsens_calib_sensors8960(void)
{
	uint32_t *main_sensor_addr, sensor_shift, red_sensor_shift;
	uint32_t sensor_mask, red_sensor_mask, i;
	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		main_sensor_addr = TSENS_8960_QFPROM_ADDR0 +
			(TSENS_8960_QFPROM_SHIFT * (i >> TSENS_MASK1));
		sensor_shift = (i & TSENS_MASK1) * TSENS_SENSOR_SHIFT;
		red_sensor_shift = sensor_shift + TSENS_RED_SHIFT;
		sensor_mask = TSENS_THRESHOLD_MAX_CODE << sensor_shift;
		red_sensor_mask = TSENS_THRESHOLD_MAX_CODE <<
							red_sensor_shift;

		tmdev->sensor[i].calib_data = (readl_relaxed(main_sensor_addr)
			& sensor_mask) >> sensor_shift;
		tmdev->sensor[i].calib_data_backup =
			(readl_relaxed(main_sensor_addr) &
				red_sensor_mask) >> red_sensor_shift;
		if (tmdev->sensor[i].calib_data_backup)
			tmdev->sensor[i].calib_data =
				tmdev->sensor[i].calib_data_backup;

		/* Hardcoded calibration data based on pervious
		 * chip. Remove once we obtain the data. */
		tmdev->sensor[i].calib_data = 91;

		if (!tmdev->sensor[i].calib_data) {
			pr_err("%s: No temperature sensor:%d data for"
			" calibration in QFPROM!\n", __func__, i);
			return -ENODEV;
		}
		tmdev->sensor[i].offset = tmdev->tsens_factor *
			TSENS_CAL_DEGC - tmdev->slope_mul_tsens_factor *
			tmdev->sensor[i].calib_data;
		tmdev->prev_reading_avail = false;
		INIT_WORK(&tmdev->sensor[i].work, notify_uspace_tsens_fn);
	}

	return 0;
}

static int tsens_check_version_support(void)
{
	int rc = 0;

	if (tmdev->hw_type == MSM_8960)
		if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 1)
			rc = -ENODEV;
	return rc;
}

static int tsens_calib_sensors(void)
{
	int rc;

	if (tmdev->hw_type == MSM_8660)
		rc = tsens_calib_sensors8660();
	else if (tmdev->hw_type == MSM_8960)
		rc = tsens_calib_sensors8960();

	return rc;
}

static int __devinit tsens_tm_probe(struct platform_device *pdev)
{
	int rc, i;
	struct tsens_platform_data *pdata;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		pr_err("No TSENS Platform data\n");
		return -EINVAL;
	}

	tmdev = kzalloc(sizeof(struct tsens_tm_device) +
			pdata->tsens_num_sensor *
			sizeof(struct tsens_tm_device_sensor),
			GFP_KERNEL);
	if (tmdev == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	tmdev->slope_mul_tsens_factor = pdata->slope;
	tmdev->tsens_factor = pdata->tsens_factor;
	tmdev->tsens_num_sensor = pdata->tsens_num_sensor;
	tmdev->hw_type = pdata->hw_type;

	rc = tsens_check_version_support();
	if (rc < 0) {
		kfree(tmdev);
		return rc;
	}

	rc = tsens_calib_sensors();
	if (rc < 0) {
		kfree(tmdev);
		return rc;
	}

	platform_set_drvdata(pdev, tmdev);

	tsens_hw_init();

	for (i = 0; i < pdata->tsens_num_sensor; i++) {
		char name[17];
		snprintf(name, sizeof(name), "tsens_tz_sensor%d", i);
		tmdev->sensor[i].mode = THERMAL_DEVICE_ENABLED;
		tmdev->sensor[i].sensor_num = i;
		tmdev->sensor[i].tz_dev = thermal_zone_device_register(name,
				TSENS_TRIP_NUM, &tmdev->sensor[i],
				&tsens_thermal_zone_ops, 0, 0, 0, 0);
		if (tmdev->sensor[i].tz_dev == NULL) {
			pr_err("%s: thermal_zone_device_register() failed.\n",
			__func__);
			rc = -ENODEV;
			goto fail;
		}
		tmdev->sensor[i].mode = THERMAL_DEVICE_DISABLED;
	}

	rc = request_irq(TSENS_UPPER_LOWER_INT, tsens_isr,
		IRQF_TRIGGER_RISING, "tsens_interrupt", tmdev);
	if (rc < 0) {
		pr_err("%s: request_irq FAIL: %d\n", __func__, rc);
		for (i = 0; i < tmdev->tsens_num_sensor; i++)
			thermal_zone_device_unregister(tmdev->sensor[i].tz_dev);
		goto fail;
	}

	tsens_disable_mode();

	pr_notice("%s: OK\n", __func__);
	mb();
	return 0;
fail:
	tsens_disable_mode();
	platform_set_drvdata(pdev, NULL);
	kfree(tmdev);
	mb();
	return rc;
}

static int __devexit tsens_tm_remove(struct platform_device *pdev)
{
	struct tsens_tm_device *tmdev = platform_get_drvdata(pdev);
	int i;

	tsens_disable_mode();
	mb();
	free_irq(TSENS_UPPER_LOWER_INT, tmdev);
	for (i = 0; i < tmdev->tsens_num_sensor; i++)
		thermal_zone_device_unregister(tmdev->sensor[i].tz_dev);
	platform_set_drvdata(pdev, NULL);
	kfree(tmdev);
	return 0;
}

static struct platform_driver tsens_tm_driver = {
	.probe	= tsens_tm_probe,
	.remove	= __devexit_p(tsens_tm_remove),
	.driver	= {
		.name = "tsens8960-tm",
		.owner = THIS_MODULE,
	},
};

static int __init tsens_init(void)
{
	return platform_driver_register(&tsens_tm_driver);
}

static void __exit tsens_exit(void)
{
	platform_driver_unregister(&tsens_tm_driver);
}

module_init(tsens_init);
module_exit(tsens_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM8960 Temperature Sensor driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:tsens8960-tm");
