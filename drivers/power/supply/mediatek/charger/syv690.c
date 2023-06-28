#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/hardware_info.h>
#include <mt-plat/v1/charger_class.h>
#include <mt-plat/v1/mtk_charger.h>
#include <tcpm.h>
#include "mtk_charger_intf.h"
#include "mtk_intf.h"
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/mtk_boot_common.h>


void __attribute__ ((weak)) Charger_Detect_Init(void)
{
}

void __attribute__ ((weak)) Charger_Detect_Release(void)
{
}

#define CHARGER_SEC_WORK_DELAY           2000  /* mS */
#define CHARGER_SEC_WORK_MAX     8

#define SYV690_MANUFACTURER		"Silergy"
#define SYV690_IRQ_PIN				 "syv690_irq"

#define SYV690_ID					1
#define BQ25890H_ID 				3
#define SC89890H_ID 				4
#define SYV690_ICHGR_STEP     (50)
#define SYV690_ICHG_STEP     (64)
#define SC89890H_ICHG_STEP   (60)
#define SYV690_VREG_OFFSET (3840)
#define SYV690_VREG_STEP     (16)
#define SYV690_ITEM_OFFSET (64)
#define SYV690_ITEM_STEP     (64)
#define SC89890H_ITEM_OFFSET (60)
#define SC89890H_ITEM_STEP   (60)
#define SC89890H_ITEM_MAX (960)

#define SYV690_IPRECHG_OFFSET (64)
#define SYV690_IPRECHG_STEP     (64)
#define SC89890H_IPRECHG_OFFSET (60)
#define SC89890H_IPRECHG_STEP   (60)

#define SYV690_BOOSTV_OFFSET (4550)
#define SYV690_BOOSTV_STEP     (64)

#define SYV690_BOOSTI_STEP     (600)

#define SYV690_AICR_OFFSET        (100)
#define SYV690_AICR_STEP   		(50)

#define SYV690_MIVR_OFFSET        (2600)
#define SYV690_MIVR_STEP   		(100)

#define SYV690_VBUS_OFFSET        (2600)
#define SYV690_VBUS_STEP   		(100)

#define SYV690_VBAT_OFFSET        (2304)
#define SYV690_VBAT_STEP   		(20)
#define SYV690_VBAT_CALIBRATION   	(30)

#define SYV690_REG_CON0      		(0x00)
#define SYV690_REG_CON1      		(0x01)
#define SYV690_REG_CON2      		(0x02)
#define SYV690_REG_CON3      		(0x03)
#define SYV690_REG_CON4             (0x04)
#define SYV690_REG_CON5             (0x05)
#define SYV690_REG_CON7              (0x07)
#define SYV690_REG_CONB  		(0x0B)
#define SYV690_REG_COND  		(0x0D)
#define CON0_IINLIM_MASK   0x1F

#define CON1_DP_DRIVE_MASK        (0x07)
#define CON1_DP_DRIVE_SHIFT       (0x05)

#define CON1_DM_DRIVE_MASK        (0x07)
#define CON1_DM_DRIVE_SHIFT       (0x02)

#define CON2_FORCE_DPDM_MASK       1
#define CON2_FORCE_DPDM_SHIFT      1

#define CON2_AUTO_DPDM_EN_MASK   1
#define CON2_AUTO_DPDM_EN_SHIFT  0

#define CON2_HVDCP_EN_MASK	       1
#define CON2_HVDCP_EN_SHIFT            3

#define CON3_CHG_CONFIG_MASK  	1
#define CON3_CHG_CONFIG_SHIFT        4

#define CON4_ICHG_CONFIG_MASK        (0X7F)
#define CON4_ICHG_CONFIG_SHIFT       (0)

#define CON5_ITERM_MASK		        (0x0f)
#define CON5_ITERM_SHIFT		        (0)

#define CON7_WTG_TIM_SET_MASK        (0x07)
#define CON7_WTG_TIM_SET_SHIFT       (0x04)

#define CONB_VBUS_STAT_MASK	         (0x07)
#define CONB_VBUS_STAT_SHIFT	         (5)

#define COND_FORCE_VINDPM_MASK	         1
#define COND_FORCE_VINDPM_SHIFT		 7

#define SYV690_TERM_CUR			  (260000)

int device_chipid = 0;
#ifndef pr_fmt
#define pr_fmt(fmt)   "CHGIC_syv690:[%s][%d]" fmt, __func__, __LINE__
#else
#undef pr_fmt
#define pr_fmt(fmt)   "CHGIC_syv690:[%s][%d]" fmt, __func__, __LINE__
#endif

int det_cnt = 0;

enum SYV690_fields {
	F_EN_HIZ, F_EN_ILIM, F_IILIM,				                                    /* Reg00 */
	F_BHOT, F_BCOLD, F_VINDPM_OFS,				                             /* Reg01 */
	F_CONV_START, F_CONV_RATE, F_BOOSTF, F_ICO_EN,
	F_HVDCP_EN, F_MAXC_EN, F_FORCE_DPM, F_AUTO_DPDM_EN,	        /* Reg02 */
	F_BAT_LOAD_EN, F_WD_RST, F_OTG_CFG, F_CHG_CFG, F_SYSVMIN,    /* Reg03 */
	F_PUMPX_EN, F_ICHG,					     					   	 /* Reg04 */
	F_IPRECHG, F_ITERM,					     						 /* Reg05 */
	F_VREG, F_BATLOWV, F_VRECHG,				     					 /* Reg06 */
	F_TERM_EN, F_STAT_DIS, F_WD, F_TMR_EN, F_CHG_TMR,
	F_JEITA_ISET,						     							 /* Reg07 */
	F_BATCMP, F_VCLAMP, F_TREG,				    					 /* Reg08 */
	F_FORCE_ICO, F_TMR2X_EN, F_BATFET_DIS, F_JEITA_VSET,
	F_BATFET_DLY, F_BATFET_RST_EN, F_PUMPX_UP, F_PUMPX_DN,	        /* Reg09 */
	F_BOOSTV, F_BOOSTI,					     						 /* Reg0A */
	F_VBUS_STAT, F_CHG_STAT, F_PG_STAT, F_SDP_STAT, F_VSYS_STAT, /* Reg0B */
	F_WD_FAULT, F_BOOST_FAULT, F_CHG_FAULT, F_BAT_FAULT,
	F_NTC_FAULT,						     						        /* Reg0C */
	F_FORCE_VINDPM, F_VINDPM,				    						 /* Reg0D */
	F_THERM_STAT, F_BATV,					     						 /* Reg0E */
	F_SYSV,							     								 /* Reg0F */
	F_TSPCT,						     								 /* Reg10 */
	F_VBUS_GD, F_VBUSV,					     						 /* Reg11 */
	F_ICHGR,						     								 /* Reg12 */
	F_VDPM_STAT, F_IDPM_STAT, F_IDPM_LIM,			    			 /* Reg13 */
	F_REG_RST, F_ICO_OPTIMIZED, F_PN, F_TS_PROFILE, F_DEV_REV,      /* Reg14 */

	F_MAX_FIELDS
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

/* initial field values, converted to register values */
struct SYV690_init_data {
	int vlim;
	int ilim;
	int ichg;	/* charge current		*/
	int vreg;	/* regulation voltage		*/
	int iterm;	/* termination current		*/
	int iprechg;	/* precharge current		*/
	int boostv;	/* boost regulation voltage	*/
	int boosti;	/* boost current limit		*/
};

struct SYV690_state {
	u8 online;
	u8 chrg_status;
	u8 chrg_fault;
	u8 vsys_status;
	u8 boost_fault;
	u8 bat_fault;
};

enum SYV690_usbsw_state {
	SYV690_USBSW_CHG = 0,
	SYV690_USBSW_USB,
};
struct SYV690_device {
	struct i2c_client *client;
	struct device *dev;

	struct power_supply *charger;
	struct charger_device *chg_dev;
	const char *chg_dev_name;
	const char *eint_name;
	bool chg_det_enable;
	bool charge_enabled;
	int irq;
	int irq_gpio;

	struct regmap *rmap;
	struct regmap_field *rmap_fields[F_MAX_FIELDS];

	int chip_id;
	struct SYV690_init_data init_data;
	struct SYV690_state state;
	struct mutex lock;

	/*usb switch*/
	int usb_switch_cb1_gpio;
	int usb_switch_cb2_gpio;
	int otg_en_gpio;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin1_active;
	struct pinctrl_state *pin1_suspend;
	struct pinctrl_state *pin2_active;
	struct pinctrl_state *pin2_suspend;	
	/* Charger type detection */
	struct mutex chgdet_lock;
	bool attach;
#ifdef CONFIG_TCPC_CLASS
	bool tcpc_attach;
#endif
	enum charger_type chg_type;
	bool bc12_en;
	bool lk_type_is_cdp;
	bool kpoc;

 	struct delayed_work charger_secdet_work;
	struct delayed_work hvdcp_det_work;
	struct delayed_work hardreset_hvdcp_work;
	struct workqueue_struct *chgdet_wq;
	struct work_struct chgdet_work;
	struct delayed_work set_input_current_work;
	int set_input_curr;
	bool need_retry_det ;
	bool  ignore_usb; 
	ktime_t boot_time;
};
struct SYV690_device *g_chg_info = NULL;
int pre_current;
static int SYV690_dump_register(struct charger_device *chg_dev);


static const struct regmap_range SYV690_readonly_reg_ranges[] = {
	regmap_reg_range(0x0b, 0x0c),
	regmap_reg_range(0x0e, 0x13),
};

static const struct regmap_access_table SYV690_writeable_regs = {
	.no_ranges = SYV690_readonly_reg_ranges,
	.n_no_ranges = ARRAY_SIZE(SYV690_readonly_reg_ranges),
};

static const struct regmap_range SYV690_volatile_reg_ranges[] = {
	regmap_reg_range(0x00, 0x00),
	regmap_reg_range(0x09, 0x09),
	regmap_reg_range(0x0b, 0x0c),
	regmap_reg_range(0x0e, 0x14),
};

static const struct regmap_access_table SYV690_volatile_regs = {
	.yes_ranges = SYV690_volatile_reg_ranges,
	.n_yes_ranges = ARRAY_SIZE(SYV690_volatile_reg_ranges),
};

static const struct regmap_config SYV690_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = 0x14,
	.cache_type = REGCACHE_RBTREE,

	.wr_table = &SYV690_writeable_regs,
	.volatile_table = &SYV690_volatile_regs,
};

static const struct reg_field SYV690_reg_fields[] = {
	/* REG00 */
	[F_EN_HIZ]		= REG_FIELD(0x00, 7, 7),
	[F_EN_ILIM]		= REG_FIELD(0x00, 6, 6),
	[F_IILIM]		= REG_FIELD(0x00, 0, 5),
	/* REG01 */
	[F_BHOT]		= REG_FIELD(0x01, 6, 7),
	[F_BCOLD]		= REG_FIELD(0x01, 5, 5),
	[F_VINDPM_OFS]		= REG_FIELD(0x01, 0, 4),
	/* REG02 */
	[F_CONV_START]		= REG_FIELD(0x02, 7, 7),
	[F_CONV_RATE]		= REG_FIELD(0x02, 6, 6),
	[F_BOOSTF]		= REG_FIELD(0x02, 5, 5),
	[F_ICO_EN]		= REG_FIELD(0x02, 4, 4),
	[F_HVDCP_EN]		= REG_FIELD(0x02, 3, 3),
	[F_MAXC_EN]		= REG_FIELD(0x02, 2, 2),
	[F_FORCE_DPM]		= REG_FIELD(0x02, 1, 1),
	[F_AUTO_DPDM_EN]	= REG_FIELD(0x02, 0, 0),
	/* REG03 */
	[F_BAT_LOAD_EN]		= REG_FIELD(0x03, 7, 7),
	[F_WD_RST]		= REG_FIELD(0x03, 6, 6),
	[F_OTG_CFG]		= REG_FIELD(0x03, 5, 5),
	[F_CHG_CFG]		= REG_FIELD(0x03, 4, 4),
	[F_SYSVMIN]		= REG_FIELD(0x03, 1, 3),
	/* REG04 */
	[F_PUMPX_EN]		= REG_FIELD(0x04, 7, 7),
	[F_ICHG]		= REG_FIELD(0x04, 0, 6),
	/* REG05 */
	[F_IPRECHG]		= REG_FIELD(0x05, 4, 7),
	[F_ITERM]		= REG_FIELD(0x05, 0, 3),
	/* REG06 */
	[F_VREG]		= REG_FIELD(0x06, 2, 7),
	[F_BATLOWV]		= REG_FIELD(0x06, 1, 1),
	[F_VRECHG]		= REG_FIELD(0x06, 0, 0),
	/* REG07 */
	[F_TERM_EN]		= REG_FIELD(0x07, 7, 7),
	[F_STAT_DIS]		= REG_FIELD(0x07, 6, 6),
	[F_WD]			= REG_FIELD(0x07, 4, 5),
	[F_TMR_EN]		= REG_FIELD(0x07, 3, 3),
	[F_CHG_TMR]		= REG_FIELD(0x07, 1, 2),
	[F_JEITA_ISET]		= REG_FIELD(0x07, 0, 0),
	/* REG08 */
	[F_BATCMP]		= REG_FIELD(0x08, 5, 7),
	[F_VCLAMP]		= REG_FIELD(0x08, 2, 4),
	[F_TREG]		= REG_FIELD(0x08, 0, 1),
	/* REG09 */
	[F_FORCE_ICO]		= REG_FIELD(0x09, 7, 7),
	[F_TMR2X_EN]		= REG_FIELD(0x09, 6, 6),
	[F_BATFET_DIS]		= REG_FIELD(0x09, 5, 5),
	[F_JEITA_VSET]		= REG_FIELD(0x09, 4, 4),
	[F_BATFET_DLY]		= REG_FIELD(0x09, 3, 3),
	[F_BATFET_RST_EN]	= REG_FIELD(0x09, 2, 2),
	[F_PUMPX_UP]		= REG_FIELD(0x09, 1, 1),
	[F_PUMPX_DN]		= REG_FIELD(0x09, 0, 0),
	/* REG0A */
	[F_BOOSTV]		= REG_FIELD(0x0A, 4, 7),
	[F_BOOSTI]		= REG_FIELD(0x0A, 0, 2),
	/* REG0B */
	[F_VBUS_STAT]		= REG_FIELD(0x0B, 5, 7),
	[F_CHG_STAT]		= REG_FIELD(0x0B, 3, 4),
	[F_PG_STAT]		= REG_FIELD(0x0B, 2, 2),
	[F_SDP_STAT]		= REG_FIELD(0x0B, 1, 1),
	[F_VSYS_STAT]		= REG_FIELD(0x0B, 0, 0),
	/* REG0C */
	[F_WD_FAULT]		= REG_FIELD(0x0C, 7, 7),
	[F_BOOST_FAULT]		= REG_FIELD(0x0C, 6, 6),
	[F_CHG_FAULT]		= REG_FIELD(0x0C, 4, 5),
	[F_BAT_FAULT]		= REG_FIELD(0x0C, 3, 3),
	[F_NTC_FAULT]		= REG_FIELD(0x0C, 0, 2),
	/* REG0D */
	[F_FORCE_VINDPM]	= REG_FIELD(0x0D, 7, 7),
	[F_VINDPM]		= REG_FIELD(0x0D, 0, 6),
	/* REG0E */
	[F_THERM_STAT]		= REG_FIELD(0x0E, 7, 7),
	[F_BATV]		= REG_FIELD(0x0E, 0, 6),
	/* REG0F */
	[F_SYSV]		= REG_FIELD(0x0F, 0, 6),
	/* REG10 */
	[F_TSPCT]		= REG_FIELD(0x10, 0, 6),
	/* REG11 */
	[F_VBUS_GD]		= REG_FIELD(0x11, 7, 7),
	[F_VBUSV]		= REG_FIELD(0x11, 0, 6),
	/* REG12 */
	[F_ICHGR]		= REG_FIELD(0x12, 0, 6),
	/* REG13 */
	[F_VDPM_STAT]		= REG_FIELD(0x13, 7, 7),
	[F_IDPM_STAT]		= REG_FIELD(0x13, 6, 6),
	[F_IDPM_LIM]		= REG_FIELD(0x13, 0, 5),
	/* REG14 */
	[F_REG_RST]		= REG_FIELD(0x14, 7, 7),
	[F_ICO_OPTIMIZED]	= REG_FIELD(0x14, 6, 6),
	[F_PN]			= REG_FIELD(0x14, 3, 5),
	[F_TS_PROFILE]		= REG_FIELD(0x14, 2, 2),
	[F_DEV_REV]		= REG_FIELD(0x14, 0, 1)
};

/*
 * Most of the val -> idx conversions can be computed, given the minimum,
 * maximum and the step between values. For the rest of conversions, we use
 * lookup tables.
 */
enum SYV690_table_ids {
	/* range tables */
	TBL_ICHG,
	TBL_ITERM,
	TBL_IPRECHG,
	TBL_VREG,
	TBL_BATCMP,
	TBL_VCLAMP,
	TBL_BOOSTV,
	TBL_SYSVMIN,

	/* lookup tables */
	TBL_TREG,
	TBL_BOOSTI,
};

enum SYV690_status {
	STATUS_NOT_CHARGING,
	STATUS_PRE_CHARGING,
	STATUS_FAST_CHARGING,
	STATUS_TERMINATION_DONE,
};

enum SYV690_chrg_fault {
	CHRG_FAULT_NORMAL,
	CHRG_FAULT_INPUT,
	CHRG_FAULT_THERMAL_SHUTDOWN,
	CHRG_FAULT_TIMER_EXPIRED,
};

enum SYV690_pmu_chg_type {
	SYV690_CHG_TYPE_NONE = 0,
	SYV690_CHG_TYPE_SDP,
	SYV690_CHG_TYPE_CDP,
	SYV690_CHG_TYPE_DCP,
	SYV690_CHG_TYPE_HVDCP,
	SYV690_CHG_TYPE_UNKNOWN,
	SYV690_CHG_TYPE_NON_STD,
	SYV690_CHG_TYPE_OTG,
	SYV690_CHG_TYPE_MAX,
};

static int SYV690_psy_online_changed(struct SYV690_device *chgInfo);
static int SYV690_psy_chg_type_changed(struct SYV690_device *chgInfo);
static  int SYV690_enable_force_dpdm(struct SYV690_device *chgInfo,bool en);
static  int SYV690_is_dpdm_done(struct SYV690_device *chgInfo,int *done);

static int __SYV690_read_reg(struct SYV690_device *chgInfo, u8 reg, u8 *data)
{
	int ret = 0;

	ret = i2c_smbus_read_byte_data(chgInfo->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __SYV690_write_reg(struct SYV690_device *chgInfo, int reg, u8 val)
{
	int ret = 0;

	ret = i2c_smbus_write_byte_data(chgInfo->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		return ret;
	}
	return 0;
}

static int SYV690_read_byte(struct SYV690_device *chgInfo, u8 reg, u8 *data)
{
	int ret = 0;

	mutex_lock(&chgInfo->lock);
	ret = __SYV690_read_reg(chgInfo, reg, data);
	mutex_unlock(&chgInfo->lock);

	return ret;
}

static int SYV690_write_byte(struct SYV690_device *chgInfo, u8 reg, u8 data)
{
	int ret = 0;

	mutex_lock(&chgInfo->lock);
	ret = __SYV690_write_reg(chgInfo, reg, data);
	mutex_unlock(&chgInfo->lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}
#if 1
static int SYV690_update_bits(struct SYV690_device *chgInfo, u8 reg,u8 data, u8 mask ,u8 shift)
{
	int ret =0;
	u8 tmp = 0;

	mutex_lock(&chgInfo->lock);
	ret = __SYV690_read_reg(chgInfo, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~(mask << shift);
	tmp |= ((data&mask) << shift);
//	pr_err("%s: reg=%02X, tmp=%02X\n", __func__,reg, tmp);
	ret = __SYV690_write_reg(chgInfo, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&chgInfo->lock);
	return ret;
}
#endif
static int SYV690_field_read(struct SYV690_device *bq,
			      enum SYV690_fields field_id)
{
	int ret;
	int val;

	ret = regmap_field_read(bq->rmap_fields[field_id], &val);
	if (ret < 0){
		pr_err("Failed: read reg\n");
		return ret;
	}
	return val;
}

static int SYV690_field_write(struct SYV690_device *bq,
			       enum SYV690_fields field_id, u8 val)
{
	return regmap_field_write(bq->rmap_fields[field_id], val);
}



static int SYV690_get_chip_state(struct SYV690_device *bq,
				  struct SYV690_state *state)
{
	int i, ret;

	struct {
		enum SYV690_fields id;
		u8 *data;
	} state_fields[] = {
		{F_CHG_STAT,	&state->chrg_status},
		{F_PG_STAT,		&state->online},
		{F_VSYS_STAT,	&state->vsys_status},
		{F_BOOST_FAULT, &state->boost_fault},
		{F_BAT_FAULT,	&state->bat_fault},
		{F_CHG_FAULT,	&state->chrg_fault}
	};

	for (i = 0; i < ARRAY_SIZE(state_fields); i++) {
		ret = SYV690_field_read(bq, state_fields[i].id);
		if (ret < 0)
			return ret;

		*state_fields[i].data = ret;
	}

	pr_err("S:CHG/PG/VSYS=%d/%d/%d, F:CHG/BOOST/BAT=%d/%d/%d\n",
		state->chrg_status, state->online, state->vsys_status,
		state->chrg_fault, state->boost_fault, state->bat_fault);

	return 0;
}

static bool SYV690_state_changed(struct SYV690_device *bq,
				  struct SYV690_state *new_state)
{
	struct SYV690_state old_state;

	old_state = bq->state;

	return (old_state.chrg_status != new_state->chrg_status ||
		old_state.chrg_fault != new_state->chrg_fault	||
		old_state.online != new_state->online		||
		old_state.bat_fault != new_state->bat_fault	||
		old_state.boost_fault != new_state->boost_fault ||
		old_state.vsys_status != new_state->vsys_status);
}

static void SYV690_handle_state_change(struct SYV690_device *bq,
					struct SYV690_state *new_state)
{
	struct SYV690_state old_state;
	old_state = bq->state;

	if (!new_state->online) {			     /* power removed */
		pre_current = 0;
		pr_err("%s :remove!\n",__func__);
	} else if (!old_state.online) {			    /* power inserted */
		pr_err("%s :attatch!\n",__func__);
	}
	return;
}

static int SYV690_chgdet_post_process(struct SYV690_device *chgInfo);
static void do_chgdet_work_handler(struct work_struct *work)
{
	struct SYV690_device *chginfo =(struct SYV690_device *)container_of(work, struct SYV690_device, chgdet_work);

	if (chginfo->bc12_en) {
		SYV690_chgdet_post_process(chginfo);
	}else{
		pr_err("%s: bc12 disabled, ignore irq\n",__func__);
	}
}

static int SYV690_set_aicr(struct charger_device *chg_dev, u32 curr);
static int SYV690_get_aicr(struct charger_device *chg_dev, u32 *curr);
static void do_set_input_current_work(struct work_struct *work)
{
	struct charger_device *chg_dev = get_charger_by_name("primary_chg");

	pr_info("%s curr = %d\n", __func__, g_chg_info->set_input_curr);
	SYV690_set_aicr(chg_dev, g_chg_info->set_input_curr);
}

static int SYV690_set_input_current(struct charger_device *chg_dev, u32 curr)
{
#ifdef FACTORY_BUILD
	int aicl_cur;
#endif
	g_chg_info->set_input_curr = curr;
	pr_info("%s curr = %d\n", __func__, curr);

	SYV690_set_aicr(chg_dev, g_chg_info->set_input_curr);
  
#ifdef FACTORY_BUILD
	SYV690_get_aicr(chg_dev, &aicl_cur);
	if (aicl_cur == 500000 && g_chg_info->set_input_curr != aicl_cur)
		schedule_delayed_work(&g_chg_info->set_input_current_work, msecs_to_jiffies(2000));
#endif
	return 0;
}

static irqreturn_t SYV690_irq_handler_thread(int irq, void *private)
{
	struct SYV690_device *chginfo = private;
	int ret = 0;
	struct SYV690_state state;

	ret = SYV690_get_chip_state(chginfo, &state);
	if (ret < 0)
		goto handled;

	if (!SYV690_state_changed(chginfo, &state))
		goto handled;

	SYV690_handle_state_change(chginfo, &state);
	chginfo->state = state;

#if 0
	if (!chginfo->bc12_en) {
		pr_err("%s: bc12 disabled, ignore irq\n",__func__);
		goto handled;
	}
	SYV690_chgdet_post_process(chginfo);
#endif

	//queue_work(chginfo->chgdet_wq, &chginfo->chgdet_work);
/*	chginfo->attach = state.online;
	if (chginfo->attach) 
		chginfo->chg_type = STANDARD_HOST;
	ret = SYV690_psy_online_changed(chginfo);
	if (ret < 0)
		pr_err("%s: report psy online fail\n", __func__);
	SYV690_psy_chg_type_changed(chginfo);
*/
handled:
	pr_err("%s \n",__func__);
	return IRQ_HANDLED;
}

#if 0 
static int SYV690_chip_reset(struct SYV690_device *bq)
{
	int ret;
	int rst_check_counter = 10;

	ret = SYV690_field_write(bq, F_REG_RST, 1);
	if (ret < 0)
		return ret;

	do {
		ret = SYV690_field_read(bq, F_REG_RST);
		if (ret < 0)
			return ret;

		usleep_range(5, 10);
	} while (ret == 1 && --rst_check_counter);

	if (!rst_check_counter)
		return -ETIMEDOUT;

	return 0;
}
#endif

static int SYV690_enable_term(struct SYV690_device *chgInfo, bool enable);
static int SYV690_hw_init(struct SYV690_device *bq)
{
	int ret;
	int i;
	struct SYV690_state state;

	const struct {
		enum SYV690_fields id;
		u32 value;
	} init_data[] = {
		{F_EN_ILIM, 0},
		{F_CONV_START , 0},
		{F_CONV_RATE , 0},
		{F_ICHG,	 	 (bq->init_data.ichg/SYV690_ICHG_STEP)},
		{F_VREG,	 (bq->init_data.vreg-SYV690_VREG_OFFSET)/SYV690_VREG_STEP},
		{F_ITERM,	 (bq->init_data.iterm -SYV690_ITEM_OFFSET)/SYV690_ITEM_STEP},
		{F_IPRECHG,	 (bq->init_data.iprechg - SYV690_IPRECHG_OFFSET)/SYV690_IPRECHG_STEP},
		{F_BOOSTV,	 (bq->init_data.boostv - SYV690_BOOSTV_OFFSET)/SYV690_BOOSTV_STEP},
		{F_BOOSTI,	 (bq->init_data.boosti/SYV690_BOOSTI_STEP)},
		//reg2 9v
		{F_MAXC_EN,			0},
		{F_BATFET_DLY , 		1},
		{F_BATFET_RST_EN , 	0},
		{F_FORCE_VINDPM, 1}
	};

#if 0
	ret = SYV690_chip_reset(bq);
	if (ret < 0)
		return ret;
#endif

	/* disable watchdog */
	ret = SYV690_field_write(bq, F_WD, 0);
	if (ret < 0)
		return ret;

	/* initialize currents/voltages and other parameters */
	for (i = 0; i < ARRAY_SIZE(init_data); i++) {
		ret = SYV690_field_write(bq, init_data[i].id,
					  init_data[i].value);
		if (ret < 0)
			return ret;
	}
	if (bq->chip_id == BQ25890H_ID) {
		SYV690_field_write(bq, F_BATCMP, 0);
  		SYV690_field_write(bq, F_VCLAMP, 0);
  		SYV690_field_write(bq, F_HVDCP_EN, 0);
	}
	if (bq->chip_id == SC89890H_ID) {
		ret = SYV690_field_write(bq, F_ICHG,
					  bq->init_data.ichg/SC89890H_ICHG_STEP);		
	
		if (ret < 0)
			return ret;
		
		  SYV690_write_byte(bq, 0x7d, 0x48);
                  SYV690_write_byte(bq, 0x7d, 0x54);
                  SYV690_write_byte(bq, 0x7d, 0x53);
                  SYV690_write_byte(bq, 0x7d, 0x38);
                  SYV690_write_byte(bq, 0x80, 0x80);
                  SYV690_write_byte(bq, 0x7d, 0x48);
                  SYV690_write_byte(bq, 0x7d, 0x54);
                  SYV690_write_byte(bq, 0x7d, 0x53);
                  SYV690_write_byte(bq, 0x7d, 0x38);

		ret = SYV690_field_write(bq, F_ITERM,
					  (bq->init_data.iterm - SC89890H_ITEM_OFFSET)/SC89890H_ITEM_STEP);
		if (ret < 0)
			return ret;
		ret = SYV690_field_write(bq, F_IPRECHG,
					  (bq->init_data.iprechg - SC89890H_IPRECHG_OFFSET)/SC89890H_IPRECHG_STEP);
		if (ret < 0)
			return ret;
	}
#if 0
	/* Configure ADC for continuous conversions. This does not enable it. */
	ret = SYV690_field_write(bq, F_CONV_RATE, 1);
	if (ret < 0)
		return ret;
#endif
	ret = SYV690_get_chip_state(bq, &state);
	if (ret < 0)
		return ret;

	mutex_lock(&bq->lock);
	bq->state = state;
	mutex_unlock(&bq->lock);

	return 0;
}
/*
		charger_ops intf
 
*/
static int SYV690_charging(struct charger_device *chg_dev, bool enable)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;
	
	ret = SYV690_field_write(chgInfo, F_CHG_CFG, enable);
	pr_err("%s charger %s\n", enable ? "enable" : "disable", ret < 0 ? "failed" : "successfully");
	ret = SYV690_field_read(chgInfo, F_CHG_CFG);
	/*
			ret < 0 read fail; 1 enable 0 disable
	*/
	if (ret >= 0){
		chgInfo->charge_enabled = ret;
	}
	if ((chgInfo->chip_id == SC89890H_ID || chgInfo->chip_id == BQ25890H_ID) && enable) {
		SYV690_update_bits(chgInfo,SYV690_REG_COND, 1, COND_FORCE_VINDPM_MASK,COND_FORCE_VINDPM_SHIFT);
		ret = SYV690_read_byte(chgInfo, 0x0d, &val);
		pr_info("%s SC89890H force vindpm:0x%x\n", __func__, val);
	}
	SYV690_dump_register(chg_dev);
	return ret;
}
/*
		charger_ops intf

*/

int SYV690_reset_watchdog_timer(struct SYV690_device *chgInfo)
{
	int ret = 0;
	ret = SYV690_field_write(chgInfo, F_WD_RST, 1);
	pr_err("SYV690_reset_watchdog_timer %s\n", ret < 0 ? "failed" : "successfully");
	return ret;
}
EXPORT_SYMBOL_GPL(SYV690_reset_watchdog_timer);

/*
		charger_ops intf

*/
int SYV690_set_term_current(struct SYV690_device *chgInfo, int curr)
{
	u8 iterm = 0;
	int ret = 0;
	int bat_temp = 0;
	struct power_supply *bms = NULL;
	union power_supply_propval val = {0,};

	bms = power_supply_get_by_name("bms");
	if (!bms) {
  		pr_err("%s %d: get power supply failed!\n", __func__, __LINE__);
		return ret;
	}

	ret = power_supply_get_property(bms,
			POWER_SUPPLY_PROP_TEMP, &val);
	if (ret)
		pr_err("Failed to read ibat\n");
	else
		bat_temp = val.intval;

	/*min 64 ma ,step 64*/
	if (chgInfo->chip_id == SYV690_ID || chgInfo->chip_id == BQ25890H_ID) {
		if (curr < SYV690_ITEM_OFFSET)
			curr = SYV690_ITEM_OFFSET;

		iterm = (curr - SYV690_ITEM_OFFSET) / SYV690_ITEM_STEP;
	} else if (chgInfo->chip_id == SC89890H_ID) { /* min 60 ma ,step 60 */
		if (bat_temp < 150) {
			pr_info("%s temp %d low 15, iterm add 120\n", __func__, bat_temp);
			curr += 120;
		}
		if (curr < SC89890H_ITEM_OFFSET)
			curr = SC89890H_ITEM_OFFSET;
		if (curr > SC89890H_ITEM_MAX)
			curr = SC89890H_ITEM_MAX;

		iterm = (curr - SC89890H_ITEM_OFFSET) / SC89890H_ITEM_STEP;
	}
	ret = SYV690_field_write(chgInfo, F_ITERM, iterm);
	pr_err(" SYV690_set_term_current  write :%s\n",  ret < 0 ? "failed" : "successfully");
	return ret;

}
EXPORT_SYMBOL_GPL(SYV690_set_term_current);

/*
		charger_ops

*/
static int SYV690_enable_term(struct SYV690_device *chgInfo, bool enable)
{
	int ret = 0;
	ret = SYV690_field_write(chgInfo, F_TERM_EN, (enable ? 1:0));
	pr_err(" SYV690_enable_term  write :%s\n",  ret < 0 ? "failed" : "successfully");
	return ret;
}
EXPORT_SYMBOL_GPL(SYV690_enable_term);
/*
		charger_ops

*/
int SYV690_adc_read_charge_current(struct SYV690_device *chgInfo)
{

	int curr = 0;
	int ret = 0;

	ret = SYV690_field_read(chgInfo, F_ICHGR);
	/*	ret < 0 read fail; 	*/
	if (ret >= 0){
		curr = ret*SYV690_ICHGR_STEP;
	}
	return curr;
}
EXPORT_SYMBOL_GPL(SYV690_adc_read_charge_current);
/*
		charger_ops

*/
static int SYV690_process_safety_timer(struct SYV690_device *chgInfo ,bool enable)
{
	int ret = 0;
	ret = SYV690_field_write(chgInfo, F_TMR_EN, (enable ? 1:0));
	pr_err(" SYV690_process_safety_timer  write :%s\n",  ret < 0 ? "failed" : "successfully");
	return ret;
}
EXPORT_SYMBOL_GPL(SYV690_process_safety_timer);
/*
		charger_ops

*/
int SYV690_enter_hiz_mode(struct SYV690_device *chgInfo, bool enable)
{
	int ret = 0;
	ret = SYV690_field_write(chgInfo, F_EN_HIZ, (enable ? 1:0));
	pr_err(" SYV690_enter_hiz_mode  write :%s\n",  ret < 0 ? "failed" : "successfully");
	return ret;
}
EXPORT_SYMBOL_GPL(SYV690_enter_hiz_mode);
/*
		charger_ops

*/
int SYV690_force_dpdm(struct SYV690_device *chgInfo)
{
	int ret = 0;
	ret = SYV690_field_write(chgInfo, F_FORCE_DPM, 1);
	pr_err(" SYV690_force_dpdm  write :%s\n",  ret < 0 ? "failed" : "successfully");
	return ret;
}
EXPORT_SYMBOL_GPL(SYV690_force_dpdm);
/*
		charger_ops

*/
#if 0
static int SYV690_check_dpdm_done(struct SYV690_device *chgInfo, bool *done)
{
	int ret = 0;
	ret = SYV690_field_read(chgInfo, F_FORCE_DPM);
	/*	ret < 0 read fail; 	*/
	if (ret >= 0){
		*done = ret;
	}
	return ret;
}
#endif
/*
		charger_ops

*/
#if 0
static int SYV690_check_vbus_type(struct SYV690_device *chgInfo)
{
	int ret = 0;
	ret = SYV690_field_read(chgInfo, F_VBUS_STAT);
	/*	ret < 0 read fail; 	*/
	//pr_err(" SYV690_check_vbus_type  read :%s\n",  ret < 0 ? "failed" : "successfully");
	return ret;
}
#endif
/*
		attatched

*/

static int SYV690_plug_in(struct charger_device *chg_dev)
{

	int ret = 0;

	ret = SYV690_charging(chg_dev, true);

	if (ret<0)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;
}
/*
		detatch

*/

static int SYV690_plug_out(struct charger_device *chg_dev)
{
	int ret =0;

	ret = SYV690_charging(chg_dev, false);

	if (ret<0)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}
/*
		charger_ops

*/
/* Number of char to write < size,then write normally,add 0x00 at end;
 * When number of char to writer >= size,then only write (size -1) char,then write 0x00 at end.
 * return the number of char to want to write.
 */
extern int snprintf(char* dest_str,size_t size,const char* format,...);
static int SYV690_dump_register(struct charger_device *chg_dev)
{
	#define  REG_DUMP_LEN   400
	u8 reg_dump_str[REG_DUMP_LEN] = {0x00};
	int snprintf_wr_idx = 0;
        struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
        u8 addr = 0;
        u8 val = 0 ;
        int ret = 0;

        memset((void *)reg_dump_str, 0x00, sizeof(reg_dump_str));

        for (addr = 0x0; addr <= 0x14; addr++) {
                ret = SYV690_read_byte(chgInfo, addr, &val);
                if (!ret){ /* successfull */
                        if(snprintf_wr_idx <= REG_DUMP_LEN - 2 ) // snprintf write 0x00 in last byte.format str len is 15
                                snprintf_wr_idx += snprintf(reg_dump_str + snprintf_wr_idx, REG_DUMP_LEN - snprintf_wr_idx, "t-reg[%.2x]:0x%.2x,", addr, val);
                        else
				pr_err("masterIC reg_info:Array idx out of range!snprintf_wr_idx:%d;t-REG[%.2x] = 0x%.2x \n ", snprintf_wr_idx, addr, val);
                }else{  /* fail */
                        if(snprintf_wr_idx <= REG_DUMP_LEN - 2 )
				snprintf_wr_idx += snprintf(reg_dump_str + snprintf_wr_idx, REG_DUMP_LEN - snprintf_wr_idx, "f-reg[%.2x]:0x%.2x,", addr, val);
                        else
				pr_err("masterIC reg_info:Array idx out of range!snprintf_wr_idx:%d;f-REG[%.2x] = 0x%.2x \n ", snprintf_wr_idx, addr, val);

                }

        }

	pr_err("masterIC reg_info:%s\n", reg_dump_str);

        return 0;
}

 int charger_ex_dump_register(void)
{
	u8 addr = 0;
	u8 val = 0 ;
	int ret = 0;

	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = SYV690_read_byte(g_chg_info, addr, &val);
		if (!ret)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
	return 0;
}

/*
		charger_ops

*/
static int SYV690_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	*en = chgInfo->charge_enabled;
	return 0;
}
/*
		charger_ops

*/
static int SYV690_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val = 0;
	int ret = 0;

	ret = SYV690_field_read(chgInfo, F_ICHG);
	if(ret >= 0){
		reg_val = ret;
		if (chgInfo->chip_id == SYV690_ID || chgInfo->chip_id == BQ25890H_ID)
			*curr  = reg_val * SYV690_ICHG_STEP*1000;
		else if (chgInfo->chip_id == SC89890H_ID)
			*curr  = reg_val * SC89890H_ICHG_STEP*1000;
		else
			*curr  = reg_val * SYV690_ICHG_STEP*1000;
	}else{
		pr_err("Failed to get ichg!\n");
	}
	return ret;
}
/*
		charger_ops

*/
static int SYV690_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 reg_val = 0;
	if (chgInfo->chip_id == SYV690_ID || chgInfo->chip_id == BQ25890H_ID)
		reg_val = (curr/1000)/SYV690_ICHG_STEP;
	else if (chgInfo->chip_id == SC89890H_ID)
		reg_val = (curr/1000)/SC89890H_ICHG_STEP;
	else
		reg_val = (curr/1000)/SYV690_ICHG_STEP;
	pr_err("Config charging Current  = %d uA\n", curr);
	ret = SYV690_field_write(chgInfo, F_ICHG, reg_val);
	pr_err(" SYV690_set_ichg  write :%s\n",  ret < 0 ? "failed" : "successfully");
	return ret;
}
/*
		charger_ops

*/

static int SYV690_get_aicr(struct charger_device *chg_dev, u32 *curr)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val = 0;
	int ret = 0;
	
	ret = SYV690_field_read(chgInfo, F_IILIM);
	if(ret >= 0){
		reg_val = ret;
		*curr  = (reg_val * 50 + 100)*1000;
		pr_err("Config charging Current  = %d uA\n", *curr);
	}else{
		pr_err("Failed to get aicr!\n");
	}
	return ret;
}
/*
		charger_ops

*/
static int SYV690_set_aicr(struct charger_device *chg_dev, u32 curr)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val = 0;
	/*
			curr -- ua ; SYV690_AICR_OFFSET --ma
	*/
	val = (curr/1000  - SYV690_AICR_OFFSET)/SYV690_AICR_STEP;
	pr_err("Config Input Current  = %d uA\n", curr);
	ret = SYV690_field_write(chgInfo, F_IILIM, val);
	pr_err(" SYV690_set_icl  write :%s\n",  ret < 0 ? "failed" : "successfully");
	return ret;
}
/*
		charger_ops

*/
static int SYV690_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val = 0;
	int ret = 0;

	ret = SYV690_field_read(chgInfo, F_VREG);
	if(ret >= 0){
		reg_val = ret;
		*volt  = (reg_val * SYV690_VREG_STEP + SYV690_VREG_OFFSET)*1000;
		pr_err("SYV690_get_vchg  = %d mV\n", (*volt)/1000);
	}else{
		pr_err("Failed to get vchg!\n");
	}
	return ret;
}
/*
		charger_ops

*/
static int SYV690_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 reg_val = (volt/1000 - SYV690_VREG_OFFSET)/SYV690_VREG_STEP;
	pr_err("SYV690_set_vchg  = %d mV\n", volt/1000);

	ret = SYV690_field_write(chgInfo, F_VREG, reg_val);
	pr_err(" SYV690_set_vchg  write :%s\n",  ret < 0 ? "failed" : "successfully");

	return ret;
}
/*
		charger_ops

*/
static int SYV690_kick_wdt(struct charger_device *chg_dev)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);

	return SYV690_reset_watchdog_timer(chgInfo);
}
/*
		charger_ops

*/
static int SYV690_set_mivr(struct charger_device *chg_dev, u32 volt)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val = 0;
	int mVolt = volt/1000;
	pr_err("SYV690_set_ivl volt = %d mV\n",mVolt);
	/* offset 2600 ,step 100mv*/
       if(mVolt < SYV690_MIVR_OFFSET)
	   	mVolt = SYV690_MIVR_OFFSET;
	val = (mVolt - SYV690_MIVR_OFFSET)/SYV690_MIVR_STEP;
	ret = SYV690_field_write(chgInfo, F_VINDPM, val);
	pr_err(" SYV690_set_ivl  write :%s\n",  ret < 0 ? "failed" : "successfully");
	return ret;

}
/*
		charger_ops

*/
static int SYV690_get_mivr(struct charger_device *chg_dev, u32 *mivr)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val = 0;
	int ret = 0;
	ret = SYV690_field_read(chgInfo, F_VINDPM);
	if(ret >= 0){
		reg_val = ret;
		*mivr  = (reg_val *SYV690_MIVR_STEP + SYV690_MIVR_OFFSET)*1000;
		pr_err("SYV690_get_vchg  = %d mV\n", (*mivr)/1000);
	}else{
		pr_err("Failed to get mivr!\n");
	}
	return ret;

}

/*
		charger_ops

*/
static int SYV690_get_mivr_state(struct charger_device *chg_dev, bool *in_loop)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	
	ret = SYV690_field_read(chgInfo, F_VDPM_STAT);
	if(ret >= 0){
		*in_loop  = ret;
		pr_err("SYV690_get_mivr_state  = %d \n",ret);
	}else{
		pr_err("Failed to get mivr state!\n");
	}
	return ret;
}
/*
		charger_ops

*/
static int SYV690_set_ieoc(struct charger_device *chg_dev, u32 curr)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);

	pr_err("SYV690_set_ieoc curr = %d ma\n", curr/1000);

	return SYV690_set_term_current(chgInfo, curr / 1000);
}
/*
		charger_ops

*/
static int SYV690_enable_te(struct charger_device *chg_dev, bool en)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);

	pr_err("SYV690_enable_te  = %d\n", en);

	return SYV690_enable_term(chgInfo, en);
}
/*
		charger_ops

*/
static int SYV690_safety_check(struct charger_device *chg_dev, u32 polling_ieoc)
{
	int adc_ibat = 0;
	static int counter;
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);

	adc_ibat = SYV690_adc_read_charge_current(chgInfo);

	pr_err("%s: polling_ieoc = %d, ibat = %d\n",__func__, polling_ieoc, adc_ibat);

	if (adc_ibat <= polling_ieoc)
		counter++;
	else
		counter = 0;

	/* If IBAT is less than polling_ieoc for 3 times, trigger EOC event */
	if (counter == 3) {
		pr_err("%s: polling_ieoc = %d, ibat = %d\n",__func__, polling_ieoc, adc_ibat);
		/*
			notify battery full
		*/
		charger_dev_notify(chgInfo->chg_dev, CHARGER_DEV_NOTIFY_EOC);
		counter = 0;
	}

	return 0;
}
/*
		charger_ops

*/
static int SYV690_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
	*uA = 100000;
	return 0;
}
/*
		charger_ops

*/
static int SYV690_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	int ret = 0,hvdcp_en,dpdm_done,retry;
	u32 constant_volt = 0;
	unsigned char data=0;

	ret = SYV690_field_read(chgInfo, F_CHG_STAT);
	if(ret >= 0){
		/*
			0 not charging
			1 pre charging
			2 fast charging
			3 charging termination done
		*/
		if (ret == 3) {
			SYV690_get_vchg(chg_dev, &constant_volt);
			if (constant_volt == 4080000) {
				ret = 2;
			}
		}
		*done  = (ret == 3);
		pr_err("SYV690_is_charging_done  = %d \n",ret);
		if ((ret == 3) && (battery_get_vbus() > 7800)) {
			SYV690_field_write(chgInfo, F_VINDPM, 0x14);
			if (g_chg_info->chg_type == HVDCP_CHARGER) {
				__SYV690_read_reg(chgInfo,SYV690_REG_CON2,&data);
				data &= (CON2_HVDCP_EN_MASK << CON2_HVDCP_EN_SHIFT );
				hvdcp_en = (data >> CON2_HVDCP_EN_SHIFT );
				if (hvdcp_en == 1) {
					SYV690_update_bits(chgInfo,SYV690_REG_CON2, 0, CON2_HVDCP_EN_MASK, CON2_HVDCP_EN_SHIFT );
                          		msleep(10);
                            		pr_err("%s close hvdcp\n",  __func__);
				}
				gpio_set_value(g_chg_info->usb_switch_cb1_gpio, 1);
				gpio_set_value(g_chg_info->usb_switch_cb2_gpio, 1);
				SYV690_enable_force_dpdm(g_chg_info,true);
				for(retry = 0;retry < 20 ; retry++) {
					SYV690_is_dpdm_done(g_chg_info,&dpdm_done);
					msleep(100);
					if(!dpdm_done)
					break;
				}
				pr_err("%s charger battery full vbus set 5V, dpdm_done:%d, retry:%d\n",  __func__, dpdm_done, retry);
			}
			if(charger_manager_pd_is_online()) {
				adapter_set_cap(5000,2000);
				pr_err("%s PD charger battery full vbus set 5V\n",  __func__);
			}
		}
	}else{
		pr_err("Failed to get charging_done state!\n");
	}
	return ret;
}
/*
		charger_ops

*/
static int SYV690_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 60 * 1000;
	return 0;
}
static int  SYV690_enable_HZ(struct charger_device *chg_dev, bool en)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	pr_err("%s en = %d\n", __func__, en);
	return SYV690_enter_hiz_mode(chgInfo , en);

}
/*
		charger_ops

*/
static int SYV690_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	return SYV690_process_safety_timer(chgInfo , en);
}
/*
		charger_ops

*/
static int SYV690_is_safety_timer_enabled(struct charger_device *chg_dev, bool *en)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	int ret;

	ret = SYV690_field_read(chgInfo, F_TMR_EN);
	if(ret >= 0){
		/* 0 disable 1 enable*/
		*en  = ret;
		pr_err("SYV690_is_safety_timer_enabled  = %d \n",ret);
	}else{
		pr_err("Failed to get _safety_timer_enabled state!\n");
	}
	return ret;

}
/*
		charger_ops

*/
static int SYV690_enable_powerpath(struct charger_device *chg_dev, bool en)
{
#if 0
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	pr_err("%s en = %d\n", __func__, en);
	return SYV690_enter_hiz_mode(chgInfo , en);
#else
	return 0;
#endif
}
/*
		charger_ops

*/
void chg_enable_powerpath(bool en)
{	
	u8 addr = 0;
	u8 val = 0 ;
	int ret = 0;
	pr_err("%s en = %d\n", __func__, en);
	SYV690_enter_hiz_mode(g_chg_info , en);
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = SYV690_read_byte(g_chg_info, addr, &val);
		if (!ret)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
}
/*
		charger_ops

*/
static int SYV690_is_powerpath_enabled(struct charger_device *chg_dev, bool *en)
{
#if 0
	int ret = 0;
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);

	ret = SYV690_field_read(chgInfo, F_EN_HIZ);
	if(ret >= 0){
		/* 0 disable 1 enable*/
		*en  = ret;
		pr_err("SYV690_is_powerpath_enabled  = %d \n",ret);
	}else{
		pr_err("Failed to get powerpath_enable state!\n");
	}
	return ret;
#else
	return 0;
#endif
}

/*
		charger_ops

*/
#if 0
static  int SYV690_enable_auto_dpdm(struct SYV690_device *chgInfo , bool en)
{
	return SYV690_update_bits(chgInfo ,SYV690_REG_CON2, (en ? 1 : 0), CON2_AUTO_DPDM_EN_MASK,CON2_AUTO_DPDM_EN_SHIFT);
}
#endif
static  int SYV690_handle_hvdcp(struct SYV690_device *chgInfo , bool en)
{
	return SYV690_update_bits(chgInfo,SYV690_REG_CON2, (en ? 1 : 0), CON2_HVDCP_EN_MASK,CON2_HVDCP_EN_SHIFT);
}
static  int SYV690_enable_force_dpdm(struct SYV690_device *chgInfo,bool en)
{
	int ret = 0;

	if (chgInfo->chip_id == SC89890H_ID) {
	    SYV690_write_byte(chgInfo, SYV690_REG_CON1, 0x45);
	    msleep(30);
	    SYV690_write_byte(chgInfo, SYV690_REG_CON1, 0x25);
	    msleep(30);
	}

	SYV690_update_bits(chgInfo,SYV690_REG_CON2, (en ? 1 : 0), CON2_FORCE_DPDM_MASK,CON2_FORCE_DPDM_SHIFT);	
	return ret;
}

static  int SYV690_is_dpdm_done(struct SYV690_device *chgInfo,int *done)
{
	int ret = 0;
	unsigned char data=0;
	ret = __SYV690_read_reg(chgInfo,SYV690_REG_CON2,&data);
	pr_err("%s data(0x%x)\n",  __func__, data);
	data &= (CON2_FORCE_DPDM_MASK << CON2_FORCE_DPDM_SHIFT);
	*done = (data >> CON2_FORCE_DPDM_SHIFT);
	 return ret;
}
#if 0
static  bool SYV690_is_hvdcp(struct SYV690_device *chgInfo)
{
#if 0
	int ret = 0;
	ret = SYV690_field_read(chgInfo, F_VBUS_STAT);
	if(ret >= 0){
		/* 0 disable 1 enable*/
		pr_err("SYV690_is_hvdcp  = %d \n",ret);
		return ret == SYV690_CHG_TYPE_HVDCP ? true:false; //4 //4 SYV690_PORTSTAT_HVDCP
	}else{
		pr_err("Failed to SYV690_is_hvdcp ERROR!\n");
		return false;
	}
#else
	unsigned char port_stat = 0;
	 __SYV690_read_reg(chgInfo,SYV690_REG_CONB, &port_stat);
	port_stat &= (CONB_VBUS_STAT_MASK << CONB_VBUS_STAT_SHIFT);
	port_stat = (port_stat >> CONB_VBUS_STAT_SHIFT);
	return (port_stat == SYV690_CHG_TYPE_HVDCP? true : false) ;
#endif
}
#endif
static unsigned char SYV690_check_charger_type(struct SYV690_device *chgInfo  )
{

	unsigned char port_stat = 0;
	 __SYV690_read_reg(chgInfo , SYV690_REG_CONB, &port_stat);
	port_stat &= (CONB_VBUS_STAT_MASK << CONB_VBUS_STAT_SHIFT);
	port_stat = (port_stat >> CONB_VBUS_STAT_SHIFT);
	return port_stat;
}

static int SYV690_get_charger_type(struct SYV690_device *chgInfo)
{
	int done = 1;
	int retry = 0;
	int bc_count = 5;
	u8 chg_type = 0;
	int vbus_volt = 0;


	if (chgInfo->chip_id == SYV690_ID || chgInfo->chip_id == BQ25890H_ID) {
		bc_count = 20;
	} else if (chgInfo->chip_id == SC89890H_ID) {
		bc_count = 8;

	}

	if (chgInfo->chip_id == SC89890H_ID || chgInfo->chip_id == BQ25890H_ID) {
		SYV690_handle_hvdcp(chgInfo, false);
	} else {
		SYV690_handle_hvdcp(chgInfo,1);
	}
	SYV690_enable_force_dpdm(chgInfo,true);

	for(retry = 0;retry < bc_count ; retry++){
		SYV690_is_dpdm_done(chgInfo,&done);
		msleep(10);
		if(!done)
			break;
	}

	pr_err("%s done : %d ,retry = %d \n",  __func__, done,retry);

	for(retry = 0;retry < bc_count ; retry++){
		chg_type = SYV690_check_charger_type(chgInfo);
		msleep(500);
		if(chg_type)
			break;
	}

	vbus_volt = battery_get_vbus();
	if (vbus_volt < 4100 && chgInfo->attach == false) {
		chgInfo->chg_type = CHARGER_UNKNOWN;
		pr_err("%s vbus_volt(%d) < 4100, not attach, main_chg_type = %d.\n",  __func__, vbus_volt, chgInfo->chg_type);
		return chgInfo->chg_type;
	} else if (vbus_volt > 4800 && chgInfo->attach == true && chg_type == SYV690_CHG_TYPE_NONE) {
		chg_type = SYV690_check_charger_type(chgInfo);
		if (chg_type == SYV690_CHG_TYPE_NONE && g_chg_info && g_chg_info->chg_type) {
			msleep(50);
			chg_type = SYV690_check_charger_type(chgInfo);
		}
		pr_err("%s chg_type = %d.\n",  __func__, chg_type);
	}

	switch (chg_type) {
	case SYV690_CHG_TYPE_NONE:
		pr_err("%s: CHARGER_UNKNOWN !!!\n", __func__);
		chgInfo->chg_type = CHARGER_UNKNOWN;
		break;
	case SYV690_CHG_TYPE_SDP:
		chgInfo->chg_type = STANDARD_HOST;
		break;
	case SYV690_CHG_TYPE_CDP:
		chgInfo->chg_type = CHARGING_HOST;
		break;
	case SYV690_CHG_TYPE_HVDCP:
			chgInfo->chg_type = HVDCP_CHARGER;
		break;
	case SYV690_CHG_TYPE_DCP:
		chgInfo->chg_type = STANDARD_CHARGER;
		break;
	default:
		chgInfo->chg_type = NONSTANDARD_CHARGER;
		break;
	}

	pr_err("%s: main_chg_type = %d ,retry = %d !!!\n", __func__, chgInfo->chg_type,retry);
	return chgInfo->chg_type;
}
/*
		charger_ops

*/
static int SYV690_psy_online_changed(struct SYV690_device *chgInfo)
{
	int ret = 0;
	union power_supply_propval propval;

	/* Get chg type det power supply */
	if (!chgInfo->charger)
		chgInfo->charger = power_supply_get_by_name("charger");
	if (!chgInfo->charger) {
		pr_err("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}

	propval.intval = chgInfo->attach;
	ret = power_supply_set_property(chgInfo->charger, POWER_SUPPLY_PROP_ONLINE,	&propval);
	if (ret < 0)
		pr_err("%s: psy online fail(%d)\n", __func__, ret);
	else
		pr_err("%s: pwr_rdy = %d\n",  __func__, chgInfo->attach);
	return ret;
}
/*
		charger_ops

*/
static int SYV690_psy_chg_type_changed(struct SYV690_device *chgInfo)
{
	int ret = 0;
	union power_supply_propval propval;

	/* Get chg type det power supply */
	if (!chgInfo->charger)
		chgInfo->charger = power_supply_get_by_name("charger");
	if (!chgInfo->charger) {
		pr_err("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}

	propval.intval = chgInfo->chg_type;
	ret = power_supply_set_property(chgInfo->charger,POWER_SUPPLY_PROP_CHARGE_TYPE,&propval);
	if (ret < 0)
		pr_err("%s: psy type failed, ret = %d\n", __func__, ret);
	else
		pr_err("%s: chg_type = %d\n", __func__, chgInfo->chg_type);
	return ret;
}

static int pmic_set_usbsw_state(struct SYV690_device *chgInfo, int state)
{
	pr_err("%s: state = %d\n", __func__, state);

	/* Switch D+D- to AP/SYV690 */
	if (state == SYV690_USBSW_CHG)
		Charger_Detect_Init();
	else
		Charger_Detect_Release();

	return 0;
}
static int __SYV690_enable_usbchgen(struct SYV690_device *chgInfo, bool en)
{
	int i, ret = 0;
	int max_wait_cnt = 10;
	enum SYV690_usbsw_state usbsw = en ? SYV690_USBSW_CHG : SYV690_USBSW_USB;

	pr_err("%s: en = %d\n", __func__, en);
	if (en) {
		if (chgInfo->lk_type_is_cdp && chgInfo->kpoc) {
			chgInfo->lk_type_is_cdp = false;
			chgInfo->kpoc = false;
			max_wait_cnt = 70;
			pr_err("%s: CDP off charging mode.\n", __func__);
		}
		/* Workaround for CDP port */
		for (i = 0; i < max_wait_cnt; i++) {
			if (is_usb_rdy())
				break;
			pr_err("%s: CDP block\n", __func__);
			if (!(chgInfo->tcpc_attach)) {
				pr_info("%s: plug out, not handle usb_switch\n", __func__);
				return 0;
			}
			msleep(100);
		}
		if (i == max_wait_cnt)
			pr_err("%s: CDP timeout\n", __func__);
		else
			pr_err("%s: CDP free\n", __func__);
	}

	pmic_set_usbsw_state(chgInfo, usbsw);
	chgInfo->bc12_en = en;
	return ret;
}
#if 0
static int SYV690_enable_usbchgen(struct SYV690_device *chgInfo, bool en)
{
	int ret = 0;
	mutex_lock(&chgInfo->chgdet_lock);
	ret = __SYV690_enable_usbchgen(chgInfo, en);
	mutex_unlock(&chgInfo->chgdet_lock);
	return ret;
}
#endif
static int SYV690_chgdet_pre_process(struct SYV690_device *chgInfo)
{
	int ret = 0;
	bool attach = false;

#ifdef CONFIG_TCPC_CLASS
	attach = chgInfo->tcpc_attach;
#endif /* CONFIG_TCPC_CLASS */

	//if (attach && is_meta_mode()) {
    if (0) {
		/* Skip charger type detection to speed up meta boot.*/
		pr_err("%s: force Standard USB Host in meta\n", __func__);
		chgInfo->attach = attach;
		chgInfo->chg_type = STANDARD_HOST;
		ret = SYV690_psy_online_changed(chgInfo);
		if (ret < 0)
			pr_err("%s: set psy online fail\n", __func__);
		return SYV690_psy_chg_type_changed(chgInfo);
	} else if (attach) {
		pr_err("%s: typec attach :%d\n", __func__,attach);
		if ( chgInfo->need_retry_det) {//is_pd_active() &&
			pr_err("force charger type: STANDARD_HOST\n");
			chgInfo->attach = attach;
			chgInfo->chg_type = STANDARD_HOST;
			ret = SYV690_psy_online_changed(chgInfo);
			if (ret < 0)
				pr_err("%s: set psy online fail\n", __func__);
			return SYV690_psy_chg_type_changed(chgInfo);
		} else if (chgInfo->ignore_usb) {
			/* Skip charger type detection for pr_swap */
			  pr_err("charger type: force Standard USB Host for pr_swap\n");
                        chgInfo->attach = attach;
                        chgInfo->chg_type = STANDARD_HOST;
                        ret = SYV690_psy_online_changed(chgInfo);
                        if (ret < 0)
                               	pr_err("%s: set psy online fail\n", __func__);
                        return SYV690_psy_chg_type_changed(chgInfo);
		}
	}

	return __SYV690_enable_usbchgen(chgInfo, attach);	
}
/*
		charger_ops

*/

static int sec_det_cnt = 0;
static int SYV690_chgdet_post_process(struct SYV690_device *chgInfo)
{
	int ret = 0;
	bool attach = false, inform_psy = true;

#ifdef CONFIG_TCPC_CLASS
	attach = chgInfo->tcpc_attach;
	pr_err("first:attach(%d) , chgInfo->attach:%d chgInfo->chg_type:%d \n", attach, chgInfo->attach, chgInfo->chg_type);
#endif /* CONFIG_TCPC_CLASS */
	
	if(false == attach  ){ /* plug out charger! */
		pr_err("remove charger,reset chg type and gpio status...!attach:%d \n", attach);	
		chgInfo->chg_type = CHARGER_UNKNOWN;
		sec_det_cnt = 0;
		det_cnt = 0;

	}else if(chgInfo->chg_type != NONSTANDARD_CHARGER && chgInfo->chg_type != CHARGER_UNKNOWN ) {/* if (chgInfo->attach == attach) { */
		chgInfo->attach = attach;
		pr_err("%s: attach(%d) is the same\n", __func__, attach);
		inform_psy = !attach;
		if(!attach){
			chgInfo->chg_type = CHARGER_UNKNOWN;
			sec_det_cnt = 0;
		}
		goto out;
	}
	chgInfo->attach = attach;
	pr_err("%s: attach = %d\n", __func__, attach);
	/* Plug out during BC12 */
	if (!attach) {
		gpio_set_value(chgInfo->usb_switch_cb1_gpio, 0);
		gpio_set_value(chgInfo->usb_switch_cb2_gpio, 0);
		chgInfo->chg_type = CHARGER_UNKNOWN;
		sec_det_cnt = 0;
		goto out;
	}
	pinctrl_gpio_direction_output(chgInfo->usb_switch_cb1_gpio);
	pinctrl_gpio_direction_output(chgInfo->usb_switch_cb2_gpio);
	//pinctrl_select_state(chgInfo->pinctrl , chgInfo->pin1_active);
	//pinctrl_select_state(chgInfo->pinctrl , chgInfo->pin2_active);
	//logic value cb1 1 + cb2 1 => no1 + no2
	//CHG
	gpio_set_value(chgInfo->usb_switch_cb1_gpio, 1);
	gpio_set_value(chgInfo->usb_switch_cb2_gpio, 1);
	pr_err("%s:Before BC1.2  cb1_gpio = %d ,cb2_gpio = %d\n", __func__, gpio_get_value(chgInfo->usb_switch_cb1_gpio),gpio_get_value(chgInfo->usb_switch_cb2_gpio));
	/* Plug in */
	SYV690_get_charger_type(chgInfo);
	pre_current = 0;
	if ((chgInfo->chg_type == NONSTANDARD_CHARGER)
		 || ((chgInfo->chg_type == CHARGER_UNKNOWN) && (true == SYV690_field_read(chgInfo, F_VBUS_GD)))) {
		if(!charger_manager_pd_is_online()) {
			pr_err("%s is nonstd, retry bc.\n", __func__);
			schedule_delayed_work(&chgInfo->charger_secdet_work, msecs_to_jiffies(CHARGER_SEC_WORK_DELAY));
		}
	}
	if (chgInfo->chg_type == STANDARD_CHARGER || chgInfo->chg_type == HVDCP_CHARGER) {
		schedule_delayed_work(&g_chg_info->hvdcp_det_work, msecs_to_jiffies(0));
	}
	if((chgInfo->chg_type == CHARGING_HOST)||(chgInfo->chg_type == STANDARD_HOST)){
		//AP
		gpio_set_value(chgInfo->usb_switch_cb1_gpio, 0);
		gpio_set_value(chgInfo->usb_switch_cb2_gpio, 0);
	}
	pr_err("%s:After BC1.2  cb1_gpio = %d ,cb2_gpio = %d\n", __func__, gpio_get_value(chgInfo->usb_switch_cb1_gpio),gpio_get_value(chgInfo->usb_switch_cb2_gpio));
	/*
			how about NONSTANDARD_CHARGER ? start second check?
	*/
out:
	if (!attach) {
		ret = __SYV690_enable_usbchgen(chgInfo, false);
		if (ret < 0)
			pr_err("%s: disable chgdet fail\n", __func__);
		SYV690_set_term_current(chgInfo, SYV690_TERM_CUR);
	} else if (chgInfo->chg_type != STANDARD_CHARGER){
		pmic_set_usbsw_state(chgInfo, SYV690_USBSW_USB);
	}

	if (!inform_psy)
		return ret;
	ret = SYV690_psy_online_changed(chgInfo);
	if (ret < 0)
		pr_err("%s: report psy online fail\n", __func__);
	SYV690_psy_chg_type_changed(chgInfo);
	if (chgInfo->chip_id == SC89890H_ID || chgInfo->chip_id == BQ25890H_ID) {
		if((chgInfo->chg_type != CHARGING_HOST) && (chgInfo->chg_type != STANDARD_HOST)) {
			SYV690_handle_hvdcp(chgInfo,1);
			SYV690_enable_force_dpdm(chgInfo,true);
		}
	}

	return 0;
}

/*
		charger_ops

*/
static int SYV690_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
#ifdef CONFIG_TCPC_CLASS
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	static int idx = 0;
	pr_err("%s en : %d\n", __func__, en);
	chgInfo->chg_det_enable = en;

	mutex_lock(&chgInfo->chgdet_lock);

	if(chgInfo->tcpc_attach == en) {
		if (chgInfo->tcpc_attach == true && (chgInfo->chg_type == CHARGER_UNKNOWN)) {
			pr_err("%s attach(%d) is the same, retry bc1.2\n", __func__, chgInfo->attach);
		} else {
			pr_err("%s attach(%d) is the same\n", __func__, chgInfo->attach);
			goto out;
		}
	}
	chgInfo->tcpc_attach = en;
#if 1
	if(en){
		SYV690_chgdet_pre_process(chgInfo);
		idx++;
		pr_err("checkNUM:insert charger idx:%d \n",idx);
	}else{
		idx=0;
		pr_err("checkNUM:remove charger idx:%d \n",idx);		
	}
	SYV690_chgdet_post_process(chgInfo);
#else
	ret = (en? SYV690_chgdet_pre_process :SYV690_chgdet_post_process)(chgInfo);
#endif
out:
	mutex_unlock(&chgInfo->chgdet_lock);
#endif /* CONFIG_TCPC_CLASS */
	return ret;
}
/*
		charger_ops

*/
static int SYV690_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);

#ifdef FACTORY_BUILD
	if (en) {
		SYV690_enter_hiz_mode(chgInfo, 0);
	}
#endif
	if (en && (chgInfo->chip_id == SC89890H_ID)) {
		ret = SYV690_field_write(chgInfo, F_CHG_CFG, 0);
	}
	gpio_set_value(chgInfo->otg_en_gpio, en);
	ret = SYV690_field_write(chgInfo, F_OTG_CFG, en);
	pr_err(" SYV690_set_otg  write :%s , otg_en = %d\n",  ret < 0 ? "failed" : "successfully",gpio_get_value(chgInfo->otg_en_gpio));
	return ret;
}
/*
		charger_ops

*/
static const u32 otg_oc_table[] = {
	500000, 750000, 1200000, 1400000, 1650000, 1875000, 2150000, 2450000
};
#define SYV690_OTG_OC_MAXVAL	(0x07)
static int SYV690_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	int ret = 0,i=0;
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);

	pr_err("SYV690_set_boost_ilmt curr = %d ua!\n", curr);
	for (i = 0; i < ARRAY_SIZE(otg_oc_table); i++) {
		if (curr <= otg_oc_table[i])
			break;
	}
	if (i == ARRAY_SIZE(otg_oc_table))
		i = SYV690_OTG_OC_MAXVAL;
	pr_err("%s: select oc threshold = %d\n", __func__, otg_oc_table[i]);

	ret = SYV690_field_write(chgInfo, F_BOOSTI, i);
	pr_err(" SYV690_set_boost_ilmt  write :%s\n",  ret < 0 ? "failed" : "successfully");
	SYV690_dump_register(chg_dev);
	return ret;
}
/*
		charger_ops

*/

static int SYV690_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	pr_err("%s: notify event = %d\n", __func__, event);
	switch (event) {
	case EVENT_EOC:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}
	return 0;
}

static void sc8989h_set_9v(void);
/*
static int SYV690_enable_hvdcp(struct charger_device *chg_dev, bool en)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);

	if (chgInfo == NULL) {
		pr_err("%s fail\n", __func__);
		return 0;
	}
	pr_info("%s enable hvdcp:%d\n", __func__, en);
	if (en) {
		schedule_delayed_work(&chgInfo->hardreset_hvdcp_work, msecs_to_jiffies(1000));
	} else {
		SYV690_handle_hvdcp(chgInfo, en);
		SYV690_enable_force_dpdm(chgInfo, true);
	}

	return 0;
}
*/
static int SYV690_get_ibus(struct charger_device *chg_dev, u32 *ibus)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val = 0;
	int ret = 0;

	ret = SYV690_field_read(chgInfo, F_ICHGR);
	if(ret >= 0){
		reg_val = ret;
		*ibus  = (reg_val * SYV690_ICHGR_STEP)*1000;
		pr_err("SYV690_get_ibus Current  = %d uA\n", *ibus);
	}else{
		pr_err("Failed to get IBUS!\n");
	}
	return ret;

}
static int SYV690_get_vbus(struct charger_device *chg_dev, u32 *vbus)
{
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val = 0;
	int ret = 0;

	ret = SYV690_field_read(chgInfo, F_VBUSV);
	if(ret >= 0){
		reg_val = ret;
		*vbus  = (reg_val * SYV690_VBUS_STEP + SYV690_VBUS_OFFSET)*1000;
		pr_err("SYV690_get_vbus  = %d uv\n", *vbus);
	}else{
		pr_err("Failed to get IBUS!\n");
	}
	return ret;
}

/*
static int SYV690_get_vbat(struct charger_device *chg_dev,u32 *vbat)
{
	u8 reg_val = 0;
	int ret = 0;
	struct SYV690_device *chgInfo = dev_get_drvdata(&chg_dev->dev);
	ret = SYV690_field_read(chgInfo, F_BATV);
	if(ret >= 0){
		reg_val = ret;
		*vbat  = (reg_val * SYV690_VBAT_STEP + SYV690_VBAT_OFFSET - SYV690_VBAT_CALIBRATION)*1000;
		pr_err("SYV690_get_vbat  = %d uv\n", *vbat);
	}else{
		pr_err("Failed to get VBAT!\n");
	}
	return ret;
}
*/
/*
===========================================================
===========================================================

*/
static struct charger_ops SYV690_chg_ops = {
	/* Normal charging */
	.plug_in = SYV690_plug_in,
	.plug_out = SYV690_plug_out,
	.dump_registers = SYV690_dump_register,
	.enable = SYV690_charging,
	.is_enabled = SYV690_is_charging_enable,
	.get_charging_current = SYV690_get_ichg,
	.set_charging_current = SYV690_set_ichg,
	.get_input_current = SYV690_get_aicr,
	.set_input_current = SYV690_set_input_current,
	.get_constant_voltage = SYV690_get_vchg,
	.set_constant_voltage = SYV690_set_vchg,
	.kick_wdt = SYV690_kick_wdt,
	.set_mivr = SYV690_set_mivr,
	.get_mivr = SYV690_get_mivr,
	.get_mivr_state = SYV690_get_mivr_state,
	.set_eoc_current = SYV690_set_ieoc,
	.enable_termination = SYV690_enable_te,
	.safety_check = SYV690_safety_check,
	.get_min_input_current = SYV690_get_min_aicr,
	.is_charging_done = SYV690_is_charging_done,
	.get_min_charging_current = SYV690_get_min_ichg,
	.enable_hz	=  SYV690_enable_HZ,

	/* Safety timer */
	.enable_safety_timer = SYV690_set_safety_timer,
	.is_safety_timer_enabled = SYV690_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = SYV690_enable_powerpath,
	.is_powerpath_enabled = SYV690_is_powerpath_enabled,

	/* Charger type detection */
	.enable_chg_type_det = SYV690_enable_chg_type_det,

	/* OTG */
	.enable_otg = SYV690_set_otg,
	.set_boost_current_limit = SYV690_set_boost_ilmt,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
	.get_ibus_adc = SYV690_get_ibus,
	.get_vbus_adc = SYV690_get_vbus,
	//.get_vbat_adc = SYV690_get_vbat,
	/* Event */
	.event =  SYV690_do_event,

	//.set_hvdcp_enable = SYV690_enable_hvdcp,
};
static const struct charger_properties SYV690_chg_props = {
	.alias_name = "syv690",
};

static int SYV690_irq_probe(struct SYV690_device *bq)
{
	int ret = 0;
	ret = devm_gpio_request(bq->dev, bq->irq_gpio, "syv690-irq");
	if (ret < 0) {
		pr_err("Error: failed to request GPIO%d (ret = %d)\n",bq->irq_gpio, ret);
		return -EINVAL;
	}
	ret = gpio_direction_input(bq->irq_gpio);
	if (ret < 0) {
		pr_err("Error: failed to set GPIO%d as input pin(ret = %d)\n",
		bq->irq_gpio, ret);
		return -EINVAL;
	}

	bq->irq = gpio_to_irq(bq->irq_gpio);
	if (bq->irq <= 0) {
		pr_err("%s gpio to irq fail, bq->irq(%d)\n",
						__func__, bq->irq);
		return -EINVAL;
	}

	pr_info("%s : IRQ number = %d\n", __func__, bq->irq);
	return bq->irq;
}


static int SYV690_parse_dt(struct device_node *np,   struct SYV690_device *chgInfo)
{
	int ret;
	struct SYV690_init_data *pdata = &chgInfo->init_data;

	ret = of_get_named_gpio(np,"usb_switch_cb1_gpio", 0);
	if (ret < 0) {
		pr_err("%s, could not get usb_switch_cb1_gpio\n", __func__);
		return ret;
	} else {
		chgInfo->usb_switch_cb1_gpio = ret;
		pr_info("%s, chgInfo->usb_switch_cb1_gpio: %d\n", __func__, chgInfo->usb_switch_cb1_gpio);
		ret = gpio_request(chgInfo->usb_switch_cb1_gpio, "GPIO158");
	}

	ret = of_get_named_gpio(np,"usb_switch_cb2_gpio", 0);
	if (ret < 0) {
		pr_err("%s, could not get usb_switch_cb2_gpio\n", __func__);
		return ret;
	} else {
		chgInfo->usb_switch_cb2_gpio = ret;
		pr_info("%s, chgInfo->usb_switch_cb2_gpio: %d\n", __func__, chgInfo->usb_switch_cb2_gpio);
		ret = gpio_request(chgInfo->usb_switch_cb2_gpio, "GPIO159");
	}

	ret = of_get_named_gpio(np,"otg_en_gpio", 0);
	if (ret < 0) {
		pr_err("%s, could not get usb_switch_cb2_gpio\n", __func__);
		return ret;
	} else {
		chgInfo->otg_en_gpio = ret;
		pr_info("%s, chgInfo->otg_en_gpio: %d\n", __func__, chgInfo->otg_en_gpio);
		ret = gpio_request(chgInfo->otg_en_gpio, "GPIO151");
	}
	
	chgInfo->pinctrl = devm_pinctrl_get(chgInfo->dev);
	if (IS_ERR_OR_NULL(chgInfo->pinctrl)) {
		pr_err("%s, No pinctrl config specified\n", __func__);
		return -EINVAL;
	}

	chgInfo->pin1_active = pinctrl_lookup_state(chgInfo->pinctrl, "active_cb1");
	if (IS_ERR_OR_NULL(chgInfo->pin1_active)) {
		pr_err("%s, could not get pin_active\n", __func__);
		return -EINVAL;
	}

	chgInfo->pin1_suspend = pinctrl_lookup_state(chgInfo->pinctrl, "sleep_cb1");
	if (IS_ERR_OR_NULL(chgInfo->pin1_suspend)) {
		pr_err("%s, could not get pin_suspend\n", __func__);
		return -EINVAL;
	}

	chgInfo->pin2_active = pinctrl_lookup_state(chgInfo->pinctrl, "active_cb2");
	if (IS_ERR_OR_NULL(chgInfo->pin2_active)) {
		pr_err("%s, could not get pin_active\n", __func__);
		return -EINVAL;
	}

	chgInfo->pin2_suspend = pinctrl_lookup_state(chgInfo->pinctrl, "sleep_cb2");
	if (IS_ERR_OR_NULL(chgInfo->pin2_suspend)) {
		pr_err("%s, could not get pin_suspend\n", __func__);
		return -EINVAL;
	}
	//switch to default ap dadm
	pinctrl_gpio_direction_output(chgInfo->usb_switch_cb1_gpio);
	pinctrl_gpio_direction_output(chgInfo->usb_switch_cb2_gpio);
	pinctrl_gpio_direction_output(chgInfo->otg_en_gpio);
	//pinctrl_select_state(chgInfo->pinctrl , chgInfo->pin1_suspend);
	//pinctrl_select_state(chgInfo->pinctrl , chgInfo->pin2_suspend);
	gpio_set_value(chgInfo->usb_switch_cb1_gpio, 0);
	gpio_set_value(chgInfo->usb_switch_cb2_gpio, 0);
	gpio_set_value(chgInfo->otg_en_gpio, 0);
	if (of_property_read_string(np, "charger_name", &chgInfo->chg_dev_name) < 0) {
		chgInfo->chg_dev_name = "primary_chg";
		pr_warn("no charger name\n");
	}

	if (of_property_read_string(np, "eint_name", &chgInfo->eint_name) < 0) {
		chgInfo->eint_name = "chr_stat";
		pr_warn("no eint name\n");
	}

	//pdata->enable_term = of_property_read_bool(np, "silergy,syv690,enable-term");

	chgInfo->chg_det_enable = of_property_read_bool(np, "silergy,syv690,charge-detect-enable");

	ret = of_property_read_u32(np, "silergy,syv690,usb-vlim", &pdata->vlim);
	if (ret) {
		pdata->vlim = 4500;
		pr_err("Failed to read node of silergy,syv690,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "silergy,syv690,usb-ilim", &pdata->ilim);
	if (ret) {
		pdata->ilim = 1550;
		pr_err("Failed to read node of silergy,syv690,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "silergy,syv690,usb-vreg", &pdata->vreg);
	if (ret) {
		pdata->vreg = 4450;
		pr_err("Failed to read node of silergy,syv690,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "silergy,syv690,usb-ichg", &pdata->ichg);
	if (ret) {
		pdata->ichg = 1550;
		pr_err("Failed to read node of silergy,syv690,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "silergy,syv690,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_err("Failed to read node of silergy,syv690,precharge-current\n");
	}

	ret = of_property_read_u32(np, "silergy,syv690,termination-current", &pdata->iterm);
	if (ret) {
		pdata->iterm = 180;
		pr_err
		    ("Failed to read node of silergy,syv690,termination-current\n");
	}

	ret = of_property_read_u32(np, "silergy,syv690,boost-voltage", &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		pr_err("Failed to read node of ti,bq2589x,boost-voltage\n");
	}

	ret = of_property_read_u32(np, "silergy,syv690,boost-current", &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of silergy,syv690,boost-current\n");
	}

#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(np, "syv690,intr_gpio", 0);
	if (ret < 0) {
		pr_err("%s no intr_gpio info\n", __func__);
	} else
		chgInfo->irq_gpio = ret;
#else
	ret = of_property_read_u32(np, "syv690,intr_gpio_num", &chgInfo->irq_gpio);
	if (ret < 0)
		pr_err("%s no intr_gpio info\n", __func__);
#endif
	if(pdata->vreg < SYV690_VREG_OFFSET)
		pdata->vreg = SYV690_VREG_OFFSET;
	if(pdata->iterm < SYV690_ITEM_OFFSET)
		pdata->iterm = SYV690_ITEM_OFFSET;
	if(pdata->boostv < SYV690_BOOSTV_OFFSET)
		pdata->boostv = SYV690_BOOSTV_OFFSET;
	if(pdata->iprechg < SYV690_IPRECHG_OFFSET)
		pdata->iprechg = SYV690_IPRECHG_OFFSET;
	return ret;
}

static ssize_t
SYV690_show_registers(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct SYV690_device *bq = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "SYV690 Reg");
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = SYV690_read_byte(bq, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
				       "Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t
SYV690_store_registers(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct SYV690_device *bq = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x14) {
		SYV690_write_byte(bq, (unsigned char) reg,  (unsigned char) val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, SYV690_show_registers,
		   SYV690_store_registers);

static struct attribute *SYV690_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group SYV690_attr_group = {
	.attrs = SYV690_attributes,
};
static const struct i2c_device_id SYV690_i2c_ids[] = {
	{ "syv690", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, SYV690_i2c_ids);

static const struct of_device_id SYV690_of_match[] = {
	{ .compatible = "silergy,syv690", },
	{ },
};
MODULE_DEVICE_TABLE(of, SYV690_of_match);

static void do_charger_secdet_work(struct work_struct *work)
{
	int ret = 0;
	union power_supply_propval propval = {0,};
	
	pr_err("%s: g_chg_info->chg_type = %d, sec_det_cnt = %d.\n",__func__, g_chg_info->chg_type, sec_det_cnt);
	if (NONSTANDARD_CHARGER == g_chg_info->chg_type && sec_det_cnt <= CHARGER_SEC_WORK_MAX) {
		if (g_chg_info->charger) {
			ret = power_supply_get_property(g_chg_info->charger,POWER_SUPPLY_PROP_CHARGE_TYPE,&propval);
			if (ret < 0) {
				pr_err("%s: psy type fail(%d)\n", __func__, ret);
				return;
			}
		
			// if (propval.intval == PPS_CHARGER)
			// 	return;
		}

		sec_det_cnt++;
		SYV690_chgdet_post_process(g_chg_info);
	}else if ((g_chg_info->chg_type == CHARGER_UNKNOWN) && (true == SYV690_field_read(g_chg_info, F_VBUS_GD)) && sec_det_cnt <= CHARGER_SEC_WORK_MAX) {
		pr_err("%s: chg_type is CHARGER_UNKNOWN, but vbus on.\n", __func__);
		sec_det_cnt++;
		SYV690_chgdet_post_process(g_chg_info);
	}else {
		sec_det_cnt = 0;
		return ;	
	}
}
static void sc8989h_set_9v(void)
{
	pr_err("SC89890H set 9V");
	SYV690_update_bits(g_chg_info, SYV690_REG_CON1,
		0x02, CON1_DM_DRIVE_MASK, CON1_DM_DRIVE_SHIFT);
	SYV690_update_bits(g_chg_info, SYV690_REG_CON1,
		0x06, CON1_DP_DRIVE_MASK, CON1_DP_DRIVE_SHIFT);
}

#define HVDCP_DETECT_DELAY   100
#define HVDCP_DETECT_COUNT   50
#define CHG_TYPE_REPORT_DELAY 120000
static void do_hvdcp_det_work(struct work_struct *work)
{
//	static int det_cnt = 0;
	u8 pre_type = 0;
	int ret = 0;
	int vbus_volt = 0;
	union power_supply_propval propval = {0,};
	unsigned long long delta;
	if(charger_manager_pd_is_online())
		return;
	delta = ktime_to_ms(ktime_sub(ktime_get(), g_chg_info->boot_time));	
	pr_err("%s:power-on time: %d ms.\n", __func__, delta);
	pre_type = g_chg_info->chg_type;
	if (g_chg_info->charger) {
		ret = power_supply_get_property(g_chg_info->charger,POWER_SUPPLY_PROP_CHARGE_TYPE,&propval);
		if (ret < 0) {
			pr_err("%s: psy type fail(%d)\n", __func__, ret);
			return;
		}
		
//		if (delta >= CHG_TYPE_REPORT_DELAY)
//			return;
	}

	if (pre_type != HVDCP_CHARGER && (SYV690_check_charger_type(g_chg_info) == SYV690_CHG_TYPE_HVDCP)) {
		det_cnt = 0;
		g_chg_info->chg_type = HVDCP_CHARGER;
		SYV690_psy_chg_type_changed(g_chg_info);
		vbus_volt = battery_get_vbus();
		if (vbus_volt > 7200) {
			ret = SYV690_field_write(g_chg_info, F_VINDPM, 0x34);
			pr_err(" hvdcp_set_ivl  write :%s\n",  ret < 0 ? "failed" : "successfully");
		}
	} else {
		det_cnt++;
		if (det_cnt == 20)
			SYV690_enable_force_dpdm(g_chg_info, true);
		if (det_cnt > HVDCP_DETECT_COUNT || (false == SYV690_field_read(g_chg_info, F_VBUS_GD))) {
			pr_info("not attach hvdcp, vbus_good:%d\n", SYV690_field_read(g_chg_info, F_VBUS_GD));
			det_cnt = 0;
			return;
		} else {
			schedule_delayed_work(&g_chg_info->hvdcp_det_work, msecs_to_jiffies(HVDCP_DETECT_DELAY));
		}
	}
	pre_current = 0;
	pr_info("%s main_type = %d, is_hvdcp = %d, det_cnt = %d\n",
			__func__, g_chg_info->chg_type, SYV690_check_charger_type(g_chg_info), det_cnt);
}

static void do_hardreset_hvdcp_work(struct work_struct *work)
{
	if ((g_chg_info->chg_type == STANDARD_HOST) ||
			(g_chg_info->chg_type == CHARGING_HOST)) {
		pr_err("%s pd is ready, return type:%d\n", __func__, g_chg_info->chg_type);
		return;
	} else {
		pr_err("%s enable hvdcp\n", __func__);
		if (g_chg_info->chip_id == SC89890H_ID)
			sc8989h_set_9v();
		else {
			SYV690_handle_hvdcp(g_chg_info, true);
			SYV690_enable_force_dpdm(g_chg_info, true);
		}
	}
}

static int SYV690_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct SYV690_device *chg_info;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	int boot_mode = 11;//UNKNOWN_BOOT
	int ret;
	int i;
	u8 addr = 0;
	u8 val = 0 ;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	chg_info = devm_kzalloc(dev, sizeof(*chg_info), GFP_KERNEL);
	if (!chg_info)
		return -ENOMEM;

	chg_info->client = client;
	chg_info->dev = dev;
	chg_info->attach = false;
	chg_info->need_retry_det = false;
	chg_info->ignore_usb = false;
	chg_info->chg_type = CHARGER_UNKNOWN;
	chg_info->charge_enabled = false;
	chg_info->boot_time = ktime_get();
	mutex_init(&chg_info->lock);
	mutex_init(&chg_info->chgdet_lock);

	chg_info->rmap = devm_regmap_init_i2c(client, &SYV690_regmap_config);
	if (IS_ERR(chg_info->rmap)) {
		pr_err("failed to allocate register map\n");
		return PTR_ERR(chg_info->rmap);
	}

	for (i = 0; i < ARRAY_SIZE(SYV690_reg_fields); i++) {
		const struct reg_field *reg_fields = SYV690_reg_fields;

		chg_info->rmap_fields[i] = devm_regmap_field_alloc(dev, chg_info->rmap,
							     reg_fields[i]);
		if (IS_ERR(chg_info->rmap_fields[i])) {
			pr_err("cannot allocate regmap field\n");
			return PTR_ERR(chg_info->rmap_fields[i]);
		}
	}

	i2c_set_clientdata(client, chg_info);

	chg_info->chip_id = SYV690_field_read(chg_info, F_PN);
	device_chipid = chg_info->chip_id;
	g_chg_info = chg_info;	
#if 0	
	if (chg_info->chip_id < 0) {
		dev_err(dev, "Cannot read chip ID.\n");
		return chg_info->chip_id;
	}

	if (chg_info->chip_id != SYV690_ID) {
		dev_err(dev, "Chip with ID=%d, not supported!\n", chg_info->chip_id);
		return -ENODEV;
	}
#else
	pr_err("Chip with ID=%d !!!\n", chg_info->chip_id);
#endif
	if (dev != NULL){
		boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
		if (!boot_node){
			chr_err("%s: failed to get boot mode phandle\n", __func__);
		}
		else {
			tag = (struct tag_bootmode *)of_get_property(boot_node,
								"atag,boot", NULL);
			if (!tag){
				chr_err("%s: failed to get atag,boot\n", __func__);
			} else {
				boot_mode = tag->bootmode;
			}
		}
	}
	if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		chg_info->kpoc = true;
		if (SYV690_check_charger_type(chg_info) == SYV690_CHG_TYPE_CDP)
			chg_info->lk_type_is_cdp = true;
        } else {
		chg_info->lk_type_is_cdp = false;
		chg_info->kpoc = false;
	}
	pr_err("%s: chg_info->lk_type_is_cdp = %d, chg_info->kpoc = %d\n", __func__, chg_info->lk_type_is_cdp, chg_info->kpoc);

	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = SYV690_read_byte(chg_info, addr, &val);
		pr_err("Reg[%.2x] = 0x%.2x,ret = %d\n", addr, val,ret);
	}
	
	match = of_match_node(SYV690_of_match, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		return -EINVAL;
	}

	ret = SYV690_parse_dt(node, chg_info);
	if (ret < 0) {
		pr_err("Cannot read device properties.\n");
		return ret;
	}

	ret = SYV690_hw_init(chg_info);
	if (ret < 0) {
		pr_err("Cannot initialize the chip.\n");
		return ret;
	}

	if (client->irq <= 0)
		client->irq = SYV690_irq_probe(chg_info);

	if (client->irq < 0) {
		dev_err(dev, "No irq resource found.\n");
		return client->irq;
	}

	ret = devm_request_threaded_irq(dev, client->irq, NULL,
					SYV690_irq_handler_thread,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					chg_info->eint_name, chg_info);
	if (ret)
		goto irq_fail;


	chg_info->chg_dev = charger_device_register(chg_info->chg_dev_name,
					      &client->dev, chg_info,
					      &SYV690_chg_ops,
					      &SYV690_chg_props);
	if (IS_ERR_OR_NULL(chg_info->chg_dev)) {
		ret = PTR_ERR(chg_info->chg_dev);
		return ret;
	}
	chg_info->chgdet_wq = create_singlethread_workqueue("chgdet_wq");
	if (!chg_info->chgdet_wq) {
		pr_err("%s: create chgdet_wq work queue fail\n",__func__);
		goto err_register_psy;
	}
	INIT_WORK(&chg_info->chgdet_work, do_chgdet_work_handler);
	INIT_DELAYED_WORK(&chg_info->charger_secdet_work, do_charger_secdet_work);
	INIT_DELAYED_WORK(&chg_info->hvdcp_det_work, do_hvdcp_det_work);
	INIT_DELAYED_WORK(&chg_info->set_input_current_work , do_set_input_current_work);
	INIT_DELAYED_WORK(&chg_info->hardreset_hvdcp_work , do_hardreset_hvdcp_work);
	device_init_wakeup(dev,true);

	if (chg_info->chip_id == SYV690_ID)
		hardwareinfo_set_prop(HARDWARE_CHARGER_IC, "SYV690_CHARGER");
	else if (chg_info->chip_id == BQ25890H_ID)
		hardwareinfo_set_prop(HARDWARE_CHARGER_IC, "BQ25890H_CHARGER");

	ret = sysfs_create_group(&chg_info->dev->kobj, &SYV690_attr_group);
	if (ret)
		pr_err("failed to register sysfs. err: %d\n", ret);
	return 0;
err_register_psy:
	charger_device_unregister(chg_info->chg_dev);
irq_fail:
	return ret;
}

static int SYV690_remove(struct i2c_client *client)
{

	return 0;
}
extern int get_shipmode_status(void);
static void SYV690_shut_down(struct  i2c_client *client)
{
	int ret;
	int retry;
	int done = 1;
	struct SYV690_device *bq = i2c_get_clientdata(client);

	if (get_shipmode_status() == 1) {
		ret = SYV690_field_write(bq, F_BATFET_DIS, 1);
		pr_err("%s set ship_mode ret=%d\n",  __func__, ret);
	}


	if (g_chg_info->chg_type == HVDCP_CHARGER) {

		ret = SYV690_field_write(bq, F_HVDCP_EN, 0);

		gpio_set_value(g_chg_info->usb_switch_cb1_gpio, 1);
		gpio_set_value(g_chg_info->usb_switch_cb2_gpio, 1);

		//disable hvdcp and do bc1.2 again
		SYV690_enable_force_dpdm(g_chg_info,true);

		for(retry = 0;retry < 20 ; retry++) {
			SYV690_is_dpdm_done(g_chg_info,&done);
			msleep(10);
			if(!done)
				break;
		}

		pr_err("%s done : %d ,retry = %d \n",  __func__, done,retry);
	}

	if(charger_manager_pd_is_online()) {
		adapter_set_cap(5000,2000);
	}

}

#ifdef CONFIG_PM_SLEEP
static int SYV690_suspend(struct device *dev)
{
	u8 addr = 0 ,val = 0;
	struct SYV690_device *chg= dev_get_drvdata(dev);
	pr_err("SYV690 Enter suspend\n");

	for (addr = 0x0; addr <= 0x14; addr++) {
		 SYV690_read_byte(chg, addr, &val);
		pr_err("SYV690_REG[%.2x] = 0x%.2x\n", addr, val);
	}
	return 0 ;
}

static int SYV690_resume(struct device *dev)
{

	return 0;
}
#endif

static const struct dev_pm_ops SYV690_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(SYV690_suspend, SYV690_resume)
};



static struct i2c_driver SYV690_driver = {
	.driver = {
		.name = "syv690",
		.of_match_table = of_match_ptr(SYV690_of_match),
		.pm = &SYV690_pm,
	},
	.probe = SYV690_probe,
	.remove = SYV690_remove,
	.shutdown = SYV690_shut_down,
	.id_table = SYV690_i2c_ids,
};
module_i2c_driver(SYV690_driver);

MODULE_AUTHOR("lvyuanchuan <lvyuanchuan@wingtech.com>");
MODULE_DESCRIPTION("SYV690 charger driver");
MODULE_LICENSE("GPL");
