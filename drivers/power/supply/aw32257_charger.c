// SPDX-License-Identifier: GPL-2.0：
// Copyright (C) 2021 Spreadtrum Communications Inc.

#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/idr.h>
#include <linux/regmap.h>
#include <linux/usb/phy.h>
#include <linux/reboot.h>
#include <linux/sysfs.h>
#include <linux/acpi.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <uapi/linux/usb/charger.h>

/* timeout for resetting chip timer */
#define AW32257_TIMER_TIMEOUT		10

#define AW32257_REG_STATUS		0x00
#define AW32257_REG_CONTROL		0x01
#define AW32257_REG_VOLTAGE		0x02
#define AW32257_REG_VENDER		0x03
#define AW32257_REG_CURRENT		0x04

/*****************  add for otg  ************************/
#define BIT_DP_DM_BC_ENB  BIT(0)
#define aw32257_CON0      0x00
#define aw32257_CON1      0x01
#define aw32257_CON2      0x02
#define aw32257_CON3      0x03
#define aw32257_CON4      0x04
#define aw32257_CON5      0x05
#define aw32257_CON6      0x06
#define aw32257_REG_NUM 7
/* CON0 */
#define CON0_OTG_MASK	0x01
#define CON1_OTG_SHIFT	3

/* CON1 */
#define CON1_LIN_LIMIT_MASK     0x03
#define CON1_LIN_LIMIT_SHIFT    6

#define CON1_LOW_V_2_MASK       0x01
#define CON1_LOW_V_2_SHIFT      5

#define CON1_LOW_V_1_MASK       0x01
#define CON1_LOW_V_1_SHIFT      4

#define CON1_TE_MASK    0x01
#define CON1_TE_SHIFT   3

#define CON1_CE_MASK    0x01
#define CON1_CE_SHIFT   2

#define CON1_HZ_MODE_MASK       0x01
#define CON1_HZ_MODE_SHIFT      1

#define CON1_OPA_MODE_MASK      0x01
#define CON1_OPA_MODE_SHIFT 0

/* CON2 */
#define CON2_CV_VTH_MASK        0x3F
#define CON2_CV_VTH_SHIFT       2

#define CON2_OTG_PL_MASK        0x01
#define CON2_OTG_PL_SHIFT       1

#define CON2_OTG_EN_MASK        0x01
#define CON2_OTG_EN_SHIFT       0

/* reset state for all registers */
#define AW32257_RESET_STATUS		BIT(6)
#define AW32257_RESET_CONTROL		(BIT(4)|BIT(5))
#define AW32257_RESET_VOLTAGE		(BIT(1)|BIT(3))
#define AW32257_RESET_CURRENT		(BIT(0)|BIT(3)|BIT(7))

/* status register */
#define AW32257_BIT_TMR_RST		7
#define AW32257_BIT_OTG			7
#define AW32257_BIT_EN_STAT		6
#define AW32257_MASK_STAT		(BIT(4)|BIT(5))
#define AW32257_SHIFT_STAT		4
#define AW32257_BIT_BOOST		3
#define AW32257_MASK_FAULT		(BIT(0)|BIT(1)|BIT(2))
#define AW32257_SHIFT_FAULT		0

/* control register */
#define AW32257_MASK_LIMIT		(BIT(6)|BIT(7))
#define AW32257_SHIFT_LIMIT		6
#define AW32257_MASK_VLOWV		(BIT(4)|BIT(5))
#define AW32257_SHIFT_VLOWV		4
#define AW32257_BIT_TE			3
#define AW32257_BIT_CE			2
#define AW32257_BIT_HZ_MODE		1
#define AW32257_BIT_OPA_MODE		0

/* voltage register */
#define AW32257_MASK_VO		(BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7))
#define AW32257_SHIFT_VO		2
#define AW32257_BIT_OTG_PL		1
#define AW32257_BIT_OTG_EN		0

/* vender register */
#define AW32257_MASK_VENDER		(BIT(5)|BIT(6)|BIT(7))
#define AW32257_SHIFT_VENDER		5
#define AW32257_MASK_PN			(BIT(3)|BIT(4))
#define AW32257_SHIFT_PN		3
#define AW32257_MASK_REVISION		(BIT(0)|BIT(1)|BIT(2))
#define AW32257_SHIFT_REVISION		0

/* current register */
#define AW32257_MASK_RESET		BIT(7)

#define AW32257_MASK_VI_CHRG          (BIT(3)|BIT(4)|BIT(5)|BIT(6))
#define AW32257_SHIFT_VI_CHRG         3

/* N/A					BIT(3) */
#define AW32257_MASK_VI_TERM		(BIT(0)|BIT(1)|BIT(2))
#define AW32257_SHIFT_VI_TERM		0

#define AW32257_DISABLE_PIN_MASK_2721                  BIT(15)
#define AW32257_DISABLE_PIN_MASK_2720                  BIT(0)
#define AW32257_DISABLE_PIN_MASK_2730                  BIT(0)

#define CONFIG_CHARGE_PD   1
#ifdef CONFIG_CHARGE_PD
int chg_pd_gpio;
#endif

#define AW32257_WAKE_UP_MS                 2000
enum aw32257_command {
	AW32257_TIMER_RESET,
	AW32257_OTG_STATUS,
	AW32257_STAT_PIN_STATUS,
	AW32257_STAT_PIN_ENABLE,
	AW32257_STAT_PIN_DISABLE,
	AW32257_CHARGE_STATUS,
	AW32257_BOOST_STATUS,
	AW32257_FAULT_STATUS,

	AW32257_CHARGE_TERMINATION_STATUS,
	AW32257_CHARGE_TERMINATION_ENABLE,
	AW32257_CHARGE_TERMINATION_DISABLE,
	AW32257_CHARGER_STATUS,
	AW32257_CHARGER_ENABLE,
	AW32257_CHARGER_DISABLE,
	AW32257_HIGH_IMPEDANCE_STATUS,
	AW32257_HIGH_IMPEDANCE_ENABLE,
	AW32257_HIGH_IMPEDANCE_DISABLE,
	AW32257_BOOST_MODE_STATUS,
	AW32257_BOOST_MODE_ENABLE,
	AW32257_BOOST_MODE_DISABLE,

	AW32257_OTG_LEVEL,
	AW32257_OTG_ACTIVATE_HIGH,
	AW32257_OTG_ACTIVATE_LOW,
	AW32257_OTG_PIN_STATUS,
	AW32257_OTG_PIN_ENABLE,
	AW32257_OTG_PIN_DISABLE,

	AW32257_VENDER_CODE,
	AW32257_PART_NUMBER,
	AW32257_REVISION,
};

enum aw32257_chip {
	BQUNKNOWN,
	BQ24150,
	BQ24150A,
	BQ24151,
	BQ24151A,
	BQ24152,
	BQ24153,
	BQ24153A,
	BQ24155,
	BQ24156,
	BQ24156A,
	BQ24157S,
	BQ24158,
};

static char *aw32257_chip_name[] = {
	"unknown",
	"bq24150",
	"bq24150a",
	"bq24151",
	"bq24151a",
	"bq24152",
	"bq24153",
	"bq24153a",
	"bq24155",
	"bq24156",
	"bq24156a",
	"bq24157s",
	"bq24158",
};

/* Supported modes with maximal current limit */
enum aw32257_mode {
	AW32257_MODE_OFF,		/* offline mode (charger disabled) */
	AW32257_MODE_NONE,		/* unknown charger (100mA) */
	AW32257_MODE_HOST_CHARGER,	/* usb host/hub charger (500mA) */
	AW32257_MODE_DEDICATED_CHARGER, /* dedicated charger (unlimited) */
	AW32257_MODE_BOOST,		/* boost mode (charging disabled) */
};

struct aw32257_platform_data {
	int current_limit;		/* mA */
	int weak_battery_voltage;	/* mV */
	int battery_regulation_voltage;	/* mV */
	int charge_current;		/* mA */
	int termination_current;	/* mA */
	int resistor_sense;		/* m ohm */
	const char *notify_device;	/* name */
};

struct aw32257_charge_current {
	int sdp_limit;
	int sdp_cur;
	int dcp_limit;
	int dcp_cur;
	int cdp_limit;
	int cdp_cur;
	int unknown_limit;
	int unknown_cur;
};
struct aw32257_device {
	struct device *dev;
	struct aw32257_platform_data init_data;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *charger;
	struct aw32257_charge_current cur;
	struct extcon_dev *edev;
	struct power_supply_desc charger_desc;
	struct delayed_work otg_work;
	struct work_struct work;
	struct mutex lock;
	struct regmap *pmic;
	enum aw32257_mode reported_mode;
	enum aw32257_mode mode;
	enum aw32257_chip chip;
	const char *timer_error;
	char *model;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	u32 limit;
	bool charging;
};

static DEFINE_MUTEX(aw32257_i2c_mutex);

#ifdef CONFIG_CHARGE_PD
static int chg_pd_notify(struct notifier_block *this, unsigned long code,
			 void *unused)
{
	switch (code) {
	case SYS_DOWN:
	case SYS_HALT:
	case SYS_POWER_OFF:
		gpio_set_value(chg_pd_gpio, 0);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block chg_pd_notifier = {
	chg_pd_notify,
	NULL,
	0
};
#endif

/* read value from register */
static int aw32257_i2c_read(struct aw32257_device *bq, u8 reg)
{
	struct i2c_client *client = to_i2c_client(bq->dev);
	struct i2c_msg msg[2];
	u8 val;
	int ret;

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(reg);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = &val;
	msg[1].len = sizeof(val);

	mutex_lock(&aw32257_i2c_mutex);
	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	mutex_unlock(&aw32257_i2c_mutex);

	if (ret < 0)
		return ret;

	return val;
}

/* read value from register, apply mask and right shift it */
static int aw32257_i2c_read_mask(struct aw32257_device *bq, u8 reg,
				 u8 mask, u8 shift)
{
	int ret;

	if (shift > 8)
		return -EINVAL;

	ret = aw32257_i2c_read(bq, reg);
	if (ret < 0)
		return ret;
	return (ret & mask) >> shift;
}

/* read value from register and return one specified bit */
static int aw32257_i2c_read_bit(struct aw32257_device *bq, u8 reg, u8 bit)
{
	if (bit > 8)
		return -EINVAL;
	return aw32257_i2c_read_mask(bq, reg, BIT(bit), bit);
}

/* write value to register */
static int aw32257_i2c_write(struct aw32257_device *bq, u8 reg, u8 val)
{
	struct i2c_client *client = to_i2c_client(bq->dev);
	struct i2c_msg msg[1];
	u8 data[2];
	int ret;

	data[0] = reg;
	data[1] = val;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = data;
	msg[0].len = ARRAY_SIZE(data);

	mutex_lock(&aw32257_i2c_mutex);
	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	mutex_unlock(&aw32257_i2c_mutex);

	/* i2c_transfer returns number of messages transferred */
	if (ret < 0)
		return ret;
	else if (ret != 1)
		return -EIO;

	return 0;
}

/* read value from register, change it with mask left shifted and write back */
static int aw32257_i2c_write_mask(struct aw32257_device *bq, u8 reg, u8 val,
				  u8 mask, u8 shift)
{
	int ret;

	if (shift > 8)
		return -EINVAL;

	ret = aw32257_i2c_read(bq, reg);
	if (ret < 0)
		return ret;

	ret &= ~mask;
	ret |= val << shift;

	return aw32257_i2c_write(bq, reg, ret);
}

/* change only one bit in register */
static int aw32257_i2c_write_bit(struct aw32257_device *bq, u8 reg,
				 bool val, u8 bit)
{
	if (bit > 8)
		return -EINVAL;
	return aw32257_i2c_write_mask(bq, reg, val, BIT(bit), bit);
}

/**** global functions ****/
/* exec command function */
static int aw32257_exec_command(struct aw32257_device *bq,
				enum aw32257_command command)
{
	int ret;

	switch (command) {
	case AW32257_TIMER_RESET:
		return aw32257_i2c_write_bit(bq, AW32257_REG_STATUS, 1,
					     AW32257_BIT_TMR_RST);
	case AW32257_OTG_STATUS:
		return aw32257_i2c_read_bit(bq, AW32257_REG_STATUS,
					    AW32257_BIT_OTG);
	case AW32257_STAT_PIN_STATUS:
		return aw32257_i2c_read_bit(bq, AW32257_REG_STATUS,
					    AW32257_BIT_EN_STAT);
	case AW32257_STAT_PIN_ENABLE:
		return aw32257_i2c_write_bit(bq, AW32257_REG_STATUS, 1,
					     AW32257_BIT_EN_STAT);
	case AW32257_STAT_PIN_DISABLE:
		return aw32257_i2c_write_bit(bq, AW32257_REG_STATUS, 0,
					     AW32257_BIT_EN_STAT);
	case AW32257_CHARGE_STATUS:
		return aw32257_i2c_read_mask(bq, AW32257_REG_STATUS,
					     AW32257_MASK_STAT,
					     AW32257_SHIFT_STAT);
	case AW32257_BOOST_STATUS:
		return aw32257_i2c_read_bit(bq, AW32257_REG_STATUS,
					    AW32257_BIT_BOOST);
	case AW32257_FAULT_STATUS:
		return aw32257_i2c_read_mask(bq, AW32257_REG_STATUS,
					     AW32257_MASK_FAULT,
					     AW32257_SHIFT_FAULT);

	case AW32257_CHARGE_TERMINATION_STATUS:
		return aw32257_i2c_read_bit(bq, AW32257_REG_CONTROL,
					    AW32257_BIT_TE);
	case AW32257_CHARGE_TERMINATION_ENABLE:
		return aw32257_i2c_write_bit(bq, AW32257_REG_CONTROL, 1,
					     AW32257_BIT_TE);
	case AW32257_CHARGE_TERMINATION_DISABLE:
		return aw32257_i2c_write_bit(bq, AW32257_REG_CONTROL, 0,
					     AW32257_BIT_TE);
	case AW32257_CHARGER_STATUS:
		ret = aw32257_i2c_read_bit(bq, AW32257_REG_CONTROL,
					   AW32257_BIT_CE);
		if (ret < 0)
			return ret;
		return ret > 0 ? 0 : 1;
	case AW32257_CHARGER_ENABLE:
		return aw32257_i2c_write_bit(bq, AW32257_REG_CONTROL, 0,
					     AW32257_BIT_CE);
	case AW32257_CHARGER_DISABLE:
		return aw32257_i2c_write_bit(bq, AW32257_REG_CONTROL, 1,
					     AW32257_BIT_CE);
	case AW32257_HIGH_IMPEDANCE_STATUS:
		return aw32257_i2c_read_bit(bq, AW32257_REG_CONTROL,
					    AW32257_BIT_HZ_MODE);
	case AW32257_HIGH_IMPEDANCE_ENABLE:
		return aw32257_i2c_write_bit(bq, AW32257_REG_CONTROL, 1,
					     AW32257_BIT_HZ_MODE);
	case AW32257_HIGH_IMPEDANCE_DISABLE:
		return aw32257_i2c_write_bit(bq, AW32257_REG_CONTROL, 0,
					     AW32257_BIT_HZ_MODE);
	case AW32257_BOOST_MODE_STATUS:
		return aw32257_i2c_read_bit(bq, AW32257_REG_CONTROL,
					    AW32257_BIT_OPA_MODE);
	case AW32257_BOOST_MODE_ENABLE:
		return aw32257_i2c_write_bit(bq, AW32257_REG_CONTROL, 1,
					     AW32257_BIT_OPA_MODE);
	case AW32257_BOOST_MODE_DISABLE:
		return aw32257_i2c_write_bit(bq, AW32257_REG_CONTROL, 0,
					     AW32257_BIT_OPA_MODE);

	case AW32257_OTG_LEVEL:
		return aw32257_i2c_read_bit(bq, AW32257_REG_VOLTAGE,
					    AW32257_BIT_OTG_PL);
	case AW32257_OTG_ACTIVATE_HIGH:
		return aw32257_i2c_write_bit(bq, AW32257_REG_VOLTAGE, 1,
					     AW32257_BIT_OTG_PL);
	case AW32257_OTG_ACTIVATE_LOW:
		return aw32257_i2c_write_bit(bq, AW32257_REG_VOLTAGE, 0,
					     AW32257_BIT_OTG_PL);
	case AW32257_OTG_PIN_STATUS:
		return aw32257_i2c_read_bit(bq, AW32257_REG_VOLTAGE,
					    AW32257_BIT_OTG_EN);
	case AW32257_OTG_PIN_ENABLE:
		return aw32257_i2c_write_bit(bq, AW32257_REG_VOLTAGE, 1,
					     AW32257_BIT_OTG_EN);
	case AW32257_OTG_PIN_DISABLE:
		return aw32257_i2c_write_bit(bq, AW32257_REG_VOLTAGE, 0,
					     AW32257_BIT_OTG_EN);

	case AW32257_VENDER_CODE:
		return aw32257_i2c_read_mask(bq, AW32257_REG_VENDER,
					     AW32257_MASK_VENDER,
					     AW32257_SHIFT_VENDER);
	case AW32257_PART_NUMBER:
		return aw32257_i2c_read_mask(bq, AW32257_REG_VENDER,
					     AW32257_MASK_PN,
					     AW32257_SHIFT_PN);
	case AW32257_REVISION:
		return aw32257_i2c_read_mask(bq, AW32257_REG_VENDER,
					     AW32257_MASK_REVISION,
					     AW32257_SHIFT_REVISION);
	}
	return -EINVAL;
}

/* detect chip type */
static enum aw32257_chip aw32257_detect_chip(struct aw32257_device *bq)
{
	struct i2c_client *client = to_i2c_client(bq->dev);
	int ret = aw32257_exec_command(bq, AW32257_PART_NUMBER);

	if (ret < 0)
		return ret;

	switch (client->addr) {
	case 0x6b:
		switch (ret) {
		case 0:
			if (bq->chip == BQ24151A)
				return bq->chip;
			return BQ24151;
		case 1:
			if (bq->chip == BQ24150A ||
				bq->chip == BQ24152 ||
				bq->chip == BQ24155)
				return bq->chip;
			return BQ24150;
		case 2:
			if (bq->chip == BQ24153A)
				return bq->chip;
			return BQ24153;
		default:
			return BQUNKNOWN;
		}
		break;

	case 0x6a:
		switch (ret) {
		case 0:
			if (bq->chip == BQ24156A)
				return bq->chip;
			return BQ24156;
		case 2:
			if (bq->chip == BQ24157S)
				return bq->chip;
			return BQ24158;
		default:
			return BQUNKNOWN;
		}
		break;
	}

	return BQUNKNOWN;
}

/* detect chip revision */
static int aw32257_detect_revision(struct aw32257_device *bq)
{
	int ret = aw32257_exec_command(bq, AW32257_REVISION);
	int chip = aw32257_detect_chip(bq);

	if (ret < 0 || chip < 0)
		return -1;

	switch (chip) {
	case BQ24150:
	case BQ24150A:
	case BQ24151:
	case BQ24151A:
	case BQ24152:
		if (ret >= 0 && ret <= 3)
			return ret;
		return -1;
	case BQ24153:
	case BQ24153A:
	case BQ24156:
	case BQ24156A:
	case BQ24157S:
	case BQ24158:
		if (ret == 3)
			return 0;
		else if (ret == 1)
			return 1;
		return -1;
	case BQ24155:
		if (ret == 3)
			return 3;
		return -1;
	case BQUNKNOWN:
		return -1;
	}

	return -1;
}

/* return chip vender code */
static int aw32257_get_vender_code(struct aw32257_device *bq)
{
	int ret;

	ret = aw32257_exec_command(bq, AW32257_VENDER_CODE);
	if (ret < 0)
		return 0;

	/* convert to binary */
	return (ret & 0x1) +
	       ((ret >> 1) & 0x1) * 10 +
	       ((ret >> 2) & 0x1) * 100;
}

/* reset all chip registers to default state */
static void aw32257_reset_chip(struct aw32257_device *bq)
{
	aw32257_i2c_write(bq, AW32257_REG_CURRENT, AW32257_RESET_CURRENT);
	aw32257_i2c_write(bq, AW32257_REG_VOLTAGE, AW32257_RESET_VOLTAGE);
	aw32257_i2c_write(bq, AW32257_REG_CONTROL, AW32257_RESET_CONTROL);
	aw32257_i2c_write(bq, AW32257_REG_STATUS, AW32257_RESET_STATUS);
	bq->timer_error = NULL;
}

/**** properties functions ****/
/* set current limit in mA */
static int aw32257_set_current_limit(struct aw32257_device *bq, int uA)
{
	int val, mA;

	mA = uA / 1000;

	if (mA <= 100)
		val = 0;
	else if (mA <= 500)
		val = 1;
	else if (mA <= 800)
		val = 2;
	else
		val = 3;

	return aw32257_i2c_write_mask(bq, AW32257_REG_CONTROL, val,
				      AW32257_MASK_LIMIT, AW32257_SHIFT_LIMIT);
}

/* get current limit in mA */
static int aw32257_get_current_limit(struct aw32257_device *bq)
{
	int ret;

	ret = aw32257_i2c_read_mask(bq, AW32257_REG_CONTROL,
				    AW32257_MASK_LIMIT, AW32257_SHIFT_LIMIT);
	if (ret < 0)
		return ret;
	else if (ret == 0)
		return 100;
	else if (ret == 1)
		return 500;
	else if (ret == 2)
		return 800;
	else if (ret == 3)
		return 1800;
	return -EINVAL;
}

/* set weak battery voltage in mV */
static int aw32257_set_weak_battery_voltage(struct aw32257_device *bq, int mV)
{
	int val;

	/* round to 100mV */
	if (mV <= 3400 + 50)
		val = 0;
	else if (mV <= 3500 + 50)
		val = 1;
	else if (mV <= 3600 + 50)
		val = 2;
	else
		val = 3;

	return aw32257_i2c_write_mask(bq, AW32257_REG_CONTROL, val,
				      AW32257_MASK_VLOWV, AW32257_SHIFT_VLOWV);
}

/* get weak battery voltage in mV */
static int aw32257_get_weak_battery_voltage(struct aw32257_device *bq)
{
	int ret;

	ret = aw32257_i2c_read_mask(bq, AW32257_REG_CONTROL,
				    AW32257_MASK_VLOWV, AW32257_SHIFT_VLOWV);
	if (ret < 0)
		return ret;
	return 100 * (34 + ret);
}

/* set battery regulation voltage in uV */
static int aw32257_set_battery_regulation_voltage(struct aw32257_device *bq,
						  int uV)
{
	int val, mV;

	mV = uV / 1000;

	val = (mV / 10 - 350) / 2;
	/*
	 * According to datasheet, maximum battery regulation voltage is
	 * 4440mV which is b101111 = 47.
	 */
	if (val < 0)
		val = 0;
	else if (val > 47)
		return -EINVAL;

	return aw32257_i2c_write_mask(bq, AW32257_REG_VOLTAGE, val,
				      AW32257_MASK_VO, AW32257_SHIFT_VO);
}

/* get battery regulation voltage in mV */
static int aw32257_get_battery_regulation_voltage(struct aw32257_device *bq)
{
	int ret = aw32257_i2c_read_mask(bq, AW32257_REG_VOLTAGE,
					AW32257_MASK_VO, AW32257_SHIFT_VO);

	if (ret < 0)
		return ret;
	return 10 * (350 + 2*ret);
}

/* Enum of charger current List */
enum CHR_CURRENT_ENUM {
	CHARGE_CURRENT_0_00_MA = 0,
	CHARGE_CURRENT_50_00_MA = 5000,
	CHARGE_CURRENT_62_50_MA = 6250,
	CHARGE_CURRENT_70_00_MA = 7000,
	CHARGE_CURRENT_75_00_MA = 7500,
	CHARGE_CURRENT_87_50_MA = 8750,
	CHARGE_CURRENT_99_00_MA = 9900,
	CHARGE_CURRENT_100_00_MA = 10000,
	CHARGE_CURRENT_125_00_MA = 12500,
	CHARGE_CURRENT_150_00_MA = 15000,
	CHARGE_CURRENT_200_00_MA = 20000,
	CHARGE_CURRENT_225_00_MA = 22500,
	CHARGE_CURRENT_250_00_MA = 25000,
	CHARGE_CURRENT_300_00_MA = 30000,
	CHARGE_CURRENT_350_00_MA = 35000,
	CHARGE_CURRENT_375_00_MA = 37500,
	CHARGE_CURRENT_400_00_MA = 40000,
	CHARGE_CURRENT_425_00_MA = 42500,
	CHARGE_CURRENT_450_00_MA = 45000,
	CHARGE_CURRENT_475_00_MA = 47500,
	CHARGE_CURRENT_500_00_MA = 50000,
	CHARGE_CURRENT_525_00_MA = 52500,
	CHARGE_CURRENT_550_00_MA = 55000,
	CHARGE_CURRENT_600_00_MA = 60000,
	CHARGE_CURRENT_625_00_MA = 62500,
	CHARGE_CURRENT_650_00_MA = 65000,
	CHARGE_CURRENT_675_00_MA = 67500,
	CHARGE_CURRENT_700_00_MA = 70000,
	CHARGE_CURRENT_750_00_MA = 75000,
	CHARGE_CURRENT_775_00_MA = 77500,
	CHARGE_CURRENT_800_00_MA = 80000,
	CHARGE_CURRENT_825_00_MA = 82500,
	CHARGE_CURRENT_850_00_MA = 85000,
	CHARGE_CURRENT_900_00_MA = 90000,
	CHARGE_CURRENT_925_00_MA = 92500,
	CHARGE_CURRENT_950_00_MA = 95000,
	CHARGE_CURRENT_975_00_MA = 97500,
	CHARGE_CURRENT_1000_00_MA = 100000,
	CHARGE_CURRENT_1050_00_MA = 105000,
	CHARGE_CURRENT_1075_00_MA = 107500,
	CHARGE_CURRENT_1100_00_MA = 110000,
	CHARGE_CURRENT_1125_00_MA = 112500,
	CHARGE_CURRENT_1150_00_MA = 115000,
	CHARGE_CURRENT_1200_00_MA = 120000,
	CHARGE_CURRENT_1225_00_MA = 122500,
	CHARGE_CURRENT_1250_00_MA = 125000,
	CHARGE_CURRENT_1275_00_MA = 127500,
	CHARGE_CURRENT_1300_00_MA = 130000,
	CHARGE_CURRENT_1350_00_MA = 135000,
	CHARGE_CURRENT_1375_00_MA = 137500,
	CHARGE_CURRENT_1400_00_MA = 140000,
	CHARGE_CURRENT_1425_00_MA = 142500,
	CHARGE_CURRENT_1450_00_MA = 145000,
	CHARGE_CURRENT_1475_00_MA = 147500,
	CHARGE_CURRENT_1500_00_MA = 150000,
	CHARGE_CURRENT_1525_00_MA = 152500,
	CHARGE_CURRENT_1575_00_MA = 157500,
	CHARGE_CURRENT_1600_00_MA = 160000,
	CHARGE_CURRENT_1650_00_MA = 165000,
	CHARGE_CURRENT_1675_00_MA = 167500,
	CHARGE_CURRENT_1700_00_MA = 170000,
	CHARGE_CURRENT_1725_00_MA = 172500,
	CHARGE_CURRENT_1750_00_MA = 175000,
	CHARGE_CURRENT_1800_00_MA = 180000,
	CHARGE_CURRENT_1825_00_MA = 182500,
	CHARGE_CURRENT_1850_00_MA = 185000,
	CHARGE_CURRENT_1875_00_MA = 187500,
	CHARGE_CURRENT_1900_00_MA = 190000,
	CHARGE_CURRENT_1950_00_MA = 195000,
	CHARGE_CURRENT_1975_00_MA = 197500,
	CHARGE_CURRENT_2000_00_MA = 200000,
	CHARGE_CURRENT_2025_00_MA = 202500,
	CHARGE_CURRENT_2050_00_MA = 205000,
	CHARGE_CURRENT_2100_00_MA = 210000,
	CHARGE_CURRENT_2125_00_MA = 212500,
	CHARGE_CURRENT_2175_00_MA = 217500,
	CHARGE_CURRENT_2200_00_MA = 220000,
	CHARGE_CURRENT_2300_00_MA = 230000,
	CHARGE_CURRENT_2250_00_MA = 225000,
	CHARGE_CURRENT_2275_00_MA = 227500,
	CHARGE_CURRENT_2325_00_MA = 232500,
	CHARGE_CURRENT_2350_00_MA = 235000,
	CHARGE_CURRENT_2400_00_MA = 240000,
	CHARGE_CURRENT_2425_00_MA = 242500,
	CHARGE_CURRENT_2500_00_MA = 250000,
	CHARGE_CURRENT_2575_00_MA = 257500,
	CHARGE_CURRENT_2600_00_MA = 260000,
	CHARGE_CURRENT_2650_00_MA = 265000,
	CHARGE_CURRENT_2700_00_MA = 270000,
	CHARGE_CURRENT_2725_00_MA = 272500,
	CHARGE_CURRENT_2800_00_MA = 280000,
	CHARGE_CURRENT_2875_00_MA = 287500,
	CHARGE_CURRENT_2900_00_MA = 290000,
	CHARGE_CURRENT_3000_00_MA = 300000,
	CHARGE_CURRENT_3100_00_MA = 310000,
	CHARGE_CURRENT_3200_00_MA = 320000,
	CHARGE_CURRENT_MAX
};

const u32 CS_VTH[] = {
	CHARGE_CURRENT_475_00_MA, CHARGE_CURRENT_600_00_MA,
	CHARGE_CURRENT_850_00_MA, CHARGE_CURRENT_975_00_MA,
	CHARGE_CURRENT_1100_00_MA, CHARGE_CURRENT_1225_00_MA,
	CHARGE_CURRENT_1350_00_MA, CHARGE_CURRENT_1475_00_MA,
	CHARGE_CURRENT_1600_00_MA, CHARGE_CURRENT_1725_00_MA,
	CHARGE_CURRENT_1850_00_MA, CHARGE_CURRENT_1975_00_MA,
	CHARGE_CURRENT_2100_00_MA
};

static u32 bmt_find_closest_level(const u32 *pList, u32 number, u32 level)
{
	u32 i, max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = 1;
	else
		max_value_in_last_element = 0;

	if (max_value_in_last_element == 1) {
		for (i = (number - 1); i != 0; i--)
			if (pList[i] <= level)
				return pList[i];
		pr_err("not find closest level:%d,small value first\n", level);
		return pList[0];
		/* return CHARGE_CURRENT_0_00_MA; */
	} else {
		for (i = 0; i < number; i++)
			if (pList[i] <= level)
				return pList[i];
		pr_err("not find closest level:%d,large value first\n", level);
		return pList[number - 1];
		/* return CHARGE_CURRENT_0_00_MA; */
	}
}

static u32 charging_parameter_to_value(const u32 *parameter,
				       const u32 array_size,
				       const u32 val)
{
	u32 i;

	for (i = 0; i < array_size; i++)
		if (val == *(parameter + i))
			return i;
	return 0;
}

/* set charge current in mA (platform data must provide resistor sense) */
static int aw32257_set_charge_current(struct aw32257_device *bq, int uA)
{
	int val, array_size, current_value, set_chr_current;

	if (bq->init_data.resistor_sense <= 0)
		return -EINVAL;

	current_value = uA / 1000;

	if (current_value >= 1200)
		current_value = 1200;

	current_value = current_value * 100;

	array_size = ARRAY_SIZE(CS_VTH);
	set_chr_current = bmt_find_closest_level(CS_VTH, array_size,
						 current_value);
	val = charging_parameter_to_value(CS_VTH, array_size, set_chr_current);

	return aw32257_i2c_write_mask(bq, AW32257_REG_CURRENT, val,
				      AW32257_MASK_VI_CHRG | AW32257_MASK_RESET,
				      AW32257_SHIFT_VI_CHRG);
}

/* get charge current in mA (platform data must provide resistor sense) */
static int aw32257_get_charge_current(struct aw32257_device *bq)
{
	int ret, cur;

	if (bq->init_data.resistor_sense <= 0)
		return -EINVAL;

	ret = aw32257_i2c_read_mask(bq, AW32257_REG_CURRENT,
				    AW32257_MASK_VI_CHRG,
				    AW32257_SHIFT_VI_CHRG);

	if (ret < 0)
		return ret;

	cur = 496 + 124 * ret;

	return cur;
}

/* set termination current in mA (platform data must provide resistor sense) */
static int aw32257_set_termination_current(struct aw32257_device *bq, int uA)
{
	int val, mA;

	mA = uA / 1000;

	if (bq->init_data.resistor_sense <= 0)
		return -EINVAL;

	val = mA / 60;

	if (val < 0)
		val = 0;
	else if (val > 7)
		val = 7;

	return aw32257_i2c_write_mask(bq, AW32257_REG_CURRENT, val,
				      AW32257_MASK_VI_TERM | AW32257_MASK_RESET,
				      AW32257_SHIFT_VI_TERM);
}

/* get termination current in mA (platform data must provide resistor sense) */
static int aw32257_get_termination_current(struct aw32257_device *bq)
{
	int ret, val;

	if (bq->init_data.resistor_sense <= 0)
		return -EINVAL;

	ret = aw32257_i2c_read_mask(bq, AW32257_REG_CURRENT,
				    AW32257_MASK_VI_TERM,
				    AW32257_SHIFT_VI_TERM);
	if (ret < 0)
		return ret;

	val = ret * 60;

	return val;
}

/************** add for otg function *********************/
#define AW32257_OTG_VALID_MS                    500
#define AW32257_FEED_WATCHDOG_VALID_MS          50
#define AW32257_OTG_RETRY_TIMES                 10
#define AW32257_LIMIT_CURRENT_MAX               3200000

#ifdef CONFIG_REGULATOR
void aw32257_set_opa_mode(struct aw32257_device *bq, unsigned int val)
{
	unsigned int ret = 0;

	ret = aw32257_i2c_write_mask(bq, (unsigned char)(aw32257_CON1),
				     (unsigned char)(val),
				     (unsigned char)(CON1_OPA_MODE_MASK),
				     (unsigned char)(CON1_OPA_MODE_SHIFT));
	if (ret)
		dev_err(bq->dev, "set_opa_mode failed\n");

}

void aw32257_set_otg_pl(struct aw32257_device *bq, unsigned int val)
{
	unsigned int ret = 0;

	ret = aw32257_i2c_write_mask(bq, (unsigned char)(aw32257_CON2),
				     (unsigned char)(val),
				     (unsigned char)(CON2_OTG_PL_MASK),
				     (unsigned char)(CON2_OTG_PL_SHIFT));
	if (ret)
		dev_err(bq->dev, "set_otg_pl failed\n");
}

void aw32257_set_otg_en(struct aw32257_device *bq, unsigned int val)
{
	unsigned int ret = 0;

	ret = aw32257_i2c_write_mask(bq, (unsigned char)(aw32257_CON2),
				     (unsigned char)(val),
				     (unsigned char)(CON2_OTG_EN_MASK),
				     (unsigned char)(CON2_OTG_EN_SHIFT));
	if (ret)
		dev_err(bq->dev, "set_otg_en failed\n");
}

static int aw32257_enable_otg(struct aw32257_device *bq, bool en)
{
	if (en) {
		aw32257_set_opa_mode(bq, 1);
		aw32257_set_otg_pl(bq, 1);
		aw32257_set_otg_en(bq, 1);
	} else {
		aw32257_i2c_write_mask(bq, 0x01, 0x30, 0xff, 0x00);
		aw32257_i2c_write_mask(bq, 0x02, 0x8e, 0xff, 0x00);
	}
	return 0;
}

static void aw32257_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct aw32257_device *info = container_of(dwork,
					struct aw32257_device, otg_work);
	bool otg_valid = extcon_get_state(info->edev, EXTCON_USB);
	int retry = 0;

	if (otg_valid)
		goto out;

	do {
		aw32257_enable_otg(info, 1);
		otg_valid = extcon_get_state(info->edev, EXTCON_USB);
	} while (!otg_valid && retry++ < AW32257_OTG_RETRY_TIMES);

	if (retry >= AW32257_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int aw32257_otg_charger_enable_otg(struct regulator_dev *dev)
{
	struct aw32257_device *info = rdev_get_drvdata(dev);
	int ret;

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		return ret;
	}

	aw32257_enable_otg(info, 1);

	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(AW32257_OTG_VALID_MS));

	return 0;
}

static int aw32257_otg_charger_disable_otg(struct regulator_dev *dev)
{
	struct aw32257_device *info = rdev_get_drvdata(dev);

	cancel_delayed_work_sync(&info->otg_work);
	aw32257_enable_otg(info, 0);
	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
							  BIT_DP_DM_BC_ENB, 0);
}

static int aw32257_otg_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct aw32257_device *info = rdev_get_drvdata(dev);
	int ret;

	ret = aw32257_i2c_read_mask(info, aw32257_CON1,
					CON0_OTG_MASK, CON1_OTG_SHIFT);

	return ret;
}

static const struct regulator_ops aw32257_otg_charger_vbus_ops = {
	.enable = aw32257_otg_charger_enable_otg,
	.disable = aw32257_otg_charger_disable_otg,
	.is_enabled = aw32257_otg_charger_vbus_is_enabled,
};

static const struct regulator_desc aw32257_otg_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &aw32257_otg_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int aw32257_charger_register_vbus_regulator(struct aw32257_device *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &aw32257_otg_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}
#else
static int aw32257_charger_register_vbus_regulator(struct aw32257_device *info)
{
	return 0;
}
#endif

/* set default value of property */
#define aw32257_set_default_value(bq, prop) \
	do { \
		int ret = 0; \
		if (bq->init_data.prop != -1) \
			ret = aw32257_set_##prop(bq, bq->init_data.prop); \
		if (ret < 0) \
			return ret; \
	} while (0)

#define _BATTERY_NAME				"sc27xx-fgu"
static bool aw32257_charger_is_bat_present(struct aw32257_device *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}

	val.intval = 0;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if (ret == 0 && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev,
			"Failed to get property of present:%d\n", ret);

	return present;
}

static int aw32257_charger_start_charge(struct aw32257_device *info)
{
	int ret;

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask, 0);
	if (ret)
		dev_err(info->dev, "enable charge failed\n");

	dev_dbg(info->dev, "bq start charge\n");

#ifdef CONFIG_CHARGE_PD
	gpio_set_value(chg_pd_gpio, 0);
#else
	aw32257_exec_command(info, AW32257_CHARGER_ENABLE);
#endif

	return ret;
}

static void aw32257_charger_stop_charge(struct aw32257_device *info)
{
	int ret;

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
				 info->charger_pd_mask);
	if (ret)
		dev_err(info->dev, "disable charge failed\n");

	dev_dbg(info->dev, "bq stop charge\n");
#ifdef CONFIG_CHARGE_PD
	gpio_set_value(chg_pd_gpio, 1);
#else
	aw32257_exec_command(info, AW32257_CHARGER_DISABLE);
#endif
}

static void aw32257_charger_work(struct work_struct *data)
{
	struct aw32257_device *info =
		container_of(data, struct aw32257_device, work);
	bool present = aw32257_charger_is_bat_present(info);

	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->charger, CM_EVENT_CHG_START_STOP, NULL);
}

static int aw32257_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct aw32257_device *info =
		container_of(nb, struct aw32257_device, usb_notify);

	info->limit = limit;

	pm_wakeup_event(info->dev, AW32257_WAKE_UP_MS);
	schedule_work(&info->work);

	return NOTIFY_OK;
}

static int aw32257_charger_get_online(struct aw32257_device *info,
				      int *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int aw32257_charger_get_health(struct aw32257_device *info,
				      int *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int aw32257_charger_get_status(struct aw32257_device *info)
{
	if (info->charging == true)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int aw32257_charger_set_status(struct aw32257_device *info,
				      int val)
{
	int ret = 0;

	if (!val && info->charging) {
		aw32257_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = aw32257_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}
int aw32257_charger_get_battery_cur(struct power_supply *psy,
				    struct aw32257_charge_current *cur)
{
	struct device_node *battery_np;
	const char *value;
	int err;

	cur->sdp_cur = -EINVAL;
	cur->sdp_limit = -EINVAL;
	cur->dcp_cur = -EINVAL;
	cur->dcp_limit = -EINVAL;
	cur->cdp_cur = -EINVAL;
	cur->cdp_limit = -EINVAL;
	cur->unknown_cur = -EINVAL;
	cur->unknown_limit = -EINVAL;

	if (!psy->of_node) {
		dev_warn(&psy->dev, "%s currently only supports devicetree\n",
			 __func__);
		return -ENXIO;
	}

	battery_np = of_parse_phandle(psy->of_node, "monitored-battery", 0);
	if (!battery_np)
		return -ENODEV;

	err = of_property_read_string(battery_np, "compatible", &value);
	if (err)
		goto out_put_node;

	if (strcmp("simple-battery", value)) {
		err = -ENODEV;
		goto out_put_node;
	}

	of_property_read_u32_index(battery_np, "charge-sdp-current-microamp", 0,
				   &cur->sdp_cur);
	of_property_read_u32_index(battery_np, "charge-sdp-current-microamp", 1,
				   &cur->sdp_limit);
	of_property_read_u32_index(battery_np, "charge-dcp-current-microamp", 0,
				   &cur->dcp_cur);
	of_property_read_u32_index(battery_np, "charge-dcp-current-microamp", 1,
				   &cur->dcp_limit);
	of_property_read_u32_index(battery_np, "charge-cdp-current-microamp", 0,
				   &cur->cdp_cur);
	of_property_read_u32_index(battery_np, "charge-cdp-current-microamp", 1,
				   &cur->cdp_limit);
	of_property_read_u32_index(battery_np, "charge-unknown-current-microamp", 0,
				   &cur->unknown_cur);
	of_property_read_u32_index(battery_np, "charge-unknown-current-microamp", 1,
				   &cur->unknown_limit);

out_put_node:
	of_node_put(battery_np);
	return err;
}

static int aw32257_charger_hw_init(struct aw32257_device *info)
{
	struct power_supply_battery_info bat_info = { };
	int voltage_max_microvolt;
	int ret;

	ret = aw32257_charger_get_battery_cur(info->charger, &info->cur);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 100 mA, and default
		 * charge termination voltage to 4.2V.
		 */
		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 5000000;
		info->cur.dcp_cur = 500000;
		info->cur.cdp_limit = 5000000;
		info->cur.cdp_cur = 1500000;
		info->cur.unknown_limit = 5000000;
		info->cur.unknown_cur = 500000;
	}

	ret = power_supply_get_battery_info(info->charger, &bat_info);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");
		voltage_max_microvolt = 4440;
	} else {
		voltage_max_microvolt = bat_info.constant_charge_voltage_max_uv / 1000;

		if (aw32257_set_battery_regulation_voltage(info, bat_info.constant_charge_voltage_max_uv) < 0)
			dev_err(info->dev, "set regulation voltage failed\n");

		power_supply_put_battery_info(info->charger, &bat_info);
	}

	aw32257_enable_otg(info, 0);

	return ret;
}

static void aw32257_charger_detect_status(struct aw32257_device *info)
{
	int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;
	dev_info(info->dev, "%s limit = %d\n", __func__, min);
	schedule_work(&info->work);
}

/**** power supply interface code ****/

static enum power_supply_property aw32257_power_supply_props[] = {
	/* TODO: maybe add more power supply properties */

	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_TEMP_ALERT_MAX,
	POWER_SUPPLY_PROP_PRECHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static enum power_supply_usb_type aw32257_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static int aw32257_property_is_writeable(struct power_supply *psy,
					 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
			ret = 1;
			break;
	default:
			ret = 0;
	}

	return ret;
}

static int aw32257_power_supply_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct aw32257_device *bq = power_supply_get_drvdata(psy);

	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = aw32257_set_charge_current(bq, val->intval);
		if (ret < 0)
			dev_err(bq->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = aw32257_charger_set_status(bq, val->intval);
		if (ret < 0)
			dev_err(bq->dev, "set charge status failed\n");
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = aw32257_set_battery_regulation_voltage(bq, val->intval);
		if (ret < 0)
			dev_err(bq->dev, "set regulation voltage failed\n");
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int aw32257_power_supply_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct aw32257_device *bq = power_supply_get_drvdata(psy);
	int ret = 0;
	int online, health, type, cur;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (bq->limit)
			val->intval = aw32257_charger_get_status(bq);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = bq->model;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!bq->charging) {
			val->intval = 0;
		} else {
			cur = aw32257_get_charge_current(bq);
			val->intval = cur * 1000;
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!bq->charging) {
			val->intval = 0;
		} else {
			switch (bq->usb_phy->chg_type) {
			case SDP_TYPE:
				val->intval = bq->cur.sdp_cur;
				break;
			case DCP_TYPE:
				val->intval = bq->cur.dcp_cur;
				break;
			case CDP_TYPE:
				val->intval = bq->cur.cdp_cur;
				break;
			default:
				val->intval = bq->cur.unknown_cur;
			}
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = aw32257_charger_get_online(bq, &online);
		if (ret)
			return -EINVAL;

		val->intval = online;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (bq->charging) {
			val->intval = 0;
		} else {
			aw32257_charger_get_health(bq, &health);
			val->intval = health;
		}

		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		type = bq->usb_phy->chg_type;

		switch (type) {
		case SDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
			break;

		case DCP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			break;

		case CDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
			break;

		default:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static int aw32257_power_supply_init(struct aw32257_device *bq,
				     struct device_node *np)
{
	int ret, chip;
	char revstr[8];
	struct power_supply_config psy_cfg = { .drv_data = bq, };

	bq->charger_desc.type = POWER_SUPPLY_TYPE_USB;
	bq->charger_desc.properties = aw32257_power_supply_props;
	bq->charger_desc.num_properties =
			ARRAY_SIZE(aw32257_power_supply_props);
	bq->charger_desc.get_property = aw32257_power_supply_get_property;
	bq->charger_desc.set_property = aw32257_power_supply_set_property;
	bq->charger_desc.property_is_writeable = aw32257_property_is_writeable;
	bq->charger_desc.usb_types = aw32257_usb_types;
	bq->charger_desc.num_usb_types = ARRAY_SIZE(aw32257_usb_types);
	psy_cfg.of_node = np;

	ret = aw32257_detect_chip(bq);

	if (ret < 0)
		chip = BQUNKNOWN;
	else
		chip = ret;

	ret = aw32257_detect_revision(bq);
	if (ret < 0) {
		strncpy(revstr, "unknown", sizeof(revstr)-1);
		revstr[sizeof(revstr)-1] = '\0';
	} else {
		sprintf(revstr, "1.%d", ret);
	}

	bq->model = kasprintf(GFP_KERNEL,
				"chip %s, revision %s, vender code %.3d",
				aw32257_chip_name[chip], revstr,
				aw32257_get_vender_code(bq));
	if (!bq->model) {
		dev_err(bq->dev, "failed to allocate model name\n");
		return -ENOMEM;
	}

	bq->charger = devm_power_supply_register(bq->dev, &bq->charger_desc,
					    &psy_cfg);
	if (IS_ERR(bq->charger)) {
		kfree(bq->model);
		return PTR_ERR(bq->charger);
	}

	return 0;
}

static void aw32257_power_supply_exit(struct aw32257_device *bq)
{
	cancel_work_sync(&bq->work);
	power_supply_unregister(bq->charger);
	kfree(bq->model);
}

/**** additional sysfs entries for power supply interface ****/

/* show *_status entries */
static ssize_t aw32257_sysfs_show_status(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct aw32257_device *bq = power_supply_get_drvdata(psy);
	enum aw32257_command command;
	int ret;

	if (strcmp(attr->attr.name, "otg_status") == 0)
		command = AW32257_OTG_STATUS;
	else if (strcmp(attr->attr.name, "charge_status") == 0)
		command = AW32257_CHARGE_STATUS;
	else if (strcmp(attr->attr.name, "boost_status") == 0)
		command = AW32257_BOOST_STATUS;
	else if (strcmp(attr->attr.name, "fault_status") == 0)
		command = AW32257_FAULT_STATUS;
	else
		return -EINVAL;

	ret = aw32257_exec_command(bq, command);
	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", ret);
}

/* show reported_mode entry (none, host, dedicated or boost) */
static ssize_t aw32257_sysfs_show_reported_mode(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct aw32257_device *bq = power_supply_get_drvdata(psy);

	switch (bq->reported_mode) {
	case AW32257_MODE_OFF:
		return sprintf(buf, "off\n");
	case AW32257_MODE_NONE:
		return sprintf(buf, "none\n");
	case AW32257_MODE_HOST_CHARGER:
		return sprintf(buf, "host\n");
	case AW32257_MODE_DEDICATED_CHARGER:
		return sprintf(buf, "dedicated\n");
	case AW32257_MODE_BOOST:
		return sprintf(buf, "boost\n");
	}

	return -EINVAL;
}

/* directly set raw value to chip register, format: 'register value' */
static ssize_t aw32257_sysfs_set_registers(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct aw32257_device *bq = power_supply_get_drvdata(psy);
	ssize_t ret = 0;
	unsigned int reg, val;

	if (sscanf(buf, "%x %x", &reg, &val) != 2)
		return -EINVAL;

	if (reg > 4 || val > 255)
		return -EINVAL;

	ret = aw32257_i2c_write(bq, reg, val);
	if (ret < 0)
		return ret;
	return count;
}

/* print value of chip register, format: 'register=value' */
static ssize_t aw32257_sysfs_print_reg(struct aw32257_device *bq,
				       u8 reg,
				       char *buf)
{
	int ret = aw32257_i2c_read(bq, reg);

	if (ret < 0)
		return sprintf(buf, "%#.2x=error %d\n", reg, ret);
	return sprintf(buf, "%#.2x=%#.2x\n", reg, ret);
}

/* show all raw values of chip register, format per line: 'register=value' */
static ssize_t aw32257_sysfs_show_registers(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct aw32257_device *bq = power_supply_get_drvdata(psy);
	ssize_t ret = 0;

	ret += aw32257_sysfs_print_reg(bq, AW32257_REG_STATUS, buf+ret);
	ret += aw32257_sysfs_print_reg(bq, AW32257_REG_CONTROL, buf+ret);
	ret += aw32257_sysfs_print_reg(bq, AW32257_REG_VOLTAGE, buf+ret);
	ret += aw32257_sysfs_print_reg(bq, AW32257_REG_VENDER, buf+ret);
	ret += aw32257_sysfs_print_reg(bq, AW32257_REG_CURRENT, buf+ret);
	return ret;
}

/* set current and voltage limit entries (in mA or mV) */
static ssize_t aw32257_sysfs_set_limit(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct aw32257_device *bq = power_supply_get_drvdata(psy);
	long val;
	int ret;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	if (strcmp(attr->attr.name, "current_limit") == 0)
		ret = aw32257_set_current_limit(bq, val);
	else if (strcmp(attr->attr.name, "weak_battery_voltage") == 0)
		ret = aw32257_set_weak_battery_voltage(bq, val);
	else if (strcmp(attr->attr.name, "battery_regulation_voltage") == 0)
		ret = aw32257_set_battery_regulation_voltage(bq, val);
	else if (strcmp(attr->attr.name, "charge_current") == 0)
		ret = aw32257_set_charge_current(bq, val);
	else if (strcmp(attr->attr.name, "termination_current") == 0)
		ret = aw32257_set_termination_current(bq, val);
	else
		return -EINVAL;

	if (ret < 0)
		return ret;
	return count;
}

/* show current and voltage limit entries (in mA or mV) */
static ssize_t aw32257_sysfs_show_limit(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct aw32257_device *bq = power_supply_get_drvdata(psy);
	int ret;

	if (strcmp(attr->attr.name, "current_limit") == 0)
		ret = aw32257_get_current_limit(bq);
	else if (strcmp(attr->attr.name, "weak_battery_voltage") == 0)
		ret = aw32257_get_weak_battery_voltage(bq);
	else if (strcmp(attr->attr.name, "battery_regulation_voltage") == 0)
		ret = aw32257_get_battery_regulation_voltage(bq);
	else if (strcmp(attr->attr.name, "charge_current") == 0)
		ret = aw32257_get_charge_current(bq);
	else if (strcmp(attr->attr.name, "termination_current") == 0)
		ret = aw32257_get_termination_current(bq);
	else
		return -EINVAL;

	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", ret);
}

/* set *_enable entries */
static ssize_t aw32257_sysfs_set_enable(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct aw32257_device *bq = power_supply_get_drvdata(psy);
	enum aw32257_command command;
	long val;
	int ret;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	if (strcmp(attr->attr.name, "charge_termination_enable") == 0)
		command = val ? AW32257_CHARGE_TERMINATION_ENABLE :
			AW32257_CHARGE_TERMINATION_DISABLE;
	else if (strcmp(attr->attr.name, "high_impedance_enable") == 0)
		command = val ? AW32257_HIGH_IMPEDANCE_ENABLE :
			AW32257_HIGH_IMPEDANCE_DISABLE;
	else if (strcmp(attr->attr.name, "otg_pin_enable") == 0)
		command = val ? AW32257_OTG_PIN_ENABLE :
			AW32257_OTG_PIN_DISABLE;
	else if (strcmp(attr->attr.name, "stat_pin_enable") == 0)
		command = val ? AW32257_STAT_PIN_ENABLE :
			AW32257_STAT_PIN_DISABLE;
	else
		return -EINVAL;

	ret = aw32257_exec_command(bq, command);
	if (ret < 0)
		return ret;
	return count;
}

/* show *_enable entries */
static ssize_t aw32257_sysfs_show_enable(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct aw32257_device *bq = power_supply_get_drvdata(psy);
	enum aw32257_command command;
	int ret;

	if (strcmp(attr->attr.name, "charge_termination_enable") == 0)
		command = AW32257_CHARGE_TERMINATION_STATUS;
	else if (strcmp(attr->attr.name, "high_impedance_enable") == 0)
		command = AW32257_HIGH_IMPEDANCE_STATUS;
	else if (strcmp(attr->attr.name, "otg_pin_enable") == 0)
		command = AW32257_OTG_PIN_STATUS;
	else if (strcmp(attr->attr.name, "stat_pin_enable") == 0)
		command = AW32257_STAT_PIN_STATUS;
	else
		return -EINVAL;

	ret = aw32257_exec_command(bq, command);
	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", ret);
}

static DEVICE_ATTR(current_limit, 0644,
		aw32257_sysfs_show_limit, aw32257_sysfs_set_limit);
static DEVICE_ATTR(weak_battery_voltage, 0644,
		aw32257_sysfs_show_limit, aw32257_sysfs_set_limit);
static DEVICE_ATTR(battery_regulation_voltage, 0644,
		aw32257_sysfs_show_limit, aw32257_sysfs_set_limit);
static DEVICE_ATTR(charge_current, 0644,
		aw32257_sysfs_show_limit, aw32257_sysfs_set_limit);
static DEVICE_ATTR(termination_current, 0644,
		aw32257_sysfs_show_limit, aw32257_sysfs_set_limit);

static DEVICE_ATTR(charge_termination_enable, 0644,
		aw32257_sysfs_show_enable, aw32257_sysfs_set_enable);
static DEVICE_ATTR(high_impedance_enable, 0644,
		aw32257_sysfs_show_enable, aw32257_sysfs_set_enable);
static DEVICE_ATTR(otg_pin_enable, 0644,
		aw32257_sysfs_show_enable, aw32257_sysfs_set_enable);
static DEVICE_ATTR(stat_pin_enable, 0644,
		aw32257_sysfs_show_enable, aw32257_sysfs_set_enable);

static DEVICE_ATTR(reported_mode, 0444,
		aw32257_sysfs_show_reported_mode, NULL);

static DEVICE_ATTR(registers, 0644,
		aw32257_sysfs_show_registers, aw32257_sysfs_set_registers);

static DEVICE_ATTR(otg_status, 0444, aw32257_sysfs_show_status, NULL);
static DEVICE_ATTR(charge_status, 0444, aw32257_sysfs_show_status, NULL);
static DEVICE_ATTR(boost_status, 0444, aw32257_sysfs_show_status, NULL);
static DEVICE_ATTR(fault_status, 0444, aw32257_sysfs_show_status, NULL);

static struct attribute *aw32257_sysfs_attributes[] = {
	/*
	 * TODO: some (appropriate) of these attrs should be switched to
	 * use power supply class props.
	 */
	&dev_attr_current_limit.attr,
	&dev_attr_weak_battery_voltage.attr,
	&dev_attr_battery_regulation_voltage.attr,
	&dev_attr_charge_current.attr,
	&dev_attr_termination_current.attr,

	&dev_attr_charge_termination_enable.attr,
	&dev_attr_high_impedance_enable.attr,
	&dev_attr_otg_pin_enable.attr,
	&dev_attr_stat_pin_enable.attr,

	&dev_attr_reported_mode.attr,

	&dev_attr_registers.attr,

	&dev_attr_otg_status.attr,
	&dev_attr_charge_status.attr,
	&dev_attr_boost_status.attr,
	&dev_attr_fault_status.attr,
	NULL,
};

static const struct attribute_group aw32257_sysfs_attr_group = {
	.attrs = aw32257_sysfs_attributes,
};

static int aw32257_sysfs_init(struct aw32257_device *bq)
{
	return sysfs_create_group(&bq->charger->dev.kobj,
			&aw32257_sysfs_attr_group);
}

static void aw32257_sysfs_exit(struct aw32257_device *bq)
{
	sysfs_remove_group(&bq->charger->dev.kobj, &aw32257_sysfs_attr_group);
}

static int aw32257_dts_init(struct aw32257_device *bq, struct device_node *np)
{
	int ret;

	if (!(bq->dev))
		return -EINVAL;

#ifdef CONFIG_CHARGE_PD
	chg_pd_gpio = of_get_named_gpio(np, "charge,chg-pd-gpio", 0);
	if (chg_pd_gpio < 0)
		dev_err(bq->dev, "Unable to get chg-pd gpio\n");
	else {
		ret = gpio_request(chg_pd_gpio, NULL);
		if (ret < 0)
			dev_err(bq->dev, "gpio_request error\n");

		gpio_direction_output(chg_pd_gpio, 0);
	}
#endif

	ret = device_property_read_u32(bq->dev,
				       "ti,current-limit",
				       &bq->init_data.current_limit);
	if (ret)
		goto error_3;
	ret = device_property_read_u32(bq->dev,
				       "ti,weak-battery-voltage",
				       &bq->init_data.weak_battery_voltage);
	if (ret)
		goto error_3;
	ret = device_property_read_u32(bq->dev,
				       "ti,battery-regulation-voltage",
				       &bq->init_data.battery_regulation_voltage);
	if (ret)
		goto error_3;
	ret = device_property_read_u32(bq->dev,
				       "ti,charge-current",
				       &bq->init_data.charge_current);
	if (ret)
		goto error_3;
	ret = device_property_read_u32(bq->dev,
				       "ti,termination-current",
				       &bq->init_data.termination_current);
	if (ret)
		goto error_3;
	ret = device_property_read_u32(bq->dev,
				       "ti,resistor-sense",
				       &bq->init_data.resistor_sense);
	if (ret)
		goto error_3;
error_3:
	return ret;
}

static int aw32257_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int ret;
	struct platform_device *regmap_pdev;
	struct aw32257_device *bq;
	struct device *dev = &client->dev;
	struct device_node *np = client->dev.of_node;
	struct device_node *regmap_np;
	struct aw32257_platform_data *pdata = client->dev.platform_data;

	if (!np && !pdata && !ACPI_HANDLE(dev)) {
		dev_err(dev, "Neither devicetree, nor platform data, nor ACPI support\n");
		return -ENODEV;
	}

	bq = devm_kzalloc(dev, sizeof(*bq), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	if (id)
		bq->chip = id->driver_data;

	dev_info(dev, "id name = %s\n", id->name);
	bq->dev = dev;
	bq->mode = AW32257_MODE_OFF;
	bq->reported_mode = AW32257_MODE_OFF;
	i2c_set_clientdata(client, bq);

	bq->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(bq->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		ret = PTR_ERR(bq->usb_phy);
		goto error_1;
	}

	ret = aw32257_charger_register_vbus_regulator(bq);
	if (ret) {
		dev_err(bq->dev, "failed to register vbus regulator.\n");
		goto error_1;
	}

	bq->edev = extcon_get_edev_by_phandle(bq->dev, 0);
	if (IS_ERR(bq->edev)) {
		dev_err(bq->dev, "failed to find vbus extcon device.\n");
		ret = PTR_ERR(bq->edev);
		goto error_1;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		dev_err(dev, "unable to get syscon node\n");
		ret =  -ENODEV;
		goto error_1;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &bq->charger_detect);
	if (ret) {
		dev_err(bq->dev, "failed to get charger_detect\n");
		ret = -EINVAL;
		goto error_1;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &bq->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		goto error_1;
	}

	if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
		bq->charger_pd_mask = AW32257_DISABLE_PIN_MASK_2721;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
		bq->charger_pd_mask = AW32257_DISABLE_PIN_MASK_2720;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2730"))
		bq->charger_pd_mask = AW32257_DISABLE_PIN_MASK_2730;
	else {
		dev_err(dev, "failed to get charger_pd mask\n");
		ret = -EINVAL;
		goto error_1;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		ret =  -ENODEV;
		goto error_1;
	}

	of_node_put(regmap_np);
	bq->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!bq->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		ret =  -ENODEV;
		goto error_1;
	}

	mutex_init(&bq->lock);
	INIT_WORK(&bq->work, aw32257_charger_work);
	INIT_DELAYED_WORK(&bq->otg_work, aw32257_charger_otg_work);

	bq->charger_desc.name = id->name;

	ret = aw32257_power_supply_init(bq, np);
	if (ret) {
		dev_err(dev, "failed to register power supply: %d\n", ret);
		goto error_1;
	}

#ifdef CONFIG_CHARGE_PD
	ret = register_reboot_notifier(&chg_pd_notifier);
	if (ret) {
		dev_err(dev, "register_reboot_notifier error: %d\n", ret);
		goto error_1;
	}
#endif

	ret = aw32257_sysfs_init(bq);
	if (ret) {
		dev_err(dev, "failed to create sysfs entries: %d\n", ret);
		goto error_2;
	}

	ret = aw32257_dts_init(bq, np);
	if (ret) {
		dev_err(dev, "failed init dts\n");
		goto error_3;
	}

	ret = aw32257_charger_hw_init(bq);
	if (ret) {
		dev_err(dev, "failed to hw init\n");
		goto error_3;
	}

	aw32257_charger_stop_charge(bq);

	device_init_wakeup(bq->dev, true);
	bq->usb_notify.notifier_call = aw32257_charger_usb_change;
	ret = usb_register_notifier(bq->usb_phy, &bq->usb_notify);
	if (ret) {
		dev_err(bq->dev, "failed to register notifier:%d\n", ret);
		goto error_3;
	}

	aw32257_set_termination_current(bq, bq->init_data.termination_current);

	aw32257_charger_detect_status(bq);

	dev_info(dev, "driver registered suc\n");
	return 0;

error_3:
	aw32257_sysfs_exit(bq);
error_2:
	unregister_reboot_notifier(&chg_pd_notifier);
error_1:
	devm_kfree(&client->dev, bq);
	return ret;
}

static int aw32257_remove(struct i2c_client *client)
{
	struct aw32257_device *bq = i2c_get_clientdata(client);

	if (bq->usb_notify.notifier_call)
		usb_unregister_notifier(bq->usb_phy, &bq->usb_notify);

	aw32257_sysfs_exit(bq);
	aw32257_power_supply_exit(bq);

	aw32257_reset_chip(bq);

	dev_info(bq->dev, "driver unregistered\n");

	return 0;
}

static const struct i2c_device_id aw32257_i2c_id_table[] = {
	{ "aw32257", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, aw32257_i2c_id_table);

#ifdef CONFIG_OF
static const struct of_device_id aw32257_of_match_table[] = {
	{ .compatible = "aw32257" },
	{},
};
MODULE_DEVICE_TABLE(of, aw32257_of_match_table);
#endif

static struct i2c_driver aw32257_driver = {
	.driver = {
		.name = "aw32257-charger",
		.of_match_table = of_match_ptr(aw32257_of_match_table),
	},
	.probe = aw32257_probe,
	.remove = aw32257_remove,
	.id_table = aw32257_i2c_id_table,
};
module_i2c_driver(aw32257_driver);

MODULE_DESCRIPTION("aw32257 charger driver");
MODULE_LICENSE("GPL v2");
