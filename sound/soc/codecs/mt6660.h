/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SND_SOC_MT6660_H
#define __SND_SOC_MT6660_H

#include <linux/mutex.h>

struct dbg_internal {
	struct dentry *rt_root;
	struct dentry *ic_root;
	bool rt_dir_create;
	struct mutex io_lock;
	u16 reg;
	u16 size;
	u16 data_buffer_size;
	void *data_buffer;
};

struct dbg_info {
	const char *dirname;
	const char *devname;
	const char *typestr;
	void *io_drvdata;
	int (*io_read)(void *drvdata, u16 reg, void *val, u16 size);
	int (*io_write)(void *drvdata, u16 reg, const void *val, u16 size);
	struct dbg_internal internal;
};

struct mt6660_chip {
	struct i2c_client *i2c;
	struct device *dev;
	struct snd_soc_component *component;
	struct platform_device *param_dev;
	struct mutex var_lock;
	struct mutex io_lock;
	struct dbg_info dbg_info;
	u16 chip_rev;
	int pwr_cnt;
};

int mt6660_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
int mt6660_i2c_remove(struct i2c_client *client);

#define MT6660_REG_DEVID		(0x00)
#define MT6660_REG_SYSTEM_CTRL		(0x03)
#define MT6660_REG_IRQ_STATUS1		(0x05)
#define MT6660_REG_ADDA_CLOCK		(0x07)
#define MT6660_REG_SERIAL_CFG1		(0x10)
#define MT6660_REG_DATAO_SEL		(0x12)
#define MT6660_REG_TDM_CFG3		(0x15)
#define MT6660_REG_HPF_CTRL		(0x18)
#define MT6660_REG_HPF1_COEF		(0x1A)
#define MT6660_REG_HPF2_COEF		(0x1B)
#define MT6660_REG_PATH_BYPASS		(0x1E)
#define MT6660_REG_WDT_CTRL		(0x20)
#define MT6660_REG_HCLIP_CTRL		(0x24)
#define MT6660_REG_VOL_CTRL		(0x29)
#define MT6660_REG_SPS_CTRL		(0x30)
#define MT6660_REG_SIGMAX		(0x33)
#define MT6660_REG_CALI_T0		(0x3F)
#define MT6660_REG_BST_CTRL		(0x40)
#define MT6660_REG_PROTECTION_CFG	(0x46)
#define MT6660_REG_DA_GAIN		(0x4c)
#define MT6660_REG_AUDIO_IN2_SEL	(0x50)
#define MT6660_REG_SIG_GAIN		(0x51)
#define MT6660_REG_PLL_CFG1		(0x60)
#define MT6660_REG_DRE_CTRL		(0x68)
#define MT6660_REG_DRE_THDMODE		(0x69)
#define MT6660_REG_DRE_CORASE		(0x6B)
#define MT6660_REG_PWM_CTRL		(0x70)
#define MT6660_REG_DC_PROTECT_CTRL	(0x74)
#define MT6660_REG_ADC_USB_MODE		(0x7c)
#define MT6660_REG_INTERNAL_CFG		(0x88)
#define MT6660_REG_RESV0		(0x98)
#define MT6660_REG_RESV1		(0x99)
#define MT6660_REG_RESV2		(0x9A)
#define MT6660_REG_RESV3		(0x9B)
#define MT6660_REG_RESV6		(0xA2)
#define MT6660_REG_RESV7		(0xA3)
#define MT6660_REG_RESV10		(0xB0)
#define MT6660_REG_RESV11		(0xB1)
#define MT6660_REG_RESV16		(0xB6)
#define MT6660_REG_RESV17		(0xB7)
#define MT6660_REG_RESV19		(0xB9)
#define MT6660_REG_RESV21		(0xBB)
#define MT6660_REG_RESV23		(0xBD)
#define MT6660_REG_RESV31		(0xD3)
#define MT6660_REG_RESV40		(0xE0)

#endif /* __SND_SOC_MT6660_H */
