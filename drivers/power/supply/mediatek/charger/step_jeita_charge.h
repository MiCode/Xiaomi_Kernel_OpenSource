#define STEP_JEITA_TUPLE_COUNT	7
#define THERMAL_LIMIT_COUNT	16
#define THERMAL_LIMIT_TUPLE	6

#define FCC_DESCENT_DELAY	350
#define JEITA_FCC_DESCENT_STEP	250
#define SW_CV_COUNT		3

#define TYPEC_BURN_LOW_TEMP		600
#define TYPEC_BURN_REPORT_TEMP		650
#define TYPEC_BURN_TEMP		700
#define TYPEC_BURN_HYST		100

#define MAX_THERMAL_FCC		22000
#define MIN_THERMAL_FCC		200

#define MIN_FFC_JEITA_CHG_INDEX    4
#define MAX_FFC_JEITA_CHG_INDEX    5

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

enum cycle_count_status {
	CYCLE_COUNT_FRESH,
	CYCLE_COUNT_LOW,
	CYCLE_COUNT_NORMAL,
	CYCLE_COUNT_HIGH,
};

void reset_mi_charge_alg(struct charger_manager *info);
int step_jeita_init(struct charger_manager *info, struct device *dev, int product_name);
