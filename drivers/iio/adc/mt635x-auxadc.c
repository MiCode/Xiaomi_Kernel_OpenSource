// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iio/iio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/mt6357/registers.h>
#include <linux/mfd/mt6358/registers.h>
#include <linux/mfd/mt6359p/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/syscore_ops.h>

#include <dt-bindings/iio/mt635x-auxadc.h>
//#include <aee.h>

#define AUXADC_DEBUG			1
#define AUXADC_RDY_BIT			BIT(15)

#define AUXADC_DEF_R_RATIO		1
#define AUXADC_DEF_AVG_NUM		8

#define AUXADC_AVG_TIME_US		10
#define AUXADC_POLL_DELAY_US		100
#define AUXADC_TIMEOUT_US		32000
#define VOLT_FULL			1800
#define IMP_VOLT_FULL			18000
#define IMP_POLL_DELAY_US		1000
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
#if AUXADC_DEBUG
	void (*convert_fn)(struct mt635x_auxadc_device *adc_dev,
			   unsigned char convert);
	int (*cali_fn)(struct mt635x_auxadc_device *adc_dev,
		       int val, int precision_factor);
#endif
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

static const struct auxadc_regs mt6359p_auxadc_regs_tbl[] = {
	MT635x_AUXADC_REG(BATADC, MT6359P, AUXADC_RQST0, 0, AUXADC_ADC0),
	MT635x_AUXADC_REG(BAT_TEMP, MT6359P, AUXADC_RQST0, 3, AUXADC_ADC3),
	MT635x_AUXADC_REG(CHIP_TEMP, MT6359P, AUXADC_RQST0, 4, AUXADC_ADC4),
	MT635x_AUXADC_REG(VCORE_TEMP, MT6359P, AUXADC_RQST1, 8, AUXADC_ADC38),
	MT635x_AUXADC_REG(VPROC_TEMP, MT6359P, AUXADC_RQST1, 9, AUXADC_ADC39),
	MT635x_AUXADC_REG(VGPU_TEMP, MT6359P, AUXADC_RQST1, 10, AUXADC_ADC40),
	MT635x_AUXADC_REG(ACCDET, MT6359P, AUXADC_RQST0, 5, AUXADC_ADC5),
	MT635x_AUXADC_REG(VDCXO, MT6359P, AUXADC_RQST0, 6, AUXADC_ADC6),
	MT635x_AUXADC_REG(TSX_TEMP, MT6359P, AUXADC_RQST0, 7, AUXADC_ADC7),
	MT635x_AUXADC_REG(HPOFS_CAL, MT6359P, AUXADC_RQST0, 9, AUXADC_ADC9),
	MT635x_AUXADC_REG(DCXO_TEMP, MT6359P, AUXADC_RQST0, 10, AUXADC_ADC10),
	MT635x_AUXADC_REG(VBIF, MT6359P, AUXADC_RQST0, 11, AUXADC_ADC11),
};

static const unsigned int mt6357_en_isink_setting[][3] = {
	{
		MT6357_DRIVER_ANA_CON0, 0xC000, 0xC000,
	}, {
		MT6357_ISINK_EN_CTRL_SMPL, 0xC00, 0xC00,
	}, {
		MT6357_ISINK_EN_CTRL_SMPL, 0xC, 0xC,
	}
};

static const unsigned int mt6357_dis_isink_setting[][3] = {
	{
		MT6357_ISINK_EN_CTRL_SMPL, 0xC, 0x0,
	}, {
		MT6357_ISINK_EN_CTRL_SMPL, 0xC00, 0x0,
	}
};

static const unsigned int mt6358_en_isink_setting[][3] = {
	{
		MT6358_ISINK0_CON1, 0x7000, 0x7000,
	}, {
		MT6358_ISINK1_CON1, 0x7000, 0x7000,
	}, {
		MT6358_ISINK_EN_CTRL_SMPL, 0x300, 0x300,
	}, {
		MT6358_ISINK_EN_CTRL_SMPL, 0x3, 0x3,
	}

};

static const unsigned int mt6358_dis_isink_setting[][3] = {
	{
		MT6358_ISINK_EN_CTRL_SMPL, 0x3, 0x0,
	}, {
		MT6358_ISINK_EN_CTRL_SMPL, 0x300, 0x0,
	}
};

static const unsigned int mt6359p_en_isink_setting[][3] = {
	{
		MT6359P_ISINK0_CON1, 0x7000, 0x7000,
	}, {
		MT6359P_ISINK1_CON1, 0x7000, 0x7000,
	}, {
		MT6359P_ISINK_EN_CTRL_SMPL, 0x300, 0x300,
	}, {
		MT6359P_ISINK_EN_CTRL_SMPL, 0x3, 0x3,
	}
};

static const unsigned int mt6359p_dis_isink_setting[][3] = {
	{
		MT6359P_ISINK_EN_CTRL_SMPL, 0x3, 0x0,
	}, {
		MT6359P_ISINK_EN_CTRL_SMPL, 0x300, 0x0,
	}
};

#if AUXADC_DEBUG
static const unsigned int mt6357_dbg_regs[] = {
	MT6357_AUXADC_STA0, MT6357_AUXADC_STA1, MT6357_AUXADC_STA2,
	MT6357_STRUP_CON6, MT6357_HK_TOP_RST_CON0,
	MT6357_HK_TOP_CLK_CON0, MT6357_HK_TOP_CLK_CON1,
	MT6357_AUXADC_IMP_CG0, MT6357_AUXADC_IMP0, MT6357_AUXADC_IMP1,
};

static const unsigned int mt6358_dbg_regs[] = {
	MT6358_AUXADC_STA0, MT6358_AUXADC_STA1, MT6358_AUXADC_STA2,
	MT6358_STRUP_CON6, MT6358_HK_TOP_RST_CON0,
	MT6358_HK_TOP_CLK_CON0, MT6358_HK_TOP_CLK_CON1,
	MT6358_AUXADC_CON20, /* check DATA_REUSE */
};

static const unsigned int mt6359p_dbg_regs[] = {
	MT6359P_AUXADC_STA0, MT6359P_AUXADC_STA1, MT6359P_AUXADC_STA2,
	MT6359P_AUXADC_CON5, MT6359P_AUXADC_CON9, MT6359P_AUXADC_CON21,
	MT6359P_HK_TOP_LDO_STATUS,
	MT6359P_AUXADC_NAG_9, MT6359P_AUXADC_NAG_10, MT6359P_AUXADC_NAG_11,
	MT6359P_AUXADC_IMP3, MT6359P_AUXADC_IMP4, MT6359P_AUXADC_IMP5,
	MT6359P_AUXADC_LBAT6, MT6359P_AUXADC_LBAT7, MT6359P_AUXADC_LBAT8,
	MT6359P_AUXADC_BAT_TEMP_7, MT6359P_AUXADC_BAT_TEMP_8,
	MT6359P_AUXADC_BAT_TEMP_9,
	MT6359P_AUXADC_LBAT2_6, MT6359P_AUXADC_LBAT2_7, MT6359P_AUXADC_LBAT2_8,
	MT6359P_AUXADC_MDRT_3, MT6359P_AUXADC_MDRT_4, MT6359P_AUXADC_MDRT_5,
	MT6359P_HK_TOP_STRUP, MT6359P_HK_TOP_RST_CON0,
	MT6359P_HK_TOP_CLK_CON0, MT6359P_HK_TOP_CLK_CON1,
	MT6359P_AUXADC_CON20, /* check DATA_REUSE */
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

static const unsigned int mt6359p_rst_setting[][3] = {
	{
		MT6359P_HK_TOP_WKEY, 0xFFFF, 0x6359,
	}, {
		MT6359P_HK_TOP_RST_CON0, 0x9, 0x9,
	}, {
		MT6359P_HK_TOP_RST_CON0, 0x9, 0,
	}, {
		MT6359P_HK_TOP_WKEY, 0xFFFF, 0,
	}, {
		MT6359P_AUXADC_RQST0, 0x80, 0x80,
	}, {
		MT6359P_AUXADC_RQST1, 0x40, 0x40,
	}
};
#endif

struct auxadc_info {
	const struct auxadc_regs *regs_tbl;
	const unsigned int (*en_isink_setting)[3];
	unsigned int num_en_isink_setting;
	const unsigned int (*dis_isink_setting)[3];
	unsigned int num_dis_isink_setting;
	int (*imp_conv)(struct mt635x_auxadc_device *adc_dev,
			int *vbat, int *ibat);
	void (*imp_stop)(struct mt635x_auxadc_device *adc_dev);
#if AUXADC_DEBUG
	const unsigned int *dbg_regs;
	unsigned int num_dbg_regs;
	const unsigned int (*rst_setting)[3];
	unsigned int num_rst_setting;
#endif
};

#if AUXADC_DEBUG
/* Put debug PURPOSE and WK setting hear */
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

static void auxadc_debug_dump(struct mt635x_auxadc_device *adc_dev,
			      int timeout_times)
{
	int i = 0, len = 0;
	unsigned char reg_log[631] = "", reg_str[21] = "";
	unsigned int reg_val = 0;

	for (i = 0; i < adc_dev->info->num_dbg_regs; i++) {
		regmap_read(adc_dev->regmap,
			    adc_dev->info->dbg_regs[i], &reg_val);
		len += snprintf(reg_str, 20, "Reg[0x%x]=0x%x,",
				adc_dev->info->dbg_regs[i], reg_val);
		strncat(reg_log, reg_str, ARRAY_SIZE(reg_log) - 1);
	}
	if (len)
		dev_notice(adc_dev->dev,
			   "(%s)Time out!(%d) %s\n"
			   , __func__, timeout_times, reg_log);
}

static void imp_timeout_handler(struct mt635x_auxadc_device *adc_dev,
				bool is_timeout)
{
	static unsigned short timeout_times;

	if (is_timeout == false) {
		timeout_times = 0;
		return;
	}
	timeout_times++;
	auxadc_debug_dump(adc_dev, timeout_times);
	if (timeout_times == 5)
		auxadc_reset(adc_dev);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	//TODO
	//else if (timeout_times > 5)
	//	aee_kernel_warning("PTIM timeout", "PTIM");
#endif
}

static void auxadc_timeout_handler(struct mt635x_auxadc_device *adc_dev,
				   bool is_timeout)
{
	static unsigned short timeout_times;

	if (is_timeout == false) {
		timeout_times = 0;
		return;
	}
	timeout_times++;
	auxadc_debug_dump(adc_dev, timeout_times);
	if (timeout_times == 11)
		auxadc_reset(adc_dev);
}
#endif

static inline int auxadc_conv_imp_vbat(struct mt635x_auxadc_device *adc_dev)
{
	int vbat;

	vbat = adc_dev->imp_vbat;
	vbat = vbat * auxadc_chans[AUXADC_IMP].r_ratio[0] * IMP_VOLT_FULL;
	vbat = (vbat / auxadc_chans[AUXADC_IMP].r_ratio[1]) >>
		auxadc_chans[AUXADC_IMP].res;
	return vbat;
}

static struct power_supply *get_mtk_gauge_psy(void)
{
	static struct power_supply *psy;
	union power_supply_propval prop;
	int ret;

	if (!psy) {
		psy = power_supply_get_by_name("mtk-gauge");
		if (!psy) {
			pr_info("%s psy is not rdy\n", __func__);
			return NULL;
		}
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&prop);
	if (!ret && prop.intval == 0)
		return psy; /* gauge enabled */
	return NULL;
}

static int auxadc_get_imp_ibat(struct mt635x_auxadc_device *adc_dev)
{
	struct power_supply *psy;
	union power_supply_propval prop;
	int ret;

	psy = get_mtk_gauge_psy();
	/* gauge disabled */
	if (!psy)
		return 0;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW,
					&prop);
	if (!ret)
		return prop.intval;
	return 0;
}

/* Using non-sleep delay time for polling IRQ status before suspend */
#define auxadc_imp_poll_timeout(map, addr, val, cond, delay_us, timeout_us) \
({ \
	int __ret = 0, __cnt = 0; \
	int __max_cnt = (timeout_us) / (delay_us); \
	for (;;) { \
		__ret = regmap_read((map), (addr), &(val)); \
		if (__ret) \
			break; \
		if (cond) \
			break; \
		if ((__cnt++) > __max_cnt) { \
			pr_info("IMP Time out!\n"); \
			__ret = -ETIMEDOUT; \
			break; \
		} \
		udelay(delay_us); \
	} \
	__ret ?: 0; \
})

#define MT6357_IMP_CK_SW_MODE_MASK	BIT(0)
#define MT6357_IMP_CK_SW_EN_MASK	BIT(1)
#define MT6357_IMP_AUTORPT_EN_MASK	BIT(15)
#define MT6357_IMP_CLR_MASK		(BIT(14) | BIT(7))
#define MT6357_IMP_IRQ_RDY_BIT		BIT(8)

static int mt6357_imp_conv(struct mt635x_auxadc_device *adc_dev,
			   int *vbat, int *ibat)
{
	int ret, val = 0;
	bool is_timeout = false;

	/* start conversion */
	regmap_update_bits(adc_dev->regmap, MT6357_AUXADC_IMP_CG0,
			   MT6357_IMP_CK_SW_MODE_MASK,
			   MT6357_IMP_CK_SW_MODE_MASK);
	regmap_update_bits(adc_dev->regmap, MT6357_AUXADC_IMP_CG0,
			   MT6357_IMP_CK_SW_EN_MASK, MT6357_IMP_CK_SW_EN_MASK);
	regmap_update_bits(adc_dev->regmap, MT6357_AUXADC_IMP1,
			   MT6357_IMP_AUTORPT_EN_MASK,
			   MT6357_IMP_AUTORPT_EN_MASK);
	/* polling IRQ status */
	ret = auxadc_imp_poll_timeout(adc_dev->regmap,
				      MT6357_AUXADC_IMP0,
				      val,
				      (val & MT6357_IMP_IRQ_RDY_BIT),
				      IMP_POLL_DELAY_US,
				      AUXADC_TIMEOUT_US);
	if (ret == -ETIMEDOUT)
		is_timeout = true;
#if AUXADC_DEBUG
	imp_timeout_handler(adc_dev, is_timeout);
#endif
	/* stop conversion */
	adc_dev->info->imp_stop(adc_dev);
	/* Get vbat/ibat */
	*vbat = adc_dev->imp_vbat;
	*ibat = auxadc_get_imp_ibat(adc_dev);

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
#define MT6358_IMP_IRQ_RDY_BIT		BIT(8)

static int mt6358_imp_conv(struct mt635x_auxadc_device *adc_dev,
			   int *vbat, int *ibat)
{
	int ret, val = 0;
	bool is_timeout = false;

	/* start conversion */
	regmap_update_bits(adc_dev->regmap, MT6358_AUXADC_DCM_CON,
			   MT6358_IMP_CK_SW_MASK, MT6358_IMP_CK_SW_MASK);
	regmap_update_bits(adc_dev->regmap, MT6358_AUXADC_IMP1,
			   MT6358_IMP_AUTORPT_EN_MASK,
			   MT6358_IMP_AUTORPT_EN_MASK);
	/* polling IRQ status */
	ret = auxadc_imp_poll_timeout(adc_dev->regmap,
				      MT6358_AUXADC_IMP0,
				      val,
				      (val & MT6358_IMP_IRQ_RDY_BIT),
				      IMP_POLL_DELAY_US,
				      AUXADC_TIMEOUT_US);
	if (ret == -ETIMEDOUT)
		is_timeout = true;
#if AUXADC_DEBUG
	imp_timeout_handler(adc_dev, is_timeout);
#endif
	/* stop conversion */
	adc_dev->info->imp_stop(adc_dev);
	/* Get vbat/ibat */
	*vbat = adc_dev->imp_vbat;
	*ibat = auxadc_get_imp_ibat(adc_dev);

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

#define MT6359P_IMP_IRQ_RDY_BIT		BIT(15)

static int mt6359p_imp_conv(struct mt635x_auxadc_device *adc_dev, int *vbat, int *ibat)
{
	int ret, val = 0;
	bool is_timeout = false;

	/* start conversion */
	regmap_write(adc_dev->regmap, MT6359P_AUXADC_IMP0, 1);

	/* polling IRQ status */
	ret = auxadc_imp_poll_timeout(adc_dev->regmap,
				      MT6359P_AUXADC_IMP1,
				      val,
				      (val & MT6359P_IMP_IRQ_RDY_BIT),
				      IMP_POLL_DELAY_US,
				      AUXADC_TIMEOUT_US);
	if (ret == -ETIMEDOUT)
		is_timeout = true;
#if AUXADC_DEBUG
	imp_timeout_handler(adc_dev, is_timeout);
#endif
	/* stop conversion */
	adc_dev->info->imp_stop(adc_dev);
	/* Get vbat/ibat */
	*vbat = adc_dev->imp_vbat;
	*ibat = auxadc_get_imp_ibat(adc_dev);

	return ret;
}

static void mt6359p_imp_stop(struct mt635x_auxadc_device *adc_dev)
{
	/* stop conversion */
	regmap_write(adc_dev->regmap, MT6359P_AUXADC_IMP0, 0);
	udelay(IMP_STOP_DELAY_US);
	regmap_read(adc_dev->regmap, MT6359P_AUXADC_IMP3, &adc_dev->imp_vbat);
	adc_dev->imp_vbat &= BIT(auxadc_chans[AUXADC_IMP].res) - 1;
}

static const struct auxadc_info mt6357_info = {
	.regs_tbl = mt6357_auxadc_regs_tbl,
	.en_isink_setting = mt6357_en_isink_setting,
	.num_en_isink_setting = ARRAY_SIZE(mt6357_en_isink_setting),
	.dis_isink_setting = mt6357_dis_isink_setting,
	.num_dis_isink_setting = ARRAY_SIZE(mt6357_dis_isink_setting),
	.imp_conv = mt6357_imp_conv,
	.imp_stop = mt6357_imp_stop,
#if AUXADC_DEBUG
	.dbg_regs = mt6357_dbg_regs,
	.num_dbg_regs = ARRAY_SIZE(mt6357_dbg_regs),
	.rst_setting = mt6357_rst_setting,
	.num_rst_setting = ARRAY_SIZE(mt6357_rst_setting),
#endif
};

static const struct auxadc_info mt6358_info = {
	.regs_tbl = mt6358_auxadc_regs_tbl,
	.en_isink_setting = mt6358_en_isink_setting,
	.num_en_isink_setting = ARRAY_SIZE(mt6358_en_isink_setting),
	.dis_isink_setting = mt6358_dis_isink_setting,
	.num_dis_isink_setting = ARRAY_SIZE(mt6358_dis_isink_setting),
	.imp_conv = mt6358_imp_conv,
	.imp_stop = mt6358_imp_stop,
#if AUXADC_DEBUG
	.dbg_regs = mt6358_dbg_regs,
	.num_dbg_regs = ARRAY_SIZE(mt6358_dbg_regs),
	.rst_setting = mt6358_rst_setting,
	.num_rst_setting = ARRAY_SIZE(mt6358_rst_setting),
#endif
};

static const struct auxadc_info mt6359p_info = {
	.regs_tbl = mt6359p_auxadc_regs_tbl,
	.en_isink_setting = mt6359p_en_isink_setting,
	.num_en_isink_setting = ARRAY_SIZE(mt6359p_en_isink_setting),
	.dis_isink_setting = mt6359p_dis_isink_setting,
	.num_dis_isink_setting = ARRAY_SIZE(mt6359p_dis_isink_setting),
	.imp_conv = mt6359p_imp_conv,
	.imp_stop = mt6359p_imp_stop,
#if AUXADC_DEBUG
	.dbg_regs = mt6359p_dbg_regs,
	.num_dbg_regs = ARRAY_SIZE(mt6359p_dbg_regs),
	.rst_setting = mt6359p_rst_setting,
	.num_rst_setting = ARRAY_SIZE(mt6359p_rst_setting),
#endif
};


/*
 * imix_r cali before entering suspend
 */
static void enable_dummy_load(struct mt635x_auxadc_device *adc_dev)
{
	int i = 0;

	for (i = 0; i < adc_dev->info->num_en_isink_setting; i++) {
		regmap_update_bits(adc_dev->regmap,
				   adc_dev->info->en_isink_setting[i][0],
				   adc_dev->info->en_isink_setting[i][1],
				   adc_dev->info->en_isink_setting[i][2]);
	}
}

static void disable_dummy_load(struct mt635x_auxadc_device *adc_dev)
{
	int i = 0;

	for (i = 0; i < adc_dev->info->num_dis_isink_setting; i++) {
		regmap_update_bits(adc_dev->regmap,
				   adc_dev->info->dis_isink_setting[i][0],
				   adc_dev->info->dis_isink_setting[i][1],
				   adc_dev->info->dis_isink_setting[i][2]);
	}
}

static int auxadc_get_rac(struct mt635x_auxadc_device *adc_dev)
{
	int vbat_1 = 0, vbat_2 = 0;
	int ibat_1 = 0, ibat_2 = 0;
	int rac = 0, ret = 0;
	int retry_count = 0;

	do {
		/* Trigger ADC PTIM mode to get VBAT and current */
		if (adc_dev->info->imp_conv)
			ret = adc_dev->info->imp_conv(adc_dev,
						      &vbat_1, &ibat_1);
		/* Convert imp vbat raw data */
		vbat_1 = auxadc_conv_imp_vbat(adc_dev);
		enable_dummy_load(adc_dev);
		mdelay(50);

		/* Trigger ADC PTIM mode again to get new VBAT and current */
		if (adc_dev->info->imp_conv)
			ret = adc_dev->info->imp_conv(adc_dev,
						      &vbat_2, &ibat_2);
		/* Convert imp vbat raw data */
		vbat_2 = auxadc_conv_imp_vbat(adc_dev);
		disable_dummy_load(adc_dev);

		/* Cal.Rac: 70mA <= c_diff <= 120mA, 4mV <= v_diff <= 200mV */
		if ((ibat_2 - ibat_1) >= 700 && (ibat_2 - ibat_1) <= 1200 &&
		    (vbat_1 - vbat_2) >= 40 && (vbat_1 - vbat_2) <= 2000) {
			/*m-ohm */
			rac = ((vbat_1 - vbat_2) * 1000) / (ibat_2 - ibat_1);
			if (rac < 0)
				ret = (rac - (rac * 2)) * 1;
			else
				ret = rac * 1;
			if (ret < 50) {
				ret = -1;
				pr_info("bypass due to Rac < 50mOhm\n");
			}
		} else {
			ret = -1;
			pr_info("bypass due to c_diff < 70mA\n");
		}
		pr_info("v1=%d,v2=%d,c1=%d,c2=%d,v_diff=%d,c_diff=%d\n",
			vbat_1, vbat_2, ibat_1, ibat_2,
			(vbat_1 - vbat_2), (ibat_2 - ibat_1));
		pr_info("rac=%d,ret=%d,retry=%d\n",
			rac, ret, retry_count);

		if (++retry_count >= 3)
			break;
	} while (ret == -1);

	return ret;
}

static int auxadc_get_uisoc(void)
{
	struct power_supply *psy;
	union power_supply_propval prop;
	int ret;

	psy = power_supply_get_by_name("battery");
	if (!psy)
		return -ENODEV;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY,
					&prop);
	if (ret || prop.intval < 0)
		return -EINVAL;
	else
		return prop.intval;
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
	bool is_timeout = false;

#if AUXADC_DEBUG
	if (auxadc_chan->convert_fn)
		auxadc_chan->convert_fn(adc_dev, 1);
#endif
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
	if (ret == -ETIMEDOUT) {
		dev_info(adc_dev->dev, "(%d)Time out!\n", auxadc_chan->ch_num);
		is_timeout = true;
	}
#if AUXADC_DEBUG
	auxadc_timeout_handler(adc_dev, is_timeout);
	if (auxadc_chan->convert_fn)
		auxadc_chan->convert_fn(adc_dev, 0);
#endif

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
#if AUXADC_DEBUG
		if (auxadc_chan->cali_fn)
			*val = auxadc_chan->cali_fn(adc_dev, *val, 10);
#endif
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

int auxadc_priv_read_channel(struct mt635x_auxadc_device *adc_dev, int channel)
{
	const struct auxadc_channels *auxadc_chan;
	int val = 0, ret = 0;

	auxadc_chan = &auxadc_chans[channel];

	ret = get_auxadc_out(adc_dev, auxadc_chan, &val);
	if (ret < 0)
		pr_info("%s ret=%d\n", __func__, ret);
	val = val * auxadc_chan->r_ratio[0] * VOLT_FULL;
	val = (val / auxadc_chan->r_ratio[1]) >> auxadc_chan->res;

	return val;
}

#if AUXADC_DEBUG
#define MT6357_DCXO_CH4_APMUX_SEL	BIT(4)
static void mt6357_dcxo_temp_conv(struct mt635x_auxadc_device *adc_dev,
				  unsigned char convert)
{
	if (convert == 1)
		regmap_update_bits(adc_dev->regmap,
				   MT6357_AUXADC_DCXO_MDRT_2,
				   MT6357_DCXO_CH4_APMUX_SEL,
				   MT6357_DCXO_CH4_APMUX_SEL);
	else if (convert == 0)
		regmap_update_bits(adc_dev->regmap,
				   MT6357_AUXADC_DCXO_MDRT_2,
				   MT6357_DCXO_CH4_APMUX_SEL, 0);
}

#define MT6357_BATON_TDET_EN		BIT(1)
static void mt6357_vbif_conv(struct mt635x_auxadc_device *adc_dev,
			     unsigned char convert)
{
	if (convert == 1)
		regmap_update_bits(adc_dev->regmap,
				   MT6357_AUXADC_CHR_TOP_CON2,
				   MT6357_BATON_TDET_EN, 0);
	else if (convert == 0)
		regmap_update_bits(adc_dev->regmap,
				   MT6357_AUXADC_CHR_TOP_CON2,
				   MT6357_BATON_TDET_EN, MT6357_BATON_TDET_EN);
}

#define MT6358_VDCXO_SWITCH		BIT(7)
static void mt6358_vdcxo_conv(struct mt635x_auxadc_device *adc_dev,
			      unsigned char convert)
{
	/* Turn on CH6 measured switch, set AUXADC_ANA_CON0[7] = 1’b1 */
	regmap_update_bits(adc_dev->regmap, MT6358_AUXADC_ANA_CON0,
			   MT6358_VDCXO_SWITCH, convert ? MT6358_VDCXO_SWITCH : 0);
}

#define MT6358_BATON_TDET_EN		BIT(1)
static void mt6358_vbif_conv(struct mt635x_auxadc_device *adc_dev,
			     unsigned char convert)
{
	regmap_update_bits(adc_dev->regmap, MT6358_HK_TOP_CHR_CON,
			   MT6358_BATON_TDET_EN, convert ? 0 : MT6358_BATON_TDET_EN);
}

static unsigned int g_DEGC;
static unsigned int g_O_VTS;
static unsigned int g_O_SLOPE_SIGN;
static unsigned int g_O_SLOPE;
static unsigned int g_CALI_FROM_EFUSE_EN;
static unsigned int g_GAIN_AUX;
static unsigned int g_SIGN_AUX;
static unsigned int g_GAIN_BGRL;
static unsigned int g_SIGN_BGRL;
static unsigned int g_TEMP_L_CALI;
static unsigned int g_GAIN_BGRH;
static unsigned int g_SIGN_BGRH;
static unsigned int g_TEMP_H_CALI;
static unsigned int g_AUXCALI_EN;
static unsigned int g_BGRCALI_EN;

static void mt6358_batadc_cali_init(struct mt635x_auxadc_device *adc_dev)
{
	struct nvmem_device *auxadc_efuse;
	int rval = 0, ret;
	unsigned int efuse = 0;
	unsigned int efuse_offset;

	auxadc_efuse = devm_nvmem_device_get(adc_dev->dev, "auxadc_efuse_dev");
	ret = PTR_ERR_OR_ZERO(auxadc_efuse);
	if (ret) {
		dev_info(adc_dev->dev, "Error: Get efuse failed (%d)\n", ret);
		return;
	}

	if (of_property_read_u32(adc_dev->dev->of_node, "cali-efuse-offset", &efuse_offset))
		efuse_offset = 0;

	regmap_read(adc_dev->regmap, MT6358_AUXADC_DIG_3_ELR8, &rval);
	if (rval & BIT(8)) {
		g_DEGC = rval & 0x3F;
		if (g_DEGC < 38 || g_DEGC > 60)
			g_DEGC = 53;
		regmap_read(adc_dev->regmap, MT6358_AUXADC_DIG_3_ELR9, &rval);
		g_O_VTS = rval & 0xFFF;
		regmap_read(adc_dev->regmap, MT6358_AUXADC_DIG_3_ELR10, &rval);
		g_O_SLOPE_SIGN = (rval & 0x100) >> 8;
		g_O_SLOPE = rval & 0x3F;
	} else {
		g_DEGC = 50;
		g_O_VTS = 1600;
	}

	ret = nvmem_device_read(auxadc_efuse, (39 + efuse_offset) * 2, 2, &efuse);
	g_CALI_FROM_EFUSE_EN = (efuse >> 2) & 0x1;
	if (g_CALI_FROM_EFUSE_EN == 1) {
		g_SIGN_AUX = (efuse >> 3) & 0x1;
		g_AUXCALI_EN = (efuse >> 6) & 0x1;
		g_GAIN_AUX = (efuse >> 8) & 0xFF;
	} else {
		g_SIGN_AUX = 0;
		g_AUXCALI_EN = 1;
		g_GAIN_AUX = 106;
	}
	g_SIGN_BGRL = (efuse >> 4) & 0x1;
	g_SIGN_BGRH = (efuse >> 5) & 0x1;
	g_BGRCALI_EN = (efuse >> 7) & 0x1;

	ret = nvmem_device_read(auxadc_efuse, (40 + efuse_offset) * 2, 2, &efuse);
	g_GAIN_BGRL = (efuse >> 9) & 0x7F;
	ret = nvmem_device_read(auxadc_efuse, (41 + efuse_offset) * 2, 2, &efuse);
	g_GAIN_BGRH = (efuse >> 9) & 0x7F;

	ret = nvmem_device_read(auxadc_efuse, (42 + efuse_offset) * 2, 2, &efuse);
	g_TEMP_L_CALI = (efuse >> 10) & 0x7;
	g_TEMP_H_CALI = (efuse >> 13) & 0x7;

	pr_info("%s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", __func__,
		g_DEGC, g_O_VTS, g_O_SLOPE_SIGN, g_O_SLOPE,
		g_CALI_FROM_EFUSE_EN, g_SIGN_AUX, g_SIGN_BGRL, g_SIGN_BGRH,
		g_AUXCALI_EN, g_BGRCALI_EN,
		g_GAIN_AUX, g_GAIN_BGRL, g_GAIN_BGRH,
		g_TEMP_L_CALI, g_TEMP_H_CALI);
}

static int wk_aux_cali(int T_curr, int vbat_out)
{
	signed long long coeff_gain_aux = 0;
	signed long long vbat_cali = 0;

	coeff_gain_aux = (317220 + 11960 * (signed long long)g_GAIN_AUX);
	vbat_cali = div_s64((vbat_out * (T_curr - 250) * coeff_gain_aux), 255);
	vbat_cali = div_s64(vbat_cali, 1000000000);
	if (g_SIGN_AUX == 0)
		vbat_out += vbat_cali;
	else
		vbat_out -= vbat_cali;
	return vbat_out;
}

static int wk_bgr_cali(int T_curr, int vbat_out)
{
	signed long long coeff_gain_bgr = 0;
	signed int T_L = -100 + g_TEMP_L_CALI * 25;
	signed int T_H = 600 + g_TEMP_H_CALI * 25;

	if (T_curr < T_L) {
		coeff_gain_bgr = (127 + 8 * (signed long long)g_GAIN_BGRL);
		if (g_SIGN_BGRL == 0)
			vbat_out += div_s64((vbat_out * (T_curr - T_L) *
					     coeff_gain_bgr), 127000000);
		else
			vbat_out -= div_s64((vbat_out * (T_curr - T_L) *
					     coeff_gain_bgr), 127000000);
	} else if (T_curr > T_H) {
		coeff_gain_bgr = (127 + 8 * (signed long long)g_GAIN_BGRH);
		if (g_SIGN_BGRH == 0)
			vbat_out -= div_s64((vbat_out * (T_curr - T_H) *
					     coeff_gain_bgr), 127000000);
		else
			vbat_out += div_s64((vbat_out * (T_curr - T_H) *
					     coeff_gain_bgr), 127000000);
	}

	return vbat_out;
}

static int mt6358_batadc_cali(struct mt635x_auxadc_device *adc_dev,
			      int vbat_out, int precision_factor)
{
	int mV_diff = 0;
	int T_curr = 0;		/* unit: 0.1 degrees C*/
	int vbat_out_old;	/* vbat_out unit: 0.1mV*/
	int vthr;		/* vthr unit: mV */

	if (!g_DEGC)
		mt6358_batadc_cali_init(adc_dev);
	vthr = auxadc_priv_read_channel(adc_dev, AUXADC_CHIP_TEMP);
	mV_diff = vthr - g_O_VTS * 1800 / 4096;
	if (g_O_SLOPE_SIGN == 0)
		T_curr = mV_diff * 10000 / (signed int)(1681 + g_O_SLOPE * 10);
	else
		T_curr = mV_diff * 10000 / (signed int)(1681 - g_O_SLOPE * 10);
	T_curr = (g_DEGC * 10 / 2) - T_curr;
	/*pr_info("%d\n", T_curr);*/

	if (precision_factor > 1)
		vbat_out *= precision_factor;

	vbat_out_old = vbat_out;

	if (g_AUXCALI_EN == 1)
		vbat_out = wk_aux_cali(T_curr, vbat_out);

	if (g_BGRCALI_EN == 1)
		vbat_out = wk_bgr_cali(T_curr, vbat_out);

	if (abs(vbat_out - vbat_out_old) > 1000) {
		pr_notice("vbat_out_old=%d, vthr=%d, T_curr=%d, vbat_out=%d\n",
			vbat_out_old, vthr, T_curr, vbat_out);
		pr_notice("%d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			g_DEGC, g_O_VTS, g_O_SLOPE_SIGN, g_O_SLOPE,
			g_SIGN_AUX, g_SIGN_BGRL, g_SIGN_BGRH,
			g_AUXCALI_EN, g_BGRCALI_EN,
			g_GAIN_AUX, g_GAIN_BGRL, g_GAIN_BGRH,
			g_TEMP_L_CALI, g_TEMP_H_CALI);
	} else
		pr_info("vbat_out_old=%d, vthr=%d, T_curr=%d, vbat_out=%d\n",
			vbat_out_old, vthr, T_curr, vbat_out);

	if (precision_factor > 1)
		vbat_out = DIV_ROUND_CLOSEST(vbat_out, precision_factor);
	return vbat_out;
}

static int bat_temp_filter(int *arr, unsigned short size)
{
	unsigned char i, i_max, i_min = 0;
	int arr_max = 0, arr_min = arr[0];
	int sum = 0;

	for (i = 0; i < size; i++) {
		sum += arr[i];
		if (arr[i] > arr_max) {
			arr_max = arr[i];
			i_max = i;
		} else if (arr[i] < arr_min) {
			arr_min = arr[i];
			i_min = i;
		}
	}
	sum = sum - arr_max - arr_min;
	return (sum/(size - 2));
}

static int wk_bat_temp_dbg(struct mt635x_auxadc_device *adc_dev,
			   int bat_temp_prev, int bat_temp)
{
	int vbif28, bat_temp_new = bat_temp;
	int arr_bat_temp[5];
	int bat = 0;
	unsigned short i;

	vbif28 = auxadc_priv_read_channel(adc_dev, AUXADC_VBIF);
	pr_notice("BAT_TEMP_PREV:%d,BAT_TEMP:%d,VBIF28:%d\n",
		bat_temp_prev, bat_temp, vbif28);
	if (bat_temp < 200 || abs(bat_temp_prev - bat_temp) > 100) {
		auxadc_debug_dump(adc_dev, 0);
		for (i = 0; i < 5; i++) {
			arr_bat_temp[i] =
				auxadc_priv_read_channel(adc_dev,
							 AUXADC_BAT_TEMP);
		}
		bat_temp_new = bat_temp_filter(arr_bat_temp, 5);
		pr_notice("%d,%d,%d,%d,%d, BAT_TEMP_NEW:%d\n",
			  arr_bat_temp[0], arr_bat_temp[1], arr_bat_temp[2],
			  arr_bat_temp[3], arr_bat_temp[4], bat_temp_new);

		/* Reset AuxADC to observe VBAT/IBAT/BAT_TEMP */
		auxadc_reset(adc_dev);
		for (i = 0; i < 5; i++) {
			bat = auxadc_priv_read_channel(adc_dev,
						       AUXADC_BATADC);
			arr_bat_temp[i] =
				auxadc_priv_read_channel(adc_dev,
							 AUXADC_BAT_TEMP);
			pr_notice("[CH3_DBG] %d,%d\n",
				  bat, arr_bat_temp[i]);
		}
		bat_temp_new = bat_temp_filter(arr_bat_temp, 5);
		pr_notice("Final BAT_TEMP_NEW:%d\n", bat_temp_new);
	}
	return bat_temp_new;
}

static int mt635x_bat_temp_cali(struct mt635x_auxadc_device *adc_dev,
				int bat_temp, int precision_factor)
{
	static int bat_temp_prev;
	static unsigned int dbg_count;
	static unsigned int aee_count;

	if (!adc_dev && bat_temp == -1 && precision_factor == -1) {
		bat_temp_prev = 0;
		return 0;
	}
	if (bat_temp_prev == 0)
		goto out;

	dbg_count++;
	if (bat_temp < 200 || abs(bat_temp_prev - bat_temp) > 100) {
		/* dump debug log when BAT_TEMP being abnormal */
		bat_temp = wk_bat_temp_dbg(adc_dev, bat_temp_prev, bat_temp);
		pr_notice("PMIC AUXADC BAT_TEMP aee_count=%d\n", aee_count);
		aee_count++;
	} else if (dbg_count % 50 == 0) {
		/* dump debug log in normal case */
		wk_bat_temp_dbg(adc_dev, bat_temp_prev, bat_temp);
	}
out:
	bat_temp_prev = bat_temp;
	return bat_temp;
}

void auxadc_set_convert_fn(int channel,
			   void (*convert_fn)(struct mt635x_auxadc_device *,
					      unsigned char convert))
{
	auxadc_chans[channel].convert_fn = convert_fn;
}

void auxadc_set_cali_fn(int channel,
			int (*cali_fn)(struct mt635x_auxadc_device *, int, int))
{
	auxadc_chans[channel].cali_fn = cali_fn;
}
#endif

#define	IMIX_R_MIN_MOHM		100
#define	IMIX_R_CALI_CNT		2
static int auxadc_cali_imix_r(struct mt635x_auxadc_device *dev)
{
	static struct mt635x_auxadc_device *adc_dev;
	static int pre_uisoc = 101;
	int cur_uisoc = auxadc_get_uisoc();
	int i, imix_r_avg = 0, rac_val[IMIX_R_CALI_CNT];

	if (dev) {
		adc_dev = dev;
		return 0;
	} else if (!adc_dev) {
		pr_info("%s NULL adc_dev, skip\n",
			__func__);
		return -EINVAL;
	} else if (!get_mtk_gauge_psy()) {
		pr_info("%s gauge disabled, skip\n",
			__func__);
		return -ENODEV;
	} else if (cur_uisoc < 0 ||
		   cur_uisoc == pre_uisoc) {
		pr_info("%s pre_SOC=%d SOC=%d, skip\n",
			__func__, pre_uisoc, cur_uisoc);
		return 0;
	}
	pre_uisoc = cur_uisoc;
	for (i = 0; i < IMIX_R_CALI_CNT; i++) {
		rac_val[i] = auxadc_get_rac(adc_dev);
		imix_r_avg += rac_val[i];
		if (rac_val[i] < 0)
			return -EIO;
	}
	imix_r_avg = imix_r_avg / IMIX_R_CALI_CNT;
	pr_info("[%s] %d,%d,ravg:%d\n",
		__func__, rac_val[0], rac_val[1], imix_r_avg);

	if (imix_r_avg > IMIX_R_MIN_MOHM)
		adc_dev->imix_r = imix_r_avg;
	return 0;
}

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
	auxadc_cali_imix_r(adc_dev);
	return 0;
}

static int auxadc_suspend_enter(void)
{
	auxadc_cali_imix_r(NULL);
#if AUXADC_DEBUG
	/* Restore bat_temp_prev when entering suspend */
	mt635x_bat_temp_cali(NULL, -1, -1);
#endif
	return 0;
}

static struct syscore_ops auxadc_syscore_ops = {
	.suspend = auxadc_suspend_enter,
};

static int auxadc_get_data_from_dt(struct mt635x_auxadc_device *adc_dev,
				   unsigned int *channel,
				   struct device_node *node)
{
	struct auxadc_channels *auxadc_chan;
	unsigned int value = 0;
	unsigned int val_arr[2] = {0};
	int ret;

	ret = of_property_read_u32(node, "channel", channel);
	if (ret) {
		dev_notice(adc_dev->dev,
			"invalid channel in node:%s\n", node->name);
		return ret;
	}
	if (*channel > AUXADC_CHAN_MAX) {
		dev_notice(adc_dev->dev,
			"invalid channel number %d in node:%s\n",
			*channel, node->name);
		return ret;
	}
	if (*channel >= ARRAY_SIZE(auxadc_chans)) {
		dev_notice(adc_dev->dev, "channel number %d in node:%s not exists\n",
			   *channel, node->name);
		return -EINVAL;
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
	adc_dev->info = of_device_get_match_data(&pdev->dev);

	ret = auxadc_parse_dt(adc_dev, node);
	if (ret < 0) {
		dev_notice(&pdev->dev, "auxadc_parse_dt fail, ret=%d\n", ret);
		return ret;
	}

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
	register_syscore_ops(&auxadc_syscore_ops);
#if AUXADC_DEBUG
	switch (chip->chip_id) {
	case MT6357_CHIP_ID:
		auxadc_set_convert_fn(AUXADC_DCXO_TEMP, mt6357_dcxo_temp_conv);
		auxadc_set_convert_fn(AUXADC_VBIF, mt6357_vbif_conv);
		auxadc_set_cali_fn(AUXADC_BAT_TEMP, mt635x_bat_temp_cali);
		break;
	case MT6358_CHIP_ID:
	case MT6366_CHIP_ID:
		auxadc_set_convert_fn(AUXADC_VDCXO, mt6358_vdcxo_conv);
		auxadc_set_convert_fn(AUXADC_VBIF, mt6358_vbif_conv);
		auxadc_set_cali_fn(AUXADC_BATADC, mt6358_batadc_cali);
		auxadc_set_cali_fn(AUXADC_BAT_TEMP, mt635x_bat_temp_cali);
		break;
	default:
		break;
	}
#endif
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
		.compatible = "mediatek,mt6359p-auxadc",
		.data = &mt6359p_info,
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
