// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#define pr_fmt(fmt) "SMB1398: %s: " fmt, __func__

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pmic-voter.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/iio/consumer.h>
#include <linux/i2c.h>
#include "bq25790.h"

#define REVID_REVISION4			0x103

/* Status register definition */
#define INPUT_STATUS_REG		0x2609
#define INPUT_USB_IN			BIT(1)
#define INPUT_WLS_IN			BIT(0)

#define PERPH0_INT_RT_STS_REG		0x2610
#define USB_IN_OVLO_STS			BIT(7)
#define WLS_IN_OVLO_STS			BIT(6)
#define USB_IN_UVLO_STS			BIT(5)
#define WLS_IN_UVLO_STS			BIT(4)
#define DIV2_IREV_LATCH_STS		BIT(3)
#define VOL_UV_LATCH_STS		BIT(2)
#define TEMP_SHUTDOWN_STS		BIT(1)
#define CFLY_HARD_FAULT_LATCH_STS	BIT(0)

#define SCHG_PERPH_INT_CLR	       0x616
#define SCHG_PERPH0_INT_CLR            0x2616
#define SCHG_PERPH1_INT_CLR            0x2716

#define PERPH0_MISC_CFG2_REG		0x2636
#define CFG_TEMP_PIN_ITEMP		BIT(1)

#define MODE_STATUS_REG			0x2641
#define SMB_EN				BIT(7)
#define PRE_EN_DCDC			BIT(6)
#define DIV2_EN_SLAVE			BIT(5)
#define LCM_EN				BIT(4)
#define DIV2_EN				BIT(3)
#define BUCK_EN				BIT(2)
#define CFLY_SS_DONE			BIT(1)
#define DCDC_EN				BIT(0)

#define SWITCHER_OFF_WIN_STATUS_REG	0x2642
#define DIV2_WIN_OV			BIT(1)
#define DIV2_WIN_UV			BIT(0)

#define SWITCHER_OFF_VIN_STATUS_REG	0x2643
#define USB_IN_OVLO			BIT(3)
#define WLS_IN_OVLO			BIT(2)
#define USB_IN_UVLO			BIT(1)
#define WLS_IN_UVLO			BIT(0)

#define SWITCHER_OFF_FAULT_REG		0x2644
#define VOUT_OV_3LVL_BUCK		BIT(5)
#define VOUT_UV_LATCH			BIT(4)
#define ITERM_3LVL_LATCH		BIT(3)
#define DIV2_IREV_LATCH			BIT(2)
#define TEMP_SHDWN			BIT(1)
#define CFLY_HARD_FAULT_LATCH		BIT(0)

#define BUCK_CC_CV_STATE_REG		0x2645
#define BUCK_IN_CC_REGULATION		BIT(1)
#define BUCK_IN_CV_REGULATION		BIT(0)

#define INPUT_CURRENT_REGULATION_REG	0x2646
#define BUCK_IN_ICL			BIT(1)
#define DIV2_IN_ILIM			BIT(0)

/* Config register definition */
#define MISC_USB_WLS_SUSPEND_REG	0x2630
#define WLS_SUSPEND			BIT(1)
#define USB_SUSPEND			BIT(0)

#define MISC_SL_SWITCH_EN_REG		0x2631
#define EN_SLAVE			BIT(1)
#define EN_SWITCHER			BIT(0)

#define MISC_DIV2_3LVL_CTRL_REG		0x2632
#define EN_DIV2_CP			BIT(2)
#define EN_3LVL_BULK			BIT(1)
#define EN_CHG_2X			BIT(0)

#define MISC_CFG0_REG			0x2634
#define DIS_SYNC_DRV_BIT		BIT(5)
#define SW_EN_SWITCHER_BIT		BIT(3)
#define CFG_ITLGS_2P_BIT			BIT(0)
#define CFG_IREV_Q1_ENABLE			BIT(1)

#define MISC_CFG1_REG			0x2635
#define CFG_OP_MODE_MASK		GENMASK(2, 0)
#define OP_MODE_DISABLED		0
#define OP_MODE_3LVL_BULK		1
#define OP_MODE_COMBO			2
#define OP_MODE_DIV2_CP			3
#define OP_MODE_PRE_REG_3S		4
#define OP_MODE_ITLGS_1P		5
#define OP_MODE_ITLGS_2X		6
#define OP_MODE_PRE_REGULATOR		7

#define PERPH0_MISC_CFG0		0x2634
#define CFG_PRE_REG_2S		BIT(1)

#define MISC_CFG2_REG			0x2636

#define NOLOCK_SPARE_REG		0x2637
#define STANDALONE_EN_BIT		BIT(5)
#define STANDALONE_EN			1
#define STANDALONE_DIS			0
#define DIV2_WIN_UV_SEL_BIT		BIT(4)
#define DIV2_WIN_UV_25MV		0
#define DIV2_WIN_UV_12P5MV		BIT(4)
#define COMBO_WIN_LO_EXIT_SEL_MASK	GENMASK(3, 2)
#define EXIT_DIV2_VOUT_HI_12P5MV	0
#define EXIT_DIV2_VOUT_HI_25MV		1
#define EXIT_DIV2_VOUT_HI_50MV		2
#define EXIT_DIV2_VOUT_HI_75MV		3
#define COMBO_WIN_HI_EXIT_SEL_MASK	GENMASK(1, 0)
#define EXIT_DIV2_VOUT_LO_75MV		0
#define EXIT_DIV2_VOUT_LO_100MV		1
#define EXIT_DIV2_VOUT_LO_200MV		2
#define EXIT_DIV2_VOUT_LO_250MV		3

#define SMB_EN_TRIGGER_CFG_REG		0x2639
#define SMB_EN_NEG_TRIGGER		BIT(1)
#define SMB_EN_POS_TRIGGER		BIT(0)

#define DIV2_LCM_CFG_REG		0x2653
#define DIV2_LCM_REFRESH_TIMER_SEL_MASK	GENMASK(5, 4)
#define DIV2_WIN_BURST_HIGH_REF_MASK	GENMASK(3, 2)
#define DIV2_WIN_BURST_LOW_REF_MASK	GENMASK(1, 0)

#define DIV2_CURRENT_REG		0x2655
#define DIV2_EN_ILIM_DET		BIT(2)
#define DIV2_EN_IREV_DET		BIT(1)
#define DIV2_EN_OCP_DET			BIT(0)

#define DIV2_PROTECTION_REG		0x2656
#define DIV2_WIN_OV_SEL_MASK		GENMASK(1, 0)
#define WIN_OV_200_MV			0
#define WIN_OV_300_MV			1
#define WIN_OV_400_MV			2
#define WIN_OV_500_MV			3

#define CFG_3LVL_BK_CURRENT            0x2657
#define CFG_HSON_SEL                   BIT(0)

#define DIV2_MODE_CFG_REG		0x265C

#define LCM_EXIT_CTRL_REG		0x265D

#define ICHG_SS_DAC_TARGET_REG		0x2660
#define ICHG_SS_DAC_VALUE_MASK		GENMASK(5, 0)
#define ICHG_STEP_MA			100

#define VOUT_DAC_TARGET_REG		0x2663
#define VOUT_DAC_VALUE_MASK		GENMASK(7, 0)
#define VOUT_1P_MIN_MV			3300
#define VOUT_1S_MIN_MV			6600
#define VOUT_1P_STEP_MV			10
#define VOUT_1S_STEP_MV			20

#define VOUT_SS_DAC_TARGET_REG		0x2666
#define VOUT_SS_DAC_VALUE_MASK		GENMASK(5, 0)
#define VOUT_SS_1P_STEP_MV		90
#define VOUT_SS_1S_STEP_MV		180

#define WLS_IIN_SS_DAC_TARGET_REG 0x2651



#define IIN_SS_DAC_TARGET_REG		0x2669
#define IIN_SS_DAC_VALUE_MASK		GENMASK(6, 0)
#define IIN_STEP_MA			50

#define CFG_OVP_IGNORE_UVLO_REG			0x2680
#define OVP_IGNORE_UVLO_MASK			BIT(5)
#define OVP_IGNORE_UVLO_EN			0
#define OVP_IGNORE_UVLO_DIS			BIT(5)

#define USBIN_OVLO_THRESHOLD_REG	0x2682
#define EN_MV_OV_OPTION1_22_15V		0
#define EN_HV_OV_OPTION2_22_15V		0
#define EN_MV_OV_OPTION1_7_2V		1
#define EN_HV_OV_OPTION2_7_2V		1
#define EN_MV_OV_OPTION1_MASK		BIT(7)
#define EN_HV_OV_OPTION2_MASK		BIT(5)

#define USBIN_OVP_THRESHOLD_REG		0x27C3
#define USBIN_OVP_SHIFT			6
#define USBIN_OVP_22_2V			(0x2 << USBIN_OVP_SHIFT)
#define USBIN_OVP_7_3V			(0x3 << USBIN_OVP_SHIFT)
#define USBIN_OVP_MASK			GENMASK(7, 6)

#define PERPH0_CFG_SDCDC_REG		0x267A
#define EN_WIN_UV_BIT			BIT(7)
#define EN_WIN_OV_BIT BIT(5)


#define IREV_TUNE_OFF_Q1_DELAY_REG	0x2674
#define IREV_DELAY_BIT			BIT(0)

#define SSUPPLY_CFG0				0x2682
#define EN_HV_OV_OPTION2			BIT(7)
#define EN_MV_OV_OPTION				BIT(5)

#define SSUPLY_TEMP_CTRL_REG		0x2683
#define SEL_OUT_TEMP_MAX_MASK		GENMASK(7, 5)
#define SEL_OUT_TEMP_MAX_SHFT		5
#define SEL_OUT_HIGHZ			(0 << SEL_OUT_TEMP_MAX_SHFT)
#define SEL_OUT_VTEMP			(1 << SEL_OUT_TEMP_MAX_SHFT)
#define SEL_OUT_ICHG			(2 << SEL_OUT_TEMP_MAX_SHFT)
#define SEL_OUT_IIN_FB			(4 << SEL_OUT_TEMP_MAX_SHFT)

#define SDCDC_1					0x26CF
#define EN_CHECK_CFLY_SSDONE_BIT			BIT(6)

#define PERPH1_INT_RT_STS_REG		0x2710
#define DIV2_WIN_OV_STS			BIT(7)
#define DIV2_WIN_UV_STS			BIT(6)
#define DIV2_ILIM_STS			BIT(5)
#define DIV2_CFLY_SS_DONE_STS		BIT(1)

#define PERPH1_LOCK_SPARE			0x27C3
#define EXT_OVPFET_MASK				GENMASK(7, 6)

/* available voters */
#define ILIM_VOTER			"ILIM_VOTER"
#define TAPER_VOTER			"TAPER_VOTER"
#define STATUS_CHANGE_VOTER		"STATUS_CHANGE_VOTER"
#define SHUTDOWN_VOTER			"SHUTDOWN_VOTER"
#define CUTOFF_SOC_VOTER		"CUTOFF_SOC_VOTER"
#define SRC_VOTER			"SRC_VOTER"
#define ICL_VOTER			"ICL_VOTER"
#define WIRELESS_VOTER			"WIRELESS_VOTER"
#define SWITCHER_TOGGLE_VOTER		"SWITCHER_TOGGLE_VOTER"
#define USER_VOTER			"USER_VOTER"
#define FCC_VOTER			"FCC_VOTER"
#define CP_VOTER			"CP_VOTER"
#define CC_MODE_VOTER			"CC_MODE_VOTER"
#define MAIN_DISABLE_VOTER		"MAIN_DISABLE_VOTER"
#define PASSTHROUGH_VOTER		"PASSTHROUGH_VOTER"
#define WIRELESS_CP_OPEN_VOTER		"WIRELESS_CP_OPEN_VOTER"
#define WIRELESS_ICL_VOTER		"WIRELESS_ICL_VOTER"
#define DELAY_OPEN_VOTER		"DELAY_OPEN_VOTER"
#define HIGH_POWER_VOTER		"HIGH_POWER_VOTER"
#define PL_SMB_EN_VOTER			"PL_SMB_EN_VOTER"
/* Constant definitions */
/* Need to define max ILIM for smb1398 */
#define DIV2_MAX_ILIM_UA		5000000
#define DIV2_MAX_ILIM_MA		5000
#define DIV2_MAX_ILIM_DUAL_CP_UA	10000000

#define TAPER_STEPPER_UA_DEFAULT	100000
#define TAPER_STEPPER_UA_IN_CC_MODE	200000

#define MAX_IOUT_UA			6300000
#define MAX_1S_VOUT_UV			11700000

#define THERMAL_SUSPEND_DECIDEGC	1400

#define DIV2_CP_MASTER			0
#define DIV2_CP_SLAVE			1
#define COMBO_PRE_REGULATOR		2

#define DIV2_CP_HW_VERSION_3		3
#define ADAPTER_XIAOMI_QC3		0x09
#define ADAPTER_XIAOMI_PD		0x0a
#define ADAPTER_ZIMI_CAR_POWER		0x0b
#define ADAPTER_XIAOMI_PD_40W		0x0c
#define ADAPTER_VOICE_BOX		0x0d
#define ADAPTER_XIAOMI_PD_45W	0x0e
#define ADAPTER_XIAOMI_PD_60W   0x0f
#define WLDC_XIAOMI_20W_IOUT_MAX_UA	1000000
#define WLDC_XIAOMI_30W_IOUT_MAX_UA	1500000
#define WLDC_XIAOMI_40W_IOUT_MAX_UA	2000000

#ifdef CONFIG_FACTORY_BUILD
#define WLDC_XIAOMI_50W_IOUT_MAX_UA	2260000
#else
#define WLDC_XIAOMI_50W_IOUT_MAX_UA	2500000
#endif

#define WLS_MAIN_CHAREGER_ICL_UA 200000
#ifdef CONFIG_FACTORY_BUILD
#define WLS_MAIN_START_ILIM_UA 1300000
#define WLS_MAIN_START_ILIM_THRESHOLD_UA 1100000
#define WLS_MAIN_START_ILIM_CNT 10
#else
#define WLS_MAIN_START_ILIM_UA 800000
#define WLS_MAIN_START_ILIM_THRESHOLD_UA 600000
#define WLS_MAIN_START_ILIM_CNT 20
#endif

#define MAX_MONITOR_CYCLE_CNT 70
#define MAX_RX_TEMPERATRUE 90

enum isns_mode {
	ISNS_MODE_OFF = 0,
	ISNS_MODE_ACTIVE,
	ISNS_MODE_STANDBY,
};

struct ovp_configs {
	const int voltage_uv;
	const u8 reg_config;
};

static const struct ovp_configs int_ovp_levels_uv[] = {
	[0] = {
		.voltage_uv	= 7200000,
		.reg_config = 0xA0
	},
	[1] = {
		.voltage_uv	= 13900000,
		.reg_config = 0x80
	},
	[2] = {
		.voltage_uv	= 17100000,
		.reg_config = 0x20
	},
	[3] = {
		.voltage_uv	= 22150000,
		.reg_config = 0x00
	},
};

static const struct ovp_configs ext_ovp_levels_uv[] = {
	[0] = {
		.voltage_uv	= 7300000,
		.reg_config = 0xC0
	},
	[1] = {
		.voltage_uv	= 14000000,
		.reg_config = 0x40
	},
	[2] = {
		.voltage_uv	= 17700000,
		.reg_config = 0x00
	},
	[3] = {
		.voltage_uv	= 22200000,
		.reg_config = 0x80
	},
};

static const int div2cp_win_ov_table_uv[] = {
	[0] = 200000,
	[1] = 300000,
	[2] = 400000,
	[3] = 1000000,
};

static const int passthrough_win_ov_table_uv[] = {
	[0] = 150000,
	[1] = 200000,
	[2] = 300000,
	[3] = 400000,
};

enum {
	/* Perph0 IRQs */
	CFLY_HARD_FAULT_LATCH_IRQ,
	TEMP_SHDWN_IRQ,
	VOUT_UV_LATH_IRQ,
	DIV2_IREV_LATCH_IRQ,
	WLS_IN_UVLO_IRQ,
	USB_IN_UVLO_IRQ,
	WLS_IN_OVLO_IRQ,
	USB_IN_OVLO_IRQ,
	/* Perph1 IRQs */
	BK_IIN_REG_IRQ,
	CFLY_SS_DONE_IRQ,
	EN_DCDC_IRQ,
	ITERM_3LVL_LATCH_IRQ,
	VOUT_OV_3LB_IRQ,
	DIV2_ILIM_IRQ,
	DIV2_WIN_UV_IRQ,
	DIV2_WIN_OV_IRQ,
	/* Perph2 IRQs */
	IN_3LVL_MODE_IRQ,
	DIV2_MODE_IRQ,
	BK_CV_REG_IRQ,
	BK_CC_REG_IRQ,
	SS_DAC_INT_IRQ,
	SMB_EN_RISE_IRQ,
	SMB_EN_FALL_IRQ,
	/* End */
	NUM_IRQS,
};

struct smb_irq {
	const char		*name;
	const irq_handler_t	handler;
	const bool		wake;
	int			shift;
};

struct smb1398_chip {
	struct device		*dev;
	struct regmap		*regmap;

	struct wakeup_source	*ws;
	struct iio_channel	*die_temp_chan;

	struct power_supply	*div2_cp_master_psy;
	struct power_supply	*div2_cp_slave_psy;
	struct power_supply	*pre_regulator_psy;
	struct power_supply	*batt_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*dc_psy;
	struct power_supply	*wls_psy;
	struct power_supply	*main_psy;
	struct power_supply	*bms_psy;
	struct notifier_block	nb;

	struct votable		*awake_votable;
	struct votable		*div2_cp_disable_votable;
	struct votable		*div2_cp_slave_disable_votable;
	struct votable		*div2_cp_ilim_votable;
	struct votable          *passthrough_mode_disable_votable;
	struct votable		*pre_regulator_iout_votable;
	struct votable		*pre_regulator_vout_votable;
	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*fcc_main_votable;
	struct votable          *smb_override_votable;
	struct votable		*usb_icl_votable;
	struct votable		*usbin_suspend_votable;
	struct votable		*rx_ocp_disable_votable;;

	struct work_struct	status_change_work;
	struct work_struct	taper_work;
	struct work_struct	cp_close_work;
	struct work_struct	wireless_close_work;
	struct work_struct	wireless_open_work;
	struct work_struct	isns_process_work;
	struct delayed_work	irev_process_work;
	struct delayed_work	high_power_monitor_work;
	struct delayed_work	stop_rx_ocp_work;

	struct mutex		die_chan_lock;
	spinlock_t		status_change_lock;

	int			irqs[NUM_IRQS];
	int			die_temp;
	int			div2_cp_min_ilim_ua;
	int			ilim_ua_disable_div2_cp_wls;
	int			ilim_ua_disable_div2_cp_slave;
	int			ilim_enlarge_pct;
	int			target_ovp_uv;
	int			force_ilimt;

	int			max_cutoff_soc;
	int			wls_max_cutoff_soc;
	int			taper_entry_fv;
	int			div2_irq_status;
	int			isns_cnt;
	u32			div2_cp_role;
	enum isns_mode		current_capability;

	bool			status_change_running;
	bool			taper_work_running;
	bool			cutoff_soc_checked;
	bool			smb_en;
	bool			switcher_en;
	bool			slave_en;
	bool			in_suspend;
	bool			batt_2s_chg;
	bool                    passthrough_mode_allowed;
	bool			slave_pass_mode;
	bool			standalone_mode;
	bool			cp_for_wireless;
	u32				max_power_cnt;
};

static const struct smb_irq smb_irqs[];
static bool is_psy_voter_available(struct smb1398_chip *chip);
static bool is_adapter_in_cc_mode(struct smb1398_chip *chip);
static bool is_for_wireless_charging(struct smb1398_chip *chip);

static int smb1398_read(struct smb1398_chip *chip, u16 reg, u8 *val)
{
	int rc = 0, value = 0;

	rc = regmap_read(chip->regmap, reg, &value);
	if (rc < 0)
		dev_err(chip->dev, "Read register 0x%x failed, rc=%d\n",
				reg, rc);
	else
		*val = (u8)value;

	return rc;
}

static int smb1398_masked_write(struct smb1398_chip *chip,
		u16 reg, u8 mask, u8 val)
{
	int rc = 0;

	rc = regmap_update_bits(chip->regmap, reg, mask, val);
	if (rc < 0)
		dev_err(chip->dev, "Update register 0x%x to 0x%x with mask 0x%x failed, rc=%d\n",
				reg, val, mask, rc);

	return rc;
}

static bool smb1398_is_rev3(struct smb1398_chip *chip)
{
	int rc = 0;
	u8 revid;

	rc = smb1398_read(chip, REVID_REVISION4, &revid);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read revid, rc=%d\n", rc);
		return true;
	}

	if (revid < 3)
		return false;

	return true;
}

static int smb1398_div2_cp_get_status1(
		struct smb1398_chip *chip, u8 *status)
{
	int rc = 0;
	u8 val;
	bool ilim, win_uv, win_ov;

	rc = smb1398_read(chip, PERPH1_INT_RT_STS_REG, &val);
	if (rc < 0)
		return rc;

	win_uv = !!(val & DIV2_WIN_UV_STS);
	win_ov = !!(val & DIV2_WIN_OV_STS);
	ilim = !!(val & DIV2_ILIM_STS);
	*status = ilim << 5 | win_uv << 1 | win_ov;

	dev_dbg(chip->dev, "status1 = 0x%x\n", *status);
	return rc;
}

static int smb1398_div2_cp_get_status2(
		struct smb1398_chip *chip, u8 *status)
{
	int rc = 0;
	u8 val;
	bool smb_en, vin_ov, vin_uv, irev, tsd, switcher_off;
	union power_supply_propval pval = {0};

	rc = smb1398_read(chip, MODE_STATUS_REG, &val);
	if (rc < 0)
		return rc;

	smb_en = !!(val & SMB_EN);
	switcher_off = !(val & PRE_EN_DCDC);

	rc = smb1398_read(chip, PERPH1_INT_RT_STS_REG, &val);
	if (rc < 0)
		return rc;

	switcher_off = !(val & DIV2_CFLY_SS_DONE_STS) && switcher_off;

	rc = smb1398_read(chip, SWITCHER_OFF_VIN_STATUS_REG, &val);
	if (rc < 0)
		return rc;

	if (!is_psy_voter_available(chip))
		return -EAGAIN;

	rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_SMB_EN_REASON, &pval);
	if (rc < 0)
		return rc;

	vin_ov = (pval.intval == POWER_SUPPLY_CP_WIRELESS) ?
		!!(val & WLS_IN_OVLO) :
		!!(val & USB_IN_OVLO);
		vin_uv = (pval.intval == POWER_SUPPLY_CP_WIRELESS) ?
		!!(val & WLS_IN_UVLO) :
		!!(val & USB_IN_UVLO);

	rc = smb1398_read(chip, SWITCHER_OFF_FAULT_REG, &val);
	if (rc < 0)
		return rc;

	irev = !!(val & DIV2_IREV_LATCH);
	tsd = !!(val & TEMP_SHDWN);

	*status = smb_en << 7 | vin_ov << 6 | vin_uv << 5
		| irev << 3 | tsd << 2 | switcher_off;

	dev_dbg(chip->dev, "status2 = 0x%x\n", *status);
	return rc;
}

static int smb1398_div2_cp_get_irq_status(
		struct smb1398_chip *chip, u8 *status)
{
	int rc = 0;
	u8 val;
	bool ilim, irev, tsd, off_vin, off_win;
	union power_supply_propval pval = {0};

	rc = smb1398_read(chip, PERPH1_INT_RT_STS_REG, &val);
	if (rc < 0)
		return rc;

	ilim = !!(val & DIV2_ILIM_STS);

	if (!is_psy_voter_available(chip))
		return -EAGAIN;

	/* Don't Report WIN_UV for Div2 CC Mode */
	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_ADAPTER_CC_MODE,
			&pval);
	if (rc < 0) {
		dev_err(chip->dev, "Get ADAPTER_CC_MODE failed, rc=%d\n");
		return rc;
	}
	if (pval.intval)
		off_win = !!(val & (DIV2_WIN_OV_STS));
	else
		off_win = !!(val & (DIV2_WIN_OV_STS | DIV2_WIN_UV_STS));

	rc = smb1398_read(chip, PERPH0_INT_RT_STS_REG, &val);
	if (rc < 0)
		return rc;

	rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_SMB_EN_REASON, &pval);
	if (rc < 0)
		return rc;

	irev = !!(val & DIV2_IREV_LATCH_STS);
	tsd = !!(val & TEMP_SHUTDOWN_STS);
	off_vin = (pval.intval == POWER_SUPPLY_CP_WIRELESS) ?
	!!(val & (WLS_IN_OVLO_STS | WLS_IN_UVLO_STS)) :
	!!(val & (USB_IN_OVLO_STS | USB_IN_UVLO_STS));

	*status = ilim << 6 | irev << 3 | tsd << 2 | off_vin << 1 | off_win;

	dev_dbg(chip->dev, "irq_status = 0x%x\n", *status);
	return rc;
}

static int smb1398_get_enable_status(struct smb1398_chip *chip)
{
	int rc = 0;
	u8 val;
	bool switcher_en = false;

	rc = smb1398_read(chip, MODE_STATUS_REG, &val);
	if (rc < 0)
		return rc;

	chip->smb_en = !!(val & SMB_EN);
	chip->switcher_en = !!(val & PRE_EN_DCDC);
	chip->slave_en = !!(val & DIV2_EN_SLAVE);

	rc = smb1398_read(chip, MISC_SL_SWITCH_EN_REG, &val);
	if (rc < 0)
		return rc;

	switcher_en = !!(val & EN_SWITCHER);
	chip->switcher_en = switcher_en && chip->switcher_en;

	dev_dbg(chip->dev, "smb_en = %d, switcher_en = %d, slave_en = %d\n",
			chip->smb_en, chip->switcher_en, chip->slave_en);
	return rc;
}

static int smb1398_set_ovlo(struct smb1398_chip *chip, int ovlo)
{
	int rc;
	int option1, option2;

	rc = smb1398_masked_write(chip, USBIN_OVP_THRESHOLD_REG,
			USBIN_OVP_MASK, ovlo);
	if (rc < 0) {
		dev_err(chip->dev, "write USBIN_OVP_THRESHOLD_REG failed, rc=%d\n", rc);
		return rc;
	}

	if (ovlo == USBIN_OVP_22_2V) {
		option1 = EN_MV_OV_OPTION1_22_15V;
		option2 = EN_HV_OV_OPTION2_22_15V;
	} else {
		option1 = EN_MV_OV_OPTION1_7_2V;
		option2 = EN_HV_OV_OPTION2_7_2V;
	}

	rc = smb1398_masked_write(chip, USBIN_OVLO_THRESHOLD_REG,
			EN_MV_OV_OPTION1_MASK, option1);
	if (rc < 0) {
		dev_err(chip->dev, "write USBIN_OVLO_THRESHOLD_REG failed, rc=%d\n", rc);
		return rc;
	}
	rc = smb1398_masked_write(chip, USBIN_OVLO_THRESHOLD_REG,
			EN_HV_OV_OPTION2_MASK, option2);
	if (rc < 0) {
		dev_err(chip->dev, "write USBIN_OVLO_THRESHOLD_REG failed, rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int smb1398_get_iin_ma(struct smb1398_chip *chip, int *iin_ma)
{
	int rc = 0;
	u8 val;

	rc = smb1398_read(chip, IIN_SS_DAC_TARGET_REG, &val);
	if (rc < 0)
		return rc;

	*iin_ma = (val & IIN_SS_DAC_VALUE_MASK) * IIN_STEP_MA;

	dev_dbg(chip->dev, "get iin_ma = %dmA\n", *iin_ma);
	return rc;
}

static int smb1398_set_iin_ma(struct smb1398_chip *chip, int iin_ma)
{
	int rc = 0;
	u8 val;

	if (iin_ma > DIV2_MAX_ILIM_MA)
		iin_ma = DIV2_MAX_ILIM_MA;

	val = iin_ma / IIN_STEP_MA;
	rc = smb1398_masked_write(chip, IIN_SS_DAC_TARGET_REG,
			IIN_SS_DAC_VALUE_MASK, val);
	if (rc < 0)
		return rc;

	rc = smb1398_masked_write(chip, WLS_IIN_SS_DAC_TARGET_REG,
				IIN_SS_DAC_VALUE_MASK, val);
	if (rc < 0)
		return rc;

	dev_dbg(chip->dev, "set iin_ma = %dmA\n", iin_ma);
	return rc;
}

static int smb1398_slave_switche_en(struct smb1398_chip *chip, bool en)
{
	int rc;
	u16 reg;
	u8 val;

	reg = MISC_SL_SWITCH_EN_REG;
	val = en ? EN_SLAVE : 0;
	rc = smb1398_masked_write(chip, reg, EN_SLAVE, val);
	if (rc < 0) {
		dev_err(chip->dev, "write slave_en failed, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int smb1398_div2_cp_switcher_en(struct smb1398_chip *chip, bool en)
{
	int rc;

	rc = smb1398_masked_write(chip, MISC_SL_SWITCH_EN_REG,
			EN_SWITCHER, en ? EN_SWITCHER : 0);
	if (rc < 0) {
		dev_err(chip->dev, "write SWITCH_EN_REG failed, rc=%d\n", rc);
		return rc;
	}

	chip->switcher_en = en;

	dev_info(chip->dev, "%s switcher\n", en ? "enable" : "disable");
	return rc;
}

static int smb1398_set_ichg_ma(struct smb1398_chip *chip, int ichg_ma)
{
	int rc = 0;
	u8 val;

	if (ichg_ma < 0 || ichg_ma > ICHG_SS_DAC_VALUE_MASK * ICHG_STEP_MA)
		return rc;

	val = ichg_ma / ICHG_STEP_MA;
	rc = smb1398_masked_write(chip, ICHG_SS_DAC_TARGET_REG,
			ICHG_SS_DAC_VALUE_MASK, val);

	dev_dbg(chip->dev, "set ichg %dmA\n", ichg_ma);
	return rc;
}

static int smb1398_get_ichg_ma(struct smb1398_chip *chip, int *ichg_ma)
{
	int rc = 0;
	u8 val;

	rc = smb1398_read(chip, ICHG_SS_DAC_TARGET_REG, &val);
	if (rc < 0)
		return rc;

	*ichg_ma = (val & ICHG_SS_DAC_VALUE_MASK) * ICHG_STEP_MA;

	dev_dbg(chip->dev, "get ichg %dmA\n", *ichg_ma);
	return 0;
}

static int smb1398_set_1s_vout_mv(struct smb1398_chip *chip, int vout_mv)
{
	int rc = 0;
	u8 val;

	if (vout_mv < VOUT_1S_MIN_MV)
		return -EINVAL;

	val = (vout_mv - VOUT_1S_MIN_MV) / VOUT_1S_STEP_MV;

	rc = smb1398_masked_write(chip, VOUT_DAC_TARGET_REG,
			VOUT_DAC_VALUE_MASK, val);
	if (rc < 0)
		return rc;

	return 0;
}

static int smb1398_get_1s_vout_mv(struct smb1398_chip *chip, int *vout_mv)
{
	int rc;
	u8 val;

	rc = smb1398_read(chip, VOUT_DAC_TARGET_REG, &val);
	if (rc < 0)
		return rc;

	*vout_mv = (val & VOUT_DAC_VALUE_MASK) * VOUT_1S_STEP_MV +
		VOUT_1S_MIN_MV;

	return 0;
}

static int smb1398_get_die_temp(struct smb1398_chip *chip, int *temp)
{
	int die_temp_deciC = 0, rc = 0;

	rc =  smb1398_get_enable_status(chip);
	if (rc < 0)
		return rc;

	if (!chip->smb_en)
		return -ENODATA;

	mutex_lock(&chip->die_chan_lock);
	rc = iio_read_channel_processed(chip->die_temp_chan, &die_temp_deciC);
	mutex_unlock(&chip->die_chan_lock);
	if (rc < 0) {
		dev_err(chip->dev, "read die_temp_chan failed, rc=%d\n", rc);
	} else {
		*temp = die_temp_deciC / 100;
		dev_dbg(chip->dev, "get die temp %d\n", *temp);
	}

	return rc;
}

static int smb1398_div2_cp_isns_mode_control(
		struct smb1398_chip *chip, enum isns_mode mode)
{
	int rc = 0;
	u8 mux_sel;

	switch (mode) {
	case ISNS_MODE_STANDBY:
		/* VTEMP */
		mux_sel = SEL_OUT_VTEMP;
		break;
	case ISNS_MODE_OFF:
		/* High-Z */
		mux_sel = SEL_OUT_HIGHZ;
		break;
	case ISNS_MODE_ACTIVE:
		/* IIN_FB */
		mux_sel = SEL_OUT_IIN_FB;
		break;
	default:
		return -EINVAL;
	}

	rc = smb1398_masked_write(chip, SSUPLY_TEMP_CTRL_REG,
			SEL_OUT_TEMP_MAX_MASK, mux_sel);
	if (rc < 0) {
		dev_err(chip->dev, "set SSUPLY_TEMP_CTRL_REG failed, rc=%d\n",
				rc);
		return rc;
	}

	rc = smb1398_masked_write(chip, PERPH0_MISC_CFG2_REG,
			CFG_TEMP_PIN_ITEMP, 0);
	if (rc < 0) {
		dev_err(chip->dev, "set PERPH0_MISC_CFG2_REG failed, rc=%d\n",
				rc);
		return rc;
	}

	return 0;
}

static inline int calculate_div2_cp_isns_ua(int temp)
{
	/* ISNS = (2850 + (0.0034 * thermal_reading) / 0.32) * 1000 uA */
	return (2850 * 1000 + div_s64((s64)temp * 340, 32));
}

static bool is_cps_available(struct smb1398_chip *chip)
{
	if (chip->div2_cp_slave_psy)
		return true;

	chip->div2_cp_slave_psy = power_supply_get_by_name("cp_slave");
	if (chip->div2_cp_slave_psy)
		return true;

	return false;
}

static int smb1398_div2_cp_get_master_isns(
		struct smb1398_chip *chip, int *isns_ua)
{
	union power_supply_propval pval = {0};
	int rc = 0, temp;

	rc = smb1398_get_enable_status(chip);
	if (rc < 0)
		return rc;

	if (!chip->smb_en)
		return -ENODATA;

	/*
	 * Follow this procedure to read master CP ISNS:
	 *   set slave CP TEMP_MUX to HighZ;
	 *   set master CP TEMP_MUX to IIN_FB;
	 *   read corresponding ADC channel in Kekaha;
	 *   set master CP TEMP_MUX to VTEMP;
	 *   set slave CP TEMP_MUX to HighZ;
	 */
	mutex_lock(&chip->die_chan_lock);
	if (is_cps_available(chip)) {
		pval.intval = ISNS_MODE_OFF;
		rc = power_supply_set_property(chip->div2_cp_slave_psy,
				POWER_SUPPLY_PROP_CURRENT_CAPABILITY, &pval);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set slave ISNS_MODE_OFF, rc=%d\n",
					rc);
			goto unlock;
		}
	}

	rc = smb1398_div2_cp_isns_mode_control(chip, ISNS_MODE_ACTIVE);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set master ISNS_MODE_ACTIVE, rc=%d\n",
				rc);
		goto unlock;
	}

	rc = iio_read_channel_processed(chip->die_temp_chan, &temp);
	if (rc < 0) {
		dev_err(chip->dev, "Read die_temp_chan failed, rc=%d\n", rc);
		goto unlock;
	}

	rc = smb1398_div2_cp_isns_mode_control(chip, ISNS_MODE_STANDBY);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set master ISNS_MODE_STANDBY, rc=%d\n",
				rc);
		goto unlock;
	}

	if (is_cps_available(chip)) {
		pval.intval = ISNS_MODE_OFF;
		rc = power_supply_set_property(chip->div2_cp_slave_psy,
				POWER_SUPPLY_PROP_CURRENT_CAPABILITY, &pval);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set slave ISNS_MODE_OFF, rc=%d\n",
					rc);
	}
unlock:
	mutex_unlock(&chip->die_chan_lock);
	if (rc >= 0) {
		*isns_ua = calculate_div2_cp_isns_ua(temp);
		dev_info(chip->dev, "master isns = %duA\n", *isns_ua);
	}

	return rc;
}

static int smb1398_div2_cp_get_slave_isns(
		struct smb1398_chip *chip, int *isns_ua)
{
	union power_supply_propval pval = {0};
	int temp = 0, rc;

	if (!is_cps_available(chip)) {
		*isns_ua = 0;
		return 0;
	}

	rc = smb1398_get_enable_status(chip);
	if (rc < 0)
		return rc;

	if (!chip->smb_en || !chip->slave_en)
		return -ENODATA;

	/*
	 * Follow this procedure to read slave CP ISNS:
	 *   set master CP TEMP_MUX to HighZ;
	 *   set slave CP TEMP_MUX to IIN_FB;
	 *   read corresponding ADC channel in Kekaha;
	 *   set slave CP TEMP_MUX to HighZ;
	 *   set master CP TEMP_MUX to VTEMP;
	 */
	mutex_lock(&chip->die_chan_lock);
	rc = smb1398_div2_cp_isns_mode_control(chip, ISNS_MODE_OFF);
	if (rc < 0) {
		dev_err(chip->dev, "set master ISNS_MODE_OFF failed, rc=%d\n",
				rc);
		goto unlock;
	}

	pval.intval = ISNS_MODE_ACTIVE;
	rc = power_supply_set_property(chip->div2_cp_slave_psy,
			POWER_SUPPLY_PROP_CURRENT_CAPABILITY, &pval);
	if (rc < 0) {
		dev_err(chip->dev, "set slave ISNS_MODE_ACTIVE failed, rc=%d\n",
				rc);
		goto unlock;
	}

	rc = iio_read_channel_processed(chip->die_temp_chan, &temp);
	if (rc < 0) {
		dev_err(chip->dev, "Read die_temp_chan failed, rc=%d\n", rc);
		goto unlock;
	}

	pval.intval = ISNS_MODE_OFF;
	rc = power_supply_set_property(chip->div2_cp_slave_psy,
			POWER_SUPPLY_PROP_CURRENT_CAPABILITY, &pval);
	if (rc < 0) {
		dev_err(chip->dev, "Set slave ISNS_MODE_OFF failed, rc=%d\n",
				 rc);
		goto unlock;
	}

	rc = smb1398_div2_cp_isns_mode_control(chip, ISNS_MODE_STANDBY);
	if (rc < 0) {
		dev_err(chip->dev, "Set master ISNS_MODE_STANDBY failed, rc=%d\n",
				rc);
		goto unlock;
	}
unlock:
	mutex_unlock(&chip->die_chan_lock);

	if (rc >= 0) {
		*isns_ua = calculate_div2_cp_isns_ua(temp);
		dev_info(chip->dev, "slave isns = %duA\n", *isns_ua);
	}

	return rc;
}

static void smb1398_toggle_switcher(struct smb1398_chip *chip)
{
	int rc = 0;

	/*
	 * Disable DIV2_ILIM detection before toggling the switcher
	 * to prevent any ILIM interrupt storm while the toggling
	 */
	rc = smb1398_masked_write(chip, DIV2_CURRENT_REG, DIV2_EN_ILIM_DET, 0);
	if (rc < 0)
		dev_err(chip->dev, "Disable EN_ILIM_DET failed, rc=%d\n", rc);

	vote(chip->div2_cp_disable_votable, SWITCHER_TOGGLE_VOTER, true, 0);

	/* Delay for toggling switcher */
	usleep_range(20, 30);
	vote(chip->div2_cp_disable_votable, SWITCHER_TOGGLE_VOTER, false, 0);

	rc = smb1398_masked_write(chip, DIV2_CURRENT_REG,
			DIV2_EN_ILIM_DET, DIV2_EN_ILIM_DET);
	if (rc < 0)
		dev_err(chip->dev, "Disable EN_ILIM_DET failed, rc=%d\n", rc);
}

static int smb1398_div2_cp_enable_ilim(struct smb1398_chip *chip, bool enable)
{
	int rc = 0;
	rc = smb1398_masked_write(chip, DIV2_CURRENT_REG, DIV2_EN_ILIM_DET,
			enable ? DIV2_EN_ILIM_DET : 0);
	if (rc < 0)
		dev_err(chip->dev, "%s EN_ILIM_DET failed, rc=%d\n",
				enable ? "Enable" : "Disable", rc);
	return rc;
}

static int smb1398_div2_cp_config_ovp(struct smb1398_chip *chip)
{
	int target_ovp_uv = 0, rc = 0, i;
	union power_supply_propval pval = {0};
	u8 in_passthrough = 0x00;

	if (!smb1398_is_rev3(chip))
		return 0;

	if (!chip->batt_psy) {
		chip->batt_psy = power_supply_get_by_name("battery");
		if (!chip->batt_psy) {
			dev_dbg(chip->dev, "Couldn't find battery psy\n");
			return -EPROBE_DEFER;
		}
	}

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &pval);
	if (rc < 0) {
		dev_err(chip->dev, "get battery FV failed, rc=%d\n", rc);
		return rc;
	}

	rc = smb1398_read(chip, MISC_CFG0_REG, &in_passthrough);
	if (rc < 0) {
		dev_err(chip->dev, "Read passthrough mode bit failed, rc=%d\n", rc);
		return rc;
	}
	in_passthrough = in_passthrough & CFG_ITLGS_2P_BIT;

	if (in_passthrough)
		target_ovp_uv = pval.intval;
	else
		target_ovp_uv = 2 * pval.intval;//Div2CP mode

	if ((target_ovp_uv > int_ovp_levels_uv[3].voltage_uv)
		|| (target_ovp_uv > ext_ovp_levels_uv[3].voltage_uv)) {
		dev_err(chip->dev, "Battery FV exceeds max OVP\n");
		return -EINVAL;
	}

	for (i = 0; i < 4; i++) {
		if (int_ovp_levels_uv[i].voltage_uv > target_ovp_uv) {
			rc = smb1398_masked_write(chip, SSUPPLY_CFG0,
				EN_HV_OV_OPTION2 | EN_MV_OV_OPTION, int_ovp_levels_uv[i].reg_config);
			if (rc < 0) {
				dev_err(chip->dev, "Write SSUPPLY_CFG0 failed, rc=%d\n", rc);
				return rc;
			}
			dev_dbg(chip->dev, "Master OVP Configured: internal OVP=%d mV\n",
					int_ovp_levels_uv[i].voltage_uv/1000);
			break;
		}
	}

	for (i = 0; i < 4; i++) {
		if (ext_ovp_levels_uv[i].voltage_uv > target_ovp_uv) {
			rc = smb1398_masked_write(chip, PERPH1_LOCK_SPARE,
				EXT_OVPFET_MASK, ext_ovp_levels_uv[i].reg_config);
			if (rc < 0) {
				dev_err(chip->dev, "Write PERPH1_LOCK_SPARE failed, rc=%d\n", rc);
				return rc;
			}
			dev_dbg(chip->dev, "Master OVP Configured: external OVP=%d mV\n",
				ext_ovp_levels_uv[i].voltage_uv/1000);
			break;
		}
	}

	if (is_cps_available(chip)) {
		pval.intval = 1;
		rc = power_supply_set_property(chip->div2_cp_slave_psy,
			POWER_SUPPLY_PROP_CP_OVP_CONFIG, &pval);
			if (rc < 0) {
				dev_err(chip->dev, "Failed to configure slave OVP, rc=%d\n", rc);
				return rc;
			}
	}

	return rc;
}

static int smb1398_div2_cp_slave_config_ovp(struct smb1398_chip *chip)
{
	int target_ovp_uv = 0, rc = 0, i;
	union power_supply_propval pval = {0};
	u8 in_passthrough = 0x00;

	if (!smb1398_is_rev3(chip))
		return 0;

	if (!chip->batt_psy) {
		chip->batt_psy = power_supply_get_by_name("battery");
		if (!chip->batt_psy) {
			dev_dbg(chip->dev, "Couldn't find battery psy\n");
			return -EPROBE_DEFER;
		}
	}

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &pval);
	if (rc < 0) {
		dev_err(chip->dev, "get battery FV failed, rc=%d\n", rc);
		return rc;
	}

	rc = smb1398_read(chip, MISC_CFG0_REG, &in_passthrough);
	if (rc < 0) {
		dev_err(chip->dev, "Read Slave passthrough mode bit failed, rc=%d\n", rc);
		return rc;
	}
	in_passthrough = in_passthrough & CFG_ITLGS_2P_BIT;

	if (in_passthrough)
		target_ovp_uv = pval.intval;
	else
		target_ovp_uv = 2 * pval.intval;//Div2CP mode

	if ((target_ovp_uv > int_ovp_levels_uv[3].voltage_uv)
		|| (target_ovp_uv > ext_ovp_levels_uv[3].voltage_uv)) {
		dev_err(chip->dev, "Battery FV exceeds max OVP\n");
		return -EINVAL;
	}

	for (i = 0; i < 4; i++) {
		if (int_ovp_levels_uv[i].voltage_uv > target_ovp_uv) {
			rc = smb1398_masked_write(chip, SSUPPLY_CFG0,
				EN_HV_OV_OPTION2 | EN_MV_OV_OPTION, int_ovp_levels_uv[i].reg_config);
			if (rc < 0) {
				dev_err(chip->dev, "Write Slave SSUPPLY_CFG0 failed, rc=%d\n", rc);
				return rc;
			}
			dev_dbg(chip->dev, "Slave OVP Configured: internal OVP=%d mV\n",
				int_ovp_levels_uv[i].voltage_uv/1000);
			break;
		}
	}

	for (i = 0; i < 4; i++) {
		if (ext_ovp_levels_uv[i].voltage_uv > target_ovp_uv) {
			rc = smb1398_masked_write(chip, PERPH1_LOCK_SPARE,
				EXT_OVPFET_MASK, ext_ovp_levels_uv[i].reg_config);
			if (rc < 0) {
				dev_err(chip->dev, "Write PERPH1_LOCK_SPARE failed, rc=%d\n", rc);
				return rc;
			}
			dev_dbg(chip->dev, "Master OVP Configured: external OVP=%d mV\n",
				ext_ovp_levels_uv[i].voltage_uv/1000);
			break;
		}
	}
	chip->target_ovp_uv = target_ovp_uv;

	return rc;
}

static int smb1398_get_ocp(struct smb1398_chip *chip)
{
	int rc;
	u8 val;

	rc = smb1398_read(chip, CFG_3LVL_BK_CURRENT, &val);
	if (rc < 0)
		dev_err(chip->dev, "faield to read ocp status=%d\n", rc);

	val = !!(val & CFG_HSON_SEL);

	return val;
}

static int smb1398_set_ocp(struct smb1398_chip *chip, bool enable)
{
	int rc;

	rc = smb1398_masked_write(chip, CFG_3LVL_BK_CURRENT,
			CFG_HSON_SEL, enable ? CFG_HSON_SEL : 0);

	return rc;
}

static int smb1398_ovp_ignore_mode(struct smb1398_chip *chip, bool enable)
{
	int rc;

	rc = smb1398_masked_write(chip, CFG_OVP_IGNORE_UVLO_REG,
			OVP_IGNORE_UVLO_MASK, OVP_IGNORE_UVLO_DIS);
	udelay(25);
	rc = smb1398_masked_write(chip, CFG_OVP_IGNORE_UVLO_REG,
			OVP_IGNORE_UVLO_MASK, OVP_IGNORE_UVLO_EN);
	return rc;
}

static int smb1398_passthrough_exit(struct smb1398_chip *chip)
{
	int rc = 0;
	union power_supply_propval pval = {0};

	/*
	 * Keep SMB_EN low when transitioning out
	 * of Passthrough Mode
	 */
	vote(chip->smb_override_votable, PASSTHROUGH_VOTER, true, 0);
	vote(chip->div2_cp_disable_votable, PASSTHROUGH_VOTER, true, 0);
	rc = smb1398_masked_write(chip, SDCDC_1,
			EN_CHECK_CFLY_SSDONE_BIT, EN_CHECK_CFLY_SSDONE_BIT);
	rc = smb1398_masked_write(chip, MISC_CFG0_REG,
			CFG_ITLGS_2P_BIT, 0);//Disable Passthrough
	if (is_cps_available(chip)) {
		pval.intval = 0;
		rc = power_supply_set_property(chip->div2_cp_slave_psy,
				POWER_SUPPLY_PROP_CP_PASSTHROUGH_MODE, &pval);
	}
	rc = smb1398_div2_cp_config_ovp(chip);
	rc = smb1398_div2_cp_enable_ilim(chip, true);//Re-enable ILIM
	vote(chip->smb_override_votable, PASSTHROUGH_VOTER, false, 0);
	vote(chip->div2_cp_disable_votable, PASSTHROUGH_VOTER, false, 0);

	dev_err(chip->dev, "DEBUG: smb1398_passthrough_exit, rc=%d\n", rc);


	return rc;
}

#define PASSTHROUGH_DELTA_OVERVOLTAGE_UV	500000
static bool is_vbus_ok_for_passthrough(struct smb1398_chip *chip)
{
	int rc;
	union power_supply_propval pval = {0};
	long vbus_uv, vbat_uv;
	int vbus_ok;

	if (!chip->usb_psy) {
		chip->usb_psy = power_supply_get_by_name("usb");
		if (!chip->usb_psy)
			return false;
	}
	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW,
			&pval);
	if (rc < 0) {
		dev_err(chip->dev, "Get USB VOLTAGE_NOW failed, rc=%d\n");
		return false;
	}
	vbus_uv = pval.intval;

	if (vbus_uv < 5500000)
		return true;

	if (!chip->batt_psy) {
		chip->batt_psy = power_supply_get_by_name("battery");
		if (!chip->batt_psy)
			return false;
	}
	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW,
			&pval);
	if (rc < 0) {
		dev_err(chip->dev, "Get BATT VOLTAGE_NOW failed, rc=%d\n");
		return false;
	}

	vbat_uv = pval.intval;

	vbus_ok = (vbus_uv + 100000) > vbat_uv ? true : false;
	dev_dbg(chip->dev, "vbat_uv=%d, vbus_uv:%d, vbus_ok:%d\n", vbat_uv, vbus_uv, vbus_ok);

	return vbus_ok;
}

static enum power_supply_property div2_cp_master_props[] = {
	POWER_SUPPLY_PROP_CP_STATUS1,
	POWER_SUPPLY_PROP_CP_STATUS2,
	POWER_SUPPLY_PROP_CP_ENABLE,
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_CP_SWITCHER_EN,
	POWER_SUPPLY_PROP_CP_DIE_TEMP,
	POWER_SUPPLY_PROP_CP_ISNS,
	POWER_SUPPLY_PROP_CP_ISNS_SLAVE,
	POWER_SUPPLY_PROP_CP_TOGGLE_SWITCHER,
	POWER_SUPPLY_PROP_CP_IRQ_STATUS,
	POWER_SUPPLY_PROP_CP_ILIM,
	POWER_SUPPLY_PROP_RESET_DIV_2_MODE,
	POWER_SUPPLY_PROP_CHIP_VERSION,
	POWER_SUPPLY_PROP_PARALLEL_MODE,
	POWER_SUPPLY_PROP_PARALLEL_OUTPUT_MODE,
	POWER_SUPPLY_PROP_MIN_ICL,
	POWER_SUPPLY_PROP_CP_WIN_OV,
	POWER_SUPPLY_PROP_CP_PASSTHROUGH_MODE,
	POWER_SUPPLY_PROP_CP_PASSTHROUGH_CONFIG,
	POWER_SUPPLY_PROP_CP_OCP_CONFIG,
};

#define FP_FET_IREV_THRESHOLD_UA 1000000
#define WLS_WIN_OV_EN_THRESHOLD_UA 1600000
static int smb1398_charge_get_wireless_adapter_type(struct smb1398_chip *chip, int *adpater_type);
static int div2_cp_master_get_prop(struct power_supply *psy,
				enum power_supply_property prop,
				union power_supply_propval *val)
{
	struct smb1398_chip *chip = power_supply_get_drvdata(psy);
	int rc = 0, ilim_ma, temp, isns_ua;
	u8 status, data, in_passthrough;
	switch (prop) {
	case POWER_SUPPLY_PROP_CP_STATUS1:
		rc = smb1398_div2_cp_get_status1(chip, &status);
		if (!rc)
			val->intval = status;
		break;
	case POWER_SUPPLY_PROP_CP_STATUS2:
		rc = smb1398_div2_cp_get_status2(chip, &status);
		if (!rc)
			val->intval = status;
		break;
	case POWER_SUPPLY_PROP_CP_ENABLE:
		rc = smb1398_get_enable_status(chip);
		if (!rc)
			val->intval = chip->smb_en &&
				!get_effective_result(
						chip->div2_cp_disable_votable);
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		rc = smb1398_read(chip, REVID_REVISION4, &data);
		if (!rc)
			val->intval = true;
		else
			val->intval = false;
		break;
	case POWER_SUPPLY_PROP_CP_SWITCHER_EN:
		rc = smb1398_get_enable_status(chip);
		if (!rc)
			val->intval = chip->switcher_en;
		break;
	case POWER_SUPPLY_PROP_CP_ISNS:
		rc = smb1398_div2_cp_get_master_isns(chip, &isns_ua);
		if (rc >= 0) {
			val->intval = isns_ua;
			schedule_work(&chip->isns_process_work);
		}
		break;
	case POWER_SUPPLY_PROP_CP_ISNS_SLAVE:
		rc = smb1398_div2_cp_get_slave_isns(chip, &isns_ua);
		if (rc >= 0)
			val->intval = isns_ua;
		break;
	case POWER_SUPPLY_PROP_CP_TOGGLE_SWITCHER:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CP_DIE_TEMP:
		if (!chip->in_suspend) {
			rc = smb1398_get_die_temp(chip, &temp);
			if ((rc >= 0) && (temp <= THERMAL_SUSPEND_DECIDEGC))
				chip->die_temp = temp;
		}

		if (chip->die_temp != -ENODATA)
			val->intval = chip->die_temp;
		else
			rc = -ENODATA;
		break;
	case POWER_SUPPLY_PROP_CP_IRQ_STATUS:
		val->intval = chip->div2_irq_status;
		rc = smb1398_div2_cp_get_irq_status(chip, &status);
		if (!rc)
			val->intval |= status;
		break;
	case POWER_SUPPLY_PROP_CP_ILIM:
		if (is_cps_available(chip)) {
			if (chip->div2_cp_ilim_votable)
				val->intval = get_effective_result(
						chip->div2_cp_ilim_votable);
			/* for wireless charging, enlage the ilimt*/
			if (is_for_wireless_charging(chip)) {
				val->intval *= chip->ilim_enlarge_pct;
				val->intval /= 100;
			}
		} else {
			rc = smb1398_get_iin_ma(chip, &ilim_ma);
			if (!rc)
				val->intval = ilim_ma * 1000;
		}
		break;
	case POWER_SUPPLY_PROP_RESET_DIV_2_MODE:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHIP_VERSION:
		rc = smb1398_read(chip, REVID_REVISION4, &data);
		if (!rc)
			val->intval = data;
		break;
	case POWER_SUPPLY_PROP_PARALLEL_MODE:
		/* USBIN only */
		val->intval = POWER_SUPPLY_PL_USBIN_USBIN;
		break;
	case POWER_SUPPLY_PROP_PARALLEL_OUTPUT_MODE:
		/* VBAT only */
		val->intval = POWER_SUPPLY_PL_OUTPUT_VBAT;
		break;
	case POWER_SUPPLY_PROP_MIN_ICL:
		val->intval = chip->div2_cp_min_ilim_ua;
		break;
	case POWER_SUPPLY_PROP_CP_WIN_OV:
		rc = smb1398_read(chip, MISC_CFG0_REG, &in_passthrough);
		if (rc < 0) {
			dev_err(chip->dev, "Read passthrough mode bit failed, rc=%d\n", rc);
			return rc;
		}
		in_passthrough = in_passthrough & CFG_ITLGS_2P_BIT;
		rc = smb1398_read(chip, DIV2_PROTECTION_REG, &data);
		if (!rc) {
			if (in_passthrough && smb1398_is_rev3(chip))
				val->intval = passthrough_win_ov_table_uv[data];
			else
				val->intval = div2cp_win_ov_table_uv[data];
		}
		break;
	case POWER_SUPPLY_PROP_CP_PASSTHROUGH_MODE:
		val->intval = chip->passthrough_mode_allowed ?
				!get_effective_result(chip->passthrough_mode_disable_votable) : 0;
		break;
	case POWER_SUPPLY_PROP_CP_PASSTHROUGH_CONFIG:
		val->intval = !!get_effective_result(chip->passthrough_mode_disable_votable);
		break;
	case POWER_SUPPLY_PROP_CP_OCP_CONFIG:
		val->intval = smb1398_get_ocp(chip);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int div2_cp_master_set_prop(struct power_supply *psy,
				enum power_supply_property prop,
				const union power_supply_propval *val)
{
	struct smb1398_chip *chip = power_supply_get_drvdata(psy);
	int rc = 0;
	union power_supply_propval pval = {0};

	switch (prop) {
	case POWER_SUPPLY_PROP_CP_ENABLE:
		vote(chip->div2_cp_disable_votable,
				USER_VOTER, !val->intval, 0);
		break;
	case POWER_SUPPLY_PROP_CP_TOGGLE_SWITCHER:
		if (!!val->intval)
			smb1398_toggle_switcher(chip);
		break;
	case POWER_SUPPLY_PROP_CP_IRQ_STATUS:
		chip->div2_irq_status = val->intval;
		break;
	case POWER_SUPPLY_PROP_CP_ILIM:
		if (chip->div2_cp_ilim_votable)
			vote_override(chip->div2_cp_ilim_votable,
					CC_MODE_VOTER, (val->intval > 0), val->intval);
		break;
	case POWER_SUPPLY_PROP_RESET_DIV_2_MODE:
		rc = smb1398_ovp_ignore_mode(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CP_PASSTHROUGH_MODE:
		vote(chip->passthrough_mode_disable_votable, USER_VOTER, !val->intval, 0);
		break;
	case POWER_SUPPLY_PROP_CP_PASSTHROUGH_CONFIG:
		dev_info(chip->dev, "set POWER_SUPPLY_PROP_CP_PASSTHROUGH_CONFIG to val->intval:%d\n", val->intval);
		if (chip->passthrough_mode_allowed && (val->intval > 0 ?
					(smb1398_is_rev3(chip) || is_vbus_ok_for_passthrough(chip))
					: val->intval == 0)) {
			if (val->intval == 1) {
				rc = smb1398_div2_cp_config_ovp(chip);
				rc = smb1398_masked_write(chip, MISC_CFG0_REG,
						CFG_ITLGS_2P_BIT, 0);//Disable Passthrough bit before enabling SMB
			}
			vote(chip->smb_override_votable, PASSTHROUGH_VOTER, !val->intval, 0);
			vote(chip->div2_cp_disable_votable, PASSTHROUGH_VOTER, !val->intval, 0);
			if (val->intval == 0) {
				smb1398_masked_write(chip, PERPH0_MISC_CFG0,
						CFG_PRE_REG_2S, 0);
				vote(chip->div2_cp_slave_disable_votable, DELAY_OPEN_VOTER, true, 0);
				/* ILIM non-functional for Passthrough - disable it */
				rc = smb1398_div2_cp_enable_ilim(chip, false);
				/* Temporarily enable Passthrough bit for WIN_OV read and OVP setting */
				rc = smb1398_masked_write(chip, MISC_CFG0_REG,
						CFG_ITLGS_2P_BIT, CFG_ITLGS_2P_BIT);
			} else if (val->intval == 1) {
				rc = smb1398_masked_write(chip, CFG_3LVL_BK_CURRENT,
						CFG_HSON_SEL, 0);//Disable OCP
				rc = smb1398_masked_write(chip, SDCDC_1,
						EN_CHECK_CFLY_SSDONE_BIT, 0);
				rc = smb1398_masked_write(chip, MISC_CFG0_REG,
					CFG_ITLGS_2P_BIT, CFG_ITLGS_2P_BIT);//Enable Passthrough
				if (is_cps_available(chip)) {
					pval.intval = 1;
					rc = power_supply_set_property(chip->div2_cp_slave_psy,
							POWER_SUPPLY_PROP_CP_PASSTHROUGH_MODE, &pval);
				}
				dev_info(chip->dev, "DEBUG: Passthrough Entry Config, rc=%d\n", rc);
			}
		} else
			rc = -EINVAL;//Passthrough only allowed for CC mode
		dev_info(chip->dev, "set rc:%d, val->intavl:%d\n", rc, val->intval);

		if (rc < 0 || val->intval < 0) {
			vote(chip->passthrough_mode_disable_votable, USER_VOTER, true, 0);
			vote(chip->div2_cp_slave_disable_votable, DELAY_OPEN_VOTER, false, 0);
			smb1398_set_ocp(chip, false);
		} else
			vote(chip->passthrough_mode_disable_votable, USER_VOTER, false, 0);
		break;
	case POWER_SUPPLY_PROP_CP_OCP_CONFIG:
		rc = smb1398_set_ocp(chip, val->intval);
		break;
	default:
		dev_err(chip->dev, "setprop %d is not supported\n", prop);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int div2_cp_master_prop_is_writeable(struct power_supply *psy,
					enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CP_ENABLE:
	case POWER_SUPPLY_PROP_CP_TOGGLE_SWITCHER:
	case POWER_SUPPLY_PROP_CP_IRQ_STATUS:
	case POWER_SUPPLY_PROP_CP_ILIM:
	case POWER_SUPPLY_PROP_RESET_DIV_2_MODE:
	case POWER_SUPPLY_PROP_CP_PASSTHROUGH_CONFIG:
	case POWER_SUPPLY_PROP_CP_PASSTHROUGH_MODE:
		return 1;
	default:
		break;
	}

	return 0;
}

static struct power_supply_desc div2_cp_master_desc = {
	.name			= "charge_pump_master",
	.type			= POWER_SUPPLY_TYPE_CHARGE_PUMP,
	.properties		= div2_cp_master_props,
	.num_properties		= ARRAY_SIZE(div2_cp_master_props),
	.get_property		= div2_cp_master_get_prop,
	.set_property		= div2_cp_master_set_prop,
	.property_is_writeable	= div2_cp_master_prop_is_writeable,
};

static int smb1398_init_div2_cp_master_psy(struct smb1398_chip *chip)
{
	struct power_supply_config div2_cp_master_psy_cfg = {};
	int rc = 0;

	div2_cp_master_psy_cfg.drv_data = chip;
	div2_cp_master_psy_cfg.of_node = chip->dev->of_node;

	chip->div2_cp_master_psy = devm_power_supply_register(chip->dev,
			&div2_cp_master_desc, &div2_cp_master_psy_cfg);
	if (IS_ERR(chip->div2_cp_master_psy)) {
		rc = PTR_ERR(chip->div2_cp_master_psy);
		dev_err(chip->dev, "Register div2_cp_master power supply failed, rc=%d\n",
				rc);
		return rc;
	}

	return 0;
}

static bool is_psy_voter_available(struct smb1398_chip *chip)
{
	if (!chip->batt_psy) {
		chip->batt_psy = power_supply_get_by_name("battery");
		if (!chip->batt_psy) {
			dev_dbg(chip->dev, "Couldn't find battery psy\n");
			return false;
		}
	}

	if (!chip->usb_psy) {
		chip->usb_psy = power_supply_get_by_name("usb");
		if (!chip->usb_psy) {
			dev_dbg(chip->dev, "Couldn't find USB psy\n");
			return false;
		}
	}

	if (!chip->dc_psy) {
		chip->dc_psy = power_supply_get_by_name("dc");
		if (!chip->dc_psy) {
			dev_dbg(chip->dev, "Couldn't find DC psy\n");
			return false;
		}
	}

	if (!chip->wls_psy) {
			chip->wls_psy = power_supply_get_by_name("wireless");
			if (!chip->wls_psy) {
				dev_dbg(chip->dev, "Couldn't find Wireless psy\n");
				return false;
		}
	}
	if (!chip->main_psy) {
			chip->main_psy = power_supply_get_by_name("main");
			if (!chip->main_psy) {
				dev_dbg(chip->dev, "Couldn't find main psy\n");
				return false;
		}
	}

	if (!chip->div2_cp_master_psy) {
		return false;
	}

	if (!chip->fcc_votable) {
		chip->fcc_votable = find_votable("FCC");
		if (!chip->fcc_votable) {
			dev_dbg(chip->dev, "Couldn't find FCC voltable\n");
			return false;
		}
	}

	if (!chip->usb_icl_votable) {
		chip->usb_icl_votable = find_votable("USB_ICL");
		if (!chip->usb_icl_votable) {
			dev_dbg(chip->dev, "Couldn't find USB_ICL voltable\n");
			return false;
		}
	}

	if (!chip->fv_votable) {
		if (chip->batt_2s_chg)
			chip->fv_votable = find_votable("BBC_FV");
		else
			chip->fv_votable = find_votable("FV");
		if (!chip->fv_votable) {
			dev_dbg(chip->dev, "Couldn't find FV voltable\n");
			return false;
		}
	}

	if (!chip->fcc_main_votable) {
		chip->fcc_main_votable = find_votable("FCC_MAIN");
		if (!chip->fcc_main_votable) {
			dev_dbg(chip->dev, "Couldn't find FCC_MAIN voltable\n");
			return false;
		}
	}

	if (!chip->smb_override_votable) {
		chip->smb_override_votable = find_votable("SMB_EN_OVERRIDE");
		if (!chip->smb_override_votable) {
			dev_dbg(chip->dev, "Couldn't find SMB_EN_OVERRIDE votable\n");
			return false;
		}
	}

	return true;
}

static bool is_cutoff_soc_reached(struct smb1398_chip *chip)
{
	int rc;
	union power_supply_propval pval = {0};

	if (!chip->batt_psy)
		goto err;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (rc < 0) {
		dev_err(chip->dev, "get battery soc failed, rc=%d\n", rc);
		goto err;
	}
	if (is_for_wireless_charging(chip)) {
		if (pval.intval >= chip->wls_max_cutoff_soc)
			return true;
	} else {
		if (pval.intval >= chip->max_cutoff_soc)
			return true;
	}
err:
	return false;
}

static bool is_adapter_in_cc_mode(struct smb1398_chip *chip)
{
	int rc;
	union power_supply_propval pval = {0};

	if (!chip->usb_psy) {
		chip->usb_psy = power_supply_get_by_name("usb");
		if (!chip->usb_psy)
			return false;
	}
	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_ADAPTER_CC_MODE,
			&pval);
	if (rc < 0) {
		dev_err(chip->dev, "Get ADAPTER_CC_MODE failed, rc=%d\n");
		return rc;
	}

	return !!pval.intval;
}

static int smb1398_awake_vote_cb(struct votable *votable,
		void *data, int awake, const char *client)
{
	struct smb1398_chip *chip = (struct smb1398_chip *)data;

	if (awake)
		pm_stay_awake(chip->dev);
	else
		pm_relax(chip->dev);

	return 0;
}

static bool is_for_wireless_charging(struct smb1398_chip *chip)
{
	int rc = 0;
	union power_supply_propval pval = {0};

	if (!chip->usb_psy)
		return false;

	rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_SMB_EN_REASON, &pval);
	if (rc < 0) {
		dev_err(chip->dev, "Get SMB_EN_REASON failed, rc=%d\n",
				rc);
		return false;
	}
	return (pval.intval == POWER_SUPPLY_CP_WIRELESS);
}

static int smb1398_div2_cp_disable_vote_cb(struct votable *votable,
		void *data, int disable, const char *client)
{
	struct smb1398_chip *chip = (struct smb1398_chip *)data;
	int rc = 0;
	bool is_for_wireless = false;
	int i;

	if (!is_psy_voter_available(chip) || chip->in_suspend)
		return -EAGAIN;

	if (disable)
		vote(chip->div2_cp_slave_disable_votable, MAIN_DISABLE_VOTER,
			true, 0);

	rc = smb1398_div2_cp_switcher_en(chip, !disable);
	if (rc < 0) {
		dev_err(chip->dev, "%s switcher failed, rc=%d\n",
				!!disable ? "disable" : "enable", rc);
		return rc;
	}
	is_for_wireless = is_for_wireless_charging(chip);

	if (!disable)
		vote(chip->div2_cp_slave_disable_votable, MAIN_DISABLE_VOTER,
				false, 0);

	if (chip->div2_cp_master_psy)
		power_supply_changed(chip->div2_cp_master_psy);

	pr_err("%s: status %d\n", __func__, disable);

	for (i = 0; i < NUM_IRQS; i++) {
		if (disable)
			disable_irq_nosync(chip->irqs[i]);
		else
			enable_irq(chip->irqs[i]);
	}

	/* if cp is disable by toggle voter, do not clean wireless cp status */
	if (is_for_wireless && disable && client && strcmp(client, SWITCHER_TOGGLE_VOTER) != 0) {
		schedule_work(&chip->wireless_close_work);
	}

	/* if cp is disable by toggle voter, do not clean wireless cp status */
	if (chip->cp_for_wireless && disable && client && strcmp(client, SWITCHER_TOGGLE_VOTER) != 0) {
		chip->cp_for_wireless = false;
		schedule_work(&chip->cp_close_work);
	}

	if (is_for_wireless && !disable && client && strcmp(client, SWITCHER_TOGGLE_VOTER) != 0) {
		chip->cp_for_wireless = true;
		schedule_work(&chip->wireless_open_work);
	}
	return 0;
}

static int smb1398_div2_cp_slave_disable_vote_cb(struct votable *votable,
		void *data, int disable, const char *client)
{
	struct smb1398_chip *chip = (struct smb1398_chip *)data;
	union power_supply_propval pval = {0};
	u16 reg;
	u8 val;
	int rc, ilim_ua;

	if (!is_cps_available(chip))
		return -ENODEV;

	/* Re-distribute ILIM to Master CP when Slave is disabled */
	if (disable && (chip->div2_cp_ilim_votable)) {
		ilim_ua = get_effective_result_locked(
				chip->div2_cp_ilim_votable);
		if (ilim_ua > DIV2_MAX_ILIM_UA)
			ilim_ua = DIV2_MAX_ILIM_UA;

		rc = smb1398_set_iin_ma(chip, ilim_ua / 1000);
		if (rc < 0) {
			dev_err(chip->dev, "set CP master ilim failed, rc=%d\n",
					rc);
			return rc;
		}
		dev_dbg(chip->dev, "slave disabled, restore master CP ilim to %duA\n",
				ilim_ua);
	}

	if (disable) {
		pval.intval = !disable;
		rc = power_supply_set_property(chip->div2_cp_slave_psy,
				POWER_SUPPLY_PROP_CP_ENABLE, &pval);
		if (rc < 0) {
			dev_err(chip->dev, "%s slave switcher failed, rc=%d\n",
					!!disable ? "disable" : "enable", rc);
			return rc;
		}
	}


	/* Enable/disable SYNC driver before enabling/disabling slave */
	reg = MISC_CFG0_REG;
	if (chip->standalone_mode)
		val = DIS_SYNC_DRV_BIT;
	else
		val = !!disable ? DIS_SYNC_DRV_BIT : 0;

	rc = smb1398_masked_write(chip, reg, DIS_SYNC_DRV_BIT, val);
	if (rc < 0) {
		dev_err(chip->dev, "%s slave SYNC_DRV failed, rc=%d\n",
				!!disable ? "disable" : "enable", rc);
		return rc;
	}

	reg = MISC_SL_SWITCH_EN_REG;
	val = !!disable ? 0 : EN_SLAVE;
	rc = smb1398_masked_write(chip, reg, EN_SLAVE, val);
	if (rc < 0) {
		dev_err(chip->dev, "write slave_en failed, rc=%d\n", rc);
		return rc;
	}

	if (!disable) {
		msleep(1);//delay 1 ms before enabling slave
		pval.intval = !disable;
		rc = power_supply_set_property(chip->div2_cp_slave_psy,
				POWER_SUPPLY_PROP_CP_ENABLE, &pval);
		if (rc < 0) {
			dev_err(chip->dev, "%s slave switcher failed, rc=%d\n",
					!!disable ? "disable" : "enable", rc);
			return rc;
		}
	}

	return rc;
}

static int smb1398_div2_cp_ilim_vote_cb(struct votable *votable,
		void *data, int ilim_ua, const char *client)
{
	struct smb1398_chip *chip = (struct smb1398_chip *)data;
	union power_supply_propval pval = {0};
	int rc = 0, max_ilim_ua, i;
	bool slave_dis, split_ilim = false;
	bool is_for_wireless = false;
	bool cp_dis = false;
	if (!is_psy_voter_available(chip) || chip->in_suspend)
		return -EAGAIN;

	if (!client)
		return -EINVAL;

	dev_info(chip->dev, "ilim_ua :%d, pass_dis:%d, cp_disable:%d, cp_salve_dis:%d\n", ilim_ua,
			get_effective_result(chip->passthrough_mode_disable_votable),
			get_effective_result(chip->div2_cp_disable_votable),
			get_effective_result(chip->div2_cp_slave_disable_votable));

	is_for_wireless = is_for_wireless_charging(chip);
	max_ilim_ua = is_cps_available(chip) ?
		DIV2_MAX_ILIM_DUAL_CP_UA : DIV2_MAX_ILIM_UA;
	ilim_ua = min(ilim_ua, max_ilim_ua);

	if (is_for_wireless) {
		if (ilim_ua < chip->ilim_ua_disable_div2_cp_wls)
			cp_dis = true;
	} else {
		if (ilim_ua < chip->div2_cp_min_ilim_ua)
			cp_dis = true;
	}

	if (cp_dis) {
		dev_dbg(chip->dev, "ilim %duA is too low to config CP charging\n",
				ilim_ua);
		vote(chip->div2_cp_disable_votable, ILIM_VOTER, true, 0);
	} else {
		if (is_cps_available(chip)) {
			split_ilim = true;
			slave_dis = ilim_ua < chip->ilim_ua_disable_div2_cp_slave;
			vote(chip->div2_cp_slave_disable_votable, ILIM_VOTER,
					slave_dis, 0);
			slave_dis = !!get_effective_result(
					chip->div2_cp_slave_disable_votable);
			rc = power_supply_get_property(chip->div2_cp_slave_psy,
				POWER_SUPPLY_PROP_CP_CFLY_SS_STATUS, &pval);
			if (rc < 0) {
				dev_err(chip->dev, "get CP slave CFLY SS failed, rc=%d\n",
						rc);
				return rc;
			}
			if (!slave_dis && !pval.intval &&
					get_effective_result(chip->passthrough_mode_disable_votable)) {
				slave_dis = true;
				for (i = 0; i < 6; i++) {
					rc = power_supply_get_property(chip->div2_cp_slave_psy,
						POWER_SUPPLY_PROP_CP_CFLY_SS_STATUS, &pval);
					if (rc < 0) {
						dev_err(chip->dev, "get CP slave CFLY SS failed, rc=%d\n",
								rc);
						return rc;
					}
					if (pval.intval) {
						slave_dis = false;
						break;
					}
				}
			}
			if (slave_dis)
				split_ilim = false;
		}

		/* hvdcp_opi using 75% of ilimt, so enlarge it manully */
		if (is_for_wireless) {
			ilim_ua *= chip->ilim_enlarge_pct;
			ilim_ua /= 100;
		}

		if (split_ilim) {
			ilim_ua /= 2;
			pval.intval = ilim_ua;
			rc = power_supply_set_property(chip->div2_cp_slave_psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_MAX, &pval);
			if (rc < 0)
				dev_err(chip->dev, "set CP slave ilim failed, rc=%d\n",
						rc);
			dev_info(chip->dev, "set CP slave ilim to %duA\n",
					ilim_ua);
		}


		rc = smb1398_set_iin_ma(chip, ilim_ua / 1000);
		if (rc < 0) {
			dev_err(chip->dev, "set CP master ilim failed, rc=%d\n",
					rc);
			return rc;
		}
		dev_info(chip->dev, "set CP master ilim to %duA\n", ilim_ua);
		vote(chip->div2_cp_disable_votable, ILIM_VOTER, false, 0);
	}

	return 0;
}

static int passthrough_mode_disable_vote_cb(struct votable *votable,
		void *data, int disable, const char *client)
{
	struct smb1398_chip *chip = (struct smb1398_chip *)data;
	int rc = 0;

	if (!is_psy_voter_available(chip))
		return -EAGAIN;

	if (!chip->passthrough_mode_allowed)
		return -EINVAL;

	if (disable) {
		rc = smb1398_passthrough_exit(chip);
		if (rc < 0)
			return rc;
	}

	/* Rely on hvdcp_opti to handle Passthrough Mode */
	power_supply_changed(chip->div2_cp_master_psy);

	return rc;
}

static int usbin_suspend_cb(struct votable *votable,
		void *data, int disable, const char *client)
{
	struct smb1398_chip *chip = (struct smb1398_chip *)data;
	smb1398_masked_write(chip, MISC_USB_WLS_SUSPEND_REG,
	USB_SUSPEND, disable?USB_SUSPEND:0);
	return 0;
}

static void smb1398_destroy_votables(struct smb1398_chip *chip)
{
	destroy_votable(chip->awake_votable);
	destroy_votable(chip->div2_cp_disable_votable);
	destroy_votable(chip->div2_cp_ilim_votable);
	destroy_votable(chip->div2_cp_slave_disable_votable);
	destroy_votable(chip->passthrough_mode_disable_votable);
}

static int smb1398_div2_cp_create_votables(struct smb1398_chip *chip)
{
	int rc;

	chip->awake_votable = create_votable("SMB1398_AWAKE",
			VOTE_SET_ANY, smb1398_awake_vote_cb, chip);
	if (IS_ERR_OR_NULL(chip->awake_votable))
		return PTR_ERR_OR_ZERO(chip->awake_votable);

	chip->div2_cp_disable_votable = create_votable("CP_DISABLE",
			VOTE_SET_ANY, smb1398_div2_cp_disable_vote_cb, chip);
	if (IS_ERR_OR_NULL(chip->div2_cp_disable_votable)) {
		rc = PTR_ERR_OR_ZERO(chip->div2_cp_disable_votable);
		goto destroy;
	}

	chip->div2_cp_slave_disable_votable = create_votable("CP_SLAVE_DISABLE",
			VOTE_SET_ANY, smb1398_div2_cp_slave_disable_vote_cb,
			chip);
	if (IS_ERR_OR_NULL(chip->div2_cp_slave_disable_votable)) {
		rc = PTR_ERR_OR_ZERO(chip->div2_cp_slave_disable_votable);
		goto destroy;
	}

	chip->div2_cp_ilim_votable = create_votable("CP_ILIM",
			VOTE_MIN, smb1398_div2_cp_ilim_vote_cb, chip);
	if (IS_ERR_OR_NULL(chip->div2_cp_ilim_votable)) {
		rc = PTR_ERR_OR_ZERO(chip->div2_cp_ilim_votable);
		goto destroy;
	}

	chip->passthrough_mode_disable_votable = create_votable("PASSTHROUGH",
			VOTE_SET_ANY, passthrough_mode_disable_vote_cb, chip);
	if (IS_ERR_OR_NULL(chip->passthrough_mode_disable_votable)) {
		rc = PTR_ERR_OR_ZERO(chip->passthrough_mode_disable_votable);
		goto destroy;
	}

	chip->usbin_suspend_votable = create_votable("USBIN_SUSPEND",
		VOTE_SET_ANY, usbin_suspend_cb, chip);
	if (IS_ERR_OR_NULL(chip->usbin_suspend_votable)) {
		rc = PTR_ERR_OR_ZERO(chip->usbin_suspend_votable);
		goto destroy;
	}

	vote(chip->div2_cp_slave_disable_votable, MAIN_DISABLE_VOTER, true, 0);
	vote(chip->div2_cp_disable_votable, USER_VOTER, true, 0);
	vote(chip->div2_cp_disable_votable, CUTOFF_SOC_VOTER,
			is_cutoff_soc_reached(chip), 0);
	if (chip->passthrough_mode_allowed)
		vote(chip->passthrough_mode_disable_votable, CUTOFF_SOC_VOTER,
				is_cutoff_soc_reached(chip), 0);

	if (is_psy_voter_available(chip))
		vote(chip->div2_cp_ilim_votable, FCC_VOTER, true,
			get_effective_result(chip->fcc_votable) / 2);
	return 0;
destroy:
	smb1398_destroy_votables(chip);

	return 0;
}

static irqreturn_t default_irq_handler(int irq, void *data)
{
	struct smb1398_chip *chip = data;
	int rc, i;
	bool switcher_en = chip->switcher_en;
	union power_supply_propval pval = {0};

	if (!is_psy_voter_available(chip))
		return IRQ_HANDLED;

	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_ADAPTER_CC_MODE,
			&pval);
	if (rc < 0) {
		dev_err(chip->dev, "Get ADAPTER_CC_MODE failed, rc=%d\n");
		return IRQ_HANDLED;
	}

	for (i = 0; i < NUM_IRQS; i++) {
		if (irq == chip->irqs[i]) {
			dev_info(chip->dev, "IRQ %s triggered\n",
					smb_irqs[i].name);
			//Ignore WIN-UV for CC MODE
			if (!(pval.intval && (strcmp(smb_irqs[i].name, "div2-win-uv") == 0)))
				chip->div2_irq_status |= 1 << smb_irqs[i].shift;

			if (strcmp(smb_irqs[i].name, "div2-irev") == 0) {
				cancel_delayed_work(&chip->irev_process_work);
				schedule_delayed_work(&chip->irev_process_work, msecs_to_jiffies(200));
			}
		}
	}

	rc = smb1398_get_enable_status(chip);
	if (rc < 0)
		goto out;

	if (chip->switcher_en != switcher_en)
		if (chip->fcc_votable)
			rerun_election(chip->fcc_votable);
out:
	if (chip->div2_cp_master_psy)
		power_supply_changed(chip->div2_cp_master_psy);
	return IRQ_HANDLED;
}

static const struct smb_irq smb_irqs[] = {
	/* useful IRQs from perph0 */
	[TEMP_SHDWN_IRQ]	= {
		.name		= "temp-shdwn",
		.handler	= default_irq_handler,
		.wake		= true,
		.shift		= 2,
	},
	[DIV2_IREV_LATCH_IRQ]	= {
		.name		= "div2-irev",
		.handler	= default_irq_handler,
		.wake		= true,
		.shift		= 3,
	},
	[USB_IN_UVLO_IRQ]	= {
		.name		= "usbin-uv",
		.handler	= default_irq_handler,
		.wake		= true,
		.shift		= 1,
	},
	[USB_IN_OVLO_IRQ]	= {
		.name		= "usbin-ov",
		.handler	= default_irq_handler,
		.wake		= true,
		.shift		= 1,
	},
	/* useful IRQs from perph1 */
	[DIV2_ILIM_IRQ]		= {
		.name		= "div2-ilim",
		.handler	= default_irq_handler,
		.wake		= true,
		.shift		= 6,
	},
	[DIV2_WIN_UV_IRQ]	= {
		.name		= "div2-win-uv",
		.handler	= default_irq_handler,
		.wake		= true,
		.shift		= 0,
	},
	[DIV2_WIN_OV_IRQ]	= {
		.name		= "div2-win-ov",
		.handler	= default_irq_handler,
		.wake		= true,
		.shift		= 0,
	},
};

static int smb1398_get_irq_index_byname(const char *irq_name)
{
	int i;

	for (i = 0; i < NUM_IRQS; i++) {
		if (smb_irqs[i].name != NULL)
			if (strcmp(smb_irqs[i].name, irq_name) == 0)
				return i;
	}

	return -ENOENT;
}

static int smb1398_request_interrupt(struct smb1398_chip *chip,
		struct device_node *node, const char *irq_name)
{
	int rc = 0, irq, irq_index;

	irq = of_irq_get_byname(node, irq_name);
	if (irq < 0) {
		dev_err(chip->dev, "Get irq %s failed\n", irq_name);
		return irq;
	}

	irq_index = smb1398_get_irq_index_byname(irq_name);
	if (irq_index < 0) {
		dev_err(chip->dev, "%s IRQ is not defined\n", irq_name);
		return irq_index;
	}

	if (!smb_irqs[irq_index].handler)
		return 0;

	rc = devm_request_threaded_irq(chip->dev, irq, NULL,
			smb_irqs[irq_index].handler,
			IRQF_ONESHOT, irq_name, chip);
	if (rc < 0) {
		dev_err(chip->dev, "Request interrupt for %s failed, rc=%d\n",
				irq_name, rc);
		return rc;
	}

	chip->irqs[irq_index] = irq;
	if (smb_irqs[irq_index].wake)
		enable_irq_wake(irq);
	disable_irq(irq);

	return 0;
}

static int smb1398_request_interrupts(struct smb1398_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	int rc = 0;
	const char *name;
	struct property *prop;

	of_property_for_each_string(node, "interrupt-names", prop, name) {
		rc = smb1398_request_interrupt(chip, node, name);
		if (rc < 0)
			return rc;
	}

	return 0;
}
static int smb1398_charge_get_wireless_adapter_type(struct smb1398_chip *chip, int *adpater_type)
{
	union power_supply_propval val = {0,};
	int ret = 0;

	if (!chip->wls_psy)
		return -ENODEV;

	ret = power_supply_get_property(chip->wls_psy,
			POWER_SUPPLY_PROP_TX_ADAPTER, &val);
	if (!ret) {
		*adpater_type = val.intval;
		pr_info("TX adapter type is:%d\n", *adpater_type);
	}
	return ret;
}
static int smb1398_get_wls_charging_icl(struct smb1398_chip *chip, int *effectiveIcl)
{
	int rc = 0;
	int tx_adapter_type;
	smb1398_charge_get_wireless_adapter_type(chip, &tx_adapter_type);
	if (tx_adapter_type == ADAPTER_XIAOMI_QC3
			|| tx_adapter_type == ADAPTER_XIAOMI_PD
			|| tx_adapter_type == ADAPTER_ZIMI_CAR_POWER) {
		*effectiveIcl = WLDC_XIAOMI_20W_IOUT_MAX_UA;
	} else if ((tx_adapter_type == ADAPTER_XIAOMI_PD_40W)
			|| (tx_adapter_type == ADAPTER_VOICE_BOX)) {
		*effectiveIcl = WLDC_XIAOMI_30W_IOUT_MAX_UA;
	} else if (tx_adapter_type == ADAPTER_XIAOMI_PD_45W ||
			tx_adapter_type == ADAPTER_XIAOMI_PD_60W) {
		*effectiveIcl = WLDC_XIAOMI_50W_IOUT_MAX_UA;
	} else {
		dev_err(chip->dev, "Get Main tx_adapter_type failed, tx_adapter_type=%d\n",
						tx_adapter_type);
		rc = -EINVAL;
	}
	return rc;
}

static void smb1398_status_change_work(struct work_struct *work)
{
	struct smb1398_chip *chip = container_of(work,
			struct smb1398_chip, status_change_work);
	union power_supply_propval pval = {0};
	int rc, wls_icl_ua, rx_iout_max_ua;
	bool is_for_wireless = is_for_wireless_charging(chip);
	if (!is_psy_voter_available(chip))
		goto out;

	if (!is_adapter_in_cc_mode(chip) && !is_for_wireless) {
		vote(chip->div2_cp_disable_votable, CUTOFF_SOC_VOTER,
				is_cutoff_soc_reached(chip), 0);
		if (chip->passthrough_mode_allowed)
			vote(chip->passthrough_mode_disable_votable, CUTOFF_SOC_VOTER,
					is_cutoff_soc_reached(chip), 0);
	}

	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_SMB_EN_MODE, &pval);
	if (rc < 0) {
		dev_err(chip->dev, "get SMB_EN_MODE failed, rc=%d\n", rc);
		goto out;
	}

	/* If no CP charging started */
	if (pval.intval != POWER_SUPPLY_CHARGER_SEC_CP) {
		chip->cutoff_soc_checked = false;
		vote(chip->div2_cp_slave_disable_votable,
				TAPER_VOTER, false, 0);
		vote(chip->div2_cp_slave_disable_votable,
				DELAY_OPEN_VOTER, false, 0);
		vote(chip->div2_cp_disable_votable, TAPER_VOTER, false, 0);
		vote(chip->passthrough_mode_disable_votable, TAPER_VOTER, false, 0);
		vote(chip->smb_override_votable, TAPER_VOTER, false, 0);
		vote(chip->div2_cp_disable_votable, SRC_VOTER, true, 0);
		vote(chip->fcc_votable, CP_VOTER, false, 0);
		vote(chip->div2_cp_ilim_votable, CC_MODE_VOTER, false, 0);
		vote(chip->passthrough_mode_disable_votable, SRC_VOTER, true, 0);
		vote(chip->passthrough_mode_disable_votable, USER_VOTER, false, 0);
		smb1398_set_ocp(chip, false);
		smb1398_masked_write(chip, PERPH0_MISC_CFG0,
				CFG_PRE_REG_2S, 0);//Allow FP FET enable on IREV for input removal
		smb1398_masked_write(chip, PERPH0_CFG_SDCDC_REG,
				EN_WIN_OV_BIT, EN_WIN_OV_BIT);//Re-Enable WIN_OV
		goto out;
	}

	if (is_for_wireless)
		chip->cutoff_soc_checked = true;

	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_SMB_EN_REASON, &pval);
	if (rc < 0) {
		dev_err(chip->dev, "Get SMB_EN_REASON failed, rc=%d\n",
				rc);
		goto out;
	}

	if (pval.intval == POWER_SUPPLY_CP_NONE) {
		vote(chip->div2_cp_disable_votable, SRC_VOTER, true, 0);
		vote(chip->passthrough_mode_disable_votable, SRC_VOTER, true, 0);
		smb1398_masked_write(chip, PERPH0_MISC_CFG0,
			CFG_PRE_REG_2S, 0);//Allow FP FET enable on IREV for input removal
		smb1398_masked_write(chip, PERPH0_CFG_SDCDC_REG,
			EN_WIN_OV_BIT, EN_WIN_OV_BIT);//Re-Enable WIN_OV
		goto out;
	}

	vote(chip->div2_cp_disable_votable, SRC_VOTER, false, 0);
	if (chip->passthrough_mode_allowed && pval.intval == POWER_SUPPLY_CP_PPS)
		vote(chip->passthrough_mode_disable_votable, SRC_VOTER, false, 0);

	if (!chip->cutoff_soc_checked) {
		vote(chip->div2_cp_disable_votable, CUTOFF_SOC_VOTER,
				is_cutoff_soc_reached(chip), 0);
		if (chip->passthrough_mode_allowed)
			vote(chip->passthrough_mode_disable_votable, CUTOFF_SOC_VOTER,
					is_cutoff_soc_reached(chip), 0);
		chip->cutoff_soc_checked = true;
	}

	if (pval.intval == POWER_SUPPLY_CP_WIRELESS) {
		/*
		 * Get the max output current from the wireless PSY
		 * and set the DIV2 CP ilim accordingly
		 */
		vote(chip->div2_cp_ilim_votable, ICL_VOTER, false, 0);

		rc = power_supply_get_property(chip->wls_psy,
					POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
		if (rc < 0) {
			dev_err(chip->dev, "Get Wireless CURRENT_MAX failed, rc=%d\n", rc);
		} else {
			smb1398_get_wls_charging_icl(chip, &rx_iout_max_ua);
			wls_icl_ua = min(pval.intval, rx_iout_max_ua);
			dev_info(chip->dev, "currnet_max: %d wls_icl_ua:%d\n", pval.intval, wls_icl_ua);
			/** SMB1398 only supports USBIN-USBIN now.
			* ILIM must be limited to current_max - icl_main
			*/
			pval.intval = wls_icl_ua - WLS_MAIN_CHAREGER_ICL_UA;
			if (pval.intval > 0)
				vote(chip->div2_cp_ilim_votable, WIRELESS_VOTER, true, pval.intval);
		}

	} else {
		vote(chip->div2_cp_ilim_votable, WIRELESS_VOTER, false, 0);
	}

	/*
	 * all votes that would result in disabling the charge pump have
	 * been cast; ensure the charge pump is still enabled before
	 * continuing.
	 */
	if (get_effective_result(chip->div2_cp_disable_votable))
		goto out;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
	if (rc < 0) {
		dev_err(chip->dev, "get CHARGE_TYPE failed, rc=%d\n",
				rc);
		goto out;
	}
	pr_info("get charge type:%d, chip->taper_work_running:%d\n", pval.intval, chip->taper_work_running);

	if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER) {
		if (!chip->taper_work_running) {
			chip->taper_work_running = true;
			vote(chip->awake_votable, TAPER_VOTER, true, 0);
			queue_work(system_long_wq, &chip->taper_work);
		}
	}
out:
	pm_relax(chip->dev);
	chip->status_change_running = false;
}

static int smb1398_notifier_cb(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct smb1398_chip *chip = container_of(nb, struct smb1398_chip, nb);
	struct power_supply *psy = (struct power_supply *)data;
	unsigned long flags;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (strcmp(psy->desc->name, "battery") == 0 ||
			strcmp(psy->desc->name, "usb") == 0 ||
			strcmp(psy->desc->name, "main") == 0 ||
			strcmp(psy->desc->name, "wireless") == 0 ||
			strcmp(psy->desc->name, "bbc") == 0 ||
			strcmp(psy->desc->name, "cp_slave") == 0) {
		spin_lock_irqsave(&chip->status_change_lock, flags);
		if (!chip->status_change_running) {
			chip->status_change_running = true;
			pm_stay_awake(chip->dev);
			schedule_work(&chip->status_change_work);
		}
		spin_unlock_irqrestore(&chip->status_change_lock, flags);
	}

	return NOTIFY_OK;
}

static void smb1398_cp_close_work(struct work_struct *work)
{
	struct smb1398_chip *chip = container_of(work,
		struct smb1398_chip, cp_close_work);
	vote(chip->usb_icl_votable, WIRELESS_VOTER, false, 0);
	vote_override(chip->fcc_main_votable, CC_MODE_VOTER, false, 0);
	vote_override(chip->usb_icl_votable, CC_MODE_VOTER, false, 0);
	vote(chip->div2_cp_ilim_votable, WIRELESS_CP_OPEN_VOTER, false, 0);
	/* disable 50w monitor work */
	chip->max_power_cnt = MAX_MONITOR_CYCLE_CNT;
	cancel_delayed_work_sync(&chip->high_power_monitor_work);
	schedule_delayed_work(&chip->stop_rx_ocp_work, msecs_to_jiffies(0));
	vote(chip->div2_cp_ilim_votable, HIGH_POWER_VOTER, false, 0);
}

/* work for wireless charging after cp closed */
#define MAX_MAIN_PMIC_ICL_UA 1500000
static void smb1398_wireless_close_work(struct work_struct *work)
{
	int effectiveICL = 0;
	int rc = 0;
	struct smb1398_chip *chip = container_of(work,
		struct smb1398_chip, wireless_close_work);
	rc = smb1398_get_wls_charging_icl(chip, &effectiveICL);
	if (!rc) {
		effectiveICL = min(effectiveICL, MAX_MAIN_PMIC_ICL_UA);
		vote(chip->usb_icl_votable, WIRELESS_ICL_VOTER, true, effectiveICL);
	}
}

static void smb1398_wireless_open_work(struct work_struct *work)
{
	struct smb1398_chip *chip = container_of(work,
		struct smb1398_chip, wireless_open_work);
	union power_supply_propval val = {0, };
	int wireless_vout = 0;
	chip->isns_cnt = 0;
	vote(chip->usb_icl_votable, WIRELESS_VOTER, true, WLS_MAIN_CHAREGER_ICL_UA);
	vote(chip->div2_cp_ilim_votable, WIRELESS_CP_OPEN_VOTER, true, WLS_MAIN_START_ILIM_UA);
	if (chip->batt_psy) {
		power_supply_get_property(chip->batt_psy,
						POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
		wireless_vout = val.intval;
		wireless_vout *= 2;
		wireless_vout /= 100000;
		wireless_vout *= 100000;
		wireless_vout += 200000;
		val.intval = wireless_vout;
		if (chip->dc_psy)
			power_supply_set_property(chip->dc_psy, POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, &val);
	}
	if (chip->div2_cp_master_psy)
		power_supply_changed(chip->div2_cp_master_psy);
}

static void smb1398_isns_process_work(struct work_struct *work)
{
	static int count;
	int rc = 0;
	int rc2 = 0;
	int isns_total = 0;
	int isns_ua = 0;
	int slave_isns_ua = 0;
	int tx_adapter_type = 0;
	u8 in_passthrough;
	union power_supply_propval pval = {0};
	struct smb1398_chip *chip = container_of(work,
			struct smb1398_chip, isns_process_work);

	/* Disable FP FET on IREV for CP charging
	 * if total ISNS >= 2A, otherwise let FP FET
	 * stay enabled upon IREV
	 */
	rc = smb1398_div2_cp_get_master_isns(chip, &isns_ua);
	if (rc >= 0) {
		isns_total = isns_ua;
		rc2 = smb1398_div2_cp_get_slave_isns(chip, &slave_isns_ua);
		if (rc2 >= 0) {
			isns_total += slave_isns_ua;
		}

		if (is_for_wireless_charging(chip)) {
			if (is_cps_available(chip)) {
				rc = power_supply_get_property(chip->div2_cp_slave_psy,
						POWER_SUPPLY_PROP_CP_CFLY_SS_STATUS, &pval);
				if (rc < 0) {
					dev_err(chip->dev, "get CP slave CFLY SS failed, rc=%d\n",
							rc);
					return;
				}
			}

			if (isns_total >= WLS_MAIN_START_ILIM_THRESHOLD_UA && chip->isns_cnt <= WLS_MAIN_START_ILIM_CNT) {
				if (chip->isns_cnt++ >= WLS_MAIN_START_ILIM_CNT) {
					vote(chip->div2_cp_ilim_votable, WIRELESS_CP_OPEN_VOTER, false, 0);
					smb1398_charge_get_wireless_adapter_type(chip, &tx_adapter_type);
					if (tx_adapter_type == ADAPTER_XIAOMI_PD_45W ||tx_adapter_type == ADAPTER_XIAOMI_PD_60W) {
						chip->max_power_cnt = 0;
						if (!chip->rx_ocp_disable_votable)
							chip->rx_ocp_disable_votable = find_votable("IDTP_OCP_DISABLE");
						if (chip->rx_ocp_disable_votable)
							vote(chip->rx_ocp_disable_votable, HIGH_POWER_VOTER, true, 0);

						schedule_delayed_work(&chip->high_power_monitor_work, msecs_to_jiffies(0));
					}
				}
			}

			if (pval.intval) {
				if (isns_total >= 2 * WLS_WIN_OV_EN_THRESHOLD_UA)
					smb1398_masked_write(chip, PERPH0_MISC_CFG0,
							CFG_PRE_REG_2S, CFG_PRE_REG_2S);
				else
					smb1398_masked_write(chip, PERPH0_MISC_CFG0,
							CFG_PRE_REG_2S, 0);
			} else {
				/* Disable FP FET on IREV for CP charging
				 * if master ISNS >= 1A for dual charge,
				 * otherwise let FP FET stay enabled upon IREV
				 */
				if (isns_ua >= WLS_WIN_OV_EN_THRESHOLD_UA)
					smb1398_masked_write(chip, PERPH0_MISC_CFG0,
							CFG_PRE_REG_2S, CFG_PRE_REG_2S);
				else
					smb1398_masked_write(chip, PERPH0_MISC_CFG0,
							CFG_PRE_REG_2S, 0);
			}
		} else {
			if (isns_total >= FP_FET_IREV_THRESHOLD_UA)
				smb1398_masked_write(chip, PERPH0_MISC_CFG0,
					CFG_PRE_REG_2S, CFG_PRE_REG_2S);

			rc = smb1398_read(chip, MISC_CFG0_REG, &in_passthrough);
			if (rc < 0) {
				dev_err(chip->dev, "Read passthrough mode bit failed, rc=%d\n", rc);
				return;
			}
			in_passthrough = in_passthrough & CFG_ITLGS_2P_BIT;

			if (in_passthrough) {
				if (isns_total >= FP_FET_IREV_THRESHOLD_UA * 2)
					vote(chip->div2_cp_slave_disable_votable, DELAY_OPEN_VOTER, false, 0);
			}
			if (isns_ua > FP_FET_IREV_THRESHOLD_UA) {
				pval.intval = true;
				rc = power_supply_set_property(chip->div2_cp_master_psy,
						POWER_SUPPLY_PROP_CP_OCP_CONFIG, &pval);
				if (rc < 0)
					dev_err(chip->dev, "Couldn't set slave ISNS_MODE_OFF, rc=%d\n",
							rc);
			}
			if (slave_isns_ua > FP_FET_IREV_THRESHOLD_UA) {
				pval.intval = true;
				rc = power_supply_set_property(chip->div2_cp_slave_psy,
						POWER_SUPPLY_PROP_CP_OCP_CONFIG, &pval);
				if (rc < 0)
					dev_err(chip->dev, "Couldn't set slave ISNS_MODE_OFF, rc=%d\n",
							rc);
			}

			if (is_adapter_in_cc_mode(chip) && !is_vbus_ok_for_passthrough(chip)) {
				if (count > 10) {
					dev_err(chip->dev, "adapter removal set smb disable\n");
					vote(chip->smb_override_votable, PL_SMB_EN_VOTER, true, 0);
				} else
					count++;
			} else {
				count = 0;
			}
		}


		/* Disable WIN_OV for WLS at low ISNS */
		if (isns_total < WLS_WIN_OV_EN_THRESHOLD_UA)
			smb1398_masked_write(chip, PERPH0_CFG_SDCDC_REG,
					EN_WIN_OV_BIT, 0);
		else
			smb1398_masked_write(chip, PERPH0_CFG_SDCDC_REG,
					EN_WIN_OV_BIT, EN_WIN_OV_BIT);
	}
}
static void smb1398_stop_rx_ocp_work(struct work_struct *work)
{
	struct smb1398_chip *chip = container_of(work,
		struct smb1398_chip, stop_rx_ocp_work.work);

	if (!chip->rx_ocp_disable_votable)
		chip->rx_ocp_disable_votable = find_votable("IDTP_OCP_DISABLE");
	if (chip->rx_ocp_disable_votable)
		vote(chip->rx_ocp_disable_votable, HIGH_POWER_VOTER, false, 0);
}

static void smb1398_high_power_monitor_work(struct work_struct *work)
{
	struct smb1398_chip *chip = container_of(work,
		struct smb1398_chip, high_power_monitor_work.work);
	bool temp_high = false;
	bool ilim_changed = false;
	union power_supply_propval val = {0};

	if (!chip->wls_psy) {
		chip->wls_psy = power_supply_get_by_name("wireless");
	}

	if (chip->wls_psy) {
		power_supply_get_property(chip->wls_psy, POWER_SUPPLY_PROP_OTG_STATE, &val);
		dev_err(chip->dev, "get rx temp: %d, cnt:%d\n", val.intval, chip->max_power_cnt);
		if (val.intval >= MAX_RX_TEMPERATRUE)
			temp_high = true;
	}

	if (get_effective_result_locked(chip->div2_cp_ilim_votable) != (WLDC_XIAOMI_50W_IOUT_MAX_UA - WLS_MAIN_CHAREGER_ICL_UA)) {
		dev_err(chip->dev, "ilim changed, go out\n");
		ilim_changed = true;
	}

	if (chip->max_power_cnt++ >= MAX_MONITOR_CYCLE_CNT || temp_high || ilim_changed) {
		/* set 40w icl, break work */
		vote(chip->div2_cp_ilim_votable, HIGH_POWER_VOTER, true, WLDC_XIAOMI_40W_IOUT_MAX_UA - WLS_MAIN_CHAREGER_ICL_UA);
		schedule_delayed_work(&chip->stop_rx_ocp_work, msecs_to_jiffies(5000));
	} else {
		schedule_delayed_work(&chip->high_power_monitor_work, msecs_to_jiffies(1000));
	}
}
static void smb1398_irev_process_work(struct work_struct *work)
{
	struct smb1398_chip *chip = container_of(work,
		struct smb1398_chip, irev_process_work.work);
	int rc;
	union power_supply_propval pval = {0};
	long vbus_uv, vbat_uv;
	int rx_off;

	if (is_for_wireless_charging(chip)) {
		if (!chip->usb_psy) {
			chip->usb_psy = power_supply_get_by_name("usb");
			if (!chip->usb_psy)
				return;
		}
		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW,
				&pval);
		if (rc < 0) {
			dev_err(chip->dev, "Get USB VOLTAGE_NOW failed, rc=%d\n");
			return;
		}

		vbus_uv = pval.intval;

		if (!chip->batt_psy) {
			chip->batt_psy = power_supply_get_by_name("battery");
			if (!chip->batt_psy)
				return;
		}
		rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW,
				&pval);
		if (rc < 0) {
			dev_err(chip->dev, "Get BATT VOLTAGE_NOW failed, rc=%d\n");
			return;
		}

		vbat_uv = pval.intval;
		dev_err(chip->dev, "vbat_uv=%d, vbus_uv:%d, rx_off:%d\n", vbat_uv, vbus_uv, rx_off);
		if (vbus_uv > 0) {
			rx_off = vbus_uv < vbat_uv ? true : false;
		}
		if (rx_off) {
			if (!chip->wls_psy) {
				chip->wls_psy = power_supply_get_by_name("wireless");
				if (!chip->wls_psy) {
					dev_dbg(chip->dev, "Couldn't find Wireless psy\n");
					return;
				}
			}
			pval.intval = 0;
			power_supply_set_property(chip->wls_psy, POWER_SUPPLY_PROP_DIV_2_MODE, &pval);
		}
	}
}
static void smb1398_taper_work(struct work_struct *work)
{
	struct smb1398_chip *chip = container_of(work,
			struct smb1398_chip, taper_work);
	union power_supply_propval pval = {0};
	int rc, fcc_ua, stepper_ua, main_fcc_ua, vcell_volt;
	bool slave_en;

	if (!is_psy_voter_available(chip))
		goto out;

	if (!chip->fcc_main_votable)
		chip->fcc_main_votable = find_votable("FCC_MAIN");

	if (chip->fcc_main_votable)
		main_fcc_ua = get_effective_result(chip->fcc_main_votable);

	while (true) {
		/*is there use batt psy or bms psy*/

		if (!chip->bms_psy) {
			chip->bms_psy = power_supply_get_by_name("bms");
			if (!chip->bms_psy) {
				dev_dbg(chip->dev, "Couldn't find bms psy\n");
				return;
			}
		}

		rc = power_supply_get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (rc < 0) {
			dev_err(chip->dev, "get bms voltage now rc=%d\n",
					rc);
			goto out;
		}
		vcell_volt = pval.intval;

		rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		if (rc < 0) {
			dev_err(chip->dev, "get CHARGE_TYPE failed, rc=%d\n",
					rc);
			goto out;
		}

		pr_info("smb1398_taper_work charge type:%d, vcell_volt:%d\n", pval.intval, vcell_volt);

		if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER) {
			stepper_ua = is_adapter_in_cc_mode(chip) ?
				TAPER_STEPPER_UA_IN_CC_MODE :
				TAPER_STEPPER_UA_DEFAULT;

			if (vcell_volt > BQ25790_FFC_FV)
				stepper_ua *= 2;

			fcc_ua = get_effective_result(chip->fcc_votable) - stepper_ua;
			dev_info(chip->dev, "Taper stepper reduce FCC to %d\n",
					fcc_ua);
			vote(chip->fcc_votable, CP_VOTER, true, fcc_ua);
			fcc_ua -= main_fcc_ua;
			/*
			 * If total FCC is less than the minimum ILIM to
			 * keep CP master and slave online, disable CP.
			 */
			if (fcc_ua <= (chip->div2_cp_min_ilim_ua * 4)) {
				vote(chip->smb_override_votable,
						TAPER_VOTER, true, 0);
				vote(chip->div2_cp_disable_votable,
						TAPER_VOTER, true, 0);
				vote(chip->passthrough_mode_disable_votable,
						TAPER_VOTER, true, 0);
				goto out;
			}

			if (is_for_wireless_charging(chip)) {
				if (fcc_ua <= chip->div2_cp_min_ilim_ua * 8) {
					vote(chip->div2_cp_disable_votable,
						TAPER_VOTER, true, 0);
					goto out;
				}
			}
			/*
			 * If total FCC is less than the minimum ILIM to keep
			 * slave CP online, disable slave, and set master CP
			 * ILIM to maximum to avoid ILIM IRQ storm.
			 */
			slave_en = !get_effective_result(
					chip->div2_cp_slave_disable_votable);
			if ((get_effective_result(chip->fcc_votable) <= chip->ilim_ua_disable_div2_cp_slave) &&
					slave_en && is_cps_available(chip)) {
				vote(chip->div2_cp_slave_disable_votable,
						TAPER_VOTER, true, 0);
				dev_info(chip->dev, "Disable slave CP in taper\n");
				vote_override(chip->div2_cp_ilim_votable,
						CC_MODE_VOTER,
						get_effective_result(
							chip->passthrough_mode_disable_votable),
						DIV2_MAX_ILIM_DUAL_CP_UA);
			}
		} else {
			dev_info(chip->dev, "Not in taper, exit!\n");
		}
		msleep(500);
	}
out:
	dev_info(chip->dev, "exit taper work\n");
	vote(chip->fcc_votable, CP_VOTER, false, 0);
	vote(chip->awake_votable, TAPER_VOTER, false, 0);
	chip->taper_work_running = false;
}

static int smb1398_div2_cp_hw_init(struct smb1398_chip *chip)
{
	int rc = 0;

	/* Configure window (Vin/2 - Vout) OV level to 500mV */
	rc = smb1398_masked_write(chip, DIV2_PROTECTION_REG,
			DIV2_WIN_OV_SEL_MASK, WIN_OV_500_MV);
	if (rc < 0) {
		dev_err(chip->dev, "set WIN_OV_500_MV failed, rc=%d\n", rc);
		return rc;
	}

	/* Configure master TEMP pin to output Vtemp signal by default */
	rc = smb1398_masked_write(chip, SSUPLY_TEMP_CTRL_REG,
			SEL_OUT_TEMP_MAX_MASK, SEL_OUT_VTEMP);
	if (rc < 0) {
		dev_err(chip->dev, "set SSUPLY_TEMP_CTRL_REG failed, rc=%d\n",
				rc);
		return rc;
	}

	/* Configure to use Vtemp signal */
	rc = smb1398_masked_write(chip, PERPH0_MISC_CFG2_REG,
			CFG_TEMP_PIN_ITEMP, 0);
	if (rc < 0) {
		dev_err(chip->dev, "set PERPH0_MISC_CFG2_REG failed, rc=%d\n",
				rc);
		return rc;
	}

	/* switcher enable controlled by register */
	rc = smb1398_masked_write(chip, MISC_CFG0_REG,
			SW_EN_SWITCHER_BIT, SW_EN_SWITCHER_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "set CFG_EN_SOURCE failed, rc=%d\n",
				rc);
		return rc;
	}

	rc = smb1398_masked_write(chip, MISC_SL_SWITCH_EN_REG,
			EN_SWITCHER, 0);
	if (rc < 0) {
		dev_err(chip->dev, "write SWITCH_EN_REG failed, rc=%d\n", rc);
		return rc;
	}
	rc = smb1398_masked_write(chip, IREV_TUNE_OFF_Q1_DELAY_REG,
			IREV_DELAY_BIT, 0);

	/*Enable Standalone mode*/
	if (chip->standalone_mode)
		rc = smb1398_masked_write(chip, NOLOCK_SPARE_REG,
			STANDALONE_EN_BIT, STANDALONE_EN_BIT);
	else
		rc = smb1398_masked_write(chip, NOLOCK_SPARE_REG,
			STANDALONE_EN_BIT, 0);

	rc = smb1398_masked_write(chip, SDCDC_1,
			EN_CHECK_CFLY_SSDONE_BIT, EN_CHECK_CFLY_SSDONE_BIT);
	rc = smb1398_masked_write(chip, MISC_CFG0_REG,
			CFG_ITLGS_2P_BIT, 0);//Disable Passthrough

	smb1398_set_ovlo(chip, USBIN_OVP_22_2V);

	/* Set minimum WIN_UV threshold for noisy WLS Rx */
	rc = smb1398_masked_write(chip, NOLOCK_SPARE_REG,
	DIV2_WIN_UV_SEL_BIT, 0);
	if (rc < 0) {
		dev_err(chip->dev, "set NOLOCK_SPARE_REG failed, rc=%d\n", rc);
		return rc;
	}
	return rc;
}

static int smb1398_div2_cp_parse_dt(struct smb1398_chip *chip)
{
	int rc = 0;

	rc = of_property_match_string(chip->dev->of_node,
			"io-channel-names", "die_temp");
	if (rc < 0) {
		dev_err(chip->dev, "die_temp IIO channel not found\n");
		return rc;
	}

	chip->die_temp_chan = devm_iio_channel_get(chip->dev,
			"die_temp");
	if (IS_ERR(chip->die_temp_chan)) {
		rc = PTR_ERR(chip->die_temp_chan);
		if (rc != -EPROBE_DEFER)
			dev_err(chip->dev, "get die_temp_chan failed, rc=%d\n",
					rc);
		chip->die_temp_chan = NULL;
		return rc;
	}

	chip->div2_cp_min_ilim_ua = 1000000;
	of_property_read_u32(chip->dev->of_node, "qcom,div2-cp-min-ilim-ua",
			&chip->div2_cp_min_ilim_ua);

	chip->max_cutoff_soc = 85;
	of_property_read_u32(chip->dev->of_node, "qcom,max-cutoff-soc",
			&chip->max_cutoff_soc);

	chip->wls_max_cutoff_soc = 85;
	of_property_read_u32(chip->dev->of_node, "qcom,wls-max-cutoff-soc",
			&chip->wls_max_cutoff_soc);

	of_property_read_u32(chip->dev->of_node, "qcom,ilim-ua-disable-slave",
			&chip->ilim_ua_disable_div2_cp_slave);

	chip->passthrough_mode_allowed = of_property_read_bool(chip->dev->of_node,
			"qcom,passthrough-mode-allowed");

	chip->ilim_ua_disable_div2_cp_wls = chip->div2_cp_min_ilim_ua*2;
	of_property_read_u32(chip->dev->of_node, "mi,ilim-ua-disable-cp-wls",
			&chip->ilim_ua_disable_div2_cp_wls);

	/* default percent of ilimt is 75% */
	chip->ilim_enlarge_pct = 133;
	of_property_read_u32(chip->dev->of_node, "mi,ilim_enlarge_pct",
			&chip->ilim_enlarge_pct);

	return 0;
}

static ssize_t smb1398_isns_delta_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smb1398_chip *chip  = i2c_get_clientdata(client);
	int master_isns, slave_isns;
	int len, rc;
	int isns_delta;

	rc = smb1398_div2_cp_get_master_isns(chip, &master_isns);
	if (rc < 0) {
		isns_delta = -EINVAL;
		isns_delta = -EINVAL;
		goto end;
	}
	rc = smb1398_div2_cp_get_slave_isns(chip, &slave_isns);
	if (rc < 0) {
		isns_delta = -EINVAL;
		goto end;
	}

	if (master_isns > slave_isns)
		isns_delta = master_isns - slave_isns;
	else
		isns_delta = slave_isns - master_isns;
end:
	len = snprintf(buf, 1024, "%d\n", isns_delta);

	return len;
}

static DEVICE_ATTR(isns_delta, S_IRUGO, smb1398_isns_delta_show, NULL);

static struct attribute *smb1398_attributes[] = {
	&dev_attr_isns_delta.attr,
	NULL,
};
static const struct attribute_group smb1398_attr_group = {
	.attrs = smb1398_attributes,
};

static int smb1398_div2_cp_master_probe(struct smb1398_chip *chip)
{
	int rc;
	union power_supply_propval pval = {0};

	rc = smb1398_div2_cp_parse_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "parse devicetree failed, rc=%d\n", rc);
		return rc;
	}

	rc = smb1398_div2_cp_hw_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "div2_cp_hw_init failed, rc=%d\n", rc);
		return rc;
	}

	rc = device_init_wakeup(chip->dev, true);
	if (rc < 0) {
		dev_err(chip->dev, "init wakeup failed for div2_cp_master device, rc=%d\n",
				rc);
		return rc;
	}

	rc = smb1398_div2_cp_create_votables(chip);
	if (rc < 0) {
		dev_err(chip->dev, "smb1398_div2_cp_create_votables failed, rc=%d\n",
				rc);
		return rc;
	}

	mutex_init(&chip->die_chan_lock);

	rc = smb1398_init_div2_cp_master_psy(chip);
	if (rc > 0) {
		dev_err(chip->dev, "smb1398_init_div2_cp_master_psy failed, rc=%d\n",
				rc);
		goto destroy_votable;
	}

	if (!chip->batt_psy) {
		chip->batt_psy = power_supply_get_by_name("battery");
		if (!chip->batt_psy) {
			dev_dbg(chip->dev, "Couldn't find battery psy\n");
			rc = -EPROBE_DEFER;
			goto destroy_votable;
		}
	}

	rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_BATT_2S_MODE, &pval);

	chip->batt_2s_chg = pval.intval;

	rc = smb1398_div2_cp_config_ovp(chip);
	if (rc < 0) {
		dev_err(chip->dev, "OVP configuration failed, rc=%d\n",
				rc);
		return rc;
	}

	spin_lock_init(&chip->status_change_lock);
	INIT_WORK(&chip->status_change_work, &smb1398_status_change_work);
	INIT_WORK(&chip->taper_work, &smb1398_taper_work);
	INIT_WORK(&chip->cp_close_work, &smb1398_cp_close_work);
	INIT_WORK(&chip->wireless_close_work, &smb1398_wireless_close_work);
	INIT_WORK(&chip->wireless_open_work, &smb1398_wireless_open_work);
	INIT_WORK(&chip->isns_process_work, &smb1398_isns_process_work);
	INIT_DELAYED_WORK(&chip->irev_process_work, &smb1398_irev_process_work);
	INIT_DELAYED_WORK(&chip->high_power_monitor_work, &smb1398_high_power_monitor_work);
	INIT_DELAYED_WORK(&chip->stop_rx_ocp_work, &smb1398_stop_rx_ocp_work);
	chip->max_power_cnt = 0;
	chip->nb.notifier_call = smb1398_notifier_cb;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		dev_err(chip->dev, "register notifier_cb failed, rc=%d\n", rc);
		goto destroy_votable;
	}

	rc = smb1398_request_interrupts(chip);
	if (rc < 0) {
		dev_err(chip->dev, "smb1398_request_interrupts failed, rc=%d\n",
				rc);
		goto destroy_votable;
	}

	rc = sysfs_create_group(&chip->dev->kobj, &smb1398_attr_group);
	if (rc)
		dev_err(chip->dev,  "Failed to register sysfs:%d\n", rc);

	dev_info(chip->dev, "smb1398 DIV2_CP master is probed successfully\n");

	return 0;
destroy_votable:
	mutex_destroy(&chip->die_chan_lock);
	smb1398_destroy_votables(chip);

	return rc;
}

static enum power_supply_property div2_cp_slave_props[] = {
	POWER_SUPPLY_PROP_CP_ENABLE,
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
	POWER_SUPPLY_PROP_CP_PASSTHROUGH_MODE,
	POWER_SUPPLY_PROP_CP_OVP_CONFIG,
	POWER_SUPPLY_PROP_CP_OCP_CONFIG,
	POWER_SUPPLY_PROP_CP_CFLY_SS_STATUS,
	POWER_SUPPLY_PROP_CP_ILIM,
	POWER_SUPPLY_PROP_RESET_DIV_2_MODE,
};

static int div2_cp_slave_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct smb1398_chip *chip = power_supply_get_drvdata(psy);
	u8 val;
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CP_ENABLE:
		pval->intval = chip->switcher_en;
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		rc = smb1398_read(chip, REVID_REVISION4, &val);
		if (!rc)
			pval->intval = true;
		else
			pval->intval = false;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		pval->intval = 0;
		if (!chip->div2_cp_ilim_votable)
			chip->div2_cp_ilim_votable =
				find_votable("CP_ILIM");
		if (chip->div2_cp_ilim_votable)
			pval->intval = get_effective_result_locked(
					chip->div2_cp_ilim_votable);
		break;
	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
		pval->intval = (int)chip->current_capability;
		break;
	case POWER_SUPPLY_PROP_CP_PASSTHROUGH_MODE:
		pval->intval = chip->slave_pass_mode;
		break;
	case POWER_SUPPLY_PROP_CP_OVP_CONFIG:
		pval->intval = chip->target_ovp_uv;
		break;
	case POWER_SUPPLY_PROP_CP_ILIM:
		pval->intval = chip->force_ilimt;
		break;
	case POWER_SUPPLY_PROP_RESET_DIV_2_MODE:
		pval->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CP_CFLY_SS_STATUS:
		rc = smb1398_read(chip, PERPH1_INT_RT_STS_REG, &val);
		if (rc < 0)
			return rc;
		pval->intval = (int)((val & DIV2_CFLY_SS_DONE_STS) >> 1);
		break;
	case POWER_SUPPLY_PROP_CP_OCP_CONFIG:
		pval->intval = smb1398_get_ocp(chip);
		break;
	default:
		dev_err(chip->dev, "read div2_cp_slave property %d is not supported\n",
				prop);
		return -EINVAL;
	}

	return 0;
}

static int div2_cp_slave_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *pval)
{
	struct smb1398_chip *chip = power_supply_get_drvdata(psy);
	int ilim_ma, rc = 0;
	enum isns_mode mode;

	switch (prop) {
	case POWER_SUPPLY_PROP_CP_ENABLE:
		rc = smb1398_div2_cp_switcher_en(chip, !!pval->intval);
		rc = smb1398_slave_switche_en(chip, !!pval->intval);
		break;
	case POWER_SUPPLY_PROP_CP_ILIM:
		chip->force_ilimt = pval->intval;
		rc = smb1398_set_iin_ma(chip, chip->force_ilimt);
		break;
	case POWER_SUPPLY_PROP_RESET_DIV_2_MODE:
		rc = smb1398_ovp_ignore_mode(chip, pval->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		ilim_ma = pval->intval / 1000;
		if (chip->force_ilimt != 0)
			ilim_ma = chip->force_ilimt / 1000;
		rc = smb1398_set_iin_ma(chip, ilim_ma);
		break;
	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
		mode = (enum isns_mode)pval->intval;
		rc = smb1398_div2_cp_isns_mode_control(chip, mode);
		if (rc < 0)
			return rc;
		chip->current_capability = mode;
		break;
	case POWER_SUPPLY_PROP_CP_PASSTHROUGH_MODE:
		if (pval->intval)
			rc = smb1398_masked_write(chip, CFG_3LVL_BK_CURRENT,
					CFG_HSON_SEL, 0);//Disable OCP
		rc = smb1398_masked_write(chip, SDCDC_1,
				EN_CHECK_CFLY_SSDONE_BIT,
				!!pval->intval ? 0 : EN_CHECK_CFLY_SSDONE_BIT);
		if (rc < 0)
			return rc;
		rc = smb1398_masked_write(chip, MISC_CFG0_REG,
				CFG_ITLGS_2P_BIT,
				!!pval->intval ? CFG_ITLGS_2P_BIT : 0);
		chip->slave_pass_mode = !!pval->intval;
		/*
		 * Adjust ILIM for 1:1 or 1:2 when transitioning
		 * between passthrough and charge pump modes
		 */
		rerun_election(chip->fcc_votable);
		break;
	case POWER_SUPPLY_PROP_CP_OVP_CONFIG:
		rc = smb1398_div2_cp_slave_config_ovp(chip);
		if (rc < 0)
			return rc;
		break;
	case POWER_SUPPLY_PROP_CP_OCP_CONFIG:
		rc = smb1398_set_ocp(chip, pval->intval);
		break;
	default:
		dev_err(chip->dev, "write div2_cp_slave property %d is not supported\n",
				prop);
		return -EINVAL;
	}

	return rc;
}

static int div2_cp_slave_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CP_ENABLE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
	case POWER_SUPPLY_PROP_CP_ILIM:
	case POWER_SUPPLY_PROP_RESET_DIV_2_MODE:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc div2_cps_psy_desc = {
	.name = "cp_slave",
	.type = POWER_SUPPLY_TYPE_PARALLEL,
	.properties = div2_cp_slave_props,
	.num_properties = ARRAY_SIZE(div2_cp_slave_props),
	.get_property = div2_cp_slave_get_prop,
	.set_property = div2_cp_slave_set_prop,
	.property_is_writeable = div2_cp_slave_is_writeable,
};

static int smb1398_init_div2_cp_slave_psy(struct smb1398_chip *chip)
{
	int rc = 0;
	struct power_supply_config cps_cfg = {};

	cps_cfg.drv_data = chip;
	cps_cfg.of_node = chip->dev->of_node;

	chip->div2_cp_slave_psy = devm_power_supply_register(chip->dev,
			&div2_cps_psy_desc, &cps_cfg);
	if (IS_ERR(chip->div2_cp_slave_psy)) {
		rc = PTR_ERR(chip->div2_cp_slave_psy);
		dev_err(chip->dev, "register div2_cp_slave_psy failed, rc=%d\n",
				rc);
		return rc;
	}

	return 0;
}

static void smb1398_clean_irq(struct smb1398_chip *chip)
{
	smb1398_masked_write(chip, SCHG_PERPH_INT_CLR, 0xFF, 0xEF);
	smb1398_masked_write(chip, SCHG_PERPH_INT_CLR, 0xFF, 0xFF);

	smb1398_masked_write(chip, SCHG_PERPH0_INT_CLR, 0xFF, 0xEF);
	smb1398_masked_write(chip, SCHG_PERPH0_INT_CLR, 0xFF, 0xFF);

	smb1398_masked_write(chip, SCHG_PERPH1_INT_CLR, 0xFF, 0xEF);
	smb1398_masked_write(chip, SCHG_PERPH1_INT_CLR, 0xFF, 0xFF);
}

static int smb1398_div2_cp_slave_probe(struct smb1398_chip *chip)
{
	int rc = 0;
	u8 status;

	/* Configure window (Vin/2 - Vout) OV level to 500mV */
	rc = smb1398_masked_write(chip, DIV2_PROTECTION_REG,
			DIV2_WIN_OV_SEL_MASK, WIN_OV_500_MV);
	if (rc < 0) {
		dev_err(chip->dev, "set WIN_OV_500_MV failed, rc=%d\n", rc);
		return rc;
	}

	rc = smb1398_read(chip, MODE_STATUS_REG, &status);
	if (rc < 0) {
		dev_err(chip->dev, "Read slave MODE_STATUS_REG failed, rc=%d\n",
				rc);
		return rc;
	}

	/*
	 * Disable slave WIN_UV detection, otherwise slave might not be
	 * enabled due to WIN_UV until master drawing very high current.
	 */
	rc = smb1398_masked_write(chip, PERPH0_CFG_SDCDC_REG, EN_WIN_UV_BIT, EN_WIN_UV_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "disable DIV2_CP WIN_UV failed, rc=%d\n",
				rc);
		return rc;
	}

	/* Configure slave TEMP pin to HIGH-Z by default */
	rc = smb1398_masked_write(chip, SSUPLY_TEMP_CTRL_REG,
			SEL_OUT_TEMP_MAX_MASK, SEL_OUT_HIGHZ);
	if (rc < 0) {
		dev_err(chip->dev, "set SSUPLY_TEMP_CTRL_REG failed, rc=%d\n",
				rc);
		return rc;
	}

	/* Configure to use Vtemp */
	rc = smb1398_masked_write(chip, PERPH0_MISC_CFG2_REG,
			CFG_TEMP_PIN_ITEMP, 0);
	if (rc < 0) {
		dev_err(chip->dev, "set PERPH0_MISC_CFG2_REG failed, rc=%d\n",
				rc);
		return rc;
	}

	/* switcher enable controlled by register */
	rc = smb1398_masked_write(chip, MISC_CFG0_REG,
			SW_EN_SWITCHER_BIT, SW_EN_SWITCHER_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "set CFG_EN_SOURCE failed, rc=%d\n",
				rc);
		return rc;
	}

	rc = smb1398_masked_write(chip, MISC_SL_SWITCH_EN_REG,
			EN_SWITCHER, 0);
	if (rc < 0) {
		dev_err(chip->dev, "write SWITCH_EN_REG failed, rc=%d\n", rc);
		return rc;
	}

	rc = smb1398_masked_write(chip, IREV_TUNE_OFF_Q1_DELAY_REG,
			IREV_DELAY_BIT, 0);

	smb1398_set_ovlo(chip, USBIN_OVP_22_2V);

	/*Enable Standalone mode*/
	if (chip->standalone_mode)
		rc = smb1398_masked_write(chip, NOLOCK_SPARE_REG,
			STANDALONE_EN_BIT, STANDALONE_EN_BIT);
	else
		rc = smb1398_masked_write(chip, NOLOCK_SPARE_REG,
			STANDALONE_EN_BIT, 0);

	rc = smb1398_masked_write(chip, SDCDC_1,
			EN_CHECK_CFLY_SSDONE_BIT, EN_CHECK_CFLY_SSDONE_BIT);
	rc = smb1398_masked_write(chip, MISC_CFG0_REG,
			CFG_ITLGS_2P_BIT, 0);//Disable Passthrough

	rc = smb1398_init_div2_cp_slave_psy(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Initial div2_cp_slave_psy failed, rc=%d\n",
				rc);
		return rc;
	}

	if (!chip->div2_cp_ilim_votable)
		chip->div2_cp_ilim_votable = find_votable("CP_ILIM");
	if (!chip->div2_cp_ilim_votable)
		return -EPROBE_DEFER;

	smb1398_clean_irq(chip);

	dev_info(chip->dev, "smb1398 DIV2_CP slave probe successfully\n");

	return 0;
}

static int smb1398_pre_regulator_iout_vote_cb(struct votable *votable,
		void *data, int iout_ua, const char *client)
{
	struct smb1398_chip *chip = (struct smb1398_chip *)data;
	int rc = 0;

	if (chip->in_suspend)
		return -EAGAIN;

	if (!client)
		return -EINVAL;

	iout_ua = min(iout_ua, MAX_IOUT_UA);
	rc = smb1398_set_ichg_ma(chip, iout_ua / 1000);
	if (rc < 0)
		return rc;

	dev_dbg(chip->dev, "set iout %duA\n", iout_ua);
	return 0;
}

static int smb1398_pre_regulator_vout_vote_cb(struct votable *votable,
		void *data, int vout_uv, const char *client)
{
	struct smb1398_chip *chip = (struct smb1398_chip *)data;
	int rc = 0;

	if (chip->in_suspend)
		return -EAGAIN;

	if (!client)
		return -EINVAL;

	vout_uv = min(vout_uv, MAX_1S_VOUT_UV);
	rc = smb1398_set_1s_vout_mv(chip, vout_uv / 1000);
	if (rc < 0)
		return rc;

	dev_dbg(chip->dev, "set vout %duV\n", vout_uv);
	return 0;
}

static int smb1398_create_pre_regulator_votables(struct smb1398_chip *chip)
{
	chip->pre_regulator_iout_votable = create_votable("PRE_REGULATOR_IOUT",
			VOTE_MIN, smb1398_pre_regulator_iout_vote_cb, chip);
	if (IS_ERR_OR_NULL(chip->pre_regulator_iout_votable))
		return PTR_ERR_OR_ZERO(chip->pre_regulator_iout_votable);

	chip->pre_regulator_vout_votable = create_votable("PRE_REGULATOR_VOUT",
			VOTE_MIN, smb1398_pre_regulator_vout_vote_cb, chip);

	if (IS_ERR_OR_NULL(chip->pre_regulator_vout_votable)) {
		destroy_votable(chip->pre_regulator_iout_votable);
		return PTR_ERR_OR_ZERO(chip->pre_regulator_vout_votable);
	}

	return 0;
}

static enum power_supply_property pre_regulator_props[] = {
	POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
};

static int pre_regulator_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct smb1398_chip *chip = power_supply_get_drvdata(psy);
	int rc, iin_ma, iout_ma, vout_mv;

	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		rc = smb1398_get_iin_ma(chip, &iin_ma);
		if (rc < 0)
			return rc;
		pval->intval = iin_ma * 1000;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		if (chip->pre_regulator_iout_votable) {
			pval->intval = get_effective_result(
					chip->pre_regulator_iout_votable);
		} else {
			rc = smb1398_get_ichg_ma(chip, &iout_ma);
			if (rc < 0)
				return rc;
			pval->intval = iout_ma * 1000;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		if (chip->pre_regulator_vout_votable) {
			pval->intval = get_effective_result(
					chip->pre_regulator_vout_votable);
		} else {
			rc = smb1398_get_1s_vout_mv(chip, &vout_mv);
			if (rc < 0)
				return rc;
			pval->intval = vout_mv * 1000;
		}
		break;
	default:
		dev_err(chip->dev, "read pre_regulator property %d is not supported\n",
				prop);
		return -EINVAL;
	}

	return 0;
}

static int pre_regulator_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *pval)
{
	struct smb1398_chip *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		rc = smb1398_set_iin_ma(chip, pval->intval / 1000);
		if (rc < 0)
			return rc;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		vote(chip->pre_regulator_iout_votable, CP_VOTER,
				true, pval->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		vote(chip->pre_regulator_vout_votable, CP_VOTER,
				true, pval->intval);
		break;
	default:
		dev_err(chip->dev, "write pre_regulator property %d is not supported\n",
				prop);
		return -EINVAL;
	}

	return rc;
}

static int pre_regulator_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc pre_regulator_psy_desc = {
	.name = "pre_regulator",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = pre_regulator_props,
	.num_properties = ARRAY_SIZE(pre_regulator_props),
	.get_property = pre_regulator_get_prop,
	.set_property = pre_regulator_set_prop,
	.property_is_writeable = pre_regulator_is_writeable,
};

static int smb1398_create_pre_regulator_psy(struct smb1398_chip *chip)
{
	struct power_supply_config pre_regulator_psy_cfg = {};
	int rc = 0;

	pre_regulator_psy_cfg.drv_data = chip;
	pre_regulator_psy_cfg.of_node = chip->dev->of_node;

	chip->pre_regulator_psy = devm_power_supply_register(chip->dev,
			&pre_regulator_psy_desc,
			&pre_regulator_psy_cfg);
	if (IS_ERR(chip->pre_regulator_psy)) {
		rc = PTR_ERR(chip->pre_regulator_psy);
		dev_err(chip->dev, "register pre_regulator psy failed, rc=%d\n",
				rc);
		return rc;
	}

	return 0;
}

static int smb1398_pre_regulator_probe(struct smb1398_chip *chip)
{
	int rc = 0;

	rc = smb1398_create_pre_regulator_votables(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Create votable for pre_regulator failed, rc=%d\n",
				rc);
		return rc;
	}

	rc = smb1398_create_pre_regulator_psy(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Create pre-regulator failed, rc=%d\n",
				rc);
		return rc;
	}

	return 0;

}

static int smb1398_probe(struct platform_device *pdev)
{
	struct smb1398_chip *chip;
	int rc = 0;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->die_temp = -ENODATA;
	chip->dev = &pdev->dev;
	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		dev_err(chip->dev, "Get regmap failed\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, chip);

	chip->standalone_mode = true;
	chip->div2_cp_role = (int)of_device_get_match_data(chip->dev);
	switch (chip->div2_cp_role) {
	case DIV2_CP_MASTER:
		rc = smb1398_div2_cp_master_probe(chip);
		break;
	case DIV2_CP_SLAVE:
		rc = smb1398_div2_cp_slave_probe(chip);
		break;
	case COMBO_PRE_REGULATOR:
		rc = smb1398_pre_regulator_probe(chip);
		break;
	default:
		dev_err(chip->dev, "Couldn't find a match role for %d\n",
				chip->div2_cp_role);
		goto cleanup;
	}

	if (rc < 0) {
		if (rc != -EPROBE_DEFER)
			dev_err(chip->dev, "Couldn't probe SMB1390 %s rc= %d\n",
				!!chip->div2_cp_role ? "slave" : "master", rc);
		goto cleanup;
	}
	return 0;

cleanup:
	platform_set_drvdata(pdev, NULL);
	return rc;
}

static int smb1398_remove(struct platform_device *pdev)
{
	struct smb1398_chip *chip = platform_get_drvdata(pdev);

	if (chip->div2_cp_role == DIV2_CP_MASTER) {
		vote(chip->awake_votable, SHUTDOWN_VOTER, false, 0);
		vote(chip->div2_cp_disable_votable, SHUTDOWN_VOTER, true, 0);
		vote(chip->div2_cp_ilim_votable, SHUTDOWN_VOTER, true, 0);
		if (chip->passthrough_mode_allowed)
			vote(chip->passthrough_mode_disable_votable, SHUTDOWN_VOTER, true, 0);
		cancel_work_sync(&chip->taper_work);
		cancel_work_sync(&chip->status_change_work);
		cancel_work_sync(&chip->cp_close_work);
		cancel_work_sync(&chip->wireless_close_work);
		cancel_work_sync(&chip->wireless_open_work);
		cancel_work_sync(&chip->isns_process_work);
		cancel_delayed_work_sync(&chip->irev_process_work);
		mutex_destroy(&chip->die_chan_lock);
		smb1398_destroy_votables(chip);
	}

	return 0;
}

static void smb1398_shutdown(struct platform_device *pdev)
{
	struct smb1398_chip *chip = platform_get_drvdata(pdev);
	int rc;

	rc = smb1398_masked_write(chip, MISC_SL_SWITCH_EN_REG,
			EN_SWITCHER, 0);
	if (rc < 0) {
		dev_err(chip->dev, "write SWITCH_EN_REG failed, rc=%d\n", rc);
		return;
	}

	rc = smb1398_masked_write(chip, SDCDC_1,
			EN_CHECK_CFLY_SSDONE_BIT, EN_CHECK_CFLY_SSDONE_BIT);
	rc = smb1398_masked_write(chip, MISC_CFG0_REG,
			CFG_ITLGS_2P_BIT, 0);//Disable Passthrough
}

static int smb1398_suspend(struct device *dev)
{
	struct smb1398_chip *chip = dev_get_drvdata(dev);
	chip->in_suspend = true;
	return 0;
}

static int smb1398_resume(struct device *dev)
{
	struct smb1398_chip *chip = dev_get_drvdata(dev);

	chip->in_suspend = false;

	return 0;
}

static const struct dev_pm_ops smb1398_pm_ops = {
	.suspend	= smb1398_suspend,
	.resume		= smb1398_resume,
};

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,smb1396-div2-cp-master",
	  .data = (void *)DIV2_CP_MASTER,
	},
	{ .compatible = "qcom,smb1396-div2-cp-slave",
	  .data = (void *)DIV2_CP_SLAVE,
	},
	{ .compatible = "qcom,smb1398-pre-regulator",
	  .data = (void *)COMBO_PRE_REGULATOR,
	},
	{
	},
};


static struct platform_driver smb1398_driver = {
	.driver	= {
		.name		= "qcom,smb1398-charger",
		.pm		= &smb1398_pm_ops,
		.of_match_table	= match_table,
	},
	.probe	= smb1398_probe,
	.remove	= smb1398_remove,
	.shutdown = smb1398_shutdown
};
module_platform_driver(smb1398_driver);

MODULE_DESCRIPTION("SMB1398 charger driver");
MODULE_LICENSE("GPL v2");
