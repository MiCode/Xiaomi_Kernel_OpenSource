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

#define ADC_ARB_SECP_CNTRL	0x190
#define ADC_ARB_SECP_AMUX_CNTRL	0x191
#define ADC_ARB_SECP_ANA_PARAM	0x192
#define ADC_ARB_SECP_RSV	0x194
#define ADC_ARB_SECP_DATA1	0x195
#define ADC_ARB_SECP_DATA0	0x196

enum pmic_bms_interrupts {
	PM8921_BMS_SBI_WRITE_OK,
	PM8921_BMS_CC_THR,
	PM8921_BMS_VSENSE_THR,
	PM8921_BMS_VSENSE_FOR_R,
	PM8921_BMS_OCV_FOR_R,
	PM8921_BMS_GOOD_OCV,
	PM8921_BMS_VSENSE_AVG,
	PM_BMS_MAX_INTS,
};

/**
 * struct pm8921_bms_chip -device information
 * @dev:	device pointer to access the parent
 * @dent:	debugfs directory
 * @r_sense:	batt sense resistance value
 * @i_test:	peak current
 * @v_failure:	battery dead voltage
 * @fcc:	battery capacity
 *
 */
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
	unsigned int		calib_delay_ms;
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
};

static struct pm8921_bms_chip *the_chip;

#define DEFAULT_RBATT_MOHMS		128
#define DEFAULT_UNUSABLE_CHARGE_MAH	10
#define DEFAULT_OCV_MICROVOLTS		3900000
#define DEFAULT_REMAINING_CHARGE_MAH	990
#define DEFAULT_COULUMB_COUNTER		0
#define DEFAULT_CHARGE_CYCLES		0

static int last_rbatt = -EINVAL;
static int last_ocv_uv = -EINVAL;
static int last_soc = -EINVAL;
static int last_real_fcc = -EINVAL;

static int last_chargecycles = DEFAULT_CHARGE_CYCLES;
static int last_charge_increase;

module_param(last_rbatt, int, 0644);
module_param(last_ocv_uv, int, 0644);
module_param(last_chargecycles, int, 0644);
module_param(last_charge_increase, int, 0644);
module_param(last_real_fcc, int, 0644);

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

#define V_PER_BIT_MUL_FACTOR	293
#define INTRINSIC_OFFSET	0x6000
static int vbatt_to_microvolt(unsigned int a)
{
	if (a <= INTRINSIC_OFFSET)
		return 0;

	return (a - INTRINSIC_OFFSET) * V_PER_BIT_MUL_FACTOR;
}

#define XOADC_CALIB_UV		625000
static int adjust_xo_reading(struct pm8921_bms_chip *chip, unsigned int uv)
{
	u64 numerator = ((u64)uv - chip->xoadc_v0625) * XOADC_CALIB_UV;
	u64 denominator =  chip->xoadc_v125 - chip->xoadc_v0625;

	if (denominator == 0)
		return uv;
	return XOADC_CALIB_UV + div_u64(numerator, denominator);
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
	return (chip->revision < PM8XXX_REVISION_8901_2p0) ?
				cc_to_microvolt_v1((s64)cc) :
				cc_to_microvolt_v2((s64)cc);
}

#define CC_READING_TICKS	55
#define SLEEP_CLK_HZ		32768
#define SECONDS_PER_HOUR	3600
static s64 ccmicrovolt_to_uvh(s64 cc_uv)
{
	return div_s64(cc_uv * CC_READING_TICKS,
			SLEEP_CLK_HZ * SECONDS_PER_HOUR);
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
	*result = vbatt_to_microvolt(reading);
	pr_debug("raw = %04x ocv_microV = %u\n", reading, *result);
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
	*result = vbatt_to_microvolt(reading);
	pr_debug("raw = %04x vbatt_for_r_microV = %u\n", reading, *result);
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
	*result = cc_to_microvolt(chip, reading);
	pr_debug("raw = %04x vsense_for_r_microV = %u\n", reading, *result);
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
	*result = vbatt_to_microvolt(reading);
	pr_debug("read = %04x ocv_for_r_microV = %u\n", reading, *result);
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
	*result = cc_to_microvolt(chip, reading);
	pr_debug("read = %04x vsense = %d\n", reading, *result);
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

static int interpolate_scalingfactor_fcc(struct pm8921_bms_chip *chip,
								int cycles)
{
	return interpolate_single_lut(chip->fcc_sf_lut, cycles);
}

static int interpolate_scalingfactor_pc(struct pm8921_bms_chip *chip,
				int cycles, int pc)
{
	int i, scalefactorrow1, scalefactorrow2, scalefactor;
	int row1 = 0;
	int row2 = 0;
	int rows = chip->pc_sf_lut->rows;
	int cols = chip->pc_sf_lut->cols;

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
	ocv = adjust_xo_reading(chip, ocv);

	rc = read_vbatt_for_rbatt(chip, &vbatt);
	if (rc) {
		pr_err("fail to read vbatt_for_rbatt rc = %d\n", rc);
		ocv = 0;
	}
	vbatt = adjust_xo_reading(chip, vbatt);

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
	pr_debug("r_batt = %umilliOhms", r_batt);
	return r_batt;
}

static int calculate_fcc(struct pm8921_bms_chip *chip, int batt_temp,
							int chargecycles)
{
	int initfcc, result, scalefactor = 0;

	initfcc = interpolate_fcc(chip, batt_temp);
	pr_debug("intfcc = %umAh batt_temp = %d\n", initfcc, batt_temp);

	scalefactor = interpolate_scalingfactor_fcc(chip, chargecycles);
	pr_debug("scalefactor = %d batt_temp = %d\n", scalefactor, batt_temp);

	/* Multiply the initial FCC value by the scale factor. */
	result = (initfcc * scalefactor) / 100;
	pr_debug("fcc mAh = %d\n", result);
	return result;
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
		last_ocv_uv = DEFAULT_OCV_MICROVOLTS;
		return rc;
	}

	rc =  pm8921_bms_get_battery_current(&ibatt);
	if (rc) {
		pr_err("failed to read batt current rc = %d\n", rc);
		last_ocv_uv = DEFAULT_OCV_MICROVOLTS;
		return rc;
	}

	rbatt = calculate_rbatt(the_chip);
	if (rbatt < 0)
		rbatt = DEFAULT_RBATT_MOHMS;
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
	} else {
		last_rbatt = rbatt;
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
	} else {
		/* update the usespace param since a good ocv is available */
		last_ocv_uv = ocv;
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

	*fcc = calculate_fcc(chip, batt_temp, chargecycles);
	pr_debug("FCC = %umAh batt_temp = %d, cycles = %d",
					*fcc, batt_temp, chargecycles);

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
			pr_debug("Adjusting SOC to %d\n",
						BATTERY_POWER_SUPPLY_SOC);
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

	if (update_userspace) {
		last_soc = soc;
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
	return phy * HKADC_V_PER_BIT_MUL_FACTOR / HKADC_V_PER_BIT_DIV_FACTOR;
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
	voltage = calib_hkadc_convert_microvolt(result.physical);

	pr_debug("result 1.25v = 0x%llx, voltage = %dmV adc_meas = %lld\n",
				result.physical, voltage, result.measurement);

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
	voltage = calib_hkadc_convert_microvolt(result.physical);
	pr_debug("result 0.625V = 0x%llx, voltage = %dmV adc_mead = %lld\n",
				result.physical, voltage, result.measurement);
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

int pm8921_bms_get_vsense_avg(int *result)
{
	if (the_chip)
		return read_vsense_avg(the_chip, result);

	pr_err("called before initialization\n");
	return -EINVAL;
}
EXPORT_SYMBOL(pm8921_bms_get_vsense_avg);

int pm8921_bms_get_battery_current(int *result)
{
	if (!the_chip) {
		pr_err("called before initialization\n");
		return -EINVAL;
	}
	if (the_chip->r_sense == 0) {
		pr_err("r_sense is zero\n");
		return -EINVAL;
	}

	read_vsense_avg(the_chip, result);
	pr_debug("vsense=%d\n", *result);
	/* cast for signed division */
	*result = *result / (int)the_chip->r_sense;
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

static int start_percent;
static int end_percent;
void pm8921_bms_charging_began(void)
{
	start_percent = pm8921_bms_get_percent_charge();
	pr_debug("start_percent = %u%%\n", start_percent);
}
EXPORT_SYMBOL_GPL(pm8921_bms_charging_began);

void pm8921_bms_charging_end(int is_battery_full)
{
	if (is_battery_full && the_chip != NULL) {
		int batt_temp, rc;
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
	}

charge_cycle_calculation:
	end_percent = pm8921_bms_get_percent_charge();
	if (end_percent > start_percent) {
		last_charge_increase = end_percent - start_percent;
		if (last_charge_increase > 100) {
			last_chargecycles++;
			last_charge_increase = last_charge_increase % 100;
		}
	}
	pr_debug("end_percent = %u%% last_charge_increase = %d"
			"last_chargecycles = %d\n",
			end_percent,
			last_charge_increase,
			last_chargecycles);
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
	 * Check if a last_good_ocv is available,
	 * if not compute it here at boot time.
	 */
	rc = read_last_good_ocv(chip, &ocv);
	if (rc || ocv == 0) {
		rc = adc_based_ocv(chip, &last_ocv_uv);
		pr_err("failed to read ocv from adc and bms rc = %d\n", rc);
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

	rc = pm8921_bms_hw_init(chip);
	if (rc) {
		pr_err("couldn't init hardware rc = %d\n", rc);
		goto free_chip;
	}

	rc = request_irqs(chip, pdev);
	if (rc) {
		pr_err("couldn't register interrupts rc = %d\n", rc);
		goto free_chip;
	}

	platform_set_drvdata(pdev, chip);
	the_chip = chip;
	create_debugfs_entries(chip);

	check_initial_ocv(chip);

	/* enable the vbatt reading interrupts for scheduling hkadc calib */
	pm8921_bms_enable_irq(chip, PM8921_BMS_GOOD_OCV);
	pm8921_bms_enable_irq(chip, PM8921_BMS_OCV_FOR_R);

	pr_info("OK battery_capacity_at_boot=%d\n",
				pm8921_bms_get_percent_charge());
	return 0;

free_chip:
	kfree(chip);
	return rc;
}

static int __devexit pm8921_bms_remove(struct platform_device *pdev)
{
	struct pm8921_bms_chip *chip = platform_get_drvdata(pdev);

	free_irqs(chip);
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
