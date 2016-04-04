/*
*  stc3117_battery.c
*  STC3117 fuel-gauge systems for lithium-ion (Li+) batteries
*
*  Copyright (C) 2011 STMicroelectronics.
*  Copyright (c) 2016 The Linux Foundation. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/
#define pr_fmt(fmt) "STC311x: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/power_supply.h>

#include <linux/qpnp/qpnp-adc.h>

#define MODE_REG		0x0
#define VMODE_BIT		BIT(0)
#define FORCE_CD_BIT		BIT(2)
#define ALM_EN_BIT		BIT(3)
#define GG_RUN_BIT		BIT(4)
#define FORCE_CC_BIT		BIT(5)
#define FORCE_VM_BIT		BIT(6)

#define CTRL_REG		0x1
#define IO0_DATA_BIT		BIT(0)
#define GG_RST_BIT		BIT(1)
#define GG_VM_BIT		BIT(2)
#define BATFAIL_BIT		BIT(3)
#define PORDET_BIT		BIT(4)
#define ALM_SOC_BIT		BIT(5)
#define ALM_VOLT_BIT		BIT(6)
#define UVLOD_BIT		BIT(7)
#define FG_RESET		(BATFAIL_BIT | UVLOD_BIT | PORDET_BIT)

#define SOC_REG			0x2
#define COUNTER_REG		0x4
#define CURRENT_REG		0x6
#define VOLTAGE_REG		0x8
#define TEMPERATURE_REG		0xa
#define AVG_CURRENT_REG		0xb
#define OCV_REG			0xd
#define CC_CNF_REG		0xf
#define VM_CNF_REG		0x11
#define ALM_SOC_REG		0x13
#define ALM_VOLTAGE_REG		0x14
#define RELAX_CURRENT_REG	0x15
#define ID_REG			0x18
#define CC_ADJ_REG		0x1b
#define VM_ADJ_REG		0x1c
#define RAM_BASE		0x20
#define RAM_SIZE		16
#define OCVTAB_REG		0x30
#define SOCTAB_REG		0x50
#define OCV_SOC_SIZE		16
#define SOCTAB_MULTIPLIER	2

#define STC3117_ID		0x16

#define CUTOFF_VOTAGE_MV	3400
#define RAM_TSTWORD		0xa5a5
#define GG_INIT			1
#define GG_RUNNING		2
#define VM_MODE			1
#define CC_MODE			0
#define NTEMP			7
#define MAX_SOC			1000
#define MAX_HRSOC		51200
#define SOC_FACTOR		512

/* 2's complement conversion macros */
#define TEMP_SIGN_BIT			7
#define AVG_CURRENT_SIGN_BIT		15
#define ADJ_SIGN_BIT			15
#define VOLTAGE_SIGN_BIT		11
#define CURRENT_SIGN_BIT		13
#define ALM_VOLTAGE_NUMERATOR		176L
#define ALM_VOLTAGE_DENOMINATOR		10L
#define RELAX_CURRENT_NUMERATOR		4704L
#define RELAX_CURRENT_DENOMINATOR	100L
#define OCV_NUMERATOR			55L
#define OCV_DENOMINATOR			100L
#define CURRENT_NUMERATOR		588L
#define CURRENT_DINOMINATOR		100L
#define AVG_CURRENT_NUMERATOR		147L
#define VOLTAGE_NUMERATOR		22LL
#define VOLTAGE_DENOMINATOR		10LL
#define CRATE_NUMERATOR			8789L
#define CRATE_DENOMINATOR		1000000L

#define OF_PROP_READ(chip, prop, dt_property, retval, optional)		\
do {									\
	if (retval)							\
		break;							\
	if (optional)							\
		prop = -EINVAL;						\
									\
	retval = of_property_read_u32(chip->dev->of_node,		\
			"st," dt_property, &prop);			\
									\
	if ((retval == -EINVAL) && optional)				\
		retval = 0;						\
	else if (retval)						\
		dev_err(chip->dev, "Error reading " #dt_property	\
				" property rc = %d\n", rc);		\
} while (0)

struct fg_data {
	int soc;
	int hr_soc;
	s16 voltage_mv;
	s16 current_ma;
	s16 avg_current_ma;
	s16 temperature;
	int ocv_mv;
};

struct fg_wakeup_source {
	struct wakeup_source	source;
	unsigned long		disabled;
};

struct stc311x_chip  {
	u8				mode_reg;
	u8				ctrl_reg;
	u16				hr_soc;
	u16				conv_counter;
	s16				current_ma;
	s16				voltage_mv;
	s16				temperature;
	s16				avg_current_ma;
	u16				cc_cnf;
	u16				vm_cnf;
	u16				ocv_mv;
	u8				cmonit_count;
	u8				cmonit_max;
	s16				cc_adj;
	s16				vm_adj;
	u32				peek_poke_address;
	int				running_mode;
	int				soc;
	int				last_temperature;
	int				ropt;
	int				nropt;

	/* Averaging/Accumulation */
	int				avg_voltage_mv;
	int				acc_voltage_mv;

	/* parameters */
	u32				cfg_alarm_soc;
	u32				cfg_alarm_voltage_mv;
	u32				cfg_relax_current_ma;
	u16				*cfg_adaptive_capacity_table;
	u16				*cfg_ocv_soc_table;
	s16				*cfg_temp_table;
	u16				*cfg_vmtemp_table;
	u8				*cfg_soc_table;
	int				cfg_cnom_mah;
	int				cfg_rsense_mohm;
	int				cfg_rbatt_mohm;
	int				cfg_term_current_ma;
	int				cfg_float_voltage_mv;
	int				cfg_empty_soc_uv;
	bool				cfg_force_vmode;
	bool				cfg_enable_soc_correction;

	/* RAM info */
	union {
		unsigned char		ram_data[RAM_SIZE];
		struct {
			u16		tstword;
			u16		hr_soc;
			u16		cc_cnf;
			u16		vm_cnf;
			u8		soc;
			u8		gg_status;
		} reg;
	} ram_info;

	struct fg_data			chip_data;
	struct i2c_client		*client;
	struct device			*dev;
	struct delayed_work		soc_calc_work;
	struct delayed_work		cutoff_check_work;
	struct fg_wakeup_source		soc_calc_wake_source;
	struct fg_wakeup_source		cutoff_wake_source;
	struct dentry			*debug_root;
	struct mutex			read_write_lock;
	struct power_supply		bms_psy;

	/* ADC notification */
	struct qpnp_adc_tm_chip         *adc_tm_dev;
	struct qpnp_vadc_chip		*vadc_dev;
	struct qpnp_adc_tm_btm_param	vbat_cutoff_params;
};

static char *fg_supplicants[] = {
	"battery",
};

#define get_val(x)			(((*x) << 8) | *(x - 1))
static int __stc311x_read_raw(struct stc311x_chip *chip, u8 reg, u8 *val,
				int bytes)
{
	int rc;

	rc = i2c_smbus_read_i2c_block_data(chip->client, reg, bytes, val);
	if (rc < 0)
		dev_err(chip->dev,
				"i2c read fail: can't read %d bytes from %02x: %d\n",
				bytes, reg, rc);
	else
		pr_debug("reading reg=0x%x val=0x%x\n", reg, *val);

	return (rc < 0) ? rc : 0;
}

static int __stc311x_write_raw(struct stc311x_chip *chip, u8 reg, u8 *val,
				int bytes)
{
	int rc;

	rc = i2c_smbus_write_i2c_block_data(chip->client, reg, bytes, val);
	if (rc < 0)
		dev_err(chip->dev,
				"i2c write fail: can't write %d bytes from %02x: %d\n",
				bytes, reg, rc);
	else
		pr_debug("writing reg=0x%x val=0x%x\n", reg, *val);

	return (rc < 0) ? rc : 0;
}

static int stc311x_read_raw(struct stc311x_chip *chip, u8 reg, u8 *val,
				int bytes)
{
	int rc;

	mutex_lock(&chip->read_write_lock);
	rc = __stc311x_read_raw(chip, reg, val, bytes);
	mutex_unlock(&chip->read_write_lock);

	return rc;
}

static int stc311x_write_raw(struct stc311x_chip *chip, u8 reg, u8 *val,
				int bytes)
{
	int rc;

	mutex_lock(&chip->read_write_lock);
	rc = __stc311x_write_raw(chip, reg, val, bytes);
	mutex_unlock(&chip->read_write_lock);

	return rc;
}

static int stc311x_masked_write(struct stc311x_chip *chip, u8 reg, u8 val,
				u8 mask)
{
	int rc;
	u8 reg_val;

	mutex_lock(&chip->read_write_lock);
	rc = __stc311x_read_raw(chip, reg, &reg_val, 1);
	if (rc < 0)
		goto out;

	reg_val &= ~mask;
	reg_val |= val & mask;
	rc = __stc311x_write_raw(chip, reg, &reg_val, 1);

	if (!rc)
		pr_debug("writing reg=0x%x mask 0x%x val=0x%x\n",
				reg, mask, reg_val);

out:
	mutex_unlock(&chip->read_write_lock);
	return rc;
}

#define LSB(val)		(val & 0xFF)
#define MSB(val)		((val >> 8) & 0xFF)
static int stc311x_write_param(struct stc311x_chip *chip, u8 reg, u16 val)
{
	int rc;
	u8 reg_val[2];

	mutex_lock(&chip->read_write_lock);
	reg_val[0] = LSB(val);
	reg_val[1] = MSB(val);
	rc = __stc311x_write_raw(chip, reg, reg_val, 2);
	mutex_unlock(&chip->read_write_lock);

	return rc;
}

static void stc311x_decode_twos_compl(struct stc311x_chip *chip,
					int16_t *val, u8 sign_bit)
{
	u16 mask;
	u16 magnitude;

	mask = BIT(sign_bit) - 1;
	magnitude = *val & mask;
	*val = (*val & BIT(sign_bit)) ?
			((~magnitude + 1) & mask) * -1 : magnitude;
}

static int calc_crc8(unsigned char *data, int len)
{
	int crc = 0;
	int i, j;

	for (i = 0; i < len; i++) {
		crc ^= data[i];
		for (j = 0; j < 8; j++) {
			crc <<= 1;
			if (crc & 0x100)
				crc ^= 7;
		}
	}

	return (crc & 255);
}

static int interpolate(s16 x, int n, s16 *tabx, u16 *taby)
{
	int index;
	int y;

	if (x >= tabx[0]) {
		y = taby[0];
	} else if (x <= tabx[n - 1]) {
		y = taby[n - 1];
	} else {
		for (index = 1; index < n; index++) {
			if (x > tabx[index])
				break;
		}
		y = ((taby[index - 1] - taby[index]) * (x - tabx[index]) * 2)
				/ (tabx[index - 1] - tabx[index]);
		y = (y + 1) / 2;
		y += taby[index];
	}

	pr_debug("interpolated value %d\n", y);
	return y;
}

static int conv(int val, int numerator, int denominator)
{

	return DIV_ROUND_CLOSEST(val * numerator, denominator);
}

static void fg_stay_awake(struct fg_wakeup_source *source)
{
	if (__test_and_clear_bit(0, &source->disabled)) {
		__pm_stay_awake(&source->source);
		pr_debug("enabled source %s\n", source->source.name);
	}
}

static void fg_relax(struct fg_wakeup_source *source)
{
	if (!__test_and_set_bit(0, &source->disabled)) {
		__pm_relax(&source->source);
		pr_debug("disabled source %s\n", source->source.name);
	}
}

static void stc311x_update_ram_crc(struct stc311x_chip *chip)
{
	int crc_val;

	/* calculate crc for [0 - 14] and store in [15] */
	crc_val = calc_crc8(chip->ram_info.ram_data, RAM_SIZE - 1);
	chip->ram_info.ram_data[RAM_SIZE - 1] = crc_val;

	pr_debug("calculated crc %d\n", crc_val);
}

static void stc311x_init_ram(struct stc311x_chip *chip)
{
	int i, rc;

	pr_debug("initializing RAM data\n");

	for (i = 0; i < RAM_SIZE; i++)
		chip->ram_info.ram_data[i] = 0;

	chip->ram_info.reg.tstword = RAM_TSTWORD;
	chip->ram_info.reg.cc_cnf = chip->cc_cnf;
	chip->ram_info.reg.vm_cnf = chip->vm_cnf;

	stc311x_update_ram_crc(chip);

	rc = stc311x_write_raw(chip, RAM_BASE, chip->ram_info.ram_data,
				RAM_SIZE);
	if (rc)
		pr_err("failed to read RAM data rc=%d\n", rc);
}

/* Update all the registers with the default values */
static int stc311x_update_params(struct stc311x_chip *chip)
{
	int rc, i;
	u8 reg_val, mask;
	u16 ocv_mv;

	/* gg_run = 0 before updating parameters */
	rc = stc311x_masked_write(chip, MODE_REG, 0, GG_RUN_BIT);
	if (rc < 0) {
		pr_err("failed to stop FG rc=%d\n", rc);
		return rc;
	}

	/* fill in OCV curve data */
	for (i = 0; i < OCV_SOC_SIZE; i++) {
		ocv_mv = conv(chip->cfg_ocv_soc_table[i], OCV_DENOMINATOR,
			       OCV_NUMERATOR);
		rc = stc311x_write_param(chip, OCVTAB_REG + (i * 2), ocv_mv);
		if (rc < 0) {
			pr_err("failed to update SOC Alarm rc=%d\n", rc);
			return rc;
		}
	}

	/* covert SOC points and fill SOC table*/
	for (i = 0; i < OCV_SOC_SIZE; i++)
		chip->cfg_soc_table[i] *= SOCTAB_MULTIPLIER;

	rc = stc311x_write_raw(chip, SOCTAB_REG, chip->cfg_soc_table,
				OCV_SOC_SIZE);
	if (rc < 0) {
		pr_err("failed to update SOCTAB rc=%d\n", rc);
		return rc;
	}

	/* update Alarm SOC */
	reg_val = chip->cfg_alarm_soc * 2;
	rc = stc311x_write_raw(chip, ALM_SOC_REG, &reg_val, 1);
	if (rc < 0) {
		pr_err("failed to update SOC Alarm rc=%d\n", rc);
		return rc;
	}

	/* update Alarm Voltage */
	reg_val = conv(chip->cfg_alarm_voltage_mv, ALM_VOLTAGE_DENOMINATOR,
			ALM_VOLTAGE_NUMERATOR);
	rc = stc311x_write_raw(chip, ALM_SOC_REG, &reg_val, 1);
	if (rc < 0) {
		pr_err("failed to update Voltage Alarm rc=%d\n", rc);
		return rc;
	}

	/* update Relaxation current */
	reg_val = conv(chip->cfg_relax_current_ma,
			RELAX_CURRENT_DENOMINATOR * chip->cfg_rsense_mohm,
			RELAX_CURRENT_NUMERATOR);
	/* mask top most bit */
	reg_val &= 0x7F;
	rc = stc311x_write_raw(chip, RELAX_CURRENT_REG, &reg_val, 1);
	if (rc < 0) {
		pr_err("failed to update Relax Current rc=%d\n", rc);
		return rc;
	}

	rc = stc311x_write_param(chip, CC_CNF_REG, chip->cc_cnf);
	if (rc < 0) {
		pr_err("failed to update CC_CNF rc=%d\n", rc);
		return rc;
	}

	rc = stc311x_write_param(chip, VM_CNF_REG, chip->vm_cnf);
	if (rc < 0) {
		pr_err("failed to update VM_CNF rc=%d\n", rc);
		return rc;
	}

	/* clear PORDET, BATFAIL, free ALM pin, reset conv counter */
	mask = (u8) ~GG_VM_BIT;
	reg_val = IO0_DATA_BIT | GG_RST_BIT;
	rc = stc311x_masked_write(chip, CTRL_REG, reg_val, mask);
	if (rc < 0) {
		pr_err("failed to stop Control reg rc=%d\n", rc);
		return rc;
	}

	/* configure/start FG */
	mask = FORCE_VM_BIT | FORCE_CC_BIT | GG_RUN_BIT
				| ALM_EN_BIT | FORCE_CD_BIT | VMODE_BIT;
	reg_val = chip->cfg_force_vmode ?
			FORCE_VM_BIT | GG_RUN_BIT | ALM_EN_BIT | VMODE_BIT
			: FORCE_CC_BIT | GG_RUN_BIT | ALM_EN_BIT;
	rc = stc311x_masked_write(chip, MODE_REG, reg_val, mask);
	if (rc < 0) {
		pr_err("failed to stop Control reg rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int stc311x_start_fg(struct stc311x_chip *chip)
{
	int rc;
	u8 reg[2];
	u16 ocv_mv;
	s16 current_ma;

	/* Read OCV */
	rc = stc311x_read_raw(chip, OCV_REG, reg, 2);
	if (rc) {
		pr_err("failed to read OCV rc=%d\n", rc);
		return rc;
	}

	ocv_mv = get_val(&reg[1]);
	ocv_mv = conv(ocv_mv, OCV_NUMERATOR, OCV_DENOMINATOR);

	/* current in mA 1 LSB = 5.88uV */
	rc = stc311x_read_raw(chip, CURRENT_REG, reg, 2);
	if (rc) {
		pr_err("failed to read OCV rc=%d\n", rc);
		return rc;
	}
	current_ma = get_val(&reg[1]);
	stc311x_decode_twos_compl(chip, &current_ma, CURRENT_SIGN_BIT);
	current_ma = conv(current_ma, CURRENT_NUMERATOR,
				CURRENT_DINOMINATOR * chip->cfg_rsense_mohm);

	/* Adjust OCV */
	chip->ocv_mv = ocv_mv + ((current_ma * chip->cfg_rbatt_mohm) / 1000);
	chip->current_ma = current_ma;

	pr_debug("compensated OCV %humV current %himA\n", chip->ocv_mv,
						chip->current_ma);
	rc = stc311x_update_params(chip);
	if (rc) {
		pr_err("failed to update FG params rc=%d\n", rc);
		return rc;
	}

	ocv_mv = conv(ocv_mv, OCV_DENOMINATOR, OCV_NUMERATOR);
	rc = stc311x_write_param(chip, OCV_REG, ocv_mv);
	if (rc) {
		pr_err("failed to write OCV rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int stc311x_restore_fg(struct stc311x_chip *chip)
{
	int rc;

	rc = stc311x_update_params(chip);
	if (rc) {
		pr_err("failed to update FG params rc=%d\n", rc);
		return rc;
	}

	if (chip->ram_info.reg.gg_status == GG_RUNNING) {
		/* Restore SOC */
		if (chip->ram_info.reg.soc != 0) {
			rc = stc311x_write_param(chip, SOC_REG,
						chip->ram_info.reg.hr_soc);
			if (rc) {
				pr_err("failed to write SOC from RAM rc=%d\n",
						rc);
				return rc;
			}

			pr_debug("updated SOC from RAM %hu\n",
						chip->ram_info.reg.hr_soc);
		}
	}

	return 0;
}

static int stc311x_start(struct stc311x_chip *chip)
{
	int rc = 0;
	u8 reg_val;

	chip->avg_voltage_mv = 0;
	chip->avg_current_ma = 0;
	chip->cc_cnf = ((chip->cfg_cnom_mah * chip->cfg_rsense_mohm)
				* 1000) / 49556;
	chip->vm_cnf = ((chip->cfg_cnom_mah * chip->cfg_rbatt_mohm)
				* 100) / 97778;
	pr_debug("caluclate cc_cnf = %x and vm_cnf = %x\n",
					chip->cc_cnf, chip->vm_cnf);

	chip->ropt = 0;
	chip->nropt = 0;

	/* Read data from RAM */
	rc = stc311x_read_raw(chip, RAM_BASE, chip->ram_info.ram_data,
				RAM_SIZE);
	if (rc) {
		pr_err("failed to read RAM data rc=%d\n", rc);
		return rc;
	}

	if ((chip->ram_info.reg.tstword != RAM_TSTWORD) ||
			(calc_crc8(chip->ram_info.ram_data, RAM_SIZE) != 0)) {
		/* RAM is invalid */
		stc311x_init_ram(chip);
		rc = stc311x_start_fg(chip);
		if (rc) {
			pr_err("failed to start FG rc=%d\n", rc);
			return rc;
		}
	} else {
		/* Valid RAM */
		rc = stc311x_read_raw(chip, CTRL_REG, &reg_val, 1);
		if (rc < 0) {
			pr_err("failed to read CTRL reg rc=%d\n", rc);
			return rc;
		}
		/* Check reset or Battery removal */
		if (reg_val & FG_RESET) {
			rc = stc311x_start_fg(chip);
			if (rc) {
				pr_err("failed to start FG rc=%d\n", rc);
				return rc;
			}
		} else {
			rc = stc311x_restore_fg(chip);
			if (rc) {
				pr_err("failed to restore FG rc=%d\n", rc);
				return rc;
			}
		}
	}

	chip->ram_info.reg.gg_status = GG_INIT;

	/* update RAM CRC */
	stc311x_update_ram_crc(chip);

	rc = stc311x_write_raw(chip, RAM_BASE, chip->ram_info.ram_data,
				RAM_SIZE);
	if (rc) {
		pr_err("failed to read RAM data rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int stc311x_force_soc(struct stc311x_chip *chip, int soc, int vbat_uv)
{
	int rc;

	rc = stc311x_write_param(chip, SOC_REG, soc);
	if (rc)
		pr_err("Failed to set SOC to %d rc=%d\n", soc, rc);
	else
		pr_warn("forcing SOC = %d vbat = %duV\n", soc, vbat_uv);

	return rc;
}

static int get_battery_voltage_adc(struct stc311x_chip *chip, int *result_uv)
{
	int rc;
	struct qpnp_vadc_result adc_result;

	if (!chip->vadc_dev)
		return -EINVAL;

	rc = qpnp_vadc_read(chip->vadc_dev, VBAT_SNS, &adc_result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
							VBAT_SNS, rc);
		return rc;
	}
	pr_debug("mvolts phy=%lld meas=0x%llx\n", adc_result.physical,
						adc_result.measurement);
	*result_uv = (int)adc_result.physical;

	return 0;
}

#define VBATT_ERROR_MARGIN_UV	20000
#define CUTOFF_MARGIN_UV	100000
#define CUTOFF_DELAY		30000
static void btm_notify_vbat(enum qpnp_tm_state state, void *ctx)
{
	struct stc311x_chip *chip = ctx;
	int vbat_uv;
	int rc;

	rc = get_battery_voltage_adc(chip, &vbat_uv);
	if (rc) {
		pr_err("failed to read battery voltage rc=%d\n", rc);
		goto out;
	}

	pr_debug("vbat is at %duV\n", vbat_uv);

	if (state == ADC_TM_LOW_STATE) {
		pr_debug("low voltage btm notification triggered\n");
		if (vbat_uv < (chip->vbat_cutoff_params.low_thr
					+ VBATT_ERROR_MARGIN_UV)) {
			fg_stay_awake(&chip->cutoff_wake_source);
			schedule_delayed_work(&chip->cutoff_check_work,
					msecs_to_jiffies(CUTOFF_DELAY));
			chip->vbat_cutoff_params.state_request =
						ADC_TM_HIGH_THR_ENABLE;
		} else {
			pr_debug("faulty btm trigger, discarding\n");
		}
	} else if (state == ADC_TM_HIGH_STATE) {
		pr_debug("high voltage btm notification triggered\n");
		if (vbat_uv >= chip->vbat_cutoff_params.high_thr) {
			chip->vbat_cutoff_params.state_request =
						ADC_TM_LOW_THR_ENABLE;
			cancel_delayed_work_sync(&chip->cutoff_check_work);
			fg_relax(&chip->cutoff_wake_source);
		} else {
			pr_debug("faulty btm trigger, discarding\n");
		}
	} else {
		pr_debug("unknown voltage notification state: %d\n", state);
	}

out:
	qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
					&chip->vbat_cutoff_params);
}

static int stc311x_vbat_cutoff_monitor(struct stc311x_chip *chip)
{
	int rc;

	chip->vbat_cutoff_params.low_thr =
			chip->cfg_empty_soc_uv + CUTOFF_MARGIN_UV;
	chip->vbat_cutoff_params.high_thr =
			chip->cfg_empty_soc_uv + CUTOFF_MARGIN_UV
						+ VBATT_ERROR_MARGIN_UV;
	chip->vbat_cutoff_params.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;
	chip->vbat_cutoff_params.channel = VBAT_SNS;
	chip->vbat_cutoff_params.btm_ctx = chip;
	chip->vbat_cutoff_params.timer_interval = ADC_MEAS1_INTERVAL_16S;
	chip->vbat_cutoff_params.threshold_notification = &btm_notify_vbat;
	rc = qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
						&chip->vbat_cutoff_params);
	if (rc) {
		pr_err("adc-tm setup failed: %d\n", rc);
		return rc;
	}

	pr_debug("set low thr to %d and high to %d\n",
					chip->vbat_cutoff_params.low_thr,
					chip->vbat_cutoff_params.high_thr);
	return 0;
}

int stc311x_parse_dt(struct stc311x_chip *chip)
{
	int rc = 0;
	int prop_len;

	chip->cfg_empty_soc_uv = -EINVAL;
	OF_PROP_READ(chip, chip->cfg_empty_soc_uv,
				"empty-soc-uv", rc, 1);
	if (chip->cfg_empty_soc_uv > 0) {
		chip->vadc_dev = qpnp_get_vadc(chip->dev, "fg");
		if (IS_ERR(chip->vadc_dev)) {
			rc = PTR_ERR(chip->vadc_dev);
			if (rc != -EPROBE_DEFER)
				pr_err("vadc not found defer probe\n");
			return rc;
		}

		chip->adc_tm_dev = qpnp_get_adc_tm(chip->dev, "fg");
		if (IS_ERR(chip->adc_tm_dev)) {
			rc = PTR_ERR(chip->adc_tm_dev);
			if (rc != -EPROBE_DEFER)
				pr_err("adc-tm property missing, rc=%d\n", rc);
			return rc;
		}

		rc =  stc311x_vbat_cutoff_monitor(chip);
		if (rc)
			pr_err("failed to configure vbat cutoff monitor rc=%d\n",
						rc);
	}
	OF_PROP_READ(chip, chip->cfg_rbatt_mohm,
					"rbatt-mohm", rc, 0);
	OF_PROP_READ(chip, chip->cfg_cnom_mah,
					"nom-capacity-mah", rc, 0);
	OF_PROP_READ(chip, chip->cfg_rsense_mohm,
					"rsense-mohm", rc, 0);
	OF_PROP_READ(chip, chip->cfg_float_voltage_mv,
					"float-voltage-mv", rc, 0);
	/*required property missing return error */
	if (rc)
		return rc;

	chip->cfg_alarm_soc = 10;
	OF_PROP_READ(chip, chip->cfg_alarm_soc, "alarm-soc", rc, 1);

	chip->cfg_alarm_voltage_mv = CUTOFF_VOTAGE_MV;
	OF_PROP_READ(chip, chip->cfg_alarm_voltage_mv,
					"alarm-voltage-mv", rc, 1);
	if (chip->cfg_alarm_voltage_mv <= CUTOFF_VOTAGE_MV)
		chip->cfg_alarm_voltage_mv = CUTOFF_VOTAGE_MV;

	chip->cfg_term_current_ma = chip->cfg_cnom_mah / 10;
	OF_PROP_READ(chip, chip->cfg_term_current_ma,
					"term-current-ma", rc, 1);

	chip->cfg_relax_current_ma = chip->cfg_cnom_mah / 20;
	OF_PROP_READ(chip, chip->cfg_relax_current_ma,
					"relax-current-ma", rc, 1);
	if (chip->cfg_relax_current_ma > 200)
			chip->cfg_relax_current_ma = 200;

	/* optional property error */
	if (rc)
		return rc;

	if (of_find_property(chip->dev->of_node,
				"st,adaptive-capacity-tbl", &prop_len)) {
		prop_len /= sizeof(u16);
		if (prop_len != NTEMP) {
			pr_err("invalid capacity data length\n");
			return -EINVAL;
		}

		chip->cfg_adaptive_capacity_table = devm_kzalloc(chip->dev,
				sizeof(u16) * NTEMP, GFP_KERNEL);
		if (!chip->cfg_adaptive_capacity_table)
			return -ENOMEM;

		rc = of_property_read_u16_array(chip->dev->of_node,
				"st,adaptive-capacity-tbl",
				chip->cfg_adaptive_capacity_table, prop_len);
		if (rc) {
			pr_err("invalid adaptive-capacity-tbl rc=%d\n", rc);
			return rc;
		}
	}

	/* Read the OCV curve data */
	if (!of_find_property(chip->dev->of_node, "st,ocv-tbl", &prop_len)) {
		pr_err("OCV-SOC curve data missing\n");
		return -EINVAL;
	}

	prop_len /= sizeof(u16);
	if (prop_len != OCV_SOC_SIZE) {
		pr_err("invalid OCV_SOC curve table\n");
		return -EINVAL;
	}

	chip->cfg_ocv_soc_table = devm_kzalloc(chip->dev,
				sizeof(u16) * OCV_SOC_SIZE, GFP_KERNEL);
	if (!chip->cfg_ocv_soc_table)
		return -ENOMEM;

	rc = of_property_read_u16_array(chip->dev->of_node, "st,ocv-tbl",
					chip->cfg_ocv_soc_table, prop_len);
	if (rc) {
		pr_err("invalid length of OCV_SOC curve table expected length=%d\n",
				OCV_SOC_SIZE);
		return rc;
	}

	/* Read the SOC point data */
	if (!of_find_property(chip->dev->of_node, "st,soc-tbl", &prop_len)) {
		pr_err("SOC point data st-soc-tbl missing\n");
		return -EINVAL;
	}

	prop_len /= sizeof(u8);
	if (prop_len != OCV_SOC_SIZE) {
		pr_err("invalid length of SOC point table expected length=%d\n",
				OCV_SOC_SIZE);
		return -EINVAL;
	}

	chip->cfg_soc_table = devm_kzalloc(chip->dev,
				sizeof(u8) * OCV_SOC_SIZE, GFP_KERNEL);
	if (!chip->cfg_soc_table)
		return -ENOMEM;

	rc = of_property_read_u8_array(chip->dev->of_node, "st,soc-tbl",
					chip->cfg_soc_table, prop_len);
	if (rc) {
		pr_err("invalid soc-tbl rc=%d\n", rc);
		return rc;
	}

	/* Read the VMTEMP data */
	if (!of_find_property(chip->dev->of_node,
				"st,vmtemp-tbl", &prop_len)) {
		pr_err("VMTEMP data missing\n");
		return -EINVAL;
	}

	prop_len /= sizeof(u16);
	if (prop_len != NTEMP) {
		pr_err("invalid OCV_SOC curve table\n");
		return -EINVAL;
	}

	chip->cfg_vmtemp_table = devm_kzalloc(chip->dev,
				sizeof(u16) * NTEMP, GFP_KERNEL);
	if (!chip->cfg_vmtemp_table)
		return -ENOMEM;

	rc = of_property_read_u16_array(chip->dev->of_node, "st,vmtemp-tbl",
			chip->cfg_vmtemp_table, prop_len);
	if (rc) {
		pr_err("invalid vmtemp-tbl rc=%d\n", rc);
		return rc;
	}

	/* Read the TEMP tbl */
	if (!of_find_property(chip->dev->of_node, "st,temp-tbl", &prop_len)) {
		pr_err("TEMP data missing\n");
		return -EINVAL;
	}

	prop_len /= sizeof(s16);
	if (prop_len != NTEMP) {
		pr_err("invalid OCV_SOC curve table\n");
		return -EINVAL;
	}

	chip->cfg_temp_table = devm_kzalloc(chip->dev,
				sizeof(s16) * NTEMP, GFP_KERNEL);
	if (!chip->cfg_temp_table)
		return -ENOMEM;

	rc = of_property_read_u16_array(chip->dev->of_node, "st,temp-tbl",
					chip->cfg_temp_table, prop_len);
	if (rc) {
		pr_err("invalid temp-tbl rc=%d\n", rc);
		return rc;
	}

	chip->cfg_force_vmode = of_property_read_bool(chip->dev->of_node,
						"st,force-voltage-mode");
	chip->cfg_enable_soc_correction =
				of_property_read_bool(chip->dev->of_node,
						"st,enable-soc-correction");

	return rc;
}

/* Reg info:
 * data[0]:	Mode reg
 * data[1]:	Control reg
 * data[2-3]:	SOC reg
 * data[4-5]:	Counter reg
 * data[6-7]:	Current reg
 * data[8-9]:	Voltage reg
 * data[10]:	Temperature reg
 * data[11-13]:	Avg. Temperature reg
 * data[14-15]: OCV reg
 * data[22]:	CMONIT count
 * data[23]:	CMONIT max
 * data[27-28]:	CC_ADJ
 * data[29-30]: VM_ADJ
 */

static int stc311x_read_fg(struct stc311x_chip *chip, struct fg_data *data)
{
	int rc;
	u8 reg[20];

	rc = stc311x_read_raw(chip, 0x0, reg, 15);
	if (rc < 0) {
		pr_err("failed to read register base rc=%d\n", rc);
		return rc;
	}

	chip->running_mode = reg[1] & GG_VM_BIT ? VM_MODE : CC_MODE;
	chip->mode_reg = reg[0];
	chip->ctrl_reg = reg[1];

	pr_debug("MODE reg = 0x%x CTRL = 0x%x run_mode = %s\n",
			chip->mode_reg, chip->ctrl_reg,
			chip->running_mode ? "VM_MODE" : "CC_MODE");
	/* Counter */
	chip->conv_counter = get_val(&reg[5]);

	/* SOC */
	data->hr_soc = get_val(&reg[3]);

	/* Current */
	data->current_ma = get_val(&reg[7]);
	stc311x_decode_twos_compl(chip, &data->current_ma, CURRENT_SIGN_BIT);
	data->current_ma = conv(data->current_ma, CURRENT_NUMERATOR,
				CURRENT_DINOMINATOR * chip->cfg_rsense_mohm);
	/* Voltage */
	data->voltage_mv = get_val(&reg[9]);
	stc311x_decode_twos_compl(chip, &data->voltage_mv, VOLTAGE_SIGN_BIT);
	data->voltage_mv = conv(data->voltage_mv, VOLTAGE_NUMERATOR,
					VOLTAGE_DENOMINATOR);

	/* Temperature */
	data->temperature = reg[10];
	stc311x_decode_twos_compl(chip, &data->temperature, TEMP_SIGN_BIT);
	data->temperature *= 10; /* DeciDegC */

	/* Average Current */
	data->avg_current_ma = get_val(&reg[12]);
	stc311x_decode_twos_compl(chip, &data->avg_current_ma,
					AVG_CURRENT_SIGN_BIT);
	if (chip->running_mode == CC_MODE) {
		data->avg_current_ma = conv(data->avg_current_ma,
				AVG_CURRENT_NUMERATOR,
				CURRENT_DINOMINATOR * chip->cfg_rsense_mohm);
	} else {
		data->avg_current_ma = conv(data->avg_current_ma,
				CRATE_NUMERATOR, CRATE_DENOMINATOR);
	}

	/* OCV */
	data->ocv_mv = get_val(&reg[14]);
	data->ocv_mv = conv(data->ocv_mv, OCV_NUMERATOR, OCV_DENOMINATOR);

	/* read rest of data */
	rc = stc311x_read_raw(chip, 22, reg, 9);
	if (rc < 0) {
		pr_err("failed to read register base rc=%d\n", rc);
		return rc;
	}

	/* CMONIT count */
	chip->cmonit_count = reg[0];

	/* CMONIT max */
	chip->cmonit_max = reg[1];

	/* ADJ no need to decode 2's complement */
	/* CC_ADJ */
	chip->cc_adj = get_val(&reg[7]);
	stc311x_decode_twos_compl(chip, &chip->cc_adj, 15);
	/* VM_ADJ */
	chip->vm_adj = get_val(&reg[9]);
	stc311x_decode_twos_compl(chip, &chip->vm_adj, 15);

	return 0;
}

static int stc311x_reset(struct stc311x_chip *chip)
{
	int rc;
	u8 mask;

	pr_debug("resetting FG and RAM\n");

	chip->ram_info.reg.tstword = 0;

	/* zero out whole RAM ? */
	rc = stc311x_write_raw(chip, 0x20, chip->ram_info.ram_data, RAM_SIZE);
	if (rc < 0) {
		pr_err("failed to update RAM base rc=%d\n", rc);
		return rc;
	}

	mask = UVLOD_BIT | PORDET_BIT | BATFAIL_BIT;
	rc = stc311x_masked_write(chip, CTRL_REG, PORDET_BIT, mask);
	if (rc < 0) {
		pr_err("failed to reset FG rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static void compensatesoc(struct stc311x_chip *chip)
{
	int r, v;
	u16 soc;

	soc = chip->soc;
	r = 0;
	r = interpolate(chip->temperature / 10, NTEMP, chip->cfg_temp_table,
				chip->cfg_adaptive_capacity_table);
	v = ((int) (soc - r) * MAX_SOC * 2) / (MAX_SOC - r);
	v = (v + 1) / 2;
	if (v < 0)
		v = 0;
	if (v > MAX_SOC)
		v = MAX_SOC;

	chip->soc = v;

	pr_debug("compensated soc old soc=%hu new soc=%hu\n", soc, chip->soc);
}

#define GAIN		10
#define A_VAR3		500
#define VAR1MAX		64
#define VAR2MAX		128
#define VAR4MAX		128

void soc_correction(struct stc311x_chip *chip)
{
	int var1 = 0, var2, var3, var4, rc;
	u16 socopt, soc;
	int current_threshold = chip->cfg_cnom_mah / 10;

	if (chip->soc > 800)
		var3 = 600;
	else if (chip->soc > 500)
		var3 = 400;
	else if (chip->soc > 250)
		var3 = 200;
	else if (chip->soc > 100)
		var3 = 300;
	else
		var3 = 400;

	var1 = ((256 * chip->avg_current_ma * A_VAR3) / var3)
				/ current_threshold;
	var1 = ((32768 * GAIN) / ((256 + (var1 * var1)) / 256)) / 10;
	var1 = (var1 + 1) / 2;
	if (!var1)
		var1 = 1;
	if (var1 > VAR1MAX)
		var1 = VAR1MAX - 1;

	var4 = chip->cc_adj - chip->vm_adj;

	if (chip->running_mode == CC_MODE)
		socopt = (chip->hr_soc + (var1 * var4)) / 64;
	else
		socopt = (chip->hr_soc + chip->cc_adj + (var1 * var4)) / 64;

	var2 = chip->nropt;
	if ((chip->avg_current_ma < (current_threshold * -1))
			|| (chip->avg_current_ma > current_threshold)) {
		if (var2 < VAR2MAX)
			var2++;

		chip->ropt = chip->ropt +
			(1000 * (chip->voltage_mv - chip->ocv_mv)
				/ chip->avg_current_ma - chip->ropt / var2);
		chip->nropt = var2;
	}

	if (socopt <= 0)
		socopt = 0;
	if (socopt >= MAX_HRSOC)
		socopt = MAX_HRSOC;

	if ((var4 < (VAR4MAX * -1)) || (var4 >= VAR4MAX)) {
		/* write param */
		rc = stc311x_write_param(chip, SOC_REG, socopt);
		if (rc < 0) {
			pr_err("failed to write soc rc=%d\n", rc);
		} else {
			soc = DIV_ROUND_CLOSEST(socopt, SOC_FACTOR);
			pr_debug("old soc = %hu old hr_soc = %hu new soc = %hu new hr_soc =%hu\n",
					chip->soc, chip->hr_soc, soc, socopt);
			chip->hr_soc = socopt;
			chip->soc = soc;
		}
	}
}

static int compensate_vm(struct stc311x_chip *chip)
{
	int r, rc;
	u8 reg;
	u16 vm_cnf;

	r = interpolate(chip->temperature / 10, NTEMP,
				chip->cfg_temp_table, chip->cfg_vmtemp_table);
	/* write the vm_cnf compensation */
	reg = chip->mode_reg & ~GG_RUN_BIT;
	rc = stc311x_write_raw(chip, MODE_REG, &reg, 1);
	if (rc) {
		pr_err("failed to reset GG_RUN rc=%d\n", rc);
		return rc;
	}

	vm_cnf = (chip->vm_cnf * r) / 100;
	rc = stc311x_write_param(chip, VM_CNF_REG, vm_cnf);
	if (rc) {
		pr_err("failed to reset VM_CNF_REG rc=%d\n", rc);
		return rc;
	}

	chip->ram_info.reg.vm_cnf = vm_cnf;
	if (!chip->cfg_force_vmode)
		chip->mode_reg |= FORCE_CC_BIT | GG_RUN_BIT;

	rc = stc311x_write_raw(chip, MODE_REG, &chip->mode_reg, 1);
	if (rc) {
		pr_err("failed to reset GG_RUN rc=%d\n", rc);
		return rc;
	}

	pr_debug("compensated vm_cnf = %hu\n", vm_cnf);

	return 0;
}

#define DELTA_TEMP		30
static void stc311x_vm_mode_fsm(struct stc311x_chip *chip)
{
	int rc;

	if ((chip->temperature > (chip->last_temperature + DELTA_TEMP))
			|| (chip->temperature
				< (chip->last_temperature - DELTA_TEMP))) {
		chip->last_temperature = chip->temperature;
		rc = compensate_vm(chip);
		if (rc < 0)
			pr_err("failed to compensate for VM rc=%d\n", rc);
	}
}

static void stc311x_mm_mode_fsm(struct stc311x_chip *chip)
{
	int rc;
	u16 soc;

	if ((chip->avg_voltage_mv > chip->cfg_float_voltage_mv)
			&& (chip->avg_current_ma < chip->cfg_term_current_ma)) {
		/* End of charge */
		soc = MAX_HRSOC;
		rc = stc311x_write_param(chip, SOC_REG, soc);
		if (rc) {
			pr_err("Failed to set SOC rc=%d\n", rc);
		} else {
			chip->hr_soc = soc;
			chip->soc = MAX_SOC;

			pr_debug("EOC reached\n");
		}
	}
}

#define AVG_FILTER		4
static int stc311x_process_fg_task(struct stc311x_chip *chip)
{
	int rc, last_soc, cutoff, soc;
	struct fg_data temp_data;

	last_soc = chip->soc;
	rc = stc311x_read_fg(chip, &temp_data);
	if (rc < 0)
		return rc;

	/* Read data from RAM */
	rc = stc311x_read_raw(chip, RAM_BASE, chip->ram_info.ram_data,
				RAM_SIZE);
	if (rc) {
		pr_err("failed to read RAM data rc=%d\n", rc);
		return rc;
	}

	if ((chip->ctrl_reg & (BATFAIL_BIT | UVLOD_BIT))) {
		pr_warn("BATFAIL_BIT/UVLOD_BIT detected\n");
		/* Reset FG */
		rc = stc311x_reset(chip);
		if (rc) {
			pr_err("failed to reset FG rc=%d\n", rc);
			return rc;
		}

		/* BATD or UVLOD detected */
		if (chip->conv_counter > 0) {
			chip->voltage_mv = temp_data.voltage_mv;
			chip->hr_soc = temp_data.hr_soc;
			chip->temperature = temp_data.temperature;
			chip->soc = DIV_ROUND_CLOSEST(chip->hr_soc * 10,
						SOC_FACTOR);
			chip->current_ma = temp_data.current_ma;
			chip->ocv_mv = temp_data.ocv_mv;
		}

		return 0;
	}

	if ((chip->ram_info.reg.tstword != RAM_TSTWORD) ||
			(calc_crc8(chip->ram_info.ram_data, RAM_SIZE) != 0)) {
		pr_debug("invalid RAM detected\n");
		/* RAM in invalid */
		stc311x_init_ram(chip);
		chip->ram_info.reg.gg_status = GG_INIT;
	} else if (!(chip->mode_reg & GG_RUN_BIT)) {
		pr_warn("FG in standby detected\n");
		rc = stc311x_restore_fg(chip);
		if (rc) {
			pr_err("failed to restore FG rc=%d\n", rc);
			return rc;
		}

		chip->ram_info.reg.gg_status = GG_INIT;
	}

	chip->hr_soc = temp_data.hr_soc;
	chip->soc = DIV_ROUND_CLOSEST(chip->hr_soc * 10, SOC_FACTOR);
	chip->temperature = temp_data.temperature;
	chip->ocv_mv = temp_data.ocv_mv;
	chip->current_ma = temp_data.current_ma;
	chip->voltage_mv = temp_data.voltage_mv;

	if (chip->ram_info.reg.gg_status == GG_INIT) {
		/* In init state wait for one convesion to complete */
		if (chip->conv_counter > 0) {
			rc = compensate_vm(chip);
			chip->last_temperature = chip->temperature;

			/* Inititalize Averaging */
			chip->avg_voltage_mv = chip->voltage_mv;
			chip->avg_current_ma = chip->current_ma;

			/* Accumulated values */
			chip->acc_voltage_mv = chip->avg_voltage_mv
							* AVG_FILTER;

			chip->ram_info.reg.gg_status = GG_RUNNING;
		}
	} else {
		chip->avg_current_ma = temp_data.avg_current_ma;
	}

	if (chip->cfg_enable_soc_correction && chip->avg_current_ma)
		soc_correction(chip);

	if (chip->cfg_adaptive_capacity_table)
		compensatesoc(chip);

	cutoff = chip->cfg_alarm_voltage_mv;

	/* We can move to moving average after some testing instead of this */
	chip->acc_voltage_mv += chip->voltage_mv - chip->avg_voltage_mv;
	chip->avg_voltage_mv = (chip->acc_voltage_mv + (AVG_FILTER / 2))
					/ AVG_FILTER;

	if (chip->running_mode == VM_MODE) {
		stc311x_vm_mode_fsm(chip);

		/* Compensation */
		if ((chip->avg_current_ma >= chip->cfg_term_current_ma)
			&& ((chip->soc >= 990) && (chip->soc <= 995))) {
			soc = 99 * SOC_FACTOR;
			rc = stc311x_write_param(chip, SOC_REG, soc);
			if (rc)
				pr_err("Failed to set SOC rc=%d\n", rc);
			else
				chip->soc = 990;
		}

		if ((chip->avg_current_ma < 0)
				&& ((chip->soc >= 15) && (chip->soc < 20))
				&& (chip->voltage_mv > (cutoff + 50))) {

			soc = 2 * SOC_FACTOR;
			rc = stc311x_write_param(chip, SOC_REG, soc);
			if (rc)
				pr_err("Failed to set SOC rc=%d\n", rc);
			else
				chip->soc = 20;
		}

	} else {
		stc311x_mm_mode_fsm(chip);
	}


	/* Store to RAM */
	chip->ram_info.reg.hr_soc = chip->hr_soc;
	chip->ram_info.reg.soc = chip->soc;
	stc311x_update_ram_crc(chip);
	rc = stc311x_write_raw(chip, RAM_BASE, chip->ram_info.ram_data,
					RAM_SIZE);
	if (rc < 0)
		pr_err("failed to update RAM rc=%d\n", rc);

	if (last_soc != chip->soc) {
		pr_debug("old soc %hu new soc = %hu hr_sox = %hu\n",
				last_soc, chip->soc, chip->hr_soc);
		power_supply_changed(&chip->bms_psy);
	}

	return 0;
}

#define LOW_SOC_DELAY		10000
#define SOC_CALC_DELAY		20000
static void soc_calc_work_fn(struct work_struct *work)
{
	int delay;
	struct stc311x_chip *chip = container_of(work, struct stc311x_chip,
					soc_calc_work.work);

	fg_stay_awake(&chip->soc_calc_wake_source);
	stc311x_process_fg_task(chip);
	fg_relax(&chip->soc_calc_wake_source);

	delay = (chip->soc <= (chip->cfg_alarm_soc * 10)) ? LOW_SOC_DELAY
							: SOC_CALC_DELAY;

	schedule_delayed_work(&chip->soc_calc_work, msecs_to_jiffies(delay));
}

static void cutoff_check_work_fn(struct work_struct *work)
{
	int rc;
	int vbat_uv;
	struct stc311x_chip *chip = container_of(work, struct stc311x_chip,
					cutoff_check_work.work);

	rc = get_battery_voltage_adc(chip, &vbat_uv);
	if (rc) {
		pr_err("failed to read VBAT rc=%d\n", rc);
		goto out;
	}
	pr_debug("vbat is at %duV\n", vbat_uv);

	/*
	 * release wakeup source and return if battery voltage crosses high
	 * threhold limit.
	 */
	if (vbat_uv >=
		(chip->vbat_cutoff_params.high_thr + VBATT_ERROR_MARGIN_UV)) {
		fg_relax(&chip->cutoff_wake_source);
		return;
	}

	if (vbat_uv <= (chip->cfg_empty_soc_uv)) {
		rc = stc311x_force_soc(chip, 0, vbat_uv);
		if (rc)
			goto out;
		stc311x_process_fg_task(chip);

		return;
	}

out:
	schedule_delayed_work(&chip->cutoff_check_work,
					msecs_to_jiffies(CUTOFF_DELAY));
}


static enum power_supply_property stc311x_bms_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
};

static int stc311x_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct stc311x_chip *chip = container_of(psy,
					struct stc311x_chip, bms_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->voltage_mv * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = chip->current_ma * 1000 * -1;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->soc / 10;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = chip->temperature;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#define LAST_CFG_REG	0x1E
static int show_cfg_regs(struct seq_file *m, void *data)
{
	struct stc311x_chip *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = 0; addr <= LAST_CFG_REG; addr++) {
		rc = stc311x_read_raw(chip, addr, &reg, 1);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int cfg_debugfs_open(struct inode *inode, struct file *file)
{
	struct stc311x_chip *chip = inode->i_private;

	return single_open(file, show_cfg_regs, chip);
}

static const struct file_operations cfg_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= cfg_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int show_ocvtab_regs(struct seq_file *m, void *data)
{
	struct stc311x_chip *chip = m->private;
	int rc, i;
	u8 reg[2];
	u16 ocv_mv;

	for (i = 0 ; i < OCV_SOC_SIZE; i++) {
		rc = stc311x_read_raw(chip, OCVTAB_REG + (i * 2), reg, 2);
		ocv_mv = get_val(&reg[1]);
		ocv_mv = conv(ocv_mv, OCV_NUMERATOR, OCV_DENOMINATOR);
		if (!rc) {
			seq_printf(m, "0x%02x = 0x%02x%x ocv=%hu\n",
				OCVTAB_REG + (i * 2), reg[1], reg[0], ocv_mv);
		}
	}

	return 0;
}

static int ocv_debugfs_open(struct inode *inode, struct file *file)
{
	struct stc311x_chip *chip = inode->i_private;

	return single_open(file, show_ocvtab_regs, chip);
}

static const struct file_operations ocv_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= ocv_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int show_soctab_regs(struct seq_file *m, void *data)
{
	struct stc311x_chip *chip = m->private;
	int rc, i;
	u8 reg;

	for (i = 0 ; i < OCV_SOC_SIZE; i++) {
		rc = stc311x_read_raw(chip, SOCTAB_REG + i, &reg, 1);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n",
					SOCTAB_REG + i, reg);
	}

	return 0;
}

static int soc_debugfs_open(struct inode *inode, struct file *file)
{
	struct stc311x_chip *chip = inode->i_private;

	return single_open(file, show_soctab_regs, chip);
}

static const struct file_operations soc_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= soc_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int show_ram_regs(struct seq_file *m, void *data)
{
	struct stc311x_chip *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = RAM_BASE; addr < RAM_BASE + RAM_SIZE; addr++) {
		rc = stc311x_read_raw(chip, addr, &reg, 1);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int ram_debugfs_open(struct inode *inode, struct file *file)
{
	struct stc311x_chip *chip = inode->i_private;

	return single_open(file, show_ram_regs, chip);
}

static const struct file_operations ram_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= ram_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int get_reg(void *data, u64 *val)
{
	struct stc311x_chip *chip = data;
	int rc;
	u8 temp;

	rc = stc311x_read_raw(chip, chip->peek_poke_address, &temp, 1);
	if (rc < 0) {
		pr_err("failed to read reg %x rc = %d\n",
				chip->peek_poke_address, rc);
		return -EAGAIN;
	}
	*val = temp;

	return 0;
}

static int set_reg(void *data, u64 val)
{
	struct stc311x_chip *chip = data;
	int rc;
	u8 temp;

	temp = (u8)val;
	rc = stc311x_write_raw(chip, chip->peek_poke_address, &temp, 1);
	if (rc < 0) {
		pr_err("failed to read reg %x rc = %d\n",
				chip->peek_poke_address, rc);
		return -EAGAIN;
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(poke_poke_debug_ops, get_reg, set_reg, "0x%02llx\n");

static void create_debugfs_entries(struct stc311x_chip *chip)
{
	struct dentry *ent;

	chip->debug_root = debugfs_create_dir("stc311x", NULL);
	if (!chip->debug_root) {
		pr_err("failed to create debug dir\n");
		return;
	}

	ent = debugfs_create_file("config_registers", S_IFREG | S_IRUGO,
			chip->debug_root, chip, &cfg_debugfs_ops);
	if (!ent)
		pr_err("failed to create config_resgisters debug file\n");

	ent = debugfs_create_file("ocv_tab", S_IFREG | S_IRUGO,
			chip->debug_root, chip, &ocv_debugfs_ops);
	if (!ent)
		pr_err("failed to create ocv_tab debug file\n");

	ent = debugfs_create_file("soc_tab", S_IFREG | S_IRUGO,
			chip->debug_root, chip, &soc_debugfs_ops);
	if (!ent)
		pr_err("failed to create soc_tab debug file\n");

	ent = debugfs_create_file("ram_dump", S_IFREG | S_IRUGO,
			chip->debug_root, chip, &ram_debugfs_ops);
	if (!ent)
		pr_err("failed to create ram_dump debug file\n");


	ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
			chip->debug_root, &chip->peek_poke_address);
	if (!ent)
		pr_err("failed to create address debug file\n");

	ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
			chip->debug_root, chip,  &poke_poke_debug_ops);
	if (!ent)
		pr_err("failed to create data debug file\n");
}

static int stc311x_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct stc311x_chip *chip;
	int rc;
	u8 reg;

	/*First check the functionality supported by the host*/
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_I2C_BLOCK))
		return -EIO;
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WRITE_I2C_BLOCK))
		return -EIO;

	chip = devm_kzalloc(&client->dev, sizeof(struct stc311x_chip),
				GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	i2c_set_clientdata(client, chip);
	chip->dev = &client->dev;
	chip->client = client;
	mutex_init(&chip->read_write_lock);
	INIT_DELAYED_WORK(&chip->soc_calc_work, soc_calc_work_fn);
	INIT_DELAYED_WORK(&chip->cutoff_check_work, cutoff_check_work_fn);

	/* read chip id */
	rc = stc311x_read_raw(chip, ID_REG, &reg, 1);
	if (rc) {
		pr_err("failed to read chip-id rc = %d\n", rc);
		return rc;
	}

	if (reg != STC3117_ID) {
		pr_err("not STC3177 chip\n");
		return -EINVAL;
	}

	rc = stc311x_parse_dt(chip);
	if (rc) {
		pr_err("failed to parse DT rc=%d\n", rc);
		return rc;
	}

	chip->soc_calc_wake_source.disabled = 1;
	wakeup_source_init(&chip->soc_calc_wake_source.source,
				"fg_soc_calc_wake");
	chip->cutoff_wake_source.disabled = 1;
	wakeup_source_init(&chip->cutoff_wake_source.source,
				"fg_cutoff_wake");
	rc = stc311x_start(chip);
	if (rc) {
		pr_err("failed to start rc=%d\n", rc);
		goto fail;
	}

	/* register to power supply framework */
	chip->bms_psy.name = "bms";
	chip->bms_psy.type = POWER_SUPPLY_TYPE_BMS;
	chip->bms_psy.get_property = stc311x_get_property;
	chip->bms_psy.properties = stc311x_bms_props;
	chip->bms_psy.num_properties = ARRAY_SIZE(stc311x_bms_props);
	chip->bms_psy.supplied_to = fg_supplicants;
	chip->bms_psy.num_supplicants = ARRAY_SIZE(fg_supplicants);
	rc = power_supply_register(chip->dev, &chip->bms_psy);
	if (rc) {
		pr_err("failed to register to power_supply framework rc=%d\n",
				rc);
		goto fail;
	}

	create_debugfs_entries(chip);
	schedule_delayed_work(&chip->soc_calc_work, 0);

	pr_info("stc311x FG successfully probed\n");
	return 0;

fail:
	wakeup_source_trash(&chip->soc_calc_wake_source.source);
	wakeup_source_trash(&chip->cutoff_wake_source.source);
	return rc;
}

static int stc311x_remove(struct i2c_client *client)
{
	int rc;
	struct stc311x_chip *chip = i2c_get_clientdata(client);

	/* release IO0DATA pin */
	rc = stc311x_masked_write(chip, CTRL_REG, IO0_DATA_BIT, IO0_DATA_BIT);
	if (rc < 0)
		pr_err("failed to release IO0DATA rc=%d\n", rc);

	/* Put FG in standby mode */
	rc = stc311x_masked_write(chip, MODE_REG, 0, GG_RUN_BIT);
	if (rc < 0)
		pr_err("failed to stopFG rc=%d\n", rc);

	cancel_delayed_work_sync(&chip->soc_calc_work);
	power_supply_unregister(&chip->bms_psy);
	debugfs_remove_recursive(chip->debug_root);
	mutex_destroy(&chip->read_write_lock);

	return 0;
}

static int st311x_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct stc311x_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&chip->soc_calc_work);

	return 0;
}

static int st311x_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct stc311x_chip *chip = i2c_get_clientdata(client);

	schedule_delayed_work(&chip->soc_calc_work, 0);

	return 0;
}

static const struct dev_pm_ops st311x_pm_ops = {
	.suspend	= st311x_suspend,
	.resume		= st311x_resume,
};

static const struct of_device_id stc311x_of_match[] = {
	{ .compatible = "st,stc3117", },
	{ },
};

static const struct i2c_device_id stc311x_id[] = {
	{ "stc3117", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, stc311x_id);

static struct i2c_driver stc311x_driver = {
	.driver = {
		.name		= "stc3117",
		.owner		= THIS_MODULE,
		.of_match_table = of_match_ptr(stc311x_of_match),
		.pm		= &st311x_pm_ops,
	},
	.probe		= stc311x_probe,
	.remove		= stc311x_remove,
	.id_table	= stc311x_id,
};

module_i2c_driver(stc311x_driver);

MODULE_AUTHOR("STMICROELECTRONICS");
MODULE_DESCRIPTION("STC311x Fuel Gauge");
MODULE_LICENSE("GPL v2");
