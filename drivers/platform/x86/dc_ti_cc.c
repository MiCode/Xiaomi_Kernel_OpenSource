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

#define CC_ACC_TO_UA(a)			(a * 367)

#define CC_ACC_TO_UAH(a)		(a / 3600)

#define ADC_TO_OCV(a)			(a * 4687)

#define ADC_TO_VBATT(a)			(a * 4687)

#define CC_SEC_TO_HR			3600

#define DRV_NAME		"dollar_cove_ti_cc"

#define THERM_CURVE_MAX_SAMPLES		13
#define THERM_CURVE_MAX_VALUES		4
#define RBATT_TYPICAL			150

struct dc_ti_cc_info {
	struct platform_device *pdev;
	struct work_struct	init_work;

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
			dc_ti_bptherm_curve_data[THERM_CURVE_MAX_SAMPLES-1][0],
			dc_ti_bptherm_curve_data[0][2]);

	for (i = 0; i < THERM_CURVE_MAX_SAMPLES; i++) {
		/* linear approximation for battery pack temperature */
		if (adc_val >= dc_ti_bptherm_curve_data[i][2]) {
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

	/* scale the counter to seconds */
	smpl_ctr /= 4;
	delta_smpl = smpl_ctr - info->smpl_ctr_prev;
	info->smpl_ctr_prev = smpl_ctr;
	/* handle sample counter overflow */
	if (delta_smpl < 0) {
		val = (int)((CC_SMPL_CTR_MAX_VAL/4) - info->smpl_ctr_prev);
		delta_smpl = val + smpl_ctr;
	}

	dev_info(&info->pdev->dev, "delta_smpl:%d\n", delta_smpl);

	/* ibatt_avg in uA */
	if (delta_smpl)
		info->ibatt_avg = (CC_ACC_TO_UA(delta_q)) / delta_smpl;

	/* convert CC to to uAhr */
	delta_q = CC_ACC_TO_UA(delta_q);
	*acc_val = CC_ACC_TO_UAH(delta_q);

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
	short int cc_intg_val;

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
	int ret;

	ret = intel_soc_pmic_setb(DC_TI_CC_CNTL_REG, CC_CNTL_CC_CTR_EN);
	if (ret < 0)
		dev_err(&info->pdev->dev,
			"Failed to set CC_CTR_EN bit:%d\n", ret);
	/*
	 * coulomb counter need 250ms
	 * to give the first IBAT sample.
	 */
	msleep(250);

	ret = dc_ti_cc_get_vocv(&info->vbat_bocv);
	if (ret)
		dev_err(&info->pdev->dev,
			"Failed to read IBAT bootup:%d\n", ret);

	ret = intel_soc_pmic_clearb(DC_TI_CC_CNTL_REG, CC_CNTL_CC_CTR_EN);
	if (ret < 0)
		dev_err(&info->pdev->dev,
			"Failed to clr CC_CTR_EN bit:%d\n", ret);
}

static void dc_ti_cc_init_worker(struct work_struct *work)
{
	struct dc_ti_cc_info *info =
	    container_of(work, struct dc_ti_cc_info, init_work);
	int ret;

	/* read bootup OCV */
	dc_ti_update_boot_ocv(info);
	dc_ti_cc_init_data(info);
	info_ptr = info;

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
	INIT_WORK(&info->init_work, dc_ti_cc_init_worker);

	/*
	 * scheduling the init worker
	 * to reduce the boot time.
	 */
	schedule_work(&info->init_work);

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
