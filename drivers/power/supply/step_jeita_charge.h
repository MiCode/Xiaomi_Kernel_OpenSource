#include <linux/kconfig.h>

#ifdef CONFIG_PRODUCT_ZIRCON
#define STEP_JEITA_TUPLE_COUNT	8
#define STEP_CHG_TUPLE_COUNT	5
#define MIN_FFC_JEITA_CHG_INDEX    4
#define MAX_FFC_JEITA_CHG_INDEX    6
#define PMIC_VBAT_ACCURACY 10
#define PMIC_VBAT_ACCURACY_QC 10
#define JEITA_NOT_CARE_FG_FULL_INDEX	6
#else
#define STEP_JEITA_TUPLE_COUNT	6
#define STEP_CHG_TUPLE_COUNT	6
#define MIN_FFC_JEITA_CHG_INDEX    4
#define MAX_FFC_JEITA_CHG_INDEX    4
#define PMIC_VBAT_ACCURACY 0
#define PMIC_VBAT_ACCURACY_QC 0
#define JEITA_NOT_CARE_FG_FULL_INDEX	5
#endif

#define THERMAL_LIMIT_COUNT	16
#define THERMAL_LIMIT_TUPLE	6

#define FCC_DESCENT_DELAY	1000
#define JEITA_FCC_DESCENT_STEP	1000
#define SW_CV_COUNT		3

#define TYPEC_BURN_TEMP		750
#define TYPEC_BURN_HYST		100

#define MAX_THERMAL_FCC		22000
#define MIN_THERMAL_FCC		500

#define BATT_CONNECT 	   0
#define BATT_ALREADY_CONNECTED  1
#define BATT_DISCONNECT  		3
#define BATT_ALREADY_DISCONNECTED  2

#define BATT_SUPPLIER_READY	0X1
#define BATT_ADAPTING_POWER_READY	0X2
#define STEP_CHARGE_READY	0x4
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

struct step_jeita_cfg2 {
	int low_threshold;
	int high_threshold;
	int value;
	int value_ffc;
};

void reset_step_jeita_charge(struct mtk_charger *info);
int step_jeita_init(struct mtk_charger *info, struct device *dev);
