// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#include <dt-bindings/iio/adc/mediatek,mt6375_auxadc.h>
#include <linux/alarmtimer.h>
#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#include <linux/iio/iio.h>

#define FGADC_R_CON0		0x2E5
#define SYSTEM_INFO_CON2_H	0x2FE
#define HK_TOP_RST_CON0		0x30F
#define HK_TOP_INT_CON0_SET	0x311
#define HK_TOP_INT_CON0_CLR	0x312
#define HK_TOP_INT_CON1_SET	0x314
#define HK_TOP_INT_CON1_CLR	0x315
#define HK_TOP_INT_MASK_CON0	0x316
#define HK_TOP_INT_MASK_CON0_SET	0x317
#define HK_TOP_INT_MASK_CON0_CLR	0x318
#define HK_TOP_INT_MASK_CON1	0x319
#define HK_TOP_INT_STATUS0	0x31C
#define HK_TOP_INT_RAW_STATUS1	0x31F
#define HK_TOP_WKEY		0x328
#define AUXADC_OUT_CH3		0x408
#define AUXADC_OUT_CH11		0x40A
#define AUXADC_OUT_CH0		0x410
#define AUXADC_OUT_IMP_AVG	0x41C
#define AUXADC_RQST0		0x438
#define AUXADC_IMP0		0x4A8
#define AUXADC_IMP1		0x4A9
#define RG_AUXADC_LBAT0		0x4AD
#define RG_AUXADC_LBAT2_0	0x4B9
#define RG_AUXADC_NAG_0		0x4D2

#define VBAT0_FLAG		BIT(0)
#define RG_RESET_MASK		BIT(1)
#define VREF_ENMASK		BIT(4)
#define BATON_ENMASK		BIT(3)
#define BATSNS_ENMASK		BIT(0)
#define ADC_OUT_RDY		BIT(7)
#define AUXADC_IMP_ENMASK	BIT(0)
#define AUXADC_IMP_PRDSEL_MASK	GENMASK(1, 0)
#define AUXADC_IMP_CNTSEL_MASK	GENMASK(3, 2)
#define AUXADC_IMP_CNTSEL_SHFT	2
#define INT_RAW_AUXADC_IMP	BIT(0)
#define NUM_IRQ_REG		2

#define AUXADC_LBAT_EN_MASK	BIT(0)
#define AUXADC_LBAT2_EN_MASK	BIT(0)
#define AUXADC_NAG_IRQ_EN_MASK	BIT(5)
#define AUXADC_NAG_EN_MASK	BIT(0)

struct mt6375_priv {
	struct device *dev;
	struct regmap *regmap;
	struct irq_domain *domain;
	struct irq_chip irq_chip;
	struct mutex adc_lock;
	struct mutex irq_lock;
	struct regulator *isink_load;
	int imix_r;
	int irq;
	u8 unmask_buf[NUM_IRQ_REG];
	int pre_uisoc;
	struct alarm vbat0_alarm;
	struct work_struct vbat0_work;
	atomic_t vbat0_flag;
	struct wakeup_source *vbat0_ws;
	struct lock_class_key info_exist_key;
};

#define VBAT0_POLL_TIME_SEC	5
#define ALARM_COUNT_MAX		12
static const int vbat_event[] = { RG_INT_STATUS_BAT_H, RG_INT_STATUS_BAT_L,
				  RG_INT_STATUS_BAT2_H, RG_INT_STATUS_BAT2_L,
				  RG_INT_STATUS_NAG_C_DLTV };

static const struct {
	u16 addr;
	u8 mask;
} vbat_event_regs[] = {
	{ RG_AUXADC_LBAT0, AUXADC_LBAT_EN_MASK },
	{ RG_AUXADC_LBAT0, AUXADC_LBAT_EN_MASK },
	{ RG_AUXADC_LBAT2_0, AUXADC_LBAT2_EN_MASK },
	{ RG_AUXADC_LBAT2_0, AUXADC_LBAT2_EN_MASK },
	{ RG_AUXADC_NAG_0, AUXADC_NAG_IRQ_EN_MASK | AUXADC_NAG_EN_MASK },
};

#define AUXADC_CHAN(_idx, _resolution, _type, _info) {		\
	.type = _type,						\
	.channel = MT6375_AUXADC_##_idx,			\
	.scan_index = MT6375_AUXADC_##_idx,			\
	.datasheet_name = #_idx,				\
	.scan_type =  {						\
		.sign = 'u',					\
		.realbits = _resolution,			\
		.storagebits = 16,				\
		.endianness = IIO_CPU,				\
	},							\
	.indexed = 1,						\
	.info_mask_separate = _info				\
}

#define AUXADC_CHAN_PROCESSED(_idx, _resolution, _type)		\
	AUXADC_CHAN(_idx, _resolution, _type, BIT(IIO_CHAN_INFO_PROCESSED))

#define AUXADC_CHAN_RAW_SCALE(_idx, _resolution, _type)		\
	AUXADC_CHAN(_idx, _resolution, _type,			\
		    BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE))

static const struct iio_chan_spec auxadc_channels[] = {
	AUXADC_CHAN_RAW_SCALE(BATSNS, 15, IIO_VOLTAGE),
	AUXADC_CHAN_RAW_SCALE(BATON, 12, IIO_VOLTAGE),
	AUXADC_CHAN_PROCESSED(IMP, 15, IIO_VOLTAGE),
	AUXADC_CHAN_RAW_SCALE(IMIX_R, 16, IIO_RESISTANCE),
	AUXADC_CHAN_RAW_SCALE(VREF, 12, IIO_VOLTAGE),
	AUXADC_CHAN_RAW_SCALE(BATSNS_DBG, 15, IIO_VOLTAGE)
};

static int auxadc_get_chg_vbat(struct mt6375_priv *priv, int *chg_vbat)
{
	static struct iio_channel *chg_vbat_chan;
	int ret = 0, vbat;

	if (IS_ERR_OR_NULL(chg_vbat_chan))
		chg_vbat_chan = devm_iio_channel_get(priv->dev, "chg_vbat");
	if (IS_ERR(chg_vbat_chan))
		return PTR_ERR(chg_vbat_chan);

	ret = iio_read_channel_processed(chg_vbat_chan, &vbat);
	if (ret < 0)
		return ret;
	*chg_vbat = vbat / 1000;
	return ret;
}

static int auxadc_read_channel(struct mt6375_priv *priv, int chan, int dbits, int *val)
{
	unsigned int enable, out_reg, rdy_val;
	u16 raw_val;
	int ret, chg_vbat = 0;

	if (chan == MT6375_AUXADC_VREF) {
		enable = VREF_ENMASK;
		out_reg = AUXADC_OUT_CH11;
	} else if (chan == MT6375_AUXADC_BATON) {
		enable = BATON_ENMASK;
		out_reg = AUXADC_OUT_CH3;
	} else if (chan == MT6375_AUXADC_BATSNS) {
		if (atomic_read(&priv->vbat0_flag)) {
			ret = auxadc_get_chg_vbat(priv, &chg_vbat);
			dev_info(priv->dev, "%s: use chg_vbat:%d(%d)\n", __func__, chg_vbat, ret);
			if (ret >= 0)
				*val = chg_vbat;
			return ret ? ret : IIO_VAL_INT;
		}

		enable = BATSNS_ENMASK;
		out_reg = AUXADC_OUT_CH0;
	} else {
		enable = BATSNS_ENMASK;
		out_reg = AUXADC_OUT_CH0;
	}

	ret = regmap_write(priv->regmap, AUXADC_RQST0, enable);
	if (ret)
		return ret;

	usleep_range(1000, 1200);

	ret = regmap_read_poll_timeout(priv->regmap, out_reg + 1, rdy_val, rdy_val & ADC_OUT_RDY,
				       500, 11*1000);
	if (ret == -ETIMEDOUT)
		dev_err(priv->dev, "(%d) channel timeout\n", chan);

	ret = regmap_raw_read(priv->regmap, out_reg, &raw_val, sizeof(raw_val));
	if (ret)
		return ret;

	*val = raw_val & (BIT(dbits) - 1);

	return IIO_VAL_INT;
}

static int gauge_get_imp_ibat(struct mt6375_priv *priv)
{
	struct power_supply *psy;
	union power_supply_propval prop;
	int ret;

	psy = power_supply_get_by_name("mtk-gauge");
	if (!psy)
		return 0;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &prop);
	if (ret)
		return ret;

	power_supply_put(psy);
	return prop.intval;
}

static int auxadc_read_imp(struct mt6375_priv *priv, int *vbat, int *ibat)
{
	unsigned int wait_time_in_ms, regval;
	const unsigned int prd_sel[] = { 6, 8, 10, 12 };
	const unsigned int cnt_sel[] = { 1, 2, 4, 8 };
	u16 raw_val;
	int ret;
	int dbits = auxadc_channels[MT6375_AUXADC_IMP].scan_type.realbits;

	if (atomic_read(&priv->vbat0_flag)) {
		dev_info(priv->dev, "%s: vbat cell abnormal, return -EIO\n", __func__);
		return -EIO;
	}

	ret = regmap_write(priv->regmap, AUXADC_IMP0, 0);
	if (ret)
		return ret;

	ret = regmap_write(priv->regmap, HK_TOP_INT_CON1_CLR, INT_RAW_AUXADC_IMP);
	if (ret)
		return ret;

	ret = regmap_write(priv->regmap, HK_TOP_INT_CON1_SET, INT_RAW_AUXADC_IMP);
	if (ret)
		return ret;

	ret = regmap_read(priv->regmap, AUXADC_IMP1, &regval);
	if (ret)
		return ret;

	wait_time_in_ms = prd_sel[regval & AUXADC_IMP_PRDSEL_MASK];
	wait_time_in_ms *= cnt_sel[(regval & AUXADC_IMP_CNTSEL_MASK) >> AUXADC_IMP_CNTSEL_SHFT];

	ret = regmap_write(priv->regmap, AUXADC_IMP0, AUXADC_IMP_ENMASK);
	if (ret)
		return ret;

	msleep(wait_time_in_ms);

	ret = regmap_read_poll_timeout(priv->regmap, HK_TOP_INT_RAW_STATUS1, regval,
				       (regval & INT_RAW_AUXADC_IMP), 100, 1000);
	if (ret == -ETIMEDOUT)
		dev_err(priv->dev, "IMP channel timeout\n");

	ret = regmap_raw_read(priv->regmap, AUXADC_OUT_IMP_AVG, &raw_val, sizeof(raw_val));
	if (ret)
		return ret;
	raw_val &= (BIT(dbits) - 1);
	*vbat = div_s64((s64)raw_val * 7360, BIT(dbits));

	ret = regmap_write(priv->regmap, AUXADC_IMP0, 0);
	if (ret)
		return ret;

	*ibat = gauge_get_imp_ibat(priv);

	return IIO_VAL_INT;
}

static int auxadc_read_scale(struct mt6375_priv *priv, int chan, int dbits, int *val1, int *val2)
{
	switch (chan) {
	case MT6375_AUXADC_BATSNS:
		if (atomic_read(&priv->vbat0_flag)) {
			*val1 = 1;
			*val2 = 1;
		} else {
			*val1 = 7360;
			*val2 = BIT(dbits);
		}
		return IIO_VAL_FRACTIONAL;
	case MT6375_AUXADC_BATSNS_DBG:
		*val1 = 7360;
		*val2 = BIT(dbits);
		return IIO_VAL_FRACTIONAL;
	case MT6375_AUXADC_BATON:
	case MT6375_AUXADC_VREF:
		*val1 = 2760;
		*val2 = BIT(dbits);
		return IIO_VAL_FRACTIONAL;
	}

	return -EINVAL;
}

static int auxadc_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan, int *val1,
			   int *val2, long mask)
{
	struct mt6375_priv *priv = iio_priv(indio_dev);
	int dbits = chan->scan_type.realbits;
	int ch_idx = chan->channel;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		switch (ch_idx) {
		case MT6375_AUXADC_IMP:
			mutex_lock(&priv->adc_lock);
			pm_stay_awake(priv->dev);
			ret = auxadc_read_imp(priv, val1, val2);
			pm_relax(priv->dev);
			mutex_unlock(&priv->adc_lock);
			return ret;
		}
		break;
	case IIO_CHAN_INFO_RAW:
		switch (ch_idx) {
		case MT6375_AUXADC_BATSNS:
		case MT6375_AUXADC_BATON:
		case MT6375_AUXADC_VREF:
		case MT6375_AUXADC_BATSNS_DBG:
			mutex_lock(&priv->adc_lock);
			pm_stay_awake(priv->dev);
			ret = auxadc_read_channel(priv, ch_idx, dbits, val1);
			pm_relax(priv->dev);
			mutex_unlock(&priv->adc_lock);
			return ret;
		case MT6375_AUXADC_IMIX_R:
			*val1 = priv->imix_r;
			return IIO_VAL_INT;
		}
		break;
	case IIO_CHAN_INFO_SCALE:
		return auxadc_read_scale(priv, ch_idx, dbits, val1, val2);
	}

	return -EINVAL;
}

static const struct iio_info auxadc_iio_info = {
	.read_raw = auxadc_read_raw,
};

static void auxadc_irq_lock(struct irq_data *data)
{
	struct mt6375_priv *priv = irq_data_get_irq_chip_data(data);

	mutex_lock(&priv->irq_lock);
}

static void auxadc_irq_sync_unlock(struct irq_data *data)
{
	struct mt6375_priv *priv = irq_data_get_irq_chip_data(data);
	int idx = data->hwirq / 8, bits = BIT(data->hwirq % 8), ret;
	unsigned int reg;

	if (priv->unmask_buf[idx] & bits)
		reg = HK_TOP_INT_CON0_SET + idx * 3;
	else
		reg = HK_TOP_INT_CON0_CLR + idx * 3;

	ret = regmap_write(priv->regmap, reg, bits);
	if (ret)
		dev_err(priv->dev, "Failed to set/clr irq con %d\n", data->hwirq);

	mutex_unlock(&priv->irq_lock);
}

static void auxadc_irq_disable(struct irq_data *data)
{
	struct mt6375_priv *priv = irq_data_get_irq_chip_data(data);

	priv->unmask_buf[data->hwirq / 8] &= ~BIT(data->hwirq % 8);
}

static void auxadc_irq_enable(struct irq_data *data)
{
	struct mt6375_priv *priv = irq_data_get_irq_chip_data(data);

	priv->unmask_buf[data->hwirq / 8] |= BIT(data->hwirq % 8);
}

static int auxadc_irq_map(struct irq_domain *h, unsigned int virq,
			  irq_hw_number_t hw)
{
	struct mt6375_priv *priv = h->host_data;

	irq_set_chip_data(virq, priv);
	irq_set_chip(virq, &priv->irq_chip);
	irq_set_nested_thread(virq, 1);
	irq_set_parent(virq, priv->irq);
	irq_set_noprobe(virq);
	return 0;
}

static const struct irq_domain_ops auxadc_domain_ops = {
	.map = auxadc_irq_map,
	.xlate = irq_domain_xlate_onetwocell,
};

static int auxadc_vbat_is_valid(struct mt6375_priv *priv, bool *valid)
{
	static struct iio_channel *auxadc_vbat_chan;
	int ret = 0, chg_vbat = 0, auxadc_vbat = 0;

	if (IS_ERR_OR_NULL(auxadc_vbat_chan))
		auxadc_vbat_chan = devm_iio_channel_get(priv->dev, "auxadc_vbat");
	if (IS_ERR(auxadc_vbat_chan))
		return PTR_ERR(auxadc_vbat_chan);

	ret = auxadc_get_chg_vbat(priv, &chg_vbat);
	dev_info(priv->dev, "%s: chg_vbat = %d(%d)\n", __func__,
		 chg_vbat, ret);

	ret |= iio_read_channel_processed(auxadc_vbat_chan, &auxadc_vbat);
	dev_info(priv->dev, "%s: auxadc_vbat = %d(%d)\n", __func__,
		 auxadc_vbat, ret);

	if (!ret && abs(chg_vbat - auxadc_vbat) > 1000) {
		dev_info(priv->dev, "%s: unexpected vbat cell!!\n", __func__);
		*valid = false;
	} else
		*valid = true;
	return ret;
}

static int auxadc_handle_vbat0(struct mt6375_priv *priv, bool is_vbat0)
{
	static struct power_supply *chg_psy;
	union power_supply_propval val;
	int i, ret;

	/* set/clr vbat0 bits */
	ret = regmap_update_bits(priv->regmap, SYSTEM_INFO_CON2_H, VBAT0_FLAG,
				 is_vbat0 ? 0xFF : 0);
	if (ret < 0) {
		dev_notice(priv->dev, "%s: failed to clear vbat0 flag\n", __func__);
		return ret;
	}
	/* notify gauge & charger */
	if (!chg_psy)
		chg_psy = devm_power_supply_get_by_phandle(priv->dev, "charger");
	if (chg_psy) {
		val.intval = is_vbat0 ? true : false;
		ret = power_supply_set_property(chg_psy,
					POWER_SUPPLY_PROP_ENERGY_EMPTY, &val);
		power_supply_changed(chg_psy);
	}

	/* mask/unmask irq & disable function */
	for (i = 0; i < ARRAY_SIZE(vbat_event); i++) {
		if (is_vbat0) {
			ret = regmap_update_bits(priv->regmap,
						 vbat_event_regs[i].addr,
						 vbat_event_regs[i].mask,
						 0);
			disable_irq_nosync(irq_find_mapping(priv->domain,
					   vbat_event[i]));
		} else {
			ret = regmap_update_bits(priv->regmap,
						 vbat_event_regs[i].addr,
						 vbat_event_regs[i].mask, 0xFF);
			enable_irq(irq_find_mapping(priv->domain,
						    vbat_event[i]));
		}
	}

	atomic_set(&priv->vbat0_flag, is_vbat0);
	return ret;
}

static void auxadc_vbat0_poll_work(struct work_struct *work)
{
	struct mt6375_priv *priv = container_of(work, struct mt6375_priv,
						vbat0_work);
	bool valid;
	ktime_t add;
	int ret;

	__pm_stay_awake(priv->vbat0_ws);
	ret = auxadc_vbat_is_valid(priv, &valid);
	if (ret < 0 || !valid) {
		dev_info(priv->dev, "%s: restart timer\n", __func__);
		add = ktime_set(VBAT0_POLL_TIME_SEC, 0);
#ifdef CONFIG_MTK_GAUGE_VBAT0_DEBUG
		alarm_forward_now(&priv->vbat0_alarm, add);
		alarm_restart(&priv->vbat0_alarm);
#endif
		__pm_relax(priv->vbat0_ws);
		return;
	}

	dev_info(priv->dev, "%s: vbat recover\n", __func__);
	if (auxadc_handle_vbat0(priv, false))
		dev_notice(priv->dev, "%s: failed to handle vbat0\n", __func__);
	__pm_relax(priv->vbat0_ws);
}

static enum alarmtimer_restart vbat0_alarm_poll_func(
					struct alarm *alarm, ktime_t now)
{
	struct mt6375_priv *priv = container_of(alarm, struct mt6375_priv,
						vbat0_alarm);
	dev_info(priv->dev, "%s: ++\n", __func__);
	schedule_work(&priv->vbat0_work);
	dev_info(priv->dev, "%s: --\n", __func__);
	return ALARMTIMER_NORESTART;
}

static int auxadc_check_vbat_event(struct mt6375_priv *priv, u8 *status_buf)
{
	int i, ret = 0, idx_i, idx_j;
	bool valid;
	ktime_t now, add;

	if (atomic_read(&priv->vbat0_flag))
		return ret;

	for (i = 0; i < ARRAY_SIZE(vbat_event); i++) {
		idx_i = vbat_event[i] / 8;
		idx_j = vbat_event[i] % 8;
		if (status_buf[idx_i] & BIT(idx_j))
			break;
	}
	if (i == ARRAY_SIZE(vbat_event)) {
		dev_info(priv->dev, "%s: without related event\n", __func__);
		return ret;
	}

	ret = auxadc_vbat_is_valid(priv, &valid);
	if (ret < 0 || valid)
		return ret;

	ret = auxadc_handle_vbat0(priv, true);
	if (ret < 0) {
		dev_notice(priv->dev, "%s: failed to handle vbat0\n", __func__);
		return ret;
	}

	/* start alarm */
	now = ktime_get_boottime();
	add = ktime_set(VBAT0_POLL_TIME_SEC, 0);
	alarm_start(&priv->vbat0_alarm, ktime_add(now, add));
	return ret;
}

static irqreturn_t auxadc_irq_thread(int irq, void *data)
{
	struct mt6375_priv *priv = data;
	static const u8 no_status[NUM_IRQ_REG];
	static const u8 mask[NUM_IRQ_REG] = { 0x3F, 0x02 };
	u8 status_buf[NUM_IRQ_REG], status;
	bool handled = false;
	int i, j, ret;

	ret = regmap_raw_read(priv->regmap, HK_TOP_INT_STATUS0, status_buf, sizeof(status_buf));
	if (ret) {
		dev_err(priv->dev, "Error reading INT status\n");
		return IRQ_HANDLED;
	}

	if (!memcmp(status_buf, no_status, NUM_IRQ_REG)) {
		return IRQ_HANDLED;
	}

	ret = auxadc_check_vbat_event(priv, status_buf);
	if (ret < 0)
		dev_info(priv->dev, "check vbat event failed\n");

	/* mask all irqs */
	for (i = 0; i < NUM_IRQ_REG; i++) {
		ret = regmap_write(priv->regmap,
				   HK_TOP_INT_MASK_CON0_SET + i * 3, mask[i]);
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

	/* after process, unmask all irqs */
	for (i = 0; i < NUM_IRQ_REG; i++) {
		ret = regmap_write(priv->regmap,
				   HK_TOP_INT_MASK_CON0_CLR + i * 3, mask[i]);
		if (ret)
			dev_err(priv->dev, "Failed to unmask irq[%d]\n", i);
	}

	ret = regmap_raw_write(priv->regmap, HK_TOP_INT_STATUS0, status_buf, sizeof(status_buf));
	if (ret)
		dev_err(priv->dev, "Error clear INT status\n");

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static int auxadc_add_irq_chip(struct mt6375_priv *priv)
{
	int i, ret;

	for (i = 0; i < NUM_IRQ_REG; i++) {
		ret = regmap_write(priv->regmap, HK_TOP_INT_CON0_CLR + i * 3, 0xFF);
		if (ret) {
			dev_err(priv->dev, "Failed to disable irq con [%d]\n", i);
			return ret;
		}

		ret = regmap_write(priv->regmap, HK_TOP_INT_MASK_CON0 + i * 3, 0);
		if (ret) {
			dev_err(priv->dev, "Failed to init irq mask [%d]\n", i);
			return ret;
		}
	}

	/* Default mask AUXADC_IMP */
	ret = regmap_update_bits(priv->regmap, HK_TOP_INT_MASK_CON1, INT_RAW_AUXADC_IMP,
				 INT_RAW_AUXADC_IMP);
	if (ret) {
		dev_err(priv->dev, "Failed to defaut unmask AUXADC_IMP\n");
		return ret;
	}

	priv->irq_chip.name = dev_name(priv->dev);
	priv->irq_chip.irq_bus_lock = auxadc_irq_lock;
	priv->irq_chip.irq_bus_sync_unlock = auxadc_irq_sync_unlock;
	priv->irq_chip.irq_disable = auxadc_irq_disable;
	priv->irq_chip.irq_enable = auxadc_irq_enable;

	priv->domain = irq_domain_add_linear(priv->dev->of_node, NUM_IRQ_REG * 8,
					     &auxadc_domain_ops, priv);
	if (!priv->domain) {
		dev_err(priv->dev, "Failed to create IRQ domain\n");
		return -ENOMEM;
	}

	ret = request_threaded_irq(priv->irq, NULL, auxadc_irq_thread, IRQF_SHARED | IRQF_ONESHOT,
				   dev_name(priv->dev), priv);
	if (ret) {
		dev_err(priv->dev, "Failed to request IRQ %d for %s: %d\n", priv->irq,
			dev_name(priv->dev), ret);
		goto err_irq;
	}

	enable_irq_wake(priv->irq);
	return 0;

err_irq:
	irq_domain_remove(priv->domain);
	return ret;
}

static void auxadc_del_irq_chip(struct mt6375_priv *priv)
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

static int auxadc_reset(struct mt6375_priv *priv)
{
	u8 reset_key[2] = { 0x63, 0x63 };
	int ret;

	ret = regmap_raw_write(priv->regmap, HK_TOP_WKEY, reset_key, sizeof(reset_key));
	if (ret)
		return ret;

	ret = regmap_write(priv->regmap, HK_TOP_RST_CON0, RG_RESET_MASK);
	if (ret)
		return ret;

	ret = regmap_write(priv->regmap, HK_TOP_RST_CON0, 0);
	if (ret)
		return ret;

	reset_key[0] = reset_key[1] = 0;
	return regmap_raw_write(priv->regmap, HK_TOP_WKEY, reset_key, sizeof(reset_key));
}

static int auxadc_get_uisoc(void)
{
	struct power_supply *psy;
	union power_supply_propval prop;
	int ret;

	psy = power_supply_get_by_name("battery");
	if (!psy)
		return -ENODEV;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &prop);
	if (ret || prop.intval < 0)
		return -EINVAL;

	power_supply_put(psy);
	return prop.intval;
}

static int auxadc_get_rac(struct mt6375_priv *priv)
{
	int vbat_1 = 0, vbat_2 = 0;
	int ibat_1 = 0, ibat_2 = 0;
	int rac = 0, ret = 0;
	int retry_count = 0;

	/* to make sure dummy load has been disabled */
	if (regulator_is_enabled(priv->isink_load))
		regulator_disable(priv->isink_load);

	do {

		mutex_lock(&priv->adc_lock);
		ret = auxadc_read_imp(priv, &vbat_1, &ibat_1);
		mutex_unlock(&priv->adc_lock);
		if (ret < 0)
			return ret;

		/* enable_dummy_load */
		ret = regulator_enable(priv->isink_load);
		if (ret)
			return ret;

		mdelay(50);

		mutex_lock(&priv->adc_lock);
		ret = auxadc_read_imp(priv, &vbat_2, &ibat_2);
		mutex_unlock(&priv->adc_lock);
		if (ret < 0)
			return ret;

		/* disable_dummy_load */
		ret = regulator_disable(priv->isink_load);
		if (ret)
			return ret;

		mdelay(50);

		/* translate to 0.1mV */
		vbat_1 *= 10;
		vbat_2 *= 10;

		/* Cal.Rac: 70mA <= c_diff <= 120mA, 4mV <= v_diff <= 200mV */
		if ((ibat_2 - ibat_1) >= 700 && (ibat_2 - ibat_1) <= 1200 &&
		    (vbat_1 - vbat_2) >= 40 && (vbat_1 - vbat_2) <= 2000) {
			/*m-ohm */
			rac = ((vbat_1 - vbat_2) * 1000) / (ibat_2 - ibat_1);
			if (rac < 0)
				ret = (rac - (rac * 2)) * 1;
			else
				ret = rac * 1;
			if (ret < 50)
				ret = -1;
		} else
			ret = -1;

		dev_info(priv->dev, "v1=%d,v2=%d,c1=%d,c2=%d,v_diff=%d,c_diff=%d,rac=%d,ret=%d,retry=%d\n",
			vbat_1, vbat_2, ibat_1, ibat_2,
			(vbat_1 - vbat_2), (ibat_2 - ibat_1),
			rac, ret, retry_count);

		if (++retry_count >= 3)
			break;
	} while (ret == -1);

	return ret;
}

#define IMIX_R_MIN_MOHM	100
#define IMIX_R_CALI_CNT	2

static int auxadc_cali_imix_r(struct mt6375_priv *priv)
{
	struct power_supply *psy;
	int cur_uisoc = auxadc_get_uisoc();
	int i, imix_r_avg = 0, rac_val[IMIX_R_CALI_CNT];

	psy = power_supply_get_by_name("mtk-gauge");
	if (!psy) {
		dev_info(priv->dev, "%s gauge disabled, skip\n", __func__);
		return -ENODEV;
	}
	power_supply_put(psy);

	if (cur_uisoc < 0 || cur_uisoc == priv->pre_uisoc) {
		dev_info(priv->dev, "%s, pre_SOC=%d SOC= %d, skip\n", __func__,
			 priv->pre_uisoc, cur_uisoc);
		return 0;
	}

	priv->pre_uisoc = cur_uisoc;

	for (i = 0; i < IMIX_R_CALI_CNT; i++) {
		rac_val[i] = auxadc_get_rac(priv);
		if (rac_val[i] < 0)
			return -EIO;

		imix_r_avg += rac_val[i];
	}

	imix_r_avg /= IMIX_R_CALI_CNT;
	if (imix_r_avg > IMIX_R_MIN_MOHM)
		priv->imix_r = imix_r_avg;

	dev_info(priv->dev, "[%s] %d, %d, ravg:%d\n", __func__, rac_val[0], rac_val[1], imix_r_avg);
	return 0;
}

static int mt6375_auxadc_parse_dt(struct mt6375_priv *priv)
{
	int ret = 0;
	struct device_node *np;
	u32 val = 0;

	np = of_find_compatible_node(NULL, NULL, "mediatek,pmic-auxadc");
	if (!np)
		return -ENODEV;

	np = of_get_child_by_name(np, "imix_r");
	if (!np) {
		dev_notice(priv->dev, "no imix_r(%d)\n", ret);
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "val", &val);
	if (ret) {
		dev_notice(priv->dev, "no imix_r(%d)\n", ret);
		return ret;
	}
	priv->imix_r = val;
	dev_info(priv->dev, "%s: imix_r = %d\n", __func__, priv->imix_r);
	return ret;
}

static int mt6375_auxadc_probe(struct platform_device *pdev)
{
	struct mt6375_priv *priv;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*priv));
	if (!indio_dev)
		return -ENOMEM;

	priv = iio_priv(indio_dev);
	priv->dev = &pdev->dev;
	mutex_init(&priv->adc_lock);
	mutex_init(&priv->irq_lock);
	priv->pre_uisoc = 101;
	atomic_set(&priv->vbat0_flag, 0);
	device_init_wakeup(&pdev->dev, true);
	platform_set_drvdata(pdev, priv);
	priv->vbat0_ws = wakeup_source_register(&pdev->dev, "vbat0_ws");
	lockdep_register_key(&priv->info_exist_key);
	lockdep_set_class(&indio_dev->info_exist_lock, &priv->info_exist_key);

	ret = mt6375_auxadc_parse_dt(priv);
	if (ret) {
		dev_notice(&pdev->dev, "Failed to parse dt\n");
		return ret;
	}

	priv->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!priv->regmap) {
		dev_err(&pdev->dev, "Failed to get parent regmap\n");
		return -ENODEV;
	}

	ret = auxadc_reset(priv);
	if (ret) {
		dev_err(&pdev->dev, "Failed to reset\n");
		return ret;
	}

	priv->irq = platform_get_irq(pdev, 0);
	if (priv->irq < 0) {
		dev_err(&pdev->dev, "Failed to get gm30 irq\n");
		return priv->irq;
	}

	ret = auxadc_add_irq_chip(priv);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add irq chip\n");
		return ret;
	}

	INIT_WORK(&priv->vbat0_work, auxadc_vbat0_poll_work);
	alarm_init(&priv->vbat0_alarm, ALARM_BOOTTIME, vbat0_alarm_poll_func);

	priv->isink_load = devm_regulator_get_exclusive(&pdev->dev, "isink_load");
	if (IS_ERR(priv->isink_load)) {
		dev_err(&pdev->dev, "Failed to get isink_load regulator [%d]\n",
			PTR_ERR(priv->isink_load));
		return PTR_ERR(priv->isink_load);
	}

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &auxadc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = auxadc_channels;
	indio_dev->num_channels = ARRAY_SIZE(auxadc_channels);

	return devm_iio_device_register(&pdev->dev, indio_dev);
}

static int mt6375_auxadc_remove(struct platform_device *pdev)
{
	struct mt6375_priv *priv = platform_get_drvdata(pdev);

	lockdep_unregister_key(&priv->info_exist_key);
	auxadc_del_irq_chip(priv);
	return 0;
}

static int mt6375_auxadc_suspend_late(struct device *dev)
{
	struct mt6375_priv *priv = dev_get_drvdata(dev);
	int ret;

	ret = auxadc_cali_imix_r(priv);
	if (ret)
		dev_err(dev, "calibrate imix_r ret=[%d]\n", ret);

	dev_info(priv->dev, "%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops mt6375_auxadc_dev_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(mt6375_auxadc_suspend_late, NULL)
};

static const struct of_device_id __maybe_unused mt6375_auxadc_of_match[] = {
	{ .compatible = "mediatek,mt6375-auxadc", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6375_auxadc_of_match);

static struct platform_driver mt6375_auxadc_driver = {
	.probe = mt6375_auxadc_probe,
	.remove = mt6375_auxadc_remove,
	.driver = {
		.name = "mt6375-auxadc",
		.of_match_table = mt6375_auxadc_of_match,
		.pm = &mt6375_auxadc_dev_pm_ops,
	},
};
module_platform_driver(mt6375_auxadc_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6375 AUXADC Driver");
MODULE_LICENSE("GPL v2");
