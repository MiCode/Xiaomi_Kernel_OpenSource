#ifndef BATTERY_COMMON_H
#define BATTERY_COMMON_H

#include <linux/ioctl.h>
#include <mach/mt_typedefs.h>
#include "charging.h"


/*****************************************************************************
 *  BATTERY VOLTAGE
 ****************************************************************************/
#define PRE_CHARGE_VOLTAGE                  3200
#define CONSTANT_CURRENT_CHARGE_VOLTAGE     4100
#define CONSTANT_VOLTAGE_CHARGE_VOLTAGE     4200
#define CV_DROPDOWN_VOLTAGE                 4000
#define CHARGER_THRESH_HOLD                 4300
#define BATTERY_UVLO_VOLTAGE                2700
#ifndef SHUTDOWN_SYSTEM_VOLTAGE
#define SHUTDOWN_SYSTEM_VOLTAGE		3400
#endif

/*****************************************************************************
 *  BATTERY TIMER
 ****************************************************************************/
/* #define MAX_CHARGING_TIME             1*60*60         // 1hr */
/* #define MAX_CHARGING_TIME                   8*60*60   // 8hr */
/* #define MAX_CHARGING_TIME                   12*60*60  // 12hr */
#define MAX_CHARGING_TIME                   24*60*60	/* 24hr */

#define MAX_POSTFULL_SAFETY_TIME		1*30*60	/* 30mins */
#define MAX_PreCC_CHARGING_TIME		1*30*60	/* 0.5hr */

/* #define MAX_CV_CHARGING_TIME                  1*30*60         // 0.5hr */
#define MAX_CV_CHARGING_TIME			3*60*60	/* 3hr */


#define MUTEX_TIMEOUT                       5000
#define BAT_TASK_PERIOD                     10	/* 10sec */
#define g_free_bat_temp					1000	/* 1 s */

/*****************************************************************************
 *  BATTERY Protection
 ****************************************************************************/
#define Battery_Percent_100    100
#define charger_OVER_VOL	    1
#define BATTERY_UNDER_VOL		2
#define BATTERY_OVER_TEMP		3
#define ADC_SAMPLE_TIMES        5

/*****************************************************************************
 *  Pulse Charging State
 ****************************************************************************/
#define  CHR_PRE                        0x1000
#define  CHR_CC                         0x1001
#define  CHR_TOP_OFF                    0x1002
#define  CHR_POST_FULL                  0x1003
#define  CHR_BATFULL                    0x1004
#define  CHR_ERROR                      0x1005
#define  CHR_HOLD						0x1006

/*****************************************************************************
 *  CallState
 ****************************************************************************/
#define CALL_IDLE 0
#define CALL_ACTIVE 1

/*****************************************************************************
 *  Enum
 ****************************************************************************/
typedef unsigned int WORD;


typedef enum {
	PMU_STATUS_OK = 0,
	PMU_STATUS_FAIL = 1,
} PMU_STATUS;


typedef enum {
	USB_SUSPEND = 0,
	USB_UNCONFIGURED,
	USB_CONFIGURED
} usb_state_enum;

typedef enum {
	BATTERY_AVG_CURRENT = 0,
	BATTERY_AVG_VOLT = 1,
	BATTERY_AVG_TEMP = 2,
	BATTERY_AVG_MAX
} BATTERY_AVG_ENUM;

typedef enum {
	BATTERY_THREAD_TIME = 0,
	CAR_TIME,
	SUSPEND_TIME,
	DURATION_NUM
} BATTERY_TIME_ENUM;

/*****************************************************************************
*   JEITA battery temperature standard
    charging info ,like temperatue, charging current, re-charging voltage, CV threshold would be reconfigurated.
    Temperature hysteresis default 6C.
    Reference table:
    degree    AC Current    USB current    CV threshold    Recharge Vol    hysteresis condition
    > 60       no charging current,             X                    X                     <54(Down)
    45~60     600mA         450mA             4.1V               4V                   <39(Down) >60(Up)
    10~45     600mA         450mA             4.2V               4.1V                <10(Down) >45(Up)
    0~10       600mA         450mA             4.1V               4V                   <0(Down)  >16(Up)
    -10~0     200mA         200mA             4V                  3.9V                <-10(Down) >6(Up)
    <-10      no charging current,              X                    X                    >-10(Up)
****************************************************************************/
typedef enum {
	TEMP_BELOW_NEG_10 = 0,
	TEMP_NEG_10_TO_POS_0,
	TEMP_POS_0_TO_POS_10,
	TEMP_POS_10_TO_POS_45,
	TEMP_POS_45_TO_POS_60,
	TEMP_ABOVE_POS_60
} temp_state_enum;


#define TEMP_POS_60_THRESHOLD  50
#define TEMP_POS_60_THRES_MINUS_X_DEGREE 47

#define TEMP_POS_45_THRESHOLD  45
#define TEMP_POS_45_THRES_MINUS_X_DEGREE 39

#define TEMP_POS_10_THRESHOLD  10
#define TEMP_POS_10_THRES_PLUS_X_DEGREE 16

#define TEMP_POS_0_THRESHOLD  0
#define TEMP_POS_0_THRES_PLUS_X_DEGREE 6

#ifdef CONFIG_MTK_FAN5405_SUPPORT
#define TEMP_NEG_10_THRESHOLD  0
#define TEMP_NEG_10_THRES_PLUS_X_DEGREE  0
#elif defined(CONFIG_MTK_BQ24158_SUPPORT)
#define TEMP_NEG_10_THRESHOLD  0
#define TEMP_NEG_10_THRES_PLUS_X_DEGREE  0
#else
#define TEMP_NEG_10_THRESHOLD  0
#define TEMP_NEG_10_THRES_PLUS_X_DEGREE  0
#endif

/*****************************************************************************
 *  Normal battery temperature state
 ****************************************************************************/
typedef enum {
	TEMP_POS_LOW = 0,
	TEMP_POS_NORMAL,
	TEMP_POS_HIGH
} batt_temp_state_enum;

/*****************************************************************************
 *  structure
 ****************************************************************************/
typedef struct {
	kal_bool bat_exist;
	kal_bool bat_full;
	INT32 bat_charging_state;
	UINT32 bat_vol;
	kal_bool bat_in_recharging_state;
	kal_uint32 Vsense;
	kal_bool charger_exist;
	UINT32 charger_vol;
	INT32 charger_protect_status;
	INT32 ICharging;
	INT32 IBattery;
	INT32 temperature;
	INT32 temperatureR;
	INT32 temperatureV;
	UINT32 total_charging_time;
	UINT32 PRE_charging_time;
	UINT32 CC_charging_time;
	UINT32 TOPOFF_charging_time;
	UINT32 POSTFULL_charging_time;
	UINT32 charger_type;
	INT32 SOC;
	INT32 UI_SOC;
	INT32 UI_SOC2;
	UINT32 nPercent_ZCV;
	UINT32 nPrecent_UI_SOC_check_point;
	UINT32 ZCV;
} PMU_ChargerStruct;

/*****************************************************************************
 *  Extern Variable
 ****************************************************************************/
extern PMU_ChargerStruct BMT_status;
extern CHARGING_CONTROL battery_charging_control;
extern kal_bool g_ftm_battery_flag;
extern int charging_level_data[1];
extern kal_bool g_call_state;
extern kal_bool g_charging_full_reset_bat_meter;
#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT) || defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
extern kal_bool ta_check_chr_type;
extern kal_bool ta_cable_out_occur;
extern kal_bool is_ta_connect;
extern struct wake_lock TA_charger_suspend_lock;
#endif


/*****************************************************************************
 *  Extern Function
 ****************************************************************************/
extern void charging_suspend_enable(void);
extern void charging_suspend_disable(void);
extern kal_bool bat_is_charger_exist(void);
extern kal_bool bat_is_charging_full(void);
extern kal_uint32 bat_get_ui_percentage(void);
extern kal_uint32 get_charging_setting_current(void);
extern kal_uint32 bat_is_recharging_phase(void);
extern void do_chrdet_int_task(void);
extern void set_usb_current_unlimited(bool enable);
extern bool get_usb_current_unlimited(void);
extern CHARGER_TYPE mt_get_charger_type(void);

extern kal_uint32 mt_battery_get_duration_time(BATTERY_TIME_ENUM duration_type);
extern void mt_battery_update_time(struct timespec * pre_time, BATTERY_TIME_ENUM duration_type);

extern kal_uint32 mt_battery_shutdown_check(void);

extern kal_uint8 bat_is_kpoc(void);

#ifdef CONFIG_MTK_SMART_BATTERY
extern void wake_up_bat(void);
extern unsigned long BAT_Get_Battery_Voltage(int polling_mode);
extern void mt_battery_charging_algorithm(void);
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
extern PMU_STATUS do_jeita_state_machine(void);
#endif

#else

#define wake_up_bat()			do {} while (0)
#define BAT_Get_Battery_Voltage(polling_mode)	({ 0; })

#endif

#ifdef CONFIG_MTK_POWER_EXT_DETECT
extern kal_bool bat_is_ext_power(void);
#endif



#endif				/* #ifndef BATTERY_COMMON_H */
