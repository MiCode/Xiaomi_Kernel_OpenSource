/* Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/pinctrl/consumer.h>

/* Mask/Bit helpers */
#define _SMB1351_MASK(BITS, POS) \
	((unsigned char)(((1 << (BITS)) - 1) << (POS)))
#define SMB1351_MASK(LEFT_BIT_POS, RIGHT_BIT_POS) \
		_SMB1351_MASK((LEFT_BIT_POS) - (RIGHT_BIT_POS) + 1, \
				(RIGHT_BIT_POS))

/* Configuration registers */
#define CHG_CURRENT_CTRL_REG			0x0
#define FAST_CHG_CURRENT_MASK			SMB1351_MASK(7, 4)
#define AC_INPUT_CURRENT_LIMIT_MASK		SMB1351_MASK(3, 0)

#define CHG_OTH_CURRENT_CTRL_REG		0x1
#define PRECHG_CURRENT_MASK			SMB1351_MASK(7, 5)
#define ITERM_MASK				SMB1351_MASK(4, 2)
#define USB_2_3_MODE_SEL_BIT			BIT(1)
#define USB_2_3_MODE_SEL_BY_I2C			0
#define USB_2_3_MODE_SEL_BY_PIN			0x2
#define USB_5_1_CMD_POLARITY_BIT		BIT(0)
#define USB_CMD_POLARITY_500_1_100_0		0
#define USB_CMD_POLARITY_500_0_100_1		0x1

#define VARIOUS_FUNC_REG			0x2
#define SUSPEND_MODE_CTRL_BIT			BIT(7)
#define SUSPEND_MODE_CTRL_BY_PIN		0
#define SUSPEND_MODE_CTRL_BY_I2C		0x80
#define BATT_TO_SYS_POWER_CTRL_BIT		BIT(6)
#define MAX_SYS_VOLTAGE				BIT(5)
#define AICL_EN_BIT				BIT(4)
#define AICL_DET_TH_BIT				BIT(3)
#define APSD_EN_BIT				BIT(2)
#define BATT_OV_BIT				BIT(1)
#define VCHG_FUNC_BIT				BIT(0)

#define VFLOAT_REG				0x3
#define PRECHG_TO_FAST_VOLTAGE_CFG_MASK		SMB1351_MASK(7, 6)
#define VFLOAT_MASK				SMB1351_MASK(5, 0)

#define CHG_CTRL_REG				0x4
#define AUTO_RECHG_BIT				BIT(7)
#define AUTO_RECHG_ENABLE			0
#define AUTO_RECHG_DISABLE			0x80
#define ITERM_EN_BIT				BIT(6)
#define ITERM_ENABLE				0
#define ITERM_DISABLE				0x40
#define MAPPED_AC_INPUT_CURRENT_LIMIT_MASK	SMB1351_MASK(5, 4)
#define AUTO_RECHG_TH_BIT			BIT(3)
#define AUTO_RECHG_TH_50MV			0
#define AUTO_RECHG_TH_100MV			0x8
#define AFCV_MASK				SMB1351_MASK(2, 0)

#define CHG_STAT_TIMERS_CTRL_REG		0x5
#define STAT_OUTPUT_POLARITY_BIT		BIT(7)
#define STAT_OUTPUT_MODE_BIT			BIT(6)
#define STAT_OUTPUT_CTRL_BIT			BIT(5)
#define OTH_CHG_IL_BIT				BIT(4)
#define COMPLETE_CHG_TIMEOUT_MASK		SMB1351_MASK(3, 2)
#define PRECHG_TIMEOUT_MASK			SMB1351_MASK(1, 0)

#define CHG_PIN_EN_CTRL_REG			0x6
#define LED_BLINK_FUNC_BIT			BIT(7)
#define EN_PIN_CTRL_MASK			SMB1351_MASK(6, 5)
#define EN_BY_I2C_0_DISABLE			0
#define EN_BY_I2C_0_ENABLE			0x20
#define EN_BY_PIN_HIGH_ENABLE			0x40
#define EN_BY_PIN_LOW_ENABLE			0x60
#define USBCS_CTRL_BIT				BIT(4)
#define USBCS_CTRL_BY_I2C			0
#define USBCS_CTRL_BY_PIN			0x10
#define USBCS_INPUT_STATE_BIT			BIT(3)
#define CHG_ERR_BIT				BIT(2)
#define APSD_DONE_BIT				BIT(1)
#define USB_FAIL_BIT				BIT(0)

#define THERM_A_CTRL_REG			0x7
#define MIN_SYS_VOLTAGE_MASK			SMB1351_MASK(7, 6)
#define LOAD_BATT_10MA_FVC_BIT			BIT(5)
#define THERM_MONITOR_BIT			BIT(4)
#define THERM_MONITOR_EN			0
#define SOFT_COLD_TEMP_LIMIT_MASK		SMB1351_MASK(3, 2)
#define SOFT_HOT_TEMP_LIMIT_MASK		SMB1351_MASK(1, 0)

#define WDOG_SAFETY_TIMER_CTRL_REG		0x8
#define AICL_FAIL_OPTION_BIT			BIT(7)
#define AICL_FAIL_TO_SUSPEND			0
#define AICL_FAIL_TO_150_MA			0x80
#define WDOG_TIMEOUT_MASK			SMB1351_MASK(6, 5)
#define WDOG_IRQ_SAFETY_TIMER_MASK		SMB1351_MASK(4, 3)
#define WDOG_IRQ_SAFETY_TIMER_EN_BIT		BIT(2)
#define WDOG_OPTION_BIT				BIT(1)
#define WDOG_TIMER_EN_BIT			BIT(0)

#define OTG_USBIN_AICL_CTRL_REG			0x9
#define OTG_ID_PIN_CTRL_MASK			SMB1351_MASK(7, 6)
#define OTG_PIN_POLARITY_BIT			BIT(5)
#define DCIN_IC_GLITCH_FILTER_HV_ADAPTER_MASK	SMB1351_MASK(4, 3)
#define DCIN_IC_GLITCH_FILTER_LV_ADAPTER_BIT	BIT(2)
#define USBIN_AICL_CFG1_BIT			BIT(1)
#define USBIN_AICL_CFG0_BIT			BIT(0)

#define OTG_TLIM_CTRL_REG			0xA
#define SWITCH_FREQ_MASK			SMB1351_MASK(7, 6)
#define THERM_LOOP_TEMP_SEL_MASK		SMB1351_MASK(5, 4)
#define OTG_OC_LIMIT_MASK			SMB1351_MASK(3, 2)
#define OTG_BATT_UVLO_TH_MASK			SMB1351_MASK(1, 0)

#define HARD_SOFT_LIMIT_CELL_TEMP_REG		0xB
#define HARD_LIMIT_COLD_TEMP_ALARM_TRIP_MASK	SMB1351_MASK(7, 6)
#define HARD_LIMIT_HOT_TEMP_ALARM_TRIP_MASK	SMB1351_MASK(5, 4)
#define SOFT_LIMIT_COLD_TEMP_ALARM_TRIP_MASK	SMB1351_MASK(3, 2)
#define SOFT_LIMIT_HOT_TEMP_ALARM_TRIP_MASK	SMB1351_MASK(1, 0)

#define FAULT_INT_REG				0xC
#define HOT_COLD_HARD_LIMIT_BIT			BIT(7)
#define HOT_COLD_SOFT_LIMIT_BIT			BIT(6)
#define BATT_UVLO_IN_OTG_BIT			BIT(5)
#define OTG_OC_BIT				BIT(4)
#define INPUT_OVLO_BIT				BIT(3)
#define INPUT_UVLO_BIT				BIT(2)
#define AICL_DONE_FAIL_BIT			BIT(1)
#define INTERNAL_OVER_TEMP_BIT			BIT(0)

#define STATUS_INT_REG				0xD
#define CHG_OR_PRECHG_TIMEOUT_BIT		BIT(7)
#define RID_CHANGE_BIT				BIT(6)
#define BATT_OVP_BIT				BIT(5)
#define FAST_TERM_TAPER_RECHG_INHIBIT_BIT	BIT(4)
#define WDOG_TIMER_BIT				BIT(3)
#define POK_BIT					BIT(2)
#define BATT_MISSING_BIT			BIT(1)
#define BATT_LOW_BIT				BIT(0)

#define VARIOUS_FUNC_2_REG			0xE
#define CHG_HOLD_OFF_TIMER_AFTER_PLUGIN_BIT	BIT(7)
#define CHG_INHIBIT_BIT				BIT(6)
#define FAST_CHG_CC_IN_BATT_SOFT_LIMIT_MODE_BIT	BIT(5)
#define FVCL_IN_BATT_SOFT_LIMIT_MODE_MASK	SMB1351_MASK(4, 3)
#define HARD_TEMP_LIMIT_BEHAVIOR_BIT		BIT(2)
#define PRECHG_TO_FASTCHG_BIT			BIT(1)
#define STAT_PIN_CONFIG_BIT			BIT(0)

#define FLEXCHARGER_REG				0x10
#define AFVC_IRQ_BIT				BIT(7)
#define CHG_CONFIG_MASK				SMB1351_MASK(6, 4)
#define LOW_BATT_VOLTAGE_DET_TH_MASK		SMB1351_MASK(3, 0)

#define VARIOUS_FUNC_3_REG			0x11
#define SAFETY_TIMER_EN_MASK			SMB1351_MASK(7, 6)
#define BLOCK_SUSPEND_DURING_VBATT_LOW_BIT	BIT(5)
#define TIMEOUT_SEL_FOR_APSD_BIT		BIT(4)
#define SDP_SUSPEND_BIT				BIT(3)
#define QC_2P1_AUTO_INCREMENT_MODE_BIT		BIT(2)
#define QC_2P1_AUTH_ALGO_BIT			BIT(1)
#define DCD_EN_BIT				BIT(0)

#define HVDCP_BATT_MISSING_CTRL_REG		0x12
#define HVDCP_ADAPTER_SEL_MASK			SMB1351_MASK(7, 6)
#define HVDCP_EN_BIT				BIT(5)
#define HVDCP_AUTO_INCREMENT_LIMIT_BIT		BIT(4)
#define BATT_MISSING_ON_INPUT_PLUGIN_BIT	BIT(3)
#define BATT_MISSING_2P6S_POLLER_BIT		BIT(2)
#define BATT_MISSING_ALGO_BIT			BIT(1)
#define BATT_MISSING_THERM_PIN_SOURCE_BIT	BIT(0)

#define PON_OPTIONS_REG				0x13
#define SYSOK_INOK_POLARITY_BIT			BIT(7)
#define SYSOK_OPTIONS_MASK			SMB1351_MASK(6, 4)
#define INPUT_MISSING_POLLER_CONFIG_BIT		BIT(3)
#define VBATT_LOW_DISABLED_OR_RESET_STATE_BIT	BIT(2)
#define QC_2P1_AUTH_ALGO_IRQ_EN_BIT		BIT(0)

#define OTG_MODE_POWER_OPTIONS_REG		0x14
#define ADAPTER_CONFIG_MASK			SMB1351_MASK(7, 6)
#define MAP_HVDCP_BIT				BIT(5)
#define SDP_LOW_BATT_FORCE_USB5_OVER_USB1_BIT	BIT(4)
#define OTG_HICCUP_MODE_BIT			BIT(2)
#define INPUT_CURRENT_LIMIT_MASK		SMB1351_MASK(1, 0)

#define CHARGER_I2C_CTRL_REG			0x15
#define FULLON_MODE_EN_BIT			BIT(7)
#define I2C_HS_MODE_EN_BIT			BIT(6)
#define SYSON_LDO_OUTPUT_SEL_BIT		BIT(5)
#define VBATT_TRACKING_VOLTAGE_DIFF_BIT		BIT(4)
#define DISABLE_AFVC_WHEN_ENTER_TAPER_BIT	BIT(3)
#define VCHG_IINV_BIT				BIT(2)
#define AFVC_OVERRIDE_BIT			BIT(1)
#define SYSOK_PIN_CONFIG_BIT			BIT(0)

#define VERSION_REG				0x2E
#define VERSION_MASK				BIT(1)

/* Command registers */
#define CMD_I2C_REG				0x30
#define CMD_RELOAD_BIT				BIT(7)
#define CMD_BQ_CFG_ACCESS_BIT			BIT(6)

#define CMD_INPUT_LIMIT_REG			0x31
#define CMD_OVERRIDE_BIT			BIT(7)
#define CMD_SUSPEND_MODE_BIT			BIT(6)
#define CMD_INPUT_CURRENT_MODE_BIT		BIT(3)
#define CMD_INPUT_CURRENT_MODE_APSD		0
#define CMD_INPUT_CURRENT_MODE_CMD		0x08
#define CMD_USB_2_3_SEL_BIT			BIT(2)
#define CMD_USB_2_MODE				0
#define CMD_USB_3_MODE				0x4
#define CMD_USB_1_5_AC_CTRL_MASK		SMB1351_MASK(1, 0)
#define CMD_USB_100_MODE			0
#define CMD_USB_500_MODE			0x2
#define CMD_USB_AC_MODE				0x1

#define CMD_CHG_REG				0x32
#define CMD_DISABLE_THERM_MONITOR_BIT		BIT(4)
#define CMD_TURN_OFF_STAT_PIN_BIT		BIT(3)
#define CMD_PRE_TO_FAST_EN_BIT			BIT(2)
#define CMD_CHG_EN_BIT				BIT(1)
#define CMD_CHG_DISABLE				0
#define CMD_CHG_ENABLE				0x2
#define CMD_OTG_EN_BIT				BIT(0)

#define CMD_DEAD_BATT_REG			0x33
#define CMD_STOP_DEAD_BATT_TIMER_MASK		SMB1351_MASK(7, 0)

#define CMD_HVDCP_REG				0x34
#define CMD_APSD_RE_RUN_BIT			BIT(7)
#define CMD_FORCE_HVDCP_2P0_BIT			BIT(5)
#define CMD_HVDCP_MODE_MASK			SMB1351_MASK(5, 0)

/* Status registers */
#define STATUS_0_REG				0x36
#define STATUS_AICL_BIT				BIT(7)
#define STATUS_INPUT_CURRENT_LIMIT_MASK		SMB1351_MASK(6, 5)
#define STATUS_DCIN_INPUT_CURRENT_LIMIT_MASK	SMB1351_MASK(4, 0)

#define STATUS_1_REG				0x37
#define STATUS_INPUT_RANGE_MASK			SMB1351_MASK(7, 4)
#define STATUS_INPUT_USB_BIT			BIT(0)

#define STATUS_2_REG				0x38
#define STATUS_FAST_CHG_BIT			BIT(7)
#define STATUS_HARD_LIMIT_BIT			BIT(6)
#define STATUS_FLOAT_VOLTAGE_MASK		SMB1351_MASK(5, 0)

#define STATUS_3_REG				0x39
#define STATUS_CHG_BIT				BIT(7)
#define STATUS_PRECHG_CURRENT_MASK		SMB1351_MASK(6, 4)
#define STATUS_FAST_CHG_CURRENT_MASK		SMB1351_MASK(3, 0)

#define STATUS_4_REG				0x3A
#define STATUS_OTG_BIT				BIT(7)
#define STATUS_AFVC_BIT				BIT(6)
#define STATUS_DONE_BIT				BIT(5)
#define STATUS_BATT_LESS_THAN_2V_BIT		BIT(4)
#define STATUS_HOLD_OFF_BIT			BIT(3)
#define STATUS_CHG_MASK				SMB1351_MASK(2, 1)
#define STATUS_NO_CHARGING			0
#define STATUS_FAST_CHARGING			0x4
#define STATUS_PRE_CHARGING			0x2
#define STATUS_TAPER_CHARGING			0x6
#define STATUS_CHG_EN_STATUS_BIT		BIT(0)

#define STATUS_5_REG				0x3B
#define STATUS_SOURCE_DETECTED_MASK		SMB1351_MASK(7, 0)
#define STATUS_PORT_CDP				0x80
#define STATUS_PORT_DCP				0x40
#define STATUS_PORT_OTHER			0x20
#define STATUS_PORT_SDP				0x10
#define STATUS_PORT_ACA_A			0x8
#define STATUS_PORT_ACA_B			0x4
#define STATUS_PORT_ACA_C			0x2
#define STATUS_PORT_ACA_DOCK			0x1

#define STATUS_6_REG				0x3C
#define STATUS_DCD_TIMEOUT_BIT			BIT(7)
#define STATUS_DCD_GOOD_DG_BIT			BIT(6)
#define STATUS_OCD_GOOD_DG_BIT			BIT(5)
#define STATUS_RID_ABD_DG_BIT			BIT(4)
#define STATUS_RID_FLOAT_STATE_MACHINE_BIT	BIT(3)
#define STATUS_RID_A_STATE_MACHINE_BIT		BIT(2)
#define STATUS_RID_B_STATE_MACHINE_BIT		BIT(1)
#define STATUS_RID_C_STATE_MACHINE_BIT		BIT(0)

#define STATUS_7_REG				0x3D
#define STATUS_HVDCP_MASK			SMB1351_MASK(7, 0)

#define STATUS_8_REG				0x3E
#define STATUS_USNIN_HV_INPUT_SEL_BIT		BIT(5)
#define STATUS_USBIN_LV_UNDER_INPUT_SEL_BIT	BIT(4)
#define STATUS_USBIN_LV_INPUT_SEL_BIT		BIT(3)

/* Revision register */
#define CHG_REVISION_REG			0x3F
#define GUI_REVISION_MASK			SMB1351_MASK(7, 4)
#define DEVICE_REVISION_MASK			SMB1351_MASK(3, 0)

/* IRQ status registers */
#define IRQ_A_REG				0x40
#define IRQ_HOT_HARD_BIT			BIT(6)
#define IRQ_COLD_HARD_BIT			BIT(4)
#define IRQ_HOT_SOFT_BIT			BIT(2)
#define IRQ_COLD_SOFT_BIT			BIT(0)

#define IRQ_B_REG				0x41
#define IRQ_BATT_TERMINAL_REMOVED_BIT		BIT(6)
#define IRQ_BATT_MISSING_BIT			BIT(4)
#define IRQ_LOW_BATT_VOLTAGE_BIT		BIT(2)
#define IRQ_INTERNAL_TEMP_LIMIT_BIT		BIT(0)

#define IRQ_C_REG				0x42
#define IRQ_PRE_TO_FAST_VOLTAGE_BIT		BIT(6)
#define IRQ_RECHG_BIT				BIT(4)
#define IRQ_TAPER_BIT				BIT(2)
#define IRQ_TERM_BIT				BIT(0)

#define IRQ_D_REG				0x43
#define IRQ_BATT_OV_BIT				BIT(6)
#define IRQ_CHG_ERROR_BIT			BIT(4)
#define IRQ_CHG_TIMEOUT_BIT			BIT(2)
#define IRQ_PRECHG_TIMEOUT_BIT			BIT(0)

#define IRQ_E_REG				0x44
#define IRQ_USBIN_OV_BIT			BIT(6)
#define IRQ_USBIN_UV_BIT			BIT(4)
#define IRQ_AFVC_BIT				BIT(2)
#define IRQ_POWER_OK_BIT			BIT(0)

#define IRQ_F_REG				0x45
#define IRQ_OTG_OVER_CURRENT_BIT		BIT(6)
#define IRQ_OTG_FAIL_BIT			BIT(4)
#define IRQ_RID_BIT				BIT(2)
#define IRQ_OTG_OC_RETRY_BIT			BIT(0)

#define IRQ_G_REG				0x46
#define IRQ_SOURCE_DET_BIT			BIT(6)
#define IRQ_AICL_DONE_BIT			BIT(4)
#define IRQ_AICL_FAIL_BIT			BIT(2)
#define IRQ_CHG_INHIBIT_BIT			BIT(0)

#define IRQ_H_REG				0x47
#define IRQ_IC_LIMIT_STATUS_BIT			BIT(5)
#define IRQ_HVDCP_2P1_STATUS_BIT		BIT(4)
#define IRQ_HVDCP_AUTH_DONE_BIT			BIT(2)
#define IRQ_WDOG_TIMEOUT_BIT			BIT(0)

/* constants */
#define USB2_MIN_CURRENT_MA			100
#define USB2_MAX_CURRENT_MA			500
#define USB3_MIN_CURRENT_MA			150
#define USB3_MAX_CURRENT_MA			900
#define SMB1351_IRQ_REG_COUNT			8
#define SMB1351_CHG_PRE_MIN_MA			100
#define SMB1351_CHG_FAST_MIN_MA			1000
#define SMB1351_CHG_FAST_MAX_MA			4500
#define SMB1351_CHG_PRE_SHIFT			5
#define SMB1351_CHG_FAST_SHIFT			4
#define DEFAULT_BATT_CAPACITY			50
#define DEFAULT_BATT_TEMP			250
#define SUSPEND_CURRENT_MA			2

#define CHG_ITERM_200MA				0x0
#define CHG_ITERM_300MA				0x04
#define CHG_ITERM_400MA				0x08
#define CHG_ITERM_500MA				0x0C
#define CHG_ITERM_600MA				0x10
#define CHG_ITERM_700MA				0x14

#define ADC_TM_WARM_COOL_THR_ENABLE		ADC_TM_HIGH_LOW_THR_ENABLE

enum reason {
	USER	= BIT(0),
	THERMAL = BIT(1),
	CURRENT = BIT(2),
	SOC	= BIT(3),
};

static char *pm_batt_supplied_to[] = {
	"bms",
};

struct smb1351_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
};

enum chip_version {
	SMB_UNKNOWN = 0,
	SMB1350,
	SMB1351,
	SMB_MAX_TYPE,
};

static const char *smb1351_version_str[SMB_MAX_TYPE] = {
	[SMB_UNKNOWN] = "Unknown",
	[SMB1350] = "SMB1350",
	[SMB1351] = "SMB1351",
};

struct smb1351_charger {
	struct i2c_client	*client;
	struct device		*dev;

	bool			recharge_disabled;
	int			recharge_mv;
	bool			iterm_disabled;
	int			iterm_ma;
	int			vfloat_mv;
	int			chg_present;
	int			fake_battery_soc;
	bool			chg_autonomous_mode;
	bool			disable_apsd;
	bool			using_pmic_therm;
	bool			jeita_supported;
	bool			battery_missing;
	const char		*bms_psy_name;
	bool			resume_completed;
	bool			irq_waiting;
	struct delayed_work	chg_remove_work;
	struct delayed_work	hvdcp_det_work;

	/* status tracking */
	bool			batt_full;
	bool			batt_hot;
	bool			batt_cold;
	bool			batt_warm;
	bool			batt_cool;

	int			battchg_disabled_status;
	int			usb_suspended_status;
	int			target_fastchg_current_max_ma;
	int			fastchg_current_max_ma;
	int			workaround_flags;

	int			parallel_pin_polarity_setting;
	bool			parallel_charger;
	bool			parallel_charger_present;
	bool			bms_controlled_charging;
	bool			apsd_rerun;
	bool			usbin_ov;
	bool			chg_remove_work_scheduled;
	bool			force_hvdcp_2p0;
	enum chip_version	version;

	/* psy */
	struct power_supply	*usb_psy;
	int			usb_psy_ma;
	struct power_supply	*bms_psy;
	struct power_supply_desc	batt_psy_d;
	struct power_supply	*batt_psy;
	struct power_supply	*parallel_psy;
	struct power_supply_desc	parallel_psy_d;

	struct smb1351_regulator	otg_vreg;
	struct mutex		irq_complete;

	struct dentry		*debug_root;
	u32			peek_poke_address;

	/* adc_tm parameters */
	struct qpnp_vadc_chip	*vadc_dev;
	struct qpnp_adc_tm_chip	*adc_tm_dev;
	struct qpnp_adc_tm_btm_param	adc_param;

	/* jeita parameters */
	int			batt_hot_decidegc;
	int			batt_cold_decidegc;
	int			batt_warm_decidegc;
	int			batt_cool_decidegc;
	int			batt_missing_decidegc;
	unsigned int		batt_warm_ma;
	unsigned int		batt_warm_mv;
	unsigned int		batt_cool_ma;
	unsigned int		batt_cool_mv;

	/* pinctrl parameters */
	const char		*pinctrl_state_name;
	struct pinctrl		*smb_pinctrl;
};

struct smb_irq_info {
	const char		*name;
	int (*smb_irq)(struct smb1351_charger *chip, u8 rt_stat);
	int			high;
	int			low;
};

struct irq_handler_info {
	u8			stat_reg;
	u8			val;
	u8			prev_val;
	struct smb_irq_info	irq_info[4];
};

/* USB input charge current */
static int usb_chg_current[] = {
	500, 685, 1000, 1100, 1200, 1300, 1500, 1600,
	1700, 1800, 2000, 2200, 2500, 3000,
};

static int fast_chg_current[] = {
	1000, 1200, 1400, 1600, 1800, 2000, 2200,
	2400, 2600, 2800, 3000, 3400, 3600, 3800,
	4000, 4640,
};

static int pre_chg_current[] = {
	200, 300, 400, 500, 600, 700,
};

struct battery_status {
	bool			batt_hot;
	bool			batt_warm;
	bool			batt_cool;
	bool			batt_cold;
	bool			batt_present;
};

enum {
	BATT_HOT = 0,
	BATT_WARM,
	BATT_NORMAL,
	BATT_COOL,
	BATT_COLD,
	BATT_MISSING,
	BATT_STATUS_MAX,
};

static struct battery_status batt_s[] = {
	[BATT_HOT] = {1, 0, 0, 0, 1},
	[BATT_WARM] = {0, 1, 0, 0, 1},
	[BATT_NORMAL] = {0, 0, 0, 0, 1},
	[BATT_COOL] = {0, 0, 1, 0, 1},
	[BATT_COLD] = {0, 0, 0, 1, 1},
	[BATT_MISSING] = {0, 0, 0, 1, 0},
};

static int smb1351_read_reg(struct smb1351_charger *chip, int reg, u8 *val)
{
	s32 ret;

	pm_stay_awake(chip->dev);
	ret = i2c_smbus_read_byte_data(chip->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from %02x: %d\n", reg, ret);
		pm_relax(chip->dev);
		return ret;
	} else {
		*val = ret;
	}
	pm_relax(chip->dev);
	pr_debug("Reading 0x%02x=0x%02x\n", reg, *val);
	return 0;
}

static int smb1351_write_reg(struct smb1351_charger *chip, int reg, u8 val)
{
	s32 ret;

	pm_stay_awake(chip->dev);
	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		pm_relax(chip->dev);
		return ret;
	}
	pm_relax(chip->dev);
	pr_debug("Writing 0x%02x=0x%02x\n", reg, val);
	return 0;
}

static int smb1351_masked_write(struct smb1351_charger *chip, int reg,
							u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	rc = smb1351_read_reg(chip, reg, &temp);
	if (rc) {
		pr_err("read failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = smb1351_write_reg(chip, reg, temp);
	if (rc) {
		pr_err("write failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	return 0;
}

static int smb1351_enable_volatile_writes(struct smb1351_charger *chip)
{
	int rc;

	rc = smb1351_masked_write(chip, CMD_I2C_REG, CMD_BQ_CFG_ACCESS_BIT,
							CMD_BQ_CFG_ACCESS_BIT);
	if (rc)
		pr_err("Couldn't write CMD_BQ_CFG_ACCESS_BIT rc=%d\n", rc);

	return rc;
}

static int smb1351_usb_suspend(struct smb1351_charger *chip, int reason,
					bool suspend)
{
	int rc = 0;
	int suspended;

	suspended = chip->usb_suspended_status;

	pr_debug("reason = %d requested_suspend = %d suspended_status = %d\n",
						reason, suspend, suspended);

	if (suspend == false)
		suspended &= ~reason;
	else
		suspended |= reason;

	pr_debug("new suspended_status = %d\n", suspended);

	rc = smb1351_masked_write(chip, CMD_INPUT_LIMIT_REG,
				CMD_SUSPEND_MODE_BIT,
				suspended ? CMD_SUSPEND_MODE_BIT : 0);
	if (rc)
		pr_err("Couldn't suspend rc = %d\n", rc);
	else
		chip->usb_suspended_status = suspended;

	return rc;
}

static int smb1351_battchg_disable(struct smb1351_charger *chip,
					int reason, int disable)
{
	int rc = 0;
	int disabled;

	if (chip->chg_autonomous_mode) {
		pr_debug("Charger in autonomous mode\n");
		return 0;
	}

	disabled = chip->battchg_disabled_status;

	pr_debug("reason = %d requested_disable = %d disabled_status = %d\n",
						reason, disable, disabled);
	if (disable == true)
		disabled |= reason;
	else
		disabled &= ~reason;

	pr_debug("new disabled_status = %d\n", disabled);

	rc = smb1351_masked_write(chip, CMD_CHG_REG, CMD_CHG_EN_BIT,
					disabled ? 0 : CMD_CHG_ENABLE);
	if (rc)
		pr_err("Couldn't %s charging rc=%d\n",
					disable ? "disable" : "enable", rc);
	else
		chip->battchg_disabled_status = disabled;

	return rc;
}

static int smb1351_fastchg_current_set(struct smb1351_charger *chip,
					unsigned int fastchg_current)
{
	int i, rc;
	bool is_pre_chg = false;


	if ((fastchg_current < SMB1351_CHG_PRE_MIN_MA) ||
		(fastchg_current > SMB1351_CHG_FAST_MAX_MA)) {
		pr_err("bad pre_fastchg current mA=%d asked to set\n",
					fastchg_current);
		return -EINVAL;
	}

	/*
	 * fast chg current could not support less than 1000mA
	 * use pre chg to instead for the parallel charging
	 */
	if (fastchg_current < SMB1351_CHG_FAST_MIN_MA) {
		is_pre_chg = true;
		pr_debug("is_pre_chg true, current is %d\n", fastchg_current);
	}

	if (is_pre_chg) {
		/* set prechg current */
		for (i = ARRAY_SIZE(pre_chg_current) - 1; i >= 0; i--) {
			if (pre_chg_current[i] <= fastchg_current)
				break;
		}
		if (i < 0)
			i = 0;
		chip->fastchg_current_max_ma = pre_chg_current[i];
		pr_debug("prechg setting %02x\n", i);

		i = i << SMB1351_CHG_PRE_SHIFT;

		rc = smb1351_masked_write(chip, CHG_OTH_CURRENT_CTRL_REG,
				PRECHG_CURRENT_MASK, i);
		if (rc)
			pr_err("Couldn't write CHG_OTH_CURRENT_CTRL_REG rc=%d\n",
									rc);

		return smb1351_masked_write(chip, VARIOUS_FUNC_2_REG,
				PRECHG_TO_FASTCHG_BIT, PRECHG_TO_FASTCHG_BIT);
	} else {
		if (chip->version == SMB_UNKNOWN)
			return -EINVAL;

		/* SMB1350 supports FCC upto 2600 mA */
		if (chip->version == SMB1350 && fastchg_current > 2600)
			fastchg_current = 2600;

		/* set fastchg current */
		for (i = ARRAY_SIZE(fast_chg_current) - 1; i >= 0; i--) {
			if (fast_chg_current[i] <= fastchg_current)
				break;
		}
		if (i < 0)
			i = 0;
		chip->fastchg_current_max_ma = fast_chg_current[i];

		i = i << SMB1351_CHG_FAST_SHIFT;
		pr_debug("fastchg limit=%d setting %02x\n",
					chip->fastchg_current_max_ma, i);

		/* make sure pre chg mode is disabled */
		rc = smb1351_masked_write(chip, VARIOUS_FUNC_2_REG,
					PRECHG_TO_FASTCHG_BIT, 0);
		if (rc)
			pr_err("Couldn't write VARIOUS_FUNC_2_REG rc=%d\n", rc);

		return smb1351_masked_write(chip, CHG_CURRENT_CTRL_REG,
					FAST_CHG_CURRENT_MASK, i);
	}
}

#define MIN_FLOAT_MV		3500
#define MAX_FLOAT_MV		4500
#define VFLOAT_STEP_MV		20

static int smb1351_float_voltage_set(struct smb1351_charger *chip,
								int vfloat_mv)
{
	u8 temp;

	if ((vfloat_mv < MIN_FLOAT_MV) || (vfloat_mv > MAX_FLOAT_MV)) {
		pr_err("bad float voltage mv =%d asked to set\n", vfloat_mv);
		return -EINVAL;
	}

	temp = (vfloat_mv - MIN_FLOAT_MV) / VFLOAT_STEP_MV;

	return smb1351_masked_write(chip, VFLOAT_REG, VFLOAT_MASK, temp);
}

static int smb1351_iterm_set(struct smb1351_charger *chip, int iterm_ma)
{
	int rc;
	u8 reg;

	if (iterm_ma <= 200)
		reg = CHG_ITERM_200MA;
	else if (iterm_ma <= 300)
		reg = CHG_ITERM_300MA;
	else if (iterm_ma <= 400)
		reg = CHG_ITERM_400MA;
	else if (iterm_ma <= 500)
		reg = CHG_ITERM_500MA;
	else if (iterm_ma <= 600)
		reg = CHG_ITERM_600MA;
	else
		reg = CHG_ITERM_700MA;

	rc = smb1351_masked_write(chip, CHG_OTH_CURRENT_CTRL_REG,
				ITERM_MASK, reg);
	if (rc) {
		pr_err("Couldn't set iterm rc = %d\n", rc);
		return rc;
	}
	/* enable the iterm */
	rc = smb1351_masked_write(chip, CHG_CTRL_REG,
				ITERM_EN_BIT, ITERM_ENABLE);
	if (rc) {
		pr_err("Couldn't enable iterm rc = %d\n", rc);
		return rc;
	}
	return 0;
}

static int smb1351_chg_otg_regulator_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smb1351_charger *chip = rdev_get_drvdata(rdev);

	rc = smb1351_masked_write(chip, CMD_CHG_REG, CMD_OTG_EN_BIT,
							CMD_OTG_EN_BIT);
	if (rc)
		pr_err("Couldn't enable  OTG mode rc=%d\n", rc);
	return rc;
}

static int smb1351_chg_otg_regulator_disable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smb1351_charger *chip = rdev_get_drvdata(rdev);

	rc = smb1351_masked_write(chip, CMD_CHG_REG, CMD_OTG_EN_BIT, 0);
	if (rc)
		pr_err("Couldn't disable OTG mode rc=%d\n", rc);
	return rc;
}

static int smb1351_chg_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	u8 reg = 0;
	struct smb1351_charger *chip = rdev_get_drvdata(rdev);

	rc = smb1351_read_reg(chip, CMD_CHG_REG, &reg);
	if (rc) {
		pr_err("Couldn't read OTG enable bit rc=%d\n", rc);
		return rc;
	}

	return (reg & CMD_OTG_EN_BIT) ? 1 : 0;
}

struct regulator_ops smb1351_chg_otg_reg_ops = {
	.enable		= smb1351_chg_otg_regulator_enable,
	.disable	= smb1351_chg_otg_regulator_disable,
	.is_enabled	= smb1351_chg_otg_regulator_is_enable,
};

static int smb1351_regulator_init(struct smb1351_charger *chip)
{
	int rc = 0;
	struct regulator_config cfg = {};

	chip->otg_vreg.rdesc.owner = THIS_MODULE;
	chip->otg_vreg.rdesc.type = REGULATOR_VOLTAGE;
	chip->otg_vreg.rdesc.ops = &smb1351_chg_otg_reg_ops;
	chip->otg_vreg.rdesc.name = 
		chip->dev->of_node->name;
	chip->otg_vreg.rdesc.of_match = 
		chip->dev->of_node->name;

	cfg.dev = chip->dev;
	cfg.driver_data = chip;

	chip->otg_vreg.rdev = regulator_register(
					&chip->otg_vreg.rdesc, &cfg);
	if (IS_ERR(chip->otg_vreg.rdev)) {
		rc = PTR_ERR(chip->otg_vreg.rdev);
		chip->otg_vreg.rdev = NULL;
		if (rc != -EPROBE_DEFER)
			pr_err("OTG reg failed, rc=%d\n", rc);
	}
	return rc;
}

static int smb_chip_get_version(struct smb1351_charger *chip)
{
	u8 ver;
	int rc = 0;

	if (chip->version == SMB_UNKNOWN) {
		rc = smb1351_read_reg(chip, VERSION_REG, &ver);
		if (rc) {
			pr_err("Couldn't read version rc=%d\n", rc);
			return rc;
		}

		/* If bit 1 is set, it is SMB1350 */
		if (ver & VERSION_MASK)
			chip->version = SMB1350;
		else
			chip->version = SMB1351;
	}

	return rc;
}

static int smb1351_hw_init(struct smb1351_charger *chip)
{
	int rc;
	u8 reg = 0, mask = 0;

	/* configure smb_pinctrl to enable irqs */
	if (chip->pinctrl_state_name) {
		chip->smb_pinctrl = pinctrl_get_select(chip->dev,
						chip->pinctrl_state_name);
		if (IS_ERR(chip->smb_pinctrl)) {
			pr_err("Could not get/set %s pinctrl state rc = %ld\n",
						chip->pinctrl_state_name,
						PTR_ERR(chip->smb_pinctrl));
			return PTR_ERR(chip->smb_pinctrl);
		}
	}

	/*
	 * If the charger is pre-configured for autonomous operation,
	 * do not apply additional settings
	 */
	if (chip->chg_autonomous_mode) {
		pr_debug("Charger configured for autonomous mode\n");
		return 0;
	}

	rc = smb_chip_get_version(chip);
	if (rc) {
		pr_err("Couldn't get version rc = %d\n", rc);
		return rc;
	}

	rc = smb1351_enable_volatile_writes(chip);
	if (rc) {
		pr_err("Couldn't configure volatile writes rc=%d\n", rc);
		return rc;
	}

	/* setup battery missing source */
	reg = BATT_MISSING_THERM_PIN_SOURCE_BIT;
	mask = BATT_MISSING_THERM_PIN_SOURCE_BIT;
	rc = smb1351_masked_write(chip, HVDCP_BATT_MISSING_CTRL_REG,
								mask, reg);
	if (rc) {
		pr_err("Couldn't set HVDCP_BATT_MISSING_CTRL_REG rc=%d\n", rc);
		return rc;
	}
	/* setup defaults for CHG_PIN_EN_CTRL_REG */
	reg = EN_BY_I2C_0_DISABLE | USBCS_CTRL_BY_I2C | CHG_ERR_BIT |
		APSD_DONE_BIT | LED_BLINK_FUNC_BIT;
	mask = EN_PIN_CTRL_MASK | USBCS_CTRL_BIT | CHG_ERR_BIT |
		APSD_DONE_BIT | LED_BLINK_FUNC_BIT;
	rc = smb1351_masked_write(chip, CHG_PIN_EN_CTRL_REG, mask, reg);
	if (rc) {
		pr_err("Couldn't set CHG_PIN_EN_CTRL_REG rc=%d\n", rc);
		return rc;
	}
	/* setup USB 2.0/3.0 detection and USB 500/100 command polarity */
	reg = USB_2_3_MODE_SEL_BY_I2C | USB_CMD_POLARITY_500_1_100_0;
	mask = USB_2_3_MODE_SEL_BIT | USB_5_1_CMD_POLARITY_BIT;
	rc = smb1351_masked_write(chip, CHG_OTH_CURRENT_CTRL_REG, mask, reg);
	if (rc) {
		pr_err("Couldn't set CHG_OTH_CURRENT_CTRL_REG rc=%d\n", rc);
		return rc;
	}
	/* setup USB suspend, AICL and APSD  */
	reg = SUSPEND_MODE_CTRL_BY_I2C | AICL_EN_BIT;
	if (!chip->disable_apsd)
		reg |= APSD_EN_BIT;
	mask = SUSPEND_MODE_CTRL_BIT | AICL_EN_BIT | APSD_EN_BIT;
	rc = smb1351_masked_write(chip, VARIOUS_FUNC_REG, mask, reg);
	if (rc) {
		pr_err("Couldn't set VARIOUS_FUNC_REG rc=%d\n",	rc);
		return rc;
	}
	/* Fault and Status IRQ configuration */
	reg = HOT_COLD_HARD_LIMIT_BIT | HOT_COLD_SOFT_LIMIT_BIT
		| INPUT_OVLO_BIT | INPUT_UVLO_BIT | AICL_DONE_FAIL_BIT;
	rc = smb1351_write_reg(chip, FAULT_INT_REG, reg);
	if (rc) {
		pr_err("Couldn't set FAULT_INT_REG rc=%d\n", rc);
		return rc;
	}
	reg = CHG_OR_PRECHG_TIMEOUT_BIT | BATT_OVP_BIT |
		FAST_TERM_TAPER_RECHG_INHIBIT_BIT |
		BATT_MISSING_BIT | BATT_LOW_BIT;
	rc = smb1351_write_reg(chip, STATUS_INT_REG, reg);
	if (rc) {
		pr_err("Couldn't set STATUS_INT_REG rc=%d\n", rc);
		return rc;
	}
	/* setup THERM Monitor */
	if (!chip->using_pmic_therm) {
		rc = smb1351_masked_write(chip, THERM_A_CTRL_REG,
			THERM_MONITOR_BIT, THERM_MONITOR_EN);
		if (rc) {
			pr_err("Couldn't set THERM_A_CTRL_REG rc=%d\n",	rc);
			return rc;
		}
	}
	/* set the fast charge current limit */
	rc = smb1351_fastchg_current_set(chip,
			chip->target_fastchg_current_max_ma);
	if (rc) {
		pr_err("Couldn't set fastchg current rc=%d\n", rc);
		return rc;
	}

	/* set the float voltage */
	if (chip->vfloat_mv != -EINVAL) {
		rc = smb1351_float_voltage_set(chip, chip->vfloat_mv);
		if (rc) {
			pr_err("Couldn't set float voltage rc = %d\n", rc);
			return rc;
		}
	}

	/* set iterm */
	if (chip->iterm_ma != -EINVAL) {
		if (chip->iterm_disabled) {
			pr_err("Error: Both iterm_disabled and iterm_ma set\n");
			return -EINVAL;
		} else {
			rc = smb1351_iterm_set(chip, chip->iterm_ma);
			if (rc) {
				pr_err("Couldn't set iterm rc = %d\n", rc);
				return rc;
			}
		}
	} else  if (chip->iterm_disabled) {
		rc = smb1351_masked_write(chip, CHG_CTRL_REG,
					ITERM_EN_BIT, ITERM_DISABLE);
		if (rc) {
			pr_err("Couldn't set iterm rc = %d\n", rc);
			return rc;
		}
	}

	/* set recharge-threshold */
	if (chip->recharge_mv != -EINVAL) {
		if (chip->recharge_disabled) {
			pr_err("Error: Both recharge_disabled and recharge_mv set\n");
			return -EINVAL;
		} else {
			reg = AUTO_RECHG_ENABLE;
			if (chip->recharge_mv > 50)
				reg |= AUTO_RECHG_TH_100MV;
			else
				reg |= AUTO_RECHG_TH_50MV;

			rc = smb1351_masked_write(chip, CHG_CTRL_REG,
					AUTO_RECHG_BIT |
					AUTO_RECHG_TH_BIT, reg);
			if (rc) {
				pr_err("Couldn't set rechg-cfg rc = %d\n", rc);
				return rc;
			}
		}
	} else if (chip->recharge_disabled) {
		rc = smb1351_masked_write(chip, CHG_CTRL_REG,
				AUTO_RECHG_BIT,
				AUTO_RECHG_DISABLE);
		if (rc) {
			pr_err("Couldn't disable auto-rechg rc = %d\n", rc);
			return rc;
		}
	}

	/* enable/disable charging by suspending usb */
	rc = smb1351_usb_suspend(chip, USER, chip->usb_suspended_status);
	if (rc) {
		pr_err("Unable to %s battery charging. rc=%d\n",
			chip->usb_suspended_status ? "disable" : "enable",
									rc);
	}

	return rc;
}

static enum power_supply_property smb1351_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_MODEL_NAME,
};

static int smb1351_get_prop_batt_status(struct smb1351_charger *chip)
{
	int rc;
	u8 reg = 0;

	if (chip->batt_full)
		return POWER_SUPPLY_STATUS_FULL;

	rc = smb1351_read_reg(chip, STATUS_4_REG, &reg);
	if (rc) {
		pr_err("Couldn't read STATUS_4 rc = %d\n", rc);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

	pr_debug("STATUS_4_REG(0x3A)=%x\n", reg);

	if (reg & STATUS_HOLD_OFF_BIT)
		return POWER_SUPPLY_STATUS_NOT_CHARGING;

	if (reg & STATUS_CHG_MASK)
		return POWER_SUPPLY_STATUS_CHARGING;

	return POWER_SUPPLY_STATUS_DISCHARGING;
}

static int smb1351_get_prop_batt_present(struct smb1351_charger *chip)
{
	return !chip->battery_missing;
}

static int smb1351_get_prop_batt_capacity(struct smb1351_charger *chip)
{
	union power_supply_propval ret = {0, };

	if (chip->fake_battery_soc >= 0)
		return chip->fake_battery_soc;

	if (chip->bms_psy) {
		power_supply_get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		return ret.intval;
	}
	pr_debug("return DEFAULT_BATT_CAPACITY\n");
	return DEFAULT_BATT_CAPACITY;
}

static int smb1351_get_prop_batt_temp(struct smb1351_charger *chip)
{
	union power_supply_propval ret = {0, };
	int rc = 0;
	struct qpnp_vadc_result results;

	if (chip->bms_psy) {
		power_supply_get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_TEMP, &ret);
		return ret.intval;
	}
	if (chip->vadc_dev) {
		rc = qpnp_vadc_read(chip->vadc_dev,
				LR_MUX1_BATT_THERM, &results);
		if (rc)
			pr_debug("Unable to read adc batt temp rc=%d\n", rc);
		else
			return (int)results.physical;
	}

	pr_debug("return default temperature\n");
	return DEFAULT_BATT_TEMP;
}

static int smb1351_get_prop_charge_type(struct smb1351_charger *chip)
{
	int rc;
	u8 reg = 0;

	rc = smb1351_read_reg(chip, STATUS_4_REG, &reg);
	if (rc) {
		pr_err("Couldn't read STATUS_4 rc = %d\n", rc);
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	pr_debug("STATUS_4_REG(0x3A)=%x\n", reg);

	reg &= STATUS_CHG_MASK;

	if (reg == STATUS_FAST_CHARGING)
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (reg == STATUS_TAPER_CHARGING)
		return POWER_SUPPLY_CHARGE_TYPE_TAPER;
	else if (reg == STATUS_PRE_CHARGING)
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	else
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
}

static int smb1351_get_prop_batt_health(struct smb1351_charger *chip)
{
	union power_supply_propval ret = {0, };

	if (chip->batt_hot)
		ret.intval = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (chip->batt_cold)
		ret.intval = POWER_SUPPLY_HEALTH_COLD;
	else if (chip->batt_warm)
		ret.intval = POWER_SUPPLY_HEALTH_WARM;
	else if (chip->batt_cool)
		ret.intval = POWER_SUPPLY_HEALTH_COOL;
	else
		ret.intval = POWER_SUPPLY_HEALTH_GOOD;

	return ret.intval;
}

static int smb1351_set_usb_chg_current(struct smb1351_charger *chip,
							int current_ma)
{
	int i, rc = 0;
	u8 reg = 0, mask = 0;

	pr_debug("USB current_ma = %d\n", current_ma);

	if (chip->chg_autonomous_mode) {
		pr_debug("Charger in autonomous mode\n");
		return 0;
	}

	/* set suspend bit when urrent_ma <= 2 */
	if (current_ma <= SUSPEND_CURRENT_MA) {
		smb1351_usb_suspend(chip, CURRENT, true);
		pr_debug("USB suspend\n");
		return 0;
	}

	if (current_ma > SUSPEND_CURRENT_MA &&
			current_ma < USB2_MIN_CURRENT_MA)
		current_ma = USB2_MIN_CURRENT_MA;

	if (current_ma == USB2_MIN_CURRENT_MA) {
		/* USB 2.0 - 100mA */
		reg = CMD_USB_2_MODE | CMD_USB_100_MODE;
	} else if (current_ma == USB3_MIN_CURRENT_MA) {
		/* USB 3.0 - 150mA */
		reg = CMD_USB_3_MODE | CMD_USB_100_MODE;
	} else if (current_ma == USB2_MAX_CURRENT_MA) {
		/* USB 2.0 - 500mA */
		reg = CMD_USB_2_MODE | CMD_USB_500_MODE;
	} else if (current_ma == USB3_MAX_CURRENT_MA) {
		/* USB 3.0 - 900mA */
		reg = CMD_USB_3_MODE | CMD_USB_500_MODE;
	} else if (current_ma > USB2_MAX_CURRENT_MA) {
		/* HC mode  - if none of the above */
		reg = CMD_USB_AC_MODE;

		for (i = ARRAY_SIZE(usb_chg_current) - 1; i >= 0; i--) {
			if (usb_chg_current[i] <= current_ma)
				break;
		}
		if (i < 0)
			i = 0;
		rc = smb1351_masked_write(chip, CHG_CURRENT_CTRL_REG,
						AC_INPUT_CURRENT_LIMIT_MASK, i);
		if (rc) {
			pr_err("Couldn't set input mA rc=%d\n", rc);
			return rc;
		}
	}
	/* control input current mode by command */
	reg |= CMD_INPUT_CURRENT_MODE_CMD;
	mask = CMD_INPUT_CURRENT_MODE_BIT | CMD_USB_2_3_SEL_BIT |
		CMD_USB_1_5_AC_CTRL_MASK;
	rc = smb1351_masked_write(chip, CMD_INPUT_LIMIT_REG, mask, reg);
	if (rc) {
		pr_err("Couldn't set charging mode rc = %d\n", rc);
		return rc;
	}

	/* unset the suspend bit here */
	smb1351_usb_suspend(chip, CURRENT, false);

	return rc;
}

static int smb1351_batt_property_is_writeable(struct power_supply *psy,
					enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CAPACITY:
		return 1;
	default:
		break;
	}
	return 0;
}

static int smb1351_battery_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	int rc;
	struct smb1351_charger *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		if (!chip->bms_controlled_charging)
			return -EINVAL;
		switch (val->intval) {
		case POWER_SUPPLY_STATUS_FULL:
			rc = smb1351_battchg_disable(chip, SOC, true);
			if (rc) {
				pr_err("Couldn't disable charging  rc = %d\n",
									rc);
			} else {
				chip->batt_full = true;
				pr_debug("status = FULL, batt_full = %d\n",
							chip->batt_full);
			}
			break;
		case POWER_SUPPLY_STATUS_DISCHARGING:
			chip->batt_full = false;
			power_supply_changed(chip->batt_psy);
			pr_debug("status = DISCHARGING, batt_full = %d\n",
							chip->batt_full);
			break;
		case POWER_SUPPLY_STATUS_CHARGING:
			rc = smb1351_battchg_disable(chip, SOC, false);
			if (rc) {
				pr_err("Couldn't enable charging rc = %d\n",
									rc);
			} else {
				chip->batt_full = false;
				pr_debug("status = CHARGING, batt_full = %d\n",
							chip->batt_full);
			}
			break;
		default:
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		smb1351_usb_suspend(chip, USER, !val->intval);
		break;
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		smb1351_battchg_disable(chip, USER, !val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		chip->fake_battery_soc = val->intval;
		power_supply_changed(chip->batt_psy);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smb1351_battery_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smb1351_charger *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = smb1351_get_prop_batt_status(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb1351_get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = smb1351_get_prop_batt_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = !chip->usb_suspended_status;
		break;
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		val->intval = !chip->battchg_disabled_status;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = smb1351_get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = smb1351_get_prop_batt_health(chip);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = smb1351_get_prop_batt_temp(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "smb1351";
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property smb1351_parallel_properties[] = {
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_PARALLEL_MODE,
};

static int smb1351_parallel_set_chg_present(struct smb1351_charger *chip,
						int present)
{
	int rc;
	u8 reg, mask = 0;

	if (present == chip->parallel_charger_present) {
		pr_debug("present %d -> %d, skipping\n",
				chip->parallel_charger_present, present);
		return 0;
	}

	if (present) {
		/* Check if SMB1351 is present */
		rc = smb1351_read_reg(chip, CHG_REVISION_REG, &reg);
		if (rc) {
			pr_debug("Failed to detect smb1351-parallel-charger, may be absent\n");
			return -ENODEV;
		}

		rc = smb_chip_get_version(chip);
		if (rc) {
			pr_err("Couldn't get version rc = %d\n", rc);
			return rc;
		}

		rc = smb1351_enable_volatile_writes(chip);
		if (rc) {
			pr_err("Couldn't configure for volatile rc = %d\n", rc);
			return rc;
		}

		/* set the float voltage */
		if (chip->vfloat_mv != -EINVAL) {
			rc = smb1351_float_voltage_set(chip, chip->vfloat_mv);
			if (rc) {
				pr_err("Couldn't set float voltage rc = %d\n",
									rc);
				return rc;
			}
		}

		/* set recharge-threshold and enable auto recharge */
		if (chip->recharge_mv != -EINVAL) {
			reg = AUTO_RECHG_ENABLE;
			if (chip->recharge_mv > 50)
				reg |= AUTO_RECHG_TH_100MV;
			else
				reg |= AUTO_RECHG_TH_50MV;

			rc = smb1351_masked_write(chip, CHG_CTRL_REG,
					AUTO_RECHG_BIT |
					AUTO_RECHG_TH_BIT, reg);
			if (rc) {
				pr_err("Couldn't set rechg-cfg rc = %d\n", rc);
				return rc;
			}
		}

		/* set chg en by pin active low  */
		reg = chip->parallel_pin_polarity_setting | USBCS_CTRL_BY_I2C;
		rc = smb1351_masked_write(chip, CHG_PIN_EN_CTRL_REG,
					EN_PIN_CTRL_MASK | USBCS_CTRL_BIT, reg);
		if (rc) {
			pr_err("Couldn't set en pin rc=%d\n", rc);
			return rc;
		}

		/* control USB suspend via command bits */
		rc = smb1351_masked_write(chip, VARIOUS_FUNC_REG,
						SUSPEND_MODE_CTRL_BIT,
						SUSPEND_MODE_CTRL_BY_I2C);
		if (rc) {
			pr_err("Couldn't set USB suspend rc=%d\n", rc);
			return rc;
		}

		/*
		 * setup USB 2.0/3.0 detection and USB 500/100
		 * command polarity
		 */
		reg = USB_2_3_MODE_SEL_BY_I2C | USB_CMD_POLARITY_500_1_100_0;
		mask = USB_2_3_MODE_SEL_BIT | USB_5_1_CMD_POLARITY_BIT;
		rc = smb1351_masked_write(chip,
				CHG_OTH_CURRENT_CTRL_REG, mask, reg);
		if (rc) {
			pr_err("Couldn't set CHG_OTH_CURRENT_CTRL_REG rc=%d\n",
					rc);
			return rc;
		}

		/* set fast charging current limit */
		chip->target_fastchg_current_max_ma = SMB1351_CHG_FAST_MIN_MA;
		rc = smb1351_fastchg_current_set(chip,
					chip->target_fastchg_current_max_ma);
		if (rc) {
			pr_err("Couldn't set fastchg current rc=%d\n", rc);
			return rc;
		}
	}

	chip->parallel_charger_present = present;
	/*
	 * When present is being set force USB suspend, start charging
	 * only when POWER_SUPPLY_PROP_CURRENT_MAX is set.
	 */
	chip->usb_psy_ma = SUSPEND_CURRENT_MA;
	smb1351_usb_suspend(chip, CURRENT, true);

	return 0;
}

static int smb1351_get_closest_usb_setpoint(int val)
{
	int i;

	for (i = ARRAY_SIZE(usb_chg_current) - 1; i >= 0; i--) {
		if (usb_chg_current[i] <= val)
			break;
	}
	if (i < 0)
		i = 0;

	if (i >= ARRAY_SIZE(usb_chg_current) - 1)
		return ARRAY_SIZE(usb_chg_current) - 1;

	/* check what is closer, i or i + 1 */
	if (abs(usb_chg_current[i] - val) < abs(usb_chg_current[i + 1] - val))
		return i;
	else
		return i + 1;
}

static bool smb1351_is_input_current_limited(struct smb1351_charger *chip)
{
	int rc;
	u8 reg;

	rc = smb1351_read_reg(chip, IRQ_H_REG, &reg);
	if (rc) {
		pr_err("Failed to read IRQ_H_REG for ICL status: %d\n", rc);
		return false;
	}

	return !!(reg & IRQ_IC_LIMIT_STATUS_BIT);
}

static int smb1351_parallel_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	int rc = 0, index;
	struct smb1351_charger *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		/*
		 *CHG EN is controlled by pin in the parallel charging.
		 *Use suspend if disable charging by command.
		 */
		if (chip->parallel_charger_present)
			rc = smb1351_usb_suspend(chip, USER, !val->intval);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = smb1351_parallel_set_chg_present(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		if (chip->parallel_charger_present) {
			chip->target_fastchg_current_max_ma =
						val->intval / 1000;
			rc = smb1351_fastchg_current_set(chip,
					chip->target_fastchg_current_max_ma);
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (chip->parallel_charger_present) {
			index = smb1351_get_closest_usb_setpoint(
						val->intval / 1000);
			chip->usb_psy_ma = usb_chg_current[index];
			rc = smb1351_set_usb_chg_current(chip,
						chip->usb_psy_ma);
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (chip->parallel_charger_present &&
			(chip->vfloat_mv != val->intval)) {
			rc = smb1351_float_voltage_set(chip, val->intval);
			if (!rc)
				chip->vfloat_mv = val->intval;
		} else {
			chip->vfloat_mv = val->intval;
		}
		break;
	default:
		return -EINVAL;
	}
	return rc;
}

static int smb1351_parallel_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		return 1;
	default:
		return 0;
	}
}

static int smb1351_parallel_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smb1351_charger *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = !chip->usb_suspended_status;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (chip->parallel_charger_present)
			val->intval = chip->usb_psy_ma * 1000;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = chip->vfloat_mv;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->parallel_charger_present;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		if (chip->parallel_charger_present)
			val->intval = chip->fastchg_current_max_ma * 1000;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (chip->parallel_charger_present)
			val->intval = smb1351_get_prop_batt_status(chip);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
		if (chip->parallel_charger_present)
			val->intval =
				smb1351_is_input_current_limited(chip) ? 1 : 0;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_PARALLEL_MODE:
		if (chip->parallel_charger_present)
			val->intval = POWER_SUPPLY_PARALLEL_USBIN_USBIN;
		else
			val->intval = POWER_SUPPLY_PARALLEL_NONE;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void smb1351_chg_set_appropriate_battery_current(
				struct smb1351_charger *chip)
{
	int rc;
	unsigned int current_max = chip->target_fastchg_current_max_ma;

	if (chip->batt_cool)
		current_max = min(current_max, chip->batt_cool_ma);
	if (chip->batt_warm)
		current_max = min(current_max, chip->batt_warm_ma);

	pr_debug("setting %dmA", current_max);

	rc = smb1351_fastchg_current_set(chip, current_max);
	if (rc)
		pr_err("Couldn't set charging current rc = %d\n", rc);
}

static void smb1351_chg_set_appropriate_vddmax(struct smb1351_charger *chip)
{
	int rc;
	unsigned int vddmax = chip->vfloat_mv;

	if (chip->batt_cool)
		vddmax = min(vddmax, chip->batt_cool_mv);
	if (chip->batt_warm)
		vddmax = min(vddmax, chip->batt_warm_mv);

	pr_debug("setting %dmV\n", vddmax);

	rc = smb1351_float_voltage_set(chip, vddmax);
	if (rc)
		pr_err("Couldn't set float voltage rc = %d\n", rc);
}

static void smb1351_chg_ctrl_in_jeita(struct smb1351_charger *chip)
{
	union power_supply_propval ret = {0, };
	int rc;

	/* enable the iterm to prevent the reverse boost */
	if (chip->iterm_disabled) {
		if (chip->batt_cool || chip->batt_warm) {
			rc = smb1351_iterm_set(chip, 100);
			pr_debug("set the iterm due to JEITA\n");
		} else {
			rc = smb1351_masked_write(chip, CHG_CTRL_REG,
						ITERM_EN_BIT, ITERM_DISABLE);
			pr_debug("disable the iterm when exits warm/cool\n");
		}
		if (rc) {
			pr_err("Couldn't set iterm rc = %d\n", rc);
			return;
		}
	}
	/*
	* When JEITA back to normal, the charging maybe disabled due to
	* the current termination. So re-enable the charging if the soc
	* is less than 100 in the normal mode. A 200ms delay is requred
	* before the disabe and enable operation.
	*/
	if (chip->bms_psy) {
		rc = power_supply_get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		if (rc) {
			pr_err("Couldn't read the bms capacity rc = %d\n",
									rc);
			return;
		}
		if (!chip->batt_cool && !chip->batt_warm
				&& !chip->batt_cold && !chip->batt_hot
				&& ret.intval < 100) {
			rc = smb1351_battchg_disable(chip, THERMAL, true);
			if (rc) {
				pr_err("Couldn't disable charging rc = %d\n",
									rc);
				return;
			}
			/* delay for resetting the charging */
			msleep(200);
			rc = smb1351_battchg_disable(chip, THERMAL, false);
			if (rc) {
				pr_err("Couldn't enable charging rc = %d\n",
									rc);
				return;
			} else {
				chip->batt_full = false;
				pr_debug("re-enable charging, batt_full = %d\n",
							chip->batt_full);
			}
			pr_debug("batt psy changed\n");
			power_supply_changed(chip->batt_psy);
		}
	}
}

#define HYSTERESIS_DECIDEGC 20
static void smb1351_chg_adc_notification(enum qpnp_tm_state state, void *ctx)
{
	struct smb1351_charger *chip = ctx;
	struct battery_status *cur = NULL;
	int temp;

	if (state >= ADC_TM_STATE_NUM) {
		pr_err("invalid state parameter %d\n", state);
		return;
	}

	temp = smb1351_get_prop_batt_temp(chip);

	pr_debug("temp = %d state = %s\n", temp,
				state == ADC_TM_WARM_STATE ? "hot" : "cold");

	/* reset the adc status request */
	chip->adc_param.state_request = ADC_TM_WARM_COOL_THR_ENABLE;

	/* temp from low to high */
	if (state == ADC_TM_WARM_STATE) {
		/* WARM -> HOT */
		if (temp >= chip->batt_hot_decidegc) {
			cur = &batt_s[BATT_HOT];
			chip->adc_param.low_temp =
				chip->batt_hot_decidegc - HYSTERESIS_DECIDEGC;
			chip->adc_param.state_request =	ADC_TM_COOL_THR_ENABLE;
		/* NORMAL -> WARM */
		} else if (temp >= chip->batt_warm_decidegc &&
					chip->jeita_supported) {
			cur = &batt_s[BATT_WARM];
			chip->adc_param.low_temp =
				chip->batt_warm_decidegc - HYSTERESIS_DECIDEGC;
			chip->adc_param.high_temp = chip->batt_hot_decidegc;
		/* COOL -> NORMAL */
		} else if (temp >= chip->batt_cool_decidegc &&
					chip->jeita_supported) {
			cur = &batt_s[BATT_NORMAL];
			chip->adc_param.low_temp =
				chip->batt_cool_decidegc - HYSTERESIS_DECIDEGC;
			chip->adc_param.high_temp = chip->batt_warm_decidegc;
		/* COLD -> COOL */
		} else if (temp >= chip->batt_cold_decidegc) {
			cur = &batt_s[BATT_COOL];
			chip->adc_param.low_temp =
				chip->batt_cold_decidegc - HYSTERESIS_DECIDEGC;
			if (chip->jeita_supported)
				chip->adc_param.high_temp =
						chip->batt_cool_decidegc;
			else
				chip->adc_param.high_temp =
						chip->batt_hot_decidegc;
		/* MISSING -> COLD */
		} else if (temp >= chip->batt_missing_decidegc) {
			cur = &batt_s[BATT_COLD];
			chip->adc_param.high_temp = chip->batt_cold_decidegc;
			chip->adc_param.low_temp = chip->batt_missing_decidegc
							- HYSTERESIS_DECIDEGC;
		}
	/* temp from high to low */
	} else {
		/* COLD -> MISSING */
		if (temp <= chip->batt_missing_decidegc) {
			cur = &batt_s[BATT_MISSING];
			chip->adc_param.high_temp = chip->batt_missing_decidegc
							+ HYSTERESIS_DECIDEGC;
			chip->adc_param.state_request = ADC_TM_WARM_THR_ENABLE;
		/* COOL -> COLD */
		} else if (temp <= chip->batt_cold_decidegc) {
			cur = &batt_s[BATT_COLD];
			chip->adc_param.high_temp =
				chip->batt_cold_decidegc + HYSTERESIS_DECIDEGC;
			/* add low_temp to enable batt present check */
			chip->adc_param.low_temp = chip->batt_missing_decidegc;
		/* NORMAL -> COOL */
		} else if (temp <= chip->batt_cool_decidegc &&
					chip->jeita_supported) {
			cur = &batt_s[BATT_COOL];
			chip->adc_param.high_temp =
				chip->batt_cool_decidegc + HYSTERESIS_DECIDEGC;
			chip->adc_param.low_temp = chip->batt_cold_decidegc;
		/* WARM -> NORMAL */
		} else if (temp <= chip->batt_warm_decidegc &&
					chip->jeita_supported) {
			cur = &batt_s[BATT_NORMAL];
			chip->adc_param.high_temp =
				chip->batt_warm_decidegc + HYSTERESIS_DECIDEGC;
			chip->adc_param.low_temp = chip->batt_cool_decidegc;
		/* HOT -> WARM */
		} else if (temp <= chip->batt_hot_decidegc) {
			cur = &batt_s[BATT_WARM];
			if (chip->jeita_supported)
				chip->adc_param.low_temp =
					chip->batt_warm_decidegc;
			else
				chip->adc_param.low_temp =
					chip->batt_cold_decidegc;
			chip->adc_param.high_temp =
				chip->batt_hot_decidegc + HYSTERESIS_DECIDEGC;
		}
	}

	if (!cur) {
		pr_debug("Couldn't choose batt state, adc state=%d and temp=%d\n",
			state, temp);
		return;
	}

	if (cur->batt_present)
		chip->battery_missing = false;
	else
		chip->battery_missing = true;

	if (cur->batt_hot ^ chip->batt_hot ||
			cur->batt_cold ^ chip->batt_cold) {
		chip->batt_hot = cur->batt_hot;
		chip->batt_cold = cur->batt_cold;
		/* stop charging explicitly since we use PMIC thermal pin*/
		if (cur->batt_hot || cur->batt_cold ||
							chip->battery_missing)
			smb1351_battchg_disable(chip, THERMAL, 1);
		else
			smb1351_battchg_disable(chip, THERMAL, 0);
	}

	if ((chip->batt_warm ^ cur->batt_warm ||
				chip->batt_cool ^ cur->batt_cool)
						&& chip->jeita_supported) {
		chip->batt_warm = cur->batt_warm;
		chip->batt_cool = cur->batt_cool;
		smb1351_chg_set_appropriate_battery_current(chip);
		smb1351_chg_set_appropriate_vddmax(chip);
		smb1351_chg_ctrl_in_jeita(chip);
	}

	pr_debug("hot %d, cold %d, warm %d, cool %d, soft jeita supported %d, missing %d, low = %d deciDegC, high = %d deciDegC\n",
		chip->batt_hot, chip->batt_cold, chip->batt_warm,
		chip->batt_cool, chip->jeita_supported,
		chip->battery_missing, chip->adc_param.low_temp,
		chip->adc_param.high_temp);
	if (qpnp_adc_tm_channel_measure(chip->adc_tm_dev, &chip->adc_param))
		pr_err("request ADC error\n");
}

static int rerun_apsd(struct smb1351_charger *chip)
{
	int rc;

	pr_debug("Reruning APSD\nDisabling APSD\n");

	rc = smb1351_masked_write(chip, CMD_HVDCP_REG, CMD_APSD_RE_RUN_BIT,
						CMD_APSD_RE_RUN_BIT);
	if (rc)
		pr_err("Couldn't re-run APSD algo\n");

	return 0;
}

static void smb1351_hvdcp_det_work(struct work_struct *work)
{
	int rc;
	u8 reg;
	union power_supply_propval pval = {0, };
	struct smb1351_charger *chip = container_of(work,
						struct smb1351_charger,
						hvdcp_det_work.work);

	rc = smb1351_read_reg(chip, STATUS_7_REG, &reg);
	if (rc) {
		pr_err("Couldn't read STATUS_7_REG rc == %d\n", rc);
		goto end;
	}
	pr_debug("STATUS_7_REG = 0x%02X\n", reg);

	if (reg) {
		pr_debug("HVDCP detected; notifying USB PSY\n");
		pval.intval = POWER_SUPPLY_TYPE_USB_HVDCP;
		power_supply_set_property(chip->usb_psy,
			POWER_SUPPLY_PROP_TYPE, &pval);
	}
end:
	pm_relax(chip->dev);
}

#define HVDCP_NOTIFY_MS 2500
static int smb1351_apsd_complete_handler(struct smb1351_charger *chip,
						u8 status)
{
	int rc;
	u8 reg = 0;
	union power_supply_propval prop = {0, };
	enum power_supply_type type = POWER_SUPPLY_TYPE_UNKNOWN;

	/*
	 * If apsd is disabled, charger detection is done by
	 * USB phy driver.
	 */
	if (chip->disable_apsd || chip->usbin_ov) {
		pr_debug("APSD %s, status = %d\n",
			chip->disable_apsd ? "disabled" : "enabled", !!status);
		pr_debug("USBIN ov, status = %d\n", chip->usbin_ov);
		return 0;
	}

	rc = smb1351_read_reg(chip, STATUS_5_REG, &reg);
	if (rc) {
		pr_err("Couldn't read STATUS_5 rc = %d\n", rc);
		return rc;
	}

	pr_debug("STATUS_5_REG(0x3B)=%x\n", reg);

	switch (reg) {
	case STATUS_PORT_ACA_DOCK:
	case STATUS_PORT_ACA_C:
	case STATUS_PORT_ACA_B:
	case STATUS_PORT_ACA_A:
		type = POWER_SUPPLY_TYPE_USB_ACA;
		break;
	case STATUS_PORT_CDP:
		type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case STATUS_PORT_DCP:
		type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case STATUS_PORT_SDP:
		type = POWER_SUPPLY_TYPE_USB;
		break;
	case STATUS_PORT_OTHER:
		type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	default:
		type = POWER_SUPPLY_TYPE_USB;
		break;
	}

	if (status) {
		chip->chg_present = true;
		pr_debug("APSD complete. USB type detected=%d chg_present=%d\n",
						type, chip->chg_present);
		if (!chip->battery_missing && !chip->apsd_rerun) {
			if (type == POWER_SUPPLY_TYPE_USB) {
				pr_debug("Setting usb psy dp=f dm=f SDP and rerun\n");
				prop.intval = POWER_SUPPLY_DP_DM_DPF_DMF;
				power_supply_set_property(chip->usb_psy,
						POWER_SUPPLY_PROP_DP_DM, &prop);
				chip->apsd_rerun = true;
				rerun_apsd(chip);
				return 0;
			}
			pr_debug("Set usb psy dp=f dm=f DCP and no rerun\n");
			prop.intval = POWER_SUPPLY_DP_DM_DPF_DMF;
			power_supply_set_property(chip->usb_psy,
					POWER_SUPPLY_PROP_DP_DM, &prop);
		}
		/*
		 * If defined force hvdcp 2p0 property,
		 * we force to hvdcp 2p0 in the APSD handler.
		 */
		if (chip->force_hvdcp_2p0) {
			pr_debug("Force set to HVDCP 2.0 mode\n");
			smb1351_masked_write(chip, VARIOUS_FUNC_3_REG,
						QC_2P1_AUTH_ALGO_BIT, 0);
			smb1351_masked_write(chip, CMD_HVDCP_REG,
						CMD_FORCE_HVDCP_2P0_BIT,
						CMD_FORCE_HVDCP_2P0_BIT);
			type = POWER_SUPPLY_TYPE_USB_HVDCP;
		} else if (type == POWER_SUPPLY_TYPE_USB_DCP) {
			pr_debug("schedule hvdcp detection worker\n");
			pm_stay_awake(chip->dev);
			schedule_delayed_work(&chip->hvdcp_det_work,
					msecs_to_jiffies(HVDCP_NOTIFY_MS));
		}

		prop.intval = type;
		power_supply_set_property(chip->usb_psy,
				POWER_SUPPLY_PROP_TYPE, &prop);
		/*
		 * SMB is now done sampling the D+/D- lines,
		 * indicate USB driver
		 */
		pr_debug("updating usb_psy present=%d\n", chip->chg_present);
		prop.intval = chip->chg_present;
		power_supply_set_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT,
				&prop);
		chip->apsd_rerun = false;
	} else if (!chip->apsd_rerun) {
		/* Handle Charger removal */
		prop.intval = POWER_SUPPLY_TYPE_UNKNOWN;
		power_supply_set_property(chip->usb_psy,
				POWER_SUPPLY_PROP_TYPE, &prop);

		chip->chg_present = false;
		prop.intval = chip->chg_present;
		power_supply_set_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT,
				&prop);

		pr_debug("Set usb psy dm=r df=r\n");
		prop.intval = POWER_SUPPLY_DP_DM_DPR_DMR;
		power_supply_set_property(chip->usb_psy,
				POWER_SUPPLY_PROP_DP_DM, &prop);
	}

	return 0;
}

/*
 * As source detect interrupt is not triggered on the falling edge,
 * we need to schedule a work for checking source detect status after
 * charger UV interrupt fired.
 */
#define FIRST_CHECK_DELAY	100
#define SECOND_CHECK_DELAY	1000
static void smb1351_chg_remove_work(struct work_struct *work)
{
	int rc;
	u8 reg;
	struct smb1351_charger *chip = container_of(work,
				struct smb1351_charger, chg_remove_work.work);

	rc = smb1351_read_reg(chip, IRQ_G_REG, &reg);
	if (rc) {
		pr_err("Couldn't read IRQ_G_REG rc = %d\n", rc);
		goto end;
	}

	if (!(reg & IRQ_SOURCE_DET_BIT)) {
		pr_debug("chg removed\n");
		smb1351_apsd_complete_handler(chip, 0);
	} else if (!chip->chg_remove_work_scheduled) {
		chip->chg_remove_work_scheduled = true;
		goto reschedule;
	} else {
		pr_debug("charger is present\n");
	}
end:
	chip->chg_remove_work_scheduled = false;
	pm_relax(chip->dev);
	return;

reschedule:
	pr_debug("reschedule after 1s\n");
	schedule_delayed_work(&chip->chg_remove_work,
				msecs_to_jiffies(SECOND_CHECK_DELAY));
}

static int smb1351_usbin_uv_handler(struct smb1351_charger *chip, u8 status)
{
	union power_supply_propval pval = {0, };

	/* use this to detect USB insertion only if !apsd */
	if (chip->disable_apsd) {
		/*
		 * If APSD is disabled, src det interrupt won't trigger.
		 * Hence use usbin_uv for removal and insertion notification
		 */
		if (status == 0) {
			chip->chg_present = true;
			pr_debug("updating usb_psy present=%d\n",
						chip->chg_present);
			pval.intval = POWER_SUPPLY_TYPE_USB;
			power_supply_set_property(chip->usb_psy,
					POWER_SUPPLY_PROP_TYPE, &pval);

			pval.intval = chip->chg_present;
			power_supply_set_property(chip->usb_psy,
					POWER_SUPPLY_PROP_PRESENT,
					&pval);
		} else {
			chip->chg_present = false;

			pval.intval = POWER_SUPPLY_TYPE_UNKNOWN;
			power_supply_set_property(chip->usb_psy,
					POWER_SUPPLY_PROP_TYPE, &pval);

			pr_debug("updating usb_psy present=%d\n",
							chip->chg_present);
			pval.intval = chip->chg_present;
			power_supply_set_property(chip->usb_psy,
					POWER_SUPPLY_PROP_PRESENT,
					&pval);
		}
		return 0;
	}

	if (status) {
		cancel_delayed_work_sync(&chip->hvdcp_det_work);
		pm_relax(chip->dev);
		pr_debug("schedule charger remove worker\n");
		schedule_delayed_work(&chip->chg_remove_work,
					msecs_to_jiffies(FIRST_CHECK_DELAY));
		pm_stay_awake(chip->dev);
	}

	pr_debug("chip->chg_present = %d\n", chip->chg_present);

	return 0;
}

static int smb1351_usbin_ov_handler(struct smb1351_charger *chip, u8 status)
{
	int rc;
	u8 reg;
	union power_supply_propval pval = {0, };

	rc = smb1351_read_reg(chip, IRQ_E_REG, &reg);
	if (rc)
		pr_err("Couldn't read IRQ_E rc = %d\n", rc);

	if (status != 0) {
		chip->chg_present = false;
		chip->usbin_ov = true;

		pval.intval = POWER_SUPPLY_TYPE_UNKNOWN;
		power_supply_set_property(chip->usb_psy,
				POWER_SUPPLY_PROP_TYPE, &pval);

		pval.intval = chip->chg_present;
		power_supply_set_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT,
				&pval);
	} else {
		chip->usbin_ov = false;
		if (reg & IRQ_USBIN_UV_BIT)
			pr_debug("Charger unplugged from OV\n");
		else
			smb1351_apsd_complete_handler(chip, 1);
	}

	if (chip->usb_psy) {
		pval.intval = status ? POWER_SUPPLY_HEALTH_OVERVOLTAGE
					: POWER_SUPPLY_HEALTH_GOOD;
		power_supply_set_property(chip->usb_psy,
				POWER_SUPPLY_PROP_HEALTH, &pval);
		pr_debug("chip ov status is %d\n", pval.intval);
	}
	pr_debug("chip->chg_present = %d\n", chip->chg_present);

	return 0;
}

static int smb1351_fast_chg_handler(struct smb1351_charger *chip, u8 status)
{
	pr_debug("enter\n");
	return 0;
}

static int smb1351_chg_term_handler(struct smb1351_charger *chip, u8 status)
{
	pr_debug("enter\n");
	if (!chip->bms_controlled_charging)
		chip->batt_full = !!status;
	return 0;
}

static int smb1351_safety_timeout_handler(struct smb1351_charger *chip,
						u8 status)
{
	pr_debug("safety_timeout triggered\n");
	return 0;
}

static int smb1351_aicl_done_handler(struct smb1351_charger *chip, u8 status)
{
	pr_debug("aicl_done triggered\n");
	return 0;
}

static int smb1351_hot_hard_handler(struct smb1351_charger *chip, u8 status)
{
	pr_debug("status = 0x%02x\n", status);
	chip->batt_hot = !!status;
	return 0;
}
static int smb1351_cold_hard_handler(struct smb1351_charger *chip, u8 status)
{
	pr_debug("status = 0x%02x\n", status);
	chip->batt_cold = !!status;
	return 0;
}
static int smb1351_hot_soft_handler(struct smb1351_charger *chip, u8 status)
{
	pr_debug("status = 0x%02x\n", status);
	chip->batt_warm = !!status;
	return 0;
}
static int smb1351_cold_soft_handler(struct smb1351_charger *chip, u8 status)
{
	pr_debug("status = 0x%02x\n", status);
	chip->batt_cool = !!status;
	return 0;
}

static int smb1351_battery_missing_handler(struct smb1351_charger *chip,
						u8 status)
{
	if (status)
		chip->battery_missing = true;
	else
		chip->battery_missing = false;

	return 0;
}

static struct irq_handler_info handlers[] = {
	[0] = {
		.stat_reg	= IRQ_A_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{	.name	 = "cold_soft",
				.smb_irq = smb1351_cold_soft_handler,
			},
			{	.name	 = "hot_soft",
				.smb_irq = smb1351_hot_soft_handler,
			},
			{	.name	 = "cold_hard",
				.smb_irq = smb1351_cold_hard_handler,
			},
			{	.name	 = "hot_hard",
				.smb_irq = smb1351_hot_hard_handler,
			},
		},
	},
	[1] = {
		.stat_reg	= IRQ_B_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{	.name	 = "internal_temp_limit",
			},
			{	.name	 = "vbatt_low",
			},
			{	.name	 = "battery_missing",
				.smb_irq = smb1351_battery_missing_handler,
			},
			{	.name	 = "batt_therm_removed",
			},
		},
	},
	[2] = {
		.stat_reg	= IRQ_C_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{	.name	 = "chg_term",
				.smb_irq = smb1351_chg_term_handler,
			},
			{	.name	 = "taper",
			},
			{	.name	 = "recharge",
			},
			{	.name	 = "fast_chg",
				.smb_irq = smb1351_fast_chg_handler,
			},
		},
	},
	[3] = {
		.stat_reg	= IRQ_D_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{	.name	 = "prechg_timeout",
			},
			{	.name	 = "safety_timeout",
				.smb_irq = smb1351_safety_timeout_handler,
			},
			{	.name	 = "chg_error",
			},
			{	.name	 = "batt_ov",
			},
		},
	},
	[4] = {
		.stat_reg	= IRQ_E_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{	.name	 = "power_ok",
			},
			{	.name	 = "afvc",
			},
			{	.name	 = "usbin_uv",
				.smb_irq = smb1351_usbin_uv_handler,
			},
			{	.name	 = "usbin_ov",
				.smb_irq = smb1351_usbin_ov_handler,
			},
		},
	},
	[5] = {
		.stat_reg	= IRQ_F_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{	.name	 = "otg_oc_retry",
			},
			{	.name	 = "rid",
			},
			{	.name	 = "otg_fail",
			},
			{	.name	 = "otg_oc",
			},
		},
	},
	[6] = {
		.stat_reg	= IRQ_G_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{	.name	 = "chg_inhibit",
			},
			{	.name	 = "aicl_fail",
			},
			{	.name	 = "aicl_done",
				.smb_irq = smb1351_aicl_done_handler,
			},
			{	.name	 = "apsd_complete",
				.smb_irq = smb1351_apsd_complete_handler,
			},
		},
	},
	[7] = {
		.stat_reg	= IRQ_H_REG,
		.val		= 0,
		.prev_val	= 0,
		.irq_info	= {
			{	.name	 = "wdog_timeout",
			},
			{	.name	 = "hvdcp_auth_done",
			},
		},
	},
};

#define IRQ_LATCHED_MASK	0x02
#define IRQ_STATUS_MASK		0x01
#define BITS_PER_IRQ		2
static irqreturn_t smb1351_chg_stat_handler(int irq, void *dev_id)
{
	struct smb1351_charger *chip = dev_id;
	int i, j;
	u8 triggered;
	u8 changed;
	u8 rt_stat, prev_rt_stat;
	int rc;
	int handler_count = 0;

	mutex_lock(&chip->irq_complete);

	chip->irq_waiting = true;
	if (!chip->resume_completed) {
		pr_debug("IRQ triggered before device-resume\n");
		disable_irq_nosync(irq);
		mutex_unlock(&chip->irq_complete);
		return IRQ_HANDLED;
	}
	chip->irq_waiting = false;

	for (i = 0; i < ARRAY_SIZE(handlers); i++) {
		rc = smb1351_read_reg(chip, handlers[i].stat_reg,
						&handlers[i].val);
		if (rc) {
			pr_err("Couldn't read %d rc = %d\n",
					handlers[i].stat_reg, rc);
			continue;
		}

		for (j = 0; j < ARRAY_SIZE(handlers[i].irq_info); j++) {
			triggered = handlers[i].val
			       & (IRQ_LATCHED_MASK << (j * BITS_PER_IRQ));
			rt_stat = handlers[i].val
				& (IRQ_STATUS_MASK << (j * BITS_PER_IRQ));
			prev_rt_stat = handlers[i].prev_val
				& (IRQ_STATUS_MASK << (j * BITS_PER_IRQ));
			changed = prev_rt_stat ^ rt_stat;

			if (triggered || changed)
				rt_stat ? handlers[i].irq_info[j].high++ :
						handlers[i].irq_info[j].low++;

			if ((triggered || changed)
				&& handlers[i].irq_info[j].smb_irq != NULL) {
				handler_count++;
				rc = handlers[i].irq_info[j].smb_irq(chip,
								rt_stat);
				if (rc)
					pr_err("Couldn't handle %d irq for reg 0x%02x rc = %d\n",
						j, handlers[i].stat_reg, rc);
			}
		}
		handlers[i].prev_val = handlers[i].val;
	}

	pr_debug("handler count = %d\n", handler_count);
	if (handler_count) {
		pr_debug("batt psy changed\n");
		power_supply_changed(chip->batt_psy);
	}

	mutex_unlock(&chip->irq_complete);

	return IRQ_HANDLED;
}

static void smb1351_external_power_changed(struct power_supply *psy)
{
	struct smb1351_charger *chip = power_supply_get_drvdata(psy);
	union power_supply_propval prop = {0,};
	int rc, current_limit = 0, online = 0;

	if (chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &prop);
	if (rc)
		pr_err("Couldn't read USB online property, rc=%d\n", rc);
	else
		online = prop.intval;

	rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (rc)
		pr_err("Couldn't read USB current_max property, rc=%d\n", rc);
	else
		current_limit = prop.intval / 1000;

	pr_debug("online = %d, current_limit = %d\n", online, current_limit);

	smb1351_enable_volatile_writes(chip);
	smb1351_set_usb_chg_current(chip, current_limit);

	pr_debug("updating batt psy\n");
}

#define LAST_CNFG_REG	0x16
static int show_cnfg_regs(struct seq_file *m, void *data)
{
	struct smb1351_charger *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = 0; addr <= LAST_CNFG_REG; addr++) {
		rc = smb1351_read_reg(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int cnfg_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb1351_charger *chip = inode->i_private;

	return single_open(file, show_cnfg_regs, chip);
}

static const struct file_operations cnfg_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= cnfg_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#define FIRST_CMD_REG	0x30
#define LAST_CMD_REG	0x34
static int show_cmd_regs(struct seq_file *m, void *data)
{
	struct smb1351_charger *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = FIRST_CMD_REG; addr <= LAST_CMD_REG; addr++) {
		rc = smb1351_read_reg(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int cmd_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb1351_charger *chip = inode->i_private;

	return single_open(file, show_cmd_regs, chip);
}

static const struct file_operations cmd_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= cmd_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#define FIRST_STATUS_REG	0x36
#define LAST_STATUS_REG		0x3F
static int show_status_regs(struct seq_file *m, void *data)
{
	struct smb1351_charger *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = FIRST_STATUS_REG; addr <= LAST_STATUS_REG; addr++) {
		rc = smb1351_read_reg(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int status_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb1351_charger *chip = inode->i_private;

	return single_open(file, show_status_regs, chip);
}

static const struct file_operations status_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= status_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int show_irq_count(struct seq_file *m, void *data)
{
	int i, j, total = 0;

	for (i = 0; i < ARRAY_SIZE(handlers); i++)
		for (j = 0; j < 4; j++) {
			seq_printf(m, "%s=%d\t(high=%d low=%d)\n",
						handlers[i].irq_info[j].name,
						handlers[i].irq_info[j].high
						+ handlers[i].irq_info[j].low,
						handlers[i].irq_info[j].high,
						handlers[i].irq_info[j].low);
			total += (handlers[i].irq_info[j].high
					+ handlers[i].irq_info[j].low);
		}

	seq_printf(m, "\n\tTotal = %d\n", total);

	return 0;
}

static int irq_count_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb1351_charger *chip = inode->i_private;

	return single_open(file, show_irq_count, chip);
}

static const struct file_operations irq_count_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= irq_count_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int get_reg(void *data, u64 *val)
{
	struct smb1351_charger *chip = data;
	int rc;
	u8 temp;

	rc = smb1351_read_reg(chip, chip->peek_poke_address, &temp);
	if (rc) {
		pr_err("Couldn't read reg %x rc = %d\n",
			chip->peek_poke_address, rc);
		return -EAGAIN;
	}
	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	struct smb1351_charger *chip = data;
	int rc;
	u8 temp;

	temp = (u8) val;
	rc = smb1351_write_reg(chip, chip->peek_poke_address, temp);
	if (rc) {
		pr_err("Couldn't write 0x%02x to 0x%02x rc= %d\n",
			temp, chip->peek_poke_address, rc);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(poke_poke_debug_ops, get_reg, set_reg, "0x%02llx\n");

static int force_irq_set(void *data, u64 val)
{
	struct smb1351_charger *chip = data;

	smb1351_chg_stat_handler(chip->client->irq, data);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(force_irq_ops, NULL, force_irq_set, "0x%02llx\n");

#ifdef DEBUG
static void dump_regs(struct smb1351_charger *chip)
{
	int rc;
	u8 reg;
	u8 addr;

	for (addr = 0; addr <= LAST_CNFG_REG; addr++) {
		rc = smb1351_read_reg(chip, addr, &reg);
		if (rc)
			pr_err("Couldn't read 0x%02x rc = %d\n", addr, rc);
		else
			pr_debug("0x%02x = 0x%02x\n", addr, reg);
	}

	for (addr = FIRST_STATUS_REG; addr <= LAST_STATUS_REG; addr++) {
		rc = smb1351_read_reg(chip, addr, &reg);
		if (rc)
			pr_err("Couldn't read 0x%02x rc = %d\n", addr, rc);
		else
			pr_debug("0x%02x = 0x%02x\n", addr, reg);
	}

	for (addr = FIRST_CMD_REG; addr <= LAST_CMD_REG; addr++) {
		rc = smb1351_read_reg(chip, addr, &reg);
		if (rc)
			pr_err("Couldn't read 0x%02x rc = %d\n", addr, rc);
		else
			pr_debug("0x%02x = 0x%02x\n", addr, reg);
	}
}
#else
static void dump_regs(struct smb1351_charger *chip)
{
}
#endif

static int smb1351_parse_dt(struct smb1351_charger *chip)
{
	int rc;
	struct device_node *node = chip->dev->of_node;

	if (!node) {
		pr_err("device tree info. missing\n");
		return -EINVAL;
	}

	chip->usb_suspended_status = of_property_read_bool(node,
					"qcom,charging-disabled");

	chip->chg_autonomous_mode = of_property_read_bool(node,
					"qcom,chg-autonomous-mode");

	chip->disable_apsd = of_property_read_bool(node, "qcom,disable-apsd");

	chip->using_pmic_therm = of_property_read_bool(node,
						"qcom,using-pmic-therm");
	chip->bms_controlled_charging  = of_property_read_bool(node,
					"qcom,bms-controlled-charging");
	chip->force_hvdcp_2p0 = of_property_read_bool(node,
					"qcom,force-hvdcp-2p0");

	rc = of_property_read_string(node, "qcom,bms-psy-name",
						&chip->bms_psy_name);
	if (rc)
		chip->bms_psy_name = NULL;

	rc = of_property_read_u32(node, "qcom,fastchg-current-max-ma",
					&chip->target_fastchg_current_max_ma);
	if (rc)
		chip->target_fastchg_current_max_ma = SMB1351_CHG_FAST_MAX_MA;

	chip->iterm_disabled = of_property_read_bool(node,
					"qcom,iterm-disabled");

	rc = of_property_read_u32(node, "qcom,iterm-ma", &chip->iterm_ma);
	if (rc)
		chip->iterm_ma = -EINVAL;

	rc = of_property_read_u32(node, "qcom,float-voltage-mv",
						&chip->vfloat_mv);
	if (rc)
		chip->vfloat_mv = -EINVAL;

	rc = of_property_read_u32(node, "qcom,recharge-mv",
						&chip->recharge_mv);
	if (rc)
		chip->recharge_mv = -EINVAL;

	chip->recharge_disabled = of_property_read_bool(node,
					"qcom,recharge-disabled");

	/* thermal and jeita support */
	rc = of_property_read_u32(node, "qcom,batt-cold-decidegc",
						&chip->batt_cold_decidegc);
	if (rc < 0)
		chip->batt_cold_decidegc = -EINVAL;

	rc = of_property_read_u32(node, "qcom,batt-hot-decidegc",
						&chip->batt_hot_decidegc);
	if (rc < 0)
		chip->batt_hot_decidegc = -EINVAL;

	rc = of_property_read_u32(node, "qcom,batt-warm-decidegc",
						&chip->batt_warm_decidegc);

	rc |= of_property_read_u32(node, "qcom,batt-cool-decidegc",
						&chip->batt_cool_decidegc);

	if (!rc) {
		rc = of_property_read_u32(node, "qcom,batt-cool-mv",
						&chip->batt_cool_mv);

		rc |= of_property_read_u32(node, "qcom,batt-warm-mv",
						&chip->batt_warm_mv);

		rc |= of_property_read_u32(node, "qcom,batt-cool-ma",
						&chip->batt_cool_ma);

		rc |= of_property_read_u32(node, "qcom,batt-warm-ma",
						&chip->batt_warm_ma);
		if (rc)
			chip->jeita_supported = false;
		else
			chip->jeita_supported = true;
	}

	pr_debug("jeita_supported = %d\n", chip->jeita_supported);

	rc = of_property_read_u32(node, "qcom,batt-missing-decidegc",
						&chip->batt_missing_decidegc);

	chip->pinctrl_state_name = of_get_property(node, "pinctrl-names", NULL);

	return 0;
}

static int smb1351_determine_initial_state(struct smb1351_charger *chip)
{
	int rc;
	u8 reg = 0;

	/*
	 * It is okay to read the interrupt status here since
	 * interrupts aren't requested. Reading interrupt status
	 * clears the interrupt so be careful to read interrupt
	 * status only in interrupt handling code
	 */

	rc = smb1351_read_reg(chip, IRQ_B_REG, &reg);
	if (rc) {
		pr_err("Couldn't read IRQ_B rc = %d\n", rc);
		goto fail_init_status;
	}

	chip->battery_missing = (reg & IRQ_BATT_MISSING_BIT) ? true : false;

	rc = smb1351_read_reg(chip, IRQ_C_REG, &reg);
	if (rc) {
		pr_err("Couldn't read IRQ_C rc = %d\n", rc);
		goto fail_init_status;
	}
	chip->batt_full = (reg & IRQ_TERM_BIT) ? true : false;

	rc = smb1351_read_reg(chip, IRQ_A_REG, &reg);
	if (rc) {
		pr_err("Couldn't read irq A rc = %d\n", rc);
		return rc;
	}

	if (reg & IRQ_HOT_HARD_BIT)
		chip->batt_hot = true;
	if (reg & IRQ_COLD_HARD_BIT)
		chip->batt_cold = true;
	if (reg & IRQ_HOT_SOFT_BIT)
		chip->batt_warm = true;
	if (reg & IRQ_COLD_SOFT_BIT)
		chip->batt_cool = true;

	rc = smb1351_read_reg(chip, IRQ_E_REG, &reg);
	if (rc) {
		pr_err("Couldn't read IRQ_E rc = %d\n", rc);
		goto fail_init_status;
	}

	if (reg & IRQ_USBIN_UV_BIT) {
		smb1351_usbin_uv_handler(chip, 1);
	} else {
		smb1351_usbin_uv_handler(chip, 0);
		smb1351_apsd_complete_handler(chip, 1);
	}

	rc = smb1351_read_reg(chip, IRQ_G_REG, &reg);
	if (rc) {
		pr_err("Couldn't read IRQ_G rc = %d\n", rc);
		goto fail_init_status;
	}

	if (reg & IRQ_SOURCE_DET_BIT)
		smb1351_apsd_complete_handler(chip, 1);

	return 0;

fail_init_status:
	pr_err("Couldn't determine initial status\n");
	return rc;
}

static int is_parallel_charger(struct i2c_client *client)
{
	struct device_node *node = client->dev.of_node;

	return of_property_read_bool(node, "qcom,parallel-charger");
}

static int create_debugfs_entries(struct smb1351_charger *chip)
{
	struct dentry *ent;

	chip->debug_root = debugfs_create_dir("smb1351", NULL);
	if (!chip->debug_root) {
		pr_err("Couldn't create debug dir\n");
	} else {
		ent = debugfs_create_file("config_registers", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &cnfg_debugfs_ops);
		if (!ent)
			pr_err("Couldn't create cnfg debug file\n");

		ent = debugfs_create_file("status_registers", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &status_debugfs_ops);
		if (!ent)
			pr_err("Couldn't create status debug file\n");

		ent = debugfs_create_file("cmd_registers", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &cmd_debugfs_ops);
		if (!ent)
			pr_err("Couldn't create cmd debug file\n");

		ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root,
					  &(chip->peek_poke_address));
		if (!ent)
			pr_err("Couldn't create address debug file\n");

		ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, chip,
					  &poke_poke_debug_ops);
		if (!ent)
			pr_err("Couldn't create data debug file\n");

		ent = debugfs_create_file("force_irq",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, chip,
					  &force_irq_ops);
		if (!ent)
			pr_err("Couldn't create data debug file\n");

		ent = debugfs_create_file("irq_count", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &irq_count_debugfs_ops);
		if (!ent)
			pr_err("Couldn't create count debug file\n");
	}
	return 0;
}

static int smb1351_main_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int rc;
	struct smb1351_charger *chip;
	struct power_supply *usb_psy;
	struct power_supply_config batt_psy_cfg = {};
	u8 reg = 0;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_debug("USB psy not found; deferring probe\n");
		return -EPROBE_DEFER;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		pr_err("Couldn't allocate memory\n");
		return -ENOMEM;
	}

	chip->client = client;
	chip->dev = &client->dev;
	chip->usb_psy = usb_psy;
	chip->fake_battery_soc = -EINVAL;
	INIT_DELAYED_WORK(&chip->chg_remove_work, smb1351_chg_remove_work);
	INIT_DELAYED_WORK(&chip->hvdcp_det_work, smb1351_hvdcp_det_work);
	device_init_wakeup(chip->dev, true);

	/* probe the device to check if its actually connected */
	rc = smb1351_read_reg(chip, CHG_REVISION_REG, &reg);
	if (rc) {
		pr_err("Failed to detect smb1351, device may be absent\n");
		return -ENODEV;
	}
	pr_debug("smb1351 chip revision is %d\n", reg);

	rc = smb1351_parse_dt(chip);
	if (rc) {
		pr_err("Couldn't parse DT nodes rc=%d\n", rc);
		return rc;
	}

	/* using vadc and adc_tm for implementing pmic therm */
	if (chip->using_pmic_therm) {
		chip->vadc_dev = qpnp_get_vadc(chip->dev, "chg");
		if (IS_ERR(chip->vadc_dev)) {
			rc = PTR_ERR(chip->vadc_dev);
			if (rc != -EPROBE_DEFER)
				pr_err("vadc property missing\n");
			return rc;
		}
		chip->adc_tm_dev = qpnp_get_adc_tm(chip->dev, "chg");
		if (IS_ERR(chip->adc_tm_dev)) {
			rc = PTR_ERR(chip->adc_tm_dev);
			if (rc != -EPROBE_DEFER)
				pr_err("adc_tm property missing\n");
			return rc;
		}
	}

	i2c_set_clientdata(client, chip);

	chip->batt_psy_d.name		= "battery";
	chip->batt_psy_d.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy_d.get_property	= smb1351_battery_get_property;
	chip->batt_psy_d.set_property	= smb1351_battery_set_property;
	chip->batt_psy_d.property_is_writeable =
					smb1351_batt_property_is_writeable;
	chip->batt_psy_d.properties	= smb1351_battery_properties;
	chip->batt_psy_d.num_properties	=
				ARRAY_SIZE(smb1351_battery_properties);
	chip->batt_psy_d.external_power_changed =
					smb1351_external_power_changed;

	chip->resume_completed = true;
	mutex_init(&chip->irq_complete);

	batt_psy_cfg.drv_data = chip;
	batt_psy_cfg.supplied_to = pm_batt_supplied_to;
	batt_psy_cfg.num_supplicants = ARRAY_SIZE(pm_batt_supplied_to);
	chip->batt_psy = devm_power_supply_register(chip->dev,
			&chip->batt_psy_d,
			&batt_psy_cfg);
	if (IS_ERR(chip->batt_psy)) {
		pr_err("Couldn't register batt psy rc=%ld\n",
				PTR_ERR(chip->batt_psy));
		return rc;
	}

	dump_regs(chip);

	rc = smb1351_regulator_init(chip);
	if (rc) {
		pr_err("Couldn't initialize smb1351 ragulator rc=%d\n", rc);
		goto fail_smb1351_regulator_init;
	}

	rc = smb1351_hw_init(chip);
	if (rc) {
		pr_err("Couldn't intialize hardware rc=%d\n", rc);
		goto fail_smb1351_hw_init;
	}

	rc = smb1351_determine_initial_state(chip);
	if (rc) {
		pr_err("Couldn't determine initial state rc=%d\n", rc);
		goto fail_smb1351_hw_init;
	}

	/* STAT irq configuration */
	if (client->irq) {
		rc = devm_request_threaded_irq(&client->dev, client->irq, NULL,
				smb1351_chg_stat_handler,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"smb1351_chg_stat_irq", chip);
		if (rc) {
			pr_err("Failed STAT irq=%d request rc = %d\n",
				client->irq, rc);
			goto fail_smb1351_hw_init;
		}
		enable_irq_wake(client->irq);
	}

	if (chip->using_pmic_therm) {
		if (!chip->jeita_supported) {
			/* add hot/cold temperature monitor */
			chip->adc_param.low_temp = chip->batt_cold_decidegc;
			chip->adc_param.high_temp = chip->batt_hot_decidegc;
		} else {
			chip->adc_param.low_temp = chip->batt_cool_decidegc;
			chip->adc_param.high_temp = chip->batt_warm_decidegc;
		}
		chip->adc_param.timer_interval = ADC_MEAS1_INTERVAL_500MS;
		chip->adc_param.state_request = ADC_TM_WARM_COOL_THR_ENABLE;
		chip->adc_param.btm_ctx = chip;
		chip->adc_param.threshold_notification =
				smb1351_chg_adc_notification;
		chip->adc_param.channel = LR_MUX1_BATT_THERM;

		rc = qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
							&chip->adc_param);
		if (rc) {
			pr_err("requesting ADC error %d\n", rc);
			goto fail_smb1351_hw_init;
		}
	}

	create_debugfs_entries(chip);

	dump_regs(chip);

	pr_info("smb1351 successfully probed. charger=%d, batt=%d version=%s\n",
			chip->chg_present,
			smb1351_get_prop_batt_present(chip),
			smb1351_version_str[chip->version]);
	return 0;

fail_smb1351_hw_init:
	regulator_unregister(chip->otg_vreg.rdev);
fail_smb1351_regulator_init:
	return rc;
}

static int smb1351_parallel_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int rc;
	struct smb1351_charger *chip;
	struct device_node *node = client->dev.of_node;
	struct power_supply_config parallel_psy_cfg = {};

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		pr_err("Couldn't allocate memory\n");
		return -ENOMEM;
	}

	chip->client = client;
	chip->dev = &client->dev;
	chip->parallel_charger = true;

	chip->usb_suspended_status = of_property_read_bool(node,
					"qcom,charging-disabled");
	rc = of_property_read_u32(node, "qcom,float-voltage-mv",
						&chip->vfloat_mv);
	if (rc)
		chip->vfloat_mv = -EINVAL;
	rc = of_property_read_u32(node, "qcom,recharge-mv",
						&chip->recharge_mv);
	if (rc)
		chip->recharge_mv = -EINVAL;

	rc = of_property_read_u32(node, "qcom,parallel-en-pin-polarity",
					&chip->parallel_pin_polarity_setting);
	if (rc)
		chip->parallel_pin_polarity_setting = EN_BY_PIN_LOW_ENABLE;
	else
		chip->parallel_pin_polarity_setting =
				chip->parallel_pin_polarity_setting ?
				EN_BY_PIN_HIGH_ENABLE : EN_BY_PIN_LOW_ENABLE;

	i2c_set_clientdata(client, chip);

	chip->parallel_psy_d.name = "parallel";
	chip->parallel_psy_d.type = POWER_SUPPLY_TYPE_PARALLEL;
	chip->parallel_psy_d.get_property = smb1351_parallel_get_property;
	chip->parallel_psy_d.set_property = smb1351_parallel_set_property;
	chip->parallel_psy_d.properties	= smb1351_parallel_properties;
	chip->parallel_psy_d.property_is_writeable
				= smb1351_parallel_is_writeable;
	chip->parallel_psy_d.num_properties
				= ARRAY_SIZE(smb1351_parallel_properties);

	parallel_psy_cfg.drv_data = chip;
	parallel_psy_cfg.num_supplicants = 0;
	chip->parallel_psy = devm_power_supply_register(chip->dev,
			&chip->parallel_psy_d,
			&parallel_psy_cfg);
	if (IS_ERR(chip->parallel_psy)) {
		pr_err("Couldn't register parallel psy rc=%ld\n",
				PTR_ERR(chip->parallel_psy));
		return rc;
	}

	chip->resume_completed = true;
	mutex_init(&chip->irq_complete);

	create_debugfs_entries(chip);

	pr_info("smb1351 parallel successfully probed.\n");

	return 0;
}

static int smb1351_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	if (is_parallel_charger(client))
		return smb1351_parallel_charger_probe(client, id);
	else
		return smb1351_main_charger_probe(client, id);
}

static int smb1351_charger_remove(struct i2c_client *client)
{
	struct smb1351_charger *chip = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&chip->chg_remove_work);

	mutex_destroy(&chip->irq_complete);
	debugfs_remove_recursive(chip->debug_root);
	return 0;
}

static int smb1351_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smb1351_charger *chip = i2c_get_clientdata(client);

	/* no suspend resume activities for parallel charger */
	if (chip->parallel_charger)
		return 0;

	mutex_lock(&chip->irq_complete);
	chip->resume_completed = false;
	mutex_unlock(&chip->irq_complete);

	return 0;
}

static int smb1351_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smb1351_charger *chip = i2c_get_clientdata(client);

	/* no suspend resume activities for parallel charger */
	if (chip->parallel_charger)
		return 0;

	if (chip->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;
}

static int smb1351_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smb1351_charger *chip = i2c_get_clientdata(client);

	/* no suspend resume activities for parallel charger */
	if (chip->parallel_charger)
		return 0;

	mutex_lock(&chip->irq_complete);
	chip->resume_completed = true;
	if (chip->irq_waiting) {
		mutex_unlock(&chip->irq_complete);
		smb1351_chg_stat_handler(client->irq, chip);
		enable_irq(client->irq);
	} else {
		mutex_unlock(&chip->irq_complete);
	}
	return 0;
}

static const struct dev_pm_ops smb1351_pm_ops = {
	.suspend	= smb1351_suspend,
	.suspend_noirq	= smb1351_suspend_noirq,
	.resume		= smb1351_resume,
};

static struct of_device_id smb1351_match_table[] = {
	{ .compatible = "qcom,smb1351-charger",},
	{ },
};

static const struct i2c_device_id smb1351_charger_id[] = {
	{"smb1351-charger", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, smb1351_charger_id);

static struct i2c_driver smb1351_charger_driver = {
	.driver		= {
		.name		= "smb1351-charger",
		.owner		= THIS_MODULE,
		.of_match_table	= smb1351_match_table,
		.pm		= &smb1351_pm_ops,
	},
	.probe		= smb1351_charger_probe,
	.remove		= smb1351_charger_remove,
	.id_table	= smb1351_charger_id,
};

module_i2c_driver(smb1351_charger_driver);

MODULE_DESCRIPTION("smb1351 Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:smb1351-charger");
