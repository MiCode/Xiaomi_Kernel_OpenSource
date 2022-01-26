/*
 *  include header file for MediaTek MT6370 Charger
 *
 *  Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_MFD_MT6370_PMU_H
#define __LINUX_MFD_MT6370_PMU_H

#include <mt-plat/rt-regmap.h>
#include <linux/rtmutex.h>
#include <linux/interrupt.h>

#define mt_dbg(dev, fmt, ...) \
	do { \
		if (dbg_log_en) \
			dev_dbg(dev, fmt, ##__VA_ARGS__); \
	} while (0)

enum {
	MT6370_PMU_COREDEV,
	MT6370_PMU_CHGDEV,
	MT6370_PMU_FLEDDEV1,
	MT6370_PMU_FLEDDEV2,
	MT6370_PMU_LDODEV,
	MT6370_PMU_RGBLEDDEV,
	MT6370_PMU_BLEDDEV,
	MT6370_PMU_DSVDEV,
	MT6370_PMU_DEV_MAX,
};

struct mt6370_pmu_irq_desc {
	const char *name;
	irq_handler_t irq_handler;
	int irq;
};

#define MT6370_PMU_IRQDESC(name) { #name, mt6370_pmu_##name##_irq_handler, -1}

struct mt6370_pmu_platform_data {
	int intr_gpio;

	void *platform_data[MT6370_PMU_DEV_MAX];
	size_t pdata_size[MT6370_PMU_DEV_MAX];

	const char **irq_enable[MT6370_PMU_DEV_MAX];
	int num_irq_enable[MT6370_PMU_DEV_MAX];
};

struct mt6370_pmu_chip {
	struct i2c_client *i2c;
	struct device *dev;
	struct irq_domain *irq_domain;
	struct rt_regmap_device *rd;
	struct mutex io_lock;
	int irq;
	uint8_t chip_rev;
	uint8_t chip_vid;
};

#define RT5081_VENDOR_ID		(0x80)
#define MT6370_VENDOR_ID		(0xE0)
#define MT6371_VENDOR_ID		(0xF0)
#define MT6372_VENDOR_ID		(0x90)
#define MT6372C_VENDOR_ID		(0xB0)

/* core control */
#define MT6370_PMU_REG_DEVINFO		(0x00)
#define MT6370_PMU_REG_CORECTRL1	(0x01)
#define MT6370_PMU_REG_CORECTRL2	(0x02)
#define MT6370_PMU_REG_RSTPASCODE1	(0x03)
#define MT6370_PMU_REG_RSTPASCODE2	(0x04)
#define MT6370_PMU_REG_HIDDENPASCODE1	(0x07)
#define MT6370_PMU_REG_HIDDENPASCODE2	(0x08)
#define MT6370_PMU_REG_HIDDENPASCODE3	(0x09)
#define MT6370_PMU_REG_HIDDENPASCODE4	(0x0A)
#define MT6370_PMU_REG_IRQIND		(0x0B)
#define MT6370_PMU_REG_IRQMASK		(0x0C)
#define MT6370_PMU_REG_IRQSET		(0x0D)
#define MT6370_PMU_REG_SHDNCTRL1	(0x0E)
#define MT6370_PMU_REG_SHDNCTRL2	(0x0F)
#define MT6370_PMU_REG_OSCCTRL		(0x10)
/* charger control */
#define MT6370_PMU_REG_CHGCTRL1		(0x11)
#define MT6370_PMU_REG_CHGCTRL2		(0x12)
#define MT6370_PMU_REG_CHGCTRL3		(0x13)
#define MT6370_PMU_REG_CHGCTRL4		(0x14)
#define MT6370_PMU_REG_CHGCTRL5		(0x15)
#define MT6370_PMU_REG_CHGCTRL6		(0x16)
#define MT6370_PMU_REG_CHGCTRL7		(0x17)
#define MT6370_PMU_REG_CHGCTRL8		(0x18)
#define MT6370_PMU_REG_CHGCTRL9		(0x19)
#define MT6370_PMU_REG_CHGCTRL10	(0x1A)
#define MT6370_PMU_REG_CHGCTRL11	(0x1B)
#define MT6370_PMU_REG_CHGCTRL12	(0x1C)
#define MT6370_PMU_REG_CHGCTRL13	(0x1D)
#define MT6370_PMU_REG_CHGCTRL14	(0x1E)
#define MT6370_PMU_REG_CHGCTRL15	(0x1F)
#define MT6370_PMU_REG_CHGCTRL16	(0x20)
#define MT6370_PMU_REG_CHGADC		(0x21)
#define MT6370_PMU_REG_DEVICETYPE	(0x22)
#define MT6370_PMU_REG_QCCTRL1		(0x23)
#define MT6370_PMU_REG_QCCTRL2		(0x24)
#define MT6370_PMU_REG_QC3P0CTRL1	(0x25)
#define MT6370_PMU_REG_QC3P0CTRL2	(0x26)
#define MT6370_PMU_REG_USBSTATUS1	(0x27)
#define MT6370_PMU_REG_QCSTATUS1	(0x28)
#define MT6370_PMU_REG_QCSTATUS2	(0x29)
#define MT6370_PMU_REG_CHGPUMP		(0x2A)
#define MT6370_PMU_REG_CHGCTRL17	(0x2B)
#define MT6370_PMU_REG_CHGCTRL18	(0x2C)
#define MT6370_PMU_REG_CHGDIRCHG1	(0x2D)
#define MT6370_PMU_REG_CHGDIRCHG2	(0x2E)
#define MT6370_PMU_REG_CHGDIRCHG3	(0x2F)
#define MT6370_PMU_REG_CHGHIDDENCTRL0	(0x30)
#define MT6370_PMU_REG_CHGHIDDENCTRL1	(0x31)
#define MT6370_PMU_REG_LG_CONTROL	(0x33)
#define MT6370_PMU_REG_CHGHIDDENCTRL6	(0x35)
#define MT6370_PMU_REG_CHGHIDDENCTRL7	(0x36)
#define MT6370_PMU_REG_CHGHIDDENCTRL8	(0x37)
#define MT6370_PMU_REG_CHGHIDDENCTRL9	(0x38)
#define MT6370_PMU_REG_CHGHIDDENCTRL15	(0x3E)
#define MT6370_PMU_REG_CHGSTAT		(0x4A)
#define MT6370_PMU_REG_CHGNTC		(0x4B)
#define MT6370_PMU_REG_ADCDATAH		(0x4C)
#define MT6370_PMU_REG_ADCDATAL		(0x4D)
#define MT6370_PMU_REG_ADCDATATUNEH	(0x4E)
#define MT6370_PMU_REG_ADCDATATUNEL	(0x4F)
#define MT6370_PMU_REG_ADCDATAORGH	(0x50)
#define MT6370_PMU_REG_ADCDATAORGL	(0x51)
#define MT6370_PMU_REG_ADCBATDATAH	(0x52)
#define MT6370_PMU_REG_ADCBATDATAL	(0x53)
#define MT6370_PMU_REG_CHGCTRL19	(0x60)
#define MT6370_PMU_REG_OVPCTRL		(0x61)
#define MT6370_PMU_REG_VDDASUPPLY	(0x62)
/* flashled control */
#define MT6370_PMU_REG_FLEDCFG		(0x70)
#define MT6370_PMU_REG_FLED1CTRL	(0x72)
#define MT6370_PMU_REG_FLEDSTRBCTRL	(0x73)
#define MT6370_PMU_REG_FLED1STRBCTRL	(0x74)
#define MT6370_PMU_REG_FLED1TORCTRL	(0x75)
#define MT6370_PMU_REG_FLED2CTRL	(0x76)
#define MT6370_PMU_REG_FLED2STRBCTRL2	(0x78)
#define MT6370_PMU_REG_FLED2TORCTRL	(0x79)
#define MT6370_PMU_REG_FLEDVMIDTRKCTRL1	(0x7A)
#define MT6370_PMU_REG_FLEDVMIDRTM	(0x7B)
#define MT6370_PMU_REG_FLEDVMIDTRKCTRL2	(0x7C)
#define MT6370_PMU_REG_FLEDEN		(0x7E)
/* ldo control */
#define MT6370_PMU_REG_LDOCFG		(0x80)
#define MT6370_PMU_REG_LDOVOUT		(0x81)
/* rgb control */
#define MT6370_PMU_REG_RGB1DIM		(0x82)
#define MT6370_PMU_REG_RGB2DIM		(0x83)
#define MT6370_PMU_REG_RGB3DIM		(0x84)
#define MT6370_PMU_REG_RGBEN		(0x85)
#define MT6370_PMU_REG_RGB1ISINK	(0x86)
#define MT6370_PMU_REG_RGB2ISINK	(0x87)
#define MT6370_PMU_REG_RGB3ISINK	(0x88)
#define MT6370_PMU_REG_RGB1TR		(0x89)
#define MT6370_PMU_REG_RGB1TF		(0x8A)
#define MT6370_PMU_REG_RGB1TONTOFF	(0x8B)
#define MT6370_PMU_REG_RGB2TR		(0x8C)
#define MT6370_PMU_REG_RGB2TF		(0x8D)
#define MT6370_PMU_REG_RGB2TONTOFF	(0x8E)
#define MT6370_PMU_REG_RGB3TR		(0x8F)
#define MT6370_PMU_REG_RGB3TF		(0x90)
#define MT6370_PMU_REG_RGB3TONTOFF	(0x91)
#define MT6370_PMU_REG_RGBCHRINDDIM	(0x92)
#define MT6370_PMU_REG_RGBCHRINDCTRL	(0x93)
#define MT6370_PMU_REG_RGBCHRINDTR	(0x94)
#define MT6370_PMU_REG_RGBCHRINDTF	(0x95)
#define MT6370_PMU_REG_RGBCHRINDTONTOFF	(0x96)
#define MT6370_PMU_REG_RGBOPENSHORTEN	(0x97)
#define MT6370_PMU_REG_RGBTONTOFF	(0x98)
#define MT6370_PMU_REG_RGBHIDDEN1	(0x99)
#define MT6370_PMU_REG_RGBHIDDEN2	(0x9A)
#define MT6370_PMU_REG_RESERVED1	(0x9F)
/* backlight control */
#define MT6370_PMU_REG_BLEN		(0xA0)
#define MT6370_PMU_REG_BLBSTCTRL	(0xA1)
#define MT6370_PMU_REG_BLPWM		(0xA2)
#define MT6370_PMU_REG_BLCTRL		(0xA3)
#define MT6370_PMU_REG_BLDIM2		(0xA4)
#define MT6370_PMU_REG_BLDIM1		(0xA5)
#define MT6370_PMU_REG_BLAFH		(0xA6)
#define MT6370_PMU_REG_BLFL		(0xA7)
#define MT6370_PMU_REG_BLFLTO		(0xA8)
#define MT6370_PMU_REG_BLTORCTRL	(0xA9)
#define MT6370_PMU_REG_BLSTRBCTRL	(0xAA)
#define MT6370_PMU_REG_BLAVG		(0xAB)
#define MT6370_PMU_REG_BLMODECTRL	(0xAD)
/* display bias control */
#define MT6370_PMU_REG_DBCTRL1		(0xB0)
#define MT6370_PMU_REG_DBCTRL2		(0xB1)
#define MT6370_PMU_REG_DBVBST		(0xB2)
#define MT6370_PMU_REG_DBVPOS		(0xB3)
#define MT6370_PMU_REG_DBVNEG		(0xB4)
/* irq event */
#define MT6370_PMU_REG_CHGIRQ1		(0xC0)
#define MT6370_PMU_REG_CHGIRQ2		(0xC1)
#define MT6370_PMU_REG_CHGIRQ3		(0xC2)
#define MT6370_PMU_REG_CHGIRQ4		(0xC3)
#define MT6370_PMU_REG_CHGIRQ5		(0xC4)
#define MT6370_PMU_REG_CHGIRQ6		(0xC5)
#define MT6370_PMU_REG_QCIRQ		(0xC6)
#define MT6370_PMU_REG_DICHGIRQ7	(0xC7)
#define MT6370_PMU_REG_OVPCTRLIRQ	(0xC8)
#define MT6370_PMU_REG_FLEDIRQ1		(0xC9)
#define MT6370_PMU_REG_FLEDIRQ2		(0xCA)
#define MT6370_PMU_REG_BASEIRQ		(0xCB)
#define MT6370_PMU_REG_LDOIRQ		(0xCC)
#define MT6370_PMU_REG_RGBIRQ		(0xCD)
#define MT6370_PMU_REG_BLIRQ		(0xCE)
#define MT6370_PMU_REG_DBIRQ		(0xCF)
/* status event */
#define MT6370_PMU_REG_CHGSTAT1		(0xD0)
#define MT6370_PMU_REG_CHGSTAT2		(0xD1)
#define MT6370_PMU_REG_CHGSTAT3		(0xD2)
#define MT6370_PMU_REG_CHGSTAT4		(0xD3)
#define MT6370_PMU_REG_CHGSTAT5		(0xD4)
#define MT6370_PMU_REG_CHGSTAT6		(0xD5)
#define MT6370_PMU_REG_QCSTAT		(0xD6)
#define MT6370_PMU_REG_DICHGSTAT	(0xD7)
#define MT6370_PMU_REG_OVPCTRLSTAT	(0xD8)
#define MT6370_PMU_REG_FLEDSTAT1	(0xD9)
#define MT6370_PMU_REG_FLEDSTAT2	(0xDA)
#define MT6370_PMU_REG_BASESTAT		(0xDB)
#define MT6370_PMU_REG_LDOSTAT		(0xDC)
#define MT6370_PMU_REG_RGBSTAT		(0xDD)
#define MT6370_PMU_REG_BLSTAT		(0xDE)
#define MT6370_PMU_REG_DBSTAT		(0xDF)
/* irq mask */
#define MT6370_PMU_CHGMASK1		(0xE0)
#define MT6370_PMU_CHGMASK2		(0xE1)
#define MT6370_PMU_CHGMASK3		(0xE2)
#define MT6370_PMU_CHGMASK4		(0xE3)
#define MT6370_PMU_CHGMASK5		(0xE4)
#define MT6370_PMU_CHGMASK6		(0xE5)
#define MT6370_PMU_DPDMMASK1		(0xE6)
#define MT6370_PMU_DICHGMASK		(0xE7)
#define MT6370_PMU_OVPCTRLMASK		(0xE8)
#define MT6370_PMU_FLEDMASK1		(0xE9)
#define MT6370_PMU_FLEDMASK2		(0xEA)
#define MT6370_PMU_BASEMASK		(0xEB)
#define MT6370_PMU_LDOMASK		(0xEC)
#define MT6370_PMU_RGBMASK		(0xED)
#define MT6370_PMU_BLMASK		(0xEE)
#define MT6370_PMU_DBMASK		(0xEF)

/* general io ops start */
extern int mt6370_pmu_reg_read(struct mt6370_pmu_chip *chip, u8 addr);
extern int mt6370_pmu_reg_write(struct mt6370_pmu_chip *chip,
	u8 addr, u8 data);
extern int mt6370_pmu_reg_update_bits(struct mt6370_pmu_chip *chip, u8 addr,
	u8 mask, u8 data);

static inline int mt6370_pmu_reg_set_bit(struct mt6370_pmu_chip *chip, u8 addr,
		u8 mask)
{
	return mt6370_pmu_reg_update_bits(chip, addr, mask, mask);
}

static inline int mt6370_pmu_reg_clr_bit(struct mt6370_pmu_chip *chip, u8 addr,
		u8 mask)
{
	return mt6370_pmu_reg_update_bits(chip, addr, mask, 0x00);
}

static inline int mt6370_pmu_reg_test_bit(
	struct mt6370_pmu_chip *chip, u8 cmd, u8 shift, bool *is_one)
{
	int ret = 0;
	u8 data = 0;

	ret = mt6370_pmu_reg_read(chip, cmd);
	if (ret < 0) {
		*is_one = false;
		return ret;
	}

	data = ret & (1 << shift);
	*is_one = (data == 0 ? false : true);

	return ret;
}

extern int mt6370_pmu_reg_block_read(struct mt6370_pmu_chip *chip, u8 addr,
	int len, u8 *dest);
extern int mt6370_pmu_reg_block_write(struct mt6370_pmu_chip *chip, u8 addr,
	int len, const u8 *src);
/* general io ops end */
/* extern function from other files start */
extern int mt6370_pmu_regmap_register(struct mt6370_pmu_chip *chip,
	struct rt_regmap_fops *regmap_ops);
extern void mt6370_pmu_regmap_unregister(struct mt6370_pmu_chip *chip);
extern int mt6370_pmu_get_virq_number(struct mt6370_pmu_chip *chip,
	const char *name);
extern const char *mt6370_pmu_get_hwirq_name(struct mt6370_pmu_chip *chip,
	int id);
extern int mt6370_pmu_irq_register(struct mt6370_pmu_chip *chip);
extern void mt6370_pmu_irq_unregister(struct mt6370_pmu_chip *chip);
extern void mt6370_pmu_irq_suspend(struct mt6370_pmu_chip *chip);
extern void mt6370_pmu_irq_resume(struct mt6370_pmu_chip *chip);
extern int mt6370_pmu_subdevs_register(struct mt6370_pmu_chip *chip);
extern void mt6370_pmu_subdevs_unregister(struct mt6370_pmu_chip *chip);
/* extern function from other files end */

#endif /* #ifndef __LINUX_MFD_MT6370_PMU_H */
