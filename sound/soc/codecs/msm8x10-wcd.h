/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef MSM8X10_WCD_H
#define MSM8X10_WCD_H

#include <sound/soc.h>
#include <sound/jack.h>
#include "wcd9xxx-mbhc.h"
#include "wcd9xxx-resmgr.h"
#include <linux/mfd/wcd9xxx/pdata.h>

#define MSM8X10_WCD_NUM_REGISTERS	0x600
#define MSM8X10_WCD_MAX_REGISTER	(MSM8X10_WCD_NUM_REGISTERS-1)
#define MSM8X10_WCD_CACHE_SIZE		MSM8X10_WCD_NUM_REGISTERS
#define MSM8X10_WCD_NUM_IRQ_REGS	3
#define MAX_REGULATOR				7
#define MSM8X10_WCD_REG_VAL(reg, val)		{reg, 0, val}
#define MSM8X10_DINO_LPASS_AUDIO_CORE_DIG_CODEC_CLK_SEL	0xFE03B004
#define MSM8X10_DINO_LPASS_DIGCODEC_CMD_RCGR			0xFE02C000
#define MSM8X10_DINO_LPASS_DIGCODEC_CFG_RCGR			0xFE02C004
#define MSM8X10_DINO_LPASS_DIGCODEC_M				0xFE02C008
#define MSM8X10_DINO_LPASS_DIGCODEC_N				0xFE02C00C
#define MSM8X10_DINO_LPASS_DIGCODEC_D				0xFE02C010
#define MSM8X10_DINO_LPASS_DIGCODEC_CBCR			0xFE02C014
#define MSM8X10_DINO_LPASS_DIGCODEC_AHB_CBCR			0xFE02C018

#define MSM8X10_CODEC_NAME "msm8x10_wcd_codec"

#define MSM8X10_WCD_IS_DINO_REG(reg) \
	(((reg >= 0x400) && (reg <= 0x5FF)) ? 1 : 0)
#define MSM8X10_WCD_IS_HELICON_REG(reg) \
	(((reg >= 0x000) && (reg <= 0x1FF)) ? 1 : 0)
extern const u8 msm8x10_wcd_reg_readable[MSM8X10_WCD_CACHE_SIZE];
extern const u8 msm8x10_wcd_reset_reg_defaults[MSM8X10_WCD_CACHE_SIZE];
struct msm8x10_wcd_codec_dai_data {
	u32 rate;
	u32 *ch_num;
	u32 ch_act;
	u32 ch_tot;
};

enum msm8x10_wcd_pid_current {
	MSM8X10_WCD_PID_MIC_2P5_UA,
	MSM8X10_WCD_PID_MIC_5_UA,
	MSM8X10_WCD_PID_MIC_10_UA,
	MSM8X10_WCD_PID_MIC_20_UA,
};

struct msm8x10_wcd_reg_mask_val {
	u16	reg;
	u8	mask;
	u8	val;
};

enum msm8x10_wcd_mbhc_analog_pwr_cfg {
	MSM8X10_WCD_ANALOG_PWR_COLLAPSED = 0,
	MSM8X10_WCD_ANALOG_PWR_ON,
	MSM8X10_WCD_NUM_ANALOG_PWR_CONFIGS,
};

/* Number of input and output Slimbus port */
enum {
	MSM8X10_WCD_RX1 = 0,
	MSM8X10_WCD_RX2,
	MSM8X10_WCD_RX3,
	MSM8X10_WCD_RX_MAX,
};

enum {
	MSM8X10_WCD_TX1 = 0,
	MSM8X10_WCD_TX2,
	MSM8X10_WCD_TX3,
	MSM8X10_WCD_TX4,
	MSM8X10_WCD_TX_MAX,
};

enum {
	/* INTR_REG 0 */
	MSM8X10_WCD_IRQ_RESERVED_0 = 0,
	MSM8X10_WCD_IRQ_MBHC_REMOVAL,
	MSM8X10_WCD_IRQ_MBHC_SHORT_TERM,
	MSM8X10_WCD_IRQ_MBHC_PRESS,
	MSM8X10_WCD_IRQ_MBHC_RELEASE,
	MSM8X10_WCD_IRQ_MBHC_POTENTIAL,
	MSM8X10_WCD_IRQ_MBHC_INSERTION,
	MSM8X10_WCD_IRQ_MBHC_HS_DET,
	/* INTR_REG 1 */
	MSM8X10_WCD_IRQ_PA_STARTUP,
	MSM8X10_WCD_IRQ_BG_PRECHARGE,
	MSM8X10_WCD_IRQ_RESERVED_1,
	MSM8X10_WCD_IRQ_EAR_PA_OCPL_FAULT,
	MSM8X10_WCD_IRQ_EAR_PA_STARTUP,
	MSM8X10_WCD_IRQ_SPKR_PA_OCPL_FAULT,
	MSM8X10_WCD_IRQ_SPKR_CLIP_FAULT,
	MSM8X10_WCD_IRQ_RESERVED_2,
	/* INTR_REG 2 */
	MSM8X10_WCD_IRQ_HPH_L_PA_STARTUP,
	MSM8X10_WCD_IRQ_HPH_R_PA_STARTUP,
	MSM8X10_WCD_IRQ_HPH_PA_OCPL_FAULT,
	MSM8X10_WCD_IRQ_HPH_PA_OCPR_FAULT,
	MSM8X10_WCD_IRQ_RESERVED_3,
	MSM8X10_WCD_IRQ_RESERVED_4,
	MSM8X10_WCD_IRQ_RESERVED_5,
	MSM8X10_WCD_IRQ_RESERVED_6,
	MSM8X10_WCD_NUM_IRQS,
};

struct msm8x10_wcd_ocp_setting {
	unsigned int	use_pdata:1; /* 0 - use sys default as recommended */
	unsigned int	num_attempts:4; /* up to 15 attempts */
	unsigned int	run_time:4; /* in duty cycle */
	unsigned int	wait_time:4; /* in duty cycle */
	unsigned int	hph_ocp_limit:3; /* Headphone OCP current limit */
};

struct msm8x10_wcd_regulator {
	const char *name;
	int min_uV;
	int max_uV;
	int optimum_uA;
	bool ondemand;
	struct regulator *regulator;
};

struct msm8x10_wcd_pdata {
	int irq;
	int irq_base;
	int num_irqs;
	int reset_gpio;
	void *msm8x10_wcd_ahb_base_vaddr;
	struct wcd9xxx_micbias_setting micbias;
	struct msm8x10_wcd_ocp_setting ocp;
	struct msm8x10_wcd_regulator regulator[MAX_REGULATOR];
	u32 mclk_rate;
};

enum msm8x10_wcd_micbias_num {
	MSM8X10_WCD_MICBIAS1 = 0,
};

enum msm8x10_wcd_pm_state {
	MSM8X10_WCD_PM_SLEEPABLE,
	MSM8X10_WCD_PM_AWAKE,
	MSM8X10_WCD_PM_ASLEEP,
};

struct msm8x10_wcd {
	struct device *dev;
	struct mutex io_lock;
	struct mutex xfer_lock;
	u8 version;

	int reset_gpio;
	int (*read_dev)(struct msm8x10_wcd *msm8x10,
			unsigned short reg);
	int (*write_dev)(struct msm8x10_wcd *msm8x10,
			 unsigned short reg, u8 val);

	u32 num_of_supplies;
	struct regulator_bulk_data *supplies;

	u8 idbyte[4];

	int num_irqs;
	u32 mclk_rate;
	char __iomem *pdino_base;

	struct wcd9xxx_core_resource wcd9xxx_res;
};

extern int msm8x10_wcd_mclk_enable(struct snd_soc_codec *codec, int mclk_enable,
			     bool dapm);
extern int msm8x10_wcd_hs_detect(struct snd_soc_codec *codec,
			struct wcd9xxx_mbhc_config *mbhc_cfg);

#endif
