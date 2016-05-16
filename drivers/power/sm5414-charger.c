/* Copyright (c) 2014 The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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

#define pr_fmt(fmt) "SM5414 %s: " fmt, __func__
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mutex.h>
#include <linux/qpnp/qpnp-adc.h>

 /* constants */
#define USB2_MIN_CURRENT_MA		100
#define USB2_MAX_CURRENT_MA		500
#define USB3_MIN_CURRENT_MA		150
#define USB3_MAX_CURRENT_MA		900
#define AC_CHG_CURRENT_MASK		0x70
#define AC_CHG_CURRENT_SHIFT		4
#define SM5414_IRQ_REG_COUNT		6
#define SM5414_FAST_CHG_MIN_MA		100
#define SM5414_FAST_CHG_MAX_MA		2500
#define SM5414_DEFAULT_BATT_CAPACITY	50
#define SM5414_BATT_GOOD_THRE_2P5	0x1

/*
 * SM5414 Registers.
 */
#define SM5414_INT1			0x00
#define SM5414_INT2			0x01
#define SM5414_INT3			0x02
#define SM5414_INTMASK1		0x03
#define SM5414_INTMASK2		0x04
#define SM5414_INTMASK3		0x05
#define SM5414_STATUS		0x06
#define SM5414_CTRL			0x07
#define SM5414_VBUSCTRL		0x08
#define SM5414_CHGCTRL1		0x09
#define SM5414_CHGCTRL2		0x0A
#define SM5414_CHGCTRL3		0x0B
#define SM5414_CHGCTRL4		0x0C
#define SM5414_CHGCTRL5		0x0D

#define SM5414_REG_NUM 0x0E

/**********************************************************
*
*   [MASK/SHIFT]
*
*********************************************************/

#define SM5414_INT1_THEMREG		0x01
#define SM5414_INT1_THEMSHDN	0x02
#define SM5414_INT1_BATOVP		0x04
#define SM5414_INT1_VBUSLIMIT	0x08
#define SM5414_INT1_AICL		0x10
#define SM5414_INT1_VBUSINOK	0x20
#define SM5414_INT1_VBUSUVLO	0x40
#define SM5414_INT1_VBUSOVP		0x80
#define SM5414_INT1_MASK		0xFF
#define SM5414_INT1_SHIFT	   0

#define SM5414_INT2_TOPOFF		0x01
#define SM5414_INT2_DONE		0x02
#define SM5414_INT2_CHGRSTF		0x04
#define SM5414_INT2_PRETMROFF	0x08
#define SM5414_INT2_OTGFAIL		0x10
#define SM5414_INT2_WEAKBAT		0x20
#define SM5414_INT2_NOBAT		0x40
#define SM5414_INT2_FASTTMROFF	0x80
#define SM5414_INT2_MASK		0xFF
#define SM5414_INT2_SHIFT	   0

#define SM5414_INT3_DISLIMIT	0x01
#define SM5414_INT3_VSYSOLP		0x02
#define SM5414_INT3_VSYSNG		0x04
#define SM5414_INT3_VSYSOK		0x08
#define SM5414_INT3_MASK		0x0F
#define SM5414_INT3_SHIFT	   0

#define SM5414_INTMSK1_THEMREGM		0x01
#define SM5414_INTMSK1_THEMSHDNM	0x02
#define SM5414_INTMSK1_BATOVPM		0x04
#define SM5414_INTMSK1_VBUSLIMITM	0x08
#define SM5414_INTMSK1_AICLM		0x10
#define SM5414_INTMSK1_VBUSINOKM	0x20
#define SM5414_INTMSK1_VBUSUVLOM	0x40
#define SM5414_INTMSK1_VBUSOVPM		0x80

#define SM5414_INTMSK2_TOPOFFM		0x01
#define SM5414_INTMSK2_DONEM		0x02
#define SM5414_INTMSK2_CHGRSTFM		0x04
#define SM5414_INTMSK2_PRETMROFFM	0x08
#define SM5414_INTMSK2_OTGFAILM		0x10
#define SM5414_INTMSK2_WEAKBATM		0x20
#define SM5414_INTMSK2_NOBATM		0x40
#define SM5414_INTMSK2_FASTTMROFFM	0x80

#define SM5414_INTMSK3_DISLIMITM	0x01
#define SM5414_INTMSK3_VSYSOLPM		0x02
#define SM5414_INTMSK3_VSYSNGM		0x04
#define SM5414_INTMSK3_VSYSOKM		0x08

#define SM5414_STATUS_VBUSOVP_MASK		  0x1
#define SM5414_STATUS_VBUSOVP_SHIFT		 7
#define SM5414_STATUS_VBUSUVLO_MASK		 0x1
#define SM5414_STATUS_VBUSUVLO_SHIFT		6
#define SM5414_STATUS_TOPOFF_MASK		   0x1
#define SM5414_STATUS_TOPOFF_SHIFT		  5
#define SM5414_STATUS_VSYSOLP_MASK		  0x1
#define SM5414_STATUS_VSYSOLP_SHIFT		 4
#define SM5414_STATUS_DISLIMIT_MASK		 0x1
#define SM5414_STATUS_DISLIMIT_SHIFT		3
#define SM5414_STATUS_THEMSHDN_MASK		 0x1
#define SM5414_STATUS_THEMSHDN_SHIFT		2
#define SM5414_STATUS_BATDET_MASK		   0x1
#define SM5414_STATUS_BATDET_SHIFT		  1
#define SM5414_STATUS_SUSPEND_MASK		  0x1
#define SM5414_STATUS_SUSPEND_SHIFT		 0

#define SM5414_CTRL_ENCOMPARATOR_MASK	   0x1
#define SM5414_CTRL_ENCOMPARATOR_SHIFT	  6
#define SM5414_CTRL_RESET_MASK			  0x1
#define SM5414_CTRL_RESET_SHIFT			 3
#define SM5414_CTRL_SUSPEN_MASK			 0x1
#define SM5414_CTRL_SUSPEN_SHIFT			2
#define SM5414_CTRL_CHGEN_MASK			  0x1
#define SM5414_CTRL_CHGEN_SHIFT			 1
#define SM5414_CTRL_ENBOOST_MASK			0x1
#define SM5414_CTRL_ENBOOST_SHIFT		   0

#define SM5414_VBUSCTRL_VBUSLIMIT_MASK	  0x3F
#define SM5414_VBUSCTRL_VBUSLIMIT_SHIFT	 0

#define SM5414_CHGCTRL1_AICLTH_MASK		 0x7
#define SM5414_CHGCTRL1_AICLTH_SHIFT		4
#define SM5414_CHGCTRL1_AUTOSTOP_MASK	   0x1
#define SM5414_CHGCTRL1_AUTOSTOP_SHIFT	  3
#define SM5414_CHGCTRL1_AICLEN_MASK		 0x1
#define SM5414_CHGCTRL1_AICLEN_SHIFT		2
#define SM5414_CHGCTRL1_PRECHG_MASK		 0x3
#define SM5414_CHGCTRL1_PRECHG_SHIFT		0

#define SM5414_CHGCTRL2_FASTCHG_MASK		0x3F
#define SM5414_CHGCTRL2_FASTCHG_SHIFT	   0

#define SM5414_CHGCTRL3_BATREG_MASK		 0xF
#define SM5414_CHGCTRL3_BATREG_SHIFT		4
#define SM5414_CHGCTRL3_WEAKBAT_MASK		0xF
#define SM5414_CHGCTRL3_WEAKBAT_SHIFT	   0

#define SM5414_CHGCTRL4_TOPOFF_MASK		 0xF
#define SM5414_CHGCTRL4_TOPOFF_SHIFT		3
#define SM5414_CHGCTRL4_DISLIMIT_MASK	   0x7
#define SM5414_CHGCTRL4_DISLIMIT_SHIFT	  0

#define SM5414_CHGCTRL5_VOTG_MASK		   0x3
#define SM5414_CHGCTRL5_VOTG_SHIFT		  4
#define SM5414_CHGCTRL5_FASTTIMER_MASK	  0x3
#define SM5414_CHGCTRL5_FASTTIMER_SHIFT	 2
#define SM5414_CHGCTRL5_TOPOFFTIMER_MASK	0x3
#define SM5414_CHGCTRL5_TOPOFFTIMER_SHIFT   0


#define FASTCHG_100mA	   0
#define FASTCHG_150mA	   1
#define FASTCHG_200mA	   2
#define FASTCHG_250mA	   3
#define FASTCHG_300mA	   4
#define FASTCHG_350mA	   5
#define FASTCHG_400mA	   6
#define FASTCHG_450mA	   7
#define FASTCHG_500mA	   8
#define FASTCHG_550mA	   9
#define FASTCHG_600mA	  10
#define FASTCHG_650mA	  11
#define FASTCHG_700mA	  12
#define FASTCHG_750mA	  13
#define FASTCHG_800mA	  14
#define FASTCHG_850mA	  15
#define FASTCHG_900mA	  16
#define FASTCHG_950mA	  17
#define FASTCHG_1000mA	 18
#define FASTCHG_1050mA	 19
#define FASTCHG_1100mA	 20
#define FASTCHG_1150mA	 21
#define FASTCHG_1200mA	 22
#define FASTCHG_1250mA	 23
#define FASTCHG_1300mA	 24
#define FASTCHG_1350mA	 25
#define FASTCHG_1400mA	 26
#define FASTCHG_1450mA	 27
#define FASTCHG_1500mA	 28
#define FASTCHG_1550mA	 29
#define FASTCHG_1600mA	 30
#define FASTCHG_1650mA	 31
#define FASTCHG_1700mA	 32
#define FASTCHG_1750mA	 33
#define FASTCHG_1800mA	 34
#define FASTCHG_1850mA	 35
#define FASTCHG_1900mA	 36
#define FASTCHG_1950mA	 37
#define FASTCHG_2000mA	 38
#define FASTCHG_2050mA	 39
#define FASTCHG_2100mA	 40
#define FASTCHG_2150mA	 41
#define FASTCHG_2200mA	 42
#define FASTCHG_2250mA	 43
#define FASTCHG_2300mA	 44
#define FASTCHG_2350mA	 45
#define FASTCHG_2400mA	 46
#define FASTCHG_2450mA	 47
#define FASTCHG_2500mA	 48


#define VBUSLIMIT_100mA	   0
#define VBUSLIMIT_150mA	   1
#define VBUSLIMIT_200mA	   2
#define VBUSLIMIT_250mA	   3
#define VBUSLIMIT_300mA	   4
#define VBUSLIMIT_350mA	   5
#define VBUSLIMIT_400mA	   6
#define VBUSLIMIT_450mA	   7
#define VBUSLIMIT_500mA	   8
#define VBUSLIMIT_550mA	   9
#define VBUSLIMIT_600mA	  10
#define VBUSLIMIT_650mA	  11
#define VBUSLIMIT_700mA	  12
#define VBUSLIMIT_750mA	  13
#define VBUSLIMIT_800mA	  14
#define VBUSLIMIT_850mA	  15
#define VBUSLIMIT_900mA	  16
#define VBUSLIMIT_950mA	  17
#define VBUSLIMIT_1000mA	 18
#define VBUSLIMIT_1050mA	 19
#define VBUSLIMIT_1100mA	 20
#define VBUSLIMIT_1150mA	 21
#define VBUSLIMIT_1200mA	 22
#define VBUSLIMIT_1250mA	 23
#define VBUSLIMIT_1300mA	 24
#define VBUSLIMIT_1350mA	 25
#define VBUSLIMIT_1400mA	 26
#define VBUSLIMIT_1450mA	 27
#define VBUSLIMIT_1500mA	 28
#define VBUSLIMIT_1550mA	 29
#define VBUSLIMIT_1600mA	 30
#define VBUSLIMIT_1650mA	 31
#define VBUSLIMIT_1700mA	 32
#define VBUSLIMIT_1750mA	 33
#define VBUSLIMIT_1800mA	 34
#define VBUSLIMIT_1850mA	 35
#define VBUSLIMIT_1900mA	 36
#define VBUSLIMIT_1950mA	 37
#define VBUSLIMIT_2000mA	 38
#define VBUSLIMIT_2050mA	 39


#define AICL_THRESHOLD_4_3_V		 0
#define AICL_THRESHOLD_4_4_V		 1
#define AICL_THRESHOLD_4_5_V		 2
#define AICL_THRESHOLD_4_6_V		 3
#define AICL_THRESHOLD_4_7_V		 4
#define AICL_THRESHOLD_4_8_V		 5
#define AICL_THRESHOLD_4_9_V		 6
#define AICL_THRESHOLD_MASK	   0x0F


#define AUTOSTOP_EN	 (1)
#define AUTOSTOP_DIS	 (0)

#define AICL_EN		 (1)
#define AICL_DIS		 (0)

#define PRECHG_150mA		 0
#define PRECHG_250mA		 1
#define PRECHG_350mA		 2
#define PRECHG_450mA		 3
#define PRECHG_MASK	   0xFC


#define BATREG_4_1_0_0_V	 0
#define BATREG_4_1_2_5_V	 1
#define BATREG_4_1_5_0_V	 2
#define BATREG_4_1_7_5_V	 3
#define BATREG_4_2_0_0_V	 4
#define BATREG_4_2_2_5_V	 5
#define BATREG_4_2_5_0_V	 6
#define BATREG_4_2_7_5_V	 7
#define BATREG_4_3_0_0_V	 8
#define BATREG_4_3_2_5_V	 9
#define BATREG_4_3_5_0_V	10
#define BATREG_4_3_7_5_V	11
#define BATREG_4_4_0_0_V	12
#define BATREG_4_4_2_5_V	13
#define BATREG_4_4_5_0_V	14
#define BATREG_4_4_7_5_V	15
#define BATREG_MASK	   0x0F


#define WEAKBAT_3_0_0_V	 0
#define WEAKBAT_3_0_5_V	 1
#define WEAKBAT_3_1_0_V	 2
#define WEAKBAT_3_1_5_V	 3
#define WEAKBAT_3_2_0_V	 4
#define WEAKBAT_3_2_5_V	 5
#define WEAKBAT_3_3_0_V	 6
#define WEAKBAT_3_3_5_V	 7
#define WEAKBAT_3_4_0_V	 8
#define WEAKBAT_3_4_5_V	 9
#define WEAKBAT_3_5_0_V	10
#define WEAKBAT_3_5_5_V	11
#define WEAKBAT_3_6_0_V	12
#define WEAKBAT_3_6_5_V	13
#define WEAKBAT_3_7_0_V	14
#define WEAKBAT_3_7_5_V	15
#define WEAKBAT_MASK	 0xF0


#define TOPOFF_100mA	   0
#define TOPOFF_150mA	   1
#define TOPOFF_200mA	   2
#define TOPOFF_250mA	   3
#define TOPOFF_300mA	   4
#define TOPOFF_350mA	   5
#define TOPOFF_400mA	   6
#define TOPOFF_450mA	   7
#define TOPOFF_500mA	   8
#define TOPOFF_550mA	   9
#define TOPOFF_600mA	  10
#define TOPOFF_650mA	  11
#define TOPOFF_MASK	 0x07


#define DISCHARGELIMIT_DISABLED	0
#define DISCHARGELIMIT_2_0_A	   1
#define DISCHARGELIMIT_2_5_A	   2
#define DISCHARGELIMIT_3_0_A	   3
#define DISCHARGELIMIT_3_5_A	   4
#define DISCHARGELIMIT_4_0_A	   5
#define DISCHARGELIMIT_4_5_A	   6
#define DISCHARGELIMIT_5_0_A	   7
#define DISCHARGELIMIT_MASK	 0xF8


#define VOTG_5_0_V	  0
#define VOTG_5_1_V	  1
#define VOTG_5_2_V	  2
#define VOTG_MASK	0x0F


#define FASTTIMER_3_5_HOUR	  0
#define FASTTIMER_4_5_HOUR	  1
#define FASTTIMER_5_5_HOUR	  2
#define FASTTIMER_DISABLED	  3
#define FASTTIMER_MASK	   0xF3


#define TOPOFFTIMER_10MIN	   0
#define TOPOFFTIMER_20MIN	   1
#define TOPOFFTIMER_30MIN	   2
#define TOPOFFTIMER_45MIN	   3
#define TOPOFFTIMER_MASK	 0xFC


#define CHARGE_EN 1
#define CHARGE_DIS 0

#define ENBOOST_EN 1
#define ENBOOST_DIS 0

#define ENCOMPARATOR_EN 1
#define ENCOMPARATOR_DIS 0

#define SUSPEND_EN 1
#define SUSPEND_DIS 0

int ValSOC = 0;

enum {
	USER	= BIT(0),
	THERMAL = BIT(1),
	CURRENT = BIT(2),
	SOC = BIT(3),
};

struct sm5414_regulator {
	struct regulator_desc   rdesc;
	struct regulator_dev	*rdev;
};

struct sm5414_charger {
	struct i2c_client   *client;
	struct device	   *dev;

	bool			iterm_disabled;
	int		 iterm_ma;
	int		 vfloat_mv;
	int		 chg_valid_gpio;
	int		 chg_valid_act_low;
	int		 chg_present;
	int		 fake_battery_soc;
	bool			using_pmic_therm;
	bool			battery_missing;
	const char	  *bms_psy_name;
	bool			resume_completed;
	bool			irq_waiting;
	struct mutex		read_write_lock;
	struct mutex		path_suspend_lock;
	struct mutex		irq_complete;

	int		 irq_gpio;
	int		 chgen_gpio;
	int		 nshdn_gpio;
	int		 charging_disabled;
	int		 fastchg_current_max_ma;
	unsigned int		cool_bat_ma;
	unsigned int		warm_bat_ma;
	unsigned int		cool_bat_mv;
	unsigned int		warm_bat_mv;

	/* debugfs related */
#if defined(CONFIG_DEBUG_FS)
	struct dentry	   *debug_root;
	u32		 peek_poke_address;
#endif
	/* status tracking */
	bool			batt_full;
	bool			batt_hot;
	bool			batt_cold;
	bool			batt_warm;
	bool			batt_cool;
	bool			jeita_supported;
	int		 charging_disabled_status;
	int		 usb_suspended;

	/* power supply */
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;
	struct power_supply batt_psy;

	/* otg 5V regulator */
	struct sm5414_regulator otg_vreg;

	/* adc_tm paramters */
	struct qpnp_vadc_chip   *vadc_dev;
	struct qpnp_adc_tm_chip *adc_tm_dev;
	struct qpnp_adc_tm_btm_param	adc_param;
	int		 cold_bat_decidegc;
	int		 hot_bat_decidegc;
	int		 cool_bat_decidegc;
	int		 warm_bat_decidegc;
	int		 bat_present_decidegc;
	/* i2c pull up regulator */
	struct regulator	*vcc_i2c;
};

/* add supplied to "bms" function */
static char *pm_batt_supplied_to[] = {
	"bms",
};

struct sm5414_charger *pSm5414;

static int __sm5414_read_reg(struct sm5414_charger *chip, u8 reg, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(chip->client, reg);
	if (ret < 0) {
		dev_err(chip->dev,
			"i2c read fail: can't read from %02x: %d\n", reg, ret);
		return ret;
	} else {
		*val = ret;
	}

	return 0;
}

static int __sm5414_write_reg(struct sm5414_charger *chip, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		dev_err(chip->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static int sm5414_read_reg(struct sm5414_charger *chip, int reg,
						u8 *val)
{
	int rc;

	mutex_lock(&chip->read_write_lock);
	rc = __sm5414_read_reg(chip, reg, val);
	mutex_unlock(&chip->read_write_lock);

	return rc;
}

static int sm5414_write_reg(struct sm5414_charger *chip, int reg,
						u8 val)
{
	int rc;

	mutex_lock(&chip->read_write_lock);
	rc = __sm5414_write_reg(chip, reg, val);
	mutex_unlock(&chip->read_write_lock);

	return rc;
}

static int sm5414_masked_write(struct sm5414_charger *chip, int reg,
							u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	mutex_lock(&chip->read_write_lock);
	rc = __sm5414_read_reg(chip, reg, &temp);
	if (rc) {
		dev_err(chip->dev,
			"sm5414_read_reg Failed: reg=%03X, rc=%d\n", reg, rc);
		goto out;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = __sm5414_write_reg(chip, reg, temp);
	if (rc) {
		dev_err(chip->dev,
			"sm5414_write Failed: reg=%03X, rc=%d\n", reg, rc);
	}
out:
	mutex_unlock(&chip->read_write_lock);
	return rc;
}

static void dump_regs(struct sm5414_charger *chip)
{
	int rc;
	u8 reg;
	u8 addr;

	for (addr = SM5414_INTMASK1; addr <= SM5414_CHGCTRL5; addr++) {
		rc = sm5414_read_reg(chip, addr, &reg);
		if (rc)
			pr_debug("Couldn't read 0x%02x rc = %d\n",
					addr, rc);
		else
			pr_debug("0x%02x = 0x%02x\n", addr, reg);
	}
}


static int disable_software_temp_monitor;
int dis_sof_temp_monitor_set(const char *val, const struct kernel_param *kp)
{
	if (!val)
		val = "1";
	return strtobool(val, kp->arg);
}

int dis_sof_temp_monitor_get(char *buffer, const struct kernel_param *kp)
{
	disable_software_temp_monitor = 1;
	return sprintf(buffer, "%c", *(bool *)kp->arg ? 'Y' : 'N');
}

static struct kernel_param_ops dis_sof_temp_monitor_ops = {
	.set = dis_sof_temp_monitor_set,
	.get = dis_sof_temp_monitor_get,
};

module_param_cb(disable_software_temp_monitor, &dis_sof_temp_monitor_ops
							, &disable_software_temp_monitor, 0644);

MODULE_PARM_DESC(debug, "1:disable software temp monitor , 0:enable, default:0");

static int sm5414_fastchg_current_set(struct sm5414_charger *chip,
					unsigned int fastchg_current)
{
	int i;

	if ((fastchg_current < SM5414_FAST_CHG_MIN_MA) ||
		(fastchg_current >  SM5414_FAST_CHG_MAX_MA)) {
		dev_dbg(chip->dev, "bad fastchg current mA=%d asked to set\n",
						fastchg_current);
		return -EINVAL;
	}


	for (i = 0x3F; i >= 0; i--) {
		if (((100 + i*50)) <= fastchg_current)
			break;
	}

	if (i < 0) {
		dev_err(chip->dev, "Invalid current setting %dmA\n",
						fastchg_current);
		i = 0;
	}

	i = i << SM5414_CHGCTRL2_FASTCHG_SHIFT;
	dev_dbg(chip->dev, "fastchg limit=%d setting %02x\n",
					fastchg_current, i);

	return sm5414_masked_write(chip, SM5414_CHGCTRL2,
				SM5414_CHGCTRL2_FASTCHG_MASK, i);
}

#define MIN_FLOAT_MV		4100
#define MAX_FLOAT_MV		4475
#define VFLOAT_STEP_MV		25

static int sm5414_float_voltage_set(struct sm5414_charger *chip, int vfloat_mv)
{
	u8 temp;

	if ((vfloat_mv < MIN_FLOAT_MV) || (vfloat_mv > MAX_FLOAT_MV)) {
		dev_err(chip->dev, "bad float voltage mv =%d asked to set\n",
					vfloat_mv);
		return -EINVAL;
	}

	temp = (vfloat_mv - MIN_FLOAT_MV) / VFLOAT_STEP_MV;
	temp = temp << SM5414_CHGCTRL3_BATREG_SHIFT;
	pr_debug("%s, vfloat_mv=%d, temp=%d\n", __func__, vfloat_mv, temp);
	return sm5414_masked_write(chip, SM5414_CHGCTRL3, SM5414_CHGCTRL3_BATREG_MASK << SM5414_CHGCTRL3_BATREG_SHIFT, temp);
}

#define MIN_TOPOFF_MA		100
#define MAX_TOPOFF_MA		650
#define CTOPOFF_STEP_MA		50

static int sm5414_term_current_set(struct sm5414_charger *chip)
{
	u8 reg = 0;
	int rc;


	if (chip->iterm_ma != -EINVAL) {
		if (chip->iterm_disabled)
			dev_err(chip->dev, "Error: Both iterm_disabled and iterm_ma set\n");

		if ((chip->iterm_ma < MIN_TOPOFF_MA) || (chip->iterm_ma > MAX_TOPOFF_MA)) {
			dev_err(chip->dev, "bad topoff current ma =%d asked to set\n",
						chip->iterm_ma);
			return -EINVAL;
		}

		reg = (chip->iterm_ma - MIN_TOPOFF_MA) / CTOPOFF_STEP_MA;
		reg = reg << SM5414_CHGCTRL4_TOPOFF_SHIFT;

		rc = sm5414_masked_write(chip, SM5414_CHGCTRL4,
							SM5414_CHGCTRL4_TOPOFF_MASK << SM5414_CHGCTRL4_TOPOFF_SHIFT, reg << SM5414_CHGCTRL4_TOPOFF_SHIFT);
		if (rc) {
			dev_err(chip->dev,
				"Couldn't set iterm rc = %d\n", rc);
			return rc;
		}
	}

	if (chip->iterm_disabled) {
		rc = sm5414_masked_write(chip, SM5414_CHGCTRL1,
					SM5414_CHGCTRL1_AUTOSTOP_MASK << SM5414_CHGCTRL1_AUTOSTOP_SHIFT,
					AUTOSTOP_DIS << SM5414_CHGCTRL1_AUTOSTOP_SHIFT);
		if (rc) {
			dev_err(chip->dev, "Couldn't set iterm rc = %d\n",
								rc);
			return rc;
		}
	} else {
		rc = sm5414_masked_write(chip, SM5414_CHGCTRL1,
					SM5414_CHGCTRL1_AUTOSTOP_MASK << SM5414_CHGCTRL1_AUTOSTOP_SHIFT, AUTOSTOP_EN << SM5414_CHGCTRL1_AUTOSTOP_SHIFT);
		if (rc) {
			dev_err(chip->dev,
				"Couldn't enable iterm rc = %d\n", rc);
			return rc;
		}
	}

	return 0;
}

static int __sm5414_charging_disable(struct sm5414_charger *chip, bool disable)
{
	int nCHG;

	pr_debug("%s : disable = %d\n", __func__, disable);


	gpio_direction_output(chip->chgen_gpio, disable ? 1 : 0);

	nCHG = gpio_get_value(chip->chgen_gpio);
	pr_debug("%s : nCHG = %d\n", __func__, nCHG);
	return 0;
}

static int sm5414_charging_disable(struct sm5414_charger *chip,
						int reason, int disable)
{
	int rc = 0;
	int disabled = 0;
	int nCHG;

	disabled = chip->charging_disabled_status;

	pr_debug("reason = %d requested_disable = %d disabled_status = %d\n",
						reason, disable, disabled);

	if (disable == true)
		disabled |= reason;
	else
		disabled &= ~reason;




	nCHG = gpio_get_value(chip->chgen_gpio);
	pr_debug("nCHG = %d disabled = %d\n", nCHG, disabled);

	if ((disabled & THERMAL) /*|| (disabled & SOC)*/) {
		pr_debug("Battery is (disabled & THERMAL) = %d or (disabled & SOC) = %d. Can't turn on charging function\n",
			(disabled & THERMAL), (disabled & SOC));
		disable = 1;
	}

	if (nCHG == disable)
		goto skip;

	rc = __sm5414_charging_disable(chip, !!disable);
	if (rc) {
		pr_debug("Failed to disable charging rc = %d\n", rc);
		return rc;
	} else {
	/* will not modify online status in this condition */
		pr_debug(" Pass charging disable rc = %d\n", rc);
		power_supply_changed(&chip->batt_psy);
	}

skip:
	chip->charging_disabled_status = disabled;
	return rc;
}


#define VFLT_300MV			0x0C
#define VFLT_200MV			0x08
#define VFLT_100MV			0x04
#define VFLT_50MV			0x00
#define VFLT_MASK			0x0C

static int sm5414_chg_otg_regulator_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct sm5414_charger *chip = rdev_get_drvdata(rdev);


	sm5414_charging_disable(chip, USER, 1);

	rc = sm5414_masked_write(chip, SM5414_CTRL, SM5414_CTRL_ENBOOST_MASK << SM5414_CTRL_ENBOOST_SHIFT, ENBOOST_EN << SM5414_CTRL_ENBOOST_SHIFT);
	if (rc)
		dev_err(chip->dev, "Couldn't enable OTG mode rc=%d, reg=%2x\n",
								rc, SM5414_CTRL);
	return rc;
}

static int sm5414_chg_otg_regulator_disable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct sm5414_charger *chip = rdev_get_drvdata(rdev);


	sm5414_charging_disable(chip, USER, 0);

	rc = sm5414_masked_write(chip, SM5414_CTRL, SM5414_CTRL_ENBOOST_MASK << SM5414_CTRL_ENBOOST_SHIFT, ENBOOST_DIS << SM5414_CTRL_ENBOOST_SHIFT);
	if (rc)
		dev_err(chip->dev, "Couldn't disable OTG mode rc=%d, reg=%2x\n",
								rc, SM5414_CTRL);
	return rc;
}

static int sm5414_chg_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	u8 reg = 0;
	struct sm5414_charger *chip = rdev_get_drvdata(rdev);

	rc = sm5414_read_reg(chip, SM5414_CTRL, &reg);
	if (rc) {
		dev_err(chip->dev,
			"Couldn't read OTG enable bit rc=%d, reg=%2x\n",
							rc, SM5414_CTRL);
		return rc;
	}

	return  (reg & SM5414_CTRL_ENBOOST_MASK) ? 1 : 0;
}

struct regulator_ops sm5414_chg_otg_reg_ops = {
	.enable	 = sm5414_chg_otg_regulator_enable,
	.disable	= sm5414_chg_otg_regulator_disable,
	.is_enabled = sm5414_chg_otg_regulator_is_enable,
};

static int sm5414_regulator_init(struct sm5414_charger *chip)
{
	int rc = 0;
	struct regulator_init_data *init_data;
	struct regulator_config cfg = {};

	init_data = of_get_regulator_init_data(chip->dev, chip->dev->of_node);
	if (!init_data) {
		dev_err(chip->dev, "Allocate memory failed\n");
		return -ENOMEM;
	}

	/* Give the name, then will register */
	if (init_data->constraints.name) {
		chip->otg_vreg.rdesc.owner = THIS_MODULE;
		chip->otg_vreg.rdesc.type = REGULATOR_VOLTAGE;
		chip->otg_vreg.rdesc.ops = &sm5414_chg_otg_reg_ops;
		chip->otg_vreg.rdesc.name = init_data->constraints.name;

		cfg.dev = chip->dev;
		cfg.init_data = init_data;
		cfg.driver_data = chip;
		cfg.of_node = chip->dev->of_node;

		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_STATUS;

		chip->otg_vreg.rdev = regulator_register(
					&chip->otg_vreg.rdesc, &cfg);
		if (IS_ERR(chip->otg_vreg.rdev)) {
			rc = PTR_ERR(chip->otg_vreg.rdev);
			chip->otg_vreg.rdev = NULL;
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev,
					"OTG reg failed, rc=%d\n", rc);
		}
	}
	return rc;
}

static int sm5414_get_prop_batt_present(struct sm5414_charger *chip)
{
	int rc;
	u8 reg = 0;

	rc = sm5414_read_reg(chip, SM5414_STATUS, &reg);

	return (reg >> SM5414_STATUS_BATDET_SHIFT) & SM5414_STATUS_BATDET_MASK;
}

#define DEFAULT_TEMP 250
static int sm5414_get_prop_batt_temp(struct sm5414_charger *chip)
{
	int rc = 0;
	struct qpnp_vadc_result results;

	if (!sm5414_get_prop_batt_present(chip))
		return DEFAULT_TEMP;

	rc = qpnp_vadc_read(chip->vadc_dev, LR_MUX1_BATT_THERM, &results);
	if (rc) {
		pr_debug("Unable to read batt temperature rc=%d\n", rc);
		return DEFAULT_TEMP;
	}
	pr_debug("get_bat_temp %d, %lld\n",
		results.adc_code, results.physical);

	return (int)results.physical;
}

static int
sm5414_get_prop_battery_voltage_now(struct sm5414_charger *chip)
{
	int rc = 0;
	struct qpnp_vadc_result results;

	rc = qpnp_vadc_read(chip->vadc_dev, VBAT_SNS, &results);
	if (rc) {
		pr_err("Unable to read vbat rc=%d\n", rc);
		return 0;
	}
	return results.physical;
}

static int sm5414_hw_init(struct sm5414_charger *chip)
{
	int rc = 0, vol_val = 0;

	/* ENCOMPARATOR */
	sm5414_masked_write(chip, SM5414_CTRL, SM5414_CTRL_ENCOMPARATOR_MASK << SM5414_CTRL_ENCOMPARATOR_SHIFT, ENCOMPARATOR_EN << SM5414_CTRL_ENCOMPARATOR_SHIFT);

	/* INTERRUPT MASK */
	sm5414_write_reg(chip, SM5414_INTMASK1, 0x1F);
	sm5414_write_reg(chip, SM5414_INTMASK2, 0xB8);
	sm5414_write_reg(chip, SM5414_INTMASK3, 0xFF);

	/* set the fast charge current limit */
	rc = sm5414_fastchg_current_set(chip, chip->fastchg_current_max_ma);
	if (rc) {
		dev_err(chip->dev, "Couldn't set fastchg current rc=%d\n", rc);
		return rc;
	}

	/* set the aiclth */
	sm5414_masked_write(chip, SM5414_CHGCTRL1, SM5414_CHGCTRL1_AICLTH_MASK << SM5414_CHGCTRL1_AICLTH_SHIFT, AICL_THRESHOLD_4_6_V << SM5414_CHGCTRL1_AICLTH_SHIFT);

	/* set the autostop */
	sm5414_masked_write(chip, SM5414_CHGCTRL1, SM5414_CHGCTRL1_AUTOSTOP_MASK << SM5414_CHGCTRL1_AUTOSTOP_SHIFT, AUTOSTOP_DIS << SM5414_CHGCTRL1_AUTOSTOP_SHIFT);

	/* set the float voltage */
	rc = sm5414_float_voltage_set(chip, chip->vfloat_mv);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't set float voltage rc = %d\n", rc);
		return rc;
	}

	/* set iterm */
	rc = sm5414_term_current_set(chip);
	if (rc)
		dev_err(chip->dev, "Couldn't set term current rc=%d\n", rc);

	vol_val = sm5414_get_prop_battery_voltage_now(chip);

	pr_debug("%s : voltage now = %d\n", __func__, vol_val);


	/* enable/disable charging */
	if (chip->charging_disabled) {
		rc = sm5414_charging_disable(chip, USER, 1);
		if (rc)
			dev_err(chip->dev, "Couldn't '%s' charging rc = %d\n",
			chip->charging_disabled ? "disable" : "enable", rc);
	} else {
		/*
		 * Enable charging explictly,
		 * because not sure the default behavior.
		 */
		rc = __sm5414_charging_disable(chip, 0);
		if (rc)
			dev_err(chip->dev, "Couldn't enable charging\n");
	}

	return rc;
}

static enum power_supply_property sm5414_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int sm5414_get_prop_batt_capacity(struct sm5414_charger *chip)
{
	union power_supply_propval ret = {0,};

	if (chip->fake_battery_soc >= 0)
		return chip->fake_battery_soc;

	if (chip->bms_psy) {
		chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		return ret.intval;
	}

	dev_dbg(chip->dev,
		"Couldn't get bms_psy, return default capacity\n");
	return SM5414_DEFAULT_BATT_CAPACITY;
}

static int sm5414_get_prop_batt_status(struct sm5414_charger *chip)
{
	int rc;
	u8 reg = 0;
	int nCHG = 0;

	pr_debug("%s : chip->batt_full = %d, ValSOC = %d\n", __func__, chip->batt_full, ValSOC);

	if (chip->batt_full)
		pr_debug("%s : chip->batt_full\n", __func__);

	if (ValSOC >= 100)
		pr_debug("%s : ValSOC >= 100\n", __func__);

	if ((chip->batt_full == 1) && (ValSOC >= 100))
		pr_debug("%s : chip->batt_full, ValSOC\n", __func__);


	if ((chip->batt_full == 1) && (ValSOC >= 100))
		return POWER_SUPPLY_STATUS_FULL;

	rc = sm5414_read_reg(chip, SM5414_STATUS, &reg);
	if (rc) {
		dev_err(chip->dev, "Couldn't read STAT_C rc = %d\n", rc);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}
	nCHG = gpio_get_value(chip->chgen_gpio);
	dev_dbg(chip->dev, "%s: SM5414_STATUS=0x%x, nCHG = %d\n", __func__, reg, nCHG);

	if ((reg & (SM5414_STATUS_VBUSOVP_MASK << SM5414_STATUS_VBUSOVP_SHIFT)) || (reg & (SM5414_STATUS_THEMSHDN_MASK << SM5414_STATUS_THEMSHDN_SHIFT)))
		return POWER_SUPPLY_STATUS_NOT_CHARGING;

	if (!(reg & (SM5414_STATUS_VBUSUVLO_MASK << SM5414_STATUS_VBUSUVLO_SHIFT)) && (nCHG == 0))
		return POWER_SUPPLY_STATUS_CHARGING;

	return POWER_SUPPLY_STATUS_DISCHARGING;
}

static int get_prop_current_now(struct sm5414_charger *chip)
{
	union power_supply_propval ret = {0,};

	if (chip->bms_psy) {
		chip->bms_psy->get_property(chip->bms_psy,
			  POWER_SUPPLY_PROP_CURRENT_NOW, &ret);
		return ret.intval;
	} else {
		pr_debug("No BMS supply registered return 0\n");
	}

	return 0;
}

static int sm5414_get_prop_batt_health(struct sm5414_charger *chip)
{
	union power_supply_propval ret = {0,};

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

static int sm5414_set_usb_chg_current(struct sm5414_charger *chip,
		int current_ma)
{
	int i, rc = 0;

	dev_err(chip->dev, "%s: USB current_ma = %d\n", __func__, current_ma);

	if (current_ma < USB3_MIN_CURRENT_MA && current_ma != 2)
		current_ma = USB2_MIN_CURRENT_MA;

	if (current_ma == USB2_MIN_CURRENT_MA) {
		/* USB 2.0 - 100mA */
		i = VBUSLIMIT_100mA;
	} else if (current_ma == USB2_MAX_CURRENT_MA) {
		/* USB 2.0 - 500mA */
		i = VBUSLIMIT_500mA;
	} else if (current_ma == USB3_MAX_CURRENT_MA) {
		/* USB 3.0 - 900mA */
		i = VBUSLIMIT_900mA;
	} else if (current_ma > USB2_MAX_CURRENT_MA) {
		/* HC mode  - if none of the above */


		for (i = 0x27; i >= 0; i--) {
			if (((100 + i*50)) <= current_ma)
				break;
		}
		if (i < 0) {
			dev_err(chip->dev, "Cannot find %dmA\n", current_ma);
			i = 0;
		}
	}

	rc = sm5414_masked_write(chip, SM5414_VBUSCTRL,
					SM5414_VBUSCTRL_VBUSLIMIT_MASK, i);
	if (rc)
		dev_err(chip->dev, "Couldn't set input mA rc=%d\n", rc);

	return rc;
}

static int
sm5414_batt_property_is_writeable(struct power_supply *psy,
					enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CAPACITY:
		return 1;
	default:
		break;
	}

	return 0;
}

static int bound_soc(int soc)
{
	soc = max(0, soc);
	soc = min(soc, 100);
	return soc;
}

static int sm5414_battery_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	int rc;
	struct sm5414_charger *chip = container_of(psy,
				struct sm5414_charger, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:

		switch (val->intval) {
		case POWER_SUPPLY_STATUS_FULL:
			pr_debug("%s : POWER_SUPPLY_PROP_STATUS / POWER_SUPPLY_STATUS_FULL\n", __func__);

			rc = sm5414_charging_disable(chip, SOC, true);
			if (rc < 0) {
				dev_err(chip->dev,
					"Couldn't set charging disable rc = %d\n", rc);
			} else {
				chip->batt_full = true;
				pr_debug("status = FULL, batt_full = %d\n",
							chip->batt_full);
			}

			break;
		case POWER_SUPPLY_STATUS_DISCHARGING:
			pr_debug("%s : POWER_SUPPLY_PROP_STATUS / POWER_SUPPLY_STATUS_DISCHARGING\n", __func__);
			chip->batt_full = false;
			power_supply_changed(&chip->batt_psy);
			dev_dbg(chip->dev, "status = DISCHARGING, batt_full = %d\n",
							chip->batt_full);
			break;
		case POWER_SUPPLY_STATUS_CHARGING:
			pr_debug("%s : POWER_SUPPLY_PROP_STATUS / POWER_SUPPLY_STATUS_CHARGING\n", __func__);
			rc = sm5414_charging_disable(chip, SOC, false);
			if (rc < 0) {
				dev_err(chip->dev,
				"Couldn't set charging disable rc = %d\n",
								rc);
			} else {
				chip->batt_full = false;
				dev_dbg(chip->dev, "status = CHARGING, batt_full = %d\n",
							chip->batt_full);
			}
			break;
		default:
			pr_debug("%s : POWER_SUPPLY_PROP_STATUS / default\n", __func__);
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		pr_debug("%s : POWER_SUPPLY_PROP_CHARGING_ENABLED : val->intval = %d\n", __func__, val->intval);
		sm5414_charging_disable(chip, USER, !val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		pr_debug("%s : POWER_SUPPLY_PROP_CHARGING_ENABLED : val->intval = %d\n", __func__, val->intval);
		chip->fake_battery_soc = bound_soc(val->intval);
		power_supply_changed(&chip->batt_psy);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sm5414_battery_get_property(struct power_supply *psy,
					   enum power_supply_property prop,
					   union power_supply_propval *val)
{
	struct sm5414_charger *chip = container_of(psy,
				struct sm5414_charger, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = sm5414_get_prop_batt_status(chip);
		pr_debug("%s : POWER_SUPPLY_PROP_STATUS : val->intval = %d\n", __func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = sm5414_get_prop_batt_present(chip);
		pr_debug("%s : POWER_SUPPLY_PROP_PRESENT : val->intval = %d\n", __func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = sm5414_get_prop_batt_capacity(chip);
		pr_debug("%s : POWER_SUPPLY_PROP_CAPACITY : val->intval = %d\n", __func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = !(chip->charging_disabled_status & USER);
		pr_debug("%s : POWER_SUPPLY_PROP_CHARGING_ENABLED : val->intval = %d\n", __func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = sm5414_get_prop_batt_health(chip);
		pr_debug("%s : POWER_SUPPLY_PROP_HEALTH : val->intval = %d\n", __func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		pr_debug("%s : POWER_SUPPLY_PROP_TECHNOLOGY : val->intval = %d\n", __func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "SM5414";
		pr_debug("%s : POWER_SUPPLY_PROP_MODEL_NAME : val->strval = %s\n", __func__, val->strval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = sm5414_get_prop_batt_temp(chip);
		pr_debug("%s : POWER_SUPPLY_PROP_TEMP : val->intval = %d\n", __func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = sm5414_get_prop_battery_voltage_now(chip);
		pr_debug("%s : POWER_SUPPLY_PROP_VOLTAGE_NOW : val->intval = %d\n", __func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_prop_current_now(chip);
		pr_debug("%s : POWER_SUPPLY_PROP_CURRENT_NOW : val->intval = %d\n", __func__, val->intval);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}


static int vbusinok_complete(struct sm5414_charger *chip, u8 status)
{
	u8 val = 0;

	pr_debug("%s\n", __func__);
	sm5414_read_reg(chip, SM5414_CTRL, &val);

	if (val & SM5414_CTRL_ENBOOST_MASK)
	   pr_debug("%s : OTG\n", __func__);
	else {
		chip->chg_present = !!status;

		pr_debug("complete. Charger chg_present=%d\n",
							chip->chg_present);



		pr_debug("%s updating usb_psy present=%d\n", __func__,
				chip->chg_present);
		power_supply_set_present(chip->usb_psy, chip->chg_present);

		sm5414_charging_disable(chip, USER, 0);
	}

	return 0;
}

static int chg_uv(struct sm5414_charger *chip, u8 status)
{
	pr_debug("%s\n", __func__);

	if (status != 0) {
		chip->chg_present = false;
		dev_dbg(chip->dev, "%s updating usb_psy present=%d",
				__func__, chip->chg_present);
	/* we can't set usb_psy as UNKNOWN here, will lead USERSPACE issue */
		power_supply_set_present(chip->usb_psy, chip->chg_present);
	}

	chip->batt_full = !status;

	power_supply_changed(chip->usb_psy);

	sm5414_charging_disable(chip, USER, 1);

	dev_dbg(chip->dev, "chip->chg_present = %d\n", chip->chg_present);

	return 0;
}

static int chg_ov(struct sm5414_charger *chip, u8 status)
{
	u8 psy_health_sts;

	pr_debug("%s\n", __func__);

	if (status)
		psy_health_sts = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else
		psy_health_sts = POWER_SUPPLY_HEALTH_GOOD;

	chip->batt_full = !status;

	power_supply_set_health_state(
				chip->usb_psy, psy_health_sts);
	power_supply_changed(chip->usb_psy);

	return 0;
}

static int chg_term(struct sm5414_charger *chip, u8 status)
{
	dev_dbg(chip->dev, "%s\n", __func__);
	pr_debug("%s\n", __func__);

	chip->batt_full = !!status;
	return 0;
}

static int chg_recharge(struct sm5414_charger *chip, u8 status)
{
	dev_dbg(chip->dev, "%s, status = %d\n", __func__, !!status);

	/* to check the status mean */
	chip->batt_full = !status;

	return 0;
}

static void sm5414_chg_set_appropriate_battery_current(
				struct sm5414_charger *chip)
{
	int rc;
	unsigned int current_max = chip->fastchg_current_max_ma;

	pr_debug("%s, batt_cool=%d, batt_warm=%d, current_max=%d, cool_bat_ma=%d, chip->warm_bat_ma=%d\n",
		__func__, chip->batt_cool, chip->batt_warm, current_max, chip->cool_bat_ma, chip->warm_bat_ma);

	if (chip->batt_cool)
		current_max =
			min(current_max, chip->cool_bat_ma);
	if (chip->batt_warm)
		current_max =
			min(current_max, chip->warm_bat_ma);
	pr_debug("setting %dmA", current_max);
	rc = sm5414_fastchg_current_set(chip, current_max);
	if (rc)
		dev_err(chip->dev, "Couldn't set charging current rc = %d\n", rc);
}

static void sm5414_chg_set_appropriate_vddmax(
				struct sm5414_charger *chip)
{
	int rc;
	unsigned int vddmax = chip->vfloat_mv;

	pr_debug("%s\n", __func__);

	if (chip->batt_cool)
		vddmax = min(vddmax, chip->cool_bat_mv);
	if (chip->batt_warm)
		vddmax = min(vddmax, chip->warm_bat_mv);

	pr_debug("setting %dmV\n", vddmax);
	rc = sm5414_float_voltage_set(chip, vddmax);
	if (rc)
		pr_debug("Couldn't set float voltage rc = %d\n", rc);
}

static void sm_chg_adc_notification(enum qpnp_tm_state state, void *ctx)
{
	struct sm5414_charger *chip = ctx;
	bool bat_hot = 0, bat_cold = 0, bat_present = 0, bat_warm = 0,
							bat_cool = 0;
	int temp;

	pr_debug("%s\n", __func__);

	if (state >= ADC_TM_STATE_NUM) {
		pr_err("invallid state parameter %d\n", state);
		return;
	}

	temp = sm5414_get_prop_batt_temp(chip);

	pr_debug("temp = %d state = %s\n", temp,
				state == ADC_TM_WARM_STATE ? "hot" : "cold");

	if (state == ADC_TM_WARM_STATE) {
		if (temp >= chip->hot_bat_decidegc) {
			bat_hot = true;
			bat_warm = false;
			bat_cold = false;
			bat_cool = false;
			bat_present = true;

			chip->adc_param.low_temp =
				chip->hot_bat_decidegc;
			chip->adc_param.state_request =
				ADC_TM_COOL_THR_ENABLE;
		} else if (temp >=
			chip->warm_bat_decidegc && chip->jeita_supported) {
			bat_hot = false;
			bat_warm = true;
			bat_cold = false;
			bat_cool = false;
			bat_present = true;

			chip->adc_param.low_temp =
				chip->warm_bat_decidegc;
			chip->adc_param.high_temp =
				chip->hot_bat_decidegc;
		} else if (temp >=
			chip->cool_bat_decidegc && chip->jeita_supported) {
			bat_hot = false;
			bat_warm = false;
			bat_cold = false;
			bat_cool = false;
			bat_present = true;

			chip->adc_param.low_temp =
				chip->cool_bat_decidegc;
			chip->adc_param.high_temp =
				chip->warm_bat_decidegc;
		} else if (temp >=
			chip->cold_bat_decidegc) {
			bat_hot = false;
			bat_warm = false;
			bat_cold = false;
			bat_cool = true;
			bat_present = true;

			chip->adc_param.low_temp = chip->cold_bat_decidegc;
			if (chip->jeita_supported)
				chip->adc_param.high_temp =
						chip->cool_bat_decidegc;
			else
				chip->adc_param.high_temp =
						chip->hot_bat_decidegc;
			chip->adc_param.state_request =
					ADC_TM_HIGH_LOW_THR_ENABLE;
		} else if (temp >= chip->bat_present_decidegc) {
			bat_hot = false;
			bat_warm = false;
			bat_cold = true;
			bat_cool = false;
			bat_present = true;

			chip->adc_param.high_temp = chip->cold_bat_decidegc;
			chip->adc_param.low_temp = chip->bat_present_decidegc;
			chip->adc_param.state_request =
					ADC_TM_HIGH_LOW_THR_ENABLE;
		}
	} else {
		if (temp <= chip->bat_present_decidegc) {
			bat_cold = true;
			bat_cool = false;
			bat_hot = false;
			bat_warm = false;
			bat_present = false;
			chip->adc_param.high_temp =
				chip->bat_present_decidegc;
			chip->adc_param.state_request =
				ADC_TM_WARM_THR_ENABLE;
		} else if (temp <= chip->cold_bat_decidegc) {
			bat_hot = false;
			bat_warm = false;
			bat_cold = true;
			bat_cool = false;
			bat_present = true;
			chip->adc_param.high_temp =
				chip->cold_bat_decidegc;
			/* add low_temp to enable batt present check */
			chip->adc_param.low_temp =
				chip->bat_present_decidegc;
			chip->adc_param.state_request =
				ADC_TM_HIGH_LOW_THR_ENABLE;
		} else if (temp <= chip->cool_bat_decidegc &&
					chip->jeita_supported) {
			bat_hot = false;
			bat_warm = false;
			bat_cold = false;
			bat_cool = true;
			bat_present = true;
			chip->adc_param.high_temp =
				chip->cool_bat_decidegc;
			chip->adc_param.low_temp =
				chip->cold_bat_decidegc;
			chip->adc_param.state_request =
				ADC_TM_HIGH_LOW_THR_ENABLE;
		} else if (temp <= chip->warm_bat_decidegc &&
					chip->jeita_supported) {
			bat_hot = false;
			bat_warm = false;
			bat_cold = false;
			bat_cool = false;
			bat_present = true;
			chip->adc_param.high_temp =
				chip->warm_bat_decidegc;
			chip->adc_param.low_temp =
				chip->cool_bat_decidegc;
			chip->adc_param.state_request =
				ADC_TM_HIGH_LOW_THR_ENABLE;
		} else if (temp <= chip->hot_bat_decidegc) {
			bat_hot = false;
			bat_warm = true;
			bat_cold = false;
			bat_cool = false;
			bat_present = true;
			if (chip->jeita_supported)
				chip->adc_param.low_temp =
					chip->warm_bat_decidegc;
			else
				chip->adc_param.low_temp =
					chip->cold_bat_decidegc;
			chip->adc_param.high_temp = chip->hot_bat_decidegc;
			chip->adc_param.state_request =
					ADC_TM_HIGH_LOW_THR_ENABLE;
		}
	}

	if (bat_present)
		chip->battery_missing = false;
	else
		chip->battery_missing = true;

	if (bat_hot ^ chip->batt_hot || bat_cold ^ chip->batt_cold) {
		chip->batt_hot = bat_hot;
		chip->batt_cold = bat_cold;
		/* stop charging explicitly since we use PMIC thermal pin*/
		if ((bat_hot || bat_cold || chip->battery_missing) && !disable_software_temp_monitor)
			sm5414_charging_disable(chip, THERMAL, 1);
		else
			sm5414_charging_disable(chip, THERMAL, 0);
	}

	if ((chip->batt_warm ^ bat_warm || chip->batt_cool ^ bat_cool)
						&& chip->jeita_supported) {
		chip->batt_warm = bat_warm;
		chip->batt_cool = bat_cool;
		sm5414_chg_set_appropriate_battery_current(chip);
		sm5414_chg_set_appropriate_vddmax(chip);
	}

	pr_debug("hot %d, cold %d, warm %d, cool %d, jeita supported %d, missing %d, low = %d deciDegC, high = %d deciDegC\n",
		chip->batt_hot, chip->batt_cold, chip->batt_warm,
		chip->batt_cool, chip->jeita_supported, chip->battery_missing,
		chip->adc_param.low_temp, chip->adc_param.high_temp);
	if (qpnp_adc_tm_channel_measure(chip->adc_tm_dev, &chip->adc_param))
		pr_debug("request ADC error\n");
}

static int battery_missing(struct sm5414_charger *chip, u8 status)
{
	pr_debug("%s\n", __func__);

	chip->battery_missing = !!status;
	return 0;
}

static int sm5414_chg_read_init(void *dev_id)
{
	struct sm5414_charger *chip = dev_id;

	u8 val1 = 0, val2 = 0, val3  = 0;
	u8 status = 0;

	pr_debug("%s : Interrup init read start\n", __func__);

	sm5414_read_reg(chip, SM5414_INT1, &val1);
	sm5414_read_reg(chip, SM5414_INT2, &val2);
	sm5414_read_reg(chip, SM5414_INT3, &val3);
	sm5414_read_reg(chip, SM5414_STATUS, &status);

	pr_debug("Init read : SM5414_INT1 = 0x%x\n", val1);
	pr_debug("Init read : SM5414_INT2 = 0x%x\n", val2);
	pr_debug("Init read : SM5414_INT3 = 0x%x\n", val3);
	pr_debug("Init read : SM5414_STATUS = 0x%x\n", status);

	pr_debug("%s : Interrup init read done\n", __func__);

	if ((status & (SM5414_STATUS_VBUSUVLO_MASK << SM5414_STATUS_VBUSUVLO_SHIFT))
		|| (status & (SM5414_STATUS_VBUSOVP_MASK << SM5414_STATUS_VBUSOVP_SHIFT))) {
		vbusinok_complete(chip, 0);
		chg_uv(chip, 1);
		pr_debug("%s : VBUSUVLO/OVP\n", __func__);
	} else {
	   vbusinok_complete(chip, 1);
	   pr_debug("%s : VBUSOK\n", __func__);
	}

	power_supply_changed(&chip->batt_psy);

	return 0;
}


static irqreturn_t sm5414_chg_stat_handler(int irq, void *dev_id)
{
	struct sm5414_charger *chip = dev_id;

	u8 val1 = 0, val2 = 0, val3  = 0;
	u8 status = 0;

	mutex_lock(&chip->irq_complete);

	pr_debug("%s\n", __func__);

	chip->irq_waiting = true;
	if (!chip->resume_completed) {
		pr_debug("IRQ triggered before device-resume\n");
		disable_irq_nosync(irq);
		mutex_unlock(&chip->irq_complete);
		return IRQ_HANDLED;
	}
	chip->irq_waiting = false;

	sm5414_read_reg(chip, SM5414_INT1, &val1);
	sm5414_read_reg(chip, SM5414_INT2, &val2);
	sm5414_read_reg(chip, SM5414_INT3, &val3);
	sm5414_read_reg(chip, SM5414_STATUS, &status);

	pr_debug("SM5414_INT1 = 0x%x\n", val1);
	pr_debug("SM5414_INT2 = 0x%x\n", val2);
	pr_debug("SM5414_INT3 = 0x%x\n", val3);
	pr_debug("SM5414_STATUS = 0x%x\n", status);

	if ((val1 & SM5414_INT1_VBUSINOK)
			&& !(status & (SM5414_STATUS_VBUSUVLO_MASK << SM5414_STATUS_VBUSUVLO_SHIFT))
			&& !(status & (SM5414_STATUS_VBUSOVP_MASK << SM5414_STATUS_VBUSOVP_SHIFT))) {
		vbusinok_complete(chip, 1);
		power_supply_changed(&chip->batt_psy);
	} else if ((val1 & SM5414_INT1_VBUSUVLO)
			&& (status & (SM5414_STATUS_VBUSUVLO_MASK << SM5414_STATUS_VBUSUVLO_SHIFT))) {
		vbusinok_complete(chip, 0);
		chg_uv(chip, 1);
	} else if ((val1 & SM5414_INT1_VBUSOVP)
			&& (status & (SM5414_STATUS_VBUSOVP_MASK << SM5414_STATUS_VBUSOVP_SHIFT))) {
		vbusinok_complete(chip, 0);
		chg_ov(chip, 1);
	}

	if ((val2 & SM5414_INT2_TOPOFF)
			&& (status & (SM5414_STATUS_TOPOFF_MASK << SM5414_STATUS_TOPOFF_SHIFT))) {
		chg_term(chip, 1);
		power_supply_changed(&chip->batt_psy);
	} else if ((val2 & SM5414_INT2_CHGRSTF)
			&& !(status & (SM5414_STATUS_TOPOFF_MASK << SM5414_STATUS_TOPOFF_SHIFT)))
		chg_recharge(chip, 1);

	if ((val2 & SM5414_INT2_NOBAT)
			&& !(status & (SM5414_STATUS_BATDET_MASK << SM5414_STATUS_BATDET_SHIFT)))
		battery_missing(chip, 1);

	pr_debug("batt psy changed\n");


	dump_regs(chip);

	mutex_unlock(&chip->irq_complete);

	return IRQ_HANDLED;
}

static irqreturn_t sm5414_chg_valid_handler(int irq, void *dev_id)
{
	struct sm5414_charger *chip = dev_id;
	int present;

	present = gpio_get_value_cansleep(chip->chg_valid_gpio);
	if (present < 0) {
		dev_err(chip->dev, "Couldn't read chg_valid gpio=%d\n",
						chip->chg_valid_gpio);
		return IRQ_HANDLED;
	}
	present ^= chip->chg_valid_act_low;

	dev_dbg(chip->dev, "%s: chg_present = %d\n", __func__, present);

	if (present != chip->chg_present) {
		chip->chg_present = present;
		dev_dbg(chip->dev, "%s updating usb_psy present=%d",
				__func__, chip->chg_present);
		power_supply_set_present(chip->usb_psy, chip->chg_present);
	}

	return IRQ_HANDLED;
}

static void sm5414_external_power_changed(struct power_supply *psy)
{
	struct sm5414_charger *chip = container_of(psy,
				struct sm5414_charger, batt_psy);
	union power_supply_propval prop = {0,};
	int rc, current_limit = 0;
	int type = 0;
	unsigned int current_max = 0;
	unsigned int vddmax = 0;
	pr_debug("%s\n", __func__);

	if (chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (rc)
		dev_err(chip->dev,
			"Couldn't read USB current_max property, rc=%d\n", rc);
	else
		current_limit = prop.intval / 1000;

	rc = chip->usb_psy->get_property(chip->usb_psy,
					POWER_SUPPLY_PROP_TYPE, &prop);

	if (rc)
		pr_debug("Couldn't read USB current_max property, rc=%d\n", rc);
	else
		type = prop.intval;

	if (current_limit == 0)
		pr_debug("%s : Don't change vbuslimit current to 100mA\n", __func__);
	else {
		if (((type == POWER_SUPPLY_TYPE_USB) || (type == POWER_SUPPLY_TYPE_UNKNOWN))/* && (IsUsbPlugIn == true)*/) {
			sm5414_term_current_set(chip);

			vddmax = chip->vfloat_mv;
			if (chip->batt_cool)
				vddmax = min(vddmax, chip->cool_bat_mv);
			if (chip->batt_warm)
				vddmax = min(vddmax, chip->warm_bat_mv);
			sm5414_float_voltage_set(chip, vddmax);

			sm5414_set_usb_chg_current(chip, 500);

			current_max = chip->fastchg_current_max_ma;
			if (chip->batt_cool)
				current_max = min(current_max, chip->cool_bat_ma);
			if (chip->batt_warm)
				current_max = min(current_max, chip->warm_bat_ma);
			sm5414_fastchg_current_set(chip, current_max);
		} else if ((type == POWER_SUPPLY_TYPE_USB_DCP)/* && (IsTAPlugIn == true)*/) {
			sm5414_term_current_set(chip);
			vddmax = chip->vfloat_mv;
			if (chip->batt_cool)
				vddmax = min(vddmax, chip->cool_bat_mv);
			if (chip->batt_warm)
				vddmax = min(vddmax, chip->warm_bat_mv);
			sm5414_float_voltage_set(chip, vddmax);

			sm5414_set_usb_chg_current(chip, 1500);

			current_max = chip->fastchg_current_max_ma;
			if (chip->batt_cool)
				current_max = min(current_max, chip->cool_bat_ma);
			if (chip->batt_warm)
				current_max = min(current_max, chip->warm_bat_ma);
			sm5414_fastchg_current_set(chip, current_max);
		} else {
			sm5414_term_current_set(chip);
			vddmax = chip->vfloat_mv;
			if (chip->batt_cool)
				vddmax = min(vddmax, chip->cool_bat_mv);
			if (chip->batt_warm)
				vddmax = min(vddmax, chip->warm_bat_mv);
			sm5414_float_voltage_set(chip, vddmax);

			sm5414_set_usb_chg_current(chip, current_max);

			current_max = chip->fastchg_current_max_ma;
			if (chip->batt_cool)
				current_max = min(current_max, chip->cool_bat_ma);
			if (chip->batt_warm)
				current_max = min(current_max, chip->warm_bat_ma);
			sm5414_fastchg_current_set(chip, current_max);

		}
	}




	pr_debug("%s : type = %d\n", __func__, type);
	pr_debug("%s : current_limit = %d\n", __func__, current_limit);

	dump_regs(chip);
}

#if defined(CONFIG_DEBUG_FS)
static int show_cnfg_regs(struct seq_file *m, void *data)
{
	struct sm5414_charger *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = SM5414_INTMASK1; addr <= SM5414_CHGCTRL5; addr++) {
		rc = sm5414_read_reg(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int cnfg_debugfs_open(struct inode *inode, struct file *file)
{
	struct sm5414_charger *chip = inode->i_private;

	return single_open(file, show_cnfg_regs, chip);
}

static const struct file_operations cnfg_debugfs_ops = {
	.owner	  = THIS_MODULE,
	.open	   = cnfg_debugfs_open,
	.read	   = seq_read,
	.llseek	 = seq_lseek,
	.release	= single_release,
};

static int get_reg(void *data, u64 *val)
{
	struct sm5414_charger *chip = data;
	int rc;
	u8 temp;

	rc = sm5414_read_reg(chip, chip->peek_poke_address, &temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't read reg %x rc = %d\n",
			chip->peek_poke_address, rc);
		return -EAGAIN;
	}
	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	struct sm5414_charger *chip = data;
	int rc;
	u8 temp;

	temp = (u8) val;
	rc = sm5414_write_reg(chip, chip->peek_poke_address, temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't write 0x%02x to 0x%02x rc= %d\n",
			chip->peek_poke_address, temp, rc);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(poke_poke_debug_ops, get_reg, set_reg, "0x%02llx\n");

static int force_irq_set(void *data, u64 val)
{
	struct sm5414_charger *chip = data;

	sm5414_chg_stat_handler(chip->client->irq, data);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(force_irq_ops, NULL, force_irq_set, "0x%02llx\n");
#endif

static int sm5414_parse_dt(struct sm5414_charger *chip)
{
	int rc;
	enum of_gpio_flags gpio_flags;
	struct device_node *node = chip->dev->of_node;
	int batt_present_degree_negative;

	if (!node) {
		dev_err(chip->dev, "device tree info. missing\n");
		return -EINVAL;
	}

	chip->chgen_gpio = of_get_named_gpio(node, "chgen-gpio", 0);
	pr_info("%s: SM5703_parse_dt chgen: %d\n", __func__, chip->chgen_gpio);

	chip->nshdn_gpio = of_get_named_gpio(node, "nshdn-gpio", 0);
	pr_info("%s: SM5703_parse_dt nshdn: %d\n", __func__, chip->nshdn_gpio);

	chip->irq_gpio = of_get_named_gpio(node, "sm-irq-gpio", 0);
	pr_info("%s: SM5703_parse_dt nshdn: %d\n", __func__, chip->irq_gpio);

	chip->charging_disabled = of_property_read_bool(node,
					"qcom, charger-disabled");

	chip->using_pmic_therm = of_property_read_bool(node,
						"qcom, using-pmic-therm");

	rc = of_property_read_string(node, "qcom, bms-psy-name",
						&chip->bms_psy_name);
	if (rc)
		chip->bms_psy_name = NULL;

	chip->chg_valid_gpio = of_get_named_gpio_flags(node,
				"qcom, chg-valid-gpio", 0, &gpio_flags);
	if (!gpio_is_valid(chip->chg_valid_gpio))
		dev_dbg(chip->dev, "Invalid chg-valid-gpio");
	else
		chip->chg_valid_act_low = gpio_flags & OF_GPIO_ACTIVE_LOW;

	rc = of_property_read_u32(node, "qcom, fastchg-current-max-ma",
						&chip->fastchg_current_max_ma);
	if (rc)
		chip->fastchg_current_max_ma = SM5414_FAST_CHG_MAX_MA;

	chip->iterm_disabled = of_property_read_bool(node,
					"qcom, iterm-disabled");

	rc = of_property_read_u32(node, "qcom, iterm-ma", &chip->iterm_ma);
	if (rc < 0)
		chip->iterm_ma = -EINVAL;

	rc = of_property_read_u32(node, "qcom, float-voltage-mv",
						&chip->vfloat_mv);
	if (rc < 0) {
		chip->vfloat_mv = -EINVAL;
		pr_err("float-voltage-mv property missing, exit\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "qcom, cold-bat-decidegc",
						&chip->cold_bat_decidegc);
	if (rc < 0)
		chip->cold_bat_decidegc = -EINVAL;

	rc = of_property_read_u32(node, "qcom, hot-bat-decidegc",
						&chip->hot_bat_decidegc);
	if (rc < 0)
		chip->hot_bat_decidegc = -EINVAL;

	rc = of_property_read_u32(node, "qcom, warm-bat-decidegc",
						&chip->warm_bat_decidegc);

	rc |= of_property_read_u32(node, "qcom, cool-bat-decidegc",
						&chip->cool_bat_decidegc);

	if (!rc) {
		rc = of_property_read_u32(node, "qcom, cool-bat-mv",
						&chip->cool_bat_mv);

		rc |= of_property_read_u32(node, "qcom, warm-bat-mv",
						&chip->warm_bat_mv);

		rc |= of_property_read_u32(node, "qcom, cool-bat-ma",
						&chip->cool_bat_ma);

		rc |= of_property_read_u32(node, "qcom, warm-bat-ma",
						&chip->warm_bat_ma);
		if (rc)
			chip->jeita_supported = false;
		else
			chip->jeita_supported = true;
	}

	pr_debug("jeita_supported = %d", chip->jeita_supported);

	rc = of_property_read_u32(node, "qcom, bat-present-decidegc",
						&batt_present_degree_negative);
	if (rc < 0)
		chip->bat_present_decidegc = -EINVAL;
	else
		chip->bat_present_decidegc = -batt_present_degree_negative;

	if (of_get_property(node, "qcom, vcc-i2c-supply", NULL)) {
		chip->vcc_i2c = devm_regulator_get(chip->dev, "vcc-i2c");
		if (IS_ERR(chip->vcc_i2c)) {
			dev_err(chip->dev,
				"%s: Failed to get vcc_i2c regulator\n",
								__func__);
			return PTR_ERR(chip->vcc_i2c);
		}
	}

	pr_debug("vfloat-mv = %d, iterm-disabled = %d, ",
			chip->vfloat_mv, chip->iterm_ma);
	pr_debug("fastchg-current = %d, charging-disabled = %d, ",
			chip->fastchg_current_max_ma,
					chip->charging_disabled);
	pr_debug("bms = %s cold-bat-degree = %d, ",
				chip->bms_psy_name, chip->cold_bat_decidegc);
	pr_debug("hot-bat-degree = %d, bat-present-decidegc = %d\n",
		chip->hot_bat_decidegc, chip->bat_present_decidegc);
	return 0;
}

static int determine_initial_state(struct sm5414_charger *chip)
{
	int rc;
	u8 val = 0;

	rc =  sm5414_read_reg(chip, SM5414_STATUS, &val);
	if (rc) {
		dev_err(chip->dev, "Couldn't read IRQ_C rc = %d\n", rc);
		goto fail_init_status;
	} else
	   dev_err(chip->dev, "SM5414_STATUS val = 0x%x\n", val);

	chip->batt_full = (val & (SM5414_STATUS_TOPOFF_MASK << SM5414_STATUS_TOPOFF_SHIFT)) ? true : false;

	if (val & (SM5414_STATUS_VBUSUVLO_MASK << SM5414_STATUS_VBUSUVLO_SHIFT)) {
		chg_uv(chip, 1);
	} else if (val & (SM5414_STATUS_VBUSOVP_MASK << SM5414_STATUS_VBUSOVP_SHIFT)) {
		chg_ov(chip, 1);
	}

	return 0;

fail_init_status:
	dev_err(chip->dev, "Couldn't determine initial status\n");
	return rc;
}

#if defined(CONFIG_DEBUG_FS)
static void sm5414_debugfs_init(struct sm5414_charger *chip)
{
	int rc;
	chip->debug_root = debugfs_create_dir("sm5414", NULL);
	if (!chip->debug_root)
		dev_err(chip->dev, "Couldn't create debug dir\n");

	if (chip->debug_root) {
		struct dentry *ent;

		ent = debugfs_create_file("config_registers", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &cnfg_debugfs_ops);
		if (!ent || IS_ERR(ent)) {
			rc = PTR_ERR(ent);
			dev_err(chip->dev,
				"Couldn't create cnfg debug file rc = %d\n",
				rc);
		}

		ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root,
					  &(chip->peek_poke_address));
		if (!ent || IS_ERR(ent)) {
			rc = PTR_ERR(ent);
			dev_err(chip->dev,
				"Couldn't create address debug file rc = %d\n",
				rc);
		}

		ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, chip,
					  &poke_poke_debug_ops);
		if (!ent || IS_ERR(ent)) {
			rc = PTR_ERR(ent);
			dev_err(chip->dev,
				"Couldn't create data debug file rc = %d\n",
				rc);
		}

		ent = debugfs_create_file("force_irq",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, chip,
					  &force_irq_ops);
		if (!ent || IS_ERR(ent)) {
			rc = PTR_ERR(ent);
			dev_err(chip->dev,
				"Couldn't create force_irq debug file rc =%d\n",
				rc);
		}
	}
}
#else
static void sm5414_debugfs_init(struct sm5414_charger *chip)
{
}
#endif

#define SMB_I2C_VTG_MIN_UV 1800000
#define SMB_I2C_VTG_MAX_UV 1800000
static int sm5414_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int rc, irq;
	struct sm5414_charger *chip;
	struct power_supply *usb_psy;


	pr_debug("%s : start\n", __func__);

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_dbg(&client->dev, "USB psy not found; deferring probe\n");
		return -EPROBE_DEFER;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "Couldn't allocate memory\n");
		return -ENOMEM;
	}

	chip->client = client;
	chip->dev = &client->dev;
	chip->usb_psy = usb_psy;
	chip->fake_battery_soc = -EINVAL;

	/* early for VADC get, defer probe if needed */
	chip->vadc_dev = qpnp_get_vadc(chip->dev, "chg");
	if (IS_ERR(chip->vadc_dev)) {
		rc = PTR_ERR(chip->vadc_dev);
		if (rc != -EPROBE_DEFER)
			pr_err("vadc property missing\n");
		return rc;
	}

	rc = sm5414_parse_dt(chip);
	if (rc) {
		dev_err(&client->dev, "Couldn't parse DT nodes rc=%d\n", rc);
		return rc;
	}

	rc = gpio_request(chip->chgen_gpio, "sm5414_nCHGEN");
	if (rc) {
		pr_info("%s : Request GPIO %d failed\n",
			   __func__, (int)chip->chgen_gpio);
	}

	rc = gpio_request(chip->nshdn_gpio, "sm5414_nSHDN");
	if (rc) {
	   pr_info("%s : Request GPIO %d failed\n",
			__func__, (int)chip->nshdn_gpio);
	}

	gpio_direction_output(chip->nshdn_gpio, 1);
	pr_info("%s : chip->nshdn_gpio %d\n", __func__, (int)gpio_get_value(chip->nshdn_gpio));

	/* i2c pull up regulator configuration */
	if (chip->vcc_i2c) {
		if (regulator_count_voltages(chip->vcc_i2c) > 0) {
			rc = regulator_set_voltage(chip->vcc_i2c,
				SMB_I2C_VTG_MIN_UV, SMB_I2C_VTG_MAX_UV);
			if (rc) {
				dev_err(&client->dev,
				"regulator vcc_i2c set failed, rc = %d\n",
								rc);
				return rc;
			}
		}

		rc = regulator_enable(chip->vcc_i2c);
		if (rc) {
			dev_err(&client->dev,
				"Regulator vcc_i2c enable failed rc = %d\n",
									rc);
			goto err_set_vtg_i2c;
		}
	}

	mutex_init(&chip->irq_complete);
	mutex_init(&chip->read_write_lock);
	mutex_init(&chip->path_suspend_lock);

	/* using adc_tm for implementing pmic therm */
	if (chip->using_pmic_therm) {
		chip->adc_tm_dev = qpnp_get_adc_tm(chip->dev, "chg");
		if (IS_ERR(chip->adc_tm_dev)) {
			rc = PTR_ERR(chip->adc_tm_dev);
			if (rc != -EPROBE_DEFER)
				pr_err("adc_tm property missing\n");
			return rc;
		}
	}

	i2c_set_clientdata(client, chip);

	chip->batt_psy.name	 = "battery";
	chip->batt_psy.type	 = POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.get_property = sm5414_battery_get_property;
	chip->batt_psy.set_property = sm5414_battery_set_property;
	chip->batt_psy.property_is_writeable =
					sm5414_batt_property_is_writeable;
	chip->batt_psy.properties   = sm5414_battery_properties;
	chip->batt_psy.num_properties   = ARRAY_SIZE(sm5414_battery_properties);
	chip->batt_psy.external_power_changed = sm5414_external_power_changed;
	chip->batt_psy.supplied_to = pm_batt_supplied_to;
	chip->batt_psy.num_supplicants = ARRAY_SIZE(pm_batt_supplied_to);

	chip->resume_completed = true;

	rc = power_supply_register(chip->dev, &chip->batt_psy);
	if (rc < 0) {
		dev_err(&client->dev, "Couldn't register batt psy rc = %d\n",
				rc);
		goto err_set_vtg_i2c;
	}

	/* We will not use it by default */
	if (gpio_is_valid(chip->chg_valid_gpio)) {
		rc = gpio_request(chip->chg_valid_gpio, "sm5414_chg_valid");
		if (rc) {
			dev_err(&client->dev,
				"gpio_request for %d failed rc=%d\n",
				chip->chg_valid_gpio, rc);
			goto fail_chg_valid_irq;
		}
		irq = gpio_to_irq(chip->chg_valid_gpio);
		if (irq < 0) {
			dev_err(&client->dev,
				"Invalid chg_valid irq = %d\n", irq);
			goto fail_chg_valid_irq;
		}
		rc = devm_request_threaded_irq(&client->dev, irq,
				NULL, sm5414_chg_valid_handler,
				IRQF_TRIGGER_FALLING,
				"sm5414_chg_valid_irq", chip);
		if (rc) {
			dev_err(&client->dev,
				"Failed request_irq irq=%d, gpio=%d rc=%d\n",
				irq, chip->chg_valid_gpio, rc);
			goto fail_chg_valid_irq;
		}
		sm5414_chg_valid_handler(irq, chip);
		enable_irq_wake(irq);
	}

	/* STAT irq configuration */
	if (gpio_is_valid(chip->irq_gpio)) {

		rc = gpio_request(chip->irq_gpio, "sm5414_irq");
		if (rc) {
			pr_debug("irq gpio request failed, rc=%d", rc);
			goto fail_sm5414_hw_init;
		}
		rc = gpio_direction_input(chip->irq_gpio);
		if (rc) {
			pr_debug("set_direction for irq gpio failed\n");
			goto fail_irq_gpio;
		}

		irq = gpio_to_irq(chip->irq_gpio);
		pr_debug("%s : irq = %d", __func__, irq);
		if (irq < 0) {
			pr_debug("Invalid irq_gpio irq = %d\n", irq);
			goto fail_irq_gpio;
		}


		sm5414_chg_read_init(chip);

		rc = devm_request_threaded_irq(&client->dev, irq, NULL,
				sm5414_chg_stat_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"sm5414_chg_stat_irq", chip);
		pr_debug("%s : rc = %d", __func__, rc);
		if (rc) {
			pr_debug("Failed STAT irq=%d request rc = %d\n", irq, rc);
			goto fail_irq_gpio;
		}
		enable_irq_wake(irq);
		pr_debug("%s : irq done", __func__);
	} else {
		pr_debug("%s : fail_irq_gpio\n", __func__);
		goto fail_irq_gpio;
	}

	dump_regs(chip);

	rc = sm5414_regulator_init(chip);
	if  (rc) {
		dev_err(&client->dev,
			"Couldn't initialize sm5414 ragulator rc=%d\n", rc);
		goto fail_regulator_register;
	}

	rc = sm5414_hw_init(chip);
	if (rc) {
		dev_err(&client->dev,
			"Couldn't intialize hardware rc=%d\n", rc);
		goto fail_sm5414_hw_init;
	}

	rc = determine_initial_state(chip);
	if (rc) {
		dev_err(&client->dev,
			"Couldn't determine initial state rc=%d\n", rc);
		goto fail_sm5414_hw_init;
	}

	if (chip->using_pmic_therm) {
		if (!chip->jeita_supported) {
			/* add hot/cold temperature monitor */
			chip->adc_param.low_temp = chip->cold_bat_decidegc;
			chip->adc_param.high_temp = chip->hot_bat_decidegc;
		} else {
			chip->adc_param.low_temp = chip->cool_bat_decidegc;
			chip->adc_param.high_temp = chip->warm_bat_decidegc;
		}
		chip->adc_param.timer_interval = ADC_MEAS2_INTERVAL_1S;
		chip->adc_param.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;
		chip->adc_param.btm_ctx = chip;
		chip->adc_param.threshold_notification =
				sm_chg_adc_notification;
		chip->adc_param.channel = LR_MUX1_BATT_THERM;

		/* update battery missing info in tm_channel_measure*/
		rc = qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
							&chip->adc_param);
		if (rc)
			pr_err("requesting ADC error %d\n", rc);
	}

	sm5414_debugfs_init(chip);

	dump_regs(chip);

	pSm5414 = chip;
	pr_debug("SM5414 successfully probed. charger=%d, batt=%d\n", chip->chg_present, sm5414_get_prop_batt_present(chip));
	return 0;

fail_sm5414_hw_init:
	regulator_unregister(chip->otg_vreg.rdev);
fail_regulator_register:
	power_supply_unregister(&chip->batt_psy);
fail_irq_gpio:
		if (gpio_is_valid(chip->irq_gpio))
			gpio_free(chip->irq_gpio);

fail_chg_valid_irq:
		if (gpio_is_valid(chip->chg_valid_gpio))
			gpio_free(chip->chg_valid_gpio);
err_set_vtg_i2c:
	if (chip->vcc_i2c)
		if (regulator_count_voltages(chip->vcc_i2c) > 0)
			regulator_set_voltage(chip->vcc_i2c, 0,
						SMB_I2C_VTG_MAX_UV);
	return rc;
}

static int sm5414_charger_remove(struct i2c_client *client)
{
	struct sm5414_charger *chip = i2c_get_clientdata(client);

	power_supply_unregister(&chip->batt_psy);
	if (gpio_is_valid(chip->chg_valid_gpio))
		gpio_free(chip->chg_valid_gpio);

	if (chip->vcc_i2c)
		regulator_disable(chip->vcc_i2c);

	mutex_destroy(&chip->irq_complete);
	debugfs_remove_recursive(chip->debug_root);
	return 0;
}

static int sm5414_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sm5414_charger *chip = i2c_get_clientdata(client);
	int rc;

	mutex_lock(&chip->irq_complete);
	if (chip->vcc_i2c) {
		rc = regulator_disable(chip->vcc_i2c);
		if (rc) {
			dev_err(chip->dev,
				"Regulator vcc_i2c disable failed rc=%d\n", rc);
			mutex_unlock(&chip->irq_complete);
			return rc;
		}
	}

	chip->resume_completed = false;
	mutex_unlock(&chip->irq_complete);
	return 0;
}

static int sm5414_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sm5414_charger *chip = i2c_get_clientdata(client);

	if (chip->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;
}

static int sm5414_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sm5414_charger *chip = i2c_get_clientdata(client);
	int rc;

	if (chip->vcc_i2c) {
		rc = regulator_enable(chip->vcc_i2c);
		if (rc) {
			dev_err(chip->dev,
				"Regulator vcc_i2c enable failed rc=%d\n", rc);
			return rc;
		}
	}

	mutex_lock(&chip->irq_complete);
	chip->resume_completed = true;
	mutex_unlock(&chip->irq_complete);
	if (chip->irq_waiting) {
		sm5414_chg_stat_handler(client->irq, chip);
		enable_irq(client->irq);
	}
	return 0;
}

void sm5414_change_current_backlight_onoff(bool sw, int cur)
{
	int rc, temp;
	int origin_current;
	struct device_node *node;
	if (pSm5414) {
		node = pSm5414->dev->of_node;
		rc = of_property_read_u32(node, "qcom, fastchg-current-max-ma",
						&origin_current);
		temp = sm5414_get_prop_batt_temp(pSm5414);

		if (temp > 150 && temp < 450) {
			pSm5414->fastchg_current_max_ma = sw?cur:origin_current;
			sm5414_set_usb_chg_current(pSm5414, pSm5414->fastchg_current_max_ma);
			sm5414_fastchg_current_set(pSm5414, pSm5414->fastchg_current_max_ma);
		}


		msleep(100);
	}

}
EXPORT_SYMBOL_GPL(sm5414_change_current_backlight_onoff);


static const struct dev_pm_ops sm5414_pm_ops = {
	.suspend	= sm5414_suspend,
	.suspend_noirq  = sm5414_suspend_noirq,
	.resume	 = sm5414_resume,
};

static struct of_device_id sm5414_match_table[] = {
	{.compatible = "qcom, sm5414-charger",},
	{},
};

static const struct i2c_device_id sm5414_charger_id[] = {
	{"sm5414-charger", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, sm5414_charger_id);

static struct i2c_driver sm5414_charger_driver = {
	.driver	 = {
		.name	   = "sm5414-charger",
		.owner	  = THIS_MODULE,
		.of_match_table = sm5414_match_table,
		.pm	 = &sm5414_pm_ops,
	},
	.probe	  = sm5414_charger_probe,
	.remove	 = sm5414_charger_remove,
	.id_table   = sm5414_charger_id,
};

module_i2c_driver(sm5414_charger_driver);

MODULE_DESCRIPTION("SM5414 Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:sm5414-charger");

