/*
 * Summit Microelectronics SMB347 Battery Charger Driver
 *
 * Copyright (C) 2011, Intel Corporation
 *
 * Authors: Bruce E. Robertson <bruce.e.robertson@intel.com>
 *          Mika Westerberg <mika.westerberg@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/power/smb347-charger.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/usb/phy.h>
#include <linux/notifier.h>
#include <linux/regmap.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/wakelock.h>
#include <linux/extcon.h>

/*
 * Configuration registers. These are mirrored to volatile RAM and can be
 * written once %CMD_A_ALLOW_WRITE is set in %CMD_A register. They will be
 * reloaded from non-volatile registers after POR.
 */


#define CFG_CHARGE_CURRENT			0x00
#define CFG_CHARGE_CURRENT_FCC_MASK		0xe0
#define CFG_CHARGE_CURRENT_FCC_SHIFT		5
#define CFG_CHARGE_CURRENT_PCC_MASK		0x18
#define CFG_CHARGE_CURRENT_PCC_SHIFT		3
#define CFG_CHARGE_CURRENT_TC_MASK		0x07
#define CFG_CHARGE_CURRENT_TC_SHIFT		0x00

#define SMB349_CFG_CHARGE_CURRENT_FCC_MASK	0xf0
#define SMB349_CFG_CHARGE_CURRENT_FCC_SHIFT	4
#define SMB349_CFG_CURRENT_LIMIT_MASK		0x0f
#define SMB349_CFG_CURRENT_LIMIT_SHIFT		0

#define CFG_CURRENT_LIMIT			0x01
#define CFG_CURRENT_LIMIT_DC_MASK		0xf0
#define CFG_CURRENT_LIMIT_DC_SHIFT		4
#define CFG_CURRENT_LIMIT_USB_MASK		0x0f

#define SMB349_CFG_CHARGE_CURRENT_PCC_MASK	0xe0
#define SMB349_CFG_CHARGE_CURRENT_PCC_SHIFT	5
#define SMB349_CFG_CHARGE_CURRENT_TC_MASK	0x1c
#define SMB349_CFG_CHARGE_CURRENT_TC_SHIFT	2
#define SMB349_USB23_SELECTION_MASK		0x2
#define SMB349_USB23_SELECTION_SHIFT		0x1

#define CFG_VARIOUS_FUNCS			0x02
#define CFG_VARIOUS_FUNCS_PRIORITY_USB		BIT(2)

#define SMB349_CFG_VARIOUS_FUNCS_SUSP_CONTROL	BIT(7)
#define SMB349_CFG_VARIOUS_FUNCS_OPTICHG_EN	BIT(4)
#define SMB349_CFG_VARIOUS_FUNCS_APSD_EN	BIT(2)

#define CFG_FLOAT_VOLTAGE			0x03
#define CFG_FLOAT_VOLTAGE_FLOAT_MASK		0x3f
#define CFG_FLOAT_VOLTAGE_THRESHOLD_MASK	0xc0
#define CFG_FLOAT_VOLTAGE_THRESHOLD_SHIFT	6

#define CFG_CHRG_CONTROL			0x4
#define CFG_CHRG_CTRL_HW_TERM			BIT(6)
#define CFG_CHRG_CTRL_AUTO_RECHARGE		BIT(7)

#define CFG_STAT				0x05
#define CFG_STAT_DISABLED			BIT(5)
#define CFG_STAT_ACTIVE_HIGH			BIT(7)

#define CFG_PIN					0x06
#define CFG_PIN_EN_CTRL_MASK			0x60
#define CFG_PIN_EN_CTRL_ACTIVE_HIGH		0x40
#define CFG_PIN_EN_CTRL_ACTIVE_LOW		0x60
#define CFG_PIN_EN_APSD_IRQ			BIT(1)
#define CFG_PIN_EN_CHARGER_ERROR		BIT(2)

#define CFG_THERM				0x07
#define CFG_THERM_SOFT_HOT_COMPENSATION_MASK	0x03
#define CFG_THERM_SOFT_HOT_COMPENSATION_SHIFT	0
#define CFG_THERM_SOFT_COLD_COMPENSATION_MASK	0x0c
#define CFG_THERM_SOFT_COLD_COMPENSATION_SHIFT	2
#define CFG_THERM_MONITOR_DISABLED		BIT(4)

#define SMB347_CFG_SYSOK			0x08
#define CFG_SYSOK_SUSPEND_HARD_LIMIT_DISABLED	BIT(2)

#define CFG_OTHER				0x09
#define CFG_OTHER_RID_MASK			0xc0
#define CFG_OTHER_RID_DISABLED_OTG_I2C		0x00
#define CFG_OTHER_RID_DISABLED_OTG_PIN		0x40
#define CFG_OTHER_RID_ENABLED_OTG_I2C		0x80
#define CFG_OTHER_RID_ENABLED_AUTO_OTG		0xc0
#define CFG_OTHER_OTG_PIN_ACTIVE_LOW		BIT(5)

#define CFG_OTG					0x0a
#define CFG_OTG_TEMP_THRESHOLD_MASK		0x30
#define CFG_OTG_TEMP_THRESHOLD_SHIFT		4
#define CFG_OTG_CC_COMPENSATION_MASK		0xc0
#define CFG_OTG_CC_COMPENSATION_SHIFT		6
#define CFG_OTG_BATTERY_UVLO_THRESHOLD_MASK	0x03

#define CFG_TEMP_LIMIT				0x0b
#define CFG_TEMP_LIMIT_SOFT_HOT_MASK		0x03
#define CFG_TEMP_LIMIT_SOFT_HOT_SHIFT		0
#define CFG_TEMP_LIMIT_SOFT_COLD_MASK		0x0c
#define CFG_TEMP_LIMIT_SOFT_COLD_SHIFT		2
#define CFG_TEMP_LIMIT_HARD_HOT_MASK		0x30
#define CFG_TEMP_LIMIT_HARD_HOT_SHIFT		4
#define CFG_TEMP_LIMIT_HARD_COLD_MASK		0xc0
#define CFG_TEMP_LIMIT_HARD_COLD_SHIFT		6

#define CFG_FAULT_IRQ				0x0c
#define CFG_FAULT_IRQ_DCIN_UV			BIT(2)
#define CFG_FAULT_IRQ_OTG_UV			BIT(5)

#define CFG_STATUS_IRQ				0x0d
#define CFG_STATUS_OTG_DET			BIT(6)
#define CFG_STATUS_IRQ_TERMINATION_OR_TAPER	BIT(4)
#define CFG_STATUS_IRQ_CHARGE_TIMEOUT		BIT(7)
#define CFG_STATUS_IRQ_INOK			BIT(2)

#define SMB347_CFG_ADDRESS			0x0e

#define SMB349_CFG_SYSOK			0x0e
#define CFG_SYSOK_CC_COMPENSATION_MASK		0x20
#define CFG_SYSOK_CC_COMPENSATION_SHIFT	5

#define SMB349_FLEXCHARGE			0x10
#define SMB349_STATUS_INT			0x11
#define SMB349_CFG_ADDRESS			0x12

/* Command registers */
#define CMD_A					0x30
#define CMD_A_CHG_ENABLED			BIT(1)
#define CMD_A_SUSPEND_ENABLED			BIT(2)
#define CMD_A_OTG_ENABLED			BIT(4)
#define CMD_A_FORCE_FCC				BIT(6)
#define CMD_A_ALLOW_WRITE			BIT(7)
#define CMD_B					0x31
#define CMD_B_MODE_HC				BIT(0)
#define CMD_C					0x33

#define CHIP_ID				0x34
#define CHIP_ID_MASK				0xf
#define CHIP_ID_SMB349				0x4
#define CHIP_ID_SMB347				0x9

/* Interrupt Status registers */
#define IRQSTAT_A				0x35
#define IRQSTAT_A_HOT_HARD_STAT		BIT(6)
#define IRQSTAT_A_HOT_HARD_IRQ			BIT(7)
#define IRQSTAT_A_COLD_HARD_STAT		BIT(4)
#define IRQSTAT_A_COLD_HARD_IRQ		BIT(5)

#define IRQSTAT_B				0x36
#define IRQSTAT_B_BATOVP_STAT			BIT(6)
#define IRQSTAT_B_BATOVP_IRQ			BIT(7)

#define IRQSTAT_C				0x37
#define IRQSTAT_C_TERMINATION_STAT		BIT(0)
#define IRQSTAT_C_TERMINATION_IRQ		BIT(1)
#define IRQSTAT_C_TAPER_IRQ			BIT(3)

#define IRQSTAT_D				0x38
#define IRQSTAT_D_CHARGE_TIMEOUT_STAT		BIT(2)
#define IRQSTAT_D_CHARGE_TIMEOUT_IRQ		BIT(3)
#define IRQSTAT_D_APSD_STAT			BIT(6)
#define IRQSTAT_D_APSD_IRQ			BIT(7)

#define IRQSTAT_E				0x39
#define IRQSTAT_E_USBIN_UV_STAT			BIT(0)
#define IRQSTAT_E_USBIN_UV_IRQ			BIT(1)
#define IRQSTAT_E_USBIN_OV_STAT			BIT(2)
#define IRQSTAT_E_USBIN_OV_IRQ			BIT(3)
#define IRQSTAT_E_DCIN_UV_STAT			BIT(4)
#define IRQSTAT_E_DCIN_UV_IRQ			BIT(5)
#define IRQSTAT_E_DCIN_OV_STAT			BIT(6)
#define IRQSTAT_E_DCIN_OV_IRQ			BIT(7)
/*
 * In SMB349 the DCIN UV status bits are reversed in the
 * IRQSTAT_E register, the DCIN_UV IRQ & Status correspond
 * to bits 1 & 0 respectively.
 */
#define SMB349_IRQSTAT_E_DCIN_UV_STAT		BIT(0)
#define SMB349_IRQSTAT_E_DCIN_UV_IRQ		BIT(1)
#define SMB349_IRQSTAT_E_DCIN_OV_STAT		BIT(2)
#define SMB349_IRQSTAT_E_DCIN_OV_IRQ		BIT(3)

#define IRQSTAT_F				0x3a
#define IRQSTAT_F_OTG_OC_IRQ			BIT(7)
#define IRQSTAT_F_OTG_OC_STAT			BIT(6)
#define IRQSTAT_F_OTG_UV_IRQ			BIT(5)
#define IRQSTAT_F_OTG_UV_STAT			BIT(4)
#define IRQSTAT_F_OTG_DET_IRQ			BIT(3)
#define IRQSTAT_F_OTG_DET_STAT			BIT(2)
#define IRQSTAT_F_PWR_OK_IRQ			BIT(1)
#define IRQSTAT_F_PWR_OK_STAT			BIT(0)

/* Status registers */
#define STAT_A					0x3b
#define STAT_A_FLOAT_VOLTAGE_MASK		0x3f

#define STAT_B					0x3c
#define STAT_B_RID_GROUND			0x10

#define STAT_C					0x3d
#define STAT_C_CHG_ENABLED			BIT(0)
#define STAT_C_HOLDOFF_STAT			BIT(3)
#define STAT_C_CHG_MASK				0x06
#define STAT_C_CHG_SHIFT			1
#define STAT_C_CHG_TERM				BIT(5)
#define STAT_C_CHARGER_ERROR			BIT(6)

#define STAT_D					0x3e
#define SMB347_STAT_D_CHG_TYPE_MASK		7

#define STAT_E					0x3f
#define SMB349_STAT_E_SUSPENDED		BIT(7)
#define SMB349_ACTUAL_COMPENSATED_CURRENT	BIT(4)
#define SMB349_ACTUAL_COMPENSATED_CURRENT_MASK	0x10
#define SMB349_ACTUAL_COMPENSATED_CURRENT_SHIFT	4
#define SMB349_COMPENSATED_CURRENT_500		500
#define SMB349_COMPENSATED_CURRENT_1000		1000

#define SMB347_MAX_REGISTER			0x3f

#define ITERM_100				100000		/*uA */

#define SMB349_CHRG_TYPE_ACA_DOCK		BIT(7)
#define SMB349_CHRG_TYPE_ACA_C			BIT(6)
#define SMB349_CHRG_TYPE_ACA_B			BIT(5)
#define SMB349_CHRG_TYPE_ACA_A			BIT(4)
#define SMB349_CHRG_TYPE_CDP			BIT(3)
#define SMB349_CHRG_TYPE_DCP			BIT(2)
#define SMB349_CHRG_TYPE_SDP			BIT(1)
#define SMB349_CHRG_TYPE_UNKNOWN		BIT(0)

#define SMB347_CHG_TYPE_CDP			1
#define SMB347_CHG_TYPE_DCP			2
#define SMB347_CHG_TYPE_UNKNOWN		3
#define SMB347_CHG_TYPE_SDP			4
#define SMB347_CHG_TYPE_ACA			5
#define SMB347_CHG_TYPE_OTHER_1		6
#define SMB347_CHG_TYPE_OTHER_2		7

#define SMB347_ACA_MASK			7
#define SMB347_ACA_SHIFT			4
#define SMB347_ACA_A				0
#define SMB347_ACA_B				1
#define SMB347_ACA_C				2

#define SMB_CHRG_TYPE_ACA_DOCK		SMB349_CHRG_TYPE_ACA_DOCK
#define SMB_CHRG_TYPE_ACA_C			SMB349_CHRG_TYPE_ACA_C
#define SMB_CHRG_TYPE_ACA_B			SMB349_CHRG_TYPE_ACA_B
#define SMB_CHRG_TYPE_ACA_A			SMB349_CHRG_TYPE_ACA_A
#define SMB_CHRG_TYPE_CDP			SMB349_CHRG_TYPE_CDP
#define SMB_CHRG_TYPE_DCP			SMB349_CHRG_TYPE_DCP
#define SMB_CHRG_TYPE_SDP			SMB349_CHRG_TYPE_SDP
#define SMB_CHRG_TYPE_UNKNOWN			SMB349_CHRG_TYPE_UNKNOWN


#ifdef CONFIG_POWER_SUPPLY_CHARGER
#define SMB_CHRG_CUR_DCP		1800
#define SMB_CHRG_CUR_ACA		1800
#define SMB_CHRG_CUR_CDP		1500
#define SMB_CHRG_CUR_SDP		500

#define SMB34X_FULL_WORK_JIFFIES	(30*HZ)
#endif

#define SMB34X_TEMP_WORK_JIFFIES	(30*HZ)

#define RETRY_READ			3

struct smb347_otg_event {
	struct list_head	node;
	bool			param;
};

#define SMB34X_EXTCON_SDP	"CHARGER_USB_SDP"
#define SMB34X_EXTCON_DCP	"CHARGER_USB_DCP"
#define SMB34X_EXTCON_CDP	"CHARGER_USB_CDP"

enum {
	EXTCON_CABLE_SDP,
	EXTCON_CABLE_DCP,
	EXTCON_CABLE_CDP,
};

static const char *smb34x_extcon_cable[] = {
	SMB34X_EXTCON_SDP,
	SMB34X_EXTCON_DCP,
	SMB34X_EXTCON_CDP,
	NULL,
};

static const int smb34x_extcon_muex[] = {0x7, 0};

#ifdef CONFIG_ACPI

#define SMB347_CHRG_CUR_NOLIMIT	1800
#define SMB347_CHRG_CUR_MEDIUM		1400
#define SMB347_CHRG_CUR_LOW		1000

static struct power_supply_throttle smb347_throttle_states[] = {
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = SMB347_CHRG_CUR_NOLIMIT,
	},
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = SMB347_CHRG_CUR_MEDIUM,
	},
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = SMB347_CHRG_CUR_LOW,
	},
	{
		.throttle_action = PSY_THROTTLE_DISABLE_CHARGING,
	},
};

static char *smb347_supplied_to[] = {
	"max170xx_battery",
	"max17042_battery",
	"max17047_battery",
};

static struct smb347_charger_platform_data smb347_pdata = {
	.battery_info			= { 0, },
	.soft_cold_temp_limit		= SMB347_TEMP_USE_DEFAULT,
	.soft_hot_temp_limit		= SMB347_TEMP_USE_DEFAULT,
	.hard_cold_temp_limit		= SMB347_TEMP_USE_DEFAULT,
	.hard_hot_temp_limit		= SMB347_TEMP_USE_DEFAULT,
	.soft_temp_limit_compensation	= SMB347_SOFT_TEMP_COMPENSATE_DEFAULT,
	.use_mains			= false,
	.use_usb			= true,
	.show_battery			= false,
	.enable_control			= SMB347_CHG_ENABLE_SW,
	.otg_control			= SMB347_OTG_CONTROL_SW,
#ifdef CONFIG_POWER_SUPPLY_CHARGER
	.supplied_to			= smb347_supplied_to,
	.num_supplicants		= ARRAY_SIZE(smb347_supplied_to),
	.throttle_states		= smb347_throttle_states,
	.num_throttle_states		= ARRAY_SIZE(smb347_throttle_states),
	.supported_cables		= POWER_SUPPLY_CHARGER_TYPE_USB,
#endif
	.detect_chg			= true,
	.reload_cfg			= true,
	.override_por			= true,
	.char_config_regs		= {
						/* Reg  Value */
						0x00, 0x46,
						0x01, 0x65,
						0x02, 0x87,
						0x03, 0xED,
						0x04, 0x38,
						0x05, 0x05,
						0x06, 0x06,
						0x07, 0x85,
						0x09, 0x8F,
						0x0A, 0x87,
						0x0B, 0x95,
						0x0C, 0xBF,
						0x0D, 0xF4,
						0x0E, 0xA0,
						0x10, 0x66,
						0x31, 0x01,
						0xFF, 0xFF
					},
};
#endif

/**
 * struct smb347_charger - smb347 charger instance
 * @lock: protects concurrent access to online variables
 * @dev: pointer to device
 * @regmap: pointer to driver regmap
 * @mains: power_supply instance for AC/DC power
 * @usb: power_supply instance for USB power
 * @battery: power_supply instance for battery
 * @mains_online: is AC/DC input connected
 * @usb_online: is USB input connected
 * @charging_enabled: is charging enabled
 * @pdata: pointer to platform data
 */
struct smb347_charger {
	struct mutex		lock;
	struct i2c_client	*client;
	struct device		*dev;
	struct regmap		*regmap;
	struct power_supply	mains;
	struct power_supply	usb;
	struct power_supply	battery;
	struct delayed_work	temp_upd_worker;
	bool			mains_online;
	bool			usb_online;
	bool			charging_enabled;
	bool			running;
	bool			drive_vbus;
	bool			a_bus_enable;
	struct dentry		*dentry;
	struct usb_phy		*otg;
	struct notifier_block	otg_nb;
	struct work_struct	otg_work;
	struct list_head	otg_queue;
	spinlock_t		otg_queue_lock;
	bool			otg_enabled;
	bool			otg_battery_uv;
	bool			is_disabled;
	const struct smb347_charger_platform_data	*pdata;
	struct extcon_dev	*edev;
	/* power supply properties */
	enum power_supply_charger_cable_type cable_type;
	int			inlmt;
	int			cc;
	int			cv;
	int			max_cc;
	int			max_cv;
	int			max_temp;
	int			min_temp;
	int			iterm;
	int			cntl_state;
	int			online;
	int			present;
#ifdef CONFIG_POWER_SUPPLY_CHARGER
	struct delayed_work	full_worker;
#endif
	struct wake_lock	wakelock;
	bool			is_smb349;
};

static struct smb347_charger *smb347_dev;

static int sm347_reload_config(struct smb347_charger *smb);
static bool smb347_is_charger_present(struct smb347_charger *smb);
static int smb347_set_writable(struct smb347_charger *smb, bool writable);


/* Fast charge current in uA */
static const unsigned int smb347_fcc_tbl[] = {
	700000,
	900000,
	1200000,
	1500000,
	1800000,
	2000000,
	2200000,
	2500000,
};


/* Pre-charge current in uA */
static const unsigned int smb347_pcc_tbl[] = {
	100000,
	150000,
	200000,
	250000,
};

/* Termination current in uA */
static const unsigned int smb347_tc_tbl[] = {
	37500,
	50000,
	100000,
	150000,
	200000,
	250000,
	500000,
	600000,
};

/* Input current limit in uA */
static const unsigned int smb347_icl_tbl[] = {
	300000,
	500000,
	700000,
	900000,
	1200000,
	1500000,
	1800000,
	2000000,
	2200000,
	2500000,
};

/* Charge current compensation in uA */
static const unsigned int smb347_ccc_tbl[] = {
	250000,
	700000,
	900000,
	1200000,
};

static const unsigned int smb349_icl_tbl[] = { /* uA */
	500000, 900000, 1000000, 1100000, 1200000, 1300000, 1500000, 1600000,
	1700000, 1800000, 2000000, 2200000, 2400000, 2500000, 3000000, 3500000,
};

static const unsigned int smb349_fcc_tbl[] = {
	1000000, 1200000, 1400000, 1600000, 1800000, 2000000, 2200000,
	2400000, 2600000, 2800000, 3000000, 3400000, 3600000, 3800000,
	4000000,
};

static const unsigned int smb349_pcc_tbl[] = {
	200000, 300000, 400000, 500000, 600000, 700000, 100000,
};

static const unsigned int smb349_tc_tbl[] = {
	200000, 300000, 400000, 500000, 600000, 700000, 100000,
};

static const unsigned int smb349_ccc_tbl[] = {
	500000, 100000,
};

static void smb34x_get_charger_uv_ov_irq_mask(struct smb347_charger *smb,
				int *uv_mask, int *ov_mask)
{
	if (smb->is_smb349) {
		*uv_mask = SMB349_IRQSTAT_E_DCIN_UV_IRQ;
		*ov_mask = SMB349_IRQSTAT_E_DCIN_OV_IRQ;
	} else {
		if (smb->pdata->use_mains) {
			*uv_mask = IRQSTAT_E_DCIN_UV_IRQ;
			*ov_mask = IRQSTAT_E_DCIN_OV_IRQ;
		} else {
			*uv_mask = IRQSTAT_E_USBIN_UV_IRQ;
			*ov_mask = IRQSTAT_E_USBIN_OV_IRQ;
		}
	}
}

static void smb34x_get_charger_uv_ov_stat_mask(struct smb347_charger *smb,
				int *uv_mask, int *ov_mask)
{
	if (smb->is_smb349) {
		*uv_mask = SMB349_IRQSTAT_E_DCIN_UV_STAT;
		*ov_mask = SMB349_IRQSTAT_E_DCIN_OV_STAT;
	} else {
		if (smb->pdata->use_mains) {
			*uv_mask = IRQSTAT_E_DCIN_UV_STAT;
			*ov_mask = IRQSTAT_E_DCIN_OV_STAT;
		} else {
			*uv_mask = IRQSTAT_E_USBIN_UV_STAT;
			*ov_mask = IRQSTAT_E_USBIN_OV_STAT;
		}
	}
}

static int smb347_read(struct smb347_charger *smb, u8 reg, u32 *data)
{
	int ret, i;

	for (i = 0; i < RETRY_READ; i++) {
		ret = regmap_read(smb->regmap, reg, data);
		if (ret < 0) {
			dev_warn(&smb->client->dev,
				"failed to read reg 0x%x: %d\n", reg, ret);
			/*
			 * during auto reload attempting a register read
			 * may fail, retry
			 */
			usleep_range(500, 1000);
			continue;
		} else
			break;
	}
	return ret;
}

static int smb347_write(struct smb347_charger *smb, u8 reg, u32 val)
{
	int ret;

	ret = regmap_write(smb->regmap, reg, val);
	if (ret < 0) {
		dev_warn(&smb->client->dev,
		"failed to write reg 0x%x: %d\n", reg, ret);
	}
	return ret;
}

static bool __maybe_unused smb347_is_suspended(struct smb347_charger *smb)
{
	int ret;
	unsigned int val;

	ret = smb347_read(smb, STAT_E, &val);
	if (ret < 0) {
		dev_warn(&smb->client->dev, "i2c failed %d", ret);
		return false;
	}
	return (val & SMB349_STAT_E_SUSPENDED) == SMB349_STAT_E_SUSPENDED;
}

/* Convert register value to current using lookup table */
static int hw_to_current(const unsigned int *tbl, size_t size, unsigned int val)
{
	if (val >= size)
		return -EINVAL;
	return tbl[val];
}

/* Convert current to register value using lookup table */
static int current_to_hw(const unsigned int *tbl, size_t size, unsigned int val)
{
	size_t i;

	for (i = 0; i < size; i++)
		if (val < tbl[i])
			break;
	return i > 0 ? i - 1 : -EINVAL;
}


static inline int smb347_force_fcc(struct smb347_charger *smb)
{
	int ret;
	u32 val;

	ret = smb347_read(smb, CMD_A, &val);
	if (ret < 0)
		return ret;

	val |= CMD_A_FORCE_FCC;

	return smb347_write(smb, CMD_A, val);
}

static int smb34x_get_health(struct smb347_charger *smb)
{
	int stat_e = 0, usb, ret;
	int chrg_health;

	if (smb->pdata->detect_chg)
		usb = !smb->is_disabled && smb347_is_charger_present(smb);
	else
		usb = !smb->is_disabled;

	ret = smb347_read(smb, IRQSTAT_E, &stat_e);
	if (ret < 0) {
		dev_warn(&smb->client->dev, "i2c failed %d", stat_e);
		chrg_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		goto end;
	}

	if (usb) {
		int uv_mask, ov_mask;
		smb34x_get_charger_uv_ov_stat_mask(smb, &uv_mask, &ov_mask);
		/* charger present && charger not disabled */
		if (stat_e & uv_mask)
			chrg_health = POWER_SUPPLY_HEALTH_DEAD;
		else if (stat_e & ov_mask)
			chrg_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			chrg_health = POWER_SUPPLY_HEALTH_GOOD;
	} else {
			chrg_health = POWER_SUPPLY_HEALTH_UNKNOWN;
	}
end:
	return chrg_health;
}

/**
 * smb347_update_ps_status - refreshes the power source status
 * @smb: pointer to smb347 charger instance
 *
 * Function checks whether any power source is connected to the charger and
 * updates internal state accordingly. If there is a change to previous state
 * function returns %1, otherwise %0 and negative errno in case of errror.
 */
static int smb347_update_ps_status(struct smb347_charger *smb)
{
	bool usb = false;
	bool dc = false;
	unsigned int val;
	int ret;
	int uv_mask, ov_mask;

	ret = regmap_read(smb->regmap, IRQSTAT_E, &val);
	if (ret < 0)
		return ret;

	smb34x_get_charger_uv_ov_stat_mask(smb, &uv_mask, &ov_mask);
	/*
	 * Dc and usb are set depending on whether they are enabled in
	 * platform data _and_ whether corresponding undervoltage is set.
	 */
	if (smb->pdata->use_mains)
		dc = !(val & uv_mask);
	if (smb->pdata->use_usb)
		usb = !(val & (uv_mask | ov_mask)) && !smb->is_disabled;

	mutex_lock(&smb->lock);
	ret = smb->mains_online != dc || smb->usb_online != usb;
	smb->mains_online = dc;
	smb->usb_online = usb;
	mutex_unlock(&smb->lock);

	return ret;
}

/*
 * smb347_is_ps_online - returns whether input power source is connected
 * @smb: pointer to smb347 charger instance
 *
 * Returns %true if input power source is connected. Note that this is
 * dependent on what platform has configured for usable power sources. For
 * example if USB is disabled, this will return %false even if the USB cable
 * is connected.
 */
static bool smb347_is_ps_online(struct smb347_charger *smb)
{
	bool ret;

	mutex_lock(&smb->lock);
	ret = (smb->usb_online || smb->mains_online) && !smb->otg_enabled;
	mutex_unlock(&smb->lock);

	return ret;
}

/**
 * smb347_charging_status - returns status of charging
 * @smb: pointer to smb347 charger instance
 *
 * Function returns charging status. %0 means no charging is in progress,
 * %1 means pre-charging, %2 fast-charging and %3 taper-charging.
 */
static int smb347_charging_status(struct smb347_charger *smb)
{
	unsigned int val;
	int ret;

	if (!smb347_is_ps_online(smb))
		return 0;

	ret = regmap_read(smb->regmap, STAT_C, &val);
	if (ret < 0)
		return 0;

	return (val & STAT_C_CHG_MASK) >> STAT_C_CHG_SHIFT;
}

static void smb347_enable_termination(struct smb347_charger *smb,
						bool enable)
{
	int ret;
	unsigned int data;

	mutex_lock(&smb->lock);
	ret = smb347_set_writable(smb, true);
	if (ret < 0) {
		dev_warn(&smb->client->dev, "i2c error %d", ret);
		mutex_unlock(&smb->lock);
		return;
	}

	ret = regmap_read(smb->regmap, CFG_STATUS_IRQ, &data);
	if (ret < 0) {
		dev_warn(&smb->client->dev, "i2c error %d", ret);
		goto err_term;
	}

	if (enable)
		data |= CFG_STATUS_IRQ_TERMINATION_OR_TAPER;
	else
		data &= ~CFG_STATUS_IRQ_TERMINATION_OR_TAPER;

	ret = regmap_write(smb->regmap, CFG_STATUS_IRQ, data);

	if (ret < 0)
		dev_warn(&smb->client->dev, "i2c error %d", ret);

err_term:
	ret = smb347_set_writable(smb, false);
	if (ret < 0)
		dev_warn(&smb->client->dev, "i2c error %d", ret);
	mutex_unlock(&smb->lock);
}

static bool smb347_is_charger_present(struct smb347_charger *smb)
{
	int ret;
	unsigned int chg_type;

	if (smb->pdata->detect_chg) {
		ret = regmap_read(smb->regmap, STAT_D, &chg_type);

		if (ret < 0)
			return 0;
		if (smb->is_smb349)
			return (chg_type <= 0) ? 0 : 1;
		else {
			if (chg_type == 7 || chg_type == 6 ||
				chg_type == 0)
				return 0;
			else
				return 1;
		}
	}
	return 1;
}

static int smb347_set_otg_reg_ctrl(struct smb347_charger *smb)
{
	int ret;
	unsigned int val;

	if (smb->pdata->detect_chg) {
		mutex_lock(&smb->lock);
		smb347_set_writable(smb, true);
		ret = smb347_read(smb, CFG_OTHER, &val);
		if (ret < 0)
			goto err_reg_ctrl;
		val |= CFG_OTHER_RID_ENABLED_OTG_I2C;
		ret = smb347_write(smb, CFG_OTHER, val);
		smb347_set_writable(smb, false);
		mutex_unlock(&smb->lock);
	} else {
		return 0;
	}

err_reg_ctrl:
	mutex_unlock(&smb->lock);
	return ret;


}

static int smb347_charging_set(struct smb347_charger *smb, bool enable)
{
	int ret = 0;

	if (smb->pdata->enable_control != SMB347_CHG_ENABLE_SW) {
		dev_dbg(smb->dev, "charging enable/disable in SW disabled\n");
		return 0;
	}

	mutex_lock(&smb->lock);
	if (smb->charging_enabled != enable) {
		ret = regmap_update_bits(smb->regmap, CMD_A,
					CMD_A_CHG_ENABLED|CMD_A_FORCE_FCC,
					 enable ? CMD_A_CHG_ENABLED : 0);
		if (!ret)
			smb->charging_enabled = enable;
	}
	mutex_unlock(&smb->lock);
	return ret;
}

static inline int smb347_charging_enable(struct smb347_charger *smb)
{
	return smb347_charging_set(smb, true);
}

static inline int smb347_charging_disable(struct smb347_charger *smb)
{
	return smb347_charging_set(smb, false);
}


static int smb347_enable_suspend(struct smb347_charger *smb)
{
	int ret;
	unsigned int val;

	if (!smb)
		return -EINVAL;

	mutex_lock(&smb->lock);
	ret = smb347_read(smb, CMD_A, &val);
	if (ret < 0)
		goto en_sus_err;

	val |= CMD_A_SUSPEND_ENABLED;

	ret = smb347_write(smb, CMD_A, val);
	if (ret < 0)
		goto en_sus_err;

	mutex_unlock(&smb->lock);

	return 0;

en_sus_err:
	dev_info(&smb->client->dev, "i2c error %d", ret);
	mutex_unlock(&smb->lock);
	return ret;
}

static int smb347_disable_suspend(struct smb347_charger *smb)
{
	int ret;
	unsigned int val;

	if (!smb)
		return -EINVAL;

	mutex_lock(&smb->lock);
	ret = smb347_read(smb, CMD_A, &val);
	if (ret < 0)
		goto dis_sus_err;

	val &= ~CMD_A_SUSPEND_ENABLED;

	ret = smb347_write(smb, CMD_A, val);
	if (ret < 0)
		goto dis_sus_err;

	mutex_unlock(&smb->lock);

	return 0;

dis_sus_err:
	dev_info(&smb->client->dev, "i2c error %d", ret);
	mutex_unlock(&smb->lock);
	return ret;
}

/*
 * smb347_enable_charger:
 *	Update the status variable, for device active state
 */
static int smb347_enable_charger(void)
{
	struct smb347_charger *smb = smb347_dev;
	int ret;

	ret = smb347_disable_suspend(smb);
	if (ret < 0)
		return ret;

	mutex_lock(&smb->lock);
	smb->is_disabled = false;
	mutex_unlock(&smb->lock);

	return 0;
}

/*
 * smb347_disable_charger:
 *	Update the status variable, for device active state
 */
static int smb347_disable_charger(void)
{
	struct smb347_charger *smb = smb347_dev;
	int ret;

	ret = smb347_enable_suspend(smb);
	if (ret < 0)
		return ret;

	mutex_lock(&smb->lock);
	smb->is_disabled = true;
	mutex_unlock(&smb->lock);

	return 0;
}

static int smb347_start_stop_charging(struct smb347_charger *smb)
{
	int ret;

	/*
	 * Depending on whether valid power source is connected or not, we
	 * disable or enable the charging. We do it manually because it
	 * depends on how the platform has configured the valid inputs.
	 */
	if (smb347_is_ps_online(smb)) {
		ret = smb347_charging_enable(smb);
		if (ret < 0)
			dev_err(smb->dev, "failed to enable charging\n");
	} else {
		ret = smb347_charging_disable(smb);
		if (ret < 0)
			dev_err(smb->dev, "failed to disable charging\n");
	}

	return ret;
}

static int smb347_otg_set(struct smb347_charger *smb, bool enable)
{
	const struct smb347_charger_platform_data *pdata = smb->pdata;
	int ret;
	unsigned int val;

	mutex_lock(&smb->lock);

	if (pdata->otg_control == SMB347_OTG_CONTROL_SW) {
		ret = smb347_read(smb, CMD_A, &val);
		if (ret < 0)
			goto out;

		if (enable)
			val |= CMD_A_OTG_ENABLED;
		else
			val &= ~CMD_A_OTG_ENABLED;

		ret = smb347_write(smb, CMD_A, val);
		if (ret < 0)
			goto out;
	} else {
		/*
		 * Switch to pin control or auto-OTG depending on how
		 * platform has configured.
		 */
		smb347_set_writable(smb, true);

		ret = smb347_read(smb, CFG_OTHER, &val);
		if (ret < 0) {
			smb347_set_writable(smb, false);
			goto out;
		}

		val &= ~CFG_OTHER_RID_MASK;

		switch (pdata->otg_control) {
		case SMB347_OTG_CONTROL_SW_PIN:
			if (enable) {
				val |= CFG_OTHER_RID_DISABLED_OTG_PIN;
				val |= CFG_OTHER_OTG_PIN_ACTIVE_LOW;
			} else {
				val &= ~CFG_OTHER_OTG_PIN_ACTIVE_LOW;
			}
			break;

		case SMB347_OTG_CONTROL_SW_AUTO:
			if (enable)
				val |= CFG_OTHER_RID_ENABLED_AUTO_OTG;
			break;

		default:
			dev_err(&smb->client->dev,
				"impossible OTG control configuration: %d\n",
				pdata->otg_control);
			break;
		}

		ret = smb347_write(smb, CFG_OTHER, val);
		smb347_set_writable(smb, false);
		if (ret < 0)
			goto out;
	}

	smb->otg_enabled = enable;

out:
	mutex_unlock(&smb->lock);
	return ret;
}

static inline int smb347_otg_enable(struct smb347_charger *smb)
{
	return smb347_otg_set(smb, true);
}

static inline int smb347_otg_disable(struct smb347_charger *smb)
{
	return smb347_otg_set(smb, false);
}

static void smb347_otg_drive_vbus(struct smb347_charger *smb, bool enable)
{
	if (enable == smb->otg_enabled)
		return;

	if (enable) {
		if (smb->otg_battery_uv) {
			dev_dbg(&smb->client->dev,
				"battery low voltage, won't enable OTG VBUS\n");
			return;
		}

		/*
		 * Normal charging must be disabled first before we try to
		 * enable OTG VBUS.
		 */
		smb347_charging_disable(smb);
		smb347_otg_enable(smb);

		if (smb->pdata->use_mains)
			power_supply_changed(&smb->mains);
		if (smb->pdata->use_usb)
			power_supply_changed(&smb->usb);

		dev_dbg(&smb->client->dev, "OTG VBUS on\n");
	} else {
		smb347_otg_disable(smb);
		/*
		 * Only re-enable charging if we have some power supply
		 * connected.
		 */
		if (smb347_is_ps_online(smb)) {
			smb347_charging_enable(smb);
			smb->otg_battery_uv = false;
			if (smb->pdata->use_mains)
				power_supply_changed(&smb->mains);
			if (smb->pdata->use_usb)
				power_supply_changed(&smb->usb);
		}

		dev_dbg(&smb->client->dev, "OTG VBUS off\n");
	}
}

#ifdef CONFIG_POWER_SUPPLY_CHARGER
static void smb347_full_worker(struct work_struct *work)
{
	struct smb347_charger *smb =
		container_of(work, struct smb347_charger, full_worker.work);

	power_supply_changed(NULL);

	schedule_delayed_work(&smb->full_worker,
				SMB34X_FULL_WORK_JIFFIES);
}
#endif


static void smb347_otg_detect(struct smb347_charger *smb)
{
	int ret;
	unsigned int val;

	/* No RID ground status in SMB347 */
	if (!smb->is_smb349)
		return;

	ret = smb347_read(smb, STAT_B, &val);

	if (ret < 0)
		dev_err(&smb->client->dev, "i2c error %d", ret);
	else if (val & STAT_B_RID_GROUND) {
		smb->drive_vbus = true;
		if (smb->a_bus_enable) {
			smb347_otg_enable(smb);
			atomic_notifier_call_chain(&smb->otg->notifier,
					USB_EVENT_ID, NULL);
		}
	} else {
		smb->drive_vbus = false;
		smb347_otg_disable(smb);
		atomic_notifier_call_chain(&smb->otg->notifier,
					USB_EVENT_NONE, NULL);
	}
}

static int smb34x_get_charger_type(struct smb347_charger *smb, u32 val)
{
	if (smb->is_smb349) {
		return val;
	} else {
		int aca_type;
		aca_type = (val >> SMB347_ACA_SHIFT) & SMB347_ACA_MASK;
		val &= 0x3;

		switch (val) {
		case SMB347_CHG_TYPE_CDP:
			return SMB_CHRG_TYPE_CDP;
		case SMB347_CHG_TYPE_DCP:
			return SMB_CHRG_TYPE_DCP;
		case SMB347_CHG_TYPE_SDP:
			return SMB_CHRG_TYPE_SDP;
		case SMB347_CHG_TYPE_ACA:
			switch (aca_type) {
			case SMB347_ACA_A:
				return SMB_CHRG_TYPE_ACA_A;
			case SMB347_ACA_B:
				return SMB_CHRG_TYPE_ACA_B;
			case SMB347_ACA_C:
				return SMB_CHRG_TYPE_ACA_C;
			default:
				return SMB_CHRG_TYPE_ACA_A;
			}
		case SMB347_CHG_TYPE_OTHER_1:
		case SMB347_CHG_TYPE_OTHER_2:
		case SMB347_CHG_TYPE_UNKNOWN:
		default:
			return SMB_CHRG_TYPE_UNKNOWN;
		}
	}
	return SMB_CHRG_TYPE_UNKNOWN;
}

static void smb34x_update_charger_type(struct smb347_charger *smb)
{
	static struct power_supply_cable_props cable_props;
	static int notify_chrg, notify_usb;
	int ret, evt = USB_EVENT_NONE;
	unsigned int val, power_ok;
	int uv_mask, ov_mask, edev_cable = EXTCON_CABLE_SDP;

	ret = smb347_read(smb, STAT_D, &val);
	if (ret < 0) {
		dev_err(&smb->client->dev, "%s:i2c read error", __func__);
		return;
	}

	dev_info(&smb->client->dev, "charger type %x\n", val);
	smb34x_get_charger_uv_ov_stat_mask(smb, &uv_mask, &ov_mask);
	/*
	 * sometimes, charger type is present on removal,
	 * check the UV status and decide disconnect
	 */
	ret = smb347_read(smb, IRQSTAT_E, &power_ok);
	if ((power_ok & (uv_mask | ov_mask)) && ret != 0) {
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_DISCONNECT;
		cable_props.ma = 0;
		evt = USB_EVENT_NONE;
		goto notify_usb_psy;
	}

	ret = smb34x_get_charger_type(smb, val);

	switch (ret) {
	case SMB_CHRG_TYPE_ACA_DOCK:
	case SMB_CHRG_TYPE_ACA_C:
	case SMB_CHRG_TYPE_ACA_B:
	case SMB_CHRG_TYPE_ACA_A:
		cable_props.chrg_evt =
			POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type =
			POWER_SUPPLY_CHARGER_TYPE_USB_ACA;
		cable_props.ma = SMB_CHRG_CUR_ACA;
		notify_chrg = 1;
		evt = USB_EVENT_VBUS;
		edev_cable = EXTCON_CABLE_DCP;
		break;

	case SMB_CHRG_TYPE_CDP:
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_CDP;
		cable_props.ma = SMB_CHRG_CUR_CDP;
		notify_chrg = 1;
		notify_usb = 1;
		evt = USB_EVENT_VBUS;
		edev_cable = EXTCON_CABLE_CDP;
		break;

	case SMB_CHRG_TYPE_SDP:
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_SDP;
		cable_props.ma = SMB_CHRG_CUR_SDP;
		notify_usb = 1;
		notify_chrg = 1;
		evt = USB_EVENT_VBUS;
		edev_cable = EXTCON_CABLE_SDP;
		break;

	case SMB_CHRG_TYPE_DCP:
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_DCP;
		cable_props.ma = SMB_CHRG_CUR_DCP;
		notify_chrg = 1;
		evt = USB_EVENT_VBUS;
		edev_cable = EXTCON_CABLE_DCP;
		break;

	default:
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_DISCONNECT;
		cable_props.ma = 0;
		evt = USB_EVENT_NONE;
		break;
	}

notify_usb_psy:
	dev_info(&smb->client->dev, "notify usb %d notify charger %d",
					notify_usb, notify_chrg);

	if (notify_usb)
		atomic_notifier_call_chain(&smb->otg->notifier, evt, NULL);

	if (evt == USB_EVENT_VBUS) {
		if (smb->pdata->reload_cfg)
			sm347_reload_config(smb);
		if (!wake_lock_active(&smb->wakelock))
			wake_lock(&smb->wakelock);
		if (smb->edev)
			extcon_set_cable_state(smb->edev,
				smb34x_extcon_cable[edev_cable], true);
	} else {
		if (wake_lock_active(&smb->wakelock))
			wake_unlock(&smb->wakelock);
		if (smb->edev)
			extcon_set_cable_state(smb->edev,
				smb34x_extcon_cable[edev_cable], false);
	}
#ifdef CONFIG_POWER_SUPPLY_CHARGER
	if (notify_chrg) {
		atomic_notifier_call_chain(&power_supply_notifier,
				PSY_CABLE_EVENT,
				&cable_props);
	}

	if (cable_props.chrg_evt == POWER_SUPPLY_CHARGER_EVENT_DISCONNECT) {
		notify_chrg = 0;
		notify_usb = 0;
	}
#endif

	return;
}

static void smb347_temp_upd_worker(struct work_struct *work)
{
	struct smb347_charger *smb =
		container_of(work, struct smb347_charger,
						temp_upd_worker.work);
	u32 stat_c, irqstat_e, irqstat_a;
	int ret, chg_status, ov_uv_stat, temp_stat;

	ret = smb347_read(smb, STAT_C, &stat_c);
	if (ret < 0)
		goto err_upd;

	ret = smb347_read(smb, IRQSTAT_E, &irqstat_e);
	if (ret < 0)
		goto err_upd;

	ret = smb347_read(smb, IRQSTAT_A, &irqstat_a);
	if (ret < 0)
		goto err_upd;

	chg_status = (stat_c & STAT_C_CHG_MASK) >> STAT_C_CHG_SHIFT;
	if (smb->is_smb349)
		ov_uv_stat = irqstat_e & (SMB349_IRQSTAT_E_DCIN_UV_STAT |
				SMB349_IRQSTAT_E_DCIN_OV_STAT);
	else
		ov_uv_stat = irqstat_e & (IRQSTAT_E_DCIN_UV_STAT |
				IRQSTAT_E_DCIN_OV_STAT);
	temp_stat = irqstat_a &
			(IRQSTAT_A_HOT_HARD_STAT|IRQSTAT_A_COLD_HARD_STAT);

	/* status = not charging, no uv, ov status, hard temp stat */
	if (!chg_status && !ov_uv_stat && temp_stat)
		power_supply_changed(&smb->usb);
	else
		return;
err_upd:
	schedule_delayed_work(&smb->temp_upd_worker,
						SMB34X_TEMP_WORK_JIFFIES);
}

static void smb347_otg_work(struct work_struct *work)
{
	struct smb347_charger *smb =
		container_of(work, struct smb347_charger, otg_work);
	struct smb347_otg_event *evt, *tmp;
	unsigned long flags;

	/* Process the whole event list in one go. */
	spin_lock_irqsave(&smb->otg_queue_lock, flags);
	list_for_each_entry_safe(evt, tmp, &smb->otg_queue, node) {
		list_del(&evt->node);
		spin_unlock_irqrestore(&smb->otg_queue_lock, flags);

		/* For now we only support set vbus events */
		smb347_otg_drive_vbus(smb, evt->param);
		kfree(evt);

		spin_lock_irqsave(&smb->otg_queue_lock, flags);
	}
	spin_unlock_irqrestore(&smb->otg_queue_lock, flags);
}

static int smb347_otg_notifier(struct notifier_block *nb, unsigned long event,
			       void *param)
{
	struct smb347_charger *smb =
		container_of(nb, struct smb347_charger, otg_nb);
	struct smb347_otg_event *evt;

	dev_dbg(&smb->client->dev, "OTG notification: %lu\n", event);
	if (!param || event != USB_EVENT_DRIVE_VBUS || !smb->running)
		return NOTIFY_DONE;

	evt = kzalloc(sizeof(*evt), GFP_ATOMIC);
	if (!evt) {
		dev_err(&smb->client->dev,
			"failed to allocate memory for OTG event\n");
		return NOTIFY_DONE;
	}

	evt->param = *(bool *)param;
	INIT_LIST_HEAD(&evt->node);

	spin_lock(&smb->otg_queue_lock);
	list_add_tail(&evt->node, &smb->otg_queue);
	spin_unlock(&smb->otg_queue_lock);

	queue_work(system_nrt_wq, &smb->otg_work);
	return NOTIFY_OK;
}

static int sm347_reload_config(struct smb347_charger *smb)
{
	int ret, i, loop_count;
	int reg_offset = 0;

	if (!smb->pdata->reload_cfg && !smb->pdata->override_por)
		return 0;

	mutex_lock(&smb->lock);
	ret = smb347_set_writable(smb, true);
	if (ret < 0)
		goto err_reload;

	loop_count = MAXSMB347_CONFIG_DATA_SIZE / 2;

	/*
	 * Program the platform specific configuration values to the device
	 */
	for (i = 0; (i < loop_count) &&
		(smb->pdata->char_config_regs[reg_offset] != 0xff); i++) {
		regmap_write(smb->regmap,
			smb->pdata->char_config_regs[reg_offset],
			smb->pdata->char_config_regs[reg_offset+1]);
		reg_offset += 2;
	}

	smb347_write(smb, CMD_B, CMD_B_MODE_HC);
	ret = smb347_set_writable(smb, false);

err_reload:
	mutex_unlock(&smb->lock);
	return ret;
}


static void smb347_usb_otg_enable(struct usb_phy *phy)
{
	struct smb347_charger *smb = smb347_dev;

	if (!smb)
		return;

	if (phy->vbus_state == VBUS_DISABLED) {
		dev_info(&smb->client->dev, "OTG Disable");
		smb->a_bus_enable = false;
		if (smb->drive_vbus) {
			atomic_notifier_call_chain(&smb->otg->notifier,
					USB_EVENT_NONE, NULL);
			smb347_otg_disable(smb);
		}
	} else {
		dev_info(&smb->client->dev, "OTG Enable");
		smb->a_bus_enable = true;
		if (smb->drive_vbus) {
			smb347_otg_enable(smb);
			atomic_notifier_call_chain(&smb->otg->notifier,
					USB_EVENT_ID, NULL);
		}
	}
}

static void smb347_hw_uninit(struct smb347_charger *smb)
{
	if (smb->otg) {
		struct smb347_otg_event *evt, *tmp;

		usb_unregister_notifier(smb->otg, &smb->otg_nb);
		smb347_otg_disable(smb);
		usb_put_phy(smb->otg);

		/* Clear all the queued events. */
		flush_work_sync(&smb->otg_work);
		list_for_each_entry_safe(evt, tmp, &smb->otg_queue, node) {
			list_del(&evt->node);
			kfree(evt);
		}
	}
}

/*
 * smb347_set_cc	: set max charge current
 * @smb		: pointer to chip private data
 * @cc			: charge current in uA
 */
static int smb347_set_cc(struct smb347_charger *smb, int cc)
{
	int ret;
	unsigned int reg, mask, shift;


	mutex_lock(&smb->lock);
	ret = smb347_set_writable(smb, true);
	if (ret < 0)
		goto err_cc;

	/* uA */
	if (smb->is_smb349)
		ret = current_to_hw(smb349_fcc_tbl,
				ARRAY_SIZE(smb349_fcc_tbl), cc * 1000);
	else
		ret = current_to_hw(smb347_fcc_tbl,
				ARRAY_SIZE(smb347_fcc_tbl), cc * 1000);

	if (ret < 0)
		goto err_cc;

	reg = CFG_CHARGE_CURRENT;
	if (smb->is_smb349) {
		mask = SMB349_CFG_CHARGE_CURRENT_FCC_MASK;
		shift = SMB349_CFG_CHARGE_CURRENT_FCC_SHIFT;
	} else {
		mask = CFG_CHARGE_CURRENT_FCC_MASK;
		shift = CFG_CHARGE_CURRENT_FCC_SHIFT;
	}

	ret = regmap_update_bits(smb->regmap, reg, mask,
				 ret << shift);
	if (ret < 0)
		goto err_cc;

	ret = smb347_set_writable(smb, false);
	mutex_unlock(&smb->lock);
	return 0;

err_cc:
	ret = smb347_set_writable(smb, false);
	mutex_unlock(&smb->lock);
	dev_info(&smb->client->dev, "%s:error writing to i2c", __func__);
	return ret;
}

/*
 * smb347_set_inlmt	: set charge current limit
 * @smb		: pointer to chip private data
 * @inlmt		: current limit in uA
 */
static int smb347_set_inlmt(struct smb347_charger *smb, int inlmt)
{
	int ret;
	unsigned int reg;

	mutex_lock(&smb->lock);
	ret = smb347_set_writable(smb, true);
	if (ret < 0)
		goto err_inlmt;

	/* uA */
	if (smb->is_smb349)
		ret = current_to_hw(smb349_icl_tbl,
				ARRAY_SIZE(smb349_icl_tbl), inlmt * 1000);
	else
		ret = current_to_hw(smb347_icl_tbl,
				ARRAY_SIZE(smb347_icl_tbl), inlmt * 1000);
	if (ret < 0)
		goto err_inlmt;

	if (smb->is_smb349)
		reg = CFG_CHARGE_CURRENT;
	else
		reg = CFG_CURRENT_LIMIT;

	ret = regmap_update_bits(smb->regmap, reg,
				 CFG_CURRENT_LIMIT_USB_MASK, ret);
	if (ret < 0)
		goto err_inlmt;

	ret = smb347_set_writable(smb, false);
	mutex_unlock(&smb->lock);
	return 0;

err_inlmt:
	ret = smb347_set_writable(smb, false);
	mutex_unlock(&smb->lock);
	dev_info(&smb->client->dev, "%s:error writing to i2c", __func__);
	return ret;
}

/*
 * smb347_set_cv	: set max charge voltage
 * @smb		: pointer to chip private data
 * @cv			: charge voltage in mV
 */
static int smb347_set_cv(struct smb347_charger *smb, int cv)
{
	int ret;
	unsigned int volt_min, volt_max;

	mutex_lock(&smb->lock);
	ret = smb347_set_writable(smb, true);
	if (ret < 0)
		goto err_cv;

	if (smb->is_smb349) {
		volt_min = 3460000;
		volt_max = 4720000;
	} else {
		volt_min = 3500000;
		volt_max = 4500000;
	}

	/* uV */
	ret = clamp_val(cv * 1000, volt_min, volt_max) - volt_min;
	ret /= 20000;

	ret = regmap_update_bits(smb->regmap, CFG_FLOAT_VOLTAGE,
				 CFG_FLOAT_VOLTAGE_FLOAT_MASK, ret);
	if (ret < 0)
		goto err_cv;

	ret = smb347_set_writable(smb, false);
	mutex_unlock(&smb->lock);
	return 0;

err_cv:
	ret = smb347_set_writable(smb, false);
	mutex_unlock(&smb->lock);
	dev_info(&smb->client->dev, "%s:error writing to i2c", __func__);
	return ret;
}

/*
 * smb347_set_iterm	: set battery charge termination current
 * @smb		: pointer to chip private data
 * @iterm		: termination current in mA
 */
static int smb347_set_iterm(struct smb347_charger *smb, int iterm)
{
	int ret;
	unsigned int reg, mask, shift;

	mutex_lock(&smb->lock);
	ret = smb347_set_writable(smb, true);
	if (ret < 0)
		goto err_iterm;
	if (smb->is_smb349)
		ret = current_to_hw(smb349_tc_tbl,
				ARRAY_SIZE(smb349_tc_tbl), iterm * 1000);
	else
		ret = current_to_hw(smb347_tc_tbl,
				ARRAY_SIZE(smb347_tc_tbl), iterm * 1000);

	if (ret < 0)
		goto err_iterm;

	if (smb->is_smb349) {
		reg = CFG_CURRENT_LIMIT;
		mask = SMB349_CFG_CHARGE_CURRENT_TC_MASK;
		shift = SMB349_CFG_CHARGE_CURRENT_TC_SHIFT;
	} else {
		reg = CFG_CHARGE_CURRENT;
		mask = CFG_CHARGE_CURRENT_TC_MASK;
		shift = CFG_CHARGE_CURRENT_TC_SHIFT;
	}

	ret = regmap_update_bits(smb->regmap, reg, mask, ret << shift);
	if (ret < 0)
		goto err_iterm;

	ret = smb347_set_writable(smb, false);
	mutex_unlock(&smb->lock);
	return 0;

err_iterm:
	ret = smb347_set_writable(smb, false);
	dev_info(&smb->client->dev, "%s:error writing to i2c", __func__);
	mutex_unlock(&smb->lock);
	return ret;
}

static int smb347_set_charge_current(struct smb347_charger *smb)
{
	int ret;
	int reg, mask, shift;

	if (smb->pdata->max_charge_current) {
		ret = smb347_set_cc(smb, smb->pdata->max_charge_current/1000);
		if (ret < 0)
			return ret;
	}

	if (smb->pdata->pre_charge_current) {
		if (smb->is_smb349)
			ret = current_to_hw(smb349_pcc_tbl,
					ARRAY_SIZE(smb349_pcc_tbl),
					smb->pdata->pre_charge_current);
		else
			ret = current_to_hw(smb347_pcc_tbl,
					ARRAY_SIZE(smb347_pcc_tbl),
					smb->pdata->pre_charge_current);
		if (ret < 0)
			return ret;

		if (smb->is_smb349) {
			reg = CFG_CURRENT_LIMIT;
			mask = SMB349_CFG_CHARGE_CURRENT_PCC_MASK;
			shift = SMB349_CFG_CHARGE_CURRENT_PCC_SHIFT;
		} else {
			reg = CFG_CHARGE_CURRENT;
			mask = CFG_CHARGE_CURRENT_PCC_MASK;
			shift = CFG_CHARGE_CURRENT_PCC_SHIFT;
		}
		ret = regmap_update_bits(smb->regmap, reg, mask, ret << shift);
		if (ret < 0)
			return ret;
	}

	if (smb->pdata->termination_current) {
		ret = smb347_set_iterm(smb,
					smb->pdata->termination_current / 1000);

		if (ret < 0)
			return ret;
	}

	return 0;
}

static int smb347_set_current_limits(struct smb347_charger *smb)
{
	int ret;

	if (smb->pdata->mains_current_limit) {
		if (smb->is_smb349)
			ret = current_to_hw(smb349_icl_tbl,
					ARRAY_SIZE(smb349_icl_tbl),
					smb->pdata->mains_current_limit);
		else
			ret = current_to_hw(smb347_icl_tbl,
					ARRAY_SIZE(smb347_icl_tbl),
					smb->pdata->mains_current_limit);
		if (ret < 0)
			return ret;

		if (!smb->is_smb349) {
			ret = regmap_update_bits(smb->regmap,
				CFG_CURRENT_LIMIT, CFG_CURRENT_LIMIT_DC_MASK,
					 ret << CFG_CURRENT_LIMIT_DC_SHIFT);
			if (ret < 0)
				return ret;
		}
	}

	if (smb->pdata->usb_hc_current_limit) {
		ret = smb347_set_inlmt(smb,
				smb->pdata->usb_hc_current_limit / 1000);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int smb347_set_voltage_limits(struct smb347_charger *smb)
{
	int ret;

	if (smb->pdata->pre_to_fast_voltage) {
		ret = smb->pdata->pre_to_fast_voltage;

		/* uV */
		ret = clamp_val(ret, 2400000, 3000000) - 2400000;
		ret /= 200000;

		ret = regmap_update_bits(smb->regmap, CFG_FLOAT_VOLTAGE,
				CFG_FLOAT_VOLTAGE_THRESHOLD_MASK,
				ret << CFG_FLOAT_VOLTAGE_THRESHOLD_SHIFT);
		if (ret < 0)
			return ret;
	}

	if (smb->pdata->max_charge_voltage) {
		ret = smb347_set_cv(smb, smb->pdata->max_charge_voltage / 1000);

		if (ret < 0)
			return ret;
	}

	return 0;
}

static int smb347_set_temp_limits(struct smb347_charger *smb)
{
	bool enable_therm_monitor = false;
	int ret = 0;
	int val;
	int reg, mask, shift;

	if (smb->pdata->chip_temp_threshold) {
		val = smb->pdata->chip_temp_threshold;

		/* degree C */
		val = clamp_val(val, 100, 130) - 100;
		val /= 10;

		ret = regmap_update_bits(smb->regmap, CFG_OTG,
					 CFG_OTG_TEMP_THRESHOLD_MASK,
					 val << CFG_OTG_TEMP_THRESHOLD_SHIFT);
		if (ret < 0)
			return ret;
	}

	if (smb->pdata->soft_cold_temp_limit != SMB347_TEMP_USE_DEFAULT) {
		val = smb->pdata->soft_cold_temp_limit;

		val = clamp_val(val, 0, 15);
		val /= 5;
		/* this goes from higher to lower so invert the value */
		val = ~val & 0x3;

		ret = regmap_update_bits(smb->regmap, CFG_TEMP_LIMIT,
					 CFG_TEMP_LIMIT_SOFT_COLD_MASK,
					 val << CFG_TEMP_LIMIT_SOFT_COLD_SHIFT);
		if (ret < 0)
			return ret;

		enable_therm_monitor = true;
	}

	if (smb->pdata->soft_hot_temp_limit != SMB347_TEMP_USE_DEFAULT) {
		val = smb->pdata->soft_hot_temp_limit;

		val = clamp_val(val, 40, 55) - 40;
		val /= 5;

		ret = regmap_update_bits(smb->regmap, CFG_TEMP_LIMIT,
					 CFG_TEMP_LIMIT_SOFT_HOT_MASK,
					 val << CFG_TEMP_LIMIT_SOFT_HOT_SHIFT);
		if (ret < 0)
			return ret;

		enable_therm_monitor = true;
	}

	if (smb->pdata->hard_cold_temp_limit != SMB347_TEMP_USE_DEFAULT) {
		val = smb->pdata->hard_cold_temp_limit;

		val = clamp_val(val, -5, 10) + 5;
		val /= 5;
		/* this goes from higher to lower so invert the value */
		val = ~val & 0x3;

		ret = regmap_update_bits(smb->regmap, CFG_TEMP_LIMIT,
					 CFG_TEMP_LIMIT_HARD_COLD_MASK,
					 val << CFG_TEMP_LIMIT_HARD_COLD_SHIFT);
		if (ret < 0)
			return ret;

		enable_therm_monitor = true;
	}

	if (smb->pdata->hard_hot_temp_limit != SMB347_TEMP_USE_DEFAULT) {
		val = smb->pdata->hard_hot_temp_limit;

		val = clamp_val(val, 50, 65) - 50;
		val /= 5;

		ret = regmap_update_bits(smb->regmap, CFG_TEMP_LIMIT,
					 CFG_TEMP_LIMIT_HARD_HOT_MASK,
					 val << CFG_TEMP_LIMIT_HARD_HOT_SHIFT);
		if (ret < 0)
			return ret;

		enable_therm_monitor = true;
	}

	/*
	 * If any of the temperature limits are set, we also enable the
	 * thermistor monitoring.
	 *
	 * When soft limits are hit, the device will start to compensate
	 * current and/or voltage depending on the configuration.
	 *
	 * When hard limit is hit, the device will suspend charging
	 * depending on the configuration.
	 */
	if (enable_therm_monitor) {
		ret = regmap_update_bits(smb->regmap, CFG_THERM,
					 CFG_THERM_MONITOR_DISABLED, 0);
		if (ret < 0)
			return ret;
	}

	if (smb->pdata->suspend_on_hard_temp_limit) {
		ret = regmap_update_bits(smb->regmap,
			(smb->is_smb349) ? SMB349_CFG_SYSOK : SMB347_CFG_SYSOK,
				 CFG_SYSOK_SUSPEND_HARD_LIMIT_DISABLED, 0);
		if (ret < 0)
			return ret;
	}

	if (smb->pdata->soft_temp_limit_compensation !=
	    SMB347_SOFT_TEMP_COMPENSATE_DEFAULT) {
		val = smb->pdata->soft_temp_limit_compensation & 0x3;

		ret = regmap_update_bits(smb->regmap, CFG_THERM,
				 CFG_THERM_SOFT_HOT_COMPENSATION_MASK,
				 val << CFG_THERM_SOFT_HOT_COMPENSATION_SHIFT);
		if (ret < 0)
			return ret;

		ret = regmap_update_bits(smb->regmap, CFG_THERM,
				 CFG_THERM_SOFT_COLD_COMPENSATION_MASK,
				 val << CFG_THERM_SOFT_COLD_COMPENSATION_SHIFT);
		if (ret < 0)
			return ret;
	}

	if (smb->pdata->charge_current_compensation) {
		if (smb->is_smb349) {
			reg = SMB349_CFG_SYSOK;
			mask = CFG_SYSOK_CC_COMPENSATION_MASK;
			shift = CFG_SYSOK_CC_COMPENSATION_SHIFT;
			val = current_to_hw(smb349_ccc_tbl,
				ARRAY_SIZE(smb349_ccc_tbl),
				smb->pdata->charge_current_compensation);
		} else {
			reg = CFG_OTG;
			mask = CFG_OTG_CC_COMPENSATION_MASK;
			shift = CFG_OTG_CC_COMPENSATION_SHIFT;
			val = current_to_hw(smb347_ccc_tbl,
				ARRAY_SIZE(smb347_ccc_tbl),
				smb->pdata->charge_current_compensation);
		}
		if (val < 0)
			return val;
		ret = regmap_update_bits(smb->regmap, reg, mask,
				(val & 0x3) << shift);
		if (ret < 0)
			return ret;
	}

	return ret;
}

/*
 * smb347_set_writable - enables/disables writing to non-volatile registers
 * @smb: pointer to smb347 charger instance
 *
 * You can enable/disable writing to the non-volatile configuration
 * registers by calling this function.
 *
 * Returns %0 on success and negative errno in case of failure.
 */
static int smb347_set_writable(struct smb347_charger *smb, bool writable)
{
	int i, ret;

	for (i = 0; i < RETRY_READ; i++) {
		ret = regmap_update_bits(smb->regmap, CMD_A, CMD_A_ALLOW_WRITE,
				  writable ? CMD_A_ALLOW_WRITE : 0);
		if (ret < 0) {
			usleep_range(500, 1000);
			continue;
		} else
			break;
	}
	return ret;
}

static int smb347_hw_init(struct smb347_charger *smb)
{
	unsigned int val;
	int ret;

	ret = smb347_set_writable(smb, true);
	if (ret < 0)
		return ret;

	/*
	 * Program the platform specific configuration values to the device
	 * first.
	 */
	ret = smb347_set_charge_current(smb);
	if (ret < 0)
		goto fail;

	ret = smb347_set_current_limits(smb);
	if (ret < 0)
		goto fail;

	ret = smb347_set_voltage_limits(smb);
	if (ret < 0)
		goto fail;

	ret = smb347_set_temp_limits(smb);
	if (ret < 0)
		goto fail;

	if (smb->pdata->override_por) {
		int i, loop_count, reg_offset = 0;
		loop_count = MAXSMB347_CONFIG_DATA_SIZE / 2;
		for (i = 0; (i < loop_count) &&
		(smb->pdata->char_config_regs[reg_offset] != 0xff); i++) {
			regmap_write(smb->regmap,
				smb->pdata->char_config_regs[reg_offset],
				smb->pdata->char_config_regs[reg_offset+1]);
			reg_offset += 2;
		}
	}

	/* If USB charging is disabled we put the USB in suspend mode */
	if (!smb->pdata->use_usb) {
		ret = regmap_update_bits(smb->regmap, CMD_A,
					 CMD_A_SUSPEND_ENABLED,
					 CMD_A_SUSPEND_ENABLED);
		if (ret < 0)
			goto fail;
	}

	/* disable charging to recover from previous errors */
	ret = smb347_read(smb, CMD_A, &val);
	if (ret < 0)
		goto fail;

	val &= ~CMD_A_CHG_ENABLED;
	ret = smb347_write(smb, CMD_A, val);
	if (ret < 0)
		goto fail;


	switch (smb->pdata->otg_control) {
	case SMB347_OTG_CONTROL_DISABLED:
		break;

	case SMB347_OTG_CONTROL_SW:
	case SMB347_OTG_CONTROL_SW_PIN:
	case SMB347_OTG_CONTROL_SW_AUTO:
		smb->otg = usb_get_phy(USB_PHY_TYPE_USB2);
		if (smb->otg) {
			INIT_WORK(&smb->otg_work, smb347_otg_work);
			INIT_LIST_HEAD(&smb->otg_queue);
			spin_lock_init(&smb->otg_queue_lock);

			smb->a_bus_enable = true;
			smb->otg->a_bus_drop = smb347_usb_otg_enable;
			smb->otg_nb.notifier_call = smb347_otg_notifier;
			ret = usb_register_notifier(smb->otg, &smb->otg_nb);
			if (ret < 0) {
				usb_put_phy(smb->otg);
				smb->otg = NULL;
				goto fail;
			}

			dev_info(&smb->client->dev,
				"registered to OTG notifications\n");
		}
		break;
	case SMB347_OTG_CONTROL_PIN:
	case SMB347_OTG_CONTROL_AUTO:
		/*
		 * If configured by platform data, we enable hardware Auto-OTG
		 * support for driving VBUS. Otherwise we disable it.
		 */
		ret = regmap_update_bits(smb->regmap, CFG_OTHER,
					CFG_OTHER_RID_MASK,
		smb->pdata->use_usb_otg ? CFG_OTHER_RID_ENABLED_AUTO_OTG : 0);

		break;
	}

	if (ret < 0)
		goto fail;

	/*
	 * Make the charging functionality controllable by a write to the
	 * command register unless pin control is specified in the platform
	 * data.
	 */
	switch (smb->pdata->enable_control) {
	case SMB347_CHG_ENABLE_PIN_ACTIVE_LOW:
		val = CFG_PIN_EN_CTRL_ACTIVE_LOW;
		break;
	case SMB347_CHG_ENABLE_PIN_ACTIVE_HIGH:
		val = CFG_PIN_EN_CTRL_ACTIVE_HIGH;
		break;
	default:
		val = 0;
		break;
	}

	ret = regmap_update_bits(smb->regmap, CFG_PIN, CFG_PIN_EN_CTRL_MASK,
				 val);
	if (ret < 0)
		goto fail;

	if (!smb->pdata->detect_chg) {
		/* Disable Automatic Power Source Detection (APSD) interrupt. */
		ret = regmap_update_bits(smb->regmap, CFG_PIN,
						CFG_PIN_EN_APSD_IRQ, 0);
		if (ret < 0)
			goto fail;
	}

	ret = smb347_update_ps_status(smb);
	if (ret < 0)
		goto fail;

#ifndef CONFIG_POWER_SUPPLY_CHARGER
	ret = smb347_start_stop_charging(smb);
#endif
	smb347_set_writable(smb, false);
	return ret;

fail:
	if (smb->otg) {
		usb_unregister_notifier(smb->otg, &smb->otg_nb);
		usb_put_phy(smb->otg);
		smb->otg = NULL;
	}
	smb347_set_writable(smb, false);
	return ret;
}

static irqreturn_t smb347_interrupt(int irq, void *data)
{
	struct smb347_charger *smb = data;
	unsigned int stat_c, irqstat_a, irqstat_b, irqstat_c, irqstat_d;
	unsigned int irqstat_e, irqstat_f;
	bool handled = false;
	int ret, uv_mask, ov_mask;

	ret = smb347_read(smb, STAT_C, &stat_c);
	if (ret < 0) {
		dev_warn(smb->dev, "reading STAT_C failed\n");
		return IRQ_NONE;
	}

	ret = smb347_read(smb, IRQSTAT_A, &irqstat_a);
	if (ret < 0) {
		dev_warn(smb->dev, "reading IRQSTAT_C failed\n");
		return IRQ_NONE;
	}

	ret = smb347_read(smb, IRQSTAT_B, &irqstat_b);
	if (ret < 0) {
		dev_warn(smb->dev, "reading IRQSTAT_C failed\n");
		return IRQ_NONE;
	}

	ret = smb347_read(smb, IRQSTAT_C, &irqstat_c);
	if (ret < 0) {
		dev_warn(smb->dev, "reading IRQSTAT_C failed\n");
		return IRQ_NONE;
	}

	ret = smb347_read(smb, IRQSTAT_D, &irqstat_d);
	if (ret < 0) {
		dev_warn(smb->dev, "reading IRQSTAT_D failed\n");
		return IRQ_NONE;
	}

	ret = smb347_read(smb, IRQSTAT_E, &irqstat_e);
	if (ret < 0) {
		dev_warn(smb->dev, "reading IRQSTAT_E failed\n");
		return IRQ_NONE;
	}

	ret = smb347_read(smb, IRQSTAT_F, &irqstat_f);
	if (ret < 0) {
		dev_warn(&smb->client->dev, "reading IRQSTAT_F failed\n");
		return IRQ_NONE;
	}

	/*
	 * If we get charger error we report the error back to user.
	 * If the error is recovered charging will resume again.
	 */
	if (stat_c & STAT_C_CHARGER_ERROR) {
		dev_err(&smb->client->dev,
			"charging stopped due to charger error\n");
		if (smb->pdata->use_usb) {
			smb->usb_online = 0;
			power_supply_changed(&smb->usb);
		}
		if (smb->pdata->show_battery)
			power_supply_changed(&smb->battery);
		handled = true;
	}

	if (irqstat_a & (IRQSTAT_A_HOT_HARD_IRQ|IRQSTAT_A_COLD_HARD_IRQ)) {
		dev_info(&smb->client->dev, "extreme temperature interrupt");
		schedule_delayed_work(&smb->temp_upd_worker, 0);
		handled = true;
	}

	if (irqstat_b & IRQSTAT_B_BATOVP_IRQ) {
		dev_info(&smb->client->dev, "BATOVP interrupt");
		/* Reset charging in case of battery OV */
		smb347_charging_set(smb, false);
		if (smb->pdata->use_usb)
			power_supply_changed(&smb->usb);
		handled = true;
	}
	/*
	 * If we reached the termination current the battery is charged and
	 * we can update the status now. Charging is automatically
	 * disabled by the hardware.
	 */
#ifndef CONFIG_POWER_SUPPLY_CHARGER
	if (irqstat_c & (IRQSTAT_C_TERMINATION_IRQ | IRQSTAT_C_TAPER_IRQ)) {
		if ((irqstat_c & IRQSTAT_C_TERMINATION_STAT) &&
						smb->pdata->show_battery)
			power_supply_changed(&smb->battery);
		dev_info(&smb->client->dev,
			"[Charge Terminated] Going to HW Maintenance mode\n");
		handled = true;
	}
#else
	if (irqstat_c & (IRQSTAT_C_TAPER_IRQ | IRQSTAT_C_TERMINATION_IRQ)) {
		if (smb->charging_enabled) {
			/*
			 * reduce the termination current value, to avoid
			 * repeated interrupts.
			 */
			smb347_set_iterm(smb, 100);
			smb347_enable_termination(smb, false);
			/*
			 * since termination has happened, charging will not be
			 * re-enabled until, disable and enable charging is
			 * done.
			 */
			smb347_charging_set(smb, false);
			smb347_charging_set(smb, true);
			schedule_delayed_work(&smb->full_worker, 0);
		}
		handled = true;
	}
#endif

	/*
	 * If we got a charger timeout INT that means the charge
	 * full is not detected with in charge timeout value.
	 */
	if (irqstat_d & IRQSTAT_D_CHARGE_TIMEOUT_IRQ) {
		dev_dbg(smb->dev, "total Charge Timeout INT received\n");

		if (irqstat_d & IRQSTAT_D_CHARGE_TIMEOUT_STAT)
			dev_warn(smb->dev, "charging stopped due to timeout\n");
		/* Restart charging once timeout has happened */
		smb347_charging_set(smb, false);
		smb347_charging_set(smb, true);
		if (smb->pdata->show_battery)
			power_supply_changed(&smb->battery);
		handled = true;
	}

	if (smb->pdata->detect_chg) {
		if (irqstat_d & IRQSTAT_D_APSD_STAT) {
			smb347_disable_suspend(smb);
			ret = IRQ_HANDLED;
		}
	}
	/*
	 * If we got an under voltage interrupt it means that AC/USB input
	 * was connected or disconnected.
	 */
	smb34x_get_charger_uv_ov_irq_mask(smb, &uv_mask, &ov_mask);
	if (irqstat_e & (uv_mask | ov_mask)) {
		if (smb347_update_ps_status(smb) > 0) {
			smb347_start_stop_charging(smb);
#ifndef CONFIG_POWER_SUPPLY_CHARGER
			if (smb->pdata->use_mains)
				power_supply_changed(&smb->mains);
			if (smb->pdata->use_usb)
				power_supply_changed(&smb->usb);
#endif
		}
		handled = true;
	}

	/*
	 * If the battery voltage falls below OTG UVLO the VBUS is
	 * automatically turned off but we must not enable it again unless
	 * UVLO is cleared. It will be cleared when external power supply
	 * is connected and the battery voltage goes over the UVLO
	 * threshold.
	 */
	if (irqstat_f & IRQSTAT_F_OTG_UV_IRQ) {
		smb->otg_battery_uv = !!(irqstat_f & IRQSTAT_F_OTG_UV_STAT);
		dev_info(&smb->client->dev, "Vbatt is below OTG UVLO\n");
		smb347_otg_disable(smb);
		handled = true;
	}

	if (irqstat_f & IRQSTAT_F_OTG_DET_IRQ) {
		smb347_otg_detect(smb);
		handled = true;
	}

	if (irqstat_f & IRQSTAT_F_PWR_OK_IRQ) {
		dev_info(&smb->client->dev, "PowerOK INTR recieved\n");

		smb347_update_ps_status(smb);

		if (smb->pdata->detect_chg)
			smb34x_update_charger_type(smb);

		ret = IRQ_HANDLED;
	}
	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static int smb347_irq_set(struct smb347_charger *smb, bool enable)
{
	int ret;
	u32 val;

	ret = smb347_set_writable(smb, true);
	if (ret < 0)
		return ret;

	/*
	 * Enable/disable interrupts for:
	 *	- under voltage
	 *	- termination current reached
	 *	- charger timeout
	 *	- charger error
	 */
	val = CFG_FAULT_IRQ_DCIN_UV;
	if (smb->otg)
		val |= CFG_FAULT_IRQ_OTG_UV;
	ret = regmap_update_bits(smb->regmap, CFG_FAULT_IRQ, 0xff,
				 enable ? val : 0);
	if (ret < 0)
		goto fail;

	val = (CFG_STATUS_IRQ_TERMINATION_OR_TAPER |
				CFG_STATUS_IRQ_INOK |
				CFG_STATUS_IRQ_CHARGE_TIMEOUT);
	if (smb->otg)
		val |= CFG_STATUS_OTG_DET;

	ret = regmap_update_bits(smb->regmap, CFG_STATUS_IRQ, 0xff,
			enable ?  val : 0);
	if (ret < 0)
		goto fail;

	val = CFG_PIN_EN_CHARGER_ERROR;
	if (smb->pdata->detect_chg)
		val |= CFG_PIN_EN_APSD_IRQ;

	ret = regmap_update_bits(smb->regmap, CFG_PIN,
		CFG_PIN_EN_CHARGER_ERROR | CFG_PIN_EN_APSD_IRQ,
		enable ? val : 0);
fail:
	smb347_set_writable(smb, false);
	return ret;
}

static inline int smb347_irq_enable(struct smb347_charger *smb)
{
	return smb347_irq_set(smb, true);
}

static inline int smb347_irq_disable(struct smb347_charger *smb)
{
	return smb347_irq_set(smb, false);
}

static int smb347_irq_init(struct smb347_charger *smb,
			   struct i2c_client *client)
{
	const struct smb347_charger_platform_data *pdata = smb->pdata;
	int ret, irq = client->irq;

	if (gpio_is_valid(pdata->irq_gpio))
		irq = gpio_to_irq(pdata->irq_gpio);

	ret = request_threaded_irq(irq, NULL, smb347_interrupt,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   client->name, smb);
	if (ret < 0)
		goto fail_gpio;

	mutex_lock(&smb->lock);
	ret = smb347_set_writable(smb, true);
	if (ret < 0)
		goto fail_irq;

	/*
	 * Configure the STAT output to be suitable for interrupts: disable
	 * all other output (except interrupts) and make it active low.
	 */
	ret = regmap_update_bits(smb->regmap, CFG_STAT,
				 CFG_STAT_ACTIVE_HIGH | CFG_STAT_DISABLED,
				 CFG_STAT_DISABLED);
	if (ret < 0)
		goto fail_readonly;

	ret = smb347_irq_enable(smb);
	if (ret < 0)
		goto fail_readonly;

	smb347_set_writable(smb, false);
	client->irq = irq;
	enable_irq_wake(smb->client->irq);
	mutex_unlock(&smb->lock);
	return 0;

fail_readonly:
	smb347_set_writable(smb, false);
fail_irq:
	mutex_unlock(&smb->lock);
	free_irq(irq, smb);
fail_gpio:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
	client->irq = 0;
	return ret;
}



/*
 * Returns the constant charge current programmed
 * into the charger in uA.
 */
static int get_const_charge_current(struct smb347_charger *smb)
{
	int ret, intval;
	unsigned int v;

	if (!smb347_is_ps_online(smb))
		return -ENODATA;

	if (smb->is_smb349) {
		ret = regmap_read(smb->regmap, STAT_E, &v);
		if (ret < 0)
			return ret;

		v &= SMB349_ACTUAL_COMPENSATED_CURRENT_MASK;
		v >>= SMB349_ACTUAL_COMPENSATED_CURRENT_SHIFT;
		intval = v ? SMB349_COMPENSATED_CURRENT_1000 :
				SMB349_COMPENSATED_CURRENT_500;
	} else {

		ret = regmap_read(smb->regmap, STAT_B, &v);
		if (ret < 0)
			return ret;

		/*
		 * The current value is composition of FCC and PCC values
		 * and we can detect which table to use from bit 5.
		 */
		if (v & 0x20) {
			intval = hw_to_current(smb347_fcc_tbl,
						ARRAY_SIZE(smb347_fcc_tbl),
						v & 7);
		} else {
			v >>= 3;
			intval = hw_to_current(smb347_pcc_tbl,
						ARRAY_SIZE(smb347_pcc_tbl),
						v & 7);
		}

	}
	return intval;
}

/*
 * Returns the constant charge voltage programmed
 * into the charger in uV.
 */
static int get_const_charge_voltage(struct smb347_charger *smb)
{
	int ret, intval;
	unsigned int v;

	if (!smb347_is_ps_online(smb))
		return -ENODATA;

	ret = regmap_read(smb->regmap, STAT_A, &v);
	if (ret < 0)
		return ret;

	v &= STAT_A_FLOAT_VOLTAGE_MASK;
	if (v > 0x3d)
		v = 0x3d;

	intval = 3500000 + v * 20000;

	return intval;
}

static int smb347_mains_get_property(struct power_supply *psy,
				     enum power_supply_property prop,
				     union power_supply_propval *val)
{
	struct smb347_charger *smb =
		container_of(psy, struct smb347_charger, mains);
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = smb->mains_online;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = get_const_charge_voltage(smb);
		if (ret < 0)
			return ret;
		else
			val->intval = ret;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = get_const_charge_current(smb);
		if (ret < 0)
			return ret;
		else
			val->intval = ret;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static enum power_supply_property smb347_mains_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
};

#ifdef CONFIG_POWER_SUPPLY_CHARGER
static enum power_supply_type power_supply_cable_type(
		enum power_supply_charger_cable_type cable)
{
	switch (cable) {

	case POWER_SUPPLY_CHARGER_TYPE_USB_DCP:
		return POWER_SUPPLY_TYPE_USB_DCP;
	case POWER_SUPPLY_CHARGER_TYPE_USB_CDP:
		return POWER_SUPPLY_TYPE_USB_CDP;
	case POWER_SUPPLY_CHARGER_TYPE_USB_ACA:
		return POWER_SUPPLY_TYPE_USB_ACA;
	case POWER_SUPPLY_CHARGER_TYPE_AC:
		return POWER_SUPPLY_TYPE_MAINS;
	case POWER_SUPPLY_CHARGER_TYPE_NONE:
	case POWER_SUPPLY_CHARGER_TYPE_USB_SDP:
	default:
		return POWER_SUPPLY_TYPE_USB;
	}

	return POWER_SUPPLY_TYPE_USB;
}
#endif


#ifndef CONFIG_POWER_SUPPLY_CHARGER
static int smb347_throttle_charging(struct smb347_charger *smb, int lim)
{
	struct power_supply_throttle *throttle_states =
					smb->pdata->throttle_states;
	int ret;

	if (lim < 0 || lim > (smb->pdata->num_throttle_states - 1))
		return -ERANGE;

	if (throttle_states[lim].throttle_action ==
				PSY_THROTTLE_CC_LIMIT) {
		ret = smb347_enable_charger();
		if (ret < 0)
			goto throttle_fail;
		ret = smb347_set_cc(smb, throttle_states[lim].throttle_val);
		if (ret < 0)
			goto throttle_fail;
		ret = smb347_charging_set(smb, true);
	} else if (throttle_states[lim].throttle_action ==
				PSY_THROTTLE_INPUT_LIMIT) {
		ret = smb347_enable_charger();
		if (ret < 0)
			goto throttle_fail;
		ret = smb347_set_inlmt(smb, throttle_states[lim].throttle_val);
		if (ret < 0)
			goto throttle_fail;
		ret = smb347_charging_set(smb, true);
	} else if (throttle_states[lim].throttle_action ==
				PSY_THROTTLE_DISABLE_CHARGING) {
		ret = smb347_enable_charger();
		if (ret < 0)
			goto throttle_fail;
		ret = smb347_charging_set(smb, false);
	} else if (throttle_states[lim].throttle_action ==
				PSY_THROTTLE_DISABLE_CHARGER) {
		ret = smb347_disable_charger();
	} else {
		return -EINVAL;
	}

throttle_fail:
	return ret;
}
#endif

static int smb347_usb_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct smb347_charger *smb =
		container_of(psy, struct smb347_charger, usb);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		smb->present = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		smb->online = val->intval;
		break;
	case POWER_SUPPLY_PROP_MAX_CHARGE_CURRENT:
		smb->max_cc = val->intval;
		break;
	case POWER_SUPPLY_PROP_MAX_CHARGE_VOLTAGE:
		smb->max_cv = val->intval;
		break;
	case POWER_SUPPLY_PROP_ENABLE_CHARGING:
		ret = smb347_charging_set(smb, (bool)val->intval);
		if (ret < 0)
			dev_err(&smb->client->dev,
				"Error %d in %s charging", ret,
				(val->intval ? "enable" : "disable"));
		if (!val->intval)
#ifdef CONFIG_POWER_SUPPLY_CHARGER
			cancel_delayed_work(&smb->full_worker);
#else
			;
#endif
		else {
			smb347_set_iterm(smb, smb->iterm);
			smb347_enable_termination(smb, true);
		}
		break;
	case POWER_SUPPLY_PROP_ENABLE_CHARGER:
		if (val->intval)
			ret = smb347_enable_charger();
		else {
			/*
			 * OTG cannot function if charger is put to
			 *  suspend state, send fake ntf in such case
			 */
			if (!smb->pdata->reload_cfg ||
				smb347_is_charger_present(smb))
				ret = smb347_disable_charger();
			else
				smb->is_disabled = true;
		}
		if (ret < 0)
			dev_err(&smb->client->dev,
				"Error %d in %s charger", ret,
				(val->intval ? "enable" : "disable"));
		/*
		 * Set OTG in Register control, as default HW
		 * settings did not enable OTG.
		 */
		smb347_set_otg_reg_ctrl(smb);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CURRENT:
		ret = smb347_set_cc(smb, val->intval);
		if (!ret) {
			mutex_lock(&smb->lock);
			smb->cc = val->intval;
			mutex_unlock(&smb->lock);
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_VOLTAGE:
		ret = smb347_set_cv(smb, val->intval);
		if (!ret) {
			mutex_lock(&smb->lock);
			smb->cv = val->intval;
			mutex_unlock(&smb->lock);
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CUR:
		ret = smb347_set_iterm(smb, val->intval);
		if (!ret) {
			mutex_lock(&smb->lock);
			smb->iterm = val->intval;
			mutex_unlock(&smb->lock);
		}
		break;
#ifdef CONFIG_POWER_SUPPLY_CHARGER
	case POWER_SUPPLY_PROP_CABLE_TYPE:
		mutex_lock(&smb->lock);
		smb->cable_type = val->intval;
		smb->usb.type = power_supply_cable_type(smb->cable_type);
		mutex_unlock(&smb->lock);
		break;
#endif
	case POWER_SUPPLY_PROP_INLMT:
		ret = smb347_set_inlmt(smb, val->intval);
		if (!ret) {
			mutex_lock(&smb->lock);
			smb->inlmt = val->intval;
			mutex_unlock(&smb->lock);
		}
		break;
	case POWER_SUPPLY_PROP_MAX_TEMP:
		mutex_lock(&smb->lock);
		smb->max_temp = val->intval;
		mutex_unlock(&smb->lock);
		break;
	case POWER_SUPPLY_PROP_MIN_TEMP:
		mutex_lock(&smb->lock);
		smb->min_temp = val->intval;
		mutex_unlock(&smb->lock);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
#ifdef CONFIG_POWER_SUPPLY_CHARGER
		if (val->intval < smb->pdata->num_throttle_states)
			smb->cntl_state = val->intval;
		else
			ret = -ERANGE;
#else
		if (val->intval < smb->pdata->num_throttle_states) {
			ret = smb347_throttle_charging(smb, val->intval);
			if (ret < 0)
				break;
			smb->cntl_state = val->intval;
		} else {
			ret = -ERANGE;
		}
#endif
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}


static int smb347_usb_get_property(struct power_supply *psy,
				   enum power_supply_property prop,
				   union power_supply_propval *val)
{
	struct smb347_charger *smb =
		container_of(psy, struct smb347_charger, usb);
	int ret = 0;

	mutex_lock(&smb->lock);
	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = smb->online;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb->present;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = smb34x_get_health(smb);
		break;
	case POWER_SUPPLY_PROP_MAX_CHARGE_CURRENT:
		val->intval = smb->max_cc;
		break;
	case POWER_SUPPLY_PROP_MAX_CHARGE_VOLTAGE:
		val->intval = smb->max_cv;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CURRENT:
		val->intval = smb->cc;
		break;
	case POWER_SUPPLY_PROP_CHARGE_VOLTAGE:
		val->intval = smb->cv;
		break;
	case POWER_SUPPLY_PROP_INLMT:
		val->intval = smb->inlmt;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CUR:
		val->intval = smb->iterm;
		break;
#ifdef CONFIG_POWER_SUPPLY_CHARGER
	case POWER_SUPPLY_PROP_CABLE_TYPE:
		val->intval = smb->cable_type;
#endif
		break;
	case POWER_SUPPLY_PROP_ENABLE_CHARGING:
		val->intval = smb->charging_enabled;
		break;
	case POWER_SUPPLY_PROP_ENABLE_CHARGER:
		val->intval = !smb->is_disabled;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		val->intval = smb->cntl_state;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		val->intval = smb->pdata->num_throttle_states;
		break;
	case POWER_SUPPLY_PROP_MAX_TEMP:
		val->intval = smb->max_temp;
		break;
	case POWER_SUPPLY_PROP_MIN_TEMP:
		val->intval = smb->min_temp;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		mutex_unlock(&smb->lock);
		ret = get_const_charge_current(smb);
		if (ret < 0)
			return ret;
		else
			val->intval = ret;
		mutex_lock(&smb->lock);
		break;

	default:
		mutex_unlock(&smb->lock);
		return -EINVAL;
	}
	mutex_unlock(&smb->lock);
	return ret;
}

static enum power_supply_property smb347_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_MAX_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_MAX_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_VOLTAGE,
#ifdef CONFIG_POWER_SUPPLY_CHARGER
	POWER_SUPPLY_PROP_INLMT,
	POWER_SUPPLY_PROP_ENABLE_CHARGING,
	POWER_SUPPLY_PROP_ENABLE_CHARGER,
	POWER_SUPPLY_PROP_CHARGE_TERM_CUR,
	POWER_SUPPLY_PROP_CABLE_TYPE,
#endif
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_MAX_TEMP,
	POWER_SUPPLY_PROP_MIN_TEMP,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
};

static int smb347_get_charging_status(struct smb347_charger *smb)
{
	int ret, status;
	unsigned int val;

	if (!smb347_is_ps_online(smb))
		return POWER_SUPPLY_STATUS_DISCHARGING;

	ret = regmap_read(smb->regmap, STAT_C, &val);
	if (ret < 0)
		return ret;

	if ((val & STAT_C_CHARGER_ERROR) ||
			(val & STAT_C_HOLDOFF_STAT)) {
		/*
		 * set to NOT CHARGING upon charger error
		 * or charging has stopped.
		 */
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	} else {
		if ((val & STAT_C_CHG_MASK) >> STAT_C_CHG_SHIFT) {
			/*
			 * set to charging if battery is in pre-charge,
			 * fast charge or taper charging mode.
			 */
			status = POWER_SUPPLY_STATUS_CHARGING;
		} else if (val & STAT_C_CHG_TERM) {
			/*
			 * set the status to FULL if battery is not in pre
			 * charge, fast charge or taper charging mode AND
			 * charging is terminated at least once.
			 */
			status = POWER_SUPPLY_STATUS_FULL;
		} else {
			/*
			 * in this case no charger error or termination
			 * occured but charging is not in progress!!!
			 */
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
	}

	return status;
}

static int smb347_battery_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smb347_charger *smb =
			container_of(psy, struct smb347_charger, battery);
	const struct smb347_charger_platform_data *pdata = smb->pdata;
	int ret;

	ret = smb347_update_ps_status(smb);
	if (ret < 0)
		return ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = smb347_get_charging_status(smb);
		if (ret < 0)
			return ret;
		val->intval = ret;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!smb347_is_ps_online(smb))
			return -ENODATA;

		/*
		 * We handle trickle and pre-charging the same, and taper
		 * and none the same.
		 */
		switch (smb347_charging_status(smb)) {
		case 1:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case 2:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;
		default:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		}
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = pdata->battery_info.technology;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = pdata->battery_info.voltage_min_design;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = pdata->battery_info.voltage_max_design;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = pdata->battery_info.charge_full_design;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = pdata->battery_info.name;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static enum power_supply_property smb347_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_MODEL_NAME,
};

static int smb347_debugfs_show(struct seq_file *s, void *data)
{
	struct smb347_charger *smb = s->private;
	int ret;
	unsigned int val;
	u8 reg, max_reg;

	if (smb->is_smb349)
		max_reg = SMB349_CFG_ADDRESS;
	else
		max_reg = SMB347_CFG_ADDRESS;

	seq_puts(s, "Control registers:\n");
	seq_puts(s, "==================\n");
	for (reg = CFG_CHARGE_CURRENT; reg <= max_reg; reg++) {
		ret = regmap_read(smb->regmap, reg, &val);
		if (ret < 0)
			return ret;
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, val);
	}
	seq_puts(s, "\n");

	seq_puts(s, "Command registers:\n");
	seq_puts(s, "==================\n");
	ret = regmap_read(smb->regmap, CMD_A, &val);
	if (ret < 0)
		return ret;
	seq_printf(s, "0x%02x:\t0x%02x\n", CMD_A, val);
	ret = regmap_read(smb->regmap, CMD_B, &val);
	if (ret < 0)
		return ret;
	seq_printf(s, "0x%02x:\t0x%02x\n", CMD_B, val);
	ret = regmap_read(smb->regmap, CMD_C, &val);
	if (ret < 0)
		return ret;
	seq_printf(s, "0x%02x:\t0x%02x\n", CMD_C, val);
	seq_puts(s, "\n");

	seq_puts(s, "Interrupt status registers:\n");
	seq_puts(s, "===========================\n");
	for (reg = IRQSTAT_A; reg <= IRQSTAT_F; reg++) {
		ret = regmap_read(smb->regmap, reg, &val);
		if (ret < 0)
			return ret;
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, val);
	}
	seq_puts(s, "\n");

	seq_puts(s, "Status registers:\n");
	seq_puts(s, "=================\n");
	for (reg = STAT_A; reg <= STAT_E; reg++) {
		ret = regmap_read(smb->regmap, reg, &val);
		if (ret < 0)
			return ret;
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, val);
	}

	return 0;
}

static int smb347_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, smb347_debugfs_show, inode->i_private);
}

static const struct file_operations smb347_debugfs_fops = {
	.open		= smb347_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static bool smb347_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case IRQSTAT_A:
	case IRQSTAT_B:
	case IRQSTAT_C:
	case IRQSTAT_D:
	case IRQSTAT_E:
	case IRQSTAT_F:
	case STAT_A:
	case STAT_B:
	case STAT_C:
	case STAT_D:
	case STAT_E:
		return true;
	}

	return false;
}

static bool smb347_readable_reg(struct device *dev, unsigned int reg)
{
	struct smb347_charger *smb = dev_get_drvdata(dev);
	switch (reg) {
	case CFG_CHARGE_CURRENT:
	case CFG_CURRENT_LIMIT:
	case CFG_FLOAT_VOLTAGE:
	case CFG_STAT:
	case CFG_PIN:
	case CFG_THERM:
	case SMB347_CFG_SYSOK:
	case CFG_OTHER:
	case CFG_OTG:
	case CFG_TEMP_LIMIT:
	case CFG_FAULT_IRQ:
	case CFG_STATUS_IRQ:
	case SMB347_CFG_ADDRESS:
	case CMD_A:
	case CMD_B:
	case CMD_C:
	case CHIP_ID:
		return true;
	}
	if (smb->is_smb349 && ((reg == SMB349_FLEXCHARGE) ||
		(reg == SMB349_STATUS_INT) || (reg == SMB349_CFG_ADDRESS)))
		return true;

	return smb347_volatile_reg(dev, reg);
}

static const struct regmap_config smb347_regmap = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= SMB347_MAX_REGISTER,
	.volatile_reg	= smb347_volatile_reg,
	.readable_reg	= smb347_readable_reg,
};

static int smb347_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	static char *battery[] = { "smb347-battery" };
	struct smb347_charger_platform_data *pdata;
	struct device *dev = &client->dev;
	struct smb347_charger *smb;
	int ret, data;
#ifdef CONFIG_ACPI
	struct gpio_desc *desc;

	if (!id) {
		dev_err(dev, "device id mismatch");
		return -EIO;
	}

	pdata = (struct smb347_charger_platform_data *)id->driver_data;
#else
	pdata = dev->platform_data;
#endif

	if (!pdata)
		return -EINVAL;

	if (!pdata->use_mains && !pdata->use_usb)
		return -EINVAL;

#ifdef CONFIG_ACPI
	desc = gpiod_get_index(dev, KBUILD_MODNAME, 0);
	if (IS_ERR(desc))
		pdata->irq_gpio = PTR_ERR(desc);
	else {
		pdata->irq_gpio = desc_to_gpio(desc);
		gpiod_put(desc);
	}
#endif
	smb = devm_kzalloc(dev, sizeof(*smb), GFP_KERNEL);
	if (!smb)
		return -ENOMEM;

	i2c_set_clientdata(client, smb);

	mutex_init(&smb->lock);
	smb->client = client;
	smb->dev = &client->dev;
	smb->pdata = pdata;

	smb->regmap = devm_regmap_init_i2c(client, &smb347_regmap);
	if (IS_ERR(smb->regmap))
		return PTR_ERR(smb->regmap);

	ret = regmap_read(smb->regmap, CHIP_ID, &data);
	if (ret < 0)
		return ret;

	if ((data & CHIP_ID_MASK) == CHIP_ID_SMB349)
		smb->is_smb349 = true;
	else
		smb->is_smb349 = false;

	dev_info(smb->dev, "Using SMB34%d", smb->is_smb349 ? 9 : 7);

	ret = smb347_hw_init(smb);
	if (ret < 0)
		return ret;

	wake_lock_init(&smb->wakelock, WAKE_LOCK_SUSPEND, "smb_wakelock");

	smb347_dev = smb;

#ifdef CONFIG_POWER_SUPPLY_CHARGER
	INIT_DELAYED_WORK(&smb->full_worker, smb347_full_worker);
#endif
	INIT_DELAYED_WORK(&smb->temp_upd_worker, smb347_temp_upd_worker);

	if (smb->pdata->use_mains) {
		smb->mains.name = "smb347-mains";
		smb->mains.type = POWER_SUPPLY_TYPE_MAINS;
		smb->mains.get_property = smb347_mains_get_property;
		smb->mains.properties = smb347_mains_properties;
		smb->mains.num_properties = ARRAY_SIZE(smb347_mains_properties);
		smb->mains.supplied_to = battery;
		smb->mains.num_supplicants = ARRAY_SIZE(battery);
		ret = power_supply_register(dev, &smb->mains);
		if (ret < 0)
			goto psy_reg1_failed;
	}

	if (smb->pdata->use_usb) {
		smb->usb.name = "smb34x-usb_charger";
		smb->usb.type = POWER_SUPPLY_TYPE_USB;
		smb->usb.get_property = smb347_usb_get_property;
		smb->usb.properties = smb347_usb_properties;
		smb->usb.num_properties = ARRAY_SIZE(smb347_usb_properties);
#ifdef CONFIG_POWER_SUPPLY_CHARGER
		smb->usb.supplied_to = pdata->supplied_to;
		smb->usb.num_supplicants = pdata->num_supplicants;
		smb->usb.throttle_states = pdata->throttle_states;
		smb->usb.num_throttle_states = pdata->num_throttle_states;
		smb->usb.supported_cables = pdata->supported_cables;
#endif
		smb->usb.set_property = smb347_usb_set_property;
		smb->max_cc = 1800;
		smb->max_cv = 4350;
		ret = power_supply_register(dev, &smb->usb);
		if (ret < 0)
			goto psy_reg2_failed;
	}

	if (smb->pdata->show_battery) {
		smb->battery.name = "smb347-battery";
		smb->battery.type = POWER_SUPPLY_TYPE_BATTERY;
		smb->battery.get_property = smb347_battery_get_property;
		smb->battery.properties = smb347_battery_properties;
		smb->battery.num_properties =
				ARRAY_SIZE(smb347_battery_properties);

		ret = power_supply_register(dev, &smb->battery);
		if (ret < 0)
			goto psy_reg3_failed;
	}

	if (smb->pdata->detect_chg) {
		/* register with extcon */
		smb->edev = devm_kzalloc(dev, sizeof(struct extcon_dev),
						GFP_KERNEL);
		if (!smb->edev) {
			dev_err(&client->dev, "mem alloc failed\n");
			ret = -ENOMEM;
			goto psy_reg4_failed;
		}
		smb->edev->name = "smb34x";
		smb->edev->supported_cable = smb34x_extcon_cable;
		smb->edev->mutually_exclusive = smb34x_extcon_muex;
		ret = extcon_dev_register(smb->edev);
		if (ret) {
			dev_err(&client->dev, "extcon registration failed!!\n");
			goto psy_reg4_failed;
		}
	}

	/*
	 * Interrupt pin is optional. If it is connected, we setup the
	 * interrupt support here.
	 */
	if (pdata->irq_gpio >= 0 || smb->client->irq > 0) {
		ret = smb347_irq_init(smb, client);
		if (ret < 0) {
			dev_warn(dev, "failed to initialize IRQ: %d\n", ret);
			dev_warn(dev, "disabling IRQ support\n");
			goto psy_irq_failed;
		} else {
			smb347_irq_enable(smb);
		}
	}

	if (smb->pdata->detect_chg)
		smb34x_update_charger_type(smb);

	smb347_otg_detect(smb);

	smb->running = true;
	smb->dentry = debugfs_create_file("smb347-regs", S_IRUSR, NULL, smb,
					  &smb347_debugfs_fops);
	return 0;

psy_irq_failed:
	if (smb->edev)
		extcon_dev_unregister(smb->edev);
psy_reg4_failed:
	if (smb->pdata->show_battery)
		power_supply_unregister(&smb->battery);
psy_reg3_failed:
	if (smb->pdata->use_usb)
		power_supply_unregister(&smb->usb);
psy_reg2_failed:
	if (smb->pdata->use_mains)
		power_supply_unregister(&smb->mains);
psy_reg1_failed:
	smb347_hw_uninit(smb);
	wake_lock_destroy(&smb->wakelock);
	smb347_dev = NULL;
	return ret;
}

static int smb347_remove(struct i2c_client *client)
{
	struct smb347_charger *smb = i2c_get_clientdata(client);

	if (!IS_ERR_OR_NULL(smb->dentry))
		debugfs_remove(smb->dentry);

	smb->running = false;

	if (client->irq) {
		smb347_irq_disable(smb);
		free_irq(client->irq, smb);
		if (gpio_is_valid(smb->pdata->irq_gpio))
			gpio_free(smb->pdata->irq_gpio);
	}

	smb347_hw_uninit(smb);
	if (smb->edev)
		extcon_dev_unregister(smb->edev);
	if (smb->pdata->show_battery)
		power_supply_unregister(&smb->battery);
	if (smb->pdata->use_usb)
		power_supply_unregister(&smb->usb);
	if (smb->pdata->use_mains)
		power_supply_unregister(&smb->mains);
	wake_lock_destroy(&smb->wakelock);

	return 0;
}

static void smb347_shutdown(struct i2c_client *client)
{
	struct smb347_charger *smb = i2c_get_clientdata(client);

	if (client->irq > 0)
		disable_irq(client->irq);

	if (smb->drive_vbus)
		smb347_otg_disable(smb);

	return;
}

static int smb347_suspend(struct device *dev)
{
	struct smb347_charger *smb = dev_get_drvdata(dev);

	if (smb->client->irq > 0)
		disable_irq(smb->client->irq);

	return 0;
}

static int smb347_resume(struct device *dev)
{
	struct smb347_charger *smb = dev_get_drvdata(dev);

	if (smb->client->irq > 0)
		enable_irq(smb->client->irq);

	return 0;
}

static const struct dev_pm_ops smb347_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(smb347_suspend, smb347_resume)
};

static const struct i2c_device_id smb347_id[] = {
	{ "smb347", (kernel_ulong_t) &smb347_pdata},
	{ "smb349", (kernel_ulong_t) &smb347_pdata},
	{ "SMB0347", (kernel_ulong_t) &smb347_pdata},
	{ "SMB0349", (kernel_ulong_t) &smb347_pdata},
	{ "SMB0349:00", (kernel_ulong_t) &smb347_pdata},
	{}
};
MODULE_DEVICE_TABLE(i2c, smb347_id);

#ifdef CONFIG_ACPI
static struct acpi_device_id smb349_acpi_match[] = {
	{"SMB0349", (kernel_ulong_t) &smb347_pdata},
	{}
};
MODULE_DEVICE_TABLE(acpi, smb349_acpi_match);

#endif


static struct i2c_driver smb347_driver = {
	.driver = {
		.name	= "smb347",
		.owner	= THIS_MODULE,
		.pm	= &smb347_pm_ops,
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(smb349_acpi_match),
#endif
	},
	.probe		= smb347_probe,
	.remove		= smb347_remove,
	.id_table	= smb347_id,
	.shutdown	= smb347_shutdown,
};

module_i2c_driver(smb347_driver);

MODULE_AUTHOR("Bruce E. Robertson <bruce.e.robertson@intel.com>");
MODULE_AUTHOR("Mika Westerberg <mika.westerberg@linux.intel.com>");
MODULE_DESCRIPTION("SMB347 battery charger driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("i2c:smb347");
