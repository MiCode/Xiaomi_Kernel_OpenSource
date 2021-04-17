/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <linux/switch.h>

#include <mtk_charger_intf.h>
#include <mt-plat/aee.h>
#include <mt-plat/charging.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/mtk_boot_common.h>
#include <mach/mtk_pe.h>

#include "inc/mt6370_pmu_charger.h"
#include "inc/mt6370_pmu.h"

#define MT6370_PMU_CHARGER_DRV_VERSION	"1.1.12_MTK"

/* ======================= */
/* MT6370 Charger Variable */
/* ======================= */

enum mt6370_pmu_charger_irqidx {
	MT6370_CHG_IRQIDX_CHGIRQ1 = 0,
	MT6370_CHG_IRQIDX_CHGIRQ2,
	MT6370_CHG_IRQIDX_CHGIRQ3,
	MT6370_CHG_IRQIDX_CHGIRQ4,
	MT6370_CHG_IRQIDX_CHGIRQ5,
	MT6370_CHG_IRQIDX_CHGIRQ6,
	MT6370_CHG_IRQIDX_QCIRQ,
	MT6370_CHG_IRQIDX_DICHGIRQ7,
	MT6370_CHG_IRQIDX_OVPCTRLIRQ,
	MT6370_CHG_IRQIDX_MAX,
};

enum mt6370_pmu_chg_type {
	MT6370_CHG_TYPE_NOVBUS = 0,
	MT6370_CHG_TYPE_UNDER_GOING,
	MT6370_CHG_TYPE_SDP,
	MT6370_CHG_TYPE_SDPNSTD,
	MT6370_CHG_TYPE_DCP,
	MT6370_CHG_TYPE_CDP,
	MT6370_CHG_TYPE_MAX,
};

enum mt6370_usbsw_state {
	MT6370_USBSW_CHG = 0,
	MT6370_USBSW_USB,
};

struct mt6370_pmu_charger_desc {
	u32 ichg;
	u32 aicr;
	u32 mivr;
	u32 cv;
	u32 ieoc;
	u32 safety_timer;
	u32 ircmp_resistor;
	u32 ircmp_vclamp;
	u32 dc_wdt;
	bool en_te;
	bool en_wdt;
	bool en_otg_wdt;
	bool en_polling;
	const char *chg_dev_name;
	const char *ls_dev_name;
};

struct mt6370_pmu_charger_data {
	/* Inherited from mtk_charger_info */
	struct mtk_charger_info mchr_info;
	struct mt6370_pmu_charger_desc *chg_desc;
	struct mt6370_pmu_chip *chip;
	struct mutex adc_access_lock;
	struct mutex irq_access_lock;
	struct mutex aicr_access_lock;
	struct mutex ichg_access_lock;
	struct mutex bc12_access_lock;
	struct device *dev;
	wait_queue_head_t wait_queue;
	bool err_state;
	enum charger_type chg_type;
	bool chg_online;
	u8 irq_flag[MT6370_CHG_IRQIDX_MAX];
	u32 zcv;
	bool adc_hang;
	struct switch_dev *usb_switch;
	bool bc12_en;
#ifndef CONFIG_TCPC_CLASS
	struct work_struct chgdet_work;
#endif
};

/* These default values will be used if there's no property in dts */
static struct mt6370_pmu_charger_desc mt6370_default_chg_desc = {
	.ichg = 2000000,		/* uA */
	.aicr = 500000,			/* uA */
	.mivr = 4400000,		/* uV */
	.cv = 4350000,			/* uA */
	.ieoc = 250000,			/* uA */
	.safety_timer = 12,		/* hour */
#ifdef CONFIG_MTK_BIF_SUPPORT
	.ircmp_resistor = 0,		/* uohm */
	.ircmp_vclamp = 0,		/* uV */
#else
	.ircmp_resistor = 25000,	/* uohm */
	.ircmp_vclamp = 32000,		/* uV */
#endif
	.dc_wdt = 4000000,		/* us */
	.en_te = true,
	.en_wdt = true,
};


static const u32 mt6370_otg_oc_threshold[] = {
	500000, 700000, 1100000, 1300000, 1800000, 2100000, 2400000, 3000000,
}; /* uA */

static const u32 mt6370_dc_vbatov_lvl[] = {
	104, 108, 119,
}; /* % * VOREG */

static const u32 mt6370_dc_wdt[] = {
	0, 125000, 250000, 500000, 1000000, 2000000, 4000000, 8000000,
}; /* us */

enum mt6370_charging_status {
	MT6370_CHG_STATUS_READY = 0,
	MT6370_CHG_STATUS_PROGRESS,
	MT6370_CHG_STATUS_DONE,
	MT6370_CHG_STATUS_FAULT,
	MT6370_CHG_STATUS_MAX,
};

/* Charging status name */
static const char *mt6370_chg_status_name[MT6370_CHG_STATUS_MAX] = {
	"ready", "progress", "done", "fault",
};

static const unsigned char mt6370_reg_en_hidden_mode[] = {
	MT6370_PMU_REG_HIDDENPASCODE1,
	MT6370_PMU_REG_HIDDENPASCODE2,
	MT6370_PMU_REG_HIDDENPASCODE3,
	MT6370_PMU_REG_HIDDENPASCODE4,
};

static const unsigned char mt6370_val_en_hidden_mode[] = {
	0x96, 0x69, 0xC3, 0x3C,
};

enum mt6370_iin_limit_sel {
	MT6370_IIMLMTSEL_AICR_3250 = 0,
	MT6370_IIMLMTSEL_CHG_TYPE,
	MT6370_IINLMTSEL_AICR,
	MT6370_IINLMTSEL_LOWER_LEVEL, /* lower of above three */
};

enum mt6370_adc_sel {
	MT6370_ADC_VBUS_DIV5 = 1,
	MT6370_ADC_VBUS_DIV2,
	MT6370_ADC_VSYS,
	MT6370_ADC_VBAT,
	MT6370_ADC_TS_BAT = 6,
	MT6370_ADC_IBUS = 8,
	MT6370_ADC_IBAT,
	MT6370_ADC_CHG_VDDP = 11,
	MT6370_ADC_TEMP_JC,
	MT6370_ADC_MAX,
};

/* Unit for each ADC parameter
 * 0 stands for reserved
 * For TS_BAT/TS_BUS, the real unit is 0.25.
 * Here we use 25, please remember to divide 100 while showing the value
 */
static const int mt6370_adc_unit[MT6370_ADC_MAX] = {
	0,
	MT6370_ADC_UNIT_VBUS_DIV5,
	MT6370_ADC_UNIT_VBUS_DIV2,
	MT6370_ADC_UNIT_VSYS,
	MT6370_ADC_UNIT_VBAT,
	0,
	MT6370_ADC_UNIT_TS_BAT,
	0,
	MT6370_ADC_UNIT_IBUS,
	MT6370_ADC_UNIT_IBAT,
	0,
	MT6370_ADC_UNIT_CHG_VDDP,
	MT6370_ADC_UNIT_TEMP_JC,
};

static const int mt6370_adc_offset[MT6370_ADC_MAX] = {
	0,
	MT6370_ADC_OFFSET_VBUS_DIV5,
	MT6370_ADC_OFFSET_VBUS_DIV2,
	MT6370_ADC_OFFSET_VSYS,
	MT6370_ADC_OFFSET_VBAT,
	0,
	MT6370_ADC_OFFSET_TS_BAT,
	0,
	MT6370_ADC_OFFSET_IBUS,
	MT6370_ADC_OFFSET_IBAT,
	0,
	MT6370_ADC_OFFSET_CHG_VDDP,
	MT6370_ADC_OFFSET_TEMP_JC,
};


/* =============================== */
/* mt6370 Charger Register Address */
/* =============================== */

static const unsigned char mt6370_chg_reg_addr[] = {
	MT6370_PMU_REG_CHGCTRL1,
	MT6370_PMU_REG_CHGCTRL2,
	MT6370_PMU_REG_CHGCTRL3,
	MT6370_PMU_REG_CHGCTRL4,
	MT6370_PMU_REG_CHGCTRL5,
	MT6370_PMU_REG_CHGCTRL6,
	MT6370_PMU_REG_CHGCTRL7,
	MT6370_PMU_REG_CHGCTRL8,
	MT6370_PMU_REG_CHGCTRL9,
	MT6370_PMU_REG_CHGCTRL10,
	MT6370_PMU_REG_CHGCTRL11,
	MT6370_PMU_REG_CHGCTRL12,
	MT6370_PMU_REG_CHGCTRL13,
	MT6370_PMU_REG_CHGCTRL14,
	MT6370_PMU_REG_CHGCTRL15,
	MT6370_PMU_REG_CHGCTRL16,
	MT6370_PMU_REG_CHGADC,
	MT6370_PMU_REG_DEVICETYPE,
	MT6370_PMU_REG_QCCTRL1,
	MT6370_PMU_REG_QCCTRL2,
	MT6370_PMU_REG_QC3P0CTRL1,
	MT6370_PMU_REG_QC3P0CTRL2,
	MT6370_PMU_REG_USBSTATUS1,
	MT6370_PMU_REG_QCSTATUS1,
	MT6370_PMU_REG_QCSTATUS2,
	MT6370_PMU_REG_CHGPUMP,
	MT6370_PMU_REG_CHGCTRL17,
	MT6370_PMU_REG_CHGCTRL18,
	MT6370_PMU_REG_CHGDIRCHG1,
	MT6370_PMU_REG_CHGDIRCHG2,
	MT6370_PMU_REG_CHGDIRCHG3,
	MT6370_PMU_REG_CHGSTAT,
	MT6370_PMU_REG_CHGNTC,
	MT6370_PMU_REG_ADCDATAH,
	MT6370_PMU_REG_ADCDATAL,
	MT6370_PMU_REG_CHGCTRL19,
	MT6370_PMU_REG_CHGSTAT1,
	MT6370_PMU_REG_CHGSTAT2,
	MT6370_PMU_REG_CHGSTAT3,
	MT6370_PMU_REG_CHGSTAT4,
	MT6370_PMU_REG_CHGSTAT5,
	MT6370_PMU_REG_CHGSTAT6,
	MT6370_PMU_REG_QCSTAT,
	MT6370_PMU_REG_DICHGSTAT,
	MT6370_PMU_REG_OVPCTRLSTAT,
};

/* ===================================================================== */
/* Internal Functions                                                    */
/* ===================================================================== */
static int mt6370_set_aicr(struct mtk_charger_info *mchr_info, void *data);
static int mt6370_get_aicr(struct mtk_charger_info *mchr_info, void *data);
static int mt6370_set_ichg(struct mtk_charger_info *mchr_info, void *data);
static int mt6370_get_ichg(struct mtk_charger_info *mchr_info, void *data);
static int mt6370_enable_charging(struct mtk_charger_info *mchr_info,
	void *data);

static inline void mt6370_chg_irq_set_flag(
	struct mt6370_pmu_charger_data *chg_data, u8 *irq, u8 mask)
{
	mutex_lock(&chg_data->irq_access_lock);
	*irq |= mask;
	mutex_unlock(&chg_data->irq_access_lock);
}

static inline void mt6370_chg_irq_clr_flag(
	struct mt6370_pmu_charger_data *chg_data, u8 *irq, u8 mask)
{
	mutex_lock(&chg_data->irq_access_lock);
	*irq &= ~mask;
	mutex_unlock(&chg_data->irq_access_lock);
}

static u8 mt6370_find_closest_reg_value(u32 min, u32 max, u32 step, u32 num,
	u32 target)
{
	u32 i = 0, cur_val = 0, next_val = 0;

	/* Smaller than minimum supported value, use minimum one */
	if (target < min)
		return 0;

	for (i = 0; i < num - 1; i++) {
		cur_val = min + i * step;
		next_val = cur_val + step;

		if (cur_val > max)
			cur_val = max;

		if (next_val > max)
			next_val = max;

		if (target >= cur_val && target < next_val)
			return i;
	}

	/* Greater than maximum supported value, use maximum one */
	return num - 1;
}

static u8 mt6370_find_closest_reg_value_via_table(const u32 *value_table,
	u32 table_size, u32 target_value)
{
	u32 i = 0;

	/* Smaller than minimum supported value, use minimum one */
	if (target_value < value_table[0])
		return 0;

	for (i = 0; i < table_size - 1; i++) {
		if (target_value >= value_table[i] &&
		    target_value < value_table[i + 1])
			return i;
	}

	/* Greater than maximum supported value, use maximum one */
	return table_size - 1;
}

static u32 mt6370_find_closest_real_value(u32 min, u32 max, u32 step,
	u8 reg_val)
{
	u32 ret_val = 0;

	ret_val = min + reg_val * step;
	if (ret_val > max)
		ret_val = max;

	return ret_val;
}

static int mt6370_set_fast_charge_timer(
	struct mt6370_pmu_charger_data *chg_data, u32 hour)
{
	int ret = 0;
	u8 reg_fct = 0;

	reg_fct = mt6370_find_closest_reg_value(
		MT6370_WT_FC_MIN,
		MT6370_WT_FC_MAX,
		MT6370_WT_FC_STEP,
		MT6370_WT_FC_NUM,
		hour
	);

	dev_info(chg_data->dev, "%s: timer = %d (0x%02X)\n", __func__, hour,
		reg_fct);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGCTRL12,
		MT6370_MASK_WT_FC,
		reg_fct << MT6370_SHIFT_WT_FC
	);

	return ret;
}

static int mt6370_set_usbsw_state(struct mt6370_pmu_charger_data *chg_data,
	int state)
{
	dev_info(chg_data->dev, "%s: state = %d\n", __func__, state);

	if (chg_data->usb_switch)
		switch_set_state(chg_data->usb_switch, state);
#ifdef CONFIG_PROJECT_PHY
	else {
		if (state == MT6370_USBSW_CHG)
			Charger_Detect_Init();
		else
			Charger_Detect_Release();
	}
#endif

	return 0;
}

/* Hardware pin current limit */
static int mt6370_enable_ilim(struct mt6370_pmu_charger_data *chg_data, bool en)
{
	int ret = 0;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);

	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
		(chg_data->chip, MT6370_PMU_REG_CHGCTRL3, MT6370_MASK_ILIM_EN);

	return ret;
}

/* Select IINLMTSEL */
static int mt6370_select_input_current_limit(
	struct mt6370_pmu_charger_data *chg_data, enum mt6370_iin_limit_sel sel)
{
	int ret = 0;

	dev_info(chg_data->dev, "%s: select input current limit = %d\n",
		__func__, sel);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGCTRL2,
		MT6370_MASK_IINLMTSEL,
		sel << MT6370_SHIFT_IINLMTSEL
	);

	return ret;
}

static int mt6370_enable_hidden_mode(struct mt6370_pmu_charger_data *chg_data,
	bool en)
{
	int ret = 0;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);

	/* Disable hidden mode */
	if (!en) {
		ret = mt6370_pmu_reg_write(chg_data->chip,
			mt6370_reg_en_hidden_mode[0], 0x00);
		if (ret < 0)
			goto err;
		return ret;
	}

	ret = mt6370_pmu_reg_block_write(
		chg_data->chip,
		mt6370_reg_en_hidden_mode[0],
		ARRAY_SIZE(mt6370_val_en_hidden_mode),
		mt6370_val_en_hidden_mode
	);
	if (ret < 0)
		goto err;

	return ret;

err:
	dev_err(chg_data->dev, "%s: en = %d failed, ret = %d\n", __func__,
		en, ret);
	return ret;
}

/* Software workaround */
static int mt6370_chg_sw_workaround(struct mt6370_pmu_charger_data *chg_data)
{
	int ret = 0;
	u8 zcv_data[2] = {0};

	dev_info(chg_data->dev, "%s\n", __func__);

	/* Enter hidden mode */
	ret = mt6370_enable_hidden_mode(chg_data, true);
	if (ret < 0)
		goto out;

	/* Read ZCV data */
	ret = mt6370_pmu_reg_block_read(chg_data->chip,
		MT6370_PMU_REG_ADCBATDATAH, 2, zcv_data);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: read zcv data failed\n", __func__);
	else {
		chg_data->zcv = 5000 * (zcv_data[0] * 256 + zcv_data[1]);

		dev_info(chg_data->dev, "%s: zcv = (0x%02X, 0x%02X, %dmV)\n",
			__func__, zcv_data[0], zcv_data[1],
			chg_data->zcv / 1000);
	}

	/* Trigger any ADC before disabling ZCV */
	ret = mt6370_pmu_reg_write(chg_data->chip, MT6370_PMU_REG_CHGADC,
		0x11);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: trigger ADC failed\n", __func__);

	/* Disable ZCV */
	ret = mt6370_pmu_reg_set_bit(chg_data->chip, MT6370_PMU_REG_OSCCTRL,
		0x04);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: disable ZCV failed\n", __func__);

	/* Disable TS auto sensing */
	ret = mt6370_pmu_reg_clr_bit(chg_data->chip,
		MT6370_PMU_REG_CHGHIDDENCTRL15, 0x01);

	mdelay(200);

	/* Disable SEN_DCP for charging mode */
	ret = mt6370_pmu_reg_clr_bit(chg_data->chip,
		MT6370_PMU_REG_QCCTRL2, MT6370_MASK_EN_DCP);

out:
	/* Exit hidden mode */
	ret = mt6370_enable_hidden_mode(chg_data, false);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: exit hidden mode failed\n",
			__func__);

	return ret;
}

static int mt6370_get_adc(struct mt6370_pmu_charger_data *chg_data,
	enum mt6370_adc_sel adc_sel, int *adc_val)
{
	int ret = 0, i = 0;
	u8 adc_data[6] = {0};
	bool adc_start = false;
	u32 aicr = 0, ichg = 0;
	s64 adc_result = 0;
	const int max_wait_times = 6;

	mutex_lock(&chg_data->adc_access_lock);
	mt6370_enable_hidden_mode(chg_data, true);

	/* Select ADC to desired channel */
	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGADC,
		MT6370_MASK_ADC_IN_SEL,
		adc_sel << MT6370_SHIFT_ADC_IN_SEL
	);

	if (ret < 0) {
		dev_err(chg_data->dev, "%s: select ch to %d failed, ret = %d\n",
			__func__, adc_sel, ret);
		goto out;
	}

	/* Workaround for IBUS & IBAT */
	if (adc_sel == MT6370_ADC_IBUS) {
		mutex_lock(&chg_data->aicr_access_lock);
		ret = mt6370_get_aicr(&chg_data->mchr_info, &aicr);
		if (ret < 0) {
			dev_err(chg_data->dev, "%s: get aicr failed\n",
				__func__);
			goto out_unlock_all;
		}
	} else if (adc_sel == MT6370_ADC_IBAT) {
		mutex_lock(&chg_data->ichg_access_lock);
		ret = mt6370_get_ichg(&chg_data->mchr_info, &ichg);
		if (ret < 0) {
			dev_err(chg_data->dev, "%s: get ichg failed\n",
				__func__);
			goto out_unlock_all;
		}
	}

	/* Start ADC conversation */
	ret = mt6370_pmu_reg_set_bit(chg_data->chip, MT6370_PMU_REG_CHGADC,
		MT6370_MASK_ADC_START);
	if (ret < 0) {
		dev_err(chg_data->dev,
			"%s: start conversation failed, sel = %d, ret = %d\n",
			__func__, adc_sel, ret);
		goto out_unlock_all;
	}

	for (i = 0; i < max_wait_times; i++) {
		msleep(35);
		ret = mt6370_pmu_reg_test_bit(chg_data->chip,
			MT6370_PMU_REG_CHGADC, MT6370_SHIFT_ADC_START,
			&adc_start);
		if (!adc_start && ret >= 0)
			break;
	}
	if (i == max_wait_times) {
		dev_err(chg_data->dev,
			"%s: wait conversation failed, sel = %d, ret = %d\n",
			__func__, adc_sel, ret);

		/* AEE for debug */
		if (!chg_data->adc_hang) {
			for (i = 0; i < ARRAY_SIZE(mt6370_chg_reg_addr); i++) {
				ret = mt6370_pmu_reg_read(chg_data->chip,
						mt6370_chg_reg_addr[i]);

				dev_err(chg_data->dev,
					"%s: reg[0x%02X] = 0x%02X\n",
					__func__, mt6370_chg_reg_addr[i], ret);
			}


			chg_data->adc_hang = true;
		}

		/* Add for debug */
		/* ZCV, reg0x10 */
		ret = mt6370_pmu_reg_read(chg_data->chip,
						MT6370_PMU_REG_OSCCTRL);
		if (ret < 0)
			dev_err(chg_data->dev, "%s: read reg0x10 failed\n",
						__func__);
		else
			dev_err(chg_data->dev, "%s: reg0x10 = 0x%02X\n",
						__func__, ret);

		/* TS auto sensing */
		ret = mt6370_pmu_reg_read(chg_data->chip,
						MT6370_PMU_REG_CHGHIDDENCTRL15);
		if (ret < 0)
			dev_err(chg_data->dev, "%s: read reg0x3E failed\n",
						__func__);
		else
			dev_err(chg_data->dev, "%s: reg0x3E = 0x%02X\n",
						__func__, ret);

	}
	mdelay(1);

	/* Read ADC data */
	ret = mt6370_pmu_reg_block_read(chg_data->chip, MT6370_PMU_REG_ADCDATAH,
		6, adc_data);
	if (ret < 0) {
		dev_err(chg_data->dev,
			"%s: read ADC data failed, ret = %d\n", __func__, ret);
		goto out_unlock_all;
	}

	dev_dbg(chg_data->dev,
		"%s: adc_sel = %d, adc_h = 0x%02X, adc_l = 0x%02X\n",
		__func__, adc_sel, adc_data[0], adc_data[1]);

	dev_dbg(chg_data->dev,
		"%s: 0x4E~51 = (0x%02X, 0x%02X, 0x%02X, 0x%02X)\n", __func__,
		adc_data[2], adc_data[3], adc_data[4], adc_data[5]);

	/* Calculate ADC value */
	adc_result = (adc_data[0] * 256
		+ adc_data[1]) * mt6370_adc_unit[adc_sel]
		+ mt6370_adc_offset[adc_sel];

out_unlock_all:
	/* Coefficient of IBUS & IBAT */
	if (adc_sel == MT6370_ADC_IBUS) {
		if (aicr < 400000) /* 400mA */
			adc_result = adc_result * 67 / 100;
		mutex_unlock(&chg_data->aicr_access_lock);
	} else if (adc_sel == MT6370_ADC_IBAT) {
		if (ichg >= 100000 && ichg <= 450000) /* 100~450mA */
			adc_result = adc_result * 475 / 1000;
		else if (ichg >= 500000 && ichg <= 850000) /* 500~850mA */
			adc_result = adc_result * 536 / 1000;
		mutex_unlock(&chg_data->ichg_access_lock);
	}

out:
	*adc_val = adc_result;
	mt6370_enable_hidden_mode(chg_data, false);
	mutex_unlock(&chg_data->adc_access_lock);
	return ret;
}


static int mt6370_enable_wdt(struct mt6370_pmu_charger_data *chg_data,
	bool en)
{
	int ret = 0;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);
	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
		(chg_data->chip, MT6370_PMU_REG_CHGCTRL13, MT6370_MASK_WDT_EN);

	return ret;
}

static int mt6370_is_charging_enable(struct mt6370_pmu_charger_data *chg_data,
	bool *en)
{
	int ret = 0;

	ret = mt6370_pmu_reg_test_bit(chg_data->chip, MT6370_PMU_REG_CHGCTRL2,
		MT6370_SHIFT_CHG_EN, en);

	return ret;
}

static int mt6370_enable_te(struct mt6370_pmu_charger_data *chg_data, bool en)
{
	int ret = 0;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);
	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
		(chg_data->chip, MT6370_PMU_REG_CHGCTRL2, MT6370_MASK_TE_EN);

	return ret;
}

static int mt6370_enable_pump_express(struct mt6370_pmu_charger_data *chg_data,
	bool en)
{
	int ret = 0, i = 0;
	const int max_wait_times = 5;
	bool pumpx_en = false;
	bool chg_en = true;
	u32 aicr = 80000;	/* 10uA */
	u32 ichg = 200000;	/* 10uA */

	dev_info(chg_data->dev, "%s: en %d\n", __func__, en);

	ret = mt6370_set_aicr(&chg_data->mchr_info, &aicr);
	if (ret < 0)
		return ret;

	ret = mt6370_set_ichg(&chg_data->mchr_info, &ichg);
	if (ret < 0)
		return ret;

	ret = mt6370_enable_charging(&chg_data->mchr_info, &chg_en);
	if (ret < 0)
		return ret;

	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
	(chg_data->chip, MT6370_PMU_REG_CHGCTRL17, MT6370_MASK_PUMPX_EN);
	if (ret < 0)
		return ret;

	for (i = 0; i < max_wait_times; i++) {
		msleep(2500);
		ret = mt6370_pmu_reg_test_bit(chg_data->chip,
			MT6370_PMU_REG_CHGCTRL17, MT6370_SHIFT_PUMPX_EN,
			&pumpx_en);
		if (!pumpx_en && ret >= 0)
			break;
	}
	if (i == max_wait_times) {
		dev_err(chg_data->dev, "%s: wait failed, ret = %d\n", __func__,
			ret);
		ret = -EIO;
		return ret;
	}

	return ret;
}

static int mt6370_get_ieoc(struct mt6370_pmu_charger_data *chg_data, u32 *ieoc)
{
	int ret = 0;
	u8 reg_ieoc = 0;

	ret = mt6370_pmu_reg_read(chg_data->chip, MT6370_PMU_REG_CHGCTRL9);
	if (ret < 0)
		return ret;

	reg_ieoc = (ret & MT6370_MASK_IEOC) >> MT6370_SHIFT_IEOC;
	*ieoc = mt6370_find_closest_real_value(
		MT6370_IEOC_MIN,
		MT6370_IEOC_MAX,
		MT6370_IEOC_STEP,
		reg_ieoc
	);

	return ret;
}

static int mt6370_get_mivr(struct mt6370_pmu_charger_data *chg_data, u32 *mivr)
{
	int ret = 0;
	u8 reg_mivr = 0;

	ret = mt6370_pmu_reg_read(chg_data->chip, MT6370_PMU_REG_CHGCTRL6);
	if (ret < 0)
		return ret;

	reg_mivr = ((ret & MT6370_MASK_MIVR) >> MT6370_SHIFT_MIVR) & 0xFF;
	*mivr = mt6370_find_closest_real_value(
		MT6370_MIVR_MIN,
		MT6370_MIVR_MAX,
		MT6370_MIVR_STEP,
		reg_mivr
	);

	return ret;
}

static int mt6370_set_ieoc(struct mt6370_pmu_charger_data *chg_data, u32 ieoc)
{
	int ret = 0;

	/* Find corresponding reg value */
	u8 reg_ieoc = mt6370_find_closest_reg_value(
		MT6370_IEOC_MIN,
		MT6370_IEOC_MAX,
		MT6370_IEOC_STEP,
		MT6370_IEOC_NUM,
		ieoc
	);

	dev_info(chg_data->dev, "%s: ieoc = %d (0x%02X)\n", __func__, ieoc,
		reg_ieoc);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGCTRL9,
		MT6370_MASK_IEOC,
		reg_ieoc << MT6370_SHIFT_IEOC
	);

	return ret;
}

static int mt6370_get_charging_status(struct mt6370_pmu_charger_data *chg_data,
	enum mt6370_charging_status *chg_stat)
{
	int ret = 0;

	ret = mt6370_pmu_reg_read(chg_data->chip, MT6370_PMU_REG_CHGSTAT);
	if (ret < 0)
		return ret;

	*chg_stat = (ret & MT6370_MASK_CHG_STAT) >> MT6370_SHIFT_CHG_STAT;

	return ret;
}

static int mt6370_set_dc_wdt(struct mt6370_pmu_charger_data *chg_data, u32 us)
{
	int ret = 0;
	u8 reg_wdt = 0;

	reg_wdt = mt6370_find_closest_reg_value_via_table(
		mt6370_dc_wdt,
		ARRAY_SIZE(mt6370_dc_wdt),
		us
	);

	dev_info(chg_data->dev, "%s: wdt = %dms(0x%02X)\n", __func__, us / 1000,
		reg_wdt);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGDIRCHG2,
		MT6370_MASK_DC_WDT,
		reg_wdt << MT6370_SHIFT_DC_WDT
	);

	return ret;
}

static int mt6370_enable_jeita(struct mt6370_pmu_charger_data *chg_data,
	bool en)
{
	int ret = 0;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);

	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
	(chg_data->chip, MT6370_PMU_REG_CHGCTRL16, MT6370_MASK_JEITA_EN);

	return ret;
}

static int mt6370_set_aicl_vth(struct mt6370_pmu_charger_data *chg_data,
	u32 aicl_vth)
{
	int ret = 0;
	u8 reg_aicl_vth = 0;

	reg_aicl_vth = mt6370_find_closest_reg_value(
		MT6370_AICL_VTH_MIN,
		MT6370_AICL_VTH_MAX,
		MT6370_AICL_VTH_STEP,
		MT6370_AICL_VTH_NUM,
		aicl_vth
	);

	dev_info(chg_data->dev, "%s: vth = %d (0x%02X)\n", __func__, aicl_vth,
		reg_aicl_vth);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGCTRL14,
		MT6370_MASK_AICL_VTH,
		reg_aicl_vth << MT6370_SHIFT_AICL_VTH
	);

	if (ret < 0)
		dev_err(chg_data->dev, "%s: set aicl vth failed, ret = %d\n",
			__func__, ret);

	return ret;
}

static int mt6370_enable_chgdet_flow(struct mt6370_pmu_charger_data *chg_data,
	bool en)
{
	int ret = 0, i = 0;
#ifdef CONFIG_TCPC_CLASS
	int vbus = 0;
#endif
	const int max_wait_cnt = 200;

	if (en) {
		/* Workaround for CDP port */
		for (i = 0; i < max_wait_cnt; i++) {
			if (is_usb_rdy())
				break;
			dev_err(chg_data->dev, "%s: CDP block\n", __func__);
#ifdef CONFIG_TCPC_CLASS
			ret = mt6370_get_adc(chg_data, MT6370_ADC_VBUS_DIV5,
				&vbus);
			if (ret >= 0 && vbus < 4300000) {
				dev_info(chg_data->dev,
					"%s: plug out, vbus = %dmV\n",
					__func__, vbus / 1000);
				return 0;
			}
#endif
			msleep(100);
		}
		if (i == max_wait_cnt)
			dev_err(chg_data->dev, "%s: CDP timeout\n", __func__);
		else
			dev_info(chg_data->dev, "%s: CDP free\n", __func__);
	}

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);
	mutex_lock(&chg_data->bc12_access_lock);
	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
	(chg_data->chip, MT6370_PMU_REG_DEVICETYPE, MT6370_MASK_USBCHGEN);
	if (ret >= 0)
		chg_data->bc12_en = en;
	mutex_unlock(&chg_data->bc12_access_lock);

	return ret;

}

static int _mt6370_set_mivr(struct mt6370_pmu_charger_data *chg_data, u32 uV)
{
	int ret = 0;
	u8 reg_mivr = 0;

	/* Find corresponding reg value */
	reg_mivr = mt6370_find_closest_reg_value(
		MT6370_MIVR_MIN,
		MT6370_MIVR_MAX,
		MT6370_MIVR_STEP,
		MT6370_MIVR_NUM,
		uV
	);

	dev_info(chg_data->dev, "%s: mivr = %d (0x%02X)\n", __func__, uV,
		reg_mivr);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGCTRL6,
		MT6370_MASK_MIVR,
		reg_mivr << MT6370_SHIFT_MIVR
	);

	return ret;

}

#ifndef CONFIG_TCPC_CLASS
static void mt6370_chgdet_work_handler(struct work_struct *work)
{
	int ret = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)container_of(work,
		struct mt6370_pmu_charger_data, chgdet_work);

	mt6370_set_usbsw_state(chg_data, MT6370_USBSW_CHG);

	/* Enable USB charger type detection */
	ret = mt6370_enable_chgdet_flow(chg_data, true);
	if (ret < 0)
		dev_err(chg_data->dev,
			"%s: enable usb chrdet failed\n", __func__);

}
#endif /* CONFIG_TCPC_CLASS */

static int _mt6370_set_ichg(struct mt6370_pmu_charger_data *chg_data, u32 uA)
{
	int ret = 0;
	u8 reg_ichg = 0;

	/* For adc workaround */
	mutex_lock(&chg_data->ichg_access_lock);

	/* Find corresponding reg value */
	reg_ichg = mt6370_find_closest_reg_value(
		MT6370_ICHG_MIN,
		MT6370_ICHG_MAX,
		MT6370_ICHG_STEP,
		MT6370_ICHG_NUM,
		uA
	);

	dev_info(chg_data->dev, "%s: ichg = %d (0x%02X)\n", __func__, uA,
		reg_ichg);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGCTRL7,
		MT6370_MASK_ICHG,
		reg_ichg << MT6370_SHIFT_ICHG
	);

	if (chg_data->chip->chip_vid == 0xF0)
		goto bypass_ieoc_workaround;
	/* Workaround to make IEOC accurate */
	if (uA < 900000) /* 900mA */
		ret = mt6370_set_ieoc(chg_data,
			chg_data->chg_desc->ieoc + 100000);
	else
		ret = mt6370_set_ieoc(chg_data, chg_data->chg_desc->ieoc);

bypass_ieoc_workaround:
	/* For adc workaround */
	mutex_unlock(&chg_data->ichg_access_lock);

	return ret;
}

static int _mt6370_set_aicr(struct mt6370_pmu_charger_data *chg_data, u32 uA)
{
	int ret = 0;
	u8 reg_aicr = 0;

	mutex_lock(&chg_data->aicr_access_lock);

	/* Find corresponding reg value */
	reg_aicr = mt6370_find_closest_reg_value(
		MT6370_AICR_MIN,
		MT6370_AICR_MAX,
		MT6370_AICR_STEP,
		MT6370_AICR_NUM,
		uA
	);

	dev_info(chg_data->dev, "%s: aicr = %d (0x%02X)\n", __func__, uA,
		reg_aicr);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGCTRL3,
		MT6370_MASK_AICR,
		reg_aicr << MT6370_SHIFT_AICR
	);

	mutex_unlock(&chg_data->aicr_access_lock);
	return ret;
}

static int _mt6370_set_cv(struct mt6370_pmu_charger_data *chg_data, u32 uV)
{
	int ret = 0;
	u8 reg_cv = 0;

	reg_cv = mt6370_find_closest_reg_value(
		MT6370_BAT_VOREG_MIN,
		MT6370_BAT_VOREG_MAX,
		MT6370_BAT_VOREG_STEP,
		MT6370_BAT_VOREG_NUM,
		uV
	);

	dev_info(chg_data->dev, "%s: bat voreg = %d (0x%02X)\n", __func__, uV,
		reg_cv);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGCTRL4,
		MT6370_MASK_BAT_VOREG,
		reg_cv << MT6370_SHIFT_BAT_VOREG
	);

	return ret;
}

static int _mt6370_enable_safety_timer(struct mt6370_pmu_charger_data *chg_data,
	bool en)
{
	int ret = 0;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);
	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
		(chg_data->chip, MT6370_PMU_REG_CHGCTRL12, MT6370_MASK_TMR_EN);

	return ret;
}

static int _mt6370_enable_hz(struct mt6370_pmu_charger_data *chg_data, bool en)
{
	int ret = 0;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);
	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
		(chg_data->chip, MT6370_PMU_REG_CHGCTRL1, MT6370_MASK_HZ_EN);

	return ret;
}

static int _mt6370_set_ircmp_resistor(struct mt6370_pmu_charger_data *chg_data,
	u32 uohm)
{
	int ret = 0;
	u8 reg_resistor = 0;

	reg_resistor = mt6370_find_closest_reg_value(
		MT6370_IRCMP_RES_MIN,
		MT6370_IRCMP_RES_MAX,
		MT6370_IRCMP_RES_STEP,
		MT6370_IRCMP_RES_NUM,
		uohm
	);

	dev_info(chg_data->dev, "%s: resistor = %d (0x%02X)\n", __func__, uohm,
		reg_resistor);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGCTRL18,
		MT6370_MASK_IRCMP_RES,
		reg_resistor << MT6370_SHIFT_IRCMP_RES
	);

	return ret;
}

static int _mt6370_set_ircmp_vclamp(struct mt6370_pmu_charger_data *chg_data,
	u32 uV)
{
	int ret = 0;
	u8 reg_vclamp = 0;

	reg_vclamp = mt6370_find_closest_reg_value(
		MT6370_IRCMP_VCLAMP_MIN,
		MT6370_IRCMP_VCLAMP_MAX,
		MT6370_IRCMP_VCLAMP_STEP,
		MT6370_IRCMP_VCLAMP_NUM,
		uV
	);

	dev_info(chg_data->dev, "%s: vclamp = %d (0x%02X)\n", __func__, uV,
		reg_vclamp);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGCTRL18,
		MT6370_MASK_IRCMP_VCLAMP,
		reg_vclamp << MT6370_SHIFT_IRCMP_VCLAMP
	);

	return ret;
}



/* =================== */
/* Released interfaces */
/* =================== */

/* This is for GM20's PE20 */
static int mt6370_hw_init(struct mtk_charger_info *mchr_info, void *data)
{
	return 0;
}

static int mt6370_enable_hz(struct mtk_charger_info *mchr_info, void *data)
{
	int ret = 0;
	bool en = *((bool *)data);
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	ret = _mt6370_enable_hz(chg_data, en);

	return ret;
}

static int mt6370_run_aicl(struct mtk_charger_info *mchr_info, void *data)
{
	int ret = 0;
	u32 mivr = 0, aicl_vth = 0, aicr = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	/* Check whether MIVR loop is active */
	if (!(chg_data->irq_flag[MT6370_CHG_IRQIDX_CHGIRQ1]
					& MT6370_MASK_CHG_MIVR)) {
		dev_info(chg_data->dev,
				"%s: mivr loop is not active\n", __func__);
		return 0;
	}
	dev_info(chg_data->dev, "%s: mivr loop is active\n", __func__);

	/* Clear chg mivr event */
	mt6370_chg_irq_clr_flag(chg_data,
		&chg_data->irq_flag[MT6370_CHG_IRQIDX_CHGIRQ1],
		MT6370_MASK_CHG_MIVR);

	ret = mt6370_get_mivr(chg_data, &mivr);
	if (ret < 0)
		goto out;

	/* Check if there's a suitable AICL_VTH */
	aicl_vth = mivr + 200;
	if (aicl_vth > MT6370_AICL_VTH_MAX) {
		pr_info("%s: no suitable VTH, vth = %d\n", __func__, aicl_vth);
		ret = -EINVAL;
		goto out;
	}

	ret = mt6370_set_aicl_vth(chg_data, aicl_vth);
	if (ret < 0)
		goto out;

	/* Clear AICL measurement IRQ */
	mt6370_chg_irq_clr_flag(chg_data,
		&chg_data->irq_flag[MT6370_CHG_IRQIDX_CHGIRQ5],
		MT6370_MASK_CHG_AICLMEASI);

	ret = mt6370_pmu_reg_set_bit(chg_data->chip, MT6370_PMU_REG_CHGCTRL14,
		MT6370_MASK_AICL_MEAS);
	if (ret < 0)
		goto out;

	ret = wait_event_interruptible_timeout(chg_data->wait_queue,
		chg_data->irq_flag[MT6370_CHG_IRQIDX_CHGIRQ5] &
		MT6370_MASK_CHG_AICLMEASI,
		msecs_to_jiffies(2500));
	if (ret <= 0) {
		pr_err("%s: wait AICL time out, ret = %d\n", __func__, ret);
		ret = -EIO;
		goto out;
	}

	ret = mt6370_get_aicr(mchr_info, &aicr);
	if (ret < 0)
		goto out;

	*((u32 *)data) = aicr / 100;
	dev_info(chg_data->dev, "%s: aicr upper bound = %dmA\n", __func__,
		aicr / 100);

	goto en_mivrirq;

out:
	*((u32 *)data) = 0;
en_mivrirq:
	ret = mt6370_pmu_reg_clr_bit(chg_data->chip,
		MT6370_PMU_CHGMASK1, MT6370_MASK_CHG_MIVRM);

	return ret;
}

static int mt6370_set_ircmp_resistor(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	u32 uV = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	uV = *((u32 *)data) * 1000;

	ret = _mt6370_set_ircmp_resistor(chg_data, uV);

	return ret;
}

static int mt6370_set_ircmp_vclamp(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	u32 uV = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	uV = *((u32 *)data) * 1000;

	ret = _mt6370_set_ircmp_vclamp(chg_data, uV);

	return ret;
}

static int mt6370_set_error_state(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	chg_data->err_state = *((bool *)data);
	mt6370_enable_hz(mchr_info, &chg_data->err_state);

	return ret;
}

static int mt6370_get_charger_type(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	dev_info(chg_data->dev, "%s\n", __func__);

#ifndef CONFIG_TCPC_CLASS
	mt6370_set_usbsw_state(chg_data, MT6370_USBSW_CHG);

	/* Clear attachi event */
	mt6370_chg_irq_clr_flag(chg_data,
		&chg_data->irq_flag[MT6370_CHG_IRQIDX_QCIRQ],
		MT6370_MASK_ATTACHI);

	/* Turn off/on USB charger detection to retrigger bc1.2 */
	ret = mt6370_enable_chgdet_flow(chg_data, false);
	if (ret < 0)
		pr_err("%s: disable usb chrdet failed\n", __func__);

	ret = mt6370_enable_chgdet_flow(chg_data, true);
	if (ret < 0)
		pr_err("%s: disable usb chrdet failed\n", __func__);
#endif

	ret = wait_event_interruptible_timeout(chg_data->wait_queue,
	chg_data->irq_flag[MT6370_CHG_IRQIDX_QCIRQ] & MT6370_MASK_ATTACHI,
	msecs_to_jiffies(1000));
	if (ret <= 0) {
		pr_err("%s: wait attachi failed, ret = %d\n", __func__, ret);
		chg_data->chg_type = CHARGER_UNKNOWN;
	}

	*(CHARGER_TYPE *)data = chg_data->chg_type;
	pr_info("%s: chg_type = %d\n", __func__, chg_data->chg_type);
	return ret;
}

static int mt6370_enable_charging(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;
	bool en = *((bool *)data);

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);

	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
		(chg_data->chip, MT6370_PMU_REG_CHGCTRL2, MT6370_MASK_CHG_EN);

	return ret;
}

static int mt6370_enable_safety_timer(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;
	bool en = *((bool *)data);

	ret = _mt6370_enable_safety_timer(chg_data, en);

	return ret;
}

static int mt6370_is_safety_timer_enable(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	ret = mt6370_pmu_reg_test_bit(chg_data->chip, MT6370_PMU_REG_CHGCTRL12,
		MT6370_SHIFT_TMR_EN, data);
	if (ret < 0)
		return ret;

	return ret;
}

static int mt6370_enable_power_path(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;
	bool en = *((bool *)data);
	u32 mivr = en ? 4500000 : MT6370_MIVR_MAX;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);

	/*
	 * enable power path -> unmask mivr irq
	 * mask mivr irq -> disable power path
	 */
	if (!en)
		ret = mt6370_pmu_reg_set_bit(chg_data->chip,
			MT6370_PMU_CHGMASK1, MT6370_MASK_CHG_MIVRM);

	ret = _mt6370_set_mivr(chg_data, mivr);

	if (en)
		ret = mt6370_pmu_reg_clr_bit(chg_data->chip,
			MT6370_PMU_CHGMASK1, MT6370_MASK_CHG_MIVRM);

	return ret;
}

static int mt6370_is_power_path_enable(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;
	u32 mivr = 0;

	ret = mt6370_get_mivr(chg_data, &mivr);
	*((bool *)data) = (mivr == MT6370_MIVR_MAX ? false : true);

	return ret;
}

static int mt6370_get_ichg(struct mtk_charger_info *mchr_info, void *data)
{
	int ret = 0;
	u8 reg_ichg = 0;
	u32 ichg = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	ret = mt6370_pmu_reg_read(chg_data->chip, MT6370_PMU_REG_CHGCTRL7);
	if (ret < 0)
		return ret;

	reg_ichg = (ret & MT6370_MASK_ICHG) >> MT6370_SHIFT_ICHG;
	ichg = mt6370_find_closest_real_value(MT6370_ICHG_MIN, MT6370_ICHG_MAX,
		MT6370_ICHG_STEP, reg_ichg);

	/* MTK's current unit : 10uA */
	/* Our current unit : uA */
	ichg /= 10;
	*((u32 *)data) = ichg;

	return ret;
}

static int mt6370_set_ichg(struct mtk_charger_info *mchr_info, void *data)
{
	int ret = 0;
	u32 uA = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	/* MTK's current unit : 10uA */
	/* Our current unit : uA */
	uA = *((u32 *)data) * 10;

	ret = _mt6370_set_ichg(chg_data, uA);

	return ret;
}

static int mt6370_get_aicr(struct mtk_charger_info *mchr_info, void *data)
{
	int ret = 0;
	u8 reg_aicr = 0;
	u32 aicr = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	ret = mt6370_pmu_reg_read(chg_data->chip, MT6370_PMU_REG_CHGCTRL3);
	if (ret < 0)
		return ret;

	reg_aicr = (ret & MT6370_MASK_AICR) >> MT6370_SHIFT_AICR;
	aicr = mt6370_find_closest_real_value(MT6370_AICR_MIN, MT6370_AICR_MAX,
		MT6370_AICR_STEP, reg_aicr);

	/* MTK's current unit : 10uA */
	/* Our current unit : uA */
	aicr /= 10;
	*((u32 *)data) = aicr;

	return ret;
}

static int mt6370_set_aicr(struct mtk_charger_info *mchr_info, void *data)
{
	int ret = 0;
	u32 uA = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	/* MTK's current unit : 10uA */
	/* Our current unit : uA */
	uA = *((u32 *)data) * 10;

	ret = _mt6370_set_aicr(chg_data, uA);

	return ret;
}

static int mt6370_set_mivr(struct mtk_charger_info *mchr_info, void *data)
{
	int ret = 0;
	u32 uV = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;
	bool en = true;

	ret = mt6370_is_power_path_enable(mchr_info, &en);
	if (!en) {
		dev_err(chg_data->dev, "%s: power path is disabled\n",
			__func__);
		return -EINVAL;
	}

	/* MTK's current unit : mV */
	/* Our current unit : uV */
	uV = *((u32 *)data) * 1000;

	ret = _mt6370_set_mivr(chg_data, uV);

	return ret;
}

static int mt6370_get_cv(struct mtk_charger_info *mchr_info, void *data)
{
	int ret = 0;
	u8 reg_cv = 0;
	u32 cv = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	ret = mt6370_pmu_reg_read(chg_data->chip, MT6370_PMU_REG_CHGCTRL4);
	if (ret < 0)
		return ret;

	reg_cv = (ret & MT6370_MASK_BAT_VOREG) >> MT6370_SHIFT_BAT_VOREG;

	cv = mt6370_find_closest_real_value(
		MT6370_BAT_VOREG_MIN,
		MT6370_BAT_VOREG_MAX,
		MT6370_BAT_VOREG_STEP,
		reg_cv
	);

	*((u32 *)data) = cv;


	return ret;
}

static int mt6370_set_cv(struct mtk_charger_info *mchr_info, void *data)
{
	int ret = 0;
	u32 uV = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	uV = *((u32 *)data);

	ret = _mt6370_set_cv(chg_data, uV);

	return ret;
}

static int mt6370_set_otg_current_limit(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	u8 reg_ilimit = 0;
	u32 uA = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	/* MTK's current unit : mA */
	/* Our current unit : uA */
	uA = *((u32 *)data) * 1000;

	reg_ilimit = mt6370_find_closest_reg_value_via_table(
		mt6370_otg_oc_threshold,
		ARRAY_SIZE(mt6370_otg_oc_threshold),
		uA
	);

	dev_info(chg_data->dev, "%s: ilimit = %d (0x%02X)\n", __func__, uA,
		reg_ilimit);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGCTRL10,
		MT6370_MASK_BOOST_OC,
		reg_ilimit << MT6370_SHIFT_BOOST_OC
	);

	return ret;
}

static int mt6370_enable_otg(struct mtk_charger_info *mchr_info, void *data)
{
	int ret = 0;
	bool en = *((bool *)data);
	bool en_otg = false;
	u32 current_limit = 500; /* mA */
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;
	u8 hidden_val = en ? 0x00 : 0x0F;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);

	/* Set OTG_OC to 500mA */
	ret = mt6370_set_otg_current_limit(mchr_info, &current_limit);
	if (ret < 0) {
		dev_err(chg_data->dev, "%s: set otg oc failed\n", __func__);
		return ret;
	}

	/* Turn off USB charger detection/Enable WDT */
	if (en) {
		ret = mt6370_enable_chgdet_flow(chg_data, false);
		if (ret < 0)
			dev_err(chg_data->dev,
				"%s: disable usb chrdet failed\n", __func__);

		if (chg_data->chg_desc->en_otg_wdt) {
			ret = mt6370_enable_wdt(chg_data, true);
			if (ret < 0)
				dev_err(chg_data->dev, "%s: en wdt failed\n",
					__func__);
		}
	}

	/* Switch OPA mode to boost mode */
	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
		(chg_data->chip, MT6370_PMU_REG_CHGCTRL1, MT6370_MASK_OPA_MODE);

	msleep(20);

	if (en) {
		ret = mt6370_pmu_reg_test_bit(chg_data->chip,
			MT6370_PMU_REG_CHGCTRL1,
			MT6370_SHIFT_OPA_MODE, &en_otg);
		if (ret < 0 || !en_otg) {
			dev_err(chg_data->dev, "%s: failed, ret = %d\n",
				__func__, ret);

			/* Disable OTG */
			mt6370_pmu_reg_clr_bit(chg_data->chip,
				MT6370_PMU_REG_CHGCTRL1, MT6370_MASK_OPA_MODE);

			/* Disable WDT */
			ret = mt6370_enable_wdt(chg_data, false);
			if (ret < 0)
				dev_err(chg_data->dev,
					"%s: disable wdt failed\n", __func__);
#ifndef CONFIG_TCPC_CLASS
			ret = mt6370_enable_chgdet_flow(chg_data, true);
			if (ret < 0)
				dev_err(chg_data->dev,
					"%s: en usb chrdet failed\n", __func__);
#endif
			return -EIO;
		}
#ifndef CONFIG_TCPC_CLASS
		mt6370_set_usbsw_state(chg_data, MT6370_USBSW_USB);
#endif
	}

	/*
	 * Woraround reg[0x25] = 0x00 after entering OTG mode
	 * reg[0x25] = 0x0F after leaving OTG mode
	 */
	ret = mt6370_enable_hidden_mode(chg_data, true);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: enter hidden mode failed\n",
			__func__);
	else {
		ret = mt6370_pmu_reg_write(chg_data->chip,
			MT6370_PMU_REG_CHGHIDDENCTRL6, hidden_val);
		if (ret < 0)
			dev_err(chg_data->dev,
				"%s: workaroud failed, ret = %d\n",
				__func__, ret);

		ret = mt6370_enable_hidden_mode(chg_data, false);
		if (ret < 0)
			dev_err(chg_data->dev,
				"%s: exist hidden mode failed\n", __func__);
	}

	/* Disable WDT */
	if (!en) {
		ret = mt6370_enable_wdt(chg_data, false);
		if (ret < 0)
			dev_err(chg_data->dev, "%s: disable wdt failed\n",
				__func__);
	}

	return ret;
}

#if 0
static int mt6370_enable_discharge(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0, i = 0;
	const u32 check_dischg_max = 3;
	bool is_dischg = true;
	bool en = *((bool *)data);
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);

	ret = mt6370_enable_hidden_mode(chg_data, true);
	if (ret < 0)
		return ret;

	/* Set bit2 of reg[0x31] to 1/0 to enable/disable discharging */
	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
		(chg_data->chip, MT6370_PMU_REG_CHGHIDDENCTRL1, 0x04);
	if (ret < 0) {
		dev_err(chg_data->dev, "%s: en = %d failed, ret = %d\n",
			__func__, en, ret);
		return ret;
	}

	if (!en) {
		for (i = 0; i < check_dischg_max; i++) {
			ret = mt6370_pmu_reg_test_bit(chg_data->chip,
				MT6370_PMU_REG_CHGHIDDENCTRL1, 2, &is_dischg);
			if (!is_dischg)
				break;
			ret = mt6370_pmu_reg_clr_bit(chg_data->chip,
				MT6370_PMU_REG_CHGHIDDENCTRL1, 0x04);
		}
		if (i == check_dischg_max)
			dev_err(chg_data->dev,
				"%s: disable discharg failed, ret = %d\n",
				__func__, ret);
	}

	ret = mt6370_enable_hidden_mode(chg_data, false);
	return ret;
}
#endif

static int mt6370_set_pep_current_pattern(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	bool is_increase = *((bool *)data);
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	dev_info(chg_data->dev, "%s: pe1.0 pump_up = %d\n", __func__,
		is_increase);

	/* Set to PE1.0 */
	ret = mt6370_pmu_reg_clr_bit(chg_data->chip, MT6370_PMU_REG_CHGCTRL17,
		MT6370_MASK_PUMPX_20_10);

	/* Set Pump Up/Down */
	ret = (is_increase ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
		(chg_data->chip, MT6370_PMU_REG_CHGCTRL17,
		MT6370_MASK_PUMPX_UP_DN);
	if (ret < 0)
		return ret;

	/* Enable PumpX */
	ret = mt6370_enable_pump_express(chg_data, true);

	return ret;
}

static int mt6370_set_pep20_reset(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	u32 mivr = 4500;	/* mA */
	u32 ichg = 51200;	/* 10uA */
	u32 aicr = 10000;	/* 10uA */

	ret = mt6370_set_mivr(mchr_info, &mivr);
	if (ret < 0)
		return ret;

	ret = mt6370_set_ichg(mchr_info, &ichg);
	if (ret < 0)
		return ret;

	ret = mt6370_set_aicr(mchr_info, &aicr);
	if (ret < 0)
		return ret;

	msleep(250);

	aicr = 70000;	/* 10uA */
	ret = mt6370_set_aicr(mchr_info, &aicr);

	return ret;
}

static int mt6370_set_pep20_current_pattern(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	u8 reg_volt = 0;
	u32 uV = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	/* MTK's current unit : mA */
	/* Our current unit : uA */
	uV = *((u32 *)data) * 1000;

	dev_info(chg_data->dev, "%s: pep2.0  = %d\n", __func__, uV);

	/* Set to PEP2.0 */
	ret = mt6370_pmu_reg_set_bit(chg_data->chip, MT6370_PMU_REG_CHGCTRL17,
		MT6370_MASK_PUMPX_20_10);
	if (ret < 0)
		return ret;

	/* Find register value of target voltage */
	reg_volt = mt6370_find_closest_reg_value(
		MT6370_PEP20_VOLT_MIN,
		MT6370_PEP20_VOLT_MAX,
		MT6370_PEP20_VOLT_STEP,
		MT6370_PEP20_VOLT_NUM,
		uV
	);

	/* Set Voltage */
	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGCTRL17,
		MT6370_MASK_PUMPX_DEC,
		reg_volt << MT6370_SHIFT_PUMPX_DEC
	);
	if (ret < 0)
		return ret;

	/* Enable PumpX */
	ret = mt6370_enable_pump_express(chg_data, true);
	ret = (ret >= 0) ? 0 : ret;

	return ret;
}

static int mt6370_set_pep20_efficiency_table(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	pep20_profile_t *profile = (pep20_profile_t *)data;

	profile[0].vchr = 8000;
	profile[1].vchr = 8000;
	profile[2].vchr = 8000;
	profile[3].vchr = 8500;
	profile[4].vchr = 8500;
	profile[5].vchr = 8500;
	profile[6].vchr = 9000;
	profile[7].vchr = 9000;
	profile[8].vchr = 9500;
	profile[9].vchr = 9500;

	return ret;
}

#if 0
static int mt6370_enable_cable_drop_comp(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	bool en = *((bool *)data);
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);

	/* Set to PEP2.0 */
	ret = mt6370_pmu_reg_set_bit(chg_data->chip, MT6370_PMU_REG_CHGCTRL17,
		MT6370_MASK_PUMPX_20_10);
	if (ret < 0)
		return ret;

	/* Set Voltage */
	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGCTRL17,
		MT6370_MASK_PUMPX_DEC,
		0x1F << MT6370_SHIFT_PUMPX_DEC
	);
	if (ret < 0)
		return ret;

	/* Enable PumpX */
	ret = mt6370_enable_pump_express(chg_data, true);

	return ret;
}
#endif

static int mt6370_is_charging_done(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	enum mt6370_charging_status chg_stat = MT6370_CHG_STATUS_READY;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	ret = mt6370_get_charging_status(chg_data, &chg_stat);
	if (ret < 0)
		return ret;

	/* Return is charging done or not */
	switch (chg_stat) {
	case MT6370_CHG_STATUS_READY:
	case MT6370_CHG_STATUS_PROGRESS:
	case MT6370_CHG_STATUS_FAULT:
		*((u32 *)data) = false;
		break;
	case MT6370_CHG_STATUS_DONE:
		*((u32 *)data) = true;
		break;
	default:
		*((u32 *)data) = false;
		break;
	}

	return 0;
}

static int mt6370_kick_wdt(struct mtk_charger_info *mchr_info, void *data)
{
	/* Any I2C communication can kick watchdog timer */
	int ret = 0;
	enum mt6370_charging_status chg_status;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	ret = mt6370_get_charging_status(chg_data, &chg_status);

	return ret;
}

static int mt6370_enable_direct_charge(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	bool en = *((bool *)data);
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);

	if (en) {
		ret = mt6370_pmu_reg_set_bit(chg_data->chip,
			MT6370_PMU_CHGMASK1, MT6370_MASK_CHG_MIVRM);
		if (ret < 0)
			dev_err(chg_data->dev, "%s: mask MIVR IRQ failed\n",
				__func__);

		/* Enable bypass mode */
		ret = mt6370_pmu_reg_set_bit(chg_data->chip,
			MT6370_PMU_REG_CHGCTRL2, MT6370_MASK_BYPASS_MODE);
		if (ret < 0) {
			dev_err(chg_data->dev, "%s: en bypass mode failed\n",
				__func__);
			goto out;
		}

		/* VG_EN = 1 */
		ret = mt6370_pmu_reg_set_bit(chg_data->chip,
			MT6370_PMU_REG_CHGPUMP, MT6370_MASK_VG_EN);
		if (ret < 0) {
			dev_err(chg_data->dev, "%s: en VG_EN failed\n",
				__func__);
			goto disable_bypass;
		}

		return ret;
	}

	/* Disable direct charge */
	/* VG_EN = 0 */
	ret = mt6370_pmu_reg_clr_bit(chg_data->chip, MT6370_PMU_REG_CHGPUMP,
		MT6370_MASK_VG_EN);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: disable VG_EN failed\n", __func__);

disable_bypass:
	/* Disable bypass mode */
	ret = mt6370_pmu_reg_clr_bit(chg_data->chip, MT6370_PMU_REG_CHGCTRL2,
		MT6370_MASK_BYPASS_MODE);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: disable bypass mode failed\n",
			__func__);

out:
	/* Unmask MIVR IRQ */
	ret = mt6370_pmu_reg_clr_bit(chg_data->chip,
		MT6370_PMU_CHGMASK1, MT6370_MASK_CHG_MIVRM);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: unmask MIVR IRQ failed\n",
			__func__);
	return ret;
}

static int mt6370_enable_dc_vbusov(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	bool en = *((bool *)data);
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
		(chg_data->chip, MT6370_PMU_REG_CHGDIRCHG3,
		MT6370_MASK_DC_VBUSOV_EN);

	return ret;
}

static int mt6370_set_dc_vbusov(struct mtk_charger_info *mchr_info, void *data)
{
	int ret = 0;
	u8 reg_vbusov = 0;
	u32 uV = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	/* MTK's current unit : mV */
	/* Our current unit : uV */
	uV = *((u32 *)data) * 1000;

	reg_vbusov = mt6370_find_closest_reg_value(
		MT6370_DC_VBUSOV_LVL_MIN,
		MT6370_DC_VBUSOV_LVL_MAX,
		MT6370_DC_VBUSOV_LVL_STEP,
		MT6370_DC_VBUSOV_LVL_NUM,
		uV
	);

	dev_info(chg_data->dev, "%s: vbusov = %d (0x%02X)\n", __func__, uV,
		reg_vbusov);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGDIRCHG3,
		MT6370_MASK_DC_VBUSOV_LVL,
		reg_vbusov << MT6370_SHIFT_DC_VBUSOV_LVL
	);

	return ret;
}

static int mt6370_enable_dc_ibusoc(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	bool en = *((bool *)data);
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);
	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
		(chg_data->chip, MT6370_PMU_REG_CHGDIRCHG1,
		MT6370_MASK_DC_IBUSOC_EN);

	return ret;
}

static int mt6370_set_dc_ibusoc(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	u8 reg_ibusoc = 0;
	u32 uA = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	/* MTK's current unit : mA */
	/* Our current unit : uA */
	uA = *((u32 *)data) * 1000;

	reg_ibusoc = mt6370_find_closest_reg_value(
		MT6370_DC_IBUSOC_LVL_MIN,
		MT6370_DC_IBUSOC_LVL_MAX,
		MT6370_DC_IBUSOC_LVL_STEP,
		MT6370_DC_IBUSOC_LVL_NUM,
		uA
	);

	dev_info(chg_data->dev, "%s: ibusoc = %d (0x%02X)\n", __func__, uA,
		reg_ibusoc);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGDIRCHG1,
		MT6370_MASK_DC_IBUSOC_LVL,
		reg_ibusoc << MT6370_SHIFT_DC_IBUSOC_LVL
	);

	return ret;
}

static int mt6370_enable_dc_vbatov(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	bool en = *((bool *)data);
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);

	ret = (en ? mt6370_pmu_reg_set_bit : mt6370_pmu_reg_clr_bit)
		(chg_data->chip, MT6370_PMU_REG_CHGDIRCHG1,
		MT6370_MASK_DC_VBATOV_EN);

	return ret;
}

static int mt6370_set_dc_vbatov(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0, i = 0;
	u8 reg_vbatov = 0;
	u32 cv = 0, vbatov = 0;
	u32 uV = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	/* MTK's current unit : mV */
	/* Our current unit : uV */
	uV = *((u32 *)data) * 1000;

	ret = mt6370_get_cv(mchr_info, &cv);
	if (ret < 0) {
		dev_err(chg_data->dev, "%s: get voreg failed\n", __func__);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(mt6370_dc_vbatov_lvl); i++) {
		vbatov = (mt6370_dc_vbatov_lvl[i] * cv) / 100;

		/* Choose closest level */
		if (uV <= vbatov) {
			reg_vbatov = i;
			break;
		}
	}
	if (i == ARRAY_SIZE(mt6370_dc_vbatov_lvl))
		reg_vbatov = i;

	dev_info(chg_data->dev, "%s: vbatov = %dmV (0x%02X)\n", __func__,
		uV / 1000, reg_vbatov);

	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_CHGDIRCHG1,
		MT6370_MASK_DC_VBATOV_LVL,
		reg_vbatov << MT6370_SHIFT_DC_VBATOV_LVL
	);

	return ret;
}

static int mt6370_is_dc_enable(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	ret = mt6370_pmu_reg_test_bit(chg_data->chip, MT6370_PMU_REG_CHGPUMP,
		MT6370_SHIFT_VG_EN, data);

	return ret;
}

static int mt6370_kick_dc_wdt(struct mtk_charger_info *mchr_info, void *data)
{
	/* Any I2C communication can reset watchdog timer */
	int ret = 0;
	enum mt6370_charging_status chg_status;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	ret = mt6370_get_charging_status(chg_data, &chg_status);

	return ret;
}

static int mt6370_get_tchg(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0, adc_temp = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	/* Get value from ADC */
	ret = mt6370_get_adc(chg_data, MT6370_ADC_TEMP_JC, &adc_temp);
	if (ret < 0)
		return ret;

	((int *)data)[0] = adc_temp;
	((int *)data)[1] = adc_temp;

	dev_info(chg_data->dev, "%s: tchg = %d\n", __func__, adc_temp);

	return ret;
}

static int mt6370_get_ibus(struct mtk_charger_info *mchr_info, void *data)
{
	int ret = 0, adc_ibus = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	/* Get value from ADC */
	ret = mt6370_get_adc(chg_data, MT6370_ADC_IBUS, &adc_ibus);
	if (ret < 0)
		return ret;

	*((u32 *)data) = adc_ibus / 1000;

	dev_info(chg_data->dev, "%s: ibus = %dmA\n", __func__, adc_ibus / 1000);
	return ret;
}

static int mt6370_get_vbus(struct mtk_charger_info *mchr_info, void *data)
{
	int ret = 0, adc_vbus = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	/* Get value from ADC */
	ret = mt6370_get_adc(chg_data, MT6370_ADC_VBUS_DIV2, &adc_vbus);
	if (ret < 0)
		return ret;

	*((u32 *)data) = adc_vbus /  1000;

	dev_info(chg_data->dev, "%s: vbus = %dmV\n", __func__, adc_vbus / 1000);
	return ret;
}

static int mt6370_dump_register(struct mtk_charger_info *mchr_info,
	void *data)
{
	int i = 0, ret = 0;
	u32 ichg = 0, aicr = 0, mivr = 0, ieoc = 0, cv = 0;
	bool chg_en = 0;
	int adc_vsys = 0, adc_vbat = 0, adc_ibat = 0, adc_ibus = 0;
	enum mt6370_charging_status chg_status = MT6370_CHG_STATUS_READY;
	u8 chg_stat = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	ret = mt6370_get_ichg(mchr_info, &ichg); /* 10uA */
	ret = mt6370_get_aicr(mchr_info, &aicr); /* 10uA */
	ret = mt6370_get_charging_status(chg_data, &chg_status);
	ret = mt6370_get_ieoc(chg_data, &ieoc);
	ret = mt6370_get_mivr(chg_data, &mivr);
	ret = mt6370_get_cv(mchr_info, &cv);
	ret = mt6370_is_charging_enable(chg_data, &chg_en);
	ret = mt6370_get_adc(chg_data, MT6370_ADC_VSYS, &adc_vsys);
	ret = mt6370_get_adc(chg_data, MT6370_ADC_VBAT, &adc_vbat);
	ret = mt6370_get_adc(chg_data, MT6370_ADC_IBAT, &adc_ibat);
	ret = mt6370_get_adc(chg_data, MT6370_ADC_IBUS, &adc_ibus);

	chg_stat = mt6370_pmu_reg_read(chg_data->chip, MT6370_PMU_REG_CHGSTAT1);

	if (chg_status == MT6370_CHG_STATUS_FAULT) {
		for (i = 0; i < ARRAY_SIZE(mt6370_chg_reg_addr); i++) {
			ret = mt6370_pmu_reg_read(chg_data->chip,
				mt6370_chg_reg_addr[i]);
			if (ret < 0)
				return ret;

			dev_dbg(chg_data->dev, "%s: reg[0x%02X] = 0x%02X\n",
				__func__, mt6370_chg_reg_addr[i], ret);
		}
	}

	dev_info(chg_data->dev,	"%s: ICHG = %dmA, AICR = %dmA, MIVR = %dmV,
		IEOC = %dmA, CV = %dmV\n", __func__, ichg / 100, aicr / 100,
		mivr / 1000, ieoc / 1000, cv / 1000);

	dev_info(chg_data->dev,
		"%s: VSYS = %dmV, VBAT = %dmV, IBAT = %dmA, IBUS = %dmA\n",
		__func__, adc_vsys / 1000, adc_vbat / 1000, adc_ibat / 1000,
		adc_ibus / 1000);

	dev_info(chg_data->dev, "%s: CHG_EN = %d, CHG_STATUS = %s,
		CHG_STAT = 0x%02X\n", __func__, chg_en,
		mt6370_chg_status_name[chg_status], chg_stat);

	ret = 0;
	return ret;
}

static int mt6370_enable_chg_type_det(struct mtk_charger_info *mchr_info,
	void *data)
{
	int ret = 0;

#ifdef CONFIG_TCPC_CLASS
	bool en = *(bool *)data;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)mchr_info;

	dev_info(chg_data->dev, "%s: en = %d\n", __func__, en);

	/* TypeC attach, clear attachi event */
	if (en)
		mt6370_chg_irq_clr_flag(chg_data,
			&chg_data->irq_flag[MT6370_CHG_IRQIDX_QCIRQ],
			MT6370_MASK_ATTACHI);
	else { /* TypeC detach */
		ret = mt6370_enable_ilim(chg_data, true);
		if (ret < 0)
			dev_err(chg_data->dev, "%s: en ilim failed\n",
				__func__);

		chg_data->chg_type = CHARGER_UNKNOWN;
	}

	/* No matter plug in/out, make usb switch to MT6370 */
	mt6370_set_usbsw_state(chg_data, MT6370_USBSW_CHG);
	ret = mt6370_enable_chgdet_flow(chg_data, en);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: en = %d, failed\n", __func__, en);
#endif

	return ret;
}

#ifdef MT6370_APPLE_SAMSUNG_TA_SUPPORT
static int mt6370_detect_apple_samsung_ta(
	struct mt6370_pmu_charger_data *chg_data)
{
	int ret = 0;
	bool dcd_timeout = false;
	bool dp_0_9v = false, dp_1_5v = false, dp_2_3v = false, dm_2_3v = false;

	/* Only SDP/CDP/DCP could possibly be Apple/Samsung TA */
	if (chg_data->chg_type != STANDARD_HOST &&
	    chg_data->chg_type != CHARGING_HOST &&
	    chg_data->chg_type != STANDARD_CHARGER)
		return -EINVAL;

	if (chg_data->chg_type == STANDARD_HOST ||
	    chg_data->chg_type == CHARGING_HOST) {
		ret = mt6370_pmu_reg_test_bit(chg_data->chip,
			MT6370_PMU_REG_QCSTAT, MT6370_SHIFT_DCDTI_STAT,
			&dcd_timeout);
		if (ret < 0) {
			dev_err(chg_data->dev, "%s: read dcd timeout failed\n",
				__func__);
			return ret;
		}

		if (!dcd_timeout) {
			dev_info(chg_data->dev, "%s: dcd is not timeout\n",
				__func__);
			return 0;
		}
	}

	/* Check DP > 0.9V */
	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_QCSTATUS2,
		0x03,
		0x03
	);

	ret = mt6370_pmu_reg_test_bit(chg_data->chip, MT6370_PMU_REG_QCSTATUS2,
		4, &dp_0_9v);
	if (ret < 0)
		return ret;

	if (!dp_0_9v) {
		dev_info(chg_data->dev, "%s: DP < 0.9V\n", __func__);
		return ret;
	}

	ret = mt6370_pmu_reg_test_bit(chg_data->chip, MT6370_PMU_REG_QCSTATUS2,
		5, &dp_1_5v);
	if (ret < 0)
		return ret;

	/* Samsung charger */
	if (!dp_1_5v) {
		dev_info(chg_data->dev, "%s: 0.9V < DP < 1.5V\n", __func__);
		chg_data->chg_type = SAMSUNG_CHARGER;
		return ret;
	}

	/* Check DP > 2.3 V */
	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_QCSTATUS2,
		0x0B,
		0x0B
	);
	ret = mt6370_pmu_reg_test_bit(chg_data->chip, MT6370_PMU_REG_QCSTATUS2,
		5, &dp_2_3v);
	if (ret < 0)
		return ret;

	/* Check DM > 2.3V */
	ret = mt6370_pmu_reg_update_bits(
		chg_data->chip,
		MT6370_PMU_REG_QCSTATUS2,
		0x0F,
		0x0F
	);
	ret = mt6370_pmu_reg_test_bit(chg_data->chip, MT6370_PMU_REG_QCSTATUS2,
		5, &dm_2_3v);
	if (ret < 0)
		return ret;

	/* Apple charger */
	if (!dp_2_3v && !dm_2_3v) {
		dev_info(chg_data->dev, "%s: 1.5V < DP < 2.3V && DM < 2.3V\n",
			__func__);
		chg_data->chg_type = APPLE_0_5A_CHARGER;
	} else if (!dp_2_3v && dm_2_3v) {
		dev_info(chg_data->dev, "%s: 1.5V < DP < 2.3V && 2.3V < DM\n",
			__func__);
		chg_data->chg_type = APPLE_1_0A_CHARGER;
	} else if (dp_2_3v && !dm_2_3v) {
		dev_info(chg_data->dev, "%s: 2.3V < DP && DM < 2.3V\n",
			__func__);
		chg_data->chg_type = APPLE_2_1A_CHARGER;
	} else {
		dev_info(chg_data->dev, "%s: 2.3V < DP && 2.3V < DM\n",
			__func__);
		chg_data->chg_type = APPLE_2_4A_CHARGER;
	}

	return 0;
}
#endif

static irqreturn_t mt6370_pmu_chg_treg_irq_handler(int irq, void *data)
{
	int ret = 0;
	bool treg_stat = false;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_err(chg_data->dev, "%s\n", __func__);

	/* Read treg status */
	ret = mt6370_pmu_reg_test_bit(chg_data->chip, MT6370_PMU_REG_CHGSTAT1,
		MT6370_SHIFT_CHG_TREG, &treg_stat);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: read treg stat failed\n", __func__);
	else
		dev_err(chg_data->dev, "%s: treg stat = %d\n", __func__,
			treg_stat);

	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_aicr_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_mivr_irq_handler(int irq, void *data)
{
	int ret = 0;
	bool mivr_stat = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	ret = mt6370_pmu_reg_test_bit(chg_data->chip, MT6370_PMU_REG_CHGSTAT1,
		MT6370_SHIFT_MIVR_STAT, &mivr_stat);
	if (ret < 0) {
		dev_err(chg_data->dev, "%s: read mivr stat failed\n", __func__);
		goto out;
	}

	if (!mivr_stat) {
		dev_info(chg_data->dev, "%s: mivr stat not act\n", __func__);
		goto out;
	}

	/* Disable MIVR IRQ */
	ret = mt6370_pmu_reg_set_bit(chg_data->chip,
		MT6370_PMU_CHGMASK1, MT6370_MASK_CHG_MIVRM);
	if (ret < 0) {
		dev_err(chg_data->dev, "%s: disable mivr IRQ failed\n",
			__func__);
		goto out;
	}

	mt6370_chg_irq_set_flag(chg_data,
		&chg_data->irq_flag[MT6370_CHG_IRQIDX_CHGIRQ1],
		MT6370_MASK_CHG_MIVR);

out:
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_pwr_rdy_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_vinovp_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_vsysuv_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_vsysov_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_vbatov_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_vbusov_irq_handler(int irq, void *data)
{
	int ret = 0;
	bool vbusov_stat = false;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_err(chg_data->dev, "%s\n", __func__);
	ret = mt6370_pmu_reg_test_bit(chg_data->chip, MT6370_PMU_REG_CHGSTAT2,
		MT6370_SHIFT_CHG_VBUSOV_STAT, &vbusov_stat);
	if (ret < 0)
		return IRQ_HANDLED;
	dev_info(chg_data->dev, "%s: stat = %d\n", __func__, vbusov_stat);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_ts_bat_cold_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_ts_bat_cool_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_ts_bat_warm_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_ts_bat_hot_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_tmri_irq_handler(int irq, void *data)
{
	int ret = 0;
	bool tmr_stat = false;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_err(chg_data->dev, "%s\n", __func__);
	ret = mt6370_pmu_reg_test_bit(chg_data->chip, MT6370_PMU_REG_CHGSTAT4,
		MT6370_SHIFT_CHG_TMRI_STAT, &tmr_stat);
	if (ret < 0)
		return IRQ_HANDLED;

	dev_info(chg_data->dev, "%s: stat = %d\n", __func__, tmr_stat);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_batabsi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_adpbadi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_rvpi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_otpi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_aiclmeasi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	mt6370_chg_irq_set_flag(chg_data,
		&chg_data->irq_flag[MT6370_CHG_IRQIDX_CHGIRQ5],
		MT6370_MASK_CHG_AICLMEASI);

	wake_up_interruptible(&chg_data->wait_queue);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_ichgmeasi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chgdet_donei_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_wdtmri_irq_handler(int irq, void *data)
{
	int ret = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	ret = mt6370_kick_wdt(&chg_data->mchr_info, NULL);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: kick wdt failed\n", __func__);

	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_ssfinishi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);

	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_rechgi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_termi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chg_ieoci_irq_handler(int irq, void *data)
{
	int ret = 0;
	bool ieoc_stat = false;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	ret = mt6370_pmu_reg_test_bit(chg_data->chip, MT6370_PMU_REG_CHGSTAT5,
		MT6370_SHIFT_CHG_IEOCI_STAT, &ieoc_stat);
	if (ret < 0)
		return IRQ_HANDLED;

	dev_info(chg_data->dev, "%s: stat = %d\n", __func__, ieoc_stat);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_adc_donei_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_pumpx_donei_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_bst_batuvi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_bst_vbusovi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_bst_olpi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_attachi_irq_handler(int irq, void *data)
{
	int ret = 0;
	u8 usb_status = 0;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);

	/* Check bc12 enable flag */
	mutex_lock(&chg_data->bc12_access_lock);
	if (!chg_data->bc12_en) {
		dev_err(chg_data->dev, "%s: bc12 disabled, ignore irq\n",
			__func__);
		mutex_unlock(&chg_data->bc12_access_lock);
		return IRQ_HANDLED;
	}
	mutex_unlock(&chg_data->bc12_access_lock);

	ret = mt6370_pmu_reg_read(chg_data->chip, MT6370_PMU_REG_USBSTATUS1);
	if (ret < 0) {
		dev_err(chg_data->dev, "%s: read charger type failed\n",
			__func__);
		return IRQ_HANDLED;
	}
	usb_status = (ret & MT6370_MASK_USB_STATUS) >> MT6370_SHIFT_USB_STATUS;

	chg_data->chg_online = true;
	switch (usb_status) {
	case MT6370_CHG_TYPE_SDP:
		chg_data->chg_type = STANDARD_HOST;
		break;
	case MT6370_CHG_TYPE_SDPNSTD:
		chg_data->chg_type = NONSTANDARD_CHARGER;
		break;
	case MT6370_CHG_TYPE_CDP:
		chg_data->chg_type = CHARGING_HOST;
		break;
	case MT6370_CHG_TYPE_DCP:
		chg_data->chg_type = STANDARD_CHARGER;
		break;
	default:
		chg_data->chg_type = CHARGER_UNKNOWN;
		break;
	}

#ifdef MT6370_APPLE_SAMSUNG_TA_SUPPORT
	ret = mt6370_detect_apple_samsung_ta(chg_data);
	if (ret < 0)
		dev_err(chg_data->dev,
			"%s: detect apple/samsung ta failed, ret = %d\n",
			__func__, ret);
#endif
	mt6370_chg_irq_set_flag(chg_data,
		&chg_data->irq_flag[MT6370_CHG_IRQIDX_QCIRQ],
		MT6370_MASK_ATTACHI);

	wake_up_interruptible(&chg_data->wait_queue);

	/* Turn off USB charger detection */
	ret = mt6370_enable_chgdet_flow(chg_data, false);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: disable usb chrdet failed\n",
			__func__);

	if (chg_data->chg_type == STANDARD_HOST ||
	    chg_data->chg_type == CHARGING_HOST)
		mt6370_set_usbsw_state(chg_data, MT6370_USBSW_USB);

	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_detachi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_qc30stpdone_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_qc_vbusdet_done_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_hvdcp_det_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_chgdeti_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_dcdti_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_dirchg_vgoki_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_info(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_dirchg_wdtmri_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_dirchg_uci_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_dirchg_oci_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_dirchg_ovi_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_ovpctrl_swon_evt_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_ovpctrl_uvp_d_evt_irq_handler(int irq, void *data)
{
#ifndef CONFIG_TCPC_CLASS
	int ret = 0;
	bool uvp_d_stat = false;
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_err(chg_data->dev, "%s\n", __func__);

	/* Check UVP_D_STAT */
	ret = mt6370_pmu_reg_test_bit(
		chg_data->chip,
		MT6370_PMU_REG_OVPCTRLSTAT,
		MT6370_SHIFT_OVPCTRL_UVP_D_STAT,
		&uvp_d_stat
	);
	if (!uvp_d_stat) {
		dev_info(chg_data->dev, "%s: no uvp_d_stat\n", __func__);
		return IRQ_HANDLED;
	}

	/* Plug out */
	chg_data->chg_online = false;
	chg_data->chg_type = CHARGER_UNKNOWN;
	/* Turn on USB charger detection */
	ret = mt6370_enable_chgdet_flow(chg_data, true);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: en usb chrdet failed\n", __func__);

	/* Switch DPDM to MT6370 */
	mt6370_set_usbsw_state(chg_data, MT6370_USBSW_CHG);

#endif /* CONFIG_TCPC_CLASS */

	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_ovpctrl_uvp_evt_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_ovpctrl_ovp_d_evt_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_ovpctrl_ovp_evt_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_charger_data *chg_data =
		(struct mt6370_pmu_charger_data *)data;

	dev_notice(chg_data->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static struct mt6370_pmu_irq_desc mt6370_chg_irq_desc[] = {
	MT6370_PMU_IRQDESC(chg_treg),
	MT6370_PMU_IRQDESC(chg_aicr),
	MT6370_PMU_IRQDESC(chg_mivr),
	MT6370_PMU_IRQDESC(pwr_rdy),
	MT6370_PMU_IRQDESC(chg_vinovp),
	MT6370_PMU_IRQDESC(chg_vsysuv),
	MT6370_PMU_IRQDESC(chg_vsysov),
	MT6370_PMU_IRQDESC(chg_vbatov),
	MT6370_PMU_IRQDESC(chg_vbusov),
	MT6370_PMU_IRQDESC(ts_bat_cold),
	MT6370_PMU_IRQDESC(ts_bat_cool),
	MT6370_PMU_IRQDESC(ts_bat_warm),
	MT6370_PMU_IRQDESC(ts_bat_hot),
	MT6370_PMU_IRQDESC(chg_tmri),
	MT6370_PMU_IRQDESC(chg_batabsi),
	MT6370_PMU_IRQDESC(chg_adpbadi),
	MT6370_PMU_IRQDESC(chg_rvpi),
	MT6370_PMU_IRQDESC(otpi),
	MT6370_PMU_IRQDESC(chg_aiclmeasi),
	MT6370_PMU_IRQDESC(chg_ichgmeasi),
	MT6370_PMU_IRQDESC(chgdet_donei),
	MT6370_PMU_IRQDESC(chg_wdtmri),
	MT6370_PMU_IRQDESC(ssfinishi),
	MT6370_PMU_IRQDESC(chg_rechgi),
	MT6370_PMU_IRQDESC(chg_termi),
	MT6370_PMU_IRQDESC(chg_ieoci),
	MT6370_PMU_IRQDESC(adc_donei),
	MT6370_PMU_IRQDESC(pumpx_donei),
	MT6370_PMU_IRQDESC(bst_batuvi),
	MT6370_PMU_IRQDESC(bst_vbusovi),
	MT6370_PMU_IRQDESC(bst_olpi),
	MT6370_PMU_IRQDESC(attachi),
	MT6370_PMU_IRQDESC(detachi),
	MT6370_PMU_IRQDESC(qc30stpdone),
	MT6370_PMU_IRQDESC(qc_vbusdet_done),
	MT6370_PMU_IRQDESC(hvdcp_det),
	MT6370_PMU_IRQDESC(chgdeti),
	MT6370_PMU_IRQDESC(dcdti),
	MT6370_PMU_IRQDESC(dirchg_vgoki),
	MT6370_PMU_IRQDESC(dirchg_wdtmri),
	MT6370_PMU_IRQDESC(dirchg_uci),
	MT6370_PMU_IRQDESC(dirchg_oci),
	MT6370_PMU_IRQDESC(dirchg_ovi),
	MT6370_PMU_IRQDESC(ovpctrl_swon_evt),
	MT6370_PMU_IRQDESC(ovpctrl_uvp_d_evt),
	MT6370_PMU_IRQDESC(ovpctrl_uvp_evt),
	MT6370_PMU_IRQDESC(ovpctrl_ovp_d_evt),
	MT6370_PMU_IRQDESC(ovpctrl_ovp_evt),
};

static void mt6370_pmu_charger_irq_register(struct platform_device *pdev)
{
	struct resource *res;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mt6370_chg_irq_desc); i++) {
		if (!mt6370_chg_irq_desc[i].name)
			continue;
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
					mt6370_chg_irq_desc[i].name);
		if (!res)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, res->start, NULL,
					mt6370_chg_irq_desc[i].irq_handler,
					IRQF_TRIGGER_FALLING,
					mt6370_chg_irq_desc[i].name,
					platform_get_drvdata(pdev));
		if (ret < 0) {
			dev_err(&pdev->dev, "request %s irq fail\n", res->name);
			continue;
		}
		mt6370_chg_irq_desc[i].irq = res->start;
	}
}

static inline int mt_parse_dt(struct device *dev,
	struct mt6370_pmu_charger_data *chg_data)
{
	struct mt6370_pmu_charger_desc *chg_desc = NULL;
	struct device_node *np = dev->of_node;
	struct i2c_client *i2c = chg_data->chip->i2c;

	dev_info(chg_data->dev, "%s\n", __func__);
	chg_data->chg_desc = &mt6370_default_chg_desc;

	chg_desc = devm_kzalloc(&i2c->dev,
		sizeof(struct mt6370_pmu_charger_desc), GFP_KERNEL);
	if (!chg_desc)
		return -ENOMEM;
	memcpy(chg_desc, &mt6370_default_chg_desc,
		sizeof(struct mt6370_pmu_charger_desc));

	if (of_property_read_string(np, "charger_name",
		&(chg_data->mchr_info.name)) < 0) {
		dev_err(chg_data->dev, "%s: no charger name\n", __func__);
		chg_data->mchr_info.name = "mt6370_charger";
	}

	if (of_property_read_u32(np, "ichg", &chg_desc->ichg) < 0)
		dev_err(chg_data->dev, "%s: no ichg\n", __func__);

	if (of_property_read_u32(np, "aicr", &chg_desc->aicr) < 0)
		dev_err(chg_data->dev, "%s: no aicr\n", __func__);

	if (of_property_read_u32(np, "mivr", &chg_desc->mivr) < 0)
		dev_err(chg_data->dev, "%s: no mivr\n", __func__);

	if (of_property_read_u32(np, "cv", &chg_desc->cv) < 0)
		dev_err(chg_data->dev, "%s: no cv\n", __func__);

	if (of_property_read_u32(np, "ieoc", &chg_desc->ieoc) < 0)
		dev_err(chg_data->dev, "%s: no ieoc\n", __func__);

	if (of_property_read_u32(np, "safety_timer",
		&chg_desc->safety_timer) < 0)
		dev_err(chg_data->dev, "%s: no safety timer\n", __func__);

	if (of_property_read_u32(np, "dc_wdt", &chg_desc->dc_wdt) < 0)
		dev_err(chg_data->dev, "%s: no dc wdt\n", __func__);

	if (of_property_read_u32(np, "ircmp_resistor",
		&chg_desc->ircmp_resistor) < 0)
		dev_err(chg_data->dev, "%s: no ircmp resistor\n", __func__);

	if (of_property_read_u32(np, "ircmp_vclamp",
		&chg_desc->ircmp_vclamp) < 0)
		dev_err(chg_data->dev, "%s: no ircmp vclamp\n", __func__);

	chg_desc->en_te = of_property_read_bool(np, "enable_te");
	chg_desc->en_wdt = of_property_read_bool(np, "enable_wdt");
	chg_desc->en_otg_wdt = of_property_read_bool(np, "enable_otg_wdt");

	chg_data->chg_desc = chg_desc;

	return 0;
}

static int mt6370_chg_init_setting(struct mt6370_pmu_charger_data *chg_data)
{
	int ret = 0;
	struct mt6370_pmu_charger_desc *chg_desc = chg_data->chg_desc;

	dev_info(chg_data->dev, "%s\n", __func__);

	/* Select IINLMTSEL to use AICR */
	ret = mt6370_select_input_current_limit(chg_data,
		MT6370_IINLMTSEL_AICR);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: select iinlmtsel failed\n",
			__func__);

	mdelay(5);

	/* Disable hardware ILIM */
	ret = mt6370_enable_ilim(chg_data, false);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: disable ilim failed\n", __func__);

	ret = _mt6370_set_ichg(chg_data, chg_desc->ichg);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: set ichg failed\n", __func__);

	ret = _mt6370_set_aicr(chg_data, chg_desc->aicr);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: set aicr failed\n", __func__);

	ret = _mt6370_set_mivr(chg_data, chg_desc->mivr);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: set mivr failed\n", __func__);

	ret = _mt6370_set_cv(chg_data, chg_desc->cv);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: set voreg failed\n", __func__);

	ret = mt6370_set_ieoc(chg_data, chg_desc->ieoc);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: set ieoc failed\n", __func__);

	ret = mt6370_enable_te(chg_data, chg_desc->en_te);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: set te failed\n", __func__);

	ret = mt6370_set_fast_charge_timer(chg_data, chg_desc->safety_timer);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: set fast timer failed\n", __func__);

	ret = mt6370_set_dc_wdt(chg_data, chg_desc->dc_wdt);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: set dc watch dog timer failed\n",
			__func__);

	ret = _mt6370_enable_safety_timer(chg_data, true);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: enable charger timer failed\n",
			__func__);

	/* Initially disable WDT to prevent 1mA power consumption */
	ret = mt6370_enable_wdt(chg_data, false);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: disable watchdog timer failed\n",
			__func__);

	/* Disable JEITA */
	ret = mt6370_enable_jeita(chg_data, false);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: disable jeita failed\n", __func__);

	/* Disable HZ */
	ret = _mt6370_enable_hz(chg_data, false);
	if (ret < 0)
		dev_err(chg_data->dev, "%s: disable hz failed\n", __func__);

	ret = _mt6370_set_ircmp_resistor(chg_data, chg_desc->ircmp_resistor);
	if (ret < 0)
		dev_err(chg_data->dev,
			"%s: set IR compensation resistor failed\n", __func__);

	ret = _mt6370_set_ircmp_vclamp(chg_data, chg_desc->ircmp_vclamp);
	if (ret < 0)
		dev_err(chg_data->dev,
			"%s: set IR compensation vclamp failed\n", __func__);

	/* Disable USB charger type detection */
	ret = mt6370_enable_chgdet_flow(chg_data, false);
	if (ret < 0)
		dev_err(chg_data->dev,
			"%s: disable usb chrdet failed\n", __func__);

	return ret;
}


static const mtk_charger_intf mt6370_mchr_intf[CHARGING_CMD_NUMBER] = {
	[CHARGING_CMD_INIT] = mt6370_hw_init,
	[CHARGING_CMD_DUMP_REGISTER] = mt6370_dump_register,
	[CHARGING_CMD_ENABLE] = mt6370_enable_charging,
	[CHARGING_CMD_SET_HIZ_SWCHR] = mt6370_enable_hz,
	[CHARGING_CMD_ENABLE_SAFETY_TIMER] = mt6370_enable_safety_timer,
	[CHARGING_CMD_ENABLE_OTG] = mt6370_enable_otg,
	[CHARGING_CMD_ENABLE_POWER_PATH] = mt6370_enable_power_path,
	[CHARGING_CMD_ENABLE_DIRECT_CHARGE] = mt6370_enable_direct_charge,
	[CHARGING_CMD_SET_CURRENT] = mt6370_set_ichg,
	[CHARGING_CMD_SET_INPUT_CURRENT] = mt6370_set_aicr,
	[CHARGING_CMD_SET_VINDPM] = mt6370_set_mivr,
	[CHARGING_CMD_SET_CV_VOLTAGE] = mt6370_set_cv,
	[CHARGING_CMD_SET_BOOST_CURRENT_LIMIT] = mt6370_set_otg_current_limit,
	[CHARGING_CMD_SET_TA_CURRENT_PATTERN] = mt6370_set_pep_current_pattern,
	[CHARGING_CMD_SET_TA20_RESET] = mt6370_set_pep20_reset,
	[CHARGING_CMD_SET_TA20_CURRENT_PATTERN] =
		mt6370_set_pep20_current_pattern,
	[CHARGING_CMD_SET_ERROR_STATE] = mt6370_set_error_state,
	[CHARGING_CMD_GET_CURRENT] = mt6370_get_ichg,
	[CHARGING_CMD_GET_INPUT_CURRENT] = mt6370_get_aicr,
	[CHARGING_CMD_GET_CHARGER_TEMPERATURE] = mt6370_get_tchg,
	[CHARGING_CMD_GET_CHARGING_STATUS] = mt6370_is_charging_done,
	[CHARGING_CMD_GET_IS_POWER_PATH_ENABLE] = mt6370_is_power_path_enable,
	[CHARGING_CMD_GET_IS_SAFETY_TIMER_ENABLE] =
		mt6370_is_safety_timer_enable,
	[CHARGING_CMD_RESET_WATCH_DOG_TIMER] = mt6370_kick_wdt,
	[CHARGING_CMD_GET_IBUS] = mt6370_get_ibus,
	[CHARGING_CMD_GET_VBUS] = mt6370_get_vbus,
	[CHARGING_CMD_RUN_AICL] = mt6370_run_aicl,
	[CHARGING_CMD_RESET_DC_WATCH_DOG_TIMER] = mt6370_kick_dc_wdt,
	[CHARGING_CMD_ENABLE_DC_VBUSOV] = mt6370_enable_dc_vbusov,
	[CHARGING_CMD_SET_DC_VBUSOV] = mt6370_set_dc_vbusov,
	[CHARGING_CMD_ENABLE_DC_VBUSOC] = mt6370_enable_dc_ibusoc,
	[CHARGING_CMD_SET_DC_VBUSOC] = mt6370_set_dc_ibusoc,
	[CHARGING_CMD_ENABLE_DC_VBATOV] = mt6370_enable_dc_vbatov,
	[CHARGING_CMD_SET_DC_VBATOV] = mt6370_set_dc_vbatov,
	[CHARGING_CMD_GET_IS_DC_ENABLE] = mt6370_is_dc_enable,
	[CHARGING_CMD_SET_IRCMP_RESISTOR] = mt6370_set_ircmp_resistor,
	[CHARGING_CMD_SET_IRCMP_VOLT_CLAMP] = mt6370_set_ircmp_vclamp,
	[CHARGING_CMD_SET_PEP20_EFFICIENCY_TABLE] =
		mt6370_set_pep20_efficiency_table,
	[CHARGING_CMD_GET_CHARGER_TYPE] = mt6370_get_charger_type,
	[CHARGING_CMD_ENABLE_CHR_TYPE_DET] = mt6370_enable_chg_type_det,

	/*
	 * The following interfaces are not related to charger
	 * Define in mtk_charger_intf.c
	 */
	[CHARGING_CMD_SW_INIT] = mtk_charger_sw_init,
	[CHARGING_CMD_SET_HV_THRESHOLD] = mtk_charger_set_hv_threshold,
	[CHARGING_CMD_GET_HV_STATUS] = mtk_charger_get_hv_status,
	[CHARGING_CMD_GET_BATTERY_STATUS] = mtk_charger_get_battery_status,
	[CHARGING_CMD_GET_CHARGER_DET_STATUS] =
		mtk_charger_get_charger_det_status,
	[CHARGING_CMD_GET_IS_PCM_TIMER_TRIGGER] =
		mtk_charger_get_is_pcm_timer_trigger,
	[CHARGING_CMD_SET_PLATFORM_RESET] = mtk_charger_set_platform_reset,
	[CHARGING_CMD_GET_PLATFORM_BOOT_MODE] =
		mtk_charger_get_platform_boot_mode,
	[CHARGING_CMD_SET_POWER_OFF] = mtk_charger_set_power_off,
	[CHARGING_CMD_GET_POWER_SOURCE] = mtk_charger_get_power_source,
	[CHARGING_CMD_GET_CSDAC_FALL_FLAG] = mtk_charger_get_csdac_full_flag,
	[CHARGING_CMD_DISO_INIT] = mtk_charger_diso_init,
	[CHARGING_CMD_GET_DISO_STATE] = mtk_charger_get_diso_state,
	[CHARGING_CMD_SET_VBUS_OVP_EN] = mtk_charger_set_vbus_ovp_en,
	[CHARGING_CMD_GET_BIF_VBAT] = mtk_charger_get_bif_vbat,
	[CHARGING_CMD_SET_CHRIND_CK_PDN] = mtk_charger_set_chrind_ck_pdn,
	[CHARGING_CMD_GET_BIF_TBAT] = mtk_charger_get_bif_tbat,
	[CHARGING_CMD_SET_DP] = mtk_charger_set_dp,
	[CHARGING_CMD_GET_BIF_IS_EXIST] = mtk_charger_get_bif_is_exist,
};


static int mt6370_pmu_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mt6370_pmu_charger_data *chg_data;
	bool use_dt = pdev->dev.of_node;

	pr_info("%s: (%s)\n", __func__, MT6370_PMU_CHARGER_DRV_VERSION);

	chg_data = devm_kzalloc(&pdev->dev, sizeof(*chg_data), GFP_KERNEL);
	if (!chg_data)
		return -ENOMEM;

	mutex_init(&chg_data->adc_access_lock);
	mutex_init(&chg_data->irq_access_lock);
	mutex_init(&chg_data->aicr_access_lock);
	mutex_init(&chg_data->ichg_access_lock);
	mutex_init(&chg_data->bc12_access_lock);
	chg_data->chip = dev_get_drvdata(pdev->dev.parent);
	chg_data->dev = &pdev->dev;
	chg_data->chg_type = CHARGER_UNKNOWN;

	if (use_dt) {
		ret = mt_parse_dt(&pdev->dev, chg_data);
		if (ret < 0)
			dev_err(chg_data->dev, "%s: parse dts failed\n",
				__func__);
	}
	platform_set_drvdata(pdev, chg_data);

	/* Init wait queue head */
	init_waitqueue_head(&chg_data->wait_queue);

#ifndef CONFIG_TCPC_CLASS
	INIT_WORK(&chg_data->chgdet_work, mt6370_chgdet_work_handler);
#endif
	/* Do initial setting */
	ret = mt6370_chg_init_setting(chg_data);
	if (ret < 0) {
		dev_err(chg_data->dev, "%s: sw init failed\n", __func__);
		goto err_chg_init_setting;
	}

	/* SW workaround */
	ret = mt6370_chg_sw_workaround(chg_data);
	if (ret < 0) {
		dev_err(chg_data->dev, "%s: software workaround failed\n",
			__func__);
		goto err_chg_sw_workaround;
	}

	mt6370_pmu_charger_irq_register(pdev);

	chg_data->mchr_info.mchr_intf = mt6370_mchr_intf;
	mtk_charger_set_info(&chg_data->mchr_info);

	mt6370_dump_register(&chg_data->mchr_info, NULL);

	/* Schedule work for microB's BC1.2 */
#ifndef CONFIG_TCPC_CLASS
	schedule_work(&chg_data->chgdet_work);
#endif
	dev_info(&pdev->dev, "%s successfully\n", __func__);
	return ret;

err_chg_sw_workaround:
err_chg_init_setting:
	mutex_destroy(&chg_data->ichg_access_lock);
	mutex_destroy(&chg_data->adc_access_lock);
	mutex_destroy(&chg_data->irq_access_lock);
	mutex_destroy(&chg_data->aicr_access_lock);
	mutex_destroy(&chg_data->bc12_access_lock);
	return ret;
}

static int mt6370_pmu_charger_remove(struct platform_device *pdev)
{
	struct mt6370_pmu_charger_data *chg_data = platform_get_drvdata(pdev);

	if (chg_data) {
		mutex_destroy(&chg_data->ichg_access_lock);
		mutex_destroy(&chg_data->adc_access_lock);
		mutex_destroy(&chg_data->irq_access_lock);
		mutex_destroy(&chg_data->aicr_access_lock);
		mutex_destroy(&chg_data->bc12_access_lock);
		dev_info(chg_data->dev, "%s successfully\n", __func__);
	}

	return 0;
}

static const struct of_device_id mt_ofid_table[] = {
	{ .compatible = "mediatek,mt6370_pmu_charger", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt_ofid_table);

static const struct platform_device_id mt_id_table[] = {
	{ "mt6370_pmu_charger", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, mt_id_table);

static struct platform_driver mt6370_pmu_charger = {
	.driver = {
		.name = "mt6370_pmu_charger",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt_ofid_table),
	},
	.probe = mt6370_pmu_charger_probe,
	.remove = mt6370_pmu_charger_remove,
	.id_table = mt_id_table,
};
module_platform_driver(mt6370_pmu_charger);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek MT6370 PMU Charger");
MODULE_VERSION(MT6370_PMU_CHARGER_DRV_VERSION);

/*
 * Release Note
 * 1.1.12_MTK
 * (1) Enable charger before sending PE+/PE+20 pattern
 * (2) Select to use reg AICR as input limit -> disable HW limit
 *
 * 1.1.11_MTK
 * (1) Fix get_adc lock unbalance issue
 * (2) Add a flag to check enable status of bc12
 *
 * 1.1.10_MTK
 * (1) Add ext usb switch control
 * (2) Add MT6370_APPLE_SAMSUNG_TA_SUPPORT config
 * (3) Remove some debug log and unncessary code
 *
 * 1.1.9_MTK
 * (1) Use polling mode for ADC done and PE+
 * (2) Return immediately if usb is plugged out while waiting CDP
 *
 * 1.1.8_MTK
 * (1) Read CHG_STAT in dump register
 * (2) Read ZCV before disabling it and add ZCV ops
 * (3) Add AEE log for ADC hang issue
 *
 * 1.1.7_MTK
 * (1) Add a adc flag for adc_done IRQ
 * (2) Not waitting ssfinish IRQ after enabling OTG
 * (3) Add debug information for ADC
 * (4) For degug, read ADC data even if conversation failed
 * (5) Trigger any ADC before disabling ZCV
 *
 * 1.1.6_MTK
 * (1) Read USB STATUS(0x27) instead of device type(0x22)
 *     to check charger type
 * (2) Show CV value in dump_register
 * (3) If PE20/30 is connected, do not run AICL
 * (4) Disable MIVR IRQ -> enable direct charge
 *     Enable MIVR IRQ -> disable direct charge
 *
 * 1.1.5_MTK
 * (1) Modify probe sequence
 * (2) Change ADC log from ratelimited to normal one for debug
 * (3) Polling ADC_START after receiving ADC_DONE irq
 * (4) Disable ZCV in probe function
 * (5) Plug out -> enable ILIM_EN
 *     Plug in -> sleep 1200ms -> disable ILIM_EN
 *
 * 1.1.4_MTK
 * (1) Add IEOC workaround
 *
 * 1.1.3_MTK
 * (1) Enable safety timer, rechg, vbusov, eoc IRQs
 *     and notify charger manager if event happens
 * (2) Modify IBAT/IBUS ADC's coefficient
 * (3) Change dev_dbg to dev_dbg_ratelimited
 *
 * 1.1.2_MTK
 * (1) Enable charger in plug in callback
 * (2) Enable power path -> Enable MIVR IRQ,
 *     Disable MIVR IRQ -> Disable power path
 * (3) Change "PrimarySWCHG" to "primary_chg"
 *     Change "load_switch" to "primary_load_switch"
 * (4) Check ADC_START bit if ADC_DONE IRQ timeout
 *
 * 1.1.1_MTK
 * (1) Shorten enable to en
 * (2) Enable WDT for charging/OTG mode, disable WDT when no cable plugged in
 * (3) Use dev_ series instead pr_ series to dump log
 * (4) Do AICL in a workqueue after receiving MIVR IRQ
 *
 * 1.1.0_MTK
 * (1) Add a load switch device to support PE30
 *
 * 1.0.9_MTK
 * (1) Report charger online to charger type detection
 * (2) Use MIVR to enable/disable power path
 * (3) Release PE+20 efficiency table interface
 * (4) For ovpctrl_uvp, read uvp status to decide it is plug-in or plug-out
 *
 * 1.0.8_MTK
 * (1) Release mt6370_enable_discharge interface
 *     discharge/OTG is controlled by PD
 *
 * 1.0.7_MTK
 * (1) Adapt to GM30
 *
 * 1.0.6_MTK
 * (1) Use wait queue instead of mdelay for waiting interrupt event
 *
 * 1.0.5_MTK
 * (1) Modify USB charger type detecion flow
 * Follow notifier from TypeC if CONFIG_TCPC_CLASS is defined
 * (2) Add WDT timeout interrupt and kick watchdog in irq handler
 *
 * 1.0.4_MTK
 * (1) Add USB charger type detection
 *
 * 1.0.3_MTK
 * (1) Remove reset chip operation in probe function
 *
 * 1.0.2_MTK
 * (1) For normal boot, set IPREC to 850mA to prevent Isys drop
 * (2) Modify naming rules of functions
 *
 * 1.0.0_MTK
 * (1) Initial Release
 */
