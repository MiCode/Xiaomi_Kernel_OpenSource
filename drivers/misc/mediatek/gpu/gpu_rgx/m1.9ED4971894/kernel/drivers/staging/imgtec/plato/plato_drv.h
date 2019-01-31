/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File           plato_drv.h
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#ifndef _PLATO_DRV_H
#define _PLATO_DRV_H

/*
 * This contains the hooks for the plato pci driver, as used by the
 * Rogue and PDP sub-devices, and the platform data passed to each of their
 * drivers
 */

#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/delay.h>

#define PLATO_INIT_SUCCESS	0
#define PLATO_INIT_FAILURE	1
#define PLATO_INIT_RETRY	2

#define PCI_VENDOR_ID_PLATO				(0x1AEE)
#define PCI_DEVICE_ID_PLATO				(0x0003)

#define PLATO_SYSTEM_NAME				"Plato"

/* Interrupt defines */
typedef enum _PLATO_INTERRUPT_ {
	PLATO_INTERRUPT_GPU = 0,
	PLATO_INTERRUPT_PDP,
	PLATO_INTERRUPT_HDMI,
	PLATO_INTERRUPT_MAX,
} PLATO_INTERRUPT;

#define PLATO_INT_SHIFT_GPU				(0)
#define PLATO_INT_SHIFT_PDP				(8)
#define PLATO_INT_SHIFT_HDMI			(9)
#define PLATO_INT_SHIFT_HDMI_WAKEUP		(11)
#define PLATO_INT_SHIFT_TEMP_A			(12)


typedef struct plato_region_ {
	resource_size_t base;
	resource_size_t size;
} plato_region;

typedef struct plato_io_region_ {
	plato_region region;
	void __iomem *registers;
} plato_io_region;

/* The following structs are initialised and passed down by the parent plato
 * driver to the respective sub-drivers
 */

#define PLATO_DEVICE_NAME_PDP			"plato_pdp"
#define PLATO_PDP_RESOURCE_REGS			"pdp-regs"
#define PLATO_PDP_RESOURCE_BIF_REGS		"pdp-bif-regs"

#define PLATO_DEVICE_NAME_HDMI			"plato_hdmi"
#define PLATO_HDMI_RESOURCE_REGS		"hdmi-regs"

struct plato_pdp_platform_data {
	resource_size_t memory_base;

	/* The following is used by the drm_pdp driver as it manages the
	 * pdp memory
	 */
	resource_size_t pdp_heap_memory_base;
	resource_size_t pdp_heap_memory_size;
};

typedef struct plato_hdmi_platform_data_ {

	resource_size_t plato_memory_base;

} plato_hdmi_platform_data;


#define PLATO_DEVICE_NAME_ROGUE			"plato_rogue"
#define PLATO_ROGUE_RESOURCE_REGS		"rogue-regs"

typedef struct plato_rogue_platform_data_ {

	/* The base address of the plato memory (CPU physical address) -
	 * used to convert from CPU-Physical to device-physical addresses
	 */
	resource_size_t plato_memory_base;

	/* The following is used to setup the services heaps */
	int has_nonmappable;
	plato_region rogue_heap_mappable;
	resource_size_t rogue_heap_dev_addr;
	plato_region rogue_heap_nonmappable;
#if defined(SUPPORT_PLATO_DISPLAY)
	plato_region pdp_heap;
#endif
} plato_rogue_platform_data;

typedef struct plato_interrupt_handler_ {
	bool enabled;
	void (*handler_function)(void *);
	void *handler_data;
} plato_interrupt_handler;

typedef struct plato_device_ {
	struct pci_dev *pdev;

	plato_io_region sys_io;
	plato_io_region aon_regs;

	spinlock_t interrupt_handler_lock;
	spinlock_t interrupt_enable_lock;

	plato_interrupt_handler
		interrupt_handlers[PLATO_INTERRUPT_MAX];

	plato_region rogue_mem;
	plato_region rogue_heap_mappable;
	plato_region rogue_heap_nonmappable;
	int has_nonmappable;

	resource_size_t dev_mem_base; /* Pointer to device memory base */

	struct platform_device *rogue_dev;

#if defined(SUPPORT_PLATO_DISPLAY)
	struct platform_device *pdp_dev;
	plato_region pdp_heap;

	struct platform_device *hdmi_dev;
#endif

#if defined(CONFIG_MTRR) || (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	int mtrr;
#endif

#if defined(PLATO_DEBUGFS)
	struct dentry *debugfs_plato_dir;
	struct dentry *debugfs_rogue_name;
#endif
} plato_device;

#if defined(PLATO_LOG_CHECKPOINTS)
#define PLATO_CHECKPOINT(p) dev_info(&p->pdev->dev, "- %s: %d", __func__, __LINE__)
#else
#define PLATO_CHECKPOINT(p)
#endif

#define plato_write_reg32(base, offset, value) \
	iowrite32(value, (base) + (offset))
#define plato_read_reg32(base, offset) ioread32(base + offset)
#define plato_sleep_ms(x) msleep(x)
#define plato_sleep_us(x) msleep(x/1000)

/* Valid values for the PLATO_MEMORY_CONFIG configuration option */
#define PLATO_MEMORY_LOCAL			(1)
#define PLATO_MEMORY_HOST			(2)
#define PLATO_MEMORY_HYBRID			(3)

#if defined(PLATO_MEMORY_CONFIG)
#if (PLATO_MEMORY_CONFIG == PLATO_MEMORY_HYBRID)
#define PVRSRV_DEVICE_PHYS_HEAP_PDP_LOCAL 2
#elif (PLATO_MEMORY_CONFIG == PLATO_MEMORY_LOCAL)
#define PVRSRV_DEVICE_PHYS_HEAP_PDP_LOCAL 1
#endif
#endif /* PLATO_MEMORY_CONFIG */

#define DCPDP_PHYS_HEAP_ID PVRSRV_DEVICE_PHYS_HEAP_PDP_LOCAL

#define PLATO_PDP_MEM_SIZE			(32 * 1024 * 1024)

#define SYS_PLATO_REG_PCI_BASENUM	(1)
#define SYS_PLATO_REG_REGION_SIZE	(4 * 1024 * 1024)

/*
 * Give system region a whole span of the reg space including
 * RGX registers. That's because there are sys register segments
 * both before and after the RGX segment.
 */
#define SYS_PLATO_REG_SYS_OFFSET			(0x0)
#define SYS_PLATO_REG_SYS_SIZE				(4 * 1024 * 1024)

/* Entire Peripheral region */
#define SYS_PLATO_REG_PERIP_OFFSET			(0x20000)
#define SYS_PLATO_REG_PERIP_SIZE			(164 * 1024)

/* Chip level registers */
#define SYS_PLATO_REG_CHIP_LEVEL_OFFSET		(SYS_PLATO_REG_PERIP_OFFSET)
#define SYS_PLATO_REG_CHIP_LEVEL_SIZE		(64 * 1024)

#define SYS_PLATO_REG_TEMPA_OFFSET			(0x80000)
#define SYS_PLATO_REG_TEMPA_SIZE			(64 * 1024)

/* USB, DMA not included */

#define SYS_PLATO_REG_DDR_A_CTRL_OFFSET		(0x120000)
#define SYS_PLATO_REG_DDR_A_CTRL_SIZE		(64 * 1024)

#define SYS_PLATO_REG_DDR_B_CTRL_OFFSET		(0x130000)
#define SYS_PLATO_REG_DDR_B_CTRL_SIZE		(64 * 1024)

#define SYS_PLATO_REG_DDR_A_PUBL_OFFSET		(0x140000)
#define SYS_PLATO_REG_DDR_A_PUBL_SIZE		(64 * 1024)

#define SYS_PLATO_REG_DDR_B_PUBL_OFFSET		(0x150000)
#define SYS_PLATO_REG_DDR_B_PUBL_SIZE		(64 * 1024)

#define SYS_PLATO_REG_NOC_OFFSET			(0x160000)
#define SYS_PLATO_REG_NOC_SIZE		        (64 * 1024)

/* Debug NOC registers */
#define SYS_PLATO_REG_NOC_DBG_DDR_A_CTRL_OFFSET (0x1500)
#define SYS_PLATO_REG_NOC_DBG_DDR_A_DATA_OFFSET (0x1580)
#define SYS_PLATO_REG_NOC_DBG_DDR_A_PUBL_OFFSET (0x1600)
#define SYS_PLATO_REG_NOC_DBG_DDR_B_CTRL_OFFSET (0x1680)
#define SYS_PLATO_REG_NOC_DBG_DDR_B_DATA_OFFSET (0x1700)
#define SYS_PLATO_REG_NOC_DBG_DDR_B_PUBL_OFFSET (0x1780)
#define SYS_PLATO_REG_NOC_DBG_DISPLAY_S_OFFSET  (0x1800)
#define SYS_PLATO_REG_NOC_DBG_GPIO_0_S_OFFSET   (0x1900)
#define SYS_PLATO_REG_NOC_DBG_GPIO_1_S_OFFSET   (0x1980)
#define SYS_PLATO_REG_NOC_DBG_GPU_S_OFFSET      (0x1A00)
#define SYS_PLATO_REG_NOC_DBG_PCI_PHY_OFFSET    (0x1A80)
#define SYS_PLATO_REG_NOC_DBG_PCI_REG_OFFSET    (0x1B00)
#define SYS_PLATO_REG_NOC_DBG_PCI_S_OFFSET      (0x1B80)
#define SYS_PLATO_REG_NOC_DBG_PERIPH_S_OFFSET   (0x1c00)
#define SYS_PLATO_REG_NOC_DBG_RET_REG_OFFSET    (0x1D00)
#define SYS_PLATO_REG_NOC_DBG_SERVICE_OFFSET    (0x1E00)

#define SYS_PLATO_REG_RGX_OFFSET			(0x170000)
#define SYS_PLATO_REG_RGX_SIZE				(64 * 1024)

#define SYS_PLATO_REG_AON_OFFSET			(0x180000)
#define SYS_PLATO_REG_AON_SIZE				(64 * 1024)

#define SYS_PLATO_REG_PDP_OFFSET			(0x200000)
#define SYS_PLATO_REG_PDP_SIZE				(128 * 1024)

#define SYS_PLATO_REG_PDP_BIF_OFFSET        (SYS_PLATO_REG_PDP_OFFSET + SYS_PLATO_REG_PDP_SIZE)
#define SYS_PLATO_REG_PDP_BIF_SIZE          (0x200)

#define SYS_PLATO_REG_HDMI_OFFSET           (SYS_PLATO_REG_PDP_OFFSET + 0x20000)
#define SYS_PLATO_REG_HDMI_SIZE             (128 * 1024)

/* Device memory (including HP mapping) on base register 4 */
#define SYS_DEV_MEM_PCI_BASENUM		(4)

/* Device memory size */
#define ONE_GB_IN_BYTES					(0x40000000ULL)
#define SYS_DEV_MEM_REGION_SIZE			(PLATO_MEMORY_SIZE_GIGABYTES * ONE_GB_IN_BYTES)

/* Plato DDR offset in device memory map at 32GB */
#define PLATO_DDR_DEV_PHYSICAL_BASE		(0x800000000)

/* DRAM is split at 48GB */
#define PLATO_DRAM_SPLIT_ADDR			(0xc00000000)

/*
 * Plato DDR region is aliased if less than 32GB memory is present.
 * This defines memory base closest to the DRAM split point.
 * If 32GB is present this is equal to PLATO_DDR_DEV_PHYSICAL_BASE
 */
#define PLATO_DDR_ALIASED_DEV_PHYSICAL_BASE \
	(PLATO_DRAM_SPLIT_ADDR - (SYS_DEV_MEM_REGION_SIZE >> 1))

#define PLATO_DDR_ALIASED_DEV_PHYSICAL_END \
	(PLATO_DRAM_SPLIT_ADDR + (SYS_DEV_MEM_REGION_SIZE >> 1))

#define PLATO_DDR_ALIASED_DEV_SEGMENT_SIZE \
	((32ULL / PLATO_MEMORY_SIZE_GIGABYTES) * ONE_GB_IN_BYTES)

/* Plato Host memory offset in device memory map at 512GB */
#define PLATO_HOSTRAM_DEV_PHYSICAL_BASE (0x8000000000)

/* Plato PLL, DDR/GPU, PDP and HDMI-SFR/CEC clocks */
#define PLATO_PLL_REF_CLOCK_SPEED	(19200000)

/* 600 MHz */
#define PLATO_MEM_CLOCK_SPEED		(600000000)
#define PLATO_MIN_MEM_CLOCK_SPEED	(600000000)
#define PLATO_MAX_MEM_CLOCK_SPEED	(800000000)

/* 396 MHz (~400 MHz) on HW, around 1MHz on the emulator */
#if defined(EMULATOR) || defined(VIRTUAL_PLATFORM)
#define	PLATO_RGX_CORE_CLOCK_SPEED	(1000000)
#else

#define	PLATO_RGX_CORE_CLOCK_SPEED	(396000000)
#define	PLATO_RGX_MIN_CORE_CLOCK_SPEED	(396000000)
#define	PLATO_RGX_MAX_CORE_CLOCK_SPEED	(742500000)
#endif

#define PLATO_MIN_PDP_CLOCK_SPEED		(165000000)
#define PLATO_TARGET_HDMI_SFR_CLOCK_SPEED	(27000000)
#define PLATO_TARGET_HDMI_CEC_CLOCK_SPEED	(32768)

#define REG_TO_CELSIUS(reg)			(((reg) * 352/4096) - 109)
#define CELSIUS_TO_REG(temp)		((((temp) + 109) * 4096) / 352)
#define PLATO_MAX_TEMP_CELSIUS		(100)

#define PLATO_LMA_HEAP_REGION_MAPPABLE			0
#define PLATO_LMA_HEAP_REGION_NONMAPPABLE		1

typedef struct {
	char *description;
	unsigned int offset;
	unsigned int value;
} PLATO_DBG_REG;

#if defined(ENABLE_PLATO_HDMI)

#if defined(HDMI_PDUMP)
/* Hard coded video formats for pdump type run only */
#define VIDEO_FORMAT_1280_720p          0
#define VIDEO_FORMAT_1920_1080p         1
#define DC_DEFAULT_VIDEO_FORMAT     (VIDEO_FORMAT_1920_1080p)
#endif

#define DEFAULT_BORDER_WIDTH_PIXELS     (0)
#define HDMI_DTD_MAX                    (20)

typedef enum _ENCODING_T_ {
	ENCODING_RGB = 0,
	ENCODING_YCC444,
	ENCODING_YCC422,
	ENCODING_YCC420
} ENCODING_T;

typedef enum _COLORIMETRY_T_ {
	ITU601 = 1,
	ITU709,
	EXTENDED_COLORIMETRY
} COLORIMETRY_T;

typedef struct _DTD_ {
	/** VIC code */
	u32 mCode;
	/** Identifies modes that ONLY can be displayed in YCC 4:2:0 */
	u8 mLimitedToYcc420;
	/** Identifies modes that can also be displayed in YCC 4:2:0 */
	u8 mYcc420;
	u16 mPixelRepetitionInput;
	/** in units of 10KHz */
	u16 mPixelClock;
	/** 1 for interlaced, 0 progressive */
	u8 mInterlaced;
	u16 mHActive;
	u16 mHBlanking;
	u16 mHBorder;
	u16 mHImageSize;
	u16 mHFrontPorchWidth;
	u16 mHBackPorchWidth;
	u16 mHSyncPulseWidth;
	/** 0 for Active low, 1 active high */
	u8 mHSyncPolarity;
	u16 mVActive;
	u16 mVBlanking;
	u16 mVBorder;
	u16 mVImageSize;
	u16 mVFrontPorchWidth;
	u16 mVBackPorchWidth;
	u16 mVSyncPulseWidth;
	/** 0 for Active low, 1 active high */
	u8 mVSyncPolarity;

	u8 mValid;
} DTD;

/* Monitor range limits comes from Display Descriptor (EDID) */
typedef struct _MONITOR_RANGE_LIMITS {
	u8 vMinRate;
	u8 vMaxRate;
	u8 hMinRate;
	u8 hMaxRate;
	u8 maxPixelClock;
	u8 valid;
} MONITOR_RANGE_LIMITS;

typedef struct _VIDEO_PARAMS {
	u8                      mHdmi;
	ENCODING_T              mEncodingOut;
	ENCODING_T              mEncodingIn;
	u8                      mColorResolution;
	u8                      mPixelRepetitionFactor;
	DTD                     mDtdList[HDMI_DTD_MAX];
	u8                      mDtdIndex;
	u8                      mDtdActiveIndex;
	u8                      mRgbQuantizationRange;
	u8                      mPixelPackingDefaultPhase;
	u8                      mColorimetry;
	u8                      mScanInfo;
	u8                      mActiveFormatAspectRatio;
	u8                      mNonUniformScaling;
	u8                      mExtColorimetry;
	u8                      mItContent;
	u16                     mEndTopBar;
	u16                     mStartBottomBar;
	u16                     mEndLeftBar;
	u16                     mStartRightBar;
	u16                     mCscFilter;
	u16                     mCscA[4];
	u16                     mCscC[4];
	u16                     mCscB[4];
	u16                     mCscScale;
	u8                      mHdmiVideoFormat;
	u8                      m3dStructure;
	u8                      m3dExtData;
	u8                      mHdmiVic;
	u8                      mDataEnablePolarity;
	MONITOR_RANGE_LIMITS    mRangeLimits;
	u8                      mPreferredTimingIncluded;
} VIDEO_PARAMS;

#endif /* ENABLE_PLATO_HDMI */

/* Exposed APIs */
int plato_enable(struct device *dev);
void plato_disable(struct device *dev);

int plato_enable_interrupt(struct device *dev, PLATO_INTERRUPT interrupt_id);
int plato_disable_interrupt(struct device *dev, PLATO_INTERRUPT interrupt_id);

int plato_set_interrupt_handler(struct device *dev,
	PLATO_INTERRUPT interrupt_id,
	void (*handler_function)(void *),
	void *handler_data);
unsigned int plato_core_clock_speed(struct device *dev);
unsigned int plato_mem_clock_speed(struct device *dev);
unsigned int plato_pll_clock_speed(struct device *dev,
	unsigned int clock_speed);
void plato_enable_pdp_clock(struct device *dev);

int plato_debug_info(struct device *dev,
	PLATO_DBG_REG *noc_dbg_regs,
	PLATO_DBG_REG *aon_dbg_regs);

/* Internal */
int plato_memory_init(plato_device *plato);
void plato_memory_deinit(plato_device *plato);
int plato_cfg_init(plato_device *plato);
int request_pci_io_addr(struct pci_dev *pdev, u32 index,
	resource_size_t offset, resource_size_t length);
void release_pci_io_addr(struct pci_dev *pdev, u32 index,
	resource_size_t start, resource_size_t length);

#if defined(PLATO_SYSTEM_PDUMP)
#include "plato_pdump.h"

static void plato_write_reg32_pdump(void *base, u32 offset, u32 value)
{
	plato_write_reg32(base, offset, value);
	plato_pdump_reg32(base, offset, value, PLATO_SYSTEM_NAME);
}

static void plato_sleep_ms_pdump(u32 intrvl)
{
	msleep(intrvl);
	plato_pdump_idl(intrvl);
}
#undef plato_write_reg32
#undef plato_sleep_ms

#define plato_write_reg32 plato_write_reg32_pdump
#define plato_sleep_ms plato_sleep_ms_pdump
#endif

#endif /* _PLATO_DRV_H */
