/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_MSM_AUDIO_DMA_H


#define BANK_OFFSET			0x1000

#define LPAIF_PCM_CTL_OFFSET		0x0000
	#define CTRL_DATA_OE		(1 << 18)
	#define RATE_8KHZ		(0 << 15)
	#define RATE_16KHZ		(1 << 15)
	#define RATE_32KHZ		(2 << 15)
	#define RATE_64KHZ		(4 << 15)
	#define RATE_128KHZ		(8 << 15)
	#define RATE_256KHZ		(9 << 15)
	#define PCM_LOOPBACK		(1 << 14)
	#define SYNC_SRC_INT		(0 << 13)
	#define SYNC_SRC_EXT		(1 << 13)
	#define PCM_MODE		(0 << 12)
	#define AUX_MODE		(1 << 12)
	#define RPCM_WIDTH_8		(0 << 11)
	#define RPCM_WIDTH_16		(1 << 11)
	#define TPCM_WIDTH_8		(0 << 10)
	#define TPCM_WIDTH_16		(1 << 10)
	#define	RPCM_SLOT(x)		(x << 5)
	#define	TPCM_SLOT(x)		 x

#define LPAIF_I2S_CTL_OFFSET(x)		(0x0004 + (0x4 * x))
	#define I2S_LOOPBACK		(1 << 15)
	#define SPK_EN_DISABLE		(0 << 14)
	#define SPK_EN_ENABLE		(1 << 14)
	#define SPK_MODE_NONE		(0 << 10)
	#define SPK_MODE_SD0		(1 << 10)
	#define SPK_MODE_SD1		(2 << 10)
	#define SPK_MODE_SD2		(3 << 10)
	#define SPK_MODE_SD3		(4 << 10)
	#define SPK_MODE_QUAD01		(5 << 10)
	#define SPK_MODE_QUAD23		(6 << 10)
	#define SPK_MODE_6CH		(7 << 10)
	#define SPK_MODE_8CH		(8 << 10)
	#define	SPK_MONO_STEREO		(0 << 9)
	#define	SPK_MONO_MONO		(1 << 9)
	#define MIC_EN_DISABLE		(0 << 8)
	#define MIC_EN_ENABLE		(1 << 8)
	#define MIC_MODE_NONE		(0 << 4)
	#define MIC_MODE_SD0		(1 << 4)
	#define MIC_MODE_SD1		(2 << 4)
	#define MIC_MODE_SD2		(3 << 4)
	#define MIC_MODE_SD3		(4 << 4)
	#define MIC_MODE_QUAD01		(5 << 4)
	#define MIC_MODE_QUAD23		(6 << 4)
	#define MIC_MODE_6CH		(7 << 4)
	#define MIC_MODE_8CH		(8 << 4)
	#define	MIC_MONO_STEREO		(0 << 3)
	#define	MIC_MONO_MONO		(1 << 3)
	#define WS_SRC_INT		(0 << 2)
	#define WS_SRC_EXT		(1 << 2)
	#define BIT_WIDTH_16		(0 << 0)
	#define BIT_WIDTH_24		(1 << 0)
	#define BIT_WIDTH_32		(2 << 0)

#define LPAIF_DMIC_CTL			0x0018
	#define DMIC_EN_DISABLE		(0 << 4)
	#define DMIC_EN_ENABLE		(1 << 4)
	#define DMIC_MODE_NONE		(0 << 1)
	#define DMIC_MODE_LEFT0		(1 << 1)
	#define DMIC_MODE_RIGHT0	(2 << 1)
	#define DMIC_MODE_LEFT1		(3 << 1)
	#define DMIC_MODE_RIGHT1	(4 << 1)
	#define DMIC_MODE_STEREO0	(5 << 1)
	#define DMIC_MODE_STEREO1	(6 << 1)
	#define DMIC_MODE_QUAD		(7 << 1)
	#define BIT_WIDTH_DMIC_16	(0 << 0)
	#define BIT_WIDTH_DMIC_20	(1 << 0)

#define LPAIF_DMIC_VOL_CTL(x)		(0x001c + (0x4 * x))
	#define UPDATE_STATUS_COMP	(0 << 20)
	#define UPDATE_STATUS_PEND	(1 << 20) /* Timeout or Zero Crossing */
	#define UPDATE_GAIN_NO		(0 << 19)
	#define UPDATE_GAIN_YES		(1 << 19)
	#define TX_HPF_BP_DC_BLOCK	(0 << 18)
	#define TX_HPF_BP_BYPASS_DC_BLOCK	(1 << 18)
	#define DMIC_GAIN_BP_GAIN	(0 << 17)
	#define DMIC_GAIN_BP_BYPASS_GAIN	(1 << 17)
	#define MUTE_EN_NORMAL		(0 << 16)
	#define MUTE_EN_MUTE		(1 << 16)
	#define TIMEOUT_VAL(x)		(x << 8)
	#define DMIC_GAIN_MUL(x)	(x << 0)

#define LPAIF_SPARE			0x0030

#define LPAIF_WRDMA_LPBK_MIX		0x1000
	#define WRDMA_LPBK_MIX_BLOCK(x)	(0 << (x - 5))
	#define WRDMA_LPBK_MIX_ALLOW(x)	(1 << (x - 5))

#define LPAIF_DEBUG_CTL			0x1004
	#define TESTMODE_OFF		(0 << 4)
	#define TESTMODE_ON		(1 << 4)
	#define TESTSEL_CH0		(0 << 0)
	#define TESTSEL_CH1		(1 << 0)
	#define TESTSEL_CH2		(2 << 0)
	#define TESTSEL_CH3		(3 << 0)
	#define TESTSEL_CH4		(4 << 0)
	#define TESTSEL_CH5		(5 << 0)
	#define TESTSEL_CH6		(6 << 0)
	#define TESTSEL_CH7		(7 << 0)
	#define TESTSEL_CH8		(8 << 0)
	#define TESTSEL_MIXER		(9 << 0)
	#define TESTSEL_CODEC_SPKR	(10 << 0)
	#define TESTSEL_CODEC_MIC	(11 << 0)
	#define TESTSEL_MI2S		(12 << 0)
	#define TESTSEL_SEC_SPKR	(13 << 0)
	#define TESTSEL_SEC_MIC		(14 << 0)
	#define TESTSEL_DMIC		(15 << 0)

#define LPAIF_MIXER_CTL			0x2000
	#define OVR_DETECTED_NO		(0 << 10)
	#define OVR_DETECTED_YES	(1 << 10)
	#define OVR_CLR_NO		(0 << 9)
	#define OVR_CLR_YES		(1 << 9)
	#define SAT_EN_DISABLE		(0 << 8)
	#define SAT_EN_ENABLE		(1 << 8)
	#define MIXER_BIT_WIDTH_8	(0 << 6)
	#define MIXER_BIT_WIDTH_16	(1 << 6)
	#define MIXER_BIT_WIDTH_24	(2 << 6)
	#define MIXER_BIT_WIDTH_32	(3 << 6)
	#define PORT1_CH_NONE		(0 << 3)
	#define PORT1_CH_0		(1 << 3)
	#define PORT1_CH_1		(2 << 3)
	#define PORT1_CH_2		(3 << 3)
	#define PORT1_CH_3		(4 << 3)
	#define PORT1_CH_4		(5 << 3)
	#define PORT0_CH_NONE		(0 << 0)
	#define PORT0_CH_0		(1 << 0)
	#define PORT0_CH_1		(2 << 0)
	#define PORT0_CH_2		(3 << 0)
	#define PORT0_CH_3		(4 << 0)
	#define PORT0_CH_4		(5 << 0)

#define DMA_IRQ_BASE			0x3000
#define DMA_IRQ_INDEX(x)		(BANK_OFFSET * x)
#define DMA_IRQ_ADDR(irq, addr)		(DMA_IRQ_BASE  \
					+ DMA_IRQ_INDEX(irq) + addr)

/* Audio Interrupt registers for DMA channel confuguration */
#define LPAIF_IRQ_EN(x)			DMA_IRQ_ADDR(x, 0x00)
#define LPAIF_IRQ_STAT(x)		DMA_IRQ_ADDR(x, 0x04)
#define	LPAIF_IRQ_RAW_STAT(x)		DMA_IRQ_ADDR(x, 0x08)
#define LPAIF_IRQ_CLEAR(x)		DMA_IRQ_ADDR(x, 0x0c)
#define LPAIF_IRQ_FORCE(x)		DMA_IRQ_ADDR(x, 0x10)
	#define PER_CH(x)		(1 << (3 * x))
	#define UNDER_CH(x)		(2 << (3 * x))
	#define ERR_CH(x)		(4 << (3 * x))

/* Audio DMA registers for DMA channel confuguration */
#define DMA_CH_CTL_BASE			0x6000
#define DMA_CH_INDEX(ch)		(BANK_OFFSET * ch)

#define DMA_CTRL_ADDR(ch, addr)		(DMA_CH_CTL_BASE \
					+ (DMA_CH_INDEX(ch) + addr))

#define LPAIF_DMA_CTL(x)		DMA_CTRL_ADDR(x, 0x00)
	#define BURST_EN		(1 << 11)
	#define WPSCNT_ONE		(0 << 8)
	#define WPSCNT_TWO		(1 << 8)
	#define WPSCNT_THREE		(2 << 8)
	#define WPSCNT_FOUR		(3 << 8)
	#define WPSCNT_SIX		(5 << 8)
	#define WPSCNT_EIGHT		(7 << 8)
	#define AUDIO_INTF_NONE		(0 << 4)
	#define AUDIO_INTF_CODEC	(1 << 4)
	#define AUDIO_INTF_PCM		(2 << 4)
	#define AUDIO_INTF_SEC_I2S	(3 << 4)
	#define AUDIO_INTF_MI2S		(4 << 4)
	#define AUDIO_INTF_HDMI		(5 << 4)
	#define AUDIO_INTF_MIXOUT	(6 << 4)
	#define AUDIO_INTF_LOOPBACK1	(7 << 4)
	#define AUDIO_INTF_LOOPBACK2    (8 << 4)
	#define FIFO_WATERMRK(x)	((x & 0x7) << 1)
	#define ENABLE			(1 << 0)

#define LPAIF_DMA_BASE(x)		DMA_CTRL_ADDR(x, 0x04)
	#define BASE_ADDR		(0xFFFFFFFF << 4)

#define	LPAIF_DMA_BUFF_LEN(x)		DMA_CTRL_ADDR(x, 0x08)
#define LPAIF_DMA_CURR_ADDR(x)		DMA_CTRL_ADDR(x, 0x0c)
#define	LPAIF_DMA_PER_LEN(x)		DMA_CTRL_ADDR(x, 0x10)
#define	LPAIF_DMA_PER_CNT(x)		DMA_CTRL_ADDR(x, 0x14)
#define	LPAIF_DMA_FRM(x)		DMA_CTRL_ADDR(x, 0x18)
#define LPAIF_DMA_FRMCLR(x)		DMA_CTRL_ADDR(x, 0x1c)
#define LPAIF_DMA_SET_BUFF_CNT(x)	DMA_CTRL_ADDR(x, 0x20)
#define	LPAIF_DMA_SET_PER_CNT(x)	DMA_CTRL_ADDR(x, 0x24)

#define LPAIF_DMA_PER_CNT_PER_CNT_MASK		0x000FFFFF
#define LPAIF_DMA_PER_CNT_PER_CNT_SHIFT		0
#define LPAIF_DMA_PER_CNT_FIFO_WORDCNT_MASK	0x00F00000
#define LPAIF_DMA_PER_CNT_FIFO_WORDCNT_SHIFT	20

/* channel assignments */

#define DMA_CH_0		0
#define DMA_CH_1		1
#define DMA_CH_2		2
#define DMA_CH_3		3
#define DMA_CH_4		4
#define DMA_CH_5		5
#define DMA_CH_6		6
#define DMA_CH_7		7

#endif
