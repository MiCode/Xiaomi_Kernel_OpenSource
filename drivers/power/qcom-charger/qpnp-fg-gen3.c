/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"FG: %s: " fmt, __func__

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_batterydata.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/iio/consumer.h>
#include <linux/qpnp/qpnp-revid.h>
#include "fg-core.h"
#include "fg-reg.h"

#define FG_GEN3_DEV_NAME	"qcom,fg-gen3"

#define PERPH_SUBTYPE_REG		0x05
#define FG_BATT_SOC_PMICOBALT		0x10
#define FG_BATT_INFO_PMICOBALT		0x11
#define FG_MEM_INFO_PMICOBALT		0x0D

/* SRAM address and offset in ascending order */
#define CUTOFF_VOLT_WORD		5
#define CUTOFF_VOLT_OFFSET		0
#define SYS_TERM_CURR_WORD		6
#define SYS_TERM_CURR_OFFSET		0
#define DELTA_SOC_THR_WORD		12
#define DELTA_SOC_THR_OFFSET		3
#define RECHARGE_SOC_THR_WORD		14
#define RECHARGE_SOC_THR_OFFSET		0
#define CHG_TERM_CURR_WORD		14
#define CHG_TERM_CURR_OFFSET		1
#define EMPTY_VOLT_WORD			15
#define EMPTY_VOLT_OFFSET		0
#define VBATT_LOW_WORD			15
#define VBATT_LOW_OFFSET		1
#define ESR_TIMER_DISCHG_MAX_WORD	17
#define ESR_TIMER_DISCHG_MAX_OFFSET	0
#define ESR_TIMER_DISCHG_INIT_WORD	17
#define ESR_TIMER_DISCHG_INIT_OFFSET	2
#define ESR_TIMER_CHG_MAX_WORD		18
#define ESR_TIMER_CHG_MAX_OFFSET	0
#define ESR_TIMER_CHG_INIT_WORD		18
#define ESR_TIMER_CHG_INIT_OFFSET	2
#define PROFILE_LOAD_WORD		24
#define PROFILE_LOAD_OFFSET		0
#define NOM_CAP_WORD			58
#define NOM_CAP_OFFSET			0
#define PROFILE_INTEGRITY_WORD		79
#define PROFILE_INTEGRITY_OFFSET	3
#define BATT_SOC_WORD			91
#define BATT_SOC_OFFSET			0
#define MONOTONIC_SOC_WORD		94
#define MONOTONIC_SOC_OFFSET		2
#define VOLTAGE_PRED_WORD		97
#define VOLTAGE_PRED_OFFSET		0
#define OCV_WORD			97
#define OCV_OFFSET			2
#define RSLOW_WORD			101
#define RSLOW_OFFSET			0
#define LAST_BATT_SOC_WORD		119
#define LAST_BATT_SOC_OFFSET		0
#define LAST_MONOTONIC_SOC_WORD		119
#define LAST_MONOTONIC_SOC_OFFSET	2

static int fg_decode_value_16b(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val);
static int fg_decode_default(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val);
static void fg_encode_voltage_16b(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val, u8 *buf);
static void fg_encode_voltage_8b(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val, u8 *buf);
static void fg_encode_current(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val, u8 *buf);
static void fg_encode_default(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val, u8 *buf);

#define PARAM(_id, _addr, _offset, _len, _num, _den, _enc, _dec)	\
	[FG_SRAM_##_id] = {						\
		.address = _addr,					\
		.offset = _offset,					\
		.len = _len,						\
		.numrtr = _num,						\
		.denmtr = _den,						\
		.encode = _enc,						\
		.decode = _dec,						\
	}								\

static struct fg_sram_param pmicobalt_v1_sram_params[] = {
	PARAM(BATT_SOC, BATT_SOC_WORD, BATT_SOC_OFFSET, 4, 1, 1, NULL,
		fg_decode_default),
	PARAM(VOLTAGE_PRED, VOLTAGE_PRED_WORD, VOLTAGE_PRED_OFFSET, 2, 244141,
		1000, NULL, fg_decode_value_16b),
	PARAM(OCV, OCV_WORD, OCV_OFFSET, 2, 244141, 1000, NULL,
		fg_decode_value_16b),
	PARAM(RSLOW, RSLOW_WORD, RSLOW_OFFSET, 2, 244141, 1000, NULL,
		fg_decode_value_16b),
	/* Entries below here are configurable during initialization */
	PARAM(CUTOFF_VOLT, CUTOFF_VOLT_WORD, CUTOFF_VOLT_OFFSET, 2, 1000000,
		244141, fg_encode_voltage_16b, NULL),
	PARAM(EMPTY_VOLT, EMPTY_VOLT_WORD, EMPTY_VOLT_OFFSET, 1, 100000, 390625,
		fg_encode_voltage_8b, NULL),
	PARAM(VBATT_LOW, VBATT_LOW_WORD, VBATT_LOW_OFFSET, 1, 100000, 390625,
		fg_encode_voltage_8b, NULL),
	PARAM(SYS_TERM_CURR, SYS_TERM_CURR_WORD, SYS_TERM_CURR_OFFSET, 3,
		1000000, 122070, fg_encode_current, NULL),
	PARAM(CHG_TERM_CURR, CHG_TERM_CURR_WORD, CHG_TERM_CURR_OFFSET, 1,
		100000, 390625, fg_encode_current, NULL),
	PARAM(DELTA_SOC_THR, DELTA_SOC_THR_WORD, DELTA_SOC_THR_OFFSET, 1, 256,
		100, fg_encode_default, NULL),
	PARAM(RECHARGE_SOC_THR, RECHARGE_SOC_THR_WORD, RECHARGE_SOC_THR_OFFSET,
		1, 256, 100, fg_encode_default, NULL),
	PARAM(ESR_TIMER_DISCHG_MAX, ESR_TIMER_DISCHG_MAX_WORD,
		ESR_TIMER_DISCHG_MAX_OFFSET, 2, 1, 1, fg_encode_default, NULL),
	PARAM(ESR_TIMER_DISCHG_INIT, ESR_TIMER_DISCHG_INIT_WORD,
		ESR_TIMER_DISCHG_INIT_OFFSET, 2, 1, 1, fg_encode_default,
		NULL),
	PARAM(ESR_TIMER_CHG_MAX, ESR_TIMER_CHG_MAX_WORD,
		ESR_TIMER_CHG_MAX_OFFSET, 2, 1, 1, fg_encode_default, NULL),
	PARAM(ESR_TIMER_CHG_INIT, ESR_TIMER_CHG_INIT_WORD,
		ESR_TIMER_CHG_INIT_OFFSET, 2, 1, 1, fg_encode_default, NULL),
};

static int fg_gen3_debug_mask;
module_param_named(
	debug_mask, fg_gen3_debug_mask, int, S_IRUSR | S_IWUSR
);

static int fg_sram_update_period_ms = 30000;
module_param_named(
	sram_update_period_ms, fg_sram_update_period_ms, int, S_IRUSR | S_IWUSR
);

/* Other functions HERE */

static int fg_awake_cb(struct votable *votable, void *data, int awake,
			const char *client)
{
	struct fg_chip *chip = data;

	if (awake)
		pm_stay_awake(chip->dev);
	else
		pm_relax(chip->dev);

	pr_debug("client: %s awake: %d\n", client, awake);
	return 0;
}

static bool is_charger_available(struct fg_chip *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
		return false;

	return true;
}

static void status_change_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
			struct fg_chip, status_change_work);
	union power_supply_propval prop = {0, };

	if (!is_charger_available(chip)) {
		fg_dbg(chip, FG_STATUS, "Charger not available?!\n");
		return;
	}

	power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_STATUS,
			&prop);
	switch (prop.intval) {
	case POWER_SUPPLY_STATUS_CHARGING:
		fg_dbg(chip, FG_POWER_SUPPLY, "Charging\n");
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
		fg_dbg(chip, FG_POWER_SUPPLY, "Discharging\n");
		break;
	case POWER_SUPPLY_STATUS_FULL:
		fg_dbg(chip, FG_POWER_SUPPLY, "Full\n");
		break;
	default:
		break;
	}
}

#define PROFILE_LEN			224
#define PROFILE_COMP_LEN		32
#define SOC_READY_WAIT_MS		2000
static void profile_load_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				profile_load_work.work);
	int rc;
	u8 buf[PROFILE_COMP_LEN], val;
	bool tried_again = false, profiles_same = false;

	if (!chip->batt_id_avail) {
		pr_err("batt_id not available\n");
		return;
	}

	rc = fg_sram_read(chip, PROFILE_INTEGRITY_WORD,
			PROFILE_INTEGRITY_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to read profile integrity rc=%d\n", rc);
		return;
	}

	vote(chip->awake_votable, PROFILE_LOAD, true, 0);
	if (val == 0x01) {
		fg_dbg(chip, FG_STATUS, "Battery profile integrity bit is set\n");
		rc = fg_sram_read(chip, PROFILE_LOAD_WORD, PROFILE_LOAD_OFFSET,
				buf, PROFILE_COMP_LEN, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in reading battery profile, rc:%d\n", rc);
			goto out;
		}
		profiles_same = memcmp(chip->batt_profile, buf,
					PROFILE_COMP_LEN) == 0;
		if (profiles_same) {
			fg_dbg(chip, FG_STATUS, "Battery profile is same\n");
			goto done;
		}
		fg_dbg(chip, FG_STATUS, "profiles are different?\n");
	}

	fg_dbg(chip, FG_STATUS, "profile loading started\n");
	rc = fg_masked_write(chip, BATT_SOC_RESTART(chip), RESTART_GO_BIT, 0);
	if (rc < 0) {
		pr_err("Error in writing to %04x, rc=%d\n",
			BATT_SOC_RESTART(chip), rc);
		goto out;
	}

	/* load battery profile */
	rc = fg_sram_write(chip, PROFILE_LOAD_WORD, PROFILE_LOAD_OFFSET,
			chip->batt_profile, PROFILE_LEN, FG_IMA_ATOMIC);
	if (rc < 0) {
		pr_err("Error in writing battery profile, rc:%d\n", rc);
		goto out;
	}

	rc = fg_masked_write(chip, BATT_SOC_RESTART(chip), RESTART_GO_BIT,
			RESTART_GO_BIT);
	if (rc < 0) {
		pr_err("Error in writing to %04x, rc=%d\n",
			BATT_SOC_RESTART(chip), rc);
		goto out;
	}

wait:
	rc = wait_for_completion_interruptible_timeout(&chip->soc_ready,
		msecs_to_jiffies(SOC_READY_WAIT_MS));

	/* If we were interrupted wait again one more time. */
	if (rc == -ERESTARTSYS && !tried_again) {
		tried_again = true;
		goto wait;
	} else if (rc <= 0) {
		pr_err("wait for soc_ready timed out rc=%d\n", rc);
		goto out;
	}

	fg_dbg(chip, FG_STATUS, "SOC is ready\n");

	/* Set the profile integrity bit */
	val = 0x1;
	rc = fg_sram_write(chip, PROFILE_INTEGRITY_WORD,
			PROFILE_INTEGRITY_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to write profile integrity rc=%d\n", rc);
		goto out;
	}

	fg_dbg(chip, FG_STATUS, "profile loaded successfully");
done:
	rc = fg_sram_read(chip, NOM_CAP_WORD, NOM_CAP_OFFSET, buf, 2,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading %04x[%d] rc=%d\n", NOM_CAP_WORD,
			NOM_CAP_OFFSET, rc);
		goto out;
	}

	chip->nom_cap_uah = (int)(buf[0] | buf[1] << 8) * 1000;
	chip->profile_loaded = true;
out:
	vote(chip->awake_votable, PROFILE_LOAD, false, 0);
	rc = fg_masked_write(chip, BATT_SOC_RESTART(chip), RESTART_GO_BIT, 0);
	if (rc < 0)
		pr_err("Error in writing to %04x, rc=%d\n",
			BATT_SOC_RESTART(chip), rc);
}

/* All getters HERE */

static int fg_decode_value_16b(struct fg_sram_param *sp,
				enum fg_sram_param_id id, int value)
{
	sp[id].value = div_u64((u64)(u16)value * sp[id].numrtr, sp[id].denmtr);
	pr_debug("id: %d raw value: %x decoded value: %x\n", id, value,
		sp[id].value);
	return sp[id].value;
}

static int fg_decode_default(struct fg_sram_param *sp,
				enum fg_sram_param_id id, int value)
{
	return value;
}

static int fg_decode(struct fg_sram_param *sp, enum fg_sram_param_id id,
			int value)
{
	if (!sp[id].decode) {
		pr_err("No decoding function for parameter %d\n", id);
		return -EINVAL;
	}

	return sp[id].decode(sp, id, value);
}

static void fg_encode_voltage_16b(struct fg_sram_param *sp,
				enum fg_sram_param_id id, int val, u8 *buf)
{
	int i, mask = 0xff;
	int64_t temp;

	temp = (int64_t)div_u64((u64)val * sp[id].numrtr, sp[id].denmtr);
	pr_debug("temp: %llx id: %d, val: %d, buf: [ ", temp, id, val);
	for (i = 0; i < sp[id].len; i++) {
		buf[i] = temp & mask;
		temp >>= 8;
		pr_debug("%x ", buf[i]);
	}
	pr_debug("]\n");
}

static void fg_encode_voltage_8b(struct fg_sram_param *sp,
				enum fg_sram_param_id  id, int val, u8 *buf)
{
	int i, mask = 0xff;
	int64_t temp;

	/* Offset is 2.5V */
	val -= 2500;
	temp = (int64_t)div_u64((u64)val * sp[id].numrtr, sp[id].denmtr);
	pr_debug("temp: %llx id: %d, val: %d, buf: [ ", temp, id, val);
	for (i = 0; i < sp[id].len; i++) {
		buf[i] = temp & mask;
		temp >>= 8;
		pr_debug("%x ", buf[i]);
	}
	pr_debug("]\n");
}

static void fg_encode_current(struct fg_sram_param *sp,
				enum fg_sram_param_id  id, int val, u8 *buf)
{
	int i, mask = 0xff;
	int64_t temp;
	s64 current_ma;

	current_ma = -val;
	temp = (int64_t)div_s64(current_ma * sp[id].numrtr, sp[id].denmtr);
	pr_debug("temp: %llx id: %d, val: %d, buf: [ ", temp, id, val);
	for (i = 0; i < sp[id].len; i++) {
		buf[i] = temp & mask;
		temp >>= 8;
		pr_debug("%x ", buf[i]);
	}
	pr_debug("]\n");
}

static void fg_encode_default(struct fg_sram_param *sp,
				enum fg_sram_param_id  id, int val, u8 *buf)
{
	int i, mask = 0xff;
	int64_t temp;

	temp = DIV_ROUND_CLOSEST(val * sp[id].numrtr, sp[id].denmtr);
	pr_debug("temp: %llx id: %d, val: %d, buf: [ ", temp, id, val);
	for (i = 0; i < sp[id].len; i++) {
		buf[i] = temp & mask;
		temp >>= 8;
		pr_debug("%x ", buf[i]);
	}
	pr_debug("]\n");
}

static void fg_encode(struct fg_sram_param *sp, enum fg_sram_param_id id,
			int val, u8 *buf)
{
	if (!sp[id].encode) {
		pr_err("No encoding function for parameter %d\n", id);
		return;
	}

	sp[id].encode(sp, id, val, buf);
}

/*
 * Please make sure *_sram_params table has the entry for the parameter
 * obtained through this function. In addition to address, offset,
 * length from where this SRAM parameter is read, a decode function
 * need to be specified.
 */
static int fg_get_sram_prop(struct fg_chip *chip, enum fg_sram_param_id id,
				int *val)
{
	int temp, rc, i;
	u8 buf[4];

	if (id < 0 || id > FG_SRAM_MAX || chip->sp[id].len > sizeof(buf))
		return -EINVAL;

	rc = fg_sram_read(chip, chip->sp[id].address, chip->sp[id].offset,
		buf, chip->sp[id].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error reading address 0x%04x[%d] rc=%d\n",
			chip->sp[id].address, chip->sp[id].offset, rc);
		return rc;
	}

	for (i = 0, temp = 0; i < chip->sp[id].len; i++)
		temp |= buf[i] << (8 * i);

	*val = fg_decode(chip->sp, id, temp);
	return 0;
}

#define BATT_TEMP_NUMR		1
#define BATT_TEMP_DENR		1
static int fg_get_battery_temp(struct fg_chip *chip, int *val)
{
	int rc = 0;
	u16 temp = 0;
	u8 buf[2];

	rc = fg_read(chip, BATT_INFO_BATT_TEMP_LSB(chip), buf, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			BATT_INFO_BATT_TEMP_LSB(chip), rc);
		return rc;
	}

	temp = ((buf[1] & BATT_TEMP_MSB_MASK) << 8) |
		(buf[0] & BATT_TEMP_LSB_MASK);
	temp = DIV_ROUND_CLOSEST(temp, 4);

	/* Value is in Kelvin; Convert it to deciDegC */
	temp = (temp - 273) * 10;
	*val = temp;
	return 0;
}

#define BATT_ESR_NUMR		244141
#define BATT_ESR_DENR		1000
static int fg_get_battery_esr(struct fg_chip *chip, int *val)
{
	int rc = 0;
	u16 temp = 0;
	u8 buf[2];

	rc = fg_read(chip, BATT_INFO_ESR_LSB(chip), buf, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			BATT_INFO_ESR_LSB(chip), rc);
		return rc;
	}

	if (chip->pmic_rev_id->rev4 < PMICOBALT_V2P0_REV4)
		temp = ((buf[0] & ESR_MSB_MASK) << 8) |
			(buf[1] & ESR_LSB_MASK);
	else
		temp = ((buf[1] & ESR_MSB_MASK) << 8) |
			(buf[0] & ESR_LSB_MASK);

	pr_debug("buf: %x %x temp: %x\n", buf[0], buf[1], temp);
	*val = div_u64((u64)temp * BATT_ESR_NUMR, BATT_ESR_DENR);
	return 0;
}

static int fg_get_battery_resistance(struct fg_chip *chip, int *val)
{
	int rc, esr, rslow;

	rc = fg_get_battery_esr(chip, &esr);
	if (rc < 0) {
		pr_err("failed to get ESR, rc=%d\n", rc);
		return rc;
	}

	rc = fg_get_sram_prop(chip, FG_SRAM_RSLOW, &rslow);
	if (rc < 0) {
		pr_err("failed to get Rslow, rc=%d\n", rc);
		return rc;
	}

	*val = esr + rslow;
	return 0;
}

#define BATT_CURRENT_NUMR	488281
#define BATT_CURRENT_DENR	1000
static int fg_get_battery_current(struct fg_chip *chip, int *val)
{
	int rc = 0;
	int64_t temp = 0;
	u8 buf[2];

	rc = fg_read(chip, BATT_INFO_IBATT_LSB(chip), buf, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			BATT_INFO_IBATT_LSB(chip), rc);
		return rc;
	}

	if (chip->pmic_rev_id->rev4 < PMICOBALT_V2P0_REV4)
		temp = buf[0] << 8 | buf[1];
	else
		temp = buf[1] << 8 | buf[0];

	pr_debug("buf: %x %x temp: %llx\n", buf[0], buf[1], temp);
	/* Sign bit is bit 15 */
	temp = twos_compliment_extend(temp, 15);
	*val = div_s64((s64)temp * BATT_CURRENT_NUMR, BATT_CURRENT_DENR);
	return 0;
}

#define BATT_VOLTAGE_NUMR	122070
#define BATT_VOLTAGE_DENR	1000
static int fg_get_battery_voltage(struct fg_chip *chip, int *val)
{
	int rc = 0;
	u16 temp = 0;
	u8 buf[2];

	rc = fg_read(chip, BATT_INFO_VBATT_LSB(chip), buf, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			BATT_INFO_VBATT_LSB(chip), rc);
		return rc;
	}

	if (chip->pmic_rev_id->rev4 < PMICOBALT_V2P0_REV4)
		temp = buf[0] << 8 | buf[1];
	else
		temp = buf[1] << 8 | buf[0];

	pr_debug("buf: %x %x temp: %x\n", buf[0], buf[1], temp);
	*val = div_u64((u64)temp * BATT_VOLTAGE_NUMR, BATT_VOLTAGE_DENR);
	return 0;
}

#define MAX_TRIES_SOC		5
static int fg_get_msoc_raw(struct fg_chip *chip, int *val)
{
	u8 cap[2];
	int rc, tries = 0;

	while (tries < MAX_TRIES_SOC) {
		rc = fg_read(chip, BATT_SOC_FG_MONOTONIC_SOC(chip), cap, 2);
		if (rc < 0) {
			pr_err("failed to read addr=0x%04x, rc=%d\n",
				BATT_SOC_FG_MONOTONIC_SOC(chip), rc);
			return rc;
		}

		if (cap[0] == cap[1])
			break;

		tries++;
	}

	if (tries == MAX_TRIES_SOC) {
		pr_err("shadow registers do not match\n");
		return -EINVAL;
	}

	fg_dbg(chip, FG_POWER_SUPPLY, "raw: 0x%02x\n", cap[0]);

	*val = cap[0];
	return 0;
}

#define FULL_CAPACITY	100
#define FULL_SOC_RAW	255
static int fg_get_prop_capacity(struct fg_chip *chip, int *val)
{
	int rc, msoc;

	rc = fg_get_msoc_raw(chip, &msoc);
	if (rc < 0)
		return rc;

	*val = DIV_ROUND_CLOSEST(msoc * FULL_CAPACITY, FULL_SOC_RAW);
	return 0;
}

#define DEFAULT_BATT_TYPE	"Unknown Battery"
#define MISSING_BATT_TYPE	"Missing Battery"
#define LOADING_BATT_TYPE	"Loading Battery"
static const char *fg_get_battery_type(struct fg_chip *chip)
{
	if (chip->battery_missing)
		return MISSING_BATT_TYPE;

	if (chip->bp.batt_type_str) {
		if (chip->profile_loaded)
			return chip->bp.batt_type_str;
		else
			return LOADING_BATT_TYPE;
	}

	return DEFAULT_BATT_TYPE;
}

static int fg_get_batt_id(struct fg_chip *chip, int *val)
{
	int rc, batt_id = -EINVAL;

	if (!chip->batt_id_chan)
		return -EINVAL;

	rc = iio_read_channel_processed(chip->batt_id_chan, &batt_id);
	if (rc < 0) {
		pr_err("Error in reading batt_id channel, rc:%d\n", rc);
		return rc;
	}

	chip->batt_id_avail = true;
	fg_dbg(chip, FG_STATUS, "batt_id: %d\n", batt_id);

	*val = batt_id;
	return 0;
}

static int fg_get_batt_profile(struct fg_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *batt_node, *profile_node;
	const char *data;
	int rc, len, batt_id;

	rc = fg_get_batt_id(chip, &batt_id);
	if (rc < 0) {
		pr_err("Error in getting batt_id rc:%d\n", rc);
		return rc;
	}

	batt_node = of_find_node_by_name(node, "qcom,battery-data");
	if (!batt_node) {
		pr_err("Batterydata not available\n");
		return -ENXIO;
	}

	batt_id /= 1000;
	profile_node = of_batterydata_get_best_profile(batt_node, batt_id,
				NULL);
	if (IS_ERR(profile_node))
		return PTR_ERR(profile_node);

	if (!profile_node) {
		pr_err("couldn't find profile handle\n");
		return -ENODATA;
	}

	rc = of_property_read_string(profile_node, "qcom,battery-type",
			&chip->bp.batt_type_str);
	if (rc < 0) {
		pr_err("battery type unavailable, rc:%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(profile_node, "qcom,max-voltage-uv",
			&chip->bp.float_volt_uv);
	if (rc < 0) {
		pr_err("battery float voltage unavailable, rc:%d\n", rc);
		chip->bp.float_volt_uv = -EINVAL;
	}

	rc = of_property_read_u32(profile_node, "qcom,nom-batt-capacity-mah",
			&chip->bp.fastchg_curr_ma);
	if (rc < 0) {
		pr_err("battery nominal capacity unavailable, rc:%d\n", rc);
		chip->bp.fastchg_curr_ma = -EINVAL;
	}

	data = of_get_property(profile_node, "qcom,fg-profile-data", &len);
	if (!data) {
		pr_err("No profile data available\n");
		return -ENODATA;
	}

	if (len != PROFILE_LEN) {
		pr_err("battery profile incorrect size: %d\n", len);
		return -EINVAL;
	}

	memcpy(chip->batt_profile, data, len);
	return 0;
}

static inline void get_temp_setpoint(int threshold, u8 *val)
{
	/* Resolution is 0.5C. Base is -30C. */
	*val = DIV_ROUND_CLOSEST((threshold + 30) * 10, 5);
}

static int fg_set_esr_timer(struct fg_chip *chip, int cycles, bool charging,
				int flags)
{
	u8 buf[2];
	int rc, timer_max, timer_init;

	if (charging) {
		timer_max = FG_SRAM_ESR_TIMER_CHG_MAX;
		timer_init = FG_SRAM_ESR_TIMER_CHG_INIT;
	} else {
		timer_max = FG_SRAM_ESR_TIMER_DISCHG_MAX;
		timer_init = FG_SRAM_ESR_TIMER_DISCHG_INIT;
	}

	fg_encode(chip->sp, timer_max, cycles, buf);
	rc = fg_sram_write(chip,
			chip->sp[timer_max].address,
			chip->sp[timer_max].offset, buf,
			chip->sp[timer_max].len, flags);
	if (rc < 0) {
		pr_err("Error in writing esr_timer_dischg_max, rc=%d\n",
			rc);
		return rc;
	}

	fg_encode(chip->sp, timer_init, cycles, buf);
	rc = fg_sram_write(chip,
			chip->sp[timer_init].address,
			chip->sp[timer_init].offset, buf,
			chip->sp[timer_init].len, flags);
	if (rc < 0) {
		pr_err("Error in writing esr_timer_dischg_init, rc=%d\n",
			rc);
		return rc;
	}

	return 0;
}

/* PSY CALLBACKS STAY HERE */

static int fg_psy_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *pval)
{
	struct fg_chip *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = fg_get_prop_capacity(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = fg_get_battery_voltage(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = fg_get_battery_current(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = fg_get_battery_temp(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		rc = fg_get_battery_resistance(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		rc = fg_get_sram_prop(chip, FG_SRAM_OCV, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		pval->intval = chip->nom_cap_uah;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE_ID:
		rc = fg_get_batt_id(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_BATTERY_TYPE:
		pval->strval = fg_get_battery_type(chip);
		break;
	default:
		break;
	}

	return rc;
}

static int fg_psy_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *pval)
{
	switch (psp) {
	default:
		break;
	}

	return 0;
}

static int fg_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	default:
		break;
	}

	return 0;
}

static void fg_external_power_changed(struct power_supply *psy)
{
	pr_debug("power supply changed\n");
}

static int fg_notifier_cb(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct fg_chip *chip = container_of(nb, struct fg_chip, nb);

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0)
		|| (strcmp(psy->desc->name, "usb") == 0))
		schedule_work(&chip->status_change_work);

	return NOTIFY_OK;
}

static enum power_supply_property fg_psy_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_BATTERY_TYPE,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

static const struct power_supply_desc fg_psy_desc = {
	.name = "bms",
	.type = POWER_SUPPLY_TYPE_BMS,
	.properties = fg_psy_props,
	.num_properties = ARRAY_SIZE(fg_psy_props),
	.get_property = fg_psy_get_property,
	.set_property = fg_psy_set_property,
	.external_power_changed = fg_external_power_changed,
	.property_is_writeable = fg_property_is_writeable,
};

/* INIT FUNCTIONS STAY HERE */

static int fg_hw_init(struct fg_chip *chip)
{
	int rc;
	u8 buf[4], val;

	fg_encode(chip->sp, FG_SRAM_CUTOFF_VOLT, chip->dt.cutoff_volt_mv, buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_CUTOFF_VOLT].address,
			chip->sp[FG_SRAM_CUTOFF_VOLT].offset, buf,
			chip->sp[FG_SRAM_CUTOFF_VOLT].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing cutoff_volt, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_EMPTY_VOLT, chip->dt.empty_volt_mv, buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_EMPTY_VOLT].address,
			chip->sp[FG_SRAM_EMPTY_VOLT].offset, buf,
			chip->sp[FG_SRAM_EMPTY_VOLT].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing empty_volt, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_CHG_TERM_CURR, chip->dt.chg_term_curr_ma,
		buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_CHG_TERM_CURR].address,
			chip->sp[FG_SRAM_CHG_TERM_CURR].offset, buf,
			chip->sp[FG_SRAM_CHG_TERM_CURR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing chg_term_curr, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_SYS_TERM_CURR, chip->dt.sys_term_curr_ma,
		buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_SYS_TERM_CURR].address,
			chip->sp[FG_SRAM_SYS_TERM_CURR].offset, buf,
			chip->sp[FG_SRAM_SYS_TERM_CURR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing sys_term_curr, rc=%d\n", rc);
		return rc;
	}

	if (chip->dt.vbatt_low_thr_mv > 0) {
		fg_encode(chip->sp, FG_SRAM_VBATT_LOW,
			chip->dt.vbatt_low_thr_mv, buf);
		rc = fg_sram_write(chip, chip->sp[FG_SRAM_VBATT_LOW].address,
				chip->sp[FG_SRAM_VBATT_LOW].offset, buf,
				chip->sp[FG_SRAM_VBATT_LOW].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing vbatt_low_thr, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.delta_soc_thr > 0 && chip->dt.delta_soc_thr < 100) {
		fg_encode(chip->sp, FG_SRAM_DELTA_SOC_THR,
			chip->dt.delta_soc_thr, buf);
		rc = fg_sram_write(chip,
				chip->sp[FG_SRAM_DELTA_SOC_THR].address,
				chip->sp[FG_SRAM_DELTA_SOC_THR].offset,
				buf, chip->sp[FG_SRAM_DELTA_SOC_THR].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing delta_soc_thr, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.recharge_soc_thr > 0 && chip->dt.recharge_soc_thr < 100) {
		fg_encode(chip->sp, FG_SRAM_RECHARGE_SOC_THR,
			chip->dt.recharge_soc_thr, buf);
		rc = fg_sram_write(chip,
				chip->sp[FG_SRAM_RECHARGE_SOC_THR].address,
				chip->sp[FG_SRAM_RECHARGE_SOC_THR].offset,
				buf, chip->sp[FG_SRAM_RECHARGE_SOC_THR].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing recharge_soc_thr, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (chip->dt.rsense_sel >= SRC_SEL_BATFET &&
			chip->dt.rsense_sel < SRC_SEL_RESERVED) {
		rc = fg_masked_write(chip, BATT_INFO_IBATT_SENSING_CFG(chip),
				SOURCE_SELECT_MASK, chip->dt.rsense_sel);
		if (rc < 0) {
			pr_err("Error in writing rsense_sel, rc=%d\n", rc);
			return rc;
		}
	}

	get_temp_setpoint(chip->dt.jeita_thresholds[JEITA_COLD], &val);
	rc = fg_write(chip, BATT_INFO_JEITA_TOO_COLD(chip), &val, 1);
	if (rc < 0) {
		pr_err("Error in writing jeita_cold, rc=%d\n", rc);
		return rc;
	}

	get_temp_setpoint(chip->dt.jeita_thresholds[JEITA_COOL], &val);
	rc = fg_write(chip, BATT_INFO_JEITA_COLD(chip), &val, 1);
	if (rc < 0) {
		pr_err("Error in writing jeita_cool, rc=%d\n", rc);
		return rc;
	}

	get_temp_setpoint(chip->dt.jeita_thresholds[JEITA_WARM], &val);
	rc = fg_write(chip, BATT_INFO_JEITA_HOT(chip), &val, 1);
	if (rc < 0) {
		pr_err("Error in writing jeita_warm, rc=%d\n", rc);
		return rc;
	}

	get_temp_setpoint(chip->dt.jeita_thresholds[JEITA_HOT], &val);
	rc = fg_write(chip, BATT_INFO_JEITA_TOO_HOT(chip), &val, 1);
	if (rc < 0) {
		pr_err("Error in writing jeita_hot, rc=%d\n", rc);
		return rc;
	}

	if (chip->dt.esr_timer_charging > 0) {
		rc = fg_set_esr_timer(chip, chip->dt.esr_timer_charging, true,
				      FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in setting ESR timer, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.esr_timer_awake > 0) {
		rc = fg_set_esr_timer(chip, chip->dt.esr_timer_awake, false,
				      FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in setting ESR timer, rc=%d\n", rc);
			return rc;
		}
	}

	return 0;
}

static int fg_memif_init(struct fg_chip *chip)
{
	return fg_ima_init(chip);
}

static int fg_batt_profile_init(struct fg_chip *chip)
{
	int rc;

	if (!chip->batt_profile) {
		chip->batt_profile = devm_kcalloc(chip->dev, PROFILE_LEN,
						sizeof(*chip->batt_profile),
						GFP_KERNEL);
		if (!chip->batt_profile)
			return -ENOMEM;
	}

	rc = fg_get_batt_profile(chip);
	if (rc < 0) {
		pr_err("Error in getting battery profile, rc:%d\n", rc);
		return rc;
	}

	schedule_delayed_work(&chip->profile_load_work, msecs_to_jiffies(0));
	return 0;
}

/* INTERRUPT HANDLERS STAY HERE */

static irqreturn_t fg_vbatt_low_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	return IRQ_HANDLED;
}

static irqreturn_t fg_batt_missing_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;
	u8 status;
	int rc;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	rc = fg_read(chip, BATT_INFO_INT_RT_STS(chip), &status, 1);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			BATT_INFO_INT_RT_STS(chip), rc);
		return IRQ_HANDLED;
	}

	chip->battery_missing = (status & BT_MISS_BIT);

	if (chip->battery_missing) {
		chip->batt_id_avail = false;
		chip->profile_loaded = false;
	} else {
		rc = fg_batt_profile_init(chip);
		if (rc < 0) {
			pr_err("Error in initializing battery profile, rc=%d\n",
				rc);
			return IRQ_HANDLED;
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t fg_delta_batt_temp_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	return IRQ_HANDLED;
}

static irqreturn_t fg_first_est_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	complete_all(&chip->soc_ready);
	return IRQ_HANDLED;
}

static irqreturn_t fg_soc_update_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	complete_all(&chip->soc_update);
	return IRQ_HANDLED;
}

static irqreturn_t fg_delta_soc_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	return IRQ_HANDLED;
}

static irqreturn_t fg_empty_soc_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	return IRQ_HANDLED;
}

static irqreturn_t fg_soc_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	return IRQ_HANDLED;
}

static irqreturn_t fg_dummy_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	return IRQ_HANDLED;
}

static struct fg_irq_info fg_irqs[FG_IRQ_MAX] = {
	/* BATT_SOC irqs */
	[MSOC_FULL_IRQ] = {
		"msoc-full", fg_soc_irq_handler, true },
	[MSOC_HIGH_IRQ] = {
		"msoc-high", fg_soc_irq_handler, true },
	[MSOC_EMPTY_IRQ] = {
		"msoc-empty", fg_empty_soc_irq_handler, true },
	[MSOC_LOW_IRQ] = {
		"msoc-low", fg_soc_irq_handler },
	[MSOC_DELTA_IRQ] = {
		"msoc-delta", fg_delta_soc_irq_handler, true },
	[BSOC_DELTA_IRQ] = {
		"bsoc-delta", fg_delta_soc_irq_handler, true },
	[SOC_READY_IRQ] = {
		"soc-ready", fg_first_est_irq_handler, true },
	[SOC_UPDATE_IRQ] = {
		"soc-update", fg_soc_update_irq_handler },
	/* BATT_INFO irqs */
	[BATT_TEMP_DELTA_IRQ] = {
		"batt-temp-delta", fg_delta_batt_temp_irq_handler },
	[BATT_MISSING_IRQ] = {
		"batt-missing", fg_batt_missing_irq_handler, true },
	[ESR_DELTA_IRQ] = {
		"esr-delta", fg_dummy_irq_handler },
	[VBATT_LOW_IRQ] = {
		"vbatt-low", fg_vbatt_low_irq_handler, true },
	[VBATT_PRED_DELTA_IRQ] = {
		"vbatt-pred-delta", fg_dummy_irq_handler },
	/* MEM_IF irqs */
	[DMA_GRANT_IRQ] = {
		"dma-grant", fg_dummy_irq_handler },
	[MEM_XCP_IRQ] = {
		"mem-xcp", fg_dummy_irq_handler },
	[IMA_RDY_IRQ] = {
		"ima-rdy", fg_dummy_irq_handler },
};

static int fg_get_irq_index_byname(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fg_irqs); i++) {
		if (strcmp(fg_irqs[i].name, name) == 0)
			return i;
	}

	pr_err("%s is not in irq list\n", name);
	return -ENOENT;
}

static int fg_register_interrupts(struct fg_chip *chip)
{
	struct device_node *child, *node = chip->dev->of_node;
	struct property *prop;
	const char *name;
	int rc, irq, irq_index;

	for_each_available_child_of_node(node, child) {
		of_property_for_each_string(child, "interrupt-names", prop,
						name) {
			irq = of_irq_get_byname(child, name);
			if (irq < 0) {
				dev_err(chip->dev, "failed to get irq %s irq:%d\n",
					name, irq);
				return irq;
			}

			irq_index = fg_get_irq_index_byname(name);
			if (irq_index < 0)
				return irq_index;

			rc = devm_request_threaded_irq(chip->dev, irq, NULL,
					fg_irqs[irq_index].handler,
					IRQF_ONESHOT, name, chip);
			if (rc < 0) {
				dev_err(chip->dev, "failed to register irq handler for %s rc:%d\n",
					name, rc);
				return rc;
			}

			fg_irqs[irq_index].irq = irq;
			if (fg_irqs[irq_index].wakeable)
				enable_irq_wake(fg_irqs[irq_index].irq);
		}
	}

	return 0;
}

#define DEFAULT_CUTOFF_VOLT_MV		3200
#define DEFAULT_EMPTY_VOLT_MV		3100
#define DEFAULT_CHG_TERM_CURR_MA	100
#define DEFAULT_SYS_TERM_CURR_MA	125
#define DEFAULT_DELTA_SOC_THR		1
#define DEFAULT_RECHARGE_SOC_THR	95
#define DEFAULT_BATT_TEMP_COLD		0
#define DEFAULT_BATT_TEMP_COOL		5
#define DEFAULT_BATT_TEMP_WARM		45
#define DEFAULT_BATT_TEMP_HOT		50
static int fg_parse_dt(struct fg_chip *chip)
{
	struct device_node *child, *revid_node, *node = chip->dev->of_node;
	u32 base, temp;
	u8 subtype;
	int rc, len;

	if (!node)  {
		dev_err(chip->dev, "device tree node missing\n");
		return -ENXIO;
	}

	revid_node = of_parse_phandle(node, "qcom,pmic-revid", 0);
	if (!revid_node) {
		pr_err("Missing qcom,pmic-revid property - driver failed\n");
		return -EINVAL;
	}

	chip->pmic_rev_id = get_revid_data(revid_node);
	if (IS_ERR_OR_NULL(chip->pmic_rev_id)) {
		pr_err("Unable to get pmic_revid rc=%ld\n",
			PTR_ERR(chip->pmic_rev_id));
		/*
		 * the revid peripheral must be registered, any failure
		 * here only indicates that the rev-id module has not
		 * probed yet.
		 */
		return -EPROBE_DEFER;
	}

	pr_debug("PMIC subtype %d Digital major %d\n",
		chip->pmic_rev_id->pmic_subtype, chip->pmic_rev_id->rev4);

	switch (chip->pmic_rev_id->pmic_subtype) {
	case PMICOBALT_SUBTYPE:
		if (chip->pmic_rev_id->rev4 < PMICOBALT_V2P0_REV4)
			chip->sp = pmicobalt_v1_sram_params;
		else
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	chip->batt_id_chan = iio_channel_get(chip->dev, "rradc_batt_id");
	if (IS_ERR(chip->batt_id_chan)) {
		if (PTR_ERR(chip->batt_id_chan) != -EPROBE_DEFER)
			pr_err("batt_id_chan unavailable %ld\n",
				PTR_ERR(chip->batt_id_chan));
		rc = PTR_ERR(chip->batt_id_chan);
		chip->batt_id_chan = NULL;
		return rc;
	}

	if (of_get_available_child_count(node) == 0) {
		dev_err(chip->dev, "No child nodes specified!\n");
		return -ENXIO;
	}

	for_each_available_child_of_node(node, child) {
		rc = of_property_read_u32(child, "reg", &base);
		if (rc < 0) {
			dev_err(chip->dev, "reg not specified in node %s, rc=%d\n",
				child->full_name, rc);
			return rc;
		}

		rc = fg_read(chip, base + PERPH_SUBTYPE_REG, &subtype, 1);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't read subtype for base %d, rc=%d\n",
				base, rc);
			return rc;
		}

		switch (subtype) {
		case FG_BATT_SOC_PMICOBALT:
			chip->batt_soc_base = base;
			break;
		case FG_BATT_INFO_PMICOBALT:
			chip->batt_info_base = base;
			break;
		case FG_MEM_INFO_PMICOBALT:
			chip->mem_if_base = base;
			break;
		default:
			dev_err(chip->dev, "Invalid peripheral subtype 0x%x\n",
				subtype);
			return -ENXIO;
		}
	}

	/* Read all the optional properties below */
	rc = of_property_read_u32(node, "qcom,fg-cutoff-voltage", &temp);
	if (rc < 0)
		chip->dt.cutoff_volt_mv = DEFAULT_CUTOFF_VOLT_MV;
	else
		chip->dt.cutoff_volt_mv = temp;

	rc = of_property_read_u32(node, "qcom,fg-empty-voltage", &temp);
	if (rc < 0)
		chip->dt.empty_volt_mv = DEFAULT_EMPTY_VOLT_MV;
	else
		chip->dt.empty_volt_mv = temp;

	rc = of_property_read_u32(node, "qcom,fg-vbatt-low-thr", &temp);
	if (rc < 0)
		chip->dt.vbatt_low_thr_mv = -EINVAL;
	else
		chip->dt.vbatt_low_thr_mv = temp;

	rc = of_property_read_u32(node, "qcom,fg-chg-term-current", &temp);
	if (rc < 0)
		chip->dt.chg_term_curr_ma = DEFAULT_CHG_TERM_CURR_MA;
	else
		chip->dt.chg_term_curr_ma = temp;

	rc = of_property_read_u32(node, "qcom,fg-sys-term-current", &temp);
	if (rc < 0)
		chip->dt.sys_term_curr_ma = DEFAULT_SYS_TERM_CURR_MA;
	else
		chip->dt.sys_term_curr_ma = temp;

	rc = of_property_read_u32(node, "qcom,fg-delta-soc-thr", &temp);
	if (rc < 0)
		chip->dt.delta_soc_thr = DEFAULT_DELTA_SOC_THR;
	else
		chip->dt.delta_soc_thr = temp;

	rc = of_property_read_u32(node, "qcom,fg-recharge-soc-thr", &temp);
	if (rc < 0)
		chip->dt.recharge_soc_thr = DEFAULT_RECHARGE_SOC_THR;
	else
		chip->dt.recharge_soc_thr = temp;

	rc = of_property_read_u32(node, "qcom,fg-rsense-sel", &temp);
	if (rc < 0)
		chip->dt.rsense_sel = SRC_SEL_BATFET_SMB;
	else
		chip->dt.rsense_sel = (u8)temp & SOURCE_SELECT_MASK;

	chip->dt.jeita_thresholds[JEITA_COLD] = DEFAULT_BATT_TEMP_COLD;
	chip->dt.jeita_thresholds[JEITA_COOL] = DEFAULT_BATT_TEMP_COOL;
	chip->dt.jeita_thresholds[JEITA_WARM] = DEFAULT_BATT_TEMP_WARM;
	chip->dt.jeita_thresholds[JEITA_HOT] = DEFAULT_BATT_TEMP_HOT;
	if (of_find_property(node, "qcom,fg-jeita-thresholds", &len)) {
		if (len == NUM_JEITA_LEVELS) {
			rc = of_property_read_u32_array(node,
					"qcom,fg-jeita-thresholds",
					chip->dt.jeita_thresholds, len);
			if (rc < 0)
				pr_warn("Error reading Jeita thresholds, default values will be used rc:%d\n",
					rc);
		}
	}

	rc = of_property_read_u32(node, "qcom,fg-esr-timer-charging", &temp);
	if (rc < 0)
		chip->dt.esr_timer_charging = -EINVAL;
	else
		chip->dt.esr_timer_charging = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-timer-awake", &temp);
	if (rc < 0)
		chip->dt.esr_timer_awake = -EINVAL;
	else
		chip->dt.esr_timer_awake = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-timer-asleep", &temp);
	if (rc < 0)
		chip->dt.esr_timer_asleep = -EINVAL;
	else
		chip->dt.esr_timer_asleep = temp;


	return 0;
}

static void fg_cleanup(struct fg_chip *chip)
{
	power_supply_unreg_notifier(&chip->nb);
	debugfs_remove_recursive(chip->dentry);
	if (chip->awake_votable)
		destroy_votable(chip->awake_votable);

	if (chip->batt_id_chan)
		iio_channel_release(chip->batt_id_chan);

	dev_set_drvdata(chip->dev, NULL);
}

static int fg_gen3_probe(struct platform_device *pdev)
{
	struct fg_chip *chip;
	struct power_supply_config fg_psy_cfg;
	int rc;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	chip->debug_mask = &fg_gen3_debug_mask;
	chip->irqs = fg_irqs;
	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		dev_err(chip->dev, "Parent regmap is unavailable\n");
		return -ENXIO;
	}

	rc = fg_parse_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Error in reading DT parameters, rc:%d\n",
			rc);
		return rc;
	}

	chip->awake_votable = create_votable("FG_WS", VOTE_SET_ANY, fg_awake_cb,
					chip);
	if (IS_ERR(chip->awake_votable)) {
		rc = PTR_ERR(chip->awake_votable);
		return rc;
	}

	mutex_init(&chip->bus_lock);
	mutex_init(&chip->sram_rw_lock);
	init_completion(&chip->soc_update);
	init_completion(&chip->soc_ready);
	INIT_DELAYED_WORK(&chip->profile_load_work, profile_load_work);
	INIT_WORK(&chip->status_change_work, status_change_work);

	rc = fg_memif_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Error in initializing FG_MEMIF, rc:%d\n",
			rc);
		goto exit;
	}

	rc = fg_hw_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Error in initializing FG hardware, rc:%d\n",
			rc);
		goto exit;
	}

	platform_set_drvdata(pdev, chip);

	/* Register the power supply */
	fg_psy_cfg.drv_data = chip;
	fg_psy_cfg.of_node = NULL;
	fg_psy_cfg.supplied_to = NULL;
	fg_psy_cfg.num_supplicants = 0;
	chip->fg_psy = devm_power_supply_register(chip->dev, &fg_psy_desc,
			&fg_psy_cfg);
	if (IS_ERR(chip->fg_psy)) {
		pr_err("failed to register fg_psy rc = %ld\n",
				PTR_ERR(chip->fg_psy));
		goto exit;
	}

	chip->nb.notifier_call = fg_notifier_cb;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		goto exit;
	}

	rc = fg_register_interrupts(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Error in registering interrupts, rc:%d\n",
			rc);
		goto exit;
	}

	/* Keep SOC_UPDATE irq disabled until we require it */
	if (fg_irqs[SOC_UPDATE_IRQ].irq)
		disable_irq_nosync(fg_irqs[SOC_UPDATE_IRQ].irq);

	rc = fg_sram_debugfs_create(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Error in creating debugfs entries, rc:%d\n",
			rc);
		goto exit;
	}

	rc = fg_batt_profile_init(chip);
	if (rc < 0)
		dev_warn(chip->dev, "Error in initializing battery profile, rc:%d\n",
			rc);

	device_init_wakeup(chip->dev, true);
	pr_debug("FG GEN3 driver successfully probed\n");
	return 0;
exit:
	fg_cleanup(chip);
	return rc;
}

static int fg_gen3_suspend(struct device *dev)
{
	struct fg_chip *chip = dev_get_drvdata(dev);
	int rc;

	if (chip->dt.esr_timer_awake > 0 && chip->dt.esr_timer_asleep > 0) {
		rc = fg_set_esr_timer(chip, chip->dt.esr_timer_asleep, false,
				      FG_IMA_NO_WLOCK);
		if (rc < 0) {
			pr_err("Error in setting ESR timer during suspend, rc=%d\n",
			       rc);
			return rc;
		}
	}

	return 0;
}

static int fg_gen3_resume(struct device *dev)
{
	struct fg_chip *chip = dev_get_drvdata(dev);
	int rc;

	if (chip->dt.esr_timer_awake > 0 && chip->dt.esr_timer_asleep > 0) {
		rc = fg_set_esr_timer(chip, chip->dt.esr_timer_awake, false,
				      FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in setting ESR timer during resume, rc=%d\n",
			       rc);
			return rc;
		}
	}

	return 0;
}

static const struct dev_pm_ops fg_gen3_pm_ops = {
	.suspend	= fg_gen3_suspend,
	.resume		= fg_gen3_resume,
};

static int fg_gen3_remove(struct platform_device *pdev)
{
	struct fg_chip *chip = dev_get_drvdata(&pdev->dev);

	fg_cleanup(chip);
	return 0;
}

static const struct of_device_id fg_gen3_match_table[] = {
	{.compatible = FG_GEN3_DEV_NAME},
	{},
};

static struct platform_driver fg_gen3_driver = {
	.driver = {
		.name = FG_GEN3_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = fg_gen3_match_table,
		.pm		= &fg_gen3_pm_ops,
	},
	.probe		= fg_gen3_probe,
	.remove		= fg_gen3_remove,
};

static int __init fg_gen3_init(void)
{
	return platform_driver_register(&fg_gen3_driver);
}

static void __exit fg_gen3_exit(void)
{
	return platform_driver_unregister(&fg_gen3_driver);
}

module_init(fg_gen3_init);
module_exit(fg_gen3_exit);

MODULE_DESCRIPTION("QPNP Fuel gauge GEN3 driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" FG_GEN3_DEV_NAME);
