/* Copyright (c) 2009,2011 The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>

#include <mach/qdsp5v2/mi2s.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>

#define DEBUG
#ifdef DEBUG
#define dprintk(format, arg...) \
printk(KERN_DEBUG format, ## arg)
#else
#define dprintk(format, arg...) do {} while (0)
#endif

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/* Device Types */
#define HDMI 0
#define CODEC_RX 1
#define CODEC_TX 2

/* Static offset for now. If different target have different
 * offset, update to platform data model
 */
#define MI2S_RESET_OFFSET   0x0
#define MI2S_MODE_OFFSET    0x4
#define MI2S_TX_MODE_OFFSET 0x8
#define MI2S_RX_MODE_OFFSET 0xc

#define MI2S_SD_N_EN_MASK 0xF0
#define MI2S_TX_RX_N_MASK 0x0F

#define MI2S_RESET__MI2S_RESET__RESET  0x1
#define MI2S_RESET__MI2S_RESET__ACTIVE 0x0
#define MI2S_MODE__MI2S_MASTER__MASTER 0x1
#define MI2S_MODE__MI2S_MASTER__SLAVE  0x0
#define MI2S_MODE__MI2S_TX_RX_WORD_TYPE__16_BIT 0x1
#define MI2S_MODE__MI2S_TX_RX_WORD_TYPE__24_BIT 0x2
#define MI2S_MODE__MI2S_TX_RX_WORD_TYPE__32_BIT 0x3
#define MI2S_TX_MODE__MI2S_TX_CODEC_16_MONO_MODE__RAW 0x0
#define MI2S_TX_MODE__MI2S_TX_CODEC_16_MONO_MODE__PACKED 0x1
#define MI2S_TX_MODE__MI2S_TX_STEREO_MODE__MONO_SAMPLE   0x0
#define MI2S_TX_MODE__MI2S_TX_STEREO_MODE__STEREO_SAMPLE 0x1
#define MI2S_TX_MODE__MI2S_TX_CH_TYPE__2_CHANNEL 0x0
#define MI2S_TX_MODE__MI2S_TX_CH_TYPE__4_CHANNEL 0x1
#define MI2S_TX_MODE__MI2S_TX_CH_TYPE__6_CHANNEL 0x2
#define MI2S_TX_MODE__MI2S_TX_CH_TYPE__8_CHANNEL 0x3
#define MI2S_TX_MODE__MI2S_TX_DMA_ACK_SYNCH_EN__SYNC_ENABLE 0x1
#define MI2S_RX_MODE__MI2S_RX_CODEC_16_MONO_MODE__RAW 0x0
#define MI2S_RX_MODE__MI2S_RX_CODEC_16_MONO_MODE__PACKED 0x1
#define MI2S_RX_MODE__MI2S_RX_STEREO_MODE__MONO_SAMPLE   0x0
#define MI2S_RX_MODE__MI2S_RX_STEREO_MODE__STEREO_SAMPLE 0x1
#define MI2S_RX_MODE__MI2S_RX_CH_TYPE__2_CH 0x0
#define MI2S_RX_MODE__MI2S_RX_DMA_ACK_SYNCH_EN__SYNC_ENABLE 0x1

#define HWIO_AUDIO1_MI2S_MODE_MI2S_MASTER_BMSK				0x1000
#define HWIO_AUDIO1_MI2S_MODE_MI2S_MASTER_SHFT				0xC
#define HWIO_AUDIO1_MI2S_MODE_MI2S_TX_RX_WORD_TYPE_BMSK  		0x300
#define HWIO_AUDIO1_MI2S_MODE_MI2S_TX_RX_WORD_TYPE_SHFT  		0x8
#define HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_BMSK		0x4
#define HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_SHFT		0x2
#define HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_P_MONO_BMSK                    0x2
#define HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_P_MONO_SHFT                    0x1
#define HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_CH_TYPE_BMSK			0x18
#define HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_CH_TYPE_SHFT			0x3
#define HWIO_AUDIO1_MI2S_TX_MODE_MI2S_4_0_CH_MAP_BMSK			0x80
#define HWIO_AUDIO1_MI2S_TX_MODE_MI2S_4_0_CH_MAP_SHFT			0x7
#define HWIO_AUDIO1_MI2S_TX_MODE_MI2S_2_0_CH_MAP_BMSK			0x60
#define HWIO_AUDIO1_MI2S_TX_MODE_MI2S_2_0_CH_MAP_SHFT			0x5
#define HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_DMA_ACK_SYNCH_EN_BMSK		0x1
#define HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_I2S_LINE_BMSK			0x60
#define HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_I2S_LINE_SHFT			0x5
#define HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_STEREO_MODE_BMSK		0x4
#define HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_STEREO_MODE_SHFT		0x2
#define HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_CODEC_P_MONO_BMSK              0x2
#define HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_CODEC_P_MONO_SHFT              0x1
#define HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_CH_TYPE_BMSK			0x18
#define HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_CH_TYPE_SHFT			0x3
#define HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_DMA_ACK_SYNCH_EN_BMSK		0x1

/* Max number of channels */
#define MAX_NUM_CHANNELS_OUT 8
#define MAX_NUM_CHANNELS_IN  2

/* Num of SD Lines */
#define MAX_SD_LINES 4

#define MI2S_SD_0_EN_MAP  0x10
#define MI2S_SD_1_EN_MAP  0x20
#define MI2S_SD_2_EN_MAP  0x40
#define MI2S_SD_3_EN_MAP  0x80
#define MI2S_SD_0_TX_MAP  0x01
#define MI2S_SD_1_TX_MAP  0x02
#define MI2S_SD_2_TX_MAP  0x04
#define MI2S_SD_3_TX_MAP  0x08

struct mi2s_state {
	void __iomem *mi2s_hdmi_base;
	void __iomem *mi2s_rx_base;
	void __iomem *mi2s_tx_base;
	struct mutex mutex_lock;

};

static struct mi2s_state the_mi2s_state;

static void __iomem *get_base_addr(struct mi2s_state *mi2s, uint8_t dev_id)
{
	switch (dev_id) {
	case HDMI:
		return mi2s->mi2s_hdmi_base;
	case CODEC_RX:
		return mi2s->mi2s_rx_base;
	case CODEC_TX:
		return mi2s->mi2s_tx_base;
	default:
		break;
	}
	return ERR_PTR(-ENODEV);
}

static void mi2s_reset(struct mi2s_state *mi2s, uint8_t dev_id)
{
	void __iomem *baddr = get_base_addr(mi2s, dev_id);
	if (!IS_ERR(baddr))
		writel(MI2S_RESET__MI2S_RESET__RESET,
		baddr + MI2S_RESET_OFFSET);
}

static void mi2s_release(struct mi2s_state *mi2s, uint8_t dev_id)
{
	void __iomem *baddr = get_base_addr(mi2s, dev_id);
	if (!IS_ERR(baddr))
		writel(MI2S_RESET__MI2S_RESET__ACTIVE,
		baddr + MI2S_RESET_OFFSET);
}

static void mi2s_master(struct mi2s_state *mi2s, uint8_t dev_id, bool master)
{
	void __iomem *baddr = get_base_addr(mi2s, dev_id);
	uint32_t val;
	if (!IS_ERR(baddr)) {
		val = readl(baddr + MI2S_MODE_OFFSET);
		if (master) {
			writel(
			((val & ~HWIO_AUDIO1_MI2S_MODE_MI2S_MASTER_BMSK) |
			 (MI2S_MODE__MI2S_MASTER__MASTER <<
			  HWIO_AUDIO1_MI2S_MODE_MI2S_MASTER_SHFT)),
			baddr + MI2S_MODE_OFFSET);
		} else {
			writel(
			((val & ~HWIO_AUDIO1_MI2S_MODE_MI2S_MASTER_BMSK) |
			 (MI2S_MODE__MI2S_MASTER__SLAVE <<
			  HWIO_AUDIO1_MI2S_MODE_MI2S_MASTER_SHFT)),
			baddr + MI2S_MODE_OFFSET);
		}
	}
}

static void mi2s_set_word_type(struct mi2s_state *mi2s, uint8_t dev_id,
	uint8_t size)
{
	void __iomem *baddr = get_base_addr(mi2s, dev_id);
	uint32_t val;
	if (!IS_ERR(baddr)) {
		val = readl(baddr + MI2S_MODE_OFFSET);
		switch (size) {
		case WT_16_BIT:
			writel(
			((val &
			~HWIO_AUDIO1_MI2S_MODE_MI2S_TX_RX_WORD_TYPE_BMSK) |
			(MI2S_MODE__MI2S_TX_RX_WORD_TYPE__16_BIT <<
			HWIO_AUDIO1_MI2S_MODE_MI2S_TX_RX_WORD_TYPE_SHFT)),
			baddr + MI2S_MODE_OFFSET);
			break;
		case WT_24_BIT:
			writel(
			((val &
			~HWIO_AUDIO1_MI2S_MODE_MI2S_TX_RX_WORD_TYPE_BMSK) |
			(MI2S_MODE__MI2S_TX_RX_WORD_TYPE__24_BIT <<
			HWIO_AUDIO1_MI2S_MODE_MI2S_TX_RX_WORD_TYPE_SHFT)),
			baddr + MI2S_MODE_OFFSET);
			break;
		case WT_32_BIT:
			writel(
			((val &
			~HWIO_AUDIO1_MI2S_MODE_MI2S_TX_RX_WORD_TYPE_BMSK) |
			(MI2S_MODE__MI2S_TX_RX_WORD_TYPE__32_BIT <<
			HWIO_AUDIO1_MI2S_MODE_MI2S_TX_RX_WORD_TYPE_SHFT)),
			baddr + MI2S_MODE_OFFSET);
			break;
		default:
			break;
		}
	}
}

static void mi2s_set_sd(struct mi2s_state *mi2s, uint8_t dev_id, uint8_t sd_map)
{
	void __iomem *baddr = get_base_addr(mi2s, dev_id);
	uint32_t val;
	if (!IS_ERR(baddr)) {
		val = readl(baddr + MI2S_MODE_OFFSET) &
			~(MI2S_SD_N_EN_MASK | MI2S_TX_RX_N_MASK);
		writel(val | sd_map, baddr + MI2S_MODE_OFFSET);
	}
}

static void mi2s_set_output_num_channels(struct mi2s_state *mi2s,
	uint8_t dev_id, uint8_t channels)
{
	void __iomem *baddr = get_base_addr(mi2s, dev_id);
	uint32_t val;
	if (!IS_ERR(baddr)) {
		val = readl(baddr + MI2S_TX_MODE_OFFSET);
		if (channels == MI2S_CHAN_MONO_RAW) {
			val = (val &
			~(HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_BMSK |
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_P_MONO_BMSK)) |
			((MI2S_TX_MODE__MI2S_TX_STEREO_MODE__MONO_SAMPLE <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_SHFT) |
			(MI2S_TX_MODE__MI2S_TX_CODEC_16_MONO_MODE__RAW <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_P_MONO_SHFT));
		} else if (channels == MI2S_CHAN_MONO_PACKED) {
			val = (val &
			~(HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_BMSK |
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_P_MONO_BMSK)) |
			((MI2S_TX_MODE__MI2S_TX_STEREO_MODE__MONO_SAMPLE <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_SHFT) |
			(MI2S_TX_MODE__MI2S_TX_CODEC_16_MONO_MODE__PACKED <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_P_MONO_SHFT));
		} else if (channels == MI2S_CHAN_STEREO) {
			val = (val &
			~(HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_BMSK |
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_CH_TYPE_BMSK)) |
			((MI2S_TX_MODE__MI2S_TX_STEREO_MODE__STEREO_SAMPLE <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_SHFT) |
			(MI2S_TX_MODE__MI2S_TX_CH_TYPE__2_CHANNEL <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_CH_TYPE_SHFT));
		} else if (channels == MI2S_CHAN_4CHANNELS) {
			val = (val &
			~(HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_BMSK |
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_CH_TYPE_BMSK)) |
			((MI2S_TX_MODE__MI2S_TX_STEREO_MODE__STEREO_SAMPLE <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_SHFT) |
			(MI2S_TX_MODE__MI2S_TX_CH_TYPE__4_CHANNEL <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_CH_TYPE_SHFT));
		} else if (channels == MI2S_CHAN_6CHANNELS) {
			val = (val &
			~(HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_BMSK |
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_CH_TYPE_BMSK)) |
			((MI2S_TX_MODE__MI2S_TX_STEREO_MODE__STEREO_SAMPLE <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_SHFT) |
			(MI2S_TX_MODE__MI2S_TX_CH_TYPE__6_CHANNEL <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_CH_TYPE_SHFT));
		} else if (channels == MI2S_CHAN_8CHANNELS) {
			val = (val &
			~(HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_BMSK |
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_CH_TYPE_BMSK)) |
			((MI2S_TX_MODE__MI2S_TX_STEREO_MODE__STEREO_SAMPLE <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_STEREO_MODE_SHFT) |
			(MI2S_TX_MODE__MI2S_TX_CH_TYPE__8_CHANNEL <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_CH_TYPE_SHFT));
		}
		writel(val, baddr + MI2S_TX_MODE_OFFSET);
	}
}

static void mi2s_set_output_4ch_map(struct mi2s_state *mi2s, uint8_t dev_id,
	bool high_low)
{
	void __iomem *baddr = get_base_addr(mi2s, dev_id);
	uint32_t val;
	if (!IS_ERR(baddr)) {
		val = readl(baddr + MI2S_TX_MODE_OFFSET);
		val = (val & ~HWIO_AUDIO1_MI2S_TX_MODE_MI2S_4_0_CH_MAP_BMSK) |
			(high_low <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_4_0_CH_MAP_SHFT);
		writel(val, baddr + MI2S_TX_MODE_OFFSET);
	}
}

static void mi2s_set_output_2ch_map(struct mi2s_state *mi2s, uint8_t dev_id,
	uint8_t sd_line)
{
	void __iomem *baddr = get_base_addr(mi2s, dev_id);
	uint32_t val;

	if (!IS_ERR(baddr)) {
		val = readl(baddr + MI2S_TX_MODE_OFFSET);
		if (sd_line < 4) {
			val = (val &
			~HWIO_AUDIO1_MI2S_TX_MODE_MI2S_2_0_CH_MAP_BMSK) |
			(sd_line <<
			HWIO_AUDIO1_MI2S_TX_MODE_MI2S_2_0_CH_MAP_SHFT);
			writel(val, baddr + MI2S_TX_MODE_OFFSET);
		}
	}
}

static void mi2s_set_output_clk_synch(struct mi2s_state *mi2s, uint8_t dev_id)
{
	void __iomem *baddr = get_base_addr(mi2s, dev_id);
	uint32_t val;

	if (!IS_ERR(baddr)) {
		val = readl(baddr + MI2S_TX_MODE_OFFSET);
		writel(((val &
		~HWIO_AUDIO1_MI2S_TX_MODE_MI2S_TX_DMA_ACK_SYNCH_EN_BMSK) |
		MI2S_TX_MODE__MI2S_TX_DMA_ACK_SYNCH_EN__SYNC_ENABLE),
		baddr + MI2S_TX_MODE_OFFSET);
	}
}

static void mi2s_set_input_sd_line(struct mi2s_state *mi2s, uint8_t dev_id,
	uint8_t sd_line)
{
	void __iomem *baddr = get_base_addr(mi2s, dev_id);
	uint32_t val;

	if (!IS_ERR(baddr)) {
		val = readl(baddr + MI2S_RX_MODE_OFFSET);
		if (sd_line < 4) {
			val = (val &
			~HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_I2S_LINE_BMSK) |
			(sd_line <<
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_I2S_LINE_SHFT);
			writel(val, baddr + MI2S_RX_MODE_OFFSET);
		}
	}
}

static void mi2s_set_input_num_channels(struct mi2s_state *mi2s, uint8_t dev_id,
	uint8_t channels)
{
	void __iomem *baddr = get_base_addr(mi2s, dev_id);
	uint32_t val;

	if (!IS_ERR(baddr)) {
		val = readl(baddr + MI2S_RX_MODE_OFFSET);
		if (channels == MI2S_CHAN_MONO_RAW) {
			val = (val &
			~(HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_STEREO_MODE_BMSK |
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_CODEC_P_MONO_BMSK)) |
			((MI2S_RX_MODE__MI2S_RX_STEREO_MODE__MONO_SAMPLE <<
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_STEREO_MODE_SHFT) |
			(MI2S_RX_MODE__MI2S_RX_CODEC_16_MONO_MODE__RAW <<
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_CODEC_P_MONO_SHFT));
		} else if (channels == MI2S_CHAN_MONO_PACKED) {
			val = (val &
			~(HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_STEREO_MODE_BMSK |
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_CODEC_P_MONO_BMSK)) |
			((MI2S_RX_MODE__MI2S_RX_STEREO_MODE__MONO_SAMPLE <<
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_STEREO_MODE_SHFT) |
			(MI2S_RX_MODE__MI2S_RX_CODEC_16_MONO_MODE__PACKED <<
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_CODEC_P_MONO_SHFT));
		} else if (channels == MI2S_CHAN_STEREO) {

			if (dev_id == HDMI)
				val = (val &
			~(HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_STEREO_MODE_BMSK |
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_CH_TYPE_BMSK)) |
			((MI2S_RX_MODE__MI2S_RX_STEREO_MODE__STEREO_SAMPLE <<
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_STEREO_MODE_SHFT) |
			(MI2S_RX_MODE__MI2S_RX_CH_TYPE__2_CH <<
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_CH_TYPE_SHFT));

			else
				val = (val &
			~(HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_STEREO_MODE_BMSK |
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_CH_TYPE_BMSK)) |
			((MI2S_RX_MODE__MI2S_RX_STEREO_MODE__STEREO_SAMPLE <<
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_STEREO_MODE_SHFT) |
			(MI2S_RX_MODE__MI2S_RX_CODEC_16_MONO_MODE__PACKED <<
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_CODEC_P_MONO_SHFT) |
			(MI2S_RX_MODE__MI2S_RX_CH_TYPE__2_CH <<
			HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_CH_TYPE_SHFT));


		}
		writel(val, baddr + MI2S_RX_MODE_OFFSET);
	}
}

static void mi2s_set_input_clk_synch(struct mi2s_state *mi2s, uint8_t dev_id)
{
	void __iomem *baddr = get_base_addr(mi2s, dev_id);
	uint32_t val;

	if (!IS_ERR(baddr)) {
		val = readl(baddr + MI2S_RX_MODE_OFFSET);
		writel(
		((val &
		~HWIO_AUDIO1_MI2S_RX_MODE_MI2S_RX_DMA_ACK_SYNCH_EN_BMSK) |
		MI2S_RX_MODE__MI2S_RX_DMA_ACK_SYNCH_EN__SYNC_ENABLE),
		baddr + MI2S_RX_MODE_OFFSET);
	}
}


static u8 num_of_bits_set(u8 sd_line_mask)
{
	u8 num_bits_set = 0;

	while (sd_line_mask) {

		if (sd_line_mask & 1)
			num_bits_set++;
		sd_line_mask = sd_line_mask >> 1;
	}
	return num_bits_set;
}


bool mi2s_set_hdmi_output_path(uint8_t channels, uint8_t size,
		uint8_t sd_line_mask)
{
	bool ret_val = MI2S_TRUE;
	struct mi2s_state *mi2s = &the_mi2s_state;
	u8 sd_line, num_of_sd_lines = 0;
	void __iomem *baddr;
	uint32_t val;

	pr_debug("%s: channels = %u size = %u sd_line_mask = 0x%x\n", __func__,
		channels, size, sd_line_mask);

	if ((channels == 0) ||  (channels > MAX_NUM_CHANNELS_OUT) ||
		((channels != 1) && (channels % 2 != 0))) {

		pr_err("%s: invalid number of channels. channels = %u\n",
				__func__, channels);
		return  MI2S_FALSE;
	}

	sd_line_mask &=  MI2S_SD_LINE_MASK;

	if (!sd_line_mask) {
		pr_err("%s: Did not set any data lines to use "
			" sd_line_mask =0x%x\n", __func__, sd_line_mask);
		return  MI2S_FALSE;
	}

	mutex_lock(&mi2s->mutex_lock);
	/* Put device in reset */
	mi2s_reset(mi2s, HDMI);

	mi2s_master(mi2s, HDMI, 1);

	/* Set word type */
	if (size <= WT_MAX)
		mi2s_set_word_type(mi2s, HDMI, size);
	else
		ret_val = MI2S_FALSE;

	/* Enable clock crossing synchronization of RD DMA ACK */
	mi2s_set_output_clk_synch(mi2s, HDMI);

	mi2s_set_output_num_channels(mi2s, HDMI, channels);

	num_of_sd_lines = num_of_bits_set(sd_line_mask);
	/*Second argument to find_first_bit should be maximum number of
	bit*/

	sd_line = find_first_bit((unsigned long *)&sd_line_mask,
			sizeof(sd_line_mask) * 8);
	pr_debug("sd_line = %d\n", sd_line);

	if (channels == 1) {

		if (num_of_sd_lines != 1) {
			pr_err("%s: for one channel only one SD lines is"
				" needed. num_of_sd_lines = %u\n",
				__func__, num_of_sd_lines);

			ret_val = MI2S_FALSE;
			goto error;
		}

		if (sd_line != 0) {
			pr_err("%s: for one channel tx, need to use SD_0 "
					"sd_line = %u\n", __func__, sd_line);

			ret_val = MI2S_FALSE;
			goto error;
		}

		/* Enable SD line 0 for Tx (only option for
			 * mono audio)
		 */
		mi2s_set_sd(mi2s, HDMI, MI2S_SD_0_EN_MAP | MI2S_SD_0_TX_MAP);

	} else if (channels == 2) {

		if (num_of_sd_lines != 1) {
			pr_err("%s: for two channel only one SD lines is"
				" needed. num_of_sd_lines = %u\n",
				__func__, num_of_sd_lines);
			ret_val = MI2S_FALSE;
			goto error;
		}

		/* Enable single SD line for Tx */
		mi2s_set_sd(mi2s, HDMI, (MI2S_SD_0_EN_MAP << sd_line) |
				(MI2S_SD_0_TX_MAP << sd_line));

		/* Set 2-channel mapping */
		mi2s_set_output_2ch_map(mi2s, HDMI, sd_line);

	} else if (channels == 4) {

		if (num_of_sd_lines != 2) {
			pr_err("%s: for 4 channels two SD lines are"
				" needed. num_of_sd_lines = %u\\n",
				__func__, num_of_sd_lines);
			ret_val = MI2S_FALSE;
			goto error;
		}

		if ((sd_line_mask && MI2S_SD_0) &&
				(sd_line_mask && MI2S_SD_1)) {

			mi2s_set_sd(mi2s, HDMI, (MI2S_SD_0_EN_MAP |
				MI2S_SD_1_EN_MAP) | (MI2S_SD_0_TX_MAP |
				MI2S_SD_1_TX_MAP));
			mi2s_set_output_4ch_map(mi2s, HDMI, MI2S_FALSE);

		} else if ((sd_line_mask && MI2S_SD_2) &&
				(sd_line_mask && MI2S_SD_3)) {

			mi2s_set_sd(mi2s, HDMI, (MI2S_SD_2_EN_MAP |
				MI2S_SD_3_EN_MAP) | (MI2S_SD_2_TX_MAP |
				MI2S_SD_3_TX_MAP));

			mi2s_set_output_4ch_map(mi2s, HDMI, MI2S_TRUE);
		} else {

			pr_err("%s: for 4 channels invalid SD lines usage"
				" sd_line_mask = 0x%x\n",
				__func__, sd_line_mask);
			ret_val = MI2S_FALSE;
			goto error;
		}
	} else if (channels == 6) {

		if (num_of_sd_lines != 3) {
			pr_err("%s: for 6 channels three SD lines are"
				" needed. num_of_sd_lines = %u\n",
				__func__, num_of_sd_lines);
			ret_val = MI2S_FALSE;
			goto error;
		}

		if ((sd_line_mask && MI2S_SD_0) &&
			(sd_line_mask && MI2S_SD_1) &&
			(sd_line_mask && MI2S_SD_2)) {

			mi2s_set_sd(mi2s, HDMI, (MI2S_SD_0_EN_MAP |
				MI2S_SD_1_EN_MAP | MI2S_SD_2_EN_MAP) |
				(MI2S_SD_0_TX_MAP | MI2S_SD_1_TX_MAP |
				MI2S_SD_2_TX_MAP));

		} else if ((sd_line_mask && MI2S_SD_1) &&
				(sd_line_mask && MI2S_SD_2) &&
				(sd_line_mask && MI2S_SD_3)) {

			mi2s_set_sd(mi2s, HDMI, (MI2S_SD_1_EN_MAP |
				MI2S_SD_2_EN_MAP | MI2S_SD_3_EN_MAP) |
				(MI2S_SD_1_TX_MAP | MI2S_SD_2_TX_MAP |
				MI2S_SD_3_TX_MAP));

		} else {

			pr_err("%s: for 6 channels invalid SD lines usage"
				" sd_line_mask = 0x%x\n",
				__func__, sd_line_mask);
			ret_val = MI2S_FALSE;
			goto error;
		}
	} else if (channels == 8) {

		if (num_of_sd_lines != 4) {
			pr_err("%s: for 8 channels four SD lines are"
				" needed. num_of_sd_lines = %u\n",
				__func__, num_of_sd_lines);
			ret_val = MI2S_FALSE;
			goto error;
		}

		mi2s_set_sd(mi2s, HDMI, (MI2S_SD_0_EN_MAP |
			MI2S_SD_1_EN_MAP | MI2S_SD_2_EN_MAP |
			MI2S_SD_3_EN_MAP) | (MI2S_SD_0_TX_MAP |
			MI2S_SD_1_TX_MAP | MI2S_SD_2_TX_MAP |
			MI2S_SD_3_TX_MAP));
	} else {
		pr_err("%s: invalid number channels = %u\n",
				__func__, channels);
			ret_val = MI2S_FALSE;
			goto error;
	}

	baddr = get_base_addr(mi2s, HDMI);

	val = readl(baddr + MI2S_MODE_OFFSET);
	pr_debug("%s(): MI2S_MODE = 0x%x\n", __func__, val);

	val = readl(baddr + MI2S_TX_MODE_OFFSET);
	pr_debug("%s(): MI2S_TX_MODE = 0x%x\n", __func__, val);


error:
	/* Release device from reset */
	mi2s_release(mi2s, HDMI);

	mutex_unlock(&mi2s->mutex_lock);
	mb();
	return ret_val;
}
EXPORT_SYMBOL(mi2s_set_hdmi_output_path);

bool mi2s_set_hdmi_input_path(uint8_t channels, uint8_t size,
		uint8_t sd_line_mask)
{
	bool ret_val = MI2S_TRUE;
	struct mi2s_state *mi2s = &the_mi2s_state;
	u8 sd_line, num_of_sd_lines = 0;
	void __iomem *baddr;
	uint32_t val;

	pr_debug("%s: channels = %u size = %u sd_line_mask = 0x%x\n", __func__,
		channels, size, sd_line_mask);

	if ((channels != 1) && (channels != MAX_NUM_CHANNELS_IN)) {

		pr_err("%s: invalid number of channels. channels = %u\n",
				__func__, channels);
		return  MI2S_FALSE;
	}

	if (size > WT_MAX) {

		pr_err("%s: mi2s word size can not be greater than 32 bits\n",
				__func__);
		return MI2S_FALSE;
	}

	sd_line_mask &=  MI2S_SD_LINE_MASK;

	if (!sd_line_mask) {
		pr_err("%s: Did not set any data lines to use "
			" sd_line_mask =0x%x\n", __func__, sd_line_mask);
		return  MI2S_FALSE;
	}

	num_of_sd_lines = num_of_bits_set(sd_line_mask);

	if (num_of_sd_lines != 1) {
		pr_err("%s: for two channel input only one SD lines is"
			" needed. num_of_sd_lines = %u sd_line_mask = 0x%x\n",
			__func__, num_of_sd_lines, sd_line_mask);
		return MI2S_FALSE;
	}

	/*Second argument to find_first_bit should be maximum number of
	bits interested*/
	sd_line = find_first_bit((unsigned long *)&sd_line_mask,
			sizeof(sd_line_mask) * 8);
	pr_debug("sd_line = %d\n", sd_line);

	/* Ensure sd_line parameter is valid (0-max) */
	if (sd_line > MAX_SD_LINES) {
		pr_err("%s: Line number can not be greater than = %u\n",
			__func__, MAX_SD_LINES);
		return MI2S_FALSE;
	}

	mutex_lock(&mi2s->mutex_lock);
	/* Put device in reset */
	mi2s_reset(mi2s, HDMI);

	mi2s_master(mi2s, HDMI, 1);

	/* Set word type */
	mi2s_set_word_type(mi2s, HDMI, size);

	/* Enable clock crossing synchronization of WR DMA ACK */
	mi2s_set_input_clk_synch(mi2s, HDMI);

	/* Ensure channels parameter is valid (non-zero, less than max,
	 * and even or mono)
	 */
	mi2s_set_input_num_channels(mi2s, HDMI, channels);

	mi2s_set_input_sd_line(mi2s, HDMI, sd_line);

	mi2s_set_sd(mi2s, HDMI, (MI2S_SD_0_EN_MAP << sd_line));

	baddr = get_base_addr(mi2s, HDMI);

	val = readl(baddr + MI2S_MODE_OFFSET);
	pr_debug("%s(): MI2S_MODE = 0x%x\n", __func__, val);

	val = readl(baddr + MI2S_RX_MODE_OFFSET);
	pr_debug("%s(): MI2S_RX_MODE = 0x%x\n", __func__, val);

	/* Release device from reset */
	mi2s_release(mi2s, HDMI);

	mutex_unlock(&mi2s->mutex_lock);
	mb();
	return ret_val;
}
EXPORT_SYMBOL(mi2s_set_hdmi_input_path);

bool mi2s_set_codec_output_path(uint8_t channels, uint8_t size)
{
	bool ret_val = MI2S_TRUE;
	struct mi2s_state *mi2s = &the_mi2s_state;

	mutex_lock(&mi2s->mutex_lock);
	/* Put device in reset */
	mi2s_reset(mi2s, CODEC_TX);

	mi2s_master(mi2s, CODEC_TX, 1);

	/* Enable clock crossing synchronization of RD DMA ACK */
	mi2s_set_output_clk_synch(mi2s, CODEC_TX);

	/* Set word type */
	if (size <= WT_MAX)
		mi2s_set_word_type(mi2s, CODEC_TX, size);
	else
		ret_val = MI2S_FALSE;

	mi2s_set_output_num_channels(mi2s, CODEC_TX, channels);

	/* Enable SD line */
	mi2s_set_sd(mi2s, CODEC_TX, MI2S_SD_0_EN_MAP | MI2S_SD_0_TX_MAP);

	/* Release device from reset */
	mi2s_release(mi2s, CODEC_TX);

	mutex_unlock(&mi2s->mutex_lock);
	mb();
	return ret_val;
}
EXPORT_SYMBOL(mi2s_set_codec_output_path);

bool mi2s_set_codec_input_path(uint8_t channels, uint8_t size)
{
	bool ret_val = MI2S_TRUE;
	struct mi2s_state *mi2s = &the_mi2s_state;

	mutex_lock(&the_mi2s_state.mutex_lock);
	/* Put device in reset */
	mi2s_reset(mi2s, CODEC_RX);

	mi2s_master(mi2s, CODEC_RX, 1);

	/* Enable clock crossing synchronization of WR DMA ACK */
	mi2s_set_input_clk_synch(mi2s, CODEC_RX);

	/* Set word type */
	if (size <= WT_MAX)
		mi2s_set_word_type(mi2s, CODEC_RX, size);
	else
		ret_val = MI2S_FALSE;

	mi2s_set_input_num_channels(mi2s, CODEC_RX, channels);

	/* Enable SD line */
	mi2s_set_sd(mi2s, CODEC_RX, MI2S_SD_0_EN_MAP);

	/* Release device from reset */
	mi2s_release(mi2s, CODEC_RX);

	mutex_unlock(&mi2s->mutex_lock);
	mb();
	return ret_val;
}
EXPORT_SYMBOL(mi2s_set_codec_input_path);


static int mi2s_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *mem_src;

	mem_src = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hdmi");
	if (!mem_src) {
		rc = -ENODEV;
		goto error_hdmi;
	}
	the_mi2s_state.mi2s_hdmi_base = ioremap(mem_src->start,
		(mem_src->end - mem_src->start) + 1);
	if (!the_mi2s_state.mi2s_hdmi_base) {
		rc = -ENOMEM;
		goto error_hdmi;
	}
	mem_src = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "codec_rx");
	if (!mem_src) {
		rc = -ENODEV;
		goto error_codec_rx;
	}
	the_mi2s_state.mi2s_rx_base = ioremap(mem_src->start,
		(mem_src->end - mem_src->start) + 1);
	if (!the_mi2s_state.mi2s_rx_base) {
		rc = -ENOMEM;
		goto error_codec_rx;
	}
	mem_src = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "codec_tx");
	if (!mem_src) {
		rc = -ENODEV;
		goto error_codec_tx;
	}
	the_mi2s_state.mi2s_tx_base = ioremap(mem_src->start,
		(mem_src->end - mem_src->start) + 1);
	if (!the_mi2s_state.mi2s_tx_base) {
		rc = -ENOMEM;
		goto error_codec_tx;
	}
	mutex_init(&the_mi2s_state.mutex_lock);

	return rc;

error_codec_tx:
	iounmap(the_mi2s_state.mi2s_rx_base);
error_codec_rx:
	iounmap(the_mi2s_state.mi2s_hdmi_base);
error_hdmi:
	return rc;

}

static int mi2s_remove(struct platform_device *pdev)
{
	iounmap(the_mi2s_state.mi2s_tx_base);
	iounmap(the_mi2s_state.mi2s_rx_base);
	iounmap(the_mi2s_state.mi2s_hdmi_base);
	return 0;
}

static struct platform_driver mi2s_driver = {
	.probe = mi2s_probe,
	.remove = mi2s_remove,
	.driver = {
		.name = "mi2s",
		.owner = THIS_MODULE,
	},
};

static int __init mi2s_init(void)
{
	return platform_driver_register(&mi2s_driver);
}

static void __exit mi2s_exit(void)
{
	platform_driver_unregister(&mi2s_driver);
}

module_init(mi2s_init);
module_exit(mi2s_exit);

MODULE_DESCRIPTION("MSM MI2S driver");
MODULE_LICENSE("GPL v2");
