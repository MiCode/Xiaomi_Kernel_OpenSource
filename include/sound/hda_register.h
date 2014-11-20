/*
 *  defines for HD Audio controller register.
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

#ifndef __SOUND_HDA_REGISTER_H
#define __SOUND_HDA_REGISTER_H

/*
 * registers
 */
#define ICH6_REG_GCAP			0x00
#define   ICH6_GCAP_64OK	(1 << 0)   /* 64bit address support */
#define   ICH6_GCAP_NSDO	(3 << 1)   /* # of serial data out signals */
#define   ICH6_GCAP_BSS		(31 << 3)  /* # of bidirectional streams */
#define   ICH6_GCAP_ISS		(15 << 8)  /* # of input streams */
#define   ICH6_GCAP_OSS		(15 << 12) /* # of output streams */
#define ICH6_REG_VMIN			0x02
#define ICH6_REG_VMAJ			0x03
#define ICH6_REG_OUTPAY			0x04
#define ICH6_REG_INPAY			0x06
#define ICH6_REG_GCTL			0x08
#define   ICH6_GCTL_RESET	(1 << 0)   /* controller reset */
#define   ICH6_GCTL_FCNTRL	(1 << 1)   /* flush control */
#define   ICH6_GCTL_UNSOL	(1 << 8)   /* accept unsol. response enable */
#define ICH6_REG_WAKEEN			0x0c
#define ICH6_REG_STATESTS		0x0e
#define ICH6_REG_GSTS			0x10
#define   ICH6_GSTS_FSTS	(1 << 1)   /* flush status */
#define ICH6_REG_GCAP2			0x12
#define ICH6_REG_LLCH			0x14
#define HDA_OUTSTRMPAY			0x18
#define HDA_INSTRMPAY			0x1A
#define ICH6_REG_INTCTL			0x20
#define ICH6_REG_INTSTS			0x24
#define ICH6_REG_WALLCLK		0x30	/* 24Mhz source */
#define ICH6_REG_OLD_SSYNC		0x34	/* SSYNC for old ICH */
#define ICH6_REG_SSYNC			0x38
#define ICH6_REG_CORBLBASE		0x40
#define ICH6_REG_CORBUBASE		0x44
#define ICH6_REG_CORBWP			0x48
#define ICH6_REG_CORBRP			0x4a
#define   ICH6_CORBRP_RST	(1 << 15)  /* read pointer reset */
#define ICH6_REG_CORBCTL		0x4c
#define   ICH6_CORBCTL_RUN	(1 << 1)   /* enable DMA */
#define   ICH6_CORBCTL_CMEIE	(1 << 0)   /* enable memory error irq */
#define ICH6_REG_CORBSTS		0x4d
#define   ICH6_CORBSTS_CMEI	(1 << 0)   /* memory error indication */
#define ICH6_REG_CORBSIZE		0x4e

#define ICH6_REG_RIRBLBASE		0x50
#define ICH6_REG_RIRBUBASE		0x54
#define ICH6_REG_RIRBWP			0x58
#define   ICH6_RIRBWP_RST	(1 << 15)  /* write pointer reset */
#define ICH6_REG_RINTCNT		0x5a
#define ICH6_REG_RIRBCTL		0x5c
#define   ICH6_RBCTL_IRQ_EN	(1 << 0)   /* enable IRQ */
#define   ICH6_RBCTL_DMA_EN	(1 << 1)   /* enable DMA */
#define   ICH6_RBCTL_OVERRUN_EN	(1 << 2)   /* enable overrun irq */
#define ICH6_REG_RIRBSTS		0x5d
#define   ICH6_RBSTS_IRQ	(1 << 0)   /* response irq */
#define   ICH6_RBSTS_OVERRUN	(1 << 2)   /* overrun irq */
#define ICH6_REG_RIRBSIZE		0x5e

#define ICH6_REG_IC			0x60
#define ICH6_REG_IR			0x64
#define ICH6_REG_IRS			0x68
#define   ICH6_IRS_VALID	(1<<1)
#define   ICH6_IRS_BUSY		(1<<0)

#define ICH6_REG_DPLBASE		0x70
#define ICH6_REG_DPUBASE		0x74
#define   ICH6_DPLBASE_ENABLE	0x1	/* Enable position buffer */

/* SD offset: SDI0=0x80, SDI1=0xa0, ... SDO3=0x160 */
enum { SDI0, SDI1, SDI2, SDI3, SDO0, SDO1, SDO2, SDO3 };

/* stream register offsets from stream base */
#define ICH6_REG_SD_CTL			0x00
#define ICH6_REG_SD_STS			0x03
#define ICH6_REG_SD_LPIB		0x04
#define ICH6_REG_SD_CBL			0x08
#define ICH6_REG_SD_LVI			0x0c
#define ICH6_REG_SD_FIFOW		0x0e
#define ICH6_REG_SD_FIFOSIZE		0x10
#define ICH6_REG_SD_FORMAT		0x12
#define ICH6_REG_SD_FIFOL		0x14
#define ICH6_REG_SD_BDLPL		0x18
#define ICH6_REG_SD_BDLPU		0x1c

/* PCI space */
#define ICH6_PCIREG_TCSEL	0x44

/*
 * other constants
 */

/* max number of SDs */
/* ICH, ATI and VIA have 4 playback and 4 capture */
#define ICH6_NUM_CAPTURE	4
#define ICH6_NUM_PLAYBACK	4

/* ULI has 6 playback and 5 capture */
#define ULI_NUM_CAPTURE		5
#define ULI_NUM_PLAYBACK	6

/* ATI HDMI may have up to 8 playbacks and 0 capture */
#define ATIHDMI_NUM_CAPTURE	0
#define ATIHDMI_NUM_PLAYBACK	8

/* TERA has 4 playback and 3 capture */
#define TERA_NUM_CAPTURE	3
#define TERA_NUM_PLAYBACK	4

/* this number is statically defined for simplicity */
#define MAX_AZX_DEV		16

/* max number of fragments - we may use more if allocating more pages for BDL */
#define BDL_SIZE		4096
#define AZX_MAX_BDL_ENTRIES	(BDL_SIZE / 16)
#define AZX_MAX_FRAG		32
/* max buffer size - no h/w limit, you can increase as you like */
#define AZX_MAX_BUF_SIZE	(1024*1024*1024)

/* RIRB int mask: overrun[2], response[0] */
#define RIRB_INT_RESPONSE	0x01
#define RIRB_INT_OVERRUN	0x04
#define RIRB_INT_MASK		0x05

/* STATESTS int mask: S3,SD2,SD1,SD0 */
#define AZX_MAX_CODECS		8
#define AZX_DEFAULT_CODECS	4
#define STATESTS_INT_MASK	((1 << AZX_MAX_CODECS) - 1)

/* SD_CTL bits */
#define SD_CTL_STREAM_RESET	0x01	/* stream reset bit */
#define SD_CTL_DMA_START	0x02	/* stream DMA start bit */
#define SD_CTL_STRIPE		(3 << 16)	/* stripe control */
#define SD_CTL_TRAFFIC_PRIO	(1 << 18)	/* traffic priority */
#define SD_CTL_DIR		(1 << 19)	/* bi-directional stream */
#define SD_CTL_STREAM_TAG_MASK	(0xf << 20)
#define SD_CTL_STREAM_TAG_SHIFT	20

/* SD_CTL and SD_STS */
#define SD_INT_DESC_ERR		0x10	/* descriptor error interrupt */
#define SD_INT_FIFO_ERR		0x08	/* FIFO error interrupt */
#define SD_INT_COMPLETE		0x04	/* completion interrupt */
#define SD_INT_MASK		(SD_INT_DESC_ERR|SD_INT_FIFO_ERR|\
				 SD_INT_COMPLETE)

/* SD_STS */
#define SD_STS_FIFO_READY	0x20	/* FIFO ready */

/* INTCTL and INTSTS */
#define ICH6_INT_ALL_STREAM	0xff	   /* all stream interrupts */
#define ICH6_INT_CTRL_EN	0x40000000 /* controller interrupt enable bit */
#define ICH6_INT_GLOBAL_EN	0x80000000 /* global interrupt enable bit */

/* below are so far hardcoded - should read registers in future */
#define ICH6_MAX_CORB_ENTRIES	256
#define ICH6_MAX_RIRB_ENTRIES	256

/* driver quirks (capabilities) */
/* bits 0-7 are used for indicating driver type */
#define AZX_DCAPS_NO_TCSEL	(1 << 8)	/* No Intel TCSEL bit */
#define AZX_DCAPS_NO_MSI	(1 << 9)	/* No MSI support */
#define AZX_DCAPS_ATI_SNOOP	(1 << 10)	/* ATI snoop enable */
#define AZX_DCAPS_NVIDIA_SNOOP	(1 << 11)	/* Nvidia snoop enable */
#define AZX_DCAPS_SCH_SNOOP	(1 << 12)	/* SCH/PCH snoop enable */
#define AZX_DCAPS_RIRB_DELAY	(1 << 13)	/* Long delay in read loop */
#define AZX_DCAPS_RIRB_PRE_DELAY (1 << 14)	/* Put a delay before read */
#define AZX_DCAPS_CTX_WORKAROUND (1 << 15)	/* X-Fi workaround */
#define AZX_DCAPS_POSFIX_LPIB	(1 << 16)	/* Use LPIB as default */
#define AZX_DCAPS_POSFIX_VIA	(1 << 17)	/* Use VIACOMBO as default */
#define AZX_DCAPS_NO_64BIT	(1 << 18)	/* No 64bit address */
#define AZX_DCAPS_SYNC_WRITE	(1 << 19)	/* sync each cmd write */
#define AZX_DCAPS_OLD_SSYNC	(1 << 20)	/* Old SSYNC reg for ICH */
#define AZX_DCAPS_BUFSIZE	(1 << 21)	/* no buffer size alignment */
#define AZX_DCAPS_ALIGN_BUFSIZE	(1 << 22)	/* buffer size alignment */
#define AZX_DCAPS_4K_BDLE_BOUNDARY (1 << 23)	/* BDLE in 4k boundary */
#define AZX_DCAPS_COUNT_LPIB_DELAY  (1 << 25)	/* Take LPIB as delay */
#define AZX_DCAPS_PM_RUNTIME	(1 << 26)	/* runtime PM support */
#define AZX_DCAPS_I915_POWERWELL (1 << 27)	/* HSW i915 powerwell support */
#define AZX_DCAPS_CORBRP_SELF_CLEAR (1 << 28)	/* CORBRP clears itself after reset */

/* position fix mode */
enum {
	POS_FIX_AUTO,
	POS_FIX_LPIB,
	POS_FIX_POSBUF,
	POS_FIX_VIACOMBO,
	POS_FIX_COMBO,
};

/* Defines for ATI HD Audio support in SB450 south bridge */
#define ATI_SB450_HDAUDIO_MISC_CNTR2_ADDR   0x42
#define ATI_SB450_HDAUDIO_ENABLE_SNOOP      0x02

/* Defines for Nvidia HDA support */
#define NVIDIA_HDA_TRANSREG_ADDR      0x4e
#define NVIDIA_HDA_ENABLE_COHBITS     0x0f
#define NVIDIA_HDA_ISTRM_COH          0x4d
#define NVIDIA_HDA_OSTRM_COH          0x4c
#define NVIDIA_HDA_ENABLE_COHBIT      0x01

/* Defines for Intel SCH HDA snoop control */
#define INTEL_SCH_HDA_DEVC      0x78
#define INTEL_SCH_HDA_DEVC_NOSNOOP       (0x1<<11)

/* Define IN stream 0 FIFO size offset in VIA controller */
#define VIA_IN_STREAM0_FIFO_SIZE_OFFSET	0x90
/* Define VIA HD Audio Device ID*/
#define VIA_HDAC_DEVICE_ID		0x3288

/* HD Audio class code */
#define PCI_CLASS_MULTIMEDIA_HD_AUDIO	0x0403

#ifdef CONFIG_SND_VERBOSE_PRINTK
#define SFX	/* nop */
#else
#define SFX	"hda-intel "
#endif

#ifdef CONFIG_X86
#define azx_snoop(chip)		((chip)->snoop)
#else
#define azx_snoop(chip)		true
#endif


/* Some temporary defines to turn on/off functionalilies for test purposes */

/* Timeouts */
#define LINK_STRST_TIMEOUT 50
#define LINK_STOP_TIMEOUT 50

#define REG_MASK(bit_num, offset) \
	(((1 << (bit_num)) - 1) << (offset))

#define HDA_RECUR_REG_OFFSET(_BASE_, _INTERVAL_, _X_, _OFFSET_) \
	((_BASE_) + ((_INTERVAL_) * (_X_)) + (_OFFSET_))

/* Intel® HD Audio HW specific definitions and parameters*/

#define HDA_PPLCB(_HSTISC_, _HSTOSC_) \
	(0x10 + 0x10 * ((_HSTISC_) + (_HSTOSC_)))

/* SPT specific definitions */

/* TODO: this parameter should be taken from HW */
#define HDA_SPT_HSTISC			0x7
/* TODO: this parameter should be taken from HW */
#define HDA_SPT_HSTOSC			0x9

#define HDA_SPT_PPLCB	HDA_PPLCB(HDA_SPT_HSTISC, HDA_SPT_HSTOSC)

#define ICH6_REG_CAP_HDR		0x0
#define CAP_HDR_VER_OFF			28
#define CAP_HDR_VER_MASK		(0xF << CAP_HDR_VER_OFF)
#define CAP_HDR_ID_OFF			16
#define CAP_HDR_ID_MASK			(0xFFF << CAP_HDR_ID_OFF)
#define CAP_HDR_NXT_PTR_MASK	0xFFFF

/*	registers of Intel® HD Audio Software Position Based FIFO
*	Capability Structure
*/
#define SPB_CAP_ID					0x4
#define ICH6_REG_SPB_BASE_ADDR		0x700
#define ICH6_REG_SPB_SPBFCH			0x00
#define ICH6_REG_SPB_SPBFCCTL		0x04
/* Base used to calculate the iterating register offset */
#define ICH6_REG_SPB_XBASE			0x08
/* Interval used to calculate the iterating register offset */
#define ICH6_REG_SPB_XINTERVAL		0x08

#define ICH6_REG_SPB_RECUR(_X_, _OFFSET_) \
	HDA_RECUR_REG_OFFSET( \
		ICH6_REG_SPB_XBASE, \
		ICH6_REG_SPB_XINTERVAL, \
		(_X_), \
		(_OFFSET_) \
		)

#define ICH6_REG_SPB_SDXSPIB(_X_)	ICH6_REG_SPB_RECUR((_X_), 0x00)
#define ICH6_REG_SPB_SDXMAXFIFOS(_X_)	ICH6_REG_SPB_RECUR((_X_), 0x04)

/*	registers of Intel® HD Audio Global Time Synchronization
*	Capability Structure
*/
#define GTS_CAP_ID					0x1
#define ICH6_REG_GTS_GTSCH			0x00
#define ICH6_REG_GTS_GTSCD			0x04
#define ICH6_REG_GTS_GTSCTLAC		0x0C
#define ICH6_REG_GTS_XBASE			0x20
#define ICH6_REG_GTS_XINTERVAL		0x20

#define ICH6_REG_GTS_RECUR(_X_, _OFFSET_) \
	HDA_RECUR_REG_OFFSET( \
		ICH6_REG_GTS_XBASE, \
		ICH6_REG_GTS_XINTERVAL, \
		(_X_), \
		(_OFFSET_) \
		)

#define ICH6_REG_GTS_GTSCC(_X_)		ICH6_REG_GTS_RECUR((_X_), 0x00)
#define ICH6_REG_GTS_WALFCC(_X_)	ICH6_REG_GTS_RECUR((_X_), 0x04)
#define ICH6_REG_GTS_TSCCL(_X_)		ICH6_REG_GTS_RECUR((_X_), 0x08)
#define ICH6_REG_GTS_TSCCU(_X_)		ICH6_REG_GTS_RECUR((_X_), 0x0C)
#define ICH6_REG_GTS_LLPFOC(_X_)	ICH6_REG_GTS_RECUR((_X_), 0x14)
#define ICH6_REG_GTS_LLPCL(_X_)		ICH6_REG_GTS_RECUR((_X_), 0x18)
#define ICH6_REG_GTS_LLPCU(_X_)		ICH6_REG_GTS_RECUR((_X_), 0x1C)

/* registers in Intel® HD Audio Processing Pipe Capability Structure */
#define PP_CAP_ID					0x3
#define ICH6_REG_PP_PPCH			0x10

#define ICH6_REG_PP_PPCTL			0x04
#define PPCTL_PIE					(1<<31)
#define PPCTL_GPROCEN				(1<<30)
/* _X_ = dma engine # and cannot
 * exceed 29 (per spec max 30 dma engines)
 */
#define PPCTL_PROCEN(_X_)				(1<<(_X_))

#define ICH6_REG_PP_PPSTS			0x08


#define ICH6_REG_PP_XBASE			0x10
#define ICH6_REG_PP_XINTERVAL		0x10

#define ICH6_REG_PP_RECUR(_X_, _OFFSET_) \
	HDA_RECUR_REG_OFFSET( \
		ICH6_REG_PP_XBASE, \
		ICH6_REG_PP_XINTERVAL, \
		(_X_), \
		(_OFFSET_) \
		)

#define PPHC_BASE			0x10
#define PPHC_INTERVAL		0x10

#define REG_PPHCLLPL			0x0
#define REG_PPHCLLPU			0x4
#define REG_PPHCLDPL			0x8
#define REG_PPHCLDPU			0xC

#define PPLC_BASE			0x10
#define PPLC_MULTI			0x10
#define PPLC_INTERVAL		0x10

#define REG_PPLCCTL			0x0
#define PPLCCTL_STRM_BITS	4
#define PPLCCTL_STRM_SHIFT	20
#define PPLCCTL_STRM_MASK \
	REG_MASK(PPLCCTL_STRM_BITS, PPLCCTL_STRM_SHIFT)
#define PPLCCTL_RUN		(1<<1)
#define PPLCCTL_STRST	(1<<0)

#define REG_PPLCFMT			0x4
#define REG_PPLCLLPL		0x8
#define REG_PPLCLLPU		0xC

#define ICH6_REG_PP_PPHCXLLPL(_X_) \
	ICH6_REG_PP_RECUR((_X_), 0x00)
#define ICH6_REG_PP_PPHCXLLPU(_X_) \
	ICH6_REG_PP_RECUR((_X_), 0x04)
#define ICH6_REG_PP_PPHCXLDPL(_X_) \
	ICH6_REG_PP_RECUR((_X_), 0x08)
#define ICH6_REG_PP_PPHCXLDPU(_X_) \
	ICH6_REG_PP_RECUR((_X_), 0x0C)
#define ICH6_REG_PP_PPLCXCTL(_X_) \
	ICH6_REG_PP_RECUR((_X_), HDA_SPT_PPLCB + 0x00)
#define ICH6_REG_PP_PPLCXFMT(_X_) \
	ICH6_REG_PP_RECUR((_X_), HDA_SPT_PPLCB + 0x04)
#define ICH6_REG_PP_PPLCXLLPL(_X_) \
	ICH6_REG_PP_RECUR((_X_), HDA_SPT_PPLCB + 0x08)
#define ICH6_REG_PP_PPLCXLLPU(_X_) \
	ICH6_REG_PP_RECUR((_X_), HDA_SPT_PPLCB + 0x0C)

/* registers in Intel® HD Audio Multiple Links Capability Structure */
#define ML_CAP_ID					0x2
#define ICH6_REG_ML_MLCH			0x00
#define ICH6_REG_ML_MLCD			0x04
#define ICH6_REG_ML_XBASE			0x40
#define ICH6_REG_ML_XINTERVAL		0x40

#define ICH6_REG_ML_RECUR(_X_, _OFFSET_) \
	HDA_RECUR_REG_OFFSET( \
		HDA_ML_XBASE, \
		HDA_ML_XINTERVAL, \
		(_X_), \
		(_OFFSET_) \
		)

#define ICH6_REG_ML_LCAPX(_X_)		ICH6_REG_ML_RECUR((_X_), 0x00)
#define ICH6_REG_ML_LCTLX(_X_)		ICH6_REG_ML_RECUR((_X_), 0x04)
#define ICH6_REG_ML_LOSIDVX(_X_)	ICH6_REG_ML_RECUR((_X_), 0x08)
#define ICH6_REG_ML_LSDIIDX(_X_)	ICH6_REG_ML_RECUR((_X_), 0x0C)
#define ICH6_REG_ML_LPSOOX(_X_)		ICH6_REG_ML_RECUR((_X_), 0x10)
#define ICH6_REG_ML_LPSIOX(_X_)		ICH6_REG_ML_RECUR((_X_), 0x12)
#define ICH6_REG_ML_LWALFC(_X_)		ICH6_REG_ML_RECUR((_X_), 0x18)
#define ICH6_REG_ML_LOUTPAYX(_X_)	ICH6_REG_ML_RECUR((_X_), 0x20)
#define ICH6_REG_ML_LINPAYX(_X_)	ICH6_REG_ML_RECUR((_X_), 0x30)

/* Intel® HD Audio Vendor Specific Registers */
#define ICH6_REG_VS_EM1					0x1000
#define ICH6_REG_VS_INRC				0x1004
#define ICH6_REG_VS_OUTRC				0x1008
#define ICH6_REG_VS_FIFOTRK				0x100C
#define ICH6_REG_VS_FIFOTRK2			0x1010
#define ICH6_REG_VS_EM2					0x1030
#define ICH6_REG_VS_EM3L				0x1038
#define ICH6_REG_VS_EM3U				0x103C
#define ICH6_REG_VS_EM4L				0x1040
#define ICH6_REG_VS_EM4U				0x1044
#define ICH6_REG_VS_LTRC				0x1048
#define ICH6_REG_VS_D0I3C				0x104A
#define ICH6_REG_VS_PCE					0x104B
#define ICH6_REG_VS_L2MAGC				0x1050
#define ICH6_REG_VS_L2LAHPT				0x1054
#define ICH6_REG_VS_SDXDPIB_XBASE		0x1084
#define ICH6_REG_VS_SDXDPIB_XINTERVAL	0x20
#define ICH6_REG_VS_SDXDPIB
#define ICH6_REG_VS_SDXEFIFOS_XBASE		0x1094
#define ICH6_REG_VS_SDXEFIFOS_XINTERVAL	0x20
#define ICH6_REG_VS_SDXEFIFOS

/* Intel® HD Audio Alias Registers */
#define ICH6_REG_ALIAS_WLCLKA			0x2030
#define ICH6_REG_ALIAS_SDXLPIBA_XBASE		0x2084
#define ICH6_REG_ALIAS_SDXLPIBA_XINTERVAL	0x20
#define ICH6_REG_ALIAS_SDXLPIBA

/*
 * macros for easy use
 */
#define azx_writel_andor(chip, reg, mask_and, mask_or) \
	azx_writel_alt( \
		(chip), \
		(reg), \
		(azx_readl_alt((chip), (reg)) & (mask_and)) | (mask_or))

#define azx_writel(chip, reg, value) \
	((chip)->ops->reg_writel(value, (chip)->remap_addr + ICH6_REG_##reg))
#define azx_writel_alt(chip, reg, value) \
	((chip)->ops->reg_writel(value, (chip)->remap_addr + reg))
#define azx_readl(chip, reg) \
	((chip)->ops->reg_readl((chip)->remap_addr + ICH6_REG_##reg))
#define azx_readl_alt(chip, reg) \
	((chip)->ops->reg_readl((chip)->remap_addr + reg))
#define azx_writew(chip, reg, value) \
	((chip)->ops->reg_writew(value, (chip)->remap_addr + ICH6_REG_##reg))
#define azx_readw(chip, reg) \
	((chip)->ops->reg_readw((chip)->remap_addr + ICH6_REG_##reg))
#define azx_writeb(chip, reg, value) \
	((chip)->ops->reg_writeb(value, (chip)->remap_addr + ICH6_REG_##reg))
#define azx_readb(chip, reg) \
	((chip)->ops->reg_readb((chip)->remap_addr + ICH6_REG_##reg))

#define azx_sd_writel(chip, dev, reg, value) \
	((chip)->ops->reg_writel(value, (dev)->sd_addr + ICH6_REG_##reg))
#define azx_sd_readl(chip, dev, reg) \
	((chip)->ops->reg_readl((dev)->sd_addr + ICH6_REG_##reg))
#define azx_sd_writew(chip, dev, reg, value) \
	((chip)->ops->reg_writew(value, (dev)->sd_addr + ICH6_REG_##reg))
#define azx_sd_readw(chip, dev, reg) \
	((chip)->ops->reg_readw((dev)->sd_addr + ICH6_REG_##reg))
#define azx_sd_writeb(chip, dev, reg, value) \
	((chip)->ops->reg_writeb(value, (dev)->sd_addr + ICH6_REG_##reg))
#define azx_sd_readb(chip, dev, reg) \
	((chip)->ops->reg_readb((dev)->sd_addr + ICH6_REG_##reg))

#define azx_pphc_writel(chip, dev, reg, value) \
	((chip)->ops->reg_writel(value, (dev)->pphc_addr + (reg)))
#define azx_pphc_readl(chip, dev, reg) \
	((chip)->ops->reg_readl((dev)->pphc_addr + (reg)))
#define azx_pphc_writew(chip, dev, reg, value) \
	((chip)->ops->reg_writew(value, (dev)->pphc_addr + (reg)))
#define azx_pphc_readw(chip, dev, reg) \
	((chip)->ops->reg_readw((dev)->pphc_addr, (reg)))
#define azx_pphc_writeb(chip, dev, reg, value) \
	((chip)->ops->reg_writeb(value, (dev)->pphc_addr + (reg)))
#define azx_pphc_readb(chip, dev, reg) \
	((chip)->ops->reg_readb((dev)->pphc_addr, (reg)))

#define azx_pplc_writel(chip, dev, reg, value) \
	((chip)->ops->reg_writel(value, (dev)->pplc_addr + (reg)))
#define azx_pplc_readl(chip, dev, reg) \
	((chip)->ops->reg_readl((dev)->pplc_addr + (reg)))
#define azx_pplc_writew(chip, dev, reg, value) \
	((chip)->ops->reg_writew(value, (dev)->pplc_addr + (reg)))
#define azx_pplc_readw(chip, dev, reg) \
	((chip)->ops->reg_readw((dev)->pplc_addr + (reg)))
#define azx_pplc_writeb(chip, dev, reg, value) \
	((chip)->ops->reg_writeb(value, (dev)->pplc_addr + (reg)))
#define azx_pplc_readb(chip, dev, reg) \
	((chip)->ops->reg_readb((dev)->pplc_addr + (reg)))

#define azx_pphc_writel_andor(chip, dev, reg, mask_and, mask_or) \
	((chip)->ops->reg_writel( \
		((chip)->ops->reg_readl((dev)->pphc_addr + (reg)) & (mask_and)) | (mask_or), \
		((dev)->pphc_addr + (reg))))
#define azx_pphc_writew_andor(chip, dev, reg, mask_and, mask_or) \
	((chip)->ops->reg_writew( \
		((chip)->ops->reg_readl((dev)->pphc_addr + (reg)) & (mask_and)) | (mask_or), \
		(dev)->pphc_addr + (reg)))
#define azx_pphc_writeb_andor(chip, dev, reg, mask_and, mask_or) \
	((chip)->ops->reg_writeb( \
		((chip)->ops->reg_readb((dev)->pphc_addr + (reg)) & (mask_and)) | (mask_or), \
		(dev)->pphc_addr + (reg)))
#define azx_pplc_writel_andor(chip, dev, reg, mask_and, mask_or) \
	((chip)->ops->reg_writel( \
		((chip)->ops->reg_readl((dev)->pplc_addr + (reg)) & (mask_and)) | (mask_or), \
		(dev)->pplc_addr + (reg)))
#define azx_pplc_writew_andor(chip, dev, reg, mask_and, mask_or) \
	((chip)->ops->reg_writew( \
		((chip)->ops->reg_readl((dev)->pplc_addr + (reg)) & (mask_and)) | (mask_or),\
		(dev)->pplc_addr + (reg)))
#define azx_pplc_writeb_andor(chip, dev, reg, mask_and, mask_or) \
	((chip)->ops->reg_writeb( \
		((chip)->ops->reg_readb((dev)->pplc_addr + (reg)) & (mask_and)) | (mask_or),\
		(dev)->pplc_addr + (reg)))

#endif /* __SOUND_HDA_REGISTER_H */
