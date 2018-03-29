/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/printk.h>
#include <asm/page.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#ifdef CONFIG_OF
#ifndef CONFIG_MTK_LEGACY
#include <linux/regulator/consumer.h>
#endif
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif
#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT
#include <linux/xlog.h>
#include <mach/mtk_thermal_monitor.h>
#include "mach/mt_vcore_dvfs.h"
#endif /* MTK_SDIO30_ONLINE_TUNING_SUPPORT */

#include <queue.h>
#include <linux/gpio.h>

#include <mt-plat/mt_boot.h>
#include <mt-plat/partition.h>

#ifdef CONFIG_MTK_HIBERNATION
#include <mtk_hibernate_dpm.h>
#endif

#include "mt_sd.h"
#include "msdc_hw_ett.h"
#include "dbg.h"
#include "board.h"

#include<mt-plat/upmu_common.h>

/* weiping fix */
#if 0
#include <mach/dma.h>


#include <mach/devs.h>
#include <mach/mt_typedefs.h>
#include <linux/mmc/sd_misc.h>
#include <mach/mt_chip.h>
#include "dbg.h"
#ifdef MTK_MSDC_BRINGUP_DEBUG
#include <mach/mt_pmic_wrap.h>
#endif
#include <linux/met_drv.h>
#include <mach/eint.h>
#include <mach/mt_storage_logger.h>
#include <mach/partition.h>
#include <mach/emi_mpu.h>
#include <mach/memory.h>
#endif

#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#else
#include <mt_idle.h>
struct clk *g_msdc0_pll_sel = NULL;
struct clk *g_msdc0_pll_800m = NULL;
struct clk *g_msdc0_pll_400m = NULL;
struct clk *g_msdc0_pll_200m = NULL;
#endif

static int msdc_get_card_status(struct mmc_host *mmc,
	struct msdc_host *host, u32 *status);
static void msdc_clksrc_onoff(struct msdc_host *host, u32 on);

/* ========================= move from dbg.c start =========================*/
/* for debug zone */
unsigned int sd_debug_zone[HOST_MAX_NUM] = {
	0,
	0,
	0,
	0,
};

/* for enable/disable register dump */
unsigned int sd_register_zone[HOST_MAX_NUM] = {
	1,
	1,
	1,
	1,
};
/* mode select */
u32 dma_size[HOST_MAX_NUM] = {
	512,
	512,
	512,
	512,
};

int drv_mode[HOST_MAX_NUM] = {
#define MODE_PIO		(0)
#define MODE_DMA		(1)
#define MODE_SIZE_DEP	(2)
	MODE_SIZE_DEP, /* using DMA or not depend on the size */
	MODE_SIZE_DEP,
	MODE_SIZE_DEP,
	MODE_SIZE_DEP,
};

unsigned char msdc_clock_src[HOST_MAX_NUM] = {
	0,
	0,
	0,
	0,
};

u32 msdc_host_mode[HOST_MAX_NUM] = {
	0,
	0,
	0,
	0,
};

u32 msdc_host_mode2[HOST_MAX_NUM] = {
	0,
	0,
	0,
	0,
};

int g_ett_tune = 0;			/* enable or disable the ETT tune */
int g_ett_hs400_tune = 0;	/* record the number of failed HS400 ETT settings */
int g_ett_cmd_tune = 0;		/* record the number of failed cmd ETT settings   */
int g_ett_read_tune = 0;	/* record the number of failed read ETT settings  */
int g_ett_write_tune = 0;	/* record the number of failed write ETT settings */
/* do not record the pass settigns, but try the worst setting of each request */
int g_reset_tune = 0;
u32 sdio_tune_flag = 0;
/* ========================= move from dbg.c end =========================*/

#define CAPACITY_2G                      (2 * 1024 * 1024 * 1024ULL)

/* #ifdef CONFIG_MTK_EMMC_SUPPORT */
#if 0
u32 g_emmc_mode_switch = 0;
EXPORT_SYMBOL(g_emmc_mode_switch);

/* #define MTK_EMMC_ETT_TO_DRIVER for eMMC off-line apply to driver */
#endif
static void msdc_init_hw(struct msdc_host *host);

#ifndef FPGA_PLATFORM
#ifndef CONFIG_MTK_LEGACY
struct regulator *reg_VEMC_3V3 = NULL;
struct regulator *reg_VMC = NULL;
struct regulator *reg_VMCH = NULL;
#endif
#endif

#ifdef MTK_MSDC_USE_CACHE
#define MSDC_MAX_FLUSH_COUNT    (3)
#define CACHE_UN_FLUSHED        (0)
#define CACHE_FLUSHED           (1)
static unsigned int g_cache_status = CACHE_UN_FLUSHED;
static unsigned long long g_flush_data_size;
static unsigned int g_flush_error_count;
static int g_flush_error_happend;
static int g_bypass_flush;
unsigned long long g_cache_part_start;
unsigned long long g_cache_part_end;
unsigned long long g_usrdata_part_start;
unsigned long long g_usrdata_part_end;

unsigned int g_emmc_cache_size = 0;
/* if disable cache by vendor fill CID.MID to g_emmc_cache_quirk[i] */
unsigned char g_emmc_cache_quirk[256];
#define CID_MANFID_SANDISK		0x2
#define CID_MANFID_TOSHIBA		0x11
#define CID_MANFID_MICRON		0x13
#define CID_MANFID_SAMSUNG		0x15
#define CID_MANFID_SANDISK_NEW	0x45
#define CID_MANFID_HYNIX		0x90
#define CID_MANFID_KSI			0x70
#endif

unsigned long long msdc_print_start_time;
unsigned long long msdc_print_end_time;
unsigned int print_nums;

static u8 emmc_id;
#ifdef MTK_EMMC_ETT_TO_DRIVER
#include "emmc_device_list.h"
static u8 m_id;		/* Manufacturer ID */
static char pro_name[8] = { 0 };	/* Product name */
#endif

#if (MSDC_DATA1_INT == 1)
static u16 u_sdio_irq_counter;
static u16 u_msdc_irq_counter;
/* static int int_sdio_irq_enable; */
#endif

struct msdc_host *ghost = NULL;
int src_clk_control = 0;
struct mmc_blk_data {
	spinlock_t lock;
	struct gendisk *disk;
	struct mmc_queue queue;

	unsigned int usage;
	unsigned int read_only;
};
static bool emmc_sleep_failed;
static int emmc_do_sleep_awake;
static struct workqueue_struct *wq_tune;

#define DRV_NAME                         "mtk-sdio"

#define MSDC_COOKIE_PIO        (1<<0)
#define MSDC_COOKIE_ASYNC    (1<<1)

#define msdc_use_async_way(x)  (x & MSDC_COOKIE_ASYNC)
#define msdc_async_use_dma(x)  ((x & MSDC_COOKIE_ASYNC) && (!(x & MSDC_COOKIE_PIO)))
#define msdc_async_use_pio(x)  ((x & MSDC_COOKIE_ASYNC) && ((x & MSDC_COOKIE_PIO)))
#ifdef FPGA_PLATFORM
#define HOST_MAX_MCLK			(200000000)
#else
#define HOST_MAX_MCLK			(200000000)
#endif
#define HOST_MIN_MCLK			(260000)

#define HOST_MAX_BLKSZ			(2048)

#define MSDC_OCR_AVAIL			(MMC_VDD_28_29 | MMC_VDD_29_30 | \
								MMC_VDD_30_31 | MMC_VDD_31_32 | \
								MMC_VDD_32_33)
/* #define MSDC_OCR_AVAIL		(MMC_VDD_32_33 | MMC_VDD_33_34) */

#define MSDC1_IRQ_SEL                    (1 << 9)

#define DEFAULT_DEBOUNCE                 (8)	/* 8 cycles */
/* data timeout counter. 65536x40(75/77) /1048576 * 3(83/85) sclk. */
#define DEFAULT_DTOC                     (3)

#define CMD_TIMEOUT                      (HZ/10 * 5)	/* 100ms x5 */
#define DAT_TIMEOUT                      (HZ    * 5)	/* 1000ms x5 */
#define CLK_TIMEOUT                      (HZ    * 5)	/* 5s    */
#define POLLING_BUSY                     (HZ     * 3)
/* a single transaction for WIFI may be 50K */
#define MAX_DMA_CNT                      (64 * 1024 - 512)
/*
 * a single transaction for LTE may be 128K
 * Basic DMA use 32 bits to store transfer size
 */
#define MAX_DMA_CNT_SDIO                 (0xFFFFFFFF - 255)

#define MAX_HW_SGMTS                     (MAX_BD_NUM)
#define MAX_PHY_SGMTS                    (MAX_BD_NUM)
#define MAX_SGMT_SZ                      (MAX_DMA_CNT)
#define MAX_SGMT_SZ_SDIO                 (MAX_DMA_CNT_SDIO)

#define CMD_TUNE_UHS_MAX_TIME            (2*32*8*8)
#define CMD_TUNE_HS_MAX_TIME             (2*32)

#define READ_TUNE_UHS_CLKMOD1_MAX_TIME   (2*32*32*8)
#define READ_TUNE_UHS_MAX_TIME           (2*32*32)
#define READ_TUNE_HS_MAX_TIME            (2*32)

#define WRITE_TUNE_HS_MAX_TIME           (2*32)
#define WRITE_TUNE_UHS_MAX_TIME          (2*32*8)

/* ================================= */

#define MSDC_LOWER_FREQ
#define MSDC_MAX_FREQ_DIV                (2)	/* 200 / (4 * 2) */
#define MSDC_MAX_TIMEOUT_RETRY           (1)
#define MSDC_MAX_TIMEOUT_RETRY_EMMC      (2)
#define MSDC_MAX_W_TIMEOUT_TUNE          (5)
#define MSDC_MAX_W_TIMEOUT_TUNE_EMMC     (64)
#define MSDC_MAX_R_TIMEOUT_TUNE          (3)
#define MSDC_MAX_POWER_CYCLE             (5)

#define MSDC_MAX_CONTINUOUS_FAIL_REQUEST_COUNT (50)

#ifdef CONFIG_OF
static struct device_node *gpio_node;
static struct device_node *infracfg_ao_node;
static struct device_node *infracfg_node;
static struct device_node *pericfg_node;
static struct device_node *emi_node;
static struct device_node *toprgu_node;
static struct device_node *apmixed_node;
static struct device_node *topckgen_node;
static struct device_node *eint_node;
static unsigned int cd_irq;
static unsigned int cd_gpio;

void __iomem *gpio_reg_base;
void __iomem *infracfg_ao_reg_base;
void __iomem *infracfg_reg_base;
void __iomem *pericfg_reg_base;
void __iomem *emi_reg_base;
void __iomem *toprgu_reg_base;
void __iomem *apmixed_reg_base1;
void __iomem *topckgen_reg_base;

#endif
#ifdef FPGA_PLATFORM
#ifdef CONFIG_OF
static void __iomem *fpga_pwr_gpio;
static void __iomem *fpga_pwr_gpio_eo;
#define PWR_GPIO                         (fpga_pwr_gpio)
#define PWR_GPIO_EO                      (fpga_pwr_gpio_eo)
#else
#define PWR_GPIO                         (0xF0000E84)
#define PWR_GPIO_EO                      (0xF0000E88)
#endif
#define PWR_MASK_VOL_33                  (1 << 10)
#define PWR_MASK_VOL_18                  (1 << 9)
#define PWR_MASK_EN                      (1 << 8)
#define PWR_MASK_VOL_33_MASK             (~(1 << 10))
#define PWR_MASK_EN_MASK                 (~(1 << 8))
#define PWR_MASK_VOL_18_MASK             (~(1 << 9))

bool hwPowerOn_fpga(void)
{
	u16 l_val;

	l_val = sdr_read16(PWR_GPIO);
#ifdef MTK_EMMC_SUPPORT
	sdr_write16(PWR_GPIO, (l_val | PWR_MASK_VOL_18 | PWR_MASK_EN));
	/* | PWR_GPIO_L4_DIR)); */
#else
	sdr_write16(PWR_GPIO, (l_val | PWR_MASK_VOL_33 | PWR_MASK_EN));
	/* | PWR_GPIO_L4_DIR)); */
#endif
	l_val = sdr_read16(PWR_GPIO);
	pr_debug("[%s]: pwr gpio = 0x%x\n", __func__, l_val);
	return true;
}
EXPORT_SYMBOL(hwPowerOn_fpga);

bool hwPowerSwitch_fpga(void)
{
	u16 l_val;

	l_val = sdr_read16(PWR_GPIO);
	sdr_write16(PWR_GPIO, (l_val & PWR_MASK_VOL_33_MASK));
	l_val = sdr_read16(PWR_GPIO);
	sdr_write16(PWR_GPIO, (l_val | PWR_MASK_VOL_18));

	l_val = sdr_read16(PWR_GPIO);
	pr_debug("[%s]: pwr gpio = 0x%x\n", __func__, l_val);
	return true;
}
EXPORT_SYMBOL(hwPowerSwitch_fpga);

bool hwPowerDown_fpga(void)
{
	u16 l_val;

	l_val = sdr_read16(PWR_GPIO);
#ifdef MTK_EMMC_SUPPORT
	sdr_write8(PWR_GPIO, (l_val & PWR_MASK_VOL_18_MASK & PWR_MASK_EN_MASK));
#else
	sdr_write8(PWR_GPIO, (l_val & PWR_MASK_VOL_18_MASK & PWR_MASK_VOL_33_MASK
		& PWR_MASK_EN_MASK));
#endif
	l_val = sdr_read16(PWR_GPIO);
	pr_debug("[%s]: pwr gpio = 0x%x\n", __func__, l_val);
	return true;
}
EXPORT_SYMBOL(hwPowerDown_fpga);
#endif

struct msdc_host *mtk_msdc_host[] = { NULL, NULL, NULL, NULL };
EXPORT_SYMBOL(mtk_msdc_host);

int g_dma_debug[HOST_MAX_NUM] = { 0, 0, 0, 0 };
u32 latest_int_status[HOST_MAX_NUM] = { 0, 0, 0, 0 };
/* 0 for PIO; 1 for DMA; 2 for nothing */
int msdc_latest_transfer_mode[HOST_MAX_NUM] = {
#define TRAN_MOD_PIO	(0)
#define TRAN_MOD_DMA	(1)
#define TRAN_MOD_NUM	(2)
	TRAN_MOD_NUM,
	TRAN_MOD_NUM,
	TRAN_MOD_NUM,
	TRAN_MOD_NUM,
};
/* 0 for read; 1 for write; 2 for nothing */
int msdc_latest_operation_type[HOST_MAX_NUM] = {
#define OPER_TYPE_READ	(0)
#define OPER_TYPE_WRITE	(1)
#define OPER_TYPE_NUM	(2)
	OPER_TYPE_NUM,
	OPER_TYPE_NUM,
	OPER_TYPE_NUM,
	OPER_TYPE_NUM,
};

#ifdef MSDC_DMA_ADDR_DEBUG
struct dma_addr msdc_latest_dma_address[MAX_BD_PER_GPD];
#endif

static int msdc_rsp[] = {
	0,			/* RESP_NONE */
	1,			/* RESP_R1 */
	2,			/* RESP_R2 */
	3,			/* RESP_R3 */
	4,			/* RESP_R4 */
	1,			/* RESP_R5 */
	1,			/* RESP_R6 */
	1,			/* RESP_R7 */
	7,			/* RESP_R1b */
};

void msdc_dump_padctl(struct msdc_host *host)
{
	switch (host->id) {
	case 0:
		pr_err("0:GPIO[%p]=0x%.8x\n", MSDC0_GPIO_MODE18_ADDR,
			sdr_read32(MSDC0_GPIO_MODE18_ADDR));
		pr_err("0:GPIO[%p]=0x%.8x\n", MSDC0_GPIO_MODE19_ADDR,
			sdr_read32(MSDC0_GPIO_MODE19_ADDR));
		pr_err("0:GPIO[%p]=0x%.8x\n", MSDC0_GPIO_IES_G5_ADDR,
			sdr_read32(MSDC0_GPIO_IES_G5_ADDR));
		pr_err("0:GPIO[%p]=0x%.8x\n", MSDC0_GPIO_SMT_G5_ADDR,
			sdr_read32(MSDC0_GPIO_SMT_G5_ADDR));
		pr_err("0:GPIO[%p]=0x%.8x\n", MSDC0_GPIO_TDSEL0_G5_ADDR,
			sdr_read32(MSDC0_GPIO_TDSEL0_G5_ADDR));
		pr_err("0:GPIO[%p]=0x%.8x\n", MSDC0_GPIO_RDSEL0_G5_ADDR,
			sdr_read32(MSDC0_GPIO_RDSEL0_G5_ADDR));
		pr_err("0:GPIO[%p]=0x%.8x\n", MSDC0_GPIO_DRV0_G5_ADDR,
			sdr_read32(MSDC0_GPIO_DRV0_G5_ADDR));
		pr_err("0:GPIO[%p]=0x%.8x\n", MSDC0_GPIO_PUPD0_G5_ADDR,
			sdr_read32(MSDC0_GPIO_PUPD0_G5_ADDR));
		pr_err("0:GPIO[%p]=0x%.8x\n", MSDC0_GPIO_PUPD1_G5_ADDR,
			sdr_read32(MSDC0_GPIO_PUPD1_G5_ADDR));
		break;
	case 1:
		pr_err("1:GPIO[%p]=0x%.8x\n", MSDC1_GPIO_MODE17_ADDR,
			sdr_read32(MSDC1_GPIO_MODE17_ADDR));
		pr_err("1:GPIO[%p]=0x%.8x\n", MSDC1_GPIO_MODE18_ADDR,
			sdr_read32(MSDC1_GPIO_MODE18_ADDR));
		pr_err("1:GPIO[%p]=0x%.8x\n", MSDC1_GPIO_IES_G4_ADDR,
			sdr_read32(MSDC1_GPIO_IES_G4_ADDR));
		pr_err("1:GPIO[%p]=0x%.8x\n", MSDC1_GPIO_SMT_G4_ADDR,
			sdr_read32(MSDC1_GPIO_SMT_G4_ADDR));
		pr_err("1:GPIO[%p]=0x%.8x\n", MSDC1_GPIO_TDSEL0_G4_ADDR,
			sdr_read32(MSDC1_GPIO_TDSEL0_G4_ADDR));
		pr_err("1:GPIO[%p]=0x%.8x\n", MSDC1_GPIO_RDSEL0_G4_ADDR,
			sdr_read32(MSDC1_GPIO_RDSEL0_G4_ADDR));
		pr_err("1:GPIO[%p]=0x%.8x\n", MSDC1_GPIO_DRV0_G4_ADDR,
			sdr_read32(MSDC1_GPIO_DRV0_G4_ADDR));
		pr_err("1:GPIO[%p]=0x%.8x\n", MSDC1_GPIO_PUPD0_G4_ADDR,
			sdr_read32(MSDC1_GPIO_PUPD0_G4_ADDR));
		break;
#ifdef CFG_DEV_MSDC2
	case 2:
		pr_err("2:GPIO[%p]=0x%.8x\n", MSDC2_GPIO_MODE20_ADDR,
			sdr_read32(MSDC2_GPIO_MODE20_ADDR));
		pr_err("2:GPIO[%p]=0x%.8x\n", MSDC2_GPIO_MODE21_ADDR,
			sdr_read32(MSDC2_GPIO_MODE21_ADDR));
		pr_err("2:GPIO[%p]=0x%.8x\n", MSDC2_GPIO_IES_G0_ADDR,
			sdr_read32(MSDC2_GPIO_IES_G0_ADDR));
		pr_err("2:GPIO[%p]=0x%.8x\n", MSDC2_GPIO_SMT_G0_ADDR,
			sdr_read32(MSDC2_GPIO_SMT_G0_ADDR));
		pr_err("2:GPIO[%p]=0x%.8x\n", MSDC2_GPIO_TDSEL0_G0_ADDR,
			sdr_read32(MSDC2_GPIO_TDSEL0_G0_ADDR));
		pr_err("2:GPIO[%p]=0x%.8x\n", MSDC2_GPIO_RDSEL0_G0_ADDR,
			sdr_read32(MSDC2_GPIO_RDSEL0_G0_ADDR));
		pr_err("2:GPIO[%p]=0x%.8x\n", MSDC2_GPIO_DRV0_G0_ADDR,
			sdr_read32(MSDC2_GPIO_DRV0_G0_ADDR));
		pr_err("2:GPIO[%p]=0x%.8x\n", MSDC2_GPIO_PUPD0_G0_ADDR,
			sdr_read32(MSDC2_GPIO_PUPD0_G0_ADDR));
		break;
#endif
	}
}

void msdc_dump_register(struct msdc_host *host)
{
	void __iomem *base = host->base;
	int i = host->id;

	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_CFG, sdr_read32(MSDC_CFG));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_IOCON, sdr_read32(MSDC_IOCON));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_PS, sdr_read32(MSDC_PS));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_INT, sdr_read32(MSDC_INT));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_INTEN, sdr_read32(MSDC_INTEN));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_FIFOCS,
		sdr_read32(MSDC_FIFOCS));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_CFG, sdr_read32(SDC_CFG));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_CMD, sdr_read32(SDC_CMD));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_ARG, sdr_read32(SDC_ARG));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_STS, sdr_read32(SDC_STS));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_RESP0, sdr_read32(SDC_RESP0));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_RESP1, sdr_read32(SDC_RESP1));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_RESP2, sdr_read32(SDC_RESP2));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_RESP3, sdr_read32(SDC_RESP3));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_BLK_NUM,
		sdr_read32(SDC_BLK_NUM));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_VOL_CHG,
		sdr_read32(SDC_VOL_CHG));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_CSTS, sdr_read32(SDC_CSTS));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_CSTS_EN,
		sdr_read32(SDC_CSTS_EN));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_DCRC_STS,
		sdr_read32(SDC_DCRC_STS));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC_CFG0, sdr_read32(EMMC_CFG0));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC_CFG1, sdr_read32(EMMC_CFG1));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC_STS, sdr_read32(EMMC_STS));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC_IOCON, sdr_read32(EMMC_IOCON));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_ACMD_RESP,
		sdr_read32(SDC_ACMD_RESP));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_ACMD19_TRG,
		sdr_read32(SDC_ACMD19_TRG));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDC_ACMD19_STS,
		sdr_read32(SDC_ACMD19_STS));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_DMA_SA_HIGH4BIT,
		sdr_read32(MSDC_DMA_SA_HIGH4BIT));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_DMA_SA,
		sdr_read32(MSDC_DMA_SA));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_DMA_CA,
		sdr_read32(MSDC_DMA_CA));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_DMA_CTRL,
		sdr_read32(MSDC_DMA_CTRL));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_DMA_CFG,
		sdr_read32(MSDC_DMA_CFG));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_DMA_LEN,
		sdr_read32(MSDC_DMA_LEN));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_DBG_SEL,
		sdr_read32(MSDC_DBG_SEL));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_DBG_OUT,
		sdr_read32(MSDC_DBG_OUT));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_PATCH_BIT0,
		sdr_read32(MSDC_PATCH_BIT0));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_PATCH_BIT1,
		sdr_read32(MSDC_PATCH_BIT1));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_PATCH_BIT2,
		sdr_read32(MSDC_PATCH_BIT2));

	if ((host->id == 2) || (host->id == 3)) {
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_DAT0_TUNE_CRC,
			sdr_read32(DAT0_TUNE_CRC));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_DAT0_TUNE_CRC,
			sdr_read32(DAT1_TUNE_CRC));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_DAT0_TUNE_CRC,
			sdr_read32(DAT2_TUNE_CRC));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_DAT0_TUNE_CRC,
			sdr_read32(DAT3_TUNE_CRC));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_CMD_TUNE_CRC,
			sdr_read32(CMD_TUNE_CRC));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_SDIO_TUNE_WIND,
			sdr_read32(SDIO_TUNE_WIND));
	}

	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_PAD_TUNE0,
		sdr_read32(MSDC_PAD_TUNE0));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_DAT_RDDLY0,
		sdr_read32(MSDC_DAT_RDDLY0));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_DAT_RDDLY1,
		sdr_read32(MSDC_DAT_RDDLY1));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_HW_DBG,
		sdr_read32(MSDC_HW_DBG));
	pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_MSDC_VERSION,
		sdr_read32(MSDC_VERSION));
	if (host->id == 0) {
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC50_PAD_DS_TUNE,
			sdr_read32(EMMC50_PAD_DS_TUNE));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC50_PAD_CMD_TUNE,
			sdr_read32(EMMC50_PAD_CMD_TUNE));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC50_PAD_DAT01_TUNE,
			sdr_read32(EMMC50_PAD_DAT01_TUNE));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC50_PAD_DAT23_TUNE,
			sdr_read32(EMMC50_PAD_DAT23_TUNE));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC50_PAD_DAT45_TUNE,
			sdr_read32(EMMC50_PAD_DAT45_TUNE));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC50_PAD_DAT67_TUNE,
			sdr_read32(EMMC50_PAD_DAT67_TUNE));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC50_CFG0,
			sdr_read32(EMMC50_CFG0));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC50_CFG1,
			sdr_read32(EMMC50_CFG1));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC50_CFG2,
			sdr_read32(EMMC50_CFG2));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC50_CFG3,
			sdr_read32(EMMC50_CFG3));
		pr_err("sd%d R[%x]=0x%.8x\n", i, OFFSET_EMMC50_CFG4,
			sdr_read32(EMMC50_CFG4));
	}
}

static void msdc_dump_dbg_register(struct msdc_host *host)
{
	void __iomem *base = host->base;
	u32 i;

	for (i = 0; i <= 0x27; i++) {
		sdr_write32(MSDC_DBG_SEL, i);
		pr_err("sd%d SEL:r[%x]=0x%x\n", host->id, OFFSET_MSDC_DBG_SEL, i);
		pr_err("sd%d OUT:r[%x]=0x%x\n", host->id, OFFSET_MSDC_DBG_OUT,
			sdr_read32(MSDC_DBG_OUT));
	}

	sdr_write32(MSDC_DBG_SEL, 0);
}

static void msdc_dump_clock_sts(struct msdc_host *host)
{
#ifdef MTK_MSDC_BRINGUP_DEBUG
	if (!(apmixed_reg_base1 && topckgen_reg_base && pericfg_reg_base)) {
		pr_err("apmixed_reg_base=%p,topckgen_reg_base=%p,clk_pericfg_base=%p\n",
			apmixed_reg_base1, topckgen_reg_base, pericfg_reg_base);
		return;
	}

	pr_err("MSDCPLL_PWR_CON0[0x%p][bit0~1 should be 2b'01]=0x%x\n",
		(apmixed_reg_base1 + MSDC_MSDCPLL_PWR_CON0_OFFSET),
		sdr_read32(apmixed_reg_base1 + MSDC_MSDCPLL_PWR_CON0_OFFSET));
	pr_err("MSDCPLL_CON0    [0x%p][bit0 should be 1b'1]=0x%x\n",
		(apmixed_reg_base1 + MSDC_MSDCPLL_CON0_OFFSET),
		sdr_read32(apmixed_reg_base1 + MSDC_MSDCPLL_CON0_OFFSET));
	pr_err("CLK_CFG_2       [0x%p][bit[31:24]should be 0x01]=0x%x\n",
		(topckgen_reg_base + MSDC_CLK_CFG_2_OFFSET),
		sdr_read32(topckgen_reg_base + MSDC_CLK_CFG_2_OFFSET));
	pr_err("CLK_CFG_3       [0x%p][bit[15:0]should be 0x0202]=0x%x\n",
		(topckgen_reg_base + MSDC_CLK_CFG_3_OFFSET),
		sdr_read32(topckgen_reg_base + MSDC_CLK_CFG_3_OFFSET));
	pr_err("PERI_PDN_STA0  [0x%p][bit13=msdc0, bit14=msdc1,0:on,1:off]=0x%x\n",
		(pericfg_reg_base + MSDC_PERI_PDN_STA0_OFFSET),
		sdr_read32(pericfg_reg_base + MSDC_PERI_PDN_STA0_OFFSET));
#endif
}

static void msdc_dump_ldo_sts(struct msdc_host *host)
{
#ifdef MTK_MSDC_BRINGUP_DEBUG
#if 0
	u32 ldo_en = 0, ldo_vol = 0;

	switch (host->id) {
	case 0:
		pwrap_read(0x0A24, &ldo_en);
		pwrap_read(0x0A64, &ldo_vol);
		pr_err("VEMC_EN[0x0A24]=0x%x, should:bit1=1\n", ldo_en);
		pr_err("VEMC_VOL[0x0A64]=0x%x,should:[bit[5:4]=2b'10]\n", ldo_vol);
		break;
	case 1:
		pwrap_read(0x0A20, &ldo_en);
		pwrap_read(0x0A6A, &ldo_vol);
		pr_err("VMC_EN[0x0A20]=0x%x, should:bit1=1,", ldo_en);
		pr_err("VMC_VOL[0x0A6A]=0x%x,should:bit[5:4]=2b'11(3.3V),2b'00(1.8V)\n",
			ldo_vol);
		pwrap_read(0x0A1C, &ldo_en);
		pwrap_read(0x0A66, &ldo_vol);
		pr_err("VMCH_EN[0x0A1C]==0x%x,should:bit1=1", ldo_en);
		pr_err("VMCH_VOL[0x0A66]=0x%x,should:bit[5:4]=2b'10(3.3V)\n", ldo_vol);
		break;
	default:
		break;
	}
#endif
#endif
}

/*
extern void Ana_Log_Print(void);
extern void Afe_Log_Print(void);
static void dump_audio_info(void)
{
	pr_err("=============== AUDIO INFO =============");
#ifndef CONFIG_MTK_FPGA
	Ana_Log_Print();
	Afe_Log_Print();
#endif
}
*/
static void dump_axi_bus_info(void)
{
	return; /*weiping fix */
	if (infracfg_ao_reg_base && infracfg_reg_base && pericfg_reg_base) {
		pr_err("=============== AXI BUS INFO =============");
		pr_err("reg[0x10001224]=0x%x", sdr_read32(infracfg_ao_reg_base + 0x224));
		pr_err("reg[0x10201000]=0x%x", sdr_read32(infracfg_reg_base + 0x000));
		pr_err("reg[0x10201018]=0x%x", sdr_read32(infracfg_reg_base + 0x018));
		pr_err("reg[0x1000320c]=0x%x", sdr_read32(pericfg_reg_base + 0x20c));
		pr_err("reg[0x10003210]=0x%x", sdr_read32(pericfg_reg_base + 0x210));
		pr_err("reg[0x10003214]=0x%x", sdr_read32(pericfg_reg_base + 0x214));
	} else
		pr_err("infracfg_ao_reg=%p,infracfg_reg_base=%p,pericfg_reg_base=%p\n",
			infracfg_ao_reg_base, infracfg_reg_base, pericfg_reg_base);
}

static void dump_emi_info(void)
{
	unsigned int i = 0;
	unsigned int addr = 0;

	return;	/*weiping fix */
	if (emi_reg_base) {
		pr_err("=============== EMI INFO =============");
		pr_err("before, reg[0x102034e8]=0x%x",
			sdr_read32(emi_reg_base + 0x4e8));
		pr_err("before, reg[0x10203400]=0x%x",
			sdr_read32(emi_reg_base + 0x400));
		sdr_write32(emi_reg_base + 0x4e8, 0x2000000);
		sdr_write32(emi_reg_base + 0x400, 0xff0001);
		pr_err("after, reg[0x102034e8]=0x%x", sdr_read32(emi_reg_base + 0x4e8));
		pr_err("after, reg[0x10203400]=0x%x", sdr_read32(emi_reg_base + 0x400));

		for (i = 0; i < 5; i++) {
			for (addr = 0; addr < 0x78; addr += 4) {
				pr_err("reg[0x%x]=0x%x", (0x10203500 + addr),
				       sdr_read32((emi_reg_base + 0x500 + addr)));
				if (addr % 0x10 == 0)
					mdelay(1);
			}
		}
	} else
		pr_err("emi_reg_base = %p\n", emi_reg_base);
}

void msdc_dump_info(u32 id)
{
	struct msdc_host *host = mtk_msdc_host[id];
	void __iomem *base;

	if (host == NULL) {
		pr_err("msdc host<%d> null\n", id);
		return;
	}

	/* when detect card, cmd13 will be sent which timeout log is not needed */
	if (!sd_register_zone[id]) {
		pr_err("msdc host<%d> is timeout when detect, so don't dump register\n", id);
		return;
	}
	base = host->base;

	/* 1: dump msdc hw register */
	msdc_dump_register(host);
	pr_err("msdc%d latest_INT_status<0x%.8x>\n", id, latest_int_status[id]);

	/* 2: check msdc clock gate and clock source */
	mdelay(10);
	msdc_dump_clock_sts(host);

	/* 3: check msdc pmic ldo */
	msdc_dump_ldo_sts(host);

	/* 4: check msdc pad control */
	msdc_dump_padctl(host);

	/* 5: For designer */
	mdelay(10);
	msdc_dump_dbg_register(host);
}

void msdc_polling_axi_status(int line, int dead)
{
	int i = 0;

	if (!pericfg_reg_base) {
		pr_err("pericfg_reg_base = %p\n", pericfg_reg_base);
		return;
	}

	while (sdr_read32(pericfg_reg_base + 0x214) & 0xc) {
		if (++i < 300) {
			mdelay(10);
		} else {
			pr_err("[%s]: check peri-bus: 0x%x at %d\n",
				__func__, sdr_read32(pericfg_reg_base + 0x214), line);

			pr_err("###### AXI bus hang! start ######");
			pr_err("======EMI======");
			dump_emi_info();
			mdelay(10);
			pr_err("======AXI======");
			dump_axi_bus_info();
			mdelay(10);
			pr_err("======AUDIO======");
			/* dump_audio_info(); */
			mdelay(10);
			pr_err("======MSDC======");
			msdc_dump_info(0);
			mdelay(10);
			pr_err("======GPD/BD======");
			msdc_dump_gpd_bd(0);
			pr_err("####### AXI bus hang! end ######");
			if (dead != 0)
				i = 0;
			else
				break;
		}
	}
}

/*
 * for AHB read / write debug
 * return DMA status.
 */
int msdc_get_dma_status(int host_id)
{
	int result = -1;

	if (host_id < 0 || host_id >= HOST_MAX_NUM) {
		pr_err("[%s] failed to get dma status, bad host_id %d\n",
			__func__, host_id);
		return result;
	}

	if (msdc_latest_transfer_mode[host_id] == TRAN_MOD_DMA) {
		switch (msdc_latest_operation_type[host_id]) {
		case OPER_TYPE_READ:
			result = 1;	/* DMA read */
			break;
		case OPER_TYPE_WRITE:
			result = 2;	/* DMA write */
			break;
		default:
			break;
		}
	} else if (msdc_latest_transfer_mode[host_id] == TRAN_MOD_PIO) {
		result = 0;	/* PIO mode */
	}

	return result;
}
EXPORT_SYMBOL(msdc_get_dma_status);

#ifdef MSDC_DMA_ADDR_DEBUG
struct dma_addr *msdc_get_dma_address(int host_id)
{
	struct bd_t *bd;
	int i = 0;
	int mode = -1;
	struct msdc_host *host;
	void __iomem *base;

	if (host_id < 0 || host_id >= HOST_MAX_NUM) {
		pr_err("[%s] failed to get dma status, bad host_id %d\n",
			__func__, host_id);
		return NULL;
	}

	if (!mtk_msdc_host[host_id]) {
		pr_err("[%s] failed to get dma status, msdc%d is not exist\n",
			__func__, host_id);
		return NULL;
	}

	host = mtk_msdc_host[host_id];
	base = host->base;
	/* spin_lock(&host->lock); */
	sdr_get_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_MODE, mode);
	if (mode == 1) {
		pr_crit("Desc.DMA\n");
		bd = host->dma.bd;
		i = 0;
		while (i < MAX_BD_PER_GPD) {
			msdc_latest_dma_address[i].start_address = (u32) bd[i].ptr;
			msdc_latest_dma_address[i].size = bd[i].buflen;
			msdc_latest_dma_address[i].end = bd[i].eol;
			if (i > 0)
				msdc_latest_dma_address[i - 1].next =
					&msdc_latest_dma_address[i];

			if (bd[i].eol)
				break;
			i++;
		}
	} else if (mode == 0) {
		pr_crit("Basic DMA\n");
		msdc_latest_dma_address[i].start_address = sdr_read32(MSDC_DMA_SA);
		msdc_latest_dma_address[i].size = sdr_read32(MSDC_DMA_LEN);
		msdc_latest_dma_address[i].end = 1;
	}
	/* spin_unlock(&host->lock); */

	return msdc_latest_dma_address;

}
EXPORT_SYMBOL(msdc_get_dma_address);
#endif

static void msdc_clr_fifo(unsigned int id)
{
	int retry = 3, cnt = 1000;
	void __iomem *base;

	if (id < 0 || id >= HOST_MAX_NUM)
		return;
	base = mtk_msdc_host[id]->base;

	if (sdr_read32(MSDC_DMA_CFG) & MSDC_DMA_CFG_STS) {
		pr_err("WARN: msdc%d, clear FIFO when DMA active,MSDC_DMA_CFG=0x%x\n",
			id, sdr_read32(MSDC_DMA_CFG));
		show_stack(current, NULL);
		sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP, 1);
		msdc_retry((sdr_read32(MSDC_DMA_CFG) & MSDC_DMA_CFG_STS),
			retry, cnt, id);
	}
	if (retry == 0) {
		pr_err("WARN:msdc%d,fail stop DMA before clear FIFO,MSDC_DMA_CFG=0x%x\n"
			, id, sdr_read32(MSDC_DMA_CFG));
		return;
	}

	retry = 3;
	cnt = 1000;
	sdr_set_bits(MSDC_FIFOCS, MSDC_FIFOCS_CLR);
	msdc_retry(sdr_read32(MSDC_FIFOCS) & MSDC_FIFOCS_CLR, retry, cnt, id);
}

static void msdc_reset_hw(unsigned int id)
{
	void __iomem *base;

	if (id < 0 || id >= HOST_MAX_NUM) {
		pr_err("invalid id: %d, HOST_MAX_NUM:%d", id, HOST_MAX_NUM);
		return;
	}
	base = mtk_msdc_host[id]->base;
	msdc_reset(id);
	msdc_clr_fifo(id);
	msdc_clr_int();
}

static int msdc_clk_stable(struct msdc_host *host, u32 mode, u32 div, u32 hs400_src)
{
	void __iomem *base = host->base;
	int retry = 0;
	int cnt = 1000;
	int retry_cnt = 1;
	do {
		retry = 3;
		sdr_set_field(MSDC_CFG, MSDC_CFG_CKMOD_HS400 | MSDC_CFG_CKMOD | MSDC_CFG_CKDIV,
			      (hs400_src << 10) | (mode << 8) | ((div + retry_cnt) % 0xff));
		/* sdr_set_field(MSDC_CFG, MSDC_CFG_CKMOD, mode); */
		msdc_retry(!(sdr_read32(MSDC_CFG) & MSDC_CFG_CKSTB), retry, cnt, host->id);
		if (retry == 0) {
			pr_err("msdc%d host->onclock(%d)\n", host->id, host->core_clkon);
			pr_err("msdc%d on clock failed ===> retry twice\n", host->id);
			msdc_dump_info(host->id);
		}
		retry = 3;
		sdr_set_field(MSDC_CFG, MSDC_CFG_CKDIV, div);
		msdc_retry(!(sdr_read32(MSDC_CFG) & MSDC_CFG_CKSTB), retry, cnt, host->id);
		if (retry == 0)
			msdc_dump_info(host->id);
		msdc_reset_hw(host->id);
		if (retry_cnt == 2)
			break;
		retry_cnt += 1;
	} while (!retry);

	return 0;
}

/* clock source for host: global */
#ifdef FPGA_PLATFORM
static u32 hclks[] = { 12000000, 12000000, 12000000, 12000000, 12000000,
	12000000, 12000000, 12000000, 0
};
#else
static u32 hclks_msdc50[] = { 26000000, 800000000, 400000000, 200000000,
	182000000, 136000000, 156000000, 416000000,
	48000000, 91000000, 624000000
};

static u32 hclks_msdc30[] = { 26000000, 208000000, 200000000, 182000000,
	136000000, 156000000, 48000000, 91000000
};

static u32 *hclks = hclks_msdc30;
#endif

/*
 * VMCH is for T-card main power.
 * VMC for T-card when no emmc, for eMMC when has emmc.
 * VGP for T-card when has emmc.
 */
u32 g_msdc0_io = 0;
u32 g_msdc0_flash = 0;
u32 g_msdc1_io = 0;
u32 g_msdc1_flash = 0;
u32 g_msdc2_io = 0;
u32 g_msdc2_flash = 0;
u32 g_msdc3_io = 0;
u32 g_msdc3_flash = 0;
u32 g_msdc4_io = 0;
u32 g_msdc4_flash = 0;

/* set start bit of data sampling */
void msdc_set_startbit(struct msdc_host *host, u8 start_bit)
{
	void __iomem *base = host->base;

	/* set start bit */
	sdr_set_field(MSDC_CFG, MSDC_CFG_START_BIT, start_bit);
	/* ERR_MSG("finished, start_bit=%d\n", start_bit); */
}

/* set the edge of data sampling */
void msdc_set_smpl(struct msdc_host *host, u8 HS400, u8 mode, u8 type, u8 *edge)
{
	void __iomem *base = host->base;
	int i = 0;

	switch (type) {
	case TYPE_CMD_RESP_EDGE:
		/* eMMC5.0 only output resp at CLK pin, so no need to select DS pin*/
		if (HS400) {
			sdr_set_field(EMMC50_CFG0, MSDC_EMMC50_CFG_PADCMD_LATCHCK, 0);
			sdr_set_field(EMMC50_CFG0, MSDC_EMMC50_CFG_CMD_RESP_SEL, 0);
		}

		if (mode == MSDC_SMPL_RISING || mode == MSDC_SMPL_FALLING)
#if 0
			if (HS400)
				sdr_set_field(EMMC50_CFG0, MSDC_EMMC50_CFG_CMD_EDGE_SEL, mode);
			else
				sdr_set_field(MSDC_IOCON, MSDC_IOCON_RSPL, mode);
#else
			sdr_set_field(MSDC_IOCON, MSDC_IOCON_RSPL, mode);
#endif
		else
			ERR_MSG("invalid resp parameter: HS400=%d, type=%d, mode=%d\n",
				HS400, type, mode);
		break;
	case TYPE_WRITE_CRC_EDGE:
		if (HS400) /* latch write crc status at DS pin */
			sdr_set_field(EMMC50_CFG0, MSDC_EMMC50_CFG_CRC_STS_SEL, 1);
		else	/* latch write crc status at CLK pin */
			sdr_set_field(EMMC50_CFG0, MSDC_EMMC50_CFG_CRC_STS_SEL, 0);

		if (mode == MSDC_SMPL_RISING || mode == MSDC_SMPL_FALLING) {
			if (HS400)
				sdr_set_field(EMMC50_CFG0, MSDC_EMMC50_CFG_CRC_STS_EDGE, mode);
			else {
				sdr_set_field(MSDC_IOCON, MSDC_IOCON_W_D_SMPL_SEL, 0);
				sdr_set_field(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, mode);
			}
		} else if ((mode == MSDC_SMPL_SEPARATE) && !HS400 && (edge != NULL))
			/* only dat0 is for write crc status */
			sdr_set_field(MSDC_IOCON, MSDC_IOCON_W_D0SPL, edge[0]);
		else
			ERR_MSG("invalid crc parameter: HS400=%d, type=%d, mode=%d\n",
				HS400, type, mode);
		break;
	case TYPE_READ_DATA_EDGE:
		/*
		 * for HS400, start bit is output both on rising and falling edge
		 * for the other mode, start bit is only output on rising edge.
		 * but DDR50 can try falling edge if error casued by pad delay
		 */
		if (HS400)
			msdc_set_startbit(host, START_AT_RISING_AND_FALLING);
		else
			msdc_set_startbit(host, START_AT_RISING);
		if (mode == MSDC_SMPL_RISING || mode == MSDC_SMPL_FALLING) {
			sdr_set_field(MSDC_IOCON, MSDC_IOCON_R_D_SMPL_SEL, 0);
			sdr_set_field(MSDC_IOCON, MSDC_IOCON_R_D_SMPL, mode);
		} else if ((mode == MSDC_SMPL_SEPARATE) && (edge != NULL)
		&& (sizeof(edge) == 8)) {
			sdr_set_field(MSDC_IOCON, MSDC_IOCON_R_D_SMPL_SEL, 1);
			for (i = 0; i < 8; i++)
				sdr_set_field(MSDC_IOCON, (MSDC_IOCON_R_D0SPL << i), edge[i]);
		} else
			ERR_MSG("invalid read parameter: HS400=%d, type=%d, mode=%d\n",
			HS400, type, mode);
		break;
	case TYPE_WRITE_DATA_EDGE:
		sdr_set_field(EMMC50_CFG0, MSDC_EMMC50_CFG_CRC_STS_SEL, 0);
		/* latch write crc status at CLK pin */
		if (mode == MSDC_SMPL_RISING || mode == MSDC_SMPL_FALLING) {
			sdr_set_field(MSDC_IOCON, MSDC_IOCON_W_D_SMPL_SEL, 0);
			sdr_set_field(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, mode);
		} else if ((mode == MSDC_SMPL_SEPARATE) && (edge != NULL)
		&& (sizeof(edge) >= 4)) {
			sdr_set_field(MSDC_IOCON, MSDC_IOCON_W_D_SMPL_SEL, 1);
			for (i = 0; i < 4; i++) { /* dat0~4 is for SDIO card */
				sdr_set_field(MSDC_IOCON, (MSDC_IOCON_W_D0SPL << i), edge[i]);
			}
		} else
			ERR_MSG("invalid write parameter: HS400=%d, type=%d, mode=%d\n",
			HS400, type, mode);
		break;
	default:
		ERR_MSG("invalid parameter: HS400=%d, type=%d, mode=%d\n",
			HS400, type, mode);
		break;
	}
}

#ifndef FPGA_PLATFORM
#ifndef CONFIG_MTK_LEGACY
enum MSDC_LDO_POWER {
	POWER_LDO_VMCH,
	POWER_LDO_VMC,
	POWER_LDO_VEMC_3V3,
};
bool msdc_hwPowerOn(unsigned int powerId, int powerVolt, char *mode_name)
{
	int err = 0;
	struct regulator *reg = NULL;

	if (powerId == POWER_LDO_VMCH)
		reg = reg_VMCH;
	else if (powerId == POWER_LDO_VMC)
		reg = reg_VMC;
	else if (powerId == POWER_LDO_VEMC_3V3)
		reg = reg_VEMC_3V3;
	if (reg == NULL)
		return false;

	/* New API voltage use micro V */
	regulator_set_voltage(reg, powerVolt, powerVolt);
	err = regulator_enable(reg);
	pr_err("msdc_hwPoweron:%d: name:%s err:%d", powerId, mode_name, err);
	return true;
}
EXPORT_SYMBOL(msdc_hwPowerOn);

bool msdc_hwPowerDown(unsigned int powerId, char *mode_name)
{
	struct regulator *reg = NULL;

	if (powerId == POWER_LDO_VMCH)
		reg = reg_VMCH;
	else if (powerId == POWER_LDO_VMC)
		reg = reg_VMC;
	else if (powerId == POWER_LDO_VEMC_3V3)
		reg = reg_VEMC_3V3;

	if (reg == NULL)
		return false;

	/* New API voltage use micro V */
	regulator_disable(reg);
	pr_err("msdc_hwPowerOff:%d: name:%s", powerId, mode_name);

	return true;
}
EXPORT_SYMBOL(msdc_hwPowerDown);

static u32 msdc_ldo_power(u32 on, unsigned int powerId, int voltage_uv,
							u32 *status)
{
	if (on) {		/* want to power on */
		if (*status == 0) {	/* can power on */
			pr_warn("msdc LDO<%d> power on<%d>\n", powerId, voltage_uv);
			msdc_hwPowerOn(powerId, voltage_uv, "msdc");
			*status = voltage_uv;
		} else if (*status == voltage_uv) {
			pr_err("msdc LDO<%d><%d> power on again!\n",
				powerId, voltage_uv);
		} else {	/* for sd3.0 later */
			pr_warn("msdc LDO<%d> change<%d> to <%d>\n",
				powerId, *status, voltage_uv);
			msdc_hwPowerDown(powerId, "msdc");
			msdc_hwPowerOn(powerId, voltage_uv, "msdc");
			*status = voltage_uv;
		}
	} else {		/* want to power off */
		if (*status != 0) {	/* has been powerred on */
			pr_warn("msdc LDO<%d> power off\n", powerId);
			msdc_hwPowerDown(powerId, "msdc");
			*status = 0;
		} else
			pr_err("LDO<%d> not power on\n", powerId);
	}
	return 0;
}
#else
static u32 msdc_ldo_power(u32 on, MT65XX_POWER powerId, int voltage_uv,
							u32 *status)
{
	if (on) {
		if (*status == 0) {
			pr_warn("msdc LDO<%d> power on<%d>\n", powerId, voltage_uv);
			hwPowerOn(powerId, voltage_uv, "msdc");
			*status = voltage_uv;
		} else if (*status == voltage_uv) {
			pr_err("msdc LDO<%d><%d> power on again!\n", powerId, voltage_uv);
		} else { /* for sd3.0 later */
			pr_warn("msdc LDO<%d> change<%d> to <%d>\n",
				powerId, *status, voltage_uv);
			hwPowerDown(powerId, "msdc");
			hwPowerOn(powerId, voltage_uv, "msdc");
			*status = voltage_uv;
		}
	} else {
		if (*status != 0) {
			pr_warn("msdc LDO<%d> power off\n", powerId);
			hwPowerDown(powerId, "msdc");
			*status = 0;
		} else
			pr_err("LDO<%d> not power on\n", powerId);
	}

	return 0;
}
#endif

void msdc_sd_power_off(void)
{
	pr_err("SD overheat,pmic Eint disable SD power!\n");
#ifdef CONFIG_MTK_LEGACY
	msdc_ldo_power(0, MT6328_POWER_LDO_VMC, VOL_3000, &g_msdc1_io);
	msdc_ldo_power(0, MT6328_POWER_LDO_VMCH, VOL_3000, &g_msdc1_flash);
#endif
}

void msdc_set_smt(struct msdc_host *host, int set_smt)
{
	switch (host->id) {
	case 0:
		if (set_smt)
			sdr_set_field(MSDC0_GPIO_SMT_G5_ADDR, MSDC0_SMT_ALL_MASK, 0x1F);
		else
			sdr_set_field(MSDC0_GPIO_SMT_G5_ADDR, MSDC0_SMT_ALL_MASK, 0x0);
		break;
	case 1:
		if (set_smt)
			sdr_set_field(MSDC1_GPIO_SMT_G4_ADDR, MSDC1_SMT_ALL_MASK, 0x7);
		else
			sdr_set_field(MSDC1_GPIO_SMT_G4_ADDR, MSDC1_SMT_ALL_MASK, 0x0);
		break;
#ifdef CFG_DEV_MSDC2 /* FIXME: For 6630 */

	case 2:
		if (set_smt)
			sdr_set_field(MSDC2_GPIO_SMT_G0_ADDR, MSDC2_SMT_ALL_MASK, 0x7);
		else
			sdr_set_field(MSDC2_GPIO_SMT_G0_ADDR, MSDC2_SMT_ALL_MASK, 0x0);
		break;

#endif
#ifdef CFG_DEV_MSDC2
		case 3:
			break;
#endif

	default:
		pr_err("error...[%s] host->id out of range!!!\n", __func__);
		break;
	}
}

void msdc_set_tdsel(struct msdc_host *host, bool sleep)
{
	switch (host->id) {
	case 0:
		sdr_set_field(MSDC0_GPIO_TDSEL0_G5_ADDR, MSDC0_TDSEL_ALL_MASK, 0);
		break;
	case 1:
		if (sleep)
			sdr_set_field(MSDC1_GPIO_TDSEL0_G4_ADDR,
			MSDC1_TDSEL_ALL_MASK, 0xFFF);
		else
			sdr_set_field(MSDC1_GPIO_TDSEL0_G4_ADDR,
			MSDC1_TDSEL_ALL_MASK, 0xAAA);
		break;
#ifdef CFG_DEV_MSDC2 /* FIXME: For 6630 */

	case 2:
		sdr_set_field(MSDC2_GPIO_TDSEL0_G0_ADDR, MSDC2_TDSEL_ALL_MASK, 0);
		break;

#endif
	default:
		pr_err("error...[%s] host->id out of range!!!\n", __func__);
		break;
	}
}

void msdc_set_rdsel(struct msdc_host *host, bool sd_18)
{
	switch (host->id) {
	case 0:
		sdr_set_field(MSDC0_GPIO_RDSEL0_G5_ADDR, MSDC0_RDSEL_ALL_MASK, 0);
		break;
	case 1:
		if (sd_18)
			sdr_set_field(MSDC1_GPIO_RDSEL0_G4_ADDR, MSDC1_RDSEL_ALL_MASK, 0);
		else
			sdr_set_field(MSDC1_GPIO_RDSEL0_G4_ADDR,
			MSDC1_RDSEL_ALL_MASK, 0xC30C);
		break;
#ifdef CFG_DEV_MSDC2 /* FIXME: For 6630 */

	case 2:
		sdr_set_field(MSDC2_GPIO_RDSEL0_G0_ADDR, MSDC2_RDSEL_ALL_MASK, 0);
		break;

#endif
	default:
		pr_err("error...[%s] host->id out of range!!!\n", __func__);
		break;
	}
}

void msdc_set_rdtdsel_dbg(struct msdc_host *host, bool rdsel, u32 value)
{
	if (rdsel) {
		switch (host->id) {
		case 0:
			sdr_set_field(MSDC0_GPIO_RDSEL0_G5_ADDR,
				MSDC0_RDSEL_CMD_MASK, value);
			sdr_set_field(MSDC0_GPIO_RDSEL0_G5_ADDR,
				MSDC0_RDSEL_DSL_MASK, value);
			sdr_set_field(MSDC0_GPIO_RDSEL0_G5_ADDR,
				MSDC0_RDSEL_CLK_MASK, value);
			sdr_set_field(MSDC0_GPIO_RDSEL0_G5_ADDR,
				MSDC0_RDSEL_DAT_MASK, value);
			sdr_set_field(MSDC0_GPIO_RDSEL0_G5_ADDR,
				MSDC0_RDSEL_RSTB_MASK, value);
			break;
		case 1:
			sdr_set_field(MSDC1_GPIO_RDSEL0_G4_ADDR,
				MSDC1_RDSEL_CMD_MASK, value);
			sdr_set_field(MSDC1_GPIO_RDSEL0_G4_ADDR,
				MSDC1_RDSEL_CLK_MASK, value);
			sdr_set_field(MSDC1_GPIO_RDSEL0_G4_ADDR,
				MSDC1_RDSEL_DAT_MASK, value);
			break;
		case 2:
			sdr_set_field(MSDC2_GPIO_RDSEL0_G0_ADDR,
				MSDC2_RDSEL_CMD_MASK, value);
			sdr_set_field(MSDC2_GPIO_RDSEL0_G0_ADDR,
				MSDC2_RDSEL_CLK_MASK, value);
			sdr_set_field(MSDC2_GPIO_RDSEL0_G0_ADDR,
				MSDC2_RDSEL_DAT_MASK, value);
			break;
		}
	} else {
		switch (host->id) {
		case 0:
			sdr_set_field(MSDC0_GPIO_TDSEL0_G5_ADDR,
				MSDC0_TDSEL_CMD_MASK, value);
			sdr_set_field(MSDC0_GPIO_TDSEL0_G5_ADDR,
				MSDC0_TDSEL_DSL_MASK, value);
			sdr_set_field(MSDC0_GPIO_TDSEL0_G5_ADDR,
				MSDC0_TDSEL_CLK_MASK, value);
			sdr_set_field(MSDC0_GPIO_TDSEL0_G5_ADDR,
				MSDC0_TDSEL_DAT_MASK, value);
			sdr_set_field(MSDC0_GPIO_TDSEL0_G5_ADDR,
				MSDC0_TDSEL_RSTB_MASK, value);
			break;
		case 1:
			sdr_set_field(MSDC1_GPIO_TDSEL0_G4_ADDR,
				MSDC1_TDSEL_CMD_MASK, value);
			sdr_set_field(MSDC1_GPIO_TDSEL0_G4_ADDR,
				MSDC1_TDSEL_CLK_MASK, value);
			sdr_set_field(MSDC1_GPIO_TDSEL0_G4_ADDR,
				MSDC1_TDSEL_DAT_MASK, value);
			break;
		case 2:
			sdr_set_field(MSDC2_GPIO_TDSEL0_G0_ADDR,
				MSDC2_TDSEL_CMD_MASK, value);
			sdr_set_field(MSDC2_GPIO_TDSEL0_G0_ADDR,
				MSDC2_TDSEL_CLK_MASK, value);
			sdr_set_field(MSDC2_GPIO_TDSEL0_G0_ADDR,
				MSDC2_TDSEL_DAT_MASK, value);
			break;
		}
	}
}

void msdc_get_rdtdsel_dbg(struct msdc_host *host, bool rdsel, u32 *value)
{
	if (rdsel) {
		switch (host->id) {
		case 0:
			sdr_get_field(MSDC0_GPIO_RDSEL0_G5_ADDR,
				MSDC0_RDSEL_CMD_MASK, *value);
			break;
		case 1:
			sdr_get_field(MSDC1_GPIO_RDSEL0_G4_ADDR,
				MSDC1_RDSEL_CMD_MASK, *value);
			break;
		case 2:
			sdr_get_field(MSDC2_GPIO_RDSEL0_G0_ADDR,
				MSDC2_RDSEL_CMD_MASK, *value);
			break;
		}
	} else {
		switch (host->id) {
		case 0:
			sdr_get_field(MSDC0_GPIO_TDSEL0_G5_ADDR,
				MSDC0_TDSEL_CMD_MASK, *value);
			break;
		case 1:
			sdr_get_field(MSDC1_GPIO_TDSEL0_G4_ADDR,
				MSDC1_TDSEL_CMD_MASK, *value);
			break;
		case 2:
			sdr_get_field(MSDC2_GPIO_TDSEL0_G0_ADDR,
				MSDC2_TDSEL_CMD_MASK, *value);
			break;
		}
	}
}

void msdc_set_sr(struct msdc_host *host, int clk, int cmd, int dat, int rst,
				int ds)
{
	switch (host->id) {
	case 0:
		sdr_set_field(MSDC0_GPIO_DRV0_G5_ADDR, MSDC0_SR_CMD_MASK, (cmd != 0));
		sdr_set_field(MSDC0_GPIO_DRV0_G5_ADDR, MSDC0_SR_DSL_MASK, (ds != 0));
		sdr_set_field(MSDC0_GPIO_DRV0_G5_ADDR, MSDC0_SR_CLK_MASK, (clk != 0));
		sdr_set_field(MSDC0_GPIO_DRV0_G5_ADDR, MSDC0_SR_DAT_MASK, (dat != 0));
		sdr_set_field(MSDC0_GPIO_DRV0_G5_ADDR, MSDC0_SR_RSTB_MASK, (rst != 0));
		break;
	case 1:
		sdr_set_field(MSDC1_GPIO_DRV0_G4_ADDR, MSDC1_SR_CMD_MASK, (cmd != 0));
		sdr_set_field(MSDC1_GPIO_DRV0_G4_ADDR, MSDC1_SR_CLK_MASK, (clk != 0));
		sdr_set_field(MSDC1_GPIO_DRV0_G4_ADDR, MSDC1_SR_DAT_MASK, (dat != 0));
		break;
#ifdef CFG_DEV_MSDC2 /* FIXME: For 6630 */
	case 2:
		sdr_set_field(MSDC2_GPIO_DRV0_G0_ADDR, MSDC2_SR_CMD_MASK, (cmd != 0));
		sdr_set_field(MSDC2_GPIO_DRV0_G0_ADDR, MSDC2_SR_CLK_MASK, (clk != 0));
		sdr_set_field(MSDC2_GPIO_DRV0_G0_ADDR, MSDC2_SR_DAT_MASK, (dat != 0));
		break;

#endif
	default:
		pr_err("error...[%s] host->id out of range!!!\n", __func__);
		break;
	}

}

void msdc_set_driving(struct msdc_host *host, struct msdc_hw *hw, bool sd_18)
{
	switch (host->id) {
	case 0:
		sdr_set_field(MSDC0_GPIO_DRV0_G5_ADDR,
			MSDC0_DRV_CMD_MASK, hw->cmd_drv);
		sdr_set_field(MSDC0_GPIO_DRV0_G5_ADDR,
			MSDC0_DRV_DSL_MASK, hw->ds_drv);
		sdr_set_field(MSDC0_GPIO_DRV0_G5_ADDR,
			MSDC0_DRV_CLK_MASK, hw->clk_drv);
		sdr_set_field(MSDC0_GPIO_DRV0_G5_ADDR,
			MSDC0_DRV_DAT_MASK, hw->dat_drv);
		sdr_set_field(MSDC0_GPIO_DRV0_G5_ADDR,
			MSDC0_DRV_RSTB_MASK, hw->rst_drv);
		break;
	case 1:
		if (sd_18) {
			sdr_set_field(MSDC1_GPIO_DRV0_G4_ADDR,
				MSDC1_DRV_CMD_MASK, hw->cmd_drv_sd_18);
			sdr_set_field(MSDC1_GPIO_DRV0_G4_ADDR,
				MSDC1_DRV_CLK_MASK, hw->clk_drv_sd_18);
			sdr_set_field(MSDC1_GPIO_DRV0_G4_ADDR,
				MSDC1_DRV_DAT_MASK, hw->dat_drv_sd_18);
		} else {
			sdr_set_field(MSDC1_GPIO_DRV0_G4_ADDR,
				MSDC1_DRV_CMD_MASK, hw->cmd_drv);
			sdr_set_field(MSDC1_GPIO_DRV0_G4_ADDR,
				MSDC1_DRV_CLK_MASK, hw->clk_drv);
			sdr_set_field(MSDC1_GPIO_DRV0_G4_ADDR,
				MSDC1_DRV_DAT_MASK, hw->dat_drv);
		}
		break;
#ifdef CFG_DEV_MSDC2
	case 2:
		sdr_set_field(MSDC2_GPIO_DRV0_G0_ADDR,
			MSDC2_DRV_CMD_MASK, hw->cmd_drv);
		sdr_set_field(MSDC2_GPIO_DRV0_G0_ADDR,
			MSDC2_DRV_CLK_MASK, hw->clk_drv);
		sdr_set_field(MSDC2_GPIO_DRV0_G0_ADDR,
			MSDC2_DRV_DAT_MASK, hw->dat_drv);
		break;

#endif
#ifdef CFG_DEV_MSDC3
	case 3:
		sdr_set_field(MSDC3_GPIO_CLK_BASE, GPIO_MSDC_DRV_MASK, hw->clk_drv);
		sdr_set_field(MSDC3_GPIO_CMD_BASE, GPIO_MSDC_DRV_MASK, hw->cmd_drv);
		sdr_set_field(MSDC3_GPIO_DAT_BASE, GPIO_MSDC_DRV_MASK, hw->dat_drv);
		break;
#endif
	default:
		pr_err("error...[%s] host->id out of range!!!\n", __func__);
		break;
	}
}

/* weiping fix */
#if 0
void msdc_get_driving(struct msdc_host *host, struct msdc_ioctl *msdc_ctl)
{
/* for try submit */
	switch (host->id) {
	case 0:
		sdr_get_field(MSDC0_GPIO_DRV0_G5_ADDR,
			MSDC0_DRV_CMD_MASK, msdc_ctl->cmd_pu_driving);
		sdr_get_field(MSDC0_GPIO_DRV0_G5_ADDR,
			MSDC0_DRV_DSL_MASK, msdc_ctl->ds_pu_driving);
		sdr_get_field(MSDC0_GPIO_DRV0_G5_ADDR,
			MSDC0_DRV_CLK_MASK, msdc_ctl->clk_pu_driving);
		sdr_get_field(MSDC0_GPIO_DRV0_G5_ADDR,
			MSDC0_DRV_DAT_MASK, msdc_ctl->dat_pu_driving);
		sdr_get_field(MSDC0_GPIO_DRV0_G5_ADDR,
			MSDC0_DRV_RSTB_MASK, msdc_ctl->rst_pu_driving);
		break;
	case 1:
		sdr_get_field(MSDC1_GPIO_DRV0_G4_ADDR,
			MSDC1_DRV_CMD_MASK, msdc_ctl->cmd_pu_driving);
		sdr_get_field(MSDC1_GPIO_DRV0_G4_ADDR,
			MSDC1_DRV_CLK_MASK, msdc_ctl->clk_pu_driving);
		sdr_get_field(MSDC1_GPIO_DRV0_G4_ADDR,
			MSDC1_DRV_DAT_MASK, msdc_ctl->dat_pu_driving);
		msdc_ctl->rst_pu_driving = 0;
		msdc_ctl->ds_pu_driving = 0;
		break;
#ifdef CFG_DEV_MSDC2 /* FIXME: For 6630 */
	case 2:
		sdr_get_field(MSDC2_GPIO_DRV0_G0_ADDR,
			MSDC2_DRV_CMD_MASK, msdc_ctl->cmd_pu_driving);
		sdr_get_field(MSDC2_GPIO_DRV0_G0_ADDR,
			MSDC2_DRV_CLK_MASK, msdc_ctl->clk_pu_driving);
		sdr_get_field(MSDC2_GPIO_DRV0_G0_ADDR,
			MSDC2_DRV_DAT_MASK, msdc_ctl->dat_pu_driving);
		msdc_ctl->rst_pu_driving = 0;
		msdc_ctl->ds_pu_driving = 0;
		break;
#endif
	default:
		pr_err("error...[%s] host->id out of range!!!\n", __func__);
		break;
	}
}
#endif
static void msdc_pin_pud(struct msdc_host *host, u32 mode)
{
	switch (host->id) {
	case 0:
		/*
		 * High-Z
		 * cmd/clk/dat/(rstb)/dsl:pd-50k
		 * clk/dsl:pd-50k, cmd/dat:pu-10k, (rstb:pu-50k)
		 */
		if (MSDC_PIN_PULL_NONE == mode) {
			sdr_set_field(MSDC0_GPIO_PUPD0_G5_ADDR,
				MSDC0_PUPD_CMD_DSL_CLK_DAT04_MASK, 0x44444444);
			sdr_set_field(MSDC0_GPIO_PUPD1_G5_ADDR,
				MSDC0_PUPD_DAT567_MASK, 0x444);
		} else if (MSDC_PIN_PULL_DOWN == mode) {
			sdr_set_field(MSDC0_GPIO_PUPD0_G5_ADDR,
				MSDC0_PUPD_CMD_DSL_CLK_DAT04_MASK, 0x66666666);
			sdr_set_field(MSDC0_GPIO_PUPD1_G5_ADDR,
				MSDC0_PUPD_DAT567_MASK, 0x666);

		} else if (MSDC_PIN_PULL_UP == mode) {
			sdr_set_field(MSDC0_GPIO_PUPD0_G5_ADDR,
				MSDC0_PUPD_CMD_DSL_CLK_DAT04_MASK, 0x11111661);
			sdr_set_field(MSDC0_GPIO_PUPD1_G5_ADDR,
				MSDC0_PUPD_DAT567_MASK, 0x111);
		}
		break;
	case 1:
		/*
		 * High-Z
		 * cmd/clk/dat:pd-50k
		 * cmd/dat:pu-50k, clk:pd-50k
		 */
		if (MSDC_PIN_PULL_NONE == mode)
			sdr_set_field(MSDC1_GPIO_PUPD0_G4_ADDR,
				MSDC1_PUPD_CMD_CLK_DAT_MASK, 0x444444);
		else if (MSDC_PIN_PULL_DOWN == mode)
			sdr_set_field(MSDC1_GPIO_PUPD0_G4_ADDR,
				MSDC1_PUPD_CMD_CLK_DAT_MASK, 0x666666);
		else if (MSDC_PIN_PULL_UP == mode)
			sdr_set_field(MSDC1_GPIO_PUPD0_G4_ADDR,
				MSDC1_PUPD_CMD_CLK_DAT_MASK, 0x222262);
		break;
#ifdef CFG_DEV_MSDC2
	case 2:
		if (MSDC_PIN_PULL_NONE == mode)
			sdr_set_field(MSDC2_GPIO_PUPD0_G0_ADDR,
				MSDC2_PUPD_CMD_CLK_DAT_MASK, 0x444444);
		else if (MSDC_PIN_PULL_DOWN == mode)
			sdr_set_field(MSDC2_GPIO_PUPD0_G0_ADDR,
				MSDC2_PUPD_CMD_CLK_DAT_MASK, 0x666666);
		else if (MSDC_PIN_PULL_UP == mode)
			sdr_set_field(MSDC2_GPIO_PUPD0_G0_ADDR,
				MSDC2_PUPD_CMD_CLK_DAT_MASK, 0x222262);
		break;

#endif
	default:
		pr_err("error...[%s] host->id out of range!!!\n", __func__);
		break;
	}

}

#ifndef CONFIG_MTK_LEGACY
static void msdc_emmc_power(struct msdc_host *host, u32 on)
{
	unsigned long tmo = 0;
	void __iomem *base = host->base;

	/* if MMC_CAP_WAIT_WHILE_BUSY not set,
	 * mmc core layer will loop for wait sa_timeout
	 */
	if (host->mmc && host->mmc->card
		&& (host->mmc->caps & MMC_CAP_WAIT_WHILE_BUSY) && (on == 0)) {
		/* max timeout: 1000ms */
		if ((DIV_ROUND_UP(host->mmc->card->ext_csd.sa_timeout, 10000)) < 1000)
			tmo = jiffies + DIV_ROUND_UP(host->mmc->card->ext_csd.sa_timeout,
				10000000 / HZ) + HZ / 100;
		else
			tmo = jiffies + HZ;

		while ((sdr_read32(MSDC_PS) & 0x10000) != 0x10000) {
			if (time_after(jiffies, tmo)) {
				ERR_MSG("Dat0 keep low before power off, sa_timeout = 0x%x",
					host->mmc->card->ext_csd.sa_timeout);
				emmc_sleep_failed = 1;
				break;
			}
		}
	}

	/* Set to 3.0V - 100mV
	 * 4'b0000: 0 mV
	 * 4'b0001: -20 mV
	 * 4'b0010: -40 mV
	 * 4'b0011: -60 mV
	 * 4'b0100: -80 mV
	 * 4'b0101: -100 mV
	 */
	msdc_ldo_power(on, POWER_LDO_VEMC_3V3, VOL_3000, &g_msdc0_flash);
	/* mt6325_upmu_set_rg_vemc_3v3_cal(0x5); */

	msdc_dump_ldo_sts(host);
}

static void msdc_sd_power(struct msdc_host *host, u32 on)
{
	switch (host->id) {
	case 1:
		msdc_set_driving(host, host->hw, 0);
		msdc_set_rdsel(host, 0);
		if (host->hw->flags & MSDC_SD_NEED_POWER)
			msdc_ldo_power(1, POWER_LDO_VMCH, VOL_3000, &g_msdc1_flash);
		else
			msdc_ldo_power(on, POWER_LDO_VMCH, VOL_3000, &g_msdc1_flash);
		msdc_ldo_power(on, POWER_LDO_VMC, VOL_3000, &g_msdc1_io);
		break;
	case 2:
		msdc_set_driving(host, host->hw, 0);
		msdc_set_rdsel(host, 0);
		msdc_ldo_power(on, POWER_LDO_VMC, VOL_3000, &g_msdc2_io);
		msdc_ldo_power(on, POWER_LDO_VMCH, VOL_3000, &g_msdc2_flash);
		break;
	default:
		break;
	}
	msdc_dump_ldo_sts(host);
}

static void msdc_sd_power_switch(struct msdc_host *host, u32 on)
{
	switch (host->id) {
	case 1:
		msdc_ldo_power(on, POWER_LDO_VMC, VOL_1800, &g_msdc1_io);
		msdc_set_rdsel(host, 1);
		msdc_set_driving(host, host->hw, 1);
		break;
	case 2:
		msdc_ldo_power(on, POWER_LDO_VMC, VOL_1800, &g_msdc2_io);
		msdc_set_rdsel(host, 1);
		msdc_set_driving(host, host->hw, 1);
		break;
	default:
		break;
	}
}
#else
static void msdc_emmc_power(struct msdc_host *host, u32 on)
{
	unsigned long tmo = 0;
	void __iomem *base = host->base;

	/* if MMC_CAP_WAIT_WHILE_BUSY not set,
	 * mmc core layer will loop for wait sa_timeout
	 */
	if (host->mmc && host->mmc->card
		&& (host->mmc->caps & MMC_CAP_WAIT_WHILE_BUSY) && (on == 0)) {
		/* max timeout: 1000ms */
		if ((DIV_ROUND_UP(host->mmc->card->ext_csd.sa_timeout, 10000)) < 1000)
			tmo = jiffies + DIV_ROUND_UP(host->mmc->card->ext_csd.sa_timeout,
				10000000 / HZ) + HZ / 100;
		else
			tmo = jiffies + HZ;

		while ((sdr_read32(MSDC_PS) & 0x10000) != 0x10000) {
			if (time_after(jiffies, tmo)) {
				ERR_MSG("Dat0 keep low before power off, sa_timeout = 0x%x",
					host->mmc->card->ext_csd.sa_timeout);
				emmc_sleep_failed = 1;
				break;
			}
		}
	}

	/* Set to 3.0V - 100mV
	 * 4'b0000: 0 mV
	 * 4'b0001: -20 mV
	 * 4'b0010: -40 mV
	 * 4'b0011: -60 mV
	 * 4'b0100: -80 mV
	 * 4'b0101: -100 mV
	 */
	msdc_ldo_power(on, MT6328_POWER_LDO_VEMC33, VOL_3000, &g_msdc0_flash);
	msdc_dump_ldo_sts(host);
}

static void msdc_sd_power(struct msdc_host *host, u32 on)
{
	switch (host->id) {
	case 1:
		msdc_set_driving(host, host->hw, 0);
		msdc_set_rdsel(host, 0);
		if (host->hw->flags & MSDC_SD_NEED_POWER)
			msdc_ldo_power(1, MT6328_POWER_LDO_VMCH, VOL_3300, &g_msdc1_flash);
		else
			msdc_ldo_power(on, MT6328_POWER_LDO_VMCH, VOL_3300, &g_msdc1_flash);
		msdc_ldo_power(on, MT6328_POWER_LDO_VMC, VOL_3300, &g_msdc1_io);
		if (on)
			upmu_set_rg_vmc_184(0);
		break;
	case 2:
		msdc_set_driving(host, host->hw, 0);
		msdc_set_rdsel(host, 0);
		msdc_ldo_power(on, MT6328_POWER_LDO_VMC, VOL_3300, &g_msdc2_io);
		msdc_ldo_power(on, MT6328_POWER_LDO_VMCH, VOL_3300, &g_msdc2_flash);
		break;
	default:
		break;
	}

	msdc_dump_ldo_sts(host);
}

static void msdc_sd_power_switch(struct msdc_host *host, u32 on)
{
	switch (host->id) {
	case 1:
		msdc_ldo_power(on, MT6328_POWER_LDO_VMC, VOL_1800, &g_msdc1_io);
		if (on)
			upmu_set_rg_vmc_184(1);
		msdc_set_rdsel(host, 1);
		msdc_set_driving(host, host->hw, 1);
		break;
	case 2:
		msdc_ldo_power(on, MT6328_POWER_LDO_VMC, VOL_1800, &g_msdc2_io);
		msdc_set_rdsel(host, 1);
		msdc_set_driving(host, host->hw, 1);
		break;
	default:
		break;
	}
}
#endif
static void msdc_sdio_power(struct msdc_host *host, u32 on)
{
	switch (host->id) {
#if defined(CFG_DEV_MSDC2)
	case 2:
		if (MSDC_VIO18_MC2 == host->power_domain) {
			if (on) {
#if 0 /* CHECK ME */ /* Bus & device keeps 1.8v */
				msdc_ldo_power(on, MT_POWER_LDO_VGP6, VOL_1800, &g_msdc2_io);
#endif
			} else {
#if 0 /* CHECK ME */ /* Bus & device keeps 3.3v */
				msdc_ldo_power(on, MT_POWER_LDO_VGP6, VOL_3000, &g_msdc2_io);
#endif
			}

		} else if (MSDC_VIO28_MC2 == host->power_domain) {
			/* Bus & device keeps 2.8v */
			/*msdc_ldo_power(on, MT_POWER_LDO_VIO28, VOL_2800, &g_msdc2_io); */
		}
		g_msdc2_flash = g_msdc2_io;
		break;
#endif

#if defined(CFG_DEV_MSDC3)
	case 3:
		break;
#endif

	default:
	/*if host_id is 3, it uses default 1.8v setting, which always turns on */
		break;
	}

}
#endif

static void msdc_reset_pwr_cycle_counter(struct msdc_host *host)
{
	host->power_cycle = 0;
	host->power_cycle_enable = 1;
}

#define CMD_TUNE_CNT	(0)
#define READ_TUNE_CNT	(1)
#define WRITE_TUNE_CNT	(2)
#define ALL_TUNE_CNT	(3)

static void msdc_reset_tmo_tune_counter(struct msdc_host *host,	int index)
{
	if (index >= 0 && index <= ALL_TUNE_CNT) {
		switch (index) {
		case CMD_TUNE_CNT:
			if (host->rwcmd_time_tune != 0)
				ERR_MSG("TMO TUNE CMD Times(%d)", host->rwcmd_time_tune);
			host->rwcmd_time_tune = 0;
			break;
		case READ_TUNE_CNT:
			if (host->read_time_tune != 0)
				ERR_MSG("TMO TUNE READ Times(%d)", host->read_time_tune);
			host->read_time_tune = 0;
			break;
		case WRITE_TUNE_CNT:
			if (host->write_time_tune != 0)
				ERR_MSG("TMO TUNE WRITE Times(%d)", host->write_time_tune);
			host->write_time_tune = 0;
			break;
		case ALL_TUNE_CNT:
			if (host->rwcmd_time_tune != 0)
				ERR_MSG("TMO TUNE CMD Times(%d)", host->rwcmd_time_tune);
			if (host->read_time_tune != 0)
				ERR_MSG("TMO TUNE READ Times(%d)", host->read_time_tune);
			if (host->write_time_tune != 0)
				ERR_MSG("TMO TUNE WRITE Times(%d)", host->write_time_tune);
			host->rwcmd_time_tune = 0;
			host->read_time_tune = 0;
			host->write_time_tune = 0;
			break;
		default:
			break;
		}
	} else {
		ERR_MSG("msdc%d ==> reset tmo counter index(%d) error!\n",
			host->id, index);
	}
}

static void msdc_reset_crc_tune_counter(struct msdc_host *host,	int index)
{
	void __iomem *base = host->base;

	if (index >= 0 && index <= ALL_TUNE_CNT) {
		switch (index) {
		case CMD_TUNE_CNT:
			if (host->t_counter.time_cmd != 0) {
				ERR_MSG("CRC TUNE CMD Times(%d)", host->t_counter.time_cmd);
				if (g_ett_tune)
					g_ett_cmd_tune = host->t_counter.time_cmd;
			}
			host->t_counter.time_cmd = 0;
			break;
		case READ_TUNE_CNT:
			if (host->t_counter.time_read != 0) {
				ERR_MSG("CRC TUNE READ Times(%d)", host->t_counter.time_read);
				if (g_ett_tune)
					g_ett_read_tune = host->t_counter.time_read;
			}
			host->t_counter.time_read = 0;
			break;
		case WRITE_TUNE_CNT:
			if (host->t_counter.time_write != 0) {
				ERR_MSG("CRC TUNE WRITE Times(%d)", host->t_counter.time_write);
				if (g_ett_tune)
					g_ett_write_tune = host->t_counter.time_write;
			}
			host->t_counter.time_write = 0;
			break;
		case ALL_TUNE_CNT:
			if (host->t_counter.time_cmd != 0) {
				ERR_MSG("CRC TUNE CMD Times(%d)", host->t_counter.time_cmd);
				if (g_ett_tune)
					g_ett_cmd_tune = host->t_counter.time_cmd;
			}
			if (host->t_counter.time_read != 0) {
				ERR_MSG("CRC TUNE READ Times(%d)", host->t_counter.time_read);
				if (g_ett_tune)
					g_ett_read_tune = host->t_counter.time_read;
			}
			if (host->t_counter.time_write != 0) {
				ERR_MSG("CRC TUNE WRITE Times(%d)", host->t_counter.time_write);
				if (g_ett_tune)
					g_ett_write_tune = host->t_counter.time_write;
			}
			host->t_counter.time_cmd = 0;
			host->t_counter.time_read = 0;
			host->t_counter.time_write = 0;
			if (host->t_counter.time_hs400 != 0) {
				if (g_reset_tune) {
					sdr_set_field(EMMC50_PAD_DS_TUNE,
						MSDC_EMMC50_PAD_DS_TUNE_DLY1, 0x1c);
					sdr_set_field(EMMC50_PAD_DS_TUNE,
						MSDC_EMMC50_PAD_DS_TUNE_DLY3, 0xe);
				}
				ERR_MSG("TUNE HS400 Times(%d)", host->t_counter.time_hs400);
				if (g_ett_tune)
					g_ett_hs400_tune = host->t_counter.time_hs400;
			}
			host->t_counter.time_hs400 = 0;
			break;
		default:
			break;
		}
	} else {
		ERR_MSG("msdc%d ==> reset crc counter index(%d) error!\n",
			host->id, index);
	}
}

#if 0
static void msdc_set_bad_card_and_remove(struct msdc_host *host)
{
	int got_polarity = 0;
	unsigned long flags;

	if (host == NULL) {
		ERR_MSG("WARN: host is NULL");
		return;
	}
	host->card_inserted = 0;

	if ((host->mmc == NULL) || (host->mmc->card == NULL)) {
		ERR_MSG("WARN: mmc or card is NULL");
		return;
	}
	if (host->mmc->card) {
		spin_lock_irqsave(&host->remove_bad_card, flags);
		got_polarity = host->sd_cd_polarity;
		host->block_bad_card = 1;

		mmc_card_set_removed(host->mmc->card);
		spin_unlock_irqrestore(&host->remove_bad_card, flags);

		if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE)
			&& (got_polarity ^ host->hw->cd_level))
			tasklet_hi_schedule(&host->card_tasklet);
		else {
			mmc_remove_card(host->mmc->card);
			host->mmc->card = NULL;
			mmc_detach_bus(host->mmc);
			mmc_power_off(host->mmc);
		}

		ERR_MSG("remove the bad card, block_bad_card=%d,card_inserted=%d",
			host->block_bad_card, host->card_inserted);
	}
}
#endif

/* host doesn't need the clock on */
void msdc_gate_clock(struct msdc_host *host, int delay)
{
	unsigned long flags;

	spin_lock_irqsave(&host->clk_gate_lock, flags);
	if (host->clk_gate_count > 0)
		host->clk_gate_count--;
	if (delay) {
		mod_timer(&host->timer, jiffies + CLK_TIMEOUT);
	} else if (host->clk_gate_count == 0) {
		del_timer(&host->timer);
		msdc_clksrc_onoff(host, 0);
	} else {
		if (is_card_sdio(host))
			host->error = -EBUSY;
	}
	spin_unlock_irqrestore(&host->clk_gate_lock, flags);
}

static void msdc_suspend_clock(struct msdc_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->clk_gate_lock, flags);
	if (host->clk_gate_count == 0) {
		del_timer(&host->timer);
		msdc_clksrc_onoff(host, 0);
		N_MSG(CLK, "[%s]: msdc%d, successfully gate clock,clk_gate_count=%d",
			__func__, host->id, host->clk_gate_count);
	} else {
		if (is_card_sdio(host))
			host->error = -EBUSY;
		ERR_MSG("[%s]:msdc%d,clock is still needed by host,clk_gate_count=%d",
			__func__, host->id, host->clk_gate_count);
	}
	spin_unlock_irqrestore(&host->clk_gate_lock, flags);
}

/* host does need the clock on */
void msdc_ungate_clock(struct msdc_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->clk_gate_lock, flags);
	host->clk_gate_count++;
	if (host->clk_gate_count == 1)
		msdc_clksrc_onoff(host, 1);
	spin_unlock_irqrestore(&host->clk_gate_lock, flags);
}

/* do we need sync object or not */
void msdc_clk_status(int *status)
{
	int g_clk_gate = 0;
	int i = 0;
	unsigned long flags;

	for (i = 0; i < HOST_MAX_NUM; i++) {
		if (!mtk_msdc_host[i])
			continue;

		spin_lock_irqsave(&mtk_msdc_host[i]->clk_gate_lock, flags);
		if (mtk_msdc_host[i]->clk_gate_count > 0)
#ifndef FPGA_PLATFORM
#ifdef CONFIG_MTK_CLKMGR
			g_clk_gate |= 1 << ((i) + MT_CG_PERI_MSDC30_0);
#endif
#endif
		spin_unlock_irqrestore(&mtk_msdc_host[i]->clk_gate_lock, flags);
	}
	*status = g_clk_gate;
}

#if 0
static void msdc_dump_card_status(struct msdc_host *host, u32 status)
{
	static const char * const state[] = {
		"Idle",		/* 0 */
		"Ready",	/* 1 */
		"Ident",	/* 2 */
		"Stby",		/* 3 */
		"Tran",		/* 4 */
		"Data",		/* 5 */
		"Rcv",		/* 6 */
		"Prg",		/* 7 */
		"Dis",		/* 8 */
		"Reserved",	/* 9 */
		"Reserved",	/* 10 */
		"Reserved",	/* 11 */
		"Reserved",	/* 12 */
		"Reserved",	/* 13 */
		"Reserved",	/* 14 */
		"I/O mode",	/* 15 */
	};
	if (status & R1_OUT_OF_RANGE)
		N_MSG(RSP, "[CARD_STATUS] Out of Range");
	if (status & R1_ADDRESS_ERROR)
		N_MSG(RSP, "[CARD_STATUS] Address Error");
	if (status & R1_BLOCK_LEN_ERROR)
		N_MSG(RSP, "[CARD_STATUS] Block Len Error");
	if (status & R1_ERASE_SEQ_ERROR)
		N_MSG(RSP, "[CARD_STATUS] Erase Seq Error");
	if (status & R1_ERASE_PARAM)
		N_MSG(RSP, "[CARD_STATUS] Erase Param");
	if (status & R1_WP_VIOLATION)
		N_MSG(RSP, "[CARD_STATUS] WP Violation");
	if (status & R1_CARD_IS_LOCKED)
		N_MSG(RSP, "[CARD_STATUS] Card is Locked");
	if (status & R1_LOCK_UNLOCK_FAILED)
		N_MSG(RSP, "[CARD_STATUS] Lock/Unlock Failed");
	if (status & R1_COM_CRC_ERROR)
		N_MSG(RSP, "[CARD_STATUS] Command CRC Error");
	if (status & R1_ILLEGAL_COMMAND)
		N_MSG(RSP, "[CARD_STATUS] Illegal Command");
	if (status & R1_CARD_ECC_FAILED)
		N_MSG(RSP, "[CARD_STATUS] Card ECC Failed");
	if (status & R1_CC_ERROR)
		N_MSG(RSP, "[CARD_STATUS] CC Error");
	if (status & R1_ERROR)
		N_MSG(RSP, "[CARD_STATUS] Error");
	if (status & R1_UNDERRUN)
		N_MSG(RSP, "[CARD_STATUS] Underrun");
	if (status & R1_OVERRUN)
		N_MSG(RSP, "[CARD_STATUS] Overrun");
	if (status & R1_CID_CSD_OVERWRITE)
		N_MSG(RSP, "[CARD_STATUS] CID/CSD Overwrite");
	if (status & R1_WP_ERASE_SKIP)
		N_MSG(RSP, "[CARD_STATUS] WP Eraser Skip");
	if (status & R1_CARD_ECC_DISABLED)
		N_MSG(RSP, "[CARD_STATUS] Card ECC Disabled");
	if (status & R1_ERASE_RESET)
		N_MSG(RSP, "[CARD_STATUS] Erase Reset");
	if ((status & R1_READY_FOR_DATA) == 0)
		N_MSG(RSP, "[CARD_STATUS] Not Ready for Data");
	if (status & R1_SWITCH_ERROR)
		N_MSG(RSP, "[CARD_STATUS] Switch error");
	if (status & R1_APP_CMD)
		N_MSG(RSP, "[CARD_STATUS] App Command");

	N_MSG(RSP, "[CARD_STATUS] '%s' State", state[R1_CURRENT_STATE(status)]);
}
#endif

static void msdc_set_timeout(struct msdc_host *host, u32 ns, u32 clks)
{
	void __iomem *base = host->base;
	u32 timeout, clk_ns;
	u32 mode = 0;

	host->timeout_ns = ns;
	host->timeout_clks = clks;
	if (host->sclk == 0) {
		timeout = 0;
	} else {
		clk_ns = 1000000000UL / host->sclk;
		timeout = (ns + clk_ns - 1) / clk_ns + clks;
		/* in 1048576 sclk cycle unit */
		timeout = (timeout + (1 << 20) - 1) >> 20;
		sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, mode);
		/* DDR mode will double the clk cycles for data timeout */
		timeout = mode >= 2 ? timeout * 2 : timeout;
		timeout = timeout > 1 ? timeout - 1 : 0;
		timeout = timeout > 255 ? 255 : timeout;
	}
	sdr_set_field(SDC_CFG, SDC_CFG_DTOC, timeout);

	pr_debug("msdc%d set read dattimeout:%dns,%d clk,MSDC_CFG=0x%x,freq=%dKHZ"
		, host->id, ns, clks, sdr_read32(MSDC_CFG), (host->sclk / 1000));
}

/* msdc_eirq_sdio() will be called when EIRQ(for WIFI) */
static void msdc_eirq_sdio(void *data)
{
	struct msdc_host *host = (struct msdc_host *)data;

	N_MSG(INT, "SDIO EINT");
#ifdef SDIO_ERROR_BYPASS
	if (host->sdio_error != -EIO) {
#endif
		mmc_signal_sdio_irq(host->mmc);
#ifdef SDIO_ERROR_BYPASS
	}
#endif
}

#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT

static void sdio_unreq_vcore(struct work_struct *work)
{
	pr_warn("** sdio_unreq_vcore() irqs_disabled():%d\n", irqs_disabled());
	might_sleep();
	/* mmc_claim_host(host->mmc); */
#if 0
	if (vcorefs_request_dvfs_opp(KIR_SDIO, OPPI_UNREQ) == 0) {
		pr_debug("unrequest vcore pass\n");
		host->sdio_performance_vcore = 0;
	} else {
		pr_err("unrequest vcore fail\n");
	}
#endif
	/* mmc_release_host(host->mmc); */
}

static noinline void sdio_set_vcore_performance(struct msdc_host *host,
						u32 enable)
{

	if (atomic_read(&host->ot_work.ot_disable)) {
		/* TODO: also return here when clock rate is not 200MHz */
		pr_info("sdio_set_vcore_performance auto-K haven't done\n");
		return;
	}

	if (enable) {
		might_sleep();
		/* true if dwork was pending, false otherwise */
		if (cancel_delayed_work_sync(&(host->set_vcore_workq)) == 0) {
			pr_warn("** cancel @ FALSE\n");
#if 0
			if (vcorefs_request_dvfs_opp(KIR_SDIO, OPPI_PERF) == 0) {
				pr_debug("msdc%d -> request vcore pass\n", host->id);
				host->sdio_performance_vcore = 1;
			} else {
				pr_err("msdc%d -> request vcore fail\n", host->id);
			}
#endif
		}
	} else {
		schedule_delayed_work(&(host->set_vcore_workq), CLK_TIMEOUT);
	}
	/* mmc_release_host(host->mmc); */
}

#endif /* MTK_SDIO30_ONLINE_TUNING_SUPPORT */

static void msdc_select_clksrc(struct msdc_host *host, int clksrc)
{
#ifndef FPGA_PLATFORM
	char name[6];
#ifndef CONFIG_MTK_CLKMGR
	int ret;
	struct clk *clk;
#endif
	if (host->id == 0)
		hclks = hclks_msdc50;
	else
		hclks = hclks_msdc30;
#endif

	pr_err("[%s]: msdc%d change clk_src from %dKHz to %d:%dKHz\n",
		__func__, host->id, (host->hclk / 1000), clksrc,
		(hclks[clksrc] / 1000));

#ifndef FPGA_PLATFORM
	sprintf(name, "MSDC%d", host->id);
#ifdef CONFIG_MTK_CLKMGR
	clkmux_sel(MT_MUX_MSDC30_0 - host->id, clksrc, name);
#else
	if (host->id != 0) {
		pr_err("NOT Support msdc%d switch pll souce[%s]%d\n",
			host->id, __func__, __LINE__);
		return;
	}
	if (clksrc == MSDC50_CLKSRC_800MHZ)
		clk = g_msdc0_pll_800m;
	else if (clksrc == MSDC50_CLKSRC_400MHZ)
		clk = g_msdc0_pll_400m;
	else if (clksrc == MSDC50_CLKSRC_200MHZ)
		clk = g_msdc0_pll_200m;
	else {
		pr_err("NOT Support msdc%d switch pll souce[%s]%d\n",
			host->id, __func__, __LINE__);
		return;
	}

	clk_enable(g_msdc0_pll_sel);
	ret = clk_set_parent(g_msdc0_pll_sel, clk);
	if (ret)
		pr_err("XXX MSDC%d switch clk source ERROR...[%s]%d\n",
			host->id, __func__, __LINE__);
	clk_disable(g_msdc0_pll_sel);
#endif
#endif

	host->hclk = hclks[clksrc];
	host->hw->clk_src = clksrc;
}

void msdc_sdio_set_long_timing_delay_by_freq(struct msdc_host *host, u32 clock)
{
#ifdef CONFIG_SDIOAUTOK_SUPPORT
	void __iomem *base = host->base;

	if (clock >= 200000000) {
		sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR,
			host->hw->wdatcrctactr_sdr200);
		sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR,
			host->hw->cmdrtactr_sdr200);
		sdr_set_field(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
			host->hw->intdatlatcksel_sdr200);
		host->saved_para.cmd_resp_ta_cntr = host->hw->cmdrtactr_sdr200;
		host->saved_para.wrdat_crc_ta_cntr = host->hw->wdatcrctactr_sdr200;
		host->saved_para.int_dat_latch_ck_sel = host->hw->intdatlatcksel_sdr200;
	} else {
		sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR,
			host->hw->wdatcrctactr_sdr50);
		sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR,
			host->hw->cmdrtactr_sdr50);
		sdr_set_field(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
			host->hw->intdatlatcksel_sdr50);
		host->saved_para.cmd_resp_ta_cntr = host->hw->cmdrtactr_sdr50;
		host->saved_para.wrdat_crc_ta_cntr = host->hw->wdatcrctactr_sdr50;
		host->saved_para.int_dat_latch_ck_sel = host->hw->intdatlatcksel_sdr50;
	}
#endif
}

int sdio_autok_processed = 0;

static void msdc_set_mclk(struct msdc_host *host, unsigned char timing, u32 hz)
{
	struct msdc_hw *hw = host->hw;
	void __iomem *base = host->base;
	u32 mode;
	u32 flags;
	u32 div;
	u32 sclk;
	u32 hclk = host->hclk;
	u32 hs400_src = 0;
	u8 clksrc = hw->clk_src;

	if (!hz) {
		pr_err("msdc%d -> !!! Set<0 Hz>", host->id);
		if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ)) {
			host->saved_para.hz = hz;
#ifdef SDIO_ERROR_BYPASS
			host->sdio_error = 0;
#endif
		}
		host->mclk = 0;
		msdc_reset_hw(host->id);
		return;
	}
	if (host->hw->host_function == MSDC_SDIO
		&& hz >= 100 * 1000 * 1000 && sdio_autok_processed == 0) {
		hz = 50 * 1000 * 1000;
		msdc_sdio_set_long_timing_delay_by_freq(host, hz);
	}

	msdc_irq_save(flags);

	if (timing == MMC_TIMING_MMC_HS400) {
		mode = 0x3;	/* HS400 mode */
		if (clksrc == MSDC50_CLKSRC_400MHZ) {
			hs400_src = 1;
			div = 0;
			sclk = hclk / 2;
		} else {
			hs400_src = 0;
			if (hz >= (hclk >> 2)) {
				div = 0;	/* mean div = 1/4 */
				sclk = hclk >> 2;	/* sclk = clk / 4 */
			} else {
				div = (hclk + ((hz << 2) - 1)) / (hz << 2);
				sclk = (hclk >> 2) / div;
				div = (div >> 1);
			}
		}
	} else if ((timing == MMC_TIMING_UHS_DDR50)
		|| (timing == MMC_TIMING_MMC_DDR52)) {
		mode = 0x2;	/* ddr mode and use divisor */
		if (hz >= (hclk >> 2)) {
			div = 0;	/* mean div = 1/4 */
			sclk = hclk >> 2;	/* sclk = clk / 4 */
		} else {
			div = (hclk + ((hz << 2) - 1)) / (hz << 2);
			sclk = (hclk >> 2) / div;
			div = (div >> 1);
		}
	} else if (hz >= hclk) {
#ifdef FPGA_PLATFORM
		mode = 0x0;	/* FPGA doesn't support no divisor */
#else
		mode = 0x1;	/* no divisor */
#endif
		div = 0;
		sclk = hclk;
	} else {
		mode = 0x0;	/* use divisor */
		if (hz >= (hclk >> 1)) {
			div = 0;	/* mean div = 1/2 */
			sclk = hclk >> 1;	/* sclk = clk / 2 */
		} else {
			div = (hclk + ((hz << 2) - 1)) / (hz << 2);
			sclk = (hclk >> 2) / div;
		}
	}

	msdc_clk_stable(host, mode, div, hs400_src);

	host->sclk = sclk;

#if 0
	if (host->sclk > 100000000)
		sdr_clr_bits(MSDC_PATCH_BIT0, CKGEN_RX_SDClKO_SEL);
	else
		sdr_set_bits(MSDC_PATCH_BIT0, CKGEN_RX_SDClKO_SEL);
#endif
	/* need because clk changed */
	msdc_set_timeout(host, host->timeout_ns, host->timeout_clks);

	if (mode == 0x3) {
		msdc_set_smpl(host, 1, host->hw->cmd_edge, TYPE_CMD_RESP_EDGE, NULL);
		msdc_set_smpl(host, 1, host->hw->rdata_edge, TYPE_READ_DATA_EDGE, NULL);
		msdc_set_smpl(host, 1, host->hw->wdata_edge, TYPE_WRITE_CRC_EDGE, NULL);
	} else {
		msdc_set_smpl(host, 0, host->hw->cmd_edge, TYPE_CMD_RESP_EDGE, NULL);
		msdc_set_smpl(host, 0, host->hw->rdata_edge, TYPE_READ_DATA_EDGE, NULL);
		msdc_set_smpl(host, 0, host->hw->wdata_edge, TYPE_WRITE_CRC_EDGE, NULL);

	}

	pr_err("msdc%d Set<%dK> src:<%dK> sclk:<%dK> timing<%d> mode:%d div:%d cfg:0x%x\n",
	       host->id, hz / 1000, hclk / 1000, sclk / 1000, timing, mode, div, sdr_read32(MSDC_CFG));

	msdc_irq_restore(flags);
}

/* 0 means pass */
/* weiping fix power-tune */
static u32 msdc_power_tuning(struct msdc_host *host)
{
	return 0;
}
/* weiping fix power-tune */
#if 0
static u32 msdc_power_tuning(struct msdc_host *host)
{
	struct mmc_host *mmc = host->mmc;
	struct mmc_card *card;
	struct mmc_request *mrq;
	u32 power_cycle = 0;
	int read_timeout_tune = 0;
	int write_timeout_tune = 0;
	u32 rwcmd_timeout_tune = 0;
	u32 read_timeout_tune_uhs104 = 0;
	u32 write_timeout_tune_uhs104 = 0;
	u32 sw_timeout = 0;
	u32 ret = 1;
	u32 host_err = 0;
	void __iomem *base = host->base;

	if (!mmc)
		return 1;

	card = mmc->card;
	if (card == NULL) {
		ERR_MSG("mmc->card is NULL");
		return 1;
	}
	/* eMMC first */
#ifdef CONFIG_MTK_EMMC_SUPPORT
	if (mmc_card_mmc(card) && (host->hw->host_function == MSDC_EMMC))
		return 1;
#endif

	if ((host->sd_30_busy > 0) && (host->sd_30_busy <= MSDC_MAX_POWER_CYCLE))
		host->power_cycle_enable = 1;
	if (mmc_card_sd(card) && (host->hw->host_function == MSDC_SD)) {
		if ((host->power_cycle < MSDC_MAX_POWER_CYCLE)
			&& (host->power_cycle_enable)) {
			/* power cycle */
			ERR_MSG("the %d time, Power cycle start", host->power_cycle);
			spin_unlock(&host->lock);
#ifdef FPGA_PLATFORM
			hwPowerDown_fpga();
#else
			if (host->power_control)
				host->power_control(host, 0);
			else
				pr_err("[ERROR]msdc%d No power control callback!\n", host->id);
#endif
			mdelay(10);
#ifdef FPGA_PLATFORM
			hwPowerOn_fpga();
#else
			if (host->power_control)
				host->power_control(host, 1);
			else
				pr_err("[ERROR]msdc%d No power control callback!\n", host->id);
#endif

			spin_lock(&host->lock);
			sdr_get_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, host->hw->ddlsel);
			sdr_get_field(MSDC_IOCON, MSDC_IOCON_RSPL, host->hw->cmd_edge);
			sdr_get_field(MSDC_IOCON, MSDC_IOCON_R_D_SMPL, host->hw->rdata_edge);
			sdr_get_field(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, host->hw->wdata_edge);
			host->saved_para.pad_tune0 = sdr_read32(MSDC_PAD_TUNE0);
			host->saved_para.ddly0 = sdr_read32(MSDC_DAT_RDDLY0);
			host->saved_para.ddly1 = sdr_read32(MSDC_DAT_RDDLY1);
			sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR,
				host->saved_para.cmd_resp_ta_cntr);
			sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR,
				host->saved_para.wrdat_crc_ta_cntr);
			sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_GET_BUSY_MA,
				host->saved_para.write_busy_margin);
			sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_GET_CRC_MA,
				host->saved_para.write_crc_margin);
			if ((host->sclk > 100000000) && (host->power_cycle >= 1))
				mmc->caps &= ~MMC_CAP_UHS_SDR104;
			if (((host->sclk <= 100000000) && ((host->sclk > 50000000)
							   || (host->timing == MMC_TIMING_UHS_DDR50)))
			    && (host->power_cycle >= 1)) {
				mmc->caps &= ~(MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR104
					| MMC_CAP_UHS_DDR50);
			}

			msdc_host_mode[host->id] = mmc->caps;
			msdc_host_mode2[host->id] = mmc->caps2;

			/* clock should set to 260K */
			mmc->ios.clock = HOST_MIN_MCLK;
			mmc->ios.bus_width = MMC_BUS_WIDTH_1;
			mmc->ios.timing = MMC_TIMING_LEGACY;
			msdc_set_mclk(host, MMC_TIMING_LEGACY, HOST_MIN_MCLK);

			/* zone_temp = sd_debug_zone[1]; */
			/* sd_debug_zone[1] |= (DBG_EVT_NRW | DBG_EVT_RW); */

			/* re-init the card */
			mrq = host->mrq;
			host->mrq = NULL;
			power_cycle = host->power_cycle;
			host->power_cycle = MSDC_MAX_POWER_CYCLE;
			read_timeout_tune = host->read_time_tune;
			write_timeout_tune = host->write_time_tune;
			rwcmd_timeout_tune = host->rwcmd_time_tune;
			read_timeout_tune_uhs104 = host->read_timeout_uhs104;
			write_timeout_tune_uhs104 = host->write_timeout_uhs104;
			sw_timeout = host->sw_timeout;
			host_err = host->error;
			spin_unlock(&host->lock);
			ret = mmc_sd_power_cycle(mmc, card->ocr, card);
			spin_lock(&host->lock);
			host->mrq = mrq;
			host->power_cycle = power_cycle;
			host->read_time_tune = read_timeout_tune;
			host->write_time_tune = write_timeout_tune;
			host->rwcmd_time_tune = rwcmd_timeout_tune;
			if (host->sclk > 100000000) {
				host->write_timeout_uhs104 = write_timeout_tune_uhs104;
			} else {
				host->read_timeout_uhs104 = 0;
				host->write_timeout_uhs104 = 0;
			}
			host->sw_timeout = sw_timeout;
			host->error = host_err;
			if (!ret)
				host->power_cycle_enable = 0;
			ERR_MSG("the %d time, Power cycle Done, host->error(0x%x), ret(%d)",
				host->power_cycle, host->error, ret);
			(host->power_cycle)++;
		} else if (host->continuous_fail_request_count <
			MSDC_MAX_CONTINUOUS_FAIL_REQUEST_COUNT) {
			host->continuous_fail_request_count++;
			host->power_cycle = 0;
		} else {
			ERR_MSG("[%d] > max continue fail request count %d,remove bad card",
			     host->continuous_fail_request_count,
			     MSDC_MAX_CONTINUOUS_FAIL_REQUEST_COUNT);
			host->continuous_fail_request_count = 0;
			/*release the lock in request */
			spin_unlock(&host->lock);
			/*card removing will define a new lock inside */
			msdc_set_bad_card_and_remove(host);
			/*restore the lock in request entry */
			spin_lock(&host->lock);
		}
	}

	return ret;
}
#endif /* weiping fix power-tune */

static void msdc_send_stop(struct msdc_host *host)
{
	struct mmc_command stop = { 0 };
	struct mmc_request mrq = { 0 };
	u32 err = -1;

	stop.opcode = MMC_STOP_TRANSMISSION;
	stop.arg = 0;
	stop.flags = MMC_RSP_R1B | MMC_CMD_AC;

	mrq.cmd = &stop;
	stop.mrq = &mrq;
	stop.data = NULL;

	err = msdc_do_command(host, &stop, 0, CMD_TIMEOUT);
}

#if 0
static void msdc_remove_card(struct work_struct *work)
{
	struct msdc_host *host = container_of(work, struct msdc_host,
		remove_card.work);

	BUG_ON(!host);
	BUG_ON(!(host->mmc));
	ERR_MSG("Need remove card");
	if (host->mmc->card) {
		if (mmc_card_present(host->mmc->card)) {
			ERR_MSG("1.remove card");
			mmc_remove_card(host->mmc->card);
		} else {
			ERR_MSG("card was not present can not remove");
			host->block_bad_card = 0;
			host->card_inserted = 1;
			host->mmc->card->state &= ~MMC_CARD_REMOVED;
			return;
		}
		mmc_claim_host(host->mmc);
		ERR_MSG("2.detach bus");
		host->mmc->card = NULL;
		mmc_detach_bus(host->mmc);
		ERR_MSG("3.Power off");
		mmc_power_off(host->mmc);
		ERR_MSG("4.Gate clock");
		msdc_gate_clock(host, 0);
		mmc_release_host(host->mmc);
	}
	ERR_MSG("Card removed");
}
#endif
int msdc_reinit(struct msdc_host *host)
{
	struct mmc_host *mmc;
	struct mmc_card *card;
	/* struct mmc_request *mrq; */
	int ret = -1;
	u32 err = 0;
	u32 status = 0;
	unsigned long tmo = 12;

	if (!host) {
		ERR_MSG("msdc_host is NULL");
		return -1;
	}
	if (host->hw->host_function != MSDC_SD)
		return -1;

	mmc = host->mmc;
	if (!mmc) {
		ERR_MSG("mmc is NULL");
		return -1;
	}

	card = mmc->card;
	if (card == NULL)
		ERR_MSG("mmc->card is NULL");
	if (host->block_bad_card)
		ERR_MSG("Need block this bad SD card from re-initialization");

	if ((host->mmc->caps & MMC_CAP_NONREMOVABLE) && (host->block_bad_card == 0)) {
		/* power cycle */
		ERR_MSG("SD card Re-Init!");
		mmc_claim_host(host->mmc);
		ERR_MSG("SD card Re-Init get host!");
		spin_lock(&host->lock);
		ERR_MSG("SD card Re-Init get lock!");
		msdc_clksrc_onoff(host, 1);
		if (host->app_cmd_arg) {
			while ((err = msdc_get_card_status(mmc, host, &status))) {
				ERR_MSG("SD card Re-Init in get card status!err(%d)", err);
				if (err == (unsigned int)-EIO) {
					if (msdc_tune_cmdrsp(host)) {
						ERR_MSG("update cmd para failed");
						break;
					}
				} else {
					break;
				}
			}
			if (err == 0) {
				if (status == 0) {
					msdc_dump_info(host->id);
				} else {
					msdc_clksrc_onoff(host, 0);
					spin_unlock(&host->lock);
					mmc_release_host(host->mmc);
					ERR_MSG("SD Card is ready.");
					return 0;
				}
			}
		}
		msdc_clksrc_onoff(host, 0);
		ERR_MSG("Reinit start..");
		mmc->ios.clock = HOST_MIN_MCLK;
		mmc->ios.bus_width = MMC_BUS_WIDTH_1;
		mmc->ios.timing = MMC_TIMING_LEGACY;
		host->card_inserted = 1;
		msdc_clksrc_onoff(host, 1);
		msdc_set_mclk(host, MMC_TIMING_LEGACY, HOST_MIN_MCLK);
		msdc_clksrc_onoff(host, 0);
		spin_unlock(&host->lock);
		mmc_release_host(host->mmc);
		if (host->mmc->card) {
			mmc_remove_card(host->mmc->card);
			host->mmc->card = NULL;
			mmc_claim_host(host->mmc);
			mmc_detach_bus(host->mmc);
			mmc_release_host(host->mmc);
		}
		mmc_power_off(host->mmc);
		mmc_detect_change(host->mmc, 0);
		while (tmo) {
			if (host->mmc->card && mmc_card_present(host->mmc->card)) {
				ret = 0;
				break;
			}
			msleep(50);
			tmo--;
		}
		ERR_MSG("Reinit %s", ret == 0 ? "success" : "fail");
	}

	if ((!(host->mmc->caps & MMC_CAP_NONREMOVABLE)) && (host->mmc->card)
	    && mmc_card_present(host->mmc->card)
	    && (!mmc_card_removed(host->mmc->card))
	    && (host->block_bad_card == 0))
		ret = 0;

	return ret;
}

static u32 msdc_status_verify_case1(struct msdc_host *host,
				struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;
	unsigned long tmo = jiffies + POLLING_BUSY;

	while (state != 4) {	/* until status to "tran" */
		msdc_reset_hw(host->id);
		while ((err = msdc_get_card_status(mmc, host, &status))) {
			ERR_MSG("CMD13 ERR<%d>", err);
			if (err != (unsigned int)-EIO) {
				return msdc_power_tuning(host);
			} else if (msdc_tune_cmdrsp(host)) {
				ERR_MSG("update cmd para failed");
				return MSDC_VERIFY_ERROR;
			}
		}

		state = R1_CURRENT_STATE(status);
		ERR_MSG("check card state<%d>", state);
		if (state == 5 || state == 6) {
			ERR_MSG("state<%d> need cmd12 to stop", state);
			msdc_send_stop(host);	/* don't tuning */
		} else if (state == 7) {	/* busy in programing */
			ERR_MSG("state<%d> card is busy", state);
			spin_unlock(&host->lock);
			msleep(100);
			spin_lock(&host->lock);
		} else if (state != 4) {
			ERR_MSG("state<%d> ??? ", state);
			return msdc_power_tuning(host);
		}

		if (time_after(jiffies, tmo)) {
			ERR_MSG("abort timeout. Do power cycle");
			if ((host->hw->host_function == MSDC_SD)
			    && (host->sclk >= 100000000
			    || (host->timing == MMC_TIMING_UHS_DDR50)))
				host->sd_30_busy++;
			return msdc_power_tuning(host);
		}
	}

	msdc_reset_hw(host->id);
	return MSDC_VERIFY_NEED_TUNE;
}

static u32 msdc_status_verify_case2(struct msdc_host *host,
				struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;		/*0: can tune normaly; 1: err hapen; 2: tune pass; */
	struct mmc_card *card = host->mmc->card;

	while (1) {
		msdc_reset_hw(host->id);
		err = msdc_get_card_status(mmc, host, &status);
		if (!err) {
			break;
		} else if (err != (unsigned int)-EIO) {
			ERR_MSG("CMD13 ERR<%d>", err);
			return msdc_power_tuning(host);
		} else if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("update cmd para failed");
			return MSDC_VERIFY_ERROR;
		}
		ERR_MSG("CMD13 ERR<%d>", err);
	}
	state = R1_CURRENT_STATE(status);

	/*wether is right RCA */
	if (cmd->arg == card->rca << 16) {
		return (3 == state || 8 == state) ?
			MSDC_VERIFY_NEED_TUNE : MSDC_VERIFY_NEED_NOT_TUNE;
	} else {
		return (4 == state || 5 == state || 6 == state
			|| 7 == state) ? MSDC_VERIFY_NEED_TUNE : MSDC_VERIFY_NEED_NOT_TUNE;
	}
}

static u32 msdc_status_verify_case3(struct msdc_host *host,
				struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;		/*0: can tune normaly; 1: tune pass; */

	while (1) {
		msdc_reset_hw(host->id);
		err = msdc_get_card_status(mmc, host, &status);
		if (!err) {
			break;
		} else if (err != (unsigned int)-EIO) {
			ERR_MSG("CMD13 ERR<%d>", err);
			return msdc_power_tuning(host);
		} else if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("update cmd para failed");
			return MSDC_VERIFY_ERROR;
		}
		ERR_MSG("CMD13 ERR<%d>", err);
	}
	state = R1_CURRENT_STATE(status);
	return (5 == state || 6 == state) ?
		MSDC_VERIFY_NEED_TUNE : MSDC_VERIFY_NEED_NOT_TUNE;
}

static u32 msdc_status_verify_case4(struct msdc_host *host,
				struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;		/*0: can tune normaly; 1: tune pass; */

	if (cmd->arg && (0x1UL << 15))
		return MSDC_VERIFY_NEED_NOT_TUNE;

	while (1) {
		msdc_reset_hw(host->id);
		err = msdc_get_card_status(mmc, host, &status);
		if (!err) {
			break;
		} else if (err != (unsigned int)-EIO) {
			ERR_MSG("CMD13 ERR<%d>", err);
			break;
		} else if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("update cmd para failed");
			return MSDC_VERIFY_ERROR;
		}
		ERR_MSG("CMD13 ERR<%d>", err);
	}
	state = R1_CURRENT_STATE(status);
	return 3 == state ? MSDC_VERIFY_NEED_NOT_TUNE : MSDC_VERIFY_NEED_TUNE;
}

#if 0
static u32 msdc_status_verify_case5(struct msdc_host *host,
				struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;		/*0: can tune normaly; 1: tune pass; */
	struct mmc_card *card = host->mmc->card;
	struct mmc_command cmd_bus_test = { 0 };
	struct mmc_request mrq_bus_sest = { 0 };

	while ((err = msdc_get_card_status(mmc, host, &status))) {
		ERR_MSG("CMD13 ERR<%d>", err);
		if (err != (unsigned int)-EIO) {
			return msdc_power_tuning(host);
		} else if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("update cmd para failed");
			return MSDC_VERIFY_ERROR;
		}
	}
	state = R1_CURRENT_STATE(status);

	if (MMC_SEND_TUNING_BLOCK == cmd->opcode) {
		if (state == 9) {
			/* send cmd14 */
			/*u32 err = -1; */
			cmd_bus_test.opcode = MMC_BUS_TEST_R;
			cmd_bus_test.arg = 0;
			cmd_bus_test.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

			mrq_bus_sest.cmd = &cmd_bus_test;
			cmd_bus_test.mrq = &mrq_bus_sest;
			cmd_bus_test.data = NULL;
			msdc_do_command(host, &cmd_bus_test, 0, CMD_TIMEOUT);
		}
	} else {
		if (state == 4) {
			/* send cmd19 */
			/*u32 err = -1; */
			cmd_bus_test.opcode = MMC_BUS_TEST_W;
			cmd_bus_test.arg = 0;
			cmd_bus_test.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

			mrq_bus_sest.cmd = &cmd_bus_test;
			cmd_bus_test.mrq = &mrq_bus_sest;
			cmd_bus_test.data = NULL;
			msdc_do_command(host, &cmd_bus_test, 0, CMD_TIMEOUT);
		}
	}
	return MSDC_VERIFY_NEED_TUNE;
}
#endif

static u32 msdc_status_verify(struct msdc_host *host, struct mmc_command *cmd)
{
	/* card is not identify */
	if (!host->mmc || !host->mmc->card || !host->mmc->card->rca)
		return MSDC_VERIFY_NEED_TUNE;

	if (((host->hw->host_function == MSDC_EMMC)
		&& IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE1))
	    || ((host->hw->host_function == MSDC_SD)
	    && IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_SD_TUNE_CASE1))
	    || (host->app_cmd
	    && IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_APP_TUNE_CASE1))) {
		return msdc_status_verify_case1(host, cmd);
	} else if (IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE2)) {
		return msdc_status_verify_case2(host, cmd);
	} else if (IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE3)) {
		return msdc_status_verify_case3(host, cmd);
	} else if ((host->hw->host_function == MSDC_EMMC)
		&& IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE4)) {
		return msdc_status_verify_case4(host, cmd);
#if 0
	} else if ((host->hw->host_function == MSDC_EMMC)
		&& IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE5)) {
		return msdc_status_verify_case5(host, cmd);
#endif
	} else {
		return MSDC_VERIFY_NEED_TUNE;
	}

}

#ifndef FPGA_PLATFORM
static void msdc_pin_config(struct msdc_host *host, int mode)
{
	msdc_pin_pud(host, mode);
}
#else
static void msdc_pin_config(struct msdc_host *host, int mode)
{
}
#endif

static void msdc_pin_reset(struct msdc_host *host, int mode)
{
	struct msdc_hw *hw = (struct msdc_hw *)host->hw;
	void __iomem *base = host->base;
	int pull = (mode == MSDC_PIN_PULL_UP) ?
		MSDC_GPIO_PULL_UP : MSDC_GPIO_PULL_DOWN;

	/* Config reset pin */
	if (hw->flags & MSDC_RST_PIN_EN) {
		if (hw->config_gpio_pin)	/* NULL */
			hw->config_gpio_pin(MSDC_RST_PIN, pull);

		if (mode == MSDC_PIN_PULL_UP)
			sdr_clr_bits(EMMC_IOCON, EMMC_IOCON_BOOTRST);
		else
			sdr_set_bits(EMMC_IOCON, EMMC_IOCON_BOOTRST);
	}
}

#if 0
static void msdc_pin_reset_force(struct msdc_host *host, int mode)
{
	struct msdc_hw *hw = (struct msdc_hw *)host->hw;
	void __iomem *base = host->base;
	int pull = (mode == MSDC_PIN_PULL_UP) ?
		MSDC_GPIO_PULL_UP : MSDC_GPIO_PULL_DOWN;

	if (hw->config_gpio_pin)	/* NULL */
		hw->config_gpio_pin(MSDC_RST_PIN, pull);

	if (mode == MSDC_PIN_PULL_UP)
		sdr_clr_bits(EMMC_IOCON, EMMC_IOCON_BOOTRST);
	else
		sdr_set_bits(EMMC_IOCON, EMMC_IOCON_BOOTRST);
}
#endif

static void msdc_set_power_mode(struct msdc_host *host, u8 mode)
{
	N_MSG(CFG, "Set power mode(%d)", mode);
	if (host->power_mode == MMC_POWER_OFF && mode != MMC_POWER_OFF) {
		msdc_pin_reset(host, MSDC_PIN_PULL_UP);
		msdc_pin_config(host, MSDC_PIN_PULL_UP);

#ifdef FPGA_PLATFORM
		hwPowerOn_fpga();
#else
		if (host->power_control)
			host->power_control(host, 1);
		else
			ERR_MSG
			    ("No power control, host_function<0x%x> & Power_domain<%d>",
			     host->hw->host_function, host->power_domain);

#endif
		mdelay(10);
	} else if (host->power_mode != MMC_POWER_OFF && mode == MMC_POWER_OFF) {

		if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ)) {
			msdc_pin_config(host, MSDC_PIN_PULL_UP);
		} else {

#ifdef FPGA_PLATFORM
			hwPowerDown_fpga();
#else
			if (host->power_control)
				host->power_control(host, 0);
			else
				ERR_MSG("No power control,host_function<0x%x>&Power_domain<%d>"
					, host->hw->host_function, host->power_domain);
#endif
			msdc_pin_config(host, MSDC_PIN_PULL_DOWN);
		}
		mdelay(10);
		msdc_pin_reset(host, MSDC_PIN_PULL_DOWN);
	}
	host->power_mode = mode;
}

#ifdef MTK_EMMC_ETT_TO_DRIVER
static int msdc_ett_offline_to_driver(struct msdc_host *host)
{
	int ret = 1;		/* 1 means failed */
	int size = sizeof(g_mmcTable) / sizeof(mmcdev_info);
	int i, temp;
	void __iomem *base = host->base;
	u32 clkmode;
	int hs400 = 0;

	sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	hs400 = (clkmode == 3) ? 1 : 0;

	/* pr_err("msdc_ett_offline_to_driver size<%d>\n", size); */

	for (i = 0; i < size; i++) {
		/*
		 * pr_err("msdc <%d> <%s> <%s>\n", i, g_mmcTable[i].pro_name, pro_name);
		 */

		if ((g_mmcTable[i].m_id == m_id)
			&& (!strncmp(g_mmcTable[i].pro_name, pro_name, 6))) {
			pr_err("msdc ett index<%d>: <%d> <%d> <0x%x> <0x%x> <0x%x>\n", i,
			       g_mmcTable[i].r_smpl, g_mmcTable[i].d_smpl,
			       g_mmcTable[i].cmd_rxdly, g_mmcTable[i].rd_rxdly,
			       g_mmcTable[i].wr_rxdly);

			/* set to msdc0 */

			msdc_set_smpl(host, hs400, g_mmcTable[i].r_smpl,
				TYPE_CMD_RESP_EDGE, NULL);
			msdc_set_smpl(host, hs400, g_mmcTable[i].d_smpl,
				TYPE_READ_DATA_EDGE, NULL);

			sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY,
				g_mmcTable[i].cmd_rxdly);
			sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLY,
				g_mmcTable[i].rd_rxdly);
			sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY,
				g_mmcTable[i].wr_rxdly);

			temp = g_mmcTable[i].rd_rxdly;
			temp &= 0x1F;
			sdr_write32(MSDC_DAT_RDDLY0,
				(temp << 0 | temp << 8 | temp << 16 | temp << 24));
			sdr_write32(MSDC_DAT_RDDLY1,
				(temp << 0 | temp << 8 | temp << 16 | temp << 24));

			ret = 0;
			break;
		}
	}

	/* if (ret) pr_err("msdc failed to find\n"); */
	return ret;
}
#endif

static void msdc_clksrc_onoff(struct msdc_host *host, u32 on)
{
	void __iomem *base = host->base;
	u32 div, mode, hs400_src;

	if (on) {
		if (0 == host->core_clkon) {
#ifndef FPGA_PLATFORM
#ifdef CONFIG_MTK_CLKMGR
			if (enable_clock(MT_CG_PERI_MSDC30_0 + host->id, "SD")) {
				pr_err("msdc%d on clock failed ===> retry once\n", host->id);
				disable_clock(MT_CG_PERI_MSDC30_0 + host->id, "SD");
				enable_clock(MT_CG_PERI_MSDC30_0 + host->id, "SD");
			}
#else
			if (clk_enable(host->clock_control)) {
				pr_err("msdc%d on clock failed ===> retry once\n", host->id);
				clk_disable(host->clock_control);
				clk_enable(host->clock_control);
			}
#endif
#endif
			host->core_clkon = 1;
			udelay(10);

			sdr_set_field(MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);

			sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, mode);
			sdr_get_field(MSDC_CFG, MSDC_CFG_CKDIV, div);
			sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD_HS400, hs400_src);
			msdc_clk_stable(host, mode, div, hs400_src);
#if 0
			if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))
				/* mdelay(1000);   wait for WIFI stable. */
#ifndef FPGA_PLATFORM
			/* freq_meter(0xf, 0); */
#endif
#endif
		}
	} else {
		if (!((host->hw->flags & MSDC_SDIO_IRQ) && src_clk_control)) {
			if (1 == host->core_clkon) {
				sdr_set_field(MSDC_CFG, MSDC_CFG_MODE, MSDC_MS);

#ifndef FPGA_PLATFORM
#ifdef CONFIG_MTK_CLKMGR
				disable_clock(MT_CG_PERI_MSDC30_0 + host->id, "SD");
#else
				clk_disable(host->clock_control);
#endif
#endif
				host->core_clkon = 0;

#ifndef FPGA_PLATFORM
				/* freq_meter(0xf, 0); */
#endif
			}
		}
	}
}

/*
 * register as callback function of WIFI(combo_sdio_register_pm) .
 * can called by msdc_drv_suspend/resume too.
 */
#ifdef CONFIG_PM
static void msdc_save_emmc_setting(struct msdc_host *host)
{
	void __iomem *base = host->base;
	u32 clkmode;

	host->saved_para.hz = host->mclk;
	host->saved_para.sdc_cfg = sdr_read32(SDC_CFG);

	sdr_get_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, host->hw->ddlsel);

	sdr_get_field(MSDC_IOCON, MSDC_IOCON_RSPL, host->hw->cmd_edge);
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_R_D_SMPL, host->hw->rdata_edge);
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, host->hw->wdata_edge);
	host->saved_para.pad_tune0 = sdr_read32(MSDC_PAD_TUNE0);
	host->saved_para.ddly0 = sdr_read32(MSDC_DAT_RDDLY0);
	host->saved_para.ddly1 = sdr_read32(MSDC_DAT_RDDLY1);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR,
		host->saved_para.cmd_resp_ta_cntr);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR,
		host->saved_para.wrdat_crc_ta_cntr);
	sdr_get_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY1,
		host->saved_para.ds_dly1);
	sdr_get_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY3,
		host->saved_para.ds_dly3);
	host->saved_para.emmc50_pad_cmd_tune = sdr_read32(EMMC50_PAD_CMD_TUNE);
	sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	host->saved_para.timing = host->timing;
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_GET_BUSY_MA,
		host->saved_para.write_busy_margin);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_GET_CRC_MA,
		host->saved_para.write_crc_margin);

	sdr_get_field(MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTS,
		host->saved_para.cfg_crcsts_path);
	sdr_get_field(MSDC_PATCH_BIT2, MSDC_PB2_CFGRESP,
		host->saved_para.cfg_cmdrsp_path);
	sdr_get_field(MSDC_PATCH_BIT2, MSDC_PB2_RESPWAITCNT,
		host->saved_para.resp_wait_cnt);
}

static void msdc_restore_emmc_setting(struct msdc_host *host)
{
	void __iomem *base = host->base;

	msdc_set_mclk(host, host->saved_para.timing, host->mclk);
	sdr_write32(SDC_CFG, host->saved_para.sdc_cfg);

	sdr_set_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, host->hw->ddlsel);

	msdc_set_smpl(host, host->saved_para.timing == MMC_TIMING_MMC_HS400,
		      host->hw->cmd_edge, TYPE_CMD_RESP_EDGE, NULL);
	msdc_set_smpl(host, host->saved_para.timing == MMC_TIMING_MMC_HS400,
		      host->hw->rdata_edge, TYPE_READ_DATA_EDGE, NULL);
	msdc_set_smpl(host, host->saved_para.timing == MMC_TIMING_MMC_HS400,
		      host->hw->wdata_edge, TYPE_WRITE_CRC_EDGE, NULL);
	sdr_write32(MSDC_PAD_TUNE0, host->saved_para.pad_tune0);
	sdr_write32(MSDC_DAT_RDDLY0, host->saved_para.ddly0);
	sdr_write32(MSDC_DAT_RDDLY1, host->saved_para.ddly1);
	sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR,
		host->saved_para.wrdat_crc_ta_cntr);
	sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR,
		host->saved_para.cmd_resp_ta_cntr);
	sdr_set_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY1,
		host->saved_para.ds_dly1);
	sdr_set_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY3,
		host->saved_para.ds_dly3);
	sdr_write32(EMMC50_PAD_CMD_TUNE, host->saved_para.emmc50_pad_cmd_tune);

	sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_GET_BUSY_MA,
		host->saved_para.write_busy_margin);
	sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_GET_CRC_MA,
		host->saved_para.write_crc_margin);

	sdr_set_field(MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTS,
		host->saved_para.cfg_crcsts_path);
	sdr_set_field(MSDC_PATCH_BIT2, MSDC_PB2_CFGRESP,
		host->saved_para.cfg_cmdrsp_path);
	sdr_set_field(MSDC_PATCH_BIT2, MSDC_PB2_RESPWAITCNT,
		host->saved_para.resp_wait_cnt);
}

static void msdc_pm(pm_message_t state, void *data)
{
	struct msdc_host *host = (struct msdc_host *)data;

	int evt = state.event;

	msdc_ungate_clock(host);
	if (host->hw->host_function == MSDC_EMMC)
		emmc_do_sleep_awake = 1;

	if (evt == PM_EVENT_SUSPEND || evt == PM_EVENT_USER_SUSPEND) {
		if (host->suspend)
			goto end;

		if (evt == PM_EVENT_SUSPEND && host->power_mode == MMC_POWER_OFF)
			goto end;

		host->suspend = 1;
		host->pm_state = state;
#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT
		atomic_set(&host->ot_work.autok_done, 0);
#endif

		pr_err("msdc%d -> %s Suspend",
			host->id, evt == PM_EVENT_SUSPEND ? "PM" : "USR");
		if (host->hw->flags & MSDC_SYS_SUSPEND) {
			if (host->hw->host_function == MSDC_EMMC) {
				msdc_save_emmc_setting(host);
				host->power_control(host, 0);
				msdc_pin_config(host, MSDC_PIN_PULL_DOWN);
				msdc_pin_reset(host, MSDC_PIN_PULL_DOWN);
			}
#ifndef FPGA_PLATFORM
			msdc_set_tdsel(host, 1);
#endif
		} else {
			host->mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;
			mmc_remove_host(host->mmc);
		}
	} else if (evt == PM_EVENT_RESUME || evt == PM_EVENT_USER_RESUME) {
		if (!host->suspend)
			goto end;

		if (evt == PM_EVENT_RESUME
			&& host->pm_state.event == PM_EVENT_USER_SUSPEND) {
			ERR_MSG("PM Resume when in USR Suspend");
			goto end;
		}

		host->suspend = 0;
		host->pm_state = state;

		pr_err("msdc%d -> %s Resume",
			host->id, evt == PM_EVENT_RESUME ? "PM" : "USR");

		if (host->hw->flags & MSDC_SYS_SUSPEND) {
#ifndef FPGA_PLATFORM
			msdc_set_tdsel(host, 0);
#endif

			if (host->hw->host_function == MSDC_EMMC) {
				msdc_reset_hw(host->id);
				msdc_pin_reset(host, MSDC_PIN_PULL_UP);
				msdc_pin_config(host, MSDC_PIN_PULL_UP);
				host->power_control(host, 1);
				mdelay(10);
				msdc_restore_emmc_setting(host);
			}
		} else {
			host->mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;
			mmc_add_host(host->mmc);
		}
	}

 end:
#ifdef SDIO_ERROR_BYPASS
	if (is_card_sdio(host))
		host->sdio_error = 0;
#endif
	if ((evt == PM_EVENT_SUSPEND) || (evt == PM_EVENT_USER_SUSPEND)) {
		if ((host->hw->host_function == MSDC_SDIO)
			&& (evt == PM_EVENT_USER_SUSPEND))
			pr_debug("msdc%d -> MSDC Device Request Suspend", host->id);
		msdc_gate_clock(host, 0);
	} else {
		msdc_gate_clock(host, 1);
	}

	if (host->hw->host_function == MSDC_SDIO) {
		host->mmc->pm_flags |= MMC_PM_KEEP_POWER;
		host->mmc->rescan_entered = 0;
	}

	if (host->hw->host_function == MSDC_EMMC)
		emmc_do_sleep_awake = 0;
}
#endif

#if 0 /* weiping fix reserve */
static u64 msdc_get_user_capacity(struct msdc_host *host)
{
	u64 device_capacity = 0;
	u32 device_legacy_capacity = 0;
	struct mmc_host *mmc = NULL;

	BUG_ON(!host);
	BUG_ON(!host->mmc);
	BUG_ON(!host->mmc->card);
	mmc = host->mmc;
	if (mmc_card_mmc(mmc->card)) {
		if (mmc->card->csd.read_blkbits) {
			device_legacy_capacity = mmc->card->csd.capacity *
				(2 << (mmc->card->csd.read_blkbits - 1));
		} else {
			device_legacy_capacity = mmc->card->csd.capacity;
			ERR_MSG("XXX read_blkbits = 0 XXX");
		}
		device_capacity =
			(u64) (mmc->card->ext_csd.sectors) * 512 > device_legacy_capacity ?
		    (u64) (mmc->card->ext_csd.sectors) * 512 : device_legacy_capacity;
	} else if (mmc_card_sd(mmc->card)) {
		device_capacity =
			(u64) (mmc->card->csd.capacity) << (mmc->card->csd.read_blkbits);
	}
	return device_capacity;
}
#endif
struct msdc_host *msdc_get_host(int host_function, bool boot, bool secondary)
{
	int host_index = 0;
	struct msdc_host *host = NULL;

	for (; host_index < HOST_MAX_NUM; ++host_index) {
		if (!mtk_msdc_host[host_index])
			continue;
		if ((host_function == mtk_msdc_host[host_index]->hw->host_function)
		    && (boot == mtk_msdc_host[host_index]->hw->boot)) {
			host = mtk_msdc_host[host_index];
			break;
		}
	}
	if (secondary && (host_function == MSDC_SD))
		host = mtk_msdc_host[2];
	if (host == NULL) {
		pr_err("[%s]MSDC-ERROR host_function:%d,boot:%d,secondary:%d\n"
		    , __func__, host_function, boot, secondary);
		/* BUG(); */
	}

	return host;
}
EXPORT_SYMBOL(msdc_get_host);

/* #ifdef CONFIG_MTK_EMMC_SUPPORT */
#if 0

u8 ext_csd[512];
EXPORT_SYMBOL(ext_csd);

int offset = 0;
char partition_access = 0;

static int msdc_get_ext_csd(u8 *dst, struct mmc_data *data,
	struct msdc_host *host)
{
	u8 *ptr;
	int i;
#ifdef MTK_MSDC_USE_CACHE
	enum boot_mode_t mode;
#endif
	struct scatterlist *sg;

	sg = data->sg;
	ptr = (u8 *) sg_virt(sg);
#ifdef MTK_MSDC_USE_CACHE
	g_emmc_cache_size = (*(ptr + 252) << 24) +
						(*(ptr + 251) << 16) +
						(*(ptr + 250) << 8) +
						(*(ptr + 249) << 0);
	/*
	 * only enable the emmc cache feature for normal boot up,
	 * alarm boot up, and sw reboot
	 */
	mode = get_boot_mode();
	if ((mode == NORMAL_BOOT) || (mode == ALARM_BOOT) || (mode == SW_REBOOT)) {
		for (i = 0; i < sizeof(g_emmc_cache_quirk); i++) {
			if (g_emmc_cache_quirk[i] == emmc_id)
				*(ptr + 252) = *(ptr + 251) = *(ptr + 250) = *(ptr + 249) = 0;
		}
	}
#else
	*(ptr + 252) = *(ptr + 251) = *(ptr + 250) = *(ptr + 249) = 0;
#endif
	memcpy(dst, ptr, msdc_sg_len(sg, host->dma_xfer));

	return 0;
}

#ifdef MTK_MSDC_USE_CACHE
static void msdc_set_cache_quirk(struct msdc_host *host)
{
	/*
	 * if need disable emmc cache feature for some vendor, plese add quirk here
	 * exmple:
	 * g_emmc_cache_quirk[0] = CID_MANFID_HYNIX;
	 * g_emmc_cache_quirk[1] = CID_MANFID_SAMSUNG;
	 */
	int i;

	for (i = 0; i < sizeof(g_emmc_cache_quirk); i++) {
		if (g_emmc_cache_quirk[i] == 0) {
			pr_debug("msdc%d total emmc cache quirk count=%d\n", host->id, i);
			break;
		}
		pr_debug("msdc%d,add emmc cache quirk[%d]=0x%x\n",
			host->id, i, g_emmc_cache_quirk[i]);
	 }
}



static int msdc_can_apply_cache(unsigned long long start_addr,
			unsigned int size)
{
	if (!g_cache_part_start && !g_cache_part_end &&
		!g_usrdata_part_start && !g_usrdata_part_end)
		return 0;

	/* since cache, userdata partition are connected,
	 * so check it as an area, else do check them separately
	 */
	if (g_cache_part_end == g_usrdata_part_start) {
		if (!((start_addr >= g_cache_part_start)
		      && (start_addr + size < g_usrdata_part_end))) {
			return 0;
		}
	} else {
		if (!(((start_addr >= g_cache_part_start)
		       && (start_addr + size < g_cache_part_end))
		      || ((start_addr >= g_usrdata_part_start)
			  && (start_addr + size < g_usrdata_part_end)))) {
			return 0;
		}
	}

	return 1;
}
#endif

int msdc_cache_ctrl(struct msdc_host *host, unsigned int enable,
			u32 *status)
{
	struct mmc_command cmd;
	struct mmc_request mrq;
	u32 err;

	memset(&cmd, 0, sizeof(struct mmc_command));
	cmd.opcode = MMC_SWITCH;	/* CMD6 */
	cmd.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) | (EXT_CSD_CACHE_CTRL << 16)
	    | ((!!enable) << 8) | EXT_CSD_CMD_SET_NORMAL;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	memset(&mrq, 0, sizeof(struct mmc_request));
	mrq.cmd = &cmd;
	cmd.mrq = &mrq;
	cmd.data = NULL;

	ERR_MSG("do disable Cache, cmd=0x%x, arg=0x%x\n", cmd.opcode, cmd.arg);
	err = msdc_do_command(host, &cmd, 0, CMD_TIMEOUT);
	/* tune until CMD13 pass. */

	if (status)
		*status = cmd.resp[0];

	if (!err) {
		host->mmc->card->ext_csd.cache_ctrl = !!enable;
		host->autocmd |= MSDC_AUTOCMD23;
		N_MSG(CHE, "enable AUTO_CMD23 because Cache feature is disabled\n");
	}

	return err;
}

int msdc_get_cache_region(void)
{
#ifdef MTK_MSDC_USE_CACHE
	struct msdc_host *host;

	host = msdc_get_host(MSDC_EMMC, MSDC_BOOT_EN, 0);

	struct hd_struct *lp_hd_struct = NULL;

	lp_hd_struct = get_part("cache");
	if (likely(lp_hd_struct)) {
		g_cache_part_start = lp_hd_struct->start_sect;
		g_cache_part_end = g_cache_part_start + lp_hd_struct->nr_sects;
		put_part(lp_hd_struct);
	} else {
		g_cache_part_start = (sector_t) (-1);
		g_cache_part_end = (sector_t) (-1);
		pr_err("There is no cache info\n");
	}

	lp_hd_struct = NULL;
	lp_hd_struct = get_part("userdata");
	if (likely(lp_hd_struct)) {
		g_usrdata_part_start = lp_hd_struct->start_sect;
		g_usrdata_part_end = g_usrdata_part_start + lp_hd_struct->nr_sects;
		put_part(lp_hd_struct);
	} else {
		g_usrdata_part_start = (sector_t) (-1);
		g_usrdata_part_end = (sector_t) (-1);
		pr_debug("There is no userdata info\n");
	}

	pr_debug("msdc0:cache(0x%lld~0x%lld), usrdata(0x%lld~0x%lld)\n",
		g_cache_part_start, g_cache_part_end,
		g_usrdata_part_start, g_usrdata_part_end);
#endif
		return 0;
}
EXPORT_SYMBOL(msdc_get_cache_region);

#if 0 /* weiping fix reserve */
#ifdef CONFIG_MTK_EMMC_SUPPORT_OTP
/* parse part_info struct, support otp & mtk reserve */
#ifndef CONFIG_MTK_GPT_SCHEME_SUPPORT
static struct excel_info *msdc_reserve_part_info(unsigned char *name)
{
	int i;

	/* find reserve partition */
	for (i = 0; i < PART_NUM; i++) {
		pr_debug("name = %s\n", PartInfo[i].name);
		if (0 == strcmp(name, PartInfo[i].name)) {
			pr_debug("size = %llu\n", PartInfo[i].size);
			return &PartInfo[i];
		}
	}

	return NULL;
}
#endif
#endif
static u32 msdc_get_other_capacity(void)
{
	u32 device_other_capacity = 0;
	int idx;

	device_other_capacity = ext_csd[EXT_CSD_BOOT_MULT] * 128 * 1024
	    + ext_csd[EXT_CSD_BOOT_MULT] * 128 * 1024
	    + ext_csd[EXT_CSD_RPMB_MULT] * 128 * 1024;

	for (idx = 0; idx < MMC_NUM_GP_PARTITION; idx++) {
		device_other_capacity +=
			(ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3 + 2] << 16) +
			(ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3 + 1] << 8) +
			ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3];
	}

	return device_other_capacity;
}

int msdc_get_offset(void)
{
	u32 l_offset;
	/* l_offset =  MBR_START_ADDRESS_BYTE - msdc_get_other_capacity(); */
	l_offset = 0;

	return l_offset >> 9;
}
EXPORT_SYMBOL(msdc_get_offset);
int msdc_get_reserve(void)
{
	u32 l_offset;
#ifndef CONFIG_MTK_GPT_SCHEME_SUPPORT
	struct excel_info *lp_excel_info;
#endif
	u32 l_otp_reserve = 0;
	u32 l_mtk_reserve = 0;

	l_offset = msdc_get_offset();	/* ==========check me */

#ifndef CONFIG_MTK_GPT_SCHEME_SUPPORT
	lp_excel_info = msdc_reserve_part_info("bmtpool");
	if (NULL == lp_excel_info) {
		pr_err("can't get otp info from part_info struct\n");
		return -1;
	}
	/* unit is 512B */
	l_mtk_reserve =
		(unsigned int)(lp_excel_info->start_address & 0xFFFFUL) << 8;

	pr_debug("mtk reserve: start address = %llu\n",
		lp_excel_info->start_address);

#ifdef CONFIG_MTK_EMMC_SUPPORT_OTP
	lp_excel_info = msdc_reserve_part_info("otp");
	if (NULL == lp_excel_info) {
		pr_err("can't get otp info from part_info struct\n");
		return -1;
	}
	/* unit is 512B */
	l_otp_reserve =
		(unsigned int)(lp_excel_info->start_address & 0xFFFFUL) << 8;

	pr_debug("otp reserve: start address = %llu\n",
		lp_excel_info->start_address);
	/* the size info stored with total reserved size */
	l_otp_reserve -= l_mtk_reserve;
#endif
	pr_debug("otp_reserve=0x%x blks,mtk_reserve=0x%x blks,offset=0x%x blks\n",
	    l_otp_reserve, l_mtk_reserve, l_offset);
#endif

	return l_offset + l_otp_reserve + l_mtk_reserve;
}
EXPORT_SYMBOL(msdc_get_reserve);
#if 0 /* removed in new combo eMMC feature */
static bool msdc_cal_offset(struct msdc_host *host)
{
/*
	u64 device_capacity = 0;
	offset =  MBR_START_ADDRESS_BYTE - msdc_get_other_capacity();
	device_capacity = msdc_get_user_capacity(host);
	if(mmc_card_blockaddr(host->mmc->card))
	offset /= 512;
	ERR_MSG("Address offset in USER REGION(Capacity %lld MB) is 0x%x",
		device_capacity/(1024*1024),offset);
	if(offset < 0) {
	ERR_MSG("XXX Address offset error(%d),please check MBR start address!!",
		(int)offset);
	BUG();
	}
*/
	offset = 0;
	return true;
}
#endif
#endif /* weiping fix reserve */
#endif
#if 0 /* weiping fix reserve */
u64 msdc_get_capacity(int get_emmc_total)
{
	u64 user_size = 0;
	u32 other_size = 0;
	u64 total_size = 0;
	int index = 0;

	for (index = 0; index < HOST_MAX_NUM; ++index) {
		if ((mtk_msdc_host[index] != NULL)
			&& (mtk_msdc_host[index]->hw->boot)) {
			user_size = msdc_get_user_capacity(mtk_msdc_host[index]);
#ifdef CONFIG_MTK_EMMC_SUPPORT
			if (get_emmc_total) {
				if (mmc_card_mmc(mtk_msdc_host[index]->mmc->card))
					other_size = msdc_get_other_capacity();
			}
#endif
			break;
		}
	}
	total_size = user_size + (u64) other_size;
	return total_size / 512;
}
EXPORT_SYMBOL(msdc_get_capacity);
#endif
/*--------------------------------------------------------------------------*/
/* mmc_host_ops members                                                     */
/*--------------------------------------------------------------------------*/
static u32 wints_cmd = MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO |
	MSDC_INT_ACMDRDY | MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO;
static unsigned int msdc_command_start(struct msdc_host *host,
						struct mmc_command *cmd, int tune,
						unsigned long timeout)
{
	void __iomem *base = host->base;
	u32 opcode = cmd->opcode;
	u32 rawcmd;
	u32 rawarg;
	u32 resp;
	unsigned long tmo;
	struct mmc_command *sbc = NULL;

	if (host->data && host->data->mrq && host->data->mrq->sbc)
		sbc = host->data->mrq->sbc;

	/* Protocol layer does not provide response type, but our hardware needs
	 * to know exact type, not just size!
	 */
	if (opcode == MMC_SEND_OP_COND || opcode == SD_APP_OP_COND)
		resp = RESP_R3;
	else if (opcode == MMC_SET_RELATIVE_ADDR || opcode == SD_SEND_RELATIVE_ADDR)
		resp = (mmc_cmd_type(cmd) == MMC_CMD_BCR) ? RESP_R6 : RESP_R1;
	else if (opcode == MMC_FAST_IO)
		resp = RESP_R4;
	else if (opcode == MMC_GO_IRQ_STATE)
		resp = RESP_R5;
	else if (opcode == MMC_SELECT_CARD) {
		resp = (cmd->arg != 0) ? RESP_R1 : RESP_NONE;
		host->app_cmd_arg = cmd->arg;
		pr_warn("msdc%d select card<0x%.8x>", host->id, cmd->arg);
	} else if (opcode == SD_IO_RW_DIRECT || opcode == SD_IO_RW_EXTENDED)
		resp = RESP_R1;	/* SDIO workaround. */
	else if (opcode == SD_SEND_IF_COND && (mmc_cmd_type(cmd) == MMC_CMD_BCR))
		resp = RESP_R1;
	else if (opcode == MMC_SEND_STATUS)	/* workaround for ignore crc */
		resp = RESP_R1;
	else {
		switch (mmc_resp_type(cmd)) {
		case MMC_RSP_R1:
			resp = RESP_R1;
			break;
		case MMC_RSP_R1B:
			resp = RESP_R1B;
			break;
		case MMC_RSP_R2:
			resp = RESP_R2;
			break;
		case MMC_RSP_R3:
			resp = RESP_R3;
			break;
		case MMC_RSP_NONE:
		default:
			resp = RESP_NONE;
			break;
		}
	}

	cmd->error = 0;
	/* rawcmd :
	 * vol_swt << 30 | auto_cmd << 28 | blklen << 16 | go_irq << 15 |
	 * stop << 14 | rw << 13 | dtype << 11 | rsptyp << 7 | brk << 6 | opcode
	 */
	rawcmd = opcode | msdc_rsp[resp] << 7 | host->blksz << 16;

	if (opcode == MMC_READ_MULTIPLE_BLOCK) {
		rawcmd |= (2 << 11);
		if (host->autocmd & MSDC_AUTOCMD12)
			rawcmd |= (1 << 28);
#ifdef MTK_MSDC_USE_CMD23
		else if ((host->autocmd & MSDC_AUTOCMD23)) {
			rawcmd |= (1 << 29);
			if (sbc) {
				/* if the block number is bigger than 0xFFFF,
				 * then CMD23 arg will be failed to set it
				 */
				if (sdr_read32(SDC_BLK_NUM) != (sbc->arg & 0xFFFF))
					pr_err("msdc%d: acmd23 arg(0x%x) != read blocks(0x%x),SDC_BLK_NUM(0x%x)\n",
					host->id, sbc->arg,	host->mrq->cmd->data->blocks,
					sdr_read32(SDC_BLK_NUM));
				else
					sdr_write32(SDC_BLK_NUM, sbc->arg);
				CMD_MSG("CMD<23> arg<0x%.8x> @ addr<0x%.8x>",
					sbc->arg, cmd->arg);
			}
		}
#endif				/* end of MTK_MSDC_USE_CMD23 */
	} else if (opcode == MMC_READ_SINGLE_BLOCK) {
		rawcmd |= (1 << 11);
	} else if (opcode == MMC_WRITE_MULTIPLE_BLOCK) {
		rawcmd |= ((2 << 11) | (1 << 13));
		if (host->autocmd & MSDC_AUTOCMD12)
			rawcmd |= (1 << 28);
#ifdef MTK_MSDC_USE_CMD23
		else if ((host->autocmd & MSDC_AUTOCMD23)) {
			rawcmd |= (1 << 29);
			if (sbc) {
				if (sdr_read32(SDC_BLK_NUM) != (sbc->arg & 0xFFFF))
					pr_err
					    ("msdc%d: acmd23 arg(0x%x) != write blocks(0x%x),SDC_BLK_NUM(0x%x)\n",
					    host->id, sbc->arg, host->mrq->cmd->data->blocks,
					    sdr_read32(SDC_BLK_NUM));
				else
					sdr_write32(SDC_BLK_NUM, sbc->arg);
				CMD_MSG("CMD<23> arg<0x%.8x> @ addr<0x%.8x>",
					sbc->arg, cmd->arg);
			}
		}
#endif /* end of MTK_MSDC_USE_CMD23 */
	} else if (opcode == MMC_WRITE_BLOCK) {
		rawcmd |= ((1 << 11) | (1 << 13));
	} else if (opcode == SD_IO_RW_EXTENDED) {
		if (cmd->data->flags & MMC_DATA_WRITE)
			rawcmd |= (1 << 13);
		if (cmd->data->blocks > 1)
			rawcmd |= (2 << 11);
		else
			rawcmd |= (1 << 11);
	} else if (opcode == SD_IO_RW_DIRECT && cmd->flags == (unsigned int)-1) {
		rawcmd |= (1 << 14);
	} else if (opcode == SD_SWITCH_VOLTAGE) {
		rawcmd |= (1 << 30);
	} else if ((opcode == SD_APP_SEND_SCR)
		|| (opcode == SD_APP_SEND_NUM_WR_BLKS)
		|| (opcode == SD_SWITCH && (mmc_cmd_type(cmd) == MMC_CMD_ADTC))
		|| (opcode == SD_APP_SD_STATUS && (mmc_cmd_type(cmd) == MMC_CMD_ADTC))
		|| (opcode == MMC_SEND_EXT_CSD && (mmc_cmd_type(cmd) == MMC_CMD_ADTC)))
		rawcmd |= (1 << 11);
	else if (opcode == MMC_STOP_TRANSMISSION) {
		rawcmd |= (1 << 14);
		rawcmd &= ~(0x0FFF << 16);
	}

	CMD_MSG("CMD<%d> arg<0x%.8x>", cmd->opcode, cmd->arg);

	tmo = jiffies + timeout;

	if (opcode == MMC_SEND_STATUS) {
		for (;;) {
			if (!sdc_is_cmd_busy())
				break;

			if (time_after(jiffies, tmo)) {
				ERR_MSG("XXX cmd_busy timeout: before CMD<%d>", opcode);
				cmd->error = (unsigned int)-ETIMEDOUT;
				msdc_dump_register(host);
				msdc_reset_hw(host->id);
				return cmd->error;
			}
		}
	} else {
		for (;;) {
			if (!sdc_is_busy())
				break;
			if (time_after(jiffies, tmo)) {
				ERR_MSG("XXX sdc_busy timeout: before CMD<%d>", opcode);
				cmd->error = (unsigned int)-ETIMEDOUT;
				msdc_dump_register(host);
				msdc_reset_hw(host->id);
				return cmd->error;
			}
		}
	}

	/* BUG_ON(in_interrupt()); */
	host->cmd = cmd;
	host->cmd_rsp = resp;

	/* use polling way */
	sdr_clr_bits(MSDC_INTEN, wints_cmd);
	rawarg = cmd->arg;

	sdc_send_cmd(rawcmd, rawarg);

/*end:*/
/* irq too fast, then cmd->error has value,
 * and don't call msdc_command_resp, don't tune.
 */
	return 0;
}

static unsigned int msdc_command_resp_polling(struct msdc_host *host,
					      struct mmc_command *cmd, int tune,
					      unsigned long timeout)
{
	void __iomem *base = host->base;
	u32 intsts;
	u32 resp;
	/* u32 status; */
	unsigned long tmo;
	/* struct mmc_data   *data = host->data; */
	u32 cmdsts = MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO;
#ifdef MTK_MSDC_USE_CMD23
	struct mmc_command *sbc = NULL;
#endif

#ifdef MTK_MSDC_USE_CMD23
	if (host->autocmd & MSDC_AUTOCMD23) {
		if (host->data && host->data->mrq && host->data->mrq->sbc)
			sbc = host->data->mrq->sbc;

		/* autocmd interrupt disabled, used polling way */
		cmdsts |= MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO;
	}
#endif

	resp = host->cmd_rsp;

	/*polling */
	tmo = jiffies + timeout;
	while (1) {
		intsts = sdr_read32(MSDC_INT);
		if (intsts & cmdsts) {
			/* clear all int flag */
#ifdef MTK_MSDC_USE_CMD23
			/* need clear autocmd23 command ready interrupt */
			intsts &= (cmdsts | MSDC_INT_ACMDRDY);
#else
			intsts &= cmdsts;
#endif
			sdr_write32(MSDC_INT, intsts);
			break;
		}

		if (time_after(jiffies, tmo)) {
			pr_err("[%s]: msdc%d XXX CMD<%d> polling resp timeout ARG<0x%.8x>\n"
				, __func__, host->id, cmd->opcode, cmd->arg);
			cmd->error = (unsigned int)-ETIMEDOUT;
			host->sw_timeout++;
			msdc_dump_info(host->id);
			msdc_reset_hw(host->id);
			goto out;
		}
	}

#ifdef MTK_MSDC_ERROR_TUNE_DEBUG
	if (g_err_tune_dbg_error && (g_err_tune_dbg_count > 0)
		&& (g_err_tune_dbg_host == host->id)) {
		if (g_err_tune_dbg_cmd == cmd->opcode) {
			if ((g_err_tune_dbg_cmd != MMC_SWITCH) ||
			    ((g_err_tune_dbg_cmd == MMC_SWITCH)
			    && (g_err_tune_dbg_arg == ((cmd->arg >> 16) & 0xff)))) {
				if (g_err_tune_dbg_error & MTK_MSDC_ERROR_CMD_TMO) {
					intsts = MSDC_INT_CMDTMO;
					g_err_tune_dbg_count--;
				} else if (g_err_tune_dbg_error & MTK_MSDC_ERROR_CMD_CRC) {
					intsts = MSDC_INT_RSPCRCERR;
					g_err_tune_dbg_count--;
				}
				pr_err("[%s]:make error cmd:%d,arg=%d,error type=%d, count=%d\n"
					, __func__, g_err_tune_dbg_cmd, g_err_tune_dbg_arg,
					g_err_tune_dbg_error, g_err_tune_dbg_count);
			}
		}
#ifdef MTK_MSDC_USE_CMD23
		if ((g_err_tune_dbg_cmd == MMC_SET_BLOCK_COUNT)
			&& sbc && (host->autocmd & MSDC_AUTOCMD23)) {
			if (g_err_tune_dbg_error & MTK_MSDC_ERROR_ACMD_TMO) {
				intsts = MSDC_INT_ACMDTMO;
				g_err_tune_dbg_count--;
				pr_err("[%s]:make ACMD23 timeout error, count=%d\n",
					__func__, g_err_tune_dbg_count);
			} else if (g_err_tune_dbg_error & MTK_MSDC_ERROR_ACMD_CRC) {
				intsts = MSDC_INT_ACMDCRCERR;
				g_err_tune_dbg_count--;
				pr_err("[%s]:make ACMD23 crc error, count=%d\n",
					__func__, g_err_tune_dbg_count);
			}
		}
#endif
	}
#endif
	/* command interrupts */
	if (intsts & cmdsts) {
#ifdef MTK_MSDC_USE_CMD23
		if ((intsts & MSDC_INT_CMDRDY) || (intsts & MSDC_INT_ACMD19_DONE)) {
#else
		if ((intsts & MSDC_INT_CMDRDY) || (intsts & MSDC_INT_ACMDRDY)
			|| (intsts & MSDC_INT_ACMD19_DONE)) {
#endif
			u32 *rsp = NULL;

			rsp = &cmd->resp[0];
			switch (host->cmd_rsp) {
			case RESP_NONE:
				break;
			case RESP_R2:
				*rsp++ = sdr_read32(SDC_RESP3);
				*rsp++ = sdr_read32(SDC_RESP2);
				*rsp++ = sdr_read32(SDC_RESP1);
				*rsp++ = sdr_read32(SDC_RESP0);
				break;
			default:	/* Response types 1, 3, 4, 5, 6, 7(1b) */
				*rsp = sdr_read32(SDC_RESP0);
				break;
			}
		} else if (intsts & MSDC_INT_RSPCRCERR) {
			cmd->error = (unsigned int)-EIO;
			pr_err("[%s]: msdc%d XXX CMD<%d> MSDC_INT_RSPCRCERR Arg<0x%.8x>",
				__func__, host->id, cmd->opcode, cmd->arg);
			if ((MMC_RSP_R1B == mmc_resp_type(cmd))
				&& (host->hw->host_function != MSDC_SDIO)) {
				pr_err("[%s]: msdc%d XXX CMD<%d> ARG<0x%.8X> CRC not reset hw\n",
					__func__, host->id, cmd->opcode, cmd->arg);
			} else if (cmd->opcode == 13) {
				pr_err("XXX CMD<13>CRC not reset hw...\n");
			} else {
				msdc_reset_hw(host->id);
			}
		} else if (intsts & MSDC_INT_CMDTMO) {
			cmd->error = (unsigned int)-ETIMEDOUT;
			pr_err("[%s]: msdc%d XXX CMD<%d> MSDC_INT_CMDTMO Arg<0x%.8x>",
				__func__, host->id, cmd->opcode, cmd->arg);
			if ((cmd->opcode != 52) && (cmd->opcode != 8) && (cmd->opcode != 5)
			    && (cmd->opcode != 55) && (cmd->opcode != 1))
				msdc_dump_info(host->id);
			if ((cmd->opcode == 5) && emmc_do_sleep_awake)
				msdc_dump_info(host->id);
			if ((MMC_RSP_R1B == mmc_resp_type(cmd))
				&& (host->hw->host_function != MSDC_SDIO)) {
				pr_err("[%s]: msdc%d XXX CMD<%d> ARG<0x%.8X> TMO not reset hw\n",
					__func__, host->id, cmd->opcode, cmd->arg);
			} else if (cmd->opcode == 13) {
				pr_err("XXX CMD<13>CRC not reset hw...\n");
			} else {
				msdc_reset_hw(host->id);
			}
		}
#ifdef MTK_MSDC_USE_CMD23
		if ((sbc != NULL) && (host->autocmd & MSDC_AUTOCMD23)) {
			if (intsts & MSDC_INT_ACMDRDY) {
				u32 *arsp = &sbc->resp[0];
				*arsp = sdr_read32(SDC_ACMD_RESP);
				CMD_MSG("CMD<23> arg<0x%.8x> @ addr<0x%.8x> resp<0x%.8x>",
					     sbc->arg, cmd->arg, sbc->resp[0]);
			} else if (intsts & MSDC_INT_ACMDCRCERR) {
				pr_err("[%s]: msdc%d, autocmd23 crc error\n",
					__func__, host->id);
				sbc->error = (unsigned int)-EIO;
				cmd->error = (unsigned int)-EIO;
				/* host->error |= REQ_CMD23_EIO; */
				/* record the error info in current cmd struct */
				msdc_reset_hw(host->id);
			} else if (intsts & MSDC_INT_ACMDTMO) {
				pr_err("[%s]: msdc%d, autocmd23 tmo error\n",
					__func__, host->id);
				sbc->error = (unsigned int)-ETIMEDOUT;
				cmd->error = (unsigned int)-ETIMEDOUT;
				msdc_dump_info(host->id);
				/* host->error |= REQ_CMD23_TMO; */
				/* record the error info in current cmd struct */
				msdc_reset_hw(host->id);
			}
		}
#endif /* end of MTK_MSDC_USE_CMD23 */
	}
 out:
	host->cmd = NULL;
	if (!cmd->error)
		CMD_MSG("CMD<%d> arg<0x%.8x> resp<0x%.8x>",
			cmd->opcode, cmd->arg, cmd->resp[0]);
	else
		CMD_MSG("CMD<%d> arg<0x%.8x> resp<0x%.8x>,error=%d",
			cmd->opcode, cmd->arg, cmd->resp[0], cmd->error);
	return cmd->error;
}

unsigned int msdc_do_command(struct msdc_host *host, struct mmc_command *cmd,
	int tune, unsigned long timeout)
{
	if ((cmd->opcode == MMC_GO_IDLE_STATE)
		&& (host->hw->host_function == MSDC_SD))
		mdelay(10);

	if (msdc_command_start(host, cmd, tune, timeout))
		goto end;
	if (msdc_command_resp_polling(host, cmd, tune, timeout))
		goto end;
 end:

	return cmd->error;
}

/* The abort condition when PIO read/write
   tmo:
*/
static int msdc_pio_abort(struct msdc_host *host, struct mmc_data *data,
	unsigned long tmo)
{
	int ret = 0;

	if (atomic_read(&host->abort))
		ret = 1;

	if (time_after(jiffies, tmo)) {
		data->error = (unsigned int)-ETIMEDOUT;
		ERR_MSG("XXX PIO Data Timeout: CMD<%d>", host->mrq->cmd->opcode);
		msdc_dump_info(host->id);
		ret = 1;
	}

	if (ret) {
		msdc_reset_hw(host->id);
		ERR_MSG("msdc pio find abort");
	}
	return ret;
}

/*
 * Need to add a timeout, or WDT timeout, system reboot.
 */
/* pio mode data read/write */
#define COMBINE_HM
int msdc_pio_read(struct msdc_host *host, struct mmc_data *data)
{
	struct scatterlist *sg = data->sg;
	void __iomem *base = host->base;
	u32 num = data->sg_len;
	u32 *ptr;
	u8 *u8ptr;
	u32 left = 0;
	u32 count, size = 0;
	u32 wints = MSDC_INTEN_DATTMO | MSDC_INTEN_DATCRCERR
		| MSDC_INTEN_XFER_COMPL;
	u32 ints = 0;
	bool get_xfer_done = 0;
	unsigned long tmo = jiffies + DAT_TIMEOUT;
	struct page *hmpage = NULL;
	int i = 0, subpage = 0, totalpages = 0;
	int flag = 0;
	ulong kaddr[DIV_ROUND_UP(MAX_SGMT_SZ, PAGE_SIZE)];

	BUG_ON(sg == NULL);
	/*MSDC_CLR_BIT32(MSDC_INTEN, wints);*/
	while (1) {
		if (!get_xfer_done) {
			ints = sdr_read32(MSDC_INT);
			latest_int_status[host->id] = ints;
			ints &= wints;
			sdr_write32(MSDC_INT, ints);
		}
		if (ints & MSDC_INT_DATTMO) {
			data->error = (unsigned int)-ETIMEDOUT;
			msdc_dump_info(host->id);
			msdc_reset_hw(host->id);
			break;
		} else if (ints & MSDC_INT_DATCRCERR) {
			data->error = (unsigned int)-EIO;
			msdc_reset_hw(host->id);
			break;
		} else if (ints & MSDC_INT_XFER_COMPL) {
			get_xfer_done = 1;
		}
		if (get_xfer_done && (num == 0) && (left == 0))
			break;
		if (msdc_pio_abort(host, data, tmo))
			goto end;
		if ((num == 0) && (left == 0))
			continue;
		left = msdc_sg_len(sg, host->dma_xfer);
		ptr = sg_virt(sg);
		flag = 0;

		if	((ptr != NULL) &&
			 !(PageHighMem((struct page *)(sg->page_link & ~0x3))))
		#ifndef COMBINE_HM
			goto check_fifo2;
		#else
			goto check_fifo1;
		#endif

		hmpage = (struct page *)(sg->page_link & ~0x3);
		totalpages = DIV_ROUND_UP((left + sg->offset), PAGE_SIZE);
		subpage = (left + sg->offset) % PAGE_SIZE;

		if (subpage != 0 || (sg->offset != 0))
			N_MSG(OPS, "msdc%d: read size or start not align %x, %x, hmpage %lx,sg offset %x\n",
				host->id,
				subpage, left, (ulong)hmpage, sg->offset);

		for (i = 0; i < totalpages; i++) {
			kaddr[i] = (ulong) kmap(hmpage + i);
			if ((i > 0) && ((kaddr[i] - kaddr[i - 1]) != PAGE_SIZE))
				flag = 1;
			if (!kaddr[i])
				ERR_MSG("msdc0:kmap failed %lx", kaddr[i]);
		}

		ptr = sg_virt(sg);

		if (ptr == NULL)
			ERR_MSG("msdc0:sg_virt %p", ptr);

		if (flag == 0)
		#ifndef COMBINE_HM
			goto check_fifo2;
		#else
			goto check_fifo1;
		#endif

		/* High memory and more than 1 va address va
		   and not continuous */
		/* pr_err("msdc0: kmap not continuous %x %x %x\n",
			left,kaddr[i],kaddr[i-1]); */
		for (i = 0; i < totalpages; i++) {
			left = PAGE_SIZE;
			ptr = (u32 *) kaddr[i];

			if (i == 0) {
				left = PAGE_SIZE - sg->offset;
				ptr = (u32 *) (kaddr[i] + sg->offset);
			}
			if ((subpage != 0) && (i == (totalpages-1)))
				left = subpage;

#ifndef COMBINE_HM
check_fifo1:
			if (left == 0)
				continue;
#else
check_fifo1:
			if ((flag == 1) && (left == 0))
				continue;
			else if ((flag == 0) && (left == 0))
				goto check_fifo_end;
#endif

			if ((msdc_rxfifocnt() >= MSDC_FIFO_THD) &&
				(left >= MSDC_FIFO_THD)) {
				count = MSDC_FIFO_THD >> 2;
				do {
#ifdef MTK_MSDC_DUMP_FIFO
					pr_debug("0x%x ", msdc_fifo_read32());
#else
					*ptr++ = msdc_fifo_read32();
#endif
				} while (--count);
				left -= MSDC_FIFO_THD;
			} else if ((left < MSDC_FIFO_THD) &&
					msdc_rxfifocnt() >= left) {
				while (left > 3) {
#ifdef MTK_MSDC_DUMP_FIFO
					pr_debug("0x%x ", msdc_fifo_read32());
#else
					*ptr++ = msdc_fifo_read32();
#endif
					left -= 4;
				}

				u8ptr = (u8 *) ptr;
				while (left) {
#ifdef MTK_MSDC_DUMP_FIFO
					pr_debug("0x%x ", msdc_fifo_read8());
#else
					*u8ptr++ = msdc_fifo_read8();
#endif
					left--;
				}
			} else {
				ints = sdr_read32(MSDC_INT);
				latest_int_status[host->id] = ints;

				if (ints & MSDC_INT_DATCRCERR) {
					ERR_MSG("[msdc%d] DAT CRC error (0x%x), Left DAT: %d bytes\n",
						host->id, ints, left);
					data->error = (unsigned int)-EIO;
				} else if (ints & MSDC_INT_DATTMO) {
					ERR_MSG("[msdc%d] DAT TMO error (0x%x), Left DAT: %d bytes\n",
						host->id, ints, left);
					data->error = (unsigned int)-ETIMEDOUT;
				} else {
					goto skip_msdc_dump_and_reset1;
				}

#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT
				if ((atomic_read(&host->ot_work.autok_done)
					!= 0) ||
					(host->hw->host_function != MSDC_SDIO) ||
					(host->mclk <= 50*1000*1000))
					msdc_dump_info(host->id);
#else
				if (ints & MSDC_INT_DATTMO)
					msdc_dump_info(host->id);
#endif
				sdr_write32(MSDC_INT, ints);
				msdc_reset_hw(host->id);
				goto end;
			}

skip_msdc_dump_and_reset1:
			if (msdc_pio_abort(host, data, tmo))
				goto end;

			goto check_fifo1;
		}

check_fifo_end:
		if (hmpage != NULL) {
			/* pr_err("read msdc0:unmap %x\n", hmpage); */
			for (i = 0; i < totalpages; i++)
				kunmap(hmpage + i);

			hmpage = NULL;
		}
		size += msdc_sg_len(sg, host->dma_xfer);
		sg = sg_next(sg);
		num--;
	}
 end:
	if (hmpage != NULL) {
		for (i = 0; i < totalpages; i++)
			kunmap(hmpage + i);
		/* pr_err("msdc0 read unmap:\n"); */
	}
	data->bytes_xfered += size;
	N_MSG(FIO, "		PIO Read<%d>bytes", size);

	/* MSDC_CLR_BIT32(MSDC_INTEN, wints); */
#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT
	/* auto-K have not done or finished */
	if ((atomic_read(&host->ot_work.autok_done) == 0) &&
		(is_card_sdio(host)))
		return data->error;
#endif

	if (data->error)
		ERR_MSG("read pio data->error<%d> left<%d> size<%d>",
			data->error, left, size);
	return data->error;
}

/* please make sure won't using PIO when size >= 512
   which means, memory card block read/write won't using pio
   then don't need to handle the CMD12 when data error.
*/
int msdc_pio_write(struct msdc_host *host, struct mmc_data *data)
{
	void __iomem *base = host->base;
	struct scatterlist *sg = data->sg;
	u32 num = data->sg_len;
	u32 *ptr;
	u8 *u8ptr;
	u32 left = 0;
	u32 count, size = 0;
	u32 wints = MSDC_INTEN_DATTMO | MSDC_INTEN_DATCRCERR
		| MSDC_INTEN_XFER_COMPL;
	bool get_xfer_done = 0;
	unsigned long tmo = jiffies + DAT_TIMEOUT;
	u32 ints = 0;
	struct page *hmpage = NULL;
	int i = 0, totalpages = 0;
	int flag, subpage = 0;
	ulong kaddr[DIV_ROUND_UP(MAX_SGMT_SZ, PAGE_SIZE)];

	/* MSDC_CLR_BIT32(MSDC_INTEN, wints); */
	while (1) {
		if (!get_xfer_done) {
			ints = sdr_read32(MSDC_INT);
			latest_int_status[host->id] = ints;
			ints &= wints;
			sdr_write32(MSDC_INT, ints);
		}
		if (ints & MSDC_INT_DATTMO) {
			data->error = (unsigned int)-ETIMEDOUT;
			msdc_dump_info(host->id);
			msdc_reset_hw(host->id);
			break;
		} else if (ints & MSDC_INT_DATCRCERR) {
			data->error = (unsigned int)-EIO;
			msdc_reset_hw(host->id);
			break;
		} else if (ints & MSDC_INT_XFER_COMPL) {
			get_xfer_done = 1;
			if ((num == 0) && (left == 0))
				break;
		}
		if (msdc_pio_abort(host, data, tmo))
			goto end;
		if ((num == 0) && (left == 0))
			continue;
		left = msdc_sg_len(sg, host->dma_xfer);
		ptr = sg_virt(sg);

		flag = 0;

		/* High memory must kmap, if already mapped,
		   only add counter */
		if	((ptr != NULL) &&
			 !(PageHighMem((struct page *)(sg->page_link & ~0x3))))
		#ifndef COMBINE_HM
			goto check_fifo2;
		#else
			goto check_fifo1;
		#endif

		hmpage = (struct page *)(sg->page_link & ~0x3);
		totalpages = DIV_ROUND_UP(left + sg->offset, PAGE_SIZE);
		subpage = (left + sg->offset) % PAGE_SIZE;

		if ((subpage != 0) || (sg->offset != 0))
			N_MSG(OPS, "msdc%d: write size or start not align %x, %x, hmpage %lx,sg offset %x\n",
				host->id,
				subpage, left, (ulong)hmpage, sg->offset);

		/* Kmap all need pages, */
		for (i = 0; i < totalpages; i++) {
			kaddr[i] = (ulong) kmap(hmpage + i);
			if ((i > 0) && ((kaddr[i] - kaddr[i - 1]) != PAGE_SIZE))
				flag = 1;
			if (!kaddr[i])
				ERR_MSG("msdc0:kmap failed %lx\n", kaddr[i]);
		}

		ptr = sg_virt(sg);

		if (ptr == NULL)
			ERR_MSG("msdc0:write sg_virt %p\n", ptr);

		if (flag == 0)
		#ifndef COMBINE_HM
			goto check_fifo2;
		#else
			goto check_fifo1;
		#endif

		/* High memory and more than 1 va address va
		   may be not continuous */
		/*pr_err(ERR "msdc0:w kmap not continuous %x %x %x\n",
			left, kaddr[i], kaddr[i-1]);*/
		for (i = 0; i < totalpages; i++) {
			left = PAGE_SIZE;
			ptr = (u32 *) kaddr[i];

			if (i == 0) {
				left = PAGE_SIZE - sg->offset;
				ptr = (u32 *) (kaddr[i] + sg->offset);
			}
			if (subpage != 0 && (i == (totalpages - 1)))
				left = subpage;

#ifndef COMBINE_HM
check_fifo1:
			if (left == 0)
				continue;
#else
check_fifo1:
			if ((flag == 1) && (left == 0))
				continue;
			else if ((flag == 0) && (left == 0))
				goto check_fifo_end;
#endif

			if (left >= MSDC_FIFO_SZ && msdc_txfifocnt() == 0) {
				count = MSDC_FIFO_SZ >> 2;
				do {
					msdc_fifo_write32(*ptr);
					ptr++;
				} while (--count);
				left -= MSDC_FIFO_SZ;
			} else if (left < MSDC_FIFO_SZ &&
				   msdc_txfifocnt() == 0) {
				while (left > 3) {
					msdc_fifo_write32(*ptr);
					ptr++;
					left -= 4;
				}
				u8ptr = (u8 *) ptr;
				while (left) {
					msdc_fifo_write8(*u8ptr);
					u8ptr++;
					left--;
				}
			} else {
				ints = sdr_read32(MSDC_INT);
				latest_int_status[host->id] = ints;

				if (ints & MSDC_INT_DATCRCERR) {
					ERR_MSG("[msdc%d] DAT CRC error (0x%x), Left DAT: %d bytes\n",
						host->id, ints, left);
					data->error = (unsigned int)-EIO;
				} else if (ints & MSDC_INT_DATTMO) {
					ERR_MSG("[msdc%d] DAT TMO error (0x%x), Left DAT: %d bytes\n",
						host->id, ints, left);
					data->error = (unsigned int)-ETIMEDOUT;
				} else {
					goto skip_msdc_dump_and_reset1;
				}

#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT
				if ((atomic_read(&host->ot_work.autok_done)
					!= 0) ||
					(host->hw->host_function != MSDC_SDIO) ||
					(host->mclk <= 50*1000*1000))
					msdc_dump_info(host->id);
#else
				if (ints & MSDC_INT_DATTMO)
					msdc_dump_info(host->id);
#endif
				sdr_write32(MSDC_INT, ints);
				msdc_reset_hw(host->id);
				goto end;
			}

skip_msdc_dump_and_reset1:
			if (msdc_pio_abort(host, data, tmo))
				goto end;

			goto check_fifo1;
		}

check_fifo_end:
		if (hmpage != NULL) {
			for (i = 0; i < totalpages; i++)
				kunmap(hmpage + i);

			hmpage = NULL;

		}
		size += msdc_sg_len(sg, host->dma_xfer);
		sg = sg_next(sg);
		num--;
	}
 end:
	if (hmpage != NULL) {
		for (i = 0; i < totalpages; i++)
			kunmap(hmpage + i);
		pr_err("msdc0 write unmap 0x%x:\n", left);
	}
	data->bytes_xfered += size;
	N_MSG(FIO, "		PIO Write<%d>bytes", size);

#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT
	/* auto-K have not done or finished */
	if ((is_card_sdio(host)) &&
		(atomic_read(&host->ot_work.autok_done) == 0))
		return data->error;
#endif

	if (data->error)
		ERR_MSG("write pio data->error<%d> left<%d> size<%d>",
			data->error, left, size);

	/*MSDC_CLR_BIT32(MSDC_INTEN, wints);*/
	return data->error;
}

static void msdc_dma_start(struct msdc_host *host)
{
	void __iomem *base = host->base;
	u32 wints = MSDC_INTEN_XFER_COMPL | MSDC_INTEN_DATTMO
		| MSDC_INTEN_DATCRCERR;

	if (host->autocmd & MSDC_AUTOCMD12)
		wints |= MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO | MSDC_INT_ACMDRDY;
	sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_START, 1);

	sdr_set_bits(MSDC_INTEN, wints);

	N_MSG(DMA, "DMA start");

	if (host->data && host->data->flags & MMC_DATA_WRITE) {
		host->write_timeout_ms = min((long)max(host->data->blocks * 500,
			host->data->timeout_ns / 1000000), 270 * 1000l);
		schedule_delayed_work(&host->write_timeout, msecs_to_jiffies(host->write_timeout_ms));
		N_MSG(DMA, "DMA Data Busy Timeout:%u ms, schedule_delayed_work", host->write_timeout_ms);
	}
}

static void msdc_dma_stop(struct msdc_host *host)
{
	void __iomem *base = host->base;
	int retry = 500;
	int count = 1000;
	u32 wints = MSDC_INTEN_XFER_COMPL | MSDC_INTEN_DATTMO
		| MSDC_INTEN_DATCRCERR;

	if (host->data && host->data->flags & MMC_DATA_WRITE) {
		cancel_delayed_work(&host->write_timeout);
		N_MSG(DMA, "DMA Data Busy Timeout:%u ms, cancel_delayed_work", host->write_timeout_ms);
		host->write_timeout_ms = 0;
	}

	/* handle autocmd12 error in msdc_irq */
	if (host->autocmd & MSDC_AUTOCMD12)
		wints |= MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO | MSDC_INT_ACMDRDY;
	N_MSG(DMA, "DMA status: 0x%.8x", sdr_read32(MSDC_DMA_CFG));
	/* while (sdr_read32(MSDC_DMA_CFG) & MSDC_DMA_CFG_STS);*/

	sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP, 1);
	/* while (sdr_read32(MSDC_DMA_CFG) & MSDC_DMA_CFG_STS); */
	msdc_retry((sdr_read32(MSDC_DMA_CFG) & MSDC_DMA_CFG_STS),
		retry, count, host->id);
	if (retry == 0) {
		ERR_MSG("###### Failed to stop DMA! start dump######");
		dump_emi_info();
		mdelay(10);
		dump_axi_bus_info();
		mdelay(10);
#ifndef FPGA_PLATFORM
		/* dump_audio_info(); */
#endif
		mdelay(10);
		ERR_MSG("####### Failed to stop DMA! finish dump######");
		msdc_polling_axi_status(__LINE__, 1);
	}
	sdr_clr_bits(MSDC_INTEN, wints);	/* Not just xfer_comp */

	N_MSG(DMA, "DMA stop");
}

/* calc checksum */
static u8 msdc_dma_calcs(u8 *buf, u32 len)
{
	u32 i, sum = 0;

	for (i = 0; i < len; i++)
		sum += buf[i];

	return 0xFF - (u8) sum;
}

/* gpd bd setup + dma registers */
static int msdc_dma_config(struct msdc_host *host, struct msdc_dma *dma)
{
	void __iomem *base = host->base;
	u32 sglen = dma->sglen;
	/* u32 i, j, num, bdlen, arg, xfersz; */
	u32 j, num, bdlen;
	dma_addr_t dma_address;
	u32 dma_len;
	u8 blkpad, dwpad, chksum;
	struct scatterlist *sg = dma->sg;
	struct gpd_t *gpd;
	struct bd_t *bd;
	struct mmc_data *data = host->mrq->data;

	switch (dma->mode) {
	case MSDC_MODE_DMA_BASIC:
		if (host->hw->host_function == MSDC_SDIO)
			BUG_ON(dma->xfersz > 0xFFFFFFFF);
		else
			BUG_ON(dma->xfersz > 65535);

		BUG_ON(dma->sglen != 1);
		dma_address = sg_dma_address(sg);
		dma_len = msdc_sg_len(sg, host->dma_xfer);

		N_MSG(DMA, "DMA BASIC mode dma_len<%x> dma_address<%llx>",
			dma_len, (u64) dma_address);

		sdr_write32(MSDC_DMA_SA, dma_address);

		sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_LASTBUF, 1);
		sdr_write32(MSDC_DMA_LEN, dma_len);
		sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_BRUSTSZ, dma->burstsz);
		sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_MODE, 0);
		break;
	case MSDC_MODE_DMA_DESC:
		blkpad = (dma->flags & DMA_FLAG_PAD_BLOCK) ? 1 : 0;
		dwpad = (dma->flags & DMA_FLAG_PAD_DWORD) ? 1 : 0;
		chksum = (dma->flags & DMA_FLAG_EN_CHKSUM) ? 1 : 0;

		/* calculate the required number of gpd */
		num = (sglen + MAX_BD_PER_GPD - 1) / MAX_BD_PER_GPD;
		BUG_ON(num != 1);

		gpd = dma->gpd;
		bd = dma->bd;
		bdlen = data->sg_count;

		/* modify gpd */
		/* gpd->intr = 0; */
		gpd->hwo = 1;	/* hw will clear it */
		gpd->bdp = 1;
		gpd->chksum = 0;	/* need to clear first. */
		gpd->chksum = (chksum ? msdc_dma_calcs((u8 *) gpd, 16) : 0);

		/* modify bd */
		for_each_sg(data->sg, sg, bdlen, j) {
#ifdef MSDC_DMA_VIOLATION_DEBUG
			if (g_dma_debug[host->id]
			    && (msdc_latest_operation_type[host->id] == OPER_TYPE_READ)) {
				pr_debug("[%s] msdc%d do write 0x10000\n", __func__, host->id);
				dma_address = 0x10000;
			} else
				dma_address = sg_dma_address(sg);
#else
			dma_address = sg_dma_address(sg);
#endif

			dma_len = msdc_sg_len(sg, host->dma_xfer);

			N_MSG(DMA, "DMA DESC mode dma_len<%x> dma_address<%llx>",
				dma_len, (u64) dma_address);

			msdc_init_bd(&bd[j], blkpad, dwpad, dma_address, dma_len);

			if (j == bdlen - 1)
				bd[j].eol = 1;	/* the last bd */
			else
				bd[j].eol = 0;

			bd[j].chksum = 0;	/* checksume need to clear first */
			bd[j].chksum = (chksum ? msdc_dma_calcs((u8 *) (&bd[j]), 16) : 0);
		}
#ifdef MSDC_DMA_VIOLATION_DEBUG
		if (g_dma_debug[host->id]
		    && (msdc_latest_operation_type[host->id] == OPER_TYPE_READ))
			g_dma_debug[host->id] = 0;
#endif

		dma->used_gpd += 2;
		dma->used_bd += bdlen;

		sdr_set_field(MSDC_DMA_CFG, MSDC_DMA_CFG_DECSEN, chksum);
		sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_BRUSTSZ, dma->burstsz);
		sdr_set_field(MSDC_DMA_CTRL, MSDC_DMA_CTRL_MODE, 1);

		sdr_write32(MSDC_DMA_SA, (u32) dma->gpd_addr);
		break;

	default:
		break;
	}

	N_MSG(DMA, "DMA_CTRL = 0x%x", sdr_read32(MSDC_DMA_CTRL));
	N_MSG(DMA, "DMA_CFG  = 0x%x", sdr_read32(MSDC_DMA_CFG));
	N_MSG(DMA, "DMA_SA   = 0x%x", sdr_read32(MSDC_DMA_SA));

	return 0;
}

static void msdc_dma_setup(struct msdc_host *host, struct msdc_dma *dma,
	struct scatterlist *sg, unsigned int sglen)
{
	u32 max_dma_len = 0;

	BUG_ON(sglen > MAX_BD_NUM);	/* not support currently */
	dma->sg = sg;
	dma->flags = DMA_FLAG_EN_CHKSUM;
	/* dma->flags = DMA_FLAG_NONE; */ /* CHECKME */
	dma->sglen = sglen;
	dma->xfersz = host->xfer_size;
	dma->burstsz = MSDC_BRUST_64B;

	if (host->hw->host_function == MSDC_SDIO)
		max_dma_len = MAX_DMA_CNT_SDIO;
	else
		max_dma_len = MAX_DMA_CNT;

	if (sglen == 1 && msdc_sg_len(sg, host->dma_xfer) <= max_dma_len)
		dma->mode = MSDC_MODE_DMA_BASIC;
	else
		dma->mode = MSDC_MODE_DMA_DESC;

	N_MSG(DMA, "DMA mode<%d> sglen<%d> xfersz<%d>",
		dma->mode, dma->sglen, dma->xfersz);

	msdc_dma_config(host, dma);
}

/* set block number before send command */
static void msdc_set_blknum(struct msdc_host *host, u32 blknum)
{
	void __iomem *base = host->base;

	sdr_write32(SDC_BLK_NUM, blknum);
}

#define REQ_CMD_EIO  (0x1 << 0)
#define REQ_CMD_TMO  (0x1 << 1)
#define REQ_DAT_ERR  (0x1 << 2)
#define REQ_STOP_EIO (0x1 << 3)
#define REQ_STOP_TMO (0x1 << 4)
#define REQ_CMD23_EIO (0x1 << 5)
#define REQ_CMD23_TMO (0x1 << 6)
static void msdc_restore_info(struct msdc_host *host)
{
	void __iomem *base = host->base;
	int retry = 3;

	msdc_reset_hw(host->id);
	/* force bit5(BV18SDT) to 0 */
	host->saved_para.msdc_cfg = host->saved_para.msdc_cfg & 0xFFFFFFDF;
	sdr_write32(MSDC_CFG, host->saved_para.msdc_cfg);
	while (retry--) {
		msdc_set_mclk(host, host->saved_para.timing, host->saved_para.hz);
		if ((sdr_read32(MSDC_CFG) & 0xFFFFFF9F) !=
			(host->saved_para.msdc_cfg & 0xFFFFFF9F)) {
			ERR_MSG("msdc set_mclk is unstable (cur_cfg=%x,save_cfg=%x, cur_hz=%d, save_hz=%d)."
				, sdr_read32(MSDC_CFG), host->saved_para.msdc_cfg, host->mclk,
				host->saved_para.hz);
		} else
			break;
	}

	sdr_set_field(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
		host->saved_para.int_dat_latch_ck_sel);	/* for SDIO 3.0 */
	sdr_set_field(MSDC_PATCH_BIT0, MSDC_PB0_CKGEN_MSDC_DLY_SEL,
		host->saved_para.ckgen_msdc_dly_sel);	/* for SDIO 3.0 */
	sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR,
		host->saved_para.cmd_resp_ta_cntr);	/* for SDIO 3.0 */
	sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR,
		host->saved_para.wrdat_crc_ta_cntr);	/* for SDIO 3.0 */
	sdr_write32(MSDC_DAT_RDDLY0, host->saved_para.ddly0);
	sdr_write32(MSDC_PAD_TUNE0, host->saved_para.pad_tune0);
	sdr_write32(SDC_CFG, host->saved_para.sdc_cfg);
	sdr_set_field(MSDC_INTEN, MSDC_INT_SDIOIRQ,
		host->saved_para.inten_sdio_irq);	/* get INTEN status for SDIO */
	sdr_write32(MSDC_IOCON, host->saved_para.iocon);
	if (host->hw->host_function == MSDC_SDIO)
		host->mmc->pm_flags |= MMC_PM_KEEP_POWER;
}

static void msdc_update_cahce_status(struct msdc_host *host,
									struct mmc_request *mrq)
{
#ifdef MTK_MSDC_USE_CACHE
	struct mmc_command *cmd;
	struct mmc_data *data;

	cmd = mrq->cmd;
	if ((host->hw->host_function == MSDC_EMMC)
		&& host->mmc->card && (host->mmc->card->ext_csd.cache_ctrl & 0x1)) {
		if (cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK) {
			data = mrq->cmd->data;
			if ((host->error == 0) && mrq->sbc
			    && (((mrq->sbc->arg >> 24) & 0x1)
			    || ((mrq->sbc->arg >> 31) & 0x1))) {
				/* if reliable write, or force prg write emmc device success,
				 * do set cache flushed status.
				 */
				if (g_cache_status == CACHE_UN_FLUSHED) {
					g_cache_status = CACHE_FLUSHED;
					g_flush_data_size = 0;
				}
			} else if (host->error == 0) {
				/* if normal write emmc device successfully,
				 * do clear the cache flushed status
				 */
				if (g_cache_status == CACHE_FLUSHED) {
					g_cache_status = CACHE_UN_FLUSHED;
					N_MSG(CHE, "normal write happen,update g_cache_status = %d",
						g_cache_status);
				}
				g_flush_data_size += data->blocks;
			} else if (host->error) {
				g_flush_data_size += data->blocks;
				ERR_MSG("write error happend, g_flush_data_size=%lld",
					g_flush_data_size);
			}
		} else if ((cmd->opcode == MMC_SWITCH)
		   && (((cmd->arg >> 16) & 0xFF) == EXT_CSD_FLUSH_CACHE)
		   && (((cmd->arg >> 8) & 0x1)) && !g_bypass_flush) {
			if (host->error == 0) {
				/* if flush cache of emmc device successfully,
				 * do set the cache flushed status
				 */
				g_cache_status = CACHE_FLUSHED;
				N_MSG(CHE, "flush happend, update g_cache_status = %d;"
					"g_flush_data_size=%lld", g_cache_status,
					g_flush_data_size);
				g_flush_data_size = 0;
			} else {
				g_flush_error_happend = 1;
			}
		}
	}
#endif
}

static int msdc_do_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;
	u32 l_autocmd23_is_set = 0;
#ifdef MTK_MSDC_USE_CMD23
	u32 l_card_no_cmd23 = 0;
#endif
	void __iomem *base = host->base;
	/* u32 intsts = 0; */
	int dma = 0, read = 1, dir = DMA_FROM_DEVICE, send_type = 0;
	u32 map_sg = 0;
#ifdef MTK_MSDC_USE_CACHE
	u32 l_force_prg = 0;

	g_bypass_flush = 0;
#endif
	unsigned long pio_tmo;
	unsigned int left = 0;

#define SND_DAT 0
#define SND_CMD 1

	if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ)) {
		/* mb(); */
		if (host->saved_para.hz) {
			if (host->saved_para.suspend_flag) {
				ERR_MSG("msdc resume[s] cur_cfg=%x, save_cfg=%x,cur_hz=%d, save_hz=%d"
					, sdr_read32(MSDC_CFG),	host->saved_para.msdc_cfg,
					host->mclk, host->saved_para.hz);
				host->saved_para.suspend_flag = 0;
				msdc_restore_info(host);
			} else if ((host->saved_para.msdc_cfg != 0) &&
				((sdr_read32(MSDC_CFG) & 0xFFFFFF9F) !=
				(host->saved_para.msdc_cfg & 0xFFFFFF9F))) {
				ERR_MSG("msdc resume[ns] cur_cfg=%x, save_cfg=%x,cur_hz=%d, save_hz=%d"
					, sdr_read32(MSDC_CFG),	host->saved_para.msdc_cfg,
					host->mclk, host->saved_para.hz);
				msdc_restore_info(host);
			}
		}
	}
#if (MSDC_DATA1_INT == 1)
	if (host->hw->flags & MSDC_SDIO_IRQ) {
		/* if((!u_sdio_irq_counter) && (!u_msdc_irq_counter))
		ERR_MSG("Ahsin u_sdio_irq_counter=%d, u_msdc_irq_counter=%d "
		"int_sdio_irq_enable=%d SDC_CFG=%x MSDC_INTEN=%x MSDC_INT=%x "
		"MSDC_PATCH_BIT0=%x", u_sdio_irq_counter, u_msdc_irq_counter,
		int_sdio_irq_enable, sdr_read32(SDC_CFG),sdr_read32(MSDC_INTEN),
		sdr_read32(MSDC_INT),sdr_read32(MSDC_PATCH_BIT0)); */
		if ((u_sdio_irq_counter > 0) && ((u_sdio_irq_counter % 800) == 0))
			ERR_MSG("Ahsin sdio_irq=%d, msdc_irq=%d  SDC_CFG=%x MSDC_INTEN=%x MSDC_INT=%x ",
			u_sdio_irq_counter,	u_msdc_irq_counter, sdr_read32(SDC_CFG),
			sdr_read32(MSDC_INTEN),	sdr_read32(MSDC_INT));
	}
#endif

	BUG_ON(mmc == NULL);
	BUG_ON(mrq == NULL);

	host->error = 0;
	atomic_set(&host->abort, 0);

	cmd = mrq->cmd;
	data = mrq->cmd->data;

	/* check msdc is work ok
	 * rule is RX/TX fifocnt must be zero after last request
	 * if find abnormal, try to reset msdc first
	 */
	if (msdc_txfifocnt() || msdc_rxfifocnt()) {
		pr_err("[SD%d] register abnormal,please check!\n", host->id);
		msdc_reset_hw(host->id);
	}

	if (!data) {
		send_type = SND_CMD;

#ifdef MTK_MSDC_USE_CACHE
		if ((host->hw->host_function == MSDC_EMMC)
			&& (cmd->opcode == MMC_SWITCH)
			&& (((cmd->arg >> 16) & 0xFF) == EXT_CSD_FLUSH_CACHE)
			&& (((cmd->arg >> 8) & 0x1))) {
			if (g_cache_status == CACHE_FLUSHED) {
				N_MSG(CHE, "bypass flush command, g_cache_status=%d",
					g_cache_status);
				g_bypass_flush = 1;
				goto done;
			}
		}
#endif

		if (msdc_do_command(host, cmd, 0, CMD_TIMEOUT))
			goto done;

/* #ifdef CONFIG_MTK_EMMC_SUPPORT */
#if 0
		if (host->hw->host_function == MSDC_EMMC &&
		    host->hw->boot == MSDC_BOOT_EN &&
		    cmd->opcode == MMC_SWITCH
		    && (((cmd->arg >> 16) & 0xFF) == EXT_CSD_PART_CONFIG))
			partition_access = (char)((cmd->arg >> 8) & 0x07);
#endif
		if ((host->hw->host_function == MSDC_EMMC) &&
			(cmd->opcode == MMC_ALL_SEND_CID))
			emmc_id = UNSTUFF_BITS(cmd->resp, 120, 8);

#ifdef MTK_EMMC_ETT_TO_DRIVER
		if ((host->hw->host_function == MSDC_EMMC)
			&& (cmd->opcode == MMC_ALL_SEND_CID)) {
			m_id = UNSTUFF_BITS(cmd->resp, 120, 8);
			pro_name[0] = UNSTUFF_BITS(cmd->resp, 96, 8);
			pro_name[1] = UNSTUFF_BITS(cmd->resp, 88, 8);
			pro_name[2] = UNSTUFF_BITS(cmd->resp, 80, 8);
			pro_name[3] = UNSTUFF_BITS(cmd->resp, 72, 8);
			pro_name[4] = UNSTUFF_BITS(cmd->resp, 64, 8);
			pro_name[5] = UNSTUFF_BITS(cmd->resp, 56, 8);
			/* pro_name[6]    = '\0'; */
		}
#endif

	} else {
		BUG_ON(data->blksz > HOST_MAX_BLKSZ);
		send_type = SND_DAT;

#ifdef MTK_MSDC_USE_CACHE
		if ((host->hw->host_function == MSDC_EMMC) && host->mmc->card
		    && (host->mmc->card->ext_csd.cache_ctrl & 0x1)
		    && (cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK))
			l_force_prg = !msdc_can_apply_cache(cmd->arg, data->blocks);
#endif

		data->error = 0;
		read = data->flags & MMC_DATA_READ ? 1 : 0;
		msdc_latest_operation_type[host->id] =
			read ? OPER_TYPE_READ : OPER_TYPE_WRITE;
		host->data = data;
		host->xfer_size = data->blocks * data->blksz;
		host->blksz = data->blksz;

		/* deside the transfer mode */
		if (drv_mode[host->id] == MODE_PIO) {
			host->dma_xfer = dma = 0;
			msdc_latest_transfer_mode[host->id] = TRAN_MOD_PIO;
		} else if (drv_mode[host->id] == MODE_DMA) {
			host->dma_xfer = dma = 1;
			msdc_latest_transfer_mode[host->id] = TRAN_MOD_DMA;
		} else if (drv_mode[host->id] == MODE_SIZE_DEP) {
			host->dma_xfer = dma =
				((host->xfer_size >= dma_size[host->id]) ? 1 : 0);
			msdc_latest_transfer_mode[host->id] =
				dma ? TRAN_MOD_DMA : TRAN_MOD_PIO;
		}
		if (read) {
			if ((host->timeout_ns != data->timeout_ns)
				|| (host->timeout_clks != data->timeout_clks)) {
				msdc_set_timeout(host, data->timeout_ns, data->timeout_clks);
			}
		}

		msdc_set_blknum(host, data->blocks);
		/* msdc_clr_fifo();  */ /* no need */

#ifdef MTK_MSDC_USE_CMD23
		if (0 == (host->autocmd & MSDC_AUTOCMD23)) {
			/* start the cmd23 first, mrq->sbc is NULL with single r/w */
			if (mrq->sbc) {
				host->autocmd &= ~MSDC_AUTOCMD12;

				if (host->hw->host_function == MSDC_EMMC) {
#ifdef MTK_MSDC_USE_CACHE
					if (l_force_prg && !((mrq->sbc->arg >> 31) & 0x1))
						mrq->sbc->arg |= (1 << 24);
#endif
				}

				if (msdc_command_start(host, mrq->sbc, 0, CMD_TIMEOUT) != 0)
					goto done;

				/* then wait command done */
				if (msdc_command_resp_polling(host, mrq->sbc, 0, CMD_TIMEOUT))
					goto stop;
			} else {
				/* some sd card may not support cmd23,
				 * some emmc card have problem with cmd23, so use cmd12 here */
				if (host->hw->host_function != MSDC_SDIO)
					host->autocmd |= MSDC_AUTOCMD12;
			}
		} else {
			/* enable auto cmd23 */
			if (mrq->sbc) {
				host->autocmd &= ~MSDC_AUTOCMD12;
				if (host->hw->host_function == MSDC_EMMC) {
#ifdef MTK_MSDC_USE_CACHE
					if (l_force_prg && !((mrq->sbc->arg >> 31) & 0x1))
						mrq->sbc->arg |= (1 << 24);
#endif
				}
			} else {
				/* some sd card may not support cmd23,
				 * some emmc card have problem with cmd23, so use cmd12 here */
				if (host->hw->host_function != MSDC_SDIO) {
					host->autocmd &= ~MSDC_AUTOCMD23;
					host->autocmd |= MSDC_AUTOCMD12;
					l_card_no_cmd23 = 1;
				}
			}
		}
#endif				/* end of MTK_MSDC_USE_CMD23 */

		if (dma) {
			msdc_dma_on();	/* enable DMA mode first!! */
			init_completion(&host->xfer_done);

#ifndef MTK_MSDC_USE_CMD23
			/* start the command first */
			if (host->hw->host_function != MSDC_SDIO)
				host->autocmd |= MSDC_AUTOCMD12;
#endif
			if (msdc_command_start(host, cmd, 0, CMD_TIMEOUT) != 0)
				goto done;

			dir = read ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
			data->sg_count = dma_map_sg(mmc_dev(mmc), data->sg, data->sg_len, dir);
			map_sg = 1;

			/* then wait command done */
			if (msdc_command_resp_polling(host, cmd, 0, CMD_TIMEOUT))
				goto stop;

			/* for read, the data coming too fast, then CRC error
			 * start DMA no business with CRC.
			 * init_completion(&host->xfer_done); */
			msdc_dma_setup(host, &host->dma, data->sg, data->sg_len);
			msdc_dma_start(host);
#ifdef STO_LOG
			if (unlikely(dumpMSDC()))
				AddStorageTrace(STORAGE_LOGGER_MSG_MSDC_DO, msdc_do_request,
					"msdc_dma_start", host->xfer_size);
#endif
			spin_unlock(&host->lock);
			if (!wait_for_completion_timeout(&host->xfer_done, DAT_TIMEOUT)) {
				ERR_MSG("XXX CMD<%d> ARG<0x%x> wait xfer_done<%d> timeout!!",
					cmd->opcode, cmd->arg, data->blocks * data->blksz);

				host->sw_timeout++;
#ifdef STO_LOG
				if (unlikely(dumpMSDC()))
					AddStorageTrace(STORAGE_LOGGER_MSG_MSDC_DO, msdc_do_request,
						"msdc_dma ERR", host->xfer_size);
#endif
				msdc_dump_info(host->id);
				data->error = (unsigned int)-ETIMEDOUT;
				msdc_reset(host->id);
			}
			spin_lock(&host->lock);
			msdc_dma_stop(host);
			if ((mrq->data && mrq->data->error)
				|| ((host->autocmd & MSDC_AUTOCMD12)
				&& mrq->stop && mrq->stop->error)
			    || (mrq->sbc && (mrq->sbc->error != 0)
				&& (host->autocmd & MSDC_AUTOCMD23))) {
				msdc_clr_fifo(host->id);
				msdc_clr_int();
			}
#ifdef STO_LOG
			if (unlikely(dumpMSDC()))
				AddStorageTrace(STORAGE_LOGGER_MSG_MSDC_DO, msdc_do_request,
					"msdc_dma_stop");
#endif
		} else {
			/* Turn off dma */
			if (is_card_sdio(host)) {
				msdc_reset_hw(host->id);
				msdc_dma_off();
				data->error = 0;
			}
			/* Firstly: send command
			 * need ask the designer, how about autocmd12
			 * or autocmd23 with pio mode
			 */
			host->autocmd &= ~MSDC_AUTOCMD12;

			l_autocmd23_is_set = 0;
			if (host->autocmd & MSDC_AUTOCMD23) {
				l_autocmd23_is_set = 1;
				host->autocmd &= ~MSDC_AUTOCMD23;
			}

			host->dma_xfer = 0;
			if (msdc_do_command(host, cmd, 0, CMD_TIMEOUT))
				goto stop;

			/* Secondly: pio data phase */
			if (read) {
#ifdef MTK_MSDC_DUMP_FIFO
				pr_debug("[%s]: start pio read\n", __func__);
#endif
				if (msdc_pio_read(host, data)) {
					msdc_gate_clock(host, 0);
					msdc_ungate_clock(host);
					goto stop;	/* need cmd12 */
				}
			} else {
#ifdef MTK_MSDC_DUMP_FIFO
				pr_debug("[%s]: start pio write\n", __func__);
#endif
				if (msdc_pio_write(host, data)) {
					msdc_gate_clock(host, 0);
					msdc_ungate_clock(host);
					goto stop;
				}
			}

			/* For write case: make sure contents in fifo flushed to device */
			if (!read) {
				pio_tmo = jiffies + DAT_TIMEOUT;
				while (1) {
					left = msdc_txfifocnt();
					if (left == 0)
						break;
					if (msdc_pio_abort(host, data, pio_tmo))
						break;
				}
			}

		} /* PIO mode */
 stop:
		/* pio mode will disable autocmd23 */
		if (l_autocmd23_is_set == 1) {
			l_autocmd23_is_set = 0;
			host->autocmd |= MSDC_AUTOCMD23;
		}
#ifndef MTK_MSDC_USE_CMD23
		/* Last: stop transfer */
		if (data && data->stop) {
			if (!((cmd->error == 0) && (data->error == 0)
			      && (host->autocmd & MSDC_AUTOCMD12)
			      && (cmd->opcode == MMC_READ_MULTIPLE_BLOCK
			      || cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK))) {
				if (msdc_do_command(host, data->stop, 0, CMD_TIMEOUT))
					goto done;
			}
		}
#else
		if (host->hw->host_function == MSDC_EMMC) {
			if (data && data->stop) {
				/* multi r/w with no cmd23 and no autocmd12,need send cmd12
				 * manual if PIO mode and autocmd23 enable, cmd12 need send,
				 * because autocmd23 is disable under PIO
				 */
				if ((((mrq->sbc == NULL) && !(host->autocmd & MSDC_AUTOCMD12))
					|| (!dma && mrq->sbc && (host->autocmd & MSDC_AUTOCMD23)))
					&& (cmd->opcode == MMC_READ_MULTIPLE_BLOCK
				     || cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK)) {
					if (msdc_do_command(host, data->stop, 0, CMD_TIMEOUT))
						goto done;
				}
			}

		} else {
			/* for non emmc card, use old flow */
			if (data && data->stop) {
				if (!((cmd->error == 0) && (data->error == 0)
				      && (host->autocmd & MSDC_AUTOCMD12)
				      && (cmd->opcode == MMC_READ_MULTIPLE_BLOCK
					  || cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK))) {
					if (msdc_do_command(host, data->stop, 0, CMD_TIMEOUT))
						goto done;
				}
			}
		}
#endif

	}

 done:

#ifdef MTK_MSDC_USE_CMD23
	/* for msdc use cmd23, but card not supported(sbc is NULL),
	 * need enable autocmd23 for next request
	 */
	if (1 == l_card_no_cmd23) {
		if (host->hw->host_function != MSDC_SDIO) {
			host->autocmd |= MSDC_AUTOCMD23;
			host->autocmd &= ~MSDC_AUTOCMD12;
			l_card_no_cmd23 = 0;
		}
	}
#endif

	if (data != NULL) {
		host->data = NULL;
		host->dma_xfer = 0;

#if 0				/* read MBR */
#ifdef CONFIG_MTK_EMMC_SUPPORT
		{
			char *ptr = sg_virt(data->sg);
			int i;

			if (cmd->arg == 0x0 && (cmd->opcode == MMC_READ_SINGLE_BLOCK
				|| cmd->opcode == MMC_READ_MULTIPLE_BLOCK)) {
				pr_debug("XXXX CMD<%d> ARG<%X> offset = <%d> data<%s %s>;"
					"sg <%p> ptr <%p> blksz<%d> block<%d> error<%d>\n",
				     cmd->opcode, cmd->arg, offset, (dma ? "dma" : "pio"),
				     (read ? "read " : "write"), data->sg, ptr, data->blksz,
				     data->blocks, data->error);

				for (i = 0; i < 512; i++) {
					if (i % 32 == 0)
						pr_debug("\n");
					pr_debug(" %2x ", ptr[i]);
				}
			}
		}
#endif
#endif				/* end read MBR */
		if (dma != 0) {
			msdc_dma_off();
			host->dma.used_bd = 0;
			host->dma.used_gpd = 0;
			if (map_sg == 1) {
				/*if(data->error == 0){
				   int retry = 3;
				   int count = 1000;
				   msdc_retry(host->dma.gpd->hwo,retry,count,host->id);
				   } */
				dma_unmap_sg(mmc_dev(mmc), data->sg, data->sg_len, dir);
			}
		}

/* #ifdef CONFIG_MTK_EMMC_SUPPORT */
#if 0
		if ((cmd->opcode == MMC_SEND_EXT_CSD) &&
			(host->hw->host_function == MSDC_EMMC))
			msdc_get_ext_csd(ext_csd, data, host);
#endif
		host->blksz = 0;

		N_MSG(OPS, "CMD<%d> data<%s %s> blksz<%d> block<%d> error<%d>",
			cmd->opcode, (dma ? "dma" : "pio"), (read ? "read " : "write"),
			data->blksz, data->blocks, data->error);

		if (!(is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))) {
			if ((cmd->opcode != 17) && (cmd->opcode != 18)
				&& (cmd->opcode != 24) && (cmd->opcode != 25)) {
				N_MSG(NRW, "CMD<%3d> arg<0x%8x> Resp<0x%8x> data<%s> size<%d>",
				      cmd->opcode, cmd->arg, cmd->resp[0],
				      (read ? "read " : "write"), data->blksz * data->blocks);
			} else {
				N_MSG(RW, "CMD<%3d> arg<0x%8x> Resp<0x%8x> block<%d>",
					cmd->opcode, cmd->arg, cmd->resp[0], data->blocks);
			}
		}
	} else {
		if (!(is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))) {
			if (cmd->opcode != 13) /* by pass CMD13 */
				N_MSG(NRW, "CMD<%3d> arg<0x%8x> resp<%8x %8x %8x %8x>",
					cmd->opcode, cmd->arg, cmd->resp[0], cmd->resp[1],
					cmd->resp[2], cmd->resp[3]);
		}
	}

	if (mrq->cmd->error == (unsigned int)-EIO) {
		if (((cmd->opcode == MMC_SELECT_CARD)
			|| (cmd->opcode == MMC_SLEEP_AWAKE))
			&& ((host->hw->host_function == MSDC_EMMC)
			|| (host->hw->host_function == MSDC_SD))) {
			/* should be deleted in new platform,
			 * as the state verify function has applied
			 */
			mrq->cmd->error = 0x0;
		} else {
			host->error |= REQ_CMD_EIO;
			sdio_tune_flag |= 0x1;

			if (mrq->cmd->opcode == SD_IO_RW_EXTENDED)
				sdio_tune_flag |= 0x1;
		}
	}

	if (mrq->cmd->error == (unsigned int)-ETIMEDOUT) {
		if ((mrq->cmd->opcode == MMC_SLEEP_AWAKE) && emmc_do_sleep_awake) {
			emmc_sleep_failed = 1;
			if (mrq->cmd->arg & (1 << 15)) {
				mrq->cmd->error = 0x0;
				pr_err("eMMC sleep CMD5 TMO will reinit...\n");
			} else {
				host->error |= REQ_CMD_TMO;
			}
		} else {
			host->error |= REQ_CMD_TMO;
		}
	}

	if (mrq->data && mrq->data->error) {
		host->error |= REQ_DAT_ERR;
		sdio_tune_flag |= 0x10;

		if (mrq->data->flags & MMC_DATA_READ)
			sdio_tune_flag |= 0x80;
		else
			sdio_tune_flag |= 0x40;
	}
#ifdef MTK_MSDC_USE_CMD23
	if (mrq->sbc && (mrq->sbc->error == (unsigned int)-EIO))
		host->error |= REQ_CMD_EIO;
	if (mrq->sbc && (mrq->sbc->error == (unsigned int)-ETIMEDOUT)) {
#ifdef CONFIG_MTK_AEE_FEATURE
		aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_NE_JBT_TRACES
			| DB_OPT_DISPLAY_HANG_DUMP, "\n@eMMC FATAL ERROR@\n",
			"eMMC fatal error");
#endif
		host->error |= REQ_CMD_TMO;
	}
#endif

	if (mrq->stop && (mrq->stop->error == (unsigned int)-EIO))
		host->error |= REQ_STOP_EIO;
	if (mrq->stop && (mrq->stop->error == (unsigned int)-ETIMEDOUT))
		host->error |= REQ_STOP_TMO;
	/* if (host->error) ERR_MSG("host->error<%d>", host->error); */
#ifdef SDIO_ERROR_BYPASS
	if (is_card_sdio(host) && !host->error)
		host->sdio_error = 0;
#endif

	msdc_update_cahce_status(host, mrq);
	return host->error;
}

static int msdc_tune_rw_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;

#ifdef MTK_MSDC_USE_CMD23
	u32 l_autocmd23_is_set = 0;
#endif

	void __iomem *base = host->base;
	/* u32 intsts = 0; */
	/* unsigned int left=0; */
	int read = 1, dma = 1;	/* dir = DMA_FROM_DEVICE, send_type=0, */

#define SND_DAT 0
#define SND_CMD 1

	BUG_ON(mmc == NULL);
	BUG_ON(mrq == NULL);

	/* host->error = 0; */
	atomic_set(&host->abort, 0);

	cmd = mrq->cmd;
	data = mrq->cmd->data;

	/* check msdc is work ok.
	 * rule is RX/TX fifocnt must be zero after last request
	 * if find abnormal, try to reset msdc first
	 */
	if (msdc_txfifocnt() || msdc_rxfifocnt()) {
		pr_err("[SD%d] register abnormal,please check!\n", host->id);
		msdc_reset_hw(host->id);
	}

	BUG_ON(data->blksz > HOST_MAX_BLKSZ);
	/* send_type=SND_DAT; */

	data->error = 0;
	read = data->flags & MMC_DATA_READ ? 1 : 0;
	msdc_latest_operation_type[host->id] = read ?
		OPER_TYPE_READ : OPER_TYPE_WRITE;
	host->data = data;
	host->xfer_size = data->blocks * data->blksz;
	host->blksz = data->blksz;
	host->dma_xfer = 1;

	/* deside the transfer mode */
	/*
	   if (drv_mode[host->id] == MODE_PIO) {
	   host->dma_xfer = dma = 0;
	   msdc_latest_transfer_mode[host->id] = TRAN_MOD_PIO;
	   } else if (drv_mode[host->id] == MODE_DMA) {
	   host->dma_xfer = dma = 1;
	   msdc_latest_transfer_mode[host->id] = TRAN_MOD_DMA;
	   } else if (drv_mode[host->id] == MODE_SIZE_DEP) {
	   host->dma_xfer = dma = ((host->xfer_size >= dma_size[host->id]) ? 1 : 0);
	   msdc_latest_transfer_mode[host->id] = dma ? TRAN_MOD_DMA: TRAN_MOD_PIO;
	   }
	 */
	if (read) {
		if ((host->timeout_ns != data->timeout_ns)
		    || (host->timeout_clks != data->timeout_clks)) {
			msdc_set_timeout(host, data->timeout_ns, data->timeout_clks);
		}
	}

	msdc_set_blknum(host, data->blocks);
	/* msdc_clr_fifo(); */ /* no need */
	msdc_dma_on();		/* enable DMA mode first!! */
	init_completion(&host->xfer_done);

	/* start the command first */
#ifndef MTK_MSDC_USE_CMD23
	if (host->hw->host_function != MSDC_SDIO)
		host->autocmd |= MSDC_AUTOCMD12;
#else
	if (host->hw->host_function != MSDC_SDIO) {
		host->autocmd |= MSDC_AUTOCMD12;

		/* disable autocmd23 in error tuning flow */
		l_autocmd23_is_set = 0;
		if (host->autocmd & MSDC_AUTOCMD23) {
			l_autocmd23_is_set = 1;
			host->autocmd &= ~MSDC_AUTOCMD23;
		}
	}
#endif

	if (msdc_command_start(host, cmd, 0, CMD_TIMEOUT))
		goto done;

	/* then wait command done */
	if (msdc_command_resp_polling(host, cmd, 0, CMD_TIMEOUT))
		goto stop;

	/* for read, the data coming too fast, then CRC error
	 * start DMA no business with CRC. */
	msdc_dma_setup(host, &host->dma, data->sg, data->sg_len);
	msdc_dma_start(host);
	/* ERR_MSG("1.Power cycle enable(%d)",host->power_cycle_enable); */
#ifdef STO_LOG
	if (unlikely(dumpMSDC()))
		AddStorageTrace(STORAGE_LOGGER_MSG_MSDC_DO, msdc_tune_rw_request,
		"msdc_dma_start", host->xfer_size);
#endif
	spin_unlock(&host->lock);
	if (!wait_for_completion_timeout(&host->xfer_done, DAT_TIMEOUT)) {
		ERR_MSG("XXX CMD<%d> ARG<0x%x> wait xfer_done<%d> timeout!!",
			cmd->opcode, cmd->arg, data->blocks * data->blksz);
		host->sw_timeout++;
#ifdef STO_LOG
		if (unlikely(dumpMSDC()))
			AddStorageTrace(STORAGE_LOGGER_MSG_MSDC_DO, msdc_tune_rw_request,
			"msdc_dma ERR",	host->xfer_size);
#endif
		msdc_dump_info(host->id);
		data->error = (unsigned int)-ETIMEDOUT;
		msdc_reset(host->id);
	}
	spin_lock(&host->lock);
	/* ERR_MSG("2.Power cycle enable(%d)",host->power_cycle_enable); */
	msdc_dma_stop(host);
	if ((mrq->data && mrq->data->error)
	    || (host->autocmd & MSDC_AUTOCMD12 && mrq->stop && mrq->stop->error)
	    || (mrq->sbc && (mrq->sbc->error != 0)
	    && (host->autocmd & MSDC_AUTOCMD23))) {
		msdc_clr_fifo(host->id);
		msdc_clr_int();
	}
#ifdef STO_LOG
	if (unlikely(dumpMSDC()))
		AddStorageTrace(STORAGE_LOGGER_MSG_MSDC_DO, msdc_tune_rw_request,
		"msdc_dma_stop");
#endif

 stop:
	/* Last: stop transfer */

	if (data->stop) {
		if (!((cmd->error == 0) && (data->error == 0)
			&& (host->autocmd == MSDC_AUTOCMD12)
			&& (cmd->opcode == MMC_READ_MULTIPLE_BLOCK
			|| cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK))) {
			if (msdc_do_command(host, data->stop, 0, CMD_TIMEOUT) != 0)
				goto done;
		}
	}

 done:
	host->data = NULL;
	host->dma_xfer = 0;
	msdc_dma_off();
	host->dma.used_bd = 0;
	host->dma.used_gpd = 0;
	host->blksz = 0;

	N_MSG(OPS, "CMD<%d> data<%s %s> blksz<%d> block<%d> error<%d>", cmd->opcode,
	      (dma ? "dma" : "pio"), (read ? "read " : "write"), data->blksz,
	      data->blocks, data->error);

	if (!(is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))) {
		if ((cmd->opcode != 17) && (cmd->opcode != 18) && (cmd->opcode != 24)
		    && (cmd->opcode != 25))
			N_MSG(NRW, "CMD<%3d> arg<0x%8x> Resp<0x%8x> data<%s> size<%d>",
				cmd->opcode, cmd->arg, cmd->resp[0], (read ? "read " : "write"),
				data->blksz * data->blocks);
		else
			N_MSG(RW, "CMD<%3d> arg<0x%8x> Resp<0x%8x> block<%d>", cmd->opcode,
			      cmd->arg, cmd->resp[0], data->blocks);
	} else {
		if (!(is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))) {
			if (cmd->opcode != 13) /* by pass CMD13 */
				N_MSG(NRW, "CMD<%3d> arg<0x%8x> resp<%8x %8x %8x %8x>",
					cmd->opcode, cmd->arg, cmd->resp[0], cmd->resp[1],
					cmd->resp[2], cmd->resp[3]);
		}
	}
	host->error = 0;
	if (mrq->cmd->error == (unsigned int)-EIO) {
		if (((cmd->opcode == MMC_SELECT_CARD)
			|| (cmd->opcode == MMC_SLEEP_AWAKE))
			&& ((host->hw->host_function == MSDC_EMMC)
		    || (host->hw->host_function == MSDC_SD))) {
			/* should be deleted in new platform,
			 * as the state verify function has applied.
			 */
			mrq->cmd->error = 0x0;
		} else {
			host->error |= REQ_CMD_EIO;
		}
	}
	if (mrq->cmd->error == (unsigned int)-ETIMEDOUT)
		host->error |= REQ_CMD_TMO;
	if (mrq->data && (mrq->data->error))
		host->error |= REQ_DAT_ERR;
	if (mrq->stop && (mrq->stop->error == (unsigned int)-EIO))
		host->error |= REQ_STOP_EIO;
	if (mrq->stop && (mrq->stop->error == (unsigned int)-ETIMEDOUT))
		host->error |= REQ_STOP_TMO;

#ifdef MTK_MSDC_USE_CMD23
	if (l_autocmd23_is_set == 1) {
		/* restore the value */
		host->autocmd |= MSDC_AUTOCMD23;
	}
#endif
	return host->error;
}

static void msdc_pre_req(struct mmc_host *mmc, struct mmc_request *mrq,
	bool is_first_req)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data;
	struct mmc_command *cmd = mrq->cmd;
	int read = 1, dir = DMA_FROM_DEVICE;

	BUG_ON(!cmd);
	data = mrq->data;
	if (data)
		data->host_cookie = MSDC_COOKIE_ASYNC;
	if (data && (cmd->opcode == MMC_READ_SINGLE_BLOCK
	    || cmd->opcode == MMC_READ_MULTIPLE_BLOCK
		|| cmd->opcode == MMC_WRITE_BLOCK
		|| cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK)) {
		host->xfer_size = data->blocks * data->blksz;
		read = data->flags & MMC_DATA_READ ? 1 : 0;
		if (drv_mode[host->id] == MODE_PIO) {
			data->host_cookie |= MSDC_COOKIE_PIO;
			msdc_latest_transfer_mode[host->id] = TRAN_MOD_PIO;
		} else if (drv_mode[host->id] == MODE_DMA) {

			msdc_latest_transfer_mode[host->id] = TRAN_MOD_DMA;
		} else if (drv_mode[host->id] == MODE_SIZE_DEP) {
			if (host->xfer_size < dma_size[host->id]) {
				data->host_cookie |= MSDC_COOKIE_PIO;
				msdc_latest_transfer_mode[host->id] = TRAN_MOD_PIO;
			} else {
				msdc_latest_transfer_mode[host->id] = TRAN_MOD_DMA;
			}
		}
		if (msdc_async_use_dma(data->host_cookie)) {
			dir = read ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
			data->sg_count = dma_map_sg(mmc_dev(mmc), data->sg, data->sg_len, dir);
		}
		N_MSG(OPS, "CMD<%d> ARG<0x%x>data<%s %s> blksz<%d> block<%d> error<%d>",
		      mrq->cmd->opcode, mrq->cmd->arg,
		      (data->host_cookie ? "dma" : "pio"), (read ? "read " : "write"),
		      data->blksz, data->blocks, data->error);
	}
}

static void msdc_dma_clear(struct msdc_host *host)
{
	void __iomem *base = host->base;

	host->data = NULL;
	host->mrq = NULL;
	host->dma_xfer = 0;
	msdc_dma_off();
	host->dma.used_bd = 0;
	host->dma.used_gpd = 0;
	host->blksz = 0;
}

static void msdc_post_req(struct mmc_host *mmc, struct mmc_request *mrq,
	int err)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data;
	/* struct mmc_command *cmd = mrq->cmd; */
	int read = 1, dir = DMA_FROM_DEVICE;

	data = mrq->data;
	if (data && (msdc_async_use_dma(data->host_cookie))) {
		host->xfer_size = data->blocks * data->blksz;
		read = data->flags & MMC_DATA_READ ? 1 : 0;
		dir = read ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
		dma_unmap_sg(mmc_dev(mmc), data->sg, data->sg_len, dir);
		data->host_cookie = 0;
		N_MSG(OPS, "CMD<%d> ARG<0x%x> blksz<%d> block<%d> error<%d>",
			mrq->cmd->opcode, mrq->cmd->arg, data->blksz, data->blocks,
			data->error);
	}
	data->host_cookie = 0;
}

static int msdc_do_request_async(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;
	void __iomem *base = host->base;

#ifdef MTK_MSDC_USE_CMD23
	u32 l_card_no_cmd23 = 0;
#endif

#ifdef MTK_MSDC_USE_CACHE
	u32 l_force_prg = 0;
#endif

	/* u32 intsts = 0; */
	/* unsigned int left=0; */
	int dma = 0, read = 1;	/* , dir = DMA_FROM_DEVICE; */

	BUG_ON(mmc == NULL);
	BUG_ON(mrq == NULL);
	if (!is_card_present(host) || host->power_mode == MMC_POWER_OFF) {
		ERR_MSG("cmd<%d> arg<0x%x> card<%d> power<%d>",
			mrq->cmd->opcode, mrq->cmd->arg,
			is_card_present(host), host->power_mode);
		mrq->cmd->error = (unsigned int)-ENOMEDIUM;
		if (mrq->done)
			mrq->done(mrq);	/* call done directly. */
		return 0;
	}
	msdc_ungate_clock(host);
	host->tune = 0;

	host->error = 0;
	atomic_set(&host->abort, 0);
	spin_lock(&host->lock);
	cmd = mrq->cmd;
	data = mrq->cmd->data;
	host->mrq = mrq;
	/* check msdc is work ok.
	 * rule is RX/TX fifocnt must be zero after last request
	 * if find abnormal, try to reset msdc first
	 */
	if (msdc_txfifocnt() || msdc_rxfifocnt()) {
		pr_err("[SD%d] register abnormal,please check!\n", host->id);
		msdc_reset_hw(host->id);
	}

	BUG_ON(data->blksz > HOST_MAX_BLKSZ);
	/* send_type=SND_DAT; */

#ifdef MTK_MSDC_USE_CACHE
	if ((host->hw->host_function == MSDC_EMMC)
		&& host->mmc->card && (host->mmc->card->ext_csd.cache_ctrl & 0x1)
	    && (cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK))
		l_force_prg = !msdc_can_apply_cache(cmd->arg, data->blocks);
#endif

	data->error = 0;
	read = data->flags & MMC_DATA_READ ? 1 : 0;
	msdc_latest_operation_type[host->id] = read ?
		OPER_TYPE_READ : OPER_TYPE_WRITE;
	host->data = data;
	host->xfer_size = data->blocks * data->blksz;
	host->blksz = data->blksz;
	host->dma_xfer = 1;
	/* deside the transfer mode */

	if ((read) && ((host->timeout_ns != data->timeout_ns)
		|| (host->timeout_clks != data->timeout_clks)))
		msdc_set_timeout(host, data->timeout_ns, data->timeout_clks);

	msdc_set_blknum(host, data->blocks);
	msdc_dma_on();		/* enable DMA mode first!! */
	/* init_completion(&host->xfer_done); */

#ifdef MTK_MSDC_USE_CMD23
	/* if tuning flow run here, no problem?? need check!!!!!!! */
	if (0 == (host->autocmd & MSDC_AUTOCMD23)) {
		/* start the cmd23 first */
		if (mrq->sbc) {
			host->autocmd &= ~MSDC_AUTOCMD12;

			if (host->hw->host_function == MSDC_EMMC) {
#ifdef MTK_MSDC_USE_CACHE
				if (l_force_prg && !((mrq->sbc->arg >> 31) & 0x1))
					mrq->sbc->arg |= (1 << 24);
#endif
			}

			if (msdc_command_start(host, mrq->sbc, 0, CMD_TIMEOUT) != 0)
				goto done;

			/* then wait command done */
			if (msdc_command_resp_polling(host, mrq->sbc, 0, CMD_TIMEOUT) != 0)
				goto stop;
		} else {
			/* some sd card may not support cmd23,
			 * some emmc card have problem with cmd23, so use cmd12 here */
			if (host->hw->host_function != MSDC_SDIO)
				host->autocmd |= MSDC_AUTOCMD12;
		}
	} else {
		if (mrq->sbc) {
			host->autocmd &= ~MSDC_AUTOCMD12;
			if (host->hw->host_function == MSDC_EMMC) {
#ifdef MTK_MSDC_USE_CACHE
				if (l_force_prg && !((mrq->sbc->arg >> 31) & 0x1))
					mrq->sbc->arg |= (1 << 24);
#endif
			}
		} else {
			/* some sd card may not support cmd23,
			 * some emmc card have problem with cmd23, so use cmd12 here */
			if (host->hw->host_function != MSDC_SDIO) {
				host->autocmd &= ~MSDC_AUTOCMD23;
				host->autocmd |= MSDC_AUTOCMD12;
				l_card_no_cmd23 = 1;
			}
		}
	}

#else
	/* start the command first */
	if (host->hw->host_function != MSDC_SDIO)
		host->autocmd |= MSDC_AUTOCMD12;
#endif				/* end of MTK_MSDC_USE_CMD23 */

	if (msdc_command_start(host, cmd, 0, CMD_TIMEOUT) != 0)
		goto done;

	/* then wait command done */
	if (msdc_command_resp_polling(host, cmd, 0, CMD_TIMEOUT) != 0)
		goto stop;

	/* for read, the data coming too fast, then CRC error
	   start DMA no business with CRC. */
	/* init_completion(&host->xfer_done); */
	msdc_dma_setup(host, &host->dma, data->sg, data->sg_len);
	msdc_dma_start(host);
	spin_unlock(&host->lock);

#ifdef MTK_MSDC_USE_CMD23
	/* for msdc use cmd23, but card not supported(sbc is NULL),
	 * need enable autocmd23 for next request.
	 */
	if (1 == l_card_no_cmd23) {
		if (host->hw->host_function != MSDC_SDIO) {
			host->autocmd |= MSDC_AUTOCMD23;
			host->autocmd &= ~MSDC_AUTOCMD12;
			l_card_no_cmd23 = 0;
		}
	}
#endif
	msdc_update_cahce_status(host, mrq);
	return 0;

 stop:
#ifndef MTK_MSDC_USE_CMD23
	/* Last: stop transfer */
	if (data && data->stop) {
		if (!((cmd->error == 0) && (data->error == 0)
			&& (host->autocmd & MSDC_AUTOCMD12)
			&& (cmd->opcode == MMC_READ_MULTIPLE_BLOCK
			|| cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK))) {
			if (msdc_do_command(host, data->stop, 0, CMD_TIMEOUT) != 0)
				goto done;
		}
	}
#else

	if (host->hw->host_function == MSDC_EMMC) {
		/* error handle will do msdc_abort_data() */
	} else {
		if (data && data->stop) {
			if (!((cmd->error == 0) && (data->error == 0)
			      && (host->autocmd & MSDC_AUTOCMD12)
			      && (cmd->opcode == MMC_READ_MULTIPLE_BLOCK
			      || cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK))) {
				if (msdc_do_command(host, data->stop, 0, CMD_TIMEOUT) != 0)
					goto done;
			}
		}
	}
#endif

 done:
#ifdef MTK_MSDC_USE_CMD23
	/* for msdc use cmd23, but card not supported(sbc is NULL),
	 * need enable autocmd23 for next request
	 */
	if (1 == l_card_no_cmd23) {
		if (host->hw->host_function != MSDC_SDIO) {
			host->autocmd |= MSDC_AUTOCMD23;
			host->autocmd &= ~MSDC_AUTOCMD12;
			l_card_no_cmd23 = 0;
		}
	}
#endif

	msdc_dma_clear(host);

	N_MSG(OPS, "CMD<%d> data<%s %s> blksz<%d> block<%d> error<%d>", cmd->opcode,
	      (dma ? "dma" : "pio"), (read ? "read " : "write"), data->blksz,
	      data->blocks, data->error);

	if (!(is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))) {
		if ((cmd->opcode != 17) && (cmd->opcode != 18) && (cmd->opcode != 24)
		    && (cmd->opcode != 25)) {
			N_MSG(NRW, "CMD<%3d> arg<0x%8x> Resp<0x%8x> data<%s> size<%d>",
				cmd->opcode, cmd->arg, cmd->resp[0], (read ? "read " : "write"),
				data->blksz * data->blocks);
		} else {
			N_MSG(RW, "CMD<%3d> arg<0x%8x> Resp<0x%8x> block<%d>", cmd->opcode,
			      cmd->arg, cmd->resp[0], data->blocks);
		}
	}
#ifdef MTK_MSDC_USE_CMD23
	if (mrq->sbc && (mrq->sbc->error == (unsigned int)-EIO))
		host->error |= REQ_CMD_EIO;
	if (mrq->sbc && (mrq->sbc->error == (unsigned int)-ETIMEDOUT)) {
#ifdef CONFIG_MTK_AEE_FEATURE
		aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_NE_JBT_TRACES
		| DB_OPT_DISPLAY_HANG_DUMP, "\n@eMMC FATAL ERROR@\n",
		"eMMC fatal error ");
#endif
		host->error |= REQ_CMD_TMO;
	}
#endif

	if (mrq->cmd->error == (unsigned int)-EIO) {
		if (((cmd->opcode == MMC_SELECT_CARD)
			|| (cmd->opcode == MMC_SLEEP_AWAKE)) &&
		    ((host->hw->host_function == MSDC_EMMC)
		    || (host->hw->host_function == MSDC_SD))) {
			/* should be deleted in new platform,
			 * as the state verify function has applied
			 */
			mrq->cmd->error = 0x0;
		} else {
			host->error |= REQ_CMD_EIO;
		}
	}
	if (mrq->cmd->error == (unsigned int)-ETIMEDOUT)
		host->error |= REQ_CMD_TMO;
	if (mrq->stop && (mrq->stop->error == (unsigned int)-EIO))
		host->error |= REQ_STOP_EIO;
	if (mrq->stop && (mrq->stop->error == (unsigned int)-ETIMEDOUT))
		host->error |= REQ_STOP_TMO;
	msdc_update_cahce_status(host, mrq);
	msdc_gate_clock(host, 1);
	spin_unlock(&host->lock);
	return host->error;
}

static int msdc_app_cmd(struct mmc_host *mmc, struct msdc_host *host)
{
	struct mmc_command cmd = { 0 };
	struct mmc_request mrq = { 0 };
	u32 err = -1;

	cmd.opcode = MMC_APP_CMD;
	cmd.arg = host->app_cmd_arg;	/* meet mmc->card is null when ACMD6 */
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;

	mrq.cmd = &cmd;
	cmd.mrq = &mrq;
	cmd.data = NULL;

	err = msdc_do_command(host, &cmd, 0, CMD_TIMEOUT);
	return err;
}

static int msdc_lower_freq(struct msdc_host *host)
{
	u32 div, mode, hs400_src;
	void __iomem *base = host->base;

	ERR_MSG("need to lower freq");
	msdc_reset_crc_tune_counter(host, ALL_TUNE_CNT);
	sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, mode);
	sdr_get_field(MSDC_CFG, MSDC_CFG_CKDIV, div);
	sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD_HS400, hs400_src);

#ifndef FPGA_PLATFORM
	if (host->id == 0)
		hclks = hclks_msdc50;
	else
		hclks = hclks_msdc30;
#endif

	if (div >= MSDC_MAX_FREQ_DIV) {
		ERR_MSG("but, div<%d> power tuning", div);
		return msdc_power_tuning(host);
	} else if ((mode == 3) && (host->id == 0)) {
		/* when HS400 low freq, you cannot change to mode 2 (DDR mode),
		 * else read data will be latched by clk, but not ds pin
		 * when card speed mode is still HS400.
		 */
		if (hs400_src == 1) {
			hs400_src = 0;
			/* change from 400Mhz to 800Mhz,
			 * because CCKDIV is invalid when 400Mhz clk src
			 */
			msdc_clock_src[host->id] = MSDC50_CLKSRC_800MHZ;
			host->hw->clk_src = msdc_clock_src[host->id];
			msdc_select_clksrc(host, host->hw->clk_src);
		}
		msdc_clk_stable(host, mode, div + 1, hs400_src);
		host->sclk = hclks[host->hw->clk_src] / (2 * 4 * (div + 1));

		ERR_MSG("new div<%d>, mode<%d> new freq.<%dKHz>",
			div + 1, mode, host->sclk / 1000);
	} else if (mode == 1) {
		mode = 0;
		msdc_clk_stable(host, mode, div + 1, hs400_src);
		host->sclk = (div == 0) ? hclks[host->hw->clk_src] / 2 :
			hclks[host->hw->clk_src] / (4 * div);

		ERR_MSG("new div<%d>, mode<%d> new freq.<%dKHz>",
			div, mode, host->sclk / 1000);
	} else {
		msdc_clk_stable(host, mode, div + 1, hs400_src);
		host->sclk = (mode == 2) ? hclks[host->hw->clk_src] /
			(2 * 4 * (div + 1)) : hclks[host->hw->clk_src] / (4 * (div + 1));

		ERR_MSG("new div<%d>, mode<%d> new freq.<%dKHz>",
			div + 1, mode, host->sclk / 1000);
	}

	return 0;
}

int msdc_tune_cmdrsp(struct msdc_host *host)
{
	int result = 0;
	void __iomem *base = host->base;
	u32 sel = 0;
	u32 cur_rsmpl = 0, orig_rsmpl;
	u32 cur_rrdly = 0, orig_rrdly;
	u32 cur_cntr = 0, orig_cmdrtc;
	u32 cur_dl_cksel = 0, orig_dl_cksel;
	u32 clkmode;
	int hs400 = 0;

	sdr_get_field(MSDC_IOCON, MSDC_IOCON_RSPL, orig_rsmpl);
	sdr_get_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, orig_rrdly);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR, orig_cmdrtc);
	sdr_get_field(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
		orig_dl_cksel);
	sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	hs400 = (clkmode == 3) ? 1 : 0;
#ifdef STO_LOG
	if (unlikely(dumpMSDC())) {
		AddStorageTrace(STORAGE_LOGGER_MSG_MSDC_DO, msdc_do_request,
			"sd_tune_ori RSPL", orig_rsmpl);
		AddStorageTrace(STORAGE_LOGGER_MSG_MSDC_DO, msdc_do_request,
			"sd_tune_ori RRDLY", orig_rrdly);
	}
#endif
#if 1
	if (host->mclk >= 100000000) {
		sel = 1;
		/* sdr_set_field(MSDC_PATCH_BIT0, MSDC_CKGEN_RX_SDCLKO_SEL,0); */
	} else {
		sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR, 1);
		/* sdr_set_field(MSDC_PATCH_BIT0, MSDC_CKGEN_RX_SDCLKO_SEL,1); */
		sdr_set_field(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL, 0);
	}

	cur_rsmpl = (orig_rsmpl + 1);
	msdc_set_smpl(host, hs400, cur_rsmpl % 2, TYPE_CMD_RESP_EDGE, NULL);
	if (host->mclk <= 400000) {
		msdc_set_smpl(host, hs400, 0, TYPE_CMD_RESP_EDGE, NULL);
		cur_rsmpl = 2;
	}
	if (cur_rsmpl >= 2) {
		cur_rrdly = (orig_rrdly + 1);
		sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, cur_rrdly % 32);
	}
	if (cur_rrdly >= 32) {
		if (sel) {
			cur_cntr = (orig_cmdrtc + 1);
			sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR,
				cur_cntr % 8);
		}
	}
	if (cur_cntr >= 8) {
		if (sel) {
			cur_dl_cksel = (orig_dl_cksel + 1);
			sdr_set_field(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
				cur_dl_cksel % 8);
		}
	}
	++(host->t_counter.time_cmd);
	if ((sel && host->t_counter.time_cmd == CMD_TUNE_UHS_MAX_TIME)
	    || (sel == 0 && host->t_counter.time_cmd == CMD_TUNE_HS_MAX_TIME)) {
#ifdef MSDC_LOWER_FREQ
		result = msdc_lower_freq(host);
#else
		result = 1;
#endif
		host->t_counter.time_cmd = 0;
	}
#else
	if (orig_rsmpl == 0) {
		cur_rsmpl = 1;
		msdc_set_smpl(host, hs400, cur_rsmpl, TYPE_CMD_RESP_EDGE, NULL);
	} else {
		cur_rsmpl = 0;
		/* need second layer */
		msdc_set_smpl(host, hs400, cur_rsmpl, TYPE_CMD_RESP_EDGE, NULL);
		cur_rrdly = (orig_rrdly + 1);
		if (cur_rrdly >= 32) {
			ERR_MSG("failed to update rrdly<%d>", cur_rrdly);
			sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, 0);
#ifdef MSDC_LOWER_FREQ
			return msdc_lower_freq(host);
#else
			return 1;
#endif
		}
		sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, cur_rrdly);
	}

#endif
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_RSPL, orig_rsmpl);
	sdr_get_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, orig_rrdly);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR, orig_cmdrtc);
	sdr_get_field(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
		orig_dl_cksel);
	pr_err("msdc%d TUNE_CMD: rsmpl<%d> rrdly<%d> cmdrtc<%d> dl_cksel<%d> sfreq.<%d>",
		host->id, orig_rsmpl, orig_rrdly, orig_cmdrtc, orig_dl_cksel, host->sclk);
#ifdef STO_LOG
	if (unlikely(dumpMSDC())) {
		AddStorageTrace(STORAGE_LOGGER_MSG_MSDC_DO, msdc_do_request,
			"sd_tune_ok RSPL", orig_rsmpl);
		AddStorageTrace(STORAGE_LOGGER_MSG_MSDC_DO, msdc_do_request,
			"sd_tune_ok RRDLY", orig_rrdly);
	}
#endif
	return result;
}

int hs400_restore_pad_tune(int restore)
{
	void __iomem *base = 0;

	if (mtk_msdc_host[0]) {
		base = mtk_msdc_host[0]->base;
		mtk_msdc_host[0]->saved_para.pad_tune0 = sdr_read32(MSDC_PAD_TUNE0);
	}
	return 0;
}

int hs400_restore_pb1(int restore)
{
	void __iomem *base = 0;

	if (mtk_msdc_host[0]) {
		base = mtk_msdc_host[0]->base;
		if (!restore)
			sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR, 0x2);
		sdr_get_field((MSDC_PATCH_BIT1), MSDC_PB1_WRDAT_CRCS_TA_CNTR,
			      mtk_msdc_host[0]->saved_para.wrdat_crc_ta_cntr);
	}
	return 0;
}

int hs400_restore_ddly0(int restore)
{
	void __iomem *base = 0;

	if (mtk_msdc_host[0]) {
		base = mtk_msdc_host[0]->base;
		mtk_msdc_host[0]->saved_para.ddly0 = sdr_read32(MSDC_DAT_RDDLY0);
	}
	return 0;
}

int hs400_restore_ddly1(int restore)
{
	void __iomem *base = 0;

	if (mtk_msdc_host[0]) {
		base = mtk_msdc_host[0]->base;
		mtk_msdc_host[0]->saved_para.ddly1 = sdr_read32(MSDC_DAT_RDDLY1);
	}
	return 0;
}

int hs400_restore_cmd_tune(int restore)
{
	void __iomem *base = 0;

	if (mtk_msdc_host[0]) {
		base = mtk_msdc_host[0]->base;
		if (!restore) {
			sdr_set_field(EMMC50_PAD_CMD_TUNE, MSDC_EMMC50_PAD_CMD_TUNE_TXDLY,
				0x8);
		}
	}
	return 0;
}

int hs400_restore_dat01_tune(int restore)
{
	void __iomem *base = 0;

	if (mtk_msdc_host[0]) {
		base = mtk_msdc_host[0]->base;
		if (0) {	/* (!restore){ */
			sdr_set_field(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT0_TXDLY,
				0x4);
			sdr_set_field(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT1_TXDLY,
				0x4);
		}
	}
	return 0;
}

int hs400_restore_dat23_tune(int restore)
{
	void __iomem *base = 0;

	if (mtk_msdc_host[0]) {
		base = mtk_msdc_host[0]->base;
		if (0) {	/* (!restore){ */
			sdr_set_field(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT2_TXDLY,
				0x4);
			sdr_set_field(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT3_TXDLY,
				0x4);
		}
	}
	return 0;
}

int hs400_restore_dat45_tune(int restore)
{
	void __iomem *base = 0;

	if (mtk_msdc_host[0]) {
		base = mtk_msdc_host[0]->base;
		if (0) {	/* (!restore){ */
			sdr_set_field(EMMC50_PAD_DAT45_TUNE, MSDC_EMMC50_PAD_DAT4_TXDLY,
				0x4);
			sdr_set_field(EMMC50_PAD_DAT45_TUNE, MSDC_EMMC50_PAD_DAT5_TXDLY,
				0x4);
		}
	}
	return 0;
}

int hs400_restore_dat67_tune(int restore)
{
	void __iomem *base = 0;

	if (mtk_msdc_host[0]) {
		base = mtk_msdc_host[0]->base;
		if (0) {	/* (!restore){ */
			sdr_set_field(EMMC50_PAD_DAT67_TUNE, MSDC_EMMC50_PAD_DAT6_TXDLY,
				0x4);
			sdr_set_field(EMMC50_PAD_DAT67_TUNE, MSDC_EMMC50_PAD_DAT7_TXDLY,
				0x4);
		}
	}
	return 0;
}

int hs400_restore_ds_tune(int restore)
{
	void __iomem *base = 0;

	if (mtk_msdc_host[0]) {
		base = mtk_msdc_host[0]->base;
		if (0) {	/* (!restore){ */
			sdr_set_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY1,
				0x7);
			sdr_set_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY3,
				0x18);
		}
	}
	return 0;
}

/*
 * 2013-12-09
 * different register settings between eMMC 4.5 backward speed mode
 * and HS400 speed mode
 */
#define HS400_BACKUP_REG_NUM (12)
static struct msdc_reg_control hs400_backup_reg_list[HS400_BACKUP_REG_NUM] = {
	/*   addr                   mask       value    default     restore_func */
	{(OFFSET_MSDC_PATCH_BIT0), (MSDC_PB0_INT_DAT_LATCH_CK_SEL), 0x0, 0x0, NULL},
	/* the defalut init value is 0x1, but HS400 need the value be 0x0 */
	{(OFFSET_MSDC_PATCH_BIT1), (MSDC_PB1_WRDAT_CRCS_TA_CNTR), 0x0, 0x0,
		hs400_restore_pb1},
	{(OFFSET_MSDC_IOCON),
		(MSDC_IOCON_R_D_SMPL | MSDC_IOCON_DDLSEL | MSDC_IOCON_R_D_SMPL_SEL
		| MSDC_IOCON_R_D0SPL | MSDC_IOCON_W_D_SMPL_SEL | MSDC_IOCON_W_D_SMPL),
		0x0, 0x0, NULL},
	{(OFFSET_MSDC_PAD_TUNE0), (MSDC_PAD_TUNE0_DATWRDLY
		| MSDC_PAD_TUNE0_DATRRDLY), 0x0, 0x0, hs400_restore_pad_tune},
	{(OFFSET_MSDC_DAT_RDDLY0), (MSDC_DAT_RDDLY0_D3 | MSDC_DAT_RDDLY0_D2
		| MSDC_DAT_RDDLY0_D1 | MSDC_DAT_RDDLY0_D0), 0x0, 0x0,
		hs400_restore_ddly0},
	{(OFFSET_MSDC_DAT_RDDLY1), (MSDC_DAT_RDDLY1_D7 | MSDC_DAT_RDDLY1_D6
		| MSDC_DAT_RDDLY1_D5 | MSDC_DAT_RDDLY1_D4), 0x0, 0x0,
		hs400_restore_ddly1},
	{(OFFSET_EMMC50_PAD_CMD_TUNE), (MSDC_EMMC50_PAD_CMD_TUNE_TXDLY),
		0x0, 0x00000200, hs400_restore_cmd_tune},
	{(OFFSET_EMMC50_PAD_DS_TUNE), (MSDC_EMMC50_PAD_DS_TUNE_DLY1
		| MSDC_EMMC50_PAD_DS_TUNE_DLY3), 0x0, 0x0, hs400_restore_ds_tune},
	{(OFFSET_EMMC50_PAD_DAT01_TUNE), (MSDC_EMMC50_PAD_DAT0_TXDLY
		| MSDC_EMMC50_PAD_DAT1_TXDLY), 0x0, 0x01000100,
		hs400_restore_dat01_tune},
	{(OFFSET_EMMC50_PAD_DAT23_TUNE), (MSDC_EMMC50_PAD_DAT2_TXDLY
		| MSDC_EMMC50_PAD_DAT3_TXDLY), 0x0, 0x01000100,
		hs400_restore_dat23_tune},
	{(OFFSET_EMMC50_PAD_DAT45_TUNE), (MSDC_EMMC50_PAD_DAT4_TXDLY
		| MSDC_EMMC50_PAD_DAT5_TXDLY), 0x0, 0x01000100,
		hs400_restore_dat45_tune},
	{(OFFSET_EMMC50_PAD_DAT67_TUNE), (MSDC_EMMC50_PAD_DAT6_TXDLY
		| MSDC_EMMC50_PAD_DAT7_TXDLY), 0x0, 0x01000100,
		hs400_restore_dat67_tune},
};

/*
 * 2013-12-09
 * when switch from eMMC 4.5 backward speed mode to HS400 speed mode
 * do back up the eMMC 4.5 backward speed mode tunning result,
 * and init them with defalut value for HS400 speed mode
 */
static void emmc_hs400_backup(void)
{
	int i = 0, err = 0;

	for (i = 0; i < HS400_BACKUP_REG_NUM; i++) {
		sdr_get_field((hs400_backup_reg_list[i].addr + mtk_msdc_host[0]->base),
			hs400_backup_reg_list[i].mask,
			      hs400_backup_reg_list[i].value);
		if (hs400_backup_reg_list[i].restore_func) {
			err = hs400_backup_reg_list[i].restore_func(0);
			if (err) {
				pr_err
				    ("[%s]: failed to restore reg[%p][0x%x];"
					"expected value[0x%x], actual value[0x%x] err=0x%x",
				     __func__, (hs400_backup_reg_list[i].addr
				     + mtk_msdc_host[0]->base), hs400_backup_reg_list[i].mask,
				     hs400_backup_reg_list[i].default_value,
				     sdr_read32((hs400_backup_reg_list[i].addr +
				     mtk_msdc_host[0]->base)), err);
			}
		}
	}
}

/*
 *  2013-12-09
 *  when switch from HS400 speed mode to eMMC 4.5 backward speed mode
 *  do restore the eMMC 4.5 backward speed mode tunning result
 */
static void emmc_hs400_restore(void)
{
	int i = 0, err = 0;

	if (!mtk_msdc_host[0]) {
		pr_err("[%s] msdc%d is not exist\n", __func__, 0);
		return;
	}

	for (i = 0; i < HS400_BACKUP_REG_NUM; i++) {
		sdr_set_field((hs400_backup_reg_list[i].addr + mtk_msdc_host[0]->base),
			hs400_backup_reg_list[i].mask,
			      hs400_backup_reg_list[i].value);
		if (hs400_backup_reg_list[i].restore_func) {
			err = hs400_backup_reg_list[i].restore_func(1);
			if (err) {
				pr_err
				    ("[%s]:failed to restore reg[%p][0x%x];"
				    "expected value[0x%x], actual value[0x%x] err=0x%x",
				     __func__, (hs400_backup_reg_list[i].addr
				     + mtk_msdc_host[0]->base), hs400_backup_reg_list[i].mask,
				     hs400_backup_reg_list[i].value,
				     sdr_read32(hs400_backup_reg_list[i].addr), err);
			}
		}
		pr_debug("[%s]:i:%d, reg=%p, value=0x%x\n", __func__, i,
			 (hs400_backup_reg_list[i].addr + mtk_msdc_host[0]->base),
			 sdr_read32((hs400_backup_reg_list[i].addr
				+ mtk_msdc_host[0]->base)));
	}
}

/*
 *  2015-01-09
 *  Runtime reducing to legacy speed or slower clock, clear eMMC ett timing
 */
#ifdef CONFIG_MMC_FFU
static void emmc_clear_timing(void)
{
	int i = 0;

	if (!mtk_msdc_host[0]) {
		pr_err("[%s] msdc%d is not exist\n", __func__, 0);
		return;
	}

	pr_err("emmc_clear_timing msdc0\n");
	for (i = 0; i < HS400_BACKUP_REG_NUM; i++)
		sdr_set_field((hs400_backup_reg_list[i].addr + mtk_msdc_host[0]->base),
			hs400_backup_reg_list[i].mask,
			hs400_backup_reg_list[i].default_value);

}
#endif
/*
 *  2013-12-09
 *  HS400 error tune flow of read/write data error
 *  HS400 error tune flow of cmd error is same as eMMC4.5 backward speed mode.
 */
int emmc_hs400_tune_rw(struct msdc_host *host)
{
	void __iomem *base = host->base;
	int cur_ds_dly1 = 0, cur_ds_dly3 = 0, orig_ds_dly1 = 0, orig_ds_dly3 = 0;
	int err = 0;

	if ((host->id != 0) || (host->timing != MMC_TIMING_MMC_HS400))
		return err;

	sdr_get_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY1,
		orig_ds_dly1);
	sdr_get_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY3,
		orig_ds_dly3);

	if (g_ett_tune) {
		cur_ds_dly3 = orig_ds_dly3 + 1;
		cur_ds_dly1 = orig_ds_dly1;
		if (cur_ds_dly3 >= 32) {
			cur_ds_dly3 = 0;
			cur_ds_dly1 = orig_ds_dly1 + 1;
			if (cur_ds_dly1 >= 32)
				cur_ds_dly1 = 0;
		}
	} else {
		cur_ds_dly1 = orig_ds_dly1 - 1;
		cur_ds_dly3 = orig_ds_dly3;
		if (cur_ds_dly1 < 0) {
			cur_ds_dly1 = 17;
			cur_ds_dly3 = orig_ds_dly3 + 1;
			if (cur_ds_dly3 >= 32)
				cur_ds_dly3 = 0;
		}
	}

	if (++host->t_counter.time_hs400 ==
		(g_ett_tune ? (32 * 32) : MAX_HS400_TUNE_COUNT)) {
		ERR_MSG("Failed to update EMMC50_PAD_DS_TUNE_DLY;"
			"cur_ds_dly3=0x%x, cur_ds_dly1=0x%x", cur_ds_dly3, cur_ds_dly1);
#ifdef MSDC_LOWER_FREQ
		err = msdc_lower_freq(host);
#else
		err = 1;
#endif
		goto out;
	} else {
		sdr_set_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY1,
			cur_ds_dly1);
		if (cur_ds_dly3 != orig_ds_dly3) {
			sdr_set_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY3,
				cur_ds_dly3);
		}
		pr_err("msdc%d HS400_TUNE: orig_ds_dly1<0x%x>, orig_ds_dly3<0x%x>;"
		    "cur_ds_dly1<0x%x>, cur_ds_dly3<0x%x>", host->id, orig_ds_dly1,
		    orig_ds_dly3, cur_ds_dly1, cur_ds_dly3);
	}
 out:
	return err;
}

int msdc_tune_read(struct msdc_host *host)
{
	void __iomem *base = host->base;
	u32 sel = 0;
	u32 ddr = 0, hs400 = 0;
	u32 dcrc;
	u32 clkmode = 0;
	u32 cur_rxdly0, cur_rxdly1;
	u32 cur_dsmpl = 0, orig_dsmpl;
	u32 cur_dsel = 0, orig_dsel;
	u32 cur_dl_cksel = 0, orig_dl_cksel;
	u32 cur_dat0 = 0, cur_dat1 = 0, cur_dat2 = 0, cur_dat3 = 0,
	    cur_dat4 = 0, cur_dat5 = 0, cur_dat6 = 0, cur_dat7 = 0;
	u32 orig_dat0, orig_dat1, orig_dat2, orig_dat3, orig_dat4,
		orig_dat5, orig_dat6, orig_dat7;
	int result = 0;

#if 1
	if (host->mclk >= 100000000)
		sel = 1;
	else
		sdr_set_field(MSDC_PATCH_BIT0, MSDC_PB0_CKGEN_MSDC_DLY_SEL, 0);

	sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	ddr = (clkmode == 2) ? 1 : 0;
	hs400 = (clkmode == 3) ? 1 : 0;

	sdr_get_field(MSDC_PATCH_BIT0, MSDC_PB0_CKGEN_MSDC_DLY_SEL, orig_dsel);
	sdr_get_field(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
		orig_dl_cksel);
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_R_D_SMPL, orig_dsmpl);

	sdr_set_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);
	cur_dsmpl = (orig_dsmpl + 1);
	msdc_set_smpl(host, hs400, cur_dsmpl % 2, TYPE_READ_DATA_EDGE, NULL);

	if (cur_dsmpl >= 2) {
		sdr_get_field(SDC_DCRC_STS, SDC_DCRC_STS_POS | SDC_DCRC_STS_NEG, dcrc);

		if (!ddr)
			dcrc &= ~SDC_DCRC_STS_NEG;

		cur_rxdly0 = sdr_read32(MSDC_DAT_RDDLY0);
		cur_rxdly1 = sdr_read32(MSDC_DAT_RDDLY1);

		orig_dat0 = (cur_rxdly0 >> 24) & 0x1F;
		orig_dat1 = (cur_rxdly0 >> 16) & 0x1F;
		orig_dat2 = (cur_rxdly0 >> 8) & 0x1F;
		orig_dat3 = (cur_rxdly0 >> 0) & 0x1F;
		orig_dat4 = (cur_rxdly1 >> 24) & 0x1F;
		orig_dat5 = (cur_rxdly1 >> 16) & 0x1F;
		orig_dat6 = (cur_rxdly1 >> 8) & 0x1F;
		orig_dat7 = (cur_rxdly1 >> 0) & 0x1F;

		if (ddr) {
			cur_dat0 = (dcrc & (1 << 0)
				    || dcrc & (1 << 8)) ? (orig_dat0 + 1) : orig_dat0;
			cur_dat1 = (dcrc & (1 << 1)
				    || dcrc & (1 << 9)) ? (orig_dat1 + 1) : orig_dat1;
			cur_dat2 = (dcrc & (1 << 2)
				    || dcrc & (1 << 10)) ? (orig_dat2 + 1) : orig_dat2;
			cur_dat3 = (dcrc & (1 << 3)
				    || dcrc & (1 << 11)) ? (orig_dat3 + 1) : orig_dat3;
			cur_dat4 = (dcrc & (1 << 4)
				    || dcrc & (1 << 12)) ? (orig_dat4 + 1) : orig_dat4;
			cur_dat5 = (dcrc & (1 << 5)
				    || dcrc & (1 << 13)) ? (orig_dat5 + 1) : orig_dat5;
			cur_dat6 = (dcrc & (1 << 6)
				    || dcrc & (1 << 14)) ? (orig_dat6 + 1) : orig_dat6;
			cur_dat7 = (dcrc & (1 << 7)
				    || dcrc & (1 << 15)) ? (orig_dat7 + 1) : orig_dat7;
		} else {
			cur_dat0 = (dcrc & (1 << 0)) ? (orig_dat0 + 1) : orig_dat0;
			cur_dat1 = (dcrc & (1 << 1)) ? (orig_dat1 + 1) : orig_dat1;
			cur_dat2 = (dcrc & (1 << 2)) ? (orig_dat2 + 1) : orig_dat2;
			cur_dat3 = (dcrc & (1 << 3)) ? (orig_dat3 + 1) : orig_dat3;
			cur_dat4 = (dcrc & (1 << 4)) ? (orig_dat4 + 1) : orig_dat4;
			cur_dat5 = (dcrc & (1 << 5)) ? (orig_dat5 + 1) : orig_dat5;
			cur_dat6 = (dcrc & (1 << 6)) ? (orig_dat6 + 1) : orig_dat6;
			cur_dat7 = (dcrc & (1 << 7)) ? (orig_dat7 + 1) : orig_dat7;
		}

		cur_rxdly0 = ((cur_dat0 & 0x1F) << 24) | ((cur_dat1 & 0x1F) << 16) |
		    ((cur_dat2 & 0x1F) << 8) | ((cur_dat3 & 0x1F) << 0);
		cur_rxdly1 = ((cur_dat4 & 0x1F) << 24) | ((cur_dat5 & 0x1F) << 16) |
		    ((cur_dat6 & 0x1F) << 8) | ((cur_dat7 & 0x1F) << 0);

		sdr_write32(MSDC_DAT_RDDLY0, cur_rxdly0);
		sdr_write32(MSDC_DAT_RDDLY1, cur_rxdly1);

	}
	if ((cur_dat0 >= 32) || (cur_dat1 >= 32) || (cur_dat2 >= 32)
		|| (cur_dat3 >= 32) || (cur_dat4 >= 32) || (cur_dat5 >= 32)
		|| (cur_dat6 >= 32) || (cur_dat7 >= 32)) {
		if (sel) {
			sdr_write32(MSDC_DAT_RDDLY0, 0);
			sdr_write32(MSDC_DAT_RDDLY1, 0);
			cur_dsel = (orig_dsel + 1);
			sdr_set_field(MSDC_PATCH_BIT0, MSDC_PB0_CKGEN_MSDC_DLY_SEL,
				cur_dsel % 32);
		}
	}
	if (cur_dsel >= 32) {
		if (clkmode == 1 && sel) {
			cur_dl_cksel = (orig_dl_cksel + 1);
			sdr_set_field(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
				cur_dl_cksel % 8);
		}
	}
	++(host->t_counter.time_read);
	if ((sel == 1 && clkmode == 1
		&& host->t_counter.time_read == READ_TUNE_UHS_CLKMOD1_MAX_TIME)
	    || (sel == 1 && (clkmode == 0 || clkmode == 2)
		&& host->t_counter.time_read == READ_TUNE_UHS_MAX_TIME)
	    || (sel == 0 && (clkmode == 0 || clkmode == 2)
		&& host->t_counter.time_read == READ_TUNE_HS_MAX_TIME)) {
#ifdef MSDC_LOWER_FREQ
		result = msdc_lower_freq(host);
#else
		result = 1;
#endif
		host->t_counter.time_read = 0;
	}
#else
	sdr_set_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);

	cur_rxdly0 = sdr_read32(MSDC_DAT_RDDLY0);
	cur_rxdly1 = sdr_read32(MSDC_DAT_RDDLY1);
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_R_D_SMPL, orig_dsmpl);
	if (orig_dsmpl == 0) {
		cur_dsmpl = 1;
		msdc_set_smpl(host, hs400, cur_dsmpl, TYPE_READ_DATA_EDGE, NULL);
	} else {
		cur_dsmpl = 0;
		/* need second layer */
		msdc_set_smpl(host, hs400, cur_dsmpl, TYPE_READ_DATA_EDGE, NULL);
		sdr_get_field(SDC_DCRC_STS, SDC_DCRC_STS_POS | SDC_DCRC_STS_NEG, dcrc);

		if (!ddr)
			dcrc &= ~SDC_DCRC_STS_NEG;

		if (sdr_read32(MSDC_ECO_VER) >= 4) {
			orig_dat0 = (cur_rxdly0 >> 24) & 0x1F;
			orig_dat1 = (cur_rxdly0 >> 16) & 0x1F;
			orig_dat2 = (cur_rxdly0 >> 8) & 0x1F;
			orig_dat3 = (cur_rxdly0 >> 0) & 0x1F;
			orig_dat4 = (cur_rxdly1 >> 24) & 0x1F;
			orig_dat5 = (cur_rxdly1 >> 16) & 0x1F;
			orig_dat6 = (cur_rxdly1 >> 8) & 0x1F;
			orig_dat7 = (cur_rxdly1 >> 0) & 0x1F;
		} else {
			orig_dat0 = (cur_rxdly0 >> 0) & 0x1F;
			orig_dat1 = (cur_rxdly0 >> 8) & 0x1F;
			orig_dat2 = (cur_rxdly0 >> 16) & 0x1F;
			orig_dat3 = (cur_rxdly0 >> 24) & 0x1F;
			orig_dat4 = (cur_rxdly1 >> 0) & 0x1F;
			orig_dat5 = (cur_rxdly1 >> 8) & 0x1F;
			orig_dat6 = (cur_rxdly1 >> 16) & 0x1F;
			orig_dat7 = (cur_rxdly1 >> 24) & 0x1F;
		}

		if (ddr) {
			cur_dat0 = (dcrc & (1 << 0)
				    || dcrc & (1 << 8)) ? (orig_dat0 + 1) : orig_dat0;
			cur_dat1 = (dcrc & (1 << 1)
				    || dcrc & (1 << 9)) ? (orig_dat1 + 1) : orig_dat1;
			cur_dat2 = (dcrc & (1 << 2)
				    || dcrc & (1 << 10)) ? (orig_dat2 + 1) : orig_dat2;
			cur_dat3 = (dcrc & (1 << 3)
				    || dcrc & (1 << 11)) ? (orig_dat3 + 1) : orig_dat3;
		} else {
			cur_dat0 = (dcrc & (1 << 0)) ? (orig_dat0 + 1) : orig_dat0;
			cur_dat1 = (dcrc & (1 << 1)) ? (orig_dat1 + 1) : orig_dat1;
			cur_dat2 = (dcrc & (1 << 2)) ? (orig_dat2 + 1) : orig_dat2;
			cur_dat3 = (dcrc & (1 << 3)) ? (orig_dat3 + 1) : orig_dat3;
		}
		cur_dat4 = (dcrc & (1 << 4)) ? (orig_dat4 + 1) : orig_dat4;
		cur_dat5 = (dcrc & (1 << 5)) ? (orig_dat5 + 1) : orig_dat5;
		cur_dat6 = (dcrc & (1 << 6)) ? (orig_dat6 + 1) : orig_dat6;
		cur_dat7 = (dcrc & (1 << 7)) ? (orig_dat7 + 1) : orig_dat7;

		if (cur_dat0 >= 32 || cur_dat1 >= 32
			|| cur_dat2 >= 32 || cur_dat3 >= 32) {
			ERR_MSG("failed to update <%xh><%xh><%xh><%xh>",
				cur_dat0, cur_dat1, cur_dat2, cur_dat3);
			sdr_write32(MSDC_DAT_RDDLY0, 0);
			sdr_write32(MSDC_DAT_RDDLY1, 0);

#ifdef MSDC_LOWER_FREQ
			return msdc_lower_freq(host);
#else
			return 1;
#endif
		}

		if (cur_dat4 >= 32 || cur_dat5 >= 32
			|| cur_dat6 >= 32 || cur_dat7 >= 32) {
			ERR_MSG("failed to update <%xh><%xh><%xh><%xh>",
				cur_dat4, cur_dat5, cur_dat6, cur_dat7);
			sdr_write32(MSDC_DAT_RDDLY0, 0);
			sdr_write32(MSDC_DAT_RDDLY1, 0);

#ifdef MSDC_LOWER_FREQ
			return msdc_lower_freq(host);
#else
			return 1;
#endif
		}

		cur_rxdly0 = (cur_dat0 << 24) | (cur_dat1 << 16)
			| (cur_dat2 << 8) | (cur_dat3 << 0);
		cur_rxdly1 = (cur_dat4 << 24) | (cur_dat5 << 16)
			| (cur_dat6 << 8) | (cur_dat7 << 0);

		sdr_write32(MSDC_DAT_RDDLY0, cur_rxdly0);
		sdr_write32(MSDC_DAT_RDDLY1, cur_rxdly1);
	}

#endif
	sdr_get_field(MSDC_PATCH_BIT0, MSDC_PB0_CKGEN_MSDC_DLY_SEL, orig_dsel);
	sdr_get_field(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
		orig_dl_cksel);
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_R_D_SMPL, orig_dsmpl);
	cur_rxdly0 = sdr_read32(MSDC_DAT_RDDLY0);
	cur_rxdly1 = sdr_read32(MSDC_DAT_RDDLY1);
	pr_err("msdc%d TUNE_READ: dsmpl<%d> rxdly0<0x%x> rxdly1<0x%x>;"
		"dsel<%d> dl_cksel<%d> sfreq.<%d>", host->id, orig_dsmpl, cur_rxdly0,
		cur_rxdly1, orig_dsel, orig_dl_cksel, host->sclk);

	return result;
}

int msdc_tune_write(struct msdc_host *host)
{
	void __iomem *base = host->base;

	/* u32 cur_wrrdly = 0, orig_wrrdly; */
	u32 cur_dsmpl = 0, orig_dsmpl;
	u32 cur_rxdly0 = 0;
	u32 orig_dat0, orig_dat1, orig_dat2, orig_dat3;
	u32 cur_dat0 = 0, cur_dat1 = 0, cur_dat2 = 0, cur_dat3 = 0;
	u32 cur_d_cntr = 0, orig_d_cntr;
	int result = 0;

	int sel = 0;
	int clkmode = 0;
	int hs400 = 0;

#if 1
	if (host->mclk >= 100000000)
		sel = 1;
	else
		sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR, 1);

	sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	/* sdr_get_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY, orig_wrrdly);*/
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, orig_dsmpl);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR, orig_d_cntr);
	sdr_set_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);

	cur_dsmpl = (orig_dsmpl + 1);
	hs400 = (clkmode == 3) ? 1 : 0;
	msdc_set_smpl(host, hs400, cur_dsmpl % 2, TYPE_WRITE_CRC_EDGE, NULL);
#if 0
	if (cur_dsmpl >= 2) {
		cur_wrrdly = (orig_wrrdly + 1);
		sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY, cur_wrrdly % 32);
	}
#endif
	if (cur_dsmpl >= 2) {
		cur_rxdly0 = sdr_read32(MSDC_DAT_RDDLY0);

		orig_dat0 = (cur_rxdly0 >> 24) & 0x1F;
		orig_dat1 = (cur_rxdly0 >> 16) & 0x1F;
		orig_dat2 = (cur_rxdly0 >> 8) & 0x1F;
		orig_dat3 = (cur_rxdly0 >> 0) & 0x1F;

		cur_dat0 = (orig_dat0 + 1);	/* only adjust bit-1 for crc */
		cur_dat1 = orig_dat1;
		cur_dat2 = orig_dat2;
		cur_dat3 = orig_dat3;

		cur_rxdly0 = ((cur_dat0 & 0x1F) << 24) | ((cur_dat1 & 0x1F) << 16) |
		    ((cur_dat2 & 0x1F) << 8) | ((cur_dat3 & 0x1F) << 0);

		sdr_write32(MSDC_DAT_RDDLY0, cur_rxdly0);
	}
	if (cur_dat0 >= 32) {
		if (sel) {
			cur_d_cntr = (orig_d_cntr + 1);
			sdr_set_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR,
				cur_d_cntr % 8);
		}
	}
	++(host->t_counter.time_write);
	if ((sel == 0 && host->t_counter.time_write == WRITE_TUNE_HS_MAX_TIME)
	    || (sel && host->t_counter.time_write == WRITE_TUNE_UHS_MAX_TIME)) {
#ifdef MSDC_LOWER_FREQ
		result = msdc_lower_freq(host);
#else
		result = 1;
#endif
		host->t_counter.time_write = 0;
	}
#else

	/* Tune Method 2. just DAT0 */
	sdr_set_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);
	cur_rxdly0 = sdr_read32(MSDC_DAT_RDDLY0);
	if (sdr_read32(MSDC_ECO_VER) >= 4) {
		orig_dat0 = (cur_rxdly0 >> 24) & 0x1F;
		orig_dat1 = (cur_rxdly0 >> 16) & 0x1F;
		orig_dat2 = (cur_rxdly0 >> 8) & 0x1F;
		orig_dat3 = (cur_rxdly0 >> 0) & 0x1F;
	} else {
		orig_dat0 = (cur_rxdly0 >> 0) & 0x1F;
		orig_dat1 = (cur_rxdly0 >> 8) & 0x1F;
		orig_dat2 = (cur_rxdly0 >> 16) & 0x1F;
		orig_dat3 = (cur_rxdly0 >> 24) & 0x1F;
	}

	sdr_get_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY, orig_wrrdly);
	cur_wrrdly = orig_wrrdly;
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, orig_dsmpl);
	if (orig_dsmpl == 0) {
		cur_dsmpl = 1;
		msdc_set_smpl(host, hs400, cur_dsmpl, TYPE_WRITE_CRC_EDGE, NULL);
	} else {
		cur_dsmpl = 0;
		/* need the second layer */
		sdr_set_field(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, cur_dsmpl);

		cur_wrrdly = (orig_wrrdly + 1);
		if (cur_wrrdly < 32) {
			sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY, cur_wrrdly);
		} else {
			cur_wrrdly = 0;
			/* need third */
			sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY, cur_wrrdly);

			cur_dat0 = orig_dat0 + 1;	/* only adjust bit-1 for crc */
			cur_dat1 = orig_dat1;
			cur_dat2 = orig_dat2;
			cur_dat3 = orig_dat3;

			if (cur_dat0 >= 32) {
				ERR_MSG("update failed <%xh>", cur_dat0);
				sdr_write32(MSDC_DAT_RDDLY0, 0);

#ifdef MSDC_LOWER_FREQ
				return msdc_lower_freq(host);
#else
				return 1;
#endif
			}

			cur_rxdly0 = (cur_dat0 << 24) | (cur_dat1 << 16)
				| (cur_dat2 << 8) | (cur_dat3 << 0);
			sdr_write32(MSDC_DAT_RDDLY0, cur_rxdly0);
		}

	}

#endif
	/* sdr_get_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY, orig_wrrdly); */
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, orig_dsmpl);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR, orig_d_cntr);
	cur_rxdly0 = sdr_read32(MSDC_DAT_RDDLY0);
	pr_err("msdc%d TUNE_WRITE: dsmpl<%d> rxdly0<0x%x> d_cntr<%d> sfreq.<%d>",
		host->id, orig_dsmpl, cur_rxdly0, orig_d_cntr, host->sclk);

	return result;
}

static int msdc_get_card_status(struct mmc_host *mmc, struct msdc_host *host,
	u32 *status)
{
	struct mmc_command cmd;
	struct mmc_request mrq;
	u32 err;

	memset(&cmd, 0, sizeof(struct mmc_command));
	cmd.opcode = MMC_SEND_STATUS;	/* CMD13 */
	cmd.arg = host->app_cmd_arg;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	memset(&mrq, 0, sizeof(struct mmc_request));
	mrq.cmd = &cmd;
	cmd.mrq = &mrq;
	cmd.data = NULL;
	/* tune until CMD13 pass. */
	err = msdc_do_command(host, &cmd, 0, CMD_TIMEOUT);

	if (status)
		*status = cmd.resp[0];

	return err;
}

/* #define TUNE_FLOW_TEST */
#ifdef TUNE_FLOW_TEST
static void msdc_reset_para(struct msdc_host *host)
{
	void __iomem *base = host->base;
	u32 dsmpl, rsmpl, clkmode;
	int hs400 = 0;

	/* because we have a card, which must work at dsmpl<0> and rsmpl<0> */

	sdr_get_field(MSDC_IOCON, MSDC_IOCON_R_D_SMPL, dsmpl);
	sdr_get_field(MSDC_IOCON, MSDC_IOCON_RSPL, rsmpl);
	sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	hs400 = (clkmode == 3) ? 1 : 0;

	if (dsmpl == 0) {
		msdc_set_smpl(host, hs400, 1, TYPE_READ_DATA_EDGE, NULL);
		ERR_MSG("set dspl<0>");
		sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, 0);
	}

	if (rsmpl == 0) {
		msdc_set_smpl(host, hs400, 1, TYPE_CMD_RESP_EDGE, NULL);
		ERR_MSG("set rspl<0>");
		sdr_write32(MSDC_DAT_RDDLY0, 0);
		sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY, 0);
	}
}
#endif

static void msdc_dump_trans_error(struct msdc_host *host,
				  struct mmc_command *cmd,
				  struct mmc_data *data,
				  struct mmc_command *stop,
				  struct mmc_command *sbc)
{
	/* void __iomem *base = host->base; */

	if ((cmd->opcode == 52) && (cmd->arg == 0xc00))
		return;
	if ((cmd->opcode == 52) && (cmd->arg == 0x80000c08))
		return;
	/* by pass the SDIO CMD TO for SD/eMMC */
	if (!(is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))) {
		if ((host->hw->host_function == MSDC_SD) && (cmd->opcode == 5))
			return;
	} else {
		if (cmd->opcode == 8)
			return;
	}

#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT
	/* auto-K have not done or finished */
	if (is_card_sdio(host))	{
		if (atomic_read(&host->ot_work.autok_done) == 0
			&& (cmd->opcode == 52 || cmd->opcode == 53))
			return;
	}
#endif

	ERR_MSG("XXX CMD<%d><0x%x> Error<%d> Resp<0x%x>",
		cmd->opcode, cmd->arg, cmd->error, cmd->resp[0]);

	if (data) {
		if (host->suspend == 1)
			ERR_MSG("XXX DAT block<%d> Error<%d>", data->blocks, data->error);
		else
			pr_debug("msdc%d XXX DAT block<%d> Error<%d>\n",
				host->id, data->blocks, data->error);
	}
	if (stop) {
		if (host->suspend == 1)
			ERR_MSG("XXX STOP<%d><0x%x> Error<%d> Resp<0x%x>",
				stop->opcode, stop->arg, stop->error, stop->resp[0]);
		else
			pr_debug("msdc%d XXX STOP<%d><0x%x> Error<%d> Resp<0x%x>\n",
				host->id, stop->opcode, stop->arg, stop->error, stop->resp[0]);
	}

	if (sbc) {
		if (host->suspend == 1)
			ERR_MSG("XXX SBC<%d><0x%x> Error<%d> Resp<0x%x>",
				sbc->opcode, sbc->arg, sbc->error, sbc->resp[0]);
		else
			pr_debug("msdc%d XXX SBC<%d><0x%x> Error<%d> Resp<0x%x>\n",
				host->id, sbc->opcode, sbc->arg, sbc->error, sbc->resp[0]);
	}
#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT
	if ((host->hw->host_function == MSDC_SDIO) && (cmd) && (data) &&
	    ((cmd->error == -EIO) || (data->error == -EIO))) {
		u32 vcore_uv_off = autok_get_current_vcore_offset();
		/* ccyeh@FIXME */
		/*int cur_temperature = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_CPU); */
		int cur_temperature = 0; /* ccyeh@FIXME */

		ERR_MSG("XXX Vcore<0x%x> CPU_Temperature<%d>",
			vcore_uv_off, cur_temperature);
	}
#endif

	if ((host->hw->host_function == MSDC_SD)
		&& (host->sclk > 100000000) && (data)
	    && (data->error != (unsigned int)-ETIMEDOUT)) {
		if ((data->flags & MMC_DATA_WRITE) && (host->write_timeout_uhs104))
			host->write_timeout_uhs104 = 0;
		if ((data->flags & MMC_DATA_READ) && (host->read_timeout_uhs104))
			host->read_timeout_uhs104 = 0;
	}

	if ((host->hw->host_function == MSDC_EMMC) && (data)
		&& (data->error != (unsigned int)-ETIMEDOUT)) {
		if ((data->flags & MMC_DATA_WRITE) && (host->write_timeout_emmc))
			host->write_timeout_emmc = 0;
		if ((data->flags & MMC_DATA_READ) && (host->read_timeout_emmc))
			host->read_timeout_emmc = 0;
	}
#ifdef SDIO_ERROR_BYPASS
	if (is_card_sdio(host) && (host->sdio_error != -EIO) && (cmd->opcode == 53)
	    && (msdc_sg_len(data->sg, host->dma_xfer) > 4)) {
		host->sdio_error = -EIO;
		ERR_MSG("XXX SDIO Error ByPass");
	}
#endif
}

/* ops.request */
static void msdc_ops_request_legacy(struct mmc_host *mmc,
				struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;
	struct mmc_command *stop = NULL;
	struct mmc_command *sbc = NULL;
	/* === for sdio profile === */
	/* CCJ fix */
#if 0
	u32 old_H32 = 0, old_L32 = 0, new_H32 = 0, new_L32 = 0;
	u32 ticks = 0, opcode = 0, sizes = 0, bRx = 0;
#endif
	u32 status_verify = 0;

	msdc_reset_crc_tune_counter(host, ALL_TUNE_CNT);
	if (host->mrq) {
		ERR_MSG("XXX host->mrq<0x%p> cmd<%d>arg<0x%x>",
			host->mrq, host->mrq->cmd->opcode, host->mrq->cmd->arg);
		BUG();
	}

	if (!is_card_present(host) || host->power_mode == MMC_POWER_OFF) {
		ERR_MSG("cmd<%d> arg<0x%x> card<%d> power<%d>",
			mrq->cmd->opcode, mrq->cmd->arg,
			is_card_present(host), host->power_mode);
		mrq->cmd->error = (unsigned int)-ENOMEDIUM;

#if 1
		if (mrq->done)
			mrq->done(mrq);	/* call done directly. */
#else
		mrq->cmd->retries = 0;	/* please don't retry. */
		mmc_request_done(mmc, mrq);
#endif

		return;
	}

	/* start to process */
	spin_lock(&host->lock);
	host->power_cycle_enable = 1;

	cmd = mrq->cmd;
	data = mrq->cmd->data;
	if (data)
		stop = data->stop;

#ifdef MTK_MSDC_USE_CMD23
	if (data)
		sbc = mrq->sbc;
#endif

	msdc_ungate_clock(host); /* set sw flag */
#if 0
	if (sdio_pro_enable) {	/* === for sdio profile === */
		if (mrq->cmd->opcode == 52 || mrq->cmd->opcode == 53)
			/* GPT_GetCounter64(&old_L32, &old_H32); */
	}
#endif
	host->mrq = mrq;

	while (msdc_do_request(mmc, mrq)) {
		/* there is some error
		 * because ISR execute time will be monitor, try to dump info here
		 */
		msdc_dump_trans_error(host, cmd, data, stop, sbc);

		if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))
			goto out;	/* sdio not tuning */
#ifdef MTK_MSDC_USE_CMD23
		if ((sbc != NULL) && (sbc->error == (unsigned int)-ETIMEDOUT)) {
			if (cmd->opcode == MMC_READ_MULTIPLE_BLOCK
				|| cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK) {
				/* not tuning, go out directly */
				pr_err("===[%s:%d]==cmd23 timeout==\n", __func__, __LINE__);
				goto out;
			}
		}
#endif

#ifdef MTK_MSDC_USE_CMD23
		/* cmd->error also set when autocmd23 crc error */
		if ((cmd->error == (unsigned int)-EIO)
		    || (stop && (stop->error == (unsigned int)-EIO))
		    || (sbc && (sbc->error == (unsigned int)-EIO))) {
#else
		if ((cmd->error == (unsigned int)-EIO)
		    || (stop && (stop->error == (unsigned int)-EIO))) {
#endif
			if (msdc_tune_cmdrsp(host)) {
				ERR_MSG("failed to updata cmd para");
				goto out;
			}
		}

		if (data && (data->error == (unsigned int)-EIO)) {
			if ((host->id == 0) && (host->timing == MMC_TIMING_MMC_HS400)) {
				if (emmc_hs400_tune_rw(host)) {
					ERR_MSG("failed to updata write para");
					goto out;
				}
			} else if (data->flags & MMC_DATA_READ) {	/* read */
				if (msdc_tune_read(host)) {
					ERR_MSG("failed to updata read para");
					goto out;
				}
			} else {
				if (msdc_tune_write(host)) {
					ERR_MSG("failed to updata write para");
					goto out;
				}
			}
		}

		status_verify = msdc_status_verify(host, cmd);
		if (MSDC_VERIFY_ERROR == status_verify) {
			ERR_MSG("status verify failed");
			/*data_abort = 1; */
			if (host->hw->host_function == MSDC_SD)
				goto out;
		} else if (MSDC_VERIFY_NEED_NOT_TUNE == status_verify) {
			/* clear the error condition. */
			ERR_MSG("need not error tune");
			cmd->error = 0;
			goto out;
		}

		/* CMD TO -> not tuning */
		if (cmd->error == (unsigned int)-ETIMEDOUT
		    && cmd->opcode != MMC_READ_SINGLE_BLOCK
		    && cmd->opcode != MMC_READ_MULTIPLE_BLOCK
		    && cmd->opcode != MMC_WRITE_BLOCK
		    && cmd->opcode != MMC_WRITE_MULTIPLE_BLOCK) {
			goto out;
		}

		if (cmd->error == (unsigned int)-ENOMEDIUM)
			goto out;

		/* [ALPS114710] Patch for data timeout issue */
		if (data && (data->error == (unsigned int)-ETIMEDOUT)) {
			if (data->flags & MMC_DATA_READ) {
				if (!(host->sw_timeout) && (host->hw->host_function == MSDC_SD)
					&& (host->sclk > 100000000)
					&& (host->read_timeout_uhs104 < MSDC_MAX_R_TIMEOUT_TUNE)) {
					if (host->t_counter.time_read)
						host->t_counter.time_read--;
					host->read_timeout_uhs104++;
					msdc_tune_read(host);
				} else if ((host->sw_timeout)
					   || (host->read_timeout_uhs104 >= MSDC_MAX_R_TIMEOUT_TUNE)
					   || (++(host->read_time_tune) > MSDC_MAX_TIMEOUT_RETRY)) {
					ERR_MSG
					    ("msdc%d exceed max read timeout retry times(%d) or;"
					    "SW timeout(%d) or read timeout tune(%d),Power cycle",
					     host->id, host->read_time_tune, host->sw_timeout,
					     host->read_timeout_uhs104);
					if (msdc_power_tuning(host))
						goto out;
				}
			} else if (data->flags & MMC_DATA_WRITE) {
				if ((!(host->sw_timeout)) &&
				    (host->hw->host_function == MSDC_SD) &&
				    (host->sclk > 100000000) &&
				    (host->write_timeout_uhs104 < MSDC_MAX_W_TIMEOUT_TUNE)) {
					if (host->t_counter.time_write)
						host->t_counter.time_write--;
					host->write_timeout_uhs104++;
					msdc_tune_write(host);
				} else if (!(host->sw_timeout) &&
					   (host->hw->host_function == MSDC_EMMC) &&
					   (host->write_timeout_emmc <
					   MSDC_MAX_W_TIMEOUT_TUNE_EMMC)) {
					if (host->t_counter.time_write)
						host->t_counter.time_write--;
					host->write_timeout_emmc++;

					if ((host->id == 0)
						&& (host->timing == MMC_TIMING_MMC_HS400))
						emmc_hs400_tune_rw(host);
					else
						msdc_tune_write(host);
				} else if ((host->hw->host_function == MSDC_SD)
					&& ((host->sw_timeout)
					|| (host->write_timeout_uhs104 >= MSDC_MAX_W_TIMEOUT_TUNE)
					|| (++(host->write_time_tune) >	MSDC_MAX_TIMEOUT_RETRY))) {
					ERR_MSG
					    ("msdc%d exceed max write timeout retry times(%d) or;"
					    "SW timeout(%d) or write timeout tune (%d),Power cycle"
					     , host->id, host->write_time_tune, host->sw_timeout,
					     host->write_timeout_uhs104);
					if (!(host->sd_30_busy) && msdc_power_tuning(host))
						goto out;
				} else if ((host->hw->host_function == MSDC_EMMC)
					   && ((host->sw_timeout) ||
						(++(host->write_time_tune) >
						MSDC_MAX_TIMEOUT_RETRY_EMMC))) {
					ERR_MSG
					    ("msdc%d exceed max write timeout retry times(%d) or;"
					    "SW timeout(%d) or write timeout tune (%d),Power cycle"
					     , host->id, host->write_time_tune, host->sw_timeout,
					     host->write_timeout_emmc);
					host->write_timeout_emmc = 0;
					goto out;
				}
			}
		}

		/* clear the error condition. */
		cmd->error = 0;
		if (data)
			data->error = 0;
		if (stop)
			stop->error = 0;

#ifdef MTK_MSDC_USE_CMD23
		if (sbc)
			sbc->error = 0;
#endif

		/* check if an app commmand. */
		if (host->app_cmd) {
			while (msdc_app_cmd(host->mmc, host)) {
				if (msdc_tune_cmdrsp(host)) {
					ERR_MSG("failed to updata cmd para for app");
					goto out;
				}
			}
		}

		if (!is_card_present(host))
			goto out;
	}

	if ((host->read_time_tune)
	    && (cmd->opcode == MMC_READ_SINGLE_BLOCK
	    || cmd->opcode == MMC_READ_MULTIPLE_BLOCK)) {
		host->read_time_tune = 0;
		ERR_MSG("Read recover");
		msdc_dump_trans_error(host, cmd, data, stop, sbc);
	}
	if ((host->write_time_tune)
	    && (cmd->opcode == MMC_WRITE_BLOCK
	    || cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK)) {
		host->write_time_tune = 0;
		ERR_MSG("Write recover");
		msdc_dump_trans_error(host, cmd, data, stop, sbc);
	}
	host->sw_timeout = 0;
	if (host->hw->host_function == MSDC_SD)
		host->continuous_fail_request_count = 0;

 out:
	msdc_reset_crc_tune_counter(host, ALL_TUNE_CNT);
#ifdef MTK_MSDC_USE_CACHE
	if (g_flush_error_happend && (host->hw->host_function == MSDC_EMMC)
		&& host->mmc->card && (host->mmc->card->ext_csd.cache_ctrl & 0x1)) {
		if ((cmd->opcode == MMC_SWITCH)
		    && (((cmd->arg >> 16) & 0xFF) == EXT_CSD_FLUSH_CACHE)
		    && (((cmd->arg >> 8) & 0x1))) {
			g_flush_error_count++;
			g_flush_error_happend = 0;
			ERR_MSG("the %d time flush error happned, g_flush_data_size=%lld",
				g_flush_error_count, g_flush_data_size);
			/*
			 * if reinit emmc at resume,cache should not be enabled
			 * because too much flush error. so add cache quirk for this emmmc.
			 * if awake emmc at resume,cache should not be enabled
			 * because too much flush error, so force set cache_size=0
			 */
			if (g_flush_error_count >= MSDC_MAX_FLUSH_COUNT) {
				if (!msdc_cache_ctrl(host, 0, NULL)) {
					g_emmc_cache_quirk[0] = emmc_id;
					host->mmc->card->ext_csd.cache_size = 0;
				}
				pr_err("msdc%d:flush cache error count=%d,Disable cache\n",
					host->id, g_flush_error_count);
			}
		}
	}
#endif

#ifdef TUNE_FLOW_TEST
	if (!is_card_sdio(host))
		msdc_reset_para(host);
#endif

	/* ==== when request done, check if app_cmd ==== */
	if (mrq->cmd->opcode == MMC_APP_CMD) {
		host->app_cmd = 1;
		host->app_cmd_arg = mrq->cmd->arg;	/* save the RCA */
	} else {
		host->app_cmd = 0;
		/* host->app_cmd_arg = 0; */
	}

	host->mrq = NULL;
/* CCJ fix */
#if 0
	/* === for sdio profile === */
	if (sdio_pro_enable) {
		if (mrq->cmd->opcode == 52 || mrq->cmd->opcode == 53) {
			/* GPT_GetCounter64(&new_L32, &new_H32); */
			ticks = msdc_time_calc(old_L32, old_H32, new_L32, new_H32);

			opcode = mrq->cmd->opcode;
			if (mrq->cmd->data) {
				sizes = mrq->cmd->data->blocks * mrq->cmd->data->blksz;
				bRx = mrq->cmd->data->flags & MMC_DATA_READ ? 1 : 0;
			} else {
				bRx = mrq->cmd->arg & 0x80000000 ? 1 : 0;
			}

			if (!mrq->cmd->error)
				msdc_performance(opcode, sizes, bRx, ticks);
		}
	}
#endif
	msdc_gate_clock(host, 1);	/* clear flag. */
	spin_unlock(&host->lock);

	mmc_request_done(mmc, mrq);
}

static void msdc_tune_async_request(struct mmc_host *mmc,
				struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;
	struct mmc_command *stop = NULL;
	struct mmc_command *sbc = NULL;
	u32 status_verify = 0;

	/* msdc_reset_crc_tune_counter(host,ALL_TUNE_CNT) */
	if (host->mrq) {
#ifdef CONFIG_MTK_AEE_FEATURE
		aee_kernel_warning("MSDC",
				   "MSDC request not clear.\n"
				   "host attached<0x%p> current<0x%p>\n", host->mrq, mrq);
#else
		WARN_ON(host->mrq);
#endif
		ERR_MSG("XXX host->mrq<0x%p> cmd<%d>arg<0x%x>",
		host->mrq, host->mrq->cmd->opcode, host->mrq->cmd->arg);
		if (host->mrq->data) {
			ERR_MSG("XXX request data size<%d>",
				host->mrq->data->blocks * host->mrq->data->blksz);
			ERR_MSG("XXX request attach to host force data timeout and retry");
			host->mrq->data->error = (unsigned int)-ETIMEDOUT;
		} else {
			ERR_MSG("XXX request attach to host force cmd timeout and retry");
			host->mrq->cmd->error = (unsigned int)-ETIMEDOUT;
		}
		ERR_MSG("XXX current request <0x%p> cmd<%d>arg<0x%x>",
			mrq, mrq->cmd->opcode, mrq->cmd->arg);
		if (mrq->data)
			ERR_MSG("XXX current request data size<%d>",
			mrq->data->blocks * mrq->data->blksz);
	}

	if (!is_card_present(host) || host->power_mode == MMC_POWER_OFF) {
		ERR_MSG("cmd<%d> arg<0x%x> card<%d> power<%d>",
			mrq->cmd->opcode, mrq->cmd->arg,
			is_card_present(host), host->power_mode);
		mrq->cmd->error = (unsigned int)-ENOMEDIUM;
		/* should call done for this request */
		goto done;
	}

	cmd = mrq->cmd;
	data = mrq->cmd->data;

	if (data)
		stop = data->stop;
#ifdef MTK_MSDC_USE_CMD23
	if (data)
		sbc = mrq->sbc;
#endif

	/* start to process */
	spin_lock(&host->lock);

	/*if(host->error & REQ_CMD_EIO)
	   cmd->error = (unsigned int)-EIO;
	   else if(host->error & REQ_CMD_TMO)
	   cmd->error = (unsigned int)-ETIMEDOUT;
	 */

	msdc_ungate_clock(host);	/* set sw flag */
	host->tune = 1;
	host->mrq = mrq;
	/* because ISR executing time will be monitor, try to dump the info here. */
	do {
		msdc_dump_trans_error(host, cmd, data, stop, sbc);
		/*if((host->t_counter.time_cmd % 16 == 15)
		   || (host->t_counter.time_read % 16 == 15)
		   || (host->t_counter.time_write % 16 == 15))
		   {
		   spin_unlock(&host->lock);
		   msleep(150);
		   ERR_MSG("sleep 150ms here!");
		   spin_lock(&host->lock);
		   goto out;
		   } */

#ifdef MTK_MSDC_USE_CMD23
		if ((sbc != NULL) && (sbc->error == (unsigned int)-ETIMEDOUT)) {
			if (cmd->opcode == MMC_READ_MULTIPLE_BLOCK
				|| cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK) {
				/* not tuning, go out directly */
				pr_err("===[%s:%d]==cmd23 timeout==\n", __func__, __LINE__);
				goto out;
			}
		}
#endif

#ifdef MTK_MSDC_USE_CMD23
		/* cmd->error also set when autocmd23 crc error */
		if ((cmd->error == (unsigned int)-EIO)
		    || (stop && (stop->error == (unsigned int)-EIO))
		    || (sbc && (sbc->error == (unsigned int)-EIO))) {
#else
		if ((cmd->error == (unsigned int)-EIO)
		    || (stop && (stop->error == (unsigned int)-EIO))) {
#endif
			if (msdc_tune_cmdrsp(host)) {
				ERR_MSG("failed to updata cmd para");
				goto out;
			}
		}

		if (data && (data->error == (unsigned int)-EIO)) {
			if ((host->id == 0) && (host->timing == MMC_TIMING_MMC_HS400)) {
				if (emmc_hs400_tune_rw(host)) {
					ERR_MSG("failed to updata write para");
					goto out;
				}
			} else if (data->flags & MMC_DATA_READ) {	/* read */
				if (msdc_tune_read(host)) {
					ERR_MSG("failed to updata read para");
					goto out;
				}
			} else {
				if (msdc_tune_write(host)) {
					ERR_MSG("failed to updata write para");
					goto out;
				}
			}
		}

		status_verify = msdc_status_verify(host, cmd);
		if (MSDC_VERIFY_ERROR == status_verify) {
			ERR_MSG("status verify failed");
			/*data_abort = 1; */
			if (host->hw->host_function == MSDC_SD)
				goto out;
		} else if (MSDC_VERIFY_NEED_NOT_TUNE == status_verify) {
			/* clear the error condition. */
			ERR_MSG("need not error tune");
			cmd->error = 0;
			goto out;
		}

		/* CMD TO -> not tuning. cmd->error also set when autocmd23 TO error */
		if (cmd->error == (unsigned int)-ETIMEDOUT) {
			if (cmd->opcode == MMC_READ_SINGLE_BLOCK
			    || cmd->opcode == MMC_READ_MULTIPLE_BLOCK
			    || cmd->opcode == MMC_WRITE_BLOCK
			    || cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK) {
				if ((host->sw_timeout)
				    || (++(host->rwcmd_time_tune) > MSDC_MAX_TIMEOUT_RETRY)) {
					ERR_MSG
					    ("msdc%d exceed max r/w cmd timeout tune times(%d);"
					    " or SW timeout(%d),Power cycle",
					     host->id, host->rwcmd_time_tune, host->sw_timeout);
					if (!(host->sd_30_busy) && msdc_power_tuning(host))
						goto out;
				}
			} else {
				goto out;
			}
		}

		if (cmd->error == (unsigned int)-ENOMEDIUM)
			goto out;
		/* [ALPS114710] Patch for data timeout issue. */
		if (data && (data->error == (unsigned int)-ETIMEDOUT)) {
			if (data->flags & MMC_DATA_READ) {
				if (!(host->sw_timeout) &&
				    (host->hw->host_function == MSDC_SD)
				    && (host->sclk > 100000000)
				    && (host->read_timeout_uhs104 < MSDC_MAX_R_TIMEOUT_TUNE)) {
					if (host->t_counter.time_read)
						host->t_counter.time_read--;
					host->read_timeout_uhs104++;
					msdc_tune_read(host);
				} else if ((host->sw_timeout)
					   || (host->read_timeout_uhs104 >= MSDC_MAX_R_TIMEOUT_TUNE)
					   || (++(host->read_time_tune) > MSDC_MAX_TIMEOUT_RETRY)) {
					ERR_MSG
					    ("msdc%d exceed max read timeout retry times(%d) or ;"
					    "SW timeout(%d) or read timeout tune(%d),Power cycle",
					     host->id, host->read_time_tune, host->sw_timeout,
					     host->read_timeout_uhs104);
					if (!(host->sd_30_busy) && msdc_power_tuning(host))
						goto out;
				}
			} else if (data->flags & MMC_DATA_WRITE) {
				if (!(host->sw_timeout) &&
				    (host->hw->host_function == MSDC_SD) &&
				    (host->sclk > 100000000) &&
				    (host->write_timeout_uhs104 < MSDC_MAX_W_TIMEOUT_TUNE)) {
					if (host->t_counter.time_write)
						host->t_counter.time_write--;
					host->write_timeout_uhs104++;
					msdc_tune_write(host);
				} else if (!(host->sw_timeout)
					&& (host->hw->host_function == MSDC_EMMC) &&
					(host->write_timeout_emmc < MSDC_MAX_W_TIMEOUT_TUNE_EMMC)) {
					if (host->t_counter.time_write)
						host->t_counter.time_write--;
					host->write_timeout_emmc++;
					if ((host->id == 0)
						&& (host->timing == MMC_TIMING_MMC_HS400))
						emmc_hs400_tune_rw(host);
					else
						msdc_tune_write(host);
				} else if ((host->hw->host_function == MSDC_SD) &&
					((host->sw_timeout)
					|| (host->write_timeout_uhs104 >= MSDC_MAX_W_TIMEOUT_TUNE)
					|| (++(host->write_time_tune) >	MSDC_MAX_TIMEOUT_RETRY))) {
					ERR_MSG
					    ("msdc%d exceed max write timeout retry times(%d) or ;"
					    "SW timeout(%d) or write timeout tune (%d),Power cycle"
					     , host->id, host->write_time_tune, host->sw_timeout,
					     host->write_timeout_uhs104);
					if (!(host->sd_30_busy) && msdc_power_tuning(host))
						goto out;
				} else if ((host->hw->host_function == MSDC_EMMC)
				&& ((host->sw_timeout)
				|| (++(host->write_time_tune) > MSDC_MAX_TIMEOUT_RETRY_EMMC))) {
					ERR_MSG
					    ("msdc%d exceed max write timeout retry times(%d) or ;"
					    "SW timeout(%d) or write timeout tune (%d),Power cycle"
					     , host->id, host->write_time_tune, host->sw_timeout,
					     host->write_timeout_emmc);
					host->write_timeout_emmc = 0;
					goto out;
				}
			}
		}
		/* clear the error condition. */
		cmd->error = 0;
		if (data)
			data->error = 0;
		if (stop)
			stop->error = 0;

#ifdef MTK_MSDC_USE_CMD23
		if (sbc)
			sbc->error = 0;
#endif

		host->sw_timeout = 0;
		if (!is_card_present(host))
			goto out;
	} while (msdc_tune_rw_request(mmc, mrq));

	if ((host->rwcmd_time_tune) && (cmd->opcode == MMC_READ_SINGLE_BLOCK
					|| cmd->opcode == MMC_READ_MULTIPLE_BLOCK
					|| cmd->opcode == MMC_WRITE_BLOCK
					|| cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK)) {
		host->rwcmd_time_tune = 0;
		ERR_MSG("RW cmd recover");
		msdc_dump_trans_error(host, cmd, data, stop, sbc);
	}
	if ((host->read_time_tune) && (cmd->opcode == MMC_READ_SINGLE_BLOCK
		|| cmd->opcode == MMC_READ_MULTIPLE_BLOCK)) {
		host->read_time_tune = 0;
		ERR_MSG("Read recover");
		msdc_dump_trans_error(host, cmd, data, stop, sbc);
	}
	if ((host->write_time_tune) && (cmd->opcode == MMC_WRITE_BLOCK
		|| cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK)) {
		host->write_time_tune = 0;
		ERR_MSG("Write recover");
		msdc_dump_trans_error(host, cmd, data, stop, sbc);
	}
	host->power_cycle_enable = 1;
	host->sw_timeout = 0;

	if (host->hw->host_function == MSDC_SD)
		host->continuous_fail_request_count = 0;
 out:
	if (host->sclk <= 50000000 && (host->timing != MMC_TIMING_UHS_DDR50))
		host->sd_30_busy = 0;
	msdc_reset_crc_tune_counter(host, ALL_TUNE_CNT);
	host->mrq = NULL;
	msdc_gate_clock(host, 1);	/* clear flag. */
	host->tune = 0;
	spin_unlock(&host->lock);

done:
	host->mrq_tune = NULL;
	mmc_request_done(mmc, mrq);
}

/* new thread tune */
static void msdc_async_tune(struct work_struct *work)
{
	struct msdc_host *host = NULL;
	struct mmc_host *mmc = NULL;

	host = container_of(work, struct msdc_host, work_tune);
	BUG_ON(!host);
	mmc = host->mmc;
	BUG_ON(!mmc);

	msdc_tune_async_request(mmc, host->mrq_tune);
}

static void msdc_ops_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct mmc_data *data;
	int host_cookie = 0;
	struct msdc_host *host = mmc_priv(mmc);

	BUG_ON(mmc == NULL);
	BUG_ON(mrq == NULL);

	if ((host->hw->host_function == MSDC_SDIO) && !(host->trans_lock.active))
		__pm_stay_awake(&host->trans_lock);

#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT	/* same as CONFIG_SDIOAUTOK_SUPPORT */
	if (host->id == 2) /* 6630 in msdc2@Denali */
		sdio_set_vcore_performance(host, 1);
#endif
	data = mrq->data;
	if (data)
		host_cookie = data->host_cookie;
	/*
	 * Asyn only support  DMA and asyc CMD flow
	 * if cmd send error occur, dma not start yet, return error,
	 * msdc_tune_async_request() will call at msdc_ops_request
	 */
	if (msdc_async_use_dma(host_cookie)) {
		if (msdc_do_request_async(mmc, mrq))
			msdc_tune_async_request(mmc, mrq);
	} else
		msdc_ops_request_legacy(mmc, mrq);

#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT	/* same as CONFIG_SDIOAUTOK_SUPPORT */
	if (host->id == 2) {	/* 6630 in msdc2@Denali */
		sdio_set_vcore_performance(host, 0);	/* disable */
	}
#endif

	if ((host->hw->host_function == MSDC_SDIO) && (host->trans_lock.active))
		__pm_relax(&host->trans_lock);
}

/* called by ops.set_ios */
static void msdc_set_buswidth(struct msdc_host *host, u32 width)
{
	void __iomem *base = host->base;
	u32 val = sdr_read32(SDC_CFG);

	val &= ~SDC_CFG_BUSWIDTH;

	switch (width) {
	default:
	case MMC_BUS_WIDTH_1:
		width = 1;
		val |= (MSDC_BUS_1BITS << 16);
		break;
	case MMC_BUS_WIDTH_4:
		val |= (MSDC_BUS_4BITS << 16);
		break;
	case MMC_BUS_WIDTH_8:
		val |= (MSDC_BUS_8BITS << 16);
		break;
	}

	sdr_write32(SDC_CFG, val);

	N_MSG(CFG, "Bus Width = %d", width);
}

static void msdc_apply_ett_settings(struct msdc_host *host, int mode)
{
	unsigned int i = 0;
	void __iomem *base = host->base;
	struct msdc_ett_settings *ett = NULL, *ett_item = NULL;
	unsigned int ett_count = 0;

	switch (emmc_id) {
#ifdef MSDC_SUPPORT_SANDISK_COMBO_ETT
	case SANDISK_EMMC_CHIP:
		pr_err("--- apply sandisk emmc ett settings\n");
		host->hw->ett_hs200_settings = msdc0_ett_hs200_settings_for_sandisk;
		host->hw->ett_hs400_settings = msdc0_ett_hs400_settings_for_sandisk;
		break;
#endif
#ifdef MSDC_SUPPORT_SAMSUNG_COMBO_ETT
	case SAMSUNG_EMMC_CHIP:
		pr_err("--- apply samsung emmc ett settings\n");
		host->hw->ett_hs200_settings = msdc0_ett_hs200_settings_for_samsung;
		host->hw->ett_hs400_settings = msdc0_ett_hs400_settings_for_samsung;
		break;
#endif
	default:
		pr_err("--- apply default emmc ett settings\n");
		break;
	}

	if (MSDC_HS200_MODE == mode) {
		ett_count = host->hw->ett_hs200_count;
		ett = host->hw->ett_hs200_settings;
		pr_err("[MSDC, %s] hs200 ett, ett_count=%d\n", __func__, host->hw->ett_hs200_count);
	} else if (MSDC_HS400_MODE == mode) {
		/* clear hs200 setting */
		ett_count = host->hw->ett_hs200_count;
		ett = host->hw->ett_hs200_settings;
		for (i = 0; i < ett_count; i++) {
			ett_item = (struct msdc_ett_settings *)(ett + i);
			sdr_set_field((base + ett_item->reg_addr), ett_item->reg_offset, 0);
		}
		ett_count = host->hw->ett_hs400_count;
		ett = host->hw->ett_hs400_settings;
		pr_err("[MSDC, %s] hs400 ett, ett_count=%d\n", __func__, host->hw->ett_hs400_count);
	}
	for (i = 0; i < ett_count; i++) {
		ett_item = (struct msdc_ett_settings *)(ett + i);
		sdr_set_field((base + ett_item->reg_addr),
				ett_item->reg_offset, ett_item->value);
		pr_err("%s:msdc%d,reg[0x%x],offset[0x%x],val[0x%x],readback[0x%x]\n"
				, __func__, host->id, ett_item->reg_addr, ett_item->reg_offset,
				ett_item->value, sdr_read32(base + ett_item->reg_addr));
	}
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR,
		host->saved_para.cmd_resp_ta_cntr);
	host->saved_para.pad_tune0 = sdr_read32(MSDC_PAD_TUNE0);
	host->saved_para.ddly0 = sdr_read32(MSDC_DAT_RDDLY0);
	host->saved_para.ddly1 = sdr_read32(MSDC_DAT_RDDLY1);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR,
		host->saved_para.wrdat_crc_ta_cntr);
	if ((host->id == 0) && (mode == MSDC_HS400_MODE)) {
		sdr_get_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY1,
			host->saved_para.ds_dly1);
		sdr_get_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY3,
			host->saved_para.ds_dly3);
	}
}

/* ops.set_ios */

static void msdc_ops_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);

	spin_lock(&host->lock);
	msdc_ungate_clock(host);

	if (host->power_mode != ios->power_mode) {
		switch (ios->power_mode) {
		case MMC_POWER_OFF:
		case MMC_POWER_UP:
			spin_unlock(&host->lock);
			msdc_init_hw(host);
			msdc_set_power_mode(host, ios->power_mode);
			spin_lock(&host->lock);
			break;
		case MMC_POWER_ON:
		default:
			break;
		}
		host->power_mode = ios->power_mode;
	}

	if (host->bus_width != ios->bus_width) {
		msdc_set_buswidth(host, ios->bus_width);
		host->bus_width = ios->bus_width;
	}

	if (host->timing != ios->timing) {
		if (host->id == 0) {
			if (ios->timing == MMC_TIMING_MMC_HS200) {
				msdc_apply_ett_settings(host, MSDC_HS200_MODE);
			} else if (ios->timing == MMC_TIMING_MMC_HS400) {
				/* switch from eMMC 4.5 backward speed mode to HS400 */
				emmc_hs400_backup();
				msdc_apply_ett_settings(host, MSDC_HS400_MODE);
			}
			/* switch from HS400 to eMMC 4.5 backward speed mode */
			if (host->timing == MMC_TIMING_MMC_HS400)
				emmc_hs400_restore();
		}
		if (ios->timing == MMC_TIMING_MMC_DDR52)
			msdc_set_mclk(host, ios->timing, ios->clock);
#ifdef CONFIG_MMC_FFU
		if ((host->hw->host_function == MSDC_EMMC) &&
		    ((ios->timing == MMC_TIMING_LEGACY) && (ios->clock <= 25000000)))
			emmc_clear_timing();
#endif

		host->timing = ios->timing;
	}
	/* reserve for FFU */
#ifdef CONFIG_MMC_FFU
	if ((ios->timing != MSDC_STATE_HS400) &&
		(host->hw->host_function == MSDC_EMMC))
		msdc_clock_src[host->id] = MSDC50_CLKSRC_200MHZ;
#endif
	if (msdc_clock_src[host->id] != host->hw->clk_src) {
		host->hw->clk_src = msdc_clock_src[host->id];
		msdc_select_clksrc(host, host->hw->clk_src);
	}

	if (host->mclk != ios->clock) {
		msdc_set_mclk(host, ios->timing, ios->clock);
		host->mclk = ios->clock;
	}

	msdc_gate_clock(host, 1);
	spin_unlock(&host->lock);
}

/* ops.get_ro */
static int msdc_ops_get_ro(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	unsigned long flags;
	int ro = 0;

	spin_lock_irqsave(&host->lock, flags);
	msdc_ungate_clock(host);
	if (host->hw->flags & MSDC_WP_PIN_EN) {	/* set for card */
		ro = (sdr_read32(MSDC_PS) >> 31);
	}
	msdc_gate_clock(host, 1);
	spin_unlock_irqrestore(&host->lock, flags);
	return ro;
}

/* ops.get_cd */
static int msdc_ops_get_cd(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base;
	unsigned long flags;
	int level = 0;
	/* int present = 1; */

	base = host->base;
	spin_lock_irqsave(&host->lock, flags);

	/* for sdio, depends on USER_RESUME */
	if (is_card_sdio(host)) {
		if (!(host->hw->flags & MSDC_SDIO_IRQ)) {
			host->card_inserted =
				(host->pm_state.event == PM_EVENT_USER_RESUME) ? 1 : 0;
			/* pr_err("sdio ops_get_cd<%d>\n", host->card_inserted); */
			goto end;
		}
	}

	/* for emmc, MSDC_REMOVABLE not set, always return 1 */
	if (mmc->caps & MMC_CAP_NONREMOVABLE) {
		host->card_inserted = 1;
		goto end;
	}
	/* msdc_ungate_clock(host); */

	if (!(mmc->caps & MMC_CAP_NONREMOVABLE)) {
		level = __gpio_get_value(cd_gpio);
		if (host->hw->cd_level)
			host->card_inserted = (level == 0) ? 0 : 1;
		else
			host->card_inserted = (level == 0) ? 1 : 0;
	} else { /* TODO Check DAT3 pins for card detection */
		host->card_inserted = 1;
	}

	/* host->card_inserted = 1; */
#if 0
	if (host->card_inserted == 0)
		msdc_gate_clock(host, 0);
	else
		msdc_gate_clock(host, 1);
#endif
	if (host->hw->host_function == MSDC_SD && host->block_bad_card)
		host->card_inserted = 0;
	pr_debug("Card insert<%d> Block bad card<%d>\n", host->card_inserted,
		host->block_bad_card);
 end:
	/* enable msdc register dump */
	sd_register_zone[host->id] = 1;

	spin_unlock_irqrestore(&host->lock, flags);
	return host->card_inserted;
}

static void msdc_ops_card_event(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	host->block_bad_card = 0;
	msdc_reset_pwr_cycle_counter(host);
	msdc_reset_crc_tune_counter(host, ALL_TUNE_CNT);
	msdc_reset_tmo_tune_counter(host, ALL_TUNE_CNT);

	/* when detect card, cmd13 will be sent which timeout log is not needed */
	sd_register_zone[host->id] = 0;
}

/* ops.enable_sdio_irq */
static void msdc_ops_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct msdc_hw *hw = host->hw;
	void __iomem *base = host->base;
	unsigned long flags;

	if (hw->flags & MSDC_EXT_SDIO_IRQ) {	/* yes for sdio */
		if (enable)
			hw->enable_sdio_eirq();	/* combo_sdio_enable_eirq */
		else
			hw->disable_sdio_eirq();	/* combo_sdio_disable_eirq */
	} else if (hw->flags & MSDC_SDIO_IRQ) {

		spin_lock_irqsave(&host->sdio_irq_lock, flags);

		if (enable) {
#if (MSDC_DATA1_INT == 1)
			while (1) {
				sdr_set_bits(MSDC_INTEN, MSDC_INT_SDIOIRQ);
				pr_debug("@#0x%08x @e >%d<\n", (sdr_read32(MSDC_INTEN)),
					host->mmc->sdio_irq_pending);
				if ((sdr_read32(MSDC_INTEN) & MSDC_INT_SDIOIRQ) == 0)
					pr_debug("Should never ever get into this >%d<\n",
						host->mmc->sdio_irq_pending);
				else
					break;
			}
#endif
		} else {
#if (MSDC_DATA1_INT == 1)
			sdr_clr_bits(MSDC_INTEN, MSDC_INT_SDIOIRQ);
			pr_debug("@#0x%08x @d\n", (sdr_read32(MSDC_INTEN)));
#endif
		}
		spin_unlock_irqrestore(&host->sdio_irq_lock, flags);
	}
}

static int msdc_ops_switch_volt(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	int err = 0;
	u32 timeout = 100;
	u32 retry = 10;
	u32 status;

	if (host->hw->host_function == MSDC_EMMC)
		return 0;

	if (ios->signal_voltage != MMC_SIGNAL_VOLTAGE_330) {
		/* make sure SDC is not busy (TBC) */
		/* WAIT_COND(!SDC_IS_BUSY(), timeout, timeout); */
		err = (unsigned int)-EIO;
		msdc_retry(sdc_is_busy(), retry, timeout, host->id);
		if (timeout == 0 && retry == 0) {
			err = (unsigned int)-ETIMEDOUT;
			goto out;
		}

		/* pull up disabled CMD and DAT[3:0] to allow card drives them to low */
		/* check if CMD/DATA lines both 0 */
		if ((sdr_read32(MSDC_PS) & ((1 << 24) | (0xF << 16))) == 0) {
			/* pull up disabled in CMD and DAT[3:0] */
			msdc_pin_config(host, MSDC_PIN_PULL_NONE);

			/* change signal from 3.3v to 1.8v for FPGA this can not work */
			if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
#ifdef FPGA_PLATFORM
				hwPowerSwitch_fpga();
#else
				if (host->power_switch)
					host->power_switch(host, 1);
				else
					ERR_MSG("[%s]msdc%d ERROR: No power switch callback,L%d\n",
					__func__, host->id, __LINE__);
#endif
			}
			/* wait at least 5ms for 1.8v signal switching in card */
			mdelay(10);

			/* config clock 10~12MHz mode for volt switch detection by host*/
			/*For FPGA 13MHz clock,this not work */
			msdc_set_mclk(host, MMC_TIMING_LEGACY, 260000);

			/* pull up enabled in CMD and DAT[3:0] */
			msdc_pin_config(host, MSDC_PIN_PULL_UP);
			mdelay(105);

			/* start to detect volt change by providing 1.8v signal to card */
			sdr_set_bits(MSDC_CFG, MSDC_CFG_BV18SDT);

			/* wait at max. 1ms */
			mdelay(1);
			/* ERR_MSG("before read status"); */

			while ((status = sdr_read32(MSDC_CFG)) & MSDC_CFG_BV18SDT)
				;

			if (status & MSDC_CFG_BV18PSS)
				err = 0;
		}
	}
 out:

	return err;
}

int msdc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
/* CCJ fix */
#if 0
	struct msdc_host *host = mmc_priv(mmc);

	if (host->hw->host_function == MSDC_SDIO)
		init_tune_sdio(host);
#endif
	return 0;
}

static struct mmc_host_ops mt_msdc_ops = {
	.post_req = msdc_post_req,
	.pre_req = msdc_pre_req,
	.request = msdc_ops_request,
	.set_ios = msdc_ops_set_ios,
	.get_ro = msdc_ops_get_ro,
	.get_cd = msdc_ops_get_cd,
	.card_event = msdc_ops_card_event,
	.enable_sdio_irq = msdc_ops_enable_sdio_irq,
	.start_signal_voltage_switch = msdc_ops_switch_volt,
	.execute_tuning = msdc_execute_tuning,
};

/*--------------------------------------------------------------------------*/
/* interrupt handler                 */
/*--------------------------------------------------------------------------*/
/* static __tcmfunc irqreturn_t msdc_irq(int irq, void *dev_id) */

static irqreturn_t msdc_irq(int irq, void *dev_id)
{
	struct msdc_host *host = (struct msdc_host *)dev_id;
	struct mmc_host *mmc = host->mmc;
	struct mmc_data *data = host->data;
	struct mmc_command *cmd = host->cmd;
	struct mmc_command *stop = NULL;
	struct mmc_request *mrq = host->mrq;
	void __iomem *base = host->base;
	u32 cmd_arg = host->mrq->cmd->arg;
	u32 cmdsts = MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO | MSDC_INT_CMDRDY |
	    MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO | MSDC_INT_ACMDRDY
	    | MSDC_INT_ACMD19_DONE;
	u32 datsts = MSDC_INT_DATCRCERR | MSDC_INT_DATTMO;
	u32 intsts, inten;

	if (host->hw->flags & MSDC_SDIO_IRQ)
		spin_lock(&host->sdio_irq_lock);	/* ccyeh */

	if (0 == host->core_clkon) {
#ifndef FPGA_PLATFORM
#ifdef CONFIG_MTK_CLKMGR
		enable_clock(MT_CG_PERI_MSDC30_0 + host->id, "SD");
#else
		clk_enable(host->clock_control);
#endif
#endif
		host->core_clkon = 1;
		sdr_set_field(MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);
		intsts = sdr_read32(MSDC_INT);
#if 0
		if (sdr_read32(MSDC_ECO_VER) >= 4) {
			sdr_set_field(MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);	/* E2 */
			intsts = sdr_read32(MSDC_INT);
			sdr_set_field(MSDC_CLKSRC_REG, MSDC1_IRQ_SEL, 0);
		} else {
			intsts = sdr_read32(MSDC_INT);
		}
#endif
	} else {
		intsts = sdr_read32(MSDC_INT);
	}

	latest_int_status[host->id] = intsts;
	inten = sdr_read32(MSDC_INTEN);
#if (MSDC_DATA1_INT == 1)
	if (host->hw->flags & MSDC_SDIO_IRQ) {
		intsts &= inten;
	} else
#endif
	{
		inten &= intsts;
	}

	sdr_write32(MSDC_INT, intsts);	/* clear interrupts */

	/* MSG will cause fatal error */
#if 0
	/* card change interrupt */
	if (intsts & MSDC_INT_CDSC) {
		IRQ_MSG("MSDC_INT_CDSC irq<0x%.8x>", intsts);
		tasklet_hi_schedule(&host->card_tasklet);
		/* tuning when plug card ? */
	}
#endif

	/* sdio interrupt */
	if (host->hw->flags & MSDC_SDIO_IRQ) {
		spin_unlock(&host->sdio_irq_lock);	/* ccyeh */

#if (MSDC_DATA1_INT == 1)
		if (intsts & MSDC_INT_SDIOIRQ)
			mmc_signal_sdio_irq(host->mmc);
#endif
	}

	/* transfer complete interrupt */
	if (data != NULL) {
#ifdef MTK_MSDC_ERROR_TUNE_DEBUG
		if (g_err_tune_dbg_error &&
			(g_err_tune_dbg_count > 0)
			&& (g_err_tune_dbg_host == host->id)) {
			if (g_err_tune_dbg_cmd == (sdr_read32(SDC_CMD) & 0x3f)) {
				if (g_err_tune_dbg_error & MTK_MSDC_ERROR_DAT_TMO) {
					intsts = MSDC_INT_DATTMO;
					g_err_tune_dbg_count--;
				} else if (g_err_tune_dbg_error & MTK_MSDC_ERROR_DAT_CRC) {
					intsts = MSDC_INT_DATCRCERR;
					g_err_tune_dbg_count--;
				}
				pr_err("%s:make error cmd:%d,arg=%d,error type=%d,count=%d\n"
					__func__, g_err_tune_dbg_cmd, g_err_tune_dbg_arg,
					g_err_tune_dbg_error, g_err_tune_dbg_count);
			}
			if ((g_err_tune_dbg_cmd == MMC_STOP_TRANSMISSION)
			    && stop && (host->autocmd & MSDC_AUTOCMD12)) {
				if (g_err_tune_dbg_error & MTK_MSDC_ERROR_ACMD_TMO) {
					intsts = MSDC_INT_ACMDTMO;
					g_err_tune_dbg_count--;
				} else if (g_err_tune_dbg_error & MTK_MSDC_ERROR_ACMD_CRC) {
					intsts = MSDC_INT_ACMDCRCERR;
					g_err_tune_dbg_count--;
				}
				pr_err("[%s]:make CMD12 error,error type=%d,count=%d\n",
					__func__, g_err_tune_dbg_error, g_err_tune_dbg_count);
			}
		}
#endif
		stop = data->stop;
#if (MSDC_DATA1_INT == 1)
	if ((host->hw->flags & MSDC_SDIO_IRQ) && (intsts & MSDC_INT_XFER_COMPL))
		goto done;
	else
#endif
		if (inten & MSDC_INT_XFER_COMPL)
			goto done;

		if (intsts & datsts) {
			/* do basic reset, or stop command will sdc_busy */
			if (intsts & MSDC_INT_DATTMO)
				msdc_dump_info(host->id);
			if (host->dma_xfer)
				msdc_reset(host->id);
			else
				msdc_reset_hw(host->id);

			atomic_set(&host->abort, 1);	/* For PIO mode exit */

			if (intsts & MSDC_INT_DATTMO) {
				data->error = (unsigned int)-ETIMEDOUT;
				ERR_MSG("XXX CMD<%d> Arg<0x%.8x> MSDC_INT_DATTMO",
					host->mrq->cmd->opcode, host->mrq->cmd->arg);
			} else if (intsts & MSDC_INT_DATCRCERR) {
				data->error = (unsigned int)-EIO;
				ERR_MSG("XXX CMD<%d> Arg<0x%.8x> MSDC_INT_DATCRCERR,SDC_DCRC_STS<0x%x>",
					host->mrq->cmd->opcode,	host->mrq->cmd->arg,
					sdr_read32(SDC_DCRC_STS));
			}
			goto tune;
		}
		if ((stop != NULL) && (host->autocmd & MSDC_AUTOCMD12)
			&& (intsts & cmdsts)) {
			if (intsts & MSDC_INT_ACMDRDY) {
				u32 *arsp = &stop->resp[0];
				*arsp = sdr_read32(SDC_ACMD_RESP);
				CMD_MSG("CMD<12> @ addr<0x%8x> resp<0x%.8x>",
					cmd_arg, stop->resp[0]);
			} else if (intsts & MSDC_INT_ACMDCRCERR) {
				stop->error = (unsigned int)-EIO;
				host->error |= REQ_STOP_EIO;
				if (host->dma_xfer)
					msdc_reset(host->id);
				else
					msdc_reset_hw(host->id);
			} else if (intsts & MSDC_INT_ACMDTMO) {
				stop->error = (unsigned int)-ETIMEDOUT;
				host->error |= REQ_STOP_TMO;
				if (host->dma_xfer)
					msdc_reset(host->id);
				else
					msdc_reset_hw(host->id);
			}
			if ((intsts & MSDC_INT_ACMDCRCERR) || (intsts & MSDC_INT_ACMDTMO))
				goto tune;
		}
	}
	/* command interrupts */
	if ((cmd != NULL) && (intsts & cmdsts)) {
#ifdef MTK_MSDC_ERROR_TUNE_DEBUG
		if (g_err_tune_dbg_error && (g_err_tune_dbg_count > 0)
			&& (g_err_tune_dbg_host == host->id)
			&& (g_err_tune_dbg_cmd == cmd->opcode)) {
			if ((g_err_tune_dbg_cmd != MMC_SWITCH)
				|| ((g_err_tune_dbg_cmd == MMC_SWITCH)
				&& (g_err_tune_dbg_arg == ((cmd->arg >> 16) & 0xff)))) {
				if (g_err_tune_dbg_error & MTK_MSDC_ERROR_CMD_TMO)
					intsts = MSDC_INT_CMDTMO;
				else if (g_err_tune_dbg_error & MTK_MSDC_ERROR_CMD_CRC)
					intsts = MSDC_INT_RSPCRCERR;
				g_err_tune_dbg_count--;
				pr_debug("%s:make error cmd:%d,arg=%d,error type=%d,count=%d\n",
					__func__, g_err_tune_dbg_cmd, g_err_tune_dbg_arg,
					g_err_tune_dbg_error, g_err_tune_dbg_count);
			}
		}
#endif
		if (intsts & MSDC_INT_CMDRDY) {
			u32 *rsp = NULL;

			rsp = &cmd->resp[0];
			switch (host->cmd_rsp) {
			case RESP_NONE:
				break;
			case RESP_R2:
				*rsp++ = sdr_read32(SDC_RESP3);
				*rsp++ = sdr_read32(SDC_RESP2);
				*rsp++ = sdr_read32(SDC_RESP1);
				*rsp++ = sdr_read32(SDC_RESP0);
				break;
			default:	/* Response types 1, 3, 4, 5, 6, 7(1b) */
				*rsp = sdr_read32(SDC_RESP0);
				break;
			}

			if (host->hw->host_function == MSDC_SD)
				host->continuous_fail_request_count = 0;
		} else if (intsts & MSDC_INT_RSPCRCERR) {
			cmd->error = (unsigned int)-EIO;
			ERR_MSG("XXX CMD<%d> MSDC_INT_RSPCRCERR Arg<0x%.8x>",
				cmd->opcode, cmd->arg);
			msdc_reset_hw(host->id);
		} else if (intsts & MSDC_INT_CMDTMO) {
			cmd->error = (unsigned int)-ETIMEDOUT;
			ERR_MSG("XXX CMD<%d> MSDC_INT_CMDTMO Arg<0x%.8x>",
				cmd->opcode, cmd->arg);
			msdc_reset_hw(host->id);
		}
		if (intsts & (MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO))
			complete(&host->cmd_done);
	}
	/* mmc irq interrupts */
	if (intsts & MSDC_INT_MMCIRQ)
		/* pr_debug("msdc[%d] MMCIRQ: SDC_CSTS=0x%.8x\r\n",
		host->id, sdr_read32(SDC_CSTS)); */
	latest_int_status[host->id] = 0;
	return IRQ_HANDLED; /* only for normal cmd*/

done:
	data->bytes_xfered = host->dma.xfersz;
	/* if sync request or tune async request use host->xfer_done */
	if (!(msdc_async_use_dma(data->host_cookie)) || !(host->tune == 0)) {
		complete(&host->xfer_done);
	} else {
		msdc_dma_stop(host);
		msdc_dma_clear(host);
		mmc_request_done(mmc, mrq);
		msdc_gate_clock(host, 1);
		host->error &= ~REQ_DAT_ERR;
	}
	if (host->hw->host_function == MSDC_SD)
		host->continuous_fail_request_count = 0;

	return IRQ_HANDLED;

tune:
	if (host->dma_xfer) {
		if ((msdc_async_use_dma(data->host_cookie)) && (host->tune == 0)) {
			msdc_dma_stop(host);
			msdc_clr_fifo(host->id);
			/*msdc_clr_int(); interrupt has been cleared before*/
			/*if msdc_irq too fast to set mrq to host->areq at mmc_start_req */
			host->mrq_tune = host->mrq;
			msdc_dma_clear(host);
			msdc_gate_clock(host, 1);
			/*begin tune:dat/acmd crc/tmo for first time async request*/
			if (!queue_work(wq_tune, &host->work_tune)) {
				pr_err("msdc%d queue work failed BUG_ON,[%s]L:%d\n",
					host->id, __func__, __LINE__);
				BUG();
			}
		} else {
		/* Autocmd12 issued but error, data transfer done INT will not issue,
		 * so cmplete is need here
		 */
			complete(&host->xfer_done);
		}

	} /* PIO mode can't do complete, because not init */

	return IRQ_HANDLED;
}

/*--------------------------------------------------------------------------*/
/* platform_driver members                                                  */
/*--------------------------------------------------------------------------*/

/* Add this function to check if no interrupt back after write.         *
 * It may occur when write crc revice, but busy over data->timeout_ns   */
static void msdc_check_write_timeout(struct work_struct *work)
{
	struct msdc_host *host = container_of(work, struct msdc_host, write_timeout.work);
	struct mmc_data  *data = host->data;
	struct mmc_request *mrq = host->mrq;
	struct mmc_host *mmc = host->mmc;

	u32 status = 0;
	u32 state = 0;
	u32 err = 0;
	unsigned long tmo;

	if (!data || !mrq || !mmc)
		return;

	pr_err("[%s]: XXX DMA Data Write Busy Timeout: %u ms, CMD<%d>",
		__func__, host->write_timeout_ms, mrq->cmd->opcode);

	if (msdc_async_use_dma(data->host_cookie) && (host->tune == 0)) {
		msdc_dump_info(host->id);

		msdc_dma_stop(host);
		msdc_dma_clear(host);
		msdc_reset_hw(host->id);

		tmo = jiffies + POLLING_BUSY;

		spin_lock(&host->lock);
		do {
			err = msdc_get_card_status(mmc, host, &status);
			if (err) {
				ERR_MSG("CMD13 ERR<%d>", err);
				break;
			}

			state = R1_CURRENT_STATE(status);
			ERR_MSG("check card state<%d>", state);
			if (state == R1_STATE_DATA || state == R1_STATE_RCV) {
				ERR_MSG("state<%d> need cmd12 to stop", state);
				msdc_send_stop(host);
			} else if (state == R1_STATE_PRG) {
				ERR_MSG("state<%d> card is busy", state);
				spin_unlock(&host->lock);
				msleep(100);
				spin_lock(&host->lock);
			}

			if (time_after(jiffies, tmo)) {
				ERR_MSG("abort timeout. Card stuck in %d state, bad card! remove it!" , state);
				spin_unlock(&host->lock);
				/*	if (MSDC_SD == host->hw->host_function)
					msdc_set_bad_card_and_remove(host);*/
				spin_lock(&host->lock);
				break;
			}
		} while (state != R1_STATE_TRAN);
		spin_unlock(&host->lock);

		data->error = (unsigned int)-ETIMEDOUT;
		host->sw_timeout++;

		if (mrq->done)
			mrq->done(mrq);

		msdc_gate_clock(host, 1);
		host->error |= REQ_DAT_ERR;
	}
}

/* called by msdc_drv_probe */

static void msdc_init_hw(struct msdc_host *host)
{
	void __iomem *base = host->base;
	struct msdc_hw *hw = host->hw;
	u32 cur_rxdly0, cur_rxdly1;

	/* Power on */
	msdc_pin_reset(host, MSDC_PIN_PULL_UP);
#ifndef FPGA_PLATFORM
#ifdef CONFIG_MTK_CLKMGR
	enable_clock(MT_CG_PERI_MSDC30_0 + host->id, "SD");
#else
	clk_enable(host->clock_control);
#endif
#endif
	host->core_clkon = 1;

	/* Configure to MMC/SD mode */
	sdr_set_field(MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);

	/* Reset */
	msdc_reset_hw(host->id);

	/* Disable card detection */
	sdr_clr_bits(MSDC_PS, MSDC_PS_CDEN);

	/* Disable and clear all interrupts */
	sdr_clr_bits(MSDC_INTEN, sdr_read32(MSDC_INTEN));
	sdr_write32(MSDC_INT, sdr_read32(MSDC_INT));

	/* reset tuning parameter */
#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6753)
	sdr_write32(MSDC_PAD_TUNE0, 0x00000000);
#else
	sdr_write32(MSDC_PAD_TUNE0, 0x00008000);
#endif
	sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY, hw->datwrddly);
	sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRRDLY, hw->cmdrrddly);
	sdr_set_field(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, hw->cmdrddly);

	sdr_write32(MSDC_IOCON, 0x00000000);
	sdr_set_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);

	cur_rxdly0 = ((hw->dat0rddly & 0x1F) << 24) |
		((hw->dat1rddly & 0x1F) << 16) | ((hw->dat2rddly & 0x1F) << 8) |
		((hw->dat3rddly & 0x1F) << 0);
	cur_rxdly1 = ((hw->dat4rddly & 0x1F) << 24) |
		((hw->dat5rddly & 0x1F) << 16) | ((hw->dat6rddly & 0x1F) << 8) |
		((hw->dat7rddly & 0x1F) << 0);
	sdr_write32(MSDC_DAT_RDDLY0, cur_rxdly0);
	sdr_write32(MSDC_DAT_RDDLY1, cur_rxdly1);
	/*lapm:bit6,7 must set to 1,if default not mach,20150515 */
	sdr_write32(MSDC_PATCH_BIT1, 0xFFFE00C9);

	host->saved_para.pad_tune0 = sdr_read32(MSDC_PAD_TUNE0);
	host->saved_para.ddly0 = sdr_read32(MSDC_DAT_RDDLY0);
	host->saved_para.ddly1 = sdr_read32(MSDC_DAT_RDDLY1);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR,
		host->saved_para.cmd_resp_ta_cntr);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR,
		host->saved_para.wrdat_crc_ta_cntr);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_GET_BUSY_MA,
		host->saved_para.write_busy_margin);
	sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_GET_CRC_MA,
		host->saved_para.write_crc_margin);
	/* disable async fifo use interl delay */
	sdr_clr_bits(MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTS);
	sdr_set_bits(MSDC_PATCH_BIT2, MSDC_PB2_CFGRESP);
	sdr_get_field(MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTS,
		host->saved_para.cfg_crcsts_path);
	sdr_get_field(MSDC_PATCH_BIT2, MSDC_PB2_CFGRESP,
		host->saved_para.cfg_cmdrsp_path);
	/* 64T + 48T cmd <-> resp */
	sdr_set_field(MSDC_PATCH_BIT2, MSDC_PB2_RESPWAITCNT, 3);
	sdr_get_field(MSDC_PATCH_BIT2, MSDC_PB2_RESPWAITCNT,
		host->saved_para.resp_wait_cnt);
	/* disable support 64G */
	/* sdr_clr_bits(MSDC_PATCH_BIT2,MSDC_PB2_SUPPORT64G); */

	if (is_card_sdio(host))
		msdc_sdio_set_long_timing_delay_by_freq(host, 50 * 1000 * 1000);

	if (host->id == 0) {
		sdr_get_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY1,
			host->saved_para.ds_dly1);
		sdr_get_field(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY3,
			host->saved_para.ds_dly3);
	}
	/* internal clock: latch read data, not apply to sdio */
	if (!(is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))) {
		host->hw->cmd_edge = 0;	/* tuning from 0 */
		host->hw->rdata_edge = 0;
		host->hw->wdata_edge = 0;
	} else if (hw->flags & MSDC_INTERNAL_CLK) {
		/* sdr_set_bits(MSDC_PATCH_BIT0, MSDC_PATCH_BIT_CKGEN_CK); */
	}

	/* for safety, should clear SDC_CFG.SDIO_INT_DET_EN & set SDC_CFG.SDIO in
	 * pre-loader,uboot,kernel drivers. and SDC_CFG.SDIO_INT_DET_EN will be only
	 * set when kernel driver wants to use SDIO bus interrupt */
	/* Configure to enable SDIO mode. it's must otherwise sdio cmd5 failed */
	sdr_set_bits(SDC_CFG, SDC_CFG_SDIO);

	/* disable detect SDIO device interrupt function */
	sdr_clr_bits(SDC_CFG, SDC_CFG_SDIOIDE);

#ifndef FPGA_PLATFORM
	msdc_set_smt(host, 1);
	msdc_set_driving(host, hw, 0);
#endif

	pr_err("msdc%d drving<clk %d,cmd %d,dat %d>",
		host->id, hw->clk_drv, hw->cmd_drv, hw->dat_drv);

	/* write crc timeout detection */
	sdr_set_field(MSDC_PATCH_BIT0, 1 << 30, 1);

	/* Configure to default data timeout */
	sdr_set_field(SDC_CFG, SDC_CFG_DTOC, DEFAULT_DTOC);

	msdc_set_buswidth(host, MMC_BUS_WIDTH_1);

	N_MSG(FUC, "init hardware done!");
}

/* called by msdc_drv_remove */
static void msdc_deinit_hw(struct msdc_host *host)
{
	void __iomem *base = host->base;

	/* Disable and clear all interrupts */
	sdr_clr_bits(MSDC_INTEN, sdr_read32(MSDC_INTEN));
	sdr_write32(MSDC_INT, sdr_read32(MSDC_INT));

	msdc_set_power_mode(host, MMC_POWER_OFF);	/* make sure power down */
}

void msdc_dump_gpd_bd(int id)
{
	struct msdc_host *host;
	int i = 0;
	struct gpd_t *gpd;
	struct bd_t *bd;

	if (id < 0 || id >= HOST_MAX_NUM)
		pr_err("[%s]: invalid host id: %d\n", __func__, id);

	host = mtk_msdc_host[id];
	if (host == NULL) {
		pr_err("[%s]: host0 or host0->dma is NULL\n", __func__);
		return;
	}
	gpd = host->dma.gpd;
	bd = host->dma.bd;

	pr_err("========== MSDC GPD INFO ==========\n");
	if (gpd == NULL) {
		pr_err("GPD is NULL\n");
		return;
	}
	pr_err("gpd addr:0x%lx\n", (ulong) (host->dma.gpd_addr));
	pr_err("hwo:0x%x, bdp:0x%x, rsv0:0x%x, chksum:0x%x,intr:0x%x,rsv1:0x%x\n",
		gpd->hwo, gpd->bdp,	gpd->rsv0, gpd->chksum, gpd->intr, gpd->rsv1);
	pr_err("nexth4:0x%x,ptrh4:0x%x, next:0x%x, ptr:0x%x, buflen:0x%x,\n",
		(unsigned int)gpd->nexth4, (unsigned int)gpd->ptrh4,
		(unsigned int)gpd->next, (unsigned int)gpd->ptr, gpd->buflen);
	pr_err("extlen:0x%x, arg:0x%x,blknum:0x%x,cmd:0x%x\n",
		gpd->extlen, gpd->arg, gpd->blknum, gpd->cmd);


	pr_err("========== MSDC BD INFO ==========\n");
	if (bd == NULL) {
		pr_err("BD is NULL\n");
		return;
	}
	pr_err("bd addr:0x%lx\n", (ulong) (host->dma.bd_addr));
	for (i = 0; i < host->dma.sglen; i++) {
		pr_err("the %d BD\n", i);
		pr_err("eol:0x%x,rsv0:0x%x,chksum:0x%x,rsv1:0x%x,blkpad:0x%x\n",
			bd->eol, bd->rsv0, bd->chksum, bd->rsv1, bd->blkpad);
		pr_err("dwpad:0x%x,rsv2:0x%x,nexth4:0x%x, ptrh4:0x%x, next:0x%x\n",
			bd->dwpad, bd->rsv2, (unsigned int)bd->nexth4,
			(unsigned int)bd->ptrh4, (unsigned int)bd->next);
		pr_err("ptr:0x%x,buflen:0x%x, rsv3:0x%x\n",
			(unsigned int)bd->ptr, bd->buflen, bd->rsv3);
	}
}

/* init gpd and bd list in msdc_drv_probe */
static void msdc_init_gpd_bd(struct msdc_host *host, struct msdc_dma *dma)
{
	struct gpd_t *gpd = dma->gpd;
	struct bd_t *bd = dma->bd;
	struct bd_t *ptr, *prev;

	/* we just support one gpd */
	int bdlen = MAX_BD_PER_GPD;

	/* init the 2 gpd */
	memset(gpd, 0, sizeof(struct gpd_t) * 2);
	gpd->next = (u32) dma->gpd_addr + sizeof(struct gpd_t);

	/* gpd->intr = 0; */
	gpd->bdp = 1;		/* hwo, cs, bd pointer */
	/* gpd->ptr  = (void*)virt_to_phys(bd); */
	gpd->ptr = (u32) dma->bd_addr;	/* physical address */

	memset(bd, 0, sizeof(struct bd_t) * bdlen);
	ptr = bd + bdlen - 1;
	while (ptr != bd) {
		prev = ptr - 1;
		prev->next = ((u32) dma->bd_addr + sizeof(struct bd_t) * (ptr - bd));
		ptr = prev;
	}
}

#ifdef MSDC_DMA_ADDR_DEBUG
static void msdc_init_dma_latest_address(void)
{
	struct dma_addr *ptr, *prev;
	int bdlen = MAX_BD_PER_GPD;

	memset(msdc_latest_dma_address, 0, sizeof(struct dma_addr) * bdlen);
	ptr = msdc_latest_dma_address + bdlen - 1;
	while (ptr != msdc_latest_dma_address) {
		prev = ptr - 1;
		prev->next = (void *)(msdc_latest_dma_address + sizeof(struct dma_addr)
			* (ptr - msdc_latest_dma_address));
		ptr = prev;
	}

}
#endif

#if 0 /*weiping fix emmc_proc */
struct gendisk *mmc_get_disk(struct mmc_card *card)
{
	struct mmc_blk_data *md;
	/* struct gendisk *disk; */

	BUG_ON(!card);
	md = mmc_get_drvdata(card);
	BUG_ON(!md);
	BUG_ON(!md->disk);

	return md->disk;
}

#if defined(CONFIG_MTK_EMMC_SUPPORT) && defined(CONFIG_PROC_FS)
static struct proc_dir_entry *proc_emmc;

#ifdef CONFIG_MTK_GPT_SCHEME_SUPPORT
static inline int emmc_proc_info(struct seq_file *m, struct hd_struct *this)
{
	char *no_partition_name = "n/a";

	return seq_printf(m, "emmc_p%d: %8.8x %8.8x \"%s\"\n", this->partno,
			  (unsigned int)this->start_sect,
			  (unsigned int)this->nr_sects,
			  ((this->info) ?
			  (char *)(this->info->volname) : no_partition_name));
}
#else
static inline int emmc_proc_info(struct seq_file *m, struct hd_struct *this)
{
	int i = 0;
	char *no_partition_name = "n/a";

	for (i = 0; i < PART_NUM; i++) {
		if (PartInfo[i].partition_idx != 0
			&& PartInfo[i].partition_idx == this->partno)
			break;
	}

	return seq_printf(m, "emmc_p%d: %8.8x %8.8x \"%s\"\n", this->partno,
			  (unsigned int)this->start_sect, (unsigned int)this->nr_sects,
			  (i >= PART_NUM ? no_partition_name : PartInfo[i].name));
}
#endif

static int proc_emmc_show(struct seq_file *m, void *v)
{
	struct disk_part_iter piter;
	struct hd_struct *part;
	struct msdc_host *host;
	struct gendisk *disk;

	/* emmc always in slot0 */
	host = msdc_get_host(MSDC_EMMC, MSDC_BOOT_EN, 0);
	BUG_ON(!host);
	BUG_ON(!host->mmc);
	BUG_ON(!host->mmc->card);
	disk = mmc_get_disk(host->mmc->card);

	seq_puts(m, "partno:    start_sect   nr_sects  partition_name\n");
	disk_part_iter_init(&piter, disk, 0);
	while ((part = disk_part_iter_next(&piter)))
		emmc_proc_info(m, part);
	disk_part_iter_exit(&piter);

	return 0;
}

static int proc_emmc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_emmc_show, NULL);
}

static const struct file_operations proc_emmc_fops = {
	.open = proc_emmc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#endif				/* CONFIG_MTK_EMMC_SUPPORT && CONFIG_PROC_FS */
#endif /*weiping fix emmc_proc */

/* This is called by run_timer_softirq */
static void msdc_timer_pm(unsigned long data)
{
	struct msdc_host *host = (struct msdc_host *)data;
	unsigned long flags;

	spin_lock_irqsave(&host->clk_gate_lock, flags);
	if (host->clk_gate_count == 0) {
		msdc_clksrc_onoff(host, 0);
		N_MSG(CLK, "time out, dsiable clock, clk_gate_count=%d",
			host->clk_gate_count);
	}

	spin_unlock_irqrestore(&host->clk_gate_lock, flags);
}

#ifndef FPGA_PLATFORM
static void msdc_set_host_power_control(struct msdc_host *host)
{

	switch (host->id) {
	case 0:
		if (MSDC_EMMC == host->hw->host_function) {
			host->power_control = msdc_emmc_power;
		} else {
			ERR_MSG("Host function error.Please check host_function<0x%x>",
				host->hw->host_function);
			BUG();
		}
		break;
	case 1:
		if (MSDC_SD == host->hw->host_function) {
			host->power_control = msdc_sd_power;
			host->power_switch = msdc_sd_power_switch;
		} else {
			ERR_MSG("Host function error.Please check host_function<0x%x>",
				host->hw->host_function);
			BUG();
		}
		break;
	case 2:
		if (MSDC_SDIO == host->hw->host_function) {
			host->power_control = msdc_sdio_power;
		} else {
			ERR_MSG("Host function error,Please check host_function<0x%x>",
				host->hw->host_function);
			BUG();
		}
		break;
	default:
		break;
	}
}
#endif				/* end of FPGA_PLATFORM */

void SRC_trigger_signal(int i_on)
{
	if ((ghost != NULL) && (ghost->hw->flags & MSDC_SDIO_IRQ)) {
		pr_debug("msdc2 SRC_trigger_signal %d\n", i_on);
		src_clk_control = i_on;
		if (src_clk_control) {
			msdc_clksrc_onoff(ghost, 1);
			/* mb(); */
				/* if (ghost->mmc->sdio_irq_thread) */
			if (ghost->mmc->sdio_irq_thread &&
				(!atomic_read(&ghost->mmc->sdio_irq_thread_abort))) {
				mmc_signal_sdio_irq(ghost->mmc);
				if (u_msdc_irq_counter < 3)
					pr_debug("msdc2 SRC_trigger_signal mmc_signal_sdio_irq\n");
			}
			/* pr_debug("msdc2 SRC_trigger_signal ghost->id=%d\n",ghost->id); */
		}
	}
}
EXPORT_SYMBOL(SRC_trigger_signal);

#ifdef CONFIG_MTK_HIBERNATION
int msdc_drv_pm_restore_noirq(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct mmc_host *mmc = NULL;
	struct msdc_host *host = NULL;
	u32 l_polarity = 0;

	BUG_ON(pdev == NULL);
	mmc = platform_get_drvdata(pdev);
	host = mmc_priv(mmc);
	if (host->hw->host_function == MSDC_SD) {
		if ((host->id == 1) && (!(mmc->caps & MMC_CAP_NONREMOVABLE))) {
			l_polarity = mt_eint_get_polarity_external(mmc->slot.cd_irq);
			if (l_polarity == MT_POLARITY_LOW)
				host->sd_cd_polarity = 0;
			else
				host->sd_cd_polarity = 1;

			if (!(host->hw->cd_level ^ host->sd_cd_polarity)
				&& host->mmc->card) {
				mmc_card_set_removed(host->mmc->card);
				host->card_inserted = 0;
			}
		} else if ((host->id == 2) && (!(mmc->caps & MMC_CAP_NONREMOVABLE))) {
			/* sdio need handle here */
		}
		host->block_bad_card = 0;
	}
	return 0;
}
#endif

#ifndef CONFIG_MTK_CLKMGR
static int msdc_get_ccf_clk_pointer(struct platform_device *pdev,
				struct msdc_host *host)
{
	int ret = 0;
	struct clk *h_clk;

	if (pdev->id == 0) {
		host->clock_control = devm_clk_get(&pdev->dev, "MSDC0-CLOCK");
		g_msdc0_pll_sel = devm_clk_get(&pdev->dev, "MSDC0_PLL_SEL");
		g_msdc0_pll_800m = devm_clk_get(&pdev->dev, "MSDC0_PLL_800M");
		g_msdc0_pll_400m = devm_clk_get(&pdev->dev, "MSDC0_PLL_400M");
		g_msdc0_pll_200m = devm_clk_get(&pdev->dev, "MSDC0_PLL_200M");
	} else if (pdev->id == 1) {
		host->clock_control = devm_clk_get(&pdev->dev, "MSDC1-CLOCK");
	} else if (pdev->id == 2) {
		host->clock_control = devm_clk_get(&pdev->dev, "MSDC2-CLOCK");
	} else if (pdev->id == 3) {
		host->clock_control = devm_clk_get(&pdev->dev, "sdio-clock");
		h_clk = devm_clk_get(&pdev->dev, "sdio-hclk");
		if (IS_ERR(h_clk)) {
			pr_err("can not get msdc%d clock control\n", pdev->id);
			ret = 1;
			goto out;
		} else {
			clk_prepare_enable(h_clk);
		}
	}
	if (IS_ERR(host->clock_control)) {
		pr_err("can not get msdc%d clock control\n", pdev->id);
		ret = 1;
		goto out;
	} else {
		if (clk_prepare(host->clock_control)) {
			pr_err("can not prepare msdc%d clock control\n", pdev->id);
			ret = 1;
			goto out;
		}
	}
	if (host->id == 0) {
		if (IS_ERR(g_msdc0_pll_sel) || IS_ERR(g_msdc0_pll_800m) ||
		    IS_ERR(g_msdc0_pll_400m) || IS_ERR(g_msdc0_pll_200m)) {
			pr_err("msdc0 error,pll_sel=%p,pll_800=%p,pll_400=%p,pll_200=%p\n",
				g_msdc0_pll_sel, g_msdc0_pll_800m, g_msdc0_pll_400m,
				g_msdc0_pll_200m);
			ret = 1;
			goto out;
		} else {
			if (clk_prepare(g_msdc0_pll_sel)) {
				pr_err("msdc%d can not prepare g_msdc0_pll_sel\n", pdev->id);
				ret = 1;
				goto out;
			}
		}
	}

out:
	return ret;
}
#endif

static void register_sdio_ops(struct msdc_host *host)
{
#ifdef CONFIG_MTK_COMBO_SDIO_SLOT
	if (host->hw) {
		host->hw->request_sdio_eirq  = mt_sdio_ops[CONFIG_MTK_COMBO_SDIO_SLOT].sdio_request_eirq;
		host->hw->enable_sdio_eirq   = mt_sdio_ops[CONFIG_MTK_COMBO_SDIO_SLOT].sdio_enable_eirq;
		host->hw->disable_sdio_eirq  = mt_sdio_ops[CONFIG_MTK_COMBO_SDIO_SLOT].sdio_disable_eirq;
		host->hw->register_pm        = mt_sdio_ops[CONFIG_MTK_COMBO_SDIO_SLOT].sdio_register_pm;
	}
#endif
}

static int msdc_get_pinctl_settings(struct msdc_host *host)
{
	struct mmc_host *mmc = host->mmc;
	struct device_node *np = mmc->parent->of_node;
	struct device_node *pinctl_node;
	struct device_node *pins_cmd_node;
	struct device_node *pins_dat_node;
	struct device_node *pins_clk_node;
	struct device_node *pins_rst_node;
	struct device_node *pins_ds_node;
	struct device_node *pinctl_sdr104_node;
	struct device_node *pinctl_sdr50_node;
	struct device_node *pinctl_ddr50_node;

	/*parse pinctl settings*/
	pinctl_node = of_parse_phandle(np, "pinctl", 0);
	pins_cmd_node = of_get_child_by_name(pinctl_node, "pins_cmd");
	of_property_read_u8(pins_cmd_node, "drive-strength", &host->hw->cmd_drv);

	pins_dat_node = of_get_child_by_name(pinctl_node, "pins_dat");
	of_property_read_u8(pins_dat_node, "drive-strength", &host->hw->dat_drv);

	pins_clk_node = of_get_child_by_name(pinctl_node, "pins_clk");
	of_property_read_u8(pins_clk_node, "drive-strength", &host->hw->clk_drv);

	pins_rst_node = of_get_child_by_name(pinctl_node, "pins_rst");
	of_property_read_u8(pins_rst_node, "drive-strength", &host->hw->rst_drv);

	pins_ds_node = of_get_child_by_name(pinctl_node, "pins_ds");
	of_property_read_u8(pins_ds_node, "drive-strength", &host->hw->ds_drv);

/********************************************************************************************************/
	pinctl_sdr104_node = of_parse_phandle(np, "pinctl_sdr104", 0);
	pins_cmd_node = of_get_child_by_name(pinctl_sdr104_node, "pins_cmd");
	of_property_read_u8(pins_cmd_node, "drive-strength", &host->hw->cmd_drv_sd_18);

	pins_dat_node = of_get_child_by_name(pinctl_sdr104_node, "pins_dat");
	of_property_read_u8(pins_dat_node, "drive-strength", &host->hw->dat_drv_sd_18);

	pins_clk_node = of_get_child_by_name(pinctl_sdr104_node, "pins_clk");
	of_property_read_u8(pins_clk_node, "drive-strength", &host->hw->clk_drv_sd_18);

/********************************************************************************************************/
	pinctl_sdr50_node = of_parse_phandle(np, "pinctl_sdr50", 0);
	pins_cmd_node = of_get_child_by_name(pinctl_sdr50_node, "pins_cmd");
	of_property_read_u8(pins_cmd_node, "drive-strength", &host->hw->cmd_drv_sd_18_sdr50);

	pins_dat_node = of_get_child_by_name(pinctl_sdr50_node, "pins_dat");
	of_property_read_u8(pins_dat_node, "drive-strength", &host->hw->dat_drv_sd_18_sdr50);

	pins_clk_node = of_get_child_by_name(pinctl_sdr50_node, "pins_clk");
	of_property_read_u8(pins_clk_node, "drive-strength", &host->hw->clk_drv_sd_18_sdr50);

/********************************************************************************************************/
	pinctl_ddr50_node = of_parse_phandle(np, "pinctl_ddr50", 0);
	pins_cmd_node = of_get_child_by_name(pinctl_ddr50_node, "pins_cmd");
	of_property_read_u8(pins_cmd_node, "drive-strength", &host->hw->cmd_drv_sd_18_ddr50);

	pins_dat_node = of_get_child_by_name(pinctl_ddr50_node, "pins_dat");
	of_property_read_u8(pins_dat_node, "drive-strength", &host->hw->dat_drv_sd_18_ddr50);

	pins_clk_node = of_get_child_by_name(pinctl_ddr50_node, "pins_clk");
	of_property_read_u8(pins_clk_node, "drive-strength", &host->hw->clk_drv_sd_18_ddr50);

	return 0;
}

static int msdc_get_rigister_settings(struct msdc_host *host)
{
	struct mmc_host *mmc = host->mmc;
	struct device_node *np = mmc->parent->of_node;
	struct device_node *register_setting_node = NULL;

	/*parse hw property settings*/
	register_setting_node = of_parse_phandle(np, "register_setting", 0);
	if (register_setting_node) {
		of_property_read_u8(register_setting_node, "dat0rddly", &host->hw->dat0rddly);
		of_property_read_u8(register_setting_node, "dat1rddly", &host->hw->dat1rddly);
		of_property_read_u8(register_setting_node, "dat2rddly", &host->hw->dat2rddly);
		of_property_read_u8(register_setting_node, "dat3rddly", &host->hw->dat3rddly);
		of_property_read_u8(register_setting_node, "dat4rddly", &host->hw->dat4rddly);
		of_property_read_u8(register_setting_node, "dat5rddly", &host->hw->dat5rddly);
		of_property_read_u8(register_setting_node, "dat6rddly", &host->hw->dat6rddly);
		of_property_read_u8(register_setting_node, "dat7rddly", &host->hw->dat7rddly);

		of_property_read_u8(register_setting_node, "datwrddly", &host->hw->datwrddly);
		of_property_read_u8(register_setting_node, "cmdrrddly", &host->hw->cmdrrddly);
		of_property_read_u8(register_setting_node, "cmdrddly", &host->hw->cmdrddly);

		of_property_read_u8(register_setting_node, "cmd_edge", &host->hw->cmd_edge);
		of_property_read_u8(register_setting_node, "rdata_edge", &host->hw->rdata_edge);
		of_property_read_u8(register_setting_node, "wdata_edge", &host->hw->wdata_edge);
	} else {
		pr_err("[MSDC%d] register_setting is not found in DT.\n", host->id);
		return 1;
	}
/*parse ett*/
#ifdef CFG_DEV_MSDC0
	if (of_property_read_u32(register_setting_node, "ett-hs200-cells", &host->hw->ett_hs200_count))
		pr_err("[MSDC] ett-hs200-cells is not found in DT.\n");
	    host->hw->ett_hs200_settings =
		kzalloc(sizeof(struct msdc_ett_settings) * host->hw->ett_hs200_count, GFP_KERNEL);

	if (MSDC_EMMC == host->hw->host_function
		&& !of_property_read_u32_array(register_setting_node, "ett-hs200-customer",
		host->hw->ett_hs200_settings, host->hw->ett_hs200_count * 3)) {
		pr_err("[MSDC%d] hs200 ett setting for customer is found in DT.\n", host->id);
	} else if (MSDC_EMMC == host->hw->host_function
		&& !of_property_read_u32_array(register_setting_node, "ett-hs200-default",
		host->hw->ett_hs200_settings, host->hw->ett_hs200_count * 3)) {
		pr_err("[MSDC%d] hs200 ett setting for default is found in DT.\n", host->id);
	} else if (MSDC_EMMC == host->hw->host_function) {
		pr_err("[MSDC%d]error: hs200 ett setting is not found in DT.\n", host->id);
	}

	if (of_property_read_u32(register_setting_node, "ett-hs400-cells", &host->hw->ett_hs400_count))
		pr_err("[MSDC] ett-hs400-cells is not found in DT.\n");
	host->hw->ett_hs400_settings =
		kzalloc(sizeof(struct msdc_ett_settings) * host->hw->ett_hs400_count, GFP_KERNEL);

	if (MSDC_EMMC == host->hw->host_function
		&& !of_property_read_u32_array(register_setting_node, "ett-hs400-customer",
		host->hw->ett_hs400_settings, host->hw->ett_hs400_count * 3)) {
		pr_err("[MSDC%d] hs400 ett setting for customer is found in DT.\n", host->id);
	} else if (MSDC_EMMC == host->hw->host_function
		&& !of_property_read_u32_array(register_setting_node, "ett-hs400-default",
		host->hw->ett_hs400_settings, host->hw->ett_hs400_count * 3)) {
		pr_err("[MSDC%d] hs400 ett setting for default is found in DT.\n", host->id);
	} else if (MSDC_EMMC == host->hw->host_function) {
		pr_err("[MSDC%d]error: hs400 ett setting is not found in DT.\n", host->id);
	}
#endif
	return 0;
}

/**
 *	msdc_of_parse() - parse host's device-tree node
 *	@host: host whose node should be parsed.
 *
 */
int msdc_of_parse(struct mmc_host *mmc)
{
	struct device_node *np;
	struct msdc_host *host = mmc_priv(mmc);
	int len = 0;
	unsigned char cd_level = false;
	int read_tmp = 0;

	if (!mmc->parent || !mmc->parent->of_node)
		return 1;

	np = mmc->parent->of_node;
	host->mmc = mmc;  /* msdc_check_init_done() need */
	host->hw = kzalloc(sizeof(struct msdc_hw), GFP_KERNEL);

	/*basic settings*/
	if (0 == strcmp(np->name, "MSDC0"))
		host->id = 0;
	else if (0 == strcmp(np->name, "MSDC1"))
		host->id = 1;
	else if (0 == strcmp(np->name, "MSDC2"))
		host->id = 2;
	else if (0 == strcmp(np->name, "sdio")) {
		host->id = 3;
		host->hw->flags |= MSDC_EXT_SDIO_IRQ;
	}

	pr_err("of msdc DT probe %s!, hostId:%d\n", np->name, host->id);

	/* iomap register */
	host->base = of_iomap(np, 0);
	if (!host->base) {
		pr_err("can't of_iomap for msdc!!\n");
		return -ENOMEM;
	}
	pr_err("of_iomap for msdc @ 0x%p\n", host->base);

	/* get irq #  */
	host->irq = irq_of_parse_and_map(np, 0);
	pr_err("msdc get irq # %d\n", host->irq);
	BUG_ON(host->irq < 0);

	/* get clk_src */
	read_tmp = 0;
	if (of_property_read_u32(np, "clk_src", &read_tmp))
		pr_err("[MDSC%d] error: clk_src isn't found in DT.\n", host->id);
	else
		host->hw->clk_src = read_tmp;

	/* get msdc flag(caps)*/
	if (of_find_property(np, "msdc-sys-suspend", &len))
		host->hw->flags |= MSDC_SYS_SUSPEND;

	/*Returns 0 on success, -EINVAL if the property does not exist,
	* -ENODATA if property does not have a value, and -EOVERFLOW if the
	* property data isn't large enough.*/

	read_tmp = 0;
	if (of_property_read_u32(np, "host-function", &read_tmp))
		pr_err("[MSDC%d] host_function isn't found in DT\n", host->id);
	else
		host->hw->host_function = read_tmp;

	if (of_find_property(np, "bootable", &len))
		host->hw->boot = 1;

	/*get cd_level*/
	of_property_read_u8(np, "cd_level", &cd_level);
	if (host->hw)
		host->hw->cd_level = cd_level;

	/*get cd_gpio*/
	of_property_read_u32_index(np, "cd-gpios", 1, &cd_gpio);

	msdc_get_rigister_settings(host);
	msdc_get_pinctl_settings(host);
	register_sdio_ops(host);
	return 0;
}

static int msdc_drv_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
#ifndef CONFIG_MTK_LEGACY
	struct device_node *msdc_node;
#endif
	struct msdc_host *host;
	void __iomem *base;
	int ret;
	struct irq_data l_irq_data;
	/*unsigned int msdc_hw_parameter[sizeof(struct tag_msdc_hw_para) / 4];
	unsigned int msdc_custom[1];*/
#ifdef CFG_DEV_MSDC0
	struct device_node *msdc_cust_node = NULL;
#endif

#ifdef FPGA_PLATFORM
	u16 l_val;
#endif

	if (0 == strcmp(pdev->dev.of_node->name, "MSDC0")) {
#ifndef CFG_DEV_MSDC0
		return 1;
#endif
	} else if (0 == strcmp(pdev->dev.of_node->name, "MSDC1")) {
#ifndef CFG_DEV_MSDC1
		return 1;
#endif
	} else if (0 == strcmp(pdev->dev.of_node->name, "MSDC2")) {
#ifndef CFG_DEV_MSDC2
		return 1;
#endif
	} else if (0 == strcmp(pdev->dev.of_node->name, "sdio")) {
#ifndef CFG_DEV_MSDC3
		return 1;
#endif
	}

	/* Allocate MMC host for this device */
	mmc = mmc_alloc_host(sizeof(struct msdc_host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;

	if (mmc_of_parse(mmc) || msdc_of_parse(mmc)) {
		pr_err("DT happens error for msdc!!\n");
		mmc_free_host(mmc);
		return 1;
	}
	host = mmc_priv(mmc);
	base = host->base;

	l_irq_data.irq = host->irq;

	if (gpio_node == NULL) {
		gpio_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-pctl-a-syscfg");
		gpio_reg_base = of_iomap(gpio_node, 0);
		pr_err("of_iomap for gpio base @ 0x%p\n", gpio_reg_base);
	}

	if (infracfg_ao_node == NULL) {
		infracfg_ao_node = of_find_compatible_node(NULL, NULL,
			"mediatek,INFRACFG_AO");
		infracfg_ao_reg_base = of_iomap(infracfg_ao_node, 0);
		pr_debug("of_iomap for infracfg_ao base @ 0x%p\n",
			infracfg_ao_reg_base);
	}
	if (infracfg_node == NULL) {
		infracfg_node = of_find_compatible_node(NULL, NULL,
			"mediatek,mt8173-infracfg");
		infracfg_reg_base = of_iomap(infracfg_node, 0);
		pr_debug("of_iomap for infracfg base @ 0x%p\n", infracfg_reg_base);
	}

	if (pericfg_node == NULL) {
		pericfg_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-pericfg");
		pericfg_reg_base = of_iomap(pericfg_node, 0);
		pr_debug("of_iomap for pericfg base @ 0x%p\n", pericfg_reg_base);
	}

	if (emi_node == NULL) {
		emi_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-dcm");
		emi_reg_base = of_iomap(emi_node, 8);
		pr_debug("of_iomap for emi base @ 0x%p\n", emi_reg_base);
	}

	if (toprgu_node == NULL) {
		toprgu_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-rgu");
		toprgu_reg_base = of_iomap(toprgu_node, 0);
		pr_debug("of_iomap for toprgu base @ 0x%p\n", toprgu_reg_base);
	}

	if (apmixed_node == NULL) {
		apmixed_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-apmixedsys");
		apmixed_reg_base1 = of_iomap(apmixed_node, 0);
		pr_err("of_iomap for APMIXED base @ 0x%p\n", apmixed_reg_base1);
	}

	if (topckgen_node == NULL) {
		topckgen_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-topckgen");
		topckgen_reg_base = of_iomap(topckgen_node, 0);
		pr_err("of_iomap for TOPCKGEN base @ 0x%p\n", topckgen_reg_base);
	}

#ifndef CONFIG_MTK_LEGACY
	/* backup original dev.of_node */
	msdc_node = pdev->dev.of_node;
	/* get regulator supply node */
	pdev->dev.of_node = of_find_compatible_node(NULL, NULL,
		"mediatek,mt_pmic_regulator_supply");
	if (reg_VEMC_3V3 == NULL)
		reg_VEMC_3V3 = regulator_get(&(pdev->dev), "VEMC_3V3");
	if (reg_VMC == NULL)
		reg_VMC = regulator_get(&(pdev->dev), "VMC");
	if (reg_VMCH == NULL)
		reg_VMCH = regulator_get(&(pdev->dev), "VMCH");
	/* restore original dev.of_node */
	pdev->dev.of_node = msdc_node;
#endif
#ifdef FPGA_PLATFORM
	if (fpga_pwr_gpio == NULL) {
		fpga_pwr_gpio = of_iomap(pdev->dev.of_node, 1);
		fpga_pwr_gpio_eo = fpga_pwr_gpio + 0x4;
		pr_err("FPAG PWR_GPIO, PWR_GPIO_EO address 0x%p, 0x%p\n",
			fpga_pwr_gpio, fpga_pwr_gpio_eo);
	}

	l_val = sdr_read16(PWR_GPIO_EO);
	sdr_write16(PWR_GPIO_EO, (l_val |	/* PWR_GPIO_L4_DIR | */
				  PWR_MASK_EN | PWR_MASK_VOL_33 | PWR_MASK_VOL_18));

	l_val = sdr_read16(PWR_GPIO_EO);
	pr_debug("[%s]: pwr gpio dir = 0x%x\n", __func__, l_val);
#endif
#ifdef CFG_DEV_MSDC0
	/* Get custom node in Device tree, if not set, use default */
	if (strcmp(pdev->dev.of_node->name, "MSDC0") == 0) {
		pdev->id = 0;
		msdc_cust_node = of_find_compatible_node(NULL, NULL,
			"mediatek,MSDC0_custom");
	}
#endif
#ifdef CFG_DEV_MSDC1
	if (strcmp(pdev->dev.of_node->name, "MSDC1") == 0) {
		pdev->id = 1;
		msdc_cust_node = of_find_compatible_node(NULL, NULL,
			"mediatek,MSDC1_custom");
	}
#endif
#if defined(CFG_DEV_MSDC2)
	if (strcmp(pdev->dev.of_node->name, "MSDC2") == 0) {
		pdev->id = 2;
		pr_err("platform_data hw:0x%p, is msdc2_hw\n", host->hw);
	}
#endif
#if defined(CFG_DEV_MSDC3)
	if (strcmp(pdev->dev.of_node->name, "sdio") == 0) {
		host->mmc->pm_flags |= MMC_PM_KEEP_POWER;
		host->mmc->pm_caps |= MMC_PM_KEEP_POWER;
		mmc->caps |= MMC_CAP_NONREMOVABLE;
		pdev->id = 3;
		host->hw->cmd_drv = 3;
		host->hw->clk_drv = 3;
		host->hw->dat_drv = 3;
		host->hw->cmdrtactr_sdr50 = 0x1;
		host->hw->wdatcrctactr_sdr50 = 0x1;
		host->hw->intdatlatcksel_sdr50 = 0x0;
		host->hw->cmdrtactr_sdr200 = 0x3;
		host->hw->wdatcrctactr_sdr200 = 0x3;
		host->hw->intdatlatcksel_sdr200 = 0x0;
		pr_err("platform_data hw:0x%p, is msdc3_hw\n", host->hw);
	}
#endif

	if ((pdev->id == 1) && (host->hw->host_function == MSDC_SD)
		&& (eint_node == NULL)) {
		eint_node = of_find_compatible_node(NULL, NULL,
			"mediatek, MSDC1_INS-eint");
		if (eint_node) {
			pr_debug("find MSDC1_INS-eint node!!\n");

			/* get irq #  */
			if (!cd_irq)
				cd_irq = irq_of_parse_and_map(eint_node, 0);
			if (!cd_irq)
				pr_debug("can't irq_of_parse_and_map for card detect eint!!\n");
			else
				pr_debug("msdc1 EINT get irq # %d\n", cd_irq);
		} else
			pr_debug("can't find MSDC1_INS-eint compatible node\n");
	}

	/* Set host parameters to mmc */
	mmc->ops = &mt_msdc_ops;
	mmc->f_min = HOST_MIN_MCLK;
	mmc->ocr_avail = MSDC_OCR_AVAIL;

	/* For sd card: MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_CD_PIN_EN |
					MSDC_REMOVABLE | MSDC_HIGHSPEED,
	   For sdio   : MSDC_EXT_SDIO_IRQ | MSDC_HIGHSPEED */
	if ((host->hw->flags & MSDC_SDIO_IRQ) || (host->hw->flags & MSDC_EXT_SDIO_IRQ))
		mmc->caps |= MMC_CAP_SDIO_IRQ;	/* yes for sdio */

#ifdef MTK_MSDC_USE_CMD23
	if (host->hw->host_function == MSDC_EMMC)
		mmc->caps |= MMC_CAP_ERASE | MMC_CAP_CMD23;
	else
		mmc->caps |= MMC_CAP_ERASE;
#else
	mmc->caps |= MMC_CAP_ERASE;
#endif
	mmc->max_busy_timeout = 0;
	/* MMC core transfer sizes tunable parameters */
	mmc->max_segs = MAX_HW_SGMTS;
	if (host->hw->host_function == MSDC_SDIO)
		mmc->max_seg_size = MAX_SGMT_SZ_SDIO;
	else
		mmc->max_seg_size = MAX_SGMT_SZ;
	mmc->max_blk_size = HOST_MAX_BLKSZ;
	mmc->max_req_size = MAX_REQ_SZ;
	mmc->max_blk_count = MAX_REQ_SZ / 512;	/*mmc->max_req_size; */
	host->dma_mask = DMA_BIT_MASK(32);
	mmc_dev(mmc)->dma_mask = &host->dma_mask;

#ifndef FPGA_PLATFORM
	if (pdev->id == 0)
		hclks = hclks_msdc50;
	else
		hclks = hclks_msdc30;
#endif
	host->error = 0;

	/* mclk: the request clock of mmc sub-system */
	host->mclk = 0;
	/* hclk: clock of clock source to msdc controller */
	host->hclk = hclks[host->hw->clk_src];
	/* sclk: the really clock after divition */
	host->sclk = 0;
	host->pm_state = PMSG_RESUME;
	host->suspend = 0;
#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT	/* same as CONFIG_SDIOAUTOK_SUPPORT */
	host->sdio_performance_vcore = 0;
	INIT_DELAYED_WORK(&(host->set_vcore_workq), sdio_unreq_vcore);
#endif
	host->core_clkon = 0;
	host->card_clkon = 0;
	host->clk_gate_count = 0;
	host->core_power = 0;
	host->power_mode = MMC_POWER_OFF;
	host->power_control = NULL;
	host->power_switch = NULL;
#ifndef CONFIG_MTK_CLKMGR
	if (msdc_get_ccf_clk_pointer(pdev, host))
		return 1;
#endif
#ifndef FPGA_PLATFORM
	msdc_set_host_power_control(host);
	/* work around:hot-plug project SD card LDO alway on if no SD card insert */
	if ((host->hw->host_function == MSDC_SD)
		&& (!(host->mmc->caps & MMC_CAP_NONREMOVABLE))) {
		msdc_sd_power(host, 1);
		msdc_sd_power(host, 0);
	}
#endif

	/*
	 * mmc_rescan if check host->caps & NONREMOVABLE not call host->ops->get_cd
	 * host->card_inserted cat not be set to 1 for eMMC, so set this flag here
	 */
	host->card_inserted = (host->mmc->caps & MMC_CAP_NONREMOVABLE) ? 1 : 0;
	host->timeout_ns = 0;
	host->timeout_clks = DEFAULT_DTOC * 1048576;

#ifndef MTK_MSDC_USE_CMD23
	if (host->hw->host_function != MSDC_SDIO)
		host->autocmd |= MSDC_AUTOCMD12;
	else
		host->autocmd &= ~MSDC_AUTOCMD12;
#else
	if (host->hw->host_function == MSDC_EMMC) {
		host->autocmd &= ~MSDC_AUTOCMD12;

#if (1 == MSDC_USE_AUTO_CMD23)
		host->autocmd |= MSDC_AUTOCMD23;
#endif

	} else if (host->hw->host_function == MSDC_SD) {
		host->autocmd |= MSDC_AUTOCMD12;
	} else {
		host->autocmd &= ~MSDC_AUTOCMD12;
	}
#endif				/* end of MTK_MSDC_USE_CMD23 */
#ifdef MTK_MSDC_USE_CACHE
	if (host->hw->host_function == MSDC_EMMC)
		msdc_set_cache_quirk(host);
#endif
	host->mrq = NULL;
	/* init_MUTEX(&host->sem); */
	/* we don't need to support multiple threads access */

	host->dma.used_gpd = 0;
	host->dma.used_bd = 0;

	/* using dma_alloc_coherent */
	/* todo: using 1, for all 4 slots */
	host->dma.gpd = dma_alloc_coherent(&pdev->dev,
		MAX_GPD_NUM * sizeof(struct gpd_t),	&host->dma.gpd_addr, GFP_KERNEL);
	host->dma.bd = dma_alloc_coherent(&pdev->dev,
		MAX_BD_NUM * sizeof(struct bd_t), &host->dma.bd_addr, GFP_KERNEL);
	BUG_ON((!host->dma.gpd) || (!host->dma.bd));
	msdc_init_gpd_bd(host, &host->dma);
	msdc_clock_src[host->id] = host->hw->clk_src;
	msdc_host_mode[host->id] = mmc->caps;
	msdc_host_mode2[host->id] = mmc->caps2;
	/*for emmc */
	mtk_msdc_host[pdev->id] = host;
	host->write_timeout_uhs104 = 0;
	host->write_timeout_emmc = 0;
	host->read_timeout_uhs104 = 0;
	host->read_timeout_emmc = 0;
	host->sw_timeout = 0;
	host->tune = 0;
	host->timing = 0;
	host->sd_cd_insert_work = 0;
	host->block_bad_card = 0;
	host->sd_30_busy = 0;
	msdc_reset_tmo_tune_counter(host, ALL_TUNE_CNT);
	msdc_reset_pwr_cycle_counter(host);

	if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ)) {
		host->saved_para.suspend_flag = 0;
		host->saved_para.msdc_cfg = 0;
		host->saved_para.mode = 0;
		host->saved_para.div = 0;
		host->saved_para.sdc_cfg = 0;
		host->saved_para.iocon = 0;
		host->saved_para.timing = 0;
		host->saved_para.hz = 0;
		host->saved_para.cmd_resp_ta_cntr = 0;	/* for SDIO 3.0 */
		host->saved_para.wrdat_crc_ta_cntr = 0;	/* for SDIO 3.0 */
		host->saved_para.int_dat_latch_ck_sel = 0;	/* for SDIO 3.0 */
		host->saved_para.ckgen_msdc_dly_sel = 0;	/* for SDIO 3.0 */
		host->saved_para.inten_sdio_irq = 0;	/* default disable */
		host->saved_para.cfg_cmdrsp_path = 0;
		host->saved_para.cfg_crcsts_path = 0;
		wakeup_source_init(&host->trans_lock, "MSDC Transfer Lock");
	}

	/*weiping fix sd hot-plug*/
	/*tasklet_init(&host->card_tasklet, msdc_tasklet_card, (ulong) host);*/
#ifdef MTK_SDIO30_ONLINE_TUNING_SUPPORT
	atomic_set(&host->ot_done, 1);
	atomic_set(&host->sdio_stopping, 0);
	host->ot_work.host = host;
	host->ot_work.chg_volt = 0;
	/* ccyeh */ atomic_set(&host->ot_work.ot_disable, 0);
	atomic_set(&host->ot_work.ot_disable, 1);	/* ccyeh */
	atomic_set(&host->ot_work.autok_done, 0);
#endif
	/* INIT_DELAYED_WORK(&host->remove_card, msdc_remove_card); */

	INIT_DELAYED_WORK(&host->write_timeout, msdc_check_write_timeout);

	spin_lock_init(&host->lock);
	spin_lock_init(&host->clk_gate_lock);
	spin_lock_init(&host->remove_bad_card);
	spin_lock_init(&host->sdio_irq_lock);
	/* init dynamtic timer */
	init_timer(&host->timer);
	/* host->timer.expires = jiffies + HZ; */
	host->timer.function = msdc_timer_pm;
	host->timer.data = (unsigned long)host;

	ret = request_irq((unsigned int)host->irq, msdc_irq, IRQF_TRIGGER_NONE, DRV_NAME,
			host);
	if (ret)
		goto release;
	/* not set for sdio */
	/* set to combo_sdio_request_eirq() for WIFI */
	/* msdc_eirq_sdio() will be called when EIRQ */
	if (host->hw->request_sdio_eirq)
		host->hw->request_sdio_eirq(msdc_eirq_sdio, (void *)host);

#ifdef CONFIG_PM
	if (host->hw->register_pm) {	/* yes for sdio */
		host->hw->register_pm(msdc_pm, (void *)host);	/* combo_sdio_register_pm() */
		if (host->hw->flags & MSDC_SYS_SUSPEND) {	/* will not set for WIFI */
			ERR_MSG("MSDC_SYS_SUSPEND and register_pm both set");
		}
		/* pm not controlled by system but by client. */
		mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;
	}
#endif

	platform_set_drvdata(pdev, mmc);

#ifdef CONFIG_MTK_HIBERNATION
	if (pdev->id == 1)
		register_swsusp_restore_noirq_func(ID_M_MSDC, msdc_drv_pm_restore_noirq,
			&(pdev->dev));
#endif

	/* Config card detection pin and enable interrupts */
	if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE)) {	/* set for card */
		sdr_clr_bits(MSDC_PS, MSDC_PS_CDEN);
		sdr_clr_bits(MSDC_INTEN, MSDC_INTEN_CDSC);
		sdr_clr_bits(SDC_CFG, SDC_CFG_INSWKUP);
	}
	/*config tune at workqueue*/
	if (!wq_tune) {
		wq_tune = create_workqueue("msdc-tune");
		if (!wq_tune) {
			ret = 1;
			goto free_irq;
		}
	}
	INIT_WORK(&host->work_tune, msdc_async_tune);
	host->mrq_tune = NULL;

	ret = mmc_add_host(mmc);
	if (ret)
		goto free_irq;
	if (host->hw->flags & MSDC_SDIO_IRQ) {
		ghost = host;
		sdr_set_bits(SDC_CFG, SDC_CFG_SDIOIDE);	/* enable sdio detection */
	}

	/* if (hw->flags & MSDC_CD_PIN_EN) */
	host->sd_cd_insert_work = 1;

#ifdef DEBUG_TEST_FOR_SIGNAL
	/* use EINT1 for trigger signal */
	/* need to remove gpio warning log at
	 * mediatek/kernel/include/mach/mt_gpio_core.h
	 * mediatek/platform/{project}/kernel/drivers/gpio/mt_gpio_affix.c */
	mt_set_gpio_mode(1, GPIO_MODE_00);
	mt_set_gpio_dir(1, GPIO_DIR_OUT);
	mt_set_gpio_pull_enable(1, 1);

	mt_set_gpio_out(1, 0);	/* 1-high, 0-low */
#endif

#ifdef MTK_MSDC_BRINGUP_DEBUG

	pr_debug("[%s]: msdc%d, mmc->caps=0x%x, mmc->caps2=0x%x\n",
		__func__, host->id, mmc->caps, mmc->caps2);
	msdc_dump_clock_sts(host);
#endif

	if (host->hw->host_function == MSDC_EMMC)
		host->mmc->pm_flags |= MMC_PM_KEEP_POWER;

#ifdef FPGA_PLATFORM
#if 0				/*def CONFIG_MTK_EMMC_SUPPORT */
	pr_debug("[%s]: waiting emmc init complete\n", __func__);
	host->mmc->card_init_wait(host->mmc);
	pr_debug("[%s]: start read write compare test\n", __func__);
	emmc_multi_rw_compare(0, 0x200, 0xf);
	pr_debug("[%s]: finish read write compare test\n", __func__);
#endif
#endif

	return 0;

 free_irq:
	free_irq(host->irq, host);
	pr_err("[%s]: msdc%d init fail free irq!\n", __func__, host->id);
 release:
	platform_set_drvdata(pdev, NULL);
	msdc_deinit_hw(host);
	pr_err("[%s]: msdc%d init fail release!\n", __func__, host->id);

	tasklet_kill(&host->card_tasklet);

	mmc_free_host(mmc);
	if (wq_tune)
		destroy_workqueue(wq_tune);
	return ret;
}

/* 4 device share one driver, using "drvdata" to show difference */
static int msdc_drv_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;
	struct resource *mem;

	mmc = platform_get_drvdata(pdev);
	BUG_ON(!mmc);

	host = mmc_priv(mmc);
	BUG_ON(!host);

	ERR_MSG("removed !!!");
#ifndef CONFIG_MTK_CLKMGR
	/* clock unprepare */
	if (host->clock_control)
		clk_unprepare(host->clock_control);
	if ((host->hw->host_function == MSDC_EMMC) && g_msdc0_pll_sel)
		clk_unprepare(g_msdc0_pll_sel);
#endif
	platform_set_drvdata(pdev, NULL);
	mmc_remove_host(host->mmc);
	msdc_deinit_hw(host);

	tasklet_kill(&host->card_tasklet);

	free_irq(host->irq, host);

	dma_free_coherent(NULL, MAX_GPD_NUM * sizeof(struct gpd_t), host->dma.gpd,
		host->dma.gpd_addr);
	dma_free_coherent(NULL, MAX_BD_NUM * sizeof(struct bd_t), host->dma.bd,
		host->dma.bd_addr);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (mem)
		release_mem_region(mem->start, mem->end - mem->start + 1);

	mmc_free_host(host->mmc);

	if (wq_tune)
		destroy_workqueue(wq_tune);

	return 0;
}

#ifdef CONFIG_PM
static int msdc_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;

	if (mmc && state.event == PM_EVENT_SUSPEND
		&& (host->hw->flags & MSDC_SYS_SUSPEND))
		msdc_pm(state, (void *)host);

	/* WIFI slot should be off when enter suspend */
	if (mmc && state.event == PM_EVENT_SUSPEND
		&& (!(host->hw->flags & MSDC_SYS_SUSPEND))) {
		msdc_suspend_clock(host);
		if (host->error == -EBUSY) {
			ret = host->error;
			host->error = 0;
		}
	}

	if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ)) {
		if (host->clk_gate_count > 0) {
			host->error = 0;
			return -EBUSY;
		}
		if (host->saved_para.suspend_flag == 0) {
			host->saved_para.hz = host->mclk;
			if (host->saved_para.hz) {
				host->saved_para.suspend_flag = 1;
				/* mb(); */
				msdc_ungate_clock(host);
				sdr_get_field(MSDC_CFG, MSDC_CFG_CKMOD, host->saved_para.mode);
				sdr_get_field(MSDC_CFG, MSDC_CFG_CKDIV, host->saved_para.div);
				sdr_get_field(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
					host->saved_para.int_dat_latch_ck_sel);	/* for SDIO 3.0 */
				sdr_get_field(MSDC_PATCH_BIT0, MSDC_PB0_CKGEN_MSDC_DLY_SEL,
					host->saved_para.ckgen_msdc_dly_sel);	/* for SDIO 3.0 */
				sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_CMD_RSP_TA_CNTR,
					host->saved_para.cmd_resp_ta_cntr);	/* for SDIO 3.0 */
				sdr_get_field(MSDC_PATCH_BIT1, MSDC_PB1_WRDAT_CRCS_TA_CNTR,
					host->saved_para.wrdat_crc_ta_cntr);	/* for SDIO 3.0 */
				/* get INTEN status for SDIO */
				sdr_get_field(MSDC_INTEN, MSDC_INT_SDIOIRQ,
					host->saved_para.inten_sdio_irq);
				host->saved_para.msdc_cfg = sdr_read32(MSDC_CFG);
				host->saved_para.ddly0 = sdr_read32(MSDC_DAT_RDDLY0);
				host->saved_para.pad_tune0 = sdr_read32(MSDC_PAD_TUNE0);
				host->saved_para.sdc_cfg = sdr_read32(SDC_CFG);
				host->saved_para.iocon = sdr_read32(MSDC_IOCON);
				host->saved_para.timing = host->timing;
				msdc_gate_clock(host, 0);
				if (host->error == -EBUSY) {
					ret = host->error;
					host->error = 0;
				}
			}

			if (host->hw->host_function == MSDC_SDIO)
				host->mmc->pm_flags |= MMC_PM_KEEP_POWER;

			ERR_MSG("msdc suspend cur_cfg=%x, save_cfg=%x, cur_hz=%d,save_hz=%d, pm_flags=0x%x"
				, sdr_read32(MSDC_CFG), host->saved_para.msdc_cfg,
				host->mclk, host->saved_para.hz, host->mmc->pm_flags);
		}
	}
	return ret;
}

static int msdc_drv_resume(struct platform_device *pdev)
{
	int ret = 0;
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct msdc_host *host = mmc_priv(mmc);
	struct pm_message state;

	if (host->hw->flags & MSDC_SDIO_IRQ)
		pr_debug("msdc msdc_drv_resume\n");
	state.event = PM_EVENT_RESUME;
	if (mmc && (host->hw->flags & MSDC_SYS_SUSPEND))
		msdc_pm(state, (void *)host);
	/* This mean WIFI not controller by PM */
	if (host->hw->host_function == MSDC_SDIO)
		host->mmc->pm_flags |= MMC_PM_KEEP_POWER;
	return ret;
}
#endif
#ifdef CONFIG_OF
static const struct of_device_id msdc_of_ids[] = {
	{.compatible = "mediatek,mt8173-sdio",},
	{},
};
#endif

static struct platform_driver mt_msdc_driver = {
	.probe = msdc_drv_probe,
	.remove = msdc_drv_remove,
#ifdef CONFIG_PM
	.suspend = msdc_drv_suspend,
	.resume = msdc_drv_resume,
#endif
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = msdc_of_ids,
#endif
		   },
};

/*--------------------------------------------------------------------------*/
/* module init/exit                                                         */
/*--------------------------------------------------------------------------*/
static int __init mt_msdc_init(void)
{
	int ret;

	ret = platform_driver_register(&mt_msdc_driver);
	if (ret) {
		pr_err(DRV_NAME ": Can't register driver");
		return ret;
	}
/* weiping fix emmc_proc */
#if 0
#if defined(CONFIG_MTK_EMMC_SUPPORT) && defined(CONFIG_PROC_FS)
	proc_emmc = proc_create("emmc", 0, NULL, &proc_emmc_fops);
#endif				/* CONFIG_MTK_EMMC_SUPPORT && CONFIG_PROC_FS */
#endif
	pr_debug(DRV_NAME ": MediaTek MSDC Driver\n");

	msdc_debug_proc_init();
#ifdef MSDC_DMA_ADDR_DEBUG
	msdc_init_dma_latest_address();
#endif
	return 0;
}

static void __exit mt_msdc_exit(void)
{
	platform_driver_unregister(&mt_msdc_driver);

#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_MSDC);
#endif
}

module_init(mt_msdc_init);
module_exit(mt_msdc_exit);
/* #ifdef CONFIG_MTK_EMMC_SUPPORT */
#if 0
late_initcall_sync(msdc_get_cache_region);
#endif
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek SD/MMC Card Driver");
