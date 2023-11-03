// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019-2021, The Linux Foundation. All rights reserved. */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
//#include <linux/regulator/debug-regulator.h>


#define wl2866d2_err(reg, message, ...) \
	pr_err("%s: " message, (reg)->rdesc.name, ##__VA_ARGS__)
#define wl2866d2_debug(reg, message, ...) \
	pr_err("%s: " message, (reg)->rdesc.name, ##__VA_ARGS__)

#define wl2866d2_vdd_step(type) \
	(type ? 12500 : 6000)
#define wl2866d2_vdd_real(value, type) \
	(type ? (1200000 + value * 12500) : (600000 + value * 6000))
#define wl2866d2_vdd_reg(value, type) \
	(type ? ((value - 1200000) / 12500) : ((value - 600000) / 6000))

#define WL2866D2_MAX_LDO              4
#define WL2866D2_DISCHARGE_REG        0x02
#define WL2866D2_ENABLE_REG           0x0E
#define WL2866D2_ENABLE_DISCHARGE_VAL 0xFF
#define WL2866D2_RETRY_WAIT_TIME      500000
#define WL2866D2_PINCTRL_ENABLE       "wl2866d2_enable"
#define WL2866D2_PINCTRL_DISABLE      "wl2866d2_disable"

#define LDO_DEBUG_SYSNODE_ENABLE 1

#if LDO_DEBUG_SYSNODE_ENABLE
#define WL2866D_MAX_LDO_NUM           4
#define WL2866D_LDOX_ENABLE_ADDR      0x0E
#define WL2866D_LDO1_ADDR             0x03
#define WL2866D_LDOX_ADDR(g_ldo_num)    (WL2866D_LDO1_ADDR + g_ldo_num -1)
#define WL2866D_LDOX_SELECT(g_ldo_num)  (0x01 << (g_ldo_num - 1))
#define IS_VALID_ADDR(g_reg_addr)       ((g_reg_addr > -1) && (g_reg_addr < 0x10))
#define IS_VALID_LDO(g_ldo_num)         ((g_ldo_num < 5) && (g_ldo_num > 0))
#define DVDD_MAX_OUTPUT               1800//mv  ldo1  ldo2
#define DVDD_MIN_OUTPUT               600 //mv  ldo1  ldo2
#define AVDD_MAX_OUTPUT               4300//mv  ldo3--ldo4
#define AVDD_MIN_OUTPUT               1200//mv  ldo3--ldo4
#define VSET_DVDD_STEP_UV             6000
#define VSET_DVDD_BASE_UV             600000//DVDD_MIN_OUTPUT
#define VSET_AVDD_STEP_UV             12500
#define VSET_AVDD_BASE_UV             1200000// AVDD_MIN_OUTPUT

#define UV2REG_CODE(g_ldo_num, vset, vcode)  \
       do{\
           if(g_ldo_num == 1 || g_ldo_num == 2){\
               vcode = DIV_ROUND_UP(vset*1000 - VSET_DVDD_BASE_UV, VSET_DVDD_STEP_UV);\
	   }else{\
               vcode = DIV_ROUND_UP(vset*1000 - VSET_AVDD_BASE_UV, VSET_AVDD_STEP_UV);\
	   }\
       }while(0)

#define REGCODE2_UV(g_ldo_num, vcode, g_vol_val)  \
       do{\
           if(g_ldo_num == 1 || g_ldo_num == 2){\
 	       g_vol_val = VSET_DVDD_BASE_UV + vcode * VSET_DVDD_STEP_UV;\
	   }else{\
 	       g_vol_val = VSET_AVDD_BASE_UV + vcode * VSET_AVDD_STEP_UV;\
	   }\
       }while(0)

#define CHECK_VOL_SET(g_ldo_num, vset)         \
       do{\
           if(g_ldo_num == 1 || g_ldo_num == 2){\
 	       if(vset < DVDD_MIN_OUTPUT)\
 	           vset = DVDD_MIN_OUTPUT;\
 	       else if(vset > DVDD_MAX_OUTPUT)\
 	           vset = DVDD_MAX_OUTPUT;\
	   }else{\
 	       if(vset < AVDD_MIN_OUTPUT)\
 	           vset = AVDD_MIN_OUTPUT;\
 	       else if(vset > AVDD_MAX_OUTPUT)\
 	           vset = AVDD_MAX_OUTPUT;\
	   }\
       }while(0)

#define STA2STR(g_ldo_num, g_reg_val) ((g_reg_val & WL2866D_LDOX_SELECT(g_ldo_num)) ? "Enabled*" : "Disabled")

#define SET_DISABLE_BIT(g_reg_val, g_ldo_num)  (g_reg_val & (~WL2866D_LDOX_SELECT(g_ldo_num)))

#define SET_ENABLE_BIT(g_reg_val, g_ldo_num)   (g_reg_val | WL2866D_LDOX_SELECT(g_ldo_num))

#define IS_LDO_ENABLE(g_reg_val, g_ldo_num)    (g_reg_val & (0x01 << (g_ldo_num - 1)))

static char g_reg_val;
static int  g_vol_val;
static int g_ldo_num_endisable;
static int g_ldo_num_getset_vol;
static int g_reg_addr;
static struct i2c_client *i2cclient;
int pmic2_dump_flag = 0;
EXPORT_SYMBOL(pmic2_dump_flag);
#endif

enum wl2866d2_vdd_type {
	VDD_TYPE_DVDD,
	VDD_TYPE_AVDD,
	VDD_TYPE_MAX,
};

enum wl2866d2_vdd_index {
	VDD_INDEX_DVDD1,
	VDD_INDEX_DVDD2,
	VDD_INDEX_AVDD1,
	VDD_INDEX_AVDD2,
	VDD_INDEX_MAX,
};

struct wl2866d2_regulator {
	u8 addr;
	int   uv;
	bool *suspended;
	bool  reg_enabled;
	struct device *dev;
	struct regmap *regmap;
	struct device_node    *of_node;
	struct regulator_dev  *rdev;
	struct regulator_desc  rdesc;
	enum wl2866d2_vdd_type  type;
	enum wl2866d2_vdd_index index;
};

struct wl2866d2_pmic {
	bool suspended;
	struct device  *dev;
	struct regmap  *regmap;
	struct pinctrl *pinctrl;
	struct device_node   *of_node;
	struct pinctrl_state *gpio_state_enable;
	struct pinctrl_state *gpio_state_disable;
};

static struct regmap_config wl2866d2_regulator_regmap_config = {
	.reg_bits     = 8,
	.val_bits     = 8,
	.max_register = 0xff,
};

struct regulator_data {
	char *name;
	int   min_uv;
	int   max_uv;
	int   step_uv;
};

static const struct regulator_data wl2866d2_reg_data[WL2866D2_MAX_LDO] = {
	{ "dvdd1",  600000, 1800000,  6000 },
	{ "dvdd2",  600000, 1800000,  6000 },
	{ "avdd1", 1200000, 4300000, 12500 },
	{ "avdd2", 1200000, 4300000, 12500 },
};

static int wl2866d2_read(struct regmap *regmap,  u8 reg, u8 *val, int count)
{
	int rc;

	pr_debug("wl2866d2 read: addr-0x%02x, count-%d\n", reg, count);

	rc = regmap_bulk_read(regmap, reg, val, count);
	if (rc < 0) {
		pr_err("wl2866d2 read: failed to read 0x%02x\n", reg);
		usleep_range(WL2866D2_RETRY_WAIT_TIME,
			     WL2866D2_RETRY_WAIT_TIME + 100);

		rc = regmap_bulk_read(regmap, reg, val, count);
		if (rc < 0)
			pr_err("wl2866d2 read: failed to read 0x%02x again\n", reg);
		else {
			pr_err("wl2866d2 read: success to read 0x%02x again, "
			       "crash for debug\n", reg);
			rc = -EINVAL;
		}
	}

	return rc;
}

static int wl2866d2_write(struct regmap *regmap, u8 reg, const u8 *val,
			 int count)
{
	int rc;

	pr_debug("wl2866d2 write: addr-0x%02x, data-0x%02x\n", reg, *val);

	rc = regmap_bulk_write(regmap, reg, val, count);
	if (rc < 0) {
		pr_err("wl2866d2 write: failed to write 0x%02x\n", reg);
		usleep_range(WL2866D2_RETRY_WAIT_TIME,
			     WL2866D2_RETRY_WAIT_TIME + 100);

		rc = regmap_bulk_write(regmap, reg, val, count);
		if (rc < 0)
			pr_err("wl2866d2 write: failed to write 0x%02x again\n", reg);
		else {
			pr_err("wl2866d2 write: success to write 0x%02x again, "
			       "crash for debug\n", reg);
			rc = -EINVAL;
		}
	}

	return rc;
}

static int wl2866d2_masked_write(struct regmap *regmap, u8 reg, u8 mask,
				u8 val)
{
	int rc;

	pr_debug("wl2866d2 masked write: addr-0x%02x, mask-0x%02x, "
			 "masked_data-0x%02x\n", reg, mask, mask & val);

	rc = regmap_update_bits(regmap, reg, mask, val);
	if (rc < 0) {
		pr_err("wl2866d2 masked write: failed to write 0x%02x to "
			   "0x%02x with mask 0x%02x\n", val, reg, mask);
		usleep_range(WL2866D2_RETRY_WAIT_TIME,
			     WL2866D2_RETRY_WAIT_TIME + 100);

		rc = regmap_update_bits(regmap, reg, mask, val);
		if (rc < 0)
			pr_err("wl2866d2 masked write: failed to write 0x%02x to "
			       "0x%02x with mask 0x%02x\n", val, reg, mask);
		else
			pr_err("wl2866d2 masked write: failed to write 0x%02x to "
			       "0x%02x with mask 0x%02x, "
			       "crash for debug\n", val, reg, mask);
			rc = -EINVAL;
	}

	return rc;
}

static int wl2866d2_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct wl2866d2_regulator *wl2866d2_reg = rdev_get_drvdata(rdev);
	int rc, voltage_uv = 0;
	u8  reg_voltage;

	if (*wl2866d2_reg->suspended)
		return wl2866d2_reg->uv;

	rc = wl2866d2_read(wl2866d2_reg->regmap, wl2866d2_reg->addr,
			  &reg_voltage, 1);
	if (rc < 0) {
		wl2866d2_err(wl2866d2_reg,
			    "failed to read regulator voltage rc = %d\n", rc);
		return rc;
	}

	voltage_uv = wl2866d2_vdd_real(reg_voltage, wl2866d2_reg->type);

	wl2866d2_debug(wl2866d2_reg, "voltage read: 0x%x -> %duV\n",
		      reg_voltage, voltage_uv);
	return voltage_uv;
}

static int wl2866d2_regulator_enable(struct regulator_dev *rdev)
{
	struct wl2866d2_regulator *wl2866d2_reg = rdev_get_drvdata(rdev);
	int rc, current_uv;

	if (*wl2866d2_reg->suspended) {
		if (wl2866d2_reg->reg_enabled)
			return 0;
		return -EPERM;
	}

	current_uv = wl2866d2_regulator_get_voltage(rdev);
	if (current_uv < 0) {
		wl2866d2_err(wl2866d2_reg, "failed to get current voltage rc = %d\n",
			    current_uv);
		return current_uv;
	}

	rc = wl2866d2_masked_write(wl2866d2_reg->regmap, WL2866D2_ENABLE_REG,
				  1 << wl2866d2_reg->index,
				  1 << wl2866d2_reg->index);
	if (rc < 0) {
		wl2866d2_err(wl2866d2_reg, "failed to enable regulator rc = %d\n",
			    rc);
		return rc;
	}

	wl2866d2_reg->reg_enabled = true;
	wl2866d2_debug(wl2866d2_reg, "regulator enabled\n");
        pmic2_dump_flag++;
	return rc;
}

static int wl2866d2_regulator_disable(struct regulator_dev *rdev)
{
	struct wl2866d2_regulator *wl2866d2_reg = rdev_get_drvdata(rdev);
	int rc;

	if (*wl2866d2_reg->suspended) {
		if (!wl2866d2_reg->reg_enabled)
			return 0;
		return -EPERM;
	}

	rc = wl2866d2_masked_write(wl2866d2_reg->regmap, WL2866D2_ENABLE_REG,
				  1 << wl2866d2_reg->index, 0);

	if (rc < 0) {
		wl2866d2_err(wl2866d2_reg, "failed to disable regulator rc = %d\n",
			    rc);
		return rc;
	}

	wl2866d2_reg->reg_enabled = false;
	wl2866d2_debug(wl2866d2_reg, "regulator disabled\n");
        pmic2_dump_flag--;
	return rc;
}

static int wl2866d2_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct wl2866d2_regulator *wl2866d2_reg = rdev_get_drvdata(rdev);
	u8 en_value;
	int rc;

	if (*wl2866d2_reg->suspended)
		return wl2866d2_reg->reg_enabled;

	rc = wl2866d2_read(wl2866d2_reg->regmap, WL2866D2_ENABLE_REG, &en_value, 1);
	if (rc < 0) {
		wl2866d2_err(wl2866d2_reg, "failed to read enable reg rc = %d\n", rc);
		return rc;
	}
	wl2866d2_debug(wl2866d2_reg, "addr:0x%x  0x%x  is enabled = %d\n", WL2866D2_ENABLE_REG, en_value, !!(en_value & 1 << wl2866d2_reg->index));

	return !!(en_value & 1 << wl2866d2_reg->index);
}

static int wl2866d2_write_voltage(struct wl2866d2_regulator *wl2866d2_reg,
				 int min_uv, int max_uv)
{
	int rc = 0, voltage;
	u8  reg_voltage;

	voltage = (DIV_ROUND_UP(min_uv, wl2866d2_vdd_step(wl2866d2_reg->type))
		   * wl2866d2_vdd_step(wl2866d2_reg->type));
	if (voltage > max_uv) {
		wl2866d2_err(wl2866d2_reg, "requested voltage above maximum limit\n");
		return -EINVAL;
	}

	reg_voltage = wl2866d2_vdd_reg(voltage, wl2866d2_reg->type);

	rc = wl2866d2_write(wl2866d2_reg->regmap, wl2866d2_reg->addr, &reg_voltage, 1);
	if (rc < 0) {
		wl2866d2_err(wl2866d2_reg, "failed to write voltage rc = %d\n", rc);
		return rc;
	}

	wl2866d2_reg->uv = voltage;
	wl2866d2_debug(wl2866d2_reg, "write register voltage: 0x%x\n", reg_voltage);
	return 0;
}

static int wl2866d2_regulator_set_voltage(struct regulator_dev *rdev,
					 int min_uv, int max_uv,
					 unsigned int *selector)
{
	struct wl2866d2_regulator *wl2866d2_reg = rdev_get_drvdata(rdev);
	int rc;

	if (*wl2866d2_reg->suspended) {
		if (min_uv <= wl2866d2_reg->uv && wl2866d2_reg->uv <= max_uv)
			return 0;
		return -EPERM;
	}

	rc = wl2866d2_write_voltage(wl2866d2_reg, min_uv, max_uv);
	if (rc < 0)
		return rc;

	wl2866d2_debug(wl2866d2_reg, "voltage set to %d\n", min_uv);
	return rc;
}

static const struct regulator_ops wl2866d2_regulator_ops = {
	.enable      = wl2866d2_regulator_enable,
	.disable     = wl2866d2_regulator_disable,
	.is_enabled  = wl2866d2_regulator_is_enabled,
	.set_voltage = wl2866d2_regulator_set_voltage,
	.get_voltage = wl2866d2_regulator_get_voltage,
};

#if LDO_DEBUG_SYSNODE_ENABLE
static ssize_t wl2866d2_DebugOps_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"===================== PMU wl2866d2 LDO debug node operation instructions =====================\n"
"  Note: lx/Lx         x in range : 1 -- 4\n"
"        l/h or L/H    set low/high current\n"
"        0/1           disable/enable\n"
"        r/w or R/W    read/write\n"
"1, cat wl2866d2_ShowAllLdoState                -> you will get all ldo state\n"
"2, cat wl2866d2_GetVolRange                    -> you will get all the ldo voltage range\n"
"3, echo w lx 0/1 > wl2866d2_LDOEnDisable       -> you will set lx enable or disable\n"
"4, echo r lx > wl2866d2_LDOEnDisable\n"
"   cat wl2866d2_LDOEnDisable                   -> you will get lx is enable or disable\n"
"5, echo w lx xxx > wl2866d2_LDOSetGetVol       -> you will set lx output xxx mV voltage\n"
"6, echo r lx > wl2866d2_LDOSetGetVol\n"
"   cat wl2866d2_LDOSetGetVol                   -> you will get the lx voltage\n"
"7, echo w 0xaa 0xbb > wl2866d2_ReadWriteReg    -> you will write 0xbb to addr 0xaa\n"
"8, echo r 0xaa > wl2866d2_ReadWriteReg\n"
"   cat wl2866d2_ReadWriteReg                   -> you will get addr 0xaa value\n"
"==========================================================================================\n");
}
static DEVICE_ATTR(wl2866d2_OpsInstructions, 0644, wl2866d2_DebugOps_show, NULL);

static ssize_t wl2866d2_allldostate_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	char en_state, vcode[WL2866D_MAX_LDO_NUM], ldo;
	int vol_val[WL2866D_MAX_LDO_NUM];
	en_state = i2c_smbus_read_byte_data(client, WL2866D_LDOX_ENABLE_ADDR);//get ldo state
	for(ldo = 1; ldo < 5; ldo++){
            vcode[ldo - 1] = i2c_smbus_read_byte_data(client, WL2866D_LDOX_ADDR(ldo));
            REGCODE2_UV(ldo, vcode[ldo - 1], vol_val[ldo - 1]);
	}
	return sprintf(buf,"========= Show All LDO State =========\n"
"   Lx     name    Vset(mV)         State\n"
"   l1    dvdd1      %4d          %s\n"
"   l2    dvdd2      %4d          %s\n"
"   l3    avdd1      %4d          %s\n"
"   l4    avdd2      %4d          %s\n"
"======================================\n"
, vol_val[0] / 1000, STA2STR(1, en_state)
, vol_val[1] / 1000, STA2STR(2, en_state)
, vol_val[2] / 1000, STA2STR(3, en_state)
, vol_val[3] / 1000, STA2STR(4, en_state));
}
static DEVICE_ATTR(wl2866d2_ShowAllLdoState, 0644, wl2866d2_allldostate_show,NULL);

static ssize_t wl2866d2_GetVolRange_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"LDO_1 -- LDO_2 V_Range:  %dmV ~ %dmV\nLDO_3 -- LDO_4 V_Range: %dmV ~ %dmV\n", DVDD_MIN_OUTPUT, DVDD_MAX_OUTPUT, AVDD_MIN_OUTPUT, AVDD_MAX_OUTPUT);
}
static DEVICE_ATTR(wl2866d2_GetVolRange, 0644, wl2866d2_GetVolRange_show,NULL);

static ssize_t wl2866d2_LDOSetGetVol_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 vcode;
	if(IS_VALID_LDO(g_ldo_num_getset_vol)){
            vcode = i2c_smbus_read_byte_data(client, WL2866D_LDOX_ADDR(g_ldo_num_getset_vol));
            g_reg_val = i2c_smbus_read_byte_data(client, WL2866D_LDOX_ENABLE_ADDR);//get ldo state
            if(IS_LDO_ENABLE(g_reg_val, g_ldo_num_getset_vol))
                REGCODE2_UV(g_ldo_num_getset_vol, vcode, g_vol_val);
	    else
                g_vol_val = 0;
	    return sprintf(buf,"Got LDO_%d: voltage:%dmV\n", g_ldo_num_getset_vol, g_vol_val / 1000);
	}else{
	    return sprintf(buf,"Invalid ldo Please indicate which ldo to get vol first !\n");
	}
}

static ssize_t wl2866d2_LDOSetGetVol_store(struct device *dev,
        struct device_attribute *attr, const char *buf,
        size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
        int ret, vset;
	char cmd, ldo_str[5], lab;
	u8 vcode;
        sscanf(buf, "%c %3s %d", &cmd, ldo_str, &vset);
        sscanf(ldo_str, "%c %d", &lab, &g_ldo_num_getset_vol);
	pr_err("%s: %c l%d %d\n", __func__, cmd, g_ldo_num_getset_vol, vset);
        if((cmd == 'W' || cmd == 'w') && IS_VALID_LDO(g_ldo_num_getset_vol)){
                CHECK_VOL_SET(g_ldo_num_getset_vol, vset);
                UV2REG_CODE(g_ldo_num_getset_vol, vset, vcode);
		g_reg_val = i2c_smbus_read_byte_data(client, WL2866D_LDOX_ENABLE_ADDR);//get ldo state
 	 	ret = i2c_smbus_write_byte_data(client, WL2866D_LDOX_ENABLE_ADDR, SET_DISABLE_BIT(g_reg_val, g_ldo_num_getset_vol));//disable ldox
                ret = i2c_smbus_write_byte_data(client, WL2866D_LDOX_ADDR(g_ldo_num_getset_vol), vcode);//set vol
 	 	ret = i2c_smbus_write_byte_data(client, WL2866D_LDOX_ENABLE_ADDR, SET_ENABLE_BIT(g_reg_val, g_ldo_num_getset_vol));//enable ldox
                pr_err("%s: l%d set:%d 0x%x\n",__func__, g_ldo_num_getset_vol, vset, vcode);
        }

	return count;
}
static DEVICE_ATTR(wl2866d2_LDOSetGetVol, 0644, wl2866d2_LDOSetGetVol_show, wl2866d2_LDOSetGetVol_store);

static ssize_t wl2866d2reg_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	if(IS_VALID_ADDR(g_reg_addr)){
	    g_reg_val = i2c_smbus_read_byte_data(client, g_reg_addr);
	    if(g_reg_val < 0)
	        pr_err("%s: read %x failed\n",__func__, g_reg_addr);
	    else
	        pr_err("%s: read data: %x \n", __func__, g_reg_val);
	    return sprintf(buf,"Read Reg 0X%x: Value:0X%x\n", g_reg_addr, g_reg_val);
	}else{
	    return sprintf(buf,"Invalid addr Please indicate which reg addr to get first !\n");
	}
}

static ssize_t wl2866d2reg_store(struct device *dev,
        struct device_attribute *attr, const char *buf,
        size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	int val, ret;
	char cmd;
        sscanf(buf, "%c %d %d", &cmd, &g_reg_addr, &val);
	pr_err("%s: %c %x %x\n",__func__, cmd, g_reg_addr, val);
	if((cmd == 'W' || cmd == 'w') && IS_VALID_ADDR(g_reg_addr)){
	    pr_err("%s: to write 0x%x 0x%x \n",__func__, g_reg_addr, val);
	    ret = i2c_smbus_write_byte_data(client, g_reg_addr, val & 0xFf);
	    if(ret)
	        pr_err("%s: write 0x%x 0x%x failed\n",__func__, g_reg_addr, val);
        }

	return count;
}
static DEVICE_ATTR(wl2866d2_ReadWriteReg, 0644, wl2866d2reg_show, wl2866d2reg_store);

static ssize_t wl2866d2_LDOEnDisable_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	if(IS_VALID_LDO(g_ldo_num_endisable)){
 	    g_reg_val = i2c_smbus_read_byte_data(client, WL2866D_LDOX_ENABLE_ADDR);
	    return sprintf(buf,"LDO status:0X%x   LDO_%d is %s\n", g_reg_val, g_ldo_num_endisable, STA2STR(g_ldo_num_endisable, g_reg_val));
	}else{
	    return sprintf(buf,"Invalid ldo Please indicate which ldo to get first !\n");
	}
}

static ssize_t wl2866d2_LDOEnDisable_store(struct device *dev,
        struct device_attribute *attr, const char *buf,
        size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
        int ret;
	int enable;
	char cmd, ldo_str[5], lab;
        sscanf(buf, "%c %3s %d", &cmd, ldo_str, &enable);
        sscanf(ldo_str, "%c %d", &lab, &g_ldo_num_endisable);
	pr_err("%s: %c l%d %d\n",__func__, cmd, g_ldo_num_endisable, enable);

	if((cmd == 'W' || cmd == 'w') && IS_VALID_LDO(g_ldo_num_endisable)){
	        g_reg_val = i2c_smbus_read_byte_data(client, WL2866D_LDOX_ENABLE_ADDR);
                if(enable)
 	           ret = i2c_smbus_write_byte_data(client, WL2866D_LDOX_ENABLE_ADDR, SET_ENABLE_BIT(g_reg_val, g_ldo_num_endisable));
                else
 	           ret = i2c_smbus_write_byte_data(client, WL2866D_LDOX_ENABLE_ADDR, SET_DISABLE_BIT(g_reg_val, g_ldo_num_endisable));
		pr_err("%x %x\n", g_reg_val, WL2866D_LDOX_SELECT(g_ldo_num_endisable));
	}
   
	return count;
}
static DEVICE_ATTR(wl2866d2_LDOEnDisable, 0644, wl2866d2_LDOEnDisable_show, wl2866d2_LDOEnDisable_store);

static struct attribute *wl2866d2_debug_node[] = {
    &dev_attr_wl2866d2_LDOSetGetVol.attr,
    &dev_attr_wl2866d2_ReadWriteReg.attr,
    &dev_attr_wl2866d2_LDOEnDisable.attr,
    &dev_attr_wl2866d2_GetVolRange.attr,
    &dev_attr_wl2866d2_OpsInstructions.attr,
    &dev_attr_wl2866d2_ShowAllLdoState.attr,
    NULL
};
static const struct attribute_group wl2866d2_debug_attribute_group = {
    .attrs = wl2866d2_debug_node,
};

void wl2866d2_dump(void){
	char en_state, vcode[WL2866D_MAX_LDO_NUM], ldo, i;
	int vol_val[WL2866D_MAX_LDO_NUM];
	en_state = i2c_smbus_read_byte_data(i2cclient, WL2866D_LDOX_ENABLE_ADDR);//get ldo state
	for(ldo = 1; ldo < 5; ldo++){
            vcode[ldo - 1] = i2c_smbus_read_byte_data(i2cclient, WL2866D_LDOX_ADDR(ldo));
            REGCODE2_UV(ldo, vcode[ldo - 1], vol_val[ldo - 1]);
	}
	pr_err("\n=============== Dump   WL2866D2   PMU    State ================\n"
"   Lx       Vset(mV)       EnState\n"
"   l1         %d         %s       \n"
"   l2         %d         %s       \n"
"   l3         %d         %s       \n"
"   l4         %d         %s       \n"
, vol_val[0] / 1000, STA2STR(1, en_state)
, vol_val[1] / 1000, STA2STR(2, en_state)
, vol_val[2] / 1000, STA2STR(3, en_state)
, vol_val[3] / 1000, STA2STR(4, en_state));
        for(i = 0x00; i < 0xF; i++){
            pr_err("addr 0x%x: data:0x%x\n", i, i2c_smbus_read_byte_data(i2cclient, i));
        }
	pr_err("\n===================================================\n");
}
EXPORT_SYMBOL(wl2866d2_dump);
#endif

static int wl2866d2_register_ldo(struct wl2866d2_regulator *wl2866d2_reg,
				const char *name)
{
	const struct regulator_data *reg_data;
	struct device_node *of_node = wl2866d2_reg->of_node;
	struct regulator_init_data *init_data;
	struct regulator_config reg_config = {};
	struct device *dev = wl2866d2_reg->dev;
	const char *vdd_type;
	int rc, i;

	reg_data = wl2866d2_reg_data;

	for (i = 0; i < WL2866D2_MAX_LDO; i++)
		if (strstr(name, reg_data[i].name))
			break;
	if (i == WL2866D2_MAX_LDO) {
		pr_err("wl2866d2_pmic: invalid regulator name %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u32(of_node, "cell-index", &wl2866d2_reg->index);
	if (rc < 0) {
		wl2866d2_err(wl2866d2_reg, "failed to get regulator index rc = %d\n",
			    rc);
		return rc;
	}

	rc = of_property_read_u8(of_node, "reg", &wl2866d2_reg->addr);
	if (rc < 0) {
		wl2866d2_err(wl2866d2_reg, "failed to get regulator register rc = %d\n",
			    rc);
		return rc;
	}

	rc = of_property_read_string(of_node, "type", &vdd_type);
	if (rc < 0) {
		wl2866d2_err(wl2866d2_reg, "failed to get regulator type rc = %d\n",
			    rc);
		return rc;
	}

	if (!strcmp(vdd_type, "dvdd"))
		wl2866d2_reg->type = VDD_TYPE_DVDD;
	else if (!strcmp(vdd_type, "avdd"))
		wl2866d2_reg->type = VDD_TYPE_AVDD;
	else {
		wl2866d2_err(wl2866d2_reg, "invalid regulator type %s\n",
			    wl2866d2_reg->type);
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(dev, of_node, &wl2866d2_reg->rdesc);
	if (init_data == NULL) {
		wl2866d2_err(wl2866d2_reg, "failed to get regulator data\n");
		return -ENODATA;
	}
	if (!init_data->constraints.name) {
		wl2866d2_err(wl2866d2_reg, "regulator name missing\n");
		return -EINVAL;
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
						 | REGULATOR_CHANGE_VOLTAGE;
	reg_config.dev = dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = wl2866d2_reg;
	reg_config.of_node = of_node;

	wl2866d2_reg->reg_enabled = false;
	wl2866d2_reg->uv = reg_data[i].min_uv;

	wl2866d2_reg->rdesc.type = REGULATOR_VOLTAGE;
	wl2866d2_reg->rdesc.ops = &wl2866d2_regulator_ops;
	wl2866d2_reg->rdesc.name = init_data->constraints.name;
	wl2866d2_reg->rdesc.uV_step = reg_data[i].step_uv;
	wl2866d2_reg->rdesc.min_uV = reg_data[i].min_uv;
	wl2866d2_reg->rdesc.n_voltages
		= ((reg_data[i].max_uv - reg_data[i].min_uv)
			/ wl2866d2_reg->rdesc.uV_step) + 1;

	wl2866d2_reg->rdev = devm_regulator_register(dev, &wl2866d2_reg->rdesc,
						    &reg_config);
	if (IS_ERR(wl2866d2_reg->rdev)) {
		rc = PTR_ERR(wl2866d2_reg->rdev);
		wl2866d2_err(wl2866d2_reg, "failed to register regulator rc = %d\n",
			    rc);
		return rc;
	}

/*	rc = devm_regulator_debug_register(dev, wl2866d2_reg->rdev);
	if (rc)
		wl2866d2_err(wl2866d2_reg, "failed to register regulator rc = %d\n",
			    rc);
*/
	wl2866d2_debug(wl2866d2_reg, "regulator registered\n");
	return 0;
}

static int wl2866d2_pmic_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct wl2866d2_pmic *chip;
	struct wl2866d2_regulator *wl2866d2_reg;
	struct device_node *child = NULL;
	const char *name;
	int rc = 0;
	u8  reg_val;
	u8 init_val = 0;
	int i;

	pr_err("wl2866d2_pmic: start probe\n");

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &client->dev;
	chip->regmap = devm_regmap_init_i2c(client, &wl2866d2_regulator_regmap_config);
	if (!chip->regmap)
		return -ENODEV;

	i2c_set_clientdata(client, chip);

	for(i = 0; i < 0xF ; i++)
		rc = wl2866d2_write(chip->regmap, i, &init_val, 1);

	for_each_available_child_of_node(chip->dev->of_node, child) {
		wl2866d2_reg = devm_kzalloc(chip->dev, sizeof(*wl2866d2_reg), GFP_KERNEL);
		if (!wl2866d2_reg)
			return -ENOMEM;

		wl2866d2_reg->dev       = chip->dev;
		wl2866d2_reg->regmap    = chip->regmap;
		wl2866d2_reg->of_node   = child;
		wl2866d2_reg->suspended = &(chip->suspended);

		rc = of_property_read_string(child, "regulator-name", &name);
		if (rc < 0) {
			wl2866d2_err(wl2866d2_reg, "failed to read register name rc = %d\n",
				    rc);
			return rc;
		}

		rc = wl2866d2_register_ldo(wl2866d2_reg, name);
		if (rc < 0) {
			wl2866d2_err(wl2866d2_reg, "failed to register regulator rc = %d\n",
				    rc);
			return rc;
		}
	}

	/* enable regulator output discharge func but none fatal */
	reg_val = WL2866D2_ENABLE_DISCHARGE_VAL;
	rc = wl2866d2_write(chip->regmap, WL2866D2_DISCHARGE_REG, &reg_val, 1);
	if (rc < 0) {
		pr_err("wl2866d2: failed to enable discharge rc = %d\n", rc);
		rc = 0;
	}

	dev_set_drvdata(&client->dev, chip);

#if LDO_DEBUG_SYSNODE_ENABLE
       rc = sysfs_create_group(&client->dev.kobj, &wl2866d2_debug_attribute_group);
       if(rc)
	       pr_err("wl2866d2 debug sys node creat failed\n");
       else
	       pr_err("wl2866d2 debug sys node creat success\n");
       i2cclient = client;
#endif

	pr_debug("wl2866d2 pimc probe successful\n");
	return rc;
}

static int wl2866d2_pmic_remove(struct i2c_client *client)
{
	i2c_set_clientdata(client, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int wl2866d2_pmic_suspend(struct device *dev)
{
	struct wl2866d2_pmic *chip = dev_get_drvdata(dev);

	chip->suspended = true;

	return 0;
}

static int wl2866d2_pmic_resume(struct device *dev)
{
	struct wl2866d2_pmic *chip = dev_get_drvdata(dev);

	chip->suspended = false;

	return 0;
}
#else
static int wl2866d2_pmic_suspend(struct device *dev)
{
	return 0;
}

static int wl2866d2_pmic_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops wl2866d2_pmic_pm_ops = {
	.suspend = wl2866d2_pmic_suspend,
	.resume  = wl2866d2_pmic_resume,
};

static const struct of_device_id wl2866d2_pmic_match_table[] = {
	{ .compatible = "xiaomi,wl2866d2-pmic2"},
	{ },
};

static const struct i2c_device_id wl2866d2_pmic_id[] = {
	{ "wl2866d2-pmic", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, wl2866d2_pmic_id);

static struct i2c_driver wl2866d2_pmic_driver = {
	.driver = {
		.name = "wl2866d2_pmic",
		.pm   = &wl2866d2_pmic_pm_ops,
		.of_match_table = wl2866d2_pmic_match_table,
	},
	.probe    = wl2866d2_pmic_probe,
	.remove   = wl2866d2_pmic_remove,
	.id_table = wl2866d2_pmic_id,
};

module_i2c_driver(wl2866d2_pmic_driver);

MODULE_DESCRIPTION("Xiaomi WL2866D2 PMIC Driver");
MODULE_LICENSE("GPL v2");
