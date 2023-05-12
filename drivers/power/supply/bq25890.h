#ifndef __BQ25890_HEADER__
#define __BQ25890_HEADER__

enum bq25890_fields {
	F_EN_HIZ, F_EN_ILIM, F_IILIM,				     /* Reg00 */
	F_BHOT, F_BCOLD, F_VINDPM_OFS,				     /* Reg01 */
	F_CONV_START, F_CONV_RATE, F_BOOSTF, F_ICO_EN,
	F_HVDCP_EN, F_MAXC_EN, F_FORCE_DPM, F_AUTO_DPDM_EN,	     /* Reg02 */
	F_BAT_LOAD_EN, F_WD_RST, F_OTG_CFG, F_CHG_CFG, F_SYSVMIN,    /* Reg03 */
	F_PUMPX_EN, F_ICHG,					     /* Reg04 */
	F_IPRECHG, F_ITERM,					     /* Reg05 */
	F_VREG, F_BATLOWV, F_VRECHG,				     /* Reg06 */
	F_TERM_EN, F_STAT_DIS, F_WD, F_TMR_EN, F_CHG_TMR,
	F_JEITA_ISET,						     /* Reg07 */
	F_BATCMP, F_VCLAMP, F_TREG,				     /* Reg08 */
	F_FORCE_ICO, F_TMR2X_EN, F_BATFET_DIS, F_JEITA_VSET,
	F_BATFET_DLY, F_BATFET_RST_EN, F_PUMPX_UP, F_PUMPX_DN,	     /* Reg09 */
	F_BOOSTV, F_BOOSTI,					     /* Reg0A */
	F_VBUS_STAT, F_CHG_STAT, F_PG_STAT, F_SDP_STAT, F_VSYS_STAT, /* Reg0B */
	F_WD_FAULT, F_BOOST_FAULT, F_CHG_FAULT, F_BAT_FAULT,
	F_NTC_FAULT,						     /* Reg0C */
	F_FORCE_VINDPM, F_VINDPM,				     /* Reg0D */
	F_THERM_STAT, F_BATV,					     /* Reg0E */
	F_SYSV,							     /* Reg0F */
	F_TSPCT,						     /* Reg10 */
	F_VBUS_GD, F_VBUSV,					     /* Reg11 */
	F_ICHGR,						     /* Reg12 */
	F_VDPM_STAT, F_IDPM_STAT, F_IDPM_LIM,			     /* Reg13 */
	F_REG_RST, F_ICO_OPTIMIZED, F_PN, F_TS_PROFILE, F_DEV_REV,   /* Reg14 */

	F_MAX_FIELDS
};

/* initial field values, converted to register values */
struct bq25890_init_data {
	u8 ichg;	/* charge current		*/
	u8 vreg;	/* regulation voltage		*/
	u8 iterm;	/* termination current		*/
	u8 iprechg;	/* precharge current		*/
	u8 sysvmin;	/* minimum system voltage limit */
	u8 boostv;	/* boost regulation voltage	*/
	u8 boosti;	/* boost current limit		*/
	u8 boostf;	/* boost frequency		*/
	u8 ilim_en;	/* enable ILIM pin		*/
	u8 treg;	/* thermal regulation threshold */
};

struct bq25890_state {
	u8 online;
	u8 chrg_status;
	u8 chrg_fault;
	u8 vsys_status;
	u8 boost_fault;
	u8 bat_fault;
	u8 vbus_status;
};

struct bq25890_device {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *charger;
	struct power_supply *usb;
	struct power_supply *batpsy;

	struct usb_phy *usb_phy;
	struct notifier_block usb_nb;
	struct work_struct usb_work;
	struct delayed_work dumpic_work;
	struct delayed_work	xm_prop_change_work;
	// struct delayed_work board_therm_work;
	unsigned long usb_event;

	struct regmap *rmap;
	struct regmap_field *rmap_fields[F_MAX_FIELDS];

	int chip_id;
	struct bq25890_init_data init_data;
	struct bq25890_state state;

	struct mutex lock; /* protect state data */

	struct class usb_debug_class;
	struct device batt_device;
	struct regulator	*dpdm_reg;

	struct delayed_work detect_float_work;
	struct delayed_work detect_vbat_set_vindpm_work;
	int detect_force_dpdm_count;
	int charger_val;
	int charger_status;
	int vbus_good_status;
	int input_suspend;
	int hq_test_input_suspend;
	int pd_auth;
	int fake_battery_id;
	int pdactive;
	int otg_enable;
	int charge_type;
	int real_type;
	int typec_cc_orientation;
	int typec_mode;
	int apdo_max_volt;
	int apdo_max_curr;
	int online;
	int update_cont;
	int old_online;
	bool			dpdm_enabled;
	struct mutex		dpdm_lock;
	unsigned int		nchannels;
	/*struct iio_channel	**bq25890_iio_chan_list;*/
	struct iio_chan_spec	*bq25890_iio_chan_ids;
	struct iio_channel		*board_therm_channel;
};
#ifdef CONFIG_HQ_QGKI
extern int bq25890_detect_status(struct bq25890_device *bq);
extern int bq25890_detect_charger_status(struct bq25890_device *bq);
extern int bq25890_charger_start_charge(struct bq25890_device *bq);
extern int bq25890_charger_stop_charge(struct bq25890_device *bq);
extern int bq25890_detect_charger_vbus_good_status(struct bq25890_device *bq);
#endif
#endif
