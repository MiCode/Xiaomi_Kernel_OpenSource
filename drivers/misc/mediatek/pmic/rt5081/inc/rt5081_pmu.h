/*
 *  include header file for Richtek RT5081 Charger
 *
 *  Copyright (C) 2016 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
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

#ifndef __LINUX_MFD_RT5081_PMU_H
#define __LINUX_MFD_RT5081_PMU_H

#include <mt-plat/rt-regmap.h>
#include <linux/rtmutex.h>
#include <linux/interrupt.h>

#define rt_dbg(dev, fmt, ...) \
	do { \
		if (dbg_log_en) \
			dev_dbg(dev, fmt, ##__VA_ARGS__); \
	} while (0)

enum {
	RT5081_PMU_COREDEV,
	RT5081_PMU_CHGDEV,
	RT5081_PMU_FLEDDEV1,
	RT5081_PMU_FLEDDEV2,
	RT5081_PMU_LDODEV,
	RT5081_PMU_RGBLEDDEV,
	RT5081_PMU_BLEDDEV,
	RT5081_PMU_DSVDEV,
	RT5081_PMU_DEV_MAX,
};

struct rt5081_pmu_irq_desc {
	const char *name;
	irq_handler_t irq_handler;
	int irq;
};

#define RT5081_PMU_IRQDESC(name) { #name, rt5081_pmu_##name##_irq_handler, -1}

struct rt5081_pmu_platform_data {
	int intr_gpio;

	void *platform_data[RT5081_PMU_DEV_MAX];
	size_t pdata_size[RT5081_PMU_DEV_MAX];

	const char **irq_enable[RT5081_PMU_DEV_MAX];
	int num_irq_enable[RT5081_PMU_DEV_MAX];
};

struct rt5081_pmu_chip {
	struct i2c_client *i2c;
	struct device *dev;
	struct irq_domain *irq_domain;
	struct rt_regmap_device *rd;
	struct rt_mutex io_lock;
	int irq;
	uint8_t chip_rev;
};

/* core control */
#define RT5081_PMU_REG_DEVINFO		(0x00)
#define RT5081_PMU_REG_CORECTRL1	(0x01)
#define RT5081_PMU_REG_CORECTRL2	(0x02)
#define RT5081_PMU_REG_RSTPASCODE1	(0x03)
#define RT5081_PMU_REG_RSTPASCODE2	(0x04)
#define RT5081_PMU_REG_HIDDENPASCODE1	(0x07)
#define RT5081_PMU_REG_HIDDENPASCODE2	(0x08)
#define RT5081_PMU_REG_HIDDENPASCODE3	(0x09)
#define RT5081_PMU_REG_HIDDENPASCODE4	(0x0A)
#define RT5081_PMU_REG_IRQIND		(0x0B)
#define RT5081_PMU_REG_IRQMASK		(0x0C)
#define RT5081_PMU_REG_IRQSET		(0x0D)
#define RT5081_PMU_REG_SHDNCTRL1	(0x0E)
#define RT5081_PMU_REG_SHDNCTRL2	(0x0F)
#define RT5081_PMU_REG_OSCCTRL		(0x10)
/* charger control */
#define RT5081_PMU_REG_CHGCTRL1		(0x11)
#define RT5081_PMU_REG_CHGCTRL2		(0x12)
#define RT5081_PMU_REG_CHGCTRL3		(0x13)
#define RT5081_PMU_REG_CHGCTRL4		(0x14)
#define RT5081_PMU_REG_CHGCTRL5		(0x15)
#define RT5081_PMU_REG_CHGCTRL6		(0x16)
#define RT5081_PMU_REG_CHGCTRL7		(0x17)
#define RT5081_PMU_REG_CHGCTRL8		(0x18)
#define RT5081_PMU_REG_CHGCTRL9		(0x19)
#define RT5081_PMU_REG_CHGCTRL10	(0x1A)
#define RT5081_PMU_REG_CHGCTRL11	(0x1B)
#define RT5081_PMU_REG_CHGCTRL12	(0x1C)
#define RT5081_PMU_REG_CHGCTRL13	(0x1D)
#define RT5081_PMU_REG_CHGCTRL14	(0x1E)
#define RT5081_PMU_REG_CHGCTRL15	(0x1F)
#define RT5081_PMU_REG_CHGCTRL16	(0x20)
#define RT5081_PMU_REG_CHGADC		(0x21)
#define RT5081_PMU_REG_DEVICETYPE	(0x22)
#define RT5081_PMU_REG_QCCTRL1		(0x23)
#define RT5081_PMU_REG_QCCTRL2		(0x24)
#define RT5081_PMU_REG_QC3P0CTRL1	(0x25)
#define RT5081_PMU_REG_QC3P0CTRL2	(0x26)
#define RT5081_PMU_REG_USBSTATUS1	(0x27)
#define RT5081_PMU_REG_QCSTATUS1	(0x28)
#define RT5081_PMU_REG_QCSTATUS2	(0x29)
#define RT5081_PMU_REG_CHGPUMP		(0x2A)
#define RT5081_PMU_REG_CHGCTRL17	(0x2B)
#define RT5081_PMU_REG_CHGCTRL18	(0x2C)
#define RT5081_PMU_REG_CHGDIRCHG1	(0x2D)
#define RT5081_PMU_REG_CHGDIRCHG2	(0x2E)
#define RT5081_PMU_REG_CHGDIRCHG3	(0x2F)
#define RT5081_PMU_REG_CHGHIDDENCTRL0	(0x30)
#define RT5081_PMU_REG_CHGHIDDENCTRL1	(0x31)
#define RT5081_PMU_REG_LG_CONTROL	(0x33)
#define RT5081_PMU_REG_CHGHIDDENCTRL6	(0x35)
#define RT5081_PMU_REG_CHGHIDDENCTRL7	(0x36)
#define RT5081_PMU_REG_CHGHIDDENCTRL8	(0x37)
#define RT5081_PMU_REG_CHGHIDDENCTRL9	(0x38)
#define RT5081_PMU_REG_CHGHIDDENCTRL15	(0x3E)
#define RT5081_PMU_REG_CHGSTAT		(0x4A)
#define RT5081_PMU_REG_CHGNTC		(0x4B)
#define RT5081_PMU_REG_ADCDATAH		(0x4C)
#define RT5081_PMU_REG_ADCDATAL		(0x4D)
#define RT5081_PMU_REG_ADCDATATUNEH	(0x4E)
#define RT5081_PMU_REG_ADCDATATUNEL	(0x4F)
#define RT5081_PMU_REG_ADCDATAORGH	(0x50)
#define RT5081_PMU_REG_ADCDATAORGL	(0x51)
#define RT5081_PMU_REG_ADCBATDATAH	(0x52)
#define RT5081_PMU_REG_ADCBATDATAL	(0x53)
#define RT5081_PMU_REG_CHGCTRL19	(0x60)
#define RT5081_PMU_REG_OVPCTRL		(0x61)
/* flashled control */
#define RT5081_PMU_REG_FLEDCFG		(0x70)
#define RT5081_PMU_REG_FLED1CTRL	(0x72)
#define RT5081_PMU_REG_FLEDSTRBCTRL	(0x73)
#define RT5081_PMU_REG_FLED1STRBCTRL	(0x74)
#define RT5081_PMU_REG_FLED1TORCTRL	(0x75)
#define RT5081_PMU_REG_FLED2CTRL	(0x76)
#define RT5081_PMU_REG_FLED2STRBCTRL2	(0x78)
#define RT5081_PMU_REG_FLED2TORCTRL	(0x79)
#define RT5081_PMU_REG_FLEDVMIDTRKCTRL1	(0x7A)
#define RT5081_PMU_REG_FLEDVMIDRTM	(0x7B)
#define RT5081_PMU_REG_FLEDVMIDTRKCTRL2	(0x7C)
#define RT5081_PMU_REG_FLEDEN		(0x7E)
/* ldo control */
#define RT5081_PMU_REG_LDOCFG		(0x80)
#define RT5081_PMU_REG_LDOVOUT		(0x81)
/* rgb control */
#define RT5081_PMU_REG_RGB1DIM		(0x82)
#define RT5081_PMU_REG_RGB2DIM		(0x83)
#define RT5081_PMU_REG_RGB3DIM		(0x84)
#define RT5081_PMU_REG_RGBEN		(0x85)
#define RT5081_PMU_REG_RGB1ISINK	(0x86)
#define RT5081_PMU_REG_RGB2ISINK	(0x87)
#define RT5081_PMU_REG_RGB3ISINK	(0x88)
#define RT5081_PMU_REG_RGB1TR		(0x89)
#define RT5081_PMU_REG_RGB1TF		(0x8A)
#define RT5081_PMU_REG_RGB1TONTOFF	(0x8B)
#define RT5081_PMU_REG_RGB2TR		(0x8C)
#define RT5081_PMU_REG_RGB2TF		(0x8D)
#define RT5081_PMU_REG_RGB2TONTOFF	(0x8E)
#define RT5081_PMU_REG_RGB3TR		(0x8F)
#define RT5081_PMU_REG_RGB3TF		(0x90)
#define RT5081_PMU_REG_RGB3TONTOFF	(0x91)
#define RT5081_PMU_REG_RGBCHRINDDIM	(0x92)
#define RT5081_PMU_REG_RGBCHRINDCTRL	(0x93)
#define RT5081_PMU_REG_RGBCHRINDTR	(0x94)
#define RT5081_PMU_REG_RGBCHRINDTF	(0x95)
#define RT5081_PMU_REG_RGBCHRINDTONTOFF	(0x96)
#define RT5081_PMU_REG_RGBOPENSHORTEN	(0x97)
#define RT5081_PMU_REG_RESERVED1	(0x9F)
/* backlight control */
#define RT5081_PMU_REG_BLEN		(0xA0)
#define RT5081_PMU_REG_BLBSTCTRL	(0xA1)
#define RT5081_PMU_REG_BLPWM		(0xA2)
#define RT5081_PMU_REG_BLCTRL		(0xA3)
#define RT5081_PMU_REG_BLDIM2		(0xA4)
#define RT5081_PMU_REG_BLDIM1		(0xA5)
#define RT5081_PMU_REG_BLAFH		(0xA6)
#define RT5081_PMU_REG_BLFL		(0xA7)
#define RT5081_PMU_REG_BLFLTO		(0xA8)
#define RT5081_PMU_REG_BLTORCTRL	(0xA9)
#define RT5081_PMU_REG_BLSTRBCTRL	(0xAA)
#define RT5081_PMU_REG_BLAVG		(0xAB)
/* display bias control */
#define RT5081_PMU_REG_DBCTRL1		(0xB0)
#define RT5081_PMU_REG_DBCTRL2		(0xB1)
#define RT5081_PMU_REG_DBVBST		(0xB2)
#define RT5081_PMU_REG_DBVPOS		(0xB3)
#define RT5081_PMU_REG_DBVNEG		(0xB4)
/* irq event */
#define RT5081_PMU_REG_CHGIRQ1		(0xC0)
#define RT5081_PMU_REG_CHGIRQ2		(0xC1)
#define RT5081_PMU_REG_CHGIRQ3		(0xC2)
#define RT5081_PMU_REG_CHGIRQ4		(0xC3)
#define RT5081_PMU_REG_CHGIRQ5		(0xC4)
#define RT5081_PMU_REG_CHGIRQ6		(0xC5)
#define RT5081_PMU_REG_QCIRQ		(0xC6)
#define RT5081_PMU_REG_DICHGIRQ7	(0xC7)
#define RT5081_PMU_REG_OVPCTRLIRQ	(0xC8)
#define RT5081_PMU_REG_FLEDIRQ1		(0xC9)
#define RT5081_PMU_REG_FLEDIRQ2		(0xCA)
#define RT5081_PMU_REG_BASEIRQ		(0xCB)
#define RT5081_PMU_REG_LDOIRQ		(0xCC)
#define RT5081_PMU_REG_RGBIRQ		(0xCD)
#define RT5081_PMU_REG_BLIRQ		(0xCE)
#define RT5081_PMU_REG_DBIRQ		(0xCF)
/* status event */
#define RT5081_PMU_REG_CHGSTAT1		(0xD0)
#define RT5081_PMU_REG_CHGSTAT2		(0xD1)
#define RT5081_PMU_REG_CHGSTAT3		(0xD2)
#define RT5081_PMU_REG_CHGSTAT4		(0xD3)
#define RT5081_PMU_REG_CHGSTAT5		(0xD4)
#define RT5081_PMU_REG_CHGSTAT6		(0xD5)
#define RT5081_PMU_REG_QCSTAT		(0xD6)
#define RT5081_PMU_REG_DICHGSTAT	(0xD7)
#define RT5081_PMU_REG_OVPCTRLSTAT	(0xD8)
#define RT5081_PMU_REG_FLEDSTAT1	(0xD9)
#define RT5081_PMU_REG_FLEDSTAT2	(0xDA)
#define RT5081_PMU_REG_BASESTAT		(0xDB)
#define RT5081_PMU_REG_LDOSTAT		(0xDC)
#define RT5081_PMU_REG_RGBSTAT		(0xDD)
#define RT5081_PMU_REG_BLSTAT		(0xDE)
#define RT5081_PMU_REG_DBSTAT		(0xDF)
/* irq mask */
#define RT5081_PMU_CHGMASK1		(0xE0)
#define RT5081_PMU_CHGMASK2		(0xE1)
#define RT5081_PMU_CHGMASK3		(0xE2)
#define RT5081_PMU_CHGMASK4		(0xE3)
#define RT5081_PMU_CHGMASK5		(0xE4)
#define RT5081_PMU_CHGMASK6		(0xE5)
#define RT5081_PMU_DPDMMASK1		(0xE6)
#define RT5081_PMU_DICHGMASK		(0xE7)
#define RT5081_PMU_OVPCTRLMASK		(0xE8)
#define RT5081_PMU_FLEDMASK1		(0xE9)
#define RT5081_PMU_FLEDMASK2		(0xEA)
#define RT5081_PMU_BASEMASK		(0xEB)
#define RT5081_PMU_LDOMASK		(0xEC)
#define RT5081_PMU_RGBMASK		(0xED)
#define RT5081_PMU_BLMASK		(0xEE)
#define RT5081_PMU_DBMASK		(0xEF)

/* general io ops start */
extern int rt5081_pmu_reg_read(struct rt5081_pmu_chip *chip, u8 addr);
extern int rt5081_pmu_reg_write(struct rt5081_pmu_chip *chip,
	u8 addr, u8 data);
extern int rt5081_pmu_reg_update_bits(struct rt5081_pmu_chip *chip, u8 addr,
	u8 mask, u8 data);

static inline int rt5081_pmu_reg_set_bit(struct rt5081_pmu_chip *chip, u8 addr,
		u8 mask)
{
	return rt5081_pmu_reg_update_bits(chip, addr, mask, mask);
}

static inline int rt5081_pmu_reg_clr_bit(struct rt5081_pmu_chip *chip, u8 addr,
		u8 mask)
{
	return rt5081_pmu_reg_update_bits(chip, addr, mask, 0x00);
}

extern int rt5081_pmu_reg_block_read(struct rt5081_pmu_chip *chip, u8 addr,
	int len, u8 *dest);
extern int rt5081_pmu_reg_block_write(struct rt5081_pmu_chip *chip, u8 addr,
	int len, const u8 *src);
/* general io ops end */
/* extern function from other files start */
extern int rt5081_pmu_regmap_register(struct rt5081_pmu_chip *chip,
	struct rt_regmap_fops *regmap_ops);
extern void rt5081_pmu_regmap_unregister(struct rt5081_pmu_chip *chip);
extern int rt5081_pmu_get_virq_number(struct rt5081_pmu_chip *chip,
	const char *name);
extern const char *rt5081_pmu_get_hwirq_name(struct rt5081_pmu_chip *chip,
	int id);
extern int rt5081_pmu_irq_register(struct rt5081_pmu_chip *chip);
extern void rt5081_pmu_irq_unregister(struct rt5081_pmu_chip *chip);
extern void rt5081_pmu_irq_suspend(struct rt5081_pmu_chip *chip);
extern void rt5081_pmu_irq_resume(struct rt5081_pmu_chip *chip);
extern int rt5081_pmu_subdevs_register(struct rt5081_pmu_chip *chip);
extern void rt5081_pmu_subdevs_unregister(struct rt5081_pmu_chip *chip);
/* extern function from other files end */

#endif /* #ifndef __LINUX_MFD_RT5081_PMU_H */
