/*
 * rt5671-dsp.h  --  RT5671 ALSA SoC DSP driver
 *
 * Copyright 2011 Realtek Microelectronics
 * Copyright (C) 2016 XiaoMi, Inc.
 * Author: Johnny Hsu <johnnyhsu@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RT5671_DSP_H__
#define __RT5671_DSP_H__

#define RT5671_DSP_CTRL1		0xe0
#define RT5671_DSP_CTRL2		0xe1
#define RT5671_DSP_CTRL3		0xe2
#define RT5671_DSP_CTRL4		0xe3
#define RT5671_DSP_CTRL5		0xe4

/* DSP Control 1 (0xe0) */
#define RT5671_DSP_CMD_MASK		(0xff << 8)
#define RT5671_DSP_CMD_PE		(0x0d << 8)	/* Patch Entry */
#define RT5671_DSP_CMD_MW		(0x3b << 8)	/* Memory Write */
#define RT5671_DSP_CMD_MR		(0x37 << 8)	/* Memory Read */
#define RT5671_DSP_CMD_RR		(0x60 << 8)	/* Register Read */
#define RT5671_DSP_CMD_RW		(0x68 << 8)	/* Register Write */
#define RT5671_DSP_REG_DATHI		(0x26 << 8)	/* High Data Addr */
#define RT5671_DSP_REG_DATLO		(0x25 << 8)	/* Low Data Addr */
#define RT5671_DSP_CLK_MASK		(0x3 << 6)
#define RT5671_DSP_CLK_SFT		6
#define RT5671_DSP_CLK_768K		(0x0 << 6)
#define RT5671_DSP_CLK_384K		(0x1 << 6)
#define RT5671_DSP_CLK_192K		(0x2 << 6)
#define RT5671_DSP_CLK_96K		(0x3 << 6)
#define RT5671_DSP_BUSY_MASK		(0x1 << 5)
#define RT5671_DSP_RW_MASK		(0x1 << 4)
#define RT5671_DSP_DL_MASK		(0x3 << 2)
#define RT5671_DSP_DL_0			(0x0 << 2)
#define RT5671_DSP_DL_1			(0x1 << 2)
#define RT5671_DSP_DL_2			(0x2 << 2)
#define RT5671_DSP_DL_3			(0x3 << 2)
#define RT5671_DSP_I2C_AL_16		(0x1 << 1)
#define RT5671_DSP_CMD_EN		(0x1)

/* Debug String Length */
#define RT5671_DSP_REG_DISP_LEN 25


enum {
	RT5671_DSP_DIS,
	RT5671_DSP_TWO_MIC_AEC,
	RT5671_DSP_TWO_MIC_EZAEC,
	RT5671_DSP_ONE_MIC_AEC,
	RT5671_DSP_ONE_MIC_EZAEC,
	RT5671_DSP_BM_48K_STO_FFP_NS,
	RT5671_DSP_HS_AEC,
	RT5671_DSP_HS_48K_STO_FFP_NS,
	RT5671_DSP_BT_AEC,
	RT5671_DSP_BT_EZAEC,
};

struct rt5671_dsp_param {
	u16 cmd_fmt;
	u16 addr;
	u16 data;
	u8 cmd;
};

int rt5671_dsp_probe(struct snd_soc_codec *codec);
int rt5671_dsp_ioctl_common(struct snd_hwdep *hw,
	struct file *file, unsigned int cmd, unsigned long arg);
#ifdef CONFIG_PM
int rt5671_dsp_suspend(struct snd_soc_codec *codec);
int rt5671_dsp_resume(struct snd_soc_codec *codec);
#endif
unsigned int rt5671_dsp_read(struct snd_soc_codec *codec, unsigned int reg);
int rt5671_dsp_write(struct snd_soc_codec *codec, unsigned int addr, unsigned int data);

#endif /* __RT5671_DSP_H__ */

