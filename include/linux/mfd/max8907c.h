/* linux/mfd/max8907c.h
 *
 * Functions to access MAX8907C power management chip.
 *
 * Copyright (C) 2010 Gyungoh Yoo <jack.yoo@maxim-ic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MFD_MAX8907C_H
#define __LINUX_MFD_MAX8907C_H

/* MAX8907C register map */
#define MAX8907C_REG_SYSENSEL               0x00
#define MAX8907C_REG_ON_OFF_IRQ1            0x01
#define MAX8907C_REG_ON_OFF_IRQ1_MASK       0x02
#define MAX8907C_REG_ON_OFF_STAT            0x03
#define MAX8907C_REG_SDCTL1                 0x04
#define MAX8907C_REG_SDSEQCNT1              0x05
#define MAX8907C_REG_SDV1                   0x06
#define MAX8907C_REG_SDCTL2                 0x07
#define MAX8907C_REG_SDSEQCNT2              0x08
#define MAX8907C_REG_SDV2                   0x09
#define MAX8907C_REG_SDCTL3                 0x0A
#define MAX8907C_REG_SDSEQCNT3              0x0B
#define MAX8907C_REG_SDV3                   0x0C
#define MAX8907C_REG_ON_OFF_IRQ2            0x0D
#define MAX8907C_REG_ON_OFF_IRQ2_MASK       0x0E
#define MAX8907C_REG_RESET_CNFG             0x0F
#define MAX8907C_REG_LDOCTL16               0x10
#define MAX8907C_REG_LDOSEQCNT16            0x11
#define MAX8907C_REG_LDO16VOUT              0x12
#define MAX8907C_REG_SDBYSEQCNT             0x13
#define MAX8907C_REG_LDOCTL17               0x14
#define MAX8907C_REG_LDOSEQCNT17            0x15
#define MAX8907C_REG_LDO17VOUT              0x16
#define MAX8907C_REG_LDOCTL1                0x18
#define MAX8907C_REG_LDOSEQCNT1             0x19
#define MAX8907C_REG_LDO1VOUT               0x1A
#define MAX8907C_REG_LDOCTL2                0x1C
#define MAX8907C_REG_LDOSEQCNT2             0x1D
#define MAX8907C_REG_LDO2VOUT               0x1E
#define MAX8907C_REG_LDOCTL3                0x20
#define MAX8907C_REG_LDOSEQCNT3             0x21
#define MAX8907C_REG_LDO3VOUT               0x22
#define MAX8907C_REG_LDOCTL4                0x24
#define MAX8907C_REG_LDOSEQCNT4             0x25
#define MAX8907C_REG_LDO4VOUT               0x26
#define MAX8907C_REG_LDOCTL5                0x28
#define MAX8907C_REG_LDOSEQCNT5             0x29
#define MAX8907C_REG_LDO5VOUT               0x2A
#define MAX8907C_REG_LDOCTL6                0x2C
#define MAX8907C_REG_LDOSEQCNT6             0x2D
#define MAX8907C_REG_LDO6VOUT               0x2E
#define MAX8907C_REG_LDOCTL7                0x30
#define MAX8907C_REG_LDOSEQCNT7             0x31
#define MAX8907C_REG_LDO7VOUT               0x32
#define MAX8907C_REG_LDOCTL8                0x34
#define MAX8907C_REG_LDOSEQCNT8             0x35
#define MAX8907C_REG_LDO8VOUT               0x36
#define MAX8907C_REG_LDOCTL9                0x38
#define MAX8907C_REG_LDOSEQCNT9             0x39
#define MAX8907C_REG_LDO9VOUT               0x3A
#define MAX8907C_REG_LDOCTL10               0x3C
#define MAX8907C_REG_LDOSEQCNT10            0x3D
#define MAX8907C_REG_LDO10VOUT              0x3E
#define MAX8907C_REG_LDOCTL11               0x40
#define MAX8907C_REG_LDOSEQCNT11            0x41
#define MAX8907C_REG_LDO11VOUT              0x42
#define MAX8907C_REG_LDOCTL12               0x44
#define MAX8907C_REG_LDOSEQCNT12            0x45
#define MAX8907C_REG_LDO12VOUT              0x46
#define MAX8907C_REG_LDOCTL13               0x48
#define MAX8907C_REG_LDOSEQCNT13            0x49
#define MAX8907C_REG_LDO13VOUT              0x4A
#define MAX8907C_REG_LDOCTL14               0x4C
#define MAX8907C_REG_LDOSEQCNT14            0x4D
#define MAX8907C_REG_LDO14VOUT              0x4E
#define MAX8907C_REG_LDOCTL15               0x50
#define MAX8907C_REG_LDOSEQCNT15            0x51
#define MAX8907C_REG_LDO15VOUT              0x52
#define MAX8907C_REG_OUT5VEN                0x54
#define MAX8907C_REG_OUT5VSEQ               0x55
#define MAX8907C_REG_OUT33VEN               0x58
#define MAX8907C_REG_OUT33VSEQ              0x59
#define MAX8907C_REG_LDOCTL19               0x5C
#define MAX8907C_REG_LDOSEQCNT19            0x5D
#define MAX8907C_REG_LDO19VOUT              0x5E
#define MAX8907C_REG_LBCNFG                 0x60
#define MAX8907C_REG_SEQ1CNFG               0x64
#define MAX8907C_REG_SEQ2CNFG               0x65
#define MAX8907C_REG_SEQ3CNFG               0x66
#define MAX8907C_REG_SEQ4CNFG               0x67
#define MAX8907C_REG_SEQ5CNFG               0x68
#define MAX8907C_REG_SEQ6CNFG               0x69
#define MAX8907C_REG_SEQ7CNFG               0x6A
#define MAX8907C_REG_LDOCTL18               0x72
#define MAX8907C_REG_LDOSEQCNT18            0x73
#define MAX8907C_REG_LDO18VOUT              0x74
#define MAX8907C_REG_BBAT_CNFG              0x78
#define MAX8907C_REG_CHG_CNTL1              0x7C
#define MAX8907C_REG_CHG_CNTL2              0x7D
#define MAX8907C_REG_CHG_IRQ1               0x7E
#define MAX8907C_REG_CHG_IRQ2               0x7F
#define MAX8907C_REG_CHG_IRQ1_MASK          0x80
#define MAX8907C_REG_CHG_IRQ2_MASK          0x81
#define MAX8907C_REG_CHG_STAT               0x82
#define MAX8907C_REG_WLED_MODE_CNTL         0x84
#define MAX8907C_REG_ILED_CNTL              0x84
#define MAX8907C_REG_II1RR                  0x8E
#define MAX8907C_REG_II2RR                  0x8F
#define MAX8907C_REG_LDOCTL20               0x9C
#define MAX8907C_REG_LDOSEQCNT20            0x9D
#define MAX8907C_REG_LDO20VOUT              0x9E

/* RTC register */
#define MAX8907C_REG_RTC_SEC                0x00
#define MAX8907C_REG_RTC_MIN                0x01
#define MAX8907C_REG_RTC_HOURS              0x02
#define MAX8907C_REG_RTC_WEEKDAY            0x03
#define MAX8907C_REG_RTC_DATE               0x04
#define MAX8907C_REG_RTC_MONTH              0x05
#define MAX8907C_REG_RTC_YEAR1              0x06
#define MAX8907C_REG_RTC_YEAR2              0x07
#define MAX8907C_REG_ALARM0_SEC             0x08
#define MAX8907C_REG_ALARM0_MIN             0x09
#define MAX8907C_REG_ALARM0_HOURS           0x0A
#define MAX8907C_REG_ALARM0_WEEKDAY         0x0B
#define MAX8907C_REG_ALARM0_DATE            0x0C
#define MAX8907C_REG_ALARM0_MONTH           0x0D
#define MAX8907C_REG_ALARM0_YEAR1           0x0E
#define MAX8907C_REG_ALARM0_YEAR2           0x0F
#define MAX8907C_REG_ALARM1_SEC             0x10
#define MAX8907C_REG_ALARM1_MIN             0x11
#define MAX8907C_REG_ALARM1_HOURS           0x12
#define MAX8907C_REG_ALARM1_WEEKDAY         0x13
#define MAX8907C_REG_ALARM1_DATE            0x14
#define MAX8907C_REG_ALARM1_MONTH           0x15
#define MAX8907C_REG_ALARM1_YEAR1           0x16
#define MAX8907C_REG_ALARM1_YEAR2           0x17
#define MAX8907C_REG_ALARM0_CNTL            0x18
#define MAX8907C_REG_ALARM1_CNTL            0x19
#define MAX8907C_REG_RTC_STATUS             0x1A
#define MAX8907C_REG_RTC_CNTL               0x1B
#define MAX8907C_REG_RTC_IRQ                0x1C
#define MAX8907C_REG_RTC_IRQ_MASK           0x1D
#define MAX8907C_REG_MPL_CNTL               0x1E

/* ADC and Touch Screen Controller register map */

#define MAX8907C_CTL     0
#define MAX8907C_SEQCNT  1
#define MAX8907C_VOUT    2

/* mask bit fields */
#define MAX8907C_MASK_LDO_SEQ           0x1C
#define MAX8907C_MASK_LDO_EN            0x01
#define MAX8907C_MASK_VBBATTCV          0x03
#define MAX8907C_MASK_OUT5V_VINEN       0x10
#define MAX8907C_MASK_OUT5V_ENSRC       0x0E
#define MAX8907C_MASK_OUT5V_EN          0x01

/* Power off bit in RESET_CNFG reg */
#define MAX8907C_MASK_POWER_OFF		0x40

#define MAX8907C_MASK_PWR_EN		0x80
#define MAX8907C_MASK_CTL_SEQ		0x1C

#define MAX8907C_PWR_EN			0x80
#define MAX8907C_CTL_SEQ		0x04

#define MAX8907C_SD_SEQ1		0x02
#define MAX8907C_SD_SEQ2		0x06

#define MAX8907C_DELAY_CNT0		0x00

#define MAX8907C_POWER_UP_DELAY_CNT1	0x10
#define MAX8907C_POWER_UP_DELAY_CNT12	0xC0

#define MAX8907C_POWER_DOWN_DELAY_CNT12	0x0C

#define RTC_I2C_ADDR			0x68

/*
 * MAX8907B revision requires s/w WAR to connect PWREN input to
 * sequencer 2 because of the bug in the silicon.
 */
#define MAX8907B_II2RR_PWREN_WAR		(0x12)

/* Defines common for all supplies PWREN  sequencer selection */
#define MAX8907B_SEQSEL_PWREN_LXX		1 /* SEQ2 (PWREN) */

/* IRQ definitions */
enum {
	MAX8907C_IRQ_VCHG_DC_OVP,
	MAX8907C_IRQ_VCHG_DC_F,
	MAX8907C_IRQ_VCHG_DC_R,
	MAX8907C_IRQ_VCHG_THM_OK_R,
	MAX8907C_IRQ_VCHG_THM_OK_F,
	MAX8907C_IRQ_VCHG_MBATTLOW_F,
	MAX8907C_IRQ_VCHG_MBATTLOW_R,
	MAX8907C_IRQ_VCHG_RST,
	MAX8907C_IRQ_VCHG_DONE,
	MAX8907C_IRQ_VCHG_TOPOFF,
	MAX8907C_IRQ_VCHG_TMR_FAULT,
	MAX8907C_IRQ_GPM_RSTIN,
	MAX8907C_IRQ_GPM_MPL,
	MAX8907C_IRQ_GPM_SW_3SEC,
	MAX8907C_IRQ_GPM_EXTON_F,
	MAX8907C_IRQ_GPM_EXTON_R,
	MAX8907C_IRQ_GPM_SW_1SEC,
	MAX8907C_IRQ_GPM_SW_F,
	MAX8907C_IRQ_GPM_SW_R,
	MAX8907C_IRQ_GPM_SYSCKEN_F,
	MAX8907C_IRQ_GPM_SYSCKEN_R,
	MAX8907C_IRQ_RTC_ALARM1,
	MAX8907C_IRQ_RTC_ALARM0,
	MAX8907C_NR_IRQS,
};

struct max8907c {
	struct device 		*dev;
	struct mutex 		io_lock;
	struct mutex		irq_lock;
	struct i2c_client 	*i2c_power;
	struct i2c_client 	*i2c_rtc;
	int			irq_base;
	int			core_irq;

	unsigned char 		cache_chg[2];
	unsigned char 		cache_on[2];
	unsigned char 		cache_rtc;

};

struct max8907c_platform_data {
	int num_subdevs;
	struct platform_device **subdevs;
	int irq_base;
	int (*max8907c_setup)(void);
	bool use_power_off;
};

int max8907c_reg_read(struct i2c_client *i2c, u8 reg);
int max8907c_reg_bulk_read(struct i2c_client *i2c, u8 reg, u8 count, u8 *val);
int max8907c_reg_write(struct i2c_client *i2c, u8 reg, u8 val);
int max8907c_reg_bulk_write(struct i2c_client *i2c, u8 reg, u8 count, u8 *val);
int max8907c_set_bits(struct i2c_client *i2c, u8 reg, u8 mask, u8 val);

int max8907c_irq_init(struct max8907c *chip, int irq, int irq_base);
void max8907c_irq_free(struct max8907c *chip);
int max8907c_suspend(struct device *dev);
int max8907c_resume(struct device *dev);
void max8907c_deep_sleep(int enter);
int max8907c_pwr_en_config(void);
int max8907c_pwr_en_attach(void);
#endif
