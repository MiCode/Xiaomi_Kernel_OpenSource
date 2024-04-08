#ifndef __QTI_USE_POGO_H__
#define __QTI_USE_POGO_H__

#include <linux/battmngr/battmngr_notifier.h>
#include "linux/battmngr/platform_class.h"
#include <linux/battmngr/battmngr_voter.h>
#include <linux/battmngr/xm_charger_core.h>
#include <linux/soc/qcom/pmic_glink.h>

extern struct battery_chg_dev *g_bcdev;

enum vote_type {
	Nanosic_FCC = 0,
	Nanosic_FV,
	Nanosic_ICL,
};

enum irq_type {
	DCIN_IRQ = 0,
	CHARGER_DONE_IRQ,
	RECHARGE_IRQ,
};

struct battery_charger_irq_notify_msg {
	struct pmic_glink_hdr	hdr;
	u8			value;
	u32			irq_type;
};

enum psy_type {
	PSY_TYPE_BATTERY,
	PSY_TYPE_USB,
	PSY_TYPE_WLS,
	PSY_TYPE_XM,
	PSY_TYPE_MAX,
};

struct psy_state {
	struct power_supply	*psy;
	char			*model;
	const int		*map;
	u32			*prop;
	u32			prop_count;
	u32			opcode_get;
	u32			opcode_set;
};

enum PM_CHGR_BATT_CHGR_STATUS {
	CHGR_BATT_CHGR_STATUS_INHIBIT,
	CHGR_BATT_CHGR_STATUS_TRICKLE,
	CHGR_BATT_CHGR_STATUS_PRECHARGE,
	CHGR_BATT_CHGR_STATUS_FULLON,
	CHGR_BATT_CHGR_STATUS_TAPER,
	CHGR_BATT_CHGR_STATUS_TERMINATION,
	CHGR_BATT_CHGR_STATUS_PAUSE,
	CHGR_BATT_CHG_STATUS_CHARGING_DISABLED,
	CHGR_BATT_CHGR_STATUS_INVALID,
};

#ifdef CONFIG_QTI_POGO_CHG
extern int write_voter_prop_id(u8 buff, u32 val);
extern bool check_g_bcdev_ops(void);
extern int read_voter_property_id(u8 buff, u32 prop_id);
extern int sc8651_wpc_gate_set(int value);
extern int qti_get_ADC_CHGR_STATUS(void);
extern int qti_enale_charge(int enable);
extern int qti_get_DCIN_STATE(void);
extern int qti_deal_report(void);
extern int qti_set_keyboard_plugin(int plugin);

#else
static inline int write_voter_prop_id(u8 buff, u32 val)
{ return val; }
static inline bool check_g_bcdev_ops(void)
{ return ERR_PTR(-ENXIO); }
static inline int read_voter_property_id(u8 buff, u32 prop_id)
{ return prop_id; }
static inline int sc8651_wpc_gate_set(int value)
{ return value; }
static inline int qti_get_ADC_CHGR_STATUS(void)
{ return 0; }
static inline int qti_enale_charge(int enable)
{ return enable; }
#endif


#endif
