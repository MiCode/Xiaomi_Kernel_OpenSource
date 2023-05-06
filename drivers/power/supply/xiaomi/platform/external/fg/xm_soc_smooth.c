
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
#ifndef abs
#define abs(x) ((x) >0? (x) : -(x))
#endif

/* get MonotonicSoc*/
#define HW_REPORT_FULL_SOC 9700
#define SOC_PROPORTION 98
#define SOC_PROPORTION_C 97

int bq_battery_soc_smooth_tracking(struct bq_fg_chip *bq,
		int raw_soc, int soc, int temp, int batt_ma)
{
	int status;
	static int system_soc, last_system_soc;
	int change_delta = 0;
	int soc_delta = 0, delta_time = 0, unit_time = 10000;
	static ktime_t last_change_time = -1;
	int soc_changed = 0;

	ktime_t tmp_time = 0;
	struct timespec64 time;
	static int ibat_pos_count = 0;

	tmp_time = ktime_get_boottime();
	time = ktime_to_timespec64(tmp_time);

	if((batt_ma > 0) && (ibat_pos_count < 10))
		ibat_pos_count++;
	else if(batt_ma <= 0)
		ibat_pos_count = 0;

	status = fg_get_batt_status(bq);

	// Map soc value according to raw_soc
	if(raw_soc > HW_REPORT_FULL_SOC)
		system_soc = 100;
	else {
		system_soc = ((raw_soc + SOC_PROPORTION_C) / SOC_PROPORTION);
		if(system_soc > 99)
			system_soc = 99;
	}

	// Get the initial value for the first time
	if(last_change_time == -1) {
		last_change_time = ktime_get();
		if(system_soc != 0)
			last_system_soc = system_soc;
		else
			last_system_soc = soc;
	}

	// If the soc jump, will smooth one cap every 10S
	soc_delta = abs(system_soc - last_system_soc);
	if(soc_delta > 1 || (bq->batt_volt < 3300 && system_soc > 0)) {
		calc_delta_time(last_change_time,&change_delta);
		delta_time = change_delta / unit_time;
		if(delta_time < 0) {
			last_change_time = ktime_get();
			delta_time = 0;
		}

		smooth_err("%s: system soc=%d,last system soc: %d,delta time: %d\n", __func__,
					system_soc,last_system_soc,delta_time);

		soc_changed = min(1,delta_time);
		if(soc_changed) {
			if ((status == POWER_SUPPLY_STATUS_CHARGING) && (system_soc > last_system_soc))
				system_soc = last_system_soc + soc_changed;
			else if (status == POWER_SUPPLY_STATUS_DISCHARGING && system_soc < last_system_soc)
				system_soc = last_system_soc - soc_changed;
		} else
			system_soc = last_system_soc;

		smooth_err("%s: fg jump smooth soc_changed=%d\n", __func__, soc_changed);
	}

	if(system_soc < last_system_soc)
		system_soc = last_system_soc - 1;

	// Avoid mismatches between charging status and soc changes
	if (((status == POWER_SUPPLY_STATUS_DISCHARGING) && (system_soc > last_system_soc))
	|| ((status == POWER_SUPPLY_STATUS_CHARGING) && (system_soc < last_system_soc) && (ibat_pos_count < 3) && ((time.tv_sec > 10))))
		system_soc = last_system_soc;

	smooth_err("%s: smooth_new:sys_soc:%d last_sys_soc:%d soc_delta:%d charging_status:%d unit_time:%d batt_ma_now=%d\n",
		__func__, system_soc, last_system_soc, soc_delta, status, unit_time, batt_ma);

	if (system_soc != last_system_soc) {
		last_change_time = ktime_get();
		last_system_soc = system_soc;
	}

	if(system_soc > 100)
		system_soc = 100;
	if(system_soc < 0)
		system_soc = 0;

	if ((system_soc == 0) && ((bq->batt_volt >= 3400) || ((time.tv_sec <= 10)))) {
		system_soc = 1;
		smooth_err("%s: uisoc::hold 1 when volt > 3400mv. \n", __func__);
	}

	if(bq->last_soc != system_soc)
		bq->last_soc = system_soc;

	return system_soc;
}

