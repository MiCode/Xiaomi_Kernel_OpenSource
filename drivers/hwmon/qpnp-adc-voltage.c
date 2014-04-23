/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#define QPNP_VADC_ACCESS					0xd0
#define QPNP_VADC_ACCESS_DATA					0xa5
#define QPNP_VADC_PERH_RESET_CTL3				0xda
#define QPNP_FOLLOW_OTST2_RB					BIT(3)
#define QPNP_FOLLOW_WARM_RB					BIT(2)
#define QPNP_FOLLOW_SHUTDOWN1_RB				BIT(1)
#define QPNP_FOLLOW_SHUTDOWN2_RB				BIT(0)

#define QPNP_INT_TEST_VAL					0xE1

#define QPNP_VADC_DATA0						0x60
#define QPNP_VADC_DATA1						0x61
#define QPNP_VADC_CONV_TIMEOUT_ERR				2
#define QPNP_VADC_CONV_TIME_MIN					2000
#define QPNP_VADC_CONV_TIME_MAX					2100
#define QPNP_ADC_COMPLETION_TIMEOUT				HZ
#define QPNP_VADC_ERR_COUNT					20

struct qpnp_vadc_chip {
	struct device			*dev;
	struct qpnp_adc_drv		*adc;
	struct list_head		list;
	struct dentry			*dent;
	struct device			*vadc_hwmon;
	bool				vadc_init_calib;
	int				max_channels_available;
	bool				vadc_iadc_sync_lock;
	u8				id;
	struct work_struct		trigger_completion_work;
	bool				vadc_poll_eoc;
	u8				revision_ana_minor;
	u8				revision_dig_major;
	struct sensor_device_attribute	sens_attr[0];
};

LIST_HEAD(qpnp_vadc_device_list);

static struct qpnp_vadc_scale_fn vadc_scale_fn[] = {
	[SCALE_DEFAULT] = {qpnp_adc_scale_default},
	[SCALE_BATT_THERM] = {qpnp_adc_scale_batt_therm},
	[SCALE_PMIC_THERM] = {qpnp_adc_scale_pmic_therm},
	[SCALE_XOTHERM] = {qpnp_adc_tdkntcg_therm},
	[SCALE_THERM_100K_PULLUP] = {qpnp_adc_scale_therm_pu2},
	[SCALE_THERM_150K_PULLUP] = {qpnp_adc_scale_therm_pu1},
	[SCALE_QRD_BATT_THERM] = {qpnp_adc_scale_qrd_batt_therm},
	[SCALE_QRD_SKUAA_BATT_THERM] = {qpnp_adc_scale_qrd_skuaa_batt_therm},
	[SCALE_QRD_SKUG_BATT_THERM] = {qpnp_adc_scale_qrd_skug_batt_therm},
};

static int32_t qpnp_vadc_read_reg(struct qpnp_vadc_chip *vadc, int16_t reg,
								u8 *data)
{
	int rc;

	rc = spmi_ext_register_readl(vadc->adc->spmi->ctrl, vadc->adc->slave,
		(vadc->adc->offset + reg), data, 1);
	if (rc < 0) {
		pr_err("qpnp adc read reg %d failed with %d\n", reg, rc);
		return rc;
	}

	return 0;
}

static int32_t qpnp_vadc_write_reg(struct qpnp_vadc_chip *vadc, int16_t reg,
								u8 data)
{
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

static int32_t qpnp_vadc_warm_rst_configure(struct qpnp_vadc_chip *vadc)
{
	int rc = 0;
	u8 data = 0;

	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_ACCESS, QPNP_VADC_ACCESS_DATA);
	if (rc < 0) {
		pr_err("VADC write access failed\n");
		return rc;
	}

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_PERH_RESET_CTL3, &data);
	if (rc < 0) {
		pr_err("VADC perh reset ctl3 read failed\n");
		return rc;
	}

	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_ACCESS, QPNP_VADC_ACCESS_DATA);
	if (rc < 0) {
		pr_err("VADC write access failed\n");
		return rc;
	}

	data |= QPNP_FOLLOW_WARM_RB;

	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_PERH_RESET_CTL3, data);
	if (rc < 0) {
		pr_err("VADC perh reset ctl3 write failed\n");
		return rc;
	}

	return 0;
}

static int32_t qpnp_vadc_enable(struct qpnp_vadc_chip *vadc, bool state)
{
	int rc = 0;
	u8 data = 0;

	data = QPNP_VADC_ADC_EN;
	if (state) {
		rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_EN_CTL1,
					data);
		if (rc < 0) {
			pr_err("VADC enable failed\n");
			return rc;
		}
	} else {
		rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_EN_CTL1,
					(~data & QPNP_VADC_ADC_EN));
		if (rc < 0) {
			pr_err("VADC disable failed\n");
			return rc;
		}
	}

	return 0;
}

static int32_t qpnp_vadc_status_debug(struct qpnp_vadc_chip *vadc)
{
	int rc = 0;
	u8 mode = 0, status1 = 0, chan = 0, dig = 0, en = 0, status2 = 0;

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_MODE_CTL, &mode);
	if (rc < 0) {
		pr_err("mode ctl register read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_ADC_DIG_PARAM, &dig);
	if (rc < 0) {
		pr_err("digital param read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_ADC_CH_SEL_CTL, &chan);
	if (rc < 0) {
		pr_err("channel read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS1, &status1);
	if (rc < 0) {
		pr_err("status1 read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS2, &status2);
	if (rc < 0) {
		pr_err("status2 read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_EN_CTL1, &en);
	if (rc < 0) {
		pr_err("en read failed with %d\n", rc);
		return rc;
	}

	pr_err("EOC not set - status1/2:%x/%x, dig:%x, ch:%x, mode:%x, en:%x\n",
			status1, status2, dig, chan, mode, en);

	rc = qpnp_vadc_enable(vadc, false);
	if (rc < 0) {
		pr_err("VADC disable failed with %d\n", rc);
		return rc;
	}

	return 0;
}
static int32_t qpnp_vadc_configure(struct qpnp_vadc_chip *vadc,
			struct qpnp_adc_amux_properties *chan_prop)
{
	u8 decimation = 0, conv_sequence = 0, conv_sequence_trig = 0;
	u8 mode_ctrl = 0;
	int rc = 0;

	/* Mode selection */
	mode_ctrl |= ((chan_prop->mode_sel << QPNP_VADC_OP_MODE_SHIFT) |
			(QPNP_VADC_ADC_TRIM_EN | QPNP_VADC_AMUX_TRIM_EN));
	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_MODE_CTL, mode_ctrl);
	if (rc < 0) {
		pr_err("Mode configure write error\n");
		return rc;
	}


	/* Channel selection */
	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_ADC_CH_SEL_CTL,
						chan_prop->amux_channel);
	if (rc < 0) {
		pr_err("Channel configure error\n");
		return rc;
	}

	/* Digital parameter setup */
	decimation = chan_prop->decimation <<
				QPNP_VADC_ADC_DIG_DEC_RATIO_SEL_SHIFT;
	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_ADC_DIG_PARAM, decimation);
	if (rc < 0) {
		pr_err("Digital parameter configure write error\n");
		return rc;
	}

	/* HW settling time delay */
	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_HW_SETTLE_DELAY,
						chan_prop->hw_settle_time);
	if (rc < 0) {
		pr_err("HW settling time setup error\n");
		return rc;
	}

	if (chan_prop->mode_sel == (ADC_OP_NORMAL_MODE <<
					QPNP_VADC_OP_MODE_SHIFT)) {
		/* Normal measurement mode */
		rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_FAST_AVG_CTL,
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
		rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_CONV_SEQ_CTL,
							conv_sequence);
		if (rc < 0) {
			pr_err("Conversion sequence error\n");
			return rc;
		}

		conv_sequence_trig = ((QPNP_VADC_CONV_SEQ_RISING_EDGE <<
				QPNP_VADC_CONV_SEQ_EDGE_SHIFT) |
				chan_prop->trigger_channel);
		rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_CONV_SEQ_TRIG_CTL,
							conv_sequence_trig);
		if (rc < 0) {
			pr_err("Conversion trigger error\n");
			return rc;
		}
	}

	if (!vadc->vadc_poll_eoc)
		INIT_COMPLETION(vadc->adc->adc_rslt_completion);

	rc = qpnp_vadc_enable(vadc, true);
	if (rc)
		return rc;

	if (!vadc->vadc_iadc_sync_lock) {
		/* Request conversion */
		rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_CONV_REQ,
					QPNP_VADC_CONV_REQ_SET);
		if (rc < 0) {
			pr_err("Request conversion failed\n");
			return rc;
		}
	}

	return 0;
}

static int32_t qpnp_vadc_read_conversion_result(struct qpnp_vadc_chip *vadc,
								int32_t *data)
{
	uint8_t rslt_lsb, rslt_msb;
	int rc = 0, status = 0;

	status = qpnp_vadc_read_reg(vadc, QPNP_VADC_DATA0, &rslt_lsb);
	if (status < 0) {
		pr_err("qpnp adc result read failed for data0\n");
		goto fail;
	}

	status = qpnp_vadc_read_reg(vadc, QPNP_VADC_DATA1, &rslt_msb);
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
	rc = qpnp_vadc_enable(vadc, false);
	if (rc)
		return rc;

	return status;
}

static int32_t qpnp_vadc_read_status(struct qpnp_vadc_chip *vadc, int mode_sel)
{
	u8 status1, status2, status2_conv_seq_state;
	u8 status_err = QPNP_VADC_CONV_TIMEOUT_ERR;
	int rc;

	switch (mode_sel) {
	case (ADC_OP_CONVERSION_SEQUENCER << QPNP_VADC_OP_MODE_SHIFT):
		rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS1, &status1);
		if (rc) {
			pr_err("qpnp_vadc read mask interrupt failed\n");
			return rc;
		}

		rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS2, &status2);
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

static int qpnp_vadc_is_valid(struct qpnp_vadc_chip *vadc)
{
	struct qpnp_vadc_chip *vadc_chip = NULL;

	list_for_each_entry(vadc_chip, &qpnp_vadc_device_list, list)
		if (vadc == vadc_chip)
			return 0;

	return -EINVAL;
}

static void qpnp_vadc_work(struct work_struct *work)
{
	struct qpnp_vadc_chip *vadc = container_of(work,
			struct qpnp_vadc_chip, trigger_completion_work);

	if (qpnp_vadc_is_valid(vadc) < 0)
		return;

	complete(&vadc->adc->adc_rslt_completion);

	return;
}

static irqreturn_t qpnp_vadc_isr(int irq, void *dev_id)
{
	struct qpnp_vadc_chip *vadc = dev_id;

	schedule_work(&vadc->trigger_completion_work);

	return IRQ_HANDLED;
}

static int32_t qpnp_vadc_version_check(struct qpnp_vadc_chip *dev)
{
	uint8_t revision;
	int rc;

	rc = qpnp_vadc_read_reg(dev, QPNP_VADC_REVISION2, &revision);
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

#define QPNP_VBAT_COEFF_1	3000
#define QPNP_VBAT_COEFF_2	45810000
#define QPNP_VBAT_COEFF_3	100000
#define QPNP_VBAT_COEFF_4	3500
#define QPNP_VBAT_COEFF_5	80000000
#define QPNP_VBAT_COEFF_6	4400
#define QPNP_VBAT_COEFF_7	32200000
#define QPNP_VBAT_COEFF_8	3880
#define QPNP_VBAT_COEFF_9	5770
#define QPNP_VBAT_COEFF_10	3660
#define QPNP_VBAT_COEFF_11	5320
#define QPNP_VBAT_COEFF_12	8060000
#define QPNP_VBAT_COEFF_13	102640000
#define QPNP_VBAT_COEFF_14	22220000
#define QPNP_VBAT_COEFF_15	83060000
#define QPNP_VBAT_COEFF_16	2810
#define QPNP_VBAT_COEFF_17	5260
#define QPNP_VBAT_COEFF_18	8027
#define QPNP_VBAT_COEFF_19	2347
#define QPNP_VBAT_COEFF_20	6043
#define QPNP_VBAT_COEFF_21	1914
#define QPNP_VBAT_OFFSET_SMIC	9446
#define QPNP_VBAT_OFFSET_GF	9441
#define QPNP_OCV_OFFSET_SMIC	4596
#define QPNP_OCV_OFFSET_GF	5896

static int32_t qpnp_ocv_comp(int64_t *result,
			struct qpnp_vadc_chip *vadc, int64_t die_temp)
{
	int64_t temp_var = 0;
	int64_t old = *result;
	int version;

	version = qpnp_adc_get_revid_version(vadc->dev);
	if (version == -EINVAL)
		return 0;

	if (version == QPNP_REV_ID_8110_2_0) {
		if (die_temp < -20000)
			die_temp = -20000;
	} else {
		if (die_temp < 25000)
			return 0;
		if (die_temp > 60000)
			die_temp = 60000;
	}

	switch (version) {
	case QPNP_REV_ID_8941_3_1:
		switch (vadc->id) {
		case COMP_ID_TSMC:
			temp_var = (((die_temp *
			(-QPNP_VBAT_COEFF_4))
			+ QPNP_VBAT_COEFF_5));
			break;
		default:
		case COMP_ID_GF:
			temp_var = (((die_temp *
			(-QPNP_VBAT_COEFF_1))
			+ QPNP_VBAT_COEFF_2));
			break;
		}
		break;
	case QPNP_REV_ID_8026_1_0:
		switch (vadc->id) {
		case COMP_ID_TSMC:
			temp_var = (((die_temp *
			(-QPNP_VBAT_COEFF_10))
			- QPNP_VBAT_COEFF_14));
			break;
		default:
		case COMP_ID_GF:
			temp_var = (((die_temp *
			(-QPNP_VBAT_COEFF_8))
			+ QPNP_VBAT_COEFF_12));
			break;
		}
		break;
	case QPNP_REV_ID_8026_2_0:
	case QPNP_REV_ID_8026_2_1:
		switch (vadc->id) {
		case COMP_ID_TSMC:
			temp_var = ((die_temp - 25000) *
			(-QPNP_VBAT_COEFF_10));
			break;
		default:
		case COMP_ID_GF:
			temp_var = ((die_temp - 25000) *
			(-QPNP_VBAT_COEFF_8));
			break;
		}
		break;
	case QPNP_REV_ID_8110_2_0:
		switch (vadc->id) {
		case COMP_ID_SMIC:
			*result -= QPNP_OCV_OFFSET_SMIC;
			if (die_temp < 25000)
				temp_var = QPNP_VBAT_COEFF_18;
			else
				temp_var = QPNP_VBAT_COEFF_19;
			temp_var = (die_temp - 25000) * temp_var;
			break;
		case COMP_ID_TSMC:
			pr_debug("No TSMC Comp Info, exiting\n");
			return 0;
		default:
		case COMP_ID_GF:
			*result -= QPNP_OCV_OFFSET_GF;
			if (die_temp < 25000)
				temp_var = QPNP_VBAT_COEFF_20;
			else
				temp_var = QPNP_VBAT_COEFF_21;
			temp_var = (die_temp - 25000) * temp_var;
			break;
		}
		break;
	default:
		temp_var = 0;
		break;
	}

	temp_var = div64_s64(temp_var, QPNP_VBAT_COEFF_3);

	temp_var = 1000000 + temp_var;

	*result = *result * temp_var;

	*result = div64_s64(*result, 1000000);
	pr_debug("%lld compensated into %lld\n", old, *result);

	return 0;
}

static int32_t qpnp_vbat_sns_comp(int64_t *result,
			struct qpnp_vadc_chip *vadc, int64_t die_temp)
{
	int64_t temp_var = 0;
	int64_t old = *result;
	int version;

	version = qpnp_adc_get_revid_version(vadc->dev);
	if (version == -EINVAL)
		return 0;

	if (version == QPNP_REV_ID_8110_2_0) {
		if (die_temp < -20000)
			die_temp = -20000;
	} else {
		if (die_temp < 25000)
			return 0;
		/* min(die_temp_c, 60_degC) */
		if (die_temp > 60000)
			die_temp = 60000;
	}

	switch (version) {
	case QPNP_REV_ID_8941_3_1:
		switch (vadc->id) {
		case COMP_ID_TSMC:
			temp_var = (die_temp *
			(-QPNP_VBAT_COEFF_1));
			break;
		default:
		case COMP_ID_GF:
			temp_var = (((die_temp *
			(-QPNP_VBAT_COEFF_6))
			+ QPNP_VBAT_COEFF_7));
			break;
		}
		break;
	case QPNP_REV_ID_8026_1_0:
		switch (vadc->id) {
		case COMP_ID_TSMC:
			temp_var = (((die_temp *
			(-QPNP_VBAT_COEFF_11))
			+ QPNP_VBAT_COEFF_15));
			break;
		default:
		case COMP_ID_GF:
			temp_var = (((die_temp *
			(-QPNP_VBAT_COEFF_9))
			+ QPNP_VBAT_COEFF_13));
			break;
		}
		break;
	case QPNP_REV_ID_8026_2_0:
	case QPNP_REV_ID_8026_2_1:
		switch (vadc->id) {
		case COMP_ID_TSMC:
			temp_var = ((die_temp - 25000) *
			(-QPNP_VBAT_COEFF_11));
			break;
		default:
		case COMP_ID_GF:
			temp_var = ((die_temp - 25000) *
			(-QPNP_VBAT_COEFF_9));
			break;
		}
		break;
	case QPNP_REV_ID_8110_2_0:
		switch (vadc->id) {
		case COMP_ID_SMIC:
			*result -= QPNP_VBAT_OFFSET_SMIC;
			temp_var = ((die_temp - 25000) *
			(QPNP_VBAT_COEFF_17));
			break;
		case COMP_ID_TSMC:
			pr_debug("No TSMC Comp Info, exiting\n");
			return 0;
		default:
		case COMP_ID_GF:
			*result -= QPNP_VBAT_OFFSET_GF;
			temp_var = ((die_temp - 25000) *
			(QPNP_VBAT_COEFF_16));
			break;
		}
		break;
	default:
		temp_var = 0;
		break;
	}

	temp_var = div64_s64(temp_var, QPNP_VBAT_COEFF_3);

	temp_var = 1000000 + temp_var;

	*result = *result * temp_var;

	*result = div64_s64(*result, 1000000);
	pr_debug("%lld compensated into %lld\n", old, *result);

	return 0;
}

int32_t qpnp_vbat_sns_comp_result(struct qpnp_vadc_chip *vadc,
					int64_t *result, bool is_pon_ocv)
{
	struct qpnp_vadc_result die_temp_result;
	int rc = 0;

	rc = qpnp_vadc_is_valid(vadc);
	if (rc < 0)
		return rc;

	rc = qpnp_vadc_conv_seq_request(vadc, ADC_SEQ_NONE,
			DIE_TEMP, &die_temp_result);
	if (rc < 0) {
		pr_err("Error reading die_temp\n");
		return rc;
	}

	if (is_pon_ocv)
		rc = qpnp_ocv_comp(result, vadc, die_temp_result.physical);
	else
		rc = qpnp_vbat_sns_comp(result, vadc,
				die_temp_result.physical);

	if (rc < 0)
		pr_err("Error with vbat compensation\n");

	return rc;
}
EXPORT_SYMBOL(qpnp_vbat_sns_comp_result);

static void qpnp_vadc_625mv_channel_sel(struct qpnp_vadc_chip *vadc,
				uint32_t *ref_channel_sel)
{
	uint32_t dt_index = 0;

	/* Check if the buffered 625mV channel exists */
	while ((vadc->adc->adc_channels[dt_index].channel_num
		!= SPARE1) && (dt_index < vadc->max_channels_available))
		dt_index++;

	if (dt_index >= vadc->max_channels_available) {
		pr_debug("Use default 625mV ref channel\n");
		*ref_channel_sel = REF_625MV;
	} else {
		pr_debug("Use buffered 625mV ref channel\n");
		*ref_channel_sel = SPARE1;
	}
}

static int32_t qpnp_vadc_calib_device(struct qpnp_vadc_chip *vadc)
{
	struct qpnp_adc_amux_properties conv;
	int rc, calib_read_1, calib_read_2, count = 0;
	u8 status1 = 0;
	uint32_t ref_channel_sel = 0;

	conv.amux_channel = REF_125V;
	conv.decimation = DECIMATION_TYPE2;
	conv.mode_sel = ADC_OP_NORMAL_MODE << QPNP_VADC_OP_MODE_SHIFT;
	conv.hw_settle_time = ADC_CHANNEL_HW_SETTLE_DELAY_0US;
	conv.fast_avg_setup = ADC_FAST_AVG_SAMPLE_1;

	rc = qpnp_vadc_configure(vadc, &conv);
	if (rc) {
		pr_err("qpnp_vadc configure failed with %d\n", rc);
		goto calib_fail;
	}

	while (status1 != QPNP_VADC_STATUS1_EOC) {
		rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS1, &status1);
		if (rc < 0)
			return rc;
		status1 &= QPNP_VADC_STATUS1_REQ_STS_EOC_MASK;
		usleep_range(QPNP_VADC_CONV_TIME_MIN,
					QPNP_VADC_CONV_TIME_MAX);
		count++;
		if (count > QPNP_VADC_ERR_COUNT) {
			rc = -ENODEV;
			goto calib_fail;
		}
	}

	rc = qpnp_vadc_read_conversion_result(vadc, &calib_read_1);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		goto calib_fail;
	}

	qpnp_vadc_625mv_channel_sel(vadc, &ref_channel_sel);
	conv.amux_channel = ref_channel_sel;
	conv.decimation = DECIMATION_TYPE2;
	conv.mode_sel = ADC_OP_NORMAL_MODE << QPNP_VADC_OP_MODE_SHIFT;
	conv.hw_settle_time = ADC_CHANNEL_HW_SETTLE_DELAY_0US;
	conv.fast_avg_setup = ADC_FAST_AVG_SAMPLE_1;
	rc = qpnp_vadc_configure(vadc, &conv);
	if (rc) {
		pr_err("qpnp adc configure failed with %d\n", rc);
		goto calib_fail;
	}

	status1 = 0;
	count = 0;
	while (status1 != QPNP_VADC_STATUS1_EOC) {
		rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS1, &status1);
		if (rc < 0)
			return rc;
		status1 &= QPNP_VADC_STATUS1_REQ_STS_EOC_MASK;
		usleep_range(QPNP_VADC_CONV_TIME_MIN,
					QPNP_VADC_CONV_TIME_MAX);
		count++;
		if (count > QPNP_VADC_ERR_COUNT) {
			rc = -ENODEV;
			goto calib_fail;
		}
	}

	rc = qpnp_vadc_read_conversion_result(vadc, &calib_read_2);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		goto calib_fail;
	}

	pr_debug("absolute reference raw: 625mV:0x%x 1.25V:0x%x\n",
				calib_read_2, calib_read_1);

	if (calib_read_1 == calib_read_2) {
		pr_err("absolute reference raw: 625mV:0x%x 1.25V:0x%x\n",
				calib_read_2, calib_read_1);
		rc = -EINVAL;
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
	rc = qpnp_vadc_configure(vadc, &conv);
	if (rc) {
		pr_err("qpnp adc configure failed with %d\n", rc);
		goto calib_fail;
	}

	status1 = 0;
	count = 0;
	while (status1 != QPNP_VADC_STATUS1_EOC) {
		rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS1, &status1);
		if (rc < 0)
			return rc;
		status1 &= QPNP_VADC_STATUS1_REQ_STS_EOC_MASK;
		usleep_range(QPNP_VADC_CONV_TIME_MIN,
					QPNP_VADC_CONV_TIME_MAX);
		count++;
		if (count > QPNP_VADC_ERR_COUNT) {
			rc = -ENODEV;
			goto calib_fail;
		}
	}

	rc = qpnp_vadc_read_conversion_result(vadc, &calib_read_1);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		goto calib_fail;
	}

	conv.amux_channel = GND_REF;
	conv.decimation = DECIMATION_TYPE2;
	conv.mode_sel = ADC_OP_NORMAL_MODE << QPNP_VADC_OP_MODE_SHIFT;
	conv.hw_settle_time = ADC_CHANNEL_HW_SETTLE_DELAY_0US;
	conv.fast_avg_setup = ADC_FAST_AVG_SAMPLE_1;
	rc = qpnp_vadc_configure(vadc, &conv);
	if (rc) {
		pr_err("qpnp adc configure failed with %d\n", rc);
		goto calib_fail;
	}

	status1 = 0;
	count = 0;
	while (status1 != QPNP_VADC_STATUS1_EOC) {
		rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS1, &status1);
		if (rc < 0)
			return rc;
		status1 &= QPNP_VADC_STATUS1_REQ_STS_EOC_MASK;
		usleep_range(QPNP_VADC_CONV_TIME_MIN,
					QPNP_VADC_CONV_TIME_MAX);
		count++;
		if (count > QPNP_VADC_ERR_COUNT) {
			rc = -ENODEV;
			goto calib_fail;
		}
	}

	rc = qpnp_vadc_read_conversion_result(vadc, &calib_read_2);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		goto calib_fail;
	}

	pr_debug("ratiometric reference raw: VDD:0x%x GND:0x%x\n",
				calib_read_1, calib_read_2);

	if (calib_read_1 == calib_read_2) {
		pr_err("ratiometric reference raw: VDD:0x%x GND:0x%x\n",
				calib_read_1, calib_read_2);
		rc = -EINVAL;
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

int32_t qpnp_get_vadc_gain_and_offset(struct qpnp_vadc_chip *vadc,
				struct qpnp_vadc_linear_graph *param,
				enum qpnp_adc_calib_type calib_type)
{
	int rc = 0;

	rc = qpnp_vadc_is_valid(vadc);
	if (rc < 0)
		return rc;

	switch (calib_type) {
	case CALIB_RATIOMETRIC:
	param->dy =
	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_RATIOMETRIC].dy;
	param->dx =
	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_RATIOMETRIC].dx;
	param->adc_vref = vadc->adc->adc_prop->adc_vdd_reference;
	param->adc_gnd =
	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_RATIOMETRIC].adc_gnd;
	break;
	case CALIB_ABSOLUTE:
	param->dy =
	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_ABSOLUTE].dy;
	param->dx =
	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_ABSOLUTE].dx;
	param->adc_vref = vadc->adc->adc_prop->adc_vdd_reference;
	param->adc_gnd =
	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_ABSOLUTE].adc_gnd;
	break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(qpnp_get_vadc_gain_and_offset);

struct qpnp_vadc_chip *qpnp_get_vadc(struct device *dev, const char *name)
{
	struct qpnp_vadc_chip *vadc;
	struct device_node *node = NULL;
	char prop_name[QPNP_MAX_PROP_NAME_LEN];

	snprintf(prop_name, QPNP_MAX_PROP_NAME_LEN, "qcom,%s-vadc", name);

	node = of_parse_phandle(dev->of_node, prop_name, 0);
	if (node == NULL)
		return ERR_PTR(-ENODEV);

	list_for_each_entry(vadc, &qpnp_vadc_device_list, list)
		if (vadc->adc->spmi->dev.of_node == node)
			return vadc;
	return ERR_PTR(-EPROBE_DEFER);
}
EXPORT_SYMBOL(qpnp_get_vadc);

int32_t qpnp_vadc_conv_seq_request(struct qpnp_vadc_chip *vadc,
				enum qpnp_vadc_trigger trigger_channel,
					enum qpnp_vadc_channels channel,
					struct qpnp_vadc_result *result)
{
	int rc = 0, scale_type, amux_prescaling, dt_index = 0;
	uint32_t ref_channel, count = 0;
	u8 status1 = 0;

	if (qpnp_vadc_is_valid(vadc))
		return -EPROBE_DEFER;

	mutex_lock(&vadc->adc->adc_lock);

	if (vadc->vadc_poll_eoc) {
		pr_debug("requesting vadc eoc stay awake\n");
		pm_stay_awake(vadc->dev);
	}

	if (!vadc->vadc_init_calib) {
		rc = qpnp_vadc_version_check(vadc);
		if (rc)
			goto fail_unlock;

		rc = qpnp_vadc_calib_device(vadc);
		if (rc) {
			pr_err("Calibration failed\n");
			goto fail_unlock;
		} else
			vadc->vadc_init_calib = true;
	}

	if (channel == REF_625MV) {
		qpnp_vadc_625mv_channel_sel(vadc, &ref_channel);
		channel = ref_channel;
	}

	vadc->adc->amux_prop->amux_channel = channel;

	while ((vadc->adc->adc_channels[dt_index].channel_num
		!= channel) && (dt_index < vadc->max_channels_available))
		dt_index++;

	if (dt_index >= vadc->max_channels_available) {
		pr_err("not a valid VADC channel\n");
		rc = -EINVAL;
		goto fail_unlock;
	}

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

	rc = qpnp_vadc_configure(vadc, vadc->adc->amux_prop);
	if (rc) {
		pr_err("qpnp vadc configure failed with %d\n", rc);
		goto fail_unlock;
	}

	if (vadc->vadc_poll_eoc) {
		while (status1 != QPNP_VADC_STATUS1_EOC) {
			rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS1,
								&status1);
			if (rc < 0)
				goto fail_unlock;
			status1 &= QPNP_VADC_STATUS1_REQ_STS_EOC_MASK;
			usleep_range(QPNP_VADC_CONV_TIME_MIN,
					QPNP_VADC_CONV_TIME_MAX);
			count++;
			if (count > QPNP_VADC_ERR_COUNT) {
				pr_err("retry error exceeded\n");
				rc = qpnp_vadc_status_debug(vadc);
				if (rc < 0)
					pr_err("VADC disable failed\n");
				rc = -EINVAL;
				goto fail_unlock;
			}
		}
	} else {
		rc = wait_for_completion_timeout(
					&vadc->adc->adc_rslt_completion,
					QPNP_ADC_COMPLETION_TIMEOUT);
		if (!rc) {
			rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS1,
								&status1);
			if (rc < 0)
				goto fail_unlock;
			status1 &= QPNP_VADC_STATUS1_REQ_STS_EOC_MASK;
			if (status1 == QPNP_VADC_STATUS1_EOC)
				pr_debug("End of conversion status set\n");
			else {
				rc = qpnp_vadc_status_debug(vadc);
				if (rc < 0)
					pr_err("VADC disable failed\n");
				rc = -EINVAL;
				goto fail_unlock;
			}
		}
	}

	if (trigger_channel < ADC_SEQ_NONE) {
		rc = qpnp_vadc_read_status(vadc,
					vadc->adc->amux_prop->mode_sel);
		if (rc)
			pr_debug("Conversion sequence timed out - %d\n", rc);
	}

	rc = qpnp_vadc_read_conversion_result(vadc, &result->adc_code);
	if (rc) {
		pr_err("qpnp vadc read adc code failed with %d\n", rc);
		goto fail_unlock;
	}

	amux_prescaling =
		vadc->adc->adc_channels[dt_index].chan_path_prescaling;

	if (amux_prescaling >= PATH_SCALING_NONE) {
		rc = -EINVAL;
		goto fail_unlock;
	}

	vadc->adc->amux_prop->chan_prop->offset_gain_numerator =
		qpnp_vadc_amux_scaling_ratio[amux_prescaling].num;
	vadc->adc->amux_prop->chan_prop->offset_gain_denominator =
		 qpnp_vadc_amux_scaling_ratio[amux_prescaling].den;

	scale_type = vadc->adc->adc_channels[dt_index].adc_scale_fn;
	if (scale_type >= SCALE_NONE) {
		rc = -EBADF;
		goto fail_unlock;
	}

	vadc_scale_fn[scale_type].chan(vadc, result->adc_code,
		vadc->adc->adc_prop, vadc->adc->amux_prop->chan_prop, result);

fail_unlock:
	if (vadc->vadc_poll_eoc) {
		pr_debug("requesting vadc eoc stay awake\n");
		pm_relax(vadc->dev);
	}

	mutex_unlock(&vadc->adc->adc_lock);

	return rc;
}
EXPORT_SYMBOL(qpnp_vadc_conv_seq_request);

int32_t qpnp_vadc_read(struct qpnp_vadc_chip *vadc,
				enum qpnp_vadc_channels channel,
				struct qpnp_vadc_result *result)
{
	struct qpnp_vadc_result die_temp_result;
	int rc = 0;

	if (channel == VBAT_SNS) {
		rc = qpnp_vadc_conv_seq_request(vadc, ADC_SEQ_NONE,
				channel, result);
		if (rc < 0) {
			pr_err("Error reading vbatt\n");
			return rc;
		}

		rc = qpnp_vadc_conv_seq_request(vadc, ADC_SEQ_NONE,
				DIE_TEMP, &die_temp_result);
		if (rc < 0) {
			pr_err("Error reading die_temp\n");
			return rc;
		}

		rc = qpnp_vbat_sns_comp(&result->physical, vadc,
						die_temp_result.physical);
		if (rc < 0)
			pr_err("Error with vbat compensation\n");

		return 0;
	} else
		return qpnp_vadc_conv_seq_request(vadc, ADC_SEQ_NONE,
				channel, result);
}
EXPORT_SYMBOL(qpnp_vadc_read);

static void qpnp_vadc_lock(struct qpnp_vadc_chip *vadc)
{
	mutex_lock(&vadc->adc->adc_lock);
}

static void qpnp_vadc_unlock(struct qpnp_vadc_chip *vadc)
{
	mutex_unlock(&vadc->adc->adc_lock);
}

int32_t qpnp_vadc_iadc_sync_request(struct qpnp_vadc_chip *vadc,
				enum qpnp_vadc_channels channel)
{
	int rc = 0, dt_index = 0;

	if (qpnp_vadc_is_valid(vadc))
		return -EPROBE_DEFER;

	qpnp_vadc_lock(vadc);

	if (!vadc->vadc_init_calib) {
		rc = qpnp_vadc_version_check(vadc);
		if (rc)
			goto fail;

		rc = qpnp_vadc_calib_device(vadc);
		if (rc) {
			pr_err("Calibration failed\n");
			goto fail;
		} else
			vadc->vadc_init_calib = true;
	}

	vadc->adc->amux_prop->amux_channel = channel;

	while ((vadc->adc->adc_channels[dt_index].channel_num
		!= channel) && (dt_index < vadc->max_channels_available))
		dt_index++;

	if (dt_index >= vadc->max_channels_available) {
		pr_err("not a valid VADC channel\n");
		rc = -EINVAL;
		goto fail;
	}

	vadc->adc->amux_prop->decimation =
			vadc->adc->adc_channels[dt_index].adc_decimation;
	vadc->adc->amux_prop->hw_settle_time =
			vadc->adc->adc_channels[dt_index].hw_settle_time;
	vadc->adc->amux_prop->fast_avg_setup =
			vadc->adc->adc_channels[dt_index].fast_avg_setup;
	vadc->adc->amux_prop->mode_sel = (ADC_OP_NORMAL_MODE
					<< QPNP_VADC_OP_MODE_SHIFT);
	vadc->vadc_iadc_sync_lock = true;

	rc = qpnp_vadc_configure(vadc, vadc->adc->amux_prop);
	if (rc) {
		pr_err("qpnp vadc configure failed with %d\n", rc);
		goto fail;
	}

	return rc;
fail:
	vadc->vadc_iadc_sync_lock = false;
	qpnp_vadc_unlock(vadc);
	return rc;
}
EXPORT_SYMBOL(qpnp_vadc_iadc_sync_request);

int32_t qpnp_vadc_iadc_sync_complete_request(struct qpnp_vadc_chip *vadc,
					enum qpnp_vadc_channels channel,
						struct qpnp_vadc_result *result)
{
	int rc = 0, scale_type, amux_prescaling, dt_index = 0;

	vadc->adc->amux_prop->amux_channel = channel;

	while ((vadc->adc->adc_channels[dt_index].channel_num
		!= channel) && (dt_index < vadc->max_channels_available))
		dt_index++;

	rc = qpnp_vadc_read_conversion_result(vadc, &result->adc_code);
	if (rc) {
		pr_err("qpnp vadc read adc code failed with %d\n", rc);
		goto fail;
	}

	amux_prescaling =
		vadc->adc->adc_channels[dt_index].chan_path_prescaling;

	if (amux_prescaling >= PATH_SCALING_NONE) {
		rc = -EINVAL;
		goto fail;
	}

	vadc->adc->amux_prop->chan_prop->offset_gain_numerator =
		qpnp_vadc_amux_scaling_ratio[amux_prescaling].num;
	vadc->adc->amux_prop->chan_prop->offset_gain_denominator =
		 qpnp_vadc_amux_scaling_ratio[amux_prescaling].den;

	scale_type = vadc->adc->adc_channels[dt_index].adc_scale_fn;
	if (scale_type >= SCALE_NONE) {
		rc = -EBADF;
		goto fail;
	}

	vadc_scale_fn[scale_type].chan(vadc, result->adc_code,
		vadc->adc->adc_prop, vadc->adc->amux_prop->chan_prop, result);

fail:
	vadc->vadc_iadc_sync_lock = false;
	qpnp_vadc_unlock(vadc);
	return rc;
}
EXPORT_SYMBOL(qpnp_vadc_iadc_sync_complete_request);

static ssize_t qpnp_adc_show(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct qpnp_vadc_chip *vadc = dev_get_drvdata(dev);
	struct qpnp_vadc_result result;
	int rc = -1;

	rc = qpnp_vadc_read(vadc, attr->index, &result);

	if (rc) {
		pr_err("VADC read error with %d\n", rc);
		return 0;
	}

	return snprintf(buf, QPNP_ADC_HWMON_NAME_LENGTH,
		"Result:%lld Raw:%d\n", result.physical, result.adc_code);
}

static struct sensor_device_attribute qpnp_adc_attr =
	SENSOR_ATTR(NULL, S_IRUGO, qpnp_adc_show, NULL, 0);

static int32_t qpnp_vadc_init_hwmon(struct qpnp_vadc_chip *vadc,
					struct spmi_device *spmi)
{
	struct device_node *child;
	struct device_node *node = spmi->dev.of_node;
	int rc = 0, i = 0, channel;

	for_each_child_of_node(node, child) {
		channel = vadc->adc->adc_channels[i].channel_num;
		qpnp_adc_attr.index = vadc->adc->adc_channels[i].channel_num;
		qpnp_adc_attr.dev_attr.attr.name =
						vadc->adc->adc_channels[i].name;
		memcpy(&vadc->sens_attr[i], &qpnp_adc_attr,
						sizeof(qpnp_adc_attr));
		sysfs_attr_init(&vadc->sens_attr[i].dev_attr.attr);
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
	struct qpnp_vadc_chip *vadc;
	struct qpnp_adc_drv *adc_qpnp;
	struct device_node *node = spmi->dev.of_node;
	struct device_node *child;
	int rc, count_adc_channel_list = 0, i = 0;
	u8 fab_id = 0;

	for_each_child_of_node(node, child)
		count_adc_channel_list++;

	if (!count_adc_channel_list) {
		pr_err("No channel listing\n");
		return -EINVAL;
	}

	vadc = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_vadc_chip) +
		(sizeof(struct sensor_device_attribute) *
				count_adc_channel_list), GFP_KERNEL);
	if (!vadc) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	vadc->dev = &(spmi->dev);
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
	mutex_init(&vadc->adc->adc_lock);

	rc = qpnp_vadc_init_hwmon(vadc, spmi);
	if (rc) {
		dev_err(&spmi->dev, "failed to initialize qpnp hwmon adc\n");
		return rc;
	}
	vadc->vadc_hwmon = hwmon_device_register(&vadc->adc->spmi->dev);
	vadc->vadc_init_calib = false;
	vadc->max_channels_available = count_adc_channel_list;
	rc = qpnp_vadc_read_reg(vadc, QPNP_INT_TEST_VAL, &fab_id);
	if (rc < 0) {
		pr_err("qpnp adc comp id failed with %d\n", rc);
		goto err_setup;
	}
	vadc->id = fab_id;

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_REVISION2,
					&vadc->revision_dig_major);
	if (rc < 0) {
		pr_err("qpnp adc dig_major rev read failed with %d\n", rc);
		goto err_setup;
	}

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_REVISION3,
					&vadc->revision_ana_minor);
	if (rc < 0) {
		pr_err("qpnp adc ana_minor rev read failed with %d\n", rc);
		goto err_setup;
	}

	rc = qpnp_vadc_warm_rst_configure(vadc);
	if (rc < 0) {
		pr_err("Setting perp reset on warm reset failed %d\n", rc);
		goto err_setup;
	}

	INIT_WORK(&vadc->trigger_completion_work, qpnp_vadc_work);

	vadc->vadc_poll_eoc = of_property_read_bool(node,
						"qcom,vadc-poll-eoc");
	if (!vadc->vadc_poll_eoc) {
		rc = devm_request_irq(&spmi->dev, vadc->adc->adc_irq_eoc,
				qpnp_vadc_isr, IRQF_TRIGGER_RISING,
				"qpnp_vadc_interrupt", vadc);
		if (rc) {
			dev_err(&spmi->dev,
			"failed to request adc irq with error %d\n", rc);
			goto err_setup;
		} else {
			enable_irq_wake(vadc->adc->adc_irq_eoc);
		}
	} else
		device_init_wakeup(vadc->dev, 1);

	vadc->vadc_iadc_sync_lock = false;
	dev_set_drvdata(&spmi->dev, vadc);
	list_add(&vadc->list, &qpnp_vadc_device_list);

	return 0;

err_setup:
	for_each_child_of_node(node, child) {
		device_remove_file(&spmi->dev,
			&vadc->sens_attr[i].dev_attr);
		i++;
	}
	hwmon_device_unregister(vadc->vadc_hwmon);

	return rc;
}

static int __devexit qpnp_vadc_remove(struct spmi_device *spmi)
{
	struct qpnp_vadc_chip *vadc = dev_get_drvdata(&spmi->dev);
	struct device_node *node = spmi->dev.of_node;
	struct device_node *child;
	int i = 0;

	for_each_child_of_node(node, child) {
		device_remove_file(&spmi->dev,
			&vadc->sens_attr[i].dev_attr);
		i++;
	}
	hwmon_device_unregister(vadc->vadc_hwmon);
	list_del(&vadc->list);
	if (vadc->vadc_poll_eoc)
		pm_relax(vadc->dev);
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
