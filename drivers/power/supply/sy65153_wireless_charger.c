/*
 * Driver for Silergy sy65153 wireless charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>

#define SY65153_REG_00	0x0
#define SY65153_REG_01	0x1
#define SY65153_REG_04	0x4
#define SY65153_REG_05	0x5
#define SY65153_REG_06	0x6
#define SY65153_REG_07	0x7
#define SY65153_REG_34	0x34
#define SY65153_REG_35	0x35
#define SY65153_REG_3A	0x3A
#define SY65153_REG_3B	0x3B
#define SY65153_REG_3C	0x3C
#define SY65153_REG_3D	0x3D
#define SY65153_REG_3E	0x3E
#define SY65153_REG_40	0x40
#define SY65153_REG_41	0x41
#define SY65153_REG_44	0x44
#define SY65153_REG_45	0x45
#define SY65153_REG_46	0x46
#define SY65153_REG_47	0x47
#define SY65153_REG_4A	0x4A
#define SY65153_REG_4E	0x4E
#define SY65153_REG_68	0x68
#define SY65153_REG_69	0x69
#define SY65153_REG_NUM	23

/* Register bits */
/* SY65153_REG_00 (0x00) Identification Part_number_L reg */
#define SY65153_REG_PART_NUM_L_MASK		GENMASK(7, 0)
#define SY65153_REG_PART_NUM_L_SHIFT		0

/* SY65153_REG_01 (0x01) Identification Part_number_H reg */
#define SY65153_REG_PART_NUM_H_MASK		GENMASK(7, 0)
#define SY65153_REG_PART_NUM_H_SHIFT		0

/* SY65153_REG_04 (0x04)  Firmware Major Revision FW_Major_Rev_L reg */
#define SY65153_REG_FW_MAJOR_REV_L_MASK		GENMASK(7, 0)
#define SY65153_REG_FW_MAJOR_REV_L_SHIFT	0

/* SY65153_REG_05 (0x05)   Firmware Major Revision FW_Major_Rev_H reg */
#define SY65153_REG_FW_MAJOR_REV_H_MASK		GENMASK(7, 0)
#define SY65153_REG_FW_MAJOR_REV_H_SHIFT	0

/* SY65153_REG_06 (0x06) Firmware Minor Revision FW_Minor_Rev_L reg */
#define SY65153_REG_FW_MINOR_REV_L_MASK		GENMASK(7, 0)
#define SY65153_REG_FW_MINOR_REV_L_SHIFT	0

/* SY65153_REG_07 (0x07)  Firmware Minor Revision FW_Minor_Rev_H reg */
#define SY65153_REG_FW_MINOR_REV_H_MASK		GENMASK(7, 0)
#define SY65153_REG_FW_MINOR_REV_H_SHIFT	0

/* SY65153_REG_34 (0x34)  status reg */
#define SY65153_REG_VOUT_STATUS_MASK		GENMASK(7, 7)
#define SY65153_REG_VOUT_STATUS_SHIFT		7
#define SY65153_REG_HANDSHAKE_PROTOCOL_MASK	GENMASK(5, 4)
#define SY65153_REG_HANDSHAKE_PROTOCOL_SHIFT	4
#define SY65153_REG_THERMAL_SHTDN_STATUS_MASK	GENMASK(2, 2)
#define SY65153_REG_THERMAL_SHTDN_STATUS_SHIFT  2
#define SY65153_REG_VRECT_OV_STATUS_MASK	GENMASK(1, 1)
#define SY65153_REG_VRECT_OV_STATUS_SHIFT	1
#define SY65153_REG_CURRENT_LIMIT_STATUS_MASK	GENMASK(0, 0)
#define SY65153_REG_CURRENT_LIMIT_STATUS_SHIFT  0

/* SY65153_REG_35 (0x35)  Reserved reg */

/* SY65153_REG_3A (0x3A)  Battery Charge Status reg */
#define SY65153_REG_BATT_CHARGE_STATUS_MASK	GENMASK(7, 0)
#define SY65153_REG_BATT_CHARGE_STATUS_SHIFT	0

/* SY65153_REG_3B (0x3B)  End Power Transfer reg */
#define SY65153_REG_EPT_CODE_MASK		GENMASK(7, 0)
#define SY65153_REG_EPT_CODE_SHIFT		0

/* vout = ADC_VOUT * 6 * 2.1 / 4095 */
/* SY65153_REG_3C (0x3C)  Output Voltage ADC_VOUT_L reg */
#define SY65153_REG_ADC_VOUT_L_MASK		GENMASK(7, 0)
#define SY65153_REG_ADC_VOUT_L_SHIFT		0

/* SY65153_REG_3D (0x3D)  Output Voltage ADC_VOUT_H reg */
#define SY65153_REG_ADC_VOUT_H_MASK		GENMASK(3, 0)
#define SY65153_REG_ADC_VOUT_H_SHIFT		0

/* VOUT = VOUT_SET * 0.1 + 3.5V */
/* SY65153_REG_3E (0x3E) Setting Output Voltage reg */
#define SY65153_REG_VOUT_SET_MASK		GENMASK(7, 0)
#define SY65153_REG_VOUT_SET_SHIFT		0

/* VOUT = ADC_VRECT * 6 * 21 / 4095 / 10 */
/* SY65153_REG_40 (0x40)  VRECT Voltage ADC_VRECT_L reg */
#define SY65153_REG_ADC_VRECT_L_MASK		GENMASK(7, 0)
#define SY65153_REG_ADC_VRECT_L_SHIFT		0

/* SY65153_REG_41 (0x41)  VRECT Voltage ADC_VRECT_H reg */
#define SY65153_REG_ADC_VRECT_H_MASK		GENMASK(3, 0)
#define SY65153_REG_ADC_VRECT_H_SHIFT		0

/* IOUT = RX_IOUT / 975 */
/* SY65153_REG_44 (0x44)  Iout Current RX_IOUT_L reg */
#define SY65153_REG_RX_IOUT_L_MASK		GENMASK(7, 0)
#define SY65153_REG_RX_IOUT_L_SHIFT		0

/* SY65153_REG_45 (0x45)  Iout Current RX_IOUT_H reg */
#define SY65153_REG_RX_IOUT_H_MASK		GENMASK(7, 0)
#define SY65153_REG_RX_IOUT_H_SHIFT		0

/*  TDIE = 352 - ADC_DIE_TEMP * 525 / 4095*/
/* SY65153_REG_46 (0x46)  Die Temperature ADC_Die_Temp_L reg */
#define SY65153_REG_ADC_DIE_TEMP_L_MASK		GENMASK(7, 0)
#define SY65153_REG_ADC_DIE_TEMP_L_SHIFT	0

/* SY65153_REG_47 (0x47) Die Temperature ADC_Die_Temp_H reg */
#define SY65153_REG_ADC_DIE_TEMP_H_MASK		GENMASK(3, 0)
#define SY65153_REG_ADC_DIE_TEMP_H_SHIFT	0

/*  ILIM =  RX_ILIM * 0.1 + 0.1(A)*/
/* SY65153_REG_4A (0x4A)  RX_ILIM reg */
#define SY65153_REG_RX_ILIM_MASK		GENMASK(7, 0)
#define SY65153_REG_RX_ILIM_SHIFT		0

/* SY65153_REG_4E (0x4E)  Command reg */
#define SY65153_REG_CLEAR_INTERRUPT_MASK	GENMASK(5, 5)
#define SY65153_REG_CLEAR_INTERRUPT_SHIFT	5
#define SY65153_REG_SEND_BATTERTY_CHARGE_STATUS_MASK	GENMASK(4, 4)
#define SY65153_REG_SEND_BATTERTY_CHARGE_STATUS_SHIFT	4
#define SY65153_REG_SEND_END_POWER_TRANSFER_MASK	GENMASK(3, 3)
#define SY65153_REG_SEND_END_POWER_TRANSFER_SHIFT	3
#define SY65153_REG_TOGGLE_LDO_ON_OFF_MASK		GENMASK(1, 1)
#define SY65153_REG_TOGGLE_LDO_ON_OFF_SHIFT	1

/* RPPO = I2C_RPPO * 4095 / 255 (mW) */
/* SY65153_REG_68 (0x68)  FOD Calibration I2C_RPPO reg */
#define SY65153_REG_I2C_RPPO_MASK		GENMASK(7, 0)
#define SY65153_REG_I2C_RPPO_SHIFT		0

/* RPPG = I2C_RPPG * 4095 / 255 (%) */
/* SY65153_REG_69 (0x69)  FOD Calibration I2C_RPPG reg */
#define SY65153_REG_I2C_RPPG_MASK		GENMASK(7, 0)
#define SY65153_REG_I2C_RPPG_SHIFT		0

#define SY65153_RPP_TYPE_UNKNOWN		(0x0)
#define SY65153_RPP_TYPE_BPP			(0x1)
#define SY65153_RPP_TYPE_10W			(0x2)
#define SY65153_RPP_TYPE_EPP			(0x3)

#define SY65153_VOUT_MAX			12000000
#define SY65153_VOUT_MIN			3500000
#define SY65153_VOUT_OFFSET			3500000
#define SY65153_VOUT_STEP			100000
#define SY65153_VOUT_5V				5000000
#define SY65153_VOUT_9V				9000000
#define SY65153_VOUT_12V			12000000

#define SY65153_ILIM_MAX			1200000
#define SY65153_ILIM_MIN			100000
#define SY65153_ILIM_OFFSET			100000
#define SY65153_ILIM_STEP			100000

#define SY65153_WAKE_UP_MS			2000
#define SY65153_PG_INT_WORK_MS			msecs_to_jiffies(500)

struct sy65153_wl_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sy65153_dump_reg;
	struct device_attribute attr_sy65153_lookup_reg;
	struct device_attribute attr_sy65153_sel_reg_id;
	struct device_attribute attr_sy65153_reg_val;
	struct attribute *attrs[5];

	struct sy65153_wl_charger_info *info;
};

struct sy65153_wl_charger_info {
	char *name;
	struct i2c_client *client;
	struct device *dev;
	struct regmap       *regmap;
	struct delayed_work    wireless_pg_int_work;
	struct delayed_work    wireless_intb_int_work;
	struct mutex    wireless_chg_pg_lock;
	struct mutex    wireless_chg_intb_lock;
	struct power_supply    *wl_psy;
	struct sy65153_wl_charger_sysfs *sysfs;

	unsigned int irq_gpio;
	unsigned int power_good_gpio;
	unsigned int switch_chg_en_gpio;
	unsigned int switch_flag_en_gpio;

	int pg_irq;
	int pg_retry_cnt;
	int online;
	bool is_charging;
	u32 part_num;
	u32 fw_major_rev;
	u32 fw_minor_rev;
	u32 limit;

	int reg_id;
};

struct sy65153_wl_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct sy65153_wl_charger_reg_tab reg_tab[SY65153_REG_NUM + 1] = {
	{0, SY65153_REG_00, "Identification Part_number_L reg"},
	{1, SY65153_REG_01, "Identification Part_number_H reg"},
	{2, SY65153_REG_04, "Revision FW_Major_Rev_L reg"},
	{3, SY65153_REG_05, "Revision FW_Major_Rev_H reg"},
	{4, SY65153_REG_06, "Revision FW_Minor_Rev_L reg"},
	{5, SY65153_REG_07, "Revision FW_Minor_Rev_H reg"},
	{6, SY65153_REG_34, "status reg"},
	{7, SY65153_REG_35, "Reserved reg"},
	{8, SY65153_REG_3A, "Battery Charge Status reg"},
	{9, SY65153_REG_3B, "End Power Transfer reg"},
	{10, SY65153_REG_3C, "Output Voltage ADC_VOUT_L reg"},
	{11, SY65153_REG_3D, "Output Voltage ADC_VOUT_H reg"},
	{12, SY65153_REG_3E, "Setting Output Voltage reg"},
	{13, SY65153_REG_40, "VRECT Voltage ADC_VRECT_L reg"},
	{14, SY65153_REG_41, "VRECT Voltage ADC_VRECT_H reg"},
	{15, SY65153_REG_44, "Iout Current RX_IOUT_L reg"},
	{16, SY65153_REG_45, "Iout Current RX_IOUT_H reg"},
	{17, SY65153_REG_46, "Die Temperature ADC_Die_Temp_L reg"},
	{18, SY65153_REG_47, "Die Temperature ADC_Die_Temp_H reg"},
	{19, SY65153_REG_4A, "RX_ILIM reg"},
	{20, SY65153_REG_4E, "Command reg"},
	{21, SY65153_REG_68, "FOD Calibration I2C_RPPO reg"},
	{22, SY65153_REG_69, "FOD Calibration I2C_RPPG reg"},
	{23, 0, "null"},
};

static void sy65153_wl_charger_dump_register(struct sy65153_wl_charger_info *info)
{
	int i, ret, len, idx = 0;
	u32 reg_val;
	char buf[512];

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < SY65153_REG_NUM; i++) {
		ret = regmap_read(info->regmap, reg_tab[i].addr, &reg_val);
		if (ret == 0) {
			len = snprintf(buf + idx, sizeof(buf) - idx,
				       "[REG_0x%.2x]=0x%.2x; ", reg_tab[i].addr, reg_val);
			idx += len;
		}
	}

	dev_info(info->dev, "%s: %s", __func__, buf);
}

static int sy65153_wl_charger_get_part_number(struct sy65153_wl_charger_info *info)
{
	u8 part_num[2] = {0};
	int ret;

	ret = regmap_bulk_read(info->regmap, SY65153_REG_00,
			       part_num, ARRAY_SIZE(part_num));
	if (ret)
		return ret;

	info->part_num |= (u32)part_num[0];
	info->part_num |= (((u32)part_num[1]) << 8);

	dev_dbg(info->dev, "part_num_l = 0x%x, part_num_h = 0x%x, part_num = 0x%x\n",
		 part_num[0], part_num[1], info->part_num);

	return ret;
}

static int sy65153_wl_charger_get_fw_major_revision(struct sy65153_wl_charger_info *info)
{
	u8 major_rev[2] = {0};
	int ret;

	ret = regmap_bulk_read(info->regmap, SY65153_REG_04,
			       major_rev, ARRAY_SIZE(major_rev));
	if (ret)
		return ret;

	info->fw_major_rev |= (u32)major_rev[0];
	info->fw_major_rev |= (((u32)major_rev[1]) << 8);

	dev_dbg(info->dev, "major_l = 0x%x, major_h = 0x%x, fw_major_rev = 0x%x\n",
		 major_rev[0], major_rev[1], info->fw_major_rev);

	return ret;
}

static int sy65153_wl_charger_get_fw_minor_revision(struct sy65153_wl_charger_info *info)
{
	u8 minor_rev[2] = {0};
	int ret;

	ret = regmap_bulk_read(info->regmap, SY65153_REG_06,
			       minor_rev, ARRAY_SIZE(minor_rev));
	if (ret)
		return ret;

	info->fw_minor_rev |= (u32)minor_rev[0];
	info->fw_minor_rev |= (((u32)minor_rev[1]) << 8);

	dev_dbg(info->dev, "minor_l = 0x%x, minor_h = 0x%x, fw_minor_rev = 0x%x\n",
		 minor_rev[0], minor_rev[1], info->part_num);

	return ret;
}

static bool sy65153_wl_charger_get_vout_status(struct sy65153_wl_charger_info *info)
{
	u32 val;
	int ret;
	bool status = false;

	ret = regmap_read(info->regmap, SY65153_REG_34, &val);
	if (ret) {
		dev_err(info->dev, "fail to get vout status, ret = %d\n", ret);
		return status;
	}

	val = (val & SY65153_REG_VOUT_STATUS_MASK) >> SY65153_REG_VOUT_STATUS_SHIFT;

	if (val)
		status = true;

	return status;
}

/**
 * return value:
 *	00  None
 *	01  BPP
 *	10  10W Mode
 *	11  EPP
 */
static u32 sy65153_wl_charger_get_rx_rpp_type(struct sy65153_wl_charger_info *info)
{
	u32 val = 0;
	u32 rpp_type = 0;
	int ret;

	ret = regmap_read(info->regmap, SY65153_REG_34, &val);
	if (ret) {
		dev_err(info->dev, "fail to get rpp type, ret = %d\n", ret);
		return rpp_type;
	}

	rpp_type = (val & SY65153_REG_HANDSHAKE_PROTOCOL_MASK) >>
		SY65153_REG_HANDSHAKE_PROTOCOL_SHIFT;

	return rpp_type;
}

static bool sy65153_wl_charger_get_thermal_shtdn_status(struct sy65153_wl_charger_info *info)
{
	u32 val;
	int ret;
	bool status = false;

	ret = regmap_read(info->regmap, SY65153_REG_34, &val);
	if (ret) {
		dev_err(info->dev, "fail to get thermal shutdown status, ret = %d\n", ret);
		return status;
	}

	val = (val & SY65153_REG_THERMAL_SHTDN_STATUS_MASK) >>
		SY65153_REG_THERMAL_SHTDN_STATUS_SHIFT;

	if (val)
		status = true;

	return status;
}

static bool sy65153_wl_charger_get_vrect_ovp_status(struct sy65153_wl_charger_info *info)
{
	u32 val;
	int ret;
	bool status = false;

	ret = regmap_read(info->regmap, SY65153_REG_34, &val);
	if (ret) {
		dev_err(info->dev, "fail to get vrect ovp status, ret = %d\n", ret);
		return status;
	}

	val = (val & SY65153_REG_VRECT_OV_STATUS_MASK) >> SY65153_REG_VRECT_OV_STATUS_SHIFT;

	if (val)
		status = true;

	return status;
}

static bool sy65153_wl_charger_get_current_limit_status(struct sy65153_wl_charger_info *info)
{
	u32 val;
	int ret;
	bool status = false;

	ret = regmap_read(info->regmap, SY65153_REG_34, &val);
	if (ret) {
		dev_err(info->dev, "fail to get current limit status, ret = %d\n", ret);
		return status;
	}

	val = (val & SY65153_REG_CURRENT_LIMIT_STATUS_MASK)
		>> SY65153_REG_CURRENT_LIMIT_STATUS_SHIFT;

	if (val)
		status = true;

	return status;
}

static int
sy65153_wl_charger_get_batt_charge_status(struct sy65153_wl_charger_info *info, u32 *value)
{
	int ret;

	*value = 0;

	ret = regmap_read(info->regmap, SY65153_REG_3A, value);
	if (ret)
		dev_err(info->dev, "fail to set batt_charge status, ret = %d\n", ret);

	return ret;
}

static int
sy65153_wl_charger_get_ept_code_status(struct sy65153_wl_charger_info *info, u32 *ept_code)
{
	int ret;

	*ept_code = 0;

	ret = regmap_read(info->regmap, SY65153_REG_3B, ept_code);
	if (ret)
		dev_err(info->dev, "fail to set ept code, ret = %d\n", ret);

	dev_info(info->dev, "ept code =  0x%x\n", *ept_code);

	return ret;
}

static int sy65153_wl_charger_get_vout(struct sy65153_wl_charger_info *info, u32 *vout)
{
	u8 vout_adc[2] = {0};
	int ret;
	u32 voutadc = 0;

	ret = regmap_bulk_read(info->regmap, SY65153_REG_3C,
			       vout_adc, ARRAY_SIZE(vout_adc));
	if (ret) {
		dev_err(info->dev, "fail to get vout, ret = %d\n", ret);
		return ret;
	}

	dev_dbg(info->dev, "voutadc_l = 0x%x, voutadc_h = 0x%x\n", vout_adc[0], vout_adc[1]);
	voutadc |= (u32)vout_adc[0];
	voutadc |= ((((u32)vout_adc[1]) & SY65153_REG_ADC_VOUT_H_MASK) << 8);

	*vout = voutadc * 6 * 21 / 10 / 4095 * 1000;

	dev_info(info->dev, "vout =  %d\n", *vout);

	return ret;
}

static int sy65153_wl_charger_set_vout(struct sy65153_wl_charger_info *info, u32 vout)
{
	int ret;
	u32 reg_val;

	dev_info(info->dev, "vout =  %d\n", vout);

	if (vout > SY65153_VOUT_MAX)
		vout = SY65153_VOUT_MAX;
	else if (vout < SY65153_VOUT_MIN)
		vout = SY65153_VOUT_MIN;

	reg_val = (vout - SY65153_VOUT_MIN) / SY65153_VOUT_STEP;

	ret = regmap_write(info->regmap, SY65153_REG_3E, reg_val);
	if (ret)
		dev_err(info->dev, "fail to set vout = %d, ret = %d\n", vout, ret);

	return ret;
}

static int sy65153_wl_charger_get_vrect(struct sy65153_wl_charger_info *info, u32 *vrect)
{
	u8 vrect_adc[2] = {0};
	int ret;
	u32 vrectadc = 0;

	ret = regmap_bulk_read(info->regmap, SY65153_REG_40,
			       vrect_adc, ARRAY_SIZE(vrect_adc));
	if (ret) {
		dev_err(info->dev, "fail to get vrect, ret = %d\n", ret);
		return ret;
	}

	dev_dbg(info->dev, "vrect_l = 0x%x, vrect_h = 0x%x\n", vrect_adc[0], vrect_adc[1]);
	vrectadc |= (u32)vrect_adc[0];
	vrectadc |= ((((u32)vrect_adc[1]) & SY65153_REG_ADC_VRECT_H_MASK) << 8);

	*vrect = vrectadc * 21 / 4095;

	dev_info(info->dev, "vrect = %d\n", *vrect);

	return ret;
}

static int sy65153_wl_charger_get_rx_iout(struct sy65153_wl_charger_info *info, u32 *iout)
{
	u8 rx_iout[2] = {0};
	u32 ioutadc = 0;
	int ret;

	ret = regmap_bulk_read(info->regmap, SY65153_REG_44,
			       rx_iout, ARRAY_SIZE(rx_iout));
	if (ret) {
		dev_err(info->dev, "fail to get rx iout, ret = %d\n", ret);
		return ret;
	}

	dev_info(info->dev, "rx_iout_l = 0x%x, rx_iout_h = 0x%x\n", rx_iout[0], rx_iout[1]);
	ioutadc |= (u32)rx_iout[0];
	ioutadc |= (((u32)rx_iout[1]) << 8);

	*iout = ioutadc * 1000000 / 975;

	dev_info(info->dev, "iout = %d\n", *iout);

	return ret;
}

static int sy65153_wl_charger_get_rx_tdie(struct sy65153_wl_charger_info *info, u32 *tdie)
{
	u8 tdie_adc[2] = {0};
	u32 dieadc = 0;
	int ret;

	*tdie = 0;

	ret = regmap_bulk_read(info->regmap, SY65153_REG_44,
			       tdie_adc, ARRAY_SIZE(tdie_adc));
	if (ret) {
		dev_err(info->dev, "fail to get Tdie, ret = %d\n", ret);
		return ret;
	}

	dev_info(info->dev, "Tdie_l = 0x%x, Tdie_h = 0x%x\n", tdie_adc[0], tdie_adc[1]);
	dieadc |= (u32)tdie_adc[0];
	dieadc |= (((u32)tdie_adc[1]) << 8);

	*tdie = 3520 - dieadc * 10 * 525 / 4095;

	dev_info(info->dev, "Tdie = %d\n", *tdie);

	return ret;
}

static int sy65153_wl_charger_set_rx_ilim(struct sy65153_wl_charger_info *info, u32 ilim)
{
	int ret;
	u32 reg_val;

	dev_info(info->dev, "set rx_ilim =  %d\n", ilim);

	if (ilim > SY65153_ILIM_MAX)
		ilim = SY65153_ILIM_MAX;
	else if (ilim < SY65153_ILIM_MIN)
		ilim = SY65153_ILIM_MIN;

	reg_val = (ilim - SY65153_ILIM_OFFSET) / SY65153_ILIM_STEP;

	ret = regmap_write(info->regmap, SY65153_REG_4A, reg_val);
	if (ret)
		dev_err(info->dev, "fail to set ilim = %d, ret = %d\n", ilim, ret);

	return ret;
}

static int sy65153_wl_charger_get_rx_ilim(struct sy65153_wl_charger_info *info, u32 *ilim)
{
	u32 val = 0;
	int ret;

	*ilim = 0;

	ret = regmap_read(info->regmap, SY65153_REG_4A, &val);
	if (ret)
		dev_err(info->dev, "fail to get rx ilim, ret = %d\n", ret);

	*ilim = val * SY65153_ILIM_STEP + SY65153_ILIM_OFFSET;

	dev_info(info->dev, "rx_ilim =  0x%x\n", *ilim);

	return ret;
}

static int sy65153_wl_charger_clear_interrupt(struct sy65153_wl_charger_info *info, bool flag)
{
	int ret;
	u32 val = 0;

	if (flag)
		val = 1;

	ret =  regmap_update_bits(info->regmap, SY65153_REG_4E,
				  SY65153_REG_CLEAR_INTERRUPT_MASK,
				  val << SY65153_REG_CLEAR_INTERRUPT_SHIFT);
	if (ret)
		dev_err(info->dev, "fail to clear interrupt, flag = %d, ret = %d\n", flag, ret);

	return ret;
}

static int sy65153_wl_charger_toggle_ldo_on_off(struct sy65153_wl_charger_info *info, bool flag)
{
	int ret;
	u32 val = 0;

	if (flag)
		val = 1;

	ret =  regmap_update_bits(info->regmap, SY65153_REG_4E,
				  SY65153_REG_TOGGLE_LDO_ON_OFF_MASK,
				  val << SY65153_REG_TOGGLE_LDO_ON_OFF_SHIFT);
	if (ret)
		dev_err(info->dev, "fail to Toggle_LDO_On-OFF, flag = %d, ret = %d\n",
			flag, ret);

	return ret;
}

static enum power_supply_property sy65153_wireless_properties[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CALIBRATE,
};

static int sy65153_wl_charger_set_property(struct power_supply *psy,
					   enum power_supply_property prop,
					   const union power_supply_propval *val)
{
	int ret = 0;
	struct sy65153_wl_charger_info *info = power_supply_get_drvdata(psy);

	if (!info)
		return -EINVAL;

	switch (prop) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sy65153_wl_charger_set_vout(info, (u32)val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sy65153_wl_charger_set_rx_ilim(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		ret = sy65153_wl_charger_toggle_ldo_on_off(info, val->intval);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int sy65153_wl_charger_get_property(struct power_supply *psy,
					   enum power_supply_property prop,
					   union power_supply_propval *val)
{
	int ret = 0;
	u32 rpp_type;
	struct sy65153_wl_charger_info *info = power_supply_get_drvdata(psy);

	if (!info)
		return -EINVAL;

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = info->online;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sy65153_wl_charger_get_vrect(info, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = sy65153_wl_charger_get_rx_iout(info, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sy65153_wl_charger_get_vout(info, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		rpp_type = sy65153_wl_charger_get_rx_rpp_type(info);
		if (rpp_type == SY65153_RPP_TYPE_BPP || rpp_type == SY65153_RPP_TYPE_10W)
			val->intval = POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP;
		else if (rpp_type == SY65153_RPP_TYPE_EPP)
			val->intval = POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP;
		else
			val->intval = POWER_SUPPLY_WIRELESS_CHARGER_TYPE_UNKNOWN;
		break;
	default:
		ret =  -EINVAL;
	}

	return ret;
}

static const struct regmap_config sy65153_wl_charger_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0xFFFF,
};

static int sy65153_wl_charger_prop_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static const struct power_supply_desc sy65153_wl_charger_desc = {
	.name			= "sy65153_wireless_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sy65153_wireless_properties,
	.num_properties		= ARRAY_SIZE(sy65153_wireless_properties),
	.get_property		= sy65153_wl_charger_get_property,
	.set_property		= sy65153_wl_charger_set_property,
	.property_is_writeable	= sy65153_wl_charger_prop_is_writeable,
};

static ssize_t sy65153_register_value_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sy65153_wl_charger_sysfs *sy65153_sysfs
		= container_of(attr, struct sy65153_wl_charger_sysfs,
		attr_sy65153_reg_val);
	struct sy65153_wl_charger_info *info = sy65153_sysfs->info;
	u32 val;
	int ret;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s sy65153_sysfs->info is null\n", __func__);

	ret = regmap_read(info->regmap, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get SY65153_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return snprintf(buf, PAGE_SIZE, "fail to get SY65153_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return snprintf(buf, PAGE_SIZE, "SY65153_REG_0x%.2x = 0x%.2x\n",
			reg_tab[info->reg_id].addr, val);
}

static ssize_t sy65153_register_value_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct sy65153_wl_charger_sysfs *sy65153_sysfs
		= container_of(attr, struct sy65153_wl_charger_sysfs,
		attr_sy65153_reg_val);
	struct sy65153_wl_charger_info *info = sy65153_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s sy65153_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = regmap_write(info->regmap, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val, reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t sy65153_register_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct sy65153_wl_charger_sysfs *sy65153_sysfs
		= container_of(attr, struct sy65153_wl_charger_sysfs,
		attr_sy65153_sel_reg_id);
	struct sy65153_wl_charger_info *info = sy65153_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s sy65153_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", sy65153_sysfs->name);
		return count;
	}

	if (id < 0 || id >= SY65153_REG_NUM) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			sy65153_sysfs->name, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s store register id = %d success\n", sy65153_sysfs->name, id);
	return count;
}

static ssize_t sy65153_register_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sy65153_wl_charger_sysfs *sy65153_sysfs
		= container_of(attr, struct sy65153_wl_charger_sysfs,
		attr_sy65153_sel_reg_id);
	struct sy65153_wl_charger_info *info = sy65153_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s sy65153_sysfs->info is null\n", __func__);

	return snprintf(buf, PAGE_SIZE, "Cuurent register id = %d\n", info->reg_id);
}

static ssize_t sy65153_register_table_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sy65153_wl_charger_sysfs *sy65153_sysfs
		= container_of(attr, struct sy65153_wl_charger_sysfs,
		attr_sy65153_lookup_reg);
	struct sy65153_wl_charger_info *info = sy65153_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[2000];

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s sy65153_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; i < SY65153_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s]; \n",
			       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", reg_tab_buf);
}

static ssize_t sy65153_dump_register_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sy65153_wl_charger_sysfs *sy65153_sysfs
		= container_of(attr, struct sy65153_wl_charger_sysfs,
		attr_sy65153_dump_reg);
	struct sy65153_wl_charger_info *info = sy65153_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s sy65153_sysfs->info is null\n", __func__);

	sy65153_wl_charger_dump_register(info);

	return snprintf(buf, PAGE_SIZE, "%s\n", sy65153_sysfs->name);
}

static int sy65153_register_sysfs(struct sy65153_wl_charger_info *info)
{
	struct sy65153_wl_charger_sysfs *sy65153_sysfs;
	int ret;

	sy65153_sysfs = devm_kzalloc(info->dev, sizeof(*sy65153_sysfs), GFP_KERNEL);
	if (!sy65153_sysfs)
		return -EINVAL;

	info->sysfs = sy65153_sysfs;
	sy65153_sysfs->name = "sy65153_sysfs";
	sy65153_sysfs->info = info;
	sy65153_sysfs->attrs[0] = &sy65153_sysfs->attr_sy65153_dump_reg.attr;
	sy65153_sysfs->attrs[1] = &sy65153_sysfs->attr_sy65153_lookup_reg.attr;
	sy65153_sysfs->attrs[2] = &sy65153_sysfs->attr_sy65153_sel_reg_id.attr;
	sy65153_sysfs->attrs[3] = &sy65153_sysfs->attr_sy65153_reg_val.attr;
	sy65153_sysfs->attrs[4] = NULL;
	sy65153_sysfs->attr_g.name = "debug";
	sy65153_sysfs->attr_g.attrs = sy65153_sysfs->attrs;

	sysfs_attr_init(&sy65153_sysfs->attr_sy65153_dump_reg.attr);
	sy65153_sysfs->attr_sy65153_dump_reg.attr.name = "sy65153_dump_reg";
	sy65153_sysfs->attr_sy65153_dump_reg.attr.mode = 0444;
	sy65153_sysfs->attr_sy65153_dump_reg.show = sy65153_dump_register_show;

	sysfs_attr_init(&sy65153_sysfs->attr_sy65153_lookup_reg.attr);
	sy65153_sysfs->attr_sy65153_lookup_reg.attr.name = "sy65153_lookup_reg";
	sy65153_sysfs->attr_sy65153_lookup_reg.attr.mode = 0444;
	sy65153_sysfs->attr_sy65153_lookup_reg.show = sy65153_register_table_show;

	sysfs_attr_init(&sy65153_sysfs->attr_sy65153_sel_reg_id.attr);
	sy65153_sysfs->attr_sy65153_sel_reg_id.attr.name = "sy65153_sel_reg_id";
	sy65153_sysfs->attr_sy65153_sel_reg_id.attr.mode = 0644;
	sy65153_sysfs->attr_sy65153_sel_reg_id.show = sy65153_register_id_show;
	sy65153_sysfs->attr_sy65153_sel_reg_id.store = sy65153_register_id_store;

	sysfs_attr_init(&sy65153_sysfs->attr_sy65153_reg_val.attr);
	sy65153_sysfs->attr_sy65153_reg_val.attr.name = "sy65153_reg_val";
	sy65153_sysfs->attr_sy65153_reg_val.attr.mode = 0644;
	sy65153_sysfs->attr_sy65153_reg_val.show = sy65153_register_value_show;
	sy65153_sysfs->attr_sy65153_reg_val.store = sy65153_register_value_store;

	ret = sysfs_create_group(&info->wl_psy->dev.kobj, &sy65153_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static void sy65153_wl_charger_intb_int_work(struct work_struct *work)
{
	struct sy65153_wl_charger_info *info =
		container_of(work, struct sy65153_wl_charger_info, wireless_intb_int_work.work);
	bool is_thm_shtdn, is_vrect_ovp, is_current_ocp, is_vout_ready;
	u32 tdie, rx_ilim, batt_charge_status, ept_code;

	mutex_lock(&info->wireless_chg_intb_lock);
	sy65153_wl_charger_clear_interrupt(info, 1);

	is_thm_shtdn = sy65153_wl_charger_get_thermal_shtdn_status(info);
	is_vrect_ovp = sy65153_wl_charger_get_vrect_ovp_status(info);
	is_current_ocp = sy65153_wl_charger_get_current_limit_status(info);
	is_vout_ready = sy65153_wl_charger_get_vout_status(info);
	sy65153_wl_charger_get_rx_tdie(info, &tdie);
	sy65153_wl_charger_get_rx_ilim(info, &rx_ilim);
	sy65153_wl_charger_get_batt_charge_status(info, &batt_charge_status);
	sy65153_wl_charger_get_ept_code_status(info, &ept_code);

	dev_info(info->dev, "is_thm_shtdn:[%s]; is_vrect_ovp:[%s]; is_current_ocp:[%s]; "
		 "is_vout_ready:[%s]; Tdie:[%d]; rx_ilim:[%d]; batt_charge_status:[0x%x]; "
		 "ept_code:[0x%x]\n",
		 is_thm_shtdn ? "true" : "false", is_vrect_ovp ? "true" : "false",
		 is_current_ocp ? "true" : "false", is_vout_ready ? "true" : "false",
		 tdie, rx_ilim, batt_charge_status, ept_code);

	sy65153_wl_charger_dump_register(info);
	mutex_unlock(&info->wireless_chg_intb_lock);
}

static irqreturn_t sy65153_wl_charger_intb_int_handler(int irq, void *dev_id)
{
	struct sy65153_wl_charger_info *info = dev_id;

	dev_info(info->dev, "intb interrupt occurs\n");

	schedule_delayed_work(&info->wireless_intb_int_work, 0);

	return IRQ_HANDLED;
}

static void sy65153_wl_charger_pg_int_work(struct work_struct *work)
{
	struct sy65153_wl_charger_info *info =
		container_of(work, struct sy65153_wl_charger_info, wireless_pg_int_work.work);

	int pg_value, ret;
	u32 rpp_type = 0, vout_value = 0;
	bool need_renotify = false;

	mutex_lock(&info->wireless_chg_pg_lock);

	pg_value = gpio_get_value(info->power_good_gpio);

	if (pg_value) {
		rpp_type = sy65153_wl_charger_get_rx_rpp_type(info);

		vout_value = SY65153_VOUT_5V;
		if (rpp_type == SY65153_RPP_TYPE_EPP)
			vout_value = SY65153_VOUT_9V;

		ret = sy65153_wl_charger_set_vout(info, vout_value);
		if (ret)
			dev_err(info->dev, "%s:Fail to set vout, ret = %d\n", __func__, ret);

		if (gpio_is_valid(info->switch_chg_en_gpio))
			gpio_set_value(info->switch_chg_en_gpio, 0);

		gpio_set_value(info->switch_flag_en_gpio, 1);
		sy65153_wl_charger_set_rx_ilim(info, SY65153_ILIM_MAX);
		info->online = 1;

		if (rpp_type == SY65153_RPP_TYPE_UNKNOWN && !info->pg_retry_cnt) {
			info->pg_retry_cnt = 10;
		} else if (rpp_type != SY65153_RPP_TYPE_UNKNOWN) {
			info->pg_retry_cnt = 0;
			need_renotify = true;
		}
	} else {
		gpio_set_value(info->switch_flag_en_gpio, 0);
		info->pg_retry_cnt = 0;
		info->online = 0;
	}

	dev_info(info->dev, "%s, is_charging = %d , online = %d, rpp type =  0x%x, "
		 "pg_value = %d, vout_value = %d, pg_retry_cnt = %d\n",
		 __func__, info->is_charging, info->online, rpp_type, pg_value,
		 vout_value, info->pg_retry_cnt);

	mutex_unlock(&info->wireless_chg_pg_lock);

	if ((!info->is_charging && info->online) ||
	    (info->is_charging && !info->online)) {
		info->is_charging = !info->is_charging;
		cm_notify_event(info->wl_psy, CM_EVENT_WL_CHG_START_STOP, NULL);
	} else if (info->is_charging && info->online && need_renotify) {
		cm_notify_event(info->wl_psy, CM_EVENT_WL_CHG_START_STOP, NULL);
	}

	if (info->pg_retry_cnt > 1) {
		info->pg_retry_cnt--;
		schedule_delayed_work(&info->wireless_pg_int_work, SY65153_PG_INT_WORK_MS);
	}
}

static irqreturn_t sy65153_wl_charger_pg_int_handler(int irq, void *dev_id)
{
	struct sy65153_wl_charger_info *info = dev_id;

	dev_info(info->dev, "power good interrupt occurs\n");

	pm_wakeup_event(info->dev, SY65153_WAKE_UP_MS);
	schedule_delayed_work(&info->wireless_pg_int_work, 0);

	return IRQ_HANDLED;
}

static void sy65153_detect_pg_int(struct sy65153_wl_charger_info *info)
{
	if (info->pg_irq)
		sy65153_wl_charger_pg_int_handler(info->pg_irq, info);
}

static int sy65153_wl_charger_parse_dt(struct sy65153_wl_charger_info *info)
{
	struct device_node *node = info->dev->of_node;
	int ret;

	if (!node) {
		dev_err(info->dev, "[SY65153] [%s] No DT data Failing Probe\n", __func__);
		return -EINVAL;
	}

	info->irq_gpio = of_get_named_gpio(node, "intb,irq_gpio", 0);
	if (!gpio_is_valid(info->irq_gpio)) {
		dev_err(info->dev, "[SY65153] [%s] fail_irq_gpio %d\n",
			__func__, info->irq_gpio);
		return -EINVAL;
	}

	info->switch_chg_en_gpio  = of_get_named_gpio(node, "switch_chg_en_gpio", 0);
	if (!gpio_is_valid(info->switch_chg_en_gpio))
		dev_err(info->dev, "[SY65153] [%s] fail_switch_chg_en_gpio %d\n",
			__func__, info->switch_chg_en_gpio);

	info->switch_flag_en_gpio  = of_get_named_gpio(node, "switch_flag_en_gpio", 0);
	if (!gpio_is_valid(info->switch_flag_en_gpio)) {
		dev_err(info->dev, "[SY65153] [%s] fail_switch_flag_en_gpio %d\n",
			__func__, info->switch_flag_en_gpio);
		ret = -EINVAL;
		goto err_switch_chg_en_gpio;
	}

	info->power_good_gpio  = of_get_named_gpio(node, "pg,irq_gpio", 0);
	if (!gpio_is_valid(info->power_good_gpio)) {
		dev_err(info->dev, "[SY65153] [%s] fail_switch_power_good_gpio %d\n",
			__func__, info->power_good_gpio);
		ret = -EINVAL;
		goto err_switch_flag_en_gpio;
	}

	return 0;

err_switch_flag_en_gpio:
	gpio_free(info->switch_flag_en_gpio);
err_switch_chg_en_gpio:
	if (gpio_is_valid(info->switch_chg_en_gpio))
		gpio_free(info->switch_chg_en_gpio);

	gpio_free(info->irq_gpio);

	return ret;
}

static int sy65153_wl_charger_init_gpio(struct sy65153_wl_charger_info *info)
{
	int ret = 0;

	ret = devm_gpio_request_one(info->dev, info->irq_gpio,
				    GPIOF_DIR_IN, "sy65153_wl_charger_intb_int");
	info->client->irq = gpio_to_irq(info->irq_gpio);
	if (info->client->irq < 0) {
		dev_err(info->dev, "[SY65153] [%s] gpio_to_irq Fail!\n", __func__);
		ret = -EINVAL;
		goto err_irq_gpio;
	}

	if (gpio_is_valid(info->switch_chg_en_gpio)) {
		ret = devm_gpio_request_one(info->dev,  info->switch_chg_en_gpio,
					    GPIOF_DIR_OUT | GPIOF_INIT_LOW, "switch_chg_en_gpio");
		if (ret) {
			dev_err(info->dev, "init switch chg en gpio fail\n");
			ret = -EINVAL;
			goto err_switch_chg_en_gpio;
		}
	}

	ret = devm_gpio_request_one(info->dev,  info->switch_flag_en_gpio,
				    GPIOF_OUT_INIT_HIGH, "switch_flag_en_gpio");
	if (ret) {
		ret = -EINVAL;
		goto err_switch_flag_en_gpio;
	}

	ret = devm_gpio_request_one(info->dev, info->power_good_gpio,
				    GPIOF_DIR_IN, "sy65153_wl_charger_pg_int");
	info->pg_irq = gpio_to_irq(info->power_good_gpio);
	if (info->pg_irq < 0) {
		dev_err(info->dev, "[SY65153] [%s] pg_irq Fail!\n", __func__);
		ret = -EINVAL;
		goto err_power_good_gpio;
	}

	if (gpio_is_valid(info->switch_chg_en_gpio))
		gpio_set_value(info->switch_chg_en_gpio, 0);

	gpio_set_value(info->switch_flag_en_gpio, 0);

	return 0;

err_power_good_gpio:
	gpio_free(info->power_good_gpio);
err_switch_flag_en_gpio:
	gpio_free(info->switch_flag_en_gpio);
err_switch_chg_en_gpio:
	if (gpio_is_valid(info->switch_chg_en_gpio))
		gpio_free(info->switch_chg_en_gpio);
err_irq_gpio:
	gpio_free(info->irq_gpio);

	return ret;
}

static int sy65153_wl_charger_init_interrupt(struct sy65153_wl_charger_info *info)
{
	int ret;

	if (!info->client->irq || !info->pg_irq) {
		dev_err(info->dev, "Failed to init interrupt, irq = %d, pg_irg = %d\n",
			info->client->irq, info->pg_irq);
		return -EINVAL;
	}

	ret = devm_request_threaded_irq(&info->client->dev, info->client->irq, NULL,
					sy65153_wl_charger_intb_int_handler,
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"sy65153_wl_charger_int_irq", info);
	if (ret) {
		dev_err(info->dev, "Failed intb irq = %d ret = %d\n", info->client->irq, ret);
		return ret;
	}

	enable_irq_wake(info->client->irq);

	ret = devm_request_threaded_irq(&info->client->dev, info->pg_irq, NULL,
					sy65153_wl_charger_pg_int_handler,
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"sy65153_wl_charger_pg_int_irq", info);
	if (ret) {
		dev_err(info->dev, "Failed pg irq = %d ret = %d\n", info->pg_irq, ret);
		return ret;
	}

	enable_irq_wake(info->pg_irq);

	return ret;
}

static int sy65153_wl_charger_hw_init(struct sy65153_wl_charger_info *info)
{
	int ret;

	ret = sy65153_wl_charger_get_part_number(info);
	if (ret)
		dev_err(info->dev, "fail to get part_number, ret = %d\n", ret);

	ret = sy65153_wl_charger_get_fw_major_revision(info);
	if (ret)
		dev_err(info->dev, "fail to get fw major revision, ret = %d\n", ret);

	ret = sy65153_wl_charger_get_fw_minor_revision(info);
	if (ret)
		dev_err(info->dev, "fail to get fw minor revision, ret = %d\n", ret);

	ret = sy65153_wl_charger_parse_dt(info);
	if (ret) {
		dev_err(info->dev, "%s, failed to obtain dts parameters, ret = %d\n",
			__func__, ret);
		return ret;
	}

	ret = sy65153_wl_charger_init_gpio(info);
	if (ret) {
		dev_err(info->dev, "%s, failed to init gpio, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = sy65153_wl_charger_init_interrupt(info);

	return ret;
}

static int sy65153_wl_charger_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct power_supply_config wl_psy_cfg = {};
	struct device *dev = &client->dev;
	struct sy65153_wl_charger_info *info;
	int ret = 0;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "i2c allocated device info data failed!\n");
		return -ENOMEM;
	}

	info->regmap = regmap_init_i2c(client, &sy65153_wl_charger_regmap_config);
	if (IS_ERR(info->regmap)) {
		dev_err(&client->dev, "i2c to regmap fail\n");
		return PTR_ERR(info->regmap);
	}

	info->client = client;
	info->dev = dev;

	device_init_wakeup(dev, true);
	i2c_set_clientdata(client, info);

	mutex_init(&info->wireless_chg_pg_lock);
	mutex_init(&info->wireless_chg_intb_lock);
	INIT_DELAYED_WORK(&info->wireless_pg_int_work, sy65153_wl_charger_pg_int_work);
	INIT_DELAYED_WORK(&info->wireless_intb_int_work, sy65153_wl_charger_intb_int_work);

	wl_psy_cfg.drv_data = info;
	wl_psy_cfg.of_node = dev->of_node;

	info->wl_psy = devm_power_supply_register(dev,
						  &sy65153_wl_charger_desc,
						  &wl_psy_cfg);
	if (IS_ERR(info->wl_psy)) {
		dev_err(dev, "Couldn't register wip psy rc=%ld\n", PTR_ERR(info->wl_psy));
		ret = PTR_ERR(info->wl_psy);
		goto err_mutex_lock;
	}

	device_init_wakeup(info->dev, true);
	ret = sy65153_wl_charger_hw_init(info);
	if (ret) {
		dev_err(info->dev, "%s, failed to init hardwave, ret = %d\n", __func__, ret);
		goto err_mutex_lock;
	}

	ret = sy65153_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto error_sysfs;
	}

	sy65153_detect_pg_int(info);

	dev_err(info->dev, "%s, probe successful!!!\n", __func__);
	return 0;

error_sysfs:
	sysfs_remove_group(&info->wl_psy->dev.kobj, &info->sysfs->attr_g);
	power_supply_unregister(info->wl_psy);
	gpio_free(info->power_good_gpio);
	gpio_free(info->switch_flag_en_gpio);
	if (gpio_is_valid(info->switch_chg_en_gpio))
		gpio_free(info->switch_chg_en_gpio);

	gpio_free(info->irq_gpio);
err_mutex_lock:
	mutex_destroy(&info->wireless_chg_pg_lock);
	mutex_destroy(&info->wireless_chg_intb_lock);
	regmap_exit(info->regmap);

	return ret;
}

static int sy65153_wl_charger_remove(struct i2c_client *client)
{
	struct sy65153_wl_charger_info *info = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&info->wireless_pg_int_work);
	cancel_delayed_work_sync(&info->wireless_intb_int_work);
	regmap_exit(info->regmap);

	return 0;
}

static const struct i2c_device_id sy65153_wl_charger_id[] = {
	{"wl_charger_sy65153", 0},
	{},
};

static const struct of_device_id  sy65153_wl_charger_match_table[] = {
	{ .compatible = "silergy,wl_charger_sy65153",},
	{}
};

MODULE_DEVICE_TABLE(of, sy65153_wl_charger_match_table);

static struct i2c_driver sy65153_wl_charger_driver = {
	.driver = {
		.name = "sy65153_wl_charger",
		.of_match_table = sy65153_wl_charger_match_table,
	},
	.probe = sy65153_wl_charger_probe,
	.remove = sy65153_wl_charger_remove,
	.id_table = sy65153_wl_charger_id,
};
module_i2c_driver(sy65153_wl_charger_driver);
MODULE_DESCRIPTION("Sylergy Wireless Power Charger Monitor driver");
MODULE_LICENSE("GPL v2");
