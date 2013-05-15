/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/mfd/pm8xxx/ccadc.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/ioport.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/rtc.h>

#define CCADC_ANA_PARAM		0x240
#define CCADC_DIG_PARAM		0x241
#define CCADC_RSV		0x242
#define CCADC_DATA0		0x244
#define CCADC_DATA1		0x245
#define CCADC_OFFSET_TRIM1	0x34A
#define CCADC_OFFSET_TRIM0	0x34B
#define CCADC_FULLSCALE_TRIM1	0x34C
#define CCADC_FULLSCALE_TRIM0	0x34D

/* note : TRIM1 is the msb and TRIM0 is the lsb */
#define ADC_ARB_SECP_CNTRL	0x190
#define ADC_ARB_SECP_AMUX_CNTRL	0x191
#define ADC_ARB_SECP_ANA_PARAM	0x192
#define ADC_ARB_SECP_DIG_PARAM	0x193
#define ADC_ARB_SECP_RSV	0x194
#define ADC_ARB_SECP_DATA1	0x195
#define ADC_ARB_SECP_DATA0	0x196

#define ADC_ARB_BMS_CNTRL	0x18D

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

struct pm8xxx_ccadc_chip {
	struct device		*dev;
	struct dentry		*dent;
	unsigned int		batt_temp_channel;
	u16			ccadc_offset;
	int			ccadc_gain_uv;
	unsigned int		revision;
	unsigned int		calib_delay_ms;
	unsigned long		last_calib_time;
	int			last_calib_temp;
	int			eoc_irq;
	int			r_sense_uohm;
	struct delayed_work	calib_ccadc_work;
	struct mutex		calib_mutex;
	bool			periodic_wakeup;
};

static struct pm8xxx_ccadc_chip *the_chip;

#ifdef DEBUG
static s64 microvolt_to_ccadc_reading(struct pm8xxx_ccadc_chip *chip, s64 cc)
{
	return div_s64(uv * CCADC_READING_RESOLUTION_D,
				CCADC_READING_RESOLUTION_N);
}
#endif

static int cc_adjust_for_offset(u16 raw)
{
	/* this has the intrinsic offset */
	return (int)raw - the_chip->ccadc_offset;
}

#define GAIN_REFERENCE_UV 25000
/*
 * gain compensation for ccadc readings - common for vsense based and
 * couloumb counter based readings
 */
s64 pm8xxx_cc_adjust_for_gain(s64 uv)
{
	if (the_chip == NULL || the_chip->ccadc_gain_uv == 0)
		return uv;

	return div_s64(uv * GAIN_REFERENCE_UV, the_chip->ccadc_gain_uv);
}
EXPORT_SYMBOL(pm8xxx_cc_adjust_for_gain);

static int pm_ccadc_masked_write(struct pm8xxx_ccadc_chip *chip, u16 addr,
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

#define REG_SBI_CONFIG		0x04F
#define PAGE3_ENABLE_MASK	0x6
static int calib_ccadc_enable_trim_access(struct pm8xxx_ccadc_chip *chip,
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

static int calib_ccadc_restore_trim_access(struct pm8xxx_ccadc_chip *chip,
							u8 sbi_config)
{
	return pm8xxx_writeb(chip->dev->parent, REG_SBI_CONFIG, sbi_config);
}

static int calib_ccadc_enable_arbiter(struct pm8xxx_ccadc_chip *chip)
{
	int rc;

	/* enable Arbiter, must be sent twice */
	rc = pm_ccadc_masked_write(chip, ADC_ARB_SECP_CNTRL,
			SEL_CCADC_BIT | EN_ARB_BIT, SEL_CCADC_BIT | EN_ARB_BIT);
	if (rc < 0) {
		pr_err("error = %d enabling arbiter for offset\n", rc);
		return rc;
	}
	rc = pm_ccadc_masked_write(chip, ADC_ARB_SECP_CNTRL,
			SEL_CCADC_BIT | EN_ARB_BIT, SEL_CCADC_BIT | EN_ARB_BIT);
	if (rc < 0) {
		pr_err("error = %d writing ADC_ARB_SECP_CNTRL\n", rc);
		return rc;
	}
	return 0;
}

static int calib_start_conv(struct pm8xxx_ccadc_chip *chip,
					u16 *result)
{
	int rc, i;
	u8 data_msb, data_lsb, reg;

	/* Start conversion */
	rc = pm_ccadc_masked_write(chip, ADC_ARB_SECP_CNTRL,
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
			msleep(20);
		else
			break;
	}
	if (i == ADC_WAIT_COUNT) {
		pr_err("waited too long for offset eoc returning -EBUSY\n");
		return -EBUSY;
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

static int calib_ccadc_read_trim(struct pm8xxx_ccadc_chip *chip,
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

static void calib_ccadc_read_offset_and_gain(struct pm8xxx_ccadc_chip *chip,
						int *gain, u16 *offset)
{
	u8 data_msb;
	u8 data_lsb;
	int rc;

	rc = calib_ccadc_read_trim(chip, CCADC_FULLSCALE_TRIM1,
						&data_msb, &data_lsb);
	*gain = (data_msb << 8) | data_lsb;

	rc = calib_ccadc_read_trim(chip, CCADC_OFFSET_TRIM1,
						&data_msb, &data_lsb);
	*offset = (data_msb << 8) | data_lsb;

	pr_debug("raw gain trim = 0x%x offset trim =0x%x\n", *gain, *offset);
	*gain = pm8xxx_ccadc_reading_to_microvolt(chip->revision,
							(s64)*gain - *offset);
	pr_debug("gain uv = %duV offset=0x%x\n", *gain, *offset);
}

#define CCADC_PROGRAM_TRIM_COUNT	2
#define ADC_ARB_BMS_CNTRL_CCADC_SHIFT	4
#define ADC_ARB_BMS_CNTRL_CONV_MASK	0x03
#define BMS_CONV_IN_PROGRESS		0x2

static int calib_ccadc_program_trim(struct pm8xxx_ccadc_chip *chip,
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

static int get_batt_temp(struct pm8xxx_ccadc_chip *chip, int *batt_temp)
{
	int rc;
	struct pm8xxx_adc_chan_result result;

	rc = pm8xxx_adc_read(chip->batt_temp_channel, &result);
	if (rc) {
		pr_err("error reading batt_temp_channel = %d, rc = %d\n",
					chip->batt_temp_channel, rc);
		return rc;
	}
	*batt_temp = result.physical;
	pr_debug("batt_temp phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	return 0;
}

static int get_current_time(unsigned long *now_tm_sec)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int rc;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("%s: unable to open rtc device (%s)\n",
			__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
		return -EINVAL;
	}

	rc = rtc_read_time(rtc, &tm);
	if (rc) {
		pr_err("Error reading rtc device (%s) : %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		return rc;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		return rc;
	}
	rtc_tm_to_time(&tm, now_tm_sec);

	return 0;
}

static void __pm8xxx_calib_ccadc(int sample_count)
{
	u8 data_msb, data_lsb, sec_cntrl;
	int result_offset, result_gain;
	u16 result;
	int i, rc;

	if (!the_chip) {
		pr_err("chip not initialized\n");
		return;
	}

	pr_debug("sample_count = %d\n", sample_count);

	mutex_lock(&the_chip->calib_mutex);
	rc = pm8xxx_readb(the_chip->dev->parent,
					ADC_ARB_SECP_CNTRL, &sec_cntrl);
	if (rc < 0) {
		pr_err("error = %d reading ADC_ARB_SECP_CNTRL\n", rc);
		goto calibration_unlock;
	}

	rc = calib_ccadc_enable_arbiter(the_chip);
	if (rc < 0) {
		pr_err("error = %d enabling arbiter for offset\n", rc);
		goto bail;
	}

	/*
	 * Set decimation ratio to 4k, lower ratio may be used in order to speed
	 * up, pending verification through bench
	 */
	rc = pm8xxx_writeb(the_chip->dev->parent, ADC_ARB_SECP_DIG_PARAM,
							CCADC_CALIB_DIG_PARAM);
	if (rc < 0) {
		pr_err("error = %d writing ADC_ARB_SECP_DIG_PARAM\n", rc);
		goto bail;
	}

	result_offset = 0;
	for (i = 0; i < sample_count; i++) {
		/* Short analog inputs to CCADC internally to ground */
		rc = pm8xxx_writeb(the_chip->dev->parent, ADC_ARB_SECP_RSV,
							CCADC_CALIB_RSV_GND);
		if (rc < 0) {
			pr_err("error = %d selecting gnd voltage\n", rc);
			goto bail;
		}

		/* Enable CCADC */
		rc = pm8xxx_writeb(the_chip->dev->parent,
				ADC_ARB_SECP_ANA_PARAM, CCADC_CALIB_ANA_PARAM);
		if (rc < 0) {
			pr_err("error = %d enabling ccadc\n", rc);
			goto bail;
		}

		rc = calib_start_conv(the_chip, &result);
		if (rc < 0) {
			pr_err("error = %d for zero volt measurement\n", rc);
			goto bail;
		}

		result_offset += result;
	}

	result_offset = result_offset / sample_count;


	pr_debug("offset result_offset = 0x%x, voltage = %llduV\n",
			result_offset,
			pm8xxx_ccadc_reading_to_microvolt(the_chip->revision,
			((s64)result_offset - CCADC_INTRINSIC_OFFSET)));

	the_chip->ccadc_offset = result_offset;
	data_msb = the_chip->ccadc_offset >> 8;
	data_lsb = the_chip->ccadc_offset;

	rc = calib_ccadc_program_trim(the_chip, CCADC_OFFSET_TRIM1,
						data_msb, data_lsb, 1);
	if (rc) {
		pr_debug("error = %d programming offset trim 0x%02x 0x%02x\n",
					rc, data_msb, data_lsb);
		/* enable the interrupt and write it when it fires */
		enable_irq(the_chip->eoc_irq);
	}

	rc = calib_ccadc_enable_arbiter(the_chip);
	if (rc < 0) {
		pr_err("error = %d enabling arbiter for gain\n", rc);
		goto bail;
	}

	/*
	 * Set decimation ratio to 4k, lower ratio may be used in order to speed
	 * up, pending verification through bench
	 */
	rc = pm8xxx_writeb(the_chip->dev->parent, ADC_ARB_SECP_DIG_PARAM,
							CCADC_CALIB_DIG_PARAM);
	if (rc < 0) {
		pr_err("error = %d enabling decimation ration for gain\n", rc);
		goto bail;
	}

	result_gain = 0;
	for (i = 0; i < sample_count; i++) {
		rc = pm8xxx_writeb(the_chip->dev->parent,
					ADC_ARB_SECP_RSV, CCADC_CALIB_RSV_25MV);
		if (rc < 0) {
			pr_err("error = %d selecting 25mV for gain\n", rc);
			goto bail;
		}

		/* Enable CCADC */
		rc = pm8xxx_writeb(the_chip->dev->parent,
			ADC_ARB_SECP_ANA_PARAM, CCADC_CALIB_ANA_PARAM);
		if (rc < 0) {
			pr_err("error = %d enabling ccadc\n", rc);
			goto bail;
		}

		rc = calib_start_conv(the_chip, &result);
		if (rc < 0) {
			pr_err("error = %d for adc reading 25mV\n", rc);
			goto bail;
		}

		result_gain += result;
	}
	result_gain = result_gain / sample_count;

	/*
	 * result_offset includes INTRINSIC OFFSET
	 * the_chip->ccadc_gain_uv will be the actual voltage
	 * measured for 25000UV
	 */
	the_chip->ccadc_gain_uv = pm8xxx_ccadc_reading_to_microvolt(
				the_chip->revision,
				((s64)result_gain - result_offset));

	pr_debug("gain result_gain = 0x%x, voltage = %d microVolts\n",
					result_gain, the_chip->ccadc_gain_uv);

	data_msb = result_gain >> 8;
	data_lsb = result_gain;
	rc = calib_ccadc_program_trim(the_chip, CCADC_FULLSCALE_TRIM1,
						data_msb, data_lsb, 0);
	if (rc)
		pr_debug("error = %d programming gain trim\n", rc);
bail:
	pm8xxx_writeb(the_chip->dev->parent, ADC_ARB_SECP_CNTRL, sec_cntrl);
calibration_unlock:
	mutex_unlock(&the_chip->calib_mutex);
}

static void pm8xxx_calib_ccadc_quick(void)
{
	__pm8xxx_calib_ccadc(2);
}

void pm8xxx_calib_ccadc(void)
{
	__pm8xxx_calib_ccadc(SAMPLE_COUNT);
}
EXPORT_SYMBOL(pm8xxx_calib_ccadc);

static void calibrate_ccadc_work(struct work_struct *work)
{
	struct pm8xxx_ccadc_chip *chip = container_of(work,
			struct pm8xxx_ccadc_chip, calib_ccadc_work.work);

	pm8xxx_calib_ccadc();
	schedule_delayed_work(&chip->calib_ccadc_work,
			round_jiffies_relative(msecs_to_jiffies
			(chip->calib_delay_ms)));
}

static irqreturn_t pm8921_bms_ccadc_eoc_handler(int irq, void *data)
{
	u8 data_msb, data_lsb;
	struct pm8xxx_ccadc_chip *chip = data;
	int rc;

	if (!the_chip)
		goto out;

	pr_debug("irq = %d triggered\n", irq);
	data_msb = chip->ccadc_offset >> 8;
	data_lsb = chip->ccadc_offset;

	rc = calib_ccadc_program_trim(chip, CCADC_OFFSET_TRIM1,
						data_msb, data_lsb, 0);
	disable_irq_nosync(chip->eoc_irq);

out:
	return IRQ_HANDLED;
}

#define CCADC_IBAT_DIG_PARAM	0xA3
#define CCADC_IBAT_RSV		0x10
#define CCADC_IBAT_ANA_PARAM	0x1A
static int ccadc_get_rsense_voltage(int *voltage_uv)
{
	u16 raw;
	int result;
	int rc = 0;

	rc = calib_ccadc_enable_arbiter(the_chip);
	if (rc < 0) {
		pr_err("error = %d enabling arbiter for offset\n", rc);
		return rc;
	}

	rc = pm8xxx_writeb(the_chip->dev->parent, ADC_ARB_SECP_DIG_PARAM,
							CCADC_IBAT_DIG_PARAM);
	if (rc < 0) {
		pr_err("error = %d writing ADC_ARB_SECP_DIG_PARAM\n", rc);
		return rc;
	}

	rc = pm8xxx_writeb(the_chip->dev->parent, ADC_ARB_SECP_RSV,
						CCADC_IBAT_RSV);
	if (rc < 0) {
		pr_err("error = %d selecting rsense\n", rc);
		return rc;
	}

	rc = pm8xxx_writeb(the_chip->dev->parent,
			ADC_ARB_SECP_ANA_PARAM, CCADC_IBAT_ANA_PARAM);
	if (rc < 0) {
		pr_err("error = %d enabling ccadc\n", rc);
		return rc;
	}

	rc = calib_start_conv(the_chip, &raw);
	if (rc < 0) {
		pr_err("error = %d for zero volt measurement\n", rc);
		return rc;
	}

	pr_debug("Vsense raw = 0x%x\n", raw);
	result = cc_adjust_for_offset(raw);
	pr_debug("Vsense after offset raw = 0x%x offset=0x%x\n",
					result,
					the_chip->ccadc_offset);
	*voltage_uv = pm8xxx_ccadc_reading_to_microvolt(the_chip->revision,
			((s64)result));
	pr_debug("Vsense before gain of %d = %d uV\n", the_chip->ccadc_gain_uv,
					*voltage_uv);
	*voltage_uv = pm8xxx_cc_adjust_for_gain(*voltage_uv);

	pr_debug("Vsense = %d uV\n", *voltage_uv);
	return 0;
}

int pm8xxx_ccadc_get_battery_current(int *bat_current_ua)
{
	int voltage_uv = 0, rc;

	rc = ccadc_get_rsense_voltage(&voltage_uv);
	if (rc) {
		pr_err("cant get voltage across rsense rc = %d\n", rc);
		return rc;
	}

	*bat_current_ua = div_s64((s64)voltage_uv * 1000000LL,
						the_chip->r_sense_uohm);
	/*
	 * ccadc reads +ve current when the battery is charging
	 * We need to return -ve if the battery is charging
	 */
	*bat_current_ua = -1 * (*bat_current_ua);
	pr_debug("bat current = %d ma\n", *bat_current_ua);
	return 0;
}
EXPORT_SYMBOL(pm8xxx_ccadc_get_battery_current);

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

static int get_calc(void *data, u64 * val)
{
	int ibat, rc;

	rc = pm8xxx_ccadc_get_battery_current(&ibat);
	*val = ibat;
	return rc;
}
DEFINE_SIMPLE_ATTRIBUTE(calc_fops, get_calc, NULL, "%lld\n");

static void create_debugfs_entries(struct pm8xxx_ccadc_chip *chip)
{
	chip->dent = debugfs_create_dir("pm8xxx-ccadc", NULL);

	if (IS_ERR(chip->dent)) {
		pr_err("ccadc couldnt create debugfs dir\n");
		return;
	}

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
	debugfs_create_file("CCADC_FULLSCALE_TRIM1", 0644, chip->dent,
			(void *)CCADC_FULLSCALE_TRIM1, &reg_fops);
	debugfs_create_file("CCADC_FULLSCALE_TRIM0", 0644, chip->dent,
			(void *)CCADC_FULLSCALE_TRIM0, &reg_fops);

	debugfs_create_file("show_ibatt", 0644, chip->dent,
				(void *)0, &calc_fops);
}

static int __devinit pm8xxx_ccadc_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct pm8xxx_ccadc_chip *chip;
	struct resource *res;
	const struct pm8xxx_ccadc_platform_data *pdata
				= pdev->dev.platform_data;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}
	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"PM8921_BMS_CCADC_EOC");
	if  (!res) {
		pr_err("failed to get irq\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(struct pm8xxx_ccadc_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("Cannot allocate pm_bms_chip\n");
		return -ENOMEM;
	}
	chip->dev = &pdev->dev;
	chip->revision = pm8xxx_get_revision(chip->dev->parent);
	chip->eoc_irq = res->start;
	chip->r_sense_uohm = pdata->r_sense_uohm;
	chip->calib_delay_ms = pdata->calib_delay_ms;
	chip->batt_temp_channel = pdata->ccadc_cdata.batt_temp_channel;
	chip->periodic_wakeup = pdata->periodic_wakeup;
	mutex_init(&chip->calib_mutex);

	calib_ccadc_read_offset_and_gain(chip,
					&chip->ccadc_gain_uv,
					&chip->ccadc_offset);
	irq_set_status_flags(chip->eoc_irq, IRQ_NOAUTOEN);
	rc = request_irq(chip->eoc_irq,
			pm8921_bms_ccadc_eoc_handler, IRQF_TRIGGER_RISING,
			"bms_eoc_ccadc", chip);
	if (rc) {
		pr_err("failed to request %d irq rc= %d\n", chip->eoc_irq, rc);
		goto free_chip;
	}

	platform_set_drvdata(pdev, chip);
	the_chip = chip;
	INIT_DELAYED_WORK(&chip->calib_ccadc_work, calibrate_ccadc_work);
	schedule_delayed_work(&chip->calib_ccadc_work, 0);

	create_debugfs_entries(chip);

	return 0;

free_chip:
	mutex_destroy(&chip->calib_mutex);
	kfree(chip);
	return rc;
}

static int __devexit pm8xxx_ccadc_remove(struct platform_device *pdev)
{
	struct pm8xxx_ccadc_chip *chip = platform_get_drvdata(pdev);

	debugfs_remove_recursive(chip->dent);
	the_chip = NULL;
	kfree(chip);
	return 0;
}

static int pm8xxx_ccadc_suspend(struct device *dev)
{
	struct pm8xxx_ccadc_chip *chip = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&chip->calib_ccadc_work);

	return 0;
}

#define CCADC_CALIB_TEMP_THRESH 20
static int pm8xxx_ccadc_resume(struct device *dev)
{
	int rc, batt_temp, delta_temp;
	unsigned long current_time_sec;
	unsigned long time_since_last_calib;

	rc = get_batt_temp(the_chip, &batt_temp);
	if (rc) {
		pr_err("unable to get batt_temp: %d\n", rc);
		return 0;
	}
	rc = get_current_time(&current_time_sec);
	if (rc) {
		pr_err("unable to get current time: %d\n", rc);
		return 0;
	}

	if (the_chip->periodic_wakeup) {
		pm8xxx_calib_ccadc_quick();
		return 0;
	}

	if (current_time_sec > the_chip->last_calib_time) {
		time_since_last_calib = current_time_sec -
					the_chip->last_calib_time;
		delta_temp = abs(batt_temp - the_chip->last_calib_temp);
		pr_debug("time since last calib: %lu, delta_temp = %d\n",
					time_since_last_calib, delta_temp);
		if (time_since_last_calib >= the_chip->calib_delay_ms/1000
				|| delta_temp > CCADC_CALIB_TEMP_THRESH) {
			the_chip->last_calib_time = current_time_sec;
			the_chip->last_calib_temp = batt_temp;
			schedule_delayed_work(&the_chip->calib_ccadc_work, 0);
		} else {
			schedule_delayed_work(&the_chip->calib_ccadc_work,
				msecs_to_jiffies(the_chip->calib_delay_ms -
					(time_since_last_calib * 1000)));
		}
	}

	return 0;
}

static const struct dev_pm_ops pm8xxx_ccadc_pm_ops = {
	.suspend	= pm8xxx_ccadc_suspend,
	.resume		= pm8xxx_ccadc_resume,
};

static struct platform_driver pm8xxx_ccadc_driver = {
	.probe	= pm8xxx_ccadc_probe,
	.remove	= __devexit_p(pm8xxx_ccadc_remove),
	.driver	= {
		.name	= PM8XXX_CCADC_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &pm8xxx_ccadc_pm_ops,
	},
};

static int __init pm8xxx_ccadc_init(void)
{
	return platform_driver_register(&pm8xxx_ccadc_driver);
}

static void __exit pm8xxx_ccadc_exit(void)
{
	platform_driver_unregister(&pm8xxx_ccadc_driver);
}

module_init(pm8xxx_ccadc_init);
module_exit(pm8xxx_ccadc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8XXX ccadc driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8XXX_CCADC_DEV_NAME);
