// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/dwc3-msm.h>
#include <linux/usb/repeater.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "../../misc/hwid/hwid.h"

/* Must match PHY_CABLE_CONNECTED definition in phy-msm-snps-eusb2.c */
#define PHY_CABLE_CONNECTED          BIT(16)

#define EUSB2_3P0_VOL_MIN			3075000 /* uV */
#define EUSB2_3P0_VOL_MAX			3300000 /* uV */
#define EUSB2_3P0_HPM_LOAD			3500	/* uA */

#define EUSB2_1P8_VOL_MIN			1800000 /* uV */
#define EUSB2_1P8_VOL_MAX			1800000 /* uV */
#define EUSB2_1P8_HPM_LOAD			32000	/* uA */

/* Repeater REVID registers */
#define EUSB2_REVISION1			0x00

/* eUSB2 status registers */
#define EUSB2_RPTR_STATUS		0x08
#define	RPTR_OK				BIT(7)

#define EUSB2_RPTR_INFRA_STATUS		0x09
#define EUSB2_RPTR_DROPOUT_REASON	0x0A

/* eUSB2 control registers */
#define EUSB2_MODE_CTL			0x40
#define HOST_SEL			BIT(1)
#define PERIPH_SEL			BIT(0)

#define EUSB2_USB_HIZ_CTL		0x41
#define HIZ_EN				BIT(7)

#define EUSB2_AUTO_RESUME_EN		0x42
#define AUTO_RESUME_EN			BIT(7)

#define EUSB2_EN_CTL1			0x46
#define EUSB2_RPTR_EN			BIT(7)

/* eUSB2 tuning parameters registers */
#define EUSB2_TUNE_USB2_CROSSOVER	0x50
#define TUNE_USB2_CROSSOVER_MASK	0x07

#define EUSB2_TUNE_IUSB2		0x51
#define TUNE_IUSB2_MASK			0x0F

#define EUSB2_TUNE_RES_FSDIF		0x52
#define TUNE_RES_FSIDF_MASK		0x07

#define EUSB2_TUNE_HSDISC		0x53
#define TUNE_HSDISC_MASK		0x07

#define EUSB2_TUNE_SQUELCH_U		0x54
#define TUNE_SQELCH_U_MASK		0x07

#define EUSB2_TUNE_USB2_SLEW		0x55
#define TUNE_USB2_SLEW_MASK		0x03

#define EUSB2_TUNE_USB2_EQU		0x56
#define TUNE_USB2_EQU_MASK		0x03

#define EUSB2_TUNE_USB2_PREEM		0x57
#define TUNE_USB2_PREEM_MASK		0x07

#define EUSB2_TUNE_USB2_HS_COMP_CUR	0x58
#define TUNE_USB2_HS_COMP_CURRENT_MASK	0x03

#define EUSB2_TUNE_EUSB_SLEW		0x59
#define TUNE_EUSB_SLEW_MASK		0x03

#define EUSB2_TUNE_EUSB_EQU		0x5A
#define TUNE_EUSB_EQU_MASK		0x03

#define EUSB2_TUNE_EUSB_HS_COMP_CUR	0x5B
#define TUNE_EUSB_HS_COMP_CUR_MASK	0x03

/* Force Enable registers */
#define EUSB2_FORCE_EN_5		0xE8
#define F_CLK_19P2M_EN			BIT(6)
#define F_CLK_19P2M_EN_SHIFT		6

#define EUSB2_FORCE_VAL_5		0xED
#define V_CLK_19P2M_EN			BIT(6)
#define V_CLK_19P2M_EN_SHIFT		6

#define REPEATER_SYSFS_CLASS_NAME "usb_enhance"

struct eusb2_repeater {
	struct usb_repeater	ur;
	struct regmap		*regmap;
	u16			reg_base;
	struct regulator	*vdd18;
	struct regulator	*vdd3;
	bool			power_enabled;

	struct dentry		*root;
	struct delayed_work dwork;
	struct device		*sysfs_dev;

	u8			usb_enhance_support_mode; //can_use
	u8			usb_enhance_state; //ready_use
	u8			usb_enhance_state_delayed;
	u8			current_eye_tune; //now_use

	u8			usb2_crossover;
	u8			iusb2;
	u8			res_fsdif;
	u8			hsdisc;
	u8			squelch_u;
	u8			usb2_slew;
	u8			usb2_equ;
	u8			usb2_preem;
	u8			hs_comp_current;
	u8			eusb_slew;
	u8			eusb_equ;
	u8			eusb_hs_comp_current;

	u32			*param_override_seq;
	u8			param_override_seq_cnt;
	u32			*host_param_override_seq;
	u8			host_param_override_seq_cnt;
	u32			*host_param_override_seq_storage;
	u8			host_param_override_seq_cnt_storage;
	u32			*param_override_seq_carwith;
	u8			param_override_seq_cnt_carwith;
};

typedef enum {
	TYPE_HOST_NORMAL = 0,
	TYPE_HOST_STORAGE = 1,
	TYPE_DEVICE_NORMAL = 0,
	TYPE_DEVICE_CARWITH = 2,
}repeater_type;

enum eusb2_repeater_usb_state {
	USB_STATE_NONE = 0,
	USB_STATE_HOST,
	USB_STATE_DEVICE,
};

static enum eusb2_repeater_usb_state eusb2_repeater_get_usb_state(struct usb_repeater *ur)
{
	if (!(ur->flags & PHY_CABLE_CONNECTED))
		return USB_STATE_NONE;
	else if (ur->flags & PHY_HOST_MODE)
		return USB_STATE_HOST;
	else
		return USB_STATE_DEVICE;
}

static enum eusb2_repeater_usb_state new_role_value = USB_STATE_NONE;
static enum eusb2_repeater_usb_state last_data_role = USB_STATE_NONE;
static struct class *eusb2_repeater_class = NULL;

static void eusb2_repeater_send_uevent(struct work_struct *work)
{
	struct eusb2_repeater *er = container_of(
        work, struct eusb2_repeater, dwork.work);
	char *envp[2];
	char state_str[32];

	er->usb_enhance_state = er->usb_enhance_state_delayed;
	er->usb_enhance_state_delayed = 0;

	snprintf(state_str, sizeof(state_str), "USB_ENHANCE_STATE=%d", er->usb_enhance_state);
	envp[0] = state_str;
	envp[1] = NULL;

	kobject_uevent_env(&er->sysfs_dev->kobj, KOBJ_CHANGE, envp);
	pr_err("send event %s\n", state_str);

}


static void phy_check_role_event(struct usb_repeater *ur)
{
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);

	new_role_value = eusb2_repeater_get_usb_state(ur);
	pr_err("new_role %d  , last_role%d\n", new_role_value, last_data_role);

	if(er->usb_enhance_state == TYPE_DEVICE_CARWITH){
		switch(last_data_role)
		{
			case USB_STATE_HOST:
			{
				switch(new_role_value)
				{
					case USB_STATE_HOST:
						break;
					case USB_STATE_DEVICE:
						break;
					case USB_STATE_NONE:
						break;
					 default:
					 pr_err("Hi MI PHY_MODE_USB_HOST Invalid value phy mode\n");
						break;
				}
				break;
			}
			case USB_STATE_DEVICE:
			{
				switch(new_role_value)
				{
					case USB_STATE_HOST:
						break;
					case USB_STATE_DEVICE:
						break;
					case USB_STATE_NONE:
							cancel_delayed_work(&er->dwork);
							udelay(100);
							er->usb_enhance_state_delayed = 0;
							schedule_delayed_work(&er->dwork, msecs_to_jiffies(300000));
						break;
					default:
					 pr_err("Hi MI PHY_MODE_USB_DEVICE Invalid value phy mode\n");
						break;
				}
				break;
			}
			case USB_STATE_NONE:
			{
				switch(new_role_value)
				{
					case USB_STATE_HOST:
							cancel_delayed_work(&er->dwork);
							udelay(100);
							er->usb_enhance_state_delayed = 0;
							schedule_delayed_work(&er->dwork, 0);
						break;
					case USB_STATE_DEVICE:
							cancel_delayed_work(&er->dwork);
						break;
					case USB_STATE_NONE:
						break;
					 default:
					 pr_err("Hi MI PHY_MODE_INVALID Invalid value phy mode\n");
						break;
				}
				break;
			}
			default:
				pr_err("Hi MI Invalid value phy mode\n");
				break;
		}
	}
	last_data_role = new_role_value;
	pr_err("phy_check_role_event done!\n");

}

static ssize_t usb_enhance_support_mode_show(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
	struct eusb2_repeater *er = dev_get_drvdata(dev);
	if (!er)
		return -ENODEV;
	return scnprintf(buf, PAGE_SIZE, "%d\n", er->usb_enhance_support_mode);
}
static DEVICE_ATTR_RO(usb_enhance_support_mode);

static ssize_t current_eye_tune_show(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
	struct eusb2_repeater *er = dev_get_drvdata(dev);
	if (!er)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%d\n", er->current_eye_tune);
}
static DEVICE_ATTR_RO(current_eye_tune);

static ssize_t usb_enhance_state_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
	struct eusb2_repeater *er = dev_get_drvdata(dev);
	if (!er)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%d\n", er->usb_enhance_state);
}

static ssize_t usb_enhance_state_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count)
{
	struct eusb2_repeater *er = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!er)
		return -ENODEV;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	er->usb_enhance_state_delayed = val;
	//er->usb_enhance_state = val;
	val = 0;

	cancel_delayed_work(&er->dwork);
	udelay(100);
	schedule_delayed_work(&er->dwork, 0);

	dev_err(dev, "usb_ready_use updated to: %d\n", er->usb_enhance_state);
	return count;
}
static DEVICE_ATTR(usb_enhance_state, 0644, usb_enhance_state_show, usb_enhance_state_store);

static struct attribute *repeater_sysfs_attrs[] = {
	&dev_attr_usb_enhance_support_mode.attr,
	&dev_attr_current_eye_tune.attr,
	&dev_attr_usb_enhance_state.attr,
	NULL,
};

static const struct attribute_group repeater_sysfs_attr_group = {
	.attrs = repeater_sysfs_attrs,
};

static int repeater_sysfs_init(struct eusb2_repeater *er, struct device *dev)
{
	int ret;

	if (!eusb2_repeater_class) {
		eusb2_repeater_class = class_create(REPEATER_SYSFS_CLASS_NAME);
		if (IS_ERR(eusb2_repeater_class)) {
			dev_err(dev, "Failed to create class %s\n",
					REPEATER_SYSFS_CLASS_NAME);
			return PTR_ERR(eusb2_repeater_class);
		}
	}

	er->sysfs_dev = device_create(eusb2_repeater_class, NULL,
					MKDEV(0, 0), er, "usb_enhance");
	if (IS_ERR(er->sysfs_dev)) {
		dev_err(dev, "Failed to create device in %s\n",
				REPEATER_SYSFS_CLASS_NAME);
		return PTR_ERR(er->sysfs_dev);
	}

	ret = sysfs_create_group(&er->sysfs_dev->kobj,
				 &repeater_sysfs_attr_group);
	if (ret) {
		dev_err(dev, "Failed to create sysfs attrs: %d\n", ret);
		device_destroy(eusb2_repeater_class, MKDEV(0, 0));
		return ret;
	}

	dev_info(dev, "Sysfs nodes created under /sys/class/%s/usb_enhance\n",
			REPEATER_SYSFS_CLASS_NAME);
	return 0;
}

/* Perform one or more register read */
static int eusb2_repeater_reg_read(struct eusb2_repeater *er,
			u8 *read_val, u16 reg_offset, size_t reg_count)
{
	int ret;

	ret = regmap_bulk_read(er->regmap, er->reg_base + reg_offset,
						read_val, reg_count);
	if (ret)
		dev_err(er->ur.dev, "read failed: addr=0x%04x, ret=%d\n",
			er->reg_base + reg_offset, ret);
	return ret;
}

/* Perform multiple registers write using given block of data */
static int eusb2_repeater_bulk_reg_write(struct eusb2_repeater *er,
			u16 reg_offset, const void *write_val, size_t reg_count)
{
	int ret;

	ret = regmap_bulk_write(er->regmap, er->reg_base + reg_offset,
						write_val, reg_count);
	if (ret)
		dev_err(er->ur.dev, "bulk write failed: addr=0x%04x, ret=%d\n",
				er->reg_base + reg_offset, ret);
	return ret;
}

/* Perform specific register write using given byte value */
static inline int eusb2_repeater_reg_write(struct eusb2_repeater *er,
					u16 reg_offset, u8 write_val)
{
	return eusb2_repeater_bulk_reg_write(er, reg_offset, &write_val, 1);
}

/* Update specified register using given bit mask and value */
static int eusb2_repeater_masked_write(struct eusb2_repeater *er,
					u16 reg_offset, u8 mask, u8 write_val)
{
	int ret;

	ret = regmap_update_bits(er->regmap, er->reg_base + reg_offset,
						mask, write_val);
	if (ret)
		dev_err(er->ur.dev, "write failed: addr=0x%04x, ret=%d\n",
				er->reg_base + reg_offset, ret);
	return ret;
}

static void eusb2_repeater_update_seq(struct eusb2_repeater *er,
						u32 *seq, u8 cnt)
{
	int i;

	dev_dbg(er->ur.dev, "param override seq count:%d\n", cnt);
	for (i = 0; i < cnt; i = i+2) {
		dev_dbg(er->ur.dev, "write 0x%02x to 0x%02x\n",
						seq[i], seq[i+1]);
		eusb2_repeater_reg_write(er, seq[i+1], seq[i]);
	}
}

static int eusb2_repeater_get_version(struct usb_repeater *ur)
{
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);
	u8 reg;

	eusb2_repeater_reg_read(er, &reg, EUSB2_REVISION1, 1);

	return reg;
}

static void eusb2_repeater_create_debugfs(struct eusb2_repeater *er)
{
	er->usb2_crossover = U8_MAX;
	er->iusb2 = U8_MAX;
	er->res_fsdif = U8_MAX;
	er->hsdisc = U8_MAX;
	er->squelch_u = U8_MAX;
	er->usb2_slew = U8_MAX;
	er->usb2_equ = U8_MAX;
	er->usb2_preem = U8_MAX;
	er->hs_comp_current = U8_MAX;
	er->eusb_slew = U8_MAX;
	er->eusb_equ = U8_MAX;
	er->eusb_hs_comp_current = U8_MAX;

	er->root = debugfs_create_dir(dev_name(er->ur.dev), NULL);
	debugfs_create_x8("usb2_crossover", 0644, er->root,
					&er->usb2_crossover);
	debugfs_create_x8("iusb2", 0644, er->root, &er->iusb2);
	debugfs_create_x8("res_fsdif", 0644, er->root, &er->res_fsdif);
	debugfs_create_x8("hsdisc", 0644, er->root, &er->hsdisc);
	debugfs_create_x8("squelch_u", 0644, er->root, &er->squelch_u);
	debugfs_create_x8("usb2_slew", 0644, er->root, &er->usb2_slew);
	debugfs_create_x8("usb2_equ", 0644, er->root, &er->usb2_equ);
	debugfs_create_x8("usb2_preem", 0644, er->root, &er->usb2_preem);
	debugfs_create_x8("hs_comp_current", 0644, er->root,
					&er->hs_comp_current);
	debugfs_create_x8("eusb_slew", 0644, er->root, &er->eusb_slew);
	debugfs_create_x8("eusb_equ", 0644, er->root, &er->eusb_equ);
	debugfs_create_x8("eusb_hs_comp_current", 0644, er->root,
					&er->eusb_hs_comp_current);
}

static int eusb2_repeater_power(struct eusb2_repeater *er, bool on)
{
	int ret = 0;

	dev_dbg(er->ur.dev, "%s turn %s regulators. power_enabled:%d\n",
			__func__, on ? "on" : "off", er->power_enabled);

	if (er->power_enabled == on) {
		dev_dbg(er->ur.dev, "regulators' regulators are already ON.\n");
		return 0;
	}

	if (!on)
		goto disable_vdd3;

	ret = regulator_set_load(er->vdd18, EUSB2_1P8_HPM_LOAD);
	if (ret < 0) {
		dev_err(er->ur.dev, "Unable to set HPM of vdd18:%d\n", ret);
		goto err_vdd18;
	}

	ret = regulator_set_voltage(er->vdd18, EUSB2_1P8_VOL_MIN,
						EUSB2_1P8_VOL_MAX);
	if (ret) {
		dev_err(er->ur.dev,
				"Unable to set voltage for vdd18:%d\n", ret);
		goto put_vdd18_lpm;
	}

	ret = regulator_enable(er->vdd18);
	if (ret) {
		dev_err(er->ur.dev, "Unable to enable vdd18:%d\n", ret);
		goto unset_vdd18;
	}

	ret = regulator_set_load(er->vdd3, EUSB2_3P0_HPM_LOAD);
	if (ret < 0) {
		dev_err(er->ur.dev, "Unable to set HPM of vdd3:%d\n", ret);
		goto disable_vdd18;
	}

	ret = regulator_set_voltage(er->vdd3, EUSB2_3P0_VOL_MIN,
						EUSB2_3P0_VOL_MAX);
	if (ret) {
		dev_err(er->ur.dev,
				"Unable to set voltage for vdd3:%d\n", ret);
		goto put_vdd3_lpm;
	}

	ret = regulator_enable(er->vdd3);
	if (ret) {
		dev_err(er->ur.dev, "Unable to enable vdd3:%d\n", ret);
		goto unset_vdd3;
	}

	er->power_enabled = true;
	dev_dbg(er->ur.dev, "eUSB2 repeater egulators are turned ON.\n");
	return ret;

disable_vdd3:
	ret = regulator_disable(er->vdd3);
	if (ret)
		dev_err(er->ur.dev, "Unable to disable vdd3:%d\n", ret);

unset_vdd3:
	ret = regulator_set_voltage(er->vdd3, 0, EUSB2_3P0_VOL_MAX);
	if (ret)
		dev_err(er->ur.dev,
			"Unable to set (0) voltage for vdd3:%d\n", ret);

put_vdd3_lpm:
	ret = regulator_set_load(er->vdd3, 0);
	if (ret < 0)
		dev_err(er->ur.dev, "Unable to set (0) HPM of vdd3\n");

disable_vdd18:
	ret = regulator_disable(er->vdd18);
	if (ret)
		dev_err(er->ur.dev, "Unable to disable vdd18:%d\n", ret);

unset_vdd18:
	ret = regulator_set_voltage(er->vdd18, 0, EUSB2_1P8_VOL_MAX);
	if (ret)
		dev_err(er->ur.dev,
			"Unable to set (0) voltage for vdd18:%d\n", ret);

put_vdd18_lpm:
	ret = regulator_set_load(er->vdd18, 0);
	if (ret < 0)
		dev_err(er->ur.dev, "Unable to set LPM of vdd18\n");

err_vdd18:
	/* case handling when regulator turning on failed */
	if (!er->power_enabled)
		return -EINVAL;

	er->power_enabled = false;
	dev_dbg(er->ur.dev, "eUSB2 repeater's regulators are turned OFF.\n");
	return ret;
}

#define INIT_MAX_CNT 5

static void eusb2_repeater_init_mode_eyes(struct usb_repeater *ur)
{
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);
	int final_use = 0;
	final_use = er->usb_enhance_state;

	phy_check_role_event(ur);

	if (ur->flags & PHY_HOST_MODE) {
		switch (final_use){
			case TYPE_HOST_STORAGE:
				eusb2_repeater_update_seq(er, er->host_param_override_seq_storage,
						er->host_param_override_seq_cnt_storage);
				er->current_eye_tune = TYPE_HOST_STORAGE;
				pr_info("HI MI init ms host!\n");
				break;
			case TYPE_HOST_NORMAL :
				eusb2_repeater_update_seq(er, er->host_param_override_seq,
						er->host_param_override_seq_cnt);
				er->current_eye_tune = TYPE_HOST_NORMAL;
				dev_info(er->ur.dev, "HI MI init normal host!\n");
				break;
			default:
				eusb2_repeater_update_seq(er, er->host_param_override_seq,
						er->host_param_override_seq_cnt);
				er->current_eye_tune = 0;
				dev_info(er->ur.dev, "HI MI init normal host!\n");
				break;
		}
	} else {
		switch (final_use){
			case TYPE_DEVICE_CARWITH:
				eusb2_repeater_update_seq(er, er->param_override_seq_carwith,
						er->param_override_seq_cnt_carwith);
				er->current_eye_tune = TYPE_DEVICE_CARWITH;
				pr_info("HI MI init car device!\n");
				break;
			case TYPE_DEVICE_NORMAL:
				eusb2_repeater_update_seq(er, er->param_override_seq,
					er->param_override_seq_cnt);
				er->current_eye_tune = TYPE_DEVICE_NORMAL;
				dev_info(er->ur.dev, "HI MI init device!\n");
				break;
			default:
				eusb2_repeater_update_seq(er, er->param_override_seq,
					er->param_override_seq_cnt);
				er->current_eye_tune = 0;
				dev_info(er->ur.dev, "HI MI init device!\n");
				break;
		}
	}
}

static int eusb2_repeater_init(struct usb_repeater *ur)
{
	u8 status;
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);
	unsigned int rptr_init_cnt = INIT_MAX_CNT;

	eusb2_repeater_init_mode_eyes(ur);

	eusb2_repeater_init_mode_eyes(ur);

	/* override tune params using debugfs based values */
	if (er->usb2_crossover <= 0x7)
		eusb2_repeater_masked_write(er, EUSB2_TUNE_USB2_CROSSOVER,
			TUNE_USB2_CROSSOVER_MASK, er->usb2_crossover);

	if (er->iusb2 <= 0xf)
		eusb2_repeater_masked_write(er, EUSB2_TUNE_IUSB2,
			TUNE_IUSB2_MASK, er->iusb2);

	if (er->res_fsdif <= 0x7)
		eusb2_repeater_masked_write(er, EUSB2_TUNE_RES_FSDIF,
			TUNE_RES_FSIDF_MASK, er->res_fsdif);

	if (er->hsdisc <= 0x7)
		eusb2_repeater_masked_write(er, EUSB2_TUNE_HSDISC,
			TUNE_HSDISC_MASK, er->hsdisc);

	if (er->squelch_u <= 0x7)
		eusb2_repeater_masked_write(er, EUSB2_TUNE_SQUELCH_U,
			TUNE_SQELCH_U_MASK, er->squelch_u);

	if (er->usb2_slew <= 0x3)
		eusb2_repeater_masked_write(er, EUSB2_TUNE_USB2_SLEW,
			TUNE_USB2_SLEW_MASK, er->usb2_slew);

	if (er->usb2_equ <= 0x3)
		eusb2_repeater_masked_write(er, EUSB2_TUNE_USB2_EQU,
			TUNE_USB2_EQU_MASK, er->usb2_equ);

	if (er->usb2_preem <= 0x7)
		eusb2_repeater_masked_write(er, EUSB2_TUNE_USB2_PREEM,
			TUNE_USB2_PREEM_MASK, er->usb2_preem);

	if (er->hs_comp_current <= 0x3)
		eusb2_repeater_masked_write(er, EUSB2_TUNE_USB2_HS_COMP_CUR,
			TUNE_USB2_HS_COMP_CURRENT_MASK, er->hs_comp_current);

	if (er->eusb_slew <= 0x3)
		eusb2_repeater_masked_write(er, EUSB2_TUNE_EUSB_SLEW,
			TUNE_EUSB_SLEW_MASK, er->eusb_slew);

	if (er->eusb_equ <= 0x3)
		eusb2_repeater_masked_write(er, EUSB2_TUNE_EUSB_EQU,
			TUNE_EUSB_EQU_MASK, er->eusb_equ);

	if (er->eusb_hs_comp_current <= 0x3)
		eusb2_repeater_masked_write(er, EUSB2_TUNE_EUSB_HS_COMP_CUR,
			TUNE_EUSB_HS_COMP_CUR_MASK, er->eusb_hs_comp_current);
	/*
	 * CM.Lx is prohibited when repeater is already into Lx state as
	 * per eUSB 1.2 Spec. Below implement software workaround until
	 * PHY and controller is fixing seen observation.
	 */
	if (ur->flags & PHY_HOST_MODE) {
		eusb2_repeater_masked_write(er, EUSB2_FORCE_EN_5,
			F_CLK_19P2M_EN, (1 << F_CLK_19P2M_EN_SHIFT));
		eusb2_repeater_masked_write(er, EUSB2_FORCE_VAL_5,
			V_CLK_19P2M_EN, (1 << V_CLK_19P2M_EN_SHIFT));
	} else {
		/*
		 * In device mode clear host mode related workaround as there
		 * is no repeater reset available, and enable/disable of
		 * repeater doesn't clear previous value due to shared
		 * regulators (say host <-> device mode switch).
		 */
		eusb2_repeater_masked_write(er, EUSB2_FORCE_EN_5,
			F_CLK_19P2M_EN, (0 << F_CLK_19P2M_EN_SHIFT));
		eusb2_repeater_masked_write(er, EUSB2_FORCE_VAL_5,
			V_CLK_19P2M_EN, (0 << V_CLK_19P2M_EN_SHIFT));
	}

	/* Wait for RPTR_STATUS for OK */
	do {
		eusb2_repeater_reg_read(er, &status, EUSB2_RPTR_STATUS, 1);
		if (status & RPTR_OK)
			break;
		usleep_range(10, 20);
	} while (--rptr_init_cnt);

	dev_info(er->ur.dev, "eUSB2 repeater status:%s\n",
			(status & RPTR_OK) ? "OK" : "NOT OK");
	return 0;
}

static int eusb2_repeater_reset(struct usb_repeater *ur,
				bool bring_out_of_reset)
{
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);

	dev_dbg(ur->dev, "eUSB2 repeater %s\n",
		bring_out_of_reset ? "out of reset" : "hold into reset");
	eusb2_repeater_masked_write(er, EUSB2_EN_CTL1, EUSB2_RPTR_EN,
		bring_out_of_reset ? EUSB2_RPTR_EN : 0x0);
	return 0;
}

static int eusb2_repeater_powerup(struct usb_repeater *ur)
{
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);

	return eusb2_repeater_power(er, true);
}

static int eusb2_repeater_powerdown(struct usb_repeater *ur)
{
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);

	return eusb2_repeater_power(er, false);
}

static void get_common_baseline_project_hwid(struct device *dev, struct device_node **param_node)
{
	uint32_t platform_id,hwid_value = 0;
	hwid_value = get_hw_id_value();
	platform_id = get_hw_version_platform();
	if (0){
		if (hwid_value == 0x00008 ||
			hwid_value == 0x00009 ||
			hwid_value == 0x10008 ||
			hwid_value == 0x20008 ||
			hwid_value == 0x90008)
		{
			*param_node = of_get_child_by_name(dev->of_node, "product_yanan");
			pr_err("gh_debug product is yanan\n");
		}
		else
		{
			*param_node = of_get_child_by_name(dev->of_node, "product_warsaw");
			pr_err("gh_debug product is warsaw\n");
		}
	}
	else
		*param_node = NULL;
}
static int eusb2_repeater_read_overrides(struct device *dev, const char *prop,
		u32 **seq, u8 *seq_cnt)
{
	int num_elem, ret;
	struct device_node *param_node = NULL;

	get_common_baseline_project_hwid(dev,&param_node);
	if (!param_node)
		param_node = dev->of_node;

	num_elem = of_property_count_elems_of_size(param_node, prop, sizeof(**seq));
	if (num_elem > 0) {
		if (num_elem % 2) {
			dev_err(dev, "invalid len for %s\n", prop);
			return -EINVAL;
		}
		*seq_cnt = num_elem;
		*seq = devm_kcalloc(dev, num_elem, sizeof(**seq), GFP_KERNEL);
		if (!*seq)
			return -ENOMEM;
		ret = of_property_read_u32_array(param_node, prop, *seq, num_elem);
		if (ret) {
			dev_err(dev, "%s read failed %d\n", prop, ret);
			return ret;
		}
	}

	if (param_node != dev->of_node)
		of_node_put(param_node);

	return 0;
}

static int eusb2_repeater_probe_tree(struct eusb2_repeater *er,struct device *dev)
{
	int ret = 0,tree_value = 0;
	uint32_t platform_id, build_major, build_minor,hwid_major,country;
	struct device_node *param_node = NULL;

	platform_id = get_hw_version_platform();
	build_major = get_hw_version_build();
	build_minor = get_hw_version_minor();
	hwid_major = get_hw_version_major();
	country = get_hw_country_version();

	pr_info("platform_id is %d, build_major is %d, build_minor is %d hwid_major: %d \n", platform_id, build_major, build_minor,hwid_major);

	ret = eusb2_repeater_read_overrides(dev, "qcom,param-override-seq",
			&er->param_override_seq, &er->param_override_seq_cnt);
	if (ret < 0)
		return ret;

	ret = eusb2_repeater_read_overrides(dev, "qcom,host-param-override-seq",
			&er->host_param_override_seq, &er->host_param_override_seq_cnt);
	if (ret < 0)
		return ret;

	get_common_baseline_project_hwid(dev,&param_node);

	if (!param_node)
		param_node = dev->of_node;

	ret = of_property_read_u32(param_node, "en-storage", &tree_value);
	if (ret) {
		pr_info("failed to get en-storage: %d\n", ret);
		tree_value = 0;
	} else {
		pr_info("en-storage = %u\n", tree_value);
	}

	if (tree_value == 1) {
		ret = eusb2_repeater_read_overrides(dev, "qcom,param-override-seq-host-ms",
				&er->host_param_override_seq_storage,
				&er->host_param_override_seq_cnt_storage);
		er->usb_enhance_support_mode |= TYPE_HOST_STORAGE ;
		pr_info("miusb enable storage eye\n");
		if (ret < 0)
			return ret;
	}

	ret = of_property_read_u32(param_node, "en-carwith", &tree_value);
	if (ret) {
		pr_info("failed to get en-carwith: %d\n", ret);
		tree_value = 0;
	} else {
		pr_info("tree_value = %u\n", tree_value);
	}

	if (tree_value == 1) {
		ret = eusb2_repeater_read_overrides(dev, "qcom,param-override-seq-car",
				&er->param_override_seq_carwith,
				&er->param_override_seq_cnt_carwith);
		er->usb_enhance_support_mode |= TYPE_DEVICE_CARWITH ;
		pr_info("miusb enable carwith eye\n");
		if (ret < 0)
			return ret;
	}

	ret = of_property_read_u32(param_node, "en-global", &tree_value);
	if (ret) {
		pr_info("failed to get en-global: %d\n", ret);
		tree_value = 0;
	} else {
		pr_info("tree_value = %u\n", tree_value);
	}

	if((tree_value == 1) && country == CountryGlobal)
	{
		ret = eusb2_repeater_read_overrides(dev, "qcom,param-override-seq-global",
			&er->param_override_seq, &er->param_override_seq_cnt);
		if (ret < 0)
			return ret;
	}

#ifdef CONFIG_FACTORY_BUILD
// This for factory usb device mode
	if(0 || tree_value == 1)
	{
		ret = eusb2_repeater_read_overrides(dev, "qcom,param-override-seq-factory",
			&er->param_override_seq,
			&er->param_override_seq_cnt);

		pr_info("miusb enable factory device scheme\n");
		if (ret < 0)
			return ret;
	}
//This for factory usb host mode
	if(0 || tree_value == 1)
	{
		ret = eusb2_repeater_read_overrides(dev, "qcom,host-param-override-seq-factory",
			&er->host_param_override_seq,
			&er->host_param_override_seq_cnt);

		pr_info("miusb enable factory host scheme\n");
		if (ret < 0)
			return ret;
	}
#endif
	if (param_node != dev->of_node)
		of_node_put(param_node);

	return 0;
}

static int eusb2_repeater_probe(struct platform_device *pdev)
{
	struct eusb2_repeater *er;
	struct device *dev = &pdev->dev;
	int ret = 0, base;

	er = devm_kzalloc(dev, sizeof(*er), GFP_KERNEL);
	if (!er) {
		ret = -ENOMEM;
		goto err_probe;
	}

	er->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!er->regmap) {
		dev_err(&pdev->dev, "failed to get parent's regmap\n");
		ret = -EINVAL;
		goto err_probe;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "reg", &base);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get reg base address:%d\n", ret);
		goto err_probe;
	}

	er->reg_base = base;
	er->vdd3 = devm_regulator_get(dev, "vdd3");
	if (IS_ERR(er->vdd3)) {
		dev_err(dev, "unable to get vdd3 supply\n");
		ret = PTR_ERR(er->vdd3);
		goto err_probe;
	}

	er->vdd18 = devm_regulator_get(dev, "vdd18");
	if (IS_ERR(er->vdd18)) {
		dev_err(dev, "unable to get vdd18 supply\n");
		ret = PTR_ERR(er->vdd18);
		goto err_probe;
	}

	er->usb_enhance_support_mode = 0;
	er->usb_enhance_state = 0;
	er->current_eye_tune = 0;
	er->usb_enhance_state_delayed = 0;

	ret = eusb2_repeater_probe_tree(er,dev);
	if(ret)
		goto err_probe;

	repeater_sysfs_init(er,dev);

	INIT_DELAYED_WORK(&er->dwork, eusb2_repeater_send_uevent);
	er->ur.dev = dev;

	platform_set_drvdata(pdev, er);

	er->ur.init		= eusb2_repeater_init;
	er->ur.reset		= eusb2_repeater_reset;
	er->ur.powerup		= eusb2_repeater_powerup;
	er->ur.powerdown	= eusb2_repeater_powerdown;
	er->ur.get_version	= eusb2_repeater_get_version;

	ret = usb_add_repeater_dev(&er->ur);
	if (ret)
		goto err_probe;

	eusb2_repeater_create_debugfs(er);
	return 0;

err_probe:
	return ret;
}

static int eusb2_repeater_remove(struct platform_device *pdev)
{
	struct eusb2_repeater *er = platform_get_drvdata(pdev);

	if (!er)
		return 0;

	debugfs_remove_recursive(er->root);
	if(er->sysfs_dev != NULL)
		sysfs_remove_group(&er->sysfs_dev->kobj, &repeater_sysfs_attr_group);
	if(eusb2_repeater_class != NULL)
		device_destroy(eusb2_repeater_class, MKDEV(0, 0));
	usb_remove_repeater_dev(&er->ur);
	eusb2_repeater_power(er, false);
	return 0;
}

static const struct of_device_id eusb2_repeater_id_table[] = {
	{
		.compatible = "qcom,pmic-eusb2-repeater",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, eusb2_repeater_id_table);

static struct platform_driver eusb2_repeater_driver = {
	.probe		= eusb2_repeater_probe,
	.remove		= eusb2_repeater_remove,
	.driver = {
		.name	= "eusb2-repeater",
		.of_match_table = of_match_ptr(eusb2_repeater_id_table),
	},
};

module_platform_driver(eusb2_repeater_driver);
MODULE_DESCRIPTION("QTI PMIC eUSB2 repeater driver");
MODULE_LICENSE("GPL");
