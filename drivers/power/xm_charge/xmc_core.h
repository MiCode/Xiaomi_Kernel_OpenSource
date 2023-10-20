#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

#include <linux/usb/typec.h>
#include <linux/power_supply.h>
#include <tcpm.h>
#include "xmc_voter.h"
#include "xmc_ops.h"

#define MAX_CP_DRIVER_NUM	3
#define MAX_BBC_DRIVER_NUM	3
#define THERMAL_TABLE_NUM	6
#define THERMAL_LEVEL_NUM	16
#define STEP_JEITA_TUPLE_NUM	7
#define VOTE_CHARGER_TYPE_NUM	9

#define CHARGE_MONITOR_DELAY	3000
#define NOTCHARGE_MONITOR_DELAY	20000
#define FAST_MONITOR_DELAY	5000

#define RANDOM_CHALLENGE_LEN_MAX	32
#define RANDOM_CHALLENGE_LEN_BQ27Z561	32
#define RANDOM_CHALLENGE_LEN_BQ28Z610	20
#define FG_DEVICE_CHEM_LEN_MAX		10

#define USBPD_MI_SVID				0x2717
#define USBPD_UVDM_SS_LEN			4
#define USBPD_UVDM_VERIFIED_LEN			1
#define USBPD_UVDM_RANDOM_NUM			4
#define USBPD_UVDM_REQUEST			0x1
#define USBPD_UVDM_HDR(svid, cmd0, cmd1)	(((svid) << 16) | (0 << 15) | ((cmd0) << 8) | (cmd1))
#define USBPD_UVDM_HDR_CMD(hdr)			((hdr) & 0xFF)

#define cut_cap(value, min, max)	((min > (value)) ? min : (((value) > (max)) ? (max) : (value)))

#define is_between(left, right, value)				\
		(((left) >= (right) && (left) >= (value)	\
			&& (value) >= (right))			\
		|| ((left) <= (right) && (left) <= (value)	\
			&& (value) <= (right)))

enum copackage_type {
	CHARGE_COPACKAGE_NORMAL,
	CHARGE_COPACKAGE_PRO,
};

enum battery_type {
	BATTERY_SINGLE,
	BATTERY_SERIAL,
	BATTERY_PARALLEL,
};

enum gauge_chip {
	VENDOR_GAUGE,
	BQ27Z561_NFG1000_BQ28Z610,
};

enum buck_boost {
	BBC_PMIC,
	BBC_BQ25790,
	BBC_MP2762,
};

enum charge_pump {
	CPC_NULL,
	CPC_BQ25970_SC8551,
	CPC_LN8000,
	CPC_BQ25980_SC8571,
	CPC_SC8561,
	CPC_LN8410,
};

enum cp_com_type {
	CPCT_NULL,
	CPCT_SINGLE,
	CPCT_SERIAL,
	CPCT_PARALLEL,
};

enum third_cp {
	TCP_NULL,
	TCP_MAX77932,
	TCP_MAX77938,
};

enum bc12_qc_chip {
	BC12_PMIC,
	BC12_XMUSB350_I350,
};

enum xmc_typec_mode {
	POWER_SUPPLY_TYPEC_NONE,

	/* Acting as source */
	POWER_SUPPLY_TYPEC_SINK,			/* Rd only */
	POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE,		/* Rd/Ra */
	POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY,	/* Rd/Rd */
	POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER,		/* Ra/Ra */
	POWER_SUPPLY_TYPEC_POWERED_CABLE_ONLY,		/* Ra only */

	/* Acting as sink */
	POWER_SUPPLY_TYPEC_SOURCE_DEFAULT,		/* Rp default */
	POWER_SUPPLY_TYPEC_SOURCE_MEDIUM,		/* Rp 1.5A */
	POWER_SUPPLY_TYPEC_SOURCE_HIGH,			/* Rp 3A */
	POWER_SUPPLY_TYPEC_NON_COMPLIANT,
};

enum pdm_sm_state {
	PDM_STATE_ENTRY,
	PDM_STATE_INIT_VBUS,
	PDM_STATE_ENABLE_CP,
	PDM_STATE_TUNE,
	PDM_STATE_EXIT,
};

enum pdm_sm_status {
	PDM_STATUS_CONTINUE,
	PDM_STATUS_HOLD,
	PDM_STATUS_EXIT,
};

enum quick_charge_type {
	QUICK_CHARGE_NONE = 0,
	QUICK_CHARGE_NORMAL = 0,
	QUICK_CHARGE_FAST,
	QUICK_CHARGE_FLASH,
	QUICK_CHARGE_TURBE,
	QUICK_CHARGE_SUPER,
	QUICK_CHARGE_MAX,
};

enum xmc_sysfs_prop {
	/* /sys/power_supply/usb */
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE,
	POWER_SUPPLY_PROP_TYPEC_MODE,
	POWER_SUPPLY_PROP_CONNECTOR_TEMP,
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
	POWER_SUPPLY_PROP_PD_AUTHENTICATION,
	POWER_SUPPLY_PROP_APDO_MAX,
	POWER_SUPPLY_PROP_POWER_MAX,
	POWER_SUPPLY_PROP_FFC_ENABLE,
	POWER_SUPPLY_PROP_INPUT_SUSPEND_USB,
	POWER_SUPPLY_PROP_MTBF_TEST,

	/* /sys/power_supply/battery */
	POWER_SUPPLY_PROP_INPUT_SUSPEND_BATTERY,
	POWER_SUPPLY_PROP_NIGHT_CHARGING,
	POWER_SUPPLY_PROP_SMART_BATT,

	/* /sys/power_supply/bms */
	POWER_SUPPLY_PROP_FASTCHARGE_MODE,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_SOC_DECIMAL,
	POWER_SUPPLY_PROP_SOC_DECIMAL_RATE,
	POWER_SUPPLY_PROP_SHUTDOWN_DELAY,
	POWER_SUPPLY_PROP_SHUTDOWN_MODE,
	POWER_SUPPLY_PROP_SOH,
	POWER_SUPPLY_PROP_RM,
	POWER_SUPPLY_PROP_FCC,
	POWER_SUPPLY_PROP_MAX_TEMP,
	POWER_SUPPLY_PROP_TIME_OT,
	POWER_SUPPLY_PROP_QMAX0,
	POWER_SUPPLY_PROP_QMAX1,
	POWER_SUPPLY_PROP_TRUE_REM_Q,
	POWER_SUPPLY_PROP_INITIAL_Q,
	POWER_SUPPLY_PROP_TRUE_FULL_CHG_Q,
	POWER_SUPPLY_PROP_T_SIM,
	POWER_SUPPLY_PROP_CELL_GRID,
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_RSOC,
	POWER_SUPPLY_PROP_CAPACITY_RAW,
};

struct chip_list {
	int battery_type;
	int gauge_chip;
	int buck_boost;
	int charge_pump[MAX_CP_DRIVER_NUM];
	int cp_com_type;
	int third_cp;
	int bc12_qc_chip;
};

struct feature_list {
	bool pdm_support;
	bool qcm_support;
	bool qc3_support;
	bool bypass_support;
	bool sic_support;
};

struct pdm_dts_config {
	int	fv;
	int	fv_ffc;
	int	max_fcc;
	int	max_vbus;
	int	max_ibus;
	int	fcc_low_hyst;
	int	fcc_high_hyst;
	int	low_tbat;
	int	high_tbat;
	int	high_vbat;
	int	high_soc;
	int	cv_vbat;
	int	cv_vbat_ffc;
	int	cv_ibat;
};

struct pdm_chip {
	struct delayed_work main_sm_work;
	struct work_struct psy_change_work;
	struct notifier_block nb;

	spinlock_t psy_change_lock;

	struct pdm_dts_config dts_config;

	enum pdm_sm_state state;
	enum pdm_sm_status sm_status;
	enum xmc_cp_div_mode div_mode;
	enum xmc_cp_div_mode master_cp_mode;
	enum xmc_cp_div_mode slave_cp_mode;
	bool	pdm_active;
	bool	psy_change_running;
	bool	master_cp_enable;
	bool	slave_cp_enable;
	bool	disable_slave;
	bool	no_delay;
	int	div_rate;
	int	master_cp_ibus;
	int	slave_cp_ibus;
	int	total_ibus;
	int	fv;
	int	soc;
	int	ibat;
	int	vbat;
	int	target_fcc;
	int	thermal_limit_fcc;
	int	step_chg_fcc;
	int	master_cp_vbus;
	int	slave_cp_vbus;
	int	vbus_step;
	int	ibus_step;
	int	ibat_step;
	int	vbat_step;
	int	final_step;
	int	request_voltage;
	int	request_current;
	int	retry_count;
	int	entry_vbus;
	int	entry_ibus;
	int	tune_vbus_count;
	int	enable_cp_count;
	int	enable_cp_fail_count;
	int	taper_count;
	int	low_ibus_count;
	int	bms_i2c_error_count;

	int	select_index;
	int	apdo_max_vbus;
	int	apdo_min_vbus;
	int	apdo_max_ibus;
	int	apdo_max_watt;

	int	vbus_control_gpio;
};

struct qcm_chip {

};

struct step_jeita_cfg0 {
	int low_threshold;
	int high_threshold;
	int value;
};

struct step_jeita_cfg1 {
	int low_threshold;
	int high_threshold;
        int extra_threshold;
	int low_value;
        int high_value;
};

enum fg_device_name {
	FG_UNKNOWN,
	FG_BQ27Z561,	/* 1-sirial */
	FG_NFG1000A,	/* 1-sirial */
	FG_NFG1000B,	/* 1-sirial */
	FG_BQ28Z610,	/* 2-sirial */
};

enum fg_device_chem {
	CHEM_UNKNOWN,
	CHEM_LWN,
	CHEM_ATL,
};

struct fg_chip {
	struct device *dev;
	struct i2c_client *client;
	struct mutex i2c_rw_lock;
	struct regmap *regmap;

	u8 regs[30];
	char log_tag[50];
	enum fg_device_chem device_chem;
	enum fg_device_name device_name;
	bool chip_ok;
	bool fac_no_bat;
	bool rw_lock;
	int i2c_error_count;

	struct wakeup_source *gauge_wakelock;
	struct delayed_work clear_rw_lock_work;

	u8 digest[RANDOM_CHALLENGE_LEN_MAX];
	bool authenticate;

	int typical_capacity;
	int report_full_rawsoc;
	int soc_gap;
	int normal_shutdown_vbat;
	int critical_shutdown_vbat;
	bool enable_shutdown_delay;
	int *dec_rate_seq;
	int dec_rate_len;

	int qmax[2];
	int true_rem_q;
	int initial_q;
	int true_full_chg_q;
	int t_sim;
	int cell_grid;

	int plugin_soc;
	int fake_uisoc;
	int uisoc;
	int rsoc;
	int rawsoc;
	int ibat;
	int vbat;
	int fake_vbat;
	int tbat;
	int fake_tbat;
	int rm;
	int fcc;
	int soh;
	int cycle_count;
	bool gauge_full;
	bool fast_charge;
	bool shutdown_mode;
	bool shutdown_delay;
	bool shutdown_flag;
	bool connector_remove;
};

struct adapter_desc {
	uint32_t adapter_svid;
	uint32_t adapter_id;
	uint32_t adapter_fw_ver;
	uint32_t adapter_hw_ver;

	struct xmc_pd_cap cap;
	enum uvdm_state uvdm_state;
	int version;
	int temp;
	int voltage;
	uint8_t power_role; /* phone's power_role */
	uint8_t data_role; /* phone's data_role */
	uint8_t current_state;
	int apdo_max;

	bool authenticate_success;
	bool authenticate_done;
	bool reauth;
	unsigned long s_secert[USBPD_UVDM_SS_LEN];
	unsigned long digest[USBPD_UVDM_SS_LEN];
};

struct buck_boost_desc {
	bool charge_enable;
	bool charge_done;
	bool input_enable;
	bool vbus_disable;
	int vbus;
	int ibus;
	int vbat;
	int state;
	bool mivr;
};

enum bbc_state {
	CHG_STAT_SLEEP,
	CHG_STAT_VBUS_RDY,
	CHG_STAT_TRICKLE,
	CHG_STAT_PRE,
	CHG_STAT_FAST,
	CHG_STAT_EOC,
	CHG_STAT_BKGND,
	CHG_STAT_DONE,
	CHG_STAT_FAULT,
	CHG_STAT_OTG = 15,
	CHG_STAT_MAX,
};

struct usb_typec_desc {
	enum xmc_bc12_type bc12_type;
	enum xmc_qc_type qc_type;
	enum xmc_pd_type pd_type;
	enum xmc_typec_mode typec_mode;
	enum typec_orientation cc_orientation;

	struct regulator *vbus_control;
	struct wakeup_source *burn_wakelock;
	struct wakeup_source *otg_wakelock;
	struct wakeup_source *attach_wakelock;
	int temp;
	int fake_temp;
	bool otg_boost;
	bool cmd_input_suspend;
	bool water_detect;
	bool burn_detect;
};

struct charge_chip {
	struct device *dev;

	struct tcpc_device *tcpc_dev;
	struct notifier_block tcpc_nb;
	struct notifier_block bc12_qc_nb;

	struct delayed_work main_monitor_work;
	struct delayed_work second_monitor_work;
	struct delayed_work burn_monitor_work;
	struct delayed_work audio_adapter_wa_work;
	struct wakeup_source *charger_present_wakelock;
	int monitor_count;

	struct chip_list chip_list;
	struct feature_list feature_list;

	struct xmc_device *master_cp_dev;
	struct xmc_device *slave_cp_dev;
	struct xmc_device *bbc_dev;
	struct xmc_device *gauge_dev;
	struct xmc_device *adapter_dev;

	struct power_supply *battery_psy;
	struct power_supply *usb_psy;
	struct power_supply *ac_psy;
	struct power_supply *bms_psy;
	struct power_supply *master_psy;
	struct power_supply *slave_psy;
	struct power_supply *bbc_psy;
	struct power_supply *bc12_qc_psy;

	struct votable *bbc_en_votable;
	struct votable *bbc_icl_votable;
	struct votable *bbc_fcc_votable;
	struct votable *bbc_fv_votable;
	struct votable *bbc_vinmin_votable;
	struct votable *bbc_iterm_votable;

	struct mutex charger_type_lock;

	struct pdm_chip pdm;
	struct qcm_chip qcm;
	struct fg_chip battery;
	struct adapter_desc adapter;
	struct buck_boost_desc bbc;
	struct usb_typec_desc usb_typec;

	bool ffc_enable;
	int fv;
	int fv_ffc;
	int iterm;
	int iterm_ffc_cool;
	int iterm_ffc_warm;
	int fv_effective;
	int iterm_effective;
	int fcc[VOTE_CHARGER_TYPE_NUM];
	int icl[VOTE_CHARGER_TYPE_NUM];
	int mivr[VOTE_CHARGER_TYPE_NUM];

	bool step_cv;
	int step_fallback_hyst;
	int step_forward_hyst;
	int jeita_fallback_hyst;
	int jeita_forward_hyst;
	int jeita_hysteresis;
	int step_index[2];
	int jeita_index[2];
	struct step_jeita_cfg0 step_chg_cfg[STEP_JEITA_TUPLE_NUM];
	struct step_jeita_cfg0 jeita_fv_cfg[STEP_JEITA_TUPLE_NUM];
	struct step_jeita_cfg1 jeita_fcc_cfg[STEP_JEITA_TUPLE_NUM];

	int thermal_limit[THERMAL_TABLE_NUM][THERMAL_LEVEL_NUM];
	int thermal_level;
	int sic_current;

	bool resume;
	bool init_done;
	bool mtbf_test;
	bool charge_full;
	bool fake_full;
	bool recharge;
	bool can_charge;
	bool charger_present;
	bool fake_charger_present;

	bool night_charging;
	int smart_fv_shift;
};

extern bool max77932_init(void);
extern bool mp2762_init(void);
extern bool bq27z561_init(struct charge_chip *chip);
extern bool sc8551_init(void);
extern bool sc8561_init(void);
extern bool ln8410_init(void);
extern bool ln8000_init(void);
extern bool bq25980_init(void);
extern bool xmusb350_init(void);
extern bool xmc_pdm_init(struct charge_chip *chip);
extern bool xmc_qcm_init(struct charge_chip *chip);
extern bool xmc_sysfs_init(struct charge_chip *chip);
extern bool xmc_detection_init(struct charge_chip *chip);
extern bool xmc_monitor_init(struct charge_chip *chip);
extern bool xmc_adapter_init(struct charge_chip *chip);

extern int xmc_sysfs_get_property(struct power_supply *psy, enum xmc_sysfs_prop prop, union power_supply_propval *val);
extern int xmc_sysfs_set_property(struct power_supply *psy, enum xmc_sysfs_prop prop, const union power_supply_propval *val);
extern int xmc_sysfs_report_uevent(struct power_supply *psy);
extern bool xmc_parse_step_chg_config(struct charge_chip *chip, bool force_update);

extern int xmc_get_log_level(void);

#define xmc_err(fmt, ...)					\
do {								\
	if (xmc_get_log_level() >= 0)				\
		printk(KERN_ERR "[XMCHG]" fmt, ##__VA_ARGS__);	\
} while (0)

#define xmc_info(fmt, ...)					\
do {								\
	if (xmc_get_log_level() >= 1)				\
		printk(KERN_ERR "[XMCHG]" fmt, ##__VA_ARGS__);	\
} while (0)

#define xmc_dbg(fmt, ...)					\
do {								\
	if (xmc_get_log_level() >= 2)				\
		printk(KERN_ERR "[XMCHG]" fmt, ##__VA_ARGS__);	\
} while (0)
