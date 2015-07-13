/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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
#include <linux/power_supply.h>
#include <linux/thermal.h>

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
#define QPNP_VADC_STATUS1_OP_MODE_MASK				0x18
#define QPNP_VADC_MEAS_INT_MODE					0x2
#define QPNP_VADC_MEAS_INT_MODE_MASK				0x10

#define QPNP_VADC_STATUS2					0x9
#define QPNP_VADC_STATUS2_CONV_SEQ_STATE				6
#define QPNP_VADC_STATUS2_FIFO_NOT_EMPTY_FLAG			BIT(1)
#define QPNP_VADC_STATUS2_CONV_SEQ_TIMEOUT_STS			BIT(0)
#define QPNP_VADC_STATUS2_CONV_SEQ_STATE_SHIFT			4
#define QPNP_VADC_CONV_TIMEOUT_ERR				2

#define QPNP_VADC_MODE_CTL					0x40
#define QPNP_VADC_OP_MODE_SHIFT					3
#define QPNP_VADC_VREF_XO_THM_FORCE				BIT(2)
#define QPNP_VADC_AMUX_TRIM_EN					BIT(1)
#define QPNP_VADC_TRIM_EN					BIT(0)
#define QPNP_VADC_EN_CTL1					0x46
#define QPNP_VADC_EN						BIT(7)
#define QPNP_VADC_CH_SEL_CTL					0x48
#define QPNP_VADC_DIG_PARAM					0x50
#define QPNP_VADC_DIG_DEC_RATIO_SEL_SHIFT			3
#define QPNP_VADC_HW_SETTLE_DELAY				0x51
#define QPNP_VADC_CONV_REQ					0x52
#define QPNP_VADC_CONV_REQ_SET					BIT(7)
#define QPNP_VADC_CONV_SEQ_CTL					0x54
#define QPNP_VADC_CONV_SEQ_HOLDOFF_SHIFT				4
#define QPNP_VADC_CONV_SEQ_TRIG_CTL				0x55
#define QPNP_VADC_MEAS_INTERVAL_CTL				0x57
#define QPNP_VADC_MEAS_INTERVAL_OP_CTL				0x59
#define QPNP_VADC_MEAS_INTERVAL_OP_SET				BIT(7)

#define QPNP_VADC_CONV_SEQ_FALLING_EDGE				0x0
#define QPNP_VADC_CONV_SEQ_RISING_EDGE				0x1
#define QPNP_VADC_CONV_SEQ_EDGE_SHIFT				7
#define QPNP_VADC_FAST_AVG_CTL					0x5a

#define QPNP_VADC_LOW_THR_LSB					0x5c
#define QPNP_VADC_LOW_THR_MSB					0x5d
#define QPNP_VADC_HIGH_THR_LSB					0x5e
#define QPNP_VADC_HIGH_THR_MSB					0x5f
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
#define QPNP_VADC_CONV_TIME_MIN					1000
#define QPNP_VADC_CONV_TIME_MAX					1100
#define QPNP_ADC_COMPLETION_TIMEOUT				HZ
#define QPNP_VADC_ERR_COUNT					20
#define QPNP_OP_MODE_SHIFT					3

#define QPNP_VADC_THR_LSB_MASK(val)				(val & 0xff)
#define QPNP_VADC_THR_MSB_MASK(val)			((val & 0xff00) >> 8)
#define QPNP_MIN_TIME						2000
#define QPNP_MAX_TIME						2000
#define QPNP_RETRY						100
#define QPNP_VADC_ABSOLUTE_RECALIB_OFFSET			8
#define QPNP_VADC_RATIOMETRIC_RECALIB_OFFSET			12
#define QPNP_VADC_RECALIB_MAXCNT				10

struct qpnp_vadc_mode_state {
	bool				meas_int_mode;
	bool				meas_int_request_in_queue;
	bool				vadc_meas_int_enable;
	struct qpnp_adc_tm_btm_param	*param;
	struct qpnp_adc_amux		vadc_meas_amux;
};

struct qpnp_vadc_thermal_data {
	bool thermal_node;
	int thermal_chan;
	enum qpnp_vadc_channels vadc_channel;
	struct thermal_zone_device *tz_dev;
	struct qpnp_vadc_chip *vadc_dev;
};

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
	bool				vadc_recalib_check;
	u8				revision_ana_minor;
	u8				revision_dig_major;
	struct workqueue_struct		*high_thr_wq;
	struct workqueue_struct		*low_thr_wq;
	struct work_struct		trigger_high_thr_work;
	struct work_struct		trigger_low_thr_work;
	struct qpnp_vadc_mode_state	*state_copy;
	struct qpnp_vadc_thermal_data	*vadc_therm_chan;
	struct power_supply		*vadc_chg_vote;
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
	[SCALE_SMB_BATT_THERM] = {qpnp_adc_scale_smb_batt_therm},
	[SCALE_QRD_SKUG_BATT_THERM] = {qpnp_adc_scale_qrd_skug_batt_therm},
	[SCALE_QRD_SKUH_BATT_THERM] = {qpnp_adc_scale_qrd_skuh_batt_therm},
	[SCALE_NCP_03WF683_THERM] = {qpnp_adc_scale_therm_ncp03},
	[SCALE_QRD_SKUT1_BATT_THERM] = {qpnp_adc_scale_qrd_skut1_batt_therm},
	[SCALE_QRD_SKUC_BATT_THERM] = {qpnp_adc_scale_qrd_skuc_batt_therm},
	[SCALE_QRD_SKUE_BATT_THERM] = {qpnp_adc_scale_qrd_skue_batt_therm},
	[SCALE_QRD_SKUL_BATT_THERM] = {qpnp_adc_scale_qrd_skul_batt_therm},
	[SCALE_PMI_CHG_TEMP] = {qpnp_adc_scale_pmi_chg_temp},
};

static struct qpnp_vadc_rscale_fn adc_vadc_rscale_fn[] = {
	[SCALE_RVADC_ABSOLUTE] = {qpnp_vadc_absolute_rthr},
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

	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_ACCESS,
						QPNP_VADC_ACCESS_DATA);
	if (rc < 0) {
		pr_err("VADC write access failed\n");
		return rc;
	}

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_PERH_RESET_CTL3, &data);
	if (rc < 0) {
		pr_err("VADC perh reset ctl3 read failed\n");
		return rc;
	}

	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_ACCESS,
						QPNP_VADC_ACCESS_DATA);
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

static int32_t qpnp_vadc_mode_select(struct qpnp_vadc_chip *vadc, u8 mode_ctl)
{
	int rc;

	mode_ctl |= (QPNP_VADC_TRIM_EN | QPNP_VADC_AMUX_TRIM_EN);

	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_MODE_CTL, mode_ctl);
	if (rc < 0)
		pr_err("vadc write mode selection err:%d\n", rc);

	return rc;
}

static int32_t qpnp_vadc_enable(struct qpnp_vadc_chip *vadc, bool state)
{
	int rc = 0;
	u8 data = 0;

	data = QPNP_VADC_EN;
	if (state) {
		if (vadc->adc->hkadc_ldo && vadc->adc->hkadc_ldo_ok) {
			rc = qpnp_adc_enable_voltage(vadc->adc);
			if (rc) {
				pr_err("failed enabling VADC LDO\n");
				return rc;
			}
		}

		rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_EN_CTL1,
					data);
		if (rc < 0) {
			pr_err("VADC enable failed\n");
			return rc;
		}
	} else {
		rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_EN_CTL1,
					(~data & QPNP_VADC_EN));
		if (rc < 0) {
			pr_err("VADC disable failed\n");
			return rc;
		}

		if (vadc->adc->hkadc_ldo && vadc->adc->hkadc_ldo_ok)
			qpnp_adc_disable_voltage(vadc->adc);
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

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_DIG_PARAM, &dig);
	if (rc < 0) {
		pr_err("digital param read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_CH_SEL_CTL, &chan);
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
	u8 mode_ctrl = 0, meas_int_op_ctl_data = 0;
	int rc = 0;

	/* Mode selection */
	mode_ctrl |= ((chan_prop->mode_sel << QPNP_VADC_OP_MODE_SHIFT) |
			(QPNP_VADC_TRIM_EN | QPNP_VADC_AMUX_TRIM_EN));
	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_MODE_CTL, mode_ctrl);
	if (rc < 0) {
		pr_err("Mode configure write error\n");
		return rc;
	}

	/* Channel selection */
	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_CH_SEL_CTL,
						chan_prop->amux_channel);
	if (rc < 0) {
		pr_err("Channel configure error\n");
		return rc;
	}

	/* Digital parameter setup */
	decimation = chan_prop->decimation <<
				QPNP_VADC_DIG_DEC_RATIO_SEL_SHIFT;
	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_DIG_PARAM, decimation);
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

	pr_debug("mode:%d, channel:%d, decimation:%d, hw_settle:%d\n",
		mode_ctrl, chan_prop->amux_channel, decimation,
					chan_prop->hw_settle_time);

	if (chan_prop->mode_sel == (ADC_OP_NORMAL_MODE <<
					QPNP_VADC_OP_MODE_SHIFT)) {
		/* Normal measurement mode */
		rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_FAST_AVG_CTL,
						chan_prop->fast_avg_setup);
		if (rc < 0) {
			pr_err("Fast averaging configure error\n");
			return rc;
		}
		/* Ensure MEAS_INTERVAL_OP_CTL is set to 0 */
		rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_MEAS_INTERVAL_OP_CTL,
						meas_int_op_ctl_data);
		if (rc < 0) {
			pr_err("Measurement interval OP configure error\n");
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
	} else if (chan_prop->mode_sel == ADC_OP_MEASUREMENT_INTERVAL) {
		rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_MEAS_INTERVAL_OP_CTL,
					QPNP_VADC_MEAS_INTERVAL_OP_SET);
		if (rc < 0) {
			pr_err("Measurement interval OP configure error\n");
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

static void qpnp_vadc_low_thr_fn(struct work_struct *work)
{
	struct qpnp_vadc_chip *vadc = container_of(work,
			struct qpnp_vadc_chip, trigger_low_thr_work);

	vadc->state_copy->meas_int_mode = false;
	vadc->state_copy->meas_int_request_in_queue = false;
	vadc->state_copy->param->threshold_notification(
			ADC_TM_LOW_STATE,
			vadc->state_copy->param->btm_ctx);

	return;
}

static void qpnp_vadc_high_thr_fn(struct work_struct *work)
{
	struct qpnp_vadc_chip *vadc = container_of(work,
			struct qpnp_vadc_chip, trigger_high_thr_work);

	vadc->state_copy->meas_int_mode = false;
	vadc->state_copy->meas_int_request_in_queue = false;
	vadc->state_copy->param->threshold_notification(
			ADC_TM_HIGH_STATE,
			vadc->state_copy->param->btm_ctx);

	return;
}
static irqreturn_t qpnp_vadc_isr(int irq, void *dev_id)
{
	struct qpnp_vadc_chip *vadc = dev_id;

	schedule_work(&vadc->trigger_completion_work);

	return IRQ_HANDLED;
}

static irqreturn_t qpnp_vadc_low_thr_isr(int irq, void *data)
{
	struct qpnp_vadc_chip *vadc = data;
	u8 mode_ctl = 0, mode = 0;
	int rc = 0;

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_MODE_CTL, &mode);
	if (rc < 0) {
		pr_err("mode ctl register read failed with %d\n", rc);
		return rc;
	}

	if (!(mode & QPNP_VADC_MEAS_INT_MODE_MASK)) {
		pr_debug("Spurious VADC threshold 0x%x\n", mode);
		return IRQ_HANDLED;
	}

	mode_ctl = ADC_OP_NORMAL_MODE;
	/* Set measurement in single measurement mode */
	qpnp_vadc_mode_select(vadc, mode_ctl);
	qpnp_vadc_enable(vadc, false);
	schedule_work(&vadc->trigger_low_thr_work);

	return IRQ_HANDLED;
}

static irqreturn_t qpnp_vadc_high_thr_isr(int irq, void *data)
{
	struct qpnp_vadc_chip *vadc = data;
	u8 mode_ctl = 0, mode = 0;
	int rc = 0;

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_MODE_CTL, &mode);
	if (rc < 0) {
		pr_err("mode ctl register read failed with %d\n", rc);
		return rc;
	}

	if (!(mode & QPNP_VADC_MEAS_INT_MODE_MASK)) {
		pr_debug("Spurious VADC threshold 0x%x\n", mode);
		return IRQ_HANDLED;
	}

	mode_ctl = ADC_OP_NORMAL_MODE;
	/* Set measurement in single measurement mode */
	qpnp_vadc_mode_select(vadc, mode_ctl);
	qpnp_vadc_enable(vadc, false);
	schedule_work(&vadc->trigger_high_thr_work);

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

static int32_t
	qpnp_vadc_channel_post_scaling_calib_check(struct qpnp_vadc_chip *vadc,
								int channel)
{
	int version, rc = 0;

	version = qpnp_adc_get_revid_version(vadc->dev);

	if (version == QPNP_REV_ID_PM8950_1_0) {
		if ((channel == LR_MUX7_HW_ID) ||
			(channel == P_MUX2_1_1) ||
			(channel == LR_MUX3_XO_THERM) ||
			(channel == LR_MUX3_BUF_XO_THERM_BUF) ||
			(channel == P_MUX4_1_1)) {
			vadc->adc->amux_prop->chan_prop->calib_type =
								CALIB_ABSOLUTE;
			return rc;
		}
	}

	return -EINVAL;
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
#define QPNP_VBAT_COEFF_22	6800
#define QPNP_VBAT_COEFF_23	3500
#define QPNP_VBAT_COEFF_24	4360
#define QPNP_VBAT_COEFF_25	8060
#define QPNP_VBAT_COEFF_26	7895
#define QPNP_VBAT_COEFF_27	5658
#define QPNP_VBAT_COEFF_28	5760
#define QPNP_VBAT_COEFF_29	7900
#define QPNP_VBAT_COEFF_30	5660
#define QPNP_VBAT_COEFF_31	3620
#define QPNP_VBAT_COEFF_32	1230
#define QPNP_VBAT_COEFF_33	5760
#define QPNP_VBAT_COEFF_34	4080
#define QPNP_VBAT_COEFF_35	7000
#define QPNP_VBAT_COEFF_36	3040
#define QPNP_VBAT_COEFF_37	3850
#define QPNP_VBAT_COEFF_38	5000
#define QPNP_VBAT_COEFF_39	2610
#define QPNP_VBAT_COEFF_40	4190
#define QPNP_VBAT_COEFF_41	5800
#define QPNP_VBAT_COEFF_42	2620
#define QPNP_VBAT_COEFF_43	4030
#define QPNP_VBAT_COEFF_44	3230
#define QPNP_VBAT_COEFF_45	3450
#define QPNP_VBAT_COEFF_46	2120
#define QPNP_VBAT_COEFF_47	3560
#define QPNP_VBAT_COEFF_48	2190
#define QPNP_VBAT_COEFF_49	4180
#define QPNP_VBAT_COEFF_50	27800000
#define QPNP_VBAT_COEFF_51	5110
#define QPNP_VBAT_COEFF_52	34444000

static int32_t qpnp_ocv_comp(int64_t *result,
			struct qpnp_vadc_chip *vadc, int64_t die_temp)
{
	int64_t temp_var = 0, offset = 0;
	int64_t old = *result;
	int version;

	version = qpnp_adc_get_revid_version(vadc->dev);
	if (version == -EINVAL)
		return 0;

	if (version == QPNP_REV_ID_8026_2_2) {
		if (die_temp > 25000)
			return 0;
	}

	switch (version) {
	case QPNP_REV_ID_8941_3_1:
		switch (vadc->id) {
		case COMP_ID_TSMC:
			 temp_var = ((die_temp - 25000) *
			(-QPNP_VBAT_COEFF_4));
			break;
		default:
		case COMP_ID_GF:
			temp_var = ((die_temp - 25000) *
			(-QPNP_VBAT_COEFF_1));
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
	case QPNP_REV_ID_8026_2_2:
		switch (vadc->id) {
		case COMP_ID_TSMC:
			*result -= QPNP_VBAT_COEFF_22;
			temp_var = (die_temp - 25000) *
					QPNP_VBAT_COEFF_24;
			break;
		default:
		case COMP_ID_GF:
			*result -= QPNP_VBAT_COEFF_22;
			temp_var = (die_temp - 25000) *
					QPNP_VBAT_COEFF_25;
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
	case QPNP_REV_ID_8916_1_0:
		switch (vadc->id) {
		case COMP_ID_SMIC:
			if (die_temp < 25000)
				temp_var = QPNP_VBAT_COEFF_26;
			else
				temp_var = QPNP_VBAT_COEFF_27;
			temp_var = (die_temp - 25000) * temp_var;
			break;
		default:
		case COMP_ID_GF:
			offset = QPNP_OCV_OFFSET_GF;
			if (die_temp < 25000)
				temp_var = QPNP_VBAT_COEFF_26;
			else
				temp_var = QPNP_VBAT_COEFF_27;
			temp_var = (die_temp - 25000) * temp_var;
			break;
		}
		break;
	case QPNP_REV_ID_8916_1_1:
		switch (vadc->id) {
		/* FAB_ID is zero */
		case COMP_ID_GF:
			if (die_temp < 25000)
				temp_var = QPNP_VBAT_COEFF_29;
			else
				temp_var = QPNP_VBAT_COEFF_30;
			temp_var = (die_temp - 25000) * temp_var;
			break;
		/* FAB_ID is non-zero */
		default:
			if (die_temp < 25000)
				temp_var = QPNP_VBAT_COEFF_31;
			else
				temp_var = (-QPNP_VBAT_COEFF_32);
			temp_var = (die_temp - 25000) * temp_var;
			break;
		}
		break;
	case QPNP_REV_ID_8916_2_0:
		switch (vadc->id) {
		case COMP_ID_SMIC:
			offset = (-QPNP_VBAT_COEFF_38);
			if (die_temp < 0)
				temp_var = die_temp * QPNP_VBAT_COEFF_36;
			else if (die_temp > 40000)
				temp_var = ((die_temp - 40000) *
						(-QPNP_VBAT_COEFF_37));
			break;
		case COMP_ID_TSMC:
			if (die_temp < 10000)
				temp_var = ((die_temp - 10000) *
						QPNP_VBAT_COEFF_41);
			else if (die_temp > 50000)
				temp_var = ((die_temp - 50000) *
						(-QPNP_VBAT_COEFF_42));
			break;
		default:
		case COMP_ID_GF:
			if (die_temp < 20000)
				temp_var = ((die_temp - 20000) *
						QPNP_VBAT_COEFF_45);
			else if (die_temp > 40000)
				temp_var = ((die_temp - 40000) *
						(-QPNP_VBAT_COEFF_46));
			break;
		}
		break;
	case QPNP_REV_ID_8909_1_0:
		switch (vadc->id) {
		case COMP_ID_SMIC:
			temp_var = (-QPNP_VBAT_COEFF_50);
			break;
		}
		break;
	case QPNP_REV_ID_8909_1_1:
		switch (vadc->id) {
		case COMP_ID_SMIC:
			temp_var = (QPNP_VBAT_COEFF_52);
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

	if (offset)
		*result -= offset;

	*result = div64_s64(*result, 1000000);
	pr_debug("%lld compensated into %lld\n", old, *result);

	return 0;
}

static int32_t qpnp_vbat_sns_comp(int64_t *result,
			struct qpnp_vadc_chip *vadc, int64_t die_temp)
{
	int64_t temp_var = 0, offset = 0;
	int64_t old = *result;
	int version;

	version = qpnp_adc_get_revid_version(vadc->dev);
	if (version == -EINVAL)
		return 0;

	if (version != QPNP_REV_ID_8941_3_1) {
		/* min(die_temp_c, 60_degC) */
		if (die_temp > 60000)
			die_temp = 60000;
	}

	switch (version) {
	case QPNP_REV_ID_8941_3_1:
		switch (vadc->id) {
		case COMP_ID_TSMC:
			temp_var = ((die_temp - 25000) *
			(-QPNP_VBAT_COEFF_1));
			break;
		default:
		case COMP_ID_GF:
			/* min(die_temp_c, 60_degC) */
			if (die_temp > 60000)
				die_temp = 60000;
			temp_var = ((die_temp - 25000) *
			(-QPNP_VBAT_COEFF_1));
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
	case QPNP_REV_ID_8026_2_2:
		switch (vadc->id) {
		case COMP_ID_TSMC:
			*result -= QPNP_VBAT_COEFF_23;
			temp_var = 0;
			break;
		default:
		case COMP_ID_GF:
			*result -= QPNP_VBAT_COEFF_23;
			temp_var = 0;
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
		default:
		case COMP_ID_GF:
			*result -= QPNP_VBAT_OFFSET_GF;
			temp_var = ((die_temp - 25000) *
			(QPNP_VBAT_COEFF_16));
			break;
		}
		break;
	case QPNP_REV_ID_8916_1_0:
		switch (vadc->id) {
		case COMP_ID_SMIC:
			temp_var = ((die_temp - 25000) *
			(QPNP_VBAT_COEFF_28));
			break;
		default:
		case COMP_ID_GF:
			temp_var = ((die_temp - 25000) *
			(QPNP_VBAT_COEFF_28));
			break;
		}
		break;
	case QPNP_REV_ID_8916_1_1:
		switch (vadc->id) {
		/* FAB_ID is zero */
		case COMP_ID_GF:
			temp_var = ((die_temp - 25000) *
			(QPNP_VBAT_COEFF_33));
			break;
		/* FAB_ID is non-zero */
		default:
			offset = QPNP_VBAT_COEFF_35;
			if (die_temp > 50000) {
				temp_var = ((die_temp - 25000) *
				(QPNP_VBAT_COEFF_34));
			}
			break;
		}
		break;
	case QPNP_REV_ID_8916_2_0:
		switch (vadc->id) {
		case COMP_ID_SMIC:
			if (die_temp < 0) {
				temp_var = (die_temp *
					QPNP_VBAT_COEFF_39);
			} else if (die_temp > 40000) {
				temp_var = ((die_temp - 40000) *
				(-QPNP_VBAT_COEFF_40));
			}
			break;
		case COMP_ID_TSMC:
			if (die_temp < 10000)
				temp_var = ((die_temp - 10000) *
					QPNP_VBAT_COEFF_43);
			else if (die_temp > 50000)
				temp_var = ((die_temp - 50000) *
						(-QPNP_VBAT_COEFF_44));
			break;
		default:
		case COMP_ID_GF:
			if (die_temp < 20000)
				temp_var = ((die_temp - 20000) *
					QPNP_VBAT_COEFF_47);
			else if (die_temp > 40000)
				temp_var = ((die_temp - 40000) *
						(-QPNP_VBAT_COEFF_48));
			break;
		}
		break;
	case QPNP_REV_ID_8909_1_0:
		switch (vadc->id) {
		case COMP_ID_SMIC:
			if (die_temp < 30000)
				temp_var = (-QPNP_VBAT_COEFF_50);
			else if (die_temp > 30000)
				temp_var = (((die_temp - 30000) *
					(-QPNP_VBAT_COEFF_49)) +
					(-QPNP_VBAT_COEFF_50));
			break;
		}
		break;
	case QPNP_REV_ID_8909_1_1:
		switch (vadc->id) {
		case COMP_ID_SMIC:
			if (die_temp < 30000)
				temp_var = (QPNP_VBAT_COEFF_52);
			else if (die_temp > 30000)
				temp_var = (((die_temp - 30000) *
					(-QPNP_VBAT_COEFF_51)) +
					(QPNP_VBAT_COEFF_52));
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

	if (offset)
		*result -= offset;

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

	pr_debug("die-temp = %lld\n", die_temp_result.physical);

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

int32_t qpnp_vadc_calib_vref(struct qpnp_vadc_chip *vadc,
					enum qpnp_adc_calib_type calib_type,
					int *calib_data)
{
	struct qpnp_adc_amux_properties conv;
	int rc, count = 0, calib_read = 0;
	u8 status1 = 0;

	if (calib_type == CALIB_ABSOLUTE)
		conv.amux_channel = REF_125V;
	else if (calib_type == CALIB_RATIOMETRIC)
		conv.amux_channel = VDD_VADC;

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

	rc = qpnp_vadc_read_conversion_result(vadc, &calib_read);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		goto calib_fail;
	}
	*calib_data = calib_read;
calib_fail:
	return rc;
}


int32_t qpnp_vadc_calib_gnd(struct qpnp_vadc_chip *vadc,
					enum qpnp_adc_calib_type calib_type,
					int *calib_data)
{
	struct qpnp_adc_amux_properties conv;
	int rc, count = 0, calib_read = 0;
	u8 status1 = 0;
	uint32_t ref_channel_sel = 0;

	if (calib_type == CALIB_ABSOLUTE) {
		qpnp_vadc_625mv_channel_sel(vadc, &ref_channel_sel);
		conv.amux_channel = ref_channel_sel;
	} else if (calib_type == CALIB_RATIOMETRIC)
		conv.amux_channel = GND_REF;

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

	rc = qpnp_vadc_read_conversion_result(vadc, &calib_read);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		goto calib_fail;
	}
	*calib_data = calib_read;
calib_fail:
	return rc;
}

static int32_t qpnp_vadc_calib_device(struct qpnp_vadc_chip *vadc)
{
	int rc, calib_read_1 = 0, calib_read_2 = 0;

	rc = qpnp_vadc_calib_vref(vadc, CALIB_ABSOLUTE, &calib_read_1);
	if (rc) {
		pr_err("qpnp adc absolute vref calib failed with %d\n", rc);
		goto calib_fail;
	}
	rc = qpnp_vadc_calib_gnd(vadc, CALIB_ABSOLUTE, &calib_read_2);
	if (rc) {
		pr_err("qpnp adc absolute gnd calib failed with %d\n", rc);
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

	calib_read_1 = 0;
	calib_read_2 = 0;
	rc = qpnp_vadc_calib_vref(vadc, CALIB_RATIOMETRIC, &calib_read_1);
	if (rc) {
		pr_err("qpnp adc ratiometric vref calib failed with %d\n", rc);
		goto calib_fail;
	}
	rc = qpnp_vadc_calib_gnd(vadc, CALIB_RATIOMETRIC, &calib_read_2);
	if (rc) {
		pr_err("qpnp adc ratiometric gnd calib failed with %d\n", rc);
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
	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_RATIOMETRIC].adc_vref
					= calib_read_1;
	vadc->adc->amux_prop->chan_prop->adc_graph[CALIB_RATIOMETRIC].adc_gnd
					= calib_read_2;

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
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(qpnp_get_vadc_gain_and_offset);

static int32_t qpnp_vadc_wait_for_req_sts_check(struct qpnp_vadc_chip *vadc)
{
	u8 status1 = 0;
	int rc, count = 0;

	/* Re-enable the peripheral */
	rc = qpnp_vadc_enable(vadc, true);
	if (rc) {
		pr_err("vadc re-enable peripheral failed with %d\n", rc);
		return rc;
	}

	/* The VADC_TM bank needs to be disabled for new conversion request */
	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS1, &status1);
	if (rc) {
		pr_err("vadc read status1 failed with %d\n", rc);
		return rc;
	}

	/* Disable the bank if a conversion is occuring */
	while ((status1 & QPNP_VADC_STATUS1_REQ_STS) && (count < QPNP_RETRY)) {
		/* Wait time is based on the optimum sampling rate
		 * and adding enough time buffer to account for ADC conversions
		 * occuring on different peripheral banks */
		usleep_range(QPNP_MIN_TIME, QPNP_MAX_TIME);
		rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS1, &status1);
		if (rc < 0) {
			pr_err("vadc disable failed with %d\n", rc);
			return rc;
		}
		count++;
	}

	if (count >= QPNP_RETRY)
		pr_err("QPNP vadc status req bit did not fall low!!\n");

	rc = qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS1, &status1);

	/* Disable the peripheral */
	rc = qpnp_vadc_enable(vadc, false);
	if (rc < 0)
		pr_err("vadc peripheral disable failed with %d\n", rc);

	return rc;
}

static int32_t qpnp_vadc_manage_meas_int_requests(struct qpnp_vadc_chip *chip)
{
	struct qpnp_vadc_chip *vadc = dev_get_drvdata(chip->dev);
	int rc = 0, dt_index = 0;
	u8 mode_ctl = 0;

	pr_debug("meas_int_mode:0x%x, mode_ctl:%0x\n",
		vadc->state_copy->meas_int_mode, mode_ctl);

	if (vadc->state_copy->meas_int_mode) {
		pr_debug("meas interval in progress. Procced to disable it\n");
		/* measurement interval in progress. Proceed to disable it */
		mode_ctl = ADC_OP_NORMAL_MODE;
		rc = qpnp_vadc_mode_select(vadc, mode_ctl);
		if (rc < 0) {
			pr_err("NORM mode select failed with %d\n", rc);
			return rc;
		}

		/* Disable bank */
		rc = qpnp_vadc_enable(vadc, false);
		if (rc) {
			pr_err("Disable bank failed with %d\n", rc);
			return rc;
		}

		/* Check if a conversion is in progress */
		rc = qpnp_vadc_wait_for_req_sts_check(vadc);
		if (rc < 0) {
			pr_err("req_sts check failed with %d\n", rc);
			return rc;
		}

		vadc->state_copy->meas_int_mode = false;
		vadc->state_copy->meas_int_request_in_queue = true;
	} else if (vadc->state_copy->meas_int_request_in_queue) {
		/* put the meas interval back in queue */
		pr_debug("put meas interval back in queue\n");
		vadc->adc->amux_prop->amux_channel =
				vadc->state_copy->vadc_meas_amux.channel_num;
		while ((vadc->adc->adc_channels[dt_index].channel_num
			!= vadc->adc->amux_prop->amux_channel) &&
			(dt_index < vadc->max_channels_available))
			dt_index++;
		if (dt_index >= vadc->max_channels_available) {
			pr_err("not a valid VADC channel\n");
			rc = -EINVAL;
			return rc;
		}

		vadc->adc->amux_prop->decimation =
			vadc->adc->amux_prop->decimation;
		vadc->adc->amux_prop->hw_settle_time =
			vadc->adc->amux_prop->hw_settle_time;
		vadc->adc->amux_prop->fast_avg_setup =
			vadc->adc->amux_prop->fast_avg_setup;
		vadc->adc->amux_prop->mode_sel = ADC_OP_MEASUREMENT_INTERVAL;
		rc = qpnp_vadc_configure(vadc, vadc->adc->amux_prop);
		if (rc) {
			pr_err("vadc configure failed with %d\n", rc);
			return rc;
		}

		vadc->state_copy->meas_int_mode = true;
		vadc->state_copy->meas_int_request_in_queue = false;
	}
	dev_set_drvdata(vadc->dev, vadc);

	return 0;
}

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
	int rc = 0, scale_type, amux_prescaling, dt_index = 0, calib_type = 0;
	uint32_t ref_channel, count = 0, local_idx = 0;
	int32_t vref_calib = 0, gnd_calib = 0, new_vref_calib = 0, offset = 0;
	int32_t calib_offset = 0;
	u8 status1 = 0;

	if (qpnp_vadc_is_valid(vadc))
		return -EPROBE_DEFER;

	mutex_lock(&vadc->adc->adc_lock);

	if (vadc->state_copy->vadc_meas_int_enable)
		qpnp_vadc_manage_meas_int_requests(vadc);

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

	calib_type = vadc->adc->adc_channels[dt_index].calib_type;
	if (calib_type >= CALIB_NONE) {
		pr_err("not a valid calib_type\n");
		rc = -EINVAL;
		goto fail_unlock;
	}
	calib_offset = (calib_type == CALIB_ABSOLUTE) ?
		QPNP_VADC_ABSOLUTE_RECALIB_OFFSET :
		QPNP_VADC_RATIOMETRIC_RECALIB_OFFSET;
	rc = qpnp_vadc_version_check(vadc);
	if (rc)
		goto fail_unlock;
	if (vadc->vadc_recalib_check) {
		rc = qpnp_vadc_calib_vref(vadc, calib_type, &vref_calib);
		if (rc) {
			pr_err("Calibration failed\n");
			goto fail_unlock;
		}
	} else if (!vadc->vadc_init_calib) {
		rc = qpnp_vadc_calib_device(vadc);
		if (rc) {
			pr_err("Calibration failed\n");
			goto fail_unlock;
		} else {
			vadc->vadc_init_calib = true;
		}
	}

recalibrate:
	status1 = 0;
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
			if (status1 == QPNP_VADC_STATUS1_EOC)
				break;
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

	if (vadc->vadc_recalib_check) {
		rc = qpnp_vadc_calib_gnd(vadc, calib_type, &gnd_calib);
		if (rc) {
			pr_err("Calibration failed\n");
			goto fail_unlock;
		}
		rc = qpnp_vadc_calib_vref(vadc, calib_type, &new_vref_calib);
		if (rc < 0) {
			pr_err("qpnp vadc calib read failed with %d\n", rc);
			goto fail_unlock;
		}

		if (local_idx >= QPNP_VADC_RECALIB_MAXCNT) {
			pr_err("invalid recalib count=%d\n", local_idx);
			rc = -EINVAL;
			goto fail_unlock;
		}
		pr_debug(
			"chan=%d, calib=%s, vref_calib=0x%x, gnd_calib=0x%x, new_vref_calib=0x%x\n",
			channel,
			((calib_type == CALIB_ABSOLUTE) ?
			"ABSOLUTE" : "RATIOMETRIC"),
			vref_calib, gnd_calib, new_vref_calib);

		offset = (new_vref_calib - vref_calib);
		if (offset < 0)
			offset = -offset;
		if (offset <= calib_offset) {
			pr_debug(
				"qpnp vadc recalibration not required,offset:%d\n",
								offset);
			local_idx = 0;
		vadc->adc->amux_prop->chan_prop->adc_graph[calib_type].dy =
						(vref_calib - gnd_calib);
		vadc->adc->amux_prop->chan_prop->adc_graph[calib_type].dx =
			(calib_type == CALIB_ABSOLUTE) ? QPNP_ADC_625_UV :
					vadc->adc->adc_prop->adc_vdd_reference;
		vadc->adc->amux_prop->chan_prop->adc_graph[calib_type].adc_vref
								= vref_calib;
		vadc->adc->amux_prop->chan_prop->adc_graph[calib_type].adc_gnd
								= gnd_calib;
		} else {
			vref_calib = new_vref_calib;
			local_idx = local_idx + 1;
			if (local_idx >= QPNP_VADC_RECALIB_MAXCNT) {
				pr_err(
				"qpnp_vadc recalibration failed, count=%d",
								local_idx);
			} else {
				pr_debug(
				"qpnp vadc recalibration requested,offset:%d\n",
								offset);
				offset = 0;
				goto recalibrate;
			}
		}
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
	vadc->adc->amux_prop->chan_prop->calib_type =
		vadc->adc->adc_channels[dt_index].calib_type;

	scale_type = vadc->adc->adc_channels[dt_index].adc_scale_fn;
	if (scale_type >= SCALE_NONE) {
		rc = -EBADF;
		goto fail_unlock;
	}

	if ((qpnp_vadc_channel_post_scaling_calib_check(vadc, channel)) < 0)
		pr_debug("Post scaling calib type not updated\n");

	vadc_scale_fn[scale_type].chan(vadc, result->adc_code,
		vadc->adc->adc_prop, vadc->adc->amux_prop->chan_prop, result);

	pr_debug("channel=%d, adc_code=%d adc_result=%lld\n",
			channel, result->adc_code, result->physical);

fail_unlock:
	if (vadc->state_copy->vadc_meas_int_enable)
		qpnp_vadc_manage_meas_int_requests(vadc);

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
	} else if (channel == SPARE2) {
		/* chg temp channel */
		int version;
		enum power_supply_property prop;
		union power_supply_propval ret = {0, };

		/* Note reading version support is not needed if by default
		 * the channel is supported across all revisions.
		 */
		version = qpnp_adc_get_revid_version(vadc->dev);
		if (version == -EINVAL)
			pr_debug("Unable to get rev-id support\n");
		else if (version == QPNP_REV_ID_PMI8994_1_0) {
			pr_err("Version does not support CHG Temp\n");
			return -EINVAL;
		}

		if (!vadc->vadc_chg_vote) {
			vadc->vadc_chg_vote =
				power_supply_get_by_name("battery");
			if (!vadc->vadc_chg_vote) {
				pr_err("no vadc_chg_vote found\n");
				return -EINVAL;
			}
		}

		prop = POWER_SUPPLY_PROP_FORCE_TLIM;
		ret.intval = 1;

		rc = vadc->vadc_chg_vote->set_property(vadc->vadc_chg_vote,
								prop, &ret);
		if (rc) {
			pr_err("error enabling the charger circuitry vote\n");
			return rc;
		}

		rc = qpnp_vadc_conv_seq_request(vadc, ADC_SEQ_NONE,
				channel, result);
		if (rc < 0)
			pr_err("Error reading die_temp\n");

		ret.intval = 0;
		rc = vadc->vadc_chg_vote->set_property(vadc->vadc_chg_vote,
								prop, &ret);
		if (rc) {
			pr_err("error enabling the charger circuitry vote\n");
			return rc;
		}

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
	int rc = 0, dt_index = 0, calib_type = 0;

	if (qpnp_vadc_is_valid(vadc))
		return -EPROBE_DEFER;

	qpnp_vadc_lock(vadc);


	vadc->adc->amux_prop->amux_channel = channel;

	while ((vadc->adc->adc_channels[dt_index].channel_num
		!= channel) && (dt_index < vadc->max_channels_available))
		dt_index++;

	if (dt_index >= vadc->max_channels_available) {
		pr_err("not a valid VADC channel\n");
		rc = -EINVAL;
		goto fail;
	}

	calib_type = vadc->adc->adc_channels[dt_index].calib_type;
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

static int32_t qpnp_vadc_thr_update(struct qpnp_vadc_chip *vadc,
					int32_t high_thr, int32_t low_thr)
{
	int rc = 0;

	pr_debug("client requested high:%d and low:%d\n",
		high_thr, low_thr);

	rc = qpnp_vadc_write_reg(vadc,
			QPNP_VADC_LOW_THR_LSB,
			QPNP_VADC_THR_LSB_MASK(low_thr));
	if (rc < 0) {
		pr_err("low threshold lsb setting failed, err:%d\n", rc);
		return rc;
	}

	rc = qpnp_vadc_write_reg(vadc,
			QPNP_VADC_LOW_THR_MSB,
			QPNP_VADC_THR_MSB_MASK(low_thr));
	if (rc < 0) {
		pr_err("low threshold msb setting failed, err:%d\n", rc);
		return rc;
	}

	rc = qpnp_vadc_write_reg(vadc,
			QPNP_VADC_HIGH_THR_LSB,
			QPNP_VADC_THR_LSB_MASK(high_thr));
	if (rc < 0) {
		pr_err("high threshold lsb setting failed, err:%d\n", rc);
		return rc;
	}

	rc = qpnp_vadc_write_reg(vadc,
			QPNP_VADC_HIGH_THR_MSB,
			QPNP_VADC_THR_MSB_MASK(high_thr));
	if (rc < 0) {
		pr_err("high threshold msb setting failed, err:%d\n", rc);
		return rc;
	}

	pr_debug("client requested high:%d and low:%d\n", high_thr, low_thr);

	return rc;
}

int32_t qpnp_vadc_channel_monitor(struct qpnp_vadc_chip *chip,
					struct qpnp_adc_tm_btm_param *param)
{
	uint32_t channel, scale_type = 0;
	uint32_t low_thr = 0, high_thr = 0;
	int rc = 0, idx = 0, amux_prescaling = 0;
	struct qpnp_vadc_chip *vadc = dev_get_drvdata(chip->dev);

	if (qpnp_vadc_is_valid(vadc))
		return -EPROBE_DEFER;

	if (!vadc->state_copy->vadc_meas_int_enable) {
		pr_err("Recurring measurement interval not available\n");
		return -EINVAL;
	}

	if (param->threshold_notification == NULL) {
		pr_debug("No notification for high/low temp??\n");
		return -EINVAL;
	}

	mutex_lock(&vadc->adc->adc_lock);

	channel = param->channel;
	while (idx < vadc->max_channels_available) {
		if (vadc->adc->adc_channels[idx].channel_num == channel)
			break;
		 else
			idx++;
	}

	if (idx >= vadc->max_channels_available)  {
		pr_err("not a valid VADC channel\n");
		rc = -EINVAL;
		goto fail_unlock;
	}

	scale_type = vadc->adc->adc_channels[idx].adc_scale_fn;
	if (scale_type >= SCALE_RVADC_SCALE_NONE) {
		rc = -EBADF;
		goto fail_unlock;
	}

	amux_prescaling =
		vadc->adc->adc_channels[idx].chan_path_prescaling;

	if (amux_prescaling >= PATH_SCALING_NONE) {
		rc = -EINVAL;
		goto fail_unlock;
	}

	vadc->adc->amux_prop->chan_prop->offset_gain_numerator =
		qpnp_vadc_amux_scaling_ratio[amux_prescaling].num;
	vadc->adc->amux_prop->chan_prop->offset_gain_denominator =
		 qpnp_vadc_amux_scaling_ratio[amux_prescaling].den;
	vadc->adc->amux_prop->chan_prop->calib_type =
		vadc->adc->adc_channels[idx].calib_type;

	pr_debug("channel:%d, scale_type:%d, dt_idx:%d",
					channel, scale_type, idx);
	vadc->adc->amux_prop->amux_channel = channel;
	vadc->adc->amux_prop->decimation =
			vadc->adc->adc_channels[idx].adc_decimation;
	vadc->adc->amux_prop->hw_settle_time =
			vadc->adc->adc_channels[idx].hw_settle_time;
	vadc->adc->amux_prop->fast_avg_setup =
			vadc->adc->adc_channels[idx].fast_avg_setup;
	vadc->adc->amux_prop->mode_sel = ADC_OP_MEASUREMENT_INTERVAL;
	adc_vadc_rscale_fn[scale_type].chan(vadc,
			vadc->adc->amux_prop->chan_prop, param,
			&low_thr, &high_thr);

	if (param->timer_interval >= ADC_MEAS1_INTERVAL_NONE) {
		pr_err("Invalid timer interval :%d\n", param->timer_interval);
		goto fail_unlock;
	}

	rc = qpnp_vadc_write_reg(vadc, QPNP_VADC_MEAS_INTERVAL_CTL,
				param->timer_interval);
	if (rc) {
		pr_err("vadc meas timer failed with %d\n", rc);
		goto fail_unlock;
	}

	rc = qpnp_vadc_thr_update(vadc, high_thr, low_thr);
	if (rc) {
		pr_err("vadc thr update failed with %d\n", rc);
		goto fail_unlock;
	}

	rc = qpnp_vadc_configure(vadc, vadc->adc->amux_prop);
	if (rc) {
		pr_err("vadc configure failed with %d\n", rc);
		goto fail_unlock;
	}

	vadc->state_copy->meas_int_mode = true;
	vadc->state_copy->param = param;
	vadc->state_copy->vadc_meas_amux.channel_num = channel;
	vadc->state_copy->vadc_meas_amux.adc_decimation =
				vadc->adc->amux_prop->decimation;
	vadc->state_copy->vadc_meas_amux.hw_settle_time =
				vadc->adc->amux_prop->hw_settle_time;
	vadc->state_copy->vadc_meas_amux.fast_avg_setup =
				vadc->adc->amux_prop->fast_avg_setup;
	vadc->state_copy->meas_int_request_in_queue = false;
	dev_set_drvdata(vadc->dev, vadc);

fail_unlock:
	mutex_unlock(&vadc->adc->adc_lock);

	return rc;
}
EXPORT_SYMBOL(qpnp_vadc_channel_monitor);

int32_t qpnp_vadc_end_channel_monitor(struct qpnp_vadc_chip *chip)
{
	struct qpnp_vadc_chip *vadc = dev_get_drvdata(chip->dev);
	u8 mode_ctl = 0;

	if (qpnp_vadc_is_valid(vadc))
		return -EPROBE_DEFER;

	if (!vadc->state_copy->vadc_meas_int_enable) {
		pr_err("Recurring measurement interval not available\n");
		return -EINVAL;
	}

	vadc->state_copy->meas_int_mode = false;
	vadc->state_copy->meas_int_request_in_queue = false;
	dev_set_drvdata(vadc->dev, vadc);
	mode_ctl = ADC_OP_NORMAL_MODE;
	/* Set measurement in single measurement mode */
	qpnp_vadc_mode_select(vadc, mode_ctl);
	qpnp_vadc_enable(vadc, false);

	return 0;
}
EXPORT_SYMBOL(qpnp_vadc_end_channel_monitor);

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
		"Result:%lld Raw:%x\n", result.physical, result.adc_code);
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

static int qpnp_vadc_get_temp(struct thermal_zone_device *thermal,
			     unsigned long *temp)
{
	struct qpnp_vadc_thermal_data *vadc_therm = thermal->devdata;
	struct qpnp_vadc_chip *vadc = vadc_therm->vadc_dev;
	struct qpnp_vadc_result result;
	int rc = 0;

	rc = qpnp_vadc_read(vadc,
				vadc_therm->vadc_channel, &result);
	if (rc) {
		pr_err("VADC read error with %d\n", rc);
		return rc;
	}

	*temp = result.physical;

	return rc;
}

static struct thermal_zone_device_ops qpnp_vadc_thermal_ops = {
	.get_temp = qpnp_vadc_get_temp,
};

static int32_t qpnp_vadc_init_thermal(struct qpnp_vadc_chip *vadc,
					struct spmi_device *spmi)
{
	struct device_node *child;
	struct device_node *node = spmi->dev.of_node;
	int rc = 0, i = 0;
	bool thermal_node = false;

	if (node == NULL)
		goto thermal_err_sens;
	for_each_child_of_node(node, child) {
		char name[QPNP_THERMALNODE_NAME_LENGTH];

		vadc->vadc_therm_chan[i].vadc_channel =
			vadc->adc->adc_channels[i].channel_num;
		vadc->vadc_therm_chan[i].thermal_chan = i;
		thermal_node = of_property_read_bool(child,
					"qcom,vadc-thermal-node");
		if (thermal_node) {
			/* Register with the thermal zone */
			vadc->vadc_therm_chan[i].thermal_node = true;
			snprintf(name, sizeof(name), "%s",
				vadc->adc->adc_channels[i].name);
			vadc->vadc_therm_chan[i].tz_dev =
				thermal_zone_device_register(name,
				0, 0, &vadc->vadc_therm_chan[i],
				&qpnp_vadc_thermal_ops, NULL, 0, 0);
			if (IS_ERR(vadc->vadc_therm_chan[i].tz_dev)) {
				pr_err("thermal device register failed.\n");
				goto thermal_err_sens;
			}
			vadc->vadc_therm_chan[i].vadc_dev = vadc;
		}
		i++;
		thermal_node = false;
	}
	return 0;
thermal_err_sens:
	pr_err("Init HWMON failed for qpnp_adc with %d\n", rc);
	return rc;
}

static int qpnp_vadc_probe(struct spmi_device *spmi)
{
	struct qpnp_vadc_chip *vadc;
	struct qpnp_adc_drv *adc_qpnp;
	struct qpnp_vadc_thermal_data *adc_thermal;
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

	vadc->state_copy = devm_kzalloc(&spmi->dev,
			sizeof(struct qpnp_vadc_mode_state), GFP_KERNEL);
	if (!vadc->state_copy) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	vadc->adc = adc_qpnp;
	adc_thermal = devm_kzalloc(&spmi->dev,
			(sizeof(struct qpnp_vadc_thermal_data) *
				count_adc_channel_list), GFP_KERNEL);
	if (!adc_thermal) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	vadc->vadc_therm_chan = adc_thermal;
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
	rc = qpnp_vadc_init_thermal(vadc, spmi);
	if (rc) {
		dev_err(&spmi->dev, "failed to initialize qpnp thermal adc\n");
		return rc;
	}
	vadc->vadc_init_calib = false;
	vadc->max_channels_available = count_adc_channel_list;
	rc = qpnp_vadc_read_reg(vadc, QPNP_INT_TEST_VAL, &fab_id);
	if (rc < 0) {
		pr_err("qpnp adc comp id failed with %d\n", rc);
		goto err_setup;
	}
	vadc->id = fab_id;
	pr_debug("fab_id = %d\n", fab_id);

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

	vadc->vadc_recalib_check = of_property_read_bool(node,
						"qcom,vadc-recalib-check");

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

	vadc->state_copy->vadc_meas_int_enable = of_property_read_bool(node,
						"qcom,vadc-meas-int-mode");
	if (vadc->state_copy->vadc_meas_int_enable) {
		vadc->adc->adc_high_thr_irq = spmi_get_irq_byname(spmi,
						NULL, "high-thr-en-set");
		if (vadc->adc->adc_high_thr_irq < 0) {
			pr_err("Invalid irq\n");
			rc = -ENXIO;
			goto err_setup;
		}

		vadc->adc->adc_low_thr_irq = spmi_get_irq_byname(spmi,
						NULL, "low-thr-en-set");
		if (vadc->adc->adc_low_thr_irq < 0) {
			pr_err("Invalid irq\n");
			rc = -ENXIO;
			goto err_setup;
		}

		rc = devm_request_irq(&spmi->dev, vadc->adc->adc_high_thr_irq,
					qpnp_vadc_high_thr_isr,
			IRQF_TRIGGER_RISING, "qpnp_vadc_high_interrupt", vadc);
		if (rc) {
			dev_err(&spmi->dev, "failed to request adc irq\n");
			goto err_setup;
		} else {
			enable_irq_wake(vadc->adc->adc_high_thr_irq);
		}

		rc = devm_request_irq(&spmi->dev, vadc->adc->adc_low_thr_irq,
					qpnp_vadc_low_thr_isr,
			IRQF_TRIGGER_RISING, "qpnp_vadc_low_interrupt", vadc);
		if (rc) {
			dev_err(&spmi->dev, "failed to request adc irq\n");
			goto err_setup;
		} else {
			enable_irq_wake(vadc->adc->adc_low_thr_irq);
		}
		INIT_WORK(&vadc->trigger_high_thr_work,
						qpnp_vadc_high_thr_fn);
		INIT_WORK(&vadc->trigger_low_thr_work, qpnp_vadc_low_thr_fn);
	}

	vadc->vadc_iadc_sync_lock = false;
	dev_set_drvdata(&spmi->dev, vadc);
	list_add(&vadc->list, &qpnp_vadc_device_list);

	return 0;

err_setup:
	for_each_child_of_node(node, child) {
		device_remove_file(&spmi->dev,
			&vadc->sens_attr[i].dev_attr);
		if (vadc->vadc_therm_chan[i].thermal_node)
			thermal_zone_device_unregister(
					vadc->vadc_therm_chan[i].tz_dev);
		i++;
	}
	hwmon_device_unregister(vadc->vadc_hwmon);

	return rc;
}

static int qpnp_vadc_remove(struct spmi_device *spmi)
{
	struct qpnp_vadc_chip *vadc = dev_get_drvdata(&spmi->dev);
	struct device_node *node = spmi->dev.of_node;
	struct device_node *child;
	int i = 0;

	for_each_child_of_node(node, child) {
		device_remove_file(&spmi->dev,
			&vadc->sens_attr[i].dev_attr);
		if (vadc->vadc_therm_chan[i].thermal_node)
			thermal_zone_device_unregister(
					vadc->vadc_therm_chan[i].tz_dev);
		i++;
	}
	hwmon_device_unregister(vadc->vadc_hwmon);
	list_del(&vadc->list);
	if (vadc->adc->hkadc_ldo && vadc->adc->hkadc_ldo_ok)
		qpnp_adc_free_voltage_resource(vadc->adc);
	dev_set_drvdata(&spmi->dev, NULL);

	return 0;
}

static const struct of_device_id qpnp_vadc_match_table[] = {
	{	.compatible = "qcom,qpnp-vadc",
	},
	{}
};

static int qpnp_vadc_suspend_noirq(struct device *dev)
{
	struct qpnp_vadc_chip *vadc = dev_get_drvdata(dev);
	u8 status = 0;

	qpnp_vadc_read_reg(vadc, QPNP_VADC_STATUS1, &status);
	if (((status & QPNP_VADC_STATUS1_OP_MODE_MASK) >>
		QPNP_VADC_OP_MODE_SHIFT) == QPNP_VADC_MEAS_INT_MODE) {
		pr_debug("Meas interval in progress\n");
	} else if (vadc->vadc_poll_eoc) {
		status &= QPNP_VADC_STATUS1_REQ_STS_EOC_MASK;
		pr_debug("vadc conversion status=%d\n", status);
		if (status != QPNP_VADC_STATUS1_EOC) {
			pr_err(
				"Aborting suspend, adc conversion requested while suspending\n");
			return -EBUSY;
		}
	}

	return 0;
}

static const struct dev_pm_ops qpnp_vadc_pm_ops = {
	.suspend_noirq	= qpnp_vadc_suspend_noirq,
};

static struct spmi_driver qpnp_vadc_driver = {
	.driver		= {
		.name	= "qcom,qpnp-vadc",
		.of_match_table = qpnp_vadc_match_table,
		.pm		= &qpnp_vadc_pm_ops,
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
