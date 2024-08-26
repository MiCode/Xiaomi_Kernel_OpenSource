#include "../charger_class/hq_charger_class.h"
#include "../charger_class/hq_cp_class.h"
#include "../common/hq_voter.h"

#define cut_cap(value, min, max)	((min > value) ? min : ((value > max) ? max : value))

#define	false	0
#define	true	1

#define	minus	0
#define	plus	1

#define	QCM_SM_DELAY_1000MS	1000
#define	QCM_SM_DELAY_500MS	500
#define	QCM_SM_DELAY_400MS	400
#define	QCM_SM_DELAY_300MS	300
#define	QCM_SM_DELAY_200MS	200

#define	QC3_VBUS_STEP		200
#define	QC35_VBUS_STEP		20

#define	QC3_MAX_TUNE_STEP	1
#define	QC35_MAX_TUNE_STEP	4

#define	QC2_VBUS_5V			5000
#define	QC2_VBUS_9V			9000
#define	QC2_VBUS_12V		12000

#define	QCM_MAIN_CHG_ICL	100

#define	ANTI_WAVE_COUNT_QC3		7
#define	ANTI_WAVE_COUNT_QC35	1
#define	MAX_TAPER_COUNT			5
#define	MIN_JEITA_CHG_INDEX		3
#define	MAX_JEITA_CHG_INDEX		4

#define	MIN_CP_IBUS		100
#define	MIN_ENTRY_FCC	2000

#define qcm_err(fmt, ...)						\
do {									\
	if (log_level >= 0)						\
		printk(KERN_ERR "[HQCHG_QCM] " fmt, ##__VA_ARGS__);	\
} while (0)

#define qcm_info(fmt, ...)						\
do {									\
	if (log_level >= 1)						\
		printk(KERN_ERR "[HQCHG_QCM] " fmt, ##__VA_ARGS__);	\
} while (0)

#define qcm_dbg(fmt, ...)						\
do {									\
	if (log_level >= 2)						\
		printk(KERN_ERR "[HQCHG_QCM] " fmt, ##__VA_ARGS__);	\
} while (0)

enum qcm_sm_status {
	QCM_SM_CONTINUE,
	QCM_SM_HOLD,
	QCM_SM_EXIT,
};

enum qcm_sm_state {
	QCM_STATE_ENTRY,
	QCM_STATE_INIT_VBUS,
	QCM_STATE_ENABLE_CP,
	QCM_STATE_TUNE,
	QCM_STATE_EXIT,
};

struct qcm_chip {
	struct device *dev;

	struct charger_dev *charger;

	struct chargerpump_dev *master_cp_chg;
	struct chargerpump_dev *slave_cp_chg;

	struct power_supply *usb_psy;
	struct power_supply *bms_psy;

	struct votable *main_icl_votable;
	struct votable *total_fcc_votable;
	struct votable *main_chg_disable;

	struct delayed_work	main_sm_work;
	struct delayed_work	psy_change_work;
	struct notifier_block	nb;
	spinlock_t		psy_change_lock;
	bool			psy_notify_busy;
	bool			qcm_sm_busy;

	int	max_vbus;
	int	max_ibus;
	int	max_ibat;
	int	max_ibus_qc3_18w;
	int	max_ibat_qc3_18w;
	int	max_ibus_qc3_27w;
	int	max_ibat_qc3_27w;
	int	max_ibus_qc35;
	int	max_ibat_qc35;
	int	cv_vbat;
	int	cv_vbat_ffc;

	int	tune_step_ibus_qc3_27;
	int	tune_step_ibus_qc3_18;
	int	tune_step_ibus_qc35;
	int	tune_step_ibus;
	int	tune_step_ibat;
	int	tune_step_vbus;
	int	tune_gap_ibat;;

	int	vbus_step;
	int	ibus_step;
	int	ibat_step;
	int	max_step;
	int	final_step;
	int	anti_wave_count;

	int	high_soc;
	int	sm_state;
	int	sm_status;
	int	qc3_type;
	bool	no_delay;
	bool	master_cp_enable;
	bool	slave_cp_enable;
	bool	disable_slave;
	bool	input_suspend;

	int	master_cp_ibus;
	int	slave_cp_ibus;
	int	cp_total_ibus;
	int	ibat;
	int	vbat;
	int	soc;
	int	target_fcc;
	int	adapter_vbus;
	int	sw_ibus;
	int	tune_vbus_count;
	int	enable_cp_count;
	int	taper_count;
};

static const unsigned char *qcm_sm_state_str[] = {
	"QCM_STATE_ENTRY",
	"QCM_STATE_INIT_VBUS",
	"QCM_STATE_ENABLE_CP",
	"QCM_STATE_TUNE",
	"QCM_STATE_EXIT",
};
