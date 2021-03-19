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

/* Status register definition */
#define INPUT_STATUS_REG		0x2609
#define INPUT_USB_IN			BIT(1)
#define INPUT_WLS_IN			BIT(0)

#define PERPH0_INT_RT_STS_REG		0x2610
#define USBIN_OVLO_STS			BIT(7)
#define WLSIN_OVLO_STS			BIT(6)
#define USBIN_UVLO_STS			BIT(5)
#define WLSIN_UVLO_STS			BIT(4)
#define DIV2_IREV_LATCH_STS		BIT(3)
#define VOL_UV_LATCH_STS		BIT(2)
#define TEMP_SHUTDOWN_STS		BIT(1)
#define CFLY_HARD_FAULT_LATCH_STS	BIT(0)

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
#define USBIN_OVLO			BIT(3)
#define WLSIN_OVLO			BIT(2)
#define USBIN_UVLO			BIT(1)
#define WLSIN_UVLO			BIT(0)

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
#define CFG_EN_SOURCE_BIT		BIT(3)

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

#define MISC_CFG2_REG			0x2636

#define NOLOCK_SPARE_REG		0x2637
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
#define WIN_OV_200MV			0
#define WIN_OV_300MV			1
#define WIN_OV_400MV			2
#define WIN_OV_500MV			3

#define DIV2_MODE_CFG_REG		0x265C

#define LCM_EXIT_CTRL_REG		0x265D

#define ICHG_SS_DAC_TARGET_REG		0x2660
#define ICHG_SS_DAC_MAX_REG		0x2661
#define ICHG_SS_DAC_MIN_REG		0x2662
#define ICHG_SS_DAC_VALUE_MASK		GENMASK(5, 0)
#define ICHG_MA_PER_BIT			100

#define VOUT_DAC_TARGET_REG		0x2663
#define VOUT_DAC_MAX_REG		0x2664
#define VOUT_DAC_MIN_REG		0x2665
#define VOUT_DAC_VALUE_MASK		GENMASK(7, 0)
#define VOUT_1P_MIN_MV			3300
#define VOUT_1S_MIN_MV			6600
#define VOUT_1P_PER_BIT_MV		10
#define VOUT_1S_PER_BIT_MV		20

#define VOUT_SS_DAC_TARGET_REG		0x2666
#define VOUT_SS_DAC_MAX_REG		0x2667
#define VOUT_SS_DAC_MIN_REG		0x2668
#define VOUT_SS_DAC_VALUE_MASK		GENMASK(5, 0)
#define VOUT_SS_1P_PER_BIT_MV		90
#define VOUT_SS_1S_PER_BIT_MV		180

#define IIN_SS_DAC_TARGET_REG		0x2669
#define IIN_SS_DAC_MAX_REG		0x266A
#define IIN_SS_DAC_MIN_REG		0x266B
#define IIN_SS_DAC_VALUE_MASK		GENMASK(6, 0)
#define IIN_MA_PER_BIT			50

#define PERPH0_CFG_SDCDC_REG		0x267A
#define EN_WIN_UV_BIT			BIT(7)

#define SSUPLY_TEMP_CTRL_REG		0x2683
#define SEL_OUT_TEMP_MUX_MASK		GENMASK(7, 5)
#define SEL_OUT_TEMP_MAX_SHFT          5
#define SEL_OUT_HIGHZ                  0 << SEL_OUT_TEMP_MAX_SHFT
#define SEL_OUT_VTEMP                  1 << SEL_OUT_TEMP_MAX_SHFT
#define SEL_OUT_ICHG                   2 << SEL_OUT_TEMP_MAX_SHFT
#define SEL_OUT_IIN_FB                 4 << SEL_OUT_TEMP_MAX_SHFT

#define PERPH1_INT_RT_STS_REG		0x2710
#define DIV2_WIN_OV_STS			BIT(7)
#define DIV2_WIN_UV_STS			BIT(6)
#define DIV2_ILIM_STS			BIT(5)
#define DIV2_CFLY_SS_DONE_STS          BIT(1)

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

/* Constant definitions */
/* Need to define max ILIM for smb1398 */
#define DIV2_MAX_ILIM_UA		5000000
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

enum isns_mode {
	ISNS_MODE_OFF = 0,
	ISNS_MODE_ACTIVE,
	ISNS_MODE_STANDBY,
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
	int			shft;
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
	struct notifier_block	nb;

	struct votable		*awake_votable;
	struct votable		*div2_cp_disable_votable;
	struct votable		*div2_cp_slave_disable_votable;
	struct votable		*div2_cp_ilim_votable;
	struct votable		*pre_regulator_iout_votable;
	struct votable		*pre_regulator_vout_votable;
	struct votable		*fcc_votable;
	struct votable		*fv_votable;

	struct work_struct	status_change_work;
	struct work_struct	taper_work;

	struct mutex		die_chan_lock;
	spinlock_t		status_change_lock;

	int			irqs[NUM_IRQS];
	int			die_temp;
	int			div2_cp_min_ilim_ua;
	int			ilim_ua_disable_div2_cp_slave;
	int			max_cutoff_soc;
	int			taper_entry_fv;
	u8			div2_forged_irq_status;
	u32			div2_cp_role;
	enum isns_mode		current_capability;

	bool			master_chip_ok;
	bool			slave_chip_ok;
	bool			status_change_running;
	bool			taper_work_running;
	bool			cutoff_soc_checked;
	bool			smb_en;
	bool			switcher_en;
	bool			slave_en;
	bool			in_suspend;
};

static const struct smb_irq smb_irqs[];

static int smb1398_read(struct smb1398_chip *chip, u16 reg, u8 *val)
{
	int rc = 0, value;

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

static int smb1398_div2_cp_forge_status1(
		struct smb1398_chip *chip, u8 *status)
{
	int rc = 0;
	u8 val;
	bool ilim, win_uv, win_ov;

	rc = smb1398_read(chip, INPUT_CURRENT_REGULATION_REG, &val);
	if (rc < 0)
		return rc;

	ilim = !!(val & DIV2_IN_ILIM);
	rc = smb1398_read(chip, PERPH1_INT_RT_STS_REG, &val);
	if (rc < 0)
		return rc;

	win_uv = !!(val & DIV2_WIN_UV_STS);
	win_ov = !!(val & DIV2_WIN_OV_STS);
	*status = ilim << 5 | win_uv << 1 | win_ov;

	dev_dbg(chip->dev, "status1 = 0x%x\n", *status);
	return rc;
}

static int smb1398_div2_cp_forge_status2(
		struct smb1398_chip *chip, u8 *status)
{
	int rc = 0;
	u8 val;
	bool smb_en, vin_ov, vin_uv, irev, tsd, switcher_off;

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

	vin_ov = !!(val & USBIN_OVLO);
	vin_uv = !!(val & USBIN_UVLO);

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

static int smb1398_div2_cp_forge_irq_status(
		struct smb1398_chip *chip, u8 *status)
{
	int rc = 0;
	u8 val;
	bool ilim, irev, tsd, off_vin, off_win;

	rc = smb1398_read(chip, PERPH1_INT_RT_STS_REG, &val);
	if (rc < 0)
		return rc;

	ilim = !!(val & DIV2_ILIM_STS);
	off_win = !!(val & (DIV2_WIN_OV_STS | DIV2_WIN_UV_STS));

	rc = smb1398_read(chip, PERPH0_INT_RT_STS_REG, &val);
	if (rc < 0)
		return rc;

	irev = !!(val & DIV2_IREV_LATCH_STS);
	tsd = !!(val & TEMP_SHUTDOWN_STS);
	off_vin = !!(val & (USBIN_OVLO_STS | USBIN_UVLO_STS));

	*status = ilim << 6 | irev << 3 | tsd << 2 | off_vin << 1 | off_win;

	dev_dbg(chip->dev, "irq_status = 0x%x\n", *status);
	return rc;
}

static int smb1398_get_enable_status(struct smb1398_chip *chip)
{
	int rc = 0;
	u8 val;

	bool switcher_en = false, cfly_ss_done = false;

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

	rc = smb1398_read(chip, PERPH1_INT_RT_STS_REG, &val);
	if (rc < 0)
		return rc;

	cfly_ss_done = !!(val & DIV2_CFLY_SS_DONE_STS);
	chip->switcher_en = cfly_ss_done && chip->switcher_en;

	dev_dbg(chip->dev, "switch_en = %d, cfly_ss_done = %d\n", switcher_en, cfly_ss_done);

	dev_dbg(chip->dev, "smb_en = %d, switcher_en = %d, slave_en = %d\n", chip->smb_en, chip->switcher_en, chip->slave_en);
	return rc;
}

static int smb1398_get_iin_ma(struct smb1398_chip *chip, int *iin_ma)
{
	int rc = 0, ilim, max;
	u8 val;

	rc = smb1398_read(chip, IIN_SS_DAC_TARGET_REG, &val);
	if (rc < 0)
		return rc;

	ilim = (val & IIN_SS_DAC_VALUE_MASK) * IIN_MA_PER_BIT;

	rc = smb1398_read(chip, IIN_SS_DAC_MAX_REG, &val);
	if (rc < 0)
		return rc;

	max = (val & IIN_SS_DAC_VALUE_MASK) * IIN_MA_PER_BIT;

	*iin_ma = min(ilim, max);

	dev_dbg(chip->dev, "get iin_ma = %dmA\n", *iin_ma);
	return rc;
}

static int smb1398_set_iin_ma(struct smb1398_chip *chip, int iin_ma)
{
	int rc = 0;
	u8 val;

	val = iin_ma / IIN_MA_PER_BIT;
	rc = smb1398_masked_write(chip, IIN_SS_DAC_TARGET_REG,
			IIN_SS_DAC_VALUE_MASK, val);

	dev_dbg(chip->dev, "set iin_ma = %dmA\n", iin_ma);
	return rc;
}

static int smb1398_div2_cp_en_switcher(struct smb1398_chip *chip, bool en)
{
	int rc;
	u8 mask, val;

	mask = EN_SWITCHER;
	val = EN_SWITCHER;
	rc = smb1398_masked_write(chip, MISC_SL_SWITCH_EN_REG,
			mask, en ? val : 0);
	if (rc < 0) {
		dev_err(chip->dev, "write SWITCH_EN_REG failed, rc=%d\n", rc);
		return rc;
	}

	chip->switcher_en = en;

	dev_dbg(chip->dev, "%s switcher\n", en ? "enable" : "disable");
	return rc;
}

static int smb1398_set_ichg_ma(struct smb1398_chip *chip, int ichg_ma)
{
	int rc = 0;
	u8 val;

	val = ichg_ma / ICHG_MA_PER_BIT;
	rc = smb1398_masked_write(chip, ICHG_SS_DAC_TARGET_REG,
			ICHG_SS_DAC_VALUE_MASK, val);

	dev_dbg(chip->dev, "set ichg %dmA\n", ichg_ma);
	return rc;
}

static int smb1398_get_ichg_ma(struct smb1398_chip *chip, int *ichg_ma)
{
	int rc = 0, ichg, max;
	u8 val;

	rc = smb1398_read(chip, ICHG_SS_DAC_TARGET_REG, &val);
	if (rc < 0)
		return rc;

	ichg = (val & ICHG_SS_DAC_VALUE_MASK) * ICHG_MA_PER_BIT;

	rc = smb1398_read(chip, ICHG_SS_DAC_MAX_REG, &val);
	if (rc < 0)
		return rc;

	max = (val & ICHG_SS_DAC_VALUE_MASK) * ICHG_MA_PER_BIT;
	*ichg_ma = min(ichg, max);

	dev_dbg(chip->dev, "get ichg %dmA\n", *ichg_ma);
	return 0;
}

static int smb1398_set_1s_vout_mv(struct smb1398_chip *chip, int vout_mv)
{
	int rc = 0;
	u8 val;

	if (vout_mv < VOUT_1S_MIN_MV)
		return -EINVAL;

	val = (vout_mv - VOUT_1S_MIN_MV) / VOUT_1S_PER_BIT_MV;

	rc = smb1398_masked_write(chip, VOUT_DAC_TARGET_REG,
			VOUT_DAC_VALUE_MASK, val);
	if (rc < 0)
		return rc;

	return 0;
}

static int smb1398_get_1s_vout_mv(struct smb1398_chip *chip, int *vout_mv)
{
	int rc, vout, max;
	u8 val;

	rc = smb1398_read(chip, VOUT_DAC_TARGET_REG, &val);
	if (rc < 0)
		return rc;

	vout = (val & VOUT_DAC_VALUE_MASK) * VOUT_1S_PER_BIT_MV +
		VOUT_1S_MIN_MV;

	rc = smb1398_read(chip, VOUT_DAC_MAX_REG, &val);
	if (rc < 0)
		return rc;

	max = (val & VOUT_DAC_VALUE_MASK) * VOUT_1S_PER_BIT_MV +
		VOUT_1S_MIN_MV;

	*vout_mv = min(vout, max);
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
	if (rc < 0)
		dev_err(chip->dev, "read die_temp_chan failed, rc=%d\n", rc);
	else {
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
	case ISNS_MODE_STANDBY:	/* VTEMP */
		mux_sel = SEL_OUT_VTEMP;
		break;
	case ISNS_MODE_OFF: /* High-Z */
		mux_sel = SEL_OUT_HIGHZ;
		break;
	case ISNS_MODE_ACTIVE: /* IIN_FB */
		mux_sel = SEL_OUT_IIN_FB;
		break;
	default:
		return -EINVAL;
	}

	rc = smb1398_masked_write(chip, SSUPLY_TEMP_CTRL_REG,
			SEL_OUT_TEMP_MUX_MASK, mux_sel);
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
	if (!chip->div2_cp_slave_psy)
		chip->div2_cp_slave_psy = power_supply_get_by_name("cp_slave");
	else
		return true;

	if (!chip->div2_cp_slave_psy)
		return false;

	return true;
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
		dev_dbg(chip->dev, "master isns = %duA\n", *isns_ua);
	}

	return rc;
}

static int smb1398_div2_cp_get_slave_isns(
		struct smb1398_chip *chip, int *isns_ua)
{
	union power_supply_propval pval = {0};
	int temp = 0, rc;

	if (!is_cps_available(chip)) {
		isns_ua = 0;
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
		dev_dbg(chip->dev, "slave isns = %duA\n", *isns_ua);
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

static enum power_supply_property div2_cp_master_props[] = {
	POWER_SUPPLY_PROP_CP_STATUS1,
	POWER_SUPPLY_PROP_CP_STATUS2,
	POWER_SUPPLY_PROP_CP_ENABLE,
	POWER_SUPPLY_PROP_CP_SWITCHER_EN,
	POWER_SUPPLY_PROP_CP_DIE_TEMP,
	POWER_SUPPLY_PROP_CP_ISNS,
	POWER_SUPPLY_PROP_CP_ISNS_SLAVE,
	POWER_SUPPLY_PROP_CP_TOGGLE_SWITCHER,
	POWER_SUPPLY_PROP_CP_IRQ_STATUS,
	POWER_SUPPLY_PROP_CP_ILIM,
	POWER_SUPPLY_PROP_CHIP_VERSION,
	POWER_SUPPLY_PROP_PARALLEL_MODE,
	POWER_SUPPLY_PROP_PARALLEL_OUTPUT_MODE,
	POWER_SUPPLY_PROP_MIN_ICL,
	POWER_SUPPLY_PROP_CHIP_OK,

};

static int div2_cp_master_get_prop(struct power_supply *psy,
				enum power_supply_property prop,
				union power_supply_propval *val)
{
	struct smb1398_chip *chip = power_supply_get_drvdata(psy);
	int rc = 0, ilim_ma, temp, isns_ua;
	u8 status;

	switch (prop) {
	case POWER_SUPPLY_PROP_CP_STATUS1:
		rc = smb1398_div2_cp_forge_status1(chip, &status);
		if (!rc)
			val->intval = status;
		break;
	case POWER_SUPPLY_PROP_CP_STATUS2:
		rc = smb1398_div2_cp_forge_status2(chip, &status);
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
		val->intval = chip->master_chip_ok;
		break;
	case POWER_SUPPLY_PROP_CP_SWITCHER_EN:
		rc = smb1398_get_enable_status(chip);
		if (!rc)
			val->intval = chip->switcher_en;
		break;
	case POWER_SUPPLY_PROP_CP_ISNS:
		rc = smb1398_div2_cp_get_master_isns(chip, &isns_ua);
		if (rc >= 0)
			val->intval = isns_ua;
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
//		if (chip->in_suspend) {
//			if (chip->die_temp != -ENODATA)
//				val->intval = chip->die_temp;
//			else
//				rc = -ENODATA;
//		} else {
//			rc = smb1398_get_die_temp(chip, &temp);
//			if (!rc) {
//				if (temp <= THERMAL_SUSPEND_DECIDEGC)
//					chip->die_temp = temp;
//				else if (chip->die_temp == -ENODATA)
//					rc = -ENODATA;
//				val->intval = chip->die_temp;
//			}
//		}
//		break;
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
		val->intval = chip->div2_forged_irq_status;
		rc = smb1398_div2_cp_forge_irq_status(chip, &status);
		if (!rc)
			val->intval |= status;
		break;
	case POWER_SUPPLY_PROP_CP_ILIM:
		if (is_cps_available(chip)) {
			if (chip->div2_cp_ilim_votable)
				val->intval = get_effective_result(
						chip->div2_cp_ilim_votable);
		} else {
			rc = smb1398_get_iin_ma(chip, &ilim_ma);
			if (!rc)
				val->intval = ilim_ma * 1000;
		}
		break;
	case POWER_SUPPLY_PROP_CHIP_VERSION:
		val->intval = DIV2_CP_HW_VERSION_3;
		break;
	case POWER_SUPPLY_PROP_PARALLEL_MODE:
		val->intval = POWER_SUPPLY_PL_USBIN_USBIN;
		break;
	case POWER_SUPPLY_PROP_PARALLEL_OUTPUT_MODE:
		val->intval = POWER_SUPPLY_PL_OUTPUT_VBAT; /* only vbat */
		break;
	case POWER_SUPPLY_PROP_MIN_ICL:
		val->intval = chip->div2_cp_min_ilim_ua;
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
		chip->div2_forged_irq_status = val->intval;
		break;
	case POWER_SUPPLY_PROP_CP_ILIM:
		if (chip->div2_cp_ilim_votable)
			vote_override(chip->div2_cp_ilim_votable,
					CC_MODE_VOTER, true, val->intval);
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

	if (!chip->fcc_votable) {
		chip->fcc_votable = find_votable("FCC");
		if (!chip->fcc_votable) {
			dev_dbg(chip->dev, "Couldn't find FCC voltable\n");
			return false;
		}
	}

	if (!chip->fv_votable) {
		chip->fv_votable = find_votable("FV");
		if (!chip->fv_votable) {
			dev_dbg(chip->dev, "Couldn't find FV voltable\n");
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

	if (pval.intval >= chip->max_cutoff_soc)
		return true;
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

static int smb1398_div2_cp_disable_vote_cb(struct votable *votable,
		void *data, int disable, const char *client)
{
	struct smb1398_chip *chip = (struct smb1398_chip *)data;
	int rc = 0;

	if (!is_psy_voter_available(chip) || chip->in_suspend)
		return -EAGAIN;

	rc = smb1398_div2_cp_en_switcher(chip, !disable);
	if (rc < 0) {
		dev_err(chip->dev, "%s switcher failed, rc=%d\n",
				!!disable ? "disable" : "enable", rc);
		return rc;
	}

	if (is_cps_available(chip))
		vote(chip->div2_cp_slave_disable_votable, MAIN_DISABLE_VOTER,
				!!disable ? true : false, 0);

	if (chip->div2_cp_master_psy)
		power_supply_changed(chip->div2_cp_master_psy);

	return 0;
}

static int smb1398_div2_cp_slave_disable_vote_cb(struct votable *votable,
		void *data, int disable, const char *client)
{
	struct smb1398_chip *chip = (struct smb1398_chip *)data;
	union power_supply_propval pval = {0};
	u16 reg;
	u8 mask, val;
	int rc;

	if (!is_cps_available(chip))
		return -ENODEV;

	reg = MISC_CFG0_REG;
	mask = DIS_SYNC_DRV_BIT;
	val = !!disable ? DIS_SYNC_DRV_BIT : 0;
	rc = smb1398_masked_write(chip, reg, mask, val);
	if (rc < 0) {
		dev_err(chip->dev, "%s slave SYNC_DRV failed, rc=%d\n",
				!!disable ? "disable" : "enable", rc);
		return rc;
	}

	reg = MISC_SL_SWITCH_EN_REG;
	mask = EN_SLAVE;
	val = !!disable ? 0 : EN_SLAVE;
	rc = smb1398_masked_write(chip, reg, mask, val);
	if (rc < 0) {
		dev_err(chip->dev, "write slave_en failed, rc=%d\n", rc);
		return rc;
	}

	pval.intval = !disable;
	rc = power_supply_set_property(chip->div2_cp_slave_psy,
		POWER_SUPPLY_PROP_CP_ENABLE, &pval);
	if (rc < 0) {
		dev_err(chip->dev, "%s slave switcher failed, rc=%d\n",
				!!disable ? "disable" : "enable", rc);
		return rc;
	}

	return rc;
}

static int smb1398_div2_cp_ilim_vote_cb(struct votable *votable,
		void *data, int ilim_ua, const char *client)
{
	struct smb1398_chip *chip = (struct smb1398_chip *)data;
	union power_supply_propval pval = {0};
	int rc = 0, max_ilim_ua;
	bool slave_dis;

	if (!is_psy_voter_available(chip) || chip->in_suspend)
		return -EAGAIN;

	if (!client)
		return -EINVAL;

	max_ilim_ua = is_cps_available(chip) ?
		DIV2_MAX_ILIM_DUAL_CP_UA : DIV2_MAX_ILIM_UA;
	ilim_ua = min(ilim_ua, max_ilim_ua);
	if (ilim_ua < chip->div2_cp_min_ilim_ua) {
		dev_dbg(chip->dev, "ilim %duA is too low to config CP charging\n",
				ilim_ua);
		vote(chip->div2_cp_disable_votable, ILIM_VOTER, true, 0);
	} else {
		slave_dis = get_effective_result(
				chip->div2_cp_slave_disable_votable);
		if (is_cps_available(chip) /*&& !slave_dis*/) {
			ilim_ua /= 2;
			pval.intval = ilim_ua;
			rc = power_supply_set_property(chip->div2_cp_slave_psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_MAX, &pval);
			if (rc < 0)
				dev_err(chip->dev, "set CP slave ilim failed, rc=%d\n",
						rc);
			dev_dbg(chip->dev, "set CP slave ilim to %duA\n",
					ilim_ua);
		}

		rc = smb1398_set_iin_ma(chip, ilim_ua / 1000);
		if (rc < 0) {
			dev_err(chip->dev, "set CP master ilim failed, rc=%d\n",
					rc);
			return rc;
		}
		dev_dbg(chip->dev, "set CP master ilim to %duA\n", ilim_ua);
		vote(chip->div2_cp_disable_votable, ILIM_VOTER, false, 0);
	}

	return 0;
}

static int smb1398_div2_cp_create_votables(struct smb1398_chip *chip)
{
	chip->awake_votable = create_votable("SMB1398_AWAKE",
			VOTE_SET_ANY, smb1398_awake_vote_cb, chip);
	if (IS_ERR(chip->awake_votable))
		return PTR_ERR(chip->awake_votable);

	chip->div2_cp_disable_votable = create_votable("CP_DISABLE",
			VOTE_SET_ANY, smb1398_div2_cp_disable_vote_cb, chip);
	if (IS_ERR(chip->div2_cp_disable_votable))
		return PTR_ERR(chip->div2_cp_disable_votable);

	chip->div2_cp_slave_disable_votable = create_votable("CP_SLAVE_DISABLE",
			VOTE_SET_ANY, smb1398_div2_cp_slave_disable_vote_cb,
			chip);
	if (IS_ERR(chip->div2_cp_slave_disable_votable))
		return PTR_ERR(chip->div2_cp_slave_disable_votable);

	chip->div2_cp_ilim_votable = create_votable("CP_ILIM",
			VOTE_MIN, smb1398_div2_cp_ilim_vote_cb, chip);
	if (IS_ERR(chip->div2_cp_ilim_votable))
		return PTR_ERR(chip->div2_cp_ilim_votable);

	vote(chip->div2_cp_disable_votable, USER_VOTER, true, 0);
	vote(chip->div2_cp_disable_votable, CUTOFF_SOC_VOTER,
			is_cutoff_soc_reached(chip), 0);

	if (is_psy_voter_available(chip))
		vote(chip->div2_cp_ilim_votable, FCC_VOTER, true,
			get_effective_result(chip->fcc_votable) / 2);

	return 0;
}

static void smb1398_destroy_votables(struct smb1398_chip *chip)
{
	destroy_votable(chip->awake_votable);
	destroy_votable(chip->div2_cp_disable_votable);
	destroy_votable(chip->div2_cp_ilim_votable);
}

static irqreturn_t default_irq_handler(int irq, void *data)
{
	struct smb1398_chip *chip = data;
	int rc, i;
	bool switcher_en = chip->switcher_en;

	for (i = 0; i < NUM_IRQS; i++) {
		if (irq == chip->irqs[i]) {
			dev_dbg(chip->dev, "IRQ %s triggered\n",
					smb_irqs[i].name);
			chip->div2_forged_irq_status |= 1 << smb_irqs[i].shft;
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
		.shft		= 2,
	},
	[DIV2_IREV_LATCH_IRQ]	= {
		.name		= "div2-irev",
		.handler	= default_irq_handler,
		.wake		= true,
		.shft		= 3,
	},
	[USB_IN_UVLO_IRQ]	= {
		.name		= "usbin-uv",
		.handler	= default_irq_handler,
		.wake		= true,
		.shft		= 1,
	},
	[USB_IN_OVLO_IRQ]	= {
		.name		= "usbin-ov",
		.handler	= default_irq_handler,
		.wake		= true,
		.shft		= 1,
	},
	/* useful IRQs from perph1 */
	[DIV2_ILIM_IRQ]		= {
		.name		= "div2-ilim",
		.handler	= default_irq_handler,
		.wake		= true,
		.shft		= 6,
	},
	[DIV2_WIN_UV_IRQ]	= {
		.name		= "div2-win-uv",
		.handler	= default_irq_handler,
		.wake		= true,
		.shft		= 0,
	},
	[DIV2_WIN_OV_IRQ]	= {
		.name		= "div2-win-ov",
		.handler	= default_irq_handler,
		.wake		= true,
		.shft		= 0,
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

	return 0;
}

static int smb1398_request_interrupts(struct smb1398_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *child;
	int rc = 0;
	const char *name;
	struct property *prop;

	for_each_available_child_of_node(node, child) {
		of_property_for_each_string(child, "interrupt-names",
				prop, name) {
			rc = smb1398_request_interrupt(chip, child, name);
			if (rc < 0)
				return rc;
		}
	}

	return rc;
}

static void smb1398_status_change_work(struct work_struct *work)
{
	struct smb1398_chip *chip = container_of(work,
			struct smb1398_chip, status_change_work);
	union power_supply_propval pval = {0};
	enum power_supply_property prop;
	int rc, ilim_ua;

	if (!is_psy_voter_available(chip))
		goto out;

	if (!is_adapter_in_cc_mode(chip))
		vote(chip->div2_cp_disable_votable, CUTOFF_SOC_VOTER,
				is_cutoff_soc_reached(chip), 0);

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
		vote(chip->div2_cp_disable_votable, TAPER_VOTER, false, 0);
		vote(chip->div2_cp_disable_votable, SRC_VOTER, true, 0);
		vote(chip->div2_cp_disable_votable, CUTOFF_SOC_VOTER, true, 0);
		vote(chip->fcc_votable, CP_VOTER, false, 0);
		vote(chip->div2_cp_ilim_votable, CC_MODE_VOTER, false, 0);
		goto out;
	}

	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_SMB_EN_REASON, &pval);
	if (rc < 0) {
		dev_err(chip->dev, "Get SMB_EN_REASON failed, rc=%d\n",
				rc);
		goto out;
	}

	if (pval.intval == POWER_SUPPLY_CP_NONE) {
		vote(chip->div2_cp_disable_votable, SRC_VOTER, true, 0);
		goto out;
	}

	vote(chip->div2_cp_disable_votable, SRC_VOTER, false, 0);
	if (!chip->cutoff_soc_checked) {
		vote(chip->div2_cp_disable_votable, CUTOFF_SOC_VOTER,
				is_cutoff_soc_reached(chip), 0);
		chip->cutoff_soc_checked = true;
	}

	if (pval.intval == POWER_SUPPLY_CP_WIRELESS) {
		/*
		 * Get the max output current from the wireless PSY
		 * and set the DIV2 CP ilim accordingly
		 */
		vote(chip->div2_cp_ilim_votable, ICL_VOTER, false, 0);
		rc = power_supply_get_property(chip->dc_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
		if (rc < 0)
			dev_err(chip->dev, "Get DC CURRENT_MAX failed, rc=%d\n",
					rc);
		else
			vote(chip->div2_cp_ilim_votable, WIRELESS_VOTER,
					true, pval.intval);
	} else {
		vote(chip->div2_cp_ilim_votable, WIRELESS_VOTER, false, 0);
		/* CC mode for PPS and CV mode for HVDCP3 */
		if (pval.intval == POWER_SUPPLY_CP_PPS)
			prop = POWER_SUPPLY_PROP_PD_CURRENT_MAX;
		else if (pval.intval == POWER_SUPPLY_CP_HVDCP3)
			prop = POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED;
		else
			goto out;

		rc = power_supply_get_property(chip->usb_psy, prop, &pval);
		if (rc < 0) {
			dev_err(chip->dev, "get INPUT_CURRENT failed, rc = %d\n",
					rc);
			goto out;
		}
		ilim_ua = pval.intval;
		/* Over draw PPS adapter to keep it in CC mode */
		if (prop == POWER_SUPPLY_PROP_PD_CURRENT_MAX)
			ilim_ua = ilim_ua * 10 / 8;

		vote(chip->div2_cp_ilim_votable, ICL_VOTER, true, ilim_ua);
	}

	/*
	 * Remove CP Taper condition disable vote if float voltage
	 * increased in comparison to voltage at which it entered taper.
	 */
	if (chip->taper_entry_fv < get_effective_result(chip->fv_votable)) {
		vote(chip->div2_cp_slave_disable_votable, TAPER_VOTER, false, 0);
		vote(chip->div2_cp_disable_votable, TAPER_VOTER, false, 0);
	}
	/*
	 * all votes that would result in disabling the charge pump have
	 * been cast; ensure the charhe pump is still enabled before
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

	if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER) {
		if (!chip->taper_work_running) {
			chip->taper_work_running = true;
			vote(chip->awake_votable, TAPER_VOTER, true, 0);
			queue_work(system_long_wq, &chip->taper_work);
		}
	}
out:
	//vote(chip->awake_votable, STATUS_CHANGE_VOTER, false, 0);
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
			strcmp(psy->desc->name, "cp_slave") == 0) {
		spin_lock_irqsave(&chip->status_change_lock, flags);
		if (!chip->status_change_running) {
			chip->status_change_running = true;
			//vote(chip->awake_votable, STATUS_CHANGE_VOTER, true, 0);
			pm_stay_awake(chip->dev);
			schedule_work(&chip->status_change_work);
		}
		spin_unlock_irqrestore(&chip->status_change_lock, flags);
	}

	return NOTIFY_OK;
}

static void smb1398_taper_work(struct work_struct *work)
{
	struct smb1398_chip *chip = container_of(work,
			struct smb1398_chip, taper_work);
	union power_supply_propval pval = {0};
	int rc, fcc_uA, fv_uV, stepper_ua;
	bool slave_en;

	if (!is_psy_voter_available(chip))
		goto out;

	chip->taper_entry_fv = get_effective_result(chip->fv_votable);
	while (true) {
		rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		if (rc < 0) {
			dev_err(chip->dev, "get CHARGE_TYPE failed, rc=%d\n",
					rc);
			goto out;
		}

		fv_uV = get_effective_result(chip->fv_votable);
		if (fv_uV > chip->taper_entry_fv) {
			dev_dbg(chip->dev, "Float voltage increased (%d-->%d)uV, exit!\n",
					chip->taper_entry_fv, fv_uV);
			vote(chip->div2_cp_disable_votable, TAPER_VOTER,
					false, 0);
			goto out;
		} else {
			chip->taper_entry_fv = fv_uV;
		}

		if (pval.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER) {
			stepper_ua = is_adapter_in_cc_mode(chip) ?
				TAPER_STEPPER_UA_IN_CC_MODE :
				TAPER_STEPPER_UA_DEFAULT;
			fcc_uA = get_effective_result(chip->fcc_votable)
				- stepper_ua;
			dev_dbg(chip->dev, "Taper stepper reduce FCC to %d\n",
					fcc_uA);
			vote(chip->fcc_votable, CP_VOTER, true, fcc_uA);
			/*
			 * If total FCC is less than the minimum ILIM to
			 * keep CP master and slave online, disable CP.
			 */
			if (fcc_uA < (chip->div2_cp_min_ilim_ua * 2)) {
				vote(chip->div2_cp_disable_votable,
						TAPER_VOTER, true, 0);
				goto out;
			}
			/*
			 * If total FCC is less than the minimum ILIM to keep
			 * slave CP online, disable slave, and set master CP
			 * ILIM to maximum to avoid ILIM IRQ storm.
			 */
			slave_en = !get_effective_result(
					chip->div2_cp_slave_disable_votable);
			if ((fcc_uA < chip->ilim_ua_disable_div2_cp_slave) &&
					slave_en && is_cps_available(chip)) {
				vote(chip->div2_cp_slave_disable_votable,
						TAPER_VOTER, true, 0);
				dev_dbg(chip->dev, "Disable slave CP in taper\n");
				vote_override(chip->div2_cp_ilim_votable,
						CC_MODE_VOTER, true,
						DIV2_MAX_ILIM_DUAL_CP_UA);
			}
		} else {
			dev_dbg(chip->dev, "Not in taper, exit!\n");
		}
		msleep(500);
	}
out:
	dev_dbg(chip->dev, "exit taper work\n");
	vote(chip->fcc_votable, CP_VOTER, false, 0);
	vote(chip->awake_votable, TAPER_VOTER, false, 0);
	chip->taper_work_running = false;
}

static int smb1398_div2_cp_hw_init(struct smb1398_chip *chip)
{
	int rc = 0;

	/* Configure window (Vin/2 - Vout) OV level to 500mV */
	rc = smb1398_masked_write(chip, DIV2_PROTECTION_REG,
			DIV2_WIN_OV_SEL_MASK, WIN_OV_500MV);
	if (rc < 0) {
		dev_err(chip->dev, "set WIN_OV_500MV failed, rc=%d\n", rc);
		return rc;
	}

	/* Configure master TEMP pin to output Vtemp signal by default */
	rc = smb1398_masked_write(chip, SSUPLY_TEMP_CTRL_REG,
			SEL_OUT_TEMP_MUX_MASK, SEL_OUT_VTEMP);
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

	rc = smb1398_masked_write(chip, MISC_CFG0_REG,
			CFG_EN_SOURCE_BIT, CFG_EN_SOURCE_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "set CFG_EN_SOURCE failed, rc=%d\n",
				rc);
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

	chip->ilim_ua_disable_div2_cp_slave = chip->div2_cp_min_ilim_ua * 3;
	of_property_read_u32(chip->dev->of_node, "qcom,ilim-ua-disable-slave",
			&chip->ilim_ua_disable_div2_cp_slave);

	return 0;
}

static int smb1398_div2_cp_master_probe(struct smb1398_chip *chip)
{
	int rc;

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

	spin_lock_init(&chip->status_change_lock);
	INIT_WORK(&chip->status_change_work, &smb1398_status_change_work);
	INIT_WORK(&chip->taper_work, &smb1398_taper_work);

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
	chip->master_chip_ok = true;

	dev_dbg(chip->dev, "smb1398 DIV2_CP master probe successfully\n");

	return 0;
destroy_votable:
	mutex_destroy(&chip->die_chan_lock);
	smb1398_destroy_votables(chip);

	return rc;
}

static enum power_supply_property div2_cp_slave_props[] = {
	POWER_SUPPLY_PROP_CP_ENABLE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
	POWER_SUPPLY_PROP_CHIP_OK,
};

static int div2_cp_slave_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct smb1398_chip *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CP_ENABLE:
		pval->intval = chip->switcher_en;
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
	case POWER_SUPPLY_PROP_CHIP_OK:
		pval->intval = chip->slave_chip_ok;
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
		rc = smb1398_div2_cp_en_switcher(chip, !!pval->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		ilim_ma = pval->intval / 1000;
		rc = smb1398_set_iin_ma(chip, ilim_ma);
		break;
	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
		mode = (enum isns_mode)pval->intval;
		rc = smb1398_div2_cp_isns_mode_control(chip, mode);
		chip->current_capability = mode;
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

static int smb1398_div2_cp_slave_probe(struct smb1398_chip *chip)
{
	int rc = 0;
	u8 status;

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
	rc = smb1398_masked_write(chip, PERPH0_CFG_SDCDC_REG, EN_WIN_UV_BIT, 0);
	if (rc < 0) {
		dev_err(chip->dev, "disable DIV2_CP WIN_UV failed, rc=%d\n",
				rc);
		return rc;
	}

	/* Configure slave TEMP pin to HIGH-Z by default */
	rc = smb1398_masked_write(chip, SSUPLY_TEMP_CTRL_REG,
			SEL_OUT_TEMP_MUX_MASK, SEL_OUT_HIGHZ);
	if (rc < 0) {
		dev_err(chip->dev, "set SSUPLY_TEMP_CTRL_REG failed, rc=%d\n",
				rc);
		return rc;
	}

	rc = smb1398_init_div2_cp_slave_psy(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Initial div2_cp_slave_psy failed, rc=%d\n",
				rc);
		return rc;
	}
	chip->slave_chip_ok = true;

	dev_dbg(chip->dev, "smb1398 DIV2_CP slave probe successfully\n");

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

static int smb1398_create_pre_regulator_votable(struct smb1398_chip *chip)
{
	chip->pre_regulator_iout_votable = create_votable("PRE_REGULATOR_IOUT",
			VOTE_MIN, smb1398_pre_regulator_iout_vote_cb, chip);
	if (IS_ERR(chip->pre_regulator_iout_votable))
		return PTR_ERR(chip->pre_regulator_iout_votable);

	chip->pre_regulator_vout_votable = create_votable("PRE_REGULATOR_VOUT",
			VOTE_MIN, smb1398_pre_regulator_vout_vote_cb, chip);

	if (IS_ERR(chip->pre_regulator_vout_votable))
		return PTR_ERR(chip->pre_regulator_vout_votable);

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
	u8 status;

	rc = smb1398_read(chip, MODE_STATUS_REG, &status);
	if (rc < 0) {
		dev_err(chip->dev, "Read pre-regulator MODE_STATUS_REG failed, rc=%d\n",
				rc);
		return rc;
	}

	rc = smb1398_create_pre_regulator_votable(chip);
	if (rc > 0) {
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
		cancel_work_sync(&chip->taper_work);
		cancel_work_sync(&chip->status_change_work);
		mutex_destroy(&chip->die_chan_lock);
		smb1398_destroy_votables(chip);
	}

	return 0;
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

	if (chip->div2_cp_role == DIV2_CP_MASTER) {
		rerun_election(chip->div2_cp_ilim_votable);
		rerun_election(chip->div2_cp_disable_votable);
	}

	return 0;
}

static const struct dev_pm_ops smb1398_pm_ops = {
	.suspend	= smb1398_suspend,
	.resume		= smb1398_resume,
};

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,smb1396-div2-cp-master-psy",
	  .data = (void *)DIV2_CP_MASTER,
	},
	{ .compatible = "qcom,smb1396-div2-cp-slave-psy",
	  .data = (void *)DIV2_CP_SLAVE,
	},
	{ .compatible = "qcom,smb1398-pre-regulator-psy",
	  .data = (void *)COMBO_PRE_REGULATOR,
	},
	{
	},
};

static struct platform_driver smb1398_driver = {
	.driver	= {
		.name		= "qcom,smb1398-charger-psy",
		.pm		= &smb1398_pm_ops,
		.of_match_table	= match_table,
	},
	.probe	= smb1398_probe,
	.remove	= smb1398_remove,
};
module_platform_driver(smb1398_driver);

MODULE_DESCRIPTION("SMB1398 charger driver");
MODULE_LICENSE("GPL v2");
