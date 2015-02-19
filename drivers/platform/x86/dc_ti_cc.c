/*
 * dc_ti_cc.c - Intel Dollar Cove(TI) Coulomb Counter Driver
 *
 * Copyright (C) 2014 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/acpi.h>
#include <linux/iio/consumer.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/power/intel_fuel_gauge.h>

#define DC_TI_CC_CNTL_REG		0x60
#define CC_CNTL_CC_CTR_EN		(1 << 0)
#define CC_CNTL_CC_CLR_EN		(1 << 1)
#define CC_CNTL_CC_CAL_EN		(1 << 2)
#define CC_CNTL_CC_OFFSET_EN		(1 << 3)
#define CC_CNTL_SMPL_INTVL_MASK		(3 << 4)
#define CC_CNTL_SMPL_INTVL_15MS		(0 << 4)
#define CC_CNTL_SMPL_INTVL_62MS		(1 << 4)
#define CC_CNTL_SMPL_INTVL_125MS	(2 << 4)
#define CC_CNTL_SMPL_INTVL_250MS	(3 << 4)

#define DC_TI_SMPL_CTR0_REG		0x69
#define DC_TI_SMPL_CTR1_REG		0x68
#define DC_TI_SMPL_CTR2_REG		0x67

#define DC_TI_CC_OFFSET_HI_REG		0x61
#define CC_OFFSET_HI_MASK		0x3F
#define DC_TI_CC_OFFSET_LO_REG		0x62

#define DC_TI_SW_OFFSET_REG		0x6C

#define DC_TI_CC_ACC3_REG		0x63
#define DC_TI_CC_ACC2_REG		0x64
#define DC_TI_CC_ACC1_REG		0x65
#define DC_TI_CC_ACC0_REG		0x66

#define DC_TI_CC_INTG1_REG		0x6A
#define DC_TI_CC_INTG1_MASK		0x3F
#define DC_TI_CC_INTG0_REG		0x6B

#define DC_TI_ADC_VBAT_HI_REG		0x54
#define ADC_VBAT_HI_MASK		0x3
#define DC_TI_ADC_VBAT_LO_REG		0x55

#define CC_SMPL_CTR_MAX_VAL		0xFFFFFF

#define ADC_TO_BPTH_IN(a)		((a * 18) / 1023)

#define BPTH_IN_TO_RNTC(a)		((47 * a) / (18 - a))

#define CC_INTG_TO_UA(a)		(a * 183)

#define CC_INTG_TO_MA(a)		((a * 367) / 1000)

#define CC_ACC_TO_UA(a)			(a * 366)

#define CC_ACC_TO_UAH(a)		(a / 3600)

#define ADC_TO_OCV(a)			(a * 4687)

#define ADC_TO_VBATT(a)			(a * 4687)

#define CC_SEC_TO_HR			3600

#define DRV_NAME		"dollar_cove_ti_cc"

#define THERM_CURVE_MAX_SAMPLES		16
#define THERM_CURVE_MAX_VALUES		4
#define RBATT_TYPICAL			150

#define CC_GAIN_STEP			25
#define DEFAULT_CC_OFFSET_STEP	2
#define TRIM_REV_3_OFFSET_STEP	1

#define DEFAULT_CC_OFFSET_SHIFT	0
#define TRIM_REV_3_OFFSET_SHIFT	1

#define DC_TI_CC_OFF_HI		0x61
#define DC_TI_CC_OFF_LO		0x62

#define EEPROM_ACCESS_CONTROL		0x88
#define EEPROM_UNLOCK			0xDA
#define EEPROM_LOCK			0x00
#define EEPROM_CTRL			0xFE
#define EEPROM_CTRL_EEPSEL_MASK	0x03
#define EEPROM_BANK0_SEL		0x01
#define EEPROM_BANK1_SEL		0x02
#define OFFSET_REG_TRIM_REV_3		0xFD  /* b7~b0 : CC offset */
#define OFFSET_REG_TRIM_REV_DEFAULT	0xF3  /* b7~b4 : CC offset */
#define EEPROM_GAIN_REG		0xF4  /* b7~b4 : CC gain */
#define DC_PMIC_TRIM_REVISION_3	0x03
#define DEF_PMIC_TRIM_REVISON		0x00
#define DC_TI_PMIC_VERSION_REG		0x00
#define PMIC_VERSION_A0		0xC0
#define PMIC_VERSION_A1		0xC1
/* CC Accumulator Bit unit 3.662uV/10mohm */
#define MAX_CC_SCALE			3662

#define OCV_AVG_SAMPLE_CNT		3

struct dc_ti_trim {
	s8 cc_offset_extra;
	s8 cc_gain_extra;
	s8 cc_off_shift;
	s8 cc_step;
	s8 cc_trim_rev;
	s16 cc_offset_intcal;
	u8 pmic_version;
	bool apply_trim;
};
struct dc_ti_cc_info {
	struct platform_device *pdev;
	struct delayed_work	init_work;
	struct dc_ti_trim trim;

	int		vbat_socv;
	int		vbat_bocv;
	int		ibat_boot;
	int		ibatt_avg;
	unsigned int	smpl_ctr_prev;
	long		cc_val_prev;
};

static struct dc_ti_cc_info *info_ptr;

static int const dc_ti_bptherm_curve_data[THERM_CURVE_MAX_SAMPLES]
[THERM_CURVE_MAX_VALUES] = {
	/* {temp_max, temp_min, adc_max, adc_min} */
	{-5, -10, 834, 796},
	{0, -5, 796, 753},
	{5, 0, 753, 708},
	{10, 5, 708, 660},
	{15, 10, 660, 610},
	{20, 15, 610, 561},
	{25, 20, 561, 512},
	{30, 25, 512, 464},
	{35, 30, 464, 418},
	{40, 35, 418, 376},
	{45, 40, 376, 336},
	{50, 45, 336, 299},
	{55, 50, 299, 266},
	{60, 55, 266, 236},
	{65, 60, 236, 209},
	{70, 65, 209, 185},
};
/* Temperature Interpolation Macros */
static int platform_interpolate_temp(int adc_val,
	int adc_max, int adc_diff, int temp_diff)
{
	int ret;

	pr_debug("%s\n", __func__);
	ret = (adc_max - adc_val) * temp_diff;
	return ret / adc_diff;
}
/* platform_adc_to_temp - Convert ADC code to temperature
 * @adc_val : ADC sensor reading
 * @tmp : finally read temperature
 *
 * Returns 0 on success or -ERANGE in error case
 */
static int platform_adc_to_temp(uint16_t adc_val, int *tmp)
{
	int temp = 0;
	int i;

	pr_debug("%s\n", __func__);

/*
* If the value returned as an ERANGE the battery icon shows an
* exclaimation mark in the COS.In order to fix the issue, if
* the ADC returns a value which is not in range specified, we
* update the value within the bound.
*/
	adc_val = clamp_t(uint16_t, adc_val,
			dc_ti_bptherm_curve_data[THERM_CURVE_MAX_SAMPLES-1][3],
			dc_ti_bptherm_curve_data[0][2]);

	for (i = 0; i < THERM_CURVE_MAX_SAMPLES; i++) {
		/* linear approximation for battery pack temperature */
		if (adc_val >= dc_ti_bptherm_curve_data[i][3] &&
			adc_val <= dc_ti_bptherm_curve_data[i][2]) {
			temp = platform_interpolate_temp(adc_val,
					dc_ti_bptherm_curve_data[i][2],
					dc_ti_bptherm_curve_data[i][2] -
					dc_ti_bptherm_curve_data[i][3],
					dc_ti_bptherm_curve_data[i][0] -
					dc_ti_bptherm_curve_data[i][1]);

			temp += dc_ti_bptherm_curve_data[i][1];
			break;
		}
	}

	*tmp = temp;

	return 0;
}
/**
 * dc_ti_read_adc_val - read ADC value of specified sensors
 * @channel: channel of the sensor to be sampled
 * @sensor_val: pointer to the charger property to hold sampled value
 * @chc :  battery info pointer
 *
 * Returns 0 if success
 */
static int dc_ti_read_adc_val(const char *map, const char *name,
			int *raw_val, struct dc_ti_cc_info *info)
{
	int ret, val;
	struct iio_channel *indio_chan;

	indio_chan = iio_channel_get(NULL, name);
	if (IS_ERR_OR_NULL(indio_chan)) {
		ret = PTR_ERR(indio_chan);
		goto exit;
	}
	ret = iio_read_channel_raw(indio_chan, &val);
	if (ret) {
		dev_err(&info->pdev->dev, "IIO channel read error\n");
		goto err_exit;
	}

	dev_dbg(&info->pdev->dev, "adc raw val=%x\n", val);
	*raw_val = val;

err_exit:
	iio_channel_release(indio_chan);
exit:
	return ret;
}

static int dc_ti_fg_get_vbatt(struct dc_ti_cc_info *info, int *vbatt)
{
	int ret, raw_val;

	ret = dc_ti_read_adc_val("VIBAT", "VBAT", &raw_val, info);
	if (ret < 0)
		goto vbatt_read_fail;

	*vbatt = ADC_TO_VBATT(raw_val);
vbatt_read_fail:
	return ret;
}

static int dc_ti_adc_to_temp(struct dc_ti_cc_info *info,
				int adc_val, int *btemp)
{
	int val = 0, ret_val = 0;

	/*
	 * Look up for the temperature value from
	 * the Thermistor ADC conversion table.
	 */
	ret_val = platform_adc_to_temp(adc_val, &val);
	if (ret_val != 0) {
		dev_err(&info->pdev->dev,
			"Error while converting adc to temp :%d\n", ret_val);
		return ret_val;
	}
	*btemp = val * 10;

	return 0;
}

static int dc_ti_fg_get_btemp(struct dc_ti_cc_info *info, int *btemp)
{
	int ret, raw_val;

	ret = dc_ti_read_adc_val("THERMAL", "BATTEMP", &raw_val, info);
	if (ret < 0)
		goto btemp_read_fail;

	ret = dc_ti_adc_to_temp(info, raw_val, btemp);
	if (ret < 0)
		dev_warn(&info->pdev->dev, "ADC conversion error:%d\n", ret);
	else
		dev_dbg(&info->pdev->dev,
				"ADC code:%d, TEMP:%d\n", raw_val, *btemp);
btemp_read_fail:
	return ret;
}

static int dc_ti_get_cc_acc_val(struct dc_ti_cc_info *info, int *acc_val)
{
	int ret, val;
	long cc_val;

	/* Read coulomb counter accumulator */
	ret = intel_soc_pmic_readb(DC_TI_CC_ACC0_REG);
	if (ret < 0)
		goto cc_read_failed;
	else
		val = ret;

	ret = intel_soc_pmic_readb(DC_TI_CC_ACC1_REG);
	if (ret < 0)
		goto cc_read_failed;
	else
		val |= (ret << 8);

	ret = intel_soc_pmic_readb(DC_TI_CC_ACC2_REG);
	if (ret < 0)
		goto cc_read_failed;
	else
		val |= (ret << 16);

	ret = intel_soc_pmic_readb(DC_TI_CC_ACC3_REG);
	if (ret < 0)
		goto cc_read_failed;
	else
		val |= (ret << 24);

	/* convert the cc_val to uAs */
	cc_val = CC_ACC_TO_UA((long)val);
	/* convert uAS to uAH */
	*acc_val = CC_ACC_TO_UAH(cc_val);

	return 0;

cc_read_failed:
	dev_err(&info->pdev->dev, "cc acc read failed:%d\n", ret);
	return ret;
}

static int dc_ti_get_cc_delta(struct dc_ti_cc_info *info, int *acc_val)
{
	int ret, delta_q, delta_smpl, val;
	long cc_val;
	unsigned int smpl_ctr;

	/* Read coulomb counter accumulator */
	ret = intel_soc_pmic_readb(DC_TI_CC_ACC0_REG);
	if (ret < 0)
		goto cc_read_failed;
	val = ret;

	ret = intel_soc_pmic_readb(DC_TI_CC_ACC1_REG);
	if (ret < 0)
		goto cc_read_failed;
	val |= (ret << 8);

	ret = intel_soc_pmic_readb(DC_TI_CC_ACC2_REG);
	if (ret < 0)
		goto cc_read_failed;
	val |= (ret << 16);

	ret = intel_soc_pmic_readb(DC_TI_CC_ACC3_REG);
	if (ret < 0)
		goto cc_read_failed;
	val |= (ret << 24);

	/*convert the cc_val to uAs */
	cc_val = val;
	delta_q = cc_val - info->cc_val_prev;
	info->cc_val_prev = cc_val;
	dev_info(&info->pdev->dev, "delta_q raw:%d\n", delta_q);

	/* Read sample counter */
	ret = intel_soc_pmic_readb(DC_TI_SMPL_CTR0_REG);
	if (ret < 0)
		goto cc_read_failed;
	smpl_ctr = ret;

	ret = intel_soc_pmic_readb(DC_TI_SMPL_CTR1_REG);
	if (ret < 0)
		goto cc_read_failed;
	smpl_ctr |= (ret << 8);

	ret = intel_soc_pmic_readb(DC_TI_SMPL_CTR2_REG);
	if (ret < 0)
		goto cc_read_failed;
	smpl_ctr |= (ret << 16);

	delta_smpl = smpl_ctr - info->smpl_ctr_prev;
	info->smpl_ctr_prev = smpl_ctr;
	/* handle sample counter overflow */
	if (delta_smpl < 0)
		delta_smpl = val + (1 << 24);

	dev_info(&info->pdev->dev, "delta_smpl:%d\n", delta_smpl);

	/* Apply the Offset and Gain corrections to delta_q */
	if (info->trim.apply_trim) {
		dev_dbg(&info->pdev->dev, "Applying TRIM correction to CC\n");
		delta_q -= ((info->trim.cc_offset_extra * info->trim.cc_step *
				delta_smpl) >> info->trim.cc_off_shift);

		delta_q *= DIV_ROUND_CLOSEST(((10000 -
				(CC_GAIN_STEP - (info->trim.cc_gain_extra *
						 CC_GAIN_STEP)))
				* MAX_CC_SCALE), 100000);
	} else {
		/* convert CC to to uAhr without offset and gain correction */
		dev_dbg(&info->pdev->dev,
					"TRIM correction not Applied to CC\n");
		delta_q = CC_ACC_TO_UA(delta_q);
	}

	dev_info(&info->pdev->dev, "delta_q correction:%d\n", delta_q);
	/* ibatt_avg in uA */
	if (delta_smpl)
		info->ibatt_avg = DIV_ROUND_CLOSEST((delta_q * 4),
						delta_smpl);

	*acc_val = DIV_ROUND_CLOSEST(delta_q, 3600);

	return 0;

cc_read_failed:
	dev_err(&info->pdev->dev, "cc acc read failed:%d\n", ret);
	return ret;
}

static void dc_ti_cc_init_data(struct dc_ti_cc_info *info)
{
	int ret, val;
	unsigned int smpl_ctr;

	/* Read coulomb counter accumulator */
	ret = intel_soc_pmic_readb(DC_TI_CC_ACC0_REG);
	if (ret < 0)
		goto cc_read_failed;
	val = ret;

	ret = intel_soc_pmic_readb(DC_TI_CC_ACC1_REG);
	if (ret < 0)
		goto cc_read_failed;
	val |= (ret << 8);

	ret = intel_soc_pmic_readb(DC_TI_CC_ACC2_REG);
	if (ret < 0)
		goto cc_read_failed;
	val |= (ret << 16);

	ret = intel_soc_pmic_readb(DC_TI_CC_ACC3_REG);
	if (ret < 0)
		goto cc_read_failed;
	val |= (ret << 24);

	/*convert the cc_val to uAs */
	info->cc_val_prev = val;

	/* Read sample counter */
	ret = intel_soc_pmic_readb(DC_TI_SMPL_CTR0_REG);
	if (ret < 0)
		goto cc_read_failed;
	smpl_ctr = ret;

	ret = intel_soc_pmic_readb(DC_TI_SMPL_CTR1_REG);
	if (ret < 0)
		goto cc_read_failed;
	smpl_ctr |= (ret << 8);

	ret = intel_soc_pmic_readb(DC_TI_SMPL_CTR2_REG);
	if (ret < 0)
		goto cc_read_failed;
	smpl_ctr |= (ret << 16);

	/* scale the counter to seconds */
	smpl_ctr /= 4;
	info->smpl_ctr_prev = smpl_ctr;

cc_read_failed:
	if (ret < 0)
		dev_err(&info->pdev->dev, "pmic read failed\n");
	return;
}

static int dc_ti_get_ibatt_avg(struct dc_ti_cc_info *info, int *ibatt_avg)
{
	*ibatt_avg = info->ibatt_avg;
	return 0;
}

static int dc_ti_fg_get_ibatt(struct dc_ti_cc_info *info, int *ibatt)
{
	int ret, val;
	short int cc_intg_val, corrected_val;

	ret = intel_soc_pmic_readb(DC_TI_CC_INTG0_REG);
	if (ret < 0)
		goto ibatt_read_failed;
	val = ret;

	ret = intel_soc_pmic_readb(DC_TI_CC_INTG1_REG);
	if (ret < 0)
		goto ibatt_read_failed;

	val |= (ret & DC_TI_CC_INTG1_MASK) << 8;

	/* scale the readings to seconds */
	cc_intg_val = (short int)(val << 2);

	corrected_val = ((cc_intg_val)
			- ((2 * info->trim.cc_offset_intcal)
			+ (info->trim.cc_offset_extra)));

	corrected_val /= (1 + DIV_ROUND_CLOSEST((CC_GAIN_STEP
			- (info->trim.cc_gain_extra * CC_GAIN_STEP)),
			10000));

	/* convert the cc integrator value to uA */
	*ibatt = CC_INTG_TO_UA((int)cc_intg_val);

	return 0;

ibatt_read_failed:
	dev_err(&info->pdev->dev, "cc intg reg read failed:%d\n", ret);
	return ret;
}

static int dc_ti_fg_get_ibatt_bootup(struct dc_ti_cc_info *info,
					int *ibatt_boot)
{
	int ret;

	ret = intel_soc_pmic_setb(DC_TI_CC_CNTL_REG, CC_CNTL_CC_CTR_EN);
	if (ret < 0) {
		dev_err(&info->pdev->dev,
			"Failed to set CC_CTR_EN bit:%d\n", ret);
		return ret;
	}
	/* Coulomb Counter need 250ms to give the first IBAT sample */
	msleep(250);
	ret = dc_ti_fg_get_ibatt(info, ibatt_boot);
	if (ret)
		dev_err(&info->pdev->dev,
			"Failed to read IBAT bootup:%d\n", ret);

	ret |= intel_soc_pmic_clearb(DC_TI_CC_CNTL_REG, CC_CNTL_CC_CTR_EN);
	if (ret < 0) {
		dev_err(&info->pdev->dev,
			 "Failed to clr CC_CTR_EN bit:%d\n", ret);
		return ret;
	}

	return 0;
}
static void dc_ti_calibrate_cc(struct dc_ti_cc_info *info)
{
	int ret;

	/* disable Coulomb Counter */
	ret = intel_soc_pmic_clearb(DC_TI_CC_CNTL_REG, CC_CNTL_CC_CTR_EN);
	if (ret < 0)
		goto cc_cal_failed;

	/* Calibrate coulomb counter */
	ret = intel_soc_pmic_setb(DC_TI_CC_CNTL_REG, CC_CNTL_CC_CTR_EN |
				CC_CNTL_CC_CAL_EN | CC_CNTL_CC_OFFSET_EN);
	if (ret < 0)
		goto cc_cal_failed;

	mdelay(1);
	dc_ti_cc_init_data(info);

	return;

cc_cal_failed:
	dev_err(&info->pdev->dev, "CC Calibration failed:%d\n", ret);
}

static int dc_ti_read_ocv(struct dc_ti_cc_info *info, int *vbat_ocv)
{
	int ret, val;

	ret = intel_soc_pmic_readb(DC_TI_ADC_VBAT_LO_REG);
	if (ret < 0)
		goto ocv_read_failed;
	val = ret;

	ret = intel_soc_pmic_readb(DC_TI_ADC_VBAT_HI_REG);
	if (ret < 0)
		goto ocv_read_failed;
	val |= (ret & ADC_VBAT_HI_MASK) << 8;

	/* convert adc code to uV */
	*vbat_ocv = ADC_TO_OCV(val);
	return 0;

ocv_read_failed:
	dev_err(&info->pdev->dev, "ocv read failed:%d\n", ret);
	return ret;
}

static int dc_ti_cc_get_batt_params(int *vbat, int *ibat, int *btemp)
{
	int ret;

	if (!info_ptr)
		return -EAGAIN;

	ret = dc_ti_fg_get_vbatt(info_ptr, vbat);
	if (ret < 0)
		goto get_params_fail;

	ret = dc_ti_fg_get_ibatt(info_ptr, ibat);
	if (ret < 0)
		goto get_params_fail;

	ret = dc_ti_fg_get_btemp(info_ptr, btemp);

get_params_fail:
	return ret;
}

static int dc_ti_cc_get_vocv(int *vocv)
{
	int ret, ibatt;

	ret = dc_ti_fg_get_ibatt(info_ptr, &ibatt);
	if (ret < 0)
		goto get_vocv_fail;
	ret = dc_ti_fg_get_vbatt(info_ptr, vocv);
	if (ret < 0)
		goto get_vocv_fail;
	/*
	 * TODO: Ibatt adjustments.
	 */
	*vocv = (*vocv - ((ibatt * RBATT_TYPICAL)/1000));

get_vocv_fail:
	return ret;
}

static int dc_ti_cc_get_vocv_bootup(int *vocv_bootup)
{
	*vocv_bootup = info_ptr->vbat_bocv;
	return 0;
}

static int dc_ti_cc_get_ibat_bootup(int *ibat_bootup)
{
	*ibat_bootup = info_ptr->ibat_boot;
	return 0;
}

static int dc_ti_cc_get_vavg(int *vavg)
{
	int ret;

	ret = dc_ti_fg_get_vbatt(info_ptr, vavg);
	return ret;
}

static int dc_ti_cc_get_iavg(int *iavg)
{
	int ret;

	ret = dc_ti_get_ibatt_avg(info_ptr, iavg);
	return ret;
}

static int dc_ti_cc_get_deltaq(int *deltaq)
{
	int ret;

	ret = dc_ti_get_cc_delta(info_ptr, deltaq);
	return ret;
}

static int dc_ti_cc_calibrate(void)
{
	dc_ti_calibrate_cc(info_ptr);
	return 0;
}

static struct intel_fg_input fg_input = {
	.get_batt_params = &dc_ti_cc_get_batt_params,
	.get_v_ocv = &dc_ti_cc_get_vocv,
	.get_v_ocv_bootup = &dc_ti_cc_get_vocv_bootup,
	.get_i_bat_bootup = &dc_ti_cc_get_ibat_bootup,
	.get_v_avg = &dc_ti_cc_get_vavg,
	.get_i_avg = &dc_ti_cc_get_iavg,
	.get_delta_q = &dc_ti_cc_get_deltaq,
	.calibrate_cc = &dc_ti_cc_calibrate,
};

static void dc_ti_update_boot_ocv(struct dc_ti_cc_info *info)
{
	int ret, vocv, idx;

	ret = intel_soc_pmic_setb(DC_TI_CC_CNTL_REG, CC_CNTL_CC_CTR_EN);
	if (ret < 0)
		dev_err(&info->pdev->dev,
			"Failed to set CC_CTR_EN bit:%d\n", ret);
	/*
	 * coulomb counter need 250ms
	 * to give the first IBAT sample.
	 */
	msleep(250);

	/*
	 * take the average of 3 OCV samples
	 * for better accuracy.
	 */
	info->vbat_bocv = 0;
	for (idx = 0; idx < OCV_AVG_SAMPLE_CNT; idx++) {
		ret = dc_ti_cc_get_vocv(&vocv);
		if (ret)
			dev_err(&info->pdev->dev,
				"Failed to read bootup vocv:%d\n", ret);
		info->vbat_bocv += vocv;
	}
	info->vbat_bocv /= OCV_AVG_SAMPLE_CNT;

	ret = intel_soc_pmic_clearb(DC_TI_CC_CNTL_REG,
			CC_CNTL_CC_CTR_EN);
	if (ret < 0)
		dev_err(&info->pdev->dev,
			"Failed to clr CC_CTR_EN bit:%d\n", ret);
}

/**
 * dc_ti_cc_read_trim_values: Function to store the offset and gain correction.
 * @info: Pointer to the dc_ti_cc_info instance.
 * Returns 0 for success, Negetive value for failure.
 */
static int dc_ti_cc_read_trim_values(struct dc_ti_cc_info *info)
{
	int ret;
	u8	val_offset, val_gain, val_int_cal_hi, val_int_cal_lo;

	/* Read the PMIC Version register*/
	info->trim.pmic_version = intel_soc_pmic_readb(DC_TI_PMIC_VERSION_REG);
	if (info->trim.pmic_version < 0) {
		dev_err(&info->pdev->dev, "Error while reading PMIC Version Reg\n");
		ret = info->trim.pmic_version;
		goto exit_trim;
	}
	/*
	 * As per the PMIC Vendor, the calibration offset and gain err
	 * values are stored in EEPROM Bank 0 and Bank 1 of the PMIC.
	 * We need to read the stored offset and gain margins and need
	 * to apply the corrections to the raw coulomb counter value.
	 */
	/* UNLOCK the EEPROM Access */
	ret = intel_soc_pmic_writeb(EEPROM_ACCESS_CONTROL,
				EEPROM_UNLOCK);
	if (ret < 0) {
		dev_err(&info->pdev->dev,
				"Err while unlocking EEPROM\n");
		goto exit_trim;
	}
	/* Select Bank 1 to read CC GAIN Err correction */
	ret = intel_soc_pmic_writeb(EEPROM_CTRL,
			((u8)(EEPROM_CTRL_EEPSEL_MASK & EEPROM_BANK1_SEL)));
	if (ret < 0) {
		dev_err(&info->pdev->dev,
				"Err while selecting EEPROM Bank1\n");
		goto exit_trim;
	}
	val_gain = intel_soc_pmic_readb(EEPROM_GAIN_REG);
	if (val_gain < 0) {
		dev_err(&info->pdev->dev,
				"Err while reading Gain Reg\n");
		ret = val_gain;
		goto exit_trim;
	}
	info->trim.cc_gain_extra = val_gain >> 4;
	info->trim.cc_trim_rev = (val_gain & 0x0F);

	val_int_cal_hi = intel_soc_pmic_readb(DC_TI_CC_OFF_HI);
	if (val_int_cal_hi < 0) {
		dev_err(&info->pdev->dev,
				"Err while reading Offset Reg\n");
		ret = val_int_cal_hi;
		goto exit_trim;
	}
	val_int_cal_lo = intel_soc_pmic_readb(DC_TI_CC_OFF_LO);
	if (val_int_cal_lo < 0) {
		dev_err(&info->pdev->dev,
				"Err while reading Offset Reg\n");
		ret = val_int_cal_lo;
		goto exit_trim;
	}
	info->trim.cc_offset_intcal =
		(s16)(((val_int_cal_hi << 8) & 0x1F)
			| (val_int_cal_lo));
	if ((s8)val_int_cal_hi & 0x3F)
		info->trim.cc_offset_intcal *= -1;

	if ((info->trim.cc_trim_rev != DEF_PMIC_TRIM_REVISON &&
		info->trim.pmic_version == PMIC_VERSION_A1) ||
		(info->trim.cc_trim_rev == DC_PMIC_TRIM_REVISION_3 &&
		info->trim.pmic_version == PMIC_VERSION_A0)) {
		/* Select Bank 0 to read CC OFFSET Correction */
		ret = intel_soc_pmic_writeb(EEPROM_CTRL,
			((u8)(EEPROM_CTRL_EEPSEL_MASK & EEPROM_BANK0_SEL)));
		if (ret < 0) {
			dev_err(&info->pdev->dev,
				"Error while selecting EEPROM Bank1\n");
			goto exit_trim;
		}
		val_offset = intel_soc_pmic_readb(OFFSET_REG_TRIM_REV_3);
		if (val_offset < 0) {
			dev_err(&info->pdev->dev,
				"Error while reading Offset Reg\n");
			ret = val_offset;
			goto exit_trim;
		}
		info->trim.cc_offset_extra = (s8)(val_offset);
		info->trim.cc_step = TRIM_REV_3_OFFSET_STEP;
		info->trim.cc_off_shift = TRIM_REV_3_OFFSET_SHIFT;
		info->trim.apply_trim = true;
		dev_warn(&info->pdev->dev,
		"TRIM Revision %d PMIC Version %d, Apply TRIM\n",
		info->trim.cc_trim_rev, info->trim.pmic_version);
	} else {
		/* Read offset trim value from Bank 0 */
		val_offset = intel_soc_pmic_readb(
					OFFSET_REG_TRIM_REV_DEFAULT);
		if (val_offset < 0) {
			dev_err(&info->pdev->dev,
				"Error while reading Offset Reg\n");
			ret = val_offset;
			goto exit_trim;
		}
		info->trim.cc_offset_extra = ((s8)val_offset) >> 4;
		info->trim.cc_step = DEFAULT_CC_OFFSET_STEP;
		info->trim.cc_off_shift = DEFAULT_CC_OFFSET_SHIFT;

		dev_warn(&info->pdev->dev,
		"TRIM Revision %d PMIC Version %d, Do Not Apply TRIM\n",
		info->trim.cc_trim_rev, info->trim.pmic_version);

		info->trim.cc_trim_rev = DEF_PMIC_TRIM_REVISON;
		info->trim.apply_trim = false;
	}

exit_trim:
	/* Lock the EEPROM Access */
	intel_soc_pmic_writeb(EEPROM_ACCESS_CONTROL, EEPROM_LOCK);
	if (ret < 0) {
		/* Reset the PMIC TRIM Revision number when error
		 * is encountered */
		info->trim.cc_trim_rev = DEF_PMIC_TRIM_REVISON;
		info->trim.apply_trim = false;
		dev_err(&info->pdev->dev, "Err Reset the TRIM params\n");
	}
	dev_warn(&info->pdev->dev, "CC OFFSET = %d GAIN = %d\n",
			info->trim.cc_offset_extra, info->trim.cc_gain_extra);
	return ret;
}
static void dc_ti_cc_init_worker(struct work_struct *work)
{
	struct dc_ti_cc_info *info =
	    container_of(to_delayed_work(work), struct dc_ti_cc_info,
				init_work.work);
	int ret;

	dc_ti_cc_read_trim_values(info);
	/* read bootup OCV */
	dc_ti_update_boot_ocv(info);
	dc_ti_cc_init_data(info);

	ret = intel_fg_register_input(&fg_input);
	if (ret < 0)
		dev_err(&info->pdev->dev, "intel FG registration failed\n");
}

static int dc_ti_cc_probe(struct platform_device *pdev)
{
	struct dc_ti_cc_info *info;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "mem alloc failed\n");
		return -ENOMEM;
	}

	info->pdev = pdev;
	platform_set_drvdata(pdev, info);
	INIT_DELAYED_WORK(&info->init_work, dc_ti_cc_init_worker);

	info_ptr = info;
	/*
	 * scheduling the init worker to reduce
	 * delays during boot time. Also delayed
	 * worker is being used to time the
	 * OCV measurment later if neccessary.
	 */
	schedule_delayed_work(&info->init_work, 0);

	return 0;
}

static int dc_ti_cc_remove(struct platform_device *pdev)
{
	intel_fg_unregister_input(&fg_input);
	return 0;
}

#ifdef CONFIG_PM
static int dc_ti_cc_suspend(struct device *dev)
{
	return 0;
}

static int dc_ti_cc_resume(struct device *dev)
{
	return 0;
}
#else
#define dc_ti_cc_suspend		NULL
#define dc_ti_cc_resume		NULL
#endif

static const struct dev_pm_ops dc_ti_cc_driver_pm_ops = {
	.suspend	= dc_ti_cc_suspend,
	.resume		= dc_ti_cc_resume,
};

static struct platform_driver dc_ti_cc_driver = {
	.probe = dc_ti_cc_probe,
	.remove = dc_ti_cc_remove,
	.driver = {
		.name = DRV_NAME,
		.pm = &dc_ti_cc_driver_pm_ops,
	},
};

static int __init dc_ti_cc_init(void)
{
	return platform_driver_register(&dc_ti_cc_driver);
}
late_initcall(dc_ti_cc_init);

static void __exit dc_ti_cc_exit(void)
{
	platform_driver_unregister(&dc_ti_cc_driver);
}
module_exit(dc_ti_cc_exit);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_DESCRIPTION("DollarCove(TI) Power Source Detect Driver");
MODULE_LICENSE("GPL");
