// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#include <linux/cdev.h>
#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/netlink.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/sched/clock.h>
#include <net/sock.h>

#include "mtk_battery.h"
#include "mtk_gauge.h"
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

#define RG_TM_PASCODE1				0x107
#define RG_ADC_CONFG1				0x1A4
#define VBAT_MON_EN_MASK			BIT(5)
#define RG_VBAT_MON_RPT				0x19C
#define RG_BM_TOP_INT_CON0_SET			0x225
#define RG_BM_TOP_INT_CON0_CLR			0x226
#define RG_BM_TOP_INT_MASK_CON0			0x22D
#define RG_BM_TOP_INT_MASK_CON0_SET		0x22E
#define RG_BM_TOP_INT_MASK_CON0_CLR		0x22F
#define RG_BM_TOP_INT_STATUS0			0x236
#define NUM_IRQ_REG				3

#define RG_FGADC_ANA_ELR4			0x263
#define FG_GAINERR_SEL_MASK			GENMASK(1, 0)
#define RG_FGADC_CON0				0x26D
#define FG_ZCV_DET_EN_MASK			BIT(2)
#define FG_ZCV_DET_EN_SHIFT			2
#define RG_FGADC_CON2				0x26F
#define FG_LATCHDATA_ST_MASK			BIT(7)
#define FG_N_CHARGE_RST_MASK			BIT(3)
#define FG_CHARGE_RST_MASK			BIT(2)
#define FG_TIME_RST_MASK			BIT(1)
#define RG_FGADC_CON3				0x270
#define FG_SW_CLEAR_MASK			BIT(3)
#define FG_SW_READ_PRE_MASK			BIT(0)
#define RG_FGADC_RST_CON0			0x277
#define FG_RSTB_STATUS_MASK			BIT(0)
#define FG_RSTB_STATUS_SHIFT			0
#define RG_FGADC_CAR_CON0			0x278
#define RG_FGADC_CAR_CON1			0x27A
#define RG_FGADC_CARTH_CON0			0x27C
#define RG_FGADC_CARTH_CON1			0x27E
#define RG_FGADC_CARTH_CON2			0x280
#define RG_FGADC_CARTH_CON3			0x282
#define RG_FGADC_NCAR_CON0			0x284
#define RG_FGADC_NCAR_CON2			0x288
#define FG_N_CHARGE_TH_MASK			GENMASK(31, 0)
#define RG_FGADC_IAVG_CON0			0x28C
#define FG_IAVG_15_00_MASK			GENMASK(15, 0)
#define RG_FGADC_IAVG_CON1			0x28E
#define FG_IAVG_VLD_MASK			BIT(0)
#define RG_FGADC_IAVG_CON2			0x28F
#define FG_IAVG_27_16_MASK			GENMASK(11, 0)
#define RG_FGADC_IAVG_CON3			0x291
#define RG_FGADC_IAVG_CON5			0x295
#define RG_FGADC_NTER_CON0			0x299
#define FGADC_NTER_MASK				GENMASK(29, 0)
#define RG_FGADC_ZCV_CON0			0x2AE
#define FGADC_ZCV_CON0_RSV			BIT(7)
#define FG_ZCV_DET_IV_MASK			GENMASK(3, 0)
#define FG_ZCV_DET_IV_SHIFT			0
#define RG_FGADC_ZCV_CON2			0x2B0
#define RG_FGADC_ZCVTH_CON0			0x2B6
#define FG_ZCV_CAR_TH_MASK			GENMASK(30, 0)
#define RG_FGADC_R_CON0				0x2E5
#define RG_FGADC_CUR_CON0			0x2E7
#define RG_FGADC_CUR_CON3			0x2ED
#define RG_SYSTEM_INFO_CON0			0x2F9

#define HK_TOP_RST_CON0				0x30F
#define RESET_MASK				BIT(0)
#define RG_HK_TOP_STRUP_CON1			0x325
#define HK_STRUP_AUXADC_START_SEL_MASK		BIT(2)
#define HK_STRUP_AUXADC_START_SEL_SHIFT		2
#define HK_TOP_WKEY				0x328
#define RG_BATON_ANA_MON0			0x388
#define AD_BATON_UNDET_MASK			BIT(1)

#define RG_AUXADC_ADC_OUT_PWRON_PCHR		0x40C
#define AUXADC_ADC_RDY_PWRON_PCHR_MASK		BIT(15)
#define AUXADC_ADC_OUT_PWRON_PCHR_MASK		GENMASK(14, 0)
#define RG_AUXADC_ADC_OUT_WAKEUP_PCHR		0x40E
#define AUXADC_ADC_RDY_WAKEUP_PCHR_MASK		BIT(15)
#define AUXADC_ADC_OUT_WAKEUP_PCHR_MASK		GENMASK(14, 0)
#define RG_AUXADC_ADC_OUT_FGADC_PCHR		0x412
#define AUXADC_ADC_OUT_FGADC_PCHR_MASK		GENMASK(14, 0)
#define RG_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR	0x414
#define AUXADC_ADC_RDY_BAT_PLUGIN_PCHR_MASK	BIT(15)
#define AUXADC_ADC_OUT_BAT_PLUGIN_PCHR_MASK	GENMASK(14, 0)
#define RG_AUXADC_ADC_OUT_NAG			0x418
#define AUXADC_ADC_OUT_NAG_MASK			GENMASK(14, 0)
#define RG_AUXADC_CON42				0x467
#define AUXADC_ADC_RDY_PWRON_CLR_MASK		BIT(3)
#define AUXADC_ADC_RDY_BAT_PLUGIN_CLR_MASK	BIT(2)
#define AUXADC_ADC_RDY_WAKEUP_CLR_MASK		BIT(0)
#define AUXADC_EFUSE_GAIN_TRIM			0x46E
#define AUXADC_EFUSE_OFFSET_TRIM		0x470
#define RG_AUXADC_LBAT2_0			0x4B9
#define AUXADC_LBAT2_EN_MASK			BIT(0)
#define RG_AUXADC_LBAT2_1			0x4BA
#define AUXADC_LBAT2_DEBT_MIN_SEL_MASK		GENMASK(5, 4)
#define AUXADC_LBAT2_DEBT_MIN_SEL_SHIFT		4
#define AUXADC_LBAT2_DEBT_MAX_SEL_MASK		GENMASK(3, 2)
#define AUXADC_LBAT2_DEBT_MAX_SEL_SHIFT		2
#define AUXADC_LBAT2_DET_PRD_SEL_MASK		GENMASK(1, 0)
#define AUXADC_LBAT2_DET_PRD_SEL_SHIFT		0
#define RG_AUXADC_LBAT2_2			0x4BB
#define AUXADC_LBAT2_DET_MAX_MASK		BIT(1)
#define AUXADC_LBAT2_DET_MAX_SHIFT		1
#define AUXADC_LBAT2_IRQ_EN_MAX_MASK		BIT(0)
#define AUXADC_LBAT2_IRQ_EN_MAX_SHIFT		0
#define RG_AUXADC_LBAT2_3			0x4BC
#define AUXADC_LBAT2_VOLT_MAX_MASK		GENMASK(11, 0)
#define RG_AUXADC_LBAT2_5			0x4BE
#define AUXADC_LBAT2_DET_MIN_MASK		BIT(1)
#define AUXADC_LBAT2_DET_MIN_SHIFT		1
#define AUXADC_LBAT2_IRQ_EN_MIN_MASK		BIT(0)
#define AUXADC_LBAT2_IRQ_EN_MIN_SHIFT		0
#define RG_AUXADC_LBAT2_6			0x4BF
#define AUXADC_LBAT2_VOLT_MIN_MASK		GENMASK(11, 0)
#define RG_AUXADC_BAT_TEMP_0			0x4C5
#define AUXADC_BAT_TEMP_EN_MASK			BIT(0)
#define RG_AUXADC_BAT_TEMP_1			0x4C6
#define AUXADC_BAT_TEMP_FROZE_EN_MASK		BIT(0)
#define RG_AUXADC_BAT_TEMP_2			0x4C7
#define AUXADC_BAT_TEMP_DEBT_MIN_SEL_MASK	GENMASK(5, 4)
#define AUXADC_BAT_TEMP_DEBT_MIN_SEL_SHIFT	4
#define AUXADC_BAT_TEMP_DEBT_MAX_SEL_MASK	GENMASK(3, 2)
#define AUXADC_BAT_TEMP_DEBT_MAX_SEL_SHIFT	2
#define AUXADC_BAT_TEMP_DET_PRD_SEL_MASK	GENMASK(1, 0)
#define AUXADC_BAT_TEMP_DET_PRD_SEL_SHIFT	0
#define RG_AUXADC_BAT_TEMP_3			0x4C8
#define AUXADC_BAT_TEMP_DET_MAX_MASK		BIT(1)
#define AUXADC_BAT_TEMP_IRQ_EN_MAX_MASK		BIT(0)
#define RG_AUXADC_BAT_TEMP_4			0x4C9
#define AUXADC_BAT_TEMP_VOLT_MAX_MASK		GENMASK(11, 0)
#define RG_AUXADC_BAT_TEMP_6			0x4CB
#define AUXADC_BAT_TEMP_DET_MIN_MASK		BIT(1)
#define AUXADC_BAT_TEMP_IRQ_EN_MIN_MASK		BIT(0)
#define RG_AUXADC_BAT_TEMP_7			0x4CC
#define AUXADC_BAT_TEMP_VOLT_MIN_MASK		GENMASK(11, 0)
#define RG_AUXADC_NAG_0				0x4D2
#define AUXADC_NAG_IRQ_EN_MASK			BIT(5)
#define AUXADC_NAG_PRD_MASK			GENMASK(4, 3)
#define AUXADC_NAG_PRD_SHIFT			3
#define AUXADC_NAG_VBAT1_SEL_MASK		BIT(2)
#define AUXADC_NAG_VBAT1_SEL_SHIFT		2
#define AUXADC_NAG_EN_MASK			BIT(0)
#define RG_AUXADC_NAG_1				0x4D3
#define AUXADC_NAG_ZCV_MASK			GENMASK(14, 0)
#define RG_AUXADC_NAG_3				0x4D5
#define AUXADC_NAG_C_DLTV_TH_MASK		GENMASK(26, 0)
#define RG_AUXADC_NAG_7				0x4D9
#define AUXADC_NAG_CNT_MASK			GENMASK(25, 0)
#define RG_AUXADC_NAG_11			0x4DD
#define RG_AUXADC_NAG_13			0x4DF
#define AUXADC_NAG_C_DLTV_MASK			GENMASK(26, 0)
#define AUXADC_EFUSE_GAIN_ERR			0X579

#define ADC_CONV_TIME_US	2200
#define ADC_VBAT_SCALE		1250
#define ADC_FROM_VBAT_RAW(raw)	((raw) * ADC_VBAT_SCALE / 1000)

#define HTOL_THRESHOLD_MAX			20
#define HTOL_THRESHOLD_MIN			5
#define HTOL_CALI_MAX				267

/* mt6359 610.352 uA */
#define UNIT_FGCURRENT				610352
/* CHARGE_LSB 0.085 uAh*/
#define UNIT_CHARGE				85

/* AUXADC */
#define R_VAL_TEMP_2				15
#define R_VAL_TEMP_3				40

#define UNIT_TIME				50
#define UNIT_FG_IAVG				305176
/* IAVG LSB: 305.176 uA */
/* 5mm ohm */
#define UNIT_FGCAR_ZCV				85
/* CHARGE_LSB = 0.085 uAh */

#define VOLTAGE_FULL_RANGES			1840
#define ADC_PRECISE				32768	/* 15 bits */

#define CAR_TO_REG_SHIFT			5
/*coulomb interrupt lsb might be different with coulomb lsb */
#define CAR_TO_REG_FACTOR			0x2E14
/* 1000 * 1000 / CHARGE_LSB */
#define UNIT_FGCAR				174080
/* CHARGE_LSB 0.085 * 2^11 */


enum {
	CHAN_BAT_VOLT = 0,
	CHAN_BAT_TEMP,
	CHAN_PTIM_BAT_VOLT,
	CHAN_PTIM_R,
	CHAN_VREF,
	CHAN_MAX
};

struct mt6375_priv {
	struct mtk_gauge gauge;
	struct device *dev;
	struct regmap *regmap;
	struct irq_domain *domain;
	struct irq_chip irq_chip;
	struct mutex irq_lock;
	int irq;
	u8 unmask_buf[NUM_IRQ_REG];
	int default_r_fg;
	u16 gain_err;
	u16 efuse_gain_err;
	int offset_trim;
	int unit_fgcurrent;
	int unit_charge;
	int unit_fg_iavg;
	int unit_fgcar_zcv;
};

/************ bat_cali *******************/
#define BAT_CALI_DEVNAME "MT_pmic_adc_cali"
/* add for meta tool----------------------------------------- */
#define Get_META_BAT_VOL _IOW('k', 10, int)
#define Get_META_BAT_SOC _IOW('k', 11, int)
#define Get_META_BAT_CAR_TUNE_VALUE _IOW('k', 12, int)
#define Set_META_BAT_CAR_TUNE_VALUE _IOW('k', 13, int)
#define Set_BAT_DISABLE_NAFG _IOW('k', 14, int)
#define Set_CARTUNE_TO_KERNEL _IOW('k', 15, int)

static struct class *bat_cali_class;
static int bat_cali_major;
static dev_t bat_cali_devno;
static struct cdev *bat_cali_cdev;

static void gauge_irq_lock(struct irq_data *data)
{
	struct mt6375_priv *priv = irq_data_get_irq_chip_data(data);

	mutex_lock(&priv->irq_lock);
}

static void gauge_irq_sync_unlock(struct irq_data *data)
{
	struct mt6375_priv *priv = irq_data_get_irq_chip_data(data);
	int idx = data->hwirq / 8, bits = BIT(data->hwirq % 8), ret;
	unsigned int reg;

	if (priv->unmask_buf[idx] & bits)
		reg = RG_BM_TOP_INT_CON0_SET + idx * 3;
	else
		reg = RG_BM_TOP_INT_CON0_CLR + idx * 3;

	ret = regmap_write(priv->regmap, reg, bits);
	if (ret)
		dev_err(priv->dev, "Failed to set/clr irq con %d\n", data->hwirq);

	mutex_unlock(&priv->irq_lock);
}

static void gauge_irq_disable(struct irq_data *data)
{
	struct mt6375_priv *priv = irq_data_get_irq_chip_data(data);

	priv->unmask_buf[data->hwirq / 8] &= ~BIT(data->hwirq % 8);
}

static void gauge_irq_enable(struct irq_data *data)
{
	struct mt6375_priv *priv = irq_data_get_irq_chip_data(data);

	priv->unmask_buf[data->hwirq / 8] |= BIT(data->hwirq % 8);
}

static int gauge_irq_map(struct irq_domain *h, unsigned int virq, irq_hw_number_t hw)
{
	struct mt6375_priv *priv = h->host_data;

	irq_set_chip_data(virq, priv);
	irq_set_chip(virq, &priv->irq_chip);
	irq_set_nested_thread(virq, 1);
	irq_set_parent(virq, priv->irq);
	irq_set_noprobe(virq);
	return 0;
}

static const struct irq_domain_ops gauge_domain_ops = {
	.map = gauge_irq_map,
	.xlate = irq_domain_xlate_onetwocell,
};

static irqreturn_t gauge_irq_thread(int irq, void *data)
{
	struct mt6375_priv *priv = data;
	u8 status_buf[NUM_IRQ_REG], status;
	static const u8 no_status[NUM_IRQ_REG];
	static const u8 mask[NUM_IRQ_REG] = { 0x9F, 0x1B, 0x0D };
	bool handled = false;
	int i, j, ret;

	ret = regmap_raw_read(priv->regmap, RG_BM_TOP_INT_STATUS0, status_buf, sizeof(status_buf));
	if (ret) {
		dev_err(priv->dev, "Error reading INT status\n");
		return IRQ_HANDLED;
	}

	if (!memcmp(status_buf, no_status, NUM_IRQ_REG)) {
		return IRQ_HANDLED;
	}

	/* mask irqs */
	for (i = 0; i < NUM_IRQ_REG; i++) {
		ret = regmap_write(priv->regmap,
				   RG_BM_TOP_INT_MASK_CON0_SET + i * 3,
				   mask[i]);
		if (ret)
			dev_err(priv->dev, "Failed to mask irq[%d]\n", i);
	}

	for (i = 0; i < NUM_IRQ_REG; i++) {
		status = status_buf[i] & priv->unmask_buf[i];
		if (!status)
			continue;

		for (j = 0; j < 8; j++) {
			if (!(status & BIT(j)))
				continue;

			handle_nested_irq(irq_find_mapping(priv->domain, i * 8 + j));
			handled = true;
		}
	}

	/* after process, unmask irqs */
	for (i = 0; i < NUM_IRQ_REG; i++) {
		ret = regmap_write(priv->regmap,
				   RG_BM_TOP_INT_MASK_CON0_CLR + i * 3,
				   mask[i]);
		if (ret)
			dev_err(priv->dev, "Failed to unmask irq[%d]\n", i);
	}

	ret = regmap_raw_write(priv->regmap, RG_BM_TOP_INT_STATUS0, status_buf, sizeof(status_buf));
	if (ret)
		dev_err(priv->dev, "Error clear INT status\n");

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static int gauge_add_irq_chip(struct mt6375_priv *priv)
{
	int i, ret;

	for (i = 0; i < NUM_IRQ_REG; i++) {
		ret = regmap_write(priv->regmap, RG_BM_TOP_INT_CON0_CLR + i * 3, 0xFF);
		if (ret) {
			dev_err(priv->dev, "Failed to disable irq con [%d]\n", i);
			return ret;
		}

		ret = regmap_write(priv->regmap, RG_BM_TOP_INT_MASK_CON0 + i * 3, 0);
		if (ret) {
			dev_err(priv->dev, "Failed to init irq mask [%d]\n", i);
			return ret;
		}
	}

	priv->irq_chip.name = dev_name(priv->dev);
	priv->irq_chip.irq_bus_lock = gauge_irq_lock;
	priv->irq_chip.irq_bus_sync_unlock = gauge_irq_sync_unlock;
	priv->irq_chip.irq_disable = gauge_irq_disable;
	priv->irq_chip.irq_enable = gauge_irq_enable;

	priv->domain = irq_domain_add_linear(priv->dev->of_node, NUM_IRQ_REG * 8,
					     &gauge_domain_ops, priv);
	if (!priv->domain) {
		dev_err(priv->dev, "Failed to create IRQ domain\n");
		return -ENOMEM;
	}

	ret = request_threaded_irq(priv->irq, NULL, gauge_irq_thread, IRQF_SHARED | IRQF_ONESHOT,
				   dev_name(priv->dev), priv);
	if (ret) {
		dev_err(priv->dev, "Failed to request IRQ %d for %s: %d\n", priv->irq,
			dev_name(priv->dev), ret);
		goto err_irq;
	}

	return 0;

err_irq:
	irq_domain_remove(priv->domain);
	return ret;
}

static void gauge_del_irq_chip(struct mt6375_priv *priv)
{
	unsigned int virq;
	int hwirq;

	free_irq(priv->irq, priv);

	for (hwirq = 0; hwirq < NUM_IRQ_REG * 8; hwirq++) {
		virq = irq_find_mapping(priv->domain, hwirq);
		if (virq)
			irq_dispose_mapping(virq);
	}

	irq_domain_remove(priv->domain);
}

static int gauge_get_all_auxadc_channels(struct mt6375_priv *priv)
{
	struct mtk_gauge *gauge = &priv->gauge;
	const char *adc_names[CHAN_MAX] = { "bat_volt", "bat_temp", "ptim_bat_volt", "ptim_r", "vref" };
	struct iio_channel *adc_chan[CHAN_MAX];
	int i;

	for (i = 0; i < CHAN_MAX; i++) {
		adc_chan[i] = devm_iio_channel_get(priv->dev, adc_names[i]);
		if (IS_ERR(adc_chan[i]))
			return PTR_ERR(adc_chan[i]);
	}

	/* Filled adc channels into mtk_gauge */
	gauge->chan_bat_temp = adc_chan[CHAN_BAT_TEMP];
	gauge->chan_bat_voltage = adc_chan[CHAN_BAT_VOLT];
	gauge->chan_ptim_bat_voltage = adc_chan[CHAN_PTIM_BAT_VOLT];
	gauge->chan_ptim_r = adc_chan[CHAN_PTIM_R];
	gauge->chan_bif = adc_chan[CHAN_VREF];

	return 0;
}

static int gauge_get_all_interrupts(struct mt6375_priv *priv)
{
	struct platform_device *pdev = to_platform_device(priv->dev);
	struct mtk_gauge *gauge = &priv->gauge;
	const char *irq_names[GAUGE_IRQ_MAX] = {
					"COULOMB_H", "COULOMB_L", "VBAT2_H", "VBAT2_L",
					"NAFG", "BAT_OUT", "ZCV", "FG_N_CHARGE_L",
					"FG_IAVG_H", "FG_IAVG_L", "BAT_TMP_H", "BAT_TMP_L" };
	int i, irq_no;

	for (i = 0; i < GAUGE_IRQ_MAX; i++) {
		irq_no = platform_get_irq_byname(pdev, irq_names[i]);
		if (irq_no < 0)
			return irq_no;

		irq_set_status_flags(irq_no, IRQ_NOAUTOEN);
		gauge->irq_no[i] = irq_no;
	}

	return 0;
}

void __attribute__ ((weak))
	mtk_battery_netlink_handler(struct sk_buff *skb)
{
}

static signed int reg_to_mv_value(signed int _reg)
{
	long long _reg64 = _reg;
	int ret;

#if defined(__LP64__) || defined(_LP64)
	_reg64 = (_reg64 * VOLTAGE_FULL_RANGES * R_VAL_TEMP_3) / ADC_PRECISE;
#else
	_reg64 = div_s64(_reg64 * VOLTAGE_FULL_RANGES * R_VAL_TEMP_3, ADC_PRECISE);
#endif
	ret = _reg64;
	bm_debug("[%s] %lld => %d\n",
		__func__, _reg64, ret);
	return ret;
}

static signed int mv_to_reg_value(signed int _mv)
{
	int ret;
	long long _reg64 = _mv;
#if defined(__LP64__) || defined(_LP64)
	_reg64 = (_reg64 * ADC_PRECISE) / (VOLTAGE_FULL_RANGES * R_VAL_TEMP_3);
#else
	_reg64 = div_s64((_reg64 * ADC_PRECISE), (VOLTAGE_FULL_RANGES * R_VAL_TEMP_3));
#endif
	ret = _reg64;

	if (ret <= 0) {
		bm_err(
			"[fg_bat_nafg][%s] mv=%d,%lld => %d,\n",
			__func__, _mv, _reg64, ret);
		return ret;
	}

	bm_debug("[%s] mv=%d,%lld => %d,\n", __func__, _mv, _reg64, ret);
	return ret;
}

static int mv_to_reg_12_temp_value(signed int _reg)
{
	int ret = (_reg * 4096) / (VOLTAGE_FULL_RANGES * R_VAL_TEMP_2);

	bm_debug("[%s] %d => %d\n", __func__, _reg, ret);
	return ret;
}

static void pre_gauge_update(struct mtk_gauge *gauge)
{
	int m = 0;
	unsigned int reg_val = 0;
	int ret = 0;

	if (gauge->gm->disableGM30) {
		return;
	}
	ret = regmap_update_bits(gauge->regmap, RG_FGADC_CON3,
				 FG_SW_READ_PRE_MASK, FG_SW_READ_PRE_MASK);
	if (ret) {
		pr_notice("%s error, ret = %d\n", __func__, ret);
		return;
	}
	do {
		m++;
		if (m > 1000) {
			bm_err("[%s] gauge_update_polling timeout 1!\r\n",
				__func__);
			break;
		}

		ret = regmap_read(gauge->regmap, RG_FGADC_CON2, &reg_val);
		if (ret) {
			pr_notice("%s error, ret = %d\n", __func__, ret);
			return;
		}
	} while (!(reg_val & FG_LATCHDATA_ST_MASK));
}

void disable_all_irq(struct mtk_battery *gm)
{
	disable_gauge_irq(gm->gauge, COULOMB_H_IRQ);
	disable_gauge_irq(gm->gauge, COULOMB_L_IRQ);
	disable_gauge_irq(gm->gauge, VBAT_H_IRQ);
	disable_gauge_irq(gm->gauge, VBAT_L_IRQ);
	disable_gauge_irq(gm->gauge, NAFG_IRQ);
	disable_gauge_irq(gm->gauge, BAT_PLUGOUT_IRQ);
	disable_gauge_irq(gm->gauge, ZCV_IRQ);
	disable_gauge_irq(gm->gauge, FG_N_CHARGE_L_IRQ);
	disable_gauge_irq(gm->gauge, FG_IAVG_H_IRQ);
	disable_gauge_irq(gm->gauge, FG_IAVG_L_IRQ);
	disable_gauge_irq(gm->gauge, BAT_TMP_H_IRQ);
	disable_gauge_irq(gm->gauge, BAT_TMP_L_IRQ);
}

static void post_gauge_update(struct mtk_gauge *gauge)
{
	int m = 0;
	unsigned int regval;
	int ret = 0;

	ret = regmap_update_bits(gauge->regmap, RG_FGADC_CON3,
				 FG_SW_CLEAR_MASK | FG_SW_READ_PRE_MASK,
				 FG_SW_CLEAR_MASK);
	if (ret) {
		pr_notice("%s error, ret = %d\n", __func__, ret);
		return;
	}
	do {
		m++;
		if (m > 1000) {
			bm_err("[%s] gauge_update_polling timeout 2!\r\n",
				__func__);
			break;
		}
		ret = regmap_read(gauge->regmap, RG_FGADC_CON2, &regval);
		if (ret) {
			pr_notice("%s error, ret = %d\n", __func__, ret);
			return;
		}
	} while (regval & FG_LATCHDATA_ST_MASK);

	ret = regmap_update_bits(gauge->regmap, RG_FGADC_CON3,
				 FG_SW_CLEAR_MASK, 0);
	if (ret) {
		pr_notice("%s error, ret = %d\n", __func__, ret);
		return;
	}
}

static int mv_to_reg_12_value(struct mtk_gauge *gauge,
	signed int _reg)
{
	int ret = (_reg * 4096) / (VOLTAGE_FULL_RANGES * R_VAL_TEMP_3);

	bm_debug("[%s] %d => %d\n", __func__, _reg, ret);
	return ret;
}

static int reg_to_current(struct mtk_gauge *gauge,
	unsigned int regval)
{
	struct mt6375_priv *priv = container_of(gauge, struct mt6375_priv, gauge);
	unsigned short uvalue16 = 0;
	int dvalue, retval;
	long long temp_value = 0;
	bool is_charging = true;

	uvalue16 = (unsigned short) regval;

	dvalue = (unsigned int) uvalue16;
	if (dvalue == 0) {
		temp_value = (long long) dvalue;
		is_charging = false;
	} else if (dvalue > 32767) {
		/* > 0x8000 */
		temp_value = (long long) (dvalue - 65535);
		temp_value = temp_value - (temp_value * 2);
		is_charging = false;
	} else {
		temp_value = (long long) dvalue;
	}

	temp_value = temp_value * priv->unit_fgcurrent;
#if defined(__LP64__) || defined(_LP64)
	do_div(temp_value, 100000);
#else
	temp_value = div_s64(temp_value, 100000);
#endif
	retval = (unsigned int) temp_value;

	bm_debug("[%s] 0x%x 0x%x 0x%x 0x%x 0x%x %d\n",
		__func__,
		regval,
		uvalue16,
		dvalue,
		(int)temp_value,
		retval,
		is_charging);

	if (is_charging == false)
		return -retval;

	return retval;
}

static u8 get_rtc_spare0_fg_value(struct mtk_gauge *gauge)
{
	struct nvmem_cell *cell;
	u8 *buf, data;

	if (gauge->gm->disableGM30)
		return 0;

	cell = nvmem_cell_get(&gauge->pdev->dev, "initialization");
	if (IS_ERR(cell)) {
		bm_err("[%s]get rtc cell fail\n", __func__);
		return 0;
	}

	buf = nvmem_cell_read(cell, NULL);
	nvmem_cell_put(cell);
	if (IS_ERR(buf)) {
		bm_err("[%s]read rtc cell fail\n", __func__);
		return 0;
	}
	bm_debug("[%s] val=0x%x, %d\n", __func__, *buf, *buf);

	data = *buf;
	kfree(buf);

	return data;
}

static void set_rtc_spare0_fg_value(struct mtk_gauge *gauge, u8 val)
{
	struct nvmem_cell *cell;
	u32 length = 1;
	int ret;

	if (gauge->gm->disableGM30)
		return;

	cell = nvmem_cell_get(&gauge->pdev->dev, "initialization");
	if (IS_ERR(cell)) {
		bm_err("[%s]get rtc cell fail\n", __func__);
		return;
	}

	ret = nvmem_cell_write(cell, &val, length);
	nvmem_cell_put(cell);
	if (ret != length)
		bm_err("[%s] write rtc cell fail\n", __func__);
}

static u8 get_rtc_spare_fg_value(struct mtk_gauge *gauge)
{
	struct nvmem_cell *cell;
	u8 *buf, data;

	if (gauge->gm->disableGM30)
		return 0;

	cell = nvmem_cell_get(&gauge->pdev->dev, "state-of-charge");
	if (IS_ERR(cell)) {
		bm_err("[%s]get rtc cell fail\n", __func__);
		return 0;
	}

	buf = nvmem_cell_read(cell, NULL);
	nvmem_cell_put(cell);

	if (IS_ERR(buf)) {
		bm_err("[%s]read rtc cell fail\n", __func__);
		return 0;
	}

	bm_debug("[%s] val=%d\n", __func__, *buf);
	data = *buf;
	kfree(buf);

	return data;
}

static void set_rtc_spare_fg_value(struct mtk_gauge *gauge, u8 val)
{
	struct nvmem_cell *cell;
	u32 length = 1;
	int ret;

	if (gauge->gm->disableGM30)
		return;

	cell = nvmem_cell_get(&gauge->pdev->dev, "state-of-charge");
	if (IS_ERR(cell)) {
		bm_err("[%s]get rtc cell fail\n", __func__);
		return;
	}

	ret = nvmem_cell_write(cell, &val, length);
	nvmem_cell_put(cell);

	if (ret != length)
		bm_err("[%s] write rtc cell fail\n", __func__);

	bm_debug("[%s] val=%d\n", __func__, val);
}

static void fgauge_read_RTC_boot_status(struct mtk_gauge *gauge)
{
	unsigned int hw_id = 0x6375;
	u8 spare0_reg = 0;
	unsigned int spare0_reg_b13 = 0;
	u8 spare3_reg = 0;
	int spare3_reg_valid = 0;

	spare0_reg = get_rtc_spare0_fg_value(gauge);
	spare3_reg = get_rtc_spare_fg_value(gauge);
	gauge->hw_status.gspare0_reg = spare0_reg;
	gauge->hw_status.gspare3_reg = spare3_reg;
	spare3_reg_valid = (spare3_reg & 0x80) >> 7;

	if (spare3_reg_valid == 0)
		gauge->hw_status.rtc_invalid = 1;
	else
		gauge->hw_status.rtc_invalid = 0;

	if (gauge->hw_status.rtc_invalid == 0) {
		spare0_reg_b13 = (spare0_reg & 0x20) >> 5;
		if ((hw_id & 0xff00) == 0x3500)
			gauge->hw_status.is_bat_plugout = spare0_reg_b13;
		else
			gauge->hw_status.is_bat_plugout = !spare0_reg_b13;

		gauge->hw_status.bat_plug_out_time = spare0_reg & 0x1f;
	} else {
		gauge->hw_status.is_bat_plugout = 1;
		gauge->hw_status.bat_plug_out_time = 31;
	}

	bm_err("[%s]rtc_invalid %d plugout %d plugout_time %d spare3 0x%x spare0 0x%x hw_id 0x%x\n",
			__func__,
			gauge->hw_status.rtc_invalid,
			gauge->hw_status.is_bat_plugout,
			gauge->hw_status.bat_plug_out_time,
			spare3_reg, spare0_reg, hw_id);
}

static int fgauge_set_info(struct mtk_gauge *gauge, enum gauge_property ginfo, unsigned int value)
{
	u16 regval;
	int ret;

	bm_debug("[%s]info:%d v:%d\n", __func__, ginfo, value);

	ret = regmap_raw_read(gauge->regmap, RG_SYSTEM_INFO_CON0, &regval, sizeof(regval));
	if (ret)
		return ret;

	switch (ginfo) {
	case GAUGE_PROP_2SEC_REBOOT:
		regval = value ? (regval | BIT(0)) : (regval & ~BIT(0));
		break;
	case GAUGE_PROP_PL_CHARGING_STATUS:
		regval = value ? (regval | BIT(1)) : (regval & ~BIT(1));
		break;
	case GAUGE_PROP_MONITER_PLCHG_STATUS:
		regval = value ? (regval | BIT(2)) : (regval & ~BIT(2));
		break;
	case GAUGE_PROP_BAT_PLUG_STATUS:
		regval = value ? (regval | BIT(3)) : (regval & ~BIT(3));
		break;
	case GAUGE_PROP_IS_NVRAM_FAIL_MODE:
		regval = value ? (regval | BIT(4)) : (regval & ~BIT(4));
		break;
	case GAUGE_PROP_MONITOR_SOFF_VALIDTIME:
		regval = value ? (regval | BIT(5)) : (regval & ~BIT(5));
		break;
	case GAUGE_PROP_CON0_SOC:
		regval &= ~GENMASK(15, 9);
		regval |= ((value / 100) << 9);
		break;
	default:
		return -EINVAL;
	}

	return regmap_raw_write(gauge->regmap, RG_SYSTEM_INFO_CON0, &regval, sizeof(regval));
}

static int fgauge_get_info(struct mtk_gauge *gauge, enum gauge_property ginfo, int *value)
{
	u16 regval = 0;
	int ret;

	ret = regmap_raw_read(gauge->regmap, RG_SYSTEM_INFO_CON0, &regval, sizeof(regval));
	if (ret)
		return ret;

	switch (ginfo) {
	case GAUGE_PROP_2SEC_REBOOT:
		*value = regval & BIT(0);
		break;
	case GAUGE_PROP_PL_CHARGING_STATUS:
		*value = (regval & BIT(1)) >> 1;
		break;
	case GAUGE_PROP_MONITER_PLCHG_STATUS:
		*value = (regval & BIT(2)) >> 2;
		break;
	case GAUGE_PROP_BAT_PLUG_STATUS:
		*value = (regval & BIT(3)) >> 3;
		break;
	case GAUGE_PROP_IS_NVRAM_FAIL_MODE:
		*value = (regval & BIT(4)) >> 4;
		break;
	case GAUGE_PROP_MONITOR_SOFF_VALIDTIME:
		*value = (regval & BIT(5)) >> 5;
		break;
	case GAUGE_PROP_CON0_SOC:
		*value = (regval & GENMASK(15, 9)) >> 9;
		break;
	default:
		return -EINVAL;
	}

	bm_debug("[%s]info:%d v:%d\n", __func__, ginfo, *value);

	return 0;
}

static unsigned int instant_current_for_car_tune(struct mtk_gauge *gauge)
{
	u16 reg_value = 0;
	int ret = 0;

	pre_gauge_update(gauge);

	ret = regmap_raw_read(gauge->regmap, RG_FGADC_CUR_CON0, &reg_value,
			      sizeof(reg_value));
	if (ret) {
		pr_notice("%s error, ret = %d\n", __func__, ret);
		return ret;
	}

	post_gauge_update(gauge);

	bm_err("%s, reg_value=0x%04x\n", __func__, reg_value);

	return reg_value;
}

static int calculate_car_tune(struct mtk_gauge *gauge)
{
	struct mt6375_priv *priv = container_of(gauge, struct mt6375_priv, gauge);
	int cali_car_tune;
	long long sum_all = 0;
	unsigned long long temp_sum = 0;
	int avg_cnt = 0;
	int i;
	unsigned int uvalue32 = 0;
	signed int dvalue = 0;
	long long Temp_Value1 = 0;
	unsigned long long Temp_Value2 = 0;
	long long current_from_ADC = 0;

	bm_err("%s, meta_current=%d,\n", __func__,
		gauge->hw_status.meta_current);
	if (gauge->hw_status.meta_current != 0) {
		for (i = 0; i < CALI_CAR_TUNE_AVG_NUM; i++) {
			uvalue32 = instant_current_for_car_tune(gauge);
			if (uvalue32 != 0) {
				if (uvalue32 <= 0x8000) {
					Temp_Value1 = (long long)uvalue32;
					bm_err("[111]uvalue32 %d Temp_Value1 %lld\n",
						uvalue32,
						Temp_Value1);
				} else if (uvalue32 > 0x8000) {

					Temp_Value1 =
					(long long) (65535 - uvalue32);
					bm_err("[222]uvalue32 %d Temp_Value1 %lld\n",
						uvalue32,
						Temp_Value1);
				}
				sum_all += Temp_Value1;
				avg_cnt++;
				/*****************/
				bm_err("[333]uvalue32 %d Temp_Value1 %lld sum_all %lld\n",
						uvalue32,
						Temp_Value1, sum_all);
				/*****************/
			}
			mdelay(30);
		}
		/*calculate the real world data    */
		/*current_from_ADC = sum_all / avg_cnt;*/
		temp_sum = sum_all;
		bm_err("[444]sum_all %lld temp_sum %lld avg_cnt %d current_from_ADC %lld\n",
			sum_all, temp_sum, avg_cnt, current_from_ADC);

		if (avg_cnt != 0)
			do_div(temp_sum, avg_cnt);
		current_from_ADC = temp_sum;

		bm_err("[555]sum_all %lld temp_sum %lld avg_cnt %d current_from_ADC %lld\n",
			sum_all, temp_sum, avg_cnt, current_from_ADC);

		Temp_Value2 = current_from_ADC * priv->unit_fgcurrent;

		bm_err("[555]Temp_Value2 %lld current_from_ADC %lld priv->unit_fgcurrent %d\n",
			Temp_Value2, current_from_ADC, priv->unit_fgcurrent);

		/* Move 100 from denominator to cali_car_tune's numerator */
		/*do_div(Temp_Value2, 1000000);*/
		do_div(Temp_Value2, 10000);

		bm_err("[666]Temp_Value2 %lld current_from_ADC %lld priv->unit_fgcurrent %d\n",
			Temp_Value2, current_from_ADC, priv->unit_fgcurrent);

		dvalue = (unsigned int) Temp_Value2;

		/* Auto adjust value */
		if (gauge->hw_status.r_fg_value != priv->default_r_fg)
			dvalue = (dvalue * priv->default_r_fg) /
				 gauge->hw_status.r_fg_value;

		bm_err("[666]dvalue %d fg_cust_data.r_fg_value %d\n",
			dvalue, gauge->hw_status.r_fg_value);

		/* Move 100 from denominator to cali_car_tune's numerator */
		/*cali_car_tune = meta_input_cali_current * 1000 / dvalue;*/

		if (dvalue != 0) {
			cali_car_tune =
				gauge->hw_status.meta_current *
				1000 * 100 / dvalue;

			bm_err("[777]dvalue %d fg_cust_data.r_fg_value %d cali_car_tune %d\n",
				dvalue,
				gauge->hw_status.r_fg_value,
				cali_car_tune);
			gauge->hw_status.tmp_car_tune = cali_car_tune;

			bm_err(
				"[fgauge_meta_cali_car_tune_value][%d] meta:%d, adc:%lld, UNI_FGCUR:%d, r_fg_value:%d\n",
				cali_car_tune, gauge->hw_status.meta_current,
				current_from_ADC, priv->unit_fgcurrent,
				gauge->hw_status.r_fg_value);
		}

		return 0;
	}

	return 0;
}

static int info_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	int ret = 0;

	if (attr->prop == GAUGE_PROP_CAR_TUNE_VALUE && (val > 500 && val < 1500)) {
		/* send external_current for calculate_car_tune */
		gauge->hw_status.meta_current = val;
		calculate_car_tune(gauge);
	} else if (attr->prop == GAUGE_PROP_R_FG_VALUE && val != 0)
		gauge->hw_status.r_fg_value = val;
	else if (attr->prop == GAUGE_PROP_VBAT2_DETECT_TIME)
		gauge->hw_status.vbat2_det_time = val;
	else if (attr->prop == GAUGE_PROP_VBAT2_DETECT_COUNTER)
		gauge->hw_status.vbat2_det_counter = val;
	else
		ret = fgauge_set_info(gauge, attr->prop, val);

	return ret;
}

static int info_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	int ret = 0;

	if (attr->prop == GAUGE_PROP_CAR_TUNE_VALUE)
		*val = gauge->hw_status.tmp_car_tune;
	else if (attr->prop == GAUGE_PROP_R_FG_VALUE)
		*val = gauge->hw_status.r_fg_value;
	else if (attr->prop == GAUGE_PROP_VBAT2_DETECT_TIME)
		*val = gauge->hw_status.vbat2_det_time;
	else if (attr->prop == GAUGE_PROP_VBAT2_DETECT_COUNTER)
		*val = gauge->hw_status.vbat2_det_counter;
	else
		ret = fgauge_get_info(gauge, attr->prop, val);

	return ret;
}

static int instant_current(struct mtk_gauge *gauge, int *val)
{
	struct mt6375_priv *priv = container_of(gauge, struct mt6375_priv, gauge);
	u16 reg_value = 0;
	int dvalue = 0;
	int r_fg_value = 0;
	int car_tune_value = 0;
	int ret = 0;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->gm->fg_cust_data.car_tune_value;

	pre_gauge_update(gauge);

	ret = regmap_raw_read(gauge->regmap, RG_FGADC_CUR_CON0, &reg_value,
			      sizeof(reg_value));
	if (ret) {
		pr_notice("%s error, ret = %d\n", __func__, ret);
		return ret;
	}

	post_gauge_update(gauge);

	dvalue = reg_to_current(gauge, reg_value);

	/* Auto adjust value */
	if (r_fg_value != priv->default_r_fg && r_fg_value != 0)
		dvalue = (dvalue * priv->default_r_fg) / r_fg_value;

	dvalue = ((dvalue * car_tune_value) / 1000);

	*val = dvalue;
	return ret;
}

static int read_hw_ocv_6375_plug_in(struct mtk_gauge *gauge)
{
	signed int adc_rdy = 0;
	signed int adc_result_reg = 0;
	signed int adc_result = 0;
	u16 regval = 0;
	unsigned int sel = 0;

	regmap_raw_read(gauge->regmap, RG_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR, &regval, sizeof(regval));

	adc_rdy = regval & AUXADC_ADC_RDY_BAT_PLUGIN_PCHR_MASK;
	adc_result_reg = regval & AUXADC_ADC_OUT_BAT_PLUGIN_PCHR_MASK;

	regmap_read(gauge->regmap, RG_HK_TOP_STRUP_CON1, &sel);
	sel = (sel & HK_STRUP_AUXADC_START_SEL_MASK) >> HK_STRUP_AUXADC_START_SEL_SHIFT;

	adc_result = reg_to_mv_value(adc_result_reg);
	bm_err("[oam] %s (pchr): adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
		__func__, adc_result_reg, adc_result,
		sel,
		adc_rdy);

	if (adc_rdy) {
		regmap_update_bits(gauge->regmap, RG_AUXADC_CON42,
				   AUXADC_ADC_RDY_BAT_PLUGIN_CLR_MASK,
				   AUXADC_ADC_RDY_BAT_PLUGIN_CLR_MASK);
		mdelay(1);
		regmap_update_bits(gauge->regmap, RG_AUXADC_CON42,
				   AUXADC_ADC_RDY_BAT_PLUGIN_CLR_MASK, 0);
	}

	return adc_result;
}

static int read_hw_ocv_6375_power_on(struct mtk_gauge *gauge)
{
	struct mt6375_priv *priv = container_of(gauge, struct mt6375_priv, gauge);
	signed int adc_result_rdy = 0;
	signed int adc_result_reg = 0;
	signed int adc_result = 0;
	u16 regval = 0;
	int offset_trim = priv->offset_trim;
	unsigned int sel = 0, data;
	bool is_ship_rst;

	regmap_raw_read(gauge->regmap, RG_AUXADC_ADC_OUT_PWRON_PCHR, &regval, sizeof(regval));

	adc_result_rdy = regval & AUXADC_ADC_RDY_PWRON_PCHR_MASK;
	adc_result_reg = regval & AUXADC_ADC_OUT_PWRON_PCHR_MASK;

	regmap_read(gauge->regmap, RG_HK_TOP_STRUP_CON1, &sel);
	sel = (sel & HK_STRUP_AUXADC_START_SEL_MASK) >> HK_STRUP_AUXADC_START_SEL_SHIFT;

	regmap_read(gauge->regmap, RG_FGADC_ZCV_CON0, &data);
	is_ship_rst = data & FGADC_ZCV_CON0_RSV ? true : false;
	if (is_ship_rst) {
		bm_err("%s: before cali, is_ship_rst:%d, offset_trim:0x%x, gain_err:0x%x, adc_result_reg:0x%x\n",
			__func__, is_ship_rst, offset_trim, priv->gain_err, adc_result_reg);
		adc_result_reg = adc_result_reg * (ADC_PRECISE + priv->gain_err) / ADC_PRECISE +
				 offset_trim;
		bm_err("%s: after cali, adc_result_reg:0x%x\n", __func__, adc_result_reg);
	}
	adc_result = reg_to_mv_value(adc_result_reg);
	bm_err("[oam] %s (pchr) : adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
		__func__, adc_result_reg, adc_result,
		sel, adc_result_rdy);

	if (adc_result_rdy) {
		regmap_update_bits(gauge->regmap, RG_AUXADC_CON42, AUXADC_ADC_RDY_PWRON_CLR_MASK,
				   AUXADC_ADC_RDY_PWRON_CLR_MASK);
		mdelay(1);
		regmap_update_bits(gauge->regmap, RG_AUXADC_CON42, AUXADC_ADC_RDY_PWRON_CLR_MASK,
				   0);
	}

	return adc_result;
}

static int read_hw_ocv_6375_before_chgin(struct mtk_gauge *gauge)
{
	signed int adc_result_rdy = 0;
	signed int adc_result_reg = 0;
	signed int adc_result = 0;
	u16 regval = 0;
	unsigned int sel = 0;

	regmap_raw_read(gauge->regmap, RG_AUXADC_ADC_OUT_WAKEUP_PCHR, &regval, sizeof(regval));

	adc_result_rdy = regval & AUXADC_ADC_RDY_WAKEUP_PCHR_MASK;
	adc_result_reg = regval & AUXADC_ADC_OUT_WAKEUP_PCHR_MASK;

	regmap_read(gauge->regmap, RG_HK_TOP_STRUP_CON1, &sel);
	sel = (sel & HK_STRUP_AUXADC_START_SEL_MASK) >> HK_STRUP_AUXADC_START_SEL_SHIFT;

	adc_result = reg_to_mv_value(adc_result_reg);
	bm_err("[oam] %s (pchr) : adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
		__func__, adc_result_reg, adc_result,
		sel, adc_result_rdy);

	if (adc_result_rdy) {
		regmap_update_bits(gauge->regmap, RG_AUXADC_CON42, AUXADC_ADC_RDY_WAKEUP_CLR_MASK,
				   AUXADC_ADC_RDY_WAKEUP_CLR_MASK);
		mdelay(1);
		regmap_update_bits(gauge->regmap, RG_AUXADC_CON42, AUXADC_ADC_RDY_WAKEUP_CLR_MASK,
				   0);
	}

	return adc_result;
}

static int read_hw_ocv_6375_power_on_rdy(struct mtk_gauge *gauge)
{
	u16 regval = 0;
	int pon_rdy = 0;

	regmap_raw_read(gauge->regmap, RG_AUXADC_ADC_OUT_PWRON_PCHR, &regval, sizeof(regval));

	pon_rdy = (regval & AUXADC_ADC_RDY_PWRON_PCHR_MASK) ? 1 : 0;

	bm_err("[%s] pwron_PCHR_rdy %d\n", __func__, pon_rdy);

	return pon_rdy;
}

static void switch_nafg_period(int _prd, int *value)
{
	if (_prd >= 1 && _prd < 5)
		*value = 0;
	else if (_prd >= 5 && _prd < 10)
		*value = 1;
	else if (_prd >= 10 && _prd < 20)
		*value = 2;
	else if (_prd >= 20)
		*value = 3;
}

static void fgauge_set_nafg_intr_internal(struct mtk_gauge *gauge, int _prd, int _zcv_mv,
					  int _thr_mv)
{
	u32 NAG_C_DLTV_Threashold;
	int period = 0;
	u16 regval = 0;

	gauge->zcv_reg = mv_to_reg_value(_zcv_mv);
	gauge->thr_reg = mv_to_reg_value(_thr_mv);

	if (gauge->thr_reg >= 32768) {
		bm_err("[%s]nag_c_dltv_thr mv=%d ,thr_reg=%d,limit thr_reg to 32767\n",
			__func__, _thr_mv, gauge->thr_reg);
		gauge->thr_reg = 32767;
	}

	regval = gauge->zcv_reg & AUXADC_NAG_ZCV_MASK;
	regmap_raw_write(gauge->regmap, RG_AUXADC_NAG_1, &regval, sizeof(regval));

	NAG_C_DLTV_Threashold = gauge->thr_reg & AUXADC_NAG_C_DLTV_TH_MASK;
	regmap_raw_write(gauge->regmap, RG_AUXADC_NAG_3, &NAG_C_DLTV_Threashold,
			 sizeof(NAG_C_DLTV_Threashold));

	switch_nafg_period(_prd, &period);

	regmap_update_bits(gauge->regmap, RG_AUXADC_NAG_0,
			   AUXADC_NAG_PRD_MASK | AUXADC_NAG_VBAT1_SEL_MASK,
			   period << AUXADC_NAG_PRD_SHIFT | 0 << AUXADC_NAG_VBAT1_SEL_SHIFT);

	bm_debug("[fg_bat_nafg][fgauge_set_nafg_interrupt_internal] time[%d] zcv[%d:%d] thr[%d:%d] 26_0[0x%x]\n",
		_prd, _zcv_mv, gauge->zcv_reg, _thr_mv, gauge->thr_reg, NAG_C_DLTV_Threashold);

}

static int get_nafg_vbat(struct mtk_gauge *gauge)
{
	u16 nag_vbat_reg = 0;
	unsigned int vbat_val;
	int nag_vbat_mv, i = 0;

	do {
		regmap_raw_read(gauge->regmap, RG_AUXADC_ADC_OUT_NAG, &nag_vbat_reg,
				sizeof(nag_vbat_reg));

		if (nag_vbat_reg & BIT(15))
			break;
		msleep(30);
		i++;
	} while (i <= 5);

	vbat_val = nag_vbat_reg & AUXADC_ADC_OUT_NAG_MASK;
	nag_vbat_mv = reg_to_mv_value(vbat_val);

	return nag_vbat_mv;
}

static void fgauge_set_zcv_intr_internal(struct mtk_gauge *gauge_dev, int fg_zcv_det_time,
					 int fg_zcv_car_th)
{
	struct mt6375_priv *priv = container_of(gauge_dev, struct mt6375_priv, gauge);
	long long fg_zcv_car_th_reg = fg_zcv_car_th;
	u32 fg_zcv_car_th_regval;

	fg_zcv_car_th_reg = (fg_zcv_car_th_reg * 100 * 1000);

#if defined(__LP64__) || defined(_LP64)
	do_div(fg_zcv_car_th_reg, priv->unit_fgcar_zcv);
#else
	fg_zcv_car_th_reg = div_s64(fg_zcv_car_th_reg, priv->unit_fgcar_zcv);
#endif

	if (gauge_dev->hw_status.r_fg_value != priv->default_r_fg)
#if defined(__LP64__) || defined(_LP64)
		fg_zcv_car_th_reg = (fg_zcv_car_th_reg *
				gauge_dev->hw_status.r_fg_value) /
				priv->default_r_fg;
#else
		fg_zcv_car_th_reg = div_s64(fg_zcv_car_th_reg *
				gauge_dev->hw_status.r_fg_value,
				priv->default_r_fg);
#endif

#if defined(__LP64__) || defined(_LP64)
	fg_zcv_car_th_reg = ((fg_zcv_car_th_reg * 1000) /
			gauge_dev->gm->fg_cust_data.car_tune_value);
#else
	fg_zcv_car_th_reg = div_s64((fg_zcv_car_th_reg * 1000),
			gauge_dev->gm->fg_cust_data.car_tune_value);
#endif

	regmap_update_bits(gauge_dev->regmap, RG_FGADC_ZCV_CON0, FG_ZCV_DET_IV_MASK,
			   fg_zcv_det_time << FG_ZCV_DET_IV_SHIFT);

	fg_zcv_car_th_regval = fg_zcv_car_th_reg & FG_ZCV_CAR_TH_MASK;
	regmap_raw_write(gauge_dev->regmap, RG_FGADC_ZCVTH_CON0, &fg_zcv_car_th_regval,
			 sizeof(fg_zcv_car_th_regval));

	bm_debug("[FG_ZCV_INT][%s] det_time %d mv %d reg %lld 30_00 0x%x\n",
		__func__, fg_zcv_det_time, fg_zcv_car_th, fg_zcv_car_th_reg,
		fg_zcv_car_th_regval);
}

static int read_fg_hw_info_current_1(struct mtk_gauge *gauge_dev, int *curr)
{
	int ret = 0;

	ret = instant_current(gauge_dev, curr);
	if (ret) {
		pr_notice("%s error, ret = %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static void read_fg_hw_info_current_2(struct mtk_gauge *gauge_dev)
{
	struct mt6375_priv *priv = container_of(gauge_dev, struct mt6375_priv, gauge);
	long long fg_current_2_reg;
	u16 cic2_reg = 0;
	signed int dvalue;
	long long Temp_Value;
	int sign_bit = 0;

	regmap_raw_read(gauge_dev->regmap, RG_FGADC_CUR_CON3, &cic2_reg, sizeof(cic2_reg));
	fg_current_2_reg = cic2_reg;

	/*calculate the real world data    */
	dvalue = (unsigned int)fg_current_2_reg;
	if (dvalue == 0) {
		Temp_Value = (long long) dvalue;
		sign_bit = 0;
	} else if (dvalue > 32767) {
		/* > 0x8000 */
		Temp_Value = (long long) (dvalue - 65535);
		Temp_Value = Temp_Value - (Temp_Value * 2);
		sign_bit = 1;
	} else {
		Temp_Value = (long long) dvalue;
		sign_bit = 0;
	}

	Temp_Value = Temp_Value * priv->unit_fgcurrent;
#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 100000);
#else
	Temp_Value = div_s64(Temp_Value, 100000);
#endif
	dvalue = (unsigned int) Temp_Value;


	if (gauge_dev->hw_status.r_fg_value != priv->default_r_fg)
		dvalue = (dvalue * priv->default_r_fg) / gauge_dev->hw_status.r_fg_value;

	if (sign_bit == 1)
		dvalue = dvalue - (dvalue * 2);

	gauge_dev->fg_hw_info.current_2 =
		(dvalue * gauge_dev->gm->fg_cust_data.car_tune_value) / 1000;
}

static void read_fg_hw_info_ncar(struct mtk_gauge *gauge_dev)
{
	struct mt6375_priv *priv = container_of(gauge_dev, struct mt6375_priv, gauge);
	unsigned int uvalue32_NCAR = 0;
	unsigned int uvalue32_NCAR_MSB = 0;
	u32 temp_NCAR = 0;
	signed int dvalue_NCAR = 0;
	long long Temp_Value = 0;

	regmap_raw_read(gauge_dev->regmap, RG_FGADC_NCAR_CON0, &temp_NCAR, sizeof(temp_NCAR));

	uvalue32_NCAR = temp_NCAR & ~BIT(31);
	uvalue32_NCAR_MSB = temp_NCAR & BIT(31);

	/*calculate the real world data    */
	dvalue_NCAR = (signed int)uvalue32_NCAR;
	if (uvalue32_NCAR == 0) {
		Temp_Value = 0;
	} else if (uvalue32_NCAR_MSB) {
		/* dis-charging */
		Temp_Value = (long long)(dvalue_NCAR - 0x7fffffff);
		/* keep negative value */
		Temp_Value = Temp_Value - (Temp_Value * 2);
	} else {
		/*charging */
		Temp_Value = (long long)dvalue_NCAR;
	}


	/* 0.1 mAh */
#if defined(__LP64__) || defined(_LP64)
	Temp_Value = Temp_Value * priv->unit_charge / 1000;
#else
	Temp_Value = div_s64(Temp_Value * priv->unit_charge, 1000);
#endif

#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 10);
	Temp_Value = Temp_Value + 5;
	do_div(Temp_Value, 10);
#else
	Temp_Value = div_s64(Temp_Value, 10);
	Temp_Value = Temp_Value + 5;
	Temp_Value = div_s64(Temp_Value, 10);
#endif

	if (uvalue32_NCAR_MSB)
		dvalue_NCAR = (signed int)(Temp_Value - (Temp_Value * 2));
	else
		dvalue_NCAR = (signed int)Temp_Value;

	/*Auto adjust value*/
	if (gauge_dev->hw_status.r_fg_value != priv->default_r_fg)
		dvalue_NCAR = (dvalue_NCAR * priv->default_r_fg) / gauge_dev->hw_status.r_fg_value;

	gauge_dev->fg_hw_info.ncar =
		((dvalue_NCAR * gauge_dev->gm->fg_cust_data.car_tune_value) / 1000);

}

static int fgauge_get_time(struct mtk_gauge *gauge_dev, unsigned int *ptime)
{
	unsigned int ret_time;
	u32 time_regval = 0;
	long long time = 0;

	pre_gauge_update(gauge_dev);

	regmap_raw_read(gauge_dev->regmap, RG_FGADC_NTER_CON0, &time_regval, sizeof(time_regval));
	time = time_regval & FGADC_NTER_MASK;

#if defined(__LP64__) || defined(_LP64)
	time = time * UNIT_TIME / 100;
#else
	time = div_s64(time * UNIT_TIME, 100);
#endif
	ret_time = time;

	bm_debug("[%s] regval:0x%x rtime:0x%llx 0x%x!\r\n",  __func__, time_regval, time, ret_time);

	post_gauge_update(gauge_dev);

	*ptime = ret_time;

	return 0;
}

static int nafg_check_corner(struct mtk_gauge *gauge)
{
	int nag_vbat = 0;
	int setto_cdltv_thr_mv = 0;
	int get_c_dltv_mv = 0;
	u32 nag_c_dltv_value = 0;
	signed int nag_c_dltv_reg_value;
	bool bcheckbit10;
	int nag_zcv = gauge->nafg_zcv_mv;

	setto_cdltv_thr_mv = gauge->nafg_c_dltv_mv;

	/*AUXADC_NAG_7*/
	regmap_raw_read(gauge->regmap, RG_AUXADC_NAG_13, &nag_c_dltv_value,
			sizeof(nag_c_dltv_value));
	nag_c_dltv_value &= AUXADC_NAG_C_DLTV_MASK;

	bcheckbit10 = nag_c_dltv_value & BIT(26);
	nag_c_dltv_reg_value = nag_c_dltv_value;

	if (bcheckbit10)
		nag_c_dltv_reg_value |= 0xF8000000;

	get_c_dltv_mv = reg_to_mv_value(nag_c_dltv_reg_value);
	nag_vbat = get_nafg_vbat(gauge);

	if (nag_vbat < 31500 && nag_zcv > 31500)
		gauge->nafg_corner = 1;
	else if (nag_zcv < 31500 && nag_vbat > 31500)
		gauge->nafg_corner = 2;
	else
		gauge->nafg_corner = 0;

	bm_debug("%s:corner:%d nag_vbat:%d nag_zcv:%d get_c_dltv_mv:%d setto_cdltv_thr_mv:%d, RG[0x%x]\n",
		__func__, gauge->nafg_corner, nag_vbat, nag_zcv, get_c_dltv_mv,
		setto_cdltv_thr_mv, nag_c_dltv_value);

	return 0;
}

static int coulomb_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	struct mt6375_priv *priv = container_of(gauge, struct mt6375_priv, gauge);
	unsigned int uvalue32_car = 0;
	unsigned int uvalue32_car_msb = 0;
	u32 temp_car = 0;
	signed int dvalue_CAR = 0;
	long long temp_value = 0;
	int r_fg_value;
	int car_tune_value;
	int ret = 0;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->gm->fg_cust_data.car_tune_value;
	pre_gauge_update(gauge);

	ret = regmap_raw_read(gauge->regmap, RG_FGADC_CAR_CON0, &temp_car,
			      sizeof(temp_car));
	if (ret) {
		pr_notice("%s error, ret = %d\n", __func__, ret);
		return ret;
	}

	post_gauge_update(gauge);

	uvalue32_car = temp_car & 0x7fffffff;
	uvalue32_car_msb = (temp_car & BIT(31)) >> 31;

	/* calculate the real world data */
	dvalue_CAR = (signed int) uvalue32_car;

	if (uvalue32_car == 0) {
		temp_value = 0;
	} else if (uvalue32_car_msb) {
		/* dis-charging */
		temp_value = (long long) (dvalue_CAR - 0x7fffffff);
		/* keep negative value */
		temp_value = temp_value - (temp_value * 2);
	} else {
		/*charging */
		temp_value = (long long) dvalue_CAR;
	}

#if defined(__LP64__) || defined(_LP64)
	temp_value = temp_value * priv->unit_charge / 1000;
#else
	temp_value = div_s64(temp_value * priv->unit_charge, 1000);
#endif


#if defined(__LP64__) || defined(_LP64)
	do_div(temp_value, 10);
	temp_value = temp_value + 5;
	do_div(temp_value, 10);
#else
	temp_value = div_s64(temp_value, 10);
	temp_value = temp_value + 5;
	temp_value = div_s64(temp_value, 10);
#endif


	if (uvalue32_car_msb)
		dvalue_CAR = (signed int) (temp_value - (temp_value * 2));
		/* keep negative value */
	else
		dvalue_CAR = (signed int) temp_value;


	bm_debug("[%s]l:0x%x h:0x%x val:%d msb:%d car:%d\n",
		__func__, temp_car & 0xFFFF, (temp_car & 0xFFFF0000) >> 16, uvalue32_car, uvalue32_car_msb, dvalue_CAR);

	/*Auto adjust value*/
	if (r_fg_value != priv->default_r_fg && r_fg_value != 0) {
		bm_debug("[%s] Auto adjust value deu to the Rfg is %d\n Ori CAR=%d",
			 __func__, r_fg_value, dvalue_CAR);

		dvalue_CAR = (dvalue_CAR * priv->default_r_fg) / r_fg_value;

		bm_debug("[%s] new CAR=%d\n", __func__, dvalue_CAR);
	}

	dvalue_CAR = (dvalue_CAR * car_tune_value) / 1000;

	bm_debug("[%s] CAR=%d r_fg_value=%d car_tune_value=%d\n",
		__func__, dvalue_CAR, r_fg_value, car_tune_value);

	*val = dvalue_CAR;

	return 0;
}

static int average_current_get(struct mtk_gauge *gauge_dev, struct mtk_gauge_sysfs_field_info *attr,
			       int *data)
{
	struct mt6375_priv *priv = container_of(gauge_dev, struct mt6375_priv, gauge);
	long long fg_iavg_reg = 0;
	long long fg_iavg_reg_tmp = 0;
	long long fg_iavg_ma = 0;
	u16 fg_iavg_reg_27_16 = 0;
	u16 fg_iavg_reg_15_00 = 0;
	int sign_bit = 0;
	int is_bat_charging;
	int iavg_vld = 0;
	int r_fg_value, car_tune_value;
	int ret = 0;

	r_fg_value = gauge_dev->hw_status.r_fg_value;
	car_tune_value = gauge_dev->gm->fg_cust_data.car_tune_value;

	pre_gauge_update(gauge_dev);

	ret = regmap_read(gauge_dev->regmap, RG_FGADC_IAVG_CON1, &iavg_vld);
	if (ret) {
		pr_notice("%s error, ret = %d\n", __func__, ret);
		return ret;
	}
	iavg_vld = iavg_vld & FG_IAVG_VLD_MASK;

	if (iavg_vld) {
		ret = regmap_raw_read(gauge_dev->regmap, RG_FGADC_IAVG_CON2,
				      &fg_iavg_reg_27_16,
				      sizeof(fg_iavg_reg_27_16));
		if (ret) {
			pr_notice("%s error, ret = %d\n", __func__, ret);
			return ret;
		}
		fg_iavg_reg_27_16 &= FG_IAVG_27_16_MASK;

		ret = regmap_raw_read(gauge_dev->regmap, RG_FGADC_IAVG_CON0,
				      &fg_iavg_reg_15_00,
				      sizeof(fg_iavg_reg_15_00));
		if (ret) {
			pr_notice("%s error, ret = %d\n", __func__, ret);
			return ret;
		}
		fg_iavg_reg_15_00 &= FG_IAVG_15_00_MASK;

		fg_iavg_reg = fg_iavg_reg_27_16;
		fg_iavg_reg = ((long long)fg_iavg_reg << 16) + fg_iavg_reg_15_00;

		sign_bit = (fg_iavg_reg_27_16 & BIT(11)) >> 11;

		if (sign_bit) {
			fg_iavg_reg_tmp = fg_iavg_reg;
			/*fg_iavg_reg = fg_iavg_reg_tmp - 0xfffffff - 1;*/
			fg_iavg_reg = 0xfffffff - fg_iavg_reg_tmp + 1;
		}

		is_bat_charging = sign_bit ? 0 : 1;

		fg_iavg_ma = fg_iavg_reg * priv->unit_fg_iavg * car_tune_value;

		bm_debug(
			"[fg_get_current_iavg] fg_iavg_ma %lld fg_iavg_reg %lld fg_iavg_reg_tmp %lld\n",
			fg_iavg_ma, fg_iavg_reg, fg_iavg_reg_tmp);

#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_ma, 1000000);
#else
		fg_iavg_ma = div_s64(fg_iavg_ma, 1000000);
#endif


		if (r_fg_value != priv->default_r_fg && r_fg_value != 0) {
#if defined(__LP64__) || defined(_LP64)
			fg_iavg_ma = (fg_iavg_ma * priv->default_r_fg / r_fg_value);
#else
			fg_iavg_ma = div_s64(fg_iavg_ma * priv->default_r_fg, r_fg_value);
#endif
		}

#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_ma, 100);
#else
		fg_iavg_ma = div_s64(fg_iavg_ma, 100);
#endif

		bm_debug("[fg_get_current_iavg] fg_iavg_ma %lld\n",
			fg_iavg_ma);


		if (sign_bit)
			fg_iavg_ma = 0 - fg_iavg_ma;

		bm_debug(
			"[fg_get_current_iavg] fg_iavg_ma %lld fg_iavg_reg %lld r_fg_value %d 27_16 0x%x 15_00 0x%x\n",
			fg_iavg_ma, fg_iavg_reg, r_fg_value, fg_iavg_reg_27_16, fg_iavg_reg_15_00);

		gauge_dev->fg_hw_info.current_avg = fg_iavg_ma;
		gauge_dev->fg_hw_info.current_avg_sign = sign_bit;
		bm_debug("[fg_get_current_iavg] PMIC_FG_IAVG_VLD == 1\n");
	} else {
		ret = read_fg_hw_info_current_1(gauge_dev,
			&gauge_dev->fg_hw_info.current_1);
		if (ret) {
			pr_notice("%s error, ret = %d\n", __func__, ret);
			return ret;
		}
		gauge_dev->fg_hw_info.current_avg = gauge_dev->fg_hw_info.current_1;

		if (gauge_dev->fg_hw_info.current_1 < 0)
			gauge_dev->fg_hw_info.current_avg_sign = 1;

		bm_debug("[fg_get_current_iavg] PMIC_FG_IAVG_VLD != 1, avg %d, current_1 %d\n",
			gauge_dev->fg_hw_info.current_avg,
			gauge_dev->fg_hw_info.current_1);
	}

	post_gauge_update(gauge_dev);
	*data = gauge_dev->fg_hw_info.current_avg;

	gauge_dev->fg_hw_info.current_avg_valid = iavg_vld;
	bm_debug("[fg_get_current_iavg] %d %d\n", *data, iavg_vld);

	return 0;
}

static int bat_temp_froze_en_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
				 int val)
{
	regmap_update_bits(gauge->regmap, RG_AUXADC_BAT_TEMP_1, AUXADC_BAT_TEMP_FROZE_EN_MASK,
			   val ? AUXADC_BAT_TEMP_FROZE_EN_MASK : 0);

	return 0;
}

static int bat_tmp_lt_threshold_set(struct mtk_gauge *gauge,
				    struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	int tmp_int_lt = mv_to_reg_12_temp_value(threshold);
	u16 regval;

	regval = tmp_int_lt & AUXADC_BAT_TEMP_VOLT_MAX_MASK;
	/* max is low temp */
	regmap_raw_write(gauge->regmap, RG_AUXADC_BAT_TEMP_4, &regval, sizeof(regval));

	bm_debug("[%s]mv:%d reg:%d\n", __func__, threshold, tmp_int_lt);
	return 0;
}

static int bat_tmp_ht_threshold_set(struct mtk_gauge *gauge,
				    struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	int tmp_int_ht = mv_to_reg_12_temp_value(threshold);
	u16 regval;

	regval = tmp_int_ht & AUXADC_BAT_TEMP_VOLT_MIN_MASK;
	/* min is high temp */
	regmap_raw_write(gauge->regmap, RG_AUXADC_BAT_TEMP_7, &regval, sizeof(regval));

	bm_debug("[%s]mv:%d reg:%d\n", __func__, threshold, tmp_int_ht);

	return 0;
}

static int en_bat_tmp_lt_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			     int en)
{
	if (en == 0) {
		disable_gauge_irq(gauge, BAT_TMP_L_IRQ);
		regmap_update_bits(gauge->regmap, RG_AUXADC_BAT_TEMP_3,
				   AUXADC_BAT_TEMP_DET_MAX_MASK | AUXADC_BAT_TEMP_IRQ_EN_MAX_MASK,
				   0);
		regmap_update_bits(gauge->regmap, RG_AUXADC_BAT_TEMP_0, AUXADC_BAT_TEMP_EN_MASK, 0);
	} else {
		/* unit: 0x10 = 2, means 5 second */
		regmap_update_bits(gauge->regmap, RG_AUXADC_BAT_TEMP_2,
				   AUXADC_BAT_TEMP_DET_PRD_SEL_MASK,
				   2 << AUXADC_BAT_TEMP_DET_PRD_SEL_SHIFT);

		/* debounce 0x10 = 2 , means 4 times*/
		/* 5s * 4 times = 20s to issue bat_temp interrupt */
		regmap_update_bits(gauge->regmap, RG_AUXADC_BAT_TEMP_2,
				   AUXADC_BAT_TEMP_DEBT_MAX_SEL_MASK,
				   2 << AUXADC_BAT_TEMP_DEBT_MAX_SEL_SHIFT);

		enable_gauge_irq(gauge, BAT_TMP_L_IRQ);

		regmap_update_bits(gauge->regmap, RG_AUXADC_BAT_TEMP_3,
				   AUXADC_BAT_TEMP_DET_MAX_MASK | AUXADC_BAT_TEMP_IRQ_EN_MAX_MASK,
				   AUXADC_BAT_TEMP_DET_MAX_MASK | AUXADC_BAT_TEMP_IRQ_EN_MAX_MASK);
		regmap_update_bits(gauge->regmap, RG_AUXADC_BAT_TEMP_0, AUXADC_BAT_TEMP_EN_MASK,
				   AUXADC_BAT_TEMP_EN_MASK);
	}

	bm_debug("[%s]en:%d\n", __func__, en);

	return 0;
}

static int en_bat_tmp_ht_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			     int en)
{
	if (en == 0) {
		disable_gauge_irq(gauge, BAT_TMP_H_IRQ);
		regmap_update_bits(gauge->regmap, RG_AUXADC_BAT_TEMP_6,
				   AUXADC_BAT_TEMP_DET_MIN_MASK | AUXADC_BAT_TEMP_IRQ_EN_MIN_MASK,
				   0);
		regmap_update_bits(gauge->regmap, RG_AUXADC_BAT_TEMP_0, AUXADC_BAT_TEMP_EN_MASK, 0);
	} else {
		/* unit: 0x10 = 2, means 5 second */
		regmap_update_bits(gauge->regmap, RG_AUXADC_BAT_TEMP_2,
				   AUXADC_BAT_TEMP_DET_PRD_SEL_MASK,
				   2 << AUXADC_BAT_TEMP_DET_PRD_SEL_SHIFT);

		/* debounce 0x10 = 2 , means 4 times*/
		/* 5s * 4 times = 20s to issue bat_temp interrupt */
		regmap_update_bits(gauge->regmap, RG_AUXADC_BAT_TEMP_2,
				   AUXADC_BAT_TEMP_DEBT_MIN_SEL_MASK,
				   2 << AUXADC_BAT_TEMP_DEBT_MIN_SEL_SHIFT);

		enable_gauge_irq(gauge, BAT_TMP_H_IRQ);

		regmap_update_bits(gauge->regmap, RG_AUXADC_BAT_TEMP_6,
				   AUXADC_BAT_TEMP_DET_MIN_MASK | AUXADC_BAT_TEMP_IRQ_EN_MIN_MASK,
				   AUXADC_BAT_TEMP_DET_MIN_MASK | AUXADC_BAT_TEMP_IRQ_EN_MIN_MASK);
		regmap_update_bits(gauge->regmap, RG_AUXADC_BAT_TEMP_0, AUXADC_BAT_TEMP_EN_MASK,
				   AUXADC_BAT_TEMP_EN_MASK);
	}

	bm_debug("[%s]en:%d\n", __func__, en);

	return 0;
}

static int event_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int event)
{
	if (event == EVT_INT_NAFG_CHECK)
		nafg_check_corner(gauge);

	return 0;
}

static signed int fg_set_iavg_intr(struct mtk_gauge *gauge_dev, void *data)
{
	struct mt6375_priv *priv = container_of(gauge_dev, struct mt6375_priv, gauge);
	int iavg_gap = *(unsigned int *) (data);
	int iavg;
	long long iavg_ht, iavg_lt;
	long long fg_iavg_reg_ht, fg_iavg_reg_lt;
	int fg_iavg_lth_28_16, fg_iavg_lth_15_00;
	int fg_iavg_hth_28_16, fg_iavg_hth_15_00;
	u32 regval;
	int ret = 0;

	ret = average_current_get(gauge_dev, NULL, &iavg);
	if (ret) {
		pr_notice("%s error, ret = %d\n", __func__, ret);
		return ret;
	}

	iavg_ht = abs(iavg) + iavg_gap;
	iavg_lt = abs(iavg) - iavg_gap;

	if (iavg_lt <= 0)
		iavg_lt = 0;

	gauge_dev->hw_status.iavg_ht = iavg_ht;
	gauge_dev->hw_status.iavg_lt = iavg_lt;

/* reverse for IAVG */
/* fg_iavg_ma * 100 * fg_cust_data.r_fg_value / DEFAULT_RFG * 1000 * 1000 */
/* / fg_cust_data.car_tune_value / priv->unit_fg_iavg  = fg_iavg_reg  */

	fg_iavg_reg_ht = iavg_ht * 100;
	if (gauge_dev->hw_status.r_fg_value != priv->default_r_fg) {
		fg_iavg_reg_ht = fg_iavg_reg_ht * gauge_dev->hw_status.r_fg_value;
#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_reg_ht, priv->default_r_fg);
#else
		fg_iavg_reg_ht = div_s64(fg_iavg_reg_ht, priv->default_r_fg);
#endif
	}

	fg_iavg_reg_ht = fg_iavg_reg_ht * 1000000;

#if defined(__LP64__) || defined(_LP64)
	do_div(fg_iavg_reg_ht, priv->unit_fg_iavg);
	do_div(fg_iavg_reg_ht, gauge_dev->gm->fg_cust_data.car_tune_value);
#else
	fg_iavg_reg_ht = div_s64(fg_iavg_reg_ht, priv->unit_fg_iavg);
	fg_iavg_reg_ht = div_s64(fg_iavg_reg_ht, gauge_dev->gm->fg_cust_data.car_tune_value);
#endif


	fg_iavg_reg_lt = iavg_lt * 100;

	if (gauge_dev->hw_status.r_fg_value != priv->default_r_fg) {
		fg_iavg_reg_lt = fg_iavg_reg_lt *
			gauge_dev->hw_status.r_fg_value;
#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_reg_lt, priv->default_r_fg);
#else
		fg_iavg_reg_lt = div_s64(fg_iavg_reg_lt, priv->default_r_fg);
#endif
	}

	fg_iavg_reg_lt = fg_iavg_reg_lt * 1000000;

#if defined(__LP64__) || defined(_LP64)
	do_div(fg_iavg_reg_lt, priv->unit_fg_iavg);
	do_div(fg_iavg_reg_lt, gauge_dev->gm->fg_cust_data.car_tune_value);
#else
	fg_iavg_reg_lt = div_s64(fg_iavg_reg_lt, priv->unit_fg_iavg);
	fg_iavg_reg_lt = div_s64(fg_iavg_reg_lt,
				gauge_dev->gm->fg_cust_data.car_tune_value);
#endif

	fg_iavg_lth_28_16 = (fg_iavg_reg_lt & 0x1fff0000) >> 16;
	fg_iavg_lth_15_00 = fg_iavg_reg_lt & 0xffff;
	fg_iavg_hth_28_16 = (fg_iavg_reg_ht & 0x1fff0000) >> 16;
	fg_iavg_hth_15_00 = fg_iavg_reg_ht & 0xffff;

	disable_gauge_irq(gauge_dev, FG_IAVG_H_IRQ);
	disable_gauge_irq(gauge_dev, FG_IAVG_L_IRQ);

	regval = fg_iavg_lth_28_16 << 16 | fg_iavg_lth_15_00;
	regmap_raw_write(gauge_dev->regmap, RG_FGADC_IAVG_CON3, &regval, sizeof(regval));

	regval = fg_iavg_hth_28_16 << 16 | fg_iavg_hth_15_00;
	regmap_raw_write(gauge_dev->regmap, RG_FGADC_IAVG_CON5, &regval, sizeof(regval));

	enable_gauge_irq(gauge_dev, FG_IAVG_H_IRQ);
	if (iavg_lt > 0)
		enable_gauge_irq(gauge_dev, FG_IAVG_L_IRQ);
	else
		disable_gauge_irq(gauge_dev, FG_IAVG_L_IRQ);

	bm_debug("[FG_IAVG_INT][%s] iavg %d iavg_gap %d iavg_ht %lld iavg_lt %lld fg_iavg_reg_ht %lld fg_iavg_reg_lt %lld\n",
			__func__, iavg, iavg_gap, iavg_ht, iavg_lt,
			fg_iavg_reg_ht, fg_iavg_reg_lt);

	bm_debug("[FG_IAVG_INT][%s] lt_28_16 0x%x lt_15_00 0x%x ht_28_16 0x%x ht_15_00 0x%x\n",
			__func__, fg_iavg_lth_28_16, fg_iavg_lth_15_00,
			fg_iavg_hth_28_16, fg_iavg_hth_15_00);

	return 0;
}

static int hw_info_set(struct mtk_gauge *gauge_dev, struct mtk_gauge_sysfs_field_info *attr, int en)
{
	int ret = 0;
	int is_iavg_valid;
	int avg_current;
	int iavg_th;
	unsigned int time;
	struct gauge_hw_status *gauge_status;

	gauge_status = &gauge_dev->hw_status;
	/* Set Read Latchdata */
	post_gauge_update(gauge_dev);

	/* Current_1 */
	read_fg_hw_info_current_1(gauge_dev, &gauge_dev->fg_hw_info.current_1);

	/* Current_2 */
	read_fg_hw_info_current_2(gauge_dev);

	/* curr_out = pmic_get_register_value(PMIC_FG_CURRENT_OUT); */
	/* fg_offset = pmic_get_register_value(PMIC_FG_OFFSET); */

	/* Iavg */
	ret = average_current_get(gauge_dev, NULL, &avg_current);
	if (ret) {
		pr_notice("%s error, ret = %d\n", __func__, ret);
		return ret;
	}
	is_iavg_valid = gauge_dev->fg_hw_info.current_avg_valid;
	if ((is_iavg_valid == 1) && (gauge_status->iavg_intr_flag == 0)) {
		bm_debug("[read_fg_hw_info]set first fg_set_iavg_intr %d %d\n",
			is_iavg_valid, gauge_status->iavg_intr_flag);
		gauge_status->iavg_intr_flag = 1;
		iavg_th = gauge_dev->gm->fg_cust_data.diff_iavg_th;
		ret = fg_set_iavg_intr(gauge_dev, &iavg_th);
	} else if (is_iavg_valid == 0) {
		gauge_status->iavg_intr_flag = 0;
		disable_gauge_irq(gauge_dev, FG_IAVG_H_IRQ);
		disable_gauge_irq(gauge_dev, FG_IAVG_L_IRQ);
		bm_debug(
			"[read_fg_hw_info] doublecheck first fg_set_iavg_intr %d %d\n",
			is_iavg_valid, gauge_status->iavg_intr_flag);
	}
	bm_debug("[read_fg_hw_info] thirdcheck first fg_set_iavg_intr %d %d\n",
		is_iavg_valid, gauge_status->iavg_intr_flag);

	/* Ncar */
	read_fg_hw_info_ncar(gauge_dev);

	/* recover read */
	post_gauge_update(gauge_dev);

	coulomb_get(gauge_dev, NULL, &gauge_dev->fg_hw_info.car);
	fgauge_get_time(gauge_dev, &time);
	gauge_dev->fg_hw_info.time = time;

	bm_debug("[FGADC_intr_end][read_fg_hw_info] curr_1 %d curr_2 %d Iavg %d sign %d car %d ncar %d time %d\n",
		gauge_dev->fg_hw_info.current_1,
		gauge_dev->fg_hw_info.current_2,
		gauge_dev->fg_hw_info.current_avg,
		gauge_dev->fg_hw_info.current_avg_sign,
		gauge_dev->fg_hw_info.car,
		gauge_dev->fg_hw_info.ncar, gauge_dev->fg_hw_info.time);

	return 0;
}

static int bat_cycle_intr_threshold_set(struct mtk_gauge *gauge,
					struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	struct mt6375_priv *priv = container_of(gauge, struct mt6375_priv, gauge);
	long long car = threshold;
	long long carReg;
	u32 regval;

	disable_gauge_irq(gauge, FG_N_CHARGE_L_IRQ);

#if defined(__LP64__) || defined(_LP64)
	car = car * 100000 / priv->unit_charge;
	/* 1000 * 100 */
#else
	car = div_s64(car * 100000, priv->unit_charge);
#endif

	if (gauge->hw_status.r_fg_value != priv->default_r_fg) {
		car = (car * gauge->hw_status.r_fg_value);
#if defined(__LP64__) || defined(_LP64)
		do_div(car, priv->default_r_fg);
#else
		car = div_s64(car, priv->default_r_fg);
#endif
	}

	car = car * 1000;
#if defined(__LP64__) || defined(_LP64)
	do_div(car, gauge->gm->fg_cust_data.car_tune_value);
#else
	car = div_s64(car, gauge->gm->fg_cust_data.car_tune_value);
#endif

	carReg = car;
	carReg = 0 - carReg;

	regval = carReg & FG_N_CHARGE_TH_MASK;
	regmap_raw_write(gauge->regmap, RG_FGADC_NCAR_CON2, &regval, sizeof(regval));

	bm_err("car:%d carR:%lld r:%lld\n", threshold, car, carReg);

	enable_gauge_irq(gauge, FG_N_CHARGE_L_IRQ);

	return 0;

}

static int ncar_reset_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	regmap_update_bits(gauge->regmap, RG_FGADC_CON2, FG_N_CHARGE_RST_MASK,
			   FG_N_CHARGE_RST_MASK);
	udelay(200);
	regmap_update_bits(gauge->regmap, RG_FGADC_CON2, FG_N_CHARGE_RST_MASK, 0);

	return 0;
}

static int soff_reset_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int en)
{
	return 0;
}

static int zcv_intr_en_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			   int en)
{
	static int cnt;

	bm_debug("%s %d %d\n", __func__, cnt, en);

	cnt = en ? cnt + 1 : cnt - 1;

	if (en == 0) {
		disable_gauge_irq(gauge, ZCV_IRQ);
		regmap_update_bits(gauge->regmap, RG_FGADC_CON0, FG_ZCV_DET_EN_MASK, 0);
		mdelay(1);
	} else if (en == 1) {
		enable_gauge_irq(gauge, ZCV_IRQ);
		regmap_update_bits(gauge->regmap, RG_FGADC_CON0, FG_ZCV_DET_EN_MASK,
				   FG_ZCV_DET_EN_MASK);
	}

	bm_debug("[FG_ZCV_INT][fg_set_zcv_intr_en] En %d\n", en);

	return 0;
}

static int zcv_intr_threshold_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
				  int zcv_avg_current)
{
	int fg_zcv_det_time;
	int fg_zcv_car_th = 0;

	fg_zcv_det_time = gauge->gm->fg_cust_data.zcv_suspend_time;
	fg_zcv_car_th = (fg_zcv_det_time + 1) * 4 * zcv_avg_current / 60;

	bm_debug("[%s] current:%d, fg_zcv_det_time:%d, fg_zcv_car_th:%d\n",
		__func__, zcv_avg_current, fg_zcv_det_time, fg_zcv_car_th);

	fgauge_set_zcv_intr_internal(gauge, fg_zcv_det_time, fg_zcv_car_th);

	return 0;
}

static int bat_plugout_en_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			      int val)
{
	if (!!val)
		enable_gauge_irq(gauge, BAT_PLUGOUT_IRQ);
	else
		disable_gauge_irq(gauge, BAT_PLUGOUT_IRQ);

	return 0;
}

static int gauge_initialized_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
				 int *val)
{
	unsigned int fg_reset_status = 0;

	regmap_read(gauge->regmap, RG_FGADC_RST_CON0, &fg_reset_status);
	*val = fg_reset_status & FG_RSTB_STATUS_MASK;

	return 0;
}

static int gauge_initialized_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
				 int val)
{
	regmap_update_bits(gauge->regmap, RG_FGADC_RST_CON0, FG_RSTB_STATUS_MASK,
			   val ? FG_RSTB_STATUS_MASK : 0);

	return 0;
}

static int reset_fg_rtc_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			    int val)
{
	int hw_id = 0x6375;
	int temp_value;
	u8 spare0_reg, after_rst_spare0_reg;
	u8 spare3_reg, after_rst_spare3_reg;

	if (gauge->gm->disableGM30)
		return 0;

	fgauge_read_RTC_boot_status(gauge);

	/* read spare0 */
	spare0_reg = get_rtc_spare0_fg_value(gauge);

	/* raise 15b to reset */
	if ((hw_id & 0xff00) == 0x3500) {
		temp_value = 0x80;
		set_rtc_spare0_fg_value(gauge, temp_value);
		mdelay(1);
		temp_value = 0x00;
		set_rtc_spare0_fg_value(gauge, temp_value);
	} else {
		temp_value = 0x80;
		set_rtc_spare0_fg_value(gauge, temp_value);
		mdelay(1);
		temp_value = 0x20;
		set_rtc_spare0_fg_value(gauge, temp_value);
	}

	/* read spare0 again */
	after_rst_spare0_reg = get_rtc_spare0_fg_value(gauge);

	/* read spare3 */
	spare3_reg = get_rtc_spare_fg_value(gauge);

	/* set spare3 0x7f */
	set_rtc_spare_fg_value(gauge, spare3_reg | 0x80);

	/* read spare3 again */
	after_rst_spare3_reg = get_rtc_spare_fg_value(gauge);

	bm_err("[fgauge_read_RTC_boot_status] spare0 0x%x 0x%x, spare3 0x%x 0x%x\n",
		spare0_reg, after_rst_spare0_reg, spare3_reg, after_rst_spare3_reg);

	return 0;
}

static int nafg_vbat_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			 int *vbat)
{
	*vbat = get_nafg_vbat(gauge);
	return 0;
}

static int nafg_zcv_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int zcv)
{
	gauge->nafg_zcv_mv = zcv;	/* 0.1 mv*/
	return 0;
}

static int nafg_en_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	static int cnt;

	bm_debug("%s %d %d\n", __func__, cnt, val);

	cnt = val ? cnt + 1 : cnt - 1;

	if (val) {
		enable_gauge_irq(gauge, NAFG_IRQ);
		bm_debug("[%s]enable:%d\n", __func__, val);
	} else {
		disable_gauge_irq(gauge, NAFG_IRQ);
		bm_debug("[%s]disable:%d\n", __func__, val);
	}

	regmap_update_bits(gauge->regmap, RG_AUXADC_NAG_0,
			   AUXADC_NAG_IRQ_EN_MASK | AUXADC_NAG_EN_MASK,
			   val ? (AUXADC_NAG_IRQ_EN_MASK | AUXADC_NAG_EN_MASK) : 0);

	return 0;
}

static int nafg_c_dltv_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			   int *nafg_c_dltv)
{
	signed int nag_c_dltv_value;
	signed int nag_c_dltv_value_h;
	signed int nag_c_dltv_reg_value;
	signed int nag_c_dltv_mv_value;
	bool bcheckbit10;
	u32 nag_c_dltv_regval = 0;

	regmap_raw_read(gauge->regmap, RG_AUXADC_NAG_13, &nag_c_dltv_regval,
			sizeof(nag_c_dltv_regval));
	nag_c_dltv_regval &= AUXADC_NAG_C_DLTV_MASK;

	nag_c_dltv_value = nag_c_dltv_regval & 0xffff;
	nag_c_dltv_value_h = nag_c_dltv_regval >> 16;

	bcheckbit10 = nag_c_dltv_value_h & 0x0400;

	if (gauge->nafg_corner == 1) {
		nag_c_dltv_reg_value = (nag_c_dltv_value & 0x7fff);
		nag_c_dltv_mv_value = reg_to_mv_value(nag_c_dltv_reg_value);
		*nafg_c_dltv = nag_c_dltv_mv_value;

		bm_debug("[fg_bat_nafg][%s] mV:Reg[%d:%d] [b10:%d][26_16(0x%04x) 15_00(0x%04x)] corner:%d\n",
			__func__, nag_c_dltv_mv_value, nag_c_dltv_reg_value,
			bcheckbit10, nag_c_dltv_value_h, nag_c_dltv_value,
			gauge->nafg_corner);
		return 0;
	} else if (gauge->nafg_corner == 2) {
		nag_c_dltv_reg_value = (nag_c_dltv_value - 32768);
		nag_c_dltv_mv_value = reg_to_mv_value(nag_c_dltv_reg_value);
		*nafg_c_dltv = nag_c_dltv_mv_value;

		bm_debug("[fg_bat_nafg][%s] mV:Reg[%d:%d] [b10:%d][26_16(0x%04x) 15_00(0x%04x)] corner:%d\n",
			__func__, nag_c_dltv_mv_value, nag_c_dltv_reg_value,
			bcheckbit10, nag_c_dltv_value_h, nag_c_dltv_value,
			gauge->nafg_corner);
		return 0;
	}

	if (bcheckbit10 == 0)
		nag_c_dltv_reg_value = (nag_c_dltv_value & 0xffff) +
				((nag_c_dltv_value_h & 0x07ff) << 16);
	else
		nag_c_dltv_reg_value = (nag_c_dltv_value & 0xffff) +
			(((nag_c_dltv_value_h | 0xf800) & 0xffff) << 16);

	nag_c_dltv_mv_value = reg_to_mv_value(nag_c_dltv_reg_value);
	*nafg_c_dltv = nag_c_dltv_mv_value;

	bm_debug("[fg_bat_nafg][%s] mV:Reg[%d:%d] [b10:%d][26_16(0x%04x) 15_00(0x%04x)] corner:%d\n",
		__func__, nag_c_dltv_mv_value, nag_c_dltv_reg_value,
		bcheckbit10, nag_c_dltv_value_h, nag_c_dltv_value, gauge->nafg_corner);

	return 0;
}

static int nafg_c_dltv_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			   int c_dltv_mv)
{
	gauge->nafg_c_dltv_mv = c_dltv_mv;	/* 0.1 mv*/
	fgauge_set_nafg_intr_internal(gauge, gauge->gm->fg_cust_data.nafg_time_setting,
				      gauge->nafg_zcv_mv, gauge->nafg_c_dltv_mv);

	return 0;
}

static int nafg_dltv_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			 int *nag_dltv)
{
	u16 nag_dltv_reg_value = 0;
	signed int nag_dltv_mv_value;
	short reg_value;

	/*AUXADC_NAG_4*/
	regmap_raw_read(gauge->regmap, RG_AUXADC_NAG_11, &nag_dltv_reg_value,
			sizeof(nag_dltv_reg_value));

	reg_value = nag_dltv_reg_value & 0xffff;

	nag_dltv_mv_value = reg_to_mv_value(nag_dltv_reg_value);
	*nag_dltv = nag_dltv_mv_value;

	bm_debug("[fg_bat_nafg][%s] mV:Reg [%d:%d] [%d:%d]\n", __func__, nag_dltv_mv_value,
		 nag_dltv_reg_value, reg_to_mv_value(reg_value), reg_value);

	return 0;
}

static int nafg_cnt_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			int *nag_cnt)
{
	u32 NAG_C_DLTV_CNT = 0;

	regmap_raw_read(gauge->regmap, RG_AUXADC_NAG_7, &NAG_C_DLTV_CNT, sizeof(NAG_C_DLTV_CNT));
	*nag_cnt = NAG_C_DLTV_CNT & AUXADC_NAG_CNT_MASK;
	bm_debug("[fg_bat_nafg][%s] %d [25_0 %d]\n", __func__, *nag_cnt, NAG_C_DLTV_CNT);

	return 0;
}

static int zcv_current_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
		    int *zcv_current)
{
	struct mt6375_priv *priv = container_of(gauge, struct mt6375_priv, gauge);
	u16 uvalue16 = 0;
	signed int dvalue = 0;
	long long Temp_Value = 0;

	regmap_raw_read(gauge->regmap, RG_FGADC_ZCV_CON2, &uvalue16, sizeof(uvalue16));

	dvalue = uvalue16;
	if (dvalue == 0) {
		Temp_Value = (long long) dvalue;
	} else if (dvalue > 32767) {
		/* > 0x8000 */
		Temp_Value = (long long) (dvalue - 65535);
		Temp_Value = Temp_Value - (Temp_Value * 2);
	} else {
		Temp_Value = (long long) dvalue;
	}

	Temp_Value = Temp_Value * priv->unit_fgcurrent;

#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 100000);
#else
	Temp_Value = div_s64(Temp_Value, 100000);
#endif
	dvalue = (unsigned int) Temp_Value;

	/* Auto adjust value */
	if (gauge->gm->fg_cust_data.r_fg_value != priv->default_r_fg) {
		bm_debug(
		"[fgauge_read_current] Auto adjust value due to the Rfg is %d\n Ori curr=%d",
		gauge->gm->fg_cust_data.r_fg_value, dvalue);

		dvalue = (dvalue * priv->default_r_fg) / gauge->gm->fg_cust_data.r_fg_value;

		bm_debug("[fgauge_read_current] new current=%d\n", dvalue);
	}

	bm_debug("[fgauge_read_current] ori current=%d\n", dvalue);
	dvalue = ((dvalue * gauge->gm->fg_cust_data.car_tune_value) / 1000);
	bm_debug("[fgauge_read_current] final current=%d (ratio=%d)\n",
		 dvalue, gauge->gm->fg_cust_data.car_tune_value);
	*zcv_current = dvalue;

	return 0;
}

static int zcv_get(struct mtk_gauge *gauge_dev, struct mtk_gauge_sysfs_field_info *attr, int *zcv)
{
	signed int adc_result_reg = 0;
	signed int adc_result = 0;
	u16 regval = 0;

	regmap_raw_read(gauge_dev->regmap, RG_AUXADC_ADC_OUT_FGADC_PCHR, &regval, sizeof(regval));
	adc_result_reg = regval & AUXADC_ADC_OUT_FGADC_PCHR_MASK;

	adc_result = reg_to_mv_value(adc_result_reg);
	bm_err("[oam] %s BATSNS  (pchr):adc_result_reg=%d, adc_result=%d\n",
		 __func__, adc_result_reg, adc_result);

	*zcv = adc_result;
	return 0;
}

static int get_charger_zcv(struct mtk_gauge *gauge_dev)
{
	struct power_supply *chg_psy;
	union power_supply_propval val;
	int ret = 0;

	chg_psy = power_supply_get_by_name("mtk-master-charger");

	if (chg_psy == NULL) {
		bm_err("[%s] can get charger psy\n", __func__);
		return -ENODEV;
	}

	ret = power_supply_get_property(chg_psy,
		POWER_SUPPLY_PROP_VOLTAGE_BOOT, &val);

	bm_err("[%s]_hw_ocv_chgin=%d, ret=%d\n", __func__, val.intval, ret);

	return val.intval;
}

static int boot_zcv_get(struct mtk_gauge *gauge_dev, struct mtk_gauge_sysfs_field_info *attr,
			int *val)
{
	int _hw_ocv, _sw_ocv;
	int _hw_ocv_src;
	int _prev_hw_ocv, _prev_hw_ocv_src;
	int _hw_ocv_rdy;
	int _flag_unreliable;
	int _hw_ocv_75_pon;
	int _hw_ocv_75_plugin;
	int _hw_ocv_75_before_chgin;
	int _hw_ocv_75_pon_rdy;
	int _hw_ocv_chgin;
	int _hw_ocv_chgin_rdy;
	int now_temp;
	int now_thr;
	int tmp_hwocv_chgin = 0;
	bool fg_is_charger_exist;
	struct mtk_battery *gm;
	struct zcv_data *zcvinfo;
	struct gauge_hw_status *p;

	gm = gauge_dev->gm;
	p = &gauge_dev->hw_status;
	zcvinfo = &gauge_dev->zcv_info;
	_hw_ocv_75_pon_rdy = read_hw_ocv_6375_power_on_rdy(gauge_dev);
	_hw_ocv_75_pon = read_hw_ocv_6375_power_on(gauge_dev);
	_hw_ocv_75_plugin = read_hw_ocv_6375_plug_in(gauge_dev);
	_hw_ocv_75_before_chgin = read_hw_ocv_6375_before_chgin(gauge_dev);

	tmp_hwocv_chgin = get_charger_zcv(gauge_dev);
	if (tmp_hwocv_chgin != -ENODEV)
		_hw_ocv_chgin = tmp_hwocv_chgin / 100;
	else
		_hw_ocv_chgin = 0;

	now_temp = gm->bs_data.bat_batt_temp;

	if (gm == NULL)
		now_thr = 300;
	else {
		if (now_temp > gm->ext_hwocv_swocv_lt_temp)
			now_thr = gm->ext_hwocv_swocv;
		else
			now_thr = gm->ext_hwocv_swocv_lt;
	}

	if (_hw_ocv_chgin < 25000)
		_hw_ocv_chgin_rdy = 0;
	else
		_hw_ocv_chgin_rdy = 1;

	/* if preloader records charge in, need to using subpmic as hwocv */
	fgauge_get_info(gauge_dev, GAUGE_PROP_PL_CHARGING_STATUS, &zcvinfo->pl_charging_status);
	fgauge_set_info(gauge_dev, GAUGE_PROP_PL_CHARGING_STATUS, 0);
	fgauge_get_info(gauge_dev, GAUGE_PROP_MONITER_PLCHG_STATUS, &zcvinfo->moniter_plchg_bit);
	fgauge_set_info(gauge_dev, GAUGE_PROP_MONITER_PLCHG_STATUS, 0);

	if (zcvinfo->pl_charging_status == 1)
		fg_is_charger_exist = 1;
	else
		fg_is_charger_exist = 0;

	_hw_ocv = _hw_ocv_75_pon;
	_sw_ocv = gauge_dev->hw_status.sw_ocv;
	/* _sw_ocv = get_sw_ocv();*/
	_hw_ocv_src = FROM_PMIC_PON_ON;
	_prev_hw_ocv = _hw_ocv;
	_prev_hw_ocv_src = FROM_PMIC_PON_ON;
	_flag_unreliable = 0;

	if (fg_is_charger_exist) {
		_hw_ocv_rdy = _hw_ocv_75_pon_rdy;
		if (_hw_ocv_rdy == 1) {
			if (_hw_ocv_chgin_rdy == 1) {
				_hw_ocv = _hw_ocv_chgin;
				_hw_ocv_src = FROM_CHR_IN;
			} else {
				_hw_ocv = _hw_ocv_75_pon;
				_hw_ocv_src = FROM_PMIC_PON_ON;
			}

			if (abs(_hw_ocv - _sw_ocv) > now_thr) {
				_prev_hw_ocv = _hw_ocv;
				_prev_hw_ocv_src = _hw_ocv_src;
				_hw_ocv = _sw_ocv;
				_hw_ocv_src = FROM_SW_OCV;
				p->flag_hw_ocv_unreliable = true;
				_flag_unreliable = 1;
			}
		} else {
			/* fixme: swocv is workaround */
			/* plug charger poweron but charger not ready */
			/* should use swocv to workaround */
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
			if (_hw_ocv_chgin_rdy != 1) {
				if (abs(_hw_ocv - _sw_ocv) > now_thr) {
					_prev_hw_ocv = _hw_ocv;
					_prev_hw_ocv_src = _hw_ocv_src;
					_hw_ocv = _sw_ocv;
					_hw_ocv_src = FROM_SW_OCV;
					p->flag_hw_ocv_unreliable = true;
					_flag_unreliable = 1;
				}
			}
		}
	} else {
		if (_hw_ocv_75_pon_rdy == 0) {
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
		}
	}

	/* final chance to check hwocv */
	if (gm != NULL)
		if (_hw_ocv < 28000 && (gm->disableGM30 == 0)) {
			bm_err("[%s] ERROR, _hw_ocv=%d  src:%d, force use swocv\n",
			__func__, _hw_ocv, _hw_ocv_src);
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
		}

	*val = _hw_ocv;

	zcvinfo->charger_zcv = _hw_ocv_chgin;
	zcvinfo->pmic_rdy = _hw_ocv_75_pon_rdy;
	zcvinfo->pmic_zcv = _hw_ocv_75_pon;
	zcvinfo->pmic_in_zcv = _hw_ocv_75_plugin;
	zcvinfo->swocv = _sw_ocv;
	zcvinfo->zcv_from = _hw_ocv_src;
	zcvinfo->zcv_tmp = now_temp;

	if (zcvinfo->zcv_1st_read == false) {
		zcvinfo->charger_zcv_1st = zcvinfo->charger_zcv;
		zcvinfo->pmic_rdy_1st = zcvinfo->pmic_rdy;
		zcvinfo->pmic_zcv_1st = zcvinfo->pmic_zcv;
		zcvinfo->pmic_in_zcv_1st = zcvinfo->pmic_in_zcv;
		zcvinfo->swocv_1st = zcvinfo->swocv;
		zcvinfo->zcv_from_1st = zcvinfo->zcv_from;
		zcvinfo->zcv_tmp_1st = zcvinfo->zcv_tmp;
		zcvinfo->zcv_1st_read = true;
	}

	gauge_dev->fg_hw_info.pmic_zcv = _hw_ocv_75_pon;
	gauge_dev->fg_hw_info.pmic_zcv_rdy = _hw_ocv_75_pon_rdy;
	gauge_dev->fg_hw_info.charger_zcv = _hw_ocv_chgin;
	gauge_dev->fg_hw_info.hw_zcv = _hw_ocv;

	bm_err("[%s] g_fg_is_charger_exist %d _hw_ocv_chgin_rdy %d pl:%d %d\n",
		__func__, fg_is_charger_exist, _hw_ocv_chgin_rdy,
		zcvinfo->pl_charging_status, zcvinfo->moniter_plchg_bit);
	bm_err("[%s] _hw_ocv %d _sw_ocv %d now_thr %d\n",
		__func__, _prev_hw_ocv, _sw_ocv, now_thr);
	bm_err("[%s] _hw_ocv %d _hw_ocv_src %d _prev_hw_ocv %d _prev_hw_ocv_src %d _flag_unreliable %d\n",
		__func__, _hw_ocv, _hw_ocv_src, _prev_hw_ocv, _prev_hw_ocv_src, _flag_unreliable);
	bm_err("[%s] _hw_ocv_75_pon_rdy %d _hw_ocv_75_pon %d _hw_ocv_75_plugin %d _hw_ocv_chgin %d _sw_ocv %d now_temp %d now_thr %d\n",
		__func__, _hw_ocv_75_pon_rdy, _hw_ocv_75_pon,
		_hw_ocv_75_plugin, _hw_ocv_chgin, _sw_ocv, now_temp, now_thr);
	bm_err("[%s] _hw_ocv_75_before_chgin %d\n", __func__, _hw_ocv_75_before_chgin);

	return 0;
}

static int reset_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
		     int threshold)
{
	bm_err("[fgauge_hw_reset]\n");
	regmap_update_bits(gauge->regmap, RG_FGADC_CON2, FG_CHARGE_RST_MASK | FG_TIME_RST_MASK,
			   FG_CHARGE_RST_MASK | FG_TIME_RST_MASK);
	mdelay(1);
	regmap_update_bits(gauge->regmap, RG_FGADC_CON2, FG_CHARGE_RST_MASK | FG_TIME_RST_MASK, 0);

	return 0;
}

static int ptim_resist_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			   int *val)
{
	int ret;

	if (IS_ERR(gauge->chan_ptim_r)) {
		bm_err("[%s]chan error\n", __func__);
		return -ENOTSUPP;
	}

	ret = iio_read_channel_processed(gauge->chan_ptim_r, val);
	if (ret < 0)
		bm_err("[%s]read fail,ret=%d\n", __func__, ret);

	return ret;
}

static int ptim_battery_voltage_get(struct mtk_gauge *gauge,
				    struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	int ret;

	if (IS_ERR(gauge->chan_ptim_bat_voltage)) {
		bm_err("[%s]chan error\n", __func__);
		return -ENOTSUPP;
	}

	ret = iio_read_channel_processed(gauge->chan_ptim_bat_voltage, val);
	if (ret < 0)
		bm_err("[%s]read fail,ret=%d\n", __func__, ret);

	return ret;
}

static int rtc_ui_soc_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			  int *val)
{
	u8 rtc_value;
	int rtc_ui_soc = 0;

	rtc_value = get_rtc_spare_fg_value(gauge);
	rtc_ui_soc = (rtc_value & 0x7f);

	*val = rtc_ui_soc;

	if (rtc_ui_soc > 100 || rtc_ui_soc < 0)
		bm_err("[%s]ERR!rtc=0x%x,ui_soc=%d\n", rtc_value, rtc_ui_soc);
	else
		bm_debug("[%s]rtc=0x%x,ui_soc=%d\n", rtc_value, rtc_ui_soc);

	return 0;
}

static int rtc_ui_soc_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	u8 spare3_reg = get_rtc_spare_fg_value(gauge);
	int spare3_reg_valid = 0;
	int new_spare3_reg = 0;

	spare3_reg_valid = (spare3_reg & 0x80);
	new_spare3_reg = spare3_reg_valid + val;

	set_rtc_spare_fg_value(gauge, new_spare3_reg);

	bm_debug("[%s] ui_soc=%d, spare3_reg=0x%x, valid:%d, new_spare3_reg:0x%x\n",
		__func__, val, spare3_reg, spare3_reg_valid, new_spare3_reg);

	return 1;
}

static void switch_vbat2_det_time(int _prd, int *value)
{
	if (_prd >= 1 && _prd < 3)
		*value = 0;
	else if (_prd >= 3 && _prd < 5)
		*value = 1;
	else if (_prd >= 5 && _prd < 10)
		*value = 2;
	else if (_prd >= 10)
		*value = 3;
}

static void switch_vbat2_debt_counter(int _prd, int *value)
{
	if (_prd >= 1 && _prd < 2)
		*value = 0;
	else if (_prd >= 2 && _prd < 4)
		*value = 1;
	else if (_prd >= 4 && _prd < 8)
		*value = 2;
	else if (_prd >= 8)
		*value = 3;
}

static int vbat_lt_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
		       int threshold)
{
	int vbat2_l_th_mv =  threshold;
	int vbat2_l_th_reg = mv_to_reg_12_value(gauge, vbat2_l_th_mv);
	int vbat2_det_counter = 0;
	int vbat2_det_time = 0;
	u16 regval;

	switch_vbat2_det_time(gauge->hw_status.vbat2_det_time, &vbat2_det_time);
	switch_vbat2_debt_counter(gauge->hw_status.vbat2_det_counter, &vbat2_det_counter);

	regval = vbat2_l_th_reg & AUXADC_LBAT2_VOLT_MIN_MASK;
	regmap_raw_write(gauge->regmap, RG_AUXADC_LBAT2_6, &regval, sizeof(regval));

	regmap_update_bits(gauge->regmap, RG_AUXADC_LBAT2_1, AUXADC_LBAT2_DET_PRD_SEL_MASK,
			   vbat2_det_time << AUXADC_LBAT2_DET_PRD_SEL_SHIFT);

	regmap_update_bits(gauge->regmap, RG_AUXADC_LBAT2_1, AUXADC_LBAT2_DEBT_MIN_SEL_MASK,
			   vbat2_det_counter << AUXADC_LBAT2_DEBT_MIN_SEL_SHIFT);

	bm_debug("[fg_set_vbat2_l_th] thr:%d [0x%x %d 0x%x %d 0x%x]\n",
		threshold,
		vbat2_l_th_reg,
		gauge->hw_status.vbat2_det_time, vbat2_det_time,
		gauge->hw_status.vbat2_det_counter, vbat2_det_counter);

	return 0;
}

static int vbat_ht_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
		       int threshold)
{
	int vbat2_h_th_mv =  threshold;
	int vbat2_h_th_reg = mv_to_reg_12_value(gauge, vbat2_h_th_mv);
	int vbat2_det_counter = 0;
	int vbat2_det_time = 0;
	u16 regval;

	switch_vbat2_det_time(gauge->hw_status.vbat2_det_time, &vbat2_det_time);
	switch_vbat2_debt_counter(gauge->hw_status.vbat2_det_counter, &vbat2_det_counter);

	regval = vbat2_h_th_reg & AUXADC_LBAT2_VOLT_MAX_MASK;
	regmap_raw_write(gauge->regmap, RG_AUXADC_LBAT2_3, &regval, sizeof(regval));

	regmap_update_bits(gauge->regmap, RG_AUXADC_LBAT2_1, AUXADC_LBAT2_DET_PRD_SEL_MASK,
			   vbat2_det_time << AUXADC_LBAT2_DET_PRD_SEL_SHIFT);

	regmap_update_bits(gauge->regmap, RG_AUXADC_LBAT2_1, AUXADC_LBAT2_DEBT_MAX_SEL_MASK,
			   vbat2_det_counter << AUXADC_LBAT2_DEBT_MAX_SEL_SHIFT);

	bm_debug("[fg_set_vbat2_h_th] thr:%d [0x%x %d 0x%x %d 0x%x]\n",
		threshold, vbat2_h_th_reg, gauge->hw_status.vbat2_det_time, vbat2_det_time,
		gauge->hw_status.vbat2_det_counter, vbat2_det_counter);

	return 0;
}

static void enable_lbat2_en(struct mtk_gauge *gauge)
{
	if (gauge->vbat_l_en == true || gauge->vbat_h_en == true)
		regmap_update_bits(gauge->regmap, RG_AUXADC_LBAT2_0, AUXADC_LBAT2_EN_MASK,
				   AUXADC_LBAT2_EN_MASK);

	if (gauge->vbat_l_en == false && gauge->vbat_h_en == false)
		regmap_update_bits(gauge->regmap, RG_AUXADC_LBAT2_0, AUXADC_LBAT2_EN_MASK, 0);
}

static int en_l_vbat_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	static int cnt;

	bm_debug("%s %d %d\n", __func__, cnt, val);

	cnt = val ? cnt + 1 : cnt - 1;

	if (val)
		enable_gauge_irq(gauge, VBAT_L_IRQ);
	else
		disable_gauge_irq(gauge, VBAT_L_IRQ);

	regmap_update_bits(gauge->regmap, RG_AUXADC_LBAT2_5,
			   AUXADC_LBAT2_IRQ_EN_MIN_MASK | AUXADC_LBAT2_DET_MIN_MASK,
			   val ? (AUXADC_LBAT2_IRQ_EN_MIN_MASK | AUXADC_LBAT2_DET_MIN_MASK) : 0);

	gauge->vbat_l_en = !!val;
	enable_lbat2_en(gauge);

	return 0;
}

static int en_h_vbat_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	if (val)
		enable_gauge_irq(gauge, VBAT_H_IRQ);
	else
		disable_gauge_irq(gauge, VBAT_H_IRQ);

	regmap_update_bits(gauge->regmap, RG_AUXADC_LBAT2_2,
			   AUXADC_LBAT2_IRQ_EN_MAX_MASK | AUXADC_LBAT2_DET_MAX_MASK,
			   val ? (AUXADC_LBAT2_IRQ_EN_MAX_MASK | AUXADC_LBAT2_DET_MAX_MASK) : 0);

	gauge->vbat_h_en = !!val;
	enable_lbat2_en(gauge);

	return 0;
}

static int mt6375_enable_auxadc_hm(struct mt6375_priv *priv, bool en)
{
	static const u8 code[] = { 0x63, 0x63 };

	if (en)
		return regmap_bulk_write(priv->regmap, HK_TOP_WKEY, code,
					 ARRAY_SIZE(code));
	return regmap_write(priv->regmap, HK_TOP_WKEY, 0);
}

static int mt6375_enable_tm(struct mt6375_priv *priv, bool en)
{
	u8 tm_pascode[] = { 0x69, 0x96, 0x63, 0x75 };

	if (en)
		return regmap_bulk_write(priv->regmap, RG_TM_PASCODE1,
					 tm_pascode, ARRAY_SIZE(tm_pascode));
	return regmap_write(priv->regmap, RG_TM_PASCODE1, 0);
}

static int mt6375_get_vbat_mon_rpt(struct mt6375_priv *priv, int *vbat)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;
	u16 data;

	psy = devm_power_supply_get_by_phandle(priv->dev, "charger");
	if (psy) {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
		if (ret >= 0)
			*vbat = val.intval;
		power_supply_put(psy);
	} else {
		ret = regmap_update_bits(priv->regmap, RG_ADC_CONFG1,
					 VBAT_MON_EN_MASK, 0xFF);
		if (ret < 0)
			return ret;
		usleep_range(ADC_CONV_TIME_US * 2, ADC_CONV_TIME_US * 3);
		ret = regmap_bulk_read(priv->regmap, RG_VBAT_MON_RPT, &data, 2);
		if (ret < 0)
			dev_notice(priv->dev, "failed to get vbat monitor report\n");
		else
			*vbat = ADC_FROM_VBAT_RAW(be16_to_cpu(data));
		regmap_update_bits(priv->regmap, RG_ADC_CONFG1,
				   VBAT_MON_EN_MASK, 0);
	}
	return ret;
}

static int battery_voltage_cali(struct mtk_gauge *gauge,
				struct mtk_gauge_sysfs_field_info *attr,
				int *val)
{
	struct mt6375_priv *priv = container_of(gauge, struct mt6375_priv, gauge);
	int ret = 0;
	int chg_vbat, auxadc_vbat, vbat_diff, vbat_diff_sum = 0, vbat_diff_avg;
	int chg_vbat_min = INT_MAX, auxadc_vbat_min = INT_MAX;
	int chg_vbat_max = 0, auxadc_vbat_max = 0;
	int cnt = 0, max_cnt = 5;
	int value = 0;
	u16 gain_err = priv->gain_err, gain_err_diff;
	u16 data = 0;

	dev_info(priv->dev, "%s\n", __func__);
	while (abs(cnt) < max_cnt) {
		ret = instant_current(gauge, &value);
		if (ret) {
			pr_notice("%s error, ret = %d\n", __func__, ret);
			return ret;
		}
		bm_err("%s: cic1 = %d\n", __func__, value);
		if (abs(value) > 500) {
			bm_err("%s: cic1 out of range(%d)\n", __func__, value);
			return -EINVAL;
		}

		ret = mt6375_get_vbat_mon_rpt(priv, &chg_vbat);
		if (ret < 0) {
			bm_err("%s: failed to get vbat_mon_rpt\n", __func__);
			return ret;
		}

		ret = iio_read_channel_processed(gauge->chan_bat_voltage,
						 &auxadc_vbat);
		if (ret < 0) {
			bm_err("%s: failed to get auxadc_vbat(%d)\n", __func__, ret);
			return ret;
		}
		dev_info(priv->dev, "%s: chg_vbat:%d, auxadc_vbat:%d\n",
			 __func__, chg_vbat, auxadc_vbat);
		chg_vbat_min = min(chg_vbat_min, chg_vbat);
		chg_vbat_max = max(chg_vbat_max, chg_vbat);
		auxadc_vbat_min = min(auxadc_vbat_min, auxadc_vbat);
		auxadc_vbat_max = max(auxadc_vbat_max, auxadc_vbat);
		dev_info(priv->dev, "%s: chg_vbat_min:%d, chg_vbat_max:%d\n",
			 __func__, chg_vbat_min, chg_vbat_max);
		dev_info(priv->dev, "%s: auxadc_vbat_min:%d, auxadc_vbat_max:%d\n",
			 __func__, auxadc_vbat_min, auxadc_vbat_max);
		if (chg_vbat_max - chg_vbat_min > HTOL_THRESHOLD_MAX ||
		    auxadc_vbat_max - auxadc_vbat_min > HTOL_THRESHOLD_MAX) {
			bm_err("%s: vbat_diff min/max out of range\n", __func__);
			return ret;
		}

		vbat_diff = chg_vbat - auxadc_vbat;
		vbat_diff_sum += vbat_diff;

		if (abs(vbat_diff) > HTOL_THRESHOLD_MAX || abs(vbat_diff) < HTOL_THRESHOLD_MIN) {
			bm_err("%s: vbat_diff is out of range(%d), no need to calibrate\n",
				__func__, vbat_diff);
			return ret;
		}

		if (vbat_diff >= HTOL_THRESHOLD_MIN && cnt++ >= 0)
			continue;
		else if (vbat_diff <= -HTOL_THRESHOLD_MIN && cnt-- <= 0)
			continue;
		else
			return ret;
	}

	vbat_diff_avg = vbat_diff_sum / max_cnt;
	dev_info(priv->dev, "%s: vbat_diff_avg:%d, gain_err:0x%x, efuse_gain_err:0x%x\n",
		 __func__, vbat_diff_avg, gain_err, priv->efuse_gain_err);
	gain_err += vbat_diff_avg;
	gain_err_diff = abs((int)gain_err - (int)priv->efuse_gain_err);
	if (abs(gain_err_diff) > HTOL_CALI_MAX) {
		bm_err("%s: gain_err_diff out of theshold(%d), adjust HTOL_THRESHOLD_MAX\n",
			__func__, gain_err_diff);
		if (gain_err > priv->efuse_gain_err)
			gain_err = priv->efuse_gain_err + HTOL_CALI_MAX;
		else
			gain_err = priv->efuse_gain_err - HTOL_CALI_MAX;
		return ret;
	}

	ret = mt6375_enable_auxadc_hm(priv, true);
	if (ret < 0)
		return ret;
	ret = regmap_bulk_write(priv->regmap, AUXADC_EFUSE_GAIN_TRIM, &gain_err, 2);
	if (ret < 0)
		goto out;
	priv->gain_err = gain_err;
out:
	mt6375_enable_auxadc_hm(priv, false);
	ret = regmap_bulk_read(priv->regmap, AUXADC_EFUSE_GAIN_TRIM, &data, 2);
	dev_info(priv->dev, "%s: after cali, gain_err:0x%x\n", __func__, data);
	dev_info(priv->dev, "%s: done(%d)\n", __func__, ret);
	return ret;
}

static int mt6375_auxadc_init_vbat_calibration(struct mt6375_priv *priv)
{
	int ret, offset_trim;
	u16 data = 0;

	regmap_bulk_read(priv->regmap, AUXADC_EFUSE_OFFSET_TRIM, &offset_trim, 2);
	if (offset_trim >= 0x4000) {
		bm_err("%s: before handle offset trim signed, offset_trim:0x%x\n",
		       __func__, offset_trim);
		offset_trim = -(0x8000 - offset_trim);
	}
	priv->offset_trim = offset_trim;

	ret = mt6375_enable_auxadc_hm(priv, true);
	if (ret < 0)
		return ret;
	ret = regmap_bulk_read(priv->regmap, AUXADC_EFUSE_GAIN_TRIM, &data, 2);
	if (ret < 0) {
		mt6375_enable_auxadc_hm(priv, false);
		return ret;
	}
	priv->gain_err = data;
	mt6375_enable_auxadc_hm(priv, false);

	ret = mt6375_enable_tm(priv, true);
	if (ret < 0)
		return ret;
	ret = regmap_bulk_read(priv->regmap, AUXADC_EFUSE_GAIN_ERR, &data, 2);
	if (ret < 0)
		bm_err("%s: failed to get auxadc efuse trim\n", __func__);
	priv->efuse_gain_err = data;
	dev_info(priv->dev, "%s: gain_err:0x%x, efuse_gain_err:0x%x\n", __func__,
		 priv->gain_err, priv->efuse_gain_err);
	return mt6375_enable_tm(priv, false);
}

static int bif_voltage_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			   int *val)
{
	int ret;

	if (IS_ERR(gauge->chan_bif)) {
		bm_err("[%s]chan error\n", __func__);
		return -ENOTSUPP;
	}

	ret = iio_read_channel_processed(gauge->chan_bif, val);
	if (ret < 0)
		bm_err("[%s]read fail,ret=%d\n", __func__, ret);

	return ret;
}

static int battery_temperature_adc_get(struct mtk_gauge *gauge,
				       struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	int ret;

	if (IS_ERR(gauge->chan_bat_temp)) {
		bm_err("[%s]chan error\n", __func__);
		return -ENOTSUPP;
	}

	ret = iio_read_channel_processed(gauge->chan_bat_temp, val);
	if (ret < 0)
		bm_err("[%s]read fail,ret=%d\n", __func__, ret);

	return ret;
}

static int __maybe_unused auxadc_reset(struct mt6375_priv *priv)
{
	int ret;

	ret = mt6375_enable_auxadc_hm(priv, true);
	if (ret < 0)
		return ret;

	ret = regmap_write(priv->regmap, HK_TOP_RST_CON0, RESET_MASK);
	if (ret)
		goto out;

	ret = regmap_write(priv->regmap, HK_TOP_RST_CON0, 0);
	if (ret)
		goto out;
out:
	mt6375_enable_auxadc_hm(priv, false);
	return ret;
}

static int bat_vol_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	struct mt6375_priv *priv = container_of(gauge, struct mt6375_priv, gauge);
	int i, ret, vbat_mon;
	u32 data = 0;
	static long long t1;
	static int print_period = 3;
	static int dump_reg[] = { 0x236, 0x237, 0x238, 0x31C, 0x31D, 0x338, 0x339,
				  0x33A, 0x35D, 0x35E, 0x408, 0x409, 0x40A, 0x40B,
				  0x410, 0x411, 0x416, 0x417, 0x41E, 0x41F, 0x422,
				  0x423, 0x45C, 0x46E, 0x46F, 0x470, 0x471 };

	if (IS_ERR(gauge->chan_bat_voltage)) {
		bm_err("[%s]chan error\n", __func__);
		return -ENOTSUPP;
	}

	ret = iio_read_channel_processed(gauge->chan_bat_voltage, val);
	if (ret < 0) {
		bm_err("[%s]read fail,ret=%d\n", __func__, ret);
		return ret;
	}

	if (*val < 1000) {
		if (t1 == 0) {
			t1 = local_clock();
		} else if ((local_clock() - t1) / NSEC_PER_SEC > print_period) {
			t1 = local_clock();
			ret = mt6375_get_vbat_mon_rpt(priv, &vbat_mon);
			bm_err("[%s] vbat_mon = %d(%d)\n", __func__, vbat_mon, ret);
			for (i = 0; i < ARRAY_SIZE(dump_reg); i++) {
				ret = regmap_read(gauge->regmap, dump_reg[i], &data);
				bm_err("[%s] addr:0x%4x, data:0x%x(%d)\n",
					__func__, dump_reg[i], data, ret);
			}
		}
	}
	return ret;
}

static int hw_version_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			  int *val)
{
	*val = GAUGE_HW_V2000;
	return 0;
}

static int battery_exist_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			     int *val)
{
	unsigned int regval = 0;
	int ret = 0;

#if defined(CONFIG_FPGA_EARLY_PORTING)
	*val = 0;
	return 0;
#endif

	ret = regmap_read(gauge->regmap, RG_BATON_ANA_MON0, &regval);
	if (ret) {
		pr_notice("%s error, ret = %d\n", __func__, ret);
		return ret;
	}
	regval = regval & AD_BATON_UNDET_MASK;

	*val = !regval ? 1 : 0;

	if (regval) {
		ret = regmap_update_bits(gauge->regmap, RG_AUXADC_CON42,
					 AUXADC_ADC_RDY_PWRON_CLR_MASK,
					 AUXADC_ADC_RDY_PWRON_CLR_MASK);
		if (ret) {
			pr_notice("%s error, ret = %d\n", __func__, ret);
			return ret;
		}
		mdelay(1);
		ret = regmap_update_bits(gauge->regmap, RG_AUXADC_CON42,
					 AUXADC_ADC_RDY_PWRON_CLR_MASK, 0);
		if (ret) {
			pr_notice("%s error, ret = %d\n", __func__, ret);
			return ret;
		}
	}

	return 0;
}

static int coulomb_interrupt_lt_set(struct mtk_gauge *gauge,
				    struct mtk_gauge_sysfs_field_info *attr, int val)
{
	struct mt6375_priv *priv = container_of(gauge, struct mt6375_priv, gauge);
	u16 temp_car_15_0 = 0;
	u16 temp_car_31_16 = 0;
	unsigned int uvalue32_car_msb = 0;
	signed int lowbound = 0;
	u16 lowbound_31_16 = 0, lowbound_15_00 = 0;
	signed int value32_car;
	long long car = val;
	int r_fg_value;
	int car_tune_value;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->gm->fg_cust_data.car_tune_value;
	bm_debug("%s car=%d\n", __func__, val);
	if (car == 0) {
		disable_gauge_irq(gauge, COULOMB_L_IRQ);
		return 0;
	}

	pre_gauge_update(gauge);

	regmap_raw_read(gauge->regmap, RG_FGADC_CAR_CON0, &temp_car_15_0, sizeof(temp_car_15_0));
	regmap_raw_read(gauge->regmap, RG_FGADC_CAR_CON1, &temp_car_31_16, sizeof(temp_car_31_16));

	post_gauge_update(gauge);

	uvalue32_car_msb = (temp_car_31_16 & 0x8000) >> 15;
	value32_car = temp_car_31_16 << 16 | temp_car_15_0;

	bm_debug("[%s] FG_CAR = 0x%x:%d uvalue32_car_msb:0x%x 0x%x 0x%x\r\n",
		__func__,
		value32_car, value32_car, uvalue32_car_msb,
		temp_car_15_0,
		temp_car_31_16);

	/* gap to register-base */
#if defined(__LP64__) || defined(_LP64)
	car = car * 100000 / priv->unit_charge;
	/* car * 1000 * 100 */
#else
	car = div_s64(car * 100000, priv->unit_charge);
#endif

	if (r_fg_value != priv->default_r_fg && priv->default_r_fg != 0)
#if defined(__LP64__) || defined(_LP64)
		car = (car * r_fg_value) / priv->default_r_fg;
#else
		car = div_s64(car * r_fg_value, priv->default_r_fg);
#endif

#if defined(__LP64__) || defined(_LP64)
	car = ((car * 1000) / car_tune_value);
#else
	car = div_s64((car * 1000), car_tune_value);
#endif

	lowbound = value32_car;

	bm_debug("[%s]low=0x%x:%d diff_car=0x%llx:%lld\r\n",
		 __func__, lowbound, lowbound, car, car);

	lowbound = lowbound - car;

	lowbound_31_16 = (lowbound & 0xffff0000) >> 16;
	lowbound_15_00 = (lowbound & 0xffff);

	bm_debug("[%s]final low=0x%x:%d car=0x%llx:%lld\r\n",
		 __func__, lowbound, lowbound, car, car);

	bm_debug("[%s] final low 0x%x 0x%x 0x%x car=0x%llx\n",
		 __func__, lowbound, lowbound_31_16, lowbound_15_00, car);

	disable_gauge_irq(gauge, COULOMB_L_IRQ);

	regmap_raw_write(gauge->regmap, RG_FGADC_CARTH_CON0, &lowbound_15_00,
			 sizeof(lowbound_15_00));

	regmap_raw_write(gauge->regmap, RG_FGADC_CARTH_CON1, &lowbound_31_16,
			 sizeof(lowbound_31_16));

	mdelay(1);

	enable_gauge_irq(gauge, COULOMB_L_IRQ);

	bm_debug("[%s] low:0x%x 0x%x car_value:%d car:%d irq:%d\r\n",
		__func__, lowbound_15_00, lowbound_31_16, val, value32_car,
		gauge->irq_no[COULOMB_L_IRQ]);

	return 0;
}

static int coulomb_interrupt_ht_set(struct mtk_gauge *gauge,
				    struct mtk_gauge_sysfs_field_info *attr, int val)
{
	struct mt6375_priv *priv = container_of(gauge, struct mt6375_priv, gauge);
	u16 temp_car_15_0 = 0;
	u16 temp_car_31_16 = 0;
	unsigned int uvalue32_car_msb = 0;
	signed int upperbound = 0;
	u16 upperbound_31_16 = 0, upperbound_15_00 = 0;
	signed int value32_car;
	long long car = val;
	int r_fg_value;
	int car_tune_value;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->gm->fg_cust_data.car_tune_value;
	bm_debug("%s car=%d\n", __func__, val);
	if (car == 0) {
		disable_gauge_irq(gauge, COULOMB_H_IRQ);
		return 0;
	}

	pre_gauge_update(gauge);

	regmap_raw_read(gauge->regmap, RG_FGADC_CAR_CON0, &temp_car_15_0, sizeof(temp_car_15_0));
	regmap_raw_read(gauge->regmap, RG_FGADC_CAR_CON1, &temp_car_31_16, sizeof(temp_car_31_16));

	post_gauge_update(gauge);

	uvalue32_car_msb = (temp_car_31_16 & 0x8000) >> 15;
	value32_car = temp_car_31_16 << 16 | temp_car_15_0;

	bm_debug("[%s] FG_CAR = 0x%x:%d uvalue32_car_msb:0x%x 0x%x 0x%x\r\n",
		__func__, value32_car, value32_car, uvalue32_car_msb,
		temp_car_15_0,
		temp_car_31_16);

#if defined(__LP64__) || defined(_LP64)
	car = car * 100000 / priv->unit_charge;
#else
	car = div_s64(car * 100000, priv->unit_charge);
#endif

	if (r_fg_value != priv->default_r_fg && priv->default_r_fg != 0)
#if defined(__LP64__) || defined(_LP64)
		car = (car * r_fg_value) /
			priv->default_r_fg;
#else
		car = div_s64(car * r_fg_value,
			priv->default_r_fg);
#endif

#if defined(__LP64__) || defined(_LP64)
	car = ((car * 1000) / car_tune_value);
#else
	car = div_s64((car * 1000), car_tune_value);
#endif

	upperbound = value32_car;

	bm_debug("[%s] upper = 0x%x:%d diff_car=0x%llx:%lld\r\n",
		 __func__, upperbound, upperbound, car, car);

	upperbound = upperbound + car;

	upperbound_31_16 = (upperbound & 0xffff0000) >> 16;
	upperbound_15_00 = (upperbound & 0xffff);

	bm_debug("[%s] final upper = 0x%x:%d car=0x%llx:%lld\r\n",
		 __func__, upperbound, upperbound, car, car);

	bm_debug("[%s] final upper 0x%x 0x%x 0x%x car=0x%llx\n",
		 __func__,
		upperbound, upperbound_31_16, upperbound_15_00, car);

	disable_gauge_irq(gauge, COULOMB_H_IRQ);

	regmap_raw_write(gauge->regmap, RG_FGADC_CARTH_CON2, &upperbound_15_00,
			 sizeof(upperbound_15_00));

	regmap_raw_write(gauge->regmap, RG_FGADC_CARTH_CON3, &upperbound_31_16,
			 sizeof(upperbound_31_16));

	mdelay(1);

	enable_gauge_irq(gauge, COULOMB_H_IRQ);

	bm_debug("[%s] high:0x%x 0x%x car_value:%d car:%d irq:%d\r\n",
		__func__, upperbound_15_00, upperbound_31_16, val, value32_car,
		gauge->irq_no[COULOMB_H_IRQ]);

	return 0;
}

static int battery_current_get(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr,
			       int *val)
{
	int ret = 0;

	ret = instant_current(gauge, val);
	if (ret) {
		pr_notice("%s error, ret = %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int initial_set(struct mtk_gauge *gauge, struct mtk_gauge_sysfs_field_info *attr, int val)
{
	int bat_flag = 0;
	int is_charger_exist;
	u16 rev_val = 0;

	regmap_update_bits(gauge->regmap, RG_AUXADC_NAG_0, AUXADC_NAG_PRD_MASK,
			   2 << AUXADC_NAG_PRD_SHIFT);

	fgauge_get_info(gauge, GAUGE_PROP_BAT_PLUG_STATUS, &bat_flag);
	fgauge_get_info(gauge, GAUGE_PROP_PL_CHARGING_STATUS, &is_charger_exist);

	regmap_raw_read(gauge->regmap, RG_SYSTEM_INFO_CON0, &rev_val, sizeof(rev_val));
	bm_err("bat_plug:%d chr:%d info:0x%x\n", bat_flag, is_charger_exist, rev_val);

	gauge->hw_status.pl_charger_status = is_charger_exist;

	if (is_charger_exist == 1) {
		gauge->hw_status.is_bat_plugout = 1;
		fgauge_set_info(gauge, GAUGE_PROP_2SEC_REBOOT, 0);
	} else
		gauge->hw_status.is_bat_plugout = bat_flag ? 0 : 1;

	fgauge_set_info(gauge, GAUGE_PROP_BAT_PLUG_STATUS, 1);
	/*[12:8], 5 bits*/
	gauge->hw_status.bat_plug_out_time = 31;

	fgauge_read_RTC_boot_status(gauge);

	return 1;
}

static ssize_t gauge_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct mtk_gauge *gauge;
	struct mtk_gauge_sysfs_field_info *gauge_attr;
	int val;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);

	gauge_attr = container_of(attr, struct mtk_gauge_sysfs_field_info, attr);
	if (gauge_attr->set != NULL) {
		mutex_lock(&gauge->ops_lock);
		gauge_attr->set(gauge, gauge_attr, val);
		mutex_unlock(&gauge->ops_lock);
	}

	return count;
}

static ssize_t gauge_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct mtk_gauge *gauge;
	struct mtk_gauge_sysfs_field_info *gauge_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);

	gauge_attr = container_of(attr, struct mtk_gauge_sysfs_field_info, attr);
	if (gauge_attr->get != NULL) {
		mutex_lock(&gauge->ops_lock);
		gauge_attr->get(gauge, gauge_attr, &val);
		mutex_unlock(&gauge->ops_lock);
	}

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

/* Must be in the same order as GAUGE_PROP_* */
static struct mtk_gauge_sysfs_field_info mt6375_sysfs_field_tbl[] = {
	GAUGE_SYSFS_FIELD_WO(initial_set, GAUGE_PROP_INITIAL),
	GAUGE_SYSFS_FIELD_RO(battery_current_get, GAUGE_PROP_BATTERY_CURRENT),
	GAUGE_SYSFS_FIELD_RO(coulomb_get, GAUGE_PROP_COULOMB),
	GAUGE_SYSFS_FIELD_WO(coulomb_interrupt_ht_set, GAUGE_PROP_COULOMB_HT_INTERRUPT),
	GAUGE_SYSFS_FIELD_WO(coulomb_interrupt_lt_set, GAUGE_PROP_COULOMB_LT_INTERRUPT),
	GAUGE_SYSFS_FIELD_RO(battery_exist_get, GAUGE_PROP_BATTERY_EXIST),
	GAUGE_SYSFS_FIELD_RO(hw_version_get, GAUGE_PROP_HW_VERSION),
	GAUGE_SYSFS_FIELD_RO(bat_vol_get, GAUGE_PROP_BATTERY_VOLTAGE),
	GAUGE_SYSFS_FIELD_RO(battery_temperature_adc_get, GAUGE_PROP_BATTERY_TEMPERATURE_ADC),
	GAUGE_SYSFS_FIELD_RO(bif_voltage_get, GAUGE_PROP_BIF_VOLTAGE),
	GAUGE_SYSFS_FIELD_WO(en_h_vbat_set, GAUGE_PROP_EN_HIGH_VBAT_INTERRUPT),
	GAUGE_SYSFS_FIELD_WO(en_l_vbat_set, GAUGE_PROP_EN_LOW_VBAT_INTERRUPT),
	GAUGE_SYSFS_FIELD_WO(vbat_ht_set, GAUGE_PROP_VBAT_HT_INTR_THRESHOLD),
	GAUGE_SYSFS_FIELD_WO(vbat_lt_set, GAUGE_PROP_VBAT_LT_INTR_THRESHOLD),
	GAUGE_SYSFS_FIELD_RW(rtc_ui_soc, rtc_ui_soc_set, rtc_ui_soc_get, GAUGE_PROP_RTC_UI_SOC),
	GAUGE_SYSFS_FIELD_RO(ptim_battery_voltage_get, GAUGE_PROP_PTIM_BATTERY_VOLTAGE),
	GAUGE_SYSFS_FIELD_RO(ptim_resist_get, GAUGE_PROP_PTIM_RESIST),
	GAUGE_SYSFS_FIELD_WO(reset_set, GAUGE_PROP_RESET),
	GAUGE_SYSFS_FIELD_RO(boot_zcv_get, GAUGE_PROP_BOOT_ZCV),
	GAUGE_SYSFS_FIELD_RO(zcv_get, GAUGE_PROP_ZCV),
	GAUGE_SYSFS_FIELD_RO(zcv_current_get, GAUGE_PROP_ZCV_CURRENT),
	GAUGE_SYSFS_FIELD_RO(nafg_cnt_get, GAUGE_PROP_NAFG_CNT),
	GAUGE_SYSFS_FIELD_RO(nafg_dltv_get, GAUGE_PROP_NAFG_DLTV),
	GAUGE_SYSFS_FIELD_RW(nafg_c_dltv, nafg_c_dltv_set, nafg_c_dltv_get, GAUGE_PROP_NAFG_C_DLTV),
	GAUGE_SYSFS_FIELD_WO(nafg_en_set, GAUGE_PROP_NAFG_EN),
	GAUGE_SYSFS_FIELD_WO(nafg_zcv_set, GAUGE_PROP_NAFG_ZCV),
	GAUGE_SYSFS_FIELD_RO(nafg_vbat_get, GAUGE_PROP_NAFG_VBAT),
	GAUGE_SYSFS_FIELD_WO(reset_fg_rtc_set, GAUGE_PROP_RESET_FG_RTC),
	GAUGE_SYSFS_FIELD_RW(gauge_initialized, gauge_initialized_set, gauge_initialized_get,
			     GAUGE_PROP_GAUGE_INITIALIZED),
	GAUGE_SYSFS_FIELD_RO(average_current_get, GAUGE_PROP_AVERAGE_CURRENT),
	GAUGE_SYSFS_FIELD_WO(bat_plugout_en_set, GAUGE_PROP_BAT_PLUGOUT_EN),
	GAUGE_SYSFS_FIELD_WO(zcv_intr_threshold_set, GAUGE_PROP_ZCV_INTR_THRESHOLD),
	GAUGE_SYSFS_FIELD_WO(zcv_intr_en_set, GAUGE_PROP_ZCV_INTR_EN),
	GAUGE_SYSFS_FIELD_WO(soff_reset_set, GAUGE_PROP_SOFF_RESET),
	GAUGE_SYSFS_FIELD_WO(ncar_reset_set, GAUGE_PROP_NCAR_RESET),
	GAUGE_SYSFS_FIELD_WO(bat_cycle_intr_threshold_set, GAUGE_PROP_BAT_CYCLE_INTR_THRESHOLD),
	GAUGE_SYSFS_FIELD_WO(hw_info_set, GAUGE_PROP_HW_INFO),
	GAUGE_SYSFS_FIELD_WO(event_set, GAUGE_PROP_EVENT),
	GAUGE_SYSFS_FIELD_WO(en_bat_tmp_ht_set, GAUGE_PROP_EN_BAT_TMP_HT),
	GAUGE_SYSFS_FIELD_WO(en_bat_tmp_lt_set, GAUGE_PROP_EN_BAT_TMP_LT),
	GAUGE_SYSFS_FIELD_WO(bat_tmp_ht_threshold_set, GAUGE_PROP_BAT_TMP_HT_THRESHOLD),
	GAUGE_SYSFS_FIELD_WO(bat_tmp_lt_threshold_set, GAUGE_PROP_BAT_TMP_LT_THRESHOLD),
	GAUGE_SYSFS_INFO_FIELD_RW(info_2sec_reboot, GAUGE_PROP_2SEC_REBOOT),
	GAUGE_SYSFS_INFO_FIELD_RW(info_pl_charging_status, GAUGE_PROP_PL_CHARGING_STATUS),
	GAUGE_SYSFS_INFO_FIELD_RW(info_monitor_plchg_status, GAUGE_PROP_MONITER_PLCHG_STATUS),
	GAUGE_SYSFS_INFO_FIELD_RW(info_bat_plug_status, GAUGE_PROP_BAT_PLUG_STATUS),
	GAUGE_SYSFS_INFO_FIELD_RW(info_is_nvram_fail_mode, GAUGE_PROP_IS_NVRAM_FAIL_MODE),
	GAUGE_SYSFS_INFO_FIELD_RW(info_monitor_soff_validtime, GAUGE_PROP_MONITOR_SOFF_VALIDTIME),
	GAUGE_SYSFS_INFO_FIELD_RW(info_con0_soc, GAUGE_PROP_CON0_SOC),
	GAUGE_SYSFS_INFO_FIELD_RW(info_shutdown_car, GAUGE_PROP_SHUTDOWN_CAR),
	GAUGE_SYSFS_INFO_FIELD_RW(car_tune_value, GAUGE_PROP_CAR_TUNE_VALUE),
	GAUGE_SYSFS_INFO_FIELD_RW(r_fg_value, GAUGE_PROP_R_FG_VALUE),
	GAUGE_SYSFS_INFO_FIELD_RW(vbat2_detect_time, GAUGE_PROP_VBAT2_DETECT_TIME),
	GAUGE_SYSFS_INFO_FIELD_RW(vbat2_detect_counter, GAUGE_PROP_VBAT2_DETECT_COUNTER),
	GAUGE_SYSFS_FIELD_WO(bat_temp_froze_en_set, GAUGE_PROP_BAT_TEMP_FROZE_EN),
	GAUGE_SYSFS_FIELD_RO(battery_voltage_cali, GAUGE_PROP_BAT_EOC)
};

static struct attribute *mt6375_sysfs_attrs[GAUGE_PROP_MAX + 1];

static const struct attribute_group mt6375_sysfs_attr_group = {
	.attrs = mt6375_sysfs_attrs,
};

static void mt6375_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(mt6375_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		mt6375_sysfs_attrs[i] = &mt6375_sysfs_field_tbl[i].attr.attr;

	mt6375_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static int mt6375_sysfs_create_group(struct mtk_gauge *gauge)
{
	mt6375_sysfs_init_attrs();

	return sysfs_create_group(&gauge->psy->dev.kobj, &mt6375_sysfs_attr_group);
}

signed int battery_meter_meta_tool_cali_car_tune(struct mtk_battery *gm,
	int meta_current)
{
	int cali_car_tune = 0;

	if (meta_current == 0)
		return gm->fg_cust_data.car_tune_value * 10;

	gm->gauge->hw_status.meta_current = meta_current;
	bm_err("%s meta_current=%d\n", __func__, meta_current);

	calculate_car_tune(gm->gauge);
	cali_car_tune = gm->gauge->hw_status.tmp_car_tune;

	bm_err("%s cali_car_tune=%d\n", __func__, cali_car_tune);

	return cali_car_tune;		/* 1000 base */
}

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_adc_cali_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int adc_out_datas[2] = { 1, 1 };

	bm_notice("%s 32bit IOCTL, cmd=0x%08x\n",
		__func__, cmd);
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		bm_err("%s file has no f_op or no f_op->unlocked_ioctl.\n",
			__func__);
		return -ENOTTY;
	}

	if (sizeof(arg) != sizeof(adc_out_datas))
		return -EFAULT;

	switch (cmd) {
	case Get_META_BAT_VOL:
	case Get_META_BAT_SOC:
	case Get_META_BAT_CAR_TUNE_VALUE:
	case Set_META_BAT_CAR_TUNE_VALUE:
	case Set_BAT_DISABLE_NAFG:
	case Set_CARTUNE_TO_KERNEL: {
		bm_notice(
			"%s send to unlocked_ioctl cmd=0x%08x\n",
			__func__,
			cmd);
		return filp->f_op->unlocked_ioctl(
			filp, cmd,
			(unsigned long)compat_ptr(arg));
	}
		break;
	default:
		bm_err("%s unknown IOCTL: 0x%08x, %d\n",
			__func__, cmd, adc_out_datas[0]);
		break;
	}

	return 0;
}
#endif

static long adc_cali_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int *user_data_addr;
	int ret = 0;
	int adc_in_data[2] = { 1, 1 };
	int adc_out_data[2] = { 1, 1 };
	int temp_car_tune;
	int isdisNAFG = 0;
	struct mtk_battery *gm;

	bm_notice("%s enter\n", __func__);

	gm = get_mtk_battery();
	mutex_lock(&gm->gauge->fg_mutex);
	user_data_addr = (int *)arg;
	ret = copy_from_user(adc_in_data, user_data_addr, sizeof(adc_in_data));
	if (adc_in_data[1] < 0) {
		bm_err("%s unknown data: %d\n", __func__, adc_in_data[1]);
		mutex_unlock(&gm->gauge->fg_mutex);
		return -EFAULT;
	}

	switch (cmd) {
		/* add for meta tool------------------------------- */

	case Get_META_BAT_VOL:
		adc_out_data[0] =
			gauge_get_int_property(GAUGE_PROP_BATTERY_VOLTAGE);
		if (copy_to_user(user_data_addr, adc_out_data,
			sizeof(adc_out_data))) {
			mutex_unlock(&gm->gauge->fg_mutex);
			return -EFAULT;
		}

		bm_notice("**** unlocked_ioctl :Get_META_BAT_VOL Done!\n");
		break;
	case Get_META_BAT_SOC:
		adc_out_data[0] = gm->ui_soc;

		if (copy_to_user(user_data_addr, adc_out_data,
			sizeof(adc_out_data))) {
			mutex_unlock(&gm->gauge->fg_mutex);
			return -EFAULT;
		}

		bm_notice("**** unlocked_ioctl :Get_META_BAT_SOC Done!\n");
		break;

	case Get_META_BAT_CAR_TUNE_VALUE:
		adc_out_data[0] = gm->fg_cust_data.car_tune_value;
		bm_err("Get_BAT_CAR_TUNE_VALUE, res=%d\n", adc_out_data[0]);

		if (copy_to_user(user_data_addr, adc_out_data,
			sizeof(adc_out_data))) {
			mutex_unlock(&gm->gauge->fg_mutex);
			return -EFAULT;
		}

		bm_notice("**** unlocked_ioctl :Get_META_BAT_CAR_TUNE_VALUE Done!\n");
		break;
	case Set_META_BAT_CAR_TUNE_VALUE:
		/* meta tool input: adc_in_data[1] (mA)*/
		/* Send cali_current to hal to calculate car_tune_value*/
		temp_car_tune =
			battery_meter_meta_tool_cali_car_tune(gm, adc_in_data[1]);

		/* return car_tune_value to meta tool in adc_out_data[0] */
		if (temp_car_tune >= 900 && temp_car_tune <= 1100)
			gm->fg_cust_data.car_tune_value = temp_car_tune;
		else
			bm_err("car_tune_value invalid:%d\n",
			temp_car_tune);

		adc_out_data[0] = temp_car_tune;

		if (copy_to_user(user_data_addr, adc_out_data,
			sizeof(adc_out_data))) {
			mutex_unlock(&gm->gauge->fg_mutex);
			return -EFAULT;
		}

		bm_err("**** unlocked_ioctl Set_BAT_CAR_TUNE_VALUE[%d], tmp_car_tune=%d result=%d, ret=%d\n",
			adc_in_data[1], adc_out_data[0], temp_car_tune,
			ret);

		break;

	case Set_BAT_DISABLE_NAFG:
		isdisNAFG = adc_in_data[1];

		if (isdisNAFG == 1) {
			gm->cmd_disable_nafg = true;
			wakeup_fg_algo_cmd(
				gm,
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_DISABLE_NAFG, 1);
		} else if (isdisNAFG == 0) {
			gm->cmd_disable_nafg = false;
			wakeup_fg_algo_cmd(
				gm,
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_DISABLE_NAFG, 0);
		}
		bm_debug("unlocked_ioctl Set_BAT_DISABLE_NAFG,isdisNAFG=%d [%d]\n",
			isdisNAFG, adc_in_data[1]);
		break;

		/* add bing meta tool------------------------------- */
	case Set_CARTUNE_TO_KERNEL:
		temp_car_tune = adc_in_data[1];
		if (temp_car_tune > 500 && temp_car_tune < 1500)
			gm->fg_cust_data.car_tune_value = temp_car_tune;

		bm_err("**** unlocked_ioctl Set_CARTUNE_TO_KERNEL[%d,%d], ret=%d\n",
			adc_in_data[0], adc_in_data[1], ret);
		break;
	default:
		bm_err("**** unlocked_ioctl unknown IOCTL: 0x%08x\n", cmd);
		mutex_unlock(&gm->gauge->fg_mutex);
		return -EINVAL;
	}

	mutex_unlock(&gm->gauge->fg_mutex);

	return 0;
}

static int adc_cali_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int adc_cali_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations adc_cali_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = adc_cali_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_adc_cali_ioctl,
#endif
	.open = adc_cali_open,
	.release = adc_cali_release,
};

static int adc_cali_cdev_init(struct platform_device *pdev)
{
	int ret = 0;
	struct class_device *class_dev = NULL;
	struct mtk_battery *gm;

	gm = get_mtk_battery();
	if (gm != NULL)
		mutex_init(&gm->gauge->fg_mutex);

	ret = alloc_chrdev_region(&bat_cali_devno, 0, 1, BAT_CALI_DEVNAME);
	if (ret)
		bm_err("Error: Can't Get Major number for adc_cali\n");

	bat_cali_cdev = cdev_alloc();
	bat_cali_cdev->owner = THIS_MODULE;
	bat_cali_cdev->ops = &adc_cali_fops;
	ret = cdev_add(bat_cali_cdev, bat_cali_devno, 1);
	if (ret)
		bm_err("adc_cali Error: cdev_add\n");

	bat_cali_major = MAJOR(bat_cali_devno);
	bat_cali_class = class_create(THIS_MODULE, BAT_CALI_DEVNAME);
	class_dev = (struct class_device *)device_create(bat_cali_class,
		NULL,
		bat_cali_devno,
		NULL, BAT_CALI_DEVNAME);

	return 0;
}

static void mtk_gauge_netlink_handler(struct sk_buff *skb)
{
	mtk_battery_netlink_handler(skb);
}

static int bat_create_netlink(struct platform_device *pdev)
{
	struct mt6375_priv *priv = platform_get_drvdata(pdev);
	struct mtk_gauge *gauge = &priv->gauge;
	struct netlink_kernel_cfg cfg = {
		.input = mtk_gauge_netlink_handler,
	};

	gauge->gm->mtk_battery_sk =
		netlink_kernel_create(&init_net, NETLINK_FGD, &cfg);

	if (gauge->gm->mtk_battery_sk == NULL) {
		bm_err("netlink_kernel_create error\n");
		return -EIO;
	}

	bm_err("[%s]netlink_kernel_create protol= %d\n",
		__func__, NETLINK_FGD);

	return 0;
}

static enum power_supply_property gauge_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_ENERGY_EMPTY,
	POWER_SUPPLY_PROP_ENERGY_EMPTY_DESIGN,
};

static int get_ptim_current(struct mtk_gauge *gauge)
{
	struct mt6375_priv *priv = container_of(gauge, struct mt6375_priv, gauge);
	u16 reg_value = 0;
	int dvalue;
	int r_fg_value;
	int car_tune_value;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->gm->fg_cust_data.car_tune_value;
	regmap_raw_read(gauge->regmap, RG_FGADC_R_CON0, &reg_value, sizeof(reg_value));

	dvalue = reg_to_current(gauge, reg_value);

	/* Auto adjust value */
	if (r_fg_value != priv->default_r_fg && r_fg_value != 0)
		dvalue = (dvalue * priv->default_r_fg) / r_fg_value;

	dvalue = ((dvalue * car_tune_value) / 1000);

	/* ptim current >0 means discharge, different to bat_current */
	dvalue = dvalue * -1;
	bm_debug("[%s]ptim current:%d\n", __func__, dvalue);

	return dvalue;
}

static int psy_gauge_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_gauge *gauge = power_supply_get_drvdata(psy);
	struct mtk_battery *gm;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		/* store disableGM30 status to mtk-gauge psy for DLPT */
		if (gauge == NULL || gauge->gm == NULL)
			val->intval = 0;
		else
			val->intval = gauge->gm->disableGM30;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if (gauge == NULL || gauge->gm == NULL)
			val->intval = 0;
		else
			val->intval = gauge->gm->disableGM30;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_ptim_current(gauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		gm = gauge->gm;
		if (gm != NULL)
			val->intval = gm->sdc.shutdown_status.is_dlpt_shutdown;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int psy_gauge_set_property(struct power_supply *psy,
			enum power_supply_property psp,
			const union power_supply_propval *val)
{
	struct mtk_gauge *gauge = power_supply_get_drvdata(psy);
	struct mtk_battery *gm;
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_notice("%s: %d %d\n", __func__, psp, val->intval);
		break;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		gm = gauge->gm;
		if (gm != NULL && val->intval == 1)
			set_shutdown_cond(gm, DLPT_SHUTDOWN);
		break;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY_DESIGN:
		gm = gauge->gm;
		if (gm != NULL && val->intval != 0) {
			gm->imix = val->intval;
			if (gm->imix > 5500) {
				gm->imix = 5500;
				pr_notice("imix check limit 5500:%d\n",
					val->intval);
			}
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;

}

static int mtk_gauge_proprietary_init(struct mt6375_priv *priv)
{
	struct mtk_gauge *gauge = &priv->gauge;
	const int r_fg_val[] = { 50, 20, 10, 5 };
	unsigned int regval;
	int ret;

	/* Get default r_fg gain error selection, must be set in LK */
	ret = regmap_read(priv->regmap, RG_FGADC_ANA_ELR4, &regval);
	if (ret)
		return ret;
	regval &= FG_GAINERR_SEL_MASK;
	priv->default_r_fg = r_fg_val[regval];

	/* Variable initialization */
	gauge->regmap = priv->regmap;
	gauge->pdev = to_platform_device(priv->dev);
	mutex_init(&gauge->ops_lock);
	gauge->hw_status.car_tune_value = 1000;
	gauge->attr = mt6375_sysfs_field_tbl;

	if (battery_psy_init(gauge->pdev))
		return -ENOMEM;

	gauge->psy_desc.name = "mtk-gauge";
	gauge->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	gauge->psy_desc.properties = gauge_properties;
	gauge->psy_desc.num_properties = ARRAY_SIZE(gauge_properties);
	gauge->psy_desc.get_property = psy_gauge_get_property;
	gauge->psy_desc.set_property = psy_gauge_set_property;
	gauge->psy_cfg.drv_data = gauge;
	gauge->psy = power_supply_register(NULL, &gauge->psy_desc, &gauge->psy_cfg);
	if (IS_ERR(gauge->psy))
		return PTR_ERR(gauge->psy);

	mt6375_sysfs_create_group(gauge);
	bat_create_netlink(gauge->pdev);
	initial_set(gauge, 0, 0);
	battery_init(gauge->pdev);
	adc_cali_cdev_init(gauge->pdev);

	return 0;
}

static void mt6375_gauge_refactor_unit(struct mt6375_priv *priv)
{
	struct fuel_gauge_custom_data fg_cust_data = priv->gauge.gm->fg_cust_data;

	priv->unit_fgcurrent = UNIT_FGCURRENT;
	priv->unit_charge = UNIT_CHARGE;
	priv->unit_fg_iavg = UNIT_FG_IAVG;
	priv->unit_fgcar_zcv = UNIT_FGCAR_ZCV;

	if (priv->gauge.hw_status.r_fg_value == 20)
		priv->default_r_fg = 20;

	if (fg_cust_data.curr_measure_20a) {
		priv->default_r_fg = 10;
		priv->unit_fgcurrent *= fg_cust_data.unit_multiple;
		priv->unit_charge *= fg_cust_data.unit_multiple;
		priv->unit_fg_iavg *= fg_cust_data.unit_multiple;
		priv->unit_fgcar_zcv *= fg_cust_data.unit_multiple;
	}

	pr_notice("%s:20A:%d,r_fg:%d,unit_fg_current:%d,unit_charge:%d,unit_fg_iavg:%d,unit_fgcar_zcv:%d\n",
		  __func__,
		  priv->gauge.gm->fg_cust_data.curr_measure_20a,
		  priv->default_r_fg,
		  priv->unit_fgcurrent,
		  priv->unit_charge,
		  priv->unit_fg_iavg,
		  priv->unit_fgcar_zcv);
}

static int mt6375_gauge_probe(struct platform_device *pdev)
{
	struct mt6375_priv *priv;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;
	mutex_init(&priv->irq_lock);
	platform_set_drvdata(pdev, priv);

	priv->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!priv->regmap) {
		dev_err(&pdev->dev, "Failed to get regmap\n");
		return -ENODEV;
	}

	priv->irq = platform_get_irq(pdev, 0);
	if (priv->irq < 0) {
		dev_err(&pdev->dev, "Failed to get gm30 irq\n");
		return priv->irq;
	}

	ret = gauge_add_irq_chip(priv);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add irq chip\n");
		return ret;
	}

	ret = gauge_get_all_auxadc_channels(priv);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get all auxadc\n");
		goto out_irq_chip;
	}

	ret = mt6375_auxadc_init_vbat_calibration(priv);
	if (ret) {
		dev_notice(&pdev->dev, "Failed to init vbat calibration\n");
		goto out_irq_chip;
	}

	ret = gauge_get_all_interrupts(priv);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get all interrupts\n");
		goto out_irq_chip;
	}

	ret = mtk_gauge_proprietary_init(priv);
	if (ret) {
		dev_err(&pdev->dev, "Failed to do mtk gauge init\n");
		goto out_irq_chip;
	}

	mt6375_gauge_refactor_unit(priv);
	return 0;

out_irq_chip:
	gauge_del_irq_chip(priv);
	return ret;
}

static int mt6375_gauge_remove(struct platform_device *pdev)
{
	struct mt6375_priv *priv = platform_get_drvdata(pdev);

	gauge_del_irq_chip(priv);
	return 0;
}

static void mt6375_gauge_shutdown(struct platform_device *pdev)
{
	struct mt6375_priv *priv = platform_get_drvdata(pdev);
	struct mtk_battery *gm = priv->gauge.gm;

	if (gm->shutdown)
		gm->shutdown(gm);
}

static int __maybe_unused mt6375_gauge_suspend(struct device *dev)
{
	struct mt6375_priv *priv = dev_get_drvdata(dev);
	struct mtk_battery *gm = priv->gauge.gm;
	pm_message_t state = { .event = 0, };

	if (gm->suspend)
		gm->suspend(gm, state);

	return 0;
}

static int __maybe_unused mt6375_gauge_resume(struct device *dev)
{
	struct mt6375_priv *priv = dev_get_drvdata(dev);
	struct mtk_battery *gm = priv->gauge.gm;

	if (gm->resume)
		gm->resume(gm);

	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6375_gauge_pm_ops, mt6375_gauge_suspend, mt6375_gauge_resume);

static const struct of_device_id __maybe_unused mt6375_gauge_of_match[] = {
	{ .compatible = "mediatek,mt6375-gauge", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6375_gauge_of_match);

static struct platform_driver mt6375_gauge_driver = {
	.probe = mt6375_gauge_probe,
	.remove = mt6375_gauge_remove,
	.shutdown = mt6375_gauge_shutdown,
	.driver = {
		.name = "mt6375-gauge",
		.pm = &mt6375_gauge_pm_ops,
		.of_match_table = mt6375_gauge_of_match,
	},
};
module_platform_driver(mt6375_gauge_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6375 Gauge Driver");
MODULE_LICENSE("GPL v2");
