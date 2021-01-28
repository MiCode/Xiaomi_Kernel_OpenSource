/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MT_SD_H
#define MT_SD_H

#ifdef CONFIG_FPGA_EARLY_PORTING
#ifndef FPGA_PLATFORM
#define FPGA_PLATFORM
#endif
#else
/* #define MTK_MSDC_BRINGUP_DEBUG */
#endif

#include <mt-plat/sync_write.h>
#include <linux/bitops.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/pm_qos.h>

#include "msdc_cust.h"

#include "autok.h"
#include "autok_dvfs.h"
#include "sw-cqhci-crypto.h"

#if defined(CONFIG_MTK_HW_FDE) && defined(CONFIG_MTK_HW_FDE_AES)
#include <fde_aes.h>
#include <fde_aes_dbg.h>
#endif

#if defined(CONFIG_MTK_EMMC_CQ_SUPPORT) && defined(CONFIG_MTK_EMMC_HW_CQ)
#error "MTK_EMMC_CQ_SUPPORT & MTK_EMMC_HW_CQ cannot define at the same time."
#endif


#ifdef CONFIG_PWR_LOSS_MTK_TEST
#include <mach/power_loss_test.h>
#else
#define MVG_EMMC_CHECK_BUSY_AND_RESET(...)
#define MVG_EMMC_SETUP(...)
#define MVG_EMMC_RESET(...)
#define MVG_EMMC_WRITE_MATCH(...)
#define MVG_EMMC_ERASE_MATCH(...)
#define MVG_EMMC_ERASE_RESET(...)
#define MVG_EMMC_DECLARE_INT32(...)
#endif

/* #define MSDC_SWITCH_MODE_WHEN_ERROR */
#define TUNE_NONE                (0)        /* No need tune */
#define TUNE_ASYNC_CMD           (0x1 << 0) /* async transfer cmd crc */
#define TUNE_ASYNC_DATA_WRITE    (0x1 << 1) /* async transfer data crc */
#define TUNE_ASYNC_DATA_READ     (0x1 << 2) /* async transfer data crc */
#define TUNE_LEGACY_CMD          (0x1 << 3) /* legacy transfer cmd crc */
#define TUNE_LEGACY_DATA_WRITE   (0x1 << 4) /* legacy transfer data crc */
#define TUNE_LEGACY_DATA_READ    (0x1 << 5) /* legacy transfer data crc */
#define TUNE_LEGACY_CMD_TMO      (0x1 << 6) /* legacy transfer cmd tmo */
#define TUNE_AUTOK_PASS          (0x1 << 7) /* autok pass flag */

#ifdef CONFIG_MTK_MMC_DEBUG
#define MSDC_DMA_ADDR_DEBUG
/* #define MTK_MSDC_LOW_IO_DEBUG */
#ifdef CONFIG_MTK_EMMC_HW_CQ
#undef MTK_MSDC_LOW_IO_DEBUG
#endif
#endif
/* #define MTK_MMC_SDIO_DEBUG */

#define MTK_MSDC_USE_CMD23
#if !defined(CONFIG_PWR_LOSS_MTK_TEST) && defined(MTK_MSDC_USE_CMD23) \
	|| defined(CONFIG_MTK_EMMC_HW_CQ)
//#define MTK_MSDC_USE_CACHE
#endif

#ifdef MTK_MSDC_USE_CMD23
#define MSDC_USE_AUTO_CMD23             (1)
#endif

/* ================================= */

#define MAX_GPD_NUM                     (1 + 1) /* one null gpd */
#define MAX_BD_NUM                      (128)
#define MAX_BD_PER_GPD                  (MAX_BD_NUM)
#define CLK_SRC_MAX_NUM                 (1)

#define SDIO_ERROR_BYPASS

/* #define SDIO_EARLY_SETTING_RESTORE */

/* #define MMC_K44_RETUNE */

/* #define MTK_MSDC_DUMP_FIFO */


/*--------------------------------------------------------------------------*/
/* Common Macro                                                             */
/*--------------------------------------------------------------------------*/
#define REG_ADDR(x)             (base + OFFSET_##x)
#define REG_ADDR_TOP(x)         (base_top + OFFSET_##x)

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

#define CARD_READY_FOR_DATA             (1<<8)
#define CARD_CURRENT_STATE(x)           ((x&0x00001E00)>>9)

#define REQ_CMD_EIO  (0x1 << 0)
#define REQ_CMD_TMO  (0x1 << 1)
#define REQ_DAT_ERR  (0x1 << 2)
#define REQ_STOP_EIO (0x1 << 3)
#define REQ_STOP_TMO (0x1 << 4)
#define REQ_CMD23_EIO (0x1 << 5)
#define REQ_CMD23_TMO (0x1 << 6)
#define REQ_CRC_STATUS_ERR (0x1 << 7)

typedef void (*sdio_irq_handler_t)(void *);  /* external irq handler */
typedef void (*pm_callback_t)(pm_message_t state, void *data);

#define MSDC_CD_PIN_EN      (1 << 0)  /* card detection pin is wired   */
#define MSDC_WP_PIN_EN      (1 << 1)  /* write protection pin is wired */
#define MSDC_RST_PIN_EN     (1 << 2)  /* emmc reset pin is wired       */
#define MSDC_SDIO_IRQ       (1 << 3)  /* use internal sdio irq (bus)   */
#define MSDC_EXT_SDIO_IRQ   (1 << 4)  /* use external sdio irq         */
#define MSDC_REMOVABLE      (1 << 5)  /* removable slot                */
#define MSDC_SDIO_DDR208    (1 << 7)  /* ddr208 mode used by 6632      */
#define MSDC_VMCH_FASTOFF   (1 << 8)  /* vmch fastoff when plug ot card      */
/* for some board, need SD power always on!! or cannot recognize the sd card*/
#define MSDC_SD_NEED_POWER  (1 << 31)

#define MSDC_BOOT_EN        (1)

struct msdc_hw_driving {
	unsigned char cmd_drv;  /* command pad driving */
	unsigned char dat_drv;  /* data pad driving */
	unsigned char clk_drv;  /* clock pad driving */
	unsigned char rst_drv;  /* RST-N pad driving */
	unsigned char ds_drv;   /* eMMC5.0 DS pad driving */
};

struct msdc_hw {
	unsigned char clk_src;  /* host clock source */
	unsigned char cmd_edge; /* command latch edge */
	unsigned char rdata_edge;       /* read data latch edge */
	unsigned char wdata_edge;       /* write data latch edge */
	struct msdc_hw_driving *driving_applied;
	struct msdc_hw_driving driving;
	struct msdc_hw_driving driving_sdr104;
	struct msdc_hw_driving driving_sdr50;
	struct msdc_hw_driving driving_ddr50;
	struct msdc_hw_driving driving_hs400;
	struct msdc_hw_driving driving_hs200;

	unsigned long flags;            /* hardware capability flags */

	unsigned char boot;             /* define boot host */
	unsigned char host_function;    /* define host function */
	unsigned char cd_level;         /* card detection level */

	/* external sdio irq operations */
	void (*request_sdio_eirq)(sdio_irq_handler_t sdio_irq_handler,
		void *data);
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
	u8  mode;                    /* dma mode        */
	u8  burstsz;                 /* burst size      */
	u8  intr;                    /* dma done interrupt */
	struct gpd_t *gpd;           /* pointer to gpd array */
	struct bd_t  *bd;            /* pointer to bd array */
	dma_addr_t gpd_addr;         /* the physical address of gpd array */
	dma_addr_t bd_addr;          /* the physical address of bd array */
	u32 used_gpd;                /* the number of used gpd elements */
	u32 used_bd;                 /* the number of used bd elements */
};

/* FIX ME, consider to move it into msdc_tune.c */
struct msdc_saved_para {
	u32 pad_tune0;
	u32 pad_tune1;
	u8 suspend_flag;
	u32 msdc_cfg;
	u32 sdc_cfg;
	u32 iocon;
	u8 timing;
	u32 hz;
	u8 inten_sdio_irq;
	u8 ds_dly1;
	u8 ds_dly3;
	u32 emmc50_pad_cmd_tune;
	u32 emmc50_dat01;
	u32 emmc50_dat23;
	u32 emmc50_dat45;
	u32 emmc50_dat67;
	u32 emmc50_cfg0;
	u32 pb0;
	u32 pb1;
	u32 pb2;
	u32 sdc_fifo_cfg;
	u32 sdc_adv_cfg0;

	/* msdc top reg  */
	u32 emmc_top_control;
	u32 emmc_top_cmd;
	u32 top_emmc50_pad_ctl0;
	u32 top_emmc50_pad_ds_tune;
	u32 top_emmc50_pad_dat_tune[8];
};

struct msdc_host {
	struct msdc_hw          *hw;

	struct mmc_host         *mmc;           /* mmc structure */
	struct mmc_command      *cmd;
	struct mmc_data         *data;
	struct mmc_request      *mrq;
	ulong                   *pio_kaddr;
	int                     err_cmd;
	int                     cmd_rsp;

	int                     error;
	spinlock_t              lock;           /* mutex */
	spinlock_t              reg_lock;
	/* to solve removing bad card
	 * race condition with hot-plug enable
	 */
	spinlock_t              remove_bad_card;
#ifdef CONFIG_MTK_EMMC_HW_CQ
	spinlock_t              cmd_dump_lock;
#endif
	 /* avoid race condition at DAT1 interrupt case*/
	spinlock_t              sdio_irq_lock;
	int                     clk_gate_count;
	struct semaphore        sem;

	u32                     blksz;          /* host block size */
	void __iomem            *base;          /* host base address */
	void __iomem            *base_top;      /* base address for msdc_top*/
	int                     id;             /* host id */

	u32                     xfer_size;      /* total transferred size */

	struct msdc_dma         dma;            /* dma channel */
	u64                     dma_mask;
	int                     dma_xfer;       /* dma transfer mode */

	u32                     timeout_ns;     /* data timeout ns */
	u32                     timeout_clks;   /* data timeout clks */

	atomic_t                abort;          /* abort transfer */

	int                     irq;            /* host interrupt */

	struct delayed_work     set_vcore_workq;
	struct completion       autok_done;

	struct completion       xfer_done;

	u32                     mclk;           /* mmc subsystem clock */
	u32                     hclk;           /* host clock speed */
	u32                     sclk;           /* SD/MS clock speed */
	u8                      timing;         /* timing specification used */
	u8                      power_mode;     /* host power mode */
	u8                      bus_width;
	u8                      card_inserted;  /* card inserted ? */
	u8                      autocmd;
	u8                      app_cmd;        /* for app command */
	u32                     app_cmd_arg;

	u32                     intsts;         /* raw insts */
	bool                    use_cmd_intr;   /* cmd intr mode */
	struct completion       cmd_done;
	u32                     busy_timeout_ms;/* check device busy */
	u32                     max_busy_timeout_ms;

	int                     pin_state;      /* for hw trapping */
	u32                     sw_timeout;
#ifdef SDCARD_ESD_RECOVERY
	/* cmd13 contunous timeout, clear when any other cmd succeed */
	u32                     cmd13_timeout_cont;
#endif
	u32                     data_timeout_cont; /* data continuous timeout */
	bool                    tuning_in_progress;
	u32                     need_tune;
	int                     autok_error;
	int                     reautok_times;
	int                     power_cycle_cnt;
	bool                    is_autok_done;
	u8                      use_hw_dvfs;
	u8                      lock_vcore;
/************************ +1 for merge ****************************************/
	u8                autok_res[AUTOK_VCORE_NUM+1][TUNING_PARA_SCAN_COUNT];
	u16                     dvfs_reg_backup_cnt;
	u16                     dvfs_reg_backup_cnt_top;
	u32                     *dvfs_reg_backup;
	u16                     *dvfs_reg_offsets;
	u16                     *dvfs_reg_offsets_src;
	u16                     *dvfs_reg_offsets_top;
	u32                     device_status;
	int                     tune_smpl_times;
	u32                     tune_latch_ck_cnt;
	struct msdc_saved_para  saved_para;
	struct wakeup_source    *trans_lock;
	bool                    block_bad_card;
	struct delayed_work     remove_card;    /* remove bad card */
	u32                     data_timeout_ms;  /* timeout ms for worker */
	struct delayed_work     data_timeout_work;  /* data timeout worker */
#ifdef SDIO_ERROR_BYPASS
	int                     sdio_error;     /* sdio error can't recovery */
#endif
	void    (*power_control)(struct msdc_host *host, u32 on);
	int    (*power_switch)(struct msdc_host *host, u32 on);
	u32                     power_io;
	u32                     power_flash;

	struct pm_qos_request   msdc_pm_qos_req; /* use for pm qos */

	struct clk              *clk_ctl;
	struct clk              *aes_clk_ctl;
	struct clk              *hclk_ctl;
	struct delayed_work	work_init; /* for init mmc_host */
	struct delayed_work	work_sdio; /* for DVFS kickoff */
	struct platform_device  *pdev;

	int                     prev_cmd_cause_dump;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	atomic_t                cq_error_need_stop;
#endif
#if defined(CONFIG_MTK_HW_FDE) \
	&& !defined(CONFIG_MTK_HW_FDE_AES)
	bool                    is_crypto_init;
	u32                     key_idx;
#endif
	u32                     dma_cnt;
	u64                     start_dma_time;
	u64                     stop_dma_time;
	/* flag to record if eMMC will enter hs400 mode */
	bool                    hs400_mode;
#ifdef CONFIG_MTK_EMMC_HW_CQ
	struct cmdq_host *cq_host;
#endif
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

#define MSDC_READ8(reg)           __raw_readb(reg)
#define MSDC_READ16(reg)          __raw_readw(reg)
#define MSDC_READ32(reg)          __raw_readl(reg)
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

#define GET_FIELD(reg, field_shift, field_mask, val) \
	(val = (reg >> field_shift) & field_mask)

#define sdc_is_busy()           (MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY)
#define sdc_is_cmd_busy()       (MSDC_READ32(SDC_STS) & SDC_STS_CMDBUSY)

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
#define sdc_send_cmdq_cmd(opcode, arg) \
	do { \
		MSDC_SET_FIELD(EMMC51_CFG0, MSDC_EMMC51_CFG_CMDQEN \
			| MSDC_EMMC51_CFG_NUM | MSDC_EMMC51_CFG_RSPTYPE \
			| MSDC_EMMC51_CFG_DTYPE, (0x81) | (opcode << 1)); \
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
			msdc_dump_info(NULL, 0, NULL, id); \
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

/* can modify to read h/w register.*/
/* #define is_card_present(h) \
 *			((MSDC_READ32(MSDC_PS) & MSDC_PS_CDSTS) ? 0 : 1);
 */
#define is_card_present(h)      (((struct msdc_host *)(h))->card_inserted)
#define is_card_sdio(h)         (((struct msdc_host *)(h))->hw->register_pm)

#define CMD_TIMEOUT             (HZ/10 * 5)     /* 100ms x5 */
#define CMD_CQ_TIMEOUT          (HZ    * 3)
#define DAT_TIMEOUT             (HZ    * 5)     /* 1000ms x5 */
#define POLLING_BUSY            (HZ    * 3)
#define POLLING_PINS            (HZ*20 / 1000)	/* 20ms */

/* data timeout for worker */
#define DATA_TIMEOUT_MS         (1000  * 30)    /* 30s */
extern struct msdc_host *mtk_msdc_host[];
extern unsigned int msdc_latest_transfer_mode[HOST_MAX_NUM];
extern u32 latest_int_status[];
extern unsigned int msdc_latest_op[];
extern unsigned int sd_register_zone[];

#ifdef MSDC_DMA_ADDR_DEBUG
extern struct dma_addr msdc_latest_dma_address[MAX_BD_PER_GPD];
#endif
extern int g_dma_debug[HOST_MAX_NUM];

extern u32 g_emmc_mode_switch;

enum {
	MODE_PIO = 0,
	MODE_DMA = 1,
	MODE_SIZE_DEP = 2,
	MODE_NONE = 3,
};

extern unsigned int sd_debug_zone[];
extern u32 drv_mode[];
extern u32 dma_size[];
extern int dma_force[];

extern u32 sdio_pro_enable;

extern bool emmc_sleep_failed;

extern int msdc_rsp[];

extern u16 msdc_offsets[];
extern u16 msdc_offsets_top[];

/**********************************************************/
/* Functions                                               */
/**********************************************************/
#include "msdc_io.h"

/* Function provided by sd.c */
int msdc_clk_stable(struct msdc_host *host, u32 mode, u32 div,
	u32 hs400_src);
void msdc_clr_fifo(unsigned int id);
unsigned int msdc_do_command(struct msdc_host *host,
	struct mmc_command *cmd, unsigned long       timeout);
void msdc_dump_info(char **buff, unsigned long *size, struct seq_file *m,
	u32 id);
void msdc_dump_register(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host);
void msdc_dump_register_core(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host);
int msdc_execute_tuning(struct mmc_host *mmc, u32 opcode);
int msdc_error_tuning(struct mmc_host *mmc,  struct mmc_request *mrq);
int msdc_cache_ctrl(struct msdc_host *host, unsigned int enable,
	u32 *status);
int msdc_get_card_status(struct mmc_host *mmc, struct msdc_host *host,
	u32 *status);
int msdc_get_dma_status(int host_id);
void msdc_ops_set_ios(struct mmc_host *mmc, struct mmc_ios *ios);
void msdc_select_clksrc(struct msdc_host *host, int clksrc);
void msdc_send_stop(struct msdc_host *host);
void msdc_set_mclk(struct msdc_host *host, unsigned char timing, u32 hz);
void msdc_set_smpl(struct msdc_host *host, u32 clock_mode, u8 mode, u8 type,
	u8 *edge);
void msdc_set_smpl_all(struct msdc_host *host, u32 clock_mode);
void msdc_set_check_endbit(struct msdc_host *host, bool enable);
int msdc_switch_part(struct msdc_host *host, char part_id);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
unsigned int msdc_do_cmdq_command(struct msdc_host *host,
	struct mmc_command *cmd,
	unsigned long timeout);
#endif

/* Function provided by msdc_partition.c */
int msdc_can_apply_cache(unsigned long long start_addr,
	unsigned int size);
int msdc_check_otp_ops(unsigned int opcode, unsigned long long start_addr,
	unsigned int size);
int msdc_get_part_info(unsigned char *name, struct hd_struct *part);
u64 msdc_get_user_capacity(struct msdc_host *host);
u32 msdc_get_other_capacity(struct msdc_host *host, char *name);

/* Function provided by msdc_tune.c */
int sdcard_hw_reset(struct mmc_host *mmc);
int sdcard_reset_tuning(struct mmc_host *mmc);
int emmc_reinit_tuning(struct mmc_host *mmc);
void msdc_init_tune_setting(struct msdc_host *host);
void msdc_ios_tune_setting(struct msdc_host *host, struct mmc_ios *ios);
void msdc_init_tune_path(struct msdc_host *host, unsigned char timing);
void msdc_sdio_restore_after_resume(struct msdc_host *host);
void msdc_restore_timing_setting(struct msdc_host *host);
void msdc_save_timing_setting(struct msdc_host *host);
void msdc_set_bad_card_and_remove(struct msdc_host *host);
void msdc_remove_card(struct work_struct *work);

/* Function provided by mmc/core/sd.c */
/* FIX ME: maybe removed in kernel 4.4 */
int mmc_sd_power_cycle(struct mmc_host *host, u32 ocr,
	struct mmc_card *card);

/* Function provided by mmc/core/bus.c */
void mmc_remove_card(struct mmc_card *card);

#define check_mmc_cache_ctrl(card) \
	(card && mmc_card_mmc(card) && (card->ext_csd.cache_ctrl & 0x1))
#define check_mmc_cache_flush_cmd(cmd) \
	((cmd->opcode == MMC_SWITCH) && \
	 (((cmd->arg >> 16) & 0xFF) == EXT_CSD_FLUSH_CACHE) && \
	 (((cmd->arg >> 8) & 0x1)))

#define check_mmc_cmd001213(opcode) \
	((opcode == MMC_GO_IDLE_STATE) || \
	 (opcode == MMC_STOP_TRANSMISSION) || \
	 (opcode == MMC_SEND_STATUS))

#define check_mmc_cmd081921(opcode) \
	((opcode == MMC_SEND_EXT_CSD) || \
	 (opcode == MMC_SEND_TUNING_BLOCK) || \
	 (opcode == MMC_SEND_TUNING_BLOCK_HS200))

#define check_mmc_cmd1718(opcode) \
	((opcode == MMC_READ_MULTIPLE_BLOCK) || \
	 (opcode == MMC_READ_SINGLE_BLOCK))
#define check_mmc_cmd2425(opcode) \
	((opcode == MMC_WRITE_MULTIPLE_BLOCK) || \
	 (opcode == MMC_WRITE_BLOCK))
#define check_mmc_cmd1825(opcode) \
	((opcode == MMC_READ_MULTIPLE_BLOCK) || \
	 (opcode == MMC_WRITE_MULTIPLE_BLOCK))

#define check_mmc_cmd4445(opcode) \
	((opcode == MMC_QUE_TASK_PARAMS) || \
	 (opcode == MMC_QUE_TASK_ADDR))
#define check_mmc_cmd46(opcode) \
	(opcode == MMC_EXECUTE_READ_TASK)
#define check_mmc_cmd47(opcode) \
	(opcode == MMC_EXECUTE_WRITE_TASK)
#define check_mmc_cmd4647(opcode) \
	((opcode == MMC_EXECUTE_READ_TASK) || \
	 (opcode == MMC_EXECUTE_WRITE_TASK))
#define check_mmc_cmd48(opcode) \
	(opcode == MMC_CMDQ_TASK_MGMT)
#define check_mmc_cmd44(x) \
	((x) && \
	 ((x)->opcode == MMC_QUE_TASK_PARAMS))
#define check_mmc_cmd13_sqs(x) \
	(((x)->opcode == MMC_SEND_STATUS) && \
	 ((x)->arg & (1 << 15)))
#define check_mmc_cmd47(opcode) \
		 (opcode == MMC_EXECUTE_WRITE_TASK)
#define check_mmc_cmd_r1b(opcode) \
		((opcode == MMC_SWITCH) || \
		 (opcode == MMC_CMDQ_TASK_MGMT))

#endif /* end of  MT_SD_H */
