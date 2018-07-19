/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"QG-K: %s: " fmt, __func__

#include <linux/alarmtimer.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_batterydata.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pmic-voter.h>
#include <linux/iio/consumer.h>
#include <linux/qpnp/qpnp-revid.h>
#include <uapi/linux/qg.h>
#include <uapi/linux/qg-profile.h>
#include "fg-alg.h"
#include "qg-sdam.h"
#include "qg-core.h"
#include "qg-reg.h"
#include "qg-util.h"
#include "qg-soc.h"
#include "qg-battery-profile.h"
#include "qg-defs.h"

static int qg_debug_mask;
module_param_named(
	debug_mask, qg_debug_mask, int, 0600
);

static int qg_esr_mod_count = 30;
module_param_named(
	esr_mod_count, qg_esr_mod_count, int, 0600
);

static int qg_esr_count = 3;
module_param_named(
	esr_count, qg_esr_count, int, 0600
);

static bool is_battery_present(struct qpnp_qg *chip)
{
	u8 reg = 0;
	int rc;

	rc = qg_read(chip, chip->qg_base + QG_STATUS1_REG, &reg, 1);
	if (rc < 0)
		pr_err("Failed to read battery presence, rc=%d\n", rc);

	return !!(reg & BATTERY_PRESENT_BIT);
}

#define DEBUG_BATT_ID_LOW	6000
#define DEBUG_BATT_ID_HIGH	8500
static bool is_debug_batt_id(struct qpnp_qg *chip)
{
	if (is_between(DEBUG_BATT_ID_LOW, DEBUG_BATT_ID_HIGH,
					chip->batt_id_ohm))
		return true;

	return false;
}

static int qg_read_ocv(struct qpnp_qg *chip, u32 *ocv_uv, u32 *ocv_raw, u8 type)
{
	int rc, addr;
	u64 temp = 0;
	char ocv_name[20];

	switch (type) {
	case S3_GOOD_OCV:
		addr = QG_S3_GOOD_OCV_V_DATA0_REG;
		strlcpy(ocv_name, "S3_GOOD_OCV", 20);
		break;
	case S7_PON_OCV:
		addr = QG_S7_PON_OCV_V_DATA0_REG;
		strlcpy(ocv_name, "S7_PON_OCV", 20);
		break;
	case S3_LAST_OCV:
		addr = QG_LAST_S3_SLEEP_V_DATA0_REG;
		strlcpy(ocv_name, "S3_LAST_OCV", 20);
		break;
	case SDAM_PON_OCV:
		addr = QG_SDAM_PON_OCV_OFFSET;
		strlcpy(ocv_name, "SDAM_PON_OCV", 20);
		break;
	default:
		pr_err("Invalid OCV type %d\n", type);
		return -EINVAL;
	}

	if (type == SDAM_PON_OCV) {
		rc = qg_sdam_read(SDAM_PON_OCV_UV, ocv_raw);
		if (rc < 0) {
			pr_err("Failed to read SDAM PON OCV rc=%d\n", rc);
			return rc;
		}
	} else {
		rc = qg_read(chip, chip->qg_base + addr, (u8 *)ocv_raw, 2);
		if (rc < 0) {
			pr_err("Failed to read ocv, rc=%d\n", rc);
			return rc;
		}
	}

	temp = *ocv_raw;
	*ocv_uv = V_RAW_TO_UV(temp);

	pr_debug("%s: OCV_RAW=%x OCV=%duV\n", ocv_name, *ocv_raw, *ocv_uv);

	return rc;
}

#define DEFAULT_S3_FIFO_LENGTH		3
static int qg_update_fifo_length(struct qpnp_qg *chip, u8 length)
{
	int rc;
	u8 s3_entry_fifo_length = 0;

	if (!length || length > 8) {
		pr_err("Invalid FIFO length %d\n", length);
		return -EINVAL;
	}

	rc = qg_masked_write(chip, chip->qg_base + QG_S2_NORMAL_MEAS_CTL2_REG,
			FIFO_LENGTH_MASK, (length - 1) << FIFO_LENGTH_SHIFT);
	if (rc < 0)
		pr_err("Failed to write S2 FIFO length, rc=%d\n", rc);

	/* update the S3 FIFO length, when S2 length is updated */
	if (length > 3)
		s3_entry_fifo_length = (chip->dt.s3_entry_fifo_length > 0) ?
			chip->dt.s3_entry_fifo_length : DEFAULT_S3_FIFO_LENGTH;
	else	/* Use S3 length as 1 for any S2 length <= 3 */
		s3_entry_fifo_length = 1;

	rc = qg_masked_write(chip,
			chip->qg_base + QG_S3_SLEEP_OCV_IBAT_CTL1_REG,
			SLEEP_IBAT_QUALIFIED_LENGTH_MASK,
			s3_entry_fifo_length - 1);
	if (rc < 0)
		pr_err("Failed to write S3-entry fifo-length, rc=%d\n",
						rc);

	return rc;
}

static int qg_master_hold(struct qpnp_qg *chip, bool hold)
{
	int rc;

	/* clear the master */
	rc = qg_masked_write(chip, chip->qg_base + QG_DATA_CTL1_REG,
					MASTER_HOLD_OR_CLR_BIT, 0);
	if (rc < 0)
		return rc;

	if (hold) {
		/* 0 -> 1, hold the master */
		rc = qg_masked_write(chip, chip->qg_base + QG_DATA_CTL1_REG,
					MASTER_HOLD_OR_CLR_BIT,
					MASTER_HOLD_OR_CLR_BIT);
		if (rc < 0)
			return rc;
	}

	qg_dbg(chip, QG_DEBUG_STATUS, "Master hold = %d\n", hold);

	return rc;
}

static void qg_notify_charger(struct qpnp_qg *chip)
{
	union power_supply_propval prop = {0, };
	int rc;

	if (!chip->batt_psy)
		return;

	if (is_debug_batt_id(chip)) {
		prop.intval = 1;
		power_supply_set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_DEBUG_BATTERY, &prop);
		return;
	}

	if (!chip->profile_loaded)
		return;

	prop.intval = chip->bp.float_volt_uv;
	rc = power_supply_set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &prop);
	if (rc < 0) {
		pr_err("Failed to set voltage_max property on batt_psy, rc=%d\n",
			rc);
		return;
	}

	prop.intval = chip->bp.fastchg_curr_ma * 1000;
	rc = power_supply_set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &prop);
	if (rc < 0) {
		pr_err("Failed to set constant_charge_current_max property on batt_psy, rc=%d\n",
			rc);
		return;
	}

	pr_debug("Notified charger on float voltage and FCC\n");

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &prop);
	if (rc < 0) {
		pr_err("Failed to get charge term current, rc=%d\n", rc);
		return;
	}
	chip->chg_iterm_ma = prop.intval;
}

static bool is_batt_available(struct qpnp_qg *chip)
{
	if (chip->batt_psy)
		return true;

	chip->batt_psy = power_supply_get_by_name("battery");
	if (!chip->batt_psy)
		return false;

	/* batt_psy is initialized, set the fcc and fv */
	qg_notify_charger(chip);

	return true;
}

static int qg_store_soc_params(struct qpnp_qg *chip)
{
	int rc, batt_temp = 0, i;
	unsigned long rtc_sec = 0;

	rc = get_rtc_time(&rtc_sec);
	if (rc < 0)
		pr_err("Failed to get RTC time, rc=%d\n", rc);
	else
		chip->sdam_data[SDAM_TIME_SEC] = rtc_sec;

	rc = qg_get_battery_temp(chip, &batt_temp);
	if (rc < 0)
		pr_err("Failed to get battery-temp, rc = %d\n", rc);
	else
		chip->sdam_data[SDAM_TEMP] = (u32)batt_temp;

	for (i = 0; i <= SDAM_TIME_SEC; i++) {
		rc |= qg_sdam_write(i, chip->sdam_data[i]);
		qg_dbg(chip, QG_DEBUG_STATUS, "SDAM write param %d value=%d\n",
					i, chip->sdam_data[i]);
	}

	return rc;
}

static int qg_process_fifo(struct qpnp_qg *chip, u32 fifo_length)
{
	int rc = 0, i, j = 0, temp;
	u8 v_fifo[MAX_FIFO_LENGTH * 2], i_fifo[MAX_FIFO_LENGTH * 2];
	u32 sample_interval = 0, sample_count = 0, fifo_v = 0, fifo_i = 0;
	unsigned long rtc_sec = 0;

	rc = get_rtc_time(&rtc_sec);
	if (rc < 0)
		pr_err("Failed to get RTC time, rc=%d\n", rc);

	chip->kdata.fifo_time = (u32)rtc_sec;

	if (!fifo_length) {
		pr_debug("No FIFO data\n");
		return 0;
	}

	qg_dbg(chip, QG_DEBUG_FIFO, "FIFO length=%d\n", fifo_length);

	rc = get_sample_interval(chip, &sample_interval);
	if (rc < 0) {
		pr_err("Failed to get FIFO sample interval, rc=%d\n", rc);
		return rc;
	}

	rc = get_sample_count(chip, &sample_count);
	if (rc < 0) {
		pr_err("Failed to get FIFO sample count, rc=%d\n", rc);
		return rc;
	}

	/*
	 * If there is pending data from suspend, append the new FIFO
	 * data to it.
	 */
	if (chip->suspend_data) {
		j = chip->kdata.fifo_length; /* append the data */
		chip->suspend_data = false;
		qg_dbg(chip, QG_DEBUG_FIFO,
			"Pending suspend-data FIFO length=%d\n", j);
	} else {
		/* clear any old pending data */
		chip->kdata.fifo_length = 0;
	}

	for (i = 0; i < fifo_length * 2; i = i + 2, j++) {
		rc = qg_read(chip, chip->qg_base + QG_V_FIFO0_DATA0_REG + i,
					&v_fifo[i], 2);
		if (rc < 0) {
			pr_err("Failed to read QG_V_FIFO, rc=%d\n", rc);
			return rc;
		}
		rc = qg_read(chip, chip->qg_base + QG_I_FIFO0_DATA0_REG + i,
					&i_fifo[i], 2);
		if (rc < 0) {
			pr_err("Failed to read QG_I_FIFO, rc=%d\n", rc);
			return rc;
		}

		fifo_v = v_fifo[i] | (v_fifo[i + 1] << 8);
		fifo_i = i_fifo[i] | (i_fifo[i + 1] << 8);

		if (fifo_v == FIFO_V_RESET_VAL || fifo_i == FIFO_I_RESET_VAL) {
			pr_err("Invalid FIFO data V_RAW=%x I_RAW=%x - FIFO rejected\n",
						fifo_v, fifo_i);
			return -EINVAL;
		}

		temp = sign_extend32(fifo_i, 15);

		chip->kdata.fifo[j].v = V_RAW_TO_UV(fifo_v);
		chip->kdata.fifo[j].i = I_RAW_TO_UA(temp);
		chip->kdata.fifo[j].interval = sample_interval;
		chip->kdata.fifo[j].count = sample_count;

		qg_dbg(chip, QG_DEBUG_FIFO, "FIFO %d raw_v=%d uV=%d raw_i=%d uA=%d interval=%d count=%d\n",
					j, fifo_v,
					chip->kdata.fifo[j].v,
					fifo_i,
					(int)chip->kdata.fifo[j].i,
					chip->kdata.fifo[j].interval,
					chip->kdata.fifo[j].count);
	}

	chip->kdata.fifo_length += fifo_length;
	chip->kdata.seq_no = chip->seq_no++ % U32_MAX;

	return rc;
}

static int qg_process_accumulator(struct qpnp_qg *chip)
{
	int rc, sample_interval = 0;
	u8 count, index = chip->kdata.fifo_length;
	u64 acc_v = 0, acc_i = 0;
	s64 temp = 0;

	rc = qg_read(chip, chip->qg_base + QG_ACCUM_CNT_RT_REG,
			&count, 1);
	if (rc < 0) {
		pr_err("Failed to read ACC count, rc=%d\n", rc);
		return rc;
	}

	if (!count) {
		pr_debug("No ACCUMULATOR data!\n");
		return 0;
	}

	rc = get_sample_interval(chip, &sample_interval);
	if (rc < 0) {
		pr_err("Failed to get ACC sample interval, rc=%d\n", rc);
		return 0;
	}

	rc = qg_read(chip, chip->qg_base + QG_V_ACCUM_DATA0_RT_REG,
			(u8 *)&acc_v, 3);
	if (rc < 0) {
		pr_err("Failed to read ACC RT V data, rc=%d\n", rc);
		return rc;
	}

	rc = qg_read(chip, chip->qg_base + QG_I_ACCUM_DATA0_RT_REG,
			(u8 *)&acc_i, 3);
	if (rc < 0) {
		pr_err("Failed to read ACC RT I data, rc=%d\n", rc);
		return rc;
	}

	temp = sign_extend64(acc_i, 23);

	chip->kdata.fifo[index].v = V_RAW_TO_UV(div_u64(acc_v, count));
	chip->kdata.fifo[index].i = I_RAW_TO_UA(div_s64(temp, count));
	chip->kdata.fifo[index].interval = sample_interval;
	chip->kdata.fifo[index].count = count;
	chip->kdata.fifo_length++;

	if (chip->kdata.fifo_length == 1)	/* Only accumulator data */
		chip->kdata.seq_no = chip->seq_no++ % U32_MAX;

	qg_dbg(chip, QG_DEBUG_FIFO, "ACC v_avg=%duV i_avg=%duA interval=%d count=%d\n",
			chip->kdata.fifo[index].v,
			(int)chip->kdata.fifo[index].i,
			chip->kdata.fifo[index].interval,
			chip->kdata.fifo[index].count);

	return rc;
}

static int qg_process_rt_fifo(struct qpnp_qg *chip)
{
	int rc;
	u32 fifo_length = 0;

	/* Get the real-time FIFO length */
	rc = get_fifo_length(chip, &fifo_length, true);
	if (rc < 0) {
		pr_err("Failed to read RT FIFO length, rc=%d\n", rc);
		return rc;
	}

	rc = qg_process_fifo(chip, fifo_length);
	if (rc < 0) {
		pr_err("Failed to process FIFO data, rc=%d\n", rc);
		return rc;
	}

	rc = qg_process_accumulator(chip);
	if (rc < 0) {
		pr_err("Failed to process ACC data, rc=%d\n", rc);
		return rc;
	}

	return rc;
}

#define MIN_FIFO_FULL_TIME_MS			12000
static int process_rt_fifo_data(struct qpnp_qg *chip, bool update_smb)
{
	int rc = 0;
	ktime_t now = ktime_get();
	s64 time_delta;

	/*
	 * Reject the FIFO read event if there are back-to-back requests
	 * This is done to gaurantee that there is always a minimum FIFO
	 * data to be processed, ignore this if vbat_low is set.
	 */
	time_delta = ktime_ms_delta(now, chip->last_user_update_time);

	qg_dbg(chip, QG_DEBUG_FIFO, "time_delta=%lld ms update_smb=%d\n",
				time_delta, update_smb);

	if (time_delta > MIN_FIFO_FULL_TIME_MS || update_smb) {
		rc = qg_master_hold(chip, true);
		if (rc < 0) {
			pr_err("Failed to hold master, rc=%d\n", rc);
			goto done;
		}

		rc = qg_process_rt_fifo(chip);
		if (rc < 0) {
			pr_err("Failed to process FIFO real-time, rc=%d\n", rc);
			goto done;
		}

		if (update_smb) {
			rc = qg_masked_write(chip, chip->qg_base +
				QG_MODE_CTL1_REG, PARALLEL_IBAT_SENSE_EN_BIT,
				chip->parallel_enabled ?
					PARALLEL_IBAT_SENSE_EN_BIT : 0);
			if (rc < 0) {
				pr_err("Failed to update SMB_EN, rc=%d\n", rc);
				goto done;
			}
			qg_dbg(chip, QG_DEBUG_STATUS, "Parallel SENSE %d\n",
						chip->parallel_enabled);
		}

		rc = qg_master_hold(chip, false);
		if (rc < 0) {
			pr_err("Failed to release master, rc=%d\n", rc);
			goto done;
		}
		/* FIFOs restarted */
		chip->last_fifo_update_time = ktime_get();

		/* signal the read thread */
		chip->data_ready = true;
		wake_up_interruptible(&chip->qg_wait_q);
		chip->last_user_update_time = now;

		/* vote to stay awake until userspace reads data */
		vote(chip->awake_votable, FIFO_RT_DONE_VOTER, true, 0);
	} else {
		qg_dbg(chip, QG_DEBUG_FIFO, "FIFO processing too early time_delta=%lld\n",
							time_delta);
	}
done:
	qg_master_hold(chip, false);
	return rc;
}

#define VBAT_LOW_HYST_UV		50000 /* 50mV */
static int qg_vbat_low_wa(struct qpnp_qg *chip)
{
	int rc, i, temp = 0;
	u32 vbat_low_uv = 0, fifo_length = 0;

	if ((chip->wa_flags & QG_VBAT_LOW_WA) && chip->vbat_low) {
		rc = qg_get_battery_temp(chip, &temp);
		if (rc < 0) {
			pr_err("Failed to read batt_temp rc=%d\n", rc);
			temp = 250;
		}

		vbat_low_uv = 1000 * ((temp < chip->dt.cold_temp_threshold) ?
					chip->dt.vbatt_low_cold_mv :
					chip->dt.vbatt_low_mv);
		vbat_low_uv += VBAT_LOW_HYST_UV;
		/*
		 * PMI632 1.0 does not generate a falling VBAT_LOW IRQ.
		 * To exit from VBAT_LOW config, check if any of the FIFO
		 * averages is > vbat_low threshold and reconfigure the
		 * FIFO length to normal.
		 */
		for (i = 0; i < chip->kdata.fifo_length; i++) {
			if (chip->kdata.fifo[i].v > vbat_low_uv) {
				chip->vbat_low = false;
				pr_info("Exit VBAT_LOW vbat_avg=%duV vbat_low=%duV updated fifo_length=%d\n",
					chip->kdata.fifo[i].v, vbat_low_uv,
					chip->dt.s2_fifo_length);
				break;
			}
		}
	}

	rc = get_fifo_length(chip, &fifo_length, false);
	if (rc < 0) {
		pr_err("Failed to get FIFO length, rc=%d\n", rc);
		return rc;
	}

	if (chip->vbat_low && fifo_length == chip->dt.s2_vbat_low_fifo_length)
		return 0;

	if (!chip->vbat_low && fifo_length == chip->dt.s2_fifo_length)
		return 0;

	rc = qg_master_hold(chip, true);
	if (rc < 0) {
		pr_err("Failed to hold master, rc=%d\n", rc);
		goto done;
	}

	fifo_length = chip->vbat_low ? chip->dt.s2_vbat_low_fifo_length :
					chip->dt.s2_fifo_length;

	rc = qg_update_fifo_length(chip, fifo_length);
	if (rc < 0)
		goto done;

	qg_dbg(chip, QG_DEBUG_STATUS, "FIFO length updated to %d vbat_low=%d\n",
					fifo_length, chip->vbat_low);
done:
	qg_master_hold(chip, false);
	/* FIFOs restarted */
	chip->last_fifo_update_time = ktime_get();
	return rc;
}

static int qg_vbat_thresholds_config(struct qpnp_qg *chip)
{
	int rc, temp = 0, vbat_mv;
	u8 reg;

	rc = qg_get_battery_temp(chip, &temp);
	if (rc < 0) {
		pr_err("Failed to read batt_temp rc=%d\n", rc);
		return rc;
	}

	vbat_mv = (temp < chip->dt.cold_temp_threshold) ?
			chip->dt.vbatt_empty_cold_mv :
			chip->dt.vbatt_empty_mv;

	rc = qg_read(chip, chip->qg_base + QG_VBAT_EMPTY_THRESHOLD_REG,
					&reg, 1);
	if (rc < 0) {
		pr_err("Failed to read vbat-empty, rc=%d\n", rc);
		return rc;
	}

	if (vbat_mv == (reg * 50))	/* No change */
		goto config_vbat_low;

	reg = vbat_mv / 50;
	rc = qg_write(chip, chip->qg_base + QG_VBAT_EMPTY_THRESHOLD_REG,
					&reg, 1);
	if (rc < 0) {
		pr_err("Failed to write vbat-empty, rc=%d\n", rc);
		return rc;
	}

	qg_dbg(chip, QG_DEBUG_STATUS,
		"VBAT EMPTY threshold updated to %dmV temp=%d\n",
						vbat_mv, temp);

config_vbat_low:
	vbat_mv = (temp < chip->dt.cold_temp_threshold) ?
			chip->dt.vbatt_low_cold_mv :
			chip->dt.vbatt_low_mv;

	rc = qg_read(chip, chip->qg_base + QG_VBAT_LOW_THRESHOLD_REG,
					&reg, 1);
	if (rc < 0) {
		pr_err("Failed to read vbat-low, rc=%d\n", rc);
		return rc;
	}

	if (vbat_mv == (reg * 50))	/* No change */
		return 0;

	reg = vbat_mv / 50;
	rc = qg_write(chip, chip->qg_base + QG_VBAT_LOW_THRESHOLD_REG,
					&reg, 1);
	if (rc < 0) {
		pr_err("Failed to write vbat-low, rc=%d\n", rc);
		return rc;
	}

	qg_dbg(chip, QG_DEBUG_STATUS,
		"VBAT LOW threshold updated to %dmV temp=%d\n",
						vbat_mv, temp);

	return rc;
}

static void qg_retrieve_esr_params(struct qpnp_qg *chip)
{
	u32 data = 0;
	int rc;

	rc = qg_sdam_read(SDAM_ESR_CHARGE_DELTA, &data);
	if (!rc && data) {
		chip->kdata.param[QG_ESR_CHARGE_DELTA].data = data;
		chip->kdata.param[QG_ESR_CHARGE_DELTA].valid = true;
		qg_dbg(chip, QG_DEBUG_ESR,
				"ESR_CHARGE_DELTA SDAM=%d\n", data);
	} else if (rc < 0) {
		pr_err("Failed to read ESR_CHARGE_DELTA rc=%d\n", rc);
	}

	rc = qg_sdam_read(SDAM_ESR_DISCHARGE_DELTA, &data);
	if (!rc && data) {
		chip->kdata.param[QG_ESR_DISCHARGE_DELTA].data = data;
		chip->kdata.param[QG_ESR_DISCHARGE_DELTA].valid = true;
		qg_dbg(chip, QG_DEBUG_ESR,
				"ESR_DISCHARGE_DELTA SDAM=%d\n", data);
	} else if (rc < 0) {
		pr_err("Failed to read ESR_DISCHARGE_DELTA rc=%d\n", rc);
	}

	rc = qg_sdam_read(SDAM_ESR_CHARGE_SF, &data);
	if (!rc && data) {
		data = CAP(QG_ESR_SF_MIN, QG_ESR_SF_MAX, data);
		chip->kdata.param[QG_ESR_CHARGE_SF].data = data;
		chip->kdata.param[QG_ESR_CHARGE_SF].valid = true;
		qg_dbg(chip, QG_DEBUG_ESR,
				"ESR_CHARGE_SF SDAM=%d\n", data);
	} else if (rc < 0) {
		pr_err("Failed to read ESR_CHARGE_SF rc=%d\n", rc);
	}

	rc = qg_sdam_read(SDAM_ESR_DISCHARGE_SF, &data);
	if (!rc && data) {
		data = CAP(QG_ESR_SF_MIN, QG_ESR_SF_MAX, data);
		chip->kdata.param[QG_ESR_DISCHARGE_SF].data = data;
		chip->kdata.param[QG_ESR_DISCHARGE_SF].valid = true;
		qg_dbg(chip, QG_DEBUG_ESR,
				"ESR_DISCHARGE_SF SDAM=%d\n", data);
	} else if (rc < 0) {
		pr_err("Failed to read ESR_DISCHARGE_SF rc=%d\n", rc);
	}
}

static void qg_store_esr_params(struct qpnp_qg *chip)
{
	unsigned int esr;

	if (chip->udata.param[QG_ESR_CHARGE_DELTA].valid) {
		esr = chip->udata.param[QG_ESR_CHARGE_DELTA].data;
		qg_sdam_write(SDAM_ESR_CHARGE_DELTA, esr);
		qg_dbg(chip, QG_DEBUG_ESR,
			"SDAM store ESR_CHARGE_DELTA=%d\n", esr);
	}

	if (chip->udata.param[QG_ESR_DISCHARGE_DELTA].valid) {
		esr = chip->udata.param[QG_ESR_DISCHARGE_DELTA].data;
		qg_sdam_write(SDAM_ESR_DISCHARGE_DELTA, esr);
		qg_dbg(chip, QG_DEBUG_ESR,
			"SDAM store ESR_DISCHARGE_DELTA=%d\n", esr);
	}

	if (chip->udata.param[QG_ESR_CHARGE_SF].valid) {
		esr = chip->udata.param[QG_ESR_CHARGE_SF].data;
		qg_sdam_write(SDAM_ESR_CHARGE_SF, esr);
		qg_dbg(chip, QG_DEBUG_ESR,
			"SDAM store ESR_CHARGE_SF=%d\n", esr);
	}

	if (chip->udata.param[QG_ESR_DISCHARGE_SF].valid) {
		esr = chip->udata.param[QG_ESR_DISCHARGE_SF].data;
		qg_sdam_write(SDAM_ESR_DISCHARGE_SF, esr);
		qg_dbg(chip, QG_DEBUG_ESR,
			"SDAM store ESR_DISCHARGE_SF=%d\n", esr);
	}
}

#define MAX_ESR_RETRY_COUNT		10
#define ESR_SD_PERCENT			10
static int qg_process_esr_data(struct qpnp_qg *chip)
{
	int i;
	int pre_i, post_i, pre_v, post_v, first_pre_i = 0;
	int diff_v, diff_i, esr_avg = 0, count = 0;

	for (i = 0; i < qg_esr_count; i++) {
		if (!chip->esr_data[i].valid)
			continue;

		pre_i = chip->esr_data[i].pre_esr_i;
		pre_v = chip->esr_data[i].pre_esr_v;
		post_i = chip->esr_data[i].post_esr_i;
		post_v = chip->esr_data[i].post_esr_v;

		/*
		 * Check if any of the pre/post readings have changed
		 * signs by comparing it with the first valid
		 * pre_i value.
		 */
		if (!first_pre_i)
			first_pre_i = pre_i;

		if ((first_pre_i < 0 && pre_i > 0) ||
			(first_pre_i > 0 && post_i < 0) ||
			(first_pre_i < 0 && post_i > 0)) {
			qg_dbg(chip, QG_DEBUG_ESR,
				"ESR-sign mismatch %d reject all data\n", i);
			esr_avg = count = 0;
			break;
		}

		/* calculate ESR */
		diff_v = abs(post_v - pre_v);
		diff_i = abs(post_i - pre_i);

		if (!diff_v || !diff_i ||
			(diff_i < chip->dt.esr_qual_i_ua) ||
			(diff_v < chip->dt.esr_qual_v_uv)) {
			qg_dbg(chip, QG_DEBUG_ESR,
				"ESR (%d) V/I %duA %duV fails qualification\n",
				i, diff_i, diff_v);
			chip->esr_data[i].valid = false;
			continue;
		}

		chip->esr_data[i].esr =
			DIV_ROUND_CLOSEST(diff_v * 1000, diff_i);
		qg_dbg(chip, QG_DEBUG_ESR,
			"ESR qualified: i=%d pre_i=%d pre_v=%d post_i=%d post_v=%d esr_diff_v=%d esr_diff_i=%d esr=%d\n",
			i, pre_i, pre_v, post_i, post_v,
			diff_v, diff_i, chip->esr_data[i].esr);

		esr_avg += chip->esr_data[i].esr;
		count++;
	}

	if (!count) {
		qg_dbg(chip, QG_DEBUG_ESR,
			"No ESR samples qualified, ESR not found\n");
		chip->esr_avg = 0;
		return 0;
	}

	esr_avg /= count;
	qg_dbg(chip, QG_DEBUG_ESR,
		"ESR all sample average=%d count=%d apply_SD=%d\n",
		esr_avg, count, (esr_avg * ESR_SD_PERCENT) / 100);

	/*
	 * Reject ESR samples which do not fall in
	 * 10% the standard-deviation
	 */
	count = 0;
	for (i = 0; i < qg_esr_count; i++) {
		if (!chip->esr_data[i].valid)
			continue;

		if ((abs(chip->esr_data[i].esr - esr_avg) <=
			(esr_avg * ESR_SD_PERCENT) / 100)) {
			/* valid ESR */
			chip->esr_avg += chip->esr_data[i].esr;
			count++;
			qg_dbg(chip, QG_DEBUG_ESR,
				"Valid ESR after SD (%d) %d mOhm\n",
				i, chip->esr_data[i].esr);
		} else {
			qg_dbg(chip, QG_DEBUG_ESR,
				"ESR (%d) %d falls-out of SD(%d)\n",
				i, chip->esr_data[i].esr, ESR_SD_PERCENT);
		}
	}

	if (count >= QG_MIN_ESR_COUNT) {
		chip->esr_avg /= count;
		qg_dbg(chip, QG_DEBUG_ESR, "Average estimated ESR %d mOhm\n",
					chip->esr_avg);
	} else {
		qg_dbg(chip, QG_DEBUG_ESR,
			"Not enough ESR samples, ESR not found\n");
		chip->esr_avg = 0;
	}

	return 0;
}

static int qg_esr_estimate(struct qpnp_qg *chip)
{
	int rc, i, ibat = 0;
	u8 esr_done_count, reg0 = 0, reg1 = 0;
	bool is_charging = false;

	if (chip->dt.esr_disable)
		return 0;

	/*
	 * Charge - enable ESR estimation if IBAT > MIN_IBAT.
	 * Discharge - enable ESR estimation only if enabled via DT.
	 */
	rc = qg_get_battery_current(chip, &ibat);
	if (rc < 0)
		return rc;
	if (chip->charge_status == POWER_SUPPLY_STATUS_CHARGING &&
				ibat > chip->dt.esr_min_ibat_ua) {
		qg_dbg(chip, QG_DEBUG_ESR,
			"Skip CHG ESR, Fails IBAT ibat(%d) min_ibat(%d)\n",
				ibat, chip->dt.esr_min_ibat_ua);
		return 0;
	}

	if (chip->charge_status != POWER_SUPPLY_STATUS_CHARGING &&
			!chip->dt.esr_discharge_enable)
		return 0;

	if (chip->batt_soc != INT_MIN && (chip->batt_soc <
					chip->dt.esr_disable_soc)) {
		qg_dbg(chip, QG_DEBUG_ESR,
			"Skip ESR, batt-soc below %d\n",
				chip->dt.esr_disable_soc);
		return 0;
	}

	qg_dbg(chip, QG_DEBUG_ESR, "FIFO done count=%d ESR mod count=%d\n",
			chip->fifo_done_count, qg_esr_mod_count);

	if ((chip->fifo_done_count % qg_esr_mod_count) != 0)
		return 0;

	if (qg_esr_count > QG_MAX_ESR_COUNT)
		qg_esr_count = QG_MAX_ESR_COUNT;

	if (qg_esr_count < QG_MIN_ESR_COUNT)
		qg_esr_count = QG_MIN_ESR_COUNT;

	/* clear all data */
	chip->esr_avg = 0;
	memset(&chip->esr_data, 0, sizeof(chip->esr_data));

	rc = qg_master_hold(chip, true);
	if (rc < 0) {
		pr_err("Failed to hold master, rc=%d\n", rc);
		goto done;
	}

	for (i = 0; i < qg_esr_count; i++) {
		/* Fire ESR measurement */
		rc = qg_masked_write(chip,
			chip->qg_base + QG_ESR_MEAS_TRIG_REG,
			HW_ESR_MEAS_START_BIT, HW_ESR_MEAS_START_BIT);
		if (rc < 0) {
			pr_err("Failed to start ESR rc=%d\n", rc);
			continue;
		}

		esr_done_count = reg0 = reg1 = 0;
		do {
			/* delay for ESR processing to complete */
			msleep(50);

			esr_done_count++;

			rc = qg_read(chip,
				chip->qg_base + QG_STATUS1_REG, &reg0, 1);
			if (rc < 0)
				continue;

			rc = qg_read(chip,
				chip->qg_base + QG_STATUS4_REG, &reg1, 1);
			if (rc < 0)
				continue;

			/* check ESR-done status */
			if (!(reg1 & ESR_MEAS_IN_PROGRESS_BIT) &&
					(reg0 & ESR_MEAS_DONE_BIT)) {
				qg_dbg(chip, QG_DEBUG_ESR,
					"ESR measurement done %d count %d\n",
						i, esr_done_count);
				break;
			}
		} while (esr_done_count < MAX_ESR_RETRY_COUNT);

		if (esr_done_count == MAX_ESR_RETRY_COUNT) {
			pr_err("Failed to get ESR done for %d iteration\n", i);
			continue;
		} else {
			/* found a valid ESR, read pre-post data */
			rc = qg_read_raw_data(chip, QG_PRE_ESR_V_DATA0_REG,
					&chip->esr_data[i].pre_esr_v);
			if (rc < 0)
				goto done;

			rc = qg_read_raw_data(chip, QG_PRE_ESR_I_DATA0_REG,
					&chip->esr_data[i].pre_esr_i);
			if (rc < 0)
				goto done;

			rc = qg_read_raw_data(chip, QG_POST_ESR_V_DATA0_REG,
					&chip->esr_data[i].post_esr_v);
			if (rc < 0)
				goto done;

			rc = qg_read_raw_data(chip, QG_POST_ESR_I_DATA0_REG,
					&chip->esr_data[i].post_esr_i);
			if (rc < 0)
				goto done;

			chip->esr_data[i].pre_esr_v =
				V_RAW_TO_UV(chip->esr_data[i].pre_esr_v);
			ibat = sign_extend32(chip->esr_data[i].pre_esr_i, 15);
			chip->esr_data[i].pre_esr_i = I_RAW_TO_UA(ibat);
			chip->esr_data[i].post_esr_v =
				V_RAW_TO_UV(chip->esr_data[i].post_esr_v);
			ibat = sign_extend32(chip->esr_data[i].post_esr_i, 15);
			chip->esr_data[i].post_esr_i = I_RAW_TO_UA(ibat);

			chip->esr_data[i].valid = true;

			if ((int)chip->esr_data[i].pre_esr_i < 0)
				is_charging = true;

			qg_dbg(chip, QG_DEBUG_ESR,
				"ESR values for %d iteration pre_v=%d pre_i=%d post_v=%d post_i=%d\n",
				i, chip->esr_data[i].pre_esr_v,
				(int)chip->esr_data[i].pre_esr_i,
				chip->esr_data[i].post_esr_v,
				(int)chip->esr_data[i].post_esr_i);
		}
		/* delay before the next ESR measurement */
		msleep(200);
	}

	rc = qg_process_esr_data(chip);
	if (rc < 0)
		pr_err("Failed to process ESR data rc=%d\n", rc);

	rc = qg_master_hold(chip, false);
	if (rc < 0) {
		pr_err("Failed to release master, rc=%d\n", rc);
		goto done;
	}
	/* FIFOs restarted */
	chip->last_fifo_update_time = ktime_get();

	if (chip->esr_avg) {
		chip->kdata.param[QG_ESR].data = chip->esr_avg;
		chip->kdata.param[QG_ESR].valid = true;
		qg_dbg(chip, QG_DEBUG_ESR, "ESR_SW=%d during %s\n",
			chip->esr_avg, is_charging ? "CHARGE" : "DISCHARGE");
		qg_retrieve_esr_params(chip);
		chip->esr_actual = chip->esr_avg;
	}

	return 0;
done:
	qg_master_hold(chip, false);
	return rc;
}

static void process_udata_work(struct work_struct *work)
{
	struct qpnp_qg *chip = container_of(work,
			struct qpnp_qg, udata_work);
	int rc;

	if (chip->udata.param[QG_CC_SOC].valid)
		chip->cc_soc = chip->udata.param[QG_CC_SOC].data;

	if (chip->udata.param[QG_BATT_SOC].valid)
		chip->batt_soc = chip->udata.param[QG_BATT_SOC].data;

	if (chip->udata.param[QG_FULL_SOC].valid)
		chip->full_soc = chip->udata.param[QG_FULL_SOC].data;

	if (chip->udata.param[QG_SOC].valid) {
		qg_dbg(chip, QG_DEBUG_SOC, "udata SOC=%d last SOC=%d\n",
			chip->udata.param[QG_SOC].data, chip->catch_up_soc);

		chip->catch_up_soc = chip->udata.param[QG_SOC].data;
		qg_scale_soc(chip, false);

		/* update parameters to SDAM */
		chip->sdam_data[SDAM_SOC] = chip->msoc;
		chip->sdam_data[SDAM_OCV_UV] =
				chip->udata.param[QG_OCV_UV].data;
		chip->sdam_data[SDAM_RBAT_MOHM] =
				chip->udata.param[QG_RBAT_MOHM].data;
		chip->sdam_data[SDAM_VALID] = 1;

		rc = qg_store_soc_params(chip);
		if (rc < 0)
			pr_err("Failed to update SDAM params, rc=%d\n", rc);
	}

	if (chip->udata.param[QG_CHARGE_COUNTER].valid)
		chip->charge_counter_uah =
			chip->udata.param[QG_CHARGE_COUNTER].data;

	if (chip->udata.param[QG_ESR].valid)
		chip->esr_last = chip->udata.param[QG_ESR].data;

	if (chip->esr_actual != -EINVAL && chip->udata.param[QG_ESR].valid) {
		chip->esr_nominal = chip->udata.param[QG_ESR].data;
		if (chip->qg_psy)
			power_supply_changed(chip->qg_psy);
	}

	if (!chip->dt.esr_disable)
		qg_store_esr_params(chip);

	qg_dbg(chip, QG_DEBUG_STATUS, "udata update: batt_soc=%d cc_soc=%d full_soc=%d qg_esr=%d\n",
		(chip->batt_soc != INT_MIN) ? chip->batt_soc : -EINVAL,
		(chip->cc_soc != INT_MIN) ? chip->cc_soc : -EINVAL,
		chip->full_soc, chip->esr_last);
	vote(chip->awake_votable, UDATA_READY_VOTER, false, 0);
}

#define MAX_FIFO_DELTA_PERCENT		10
static irqreturn_t qg_fifo_update_done_handler(int irq, void *data)
{
	ktime_t now = ktime_get();
	int rc, hw_delta_ms = 0, margin_ms = 0;
	u32 fifo_length = 0;
	s64 time_delta_ms = 0;
	struct qpnp_qg *chip = data;

	time_delta_ms = ktime_ms_delta(now, chip->last_fifo_update_time);
	chip->last_fifo_update_time = now;

	qg_dbg(chip, QG_DEBUG_IRQ, "IRQ triggered\n");
	mutex_lock(&chip->data_lock);

	rc = get_fifo_length(chip, &fifo_length, false);
	if (rc < 0) {
		pr_err("Failed to get FIFO length, rc=%d\n", rc);
		goto done;
	}

	rc = qg_process_fifo(chip, fifo_length);
	if (rc < 0) {
		pr_err("Failed to process QG FIFO, rc=%d\n", rc);
		goto done;
	}

	if (++chip->fifo_done_count == U32_MAX)
		chip->fifo_done_count = 0;

	rc = qg_vbat_thresholds_config(chip);
	if (rc < 0)
		pr_err("Failed to apply VBAT EMPTY config rc=%d\n", rc);

	rc = qg_vbat_low_wa(chip);
	if (rc < 0) {
		pr_err("Failed to apply VBAT LOW WA, rc=%d\n", rc);
		goto done;
	}

	rc = qg_esr_estimate(chip);
	if (rc < 0) {
		pr_err("Failed to estimate ESR, rc=%d\n", rc);
		goto done;
	}

	rc = get_fifo_done_time(chip, false, &hw_delta_ms);
	if (rc < 0)
		hw_delta_ms = 0;
	else
		margin_ms = (hw_delta_ms * MAX_FIFO_DELTA_PERCENT) / 100;

	if (abs(hw_delta_ms - time_delta_ms) < margin_ms) {
		chip->kdata.param[QG_FIFO_TIME_DELTA].data = time_delta_ms;
		chip->kdata.param[QG_FIFO_TIME_DELTA].valid = true;
		qg_dbg(chip, QG_DEBUG_FIFO, "FIFO_done time_delta_ms=%lld\n",
							time_delta_ms);
	}

	/* signal the read thread */
	chip->data_ready = true;
	wake_up_interruptible(&chip->qg_wait_q);

	/* vote to stay awake until userspace reads data */
	vote(chip->awake_votable, FIFO_DONE_VOTER, true, 0);

done:
	mutex_unlock(&chip->data_lock);
	return IRQ_HANDLED;
}

static irqreturn_t qg_vbat_low_handler(int irq, void *data)
{
	int rc;
	struct qpnp_qg *chip = data;
	u8 status = 0;

	qg_dbg(chip, QG_DEBUG_IRQ, "IRQ triggered\n");
	mutex_lock(&chip->data_lock);

	rc = qg_read(chip, chip->qg_base + QG_INT_RT_STS_REG, &status, 1);
	if (rc < 0) {
		pr_err("Failed to read RT status, rc=%d\n", rc);
		goto done;
	}
	/* ignore VBAT low if battery is missing */
	if ((status & BATTERY_MISSING_INT_RT_STS_BIT) ||
			chip->battery_missing)
		goto done;

	chip->vbat_low = !!(status & VBAT_LOW_INT_RT_STS_BIT);

	qg_dbg(chip, QG_DEBUG_IRQ, "VBAT_LOW = %d\n", chip->vbat_low);
done:
	mutex_unlock(&chip->data_lock);
	return IRQ_HANDLED;
}

static irqreturn_t qg_vbat_empty_handler(int irq, void *data)
{
	struct qpnp_qg *chip = data;
	u32 ocv_uv = 0;
	int rc;
	u8 status = 0;

	qg_dbg(chip, QG_DEBUG_IRQ, "IRQ triggered\n");

	rc = qg_read(chip, chip->qg_base + QG_INT_RT_STS_REG, &status, 1);
	if (rc < 0)
		pr_err("Failed to read RT status rc=%d\n", rc);

	/* ignore VBAT empty if battery is missing */
	if ((status & BATTERY_MISSING_INT_RT_STS_BIT) ||
			chip->battery_missing)
		return IRQ_HANDLED;

	pr_warn("VBATT EMPTY SOC = 0\n");

	chip->catch_up_soc = 0;
	qg_scale_soc(chip, true);

	qg_sdam_read(SDAM_OCV_UV, &ocv_uv);
	chip->sdam_data[SDAM_SOC] = 0;
	chip->sdam_data[SDAM_OCV_UV] = ocv_uv;
	chip->sdam_data[SDAM_VALID] = 1;

	qg_store_soc_params(chip);

	if (chip->qg_psy)
		power_supply_changed(chip->qg_psy);

	return IRQ_HANDLED;
}

static irqreturn_t qg_good_ocv_handler(int irq, void *data)
{
	int rc;
	u8 status = 0;
	u32 ocv_uv = 0, ocv_raw = 0;
	struct qpnp_qg *chip = data;

	qg_dbg(chip, QG_DEBUG_IRQ, "IRQ triggered\n");

	mutex_lock(&chip->data_lock);

	rc = qg_read(chip, chip->qg_base + QG_STATUS2_REG, &status, 1);
	if (rc < 0) {
		pr_err("Failed to read status2 register rc=%d\n", rc);
		goto done;
	}

	if (!(status & GOOD_OCV_BIT))
		goto done;

	rc = qg_read_ocv(chip, &ocv_uv, &ocv_raw, S3_GOOD_OCV);
	if (rc < 0) {
		pr_err("Failed to read good_ocv, rc=%d\n", rc);
		goto done;
	}

	chip->kdata.param[QG_GOOD_OCV_UV].data = ocv_uv;
	chip->kdata.param[QG_GOOD_OCV_UV].valid = true;

	vote(chip->awake_votable, GOOD_OCV_VOTER, true, 0);

	/* signal the readd thread */
	chip->data_ready = true;
	wake_up_interruptible(&chip->qg_wait_q);
done:
	mutex_unlock(&chip->data_lock);
	return IRQ_HANDLED;
}

static struct qg_irq_info qg_irqs[] = {
	[QG_BATT_MISSING_IRQ] = {
		.name		= "qg-batt-missing",
	},
	[QG_VBATT_LOW_IRQ] = {
		.name		= "qg-vbat-low",
		.handler	= qg_vbat_low_handler,
		.wake		= true,
	},
	[QG_VBATT_EMPTY_IRQ] = {
		.name		= "qg-vbat-empty",
		.handler	= qg_vbat_empty_handler,
		.wake		= true,
	},
	[QG_FIFO_UPDATE_DONE_IRQ] = {
		.name		= "qg-fifo-done",
		.handler	= qg_fifo_update_done_handler,
		.wake		= true,
	},
	[QG_GOOD_OCV_IRQ] = {
		.name		= "qg-good-ocv",
		.handler	= qg_good_ocv_handler,
		.wake		= true,
	},
	[QG_FSM_STAT_CHG_IRQ] = {
		.name		= "qg-fsm-state-chg",
	},
	[QG_EVENT_IRQ] = {
		.name		= "qg-event",
	},
};

static int qg_awake_cb(struct votable *votable, void *data, int awake,
			const char *client)
{
	struct qpnp_qg *chip = data;

	/* ignore if the QG device is not open */
	if (!chip->qg_device_open)
		return 0;

	if (awake)
		pm_stay_awake(chip->dev);
	else
		pm_relax(chip->dev);

	pr_debug("client: %s awake: %d\n", client, awake);
	return 0;
}

static int qg_fifo_irq_disable_cb(struct votable *votable, void *data,
				int disable, const char *client)
{
	if (disable) {
		if (qg_irqs[QG_FIFO_UPDATE_DONE_IRQ].wake)
			disable_irq_wake(
				qg_irqs[QG_FIFO_UPDATE_DONE_IRQ].irq);
		if (qg_irqs[QG_FIFO_UPDATE_DONE_IRQ].irq)
			disable_irq_nosync(
				qg_irqs[QG_FIFO_UPDATE_DONE_IRQ].irq);
	} else {
		if (qg_irqs[QG_FIFO_UPDATE_DONE_IRQ].irq)
			enable_irq(qg_irqs[QG_FIFO_UPDATE_DONE_IRQ].irq);
		if (qg_irqs[QG_FIFO_UPDATE_DONE_IRQ].wake)
			enable_irq_wake(
				qg_irqs[QG_FIFO_UPDATE_DONE_IRQ].irq);
	}

	return 0;
}

static int qg_vbatt_irq_disable_cb(struct votable *votable, void *data,
				int disable, const char *client)
{
	if (disable) {
		if (qg_irqs[QG_VBATT_LOW_IRQ].wake)
			disable_irq_wake(qg_irqs[QG_VBATT_LOW_IRQ].irq);
		if (qg_irqs[QG_VBATT_EMPTY_IRQ].wake)
			disable_irq_wake(qg_irqs[QG_VBATT_EMPTY_IRQ].irq);
		if (qg_irqs[QG_VBATT_LOW_IRQ].irq)
			disable_irq_nosync(qg_irqs[QG_VBATT_LOW_IRQ].irq);
		if (qg_irqs[QG_VBATT_EMPTY_IRQ].irq)
			disable_irq_nosync(qg_irqs[QG_VBATT_EMPTY_IRQ].irq);
	} else {
		if (qg_irqs[QG_VBATT_LOW_IRQ].irq)
			enable_irq(qg_irqs[QG_VBATT_LOW_IRQ].irq);
		if (qg_irqs[QG_VBATT_EMPTY_IRQ].irq)
			enable_irq(qg_irqs[QG_VBATT_EMPTY_IRQ].irq);
		if (qg_irqs[QG_VBATT_LOW_IRQ].wake)
			enable_irq_wake(qg_irqs[QG_VBATT_LOW_IRQ].irq);
		if (qg_irqs[QG_VBATT_EMPTY_IRQ].wake)
			enable_irq_wake(qg_irqs[QG_VBATT_EMPTY_IRQ].irq);
	}

	return 0;
}

static int qg_good_ocv_irq_disable_cb(struct votable *votable, void *data,
				int disable, const char *client)
{
	if (disable) {
		if (qg_irqs[QG_GOOD_OCV_IRQ].wake)
			disable_irq_wake(qg_irqs[QG_GOOD_OCV_IRQ].irq);
		if (qg_irqs[QG_GOOD_OCV_IRQ].irq)
			disable_irq_nosync(qg_irqs[QG_GOOD_OCV_IRQ].irq);
	} else {
		if (qg_irqs[QG_GOOD_OCV_IRQ].irq)
			enable_irq(qg_irqs[QG_GOOD_OCV_IRQ].irq);
		if (qg_irqs[QG_GOOD_OCV_IRQ].wake)
			enable_irq_wake(qg_irqs[QG_GOOD_OCV_IRQ].irq);
	}

	return 0;
}

/* ALG callback functions below */

static int qg_get_learned_capacity(void *data, int64_t *learned_cap_uah)
{
	struct qpnp_qg *chip = data;
	int16_t cc_mah;
	int rc;

	if (!chip)
		return -ENODEV;

	if (chip->battery_missing || !chip->profile_loaded)
		return -ENODEV;

	rc = qg_sdam_multibyte_read(QG_SDAM_LEARNED_CAPACITY_OFFSET,
					(u8 *)&cc_mah, 2);
	if (rc < 0) {
		pr_err("Error in reading learned_capacity, rc=%d\n", rc);
		return rc;
	}
	*learned_cap_uah = cc_mah * 1000;

	qg_dbg(chip, QG_DEBUG_ALG_CL, "Retrieved learned capacity %llduah\n",
					*learned_cap_uah);
	return 0;
}

static int qg_store_learned_capacity(void *data, int64_t learned_cap_uah)
{
	struct qpnp_qg *chip = data;
	int16_t cc_mah;
	int rc;

	if (!chip)
		return -ENODEV;

	if (chip->battery_missing || !learned_cap_uah)
		return -ENODEV;

	cc_mah = div64_s64(learned_cap_uah, 1000);
	rc = qg_sdam_multibyte_write(QG_SDAM_LEARNED_CAPACITY_OFFSET,
					 (u8 *)&cc_mah, 2);
	if (rc < 0) {
		pr_err("Error in writing learned_capacity, rc=%d\n", rc);
		return rc;
	}

	qg_dbg(chip, QG_DEBUG_ALG_CL, "Stored learned capacity %llduah\n",
					learned_cap_uah);
	return 0;
}

static int qg_get_cc_soc(void *data, int *cc_soc)
{
	struct qpnp_qg *chip = data;

	if (!chip)
		return -ENODEV;

	if (chip->cc_soc == INT_MIN)
		return -EINVAL;

	*cc_soc = chip->cc_soc;

	return 0;
}

static int qg_restore_cycle_count(void *data, u16 *buf, int length)
{
	struct qpnp_qg *chip = data;
	int id, rc = 0;
	u8 tmp[2];

	if (!chip)
		return -ENODEV;

	if (chip->battery_missing || !chip->profile_loaded)
		return -ENODEV;

	if (!buf || length > BUCKET_COUNT)
		return -EINVAL;

	for (id = 0; id < length; id++) {
		rc = qg_sdam_multibyte_read(
				QG_SDAM_CYCLE_COUNT_OFFSET + (id * 2),
				(u8 *)tmp, 2);
		if (rc < 0) {
			pr_err("failed to read bucket %d rc=%d\n", id, rc);
			return rc;
		}
		*buf++ = tmp[0] | tmp[1] << 8;
	}

	return rc;
}

static int qg_store_cycle_count(void *data, u16 *buf, int id, int length)
{
	struct qpnp_qg *chip = data;
	int rc = 0;

	if (!chip)
		return -ENODEV;

	if (chip->battery_missing || !chip->profile_loaded)
		return -ENODEV;

	if (!buf || length > BUCKET_COUNT * 2 || id < 0 ||
		id > BUCKET_COUNT - 1 ||
		(((id * 2) + length) > BUCKET_COUNT * 2))
		return -EINVAL;

	rc = qg_sdam_multibyte_write(
			QG_SDAM_CYCLE_COUNT_OFFSET + (id * 2),
			(u8 *)buf, length);
	if (rc < 0)
		pr_err("failed to write bucket %d rc=%d\n", id, rc);

	return rc;
}

#define DEFAULT_BATT_TYPE	"Unknown Battery"
#define MISSING_BATT_TYPE	"Missing Battery"
#define DEBUG_BATT_TYPE		"Debug Board"
static const char *qg_get_battery_type(struct qpnp_qg *chip)
{
	if (chip->battery_missing)
		return MISSING_BATT_TYPE;

	if (is_debug_batt_id(chip))
		return DEBUG_BATT_TYPE;

	if (chip->bp.batt_type_str) {
		if (chip->profile_loaded)
			return chip->bp.batt_type_str;
	}

	return DEFAULT_BATT_TYPE;
}

static int qg_get_battery_voltage(struct qpnp_qg *chip, int *vbat_uv)
{
	int rc = 0;
	u64 last_vbat = 0;

	if (chip->battery_missing) {
		*vbat_uv = 3700000;
		return 0;
	}

	rc = qg_read(chip, chip->qg_base + QG_LAST_ADC_V_DATA0_REG,
				(u8 *)&last_vbat, 2);
	if (rc < 0) {
		pr_err("Failed to read LAST_ADV_V reg, rc=%d\n", rc);
		return rc;
	}

	*vbat_uv = V_RAW_TO_UV(last_vbat);

	return rc;
}

#define DEBUG_BATT_SOC		67
#define BATT_MISSING_SOC	50
#define EMPTY_SOC		0
#define FULL_SOC		100
static int qg_get_battery_capacity(struct qpnp_qg *chip, int *soc)
{
	if (is_debug_batt_id(chip)) {
		*soc = DEBUG_BATT_SOC;
		return 0;
	}

	if (chip->battery_missing || !chip->profile_loaded) {
		*soc = BATT_MISSING_SOC;
		return 0;
	}

	if (chip->charge_full) {
		*soc = FULL_SOC;
		return 0;
	}

	mutex_lock(&chip->soc_lock);

	if (chip->dt.linearize_soc && chip->maint_soc > 0)
		*soc = chip->maint_soc;
	else
		*soc = chip->msoc;

	mutex_unlock(&chip->soc_lock);

	return 0;
}

static int qg_get_ttf_param(void *data, enum ttf_param param, int *val)
{
	union power_supply_propval prop = {0, };
	struct qpnp_qg *chip = data;
	int rc = 0;
	int64_t temp = 0;

	if (!chip)
		return -ENODEV;

	if (chip->battery_missing || !chip->profile_loaded)
		return -ENODEV;

	switch (param) {
	case TTF_MSOC:
		rc = qg_get_battery_capacity(chip, val);
		break;
	case TTF_VBAT:
		rc = qg_get_battery_voltage(chip, val);
		break;
	case TTF_IBAT:
		rc = qg_get_battery_current(chip, val);
		break;
	case TTF_FCC:
		if (chip->qg_psy) {
			rc = power_supply_get_property(chip->qg_psy,
				POWER_SUPPLY_PROP_CHARGE_FULL, &prop);
			if (rc >= 0) {
				temp = div64_u64(prop.intval, 1000);
				*val  = div64_u64(chip->full_soc * temp,
						QG_SOC_FULL);
			}
		}
		break;
	case TTF_MODE:
		*val = TTF_MODE_NORMAL;
		break;
	case TTF_ITERM:
		if (chip->chg_iterm_ma == INT_MIN)
			*val = 0;
		else
			*val = chip->chg_iterm_ma;
		break;
	case TTF_RBATT:
		rc = qg_sdam_read(SDAM_RBAT_MOHM, val);
		if (!rc)
			*val *= 1000;
		break;
	case TTF_VFLOAT:
		*val = chip->bp.float_volt_uv;
		break;
	case TTF_CHG_TYPE:
		*val = chip->charge_type;
		break;
	case TTF_CHG_STATUS:
		*val = chip->charge_status;
		break;
	default:
		pr_err("Unsupported property %d\n", param);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int qg_ttf_awake_voter(void *data, bool val)
{
	struct qpnp_qg *chip = data;

	if (!chip)
		return -ENODEV;

	if (chip->battery_missing || !chip->profile_loaded)
		return -ENODEV;

	vote(chip->awake_votable, TTF_AWAKE_VOTER, val, 0);

	return 0;
}

static int qg_psy_set_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       const union power_supply_propval *pval)
{
	struct qpnp_qg *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (chip->dt.cl_disable) {
			pr_warn("Capacity learning disabled!\n");
			return 0;
		}
		if (chip->cl->active) {
			pr_warn("Capacity learning active!\n");
			return 0;
		}
		if (pval->intval <= 0 || pval->intval > chip->cl->nom_cap_uah) {
			pr_err("charge_full is out of bounds\n");
			return -EINVAL;
		}
		mutex_lock(&chip->cl->lock);
		rc = qg_store_learned_capacity(chip, pval->intval);
		if (!rc)
			chip->cl->learned_cap_uah = pval->intval;
		mutex_unlock(&chip->cl->lock);
		break;
	case POWER_SUPPLY_PROP_SOH:
		chip->soh = pval->intval;
		qg_dbg(chip, QG_DEBUG_STATUS, "SOH update: SOH=%d esr_actual=%d esr_nominal=%d\n",
				chip->soh, chip->esr_actual, chip->esr_nominal);
		break;
	case POWER_SUPPLY_PROP_ESR_ACTUAL:
		chip->esr_actual = pval->intval;
		break;
	case POWER_SUPPLY_PROP_ESR_NOMINAL:
		chip->esr_nominal = pval->intval;
		break;
	default:
		break;
	}
	return 0;
}

static int qg_psy_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *pval)
{
	struct qpnp_qg *chip = power_supply_get_drvdata(psy);
	int rc = 0;
	int64_t temp = 0;

	pval->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = qg_get_battery_capacity(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = qg_get_battery_voltage(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = qg_get_battery_current(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		rc = qg_sdam_read(SDAM_OCV_UV, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = qg_get_battery_temp(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_RESISTANCE_ID:
		pval->intval = chip->batt_id_ohm;
		break;
	case POWER_SUPPLY_PROP_DEBUG_BATTERY:
		pval->intval = is_debug_batt_id(chip);
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		rc = qg_sdam_read(SDAM_RBAT_MOHM, &pval->intval);
		if (!rc)
			pval->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE_NOW:
		pval->intval = chip->esr_last;
		break;
	case POWER_SUPPLY_PROP_SOC_REPORTING_READY:
		pval->intval = chip->soc_reporting_ready;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE_CAPACITIVE:
		pval->intval = chip->dt.rbat_conn_mohm;
		break;
	case POWER_SUPPLY_PROP_BATTERY_TYPE:
		pval->strval = qg_get_battery_type(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		pval->intval = chip->dt.vbatt_cutoff_mv * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pval->intval = chip->bp.float_volt_uv;
		break;
	case POWER_SUPPLY_PROP_BATT_FULL_CURRENT:
		pval->intval = chip->dt.iterm_ma * 1000;
		break;
	case POWER_SUPPLY_PROP_BATT_PROFILE_VERSION:
		pval->intval = chip->bp.qg_profile_version;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		pval->intval = chip->charge_counter_uah;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (!chip->dt.cl_disable && chip->dt.cl_feedback_on)
			rc = qg_get_learned_capacity(chip, &temp);
		else
			rc = qg_get_nominal_capacity((int *)&temp, 250, true);
		if (!rc)
			pval->intval = (int)temp;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		rc = qg_get_nominal_capacity((int *)&temp, 250, true);
		if (!rc)
			pval->intval = (int)temp;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNTS:
		rc = get_cycle_counts(chip->counter, &pval->strval);
		if (rc < 0)
			pval->strval = NULL;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		rc = get_cycle_count(chip->counter, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
		rc = ttf_get_time_to_full(chip->ttf, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		rc = ttf_get_time_to_empty(chip->ttf, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_ESR_ACTUAL:
		pval->intval = (chip->esr_actual == -EINVAL) ?  -EINVAL :
					(chip->esr_actual * 1000);
		break;
	case POWER_SUPPLY_PROP_ESR_NOMINAL:
		pval->intval = (chip->esr_nominal == -EINVAL) ?  -EINVAL :
					(chip->esr_nominal * 1000);
		break;
	case POWER_SUPPLY_PROP_SOH:
		pval->intval = chip->soh;
		break;
	default:
		pr_debug("Unsupported property %d\n", psp);
		break;
	}

	return rc;
}

static int qg_property_is_writeable(struct power_supply *psy,
				enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_ESR_ACTUAL:
	case POWER_SUPPLY_PROP_ESR_NOMINAL:
	case POWER_SUPPLY_PROP_SOH:
		return 1;
	default:
		break;
	}
	return 0;
}

static enum power_supply_property qg_psy_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_RESISTANCE_NOW,
	POWER_SUPPLY_PROP_SOC_REPORTING_READY,
	POWER_SUPPLY_PROP_RESISTANCE_CAPACITIVE,
	POWER_SUPPLY_PROP_DEBUG_BATTERY,
	POWER_SUPPLY_PROP_BATTERY_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_BATT_FULL_CURRENT,
	POWER_SUPPLY_PROP_BATT_PROFILE_VERSION,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CYCLE_COUNTS,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_ESR_ACTUAL,
	POWER_SUPPLY_PROP_ESR_NOMINAL,
	POWER_SUPPLY_PROP_SOH,
};

static const struct power_supply_desc qg_psy_desc = {
	.name = "bms",
	.type = POWER_SUPPLY_TYPE_BMS,
	.properties = qg_psy_props,
	.num_properties = ARRAY_SIZE(qg_psy_props),
	.get_property = qg_psy_get_property,
	.set_property = qg_psy_set_property,
	.property_is_writeable = qg_property_is_writeable,
};

#define DEFAULT_RECHARGE_SOC 95
static int qg_charge_full_update(struct qpnp_qg *chip)
{
	union power_supply_propval prop = {0, };
	int rc, recharge_soc, health;

	if (!chip->dt.hold_soc_while_full)
		goto out;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_HEALTH, &prop);
	if (rc < 0) {
		pr_err("Failed to get battery health, rc=%d\n", rc);
		goto out;
	}
	health = prop.intval;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_RECHARGE_SOC, &prop);
	if (rc < 0 || prop.intval < 0) {
		pr_debug("Failed to get recharge-soc\n");
		recharge_soc = DEFAULT_RECHARGE_SOC;
	}
	recharge_soc = prop.intval;

	qg_dbg(chip, QG_DEBUG_STATUS, "msoc=%d health=%d charge_full=%d\n",
				chip->msoc, health, chip->charge_full);
	if (chip->charge_done && !chip->charge_full) {
		if (chip->msoc >= 99 && health == POWER_SUPPLY_HEALTH_GOOD) {
			chip->charge_full = true;
			qg_dbg(chip, QG_DEBUG_STATUS, "Setting charge_full (0->1) @ msoc=%d\n",
					chip->msoc);
		} else if (health != POWER_SUPPLY_HEALTH_GOOD) {
			/* terminated in JEITA */
			qg_dbg(chip, QG_DEBUG_STATUS, "Terminated charging @ msoc=%d\n",
					chip->msoc);
		}
	} else if ((!chip->charge_done || chip->msoc < recharge_soc)
				&& chip->charge_full) {

		if (chip->wa_flags & QG_RECHARGE_SOC_WA) {
			/* Force recharge */
			prop.intval = 0;
			rc = power_supply_set_property(chip->batt_psy,
				POWER_SUPPLY_PROP_RECHARGE_SOC, &prop);
			if (rc < 0)
				pr_err("Failed to force recharge rc=%d\n", rc);
			else
				qg_dbg(chip, QG_DEBUG_STATUS,
					"Forced recharge\n");
		}

		/*
		 * If recharge or discharge has started and
		 * if linearize soc dtsi property defined
		 * scale msoc from 100% for better UX.
		 */
		if (chip->dt.linearize_soc && chip->msoc < 99) {
			chip->maint_soc = FULL_SOC;
			qg_scale_soc(chip, false);
		}

		qg_dbg(chip, QG_DEBUG_STATUS, "msoc=%d recharge_soc=%d charge_full (1->0)\n",
					chip->msoc, recharge_soc);
		chip->charge_full = false;
	}
out:
	return 0;
}

static int qg_parallel_status_update(struct qpnp_qg *chip)
{
	int rc;
	bool parallel_enabled = is_parallel_enabled(chip);
	bool update_smb = false;

	if (parallel_enabled == chip->parallel_enabled)
		return 0;

	chip->parallel_enabled = parallel_enabled;
	qg_dbg(chip, QG_DEBUG_STATUS,
		"Parallel status changed Enabled=%d\n", parallel_enabled);

	mutex_lock(&chip->data_lock);

	/*
	 * Parallel charger uses the same external sense, hence do not
	 * enable SMB sensing if PMI632 is configured for external sense.
	 */
	if (!chip->dt.qg_ext_sense)
		update_smb = true;

	rc = process_rt_fifo_data(chip, update_smb);
	if (rc < 0)
		pr_err("Failed to process RT FIFO data, rc=%d\n", rc);

	mutex_unlock(&chip->data_lock);

	return 0;
}

static int qg_usb_status_update(struct qpnp_qg *chip)
{
	bool usb_present = is_usb_present(chip);

	if (chip->usb_present != usb_present) {
		qg_dbg(chip, QG_DEBUG_STATUS,
			"USB status changed Present=%d\n",
							usb_present);
		qg_scale_soc(chip, false);
	}

	chip->usb_present = usb_present;

	return 0;
}

static int qg_handle_battery_removal(struct qpnp_qg *chip)
{
	int rc, length = QG_SDAM_MAX_OFFSET - QG_SDAM_VALID_OFFSET;
	u8 *data;

	/* clear SDAM */
	data = kcalloc(length, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	rc = qg_sdam_multibyte_write(QG_SDAM_VALID_OFFSET, data, length);
	if (rc < 0)
		pr_err("Failed to clear SDAM rc=%d\n", rc);

	return rc;
}

#define MAX_QG_OK_RETRIES	20
static int qg_handle_battery_insertion(struct qpnp_qg *chip)
{
	int rc, count = 0;
	u32 ocv_uv = 0, ocv_raw = 0;
	u8 reg = 0;

	do {
		rc = qg_read(chip, chip->qg_base + QG_STATUS1_REG, &reg, 1);
		if (rc < 0) {
			pr_err("Failed to read STATUS1_REG rc=%d\n", rc);
			return rc;
		}

		if (reg & QG_OK_BIT)
			break;

		msleep(200);
		count++;
	} while (count < MAX_QG_OK_RETRIES);

	if (count == MAX_QG_OK_RETRIES) {
		qg_dbg(chip, QG_DEBUG_STATUS, "QG_OK not set!\n");
		return 0;
	}

	/* read S7 PON OCV */
	rc = qg_read_ocv(chip, &ocv_uv, &ocv_raw, S7_PON_OCV);
	if (rc < 0) {
		pr_err("Failed to read PON OCV rc=%d\n", rc);
		return rc;
	}

	qg_dbg(chip, QG_DEBUG_STATUS,
		"S7_OCV on battery insertion = %duV\n", ocv_uv);

	chip->kdata.param[QG_GOOD_OCV_UV].data = ocv_uv;
	chip->kdata.param[QG_GOOD_OCV_UV].valid = true;
	/* clear all the userspace data */
	chip->kdata.param[QG_CLEAR_LEARNT_DATA].data = 1;
	chip->kdata.param[QG_CLEAR_LEARNT_DATA].valid = true;

	vote(chip->awake_votable, GOOD_OCV_VOTER, true, 0);
	/* signal the read thread */
	chip->data_ready = true;
	wake_up_interruptible(&chip->qg_wait_q);

	return 0;
}

static int qg_battery_status_update(struct qpnp_qg *chip)
{
	int rc;
	union power_supply_propval prop = {0, };

	if (!is_batt_available(chip))
		return 0;

	mutex_lock(&chip->data_lock);

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_PRESENT, &prop);
	if (rc < 0) {
		pr_err("Failed to get battery-present, rc=%d\n", rc);
		goto done;
	}

	if (chip->battery_missing && prop.intval) {
		pr_warn("Battery inserted!\n");
		rc = qg_handle_battery_insertion(chip);
		if (rc < 0)
			pr_err("Failed in battery-insertion rc=%d\n", rc);
	} else if (!chip->battery_missing && !prop.intval) {
		pr_warn("Battery removed!\n");
		rc = qg_handle_battery_removal(chip);
		if (rc < 0)
			pr_err("Failed in battery-removal rc=%d\n", rc);
	}

	chip->battery_missing = !prop.intval;

done:
	mutex_unlock(&chip->data_lock);
	return rc;
}


static void qg_status_change_work(struct work_struct *work)
{
	struct qpnp_qg *chip = container_of(work,
			struct qpnp_qg, qg_status_change_work);
	union power_supply_propval prop = {0, };
	int rc = 0, batt_temp = 0, batt_soc_32b = 0;

	if (!is_batt_available(chip)) {
		pr_debug("batt-psy not available\n");
		goto out;
	}

	rc = qg_battery_status_update(chip);
	if (rc < 0)
		pr_err("Failed to process battery status update rc=%d\n", rc);

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &prop);
	if (rc < 0)
		pr_err("Failed to get charge-type, rc=%d\n", rc);
	else
		chip->charge_type = prop.intval;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_STATUS, &prop);
	if (rc < 0)
		pr_err("Failed to get charger status, rc=%d\n", rc);
	else
		chip->charge_status = prop.intval;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_DONE, &prop);
	if (rc < 0)
		pr_err("Failed to get charge done status, rc=%d\n", rc);
	else
		chip->charge_done = prop.intval;

	qg_dbg(chip, QG_DEBUG_STATUS, "charge_status=%d charge_done=%d\n",
			chip->charge_status, chip->charge_done);

	rc = qg_parallel_status_update(chip);
	if (rc < 0)
		pr_err("Failed to update parallel-status, rc=%d\n", rc);

	rc = qg_usb_status_update(chip);
	if (rc < 0)
		pr_err("Failed to update usb status, rc=%d\n", rc);

	cycle_count_update(chip->counter,
			DIV_ROUND_CLOSEST(chip->msoc * 255, 100),
			chip->charge_status, chip->charge_done,
			chip->usb_present);

	if (!chip->dt.cl_disable) {
		rc = qg_get_battery_temp(chip, &batt_temp);
		if (rc < 0) {
			pr_err("Failed to read BATT_TEMP at PON rc=%d\n", rc);
		} else {
			batt_soc_32b = div64_u64(
					chip->batt_soc * BATT_SOC_32BIT,
					QG_SOC_FULL);
			cap_learning_update(chip->cl, batt_temp, batt_soc_32b,
				chip->charge_status, chip->charge_done,
				chip->usb_present, false);
		}
	}
	rc = qg_charge_full_update(chip);
	if (rc < 0)
		pr_err("Failed in charge_full_update, rc=%d\n", rc);

	ttf_update(chip->ttf, chip->usb_present);
out:
	pm_relax(chip->dev);
}

static int qg_notifier_cb(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct qpnp_qg *chip = container_of(nb, struct qpnp_qg, nb);

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (work_pending(&chip->qg_status_change_work))
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0)
		|| (strcmp(psy->desc->name, "parallel") == 0)
		|| (strcmp(psy->desc->name, "usb") == 0)) {
		/*
		 * We cannot vote for awake votable here as that takes
		 * a mutex lock and this is executed in an atomic context.
		 */
		pm_stay_awake(chip->dev);
		schedule_work(&chip->qg_status_change_work);
	}

	return NOTIFY_OK;
}

static int qg_init_psy(struct qpnp_qg *chip)
{
	struct power_supply_config qg_psy_cfg;
	int rc;

	qg_psy_cfg.drv_data = chip;
	qg_psy_cfg.of_node = NULL;
	qg_psy_cfg.supplied_to = NULL;
	qg_psy_cfg.num_supplicants = 0;
	chip->qg_psy = devm_power_supply_register(chip->dev,
				&qg_psy_desc, &qg_psy_cfg);
	if (IS_ERR_OR_NULL(chip->qg_psy)) {
		pr_err("Failed to register qg_psy rc = %ld\n",
				PTR_ERR(chip->qg_psy));
		return -ENODEV;
	}

	chip->nb.notifier_call = qg_notifier_cb;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0)
		pr_err("Failed register psy notifier rc = %d\n", rc);

	return rc;
}

static ssize_t qg_device_read(struct file *file, char __user *buf, size_t count,
			  loff_t *ppos)
{
	int rc;
	struct qpnp_qg *chip = file->private_data;
	unsigned long data_size = sizeof(chip->kdata);

	/* non-blocking access, return */
	if (!chip->data_ready && (file->f_flags & O_NONBLOCK))
		return 0;

	/* blocking access wait on data_ready */
	if (!(file->f_flags & O_NONBLOCK)) {
		rc = wait_event_interruptible(chip->qg_wait_q,
					chip->data_ready);
		if (rc < 0) {
			pr_debug("Failed wait! rc=%d\n", rc);
			return rc;
		}
	}

	mutex_lock(&chip->data_lock);

	if (!chip->data_ready) {
		pr_debug("No Data, false wakeup\n");
		rc = -EFAULT;
		goto fail_read;
	}


	if (copy_to_user(buf, &chip->kdata, data_size)) {
		pr_err("Failed in copy_to_user\n");
		rc = -EFAULT;
		goto fail_read;
	}
	chip->data_ready = false;

	/* release all wake sources */
	vote(chip->awake_votable, GOOD_OCV_VOTER, false, 0);
	vote(chip->awake_votable, FIFO_DONE_VOTER, false, 0);
	vote(chip->awake_votable, FIFO_RT_DONE_VOTER, false, 0);
	vote(chip->awake_votable, SUSPEND_DATA_VOTER, false, 0);

	qg_dbg(chip, QG_DEBUG_DEVICE,
		"QG device read complete Seq_no=%u Size=%ld\n",
				chip->kdata.seq_no, data_size);

	/* clear data */
	memset(&chip->kdata, 0, sizeof(chip->kdata));

	mutex_unlock(&chip->data_lock);

	return data_size;

fail_read:
	mutex_unlock(&chip->data_lock);
	return rc;
}

static ssize_t qg_device_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	int rc = -EINVAL;
	struct qpnp_qg *chip = file->private_data;
	unsigned long data_size = sizeof(chip->udata);

	mutex_lock(&chip->data_lock);
	if (count == 0) {
		pr_err("No data!\n");
		goto fail;
	}

	if (count != 0 && count < data_size) {
		pr_err("Invalid datasize %zu expected %lu\n", count, data_size);
		goto fail;
	}

	if (copy_from_user(&chip->udata, buf, data_size)) {
		pr_err("Failed in copy_from_user\n");
		rc = -EFAULT;
		goto fail;
	}

	rc = data_size;
	vote(chip->awake_votable, UDATA_READY_VOTER, true, 0);
	schedule_work(&chip->udata_work);
	qg_dbg(chip, QG_DEBUG_DEVICE, "QG write complete size=%d\n", rc);
fail:
	mutex_unlock(&chip->data_lock);
	return rc;
}

static unsigned int qg_device_poll(struct file *file, poll_table *wait)
{
	struct qpnp_qg *chip = file->private_data;
	unsigned int mask = 0;

	poll_wait(file, &chip->qg_wait_q, wait);

	if (chip->data_ready)
		mask = POLLIN | POLLRDNORM;

	return mask;
}

static int qg_device_open(struct inode *inode, struct file *file)
{
	struct qpnp_qg *chip = container_of(inode->i_cdev,
				struct qpnp_qg, qg_cdev);

	file->private_data = chip;
	chip->qg_device_open = true;
	qg_dbg(chip, QG_DEBUG_DEVICE, "QG device opened!\n");

	return 0;
}

static int qg_device_release(struct inode *inode, struct file *file)
{
	struct qpnp_qg *chip = container_of(inode->i_cdev,
				struct qpnp_qg, qg_cdev);

	file->private_data = chip;
	chip->qg_device_open = false;
	qg_dbg(chip, QG_DEBUG_DEVICE, "QG device closed!\n");

	return 0;
}

static const struct file_operations qg_fops = {
	.owner		= THIS_MODULE,
	.open		= qg_device_open,
	.release	= qg_device_release,
	.read		= qg_device_read,
	.write		= qg_device_write,
	.poll		= qg_device_poll,
};

static int qg_register_device(struct qpnp_qg *chip)
{
	int rc;

	rc = alloc_chrdev_region(&chip->dev_no, 0, 1, "qg");
	if (rc < 0) {
		pr_err("Failed to allocate chardev rc=%d\n", rc);
		return rc;
	}

	cdev_init(&chip->qg_cdev, &qg_fops);
	rc = cdev_add(&chip->qg_cdev, chip->dev_no, 1);
	if (rc < 0) {
		pr_err("Failed to cdev_add rc=%d\n", rc);
		goto unregister_chrdev;
	}

	chip->qg_class = class_create(THIS_MODULE, "qg");
	if (IS_ERR_OR_NULL(chip->qg_class)) {
		pr_err("Failed to create qg class\n");
		rc = -EINVAL;
		goto delete_cdev;
	}
	chip->qg_device = device_create(chip->qg_class, NULL, chip->dev_no,
					NULL, "qg");
	if (IS_ERR(chip->qg_device)) {
		pr_err("Failed to create qg_device\n");
		rc = -EINVAL;
		goto destroy_class;
	}

	qg_dbg(chip, QG_DEBUG_DEVICE, "'/dev/qg' successfully created\n");

	return 0;

destroy_class:
	class_destroy(chip->qg_class);
delete_cdev:
	cdev_del(&chip->qg_cdev);
unregister_chrdev:
	unregister_chrdev_region(chip->dev_no, 1);
	return rc;
}

#define BID_RPULL_OHM		100000
#define BID_VREF_MV		1875
static int get_batt_id_ohm(struct qpnp_qg *chip, u32 *batt_id_ohm)
{
	int rc, batt_id_mv;
	int64_t denom;

	/* Read battery-id */
	rc = iio_read_channel_processed(chip->batt_id_chan, &batt_id_mv);
	if (rc) {
		pr_err("Failed to read BATT_ID over ADC, rc=%d\n", rc);
		return rc;
	}

	batt_id_mv = div_s64(batt_id_mv, 1000);
	if (batt_id_mv == 0) {
		pr_debug("batt_id_mv = 0 from ADC\n");
		return 0;
	}

	denom = div64_s64(BID_VREF_MV * 1000, batt_id_mv) - 1000;
	if (denom <= 0) {
		/* batt id connector might be open, return 0 kohms */
		return 0;
	}

	*batt_id_ohm = div64_u64(BID_RPULL_OHM * 1000 + denom / 2, denom);

	qg_dbg(chip, QG_DEBUG_PROFILE, "batt_id_mv=%d, batt_id_ohm=%d\n",
					batt_id_mv, *batt_id_ohm);

	return 0;
}

static int qg_load_battery_profile(struct qpnp_qg *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *batt_node, *profile_node;
	int rc;

	batt_node = of_find_node_by_name(node, "qcom,battery-data");
	if (!batt_node) {
		pr_err("Batterydata not available\n");
		return -ENXIO;
	}

	profile_node = of_batterydata_get_best_profile(batt_node,
				chip->batt_id_ohm / 1000, NULL);
	if (IS_ERR(profile_node)) {
		rc = PTR_ERR(profile_node);
		pr_err("Failed to detect valid QG battery profile %d\n", rc);
		return rc;
	}

	rc = of_property_read_string(profile_node, "qcom,battery-type",
				&chip->bp.batt_type_str);
	if (rc < 0) {
		pr_err("Failed to detect battery type rc:%d\n", rc);
		return rc;
	}

	rc = qg_batterydata_init(profile_node);
	if (rc < 0) {
		pr_err("Failed to initialize battery-profile rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(profile_node, "qcom,max-voltage-uv",
				&chip->bp.float_volt_uv);
	if (rc < 0) {
		pr_err("Failed to read battery float-voltage rc:%d\n", rc);
		chip->bp.float_volt_uv = -EINVAL;
	}

	rc = of_property_read_u32(profile_node, "qcom,fastchg-current-ma",
				&chip->bp.fastchg_curr_ma);
	if (rc < 0) {
		pr_err("Failed to read battery fastcharge current rc:%d\n", rc);
		chip->bp.fastchg_curr_ma = -EINVAL;
	}

	rc = of_property_read_u32(profile_node, "qcom,qg-batt-profile-ver",
				&chip->bp.qg_profile_version);
	if (rc < 0) {
		pr_err("Failed to read QG profile version rc:%d\n", rc);
		chip->bp.qg_profile_version = -EINVAL;
	}

	qg_dbg(chip, QG_DEBUG_PROFILE, "profile=%s FV=%duV FCC=%dma\n",
			chip->bp.batt_type_str, chip->bp.float_volt_uv,
			chip->bp.fastchg_curr_ma);

	return 0;
}

static int qg_setup_battery(struct qpnp_qg *chip)
{
	int rc;

	if (!is_battery_present(chip)) {
		qg_dbg(chip, QG_DEBUG_PROFILE, "Battery Missing!\n");
		chip->battery_missing = true;
		chip->profile_loaded = false;
		chip->soc_reporting_ready = true;
	} else {
		/* battery present */
		rc = get_batt_id_ohm(chip, &chip->batt_id_ohm);
		if (rc < 0) {
			pr_err("Failed to detect batt_id rc=%d\n", rc);
			chip->profile_loaded = false;
		} else {
			rc = qg_load_battery_profile(chip);
			if (rc < 0) {
				pr_err("Failed to load battery-profile rc=%d\n",
								rc);
				chip->profile_loaded = false;
				chip->soc_reporting_ready = true;
			} else {
				chip->profile_loaded = true;
			}
		}
	}

	qg_dbg(chip, QG_DEBUG_PROFILE, "battery_missing=%d batt_id_ohm=%d Ohm profile_loaded=%d profile=%s\n",
			chip->battery_missing, chip->batt_id_ohm,
			chip->profile_loaded, chip->bp.batt_type_str);

	return 0;
}


static struct ocv_all ocv[] = {
	[S7_PON_OCV] = { 0, 0, "S7_PON_OCV"},
	[S3_GOOD_OCV] = { 0, 0, "S3_GOOD_OCV"},
	[S3_LAST_OCV] = { 0, 0, "S3_LAST_OCV"},
	[SDAM_PON_OCV] = { 0, 0, "SDAM_PON_OCV"},
};

#define S7_ERROR_MARGIN_UV		20000
static int qg_determine_pon_soc(struct qpnp_qg *chip)
{
	int rc = 0, batt_temp = 0, i;
	bool use_pon_ocv = true;
	unsigned long rtc_sec = 0;
	u32 ocv_uv = 0, soc = 0, shutdown[SDAM_MAX] = {0};
	char ocv_type[20] = "NONE";

	if (!chip->profile_loaded) {
		qg_dbg(chip, QG_DEBUG_PON, "No Profile, skipping PON soc\n");
		return 0;
	}

	rc = get_rtc_time(&rtc_sec);
	if (rc < 0) {
		pr_err("Failed to read RTC time rc=%d\n", rc);
		goto use_pon_ocv;
	}

	rc = qg_sdam_read_all(shutdown);
	if (rc < 0) {
		pr_err("Failed to read shutdown params rc=%d\n", rc);
		goto use_pon_ocv;
	}

	qg_dbg(chip, QG_DEBUG_PON, "Shutdown: Valid=%d SOC=%d OCV=%duV time=%dsecs, time_now=%ldsecs\n",
			shutdown[SDAM_VALID],
			shutdown[SDAM_SOC],
			shutdown[SDAM_OCV_UV],
			shutdown[SDAM_TIME_SEC],
			rtc_sec);
	/*
	 * Use the shutdown SOC if
	 * 1. The device was powered off for < ignore_shutdown_time
	 * 2. SDAM read is a success & SDAM data is valid
	 */
	if (shutdown[SDAM_VALID] && is_between(0,
			chip->dt.ignore_shutdown_soc_secs,
			(rtc_sec - shutdown[SDAM_TIME_SEC]))) {
		use_pon_ocv = false;
		ocv_uv = shutdown[SDAM_OCV_UV];
		soc = shutdown[SDAM_SOC];
		strlcpy(ocv_type, "SHUTDOWN_SOC", 20);
		qg_dbg(chip, QG_DEBUG_PON, "Using SHUTDOWN_SOC @ PON\n");
	}

use_pon_ocv:
	if (use_pon_ocv == true) {
		rc = qg_get_battery_temp(chip, &batt_temp);
		if (rc) {
			pr_err("Failed to read BATT_TEMP at PON rc=%d\n", rc);
			goto done;
		}

		/* read all OCVs */
		for (i = S7_PON_OCV; i < PON_OCV_MAX; i++) {
			rc = qg_read_ocv(chip, &ocv[i].ocv_uv,
						&ocv[i].ocv_raw, i);
			if (rc < 0)
				pr_err("Failed to read %s OCV rc=%d\n",
						ocv[i].ocv_type, rc);
			else
				qg_dbg(chip, QG_DEBUG_PON, "%s OCV=%d\n",
					ocv[i].ocv_type, ocv[i].ocv_uv);
		}

		if (ocv[S3_LAST_OCV].ocv_raw == FIFO_V_RESET_VAL) {
			if (!ocv[SDAM_PON_OCV].ocv_uv) {
				strlcpy(ocv_type, "S7_PON_SOC", 20);
				ocv_uv = ocv[S7_PON_OCV].ocv_uv;
			} else if (ocv[SDAM_PON_OCV].ocv_uv <=
					ocv[S7_PON_OCV].ocv_uv) {
				strlcpy(ocv_type, "S7_PON_SOC", 20);
				ocv_uv = ocv[S7_PON_OCV].ocv_uv;
			} else if (!shutdown[SDAM_VALID] &&
				((ocv[SDAM_PON_OCV].ocv_uv -
					ocv[S7_PON_OCV].ocv_uv) >
					S7_ERROR_MARGIN_UV)) {
				strlcpy(ocv_type, "S7_PON_SOC", 20);
				ocv_uv = ocv[S7_PON_OCV].ocv_uv;
			} else {
				strlcpy(ocv_type, "SDAM_PON_SOC", 20);
				ocv_uv = ocv[SDAM_PON_OCV].ocv_uv;
			}
		} else {
			if (ocv[S3_LAST_OCV].ocv_uv >= ocv[S7_PON_OCV].ocv_uv) {
				strlcpy(ocv_type, "S3_LAST_SOC", 20);
				ocv_uv = ocv[S3_LAST_OCV].ocv_uv;
			} else {
				strlcpy(ocv_type, "S7_PON_SOC", 20);
				ocv_uv = ocv[S7_PON_OCV].ocv_uv;
			}
		}

		ocv_uv = CAP(QG_MIN_OCV_UV, QG_MAX_OCV_UV, ocv_uv);
		rc = lookup_soc_ocv(&soc, ocv_uv, batt_temp, false);
		if (rc < 0) {
			pr_err("Failed to lookup SOC@PON rc=%d\n", rc);
			goto done;
		}
	}
done:
	if (rc < 0) {
		pr_err("Failed to get %s @ PON, rc=%d\n", ocv_type, rc);
		return rc;
	}

	chip->pon_soc = chip->catch_up_soc = chip->msoc = soc;
	chip->kdata.param[QG_PON_OCV_UV].data = ocv_uv;
	chip->kdata.param[QG_PON_OCV_UV].valid = true;

	/* write back to SDAM */
	chip->sdam_data[SDAM_SOC] = soc;
	chip->sdam_data[SDAM_OCV_UV] = ocv_uv;
	chip->sdam_data[SDAM_VALID] = 1;

	rc = qg_write_monotonic_soc(chip, chip->msoc);
	if (rc < 0)
		pr_err("Failed to update MSOC register rc=%d\n", rc);

	rc = qg_store_soc_params(chip);
	if (rc < 0)
		pr_err("Failed to update sdam params rc=%d\n", rc);

	pr_info("using %s @ PON ocv_uv=%duV soc=%d\n",
			ocv_type, ocv_uv, chip->msoc);

	/* SOC reporting is now ready */
	chip->soc_reporting_ready = 1;

	return 0;
}

static int qg_set_wa_flags(struct qpnp_qg *chip)
{
	switch (chip->pmic_rev_id->pmic_subtype) {
	case PMI632_SUBTYPE:
		chip->wa_flags |= QG_RECHARGE_SOC_WA;
		if (chip->pmic_rev_id->rev4 == PMI632_V1P0_REV4)
			chip->wa_flags |= QG_VBAT_LOW_WA;
		break;
	default:
		pr_err("Unsupported PMIC subtype %d\n",
			chip->pmic_rev_id->pmic_subtype);
		return -EINVAL;
	}

	qg_dbg(chip, QG_DEBUG_PON, "wa_flags = %x\n", chip->wa_flags);

	return 0;
}

#define ADC_CONV_DLY_512MS		0xA
static int qg_hw_init(struct qpnp_qg *chip)
{
	int rc, temp;
	u8 reg;

	rc = qg_set_wa_flags(chip);
	if (rc < 0) {
		pr_err("Failed to update PMIC type flags, rc=%d\n", rc);
		return rc;
	}

	rc = qg_master_hold(chip, true);
	if (rc < 0) {
		pr_err("Failed to hold master, rc=%d\n", rc);
		goto done_fifo;
	}

	rc = qg_process_rt_fifo(chip);
	if (rc < 0) {
		pr_err("Failed to process FIFO real-time, rc=%d\n", rc);
		goto done_fifo;
	}

	/* update the changed S2 fifo DT parameters */
	if (chip->dt.s2_fifo_length > 0) {
		rc = qg_update_fifo_length(chip, chip->dt.s2_fifo_length);
		if (rc < 0)
			goto done_fifo;
	}

	if (chip->dt.s2_acc_length > 0) {
		reg = ilog2(chip->dt.s2_acc_length) - 1;
		rc = qg_masked_write(chip, chip->qg_base +
				QG_S2_NORMAL_MEAS_CTL2_REG,
				NUM_OF_ACCUM_MASK, reg);
		if (rc < 0) {
			pr_err("Failed to write S2 ACC length, rc=%d\n", rc);
			goto done_fifo;
		}
	}

	if (chip->dt.s2_acc_intvl_ms > 0) {
		reg = chip->dt.s2_acc_intvl_ms / 10;
		rc = qg_write(chip, chip->qg_base +
				QG_S2_NORMAL_MEAS_CTL3_REG,
				&reg, 1);
		if (rc < 0) {
			pr_err("Failed to write S2 ACC intrvl, rc=%d\n", rc);
			goto done_fifo;
		}
	}

	/* signal the read thread */
	chip->data_ready = true;
	wake_up_interruptible(&chip->qg_wait_q);

done_fifo:
	rc = qg_master_hold(chip, false);
	if (rc < 0) {
		pr_err("Failed to release master, rc=%d\n", rc);
		return rc;
	}
	chip->last_fifo_update_time = ktime_get();

	if (chip->dt.ocv_timer_expiry_min != -EINVAL) {
		if (chip->dt.ocv_timer_expiry_min < 2)
			chip->dt.ocv_timer_expiry_min = 2;
		else if (chip->dt.ocv_timer_expiry_min > 30)
			chip->dt.ocv_timer_expiry_min = 30;

		reg = (chip->dt.ocv_timer_expiry_min - 2) / 4;
		rc = qg_masked_write(chip,
			chip->qg_base + QG_S3_SLEEP_OCV_MEAS_CTL4_REG,
			SLEEP_IBAT_QUALIFIED_LENGTH_MASK, reg);
		if (rc < 0) {
			pr_err("Failed to write OCV timer, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.ocv_tol_threshold_uv != -EINVAL) {
		if (chip->dt.ocv_tol_threshold_uv < 0)
			chip->dt.ocv_tol_threshold_uv = 0;
		else if (chip->dt.ocv_tol_threshold_uv > 12262)
			chip->dt.ocv_tol_threshold_uv = 12262;

		reg = chip->dt.ocv_tol_threshold_uv / 195;
		rc = qg_masked_write(chip,
			chip->qg_base + QG_S3_SLEEP_OCV_TREND_CTL2_REG,
			TREND_TOL_MASK, reg);
		if (rc < 0) {
			pr_err("Failed to write OCV tol-thresh, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.s3_entry_fifo_length != -EINVAL) {
		if (chip->dt.s3_entry_fifo_length < 1)
			chip->dt.s3_entry_fifo_length = 1;
		else if (chip->dt.s3_entry_fifo_length > 8)
			chip->dt.s3_entry_fifo_length = 8;

		reg = chip->dt.s3_entry_fifo_length - 1;
		rc = qg_masked_write(chip,
			chip->qg_base + QG_S3_SLEEP_OCV_IBAT_CTL1_REG,
			SLEEP_IBAT_QUALIFIED_LENGTH_MASK, reg);
		if (rc < 0) {
			pr_err("Failed to write S3-entry fifo-length, rc=%d\n",
							rc);
			return rc;
		}
	}

	if (chip->dt.s3_entry_ibat_ua != -EINVAL) {
		if (chip->dt.s3_entry_ibat_ua < 0)
			chip->dt.s3_entry_ibat_ua = 0;
		else if (chip->dt.s3_entry_ibat_ua > 155550)
			chip->dt.s3_entry_ibat_ua = 155550;

		reg = chip->dt.s3_entry_ibat_ua / 610;
		rc = qg_write(chip, chip->qg_base +
				QG_S3_ENTRY_IBAT_THRESHOLD_REG,
				&reg, 1);
		if (rc < 0) {
			pr_err("Failed to write S3-entry ibat-uA, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.s3_exit_ibat_ua != -EINVAL) {
		if (chip->dt.s3_exit_ibat_ua < 0)
			chip->dt.s3_exit_ibat_ua = 0;
		else if (chip->dt.s3_exit_ibat_ua > 155550)
			chip->dt.s3_exit_ibat_ua = 155550;

		rc = qg_read(chip, chip->qg_base +
				QG_S3_ENTRY_IBAT_THRESHOLD_REG,
				&reg, 1);
		if (rc < 0) {
			pr_err("Failed to read S3-entry ibat-uA, rc=%d", rc);
			return rc;
		}
		temp = reg * 610;
		if (chip->dt.s3_exit_ibat_ua < temp)
			chip->dt.s3_exit_ibat_ua = temp;
		else
			chip->dt.s3_exit_ibat_ua -= temp;

		reg = chip->dt.s3_exit_ibat_ua / 610;
		rc = qg_write(chip,
			chip->qg_base + QG_S3_EXIT_IBAT_THRESHOLD_REG,
			&reg, 1);
		if (rc < 0) {
			pr_err("Failed to write S3-entry ibat-uA, rc=%d\n", rc);
			return rc;
		}
	}

	/* vbat based configs */
	if (chip->dt.vbatt_low_mv < 0)
		chip->dt.vbatt_low_mv = 0;
	else if (chip->dt.vbatt_low_mv > 12750)
		chip->dt.vbatt_low_mv = 12750;

	if (chip->dt.vbatt_empty_mv < 0)
		chip->dt.vbatt_empty_mv = 0;
	else if (chip->dt.vbatt_empty_mv > 12750)
		chip->dt.vbatt_empty_mv = 12750;

	if (chip->dt.vbatt_empty_cold_mv < 0)
		chip->dt.vbatt_empty_cold_mv = 0;
	else if (chip->dt.vbatt_empty_cold_mv > 12750)
		chip->dt.vbatt_empty_cold_mv = 12750;

	rc = qg_vbat_thresholds_config(chip);
	if (rc < 0) {
		pr_err("Failed to configure VBAT empty/low rc=%d\n", rc);
		return rc;
	}

	/* disable S5 */
	rc = qg_masked_write(chip, chip->qg_base +
				QG_S5_OCV_VALIDATE_MEAS_CTL1_REG,
				ALLOW_S5_BIT, 0);
	if (rc < 0)
		pr_err("Failed to disable S5 rc=%d\n", rc);

	/* change PON OCV time to 512ms */
	rc = qg_masked_write(chip, chip->qg_base +
				QG_S7_PON_OCV_MEAS_CTL1_REG,
				ADC_CONV_DLY_MASK,
				ADC_CONV_DLY_512MS);
	if (rc < 0)
		pr_err("Failed to reconfigure S7-delay rc=%d\n", rc);


	return 0;
}

static int qg_post_init(struct qpnp_qg *chip)
{
	/* disable all IRQs if profile is not loaded */
	if (!chip->profile_loaded) {
		vote(chip->vbatt_irq_disable_votable,
				PROFILE_IRQ_DISABLE, true, 0);
		vote(chip->fifo_irq_disable_votable,
				PROFILE_IRQ_DISABLE, true, 0);
		vote(chip->good_ocv_irq_disable_votable,
				PROFILE_IRQ_DISABLE, true, 0);
	} else {
		/* disable GOOD_OCV IRQ at init */
		vote(chip->good_ocv_irq_disable_votable,
				QG_INIT_STATE_IRQ_DISABLE, true, 0);
	}

	/* restore ESR data */
	if (!chip->dt.esr_disable)
		qg_retrieve_esr_params(chip);

	return 0;
}

static int qg_get_irq_index_byname(const char *irq_name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(qg_irqs); i++) {
		if (strcmp(qg_irqs[i].name, irq_name) == 0)
			return i;
	}

	return -ENOENT;
}

static int qg_request_interrupt(struct qpnp_qg *chip,
		struct device_node *node, const char *irq_name)
{
	int rc, irq, irq_index;

	irq = of_irq_get_byname(node, irq_name);
	if (irq < 0) {
		pr_err("Failed to get irq %s byname\n", irq_name);
		return irq;
	}

	irq_index = qg_get_irq_index_byname(irq_name);
	if (irq_index < 0) {
		pr_err("%s is not a defined irq\n", irq_name);
		return irq_index;
	}

	if (!qg_irqs[irq_index].handler)
		return 0;

	rc = devm_request_threaded_irq(chip->dev, irq, NULL,
				qg_irqs[irq_index].handler,
				IRQF_ONESHOT, irq_name, chip);
	if (rc < 0) {
		pr_err("Failed to request irq %d\n", irq);
		return rc;
	}

	qg_irqs[irq_index].irq = irq;
	if (qg_irqs[irq_index].wake)
		enable_irq_wake(irq);

	qg_dbg(chip, QG_DEBUG_PON, "IRQ %s registered wakeable=%d\n",
			qg_irqs[irq_index].name, qg_irqs[irq_index].wake);

	return 0;
}

static int qg_request_irqs(struct qpnp_qg *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *child;
	const char *name;
	struct property *prop;
	int rc = 0;

	for_each_available_child_of_node(node, child) {
		of_property_for_each_string(child, "interrupt-names",
					    prop, name) {
			rc = qg_request_interrupt(chip, child, name);
			if (rc < 0)
				return rc;
		}
	}


	return 0;
}

#define QG_TTF_ITERM_DELTA_MA		1
static int qg_alg_init(struct qpnp_qg *chip)
{
	struct cycle_counter *counter;
	struct cap_learning *cl;
	struct ttf *ttf;
	struct device_node *node = chip->dev->of_node;
	int rc;

	counter = devm_kzalloc(chip->dev, sizeof(*counter), GFP_KERNEL);
	if (!counter)
		return -ENOMEM;

	counter->restore_count = qg_restore_cycle_count;
	counter->store_count = qg_store_cycle_count;
	counter->data = chip;

	rc = cycle_count_init(counter);
	if (rc < 0) {
		dev_err(chip->dev, "Error in initializing cycle counter, rc:%d\n",
			rc);
		counter->data = NULL;
		devm_kfree(chip->dev, counter);
		return rc;
	}

	chip->counter = counter;

	ttf = devm_kzalloc(chip->dev, sizeof(*ttf), GFP_KERNEL);
	if (!ttf)
		return -ENOMEM;

	ttf->get_ttf_param = qg_get_ttf_param;
	ttf->awake_voter = qg_ttf_awake_voter;
	ttf->iterm_delta = QG_TTF_ITERM_DELTA_MA;
	ttf->data = chip;

	rc = ttf_tte_init(ttf);
	if (rc < 0) {
		dev_err(chip->dev, "Error in initializing ttf, rc:%d\n",
			rc);
		ttf->data = NULL;
		counter->data = NULL;
		devm_kfree(chip->dev, ttf);
		devm_kfree(chip->dev, counter);
		return rc;
	}

	chip->ttf = ttf;

	chip->dt.cl_disable = of_property_read_bool(node,
					"qcom,cl-disable");

	/*Return if capacity learning is disabled*/
	if (chip->dt.cl_disable)
		return 0;

	cl = devm_kzalloc(chip->dev, sizeof(*cl), GFP_KERNEL);
	if (!cl)
		return -ENOMEM;

	cl->cc_soc_max = QG_SOC_FULL;
	cl->get_cc_soc = qg_get_cc_soc;
	cl->get_learned_capacity = qg_get_learned_capacity;
	cl->store_learned_capacity = qg_store_learned_capacity;
	cl->data = chip;

	rc = cap_learning_init(cl);
	if (rc < 0) {
		dev_err(chip->dev, "Error in initializing capacity learning, rc:%d\n",
			rc);
		counter->data = NULL;
		cl->data = NULL;
		devm_kfree(chip->dev, counter);
		devm_kfree(chip->dev, ttf);
		devm_kfree(chip->dev, cl);
		return rc;
	}

	chip->cl = cl;
	return 0;
}

#define DEFAULT_VBATT_EMPTY_MV		3200
#define DEFAULT_VBATT_EMPTY_COLD_MV	3000
#define DEFAULT_VBATT_CUTOFF_MV		3400
#define DEFAULT_VBATT_LOW_MV		3500
#define DEFAULT_VBATT_LOW_COLD_MV	3800
#define DEFAULT_ITERM_MA		100
#define DEFAULT_S2_FIFO_LENGTH		5
#define DEFAULT_S2_VBAT_LOW_LENGTH	2
#define DEFAULT_S2_ACC_LENGTH		128
#define DEFAULT_S2_ACC_INTVL_MS		100
#define DEFAULT_DELTA_SOC		1
#define DEFAULT_SHUTDOWN_SOC_SECS	360
#define DEFAULT_COLD_TEMP_THRESHOLD	0
#define DEFAULT_CL_MIN_START_SOC	10
#define DEFAULT_CL_MAX_START_SOC	15
#define DEFAULT_CL_MIN_TEMP_DECIDEGC	150
#define DEFAULT_CL_MAX_TEMP_DECIDEGC	500
#define DEFAULT_CL_MAX_INC_DECIPERC	10
#define DEFAULT_CL_MAX_DEC_DECIPERC	20
#define DEFAULT_CL_MIN_LIM_DECIPERC	500
#define DEFAULT_CL_MAX_LIM_DECIPERC	100
#define DEFAULT_ESR_QUAL_CURRENT_UA	130000
#define DEFAULT_ESR_QUAL_VBAT_UV	7000
#define DEFAULT_ESR_DISABLE_SOC		1000
#define ESR_CHG_MIN_IBAT_UA		(-450000)
static int qg_parse_dt(struct qpnp_qg *chip)
{
	int rc = 0;
	struct device_node *revid_node, *child, *node = chip->dev->of_node;
	u32 base, temp;
	u8 type;

	if (!node)  {
		pr_err("Failed to find device-tree node\n");
		return -ENXIO;
	}

	revid_node = of_parse_phandle(node, "qcom,pmic-revid", 0);
	if (!revid_node) {
		pr_err("Missing qcom,pmic-revid property - driver failed\n");
		return -EINVAL;
	}

	chip->pmic_rev_id = get_revid_data(revid_node);
	of_node_put(revid_node);
	if (IS_ERR_OR_NULL(chip->pmic_rev_id)) {
		pr_err("Failed to get pmic_revid, rc=%ld\n",
			PTR_ERR(chip->pmic_rev_id));
		/*
		 * the revid peripheral must be registered, any failure
		 * here only indicates that the rev-id module has not
		 * probed yet.
		 */
		return -EPROBE_DEFER;
	}

	qg_dbg(chip, QG_DEBUG_PON, "PMIC subtype %d Digital major %d\n",
		chip->pmic_rev_id->pmic_subtype, chip->pmic_rev_id->rev4);

	for_each_available_child_of_node(node, child) {
		rc = of_property_read_u32(child, "reg", &base);
		if (rc < 0) {
			pr_err("Failed to read base address, rc=%d\n", rc);
			return rc;
		}

		rc = qg_read(chip, base + PERPH_TYPE_REG, &type, 1);
		if (rc < 0) {
			pr_err("Failed to read type, rc=%d\n", rc);
			return rc;
		}

		switch (type) {
		case QG_TYPE:
			chip->qg_base = base;
			break;
		default:
			break;
		}
	}

	if (!chip->qg_base) {
		pr_err("QG device node missing\n");
		return -EINVAL;
	}

	/* S2 state params */
	rc = of_property_read_u32(node, "qcom,s2-fifo-length", &temp);
	if (rc < 0)
		chip->dt.s2_fifo_length = DEFAULT_S2_FIFO_LENGTH;
	else
		chip->dt.s2_fifo_length = temp;

	rc = of_property_read_u32(node, "qcom,s2-vbat-low-fifo-length", &temp);
	if (rc < 0)
		chip->dt.s2_vbat_low_fifo_length = DEFAULT_S2_VBAT_LOW_LENGTH;
	else
		chip->dt.s2_vbat_low_fifo_length = temp;

	rc = of_property_read_u32(node, "qcom,s2-acc-length", &temp);
	if (rc < 0)
		chip->dt.s2_acc_length = DEFAULT_S2_ACC_LENGTH;
	else
		chip->dt.s2_acc_length = temp;

	rc = of_property_read_u32(node, "qcom,s2-acc-interval-ms", &temp);
	if (rc < 0)
		chip->dt.s2_acc_intvl_ms = DEFAULT_S2_ACC_INTVL_MS;
	else
		chip->dt.s2_acc_intvl_ms = temp;

	qg_dbg(chip, QG_DEBUG_PON, "DT: S2 FIFO length=%d low_vbat_length=%d acc_length=%d acc_interval=%d\n",
		chip->dt.s2_fifo_length, chip->dt.s2_vbat_low_fifo_length,
		chip->dt.s2_acc_length, chip->dt.s2_acc_intvl_ms);

	/* OCV params */
	rc = of_property_read_u32(node, "qcom,ocv-timer-expiry-min", &temp);
	if (rc < 0)
		chip->dt.ocv_timer_expiry_min = -EINVAL;
	else
		chip->dt.ocv_timer_expiry_min = temp;

	rc = of_property_read_u32(node, "qcom,ocv-tol-threshold-uv", &temp);
	if (rc < 0)
		chip->dt.ocv_tol_threshold_uv = -EINVAL;
	else
		chip->dt.ocv_tol_threshold_uv = temp;

	qg_dbg(chip, QG_DEBUG_PON, "DT: OCV timer_expiry =%dmin ocv_tol_threshold=%duV\n",
		chip->dt.ocv_timer_expiry_min, chip->dt.ocv_tol_threshold_uv);

	/* S3 sleep configuration */
	rc = of_property_read_u32(node, "qcom,s3-entry-fifo-length", &temp);
	if (rc < 0)
		chip->dt.s3_entry_fifo_length = -EINVAL;
	else
		chip->dt.s3_entry_fifo_length = temp;

	rc = of_property_read_u32(node, "qcom,s3-entry-ibat-ua", &temp);
	if (rc < 0)
		chip->dt.s3_entry_ibat_ua = -EINVAL;
	else
		chip->dt.s3_entry_ibat_ua = temp;

	rc = of_property_read_u32(node, "qcom,s3-exit-ibat-ua", &temp);
	if (rc < 0)
		chip->dt.s3_exit_ibat_ua = -EINVAL;
	else
		chip->dt.s3_exit_ibat_ua = temp;

	/* VBAT thresholds */
	rc = of_property_read_u32(node, "qcom,vbatt-empty-mv", &temp);
	if (rc < 0)
		chip->dt.vbatt_empty_mv = DEFAULT_VBATT_EMPTY_MV;
	else
		chip->dt.vbatt_empty_mv = temp;

	rc = of_property_read_u32(node, "qcom,vbatt-empty-cold-mv", &temp);
	if (rc < 0)
		chip->dt.vbatt_empty_cold_mv = DEFAULT_VBATT_EMPTY_COLD_MV;
	else
		chip->dt.vbatt_empty_cold_mv = temp;

	rc = of_property_read_u32(node, "qcom,cold-temp-threshold", &temp);
	if (rc < 0)
		chip->dt.cold_temp_threshold = DEFAULT_COLD_TEMP_THRESHOLD;
	else
		chip->dt.cold_temp_threshold = temp;

	rc = of_property_read_u32(node, "qcom,vbatt-low-mv", &temp);
	if (rc < 0)
		chip->dt.vbatt_low_mv = DEFAULT_VBATT_LOW_MV;
	else
		chip->dt.vbatt_low_mv = temp;

	rc = of_property_read_u32(node, "qcom,vbatt-low-cold-mv", &temp);
	if (rc < 0)
		chip->dt.vbatt_low_cold_mv = DEFAULT_VBATT_LOW_COLD_MV;
	else
		chip->dt.vbatt_low_cold_mv = temp;

	rc = of_property_read_u32(node, "qcom,vbatt-cutoff-mv", &temp);
	if (rc < 0)
		chip->dt.vbatt_cutoff_mv = DEFAULT_VBATT_CUTOFF_MV;
	else
		chip->dt.vbatt_cutoff_mv = temp;

	/* IBAT thresholds */
	rc = of_property_read_u32(node, "qcom,qg-iterm-ma", &temp);
	if (rc < 0)
		chip->dt.iterm_ma = DEFAULT_ITERM_MA;
	else
		chip->dt.iterm_ma = temp;

	rc = of_property_read_u32(node, "qcom,delta-soc", &temp);
	if (rc < 0)
		chip->dt.delta_soc = DEFAULT_DELTA_SOC;
	else
		chip->dt.delta_soc = temp;

	rc = of_property_read_u32(node, "qcom,ignore-shutdown-soc-secs", &temp);
	if (rc < 0)
		chip->dt.ignore_shutdown_soc_secs = DEFAULT_SHUTDOWN_SOC_SECS;
	else
		chip->dt.ignore_shutdown_soc_secs = temp;

	chip->dt.hold_soc_while_full = of_property_read_bool(node,
					"qcom,hold-soc-while-full");

	chip->dt.linearize_soc = of_property_read_bool(node,
					"qcom,linearize-soc");

	rc = of_property_read_u32(node, "qcom,rbat-conn-mohm", &temp);
	if (rc < 0)
		chip->dt.rbat_conn_mohm = 0;
	else
		chip->dt.rbat_conn_mohm = temp;

	/* esr */
	chip->dt.esr_disable = of_property_read_bool(node,
					"qcom,esr-disable");

	chip->dt.esr_discharge_enable = of_property_read_bool(node,
					"qcom,esr-discharge-enable");

	rc = of_property_read_u32(node, "qcom,esr-qual-current-ua", &temp);
	if (rc < 0)
		chip->dt.esr_qual_i_ua = DEFAULT_ESR_QUAL_CURRENT_UA;
	else
		chip->dt.esr_qual_i_ua = temp;

	rc = of_property_read_u32(node, "qcom,esr-qual-vbatt-uv", &temp);
	if (rc < 0)
		chip->dt.esr_qual_v_uv = DEFAULT_ESR_QUAL_VBAT_UV;
	else
		chip->dt.esr_qual_v_uv = temp;

	rc = of_property_read_u32(node, "qcom,esr-disable-soc", &temp);
	if (rc < 0)
		chip->dt.esr_disable_soc = DEFAULT_ESR_DISABLE_SOC;
	else
		chip->dt.esr_disable_soc = temp * 100;

	rc = of_property_read_u32(node, "qcom,esr-chg-min-ibat-ua", &temp);
	if (rc < 0)
		chip->dt.esr_min_ibat_ua = ESR_CHG_MIN_IBAT_UA;
	else
		chip->dt.esr_min_ibat_ua = (int)temp;

	chip->dt.qg_ext_sense = of_property_read_bool(node, "qcom,qg-ext-sns");

	/* Capacity learning params*/
	if (!chip->dt.cl_disable) {
		chip->dt.cl_feedback_on = of_property_read_bool(node,
						"qcom,cl-feedback-on");

		rc = of_property_read_u32(node, "qcom,cl-min-start-soc", &temp);
		if (rc < 0)
			chip->cl->dt.min_start_soc = DEFAULT_CL_MIN_START_SOC;
		else
			chip->cl->dt.min_start_soc = temp;

		rc = of_property_read_u32(node, "qcom,cl-max-start-soc", &temp);
		if (rc < 0)
			chip->cl->dt.max_start_soc = DEFAULT_CL_MAX_START_SOC;
		else
			chip->cl->dt.max_start_soc = temp;

		rc = of_property_read_u32(node, "qcom,cl-min-temp", &temp);
		if (rc < 0)
			chip->cl->dt.min_temp = DEFAULT_CL_MIN_TEMP_DECIDEGC;
		else
			chip->cl->dt.min_temp = temp;

		rc = of_property_read_u32(node, "qcom,cl-max-temp", &temp);
		if (rc < 0)
			chip->cl->dt.max_temp = DEFAULT_CL_MAX_TEMP_DECIDEGC;
		else
			chip->cl->dt.max_temp = temp;

		rc = of_property_read_u32(node, "qcom,cl-max-increment", &temp);
		if (rc < 0)
			chip->cl->dt.max_cap_inc = DEFAULT_CL_MAX_INC_DECIPERC;
		else
			chip->cl->dt.max_cap_inc = temp;

		rc = of_property_read_u32(node, "qcom,cl-max-decrement", &temp);
		if (rc < 0)
			chip->cl->dt.max_cap_dec = DEFAULT_CL_MAX_DEC_DECIPERC;
		else
			chip->cl->dt.max_cap_dec = temp;

		rc = of_property_read_u32(node, "qcom,cl-min-limit", &temp);
		if (rc < 0)
			chip->cl->dt.min_cap_limit =
						DEFAULT_CL_MIN_LIM_DECIPERC;
		else
			chip->cl->dt.min_cap_limit = temp;

		rc = of_property_read_u32(node, "qcom,cl-max-limit", &temp);
		if (rc < 0)
			chip->cl->dt.max_cap_limit =
						DEFAULT_CL_MAX_LIM_DECIPERC;
		else
			chip->cl->dt.max_cap_limit = temp;

		qg_dbg(chip, QG_DEBUG_PON, "DT: cl_min_start_soc=%d cl_max_start_soc=%d cl_min_temp=%d cl_max_temp=%d\n",
			chip->cl->dt.min_start_soc, chip->cl->dt.max_start_soc,
			chip->cl->dt.min_temp, chip->cl->dt.max_temp);
	}
	qg_dbg(chip, QG_DEBUG_PON, "DT: vbatt_empty_mv=%dmV vbatt_low_mv=%dmV delta_soc=%d ext-sns=%d\n",
			chip->dt.vbatt_empty_mv, chip->dt.vbatt_low_mv,
			chip->dt.delta_soc, chip->dt.qg_ext_sense);

	return 0;
}

static int process_suspend(struct qpnp_qg *chip)
{
	u8 status = 0;
	int rc;
	u32 fifo_rt_length = 0, sleep_fifo_length = 0;

	/* skip if profile is not loaded */
	if (!chip->profile_loaded)
		return 0;

	cancel_delayed_work_sync(&chip->ttf->ttf_work);

	chip->suspend_data = false;

	/* ignore any suspend processing if we are charging */
	if (chip->charge_status == POWER_SUPPLY_STATUS_CHARGING) {
		qg_dbg(chip, QG_DEBUG_PM, "Charging @ suspend - ignore processing\n");
		return 0;
	}

	rc = get_fifo_length(chip, &fifo_rt_length, true);
	if (rc < 0) {
		pr_err("Failed to read FIFO RT count, rc=%d\n", rc);
		return rc;
	}

	rc = qg_read(chip, chip->qg_base + QG_S3_SLEEP_OCV_IBAT_CTL1_REG,
			(u8 *)&sleep_fifo_length, 1);
	if (rc < 0) {
		pr_err("Failed to read sleep FIFO count, rc=%d\n", rc);
		return rc;
	}
	sleep_fifo_length &= SLEEP_IBAT_QUALIFIED_LENGTH_MASK;
	/*
	 * If the real-time FIFO count is greater than
	 * the the #fifo to enter sleep, save the FIFO data
	 * and reset the fifo count.
	 */
	if (fifo_rt_length >= (chip->dt.s2_fifo_length - sleep_fifo_length)) {
		rc = qg_master_hold(chip, true);
		if (rc < 0) {
			pr_err("Failed to hold master, rc=%d\n", rc);
			return rc;
		}

		rc = qg_process_rt_fifo(chip);
		if (rc < 0) {
			pr_err("Failed to process FIFO real-time, rc=%d\n", rc);
			qg_master_hold(chip, false);
			return rc;
		}

		rc = qg_master_hold(chip, false);
		if (rc < 0) {
			pr_err("Failed to release master, rc=%d\n", rc);
			return rc;
		}
		/* FIFOs restarted */
		chip->last_fifo_update_time = ktime_get();

		chip->suspend_data = true;
	}

	/* read STATUS2 register to clear its last state */
	qg_read(chip, chip->qg_base + QG_STATUS2_REG, &status, 1);

	qg_dbg(chip, QG_DEBUG_PM, "FIFO rt_length=%d sleep_fifo_length=%d default_s2_count=%d suspend_data=%d\n",
			fifo_rt_length, sleep_fifo_length,
			chip->dt.s2_fifo_length, chip->suspend_data);

	return rc;
}

static int process_resume(struct qpnp_qg *chip)
{
	u8 status2 = 0, rt_status = 0;
	u32 ocv_uv = 0, ocv_raw = 0;
	int rc, batt_temp = 0;

	/* skip if profile is not loaded */
	if (!chip->profile_loaded)
		return 0;

	rc = qg_read(chip, chip->qg_base + QG_STATUS2_REG, &status2, 1);
	if (rc < 0) {
		pr_err("Failed to read status2 register, rc=%d\n", rc);
		return rc;
	}

	if (status2 & GOOD_OCV_BIT) {
		rc = qg_read_ocv(chip, &ocv_uv, &ocv_raw, S3_GOOD_OCV);
		if (rc < 0) {
			pr_err("Failed to read good_ocv, rc=%d\n", rc);
			return rc;
		}
		rc = qg_get_battery_temp(chip, &batt_temp);
		if (rc < 0) {
			pr_err("Failed to read BATT_TEMP, rc=%d\n", rc);
			return rc;
		}

		 /* Clear suspend data as there has been a GOOD OCV */
		memset(&chip->kdata, 0, sizeof(chip->kdata));
		chip->kdata.param[QG_GOOD_OCV_UV].data = ocv_uv;
		chip->kdata.param[QG_GOOD_OCV_UV].valid = true;
		chip->suspend_data = false;

		qg_dbg(chip, QG_DEBUG_PM, "GOOD OCV @ resume good_ocv=%d uV\n",
				ocv_uv);
	}

	rc = qg_read(chip, chip->qg_base + QG_INT_LATCHED_STS_REG,
						&rt_status, 1);
	if (rc < 0) {
		pr_err("Failed to read latched status register, rc=%d\n", rc);
		return rc;
	}
	rt_status &= FIFO_UPDATE_DONE_INT_LAT_STS_BIT;

	qg_dbg(chip, QG_DEBUG_PM, "FIFO_DONE_STS=%d suspend_data=%d good_ocv=%d\n",
				!!rt_status, chip->suspend_data,
				chip->kdata.param[QG_GOOD_OCV_UV].valid);
	/*
	 * If this is not a wakeup from FIFO-done,
	 * process the data immediately if - we have data from
	 * suspend or there is a good OCV.
	 */
	if (!rt_status && (chip->suspend_data ||
			chip->kdata.param[QG_GOOD_OCV_UV].valid)) {
		vote(chip->awake_votable, SUSPEND_DATA_VOTER, true, 0);
		/* signal the read thread */
		chip->data_ready = true;
		wake_up_interruptible(&chip->qg_wait_q);
		chip->suspend_data = false;
	}

	schedule_delayed_work(&chip->ttf->ttf_work, 0);

	return rc;
}

static int qpnp_qg_suspend_noirq(struct device *dev)
{
	int rc;
	struct qpnp_qg *chip = dev_get_drvdata(dev);

	mutex_lock(&chip->data_lock);

	rc = process_suspend(chip);
	if (rc < 0)
		pr_err("Failed to process QG suspend, rc=%d\n", rc);

	mutex_unlock(&chip->data_lock);

	return 0;
}

static int qpnp_qg_resume_noirq(struct device *dev)
{
	int rc;
	struct qpnp_qg *chip = dev_get_drvdata(dev);

	mutex_lock(&chip->data_lock);

	rc = process_resume(chip);
	if (rc < 0)
		pr_err("Failed to process QG resume, rc=%d\n", rc);

	mutex_unlock(&chip->data_lock);

	return 0;
}

static int qpnp_qg_suspend(struct device *dev)
{
	struct qpnp_qg *chip = dev_get_drvdata(dev);

	/* skip if profile is not loaded */
	if (!chip->profile_loaded)
		return 0;

	/* disable GOOD_OCV IRQ in sleep */
	vote(chip->good_ocv_irq_disable_votable,
			QG_INIT_STATE_IRQ_DISABLE, true, 0);

	return 0;
}

static int qpnp_qg_resume(struct device *dev)
{
	struct qpnp_qg *chip = dev_get_drvdata(dev);

	/* skip if profile is not loaded */
	if (!chip->profile_loaded)
		return 0;

	/* enable GOOD_OCV IRQ when active */
	vote(chip->good_ocv_irq_disable_votable,
			QG_INIT_STATE_IRQ_DISABLE, false, 0);

	return 0;
}

static const struct dev_pm_ops qpnp_qg_pm_ops = {
	.suspend_noirq	= qpnp_qg_suspend_noirq,
	.resume_noirq	= qpnp_qg_resume_noirq,
	.suspend	= qpnp_qg_suspend,
	.resume		= qpnp_qg_resume,
};

static int qpnp_qg_probe(struct platform_device *pdev)
{
	int rc = 0, soc = 0, nom_cap_uah;
	struct qpnp_qg *chip;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap) {
		pr_err("Parent regmap is unavailable\n");
		return -ENXIO;
	}

	/* ADC for BID & THERM */
	chip->batt_id_chan = iio_channel_get(&pdev->dev, "batt-id");
	if (IS_ERR(chip->batt_id_chan)) {
		rc = PTR_ERR(chip->batt_id_chan);
		if (rc != -EPROBE_DEFER) {
			pr_err("batt-id channel unavailable, rc=%d\n", rc);
			chip->batt_id_chan = NULL;
			return rc;
		}
	}

	chip->batt_therm_chan = iio_channel_get(&pdev->dev, "batt-therm");
	if (IS_ERR(chip->batt_therm_chan)) {
		rc = PTR_ERR(chip->batt_therm_chan);
		if (rc != -EPROBE_DEFER) {
			pr_err("batt-therm channel unavailable, rc=%d\n", rc);
			chip->batt_therm_chan = NULL;
			return rc;
		}
	}

	chip->dev = &pdev->dev;
	chip->debug_mask = &qg_debug_mask;
	platform_set_drvdata(pdev, chip);
	INIT_WORK(&chip->udata_work, process_udata_work);
	INIT_WORK(&chip->qg_status_change_work, qg_status_change_work);
	mutex_init(&chip->bus_lock);
	mutex_init(&chip->soc_lock);
	mutex_init(&chip->data_lock);
	init_waitqueue_head(&chip->qg_wait_q);
	chip->maint_soc = -EINVAL;
	chip->batt_soc = INT_MIN;
	chip->cc_soc = INT_MIN;
	chip->full_soc = QG_SOC_FULL;
	chip->chg_iterm_ma = INT_MIN;
	chip->soh = -EINVAL;
	chip->esr_actual = -EINVAL;
	chip->esr_nominal = -EINVAL;

	rc = qg_alg_init(chip);
	if (rc < 0) {
		pr_err("Error in alg_init, rc:%d\n", rc);
		return rc;
	}

	rc = qg_parse_dt(chip);
	if (rc < 0) {
		pr_err("Failed to parse DT, rc=%d\n", rc);
		return rc;
	}

	rc = qg_hw_init(chip);
	if (rc < 0) {
		pr_err("Failed to hw_init, rc=%d\n", rc);
		return rc;
	}

	rc = qg_setup_battery(chip);
	if (rc < 0) {
		pr_err("Failed to setup battery, rc=%d\n", rc);
		return rc;
	}

	rc = qg_register_device(chip);
	if (rc < 0) {
		pr_err("Failed to register QG char device, rc=%d\n", rc);
		return rc;
	}

	rc = qg_sdam_init(chip->dev);
	if (rc < 0) {
		pr_err("Failed to initialize QG SDAM, rc=%d\n", rc);
		return rc;
	}

	rc = qg_soc_init(chip);
	if (rc < 0) {
		pr_err("Failed to initialize SOC scaling init rc=%d\n", rc);
		return rc;
	}

	if (chip->profile_loaded) {
		if (!chip->dt.cl_disable) {
			/*
			 * Use FCC @ 25 C and charge-profile for
			 * Nominal Capacity
			 */
			rc = qg_get_nominal_capacity(&nom_cap_uah, 250, true);
			if (!rc) {
				rc = cap_learning_post_profile_init(chip->cl,
						nom_cap_uah);
				if (rc < 0) {
					pr_err("Error in cap_learning_post_profile_init rc=%d\n",
						rc);
					return rc;
				}
			}
		}
		rc = restore_cycle_count(chip->counter);
		if (rc < 0) {
			pr_err("Error in restoring cycle_count, rc=%d\n", rc);
			return rc;
		}
		schedule_delayed_work(&chip->ttf->ttf_work, 10000);
	}

	rc = qg_determine_pon_soc(chip);
	if (rc < 0) {
		pr_err("Failed to determine initial state, rc=%d\n", rc);
		goto fail_device;
	}

	chip->awake_votable = create_votable("QG_WS", VOTE_SET_ANY,
					 qg_awake_cb, chip);
	if (IS_ERR(chip->awake_votable)) {
		rc = PTR_ERR(chip->awake_votable);
		chip->awake_votable = NULL;
		goto fail_device;
	}

	chip->vbatt_irq_disable_votable = create_votable("QG_VBATT_IRQ_DISABLE",
				VOTE_SET_ANY, qg_vbatt_irq_disable_cb, chip);
	if (IS_ERR(chip->vbatt_irq_disable_votable)) {
		rc = PTR_ERR(chip->vbatt_irq_disable_votable);
		chip->vbatt_irq_disable_votable = NULL;
		goto fail_device;
	}

	chip->fifo_irq_disable_votable = create_votable("QG_FIFO_IRQ_DISABLE",
				VOTE_SET_ANY, qg_fifo_irq_disable_cb, chip);
	if (IS_ERR(chip->fifo_irq_disable_votable)) {
		rc = PTR_ERR(chip->fifo_irq_disable_votable);
		chip->fifo_irq_disable_votable = NULL;
		goto fail_device;
	}

	chip->good_ocv_irq_disable_votable =
			create_votable("QG_GOOD_IRQ_DISABLE",
			VOTE_SET_ANY, qg_good_ocv_irq_disable_cb, chip);
	if (IS_ERR(chip->good_ocv_irq_disable_votable)) {
		rc = PTR_ERR(chip->good_ocv_irq_disable_votable);
		chip->good_ocv_irq_disable_votable = NULL;
		goto fail_device;
	}

	rc = qg_init_psy(chip);
	if (rc < 0) {
		pr_err("Failed to initialize QG psy, rc=%d\n", rc);
		goto fail_votable;
	}

	rc = qg_request_irqs(chip);
	if (rc < 0) {
		pr_err("Failed to register QG interrupts, rc=%d\n", rc);
		goto fail_votable;
	}

	rc = qg_post_init(chip);
	if (rc < 0) {
		pr_err("Failed in qg_post_init rc=%d\n", rc);
		goto fail_votable;
	}

	qg_get_battery_capacity(chip, &soc);
	pr_info("QG initialized! battery_profile=%s SOC=%d\n",
				qg_get_battery_type(chip), soc);

	return rc;

fail_votable:
	destroy_votable(chip->awake_votable);
fail_device:
	device_destroy(chip->qg_class, chip->dev_no);
	cdev_del(&chip->qg_cdev);
	unregister_chrdev_region(chip->dev_no, 1);
	return rc;
}

static int qpnp_qg_remove(struct platform_device *pdev)
{
	struct qpnp_qg *chip = platform_get_drvdata(pdev);

	qg_batterydata_exit();
	qg_soc_exit(chip);

	cancel_work_sync(&chip->udata_work);
	cancel_work_sync(&chip->qg_status_change_work);
	device_destroy(chip->qg_class, chip->dev_no);
	cdev_del(&chip->qg_cdev);
	unregister_chrdev_region(chip->dev_no, 1);
	mutex_destroy(&chip->bus_lock);
	mutex_destroy(&chip->data_lock);
	mutex_destroy(&chip->soc_lock);
	if (chip->awake_votable)
		destroy_votable(chip->awake_votable);

	return 0;
}

static void qpnp_qg_shutdown(struct platform_device *pdev)
{
	struct qpnp_qg *chip = platform_get_drvdata(pdev);

	if (!is_usb_present(chip) || !chip->profile_loaded)
		return;
	/*
	 * Charging status doesn't matter when the device shuts down and we
	 * have to treat this as charge done. Hence pass charge_done as true.
	 */
	cycle_count_update(chip->counter,
			DIV_ROUND_CLOSEST(chip->msoc * 255, 100),
			POWER_SUPPLY_STATUS_NOT_CHARGING,
			true, chip->usb_present);
}

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,qpnp-qg", },
	{ },
};

static struct platform_driver qpnp_qg_driver = {
	.driver		= {
		.name		= "qcom,qpnp-qg",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
		.pm		= &qpnp_qg_pm_ops,
	},
	.probe		= qpnp_qg_probe,
	.remove		= qpnp_qg_remove,
	.shutdown	= qpnp_qg_shutdown,
};
module_platform_driver(qpnp_qg_driver);

MODULE_DESCRIPTION("QPNP QG Driver");
MODULE_LICENSE("GPL v2");
