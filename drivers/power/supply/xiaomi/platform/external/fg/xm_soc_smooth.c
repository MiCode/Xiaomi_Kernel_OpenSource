
#include "inc/bq27z561.h"
#include "inc/bq27z561_iio.h"
#include "inc/xm_battery_auth.h"
#include "inc/xm_soc_smooth.h"

static int log_level = 2;
#define smooth_err(fmt, ...)							\
do {										\
	if (log_level >= 0)							\
		printk(KERN_ERR "[xm_soc_smooth] " fmt, ##__VA_ARGS__);	\
} while (0)

#define smooth_info(fmt, ...)							\
do {										\
	if (log_level >= 1)							\
		printk(KERN_ERR "[xm_soc_smooth] " fmt, ##__VA_ARGS__);	\
} while (0)

#define smooth_dbg(fmt, ...)							\
do {										\
	if (log_level >= 2)							\
		printk(KERN_ERR "[xm_soc_smooth] " fmt, ##__VA_ARGS__);	\
} while (0)

int bq_battery_soc_smooth_tracking(struct bq_fg_chip *bq,
		int raw_soc, int batt_soc, int batt_temp, int batt_ma)
{
	static int last_batt_soc = -1, system_soc, cold_smooth;
	static int last_status;
	int change_delta = 0, rc;
	int status;
//	int optimiz_delta = 0;
	static ktime_t last_change_time;
	static ktime_t last_optimiz_time;
	int unit_time = 0;
	int soc_changed = 0, delta_time = 0;
	static int optimiz_soc, last_raw_soc;
	union power_supply_propval pval = {0, };
	int batt_ma_avg, i;
	static int old_batt_ma = 0;

	if (bq->optimiz_soc > 0) {
		bq->ffc_smooth = true;
		last_batt_soc = bq->optimiz_soc;
		system_soc = bq->optimiz_soc;
		last_change_time = ktime_get();
		bq->optimiz_soc = 0;
	}

	if (!bq->usb_psy || !bq->batt_psy) {
		bq->usb_psy = power_supply_get_by_name("usb");
		if (!bq->usb_psy) {
			return batt_soc;
		}
		bq->batt_psy = power_supply_get_by_name("battery");
		if (!bq->batt_psy) {
			return batt_soc;
		}
	}

	if (last_batt_soc < 0)
		last_batt_soc = batt_soc;

	if (raw_soc == FG_RAW_SOC_FULL)
		bq->ffc_smooth = false;

	if (bq->ffc_smooth) {
		rc = power_supply_get_property(bq->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &pval);
		if (rc < 0) {
			smooth_err("%s: failed get batt staus\n", __func__);
			return -EINVAL;
		}
		status = pval.intval;
		if (batt_soc == system_soc) {
			bq->ffc_smooth = false;
			return batt_soc;
		}
		if (status != last_status) {
			if (last_status == POWER_SUPPLY_STATUS_CHARGING
					&& status == POWER_SUPPLY_STATUS_DISCHARGING)
				last_change_time = ktime_get();
			last_status = status;
		}
	}
#if 0
	if (bq->fast_mode && raw_soc >= FG_REPORT_FULL_SOC && raw_soc != FG_RAW_SOC_FULL) {
		if (last_optimiz_time == 0)
			last_optimiz_time = ktime_get();
		calc_delta_time(last_optimiz_time, &optimiz_delta);
		delta_time = optimiz_delta / FG_OPTIMIZ_FULL_TIME;
		soc_changed = min(1, delta_time);
		if (raw_soc > last_raw_soc && soc_changed) {
			last_raw_soc = raw_soc;
			optimiz_soc += soc_changed;
			last_optimiz_time = ktime_get();
			bq_dbg(PR_DEBUG, "optimiz_soc:%d, last_optimiz_time%ld\n",
					optimiz_soc, last_optimiz_time);
			if (optimiz_soc > 100)
				optimiz_soc = 100;
			bq->ffc_smooth = true;
		}
		if (batt_soc > optimiz_soc) {
			optimiz_soc = batt_soc;
			last_optimiz_time = ktime_get();
		}
		if (bq->ffc_smooth)
			batt_soc = optimiz_soc;
		last_change_time = ktime_get();
	} else {
		optimiz_soc = batt_soc + 1;
		last_raw_soc = raw_soc;
		last_optimiz_time = ktime_get();
	}
#else
	optimiz_soc = batt_soc + 1;
	last_raw_soc = raw_soc;
	last_optimiz_time = ktime_get();
#endif
	calc_delta_time(last_change_time, &change_delta);
	fg_read_avg_current(bq, &batt_ma_avg);
	if (batt_temp > 150 && !cold_smooth && batt_soc != 0) {
		if (bq->ffc_smooth && (status == POWER_SUPPLY_STATUS_DISCHARGING ||
					status == POWER_SUPPLY_STATUS_NOT_CHARGING ||
					batt_ma_avg > 50)) {
			for (i = 1; i < FFC_SMOOTH_LEN; i++) {
				if (batt_ma_avg < ffc_dischg_smooth[i].curr_lim) {
					unit_time = ffc_dischg_smooth[i-1].time;
					break;
				}
			}
			if (i == FFC_SMOOTH_LEN) {
				unit_time = ffc_dischg_smooth[FFC_SMOOTH_LEN-1].time;
			}
		}
	} else {
		/* Calculated average current > 1000mA */
		if (batt_ma_avg > BATT_HIGH_AVG_CURRENT)
			/* Heavy loading current, ignore battery soc limit*/
			unit_time = LOW_TEMP_CHARGING_DELTA;
		else
			unit_time = LOW_TEMP_DISCHARGING_DELTA;
		if (batt_soc != last_batt_soc)
			cold_smooth = true;
		else
			cold_smooth = false;
	}
	if (unit_time > 0) {
		delta_time = change_delta / unit_time;
		soc_changed = min(1, delta_time);
	} else {
		if (!bq->ffc_smooth)
			bq->update_now = true;
	}

	smooth_err("%s: batt_ma_avg:%d, batt_ma:%d, cold_smooth:%d, optimiz_soc:%d", __func__,
			batt_ma_avg, batt_ma, cold_smooth, optimiz_soc);
	smooth_err("%s: delta_time:%d, change_delta:%d, unit_time:%d"
			" soc_changed:%d, bq->update_now:%d, bq->ffc_smooth %d", __func__,
			delta_time, change_delta, unit_time,
			soc_changed, bq->update_now, bq->ffc_smooth);

	if(batt_ma > 0 && old_batt_ma <= 0)
	{
		change_delta = 0;
		last_change_time = ktime_get();
		smooth_err("%s: old_batt_ma:%d", __func__, old_batt_ma);
	}
	old_batt_ma = batt_ma;

	if (last_batt_soc < batt_soc && batt_ma < 0) {
		/* Battery in charging status
		 * update the soc when resuming device
		 */
		if(batt_soc - last_batt_soc >= 1) {
			if(bq->fast_mode){
				if(change_delta > 5000) {
					last_batt_soc++;
					last_change_time = ktime_get();
					bq->update_now = false;
				}
			} else {
				if(change_delta > 30000) {
					last_batt_soc++;
					last_change_time = ktime_get();
					bq->update_now = false;
				}
			}
			smooth_err("%s: raw_soc:%d batt_soc:%d,last_batt_soc:%d,change_delta:%d bq->resume_update:%d", __func__,
				raw_soc, batt_soc, last_batt_soc, change_delta, bq->resume_update);
		}
		else
		last_batt_soc = bq->update_now ?
			batt_soc : last_batt_soc + soc_changed;
	} else if (last_batt_soc > batt_soc && batt_ma > 0) {
		/* Battery in discharging status
		 * update the soc when resuming device
		 */
		if(bq->resume_update && batt_soc >= 80 && (last_batt_soc - batt_soc <= 3)){
			bq->resume_update = false;
		}
		if(last_batt_soc - batt_soc >= 1 && !bq->resume_update) {
			if(batt_soc == 100 && change_delta > 60000){
				last_batt_soc--;
				last_change_time = ktime_get();
				bq->update_now = false;
			} else if(batt_soc >= 30 && batt_soc < 100 && change_delta > 20000) {
				last_batt_soc--;
				last_change_time = ktime_get();
				bq->update_now = false;
			} else if(batt_soc < 30 && change_delta > 20000) {
				last_batt_soc--;
				last_change_time = ktime_get();
				bq->update_now = false;
			}
			smooth_err("%s: raw_soc:%d batt_soc:%d,last_batt_soc:%d,change_delta:%d bq->resume_update:%d", __func__,
				raw_soc, batt_soc, last_batt_soc, change_delta, bq->resume_update);
		} else {
			last_batt_soc = bq->update_now ?
				batt_soc : last_batt_soc - soc_changed;
				bq->resume_update = false;
		}
	}
	if(batt_ma == 0 && bq->batt_volt > 4350){
		if(last_batt_soc < 100 && change_delta > 60000)
			last_batt_soc++;
	}

	if(bq->batt_curr < 0 && bq->batt_curr > -1200 && bq->batt_volt > 4460 && bq->fast_mode){
		if(last_batt_soc < 100 && change_delta > 30000) {
			last_change_time = ktime_get();
			last_batt_soc++;
			smooth_err("%s: last_batt_soc:%d\n", __func__, last_batt_soc);
		}
	}

	bq->update_now = false;

	if (system_soc != last_batt_soc) {
		system_soc = last_batt_soc;
		last_change_time = ktime_get();
	}

	smooth_err("%s: raw_soc:%d batt_soc:%d,last_batt_soc:%d,system_soc:%d"
			" bq->fast_mode:%d", __func__,
			raw_soc, batt_soc, last_batt_soc, system_soc,
			bq->fast_mode);

	return system_soc;
}

