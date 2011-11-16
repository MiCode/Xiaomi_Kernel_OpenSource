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
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/mfd/pm8xxx/pm8921-bms.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/pm8921-adc.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define BMS_CONTROL		0x224
#define BMS_OUTPUT0		0x230
#define BMS_OUTPUT1		0x231
#define BMS_TEST1		0x237
#define CCADC_ANA_PARAM		0x240
#define CCADC_DIG_PARAM		0x241
#define CCADC_RSV		0x242
#define CCADC_DATA0		0x244
#define CCADC_DATA1		0x245
#define CCADC_OFFSET_TRIM1	0x34A
#define CCADC_OFFSET_TRIM0	0x34B
#define CCADC_FULLSCALE_TRIM1	0x34C
#define CCADC_FULLSCALE_TRIM0	0x34D

#define ADC_ARB_SECP_CNTRL	0x190
#define ADC_ARB_SECP_AMUX_CNTRL	0x191
#define ADC_ARB_SECP_ANA_PARAM	0x192
#define ADC_ARB_SECP_DIG_PARAM	0x193
#define ADC_ARB_SECP_RSV	0x194
#define ADC_ARB_SECP_DATA1	0x195
#define ADC_ARB_SECP_DATA0	0x196

#define ADC_ARB_BMS_CNTRL	0x18D

enum pmic_bms_interrupts {
	PM8921_BMS_SBI_WRITE_OK,
	PM8921_BMS_CC_THR,
	PM8921_BMS_VSENSE_THR,
	PM8921_BMS_VSENSE_FOR_R,
	PM8921_BMS_OCV_FOR_R,
	PM8921_BMS_GOOD_OCV,
	PM8921_BMS_VSENSE_AVG,
	PM8921_BMS_CCADC_EOC,
	PM_BMS_MAX_INTS,
};

struct pm8921_bms_chip {
	struct device		*dev;
	struct dentry		*dent;
	unsigned int		r_sense;
	unsigned int		i_test;
	unsigned int		v_failure;
	unsigned int		fcc;
	struct single_row_lut	*fcc_temp_lut;
	struct single_row_lut	*fcc_sf_lut;
	struct pc_temp_ocv_lut	*pc_temp_ocv_lut;
	struct pc_sf_lut	*pc_sf_lut;
	struct work_struct	calib_hkadc_work;
	struct delayed_work	calib_ccadc_work;
	unsigned int		calib_delay_ms;
	int			ccadc_gain_uv;
	u16			ccadc_result_offset;
	unsigned int		revision;
	unsigned int		xoadc_v0625;
	unsigned int		xoadc_v125;
	unsigned int		batt_temp_channel;
	unsigned int		vbat_channel;
	unsigned int		ref625mv_channel;
	unsigned int		ref1p25v_channel;
	unsigned int		batt_id_channel;
	unsigned int		pmic_bms_irq[PM_BMS_MAX_INTS];
	DECLARE_BITMAP(enabled_irqs, PM_BMS_MAX_INTS);
	spinlock_t		bms_output_lock;
	struct single_row_lut	*adjusted_fcc_temp_lut;
	unsigned int		charging_began;
	unsigned int		start_percent;
	unsigned int		end_percent;

	uint16_t		ocv_reading_at_100;
	int			cc_reading_at_100;
};

static struct pm8921_bms_chip *the_chip;

#define DEFAULT_RBATT_MOHMS		128
#define DEFAULT_OCV_MICROVOLTS		3900000
#define DEFAULT_CHARGE_CYCLES		0

static int last_chargecycles = DEFAULT_CHARGE_CYCLES;
static int last_charge_increase;
module_param(last_chargecycles, int, 0644);
module_param(last_charge_increase, int, 0644);

static int last_rbatt = -EINVAL;
static int last_ocv_uv = -EINVAL;
static int last_soc = -EINVAL;
static int last_real_fcc = -EINVAL;
static int last_real_fcc_batt_temp = -EINVAL;

static int bms_ops_set(const char *val, const struct kernel_param *kp)
{
	if (*(int *)kp->arg == -EINVAL)
		return param_set_int(val, kp);
	else
		return 0;
}

static struct kernel_param_ops bms_param_ops = {
	.set = bms_ops_set,
	.get = param_get_int,
};

module_param_cb(last_rbatt, &bms_param_ops, &last_rbatt, 0644);
module_param_cb(last_ocv_uv, &bms_param_ops, &last_ocv_uv, 0644);
module_param_cb(last_soc, &bms_param_ops, &last_soc, 0644);

static int interpolate_fcc(struct pm8921_bms_chip *chip, int batt_temp);
static void readjust_fcc_table(void)
{
	struct single_row_lut *temp, *old;
	int i, fcc, ratio;

	if (!the_chip->fcc_temp_lut) {
		pr_err("The static fcc lut table is NULL\n");
		return;
	}

	temp = kzalloc(sizeof(struct single_row_lut), GFP_KERNEL);
	if (!temp) {
		pr_err("Cannot allocate memory for adjusted fcc table\n");
		return;
	}

	fcc = interpolate_fcc(the_chip, last_real_fcc_batt_temp);

	temp->cols = the_chip->fcc_temp_lut->cols;
	for (i = 0; i < the_chip->fcc_temp_lut->cols; i++) {
		temp->x[i] = the_chip->fcc_temp_lut->x[i];
		ratio = div_u64(the_chip->fcc_temp_lut->y[i] * 1000, fcc);
		temp->y[i] =  (ratio * last_real_fcc);
		temp->y[i] /= 1000;
		pr_debug("temp=%d, staticfcc=%d, adjfcc=%d, ratio=%d\n",
				temp->x[i], the_chip->fcc_temp_lut->y[i],
				temp->y[i], ratio);
	}

	old = the_chip->adjusted_fcc_temp_lut;
	the_chip->adjusted_fcc_temp_lut = temp;
	kfree(old);
}

static int bms_last_real_fcc_set(const char *val,
				const struct kernel_param *kp)
{
	int rc = 0;

	if (last_real_fcc == -EINVAL)
		rc = param_set_int(val, kp);
	if (rc) {
		pr_err("Failed to set last_real_fcc rc=%d\n", rc);
		return rc;
	}
	if (last_real_fcc_batt_temp != -EINVAL)
		readjust_fcc_table();
	return rc;
}
static struct kernel_param_ops bms_last_real_fcc_param_ops = {
	.set = bms_last_real_fcc_set,
	.get = param_get_int,
};
module_param_cb(last_real_fcc, &bms_last_real_fcc_param_ops,
					&last_real_fcc, 0644);

static int bms_last_real_fcc_batt_temp_set(const char *val,
				const struct kernel_param *kp)
{
	int rc = 0;

	if (last_real_fcc_batt_temp == -EINVAL)
		rc = param_set_int(val, kp);
	if (rc) {
		pr_err("Failed to set last_real_fcc_batt_temp rc=%d\n", rc);
		return rc;
	}
	if (last_real_fcc != -EINVAL)
		readjust_fcc_table();
	return rc;
}

static struct kernel_param_ops bms_last_real_fcc_batt_temp_param_ops = {
	.set = bms_last_real_fcc_batt_temp_set,
	.get = param_get_int,
};
module_param_cb(last_real_fcc_batt_temp, &bms_last_real_fcc_batt_temp_param_ops,
					&last_real_fcc_batt_temp, 0644);

static int pm_bms_get_rt_status(struct pm8921_bms_chip *chip, int irq_id)
{
	return pm8xxx_read_irq_stat(chip->dev->parent,
					chip->pmic_bms_irq[irq_id]);
}

static void pm8921_bms_enable_irq(struct pm8921_bms_chip *chip, int interrupt)
{
	if (!__test_and_set_bit(interrupt, chip->enabled_irqs)) {
		dev_dbg(chip->dev, "%s %d\n", __func__,
						chip->pmic_bms_irq[interrupt]);
		enable_irq(chip->pmic_bms_irq[interrupt]);
	}
}

static void pm8921_bms_disable_irq(struct pm8921_bms_chip *chip, int interrupt)
{
	if (__test_and_clear_bit(interrupt, chip->enabled_irqs)) {
		pr_debug("%d\n", chip->pmic_bms_irq[interrupt]);
		disable_irq_nosync(chip->pmic_bms_irq[interrupt]);
	}
}

static int pm_bms_masked_write(struct pm8921_bms_chip *chip, u16 addr,
							u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = pm8xxx_readb(chip->dev->parent, addr, &reg);
	if (rc) {
		pr_err("read failed addr = %03X, rc = %d\n", addr, rc);
		return rc;
	}
	reg &= ~mask;
	reg |= val & mask;
	rc = pm8xxx_writeb(chip->dev->parent, addr, reg);
	if (rc) {
		pr_err("write failed addr = %03X, rc = %d\n", addr, rc);
		return rc;
	}
	return 0;
}

#define HOLD_OREG_DATA		BIT(1)
static int pm_bms_lock_output_data(struct pm8921_bms_chip *chip)
{
	int rc;

	rc = pm_bms_masked_write(chip, BMS_CONTROL, HOLD_OREG_DATA,
					HOLD_OREG_DATA);
	if (rc) {
		pr_err("couldnt lock bms output rc = %d\n", rc);
		return rc;
	}
	return 0;
}

static int pm_bms_unlock_output_data(struct pm8921_bms_chip *chip)
{
	int rc;

	rc = pm_bms_masked_write(chip, BMS_CONTROL, HOLD_OREG_DATA, 0);
	if (rc) {
		pr_err("fail to unlock BMS_CONTROL rc = %d\n", rc);
		return rc;
	}
	return 0;
}

#define SELECT_OUTPUT_DATA	0x1C
#define SELECT_OUTPUT_TYPE_SHIFT	2
#define OCV_FOR_RBATT		0x0
#define VSENSE_FOR_RBATT	0x1
#define VBATT_FOR_RBATT		0x2
#define CC_MSB			0x3
#define CC_LSB			0x4
#define LAST_GOOD_OCV_VALUE	0x5
#define VSENSE_AVG		0x6
#define VBATT_AVG		0x7

static int pm_bms_read_output_data(struct pm8921_bms_chip *chip, int type,
						int16_t *result)
{
	int rc;
	u8 reg;

	if (!result) {
		pr_err("result pointer null\n");
		return -EINVAL;
	}
	*result = 0;
	if (type < OCV_FOR_RBATT || type > VBATT_AVG) {
		pr_err("invalid type %d asked to read\n", type);
		return -EINVAL;
	}

	/* make sure the bms registers are locked */
	rc = pm8xxx_readb(chip->dev->parent, BMS_CONTROL, &reg);
	if (rc) {
		pr_err("fail to read BMS_OUTPUT0 for type %d rc = %d\n",
			type, rc);
		return rc;
	}

	/* Output register data must be held (locked) while reading output */
	WARN_ON(!(reg && HOLD_OREG_DATA));

	rc = pm_bms_masked_write(chip, BMS_CONTROL, SELECT_OUTPUT_DATA,
					type << SELECT_OUTPUT_TYPE_SHIFT);
	if (rc) {
		pr_err("fail to select %d type in BMS_CONTROL rc = %d\n",
						type, rc);
		return rc;
	}

	rc = pm8xxx_readb(chip->dev->parent, BMS_OUTPUT0, &reg);
	if (rc) {
		pr_err("fail to read BMS_OUTPUT0 for type %d rc = %d\n",
			type, rc);
		return rc;
	}
	*result = reg;
	rc = pm8xxx_readb(chip->dev->parent, BMS_OUTPUT1, &reg);
	if (rc) {
		pr_err("fail to read BMS_OUTPUT1 for type %d rc = %d\n",
			type, rc);
		return rc;
	}
	*result |= reg << 8;
	pr_debug("type %d result %x", type, *result);
	return 0;
}

#define V_PER_BIT_MUL_FACTOR	97656
#define V_PER_BIT_DIV_FACTOR	1000
#define XOADC_INTRINSIC_OFFSET	0x6000
static int xoadc_reading_to_microvolt(unsigned int a)
{
	if (a <= XOADC_INTRINSIC_OFFSET)
		return 0;

	return (a - XOADC_INTRINSIC_OFFSET)
			* V_PER_BIT_MUL_FACTOR / V_PER_BIT_DIV_FACTOR;
}

#define XOADC_CALIB_UV		625000
#define VBATT_MUL_FACTOR	3
static int adjust_xo_vbatt_reading(struct pm8921_bms_chip *chip,
							unsigned int uv)
{
	u64 numerator, denominator;

	if (uv == 0)
		return 0;

	numerator = ((u64)uv - chip->xoadc_v0625) * XOADC_CALIB_UV;
	denominator =  chip->xoadc_v125 - chip->xoadc_v0625;
	if (denominator == 0)
		return uv * VBATT_MUL_FACTOR;
	return (XOADC_CALIB_UV + div_u64(numerator, denominator))
						* VBATT_MUL_FACTOR;
}

#define CC_RESOLUTION_N_V1	1085069
#define CC_RESOLUTION_D_V1	100000
#define CC_RESOLUTION_N_V2	868056
#define CC_RESOLUTION_D_V2	10000
static s64 cc_to_microvolt_v1(s64 cc)
{
	return div_s64(cc * CC_RESOLUTION_N_V1, CC_RESOLUTION_D_V1);
}

static s64 cc_to_microvolt_v2(s64 cc)
{
	return div_s64(cc * CC_RESOLUTION_N_V2, CC_RESOLUTION_D_V2);
}

static s64 cc_to_microvolt(struct pm8921_bms_chip *chip, s64 cc)
{
	/*
	 * resolution (the value of a single bit) was changed after revision 2.0
	 * for more accurate readings
	 */
	return (chip->revision < PM8XXX_REVISION_8921_2p0) ?
				cc_to_microvolt_v1((s64)cc) :
				cc_to_microvolt_v2((s64)cc);
}

#define CCADC_READING_RESOLUTION_N_V1	1085069
#define CCADC_READING_RESOLUTION_D_V1	100000
#define CCADC_READING_RESOLUTION_N_V2	542535
#define CCADC_READING_RESOLUTION_D_V2	100000
static s64 ccadc_reading_to_microvolt_v1(s64 cc)
{
	return div_s64(cc * CCADC_READING_RESOLUTION_N_V1,
					CCADC_READING_RESOLUTION_D_V1);
}

static s64 ccadc_reading_to_microvolt_v2(s64 cc)
{
	return div_s64(cc * CCADC_READING_RESOLUTION_N_V2,
					CCADC_READING_RESOLUTION_D_V2);
}

static s64 ccadc_reading_to_microvolt(struct pm8921_bms_chip *chip, s64 cc)
{
	/*
	 * resolution (the value of a single bit) was changed after revision 2.0
	 * for more accurate readings
	 */
	return (chip->revision < PM8XXX_REVISION_8921_2p0) ?
				ccadc_reading_to_microvolt_v1((s64)cc) :
				ccadc_reading_to_microvolt_v2((s64)cc);
}

static s64 microvolt_to_ccadc_reading_v1(s64 uv)
{
	return div_s64(uv * CCADC_READING_RESOLUTION_D_V1,
				CCADC_READING_RESOLUTION_N_V1);
}

static s64 microvolt_to_ccadc_reading_v2(s64 uv)
{
	return div_s64(uv * CCADC_READING_RESOLUTION_D_V2,
				CCADC_READING_RESOLUTION_N_V2);
}

static s64 microvolt_to_ccadc_reading(struct pm8921_bms_chip *chip, s64 cc)
{
	/*
	 * resolution (the value of a single bit) was changed after revision 2.0
	 * for more accurate readings
	 */
	return (chip->revision < PM8XXX_REVISION_8921_2p0) ?
				microvolt_to_ccadc_reading_v1((s64)cc) :
				microvolt_to_ccadc_reading_v2((s64)cc);
}

#define CC_READING_TICKS	55
#define SLEEP_CLK_HZ		32768
#define SECONDS_PER_HOUR	3600
static s64 ccmicrovolt_to_uvh(s64 cc_uv)
{
	return div_s64(cc_uv * CC_READING_TICKS,
			SLEEP_CLK_HZ * SECONDS_PER_HOUR);
}

#define GAIN_REFERENCE_UV 25000
/*
 * gain compensation for ccadc readings - common for vsense based and
 * couloumb counter based readings
 */
static s64 cc_adjust_for_gain(struct pm8921_bms_chip *chip, s64 cc)
{
	if (chip->ccadc_gain_uv == 0)
		return cc;

	return div_s64(cc * GAIN_REFERENCE_UV, chip->ccadc_gain_uv);
}

/* returns the signed value read from the hardware */
static int read_cc(struct pm8921_bms_chip *chip, int *result)
{
	int rc;
	uint16_t msw, lsw;

	rc = pm_bms_read_output_data(chip, CC_LSB, &lsw);
	if (rc) {
		pr_err("fail to read CC_LSB rc = %d\n", rc);
		return rc;
	}
	rc = pm_bms_read_output_data(chip, CC_MSB, &msw);
	if (rc) {
		pr_err("fail to read CC_MSB rc = %d\n", rc);
		return rc;
	}
	*result = msw << 16 | lsw;
	pr_debug("msw = %04x lsw = %04x cc = %d\n", msw, lsw, *result);
	*result = *result - chip->cc_reading_at_100;
	pr_debug("cc = %d after subtracting %d\n",
					*result, chip->cc_reading_at_100);
	return 0;
}

static int read_last_good_ocv(struct pm8921_bms_chip *chip, uint *result)
{
	int rc;
	uint16_t reading;

	rc = pm_bms_read_output_data(chip, LAST_GOOD_OCV_VALUE, &reading);
	if (rc) {
		pr_err("fail to read LAST_GOOD_OCV_VALUE rc = %d\n", rc);
		return rc;
	}

	if (chip->ocv_reading_at_100 != reading) {
		chip->ocv_reading_at_100 = 0;
		chip->cc_reading_at_100 = 0;
		*result = xoadc_reading_to_microvolt(reading);
		pr_debug("raw = %04x ocv_uV = %u\n", reading, *result);
		*result = adjust_xo_vbatt_reading(chip, *result);
		pr_debug("after adj ocv_uV = %u\n", *result);
		if (*result != 0)
			last_ocv_uv = *result;
	} else {
		/*
		 * force 100% ocv by selecting the highest profiled ocv
		 * This is the first row last column entry in the ocv
		 * lookup table
		 */
		int cols = chip->pc_temp_ocv_lut->cols;

		pr_debug("Forcing max voltage %d\n",
				1000 * chip->pc_temp_ocv_lut->ocv[0][cols-1]);
		*result = 1000 * chip->pc_temp_ocv_lut->ocv[0][cols-1];
	}

	return 0;
}

static int read_vbatt_for_rbatt(struct pm8921_bms_chip *chip, uint *result)
{
	int rc;
	uint16_t reading;

	rc = pm_bms_read_output_data(chip, VBATT_FOR_RBATT, &reading);
	if (rc) {
		pr_err("fail to read VBATT_FOR_RBATT rc = %d\n", rc);
		return rc;
	}
	*result = xoadc_reading_to_microvolt(reading);
	pr_debug("raw = %04x vbatt_for_r_microV = %u\n", reading, *result);
	*result = adjust_xo_vbatt_reading(chip, *result);
	pr_debug("after adj vbatt_for_r_uV = %u\n", *result);
	return 0;
}

static int read_vsense_for_rbatt(struct pm8921_bms_chip *chip, uint *result)
{
	int rc;
	uint16_t reading;

	rc = pm_bms_read_output_data(chip, VSENSE_FOR_RBATT, &reading);
	if (rc) {
		pr_err("fail to read VSENSE_FOR_RBATT rc = %d\n", rc);
		return rc;
	}
	*result = ccadc_reading_to_microvolt(chip, reading);
	pr_debug("raw = %04x vsense_for_r_uV = %u\n", reading, *result);
	*result = cc_adjust_for_gain(chip, *result);
	pr_debug("after adj vsense_for_r_uV = %u\n", *result);
	return 0;
}

static int read_ocv_for_rbatt(struct pm8921_bms_chip *chip, uint *result)
{
	int rc;
	uint16_t reading;

	rc = pm_bms_read_output_data(chip, OCV_FOR_RBATT, &reading);
	if (rc) {
		pr_err("fail to read OCV_FOR_RBATT rc = %d\n", rc);
		return rc;
	}
	*result = xoadc_reading_to_microvolt(reading);
	pr_debug("raw = %04x ocv_for_r_uV = %u\n", reading, *result);
	*result = adjust_xo_vbatt_reading(chip, *result);
	pr_debug("after adj ocv_for_r_uV = %u\n", *result);
	return 0;
}

static int read_vsense_avg(struct pm8921_bms_chip *chip, int *result)
{
	int rc;
	int16_t reading;

	rc = pm_bms_read_output_data(chip, VSENSE_AVG, &reading);
	if (rc) {
		pr_err("fail to read VSENSE_AVG rc = %d\n", rc);
		return rc;
	}
	*result = ccadc_reading_to_microvolt(chip, reading);
	pr_debug("raw = %04x vsense = %d\n", reading, *result);
	*result = cc_adjust_for_gain(the_chip, (s64)*result);
	pr_debug("after adj vsense = %d\n", *result);
	return 0;
}

static int linear_interpolate(int y0, int x0, int y1, int x1, int x)
{
	if (y0 == y1 || x == x0)
		return y0;
	if (x1 == x0 || x == x1)
		return y1;

	return y0 + ((y1 - y0) * (x - x0) / (x1 - x0));
}

static int interpolate_single_lut(struct single_row_lut *lut, int x)
{
	int i, result;

	if (x < lut->x[0]) {
		pr_debug("x %d less than known range return y = %d lut = %pS\n",
							x, lut->y[0], lut);
		return lut->y[0];
	}
	if (x > lut->x[lut->cols - 1]) {
		pr_debug("x %d more than known range return y = %d lut = %pS\n",
						x, lut->y[lut->cols - 1], lut);
		return lut->y[lut->cols - 1];
	}

	for (i = 0; i < lut->cols; i++)
		if (x <= lut->x[i])
			break;
	if (x == lut->x[i]) {
		result = lut->y[i];
	} else {
		result = linear_interpolate(
			lut->y[i - 1],
			lut->x[i - 1],
			lut->y[i],
			lut->x[i],
			x);
	}
	return result;
}

static int interpolate_fcc(struct pm8921_bms_chip *chip, int batt_temp)
{
	return interpolate_single_lut(chip->fcc_temp_lut, batt_temp);
}

static int interpolate_fcc_adjusted(struct pm8921_bms_chip *chip, int batt_temp)
{
	return interpolate_single_lut(chip->adjusted_fcc_temp_lut, batt_temp);
}

static int interpolate_scalingfactor_fcc(struct pm8921_bms_chip *chip,
								int cycles)
{
	/*
	 * sf table could be null when no battery aging data is available, in
	 * that case return 100%
	 */
	if (chip->fcc_sf_lut)
		return interpolate_single_lut(chip->fcc_sf_lut, cycles);
	else
		return 100;
}

static int interpolate_scalingfactor_pc(struct pm8921_bms_chip *chip,
				int cycles, int pc)
{
	int i, scalefactorrow1, scalefactorrow2, scalefactor;
	int rows, cols;
	int row1 = 0;
	int row2 = 0;

	/*
	 * sf table could be null when no battery aging data is available, in
	 * that case return 100%
	 */
	if (!chip->pc_sf_lut)
		return 100;

	rows = chip->pc_sf_lut->rows;
	cols = chip->pc_sf_lut->cols;
	if (pc > chip->pc_sf_lut->percent[0]) {
		pr_debug("pc %d greater than known pc ranges for sfd\n", pc);
		row1 = 0;
		row2 = 0;
	}
	if (pc < chip->pc_sf_lut->percent[rows - 1]) {
		pr_debug("pc %d less than known pc ranges for sf", pc);
		row1 = rows - 1;
		row2 = rows - 1;
	}
	for (i = 0; i < rows; i++) {
		if (pc == chip->pc_sf_lut->percent[i]) {
			row1 = i;
			row2 = i;
			break;
		}
		if (pc > chip->pc_sf_lut->percent[i]) {
			row1 = i - 1;
			row2 = i;
			break;
		}
	}

	if (cycles < chip->pc_sf_lut->cycles[0])
		cycles = chip->pc_sf_lut->cycles[0];
	if (cycles > chip->pc_sf_lut->cycles[cols - 1])
		cycles = chip->pc_sf_lut->cycles[cols - 1];

	for (i = 0; i < cols; i++)
		if (cycles <= chip->pc_sf_lut->cycles[i])
			break;
	if (cycles == chip->pc_sf_lut->cycles[i]) {
		scalefactor = linear_interpolate(
				chip->pc_sf_lut->sf[row1][i],
				chip->pc_sf_lut->percent[row1],
				chip->pc_sf_lut->sf[row2][i],
				chip->pc_sf_lut->percent[row2],
				pc);
		return scalefactor;
	}

	scalefactorrow1 = linear_interpolate(
				chip->pc_sf_lut->sf[row1][i - 1],
				chip->pc_sf_lut->cycles[i - 1],
				chip->pc_sf_lut->sf[row1][i],
				chip->pc_sf_lut->cycles[i],
				cycles);

	scalefactorrow2 = linear_interpolate(
				chip->pc_sf_lut->sf[row2][i - 1],
				chip->pc_sf_lut->cycles[i - 1],
				chip->pc_sf_lut->sf[row2][i],
				chip->pc_sf_lut->cycles[i],
				cycles);

	scalefactor = linear_interpolate(
				scalefactorrow1,
				chip->pc_sf_lut->percent[row1],
				scalefactorrow2,
				chip->pc_sf_lut->percent[row2],
				pc);

	return scalefactor;
}

static int is_between(int left, int right, int value)
{
	if (left >= right && left >= value && value >= right)
		return 1;
	if (left <= right && left <= value && value <= right)
		return 1;

	return 0;
}

static int interpolate_pc(struct pm8921_bms_chip *chip,
				int batt_temp, int ocv)
{
	int i, j, pcj, pcj_minus_one, pc;
	int rows = chip->pc_temp_ocv_lut->rows;
	int cols = chip->pc_temp_ocv_lut->cols;

	if (batt_temp < chip->pc_temp_ocv_lut->temp[0]) {
		pr_debug("batt_temp %d < known temp range for pc\n", batt_temp);
		batt_temp = chip->pc_temp_ocv_lut->temp[0];
	}
	if (batt_temp > chip->pc_temp_ocv_lut->temp[cols - 1]) {
		pr_debug("batt_temp %d > known temp range for pc\n", batt_temp);
		batt_temp = chip->pc_temp_ocv_lut->temp[cols - 1];
	}

	for (j = 0; j < cols; j++)
		if (batt_temp <= chip->pc_temp_ocv_lut->temp[j])
			break;
	if (batt_temp == chip->pc_temp_ocv_lut->temp[j]) {
		/* found an exact match for temp in the table */
		if (ocv >= chip->pc_temp_ocv_lut->ocv[0][j])
			return chip->pc_temp_ocv_lut->percent[0];
		if (ocv <= chip->pc_temp_ocv_lut->ocv[rows - 1][j])
			return chip->pc_temp_ocv_lut->percent[rows - 1];
		for (i = 0; i < rows; i++) {
			if (ocv >= chip->pc_temp_ocv_lut->ocv[i][j]) {
				if (ocv == chip->pc_temp_ocv_lut->ocv[i][j])
					return
					chip->pc_temp_ocv_lut->percent[i];
				pc = linear_interpolate(
					chip->pc_temp_ocv_lut->percent[i],
					chip->pc_temp_ocv_lut->ocv[i][j],
					chip->pc_temp_ocv_lut->percent[i - 1],
					chip->pc_temp_ocv_lut->ocv[i - 1][j],
					ocv);
				return pc;
			}
		}
	}

	/*
	 * batt_temp is within temperature for
	 * column j-1 and j
	 */
	if (ocv >= chip->pc_temp_ocv_lut->ocv[0][j])
		return chip->pc_temp_ocv_lut->percent[0];
	if (ocv <= chip->pc_temp_ocv_lut->ocv[rows - 1][j - 1])
		return chip->pc_temp_ocv_lut->percent[rows - 1];

	pcj_minus_one = 0;
	pcj = 0;
	for (i = 0; i < rows-1; i++) {
		if (pcj == 0
			&& is_between(chip->pc_temp_ocv_lut->ocv[i][j],
				chip->pc_temp_ocv_lut->ocv[i+1][j], ocv)) {
			pcj = linear_interpolate(
				chip->pc_temp_ocv_lut->percent[i],
				chip->pc_temp_ocv_lut->ocv[i][j],
				chip->pc_temp_ocv_lut->percent[i + 1],
				chip->pc_temp_ocv_lut->ocv[i+1][j],
				ocv);
		}

		if (pcj_minus_one == 0
			&& is_between(chip->pc_temp_ocv_lut->ocv[i][j-1],
				chip->pc_temp_ocv_lut->ocv[i+1][j-1], ocv)) {

			pcj_minus_one = linear_interpolate(
				chip->pc_temp_ocv_lut->percent[i],
				chip->pc_temp_ocv_lut->ocv[i][j-1],
				chip->pc_temp_ocv_lut->percent[i + 1],
				chip->pc_temp_ocv_lut->ocv[i+1][j-1],
				ocv);
		}

		if (pcj && pcj_minus_one) {
			pc = linear_interpolate(
				pcj_minus_one,
				chip->pc_temp_ocv_lut->temp[j-1],
				pcj,
				chip->pc_temp_ocv_lut->temp[j],
				batt_temp);
			return pc;
		}
	}

	if (pcj)
		return pcj;

	if (pcj_minus_one)
		return pcj_minus_one;

	pr_debug("%d ocv wasn't found for temp %d in the LUT returning 100%%",
							ocv, batt_temp);
	return 100;
}

static int calculate_rbatt(struct pm8921_bms_chip *chip)
{
	int rc;
	unsigned int ocv, vsense, vbatt, r_batt;

	rc = read_ocv_for_rbatt(chip, &ocv);
	if (rc) {
		pr_err("fail to read ocv_for_rbatt rc = %d\n", rc);
		ocv = 0;
	}

	rc = read_vbatt_for_rbatt(chip, &vbatt);
	if (rc) {
		pr_err("fail to read vbatt_for_rbatt rc = %d\n", rc);
		ocv = 0;
	}

	rc = read_vsense_for_rbatt(chip, &vsense);
	if (rc) {
		pr_err("fail to read vsense_for_rbatt rc = %d\n", rc);
		ocv = 0;
	}
	if (ocv == 0
		|| ocv == vbatt
		|| vsense == 0) {
		pr_debug("rbatt readings unavailable ocv = %d, vbatt = %d,"
					"vsen = %d\n",
					ocv, vbatt, vsense);
		return -EINVAL;
	}
	r_batt = ((ocv - vbatt) * chip->r_sense) / vsense;
	last_rbatt = r_batt;
	pr_debug("r_batt = %umilliOhms", r_batt);
	return r_batt;
}

static int calculate_fcc(struct pm8921_bms_chip *chip, int batt_temp,
							int chargecycles)
{
	int initfcc, result, scalefactor = 0;

	if (chip->adjusted_fcc_temp_lut == NULL) {
		initfcc = interpolate_fcc(chip, batt_temp);

		scalefactor = interpolate_scalingfactor_fcc(chip, chargecycles);

		/* Multiply the initial FCC value by the scale factor. */
		result = (initfcc * scalefactor) / 100;
		pr_debug("fcc mAh = %d\n", result);
		return result;
	} else {
		return interpolate_fcc_adjusted(chip, batt_temp);
	}
}

static int get_battery_uvolts(struct pm8921_bms_chip *chip, int *uvolts)
{
	int rc;
	struct pm8921_adc_chan_result result;

	rc = pm8921_adc_read(chip->vbat_channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					chip->vbat_channel, rc);
		return rc;
	}
	pr_debug("mvolts phy = %lld meas = 0x%llx", result.physical,
						result.measurement);
	*uvolts = (int)result.physical;
	*uvolts = *uvolts * 1000;
	return 0;
}

static int adc_based_ocv(struct pm8921_bms_chip *chip, int *ocv)
{
	int vbatt, rbatt, ibatt, rc;

	rc = get_battery_uvolts(chip, &vbatt);
	if (rc) {
		pr_err("failed to read vbatt from adc rc = %d\n", rc);
		return rc;
	}

	rc =  pm8921_bms_get_battery_current(&ibatt);
	if (rc) {
		pr_err("failed to read batt current rc = %d\n", rc);
		return rc;
	}

	rbatt = calculate_rbatt(the_chip);
	if (rbatt < 0)
		rbatt = (last_rbatt < 0) ? DEFAULT_RBATT_MOHMS : last_rbatt;
	*ocv = vbatt + ibatt * rbatt;
	return 0;
}

static int calculate_pc(struct pm8921_bms_chip *chip, int ocv_uv, int batt_temp,
							int chargecycles)
{
	int pc, scalefactor;

	pc = interpolate_pc(chip, batt_temp, ocv_uv / 1000);
	pr_debug("pc = %u for ocv = %dmicroVolts batt_temp = %d\n",
					pc, ocv_uv, batt_temp);

	scalefactor = interpolate_scalingfactor_pc(chip, chargecycles, pc);
	pr_debug("scalefactor = %u batt_temp = %d\n", scalefactor, batt_temp);

	/* Multiply the initial FCC value by the scale factor. */
	pc = (pc * scalefactor) / 100;
	return pc;
}

static void calculate_cc_mah(struct pm8921_bms_chip *chip, int64_t *val,
			int *coulumb_counter)
{
	int rc;
	int64_t cc_voltage_uv, cc_uvh, cc_mah;

	rc = read_cc(the_chip, coulumb_counter);
	cc_voltage_uv = (int64_t)*coulumb_counter;
	cc_voltage_uv = cc_to_microvolt(chip, cc_voltage_uv);
	cc_voltage_uv = cc_adjust_for_gain(chip, cc_voltage_uv);
	pr_debug("cc_voltage_uv = %lld microvolts\n", cc_voltage_uv);
	cc_uvh = ccmicrovolt_to_uvh(cc_voltage_uv);
	pr_debug("cc_uvh = %lld micro_volt_hour\n", cc_uvh);
	cc_mah = div_s64(cc_uvh, chip->r_sense);
	*val = cc_mah;
}

static int calculate_unusable_charge_mah(struct pm8921_bms_chip *chip,
				 int fcc, int batt_temp, int chargecycles)
{
	int rbatt, voltage_unusable_uv, pc_unusable;

	rbatt = calculate_rbatt(chip);
	if (rbatt < 0) {
		rbatt = (last_rbatt < 0) ? DEFAULT_RBATT_MOHMS : last_rbatt;
		pr_debug("rbatt unavailable assuming %d\n", rbatt);
	}

	/* calculate unusable charge */
	voltage_unusable_uv = (rbatt * chip->i_test)
						+ (chip->v_failure * 1000);
	pc_unusable = calculate_pc(chip, voltage_unusable_uv,
						batt_temp, chargecycles);
	pr_debug("rbatt = %umilliOhms unusable_v =%d unusable_pc = %d\n",
			rbatt, voltage_unusable_uv, pc_unusable);
	return (fcc * pc_unusable) / 100;
}

/* calculate remainging charge at the time of ocv */
static int calculate_remaining_charge_mah(struct pm8921_bms_chip *chip, int fcc,
						int batt_temp, int chargecycles)
{
	int rc, ocv, pc;

	/* calculate remainging charge */
	ocv = 0;
	rc = read_last_good_ocv(chip, &ocv);
	if (rc)
		pr_debug("failed to read ocv rc = %d\n", rc);

	if (ocv == 0) {
		ocv = last_ocv_uv;
		pr_debug("ocv not available using last_ocv_uv=%d\n", ocv);
	}

	pc = calculate_pc(chip, ocv, batt_temp, chargecycles);
	pr_debug("ocv = %d pc = %d\n", ocv, pc);
	return (fcc * pc) / 100;
}

static void calculate_charging_params(struct pm8921_bms_chip *chip,
						int batt_temp, int chargecycles,
						int *fcc,
						int *unusable_charge,
						int *remaining_charge,
						int64_t *cc_mah)
{
	int coulumb_counter;
	unsigned long flags;

	*fcc = calculate_fcc(chip, batt_temp, chargecycles);
	pr_debug("FCC = %umAh batt_temp = %d, cycles = %d",
					*fcc, batt_temp, chargecycles);

	/* fcc doesnt need to be read from hardware, lock the bms now */
	spin_lock_irqsave(&chip->bms_output_lock, flags);
	pm_bms_lock_output_data(chip);

	*unusable_charge = calculate_unusable_charge_mah(chip, *fcc,
						batt_temp, chargecycles);

	pr_debug("UUC = %umAh", *unusable_charge);

	/* calculate remainging charge */
	*remaining_charge = calculate_remaining_charge_mah(chip, *fcc,
						batt_temp, chargecycles);
	pr_debug("RC = %umAh\n", *remaining_charge);

	/* calculate cc milli_volt_hour */
	calculate_cc_mah(chip, cc_mah, &coulumb_counter);
	pr_debug("cc_mah = %lldmAh cc = %d\n", *cc_mah, coulumb_counter);

	pm_bms_unlock_output_data(chip);
	spin_unlock_irqrestore(&chip->bms_output_lock, flags);
}

static int calculate_real_fcc(struct pm8921_bms_chip *chip,
						int batt_temp, int chargecycles)
{
	int fcc, unusable_charge;
	int remaining_charge;
	int64_t cc_mah;
	int real_fcc;

	calculate_charging_params(chip, batt_temp, chargecycles,
						&fcc,
						&unusable_charge,
						&remaining_charge,
						&cc_mah);

	real_fcc = remaining_charge - cc_mah;
	pr_debug("real_fcc = %d, RC = %d CC = %lld\n",
			real_fcc, remaining_charge, cc_mah);
	return real_fcc;
}
/*
 * Remaining Usable Charge = remaining_charge (charge at ocv instance)
 *				- coloumb counter charge
 *				- unusable charge (due to battery resistance)
 * SOC% = (remaining usable charge/ fcc - usable_charge);
 */
#define BMS_BATT_NOMINAL	3700000
#define MIN_OPERABLE_SOC	10
#define BATTERY_POWER_SUPPLY_SOC	53
static int calculate_state_of_charge(struct pm8921_bms_chip *chip,
						int batt_temp, int chargecycles)
{
	int remaining_usable_charge, fcc, unusable_charge;
	int remaining_charge, soc;
	int update_userspace = 1;
	int64_t cc_mah;

	calculate_charging_params(chip, batt_temp, chargecycles,
						&fcc,
						&unusable_charge,
						&remaining_charge,
						&cc_mah);

	/* calculate remaining usable charge */
	remaining_usable_charge = remaining_charge - cc_mah - unusable_charge;
	pr_debug("RUC = %dmAh\n", remaining_usable_charge);
	soc = (remaining_usable_charge * 100) / (fcc - unusable_charge);
	if (soc > 100)
		soc = 100;
	pr_debug("SOC = %u%%\n", soc);

	if (soc < MIN_OPERABLE_SOC) {
		int ocv = 0, rc;

		rc = adc_based_ocv(chip, &ocv);
		if (rc == 0 && ocv >= BMS_BATT_NOMINAL) {
			/*
			 * The ocv doesnt seem to have dropped for
			 * soc to go negative.
			 * The setup must be using a power supply
			 * instead of real batteries.
			 * Fake high enough soc to prevent userspace
			 * shutdown for low battery
			 */
			soc = BATTERY_POWER_SUPPLY_SOC;
			pr_debug("Adjusting SOC to %d\n", soc);
		}
	}

	if (soc < 0) {
		pr_err("bad rem_usb_chg = %d rem_chg %d,"
				"cc_mah %lld, unusb_chg %d\n",
				remaining_usable_charge, remaining_charge,
				cc_mah, unusable_charge);
		pr_err("for bad rem_usb_chg last_ocv_uv = %d"
				"chargecycles = %d, batt_temp = %d"
				"fcc = %d soc =%d\n",
				last_ocv_uv, chargecycles, batt_temp,
				fcc, soc);
		update_userspace = 0;
	}

	if (last_soc == -EINVAL || soc <= last_soc) {
		last_soc = update_userspace ? soc : last_soc;
		return soc;
	}

	/*
	 * soc > last_soc
	 * the device must be charging for reporting a higher soc, if not ignore
	 * this soc and continue reporting the last_soc
	 */
	if (the_chip->start_percent != 0) {
		last_soc = soc;
	} else {
		pr_debug("soc = %d reporting last_soc = %d\n", soc, last_soc);
		soc = last_soc;
	}

	return soc;
}

#define XOADC_MAX_1P25V		1312500
#define XOADC_MIN_1P25V		1187500
#define XOADC_MAX_0P625V		656250
#define XOADC_MIN_0P625V		593750

#define HKADC_V_PER_BIT_MUL_FACTOR	977
#define HKADC_V_PER_BIT_DIV_FACTOR	10
static int calib_hkadc_convert_microvolt(unsigned int phy)
{
	return (phy - 0x6000) *
			HKADC_V_PER_BIT_MUL_FACTOR / HKADC_V_PER_BIT_DIV_FACTOR;
}

static void calib_hkadc(struct pm8921_bms_chip *chip)
{
	int voltage, rc;
	struct pm8921_adc_chan_result result;

	rc = pm8921_adc_read(the_chip->ref1p25v_channel, &result);
	if (rc) {
		pr_err("ADC failed for 1.25volts rc = %d\n", rc);
		return;
	}
	voltage = calib_hkadc_convert_microvolt(result.adc_code);

	pr_debug("result 1.25v = 0x%x, voltage = %duV adc_meas = %lld\n",
				result.adc_code, voltage, result.measurement);

	/* check for valid range */
	if (voltage > XOADC_MAX_1P25V)
		voltage = XOADC_MAX_1P25V;
	else if (voltage < XOADC_MIN_1P25V)
		voltage = XOADC_MIN_1P25V;
	chip->xoadc_v125 = voltage;

	rc = pm8921_adc_read(the_chip->ref625mv_channel, &result);
	if (rc) {
		pr_err("ADC failed for 1.25volts rc = %d\n", rc);
		return;
	}
	voltage = calib_hkadc_convert_microvolt(result.adc_code);
	pr_debug("result 0.625V = 0x%x, voltage = %duV adc_mead = %lld\n",
				result.adc_code, voltage, result.measurement);
	/* check for valid range */
	if (voltage > XOADC_MAX_0P625V)
		voltage = XOADC_MAX_0P625V;
	else if (voltage < XOADC_MIN_0P625V)
		voltage = XOADC_MIN_0P625V;

	chip->xoadc_v0625 = voltage;
}

static void calibrate_hkadc_work(struct work_struct *work)
{
	struct pm8921_bms_chip *chip = container_of(work,
				struct pm8921_bms_chip, calib_hkadc_work);

	calib_hkadc(chip);
}

#define START_CONV_BIT	BIT(7)
#define EOC_CONV_BIT	BIT(6)
#define SEL_CCADC_BIT	BIT(1)
#define EN_ARB_BIT	BIT(0)

#define CCADC_CALIB_DIG_PARAM	0xE3
#define CCADC_CALIB_RSV_GND	0x40
#define CCADC_CALIB_RSV_25MV	0x80
#define CCADC_CALIB_ANA_PARAM	0x1B
#define SAMPLE_COUNT		16
#define ADC_WAIT_COUNT		10

#define CCADC_MAX_25MV		30000
#define CCADC_MIN_25MV		20000
#define CCADC_MAX_0UV		-4000
#define CCADC_MIN_0UV		-7000

#define CCADC_INTRINSIC_OFFSET  0xC000

#define REG_SBI_CONFIG		0x04F
#define PAGE3_ENABLE_MASK	0x6

static int calib_ccadc_enable_trim_access(struct pm8921_bms_chip *chip,
								u8 *sbi_config)
{
	u8 reg;
	int rc;

	rc = pm8xxx_readb(chip->dev->parent, REG_SBI_CONFIG, sbi_config);
	if (rc) {
		pr_err("error = %d reading sbi config reg\n", rc);
		return rc;
	}

	reg = *sbi_config | PAGE3_ENABLE_MASK;
	return pm8xxx_writeb(chip->dev->parent, REG_SBI_CONFIG, reg);
}

static int calib_ccadc_restore_trim_access(struct pm8921_bms_chip *chip,
							u8 sbi_config)
{
	return pm8xxx_writeb(chip->dev->parent, REG_SBI_CONFIG, sbi_config);
}

static int calib_ccadc_enable_arbiter(struct pm8921_bms_chip *chip)
{
	int rc;

	/* enable Arbiter, must be sent twice */
	rc = pm_bms_masked_write(chip, ADC_ARB_SECP_CNTRL,
			SEL_CCADC_BIT | EN_ARB_BIT, SEL_CCADC_BIT | EN_ARB_BIT);
	if (rc < 0) {
		pr_err("error = %d enabling arbiter for offset\n", rc);
		return rc;
	}
	rc = pm_bms_masked_write(chip, ADC_ARB_SECP_CNTRL,
			SEL_CCADC_BIT | EN_ARB_BIT, SEL_CCADC_BIT | EN_ARB_BIT);
	if (rc < 0) {
		pr_err("error = %d writing ADC_ARB_SECP_CNTRL\n", rc);
		return rc;
	}
	return 0;
}

static int calib_start_conv(struct pm8921_bms_chip *chip,
					u16 *result)
{
	int rc, i;
	u8 data_msb, data_lsb, reg;

	/* Start conversion */
	rc = pm_bms_masked_write(chip, ADC_ARB_SECP_CNTRL,
					START_CONV_BIT, START_CONV_BIT);
	if (rc < 0) {
		pr_err("error = %d starting offset meas\n", rc);
		return rc;
	}

	/* Wait for End of conversion */
	for (i = 0; i < ADC_WAIT_COUNT; i++) {
		rc = pm8xxx_readb(chip->dev->parent,
					ADC_ARB_SECP_CNTRL, &reg);
		if (rc < 0) {
			pr_err("error = %d read eoc for offset\n", rc);
			return rc;
		}
		if ((reg & (START_CONV_BIT | EOC_CONV_BIT)) != EOC_CONV_BIT)
			msleep(60);
		else
			break;
	}
	if (i == ADC_WAIT_COUNT) {
		pr_err("waited too long for offset eoc\n");
		return rc;
	}

	rc = pm8xxx_readb(chip->dev->parent, ADC_ARB_SECP_DATA0, &data_lsb);
	if (rc < 0) {
		pr_err("error = %d reading offset lsb\n", rc);
		return rc;
	}

	rc = pm8xxx_readb(chip->dev->parent, ADC_ARB_SECP_DATA1, &data_msb);
	if (rc < 0) {
		pr_err("error = %d reading offset msb\n", rc);
		return rc;
	}

	*result = (data_msb << 8) | data_lsb;
	return 0;
}

static int calib_ccadc_read_trim(struct pm8921_bms_chip *chip,
					int addr, u8 *data_msb, u8 *data_lsb)
{
	int rc;
	u8 sbi_config;

	calib_ccadc_enable_trim_access(chip, &sbi_config);
	rc = pm8xxx_readb(chip->dev->parent, addr, data_msb);
	if (rc < 0) {
		pr_err("error = %d read msb\n", rc);
		return rc;
	}
	rc = pm8xxx_readb(chip->dev->parent, addr + 1, data_lsb);
	if (rc < 0) {
		pr_err("error = %d read lsb\n", rc);
		return rc;
	}
	calib_ccadc_restore_trim_access(chip, sbi_config);
	return 0;
}

static int calib_ccadc_read_gain_uv(struct pm8921_bms_chip *chip)
{
	s8 data_msb;
	u8 data_lsb;
	int rc, gain, offset;

	rc = calib_ccadc_read_trim(chip, CCADC_FULLSCALE_TRIM1,
						&data_msb, &data_lsb);
	gain = (data_msb << 8) | data_lsb;

	rc = calib_ccadc_read_trim(chip, CCADC_OFFSET_TRIM1,
						&data_msb, &data_lsb);
	offset = (data_msb << 8) | data_lsb;

	pr_debug("raw gain trim = 0x%x offset trim =0x%x\n", gain, offset);
	gain = ccadc_reading_to_microvolt(chip, (s64)gain - offset);
	return gain;
}

#define CCADC_PROGRAM_TRIM_COUNT	2
#define ADC_ARB_BMS_CNTRL_CCADC_SHIFT	4
#define ADC_ARB_BMS_CNTRL_CONV_MASK	0x03
#define BMS_CONV_IN_PROGRESS		0x2

static int calib_ccadc_program_trim(struct pm8921_bms_chip *chip,
					int addr, u8 data_msb, u8 data_lsb,
					int wait)
{
	int i, rc, loop;
	u8 cntrl, sbi_config;
	bool in_progress = 0;

	loop = wait ? CCADC_PROGRAM_TRIM_COUNT : 0;

	calib_ccadc_enable_trim_access(chip, &sbi_config);

	for (i = 0; i < loop; i++) {
		rc = pm8xxx_readb(chip->dev->parent, ADC_ARB_BMS_CNTRL, &cntrl);
		if (rc < 0) {
			pr_err("error = %d reading ADC_ARB_BMS_CNTRL\n", rc);
			return rc;
		}

		/* break if a ccadc conversion is not happening */
		in_progress = (((cntrl >> ADC_ARB_BMS_CNTRL_CCADC_SHIFT)
			& ADC_ARB_BMS_CNTRL_CONV_MASK) == BMS_CONV_IN_PROGRESS);

		if (!in_progress)
			break;
	}

	if (in_progress) {
		pr_debug("conv in progress cannot write trim,returing EBUSY\n");
		return -EBUSY;
	}

	rc = pm8xxx_writeb(chip->dev->parent, addr, data_msb);
	if (rc < 0) {
		pr_err("error = %d write msb = 0x%x\n", rc, data_msb);
		return rc;
	}
	rc = pm8xxx_writeb(chip->dev->parent, addr + 1, data_lsb);
	if (rc < 0) {
		pr_err("error = %d write lsb = 0x%x\n", rc, data_lsb);
		return rc;
	}
	calib_ccadc_restore_trim_access(chip, sbi_config);
	return 0;
}

static void calib_ccadc(struct pm8921_bms_chip *chip)
{
	u8 data_msb, data_lsb, sec_cntrl;
	int result_offset, voltage_offset, result_gain;
	u16 result;
	int i, rc;

	rc = pm8xxx_readb(chip->dev->parent, ADC_ARB_SECP_CNTRL, &sec_cntrl);
	if (rc < 0) {
		pr_err("error = %d reading ADC_ARB_SECP_CNTRL\n", rc);
		return;
	}

	rc = calib_ccadc_enable_arbiter(chip);
	if (rc < 0) {
		pr_err("error = %d enabling arbiter for offset\n", rc);
		goto bail;
	}

	/*
	 * Set decimation ratio to 4k, lower ratio may be used in order to speed
	 * up, pending verification through bench
	 */
	rc = pm8xxx_writeb(chip->dev->parent, ADC_ARB_SECP_DIG_PARAM,
							CCADC_CALIB_DIG_PARAM);
	if (rc < 0) {
		pr_err("error = %d writing ADC_ARB_SECP_DIG_PARAM\n", rc);
		goto bail;
	}

	result_offset = 0;
	for (i = 0; i < SAMPLE_COUNT; i++) {
		/* Short analog inputs to CCADC internally to ground */
		rc = pm8xxx_writeb(chip->dev->parent, ADC_ARB_SECP_RSV,
							CCADC_CALIB_RSV_GND);
		if (rc < 0) {
			pr_err("error = %d selecting gnd voltage\n", rc);
			goto bail;
		}

		/* Enable CCADC */
		rc = pm8xxx_writeb(chip->dev->parent, ADC_ARB_SECP_ANA_PARAM,
							CCADC_CALIB_ANA_PARAM);
		if (rc < 0) {
			pr_err("error = %d enabling ccadc\n", rc);
			goto bail;
		}

		rc = calib_start_conv(chip, &result);
		if (rc < 0) {
			pr_err("error = %d for zero volt measurement\n", rc);
			goto bail;
		}

		result_offset += result;
	}

	result_offset = result_offset / SAMPLE_COUNT;

	voltage_offset = ccadc_reading_to_microvolt(chip,
			((s64)result_offset - CCADC_INTRINSIC_OFFSET));

	pr_debug("offset result_offset = 0x%x, voltage = %d microVolts\n",
				result_offset, voltage_offset);

	/* Sanity Check */
	if (voltage_offset > CCADC_MAX_0UV) {
		pr_err("offset voltage = %d is huge limiting to %d\n",
					voltage_offset, CCADC_MAX_0UV);
		result_offset = CCADC_INTRINSIC_OFFSET
			+ microvolt_to_ccadc_reading(chip, (s64)CCADC_MAX_0UV);
	} else if (voltage_offset < CCADC_MIN_0UV) {
		pr_err("offset voltage = %d is too low limiting to %d\n",
					voltage_offset, CCADC_MIN_0UV);
		result_offset = CCADC_INTRINSIC_OFFSET
			+ microvolt_to_ccadc_reading(chip, (s64)CCADC_MIN_0UV);
	}

	chip->ccadc_result_offset = result_offset;
	data_msb = chip->ccadc_result_offset >> 8;
	data_lsb = chip->ccadc_result_offset;

	rc = calib_ccadc_program_trim(chip, CCADC_OFFSET_TRIM1,
						data_msb, data_lsb, 1);
	if (rc) {
		pr_debug("error = %d programming offset trim 0x%02x 0x%02x\n",
					rc, data_msb, data_lsb);
		/* enable the interrupt and write it when it fires */
		pm8921_bms_enable_irq(chip, PM8921_BMS_CCADC_EOC);
	}

	rc = calib_ccadc_enable_arbiter(chip);
	if (rc < 0) {
		pr_err("error = %d enabling arbiter for gain\n", rc);
		goto bail;
	}

	/*
	 * Set decimation ratio to 4k, lower ratio may be used in order to speed
	 * up, pending verification through bench
	 */
	rc = pm8xxx_writeb(chip->dev->parent, ADC_ARB_SECP_DIG_PARAM,
							CCADC_CALIB_DIG_PARAM);
	if (rc < 0) {
		pr_err("error = %d enabling decimation ration for gain\n", rc);
		goto bail;
	}

	result_gain = 0;
	for (i = 0; i < SAMPLE_COUNT; i++) {
		rc = pm8xxx_writeb(chip->dev->parent,
					ADC_ARB_SECP_RSV, CCADC_CALIB_RSV_25MV);
		if (rc < 0) {
			pr_err("error = %d selecting 25mV for gain\n", rc);
			goto bail;
		}

		/* Enable CCADC */
		rc = pm8xxx_writeb(chip->dev->parent, ADC_ARB_SECP_ANA_PARAM,
							CCADC_CALIB_ANA_PARAM);
		if (rc < 0) {
			pr_err("error = %d enabling ccadc\n", rc);
			goto bail;
		}

		rc = calib_start_conv(chip, &result);
		if (rc < 0) {
			pr_err("error = %d for adc reading 25mV\n", rc);
			goto bail;
		}

		result_gain += result;
	}
	result_gain = result_gain / SAMPLE_COUNT;

	/*
	 * result_offset includes INTRINSIC OFFSET
	 * chip->ccadc_gain_uv will be the actual voltage
	 * measured for 25000UV
	 */
	chip->ccadc_gain_uv = ccadc_reading_to_microvolt(chip,
				((s64)result_gain - result_offset));

	pr_debug("gain result_gain = 0x%x, voltage = %d microVolts\n",
							result_gain,
							chip->ccadc_gain_uv);
	/* Sanity Check */
	if (chip->ccadc_gain_uv > CCADC_MAX_25MV) {
		pr_err("gain voltage = %d is huge limiting to %d\n",
					chip->ccadc_gain_uv, CCADC_MAX_25MV);
		chip->ccadc_gain_uv = CCADC_MAX_25MV;
		result_gain = result_offset +
			microvolt_to_ccadc_reading(chip, CCADC_MAX_25MV);
	} else if (chip->ccadc_gain_uv < CCADC_MIN_25MV) {
		pr_err("gain voltage = %d is too low limiting to %d\n",
					chip->ccadc_gain_uv, CCADC_MIN_25MV);
		chip->ccadc_gain_uv = CCADC_MIN_25MV;
		result_gain = result_offset +
			microvolt_to_ccadc_reading(chip, CCADC_MIN_25MV);
	}

	data_msb = result_gain >> 8;
	data_lsb = result_gain;
	rc = calib_ccadc_program_trim(chip, CCADC_FULLSCALE_TRIM1,
						data_msb, data_lsb, 0);
	if (rc)
		pr_debug("error = %d programming gain trim\n", rc);
bail:
	pm8xxx_writeb(chip->dev->parent, ADC_ARB_SECP_CNTRL, sec_cntrl);
}

static void calibrate_ccadc_work(struct work_struct *work)
{
	struct pm8921_bms_chip *chip = container_of(work,
				struct pm8921_bms_chip, calib_ccadc_work.work);

	calib_ccadc(chip);
	schedule_delayed_work(&chip->calib_ccadc_work,
			round_jiffies_relative(msecs_to_jiffies
			(chip->calib_delay_ms)));
}

int pm8921_bms_get_vsense_avg(int *result)
{
	int rc = -EINVAL;
	unsigned long flags;

	if (the_chip) {
		spin_lock_irqsave(&the_chip->bms_output_lock, flags);
		pm_bms_lock_output_data(the_chip);
		rc = read_vsense_avg(the_chip, result);
		pm_bms_unlock_output_data(the_chip);
		spin_unlock_irqrestore(&the_chip->bms_output_lock, flags);
	}

	pr_err("called before initialization\n");
	return rc;
}
EXPORT_SYMBOL(pm8921_bms_get_vsense_avg);

int pm8921_bms_get_battery_current(int *result)
{
	unsigned long flags;
	int vsense;

	if (!the_chip) {
		pr_err("called before initialization\n");
		return -EINVAL;
	}
	if (the_chip->r_sense == 0) {
		pr_err("r_sense is zero\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&the_chip->bms_output_lock, flags);
	pm_bms_lock_output_data(the_chip);
	read_vsense_avg(the_chip, &vsense);
	pm_bms_unlock_output_data(the_chip);
	spin_unlock_irqrestore(&the_chip->bms_output_lock, flags);
	pr_debug("vsense=%d\n", vsense);
	/* cast for signed division */
	*result = vsense / (int)the_chip->r_sense;

	return 0;
}
EXPORT_SYMBOL(pm8921_bms_get_battery_current);

int pm8921_bms_get_percent_charge(void)
{
	int batt_temp, rc;
	struct pm8921_adc_chan_result result;

	if (!the_chip) {
		pr_err("called before initialization\n");
		return -EINVAL;
	}

	rc = pm8921_adc_read(the_chip->batt_temp_channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					the_chip->batt_temp_channel, rc);
		return rc;
	}
	pr_debug("batt_temp phy = %lld meas = 0x%llx", result.physical,
						result.measurement);
	batt_temp = (int)result.physical;
	return calculate_state_of_charge(the_chip,
					batt_temp, last_chargecycles);
}
EXPORT_SYMBOL_GPL(pm8921_bms_get_percent_charge);

int pm8921_bms_get_fcc(void)
{
	int batt_temp, rc;
	struct pm8921_adc_chan_result result;

	if (!the_chip) {
		pr_err("called before initialization\n");
		return -EINVAL;
	}

	rc = pm8921_adc_read(the_chip->batt_temp_channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					the_chip->batt_temp_channel, rc);
		return rc;
	}
	pr_debug("batt_temp phy = %lld meas = 0x%llx", result.physical,
						result.measurement);
	batt_temp = (int)result.physical;
	return calculate_fcc(the_chip, batt_temp, last_chargecycles);
}
EXPORT_SYMBOL_GPL(pm8921_bms_get_fcc);

void pm8921_bms_charging_began(void)
{
	the_chip->start_percent = pm8921_bms_get_percent_charge();
	pr_debug("start_percent = %u%%\n", the_chip->start_percent);
}
EXPORT_SYMBOL_GPL(pm8921_bms_charging_began);

void pm8921_bms_charging_end(int is_battery_full)
{
	if (is_battery_full && the_chip != NULL) {
		unsigned long flags;
		int batt_temp, rc, cc_reading;
		struct pm8921_adc_chan_result result;

		rc = pm8921_adc_read(the_chip->batt_temp_channel, &result);
		if (rc) {
			pr_err("error reading adc channel = %d, rc = %d\n",
					the_chip->batt_temp_channel, rc);
			goto charge_cycle_calculation;
		}
		pr_debug("batt_temp phy = %lld meas = 0x%llx", result.physical,
							result.measurement);
		batt_temp = (int)result.physical;
		last_real_fcc = calculate_real_fcc(the_chip,
						batt_temp, last_chargecycles);
		last_real_fcc_batt_temp = batt_temp;
		readjust_fcc_table();

		spin_lock_irqsave(&the_chip->bms_output_lock, flags);
		pm_bms_lock_output_data(the_chip);
		pm_bms_read_output_data(the_chip, LAST_GOOD_OCV_VALUE,
						&the_chip->ocv_reading_at_100);
		read_cc(the_chip, &cc_reading);
		pm_bms_unlock_output_data(the_chip);
		spin_unlock_irqrestore(&the_chip->bms_output_lock, flags);
		the_chip->cc_reading_at_100 = cc_reading;
		pr_debug("EOC ocv_reading = 0x%x cc_reading = %d\n",
				the_chip->ocv_reading_at_100,
				the_chip->cc_reading_at_100);
	}

charge_cycle_calculation:
	the_chip->end_percent = pm8921_bms_get_percent_charge();
	if (the_chip->end_percent > the_chip->start_percent) {
		last_charge_increase =
			the_chip->end_percent - the_chip->start_percent;
		if (last_charge_increase > 100) {
			last_chargecycles++;
			last_charge_increase = last_charge_increase % 100;
		}
	}
	pr_debug("end_percent = %u%% last_charge_increase = %d"
			"last_chargecycles = %d\n",
			the_chip->end_percent,
			last_charge_increase,
			last_chargecycles);
	the_chip->start_percent = 0;
	the_chip->end_percent = 0;
}
EXPORT_SYMBOL_GPL(pm8921_bms_charging_end);

static irqreturn_t pm8921_bms_sbi_write_ok_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_cc_thr_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_vsense_thr_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_vsense_for_r_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_ocv_for_r_handler(int irq, void *data)
{
	struct pm8921_bms_chip *chip = data;

	pr_debug("irq = %d triggered", irq);
	schedule_work(&chip->calib_hkadc_work);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_good_ocv_handler(int irq, void *data)
{
	struct pm8921_bms_chip *chip = data;

	pr_debug("irq = %d triggered", irq);
	schedule_work(&chip->calib_hkadc_work);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_vsense_avg_handler(int irq, void *data)
{
	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_ccadc_eoc_handler(int irq, void *data)
{
	u8 data_msb, data_lsb;
	struct pm8921_bms_chip *chip = data;
	int rc;

	pr_debug("irq = %d triggered\n", irq);
	data_msb = chip->ccadc_result_offset >> 8;
	data_lsb = chip->ccadc_result_offset;

	rc = calib_ccadc_program_trim(chip, CCADC_OFFSET_TRIM1,
						data_msb, data_lsb, 0);
	pm8921_bms_disable_irq(chip, PM8921_BMS_CCADC_EOC);

	return IRQ_HANDLED;
}

struct pm_bms_irq_init_data {
	unsigned int	irq_id;
	char		*name;
	unsigned long	flags;
	irqreturn_t	(*handler)(int, void *);
};

#define BMS_IRQ(_id, _flags, _handler) \
{ \
	.irq_id		= _id, \
	.name		= #_id, \
	.flags		= _flags, \
	.handler	= _handler, \
}

struct pm_bms_irq_init_data bms_irq_data[] = {
	BMS_IRQ(PM8921_BMS_SBI_WRITE_OK, IRQF_TRIGGER_RISING,
				pm8921_bms_sbi_write_ok_handler),
	BMS_IRQ(PM8921_BMS_CC_THR, IRQF_TRIGGER_RISING,
				pm8921_bms_cc_thr_handler),
	BMS_IRQ(PM8921_BMS_VSENSE_THR, IRQF_TRIGGER_RISING,
				pm8921_bms_vsense_thr_handler),
	BMS_IRQ(PM8921_BMS_VSENSE_FOR_R, IRQF_TRIGGER_RISING,
				pm8921_bms_vsense_for_r_handler),
	BMS_IRQ(PM8921_BMS_OCV_FOR_R, IRQF_TRIGGER_RISING,
				pm8921_bms_ocv_for_r_handler),
	BMS_IRQ(PM8921_BMS_GOOD_OCV, IRQF_TRIGGER_RISING,
				pm8921_bms_good_ocv_handler),
	BMS_IRQ(PM8921_BMS_VSENSE_AVG, IRQF_TRIGGER_RISING,
				pm8921_bms_vsense_avg_handler),
	BMS_IRQ(PM8921_BMS_CCADC_EOC, IRQF_TRIGGER_RISING,
				pm8921_bms_ccadc_eoc_handler),
};

static void free_irqs(struct pm8921_bms_chip *chip)
{
	int i;

	for (i = 0; i < PM_BMS_MAX_INTS; i++)
		if (chip->pmic_bms_irq[i]) {
			free_irq(chip->pmic_bms_irq[i], NULL);
			chip->pmic_bms_irq[i] = 0;
		}
}

static int __devinit request_irqs(struct pm8921_bms_chip *chip,
					struct platform_device *pdev)
{
	struct resource *res;
	int ret, i;

	ret = 0;
	bitmap_fill(chip->enabled_irqs, PM_BMS_MAX_INTS);

	for (i = 0; i < ARRAY_SIZE(bms_irq_data); i++) {
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
				bms_irq_data[i].name);
		if (res == NULL) {
			pr_err("couldn't find %s\n", bms_irq_data[i].name);
			goto err_out;
		}
		ret = request_irq(res->start, bms_irq_data[i].handler,
			bms_irq_data[i].flags,
			bms_irq_data[i].name, chip);
		if (ret < 0) {
			pr_err("couldn't request %d (%s) %d\n", res->start,
					bms_irq_data[i].name, ret);
			goto err_out;
		}
		chip->pmic_bms_irq[bms_irq_data[i].irq_id] = res->start;
		pm8921_bms_disable_irq(chip, bms_irq_data[i].irq_id);
	}
	return 0;

err_out:
	free_irqs(chip);
	return -EINVAL;
}

#define EN_BMS_BIT	BIT(7)
#define EN_PON_HS_BIT	BIT(0)
static int __devinit pm8921_bms_hw_init(struct pm8921_bms_chip *chip)
{
	int rc;

	rc = pm_bms_masked_write(chip, BMS_CONTROL,
			EN_BMS_BIT | EN_PON_HS_BIT, EN_BMS_BIT | EN_PON_HS_BIT);
	if (rc) {
		pr_err("failed to enable pon and bms addr = %d %d",
				BMS_CONTROL, rc);
	}

	return 0;
}

static void check_initial_ocv(struct pm8921_bms_chip *chip)
{
	int ocv, rc;

	/*
	 * Check if a ocv is available in bms hw,
	 * if not compute it here at boot time and save it
	 * in the last_ocv_uv.
	 */
	ocv = 0;
	rc = read_last_good_ocv(chip, &ocv);
	if (rc || ocv == 0) {
		rc = adc_based_ocv(chip, &ocv);
		if (rc) {
			pr_err("failed to read adc based ocv rc = %d\n", rc);
			ocv = DEFAULT_OCV_MICROVOLTS;
		}
		last_ocv_uv = ocv;
	}
	pr_debug("ocv = %d last_ocv_uv = %d\n", ocv, last_ocv_uv);
}

static int64_t read_battery_id(struct pm8921_bms_chip *chip)
{
	int rc;
	struct pm8921_adc_chan_result result;

	rc = pm8921_adc_read(chip->batt_id_channel, &result);
	if (rc) {
		pr_err("error reading batt id channel = %d, rc = %d\n",
					chip->vbat_channel, rc);
		return rc;
	}
	pr_debug("batt_id phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	return result.physical;
}

#define PALLADIUM_ID_MIN  2500
#define PALLADIUM_ID_MAX  4000
static int set_battery_data(struct pm8921_bms_chip *chip)
{
	int64_t battery_id;

	battery_id = read_battery_id(chip);

	if (battery_id < 0) {
		pr_err("cannot read battery id err = %lld\n", battery_id);
		return battery_id;
	}

	if (is_between(PALLADIUM_ID_MIN, PALLADIUM_ID_MAX, battery_id)) {
		chip->fcc = palladium_1500_data.fcc;
		chip->fcc_temp_lut = palladium_1500_data.fcc_temp_lut;
		chip->fcc_sf_lut = palladium_1500_data.fcc_sf_lut;
		chip->pc_temp_ocv_lut = palladium_1500_data.pc_temp_ocv_lut;
		chip->pc_sf_lut = palladium_1500_data.pc_sf_lut;
		return 0;
	} else {
		pr_warn("invalid battery id, palladium 1500 assumed\n");
		chip->fcc = palladium_1500_data.fcc;
		chip->fcc_temp_lut = palladium_1500_data.fcc_temp_lut;
		chip->fcc_sf_lut = palladium_1500_data.fcc_sf_lut;
		chip->pc_temp_ocv_lut = palladium_1500_data.pc_temp_ocv_lut;
		chip->pc_sf_lut = palladium_1500_data.pc_sf_lut;
		return 0;
	}
}

enum {
	CALC_RBATT,
	CALC_FCC,
	CALC_PC,
	CALC_SOC,
	CALIB_HKADC,
	CALIB_CCADC,
};

static int test_batt_temp = 5;
static int test_chargecycle = 150;
static int test_ocv = 3900000;
enum {
	TEST_BATT_TEMP,
	TEST_CHARGE_CYCLE,
	TEST_OCV,
};
static int get_test_param(void *data, u64 * val)
{
	switch ((int)data) {
	case TEST_BATT_TEMP:
		*val = test_batt_temp;
		break;
	case TEST_CHARGE_CYCLE:
		*val = test_chargecycle;
		break;
	case TEST_OCV:
		*val = test_ocv;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
static int set_test_param(void *data, u64  val)
{
	switch ((int)data) {
	case TEST_BATT_TEMP:
		test_batt_temp = (int)val;
		break;
	case TEST_CHARGE_CYCLE:
		test_chargecycle = (int)val;
		break;
	case TEST_OCV:
		test_ocv = (int)val;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(temp_fops, get_test_param, set_test_param, "%llu\n");

static int get_calc(void *data, u64 * val)
{
	int param = (int)data;
	int ret = 0;

	*val = 0;

	/* global irq number passed in via data */
	switch (param) {
	case CALC_RBATT:
		*val = calculate_rbatt(the_chip);
		break;
	case CALC_FCC:
		*val = calculate_fcc(the_chip, test_batt_temp,
							test_chargecycle);
		break;
	case CALC_PC:
		*val = calculate_pc(the_chip, test_ocv, test_batt_temp,
							test_chargecycle);
		break;
	case CALC_SOC:
		*val = calculate_state_of_charge(the_chip,
					test_batt_temp, test_chargecycle);
		break;
	case CALIB_HKADC:
		/* reading this will trigger calibration */
		*val = 0;
		calib_hkadc(the_chip);
		break;
	case CALIB_CCADC:
		/* reading this will trigger calibration */
		*val = 0;
		calib_ccadc(the_chip);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(calc_fops, get_calc, NULL, "%llu\n");

static int get_reading(void *data, u64 * val)
{
	int param = (int)data;
	int ret = 0;

	*val = 0;

	/* global irq number passed in via data */
	switch (param) {
	case CC_MSB:
	case CC_LSB:
		read_cc(the_chip, (int *)val);
		break;
	case LAST_GOOD_OCV_VALUE:
		read_last_good_ocv(the_chip, (uint *)val);
		break;
	case VBATT_FOR_RBATT:
		read_vbatt_for_rbatt(the_chip, (uint *)val);
		break;
	case VSENSE_FOR_RBATT:
		read_vsense_for_rbatt(the_chip, (uint *)val);
		break;
	case OCV_FOR_RBATT:
		read_ocv_for_rbatt(the_chip, (uint *)val);
		break;
	case VSENSE_AVG:
		read_vsense_avg(the_chip, (uint *)val);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(reading_fops, get_reading, NULL, "%lld\n");

static int get_rt_status(void *data, u64 * val)
{
	int i = (int)data;
	int ret;

	/* global irq number passed in via data */
	ret = pm_bms_get_rt_status(the_chip, i);
	*val = ret;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rt_fops, get_rt_status, NULL, "%llu\n");

static int get_reg(void *data, u64 * val)
{
	int addr = (int)data;
	int ret;
	u8 temp;

	ret = pm8xxx_readb(the_chip->dev->parent, addr, &temp);
	if (ret) {
		pr_err("pm8xxx_readb to %x value = %d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}
	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	int addr = (int)data;
	int ret;
	u8 temp;

	temp = (u8) val;
	ret = pm8xxx_writeb(the_chip->dev->parent, addr, temp);
	if (ret) {
		pr_err("pm8xxx_writeb to %x value = %d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

static void create_debugfs_entries(struct pm8921_bms_chip *chip)
{
	int i;

	chip->dent = debugfs_create_dir("pm8921_bms", NULL);

	if (IS_ERR(chip->dent)) {
		pr_err("pmic bms couldnt create debugfs dir\n");
		return;
	}

	debugfs_create_file("BMS_CONTROL", 0644, chip->dent,
			(void *)BMS_CONTROL, &reg_fops);
	debugfs_create_file("BMS_OUTPUT0", 0644, chip->dent,
			(void *)BMS_OUTPUT0, &reg_fops);
	debugfs_create_file("BMS_OUTPUT1", 0644, chip->dent,
			(void *)BMS_OUTPUT1, &reg_fops);
	debugfs_create_file("BMS_TEST1", 0644, chip->dent,
			(void *)BMS_TEST1, &reg_fops);
	debugfs_create_file("CCADC_ANA_PARAM", 0644, chip->dent,
			(void *)CCADC_ANA_PARAM, &reg_fops);
	debugfs_create_file("CCADC_DIG_PARAM", 0644, chip->dent,
			(void *)CCADC_DIG_PARAM, &reg_fops);
	debugfs_create_file("CCADC_RSV", 0644, chip->dent,
			(void *)CCADC_RSV, &reg_fops);
	debugfs_create_file("CCADC_DATA0", 0644, chip->dent,
			(void *)CCADC_DATA0, &reg_fops);
	debugfs_create_file("CCADC_DATA1", 0644, chip->dent,
			(void *)CCADC_DATA1, &reg_fops);
	debugfs_create_file("CCADC_OFFSET_TRIM1", 0644, chip->dent,
			(void *)CCADC_OFFSET_TRIM1, &reg_fops);
	debugfs_create_file("CCADC_OFFSET_TRIM0", 0644, chip->dent,
			(void *)CCADC_OFFSET_TRIM0, &reg_fops);

	debugfs_create_file("test_batt_temp", 0644, chip->dent,
				(void *)TEST_BATT_TEMP, &temp_fops);
	debugfs_create_file("test_chargecycle", 0644, chip->dent,
				(void *)TEST_CHARGE_CYCLE, &temp_fops);
	debugfs_create_file("test_ocv", 0644, chip->dent,
				(void *)TEST_OCV, &temp_fops);

	debugfs_create_file("read_cc", 0644, chip->dent,
				(void *)CC_MSB, &reading_fops);
	debugfs_create_file("read_last_good_ocv", 0644, chip->dent,
				(void *)LAST_GOOD_OCV_VALUE, &reading_fops);
	debugfs_create_file("read_vbatt_for_rbatt", 0644, chip->dent,
				(void *)VBATT_FOR_RBATT, &reading_fops);
	debugfs_create_file("read_vsense_for_rbatt", 0644, chip->dent,
				(void *)VSENSE_FOR_RBATT, &reading_fops);
	debugfs_create_file("read_ocv_for_rbatt", 0644, chip->dent,
				(void *)OCV_FOR_RBATT, &reading_fops);
	debugfs_create_file("read_vsense_avg", 0644, chip->dent,
				(void *)VSENSE_AVG, &reading_fops);

	debugfs_create_file("show_rbatt", 0644, chip->dent,
				(void *)CALC_RBATT, &calc_fops);
	debugfs_create_file("show_fcc", 0644, chip->dent,
				(void *)CALC_FCC, &calc_fops);
	debugfs_create_file("show_pc", 0644, chip->dent,
				(void *)CALC_PC, &calc_fops);
	debugfs_create_file("show_soc", 0644, chip->dent,
				(void *)CALC_SOC, &calc_fops);
	debugfs_create_file("calib_hkadc", 0644, chip->dent,
				(void *)CALIB_HKADC, &calc_fops);
	debugfs_create_file("calib_ccadc", 0644, chip->dent,
				(void *)CALIB_CCADC, &calc_fops);

	for (i = 0; i < ARRAY_SIZE(bms_irq_data); i++) {
		if (chip->pmic_bms_irq[bms_irq_data[i].irq_id])
			debugfs_create_file(bms_irq_data[i].name, 0444,
				chip->dent,
				(void *)bms_irq_data[i].irq_id,
				&rt_fops);
	}
}

static int __devinit pm8921_bms_probe(struct platform_device *pdev)
{
	int rc = 0;
	int vbatt;
	struct pm8921_bms_chip *chip;
	const struct pm8921_bms_platform_data *pdata
				= pdev->dev.platform_data;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(struct pm8921_bms_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("Cannot allocate pm_bms_chip\n");
		return -ENOMEM;
	}
	spin_lock_init(&chip->bms_output_lock);
	chip->dev = &pdev->dev;
	chip->r_sense = pdata->r_sense;
	chip->i_test = pdata->i_test;
	chip->v_failure = pdata->v_failure;
	chip->calib_delay_ms = pdata->calib_delay_ms;
	rc = set_battery_data(chip);
	if (rc) {
		pr_err("%s bad battery data %d\n", __func__, rc);
		goto free_chip;
	}

	chip->batt_temp_channel = pdata->bms_cdata.batt_temp_channel;
	chip->vbat_channel = pdata->bms_cdata.vbat_channel;
	chip->ref625mv_channel = pdata->bms_cdata.ref625mv_channel;
	chip->ref1p25v_channel = pdata->bms_cdata.ref1p25v_channel;
	chip->batt_id_channel = pdata->bms_cdata.batt_id_channel;
	chip->revision = pm8xxx_get_revision(chip->dev->parent);
	INIT_WORK(&chip->calib_hkadc_work, calibrate_hkadc_work);

	rc = request_irqs(chip, pdev);
	if (rc) {
		pr_err("couldn't register interrupts rc = %d\n", rc);
		goto free_chip;
	}

	rc = pm8921_bms_hw_init(chip);
	if (rc) {
		pr_err("couldn't init hardware rc = %d\n", rc);
		goto free_irqs;
	}

	platform_set_drvdata(pdev, chip);
	the_chip = chip;
	create_debugfs_entries(chip);

	check_initial_ocv(chip);
	chip->ccadc_gain_uv = calib_ccadc_read_gain_uv(chip);

	INIT_DELAYED_WORK(&chip->calib_ccadc_work, calibrate_ccadc_work);
	/* begin calibration only on chips > 2.0 */
	if (chip->revision >= PM8XXX_REVISION_8921_2p0)
		calibrate_ccadc_work(&(chip->calib_ccadc_work.work));

	/* initial hkadc calibration */
	schedule_work(&chip->calib_hkadc_work);
	/* enable the vbatt reading interrupts for scheduling hkadc calib */
	pm8921_bms_enable_irq(chip, PM8921_BMS_GOOD_OCV);
	pm8921_bms_enable_irq(chip, PM8921_BMS_OCV_FOR_R);

	get_battery_uvolts(chip, &vbatt);
	pr_info("OK battery_capacity_at_boot=%d volt = %d ocv = %d\n",
				pm8921_bms_get_percent_charge(),
				vbatt, last_ocv_uv);
	return 0;

free_irqs:
	free_irqs(chip);
free_chip:
	kfree(chip);
	return rc;
}

static int __devexit pm8921_bms_remove(struct platform_device *pdev)
{
	struct pm8921_bms_chip *chip = platform_get_drvdata(pdev);

	free_irqs(chip);
	kfree(chip->adjusted_fcc_temp_lut);
	platform_set_drvdata(pdev, NULL);
	the_chip = NULL;
	kfree(chip);
	return 0;
}

static struct platform_driver pm8921_bms_driver = {
	.probe	= pm8921_bms_probe,
	.remove	= __devexit_p(pm8921_bms_remove),
	.driver	= {
		.name	= PM8921_BMS_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8921_bms_init(void)
{
	return platform_driver_register(&pm8921_bms_driver);
}

static void __exit pm8921_bms_exit(void)
{
	platform_driver_unregister(&pm8921_bms_driver);
}

late_initcall(pm8921_bms_init);
module_exit(pm8921_bms_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8921 bms driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8921_BMS_DEV_NAME);
