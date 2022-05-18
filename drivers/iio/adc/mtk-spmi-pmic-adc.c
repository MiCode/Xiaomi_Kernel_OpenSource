// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iio/iio.h>
#include <linux/kernel.h>
#include <linux/mfd/mt6363/registers.h>
#include <linux/mfd/mt6368/registers.h>
#include <linux/mfd/mt6373/registers.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/syscore_ops.h>

#include <dt-bindings/iio/mt635x-auxadc.h>

#define AUXADC_RDY_BIT			BIT(15)

#define AUXADC_DEF_R_RATIO		1
#define AUXADC_DEF_AVG_NUM		32

#define AUXADC_AVG_TIME_US		10
#define AUXADC_POLL_DELAY_US		100
#define AUXADC_TIMEOUT_US		32000
#define VOLT_FULL			1840

#define IMP_VOLT_FULL			18400
#define IMIX_R_MIN_MOHM			100
#define IMIX_R_CALI_CNT			2

#define EXT_THR_PURES_SHIFT		3
#define EXT_THR_SEL_MASK		0x1F

#define DT_CHANNEL_CONVERT(val)		((val) & 0xFF)
#define DT_PURES_CONVERT(val)		((val) & 0xFF00) >> 8

struct pmic_adc_device {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
	struct iio_chan_spec *iio_chans;
	unsigned int nchannels;
	const struct auxadc_info *info;
	struct regulator *isink_load;
	int imix_r;
	int imp_curr;
	int pre_uisoc;
};

static struct pmic_adc_device *imix_r_dev;

/*
 * @ch_name:	HW channel name
 * @res:	ADC resolution
 * @r_ratio:	resistance ratio, represented by r_ratio[0] / r_ratio[1]
 * @avg_num:	sampling times of AUXADC measurments then average it
 * @regs:	request and data output registers for this channel
 */
struct auxadc_channels {
	enum iio_chan_type type;
	long info_mask;
	/* AUXADC channel attribute */
	const char *ch_name;
	unsigned char res;
	unsigned char r_ratio[2];
	unsigned short avg_num;
	const struct auxadc_regs *regs;
};

#define AUXADC_CHANNEL(_ch_name, _res)	\
	[AUXADC_##_ch_name] = {				\
		.type = IIO_VOLTAGE,			\
		.info_mask = BIT(IIO_CHAN_INFO_RAW) |		\
			     BIT(IIO_CHAN_INFO_PROCESSED),	\
		.ch_name = __stringify(_ch_name),	\
		.res = _res,				\
	}

/*
 * The array represents all possible AUXADC channels found
 * in the supported PMICs.
 */
static struct auxadc_channels auxadc_chans[] = {
	AUXADC_CHANNEL(BATADC, 15),
	AUXADC_CHANNEL(BAT_TEMP, 12),
	AUXADC_CHANNEL(CHIP_TEMP, 12),
	AUXADC_CHANNEL(VCORE_TEMP, 12),
	AUXADC_CHANNEL(VPROC_TEMP, 12),
	AUXADC_CHANNEL(VGPU_TEMP, 12),
	AUXADC_CHANNEL(ACCDET, 12),
	AUXADC_CHANNEL(HPOFS_CAL, 15),
	AUXADC_CHANNEL(VTREF, 12),
	AUXADC_CHANNEL(IMP, 15),
	[AUXADC_IMIX_R] = {
		.type = IIO_RESISTANCE,
		.info_mask = BIT(IIO_CHAN_INFO_RAW),
		.ch_name = "IMIX_R",
	},
	AUXADC_CHANNEL(VSYSSNS, 15),
	AUXADC_CHANNEL(VIN1, 15),
	AUXADC_CHANNEL(VIN2, 15),
	AUXADC_CHANNEL(VIN3, 15),
	AUXADC_CHANNEL(VIN4, 15),
	AUXADC_CHANNEL(VIN5, 15),
	AUXADC_CHANNEL(VIN6, 15),
	AUXADC_CHANNEL(VIN7, 15),
};

struct auxadc_regs {
	unsigned int enable_reg;
	unsigned int enable_mask;
	unsigned int ready_reg;
	unsigned int ready_mask;
	unsigned int value_reg;
	unsigned int ext_thr_sel;
	u8 src_sel;
};

#define AUXADC_REG(_ch_name, _chip, _enable_reg, _enable_mask, _value_reg) \
	[AUXADC_##_ch_name] = {				\
		.enable_reg = _chip##_##_enable_reg,	\
		.enable_mask = _enable_mask,		\
		.ready_reg = _chip##_##_value_reg,	\
		.ready_mask = AUXADC_RDY_BIT,		\
		.value_reg = _chip##_##_value_reg,	\
	}						\

#define TIA_ADC_REG(_src_sel, _chip)	\
	[AUXADC_VIN##_src_sel] = {			\
		.enable_reg = _chip##_AUXADC_RQST1,	\
		.enable_mask = BIT(4),			\
		.ready_reg = _chip##_AUXADC_ADC_CH12_L,	\
		.ready_mask = AUXADC_RDY_BIT,		\
		.value_reg = _chip##_AUXADC_ADC_CH12_L,	\
		.ext_thr_sel = _chip##_SDMADC_CON0,	\
		.src_sel = _src_sel,			\
	}						\

static const struct auxadc_regs mt6363_auxadc_regs_tbl[] = {
	AUXADC_REG(BATADC, MT6363, AUXADC_RQST0, BIT(0), AUXADC_ADC0_L),
	AUXADC_REG(BAT_TEMP, MT6363, AUXADC_RQST0, BIT(3), AUXADC_ADC3_L),
	AUXADC_REG(CHIP_TEMP, MT6363, AUXADC_RQST0, BIT(4), AUXADC_ADC4_L),
	AUXADC_REG(VCORE_TEMP, MT6363, AUXADC_RQST3, BIT(0), AUXADC_ADC38_L),
	AUXADC_REG(VPROC_TEMP, MT6363, AUXADC_RQST3, BIT(1), AUXADC_ADC39_L),
	AUXADC_REG(VGPU_TEMP, MT6363, AUXADC_RQST3, BIT(2), AUXADC_ADC40_L),
	AUXADC_REG(VTREF, MT6363, AUXADC_RQST1, BIT(3), AUXADC_ADC11_L),
	[AUXADC_IMP] = {
		.enable_reg = MT6363_AUXADC_IMP0,
		.enable_mask = BIT(0),
		.ready_reg = MT6363_AUXADC_IMP1,
		.ready_mask = BIT(7),
		.value_reg = MT6363_AUXADC_ADC42_L,
	},
	AUXADC_REG(VSYSSNS, MT6363, AUXADC_RQST1, BIT(6), AUXADC_ADC_CH14_L),
	TIA_ADC_REG(1, MT6363),
	TIA_ADC_REG(2, MT6363),
	TIA_ADC_REG(3, MT6363),
	TIA_ADC_REG(4, MT6363),
	TIA_ADC_REG(5, MT6363),
	TIA_ADC_REG(6, MT6363),
	TIA_ADC_REG(7, MT6363),
};

static const struct auxadc_regs mt6368_auxadc_regs_tbl[] = {
	AUXADC_REG(CHIP_TEMP, MT6368, AUXADC_RQST0, BIT(4), AUXADC_ADC4_L),
	AUXADC_REG(VCORE_TEMP, MT6368, AUXADC_RQST3, BIT(0), AUXADC_ADC38_L),
	AUXADC_REG(VPROC_TEMP, MT6368, AUXADC_RQST3, BIT(1), AUXADC_ADC39_L),
	AUXADC_REG(VGPU_TEMP, MT6368, AUXADC_RQST3, BIT(2), AUXADC_ADC40_L),
	AUXADC_REG(ACCDET, MT6368, AUXADC_RQST0, BIT(5), AUXADC_ADC5_L),
	AUXADC_REG(HPOFS_CAL, MT6368, AUXADC_RQST1, BIT(1), AUXADC_ADC9_L),
	TIA_ADC_REG(1, MT6368),
	TIA_ADC_REG(2, MT6368),
};

static const struct auxadc_regs mt6373_auxadc_regs_tbl[] = {
	AUXADC_REG(CHIP_TEMP, MT6373, AUXADC_RQST0, BIT(4), AUXADC_ADC4_L),
	AUXADC_REG(VCORE_TEMP, MT6373, AUXADC_RQST3, BIT(0), AUXADC_ADC38_L),
	AUXADC_REG(VPROC_TEMP, MT6373, AUXADC_RQST3, BIT(1), AUXADC_ADC39_L),
	AUXADC_REG(VGPU_TEMP, MT6373, AUXADC_RQST3, BIT(2), AUXADC_ADC40_L),
	TIA_ADC_REG(1, MT6373),
	TIA_ADC_REG(2, MT6373),
	TIA_ADC_REG(3, MT6373),
	TIA_ADC_REG(4, MT6373),
	TIA_ADC_REG(5, MT6373),
};

struct auxadc_info {
	const struct auxadc_regs *regs_tbl;
};

static const struct auxadc_info mt6363_info = {
	.regs_tbl = mt6363_auxadc_regs_tbl,
};

static const struct auxadc_info mt6368_info = {
	.regs_tbl = mt6368_auxadc_regs_tbl,
};

static const struct auxadc_info mt6373_info = {
	.regs_tbl = mt6373_auxadc_regs_tbl,
};

#define regmap_bulk_read_poll_timeout(map, addr, val, val_count, cond, sleep_us, timeout_us) \
({ \
	u64 __timeout_us = (timeout_us); \
	unsigned long __sleep_us = (sleep_us); \
	ktime_t __timeout = ktime_add_us(ktime_get(), __timeout_us); \
	int __ret; \
	might_sleep_if(__sleep_us); \
	for (;;) { \
		__ret = regmap_bulk_read((map), (addr), (u8 *) &(val), val_count); \
		if (__ret) \
			break; \
		if (cond) \
			break; \
		if ((__timeout_us) && \
		    ktime_compare(ktime_get(), __timeout) > 0) { \
			__ret = regmap_bulk_read((map), (addr), (u8 *) &(val), val_count); \
			break; \
		} \
		if (__sleep_us) \
			usleep_range((__sleep_us >> 2) + 1, __sleep_us); \
	} \
	__ret ?: ((cond) ? 0 : -ETIMEDOUT); \
})

/*
 * @adc_dev:	 pointer to the struct pmic_adc_device
 * @auxadc_chan: pointer to the struct auxadc_channels, it represents specific
		 auxadc channel
 * @val:	 pointer to output value
 */
static int get_auxadc_out(struct pmic_adc_device *adc_dev,
			  int channel, int channel2, int *val)
{
	int ret;
	u16 buf = 0;
	const struct auxadc_channels *auxadc_chan = &auxadc_chans[channel];

	if (!auxadc_chan->regs)
		return -EINVAL;

	if (auxadc_chan->regs->ext_thr_sel) {
		buf = (channel2 << EXT_THR_PURES_SHIFT)
			| auxadc_chan->regs->src_sel;
		ret = regmap_update_bits(adc_dev->regmap,
					 auxadc_chan->regs->ext_thr_sel,
					 EXT_THR_SEL_MASK, buf);
	}
	regmap_write(adc_dev->regmap,
		     auxadc_chan->regs->enable_reg,
		     auxadc_chan->regs->enable_mask);
	usleep_range(auxadc_chan->avg_num * AUXADC_AVG_TIME_US,
		     (auxadc_chan->avg_num + 1) * AUXADC_AVG_TIME_US);

	ret = regmap_bulk_read_poll_timeout(adc_dev->regmap,
					    auxadc_chan->regs->value_reg,
					    buf, 2,
					    (buf & AUXADC_RDY_BIT),
					    AUXADC_POLL_DELAY_US,
					    AUXADC_TIMEOUT_US);
	*val = buf & (BIT(auxadc_chan->res) - 1);
	if (ret == -ETIMEDOUT)
		dev_err(adc_dev->dev, "%s Time out!\n", auxadc_chan->ch_name);

	/* set PURES to OPEN after measuring done */
	if (auxadc_chan->regs->ext_thr_sel) {
		buf = (ADC_PURES_OPEN << EXT_THR_PURES_SHIFT)
			| auxadc_chan->regs->src_sel;
		ret = regmap_update_bits(adc_dev->regmap,
					 auxadc_chan->regs->ext_thr_sel,
					 EXT_THR_SEL_MASK, buf);
	}

	return ret;
}

static int gauge_get_imp_ibat(void)
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

static int get_imp_out(struct pmic_adc_device *adc_dev, int *val)
{
	int ret, cnt = 0;
	int max_cnt = AUXADC_TIMEOUT_US / AUXADC_POLL_DELAY_US;
	unsigned int buf = 0;
	const struct auxadc_channels *auxadc_chan = &auxadc_chans[AUXADC_IMP];

	if (!auxadc_chan->regs)
		return -EINVAL;

	regmap_write(adc_dev->regmap,
		     auxadc_chan->regs->enable_reg,
		     auxadc_chan->regs->enable_mask);
	for (;;) {
		ret = regmap_read(adc_dev->regmap, auxadc_chan->regs->ready_reg, &buf);
		if (ret)
			break;
		if (buf & auxadc_chan->regs->ready_mask)
			break;
		if ((cnt)++ > max_cnt) {
			dev_err(adc_dev->dev, "IMP Time out over %d times\n", cnt);
			return -ETIMEDOUT;
		}
		udelay(AUXADC_POLL_DELAY_US);
	}

	ret = regmap_bulk_read(adc_dev->regmap, auxadc_chan->regs->value_reg, (u8 *) &buf, 2);
	if (ret)
		return ret;
	*val = buf & (BIT(auxadc_chan->res) - 1);
	adc_dev->imp_curr = gauge_get_imp_ibat();

	return 0;
}

static int pmic_adc_read_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan,
			     int *val, int *val2, long mask)
{
	struct pmic_adc_device *adc_dev = iio_priv(indio_dev);
	const struct auxadc_channels *auxadc_chan;
	int auxadc_out = 0;
	int ret = 0;

	mutex_lock(&adc_dev->lock);
	switch (chan->channel) {
	case AUXADC_IMP:
		ret = get_imp_out(adc_dev, &auxadc_out);
		break;
	case AUXADC_IMIX_R:
		auxadc_out = adc_dev->imix_r;
		break;
	default:
		ret = get_auxadc_out(adc_dev,
				     chan->channel, chan->channel2,
				     &auxadc_out);
		break;
	}
	mutex_unlock(&adc_dev->lock);

	if (ret != -ETIMEDOUT && ret < 0)
		goto err;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		auxadc_chan = &auxadc_chans[chan->channel];
		*val = auxadc_out * auxadc_chan->r_ratio[0] * VOLT_FULL;
		*val = (*val / auxadc_chan->r_ratio[1]) >> auxadc_chan->res;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_RAW:
		*val = auxadc_out;
		ret = IIO_VAL_INT;
		break;
	default:
		return -EINVAL;
	}
	if (chan->channel == AUXADC_IMP) {
		*val2 = adc_dev->imp_curr;
		ret = IIO_VAL_INT_MULTIPLE;
	}
err:
	return ret;
}

static int pmic_adc_of_xlate(struct iio_dev *indio_dev,
			     const struct of_phandle_args *iiospec)
{
	int i;
	int channel = DT_CHANNEL_CONVERT(iiospec->args[0]);
	int channel2 = DT_PURES_CONVERT(iiospec->args[0]);

	for (i = 0; i < indio_dev->num_channels; i++) {
		if (indio_dev->channels[i].channel == channel &&
		    indio_dev->channels[i].channel2 == channel2)
			return i;
	}

	return -EINVAL;
}

static const struct iio_info pmic_adc_info = {
	.read_raw = &pmic_adc_read_raw,
	.of_xlate = &pmic_adc_of_xlate,
};

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

/* vbat unit is 0.1mV; ibat unit is 0.1mA */
static int auxadc_get_imp_vbat_ibat(struct pmic_adc_device *adc_dev,
				    int *vbat, int *ibat)
{
	const struct auxadc_channels *imp_chan = &auxadc_chans[AUXADC_IMP];
	int auxadc_out;
	int ret;

	mutex_lock(&adc_dev->lock);
	ret = get_imp_out(adc_dev, &auxadc_out);
	mutex_unlock(&adc_dev->lock);
	if (ret)
		return ret;

	*vbat = auxadc_out * imp_chan->r_ratio[0] * IMP_VOLT_FULL;
	*vbat = (*vbat / imp_chan->r_ratio[1]) >> imp_chan->res;
	*ibat = adc_dev->imp_curr;

	return 0;
}

static int auxadc_get_rac(struct pmic_adc_device *adc_dev)
{
	int vbat_1 = 0, vbat_2 = 0;
	int ibat_1 = 0, ibat_2 = 0;
	int rac = 0;
	int ret, retry_count = 0;

	if (!adc_dev->isink_load)
		return -EINVAL;
	do {
		ret = auxadc_get_imp_vbat_ibat(adc_dev, &vbat_1, &ibat_1);
		if (ret)
			break;

		/* enable dummy_load */
		ret = regulator_enable(adc_dev->isink_load);
		if (ret)
			return ret;
		mdelay(50);

		ret = auxadc_get_imp_vbat_ibat(adc_dev, &vbat_2, &ibat_2);
		if (ret)
			break;
		/* disable dummy_load */
		ret = regulator_disable(adc_dev->isink_load);
		if (ret)
			return ret;

		/* Cal.Rac: 70mA <= c_diff <= 120mA, 4mV <= v_diff <= 200mV */
		if ((ibat_2 - ibat_1) >= 700 && (ibat_2 - ibat_1) <= 1200 &&
		    (vbat_1 - vbat_2) >= 40 && (vbat_1 - vbat_2) <= 2000) {
			/*m-ohm */
			rac = ((vbat_1 - vbat_2) * 1000) / (ibat_2 - ibat_1);
			if (rac < 0)
				rac = -rac;
			if (rac < 50) {
				ret = -EAGAIN;
				dev_info(adc_dev->dev, "bypass due to Rac=%d too small\n", rac);
			}
		} else {
			ret = -EAGAIN;
			dev_info(adc_dev->dev,
				 "bypass due to c_diff or v_diff not in range\n");
		}
		dev_info(adc_dev->dev,
			 "v1=%d,v2=%d,c1=%d,c2=%d,v_diff=%d,c_diff=%d\n",
			 vbat_1, vbat_2, ibat_1, ibat_2,
			 (vbat_1 - vbat_2), (ibat_2 - ibat_1));
		dev_info(adc_dev->dev, "rac=%d,ret=%d,retry=%d\n",
			 rac, ret, retry_count);

		if (++retry_count >= 3)
			break;
	} while (ret == -EAGAIN);

	return ret < 0 ? ret : rac;
}

static int auxadc_init_imix_r(struct pmic_adc_device *adc_dev,
			      struct device_node *imix_r_node)
{
	unsigned int val = 0;
	int ret;

	if (!adc_dev)
		return -EINVAL;

	adc_dev->isink_load = devm_regulator_get_exclusive(adc_dev->dev, "isink_load");
	if (IS_ERR(adc_dev->isink_load)) {
		dev_err(adc_dev->dev, "Failed to get isink_load regulator, ret=%d\n",
			PTR_ERR(adc_dev->isink_load));
		return PTR_ERR(adc_dev->isink_load);
	}

	imix_r_dev = adc_dev;
	if (imix_r_dev->imix_r)
		return 0;

	ret = of_property_read_u32(imix_r_node, "val", &val);
	if (ret)
		dev_notice(imix_r_dev->dev, "no imix_r, ret=%d\n", ret);
	imix_r_dev->imix_r = (int)val;
	imix_r_dev->pre_uisoc = 101;
	return 0;
}

static int auxadc_cali_imix_r(void)
{
	struct power_supply *psy;
	int cur_uisoc = auxadc_get_uisoc();
	int i, imix_r_avg = 0;
	int rac_val[IMIX_R_CALI_CNT];

	if (!imix_r_dev)
		return -EINVAL;

	psy = power_supply_get_by_name("mtk-gauge");
	if (!psy) {
		dev_info(imix_r_dev->dev, "gauge disabled, skip\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	if (cur_uisoc < 0 || cur_uisoc == imix_r_dev->pre_uisoc) {
		dev_info(imix_r_dev->dev, "pre_SOC=%d SOC=%d, skip\n",
			 imix_r_dev->pre_uisoc, cur_uisoc);
		return 0;
	}

	imix_r_dev->pre_uisoc = cur_uisoc;
	for (i = 0; i < IMIX_R_CALI_CNT; i++) {
		rac_val[i] = auxadc_get_rac(imix_r_dev);
		if (rac_val[i] < 0) {
			dev_info(imix_r_dev->dev,
				 "get_rac error,ret=%d\n", rac_val[i]);
			return -EINVAL;
		}
		imix_r_avg += rac_val[i];
	}

	imix_r_avg = imix_r_avg / IMIX_R_CALI_CNT;
	if (imix_r_avg > IMIX_R_MIN_MOHM)
		imix_r_dev->imix_r = imix_r_avg;
	dev_info(imix_r_dev->dev, "%d, %d, imix_r_avg:%d\n",
		 rac_val[0], rac_val[1], imix_r_avg);

	return 0;
}

static int auxadc_suspend_enter(void)
{
	auxadc_cali_imix_r();
	return 0;
}

static struct syscore_ops auxadc_syscore_ops = {
	.suspend = auxadc_suspend_enter,
};

static int auxadc_get_data_from_dt(struct pmic_adc_device *adc_dev,
				   struct iio_chan_spec *iio_chan,
				   struct device_node *node)
{
	struct auxadc_channels *auxadc_chan;
	unsigned int channel = 0;
	unsigned int value = 0;
	unsigned int val_arr[2] = {0};
	int ret;

	ret = of_property_read_u32(node, "channel", &channel);
	if (ret) {
		dev_notice(adc_dev->dev, "invalid channel in node:%s\n",
			   node->name);
		return ret;
	}
	if (channel > AUXADC_CHAN_MAX) {
		dev_notice(adc_dev->dev, "invalid channel number %d in node:%s\n",
			   channel, node->name);
		return -EINVAL;
	}
	if (channel >= ARRAY_SIZE(auxadc_chans)) {
		dev_notice(adc_dev->dev, "channel number %d in node:%s not exists\n",
			   channel, node->name);
		return -EINVAL;
	}
	iio_chan->channel = channel;
	iio_chan->datasheet_name = auxadc_chans[channel].ch_name;
	iio_chan->info_mask_separate = auxadc_chans[channel].info_mask;
	iio_chan->type = auxadc_chans[channel].type;
	iio_chan->extend_name = node->name;
	ret = of_property_read_u32(node, "pures", &value);
	if (!ret)
		iio_chan->channel2 = value;

	if (channel == AUXADC_IMIX_R)
		return auxadc_init_imix_r(adc_dev, node);
	else {
		auxadc_chan = &auxadc_chans[channel];
		auxadc_chan->regs = &adc_dev->info->regs_tbl[channel];
	}

	ret = of_property_read_u32_array(node, "resistance-ratio", val_arr, 2);
	if (!ret) {
		auxadc_chan->r_ratio[0] = val_arr[0];
		auxadc_chan->r_ratio[1] = val_arr[1];
	} else {
		auxadc_chan->r_ratio[0] = AUXADC_DEF_R_RATIO;
		auxadc_chan->r_ratio[1] = 1;
	}

	ret = of_property_read_u32(node, "avg-num", &value);
	if (!ret)
		auxadc_chan->avg_num = value;
	else
		auxadc_chan->avg_num = AUXADC_DEF_AVG_NUM;

	return 0;
}

static int auxadc_parse_dt(struct pmic_adc_device *adc_dev,
			   struct device_node *node)
{
	struct iio_chan_spec *iio_chan;
	struct device_node *child;
	unsigned int index = 0;
	int ret;

	adc_dev->nchannels = of_get_available_child_count(node);
	if (!adc_dev->nchannels)
		return -EINVAL;

	adc_dev->iio_chans = devm_kcalloc(adc_dev->dev, adc_dev->nchannels,
		sizeof(*adc_dev->iio_chans), GFP_KERNEL);
	if (!adc_dev->iio_chans)
		return -ENOMEM;
	iio_chan = adc_dev->iio_chans;

	for_each_available_child_of_node(node, child) {
		ret = auxadc_get_data_from_dt(adc_dev, iio_chan, child);
		if (ret < 0) {
			of_node_put(child);
			return ret;
		}
		iio_chan->indexed = 1;
		iio_chan->address = index++;
		iio_chan++;
	}

	return 0;
}

static int pmic_adc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct pmic_adc_device *adc_dev;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*adc_dev));
	if (!indio_dev)
		return -ENOMEM;

	adc_dev = iio_priv(indio_dev);
	adc_dev->dev = &pdev->dev;
	adc_dev->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	mutex_init(&adc_dev->lock);
	adc_dev->info = of_device_get_match_data(&pdev->dev);

	ret = auxadc_parse_dt(adc_dev, node);
	if (ret) {
		dev_notice(&pdev->dev, "auxadc_parse_dt fail, ret=%d\n", ret);
		return ret;
	}

	indio_dev->dev.parent = &pdev->dev;
	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->info = &pmic_adc_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = adc_dev->iio_chans;
	indio_dev->num_channels = adc_dev->nchannels;

	ret = devm_iio_device_register(&pdev->dev, indio_dev);
	if (ret < 0) {
		dev_notice(&pdev->dev, "failed to register iio device!\n");
		return ret;
	}
	if (adc_dev->imix_r)
		register_syscore_ops(&auxadc_syscore_ops);
	dev_info(&pdev->dev, "%s done\n", __func__);

	return 0;
}

static const struct of_device_id pmic_adc_of_match[] = {
	{ .compatible = "mediatek,mt6363-auxadc", .data = &mt6363_info, },
	{ .compatible = "mediatek,mt6368-auxadc", .data = &mt6368_info, },
	{ .compatible = "mediatek,mt6373-auxadc", .data = &mt6373_info, },
	{ }
};
MODULE_DEVICE_TABLE(of, pmic_adc_of_match);

static struct platform_driver pmic_adc_driver = {
	.driver = {
		.name = "mtk-spmi-pmic-adc",
		.of_match_table = pmic_adc_of_match,
	},
	.probe	= pmic_adc_probe,
};
module_platform_driver(pmic_adc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SPMI PMIC ADC Driver");
