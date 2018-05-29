/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/vmalloc.h>
#include "tsens.h"
#include "thermal_core.h"

#define TSENS_DRIVER_NAME			"msm-tsens"

#define TSENS_UPPER_LOWER_INTERRUPT_CTRL(n)		(n)
#define TSENS_INTERRUPT_EN		BIT(0)

#define TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(n)	((n) + 0x04)
#define TSENS_UPPER_STATUS_CLR		BIT(21)
#define TSENS_LOWER_STATUS_CLR		BIT(20)
#define TSENS_UPPER_THRESHOLD_MASK	0xffc00
#define TSENS_LOWER_THRESHOLD_MASK	0x3ff
#define TSENS_UPPER_THRESHOLD_SHIFT	10

#define TSENS_S0_STATUS_ADDR(n)		((n) + 0x30)
#define TSENS_SN_ADDR_OFFSET		0x4
#define TSENS_SN_STATUS_TEMP_MASK	0x3ff
#define TSENS_SN_STATUS_LOWER_STATUS	BIT(11)
#define TSENS_SN_STATUS_UPPER_STATUS	BIT(12)
#define TSENS_STATUS_ADDR_OFFSET			2

#define TSENS_TRDY_MASK			BIT(0)

#define TSENS_SN_STATUS_ADDR(n)	((n) + 0x44)
#define TSENS_SN_STATUS_VALID		BIT(14)
#define TSENS_SN_STATUS_VALID_MASK	0x4000
#define TSENS_TRDY_ADDR(n)		((n) + 0x84)

#define TSENS_CTRL_ADDR(n)		(n)
#define TSENS_EN				BIT(0)
#define TSENS_CTRL_SENSOR_EN_MASK(n)		((n >> 3) & 0x7ff)
#define TSENS_TRDY_RDY_MIN_TIME		2000
#define TSENS_TRDY_RDY_MAX_TIME		2100
#define TSENS_THRESHOLD_MAX_CODE	0x3ff
#define TSENS_THRESHOLD_MIN_CODE	0x0
#define TSENS_SCALE_MILLIDEG		1000

/* eeprom layout data for 8937 */
#define BASE0_MASK	0x000000ff
#define BASE1_MASK	0xff000000
#define BASE1_SHIFT	24

#define S0_P1_MASK		0x000001f8
#define S1_P1_MASK		0x001f8000
#define S2_P1_MASK_0_4		0xf8000000
#define S2_P1_MASK_5		0x00000001
#define S3_P1_MASK		0x00001f80
#define S4_P1_MASK		0x01f80000
#define S5_P1_MASK		0x00003f00
#define S6_P1_MASK		0x03f00000
#define S7_P1_MASK		0x0000003f
#define S8_P1_MASK		0x0003f000
#define S9_P1_MASK		0x0000003f
#define S10_P1_MASK		0x0003f000

#define S0_P2_MASK		0x00007e00
#define S1_P2_MASK		0x07e00000
#define S2_P2_MASK		0x0000007e
#define S3_P2_MASK		0x0007e000
#define S4_P2_MASK		0x7e000000
#define S5_P2_MASK		0x000fc000
#define S6_P2_MASK		0xfc000000
#define S7_P2_MASK		0x00000fc0
#define S8_P2_MASK		0x00fc0000
#define S9_P2_MASK		0x00000fc0
#define S10_P2_MASK		0x00fc0000

#define S0_P1_SHIFT     3
#define S1_P1_SHIFT     15
#define S2_P1_SHIFT_0_4 27
#define S2_P1_SHIFT_5   5
#define S3_P1_SHIFT     7
#define S4_P1_SHIFT     19
#define S5_P1_SHIFT     8
#define S6_P1_SHIFT     20
#define S8_P1_SHIFT     12
#define S10_P1_SHIFT    12

#define S0_P2_SHIFT     9
#define S1_P2_SHIFT     21
#define S2_P2_SHIFT     1
#define S3_P2_SHIFT     13
#define S4_P2_SHIFT     25
#define S5_P2_SHIFT     14
#define S6_P2_SHIFT     26
#define S7_P2_SHIFT     6
#define S8_P2_SHIFT     18
#define S9_P2_SHIFT     6
#define S10_P2_SHIFT    18

#define CAL_SEL_MASK	0x00000007

#define CAL_DEGC_PT1		30
#define CAL_DEGC_PT2		120
#define SLOPE_FACTOR		1000
#define SLOPE_DEFAULT		3200

/*
 * Use this function on devices where slope and offset calculations
 * depend on calibration data read from qfprom. On others the slope
 * and offset values are derived from tz->tzp->slope and tz->tzp->offset
 * resp.
 */
static void compute_intercept_slope(struct tsens_device *tmdev, u32 *p1,
			     u32 *p2, u32 mode)
{
	int i;
	int num, den;

	for (i = 0; i < TSENS_1x_MAX_SENSORS; i++) {
		pr_debug(
			"sensor%d - data_point1:%#x data_point2:%#x\n",
			i, p1[i], p2[i]);

		tmdev->sensor[i].slope = SLOPE_DEFAULT;
		if (mode == TWO_PT_CALIB) {
			/*
			 * slope (m) = adc_code2 - adc_code1 (y2 - y1)/
			 *	temp_120_degc - temp_30_degc (x2 - x1)
			 */
			num = p2[i] - p1[i];
			num *= SLOPE_FACTOR;
			den = CAL_DEGC_PT2 - CAL_DEGC_PT1;
			tmdev->sensor[i].slope = num / den;
		}

		tmdev->sensor[i].offset = (p1[i] * SLOPE_FACTOR) -
				(CAL_DEGC_PT1 *
				tmdev->sensor[i].slope);
		pr_debug("offset:%d\n", tmdev->sensor[i].offset);
	}
}

static int code_to_degc(u32 adc_code, const struct tsens_sensor *sensor)
{
	int degc, num, den;

	num = (adc_code * SLOPE_FACTOR) - sensor->offset;
	den = sensor->slope;

	if (num > 0)
		degc = num + (den / 2);
	else if (num < 0)
		degc = num - (den / 2);
	else
		degc = num;

	degc /= den;

	return degc;
}

static int degc_to_code(int degc, const struct tsens_sensor *sensor)
{
	int code = ((degc * sensor->slope)
		+ sensor->offset)/SLOPE_FACTOR;

	if (code > TSENS_THRESHOLD_MAX_CODE)
		code = TSENS_THRESHOLD_MAX_CODE;
	else if (code < TSENS_THRESHOLD_MIN_CODE)
		code = TSENS_THRESHOLD_MIN_CODE;
	pr_debug("raw_code:0x%x, degc:%d\n",
			code, degc);
	return code;
}

static int calibrate_8937(struct tsens_device *tmdev)
{
	int base0 = 0, base1 = 0, i;
	u32 p1[TSENS_1x_MAX_SENSORS], p2[TSENS_1x_MAX_SENSORS];
	int mode = 0, tmp = 0;
	u32 qfprom_cdata[5] = {0, 0, 0, 0, 0};

	qfprom_cdata[0] = readl_relaxed(tmdev->tsens_calib_addr + 0x1D8);
	qfprom_cdata[1] = readl_relaxed(tmdev->tsens_calib_addr + 0x1DC);
	qfprom_cdata[2] = readl_relaxed(tmdev->tsens_calib_addr + 0x210);
	qfprom_cdata[3] = readl_relaxed(tmdev->tsens_calib_addr + 0x214);
	qfprom_cdata[4] = readl_relaxed(tmdev->tsens_calib_addr + 0x230);

	mode = (qfprom_cdata[2] & CAL_SEL_MASK);
	pr_debug("calibration mode is %d\n", mode);

	switch (mode) {
	case TWO_PT_CALIB:
		base1 = (qfprom_cdata[1] & BASE1_MASK) >> BASE1_SHIFT;
		p2[0] = (qfprom_cdata[2] & S0_P2_MASK) >> S0_P2_SHIFT;
		p2[1] = (qfprom_cdata[2] & S1_P2_MASK) >> S1_P2_SHIFT;
		p2[2] = (qfprom_cdata[3] & S2_P2_MASK) >> S2_P2_SHIFT;
		p2[3] = (qfprom_cdata[3] & S3_P2_MASK) >> S3_P2_SHIFT;
		p2[4] = (qfprom_cdata[3] & S4_P2_MASK) >> S4_P2_SHIFT;
		p2[5] = (qfprom_cdata[0] & S5_P2_MASK) >> S5_P2_SHIFT;
		p2[6] = (qfprom_cdata[0] & S6_P2_MASK) >> S6_P2_SHIFT;
		p2[7] = (qfprom_cdata[1] & S7_P2_MASK) >> S7_P2_SHIFT;
		p2[8] = (qfprom_cdata[1] & S8_P2_MASK) >> S8_P2_SHIFT;
		p2[9] = (qfprom_cdata[4] & S9_P2_MASK) >> S9_P2_SHIFT;
		p2[10] = (qfprom_cdata[4] & S10_P2_MASK) >> S10_P2_SHIFT;

		for (i = 0; i < TSENS_1x_MAX_SENSORS; i++)
			p2[i] = ((base1 + p2[i]) << 2);
		/* Fall through */
	case ONE_PT_CALIB2:
		base0 = (qfprom_cdata[0] & BASE0_MASK);
		p1[0] = (qfprom_cdata[2] & S0_P1_MASK) >> S0_P1_SHIFT;
		p1[1] = (qfprom_cdata[2] & S1_P1_MASK) >> S1_P1_SHIFT;
		p1[2] = (qfprom_cdata[2] & S2_P1_MASK_0_4) >> S2_P1_SHIFT_0_4;
		tmp = (qfprom_cdata[3] & S2_P1_MASK_5) << S2_P1_SHIFT_5;
		p1[2] |= tmp;
		p1[3] = (qfprom_cdata[3] & S3_P1_MASK) >> S3_P1_SHIFT;
		p1[4] = (qfprom_cdata[3] & S4_P1_MASK) >> S4_P1_SHIFT;
		p1[5] = (qfprom_cdata[0] & S5_P1_MASK) >> S5_P1_SHIFT;
		p1[6] = (qfprom_cdata[0] & S6_P1_MASK) >> S6_P1_SHIFT;
		p1[7] = (qfprom_cdata[1] & S7_P1_MASK);
		p1[8] = (qfprom_cdata[1] & S8_P1_MASK) >> S8_P1_SHIFT;
		p1[9] = (qfprom_cdata[4] & S9_P1_MASK);
		p1[10] = (qfprom_cdata[4] & S10_P1_MASK) >> S10_P1_SHIFT;

		for (i = 0; i < TSENS_1x_MAX_SENSORS; i++)
			p1[i] = (((base0) + p1[i]) << 2);
		break;
	default:
		for (i = 0; i < TSENS_1x_MAX_SENSORS; i++) {
			p1[i] = 500;
			p2[i] = 780;
		}
		break;
	}

	compute_intercept_slope(tmdev, p1, p2, mode);

	return 0;
}

static int tsens1xxx_get_temp(struct tsens_sensor *sensor, int *temp)
{
	struct tsens_device *tmdev = NULL;
	unsigned int code;
	void __iomem *sensor_addr;
	void __iomem *trdy_addr;
	int last_temp = 0, last_temp2 = 0, last_temp3 = 0;
	bool last_temp_valid = false, last_temp2_valid = false;
	bool last_temp3_valid = false;

	if (!sensor)
		return -EINVAL;

	tmdev = sensor->tmdev;

	trdy_addr = TSENS_TRDY_ADDR(tmdev->tsens_tm_addr);
	sensor_addr = TSENS_SN_STATUS_ADDR(tmdev->tsens_tm_addr);

	code = readl_relaxed(sensor_addr +
			(sensor->hw_id << TSENS_STATUS_ADDR_OFFSET));
	last_temp = code & TSENS_SN_STATUS_TEMP_MASK;

	if (tmdev->ctrl_data->valid_status_check) {
		if (code & TSENS_SN_STATUS_VALID)
			last_temp_valid = true;
		else {
			code = readl_relaxed(sensor_addr +
				(sensor->hw_id << TSENS_STATUS_ADDR_OFFSET));
			last_temp2 = code & TSENS_SN_STATUS_TEMP_MASK;
			if (code & TSENS_SN_STATUS_VALID) {
				last_temp = last_temp2;
				last_temp2_valid = true;
			} else {
				code = readl_relaxed(sensor_addr +
					(sensor->hw_id <<
					TSENS_STATUS_ADDR_OFFSET));
				last_temp3 = code & TSENS_SN_STATUS_TEMP_MASK;
				if (code & TSENS_SN_STATUS_VALID) {
					last_temp = last_temp3;
					last_temp3_valid = true;
				}
			}
		}
	}

	if ((tmdev->ctrl_data->valid_status_check) &&
		(!last_temp_valid && !last_temp2_valid && !last_temp3_valid)) {
		if (last_temp == last_temp2)
			last_temp = last_temp2;
		else if (last_temp2 == last_temp3)
			last_temp = last_temp3;
	}

	*temp = code_to_degc(last_temp, sensor);
	*temp = *temp * TSENS_SCALE_MILLIDEG;

	if (tmdev->ops->dbg)
		tmdev->ops->dbg(tmdev, (u32)sensor->hw_id,
				TSENS_DBG_LOG_TEMP_READS, temp);

	return 0;
}

static int tsens_tz_activate_trip_type(struct tsens_sensor *tm_sensor,
			int trip, enum thermal_device_mode mode)
{
	struct tsens_device *tmdev = NULL;
	unsigned int reg_cntl, code, hi_code, lo_code, mask;

	/* clear the interrupt and unmask */
	if (!tm_sensor || trip < 0)
		return -EINVAL;

	tmdev = tm_sensor->tmdev;
	if (!tmdev)
		return -EINVAL;

	lo_code = TSENS_THRESHOLD_MIN_CODE;
	hi_code = TSENS_THRESHOLD_MAX_CODE;

	reg_cntl = readl_relaxed((TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
					(tmdev->tsens_tm_addr) +
					(tm_sensor->hw_id *
					TSENS_SN_ADDR_OFFSET)));

	switch (trip) {
	case THERMAL_TRIP_CONFIGURABLE_HI:
		tmdev->sensor[tm_sensor->hw_id].thr_state.high_th_state = mode;

		code = (reg_cntl & TSENS_UPPER_THRESHOLD_MASK)
					>> TSENS_UPPER_THRESHOLD_SHIFT;
		mask = TSENS_UPPER_STATUS_CLR;

		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_cntl & TSENS_LOWER_THRESHOLD_MASK);
		break;
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		tmdev->sensor[tm_sensor->hw_id].thr_state.low_th_state = mode;

		code = (reg_cntl & TSENS_LOWER_THRESHOLD_MASK);
		mask = TSENS_LOWER_STATUS_CLR;

		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_cntl & TSENS_UPPER_THRESHOLD_MASK)
					>> TSENS_UPPER_THRESHOLD_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	if (mode == THERMAL_DEVICE_DISABLED)
		writel_relaxed(reg_cntl | mask,
		(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(tmdev->tsens_tm_addr) +
			(tm_sensor->hw_id * TSENS_SN_ADDR_OFFSET)));
	else
		writel_relaxed(reg_cntl & ~mask,
		(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(tmdev->tsens_tm_addr) +
		(tm_sensor->hw_id * TSENS_SN_ADDR_OFFSET)));
	/* Enable the thresholds */
	mb();

	return 0;
}

static int tsens1xxx_set_trip_temp(struct tsens_sensor *tm_sensor,
						int low_temp, int high_temp)
{
	unsigned int reg_cntl;
	unsigned long flags;
	struct tsens_device *tmdev = NULL;
	int high_code, low_code, rc = 0;

	if (!tm_sensor)
		return -EINVAL;

	tmdev = tm_sensor->tmdev;
	if (!tmdev)
		return -EINVAL;

	spin_lock_irqsave(&tmdev->tsens_upp_low_lock, flags);

	if (high_temp != INT_MAX) {
		high_temp /= TSENS_SCALE_MILLIDEG;
		high_code = degc_to_code(high_temp, tm_sensor);
		tmdev->sensor[tm_sensor->hw_id].thr_state.high_adc_code =
							high_code;
		tmdev->sensor[tm_sensor->hw_id].thr_state.high_temp =
							high_temp;

		reg_cntl = readl_relaxed(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
					(tmdev->tsens_tm_addr) +
					(tm_sensor->hw_id *
					 TSENS_SN_ADDR_OFFSET));

		high_code <<= TSENS_UPPER_THRESHOLD_SHIFT;
		reg_cntl &= ~TSENS_UPPER_THRESHOLD_MASK;
		writel_relaxed(reg_cntl | high_code,
				(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
					(tmdev->tsens_tm_addr) +
					(tm_sensor->hw_id *
					TSENS_SN_ADDR_OFFSET)));
	}

	if (low_temp != INT_MIN) {
		low_temp /= TSENS_SCALE_MILLIDEG;
		low_code = degc_to_code(low_temp, tm_sensor);
		tmdev->sensor[tm_sensor->hw_id].thr_state.low_adc_code =
							low_code;
		tmdev->sensor[tm_sensor->hw_id].thr_state.low_temp =
							low_temp;

		reg_cntl = readl_relaxed(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
					(tmdev->tsens_tm_addr) +
					(tm_sensor->hw_id *
					TSENS_SN_ADDR_OFFSET));

		reg_cntl &= ~TSENS_LOWER_THRESHOLD_MASK;
		writel_relaxed(reg_cntl | low_code,
				(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
					(tmdev->tsens_tm_addr) +
					(tm_sensor->hw_id *
					TSENS_SN_ADDR_OFFSET)));
	}
	/* Set trip temperature thresholds */
	mb();

	if (high_temp != INT_MAX) {
		rc = tsens_tz_activate_trip_type(tm_sensor,
				THERMAL_TRIP_CONFIGURABLE_HI,
				THERMAL_DEVICE_ENABLED);
		if (rc) {
			pr_err("trip high enable error :%d\n", rc);
			goto fail;
		}
	} else {
		rc = tsens_tz_activate_trip_type(tm_sensor,
				THERMAL_TRIP_CONFIGURABLE_HI,
				THERMAL_DEVICE_DISABLED);
		if (rc) {
			pr_err("trip high disable error :%d\n", rc);
			goto fail;
		}
	}

	if (low_temp != INT_MIN) {
		rc = tsens_tz_activate_trip_type(tm_sensor,
				THERMAL_TRIP_CONFIGURABLE_LOW,
				THERMAL_DEVICE_ENABLED);
		if (rc) {
			pr_err("trip low enable activation error :%d\n", rc);
			goto fail;
		}
	} else {
		rc = tsens_tz_activate_trip_type(tm_sensor,
				THERMAL_TRIP_CONFIGURABLE_LOW,
				THERMAL_DEVICE_DISABLED);
		if (rc) {
			pr_err("trip low disable error :%d\n", rc);
			goto fail;
		}
	}

fail:
	spin_unlock_irqrestore(&tmdev->tsens_upp_low_lock, flags);
	return rc;
}

static irqreturn_t tsens_irq_thread(int irq, void *data)
{
	struct tsens_device *tm = data;
	unsigned int i, status, threshold, temp, th_temp;
	unsigned long flags;
	void __iomem *sensor_status_addr;
	void __iomem *sensor_status_ctrl_addr;
	u32 rc = 0, addr_offset;

	sensor_status_addr = TSENS_SN_STATUS_ADDR(tm->tsens_tm_addr);
	sensor_status_ctrl_addr =
		TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(tm->tsens_tm_addr);

	for (i = 0; i < TSENS_1x_MAX_SENSORS; i++) {
		bool upper_thr = false, lower_thr = false;

		if (IS_ERR(tm->sensor[i].tzd))
			continue;

		rc = tsens1xxx_get_temp(&tm->sensor[i], &temp);
		if (rc) {
			pr_debug("Error:%d reading temp sensor:%d\n", rc, i);
			continue;
		}

		spin_lock_irqsave(&tm->tsens_upp_low_lock, flags);

		addr_offset = tm->sensor[i].hw_id *
						TSENS_SN_ADDR_OFFSET;
		status = readl_relaxed(sensor_status_addr + addr_offset);
		threshold = readl_relaxed(sensor_status_ctrl_addr +
								addr_offset);

		if (status & TSENS_SN_STATUS_UPPER_STATUS) {
			writel_relaxed(threshold | TSENS_UPPER_STATUS_CLR,
				TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(
					tm->tsens_tm_addr + addr_offset));
			th_temp = code_to_degc((threshold &
					TSENS_UPPER_THRESHOLD_MASK) >>
					TSENS_UPPER_THRESHOLD_SHIFT,
					(tm->sensor + i));
			if (th_temp > (temp/TSENS_SCALE_MILLIDEG)) {
				pr_debug("Re-arm high threshold\n");
				rc = tsens_tz_activate_trip_type(
						&tm->sensor[i],
						THERMAL_TRIP_CONFIGURABLE_HI,
						THERMAL_DEVICE_ENABLED);
				if (rc)
					pr_err("high rearm failed");
			} else {
				upper_thr = true;
				tm->sensor[i].thr_state.high_th_state =
					THERMAL_DEVICE_DISABLED;
			}
		}

		if (status & TSENS_SN_STATUS_LOWER_STATUS) {
			writel_relaxed(threshold | TSENS_LOWER_STATUS_CLR,
				TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(
					tm->tsens_tm_addr + addr_offset));
			th_temp = code_to_degc((threshold &
					TSENS_LOWER_THRESHOLD_MASK),
					(tm->sensor + i));
			if (th_temp < (temp/TSENS_SCALE_MILLIDEG)) {
				pr_debug("Re-arm Low threshold\n");
				rc = tsens_tz_activate_trip_type(
						&tm->sensor[i],
						THERMAL_TRIP_CONFIGURABLE_LOW,
						THERMAL_DEVICE_ENABLED);
				if (rc)
					pr_err("low rearm failed");
			} else {
				lower_thr = true;
				tm->sensor[i].thr_state.low_th_state =
					THERMAL_DEVICE_DISABLED;
			}
		}
		spin_unlock_irqrestore(&tm->tsens_upp_low_lock, flags);

		if (upper_thr || lower_thr) {
			pr_debug("sensor:%d trigger temp (%d degC)\n",
				tm->sensor[i].hw_id,
				code_to_degc((status &
				TSENS_SN_STATUS_TEMP_MASK),
				tm->sensor));
			of_thermal_handle_trip(tm->sensor[i].tzd);
		}
	}

	/* Disable monitoring sensor trip threshold for triggered sensor */
	mb();

	if (tm->ops->dbg)
		tm->ops->dbg(tm, 0, TSENS_DBG_LOG_INTERRUPT_TIMESTAMP, NULL);

	return IRQ_HANDLED;
}

static int tsens1xxx_hw_sensor_en(struct tsens_device *tmdev,
					u32 sensor_id)
{
	void __iomem *srot_addr;
	unsigned int srot_val, sensor_en;

	srot_addr = TSENS_CTRL_ADDR(tmdev->tsens_srot_addr + 0x4);
	srot_val = readl_relaxed(srot_addr);
	srot_val = TSENS_CTRL_SENSOR_EN_MASK(srot_val);

	sensor_en = ((1 << sensor_id) & srot_val);

	return sensor_en;
}

static int tsens1xxx_hw_init(struct tsens_device *tmdev)
{
	void __iomem *srot_addr;
	unsigned int srot_val;

	srot_addr = TSENS_CTRL_ADDR(tmdev->tsens_srot_addr + 0x4);
	srot_val = readl_relaxed(srot_addr);
	if (!(srot_val & TSENS_EN)) {
		pr_err("TSENS device is not enabled\n");
		return -ENODEV;
	}

	writel_relaxed(TSENS_INTERRUPT_EN,
			TSENS_UPPER_LOWER_INTERRUPT_CTRL(tmdev->tsens_tm_addr));

	spin_lock_init(&tmdev->tsens_upp_low_lock);

	if (tmdev->ctrl_data->mtc) {
		if (tmdev->ops->dbg)
			tmdev->ops->dbg(tmdev, 0, TSENS_DBG_MTC_DATA, NULL);
	}

	return 0;
}

static const struct tsens_irqs tsens1xxx_irqs[] = {
	{ "tsens-upper-lower", tsens_irq_thread},
};

static int tsens1xxx_register_interrupts(struct tsens_device *tmdev)
{
	struct platform_device *pdev;
	int i, rc;

	if (!tmdev)
		return -EINVAL;

	pdev = tmdev->pdev;

	for (i = 0; i < ARRAY_SIZE(tsens1xxx_irqs); i++) {
		int irq;

		irq = platform_get_irq_byname(pdev, tsens1xxx_irqs[i].name);
		if (irq < 0) {
			dev_err(&pdev->dev, "failed to get irq %s\n",
					tsens1xxx_irqs[i].name);
			return irq;
		}

		rc = devm_request_threaded_irq(&pdev->dev, irq, NULL,
				tsens1xxx_irqs[i].handler,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				tsens1xxx_irqs[i].name, tmdev);
		if (rc) {
			dev_err(&pdev->dev, "failed to get irq %s\n",
					tsens1xxx_irqs[i].name);
			return rc;
		}
		enable_irq_wake(irq);
	}

	return 0;
}

static const struct tsens_ops ops_tsens1xxx = {
	.hw_init		= tsens1xxx_hw_init,
	.get_temp		= tsens1xxx_get_temp,
	.set_trips		= tsens1xxx_set_trip_temp,
	.interrupts_reg	= tsens1xxx_register_interrupts,
	.sensor_en		= tsens1xxx_hw_sensor_en,
	.calibrate		= calibrate_8937,
	.dbg            = tsens2xxx_dbg,
};

const struct tsens_data data_tsens14xx = {
	.ops = &ops_tsens1xxx,
	.valid_status_check = true,
	.mtc = true,
	.ver_major = 1,
	.ver_minor = 4,
};
