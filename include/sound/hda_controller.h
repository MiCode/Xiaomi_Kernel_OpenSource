/*
 *  Common functionality for for HD Audio.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 */

#ifndef __SOUND_HDA_CONTROLLER_H
#define __SOUND_HDA_CONTROLLER_H

#include <linux/interrupt.h>
#include <sound/core.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/hda_bus.h>
#include <sound/hda_register.h>

/* DSP lock helpers */
#ifdef CONFIG_SND_HDA_DSP_LOADER
#define dsp_lock_init(dev)	mutex_init(&(dev)->dsp_mutex)
#define dsp_lock(dev)		mutex_lock(&(dev)->dsp_mutex)
#define dsp_unlock(dev)		mutex_unlock(&(dev)->dsp_mutex)
#define dsp_is_locked(dev)	((dev)->locked)
#else
#define dsp_lock_init(dev)	do {} while (0)
#define dsp_lock(dev)		do {} while (0)
#define dsp_unlock(dev)		do {} while (0)
#define dsp_is_locked(dev)	0
#endif

struct azx_dev {
	struct snd_dma_buffer bdl; /* BDL buffer */
	u32 *posbuf;		/* position buffer pointer */

	unsigned int bufsize;	/* size of the play buffer in bytes */
	unsigned int period_bytes; /* size of the period in bytes */
	unsigned int frags;	/* number for period in the play buffer */
	unsigned int fifo_size;	/* FIFO size */
	unsigned long start_wallclk;	/* start + minimum wallclk */
	unsigned long period_wallclk;	/* wallclk for period */

	void __iomem *sd_addr;	/* stream descriptor pointer */

	u32 sd_int_sta_mask;	/* stream int status mask */

	/* pcm support */
	struct snd_pcm_substream *substream;	/* assigned substream,
						 * set in PCM open
						 */
	unsigned int format_val;	/* format value to be set in the
					 * controller and the codec
					 */
	unsigned char stream_tag;	/* assigned stream */
	unsigned char index;		/* stream index */
	int assigned_key;		/* last device# key assigned to */

	unsigned int opened:1;
	unsigned int running:1;
	unsigned int irq_pending:1;
	unsigned int prepared:1;
	unsigned int locked:1;
	/*
	 * For VIA:
	 *  A flag to ensure DMA position is 0
	 *  when link position is not greater than FIFO size
	 */
	unsigned int insufficient:1;
	unsigned int wc_marked:1;
	unsigned int no_period_wakeup:1;

	struct timecounter  azx_tc;
	struct cyclecounter azx_cc;

	int delay_negative_threshold;

	unsigned int decoupled:1;
	void __iomem *pphc_addr;        /* processing pipe stream regs pointer */
	void __iomem *pplc_addr;        /* processing pipe stream regs pointer */
};

/* CORB/RIRB */
struct azx_rb {
	u32 *buf;		/* CORB/RIRB buffer
				 * Each CORB entry is 4byte, RIRB is 8byte
				 */
	dma_addr_t addr;	/* physical address of CORB/RIRB buffer */
	/* for RIRB */
	unsigned short rp, wp;	/* read/write pointers */
	int cmds[AZX_MAX_CODECS];	/* number of pending requests */
	u32 res[AZX_MAX_CODECS];	/* last read value */
};

struct azx;

/* Functions to read/write to hda registers. */
struct hda_controller_ops {
	/* Register Access */
	void (*reg_writel)(u32 value, u32 __iomem *addr);
	u32 (*reg_readl)(u32 __iomem *addr);
	void (*reg_writew)(u16 value, u16 __iomem *addr);
	u16 (*reg_readw)(u16 __iomem *addr);
	void (*reg_writeb)(u8 value, u8 __iomem *addr);
	u8 (*reg_readb)(u8 __iomem *addr);
	/* Disable msi if supported, PCI only */
	int (*disable_msi_reset_irq)(struct azx *);
	/* Allocation ops */
	int (*dma_alloc_pages)(struct azx *chip,
			       int type,
			       size_t size,
			       struct snd_dma_buffer *buf);
	void (*dma_free_pages)(struct azx *chip, struct snd_dma_buffer *buf);
	int (*substream_alloc_pages)(struct azx *chip,
				     struct snd_pcm_substream *substream,
				     size_t size);
	int (*substream_free_pages)(struct azx *chip,
				    struct snd_pcm_substream *substream);
	void (*pcm_mmap_prepare)(struct snd_pcm_substream *substream,
				 struct vm_area_struct *area);
	/* Check if current position is acceptable */
	int (*position_check)(struct azx *chip, struct azx_dev *azx_dev);
};

struct azx {
	struct pci_dev *pci;
	struct device *dev;
	int dev_index;

	/* chip type specific */
	int driver_type;
	unsigned int driver_caps;
	int playback_streams;
	int playback_index_offset;
	int capture_streams;
	int capture_index_offset;
	int num_streams;

	/* Register interaction. */
	const struct hda_controller_ops *ops;

	/* pci resources */
	unsigned long addr;
	void __iomem *remap_addr;
	int irq;

	/* locks */
	spinlock_t reg_lock;
	struct mutex open_mutex; /* Prevents concurrent open/close operations */
	struct completion probe_wait;

	/* streams (x num_streams) */
	struct azx_dev *azx_dev;

	/* HD codec */
	unsigned short codec_mask;
	int  codec_probe_mask; /* copied from probe_mask option */
	struct hda_bus *bus;
	unsigned int beep_mode;

	/* CORB/RIRB */
	struct azx_rb corb;
	struct azx_rb rirb;

	/* CORB/RIRB and position buffers */
	struct snd_dma_buffer rb;
	struct snd_dma_buffer posbuf;

	/* flags */
	int position_fix[2]; /* for both playback/capture streams */
	const int *bdl_pos_adj;
	int poll_count;
	unsigned int running:1;
	unsigned int initialized:1;
	unsigned int single_cmd:1;
	unsigned int polling_mode:1;
	unsigned int msi:1;
	unsigned int irq_pending_warned:1;
	unsigned int probing:1; /* codec probing phase */
	unsigned int snoop:1;
	unsigned int align_buffer_size:1;
	unsigned int region_requested:1;

	/* VGA-switcheroo setup */
	unsigned int use_vga_switcheroo:1;
	unsigned int vga_switcheroo_registered:1;
	unsigned int init_failed:1; /* delayed init failed */
	unsigned int disabled:1; /* disabled by VGA-switcher */

	/* for debugging */
	unsigned int last_cmd[AZX_MAX_CODECS];

	/* for pending irqs */
	struct work_struct irq_pending_work;

	struct work_struct probe_work;

	/* reboot notifier (for mysterious hangup problem at power-down) */
	struct notifier_block reboot_notifier;

	/* secondary power domain for hdmi audio under vga device */
	struct dev_pm_domain hdmi_pm_domain;
	unsigned int ppcap_offset;
	unsigned int spbcap_offset;
	struct azx_dev saved_azx_dev;
	/* link (x num_streams) */
	struct azx_dev *link_dev;
};

static struct snd_pcm_hardware azx_pcm_hw = {
	.info =			(SNDRV_PCM_INFO_MMAP |
				 SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID |
				 /* No full-resume yet implemented */
				 /* SNDRV_PCM_INFO_RESUME |*/
				 SNDRV_PCM_INFO_PAUSE |
				 SNDRV_PCM_INFO_SYNC_START |
				 SNDRV_PCM_INFO_HAS_WALL_CLOCK |
				 SNDRV_PCM_INFO_NO_PERIOD_WAKEUP),
	.formats =		SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE,
	.rates =		SNDRV_PCM_RATE_48000,
	.rate_min =		48000,
	.rate_max =		48000,
	.channels_min =		2,
	.channels_max =		2,
	.buffer_bytes_max =	AZX_MAX_BUF_SIZE,
	.period_bytes_min =	128,
	.period_bytes_max =	AZX_MAX_BUF_SIZE / 2,
	.periods_min =		2,
	.periods_max =		AZX_MAX_FRAG,
	.fifo_size =		0,
};


void __mark_pages_wc(struct azx *chip, struct snd_dma_buffer *dmab, bool on);

#ifdef CONFIG_X86
static inline void mark_pages_wc(struct azx *chip, struct snd_dma_buffer *buf,
				 bool on)
{
	__mark_pages_wc(chip, buf, on);
}
static inline void mark_runtime_wc(struct azx *chip, struct azx_dev *azx_dev,
				   struct snd_pcm_substream *substream, bool on)
{
	if (azx_dev->wc_marked != on) {
		__mark_pages_wc(chip, snd_pcm_get_dma_buf(substream), on);
		azx_dev->wc_marked = on;
	}
}
#else
/* NOP for other archs */
static inline void mark_pages_wc(struct azx *chip, struct snd_dma_buffer *buf,
				 bool on)
{
}
static inline void mark_runtime_wc(struct azx *chip, struct azx_dev *azx_dev,
				   struct snd_pcm_substream *substream, bool on)
{
}
#endif

/*capabilities parsing functions*/
int azx_parse_capabilities(struct azx *chip);
void azx_ppcap_enable(struct azx *chip, bool enable);
void azx_ppcap_int_enable(struct azx *chip, bool enable);
void azx_spbcap_one_enable(struct azx *chip, bool enable, int num_stream);

/* PCM setup */
static inline struct azx_dev *get_azx_dev(struct snd_pcm_substream *substream)
{
	return substream->runtime->private_data;
}

/* Stream control. */
void azx_stream_start(struct azx *chip, struct azx_dev *azx_dev);
void azx_stream_stop(struct azx *chip, struct azx_dev *azx_dev);
void azx_stream_reset(struct azx *chip, struct azx_dev *azx_dev);
int azx_setup_controller(struct azx *chip, struct azx_dev *azx_dev,
			 bool enable_lpib);
int setup_bdle(struct azx *chip, struct snd_dma_buffer *dmab,
		struct azx_dev *azx_dev, u32 **bdlp,
		int ofs, int size, int with_ioc, bool bdry_check);
/* Allocation functions. */
int azx_alloc_stream_pages(struct azx *chip);
void azx_free_stream_pages(struct azx *chip);

/* pcm helper functions */
int azx_setup_periods(struct azx *chip,
			struct snd_pcm_substream *substream,
			struct azx_dev *azx_dev);
unsigned int azx_calculate_position(struct azx *chip,
				struct azx_dev *azx_dev,
				bool with_check, int codec_delay);
int azx_get_wallclock_tstamp(struct snd_pcm_substream *substream,
				struct timespec *ts);
void azx_reset_device(struct azx *chip, struct azx_dev *azx_dev);
int azx_set_device_params(struct azx *chip, struct snd_pcm_substream *substream,
				unsigned int format_val);
void azx_set_pcm_constrains(struct azx *chip, struct snd_pcm_runtime *runtime);

/* Low level azx interface */
void azx_init_chip(struct azx *chip, int full_reset);
void azx_stop_chip(struct azx *chip);
void azx_enter_link_reset(struct azx *chip);
void azx_exit_link_reset(struct azx *chip);
irqreturn_t azx_interrupt(int irq, void *dev_id);
irqreturn_t azx_threaded_handler(int irq, void *dev_id);

/* Codec interface */
int azx_codec_configure(struct azx *chip);
int azx_mixer_create(struct azx *chip);
int azx_init_stream(struct azx *chip);
int azx_send_cmd(struct hda_bus *bus, unsigned int val);
unsigned int azx_get_response(struct hda_bus *bus, unsigned int addr);

#endif /* __SOUND_HDA_CONTROLLER_H */
