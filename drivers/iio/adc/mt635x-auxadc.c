// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iio/iio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/mt6357/registers.h>
#include <linux/mfd/mt6358/registers.h>
#include <linux/mfd/mt6359/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <dt-bindings/iio/mt635x-auxadc.h>

#define AUXADC_RDY_BIT			BIT(15)

#define AUXADC_DEF_R_RATIO		1
#define AUXADC_DEF_AVG_NUM		8

#define AUXADC_AVG_TIME_US		10
#define AUXADC_POLL_DELAY_US		100
#define AUXADC_TIMEOUT_US		32000
#define VOLT_FULL			1800
#define IMP_STOP_DELAY_US		150

struct mt635x_auxadc_device {
	unsigned int chip_id;
	struct regmap *regmap;
	struct device *dev;
	unsigned int nchannels;
	struct iio_chan_spec *iio_chans;
	struct mutex lock;
	const struct auxadc_info *info;
	int imp_vbat;
	struct completion imp_done;
	int imix_r;
};

/*
 * @ch_name:	HW channel name
 * @ch_num:	HW channel number
 * @res:	ADC resolution
 * @r_ratio:	resistance ratio, represented by r_ratio[0] / r_ratio[1]
 * @avg_num:	sampling times of AUXADC measurments then average it
 * @regs:	request and data output registers for this channel
 * @has_regs:	determine if this channel has request and data output registers
 */
struct auxadc_channels {
	enum iio_chan_type type;
	long info_mask;
	/* AUXADC channel attribute */
	const char *ch_name;
	unsigned char ch_num;
	unsigned char res;
	unsigned char r_ratio[2];
	unsigned short avg_num;
	const struct auxadc_regs *regs;
	bool has_regs;
};

#define MT635x_AUXADC_CHANNEL(_ch_name, _ch_num, _res, _has_regs)	\
	[AUXADC_##_ch_name] = {				\
		.type = IIO_VOLTAGE,			\
		.info_mask = BIT(IIO_CHAN_INFO_RAW) |		\
			     BIT(IIO_CHAN_INFO_PROCESSED),	\
		.ch_name = __stringify(_ch_name),	\
		.ch_num = _ch_num,			\
		.res = _res,				\
		.has_regs = _has_regs,			\
	}

/*
 * The array represents all possible AUXADC channels found
 * in the supported PMICs.
 */
static struct auxadc_channels auxadc_chans[] = {
	MT635x_AUXADC_CHANNEL(BATADC, 0, 15, true),
	MT635x_AUXADC_CHANNEL(ISENSE, 0, 15, true),
	MT635x_AUXADC_CHANNEL(VCDT, 2, 12, true),
	MT635x_AUXADC_CHANNEL(BAT_TEMP, 3, 12, true),
	MT635x_AUXADC_CHANNEL(BATID, 3, 12, true),
	MT635x_AUXADC_CHANNEL(CHIP_TEMP, 4, 12, true),
	MT635x_AUXADC_CHANNEL(VCORE_TEMP, 4, 12, true),
	MT635x_AUXADC_CHANNEL(VPROC_TEMP, 4, 12, true),
	MT635x_AUXADC_CHANNEL(VGPU_TEMP, 4, 12, true),
	MT635x_AUXADC_CHANNEL(ACCDET, 5, 12, true),
	MT635x_AUXADC_CHANNEL(VDCXO, 6, 12, true),
	MT635x_AUXADC_CHANNEL(TSX_TEMP, 7, 15, true),
	MT635x_AUXADC_CHANNEL(HPOFS_CAL, 9, 15, true),
	MT635x_AUXADC_CHANNEL(DCXO_TEMP, 10, 15, true),
	MT635x_AUXADC_CHANNEL(VBIF, 11, 12, true),
	MT635x_AUXADC_CHANNEL(IMP, 0, 15, false),
	[AUXADC_IMIX_R] = {
		.type = IIO_RESISTANCE,
		.info_mask = BIT(IIO_CHAN_INFO_RAW),
		.ch_name = "IMIX_R",
		.has_regs = false,
	},
};

struct auxadc_regs {
	unsigned int rqst_reg;
	unsigned int rqst_shift;
	unsigned int out_reg;
};

#define MT635x_AUXADC_REG(_ch_name, _chip, _rqst_reg, _rqst_shift, _out_reg) \
	[AUXADC_##_ch_name] = {				\
		.rqst_reg = _chip##_##_rqst_reg,	\
		.rqst_shift = _rqst_shift,		\
		.out_reg = _chip##_##_out_reg,		\
	}						\

static const struct auxadc_regs mt6357_auxadc_regs_tbl[] = {
	MT635x_AUXADC_REG(BATADC, MT6357, AUXADC_RQST0, 0, AUXADC_ADC0),
	MT635x_AUXADC_REG(ISENSE, MT6357, AUXADC_RQST0, 1, AUXADC_ADC1),
	MT635x_AUXADC_REG(VCDT, MT6357, AUXADC_RQST0, 2, AUXADC_ADC2),
	MT635x_AUXADC_REG(BAT_TEMP, MT6357, AUXADC_RQST0, 3, AUXADC_ADC3),
	MT635x_AUXADC_REG(CHIP_TEMP, MT6357, AUXADC_RQST0, 4, AUXADC_ADC4),
	MT635x_AUXADC_REG(VCORE_TEMP, MT6357, AUXADC_RQST2, 5, AUXADC_ADC46),
	MT635x_AUXADC_REG(VPROC_TEMP, MT6357, AUXADC_RQST2, 6, AUXADC_ADC47),
	MT635x_AUXADC_REG(ACCDET, MT6357, AUXADC_RQST0, 5, AUXADC_ADC5),
	MT635x_AUXADC_REG(TSX_TEMP, MT6357, AUXADC_RQST0, 7, AUXADC_ADC7),
	MT635x_AUXADC_REG(HPOFS_CAL, MT6357, AUXADC_RQST0, 9, AUXADC_ADC9),
	MT635x_AUXADC_REG(DCXO_TEMP, MT6357, AUXADC_RQST0, 4, AUXADC_ADC40),
	MT635x_AUXADC_REG(VBIF, MT6357, AUXADC_RQST0, 11, AUXADC_ADC11),
};

static const struct auxadc_regs mt6358_auxadc_regs_tbl[] = {
	MT635x_AUXADC_REG(BATADC, MT6358, AUXADC_RQST0, 0, AUXADC_ADC0),
	MT635x_AUXADC_REG(VCDT, MT6358, AUXADC_RQST0, 2, AUXADC_ADC2),
	MT635x_AUXADC_REG(BAT_TEMP, MT6358, AUXADC_RQST0, 3, AUXADC_ADC3),
	MT635x_AUXADC_REG(CHIP_TEMP, MT6358, AUXADC_RQST0, 4, AUXADC_ADC4),
	MT635x_AUXADC_REG(VCORE_TEMP, MT6358, AUXADC_RQST1, 8, AUXADC_ADC38),
	MT635x_AUXADC_REG(VPROC_TEMP, MT6358, AUXADC_RQST1, 9, AUXADC_ADC39),
	MT635x_AUXADC_REG(VGPU_TEMP, MT6358, AUXADC_RQST1, 10, AUXADC_ADC40),
	MT635x_AUXADC_REG(ACCDET, MT6358, AUXADC_RQST0, 5, AUXADC_ADC5),
	MT635x_AUXADC_REG(VDCXO, MT6358, AUXADC_RQST0, 6, AUXADC_ADC6),
	MT635x_AUXADC_REG(TSX_TEMP, MT6358, AUXADC_RQST0, 7, AUXADC_ADC7),
	MT635x_AUXADC_REG(HPOFS_CAL, MT6358, AUXADC_RQST0, 9, AUXADC_ADC9),
	MT635x_AUXADC_REG(DCXO_TEMP, MT6358, AUXADC_RQST0, 10, AUXADC_ADC10),
	MT635x_AUXADC_REG(VBIF, MT6358, AUXADC_RQST0, 11, AUXADC_ADC11),
};

static const struct auxadc_regs mt6359_auxadc_regs_tbl[] = {
	MT635x_AUXADC_REG(BATADC, MT6359, AUXADC_RQST0, 0, AUXADC_ADC0),
	MT635x_AUXADC_REG(BAT_TEMP, MT6359, AUXADC_RQST0, 3, AUXADC_ADC3),
	MT635x_AUXADC_REG(CHIP_TEMP, MT6359, AUXADC_RQST0, 4, AUXADC_ADC4),
	MT635x_AUXADC_REG(VCORE_TEMP, MT6359, AUXADC_RQST1, 8, AUXADC_ADC38),
	MT635x_AUXADC_REG(VPROC_TEMP, MT6359, AUXADC_RQST1, 9, AUXADC_ADC39),
	MT635x_AUXADC_REG(VGPU_TEMP, MT6359, AUXADC_RQST1, 10, AUXADC_ADC40),
	MT635x_AUXADC_REG(ACCDET, MT6359, AUXADC_RQST0, 5, AUXADC_ADC5),
	MT635x_AUXADC_REG(VDCXO, MT6359, AUXADC_RQST0, 6, AUXADC_ADC6),
	MT635x_AUXADC_REG(TSX_TEMP, MT6359, AUXADC_RQST0, 7, AUXADC_ADC7),
	MT635x_AUXADC_REG(HPOFS_CAL, MT6359, AUXADC_RQST0, 9, AUXADC_ADC9),
	MT635x_AUXADC_REG(DCXO_TEMP, MT6359, AUXADC_RQST0, 10, AUXADC_ADC10),
	MT635x_AUXADC_REG(VBIF, MT6359, AUXADC_RQST0, 11, AUXADC_ADC11),
};

static const unsigned int mt6357_rst_setting[][3] = {
	{
		MT6357_HK_TOP_RST_CON0, 0x9, 0x9,
	}, {
		MT6357_HK_TOP_RST_CON0, 0x9, 0,
	}, {
		MT6357_AUXADC_RQST0, 0x80, 0x80,
	}, {
		MT6357_AUXADC_RQST1, 0x400, 0x400,
	}
};

static const unsigned int mt6358_rst_setting[][3] = {
	{
		MT6358_HK_TOP_RST_CON0, 0x9, 0x9,
	}, {
		MT6358_HK_TOP_RST_CON0, 0x9, 0,
	}, {
		MT6358_AUXADC_RQST0, 0x80, 0x80,
	}, {
		MT6358_AUXADC_RQST1, 0x40, 0x40,
	}
};

static const unsigned int mt6359_rst_setting[][3] = {
	{
		MT6359_HK_TOP_WKEY, 0xFFFF, 0x6359,
	}, {
		MT6359_HK_TOP_RST_CON0, 0x9, 0x9,
	}, {
		MT6359_HK_TOP_RST_CON0, 0x9, 0,
	}, {
		MT6359_HK_TOP_WKEY, 0xFFFF, 0,
	}, {
		MT6359_AUXADC_RQST0, 0x80, 0x80,
	}, {
		MT6359_AUXADC_RQST1, 0x40, 0x40,
	}
};

struct auxadc_info {
	const struct auxadc_regs *regs_tbl;
	const unsigned int (*rst_setting)[3];
	unsigned int num_rst_setting;
	int (*imp_conv)(struct mt635x_auxadc_device *adc_dev,
			int *vbat, int *ibat);
	void (*imp_stop)(struct mt635x_auxadc_device *adc_dev);
};

#define MT6357_IMP_CK_SW_MODE_MASK	BIT(0)
#define MT6357_IMP_CK_SW_EN_MASK	BIT(1)
#define MT6357_IMP_AUTORPT_EN_MASK	BIT(15)
#define MT6357_IMP_CLR_MASK		(BIT(14) | BIT(7))

static int mt6357_imp_conv(struct mt635x_auxadc_device *adc_dev,
			   int *vbat, int *ibat)
{
	int ret;

	reinit_completion(&adc_dev->imp_done);
	/* start conversion */
	regmap_update_bits(adc_dev->regmap, MT6357_AUXADC_IMP_CG0,
			   MT6357_IMP_CK_SW_MODE_MASK,
			   MT6357_IMP_CK_SW_MODE_MASK);
	regmap_update_bits(adc_dev->regmap, MT6357_AUXADC_IMP_CG0,
			   MT6357_IMP_CK_SW_EN_MASK, MT6357_IMP_CK_SW_EN_MASK);
	regmap_update_bits(adc_dev->regmap, MT6357_AUXADC_IMP1,
			   MT6357_IMP_AUTORPT_EN_MASK,
			   MT6357_IMP_AUTORPT_EN_MASK);
	ret = wait_for_completion_timeout(&adc_dev->imp_done,
					  usecs_to_jiffies(AUXADC_TIMEOUT_US));
	if (!ret) {
		adc_dev->info->imp_stop(adc_dev);
		dev_err(adc_dev->dev, "IMP Time out!\n");
		ret = -ETIMEDOUT;
	}
	*vbat = adc_dev->imp_vbat;
	regmap_read(adc_dev->regmap, MT6357_FGADC_R_CON0, ibat);

	return ret;
}

static void mt6357_imp_stop(struct mt635x_auxadc_device *adc_dev)
{
	regmap_read(adc_dev->regmap, MT6357_AUXADC_ADC33, &adc_dev->imp_vbat);
	adc_dev->imp_vbat &= BIT(auxadc_chans[AUXADC_IMP].res) - 1;
	/* stop conversion after read VBAT */
	regmap_update_bits(adc_dev->regmap, MT6357_AUXADC_IMP0,
			   MT6357_IMP_CLR_MASK, MT6357_IMP_CLR_MASK);
	regmap_update_bits(adc_dev->regmap, MT6357_AUXADC_IMP0,
			   MT6357_IMP_CLR_MASK, 0);
	regmap_update_bits(adc_dev->regmap, MT6357_AUXADC_IMP1,
			   MT6357_IMP_AUTORPT_EN_MASK, 0);
	regmap_update_bits(adc_dev->regmap, MT6357_AUXADC_IMP_CG0,
			   MT6357_IMP_CK_SW_MODE_MASK, 0);
	regmap_update_bits(adc_dev->regmap, MT6357_AUXADC_IMP_CG0,
			   MT6357_IMP_CK_SW_EN_MASK, MT6357_IMP_CK_SW_EN_MASK);
}

#define MT6358_IMP_CK_SW_MASK		(BIT(1) | BIT(0))
#define MT6358_IMP_AUTORPT_EN_MASK	BIT(15)
#define MT6358_IMP_CLR_MASK		(BIT(14) | BIT(7))

static int mt6358_imp_conv(struct mt635x_auxadc_device *adc_dev,
			   int *vbat, int *ibat)
{
	int ret;

	reinit_completion(&adc_dev->imp_done);
	/* start conversion */
	regmap_update_bits(adc_dev->regmap, MT6358_AUXADC_DCM_CON,
			   MT6358_IMP_CK_SW_MASK, MT6358_IMP_CK_SW_MASK);
	regmap_update_bits(adc_dev->regmap, MT6358_AUXADC_IMP1,
			   MT6358_IMP_AUTORPT_EN_MASK,
			   MT6358_IMP_AUTORPT_EN_MASK);
	ret = wait_for_completion_timeout(&adc_dev->imp_done,
					  usecs_to_jiffies(AUXADC_TIMEOUT_US));
	if (!ret) {
		adc_dev->info->imp_stop(adc_dev);
		dev_err(adc_dev->dev, "IMP Time out!\n");
		ret = -ETIMEDOUT;
	}
	*vbat = adc_dev->imp_vbat;
	regmap_read(adc_dev->regmap, MT6358_FGADC_R_CON0, ibat);

	return ret;
}

static void mt6358_imp_stop(struct mt635x_auxadc_device *adc_dev)
{
	regmap_read(adc_dev->regmap, MT6358_AUXADC_ADC28, &adc_dev->imp_vbat);
	adc_dev->imp_vbat &= BIT(auxadc_chans[AUXADC_IMP].res) - 1;
	/* stop conversion after read VBAT */
	regmap_update_bits(adc_dev->regmap, MT6358_AUXADC_IMP0,
			   MT6358_IMP_CLR_MASK, MT6358_IMP_CLR_MASK);
	regmap_update_bits(adc_dev->regmap, MT6358_AUXADC_IMP0,
			   MT6358_IMP_CLR_MASK, 0);
	regmap_update_bits(adc_dev->regmap, MT6358_AUXADC_IMP1,
			   MT6358_IMP_AUTORPT_EN_MASK, 0);
	regmap_update_bits(adc_dev->regmap, MT6358_AUXADC_DCM_CON,
			   MT6358_IMP_CK_SW_MASK, 0);
}

static int mt6359_imp_conv(struct mt635x_auxadc_device *adc_dev,
			   int *vbat, int *ibat)
{
	int ret;

	reinit_completion(&adc_dev->imp_done);
	/* start conversion */
	regmap_write(adc_dev->regmap, MT6359_AUXADC_IMP0, 1);
	ret = wait_for_completion_timeout(&adc_dev->imp_done,
					  usecs_to_jiffies(AUXADC_TIMEOUT_US));
	if (!ret) {
		adc_dev->info->imp_stop(adc_dev);
		dev_err(adc_dev->dev, "IMP Time out!\n");
		ret = -ETIMEDOUT;
	}
	*vbat = adc_dev->imp_vbat;
	regmap_read(adc_dev->regmap, MT6359_FGADC_R_CON0, ibat);

	return ret;
}

static void mt6359_imp_stop(struct mt635x_auxadc_device *adc_dev)
{
	/* stop conversio */
	regmap_write(adc_dev->regmap, MT6359_AUXADC_IMP0, 0);
	udelay(IMP_STOP_DELAY_US);
	regmap_read(adc_dev->regmap, MT6359_AUXADC_IMP3, &adc_dev->imp_vbat);
	adc_dev->imp_vbat &= BIT(auxadc_chans[AUXADC_IMP].res) - 1;
}

static const struct auxadc_info mt6357_info = {
	.regs_tbl = mt6357_auxadc_regs_tbl,
	.rst_setting = mt6357_rst_setting,
	.num_rst_setting = ARRAY_SIZE(mt6357_rst_setting),
	.imp_conv = mt6357_imp_conv,
	.imp_stop = mt6357_imp_stop,
};

static const struct auxadc_info mt6358_info = {
	.regs_tbl = mt6358_auxadc_regs_tbl,
	.rst_setting = mt6358_rst_setting,
	.num_rst_setting = ARRAY_SIZE(mt6358_rst_setting),
	.imp_conv = mt6358_imp_conv,
	.imp_stop = mt6358_imp_stop,
};

static const struct auxadc_info mt6359_info = {
	.regs_tbl = mt6359_auxadc_regs_tbl,
	.rst_setting = mt6359_rst_setting,
	.num_rst_setting = ARRAY_SIZE(mt6359_rst_setting),
	.imp_conv = mt6359_imp_conv,
	.imp_stop = mt6359_imp_stop,
};

static irqreturn_t imp_isr(int irq, void *dev_id)
{
	struct mt635x_auxadc_device *adc_dev = dev_id;

	adc_dev->info->imp_stop(adc_dev);
	complete(&adc_dev->imp_done);
	return IRQ_HANDLED;
}

static void auxadc_reset(struct mt635x_auxadc_device *adc_dev)
{
	int i;

	for (i = 0; i < adc_dev->info->num_rst_setting; i++) {
		regmap_update_bits(adc_dev->regmap,
				   adc_dev->info->rst_setting[i][0],
				   adc_dev->info->rst_setting[i][1],
				   adc_dev->info->rst_setting[i][2]);
	}
	dev_info(adc_dev->dev, "reset AUXADC done\n");
}

/*
 * @adc_dev:	 pointer to the struct mt635x_auxadc_device
 * @auxadc_chan: pointer to the struct auxadc_channels, it represents specific
		 auxadc channel
 * @val:	 pointer to output value
 */
static int get_auxadc_out(struct mt635x_auxadc_device *adc_dev,
			  const struct auxadc_channels *auxadc_chan, int *val)
{
	int ret;

	regmap_write(adc_dev->regmap,
		     auxadc_chan->regs->rqst_reg,
		     BIT(auxadc_chan->regs->rqst_shift));
	usleep_range(auxadc_chan->avg_num * AUXADC_AVG_TIME_US,
		     (auxadc_chan->avg_num + 1) * AUXADC_AVG_TIME_US);

	ret = regmap_read_poll_timeout(adc_dev->regmap,
				       auxadc_chan->regs->out_reg,
				       *val,
				       (*val & AUXADC_RDY_BIT),
				       AUXADC_POLL_DELAY_US,
				       AUXADC_TIMEOUT_US);
	*val &= BIT(auxadc_chan->res) - 1;
	if (ret == -ETIMEDOUT)
		dev_err(adc_dev->dev, "(%d)Time out!\n", auxadc_chan->ch_num);

	return ret;
}

static int mt635x_auxadc_read_raw(struct iio_dev *indio_dev,
				  struct iio_chan_spec const *chan,
				  int *val,
				  int *val2,
				  long mask)
{
	struct mt635x_auxadc_device *adc_dev = iio_priv(indio_dev);
	const struct auxadc_channels *auxadc_chan;
	int auxadc_out = 0;
	int ret;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 5);

	mutex_lock(&adc_dev->lock);
	pm_stay_awake(adc_dev->dev);

	auxadc_chan = &auxadc_chans[chan->channel];
	switch (chan->channel) {
	case AUXADC_IMP:
		if (adc_dev->info->imp_conv)
			ret = adc_dev->info->imp_conv(adc_dev,
						      &auxadc_out, val2);
		else
			ret = -EINVAL;
		break;
	case AUXADC_IMIX_R:
		auxadc_out = adc_dev->imix_r;
		ret = 0;
		break;
	default:
		if (auxadc_chan->regs)
			ret = get_auxadc_out(adc_dev, auxadc_chan,
					     &auxadc_out);
		else
			ret = -EINVAL;
		break;
	}

	pm_relax(adc_dev->dev);
	mutex_unlock(&adc_dev->lock);
	if (ret != -ETIMEDOUT && ret < 0)
		goto err;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
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
	if (chan->channel == AUXADC_IMP)
		ret = IIO_VAL_INT_MULTIPLE;
	if (__ratelimit(&ratelimit)) {
		dev_info(adc_dev->dev,
			"name:%s, channel=%d, adc_out=0x%x, adc_result=%d\n",
			auxadc_chan->ch_name, auxadc_chan->ch_num,
			auxadc_out, *val);
	}
err:
	return ret;
}

static int mt635x_auxadc_of_xlate(struct iio_dev *indio_dev,
				  const struct of_phandle_args *iiospec)
{
	int i;

	for (i = 0; i < indio_dev->num_channels; i++) {
		if (indio_dev->channels[i].channel == iiospec->args[0])
			return i;
	}

	return -EINVAL;
}

static const struct iio_info mt635x_auxadc_info = {
	.read_raw = &mt635x_auxadc_read_raw,
	.of_xlate = &mt635x_auxadc_of_xlate,
};

static int auxadc_init_imix_r(struct mt635x_auxadc_device *adc_dev,
			      struct device_node *imix_r_node)
{
	unsigned int val = 0;
	int ret;

	if (adc_dev->imix_r)
		return 0;
	ret = of_property_read_u32(imix_r_node, "val", &val);
	if (ret)
		dev_notice(adc_dev->dev, "no imix_r, ret=%d\n", ret);
	adc_dev->imix_r = (int)val;
	return 0;
}

static int auxadc_get_data_from_dt(struct mt635x_auxadc_device *adc_dev,
				   unsigned int *channel,
				   struct device_node *node)
{
	struct auxadc_channels *auxadc_chan;
	unsigned int value, val_arr[2];
	int ret;

	ret = of_property_read_u32(node, "channel", channel);
	if (ret) {
		dev_notice(adc_dev->dev,
			"invalid channel in node:%s\n", node->name);
		return ret;
	}
	if (*channel < AUXADC_CHAN_MIN || *channel > AUXADC_CHAN_MAX) {
		dev_notice(adc_dev->dev,
			"invalid channel number %d in node:%s\n",
			*channel, node->name);
		return ret;
	}
	if (*channel == AUXADC_IMIX_R)
		return auxadc_init_imix_r(adc_dev, node);
	auxadc_chan = &auxadc_chans[*channel];

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

static int auxadc_parse_dt(struct mt635x_auxadc_device *adc_dev,
			   struct device_node *node)
{
	struct iio_chan_spec *iio_chan;
	struct device_node *child;
	unsigned int channel = 0, index = 0;
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
		ret = auxadc_get_data_from_dt(adc_dev, &channel, child);
		if (ret) {
			of_node_put(child);
			return ret;
		}
		if (auxadc_chans[channel].has_regs) {
			auxadc_chans[channel].regs =
				&adc_dev->info->regs_tbl[channel];
		}

		iio_chan->channel = channel;
		iio_chan->datasheet_name = auxadc_chans[channel].ch_name;
		iio_chan->extend_name = auxadc_chans[channel].ch_name;
		iio_chan->info_mask_separate = auxadc_chans[channel].info_mask;
		iio_chan->type = auxadc_chans[channel].type;
		iio_chan->indexed = 1;
		iio_chan->address = index++;

		iio_chan++;
	}

	return 0;
}

static int mt635x_auxadc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct mt635x_auxadc_device *adc_dev;
	struct iio_dev *indio_dev;
	struct mt6397_chip *chip;
	int ret, imp_irq;

	chip = dev_get_drvdata(pdev->dev.parent);
	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*adc_dev));
	if (!indio_dev)
		return -ENOMEM;

	adc_dev = iio_priv(indio_dev);
	adc_dev->regmap = chip->regmap;
	adc_dev->dev = &pdev->dev;
	mutex_init(&adc_dev->lock);
	init_completion(&adc_dev->imp_done);
	device_init_wakeup(&pdev->dev, true);
	adc_dev->info = of_device_get_match_data(&pdev->dev);
	imp_irq = platform_get_irq_byname(pdev, "imp");
	if (imp_irq < 0) {
		dev_notice(&pdev->dev, "failed to get IMP irq, ret=%d\n",
			   imp_irq);
		return imp_irq;
	}
	ret = devm_request_threaded_irq(&pdev->dev, imp_irq, NULL, imp_isr,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					"auxadc_imp", adc_dev);
	if (ret) {
		dev_notice(&pdev->dev,
			   "failed to request IMP irq, ret=%d\n", ret);
		return ret;
	}

	ret = auxadc_parse_dt(adc_dev, node);
	if (ret < 0) {
		dev_notice(&pdev->dev, "auxadc_parse_dt fail, ret=%d\n", ret);
		return ret;
	}
	auxadc_reset(adc_dev);

	indio_dev->dev.parent = &pdev->dev;
	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->info = &mt635x_auxadc_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = adc_dev->iio_chans;
	indio_dev->num_channels = adc_dev->nchannels;

	ret = devm_iio_device_register(&pdev->dev, indio_dev);
	if (ret < 0) {
		dev_notice(&pdev->dev, "failed to register iio device!\n");
		return ret;
	}
	dev_info(&pdev->dev, "%s done\n", __func__);

	return 0;
}

static const struct of_device_id mt635x_auxadc_of_match[] = {
	{
		.compatible = "mediatek,mt6357-auxadc",
		.data = &mt6357_info,
	}, {
		.compatible = "mediatek,mt6358-auxadc",
		.data = &mt6358_info,
	}, {
		.compatible = "mediatek,mt6359-auxadc",
		.data = &mt6359_info,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, mt635x_auxadc_of_match);

static struct platform_driver mt635x_auxadc_driver = {
	.driver = {
		.name = "mt635x-auxadc",
		.of_match_table = mt635x_auxadc_of_match,
	},
	.probe	= mt635x_auxadc_probe,
};
module_platform_driver(mt635x_auxadc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MediaTek PMIC AUXADC Driver for MT635x PMIC");
