
#include <linux/delay.h>
#include <sound/hda_register.h>
#include <sound/hda_controller.h>
#include <sound/hda_dma.h>

void azx_set_dma_decouple_mode(struct azx *chip)
{
	int i = 0;

	for (i = 0; i < chip->num_streams; i++) {
		struct azx_dev *azx_dev = &chip->azx_dev[i];
		azx_dma_decouple(chip, azx_dev, true);
	}
}
EXPORT_SYMBOL_GPL(azx_set_dma_decouple_mode);

void azx_host_dma_reset(struct azx *chip, struct azx_dev *azx_dev)
{
	unsigned char val;
	int timeout;

	azx_host_dma_stop(chip, azx_dev);

	azx_sd_writeb(chip, azx_dev, SD_CTL, azx_sd_readb(chip, azx_dev, SD_CTL) |
		      SD_CTL_STREAM_RESET);
	udelay(3);
	timeout = 300;
	while (!((val = azx_sd_readb(chip, azx_dev, SD_CTL)) & SD_CTL_STREAM_RESET) &&
	       --timeout)
		;
	val &= ~SD_CTL_STREAM_RESET;
	azx_sd_writeb(chip, azx_dev, SD_CTL, val);
	udelay(3);

	timeout = 300;
	/* waiting for hardware to report that the stream is out of reset */
	while (((val = azx_sd_readb(chip, azx_dev, SD_CTL)) & SD_CTL_STREAM_RESET) &&
	       --timeout)
		;

	if (!timeout)
		dev_err(chip->dev, "Resetting stream FAILED");

	/* reset first position - may not be synced with hw at this time */
	*azx_dev->posbuf = 0;
}


void azx_host_dma_stop(struct azx *chip, struct azx_dev *azx_dev)
{
	azx_sd_writeb(chip, azx_dev, SD_CTL, azx_sd_readb(chip, azx_dev, SD_CTL) &
		      ~(SD_CTL_DMA_START | SD_INT_MASK));
	azx_sd_writeb(chip, azx_dev, SD_STS, SD_INT_MASK); /* to be sure */
}

void azx_host_dma_start(struct azx *chip, struct azx_dev *azx_dev)
{

	/* enable SIE */
	azx_writel(chip, INTCTL,
		   azx_readl(chip, INTCTL) | (1 << azx_dev->index));
	/* set DMA start and interrupt mask */
	azx_sd_writeb(chip, azx_dev, SD_CTL, azx_sd_readb(chip, azx_dev, SD_CTL) |
		      SD_CTL_DMA_START | SD_INT_MASK);
}

/* assign a stream for the PCM */
static inline struct azx_dev *
azx_assign_device(struct azx *chip, bool is_playback)
{
	int i, nums;
	struct azx_dev *azx_dev;

	if (is_playback) {
		nums = chip->playback_streams;
		azx_dev = &chip->azx_dev[chip->playback_index_offset + nums - 1];
	} else {
		nums = chip->capture_streams;
		azx_dev = &chip->azx_dev[chip->capture_index_offset + nums - 1];
	}

	for (i = 0; i < nums; i++, azx_dev--) {
		dsp_lock(azx_dev);
		if (!azx_dev->opened && !dsp_is_locked(azx_dev)) {
			azx_dev->opened = 1;
			dsp_unlock(azx_dev);
			return azx_dev;
		}
		dsp_unlock(azx_dev);
	}

	return NULL;
}

/* release the assigned stream */
static inline void azx_release_device(struct azx_dev *azx_dev)
{
	azx_dev->opened = 0;
}

static struct azx_dev *get_azx_dev_by_dma(struct azx *chip,
	struct dma_desc *dma)
{
	struct azx_dev *azx_dev;
	int index;

	index = dma->playback ?
	chip->playback_index_offset :
	chip->capture_index_offset;

	index += dma->id;

	if (index >= chip->num_streams) {
		dev_err(chip->dev, "requested DMA index: 0x%x is out of range", index);
		return NULL;
	}
	azx_dev = &chip->azx_dev[index];
	return azx_dev;
}

/* Get DMA id seperate 0-based numeration
 * for playback and capture streams
 */
static int azx_get_dma_id(struct azx *chip, struct azx_dev *azx_dev)
{
	return azx_dev->index >= chip->playback_index_offset ?
		azx_dev->index - chip->playback_index_offset :
		azx_dev->index - chip->capture_index_offset;
}

int azx_alloc_decoupled_dma(void *ctx, struct dma_desc *dma)
{
	int ret = 0;
	struct azx *chip = (struct azx *) ctx;
	struct azx_dev *azx_dev;

	azx_dev = azx_assign_device(chip, dma->playback);

	if (NULL == azx_dev) {
		dev_err(chip->dev, "Failed to allocate the requested DMA");
		return -EIO;
	}

	spin_lock_irq(&chip->reg_lock);

	azx_host_dma_reset(chip, azx_dev);

	ret = azx_dma_decouple(chip, azx_dev, true);
	if (ret) {
		dev_err(chip->dev, "Failed to decouple the requested DMA");
		spin_unlock_irq(&chip->reg_lock);
		azx_release_device(azx_dev);
	}

	spin_unlock_irq(&chip->reg_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(azx_alloc_decoupled_dma);

int azx_release_decoupled_dma(void *ctx, struct dma_desc *dma)
{
	int ret = 0;
	struct azx *chip = (struct azx *) ctx;
	struct azx_dev *azx_dev;

	azx_dev = get_azx_dev_by_dma(chip, dma);
	if (NULL == azx_dev) {
		dev_err(chip->dev, "The requested DMA does not exist");
		return -EIO;
	}

	spin_lock_irq(&chip->reg_lock);

	if (!azx_dev->decoupled) {
		dev_err(chip->dev, "Cannot release a non-allocated DMA");
		spin_unlock_irq(&chip->reg_lock);
		return -EINVAL;
	}

	azx_link_dma_reset(chip, azx_dev);

	azx_host_dma_reset(chip, azx_dev);

	ret = azx_dma_decouple(chip, azx_dev, false);
	if (ret) {
		dev_err(chip->dev, "Failed to couple the requested DMA");
		spin_unlock_irq(&chip->reg_lock);
		return ret;
	}

	azx_release_device(azx_dev);

	spin_unlock_irq(&chip->reg_lock);

	return ret;

}
EXPORT_SYMBOL_GPL(azx_release_decoupled_dma);

int azx_dma_decouple(struct azx *chip, struct azx_dev *azx_dev,	bool decouple)
{
	dev_dbg(chip->dev, "azx_dev is already %s", decouple ? "decoupled" : "coupled");

	azx_writel_andor(
		chip,
		chip->ppcap_offset + ICH6_REG_PP_PPCTL,
		~PPCTL_PROCEN(azx_dev->index),
		decouple ? PPCTL_PROCEN(azx_dev->index) : 0);

	azx_dev->decoupled = decouple;

	return 0;
}
EXPORT_SYMBOL_GPL(azx_dma_decouple);

int azx_link_dma_reset(struct azx *chip, struct azx_dev *azx_dev)
{
	unsigned int reg;
	unsigned int cnt;

	/* Run bit needs to be cleared before touching reset */
	azx_pplc_writel_andor(chip, azx_dev, REG_PPLCCTL, ~PPLCCTL_RUN, 0);

	/* Assert reset */
	azx_pplc_writel_andor(chip, azx_dev, REG_PPLCCTL, ~PPLCCTL_STRST,
PPLCCTL_STRST);
	udelay(5);
	for (cnt = LINK_STRST_TIMEOUT; cnt > 0; cnt--) {
		if (azx_pplc_readl(chip, azx_dev, REG_PPLCCTL) & PPLCCTL_STRST)
			break;
		msleep(1);
	}

	if (!cnt) {
		/* TODO: consider recovery like controller reset */
		dev_err(chip->dev, "Link DMA id: %d reset entry failed.",
								azx_dev->index);
		return -EIO;
	}

	/* Deassert reset */
	azx_pplc_writel_andor(chip, azx_dev, REG_PPLCCTL, ~PPLCCTL_STRST, 0);
	udelay(5);
	for (cnt = LINK_STRST_TIMEOUT; cnt > 0; cnt--) {
		if (!(azx_pplc_readl(chip, azx_dev, REG_PPLCCTL) & PPLCCTL_STRST))
			break;
		msleep(1);
	}

	if (!cnt) {
		/* TODO: consider recovery like controller reset */
		dev_err(chip->dev, "Link DMA id: %d reset exit failed.", azx_dev->index);
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(azx_link_dma_reset);

int azx_link_dma_run_ctrl(struct azx *chip, struct azx_dev *azx_dev,
		 bool run)
{
	unsigned int cnt;

	/* Shall we call setup_controller instead */

	azx_pplc_writel_andor(chip, azx_dev, REG_PPLCCTL, ~PPLCCTL_RUN, run ? PPLCCTL_RUN : 0);

	/* Need to check HW state in case of stopping */
	if (!run) {
		udelay(5);
		for (cnt = LINK_STOP_TIMEOUT; cnt > 0; cnt--) {
			if (!(azx_pplc_readl(chip, azx_dev, REG_PPLCCTL) & PPLCCTL_RUN))
				break;
			msleep(1);
		}
		if (!cnt) {
			/* TODO: consider recovery like controller reset */
			dev_err(chip->dev, "Link DMA id: %d failed to stop", azx_dev->index);
			return -EIO;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(azx_link_dma_run_ctrl);

int azx_link_dma_set_stream_id(struct azx *chip, struct azx_dev *azx_dev)
{

	if (azx_pplc_readl(chip, azx_dev, REG_PPLCCTL) & PPLCCTL_RUN) {
		dev_err(chip->dev,
		 "Failed to change stream id while the DMA is running\n");
		return -EBUSY;
	}

	azx_pplc_writel_andor(chip, azx_dev, REG_PPLCCTL, ~PPLCCTL_STRM_MASK,
		(azx_dev->stream_tag << PPLCCTL_STRM_SHIFT) &
			PPLCCTL_STRM_MASK);

	return 0;
}
EXPORT_SYMBOL_GPL(azx_link_dma_set_stream_id);

int azx_link_dma_set_format(struct azx *chip,
	struct azx_dev *azx_dev, int fmt)
{

	if (azx_pplc_readl(chip, azx_dev, REG_PPLCCTL) & PPLCCTL_RUN) {
		dev_err(chip->dev, "The requested DMA is running");
		return -EBUSY;
	}

	/* TODO: investigate if fmt fields needed as args */
	azx_pplc_writew_andor(chip, azx_dev, REG_PPLCFMT, ~0, fmt);

	return 0;
}
EXPORT_SYMBOL_GPL(azx_link_dma_set_format);

int azx_host_dma_reset_locked(struct azx *chip, struct dma_desc *dma)
{
	struct azx_dev *azx_dev = get_azx_dev_by_dma(chip, dma);

	if (NULL == azx_dev) {
		dev_err(chip->dev, "The requested DMA does not exist");
		return -EIO;
	}

	spin_lock_irq(&chip->reg_lock);

	azx_host_dma_reset(chip, azx_dev);

	spin_unlock_irq(&chip->reg_lock);

	return 0;
}


int azx_host_dma_run_ctrl_locked(struct azx *chip, struct dma_desc *dma, bool run)
{
	struct azx_dev *azx_dev = get_azx_dev_by_dma(chip, dma);

	if (NULL == azx_dev) {
		dev_dbg(chip->dev, "The requested DMA does not exist");
		return -EIO;
	}

	spin_lock_irq(&chip->reg_lock);

	if (run) {
		azx_dev->format_val = dma->fmt;

		azx_setup_controller(chip, azx_dev, 1);

		if (dma->playback)
			azx_dev->fifo_size = azx_sd_readw(chip, azx_dev, SD_FIFOSIZE) + 1;
		else
			azx_dev->fifo_size = 0;

		dev_dbg(chip->dev, "FIFO size: 0x%x", azx_dev->fifo_size);

		azx_host_dma_start(chip, azx_dev);
	} else
		azx_host_dma_stop(chip, azx_dev);

	azx_dev->running = !!run;

	spin_unlock_irq(&chip->reg_lock);

	return 0;
}

void azx_host_dma_set_stream_id(struct azx *chip, struct azx_dev *azx_dev)
{
	u32 val = azx_sd_readl(chip, azx_dev, SD_CTL);
	val = (val & ~SD_CTL_STREAM_TAG_MASK) |
		(azx_dev->stream_tag << SD_CTL_STREAM_TAG_SHIFT);
	if (!azx_snoop(chip))
		val |= SD_CTL_TRAFFIC_PRIO;
	azx_sd_writel(chip, azx_dev, SD_CTL, val);

}

void azx_host_dma_set_format(struct azx *chip, struct azx_dev *azx_dev, int fmt)
{
	azx_dev->format_val = fmt;
	azx_sd_writew(chip, azx_dev, SD_FORMAT, azx_dev->format_val);
}

