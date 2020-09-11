/*
 * cp_qc30.h
 *
 */

#ifndef SRC_PDLIB_USB_PD_POLICY_MANAGER_H_
#define SRC_PDLIB_USB_PD_POLICY_MANAGER_H_
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

typedef enum  {
	CP_STATE_ENTRY,
	CP_STATE_DISCONNECT,
	CP_STATE_SW_ENTRY,
	CP_STATE_SW_ENTRY_2,
	CP_STATE_SW_LOOP,
	CP_STATE_FLASH2_ENTRY,
	CP_STATE_FLASH2_ENTRY_1,
	CP_STATE_FLASH2_ENTRY_3,
	CP_STATE_FLASH2_TUNE,
	CP_STATE_FLASH2_DELAY,
	CP_STATE_STOP_CHARGE,
} pm_sm_state_t;

#define	BAT_OVP_FAULT_SHIFT			0
#define	BAT_OCP_FAULT_SHIFT			1
#define	BUS_OVP_FAULT_SHIFT			2
#define	BUS_OCP_FAULT_SHIFT			3
#define	BAT_THERM_FAULT_SHIFT			4
#define	BUS_THERM_FAULT_SHIFT			5
#define	DIE_THERM_FAULT_SHIFT			6

#define	BAT_OVP_FAULT_MASK		(1 << BAT_OVP_FAULT_SHIFT)
#define	BAT_OCP_FAULT_MASK		(1 << BAT_OCP_FAULT_SHIFT)
#define	BUS_OVP_FAULT_MASK		(1 << BUS_OVP_FAULT_SHIFT)
#define	BUS_OCP_FAULT_MASK		(1 << BUS_OCP_FAULT_SHIFT)
#define	BAT_THERM_FAULT_MASK		(1 << BAT_THERM_FAULT_SHIFT)
#define	BUS_THERM_FAULT_MASK		(1 << BUS_THERM_FAULT_SHIFT)
#define	DIE_THERM_FAULT_MASK		(1 << DIE_THERM_FAULT_SHIFT)

#define	BAT_OVP_ALARM_SHIFT			0
#define	BAT_OCP_ALARM_SHIFT			1
#define	BUS_OVP_ALARM_SHIFT			2
#define	BUS_OCP_ALARM_SHIFT			3
#define	BAT_THERM_ALARM_SHIFT			4
#define	BUS_THERM_ALARM_SHIFT			5
#define	DIE_THERM_ALARM_SHIFT			6
#define BAT_UCP_ALARM_SHIFT			7

#define	BAT_OVP_ALARM_MASK		(1 << BAT_OVP_ALARM_SHIFT)
#define	BAT_OCP_ALARM_MASK		(1 << BAT_OCP_ALARM_SHIFT)
#define	BUS_OVP_ALARM_MASK		(1 << BUS_OVP_ALARM_SHIFT)
#define	BUS_OCP_ALARM_MASK		(1 << BUS_OCP_ALARM_SHIFT)
#define	BAT_THERM_ALARM_MASK		(1 << BAT_THERM_ALARM_SHIFT)
#define	BUS_THERM_ALARM_MASK		(1 << BUS_THERM_ALARM_SHIFT)
#define	DIE_THERM_ALARM_MASK		(1 << DIE_THERM_ALARM_SHIFT)
#define	BAT_UCP_ALARM_MASK		(1 << BAT_UCP_ALARM_SHIFT)

#define VBAT_REG_STATUS_SHIFT			0
#define IBAT_REG_STATUS_SHIFT			1

#define VBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)
#define IBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)

enum hvdcp3_type {
	HVDCP3_NONE = 0,
	HVDCP3_CLASSA_18W,
	HVDCP3_CLASSB_27W,
	HVDCP3_P_CLASSA_18W,
	HVDCP3_P_CLASSB_27W,
};
#define HVDCP3_CLASS_B_BAT_CURRENT_MA			3600
#define HVDCP3_CLASS_B_BUS_CURRENT_MA			1900
#define HVDCP3_CLASS_A_BAT_CURRENT_MA			3200
#define HVDCP3_CLASS_A_BUS_CURRENT_MA			1700

#define HVDCP3_P_CLASSB_BAT_CURRENT_MA			5600
#define HVDCP3_P_CLASSB_BUS_CURRENT_MA			2700

#define HVDCP3_P_CLASSA_BAT_CURRENT_MA			4000
#define HVDCP3_P_CLASSA_BUS_CURRENT_MA			2100

#define MAX_THERMAL_LEVEL			14
#define JEITA_WARM_THR			480
#define JEITA_COOL_NOT_ALLOW_CP_THR			100
#define JEITA_HYSTERESIS			20

#define PM_WORK_TIME_500MS	500

struct flash2_policy {
	int down_steps;
	uint16_t volt_hysteresis;
};

struct bq2597x {
	bool charge_enabled;

	bool batt_pres;
	bool vbus_pres;

	/* alarm/fault status */
	bool bat_ovp_fault;
	bool bat_ocp_fault;
	bool bus_ovp_fault;
	bool bus_ocp_fault;

	bool bat_ovp_alarm;
	bool bat_ocp_alarm;
	bool bus_ovp_alarm;
	bool bus_ocp_alarm;

	bool bat_ucp_alarm;

	bool bat_therm_alarm;
	bool bus_therm_alarm;
	bool die_therm_alarm;

	bool bat_therm_fault;
	bool bus_therm_fault;
	bool die_therm_fault;

	bool therm_shutdown_flag;
	bool therm_shutdown_stat;

	bool vbat_reg;
	bool ibat_reg;

	int  vout_volt;
	int  vbat_volt;
	int  vbus_volt;
	int  ibat_curr;
	int  ibus_curr;

	int  bat_temp;
	int  bus_temp;
	int  die_temp;
};

struct sw_charger {
	bool charge_enabled;
};

#define PM_STATE_LOG_MAX    32
typedef struct {
	bool        sw_is_charging;
	bool        flash2_is_charging;
	bool        sw_from_flash2;
	bool        sw_near_cv;
	bool		sw_fc2_init_fail;
	bool		bms_fastcharge_mode;

	uint32_t		ibus_lmt_curr;
	pm_sm_state_t     state;
	pm_sm_state_t state_log[PM_STATE_LOG_MAX];
	uint8_t     log_idx;

	int			usb_type;
	int			hvdcp3_type;
	int			usb_present;
	int			ibat_now;
	struct bq2597x			bq2597x;
	struct sw_charger			sw_chager;
	struct votable		*fcc_votable;

	struct power_supply *fc_psy;
	struct power_supply *sw_psy;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;
	struct power_supply *charger_identify_psy;
	/* jeita or thermal related */
	bool			jeita_triggered;
	bool			is_temp_out_fc2_range;
} pm_t;


struct sys_config {
	uint16_t bat_volt_lp_lmt;
	uint16_t bat_curr_lp_lmt;
	uint16_t bus_volt_lp_lmt;
	uint16_t bus_curr_lp_lmt;
	uint16_t bus_volt_alarm_threshold;
	int32_t fc2_taper_current;

	struct flash2_policy flash2_policy;

	uint16_t min_vbat_start_flash2;
	bool	cp_sec_enable;
};

extern int tcpm_typec_set_custom_hv(
	struct tcpc_device *tcpc_dev, bool en);
#endif /* SRC_PDLIB_USB_PD_POLICY_MANAGER_H_ */
