/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/iio/adc/mt635x-auxadc-internal.h>
#include <linux/regmap.h>
#include <linux/mfd/mt6358/core.h>
#if defined(CONFIG_MTK_PMIC_CHIP_MT6357)
#include <linux/mfd/mt6357/registers.h>
#elif defined(CONFIG_MTK_PMIC_CHIP_MT6358)
#include <linux/mfd/mt6358/registers.h>
#elif defined(CONFIG_MTK_PMIC_CHIP_MT6359)
#include <linux/mfd/mt6359/registers.h>
#elif defined(CONFIG_MTK_PMIC_CHIP_MT6359P)
#include <linux/mfd/mt6359p/registers.h>
#elif defined(CONFIG_MTK_PMIC_CHIP_MT6390)
#include <linux/mfd/mt6390/registers.h>
#endif

#define AUXADC_RDY_SHIFT		15

#define AUXADC_DEF_R_RATIO		1
#define AUXADC_DEF_AVG_NUM		8

#define AUXADC_AVG_TIME_US		10
#define MAX_TIMEOUT_COUNT		160 /* 100us-200us * 160 = 16ms-32ms */
#define VOLT_FULL			1800

#define AUXADC_CHAN_MIN			AUXADC_BATADC
#define AUXADC_CHAN_MAX			AUXADC_VBIF

struct mt635x_auxadc_device {
	unsigned int chip_id;
	struct regmap *regmap;
	struct device *dev;
	unsigned int nchannels;
	struct iio_chan_spec *iio_chans;
	struct mutex lock;
	const struct auxadc_regs *auxadc_regs_tbl;
	const unsigned int *dbg_regs;
	unsigned int num_dbg_regs;
	const unsigned int (*rst_setting)[3];
	unsigned int num_rst_setting;
};

/*
 * @res:	ADC resolution
 * @r_ratio:	resistance ratio, represented by r_ratio[0] / r_ratio[1]
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
	void (*convert_fn)(unsigned char convert);
	int (*cali_fn)(int val, int precision_factor);
};

#define MT635x_AUXADC_CHANNEL(_ch_name, _ch_num, _res)	\
	[AUXADC_##_ch_name] = {						\
		.type = IIO_VOLTAGE,					\
		.info_mask = BIT(IIO_CHAN_INFO_RAW) |			\
			BIT(IIO_CHAN_INFO_PROCESSED),			\
		.ch_name = __stringify(_ch_name),			\
		.ch_num = _ch_num,					\
		.res = _res,						\
	}								\

/*
 * The array represents all possible ADC channels found in the supported PMICs.
 * Every index in the array is equal to the channel number per datasheet. The
 * gaps in the array should be treated as reserved channels.
 */
static struct auxadc_channels auxadc_chans[] = {
	MT635x_AUXADC_CHANNEL(BATADC, 0, 15),
	MT635x_AUXADC_CHANNEL(ISENSE, 0, 15),
	MT635x_AUXADC_CHANNEL(VCDT, 2, 12),
	MT635x_AUXADC_CHANNEL(BAT_TEMP, 3, 12),
	MT635x_AUXADC_CHANNEL(BATID, 3, 12),
	MT635x_AUXADC_CHANNEL(CHIP_TEMP, 4, 12),
	MT635x_AUXADC_CHANNEL(VCORE_TEMP, 4, 12),
	MT635x_AUXADC_CHANNEL(VPROC_TEMP, 4, 12),
	MT635x_AUXADC_CHANNEL(VGPU_TEMP, 4, 12),
	MT635x_AUXADC_CHANNEL(ACCDET, 5, 12),
	MT635x_AUXADC_CHANNEL(VDCXO, 6, 12),
	MT635x_AUXADC_CHANNEL(TSX_TEMP, 7, 15),
	MT635x_AUXADC_CHANNEL(HPOFS_CAL, 9, 15),
	MT635x_AUXADC_CHANNEL(DCXO_TEMP, 10, 15),
	MT635x_AUXADC_CHANNEL(VBIF, 11, 12),
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
#if defined(CONFIG_MTK_PMIC_CHIP_MT6357)
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
#endif
#if defined(CONFIG_MTK_PMIC_CHIP_MT6390)
	MT635x_AUXADC_REG(BATADC, MT6390, AUXADC_RQST0, 0, AUXADC_ADC0),
	MT635x_AUXADC_REG(ISENSE, MT6390, AUXADC_RQST0, 1, AUXADC_ADC1),
	MT635x_AUXADC_REG(VCDT, MT6390, AUXADC_RQST0, 2, AUXADC_ADC2),
	MT635x_AUXADC_REG(BAT_TEMP, MT6390, AUXADC_RQST0, 3, AUXADC_ADC3),
	MT635x_AUXADC_REG(CHIP_TEMP, MT6390, AUXADC_RQST0, 4, AUXADC_ADC4),
	MT635x_AUXADC_REG(VCORE_TEMP, MT6390, AUXADC_RQST2, 5, AUXADC_ADC46),
	MT635x_AUXADC_REG(VPROC_TEMP, MT6390, AUXADC_RQST2, 6, AUXADC_ADC47),
	MT635x_AUXADC_REG(ACCDET, MT6390, AUXADC_RQST0, 5, AUXADC_ADC5),
	MT635x_AUXADC_REG(TSX_TEMP, MT6390, AUXADC_RQST0, 7, AUXADC_ADC7),
	MT635x_AUXADC_REG(HPOFS_CAL, MT6390, AUXADC_RQST0, 9, AUXADC_ADC9),
	MT635x_AUXADC_REG(DCXO_TEMP, MT6390, AUXADC_RQST0, 4, AUXADC_ADC40),
	MT635x_AUXADC_REG(VBIF, MT6390, AUXADC_RQST0, 11, AUXADC_ADC11),
#endif
};

static const struct auxadc_regs mt6358_auxadc_regs_tbl[] = {
#if defined(CONFIG_MTK_PMIC_CHIP_MT6358)
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
#endif
};

static const struct auxadc_regs mt6359_auxadc_regs_tbl[] = {
#if defined(CONFIG_MTK_PMIC_CHIP_MT6359) \
|| defined(CONFIG_MTK_PMIC_CHIP_MT6359P)
	MT635x_AUXADC_REG(BATADC, MT6359, AUXADC_RQST0, 0, AUXADC_ADC0),
	MT635x_AUXADC_REG(VCDT, MT6359, AUXADC_RQST0, 2, AUXADC_ADC2),
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
#endif
};

static const unsigned int mt6357_dbg_regs[] = {
#if defined(CONFIG_MTK_PMIC_CHIP_MT6357)
	MT6357_AUXADC_STA0, MT6357_AUXADC_STA1, MT6357_AUXADC_STA2,
	MT6357_STRUP_CON6, MT6357_HK_TOP_RST_CON0,
	MT6357_HK_TOP_CLK_CON0, MT6357_HK_TOP_CLK_CON1,
#endif
#if defined(CONFIG_MTK_PMIC_CHIP_MT6390)
	MT6390_AUXADC_STA0, MT6390_AUXADC_STA1, MT6390_AUXADC_STA2,
	MT6390_STRUP_CON6, MT6390_HK_TOP_RST_CON0,
	MT6390_HK_TOP_CLK_CON0, MT6390_HK_TOP_CLK_CON1,
#endif
};

static const unsigned int mt6358_dbg_regs[] = {
#if defined(CONFIG_MTK_PMIC_CHIP_MT6358)
	MT6358_AUXADC_STA0, MT6358_AUXADC_STA1, MT6358_AUXADC_STA2,
	MT6358_STRUP_CON6, MT6358_HK_TOP_RST_CON0,
	MT6358_HK_TOP_CLK_CON0, MT6358_HK_TOP_CLK_CON1,
	MT6358_AUXADC_CON20, /* check DATA_REUSE */
#endif
};

static const unsigned int mt6359_dbg_regs[] = {
#if defined(CONFIG_MTK_PMIC_CHIP_MT6359) \
|| defined(CONFIG_MTK_PMIC_CHIP_MT6359P)
	MT6359_AUXADC_STA0, MT6359_AUXADC_STA1, MT6359_AUXADC_STA2,
	MT6359_AUXADC_CON5, MT6359_AUXADC_CON9, MT6359_AUXADC_CON21,
	MT6359_HK_TOP_LDO_STATUS,
	MT6359_AUXADC_NAG_9, MT6359_AUXADC_NAG_10, MT6359_AUXADC_NAG_11,
	MT6359_AUXADC_IMP3, MT6359_AUXADC_IMP4, MT6359_AUXADC_IMP5,
	MT6359_AUXADC_LBAT6, MT6359_AUXADC_LBAT7, MT6359_AUXADC_LBAT8,
	MT6359_AUXADC_BAT_TEMP_7, MT6359_AUXADC_BAT_TEMP_8,
	MT6359_AUXADC_BAT_TEMP_9,
	MT6359_AUXADC_LBAT2_6, MT6359_AUXADC_LBAT2_7, MT6359_AUXADC_LBAT2_8,
	MT6359_AUXADC_MDRT_3, MT6359_AUXADC_MDRT_4, MT6359_AUXADC_MDRT_5,
	MT6359_HK_TOP_STRUP, MT6359_HK_TOP_RST_CON0,
	MT6359_HK_TOP_CLK_CON0, MT6359_HK_TOP_CLK_CON1,
	MT6359_AUXADC_CON20, /* check DATA_REUSE */
#endif
};

static const unsigned int mt6357_rst_setting[][3] = {
#if defined(CONFIG_MTK_PMIC_CHIP_MT6357)
	{
		MT6357_HK_TOP_RST_CON0, 0x9, 0x9,
	}, {
		MT6357_HK_TOP_RST_CON0, 0x9, 0,
	}, {
		MT6357_AUXADC_RQST0, 0x80, 0x80,
	}, {
		MT6357_AUXADC_RQST1, 0x400, 0x400,
	}
#endif
#if defined(CONFIG_MTK_PMIC_CHIP_MT6390)
	{
		MT6390_HK_TOP_RST_CON0, 0x9, 0x9,
	}, {
		MT6390_HK_TOP_RST_CON0, 0x9, 0,
	}, {
		MT6390_AUXADC_RQST0, 0x80, 0x80,
	}, {
		MT6390_AUXADC_RQST1, 0x400, 0x400,
	}
#endif
};

static const unsigned int mt6358_rst_setting[][3] = {
#if defined(CONFIG_MTK_PMIC_CHIP_MT6358)
	{
		MT6358_HK_TOP_RST_CON0, 0x9, 0x9,
	}, {
		MT6358_HK_TOP_RST_CON0, 0x9, 0,
	}, {
		MT6358_AUXADC_RQST0, 0x80, 0x80,
	}, {
		MT6358_AUXADC_RQST1, 0x40, 0x40,
	}
#endif
};

static const unsigned int mt6359_rst_setting[][3] = {
#if defined(CONFIG_MTK_PMIC_CHIP_MT6359) \
|| defined(CONFIG_MTK_PMIC_CHIP_MT6359P)
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
#endif
};

static unsigned short get_auxadc_out(struct mt635x_auxadc_device *adc_dev,
				     int channel);

/* Exported function for chip init */
void auxadc_set_convert_fn(unsigned int channel,
			   void (*convert_fn)(unsigned char))
{
	auxadc_chans[channel].convert_fn = convert_fn;
}

void auxadc_set_cali_fn(unsigned int channel, int (*cali_fn)(int, int))
{
	auxadc_chans[channel].cali_fn = cali_fn;
}

int auxadc_priv_read_channel(struct device *dev, int channel)
{
	const struct auxadc_channels *auxadc_chan;
	struct iio_dev *indio_dev;
	struct mt635x_auxadc_device *adc_dev;
	int val;

	auxadc_chan = &auxadc_chans[channel];
	indio_dev = platform_get_drvdata(to_platform_device(dev));
	adc_dev = iio_priv(indio_dev);

	val = (int)get_auxadc_out(adc_dev, channel);
	val = val * auxadc_chan->r_ratio[0] * VOLT_FULL;
	val = (val / auxadc_chan->r_ratio[1]) >> auxadc_chan->res;

	return val;
}

unsigned char *auxadc_get_r_ratio(int channel)
{
	const struct auxadc_channels *auxadc_chan = &auxadc_chans[channel];

	return (unsigned char *)auxadc_chan->r_ratio;
}

#define AUXADC_MAP(_adc_channel_label)				\
	{							\
		.adc_channel_label = _adc_channel_label,	\
		.consumer_dev_name = "mt635x-auxadc",		\
		.consumer_channel = "AUXADC_"_adc_channel_label,\
	}

/* for consumer drivers */
static struct iio_map mt635x_auxadc_default_maps[] = {
	AUXADC_MAP("BATADC"),
	AUXADC_MAP("ISENSE"),
	AUXADC_MAP("VCDT"),
	AUXADC_MAP("BAT_TEMP"),
	AUXADC_MAP("BATID"),
	AUXADC_MAP("CHIP_TEMP"),
	AUXADC_MAP("VCORE_TEMP"),
	AUXADC_MAP("VPROC_TEMP"),
	AUXADC_MAP("VGPU_TEMP"),
	AUXADC_MAP("ACCDET"),
	AUXADC_MAP("VDCXO"),
	AUXADC_MAP("TSX_TEMP"),
	AUXADC_MAP("HPOFS_CAL"),
	AUXADC_MAP("DCXO_TEMP"),
	AUXADC_MAP("VBIF"),
	{},
};

static void auxadc_reset(struct mt635x_auxadc_device *adc_dev)
{
	int i;

	for (i = 0; i < adc_dev->num_rst_setting; i++) {
		regmap_update_bits(adc_dev->regmap,
				   adc_dev->rst_setting[i][0],
				   adc_dev->rst_setting[i][1],
				   adc_dev->rst_setting[i][2]);
	}
	dev_notice(adc_dev->dev, "reset AUXADC done\n");
}

static void auxadc_timeout_handler(struct mt635x_auxadc_device *adc_dev,
			    bool is_timeout, unsigned char ch_num)
{
	int i, ret;
	unsigned char reg_log[631] = "", reg_str[21] = "";
	unsigned int reg_val = 0;
	static unsigned short timeout_times;

	if (is_timeout == false) {
		timeout_times = 0;
		return;
	}
	timeout_times++;
	for (i = 0; i < adc_dev->num_dbg_regs; i++) {
		regmap_read(adc_dev->regmap,
			    adc_dev->dbg_regs[i], &reg_val);
		ret = snprintf(reg_str, 20, "Reg[0x%x]=0x%x,",
			       adc_dev->dbg_regs[i], reg_val);
		if (ret < 0)
			break;
		strncat(reg_log, reg_str, ARRAY_SIZE(reg_log) - 1);
	}
	dev_notice(adc_dev->dev,
		   "(%d)Time out!(%d) %s\n",
		   ch_num, timeout_times, reg_log);
	if (timeout_times == 11)
		auxadc_reset(adc_dev);
}

/*
 * @adc_dev:	pointer to the struct mt635x_auxadc_device
 * @channel:	channel number, refer to the auxadc_chans index
		pass from struct iio_chan_spec.channel
 */
static unsigned short get_auxadc_out(struct mt635x_auxadc_device *adc_dev,
				     int channel)
{
	unsigned int count = 0;
	unsigned int auxadc_out = 0;
	bool is_timeout = false;
	const struct auxadc_channels *auxadc_chan;

	auxadc_chan = &auxadc_chans[channel];
	if (auxadc_chan->convert_fn)
		auxadc_chan->convert_fn(1);

	regmap_write(adc_dev->regmap,
		     auxadc_chan->regs->rqst_reg,
		     BIT(auxadc_chan->regs->rqst_shift));
	udelay(auxadc_chan->avg_num * AUXADC_AVG_TIME_US);

	regmap_read(adc_dev->regmap,
		    auxadc_chan->regs->out_reg, &auxadc_out);
	while (((auxadc_out >> AUXADC_RDY_SHIFT) & 0x1) != 1) {
		usleep_range(100, 200);
		if ((count++) > MAX_TIMEOUT_COUNT) {
			is_timeout = true;
			break;
		}
		regmap_read(adc_dev->regmap,
			    auxadc_chan->regs->out_reg, &auxadc_out);
	}
	auxadc_out &= BIT(auxadc_chan->res) - 1;
	auxadc_timeout_handler(adc_dev, is_timeout, auxadc_chan->ch_num);

	if (auxadc_chan->convert_fn)
		auxadc_chan->convert_fn(0);

	return auxadc_out;
}

static int mt635x_auxadc_read_raw(struct iio_dev *indio_dev,
				  struct iio_chan_spec const *chan,
				  int *val,
				  int *val2,
				  long mask)
{
	struct mt635x_auxadc_device *adc_dev = iio_priv(indio_dev);
	const struct auxadc_channels *auxadc_chan;
	unsigned short auxadc_out;
	int ret;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 5);

	mutex_lock(&adc_dev->lock);
	pm_stay_awake(adc_dev->dev);

	auxadc_out = get_auxadc_out(adc_dev, chan->channel);

	pm_relax(adc_dev->dev);
	mutex_unlock(&adc_dev->lock);

	auxadc_chan = &auxadc_chans[chan->channel];
	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		*val = (int)(auxadc_out * auxadc_chan->r_ratio[0] * VOLT_FULL);
		*val = (*val / auxadc_chan->r_ratio[1]) >> auxadc_chan->res;
		if (auxadc_chan->cali_fn)
			*val = auxadc_chan->cali_fn(*val, 10);
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_RAW:
		*val = (int)auxadc_out;
		ret = IIO_VAL_INT;
		break;
	default:
		return -EINVAL;
	}
	if (__ratelimit(&ratelimit)) {
		dev_info(adc_dev->dev,
			"name:%s, channel=%d, adc_out=0x%x, adc_result=%d\n",
			auxadc_chan->ch_name, auxadc_chan->ch_num,
			auxadc_out, *val);
	}
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
	.driver_module = THIS_MODULE,
	.read_raw = &mt635x_auxadc_read_raw,
	.of_xlate = &mt635x_auxadc_of_xlate,
};

static int auxadc_get_data_from_dt(struct mt635x_auxadc_device *adc_dev,
	unsigned int *channel,
	struct device_node *node)
{
	struct auxadc_channels *auxadc_chan;
	unsigned int value = 0, val_arr[2] = {0};
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
	const struct auxadc_channels *auxadc_chan;
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
		auxadc_chans[channel].regs = &adc_dev->auxadc_regs_tbl[channel];
		auxadc_chan = &auxadc_chans[channel];

		iio_chan->channel = channel;
		iio_chan->datasheet_name = auxadc_chan->ch_name;
		iio_chan->extend_name = auxadc_chan->ch_name;
		iio_chan->info_mask_separate = auxadc_chan->info_mask;
		iio_chan->type = auxadc_chan->type;
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
	struct mt6358_chip *chip;
	int ret;

	chip = dev_get_drvdata(pdev->dev.parent);
	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*adc_dev));
	if (!indio_dev)
		return -ENOMEM;

	adc_dev = iio_priv(indio_dev);
	adc_dev->regmap = chip->regmap;
	adc_dev->dev = &pdev->dev;
	mutex_init(&adc_dev->lock);
	device_init_wakeup(&pdev->dev, true);
	adc_dev->chip_id = (uintptr_t)of_device_get_match_data(&pdev->dev);

	switch (adc_dev->chip_id) {
	case 0x6357:
		adc_dev->auxadc_regs_tbl = mt6357_auxadc_regs_tbl;
		adc_dev->dbg_regs = mt6357_dbg_regs;
		adc_dev->num_dbg_regs = ARRAY_SIZE(mt6357_dbg_regs);
		adc_dev->rst_setting = mt6357_rst_setting;
		adc_dev->num_rst_setting = ARRAY_SIZE(mt6357_rst_setting);
		break;
	case 0x6358:
		adc_dev->auxadc_regs_tbl = mt6358_auxadc_regs_tbl;
		adc_dev->dbg_regs = mt6358_dbg_regs;
		adc_dev->num_dbg_regs = ARRAY_SIZE(mt6358_dbg_regs);
		adc_dev->rst_setting = mt6358_rst_setting;
		adc_dev->num_rst_setting = ARRAY_SIZE(mt6358_rst_setting);
		break;
	case 0x6359:
		adc_dev->auxadc_regs_tbl = mt6359_auxadc_regs_tbl;
		adc_dev->dbg_regs = mt6359_dbg_regs;
		adc_dev->num_dbg_regs = ARRAY_SIZE(mt6359_dbg_regs);
		adc_dev->rst_setting = mt6359_rst_setting;
		adc_dev->num_rst_setting = ARRAY_SIZE(mt6359_rst_setting);
		break;
	default:
		break;
	}

	ret = auxadc_parse_dt(adc_dev, node);
	if (ret < 0) {
		dev_notice(&pdev->dev,
			"auxadc_parse_dt fail, ret=%d\n", ret);
		return ret;
	}

	indio_dev->dev.parent = &pdev->dev;
	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->info = &mt635x_auxadc_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = adc_dev->iio_chans;
	indio_dev->num_channels = adc_dev->nchannels;

	ret = iio_map_array_register(indio_dev, mt635x_auxadc_default_maps);
	if (ret) {
		dev_notice(&pdev->dev, "failed to register iio map:%d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, indio_dev);

	ret = iio_device_register(indio_dev);
	if (ret < 0) {
		dev_notice(&pdev->dev, "failed to register iio device!\n");
		iio_map_array_unregister(indio_dev);
		return ret;
	}
	dev_info(&pdev->dev, "%s done\n", __func__);

	ret = pmic_auxadc_chip_init(&pdev->dev);
	if (ret < 0) {
		dev_notice(&pdev->dev,
			"pmic_auxadc_chip_init fail, ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int mt635x_auxadc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);

	iio_device_unregister(indio_dev);
	iio_map_array_unregister(indio_dev);

	return 0;
}

static const struct of_device_id mt635x_auxadc_of_match[] = {
	{
		.compatible = "mediatek,mt6357-auxadc",
		.data = (void *)0x6357,
	}, {
		.compatible = "mediatek,mt6358-auxadc",
		.data = (void *)0x6358,
	}, {
		.compatible = "mediatek,mt6359-auxadc",
		.data = (void *)0x6359,
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
	.remove	= mt635x_auxadc_remove,
};
#if 1 /* internal version */
static int __init mt635x_auxadc_driver_init(void)
{
	return platform_driver_register(&mt635x_auxadc_driver);
}
fs_initcall(mt635x_auxadc_driver_init);
#else
module_platform_driver(mt635x_auxadc_driver);
#endif

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MediaTek PMIC AUXADC Driver for MT635x PMIC");
