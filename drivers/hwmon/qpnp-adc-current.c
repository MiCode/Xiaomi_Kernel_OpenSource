/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/hwmon-sysfs.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>

/* QPNP IADC register definition */
#define QPNP_IADC_REVISION1				0x0
#define QPNP_IADC_REVISION2				0x1
#define QPNP_IADC_REVISION3				0x2
#define QPNP_IADC_REVISION4				0x3
#define QPNP_IADC_PERPH_TYPE				0x4
#define QPNP_IADC_PERH_SUBTYPE				0x5

#define QPNP_IADC_SUPPORTED_REVISION2			1

#define QPNP_STATUS1					0x8
#define QPNP_STATUS1_OP_MODE				4
#define QPNP_STATUS1_MULTI_MEAS_EN			BIT(3)
#define QPNP_STATUS1_MEAS_INTERVAL_EN_STS		BIT(2)
#define QPNP_STATUS1_REQ_STS				BIT(1)
#define QPNP_STATUS1_EOC				BIT(0)
#define QPNP_STATUS1_REQ_STS_EOC_MASK			0x3
#define QPNP_STATUS2					0x9
#define QPNP_STATUS2_CONV_SEQ_STATE_SHIFT		4
#define QPNP_STATUS2_FIFO_NOT_EMPTY_FLAG		BIT(1)
#define QPNP_STATUS2_CONV_SEQ_TIMEOUT_STS		BIT(0)
#define QPNP_CONV_TIMEOUT_ERR				2

#define QPNP_IADC_MODE_CTL				0x40
#define QPNP_OP_MODE_SHIFT				4
#define QPNP_USE_BMS_DATA				BIT(4)
#define QPNP_VADC_SYNCH_EN				BIT(2)
#define QPNP_OFFSET_RMV_EN				BIT(1)
#define QPNP_ADC_TRIM_EN				BIT(0)
#define QPNP_IADC_EN_CTL1				0x46
#define QPNP_IADC_ADC_EN				BIT(7)
#define QPNP_ADC_CH_SEL_CTL				0x48
#define QPNP_ADC_DIG_PARAM				0x50
#define QPNP_ADC_CLK_SEL_MASK				0x3
#define QPNP_ADC_DEC_RATIO_SEL_MASK			0xc
#define QPNP_ADC_DIG_DEC_RATIO_SEL_SHIFT		2

#define QPNP_HW_SETTLE_DELAY				0x51
#define QPNP_CONV_REQ					0x52
#define QPNP_CONV_REQ_SET				BIT(7)
#define QPNP_CONV_SEQ_CTL				0x54
#define QPNP_CONV_SEQ_HOLDOFF_SHIFT			4
#define QPNP_CONV_SEQ_TRIG_CTL				0x55
#define QPNP_FAST_AVG_CTL				0x5a

#define QPNP_M0_LOW_THR_LSB				0x5c
#define QPNP_M0_LOW_THR_MSB				0x5d
#define QPNP_M0_HIGH_THR_LSB				0x5e
#define QPNP_M0_HIGH_THR_MSB				0x5f
#define QPNP_M1_LOW_THR_LSB				0x69
#define QPNP_M1_LOW_THR_MSB				0x6a
#define QPNP_M1_HIGH_THR_LSB				0x6b
#define QPNP_M1_HIGH_THR_MSB				0x6c

#define QPNP_DATA0					0x60
#define QPNP_DATA1					0x61
#define QPNP_CONV_TIMEOUT_ERR				2

#define QPNP_IADC_SEC_ACCESS				0xD0
#define QPNP_IADC_SEC_ACCESS_DATA			0xA5
#define QPNP_IADC_MSB_OFFSET				0xF2
#define QPNP_IADC_LSB_OFFSET				0xF3
#define QPNP_IADC_NOMINAL_RSENSE			0xF4
#define QPNP_IADC_ATE_GAIN_CALIB_OFFSET			0xF5
#define QPNP_INT_TEST_VAL				0xE1

#define QPNP_IADC_ADC_CH_SEL_CTL			0x48
#define QPNP_IADC_ADC_CHX_SEL_SHIFT			3

#define QPNP_IADC_ADC_DIG_PARAM				0x50
#define QPNP_IADC_CLK_SEL_SHIFT				1
#define QPNP_IADC_DEC_RATIO_SEL				3

#define QPNP_IADC_CONV_REQUEST				0x52
#define QPNP_IADC_CONV_REQ				BIT(7)

#define QPNP_IADC_DATA0					0x60
#define QPNP_IADC_DATA1					0x61

#define QPNP_ADC_CONV_TIME_MIN				2000
#define QPNP_ADC_CONV_TIME_MAX				2100
#define QPNP_ADC_ERR_COUNT				20

#define QPNP_ADC_GAIN_NV				17857
#define QPNP_OFFSET_CALIBRATION_SHORT_CADC_LEADS_IDEAL	0
#define QPNP_IADC_INTERNAL_RSENSE_N_OHMS_FACTOR		10000000
#define QPNP_IADC_NANO_VOLTS_FACTOR			1000000
#define QPNP_IADC_CALIB_SECONDS				300000
#define QPNP_IADC_RSENSE_LSB_N_OHMS_PER_BIT		15625
#define QPNP_IADC_DIE_TEMP_CALIB_OFFSET			5000

#define QPNP_RAW_CODE_16_BIT_MSB_MASK			0xff00
#define QPNP_RAW_CODE_16_BIT_LSB_MASK			0xff
#define QPNP_BIT_SHIFT_8				8
#define QPNP_RSENSE_MSB_SIGN_CHECK			0x80
#define QPNP_ADC_COMPLETION_TIMEOUT			HZ
#define SMBB_BAT_IF_TRIM_CNST_RDS_MASK			0x7
#define SMBB_BAT_IF_TRIM_CNST_RDS_MASK_CONST		2
#define QPNP_IADC1_USR_TRIM2_ADC_FULLSCALE1_CONST	127
#define QPNP_IADC_RSENSE_DEFAULT_VALUE			7800000
#define QPNP_IADC_RSENSE_DEFAULT_TYPEB_GF		9000000
#define QPNP_IADC_RSENSE_DEFAULT_TYPEB_SMIC		9700000

struct qpnp_iadc_comp {
	bool	ext_rsense;
	u8	id;
	u8	sys_gain;
	u8	revision_dig_major;
	u8	revision_ana_minor;
};

struct qpnp_iadc_chip {
	struct device				*dev;
	struct qpnp_adc_drv			*adc;
	int32_t					rsense;
	bool					external_rsense;
	bool					default_internal_rsense;
	struct device				*iadc_hwmon;
	struct list_head			list;
	int64_t					die_temp;
	struct delayed_work			iadc_work;
	bool					iadc_mode_sel;
	struct qpnp_iadc_comp			iadc_comp;
	struct qpnp_vadc_chip			*vadc_dev;
	struct work_struct			trigger_completion_work;
	bool					skip_auto_calibrations;
	bool					iadc_poll_eoc;
	u16					batt_id_trim_cnst_rds;
	int					rds_trim_default_type;
	bool					rds_trim_default_check;
	int32_t					rsense_workaround_value;
	struct sensor_device_attribute		sens_attr[0];
};

LIST_HEAD(qpnp_iadc_device_list);

enum qpnp_iadc_rsense_rds_workaround {
	QPNP_IADC_RDS_DEFAULT_TYPEA,
	QPNP_IADC_RDS_DEFAULT_TYPEB,
};

static int32_t qpnp_iadc_read_reg(struct qpnp_iadc_chip *iadc,
						uint32_t reg, u8 *data)
{
	int rc;

	rc = spmi_ext_register_readl(iadc->adc->spmi->ctrl, iadc->adc->slave,
		(iadc->adc->offset + reg), data, 1);
	if (rc < 0) {
		pr_err("qpnp iadc read reg %d failed with %d\n", reg, rc);
		return rc;
	}

	return 0;
}

static int32_t qpnp_iadc_write_reg(struct qpnp_iadc_chip *iadc,
						uint32_t reg, u8 data)
{
	int rc;
	u8 *buf;

	buf = &data;
	rc = spmi_ext_register_writel(iadc->adc->spmi->ctrl, iadc->adc->slave,
		(iadc->adc->offset + reg), buf, 1);
	if (rc < 0) {
		pr_err("qpnp iadc write reg %d failed with %d\n", reg, rc);
		return rc;
	}

	return 0;
}

static int qpnp_iadc_is_valid(struct qpnp_iadc_chip *iadc)
{
	struct qpnp_iadc_chip *iadc_chip = NULL;

	list_for_each_entry(iadc_chip, &qpnp_iadc_device_list, list)
		if (iadc == iadc_chip)
			return 0;

	return -EINVAL;
}

static void qpnp_iadc_trigger_completion(struct work_struct *work)
{
	struct qpnp_iadc_chip *iadc = container_of(work,
			struct qpnp_iadc_chip, trigger_completion_work);

	if (qpnp_iadc_is_valid(iadc) < 0)
		return;

	complete(&iadc->adc->adc_rslt_completion);

	return;
}

static irqreturn_t qpnp_iadc_isr(int irq, void *dev_id)
{
	struct qpnp_iadc_chip *iadc = dev_id;

	schedule_work(&iadc->trigger_completion_work);

	return IRQ_HANDLED;
}

static int32_t qpnp_iadc_enable(struct qpnp_iadc_chip *dev, bool state)
{
	int rc = 0;
	u8 data = 0;

	data = QPNP_IADC_ADC_EN;
	if (state) {
		rc = qpnp_iadc_write_reg(dev, QPNP_IADC_EN_CTL1,
					data);
		if (rc < 0) {
			pr_err("IADC enable failed\n");
			return rc;
		}
	} else {
		rc = qpnp_iadc_write_reg(dev, QPNP_IADC_EN_CTL1,
					(~data & QPNP_IADC_ADC_EN));
		if (rc < 0) {
			pr_err("IADC disable failed\n");
			return rc;
		}
	}

	return 0;
}

static int32_t qpnp_iadc_status_debug(struct qpnp_iadc_chip *dev)
{
	int rc = 0;
	u8 mode = 0, status1 = 0, chan = 0, dig = 0, en = 0;

	rc = qpnp_iadc_read_reg(dev, QPNP_IADC_MODE_CTL, &mode);
	if (rc < 0) {
		pr_err("mode ctl register read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_read_reg(dev, QPNP_ADC_DIG_PARAM, &dig);
	if (rc < 0) {
		pr_err("digital param read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_read_reg(dev, QPNP_IADC_ADC_CH_SEL_CTL, &chan);
	if (rc < 0) {
		pr_err("channel read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_read_reg(dev, QPNP_STATUS1, &status1);
	if (rc < 0) {
		pr_err("status1 read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_read_reg(dev, QPNP_IADC_EN_CTL1, &en);
	if (rc < 0) {
		pr_err("en read failed with %d\n", rc);
		return rc;
	}

	pr_debug("EOC not set with status:%x, dig:%x, ch:%x, mode:%x, en:%x\n",
			status1, dig, chan, mode, en);

	rc = qpnp_iadc_enable(dev, false);
	if (rc < 0) {
		pr_err("IADC disable failed with %d\n", rc);
		return rc;
	}

	return 0;
}

static int32_t qpnp_iadc_read_conversion_result(struct qpnp_iadc_chip *iadc,
								int16_t *data)
{
	uint8_t rslt_lsb, rslt_msb;
	uint16_t rslt;
	int32_t rc;

	rc = qpnp_iadc_read_reg(iadc, QPNP_IADC_DATA0, &rslt_lsb);
	if (rc < 0) {
		pr_err("qpnp adc result read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_read_reg(iadc, QPNP_IADC_DATA1, &rslt_msb);
	if (rc < 0) {
		pr_err("qpnp adc result read failed with %d\n", rc);
		return rc;
	}

	rslt = (rslt_msb << 8) | rslt_lsb;
	*data = rslt;

	rc = qpnp_iadc_enable(iadc, false);
	if (rc)
		return rc;

	return 0;
}

#define QPNP_IADC_PM8941_3_1_REV2	3
#define QPNP_IADC_PM8941_3_1_REV3	2
#define QPNP_IADC_PM8026_1_REV2		1
#define QPNP_IADC_PM8026_1_REV3		2
#define QPNP_IADC_PM8026_2_REV2		4
#define QPNP_IADC_PM8026_2_REV3		2
#define QPNP_IADC_PM8110_1_REV2		2
#define QPNP_IADC_PM8110_1_REV3		2

#define QPNP_IADC_REV_ID_8941_3_1	1
#define QPNP_IADC_REV_ID_8026_1_0	2
#define QPNP_IADC_REV_ID_8026_2_0	3
#define QPNP_IADC_REV_ID_8110_1_0	4

static void qpnp_temp_comp_version_check(struct qpnp_iadc_chip *iadc,
						int32_t *version)
{
	if ((iadc->iadc_comp.revision_dig_major ==
			QPNP_IADC_PM8941_3_1_REV2) &&
			(iadc->iadc_comp.revision_ana_minor ==
			QPNP_IADC_PM8941_3_1_REV3))
		*version = QPNP_IADC_REV_ID_8941_3_1;
	else if ((iadc->iadc_comp.revision_dig_major ==
			QPNP_IADC_PM8026_1_REV2) &&
			(iadc->iadc_comp.revision_ana_minor ==
			QPNP_IADC_PM8026_1_REV3))
		*version = QPNP_IADC_REV_ID_8026_1_0;
	else if ((iadc->iadc_comp.revision_dig_major ==
			QPNP_IADC_PM8026_2_REV2) &&
			(iadc->iadc_comp.revision_ana_minor ==
			QPNP_IADC_PM8026_2_REV3))
		*version = QPNP_IADC_REV_ID_8026_2_0;
	else if ((iadc->iadc_comp.revision_dig_major ==
			QPNP_IADC_PM8110_1_REV2) &&
			(iadc->iadc_comp.revision_ana_minor ==
			QPNP_IADC_PM8110_1_REV3))
		*version = QPNP_IADC_REV_ID_8110_1_0;
	else
		*version = -EINVAL;

	return;
}

#define QPNP_COEFF_1					969000
#define QPNP_COEFF_2					32
#define QPNP_COEFF_3_TYPEA				1700000
#define QPNP_COEFF_3_TYPEB				1000000
#define QPNP_COEFF_4					100
#define QPNP_COEFF_5					15
#define QPNP_COEFF_6					100000
#define QPNP_COEFF_7					21
#define QPNP_COEFF_8					100000000
#define QPNP_COEFF_9					38
#define QPNP_COEFF_10					40
#define QPNP_COEFF_11					7
#define QPNP_COEFF_12					11
#define QPNP_COEFF_13					37
#define QPNP_COEFF_14					39
#define QPNP_COEFF_15					9
#define QPNP_COEFF_16					11
#define QPNP_COEFF_17					851200
#define QPNP_COEFF_18					296500
#define QPNP_COEFF_19					222400
#define QPNP_COEFF_20					813800
#define QPNP_COEFF_21					1059100
#define QPNP_COEFF_22					5000000
#define QPNP_COEFF_23					3722500
#define QPNP_COEFF_24					84

static int32_t qpnp_iadc_comp(int64_t *result, struct qpnp_iadc_chip *iadc,
							int64_t die_temp)
{
	int64_t temp_var = 0, sys_gain_coeff = 0, old;
	int32_t coeff_a = 0, coeff_b = 0;
	int32_t version;

	qpnp_temp_comp_version_check(iadc, &version);
	if (version == -EINVAL)
		return 0;

	old = *result;
	*result = *result * 1000000;

	if (iadc->iadc_comp.sys_gain > 127)
		sys_gain_coeff = -QPNP_COEFF_6 *
				(iadc->iadc_comp.sys_gain - 128);
	else
		sys_gain_coeff = QPNP_COEFF_6 *
				iadc->iadc_comp.sys_gain;

	switch (version) {
	case QPNP_IADC_REV_ID_8941_3_1:
		switch (iadc->iadc_comp.id) {
		case COMP_ID_GF:
			if (!iadc->iadc_comp.ext_rsense) {
				/* internal rsense */
				coeff_a = QPNP_COEFF_2;
				coeff_b = -QPNP_COEFF_3_TYPEA;
			} else {
				if (*result < 0) {
					/* charge */
					coeff_a = QPNP_COEFF_5;
					coeff_b = QPNP_COEFF_6;
				} else {
					/* discharge */
					coeff_a = -QPNP_COEFF_7;
					coeff_b = QPNP_COEFF_6;
				}
			}
			break;
		case COMP_ID_TSMC:
		default:
			if (!iadc->iadc_comp.ext_rsense) {
				/* internal rsense */
				coeff_a = QPNP_COEFF_2;
				coeff_b = -QPNP_COEFF_3_TYPEB;
			} else {
				if (*result < 0) {
					/* charge */
					coeff_a = QPNP_COEFF_5;
					coeff_b = QPNP_COEFF_6;
				} else {
					/* discharge */
					coeff_a = -QPNP_COEFF_7;
					coeff_b = QPNP_COEFF_6;
				}
			}
			break;
		}
		break;
	case QPNP_IADC_REV_ID_8026_1_0:
		/* pm8026 rev 1.0 */
		switch (iadc->iadc_comp.id) {
		case COMP_ID_GF:
			if (!iadc->iadc_comp.ext_rsense) {
				/* internal rsense */
				if (*result < 0) {
					/* charge */
					coeff_a = QPNP_COEFF_9;
					coeff_b = -QPNP_COEFF_17;
				} else {
					coeff_a = QPNP_COEFF_10;
					coeff_b = QPNP_COEFF_18;
				}
			} else {
				if (*result < 0) {
					/* charge */
					coeff_a = -QPNP_COEFF_11;
					coeff_b = 0;
				} else {
					/* discharge */
					coeff_a = -QPNP_COEFF_17;
					coeff_b = -QPNP_COEFF_19;
				}
			}
			break;
		case COMP_ID_TSMC:
		default:
			if (!iadc->iadc_comp.ext_rsense) {
				/* internal rsense */
				if (*result < 0) {
					/* charge */
					coeff_a = QPNP_COEFF_13;
					coeff_b = -QPNP_COEFF_20;
				} else {
					coeff_a = QPNP_COEFF_14;
					coeff_b = QPNP_COEFF_21;
				}
			} else {
				if (*result < 0) {
					/* charge */
					coeff_a = -QPNP_COEFF_15;
					coeff_b = 0;
				} else {
					/* discharge */
					coeff_a = -QPNP_COEFF_12;
					coeff_b = -QPNP_COEFF_19;
				}
			}
			break;
		}
		break;
	case QPNP_IADC_REV_ID_8110_1_0:
		/* pm8110 rev 1.0 */
		switch (iadc->iadc_comp.id) {
		case COMP_ID_GF:
			if (!iadc->iadc_comp.ext_rsense) {
				/* internal rsense */
				if (*result < 0) {
					/* charge */
					coeff_a = QPNP_COEFF_24;
					coeff_b = -QPNP_COEFF_22;
				} else {
					coeff_a = QPNP_COEFF_24;
					coeff_b = -QPNP_COEFF_23;
				}
			}
			break;
		case COMP_ID_SMIC:
		default:
			if (!iadc->iadc_comp.ext_rsense) {
				/* internal rsense */
				if (*result < 0) {
					/* charge */
					coeff_a = QPNP_COEFF_24;
					coeff_b = -QPNP_COEFF_22;
				} else {
					coeff_a = QPNP_COEFF_24;
					coeff_b = -QPNP_COEFF_23;
				}
			}
			break;
		}
		break;
	default:
	case QPNP_IADC_REV_ID_8026_2_0:
		/* pm8026 rev 1.0 */
		coeff_a = 0;
		coeff_b = 0;
		break;
	}

	temp_var = (coeff_a * die_temp) + coeff_b;
	temp_var = div64_s64(temp_var, QPNP_COEFF_4);
	temp_var = 1000 * (1000000 - temp_var);

	if (!iadc->iadc_comp.ext_rsense) {
		/* internal rsense */
		*result = div64_s64(*result * 1000, temp_var);
	}

	if (iadc->iadc_comp.ext_rsense) {
		/* external rsense */
		sys_gain_coeff = (1000000 +
			div64_s64(sys_gain_coeff, QPNP_COEFF_4));
		temp_var = div64_s64(temp_var * sys_gain_coeff, 1000000);
		*result = div64_s64(*result * 1000, temp_var);
	}
	pr_debug("%lld compensated into %lld\n", old, *result);

	return 0;
}

int32_t qpnp_iadc_comp_result(struct qpnp_iadc_chip *iadc, int64_t *result)
{
	return qpnp_iadc_comp(result, iadc, iadc->die_temp);
}
EXPORT_SYMBOL(qpnp_iadc_comp_result);

static int qpnp_iadc_rds_trim_update_check(struct qpnp_iadc_chip *iadc)
{
	int rc = 0;
	u8 trim2_val = 0, smbb_batt_trm_data = 0;

	if (!iadc->rds_trim_default_check) {
		pr_debug("No internal rds trim check needed\n");
		return 0;
	}

	rc = qpnp_iadc_read_reg(iadc, QPNP_IADC_NOMINAL_RSENSE, &trim2_val);
	if (rc < 0) {
		pr_err("qpnp adc trim2_fullscale1 reg read failed %d\n", rc);
		return rc;
	}

	rc = spmi_ext_register_readl(iadc->adc->spmi->ctrl, iadc->adc->slave,
		iadc->batt_id_trim_cnst_rds, &smbb_batt_trm_data, 1);
	if (rc < 0) {
		pr_err("batt_id trim_cnst rds reg read failed %d\n", rc);
		return rc;
	}

	pr_debug("n_trim:0x%x smb_trm:0x%x\n", trim2_val, smbb_batt_trm_data);

	if (iadc->rds_trim_default_type == QPNP_IADC_RDS_DEFAULT_TYPEA) {
		if (((smbb_batt_trm_data & SMBB_BAT_IF_TRIM_CNST_RDS_MASK) ==
				SMBB_BAT_IF_TRIM_CNST_RDS_MASK_CONST) &&
		(trim2_val == QPNP_IADC1_USR_TRIM2_ADC_FULLSCALE1_CONST)) {
			iadc->rsense_workaround_value =
					QPNP_IADC_RSENSE_DEFAULT_VALUE;
			iadc->default_internal_rsense = true;
		}
	} else if (iadc->rds_trim_default_type ==
						QPNP_IADC_RDS_DEFAULT_TYPEB) {
		if (((smbb_batt_trm_data & SMBB_BAT_IF_TRIM_CNST_RDS_MASK) >=
				SMBB_BAT_IF_TRIM_CNST_RDS_MASK_CONST) &&
		(trim2_val == QPNP_IADC1_USR_TRIM2_ADC_FULLSCALE1_CONST)) {
			iadc->rsense_workaround_value =
					QPNP_IADC_RSENSE_DEFAULT_VALUE;
				iadc->default_internal_rsense = true;
		} else if (((smbb_batt_trm_data &
			SMBB_BAT_IF_TRIM_CNST_RDS_MASK)
			< SMBB_BAT_IF_TRIM_CNST_RDS_MASK_CONST) &&
			(trim2_val ==
				QPNP_IADC1_USR_TRIM2_ADC_FULLSCALE1_CONST)) {
			if (iadc->iadc_comp.id == COMP_ID_GF) {
				iadc->rsense_workaround_value =
					QPNP_IADC_RSENSE_DEFAULT_TYPEB_GF;
				iadc->default_internal_rsense = true;
			} else if (iadc->iadc_comp.id == COMP_ID_SMIC) {
				iadc->rsense_workaround_value =
					QPNP_IADC_RSENSE_DEFAULT_TYPEB_SMIC;
				iadc->default_internal_rsense = true;
			}
		}
	}

	return 0;
}

static int32_t qpnp_iadc_comp_info(struct qpnp_iadc_chip *iadc)
{
	int rc = 0;

	rc = qpnp_iadc_read_reg(iadc, QPNP_INT_TEST_VAL, &iadc->iadc_comp.id);
	if (rc < 0) {
		pr_err("qpnp adc comp id failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_read_reg(iadc, QPNP_IADC_REVISION2,
					&iadc->iadc_comp.revision_dig_major);
	if (rc < 0) {
		pr_err("qpnp adc revision2 read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_read_reg(iadc, QPNP_IADC_REVISION3,
					&iadc->iadc_comp.revision_ana_minor);
	if (rc < 0) {
		pr_err("qpnp adc revision3 read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_read_reg(iadc, QPNP_IADC_ATE_GAIN_CALIB_OFFSET,
						&iadc->iadc_comp.sys_gain);
	if (rc < 0) {
		pr_err("full scale read failed with %d\n", rc);
		return rc;
	}

	if (iadc->external_rsense)
		iadc->iadc_comp.ext_rsense = true;

	pr_debug("fab id = %u, revision_dig_major = %u, revision_ana_minor = %u sys gain = %u, external_rsense = %d\n",
			iadc->iadc_comp.id,
			iadc->iadc_comp.revision_dig_major,
			iadc->iadc_comp.revision_ana_minor,
			iadc->iadc_comp.sys_gain,
			iadc->iadc_comp.ext_rsense);
	return rc;
}

static int32_t qpnp_iadc_configure(struct qpnp_iadc_chip *iadc,
					enum qpnp_iadc_channels channel,
					uint16_t *raw_code, uint32_t mode_sel)
{
	u8 qpnp_iadc_mode_reg = 0, qpnp_iadc_ch_sel_reg = 0;
	u8 qpnp_iadc_conv_req = 0, qpnp_iadc_dig_param_reg = 0;
	u8 status1 = 0;
	uint32_t count = 0;
	int32_t rc = 0;

	qpnp_iadc_ch_sel_reg = channel;

	qpnp_iadc_dig_param_reg |= iadc->adc->amux_prop->decimation <<
					QPNP_IADC_DEC_RATIO_SEL;
	if (iadc->iadc_mode_sel)
		qpnp_iadc_mode_reg |= (QPNP_ADC_TRIM_EN | QPNP_VADC_SYNCH_EN);
	else
		qpnp_iadc_mode_reg |= QPNP_ADC_TRIM_EN;

	qpnp_iadc_conv_req = QPNP_IADC_CONV_REQ;

	rc = qpnp_iadc_write_reg(iadc, QPNP_IADC_MODE_CTL, qpnp_iadc_mode_reg);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_write_reg(iadc, QPNP_IADC_ADC_CH_SEL_CTL,
						qpnp_iadc_ch_sel_reg);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_write_reg(iadc, QPNP_ADC_DIG_PARAM,
						qpnp_iadc_dig_param_reg);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_write_reg(iadc, QPNP_HW_SETTLE_DELAY,
				iadc->adc->amux_prop->hw_settle_time);
	if (rc < 0) {
		pr_err("qpnp adc configure error for hw settling time setup\n");
		return rc;
	}

	rc = qpnp_iadc_write_reg(iadc, QPNP_FAST_AVG_CTL,
					iadc->adc->amux_prop->fast_avg_setup);
	if (rc < 0) {
		pr_err("qpnp adc fast averaging configure error\n");
		return rc;
	}

	if (!iadc->iadc_poll_eoc)
		INIT_COMPLETION(iadc->adc->adc_rslt_completion);

	rc = qpnp_iadc_enable(iadc, true);
	if (rc)
		return rc;

	rc = qpnp_iadc_write_reg(iadc, QPNP_CONV_REQ, qpnp_iadc_conv_req);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		return rc;
	}

	if (iadc->iadc_poll_eoc) {
		while (status1 != QPNP_STATUS1_EOC) {
			rc = qpnp_iadc_read_reg(iadc, QPNP_STATUS1, &status1);
			if (rc < 0)
				return rc;
			status1 &= QPNP_STATUS1_REQ_STS_EOC_MASK;
			usleep_range(QPNP_ADC_CONV_TIME_MIN,
					QPNP_ADC_CONV_TIME_MAX);
			count++;
			if (count > QPNP_ADC_ERR_COUNT) {
				pr_err("retry error exceeded\n");
				rc = qpnp_iadc_status_debug(iadc);
				if (rc < 0)
					pr_err("IADC status debug failed\n");
				rc = -EINVAL;
				return rc;
			}
		}
	} else {
		rc = wait_for_completion_timeout(
				&iadc->adc->adc_rslt_completion,
				QPNP_ADC_COMPLETION_TIMEOUT);
		if (!rc) {
			rc = qpnp_iadc_read_reg(iadc, QPNP_STATUS1, &status1);
			if (rc < 0)
				return rc;
			status1 &= QPNP_STATUS1_REQ_STS_EOC_MASK;
			if (status1 == QPNP_STATUS1_EOC)
				pr_debug("End of conversion status set\n");
			else {
				rc = qpnp_iadc_status_debug(iadc);
				if (rc < 0) {
					pr_err("status debug failed %d\n", rc);
					return rc;
				}
				return -EINVAL;
			}
		}
	}

	rc = qpnp_iadc_read_conversion_result(iadc, raw_code);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		return rc;
	}

	return 0;
}

#define IADC_CENTER	0xC000
#define IADC_READING_RESOLUTION_N	542535
#define IADC_READING_RESOLUTION_D	100000
static int32_t qpnp_convert_raw_offset_voltage(struct qpnp_iadc_chip *iadc)
{
	s64 numerator;

	if ((iadc->adc->calib.gain_raw - iadc->adc->calib.offset_raw) == 0) {
		pr_err("raw offset errors! raw_gain:0x%x and raw_offset:0x%x\n",
			iadc->adc->calib.gain_raw, iadc->adc->calib.offset_raw);
		return -EINVAL;
	}

	numerator = iadc->adc->calib.offset_raw - IADC_CENTER;
	numerator *= IADC_READING_RESOLUTION_N;
	iadc->adc->calib.offset_uv = div_s64(numerator,
						IADC_READING_RESOLUTION_D);

	numerator = iadc->adc->calib.gain_raw - iadc->adc->calib.offset_raw;
	numerator *= IADC_READING_RESOLUTION_N;

	iadc->adc->calib.gain_uv = div_s64(numerator,
						IADC_READING_RESOLUTION_D);

	pr_debug("gain_uv:%d offset_uv:%d\n",
			iadc->adc->calib.gain_uv, iadc->adc->calib.offset_uv);
	return 0;
}

#define IADC_IDEAL_RAW_GAIN	3291
int32_t qpnp_iadc_calibrate_for_trim(struct qpnp_iadc_chip *iadc,
							bool batfet_closed)
{
	uint8_t rslt_lsb, rslt_msb;
	int32_t rc = 0;
	uint16_t raw_data;
	uint32_t mode_sel = 0;

	if (qpnp_iadc_is_valid(iadc) < 0)
		return -EPROBE_DEFER;

	mutex_lock(&iadc->adc->adc_lock);

	if (iadc->iadc_poll_eoc) {
		pr_debug("acquiring iadc eoc wakelock\n");
		pm_stay_awake(iadc->dev);
	}

	rc = qpnp_iadc_configure(iadc, GAIN_CALIBRATION_17P857MV,
						&raw_data, mode_sel);
	if (rc < 0) {
		pr_err("qpnp adc result read failed with %d\n", rc);
		goto fail;
	}

	iadc->adc->calib.gain_raw = raw_data;

	/*
	 * there is a features in the BMS where if the batfet is opened
	 * the BMS reads from INTERNAL_RSENSE (channel 0) actually go to
	 * OFFSET_CALIBRATION_CSP_CSN (channel 5). Hence if batfet is opened
	 * we have to calibrate based on OFFSET_CALIBRATION_CSP_CSN even for
	 * internal rsense.
	 */
	if (!batfet_closed || iadc->external_rsense) {
		/* external offset calculation */
		rc = qpnp_iadc_configure(iadc, OFFSET_CALIBRATION_CSP_CSN,
						&raw_data, mode_sel);
		if (rc < 0) {
			pr_err("qpnp adc result read failed with %d\n", rc);
			goto fail;
		}
	} else {
		/* internal offset calculation */
		rc = qpnp_iadc_configure(iadc, OFFSET_CALIBRATION_CSP2_CSN2,
						&raw_data, mode_sel);
		if (rc < 0) {
			pr_err("qpnp adc result read failed with %d\n", rc);
			goto fail;
		}
	}

	iadc->adc->calib.offset_raw = raw_data;
	if (rc < 0) {
		pr_err("qpnp adc offset/gain calculation failed\n");
		goto fail;
	}

	if (iadc->iadc_comp.revision_dig_major == QPNP_IADC_PM8026_2_REV2
		&& iadc->iadc_comp.revision_ana_minor ==
						QPNP_IADC_PM8026_2_REV3)
		iadc->adc->calib.gain_raw =
			iadc->adc->calib.offset_raw + IADC_IDEAL_RAW_GAIN;

	pr_debug("raw gain:0x%x, raw offset:0x%x\n",
		iadc->adc->calib.gain_raw, iadc->adc->calib.offset_raw);

	rc = qpnp_convert_raw_offset_voltage(iadc);
	if (rc < 0) {
		pr_err("qpnp raw_voltage conversion failed\n");
		goto fail;
	}

	rslt_msb = (raw_data & QPNP_RAW_CODE_16_BIT_MSB_MASK) >>
							QPNP_BIT_SHIFT_8;
	rslt_lsb = raw_data & QPNP_RAW_CODE_16_BIT_LSB_MASK;

	pr_debug("trim values:lsb:0x%x and msb:0x%x\n", rslt_lsb, rslt_msb);

	rc = qpnp_iadc_write_reg(iadc, QPNP_IADC_SEC_ACCESS,
					QPNP_IADC_SEC_ACCESS_DATA);
	if (rc < 0) {
		pr_err("qpnp iadc configure error for sec access\n");
		goto fail;
	}

	rc = qpnp_iadc_write_reg(iadc, QPNP_IADC_MSB_OFFSET,
						rslt_msb);
	if (rc < 0) {
		pr_err("qpnp iadc configure error for MSB write\n");
		goto fail;
	}

	rc = qpnp_iadc_write_reg(iadc, QPNP_IADC_SEC_ACCESS,
					QPNP_IADC_SEC_ACCESS_DATA);
	if (rc < 0) {
		pr_err("qpnp iadc configure error for sec access\n");
		goto fail;
	}

	rc = qpnp_iadc_write_reg(iadc, QPNP_IADC_LSB_OFFSET,
						rslt_lsb);
	if (rc < 0) {
		pr_err("qpnp iadc configure error for LSB write\n");
		goto fail;
	}
fail:
	if (iadc->iadc_poll_eoc) {
		pr_debug("releasing iadc eoc wakelock\n");
		pm_relax(iadc->dev);
	}
	mutex_unlock(&iadc->adc->adc_lock);
	return rc;
}
EXPORT_SYMBOL(qpnp_iadc_calibrate_for_trim);

static void qpnp_iadc_work(struct work_struct *work)
{
	struct qpnp_iadc_chip *iadc = container_of(work,
			struct qpnp_iadc_chip, iadc_work.work);
	int rc = 0;

	if (!iadc->skip_auto_calibrations) {
		rc = qpnp_iadc_calibrate_for_trim(iadc, true);
		if (rc)
			pr_debug("periodic IADC calibration failed\n");
	}

	schedule_delayed_work(&iadc->iadc_work,
		round_jiffies_relative(msecs_to_jiffies
				(QPNP_IADC_CALIB_SECONDS)));
	return;
}

static int32_t qpnp_iadc_version_check(struct qpnp_iadc_chip *iadc)
{
	uint8_t revision;
	int rc;

	rc = qpnp_iadc_read_reg(iadc, QPNP_IADC_REVISION2, &revision);
	if (rc < 0) {
		pr_err("qpnp adc result read failed with %d\n", rc);
		return rc;
	}

	if (revision < QPNP_IADC_SUPPORTED_REVISION2) {
		pr_err("IADC Version not supported\n");
		return -EINVAL;
	}

	return 0;
}

struct qpnp_iadc_chip *qpnp_get_iadc(struct device *dev, const char *name)
{
	struct qpnp_iadc_chip *iadc;
	struct device_node *node = NULL;
	char prop_name[QPNP_MAX_PROP_NAME_LEN];

	snprintf(prop_name, QPNP_MAX_PROP_NAME_LEN, "qcom,%s-iadc", name);

	node = of_parse_phandle(dev->of_node, prop_name, 0);
	if (node == NULL)
		return ERR_PTR(-ENODEV);

	list_for_each_entry(iadc, &qpnp_iadc_device_list, list)
		if (iadc->adc->spmi->dev.of_node == node)
			return iadc;
	return ERR_PTR(-EPROBE_DEFER);
}
EXPORT_SYMBOL(qpnp_get_iadc);

int32_t qpnp_iadc_get_rsense(struct qpnp_iadc_chip *iadc, int32_t *rsense)
{
	uint8_t	rslt_rsense;
	int32_t	rc = 0, sign_bit = 0;

	if (qpnp_iadc_is_valid(iadc) < 0)
		return -EPROBE_DEFER;

	if (iadc->external_rsense) {
		*rsense = iadc->rsense;
		return rc;
	}

	if (iadc->default_internal_rsense) {
		*rsense = iadc->rsense_workaround_value;
		return rc;
	}

	rc = qpnp_iadc_read_reg(iadc, QPNP_IADC_NOMINAL_RSENSE, &rslt_rsense);
	if (rc < 0) {
		pr_err("qpnp adc rsense read failed with %d\n", rc);
		return rc;
	}

	pr_debug("rsense:0%x\n", rslt_rsense);

	if (rslt_rsense & QPNP_RSENSE_MSB_SIGN_CHECK)
		sign_bit = 1;

	rslt_rsense &= ~QPNP_RSENSE_MSB_SIGN_CHECK;

	if (sign_bit)
		*rsense = QPNP_IADC_INTERNAL_RSENSE_N_OHMS_FACTOR -
			(rslt_rsense * QPNP_IADC_RSENSE_LSB_N_OHMS_PER_BIT);
	else
		*rsense = QPNP_IADC_INTERNAL_RSENSE_N_OHMS_FACTOR +
			(rslt_rsense * QPNP_IADC_RSENSE_LSB_N_OHMS_PER_BIT);

	pr_debug("rsense value is %d\n", *rsense);

	return rc;
}
EXPORT_SYMBOL(qpnp_iadc_get_rsense);

static int32_t qpnp_check_pmic_temp(struct qpnp_iadc_chip *iadc)
{
	struct qpnp_vadc_result result_pmic_therm;
	int64_t die_temp_offset;
	int rc = 0;

	rc = qpnp_vadc_read(iadc->vadc_dev, DIE_TEMP, &result_pmic_therm);
	if (rc < 0)
		return rc;

	die_temp_offset = result_pmic_therm.physical -
			iadc->die_temp;
	if (die_temp_offset < 0)
		die_temp_offset = -die_temp_offset;

	if (die_temp_offset > QPNP_IADC_DIE_TEMP_CALIB_OFFSET) {
		iadc->die_temp = result_pmic_therm.physical;
		if (!iadc->skip_auto_calibrations) {
			rc = qpnp_iadc_calibrate_for_trim(iadc, true);
			if (rc)
				pr_err("IADC calibration failed rc = %d\n", rc);
		}
	}

	return rc;
}

int32_t qpnp_iadc_read(struct qpnp_iadc_chip *iadc,
				enum qpnp_iadc_channels channel,
				struct qpnp_iadc_result *result)
{
	int32_t rc, rsense_n_ohms, sign = 0, num, mode_sel = 0;
	int32_t rsense_u_ohms = 0;
	int64_t result_current;
	uint16_t raw_data;

	if (qpnp_iadc_is_valid(iadc) < 0)
		return -EPROBE_DEFER;

	if ((iadc->adc->calib.gain_raw - iadc->adc->calib.offset_raw) == 0) {
		pr_err("raw offset errors! run iadc calibration again\n");
		return -EINVAL;
	}

	rc = qpnp_check_pmic_temp(iadc);
	if (rc) {
		pr_err("Error checking pmic therm temp\n");
		return rc;
	}

	mutex_lock(&iadc->adc->adc_lock);

	if (iadc->iadc_poll_eoc) {
		pr_debug("acquiring iadc eoc wakelock\n");
		pm_stay_awake(iadc->dev);
	}

	rc = qpnp_iadc_configure(iadc, channel, &raw_data, mode_sel);
	if (rc < 0) {
		pr_err("qpnp adc result read failed with %d\n", rc);
		goto fail;
	}

	rc = qpnp_iadc_get_rsense(iadc, &rsense_n_ohms);
	pr_debug("current raw:0%x and rsense:%d\n",
			raw_data, rsense_n_ohms);
	rsense_u_ohms = rsense_n_ohms/1000;
	num = raw_data - iadc->adc->calib.offset_raw;
	if (num < 0) {
		sign = 1;
		num = -num;
	}

	result->result_uv = (num * QPNP_ADC_GAIN_NV)/
		(iadc->adc->calib.gain_raw - iadc->adc->calib.offset_raw);
	result_current = result->result_uv;
	result_current *= QPNP_IADC_NANO_VOLTS_FACTOR;
	/* Intentional fall through. Process the result w/o comp */
	do_div(result_current, rsense_u_ohms);

	if (sign) {
		result->result_uv = -result->result_uv;
		result_current = -result_current;
	}
	rc = qpnp_iadc_comp_result(iadc, &result_current);
	if (rc < 0)
		pr_err("Error during compensating the IADC\n");
	rc = 0;

	result->result_ua = (int32_t) result_current;
fail:
	if (iadc->iadc_poll_eoc) {
		pr_debug("releasing iadc eoc wakelock\n");
		pm_relax(iadc->dev);
	}
	mutex_unlock(&iadc->adc->adc_lock);

	return rc;
}
EXPORT_SYMBOL(qpnp_iadc_read);

int32_t qpnp_iadc_get_gain_and_offset(struct qpnp_iadc_chip *iadc,
					struct qpnp_iadc_calib *result)
{
	int rc;

	if (qpnp_iadc_is_valid(iadc) < 0)
		return -EPROBE_DEFER;

	rc = qpnp_check_pmic_temp(iadc);
	if (rc) {
		pr_err("Error checking pmic therm temp\n");
		return rc;
	}

	mutex_lock(&iadc->adc->adc_lock);
	result->gain_raw = iadc->adc->calib.gain_raw;
	result->ideal_gain_nv = QPNP_ADC_GAIN_NV;
	result->gain_uv = iadc->adc->calib.gain_uv;
	result->offset_raw = iadc->adc->calib.offset_raw;
	result->ideal_offset_uv =
				QPNP_OFFSET_CALIBRATION_SHORT_CADC_LEADS_IDEAL;
	result->offset_uv = iadc->adc->calib.offset_uv;
	pr_debug("raw gain:0%x, raw offset:0%x\n",
			result->gain_raw, result->offset_raw);
	pr_debug("gain_uv:%d offset_uv:%d\n",
			result->gain_uv, result->offset_uv);
	mutex_unlock(&iadc->adc->adc_lock);

	return 0;
}
EXPORT_SYMBOL(qpnp_iadc_get_gain_and_offset);

int qpnp_iadc_skip_calibration(struct qpnp_iadc_chip *iadc)
{
	iadc->skip_auto_calibrations = true;
	return 0;
}
EXPORT_SYMBOL(qpnp_iadc_skip_calibration);

int qpnp_iadc_resume_calibration(struct qpnp_iadc_chip *iadc)
{
	iadc->skip_auto_calibrations = false;
	return 0;
}
EXPORT_SYMBOL(qpnp_iadc_resume_calibration);

int32_t qpnp_iadc_vadc_sync_read(struct qpnp_iadc_chip *iadc,
	enum qpnp_iadc_channels i_channel, struct qpnp_iadc_result *i_result,
	enum qpnp_vadc_channels v_channel, struct qpnp_vadc_result *v_result)
{
	int rc = 0, mode_sel = 0, num = 0, rsense_n_ohms = 0, sign = 0;
	uint16_t raw_data;
	int32_t rsense_u_ohms = 0;
	int64_t result_current;

	if (qpnp_iadc_is_valid(iadc) < 0)
		return -EPROBE_DEFER;

	mutex_lock(&iadc->adc->adc_lock);

	if (iadc->iadc_poll_eoc) {
		pr_debug("acquiring iadc eoc wakelock\n");
		pm_stay_awake(iadc->dev);
	}

	iadc->iadc_mode_sel = true;

	rc = qpnp_vadc_iadc_sync_request(iadc->vadc_dev, v_channel);
	if (rc) {
		pr_err("Configuring VADC failed\n");
		goto fail;
	}

	rc = qpnp_iadc_configure(iadc, i_channel, &raw_data, mode_sel);
	if (rc < 0) {
		pr_err("qpnp adc result read failed with %d\n", rc);
		goto fail_release_vadc;
	}

	rc = qpnp_iadc_get_rsense(iadc, &rsense_n_ohms);
	pr_debug("current raw:0%x and rsense:%d\n",
			raw_data, rsense_n_ohms);
	rsense_u_ohms = rsense_n_ohms/1000;
	num = raw_data - iadc->adc->calib.offset_raw;
	if (num < 0) {
		sign = 1;
		num = -num;
	}

	i_result->result_uv = (num * QPNP_ADC_GAIN_NV)/
		(iadc->adc->calib.gain_raw - iadc->adc->calib.offset_raw);
	result_current = i_result->result_uv;
	result_current *= QPNP_IADC_NANO_VOLTS_FACTOR;
	/* Intentional fall through. Process the result w/o comp */
	do_div(result_current, rsense_u_ohms);

	if (sign) {
		i_result->result_uv = -i_result->result_uv;
		result_current = -result_current;
	}
	result_current *= -1;
	rc = qpnp_iadc_comp_result(iadc, &result_current);
	if (rc < 0)
		pr_err("Error during compensating the IADC\n");
	rc = 0;
	result_current *= -1;

	i_result->result_ua = (int32_t) result_current;

fail_release_vadc:
	rc = qpnp_vadc_iadc_sync_complete_request(iadc->vadc_dev, v_channel,
							v_result);
	if (rc)
		pr_err("Releasing VADC failed\n");
fail:
	iadc->iadc_mode_sel = false;

	if (iadc->iadc_poll_eoc) {
		pr_debug("releasing iadc eoc wakelock\n");
		pm_relax(iadc->dev);
	}
	mutex_unlock(&iadc->adc->adc_lock);

	return rc;
}
EXPORT_SYMBOL(qpnp_iadc_vadc_sync_read);

static ssize_t qpnp_iadc_show(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct qpnp_iadc_chip *iadc = dev_get_drvdata(dev);
	struct qpnp_iadc_result result;
	int rc = -1;

	rc = qpnp_iadc_read(iadc, attr->index, &result);

	if (rc)
		return 0;

	return snprintf(buf, QPNP_ADC_HWMON_NAME_LENGTH,
					"Result:%d\n", result.result_ua);
}

static struct sensor_device_attribute qpnp_adc_attr =
	SENSOR_ATTR(NULL, S_IRUGO, qpnp_iadc_show, NULL, 0);

static int32_t qpnp_iadc_init_hwmon(struct qpnp_iadc_chip *iadc,
						struct spmi_device *spmi)
{
	struct device_node *child;
	struct device_node *node = spmi->dev.of_node;
	int rc = 0, i = 0, channel;

	for_each_child_of_node(node, child) {
		channel = iadc->adc->adc_channels[i].channel_num;
		qpnp_adc_attr.index = iadc->adc->adc_channels[i].channel_num;
		qpnp_adc_attr.dev_attr.attr.name =
						iadc->adc->adc_channels[i].name;
		memcpy(&iadc->sens_attr[i], &qpnp_adc_attr,
						sizeof(qpnp_adc_attr));
		sysfs_attr_init(&iadc->sens_attr[i].dev_attr.attr);
		rc = device_create_file(&spmi->dev,
				&iadc->sens_attr[i].dev_attr);
		if (rc) {
			dev_err(&spmi->dev,
				"device_create_file failed for dev %s\n",
				iadc->adc->adc_channels[i].name);
			goto hwmon_err_sens;
		}
		i++;
	}

	return 0;
hwmon_err_sens:
	pr_err("Init HWMON failed for qpnp_iadc with %d\n", rc);
	return rc;
}

static int __devinit qpnp_iadc_probe(struct spmi_device *spmi)
{
	struct qpnp_iadc_chip *iadc;
	struct qpnp_adc_drv *adc_qpnp;
	struct device_node *node = spmi->dev.of_node;
	struct device_node *child;
	struct resource *res;
	int rc, count_adc_channel_list = 0, i = 0;

	for_each_child_of_node(node, child)
		count_adc_channel_list++;

	if (!count_adc_channel_list) {
		pr_err("No channel listing\n");
		return -EINVAL;
	}

	iadc = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_iadc_chip) +
		(sizeof(struct sensor_device_attribute) *
				count_adc_channel_list), GFP_KERNEL);
	if (!iadc) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	adc_qpnp = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_adc_drv),
			GFP_KERNEL);
	if (!adc_qpnp) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	iadc->dev = &(spmi->dev);
	iadc->adc = adc_qpnp;

	rc = qpnp_adc_get_devicetree_data(spmi, iadc->adc);
	if (rc) {
		dev_err(&spmi->dev, "failed to read device tree\n");
		return rc;
	}

	res = spmi_get_resource_byname(spmi, NULL, IORESOURCE_MEM,
		"batt-id-trim-cnst-rds");
	if (!res) {
		dev_err(&spmi->dev, "failed to read batt_id trim register\n");
		return -EINVAL;
	}
	iadc->batt_id_trim_cnst_rds = res->start;
	rc = of_property_read_u32(node, "qcom,use-default-rds-trim",
			&iadc->rds_trim_default_type);
	if (rc)
		pr_debug("No trim workaround needed\n");
	else {
		pr_debug("Use internal RDS trim workaround\n");
		iadc->rds_trim_default_check = true;
	}

	iadc->vadc_dev = qpnp_get_vadc(&spmi->dev, "iadc");
	if (IS_ERR(iadc->vadc_dev)) {
		rc = PTR_ERR(iadc->vadc_dev);
		if (rc != -EPROBE_DEFER)
			pr_err("vadc property missing, rc=%d\n", rc);
		return rc;
	}

	mutex_init(&iadc->adc->adc_lock);

	rc = of_property_read_u32(node, "qcom,rsense",
			&iadc->rsense);
	if (rc)
		pr_debug("Defaulting to internal rsense\n");
	else {
		pr_debug("Use external rsense\n");
		iadc->external_rsense = true;
	}

	iadc->iadc_poll_eoc = of_property_read_bool(node,
						"qcom,iadc-poll-eoc");
	if (!iadc->iadc_poll_eoc) {
		rc = devm_request_irq(&spmi->dev, iadc->adc->adc_irq_eoc,
				qpnp_iadc_isr, IRQF_TRIGGER_RISING,
				"qpnp_iadc_interrupt", iadc);
		if (rc) {
			dev_err(&spmi->dev, "failed to request adc irq\n");
			return rc;
		} else
			enable_irq_wake(iadc->adc->adc_irq_eoc);
	}

	rc = qpnp_iadc_init_hwmon(iadc, spmi);
	if (rc) {
		dev_err(&spmi->dev, "failed to initialize qpnp hwmon adc\n");
		return rc;
	}
	iadc->iadc_hwmon = hwmon_device_register(&iadc->adc->spmi->dev);

	rc = qpnp_iadc_version_check(iadc);
	if (rc) {
		dev_err(&spmi->dev, "IADC version not supported\n");
		goto fail;
	}

	INIT_WORK(&iadc->trigger_completion_work, qpnp_iadc_trigger_completion);
	INIT_DELAYED_WORK(&iadc->iadc_work, qpnp_iadc_work);
	rc = qpnp_iadc_comp_info(iadc);
	if (rc) {
		dev_err(&spmi->dev, "abstracting IADC comp info failed!\n");
		goto fail;
	}

	rc = qpnp_iadc_rds_trim_update_check(iadc);
	if (rc) {
		dev_err(&spmi->dev, "Rds trim update failed!\n");
		goto fail;
	}

	dev_set_drvdata(&spmi->dev, iadc);
	list_add(&iadc->list, &qpnp_iadc_device_list);
	rc = qpnp_iadc_calibrate_for_trim(iadc, true);
	if (rc)
		dev_err(&spmi->dev, "failed to calibrate for USR trim\n");

	if (iadc->iadc_poll_eoc)
		device_init_wakeup(iadc->dev, 1);

	schedule_delayed_work(&iadc->iadc_work,
			round_jiffies_relative(msecs_to_jiffies
					(QPNP_IADC_CALIB_SECONDS)));
	return 0;
fail:
	for_each_child_of_node(node, child) {
		device_remove_file(&spmi->dev,
			&iadc->sens_attr[i].dev_attr);
		i++;
	}
	hwmon_device_unregister(iadc->iadc_hwmon);

	return rc;
}

static int __devexit qpnp_iadc_remove(struct spmi_device *spmi)
{
	struct qpnp_iadc_chip *iadc = dev_get_drvdata(&spmi->dev);
	struct device_node *node = spmi->dev.of_node;
	struct device_node *child;
	int i = 0;

	cancel_delayed_work(&iadc->iadc_work);
	for_each_child_of_node(node, child) {
		device_remove_file(&spmi->dev,
			&iadc->sens_attr[i].dev_attr);
		i++;
	}
	hwmon_device_unregister(iadc->iadc_hwmon);
	if (iadc->iadc_poll_eoc)
		pm_relax(iadc->dev);
	dev_set_drvdata(&spmi->dev, NULL);

	return 0;
}

static const struct of_device_id qpnp_iadc_match_table[] = {
	{	.compatible = "qcom,qpnp-iadc",
	},
	{}
};

static struct spmi_driver qpnp_iadc_driver = {
	.driver		= {
		.name	= "qcom,qpnp-iadc",
		.of_match_table = qpnp_iadc_match_table,
	},
	.probe		= qpnp_iadc_probe,
	.remove		= qpnp_iadc_remove,
};

static int __init qpnp_iadc_init(void)
{
	return spmi_driver_register(&qpnp_iadc_driver);
}
module_init(qpnp_iadc_init);

static void __exit qpnp_iadc_exit(void)
{
	spmi_driver_unregister(&qpnp_iadc_driver);
}
module_exit(qpnp_iadc_exit);

MODULE_DESCRIPTION("QPNP PMIC current ADC driver");
MODULE_LICENSE("GPL v2");
