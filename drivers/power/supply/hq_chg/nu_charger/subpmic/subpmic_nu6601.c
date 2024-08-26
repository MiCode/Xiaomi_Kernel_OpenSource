/*
 * Copyright (C) 2022 Nuvolta Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>

#include "../../charger_class/hq_charger_class.h"
#include "../../charger_class/hq_led_class.h"
#include "inc/subpmic_nu6601.h"
#include "inc/nu6601_qc.h"

#define NU6601_TERM_CURR_OFFSET	50
#define NU6601_TERM_CURR_250	250

extern struct srcu_notifier_head g_nu6601_notifier;
enum nu6601_event {
	/*register event*/
	NU6601_EVENT_READ_TCPC_REG = 0,
	NU6601_EVENT_WRITE_TCPC_REG,
	
	/*cc event*/
	NU6601_EVENT_CC,

	/*com event*/
	NU6601_EVENT_DATA_INT,
	NU6601_EVENT_APEND_DATA,
	NU6601_EVENT_VER,
};

struct nu6601_evt_data {
	uint8_t reg;
	uint8_t value;
	
	bool is_drp;
};

static uint8_t g_devaddr;
static uint8_t g_regaddr;
static uint8_t g_writeval;
static int last_online = 0;

enum flashlight_type {
	FLASHLIGHT_TYPE_FLASH = 0,
	FLASHLIGHT_TYPE_TORCH,
};

enum flashlight_id {
	FLASHLIGHT_LED1 = 1,
	FLASHLIGHT_LED2,
	FLASHLIGHT_LED1_2,
};


#define NU6601_CHARGER_DRV_VERSION	"1.0.3_NVT_230613"

/* Charging usb type name */
const char *nu6601_chg_usb_type_name[] = {
	"NULL", "USB Host SDP", "USB CDP", "USB DCP", 
	"Reserverd", "Unknow Apapter", "Non-Standard Apapter"
};

/* Charger status name */
static const char *nu6601_chg_status_name[] = {
	"(0x0): NULL",
	"(0x1): VSYS Reset (triggered by nQON)",
	"(0x2): In Ship Mode",
	"(0x3): BFET Soft-Start",
	"(0x4): System On_Battery",
	"(0x5): Charger Buck Soft-Start 1",
	"(0x6): Charger Buck Soft-Start 2",
	"(0x7): System On_Charger",
	"(0x8): Charging Paused",
	"(0x9): Trickle Charging",
	"(0xA): Pre-Charging",
	"(0xB): Linear Fast Charging",
	"(0xC): CC (Constant-Current) Charging",
	"(0xD): CV (Constant-Voltage) Charging",
	"(0xE): Charge Termination",
	"(0xF): VSYS is OFF",
};

enum ADC_MODULE{
    ADC_VAC,
    ADC_VBUS,
    ADC_VPMID, //not support
    ADC_VSYS,
    ADC_VBAT,
    ADC_VBATPN,
    ADC_IBUS,
    ADC_IBAT,
    ADC_TDIE,
    ADC_TSBUS,
    ADC_TSBAT,
    ADC_BATID,
};

#define ADC_LSB(ch) ch##_ADC_LSB
static const u32 adc_step[] = {
	ADC_LSB(VAC),
	ADC_LSB(VBUS),
	ADC_LSB(VPMID),
	ADC_LSB(VSYS),
	ADC_LSB(VBAT),
	ADC_LSB(VBATPN),
	ADC_LSB(IBUS),
	ADC_LSB(IBAT),
	ADC_LSB(TDIE),
	ADC_LSB(TSBUS),
	ADC_LSB(TSBAT),
	ADC_LSB(BATID),
};

static const int prechg_volt[] = {
    2900, 3000, 3100, 3200,
};

static const int rechg_volt[] = {
    100, 200,  /* below VBAT_REG */
};

static const int wd_time[] = {
    0, 500, 1000, 2000, 30000, 40000, 50000, 60000,
    7000, 8000, 9000, 10000, 20000, 40000, 80000, 160000,
};

static const int temp_var[106][2] = {
	{-20, 919},{-19, 914},{-17, 904},{-16, 899},{-15, 893},{-14, 888},{-13, 882},{-12, 875},{-11, 869},{-10, 862},{-9, 855},
	{-8, 848},{-7, 840},{-6, 832},{-5, 824},{-4, 816},{-3, 808},{-2, 799},{-1, 790},{0, 781},{1, 771},{2, 761},
	{3, 751},{4, 741},{5, 731},{6, 721},{7, 710},{8, 699},{9, 688},{10, 677},{11, 665},{12, 654},{13, 642},
	{14, 631},{15, 619},{16, 607},{17, 595},{18, 584},{19, 572},{20, 559},{21, 548},{22, 536},{23, 524},{24, 512},
	{25, 500},{26, 488},{27, 477},{28, 465},{29, 453},{30, 442},{31, 431},{32, 420},{33, 409},{34, 398},{35, 387},
	{36, 377},{37, 366},{38, 356},{39, 346},{40, 336},{41, 327},{42, 317},{43, 308},{44, 299},{45, 290},{46, 282},
	{47, 273},{48, 265},{49, 257},{50, 249},{51, 242},{52, 234},{53, 227},{54, 220},{55, 213},{56, 207},{57, 200},
	{58, 194},{59, 188},{60, 182},{61, 176},{62, 171},{63, 165},{64, 160},{65, 155},{66, 150},{67, 145},{68, 141},
	{69, 136},{70, 132},{71, 128},{72, 124},{73, 120},{74, 116},{75, 112},{76, 109},{77, 105},{78, 102},{79, 99},
	{80, 96},{81, 93},{82, 90},{83, 87},{84, 84},{85, 82},
};

enum flashlight_mode {
	FLASHLIGHT_MODE_OFF = 0,
	FLASHLIGHT_MODE_TORCH,
	FLASHLIGHT_MODE_FLASH,
	FLASHLIGHT_MODE_MAX,
};

static void nu6601_enable_irq(struct nu6601_charger *chg, bool en);
static int nu6601_generate_qc35_pulses(struct soft_qc35 *qc, u8 pulses);
static int nu6601_set_qc_mode(struct soft_qc35 *qc, u8 mode);
static int nu6601_hvdcp_det_enable(struct charger_dev *subpmic_dev, int qc3_en);
static int nu6601_set_led_flash(struct nu6601_charger *chg, int index);

static int nu6601_reg_block_read(struct nu6601_charger *chg, int id, u8 reg,
                                u8 *val,size_t count)
{
    int ret = 0;
    ret = regmap_bulk_read(chg->rmap, reg | id << 8, val, count);
    if (ret < 0) {
        dev_err(chg->dev, "i2c block read failed\n");
    }
    return ret;
}

static int nu6601_reg_block_write(struct nu6601_charger *chg, int id, u8 reg,
                                u8 *val, size_t count)
{
    int ret = 0;
	int i = 0;
    ret = regmap_bulk_write(chg->rmap, reg | id << 8, val, count);
    if (ret < 0) {
        dev_err(chg->dev, "i2c block write failed\n");
        dev_err(chg->dev, "id=%d,reg=0x%x,count=%ld,", id, reg, count);
		for (i = 0; i < count; i++)
			dev_err(chg->dev, "value[%d]=0x%x ", i, val[i]);
		dev_err(chg->dev, "\n");
    }
    return ret;
}

static int nu6601_reg_write(struct nu6601_charger *chg, int id, u8 reg,
                                                    u8 val)
{
    u8 temp = val;
    return nu6601_reg_block_write(chg, id, reg, &temp, 1);
}

static int nu6601_reg_read(struct nu6601_charger *chg, int id, u8 reg,
                                                    u8 *val)
{
    return nu6601_reg_block_read(chg, id, reg, val, 1);
}

static int nu6601_reg_update_bits(struct nu6601_charger *chg, int id, u8 reg,
                                                    u8 mask, u8 val)
{
	u8 data;
	int ret;

	ret = nu6601_reg_block_read(chg, id, reg, &data, 1);
    if (ret < 0) {
        dev_err(chg->dev, "i2c update bits failed\n");
    }

	data &= ~mask;
	data |= val;

	return nu6601_reg_block_write(chg, id, reg, &data, 1);
}

static int nu6601_reg_set_bit(struct nu6601_charger *chg, int id, u8 addr,
		u8 mask)
{
	return nu6601_reg_update_bits(chg, id, addr, mask, mask);
}

static int nu6601_reg_clr_bit(struct nu6601_charger *chg, int id, u8 addr,
		u8 mask)
{
	return nu6601_reg_update_bits(chg, id, addr, mask, 0x00);
}

static int nu6601_charger_key(struct nu6601_charger *chg, bool en)
{	
	int ret;

	if (en) {
		ret = nu6601_reg_write(chg, ADC_I2C, 0xBA, 0xFF);
		ret |= nu6601_reg_write(chg, ADC_I2C, 0xBA, 0x78);
		ret |= nu6601_reg_write(chg, ADC_I2C, 0xBA, 0x87);
		ret |= nu6601_reg_write(chg, ADC_I2C, 0xBA, 0xAA);
		ret |= nu6601_reg_write(chg, ADC_I2C, 0xBA, 0x55);
	} else {
		ret = nu6601_reg_write(chg, ADC_I2C, 0xBA, 0xFF);
	}

	return ret;
}

/* interfaces that can be called by other module */
int nu6601_adc_start(struct nu6601_charger *chg, bool oneshot)
{
	int ret;

	/*adc mode*/
	if (oneshot) {
		ret = nu6601_reg_update_bits(chg, ADC_I2C, NU6601_REG_ADC_CTRL, 
				ADC_MODE_MASK | CONV_REQ_MASK, ADC_MODE_ONT_SHOT << ADC_MODE_SHIFT | CONV_REQ_START << CONV_REQ_SHIFT);
	} else
		ret = nu6601_reg_update_bits(chg, ADC_I2C, NU6601_REG_ADC_CTRL, 
				ADC_MODE_MASK, ADC_MODE_CONTINUE << ADC_MODE_SHIFT);

	/*adc en*/
	ret = nu6601_reg_update_bits(chg, ADC_I2C, NU6601_REG_ADC_CTRL,
			ADC_EN_MASK, ADC_ENABLE << ADC_EN_SHIFT);
	if (ret < 0) {
		dev_err(chg->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
		return ret;
	}

	return ret;
}

int nu6601_adc_stop(struct nu6601_charger *chg)
{
    if (!chg->state.online)
		return 0;
	return  nu6601_reg_update_bits(chg, ADC_I2C, NU6601_REG_ADC_CTRL,
			ADC_EN_MASK, ADC_ENABLE << ADC_EN_SHIFT);
}

static int nu6601_get_adc(struct nu6601_charger *chg,
                                    enum ADC_MODULE id)
{
	u8 val[2] = {0};
	int ret = 0;
	u32 reg = NU6601_REG_VAC_ADC1 + id * 2;
	int i = 0;

	ret = nu6601_reg_block_read(chg, ADC_I2C,
		reg, val, 2);
	if (ret < 0)
		return ret;

	ret = val[1] + (val[0] << 8);
	if (id == ADC_TDIE) {
		ret = ((ret*adc_step[id] / 1000 - 877)*1000)/7852 + 25;
	} else if (id == ADC_BATID) {
		ret = 10000 / (10000 / (ret * adc_step[id] * 12207 / 1000000) - 100);
	} else if (id == ADC_TSBAT || id == ADC_TSBUS) {
		ret = ret * adc_step[id] * 122 / 1000;
		for (i = 0; i < 105; i++) {
			if ((ret < temp_var[i][1]) && (ret >= temp_var[i+1][1])) {
				ret = temp_var[i+1][0];
				break;
			}
		}
	} else {
		ret *= adc_step[id];
	}

	return ret;
}

int nu6601_set_adc_func(struct nu6601_charger *chg, int adc_ch, bool en)
{
    int ret;
    u8 val[2] = {0};
    ret = nu6601_reg_block_read(chg, ADC_I2C, NU6601_REG_ADC_CH_EN, val, 2);
    if (ret < 0)
        return ret;
    val[0] = en ? val[0]|(adc_ch >> 8) : val[0] & ~(adc_ch >> 8);
    val[1] = en ? val[1]|adc_ch : val[1] & ~adc_ch;

	dev_err(chg->dev, "%s set adc func %x %x\n", __func__, val[0], val[1]);

    return nu6601_reg_block_write(chg, ADC_I2C, NU6601_REG_ADC_CH_EN, val, 2);
}

static int nu6601_set_vsys_ovp(struct nu6601_charger *chg, int mv)
{
    if (mv <= 4500)
        mv = 0;
    else if (mv <= 4600)
        mv = 1;
    else if (mv <= 4700)
        mv = 2;
    else if (mv <= 4800)
        mv = 3;
    else if (mv <= 4900)
        mv = 4;
    else if (mv <= 5000)
        mv = 5;
    else if (mv <= 5100)
        mv = 6;
	else {
		mv = 7;
	}

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_VSYS_OVP, 
			VSYS_OVP_THD_MASK, mv << VSYS_OVP_THD_SHIFT);
}

static int nu6601_set_vac_ovp(struct nu6601_charger *chg, int mv)
{
    if (mv <= 6500)
        mv = 0;
    else if (mv <= 11000)
        mv = 1;
    else if (mv <= 12000)
        mv = 2;
    else if (mv <= 13000)
        mv = 3;
    else if (mv <= 14000)
        mv = 4;
    else if (mv <= 15000)
        mv = 5;
    else if (mv <= 16000)
        mv = 6;
	else {
		mv = 7;
	}

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_VAC_OVP, 
			VAC_OVP_THD_MASK, mv << VAC_OVP_THD_SHIFT);
}

static int nu6601_set_vbus_ovp(struct nu6601_charger *chg, int mv)
{
    if (mv <= 6000)
        mv = 0;
    else if (mv <= 10000)
        mv = 1;
    else if (mv <= 12000)
        mv = 2;
    else {
        mv = 3;
	}

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_VBUS_OVP_CTRL, 
			VBUS_OVP_THD_MASK, mv << VBUS_OVP_THD_SHIFT);
}

static int _nu6601_get_vbus_type(struct nu6601_charger *chg)
{
	int ret;
	int type;
	uint8_t val = 0;

	ret = nu6601_reg_read(chg, DPDM_I2C, NU6601_REG_BC12_TYPE, &val);
	if (ret < 0)
		return ret;

	type = (val & BC12_TYPE_MASK) >> BC12_TYPE_SHIFT;

	if (type == BC12_TYPE_NONSTANDARD) {
		type += (val & UNSTANDARD_TYPE_MASK) >> UNSTANDARD_TYPE_SHIT;
	}

	pr_err("%s: bc1.2= %x\n", __func__, type);
	//pr_err("%s: bc1.2= %s\n", __func__, nu6601_chg_usb_type_name[type]);

	return type;
}

int _nu6601_get_chg_type(struct nu6601_charger *chg)
{
	int ret = 0;
	int type = 0;
	uint8_t val;

	ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_CHG_STATUS, &val);
	if (ret < 0)
		return ret;

	type = (val & CHG_FSM_MASK) >> CHG_FSM_SHIFT;
	pr_info("%s: charger status = %s\n", __func__, nu6601_chg_status_name[type]);

	return type;
}

static int nu6601_is_charge_en(struct nu6601_charger *chg)
{
	int ret;
	uint8_t val = 0;

	ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_CHG_CTRL, &val);
	if (ret < 0)
		return ret;

	return (val & CHG_EN_CMD_MASK) >> CHG_EN_CMD_SHIFT;
}

static int nu6601_enable_charger(struct nu6601_charger *chg)
{
	u8 val = CHG_ENABLE << CHG_EN_CMD_SHIFT;
	dev_err(chg->dev, "%s enable charger\n", __func__);

	return  nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_CHG_CTRL, 
			CHG_EN_CMD_MASK, val);
}

static int nu6601_disable_charger(struct nu6601_charger *chg)
{
	int ret;
	u8 val = CHG_DISABLE << CHG_EN_CMD_SHIFT;
	dev_err(chg->dev, "%s disable charger\n", __func__);

	ret = nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_CHG_CTRL,
			CHG_EN_CMD_MASK, val);
	return ret;
}

static int nu6601_cid_enable(struct nu6601_charger *chg, bool en)
{
	int ret;
	uint8_t CID_EN_register_value = 0;
	uint8_t BC12INT_MASK_value = 0;
	ret = nu6601_charger_key(chg, true);
	if(en) {
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_CID_EN,
		RID_CID_SEL_MASK | CID_EN_MASK, SEL_CID << RID_CID_SEL_SHIFT | CID_EN << CID_EN_SHIFT);
		ret |= nu6601_reg_clr_bit(chg, DPDM_I2C, NU6601_REG_BC12INT_MASK, RID_DETED_INT_MASK);
		dev_info(chg->dev, "cid enabled\n");
	} else {
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_CID_EN,
		RID_CID_SEL_MASK | CID_EN_MASK, SEL_RID << RID_CID_SEL_SHIFT | CID_DIS << CID_EN_SHIFT);
		ret |= nu6601_reg_set_bit(chg, DPDM_I2C, NU6601_REG_BC12INT_MASK, RID_DETED_INT_MASK);
		dev_info(chg->dev, "cid disabled\n");
	}
	ret |= nu6601_reg_read(chg, CHG_I2C, NU6601_REG_CID_EN, &CID_EN_register_value);
	ret |= nu6601_reg_read(chg, DPDM_I2C, NU6601_REG_BC12INT_MASK, &BC12INT_MASK_value);
	ret |= nu6601_charger_key(chg, false);
	dev_info(chg->dev, "CID_EN_register_value = 0x%x, BC12INT_MASK_value = 0x%x\n", CID_EN_register_value,BC12INT_MASK_value);
	return ret;
}

static int nu6601_hvdcp_det_enable(struct charger_dev *subpmic_dev, int qc3_en)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	int val;

	mdelay(100);
	if(subpmic_dev->m_pd_active != 0 && subpmic_dev->m_pd_active != 10) {
		dev_info(chg->dev, "%s m_pd_active is %d\n", __func__,subpmic_dev->m_pd_active);
		return 0;
	}
	dev_err(chg->dev, "%s enable HVDCP detect\n", __func__);

	chg->qc3_enable = qc3_en;
	val = QC_ENABLE << QC_EN_SHIFT | QC20_5V << QC_MODE_SHIFT; 

	return nu6601_reg_update_bits(chg, DPDM_I2C, NU6601_REG_QC_DPDM_CTRL, 
			QC_EN_MASK | QC_MODE_MASK, val);
}

static int nu6601_chg_qc3_vbus_puls(struct charger_dev *subpmic_dev, bool state, int count)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	int ret  = 0;
	int i = 0;

	for (i = 0; i < count; i++) {
		if (state)
			ret = nu6601_generate_qc35_pulses(chg->qc, DP_COT_PULSE);
		else
			ret = nu6601_generate_qc35_pulses(chg->qc, DM_COT_PULSE);
		mdelay(8);
	}
	return ret;
}

static int nu6601_chg_request_qc20(struct charger_dev *subpmic_dev, int mv)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	int ret  = 0;;

	dev_err(chg->dev, "vbus type %d, mv %d\n", chg->state.vbus_type, mv);

    if (chg->state.vbus_type < BC12_TYPE_HVDCP) {
        return -EIO;
    }

    if (mv == 5000) {
		ret = nu6601_set_qc_mode(chg->qc, QC20_5V);
	} else if (mv == 9000) {
		ret = nu6601_set_qc_mode(chg->qc, QC20_9V);
		msleep(50);
		ret |= nu6601_set_qc_mode(chg->qc, QC20_9V);
	} else if (mv == 12000) {
		ret = nu6601_set_qc_mode(chg->qc, QC20_5V);
		msleep(50);
		ret |= nu6601_set_qc_mode(chg->qc, QC20_12V);
	}

    return ret;
}

static int nu6601_bc12_det_enable(struct nu6601_charger *chg, bool en)
{
	int val;

#if 0
	int ret;
	u8 bc;
	ret = nu6601_reg_read(chg, DPDM_I2C, NU6601_REG_BC12_CTRL, &bc);
	if (ret < 0) {
		dev_err(chg->dev, "read bc12 reg failed :%d\n", ret);
		return ret;
	}

	if (((bc & BC12_EN_MASK) >> BC12_EN_SHIFT) == BC12_ENABLE) {
		dev_err(chg->dev, "%s bc12 already enable\n", __func__);
		return 0;
	}
	dev_err(chg->dev, "%s enable bc12 detect\n", __func__);
#endif
	val = QC_DISABLE << QC_EN_SHIFT;
	nu6601_reg_update_bits(chg, DPDM_I2C, NU6601_REG_QC_DPDM_CTRL, QC_EN_MASK | QC_MODE_MASK, val);
	if (en) {
		#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
		Charger_Detect_Init();
		mdelay(100);
		#endif
		val = BC12_ENABLE << BC12_EN_SHIFT;
	} else {
		val = BC12_DISABLE << BC12_EN_SHIFT;
	}

	return nu6601_reg_update_bits(chg, DPDM_I2C, NU6601_REG_BC12_CTRL,
			BC12_EN_MASK, val);
}

static int nu6601_qc_command(struct nu6601_charger *chg)
{
	u8 val = QC_COMMAND << QC_COMMAND_SHIFT;
	//dev_err(chg->dev, "%s enable qc pulse\n", __func__);

	return nu6601_reg_update_bits(chg, DPDM_I2C, NU6601_REG_QC_DPDM_CTRL, 
			QC_COMMAND_MASK, val);
}

static int nu6601_generate_qc35_pulses(struct soft_qc35 *qc, u8 pulses)
{
    struct nu6601_charger *chg = qc->private;
	int ret;
	u8 val = pulses << QC3P5_PULSE_SHIFT;
	//dev_err(chg->dev, "%s Generate qc35 pulse %d\n", __func__, pulses);

	ret = nu6601_reg_update_bits(chg, DPDM_I2C, NU6601_REG_QC_DPDM_CTRL, 
			QC3P5_PULSE_MASK, val);
	if (ret == 0)
		ret = nu6601_qc_command(chg);

	return ret;
}

static int nu6601_get_vbus(struct soft_qc35 *qc)
{
    struct nu6601_charger *chg = qc->private;
	return nu6601_get_adc(chg, ADC_VBUS) / 1000;
}

static int nu6601_set_qc_mode(struct soft_qc35 *qc, u8 mode)
{
    struct nu6601_charger *chg = qc->private;
	int ret;
	u8 val = mode << QC_MODE_SHIFT;
	dev_err(chg->dev, "%s set qc mode %d\n", __func__, mode);

	ret = nu6601_reg_update_bits(chg, DPDM_I2C, NU6601_REG_QC_DPDM_CTRL, 
			QC_MODE_MASK , val);
	return ret;
}

#if 0
int nu6601_get_vsysmin_volt(struct nu6601_charger *chg)
{
	int ret;
	u8 val;

	ret = nu6601_reg_block_read(chg, CHG_I2C,
		NU6601_REG_VSYS_MIN, &val, 1);
	if (ret < 0) {
		dev_err(chg->dev, "read vsys min failed :%d\n", ret);
		return ret;
	} else{
		val = (val & VSYS_MIN_MASK) >> VSYS_MIN_SHIFT;
		return (val > VSYS_MIN_LV1) ? (VSYS_MIN_BASE + VSYS_MIN_LV1_LSB * VSYS_MIN_LV1
				+ (val - VSYS_MIN_LV1) * VSYS_MIN_LV2_LSB) 
			: (VSYS_MIN_BASE + VSYS_MIN_LV1_LSB * VSYS_MIN_LV1);
	}
}

static int nu6601_set_vsysmin_volt(struct nu6601_charger *chg, int mV)
{
	u8 val;

	if (mV > (VSYS_MIN_BASE + VSYS_MIN_LV1_LSB * VSYS_MIN_LV1)) {
		val = VSYS_MIN_LV1 + (mV - (VSYS_MIN_BASE + VSYS_MIN_LV1_LSB * VSYS_MIN_LV1)) /
			VSYS_MIN_LV2_LSB;
	} else {
		val = (mV - VSYS_MIN_BASE) / VSYS_MIN_LV1_LSB;
	}

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_VSYS_MIN,
			VSYS_MIN_MASK, val << VSYS_MIN_SHIFT);
}

static int nu6601_set_vbat_track_volt(struct nu6601_charger *chg, int val)
{
	if (val > VBAT_TRACK_200MV) {
		val = VBAT_TRACK_200MV;
	}

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_VSYS_MIN,
			VBAT_TRACK_MASK, val << VBAT_TRACK_SHIFT);
}
#endif

static int nu6601_set_buck_fsw(struct nu6601_charger *chg, int val)
{
	pr_err("nu6601_set_buck_fsw val = %d hz", val);
	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_BUCK_CTRL, 
			BUCK_FSW_SEL_MASK, val << BUCK_FSW_SEL_SHIFT);
}

static int nu6601_get_buck_fsw(struct nu6601_charger *chg)
{
	uint8_t val;
	int ret;
	ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_BUCK_CTRL, &val);
	if (ret < 0) {
		dev_err(chg->dev, "read NU6601_REG_BUCK_CTRL failed :%d\n", ret);
		return ret;
	} else{
		return (val & BUCK_FSW_SEL_MASK) >> BUCK_FSW_SEL_SHIFT;
	}
}

static int nu6601_set_buck_hs_ipeak(struct nu6601_charger *chg, int mA)
{
	u8 ipeak;

	if (mA > BUCK_HS_IPEAK_MAX_8A)
		mA = BUCK_HS_IPEAK_MAX_8A;
	ipeak = (mA - BUCK_HS_IPEAK_BASE)/ BUCK_HS_IPEAK_LSB;

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_BUCK_CTRL,
			BUCK_HS_IPEAK_MASK, ipeak << BUCK_HS_IPEAK_SHIFT);
}

static int nu6601_set_boost_ls_ipeak(struct nu6601_charger *chg, int mA)
{
	u8 ipeak;

	if (mA > BOOST_LS_IPEAK_MAX_8A)
		mA = BOOST_LS_IPEAK_MAX_8A;
	ipeak = (mA - BOOST_LS_IPEAK_BASE)/ BOOST_LS_IPEAK_LSB;

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_BOOST_ILIM,
			BOOST_LS_IPEAK_MASK, ipeak << BOOST_LS_IPEAK_SHIFT);
}

static int _nu6601_get_charge_current(struct nu6601_charger *chg)
{
	u8 val;
	int ret;

	ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_ICHG_CC, &val);
	if (ret < 0) {
		dev_err(chg->dev, "read NU6601_REG_BUCK_CTRL failed :%d\n", ret);
		return ret;
	} else{
		return ((val & ICHG_CC_SEL_MASK) >> ICHG_CC_SEL_SHIFT) * ICHG_CC_SEL_LSB_MA;
	}
}


static int _nu6601_set_charge_current(struct nu6601_charger *chg, int mA)
{
	u8 ichg;
	u8 chg_en;

	if (mA == 0) {
		nu6601_disable_charger(chg);
	} else {
		chg_en = nu6601_is_charge_en(chg);
		if (chg_en != 1)
			nu6601_enable_charger(chg);
	} 

	if (mA > ICHG_CC_MAX_MA)
		mA = ICHG_CC_MAX_MA;
	ichg = mA / ICHG_CC_SEL_LSB_MA;
	dev_err(chg->dev, "_nu6601_set_charge_current  %d mA\n", mA);

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_ICHG_CC,
			ICHG_CC_SEL_MASK, ichg << ICHG_CC_SEL_SHIFT);
}

static int _nu6601_set_chargevoltage(struct nu6601_charger *chg, int mV)
{
	u8 val;
    if (mV < VBAT_MIN) {
        mV = VBAT_MIN;
	}
    if (mV > VBAT_MAX) {
        mV = VBAT_MAX;
	}

	val = (mV - VBAT_REG_SEL_FIXED_OFFSET) / VBAT_REG_SEL_LSB_MV;
	dev_err(chg->dev, "_nu6601_set_chargevoltage  %d mV\n", mV);

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_VBAT_REG, 
			VBAT_REG_SEL_MASK, val << VBAT_REG_SEL_SHIFT);
}

static int _nu6601_set_input_volt_limit(struct nu6601_charger *chg, int volt)
{
	u8 val = 0;

	if (volt < VINDPM_ABS_MIN)
		volt = VINDPM_ABS_MIN;
	else if (volt > VINDPM_ABS_MAX)
		volt = VINDPM_ABS_MAX;
	
	if (volt <= (VINDPM_ABS_OFFSET + (VINDPM_ABS_LSBL * VINDPM_ABS_INTER_VALUE))) {
		val = (volt - VINDPM_ABS_OFFSET) / VINDPM_ABS_LSBL;
	} else if (volt >= VINDPM_ABS_OFFSET1) {
		val = (volt - VINDPM_ABS_OFFSET1) / VINDPM_ABS_LSBH + VINDPM_ABS_INTER_VALUE + 1;
	} else {
		val = VINDPM_ABS_INTER_VALUE;
	}
	dev_err(chg->dev, "_nu6601_set_input_volt_limit  %d mV\n", volt);

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_VINDPM,
			VINDPM_ABS_MASK, val << VINDPM_ABS_SHIFT);
}

__maybe_unused
static int nu6601_set_vindpm_offset(struct nu6601_charger *chg, int offset)
{
	u8 val = 0;

	if (offset == 0)
		val = VINDPM_TRK_0MV;
	else if (offset <= 150)
		val = VINDPM_TRK_150MV;
	else if (offset <= 200)
		val = VINDPM_TRK_200MV;
	else
		val = VINDPM_TRK_250MV;

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_VINDPM,
			VINDPM_TRK_MASK, val << VINDPM_TRK_SHIFT);
}

static int nu6601_reset_chip(struct charger_dev *subpmic_dev)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	int ret = 0;
	u8 val = SW_RST_ALL << SW_RST_SHIFT;

	ret = nu6601_reg_update_bits(chg, ADC_I2C, NU6601_REG_SW_RST, SW_RST_MASK, val);
	return ret;
}

static int nu6601_enable_ico(struct nu6601_charger* chg, bool enable)
{
	u8 val = 0;
	int ret = 0;

	if (enable)
		val = ICO_ENABLE << ICO_EN_SHIFT;
	else
		val = ICO_DISABLE << ICO_EN_SHIFT;

	ret = nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_ICO_CTRL, ICO_EN_MASK, val);

	return ret;
}

static int nu6601_enable_rechg_dis(struct nu6601_charger* chg, bool enable)
{
	u8 val = 0;
	int ret = 0;

	if (enable)
		val = RECHG_DIS_DISABLE << RECHG_DIS_SHIFT;
	else
		val = RECHG_DIS_ENABLE << RECHG_DIS_SHIFT;

	ret = nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_RECHG_CFG, RECHG_DIS_MASK, val);

	return ret;
}

static int nu6601_set_rechg_volt(struct charger_dev *subpmic_dev, int mv)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
    int i = 0;
    if (mv < rechg_volt[0])
        mv = rechg_volt[0];
    if (mv > rechg_volt[ARRAY_SIZE(rechg_volt) - 1])
        mv = rechg_volt[ARRAY_SIZE(rechg_volt) - 1];

    for (i = 0; i < ARRAY_SIZE(rechg_volt); i++) {
        if (mv <= rechg_volt[i])
            break;
    }

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_RECHG_CFG,
			VBAT_RECHG_MASK, i << VBAT_RECHG_SHIFT);
}

int nu6601_fled_set_on(struct nu6601_charger *chg, int id,
					enum flashlight_type type)
{
	u8 ret = 0;
	u8 val = 0;
	if(id & FLASHLIGHT_LED1) {
		val |= LED1_EN_MASK;
	}
	if(id & FLASHLIGHT_LED2) {
		val |= LED2_EN_MASK;
	}
	if(id & FLASHLIGHT_LED1_2) {
		val |= LED1_EN_MASK;
		val |= LED2_EN_MASK;
	}
	mutex_lock(&chg->fled_lock);
	switch (type) {
	case FLASHLIGHT_TYPE_TORCH:
		val |= TORCH_EN_MASK;
		ret |= nu6601_reg_set_bit(chg, CHG_I2C, NU6601_REG_LED_CTRL, val);
		break;
	case FLASHLIGHT_TYPE_FLASH:
		val |= STROBE_EN_MASK;
		ret |= nu6601_reg_set_bit(chg, CHG_I2C, NU6601_REG_LED_CTRL, val);
		break;
	default :
		dev_err(chg->dev, "%s:%d unknow type:%d\n", __func__, __LINE__, type);
		break;
	}
	mutex_unlock(&chg->fled_lock);
	return ret;
}

int nu6601_fled_set_off(struct nu6601_charger *chg, int id,
					enum flashlight_type type)
{
	u8 ret = 0;
	u8 val = 0;
	if(id & FLASHLIGHT_LED1) {
		val |= LED1_EN_MASK;
	}
	if(id & FLASHLIGHT_LED2) {
		val |= LED2_EN_MASK;
	}
	if(id & FLASHLIGHT_LED1_2) {
		val |= LED1_EN_MASK;
		val |= LED2_EN_MASK;
	}
	mutex_lock(&chg->fled_lock);
	val |= TORCH_EN_MASK | STROBE_EN_MASK;
	ret |= nu6601_reg_clr_bit(chg, CHG_I2C,
			NU6601_REG_LED_CTRL, val);
	ret |= nu6601_reg_clr_bit(chg, CHG_I2C,
			NU6601_REG_LED_CTRL, LED_FUNCTION_EN_MASK);
	mutex_unlock(&chg->fled_lock);
	return ret;
}


static int nu6601_get_chg_status(struct charger_dev *subpmic_dev, uint32_t *chg_state, uint32_t *chg_status)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
    int type = 0;
    *chg_state = 0;
    *chg_status = 0;

	if (!chg->state.online) {
        *chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
        return 0;
    }

    type = _nu6601_get_chg_type(chg);
	switch (type) {
        case CC_CHARGING:
        case CV_CHARGING:
            *chg_state = POWER_SUPPLY_CHARGE_TYPE_FAST;
            *chg_status = POWER_SUPPLY_STATUS_CHARGING;
            break;
        case PRE_CHARGING:
        case TRICKLE_CHARGING:
            *chg_state = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
            *chg_status = POWER_SUPPLY_STATUS_CHARGING;
            break;
        case CHARGE_TERMINATION:
            *chg_status = POWER_SUPPLY_STATUS_FULL;
#if 0//TODO
            if (sc->qc_result == QC2_MODE && (sc->qc_vbus != 5000)) {
                subpmic_chg_write_byte(sc, SUBPMIC_REG_DPDM_QC_CTRL, 0x03);
                sc->qc_vbus = 5000;
            }
#endif
            break;
        case SYSTEM_ON_CHARGER:
            *chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
            break;
        default:break;
    }

	return 0;
}

static int nu6601_get_otg_status(struct charger_dev *subpmic_dev, bool *state)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	int ret;
    uint8_t boost = 0,qrb = 0;

	ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_BOOST_CTRL, &boost);
	if (ret < 0)
		return ret;

	ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_QRB_CTRL, &qrb);
	if (ret < 0)
		return ret;

	*state = (((boost & BOOST_EN_MASK) >> BOOST_EN_SHIFT) && ((qrb & QRB_OK_MASK) >> QRB_OK_SHIFT));

	return 0;
}

static int nu6601_get_boost_status(struct charger_dev *subpmic_dev)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	u8 ret;
	u8 boost = 0;

	ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_BOOST_CTRL, &boost);
	if (ret < 0) {
		dev_err(chg->dev, "read NU6601_REG_BOOST_CTRL failed :%d\n", ret);
		return ret;
	}

	return ((boost & BOOST_EN_MASK) >> BOOST_EN_SHIFT);
}

static int nu6601_get_vbus_type(struct charger_dev *subpmic_dev, enum vbus_type *vbus_type)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);

	switch (chg->state.vbus_type) {
    case BC12_TYPE_FLOAT:
        *vbus_type = VBUS_TYPE_FLOAT;
		break;
    case BC12_TYPE_SDP:
	if(chg->state.dcd_to == 1) {
		chg->state.dcd_to = 0;
		*vbus_type = VBUS_TYPE_FLOAT;
	} else
        *vbus_type = VBUS_TYPE_SDP;
		break;
    case BC12_TYPE_CDP:
        *vbus_type = VBUS_TYPE_CDP;
		break;
    case BC12_TYPE_DCP:
    case BC12_TYPE_SAMSUNG_5V2A:
        *vbus_type = VBUS_TYPE_DCP;
		break;
    case BC12_TYPE_HVDCP:
       *vbus_type = VBUS_TYPE_HVDCP;
		break;
    case BC12_TYPE_HVDCP_30:
        *vbus_type = VBUS_TYPE_HVDCP_3;
		break;
    case BC12_TYPE_HVDCP_3_PLUS_18:
    case BC12_TYPE_HVDCP_3_PLUS_27:
    case BC12_TYPE_HVDCP_3_PLUS_40:
        *vbus_type = VBUS_TYPE_HVDCP_3P5;
		break;
    case BC12_TYPE_APPLE_5V1A:
    case BC12_TYPE_APPLE_5V2A:
    case BC12_TYPE_APPLE_5V2_4A:
    case BC12_TYPE_OTHERS:
        *vbus_type = VBUS_TYPE_NON_STAND;
		break;
    default:
        *vbus_type = VBUS_TYPE_NONE;
    }

	//dev_err(chg->dev, "get vbus type :%d\n", *vbus_type);
	return 0;
}

static int nu6601_chg_get_adc(struct charger_dev *subpmic_dev, 
            enum sc_adc_channel channel, uint32_t *value)
{
    int ret = 0;
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);
    switch (channel) {
    case ADC_GET_VBUS:
		ret = nu6601_get_adc(chg, ADC_VBUS) / 1000;
        break;
    case ADC_GET_VSYS:
		ret = nu6601_get_adc(chg, ADC_VSYS) / 1000;
        break;
    case ADC_GET_VBAT:
		ret = nu6601_get_adc(chg, ADC_VBATPN) / 1000;
        break;
    case ADC_GET_VAC:
		ret = nu6601_get_adc(chg, ADC_VAC) / 1000;
        break;
    case ADC_GET_IBUS:
		ret = nu6601_get_adc(chg, ADC_IBUS) / 1000;
        break;
    case ADC_GET_IBAT:
		ret = nu6601_get_adc(chg, ADC_IBAT) / 1000;
        break;
    case ADC_GET_TSBUS:
        ret = nu6601_get_adc(chg, ADC_TSBUS);
        break;
    case ADC_GET_TSBAT:
        ret = nu6601_get_adc(chg, ADC_TSBAT);
        break;
    case ADC_GET_TDIE:
        ret = nu6601_get_adc(chg, ADC_TDIE);
        break;
    default:
        ret = -1;
        break;
    }
    if (ret < 0)
        return ret;

    *value = ret;

    return 0;
}

static int nu6601_get_online(struct charger_dev *subpmic_dev, bool *en)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
    *en = chg->state.online;

    return 0;
}

static int nu6601_read_vindpm_volt(struct charger_dev *subpmic_dev, uint32_t *mv)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	uint8_t val;
	int volt;
	int ret;
	ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_VINDPM, &val);
	if (ret < 0) {
		dev_err(chg->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		val = (val & VINDPM_ABS_MASK) >> VINDPM_ABS_SHIFT;
		if (val <= VINDPM_ABS_INTER_VALUE)
			volt = VINDPM_ABS_OFFSET + val * 
				VINDPM_ABS_LSBL;
		else 
			volt = VINDPM_ABS_OFFSET1 + (val -
					VINDPM_ABS_INTER_VALUE) * VINDPM_ABS_LSBH;

		*mv = volt;
		return 0;
	}
}

static int nu6601_get_input_current_limit(struct charger_dev *subpmic_dev, uint32_t *ma)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	u8 val;
	int ret;

	ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_IINDPM, &val);
	if (ret < 0) {
		dev_err(chg->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	}

	val = (val & IINDPM_SW_MASK) >> IINDPM_SW_SHIFT;

	*ma = val *IINDPM_SW_LSB + IINDPM_SW_OFFSET;

	return 0;
}

static int nu6601_is_charge_done(struct charger_dev *subpmic_dev, bool *done)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);
    *done =  _nu6601_get_chg_type(chg) == CHARGE_TERMINATION;

	return 0;
}

int nu6601_get_hiz_mode(struct charger_dev *subpmic_dev, bool *state)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	int ret;
	uint8_t val;

	ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_IINDPM, &val);
	if (ret)
		return ret;
	*state = (val & USB_SUSPEND_MASK) >> USB_SUSPEND_SHIFT;

	return 0;
}

static int _nu6601_set_input_current_limit(struct nu6601_charger *chg, int mA, int id)
{
	u8 val;
	int ret = 0;

    if (mA < IINDPM_MIN) mA = IINDPM_MIN;
    if (mA > IINDPM_MAX) mA = IINDPM_MAX;
	val = (mA - IINDPM_SW_OFFSET) / IINDPM_SW_LSB;
	dev_err(chg->dev, "%s icl=%d \n", __func__, mA);

	ret = nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_IINDPM,
			IINDPM_SW_MASK, val << IINDPM_SW_SHIFT);

	nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_IIN_FINAL,
			IIN_FINAL_OVRD_MASK, IIN_FINAL_OVRD << IIN_FINAL_OVRD_SHIFT);
	return ret;
}

static int nu6601_set_input_current_limit(struct charger_dev *subpmic_dev, int mA)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);

	return _nu6601_set_input_current_limit(chg, mA, 1);
}

static int nu6601_set_hiz_mode(struct charger_dev *subpmic_dev, bool en)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	u8 val = en ? USB_SUSPEND << USB_SUSPEND_SHIFT :
		USB_NORMAL << USB_SUSPEND_SHIFT;

	if (chg->input_suspend_status)
		return 0;

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_IINDPM, USB_SUSPEND_MASK, val);
}

static int nu6601_set_term_current(struct charger_dev *subpmic_dev, int curr)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	u8 iterm;

    if (curr < ITERM_MIN) {
        curr = ITERM_MIN;
	}
	if (curr == NU6601_TERM_CURR_250) {
		curr += NU6601_TERM_CURR_OFFSET;
	}
    if (curr > ITERM_MAX) {
        curr = ITERM_MAX;
	}

	iterm = (curr - ITERM_FIXED_OFFSET) / ITERM_LSB_MA;
	pr_err("nu6601_set_term_current cur = %d mA", curr);

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_CHG_TERM_CFG,
			ITERM_MASK, iterm << ITERM_SHIFT);
}

static int nu6601_chg_disable_power_path(struct charger_dev *charger, bool en)
{
	struct nu6601_charger *chg = charger_get_private(charger);
	u8 val = en ? USB_SUSPEND << USB_SUSPEND_SHIFT :
		USB_NORMAL << USB_SUSPEND_SHIFT;

	chg->input_suspend_status = en;

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_IINDPM, USB_SUSPEND_MASK, val);
}

__maybe_unused
static int subpmic_chg_set_shipmode(struct nu6601_charger *chg, bool en)
{
	int ret = 0;

	if (en) {
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_SHIP_MODE_CTRL, BATFET_DIS_DLY_MASK,
		BATFET_DIS_DLY << BATFET_DIS_DLY_SHIFT);
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_SHIP_MODE_CTRL, BATFET_DIS_MASK,
		BATFET_DIS << BATFET_DIS_SHIFT);
	}

	return ret;
}

static int nu6601_chg_set_shipmode(struct charger_dev *charger, bool en)
{
	struct nu6601_charger *chg = charger_get_private(charger);
	subpmic_chg_set_shipmode(chg, en);
	return 0;
}

static int nu6601_get_term_current(struct charger_dev *subpmic_dev, uint32_t *ma)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	u8 iterm;
	int ret;

	ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_CHG_TERM_CFG, &iterm);
	if (ret)
		return ret;
	*ma = ((iterm & ITERM_MASK) >> ITERM_SHIFT) * ITERM_LSB_MA + ITERM_FIXED_OFFSET;

	return 0;
}

static int nu6601_get_term_volt(struct charger_dev *subpmic_dev, uint32_t *mv)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	u8 volt;
	int ret;

	ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_VBAT_REG, &volt);
	if (ret)
		return ret;
	*mv = ((volt & VBAT_REG_SEL_MASK) >> VBAT_REG_SEL_SHIFT) * VBAT_REG_SEL_LSB_MV + 
		VBAT_REG_SEL_FIXED_OFFSET;
	return 0;
}

static int nu6601_enable_charging(struct charger_dev *subpmic_dev, bool en)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	int ret = 0;

	dev_err(chg->dev, "%s: en = %d\n", __func__, en);

	if (en) {
		ret  = nu6601_enable_charger(chg);
		if (ret < 0) {
			dev_err(chg->dev,
					"%s: set ichg fail\n", __func__);
			goto out;
		}
	} else {
		ret = nu6601_disable_charger(chg);
	}
out:
	return ret;
}

static int nu6601_get_charge_en(struct charger_dev *subpmic_dev)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	int ret = 0;

	ret = nu6601_is_charge_en(chg);

	return ret;
}

static int nu6601_enable_qrb(struct nu6601_charger *chg, bool en)
{
	u8 val = en ? QRB_AUTO_EN_ENABLE << QRB_AUTO_EN_SHIFT : 0;

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_QRB_CTRL, QRB_AUTO_EN_MASK, val);
}

static int nu6601_force_qrb_off(struct nu6601_charger *chg, bool en)
{
	u8 val = en ? QRB_FORCE_OFF << QRB_FORCE_SHIFT : 0;

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_QRB_CTRL, QRB_FORCE_MASK, val);
}

static int nu6601_enable_boost(struct nu6601_charger *chg, bool en)
{
	int ret;
	u8 cap;
	u8 val = en ? BOOST_ENABLE << BOOST_EN_SHIFT :
		BOOST_DISABLE << BOOST_EN_SHIFT;

	if (en) {
		nu6601_charger_key(chg, true);
		if (chg->revision == NU6601_REV_ID_A2) { 
			ret = nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_BUBO_DRV_CFG, 
					LS_DRV_MASK | HS_DRV_MASK, (HS_DRV_100P << HS_DRV_SHIFT) | (LS_DRV_25P << LS_DRV_SHIFT));
			if (ret < 0) {
				dev_err(chg->dev, "%s: write HS/LS fail !!!\n", __func__);
				nu6601_charger_key(chg, false);
				return ret;
			}

			ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_BUBO_DRV_CFG, &cap);
			if (ret < 0) {
				dev_err(chg->dev, "%s: read HS/LS fail !!!\n", __func__);
				nu6601_charger_key(chg, false);
				return ret;
			}
			if ((cap & (LS_DRV_MASK | HS_DRV_MASK)) != ((HS_DRV_100P << HS_DRV_SHIFT) | (LS_DRV_25P << LS_DRV_SHIFT))) {
				dev_err(chg->dev, "%s: error HS/LS !!!\n", __func__);
				nu6601_charger_key(chg, false);
				return ret;
			}
		}

		ret = nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_BOOST_CTRL, BOOST_EN_MASK, val);
		if (ret < 0) {
			nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_BUBO_DRV_CFG, 
					LS_DRV_MASK | HS_DRV_MASK, (HS_DRV_25P << HS_DRV_SHIFT) | (LS_DRV_100P << LS_DRV_SHIFT));
			nu6601_charger_key(chg, false);
			return ret;
		}

		if (chg->revision == NU6601_REV_ID_A2) { 
			nu6601_reg_write(chg, ADC_I2C, 0xB4, 0x15);
			nu6601_reg_write(chg, ADC_I2C, 0xB5, 0x30);
		}
		nu6601_charger_key(chg, false);

		return ret;
	} else {
		ret = nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_BOOST_CTRL, BOOST_EN_MASK, val);
		if (ret < 0) {
			return ret;
		}

		if (chg->revision == NU6601_REV_ID_A2) { 
			nu6601_charger_key(chg, true);
			nu6601_reg_write(chg, ADC_I2C, 0xB4, 0x15);
			nu6601_reg_write(chg, ADC_I2C, 0xB5, 0x00);
		}

		ret = nu6601_set_hiz_mode(chg->subpmic_dev, true);
		if (ret < 0) {
			nu6601_charger_key(chg, false);
			dev_err(chg->dev,
					"%s: nu6601_set_hiz_mode fail\n", __func__);
			return ret;
		}

		if (chg->revision == NU6601_REV_ID_A2) { 
			ret = nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_BUBO_DRV_CFG, 
					LS_DRV_MASK | HS_DRV_MASK, (HS_DRV_25P << HS_DRV_SHIFT) | (LS_DRV_100P << LS_DRV_SHIFT));
			nu6601_charger_key(chg, false);
		}

		nu6601_reg_set_bit(chg, CHG_I2C, NU6601_REG_BOOST_CTRL, BOOST_STOP_CLR_MASK); //for clear stop stat
		return ret;
	}
}

static int nu6601_set_otg(struct charger_dev *subpmic_dev, bool en, bool fqrb_off)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
    int ret;
    int cnt = 0;
    u8 boost_state;

	/*1. en = 1, usb suspend = 1 first*/
	if (en) {
		ret = nu6601_set_hiz_mode(subpmic_dev, en);
		if (ret < 0) {
			dev_err(chg->dev,
					"%s: nu6601_set_hiz_mode fail\n", __func__);
			return ret;
		}
	}

	/*2. led flash, QRB_FORCE_OFF*/
	if (fqrb_off) {
		ret = nu6601_force_qrb_off(chg, en);
		if (ret < 0) {
			dev_err(chg->dev,
					"%s: nu6601_force_qrb_off fail\n", __func__);
			nu6601_set_hiz_mode(subpmic_dev, false);
			return ret;
		}
	}

    do {
		boost_state = 0;
		ret = nu6601_enable_boost(chg, en ? true : false);
		if (ret < 0) {
			dev_err(chg->dev,
					"%s: nu6601_enable_boost fail\n", __func__);
			if (fqrb_off)
				nu6601_force_qrb_off(chg, false);
			nu6601_set_hiz_mode(subpmic_dev, false);
			return ret;
		}
		msleep(30);
		ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_BOOST_OP_STAT, &boost_state);
		if (cnt++ > 3) {
			dev_err(chg->dev,
					"%s: read NU6601_REG_BOOST_OP_STAT  fail\n", __func__);
			nu6601_force_qrb_off(chg, false);
			nu6601_set_hiz_mode(subpmic_dev, false);
			return -EIO;
		}
	} while (en != (!!(boost_state & (BOOST_GOOD << BOOST_FSM_SHIFT))));

	if (!en) {
		ret = nu6601_set_hiz_mode(subpmic_dev, en);
		if (ret < 0) {
			dev_err(chg->dev,
					"%s: nu6601_set_hiz_mode fail\n", __func__);
			return ret;
		}
	}

    dev_err(chg->dev, "otg set success");

    return 0;
}

static int nu6601_request_otg(struct nu6601_charger *chg, int index, bool en, bool fqrb_off)
{
    int ret = 0;
#if 0
    if (en)
        set_bit(index, &chg->request_otg);
    else
        clear_bit(index, &chg->request_otg);
#else
	chg->request_otg = !!en;
#endif

    dev_err(chg->dev, "now request_otg = %ld\n", chg->request_otg);

    /* ap requset otg, OTG_AUTO_EN = en*/
    if (index != 1 && en) {
		nu6601_reg_set_bit(chg, CHG_I2C, NU6601_REG_VAC_OVP, VAC_OVP_IGNORE_UV_MASK);
		ret = nu6601_enable_qrb(chg, true);
    } else if (index != 1 && !en){
		ret = nu6601_enable_qrb(chg, false);
		nu6601_reg_clr_bit(chg, CHG_I2C, NU6601_REG_VAC_OVP, VAC_OVP_IGNORE_UV_MASK);
    }

    if (!en && chg->request_otg) {
        return 0;
    }
    ret = nu6601_set_otg(chg->subpmic_dev, en, fqrb_off);
    if (ret < 0) {
#if 0
        if (en)
            clear_bit(index, &chg->request_otg);
        else
            set_bit(index, &chg->request_otg);
#else
	chg->request_otg = !!!en;
#endif
        return ret;
    }

    return 0;
}

static int nu6601_pd_request_otg(struct charger_dev *subpmic_dev, bool en)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
    return nu6601_request_otg(chg, 0, en, false);
}

int nu6601_set_otg_volt(struct charger_dev *subpmic_dev, int volt)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	u8 val = 0;

	if (volt < BOOST_VOUT_MIN)
		volt = BOOST_VOUT_MIN;
	if (volt > BOOST_VOUT_MAX)
		volt = BOOST_VOUT_MAX;

	val = ((volt - BOOST_VOUT_BASE) / BOOST_VOUT_LSB) << BOOST_VOUT_SHIFT;

	return nu6601_reg_update_bits(chg, CHG_I2C , NU6601_REG_BOOST_CTRL,
							   BOOST_VOUT_MASK, val);
}

int nu6601_set_otg_current(struct charger_dev *subpmic_dev, int mA)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	u8 temp;

	if (mA <= 500)
		temp = BOOST_IOUT_LIM_500MA;
	else if (mA > 500 && mA <= 1100)
		temp = BOOST_IOUT_LIM_1000MA;
	else if (mA > 1100 && mA <= 1600)
		temp = BOOST_IOUT_LIM_1500MA;
	else if (mA > 1500 && mA <= 2100)
		temp = BOOST_IOUT_LIM_2000MA;
	else if (mA > 2000 && mA <= 2600)
		temp = BOOST_IOUT_LIM_2500MA;
	else
		temp = BOOST_IOUT_LIM_3000MA;

	pr_err("nu6601_set_otg_current cur = %d mA", mA);
	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_BOOST_ILIM, 
			BOOST_IOUT_LIM_MASK, temp << BOOST_IOUT_LIM_SHIFT);
}


static int nu6601_set_input_volt_limit(struct charger_dev *subpmic_dev, int mV)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	
	return _nu6601_set_input_volt_limit(chg, mV);
}

static int nu6601_get_charge_current(struct charger_dev *subpmic_dev, uint32_t *mA)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);

	 *mA = _nu6601_get_charge_current(chg);

	return 0;
}

static int nu6601_set_charge_current(struct charger_dev *subpmic_dev, int mA)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	int ret;

	ret = _nu6601_set_charge_current(chg, mA);

	return ret;
}

static int nu6601_enable_term(struct charger_dev *subpmic_dev, bool en)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	u8 val = en ? TERM_ENABLE << TERM_EN_SHIFT :
		TERM_DISABLE << TERM_EN_SHIFT;

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_CHG_TERM_CFG, TERM_EN_MASK, val);
}

static int nu6601_set_chargevoltage(struct charger_dev *subpmic_dev, int mV)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);

	return _nu6601_set_chargevoltage(chg, mV);
}

static int nu6601_adc_enable(struct charger_dev *subpmic_dev, bool en)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	
	if (en) {
		return nu6601_adc_start(chg, false);
	} else {
		return nu6601_adc_stop(chg);
	}
}

static int nu6601_set_prechg_volt(struct charger_dev *subpmic_dev, int mv)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
    int i = 0;
    if (mv < prechg_volt[0])
        mv = prechg_volt[0];
    if (mv > prechg_volt[ARRAY_SIZE(prechg_volt) - 1])
        mv = prechg_volt[ARRAY_SIZE(prechg_volt) - 1];

    for (i = 0; i < ARRAY_SIZE(prechg_volt); i++) {
        if (mv <= prechg_volt[i])
            break;
    }

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_PRE_CHG_CFG,
			VPRE_THRESHOLD_MASK, i << VPRE_THRESHOLD_SHIFT);
}

static int nu6601_set_prechg_current(struct charger_dev *subpmic_dev, int mA)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	u8 iprechg;

	/* NU6601 pre chg max current limit is 500mA*/

	if (mA < IPRE_LIMIT_MIN)
		mA = IPRE_LIMIT_MIN;
	if (mA > IPRE_LIMIT_MAX)
		mA = IPRE_LIMIT_MAX;

	pr_err("nu6601_set_prechg_current cur = %d mA", mA);

	iprechg = mA / IPRE_LIMIT_LSB_MA;

	return nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_PRE_CHG_CFG,
			IPRE_LIMIT_MASK, iprechg << IPRE_LIMIT_SHIFT);
}

static int nu6601_force_dpdm(struct charger_dev *subpmic_dev)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
    return nu6601_bc12_det_enable(chg, true);
}

static int nu6601_request_dpdm(struct charger_dev *subpmic_dev, bool en)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	//todo
	int rc = 0;

	pr_err("%s: enable %d\n", __func__, en);

	/* fetch the DPDM regulator */
	if (!chg->dpdm_reg && of_get_property(chg->dev->of_node, "dpdm-supply", NULL)) {
		chg->dpdm_reg = devm_regulator_get(chg->dev, "dpdm");
		if (IS_ERR(chg->dpdm_reg)) {
			rc = PTR_ERR(chg->dpdm_reg);
			pr_err("%s: Couldn't get dpdm regulator rc=%d\n",
						__func__, rc);
		chg->dpdm_reg = NULL;
			return rc;
		}
	}

	mutex_lock(&chg->dpdm_lock);
	if (en) {
		if (chg->dpdm_reg && !chg->dpdm_enabled) {
			pr_err("%s: enabling DPDM regulator\n", __func__);
			rc = regulator_enable(chg->dpdm_reg);
			if (rc < 0)
				pr_err("%s: Couldn't enable dpdm regulator rc=%d\n",
						__func__, rc);
			else
				chg->dpdm_enabled = true;
		}
	} else {
		if (chg->dpdm_reg && chg->dpdm_enabled) {
			pr_err("%s: disabling DPDM regulator\n", __func__);
			rc = regulator_disable(chg->dpdm_reg);
			if (rc < 0)
				pr_err("%s: Couldn't disable dpdm regulator rc=%d\n",
						__func__, rc);
			else
				chg->dpdm_enabled = false;
		}
	}
	mutex_unlock(&chg->dpdm_lock);

	return rc;
}

static int nu6601_set_watchdog_timer(struct charger_dev *subpmic_dev, int ms)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
    int i = 0;

    if (ms < wd_time[0])
        ms = wd_time[0];
    if (ms > wd_time[ARRAY_SIZE(wd_time) - 1])
        ms = wd_time[ARRAY_SIZE(wd_time) - 1];
    for (i = 0; i < ARRAY_SIZE(wd_time); i++) {
        if (ms <= wd_time[i])
            break;
	}
    
	return nu6601_reg_update_bits(chg, ADC_I2C, NU6601_REG_I2C_RST,
			WD_TIMEOUT_MASK, i);
}

static int nu6601_reset_watchdog_timer(struct charger_dev *subpmic_dev)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	u8 val = WD_TIMER_RESET << WD_TIMER_RST_SHIFT;

	return nu6601_reg_update_bits(chg, ADC_I2C, NU6601_REG_I2C_RST,
			WD_TIMER_RST_MASK, val);
}

int nu6601_enable_qc35_detect(struct charger_dev *subpmic_dev)
{
    struct nu6601_charger *chg = charger_get_private(subpmic_dev);

	return qc35_detect_start(chg->qc);
}

 int nu6601_get_torch_max_lvl(struct charger_dev *subpmic_dev)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	return (chg->fled_cfg.torch_max_lvl * LED_TROCH_CURRENT_LSB + LED_TROCH_CURRENT_BASE) / 1000;
}

 int nu6601_get_strobe_max_lvl(struct charger_dev *subpmic_dev)
{
	struct nu6601_charger *chg = charger_get_private(subpmic_dev);
	return (chg->fled_cfg.strobe_max_lvl * LED_STROBE_CURRENT_LSB + LED_STROBE_CURRENT_BASE) / 1000;
}

static int nu6601_set_strobe_timeout(struct nu6601_charger *chg, int ms)
{
	u8 val = 0;
	u8 ret = 0;

	dev_err(chg->dev, "%s:%d timeout:%d\n", __func__, __LINE__, ms);

	if (ms < LED_STROBE_TIMEOUT_MIN)
		ms = LED_STROBE_TIMEOUT_MIN;
	else if (ms > LED_STROBE_TIMEOUT_MAX)
		ms = LED_STROBE_TIMEOUT_MAX;

	if (ms <= LED_STROBE_TIMEOUT_LEVEL1) {
		val = (ms - LED_STROBE_TIMEOUT_BASE) / LED_STROBE_TIMEOUT_LEVEL1_LSB;
	} else if (ms > LED_STROBE_TIMEOUT_LEVEL1) {
		val = (ms - LED_STROBE_TIMEOUT_LEVEL1) / LED_STROBE_TIMEOUT_LEVEL2_LSB + 9;
	} else {
		return -EINVAL;
	}

	ret = nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_LED_TIME_CFG_A,
		LED_STROBE_TIMEOUT_MASK, val);

	return ret;
}

 int nu6601_set_torch_lvl(struct nu6601_charger *chg, u8 id, int curr)
{
	int ret = 0;
	int lvl = 0;
	if(curr > LED_OFF) {
		ret = nu6601_reg_set_bit(chg, CHG_I2C, NU6601_REG_LED_CTRL,
				LED_FUNCTION_EN_MASK);
		if(ret < 0) {
			dev_err(chg->dev, "%s:%d set NU6601_REG_LED_CTRL ENABLE failed\n",
					__func__, __LINE__);
			return ret;
		}
	}

	if (curr < LED_OFF) {
		curr = LED_OFF;
	} else if(curr < (LED_TROCH_CURRENT_BASE/1000)) {
		curr = LED_TROCH_CURRENT_BASE/1000;
	}
	if (curr > LED_TROCH_CURRENT_MAX)
		curr = LED_TROCH_CURRENT_MAX;

	lvl = (curr * 1000 - LED_TROCH_CURRENT_BASE) / LED_TROCH_CURRENT_LSB;
	dev_err(chg->dev, "%s:%d set current id=%d curr=%d lvl=%d ret=%d\n",
				__func__, __LINE__, id, curr, lvl, ret);
	if (id & FLASHLIGHT_LED1) {
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_TLED1_BR,
			LED1_TORCH_CURRENT_MASK, lvl);
	}
	if (id & FLASHLIGHT_LED2) {
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_TLED2_BR,
				LED2_TORCH_CURRENT_MASK, lvl);
	}
	if (id & FLASHLIGHT_LED1_2) {
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_TLED1_BR,
			LED1_TORCH_CURRENT_MASK, lvl);
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_TLED2_BR,
				LED2_TORCH_CURRENT_MASK, lvl);
	}
	if (ret < 0) {
		dev_err(chg->dev, "%s:%d error: id=%d curr=%d lvl=%d ret=%d\n",
				__func__, __LINE__, id, curr, lvl, ret);
	}

	return ret;
}

/*
 *1. not support one in torch mode and another one in strobe mode at the same time
 *2. if want turn on dual flash mode ,please set id = 3,mode = FLASHLIGHT_MODE_FLASH
 *3. if want turn on dual torch mode ,please set id = 3,mode = FLASHLIGHT_MODE_TORCH
 *4. flash mode is not supported in troch mode, and you need to turn off troch mode
 *   mode first
 *5. FLASHLIGHT_MODE_FLASH or FLASHLIGHT_MODE_DUAL_FLASH mode need set otg_en first
 */
 int nu6601_set_strobe_lvl(struct nu6601_charger *chg, u8 id, int curr)
{
	u8 lvl = 0;
	u8 ret = 0;
	int now_vbus = 0;
	if(curr > LED_OFF) {
		ret = nu6601_reg_set_bit(chg, CHG_I2C, NU6601_REG_LED_CTRL,
				LED_FUNCTION_EN_MASK);
		if(ret < 0) {
			dev_err(chg->dev, "%s:%d set NU6601_REG_LED_CTRL ENABLE failed\n",
					__func__, __LINE__);
			return ret;
		}
	}

	if (curr < LED_OFF){
		curr = LED_OFF;
	} else if(curr < (LED_STROBE_CURRENT_BASE/1000)) {
		curr = LED_STROBE_CURRENT_BASE/1000;
	}
	if (curr > LED_STROBE_CURRENT_MAX)
		curr = LED_STROBE_CURRENT_MAX;

	lvl = (curr * 1000 - LED_STROBE_CURRENT_BASE) / LED_STROBE_CURRENT_LSB;
	dev_err(chg->dev, "%s:%d set current id=%d curr=%d lvl=%d ret=%d\n",
				__func__, __LINE__, id, curr, lvl, ret);
	if (lvl > chg->fled_cfg.strobe_max_lvl){
		lvl = chg->fled_cfg.strobe_max_lvl;
	}

	if (id & FLASHLIGHT_LED1) {
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_FLED1_BR,
				LED_STROBE_CURRENT_MASK, lvl);
	}
	if (id & FLASHLIGHT_LED2) {
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_FLED2_BR,
				LED_STROBE_CURRENT_MASK, lvl);
	}

	if (id & FLASHLIGHT_LED1_2) {
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_FLED1_BR,
				LED_STROBE_CURRENT_MASK, lvl);
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_FLED2_BR,
				LED_STROBE_CURRENT_MASK, lvl);
	}

	if (ret < 0){
		dev_err(chg->dev, "%s:%d error: id=%d curr=%d lvl=%d ret=%d\n",
				__func__, __LINE__, id, curr, lvl, ret);
		return ret;
	}

	if (down_timeout(&chg->led_semaphore, msecs_to_jiffies(800)) < 0) {
		dev_err(chg->dev, "%s:%d led_semaphore timeout\n",__func__, __LINE__);
		return -EBUSY;
	}

	ret = nu6601_get_boost_status(chg->subpmic_dev);

	if(curr > LED_OFF) {
		chg->in_otg = !!ret;
		if(chg->in_otg) {
			goto out;
		}
		//mask irq
		nu6601_enable_irq(chg, false);

		//get vbus volt
		now_vbus = nu6601_get_adc(chg, ADC_VBUS) / 1000;
		if (now_vbus > 5300)
			goto flash_enable_irq;

		ret = nu6601_request_otg(chg, 1, true, true);
		if (ret < 0) {
			goto flash_enable_irq;
		}
		goto out;
	}
	if (chg->in_otg) {
		goto out;
	}
	nu6601_request_otg(chg, 1, false, true);
flash_enable_irq:
	//unmask irq
	if(!chg->enable_irq) {
		nu6601_enable_irq(chg, true);
	}
out:
	up(&chg->led_semaphore);
	return ret;
}

__maybe_unused
static int subpmic_set_led_flash_curr(struct subpmic_led_dev *led_dev, enum SUBPMIC_LED_ID id, int ma)
{
    struct nu6601_charger *chg = subpmic_led_get_private(led_dev);
    return nu6601_set_strobe_lvl(chg, id, ma);
}

__maybe_unused
static int subpmic_set_led_flash_time(struct subpmic_led_dev *led_dev, enum SUBPMIC_LED_ID id, int ms)
{
    struct nu6601_charger *chg = subpmic_led_get_private(led_dev);
    return nu6601_set_strobe_timeout(chg, ms);
}

__maybe_unused
static int subpmic_set_led_flash_enable(struct subpmic_led_dev *led_dev, enum SUBPMIC_LED_ID id, bool en)
{
    struct nu6601_charger *chg = subpmic_led_get_private(led_dev);
	if (en) {
		return nu6601_set_led_flash(chg, id);
	} else {
		return 0;
	}
}

__maybe_unused
static int subpmic_set_led_torch_curr(struct subpmic_led_dev *led_dev, enum SUBPMIC_LED_ID id, int ma)
{
    struct nu6601_charger *chg = subpmic_led_get_private(led_dev);
	return nu6601_set_torch_lvl(chg, id, ma);
}

__maybe_unused
static int subpmic_set_led_torch_enable(struct subpmic_led_dev *led_dev, enum SUBPMIC_LED_ID id, bool en)
{
    struct nu6601_charger *chg = subpmic_led_get_private(led_dev);
	int ret;
	if (en) {
		ret = nu6601_fled_set_on(chg, id, FLASHLIGHT_TYPE_TORCH);
	} else {
		ret = nu6601_fled_set_off(chg, id, FLASHLIGHT_TYPE_TORCH);
	}

	return ret;
}

static void subpmic_chg_shutdown(struct platform_device *pdev)
{
	struct nu6601_charger *chg = platform_get_drvdata(pdev);
	nu6601_request_otg(chg, 0, false, false);
}

static struct charger_ops nu6601_ops = {
    .get_adc = nu6601_chg_get_adc,
	.get_vbus_type = nu6601_get_vbus_type,
	.get_online = nu6601_get_online,
	.is_charge_done = nu6601_is_charge_done,
	.get_hiz_status = nu6601_get_hiz_mode,
    .get_ichg = nu6601_get_charge_current,
	.get_input_volt_lmt = nu6601_read_vindpm_volt,
	.get_input_curr_lmt = nu6601_get_input_current_limit,
    .get_chg_status = nu6601_get_chg_status,
    .get_otg_status = nu6601_get_otg_status,
    .get_term_curr = nu6601_get_term_current,
    .get_term_volt = nu6601_get_term_volt,
    .set_hiz = nu6601_set_hiz_mode,
    .set_input_curr_lmt = nu6601_set_input_current_limit,
    .disable_power_path = nu6601_chg_disable_power_path,//TODO
    .set_input_volt_lmt = nu6601_set_input_volt_limit,

    .set_ichg = nu6601_set_charge_current,
    .set_chg = nu6601_enable_charging,
    .get_chg = nu6601_get_charge_en,
    .set_otg = nu6601_pd_request_otg,
    .set_otg_curr = nu6601_set_otg_current,
    .set_otg_volt = nu6601_set_otg_volt,
    .set_term = nu6601_enable_term,
    .set_term_curr = nu6601_set_term_current,
    .set_term_volt = nu6601_set_chargevoltage,
    .adc_enable = nu6601_adc_enable,
    .set_prechg_curr = nu6601_set_prechg_current,
    .set_prechg_volt = nu6601_set_prechg_volt,
    .force_dpdm = nu6601_force_dpdm,
    .reset = nu6601_reset_chip,
    .request_dpdm = nu6601_request_dpdm,
    .set_wd_timeout = nu6601_set_watchdog_timer,
    .kick_wd = nu6601_reset_watchdog_timer,
    .set_shipmode = nu6601_chg_set_shipmode,
    .set_rechg_vol = nu6601_set_rechg_volt,

	.qc_identify = nu6601_hvdcp_det_enable,
	.qc3_vbus_puls = nu6601_chg_qc3_vbus_puls,
	.qc2_vbus_mode = nu6601_chg_request_qc20,
};


static struct subpmic_led_ops nu6601_led_ops = {
    .set_led_flash_curr = subpmic_set_led_flash_curr,
    .set_led_flash_time = subpmic_set_led_flash_time,
    .set_led_flash_enable = subpmic_set_led_flash_enable,
    .set_led_torch_curr = subpmic_set_led_torch_curr,
    .set_led_torch_enable = subpmic_set_led_torch_enable,
};

static struct soft_qc35_ops qc35_ops = {
	.generate_pulses = nu6601_generate_qc35_pulses,
	.set_qc_mode = nu6601_set_qc_mode,
	.get_vbus = nu6601_get_vbus,
};

static int nu6601_dump_regs(struct nu6601_charger *chg, char *buf)
{
	int count = 0,reg = 0;
	u8 val = 0;

	nu6601_charger_key(chg, true);
	for (reg = 0x10; reg < 0x7f; reg++) {
		nu6601_reg_read(chg, CHG_I2C, reg, &val);
		if (buf)
			count = snprintf(buf + count, PAGE_SIZE - count,
					"[0x%x] -> 0x%x\n", reg, val);
		pr_err("nu6601 reg 0x33: [0x%x] -> 0x%x\n", reg, val);
	}
	nu6601_charger_key(chg, false);

	return count;
}

static void nu6601_dump_reg_workfunc(struct work_struct *work)
{
	struct nu6601_charger *chg = container_of(work, struct nu6601_charger, dump_reg_work.work);

    nu6601_dump_regs(chg, NULL);
    schedule_delayed_work(&chg->dump_reg_work, msecs_to_jiffies(10000));
}

static void nu6601_buck_fsw_workfunc(struct work_struct *work)
{
	struct nu6601_charger *chg = container_of(work, struct nu6601_charger, buck_fsw_work.work);
	int	vbat;
	int	vbus;
	int fsw;

	if ( _nu6601_get_chg_type(chg) == CHARGE_TERMINATION ) {
		dev_err(chg->dev, "%s  is_charge_done , stop fsw workfunc\n", __func__);
		return ;
	} else {
		vbat = nu6601_get_adc(chg, ADC_VBAT)/1000;
		vbus = nu6601_get_adc(chg, ADC_VBUS) / 1000;
		fsw = nu6601_get_buck_fsw(chg);
		dev_err(chg->dev, "%s  vbat %d mv , vbus %d mv, fsw %d\n", __func__, vbat, vbus, fsw);
		if ((chg->state.online == 1) &&
				(vbat > 4100 && ((vbus - vbat) < 550))) {
			if (fsw == BUCK_FSW_SEL_1000K) {
				nu6601_set_hiz_mode(chg->subpmic_dev, true);
				nu6601_set_buck_fsw(chg, BUCK_FSW_SEL_500K);
				nu6601_set_hiz_mode(chg->subpmic_dev, false);
			}
		} else if (chg->state.online == 1 && vbat < 4000) {
			if (fsw == BUCK_FSW_SEL_500K) {
				nu6601_set_hiz_mode(chg->subpmic_dev, true);
				nu6601_set_buck_fsw(chg, BUCK_FSW_SEL_1000K);
				nu6601_set_hiz_mode(chg->subpmic_dev, false);
			}
		}

		schedule_delayed_work(&chg->buck_fsw_work, msecs_to_jiffies(7000));
	}
}

/* N19A code for HQ-374473 by p-yanzelin at 2024/03/15 start */
static void nu6601_first_cid_det_workfunc(struct work_struct *work)
{
	struct nu6601_charger *chg = container_of(work,
			struct nu6601_charger, first_cid_det_work.work);
	uint8_t val, bc12_int_stat;

	dev_info(chg->dev, "%s\n", __func__);
	if (chg->chg_cfg.cid_en) {
		/* register 0xB0(BC1P2_INT_STAT) */
		/* bit4(RID_CID_DETED_INT_STAT) ==> 0: USB C plugged out, 1:USB C plugged in */
		nu6601_reg_read(chg, DPDM_I2C, 0xB0, &bc12_int_stat);
		if ((bc12_int_stat & 0x10) == 0x10) {
			val = 1;
			dev_info(chg->dev, "CID actived and USB plugged in");
		} else {
			val = 0;
			dev_info(chg->dev, "CID actived and USB plugged out");
		}
		srcu_notifier_call_chain(&g_nu6601_notifier, NU6601_EVENT_CC, &val);
	}
}
/* N19A code for HQ-374473 by p-yanzelin at 2024/03/15 end */

static void nu6601_update_state(struct nu6601_charger *chg)
{
	int ret;
	uint8_t val;

	ret = nu6601_reg_read(chg, CHG_I2C, NU6601_REG_VBUS_DET_STATUS, &val);
	if (ret < 0) {
		dev_err(chg->dev, "%s\n  read NU6601_REG_VBUS_DET_STATUS error ", __func__);
	} else {
		if ((val & VBUS_GD_MASK) == VBUS_GD_MASK) {
			chg->state.online = 1;
			chg->state.vbus_type = _nu6601_get_vbus_type(chg);
			if ((chg->state.vbus_type == BC12_TYPE_SDP) &&
					(chg->state.dcd_to == 1)) {
				chg->state.vbus_type = BC12_TYPE_FLOAT;
				chg->state.dcd_to = 0;
			}
		} else {
			chg->state.online = 0;
			chg->state.vbus_type = BC12_TYPE_NONE;
		}
	}
}

static irqreturn_t nu6601_usb_det_done_irq_handler(int irq, void *data)
{
#if 0
	struct nu6601_charger *chg =
	(struct nu6601_charger *)data;

	dev_err(chg->dev, "%s\n", __func__);
#endif
	return IRQ_HANDLED;
}

static irqreturn_t nu6601_vbus_0v_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;
	int fsw;

	nu6601_update_state(chg);
	if (chg->state.online == 0) {
		qc35_detect_stop(chg->qc);
		nu6601_bc12_det_enable(chg, false);
		chg->state.dcd_to = 0;
		charger_changed(chg->subpmic_dev);

		cancel_delayed_work_sync(&chg->buck_fsw_work);
		fsw = nu6601_get_buck_fsw(chg);
		if (fsw == BUCK_FSW_SEL_500K) {
			nu6601_set_hiz_mode(chg->subpmic_dev, true);
			nu6601_set_buck_fsw(chg, BUCK_FSW_SEL_1000K);
			nu6601_set_hiz_mode(chg->subpmic_dev, false);
		}
		nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_IIN_FINAL,
				IIN_FINAL_OVRD_MASK, 0);

		nu6601_enable_charger(chg);

	}
	last_online = 0;
	dev_err(chg->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t nu6601_vbus_gd_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;

	nu6601_update_state(chg);
	if (last_online == 0 && chg->state.online == 1) {
		nu6601_enable_charger(chg);
		chg->state.vbus_type = BC12_TYPE_NONE;
		schedule_delayed_work(&chg->buck_fsw_work, msecs_to_jiffies(100));
		charger_changed(chg->subpmic_dev);
	}
	last_online = 1;
	dev_err(chg->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t nu6601_chg_fsm_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;
	int chg_type;

	chg_type = _nu6601_get_chg_type(chg);

	return IRQ_HANDLED;
}

static irqreturn_t nu6601_chg_ok_irq_handler(int irq, void *data)
{
#if 0
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;

	dev_notice(chg->dev, "%s\n", __func__);
#endif

	return IRQ_HANDLED;
}

static irqreturn_t nu6601_dcd_timeout_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;
	if (chg->state.online == 1)
		chg->state.dcd_to = 1;

	dev_err(chg->dev, "%s\n", __func__);

	return IRQ_HANDLED;
}

static irqreturn_t nu6601_rid_cid_det_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;
	uint8_t val;
	uint8_t bc12_int_stat;
	dev_info(chg->dev, "%s\n", __func__);
	if (chg->chg_cfg.cid_en) {
	//register 0xB0(BC1P2_INT_STAT) bit4(RID_CID_DETED_INT_STAT) -> 0: USB C plugged out, 1:USB C plugged in
		nu6601_reg_read(chg, DPDM_I2C, 0xB0, &bc12_int_stat);
		if ((bc12_int_stat & 0x10) == 0x10) {
			val = 1;
			dev_info(chg->dev, "CID actived and USB plugged in");
		} else {
			val = 0;
			dev_info(chg->dev, "CID actived and USB plugged out");
		}
		srcu_notifier_call_chain(&g_nu6601_notifier, NU6601_EVENT_CC, &val);
	}
	return IRQ_HANDLED;
}

static irqreturn_t nu6601_bc1p2_det_done_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;
	dev_info(chg->dev, "%s\n", __func__);

	chg->state.vbus_type = _nu6601_get_vbus_type(chg);
	if ((chg->state.vbus_type == BC12_TYPE_SDP) &&
			(chg->state.dcd_to == 1)) {
		chg->state.vbus_type = BC12_TYPE_FLOAT;
		chg->state.dcd_to = 0;
	}
	charger_changed(chg->subpmic_dev);
	#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	Charger_Detect_Release();
	#endif

	return IRQ_HANDLED;
}

static irqreturn_t nu6601_hvdcp_det_ok_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;

	dev_err(chg->dev, "%s\n", __func__);

	chg->state.vbus_type = BC12_TYPE_HVDCP;

	mdelay(50);

	if (chg->qc3_enable)
		nu6601_enable_qc35_detect(chg->subpmic_dev);
	else {
		charger_changed(chg->subpmic_dev);
		nu6601_set_qc_mode(chg->qc, QC20_9V);
	}

	return IRQ_HANDLED;
}

static irqreturn_t nu6601_hvdcp_det_fail_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;

//	charger_changed(chg->subpmic_dev);

	dev_info(chg->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t nu6601_dp_16pluse_done_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;

	soft_qc35_update_dpdm_state(chg->qc, DP_16PLUSE_DONE);

	return IRQ_HANDLED;
}

static irqreturn_t nu6601_dm_16pluse_done_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;

	soft_qc35_update_dpdm_state(chg->qc, DM_16PLUSE_DONE);
	return IRQ_HANDLED;
}

static irqreturn_t nu6601_dpdm_3pluse_done_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;

	//dev_notice(chg->dev, "%s\n", __func__);
	soft_qc35_update_dpdm_state(chg->qc, DPDM_3PLUSE_DONE);
	return IRQ_HANDLED;
}

static irqreturn_t nu6601_dm_cot_pluse_done_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;

	dev_info(chg->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t nu6601_dp_cot_pluse_done_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;

	dev_info(chg->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t nu6601_dpdm_2pluse_done_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;

	soft_qc35_update_dpdm_state(chg->qc, DPDM_2PLUSE_DONE);
	return IRQ_HANDLED;
}

static irqreturn_t nu6601_boost_fail_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;
	u8 fault;

	chg->state.boost_good = 0;
	dev_notice(chg->dev, "%s\n", __func__);

	nu6601_reg_read(chg, CHG_I2C, 0x58, &fault);
	dev_notice(chg->dev, "fault flag : %x\n", fault);
	nu6601_reg_write(chg, CHG_I2C, 0x58, fault);
	nu6601_reg_read(chg, CHG_I2C, 0x59, &fault);
	dev_notice(chg->dev, "fault stat : %x\n", fault);

	nu6601_enable_qrb(chg, false);
    nu6601_force_qrb_off(chg, false);
	nu6601_enable_boost(chg, false);
	nu6601_set_hiz_mode(chg->subpmic_dev, false);

	return IRQ_HANDLED;
}

static irqreturn_t nu6601_boost_gd_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;

	nu6601_set_hiz_mode(chg->subpmic_dev, false);
	chg->state.boost_good = 1;
	return IRQ_HANDLED;
}

static irqreturn_t nu6601_vbat_ov_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;

	dev_notice(chg->dev, "%s\n", __func__);

	return IRQ_HANDLED;
}

static int soft_qc35_notify_cb(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct soft_qc35 *qc = data;
	struct nu6601_charger *chg = qc->private;
	dev_err(chg->dev, "qc35 detect : -> %ld\n", event);
	chg->state.vbus_type += event;

	//schedule_work
	charger_changed(chg->subpmic_dev);

	if (chg->state.vbus_type == BC12_TYPE_HVDCP) {
		msleep(50);
		nu6601_set_qc_mode(chg->qc, QC20_5V);
		msleep(50);
		nu6601_set_qc_mode(chg->qc, QC20_9V);
	}

	return NOTIFY_OK;
}

static irqreturn_t nu6601_led1_timeout_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;
	//int ret;

	complete(&chg->led_complete);
	return IRQ_HANDLED;
}

static irqreturn_t nu6601_led2_timeout_irq_handler(int irq, void *data)
{
	struct nu6601_charger *chg =
		(struct nu6601_charger *)data;
	//int ret;

	complete(&chg->led_complete);
	return IRQ_HANDLED;
}

static int nu6601_set_led_flash(struct nu6601_charger *chg, int index)
{
	int ret = 0;

	dev_err(chg->dev, "%s:%d index:%d\n", __func__, __LINE__, index);

	if (down_timeout(&chg->led_semaphore, msecs_to_jiffies(500)) < 0) {
		dev_err(chg->dev, "%s:%d led_semaphore timeout\n",__func__, __LINE__);
		return -EBUSY;
	}

	chg->led_index = index;
	schedule_delayed_work(&chg->led_work, msecs_to_jiffies(0));
	return ret;
}

static void nu6601_led_flash_done_workfunc(struct work_struct *work)
{
	struct nu6601_charger *chg = container_of(work,
			struct nu6601_charger, led_work.work);
	int ret = 0;
	int now_vbus = 0;
	u8 val = 0;
	bool otg_st;

	ret = nu6601_get_boost_status(chg->subpmic_dev);
	chg->in_otg = !!ret;
	if (!chg->in_otg) {

		//get vbus volt
		now_vbus = nu6601_get_adc(chg, ADC_VBUS) / 1000;
		if (now_vbus > 5300)
			goto err_vbus;

		ret = nu6601_request_otg(chg, 1, true, true);
		if (ret < 0) {
			goto err_set_otg;
		}
	}

	//open flash led
	ret = nu6601_fled_set_on(chg, chg->led_index, FLASHLIGHT_TYPE_FLASH);
	if (ret < 0) {
		if (chg->in_otg)
			goto out;
		else
			goto err_en_flash;
	}
	reinit_completion(&chg->led_complete);
	//wait_for_completion(&chg->led_complete);
	if (!wait_for_completion_timeout(&chg->led_complete,
				msecs_to_jiffies(LED_STROBE_TIMEOUT_LEVEL2))) {
		dev_err(chg->dev, "%s:%d led flash timeout failed", __func__, __LINE__);
	}
	nu6601_fled_set_off(chg, FLASHLIGHT_LED1_2, FLASHLIGHT_TYPE_FLASH);

err_en_flash:
	ret = nu6601_get_otg_status(chg->subpmic_dev, &otg_st);
	if ((chg->in_otg) && !otg_st)
		nu6601_request_otg(chg, 1, false, true);

err_vbus:
err_set_otg:
	//unmask irq
	msleep(300);
	if(!chg->enable_irq) {
		nu6601_reg_read(chg, CHG_I2C, NU6601_REG_CHGINT_FLAG, &val);//clear vbus_gd vbus_0v
		nu6601_enable_irq(chg, true);
	}
out:
	up(&chg->led_semaphore);
}

static struct nu6601_irq_desc nu6601_chg_irq_desc[] = {
	/* charger irq*/
	NU6601_IRQDESC(usb_det_done),
	NU6601_IRQDESC(vbus_0v),
	NU6601_IRQDESC(vbus_gd),
	NU6601_IRQDESC(chg_fsm),
	NU6601_IRQDESC(chg_ok),

	/* dpdm irq*/
	NU6601_IRQDESC(dcd_timeout),
	NU6601_IRQDESC(rid_cid_det),
	NU6601_IRQDESC(bc1p2_det_done),

	/* QC irq*/
	NU6601_IRQDESC(hvdcp_det_ok),
	NU6601_IRQDESC(hvdcp_det_fail),
	NU6601_IRQDESC(dp_16pluse_done),
	NU6601_IRQDESC(dm_16pluse_done),
	NU6601_IRQDESC(dpdm_3pluse_done),
	NU6601_IRQDESC(dpdm_2pluse_done),
	NU6601_IRQDESC(dp_cot_pluse_done),
	NU6601_IRQDESC(dm_cot_pluse_done),

	/* QC irq*/
	NU6601_IRQDESC(boost_fail),
	NU6601_IRQDESC(boost_gd),

	/* led irq*/
	NU6601_IRQDESC(led1_timeout),
	NU6601_IRQDESC(led2_timeout),

	/* BATIF*/
	NU6601_IRQDESC(vbat_ov),
};


static void nu6601_enable_irq(struct nu6601_charger *chg, bool en)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(nu6601_chg_irq_desc); i++) {
		if (!strcmp(nu6601_chg_irq_desc[i].name, "led1_timeout") ||
				!strcmp(nu6601_chg_irq_desc[i].name, "led2_timeout"))
			continue;

		(en ? enable_irq : disable_irq_nosync)(nu6601_chg_irq_desc[i].irq);
	}
	dev_err(chg->dev, "nu6601_enable_irq: en = %d\n", en);
	chg->enable_irq = en;
}

static int nu6601_charger_irq_register(struct platform_device *pdev)
{
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(nu6601_chg_irq_desc); i++) {
		if (!nu6601_chg_irq_desc[i].name)
			continue;
		ret = platform_get_irq_byname(pdev, nu6601_chg_irq_desc[i].name);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to get irq %s\n", nu6601_chg_irq_desc[i].name);
			return ret;
		}

		dev_err(&pdev->dev, "%s irq = %d\n", nu6601_chg_irq_desc[i].name, ret);
		nu6601_chg_irq_desc[i].irq = ret;
		ret = devm_request_threaded_irq(&pdev->dev, ret, NULL,
				nu6601_chg_irq_desc[i].irq_handler,
				IRQF_TRIGGER_FALLING,
				nu6601_chg_irq_desc[i].name,
				platform_get_drvdata(pdev));
		if (ret < 0) {
			dev_err(&pdev->dev, "request %s irq fail\n", nu6601_chg_irq_desc[i].name);
			return ret;
		}
	}

	return 0;
}

static int nu6601_charger_sw_workaround(struct nu6601_charger *chg)
{
	int ret = 0;

	ret = nu6601_charger_key(chg, true);
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x49, 0x40, 0x40);
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x50, 0x03, 0x01);
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x60, 0xFF, 0x24);
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x63, 0xFF, 0x39);
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x65, 0xFF, 0xC0);
	ret |= nu6601_reg_write(chg, CHG_I2C, 0x49, 0x3F);
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x3A, 0x40, 0x40);

	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x56, 0x03, 0x01);
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x5F, 0xFF, 0x5F);
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x61, 0xFF, 0x24);
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x6A, 0xFF, 0x3A);
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x5C, 0xFF, 0x44);
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x5D, 0xFF, 0x07);
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x5B, 0xFF, 0x0A);
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, 0x49, 0x40, 0x00);
	ret |= nu6601_charger_key(chg, false);

	return ret;
}

static int nu6601_charger_init_setting(struct nu6601_charger *chg)
{
	int ret = 0;

	/*common initialization*/
	//nu6601_disable_watchdog_timer(chg);
	//nu6601_exit_hiz_mode(chg);
	//ret = nu6601_reset_chip(chg);

	ret = nu6601_set_watchdog_timer(chg->subpmic_dev, WD_TIMEOUT_DISABLE);

	ret |= nu6601_set_vsys_ovp(chg, 5200);
	ret |= nu6601_set_vac_ovp(chg, 14000);
	ret |= nu6601_set_vbus_ovp(chg, 14000);

	ret |= nu6601_enable_term(chg->subpmic_dev, chg->chg_cfg.enable_term);

	ret |= nu6601_enable_rechg_dis(chg, chg->chg_cfg.rechg_dis);

	ret |= nu6601_set_rechg_volt(chg->subpmic_dev, rechg_volt[chg->chg_cfg.rechg_vol]);

	ret |= nu6601_enable_ico(chg, chg->chg_cfg.enable_ico);

	ret |= _nu6601_set_input_volt_limit(chg, 4600);

	ret |= nu6601_set_term_current(chg->subpmic_dev, chg->chg_cfg.term_current);

	/*ret |= _nu6601_set_chargevoltage(chg, chg->chg_cfg.charge_voltage);*/

	/*ret |= _nu6601_set_charge_current(chg, chg->chg_cfg.charge_current);*/

	ret |= nu6601_set_prechg_current(chg->subpmic_dev, chg->chg_cfg.precharge_current);

	//ret |= _nu6601_set_input_current_limit(chg, chg->chg_cfg.input_curr_limit, 1);

	ret |= nu6601_set_otg_volt(chg->subpmic_dev, chg->chg_cfg.otg_vol);

	ret |= nu6601_set_otg_current(chg->subpmic_dev, 2000);

	ret |= nu6601_adc_start(chg, false);

	ret |= nu6601_reg_update_bits(chg, ADC_I2C, NU6601_REG_I2C_RST, 
			PWR_KEY_LONG_RST_ENMASK,  PWR_KEY_LONG_RST_ENABLE << PWR_KEY_LONG_RST_SHIFT);

	ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_VBAT_REG, 
			BATSNS_EN_MASK,  chg->chg_cfg.batsns_en << BATSNS_EN_SHIFT);

	ret |= nu6601_reg_set_bit(chg, CHG_I2C, NU6601_REG_BOOST_CTRL, BOOST_STOP_CLR_MASK); //for clear stop stat
	ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_BOOST_PORT_CFG, 
			VBAT_LOW_MASK, VBAT_LOW_3V << VBAT_LOW_SHIFT); //set default vbat low 3V


	ret |= nu6601_reg_write(chg, CHG_I2C, NU6601_REG_SHIP_MODE_CTRL, 0x54);
	ret |= nu6601_reg_write(chg, ADC_I2C, NU6601_REG_ADC_CH_EN1, 0xF9);

	ret |= nu6601_bc12_det_enable(chg, false);

	ret |= nu6601_set_buck_hs_ipeak(chg, 8000);
	ret |= nu6601_set_boost_ls_ipeak(chg, 8000);
	ret |= nu6601_set_buck_fsw(chg, BUCK_FSW_SEL_1000K);
	if (chg->revision == NU6601_REV_ID_A2) {
		nu6601_charger_key(chg, true);
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_BSM_CFG3,
				BSM_SLOW_OFF_DGL_MASK, BSM_SLOW_OFF_DGL_10US << BSM_SLOW_OFF_DGL_SHIFT);
		//ret |= nu6601_reg_set_bit(chg, CHG_I2C, NU6601_REG_BFET_CFG, BFET_MAX_VGS_CLAMP_EN_MASK | BFET_MIN_VGS_CLAMP_EN_MASK);
		ret |= nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_BUBO_DRV_CFG,
				LS_DRV_MASK | HS_DRV_MASK, (HS_DRV_50P << HS_DRV_SHIFT) | (LS_DRV_50P << LS_DRV_SHIFT));
		nu6601_charger_key(chg, false);
	}

	ret |= nu6601_enable_charger(chg);
	if(chg->chg_cfg.cid_en == 1)
		nu6601_cid_enable(chg, true);

	return ret;
}

static inline int nu6601_charger_parse_dt(struct device *dev,
		struct nu6601_charger *chg)
{
	int ret;
	int val;
	struct device_node *np = dev->of_node;

	dev_info(chg->dev, "%s\n", __func__);

	chg->chg_cfg.enable_term = of_property_read_bool(np, "charger,enable-termination");
	chg->chg_cfg.enable_ico = of_property_read_bool(np, "charger,enable-ico");
	ret = of_property_read_u32(np, "charger,otg-vol", &chg->chg_cfg.otg_vol);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "charger,batsns-en", &chg->chg_cfg.batsns_en);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "charger,otg-current", &chg->chg_cfg.otg_current);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "charger,iindpm-disable", &chg->chg_cfg.iindpm_dis);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "charger,charge-voltage", &chg->chg_cfg.charge_voltage);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "charger,charge-current", &chg->chg_cfg.charge_current);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "charger,input-curr-limit", &chg->chg_cfg.input_curr_limit);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "charger,precharge-current", &chg->chg_cfg.precharge_current);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "charger,term-current", &chg->chg_cfg.term_current);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "charger,rechg-dis", &chg->chg_cfg.rechg_dis);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "charger,rechg-volt", &chg->chg_cfg.rechg_vol);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "charger,cid-en", &chg->chg_cfg.cid_en);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "torch_max_level", &val);
	if (ret < 0) {
		pr_err("%s use default torch cur\n", __func__);
		chg->fled_cfg.torch_max_lvl = 36;
	} else {
		chg->fled_cfg.torch_max_lvl = val;
	}

	ret = of_property_read_u32(np, "strobe_max_level", &val);
	if (ret) {
		chg->fled_cfg.strobe_max_lvl = 116;
		pr_err("%s use default strobe cur\n", __func__);
	} else {
		chg->fled_cfg.strobe_max_lvl = val;
	}

	ret = of_property_read_u32(np, "strobe_max_timeout", &val);
	if (ret) {
		pr_err("%s use default strobe timeout\n", __func__);
		chg->fled_cfg.strobe_max_timeout = LED_STROBE_TIMEOUT_MAX;
	} else {
		chg->fled_cfg.strobe_max_timeout = val;
	}

	return 0;
}

int nu6601_detect_device(struct nu6601_charger *chg)
{
	int ret;
	uint8_t val = 0;

	ret = nu6601_reg_read(chg, ADC_I2C, NU6601_REG_DEV_ID, &val);
	if (ret == 0) {
		chg->part_no = val;
	}

	ret = nu6601_reg_read(chg, ADC_I2C, NU6601_REG_REV_ID, &val);
	if (ret == 0) {
		chg->revision = val;
	}

	return ret;
}

static int get_parameters(char *buf, unsigned long *param, int num_of_par)
{
	int cnt = 0;
	char *token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token) {
			if (kstrtoul(token, 0, &param[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}

	return 0;
}

static ssize_t nu6601_ledtest_store_property(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct nu6601_charger *chg = dev_get_drvdata(dev);
	int ret;
	long int val;
	ret = get_parameters((char *)buf, &val, 1);
	if (ret < 0) {
		dev_err(dev, "get parameters fail\n");
		return -EINVAL;
	}
	switch (val) {
		case 1: /* enable otg */
			nu6601_request_otg(chg, 0, true, false);
			break;
		case 2: /* disenable otg */
			nu6601_request_otg(chg, 0, false, false);
			break;
		case 3:
			break;
		case 4:
			nu6601_set_torch_lvl(chg, FLASHLIGHT_LED1, 200);
			nu6601_fled_set_on(chg, FLASHLIGHT_LED1, FLASHLIGHT_TYPE_TORCH);
			break;
		case 5:
			nu6601_fled_set_off(chg, FLASHLIGHT_LED1, FLASHLIGHT_TYPE_TORCH);
			break;
		case 6:
			nu6601_set_torch_lvl(chg, FLASHLIGHT_LED2, 300);
			nu6601_fled_set_on(chg, FLASHLIGHT_LED2, FLASHLIGHT_TYPE_TORCH);
			break;
		case 7:
			nu6601_fled_set_off(chg, FLASHLIGHT_LED2, FLASHLIGHT_TYPE_TORCH);
			break;
		case 8:
			ret = nu6601_set_strobe_lvl(chg, FLASHLIGHT_LED1, 800); //800mA
			if (ret >= 0) {
				nu6601_set_strobe_timeout(chg, 300); //300ms
				nu6601_set_led_flash(chg, FLASHLIGHT_LED1);
			}
			break;
		case 9:
			ret = nu6601_set_strobe_lvl(chg, FLASHLIGHT_LED2, 1000);
			if (ret >= 0) {
				nu6601_set_strobe_timeout(chg, 300); //300ms
				nu6601_set_led_flash(chg, FLASHLIGHT_LED2);
			}
			break;
		case 10:
			nu6601_set_torch_lvl(chg, FLASHLIGHT_LED1, 150);
			nu6601_set_torch_lvl(chg, FLASHLIGHT_LED2, 150);
			nu6601_fled_set_on(chg, FLASHLIGHT_LED1_2, FLASHLIGHT_TYPE_TORCH);
			break;
		case 11:
			nu6601_fled_set_off(chg, FLASHLIGHT_LED1_2, FLASHLIGHT_TYPE_TORCH);
			break;
		case 12:
			nu6601_set_strobe_timeout(chg, 300); //300ms
			ret = nu6601_set_strobe_lvl(chg, FLASHLIGHT_LED1, 800);
			if (ret >= 0) {
				nu6601_set_strobe_lvl(chg, FLASHLIGHT_LED2, 800);
				nu6601_set_led_flash(chg, FLASHLIGHT_LED1_2);
			}
			break;

		default:
			break;
	}

	return count;
}

#ifdef CONFIG_ENABLE_SYSFS_DEBUG
static ssize_t nu6601_ledtest_show_property(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;
	ret = snprintf(buf, 256, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
			"1: otg enable", "2: otg disable", "3: Reserverd",
			"4: led1 torch en", "5: led1 torch disable",
			"6: led2 torch en", "7: led2 torch disable",
			"8: led1 flash en", "9: led2 flash en",
			"10: dual troch en", "11: dual torch disable",
			"12: dual flash en");
	return ret;
}
static DEVICE_ATTR(ledtest, 0660, nu6601_ledtest_show_property,
		nu6601_ledtest_store_property);

static ssize_t nu6601_cc_store_property(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct nu6601_charger *chg = dev_get_drvdata(dev);
	int ret;
	long int val;
	ret = get_parameters((char *)buf, &val, 1);
	if (ret < 0) {
		dev_err(dev, "get parameters fail\n");
		return -EINVAL;
	}

	_nu6601_set_charge_current(chg, val);

	return count;
}

static ssize_t nu6601_cc_show_property(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;
	ret = snprintf(buf, 256, "%s\n", "set cc current 0~4000mA");
	return ret;
}

static DEVICE_ATTR(cc, 0660, nu6601_cc_show_property,
		nu6601_cc_store_property);

static ssize_t nu6601_qctest_store_property(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct nu6601_charger *chg = dev_get_drvdata(dev);
	int ret;
	long int val;
	ret = get_parameters((char *)buf, &val, 1);
	if (ret < 0) {
		dev_err(dev, "get parameters fail\n");
		return -EINVAL;
	}
	switch (val) {
		case 1: /* enable qc2 */
			nu6601_hvdcp_det_enable(chg->subpmic_dev, 1);
			break;
		case 2: /* enable qc3 */
			nu6601_enable_qc35_detect(chg->subpmic_dev);
			break;
		default:
			break;
	}

	return count;
}

static ssize_t nu6601_qctest_show_property(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;
	ret = snprintf(buf, 256, "%s\n%s\n", "1: enable HVDCP", "2: enable QC3");
	return ret;
}

static DEVICE_ATTR(qctest, 0660, nu6601_qctest_show_property,
		nu6601_qctest_store_property);

static ssize_t shipping_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct nu6601_charger *chg = dev_get_drvdata(dev);
	int32_t tmp = 0;
	int ret = 0;

	if (kstrtoint(buf, 10, &tmp) < 0) {
		dev_notice(dev, "parsing number fail\n");
		return -EINVAL;
	}
	if (tmp != 6601)
		return -EINVAL;

	ret = nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_SHIP_MODE_CTRL, BATFET_DIS_MASK,  BATFET_DIS << BATFET_DIS_SHIFT);
	if (ret < 0) { 
		dev_notice(dev, "enter shipping mode failed\n");
		return ret; 
	} 

	dev_notice(dev, "enter shipping mode\n");

	return count;
}
static const DEVICE_ATTR_WO(shipping_mode);

static ssize_t nu6601_dump_reg_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct nu6601_charger *chg = dev_get_drvdata(dev);
	int ret;
	uint8_t val;

	if (g_devaddr == 0) {
		ret = snprintf(buf, 256, "%s\n%s\n%s\n", 
				"echo 0x30xx/0x31xx/0x32xx > dumpreg && cat dumpreg",
				"0x3010: devaddr=0x30, regaddr=0x10",
				"devaddr will be clear to 0 after cat");
	} else {
		nu6601_charger_key(chg, true);
		if (g_writeval != 0)
			nu6601_reg_write(chg, (g_devaddr - 0x30) + ADC_I2C, g_regaddr, g_writeval);
		nu6601_reg_read(chg, (g_devaddr - 0x30) + ADC_I2C, g_regaddr, &val);
		ret = snprintf(buf, 256, "[0x%02x]->[0x%02x]: 0x%02x\n", g_devaddr, g_regaddr, val);
		nu6601_charger_key(chg, false);
		g_devaddr = g_regaddr = g_writeval = 0;
	}
	return ret;
}

static ssize_t nu6601_dump_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct nu6601_charger *chg = dev_get_drvdata(dev);
	int ret;
	long int par;
	ret = get_parameters((char *)buf, &par, 1);
	if (ret < 0) {
		dev_err(dev, "get parameters fail\n");
		return -EINVAL;
	}

	if (par == 0xdddd) {
		schedule_delayed_work(&chg->dump_reg_work, msecs_to_jiffies(10000));
		return count;
	}

	if (par == 0xeeee) {
		cancel_delayed_work_sync(&chg->dump_reg_work);
		return count;
	}
	

	if (par > 0xffff) {
		g_devaddr = (par >> 16) & 0xff;
		g_regaddr = (par >> 8) & 0xff;
		g_writeval = par & 0xff;
	} else {
		g_devaddr = (par >> 8) & 0xff;
		g_regaddr = par & 0xff;
	}

	if ((g_devaddr < 0x30) || (g_devaddr > 0x32))
		return -EINVAL;

	return count;
}
static DEVICE_ATTR(dumpreg, 0660, nu6601_dump_reg_show,
		nu6601_dump_reg_store);

static ssize_t nu6601_tcpc_test_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;

	ret = snprintf(buf, 256, "%s\n%s\n%s\n", 
			"1 open tcpc data int, 0 close tcpc data int",
			"3 open append data, 2 close apend data",
			"5 set tcpc drp mode, 4 set tcpc sink only mode, 6 open cid func, 7 close cid func");
	return ret;
}

static ssize_t nu6601_tcpc_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	long int par;
	uint8_t val;
	struct nu6601_charger *chg = dev_get_drvdata(dev);

	ret = get_parameters((char *)buf, &par, 1);
	if (ret < 0) {
		dev_err(dev, "get parameters fail\n");
		return -EINVAL;
	}

#if 1
	switch(par) {
		case 0:
			val = 0;
			srcu_notifier_call_chain(&g_nu6601_notifier, NU6601_EVENT_DATA_INT, &val);
			break;
		case 1:
			val = 1;
			srcu_notifier_call_chain(&g_nu6601_notifier, NU6601_EVENT_DATA_INT, &val);
			break;
		case 2:
			val = 0;
			srcu_notifier_call_chain(&g_nu6601_notifier, NU6601_EVENT_APEND_DATA, &val);
			break;
		case 3:
			val = 1;
			srcu_notifier_call_chain(&g_nu6601_notifier, NU6601_EVENT_APEND_DATA, &val);
			break;

		case 4:
			val = 0;
			srcu_notifier_call_chain(&g_nu6601_notifier, NU6601_EVENT_CC, &val);
			break;
		case 5:
			val = 1;
			srcu_notifier_call_chain(&g_nu6601_notifier, NU6601_EVENT_CC, &val);
			break;
		case 6:
			chg->chg_cfg.cid_en = 1;
			nu6601_charger_key(chg, true);
			nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_CID_EN, 
					RID_CID_SEL_MASK | CID_EN_MASK, SEL_CID << RID_CID_SEL_SHIFT | CID_EN << CID_EN_SHIFT);
			nu6601_charger_key(chg, false);
			break;
		case 7:
			chg->chg_cfg.cid_en = 0;
			nu6601_charger_key(chg, true);
			nu6601_reg_update_bits(chg, CHG_I2C, NU6601_REG_CID_EN, 
					RID_CID_SEL_MASK | CID_EN_MASK, SEL_RID << RID_CID_SEL_SHIFT | CID_DIS << CID_EN_SHIFT);
			nu6601_charger_key(chg, false);
			break;
		default:
			val = 0;
			srcu_notifier_call_chain(&g_nu6601_notifier, par, &val);
			break;
	};

#endif

	return count;
}
static DEVICE_ATTR(tcpc_test, 0660, nu6601_tcpc_test_show,
		nu6601_tcpc_test_store);

static ssize_t nu6601_dump_adc(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct nu6601_charger *chg = dev_get_drvdata(dev);
	int ret;
	int chg_status;
	int usb_type;
	u32 vac = 0, vbus = 0, ibus = 0, vbat = 0, vsys = 0;
	u32 ibat = 0, tsbus = 0, tsbat = 0;
	u32 vbatpn = 0, tdie = 0, batid = 0;
	uint8_t revid, sn0, sn1, sn2, sn3;

	chg_status = _nu6601_get_chg_type(chg);
	usb_type = _nu6601_get_vbus_type(chg);

	vac = nu6601_get_adc(chg, ADC_VAC) / 1000;
	vbus = nu6601_get_adc(chg, ADC_VBUS) / 1000;
	ibus = nu6601_get_adc(chg, ADC_IBUS) / 1000;
	vbat = nu6601_get_adc(chg, ADC_VBAT) / 1000;
	vsys = nu6601_get_adc(chg, ADC_VSYS) / 1000;
	ibat = nu6601_get_adc(chg, ADC_IBAT) / 1000;
	vbatpn = nu6601_get_adc(chg, ADC_VBATPN) / 1000;
	tsbus = nu6601_get_adc(chg, ADC_TSBUS);
	tsbat = nu6601_get_adc(chg, ADC_TSBAT);
	batid = nu6601_get_adc(chg, ADC_BATID);
	tdie = nu6601_get_adc(chg, ADC_TDIE);

	nu6601_charger_key(chg, true);
	nu6601_reg_read(chg, ADC_I2C, 0xEB, &revid);
	nu6601_reg_read(chg, ADC_I2C, 0xEC, &sn0);
	nu6601_reg_read(chg, ADC_I2C, 0xED, &sn1);
	nu6601_reg_read(chg, ADC_I2C, 0xEE, &sn2);
	nu6601_reg_read(chg, ADC_I2C, 0xEF, &sn3);
	nu6601_charger_key(chg, false);

	ret = snprintf(buf, 256, "chg sta: %d usb type: %d\n vac = %dmV, vbus = %dmV, ibus = %dmA, vbat = %dmV, vsys = %dmV, ibat = %dmA, tsbus = %d `C, tsbat = %d `C, vbatpn = %dmV, batid = %dK, tdie = %d `C\n revid = 0x%x, sn0 = 0x%x, sn1 = 0x%x, sn2 = 0x%x, sn3 = 0x%x \n",
			chg_status, usb_type,
			vac, vbus, ibus, vbat, vsys, ibat, 
			tsbus, tsbat, vbatpn, batid, tdie,
			revid, sn0, sn1, sn2, sn3);

	return ret;
}
static DEVICE_ATTR(dumpadc, 0440, nu6601_dump_adc, NULL);

static void nu6601_sysfs_file_init(struct device *dev)
{
	device_create_file(dev, &dev_attr_cc);
	device_create_file(dev, &dev_attr_qctest);
	device_create_file(dev, &dev_attr_dumpadc);
	device_create_file(dev, &dev_attr_dumpreg);
	device_create_file(dev, &dev_attr_ledtest);
	device_create_file(dev, &dev_attr_shipping_mode);
	device_create_file(dev, &dev_attr_tcpc_test);
}
#endif

int brightness_set(void * private, enum flashlight_type type,
		int index, int brightness)
{
	struct nu6601_charger *chg = private;
	int ret = 0;
	dev_err(chg->dev, "%s:%d flash type:%d, index:%d  brightness:%d\n",
			__func__, __LINE__, type, index, brightness);

	switch(type) {
		case FLASHLIGHT_TYPE_FLASH:
			ret = nu6601_set_strobe_lvl(chg, index, brightness);
			break;
		case FLASHLIGHT_TYPE_TORCH:
			ret = nu6601_set_torch_lvl(chg, index, brightness);
			break;
		default:
			break;
	}
	return ret;
}

int strobe_set(void *private, enum flashlight_type type, int index, bool state)
{
	struct nu6601_charger *chg = private;
	int ret = 0;
	dev_err(chg->dev, "%s:%d flash type:%d, index:%d, state:%d\n",
			__func__, __LINE__, type, index, state);

	if(state) {
		if(FLASHLIGHT_TYPE_FLASH == type) {
			ret = nu6601_set_led_flash(chg, index);
		}
		else {
			ret = nu6601_fled_set_on(chg, index, type);
		}
	} else {
		if(FLASHLIGHT_TYPE_TORCH == type) {
			ret = nu6601_fled_set_off(chg, index, type);
		}
	}

	return ret;
}

static int nu6601_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	uint8_t val;
	struct nu6601_charger *chg;

	pr_err("%s: (%s)\n", __func__, NU6601_CHARGER_DRV_VERSION);

	chg= devm_kzalloc(&pdev->dev, sizeof(*chg), GFP_KERNEL);
	if (!chg) {
		dev_err(&pdev->dev, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	chg->rmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chg->rmap) {
		dev_err(&pdev->dev, "failed to get regmap\n");
		return -ENODEV;
	}

	chg->dev = &pdev->dev;
	platform_set_drvdata(pdev, chg);

	ret = nu6601_detect_device(chg);
	if (chg->part_no == NU6601_DEV_ID) {
		dev_err(chg->dev, "%s: charger device nu6601 detected, rev id 0x%d\n", __func__, chg->revision);
	} else {
		dev_err(chg->dev, "%s: no charger device found:%d\n", __func__, ret);
		return -ENODEV;
	}

	if (chg->revision == NU6601_REV_ID_A1) { 
		val = 1;
		srcu_notifier_call_chain(&g_nu6601_notifier, NU6601_EVENT_VER, &val);
	}

	/* Register charger device */
	chg->subpmic_dev = charger_register("primary_chg",
			chg->dev, &nu6601_ops, chg);
	if (IS_ERR_OR_NULL(chg->subpmic_dev)) {
		ret = PTR_ERR(chg->subpmic_dev);
		goto err_dev;
	}

	/* Register led device */
	chg->led_dev = subpmic_led_register("subpmic_led",
			chg->dev, &nu6601_led_ops, chg);
	if (IS_ERR_OR_NULL(chg->led_dev)) {
		ret = PTR_ERR(chg->led_dev);
		goto err_led_dev;
	}


	ret = nu6601_charger_parse_dt(&pdev->dev, chg);
	if (ret < 0) {
		dev_err(chg->dev, "%s: parse dts failed\n", __func__);
		goto err_dtb;
	}

	INIT_DELAYED_WORK(&chg->buck_fsw_work, nu6601_buck_fsw_workfunc);

	/* SW workaround */
	ret = nu6601_charger_sw_workaround(chg);
	if (ret < 0) {
		dev_err(chg->dev, "%s: software workaround failed\n",
				__func__);
		goto err_wa;
	}

	/* Do initial setting */
	ret = nu6601_charger_init_setting(chg);
	if (ret < 0) {
		dev_err(chg->dev, "%s: sw init failed\n", __func__);
		goto err_init;
	}

	ret = nu6601_charger_irq_register(pdev);
	if (ret < 0) {
		dev_err(chg->dev, "irq request failed\n");
		goto err_irq;
	}

	chg->qc = soft_qc35_register(chg, &qc35_ops);
	if (!chg->qc) {
		ret = PTR_ERR(chg->qc);
		goto err_qc;
	}

	chg->qc35_result_nb.notifier_call = soft_qc35_notify_cb;
	qc35_register_notifier(chg->qc, &chg->qc35_result_nb);

	mutex_init(&chg->dpdm_lock);
	mutex_init(&chg->fled_lock);
	INIT_DELAYED_WORK(&chg->led_work, nu6601_led_flash_done_workfunc);
	INIT_DELAYED_WORK(&chg->dump_reg_work, nu6601_dump_reg_workfunc);
	/* N19A code for HQ-374473 by p-yanzelin at 2024/03/15 start */
	INIT_DELAYED_WORK(&chg->first_cid_det_work, nu6601_first_cid_det_workfunc);
	schedule_delayed_work(&chg->first_cid_det_work, msecs_to_jiffies(1500));
	/* N19A code for HQ-374473 by p-yanzelin at 2024/03/15 end */

	sema_init(&chg->led_semaphore, 1);
	init_completion(&chg->led_complete);
	chg->request_otg = 0;

#ifdef CONFIG_ENABLE_SYSFS_DEBUG
	nu6601_sysfs_file_init(chg->dev);
#endif /* CONFIG_ENABLE_SYSFS_DEBUG */

    //schedule_delayed_work(&chg->dump_reg_work, msecs_to_jiffies(10000));

	nu6601_update_state(chg);
	charger_changed(chg->subpmic_dev);

	dev_info(&pdev->dev, "%s successfully\n", __func__);
	return 0;

err_qc:
err_wa:
err_init:
err_irq:
err_dtb:
	//subpmic_led_unregister(chg->led_dev);
err_led_dev:
	charger_unregister(chg->subpmic_dev);
err_dev:
	dev_info(&pdev->dev, "%s failed\n", __func__);
	return ret;
}

static int nu6601_charger_remove(struct platform_device *pdev)
{
	struct nu6601_charger *chg = platform_get_drvdata(pdev);

	if (chg) {
		dev_info(chg->dev, "%s successfully\n", __func__);
#ifdef CONFIG_ENABLE_SYSFS_DEBUG
		device_remove_file(chg->dev, &dev_attr_cc);
		device_remove_file(chg->dev, &dev_attr_qctest);
		device_remove_file(chg->dev, &dev_attr_dumpadc);
		device_remove_file(chg->dev, &dev_attr_dumpreg);
		device_remove_file(chg->dev, &dev_attr_ledtest);
		device_remove_file(chg->dev, &dev_attr_shipping_mode);
		device_remove_file(chg->dev, &dev_attr_tcpc_test);
#endif /* CONFIG_ENABLE_SYSFS_DEBUG */
		soft_qc35_unregister(chg->qc);
		cancel_delayed_work_sync(&chg->led_work);
		charger_unregister(chg->subpmic_dev);
		mutex_destroy(&chg->dpdm_lock);
		mutex_destroy(&chg->fled_lock);
	}

	return 0;
}

static const struct of_device_id ofid_table[] = {
	{ .compatible = "nuvolta,nu6601_charger", },
	{ },
};
MODULE_DEVICE_TABLE(of, ofid_table);

static const struct platform_device_id nu_id_table[] = {
	{ "nu6601_charger", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, nu_id_table);

static struct platform_driver nu6601_charger = {
	.driver = {
		.name = "nu6601_charger",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ofid_table),
	},
	.probe = nu6601_charger_probe,
	.remove = nu6601_charger_remove,
	.id_table = nu_id_table,
	.shutdown = subpmic_chg_shutdown,
};
module_platform_driver(nu6601_charger);

MODULE_AUTHOR("mick.ye@nuvoltatech.com");
MODULE_DESCRIPTION("Nuvolta NU6601 Charger Driver");
MODULE_LICENSE("GPL v2");
