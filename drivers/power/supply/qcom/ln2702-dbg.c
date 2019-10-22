/*
 * Driver (skeleton code) for LIONSEMI LN2702 SC Voltage Regulator
 *
 * Copyright (C) 2019 Lion Semiconductor Inc.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * Author: Jae Lee <kjaelee@lionsemi.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/power_supply.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include <linux/power/ln2702.h>

#define LN2702_DRIVER_NAME	"ln2702"
#define LN2702_HW_REV_A1


/*********************************************************************************
 * DESIGN GUIDE:
 *********************************************************************************/













/*********************************************************************************
 * register map
 *********************************************************************************/


#define LN2702_MASK_SC_OPERATION_MODE	0x03

#ifdef LN2702_HW_REV_A1
   #define LN2702_MASK_BC_SYS		0x0F
#elif  LN2702_HW_REV_B0
   #define LN2702_MASK_BC_SYS		0x1F
#endif


#define LN2702_REG_DEVICE_ID		0x00
#define LN2702_REG_INT_DEVICE_0		0x01
#define LN2702_REG_INT_DEVICE_1		0x02
#define LN2702_REG_INT_HV_SC_0		0x03
#define LN2702_REG_INT_HV_SC_1		0x04
#define LN2702_REG_INT_DEVICE_0_MSK	0x05
#define LN2702_REG_INT_DEVICE_1_MSK	0x06
#define LN2702_REG_INT_HV_SC_0_MSK	0x07
#define LN2702_REG_INT_HV_SC_1_MSK	0x08
#define LN2702_REG_INT_DEVICE_0_STS	0x09
#define LN2702_REG_INT_DEVICE_1_STS	0x0A
#define LN2702_REG_INT_HV_SC_0_STS	0x0B
#define LN2702_REG_INT_HV_SC_1_STS	0x0C
#define LN2702_REG_DEVICE_CTRL_0	0x0D
#define LN2702_REG_DEVICE_CTRL_1	0x0E
#define LN2702_REG_HV_SC_CTRL_0		0x0F
#define LN2702_REG_HV_SC_CTRL_1		0x10
#define LN2702_REG_HV_SC_CTRL_2		0x11
#define LN2702_REG_SC_DITHER_CTRL	0x12
#define LN2702_REG_GLITCH_CTRL		0x13
#define LN2702_REG_FAULT_CTRL		0x14
#define LN2702_REG_TRACK_CTRL		0x15

#define LN2702_REG_STS_D		0x3A
#define LN2702_REG_DEVICE_MARKER	0x46
#define LN2702_MAX_REGISTER		LN2702_REG_DEVICE_MARKER


/*********************************************************************************
 * data structures / platform data
 *********************************************************************************/



enum {
    LN2702_STATE_UNKNOWN = -1,
    LN2702_STATE_IDLE = 2,
    LN2702_STATE_SW_ACTIVE  = 7,
    LN2702_STATE_BYPASS_ACTIVE  = 12,
};

/**
 * struct ln2702_info - ln2702 regulator instance
 * @monitor_wake_lock: lock to enter the suspend mode
 * @lock: protects concurrent access to online variables
 * @client: pointer to client
 * @regmap: pointer to driver regmap
 * @op_mode : chip operation mode (STANDBY, BYPASS, SWITCHING)
 * @reverse_power : enable reverse power path
 * @pdata: pointer to platform data
 */
struct ln2702_info {

	struct i2c_client	*client;
	struct mutex		lock;
	struct regmap		*regmap;



	unsigned int		op_mode;
	bool			reverse_power;
	bool			auto_recovery;


};


static const struct regmap_config ln2702_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= LN2702_MAX_REGISTER,
};


/*********************************************************************************
 * supported functionality
 *********************************************************************************/

void ln2702_use_ext_5V(struct ln2702_info *info, unsigned int enable)
{
	regmap_update_bits(info->regmap, LN2702_REG_DEVICE_CTRL_1,
				0x01,
				(enable & 0x01));
}
EXPORT_SYMBOL(ln2702_use_ext_5V);


void ln2702_set_infet(struct ln2702_info *info, unsigned int enable)
{
	regmap_update_bits(info->regmap, LN2702_REG_DEVICE_CTRL_0,
				0x01,
				(enable & 0x01));
}
EXPORT_SYMBOL(ln2702_set_infet);


void ln2702_set_powerpath(struct ln2702_info *info, bool forward_path) {
	info->reverse_power = (!forward_path);
}
EXPORT_SYMBOL(ln2702_set_powerpath);


static inline int ln2702_get_opmode(struct ln2702_info *info)
{
	int val;
	if (regmap_read(info->regmap, LN2702_REG_HV_SC_CTRL_0, &val) < 0)
	   val = LN2702_OPMODE_UNKNOWN;
	return (val & LN2702_MASK_SC_OPERATION_MODE);
}



/* configure minimum set of control registers */
static inline void ln2702_set_base_opt(struct ln2702_info *info,
			unsigned int sc_out_precharge_cfg,
			unsigned int precharge_fault_chk_en,
			unsigned int track_cfg)
{
	regmap_update_bits(info->regmap, LN2702_REG_HV_SC_CTRL_1, 0x20,
				(sc_out_precharge_cfg<<5));
	regmap_update_bits(info->regmap, LN2702_REG_TRACK_CTRL,
				0x30,
				(precharge_fault_chk_en<<5) |
				(track_cfg<<4));
}

static inline void ln2702_set_fault_opt(struct ln2702_info *info,
			bool set_ov,
			unsigned int disable_vbus_in_switch_ok,
			unsigned int disable_sc_out_min_ok,
			unsigned int disable_wpc_in_min_ok,
			unsigned int disable_vbus_in_min_ok)
{
	if (set_ov) {
	   regmap_update_bits(info->regmap, LN2702_REG_FAULT_CTRL, 0x3F,
				(3<<4) |
				(disable_vbus_in_switch_ok<<3) |
				(disable_sc_out_min_ok<<2) |
				(disable_wpc_in_min_ok<<1) |
				(disable_vbus_in_min_ok));
	} else {
	   regmap_update_bits(info->regmap, LN2702_REG_FAULT_CTRL, 0x3F,
				(disable_vbus_in_switch_ok<<3) |
				(disable_sc_out_min_ok<<2) |
				(disable_wpc_in_min_ok<<1) |
				(disable_vbus_in_min_ok));
	}
}


static void ln2702_enter_standby(struct ln2702_info *info)
{

	regmap_update_bits(info->regmap, LN2702_REG_HV_SC_CTRL_0,
				LN2702_MASK_SC_OPERATION_MODE,
				LN2702_OPMODE_STANDBY);

	ln2702_set_base_opt(info, 1, 1, 1);
	ln2702_set_fault_opt(info, false, 0, 1, 0, 0);
}


static void ln2702_enter_bypass(struct ln2702_info *info)
{
	ln2702_set_base_opt(info, 1, 1, 1);

	if (info->reverse_power)
	   ln2702_set_fault_opt(info, false, 1, 0, 1, 1);
	else
	   ln2702_set_fault_opt(info, false, 1, 1, 0, 0);


	regmap_update_bits(info->regmap, LN2702_REG_HV_SC_CTRL_0,
			LN2702_MASK_SC_OPERATION_MODE, LN2702_OPMODE_BYPASS);
}

static void ln2702_enter_switching(struct ln2702_info *info)
{
	if (info->reverse_power) {
	   ln2702_set_base_opt(info, 0, 0, 0);
	   ln2702_set_fault_opt(info, true, 1, 1, 0, 1);
	} else {
#ifdef LN2702_HW_REV_A1
	   ln2702_set_base_opt(info, 0, 0, 0);
#else
	   ln2702_set_base_opt(info, 0, 1, 0);
#endif
	   ln2702_set_fault_opt(info, true, 1, 1, 0, 0);
	}


	regmap_update_bits(info->regmap, LN2702_REG_HV_SC_CTRL_0,
			LN2702_MASK_SC_OPERATION_MODE, LN2702_OPMODE_SWITCHING);

	if (info->reverse_power)
	   ln2702_set_fault_opt(info, false, 1, 0, 0, 1);
	else
	   ln2702_set_fault_opt(info, false, 0, 1, 0, 0);
}


/* main function for setting/changing operation mode */
bool ln2702_change_opmode(struct ln2702_info *info, unsigned int target_mode)
{
	bool ret;

	info->op_mode = ln2702_get_opmode(info);

	if (target_mode < 0 || target_mode > LN2702_OPMODE_SWITCHING_ALT) {
	   pr_err("%s: target operation mode (0x%02X) is invalid\n",
		  __func__, target_mode);
	   return false;
	}

	/* NOTE:
	 *      CUSTOMER should know/indicate if power path is forward/reverse mode
	 *      based on power connections before attempting to change operation mode
         */


	ret = true;
	switch(target_mode) {
	  case LN2702_OPMODE_STANDBY:
		ln2702_enter_standby(info);
		break;
	  case LN2702_OPMODE_BYPASS:
		ln2702_enter_bypass(info);
		break;
	  case LN2702_OPMODE_SWITCHING:
	  case LN2702_OPMODE_SWITCHING_ALT:
		ln2702_enter_switching(info);
		break;
	  default:
		ret = false;
	}
	return ret;
}
EXPORT_SYMBOL(ln2702_change_opmode);


int ln2702_hw_init(struct ln2702_info *info)
{
	/* NOTES:
	 *   When power source is removed, LN2702 is powered off and all register settings are lost
	 *   So, LN2702 should be re-initialized every time it is powered on
	 *   This routine should be called when the RX IC or PMIC detects a new power source
	 */

	/* CUSTOMER should add basic initialization tasks
	 * -- unmasking interrupts
	 * -- overriding OTP settings, etc
	 */
	pr_info("%s: HW initialization\n", __func__);


#ifdef LN2702_HW_REV_A1
	regmap_update_bits(info->regmap, LN2702_REG_HV_SC_CTRL_1, 0x10, (1<<4));
	regmap_update_bits(info->regmap, LN2702_REG_TRACK_CTRL, 0x04, (1<<2));
#endif



	info->reverse_power = false;
	return 1;
}
EXPORT_SYMBOL(ln2702_hw_init);


/*********************************************************************************
 * LION-DEBUG: user-space interaction through sysfs (for testing only)
 *********************************************************************************/

static unsigned int ln2702_regcmd_addr;
static unsigned int ln2702_regcmd_data;
static bool         ln2702_regcmd_valid;

static ssize_t ln2702_sysfs_show_regcmd(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{


	if (ln2702_regcmd_valid) {
	   pr_info("%s: /sysfs/ return stored data value (0x%02X)\n",
		__func__, ln2702_regcmd_data & 0xFF);
	   return scnprintf(buf, PAGE_SIZE, "0x%02x\n", ln2702_regcmd_data & 0xFF);
	} else {
	   pr_warn("%s: /sysfs/ regcmd is invalid or was not issued\n", __func__);
	   return scnprintf(buf, PAGE_SIZE, "NA\n");
	}
}

static ssize_t ln2702_sysfs_store_regcmd(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	struct ln2702_info *info = dev_get_drvdata(dev);
	bool write_cmd;
	int ret;

	/* command format:
	 * 	>>read,0x01		: read from 0x01
	 * 	>>write,0x01,0xAA	: write to  0x01
	 */

	ln2702_regcmd_valid = false;
	if      (strncmp("read,",  buf, 5)==0) write_cmd = false;
	else if (strncmp("write,", buf, 6)==0) write_cmd = true;
	else {
	   pr_warn("%s: /sysfs/ invalid command: %s\n", __func__, buf);
	   goto parse_error;
	}

	if (write_cmd) {
	   if (sscanf(buf, "write,%x,%x", &ln2702_regcmd_addr, &ln2702_regcmd_data) != 2)
	      goto parse_error;
	   if (ln2702_regcmd_addr > LN2702_MAX_REGISTER || ln2702_regcmd_data > 0xFF) {
	      pr_warn("%s: /sysfs/ write command addr/data out of bounds\n", __func__);
	      goto parse_error;
	   }

	   pr_info("%s: /sysfs/ writing to addr 0x%02X (data=0x%02X)\n",
			__func__, ln2702_regcmd_addr, ln2702_regcmd_data);
	   ret = regmap_write(info->regmap, ln2702_regcmd_addr, ln2702_regcmd_data);
	} else {
	   if (sscanf(buf, "read,%x", &ln2702_regcmd_addr) != 1)
	      goto parse_error;
	   if (ln2702_regcmd_addr > LN2702_MAX_REGISTER) {
	      pr_warn("%s: /sysfs/ read command addr out of bounds\n", __func__);
	      goto parse_error;
	   }

	   pr_info("%s: /sysfs/ reading from addr 0x%02X\n",
			__func__, ln2702_regcmd_addr);
	   ret = regmap_read(info->regmap, ln2702_regcmd_addr, &ln2702_regcmd_data);
	}
	if (ret < 0)
	   return ret;

	ln2702_regcmd_valid = true;
	return count;

parse_error:
	pr_warn("%s: /sysfs/ unable to parse command (%s)\n", __func__, buf);
	return count;
}


static ssize_t ln2702_sysfs_show_opmode(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct ln2702_info *info = dev_get_drvdata(dev);
	int ret;

	pr_info("%s: /sysfs/ retrieve op_mode\n", __func__);

	info->op_mode = ln2702_get_opmode(info);
	switch(info->op_mode) {
	  case LN2702_OPMODE_STANDBY:
		ret = scnprintf(buf, PAGE_SIZE, "standby\n");
		break;
	  case LN2702_OPMODE_BYPASS:
		ret = scnprintf(buf, PAGE_SIZE, "bypass\n");
		break;
	  case LN2702_OPMODE_SWITCHING:
	  case LN2702_OPMODE_SWITCHING_ALT:
		ret = scnprintf(buf, PAGE_SIZE, "switching\n");
		break;
	  case LN2702_OPMODE_UNKNOWN:
	  default:
		ret = scnprintf(buf, PAGE_SIZE, "unknown\n");
	}
	return ret;
}

static ssize_t ln2702_sysfs_show_dump(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct ln2702_info *info = dev_get_drvdata(dev);
	int val;

	pr_info("\n\ndump: /sysfs/ dump registers\n");

	regmap_read(info->regmap, LN2702_REG_DEVICE_CTRL_0, &val);
	pr_info("dump:      --> 0x0D [DEVICE_CTRL_0]  : 0x%02X\n", val & 0xFF);

	regmap_read(info->regmap, LN2702_REG_HV_SC_CTRL_0, &val);
	pr_info("dump:      --> 0x0F [HV_SC_CTRL_0 ]  : 0x%02X\n", val & 0xFF);

	regmap_read(info->regmap, LN2702_REG_HV_SC_CTRL_1, &val);
	pr_info("dump:      --> 0x10 [HV_SC_CTRL_1 ]  : 0x%02X\n", val & 0xFF);

	regmap_read(info->regmap, LN2702_REG_FAULT_CTRL, &val);
	pr_info("dump:      --> 0x14 [FAULT_CTRL   ]  : 0x%02X\n", val & 0xFF);

	regmap_read(info->regmap, LN2702_REG_TRACK_CTRL, &val);
	pr_info("dump:      --> 0x15 [TRACK_CTRL   ]  : 0x%02X\n", val & 0xFF);

#ifdef LN2702_HW_REV_A1
	regmap_read(info->regmap, LN2702_REG_STS_D, &val);
	pr_info("dump:      --> 0x3A [STS_D        ]  : 0x%02X\n", val & 0xFF);
#elif  LN2702_HW_REV_B0
	regmap_read(info->regmap, LN2702_REG_STS_F, &val);
	pr_info("dump:      --> 0x42 [STS_F        ]  : 0x%02X\n", val & 0xFF);
	regmap_read(info->regmap, LN2702_REG_STS_G, &val);
	pr_info("dump:      --> 0x43 [STS_G        ]  : 0x%02X\n", val & 0xFF);
#endif

	pr_info("dump: /sysfs/ -----------------------------------\n");
	return scnprintf(buf, PAGE_SIZE, "done\n");
}


static ssize_t ln2702_sysfs_store_opmode(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	struct ln2702_info *info = dev_get_drvdata(dev);
	int opmode;

	if      (strncmp("standby", buf, 7)==0)   opmode = LN2702_OPMODE_STANDBY;
	else if (strncmp("bypass", buf, 6)==0)    opmode = LN2702_OPMODE_BYPASS;
	else if (strncmp("switching", buf, 9)==0) opmode = LN2702_OPMODE_SWITCHING;
	else                                      opmode = LN2702_OPMODE_UNKNOWN;

	pr_info("%s: /sysfs/ set op_mode to 0x%02X==%s", __func__, opmode, buf);

	if (!ln2702_change_opmode(info, opmode))
	   pr_warn("%s: /sysfs/ unable to enter %s\n", __func__, buf);

	return count;
}


static ssize_t ln2702_sysfs_store_infet(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	struct ln2702_info *info = dev_get_drvdata(dev);
	unsigned int enable;
	if (strncmp("1", buf, 1)==0) enable = 1;
	else enable = 0;
	ln2702_set_infet(info, enable);
	return count;
}

static ssize_t ln2702_sysfs_store_ext_5v(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	struct ln2702_info *info = dev_get_drvdata(dev);
	unsigned int enable;
	if (strncmp("1", buf, 1)==0) enable = 1;
	else enable = 0;
	ln2702_use_ext_5V(info, enable);
	return count;
}

static ssize_t ln2702_sysfs_store_powerpath(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	struct ln2702_info *info = dev_get_drvdata(dev);
	bool forward;
	if (strncmp("forward", buf, 1)==0) forward = true;
	else forward = false;
	ln2702_set_powerpath(info, forward);
	return count;
}


static DEVICE_ATTR(dump, 0444, ln2702_sysfs_show_dump, NULL);
static DEVICE_ATTR(infet, 0200, NULL, ln2702_sysfs_store_infet);
static DEVICE_ATTR(ext_5v, 0200, NULL, ln2702_sysfs_store_ext_5v);
static DEVICE_ATTR(powerpath, 0200, NULL, ln2702_sysfs_store_powerpath);
static DEVICE_ATTR(opmode, 0644, ln2702_sysfs_show_opmode, ln2702_sysfs_store_opmode);
static DEVICE_ATTR(regcmd, 0644, ln2702_sysfs_show_regcmd, ln2702_sysfs_store_regcmd);
static struct attribute *ln2702_info_attr[] = {
	&dev_attr_dump.attr,
	&dev_attr_regcmd.attr,
	&dev_attr_opmode.attr,
	&dev_attr_infet.attr,
	&dev_attr_ext_5v.attr,
	&dev_attr_powerpath.attr,
	NULL,
};

static const struct attribute_group ln2702_attr_group = {
	.attrs = ln2702_info_attr,
};

/*
static enum power_supply_property cp2702_props[] = {
        POWER_SUPPLY_PROP_PIN_ENABLED,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_WIRELESS_VERSION,
	POWER_SUPPLY_PROP_SIGNAL_STRENGTH,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_PROP_TX_ADAPTER,
};

static const struct power_supply_desc cp_psy_desc = {
        .name = LN2702_DRIVER_NAME,
        .type = POWER_SUPPLY_TYPE_WIRELESS,
        .properties = ln2702_props,
        .num_properties = ARRAY_SIZE(ln2702_props),
        .get_property = ln2702_get_prop,
        .set_property = ln2702_set_prop,
        .property_is_writeable = ln2702_prop_is_writeable,
};
*/

/*********************************************************************************
 * device layer
 *********************************************************************************/
static int ln2702_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ln2702_info *info;

	/* allocate memory for our device state and initialize it */
	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL)
	   return -ENOMEM;

	mutex_init(&info->lock);
	info->client  = client;

	i2c_set_clientdata(client, info);

	info->regmap = devm_regmap_init_i2c(client, &ln2702_regmap_config);
	if (IS_ERR(info->regmap)) {
	   pr_err("%s: failed to initialize regmap\n", __func__);
	   return PTR_ERR(info->regmap);
	}







	/* LION-DEBUG: test user-app through sysfs */
	if (sysfs_create_group(&info->client->dev.kobj, &ln2702_attr_group) < 0) {
	   pr_err("%s: unable to create sysfs entries\n", __func__);
	   return -EINVAL;
	}
/*
        info->cp_psy = power_supply_register(info->client->dev,
                        &cp_psy_desc,
                        &cp_cfg);
*/

	pr_info("[ln2702] %s: success!\n", __func__);

	return 0;
}

static int ln2702_remove(struct i2c_client *client)
{


	pr_info("%s: ==============/REMOVE/==============\n", __func__);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ln2702_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ln2702_info *info = i2c_get_clientdata(client);

	ln2702_change_opmode(info, LN2702_OPMODE_BYPASS);

	pr_info("%s: cancel delayed work\n", __func__);

	return 0;
}

static int ln2702_resume(struct device *dev)
{
/*
	struct i2c_client *client = to_i2c_client(dev);
	struct ln2702_info *info = i2c_get_clientdata(client);
*/

	pr_info("%s: update/resume\n", __func__);

	return 0;
}
#endif
static SIMPLE_DEV_PM_OPS(ln2702_pm_ops, ln2702_suspend, ln2702_resume);

#ifdef CONFIG_OF
static const struct of_device_id ln2702_dt_match[] = {
	{ .compatible = "lionsemi,ln2702" },
	{ },
};
MODULE_DEVICE_TABLE(of, ln2702_dt_match);
#endif

static const struct i2c_device_id ln2702_id[] = {
	{ "ln2702", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ln2702_id);

static struct i2c_driver ln2702_driver = {
	.driver   = {
		.name = LN2702_DRIVER_NAME,
		.of_match_table = of_match_ptr(ln2702_dt_match),
		.pm   = &ln2702_pm_ops,
	},
	.probe    = ln2702_probe,
	.remove   = ln2702_remove,
	.id_table = ln2702_id,
};
module_i2c_driver(ln2702_driver);

MODULE_AUTHOR("Jae Lee");
MODULE_DESCRIPTION("LN2702 driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.2.2");
