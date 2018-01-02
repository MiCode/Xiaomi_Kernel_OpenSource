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
#include <linux/power_supply.h>
#include <uapi/linux/qg.h>
#include "qg-sdam.h"
#include "qg-core.h"
#include "qg-reg.h"
#include "qg-util.h"
#include "qg-defs.h"

#define DEFAULT_UPDATE_TIME_MS			64000
#define SOC_SCALE_HYST_MS			2000
static void get_next_update_time(struct qpnp_qg *chip, int *time_ms)
{
	int rc = 0, full_time_ms = 0, rt_time_ms = 0;

	*time_ms = DEFAULT_UPDATE_TIME_MS;

	rc = get_fifo_done_time(chip, false, &full_time_ms);
	if (rc < 0)
		return;

	rc = get_fifo_done_time(chip, true, &rt_time_ms);
	if (rc < 0)
		return;

	*time_ms = full_time_ms - rt_time_ms;

	if (*time_ms < 0)
		*time_ms = 0;

	qg_dbg(chip, QG_DEBUG_SOC, "SOC scale next-update-time %d secs\n",
					*time_ms / 1000);
}

static bool is_scaling_required(struct qpnp_qg *chip)
{
	if ((abs(chip->catch_up_soc - chip->msoc) < chip->dt.delta_soc) &&
		chip->catch_up_soc != 0 && chip->catch_up_soc != 100)
		return false;

	if (chip->catch_up_soc == chip->msoc)
		/* SOC has not changed */
		return false;


	if (chip->catch_up_soc > chip->msoc && !is_usb_present(chip))
		/* USB is not present and SOC has increased */
		return false;

	return true;
}

static void update_msoc(struct qpnp_qg *chip)
{
	int rc;

	if (chip->catch_up_soc > chip->msoc) {
		/* SOC increased */
		if (is_usb_present(chip)) /* Increment if USB is present */
			chip->msoc += chip->dt.delta_soc;
	} else {
		/* SOC dropped */
		chip->msoc -= chip->dt.delta_soc;
	}
	chip->msoc = CAP(0, 100, chip->msoc);

	/* update the SOC register */
	rc = qg_write_monotonic_soc(chip, chip->msoc);
	if (rc < 0)
		pr_err("Failed to update MSOC register rc=%d\n", rc);

	/* update SDAM with the new MSOC */
	rc = qg_sdam_write(SDAM_SOC, chip->msoc);
	if (rc < 0)
		pr_err("Failed to update SDAM with MSOC rc=%d\n", rc);

	qg_dbg(chip, QG_DEBUG_SOC,
		"SOC scale: Update msoc=%d catch_up_soc=%d delta_soc=%d\n",
		chip->msoc, chip->catch_up_soc, chip->dt.delta_soc);
}

static void scale_soc_stop(struct qpnp_qg *chip)
{
	chip->next_wakeup_ms = 0;
	alarm_cancel(&chip->alarm_timer);

	qg_dbg(chip, QG_DEBUG_SOC,
			"SOC scale stopped: msoc=%d catch_up_soc=%d\n",
			chip->msoc, chip->catch_up_soc);
}

static void scale_soc_work(struct work_struct *work)
{
	struct qpnp_qg *chip = container_of(work,
			struct qpnp_qg, scale_soc_work);

	mutex_lock(&chip->soc_lock);

	if (!is_scaling_required(chip)) {
		scale_soc_stop(chip);
		goto done;
	}

	update_msoc(chip);

	if (is_scaling_required(chip)) {
		alarm_start_relative(&chip->alarm_timer,
				ms_to_ktime(chip->next_wakeup_ms));
	} else {
		scale_soc_stop(chip);
		goto done_psy;
	}

	qg_dbg(chip, QG_DEBUG_SOC,
		"SOC scale: Work msoc=%d catch_up_soc=%d delta_soc=%d next_wakeup=%d sec\n",
			chip->msoc, chip->catch_up_soc, chip->dt.delta_soc,
			chip->next_wakeup_ms / 1000);

done_psy:
	power_supply_changed(chip->qg_psy);
done:
	pm_relax(chip->dev);
	mutex_unlock(&chip->soc_lock);
}

static enum alarmtimer_restart
	qpnp_msoc_timer(struct alarm *alarm, ktime_t now)
{
	struct qpnp_qg *chip = container_of(alarm,
				struct qpnp_qg, alarm_timer);

	/* timer callback runs in atomic context, cannot use voter */
	pm_stay_awake(chip->dev);
	schedule_work(&chip->scale_soc_work);

	return ALARMTIMER_NORESTART;
}

int qg_scale_soc(struct qpnp_qg *chip, bool force_soc)
{
	int soc_points = 0;
	int rc = 0, time_ms = 0;

	mutex_lock(&chip->soc_lock);

	qg_dbg(chip, QG_DEBUG_SOC,
			"SOC scale: Start msoc=%d catch_up_soc=%d delta_soc=%d\n",
			chip->msoc, chip->catch_up_soc, chip->dt.delta_soc);

	if (force_soc) {
		chip->msoc = chip->catch_up_soc;
		rc = qg_write_monotonic_soc(chip, chip->msoc);
		if (rc < 0)
			pr_err("Failed to update MSOC register rc=%d\n", rc);

		qg_dbg(chip, QG_DEBUG_SOC,
			"SOC scale: Forced msoc=%d\n", chip->msoc);
		goto done_psy;
	}

	if (!is_scaling_required(chip)) {
		scale_soc_stop(chip);
		goto done;
	}

	update_msoc(chip);

	if (is_scaling_required(chip)) {
		get_next_update_time(chip, &time_ms);
		soc_points = abs(chip->msoc - chip->catch_up_soc)
					/ chip->dt.delta_soc;
		chip->next_wakeup_ms = (time_ms / (soc_points + 1))
					- SOC_SCALE_HYST_MS;
		if (chip->next_wakeup_ms < 0)
			chip->next_wakeup_ms = 1; /* wake up immediately */
		alarm_start_relative(&chip->alarm_timer,
					ms_to_ktime(chip->next_wakeup_ms));
	} else {
		scale_soc_stop(chip);
		goto done_psy;
	}

	qg_dbg(chip, QG_DEBUG_SOC,
		"SOC scale: msoc=%d catch_up_soc=%d delta_soc=%d soc_points=%d next_wakeup=%d sec\n",
			chip->msoc, chip->catch_up_soc,	chip->dt.delta_soc,
			soc_points, chip->next_wakeup_ms / 1000);

done_psy:
	power_supply_changed(chip->qg_psy);
done:
	mutex_unlock(&chip->soc_lock);
	return rc;
}

int qg_soc_init(struct qpnp_qg *chip)
{
	if (alarmtimer_get_rtcdev()) {
		alarm_init(&chip->alarm_timer, ALARM_BOOTTIME,
			qpnp_msoc_timer);
	} else {
		pr_err("Failed to get soc alarm-timer\n");
		return -EINVAL;
	}
	INIT_WORK(&chip->scale_soc_work, scale_soc_work);

	return 0;
}

void qg_soc_exit(struct qpnp_qg *chip)
{
	alarm_cancel(&chip->alarm_timer);
}
