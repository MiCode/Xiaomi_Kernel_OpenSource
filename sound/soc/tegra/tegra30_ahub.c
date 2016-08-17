/*
 * tegra30_ahub.c - Tegra 30 AHUB driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (C) 2011 - NVIDIA, Inc.
 * Copyright (c) 2012, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <mach/dma.h>
#include <mach/iomap.h>
#include <sound/soc.h>
#include "tegra30_ahub.h"

#define DRV_NAME "tegra30-ahub"

static struct tegra30_ahub *ahub;

static inline void tegra30_apbif_write(u32 reg, u32 val)
{
#ifdef CONFIG_PM
	ahub->apbif_reg_cache[reg >> 2] = val;
#endif
	__raw_writel(val, ahub->apbif_regs + reg);
}

static inline u32 tegra30_apbif_read(u32 reg)
{
	return __raw_readl(ahub->apbif_regs + reg);
}

static inline void tegra30_audio_write(u32 reg, u32 val)
{
#ifdef CONFIG_PM
	ahub->ahub_reg_cache[reg >> 2] = val;
#endif
	__raw_writel(val, ahub->audio_regs + reg);
}

static inline u32 tegra30_audio_read(u32 reg)
{
	return __raw_readl(ahub->audio_regs + reg);
}

#ifdef CONFIG_PM
int tegra30_ahub_apbif_resume()
{
	int i = 0;
	int cache_idx_rsvd;

	tegra30_ahub_enable_clocks();

	/*restore ahub regs*/
	for (i = 0; i < TEGRA30_AHUB_AUDIO_RX_COUNT; i++)
		tegra30_audio_write(i<<2, ahub->ahub_reg_cache[i]);

	/*restore apbif regs*/
	cache_idx_rsvd = TEGRA30_APBIF_CACHE_REG_INDEX_RSVD;
	for (i = 0; i < TEGRA30_APBIF_CACHE_REG_COUNT; i++) {
		if (i == cache_idx_rsvd) {
			cache_idx_rsvd +=
				TEGRA30_APBIF_CACHE_REG_INDEX_RSVD_STRIDE;
			continue;
		}

		tegra30_apbif_write(i<<2, ahub->apbif_reg_cache[i]);
	}

	tegra30_ahub_disable_clocks();

	return 0;
}
#endif

/*
 * clk_apbif isn't required for a theoretical I2S<->I2S configuration where
 * no PCM data is read from or sent to memory. However, that's an unlikely
 * use-case, and not something the rest of the driver supports right now, so
 * we'll just treat the two clocks as one for now.
 *
 * This function should not be a plain ref-count. Instead, each active stream
 * contributes some requirement to the minimum clock rate, so starting or
 * stopping streams should dynamically adjust the clock as required.  However,
 * this is not yet implemented.
 */
void tegra30_ahub_enable_clocks(void)
{
	clk_enable(ahub->clk_d_audio);
	clk_enable(ahub->clk_apbif);
}

void tegra30_ahub_disable_clocks(void)
{
	clk_disable(ahub->clk_apbif);
	clk_disable(ahub->clk_d_audio);
}

/*
 * for TDM mode, ahub has to run faster than I2S controller.  This will avoid
 * FIFO overflow/underflow, the causes of slot-hopping symptoms
 */
void tegra30_ahub_clock_set_rate(int rate)
{
	clk_set_rate(ahub->clk_d_audio, rate);
}

#ifdef CONFIG_DEBUG_FS
static inline u32 tegra30_ahub_read(u32 space, u32 reg)
{
	if (space == 0)
		return tegra30_apbif_read(reg);
	else
		return tegra30_audio_read(reg);
}

static int tegra30_ahub_show(struct seq_file *s, void *unused)
{
#define REG(space, r) { space, r, 0,          1,         #r }
#define ARR(space, r) { space, r, r##_STRIDE, r##_COUNT, #r }
	static const struct {
		int space;
		u32 offset;
		u32 stride;
		u32 count;
		const char *name;
	} regs[] = {
		ARR(0, TEGRA30_AHUB_CHANNEL_CTRL),
		ARR(0, TEGRA30_AHUB_CHANNEL_CLEAR),
		ARR(0, TEGRA30_AHUB_CHANNEL_STATUS),
		ARR(0, TEGRA30_AHUB_CIF_TX_CTRL),
		ARR(0, TEGRA30_AHUB_CIF_RX_CTRL),
		REG(0, TEGRA30_AHUB_CONFIG_LINK_CTRL),
		REG(0, TEGRA30_AHUB_MISC_CTRL),
		REG(0, TEGRA30_AHUB_APBDMA_LIVE_STATUS),
		REG(0, TEGRA30_AHUB_I2S_LIVE_STATUS),
		ARR(0, TEGRA30_AHUB_DAM_LIVE_STATUS),
		REG(0, TEGRA30_AHUB_SPDIF_LIVE_STATUS),
		REG(0, TEGRA30_AHUB_I2S_INT_MASK),
		REG(0, TEGRA30_AHUB_DAM_INT_MASK),
		REG(0, TEGRA30_AHUB_SPDIF_INT_MASK),
		REG(0, TEGRA30_AHUB_APBIF_INT_MASK),
		REG(0, TEGRA30_AHUB_I2S_INT_STATUS),
		REG(0, TEGRA30_AHUB_DAM_INT_STATUS),
		REG(0, TEGRA30_AHUB_SPDIF_INT_STATUS),
		REG(0, TEGRA30_AHUB_APBIF_INT_STATUS),
		REG(0, TEGRA30_AHUB_I2S_INT_SOURCE),
		REG(0, TEGRA30_AHUB_DAM_INT_SOURCE),
		REG(0, TEGRA30_AHUB_SPDIF_INT_SOURCE),
		REG(0, TEGRA30_AHUB_APBIF_INT_SOURCE),
		REG(0, TEGRA30_AHUB_I2S_INT_SET),
		REG(0, TEGRA30_AHUB_DAM_INT_SET),
		REG(0, TEGRA30_AHUB_SPDIF_INT_SET),
		REG(0, TEGRA30_AHUB_APBIF_INT_SET),
		ARR(1, TEGRA30_AHUB_AUDIO_RX),
	};
#undef ARRG
#undef REG

	int i, j;

	tegra30_ahub_enable_clocks();

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		if (regs[i].count > 1) {
			for (j = 0; j < regs[i].count; j++) {
				u32 reg = regs[i].offset + (j * regs[i].stride);
				u32 val = tegra30_ahub_read(regs[i].space, reg);
				seq_printf(s, "%s[%d] = %08x\n", regs[i].name,
					   j, val);
			}
		} else {
			u32 val = tegra30_ahub_read(regs[i].space,
						  regs[i].offset);
			seq_printf(s, "%s = %08x\n", regs[i].name, val);
		}
	}

	tegra30_ahub_disable_clocks();

	return 0;
}

static int tegra30_ahub_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra30_ahub_show, inode->i_private);
}

static const struct file_operations tegra30_ahub_debug_fops = {
	.open    = tegra30_ahub_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static void tegra30_ahub_debug_add(struct tegra30_ahub *ahub)
{
	ahub->debug = debugfs_create_file(DRV_NAME, S_IRUGO,
					  snd_soc_debugfs_root, ahub,
					  &tegra30_ahub_debug_fops);
}

static void tegra30_ahub_debug_remove(struct tegra30_ahub *ahub)
{
	if (ahub->debug)
		debugfs_remove(ahub->debug);
}
#else
static inline void tegra30_ahub_debug_add(struct tegra30_ahub *ahub)
{
}

static inline void tegra30_ahub_debug_remove(struct tegra30_ahub *ahub)
{
}
#endif

int tegra30_ahub_allocate_rx_fifo(enum tegra30_ahub_rxcif *rxcif,
				  unsigned long *fiforeg,
				  unsigned long *reqsel)
{
	int channel;
	u32 reg, val;

	channel = find_first_zero_bit(ahub->rx_usage,
				      TEGRA30_AHUB_CHANNEL_CTRL_COUNT);
	if (channel >= TEGRA30_AHUB_CHANNEL_CTRL_COUNT)
		return -EBUSY;

	__set_bit(channel, ahub->rx_usage);

	*rxcif = TEGRA30_AHUB_RXCIF_APBIF_RX0 + channel;
	*fiforeg = ahub->apbif_addr + TEGRA30_AHUB_CHANNEL_RXFIFO +
		   (channel * TEGRA30_AHUB_CHANNEL_RXFIFO_STRIDE);
	*reqsel = TEGRA_DMA_REQ_SEL_APBIF_CH0 + channel;

	tegra30_ahub_enable_clocks();

	reg = TEGRA30_AHUB_CHANNEL_CTRL +
	      (channel * TEGRA30_AHUB_CHANNEL_CTRL_STRIDE);
	val = tegra30_apbif_read(reg);
	val &= ~(TEGRA30_AHUB_CHANNEL_CTRL_RX_THRESHOLD_MASK);
	val |= (7 << TEGRA30_AHUB_CHANNEL_CTRL_RX_THRESHOLD_SHIFT);
	tegra30_apbif_write(reg, val);

	reg = TEGRA30_AHUB_CIF_RX_CTRL +
	      (channel * TEGRA30_AHUB_CIF_RX_CTRL_STRIDE);
	val = (0 << TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT) |
	      (1 << TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
	      (1 << TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT) |
	      TEGRA30_AUDIOCIF_CTRL_DIRECTION_RX;
	tegra30_apbif_write(reg, val);

	tegra30_ahub_disable_clocks();

	return 0;
}

int tegra30_ahub_rx_fifo_is_enabled(int i2s_id)
{
	int val, mask;

	val = tegra30_apbif_read(TEGRA30_AHUB_I2S_LIVE_STATUS);
	mask = (TEGRA30_AHUB_I2S_LIVE_STATUS_I2S0_RX_FIFO_ENABLED << (i2s_id*2));
	val &= mask;
	return val;
}

int tegra30_ahub_tx_fifo_is_enabled(int i2s_id)
{
	int val, mask;

	val = tegra30_apbif_read(TEGRA30_AHUB_I2S_LIVE_STATUS);
	mask = (TEGRA30_AHUB_I2S_LIVE_STATUS_I2S0_TX_FIFO_ENABLED << (i2s_id*2));
	val &= mask;

	return val;
}


int tegra30_ahub_rx_fifo_is_empty(int i2s_id)
{
	int val, mask;

	val = tegra30_apbif_read(TEGRA30_AHUB_I2S_LIVE_STATUS);
	mask = (TEGRA30_AHUB_I2S_LIVE_STATUS_I2S0_RX_FIFO_EMPTY << (i2s_id*2));
	val &= mask;
	return val;
}

int tegra30_ahub_tx_fifo_is_empty(int i2s_id)
{
	int val, mask;

	val = tegra30_apbif_read(TEGRA30_AHUB_I2S_LIVE_STATUS);
	mask = (TEGRA30_AHUB_I2S_LIVE_STATUS_I2S0_TX_FIFO_EMPTY << (i2s_id*2));
	val &= mask;

	return val;
}


int tegra30_ahub_dam_ch0_is_enabled(int dam_id)
{
	int val, mask;

	val = tegra30_apbif_read((TEGRA30_AHUB_DAM_LIVE_STATUS) +
			(dam_id * TEGRA30_AHUB_DAM_LIVE_STATUS_STRIDE));
	mask = TEGRA30_AHUB_DAM_LIVE_STATUS_RX0_ENABLED;
	val &= mask;

	return val;
}

int tegra30_ahub_dam_ch1_is_enabled(int dam_id)
{
	int val, mask;

	val = tegra30_apbif_read((TEGRA30_AHUB_DAM_LIVE_STATUS) +
			(dam_id * TEGRA30_AHUB_DAM_LIVE_STATUS_STRIDE));
	mask = TEGRA30_AHUB_DAM_LIVE_STATUS_RX1_ENABLED;
	val &= mask;

	return val;
}

int tegra30_ahub_dam_tx_is_enabled(int dam_id)
{
	int val, mask;

	val = tegra30_apbif_read((TEGRA30_AHUB_DAM_LIVE_STATUS) +
			(dam_id * TEGRA30_AHUB_DAM_LIVE_STATUS_STRIDE));
	mask = TEGRA30_AHUB_DAM_LIVE_STATUS_TX_ENABLED;
	val &= mask;

	return val;
}


int tegra30_ahub_dam_ch0_is_empty(int dam_id)
{
	int val, mask;

	val = tegra30_apbif_read((TEGRA30_AHUB_DAM_LIVE_STATUS) +
			(dam_id * TEGRA30_AHUB_DAM_LIVE_STATUS_STRIDE));
	mask = TEGRA30_AHUB_DAM_LIVE_STATUS_RX0FIFO_EMPTY;
	val &= mask;

	return val;
}

int tegra30_ahub_dam_ch1_is_empty(int dam_id)
{
	int val, mask;

	val = tegra30_apbif_read((TEGRA30_AHUB_DAM_LIVE_STATUS) +
			(dam_id * TEGRA30_AHUB_DAM_LIVE_STATUS_STRIDE));
	mask = TEGRA30_AHUB_DAM_LIVE_STATUS_RX1FIFO_EMPTY;
	val &= mask;

	return val;
}

int tegra30_ahub_dam_tx_is_empty(int dam_id)
{
	int val, mask;

	val = tegra30_apbif_read((TEGRA30_AHUB_DAM_LIVE_STATUS) +
			(dam_id * TEGRA30_AHUB_DAM_LIVE_STATUS_STRIDE));
	mask = TEGRA30_AHUB_DAM_LIVE_STATUS_TXFIFO_EMPTY;
	val &= mask;

	return val;
}


int tegra30_ahub_set_rx_fifo_pack_mode(enum tegra30_ahub_rxcif rxcif,
							unsigned int pack_mode)
{
	int channel = rxcif - TEGRA30_AHUB_RXCIF_APBIF_RX0;
	int reg, val;

	tegra30_ahub_enable_clocks();
	reg = TEGRA30_AHUB_CHANNEL_CTRL +
	      (channel * TEGRA30_AHUB_CHANNEL_CTRL_STRIDE);
	val = tegra30_apbif_read(reg);

	val &= ~TEGRA30_AHUB_CHANNEL_CTRL_RX_PACK_MASK;
	val &= ~TEGRA30_AHUB_CHANNEL_CTRL_RX_PACK_EN;

	if ((pack_mode == TEGRA30_AHUB_CHANNEL_CTRL_RX_PACK_16) ||
		(pack_mode == TEGRA30_AHUB_CHANNEL_CTRL_RX_PACK_8_4))
		val |= (TEGRA30_AHUB_CHANNEL_CTRL_RX_PACK_EN |
						pack_mode);
	tegra30_apbif_write(reg, val);
	tegra30_ahub_disable_clocks();

	return 0;
}

int tegra30_ahub_set_tx_fifo_pack_mode(enum tegra30_ahub_txcif txcif,
							unsigned int pack_mode)
{
	int channel = txcif - TEGRA30_AHUB_TXCIF_APBIF_TX0;
	int reg, val;

	tegra30_ahub_enable_clocks();
	reg = TEGRA30_AHUB_CHANNEL_CTRL +
	      (channel * TEGRA30_AHUB_CHANNEL_CTRL_STRIDE);
	val = tegra30_apbif_read(reg);

	val &= ~TEGRA30_AHUB_CHANNEL_CTRL_TX_PACK_MASK;
	val &= ~TEGRA30_AHUB_CHANNEL_CTRL_TX_PACK_EN;

	if ((pack_mode == TEGRA30_AHUB_CHANNEL_CTRL_TX_PACK_16) ||
		(pack_mode == TEGRA30_AHUB_CHANNEL_CTRL_TX_PACK_8_4))
		val |= (TEGRA30_AHUB_CHANNEL_CTRL_TX_PACK_EN |
						pack_mode);
	tegra30_apbif_write(reg, val);
	tegra30_ahub_disable_clocks();

	return 0;
}

int tegra30_ahub_enable_rx_fifo(enum tegra30_ahub_rxcif rxcif)
{
	int channel = rxcif - TEGRA30_AHUB_RXCIF_APBIF_RX0;
	int reg, val;

	tegra30_ahub_enable_clocks();

	reg = TEGRA30_AHUB_CHANNEL_CTRL +
	      (channel * TEGRA30_AHUB_CHANNEL_CTRL_STRIDE);
	val = tegra30_apbif_read(reg);
	val |= TEGRA30_AHUB_CHANNEL_CTRL_RX_EN;
	tegra30_apbif_write(reg, val);

	return 0;
}

int tegra30_ahub_disable_rx_fifo(enum tegra30_ahub_rxcif rxcif)
{
	int channel = rxcif - TEGRA30_AHUB_RXCIF_APBIF_RX0;
	int reg, val;

	reg = TEGRA30_AHUB_CHANNEL_CTRL +
	      (channel * TEGRA30_AHUB_CHANNEL_CTRL_STRIDE);
	val = tegra30_apbif_read(reg);
	val &= ~TEGRA30_AHUB_CHANNEL_CTRL_RX_EN;
	tegra30_apbif_write(reg, val);

	tegra30_ahub_disable_clocks();

	return 0;
}

int tegra30_ahub_free_rx_fifo(enum tegra30_ahub_rxcif rxcif)
{
	int channel = rxcif - TEGRA30_AHUB_RXCIF_APBIF_RX0;

	__clear_bit(channel, ahub->rx_usage);

	return 0;
}

int tegra30_ahub_allocate_tx_fifo(enum tegra30_ahub_txcif *txcif,
				  unsigned long *fiforeg,
				  unsigned long *reqsel)
{
	int channel;
	u32 reg, val;

	channel = find_first_zero_bit(ahub->tx_usage,
				      TEGRA30_AHUB_CHANNEL_CTRL_COUNT);
	if (channel >= TEGRA30_AHUB_CHANNEL_CTRL_COUNT)
		return -EBUSY;

	__set_bit(channel, ahub->tx_usage);

	*txcif = TEGRA30_AHUB_TXCIF_APBIF_TX0 + channel;
	*fiforeg = ahub->apbif_addr + TEGRA30_AHUB_CHANNEL_TXFIFO +
		   (channel * TEGRA30_AHUB_CHANNEL_TXFIFO_STRIDE);
	*reqsel = TEGRA_DMA_REQ_SEL_APBIF_CH0 + channel;

	tegra30_ahub_enable_clocks();

	reg = TEGRA30_AHUB_CHANNEL_CTRL +
	      (channel * TEGRA30_AHUB_CHANNEL_CTRL_STRIDE);
	val = tegra30_apbif_read(reg);
	val &= ~(TEGRA30_AHUB_CHANNEL_CTRL_TX_THRESHOLD_MASK);
	val |= (7 << TEGRA30_AHUB_CHANNEL_CTRL_TX_THRESHOLD_SHIFT);
	tegra30_apbif_write(reg, val);

	reg = TEGRA30_AHUB_CIF_TX_CTRL +
	      (channel * TEGRA30_AHUB_CIF_TX_CTRL_STRIDE);
	val = (0 << TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT) |
	      (1 << TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
	      (1 << TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT) |
	      TEGRA30_AUDIOCIF_CTRL_DIRECTION_TX;
	tegra30_apbif_write(reg, val);

	tegra30_ahub_disable_clocks();

	return 0;
}

int tegra30_ahub_enable_tx_fifo(enum tegra30_ahub_txcif txcif)
{
	int channel = txcif - TEGRA30_AHUB_TXCIF_APBIF_TX0;
	int reg, val;

	tegra30_ahub_enable_clocks();

	reg = TEGRA30_AHUB_CHANNEL_CTRL +
	      (channel * TEGRA30_AHUB_CHANNEL_CTRL_STRIDE);
	val = tegra30_apbif_read(reg);
	val |= TEGRA30_AHUB_CHANNEL_CTRL_TX_EN;
	tegra30_apbif_write(reg, val);

	return 0;
}

int tegra30_ahub_disable_tx_fifo(enum tegra30_ahub_txcif txcif)
{
	int channel = txcif - TEGRA30_AHUB_TXCIF_APBIF_TX0;
	int reg, val;

	reg = TEGRA30_AHUB_CHANNEL_CTRL +
	      (channel * TEGRA30_AHUB_CHANNEL_CTRL_STRIDE);
	val = tegra30_apbif_read(reg);
	val &= ~TEGRA30_AHUB_CHANNEL_CTRL_TX_EN;
	tegra30_apbif_write(reg, val);

	tegra30_ahub_disable_clocks();

	return 0;
}

int tegra30_ahub_free_tx_fifo(enum tegra30_ahub_txcif txcif)
{
	int channel = txcif - TEGRA30_AHUB_TXCIF_APBIF_TX0;

	__clear_bit(channel, ahub->tx_usage);

	return 0;
}

int tegra30_ahub_set_rx_cif_source(enum tegra30_ahub_rxcif rxcif,
				   enum tegra30_ahub_txcif txcif)
{
	int channel = rxcif - TEGRA30_AHUB_RXCIF_APBIF_RX0;
	int reg;

	tegra30_ahub_enable_clocks();

	reg = TEGRA30_AHUB_AUDIO_RX +
	      (channel * TEGRA30_AHUB_AUDIO_RX_STRIDE);
	tegra30_audio_write(reg, 1 << txcif);

	tegra30_ahub_disable_clocks();

	return 0;
}

int tegra30_ahub_unset_rx_cif_source(enum tegra30_ahub_rxcif rxcif)
{
	int channel = rxcif - TEGRA30_AHUB_RXCIF_APBIF_RX0;
	int reg;

	tegra30_ahub_enable_clocks();

	reg = TEGRA30_AHUB_AUDIO_RX +
	      (channel * TEGRA30_AHUB_AUDIO_RX_STRIDE);
	tegra30_audio_write(reg, 0);

	tegra30_ahub_disable_clocks();

	return 0;
}

int tegra30_ahub_set_rx_cif_channels(enum tegra30_ahub_rxcif rxcif,
				     unsigned int audio_ch,
				     unsigned int client_ch)
{
	int channel = rxcif - TEGRA30_AHUB_RXCIF_APBIF_RX0;
	unsigned int reg, val;

	tegra30_ahub_enable_clocks();

	reg = TEGRA30_AHUB_CIF_RX_CTRL +
	      (channel * TEGRA30_AHUB_CIF_RX_CTRL_STRIDE);
	val = tegra30_apbif_read(reg);
	val &= ~(TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_MASK |
		TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_MASK);
	val |= ((audio_ch - 1) << TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
	      ((client_ch - 1) << TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT);
	tegra30_apbif_write(reg, val);

	tegra30_ahub_disable_clocks();

	return 0;
}

int tegra30_ahub_set_tx_cif_channels(enum tegra30_ahub_txcif txcif,
				     unsigned int audio_ch,
				     unsigned int client_ch)
{
	int channel = txcif - TEGRA30_AHUB_TXCIF_APBIF_TX0;
	unsigned int reg, val;

	tegra30_ahub_enable_clocks();

	reg = TEGRA30_AHUB_CIF_TX_CTRL +
	      (channel * TEGRA30_AHUB_CIF_TX_CTRL_STRIDE);
	val = tegra30_apbif_read(reg);
	val &= ~(TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_MASK |
		TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_MASK);
	val |= ((audio_ch - 1) << TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
	      ((client_ch - 1) << TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT);

	tegra30_apbif_write(reg, val);

	tegra30_ahub_disable_clocks();

	return 0;
}

int tegra30_ahub_set_rx_cif_bits(enum tegra30_ahub_rxcif rxcif,
				     unsigned int audio_bits,
				     unsigned int client_bits)
{
	int channel = rxcif - TEGRA30_AHUB_RXCIF_APBIF_RX0;
	unsigned int reg, val;

	tegra30_ahub_enable_clocks();

	reg = TEGRA30_AHUB_CIF_RX_CTRL +
	      (channel * TEGRA30_AHUB_CIF_RX_CTRL_STRIDE);
	val = tegra30_apbif_read(reg);
	val &= ~(TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_MASK |
		TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_MASK);
	val |= ((audio_bits) << TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_SHIFT) |
	      ((client_bits) << TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_SHIFT);
	tegra30_apbif_write(reg, val);

	tegra30_ahub_disable_clocks();

	return 0;
}

int tegra30_ahub_set_tx_cif_bits(enum tegra30_ahub_txcif txcif,
				     unsigned int audio_bits,
				     unsigned int client_bits)
{
	int channel = txcif - TEGRA30_AHUB_TXCIF_APBIF_TX0;
	unsigned int reg, val;

	tegra30_ahub_enable_clocks();

	reg = TEGRA30_AHUB_CIF_TX_CTRL +
	      (channel * TEGRA30_AHUB_CIF_TX_CTRL_STRIDE);
	val = tegra30_apbif_read(reg);
	val &= ~(TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_MASK |
		TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_MASK);
	val |= ((audio_bits) << TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_SHIFT) |
	      ((client_bits) << TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_SHIFT);

	tegra30_apbif_write(reg, val);

	tegra30_ahub_disable_clocks();

	return 0;
}


static int __devinit tegra30_ahub_probe(struct platform_device *pdev)
{
	struct resource *res0, *res1, *region;
	int ret = 0;
#ifdef CONFIG_PM
	int i = 0, cache_idx_rsvd;
#endif
	int clkm_rate;

	if (ahub)
		return -ENODEV;

	ahub = kzalloc(sizeof(struct tegra30_ahub), GFP_KERNEL);
	if (!ahub) {
		dev_err(&pdev->dev, "Can't allocate tegra30_ahub\n");
		ret = -ENOMEM;
		goto exit;
	}
	ahub->dev = &pdev->dev;

	ahub->clk_d_audio = clk_get(&pdev->dev, "d_audio");
	if (IS_ERR(ahub->clk_d_audio)) {
		dev_err(&pdev->dev, "Can't retrieve ahub d_audio clock\n");
		ret = PTR_ERR(ahub->clk_d_audio);
		goto err_free;
	}
	clkm_rate = clk_get_rate(clk_get_parent(ahub->clk_d_audio));

	while (clkm_rate > 13000000)
		clkm_rate >>= 1;

	clk_set_rate(ahub->clk_d_audio,clkm_rate);

	ahub->clk_apbif = clk_get(&pdev->dev, "apbif");
	if (IS_ERR(ahub->clk_apbif)) {
		dev_err(&pdev->dev, "Can't retrieve ahub apbif clock\n");
		ret = PTR_ERR(ahub->clk_apbif);
		goto err_clk_put_d_audio;
	}

	res0 = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res0) {
		dev_err(&pdev->dev, "No memory 0 resource\n");
		ret = -ENODEV;
		goto err_clk_put_apbif;
	}

	region = request_mem_region(res0->start, resource_size(res0),
				    pdev->name);
	if (!region) {
		dev_err(&pdev->dev, "Memory region 0 already claimed\n");
		ret = -EBUSY;
		goto err_clk_put_apbif;
	}

	ahub->apbif_regs = ioremap(res0->start, resource_size(res0));
	if (!ahub->apbif_regs) {
		dev_err(&pdev->dev, "ioremap 0 failed\n");
		ret = -ENOMEM;
		goto err_release0;
	}

	ahub->apbif_addr = res0->start;

	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res1) {
		dev_err(&pdev->dev, "No memory 1 resource\n");
		ret = -ENODEV;
		goto err_unmap0;
	}

	region = request_mem_region(res1->start, resource_size(res1),
				    pdev->name);
	if (!region) {
		dev_err(&pdev->dev, "Memory region 1 already claimed\n");
		ret = -EBUSY;
		goto err_unmap0;
	}

	ahub->audio_regs = ioremap(res1->start, resource_size(res1));
	if (!ahub->audio_regs) {
		dev_err(&pdev->dev, "ioremap 1 failed\n");
		ret = -ENOMEM;
		goto err_release1;
	}

#ifdef CONFIG_PM
	/* cache the POR values of ahub/apbif regs*/
	tegra30_ahub_enable_clocks();

	for (i = 0; i < TEGRA30_AHUB_AUDIO_RX_COUNT; i++)
		ahub->ahub_reg_cache[i] = tegra30_audio_read(i<<2);

	cache_idx_rsvd = TEGRA30_APBIF_CACHE_REG_INDEX_RSVD;
	for (i = 0; i < TEGRA30_APBIF_CACHE_REG_COUNT; i++) {
		if (i == cache_idx_rsvd) {
			cache_idx_rsvd +=
				TEGRA30_APBIF_CACHE_REG_INDEX_RSVD_STRIDE;
			continue;
		}

		ahub->apbif_reg_cache[i] = tegra30_apbif_read(i<<2);
	}

	tegra30_ahub_disable_clocks();
#endif

	tegra30_ahub_debug_add(ahub);

	platform_set_drvdata(pdev, ahub);

	return 0;

err_release1:
	release_mem_region(res1->start, resource_size(res1));
err_unmap0:
	iounmap(ahub->apbif_regs);
err_release0:
	release_mem_region(res0->start, resource_size(res0));
err_clk_put_apbif:
	clk_put(ahub->clk_apbif);
err_clk_put_d_audio:
	clk_put(ahub->clk_d_audio);
err_free:
	kfree(ahub);
	ahub = 0;
exit:
	return ret;
}

static int __devexit tegra30_ahub_remove(struct platform_device *pdev)
{
	struct resource *res;

	if (!ahub)
		return -ENODEV;

	platform_set_drvdata(pdev, NULL);

	tegra30_ahub_debug_remove(ahub);

	iounmap(ahub->audio_regs);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	release_mem_region(res->start, resource_size(res));

	iounmap(ahub->apbif_regs);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, resource_size(res));

	clk_put(ahub->clk_apbif);
	clk_put(ahub->clk_d_audio);

	kfree(ahub);
	ahub = 0;

	return 0;
}

static struct platform_driver tegra30_ahub_driver = {
	.probe = tegra30_ahub_probe,
	.remove = __devexit_p(tegra30_ahub_remove),
	.driver = {
		.name = DRV_NAME,
	},
};

static int __init tegra30_ahub_modinit(void)
{
	return platform_driver_register(&tegra30_ahub_driver);
}
module_init(tegra30_ahub_modinit);

static void __exit tegra30_ahub_modexit(void)
{
	platform_driver_unregister(&tegra30_ahub_driver);
}
module_exit(tegra30_ahub_modexit);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra 30 AHUB driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
