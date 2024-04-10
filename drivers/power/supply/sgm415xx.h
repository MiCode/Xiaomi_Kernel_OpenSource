/* SPDX-License-Identifier: GPL-2.0-only */
// sgm4154x Charger Driver
// Copyright (C) 2021 Texas Instruments Incorporated - http://www.sg-micro.com

#ifndef _SGM4154x_CHARGER_H
#define _SGM4154x_CHARGER_H

#include <linux/i2c.h>

#define SGM4154x_MANUFACTURER	"Texas Instruments"
//#define __SGM41541_CHIP_ID__
//#define __SGM41542_CHIP_ID__
#define __SGM41513_CHIP_ID__
//#define __SGM41513A_CHIP_ID__
//#define __SGM41513D_CHIP_ID__
//#define __SGM41516_CHIP_ID__
//#define __SGM41516D_CHIP_ID__
//#define __SGM41543_CHIP_ID__
//#define __SGM41543D_CHIP_ID__

#ifdef __SGM41541_CHIP_ID__
#define SGM4154x_NAME		"sgm41541"
#define SGM4154x_PN_ID     (BIT(6)| BIT(5))
#endif

#ifdef __SGM41542_CHIP_ID__
#define SGM4154x_NAME		"sgm41542"
#define SGM4154x_PN_ID      (BIT(6)| BIT(5)| BIT(3))
#endif

#ifdef __SGM41513_CHIP_ID__
#define SGM4154x_NAME		"sgm41513"
#define SGM4154x_PN_ID      0
#endif

#ifdef __SGM41513A_CHIP_ID__
#define SGM4154x_NAME		"sgm41513A"
#define SGM4154x_PN_ID      BIT(3)
#endif

#ifdef __SGM41513D_CHIP_ID__
#define SGM4154x_NAME		"sgm41513D"
#define SGM4154x_PN_ID      BIT(3)
#endif

#ifdef __SGM41516_CHIP_ID__
#define SGM4154x_NAME		"sgm41516"
#define SGM4154x_PN_ID      (BIT(6)| BIT(5))
#endif

#ifdef __SGM41516D_CHIP_ID__
#define SGM4154x_NAME		"sgm41516D"
#define SGM4154x_PN_ID     (BIT(6)| BIT(5)| BIT(3))
#endif

#ifdef __SGM41543_CHIP_ID__
#define SGM4154x_NAME		"sgm41543"
#define SGM4154x_PN_ID      BIT(6)
#endif

#ifdef __SGM41543D_CHIP_ID__
#define SGM4154x_NAME		"sgm41543D"
#define SGM4154x_PN_ID      (BIT(6)| BIT(3))
#endif

/*define register*/
#define SGM4154x_CHRG_CTRL_0	0x00
#define SGM4154x_CHRG_CTRL_1	0x01
#define SGM4154x_CHRG_CTRL_2	0x02
#define SGM4154x_CHRG_CTRL_3	0x03
#define SGM4154x_CHRG_CTRL_4	0x04
#define SGM4154x_CHRG_CTRL_5	0x05
#define SGM4154x_CHRG_CTRL_6	0x06
#define SGM4154x_CHRG_CTRL_7	0x07
#define SGM4154x_CHRG_STAT	    0x08
#define SGM4154x_CHRG_FAULT	    0x09
#define SGM4154x_CHRG_CTRL_a	0x0a
#define SGM4154x_CHRG_CTRL_b	0x0b
#define SGM4154x_CHRG_CTRL_c	0x0c
#define SGM4154x_CHRG_CTRL_d	0x0d
#define SGM4154x_INPUT_DET   	0x0e
#define SGM4154x_CHRG_CTRL_f	0x0f

/* charge status flags  */
#define SGM4154x_CHRG_EN		BIT(4)
#define SGM4154x_HIZ_EN		    BIT(7)
#define SGM4154x_TERM_EN		BIT(7)
#define SGM4154x_VAC_OVP_MASK	GENMASK(7, 6)
#define SGM4154x_DPDM_ONGOING   BIT(7)
#define SGM4154x_VBUS_GOOD      BIT(7)

#define SGM4154x_BOOSTV 		GENMASK(5, 4)
#define SGM4154x_BOOST_LIM 		BIT(7)
#define SGM4154x_OTG_EN		    BIT(5)

/* Part ID  */
#define SGM4154x_PN_MASK	    GENMASK(6, 3)

/* WDT TIMER SET  */
#define SGM4154x_WDT_TIMER_MASK        GENMASK(5, 4)
#define SGM4154x_WDT_TIMER_DISABLE     0
#define SGM4154x_WDT_TIMER_40S         BIT(4)
#define SGM4154x_WDT_TIMER_80S         BIT(5)
#define SGM4154x_WDT_TIMER_160S        (BIT(4)| BIT(5))

#define SGM4154x_WDT_RST_MASK          BIT(6)

/* SAFETY TIMER SET  */
#define SGM4154x_SAFETY_TIMER_MASK     GENMASK(3, 3)
#define SGM4154x_SAFETY_TIMER_DISABLE     0
#define SGM4154x_SAFETY_TIMER_EN       BIT(3)
#define SGM4154x_SAFETY_TIMER_5H         0
#define SGM4154x_SAFETY_TIMER_10H      BIT(2)

/* recharge voltage  */
#define SGM4154x_VRECHARGE              BIT(0)
#define SGM4154x_VRECHRG_STEP_mV		100
#define SGM4154x_VRECHRG_OFFSET_mV		100

/* charge status  */
#define SGM4154x_VSYS_STAT		BIT(0)
#define SGM4154x_THERM_STAT		BIT(1)
#define SGM4154x_PG_STAT		BIT(2)
#define SGM4154x_CHG_STAT_MASK	GENMASK(4, 3)
#define SGM4154x_PRECHRG		BIT(3)
#define SGM4154x_FAST_CHRG	    BIT(4)
#define SGM4154x_TERM_CHRG	    (BIT(3)| BIT(4))

/* charge type  */
#define SGM4154x_VBUS_STAT_MASK	GENMASK(7, 5)
#define SGM4154x_NOT_CHRGING	0
#define SGM4154x_USB_SDP		BIT(5)
#define SGM4154x_USB_CDP		BIT(6)
#define SGM4154x_USB_DCP		(BIT(5) | BIT(6))
#define SGM4154x_UNKNOWN	    (BIT(7) | BIT(5))
#define SGM4154x_NON_STANDARD	(BIT(7) | BIT(6))
#define SGM4154x_OTG_MODE	    (BIT(7) | BIT(6) | BIT(5))

/* TEMP Status  */
#define SGM4154x_TEMP_MASK	    GENMASK(2, 0)
#define SGM4154x_TEMP_NORMAL	BIT(0)
#define SGM4154x_TEMP_WARM	    BIT(1)
#define SGM4154x_TEMP_COOL	    (BIT(0) | BIT(1))
#define SGM4154x_TEMP_COLD	    (BIT(0) | BIT(3))
#define SGM4154x_TEMP_HOT	    (BIT(2) | BIT(3))

/* precharge current  */
#define SGM4154x_PRECHRG_CUR_MASK		GENMASK(7, 4)
#define SGM4154x_PRECHRG_CURRENT_STEP_uA		60000
#define SGM4154x_PRECHRG_I_MIN_uA		60000
#define SGM4154x_PRECHRG_I_MAX_uA		780000
#define SGM4154x_PRECHRG_I_DEF_uA		180000
#define SGM4154x_PRECHRG_I_SET_uA		240000 //uA

/* termination current  */
#define SGM4154x_TERMCHRG_CUR_MASK		GENMASK(3, 0)
#define SGM4154x_TERMCHRG_CURRENT_STEP_uA	60000
#define SGM4154x_TERMCHRG_I_MIN_uA		60000
#define SGM4154x_TERMCHRG_I_MAX_uA		960000
#define SGM4154x_TERMCHRG_I_DEF_uA		180000
#define SGM4154x_TERMCHRG_I_SET_uA		240000 //uA

/* charge current  */
#define SGM4154x_ICHRG_I_MASK		GENMASK(5, 0)

#define SGM4154x_ICHRG_I_MIN_uA			0
#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
#define SGM4154x_ICHRG_I_MAX_uA			3000000
#define SGM4154x_ICHRG_I_DEF_uA			1980000
#else
#define SGM4154x_ICHRG_I_STEP_uA	    60000
#define SGM4154x_ICHRG_I_MAX_uA			3780000
#define SGM4154x_ICHRG_I_DEF_uA			2040000
#endif
/* charge voltage  */
#define SGM4154x_VREG_V_MASK		GENMASK(7, 3)
#define SGM4154x_VREG_V_MAX_uV	    4624000
#define SGM4154x_VREG_V_MIN_uV	    3856000
#define SGM4154x_VREG_V_DEF_uV	    4208000
#define SGM4154x_VREG_V_STEP_uV	    32000
#define SGM4154x_VREG_V_SET_uV		4400000  //uV

/* iindpm current  */
#define SGM4154x_IINDPM_I_MASK		GENMASK(4, 0)
#define SGM4154x_IINDPM_I_MIN_uA	100000
#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
#define SGM4154x_IINDPM_I_MAX_uA	3200000
#else
#define SGM4154x_IINDPM_I_MAX_uA	3800000
#endif
#define SGM4154x_IINDPM_STEP_uA	    100000
#define SGM4154x_IINDPM_DEF_uA	    2400000

/* vindpm voltage  */
#define SGM4154x_VINDPM_V_MASK      GENMASK(3, 0)
#define SGM4154x_VINDPM_V_MIN_uV    3900000
#define SGM4154x_VINDPM_V_MAX_uV    12000000
#define SGM4154x_VINDPM_STEP_uV     100000
#define SGM4154x_VINDPM_DEF_uV	    4500000
#define SGM4154x_VINDPM_OS_MASK     GENMASK(1, 0)

/* DP DM SEL  */
#define SGM4154x_DP_VSEL_MASK       GENMASK(4, 3)
#define SGM4154x_DM_VSEL_MASK       GENMASK(2, 1)

/* PUMPX SET  */
#define SGM4154x_EN_PUMPX           BIT(7)
#define SGM4154x_PUMPX_UP           BIT(6)
#define SGM4154x_PUMPX_DN           BIT(5)
#define SGM4154x_OTGF_ITREMR		BIT(0)
/* add for ship_mode*/
#define SGM4154X_BATFET_DLY			BIT(3)
#define SGM4154X_BATFET_DIS			BIT(5)
#define SGM4154X_BATFET_RST			BIT(2)

struct sgm4154x_init_data {
	u32 ichg;	/* charge current		*/
	u32 ilim;	/* input current		*/
	u32 vreg;	/* regulation voltage		*/
	u32 iterm;	/* termination current		*/
	u32 iprechg;	/* precharge current		*/
	u32 vlim;	/* minimum system voltage limit */
	u32 max_ichg;
	u32 max_vreg;
};

struct sgm4154x_state {
	bool vsys_stat;
	bool therm_stat;
	bool online;	
	u8 chrg_stat;
	u8 vbus_status;

	bool chrg_en;
	bool hiz_en;
	bool term_en;
	bool vbus_gd;
	u8 chrg_type;
	u8 health;
	u8 chrg_fault;
	u8 ntc_fault;
};

struct sgm4154x_device {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *charger;	
	struct power_supply *usb;
	struct power_supply *ac;
	struct mutex lock;
	struct mutex i2c_rw_lock;

	struct usb_phy *usb2_phy;
	struct usb_phy *usb3_phy;
	struct notifier_block usb_nb;
	struct work_struct usb_work;
	unsigned long usb_event;
	struct regmap *regmap;

	char model_name[I2C_NAME_SIZE];
	u8 device_id;

	struct sgm4154x_init_data init_data;
	struct sgm4154x_state state;
	u32 watchdog_timer;
	#if 1//defined(CONFIG_MTK_GAUGE_VERSION) && (CONFIG_MTK_GAUGE_VERSION == 30)
	struct charger_device *chg_dev;
	#endif
	struct regulator_dev *otg_rdev;

	struct delayed_work charge_detect_delayed_work;
	struct delayed_work charge_monitor_work;
	struct notifier_block pm_nb;
	bool sgm4154x_suspend_flag;

	struct wakeup_source *charger_wakelock;
	//bool enable_sw_jeita;
	//struct sgm4154x_jeita data;
};

#endif /* _SGM4154x_CHARGER_H */
