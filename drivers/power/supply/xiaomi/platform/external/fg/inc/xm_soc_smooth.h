
#ifndef __XM_SOC_SMOOTH_H
#define __XM_SOC_SMOOTH_H

#define BATT_HIGH_AVG_CURRENT		1000000
#define NORMAL_TEMP_CHARGING_DELTA	10000
#define NORMAL_DISTEMP_CHARGING_DELTA	60000
#define LOW_TEMP_CHARGING_DELTA		20000
#define LOW_TEMP_DISCHARGING_DELTA	40000
#define FFC_SMOOTH_LEN			4
#define FG_RAW_SOC_FULL			10000
#define FG_REPORT_FULL_SOC		9400
#define FG_OPTIMIZ_FULL_TIME		64000

struct ffc_smooth {
	int curr_lim;
	int time;
};

static struct ffc_smooth ffc_dischg_smooth[FFC_SMOOTH_LEN] = {
	{0,    300000},
	{300,  150000},
	{600,   72000},
	{1000,  50000},
};

int bq_battery_soc_smooth_tracking(struct bq_fg_chip *bq,
		int raw_soc, int batt_soc, int batt_temp, int batt_ma);

#endif /* __XM_SOC_SMOOTH_H */

