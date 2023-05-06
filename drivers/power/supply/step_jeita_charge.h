#define STEP_JEITA_TUPLE_COUNT	6
#define THERMAL_LIMIT_COUNT	16
#define THERMAL_LIMIT_TUPLE	6

#define FCC_DESCENT_DELAY	1000
#define JEITA_FCC_DESCENT_STEP	1000
#define SW_CV_COUNT		3

#define TYPEC_BURN_TEMP		750
#define TYPEC_BURN_HYST		100

#define MAX_THERMAL_FCC		22000
#define MIN_THERMAL_FCC		500

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

void reset_step_jeita_charge(struct mtk_charger *info);
int step_jeita_init(struct mtk_charger *info, struct device *dev);
