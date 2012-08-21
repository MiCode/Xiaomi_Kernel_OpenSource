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
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/hwmon.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/spmi.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/hwmon-sysfs.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/platform_device.h>

/* QPNP VADC register definition */
#define QPNP_VADC_REVISION1				0x0
#define QPNP_VADC_REVISION2				0x1
#define QPNP_VADC_REVISION3				0x2
#define QPNP_VADC_REVISION4				0x3
#define QPNP_VADC_PERPH_TYPE				0x4
#define QPNP_VADC_PERH_SUBTYPE				0x5

#define QPNP_VADC_SUPPORTED_REVISION2			1

#define QPNP_VADC_STATUS1					0x8
#define QPNP_VADC_STATUS1_OP_MODE				4
#define QPNP_VADC_STATUS1_MEAS_INTERVAL_EN_STS			BIT(2)
#define QPNP_VADC_STATUS1_REQ_STS				BIT(1)
#define QPNP_VADC_STATUS1_EOC					BIT(0)
#define QPNP_VADC_STATUS1_REQ_STS_EOC_MASK			0x3
#define QPNP_VADC_STATUS2					0x9
#define QPNP_VADC_STATUS2_CONV_SEQ_STATE				6
#define QPNP_VADC_STATUS2_FIFO_NOT_EMPTY_FLAG			BIT(1)
#define QPNP_VADC_STATUS2_CONV_SEQ_TIMEOUT_STS			BIT(0)
#define QPNP_VADC_STATUS2_CONV_SEQ_STATE_SHIFT			4
#define QPNP_VADC_CONV_TIMEOUT_ERR				2

#define QPNP_VADC_INT_SET_TYPE					0x11
#define QPNP_VADC_INT_POLARITY_HIGH				0x12
#define QPNP_VADC_INT_POLARITY_LOW				0x13
#define QPNP_VADC_INT_LATCHED_CLR				0x14
#define QPNP_VADC_INT_EN_SET					0x15
#define QPNP_VADC_INT_CLR					0x16
#define QPNP_VADC_INT_LOW_THR_BIT				BIT(4)
#define QPNP_VADC_INT_HIGH_THR_BIT				BIT(3)
#define QPNP_VADC_INT_CONV_SEQ_TIMEOUT_BIT			BIT(2)
#define QPNP_VADC_INT_FIFO_NOT_EMPTY_BIT			BIT(1)
#define QPNP_VADC_INT_EOC_BIT					BIT(0)
#define QPNP_VADC_INT_CLR_MASK					0x1f
#define QPNP_VADC_MODE_CTL					0x40
#define QPNP_VADC_OP_MODE_SHIFT					4
#define QPNP_VADC_VREF_XO_THM_FORCE				BIT(2)
#define QPNP_VADC_AMUX_TRIM_EN					BIT(1)
#define QPNP_VADC_ADC_TRIM_EN					BIT(0)
#define QPNP_VADC_EN_CTL1					0x46
#define QPNP_VADC_ADC_EN					BIT(7)
#define QPNP_VADC_ADC_CH_SEL_CTL					0x48
#define QPNP_VADC_ADC_DIG_PARAM					0x50
#define QPNP_VADC_ADC_DIG_DEC_RATIO_SEL_SHIFT			3
#define QPNP_VADC_HW_SETTLE_DELAY				0x51
#define QPNP_VADC_CONV_REQ					0x52
#define QPNP_VADC_CONV_REQ_SET					BIT(7)
#define QPNP_VADC_CONV_SEQ_CTL					0x54
#define QPNP_VADC_CONV_SEQ_HOLDOFF_SHIFT				4
#define QPNP_VADC_CONV_SEQ_TRIG_CTL				0x55
#define QPNP_VADC_CONV_SEQ_FALLING_EDGE				0x0
#define QPNP_VADC_CONV_SEQ_RISING_EDGE				0x1
#define QPNP_VADC_CONV_SEQ_EDGE_SHIFT				7
#define QPNP_VADC_FAST_AVG_CTL					0x5a

#define QPNP_VADC_M0_LOW_THR_LSB					0x5c
#define QPNP_VADC_M0_LOW_THR_MSB					0x5d
#define QPNP_VADC_M0_HIGH_THR_LSB				0x5e
#define QPNP_VADC_M0_HIGH_THR_MSB				0x5f
#define QPNP_VADC_M1_LOW_THR_LSB					0x69
#define QPNP_VADC_M1_LOW_THR_MSB					0x6a
#define QPNP_VADC_M1_HIGH_THR_LSB				0x6b
#define QPNP_VADC_M1_HIGH_THR_MSB				0x6c

#define QPNP_VADC_DATA0						0x60
#define QPNP_VADC_DATA1						0x61
#define QPNP_VADC_CONV_TIMEOUT_ERR				2
#define QPNP_VADC_CONV_TIME_MIN					2000
#define QPNP_VADC_CONV_TIME_MAX					2100

struct qpnp_vadc_drv {
	struct qpnp_adc_drv		*adc;
	struct dentry			*dent;
	struct device			*vadc_hwmon;
	bool				vadc_init_calib;
	bool				vadc_initialized;
	int				max_channels_available;
	struct sensor_device_attribute		sens_attr[0];
};

struct qpnp_vadc_drv *qpnp_vadc;

static struct qpnp_vadc_scale_fn vadc_scale_fn[] = {
	[SCALE_DEFAULT] = {qpnp_adc_scale_default},
	[SCALE_BATT_THERM] = {qpnp_adc_scale_batt_therm},
	[SCALE_PMIC_THERM] = {qpnp_adc_scale_pmic_therm},
	[SCALE_XOTHERM] = {qpnp_adc_tdkntcg_therm},
};

static int32_t qpnp_vadc_read_reg(int16_t reg, u8 *data)
{
	struct qpnp_vadc_drv *vadc = qpnp_vadc;
	int rc;

	rc = spmi_ext_register_readl(vadc->adc->spmi->ctrl, vadc->adc->slave,
		(vadc->adc->offset + reg), data, 1);
	if (rc < 0) {
		pr_err("qpnp adc read reg %d failed with %d\n", reg, rc);
		return rc;
	}

	return 0;
}

static int32_t qpnp_vadc_write_reg(int16_t reg, u8 data)
{
	struct qpnp_vadc_drv *vadc = qpnp_vadc;
	int rc;
	u8 *buf;

	buf = &data;

	rc = spmi_ext_register_writel(vadc->adc->spmi->ctrl, vadc->adc->slave,
		(vadc->adc->offset + reg), buf, 1);
	if (rc < 0) {
		pr_err("qpnp adc write reg %d failed with %d\n", reg, rc);
		return rc;
	}

	return 0;
}

static int32_t qpnp_vadc_configure_interrupt(void)
{
	int rc = 0;
	u8 data = 0;

	/* Configure interrupt as an Edge trigger */
	rc = qpnp_vadc_write_reg(QPNP_VADC_INT_SET_TYPE,
					QPNP_VADC_INT_CLR_MASK);
	if (rc < 0) {
		pr_err("%s Interrupt configure failed\n", __func__);
		return rc;
	}

	/* Configure interrupt for rising edge trigger */
	rc = qpnp_vadc_write_reg(QPNP_VADC_INT_POLARITY_HIGH,
					QPNP_VADC_INT_CLR_MASK);
	if (rc < 0) {
		pr_err("%s Rising edge trigger configure failed\n", __func__);
		return rc;
	}

	/* Disable low level interrupt triggering */
	data = QPNP_VADC_INT_CLR_MASK;
	rc = qpnp_vadc_write_reg(QPNP_VADC_INT_POLARITY_LOW,
					(~data & QPNP_VADC_INT_CLR_MASK));
	if (rc < 0) {
		pr_err("%s Setting level low to disable failed\n", __func__);
		return rc;
	}

	return 0;
}

static int32_t qpnp_vadc_enable(bool state)
{
	int rc = 0;
	u8 data = 0;

	data = QPNP_VADC_ADC_EN;
	if (state) {
		rc = qpnp_vadc_write_reg(QPNP_VADC_EN_CTL1,
					data);
		if (rc < 0) {
			pr_err("VADC enable failed\n");
			return rc;
		}
	} else {
		rc = qpnp_vadc_write_reg(QPNP_VADC_EN_CTL1,
					(~data & QPNP_VADC_ADC_EN));
		if (rc < 0) {
			pr_err("VADC disable failed\n");
			return rc;
		}
	}

	return 0;
}

int32_t qpnp_vadc_configure(
			struct qpnp_adc_amux_properties *chan_prop)
{
	struct qpnp_vadc_drv *vadc = qpnp_vadc;
	u8 decimation = 0, conv_sequence = 0, conv_sequence_trig = 0;
	u8 mode_ctrl = 0;
	int rc = 0;

	rc = qpnp_vadc_write_reg(QPNP_VADC_INT_EN_SET,
			QPNP_VADC_INT_EOC_BIT);
	if (rc < 0) {
		pr_err("Configure error for interrupt setup\n");
		return rc;
	}

	/* Mode selection */
	mode_ctrl = chan_prop->mode_sel << QPNP_VADC_OP_MODE_SHIFT;
	rc = qpnp_vadc_write_reg(QPNP_VADC_MODE_CTL, mode_ctrl);
	if (rc < 0) {
		pr_err("Mode configure write error\n");
		return rc;
	}


	/* Channel selection */
	rc = qpnp_vadc_write_reg(QPNP_VADC_ADC_CH_SEL_CTL,
						chan_prop->amux_channel);
	if (rc < 0) {
		pr_err("Channel configure error\n");
		return rc;
	}

	/* Digital parameter setup */
	decimation = chan_prop->decimation <<
				QPNP_VADC_ADC_DIG_DEC_RATIO_SEL_SHIFT;
	rc = qpnp_vadc_write_reg(QPNP_VADC_ADC_DIG_PARAM, decimation);
	if (rc < 0) {
		pr_err("Digital parameter configure write error\n");
		return rc;
	}

	/* HW settling time delay */
	rc = qpnp_vadc_write_reg(QPNP_VADC_HW_SETTLE_DELAY,
						chan_prop->hw_settle_time);
	if (rc < 0) {
		pr_err("HW settling time setup error\n");
		return rc;
	}

	if (chan_prop->mode_sel == (ADC_OP_NORMAL_MODE <<
					QPNP_VADC_OP_MODE_SHIFT)) {
		/* Normal measurement mode */
		rc = qpnp_vadc_write_reg(QPNP_VADC_FAST_AVG_CTL,
						chan_prop->fast_avg_setup);
		if (rc < 0) {
			pr_err("Fast averaging configure error\n");
			return rc;
		}
	} else if (chan_prop->mode_sel == (ADC_OP_CONVERSION_SEQUENCER <<
					QPNP_VADC_OP_MODE_SHIFT)) {
		/* Conversion sequence mode */
		conv_sequence = ((ADC_SEQ_HOLD_100US <<
				QPNP_VADC_CONV_SEQ_HOLDOFF_SHIFT) |
				ADC_CONV_SEQ_TIMEOUT_5MS);
		rc = qpnp_vadc_write_reg(QPNP_VADC_CONV_SEQ_CTL,
							conv_sequence);
		if (rc < 0) {
			pr_err("Conversion sequence error\n");
			return rc;
		}

		conv_sequence_trig = ((QPNP_VADC_CONV_SEQ_RISING_EDGE <<
				QPNP_VADC_CONV_SEQ_EDGE_SHIFT) |
				chan_prop->trigger_channel);
		rc = qpnp_vadc_write_reg(QPNP_VADC_CONV_SEQ_TRIG_CTL,
							conv_sequence_trig);
		if (rc < 0) {
			pr_err("Conversion trigger error\n");
			return rc;
		}
	}

	INIT_COMPLETION(vadc->adc->adc_rslt_completion);

	rc = qpnp_vadc_enable(true);
	if (rc)
		return rc;

	/* Request conversion */
	rc = qpnp_vadc_write_reg(QPNP_VADC_CONV_REQ, QPNP_VADC_CONV_REQ_SET);
	if (rc < 0) {
		pr_err("Request conversion failed\n");
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(qpnp_vadc_configure);

static int32_t qpnp_vadc_read_conversion_result(int32_t *data)
{
	uint8_t rslt_lsb, rslt_msb;
	int rc = 0, status = 0;

	status = qpnp_vadc_read_reg(QPNP_VADC_DATA0, &rslt_lsb);
	if (status < 0) {
		pr_err("qpnp adc result read failed for data0\n");
		goto fail;
	}

	status = qpnp_vadc_read_reg(QPNP_VADC_DATA1, &rslt_msb);
	if (status < 0) {
		pr_err("qpnp adc result read failed for data1\n");
		goto fail;
	}

	*data = (rslt_msb << 8) | rslt_lsb;

	status = qpnp_vadc_check_result(data);
	if (status < 0) {
		pr_err("VADC data check failed\n");
		goto fail;
	}

fail:
	rc = qpnp_vadc_enable(false);
	if (rc)
		return rc;

	return status;
}

static int32_t qpnp_vadc_read_status(int mode_sel)
{
	u8 status1, status2, status2_conv_seq_state;
	u8 status_err = QPNP_VADC_CONV_TIMEOUT_ERR;
	int rc;

	switch (mode_sel) {
	case (ADC_OP_CONVERSION_SEQUENCER << QPNP_VADC_OP_MODE_SHIFT):
		rc = qpnp_vadc_read_reg(QPNP_VADC_STATUS1, &status1);
		if (rc) {
			pr_err("qpnp_vadc read mask interrupt failed\n");
			return rc;
		}

		rc = qpnp_vadc_read_reg(QPNP_VADC_STATUS2, &status2);
		if (rc) {
			pr_err("qpnp_vadc read mask interrupt failed\n");
			return rc;
		}

		if (!(status2 & ~QPNP_VADC_STATUS2_CONV_SEQ_TIMEOUT_STS) &&
			(status1 & (~QPNP_VADC_STATUS1_REQ_STS |
						QPNP_VADC_STATUS1_EOC))) {
			rc = status_err;
			return rc;
		}

		status2_conv_seq_state = status2 >>
					QPNP_VADC_STATUS2_CONV_SEQ_STATE_SHIFT;
		if (status2_conv_seq_state != ADC_CONV_SEQ_IDLE) {
			pr_err("qpnp vadc seq error with status %d\n",
						status2);
			rc = -EINVAL;
			return rc;
		}
	}

	return 0;
}

static void qpnp_vadc_work(struct work_struct *work)
{
	struct qpnp_vadc_drv *vadc = qpnp_vadc;
	int rc;

	rc = qpnp_vadc_write_reg(QPNP_VADC_INT_CLR, QPNP_VADC_INT_EOC_BIT);
	if (rc)
		pr_err("qpnp_vadc clear mask interrupt failed with %d\n", rc);

	complete(&vadc->adc->adc_rslt_completion);

	return;
}
DECLARE_WORK(trigger_completion_work, qpnp_vadc_work);

static irqreturn_t qpnp_vadc_isr(int irq, void *dev_id)
{
	schedule_work(&trigger_completion_work);

	return IRQ_HANDLED;
}

static int32_t qpnp_vadc_version_check(void)
{
	uint8_t revision;
	int rc;

	rc = qpnp_vadc_read_reg(QPNP_VADC_REVISION2, &revision);
	if (rc < 0) {
		pr_err("qpnp adc result read failed with %d\n", rc);
		return rc;
	}

	if (revision < QPNP_VADC_SUPPORTED_REVISION2) {
		pr_err("VADC Version not supported\n");
		return -EINVAL;
	}

	return 0;
}

static uint32_t qpnp_vadc_calib_device(void)
{
	struct qpnp_vadc_drv *vadc = qpnp_vadc;
	struct qpnp_adc_amux_properties conv;
	int rc, calib_read_1, calib_read_2;
	u8 status1 = 0;

	conv.amux_channel = REF_125V;
	conv.decimation = DECIMATION_TYPE2;
	conv.mode_sel = ADC_OP_NORMAL_MODE << QPNP_VADC_OP_MODE_SHIFT;
	conv.hw_settle_time = ADC_CHANNEL_HW_SETTLE_DELAY_0US;
	conv.fast_avg_setup = ADC_FAST_AVG_SAMPLE_1;

	rc = qpnp_vadc_configure(&conv);
	if (rc) {
		pr_err("qpnp_vadc configure failed with %d\n", rc);
		goto calib_fail;
	}

	while (status1 != QPNP_VADC_STATUS1_EOC) {
		rc = qpnp_vadc_read_reg(QPNP_VADC_STATUS1, &status1);
		if (rc < 0)
			return rc;
		status1 &= QPNP_VADC_STATUS1_REQ_STS_EOC_MASK;
		usleep_range(QPNP_VADC_CONV_TIME_MIN,
					QPNP_VADC_CONV_TIME_MAX);
	}

	rc = qpnp_vadc_read_conversion_result(&calib_read_1);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		goto calib_fail;
	}

	conv.amux_channel = REF_625MV;
	conv.decimation = DECIMATION_TYPE2;
	conv.mode_sel = ADC_OP_NORMAL_MODE << QPNP_VADC_OP_MODE_SHIFT;
	conv.hw_settle_time = ADC_CHANNEL_HW_SETTLE_DELAY_0US;
	conv.fast_avg_setup = ADC_FAST_AVG_SAMPLE_1;
	rc = qpnp_vadc_configure(&conv);
	if (rc) {
		pr_err("qpnp adc configure failed with %d\n", rc);
		goto calib_fail;
	}

	status1 = 0;
	while (status1 != QPNP_VADC_STATUS1_EOC) {
		rc = qpnp_vadc_read_reg(QPNP_VADC_STATUS1, &status1);
		if (rc < 0)
			return rc;
		status1 &= QPNP_VADC_STATUS1_REQ_STS_EOC_MASK;
		usleep_range(QPNP_VADC_CONV_TIME_MIN,
					QPNP_VADC_CONV_TIME_MAX);
	}

	rc = qpnp_vadc_read_conversion_result(&calib_read_2);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		goto calib_fail;
	}

	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_ABSOLUTE].dy =
					(calib_read_1 - calib_read_2);

	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_ABSOLUTE].dx
						= QPNP_ADC_625_UV;
	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_ABSOLUTE].adc_vref =
					calib_read_1;
	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_ABSOLUTE].adc_gnd =
					calib_read_2;
	/* Ratiometric Calibration */
	conv.amux_channel = VDD_VADC;
	conv.decimation = DECIMATION_TYPE2;
	conv.mode_sel = ADC_OP_NORMAL_MODE << QPNP_VADC_OP_MODE_SHIFT;
	conv.hw_settle_time = ADC_CHANNEL_HW_SETTLE_DELAY_0US;
	conv.fast_avg_setup = ADC_FAST_AVG_SAMPLE_1;
	rc = qpnp_vadc_configure(&conv);
	if (rc) {
		pr_err("qpnp adc configure failed with %d\n", rc);
		goto calib_fail;
	}

	status1 = 0;
	while (status1 != QPNP_VADC_STATUS1_EOC) {
		rc = qpnp_vadc_read_reg(QPNP_VADC_STATUS1, &status1);
		if (rc < 0)
			return rc;
		status1 &= QPNP_VADC_STATUS1_REQ_STS_EOC_MASK;
		usleep_range(QPNP_VADC_CONV_TIME_MIN,
					QPNP_VADC_CONV_TIME_MAX);
	}

	rc = qpnp_vadc_read_conversion_result(&calib_read_1);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		goto calib_fail;
	}

	conv.amux_channel = GND_REF;
	conv.decimation = DECIMATION_TYPE2;
	conv.mode_sel = ADC_OP_NORMAL_MODE << QPNP_VADC_OP_MODE_SHIFT;
	conv.hw_settle_time = ADC_CHANNEL_HW_SETTLE_DELAY_0US;
	conv.fast_avg_setup = ADC_FAST_AVG_SAMPLE_1;
	rc = qpnp_vadc_configure(&conv);
	if (rc) {
		pr_err("qpnp adc configure failed with %d\n", rc);
		goto calib_fail;
	}

	status1 = 0;
	while (status1 != QPNP_VADC_STATUS1_EOC) {
		rc = qpnp_vadc_read_reg(QPNP_VADC_STATUS1, &status1);
		if (rc < 0)
			return rc;
		status1 &= QPNP_VADC_STATUS1_REQ_STS_EOC_MASK;
		usleep_range(QPNP_VADC_CONV_TIME_MIN,
					QPNP_VADC_CONV_TIME_MAX);
	}

	rc = qpnp_vadc_read_conversion_result(&calib_read_2);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		goto calib_fail;
	}

	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_RATIOMETRIC].dy =
					(calib_read_1 - calib_read_2);
	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_RATIOMETRIC].dx =
					vadc->adc->adc_prop->adc_vdd_reference;
	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_RATIOMETRIC].adc_vref =
					calib_read_1;
	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_RATIOMETRIC].adc_gnd =
					calib_read_2;

calib_fail:
	return rc;
}

int32_t qpnp_vadc_is_ready(void)
{
	struct qpnp_vadc_drv *vadc = qpnp_vadc;

	if (!vadc || !vadc->vadc_initialized)
		return -EPROBE_DEFER;
	else
		return 0;
}
EXPORT_SYMBOL(qpnp_vadc_is_ready);

int32_t qpnp_vadc_conv_seq_request(enum qpnp_vadc_trigger trigger_channel,
					enum qpnp_vadc_channels channel,
					struct qpnp_vadc_result *result)
{
	struct qpnp_vadc_drv *vadc = qpnp_vadc;
	int rc = 0, scale_type, amux_prescaling, dt_index = 0;

	if (!vadc || !vadc->vadc_initialized)
		return -EPROBE_DEFER;

	if (!vadc->vadc_init_calib) {
		rc = qpnp_vadc_version_check();
		if (rc)
			return rc;
		rc = qpnp_vadc_calib_device();
		if (rc) {
			pr_err("Calibration failed\n");
			return rc;
		} else
			vadc->vadc_init_calib = true;
	}

	mutex_lock(&vadc->adc->adc_lock);

	vadc->adc->amux_prop->amux_channel = channel;

	while (vadc->adc->adc_channels[dt_index].channel_num
			!= channel || dt_index > vadc->max_channels_available)
		dt_index++;

	if (dt_index > vadc->max_channels_available)
		goto fail_unlock;

	vadc->adc->amux_prop->decimation =
			vadc->adc->adc_channels[dt_index].adc_decimation;
	vadc->adc->amux_prop->hw_settle_time =
			vadc->adc->adc_channels[dt_index].hw_settle_time;
	vadc->adc->amux_prop->fast_avg_setup =
			vadc->adc->adc_channels[dt_index].fast_avg_setup;

	if (trigger_channel < ADC_SEQ_NONE)
		vadc->adc->amux_prop->mode_sel = (ADC_OP_CONVERSION_SEQUENCER
						<< QPNP_VADC_OP_MODE_SHIFT);
	else if (trigger_channel == ADC_SEQ_NONE)
		vadc->adc->amux_prop->mode_sel = (ADC_OP_NORMAL_MODE
						<< QPNP_VADC_OP_MODE_SHIFT);
	else {
		pr_err("Invalid trigger channel:%d\n", trigger_channel);
		goto fail_unlock;
	}

	vadc->adc->amux_prop->trigger_channel = trigger_channel;

	rc = qpnp_vadc_configure(vadc->adc->amux_prop);
	if (rc) {
		pr_err("qpnp vadc configure failed with %d\n", rc);
		goto fail_unlock;
	}

	wait_for_completion(&vadc->adc->adc_rslt_completion);

	if (trigger_channel < ADC_SEQ_NONE) {
		rc = qpnp_vadc_read_status(vadc->adc->amux_prop->mode_sel);
		if (rc)
			pr_debug("Conversion sequence timed out - %d\n", rc);
	}

	rc = qpnp_vadc_read_conversion_result(&result->adc_code);
	if (rc) {
		pr_err("qpnp vadc read adc code failed with %d\n", rc);
		goto fail_unlock;
	}

	amux_prescaling =
		vadc->adc->adc_channels[dt_index].chan_path_prescaling;

	vadc->adc->amux_prop->chan_prop->offset_gain_numerator =
		qpnp_vadc_amux_scaling_ratio[amux_prescaling].num;
	vadc->adc->amux_prop->chan_prop->offset_gain_denominator =
		 qpnp_vadc_amux_scaling_ratio[amux_prescaling].den;

	scale_type = vadc->adc->adc_channels[dt_index].adc_scale_fn;
	if (scale_type >= SCALE_NONE) {
		rc = -EBADF;
		goto fail_unlock;
	}

	vadc_scale_fn[scale_type].chan(result->adc_code,
		vadc->adc->adc_prop, vadc->adc->amux_prop->chan_prop, result);

fail_unlock:
	mutex_unlock(&vadc->adc->adc_lock);

	return rc;
}
EXPORT_SYMBOL(qpnp_vadc_conv_seq_request);

int32_t qpnp_vadc_read(enum qpnp_vadc_channels channel,
				struct qpnp_vadc_result *result)
{
	return qpnp_vadc_conv_seq_request(ADC_SEQ_NONE,
				channel, result);
}
EXPORT_SYMBOL_GPL(qpnp_vadc_read);

static ssize_t qpnp_adc_show(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct qpnp_vadc_result result;
	int rc = -1;

	rc = qpnp_vadc_read(attr->index, &result);

	if (rc) {
		pr_err("VADC read error with %d\n", rc);
		return 0;
	}

	return snprintf(buf, QPNP_ADC_HWMON_NAME_LENGTH,
		"Result:%lld Raw:%d\n", result.physical, result.adc_code);
}

static struct sensor_device_attribute qpnp_adc_attr =
	SENSOR_ATTR(NULL, S_IRUGO, qpnp_adc_show, NULL, 0);

static int32_t qpnp_vadc_init_hwmon(struct spmi_device *spmi)
{
	struct qpnp_vadc_drv *vadc = qpnp_vadc;
	struct device_node *child;
	struct device_node *node = spmi->dev.of_node;
	int rc = 0, i = 0, channel;

	for_each_child_of_node(node, child) {
		channel = vadc->adc->adc_channels[i].channel_num;
		qpnp_adc_attr.index = vadc->adc->adc_channels[i].channel_num;
		qpnp_adc_attr.dev_attr.attr.name =
						vadc->adc->adc_channels[i].name;
		sysfs_attr_init(&vadc->sens_attr[i].dev_attr.attr);
		memcpy(&vadc->sens_attr[i], &qpnp_adc_attr,
						sizeof(qpnp_adc_attr));
		rc = device_create_file(&spmi->dev,
				&vadc->sens_attr[i].dev_attr);
		if (rc) {
			dev_err(&spmi->dev,
				"device_create_file failed for dev %s\n",
				vadc->adc->adc_channels[i].name);
			goto hwmon_err_sens;
		}
		i++;
	}

	return 0;
hwmon_err_sens:
	pr_err("Init HWMON failed for qpnp_adc with %d\n", rc);
	return rc;
}

static int __devinit qpnp_vadc_probe(struct spmi_device *spmi)
{
	struct qpnp_vadc_drv *vadc;
	struct qpnp_adc_drv *adc_qpnp;
	struct device_node *node = spmi->dev.of_node;
	struct device_node *child;
	int rc, count_adc_channel_list = 0;

	if (!node)
		return -EINVAL;

	if (qpnp_vadc) {
		pr_err("VADC already in use\n");
		return -EBUSY;
	}

	for_each_child_of_node(node, child)
		count_adc_channel_list++;

	if (!count_adc_channel_list) {
		pr_err("No channel listing\n");
		return -EINVAL;
	}

	vadc = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_vadc_drv) +
		(sizeof(struct sensor_device_attribute) *
				count_adc_channel_list), GFP_KERNEL);
	if (!vadc) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	adc_qpnp = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_adc_drv),
			GFP_KERNEL);
	if (!adc_qpnp) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	vadc->adc = adc_qpnp;

	rc = qpnp_adc_get_devicetree_data(spmi, vadc->adc);
	if (rc) {
		dev_err(&spmi->dev, "failed to read device tree\n");
		return rc;
	}

	rc = devm_request_irq(&spmi->dev, vadc->adc->adc_irq,
				qpnp_vadc_isr, IRQF_TRIGGER_RISING,
				"qpnp_vadc_interrupt", vadc);
	if (rc) {
		dev_err(&spmi->dev,
			"failed to request adc irq with error %d\n", rc);
		return rc;
	} else {
		enable_irq_wake(vadc->adc->adc_irq);
	}

	qpnp_vadc = vadc;
	dev_set_drvdata(&spmi->dev, vadc);
	rc = qpnp_vadc_init_hwmon(spmi);
	if (rc) {
		dev_err(&spmi->dev, "failed to initialize qpnp hwmon adc\n");
		goto fail_free_irq;
	}
	vadc->vadc_hwmon = hwmon_device_register(&vadc->adc->spmi->dev);
	vadc->vadc_init_calib = false;
	vadc->vadc_initialized = true;
	vadc->max_channels_available = count_adc_channel_list;

	rc = qpnp_vadc_configure_interrupt();
	if (rc) {
		dev_err(&spmi->dev, "failed to configure interrupt");
		goto fail_free_irq;
	}

	return 0;

fail_free_irq:
	free_irq(vadc->adc->adc_irq, vadc);

	return rc;
}

static int __devexit qpnp_vadc_remove(struct spmi_device *spmi)
{
	struct qpnp_vadc_drv *vadc = dev_get_drvdata(&spmi->dev);
	struct device_node *node = spmi->dev.of_node;
	struct device_node *child;
	int i = 0;

	for_each_child_of_node(node, child) {
		device_remove_file(&spmi->dev,
			&vadc->sens_attr[i].dev_attr);
		i++;
	}
	free_irq(vadc->adc->adc_irq, vadc);
	vadc->vadc_initialized = false;
	dev_set_drvdata(&spmi->dev, NULL);

	return 0;
}

static const struct of_device_id qpnp_vadc_match_table[] = {
	{	.compatible = "qcom,qpnp-vadc",
	},
	{}
};

static struct spmi_driver qpnp_vadc_driver = {
	.driver		= {
		.name	= "qcom,qpnp-vadc",
		.of_match_table = qpnp_vadc_match_table,
	},
	.probe		= qpnp_vadc_probe,
	.remove		= qpnp_vadc_remove,
};

static int __init qpnp_vadc_init(void)
{
	return spmi_driver_register(&qpnp_vadc_driver);
}
module_init(qpnp_vadc_init);

static void __exit qpnp_vadc_exit(void)
{
	spmi_driver_unregister(&qpnp_vadc_driver);
}
module_exit(qpnp_vadc_exit);

MODULE_DESCRIPTION("QPNP PMIC Voltage ADC driver");
MODULE_LICENSE("GPL v2");
