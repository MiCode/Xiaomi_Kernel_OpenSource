#ifndef __BATTERY_ID_H__

#define __BATTERY_ID_H__

enum {
	POWER_SUPPLY_BATTERY_REMOVED = 0,
	POWER_SUPPLY_BATTERY_INSERTED,
};

enum batt_chrg_prof_type {
	CHRG_PROF_NONE = 0,
	PSE_MOD_CHRG_PROF,
};

/* charging profile structure definition */
struct ps_batt_chg_prof {
	enum batt_chrg_prof_type chrg_prof_type;
	void *batt_prof;
};

/* PSE Modified Algo Structure */
/* Parameters defining the charging range */
struct ps_temp_chg_table {
	/* upper temperature limit for each zone */
	short int temp_up_lim;
	/* charge current and voltage */
	short int full_chrg_vol;
	short int full_chrg_cur;
	/* maintenance thresholds */
	/* maintenance lower threshold. Once battery hits full, charging
	*  charging will be resumed when battery voltage <= this voltage
	*/
	short int maint_chrg_vol_ll;
	/* Charge current and voltage in maintenance mode */
	short int maint_chrg_vol_ul;
	short int maint_chrg_cur;
} __packed;


#define BATTID_STR_LEN		8
#define BATT_TEMP_NR_RNG	6
#define BATTID_UNKNOWN		"UNKNOWNB"
/* Charging Profile */
struct ps_pse_mod_prof {
	/* battery id */
	char batt_id[BATTID_STR_LEN];
	/* type of battery */
	u16 battery_type;
	u16 capacity;
	u16 voltage_max;
	/* charge termination current */
	u16 chrg_term_ma;
	/* Low battery level voltage */
	u16 low_batt_mV;
	/* upper and lower temperature limits on discharging */
	s8 disch_tmp_ul;
	s8 disch_tmp_ll;
	/* number of temperature monitoring ranges */
	u16 temp_mon_ranges;
	struct ps_temp_chg_table temp_mon_range[BATT_TEMP_NR_RNG];
	/* Lowest temperature supported */
	short int temp_low_lim;
} __packed;

/*For notification during battery change event*/
extern struct atomic_notifier_head    batt_id_notifier;

extern void battery_prop_changed(int battery_conn_stat,
				struct ps_batt_chg_prof *batt_prop);
#ifdef CONFIG_POWER_SUPPLY_BATTID
extern int get_batt_prop(struct ps_batt_chg_prof *batt_prop);
#else
static inline int get_batt_prop(struct ps_batt_chg_prof *batt_prop)
{
	return -ENOMEM;
}
#endif
extern int batt_id_reg_notifier(struct notifier_block *nb);
extern void batt_id_unreg_notifier(struct notifier_block *nb);
#endif
