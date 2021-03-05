/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MT6360_PMU_H
#define __MT6360_PMU_H

#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include "config.h"
#include <mt-plat/rt-regmap.h>

extern bool dbg_log_en;
#define mt_dbg(dev, fmt, ...) \
	do { \
		if (dbg_log_en) \
			dev_dbg(dev, fmt, ##__VA_ARGS__); \
	} while (0)

enum {
	MT6360_PMUDEV_CORE = 0,
	MT6360_PMUDEV_ADC,
	MT6360_PMUDEV_CHG,
	MT6360_PMUDEV_FLED,
	MT6360_PMUDEV_RGBLED,
	MT6360_PMUDEV_MAX,
};

struct mt6360_pmu_platform_data {
	u32 int_ret, disable_lpsd;
	int irq_gpio;
	void *dev_platform_data[MT6360_PMUDEV_MAX];
	size_t dev_pdata_size[MT6360_PMUDEV_MAX];
	struct resource *dev_irq_resources[MT6360_PMUDEV_MAX];
	int dev_irq_res_cnt[MT6360_PMUDEV_MAX];
};

struct mt6360_pmu_info;

struct mt6360_pmu_irq_desc {
	const char *name;
	irq_handler_t irq_handler;
	int irq;
};

#define MT6360_PMU_IRQDESC(name) {#name, mt6360_pmu_##name##_handler, -1}

/* register defininition */
#define MT6360_PMU_DEV_INFO			(0x00)
#define MT6360_PMU_CORE_CTRL1			(0x01)
#define MT6360_PMU_RST1				(0x02)
#define MT6360_PMU_CRCEN			(0x03)
#define MT6360_PMU_RST_PAS_CODE1		(0x04)
#define MT6360_PMU_RST_PAS_CODE2		(0x05)
#define MT6360_PMU_CORE_CTRL2			(0x06)
#define MT6360_PMU_TM_PAS_CODE1			(0x07)
#define MT6360_PMU_TM_PAS_CODE2			(0x08)
#define MT6360_PMU_TM_PAS_CODE3			(0x09)
#define MT6360_PMU_TM_PAS_CODE4			(0x0A)
#define MT6360_PMU_IRQ_IND			(0x0B)
#define MT6360_PMU_IRQ_MASK			(0x0C)
#define MT6360_PMU_IRQ_SET			(0x0D)
#define MT6360_PMU_SHDN_CTRL			(0x0E)
#define MT6360_PMU_TM_INF			(0x0F)
#define MT6360_PMU_I2C_CTRL			(0x10)
#define MT6360_PMU_CHG_CTRL1			(0x11)
#define MT6360_PMU_CHG_CTRL2			(0x12)
#define MT6360_PMU_CHG_CTRL3			(0x13)
#define MT6360_PMU_CHG_CTRL4			(0x14)
#define MT6360_PMU_CHG_CTRL5			(0x15)
#define MT6360_PMU_CHG_CTRL6			(0x16)
#define MT6360_PMU_CHG_CTRL7			(0x17)
#define MT6360_PMU_CHG_CTRL8			(0x18)
#define MT6360_PMU_CHG_CTRL9			(0x19)
#define MT6360_PMU_CHG_CTRL10			(0x1A)
#define MT6360_PMU_CHG_CTRL11			(0x1B)
#define MT6360_PMU_CHG_CTRL12			(0x1C)
#define MT6360_PMU_CHG_CTRL13			(0x1D)
#define MT6360_PMU_CHG_CTRL14			(0x1E)
#define MT6360_PMU_CHG_CTRL15			(0x1F)
#define MT6360_PMU_CHG_CTRL16			(0x20)
#define MT6360_PMU_CHG_AICC_RESULT		(0x21)
#define MT6360_PMU_DEVICE_TYPE			(0x22)
#define MT6360_PMU_DCP_CONTROL			(0x24)
#define MT6360_PMU_USB_STATUS1			(0x27)
#define MT6360_PMU_DPDM_CTRL			(0x28)
#define MT6360_PMU_CHG_PUMP			(0x2A)
#define MT6360_PMU_CHG_CTRL17			(0x2B)
#define MT6360_PMU_CHG_CTRL18			(0x2C)
#define MT6360_PMU_CHRDET_CTRL1			(0x2D)
#define MT6360_PMU_CHRDET_CTRL2			(0x2E)
#define MT6360_PMU_DPDN_CTRL			(0x2F)
#define MT6360_PMU_CHG_HIDDEN_CTRL1		(0x30)
#define MT6360_PMU_CHG_HIDDEN_CTRL2		(0x31)
#define MT6360_PMU_CHG_HIDDEN_CTRL3		(0x32)
#define MT6360_PMU_CHG_HIDDEN_CTRL4		(0x33)
#define MT6360_PMU_CHG_HIDDEN_CTRL5		(0x34)
#define MT6360_PMU_CHG_HIDDEN_CTRL6		(0x35)
#define MT6360_PMU_CHG_HIDDEN_CTRL7		(0x36)
#define MT6360_PMU_CHG_HIDDEN_CTRL8		(0x37)
#define MT6360_PMU_CHG_HIDDEN_CTRL9		(0x38)
#define MT6360_PMU_CHG_HIDDEN_CTRL10		(0x39)
#define MT6360_PMU_CHG_HIDDEN_CTRL11		(0x3A)
#define MT6360_PMU_CHG_HIDDEN_CTRL12		(0x3B)
#define MT6360_PMU_CHG_HIDDEN_CTRL13		(0x3C)
#define MT6360_PMU_CHG_HIDDEN_CTRL14		(0x3D)
#define MT6360_PMU_CHG_HIDDEN_CTRL15		(0x3E)
#define MT6360_PMU_CHG_HIDDEN_CTRL16		(0x3F)
#define MT6360_PMU_CHG_HIDDEN_CTRL17		(0x40)
#define MT6360_PMU_CHG_HIDDEN_CTRL18		(0x41)
#define MT6360_PMU_CHG_HIDDEN_CTRL19		(0x42)
#define MT6360_PMU_CHG_HIDDEN_CTRL20		(0x43)
#define MT6360_PMU_CHG_HIDDEN_CTRL21		(0x44)
#define MT6360_PMU_CHG_HIDDEN_CTRL22		(0x45)
#define MT6360_PMU_CHG_HIDDEN_CTRL23		(0x46)
#define MT6360_PMU_CHG_HIDDEN_CTRL24		(0x47)
#define MT6360_PMU_CHG_HIDDEN_CTRL25		(0x48)
#define MT6360_PMU_BC12_CTRL			(0x49)
#define MT6360_PMU_CHG_STAT			(0x4A)
#define MT6360_PMU_RESV1			(0x4B)
#define MT6360_PMU_TYPEC_OTP_TH_SEL_CODEH	(0x4E)
#define MT6360_PMU_TYPEC_OTP_TH_SEL_CODEL	(0x4F)
#define MT6360_PMU_TYPEC_OTP_HYST_TH		(0x50)
#define MT6360_PMU_TYPEC_OTP_CTRL		(0x51)
#define MT6360_PMU_ADC_BAT_DATA_H		(0x52)
#define MT6360_PMU_ADC_BAT_DATA_L		(0x53)
#define MT6360_PMU_IMID_BACKBST_ON		(0x54)
#define MT6360_PMU_IMID_BACKBST_OFF		(0x55)
#define MT6360_PMU_ADC_CONFIG			(0x56)
#define MT6360_PMU_ADC_EN2			(0x57)
#define MT6360_PMU_ADC_IDLE_T			(0x58)
#define MT6360_PMU_ADC_RPT_1			(0x5A)
#define MT6360_PMU_ADC_RPT_2			(0x5B)
#define MT6360_PMU_ADC_RPT_3			(0x5C)
#define MT6360_PMU_ADC_RPT_ORG1			(0x5D)
#define MT6360_PMU_ADC_RPT_ORG2			(0x5E)
#define MT6360_PMU_BAT_OVP_TH_SEL_CODEH		(0x5F)
#define MT6360_PMU_BAT_OVP_TH_SEL_CODEL		(0x60)
#define MT6360_PMU_CHG_CTRL19			(0x61)
#define MT6360_PMU_VDDASUPPLY			(0x62)
#define MT6360_PMU_BC12_MANUAL			(0x63)
#define MT6360_PMU_CTD_CTRL			(0x65)
#define MT6360_PMU_CHG_CTRL20			(0x66)
#define MT6360_PMU_CHG_HIDDEN_CTRL26		(0x67)
#define MT6360_PMU_CHG_HIDDEN_CTRL27		(0x68)
#define MT6360_PMU_RESV2			(0x69)
#define MT6360_PMU_USBID_CTRL1			(0x6D)
#define MT6360_PMU_USBID_CTRL2			(0x6E)
#define MT6360_PMU_USBID_CTRL3			(0x6F)
#define MT6360_PMU_FLED_CFG			(0x70)
#define MT6360_PMU_RESV3			(0x71)
#define MT6360_PMU_FLED1_CTRL			(0x72)
#define MT6360_PMU_FLED_STRB_CTRL		(0x73)
#define MT6360_PMU_FLED1_STRB_CTRL2		(0x74)
#define MT6360_PMU_FLED1_TOR_CTRL		(0x75)
#define MT6360_PMU_FLED2_CTRL			(0x76)
#define MT6360_PMU_RESV4			(0x77)
#define MT6360_PMU_FLED2_STRB_CTRL2		(0x78)
#define MT6360_PMU_FLED2_TOR_CTRL		(0x79)
#define MT6360_PMU_FLED_VMIDTRK_CTRL1		(0x7A)
#define MT6360_PMU_FLED_VMID_RTM		(0x7B)
#define MT6360_PMU_FLED_VMIDTRK_CTRL2		(0x7C)
#define MT6360_PMU_FLED_PWSEL			(0x7D)
#define MT6360_PMU_FLED_EN			(0x7E)
#define MT6360_PMU_FLED_Hidden1			(0x7F)
#define MT6360_PMU_RGB_EN			(0x80)
#define MT6360_PMU_RGB1_ISNK			(0x81)
#define MT6360_PMU_RGB2_ISNK			(0x82)
#define MT6360_PMU_RGB3_ISNK			(0x83)
#define MT6360_PMU_RGB_ML_ISNK			(0x84)
#define MT6360_PMU_RGB1_DIM			(0x85)
#define MT6360_PMU_RGB2_DIM			(0x86)
#define MT6360_PMU_RGB3_DIM			(0x87)
#define MT6360_PMU_RESV5			(0x88)
#define MT6360_PMU_RGB12_Freq			(0x89)
#define MT6360_PMU_RGB34_Freq			(0x8A)
#define MT6360_PMU_RGB1_Tr			(0x8B)
#define MT6360_PMU_RGB1_Tf			(0x8C)
#define MT6360_PMU_RGB1_TON_TOFF		(0x8D)
#define MT6360_PMU_RGB2_Tr			(0x8E)
#define MT6360_PMU_RGB2_Tf			(0x8F)
#define MT6360_PMU_RGB2_TON_TOFF		(0x90)
#define MT6360_PMU_RGB3_Tr			(0x91)
#define MT6360_PMU_RGB3_Tf			(0x92)
#define MT6360_PMU_RGB3_TON_TOFF		(0x93)
#define MT6360_PMU_RGB_Hidden_CTRL1		(0x94)
#define MT6360_PMU_RGB_Hidden_CTRL2		(0x95)
#define MT6360_PMU_RESV6			(0x97)
#define MT6360_PMU_SPARE1			(0x9A)
#define MT6360_PMU_SPARE2			(0xA0)
#define MT6360_PMU_SPARE3			(0xB0)
#define MT6360_PMU_SPARE4			(0xC0)
#define MT6360_PMU_CHG_IRQ1			(0xD0)
#define MT6360_PMU_CHG_IRQ2			(0xD1)
#define MT6360_PMU_CHG_IRQ3			(0xD2)
#define MT6360_PMU_CHG_IRQ4			(0xD3)
#define MT6360_PMU_CHG_IRQ5			(0xD4)
#define MT6360_PMU_CHG_IRQ6			(0xD5)
#define MT6360_PMU_DPDM_IRQ			(0xD6)
#define MT6360_PMU_CHRDET_IRQ			(0xD7)
#define MT6360_PMU_BASE_IRQ			(0xD8)
#define MT6360_PMU_FLED_IRQ1			(0xD9)
#define MT6360_PMU_FLED_IRQ2			(0xDA)
#define MT6360_PMU_RGB_IRQ			(0xDB)
#define MT6360_PMU_BUCK1_IRQ			(0xDC)
#define MT6360_PMU_BUCK2_IRQ			(0xDD)
#define MT6360_PMU_LDO_IRQ1			(0xDE)
#define MT6360_PMU_LDO_IRQ2			(0xDF)
#define MT6360_PMU_CHG_STAT1			(0xE0)
#define MT6360_PMU_CHG_STAT2			(0xE1)
#define MT6360_PMU_CHG_STAT3			(0xE2)
#define MT6360_PMU_CHG_STAT4			(0xE3)
#define MT6360_PMU_CHG_STAT5			(0xE4)
#define MT6360_PMU_CHG_STAT6			(0xE5)
#define MT6360_PMU_DPDM_STAT			(0xE6)
#define MT6360_PMU_CHRDET_STAT			(0xE7)
#define MT6360_PMU_BASE_STAT			(0xE8)
#define MT6360_PMU_FLED_STAT1			(0xE9)
#define MT6360_PMU_FLED_STAT2			(0xEA)
#define MT6360_PMU_RGB_STAT			(0xEB)
#define MT6360_PMU_BUCK1_STAT			(0xEC)
#define MT6360_PMU_BUCK2_STAT			(0xED)
#define MT6360_PMU_LDO_STAT1			(0xEE)
#define MT6360_PMU_LDO_STAT2			(0xEF)
#define MT6360_PMU_CHG_MASK1			(0xF0)
#define MT6360_PMU_CHG_MASK2			(0xF1)
#define MT6360_PMU_CHG_MASK3			(0xF2)
#define MT6360_PMU_CHG_MASK4			(0xF3)
#define MT6360_PMU_CHG_MASK5			(0xF4)
#define MT6360_PMU_CHG_MASK6			(0xF5)
#define MT6360_PMU_DPDM_MASK			(0xF6)
#define MT6360_PMU_CHRDET_MASK			(0xF7)
#define MT6360_PMU_BASE_MASK			(0xF8)
#define MT6360_PMU_FLED_MASK1			(0xF9)
#define MT6360_PMU_FLED_MASK2			(0xFA)
#define MT6360_PMU_FAULTB_MASK			(0xFB)
#define MT6360_PMU_BUCK1_MASK			(0xFC)
#define MT6360_PMU_BUCK2_MASK			(0xFD)
#define MT6360_PMU_LDO_MASK1			(0xFE)
#define MT6360_PMU_LDO_MASK2			(0xFF)

#define MT6360_PMU_IRQ_REGNUM	(MT6360_PMU_LDO_IRQ2 - MT6360_PMU_CHG_IRQ1 + 1)
#define MT6360_PMU_IRQEVT_MAX	(MT6360_PMU_IRQ_REGNUM * 8)

/* MT6360_PMU_IRQ_SET */
#define MT6360_IRQ_RETRIG	(0x04)

struct mt6360_pmu_info {
	struct i2c_client *i2c;
	struct device *dev;
	struct rt_regmap_device *regmap;
	struct irq_domain *irq_domain;
	u8 cache_irq_masks[MT6360_PMU_IRQ_REGNUM];
	struct mutex io_lock;
	int irq;
	u8 chip_rev;
};

/* Global IO API for subdev */
extern int mt6360_pmu_reg_read(struct mt6360_pmu_info *mpi, u8 addr);
extern int mt6360_pmu_reg_write(struct mt6360_pmu_info *mpi, u8 addr, u8 data);
extern int mt6360_pmu_reg_update_bits(struct mt6360_pmu_info *mpi,
				      u8 addr, u8 mask, u8 data);
static inline int mt6360_pmu_reg_set_bits(struct mt6360_pmu_info *mpi,
					  u8 addr, u8 mask)
{
	return mt6360_pmu_reg_update_bits(mpi, addr, mask, mask);
}

static inline int mt6360_pmu_reg_clr_bits(struct mt6360_pmu_info *mpi,
					  u8 addr, u8 mask)
{
	return mt6360_pmu_reg_update_bits(mpi, addr, mask, 0);
}
extern int mt6360_pmu_reg_block_read(struct mt6360_pmu_info *mpi,
				     u8 addr, u8 len, u8 *dst);
extern int mt6360_pmu_reg_block_write(struct mt6360_pmu_info *mpi,
				      u8 addr, u8 len, const u8 *src);

#define  MT6360_DT_VALPROP(name, type) \
			{#name, offsetof(type, name)}

struct mt6360_val_prop {
	const char *name;
	size_t offset;
};

static inline void mt6360_dt_parser_helper(struct device_node *np, void *data,
					   const struct mt6360_val_prop *props,
					   int prop_cnt)
{
	int i;

	for (i = 0; i < prop_cnt; i++) {
		if (unlikely(!props[i].name))
			continue;
		of_property_read_u32(np, props[i].name, data + props[i].offset);
	}
}

#define MT6360_PDATA_VALPROP(name, type, reg, shift, mask, func, base) \
			{offsetof(type, name), reg, shift, mask, func, base}

struct mt6360_pdata_prop {
	size_t offset;
	u8 reg;
	u8 shift;
	u8 mask;
	u32 (*transform)(u32 val);
	u8 base;
};

static inline int mt6360_pdata_apply_helper(void *info, void *pdata,
					   const struct mt6360_pdata_prop *prop,
					   int prop_cnt)
{
	int i, ret;
	u32 val;

	for (i = 0; i < prop_cnt; i++) {
		val = *(u32 *)(pdata + prop[i].offset);
		if (prop[i].transform)
			val = prop[i].transform(val);
		val += prop[i].base;
		ret = mt6360_pmu_reg_update_bits(info,
			     prop[i].reg, prop[i].mask, val << prop[i].shift);
		if (ret < 0)
			return ret;
	}
	return 0;
}

extern int mt6360_pmu_regmap_register(struct mt6360_pmu_info *mpi,
				      struct rt_regmap_fops *fops);
extern void mt6360_pmu_regmap_unregister(struct mt6360_pmu_info *mpi);

extern int mt6360_pmu_irq_register(struct mt6360_pmu_info *mpi);
extern void mt6360_pmu_irq_unregister(struct mt6360_pmu_info *mpi);
extern int mt6360_pmu_irq_suspend(struct mt6360_pmu_info *mpi);
extern int mt6360_pmu_irq_resume(struct mt6360_pmu_info *mpi);

extern int mt6360_pmu_subdev_register(struct mt6360_pmu_info *mpi);
extern void mt6360_pmu_subdev_unregister(struct mt6360_pmu_info *mpi);

#endif /* __MT6360_PMU_H */
