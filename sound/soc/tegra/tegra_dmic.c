/*
 * tegra_dmic.c - Tegra DMIC driver
 *
 * Author: Ankit Gupta <ankitgupta@nvidia.com>
 * Copyright (C) 2013, NVIDIA CORPORATION. All rights reserved.
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
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include <asm/io.h>
#include <mach/iomap.h>

#include "tegra_pcm.h"
#include "tegra_dmic.h"
#include "tegra30_ahub.h"

#define DRV_NAME "tegra-dmic"

struct tegra_dmic {
	struct device *dev;
	struct dentry *debug;
	struct tegra_pcm_dma_params capture_dma_data;
	enum tegra30_ahub_rxcif rx_cif;
	void __iomem *io_base;
	struct clk *clk;
	struct clk *parent;
	struct mutex mutex;
	u32 over_sampling_ratio;
	u32 sampling_freq;
	u32 sys_clk_rate;
	u32 channels;
	u32 data_format;
};

static inline void tegra_dmic_write(struct tegra_dmic *dmic, u32 reg,
	u32 val)
{
	__raw_writel(val, dmic->io_base + reg);
}

static inline u32 tegra_dmic_read(struct tegra_dmic *dmic, u32 reg)
{
	return __raw_readl(dmic->io_base + reg);
}

#ifdef CONFIG_DEBUG_FS
static int tegra_dmic_show(struct seq_file *s, void *unused)
{
#define REG(r) { r, #r }
	static const struct {
		int offset;
		const char *name;
	} regs[] = {
		REG(TEGRA_DMIC_CTRL),
		REG(TEGRA_DMIC_FILTER_CTRL),
		REG(TEGRA_DMIC_AUDIO_CIF_TX_CTRL),
		REG(TEGRA_DMIC_DCR_FILTER_GAIN),
	};
#undef REG

	struct tegra_dmic *dmic = s->private;
	int i;

	clk_enable(dmic->clk);

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		u32 val = tegra_dmic_read(dmic, regs[i].offset);
		seq_printf(s, "%s = %08x\n", regs[i].name, val);
	}

	clk_disable(dmic->clk);

	return 0;
}

static int tegra_dmic_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra_dmic_show, inode->i_private);
}

static const struct file_operations tegra_dmic_debug_fops = {
	.open    = tegra_dmic_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static void tegra_dmic_debug_add(struct tegra_dmic *dmic, int id)
{
	char name[] = DRV_NAME ".0";

	snprintf(name, sizeof(name), DRV_NAME".%1d", id);
	dmic->debug = debugfs_create_file(name, S_IRUGO,
		snd_soc_debugfs_root, dmic, &tegra_dmic_debug_fops);
}

static void tegra_dmic_debug_remove(struct tegra_dmic *dmic)
{
	if (dmic->debug)
		debugfs_remove(dmic->debug);
}
#else
static inline void tegra_dmic_debug_add(struct tegra_dmic *dmic, int id)
{
}

static inline void tegra_dmic_debug_remove(struct tegra_dmic *dmic)
{
}
#endif

static void tegra_dmic_soft_reset(struct tegra_dmic *dmic)
{
	u32 ctrl;

	ctrl = tegra_dmic_read(dmic, TEGRA_DMIC_FILTER_CTRL);
	ctrl &= TEGRA_DMIC_SOFT_RESET_MASK;
	ctrl |= 0x1 << TEGRA_DMIC_SOFT_RESET_SHIFT;
	tegra_dmic_write(dmic, TEGRA_DMIC_FILTER_CTRL, ctrl);

	/* Now continuosly poll for reset bit to go low. */
	while (ctrl & (0x1 << TEGRA_DMIC_SOFT_RESET_SHIFT)) {
		ctrl = tegra_dmic_read(dmic, TEGRA_DMIC_FILTER_CTRL);
		ctrl &= TEGRA_DMIC_SOFT_RESET_MASK;
	}
}

static void tegra_dmic_init_params(struct tegra_dmic *dmic)
{
	dmic->channels = 0;
	dmic->data_format = 0;
	dmic->over_sampling_ratio = 0;
	dmic->sampling_freq = 0;
	dmic->sys_clk_rate = 0;

	/*
	 * Hard code register settings to disable sync clock for DMIC.
	 */
	__raw_writel(0x1 << 4 , IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x560);
	__raw_writel(0x1 << 4 , IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x564);
}

void tegra_dmic_set_acif(struct snd_soc_dai *dai,
	struct tegra_dmic_cif *cif_info)
{
	struct tegra_dmic *dmic = snd_soc_dai_get_drvdata(dai);
	u32 ctrl;

	mutex_lock(&dmic->mutex);
	ctrl = tegra_dmic_read(dmic, TEGRA_DMIC_AUDIO_CIF_TX_CTRL);

	ctrl &= ~TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_MASK;
	ctrl |= (cif_info->threshold <<
		TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT);

	ctrl &= ~TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_MASK;
	ctrl |= ((cif_info->audio_channels - 1) <<
		TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT);

	ctrl &= ~TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_MASK;
	ctrl |= ((cif_info->client_channels - 1) <<
		TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT);

	ctrl &= ~TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_MASK;
	ctrl |= (cif_info->audio_bits <<
		TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_SHIFT);

	ctrl &= ~TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_MASK;
	ctrl |= (cif_info->client_bits <<
		TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_SHIFT);

	ctrl &= ~TEGRA30_AUDIOCIF_CTRL_EXPAND_MASK;
	ctrl |= (cif_info->expand <<
		TEGRA30_AUDIOCIF_CTRL_EXPAND_SHIFT);

	ctrl &= ~TEGRA30_AUDIOCIF_CTRL_STEREO_CONV_MASK;
	ctrl |= (cif_info->stereo_conv <<
		TEGRA30_AUDIOCIF_CTRL_STEREO_CONV_SHIFT);

	ctrl &= ~TEGRA30_AUDIOCIF_CTRL_REPLICATE_MASK;
	ctrl |= (cif_info->replicate <<
		TEGRA30_AUDIOCIF_CTRL_REPLICATE_SHIFT);

	ctrl &= ~TEGRA30_AUDIOCIF_CTRL_TRUNCATE_MASK;
	ctrl |= (cif_info->truncate <<
		TEGRA30_AUDIOCIF_CTRL_TRUNCATE_SHIFT);

	ctrl &= ~TEGRA30_AUDIOCIF_CTRL_MONO_CONV_MASK;
	ctrl |= (cif_info->mono_conv <<
		TEGRA30_AUDIOCIF_CTRL_MONO_CONV_SHIFT);

	tegra_dmic_write(dmic, TEGRA_DMIC_AUDIO_CIF_TX_CTRL, ctrl);
	mutex_unlock(&dmic->mutex);
}

void tegra_dmic_set_rx_cif(struct snd_soc_dai *dai,
	struct tegra_dmic_cif *cif_info, u32 data_format)
{
	struct tegra_dmic *dmic = snd_soc_dai_get_drvdata(dai);

	mutex_lock(&dmic->mutex);
	tegra30_ahub_set_rx_cif_bits(dmic->rx_cif, cif_info->audio_bits,
			cif_info->client_bits);
	tegra30_ahub_set_rx_cif_channels(dmic->rx_cif,
			cif_info->audio_channels,
			cif_info->client_channels);

	if (data_format == SNDRV_PCM_FORMAT_S16_LE)
		tegra30_ahub_set_rx_fifo_pack_mode(dmic->rx_cif,
			TEGRA30_AHUB_CHANNEL_CTRL_RX_PACK_16);
	else
		tegra30_ahub_set_rx_fifo_pack_mode(dmic->rx_cif, 0);

	if (cif_info->client_channels == 1)
		tegra30_ahub_set_rx_cif_stereo_conv(dmic->rx_cif);

	mutex_unlock(&dmic->mutex);
}

static void tegra_dmic_start(struct tegra_dmic *dmic)
{
	u32 ctrl;

	mutex_lock(&dmic->mutex);
	/* Enable Rx FIFOs. */
	tegra30_ahub_enable_rx_fifo(dmic->rx_cif);

	ctrl = tegra_dmic_read(dmic, TEGRA_DMIC_CTRL);
	/*
	 * Caution : Set DMIC h/w params before enabling it.
	 */
	ctrl &= TEGRA_DMIC_ENABLE_MASK;
	ctrl |= 0x1 << TEGRA_DMIC_ENABLE_SHIFT;
	tegra_dmic_write(dmic, TEGRA_DMIC_CTRL, ctrl);
	mutex_unlock(&dmic->mutex);
}

static void tegra_dmic_stop(struct tegra_dmic *dmic)
{
	u32 ctrl;

	mutex_lock(&dmic->mutex);
	ctrl = tegra_dmic_read(dmic, TEGRA_DMIC_CTRL);

	ctrl &= TEGRA_DMIC_ENABLE_MASK;
	ctrl |= 0x0 << TEGRA_DMIC_ENABLE_SHIFT;
	tegra_dmic_write(dmic, TEGRA_DMIC_CTRL, ctrl);
	tegra_dmic_soft_reset(dmic);

	/* Disable Rx FIFOs. */
	tegra30_ahub_disable_rx_fifo(dmic->rx_cif);
	mutex_unlock(&dmic->mutex);
}

static int tegra_dmic_dai_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct tegra_dmic *dmic = snd_soc_dai_get_drvdata(dai);
	u32 dmic_cif_id;
	int ret = 0;

	mutex_lock(&dmic->mutex);

	tegra30_ahub_enable_clocks();
	ret = clk_enable(dmic->clk);
	if (ret < 0)
		dev_err(dmic->dev, "Can't enable DMIC clock\n");

	/* Obtain Rx FIFOs and setup connections. */

	ret = tegra30_ahub_allocate_rx_fifo(&dmic->rx_cif,
		&dmic->capture_dma_data.addr,
		&dmic->capture_dma_data.req_sel);
	dmic->capture_dma_data.wrap = 4;
	dmic->capture_dma_data.width = 32;

	if (dai->id == TEGRA_DMIC_FRONT)
		dmic_cif_id = TEGRA30_AHUB_TXCIF_DMIC0_TX0;
	else if (dai->id == TEGRA_DMIC_BACK)
		dmic_cif_id = TEGRA30_AHUB_TXCIF_DMIC1_TX0;
	else
		return -EINVAL;

	tegra30_ahub_set_rx_cif_source(dmic->rx_cif,
		dmic_cif_id);

	mutex_unlock(&dmic->mutex);

	return ret;
}

static void tegra_dmic_dai_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct tegra_dmic *dmic = snd_soc_dai_get_drvdata(dai);

	mutex_lock(&dmic->mutex);

	/* Free up FIFOs and unset connection. */
	tegra30_ahub_free_rx_fifo(dmic->rx_cif);
	tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_APBIF_RX0);

	clk_disable(dmic->clk);
	tegra30_ahub_disable_clocks();
	mutex_unlock(&dmic->mutex);
}

static int tegra_dmic_dai_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct tegra_dmic *dmic = snd_soc_dai_get_drvdata(dai);
	u32 ctrl;

	mutex_lock(&dmic->mutex);
	ctrl = tegra_dmic_read(dmic, TEGRA_DMIC_CTRL);

	dmic->channels = params_channels(params);
	dmic->sampling_freq = params_rate(params);
	dmic->data_format = params_format(params);

	/*
	 * Always operate DMIC in stereo mode. Conversion to mono,
	 * can be done at later stage.
	 */
	ctrl &= TEGRA_DMIC_CHANNEL_SELECT_MASK;
	ctrl |= (0x3 << TEGRA_DMIC_CHANNEL_SELECT_SHIFT);

	ctrl &= TEGRA_DMIC_OSR_MASK;
	switch (dmic->sampling_freq) {
	case 8000:
		ctrl |= 0x1 << TEGRA_DMIC_OSR_SHIFT; /* OSR = 128 */
		dmic->over_sampling_ratio = 128;
		break;
	case 16000:
	case 44100:
	case 48000:
		ctrl |= 0x0 << TEGRA_DMIC_OSR_SHIFT; /* OSR = 64 */
		dmic->over_sampling_ratio = 64;
		break;
	default:
		return -EINVAL;
	}

	ctrl &= TEGRA_DMIC_OUTPUT_WIDTH_MASK;
	switch (dmic->data_format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		ctrl |= 0x0 << TEGRA_DMIC_OUTPUT_WIDTH_SHIFT;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		ctrl |= 0x1 << TEGRA_DMIC_OUTPUT_WIDTH_SHIFT;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		ctrl |= 0x2 << TEGRA_DMIC_OUTPUT_WIDTH_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	tegra_dmic_write(dmic, TEGRA_DMIC_CTRL, ctrl);
	mutex_unlock(&dmic->mutex);

	return 0;
}

static int tegra_dmic_dai_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct tegra_dmic *dmic = snd_soc_dai_get_drvdata(dai);
	u32 ctrl;

	/*
	 * Configure DMIC filters.
	 * Currently, only SC filter is turned on, as per
	 * hardware specifications.
	 */
	mutex_lock(&dmic->mutex);
	ctrl = tegra_dmic_read(dmic, TEGRA_DMIC_FILTER_CTRL);

	ctrl &= TEGRA_DMIC_DCR_FILTER_MASK;
	ctrl |= 0x0 << TEGRA_DMIC_DCR_FILTER_SHIFT;

	ctrl &= TEGRA_DMIC_LP_FILTER_MASK;
	ctrl |= 0x0 << TEGRA_DMIC_LP_FILTER_SHIFT;

	ctrl &= TEGRA_DMIC_SC_FILTER_MASK;
	ctrl |= 0x1 << TEGRA_DMIC_SC_FILTER_SHIFT;

	ctrl &= TEGRA_DMIC_TRIMMER_SEL_MASK;
	ctrl |= 0x0 << TEGRA_DMIC_TRIMMER_SEL_SHIFT;

	tegra_dmic_write(dmic, TEGRA_DMIC_FILTER_CTRL, ctrl);

	/* Configure DMIC engine fields */
	ctrl = tegra_dmic_read(dmic, TEGRA_DMIC_CTRL);

	ctrl &= TEGRA_DMIC_LRSEL_POLARITY_MASK;
	ctrl |= 0x0 << TEGRA_DMIC_LRSEL_POLARITY_SHIFT;

	ctrl &= TEGRA_DMIC_CLOCKING_GATE_MASK;
	ctrl |= 0x1 << TEGRA_DMIC_CLOCKING_GATE_SHIFT;

	ctrl &= TEGRA_DMIC_SINC_DEC_ORDER_MASK;
	ctrl |= 0x1 << TEGRA_DMIC_SINC_DEC_ORDER_SHIFT;

	ctrl &= TEGRA_DMIC_DMICFILT_BYPASS_MASK;
	ctrl |= 0x0 << TEGRA_DMIC_DMICFILT_BYPASS_SHIFT;

	tegra_dmic_write(dmic, TEGRA_DMIC_CTRL, ctrl);
	mutex_unlock(&dmic->mutex);

	return 0;
}

static int tegra_dmic_dai_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct tegra_dmic *dmic = snd_soc_dai_get_drvdata(dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		tegra_dmic_start(dmic);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		tegra_dmic_stop(dmic);
		break;
	default:
		break;
	}

	return 0;
}

static int tegra_dmic_set_dai_sysclk(struct snd_soc_dai *dai,
	int clk_id, unsigned int freq, int dir)
{
	struct tegra_dmic *dmic = snd_soc_dai_get_drvdata(dai);
	int ret;

	mutex_lock(&dmic->mutex);
	dmic->sys_clk_rate = freq;
	ret = clk_set_rate(dmic->clk, dmic->sys_clk_rate);
	if (ret < 0) {
		dev_err(dmic->dev, "Can't set clock rate\n");
		return -EPERM;
	}

	mutex_unlock(&dmic->mutex);
	return 0;
}

static const struct snd_soc_dai_ops tegra_dmic_dai_ops = {
	.startup	= tegra_dmic_dai_startup,
	.shutdown	= tegra_dmic_dai_shutdown,
	.hw_params	= tegra_dmic_dai_hw_params,
	.prepare	= tegra_dmic_dai_prepare,
	.trigger	= tegra_dmic_dai_trigger,
	.set_sysclk	= tegra_dmic_set_dai_sysclk,
};

static int tegra_dmic_probe(struct snd_soc_dai *dai)
{
	struct tegra_dmic *dmic = snd_soc_dai_get_drvdata(dai);
	dai->capture_dma_data = &dmic->capture_dma_data;

	return 0;
}

static int tegra_dmic_remove(struct snd_soc_dai *dai)
{
	return 0;
}

/*
 * Create two DMIC dai's. One for front mic and other for back mic.
 * Front mic will be used for capturing pertinent data, while back
 * mic is useful to capture white noise which helps in certain
 * applications requiring Noise Cancellation.
 */

static struct snd_soc_dai_driver tegra_dmic_dai[] = {
	{
		.name = "tegra-dmic.0",
		.probe = tegra_dmic_probe,
		.remove = tegra_dmic_remove,
		.id = TEGRA_DMIC_FRONT,
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = TEGRA_DMIC_RATES,
			.formats = TEGRA_DMIC_FORMATS,
		},
		.ops = &tegra_dmic_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "tegra-dmic.1",
		.probe = tegra_dmic_probe,
		.remove = tegra_dmic_remove,
		.id = TEGRA_DMIC_BACK,
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = TEGRA_DMIC_RATES,
			.formats = TEGRA_DMIC_FORMATS,
		},
		.ops = &tegra_dmic_dai_ops,
		.symmetric_rates = 1,
	}
};

static __devinit int asoc_dmic_probe(struct platform_device *pdev)
{
	struct tegra_dmic *dmic;
	struct resource *dmic_resource, *dmic_region;
	int ret = 0;
	char *dmic_clk_name;
	long unsigned parent_rate;

	dmic = devm_kzalloc(&pdev->dev, sizeof(struct tegra_dmic),
		GFP_KERNEL);
	if (!dmic)
		return -ENOMEM;

	platform_set_drvdata(pdev, dmic);
	dmic->dev = &pdev->dev;
	tegra_dmic_init_params(dmic);
	mutex_init(&dmic->mutex);

	switch (pdev->id) {
	case TEGRA_DMIC_FRONT:
		dmic_clk_name = "dmic0";
		break;
	case TEGRA_DMIC_BACK:
		dmic_clk_name = "dmic1";
		break;
	default:
		ret = -EINVAL;
		goto err_free_dmic;
	}

	dmic->clk = clk_get(dmic->dev, dmic_clk_name);
	if (IS_ERR(dmic->clk)) {
		dev_err(dmic->dev, "cant get dmic clk\n");
		ret = -ENODEV;
		goto err_put_clk;
	}

	dmic->parent = clk_get(dmic->dev, "pll_a_out0");
	if (IS_ERR(dmic->parent)) {
		dev_err(dmic->dev, "cant get dmic parent\n");
		ret = -ENODEV;
		goto err_put_clk;
	}

	parent_rate = clk_get_rate(dmic->parent);
	ret = clk_set_rate(dmic->clk, parent_rate);
	if (ret < 0) {
		dev_err(dmic->dev, "can't set dmic clk rate\n");
		goto err_put_clk;
	}

	dmic_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!dmic_resource) {
		dev_err(&pdev->dev, "No memory 0 resource\n");
		ret = -ENODEV;
		goto err_put_clk;
	}

	dmic_region = devm_request_mem_region(&pdev->dev,
			dmic_resource->start, resource_size(dmic_resource),
			pdev->name);
	if (!dmic_region) {
		dev_err(&pdev->dev, "Memory region 0 already claimed\n");
		ret = -EBUSY;
		goto err_put_clk;
	}

	dmic->io_base = devm_ioremap(&pdev->dev, dmic_resource->start,
			resource_size(dmic_resource));
	if (!dmic->io_base) {
		dev_err(&pdev->dev, "ioremap 0 failed\n");
		ret = -ENOMEM;
		goto err_put_clk;
	}

	ret = snd_soc_register_dai(&pdev->dev, &tegra_dmic_dai[pdev->id]);
	if (ret) {
		dev_err(&pdev->dev, "dai registration failed\n");
		ret = -EBUSY;
		goto err_put_clk;
	}

	ret = tegra_pcm_platform_register(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Could not register PCM: %d\n", ret);
		goto err_unregister_dai;
	}

	tegra_dmic_debug_add(dmic, pdev->id);
	return 0;

err_unregister_dai:
	snd_soc_unregister_dai(&pdev->dev);

err_put_clk:
	if (dmic->parent)
		clk_put(dmic->parent);

	if (dmic->clk)
		clk_put(dmic->clk);

err_free_dmic:
	kfree(dmic);

	return ret;
}

static int __devexit asoc_dmic_remove(struct platform_device *pdev)
{
	struct tegra_dmic *dmic = platform_get_drvdata(pdev);
	struct resource *res;

	tegra_dmic_debug_remove(dmic);
	tegra_pcm_platform_unregister(&pdev->dev);
	snd_soc_unregister_dai(&pdev->dev);
	iounmap(dmic->io_base);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, resource_size(res));
	else
		dev_err(&pdev->dev, "failed to get memory resource\n");

	if (dmic->parent)
		clk_put(dmic->parent);

	if (dmic->clk)
		clk_put(dmic->clk);

	kfree(dmic);

	return 0;
}

static struct platform_driver asoc_dmic_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = asoc_dmic_probe,
	.remove = __devexit_p(asoc_dmic_remove),
};

static int __init tegra_dmic_modinit(void)
{
	return platform_driver_register(&asoc_dmic_driver);
}
module_init(tegra_dmic_modinit);

static void __exit tegra_dmic_modexit(void)
{
	platform_driver_unregister(&asoc_dmic_driver);
}
module_exit(tegra_dmic_modexit);

MODULE_ALIAS("platform:tegra-dmic");
MODULE_AUTHOR("Ankit Gupta <ankitgupta@nvidia.com>");
MODULE_DESCRIPTION("Tegra+DMIC ASoC Interface");
MODULE_LICENSE("GPL");
