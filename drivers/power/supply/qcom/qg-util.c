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

#include <linux/alarmtimer.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <uapi/linux/qg.h>
#include "qg-sdam.h"
#include "qg-core.h"
#include "qg-reg.h"
#include "qg-defs.h"
#include "qg-util.h"

static inline bool is_sticky_register(u32 addr)
{
	if ((addr & 0xFF) == QG_STATUS2_REG)
		return true;

	return false;
}

int qg_read(struct qpnp_qg *chip, u32 addr, u8 *val, int len)
{
	int rc, i;
	u32 dummy = 0;

	rc = regmap_bulk_read(chip->regmap, addr, val, len);
	if (rc < 0) {
		pr_err("Failed regmap_read for address %04x rc=%d\n", addr, rc);
		return rc;
	}

	if (is_sticky_register(addr)) {
		/* write to the sticky register to clear it */
		rc = regmap_write(chip->regmap, addr, dummy);
		if (rc < 0) {
			pr_err("Failed regmap_write for %04x rc=%d\n",
						addr, rc);
			return rc;
		}
	}

	if (*chip->debug_mask & QG_DEBUG_BUS_READ) {
		pr_info("length %d addr=%04x\n", len, addr);
		for (i = 0; i < len; i++)
			pr_info("val[%d]: %02x\n", i, val[i]);
	}

	return 0;
}

int qg_write(struct qpnp_qg *chip, u32 addr, u8 *val, int len)
{
	int rc, i;

	mutex_lock(&chip->bus_lock);

	if (len > 1)
		rc = regmap_bulk_write(chip->regmap, addr, val, len);
	else
		rc = regmap_write(chip->regmap, addr, *val);

	if (rc < 0) {
		pr_err("Failed regmap_write for address %04x rc=%d\n",
				addr, rc);
		goto out;
	}

	if (*chip->debug_mask & QG_DEBUG_BUS_WRITE) {
		pr_info("length %d addr=%04x\n", len, addr);
		for (i = 0; i < len; i++)
			pr_info("val[%d]: %02x\n", i, val[i]);
	}
out:
	mutex_unlock(&chip->bus_lock);
	return rc;
}

int qg_masked_write(struct qpnp_qg *chip, int addr, u32 mask, u32 val)
{
	int rc;

	mutex_lock(&chip->bus_lock);

	rc = regmap_update_bits(chip->regmap, addr, mask, val);
	if (rc < 0) {
		pr_err("Failed regmap_update_bits for address %04x rc=%d\n",
				addr, rc);
		goto out;
	}

	if (*chip->debug_mask & QG_DEBUG_BUS_WRITE)
		pr_info("addr=%04x mask: %02x val: %02x\n", addr, mask, val);

out:
	mutex_unlock(&chip->bus_lock);
	return rc;
}

int qg_read_raw_data(struct qpnp_qg *chip, int addr, u32 *data)
{
	int rc;
	u8 reg[2] = {0};

	rc = qg_read(chip, chip->qg_base + addr, &reg[0], 2);
	if (rc < 0) {
		pr_err("Failed to read QG addr %d rc=%d\n", addr, rc);
		return rc;
	}

	*data = reg[0] | (reg[1] << 8);

	return rc;
}

int get_fifo_length(struct qpnp_qg *chip, u32 *fifo_length, bool rt)
{
	int rc;
	u8 reg = 0;
	u32 addr;

	addr = rt ? QG_STATUS3_REG : QG_S2_NORMAL_MEAS_CTL2_REG;
	rc = qg_read(chip, chip->qg_base + addr, &reg, 1);
	if (rc < 0) {
		pr_err("Failed to read FIFO length rc=%d\n", rc);
		return rc;
	}

	if (rt) {
		*fifo_length = reg & COUNT_FIFO_RT_MASK;
	} else {
		*fifo_length = (reg & FIFO_LENGTH_MASK) >> FIFO_LENGTH_SHIFT;
		*fifo_length += 1;
	}

	return rc;
}

int get_sample_count(struct qpnp_qg *chip, u32 *sample_count)
{
	int rc;
	u8 reg = 0;

	rc = qg_read(chip, chip->qg_base + QG_S2_NORMAL_MEAS_CTL2_REG,
					&reg, 1);
	if (rc < 0) {
		pr_err("Failed to read FIFO sample count rc=%d\n", rc);
		return rc;
	}

	*sample_count = 1 << ((reg & NUM_OF_ACCUM_MASK) + 1);

	return rc;
}

int get_sample_interval(struct qpnp_qg *chip, u32 *sample_interval)
{
	int rc;
	u8 reg = 0;

	rc = qg_read(chip, chip->qg_base + QG_S2_NORMAL_MEAS_CTL3_REG,
					&reg, 1);
	if (rc < 0) {
		pr_err("Failed to read FIFO sample interval rc=%d\n", rc);
		return rc;
	}

	*sample_interval = reg * 10;

	return rc;
}

int get_rtc_time(unsigned long *rtc_time)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int rc;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("Failed to open rtc device (%s)\n",
				CONFIG_RTC_HCTOSYS_DEVICE);
		return -EINVAL;
	}

	rc = rtc_read_time(rtc, &tm);
	if (rc) {
		pr_err("Failed to read rtc time (%s) : %d\n",
				CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
				CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}
	rtc_tm_to_time(&tm, rtc_time);

close_time:
	rtc_class_close(rtc);
	return rc;
}

int get_fifo_done_time(struct qpnp_qg *chip, bool rt, int *time_ms)
{
	int rc, length = 0;
	u32 sample_count = 0, sample_interval = 0, acc_count = 0;

	rc = get_fifo_length(chip, &length, rt ? true : false);
	if (rc < 0)
		return rc;

	rc = get_sample_count(chip, &sample_count);
	if (rc < 0)
		return rc;

	rc = get_sample_interval(chip, &sample_interval);
	if (rc < 0)
		return rc;

	*time_ms = length * sample_count * sample_interval;

	if (rt) {
		rc = qg_read(chip, chip->qg_base + QG_ACCUM_CNT_RT_REG,
					(u8 *)&acc_count, 1);
		if (rc < 0)
			return rc;

		*time_ms += ((sample_count - acc_count) * sample_interval);
	}

	return 0;
}

static bool is_usb_available(struct qpnp_qg *chip)
{
	if (chip->usb_psy)
		return true;

	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy)
		return false;

	return true;
}

bool is_usb_present(struct qpnp_qg *chip)
{
	union power_supply_propval pval = {0, };

	if (is_usb_available(chip))
		power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);

	return pval.intval ? true : false;
}

static bool is_parallel_available(struct qpnp_qg *chip)
{
	if (chip->parallel_psy)
		return true;

	chip->parallel_psy = power_supply_get_by_name("parallel");
	if (!chip->parallel_psy)
		return false;

	return true;
}

bool is_parallel_enabled(struct qpnp_qg *chip)
{
	union power_supply_propval pval = {0, };

	if (is_parallel_available(chip)) {
		power_supply_get_property(chip->parallel_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &pval);
	}

	return pval.intval ? true : false;
}

int qg_write_monotonic_soc(struct qpnp_qg *chip, int msoc)
{
	u8 reg = 0;
	int rc;

	reg = (msoc * 255) / 100;
	rc = qg_write(chip, chip->qg_base + QG_SOC_MONOTONIC_REG,
				&reg, 1);
	if (rc < 0)
		pr_err("Failed to update QG_SOC_MONOTINIC reg rc=%d\n", rc);

	return rc;
}

int qg_get_battery_temp(struct qpnp_qg *chip, int *temp)
{
	int rc = 0;
	struct qpnp_vadc_result result;

	if (chip->battery_missing) {
		*temp = 250;
		return 0;
	}

	rc = qpnp_vadc_read(chip->vadc_dev, VADC_BAT_THERM_PU2, &result);
	if (rc) {
		pr_err("Failed reading adc channel=%d, rc=%d\n",
					VADC_BAT_THERM_PU2, rc);
		return rc;
	}
	pr_debug("batt_temp = %lld meas = 0x%llx\n",
			result.physical, result.measurement);

	*temp = (int)result.physical;

	return rc;
}

int qg_get_battery_current(struct qpnp_qg *chip, int *ibat_ua)
{
	int rc = 0, last_ibat = 0;

	if (chip->battery_missing) {
		*ibat_ua = 0;
		return 0;
	}

	if (chip->dt.qg_vbms_mode) {
		*ibat_ua = chip->vbms_ibat_ua;
		return 0;
	}

	/* hold data */
	rc = qg_masked_write(chip, chip->qg_base + QG_DATA_CTL2_REG,
				BURST_AVG_HOLD_FOR_READ_BIT,
				BURST_AVG_HOLD_FOR_READ_BIT);
	if (rc < 0) {
		pr_err("Failed to hold burst-avg data rc=%d\n", rc);
		goto release;
	}

	rc = qg_read(chip, chip->qg_base + QG_LAST_BURST_AVG_I_DATA0_REG,
				(u8 *)&last_ibat, 2);
	if (rc < 0) {
		pr_err("Failed to read LAST_BURST_AVG_I reg, rc=%d\n", rc);
		goto release;
	}

	last_ibat = sign_extend32(last_ibat, 15);
	*ibat_ua = I_RAW_TO_UA(last_ibat);

release:
	/* release */
	qg_masked_write(chip, chip->qg_base + QG_DATA_CTL2_REG,
				BURST_AVG_HOLD_FOR_READ_BIT, 0);
	return rc;
}

int qg_get_battery_voltage(struct qpnp_qg *chip, int *vbat_uv)
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
