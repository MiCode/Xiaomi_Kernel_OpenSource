
#ifndef __BQ2589X_HEADER__
#define __BQ2589X_HEADER__
/* Register 00h */
#define BQ2589X_REG_00				0x00
#define BQ2589X_ENHIZ_MASK			0x80
#define BQ2589X_ENHIZ_SHIFT			7
#define BQ2589X_HIZ_ENABLE			1
#define BQ2589X_HIZ_DISABLE			0
#define BQ2589X_ENILIM_MASK			0x40
#define BQ2589X_ENILIM_SHIFT			6
#define BQ2589X_ENILIM_ENABLE			1
#define BQ2589X_ENILIM_DISABLE			0
#define BQ2589X_IINLIM_MASK			0x3F
#define BQ2589X_IINLIM_SHIFT			0
#define BQ2589X_IINLIM_BASE			100
#define BQ2589X_IINLIM_LSB			50
/* Register 01h */
#define BQ2589X_REG_01				0x01
#define	BQ2589X_DPDAC_MASK			0xE0
#define	BQ2589X_DPDAC_SHIFT			5
#define BQ2589X_DP_HIZ				0x00
#define BQ2589X_DP_0V				0x01
#define BQ2589X_DP_0P6V				0x02
#define BQ2589X_DP_1P2V				0x03
#define BQ2589X_DP_2P0V				0x04
#define BQ2589X_DP_2P7V				0x05
#define BQ2589X_DP_3P3V				0x06
#define BQ2589X_DP_SHORT			0x07
#define	BQ2589X_DMDAC_MASK			0x1C
#define	BQ2589X_DMDAC_SHIFT			2
#define BQ2589X_DM_HIZ				0x00
#define BQ2589X_DM_0V				0x01
#define BQ2589X_DM_0P6V				0x02
#define BQ2589X_DM_1P2V				0x03
#define BQ2589X_DM_2P0V				0x04
#define BQ2589X_DM_2P7V				0x05
#define BQ2589X_DM_3P3V				0x06
#define	BQ2589X_EN12V_MASK			0x02
#define	BQ2589X_EN12V_SHIFT			1
#define	BQ2589X_ENABLE_12V			1
#define	BQ2589X_DISABLE_12V			0
#define BQ2589X_VINDPMOS_MASK       0x01
#define BQ2589X_VINDPMOS_SHIFT      0
#define	BQ2589X_VINDPMOS_400MV		0
#define	BQ2589X_VINDPMOS_600MV		1
#else
#define BQ2589X_VINDPMOS_MASK       0x1F
#define BQ2589X_VINDPMOS_SHIFT      0
#define	BQ2589X_VINDPMOS_BASE		0
#define	BQ2589X_VINDPMOS_LSB		100
#endif
/* Register 0x02 */
#define BQ2589X_REG_02              0x02
#define BQ2589X_CONV_START_MASK      0x80
#define BQ2589X_CONV_START_SHIFT     7
#define BQ2589X_CONV_START           0
#define BQ2589X_CONV_RATE_MASK       0x40
#define BQ2589X_CONV_RATE_SHIFT      6
#define BQ2589X_ADC_CONTINUE_ENABLE  1
#define BQ2589X_ADC_CONTINUE_DISABLE 0
#define BQ2589X_BOOST_FREQ_MASK      0x20
#define BQ2589X_BOOST_FREQ_SHIFT     5
#define BQ2589X_BOOST_FREQ_1500K     0
#define BQ2589X_BOOST_FREQ_500K      1
#define BQ2589X_ICOEN_MASK          0x10
#define BQ2589X_ICOEN_SHIFT         4
#define BQ2589X_ICO_ENABLE          1
#define BQ2589X_ICO_DISABLE         0
#define BQ2589X_HVDCPEN_MASK        0x08
#define BQ2589X_HVDCPEN_SHIFT       3
#define BQ2589X_HVDCP_ENABLE        1
#define BQ2589X_HVDCP_DISABLE       0
#define BQ2589X_MAXCEN_MASK         0x04
#define BQ2589X_MAXCEN_SHIFT        2
#define BQ2589X_MAXC_ENABLE         1
#define BQ2589X_MAXC_DISABLE        0
#define BQ2589X_FORCE_DPDM_MASK     0x02
#define BQ2589X_FORCE_DPDM_SHIFT    1
#define BQ2589X_FORCE_DPDM          1
#define BQ2589X_AUTO_DPDM_EN_MASK   0x01
#define BQ2589X_AUTO_DPDM_EN_SHIFT  0
#define BQ2589X_AUTO_DPDM_ENABLE    1
#define BQ2589X_AUTO_DPDM_DISABLE   0
#define BQ2589X_ENABLE_9V           0
#define BQ2589X_EN9V_SHIFT          2
#define BQ2589X_EN9V_MASK           4
#define BQ2589X_DISABLE_AICL        0
#define BQ2589X_AICL_SHIFT          4
#define BQ2589X_AICL_MASK           0x10
/* Register 0x03 */
#define BQ2589X_REG_03              0x03
#define BQ2589X_BAT_VOKOTG_EN_MASK   0x80
#define BQ2589X_BAT_VOKOTG_EN_SHIFT  7
#define BQ2589X_BAT_FORCE_DSEL_MASK  0x80
#define BQ2589X_BAT_FORCE_DSEL_SHIFT 7
#define BQ2589X_WDT_RESET_MASK      0x40
#define BQ2589X_WDT_RESET_SHIFT     6
#define BQ2589X_WDT_RESET           1
#define BQ2589X_OTG_CONFIG_MASK     0x20
#define BQ2589X_OTG_CONFIG_SHIFT    5
#define BQ2589X_OTG_ENABLE          1
#define BQ2589X_OTG_DISABLE         0
#define BQ2589X_CHG_CONFIG_MASK     0x10
#define BQ2589X_CHG_CONFIG_SHIFT    4
#define BQ2589X_CHG_ENABLE          1
#define BQ2589X_CHG_DISABLE         0
#define BQ2589X_SYS_MINV_MASK       0x0E
#define BQ2589X_SYS_MINV_SHIFT      1
#define BQ2589X_SYS_MINV_BASE       3000
#define BQ2589X_SYS_MINV_LSB        100
#define BQ2589X_REG_POC_CHG_CONFIG_MASK		(BIT(5) | BIT(4))
#define BQ2589X_REG_POC_CHG_CONFIG_SHIFT		4
#define BQ2589X_REG_POC_CHG_CONFIG_DISABLE		0x0
#define BQ2589X_REG_POC_CHG_CONFIG_CHARGE		0x1
#define BQ2589X_REG_POC_CHG_CONFIG_OTG			0x2
/* Register 0x04*/
#define BQ2589X_REG_04              0x04
#define BQ2589X_EN_PUMPX_MASK       0x80
#define BQ2589X_EN_PUMPX_SHIFT      7
#define BQ2589X_PUMPX_ENABLE        1
#define BQ2589X_PUMPX_DISABLE       0
#define BQ2589X_ICHG_MASK           0x7F
#define BQ2589X_ICHG_SHIFT          0
#define BQ2589X_ICHG_BASE           0
#define BQ2589X_ICHG_LSB            64
/* Register 0x05*/
#define BQ2589X_REG_05              0x05
#define BQ2589X_IPRECHG_MASK        0xF0
#define BQ2589X_IPRECHG_SHIFT       4
#define BQ2589X_ITERM_MASK          0x0F
#define BQ2589X_ITERM_SHIFT         0
#define BQ2589X_IPRECHG_BASE        64
#define BQ2589X_IPRECHG_LSB         64
#define BQ2589X_ITERM_BASE          64
#define BQ2589X_ITERM_LSB           64
/* Register 0x06*/
#define BQ2589X_REG_06              0x06
#define BQ2589X_VREG_MASK           0xFC
#define BQ2589X_VREG_SHIFT          2
#define BQ2589X_BATLOWV_MASK        0x02
#define BQ2589X_BATLOWV_SHIFT       1
#define BQ2589X_BATLOWV_2800MV      0
#define BQ2589X_BATLOWV_3000MV      1
#define BQ2589X_VRECHG_MASK         0x01
#define BQ2589X_VRECHG_SHIFT        0
#define BQ2589X_VRECHG_100MV        0
#define BQ2589X_VRECHG_200MV        1
#define BQ2589X_VREG_BASE           3840
#define BQ2589X_VREG_LSB            16
/* Register 0x07*/
#define BQ2589X_REG_07              0x07
#define BQ2589X_EN_TERM_MASK        0x80
#define BQ2589X_EN_TERM_SHIFT       7
#define BQ2589X_TERM_ENABLE         1
#define BQ2589X_TERM_DISABLE        0
#define BQ2589X_WDT_MASK            0x30
#define BQ2589X_WDT_SHIFT           4
#define BQ2589X_WDT_DISABLE         0
#define BQ2589X_WDT_40S             1
#define BQ2589X_WDT_80S             2
#define BQ2589X_WDT_160S            3
#define BQ2589X_WDT_BASE            0
#define BQ2589X_WDT_LSB             40
#define BQ2589X_EN_TIMER_MASK       0x08
#define BQ2589X_EN_TIMER_SHIFT      3
#define BQ2589X_CHG_TIMER_ENABLE    1
#define BQ2589X_CHG_TIMER_DISABLE   0
#define BQ2589X_CHG_TIMER_MASK      0x06
#define BQ2589X_CHG_TIMER_SHIFT     1
#define BQ2589X_CHG_TIMER_5HOURS    0
#define BQ2589X_CHG_TIMER_8HOURS    1
#define BQ2589X_CHG_TIMER_12HOURS   2
#define BQ2589X_CHG_TIMER_20HOURS   3
#define BQ2589X_JEITA_ISET_MASK     0x01
#define BQ2589X_JEITA_ISET_SHIFT    0
#define BQ2589X_JEITA_ISET_50PCT    0
#define BQ2589X_JEITA_ISET_20PCT    1
/* Register 0x08*/
#define BQ2589X_REG_08              0x08
#define BQ2589X_BAT_COMP_MASK       0xE0
#define BQ2589X_BAT_COMP_SHIFT      5
#define BQ2589X_VCLAMP_MASK         0x1C
#define BQ2589X_VCLAMP_SHIFT        2
#define BQ2589X_TREG_MASK           0x03
#define BQ2589X_TREG_SHIFT          0
#define BQ2589X_TREG_60C            0
#define BQ2589X_TREG_80C            1
#define BQ2589X_TREG_100C           2
#define BQ2589X_TREG_120C           3
#define BQ2589X_BAT_COMP_BASE       0
#define BQ2589X_BAT_COMP_LSB        20
#define BQ2589X_VCLAMP_BASE         0
#define BQ2589X_VCLAMP_LSB          32
/* Register 0x09*/
#define BQ2589X_REG_09              0x09
#define BQ2589X_FORCE_ICO_MASK      0x80
#define BQ2589X_FORCE_ICO_SHIFT     7
#define BQ2589X_FORCE_ICO           1
#define BQ2589X_TMR2X_EN_MASK       0x40
#define BQ2589X_TMR2X_EN_SHIFT      6
#define BQ2589X_BATFET_DIS_MASK     0x20
#define BQ2589X_BATFET_DIS_SHIFT    5
#define BQ2589X_BATFET_OFF          1
#define BQ2589X_JEITA_VSET_MASK     0x10
#define BQ2589X_JEITA_VSET_SHIFT    4
#define BQ2589X_JEITA_VSET_N150MV   0
#define BQ2589X_JEITA_VSET_VREG     1
#define BQ2589X_BATFET_RST_EN_MASK  0x04
#define BQ2589X_BATFET_RST_EN_SHIFT 2
#define BQ2589X_PUMPX_UP_MASK       0x02
#define BQ2589X_PUMPX_UP_SHIFT      1
#define BQ2589X_PUMPX_UP            1
#define BQ2589X_PUMPX_DOWN_MASK     0x01
#define BQ2589X_PUMPX_DOWN_SHIFT    0
#define BQ2589X_PUMPX_DOWN          1
/* Register 0x0A*/
#define BQ2589X_REG_0A              0x0A
#define BQ2589X_BOOSTV_MASK         0xF0
#define BQ2589X_BOOSTV_SHIFT        4
#define BQ2589X_BOOSTV_BASE         4550
#define BQ2589X_BOOSTV_LSB          64
#define	BQ2589X_PFM_OTG_DIS_MASK	0x08
#define	BQ2589X_PFM_OTG_DIS_SHIFT	3
#define BQ2589X_BOOST_LIM_MASK      0x07
#define BQ2589X_BOOST_LIM_SHIFT     0
#define BQ2589X_BOOST_LIM_500MA     0x00
#define BQ2589X_BOOST_LIM_700MA     0x01
#define BQ2589X_BOOST_LIM_1100MA    0x02
#define BQ2589X_BOOST_LIM_1300MA    0x03
#define BQ2589X_BOOST_LIM_1600MA    0x04
#define BQ2589X_BOOST_LIM_1800MA    0x05
#define BQ2589X_BOOST_LIM_2100MA    0x06
#define BQ2589X_BOOST_LIM_2400MA    0x07
#define BQ2589X_BOOST_VOL_CURR      0x72
/* Register 0x0B*/
#define BQ2589X_REG_0B              0x0B
#define BQ2589X_VBUS_STAT_MASK      0xE0
#define BQ2589X_VBUS_STAT_SHIFT     5
#define BQ2589X_VBUS_TYPE_NONE		0
#define BQ2589X_VBUS_TYPE_SDP		1
#define BQ2589X_VBUS_TYPE_CDP		2
#define BQ2589X_VBUS_TYPE_DCP		3
#define BQ2589X_VBUS_TYPE_HVDCP		4
#define BQ2589X_VBUS_TYPE_UNKNOWN	5
#define BQ2589X_VBUS_TYPE_NON_STD	6
#define BQ2589X_VBUS_TYPE_OTG		7
#define BQ2589X_CHRG_STAT_MASK      0x18
#define BQ2589X_CHRG_STAT_SHIFT     3
#define BQ2589X_CHRG_STAT_IDLE      0
#define BQ2589X_CHRG_STAT_PRECHG    1
#define BQ2589X_CHRG_STAT_FASTCHG   2
#define BQ2589X_CHRG_STAT_CHGDONE   3
#define BQ2589X_PG_STAT_MASK        0x04
#define BQ2589X_PG_STAT_SHIFT       2
#define BQ2589X_SDP_STAT_MASK       0x02
#define BQ2589X_SDP_STAT_SHIFT      1
#define BQ2589X_VSYS_STAT_MASK      0x01
#define BQ2589X_VSYS_STAT_SHIFT     0
/* Register 0x0C*/
#define BQ2589X_REG_0C              0x0c
#define BQ2589X_FAULT_WDT_MASK      0x80
#define BQ2589X_FAULT_WDT_SHIFT     7
#define BQ2589X_FAULT_BOOST_MASK    0x40
#define BQ2589X_FAULT_BOOST_SHIFT   6
#define BQ2589X_FAULT_CHRG_MASK     0x30
#define BQ2589X_FAULT_CHRG_SHIFT    4
#define BQ2589X_FAULT_CHRG_NORMAL   0
#define BQ2589X_FAULT_CHRG_INPUT    1
#define BQ2589X_FAULT_CHRG_THERMAL  2
#define BQ2589X_FAULT_CHRG_TIMER    3
#define BQ2589X_FAULT_BAT_MASK      0x08
#define BQ2589X_FAULT_BAT_SHIFT     3
#define BQ2589X_FAULT_NTC_MASK      0x07
#define BQ2589X_FAULT_NTC_SHIFT     0
#define BQ2589X_FAULT_NTC_TSCOLD    1
#define BQ2589X_FAULT_NTC_TSHOT     2
#define BQ2589X_FAULT_NTC_WARM      2
#define BQ2589X_FAULT_NTC_COOL      3
#define BQ2589X_FAULT_NTC_COLD      5
#define BQ2589X_FAULT_NTC_HOT       6
/* Register 0x0D*/
#define BQ2589X_REG_0D              0x0D
#define BQ2589X_FORCE_VINDPM_MASK   0x80
#define BQ2589X_FORCE_VINDPM_SHIFT  7
#define BQ2589X_FORCE_VINDPM_ENABLE 1
#define BQ2589X_FORCE_VINDPM_DISABLE 0
#define BQ2589X_VINDPM_MASK         0x7F
#define BQ2589X_VINDPM_SHIFT        0
#define BQ2589X_VINDPM_BASE         2600
#define BQ2589X_VINDPM_LSB          100
#define VINDPM_HIGH            8400
#define VINDPM_LOW             4500
/* Register 0x0E*/
#define BQ2589X_REG_0E              0x0E
#define BQ2589X_THERM_STAT_MASK     0x80
#define BQ2589X_THERM_STAT_SHIFT    7
#define BQ2589X_BATV_MASK           0x7F
#define BQ2589X_BATV_SHIFT          0
#define BQ2589X_BATV_BASE           2304
#define BQ2589X_BATV_LSB            20
/* Register 0x0F*/
#define BQ2589X_REG_0F              0x0F
#define BQ2589X_SYSV_MASK           0x7F
#define BQ2589X_SYSV_SHIFT          0
#define BQ2589X_SYSV_BASE           2304
#define BQ2589X_SYSV_LSB            20
/* Register 0x10*/
#define BQ2589X_REG_10              0x10
#define BQ2589X_TSPCT_MASK          0x7F
#define BQ2589X_TSPCT_SHIFT         0
#define BQ2589X_TSPCT_BASE          21
#define BQ2589X_TSPCT_LSB           465//should be 0.465,kernel does not support float
/* Register 0x11*/
#define BQ2589X_REG_11              0x11
#define BQ2589X_VBUS_GD_MASK        0x80
#define BQ2589X_VBUS_GD_SHIFT       7
#define BQ2589X_VBUSV_MASK          0x7F
#define BQ2589X_VBUSV_SHIFT         0
#define BQ2589X_VBUSV_BASE          2600
#define BQ2589X_VBUSV_LSB           100
/* Register 0x12*/
#define BQ2589X_REG_12              0x12
#define BQ2589X_ICHGR_MASK          0x7F
#define BQ2589X_ICHGR_SHIFT         0
#define BQ2589X_ICHGR_BASE          0
#define BQ2589X_ICHGR_LSB           50
/* Register 0x13*/
#define BQ2589X_REG_13              0x13
#define BQ2589X_VDPM_STAT_MASK      0x80
#define BQ2589X_VDPM_STAT_SHIFT     7
#define BQ2589X_IDPM_STAT_MASK      0x40
#define BQ2589X_IDPM_STAT_SHIFT     6
#define BQ2589X_IDPM_LIM_MASK       0x3F
#define BQ2589X_IDPM_LIM_SHIFT      0
#define BQ2589X_IDPM_LIM_BASE       100
#define BQ2589X_IDPM_LIM_LSB        50
/* Register 0x14*/
#define BQ2589X_REG_14              0x14
#define BQ2589X_RESET_MASK          0x80
#define BQ2589X_RESET_SHIFT         7
#define BQ2589X_RESET               1
#define BQ2589X_ICO_OPTIMIZED_MASK  0x40
#define BQ2589X_ICO_OPTIMIZED_SHIFT 6
#define BQ2589X_PN_MASK             0x38
#define BQ2589X_PN_SHIFT            3
#define BQ2589X_TS_PROFILE_MASK     0x04
#define BQ2589X_TS_PROFILE_SHIFT    2
#define BQ2589X_DEV_REV_MASK        0x03
#define BQ2589X_DEV_REV_SHIFT       0

struct bq2589x;

enum {
	PN_BQ25890,
	PN_BQ25892,
	PN_BQ25895,
};
static int pn_data[] = {
	[PN_BQ25890] = 0x03,
	[PN_BQ25892] = 0x00,
	[PN_BQ25895] = 0x07,
};
static char *pn_str[] = {
	[PN_BQ25890] = "bq25890",
	[PN_BQ25892] = "bq25892",
	[PN_BQ25895] = "bq25895",
};
enum bq2589x_part_no {
	BQ25890 = 0x03,
	BQ25892 = 0x00,
	BQ25895 = 0x07,
};
struct chg_para{
	int vlim;
	int ilim;
	int vreg;
	int ichg;
};
struct bq2589x_platform_data {
	int iprechg;
	int iterm;
	int boostv;
	int boosti;
	struct chg_para usb;
};

enum charger_type {
	CHARGER_UNKNOWN = 0,
	STANDARD_HOST,		/* USB : 450mA */
	CHARGING_HOST,
	NONSTANDARD_CHARGER,	/* AC : 450mA~1A */
	STANDARD_CHARGER,	/* AC : ~1A */
	APPLE_2_1A_CHARGER, /* 2.1A apple charger */
	APPLE_1_0A_CHARGER, /* 1A apple charger */
	APPLE_0_5A_CHARGER, /* 0.5A apple charger */
	WIRELESS_CHARGER,
	HVDCP_CHARGER, /* QC2 QC3 QC3.5 PD */
};

#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2

enum bq2589x_usbsw {
	USBSW_CHG = 0,
	USBSW_USB,
};

struct bq2589x {
	struct device *dev;
	struct i2c_client *client;
	enum bq2589x_part_no part_no;
	int revision;
	const char *chg_dev_name;
	const char *eint_name;
	bool chg_det_enable;
	enum charger_type chg_type;
	enum power_supply_usb_type usb_type;
	int status;
	int irq;
	struct mutex i2c_rw_lock;
	bool charge_enabled;	/* Register bit status */
	bool power_good;
	struct bq2589x_platform_data *platform_data;
	struct charger_device *chg_dev;
	struct power_supply *psy;
	struct power_supply_desc usb_desc;
	struct power_supply_config usb_cfg;
	struct power_supply *usb_psy;
	struct power_supply_desc chg_desc;
	struct power_supply_config chg_cfg;
	struct power_supply *chg_psy;
	bool online;
	int real_type;
	int pre_real_type;
	int cc_orientation;
	bool input_suspend;
	struct regulator_dev *rdev;
	struct regulator_desc rdesc;
	int switch_sel_en_gpio;
	bool otg_enable;
	bool is_online;
	int charge_status;
	struct delayed_work float_detect_work;
	int float_detect_count;
	struct delayed_work dcp_detect_work;
	int dcp_detect_count;
	int temp_now;
	struct power_supply *battery_psy;
	bool is_qc_11w;
	bool cdp_detect;
	bool is_soft_full;
	struct wakeup_source *irq_wake_lock;
	int typec_mode;
};

enum {
    PORT_STAT_NOINFO,
    PORT_STAT_SDP,
    PORT_STAT_CDP,
    PORT_STAT_DCP,
    PORT_STAT_HVDCP,
    PORT_STAT_UNKOWNADT,
    PORT_STAT_NOSTAND,
    PORT_STAT_OTG,
};

enum quick_charge_type {
	QUICK_CHARGE_NORMAL = 0,
	QUICK_CHARGE_FAST,
	QUICK_CHARGE_FLASH,
	QUICK_CHARGE_TURBE,
	QUICK_CHARGE_SUPER,
	QUICK_CHARGE_MAX,
};

enum xm_chg_type {
	XM_TYPE_UNKNOW = 0,
	XM_TYPE_SDP,
	XM_TYPE_CDP,
	XM_TYPE_DCP,
	XM_TYPE_HVDCP,
	XM_TYPE_FLOAT,
};

static enum power_supply_usb_type xm_chg_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID,
};

enum power_supply_typec_mode {
	POWER_SUPPLY_TYPEC_NONE,

	POWER_SUPPLY_TYPEC_SINK,
	POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE,
	POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY,
	POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER,
	POWER_SUPPLY_TYPEC_POWERED_CABLE_ONLY,

	POWER_SUPPLY_TYPEC_SOURCE_DEFAULT,
	POWER_SUPPLY_TYPEC_SOURCE_MEDIUM,
	POWER_SUPPLY_TYPEC_SOURCE_HIGH,
	POWER_SUPPLY_TYPEC_NON_COMPLIANT,
};

#define USB_SYSFS_FIELD_RW(_name, _prop)	\
{									 \
	.attr	= __ATTR(_name, 0644, usb_sysfs_show, usb_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name##_set,						\
	.get	= _name##_get,						\
}
#define USB_SYSFS_FIELD_RO(_name, _prop)	\
{			\
	.attr   = __ATTR(_name, 0444, usb_sysfs_show, usb_sysfs_store),\
	.prop   = _prop,				  \
	.get	= _name##_get,						\
}
#define USB_SYSFS_FIELD_WO(_name, _prop)	\
{								   \
	.attr	= __ATTR(_name, 0200, usb_sysfs_show, usb_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name##_set,						\
}
enum usb_property {
	USB_PROP_TYPEC_CC_ORIENTATION,
	USB_PROP_INPUT_SUSPEND,
	USB_PROP_CHIP_STATE,
	USB_PROP_REAL_TYPE,
	USB_PROP_OTG_ENABLE,
	USB_PROP_APDO_MAX,
	USB_PROP_QUICK_CHARGE_TYPE,
	USB_PROP_INPUT_CURRENT_NOW,
	USB_PROP_MTBF_TEST,
	USB_PROP_TYPEC_MODE,
};

struct xm_usb_sysfs_field_info {
	struct device_attribute attr;
	enum usb_property prop;
	int (*set)(struct bq2589x *bq,
		struct xm_usb_sysfs_field_info *attr, int val);
	int (*get)(struct bq2589x *bq,
		struct xm_usb_sysfs_field_info *attr, int *val);
};

enum usb_real_type {
	USB_REAL_TYPE_UNKNOWN = 0,
	USB_REAL_TYPE_SDP,
	USB_REAL_TYPE_CDP,
	USB_REAL_TYPE_DCP,
	USB_REAL_TYPE_HVDCP,
	USB_REAL_TYPE_FLOAT,
};

extern int usb_get_property(enum usb_property bp, int *val);
extern int usb_set_property(enum usb_property bp, int val);
