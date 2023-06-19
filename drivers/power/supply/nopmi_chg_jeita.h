#if !defined(__NOPMI_CHG_JEITA_H__)
#define __NOPMI_CHG_JEITA_H__

#include "nopmi_chg_common.h"

/* sw jeita */
#define JEITA_TEMP_ABOVE_T4_CV	4240
#define JEITA_TEMP_T3_TO_T4_CV	4240
#define JEITA_TEMP_T2_TO_T3_CV	4340
#define JEITA_TEMP_T1P5_TO_T2_CV	4240
#define JEITA_TEMP_T1_TO_T1P5_CV	4040
#define JEITA_TEMP_T0_TO_T1_CV	4040
#define JEITA_TEMP_TN1_TO_T0_CV	4040
#define JEITA_TEMP_BELOW_T0_CV	4040
#define JEITA_TEMP_NORMAL_VOLTAGE	4450
#define TEMP_T4_THRES  50
#define TEMP_T4_THRES_MINUS_X_DEGREE 47
#define TEMP_T3_THRES  45
#define TEMP_T3_THRES_MINUS_X_DEGREE 39
#define TEMP_T2_THRES  10
#define TEMP_T2_THRES_PLUS_X_DEGREE 16
#define TEMP_T1P5_THRES  5
#define TEMP_T1P5_THRES_PLUS_X_DEGREE 10
#define TEMP_T1_THRES  0
#define TEMP_T1_THRES_PLUS_X_DEGREE 6
#define TEMP_T0_THRES  0
#define TEMP_T0_THRES_PLUS_X_DEGREE  0
#define TEMP_TN1_THRES  -10
#define TEMP_TN1_THRES_PLUS_X_DEGREE  -10
#define TEMP_NEG_10_THRES 0
#define TEMP_TN1_TO_T0_FCC  442
#define TEMP_T0_TO_T1_FCC   884
#define TEMP_T1_TO_T1P5_FCC 2210
#define TEMP_T1P5_TO_T2_FCC 3536
#define TEMP_T2_TO_T3_FCC   4000
#define TEMP_T3_TO_T4_FCC   2210

#define JEITA_WORK_DELAY_MS		2000

#define JEITA_VOTER		"JEITA_VOTER"

struct sw_jeita_data {
	int sm;
	int pre_sm;
	int cv;
	int term_curr;
	int pre_cv;
	bool charging;
	bool can_recharging;
	bool error_recovery_flag;
};
/*
 * Software JEITA
 * T0: -10 degree Celsius
 * T1: 0 degree Celsius
 * T2: 10 degree Celsius
 * T3: 45 degree Celsius
 * T4: 50 degree Celsius
 */
enum sw_jeita_state_enum {
	TEMP_BELOW_T0 = 0,
	TEMP_TN1_TO_T0,
	TEMP_T0_TO_T1,
	TEMP_T1_TO_T1P5,
	TEMP_T1P5_TO_T2,
	TEMP_T2_TO_T3,
	TEMP_T3_TO_T4,
	TEMP_ABOVE_T4
};

struct nopmi_chg_jeita_config{
	/* sw jeita */
	bool enable_sw_jeita;
	int jeita_temp_above_t4_cv;
	int jeita_temp_t3_to_t4_cv;
	int jeita_temp_t2_to_t3_cv;
	int jeita_temp_t1p5_to_t2_cv;
	int jeita_temp_t1_to_t1p5_cv;
	int jeita_temp_t0_to_t1_cv;
	int jeita_temp_tn1_to_t0_cv;
	int jeita_temp_below_t0_cv;
	int normal_charge_voltage;
	int temp_t4_thres;
	int temp_t4_thres_minus_x_degree;
	int temp_t3_thres;
	int temp_t3_thres_minus_x_degree;
	int temp_t2_thres;
	int temp_t2_thres_plus_x_degree;
	int temp_t1p5_thres;
	int temp_t1p5_thres_plus_x_degree;
	int temp_t1_thres;
	int temp_t1_thres_plus_x_degree;
	int temp_t0_thres;
	int temp_t0_thres_plus_x_degree;
	int temp_tn1_thres;
	int temp_tn1_thres_plus_x_degree;
	int temp_neg_10_thres;
	int temp_t3_to_t4_fcc;
	int temp_t2_to_t3_fcc;
	int temp_t1p5_to_t2_fcc;
	int temp_t1_to_t1p5_fcc;
	int temp_t0_to_t1_fcc;
	int temp_tn1_to_t0_fcc;
};

struct nopmi_chg_jeita_st {
	bool	sw_jeita_start;
	bool	usb_present;
	int		battery_temp;
	int     battery_id;
	struct sw_jeita_data *sw_jeita;
	struct nopmi_chg_jeita_config dt;
	struct delayed_work	jeita_work;
	struct power_supply *bms_psy;
	struct power_supply *bbc_psy;
	struct power_supply *usb_psy;

	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
	struct votable		*chgctrl_votable;
};

enum charge_mode {
	CHG_MODE_BUCK_OFF = 0,  /* buck, chg off */
	CHG_MODE_CHARGING_OFF, /*chg off */
	CHG_MODE_CHARGING,     /* buck, chg on */
	CHG_MODE_MAX,
  };

enum {
        BATTERY_VENDOR_START = 0,
        BATTERY_VENDOR_GY = 1,
        BATTERY_VENDOR_XWD = 2,
        BATTERY_VENDOR_NVT = 3,
        BATTERY_VENDOR_UNKNOWN = 4
};

void start_nopmi_chg_jeita_workfunc(void);
void stop_nopmi_chg_jeita_workfunc(void);
int nopmi_chg_jeita_init(struct nopmi_chg_jeita_st *nopmi_chg_jeita);
int nopmi_chg_jeita_deinit(struct nopmi_chg_jeita_st *nopmi_chg_jeita);

#endif
