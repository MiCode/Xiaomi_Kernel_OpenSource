#ifndef __MT_MSDC_DEUBG__
#define __MT_MSDC_DEUBG__
#include "mt_sd.h"
#include <linux/xlog.h>

/* ========================== */
extern u32 sdio_pro_enable;
/* for a type command, e.g. CMD53, 2 blocks */
struct cmd_profile {
	u32 max_tc;		/* Max tick count */
	u32 min_tc;
	u32 tot_tc;		/* total tick count */
	u32 tot_bytes;
	u32 count;		/* the counts of the command */
};

/* dump when total_tc and total_bytes */
struct sdio_profile {
	u32 total_tc;		/* total tick count of CMD52 and CMD53 */
	u32 total_tx_bytes;	/* total bytes of CMD53 Tx */
	u32 total_rx_bytes;	/* total bytes of CMD53 Rx */

	/*CMD52 */
	struct cmd_profile cmd52_tx;
	struct cmd_profile cmd52_rx;

	/*CMD53 in byte unit */
	struct cmd_profile cmd53_tx_byte[512];
	struct cmd_profile cmd53_rx_byte[512];

	/*CMD53 in block unit */
	struct cmd_profile cmd53_tx_blk[100];
	struct cmd_profile cmd53_rx_blk[100];
};

//#define MTK_MSDC_ERROR_TUNE_DEBUG
#ifdef MTK_MSDC_ERROR_TUNE_DEBUG
#define MTK_MSDC_ERROR_NONE (0)
#define MTK_MSDC_ERROR_CMD_TMO (0x1)
#define MTK_MSDC_ERROR_CMD_CRC (0x1 << 1)
#define MTK_MSDC_ERROR_DAT_TMO (0x1 << 2)
#define MTK_MSDC_ERROR_DAT_CRC (0x1 << 3)
#define MTK_MSDC_ERROR_ACMD_TMO (0x1 << 4)
#define MTK_MSDC_ERROR_ACMD_CRC (0x1 << 5)

extern unsigned int g_err_tune_dbg_host;
extern unsigned int g_err_tune_dbg_cmd;
extern unsigned int g_err_tune_dbg_arg;
extern unsigned int g_err_tune_dbg_error;
extern unsigned int g_err_tune_dbg_count;
#endif

/* ========================== */
typedef enum {
    SD_TOOL_ZONE = 0, 
    SD_TOOL_DMA_SIZE  = 1,	
    SD_TOOL_PM_ENABLE = 2,
    SD_TOOL_SDIO_PROFILE = 3,
    SD_TOOL_CLK_SRC_SELECT = 4,
    SD_TOOL_REG_ACCESS = 5,
    SD_TOOL_SET_DRIVING =6,
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
#if MTK_MSDC_USE_EDC_EMMC_CACHE
    MMC_EDC_EMMC_CACHE = 22,
#endif
	MMC_DUMP_GPD = 23,
	MMC_ETT_TUNE = 24,
	MMC_CRC_STRESS = 25,
	ENABLE_AXI_MODULE = 26,
} msdc_dbg;	

typedef enum {
	MODE_PIO = 0,
	MODE_DMA = 1,
	MODE_SIZE_DEP = 2,
} msdc_mode;
typedef struct {
	unsigned char clk_drv;
	unsigned char cmd_drv;
	unsigned char dat_drv;
	unsigned char rst_drv;
	unsigned char ds_drv;
} drv_mod;

extern msdc_mode drv_mode[HOST_MAX_NUM];
extern u32 dma_size[HOST_MAX_NUM];
extern struct msdc_host *mtk_msdc_host[HOST_MAX_NUM];	/* for fpga early porting */
extern unsigned char msdc_clock_src[HOST_MAX_NUM];
extern drv_mod msdc_drv_mode[HOST_MAX_NUM];
extern u32 msdc_host_mode[HOST_MAX_NUM];	/*SD/eMMC mode (HS/DDR/UHS) */
extern u32 msdc_host_mode2[HOST_MAX_NUM];
extern int g_dma_debug[HOST_MAX_NUM];

extern transfer_mode msdc_latest_transfer_mode[HOST_MAX_NUM];
extern operation_type msdc_latest_operation_type[HOST_MAX_NUM];
extern struct dma_addr msdc_latest_dma_address[MAX_BD_PER_GPD];
extern struct dma_addr *msdc_get_dma_address(int host_id);
extern int msdc_get_dma_status(int host_id);
extern int emmc_multi_rw_compare(int host_num, uint address, int count);
extern int msdc_tune_write(struct msdc_host *host);
extern int msdc_tune_read(struct msdc_host *host);
extern int msdc_tune_cmdrsp(struct msdc_host *host);
extern int emmc_hs400_tune_rw(struct msdc_host *host);

extern int g_ett_tune;
extern int g_ett_hs400_tune;
extern int g_ett_cmd_tune;
extern int g_ett_read_tune;
extern int g_ett_write_tune;
extern int g_reset_tune;

extern u32 sdio_enable_tune;
extern u32 sdio_iocon_dspl;
extern u32 sdio_iocon_w_dspl;
extern u32 sdio_iocon_rspl;

extern u32 sdio_pad_tune_rrdly;
extern u32 sdio_pad_tune_rdly;
extern u32 sdio_pad_tune_wrdly;
extern u32 sdio_dat_rd_dly0_0;
extern u32 sdio_dat_rd_dly0_1;
extern u32 sdio_dat_rd_dly0_2;
extern u32 sdio_dat_rd_dly0_3;
extern u32 sdio_dat_rd_dly1_0;
extern u32 sdio_dat_rd_dly1_1;
extern u32 sdio_dat_rd_dly1_2;
extern u32 sdio_dat_rd_dly1_3;
extern u32 sdio_clk_drv;
extern u32 sdio_cmd_drv;
extern u32 sdio_data_drv;
extern u32 sdio_tune_flag;
/* Debug message event */
#define DBG_EVT_NONE        (0)	/* No event */
#define DBG_EVT_DMA         (1 << 0)	/* DMA related event */
#define DBG_EVT_CMD         (1 << 1)	/* MSDC CMD related event */
#define DBG_EVT_RSP         (1 << 2)	/* MSDC CMD RSP related event */
#define DBG_EVT_INT         (1 << 3)	/* MSDC INT event */
#define DBG_EVT_CFG         (1 << 4)	/* MSDC CFG event */
#define DBG_EVT_FUC         (1 << 5)	/* Function event */
#define DBG_EVT_OPS         (1 << 6)	/* Read/Write operation event */
#define DBG_EVT_FIO         (1 << 7)	/* FIFO operation event */
#define DBG_EVT_WRN         (1 << 8)	/* Warning event */
#define DBG_EVT_PWR         (1 << 9)	/* Power event */
#define DBG_EVT_CLK         (1 << 10)	/* Trace clock gate/ungate operation */
#define DBG_EVT_CHE         (1 << 11)	/* eMMC cache feature operation */
/* ==================================================== */
#define DBG_EVT_RW          (1 << 12)	/* Trace the Read/Write Command */
#define DBG_EVT_NRW         (1 << 13)	/* Trace other Command */
#define DBG_EVT_ALL         (0xffffffff)

#define DBG_EVT_MASK        (DBG_EVT_ALL)

extern unsigned int sd_debug_zone[HOST_MAX_NUM];
#define TAG "msdc"
#define N_MSG(evt, fmt, args...) \
do {    \
    if ((DBG_EVT_##evt) & sd_debug_zone[host->id]) { \
	pr_err(TAG"%d -> "fmt" <- %s() : L<%d> PID<%s><0x%x>\n", \
		host->id,  ##args , __FUNCTION__, __LINE__, current->comm, current->pid); \
    }   \
} while(0)
#if 1
#define ERR_MSG(fmt, args...) \
do { \
	pr_err(TAG"%d -> "fmt" <- %s() : L<%d> PID<%s><0x%x>\n", \
        host->id,  ##args , __FUNCTION__, __LINE__, current->comm, current->pid); \
} while(0); 
#else
#define MAX_PRINT_PERIOD            (500000000)  /* 500ms */
#define MAX_PRINT_NUMS_OVER_PERIOD  (50)
#define ERR_MSG(fmt, args...) \
do { \
	   if (print_nums == 0){ \
		   print_nums++; \
		   msdc_print_start_time = sched_clock(); \
		   pr_err(TAG"MSDC",TAG"%d -> "fmt" <- %s() : L<%d> PID<%s><0x%x>\n", \
		   		host->id,  ##args , __FUNCTION__, __LINE__, current->comm, current->pid); \
	   } else { \
		   msdc_print_end_time = sched_clock();	\
		   if ((msdc_print_end_time - msdc_print_start_time) >= MAX_PRINT_PERIOD){ \
			   pr_err(TAG"MSDC",TAG"%d -> "fmt" <- %s() : L<%d> PID<%s><0x%x>\n", \
					host->id,  ##args , __FUNCTION__, __LINE__, current->comm, current->pid); \
			   print_nums = 0; \
		   } else {	\
			   if (print_nums <= MAX_PRINT_NUMS_OVER_PERIOD){ \
					pr_err(TAG"MSDC",TAG"%d -> "fmt" <- %s() : L<%d> PID<%s><0x%x>\n", \
						host->id,  ##args , __FUNCTION__, __LINE__, current->comm, current->pid); \
					print_nums++;	\
			   } \
		   } \
	   } \
} while(0);
#endif
#define INIT_MSG(fmt, args...) \
do { \
    pr_err(TAG"%d -> "fmt" <- %s() : L<%d> PID<%s><0x%x>\n", \
		host->id,  ##args , __FUNCTION__, __LINE__, current->comm, current->pid); \
} while (0);
#define INFO_MSG(fmt, args...) \
do { \
	pr_info(TAG"%d -> "fmt" <- %s() : L<%d> PID<%s><0x%x>\n", \
		host->id,  ##args , __FUNCTION__, __LINE__, current->comm, current->pid); \
} while(0);

#if 0
/* PID in ISR in not corrent */
#define IRQ_MSG(fmt, args...) \
do { \
    pr_err(TAG"%d -> "fmt" <- %s() : L<%d>\n", \
		host->id,  ##args , __FUNCTION__, __LINE__); \
} while (0);
#else
#define IRQ_MSG(fmt, args...) \
do { \
} while (0);
#endif

int msdc_debug_proc_init(void);

extern void GPT_GetCounter64(UINT32 *cntL32, UINT32 *cntH32);
u32 msdc_time_calc(u32 old_L32, u32 old_H32, u32 new_L32, u32 new_H32);
void msdc_performance(u32 opcode, u32 sizes, u32 bRx, u32 ticks);   

#endif
