/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
 * Qualcomm's PM8921 ADC Arbiter driver
 */
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/mpp.h>
#include <linux/mfd/pm8921-adc.h>
#include <linux/debugfs.h>

/* User Bank register set */
#define PM8921_ADC_ARB_USRP_CNTRL1			0x197
#define PM8921_ADC_ARB_USRP_CNTRL1_EN_ARB		BIT(0)
#define PM8921_ADC_ARB_USRP_CNTRL1_RSV1			BIT(1)
#define PM8921_ADC_ARB_USRP_CNTRL1_RSV2			BIT(2)
#define PM8921_ADC_ARB_USRP_CNTRL1_RSV3			BIT(3)
#define PM8921_ADC_ARB_USRP_CNTRL1_RSV4			BIT(4)
#define PM8921_ADC_ARB_USRP_CNTRL1_RSV5			BIT(5)
#define PM8921_ADC_ARB_USRP_CNTRL1_EOC			BIT(6)
#define PM8921_ADC_ARB_USRP_CNTRL1_REQ			BIT(7)

#define PM8921_ADC_ARB_USRP_AMUX_CNTRL			0x198
#define PM8921_ADC_ARB_USRP_AMUX_CNTRL_RSV0		BIT(0)
#define PM8921_ADC_ARB_USRP_AMUX_CNTRL_RSV1		BIT(1)
#define PM8921_ADC_ARB_USRP_AMUX_CNTRL_PREMUX0		BIT(2)
#define PM8921_ADC_ARB_USRP_AMUX_CNTRL_PREMUX1		BIT(3)
#define PM8921_ADC_ARB_USRP_AMUX_CNTRL_SEL0		BIT(4)
#define PM8921_ADC_ARB_USRP_AMUX_CNTRL_SEL1		BIT(5)
#define PM8921_ADC_ARB_USRP_AMUX_CNTRL_SEL2		BIT(6)
#define PM8921_ADC_ARB_USRP_AMUX_CNTRL_SEL3		BIT(7)

#define PM8921_ADC_ARB_USRP_ANA_PARAM			0x199
#define PM8921_ADC_ARB_USRP_DIG_PARAM			0x19A
#define PM8921_ADC_ARB_USRP_DIG_PARAM_SEL_SHIFT0	BIT(0)
#define PM8921_ADC_ARB_USRP_DIG_PARAM_SEL_SHIFT1	BIT(1)
#define PM8921_ADC_ARB_USRP_DIG_PARAM_CLK_RATE0		BIT(2)
#define PM8921_ADC_ARB_USRP_DIG_PARAM_CLK_RATE1		BIT(3)
#define PM8921_ADC_ARB_USRP_DIG_PARAM_EOC		BIT(4)
#define PM8921_ADC_ARB_USRP_DIG_PARAM_DEC_RATE0		BIT(5)
#define PM8921_ADC_ARB_USRP_DIG_PARAM_DEC_RATE1		BIT(6)
#define PM8921_ADC_ARB_USRP_DIG_PARAM_EN		BIT(7)

#define PM8921_ADC_ARB_USRP_RSV				0x19B
#define PM8921_ADC_ARB_USRP_RSV_RST			BIT(0)
#define PM8921_ADC_ARB_USRP_RSV_DTEST0			BIT(1)
#define PM8921_ADC_ARB_USRP_RSV_DTEST1			BIT(2)
#define PM8921_ADC_ARB_USRP_RSV_OP			BIT(3)
#define PM8921_ADC_ARB_USRP_RSV_IP_SEL0			BIT(4)
#define PM8921_ADC_ARB_USRP_RSV_IP_SEL1			BIT(5)
#define PM8921_ADC_ARB_USRP_RSV_IP_SEL2			BIT(6)
#define PM8921_ADC_ARB_USRP_RSV_TRM			BIT(7)

#define PM8921_ADC_ARB_USRP_DATA0			0x19D
#define PM8921_ADC_ARB_USRP_DATA1			0x19C

#define PM8921_ADC_ARB_BTM_CNTRL1			0x17e
#define PM8921_ADC_ARB_BTM_CNTRL1_EN_BTM		BIT(0)
#define PM8921_ADC_ARB_BTM_CNTRL1_SEL_OP_MODE		BIT(1)
#define PM8921_ADC_ARB_BTM_CNTRL1_MEAS_INTERVAL1	BIT(2)
#define PM8921_ADC_ARB_BTM_CNTRL1_MEAS_INTERVAL2	BIT(3)
#define PM8921_ADC_ARB_BTM_CNTRL1_MEAS_INTERVAL3	BIT(4)
#define PM8921_ADC_ARB_BTM_CNTRL1_MEAS_INTERVAL4	BIT(5)
#define PM8921_ADC_ARB_BTM_CNTRL1_EOC			BIT(6)
#define PM8921_ADC_ARB_BTM_CNTRL1_REQ			BIT(7)

#define PM8921_ADC_ARB_BTM_CNTRL2			0x18c
#define PM8921_ADC_ARB_BTM_AMUX_CNTRL			0x17f
#define PM8921_ADC_ARB_BTM_ANA_PARAM			0x180
#define PM8921_ADC_ARB_BTM_DIG_PARAM			0x181
#define PM8921_ADC_ARB_BTM_RSV				0x182
#define PM8921_ADC_ARB_BTM_DATA1			0x183
#define PM8921_ADC_ARB_BTM_DATA0			0x184
#define PM8921_ADC_ARB_BTM_BAT_COOL_THR1		0x185
#define PM8921_ADC_ARB_BTM_BAT_COOL_THR0		0x186
#define PM8921_ADC_ARB_BTM_BAT_WARM_THR1		0x187
#define PM8921_ADC_ARB_BTM_BAT_WARM_THR0		0x188

#define PM8921_ADC_ARB_ANA_DIG				0xa0
#define PM8921_ADC_AMUX_MPP_SEL				2
#define PM8921_ADC_AMUX_SEL				4
#define PM8921_ADC_RSV_IP_SEL				4
#define PM8921_ADC_BTM_CHANNEL_SEL			4
#define PM8921_MAX_CHANNEL_PROPERTIES			2
#define PM8921_ADC_IRQ_0				0
#define PM8921_ADC_IRQ_1				1
#define PM8921_ADC_IRQ_2				2
#define PM8921_ADC_BTM_INTERVAL_SEL			5
#define PM8921_ADC_BTM_DECIMATION_SEL			5
#define PM8921_ADC_MUL					10
#define PM8921_ADC_CONV_TIME_MIN			2000
#define PM8921_ADC_CONV_TIME_MAX			2100

struct pm8921_adc {
	struct device				*dev;
	struct pm8921_adc_properties		*adc_prop;
	int					adc_irq;
	struct mutex				adc_lock;
	struct mutex				btm_lock;
	uint32_t				adc_num_channel;
	struct completion			adc_rslt_completion;
	struct pm8921_adc_amux			*adc_channel;
	struct pm8921_adc_amux_properties	*conv;
	struct pm8921_adc_arb_btm		*batt;
	int					btm_warm_irq;
	int					btm_cold_irq;
	struct dentry				*dent;
};

struct pm8921_adc_amux_properties {
	uint32_t				amux_channel;
	uint32_t				decimation;
	uint32_t				amux_ip_rsv;
	uint32_t				amux_mpp_channel;
	struct pm8921_adc_chan_properties	*chan_prop;
};

static const struct pm8921_adc_scaling_ratio pm8921_amux_scaling_ratio[] = {
	{1, 1},
	{1, 3},
	{1, 4},
	{1, 6}
};

static struct pm8921_adc *pmic_adc;

static struct pm8921_adc_scale_fn adc_scale_fn[] = {
	[ADC_SCALE_DEFAULT] = {pm8921_adc_scale_default},
	[ADC_SCALE_BATT_THERM] = {pm8921_adc_scale_batt_therm},
	[ADC_SCALE_PMIC_THERM] = {pm8921_adc_scale_pmic_therm},
	[ADC_SCALE_XTERN_CHGR_CUR] = {pm8921_adc_scale_xtern_chgr_cur},
};

static bool pm8921_adc_calib_first_adc, pm8921_btm_calib_first_adc;
static bool pm8921_adc_initialized, pm8921_adc_calib_device_init;

static int32_t pm8921_adc_arb_cntrl(uint32_t arb_cntrl)
{
	struct pm8921_adc *adc_pmic = pmic_adc;
	int i, rc;
	u8 data_arb_cntrl = 0;

	if (arb_cntrl)
		data_arb_cntrl |= PM8921_ADC_ARB_USRP_CNTRL1_EN_ARB;

	/* Write twice to the CNTRL register for the arbiter settings
	   to take into effect */
	for (i = 0; i < 2; i++) {
		rc = pm8xxx_writeb(adc_pmic->dev->parent,
				PM8921_ADC_ARB_USRP_CNTRL1, data_arb_cntrl);
		if (rc < 0) {
			pr_err("PM8921 arb cntrl write failed with %d\n", rc);
			return rc;
		}
	}

	if (arb_cntrl) {
		data_arb_cntrl |= PM8921_ADC_ARB_USRP_CNTRL1_REQ;
		rc = pm8xxx_writeb(adc_pmic->dev->parent,
			PM8921_ADC_ARB_USRP_CNTRL1, data_arb_cntrl);
	}

	return 0;
}

static uint32_t pm8921_adc_read_reg(uint32_t reg, u8 *data)
{
	struct pm8921_adc *adc_pmic = pmic_adc;
	int rc;

	rc = pm8xxx_readb(adc_pmic->dev->parent, reg, data);
	if (rc < 0) {
		pr_err("PM8921 adc read reg %d failed with %d\n", reg, rc);
		return rc;
	}

	return 0;
}

static uint32_t pm8921_adc_write_reg(uint32_t reg, u8 data)
{
	struct pm8921_adc *adc_pmic = pmic_adc;
	int rc;

	rc = pm8xxx_writeb(adc_pmic->dev->parent, reg, data);
	if (rc < 0) {
		pr_err("PM8921 adc write reg %d failed with %d\n", reg, rc);
		return rc;
	}

	return 0;
}

static int32_t pm8921_adc_configure(
				struct pm8921_adc_amux_properties *chan_prop)
{
	struct pm8921_adc *adc_pmic = pmic_adc;
	u8 data_amux_chan = 0, data_arb_rsv = 0, data_dig_param = 0;
	int rc;

	data_amux_chan |= chan_prop->amux_channel << PM8921_ADC_AMUX_SEL;

	if (chan_prop->amux_mpp_channel)
		data_amux_chan |= chan_prop->amux_mpp_channel <<
					PM8921_ADC_AMUX_MPP_SEL;

	rc = pm8921_adc_write_reg(PM8921_ADC_ARB_USRP_AMUX_CNTRL,
							data_amux_chan);
	if (rc < 0)
		return rc;

	data_arb_rsv &= (PM8921_ADC_ARB_USRP_RSV_RST |
		PM8921_ADC_ARB_USRP_RSV_DTEST0 |
		PM8921_ADC_ARB_USRP_RSV_DTEST1 |
		PM8921_ADC_ARB_USRP_RSV_OP |
		PM8921_ADC_ARB_USRP_RSV_TRM);
	data_arb_rsv |= chan_prop->amux_ip_rsv << PM8921_ADC_RSV_IP_SEL;

	rc = pm8921_adc_write_reg(PM8921_ADC_ARB_USRP_RSV, data_arb_rsv);
	if (rc < 0)
		return rc;

	rc = pm8921_adc_read_reg(PM8921_ADC_ARB_USRP_DIG_PARAM,
							&data_dig_param);
	if (rc < 0)
		return rc;

	/* Default 2.4Mhz clock rate */
	/* Client chooses the decimation */
	switch (chan_prop->decimation) {
	case ADC_DECIMATION_TYPE1:
		data_dig_param |= PM8921_ADC_ARB_USRP_DIG_PARAM_DEC_RATE0;
		break;
	case ADC_DECIMATION_TYPE2:
		data_dig_param |= (PM8921_ADC_ARB_USRP_DIG_PARAM_DEC_RATE0
				| PM8921_ADC_ARB_USRP_DIG_PARAM_DEC_RATE1);
		break;
	default:
		data_dig_param |= PM8921_ADC_ARB_USRP_DIG_PARAM_DEC_RATE0;
		break;
	}
	rc = pm8921_adc_write_reg(PM8921_ADC_ARB_USRP_DIG_PARAM,
						PM8921_ADC_ARB_ANA_DIG);
	if (rc < 0)
		return rc;

	rc = pm8921_adc_write_reg(PM8921_ADC_ARB_USRP_ANA_PARAM,
						PM8921_ADC_ARB_ANA_DIG);
	if (rc < 0)
		return rc;

	if (!pm8921_adc_calib_first_adc)
		enable_irq(adc_pmic->adc_irq);

	rc = pm8921_adc_arb_cntrl(1);
	if (rc < 0) {
		pr_err("Configuring ADC Arbiter"
				"enable failed with %d\n", rc);
		return rc;
	}

	return 0;
}

static uint32_t pm8921_adc_read_adc_code(int32_t *data)
{
	struct pm8921_adc *adc_pmic = pmic_adc;
	uint8_t rslt_lsb, rslt_msb;
	int32_t rc, max_ideal_adc_code = 1 << adc_pmic->adc_prop->bitresolution;

	rc = pm8xxx_readb(adc_pmic->dev->parent,
				PM8921_ADC_ARB_USRP_DATA0, &rslt_lsb);
	if (rc < 0) {
		pr_err("PM8921 adc result read failed with %d\n", rc);
		return rc;
	}

	rc = pm8xxx_readb(adc_pmic->dev->parent,
				PM8921_ADC_ARB_USRP_DATA1, &rslt_msb);
	if (rc < 0) {
		pr_err("PM8921 adc result read failed with %d\n", rc);
		return rc;
	}

	*data = (rslt_msb << 8) | rslt_lsb;

	/* Use the midpoint to determine underflow or overflow */
	if (*data > max_ideal_adc_code + (max_ideal_adc_code >> 1))
		*data |= ((1 << (8 * sizeof(*data) -
			adc_pmic->adc_prop->bitresolution)) - 1) <<
			adc_pmic->adc_prop->bitresolution;

	/* Default value for switching off the arbiter after reading
	   the ADC value. Bit 0 set to 0. */
	rc = pm8921_adc_arb_cntrl(0);
	if (rc < 0) {
		pr_err("%s: Configuring ADC Arbiter disable"
					"failed\n", __func__);
		return rc;
	}

	return 0;
}

static irqreturn_t pm8921_adc_isr(int irq, void *dev_id)
{
	struct pm8921_adc *adc_8921 = dev_id;

	disable_irq_nosync(adc_8921->adc_irq);

	if (pm8921_adc_calib_first_adc)
		return IRQ_HANDLED;
	/* TODO Handle spurius interrupt condition */
	complete(&adc_8921->adc_rslt_completion);

	return IRQ_HANDLED;
}

static irqreturn_t pm8921_btm_warm_isr(int irq, void *dev_id)
{
	struct pm8921_adc *btm_8921 = dev_id;

	disable_irq_nosync(btm_8921->btm_warm_irq);

	if (pm8921_btm_calib_first_adc)
		return IRQ_HANDLED;

	if (btm_8921->batt->btm_param->btm_warm_fn != NULL)
		btm_8921->batt->btm_param->btm_warm_fn();

	return IRQ_HANDLED;
}

static irqreturn_t pm8921_btm_cold_isr(int irq, void *dev_id)
{
	struct pm8921_adc *btm_8921 = dev_id;

	disable_irq_nosync(btm_8921->btm_cold_irq);

	if (pm8921_btm_calib_first_adc)
		return IRQ_HANDLED;

	if (btm_8921->batt->btm_param->btm_cold_fn != NULL)
		btm_8921->batt->btm_param->btm_cold_fn();

	return IRQ_HANDLED;
}

static uint32_t pm8921_adc_calib_device(void)
{
	struct pm8921_adc *adc_pmic = pmic_adc;
	struct pm8921_adc_amux_properties conv;
	int rc, offset_adc, slope_adc, calib_read_1, calib_read_2;
	u8 data_arb_usrp_cntrl1 = 0;

	conv.amux_channel = CHANNEL_125V;
	conv.decimation = ADC_DECIMATION_TYPE2;
	conv.amux_ip_rsv = AMUX_RSV1;
	conv.amux_mpp_channel = PREMUX_MPP_SCALE_0;
	pm8921_adc_calib_first_adc = true;
	rc = pm8921_adc_configure(&conv);
	if (rc) {
		pr_err("pm8921_adc configure failed with %d\n", rc);
		goto calib_fail;
	}

	while (data_arb_usrp_cntrl1 != (PM8921_ADC_ARB_USRP_CNTRL1_EOC |
					PM8921_ADC_ARB_USRP_CNTRL1_EN_ARB)) {
		rc = pm8921_adc_read_reg(PM8921_ADC_ARB_USRP_CNTRL1,
					&data_arb_usrp_cntrl1);
		if (rc < 0)
			return rc;
		usleep_range(PM8921_ADC_CONV_TIME_MIN,
					PM8921_ADC_CONV_TIME_MAX);
	}
	data_arb_usrp_cntrl1 = 0;

	rc = pm8921_adc_read_adc_code(&calib_read_1);
	if (rc) {
		pr_err("pm8921_adc read adc failed with %d\n", rc);
		pm8921_adc_calib_first_adc = false;
		goto calib_fail;
	}
	pm8921_adc_calib_first_adc = false;

	conv.amux_channel = CHANNEL_625MV;
	conv.decimation = ADC_DECIMATION_TYPE2;
	conv.amux_ip_rsv = AMUX_RSV1;
	conv.amux_mpp_channel = PREMUX_MPP_SCALE_0;
	pm8921_adc_calib_first_adc = true;
	rc = pm8921_adc_configure(&conv);
	if (rc) {
		pr_err("pm8921_adc configure failed with %d\n", rc);
		goto calib_fail;
	}

	while (data_arb_usrp_cntrl1 != (PM8921_ADC_ARB_USRP_CNTRL1_EOC |
					PM8921_ADC_ARB_USRP_CNTRL1_EN_ARB)) {
		rc = pm8921_adc_read_reg(PM8921_ADC_ARB_USRP_CNTRL1,
					&data_arb_usrp_cntrl1);
		if (rc < 0)
			return rc;
		usleep_range(PM8921_ADC_CONV_TIME_MIN,
					PM8921_ADC_CONV_TIME_MAX);
	}
	data_arb_usrp_cntrl1 = 0;

	rc = pm8921_adc_read_adc_code(&calib_read_2);
	if (rc) {
		pr_err("pm8921_adc read adc failed with %d\n", rc);
		pm8921_adc_calib_first_adc = false;
		goto calib_fail;
	}
	pm8921_adc_calib_first_adc = false;

	slope_adc = (((calib_read_1 - calib_read_2) << PM8921_ADC_MUL)/
					PM8921_CHANNEL_ADC_625_MV);
	offset_adc = calib_read_2 -
			((slope_adc * PM8921_CHANNEL_ADC_625_MV) >>
							PM8921_ADC_MUL);

	adc_pmic->conv->chan_prop->adc_graph[ADC_CALIB_ABSOLUTE].offset
								= offset_adc;
	adc_pmic->conv->chan_prop->adc_graph[ADC_CALIB_ABSOLUTE].dy =
					(calib_read_1 - calib_read_2);
	adc_pmic->conv->chan_prop->adc_graph[ADC_CALIB_ABSOLUTE].dx
						= PM8921_CHANNEL_ADC_625_MV;
	rc = pm8921_adc_arb_cntrl(0);
	if (rc < 0) {
		pr_err("%s: Configuring ADC Arbiter disable"
					"failed\n", __func__);
		return rc;
	}
	/* Ratiometric Calibration */
	conv.amux_channel = CHANNEL_MUXOFF;
	conv.decimation = ADC_DECIMATION_TYPE2;
	conv.amux_ip_rsv = AMUX_RSV5;
	conv.amux_mpp_channel = PREMUX_MPP_SCALE_0;
	pm8921_adc_calib_first_adc = true;
	rc = pm8921_adc_configure(&conv);
	if (rc) {
		pr_err("pm8921_adc configure failed with %d\n", rc);
		goto calib_fail;
	}

	while (data_arb_usrp_cntrl1 != (PM8921_ADC_ARB_USRP_CNTRL1_EOC |
					PM8921_ADC_ARB_USRP_CNTRL1_EN_ARB)) {
		rc = pm8921_adc_read_reg(PM8921_ADC_ARB_USRP_CNTRL1,
					&data_arb_usrp_cntrl1);
		if (rc < 0)
			return rc;
		usleep_range(PM8921_ADC_CONV_TIME_MIN,
					PM8921_ADC_CONV_TIME_MAX);
	}
	data_arb_usrp_cntrl1 = 0;

	rc = pm8921_adc_read_adc_code(&calib_read_1);
	if (rc) {
		pr_err("pm8921_adc read adc failed with %d\n", rc);
		pm8921_adc_calib_first_adc = false;
		goto calib_fail;
	}
	pm8921_adc_calib_first_adc = false;

	conv.amux_channel = CHANNEL_MUXOFF;
	conv.decimation = ADC_DECIMATION_TYPE2;
	conv.amux_ip_rsv = AMUX_RSV4;
	conv.amux_mpp_channel = PREMUX_MPP_SCALE_0;
	pm8921_adc_calib_first_adc = true;
	rc = pm8921_adc_configure(&conv);
	if (rc) {
		pr_err("pm8921_adc configure failed with %d\n", rc);
		goto calib_fail;
	}

	while (data_arb_usrp_cntrl1 != (PM8921_ADC_ARB_USRP_CNTRL1_EOC |
					PM8921_ADC_ARB_USRP_CNTRL1_EN_ARB)) {
		rc = pm8921_adc_read_reg(PM8921_ADC_ARB_USRP_CNTRL1,
					&data_arb_usrp_cntrl1);
		if (rc < 0)
			return rc;
		usleep_range(PM8921_ADC_CONV_TIME_MIN,
					PM8921_ADC_CONV_TIME_MAX);
	}
	data_arb_usrp_cntrl1 = 0;

	rc = pm8921_adc_read_adc_code(&calib_read_2);
	if (rc) {
		pr_err("pm8921_adc read adc failed with %d\n", rc);
		pm8921_adc_calib_first_adc = false;
		goto calib_fail;
	}
	pm8921_adc_calib_first_adc = false;

	slope_adc = (((calib_read_1 - calib_read_2) << PM8921_ADC_MUL)/
				adc_pmic->adc_prop->adc_vdd_reference);
	offset_adc = calib_read_2 -
			((slope_adc * adc_pmic->adc_prop->adc_vdd_reference)
							>> PM8921_ADC_MUL);

	adc_pmic->conv->chan_prop->adc_graph[ADC_CALIB_RATIOMETRIC].offset
								= offset_adc;
	adc_pmic->conv->chan_prop->adc_graph[ADC_CALIB_RATIOMETRIC].dy =
					(calib_read_1 - calib_read_2);
	adc_pmic->conv->chan_prop->adc_graph[ADC_CALIB_RATIOMETRIC].dx =
					adc_pmic->adc_prop->adc_vdd_reference;
calib_fail:
	rc = pm8921_adc_arb_cntrl(0);
	if (rc < 0) {
		pr_err("%s: Configuring ADC Arbiter disable"
					"failed\n", __func__);
	}

	return rc;
}

uint32_t pm8921_adc_read(enum pm8921_adc_channels channel,
				struct pm8921_adc_chan_result *result)
{
	return pm8921_adc_mpp_read(channel, result, PREMUX_MPP_SCALE_0);
}
EXPORT_SYMBOL_GPL(pm8921_adc_read);

uint32_t pm8921_adc_mpp_read(enum pm8921_adc_mpp_channels channel,
				struct pm8921_adc_chan_result *result,
				enum pm8921_adc_premux_mpp_scale_type mpp_scale)
{
	struct pm8921_adc *adc_pmic = pmic_adc;
	int i = 0, rc, amux_prescaling, scale_type;

	if (!pm8921_adc_initialized)
		return -ENODEV;

	if (!pm8921_adc_calib_device_init) {
		if (pm8921_adc_calib_device() == 0)
			pm8921_adc_calib_device_init = true;
	}

	mutex_lock(&adc_pmic->adc_lock);

	for (i = 0; i < adc_pmic->adc_num_channel; i++) {
		if (channel == adc_pmic->adc_channel[i].channel_name)
			break;
	}

	if (i == adc_pmic->adc_num_channel) {
		mutex_unlock(&adc_pmic->adc_lock);
		return -EBADF; /* unknown channel */
	}

	adc_pmic->conv->amux_channel = i;
	adc_pmic->conv->amux_mpp_channel = mpp_scale;

	adc_pmic->conv->amux_ip_rsv = adc_pmic->adc_channel[i].adc_rsv;
	adc_pmic->conv->decimation = adc_pmic->adc_channel[i].adc_decimation;
	amux_prescaling = adc_pmic->adc_channel[i].chan_path_prescaling;

	adc_pmic->conv->chan_prop->offset_gain_numerator =
		pm8921_amux_scaling_ratio[amux_prescaling].num;
	adc_pmic->conv->chan_prop->offset_gain_denominator =
		 pm8921_amux_scaling_ratio[amux_prescaling].den;

	rc = pm8921_adc_configure(adc_pmic->conv);
	if (rc) {
		mutex_unlock(&adc_pmic->adc_lock);
		return -EINVAL;
	}

	wait_for_completion(&adc_pmic->adc_rslt_completion);

	rc = pm8921_adc_read_adc_code(&result->adc_code);
	if (rc) {
		mutex_unlock(&adc_pmic->adc_lock);
		return -EINVAL;
	}

	scale_type = adc_pmic->adc_channel[i].adc_scale_fn;
	if (scale_type >= ADC_SCALE_NONE) {
		mutex_unlock(&adc_pmic->adc_lock);
		return -EBADF;
	}

	adc_scale_fn[scale_type].chan(result->adc_code,
			adc_pmic->adc_prop, adc_pmic->conv->chan_prop, result);

	mutex_unlock(&adc_pmic->adc_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(pm8921_adc_mpp_read);

uint32_t pm8921_adc_btm_configure(struct pm8921_adc_arb_btm_param *btm_param)
{
	struct pm8921_adc *adc_pmic = pmic_adc;
	u8 data_btm_cool_thr0, data_btm_cool_thr1;
	u8 data_btm_warm_thr0, data_btm_warm_thr1;
	u8 arb_btm_cntrl1;
	int rc;

	mutex_lock(&adc_pmic->btm_lock);

	data_btm_cool_thr0 = ((btm_param->low_thr_voltage << 24) >> 24);
	data_btm_cool_thr1 = ((btm_param->low_thr_voltage << 16) >> 24);
	data_btm_warm_thr0 = ((btm_param->high_thr_voltage << 24) >> 24);
	data_btm_warm_thr1 = ((btm_param->high_thr_voltage << 16) >> 24);

	rc = pm8921_adc_write_reg(PM8921_ADC_ARB_BTM_BAT_COOL_THR0,
						data_btm_cool_thr0);
	if (rc < 0)
		goto write_err;

	rc = pm8921_adc_write_reg(PM8921_ADC_ARB_BTM_BAT_COOL_THR1,
						data_btm_cool_thr0);
	if (rc < 0)
		goto write_err;

	rc = pm8921_adc_write_reg(PM8921_ADC_ARB_BTM_BAT_WARM_THR0,
						data_btm_warm_thr0);
	if (rc < 0)
		goto write_err;

	rc = pm8921_adc_write_reg(PM8921_ADC_ARB_BTM_BAT_WARM_THR1,
						data_btm_warm_thr1);
	if (rc < 0)
		goto write_err;

	arb_btm_cntrl1 = btm_param->interval << PM8921_ADC_BTM_INTERVAL_SEL;

	rc = pm8921_adc_write_reg(PM8921_ADC_ARB_BTM_CNTRL1, arb_btm_cntrl1);
	if (rc < 0)
		goto write_err;

	adc_pmic->batt->btm_param->btm_warm_fn = btm_param->btm_warm_fn;
	adc_pmic->batt->btm_param->btm_cold_fn = btm_param->btm_cold_fn;

	mutex_unlock(&adc_pmic->btm_lock);

	return rc;

write_err:
	mutex_unlock(&adc_pmic->btm_lock);
	return rc;
}
EXPORT_SYMBOL_GPL(pm8921_adc_btm_configure);

static uint32_t pm8921_adc_btm_read(uint32_t channel)
{
	struct pm8921_adc *adc_pmic = pmic_adc;
	int rc, i;
	u8 arb_btm_dig_param, arb_btm_ana_param, arb_btm_rsv;
	u8 arb_btm_amux_cntrl, arb_btm_decimation, data_arb_btm_cntrl;

	arb_btm_amux_cntrl = channel << PM8921_ADC_BTM_CHANNEL_SEL;
	arb_btm_rsv = adc_pmic->adc_channel[channel].adc_rsv;
	arb_btm_decimation =
		adc_pmic->adc_channel[channel].adc_decimation;
	arb_btm_ana_param = PM8921_ADC_ARB_ANA_DIG;

	mutex_lock(&adc_pmic->btm_lock);

	rc = pm8921_adc_write_reg(PM8921_ADC_ARB_BTM_AMUX_CNTRL,
						arb_btm_amux_cntrl);
	if (rc < 0)
		goto write_err;

	rc = pm8921_adc_write_reg(PM8921_ADC_ARB_BTM_RSV, arb_btm_rsv);
	if (rc < 0)
		goto write_err;

	arb_btm_dig_param = arb_btm_decimation <<
				PM8921_ADC_BTM_DECIMATION_SEL;
	rc = pm8921_adc_write_reg(PM8921_ADC_ARB_BTM_DIG_PARAM,
						arb_btm_dig_param);
	if (rc < 0)
		goto write_err;

	rc = pm8921_adc_write_reg(PM8921_ADC_ARB_BTM_ANA_PARAM,
						arb_btm_ana_param);
	if (rc < 0)
		goto write_err;

	data_arb_btm_cntrl = PM8921_ADC_ARB_BTM_CNTRL1_EOC |
				PM8921_ADC_ARB_BTM_CNTRL1_EN_BTM;

	/* Write twice to the CNTRL register for the arbiter settings
	   to take into effect */
	for (i = 0; i < 2; i++) {
		rc = pm8921_adc_write_reg(PM8921_ADC_ARB_BTM_CNTRL1,
						data_arb_btm_cntrl);
		if (rc < 0)
			goto write_err;
	}

	mutex_unlock(&adc_pmic->btm_lock);

	return 0;

write_err:
	mutex_unlock(&adc_pmic->btm_lock);
	return rc;
}

uint32_t pm8921_adc_btm_start(void)
{
	int rc;

	rc = pm8921_adc_btm_read(CHANNEL_BATT_THERM);
	return rc;
}
EXPORT_SYMBOL_GPL(pm8921_adc_btm_start);

uint32_t pm8921_adc_btm_end(void)
{
	struct pm8921_adc *adc_pmic = pmic_adc;
	int i, rc;
	u8 data_arb_btm_cntrl;

	/* Set BTM registers to Disable mode */
	data_arb_btm_cntrl = PM8921_ADC_ARB_BTM_CNTRL1_EOC;

	mutex_lock(&adc_pmic->btm_lock);
	/* Write twice to the CNTRL register for the arbiter settings
	   to take into effect */
	for (i = 0; i < 2; i++) {
		rc = pm8921_adc_write_reg(PM8921_ADC_ARB_BTM_CNTRL1,
							data_arb_btm_cntrl);
		if (rc < 0) {
			mutex_unlock(&adc_pmic->btm_lock);
			return rc;
		}
	}
	mutex_unlock(&adc_pmic->btm_lock);

	return rc;
}
EXPORT_SYMBOL_GPL(pm8921_adc_btm_end);

static int get_adc(void *data, u64 *val)
{
	struct pm8921_adc_chan_result result;
	int i = (int)data;
	int rc;

	rc = pm8921_adc_read(i, &result);

	pr_info("ADC value raw:%x physical:%lld\n",
			result.adc_code, result.physical);
	*val = result.physical;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_adc, NULL, "%llu\n");

#ifdef CONFIG_DEBUG_FS
static void create_debugfs_entries(void)
{
	pmic_adc->dent = debugfs_create_dir("pm8921_adc", NULL);

	if (IS_ERR(pmic_adc->dent)) {
		pr_err("pmic adc debugfs dir not created\n");
		return;
	}

	debugfs_create_file("vbat", 0644, pmic_adc->dent,
			    (void *)CHANNEL_VBAT, &reg_fops);
	debugfs_create_file("625mv", 0644, pmic_adc->dent,
			    (void *)CHANNEL_625MV, &reg_fops);
	debugfs_create_file("125v", 0644, pmic_adc->dent,
			    (void *)CHANNEL_125V, &reg_fops);
	debugfs_create_file("die_temp", 0644, pmic_adc->dent,
			    (void *)CHANNEL_DIE_TEMP, &reg_fops);
	debugfs_create_file("vcoin", 0644, pmic_adc->dent,
			    (void *)CHANNEL_VCOIN, &reg_fops);
	debugfs_create_file("dc_in", 0644, pmic_adc->dent,
			    (void *)CHANNEL_DCIN, &reg_fops);
	debugfs_create_file("vph_pwr", 0644, pmic_adc->dent,
			    (void *)CHANNEL_VPH_PWR, &reg_fops);
	debugfs_create_file("usb_in", 0644, pmic_adc->dent,
			    (void *)CHANNEL_USBIN, &reg_fops);
	debugfs_create_file("batt_therm", 0644, pmic_adc->dent,
			    (void *)CHANNEL_BATT_THERM, &reg_fops);
	debugfs_create_file("batt_id", 0644, pmic_adc->dent,
			    (void *)CHANNEL_BATT_ID, &reg_fops);
	debugfs_create_file("chg_temp", 0644, pmic_adc->dent,
			    (void *)CHANNEL_CHG_TEMP, &reg_fops);
	debugfs_create_file("charger_current", 0644, pmic_adc->dent,
			    (void *)CHANNEL_ICHG, &reg_fops);
	debugfs_create_file("ibat", 0644, pmic_adc->dent,
			    (void *)CHANNEL_IBAT, &reg_fops);
}
#else
static inline void create_debugfs_entries(void)
{
}
#endif

static int __devexit pm8921_adc_teardown(struct platform_device *pdev)
{
	struct pm8921_adc *adc_pmic = pmic_adc;

	device_init_wakeup(&pdev->dev, 0);
	free_irq(adc_pmic->adc_irq, adc_pmic);
	free_irq(adc_pmic->btm_warm_irq, adc_pmic);
	free_irq(adc_pmic->btm_cold_irq, adc_pmic);
	platform_set_drvdata(pdev, NULL);
	pmic_adc = NULL;
	kfree(adc_pmic->conv->chan_prop);
	kfree(adc_pmic->adc_channel);
	kfree(adc_pmic);
	pm8921_adc_initialized = false;

	return 0;
}

static int __devinit pm8921_adc_probe(struct platform_device *pdev)
{
	const struct pm8921_adc_platform_data *pdata = pdev->dev.platform_data;
	struct pm8921_adc *adc_pmic;
	struct pm8921_adc_amux_properties *adc_amux_prop;
	struct pm8921_adc_chan_properties *adc_pmic_chanprop;
	struct pm8921_adc_amux *adc_amux;
	int rc = 0;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data?\n");
		return -EINVAL;
	}

	adc_pmic = kzalloc(sizeof(struct pm8921_adc),
						GFP_KERNEL);
	if (!adc_pmic) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	adc_amux_prop = kzalloc(sizeof(struct pm8921_adc_amux_properties),
						GFP_KERNEL);
	if (!adc_amux_prop) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	adc_amux = kzalloc(sizeof(struct pm8921_adc_amux),
						GFP_KERNEL);
	if (!adc_amux) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	adc_pmic_chanprop = kzalloc(sizeof(struct pm8921_adc_chan_properties),
						GFP_KERNEL);
	if (!adc_pmic_chanprop) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	adc_pmic->dev = &pdev->dev;
	adc_pmic->adc_prop = pdata->adc_prop;
	adc_pmic->conv = adc_amux_prop;
	adc_pmic->conv->chan_prop = adc_pmic_chanprop;

	init_completion(&adc_pmic->adc_rslt_completion);
	adc_amux = pdata->adc_channel;
	adc_pmic->adc_channel = adc_amux;
	adc_pmic->adc_num_channel = pdata->adc_num_channel;

	mutex_init(&adc_pmic->adc_lock);
	mutex_init(&adc_pmic->btm_lock);

	adc_pmic->adc_irq = platform_get_irq(pdev, PM8921_ADC_IRQ_0);
	if (adc_pmic->adc_irq < 0) {
		rc = -ENXIO;
		goto err_cleanup;
	}

	rc = request_irq(adc_pmic->adc_irq,
				pm8921_adc_isr,
		IRQF_TRIGGER_RISING, "pm8921_adc_interrupt", adc_pmic);
	if (rc) {
		dev_err(&pdev->dev, "failed to request adc irq "
						"with error %d\n", rc);
		goto err_cleanup;
	}

	disable_irq_nosync(adc_pmic->adc_irq);

	adc_pmic->btm_warm_irq = platform_get_irq(pdev, PM8921_ADC_IRQ_1);
	if (adc_pmic->btm_warm_irq < 0) {
		rc = -ENXIO;
		goto err_cleanup;
	}

	rc = request_irq(adc_pmic->btm_warm_irq,
				pm8921_btm_warm_isr,
		IRQF_TRIGGER_RISING, "pm8921_btm_warm_interrupt", adc_pmic);
	if (rc) {
		pr_err("btm warm irq failed %d with interrupt number %d\n",
						rc, adc_pmic->btm_warm_irq);
		dev_err(&pdev->dev, "failed to request btm irq\n");
		goto err_cleanup;
	}

	disable_irq_nosync(adc_pmic->btm_warm_irq);

	adc_pmic->btm_cold_irq = platform_get_irq(pdev, PM8921_ADC_IRQ_2);
	if (adc_pmic->btm_cold_irq < 0) {
		rc = -ENXIO;
		goto err_cleanup;
	}

	rc = request_irq(adc_pmic->btm_cold_irq,
				pm8921_btm_cold_isr,
		IRQF_TRIGGER_RISING, "pm8921_btm_cold_interrupt", adc_pmic);
	if (rc) {
		pr_err("btm cold irq failed with return %d and number %d\n",
						rc, adc_pmic->btm_cold_irq);
		dev_err(&pdev->dev, "failed to request btm irq\n");
		goto err_cleanup;
	}

	disable_irq_nosync(adc_pmic->btm_cold_irq);
	device_init_wakeup(&pdev->dev, pdata->adc_wakeup);
	platform_set_drvdata(pdev, adc_pmic);
	pmic_adc = adc_pmic;

	create_debugfs_entries();
	pm8921_adc_calib_first_adc = false;
	pm8921_btm_calib_first_adc = false;
	pm8921_adc_calib_device_init = false;
	pm8921_adc_initialized = true;
	return 0;

err_cleanup:
	pm8921_adc_teardown(pdev);
	return rc;
}

static struct platform_driver pm8921_adc_driver = {
	.probe	= pm8921_adc_probe,
	.remove	= __devexit_p(pm8921_adc_teardown),
	.driver	= {
		.name	= PM8921_ADC_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8921_adc_init(void)
{
	return platform_driver_register(&pm8921_adc_driver);
}
module_init(pm8921_adc_init);

static void __exit pm8921_adc_exit(void)
{
	platform_driver_unregister(&pm8921_adc_driver);
}
module_exit(pm8921_adc_exit);

MODULE_ALIAS("platform:" PM8921_ADC_DEV_NAME);
MODULE_DESCRIPTION("PMIC8921 ADC driver");
MODULE_LICENSE("GPL v2");
