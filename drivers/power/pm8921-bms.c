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
#include <linux/mfd/pm8921-adc.h>
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

#define ADC_ARB_SECP_CNTRL 0x190
#define ADC_ARB_SECP_AMUX_CNTRL 0x191
#define ADC_ARB_SECP_ANA_PARAM 0x192
#define ADC_ARB_SECP_RSV 0x194
#define ADC_ARB_SECP_DATA1 0x195
#define ADC_ARB_SECP_DATA0 0x196

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
	struct delayed_work	calib_work;
	unsigned int		calib_delay_ms;
	unsigned int		revision;
	unsigned int		batt_temp_channel;
	unsigned int		vbat_channel;
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

static int last_chargecycles = DEFAULT_CHARGE_CYCLES;
static int last_charge_increase;

module_param(last_rbatt, int, 0644);
module_param(last_ocv_uv, int, 0644);
module_param(last_chargecycles, int, 0644);
module_param(last_charge_increase, int, 0644);

static int pm_bms_get_rt_status(struct pm8921_bms_chip *chip, int irq_id)
{
	return pm8xxx_read_irq_stat(chip->dev->parent,
					chip->pmic_bms_irq[irq_id]);
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
	int i, scalefactorrow1, scalefactorrow2, scalefactor, row1, row2;
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

static int calculate_state_of_charge(struct pm8921_bms_chip *chip,
						int batt_temp, int chargecycles)
{
	int remaining_usable_charge, fcc, unusable_charge;
	int remaining_charge, soc, coulumb_counter;
	int update_userspace = 1;
	int64_t cc_mah;

	fcc = calculate_fcc(chip, batt_temp, chargecycles);
	pr_debug("FCC = %umAh batt_temp = %d, cycles = %d",
					fcc, batt_temp, chargecycles);

	unusable_charge = calculate_unusable_charge_mah(chip, fcc,
						batt_temp, chargecycles);

	pr_debug("UUC = %umAh", unusable_charge);

	/* calculate remainging charge */
	remaining_charge = calculate_remaining_charge_mah(chip, fcc, batt_temp,
								chargecycles);
	pr_debug("RC = %umAh\n", remaining_charge);

	/* calculate cc milli_volt_hour */
	calculate_cc_mah(chip, &cc_mah, &coulumb_counter);
	pr_debug("cc_mah = %lldmAh cc = %d\n", cc_mah, coulumb_counter);

	/* calculate remaining usable charge */
	remaining_usable_charge = remaining_charge - cc_mah - unusable_charge;
	pr_debug("RUC = %dmAh\n", remaining_usable_charge);
	if (remaining_usable_charge < 0) {
		pr_err("bad rem_usb_chg cc_mah %lld, rem_chg %d unusb_chg %d\n",
				cc_mah, remaining_charge, unusable_charge);
		update_userspace = 0;
	}

	soc = (remaining_usable_charge * 100) / (fcc - unusable_charge);
	if (soc > 100 || soc < 0) {
		pr_err("bad soc rem_usb_chg %d fcc %d unusb_chg %d\n",
				remaining_usable_charge, fcc, unusable_charge);
		update_userspace = 0;
	}
	pr_debug("SOC = %u%%\n", soc);

	if (update_userspace) {
		last_soc = soc;
	}
	return soc;
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
	*result = *result / the_chip->r_sense;
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

static int start_percent;
static int end_percent;
void pm8921_bms_charging_began(void)
{
	start_percent = pm8921_bms_get_percent_charge();
	pr_debug("start_percent = %u%%\n", start_percent);
}
EXPORT_SYMBOL_GPL(pm8921_bms_charging_began);

void pm8921_bms_charging_end(void)
{
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

	pr_debug("irq = %d triggered", irq);
	return IRQ_HANDLED;
}

static irqreturn_t pm8921_bms_good_ocv_handler(int irq, void *data)
{

	pr_debug("irq = %d triggered", irq);
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
	int ocv, vbatt, rbatt, ibatt, rc;

	/*
	 * Check if a last_good_ocv is available,
	 * if not compute it here at boot time.
	 */
	rc = read_last_good_ocv(chip, &ocv);
	if (rc || ocv == 0) {
		rc = get_battery_uvolts(chip, &vbatt);
		if (rc) {
			pr_err("failed to read vbatt from adc rc = %d\n", rc);
			last_ocv_uv = DEFAULT_OCV_MICROVOLTS;
			return;
		}

		rc =  pm8921_bms_get_battery_current(&ibatt);
		if (rc) {
			pr_err("failed to read batt current rc = %d\n", rc);
			last_ocv_uv = DEFAULT_OCV_MICROVOLTS;
			return;
		}

		rbatt = calculate_rbatt(the_chip);
		if (rbatt < 0)
			rbatt = DEFAULT_RBATT_MOHMS;
		last_ocv_uv = vbatt + ibatt * rbatt;
	}
	pr_debug("ocv = %d last_ocv_uv = %d\n", ocv, last_ocv_uv);
}

enum {
	CALC_RBATT,
	CALC_FCC,
	CALC_PC,
	CALC_SOC,
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

	for (i = 0; i < ARRAY_SIZE(bms_irq_data); i++) {
		if (chip->pmic_bms_irq[bms_irq_data[i].irq_id])
			debugfs_create_file(bms_irq_data[i].name, 0444,
				chip->dent,
				(void *)bms_irq_data[i].irq_id,
				&rt_fops);
	}
}

static void calibrate_work(struct work_struct *work)
{
	/* TODO */
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
	chip->fcc = pdata->batt_data->fcc;

	chip->fcc_temp_lut = pdata->batt_data->fcc_temp_lut;
	chip->fcc_sf_lut = pdata->batt_data->fcc_sf_lut;
	chip->pc_temp_ocv_lut = pdata->batt_data->pc_temp_ocv_lut;
	chip->pc_sf_lut = pdata->batt_data->pc_sf_lut;

	chip->batt_temp_channel = pdata->bms_cdata.batt_temp_channel;
	chip->vbat_channel = pdata->bms_cdata.vbat_channel;
	chip->revision = pm8xxx_get_revision(chip->dev->parent);

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

	INIT_DELAYED_WORK(&chip->calib_work, calibrate_work);
	schedule_delayed_work(&chip->calib_work,
			round_jiffies_relative(msecs_to_jiffies
			(chip->calib_delay_ms)));
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
