/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _SGM4151x_CHARGER_H
#define _SGM4151x_CHARGER_H

#include <linux/i2c.h>

/*define register*/
#define SGM4151x_CHRG_CTRL_0	0x00
#define SGM4151x_CHRG_CTRL_1	0x01
#define SGM4151x_CHRG_CTRL_2	0x02
#define SGM4151x_CHRG_CTRL_3	0x03
#define SGM4151x_CHRG_CTRL_4	0x04
#define SGM4151x_CHRG_CTRL_5	0x05
#define SGM4151x_CHRG_CTRL_6	0x06
#define SGM4151x_CHRG_CTRL_7	0x07
#define SGM4151x_CHRG_STAT		0x08
#define SGM4151x_CHRG_FAULT		0x09
#define SGM4151x_CHRG_CTRL_A	0x0a
#define SGM4151x_CHRG_CTRL_B	0x0b


/* charge status flags  */
#define SGM4151x_CHRG_EN		BIT(4)
#define SGM4151x_HIZ_EN			BIT(7)
#define SGM4151x_TERM_EN		BIT(7)
#define SGM4151x_VAC_OVP_MASK	GENMASK(7, 6)
#define SGM4151x_DPDM_ONGOING	BIT(7)
#define SGM4151x_VBUS_GOOD		BIT(7)

#define SGM4151x_OTG_MASK		GENMASK(5, 4)
#define SGM4151x_OTG_EN			BIT(5)

/* Part ID  */
#define SGM4151x_PN_MASK		GENMASK(6, 3)
#define SGM4151x_PN_41541_ID	(BIT(6) | BIT(5))
#define SGM4151x_PN_41516_ID	(BIT(6) | BIT(5))
#define SGM4151x_PN_41542_ID	(BIT(6) | BIT(5) | BIT(3))
#define SGM4151x_PN_41516D_ID	(BIT(6) | BIT(5) | BIT(3))

/* WDT TIMER SET  */
#define SGM4151x_WDT_TIMER_MASK		GENMASK(5, 4)
#define SGM4151x_WDT_TIMER_DISABLE	0
#define SGM4151x_WDT_TIMER_40S		BIT(4)
#define SGM4151x_WDT_TIMER_80S		BIT(5)
#define SGM4151x_WDT_TIMER_160S		(BIT(4) | BIT(5))

#define SGM4151x_WDT_RST_MASK		BIT(6)

/* SAFETY TIMER SET  */
#define SGM4151x_SAFETY_TIMER_MASK		GENMASK(3, 3)
#define SGM4151x_SAFETY_TIMER_DISABLE	0
#define SGM4151x_SAFETY_TIMER_EN		BIT(3)
#define SGM4151x_SAFETY_TIMER_5H		0
#define SGM4151x_SAFETY_TIMER_10H		BIT(2)

/* recharge voltage  */
#define SGM4151x_VRECHARGE			BIT(0)
#define SGM4151x_VRECHRG_STEP_mV	100
#define SGM4151x_VRECHRG_OFFSET_mV	100

/* charge status  */
#define SGM4151x_VSYS_STAT		BIT(0)
#define SGM4151x_THERM_STAT		BIT(1)
#define SGM4151x_PG_STAT		BIT(2)
#define SGM4151x_CHG_STAT_MASK	GENMASK(4, 3)
#define SGM4151x_PRECHRG		BIT(3)
#define SGM4151x_FAST_CHRG		BIT(4)
#define SGM4151x_TERM_CHRG		(BIT(3) | BIT(4))

/* charge type  */
#define SGM4151x_VBUS_STAT_MASK	GENMASK(7, 5)
#define SGM4151x_NOT_CHRGING	0
#define SGM4151x_OTG_MODE		(BIT(7) | BIT(6) | BIT(5))

/* TEMP Status  */
#define SGM4151x_TEMP_MASK		GENMASK(2, 0)
#define SGM4151x_TEMP_NORMAL	BIT(0)
#define SGM4151x_TEMP_WARM		BIT(1)
#define SGM4151x_TEMP_COOL		(BIT(0) | BIT(1))
#define SGM4151x_TEMP_COLD		(BIT(0) | BIT(3))
#define SGM4151x_TEMP_HOT		(BIT(2) | BIT(3))

/* precharge current  */
#define SGM4151x_PRECHRG_CUR_MASK			GENMASK(7, 4)
#define SGM4151x_PRECHRG_CURRENT_STEP_uA	60000
#define SGM4151x_PRECHRG_I_MIN_uA			60000
#define SGM4151x_PRECHRG_I_MAX_uA			780000
#define SGM4151x_PRECHRG_I_DEF_uA			180000

/* termination current  */
#define SGM4151x_TERMCHRG_CUR_MASK			GENMASK(3, 0)
#define SGM4151x_TERMCHRG_CURRENT_STEP_uA	60000
#define SGM4151x_TERMCHRG_I_MIN_uA			60000
#define SGM4151x_TERMCHRG_I_MAX_uA			960000
#define SGM4151x_TERMCHRG_I_DEF_uA			180000

/* charge current  */
#define SGM4151x_ICHRG_CUR_MASK			GENMASK(5, 0)
#define SGM4151x_ICHRG_CURRENT_STEP_uA	60000
#define SGM4151x_ICHRG_I_MIN_uA			0
#define SGM4151x_ICHRG_I_MAX_uA			3000000
#define SGM4151x_ICHRG_I_DEF_uA			1800000

/* charge voltage  */
#define SGM4151x_VREG_V_MASK	GENMASK(7, 3)
#define SGM4151x_VREG_V_MAX_uV	4624000
#define SGM4151x_VREG_V_MIN_uV	3856000
#define SGM4151x_VREG_V_DEF_uV	4408000
#define SGM4151x_VREG_V_SPEC_uV	4352000

#define SGM4151x_VREG_V_STEP_uV	32000

/* VREG Fine Tuning  */
//#define SGM4151x_VREG_FT_MASK		GENMASK(7, 6)
//#define SGM4151x_VREG_FT_UP_8mV	BIT(6)
//#define SGM4151x_VREG_FT_DN_8mV	BIT(7)
//#define SGM4151x_VREG_FT_DN_16mV	(BIT(7) | BIT(6))

/* iindpm current  */
#define SGM4151x_IINDPM_I_MASK		GENMASK(4, 0)
#define SGM4151x_IINDPM_I_MIN_uA	100000
#define SGM4151x_IINDPM_I_MAX_uA	3200000
#define SGM4151x_IINDPM_STEP_uA		100000
#define SGM4151x_IINDPM_DEF_uA		1500000

#define SGM4151x_IINDPM_STAT	BIT(5)

/* vindpm voltage  */
#define SGM4151x_VINDPM_V_MASK		GENMASK(3, 0)
#define SGM4151x_VINDPM_V_MIN_uV	3900000
#define SGM4151x_VINDPM_V_MAX_uV	5400000
#define SGM4151x_VINDPM_STEP_uV		100000
#define SGM4151x_VINDPM_DEF_uV		4500000
//#define SGM4151x_VINDPM_OS_MASK	GENMASK(1, 0)

#define SGM4151x_VINDPM_STAT	BIT(6)

/* DP DM SEL  */
//#define SGM4151x_DP_VSEL_MASK	GENMASK(4, 3)
//#define SGM4151x_DM_VSEL_MASK	GENMASK(2, 1)

/* PUMPX SET  */
//#define SGM4151x_EN_PUMPX	BIT(7)
//#define SGM4151x_PUMPX_UP	BIT(6)
//#define SGM4151x_PUMPX_DN	BIT(5)

struct sgm4151x_device {
	struct i2c_client *client;
	struct device *dev;
	struct charger_device *chg_dev;
	struct power_supply *charger;
	struct mutex i2c_rw_lock;
	struct power_supply_config psy_cfg;
	struct power_supply_desc sgm4151x_power_supply_desc;
	struct charger_properties sgm4151x_chg_props;
};
#endif /* _SGM4151x_CHARGER_H */
