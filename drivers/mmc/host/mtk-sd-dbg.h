/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MT_MSDC_DEUBG__
#define __MT_MSDC_DEUBG__
#include "mtk-msdc.h"
#include <linux/seq_file.h>
/* #define MTK_MSDC_ERROR_TUNE_DEBUG */

enum {
	SD_TOOL_ZONE = 0,
	SD_TOOL_DMA_SIZE  = 1,
	SD_TOOL_SDIO_PROFILE = 3,
	SD_TOOL_REG_ACCESS = 5,
	SD_TOOL_SET_DRIVING = 6,
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
	MSDC_READ_WRITE = 20,
	MMC_ERROR_TUNE = 21,
	MMC_EDC_EMMC_CACHE = 22,
	MMC_DUMP_GPD = 23,
	MMC_ETT_TUNE = 24,
	MMC_CRC_STRESS = 25,
	ENABLE_AXI_MODULE = 26,
	DO_AUTOK_OFFLINE_TUNE_TX = 29,
	SDIO_AUTOK_RESULT = 30,
	MMC_CMDQ_STATUS = 31,
	SD_TOOL_TOP_REG_ACCESS = 32,
};

/* Debug message event */
#define DBG_EVT_NONE        (0)         /* No event */
#define DBG_EVT_DMA         (1 << 0)    /* DMA related event */
#define DBG_EVT_CMD         (1 << 1)    /* MSDC CMD related event */
#define DBG_EVT_RSP         (1 << 2)    /* MSDC CMD RSP related event */
#define DBG_EVT_INT         (1 << 3)    /* MSDC INT event */
#define DBG_EVT_CFG         (1 << 4)    /* MSDC CFG event */
#define DBG_EVT_FUC         (1 << 5)    /* Function event */
#define DBG_EVT_OPS         (1 << 6)    /* Read/Write operation event */
#define DBG_EVT_FIO         (1 << 7)    /* FIFO operation event */
#define DBG_EVT_WRN         (1 << 8)    /* Warning event */
#define DBG_EVT_PWR         (1 << 9)    /* Power event */
#define DBG_EVT_CLK         (1 << 10)   /* Clock gate/ungate operation */
#define DBG_EVT_CHE         (1 << 11)   /* eMMC cache feature operation */
/* ==================================================== */
#define DBG_EVT_RW          (1 << 12)   /* Trace the Read/Write Command */
#define DBG_EVT_NRW         (1 << 13)   /* Trace other Command */
#define DBG_EVT_ALL         (0xffffffff)

#define DBG_EVT_MASK        (DBG_EVT_ALL)

#define TAGMSDC "msdc"

#ifndef MTK_MMC_PRINT_PERIOD
#define ERR_MSG(fmt, args...) \
	pr_info(TAGMSDC"%d -> "fmt" <- %s() : L<%d> PID<%s><0x%x>\n", \
		host->id, ##args, __func__, __LINE__, current->comm, \
		current->pid)

#else
#define MAX_PRINT_PERIOD            (500000000)  /* 500ms */
#define MAX_PRINT_NUMS_OVER_PERIOD  (50)
#define ERR_MSG(fmt, args...) \
do { \
	if (print_nums == 0) { \
		print_nums++; \
		msdc_print_start_time = sched_clock(); \
		pr_info(TAGMSDC"MSDC", TAG"%d -> "fmt" <- %s() : L<%d> " \
			"PID<%s><0x%x>\n", \
			host->id, ##args, __func__, __LINE__, \
			current->comm, current->pid); \
	} else { \
		msdc_print_end_time = sched_clock();    \
		if ((msdc_print_end_time - msdc_print_start_time) >= \
			MAX_PRINT_PERIOD) { \
			pr_info( \
			TAGMSDC"MSDC", TAG"%d -> "fmt" <- %s() : L<%d> " \
				"PID<%s><0x%x>\n", \
				host->id, ##args, __func__, __LINE__, \
				current->comm, current->pid); \
			print_nums = 0; \
		} \
		if (print_nums <= MAX_PRINT_NUMS_OVER_PERIOD) { \
			pr_info(TAGMSDC"MSDC", TAG"%d -> "fmt" <- %s() : " \
				"L<%d> PID<%s><0x%x>\n", \
				host->id, ##args, __func__, \
				__LINE__, current->comm, current->pid); \
			print_nums++;   \
		} \
	} \
} while (0)
#endif

#define INIT_MSG(fmt, args...) \
	pr_info(TAGMSDC"%d -> "fmt" <- %s() : L<%d> PID<%s><0x%x>\n", \
		host->id, ##args, __func__, __LINE__, current->comm, \
		current->pid)

#define INFO_MSG(fmt, args...) \
	pr_debug(TAGMSDC"%d -> "fmt" <- %s() : L<%d> PID<%s><0x%x>\n", \
		host->id, ##args, __func__, __LINE__, current->comm, \
		current->pid)

#ifdef MTK_MMC_PRINT_IRQ_MSG
/* PID in ISR in not corrent */
#define IRQ_MSG(fmt, args...) \
	pr_info(TAGMSDC"%d -> "fmt" <- %s() : L<%d>\n", \
		host->id, ##args, __func__, __LINE__)
#else
#define IRQ_MSG(fmt, args...)
#endif

/*
 * snprintf may return a value of size or "more" to indicate
 * that the output was truncated, thus be careful of "more"
 * case.
 */
#define SPREAD_PRINTF(buff, size, evt, fmt, args...) \
do { \
	if (buff && size && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
		if (var > 0) { \
			if (var > *(size)) \
				var = *(size); \
			*(size) -= var; \
			*(buff) += var; \
		} \
	} \
	if (evt) \
		seq_printf(evt, fmt, ##args); \
	if (!buff && !evt) { \
		pr_info(fmt, ##args); \
	} \
} while (0)

#define MAGIC_CQHCI_DBG_TYPE 5
#define MAGIC_CQHCI_DBG_NUM_L 100
#define MAGIC_CQHCI_DBG_NUM_U 200
#define MAGIC_CQHCI_DBG_NUM_RI 500

#define MAGIC_CQHCI_DBG_TYPE_DCMD 60
/* softirq type */
#define MAGIC_CQHCI_DBG_TYPE_SIRQ 70
void msdc_debug_set_host(struct mmc_host *mmc);
int msdc_debug_proc_init(void);
void dbg_add_host_log(struct mmc_host *mmc, int type, int cmd, int arg);
void mmc_cmd_dump(char **buff, unsigned long *size, struct seq_file *m,
		struct mmc_host *mmc, u32 latest_cnt);
void msdc_dump_host_state(char **buff, unsigned long *size,
		struct seq_file *m, struct msdc_host *host);
#endif
