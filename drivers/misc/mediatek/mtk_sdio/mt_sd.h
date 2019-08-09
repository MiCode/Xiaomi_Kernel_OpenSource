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

#ifndef MT_SD_H
#define MT_SD_H

#ifdef CONFIG_FPGA_EARLY_PORTING
#define FPGA_PLATFORM
#else
/*#define MTK_MSDC_BRINGUP_DEBUG*/
#endif

#include <linux/bitops.h>

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/semaphore.h>

#include "msdc.h"

#ifdef CONFIG_MTK_COMBO_COMM_SDIO
#define CFG_DEV_MSDC2
#define CONFIG_MTK_COMBO_SDIO_SLOT
#endif

#ifdef CONFIG_MTK_COMBO_SDIO_SLOT
#include "mtk_wcn_cmb_stub.h"
#endif

#define MSDC_WQ_ERROR_TUNE

#define MSDC_AUTOK_ON_ERROR
#ifdef MSDC_AUTOK_ON_ERROR
/*#define DATA_TUNE_READ_DATA_ALLOW_FALLING_EDGE*/
#endif

#define MSDC_DMA_ADDR_DEBUG
/*#define MSDC_HQA*/

#define MTK_MSDC_USE_CMD23
#if defined(CONFIG_MTK_EMMC_CACHE) && defined(MTK_MSDC_USE_CMD23)
#define MTK_MSDC_USE_CACHE
#endif

#ifdef MTK_MSDC_USE_CMD23
#define MSDC_USE_AUTO_CMD23             (1)
#endif

#ifdef MTK_MSDC_USE_CACHE
#ifndef MMC_ENABLED_EMPTY_QUEUE_FLUSH
/* #define MTK_MSDC_FLUSH_BY_CLK_GATE */
#endif
#endif

#define HOST_MAX_NUM                    (4)
#define MAX_REQ_SZ                      (512 * 1024)

#ifdef FPGA_PLATFORM
#define HOST_MAX_MCLK                   (200000000)
#else
#define HOST_MAX_MCLK                   (200000000)
#endif
#define HOST_MIN_MCLK                   (260000)

/* ================================= */

#define MAX_GPD_NUM                     (1 + 1) /* one null gpd */
#define MAX_BD_NUM                      (1024)
#define MAX_BD_PER_GPD                  (MAX_BD_NUM)
#define CLK_SRC_MAX_NUM                 (1)

#define SDIO_ERROR_BYPASS

/*#define MTK_MSDC_DUMP_FIFO*/

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
#define CONFIG_CMDQ_CMD_DAT_PARALLEL
#endif

/*--------------------------------------------------------------------------*/
/* Common Macro                                                             */
/*--------------------------------------------------------------------------*/
#define REG_ADDR(x)                     ((u32 *)(base + OFFSET_##x))

/*--------------------------------------------------------------------------*/
/* Common Definition                                                        */
/*--------------------------------------------------------------------------*/
#define MSDC_FIFO_SZ            (128)
#define MSDC_FIFO_THD           (64)    /* (128) */
#define MSDC_NUM                (4)

/* No memory stick mode, 0 use to gate clock */
#define MSDC_MS                 (0)
#define MSDC_SDMMC              (1)

#define MSDC_MODE_UNKNOWN       (0)
#define MSDC_MODE_PIO           (1)
#define MSDC_MODE_DMA_BASIC     (2)
#define MSDC_MODE_DMA_DESC      (3)
#define MSDC_MODE_DMA_ENHANCED  (4)

#define MSDC_BUS_1BITS          (0)
#define MSDC_BUS_4BITS          (1)
#define MSDC_BUS_8BITS          (2)

#define MSDC_BRUST_8B           (3)
#define MSDC_BRUST_16B          (4)
#define MSDC_BRUST_32B          (5)
#define MSDC_BRUST_64B          (6)

#define MSDC_AUTOCMD12          (0x0001)
#define MSDC_AUTOCMD23          (0x0002)
#define MSDC_AUTOCMD19          (0x0003)
#define MSDC_AUTOCMD53          (0x0004)

#define MSDC_EMMC_BOOTMODE0     (0)     /* Pull low CMD mode */
#define MSDC_EMMC_BOOTMODE1     (1)     /* Reset CMD mode */

enum {
	RESP_NONE = 0,
	RESP_R1,
	RESP_R2,
	RESP_R3,
	RESP_R4,
	RESP_R5,
	RESP_R6,
	RESP_R7,
	RESP_R1B
};

#include "msdc_reg.h"

/* MSDC_CFG[START_BIT] value */
#define START_AT_RISING                 (0x0)
#define START_AT_FALLING                (0x1)
#define START_AT_RISING_AND_FALLING     (0x2)
#define START_AT_RISING_OR_FALLING      (0x3)

#define TYPE_CMD_RESP_EDGE              (0)
#define TYPE_WRITE_CRC_EDGE             (1)
#define TYPE_READ_DATA_EDGE             (2)
#define TYPE_WRITE_DATA_EDGE            (3)

#define CARD_READY_FOR_DATA             (1<<8)
#define CARD_CURRENT_STATE(x)           ((x&0x00001E00)>>9)

#define REQ_CMD_EIO  (0x1 << 0)
#define REQ_CMD_TMO  (0x1 << 1)
#define REQ_DAT_ERR  (0x1 << 2)
#define REQ_STOP_EIO (0x1 << 3)
#define REQ_STOP_TMO (0x1 << 4)
#define REQ_CMD23_EIO (0x1 << 5)
#define REQ_CMD23_TMO (0x1 << 6)

typedef void (*sdio_irq_handler_t)(void *);  /* external irq handler */
typedef void (*pm_callback_t)(pm_message_t state, void *data);

#ifdef CONFIG_MTK_COMBO_COMM
#include <mt-plat/mtk_wcn_cmb_stub.h>
#endif

#define MSDC_CD_PIN_EN      (1 << 0)  /* card detection pin is wired   */
#define MSDC_WP_PIN_EN      (1 << 1)  /* write protection pin is wired */
#define MSDC_RST_PIN_EN     (1 << 2)  /* emmc reset pin is wired       */
#define MSDC_SDIO_IRQ       (1 << 3)  /* use internal sdio irq (bus)   */
#define MSDC_EXT_SDIO_IRQ   (1 << 4)  /* use external sdio irq         */
#define MSDC_REMOVABLE      (1 << 5)  /* removable slot                */
#define MSDC_SYS_SUSPEND    (1 << 6)  /* suspended by system           */
#define MSDC_SD_NEED_POWER  (1 << 31) /* for some board, need SD power always on!! or cannot recognize the sd card*/

#define MSDC_CMD_PIN        (0)
#define MSDC_DAT_PIN        (1)
#define MSDC_CD_PIN         (2)
#define MSDC_WP_PIN         (3)
#define MSDC_RST_PIN        (4)

#define MSDC_DATA1_INT      (1)
#define MSDC_BOOT_EN        (1)

struct msdc_hw {
	unsigned char clk_src;  /* host clock source */
	unsigned char cmd_edge; /* command latch edge */
	unsigned char rdata_edge;       /* read data latch edge */
	unsigned char wdata_edge;       /* write data latch edge */
	unsigned char clk_drv;  /* clock pad driving */
	unsigned char cmd_drv;  /* command pad driving */
	unsigned char dat_drv;  /* data pad driving */
	unsigned char rst_drv;  /* RST-N pad driving */
	unsigned char ds_drv;   /* eMMC5.0 DS pad driving */
	unsigned char clk_drv_sd_18;    /* clock pad driving for SD card at 1.8v sdr104 mode */
	unsigned char cmd_drv_sd_18;    /* command pad driving for SD card at 1.8v sdr104 mode */
	unsigned char dat_drv_sd_18;    /* data pad driving for SD card at 1.8v sdr104 mode */
	unsigned char clk_drv_sd_18_sdr50;      /* clock pad driving for SD card at 1.8v sdr50 mode */
	unsigned char cmd_drv_sd_18_sdr50;      /* command pad driving for SD card at 1.8v sdr50 mode */
	unsigned char dat_drv_sd_18_sdr50;      /* data pad driving for SD card at 1.8v sdr50 mode */
	unsigned char clk_drv_sd_18_ddr50;      /* clock pad driving for SD card at 1.8v ddr50 mode */
	unsigned char cmd_drv_sd_18_ddr50;      /* command pad driving for SD card at 1.8v ddr50 mode */
	unsigned char dat_drv_sd_18_ddr50;      /* data pad driving for SD card at 1.8v ddr50 mode */
	unsigned long flags;    /* hardware capability flags */

	unsigned char datrddly[8];      /*read; range: 0~31*/
	unsigned char datwrddly;        /*write; range: 0~31*/
	unsigned char cmdrrddly;        /*cmd; range: 0~31*/
	unsigned char cmdrddly;         /*cmd; range: 0~31*/

	unsigned char cmdrtactr_sdr50;  /* command response turn around counter, sdr 50 mode*/
	unsigned char wdatcrctactr_sdr50;       /* write data crc turn around counter, sdr 50 mode*/
	unsigned char intdatlatcksel_sdr50;     /* internal data latch CK select, sdr 50 mode*/
	unsigned char cmdrtactr_sdr200; /* command response turn around counter, sdr 200 mode*/
	unsigned char wdatcrctactr_sdr200;      /* write data crc turn around counter, sdr 200 mode*/
	unsigned char intdatlatcksel_sdr200;    /* internal data latch CK select, sdr 200 mode*/

	unsigned int  boot;              /* define boot host */
	unsigned char host_function;    /* define host function */
	unsigned char cd_level;         /* card detection level */

	/* external power control for card */
	void (*ext_power_on)(void);
	void (*ext_power_off)(void);

	/* external sdio irq operations */
	void (*request_sdio_eirq)(sdio_irq_handler_t sdio_irq_handler, void *data);
	void (*enable_sdio_eirq)(void);
	void (*disable_sdio_eirq)(void);

	/* power management callback for external module */
	void (*register_pm)(pm_callback_t pm_cb, void *data);
};


/*--------------------------------------------------------------------------*/
/* Descriptor Structure                                                     */
/*--------------------------------------------------------------------------*/
struct gpd_t {
	u32  hwo:1; /* could be changed by hw */
	u32  bdp:1;
	u32  rsv0:6;
	u32  chksum:8;
	u32  intr:1;
	u32  rsv1:7;
	u32  nexth4:4;
	u32  ptrh4:4;
	u32  next;
	u32  ptr;
	u32  buflen:24;
	u32  extlen:8;
	u32  arg;
	u32  blknum;
	u32  cmd;
};

struct bd_t {
	u32  eol:1;
	u32  rsv0:7;
	u32  chksum:8;
	u32  rsv1:1;
	u32  blkpad:1;
	u32  dwpad:1;
	u32  rsv2:5;
	u32  nexth4:4;
	u32  ptrh4:4;
	u32  next;
	u32  ptr;
	u32  buflen:24;
	u32  rsv3:8;
};

struct scatterlist_ex {
	u32 cmd;
	u32 arg;
	u32 sglen;
	struct scatterlist *sg;
};

#define DMA_FLAG_NONE       (0x00000000)
#define DMA_FLAG_EN_CHKSUM  (0x00000001)
#define DMA_FLAG_PAD_BLOCK  (0x00000002)
#define DMA_FLAG_PAD_DWORD  (0x00000004)

struct msdc_dma {
	u32 flags;                   /* flags */
	u32 xfersz;                  /* xfer size in bytes */
	u32 sglen;                   /* size of scatter list */
	u32 blklen;                  /* block size */
	struct scatterlist *sg;      /* I/O scatter list */
	struct scatterlist_ex *esg;  /* extended I/O scatter list */
	u8  mode;                    /* dma mode        */
	u8  burstsz;                 /* burst size      */
	u8  intr;                    /* dma done interrupt */
	u8  padding;                 /* padding */
	u32 cmd;                     /* enhanced mode command */
	u32 arg;                     /* enhanced mode arg */
	u32 rsp;                     /* enhanced mode command response */
	u32 autorsp;                 /* auto command response */

	struct gpd_t *gpd;           /* pointer to gpd array */
	struct bd_t  *bd;            /* pointer to bd array */
	dma_addr_t gpd_addr;         /* the physical address of gpd array */
	dma_addr_t bd_addr;          /* the physical address of bd array */
	u32 used_gpd;                /* the number of used gpd elements */
	u32 used_bd;                 /* the number of used bd elements */
};

struct tune_counter {
	u32 time_cmd;
	u32 time_read;
	u32 time_write;
	u32 time_hs400;
};

/*FIX ME, consider to move it into msdc_tune.c*/
struct msdc_saved_para {
	u32 pad_tune0;
	u32 pad_tune1;
	u32 ddly0;
	u32 ddly1;
	u8 suspend_flag;
	u32 msdc_cfg;
	u32 mode;
	u32 div;
	u32 sdc_cfg;
	u32 iocon;
	u8 timing;
	u32 hz;
	u8 int_dat_latch_ck_sel;
	u8 ckgen_msdc_dly_sel;
	u8 inten_sdio_irq;
/* for write: 3T need wait before host check busy after crc status */
	u8 write_busy_margin;
/* for write: host check timeout change to 16T */
	u8 write_crc_margin;
	u8 ds_dly1;
	u8 ds_dly3;
	u32 emmc50_pad_cmd_tune;
	u32 emmc50_dat01;
	u32 emmc50_dat23;
	u32 emmc50_dat45;
	u32 emmc50_dat67;
	u32 pb1;
	u32 pb2;
};

struct msdc_host {
	struct msdc_hw          *hw;

	struct mmc_host         *mmc;           /* mmc structure */
	struct mmc_command      *cmd;
	struct mmc_data         *data;
	struct mmc_request      *mrq;
	int                     cmd_rsp;
	int                     cmd_rsp_done;
	int                     cmd_r1b_done;

	int                     error;
	spinlock_t              lock;           /* mutex */
	spinlock_t              clk_gate_lock;
	spinlock_t              remove_bad_card; /* to solve removing bad card
						  * race condition
						  * with hot-plug enable
						  */
	spinlock_t              sdio_irq_lock;  /* avoid race condition
						 * at DAT1 interrupt case
						 */
	int                     clk_gate_count;
	bool                    clk_on;
	struct semaphore        sem;

	u32                     blksz;          /* host block size */
	void __iomem            *base;          /* host base address */
	int                     id;             /* host id */

	u32                     xfer_size;      /* total transferred size */

	struct msdc_dma         dma;            /* dma channel */
	u64                     dma_mask;
	u32                     dma_addr;       /* dma transfer address */
	u32                     dma_left_size;  /* dma transfer left size */
	u32                     dma_xfer_size;  /* dma transfer size in bytes */
	int                     dma_xfer;       /* dma transfer mode */

	u32                     write_timeout_ms; /* write busy timeout ms */
	u32                     timeout_ns;     /* data timeout ns */
	u32                     timeout_clks;   /* data timeout clks */

	atomic_t                abort;          /* abort transfer */

	int                     irq;            /* host interrupt */

#ifdef MTK_MSDC_FLUSH_BY_CLK_GATE
	struct tasklet_struct   flush_cache_tasklet;
#endif

	struct delayed_work     set_vcore_workq;
	struct completion       autok_done;
	bool                    is_autok_done;

	atomic_t                sdio_stopping;

	struct completion       cmd_done;
	struct completion       xfer_done;
	struct pm_message       pm_state;

	u32                     mclk;           /* mmc subsystem clock */
	u32                     hclk;           /* host clock speed */
	u32                     sclk;           /* SD/MS clock speed */
	u8                      core_clkon;     /* host clock(cg) status */
	u8                      timing;         /* timing specification used */
	u8                      power_mode;     /* host power mode */
	u8                      bus_width;
	u8                      card_inserted;  /* card inserted ? */
	u8                      suspend;        /* host suspended ? */
	u8                      app_cmd;        /* for app command */
	u32                     app_cmd_arg;
	u64                     starttime;
	struct timer_list       timer;
	struct tune_counter     t_counter;
	u32                     rwcmd_time_tune;
	int                     read_time_tune;
	int                     write_time_tune;
	u32                     write_timeout_uhs104;
	u32                     read_timeout_uhs104;
	u32                     write_timeout_emmc;
	u32                     read_timeout_emmc;
	u8                      autocmd;
	u32                     sw_timeout;
	u32                     power_cycle;    /* power cycle done
						 * in tuning flow
						 */
	bool                    power_cycle_enable; /* enable power cycle */
	bool                    error_tune_enable;  /* enable error tune flow */
	u32                     sd_30_busy;
	bool                    tune;
	bool			card_selected;
	bool                    first_tune_done;
	bool                    async_tuning_in_progress;
	bool                    async_tuning_done;
	bool                    legacy_tuning_in_progress;
	bool                    legacy_tuning_done;
	int                     autok_error;
	u32                     tune_latch_ck_cnt;
#ifndef MSDC_WQ_ERROR_TUNE
	unsigned int            err_mrq_dir;
#endif
	struct msdc_saved_para  saved_para;
	bool                    block_bad_card;
	struct delayed_work     write_timeout;  /* check if write busy timeout*/
#ifdef SDIO_ERROR_BYPASS
	int                     sdio_error;     /* sdio error can't recovery */
#endif
	void    (*power_control)(struct msdc_host *host, u32 on);
	void    (*power_switch)(struct msdc_host *host, u32 on);
	u32                     vmc_cal_default;

	struct clk *clock_control;
	struct clk *clock_hclk;
	struct clk *clock_source_cg;
	struct delayed_work	work_init; /* for init mmc card */
	struct platform_device  *pdev;

#ifdef MSDC_WQ_ERROR_TUNE
	struct work_struct	work_tune; /* new thread tune */
	struct mmc_request	*mrq_tune; /* backup host->mrq */
#endif
};

enum {
	cmd_counter = 0,
	read_counter,
	write_counter,
	all_counter,
};

enum {
	TRAN_MOD_PIO,
	TRAN_MOD_DMA,
	TRAN_MOD_NUM
};

enum {
	OPER_TYPE_READ,
	OPER_TYPE_WRITE,
	OPER_TYPE_NUM
};

struct dma_addr {
	u32 start_address;
	u32 size;
	u8 end;
	struct dma_addr *next;
};

struct msdc_reg_control {
	ulong addr;
	u32 mask;
	u32 value;
	u32 default_value;
	/*int (*restore_func)(int restore);*/
};

static inline unsigned int uffs(unsigned int x)
{
	unsigned int r = 1;

	if (!x)
		return 0;
	if (!(x & 0xffff)) {
		x >>= 16;
		r += 16;
	}
	if (!(x & 0xff)) {
		x >>= 8;
		r += 8;
	}
	if (!(x & 0xf)) {
		x >>= 4;
		r += 4;
	}
	if (!(x & 3)) {
		x >>= 2;
		r += 2;
	}
	if (!(x & 1)) {
		x >>= 1;
		r += 1;
	}
	return r;
}

#if defined(__KERNEL__)

#include <linux/io.h>
#include <asm/cacheflush.h>
/*
 * Define macros.
 */
#define mt_reg_sync_writel(v, a) \
	do {    \
		__raw_writel((v), (void __force __iomem *)((a)));   \
		mb();		/* mb() */	 \
	} while (0)

#define mt_reg_sync_writew(v, a) \
	do {    \
		__raw_writew((v), (void __force __iomem *)((a)));   \
		mb();		/* mb() */	 \
	} while (0)

#define mt_reg_sync_writeb(v, a) \
	do {    \
		__raw_writeb((v), (void __force __iomem *)((a)));   \
		mb();		/* mb() */	 \
	} while (0)

#else				/* __KERNEL__ */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define mt_reg_sync_writel(v, a)        mt65xx_reg_sync_writel(v, a)
#define mt_reg_sync_writew(v, a)        mt65xx_reg_sync_writew(v, a)
#define mt_reg_sync_writeb(v, a)        mt65xx_reg_sync_writeb(v, a)

#define mb()   /* mb() */	\
	{    \
		__asm__ __volatile__ ("dsb" : : : "memory"); \
	}

#define mt65xx_reg_sync_writel(v, a) \
	do {    \
		*(unsigned int *)(a) = (v);    \
		mb();		/* mb() */	 \
	} while (0)

#define mt65xx_reg_sync_writew(v, a) \
	do {    \
		*(unsigned short *)(a) = (v);    \
		mb();		/* mb() */	 \
	} while (0)

#define mt65xx_reg_sync_writeb(v, a) \
	do {    \
		*(unsigned char *)(a) = (v);    \
		mb();		/* mb() */	 \
	} while (0)

#endif				/* __KERNEL__ */

#define MSDC_READ8(reg)           __raw_readb((const void *)reg)
#define MSDC_READ16(reg)          __raw_readw((const void *)reg)
#define MSDC_READ32(reg)          __raw_readl((const void *)reg)
#define MSDC_WRITE8(reg, val)     mt_reg_sync_writeb(val, reg)
#define MSDC_WRITE16(reg, val)    mt_reg_sync_writew(val, reg)
#define MSDC_WRITE32(reg, val)    mt_reg_sync_writel(val, reg)

#define UNSTUFF_BITS(resp, start, size) \
({ \
	const int __size = size; \
	const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1; \
	const int __off = 3 - ((start) / 32); \
	const int __shft = (start) & 31; \
	u32 __res; \
	__res = resp[__off] >> __shft; \
	if (__size + __shft > 32) \
		__res |= resp[__off-1] << ((32 - __shft) % 32); \
	__res & __mask; \
})

#define MSDC_SET_BIT32(reg, bs) \
	do { \
		unsigned int tv = MSDC_READ32(reg);\
		tv |= (u32)(bs); \
		MSDC_WRITE32(reg, tv); \
	} while (0)

#define MSDC_CLR_BIT32(reg, bs) \
	do { \
		unsigned int tv = MSDC_READ32(reg);\
		tv &= ~((u32)(bs)); \
		MSDC_WRITE32(reg, tv); \
	} while (0)

#define MSDC_SET_FIELD(reg, field, val) \
	do { \
		unsigned int tv = MSDC_READ32(reg); \
		tv &= ~(field); \
		tv |= ((val) << (uffs((unsigned int)field) - 1)); \
		MSDC_WRITE32(reg, tv); \
	} while (0)

#define MSDC_GET_FIELD(reg, field, val) \
	do { \
		unsigned int tv = MSDC_READ32(reg); \
		val = ((tv & (field)) >> (uffs((unsigned int)field) - 1)); \
	} while (0)

#define sdc_is_busy()           (MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY)
#define sdc_is_cmd_busy()       (MSDC_READ32(SDC_STS) & SDC_STS_CMDBUSY)

#ifdef CONFIG_CMDQ_CMD_DAT_PARALLEL
#define sdc_send_cmdq_cmd(opcode, arg) \
	do { \
		MSDC_SET_FIELD(EMMC51_CFG0, MSDC_EMMC51_CFG_CMDQEN, (1)); \
		MSDC_SET_FIELD(EMMC51_CFG0, MSDC_EMMC51_CFG_NUM, (opcode)); \
		MSDC_SET_FIELD(EMMC51_CFG0, MSDC_EMMC51_CFG_RSPTYPE, (1)); \
		MSDC_SET_FIELD(EMMC51_CFG0, MSDC_EMMC51_CFG_DTYPE, (0)); \
		MSDC_WRITE32(SDC_ARG, (arg)); \
		MSDC_WRITE32(SDC_CMD, (0x0)); \
	} while (0)
#endif

#define sdc_send_cmd(cmd, arg) \
	do { \
		MSDC_WRITE32(SDC_ARG, (arg)); \
		MSDC_WRITE32(SDC_CMD, (cmd)); \
	} while (0)

#define msdc_retry(expr, retry, cnt, id) \
	do { \
		int backup = cnt; \
		while (retry) { \
			if (!(expr)) \
				break; \
			if (cnt-- == 0) { \
				retry--; mdelay(1); cnt = backup; \
			} \
		} \
		if (retry == 0) { \
			msdc_dump_info(id); \
		} \
		WARN_ON(retry == 0); \
	} while (0)

#define msdc_reset(id) \
	do { \
		int retry = 3, cnt = 1000; \
		MSDC_SET_BIT32(MSDC_CFG, MSDC_CFG_RST); \
		msdc_retry(MSDC_READ32(MSDC_CFG) & MSDC_CFG_RST, retry, \
			cnt, id); \
	} while (0)

#define msdc_clr_int() \
	do { \
		u32 val = MSDC_READ32(MSDC_INT); \
		MSDC_WRITE32(MSDC_INT, val); \
	} while (0)

#define msdc_reset_hw(id) \
	do { \
		msdc_reset(id); \
		msdc_clr_fifo(id); \
		msdc_clr_int(); \
	} while (0)

#define msdc_txfifocnt()        ((MSDC_READ32(MSDC_FIFOCS) \
				 & MSDC_FIFOCS_TXCNT) >> 16)
#define msdc_rxfifocnt()        ((MSDC_READ32(MSDC_FIFOCS) \
				 & MSDC_FIFOCS_RXCNT) >> 0)
#define msdc_fifo_write32(v)    MSDC_WRITE32(MSDC_TXDATA, (v))
#define msdc_fifo_write8(v)     MSDC_WRITE8(MSDC_TXDATA, (v))
#define msdc_fifo_read32()      MSDC_READ32(MSDC_RXDATA)
#define msdc_fifo_read8()       MSDC_READ8(MSDC_RXDATA)

/* can modify to read h/w register. */
/* #define is_card_present(h) \
 *	((MSDC_READ32(MSDC_PS) & MSDC_PS_CDSTS) ? 0 : 1);
 */
#define is_card_present(h)      (((struct msdc_host *)(h))->card_inserted)
#define is_card_sdio(h)         (((struct msdc_host *)(h))->hw->register_pm)

#define CMD_TIMEOUT             (HZ/10 * 5)     /* 100ms x5 */
#define DAT_TIMEOUT             (HZ    * 5)     /* 1000ms x5 */
#define CLK_TIMEOUT             (HZ    * 5)     /* 5s    */
#define POLLING_BUSY            (HZ    * 3)

#ifdef CONFIG_OF
#if defined(CFG_DEV_MSDC2)
extern struct msdc_hw msdc2_hw;
#endif
#if defined(CFG_DEV_MSDC3)
extern struct msdc_hw msdc3_hw;
#endif
#endif

extern struct msdc_host *mtk_msdc_host[];
extern unsigned int msdc_latest_transfer_mode[HOST_MAX_NUM];
#ifdef MSDC_DMA_ADDR_DEBUG
extern struct dma_addr msdc_latest_dma_address[MAX_BD_PER_GPD];
#endif
extern int g_dma_debug[HOST_MAX_NUM];

extern u32 g_emmc_mode_switch;

enum {
	SD_TOOL_ZONE = 0,
	SD_TOOL_DMA_SIZE  = 1,
	SD_TOOL_PM_ENABLE = 2,
	SD_TOOL_SDIO_PROFILE = 3,
	SD_TOOL_CLK_SRC_SELECT = 4,
	SD_TOOL_REG_ACCESS = 5,
	SD_TOOL_SET_DRIVING = 6,
	SD_TOOL_DESENSE = 7,
	RW_BIT_BY_BIT_COMPARE = 8,
	SMP_TEST_ON_ONE_HOST = 9,
	SMP_TEST_ON_ALL_HOST = 10,
	SD_TOOL_MSDC_HOST_MODE = 11,
	SD_TOOL_DMA_STATUS = 12,
	SD_TOOL_ENABLE_SLEW_RATE = 13,
	SD_TOOL_ENABLE_SMT = 14,
	MMC_PERF_DEBUG = 15,
	MMC_PERF_DEBUG_PRINT = 16,
	SD_TOOL_SET_RDTDSEL = 17,
	MMC_REGISTER_READ = 18,
	MMC_REGISTER_WRITE = 19,
	MSDC_READ_WRITE = 20,
	MMC_ERROR_TUNE = 21,
	MMC_EDC_EMMC_CACHE = 22,
	MMC_DUMP_GPD = 23,
	MMC_ETT_TUNE = 24,
	MMC_CRC_STRESS = 25,
	ENABLE_AXI_MODULE = 26,
	SDIO_AUTOK_RESULT = 27,
	MMC_CMDQ_STATUS = 28,
	DO_AUTOK_OFFLINE_TUNE_TX = 29
};

enum {
	MODE_PIO = 0,
	MODE_DMA = 1,
	MODE_SIZE_DEP = 2,
};

/* Variable declared in dbg.c */
extern u32 msdc_host_mode[];
extern u32 msdc_host_mode2[];

extern unsigned int sd_debug_zone[];
extern u32 drv_mode[];
extern u32 dma_size[];
extern unsigned char msdc_clock_src[];

extern u32 sdio_pro_enable;

extern bool emmc_sleep_failed;

extern int msdc_rsp[];


/**********************************************************/
/* Functions                                               */
/**********************************************************/
#include "msdc_io.h"

/* Function provided by sd.c */
void msdc_prepare_clk(struct msdc_host *host);
void msdc_unprepare_clk(struct msdc_host *host);
int msdc_clk_stable(struct msdc_host *host, u32 mode, u32 div,
	u32 hs400_src);
void msdc_clr_fifo(unsigned int id);
unsigned int msdc_do_command(struct msdc_host *host,
	struct mmc_command *cmd,
	int                 tune,
	unsigned long       timeout);
void msdc_dump_info(u32 id);
void msdc_dump_register(struct msdc_host *host);
void msdc_dump_register_core(u32 id, void __iomem *base);
void msdc_dump_dbg_register_core(u32 id, void __iomem *base);
void msdc_get_cache_region(struct work_struct *work);
int msdc_cache_ctrl(struct msdc_host *host, unsigned int enable,
	u32 *status);
int msdc_get_card_status(struct mmc_host *mmc, struct msdc_host *host,
	u32 *status);
int msdc_get_dma_status(int host_id);
struct msdc_host *msdc_get_host(int host_function, bool boot,
	bool secondary);
int msdc_reinit(struct msdc_host *host);
void msdc_select_clksrc(struct msdc_host *host, int clksrc);
void msdc_send_stop(struct msdc_host *host);
void msdc_set_mclk(struct msdc_host *host, unsigned char timing, u32 hz);
void msdc_set_smpl(struct msdc_host *host, u32 clkmode, u8 mode, u8 type,
	u8 *edge);
void msdc_set_smpl_all(struct msdc_host *host, u32 clock_mode);
int msdc_switch_part(struct msdc_host *host, char part_id);
int msdc_execute_tuning(struct mmc_host *mmc, u32 opcode);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
unsigned int msdc_do_cmdq_command(struct msdc_host *host,
	struct mmc_command *cmd,
	int tune,
	unsigned long timeout);
#endif

/* Function provided by msdc_partition.c */
#ifdef CONFIG_PWR_LOSS_MTK_TEST
void msdc_proc_emmc_create(void);
#endif
int msdc_can_apply_cache(unsigned long long start_addr,
	unsigned int size);
struct gendisk *mmc_get_disk(struct mmc_card *card);
u64 msdc_get_capacity(int get_emmc_total);
u64 msdc_get_user_capacity(struct msdc_host *host);
u32 msdc_get_other_capacity(struct msdc_host *host, char *name);

/* Function provided by mmc/core/bus.c */
void mmc_remove_card(struct mmc_card *card);


#define check_mmc_cache_ctrl(card) \
	(card && mmc_card_mmc(card) && (card->ext_csd.cache_ctrl & 0x1))
#define check_mmc_cache_flush_cmd(cmd) \
	((cmd->opcode == MMC_SWITCH) && \
	 (((cmd->arg >> 16) & 0xFF) == EXT_CSD_FLUSH_CACHE) && \
	 (((cmd->arg >> 8) & 0x1)))
#define check_mmc_cmd2425(opcode) \
	((opcode == MMC_WRITE_MULTIPLE_BLOCK) || \
	 (opcode == MMC_WRITE_BLOCK))
#define check_mmc_cmd1718(opcode) \
	((opcode == MMC_READ_MULTIPLE_BLOCK) || \
	 (opcode == MMC_READ_SINGLE_BLOCK))
#define check_mmc_cmd1825(opcode) \
	((opcode == MMC_READ_MULTIPLE_BLOCK) || \
	 (opcode == MMC_WRITE_MULTIPLE_BLOCK))
#define check_mmc_cmd01213(opcode) \
	((opcode == MMC_GO_IDLE_STATE) || \
	 (opcode == MMC_STOP_TRANSMISSION) || \
	 (opcode == MMC_SEND_STATUS))
#define check_mmc_cmd4445(opcode) \
	((opcode == MMC_SET_QUEUE_CONTEXT) || \
	 (opcode == MMC_QUEUE_READ_ADDRESS))
#define check_mmc_cmd4647(opcode) \
	((opcode == MMC_READ_REQUESTED_QUEUE) || \
	 (opcode == MMC_WRITE_REQUESTED_QUEUE))
#define check_mmc_cmd48(opcode) \
	(opcode == MMC_CMDQ_TASK_MGMT)
#define check_mmc_cmd44(x) \
	((x) && \
	 ((x)->opcode == MMC_SET_QUEUE_CONTEXT))
#define check_mmc_cmd13_sqs(x) \
	(((x)->opcode == MMC_SEND_STATUS) && \
	 ((x)->arg & (1 << 15)))

#endif /* end of  MT_SD_H */

