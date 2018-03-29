/******************************************************************************
* mtk_nand.c - MTK NAND Flash Device Driver
*
* Copyright 2009-2012 MediaTek Co., Ltd.
*
* DESCRIPTION:
*	This file provid the other drivers nand relative functions
*
* modification history
* ----------------------------------------
* v3.0, 11 Feb 2010, mtk
* ----------------------------------------
******************************************************************************/
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/syscalls.h>
#include <asm/io.h>
#include <asm/cacheflush.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <mach/dma.h>
#include <mach/mt_clkmgr.h>
#include <mach/mtk_nand.h>
#include "mtk_nand_util.h"
#include "bmt.h"
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/rtc.h>
#ifdef CONFIG_PWR_LOSS_MTK_SPOH
#include <mach/power_loss_test.h>
#endif
#include <asm/div64.h>
#include <linux/regulator/consumer.h>
#define VERSION	"v2.1 Fix AHB virt2phys error"
#define MODULE_NAME	"# MTK NAND #"
#define PROCNAME	"driver/nand"
#define PMT							1
/*#define _MTK_NAND_DUMMY_DRIVER_*/
#define __INTERNAL_USE_AHB_MODE__	1

/*
 * Access Pattern Logger
 *
 * Enable the feacility to record MTD/DRV read/program/erase pattern
 */

#if (defined(CONFIG_MTK_MLC_NAND_SUPPORT) || defined(CONFIG_MTK_TLC_NAND_SUPPORT))
bool MLC_DEVICE = TRUE;
#endif
static bool DDR_INTERFACE = FALSE;
void __iomem *mtk_nfi_base;
void __iomem *mtk_nfiecc_base;
void __iomem *mtk_io_base;

struct device_node *mtk_nfi_node;
struct device_node *mtk_nfiecc_node;
struct device_node *mtk_io_node;
struct device_node *mtk_pm_node;

struct regulator *nfi_reg_vemc_3v3 = NULL;

unsigned int nfi_irq = 0;
#define MT_NFI_IRQ_ID nfi_irq

struct device *mtk_dev;
struct scatterlist mtk_sg;
enum dma_data_direction mtk_dir;


bool tlc_lg_left_plane = TRUE;
enum NFI_TLC_PG_CYCLE tlc_program_cycle;
bool tlc_not_keep_erase_lvl = FALSE;
u32 slc_ratio = 6;
u32 sys_slc_ratio = 6;
u32 usr_slc_ratio = 6;

u32 force_slc_flag = 0;

#if defined(NAND_OTP_SUPPORT)

#define SAMSUNG_OTP_SUPPORT	 1
#define OTP_MAGIC_NUM		0x4E3AF28B
#define SAMSUNG_OTP_PAGE_NUM	6

static const unsigned int Samsung_OTP_Page[SAMSUNG_OTP_PAGE_NUM] = {
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1b };

static struct mtk_otp_config g_mtk_otp_fuc;
static spinlock_t g_OTPLock;

#define OTP_MAGIC		'k'

/* NAND OTP IO control number */
#define OTP_GET_LENGTH		_IOW(OTP_MAGIC, 1, int)
#define OTP_READ			_IOW(OTP_MAGIC, 2, int)
#define OTP_WRITE			_IOW(OTP_MAGIC, 3, int)

#define FS_OTP_READ		 0
#define FS_OTP_WRITE		1

/* NAND OTP Error codes */
#define OTP_SUCCESS				0
#define OTP_ERROR_OVERSCOPE		  -1
#define OTP_ERROR_TIMEOUT			-2
#define OTP_ERROR_BUSY			-3
#define OTP_ERROR_NOMEM			  -4
#define OTP_ERROR_RESET			  -5

struct mtk_otp_config {
	u32 (*OTPRead)(u32 PageAddr, void *BufferPtr, void *SparePtr);
	u32 (*OTPWrite)(u32 PageAddr, void *BufferPtr, void *SparePtr);
	u32 (*OTPQueryLength)(u32 *Length);
};

struct otp_ctl {
	unsigned int QLength;
	unsigned int Offset;
	unsigned int Length;
	char *BufferPtr;
	unsigned int status;
};
#endif

#define ERR_RTN_SUCCESS		1
#define ERR_RTN_FAIL		0
#define ERR_RTN_BCH_FAIL	-1

#define NFI_SET_REG32(reg, value) \
do {	\
	g_value = (DRV_Reg32(reg) | (value));\
	DRV_WriteReg32(reg, g_value); \
} while (0)

#define NFI_SET_REG16(reg, value) \
do {	\
	g_value = (DRV_Reg16(reg) | (value));\
	DRV_WriteReg16(reg, g_value); \
} while (0)

#define NFI_CLN_REG32(reg, value) \
do {	\
	g_value = (DRV_Reg32(reg) & (~(value)));\
	DRV_WriteReg32(reg, g_value); \
} while (0)

#define NFI_CLN_REG16(reg, value) \
do {	\
	g_value = (DRV_Reg16(reg) & (~(value)));\
	DRV_WriteReg16(reg, g_value); \
} while (0)

#define NFI_WAIT_STATE_DONE(state) do {; } while (__raw_readl(NFI_STA_REG32) & state)
#define NFI_WAIT_TO_READY()  do {; } while (!(__raw_readl(NFI_STA_REG32) & STA_BUSY2READY))
#define FIFO_PIO_READY(x)  (0x1 & x)
#define WAIT_NFI_PIO_READY(timeout) \
	do {\
		--timeout;\
		while ((!FIFO_PIO_READY(DRV_Reg16(NFI_PIO_DIRDY_REG16))) && \
		(timeout)) \
			; \
	} while (0)


#define NAND_SECTOR_SIZE (512)
#define OOB_PER_SECTOR	  (16)
#define OOB_AVAI_PER_SECTOR (8)

#if defined(CONFIG_MTK_COMBO_NAND_SUPPORT)
#ifndef PART_SIZE_BMTPOOL
		#define BMT_POOL_SIZE (80)
#else
		#define BMT_POOL_SIZE (PART_SIZE_BMTPOOL)
#endif
#else
#ifndef PART_SIZE_BMTPOOL
	#define BMT_POOL_SIZE (80)
#else
	#define BMT_POOL_SIZE (PART_SIZE_BMTPOOL)
#endif
#endif
u8 ecc_threshold;
#define PMT_POOL_SIZE	(2)
bool g_b2Die_CS;

/*******************************************************************************
 * Gloable Varible Definition
 *******************************************************************************/
struct nand_perf_log {
	unsigned int ReadPageCount;
	suseconds_t  ReadPageTotalTime;
	unsigned int ReadBusyCount;
	suseconds_t  ReadBusyTotalTime;
	unsigned int ReadDMACount;
	suseconds_t  ReadDMATotalTime;

	unsigned int ReadSubPageCount;
	suseconds_t  ReadSubPageTotalTime;

	unsigned int WritePageCount;
	suseconds_t  WritePageTotalTime;
	unsigned int WriteBusyCount;
	suseconds_t  WriteBusyTotalTime;
	unsigned int WriteDMACount;
	suseconds_t  WriteDMATotalTime;

	unsigned int EraseBlockCount;
	suseconds_t  EraseBlockTotalTime;

};

#ifdef PWR_LOSS_SPOH

#define PL_TIME_RAND_PROG(chip, page_addr, time) do { \
	if (host->pl.nand_program_wdt_enable == 1) \
		PL_TIME_RAND(page_addr, time, host->pl.last_prog_time); \
	else \
		time = 0; \
	} while (0)

#define PL_TIME_RAND_ERASE(chip, page_addr, time) do { \
	if (host->pl.nand_erase_wdt_enable == 1) { \
		PL_TIME_RAND(page_addr, time, host->pl.last_erase_time); \
		if (time != 0) \
			pr_err("[MVG_TEST]: Erase reset in %d us\n", time); \
		} else \
		time = 0; \
	} while (0)

#define PL_TIME_PROG(duration)	host->pl.last_prog_time = duration
#define PL_TIME_ERASE(duration) host->pl.last_erase_time = duration
#define PL_TIME_PROG_WDT_SET(WDT) host->pl.nand_program_wdt_enable = WDT
#define PL_TIME_ERASE_WDT_SET(WDT) host->pl.nand_erase_wdt_enable = WDT

#define PL_NAND_BEGIN(time) PL_BEGIN(time)

#define PL_NAND_RESET(time) PL_RESET(time)

#define PL_NAND_END(pl_time_write, duration) PL_END(pl_time_write, duration)


#else

#define PL_TIME_RAND_PROG(chip, page_addr, time)
#define PL_TIME_RAND_ERASE(chip, page_addr, time)

#define PL_TIME_PROG(duration)
#define PL_TIME_ERASE(duration)

#define PL_TIME_PROG_WDT_SET(WDT)
#define PL_TIME_ERASE_WDT_SET(WDT)

#define PL_NAND_BEGIN(time)
#define PL_NAND_RESET(time)
#define PL_NAND_END(pl_time_write, duration)

#endif
#ifdef DUMP_PEF
static struct nand_perf_log g_NandPerfLog = {0};
static struct timeval g_NandLogTimer = {0};
#endif
#ifdef MTK_NAND_CMD_DUMP
struct cmd_sequence {
	u8 cmd_array[2];
	u32 address[3];
};
struct write_trace_dbg {
	struct cmd_sequence cmd;
};
#define MAX_CMD_LOG_CNT	(20480)
struct write_trace_dbg dbg_inf[MAX_CMD_LOG_CNT];
u32 current_idx = 0;
u32 last_idx = 0;

#define current_idx_add() do {\
	current_idx++;\
	if (current_idx == MAX_CMD_LOG_CNT)\
		current_idx = 0;\
	dbg_inf[current_idx].cmd.cmd_array[0] = 0xFF;\
	dbg_inf[current_idx].cmd.cmd_array[1] = 0xFF;\
	dbg_inf[current_idx].cmd.address[0] = 0xFF;\
	dbg_inf[current_idx].cmd.address[1] = 0xFF;\
	dbg_inf[current_idx].cmd.address[2] = 0xFF;\
} while (0)

void dump_cmd_log(void)
{
	u32 idx;

	idx = current_idx;
	while (idx != last_idx) {
		MSG(INIT, "dbg_inf[%d].cmd = (0x%x, 0x%x) addr(%d, %d, %d)\n",
			idx, dbg_inf[idx].cmd.cmd_array[0], dbg_inf[idx].cmd.cmd_array[1],
			dbg_inf[idx].cmd.address[0], dbg_inf[idx].cmd.address[1],
			dbg_inf[idx].cmd.address[2]);
		if (idx == 0)
			idx = MAX_CMD_LOG_CNT;
		idx--;
		}
		last_idx = current_idx;
}
#else
#define dump_cmd_log()
#define current_idx_add()
#endif
#ifdef NAND_PFM
static suseconds_t g_PFM_R;
static suseconds_t g_PFM_W;
static suseconds_t g_PFM_E;
static u32 g_PFM_RNum;
static u32 g_PFM_RD;
static u32 g_PFM_WD;
static struct timeval g_now;

#define PFM_BEGIN(time) \
do { \
	do_gettimeofday(&g_now); \
	(time) = g_now; \
} while (0)

#define PFM_END_R(time, n) \
do { \
	do_gettimeofday(&g_now); \
	g_PFM_R += (g_now.tv_sec * 1000000 + g_now.tv_usec) - (time.tv_sec * 1000000 + time.tv_usec); \
	g_PFM_RNum += 1; \
	g_PFM_RD += n; \
	MSG(PERFORMANCE, "%s - Read PFM: %lu, data: %d, ReadOOB: %d (%d, %d)\n", MODULE_NAME , g_PFM_R, g_PFM_RD, \
	g_kCMD.pureReadOOB, g_kCMD.pureReadOOBNum, g_PFM_RNum); \
} while (0)

#define PFM_END_W(time, n) \
do { \
	do_gettimeofday(&g_now); \
	g_PFM_W += (g_now.tv_sec * 1000000 + g_now.tv_usec) - (time.tv_sec * 1000000 + time.tv_usec); \
	g_PFM_WD += n; \
	MSG(PERFORMANCE, "%s - Write PFM: %lu, data: %d\n", MODULE_NAME, g_PFM_W, g_PFM_WD); \
} while (0)

#define PFM_END_E(time) \
do { \
	do_gettimeofday(&g_now); \
	g_PFM_E += (g_now.tv_sec * 1000000 + g_now.tv_usec) - (time.tv_sec * 1000000 + time.tv_usec); \
	MSG(PERFORMANCE, "%s - Erase PFM: %lu\n", MODULE_NAME, g_PFM_E); \
} while (0)
#else
#define PFM_BEGIN(time)
#define PFM_END_R(time, n)
#define PFM_END_W(time, n)
#define PFM_END_E(time)
#endif
/* #define __D_PFM__ */

#ifdef __D_PFM__
static suseconds_t g_DPFM_R;
static suseconds_t g_DPFM_RS;
static suseconds_t g_DPFM_W;
static suseconds_t g_DPFM_WB;
static suseconds_t g_DPFM_E;
static u64 g_PFM_RD;
static u64 g_PFM_RDS;

static u64 g_PFM_WD;
static u64 g_PFM_WBD;
static u64 g_PFM_RDN;
static u64 g_PFM_RDSN;
static u64 g_PFM_WDN;
static u64 g_PFM_EDN;
static struct timeval g_now;

#define PFM_DBEGIN(time) \
do { \
	do_gettimeofday(&g_now); \
	(time) = g_now; \
} while (0)

#define PFM_END_DR(time, n, p) \
do { \
	do_gettimeofday(&g_now); \
	g_DPFM_R += (g_now.tv_sec * 1000000 + g_now.tv_usec) - (time.tv_sec * 1000000 + time.tv_usec); \
	g_PFM_RD += n; \
	g_PFM_RDN += p;\
} while (0)

#define PFM_END_DRS(time, n, p) \
do { \
	do_gettimeofday(&g_now); \
	g_DPFM_RS += (g_now.tv_sec * 1000000 + g_now.tv_usec) - (time.tv_sec * 1000000 + time.tv_usec); \
	g_PFM_RDS += n; \
	g_PFM_RDSN += p;\
} while (0)


#define PFM_END_DW(time, n) \
do { \
	do_gettimeofday(&g_now); \
	g_DPFM_W += (g_now.tv_sec * 1000000 + g_now.tv_usec) - (time.tv_sec * 1000000 + time.tv_usec); \
	g_PFM_WD += n; \
	g_PFM_WDN += 1;\
} while (0)

#define PFM_END_DBW(time) \
do { \
	do_gettimeofday(&g_now); \
	g_DPFM_WB += (g_now.tv_sec * 1000000 + g_now.tv_usec) - (time.tv_sec * 1000000 + time.tv_usec); \
	g_PFM_WBD += 1; \
} while (0)


#define PFM_END_DE(time) \
do { \
	do_gettimeofday(&g_now); \
	g_DPFM_E += (g_now.tv_sec * 1000000 + g_now.tv_usec) - (time.tv_sec * 1000000 + time.tv_usec); \
	g_PFM_EDN += 1;\
} while (0)

#define PFM_DUMP_D() \
do { \
	pr_err("%s - Read page PFM: %lu, data: %lld,  #%lld sectors\n", MODULE_NAME , g_DPFM_R, g_PFM_RD, g_PFM_RDN); \
	pr_err("%s - Read sector: %lu, data: %lld, #%lld sectors\n", MODULE_NAME , g_DPFM_RS, g_PFM_RDS, g_PFM_RDSN); \
	pr_err("%s - Write PFM: %lu, data: %lld #%lld times\n", MODULE_NAME, g_DPFM_W, g_PFM_WD, g_PFM_WDN); \
	pr_err("%s - Write TLC PFM: %lu, #%lld times\n", MODULE_NAME, g_DPFM_WB, g_PFM_WBD); \
	pr_err("%s - Erase PFM: %lu #%lld times\n", MODULE_NAME, g_DPFM_E, g_PFM_EDN); \
} while (0)


#endif

#define TIMEOUT_1 0x1fff
#define TIMEOUT_2 0x8ff
#define TIMEOUT_3 0xffff
#define TIMEOUT_4 0xffff

#define NFI_ISSUE_COMMAND(cmd, col_addr, row_addr, col_num, row_num) \
do { \
	DRV_WriteReg16(NFI_CMD_REG16, cmd);\
	while (DRV_Reg32(NFI_STA_REG32) & STA_CMD_STATE)\
		;\
	  DRV_WriteReg32(NFI_COLADDR_REG32, col_addr);\
	  DRV_WriteReg32(NFI_ROWADDR_REG32, row_addr);\
	DRV_WriteReg16(NFI_ADDRNOB_REG16, col_num | (row_num << ADDR_ROW_NOB_SHIFT));\
	while (DRV_Reg32(NFI_STA_REG32) & STA_ADDR_STATE)\
		;\
} while (0)

/*-------------------------------------------------------------------------------*/
static struct completion g_comp_AHB_Done;
static struct NAND_CMD g_kCMD;
bool g_bInitDone;
static int g_i4Interrupt;
static bool g_bcmdstatus;
static bool g_brandstatus;
static u32 g_value;
static int g_page_size;
static int g_block_size;
static u32 PAGES_PER_BLOCK = 255;

#if __INTERNAL_USE_AHB_MODE__
bool g_bHwEcc = true;
#else
bool g_bHwEcc = false;
#endif
#define LPAGE 16384
#define LSPARE 2048

/* create default to read id */
struct mtk_nand_host_hw mtk_nand_hw = {
	.nfi_bus_width		  = 8,
	.nfi_access_timing		= NFI_DEFAULT_ACCESS_TIMING,
	.nfi_cs_num				= NFI_CS_NUM,
	.nand_sec_size			= 512,
	.nand_sec_shift			= 9,
	.nand_ecc_size			= 2048,
	.nand_ecc_bytes			= 32,
	.nand_ecc_mode			= NAND_ECC_HW,
};

static u8 *local_buffer_16_align;	/* 16 byte aligned buffer, for HW issue */
__aligned(64)
static u8 local_buffer[16384 + 2048];
static u8 *temp_buffer_16_align;	/* 16 byte aligned buffer, for HW issue */
__aligned(64)
static u8 temp_buffer[16384 + 2048];
__aligned(64)
u8 rrtry_buffer[16384 + 2048];

#ifdef NAND_FEATURE_TEST
__aligned(64)
static u8 test_buffer[16384 + 2048];
#endif

__aligned(64)
static u8 local_tlc_wl_buffer[16384 + 2048];

static int mtk_nand_interface_config(struct mtd_info *mtd);

static bmt_struct *g_bmt;
struct mtk_nand_host *host;
static u8 g_running_dma;
#ifdef DUMP_NATIVE_BACKTRACE
static u32 g_dump_count;
#endif
int manu_id;
int dev_id;

static u8 local_oob_buf[2048];

#ifdef _MTK_NAND_DUMMY_DRIVER_
int dummy_driver_debug;
#endif

typedef u32 (*GetLowPageNumber)(u32 pageNo);
typedef u32 (*TransferPageNumber)(u32 pageNo, bool high_to_low);

enum NAND_TYPE_MASK {
	TYPE_ASYNC = 0x0,
	TYPE_TOGGLE = 0x1,
	TYPE_SYNC = 0x2,
	TYPE_RESERVED = 0x3,
	TYPE_MLC = 0x4,
	TYPE_SLC = 0x4,
};


flashdev_info gn_devinfo;

GetLowPageNumber functArray[] = {
	MICRON_TRANSFER,
	HYNIX_TRANSFER,
	SANDISK_TRANSFER,
};

TransferPageNumber fsFuncArray[] = {
	micron_pairpage_mapping,
	hynix_pairpage_mapping,
	sandisk_pairpage_mapping,
};

u32 SANDISK_TRANSFER(u32 pageNo)
{
	if (0 == pageNo)
		return pageNo;
	else
		return pageNo+pageNo-1;
}

u32 HYNIX_TRANSFER(u32 pageNo)
{
	u32 temp;

	if (pageNo < 4)
		return pageNo;
	temp = pageNo+(pageNo&0xFFFFFFFE)-2;
	return temp;
}


u32 MICRON_TRANSFER(u32 pageNo)
{
	u32 temp;

	if (pageNo < 4)
		return pageNo;
	temp = (pageNo - 4) & 0xFFFFFFFE;
	if (pageNo <= 130)
		return pageNo + temp;
	return pageNo + temp - 2;
}

u32 sandisk_pairpage_mapping(u32 page, bool high_to_low)
{
	if (TRUE == high_to_low) {
		if (page == 255)
			return page-2;
		if ((page == 0) || (1 == (page%2)))
			return page;
		if (page == 2)
			return 0;
		return page - 3;
	}
	if (TRUE != high_to_low) {
		if ((page != 0) && (0 == (page%2)))
			return page;
		if (page == 255)
			return page;
		if (page == 0 || page == 253)
			return page + 2;
		return page+3;
	}
	return page + 3;
}

u32 hynix_pairpage_mapping(u32 page, bool high_to_low)
{
	u32 offset;

	if (TRUE == high_to_low) {
		/*Micron 256pages */
		if (page < 4)
			return page;

		offset = page % 4;
		if (offset == 2 || offset == 3)
			return page;
		if (page == 4 || page == 5 || page == 254 || page == 255)
			return page - 4;
		return page - 6;
	}
	if (TRUE != high_to_low) {
		if (page > 251)
			return page;

		if (page == 0 || page == 1)
			return page + 4;
		offset = page % 4;
		if (offset == 0 || offset == 1)
			return page;
		return page + 6;
	}
	return page + 6;
}

u32 micron_pairpage_mapping(u32 page, bool high_to_low)
{
	u32 offset;

	if (TRUE == high_to_low) {
		/*Micron 256pages */
		if ((page < 4) || (page > 251))
			return page;

		offset = page % 4;
		if (offset == 0 || offset == 1)
			return page;
		else
			return page - 6;

	} else {
		if ((page == 2) || (page == 3) || (page > 247))
			return page;

		offset = page % 4;
		if (offset == 0 || offset == 1)
			return page + 6;
		else
			return page;
	}
}

int mtk_nand_paired_page_transfer(u32 pageNo, bool high_to_low)
{
	if (gn_devinfo.vendor != VEND_NONE)
		return fsFuncArray[gn_devinfo.feature_set.ptbl_idx] (pageNo, high_to_low);
	else
		return 0xFFFFFFFF;
}


void nand_enable_clock(void)
{
#if 1
	enable_clock(MT_CG_NFI_SW_CG, "NFI");
	enable_clock(MT_CG_NFI2X_SW_CG, "NFI");
	enable_clock(MT_CG_NFI_BUS_SW_CG, "NFI");
	enable_clock(MT_CG_NFIECC_SW_CG, "NFI");
#endif	 /*TODO*/
return;
}

void nand_disable_clock(void)
{
#if 1
	disable_clock(MT_CG_NFI_SW_CG, "NFI");
	disable_clock(MT_CG_NFI2X_SW_CG, "NFI");
	disable_clock(MT_CG_NFI_BUS_SW_CG, "NFI");
	disable_clock(MT_CG_NFIECC_SW_CG, "NFI");
#endif	 /*TODO*/
return;
}

static struct nand_ecclayout nand_oob_16 = {
	.eccbytes = 8,
	.eccpos = {8, 9, 10, 11, 12, 13, 14, 15},
	.oobfree = {{1, 6}, {0, 0} }
};

struct nand_ecclayout nand_oob_64 = {
	.eccbytes = 32,
	.eccpos = {32, 33, 34, 35, 36, 37, 38, 39,
			40, 41, 42, 43, 44, 45, 46, 47,
			48, 49, 50, 51, 52, 53, 54, 55,
			56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = {{1, 7}, {9, 7}, {17, 7}, {25, 6}, {0, 0} }
};

struct nand_ecclayout nand_oob_128 = {
	.eccbytes = 64,
	.eccpos = {
			64, 65, 66, 67, 68, 69, 70, 71,
			72, 73, 74, 75, 76, 77, 78, 79,
			80, 81, 82, 83, 84, 85, 86, 86,
			88, 89, 90, 91, 92, 93, 94, 95,
			96, 97, 98, 99, 100, 101, 102, 103,
			104, 105, 106, 107, 108, 109, 110, 111,
			112, 113, 114, 115, 116, 117, 118, 119,
			120, 121, 122, 123, 124, 125, 126, 127},
	.oobfree = {{1, 7}, {9, 7}, {17, 7}, {25, 7}, {33, 7}, {41, 7}, {49, 7}, {57, 6} }
};

static bool use_randomizer = FALSE;

#ifdef DUMP_PEF
static suseconds_t Cal_timediff(struct timeval *end_time, struct timeval *start_time)
{
	struct timeval difference;

	difference.tv_sec = end_time->tv_sec - start_time->tv_sec;
	difference.tv_usec = end_time->tv_usec - start_time->tv_usec;

  /* Using while instead of if below makes the code slightly more robust. */

	while (difference.tv_usec < 0) {
		difference.tv_usec += 1000000;
		difference.tv_sec -= 1;
	}

	return 1000000LL * difference.tv_sec + difference.tv_usec;

} /* timeval_diff() */

void dump_nand_rwcount(void)
{
#if 0
	struct timeval now_time;

	do_gettimeofday(&now_time);
	if (Cal_timediff(&now_time, &g_NandLogTimer) > (500 * 1000)) {
		MSG(INIT,
			"RP: %d (%lu us) RSC: %d (%lu us) WPC: %d (%lu us) EC: %d mtd(0/512/1K/2K/3K/4K): %d %d %d %d %d %d\n ",
			g_NandPerfLog.ReadPageCount,
			g_NandPerfLog.ReadPageCount ? (g_NandPerfLog.ReadPageTotalTime /
						g_NandPerfLog.ReadPageCount) : 0,
			g_NandPerfLog.ReadSubPageCount,
			g_NandPerfLog.ReadSubPageCount ? (g_NandPerfLog.ReadSubPageTotalTime /
							  g_NandPerfLog.ReadSubPageCount) : 0,
			g_NandPerfLog.WritePageCount,
			g_NandPerfLog.WritePageCount ? (g_NandPerfLog.WritePageTotalTime /
							g_NandPerfLog.WritePageCount) : 0,
			g_NandPerfLog.EraseBlockCount, g_MtdPerfLog.read_size_0_512,
			g_MtdPerfLog.read_size_512_1K, g_MtdPerfLog.read_size_1K_2K,
			g_MtdPerfLog.read_size_2K_3K, g_MtdPerfLog.read_size_3K_4K,
			g_MtdPerfLog.read_size_Above_4K);

		memset(&g_NandPerfLog, 0x00, sizeof(g_NandPerfLog));
		memset(&g_MtdPerfLog, 0x00, sizeof(g_MtdPerfLog));
		do_gettimeofday(&g_NandLogTimer);

	}
#endif
}
#endif
void dump_nfi(void)
{
#if __DEBUG_NAND
	pr_err("~~~~Dump NFI Register in Kernel~~~~\n");
	pr_err("NFI_CNFG_REG16: 0x%x\n", DRV_Reg16(NFI_CNFG_REG16));
	pr_err("NFI_PAGEFMT_REG16: 0x%x\n", DRV_Reg16(NFI_PAGEFMT_REG16));
	pr_err("NFI_CON_REG16: 0x%x\n", DRV_Reg16(NFI_CON_REG16));
	pr_err("NFI_ACCCON_REG32: 0x%x\n", DRV_Reg32(NFI_ACCCON_REG32));
	pr_err("NFI_INTR_EN_REG16: 0x%x\n", DRV_Reg16(NFI_INTR_EN_REG16));
	pr_err("NFI_INTR_REG16: 0x%x\n", DRV_Reg16(NFI_INTR_REG16));
	pr_err("NFI_CMD_REG16: 0x%x\n", DRV_Reg16(NFI_CMD_REG16));
	pr_err("NFI_ADDRNOB_REG16: 0x%x\n", DRV_Reg16(NFI_ADDRNOB_REG16));
	pr_err("NFI_COLADDR_REG32: 0x%x\n", DRV_Reg32(NFI_COLADDR_REG32));
	pr_err("NFI_ROWADDR_REG32: 0x%x\n", DRV_Reg32(NFI_ROWADDR_REG32));
	pr_err("NFI_STRDATA_REG16: 0x%x\n", DRV_Reg16(NFI_STRDATA_REG16));
	pr_err("NFI_DATAW_REG32: 0x%x\n", DRV_Reg32(NFI_DATAW_REG32));
	pr_err("NFI_DATAR_REG32: 0x%x\n", DRV_Reg32(NFI_DATAR_REG32));
	pr_err("NFI_PIO_DIRDY_REG16: 0x%x\n", DRV_Reg16(NFI_PIO_DIRDY_REG16));
	pr_err("NFI_STA_REG32: 0x%x\n", DRV_Reg32(NFI_STA_REG32));
	pr_err("NFI_FIFOSTA_REG16: 0x%x\n", DRV_Reg16(NFI_FIFOSTA_REG16));
/*	pr_err("NFI_LOCKSTA_REG16: 0x%x\n", DRV_Reg16(NFI_LOCKSTA_REG16));*/
	pr_err("NFI_ADDRCNTR_REG16: 0x%x\n", DRV_Reg16(NFI_ADDRCNTR_REG16));
	pr_err("NFI_STRADDR_REG32: 0x%x\n", DRV_Reg32(NFI_STRADDR_REG32));
	pr_err("NFI_BYTELEN_REG16: 0x%x\n", DRV_Reg16(NFI_BYTELEN_REG16));
	pr_err("NFI_CSEL_REG16: 0x%x\n", DRV_Reg16(NFI_CSEL_REG16));
	pr_err("NFI_IOCON_REG16: 0x%x\n", DRV_Reg16(NFI_IOCON_REG16));
	pr_err("NFI_FDM0L_REG32: 0x%x\n", DRV_Reg32(NFI_FDM0L_REG32));
	pr_err("NFI_FDM0M_REG32: 0x%x\n", DRV_Reg32(NFI_FDM0M_REG32));
	pr_err("NFI_LOCK_REG16: 0x%x\n", DRV_Reg16(NFI_LOCK_REG16));
	pr_err("NFI_LOCKCON_REG32: 0x%x\n", DRV_Reg32(NFI_LOCKCON_REG32));
	pr_err("NFI_LOCKANOB_REG16: 0x%x\n", DRV_Reg16(NFI_LOCKANOB_REG16));
	pr_err("NFI_FIFODATA0_REG32: 0x%x\n", DRV_Reg32(NFI_FIFODATA0_REG32));
	pr_err("NFI_FIFODATA1_REG32: 0x%x\n", DRV_Reg32(NFI_FIFODATA1_REG32));
	pr_err("NFI_FIFODATA2_REG32: 0x%x\n", DRV_Reg32(NFI_FIFODATA2_REG32));
	pr_err("NFI_FIFODATA3_REG32: 0x%x\n", DRV_Reg32(NFI_FIFODATA3_REG32));
	pr_err("NFI_MASTERSTA_REG16: 0x%x\n", DRV_Reg16(NFI_MASTERSTA_REG16));
	pr_err("NFI_DEBUG_CON1_REG16: 0x%x\n", DRV_Reg16(NFI_DEBUG_CON1_REG16));
	pr_err("ECC_ENCCON_REG16	: %x\n", *ECC_ENCCON_REG16);
	pr_err("ECC_ENCCNFG_REG32	: %x\n", *ECC_ENCCNFG_REG32);
	pr_err("ECC_ENCDIADDR_REG32	: %x\n", *ECC_ENCDIADDR_REG32);
	pr_err("ECC_ENCIDLE_REG32	: %x\n", *ECC_ENCIDLE_REG32);
	pr_err("ECC_ENCPAR0_REG32  : %x\n", *ECC_ENCPAR0_REG32);
	pr_err("ECC_ENCPAR1_REG32  : %x\n", *ECC_ENCPAR1_REG32);
	pr_err("ECC_ENCPAR2_REG32  : %x\n", *ECC_ENCPAR2_REG32);
	pr_err("ECC_ENCPAR3_REG32  : %x\n", *ECC_ENCPAR3_REG32);
	pr_err("ECC_ENCPAR4_REG32  : %x\n", *ECC_ENCPAR4_REG32);
	pr_err("ECC_ENCPAR5_REG32  : %x\n", *ECC_ENCPAR5_REG32);
	pr_err("ECC_ENCPAR6_REG32  : %x\n", *ECC_ENCPAR6_REG32);
	pr_err("ECC_ENCSTA_REG32	: %x\n", *ECC_ENCSTA_REG32);
	pr_err("ECC_ENCIRQEN_REG16 : %x\n", *ECC_ENCIRQEN_REG16);
	pr_err("ECC_ENCIRQSTA_REG16: %x\n", *ECC_ENCIRQSTA_REG16);
	pr_err("ECC_DECCON_REG16	: %x\n", *ECC_DECCON_REG16);
	pr_err("ECC_DECCNFG_REG32  : %x\n", *ECC_DECCNFG_REG32);
	pr_err("ECC_DECDIADDR_REG32: %x\n", *ECC_DECDIADDR_REG32);
	pr_err("ECC_DECIDLE_REG16  : %x\n", *ECC_DECIDLE_REG16);
	pr_err("ECC_DECFER_REG16	: %x\n", *ECC_DECFER_REG16);
	pr_err("ECC_DECENUM0_REG32 : %x\n", *ECC_DECENUM0_REG32);
	pr_err("ECC_DECENUM1_REG32 : %x\n", *ECC_DECENUM1_REG32);
	pr_err("ECC_DECDONE_REG16  : %x\n", *ECC_DECDONE_REG16);
	pr_err("ECC_DECEL0_REG32	: %x\n", *ECC_DECEL0_REG32);
	pr_err("ECC_DECEL1_REG32	: %x\n", *ECC_DECEL1_REG32);
	pr_err("ECC_DECEL2_REG32	: %x\n", *ECC_DECEL2_REG32);
	pr_err("ECC_DECEL3_REG32	: %x\n", *ECC_DECEL3_REG32);
	pr_err("ECC_DECEL4_REG32	: %x\n", *ECC_DECEL4_REG32);
	pr_err("ECC_DECEL5_REG32	: %x\n", *ECC_DECEL5_REG32);
	pr_err("ECC_DECEL6_REG32	: %x\n", *ECC_DECEL6_REG32);
	pr_err("ECC_DECEL7_REG32	: %x\n", *ECC_DECEL7_REG32);
	pr_err("ECC_DECIRQEN_REG16 : %x\n", *ECC_DECIRQEN_REG16);
	pr_err("ECC_DECIRQSTA_REG16: %x\n", *ECC_DECIRQSTA_REG16);
	pr_err("ECC_DECFSM_REG32	: %x\n", *ECC_DECFSM_REG32);
	pr_err("ECC_BYPASS_REG32	: %x\n", *ECC_BYPASS_REG32);
#endif
}

u8 NFI_DMA_status(void)
{
	return g_running_dma;
}
EXPORT_SYMBOL(NFI_DMA_status);

u32 NFI_DMA_address(void)
{
	return DRV_Reg32(NFI_STRADDR_REG32);
}
EXPORT_SYMBOL(NFI_DMA_address);

u32 nand_virt_to_phys_add(u32 va)
{
	u32 pageOffset = (va & (PAGE_SIZE - 1));
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;
	u32 pa;

	if (virt_addr_valid(va))
		return __virt_to_phys(va);

	if (NULL == current) {
		pr_err("[nand_virt_to_phys_add] ERROR , current is NULL!\n");
		return 0;
	}

	if (NULL == current->mm) {
		pr_err
			("[nand_virt_to_phys_add] ERROR current->mm is NULL! tgid = 0x%x, name = %s\n",
			 current->tgid, current->comm);
		return 0;
	}

	pgd = pgd_offset(current->mm, va);	/* what is tsk->mm */
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		pr_err("[nand_virt_to_phys_add] ERROR, va = 0x%x, pgd invalid!\n", va);
		return 0;
	}

	pmd = pmd_offset((pud_t *) pgd, va);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		pr_err("[nand_virt_to_phys_add] ERROR, va = 0x%x, pmd invalid!\n", va);
		return 0;
	}

	pte = pte_offset_map(pmd, va);
	if (pte_present(*pte)) {
		pa = (pte_val(*pte) & (PAGE_MASK)) | pageOffset;
		return pa;
	}

	pr_err("[nand_virt_to_phys_add] ERROR va = 0x%x, pte invalid!\n", va);
	return 0;
}
EXPORT_SYMBOL(nand_virt_to_phys_add);

bool get_device_info(u8 *id, flashdev_info *gn_devinfo)
{
	u32 i, m, n, mismatch;
	int target = -1;
	u8 target_id_len = 0;
	unsigned int flash_number = sizeof(gen_FlashTable) / sizeof(gen_FlashTable[0]);

	for (i = 0; i < flash_number; i++) {
		mismatch = 0;
		for (m = 0; m < gen_FlashTable[i].id_length; m++) {
			if (id[m] != gen_FlashTable[i].id[m]) {
				mismatch = 1;
				break;
			}
		}
		if (mismatch == 0 && gen_FlashTable[i].id_length > target_id_len) {
				target = i;
				target_id_len = gen_FlashTable[i].id_length;
		}
	}

	if (target != -1) {
		MSG(INIT, "Recognize NAND: ID [");
		for (n = 0; n < gen_FlashTable[target].id_length; n++) {
			gn_devinfo->id[n] = gen_FlashTable[target].id[n];
			MSG(INIT, "%x ", gn_devinfo->id[n]);
		}
		MSG(INIT,
			"], Device Name [%s], Page Size [%d]B Spare Size [%d]B Total Size [%d]MB\n",
			gen_FlashTable[target].devciename, gen_FlashTable[target].pagesize,
			gen_FlashTable[target].sparesize, gen_FlashTable[target].totalsize);
		gn_devinfo->id_length = gen_FlashTable[target].id_length;
		gn_devinfo->blocksize = gen_FlashTable[target].blocksize;
		gn_devinfo->addr_cycle = gen_FlashTable[target].addr_cycle;
		gn_devinfo->iowidth = gen_FlashTable[target].iowidth;
		gn_devinfo->timmingsetting = gen_FlashTable[target].timmingsetting;
		gn_devinfo->advancedmode = gen_FlashTable[target].advancedmode;
		gn_devinfo->pagesize = gen_FlashTable[target].pagesize;
		gn_devinfo->sparesize = gen_FlashTable[target].sparesize;
		gn_devinfo->totalsize = gen_FlashTable[target].totalsize;
		gn_devinfo->sectorsize = gen_FlashTable[target].sectorsize;
		gn_devinfo->s_acccon = gen_FlashTable[target].s_acccon;
		gn_devinfo->s_acccon1 = gen_FlashTable[target].s_acccon1;
		gn_devinfo->freq = gen_FlashTable[target].freq;
		gn_devinfo->vendor = gen_FlashTable[target].vendor;
		gn_devinfo->dqs_delay_ctrl = gen_FlashTable[target].dqs_delay_ctrl;
		memcpy((u8 *) &gn_devinfo->feature_set,
			(u8 *) &gen_FlashTable[target].feature_set, sizeof(struct MLC_feature_set));
		memcpy(gn_devinfo->devciename, gen_FlashTable[target].devciename,
			sizeof(gn_devinfo->devciename));
		gn_devinfo->NAND_FLASH_TYPE = gen_FlashTable[target].NAND_FLASH_TYPE;
		memcpy
			((u8 *)&gn_devinfo->tlcControl,
			(u8 *)&gen_FlashTable[target].tlcControl, sizeof(struct NFI_TLC_CTRL));
		return true;
	}
	MSG(INIT, "Not Found NAND: ID [");
	for (n = 0; n < NAND_MAX_ID; n++)
			MSG(INIT, "%x ", id[n]);
		MSG(INIT, "]\n");
		return false;

	}

#ifdef DUMP_NATIVE_BACKTRACE
#define NFI_NATIVE_LOG_SD	"/sdcard/NFI_native_log_%s-%02d-%02d-%02d_%02d-%02d-%02d.log"
#define NFI_NATIVE_LOG_DATA "/data/NFI_native_log_%s-%02d-%02d-%02d_%02d-%02d-%02d.log"
static int nfi_flush_log(char *s)
{
	mm_segment_t old_fs;
	struct rtc_time tm;
	struct timeval tv = { 0 };
	struct file *filp;
	char name[256];
	unsigned int re = 0;
	int data_write = 0;

	do_gettimeofday(&tv);
	rtc_time_to_tm(tv.tv_sec, &tm);
	memset(name, 0, sizeof(name));
	sprintf(name, NFI_NATIVE_LOG_DATA, s, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open(name, O_WRONLY | O_CREAT, 0777);
	if (IS_ERR(filp)) {
		pr_err("[NFI_flush_log]error create file in %s, IS_ERR: %ld, PTR_ERR: %ld\n", name,
			IS_ERR(filp), PTR_ERR(filp));
		memset(name, 0, sizeof(name));
		sprintf(name, NFI_NATIVE_LOG_SD, s, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
		filp = filp_open(name, O_WRONLY | O_CREAT, 0777);
		if (IS_ERR(filp)) {
			pr_err
				("[NFI_flush_log]error create file in %s, IS_ERR: %ld, PTR_ERR: %ld\n",
				 name, IS_ERR(filp), PTR_ERR(filp));
			set_fs(old_fs);
			return -1;
		}
	}
	pr_err("[NFI_flush_log]log file: %s\n", name);
	set_fs(old_fs);

	if (!(filp->f_op) || !(filp->f_op->write)) {
		pr_err("[NFI_flush_log] No operation\n");
		re = -1;
		goto ClOSE_FILE;
	}

	DumpNativeInfo();
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	data_write = vfs_write(filp, (char __user *)NativeInfo, strlen(NativeInfo), &filp->f_pos);
	if (!data_write) {
		pr_err("[nfi_flush_log] write fail\n");
		re = -1;
	}
	set_fs(old_fs);

ClOSE_FILE:
	if (filp) {
		filp_close(filp, current->files);
		filp = NULL;
	}
	return re;
}
#endif

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)

void NFI_TLC_GetMappedWL(u32 pageidx, struct NFI_TLC_WL_INFO *WL_Info)
{
	WL_Info->word_line_idx = pageidx / 3;
	WL_Info->wl_pre = (enum NFI_TLC_WL_PRE)(pageidx % 3);
}

u32 NFI_TLC_GetRowAddr(u32 rowaddr)
{
	u32 real_row;
	u32 temp = 0xFF;
	int page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;

	if (gn_devinfo.tlcControl.normaltlc)
		temp = page_per_block / 3;
	else
		temp = page_per_block;

	real_row = ((rowaddr / temp) << gn_devinfo.tlcControl.block_bit) | (rowaddr % temp);

	return real_row;
}

u32 NFI_TLC_SetpPlaneAddr(u32 rowaddr, bool left_plane)
{
	u32 real_row;

	if (gn_devinfo.tlcControl.pPlaneEn) {
		if (left_plane)
			real_row = (rowaddr & (~(1 << gn_devinfo.tlcControl.pPlane_bit)));
		else
			real_row = (rowaddr | (1 << gn_devinfo.tlcControl.pPlane_bit));
	} else
			real_row = rowaddr;
	return real_row;
}

u32 NFI_TLC_GetMappedPgAddr(u32 rowaddr)
{
	u32 page_idx;
	u32 page_shift = 0;
	u32 real_row;
	int page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;

	real_row = rowaddr;
	if (gn_devinfo.tlcControl.normaltlc) {
		page_shift = gn_devinfo.tlcControl.block_bit;
		if (gn_devinfo.tlcControl.pPlaneEn)
			real_row &= (~(1 << gn_devinfo.tlcControl.pPlane_bit));

		page_idx =
			((real_row >> page_shift) * page_per_block)
			+ (((real_row << (32-page_shift)) >> (32-page_shift)) * 3);
	} else {
		page_shift = gn_devinfo.tlcControl.block_bit;
		page_idx = ((real_row >> page_shift) * page_per_block)
		+ ((real_row << (32-page_shift)) >> (32-page_shift));
	}

	return page_idx;
}
#endif

static bool mtk_nand_reset(void);

u32 mtk_nand_page_transform(struct mtd_info *mtd, struct nand_chip *chip, u32 page, u32 *blk,
				u32 *map_blk)
{
	u32 block_size = (gn_devinfo.blocksize * 1024);
	u32 page_size = (1 << chip->page_shift);
	loff_t start_address = 0;
	u32 idx;
	u32 block;
	u32 page_in_block;
	u32 mapped_block;
	bool raw_part = FALSE;
	loff_t logical_address = (loff_t)page * (1 << chip->page_shift);
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	loff_t temp, temp1;
#endif

	gn_devinfo.tlcControl.slcopmodeEn = FALSE;
	if (MLC_DEVICE && init_pmt_done == TRUE) {
		start_address = part_get_startaddress(logical_address, &idx);
		if (raw_partition(idx))
			raw_part = TRUE;
		else
			raw_part = FALSE;
	}
	if (init_pmt_done != TRUE && gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		gn_devinfo.tlcControl.slcopmodeEn = TRUE;

	if (raw_part == TRUE) {
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
			if (gn_devinfo.tlcControl.normaltlc) {
				temp = start_address;
				temp1 = logical_address - start_address;
				do_div(temp, (block_size & 0xFFFFFFFF));
				do_div(temp1, ((block_size / 3) & 0xFFFFFFFF));
				block = (u32)((u32)temp + (u32)temp1);
				/*block = (u32)((u32)(start_address / block_size) +
					(u32)((logical_address - start_address) / (block_size / 3)));*/
				page_in_block = ((u32)((logical_address - start_address) >> chip->page_shift)
					% ((mtd->erasesize / page_size) / 3));
				page_in_block *= 3;
				gn_devinfo.tlcControl.slcopmodeEn = TRUE;
			} else {
				temp = start_address;
				temp1 = logical_address - start_address;
				do_div(temp, (block_size & 0xFFFFFFFF));
				do_div(temp1, ((block_size / 3) & 0xFFFFFFFF));
				block = (u32)((u32)temp + (u32)temp1);
				page_in_block = ((u32)((logical_address - start_address) >> chip->page_shift)
					 % ((mtd->erasesize / page_size) / 3));

				if (gn_devinfo.vendor != VEND_NONE)
					page_in_block = functArray[gn_devinfo.feature_set.ptbl_idx](page_in_block);
	}
		} else
#endif
	{
		block = (u32)((u32)(start_address >> chip->phys_erase_shift)
				+ (u32)((logical_address-start_address) >> (chip->phys_erase_shift - 1)));
		page_in_block =
			((u32)((logical_address - start_address) >> chip->page_shift)
			% ((mtd->erasesize / page_size) / 2));

		if (gn_devinfo.vendor != VEND_NONE)
			page_in_block = functArray[gn_devinfo.feature_set.ptbl_idx](page_in_block);
	}

		mapped_block = get_mapping_block_index(block);

		/*MSG(INIT , "[page_in_block]mapped_block = %d, page_in_block = %d\n", mapped_block, page_in_block); */
		*blk = block;
		*map_blk = mapped_block;
	} else {
		if (((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
			|| (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER))
		&& (!mtk_block_istlc(logical_address))) {
			mtk_slc_blk_addr(logical_address, &block, &page_in_block);
			gn_devinfo.tlcControl.slcopmodeEn = TRUE;
		} else {
			block = page / (block_size / page_size);
			page_in_block = page % (block_size / page_size);
		}
		mapped_block = get_mapping_block_index(block);
		*blk = block;
		*map_blk = mapped_block;
	}
	return page_in_block;
}

bool mtk_nand_IsRawPartition(loff_t logical_address)
{
	u32 idx;

	part_get_startaddress(logical_address, &idx);
	if (raw_partition(idx))
		return true;
	else
		return false;
}

u16 randomizer_seed[128] = {
0x576A, 0x05E8, 0x629D, 0x45A3,
0x649C, 0x4BF0, 0x2342, 0x272E,
0x7358, 0x4FF3, 0x73EC, 0x5F70,
0x7A60, 0x1AD8, 0x3472, 0x3612,
0x224F, 0x0454, 0x030E, 0x70A5,
0x7809, 0x2521, 0x48F4, 0x5A2D,
0x492A, 0x043D, 0x7F61, 0x3969,
0x517A, 0x3B42, 0x769D, 0x0647,
0x7E2A, 0x1383, 0x49D9, 0x07B8,
0x2578, 0x4EEC, 0x4423, 0x352F,
0x5B22, 0x72B9, 0x367B, 0x24B6,
0x7E8E, 0x2318, 0x6BD0, 0x5519,
0x1783, 0x18A7, 0x7B6E, 0x7602,
0x4B7F, 0x3648, 0x2C53, 0x6B99,
0x0C23, 0x67CF, 0x7E0E, 0x4D8C,
0x5079, 0x209D, 0x244A, 0x747B,
0x350B, 0x0E4D, 0x7004, 0x6AC3,
0x7F3E, 0x21F5, 0x7A15, 0x2379,
0x1517, 0x1ABA, 0x4E77, 0x15A1,
0x04FA, 0x2D61, 0x253A, 0x1302,
0x1F63, 0x5AB3, 0x049A, 0x5AE8,
0x1CD7, 0x4A00, 0x30C8, 0x3247,
0x729C, 0x5034, 0x2B0E, 0x57F2,
0x00E4, 0x575B, 0x6192, 0x38F8,
0x2F6A, 0x0C14, 0x45FC, 0x41DF,
0x38DA, 0x7AE1, 0x7322, 0x62DF,
0x5E39, 0x0E64, 0x6D85, 0x5951,
0x5937, 0x6281, 0x33A1, 0x6A32,
0x3A5A, 0x2BAC, 0x743A, 0x5E74,
0x3B2E, 0x7EC7, 0x4FD2, 0x5D28,
0x751F, 0x3EF8, 0x39B1, 0x4E49,
	0x746B, 0x6EF6, 0x44BE, 0x6DB7
};

static int mtk_nand_randomizer_config(struct gRandConfig *conf, u16 seed)
{
#if 1
	if (gn_devinfo.vendor != VEND_NONE) {
		u16 nfi_cnfg = 0;
		u32 nfi_ran_cnfg = 0;
		u8 i;

		/* set up NFI_CNFG */
		nfi_cnfg = DRV_Reg16(NFI_CNFG_REG16);
		nfi_ran_cnfg = DRV_Reg32(NFI_RANDOM_CNFG_REG32);
		if (conf->type == RAND_TYPE_SAMSUNG) {
			nfi_ran_cnfg = 0;
			nfi_ran_cnfg |= seed << EN_SEED_SHIFT;
			nfi_ran_cnfg |= seed << DE_SEED_SHIFT;
			nfi_cnfg |= CNFG_RAN_SEC;
			nfi_cnfg |= CNFG_RAN_SEL;
			use_randomizer = TRUE;
			/*nfi_ran_cnfg |= 0x00010001; */
		} else if (conf->type == RAND_TYPE_TOSHIBA) {
			use_randomizer = TRUE;
			for (i = 0; i < 6; i++) {
				DRV_WriteReg32(NFI_RANDOM_ENSEED01_TS_REG32 + i, conf->seed[i]);
				DRV_WriteReg32(NFI_RANDOM_DESEED01_TS_REG32 + i, conf->seed[i]);
			}
			nfi_cnfg |= CNFG_RAN_SEC;
			nfi_cnfg &= ~CNFG_RAN_SEL;
			/*nfi_ran_cnfg |= 0x00010001; */
		} else {
			nfi_ran_cnfg &= ~0x00010001;
			use_randomizer = FALSE;
			return 0;
		}

		DRV_WriteReg16(NFI_CNFG_REG16, nfi_cnfg);
		DRV_WriteReg32(NFI_RANDOM_CNFG_REG32, nfi_ran_cnfg);
	}
#endif
	return 0;
}

static bool mtk_nand_israndomizeron(void)
{
#if 1
	if (gn_devinfo.vendor != VEND_NONE) {
		u32 nfi_ran_cnfg = 0;

		nfi_ran_cnfg = DRV_Reg32(NFI_RANDOM_CNFG_REG32);
		if (nfi_ran_cnfg & 0x00010001)
			return TRUE;
	}
#endif
	return FALSE;
}

static void mtk_nand_interface_switch(struct mtd_info *mtd)
{
	if (gn_devinfo.iowidth == IO_ONFI || gn_devinfo.iowidth == IO_TOGGLEDDR
		|| gn_devinfo.iowidth == IO_TOGGLESDR) {
		if (DDR_INTERFACE == FALSE) {
			if (mtk_nand_interface_config(mtd)) {
				MSG(INIT, "[NFI] interface switch sync!!!!\n");
				DDR_INTERFACE = TRUE;
			} else {
				MSG(INIT, "[NFI] interface switch fail!!!!\n");
				DDR_INTERFACE = FALSE;
			}
		}
	}
}


static void mtk_nand_turn_on_randomizer(struct mtd_info *mtd, struct nand_chip *chip,
					u32 page)
{
#if 1
	/*struct gRandConfig *conf = &gn_devinfo.feature_set.randConfig; */
	if (gn_devinfo.vendor != VEND_NONE) {
		u32 nfi_ran_cnfg = 0;
		u16 seed;
		u32 page_size = (1 << chip->page_shift);
		u32 page_per_blk = (mtd->erasesize / page_size);

		if (page_per_blk < 128)
			seed = randomizer_seed[page % page_per_blk];
		else
			seed = randomizer_seed[page % 128];

		mtk_nand_randomizer_config(&gn_devinfo.feature_set.randConfig, seed);
		nfi_ran_cnfg = DRV_Reg32(NFI_RANDOM_CNFG_REG32);
		nfi_ran_cnfg |= 0x00010001;
		DRV_WriteReg32(NFI_RANDOM_CNFG_REG32, nfi_ran_cnfg);
	}
#endif
}

static void mtk_nand_turn_off_randomizer(void)
{
#if 1
	if (gn_devinfo.vendor != VEND_NONE) {
		u32 nfi_ran_cnfg = 0;

		nfi_ran_cnfg = DRV_Reg32(NFI_RANDOM_CNFG_REG32);
		nfi_ran_cnfg &= ~0x00010001;
		DRV_WriteReg32(NFI_RANDOM_CNFG_REG32, nfi_ran_cnfg);
	}
#endif
}

/******************************************************************************
 * mtk_nand_irq_handler
 *
 * DESCRIPTION:
 *NAND interrupt handler!
 *
 * PARAMETERS:
 *int irq
 *void *dev_id
 *
 * RETURNS:
 *IRQ_HANDLED : Successfully handle the IRQ
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
/* Modified for TCM used */
static irqreturn_t mtk_nand_irq_handler(int irqno, void *dev_id)
{
	u16 u16IntStatus = DRV_Reg16(NFI_INTR_REG16);
	(void)irqno;

	if (u16IntStatus & (u16) INTR_AHB_DONE_EN)
		complete(&g_comp_AHB_Done);

	return IRQ_HANDLED;
}

/******************************************************************************
 * ECC_Config
 *
 * DESCRIPTION:
 *Configure HW ECC!
 *
 * PARAMETERS:
 *struct mtk_nand_host_hw *hw
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void ECC_Config(struct mtk_nand_host_hw *hw, u32 ecc_bit)
{
	u32 u4ENCODESize;
	u32 u4DECODESize;
	u32 ecc_bit_cfg = ECC_CNFG_ECC4;

	/* Sector + FDM + YAFFS2 meta data bits */
	u4DECODESize = ((hw->nand_sec_size + hw->nand_fdm_size) << 3) + ecc_bit * ECC_PARITY_BIT;

	switch (ecc_bit) {
	case 4:
		ecc_bit_cfg = ECC_CNFG_ECC4;
		break;
	case 8:
		ecc_bit_cfg = ECC_CNFG_ECC8;
		break;
	case 10:
		ecc_bit_cfg = ECC_CNFG_ECC10;
		break;
	case 12:
		ecc_bit_cfg = ECC_CNFG_ECC12;
		break;
	case 14:
		ecc_bit_cfg = ECC_CNFG_ECC14;
		break;
	case 16:
		ecc_bit_cfg = ECC_CNFG_ECC16;
		break;
	case 18:
		ecc_bit_cfg = ECC_CNFG_ECC18;
		break;
	case 20:
		ecc_bit_cfg = ECC_CNFG_ECC20;
		break;
	case 22:
		ecc_bit_cfg = ECC_CNFG_ECC22;
		break;
	case 24:
		ecc_bit_cfg = ECC_CNFG_ECC24;
		break;
	case 28:
		ecc_bit_cfg = ECC_CNFG_ECC28;
		break;
	case 32:
		ecc_bit_cfg = ECC_CNFG_ECC32;
		break;
	case 36:
		ecc_bit_cfg = ECC_CNFG_ECC36;
		break;
	case 40:
		ecc_bit_cfg = ECC_CNFG_ECC40;
		break;
	case 44:
		ecc_bit_cfg = ECC_CNFG_ECC44;
		break;
	case 48:
		ecc_bit_cfg = ECC_CNFG_ECC48;
		break;
	case 52:
		ecc_bit_cfg = ECC_CNFG_ECC52;
		break;
	case 56:
		ecc_bit_cfg = ECC_CNFG_ECC56;
		break;
	case 60:
		ecc_bit_cfg = ECC_CNFG_ECC60;
		break;
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	case 68:
		ecc_bit_cfg = ECC_CNFG_ECC68;
		u4DECODESize -= 7;
		break;
	case 72:
		ecc_bit_cfg = ECC_CNFG_ECC72;
		u4DECODESize -= 7;
		break;
	case 80:
		ecc_bit_cfg = ECC_CNFG_ECC80;
		u4DECODESize -= 7;
		break;
#endif
	default:
		break;
	}
	DRV_WriteReg16(ECC_DECCON_REG16, DEC_DE);
	do {
		;
	} while (!DRV_Reg16(ECC_DECIDLE_REG16));

	DRV_WriteReg16(ECC_ENCCON_REG16, ENC_DE);
	do {
		;
	} while (!DRV_Reg32(ECC_ENCIDLE_REG32));

	/* setup FDM register base */

	/* Sector + FDM */
	u4ENCODESize = (hw->nand_sec_size + hw->nand_fdm_size) << 3;
	/* Sector + FDM + YAFFS2 meta data bits */

	/* configure ECC decoder && encoder */
	DRV_WriteReg32(ECC_DECCNFG_REG32,
			ecc_bit_cfg | DEC_CNFG_NFI | DEC_CNFG_EMPTY_EN | (u4DECODESize <<
									 DEC_CNFG_CODE_SHIFT));

	DRV_WriteReg32(ECC_ENCCNFG_REG32,
			ecc_bit_cfg | ENC_CNFG_NFI | (u4ENCODESize << ENC_CNFG_MSG_SHIFT));
#ifndef MANUAL_CORRECT
	NFI_SET_REG32(ECC_DECCNFG_REG32, DEC_CNFG_CORRECT);
#else
	NFI_SET_REG32(ECC_DECCNFG_REG32, DEC_CNFG_EL);
#endif
}

/******************************************************************************
 * ECC_Decode_Start
 *
 * DESCRIPTION:
 *HW ECC Decode Start !
 *
 * PARAMETERS:
 *None
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void ECC_Decode_Start(void)
{
	/* wait for device returning idle */
	while (!(DRV_Reg16(ECC_DECIDLE_REG16) & DEC_IDLE))
		;
	DRV_WriteReg16(ECC_DECCON_REG16, DEC_EN);
}

/******************************************************************************
 * ECC_Decode_End
 *
 * DESCRIPTION:
 *HW ECC Decode End !
 *
 * PARAMETERS:
 *None
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void ECC_Decode_End(void)
{
	/* wait for device returning idle */
	while (!(DRV_Reg16(ECC_DECIDLE_REG16) & DEC_IDLE))
		;
	DRV_WriteReg16(ECC_DECCON_REG16, DEC_DE);
}

/******************************************************************************
 * ECC_Encode_Start
 *
 * DESCRIPTION:
 *HW ECC Encode Start !
 *
 * PARAMETERS:
 *None
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void ECC_Encode_Start(void)
{
	/* wait for device returning idle */
	while (!(DRV_Reg32(ECC_ENCIDLE_REG32) & ENC_IDLE))
		;
	mb();			/*make sure process order */
	DRV_WriteReg16(ECC_ENCCON_REG16, ENC_EN);
}

/******************************************************************************
 * ECC_Encode_End
 *
 * DESCRIPTION:
 *HW ECC Encode End !
 *
 * PARAMETERS:
 *None
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void ECC_Encode_End(void)
{
	/* wait for device returning idle */
	while (!(DRV_Reg32(ECC_ENCIDLE_REG32) & ENC_IDLE))
		;
	mb();			/*make sure process order */
	DRV_WriteReg16(ECC_ENCCON_REG16, ENC_DE);
}

/******************************************************************************
 * mtk_nand_check_bch_error
 *
 * DESCRIPTION:
 *Check BCH error or not !
 *
 * PARAMETERS:
 *struct mtd_info *mtd
 *	 u8 *pDataBuf
 *	 u32 u4SecIndex
 *	 u32 u4PageAddr
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static bool mtk_nand_check_bch_error(struct mtd_info *mtd, u8 *pDataBuf, u8 *spareBuf,
					 u32 u4SecIndex, u32 u4PageAddr, u32 *bitmap)
{
	bool ret = true;
	u16 u2SectorDoneMask = 1 << u4SecIndex;
	u32 u4ErrorNumDebug0, u4ErrorNumDebug1, i, u4ErrNum;
	u32 timeout = 0xFFFF;
	u32 correct_count = 0;
	u32 page_size = (u4SecIndex+1)*host->hw->nand_sec_size;
	u32 sec_num = u4SecIndex+1;
	u16 failed_sec = 0;
	u32 maxSectorBitErr = 0;
	u32 uncorrect_sector = 0;

#ifdef MANUAL_CORRECT
	u32 au4ErrBitLoc[6];
	u32 u4ErrByteLoc, u4BitOffset;
	u32 u4ErrBitLoc1th, u4ErrBitLoc2nd;
#endif

	while (0 == (u2SectorDoneMask & DRV_Reg16(ECC_DECDONE_REG16))) {
		timeout--;
		if (0 == timeout)
			return false;
	}
#ifndef MANUAL_CORRECT
	if (0 == (DRV_Reg32(NFI_STA_REG32) & STA_READ_EMPTY)) {
		u4ErrorNumDebug0 = DRV_Reg32(ECC_DECENUM0_REG32);
		u4ErrorNumDebug1 = DRV_Reg32(ECC_DECENUM1_REG32);
		if (0 != (u4ErrorNumDebug0 & 0xFFFFFFFF) || 0 != (u4ErrorNumDebug1 & 0xFFFFFFFF)) {
			for (i = 0; i <= u4SecIndex; ++i) {
#if 1
				u4ErrNum =
					(DRV_Reg32((ECC_DECENUM0_REG32 + (i / 4))) >> ((i % 4) * 8)) &
					ERR_NUM0;
#else
				if (i < 4)
					u4ErrNum = DRV_Reg32(ECC_DECENUM0_REG32) >> (i * 8);
				else
					u4ErrNum = DRV_Reg32(ECC_DECENUM1_REG32) >> ((i - 4) * 8);

				u4ErrNum &= ERR_NUM0;
#endif
				if (ERR_NUM0 == u4ErrNum) {
					failed_sec++;
					ret = false;
					uncorrect_sector |= (1 << i);
				} else {
					if (bitmap)
						*bitmap |= 1 << i;
					if (maxSectorBitErr < u4ErrNum)
							maxSectorBitErr = u4ErrNum;
					correct_count += u4ErrNum;
				}
			}
			mtd->ecc_stats.failed += failed_sec;
			if ((maxSectorBitErr > ecc_threshold) && (FALSE != ret)) {
				MSG(INIT,
					"ECC bit flips (0x%x) exceed eccthreshold (0x%x), u4PageAddr 0x%x\n",
					maxSectorBitErr, ecc_threshold, u4PageAddr);
				mtd->ecc_stats.corrected++;
			}
#ifdef CONFIG_MTK_NAND_BITFLIP
			if (26 > (prandom_u32() % 10000))
				mtd->ecc_stats.corrected++;
#endif
		}
	}
	if (0 != (DRV_Reg32(NFI_STA_REG32) & STA_READ_EMPTY)) {
		ret = true;
		uncorrect_sector = 0;
		memset(pDataBuf, 0xff, page_size);
		memset(spareBuf, 0xff, sec_num*host->hw->nand_fdm_size);
		maxSectorBitErr = 0;
		failed_sec = 0;
	}
#else
	/* We will manually correct the error bits in the last sector, not all the sectors of the page! */
	memset(au4ErrBitLoc, 0x0, sizeof(au4ErrBitLoc));
	u4ErrorNumDebug = DRV_Reg32(ECC_DECENUM_REG32);
	u4ErrNum =
		(DRV_Reg32((ECC_DECENUM_REG32 + (u4SecIndex / 4))) >> ((u4SecIndex % 4) * 8)) &
		ERR_NUM0;

	if (u4ErrNum) {
		if (ERR_NUM0 == u4ErrNum) {
			mtd->ecc_stats.failed++;
			ret = false;
		} else {
			for (i = 0; i < ((u4ErrNum + 1) >> 1); ++i) {
				au4ErrBitLoc[i] = DRV_Reg32(ECC_DECEL0_REG32 + i);
				u4ErrBitLoc1th = au4ErrBitLoc[i] & 0x3FFF;

				if (u4ErrBitLoc1th < 0x1000) {
					u4ErrByteLoc = u4ErrBitLoc1th / 8;
					u4BitOffset = u4ErrBitLoc1th % 8;
					pDataBuf[u4ErrByteLoc] =
						pDataBuf[u4ErrByteLoc] ^ (1 << u4BitOffset);
					mtd->ecc_stats.corrected++;
				} else {
					mtd->ecc_stats.failed++;
					uncorrect_sector |= (1 << i);
				}
				u4ErrBitLoc2nd = (au4ErrBitLoc[i] >> 16) & 0x3FFF;
				if (0 != u4ErrBitLoc2nd) {
					if (u4ErrBitLoc2nd < 0x1000) {
						u4ErrByteLoc = u4ErrBitLoc2nd / 8;
						u4BitOffset = u4ErrBitLoc2nd % 8;
						pDataBuf[u4ErrByteLoc] =
							pDataBuf[u4ErrByteLoc] ^ (1 << u4BitOffset);
						mtd->ecc_stats.corrected++;
					} else {
						mtd->ecc_stats.failed++;
						uncorrect_sector |= (1 << i);
					}
				}
			}
		}
		if (0 == (DRV_Reg16(ECC_DECFER_REG16) & (1 << u4SecIndex)))
			ret = false;
	}
#endif
	if (uncorrect_sector) {
		pr_err("UnCorrectable ECC errors at PageAddr = %d, Sectormap = 0x%x\n",
			u4PageAddr, uncorrect_sector);
	}
	return ret;
}

/******************************************************************************
 * mtk_nand_RFIFOValidSize
 *
 * DESCRIPTION:
 *Check the Read FIFO data bytes !
 *
 * PARAMETERS:
 *u16 u2Size
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static bool mtk_nand_RFIFOValidSize(u16 u2Size)
{
	u32 timeout = 0xFFFF;

	while (FIFO_RD_REMAIN(DRV_Reg16(NFI_FIFOSTA_REG16)) < u2Size) {
		timeout--;
		if (0 == timeout)
			return false;
	}
	return true;
}

/******************************************************************************
 * mtk_nand_WFIFOValidSize
 *
 * DESCRIPTION:
 *Check the Write FIFO data bytes !
 *
 * PARAMETERS:
 *u16 u2Size
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static bool mtk_nand_WFIFOValidSize(u16 u2Size)
{
	u32 timeout = 0xFFFF;

	while (FIFO_WR_REMAIN(DRV_Reg16(NFI_FIFOSTA_REG16)) > u2Size) {
		timeout--;
		if (0 == timeout)
			return false;
	}
	return true;
}

/******************************************************************************
 * mtk_nand_status_ready
 *
 * DESCRIPTION:
 *Indicate the NAND device is ready or not !
 *
 * PARAMETERS:
 *u32 u4Status
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static bool mtk_nand_status_ready(u32 u4Status)
{
	u32 timeout = 0xFFFF;

	while ((DRV_Reg32(NFI_STA_REG32) & u4Status) != 0) {
		timeout--;
		if (0 == timeout)
			return false;
	}
	return true;
}

/******************************************************************************
 * mtk_nand_reset
 *
 * DESCRIPTION:
 *Reset the NAND device hardware component !
 *
 * PARAMETERS:
 *struct mtk_nand_host *host (Initial setting data)
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static bool mtk_nand_reset(void)
{
	/* HW recommended reset flow */
	int timeout = 0xFFFF;

	if (DRV_Reg16(NFI_MASTERSTA_REG16) & 0xFFF) {
		mb();		/*make sure process order */
		DRV_WriteReg32(NFI_CON_REG16, CON_FIFO_FLUSH | CON_NFI_RST);
		while (DRV_Reg16(NFI_MASTERSTA_REG16) & 0xFFF) {
			timeout--;
			if (!timeout)
				MSG(INIT, "Wait for NFI_MASTERSTA timeout\n");
		}
	}
	/* issue reset operation */
	mb();			/*make sure process order */
	DRV_WriteReg32(NFI_CON_REG16, CON_FIFO_FLUSH | CON_NFI_RST);

	return mtk_nand_status_ready(STA_NFI_FSM_MASK | STA_NAND_BUSY) && mtk_nand_RFIFOValidSize(0)
		&& mtk_nand_WFIFOValidSize(0);
}

/******************************************************************************
 * mtk_nand_set_mode
 *
 * DESCRIPTION:
 *	Set the oepration mode !
 *
 * PARAMETERS:
 *u16 u2OpMode (read/write)
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void mtk_nand_set_mode(u16 u2OpMode)
{
	u16 u2Mode = DRV_Reg16(NFI_CNFG_REG16);

	u2Mode &= ~CNFG_OP_MODE_MASK;
	u2Mode |= u2OpMode;
	DRV_WriteReg16(NFI_CNFG_REG16, u2Mode);
}

/******************************************************************************
 * mtk_nand_set_autoformat
 *
 * DESCRIPTION:
 *	Enable/Disable hardware autoformat !
 *
 * PARAMETERS:
 *bool bEnable (Enable/Disable)
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void mtk_nand_set_autoformat(bool bEnable)
{
	if (bEnable)
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_AUTO_FMT_EN);
	else
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AUTO_FMT_EN);
}

/******************************************************************************
 * mtk_nand_configure_fdm
 *
 * DESCRIPTION:
 *Configure the FDM data size !
 *
 * PARAMETERS:
 *u16 u2FDMSize
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void mtk_nand_configure_fdm(u16 u2FDMSize)
{
	NFI_CLN_REG32(NFI_PAGEFMT_REG16, PAGEFMT_FDM_MASK | PAGEFMT_FDM_ECC_MASK);
	NFI_SET_REG32(NFI_PAGEFMT_REG16, u2FDMSize << PAGEFMT_FDM_SHIFT);
	NFI_SET_REG32(NFI_PAGEFMT_REG16, u2FDMSize << PAGEFMT_FDM_ECC_SHIFT);
}


static bool mtk_nand_pio_ready(void)
{
	int count = 0;

	while (!(DRV_Reg16(NFI_PIO_DIRDY_REG16) & 1)) {
		count++;
		if (count > 0xffff) {
			pr_err("PIO_DIRDY timeout\n");
			return false;
		}
	}

	return true;
}

/******************************************************************************
 * mtk_nand_set_command
 *
 * DESCRIPTION:
 *	Send hardware commands to NAND devices !
 *
 * PARAMETERS:
 *u16 command
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static bool mtk_nand_set_command(u16 command)
{
	/* Write command to device */
#ifdef MTK_NAND_CMD_DUMP
	if ((command == NAND_CMD_READ0) ||
		(command == NAND_CMD_SEQIN) ||
		(command == NAND_CMD_ERASE1) || (command == 0xEE) || (command == 0xEF)) {
		if (dbg_inf[current_idx].cmd.cmd_array[0] != 0xFF) {
			MSG(INIT, "Kuohong help com[0] is not 0xFF, idx: %d value: %x!\n",
				current_idx, dbg_inf[current_idx].cmd.cmd_array[0]);
			dump_stack();
		}
		dbg_inf[current_idx].cmd.cmd_array[0] = command;
	} else if ((command == NAND_CMD_READSTART) ||
		(command == NAND_CMD_PAGEPROG) || (command == NAND_CMD_ERASE2)) {
		if (dbg_inf[current_idx].cmd.cmd_array[1] != 0xFF) {
			MSG(INIT, "Kuohong help com[1] is not 0xFF!\n");
			dump_stack();
		}
		dbg_inf[current_idx].cmd.cmd_array[1] = command;
		current_idx_add();
	} else {
		dbg_inf[current_idx].cmd.cmd_array[0] = command;
		current_idx_add();
	}
#endif
	mb();			/*make sure process order */
	DRV_WriteReg16(NFI_CMD_REG16, command);
	return mtk_nand_status_ready(STA_CMD_STATE);
}

/******************************************************************************
 * mtk_nand_set_address
 *
 * DESCRIPTION:
 *	Set the hardware address register !
 *
 * PARAMETERS:
 *struct nand_chip *nand, u32 u4RowAddr
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static bool mtk_nand_set_address(u32 u4ColAddr, u32 u4RowAddr, u16 u2ColNOB, u16 u2RowNOB)
{
	/* fill cycle addr */
#ifdef MTK_NAND_CMD_DUMP
	u16 command;

	command = dbg_inf[current_idx].cmd.cmd_array[0];
	if ((command == NAND_CMD_READ0) ||
		(command == NAND_CMD_SEQIN) || (command == NAND_CMD_ERASE1)) {
		if (dbg_inf[current_idx].cmd.cmd_array[1] != 0xFF) {
			MSG(INIT, "Kuohong help com[1] is not 0xFF!\n");
			dump_stack();
		}
		dbg_inf[current_idx].cmd.address[0] = u4ColAddr;
		dbg_inf[current_idx].cmd.address[1] = u4RowAddr;
		dbg_inf[current_idx].cmd.address[2] = u2ColNOB | (u2RowNOB << ADDR_ROW_NOB_SHIFT);
	} else if ((command == 0xEE) || (command == 0xEF)) {
		dbg_inf[current_idx].cmd.address[0] = u4ColAddr;
		dbg_inf[current_idx].cmd.address[1] = u4RowAddr;
		dbg_inf[current_idx].cmd.address[2] = u2ColNOB | (u2RowNOB << ADDR_ROW_NOB_SHIFT);
		current_idx_add();
	} else {
		dbg_inf[current_idx].cmd.address[0] = u4ColAddr;
		dbg_inf[current_idx].cmd.address[1] = u4RowAddr;
		dbg_inf[current_idx].cmd.address[2] = u2ColNOB | (u2RowNOB << ADDR_ROW_NOB_SHIFT);
		current_idx_add();
	}
#endif

	mb();			/*make sure process order */
	DRV_WriteReg32(NFI_COLADDR_REG32, u4ColAddr);
	DRV_WriteReg32(NFI_ROWADDR_REG32, u4RowAddr);
	DRV_WriteReg16(NFI_ADDRNOB_REG16, u2ColNOB | (u2RowNOB << ADDR_ROW_NOB_SHIFT));
	return mtk_nand_status_ready(STA_ADDR_STATE);
}

/*-------------------------------------------------------------------------------*/
static bool mtk_nand_device_reset(void)
{
	u32 timeout = 0xFFFF;

	mtk_nand_reset();

	DRV_WriteReg16(NFI_CNFG_REG16, CNFG_OP_RESET);

	mtk_nand_set_command(NAND_CMD_RESET);

	while (!(DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY_RETURN) && (timeout--))
		;


	mtk_nand_interface_async();
	if (!timeout)
		return FALSE;
	else
		return TRUE;
}

/*-------------------------------------------------------------------------------*/

/******************************************************************************
 * mtk_nand_check_RW_count
 *
 * DESCRIPTION:
 *	Check the RW how many sectors !
 *
 * PARAMETERS:
 *u16 u2WriteSize
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static bool mtk_nand_check_RW_count(u16 u2WriteSize)
{
	u32 timeout = 0xFFFF;
	u16 u2SecNum = u2WriteSize >> host->hw->nand_sec_shift;

	while (ADDRCNTR_CNTR(DRV_Reg32(NFI_ADDRCNTR_REG16)) < u2SecNum) {
		timeout--;
		if (0 == timeout) {
			pr_debug("[%s] timeout\n", __func__);
			return false;
		}
	}
	return true;
}

int mtk_nand_interface_async(void)
{
	int retry = 10;

	if (DDR_INTERFACE == TRUE) {
		while ((DRV_Reg16(NFI_NAND_TYPE_CNFG_REG32) != 4) && retry--) {
			DRV_WriteReg16(NFI_NAND_TYPE_CNFG_REG32, 0);
			MSG(INIT, "NFI_NAND_TYPE_CNFG_REG32 0x%x\n",
				DRV_Reg16(NFI_NAND_TYPE_CNFG_REG32));
		}
		mb();		/*make sure process order */
		clkmux_sel(MT_CLKMUX_NFI1X_INFRA_SEL, MT_CG_SYS_26M, "NFI");
		mb();		/*make sure process order */
		NFI_SET_REG32(NFI_DEBUG_CON1_REG16, NFI_BYPASS);

		NFI_SET_REG32(ECC_BYPASS_REG32, ECC_BYPASS);
		DRV_WriteReg32(NFI_ACCCON_REG32, gn_devinfo.timmingsetting);

		DDR_INTERFACE = FALSE;
		MSG(INIT, "Disable DDR mode\n");
	} else
		MSG(INIT, "already legacy mode\n");
	return 0;
}

static int mtk_nand_interface_config(struct mtd_info *mtd)
{
#if 1
	u32 timeout;
	u32 val;
	int retry = 10;
	int sretry = 10;
	struct gFeatureSet *feature_set = &(gn_devinfo.feature_set.FeatureSet);
	enum cg_clk_id id = MT_CG_SYS_26M;

	if (gn_devinfo.iowidth == IO_ONFI || gn_devinfo.iowidth == IO_TOGGLEDDR
		|| gn_devinfo.iowidth == IO_TOGGLESDR) {
		do {

			if (gn_devinfo.freq == 80) {
				id = MT_CG_MPLL_D14;
				MSG(INIT, "[%s] MT_CG_MPLL_D14\n", __func__);
			} else if (gn_devinfo.freq == 100) {
				id = MT_CG_MPLL_D8;
				MSG(INIT, "[%s] MT_CG_MPLL_D8\n", __func__);
			}
/*reset*/

			mtk_nand_set_command(NAND_CMD_RESET);
			while (!(DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY_RETURN))
				;
			mtk_nand_reset();
/*set feature*/
			mtk_nand_SetFeature(mtd, (u16) feature_set->sfeatureCmd,
						feature_set->Interface.address,
						(u8 *) &feature_set->Interface.feature,
						sizeof(feature_set->Interface.feature));

			NFI_CLN_REG32(NFI_DEBUG_CON1_REG16, HWDCM_SWCON_ON);

/*setup register*/
			NFI_CLN_REG32(NFI_DEBUG_CON1_REG16, NFI_BYPASS);


			/*clear bypass of ecc */
			NFI_CLN_REG32(ECC_BYPASS_REG32, ECC_BYPASS);
			/*set infra_sel */
			disable_clock(MT_CG_NFI_SW_CG, "NFI");
			mb();	/*make sure process order */
			disable_clock(MT_CG_NFI2X_SW_CG, "NFI");
			mb();	/*make sure process order */
			clkmux_sel(MT_CLKMUX_NFI2X_GFMUX_SEL, id, "NFI");
			mb();	/*make sure process order */
			clkmux_sel(MT_CLKMUX_NFI1X_INFRA_SEL, MT_CG_SYS_TEMP, "NFI");
			mb();	/*make sure process order */
			enable_clock(MT_CG_NFI2X_SW_CG, "NFI");
			mb();	/*make sure process order */
			enable_clock(MT_CG_NFI_SW_CG, "NFI");
			/*enable_clock(MT_CG_MPLL_D7, "NFI"); */
			mb();	/*make sure process order */




			DRV_WriteReg32(NFI_DLYCTRL_REG32, 0xA001);
			val = gn_devinfo.dqs_delay_ctrl + (3 << 24);
			DRV_WriteReg32(NFI_DQS_DELAY_CTRL, val);



			if (gn_devinfo.iowidth == IO_ONFI) {
				while ((DRV_Reg16(NFI_NAND_TYPE_CNFG_REG32) != 2) && retry--) {
					DRV_WriteReg16(NFI_NAND_TYPE_CNFG_REG32, 2);
					MSG(INIT, "NFI_NAND_TYPE_CNFG_REG32 0x%x\n",
						DRV_Reg16(NFI_NAND_TYPE_CNFG_REG32));
				}
			} else {
				while ((DRV_Reg16(NFI_NAND_TYPE_CNFG_REG32) != 1) && retry--) {
					DRV_WriteReg16(NFI_NAND_TYPE_CNFG_REG32, 1);
					MSG(INIT, "NFI_NAND_TYPE_CNFG_REG32 0x%x\n",
						DRV_Reg16(NFI_NAND_TYPE_CNFG_REG32));
				}
			}

			DRV_WriteReg32(NFI_ACCCON1_REG3, gn_devinfo.s_acccon1);
			DRV_WriteReg32(NFI_ACCCON_REG32, gn_devinfo.s_acccon);

			mtk_nand_GetFeature(mtd, feature_set->gfeatureCmd,
						feature_set->Interface.address, (u8 *) &val, 4);

		} while (((val & 0xFF) != (feature_set->Interface.feature & 0xFF)) && sretry--);

		if ((val & 0xFF) != (feature_set->Interface.feature & 0xFF)) {
			MSG(INIT, "[%s] fail %d\n", __func__, val);
			mtk_nand_set_command(NAND_CMD_RESET);
			timeout = TIMEOUT_4;
			while (timeout)
				timeout--;
			mtk_nand_reset();
			DRV_WriteReg16(NFI_NAND_TYPE_CNFG_REG32, 0);
			mb();	/*make sure process order */
			disable_clock(MT_CG_NFI_SW_CG, "NFI");
			mb();	/*make sure process order */
			clkmux_sel(MT_CLKMUX_NFI1X_INFRA_SEL, MT_CG_SYS_26M, "NFI");
			mb();	/*make sure process order */
			enable_clock(MT_CG_NFI_SW_CG, "NFI");
			mb();	/*make sure process order */
			NFI_SET_REG32(NFI_DEBUG_CON1_REG16, NFI_BYPASS);
			NFI_SET_REG32(ECC_BYPASS_REG32, ECC_BYPASS);
			DRV_WriteReg32(NFI_ACCCON_REG32, gn_devinfo.timmingsetting);

			return 0;
		}
		MSG(INIT, "[%s] success\n", __func__);

	} else {
		MSG(INIT, "[%s] legacy interface\n", __func__);
	}
#endif	 /*TODO*/
		return 1;
}

/******************************************************************************
 * mtk_nand_ready_for_read
 *
 * DESCRIPTION:
 *	Prepare hardware environment for read !
 *
 * PARAMETERS:
 *struct nand_chip *nand, u32 u4RowAddr
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static bool mtk_nand_ready_for_read(struct nand_chip *nand, u32 u4RowAddr, u32 u4ColAddr,
					u16 sec_num, bool full, u8 *buf, enum readCommand cmd)
{
	/* Reset NFI HW internal state machine and flush NFI in/out FIFO */
	bool bRet = false;
	/*u16 sec_num = 1 << (nand->page_shift - host->hw->nand_sec_shift); */
	u32 col_addr = u4ColAddr;
	u32 colnob = 2, rownob = gn_devinfo.addr_cycle - 2;
	/*u32 reg_val = DRV_Reg32(NFI_MASTERRST_REG32); */
#ifndef CONFIG_MTK_TLC_NAND_SUPPORT
	u32 phys = 0;
#endif
#ifdef DUMP_PEF
	struct timeval stimer, etimer;

	do_gettimeofday(&stimer);
#endif
#ifndef CONFIG_MTK_TLC_NAND_SUPPORT
	if (full) {
		mtk_dir = DMA_FROM_DEVICE;
		sg_init_one(&mtk_sg, buf, (sec_num * (1 << host->hw->nand_sec_shift)));
		dma_map_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
		phys = mtk_sg.dma_address;
	}
#endif

	if (DRV_Reg32(NFI_NAND_TYPE_CNFG_REG32) & 0x3) {
		NFI_SET_REG16(NFI_MASTERRST_REG32, PAD_MACRO_RST);	/*reset */
		NFI_CLN_REG16(NFI_MASTERRST_REG32, PAD_MACRO_RST);	/*dereset */
	}

	if (nand->options & NAND_BUSWIDTH_16)
		col_addr /= 2;

	if (!mtk_nand_reset())
		goto cleanup;

	if (g_bHwEcc) {
		/* Enable HW ECC */
		NFI_SET_REG32(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
	} else
		NFI_CLN_REG32(NFI_CNFG_REG16, CNFG_HW_ECC_EN);

	mtk_nand_set_mode(CNFG_OP_READ);
	NFI_SET_REG16(NFI_CNFG_REG16, CNFG_READ_EN);
#ifndef CONFIG_MTK_TLC_NAND_SUPPORT
	DRV_WriteReg32(NFI_CON_REG16, sec_num << CON_NFI_SEC_SHIFT);
#endif

	if (full) {
#if __INTERNAL_USE_AHB_MODE__
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_AHB);
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
		/* phys = nand_virt_to_phys_add((u32) buf); */
#ifndef CONFIG_MTK_TLC_NAND_SUPPORT
		if (!phys) {
			pr_err
				("[mtk_nand_ready_for_read]convert virt addr (%x) to phys add (%x)fail!!!",
				 (u32) buf, phys);
			return false;
		}
		DRV_WriteReg32(NFI_STRADDR_REG32, phys);
#endif
#else
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);
#endif

		if (g_bHwEcc)
			NFI_SET_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
		else
			NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);

	} else {
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);
	}

	mtk_nand_set_autoformat(full);
#ifndef CONFIG_MTK_TLC_NAND_SUPPORT
	if (full) {
		if (g_bHwEcc)
			ECC_Decode_Start();
	}
#endif
	if (cmd == AD_CACHE_FINAL) {
		if (!mtk_nand_set_command(0x3F))
			goto cleanup;
		if (!mtk_nand_status_ready(STA_NAND_BUSY))
			goto cleanup;
		return true;
	}

	if (!mtk_nand_set_command(NAND_CMD_READ0))
		goto cleanup;

	if (!mtk_nand_set_address(col_addr, u4RowAddr, colnob, rownob))
		goto cleanup;

	if (cmd == NORMAL_READ) {
		if (!mtk_nand_set_command(NAND_CMD_READSTART))
			goto cleanup;
	} else {
		if (!mtk_nand_set_command(0x31))
			goto cleanup;
	}

	if (!mtk_nand_status_ready(STA_NAND_BUSY))
		goto cleanup;

	bRet = true;

cleanup:
#ifdef DUMP_PEF
	do_gettimeofday(&etimer);
	  g_NandPerfLog.ReadBusyTotalTime += Cal_timediff(&etimer, &stimer);
	  g_NandPerfLog.ReadBusyCount++;
#endif
	return bRet;
}

/******************************************************************************
 * mtk_nand_ready_for_write
 *
 * DESCRIPTION:
 *	Prepare hardware environment for write !
 *
 * PARAMETERS:
 *struct nand_chip *nand, u32 u4RowAddr
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static bool mtk_nand_ready_for_write(struct nand_chip *nand, u32 u4RowAddr, u32 col_addr, bool full,
					 u8 *buf)
{
	bool bRet = false;
	u32 sec_num = 1 << (nand->page_shift - host->hw->nand_sec_shift);
	u32 colnob = 2, rownob = gn_devinfo.addr_cycle - 2;
#if __INTERNAL_USE_AHB_MODE__
	u32 phys = 0;
	/*u32 T_phys = 0; */
#endif
	u32 temp_sec_num;

	temp_sec_num = sec_num;

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		&& gn_devinfo.tlcControl.normaltlc
		&& gn_devinfo.tlcControl.pPlaneEn) {

		temp_sec_num = sec_num / 2;
	}
#endif

	if (full) {
		mtk_dir = DMA_TO_DEVICE;
		sg_init_one(&mtk_sg, buf, temp_sec_num * (1 << host->hw->nand_sec_shift));
		dma_map_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
		phys = mtk_sg.dma_address;
	}

	if (nand->options & NAND_BUSWIDTH_16)
		col_addr /= 2;

	/* Reset NFI HW internal state machine and flush NFI in/out FIFO */
	if (!mtk_nand_reset())
		return false;

	mtk_nand_set_mode(CNFG_OP_PRGM);

	NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_READ_EN);

	DRV_WriteReg32(NFI_CON_REG16, temp_sec_num << CON_NFI_SEC_SHIFT);

	if (full) {
#if __INTERNAL_USE_AHB_MODE__
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_AHB);
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
		/*phys = nand_virt_to_phys_add((unsigned long) buf);*/
		/*T_phys = __virt_to_phys(buf); */
		if (!phys) {
			pr_err
				("[mt65xx_nand_ready_for_write]convert virt addr (%x) to phys add fail!!!",
				 (u32) buf);
			return false;
		}
		DRV_WriteReg32(NFI_STRADDR_REG32, phys);
#else
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);
#endif
		if (g_bHwEcc)
			NFI_SET_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
		else
			NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
	} else {
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);
	}

	mtk_nand_set_autoformat(full);

	if (full) {
		if (g_bHwEcc)
			ECC_Encode_Start();
	}

	if (!mtk_nand_set_command(NAND_CMD_SEQIN))
		goto cleanup;

	/*1 FIXED ME: For Any Kind of AddrCycle */
	if (!mtk_nand_set_address(col_addr, u4RowAddr, colnob, rownob))
		goto cleanup;

	if (!mtk_nand_status_ready(STA_NAND_BUSY))
		goto cleanup;

	bRet = true;
cleanup:

	return bRet;
}

static bool mtk_nand_check_dececc_done(u32 u4SecNum)
{
	struct timeval timer_timeout, timer_cur;
	u32 dec_mask;

	do_gettimeofday(&timer_timeout);

	timer_timeout.tv_usec += 800 * 1000;	/* 500ms */
	if (timer_timeout.tv_usec >= 1000000) {
		timer_timeout.tv_usec -= 1000000;
		timer_timeout.tv_sec += 1;
	}

	dec_mask = (1 << (u4SecNum - 1));
	while (dec_mask != (DRV_Reg16(ECC_DECDONE_REG16) & dec_mask)) {
		do_gettimeofday(&timer_cur);
		if (timeval_compare(&timer_cur, &timer_timeout) >= 0) {
			MSG(INIT, "ECC_DECDONE: timeout 0x%x %d time sec %d, usec %d\n",
				DRV_Reg16(ECC_DECDONE_REG16), u4SecNum,
				(int)(timer_cur.tv_sec - timer_timeout.tv_sec),
				(int)(timer_cur.tv_usec - timer_timeout.tv_usec));
					dump_nfi();
			if (dec_mask == (DRV_Reg16(ECC_DECDONE_REG16) & dec_mask)) {
				MSG(INIT, "ECC_DECDONE: timeout but finish job\n");
				break;
			}
			return false;
		  }
	  }
	while (DRV_Reg32(ECC_DECFSM_REG32) != ECC_DECFSM_IDLE) {
		do_gettimeofday(&timer_cur);
		if (timeval_compare(&timer_cur, &timer_timeout) >= 0) {
			MSG(INIT,
				"ECC_DECDONE: timeout ECC_DECFSM_REG32 0x%x 0x%x %d  time sec %d, usec %d\n",
				DRV_Reg16(ECC_DECFSM_REG32), DRV_Reg16(ECC_DECDONE_REG16), u4SecNum,
				(int)(timer_cur.tv_sec - timer_timeout.tv_sec),
				(int)(timer_cur.tv_usec - timer_timeout.tv_usec));
			dump_nfi();
			if (DRV_Reg32(ECC_DECFSM_REG32) == ECC_DECFSM_IDLE) {
				MSG(INIT, "ECC_DECDONE: timeout but finish job\n");
				break;
			}
			return false;
		}
	}
	return true;
}

/******************************************************************************
 * mtk_nand_read_page_data
 *
 * DESCRIPTION:
 *Fill the page data into buffer !
 *
 * PARAMETERS:
 *u8 *pDataBuf, u32 u4Size
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static bool mtk_nand_dma_read_data(struct mtd_info *mtd, u8 *buf, u32 length)
{
	int interrupt_en = g_i4Interrupt;
	int timeout = 0xfffff;
	struct scatterlist sg;
	enum dma_data_direction dir = DMA_FROM_DEVICE;
#ifdef DUMP_PEF
	struct timeval stimer, etimer;

	do_gettimeofday(&stimer);
#endif
	sg_init_one(&sg, buf, length);
	dma_map_sg(&(mtd->dev), &sg, 1, dir);

	NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);
	/* DRV_WriteReg32(NFI_STRADDR_REG32, __virt_to_phys(pDataBuf)); */

	if ((unsigned int)buf % 16) {
		pr_debug("Un-16-aligned address\n");
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
	} else {
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
	}

	NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);

	DRV_Reg16(NFI_INTR_REG16);
	DRV_WriteReg16(NFI_INTR_EN_REG16, INTR_AHB_DONE_EN);

	if (interrupt_en)
		init_completion(&g_comp_AHB_Done);

	/*dmac_inv_range(pDataBuf, pDataBuf + u4Size); */
	mb();			/*make sure process order */
	NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BRD);
	g_running_dma = 1;

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
	&& (gn_devinfo.tlcControl.needchangecolumn))
		DRV_WriteReg16(NFI_TLC_RD_WHR2_REG16, (TLC_RD_WHR2_EN | 0x055));
#endif

	if (interrupt_en) {
		/* Wait 10ms for AHB done */
		if (!wait_for_completion_timeout(&g_comp_AHB_Done, 0xFFF)) {
			MSG(INIT, "wait for completion timeout happened @ [%s]: %d\n", __func__,
				__LINE__);
			dump_nfi();
			g_running_dma = 0;
			return false;
		}
		g_running_dma = 0;
		while ((length >> host->hw->nand_sec_shift) >
			((DRV_Reg32(NFI_BYTELEN_REG16) & 0x1f000) >> 12)) {
			timeout--;
			if (0 == timeout) {
				pr_err("[%s] poll BYTELEN error\n", __func__);
				g_running_dma = 0;
				return false;	/*4 AHB Mode Time Out! */
			}
		}
	} else {
		while (!DRV_Reg16(NFI_INTR_REG16)) {
			timeout--;
			if (0 == timeout) {
				pr_err("[%s] poll nfi_intr error\n", __func__);
				dump_nfi();
				g_running_dma = 0;
				return false;	/*4 AHB Mode Time Out! */
			}
		}
		g_running_dma = 0;
		while ((length >> host->hw->nand_sec_shift) >
			((DRV_Reg32(NFI_BYTELEN_REG16) & 0x1f000) >> 12)) {
			timeout--;
			if (0 == timeout) {
				pr_err("[%s] poll BYTELEN error\n", __func__);
				dump_nfi();
				g_running_dma = 0;
				return false;	/*4 AHB Mode Time Out! */
			}
		}
	}

	dma_unmap_sg(&(mtd->dev), &sg, 1, dir);
#ifdef DUMP_PEF
	do_gettimeofday(&etimer);
	g_NandPerfLog.ReadDMATotalTime += Cal_timediff(&etimer, &stimer);
	g_NandPerfLog.ReadDMACount++;
#endif
	return true;
}

static bool mtk_nand_mcu_read_data(u8 *buf, u32 length)
{
	int timeout = 0xffff;
	u32 i;
	u32 *buf32 = (u32 *) buf;
#ifdef TESTTIME
	unsigned long long time1, time2;

	time1 = sched_clock();
#endif
	if ((u32) buf % 4 || length % 4)
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);
	else
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);

	/*DRV_WriteReg32(NFI_STRADDR_REG32, 0); */
	mb();			/*make sure process order */
	NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BRD);

	if ((u32) buf % 4 || length % 4) {
		for (i = 0; (i < (length)) && (timeout > 0);) {
			/*if (FIFO_RD_REMAIN(DRV_Reg16(NFI_FIFOSTA_REG16)) >= 4) */
			if (DRV_Reg16(NFI_PIO_DIRDY_REG16) & 1) {
				*buf++ = (u8) DRV_Reg32(NFI_DATAR_REG32);
				i++;
			} else {
				timeout--;
			}
			if (0 == timeout) {
				pr_err("[%s] timeout\n", __func__);
				dump_nfi();
				return false;
			}
		}
	} else {
		for (i = 0; (i < (length >> 2)) && (timeout > 0);) {
			/*if (FIFO_RD_REMAIN(DRV_Reg16(NFI_FIFOSTA_REG16)) >= 4) */
			if (DRV_Reg16(NFI_PIO_DIRDY_REG16) & 1) {
				*buf32++ = DRV_Reg32(NFI_DATAR_REG32);
				i++;
			} else {
				timeout--;
			}
			if (0 == timeout) {
				pr_err("[%s] timeout\n", __func__);
				dump_nfi();
				return false;
			}
		}
	}
#ifdef TESTTIME
	time2 = sched_clock() - time1;
	if (!readdatatime)
		readdatatime = (time2);

#endif
	return true;
}

static bool mtk_nand_read_page_data(struct mtd_info *mtd, u8 *pDataBuf, u32 u4Size)
{
#if (__INTERNAL_USE_AHB_MODE__)
	return mtk_nand_dma_read_data(mtd, pDataBuf, u4Size);
#else
	return mtk_nand_mcu_read_data(mtd, pDataBuf, u4Size);
#endif
}

/******************************************************************************
 * mtk_nand_write_page_data
 *
 * DESCRIPTION:
 *Fill the page data into buffer !
 *
 * PARAMETERS:
 *u8 *pDataBuf, u32 u4Size
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static bool mtk_nand_dma_write_data(struct mtd_info *mtd, u8 *pDataBuf, u32 u4Size)
{
	int i4Interrupt = 0;	/*g_i4Interrupt; */
	u32 timeout = 0xFFFF;
	struct scatterlist sg;
	enum dma_data_direction dir = DMA_TO_DEVICE;
#ifdef DUMP_PEF
	struct timeval stimer, etimer;

	do_gettimeofday(&stimer);
#endif
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	u32 reg_val;
#endif

	NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);
	DRV_Reg16(NFI_INTR_REG16);
	DRV_WriteReg16(NFI_INTR_EN_REG16, 0);
	/* DRV_WriteReg32(NFI_STRADDR_REG32, (u32 *)virt_to_phys(pDataBuf)); */

	if ((unsigned int)pDataBuf % 16) {	/* TODO: can not use AHB mode here */
		pr_debug("Un-16-aligned address\n");
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
	} else {
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
	}

	NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);

	if (i4Interrupt) {
		init_completion(&g_comp_AHB_Done);
		DRV_Reg16(NFI_INTR_REG16);
		DRV_WriteReg16(NFI_INTR_EN_REG16, INTR_AHB_DONE_EN);
	}


#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
		reg_val = DRV_Reg16(NFI_DEBUG_CON1_REG16);
		reg_val |= 0x4000;
		DRV_WriteReg16(NFI_DEBUG_CON1_REG16, reg_val);
	}
#endif

	/*dmac_clean_range(pDataBuf, pDataBuf + u4Size); */
	mb();			/*make sure process order */
	NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BWR);
	g_running_dma = 3;
	if (i4Interrupt) {
		/* Wait 10ms for AHB done */
		if (!wait_for_completion_timeout(&g_comp_AHB_Done, 10)) {
			MSG(READ, "wait for completion timeout happened @ [%s]: %d\n", __func__,
				__LINE__);
			dump_nfi();
			g_running_dma = 0;
			return false;
		}
		g_running_dma = 0;
		/* wait_for_completion(&g_comp_AHB_Done); */
	} else {
		while ((u4Size >> host->hw->nand_sec_shift) >
			((DRV_Reg32(NFI_BYTELEN_REG16) & 0x1f000) >> 12)) {
			timeout--;
			if (0 == timeout) {
				pr_err("[%s] poll BYTELEN error\n", __func__);
				g_running_dma = 0;
				return false;	/*4 AHB Mode Time Out! */
			}
		}
		g_running_dma = 0;
	}

	dma_unmap_sg(&(mtd->dev), &sg, 1, dir);
#ifdef DUMP_PEF
	do_gettimeofday(&etimer);
	g_NandPerfLog.WriteDMATotalTime += Cal_timediff(&etimer, &stimer);
	g_NandPerfLog.WriteDMACount++;
#endif
	return true;
}

static bool mtk_nand_mcu_write_data(struct mtd_info *mtd, const u8 *buf, u32 length)
{
	u32 timeout = 0xFFFF;
	u32 i;
	u32 *pBuf32;

	NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);
	mb();			/*make sure process order */
	NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BWR);
	pBuf32 = (u32 *) buf;

	if ((u32) buf % 4 || length % 4)
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);
	else
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);

	if ((u32) buf % 4 || length % 4) {
		for (i = 0; (i < (length)) && (timeout > 0);) {
			if (DRV_Reg16(NFI_PIO_DIRDY_REG16) & 1) {
				DRV_WriteReg32(NFI_DATAW_REG32, *buf++);
				i++;
			} else {
				timeout--;
			}
			if (0 == timeout) {
				pr_err("[%s] timeout\n", __func__);
				dump_nfi();
				return false;
			}
		}
	} else {
		for (i = 0; (i < (length >> 2)) && (timeout > 0);) {
			/* if (FIFO_WR_REMAIN(DRV_Reg16(NFI_FIFOSTA_REG16)) <= 12) */
			if (DRV_Reg16(NFI_PIO_DIRDY_REG16) & 1) {
				DRV_WriteReg32(NFI_DATAW_REG32, *pBuf32++);
				i++;
			} else {
				timeout--;
			}
			if (0 == timeout) {
				pr_err("[%s] timeout\n", __func__);
				dump_nfi();
				return false;
			}
		}
	}

	return true;
}

static bool mtk_nand_write_page_data(struct mtd_info *mtd, u8 *buf, u32 size)
{
#if (__INTERNAL_USE_AHB_MODE__)
	return mtk_nand_dma_write_data(mtd, buf, size);
#else
	return mtk_nand_mcu_write_data(mtd, buf, size);
#endif
}

/******************************************************************************
 * mtk_nand_read_fdm_data
 *
 * DESCRIPTION:
 *Read a fdm data !
 *
 * PARAMETERS:
 *u8 *pDataBuf, u32 u4SecNum
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void mtk_nand_read_fdm_data(u8 *pDataBuf, u32 u4SecNum)
{
#ifndef CONFIG_MTK_TLC_NAND_SUPPORT
	u32 i;
	u32 *pBuf32 = (u32 *) pDataBuf;

	if (pBuf32) {
		for (i = 0; i < u4SecNum; ++i) {
			*pBuf32++ = DRV_Reg32(NFI_FDM0L_REG32 + (i << 1));
			*pBuf32++ = DRV_Reg32(NFI_FDM0M_REG32 + (i << 1));
		}
	}
#else
	u32 fdm_temp[2];
	u32 i, j;
	u8 *byte_ptr;

	byte_ptr = (u8 *)fdm_temp;
	if (pDataBuf) {
		for (i = 0; i < u4SecNum; ++i) {
			fdm_temp[0] = DRV_Reg32(NFI_FDM0L_REG32 + (i << 1));
			fdm_temp[1] = DRV_Reg32(NFI_FDM0M_REG32 + (i << 1));
			for (j = 0; j < host->hw->nand_fdm_size; j++)
				*(pDataBuf + (i * host->hw->nand_fdm_size) + j) = *(byte_ptr + j);
		}
	}
#endif
}

/******************************************************************************
 * mtk_nand_write_fdm_data
 *
 * DESCRIPTION:
 *Write a fdm data !
 *
 * PARAMETERS:
 *u8 *pDataBuf, u32 u4SecNum
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static u8 fdm_buf[128];
static void mtk_nand_write_fdm_data(struct nand_chip *chip, u8 *pDataBuf, u32 u4SecNum)
{
	u32 i, j;
	u8 checksum = 0;
	bool empty = true;
	struct nand_oobfree *free_entry;
	u8 *pBuf;
	u8 *byte_ptr;
	u32 fdm_data[2];

	memcpy(fdm_buf, pDataBuf, u4SecNum * host->hw->nand_fdm_size);

	free_entry = chip->ecc.layout->oobfree;
	for (i = 0; i < MTD_MAX_OOBFREE_ENTRIES && free_entry[i].length; i++) {
		for (j = 0; j < free_entry[i].length; j++) {
			if (pDataBuf[free_entry[i].offset + j] != 0xFF)
				empty = false;
			checksum ^= pDataBuf[free_entry[i].offset + j];
		}
	}

	if (!empty)
		fdm_buf[free_entry[i - 1].offset + free_entry[i - 1].length] = checksum;

	pBuf = (u8 *)fdm_data;
	byte_ptr = (u8 *)fdm_buf;

	for (i = 0; i < u4SecNum; ++i) {
		fdm_data[0] = 0xFFFFFFFF;
		fdm_data[1] = 0xFFFFFFFF;

		for (j = 0; j < host->hw->nand_fdm_size; j++)
			*(pBuf + j) = *(byte_ptr + j + (i * host->hw->nand_fdm_size));

		DRV_WriteReg32(NFI_FDM0L_REG32 + (i << 1), fdm_data[0]);
		DRV_WriteReg32(NFI_FDM0M_REG32 + (i << 1), fdm_data[1]);
	}
}

/******************************************************************************
 * mtk_nand_stop_read
 *
 * DESCRIPTION:
 *Stop read operation !
 *
 * PARAMETERS:
 *None
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void mtk_nand_stop_read(void)
{
	NFI_CLN_REG32(NFI_CON_REG16, CON_NFI_BRD);
	mtk_nand_reset();
	if (g_bHwEcc)
		ECC_Decode_End();

	DRV_WriteReg16(NFI_INTR_EN_REG16, 0);
}

/******************************************************************************
 * mtk_nand_stop_write
 *
 * DESCRIPTION:
 *Stop write operation !
 *
 * PARAMETERS:
 *None
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void mtk_nand_stop_write(void)
{
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	u32 reg_val;
#endif

	NFI_CLN_REG32(NFI_CON_REG16, CON_NFI_BWR);
	if (g_bHwEcc)
		ECC_Encode_End();

	DRV_WriteReg16(NFI_INTR_EN_REG16, 0);

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
		reg_val = DRV_Reg16(NFI_DEBUG_CON1_REG16);
		reg_val &= (~0x4000);
		DRV_WriteReg16(NFI_DEBUG_CON1_REG16, reg_val);
}
#endif
}

/*---------------------------------------------------------------------------*/
#define STATUS_READY			(0x40)
#define STATUS_FAIL				(0x01)
#define STATUS_WR_ALLOW			(0x80)
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)

static bool mtk_nand_read_status(void)
{
	int status = 0;
	unsigned int timeout;

	mtk_nand_reset();

	/* Disable HW ECC */
	NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);

	/* Disable 16-bit I/O */
	NFI_CLN_REG32(NFI_PAGEFMT_REG16, PAGEFMT_DBYTE_EN);
	NFI_SET_REG16(NFI_CNFG_REG16, CNFG_OP_SRD | CNFG_READ_EN | CNFG_BYTE_RW);

	DRV_WriteReg32(NFI_CON_REG16, CON_NFI_SRD | (1 << CON_NFI_NOB_SHIFT));

	DRV_WriteReg32(NFI_CON_REG16, 0x3);
	mtk_nand_set_mode(CNFG_OP_SRD);
	DRV_WriteReg16(NFI_CNFG_REG16, 0x2042);
	mtk_nand_set_command(NAND_CMD_STATUS);
	DRV_WriteReg32(NFI_CON_REG16, 0x90);

	timeout = TIMEOUT_4;
	WAIT_NFI_PIO_READY(timeout);

	if (timeout)
		status = (DRV_Reg16(NFI_DATAR_REG32));

	/*~  clear NOB */
	DRV_WriteReg32(NFI_CON_REG16, 0);

	if (gn_devinfo.iowidth == 16) {
		NFI_SET_REG32(NFI_PAGEFMT_REG16, PAGEFMT_DBYTE_EN);
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);
	}

	/* flash is ready now, check status code */
	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
			&& (gn_devinfo.tlcControl.slcopmodeEn)) {
		/*pr_warn("status 0x%x", status);*/
		if (SLC_MODE_OP_FALI & status) {
			if (!(STATUS_WR_ALLOW & status))
				MSG(INIT, "status locked\n");
			else
				MSG(INIT, "status unknown\n");

			return FALSE;
		}
		return TRUE;
	}
	if (STATUS_FAIL & status)
		return FALSE;
	return TRUE;
}
#endif

bool mtk_nand_SetFeature(struct mtd_info *mtd, u16 cmd, u32 addr, u8 *value,  u8 bytes)
{
	u16 reg_val = 0;
	u8 write_count = 0;
	u32		reg = 0;
	u32 timeout = TIMEOUT_3;

	mtk_nand_reset();

	reg = DRV_Reg32(NFI_NAND_TYPE_CNFG_REG32);
	if (!(reg&TYPE_SLC))
		bytes <<= 1;

	reg_val |= (CNFG_OP_CUST | CNFG_BYTE_RW);
	DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

	mtk_nand_set_command(cmd);
	mtk_nand_set_address(addr, 0, 1, 0);
	mtk_nand_status_ready(STA_NFI_OP_MASK);
	DRV_WriteReg32(NFI_CON_REG16, 1 << CON_NFI_SEC_SHIFT);
	NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BWR);
	DRV_WriteReg16(NFI_STRDATA_REG16, 0x1);
	while ((write_count < bytes) && timeout) {
		WAIT_NFI_PIO_READY(timeout);
		if (timeout == 0)
			break;
		if (reg&TYPE_SLC)
			DRV_WriteReg8(NFI_DATAW_REG32, *value++);
		else if (write_count % 2)
			DRV_WriteReg8(NFI_DATAW_REG32, *value++);
		else
			DRV_WriteReg8(NFI_DATAW_REG32, *value);
		write_count++;
		timeout = TIMEOUT_3;
	}

	*NFI_CNRNB_REG16 = 0x81;
	if (!mtk_nand_status_ready(STA_NAND_BUSY_RETURN))
		return FALSE;

	return TRUE;
}

bool mtk_nand_GetFeature(struct mtd_info *mtd, u16 cmd, u32 addr, u8 *value,  u8 bytes)
{
	u16 reg_val = 0;
	u8 read_count = 0;
	u32 timeout = TIMEOUT_3;

	mtk_nand_reset();

	reg_val |= (CNFG_OP_CUST | CNFG_BYTE_RW | CNFG_READ_EN);
	DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

	mtk_nand_set_command(cmd);
	mtk_nand_set_address(addr, 0, 1, 0);

	mtk_nand_status_ready(STA_NFI_OP_MASK);
	*NFI_CNRNB_REG16 = 0x81;
	mtk_nand_status_ready(STA_NAND_BUSY_RETURN);

	reg_val = DRV_Reg32(NFI_CON_REG16);
	reg_val &= ~CON_NFI_NOB_MASK;
	reg_val |= ((4 << CON_NFI_NOB_SHIFT)|CON_NFI_SRD);
	DRV_WriteReg32(NFI_CON_REG16, reg_val);
	DRV_WriteReg16(NFI_STRDATA_REG16, 0x1);
	while ((read_count < bytes) && timeout) {
		WAIT_NFI_PIO_READY(timeout);
		if (timeout == 0)
			break;

		*value++ = DRV_Reg8(NFI_DATAR_REG32);
		read_count++;
		timeout = TIMEOUT_3;
	}
	if (timeout != 0)
		return TRUE;
	else
		return FALSE;

}


#if 1
const u8 data_tbl[8][5] = {
	{0x04, 0x04, 0x7C, 0x7E, 0x00},
	{0x00, 0x7C, 0x78, 0x78, 0x00},
	{0x7C, 0x76, 0x74, 0x72, 0x00},
	{0x08, 0x08, 0x00, 0x00, 0x00},
	{0x0B, 0x7E, 0x76, 0x74, 0x00},
	{0x10, 0x76, 0x72, 0x70, 0x00},
	{0x02, 0x7C, 0x7E, 0x70, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00}
};

static void mtk_nand_modeentry_rrtry(void)
{
	mtk_nand_reset();

	mtk_nand_set_mode(CNFG_OP_CUST);

	mtk_nand_set_command(0x5C);
	mtk_nand_set_command(0xC5);

	mtk_nand_status_ready(STA_NFI_OP_MASK);
}

static void mtk_nand_rren_rrtry(bool needB3)
{
	mtk_nand_reset();

	mtk_nand_set_mode(CNFG_OP_CUST);

	if (needB3)
		mtk_nand_set_command(0xB3);
	mtk_nand_set_command(0x26);
	mtk_nand_set_command(0x5D);

	mtk_nand_status_ready(STA_NFI_OP_MASK);
}

static void mtk_nand_sprmset_rrtry(u32 addr, u32 data)
{
	u16 reg_val = 0;
	u32 timeout = TIMEOUT_3;	/*0xffff; */

	mtk_nand_reset();

	reg_val |= (CNFG_OP_CUST | CNFG_BYTE_RW);
	DRV_WriteReg16(NFI_CNFG_REG16, reg_val);
	mtk_nand_set_command(0x55);
	mtk_nand_set_address(addr, 0, 1, 0);
	mtk_nand_status_ready(STA_NFI_OP_MASK);
	DRV_WriteReg32(NFI_CON_REG16, 1 << CON_NFI_SEC_SHIFT);
	NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BWR);
	DRV_WriteReg16(NFI_STRDATA_REG16, 0x1);
	WAIT_NFI_PIO_READY(timeout);
	timeout = TIMEOUT_3;
	DRV_WriteReg8(NFI_DATAW_REG32, data);

	while (!(DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY_RETURN) && (timeout--))
		;
}

static void mtk_nand_toshiba_rrtry(struct mtd_info *mtd, flashdev_info deviceinfo, u32 retryCount,
				bool defValue)
{
	u32 acccon;
	u8 cnt = 0;
	u8 add_reg[6] = {0x04, 0x05, 0x06, 0x07, 0x0D};

	acccon = DRV_Reg32(NFI_ACCCON_REG32);

	if (0 == retryCount)
		mtk_nand_modeentry_rrtry();

	for (cnt = 0; cnt < 5; cnt++)
		mtk_nand_sprmset_rrtry(add_reg[cnt], data_tbl[retryCount][cnt]);

	if (3 == retryCount)
		mtk_nand_rren_rrtry(TRUE);
	else if (6 > retryCount)
		mtk_nand_rren_rrtry(FALSE);

	if (7 == retryCount) {
		mtk_nand_device_reset();
		mtk_nand_reset();
	}

}

const u8 tb_slc_1y[8][2] = {
	{0xF0, 0x00},
	{0xE0, 0x00},
	{0xD0, 0x00},
	{0xC0, 0x00},
	{0x20, 0x00},
	{0x30, 0x00},
	{0x40, 0x00},
	{0x00, 0x00}
};


const u8 tb_tlc_1y[31][7] = {
		{0xFE, 0x03, 0x02, 0x02, 0xFF, 0xFC, 0xFD},
		{0xFE, 0x02, 0x01, 0x01, 0xFE, 0xFA, 0xFB},
		{0xFE, 0x00, 0x00, 0xFF, 0xFC, 0xF8, 0xF9},
		{0xFD, 0xFF, 0xFE, 0xFE, 0xFA, 0xF6, 0xF7},
		{0xFD, 0xFE, 0xFD, 0xFC, 0xF8, 0xF4, 0xF5},
		{0xFD, 0xFD, 0xFC, 0xFB, 0xF6, 0xF2, 0xF2},
		{0xFD, 0xFB, 0xFB, 0xF9, 0xF5, 0xF0, 0xF0},
		{0xFD, 0xFA, 0xF9, 0xF8, 0xF3, 0xEE, 0xEE},
		{0xFD, 0xF9, 0xF8, 0xF6, 0xF1, 0xEC, 0xEC},
		{0xFD, 0xF8, 0xF7, 0xF5, 0xEF, 0xEA, 0xE9},
		{0xFC, 0xF6, 0xF6, 0xF3, 0xEE, 0xE8, 0xE7},
		{0xFA, 0xFA, 0xFB, 0xFA, 0xFB, 0xFA, 0xFA},
		{0xFA, 0xFA, 0xFA, 0xF9, 0xFA, 0xF8, 0xF8},
		{0xFA, 0xFA, 0xFA, 0xF8, 0xF9, 0xF6, 0xF5},
		{0xFB, 0xFA, 0xF9, 0xF7, 0xF7, 0xF4, 0xF3},
		{0xFB, 0xFB, 0xF9, 0xF6, 0xF6, 0xF2, 0xF0},
		{0xFB, 0xFB, 0xF8, 0xF5, 0xF5, 0xF0, 0xEE},
		{0xFB, 0xFB, 0xF8, 0xF5, 0xF4, 0xEE, 0xEB},
		{0xFC, 0xFB, 0xF7, 0xF4, 0xF2, 0xEC, 0xE9},
		{0xFC, 0xFE, 0xFE, 0xF9, 0xFA, 0xF8, 0xF8},
		{0xFD, 0xFE, 0xFD, 0xF7, 0xF7, 0xF4, 0xF3},
		{0xFD, 0xFF, 0xFC, 0xF5, 0xF5, 0xF0, 0xEE},
		{0xFE, 0x03, 0x03, 0x04, 0x01, 0xFF, 0x01},
		{0xFC, 0x00, 0x00, 0x01, 0xFE, 0xFC, 0xFE},
		{0xFA, 0xFA, 0xFC, 0xFC, 0xFA, 0xF7, 0xFA},
		{0x00, 0x03, 0x02, 0x03, 0xFF, 0xFC, 0xFE},
		{0x04, 0x03, 0x03, 0x03, 0x00, 0xFC, 0xFD},
		{0x08, 0x04, 0x03, 0x04, 0x00, 0xFC, 0xFC},
		{0xFC, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08},
		{0xF8, 0x00, 0x00, 0x00, 0x08, 0x08, 0x10},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

static void mtk_nand_modeentry_tlc_rrtry(void)
{
	u32	reg_val = 0;
	u32	timeout = TIMEOUT_3;

	mtk_nand_reset();

	mtk_nand_set_mode(CNFG_OP_CUST);

	mtk_nand_set_command(0x5C);
	mtk_nand_set_command(0xC5);

	mtk_nand_status_ready(STA_NFI_OP_MASK);

	mtk_nand_reset();

	reg_val |= (CNFG_OP_CUST | CNFG_BYTE_RW);
	DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

	mtk_nand_set_command(0x55);
	mtk_nand_set_address(0, 0, 1, 0);

	mtk_nand_status_ready(STA_NFI_OP_MASK);

	DRV_WriteReg32(NFI_CON_REG16, 1 << CON_NFI_SEC_SHIFT);
	NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BWR);
	DRV_WriteReg16(NFI_STRDATA_REG16, 0x1);


	WAIT_NFI_PIO_READY(timeout);
	timeout = TIMEOUT_3;
	DRV_WriteReg8(NFI_DATAW_REG32, 0x01);

	while (!(DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY_RETURN) && (timeout--))
		;
}


static void mtk_nand_toshiba_tlc_1y_rrtry(flashdev_info deviceinfo, u32 retryCount, bool defValue)
{
	u8 add_reg[7] = {0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
	u8 cnt = 0;

	if (TRUE == defValue) {
		for (cnt = 0; cnt < 7; cnt++)
			mtk_nand_sprmset_rrtry(add_reg[cnt], tb_tlc_1y[30][cnt]);
		mtk_nand_set_mode(CNFG_OP_RESET);
		NFI_ISSUE_COMMAND(NAND_CMD_RESET, 0, 0, 0, 0);
		mtk_nand_reset();
		return;
	}

	if (0 == retryCount)
		mtk_nand_modeentry_tlc_rrtry();

	for (cnt = 0; cnt < 7; cnt++)
		mtk_nand_sprmset_rrtry(add_reg[cnt], tb_tlc_1y[retryCount][cnt]);

	mtk_nand_reset();

	mtk_nand_set_mode(CNFG_OP_CUST);

	if (31 == retryCount)
		mtk_nand_set_command(0xB3);
	mtk_nand_set_command(0x5D);

	mtk_nand_status_ready(STA_NFI_OP_MASK);

}

static void mtk_nand_toshiba_slc_1y_rrtry(flashdev_info deviceinfo, u32 retryCount, bool defValue)
{
	u8 add_reg[2] = {0x0B, 0x0D};
	u8 cnt = 0;

	if (TRUE == defValue) {
		for (cnt = 0; cnt < 2; cnt++)
			mtk_nand_sprmset_rrtry(add_reg[cnt], tb_slc_1y[7][cnt]);
		mtk_nand_set_mode(CNFG_OP_RESET);
		NFI_ISSUE_COMMAND(NAND_CMD_RESET, 0, 0, 0, 0);
		mtk_nand_reset();
	}

	if (0 == retryCount)
		mtk_nand_modeentry_tlc_rrtry();

	for (cnt = 0; cnt < 2; cnt++)
		mtk_nand_sprmset_rrtry(add_reg[cnt], tb_slc_1y[retryCount][cnt]);

	mtk_nand_reset();

	mtk_nand_set_mode(CNFG_OP_CUST);

	mtk_nand_set_command(0x5D);

	mtk_nand_status_ready(STA_NFI_OP_MASK);

}

static void mtk_nand_toshiba_tlc_rrtry(struct mtd_info *mtd, struct flashdev_info_t deviceinfo,
				u32 retryCount, bool defValue)
{
	if (gn_devinfo.tlcControl.slcopmodeEn)
		mtk_nand_toshiba_slc_1y_rrtry(deviceinfo, retryCount, defValue);
	else
		mtk_nand_toshiba_tlc_1y_rrtry(deviceinfo, retryCount, defValue);
}

#endif
static void mtk_nand_micron_rrtry(struct mtd_info *mtd, flashdev_info deviceinfo, u32 feature,
				  bool defValue)
{
	mtk_nand_SetFeature(mtd, deviceinfo.feature_set.FeatureSet.sfeatureCmd,
				deviceinfo.feature_set.FeatureSet.readRetryAddress,
								(u8 *)&feature, 4);
}

static int g_sandisk_retry_case;	/*for new read retry table case 1, 2, 3, 4 */
static void mtk_nand_sandisk_rrtry(struct mtd_info *mtd, flashdev_info deviceinfo, u32 feature,
				bool defValue)
{
	if (FALSE == defValue)
		mtk_nand_reset();
	else {
		mtk_nand_device_reset();
		mtk_nand_reset();
		/*should do NAND DEVICE interface change under sync mode */
	}

	mtk_nand_SetFeature(mtd, deviceinfo.feature_set.FeatureSet.sfeatureCmd,
				deviceinfo.feature_set.FeatureSet.readRetryAddress,
				(u8 *) &feature, 4);
	if (FALSE == defValue) {
		if (g_sandisk_retry_case > 1) {
			if (g_sandisk_retry_case == 3) {
				u32 timeout = TIMEOUT_3;

				mtk_nand_reset();
				DRV_WriteReg16(NFI_CNFG_REG16, (CNFG_OP_CUST | CNFG_BYTE_RW));
				mtk_nand_set_command(0x5C);
				mtk_nand_set_command(0xC5);
				mtk_nand_set_command(0x55);
				mtk_nand_set_address(0x00, 0, 1, 0);	/* test mode entry */
				mtk_nand_status_ready(STA_NFI_OP_MASK);
				DRV_WriteReg32(NFI_CON_REG16, 1 << CON_NFI_SEC_SHIFT);
				NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BWR);
				DRV_WriteReg16(NFI_STRDATA_REG16, 0x1);
				WAIT_NFI_PIO_READY(timeout);
				DRV_WriteReg8(NFI_DATAW_REG32, 0x01);
				while (!(DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY_RETURN)
					&& (timeout--))
								;
				mtk_nand_reset();
				timeout = TIMEOUT_3;
				mtk_nand_set_command(0x55);
				mtk_nand_set_address(0x23, 0, 1, 0);
				/*changing parameter LMFLGFIX_NEXT = 1 to all die */
				mtk_nand_status_ready(STA_NFI_OP_MASK);
				DRV_WriteReg32(NFI_CON_REG16, 1 << CON_NFI_SEC_SHIFT);
				NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BWR);
				DRV_WriteReg16(NFI_STRDATA_REG16, 0x1);
				WAIT_NFI_PIO_READY(timeout);
				DRV_WriteReg8(NFI_DATAW_REG32, 0xC0);
				while (!(DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY_RETURN)
					&& (timeout--))
								;
				mtk_nand_reset();
				/*pr_err("Case3# Set LMFLGFIX_NEXT = 1\n"); */
			}
			mtk_nand_set_command(0x25);
			/*pr_err("Case2#3# Set cmd 25\n"); */
		}
		mtk_nand_set_command(deviceinfo.feature_set.FeatureSet.readRetryPreCmd);
	}
}

/*sandisk 19nm read retry*/
u16 sandisk_19nm_rr_table[18] = {
	0x0000,
	0xFF0F, 0xEEFE, 0xDDFD, 0x11EE,
	0x22ED, 0x33DF, 0xCDDE, 0x01DD,
	0x0211, 0x1222, 0xBD21, 0xAD32,
	0x9DF0, 0xBCEF, 0xACDC, 0x9CFF,
	0x0000
};

static void sandisk_19nm_rr_init(void)
{
	u32		  reg_val		 = 0;
	u32		count	  = 0;
	u32		  timeout = 0xffff;
	u32 acccon;

	acccon = DRV_Reg32(NFI_ACCCON_REG32);

	mtk_nand_reset();

	reg_val = (CNFG_OP_CUST | CNFG_BYTE_RW);
	DRV_WriteReg16(NFI_CNFG_REG16, reg_val);
	mtk_nand_set_command(0x3B);
	mtk_nand_set_command(0xB9);

	for (count = 0; count < 9; count++) {
		mtk_nand_set_command(0x53);
		mtk_nand_set_address((0x04 + count), 0, 1, 0);
		DRV_WriteReg16(NFI_CON_REG16, (CON_NFI_BWR | (1 << CON_NFI_SEC_SHIFT)));
		DRV_WriteReg16(NFI_STRDATA_REG16, 1);
		timeout = 0xffff;
		WAIT_NFI_PIO_READY(timeout);
		DRV_WriteReg32(NFI_DATAW_REG32, 0x00);
		mtk_nand_reset();
	}

}

static void sandisk_19nm_rr_loading(u32 retryCount, bool defValue)
{
	u32 reg_val = 0;
	u32 timeout = 0xffff;
	u32 acccon;
	u8 count;
	u8 cmd_reg[4] = { 0x4, 0x5, 0x7 };

	acccon = DRV_Reg32(NFI_ACCCON_REG32);

	mtk_nand_reset();

	reg_val = (CNFG_OP_CUST | CNFG_BYTE_RW);
	DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

	if ((0 != retryCount) || defValue)
		mtk_nand_set_command(0xD6);

	mtk_nand_set_command(0x3B);
	mtk_nand_set_command(0xB9);
	for (count = 0; count < 3; count++) {
		mtk_nand_set_command(0x53);
		mtk_nand_set_address(cmd_reg[count], 0, 1, 0);
		DRV_WriteReg16(NFI_CON_REG16, (CON_NFI_BWR | (1 << CON_NFI_SEC_SHIFT)));
		DRV_WriteReg16(NFI_STRDATA_REG16, 1);
		timeout = 0xffff;
		WAIT_NFI_PIO_READY(timeout);
		if (count == 0)
			DRV_WriteReg32(NFI_DATAW_REG32,
					(((sandisk_19nm_rr_table[retryCount] & 0xF000) >> 8) |
					((sandisk_19nm_rr_table[retryCount] & 0x00F0) >> 4)));
		else if (count == 1)
			DRV_WriteReg32(NFI_DATAW_REG32,
					((sandisk_19nm_rr_table[retryCount] & 0x000F) << 4));
		else if (count == 2)
			DRV_WriteReg32(NFI_DATAW_REG32,
					((sandisk_19nm_rr_table[retryCount] & 0x0F00) >> 4));

		mtk_nand_reset();
	}

	if (!defValue)
		mtk_nand_set_command(0xB6);

}

static void mtk_nand_sandisk_19nm_rrtry(struct mtd_info *mtd, flashdev_info deviceinfo,
					u32 retryCount, bool defValue)
{
	if ((retryCount == 0) && (!defValue))
		sandisk_19nm_rr_init();
	sandisk_19nm_rr_loading(retryCount, defValue);
}

/*hynix 16nm read retry table read*/
#define HYNIX_RR_TABLE_SIZE  (1026)	/*hynix read retry table size */
#define SINGLE_RR_TABLE_SIZE (64)

#define READ_RETRY_STEP (gn_devinfo.feature_set.FeatureSet.readRetryCnt + \
				gn_devinfo.feature_set.FeatureSet.readRetryStart)
#define HYNIX_16NM_RR_TABLE_SIZE  ((READ_RETRY_STEP == 12) ? (784) : (528))	/*hynix read retry table size */
#define SINGLE_RR_TABLE_16NM_SIZE  ((READ_RETRY_STEP == 12)?(48):(32))

u8 nand_hynix_rr_table[(HYNIX_RR_TABLE_SIZE + 16) / 16 * 16];	/*align as 16 byte */

#define NAND_HYX_RR_TBL_BUF nand_hynix_rr_table

static u8 real_hynix_rr_table_idx;
static u32 g_hynix_retry_count;

static bool hynix_rr_table_select(u8 table_index, flashdev_info *deviceinfo)
{
	u32 i;
	u32 table_size =
		(deviceinfo->feature_set.FeatureSet.rtype ==
		 RTYPE_HYNIX_16NM) ? SINGLE_RR_TABLE_16NM_SIZE : SINGLE_RR_TABLE_SIZE;

	for (i = 0; i < table_size; i++) {
		u8 *temp_rr_table = (u8 *) NAND_HYX_RR_TBL_BUF + table_size * table_index * 2 + 2;
		u8 *temp_inversed_rr_table =
			(u8 *) NAND_HYX_RR_TBL_BUF + table_size * table_index * 2 + table_size + 2;
		if (deviceinfo->feature_set.FeatureSet.rtype == RTYPE_HYNIX_16NM) {
			temp_rr_table += 14;
			temp_inversed_rr_table += 14;
		}
		if (0xFF != (temp_rr_table[i] ^ temp_inversed_rr_table[i]))
			return FALSE;	/* error table */
	}
/* print table*/
	if (deviceinfo->feature_set.FeatureSet.rtype == RTYPE_HYNIX_16NM)
		table_size += 16;
	else
		table_size += 2;

	return TRUE;		/* correct table */
}

static void HYNIX_RR_TABLE_READ(flashdev_info *deviceinfo)
{
	u32 reg_val = 0;
	u32 read_count = 0, max_count = HYNIX_RR_TABLE_SIZE;
	u32 timeout = 0xffff;
	u8 *rr_table = (u8 *) (NAND_HYX_RR_TBL_BUF);
	u8 table_index = 0;
	u8 add_reg1[3] = { 0xFF, 0xCC };
	u8 data_reg1[3] = { 0x40, 0x4D };
	u8 cmd_reg[6] = { 0x16, 0x17, 0x04, 0x19, 0x00 };
	u8 add_reg2[6] = { 0x00, 0x00, 0x00, 0x02, 0x00 };
	bool RR_TABLE_EXIST = TRUE;

	if (deviceinfo->feature_set.FeatureSet.rtype == RTYPE_HYNIX_16NM) {
		read_count = 1;
		add_reg1[1] = 0x38;
		data_reg1[1] = 0x52;
		max_count = HYNIX_16NM_RR_TABLE_SIZE;
		if (READ_RETRY_STEP == 12)
			add_reg2[2] = 0x1F;
	}
	mtk_nand_device_reset();

	mtk_nand_reset();

	DRV_WriteReg16(NFI_CNFG_REG16, (CNFG_OP_CUST | CNFG_BYTE_RW));

	mtk_nand_set_command(0x36);

	for (; read_count < 2; read_count++) {
		mtk_nand_set_address(add_reg1[read_count], 0, 1, 0);
		DRV_WriteReg16(NFI_CON_REG16, (CON_NFI_BWR | (1 << CON_NFI_SEC_SHIFT)));
		DRV_WriteReg16(NFI_STRDATA_REG16, 1);
		timeout = 0xffff;
		WAIT_NFI_PIO_READY(timeout);
		DRV_WriteReg32(NFI_DATAW_REG32, data_reg1[read_count]);
		mtk_nand_reset();
	}

	for (read_count = 0; read_count < 5; read_count++)
		mtk_nand_set_command(cmd_reg[read_count]);

	for (read_count = 0; read_count < 5; read_count++)
		mtk_nand_set_address(add_reg2[read_count], 0, 1, 0);

	mtk_nand_set_command(0x30);
	DRV_WriteReg16(NFI_CNRNB_REG16, 0xF1);
	timeout = 0xffff;
	while (!(DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY_RETURN) && (timeout--))
		;

	reg_val = (CNFG_OP_CUST | CNFG_BYTE_RW | CNFG_READ_EN);
	DRV_WriteReg16(NFI_CNFG_REG16, reg_val);
	DRV_WriteReg16(NFI_CON_REG16, (CON_NFI_BRD | (2 << CON_NFI_SEC_SHIFT)));
	DRV_WriteReg16(NFI_STRDATA_REG16, 0x1);
	timeout = 0xffff;
	read_count = 0;
	while ((read_count < max_count) && timeout) {
		WAIT_NFI_PIO_READY(timeout);
		*rr_table++ = (u8) DRV_Reg32(NFI_DATAR_REG32);
		read_count++;
		timeout = 0xFFFF;
	}

	mtk_nand_device_reset();

	reg_val = (CNFG_OP_CUST | CNFG_BYTE_RW);
	if (deviceinfo->feature_set.FeatureSet.rtype == RTYPE_HYNIX_16NM) {
		DRV_WriteReg16(NFI_CNFG_REG16, reg_val);
		mtk_nand_set_command(0x36);
		mtk_nand_set_address(0x38, 0, 1, 0);
		DRV_WriteReg16(NFI_CON_REG16, (CON_NFI_BWR | (1 << CON_NFI_SEC_SHIFT)));
		DRV_WriteReg16(NFI_STRDATA_REG16, 1);
		WAIT_NFI_PIO_READY(timeout);
		DRV_WriteReg32(NFI_DATAW_REG32, 0x00);
		mtk_nand_reset();
		mtk_nand_set_command(0x16);
		mtk_nand_set_command(0x00);
		mtk_nand_set_address(0x00, 0, 1, 0);
		mtk_nand_set_command(0x30);
	} else {
		DRV_WriteReg16(NFI_CNFG_REG16, reg_val);
		mtk_nand_set_command(0x38);
	}
	timeout = 0xffff;
	while (!(DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY_RETURN) && (timeout--))
		;
	rr_table = (u8 *) (NAND_HYX_RR_TBL_BUF);
	if (deviceinfo->feature_set.FeatureSet.rtype == RTYPE_HYNIX) {
		if ((rr_table[0] != 8) || (rr_table[1] != 8)) {
			RR_TABLE_EXIST = FALSE;
			ASSERT(0);
		}
	} else if (deviceinfo->feature_set.FeatureSet.rtype == RTYPE_HYNIX_16NM) {
		for (read_count = 0; read_count < 8; read_count++) {
			if ((rr_table[read_count] != 8) || (rr_table[read_count + 8] != 4)) {
				RR_TABLE_EXIST = FALSE;
				break;
			}
		}
	}
	if (RR_TABLE_EXIST) {
		for (table_index = 0; table_index < 8; table_index++) {
			if (hynix_rr_table_select(table_index, deviceinfo)) {
				real_hynix_rr_table_idx = table_index;
				MSG(INIT, "Hynix rr_tbl_id %d\n", real_hynix_rr_table_idx);
				break;
			}
		}
		if (table_index == 8)
			ASSERT(0);
	} else {
		MSG(INIT, "Hynix RR table index error!\n");
	}
}

static void HYNIX_Set_RR_Para(u32 rr_index, flashdev_info *deviceinfo)
{
	u32 timeout = 0xffff;
	u8 count, max_count = 8;
	u8 add_reg[9] = { 0xCC, 0xBF, 0xAA, 0xAB, 0xCD, 0xAD, 0xAE, 0xAF };
	u8 *hynix_rr_table =
		(u8 *) NAND_HYX_RR_TBL_BUF + SINGLE_RR_TABLE_SIZE * real_hynix_rr_table_idx * 2 + 2;
	if (deviceinfo->feature_set.FeatureSet.rtype == RTYPE_HYNIX_16NM) {
		add_reg[0] = 0x38;
		for (count = 1; count < 4; count++)
			add_reg[count] = add_reg[0] + count;

		hynix_rr_table += 14;
		max_count = 4;
	}
	mtk_nand_reset();

	DRV_WriteReg16(NFI_CNFG_REG16, (CNFG_OP_CUST | CNFG_BYTE_RW));
	mtk_nand_set_command(0x36);

	for (count = 0; count < max_count; count++) {
		mtk_nand_set_address(add_reg[count], 0, 1, 0);
		DRV_WriteReg16(NFI_CON_REG16, (CON_NFI_BWR | (1 << CON_NFI_SEC_SHIFT)));
		DRV_WriteReg16(NFI_STRDATA_REG16, 1);
		timeout = 0xffff;
		WAIT_NFI_PIO_READY(timeout);
		if (timeout == 0) {
			pr_err("HYNIX_Set_RR_Para timeout\n");
			break;
		}
		DRV_WriteReg32(NFI_DATAW_REG32, hynix_rr_table[rr_index * max_count + count]);
	}
	mtk_nand_set_command(0x16);
	mtk_nand_reset();
}
#if 0
static void HYNIX_Get_RR_Para(u32 rr_index, flashdev_info *deviceinfo)
{
	u32 reg_val = 0;
	u32 timeout = 0xffff;
	u8 count, max_count = 8;
	u8 add_reg[9] = { 0xCC, 0xBF, 0xAA, 0xAB, 0xCD, 0xAD, 0xAE, 0xAF };
	u8 *hynix_rr_table =
		(u8 *) NAND_HYX_RR_TBL_BUF + SINGLE_RR_TABLE_SIZE * real_hynix_rr_table_idx * 2 + 2;
	if (deviceinfo->feature_set.FeatureSet.rtype == RTYPE_HYNIX_16NM) {
		add_reg[0] = 0x38;
		for (count = 1; count < 4; count++)
			add_reg[count] = add_reg[0] + count;

		hynix_rr_table += 14;
		max_count = 4;
	}
	mtk_nand_reset();

	DRV_WriteReg16(NFI_CNFG_REG16, (CNFG_OP_CUST | CNFG_BYTE_RW | CNFG_READ_EN));

	for (count = 0; count < max_count; count++) {
		mtk_nand_set_command(0x37);
		mtk_nand_set_address(add_reg[count], 0, 1, 0);

		DRV_WriteReg16(NFI_CON_REG16, (CON_NFI_SRD | (1 << CON_NFI_NOB_SHIFT)));
		DRV_WriteReg16(NFI_STRDATA_REG16, 1);

		timeout = 0xffff;
		WAIT_NFI_PIO_READY(timeout);
		if (timeout == 0)
			pr_err("HYNIX_Get_RR_Para timeout\n");

		pr_err("Get[%02X]%02X\n", add_reg[count], DRV_Reg8(NFI_DATAR_REG32));
		mtk_nand_reset();
	}
}
#endif

static void mtk_nand_hynix_rrtry(struct mtd_info *mtd, flashdev_info deviceinfo, u32 retryCount,
				 bool defValue)
{
	if (defValue == FALSE) {
		if (g_hynix_retry_count == READ_RETRY_STEP)
			g_hynix_retry_count = 0;

		pr_err("Hynix Retry %d\n", g_hynix_retry_count);
		HYNIX_Set_RR_Para(g_hynix_retry_count, &deviceinfo);
		g_hynix_retry_count++;
	}
}

static void mtk_nand_hynix_16nm_rrtry(struct mtd_info *mtd, flashdev_info deviceinfo,
					  u32 retryCount, bool defValue)
{
	if (defValue == FALSE) {
		if (g_hynix_retry_count == READ_RETRY_STEP)
			g_hynix_retry_count = 0;

		pr_err("Hynix 16nm Retry %d\n", g_hynix_retry_count);
		HYNIX_Set_RR_Para(g_hynix_retry_count, &deviceinfo);
		g_hynix_retry_count++;
	}
}

u32 special_rrtry_setting[37] = {
	0x00000000, 0x7C00007C, 0x787C0004, 0x74780078,
	0x7C007C08, 0x787C7C00, 0x74787C7C, 0x70747C00,
	0x7C007800, 0x787C7800, 0x74787800, 0x70747800,
	0x6C707800, 0x00040400, 0x7C000400, 0x787C040C,
	0x7478040C, 0x7C000810, 0x00040810, 0x04040C0C,
	0x00040C10, 0x00081014, 0x000C1418, 0x7C040C0C,
	0x74787478, 0x70747478, 0x6C707478, 0x686C7478,
	0x74787078, 0x70747078, 0x686C7078, 0x6C707078,
	0x6C706C78, 0x686C6C78, 0x64686C78, 0x686C6874,
	0x64686874,
};

u32 special_mlcslc_rrtry_setting[23] = {
	0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x7C, 0x78,
	0x74, 0x18, 0x1C, 0x20, 0x70, 0x6C, 0x68, 0x24,
	0x28, 0x2C, 0x64, 0x60, 0x5C, 0x58, 0x54,
};

u32 sandisk_tlc_rrtbl_12h[40] = {
	0x00000000, 0x08000004, 0x00000404, 0x04040408,
	0x08040408, 0x0004080C, 0x04040810, 0x0C0C0C00,
	0x0E0E0E00, 0x10101000, 0x12121200, 0x080808FC,
	0xFC08FCF8, 0x0000FBF6, 0x0408FBF4, 0xFEFCF8FA,
	0xFCF8F4EC, 0xF8F8F8EC, 0x0002FCE4, 0xFCFEFEFE,
	0xFFFC00FD, 0xFEFB00FC, 0xFEFAFEFA, 0xFDF9FDFA,
	0xFBF8FBFA, 0xF9F7FAF8, 0xF8F6F9F4, 0xF5F4F8F2,
	0xF4F2F6EE, 0xF0F0F4E8, 0xECECF0E6, 0x020400FA,
	0x00FEFFF8, 0xFEFEFDF6, 0xFDFDFCF4, 0xFBFCFCF2,
	0xF9FBFBF0, 0xF8F9F9EE, 0xF6F8F8ED, 0xF4F7F6EA,
};

u32 sandisk_tlc_rrtbl_13h[40] = {
	0x00000000, 0x00040800, 0x00080004, 0x00020404,
	0x00040800, 0x00080000, 0x00FC0000, 0x000C0C0C,
	0x000E0E0E, 0x00101010, 0x00141414, 0x000008FC,
	0x0004FCF8, 0x00FC00F6, 0x00FC0404, 0x00FCFE08,
	0x00FCFC00, 0x00F8F8FA, 0x000000F4, 0x00FAFC02,
	0x00F8FF00, 0x00F6FDFE, 0x00F4FBFC, 0x00F2F9FA,
	0x00F0F7F8, 0x00EEF5F6, 0x00ECF3F4, 0x00EAF1F2,
	0x00E8ECEE, 0x00E0E4E8, 0x00DAE0E2, 0x00000000,
	0x00FEFEFE, 0x00FBFCFC, 0x00F9FAFA, 0x00F7F8F8,
	0x00F5F6F6, 0x00F3F4F4, 0x00F1F2F2, 0x00EFF0EF,
};

u32 sandisk_tlc_rrtbl_14h[11] = {
	0x00000000, 0x00000010, 0x00000020, 0x00000030,
	0x00000040, 0x00000050, 0x00000060, 0x000000F0,
	0x000000E0, 0x000000D0, 0x000000C0,
};

static void mtk_nand_sandisk_tlc_1ynm_rrtry(struct mtd_info *mtd,
					struct flashdev_info_t deviceinfo, u32 feature, bool defValue)
{
	u16 reg_val = 0;
	u32	timeout = TIMEOUT_3;
	u32 value1, value2, value3;

	if ((feature > 1) || defValue) {
		mtk_nand_reset();
		reg_val |= (CNFG_OP_CUST | CNFG_BYTE_RW);
		DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

		mtk_nand_set_command(0x55);
		mtk_nand_set_address(0, 0, 1, 0);
		DRV_WriteReg32(NFI_CON_REG16, 1 << CON_NFI_SEC_SHIFT);
		NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BWR);
		DRV_WriteReg16(NFI_STRDATA_REG16, 0x1);
		WAIT_NFI_PIO_READY(timeout);
		if (timeout == 0)
			MSG(INIT, "mtk_nand_sandisk_tlc_1ynm_rrtry: timeout\n");

		DRV_WriteReg32(NFI_DATAW_REG32, 0);

		mtk_nand_device_reset();
	}

	if (gn_devinfo.tlcControl.slcopmodeEn) {
		value3 = sandisk_tlc_rrtbl_14h[feature];
		mtk_nand_SetFeature(mtd, deviceinfo.feature_set.FeatureSet.sfeatureCmd, 0x14, (u8 *)&value3, 4);
	} else {
		value1 = sandisk_tlc_rrtbl_12h[feature];
		value2 = sandisk_tlc_rrtbl_13h[feature];
		mtk_nand_SetFeature(mtd, deviceinfo.feature_set.FeatureSet.sfeatureCmd, 0x12, (u8 *)&value1, 4);
		mtk_nand_SetFeature(mtd, deviceinfo.feature_set.FeatureSet.sfeatureCmd, 0x13, (u8 *)&value2, 4);
	}

	if (FALSE == defValue) {
		mtk_nand_reset();
		reg_val |= (CNFG_OP_CUST | CNFG_BYTE_RW);
		DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

		mtk_nand_set_command(0x5D);

		mtk_nand_reset();
	}
}

static u32 mtk_nand_rrtry_setting(flashdev_info deviceinfo, enum readRetryType type, u32 retryStart,
				  u32 loopNo)
{
	u32 value;
	{
		if (retryStart != 0xFFFFFFFF)
			value = retryStart + loopNo;
		else {
			if (gn_devinfo.tlcControl.slcopmodeEn)
				value = special_mlcslc_rrtry_setting[loopNo];
			else
				value = special_rrtry_setting[loopNo];
		}
	}

	return value;
}

typedef void(*rrtryFunctionType) (struct mtd_info *mtd, flashdev_info deviceinfo, u32 feature,
				 bool defValue);

static rrtryFunctionType rtyFuncArray[] = {
	mtk_nand_micron_rrtry,
	mtk_nand_sandisk_rrtry,
	mtk_nand_sandisk_19nm_rrtry,
	mtk_nand_toshiba_rrtry,
	mtk_nand_hynix_rrtry,
	mtk_nand_hynix_16nm_rrtry,
	mtk_nand_sandisk_tlc_1ynm_rrtry,
	mtk_nand_toshiba_tlc_rrtry
};


static void mtk_nand_rrtry_func(struct mtd_info *mtd, flashdev_info deviceinfo, u32 feature,
				bool defValue)
{
	if (MLC_DEVICE)
		rtyFuncArray[deviceinfo.feature_set.FeatureSet.rtype] (mtd, deviceinfo, feature,
									defValue);
}

/******************************************************************************
 * mtk_nand_exec_read_page
 *
 * DESCRIPTION:
 *Read a page data !
 *
 * PARAMETERS:
 *struct mtd_info *mtd, u32 u4RowAddr, u32 u4PageSize,
 *u8 *pPageBuf, u8 *pFDMBuf
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
int mtk_nand_exec_read_page(struct mtd_info *mtd, u32 u4RowAddr, u32 u4PageSize, u8 *pPageBuf,
				u8 *pFDMBuf)
{
	u8 *buf;
	int bRet = ERR_RTN_SUCCESS;
	struct nand_chip *nand = mtd->priv;
	u32 u4SecNum = u4PageSize >> host->hw->nand_sec_shift;
	u32 backup_corrected, backup_failed;
	bool readRetry = FALSE;
	int retryCount = 0;
	u32 retrytotalcnt = gn_devinfo.feature_set.FeatureSet.readRetryCnt;
	u32 tempBitMap, bitMap;
#ifdef NAND_PFM
	struct timeval pfm_time_read;
#endif
	struct NFI_TLC_WL_INFO  tlc_wl_info;
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	bool tlc_left_plane = TRUE;
	unsigned int phys = 0;
#endif
	u32 reg_val = 0;
	u32 real_row_addr = 0;
	u32 logical_plane_num = 1;
	u32 data_sector_num = 0;
	u8	*temp_byte_ptr = NULL;
	u8	*spare_ptr = NULL;
#if 0
	unsigned short PageFmt_Reg = 0;
	unsigned int NAND_ECC_Enc_Reg = 0;
	unsigned int NAND_ECC_Dec_Reg = 0;
#endif
	u32 block_addr = 0;
	u32 page_in_block = 0;
	u32 page_per_block = 0;

	/*MSG(INIT, "mtk_nand_exec_read_page, u4RowAddr: %x\n", u4RowAddr);*/

	page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;

	PFM_BEGIN(pfm_time_read);
	tempBitMap = 0;

	if (((u32) pPageBuf % 16) && local_buffer_16_align)
		buf = local_buffer_16_align;
	else {
		if (virt_addr_valid(pPageBuf) == 0)
			buf = local_buffer_16_align;
		else
			buf = pPageBuf;
	}
	backup_corrected = mtd->ecc_stats.corrected;
	backup_failed = mtd->ecc_stats.failed;
	bitMap = 0;
	do {
		mtk_nand_interface_switch(mtd);
		data_sector_num = u4SecNum;
		temp_byte_ptr = buf;
		spare_ptr = pFDMBuf;
		logical_plane_num = 1;
		tlc_wl_info.wl_pre = WL_LOW_PAGE; /* init for build warning*/
		tlc_wl_info.word_line_idx = u4RowAddr;

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
			if (gn_devinfo.tlcControl.normaltlc) {
				NFI_TLC_GetMappedWL(u4RowAddr, &tlc_wl_info);
				real_row_addr = NFI_TLC_GetRowAddr(tlc_wl_info.word_line_idx);

				if (gn_devinfo.tlcControl.pPlaneEn) {
					tlc_left_plane = TRUE;
					logical_plane_num = 2;
					data_sector_num /= 2;
					real_row_addr = NFI_TLC_SetpPlaneAddr(real_row_addr, tlc_left_plane);
				}
				/* MSG(INIT, "mtk_nand_exec_read_page, u4RowAddr: 0x%x real_row_addr 0x%x %d\n",
					u4RowAddr, real_row_addr, gn_devinfo.tlcControl.slcopmodeEn); */
			} else {
				real_row_addr = NFI_TLC_GetRowAddr(u4RowAddr);
			}

			if (gn_devinfo.tlcControl.slcopmodeEn) {
				if (0xFF != gn_devinfo.tlcControl.en_slc_mode_cmd) {
					reg_val = DRV_Reg16(NFI_CNFG_REG16);
					reg_val &= ~CNFG_READ_EN;
					reg_val &= ~CNFG_OP_MODE_MASK;
					reg_val |= CNFG_OP_CUST;
					DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

					mtk_nand_set_command(gn_devinfo.tlcControl.en_slc_mode_cmd);

					reg_val = DRV_Reg32(NFI_CON_REG16);
					reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
					/* issue reset operation */
					DRV_WriteReg32(NFI_CON_REG16, reg_val);
				}
			} else {
				if (gn_devinfo.tlcControl.normaltlc) {
					reg_val = DRV_Reg16(NFI_CNFG_REG16);
					reg_val &= ~CNFG_READ_EN;
					reg_val &= ~CNFG_OP_MODE_MASK;
					reg_val |= CNFG_OP_CUST;
					DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

					if (tlc_wl_info.wl_pre == WL_LOW_PAGE)
						mtk_nand_set_command(LOW_PG_SELECT_CMD);
					else if (tlc_wl_info.wl_pre == WL_MID_PAGE)
						mtk_nand_set_command(MID_PG_SELECT_CMD);
					else if (tlc_wl_info.wl_pre == WL_HIGH_PAGE)
						mtk_nand_set_command(HIGH_PG_SELECT_CMD);

					reg_val = DRV_Reg32(NFI_CON_REG16);
					reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
					/* issue reset operation */
					DRV_WriteReg32(NFI_CON_REG16, reg_val);
				}
			}
			reg_val = 0;
		} else
#endif
			real_row_addr = u4RowAddr;

	/* if(force_slc_flag == 1)
		gn_devinfo.tlcControl.slcopmodeEn = true; */

	if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER) {
		if (gn_devinfo.tlcControl.slcopmodeEn) {
			if (0xFF != gn_devinfo.tlcControl.en_slc_mode_cmd) {
				reg_val = DRV_Reg16(NFI_CNFG_REG16);
				reg_val &= ~CNFG_READ_EN;
				reg_val &= ~CNFG_OP_MODE_MASK;
				reg_val |= CNFG_OP_CUST;
				DRV_WriteReg16(NFI_CNFG_REG16, reg_val);
				mtk_nand_set_command(gn_devinfo.tlcControl.en_slc_mode_cmd);
				reg_val = DRV_Reg32(NFI_CON_REG16);
				reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
				/* issue reset operation */
				DRV_WriteReg32(NFI_CON_REG16, reg_val);

				if (gn_devinfo.vendor == VEND_SANDISK) {
					block_addr = real_row_addr/page_per_block;
					page_in_block = real_row_addr % page_per_block;
					page_in_block <<= 1;
					real_row_addr = page_in_block + block_addr * page_per_block;
					/* pr_err("mtk_nand_exec_read_sector SLC Mode real_row_addr:%d, u4RowAddr:%d\n",
						real_row_addr, u4RowAddr); */
				}
			}
		} else {
			if (0xFF != gn_devinfo.tlcControl.dis_slc_mode_cmd) {
				reg_val = DRV_Reg16(NFI_CNFG_REG16);
				reg_val &= ~CNFG_READ_EN;
				reg_val &= ~CNFG_OP_MODE_MASK;
				reg_val |= CNFG_OP_CUST;
				DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

				mtk_nand_set_command(gn_devinfo.tlcControl.dis_slc_mode_cmd);
				reg_val = DRV_Reg32(NFI_CON_REG16);
				reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
				/* issue reset operation */
				DRV_WriteReg32(NFI_CON_REG16, reg_val);
			}
		}
	}
	if (use_randomizer && u4RowAddr >= RAND_START_ADDR) {
		if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
			if (gn_devinfo.tlcControl.slcopmodeEn)
				mtk_nand_turn_on_randomizer(mtd, nand, tlc_wl_info.word_line_idx);
			else
				mtk_nand_turn_on_randomizer(mtd, nand,
					(tlc_wl_info.word_line_idx*3+tlc_wl_info.wl_pre));
		} else
			mtk_nand_turn_on_randomizer(mtd, nand, u4RowAddr);
	}

	if (mtk_nand_ready_for_read(nand, real_row_addr, 0, data_sector_num, true, buf, NORMAL_READ)) {
		while (logical_plane_num) {
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
				if (gn_devinfo.tlcControl.needchangecolumn) {
					if (gn_devinfo.tlcControl.pPlaneEn)
						real_row_addr = NFI_TLC_SetpPlaneAddr(real_row_addr, tlc_left_plane);
#if 1
					reg_val = DRV_Reg32(NFI_CON_REG16);
					reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
					/* issue reset operation */
					DRV_WriteReg32(NFI_CON_REG16, reg_val);
#endif

					reg_val = DRV_Reg16(NFI_CNFG_REG16);
					reg_val &= ~CNFG_READ_EN;
					reg_val &= ~CNFG_OP_MODE_MASK;
					reg_val |= CNFG_OP_CUST;
					DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

					mtk_nand_set_command(CHANGE_COLUNM_ADDR_1ST_CMD);
					mtk_nand_set_address(0, real_row_addr, 2, gn_devinfo.addr_cycle - 2);
					mtk_nand_set_command(CHANGE_COLUNM_ADDR_2ND_CMD);

					reg_val = DRV_Reg16(NFI_CNFG_REG16);
					reg_val |= CNFG_READ_EN;
					reg_val &= ~CNFG_OP_MODE_MASK;
					reg_val |= CNFG_OP_READ;
					DRV_WriteReg16(NFI_CNFG_REG16, reg_val);
				}
			}

			mtk_dir = DMA_FROM_DEVICE;
			sg_init_one(&mtk_sg, temp_byte_ptr, (data_sector_num * (1 << host->hw->nand_sec_shift)));
			dma_map_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
			phys = mtk_sg.dma_address;
#if __INTERNAL_USE_AHB_MODE__
			if (!phys)
				pr_warn("[%s]convert virt addr (%lx) to phys add (%x)fail!!!",
					__func__, (unsigned long) temp_byte_ptr, phys);
			else
				DRV_WriteReg32(NFI_STRADDR_REG32, phys);
#endif

			DRV_WriteReg32(NFI_CON_REG16, data_sector_num << CON_NFI_SEC_SHIFT);

			if (g_bHwEcc)
				ECC_Decode_Start();
#endif
			if (!mtk_nand_read_page_data(mtd, temp_byte_ptr,
				data_sector_num * (1 << host->hw->nand_sec_shift))) {
				MSG(INIT, "mtk_nand_read_page_data fail\n");
				bRet = ERR_RTN_FAIL;
			}
			dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
			if (!mtk_nand_status_ready(STA_NAND_BUSY)) {
				MSG(INIT, "mtk_nand_status_ready fail\n");
				bRet = ERR_RTN_FAIL;
			}
			if (g_bHwEcc) {
				if (!mtk_nand_check_dececc_done(data_sector_num)) {
					MSG(INIT, "mtk_nand_check_dececc_done fail\n");
					bRet = ERR_RTN_FAIL;
				}
			}
			mtk_nand_read_fdm_data(spare_ptr, data_sector_num);
			if (g_bHwEcc) {
				if (!mtk_nand_check_bch_error
					(mtd, temp_byte_ptr, spare_ptr, data_sector_num - 1, u4RowAddr, &tempBitMap)) {
					if (gn_devinfo.vendor != VEND_NONE)
						readRetry = TRUE;

					MSG(INIT, "mtk_nand_check_bch_error fail, retryCount: %d\n",
						retryCount);
				bRet = ERR_RTN_BCH_FAIL;
			} else {
					if ((0 != (DRV_Reg32(NFI_STA_REG32) & STA_READ_EMPTY)) &&
						(retryCount != 0)) {
							MSG(INIT,
								"read retry read empty page, return as uncorrectable\n");
							mtd->ecc_stats.failed += data_sector_num;
						bRet = ERR_RTN_BCH_FAIL;
					}
			}
		}
		mtk_nand_stop_read();

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
				if (gn_devinfo.tlcControl.needchangecolumn)
					DRV_WriteReg16(NFI_TLC_RD_WHR2_REG16, 0x055);

				if (2 == logical_plane_num) {
					tlc_left_plane = FALSE;
					spare_ptr += (host->hw->nand_fdm_size * data_sector_num);

#if __INTERNAL_USE_AHB_MODE__
					temp_byte_ptr += (data_sector_num * (1 << host->hw->nand_sec_shift));
#else
					temp_byte_ptr +=
					(data_sector_num * ((1 << host->hw->nand_sec_shift) + spare_per_sector);
#endif
				}
			}
#endif
			logical_plane_num--;

			if (bRet == ERR_RTN_BCH_FAIL)
				break;
				}
			}
#ifndef CONFIG_MTK_TLC_NAND_SUPPORT
	else
		dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
#endif

		if (use_randomizer && u4RowAddr >= RAND_START_ADDR)
			mtk_nand_turn_off_randomizer();

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC
		|| gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER) {
			if ((gn_devinfo.tlcControl.slcopmodeEn)
				&& (0xFF != gn_devinfo.tlcControl.dis_slc_mode_cmd)) {
				reg_val = DRV_Reg32(NFI_CON_REG16);
				reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
				/* issue reset operation */
				DRV_WriteReg32(NFI_CON_REG16, reg_val);

				reg_val = DRV_Reg16(NFI_CNFG_REG16);
				reg_val &= ~CNFG_READ_EN;
				reg_val &= ~CNFG_OP_MODE_MASK;
				reg_val |= CNFG_OP_CUST;
				DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

				mtk_nand_set_command(gn_devinfo.tlcControl.dis_slc_mode_cmd);
			}
		}
#endif
		if (bRet == ERR_RTN_BCH_FAIL) {
			u32 feature;

						tempBitMap = 0;
						feature =
				mtk_nand_rrtry_setting(gn_devinfo,
						gn_devinfo.feature_set.FeatureSet.rtype,
						gn_devinfo.feature_set.FeatureSet.readRetryStart,
						retryCount);
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			if ((gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_SANDISK_TLC_1YNM)
				&& (gn_devinfo.tlcControl.slcopmodeEn))
				retrytotalcnt = 10;
			if (gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_TOSHIBA_TLC) {
				if (gn_devinfo.tlcControl.slcopmodeEn)
					retrytotalcnt = 8;
				else
					retrytotalcnt = 31;
			}
#endif

			if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER)  {
				if ((gn_devinfo.tlcControl.slcopmodeEn)
					&& (gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_SANDISK)) {
					retrytotalcnt = 22;
				}
			}

			if (retryCount < retrytotalcnt) {
				mtd->ecc_stats.corrected = backup_corrected;
				mtd->ecc_stats.failed = backup_failed;
				mtk_nand_rrtry_func(mtd, gn_devinfo, feature, FALSE);
				retryCount++;
			} else {
				feature = gn_devinfo.feature_set.FeatureSet.readRetryDefault;
				if ((gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_SANDISK)
					&& (g_sandisk_retry_case < 3)) {
					g_sandisk_retry_case++;
					/*pr_err("Sandisk read retry case#%d\n", g_sandisk_retry_case); */
					tempBitMap = 0;
					mtd->ecc_stats.corrected = backup_corrected;
					mtd->ecc_stats.failed = backup_failed;
					mtk_nand_rrtry_func(mtd, gn_devinfo, feature, FALSE);
					retryCount = 0;
				} else {
					mtk_nand_rrtry_func(mtd, gn_devinfo, feature, TRUE);
				readRetry = FALSE;
					g_sandisk_retry_case = 0;
				}
			}
			if ((g_sandisk_retry_case == 1) || (g_sandisk_retry_case == 3)) {
				mtk_nand_set_command(0x26);
				/*pr_err("Case1#3# Set cmd 26\n"); */
		}
		} else {
			if ((retryCount != 0) && MLC_DEVICE) {
				u32 feature = gn_devinfo.feature_set.FeatureSet.readRetryDefault;

				mtk_nand_rrtry_func(mtd, gn_devinfo, feature, TRUE);
			}
			readRetry = FALSE;
			g_sandisk_retry_case = 0;
		}
		if (TRUE == readRetry)
			bRet = ERR_RTN_SUCCESS;
	} while (readRetry);
	if (retryCount != 0) {
		u32 feature = gn_devinfo.feature_set.FeatureSet.readRetryDefault;

		if (bRet == ERR_RTN_SUCCESS) {
			MSG(INIT, "OK Read retry Buf: %x %x %x %x\n", pPageBuf[0], pPageBuf[1],
				pPageBuf[2], pPageBuf[3]);
			MSG(INIT,
				"u4RowAddr: 0x%x read retry pass, retrycnt: %d ENUM0: %x, ENUM1: %x, mtd_ecc(A): %x, mtd_ecc(B): %x\n",
				u4RowAddr, retryCount, DRV_Reg32(ECC_DECENUM1_REG32),
				DRV_Reg32(ECC_DECENUM0_REG32), mtd->ecc_stats.failed, backup_failed);
			MSG(INIT, "Read retry over %d times, trigger re-write\n", retryCount);
			mtd->ecc_stats.corrected++;
			if ((gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_HYNIX_16NM)
				|| (gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_HYNIX))
				g_hynix_retry_count--;
		} else {
			MSG(INIT,
				"u4RowAddr: 0x%x read retry fail, mtd_ecc(A): %x , fail, mtd_ecc(B): %x\n",
				u4RowAddr, mtd->ecc_stats.failed, backup_failed);
		}
		mtk_nand_rrtry_func(mtd, gn_devinfo, feature, TRUE);
		g_sandisk_retry_case = 0;
	}

	if (buf == local_buffer_16_align)
		memcpy(pPageBuf, buf, u4PageSize);

	if (bRet != ERR_RTN_SUCCESS) {
		MSG(INIT, "ECC uncorrectable , fake buffer returned\n");
		memset(pPageBuf, 0xff, u4PageSize);
		memset(pFDMBuf, 0xff, u4SecNum * host->hw->nand_fdm_size);
	}

	PFM_END_R(pfm_time_read, u4PageSize + 32);

	return bRet;
}

bool mtk_nand_exec_read_sector(struct mtd_info *mtd, u32 u4RowAddr, u32 u4ColAddr, u32 u4PageSize,
				u8 *pPageBuf, u8 *pFDMBuf, int subpageno)
{
	u8 *buf;
	int bRet = ERR_RTN_SUCCESS;
	struct nand_chip *nand = mtd->priv;
	u32 u4SecNum = subpageno;
	u32 backup_corrected, backup_failed;
	bool readRetry = FALSE;
	int retryCount = 0;
	u32 retrytotalcnt = gn_devinfo.feature_set.FeatureSet.readRetryCnt;
	u32 tempBitMap;
#ifdef NAND_PFM
	struct timeval pfm_time_read;
#endif
	struct NFI_TLC_WL_INFO  tlc_wl_info;
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	bool tlc_left_plane = TRUE;
	unsigned int phys = 0;
	u32 sector_per_page = mtd->writesize >> host->hw->nand_sec_shift;
	int spare_per_sector = mtd->oobsize / sector_per_page;
#endif
	u32 reg_val = 0;
	u32 real_row_addr = 0;
	u32 logical_plane_num = 1;
	u32 temp_col_addr[2] = {0, 0};
	u32 data_sector_num[2] = {0, 0};
	u8	*temp_byte_ptr = NULL;
	u8	*spare_ptr = NULL;
	u32 block_addr = 0;
	u32 page_in_block = 0;
	u32 page_per_block = 0;

	/*MSG(INIT, "mtk_nand_exec_read_page, host->hw->nand_sec_shift: %d\n", host->hw->nand_sec_shift); */

	page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;

	PFM_BEGIN(pfm_time_read);

	if (((u32) pPageBuf % 16) && local_buffer_16_align)
		buf = local_buffer_16_align;
	else {
		if (virt_addr_valid(pPageBuf) == 0)
			buf = local_buffer_16_align;
		else
			buf = pPageBuf;
	}
	backup_corrected = mtd->ecc_stats.corrected;
	backup_failed = mtd->ecc_stats.failed;
	do {
		mtk_nand_interface_switch(mtd);
		temp_byte_ptr = buf;
		spare_ptr = pFDMBuf;
		temp_col_addr[0] = u4ColAddr;
		temp_col_addr[1] = 0;
		data_sector_num[0] = u4SecNum;
		data_sector_num[1] = 0;
		logical_plane_num = 1;
		tlc_wl_info.word_line_idx = u4RowAddr;
		tlc_wl_info.wl_pre = WL_LOW_PAGE;

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
			if (gn_devinfo.tlcControl.normaltlc) {
				NFI_TLC_GetMappedWL(u4RowAddr, &tlc_wl_info);
				real_row_addr = NFI_TLC_GetRowAddr(tlc_wl_info.word_line_idx);

				if ((gn_devinfo.tlcControl.pPlaneEn)
				&& (u4ColAddr < ((sector_per_page / 2)
				* (host->hw->nand_sec_size + spare_per_sector)))) {
					tlc_left_plane = TRUE;
					if ((u4ColAddr + (u4SecNum * (host->hw->nand_sec_size + spare_per_sector)))
					> ((sector_per_page / 2) * (host->hw->nand_sec_size + spare_per_sector))) {
						logical_plane_num = 2;
						data_sector_num[1] =
						(sector_per_page / 2)
						- (u4ColAddr / (host->hw->nand_sec_size + spare_per_sector));
						data_sector_num[0] = u4SecNum - data_sector_num[1];
						temp_col_addr[0] = 0;
						temp_col_addr[1] = u4ColAddr;
					}
					real_row_addr = NFI_TLC_SetpPlaneAddr(real_row_addr, tlc_left_plane);
				}
				if (gn_devinfo.tlcControl.pPlaneEn && (u4ColAddr >= ((sector_per_page / 2)
					* (host->hw->nand_sec_size + spare_per_sector)))) {
					temp_col_addr[0] =
						u4ColAddr - ((sector_per_page / 2)
							* (host->hw->nand_sec_size + spare_per_sector));
					tlc_left_plane = FALSE;
					real_row_addr = NFI_TLC_SetpPlaneAddr(real_row_addr, tlc_left_plane);
				}
			} else {
				real_row_addr = NFI_TLC_GetRowAddr(u4RowAddr);
			}

			if (gn_devinfo.tlcControl.slcopmodeEn) {
				if (0xFF != gn_devinfo.tlcControl.en_slc_mode_cmd) {
					reg_val = DRV_Reg16(NFI_CNFG_REG16);
					reg_val &= ~CNFG_READ_EN;
					reg_val &= ~CNFG_OP_MODE_MASK;
					reg_val |= CNFG_OP_CUST;
					DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

					mtk_nand_set_command(gn_devinfo.tlcControl.en_slc_mode_cmd);

					reg_val = DRV_Reg32(NFI_CON_REG16);
					reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
					/* issue reset operation */
					DRV_WriteReg32(NFI_CON_REG16, reg_val);
				}
			} else {
				if (gn_devinfo.tlcControl.normaltlc) {
					reg_val = DRV_Reg16(NFI_CNFG_REG16);
					reg_val &= ~CNFG_READ_EN;
					reg_val &= ~CNFG_OP_MODE_MASK;
					reg_val |= CNFG_OP_CUST;
					DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

					if (tlc_wl_info.wl_pre == WL_LOW_PAGE)
						mtk_nand_set_command(LOW_PG_SELECT_CMD);
					else if (tlc_wl_info.wl_pre == WL_MID_PAGE)
						mtk_nand_set_command(MID_PG_SELECT_CMD);
					else if (tlc_wl_info.wl_pre == WL_HIGH_PAGE)
						mtk_nand_set_command(HIGH_PG_SELECT_CMD);

					reg_val = DRV_Reg32(NFI_CON_REG16);
					reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
					/* issue reset operation */
					DRV_WriteReg32(NFI_CON_REG16, reg_val);
				}
			}
			reg_val = 0;
		} else
#endif
		{
			real_row_addr = u4RowAddr;
		}
		if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER) {
			if (gn_devinfo.tlcControl.slcopmodeEn) {
				if (0xFF != gn_devinfo.tlcControl.en_slc_mode_cmd) {
					reg_val = DRV_Reg16(NFI_CNFG_REG16);
					reg_val &= ~CNFG_READ_EN;
					reg_val &= ~CNFG_OP_MODE_MASK;
					reg_val |= CNFG_OP_CUST;
					DRV_WriteReg16(NFI_CNFG_REG16, reg_val);
					mtk_nand_set_command(gn_devinfo.tlcControl.en_slc_mode_cmd);
					reg_val = DRV_Reg32(NFI_CON_REG16);
					reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
					/* issue reset operation */
					DRV_WriteReg32(NFI_CON_REG16, reg_val);

					if (gn_devinfo.vendor == VEND_SANDISK) {
						block_addr = real_row_addr/page_per_block;
						page_in_block = real_row_addr % page_per_block;
						page_in_block <<= 1;
						real_row_addr = page_in_block + block_addr * page_per_block;
		/* pr_err("mtk_nand_exec_read_sector SLC Mode real_row_addr:%d, u4RowAddr:%d\n",
					real_row_addr, u4RowAddr); */
					}
				}
			}  else {
				if (0xFF != gn_devinfo.tlcControl.dis_slc_mode_cmd) {
					reg_val = DRV_Reg16(NFI_CNFG_REG16);
					reg_val &= ~CNFG_READ_EN;
					reg_val &= ~CNFG_OP_MODE_MASK;
					reg_val |= CNFG_OP_CUST;
					DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

					mtk_nand_set_command(gn_devinfo.tlcControl.dis_slc_mode_cmd);
					reg_val = DRV_Reg32(NFI_CON_REG16);
					reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
					/* issue reset operation */
					DRV_WriteReg32(NFI_CON_REG16, reg_val);
				}
			}
		}
		if (use_randomizer && u4RowAddr >= RAND_START_ADDR) {
			if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
				if (gn_devinfo.tlcControl.slcopmodeEn)
					mtk_nand_turn_on_randomizer(mtd, nand, tlc_wl_info.word_line_idx);
				else
					mtk_nand_turn_on_randomizer(mtd, nand,
					(tlc_wl_info.word_line_idx*3+tlc_wl_info.wl_pre));
			} else
				mtk_nand_turn_on_randomizer(mtd, nand, u4RowAddr);
		}

	if (mtk_nand_ready_for_read
		(nand, real_row_addr, temp_col_addr[logical_plane_num-1],
			data_sector_num[logical_plane_num - 1], true, buf, NORMAL_READ)) {
		while (logical_plane_num) {
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
				if (gn_devinfo.tlcControl.needchangecolumn) {
					if (gn_devinfo.tlcControl.pPlaneEn)
						real_row_addr = NFI_TLC_SetpPlaneAddr(real_row_addr, tlc_left_plane);
#if 1
					reg_val = DRV_Reg32(NFI_CON_REG16);
					reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
					/* issue reset operation */
					DRV_WriteReg32(NFI_CON_REG16, reg_val);
#endif

					reg_val = DRV_Reg16(NFI_CNFG_REG16);
					reg_val &= ~CNFG_READ_EN;
					reg_val &= ~CNFG_OP_MODE_MASK;
					reg_val |= CNFG_OP_CUST;
					DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

					mtk_nand_set_command(CHANGE_COLUNM_ADDR_1ST_CMD);
					mtk_nand_set_address
						(temp_col_addr[logical_plane_num-1], real_row_addr,
							2, gn_devinfo.addr_cycle - 2);
					mtk_nand_set_command(CHANGE_COLUNM_ADDR_2ND_CMD);

					reg_val = DRV_Reg16(NFI_CNFG_REG16);
					reg_val |= CNFG_READ_EN;
					reg_val &= ~CNFG_OP_MODE_MASK;
					reg_val |= CNFG_OP_READ;
					DRV_WriteReg16(NFI_CNFG_REG16, reg_val);
				}
			}

			mtk_dir = DMA_FROM_DEVICE;
			sg_init_one
				(&mtk_sg, temp_byte_ptr, (data_sector_num[logical_plane_num - 1]
					* (1 << host->hw->nand_sec_shift)));
			dma_map_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
			phys = mtk_sg.dma_address;
#if __INTERNAL_USE_AHB_MODE__
			if (!phys)
				pr_warn("[mtk_nand_ready_for_read]convert virt addr (%lx) to phys add (%x)fail!!!",
					(unsigned long) temp_byte_ptr, phys);
			else
				DRV_WriteReg32(NFI_STRADDR_REG32, phys);
#endif

			DRV_WriteReg32(NFI_CON_REG16, data_sector_num[logical_plane_num - 1] << CON_NFI_SEC_SHIFT);

			if (g_bHwEcc)
				ECC_Decode_Start();
#endif

			if (!mtk_nand_read_page_data
				(mtd, temp_byte_ptr, data_sector_num[logical_plane_num - 1]
					* (1 << host->hw->nand_sec_shift))) {
				MSG(INIT, "mtk_nand_read_page_data fail\n");
				bRet = ERR_RTN_FAIL;
			}
		dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
		if (!mtk_nand_status_ready(STA_NAND_BUSY)) {
			MSG(INIT, "mtk_nand_status_ready fail\n");
			bRet = ERR_RTN_FAIL;
		}

		if (g_bHwEcc) {
			if (!mtk_nand_check_dececc_done(data_sector_num[logical_plane_num - 1])) {
				MSG(INIT, "mtk_nand_check_dececc_done fail\n");
				bRet = ERR_RTN_FAIL;
			}
		}
			mtk_nand_read_fdm_data(spare_ptr, data_sector_num[logical_plane_num - 1]);
		if (g_bHwEcc) {
			if (!mtk_nand_check_bch_error
				(mtd, temp_byte_ptr, spare_ptr, data_sector_num[logical_plane_num - 1] - 1,
				u4RowAddr, NULL)) {
				if (gn_devinfo.vendor != VEND_NONE)
					readRetry = TRUE;
				MSG(INIT, "mtk_nand_check_bch_error fail, retryCount:%d\n", retryCount);
				bRet = ERR_RTN_BCH_FAIL;
			} else {
				if (0 != (DRV_Reg32(NFI_STA_REG32) & STA_READ_EMPTY) &&
					(retryCount != 0)) {
					MSG(INIT, "NFI read retry read empty page, return as uecc\n");
					mtd->ecc_stats.failed += data_sector_num[logical_plane_num - 1];
					bRet = ERR_RTN_BCH_FAIL;
				}
			}
		}
		mtk_nand_stop_read();

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
				if (gn_devinfo.tlcControl.needchangecolumn)
					DRV_WriteReg16(NFI_TLC_RD_WHR2_REG16, 0x055);

				if (2 == logical_plane_num) {
					tlc_left_plane = FALSE;
					spare_ptr += (host->hw->nand_fdm_size * data_sector_num[logical_plane_num - 1]);

#if __INTERNAL_USE_AHB_MODE__
					temp_byte_ptr +=
						(data_sector_num[logical_plane_num - 1]
							* (1 << host->hw->nand_sec_shift));
#else
					temp_byte_ptr +=
						(data_sector_num[logical_plane_num - 1]
							* ((1 << host->hw->nand_sec_shift)
							+ spare_per_sector);
#endif
				}
	}
#endif
			logical_plane_num--;

			if (bRet == ERR_RTN_BCH_FAIL)
				break;
	}
	}
#ifndef CONFIG_MTK_TLC_NAND_SUPPORT
	else
		dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);

#endif

		if (use_randomizer && u4RowAddr >= RAND_START_ADDR)
			mtk_nand_turn_off_randomizer();

	/* if(force_slc_flag == 1)
		gn_devinfo.tlcControl.slcopmodeEn = true; */

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC
		|| gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER) {
			if ((gn_devinfo.tlcControl.slcopmodeEn)
				&& (0xFF != gn_devinfo.tlcControl.dis_slc_mode_cmd)) {
				reg_val = DRV_Reg32(NFI_CON_REG16);
				reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
				/* issue reset operation */
				DRV_WriteReg32(NFI_CON_REG16, reg_val);

				reg_val = DRV_Reg16(NFI_CNFG_REG16);
				reg_val &= ~CNFG_READ_EN;
				reg_val &= ~CNFG_OP_MODE_MASK;
				reg_val |= CNFG_OP_CUST;
				DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

				mtk_nand_set_command(gn_devinfo.tlcControl.dis_slc_mode_cmd);
			}
		}
#endif
		if (bRet == ERR_RTN_BCH_FAIL) {
			u32 feature = mtk_nand_rrtry_setting(gn_devinfo,
				gn_devinfo.feature_set.FeatureSet.rtype,
				gn_devinfo.feature_set.FeatureSet.readRetryStart, retryCount);
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			if ((gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_SANDISK_TLC_1YNM)
				&& (gn_devinfo.tlcControl.slcopmodeEn))
				retrytotalcnt = 10;
			if (gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_TOSHIBA_TLC) {
				if (gn_devinfo.tlcControl.slcopmodeEn)
					retrytotalcnt = 8;
				else
					retrytotalcnt = 31;
			}
#endif

			if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER)  {
				if ((gn_devinfo.tlcControl.slcopmodeEn)
					&& (gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_SANDISK)) {
					retrytotalcnt = 22;
				}
			}

			if (retryCount < retrytotalcnt) {
				mtd->ecc_stats.corrected = backup_corrected;
				mtd->ecc_stats.failed = backup_failed;
				mtk_nand_rrtry_func(mtd, gn_devinfo, feature, FALSE);
				retryCount++;
			} else {
				feature = gn_devinfo.feature_set.FeatureSet.readRetryDefault;
				if ((gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_SANDISK)
					&& (g_sandisk_retry_case < 3)) {
					g_sandisk_retry_case++;
					pr_warn("Sandisk read retry case#%d\n", g_sandisk_retry_case);
					tempBitMap = 0;
					mtd->ecc_stats.corrected = backup_corrected;
					mtd->ecc_stats.failed = backup_failed;
					mtk_nand_rrtry_func(mtd, gn_devinfo, feature, FALSE);
					retryCount = 0;
				} else {
				mtk_nand_rrtry_func(mtd, gn_devinfo, feature, TRUE);
				readRetry = FALSE;
					g_sandisk_retry_case = 0;
				}
			}
			if ((g_sandisk_retry_case == 1) || (g_sandisk_retry_case == 3))
				mtk_nand_set_command(0x26);

		} else {
			if ((retryCount != 0) && MLC_DEVICE) {
				u32 feature = gn_devinfo.feature_set.FeatureSet.readRetryDefault;

				mtk_nand_rrtry_func(mtd, gn_devinfo, feature, TRUE);
			}
			readRetry = FALSE;
			g_sandisk_retry_case = 0;
		}
		if (TRUE == readRetry)
			bRet = ERR_RTN_SUCCESS;
	} while (readRetry);
	if (retryCount != 0) {
		u32 feature = gn_devinfo.feature_set.FeatureSet.readRetryDefault;

		if (bRet == ERR_RTN_SUCCESS) {
			MSG(INIT, "[Sector RD]u4RowAddr:0x%x read retry pass, retrycnt:%d ENUM0:%x, ENUM1:%x,\n",
				u4RowAddr, retryCount, DRV_Reg32(ECC_DECENUM1_REG32), DRV_Reg32(ECC_DECENUM0_REG32));
			mtd->ecc_stats.corrected++;
			if ((gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_HYNIX_16NM)
				|| (gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_HYNIX)) {
				g_hynix_retry_count--;
			}
		} else {
			MSG(INIT, "[Sector RD]u4RowAddr:0x%x read retry fail, mtd_ecc(A):%x , fail, mtd_ecc(B):%x\n",
				u4RowAddr, mtd->ecc_stats.failed, backup_failed);
		}
		mtk_nand_rrtry_func(mtd, gn_devinfo, feature, TRUE);
		g_sandisk_retry_case = 0;
	}

	if (buf == local_buffer_16_align)
		memcpy(pPageBuf, buf, u4PageSize);

	PFM_END_R(pfm_time_read, u4PageSize + 32);
	if (use_randomizer && u4RowAddr >= RAND_START_ADDR)
		mtk_nand_turn_off_randomizer();
	if (bRet != ERR_RTN_SUCCESS) {
		MSG(INIT, "ECC uncorrectable , fake buffer returned\n");
		memset(pPageBuf, 0xff, u4PageSize);
		memset(pFDMBuf, 0xff, u4SecNum*host->hw->nand_fdm_size);
	}
	return bRet;
}


/******************************************************************************
 * mtk_nand_exec_write_page
 *
 * DESCRIPTION:
 *Write a page data !
 *
 * PARAMETERS:
 *struct mtd_info *mtd, u32 u4RowAddr, u32 u4PageSize,
 *u8 *pPageBuf, u8 *pFDMBuf
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
int mtk_nand_exec_write_page_hw(struct mtd_info *mtd, u32 u4RowAddr, u32 u4PageSize, u8 *pPageBuf,
				 u8 *pFDMBuf)
{
	struct nand_chip *chip = mtd->priv;
	u32 u4SecNum = u4PageSize >> host->hw->nand_sec_shift;
	u8 *buf;
	u8 status;
#ifdef PWR_LOSS_SPOH
	u32 time;
	struct timeval pl_time_write;
	suseconds_t duration;
#endif
	struct NFI_TLC_WL_INFO  tlc_wl_info;
	u32 reg_val;
	u32 real_row_addr = 0;
	u32 block_addr = 0;
	u32 page_in_block = 0;
	u32 page_per_block = 0;

	page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;

	mtk_nand_interface_switch(mtd);

#ifdef _MTK_NAND_DUMMY_DRIVER_
	if (dummy_driver_debug) {
		unsigned long long time = sched_clock();

		if (!((time * 123 + 59) % 32768)) {
			pr_debug("[NAND_DUMMY_DRIVER] Simulate write error at page: 0x%x\n",
				u4RowAddr);
			return -EIO;
		}
	}
#endif

#ifdef NAND_PFM
	struct timeval pfm_time_write;
#endif
	if (((unsigned long) pPageBuf % 16) && local_buffer_16_align) {
		pr_debug("Data buffer not 16 bytes aligned: %p\n", pPageBuf);
		memcpy(local_buffer_16_align, pPageBuf, u4PageSize);
		buf = local_buffer_16_align;
	} else {
		if (virt_addr_valid(pPageBuf) == 0) {
			memcpy(local_buffer_16_align, pPageBuf, u4PageSize);
			buf = local_buffer_16_align;
		} else {
			buf = pPageBuf;
		}
	}
	PFM_BEGIN(pfm_time_write);
	tlc_wl_info.wl_pre = WL_LOW_PAGE; /* avoid compile warning */
	tlc_wl_info.word_line_idx = u4RowAddr;
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	mtk_nand_reset();
	if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)	 {
		if (gn_devinfo.tlcControl.normaltlc) {
			NFI_TLC_GetMappedWL(u4RowAddr, &tlc_wl_info);
			real_row_addr = NFI_TLC_GetRowAddr(tlc_wl_info.word_line_idx);
			if (gn_devinfo.tlcControl.pPlaneEn)
				real_row_addr = NFI_TLC_SetpPlaneAddr(real_row_addr, tlc_lg_left_plane);
		} else
			real_row_addr = NFI_TLC_GetRowAddr(u4RowAddr);

		if (gn_devinfo.tlcControl.slcopmodeEn) {
			if ((!gn_devinfo.tlcControl.pPlaneEn) || tlc_lg_left_plane) {
				if (0xFF != gn_devinfo.tlcControl.en_slc_mode_cmd) {
					reg_val = DRV_Reg16(NFI_CNFG_REG16);
					reg_val &= ~CNFG_READ_EN;
					reg_val &= ~CNFG_OP_MODE_MASK;
					reg_val |= CNFG_OP_CUST;
					DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

					mtk_nand_set_command(gn_devinfo.tlcControl.en_slc_mode_cmd);

					reg_val = DRV_Reg32(NFI_CON_REG16);
					reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
					/* issue reset operation */
					DRV_WriteReg32(NFI_CON_REG16, reg_val);
				}
			}
		} else {
			if (gn_devinfo.tlcControl.normaltlc) {
				reg_val = DRV_Reg16(NFI_CNFG_REG16);
				reg_val &= ~CNFG_READ_EN;
				reg_val &= ~CNFG_OP_MODE_MASK;
				reg_val |= CNFG_OP_CUST;
				DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

				if (PROGRAM_1ST_CYCLE == tlc_program_cycle)
					mtk_nand_set_command(PROGRAM_1ST_CYCLE_CMD);
				else if (PROGRAM_2ND_CYCLE == tlc_program_cycle)
					mtk_nand_set_command(PROGRAM_2ND_CYCLE_CMD);

				if (tlc_wl_info.wl_pre == WL_LOW_PAGE)
					mtk_nand_set_command(LOW_PG_SELECT_CMD);
				else if (tlc_wl_info.wl_pre == WL_MID_PAGE)
					mtk_nand_set_command(MID_PG_SELECT_CMD);
				else if (tlc_wl_info.wl_pre == WL_HIGH_PAGE)
					mtk_nand_set_command(HIGH_PG_SELECT_CMD);

				reg_val = DRV_Reg32(NFI_CON_REG16);
				reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
				/* issue reset operation */
				DRV_WriteReg32(NFI_CON_REG16, reg_val);
			}
		}
	} else
#endif
	{
		real_row_addr = u4RowAddr;
	}

	/* if(force_slc_flag == 1)
		gn_devinfo.tlcControl.slcopmodeEn = true; */

	if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER) {
		if (gn_devinfo.tlcControl.slcopmodeEn) {
			if (0xFF != gn_devinfo.tlcControl.en_slc_mode_cmd) {
				reg_val = DRV_Reg16(NFI_CNFG_REG16);
				reg_val &= ~CNFG_READ_EN;
				reg_val &= ~CNFG_OP_MODE_MASK;
				reg_val |= CNFG_OP_CUST;
				DRV_WriteReg16(NFI_CNFG_REG16, reg_val);
				mtk_nand_set_command(gn_devinfo.tlcControl.en_slc_mode_cmd);
				reg_val = DRV_Reg32(NFI_CON_REG16);
				reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
				/* issue reset operation */
				DRV_WriteReg32(NFI_CON_REG16, reg_val);

				if (gn_devinfo.vendor == VEND_SANDISK) {
					block_addr = real_row_addr/page_per_block;
					page_in_block = real_row_addr % page_per_block;
					page_in_block <<= 1;
					real_row_addr = page_in_block + block_addr * page_per_block;
					/* pr_err("mtk_nand_exec_read_sector SLC Mode real_row_addr:%d, u4RowAddr:%d\n",
						real_row_addr, u4RowAddr); */
				}
			}
		}  else {
			if (0xFF != gn_devinfo.tlcControl.dis_slc_mode_cmd) {
				reg_val = DRV_Reg16(NFI_CNFG_REG16);
				reg_val &= ~CNFG_READ_EN;
				reg_val &= ~CNFG_OP_MODE_MASK;
				reg_val |= CNFG_OP_CUST;
				DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

				mtk_nand_set_command(gn_devinfo.tlcControl.dis_slc_mode_cmd);
				reg_val = DRV_Reg32(NFI_CON_REG16);
				reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
				/* issue reset operation */
				DRV_WriteReg32(NFI_CON_REG16, reg_val);
			}
		}
	}
	if (use_randomizer && u4RowAddr >= RAND_START_ADDR) {
		if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
			if (gn_devinfo.tlcControl.slcopmodeEn)
				mtk_nand_turn_on_randomizer(mtd, chip, tlc_wl_info.word_line_idx);
			else
				mtk_nand_turn_on_randomizer(mtd, chip,
				(tlc_wl_info.word_line_idx*3+tlc_wl_info.wl_pre));
		} else
			mtk_nand_turn_on_randomizer(mtd, chip, u4RowAddr);
	}
	if (mtk_nand_ready_for_write(chip, real_row_addr, 0, true, buf)) {
		mtk_nand_write_fdm_data(chip, pFDMBuf, u4SecNum);
		(void)mtk_nand_write_page_data(mtd, buf, u4PageSize);
		dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
		(void)mtk_nand_check_RW_count(u4PageSize);
		mtk_nand_stop_write();
		PL_NAND_BEGIN(pl_time_write);
		PL_TIME_RAND_PROG(chip, u4RowAddr, time);
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
			if (gn_devinfo.tlcControl.normaltlc) {
				if ((gn_devinfo.tlcControl.pPlaneEn) && tlc_lg_left_plane) {
					mtk_nand_set_command(PROGRAM_LEFT_PLANE_CMD);
				} else {
					if ((tlc_wl_info.wl_pre == WL_HIGH_PAGE) || gn_devinfo.tlcControl.slcopmodeEn)
						mtk_nand_set_command(NAND_CMD_PAGEPROG);
					else
						mtk_nand_set_command(PROGRAM_RIGHT_PLANE_CMD);
				}
			} else
				mtk_nand_set_command(NAND_CMD_PAGEPROG);
		} else
#endif
		(void)mtk_nand_set_command(NAND_CMD_PAGEPROG);
		PL_NAND_RESET(time);
		{
#ifdef DUMP_PEF
			struct timeval stimer, etimer;

			do_gettimeofday(&stimer);
#endif
			while (DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY)
				;
#ifdef DUMP_PEF
			do_gettimeofday(&etimer);
			g_NandPerfLog.WriteBusyTotalTime += Cal_timediff(&etimer, &stimer);
			g_NandPerfLog.WriteBusyCount++;
#endif
		}

	PL_NAND_END(pl_time_write, duration);
	PL_TIME_PROG(duration);
	PFM_END_W(pfm_time_write, u4PageSize + 32);

	if (use_randomizer && u4RowAddr >= RAND_START_ADDR)
		mtk_nand_turn_off_randomizer();
	status = chip->waitfunc(mtd, chip);
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
			if ((gn_devinfo.tlcControl.slcopmodeEn)
				&& (0xFF != gn_devinfo.tlcControl.dis_slc_mode_cmd)) {
				reg_val = DRV_Reg32(NFI_CON_REG16);
				reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
				/* issue reset operation */
				DRV_WriteReg32(NFI_CON_REG16, reg_val);

				reg_val = DRV_Reg16(NFI_CNFG_REG16);
				reg_val &= ~CNFG_READ_EN;
				reg_val &= ~CNFG_OP_MODE_MASK;
				reg_val |= CNFG_OP_CUST;
				DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

				mtk_nand_set_command(gn_devinfo.tlcControl.dis_slc_mode_cmd);
			}
		}

		if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
			&& (gn_devinfo.tlcControl.slcopmodeEn)) {
			if (status & SLC_MODE_OP_FALI)
				return -EIO;
			else
				return 0;
		} else
#endif
		{
	if (status & NAND_STATUS_FAIL)
		return -EIO;
	else
		return 0;
}
	} else {
		dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
		pr_warn("[Bean]mtk_nand_ready_for_write fail!\n");
		if (use_randomizer && u4RowAddr >= RAND_START_ADDR)
			mtk_nand_turn_off_randomizer();
		return -EIO;
	}
}

int mtk_nand_exec_write_page(struct mtd_info *mtd, u32 u4RowAddr, u32 u4PageSize, u8 *pPageBuf, u8 *pFDMBuf)
{
	int bRet;
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	u8 *temp_page_buf = NULL;
	u8 *temp_fdm_buf = NULL;
	u32 u4SecNum = u4PageSize >> host->hw->nand_sec_shift;

	if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)	 {
		if ((gn_devinfo.tlcControl.normaltlc) && (gn_devinfo.tlcControl.pPlaneEn)) {
			tlc_lg_left_plane = TRUE;
			temp_page_buf = pPageBuf;
			temp_fdm_buf = pFDMBuf;

			bRet = mtk_nand_exec_write_page_hw(mtd, u4RowAddr, u4PageSize / 2, temp_page_buf, temp_fdm_buf);
			{
				return bRet;
			}

			tlc_lg_left_plane = FALSE;
			temp_page_buf += (u4PageSize / 2);
			temp_fdm_buf += ((u4SecNum / 2) * host->hw->nand_fdm_size);

			bRet = mtk_nand_exec_write_page_hw(mtd, u4RowAddr, u4PageSize / 2, temp_page_buf, temp_fdm_buf);
		} else {
			bRet = mtk_nand_exec_write_page_hw(mtd, u4RowAddr, u4PageSize, pPageBuf, pFDMBuf);
		}
	} else
#endif
	{
		bRet = mtk_nand_exec_write_page_hw(mtd, u4RowAddr, u4PageSize, pPageBuf, pFDMBuf);
	}
	return bRet;
}
/******************************************************************************
 *
 * Write a page to a logical address
 *
 *****************************************************************************/
static int mtk_nand_write_page(struct mtd_info *mtd, struct nand_chip *chip,
				uint32_t offset, int data_len, const uint8_t *buf,
				int oob_required, int page, int cached, int raw)
{
	int page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;
	u32 block;
	u32 page_in_block;
	u32 mapped_block;
#ifdef DUMP_PEF
	struct timeval stimer, etimer;

	do_gettimeofday(&stimer);
#endif
#ifdef __D_PFM__
	struct timeval pfm_time_write;
#endif

	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);
	/*pr_err("[WRITE] %d, %d, %d %d\n", mapped_block, block, page_in_block, page_per_block);*/
	if (mapped_block != block)
		set_bad_index_to_oob(chip->oob_poi, block);
	else
		set_bad_index_to_oob(chip->oob_poi, FAKE_INDEX);
#ifdef __D_PFM__
		PFM_DBEGIN(pfm_time_write);
#endif

	if (mtk_nand_exec_write_page
		(mtd, page_in_block + mapped_block * page_per_block, mtd->writesize, (u8 *) buf,
		 chip->oob_poi)) {
		MSG(INIT, "write fail at block: 0x%x, page: 0x%x\n", mapped_block, page_in_block);
		if (update_bmt
			((u64) ((u64) page_in_block + (u64) mapped_block * page_per_block) << chip->
			 page_shift, UPDATE_WRITE_FAIL, (u8 *) buf, chip->oob_poi)) {
			MSG(INIT, "Update BMT success\n");
		} else {
			MSG(INIT, "Update BMT fail\n");
			return -EIO;
		}
		return 0;
	}
#ifdef __D_PFM__
	PFM_END_DW(pfm_time_write, mtd->writesize);
#endif

#ifdef DUMP_PEF
	do_gettimeofday(&etimer);
	g_NandPerfLog.WritePageTotalTime += Cal_timediff(&etimer, &stimer);
	g_NandPerfLog.WritePageCount++;
	dump_nand_rwcount();
#endif
	return 0;
}

/*-------------------------------------------------------------------------------*/
/*
static void mtk_nand_command_sp(struct mtd_info *mtd, unsigned int command, int column, int page_addr)
{
	g_u4ColAddr	= column;
	g_u4RowAddr	= page_addr;

	switch (command) {
	case NAND_CMD_STATUS:
		break;

	case NAND_CMD_READID:
		break;

	case NAND_CMD_RESET:
		break;

	case NAND_CMD_RNDOUT:
	case NAND_CMD_RNDOUTSTART:
	case NAND_CMD_RNDIN:
	case NAND_CMD_CACHEDPROG:
	case NAND_CMD_STATUS_MULTI:
	default:
		break;
	}

}
*/

/******************************************************************************
 * mtk_nand_command_bp
 *
 * DESCRIPTION:
 *Handle the commands from MTD !
 *
 * PARAMETERS:
 *struct mtd_info *mtd, unsigned int command, int column, int page_addr
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void mtk_nand_command_bp(struct mtd_info *mtd, unsigned int command, int column,
				int page_addr)
{
	struct nand_chip *nand = mtd->priv;
#ifdef NAND_PFM
	struct timeval pfm_time_erase;
#endif
	switch (command) {
	case NAND_CMD_SEQIN:
	  memset(g_kCMD.au1OOB, 0xFF, sizeof(g_kCMD.au1OOB));
	  g_kCMD.pDataBuf = NULL;
	  g_kCMD.u4RowAddr = page_addr;
	  g_kCMD.u4ColAddr = column;
	  break;

	case NAND_CMD_PAGEPROG:
	if (g_kCMD.pDataBuf || (0xFF != g_kCMD.au1OOB[0])) {
		u8 *pDataBuf = g_kCMD.pDataBuf ? g_kCMD.pDataBuf : nand->buffers->databuf;
		if (((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
			|| (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER))
				&& (gn_devinfo.tlcControl.normaltlc)
				&& (!mtk_block_istlc(g_kCMD.u4RowAddr * mtd->writesize)))
			gn_devinfo.tlcControl.slcopmodeEn = TRUE;
		else
			gn_devinfo.tlcControl.slcopmodeEn = FALSE;

		mtk_nand_exec_write_page(mtd, g_kCMD.u4RowAddr, mtd->writesize, pDataBuf,
					 g_kCMD.au1OOB);
		g_kCMD.u4RowAddr = (u32) -1;
		g_kCMD.u4OOBRowAddr = (u32) -1;
	  }
	  break;

	case NAND_CMD_READOOB:
	  g_kCMD.u4RowAddr = page_addr;
	  g_kCMD.u4ColAddr = column + mtd->writesize;
#ifdef NAND_PFM
	  g_kCMD.pureReadOOB = 1;
	  g_kCMD.pureReadOOBNum += 1;
#endif
	  break;

	case NAND_CMD_READ0:
	  g_kCMD.u4RowAddr = page_addr;
	  g_kCMD.u4ColAddr = column;
#ifdef NAND_PFM
	  g_kCMD.pureReadOOB = 0;
#endif
	  break;

	case NAND_CMD_ERASE1:
	  PFM_BEGIN(pfm_time_erase);
	  (void)mtk_nand_reset();
	  mtk_nand_set_mode(CNFG_OP_ERASE);
	  (void)mtk_nand_set_command(NAND_CMD_ERASE1);
		(void)mtk_nand_set_address(0, page_addr, 0, gn_devinfo.addr_cycle - 2);
	  break;

	case NAND_CMD_ERASE2:
	  (void)mtk_nand_set_command(NAND_CMD_ERASE2);
	while (DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY)
		;
	  PFM_END_E(pfm_time_erase);
	  break;

	case NAND_CMD_STATUS:
		mtk_nand_interface_switch(mtd);
	  (void)mtk_nand_reset();
		if (mtk_nand_israndomizeron()) {
			g_brandstatus = TRUE;
			mtk_nand_turn_off_randomizer();
	  }
	  NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);
	  mtk_nand_set_mode(CNFG_OP_SRD);
	  mtk_nand_set_mode(CNFG_READ_EN);
	  NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);
	  NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
	  (void)mtk_nand_set_command(NAND_CMD_STATUS);
	  NFI_CLN_REG32(NFI_CON_REG16, CON_NFI_NOB_MASK);
		mb();		/*make sure process order */
	  DRV_WriteReg32(NFI_CON_REG16, CON_NFI_SRD | (1 << CON_NFI_NOB_SHIFT));
	  g_bcmdstatus = true;
	  break;

	case NAND_CMD_RESET:
	  (void)mtk_nand_reset();
	  break;

	case NAND_CMD_READID:
	  /* Issue NAND chip reset command */

	  mtk_nand_reset();
	  /* Disable HW ECC */
	  NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
	  NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);

	  /* Disable 16-bit I/O */
	/*NFI_CLN_REG16(NFI_PAGEFMT_REG16, PAGEFMT_DBYTE_EN); */

	  NFI_SET_REG16(NFI_CNFG_REG16, CNFG_READ_EN | CNFG_BYTE_RW);
	  (void)mtk_nand_reset();
		mb();		/*make sure process order */
	  mtk_nand_set_mode(CNFG_OP_SRD);
	  (void)mtk_nand_set_command(NAND_CMD_READID);
	  (void)mtk_nand_set_address(0, 0, 1, 0);
	  DRV_WriteReg32(NFI_CON_REG16, CON_NFI_SRD);
		while (DRV_Reg32(NFI_STA_REG32) & STA_DATAR_STATE)
			;
	  break;

	default:
	  BUG();
	  break;
	}
}

/******************************************************************************
 * mtk_nand_select_chip
 *
 * DESCRIPTION:
 *Select a chip !
 *
 * PARAMETERS:
 *struct mtd_info *mtd, int chip
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void mtk_nand_select_chip(struct mtd_info *mtd, int chip)
{
	if (chip == -1 && false == g_bInitDone) {
		struct nand_chip *nand = mtd->priv;

	struct mtk_nand_host *host = nand->priv;
	struct mtk_nand_host_hw *hw = host->hw;
	u32 spare_per_sector = mtd->oobsize / (mtd->writesize / hw->nand_sec_size);
	u32 ecc_bit = 4;
	u32 spare_bit = PAGEFMT_SPARE_16;

	hw->nand_fdm_size = 8;
		switch (spare_per_sector) {
		case 16:
			spare_bit = PAGEFMT_SPARE_16;
			ecc_bit = 4;
			spare_per_sector = 16;
			break;
		case 26:
		case 27:
		case 28:
		spare_bit = PAGEFMT_SPARE_26;
			ecc_bit = 10;
		spare_per_sector = 26;
			break;
		case 32:
			ecc_bit = 12;
			if (MLC_DEVICE == TRUE)
				spare_bit = PAGEFMT_SPARE_32_1KS;
			else
				spare_bit = PAGEFMT_SPARE_32;
			spare_per_sector = 32;
			break;
		case 40:
			ecc_bit = 18;
			spare_bit = PAGEFMT_SPARE_40;
			spare_per_sector = 40;
			break;
		case 44:
			ecc_bit = 20;
			spare_bit = PAGEFMT_SPARE_44;
			spare_per_sector = 44;
			break;
		case 48:
		case 49:
			ecc_bit = 22;
			spare_bit = PAGEFMT_SPARE_48;
			spare_per_sector = 48;
			break;
		case 50:
		case 51:
			ecc_bit = 24;
			spare_bit = PAGEFMT_SPARE_50;
			spare_per_sector = 50;
			break;
		case 52:
		case 54:
		case 56:
			ecc_bit = 24;
			if (MLC_DEVICE == TRUE)
				spare_bit = PAGEFMT_SPARE_52_1KS;
			else
				spare_bit = PAGEFMT_SPARE_52;
			spare_per_sector = 32;
			break;
		case 62:
		case 63:
			ecc_bit = 28;
			spare_bit = PAGEFMT_SPARE_62;
			spare_per_sector = 62;
			break;
		case 64:
			ecc_bit = 32;
			if (MLC_DEVICE == TRUE)
				spare_bit = PAGEFMT_SPARE_64_1KS;
			else
				spare_bit = PAGEFMT_SPARE_64;
			spare_per_sector = 64;
			break;
		case 72:
			ecc_bit = 36;
			if (MLC_DEVICE == TRUE)
				spare_bit = PAGEFMT_SPARE_72_1KS;
			spare_per_sector = 72;
			break;
		case 80:
			ecc_bit = 40;
			if (MLC_DEVICE == TRUE)
				spare_bit = PAGEFMT_SPARE_80_1KS;
			spare_per_sector = 80;
			break;
		case 88:
			ecc_bit = 44;
			if (MLC_DEVICE == TRUE)
				spare_bit = PAGEFMT_SPARE_88_1KS;
			spare_per_sector = 88;
			break;
		case 96:
		case 98:
			ecc_bit = 48;
			if (MLC_DEVICE == TRUE)
				spare_bit = PAGEFMT_SPARE_96_1KS;
			spare_per_sector = 96;
			break;
		case 100:
		case 102:
		case 104:
			ecc_bit = 52;
			if (MLC_DEVICE == TRUE)
				spare_bit = PAGEFMT_SPARE_100_1KS;
			spare_per_sector = 100;
			break;
		case 122:
		case 124:
		case 126:
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
				&& gn_devinfo.tlcControl.ecc_recalculate_en) {
				if (60 < gn_devinfo.tlcControl.ecc_required) {
					hw->nand_fdm_size = 3;
					ecc_bit = 68;
				} else
					ecc_bit = 60;
			} else
#endif
				ecc_bit = 60;
			if (hw->nand_sec_size == 1024)
				spare_bit = PAGEFMT_SPARE_122_1KS;
			spare_per_sector = 122;
			break;
		case 128:
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
				&& gn_devinfo.tlcControl.ecc_recalculate_en) {
				if (68 < gn_devinfo.tlcControl.ecc_required) {
					hw->nand_fdm_size = 2;
					ecc_bit = 72;
				} else
					ecc_bit = 68;
			} else
#endif
				ecc_bit = 68;
			if (hw->nand_sec_size == 1024)
				spare_bit = PAGEFMT_SPARE_128_1KS;
			spare_per_sector = 128;
			break;
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		case 134:
			ecc_bit = 72;
			if (hw->nand_sec_size == 1024)
				spare_bit = PAGEFMT_SPARE_134_1KS;
			spare_per_sector = 134;
			break;
		case 148:
			ecc_bit = 80;
			if (hw->nand_sec_size == 1024)
				spare_bit = PAGEFMT_SPARE_148_1KS;
			spare_per_sector = 148;
			break;
#endif
		default:
		MSG(INIT, "[NAND]: NFI not support oobsize: %x\n", spare_per_sector);
			ASSERT(0);
	}

	 mtd->oobsize = spare_per_sector * (mtd->writesize / hw->nand_sec_size);
		pr_err("[NAND]select ecc bit: %d, sparesize: %d\n", ecc_bit, mtd->oobsize);
		/* Setup PageFormat */

		if (16384 == mtd->writesize) {
			NFI_SET_REG32(NFI_PAGEFMT_REG16,
					  (spare_bit << PAGEFMT_SPARE_SHIFT) | PAGEFMT_16K_1KS);
			nand->cmdfunc = mtk_nand_command_bp;
		} else if (8192 == mtd->writesize) {
			NFI_SET_REG32(NFI_PAGEFMT_REG16,
					  (spare_bit << PAGEFMT_SPARE_SHIFT) | PAGEFMT_8K_1KS);
			nand->cmdfunc = mtk_nand_command_bp;
		} else if (4096 == mtd->writesize) {
			if (MLC_DEVICE == FALSE)
				NFI_SET_REG32(NFI_PAGEFMT_REG16,
						  (spare_bit << PAGEFMT_SPARE_SHIFT) | PAGEFMT_4K);
			else
				NFI_SET_REG32(NFI_PAGEFMT_REG16,
						  (spare_bit << PAGEFMT_SPARE_SHIFT) | PAGEFMT_4K_1KS);
			nand->cmdfunc = mtk_nand_command_bp;
		} else if (2048 == mtd->writesize) {
			if (MLC_DEVICE == FALSE)
				NFI_SET_REG32(NFI_PAGEFMT_REG16,
						  (spare_bit << PAGEFMT_SPARE_SHIFT) | PAGEFMT_2K);
			else
				NFI_SET_REG32(NFI_PAGEFMT_REG16,
						  (spare_bit << PAGEFMT_SPARE_SHIFT) | PAGEFMT_2K_1KS);
			nand->cmdfunc = mtk_nand_command_bp;
		}

	mtk_nand_configure_fdm(hw->nand_fdm_size);

	 ecc_threshold = ecc_bit*4/5;
	 ECC_Config(hw, ecc_bit);
		g_bInitDone = true;

		/*xiaolei for kernel3.10 */
	 nand->ecc.strength = ecc_bit;
	 mtd->bitflip_threshold = nand->ecc.strength;
	}
	switch (chip) {
	case -1:
		  break;
	case 0:
#ifdef CFG_FPGA_PLATFORM	/* FPGA NAND is placed at CS1 not CS0 */
			DRV_WriteReg16(NFI_CSEL_REG16, 0);
			break;
#endif
	case 1:
		if (chip != 0)
			pr_warn("[BUG!!!]select_chip %d\n", chip);
		DRV_WriteReg16(NFI_CSEL_REG16, chip);
		break;
	}
}

/******************************************************************************
 * mtk_nand_read_byte
 *
 * DESCRIPTION:
 *Read a byte of data !
 *
 * PARAMETERS:
 *struct mtd_info *mtd
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static uint8_t mtk_nand_read_byte(struct mtd_info *mtd)
{
	uint8_t retval = 0;

	if (!mtk_nand_pio_ready()) {
		pr_err("pio ready timeout\n");
		retval = false;
	}

	if (g_bcmdstatus) {
		retval = DRV_Reg8(NFI_DATAR_REG32);
		NFI_CLN_REG32(NFI_CON_REG16, CON_NFI_NOB_MASK);
		mtk_nand_reset();
#if (__INTERNAL_USE_AHB_MODE__)
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_AHB);
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
#endif
		if (g_bHwEcc)
			NFI_SET_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
		else
			NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);

		g_bcmdstatus = false;
	} else
		retval = DRV_Reg8(NFI_DATAR_REG32);

	return retval;
}

/******************************************************************************
 * mtk_nand_read_buf
 *
 * DESCRIPTION:
 *Read NAND data !
 *
 * PARAMETERS:
 *struct mtd_info *mtd, uint8_t *buf, int len
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void mtk_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct nand_chip *nand = (struct nand_chip *)mtd->priv;
	struct NAND_CMD *pkCMD = &g_kCMD;
	u32 u4ColAddr = pkCMD->u4ColAddr;
	u32 u4PageSize = mtd->writesize;

	if (((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		|| (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER))
		 && (gn_devinfo.tlcControl.normaltlc) && (!mtk_block_istlc(pkCMD->u4RowAddr * mtd->writesize)))
		gn_devinfo.tlcControl.slcopmodeEn = TRUE;
	else
		gn_devinfo.tlcControl.slcopmodeEn = FALSE;

	if (u4ColAddr < u4PageSize) {
		if ((u4ColAddr == 0) && (len >= u4PageSize)) {
			mtk_nand_exec_read_page(mtd, pkCMD->u4RowAddr, u4PageSize, buf,
						pkCMD->au1OOB);
			if (len > u4PageSize) {
				u32 u4Size = min(len - u4PageSize, sizeof(pkCMD->au1OOB));

				memcpy(buf + u4PageSize, pkCMD->au1OOB, u4Size);
			}
		} else {
			mtk_nand_exec_read_page(mtd, pkCMD->u4RowAddr, u4PageSize,
						nand->buffers->databuf, pkCMD->au1OOB);
			memcpy(buf, nand->buffers->databuf + u4ColAddr, len);
		}
		pkCMD->u4OOBRowAddr = pkCMD->u4RowAddr;
	} else {
		u32 u4Offset = u4ColAddr - u4PageSize;
		u32 u4Size = min(len - u4Offset, sizeof(pkCMD->au1OOB));

		if (pkCMD->u4OOBRowAddr != pkCMD->u4RowAddr) {
			mtk_nand_exec_read_page(mtd, pkCMD->u4RowAddr, u4PageSize,
						nand->buffers->databuf, pkCMD->au1OOB);
			pkCMD->u4OOBRowAddr = pkCMD->u4RowAddr;
		}
		memcpy(buf, pkCMD->au1OOB + u4Offset, u4Size);
	}
	pkCMD->u4ColAddr += len;
}

/******************************************************************************
 * mtk_nand_write_buf
 *
 * DESCRIPTION:
 *Write NAND data !
 *
 * PARAMETERS:
 *struct mtd_info *mtd, const uint8_t *buf, int len
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void mtk_nand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct NAND_CMD *pkCMD = &g_kCMD;
	u32 u4ColAddr = pkCMD->u4ColAddr;
	u32 u4PageSize = mtd->writesize;
	int i4Size, i;

	if (u4ColAddr >= u4PageSize) {
		u32 u4Offset = u4ColAddr - u4PageSize;
		u8 *pOOB = pkCMD->au1OOB + u4Offset;

		i4Size = min(len, (int)(sizeof(pkCMD->au1OOB) - u4Offset));

		for (i = 0; i < i4Size; i++)
			pOOB[i] &= buf[i];
	} else {
		pkCMD->pDataBuf = (u8 *) buf;
	}

	pkCMD->u4ColAddr += len;
}

/******************************************************************************
 * mtk_nand_write_page_hwecc
 *
 * DESCRIPTION:
 *Write NAND data with hardware ecc !
 *
 * PARAMETERS:
 *struct mtd_info *mtd, struct nand_chip *chip, const uint8_t *buf
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static int mtk_nand_write_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
					  const uint8_t *buf, int oob_required)
{
	mtk_nand_write_buf(mtd, buf, mtd->writesize);
	mtk_nand_write_buf(mtd, chip->oob_poi, mtd->oobsize);
	return 0;
}

/******************************************************************************
 * mtk_nand_read_page_hwecc
 *
 * DESCRIPTION:
 *Read NAND data with hardware ecc !
 *
 * PARAMETERS:
 *struct mtd_info *mtd, struct nand_chip *chip, uint8_t *buf
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static int mtk_nand_read_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip, uint8_t *buf,
					int oob_required, int page)
{
#if 0
	mtk_nand_read_buf(mtd, buf, mtd->writesize);
	mtk_nand_read_buf(mtd, chip->oob_poi, mtd->oobsize);
#else
	struct NAND_CMD *pkCMD = &g_kCMD;
	u32 u4ColAddr = pkCMD->u4ColAddr;
	u32 u4PageSize = mtd->writesize;

	if (((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		|| (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER))
		 && (gn_devinfo.tlcControl.normaltlc) && (!mtk_block_istlc(pkCMD->u4RowAddr * mtd->writesize)))
		gn_devinfo.tlcControl.slcopmodeEn = TRUE;
	else
		gn_devinfo.tlcControl.slcopmodeEn = FALSE;

	if (u4ColAddr == 0) {
		mtk_nand_exec_read_page(mtd, pkCMD->u4RowAddr, u4PageSize, buf, chip->oob_poi);
		pkCMD->u4ColAddr += u4PageSize + mtd->oobsize;
	}
#endif
	return 0;
}

/******************************************************************************
 *
 * Read a page to a logical address
 *
 *****************************************************************************/
static int mtk_nand_read_page(struct mtd_info *mtd, struct nand_chip *chip, u8 *buf, int page)
{
	int page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;
	u32 block;
	u32 page_in_block;
	u32 mapped_block;
	int bRet = ERR_RTN_SUCCESS;
#ifdef __D_PFM__
	int sec_num = 1<<(chip->page_shift - host->hw->nand_sec_shift);
	struct timeval pfm_time_read;
#endif
#ifdef DUMP_PEF
	struct timeval stimer, etimer;

	do_gettimeofday(&stimer);
#endif
	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);
	/*MSG(INIT, "[WRITE] %d, %d, %d %d\n", mapped_block, block, page_in_block, page_per_block); */
#ifdef __D_PFM__
	PFM_DBEGIN(pfm_time_read);
#endif
	bRet =
		mtk_nand_exec_read_page(mtd, page_in_block + mapped_block * page_per_block,
					mtd->writesize, buf, chip->oob_poi);
#ifdef __D_PFM__
	PFM_END_DR(pfm_time_read, mtd->writesize, sec_num);
#endif
	if (bRet == ERR_RTN_SUCCESS) {
#ifdef DUMP_PEF
		do_gettimeofday(&etimer);
		g_NandPerfLog.ReadPageTotalTime += Cal_timediff(&etimer, &stimer);
		g_NandPerfLog.ReadPageCount++;
		dump_nand_rwcount();
#endif
#ifdef CFG_SNAND_ACCESS_PATTERN_LOGGER
		if (g_snand_pm_on == 1)
			mtk_snand_pm_add_drv_record(_SNAND_PM_OP_READ_PAGE,
							page_in_block + mapped_block * page_per_block,
							0, Cal_timediff(&etimer, &stimer));
#endif
		return 0;
	}

	/* else
	return -EIO; */
	return 0;
}

static int mtk_nand_read_subpage(struct mtd_info *mtd, struct nand_chip *chip, u8 *buf, int page,
				 int subpage, int subpageno)
{
	int page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;
	u32 block;
	int coladdr;
	u32 page_in_block;
	u32 mapped_block;
	int bRet = ERR_RTN_SUCCESS;
	int sec_num = 1<<(chip->page_shift - host->hw->nand_sec_shift);
	int spare_per_sector = mtd->oobsize / sec_num;
#ifdef DUMP_PEF
	struct timeval stimer, etimer;

	do_gettimeofday(&stimer);
#endif
#ifdef __D_PFM__
		struct timeval pfm_time_read;
#endif

	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);
	coladdr = subpage * (gn_devinfo.sectorsize + spare_per_sector);
#ifdef __D_PFM__
		PFM_DBEGIN(pfm_time_read);
#endif

	bRet =
		mtk_nand_exec_read_sector(mtd, page_in_block + mapped_block * page_per_block, coladdr,
					  gn_devinfo.sectorsize * subpageno, buf, chip->oob_poi,
					  subpageno);
#ifdef __D_PFM__
		PFM_END_DRS(pfm_time_read, gn_devinfo.sectorsize * subpageno, subpageno);
#endif

	if (bRet == ERR_RTN_SUCCESS) {
#ifdef DUMP_PEF
		do_gettimeofday(&etimer);
		g_NandPerfLog.ReadSubPageTotalTime += Cal_timediff(&etimer, &stimer);
		g_NandPerfLog.ReadSubPageCount++;
		dump_nand_rwcount();
#endif
#ifdef CFG_SNAND_ACCESS_PATTERN_LOGGER
		if (g_snand_pm_on == 1)
			mtk_snand_pm_add_drv_record(_SNAND_PM_OP_READ_SEC,
							page_in_block + mapped_block * page_per_block,
							subpage, Cal_timediff(&etimer, &stimer));
#endif
		return 0;
	}

	/* else
	return -EIO; */
	if (block == 62)
		mtk_nand_read_subpage(mtd, chip, buf, page++, subpage, subpageno);
	return 0;
}


/******************************************************************************
 *
 * Erase a block at a logical address
 *
 *****************************************************************************/
int mtk_nand_erase_hw(struct mtd_info *mtd, int page)
{
#ifdef PWR_LOSS_SPOH
	struct timeval pl_time_write;
	suseconds_t duration;
	u32 time;
#endif
	int result;
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	struct NFI_TLC_WL_INFO  tlc_wl_info;
#endif
	u32 reg_val = 0;
	u32 real_row_addr = 0;

#ifdef _MTK_NAND_DUMMY_DRIVER_
	if (dummy_driver_debug) {
		unsigned long long time = sched_clock();

		if (!((time * 123 + 59) % 1024)) {
			pr_debug("[NAND_DUMMY_DRIVER] Simulate erase error at page: 0x%x\n", page);
			return NAND_STATUS_FAIL;
		}
	}
#endif

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
		if (gn_devinfo.tlcControl.normaltlc) {
			NFI_TLC_GetMappedWL(page, &tlc_wl_info);
			real_row_addr = NFI_TLC_GetRowAddr(tlc_wl_info.word_line_idx);
		} else {
			real_row_addr = NFI_TLC_GetRowAddr(page);
		}
	} else
#endif
	{
		real_row_addr = page;
	}

	/* if(force_slc_flag == 1)
		gn_devinfo.tlcControl.slcopmodeEn = true; */

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
		if ((gn_devinfo.tlcControl.slcopmodeEn)
			&& (0xFF != gn_devinfo.tlcControl.en_slc_mode_cmd)) {
			reg_val = DRV_Reg16(NFI_CNFG_REG16);
			reg_val &= ~CNFG_READ_EN;
			reg_val &= ~CNFG_OP_MODE_MASK;
			reg_val |= CNFG_OP_CUST;
			DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

			mtk_nand_set_command(gn_devinfo.tlcControl.en_slc_mode_cmd);

			reg_val = DRV_Reg32(NFI_CON_REG16);
			reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
			/* issue reset operation */
			DRV_WriteReg32(NFI_CON_REG16, reg_val);
		} else {
			if (tlc_not_keep_erase_lvl) {
				reg_val = DRV_Reg16(NFI_CNFG_REG16);
				reg_val &= ~CNFG_READ_EN;
				reg_val &= ~CNFG_OP_MODE_MASK;
				reg_val |= CNFG_OP_CUST;
				DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

				mtk_nand_set_command(NOT_KEEP_ERASE_LVL_A19NM_CMD);

				reg_val = DRV_Reg32(NFI_CON_REG16);
				reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
				/* issue reset operation */
				DRV_WriteReg32(NFI_CON_REG16, reg_val);
			}
		}
	}
#endif

	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER) && (gn_devinfo.vendor == VEND_SANDISK)) {
		if (gn_devinfo.tlcControl.slcopmodeEn) {
			if (0xFF != gn_devinfo.tlcControl.en_slc_mode_cmd) {
				reg_val = DRV_Reg16(NFI_CNFG_REG16);
				reg_val &= ~CNFG_READ_EN;
				reg_val &= ~CNFG_OP_MODE_MASK;
				reg_val |= CNFG_OP_CUST;
				DRV_WriteReg16(NFI_CNFG_REG16, reg_val);
				mtk_nand_set_command(gn_devinfo.tlcControl.en_slc_mode_cmd);
				reg_val = DRV_Reg32(NFI_CON_REG16);
				reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
				/* issue reset operation */
				DRV_WriteReg32(NFI_CON_REG16, reg_val);
				/* pr_err("mtk_nand_erase_hw SLC Mode %d\n", real_row_addr); */
			}
		}  else {
			if (0xFF != gn_devinfo.tlcControl.dis_slc_mode_cmd) {
				reg_val = DRV_Reg16(NFI_CNFG_REG16);
				reg_val &= ~CNFG_READ_EN;
				reg_val &= ~CNFG_OP_MODE_MASK;
				reg_val |= CNFG_OP_CUST;
				DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

				mtk_nand_set_command(gn_devinfo.tlcControl.dis_slc_mode_cmd);
				reg_val = DRV_Reg32(NFI_CON_REG16);
				reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
				/* issue reset operation */
				DRV_WriteReg32(NFI_CON_REG16, reg_val);
			}
		}
	}

	PL_NAND_BEGIN(pl_time_write);
	PL_TIME_RAND_ERASE(chip, page, time);
	chip->erase(mtd, real_row_addr);
	PL_NAND_RESET(time);
	result = chip->waitfunc(mtd, chip);
	PL_NAND_END(pl_time_write, duration);
	PL_TIME_ERASE(duration);

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		&& (gn_devinfo.tlcControl.slcopmodeEn)) {
		if (0xFF != gn_devinfo.tlcControl.dis_slc_mode_cmd) {
			reg_val = DRV_Reg32(NFI_CON_REG16);
			reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
			/* issue reset operation */
			DRV_WriteReg32(NFI_CON_REG16, reg_val);

			reg_val = DRV_Reg16(NFI_CNFG_REG16);
			reg_val &= ~CNFG_READ_EN;
			reg_val &= ~CNFG_OP_MODE_MASK;
			reg_val |= CNFG_OP_CUST;
			DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

			mtk_nand_set_command(gn_devinfo.tlcControl.dis_slc_mode_cmd);
		}
	}
#endif

	return result;
}

static int mtk_nand_erase(struct mtd_info *mtd, int page)
{
	int status;
	struct nand_chip *chip = mtd->priv;
	int page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;
	u32 block;
	u32 page_in_block;
	u32 mapped_block;
	bool erase_fail = FALSE;
#ifdef DUMP_PEF
	struct timeval stimer, etimer;

	do_gettimeofday(&stimer);
#endif
#ifdef __D_PFM__
	struct timeval pfm_time_erase;

	PFM_DBEGIN(pfm_time_erase);
#endif

	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);
	/*pr_err("[ERASE] 0x%x 0x%x\n", mapped_block, page);*/
	status = mtk_nand_erase_hw(mtd, page_in_block + page_per_block * mapped_block);

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		&& (gn_devinfo.tlcControl.slcopmodeEn)) {
		if (status & SLC_MODE_OP_FALI) {
			erase_fail = TRUE;
			MSG(INIT, "mtk_nand_erase: page %d fail\n", page);
		}
	} else
#endif
	{
	if (status & NAND_STATUS_FAIL)
			erase_fail = TRUE;
	}

	if (erase_fail) {
		if (update_bmt
			((u64) ((u64) page_in_block + (u64) mapped_block * page_per_block) <<
			 chip->page_shift, UPDATE_ERASE_FAIL, NULL, NULL)) {
			MSG(INIT, "Erase fail at block: 0x%x, update BMT success\n", mapped_block);
		} else {
			MSG(INIT, "Erase fail at block: 0x%x, update BMT fail\n", mapped_block);
			return NAND_STATUS_FAIL;
		}
	}
#ifdef __D_PFM__
	PFM_END_DE(pfm_time_erase);
#endif
#ifdef DUMP_PEF
	do_gettimeofday(&etimer);
	g_NandPerfLog.EraseBlockTotalTime += Cal_timediff(&etimer, &stimer);
	g_NandPerfLog.EraseBlockCount++;
	dump_nand_rwcount();
#endif
#ifdef CFG_SNAND_ACCESS_PATTERN_LOGGER
	if (g_snand_pm_on == 1)
		mtk_snand_pm_add_drv_record(_SNAND_PM_OP_ERASE,
						page_in_block + page_per_block * mapped_block, 0,
						Cal_timediff(&etimer, &stimer));
#endif

	return 0;
}

/******************************************************************************
 * mtk_nand_read_multi_page_cache
 *
 * description:
 *read multi page data using cache read
 *
 * parameters:
 *struct mtd_info *mtd, struct nand_chip *chip, int page, struct mtd_oob_ops *ops
 *
 * returns:
 *none
 *
 * notes:
 *only available for nand flash support cache read.
 *read main data only.
 *
 *****************************************************************************/
#if 0
static int mtk_nand_read_multi_page_cache(struct mtd_info *mtd, struct nand_chip *chip, int page,
					  struct mtd_oob_ops *ops)
{
	int res = -EIO;
	int len = ops->len;
	struct mtd_ecc_stats stat = mtd->ecc_stats;
	uint8_t *buf = ops->datbuf;

	if (!mtk_nand_ready_for_read(chip, page, 0, true, buf))
		return -EIO;

	while (len > 0) {
		mtk_nand_set_mode(CNFG_OP_CUST);
		DRV_WriteReg32(NFI_CON_REG16, 8 << CON_NFI_SEC_SHIFT);

		if (len > mtd->writesize) {
			if (!mtk_nand_set_command(0x31))	/* todo: add cache read command */
				goto ret;
		} else {
			if (!mtk_nand_set_command(0x3f))	/* last page remained */
				goto ret;
		}

		mtk_nand_status_ready(STA_NAND_BUSY);

#ifdef __INTERNAL_USE_AHB_MODE__
		if (!mtk_nand_read_page_data(mtd, buf, mtd->writesize))
			goto ret;
#else
		if (!mtk_nand_mcu_read_data(buf, mtd->writesize))
			goto ret;
#endif

		/* get ecc error info */
		mtk_nand_check_bch_error(mtd, buf, 3, page);
		ECC_Decode_End();

		page++;
		len -= mtd->writesize;
		buf += mtd->writesize;
		ops->retlen += mtd->writesize;

		if (len > 0) {
			ECC_Decode_Start();
			mtk_nand_reset();
		}

	}

	res = 0;

ret:
	mtk_nand_stop_read();

	if (res)
		return res;

	if (mtd->ecc_stats.failed > stat.failed) {
		pr_debug("ecc fail happened\n");
		return -EBADMSG;
	}

	return mtd->ecc_stats.corrected - stat.corrected ? -EUCLEAN : 0;
}
#endif

/******************************************************************************
 * mtk_nand_read_oob_raw
 *
 * DESCRIPTION:
 *Read oob data
 *
 * PARAMETERS:
 *struct mtd_info *mtd, const uint8_t *buf, int addr, int len
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *this function read raw oob data out of flash, so need to re-organise
 *data format before using.
 *len should be times of 8, call this after nand_get_device.
 *Should notice, this function read data without ECC protection.
 *
 *****************************************************************************/
static int mtk_nand_read_oob_raw(struct mtd_info *mtd, uint8_t *buf, int page_addr, int len)
{
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	u32 col_addr = 0;
	u32 sector = 0;
	int res = 0;
	u32 colnob = 2, rawnob = gn_devinfo.addr_cycle - 2;
	int randomread = 0;
	int read_len = 0;
	int sec_num = 1<<(chip->page_shift - host->hw->nand_sec_shift);
	int spare_per_sector = mtd->oobsize / sec_num;
	u32 sector_size = NAND_SECTOR_SIZE;

	if (gn_devinfo.sectorsize == 1024)
		sector_size = 1024;

	if (len > NAND_MAX_OOBSIZE || len % OOB_AVAI_PER_SECTOR || !buf) {
		pr_warn("[%s] invalid parameter, len: %d, buf: %p\n", __func__, len, buf);
		return -EINVAL;
	}
	if (len > spare_per_sector)
		randomread = 1;

	if (!randomread || !(gn_devinfo.advancedmode & RAMDOM_READ)) {
		while (len > 0) {
			read_len = min(len, spare_per_sector);
			col_addr = sector_size + sector * (sector_size + spare_per_sector);
			/* TODO: Fix this hard-code 16 */
			if (!mtk_nand_ready_for_read
				(chip, page_addr, col_addr, sec_num, false, NULL, NORMAL_READ)) {
				pr_warn("mtk_nand_ready_for_read return failed\n");
				res = -EIO;
				goto error;
			}
			if (!mtk_nand_mcu_read_data(buf + spare_per_sector * sector, read_len)) {
				pr_warn("mtk_nand_mcu_read_data return failed\n");
				res = -EIO;
				goto error;
			}
			mtk_nand_stop_read();
			sector++;
			len -= read_len;
		}
	} else {
		col_addr = sector_size;
		if (chip->options & NAND_BUSWIDTH_16)
			col_addr /= 2;

		if (!mtk_nand_reset())
			goto error;

		mtk_nand_set_mode(0x6000);
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_READ_EN);
		DRV_WriteReg32(NFI_CON_REG16, 4 << CON_NFI_SEC_SHIFT);

		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);

		mtk_nand_set_autoformat(false);

		if (!mtk_nand_set_command(NAND_CMD_READ0))
			goto error;

		/*1 FIXED ME: For Any Kind of AddrCycle */
		if (!mtk_nand_set_address(col_addr, page_addr, colnob, rawnob))
			goto error;

		if (!mtk_nand_set_command(NAND_CMD_READSTART))
			goto error;

		if (!mtk_nand_status_ready(STA_NAND_BUSY))
			goto error;

		read_len = min(len, spare_per_sector);
		if (!mtk_nand_mcu_read_data(buf + spare_per_sector * sector, read_len)) {
			pr_warn("mtk_nand_mcu_read_data return failed first 16\n");
			res = -EIO;
			goto error;
		}
		sector++;
		len -= read_len;
		mtk_nand_stop_read();
		while (len > 0) {
			read_len = min(len, spare_per_sector);
			if (!mtk_nand_set_command(0x05))
				goto error;

			col_addr = sector_size + sector * (sector_size + 16);	/*: TODO_JP careful 16 */
			if (chip->options & NAND_BUSWIDTH_16)
				col_addr /= 2;

			DRV_WriteReg32(NFI_COLADDR_REG32, col_addr);
			DRV_WriteReg16(NFI_ADDRNOB_REG16, 2);
			DRV_WriteReg32(NFI_CON_REG16, 4 << CON_NFI_SEC_SHIFT);

			if (!mtk_nand_status_ready(STA_ADDR_STATE))
				goto error;

			if (!mtk_nand_set_command(0xE0))
				goto error;

			if (!mtk_nand_status_ready(STA_NAND_BUSY))
				goto error;

			if (!mtk_nand_mcu_read_data(buf + spare_per_sector * sector, read_len)) {
				/* TODO: and this 8 */
				pr_warn("mtk_nand_mcu_read_data return failed first 16\n");
				res = -EIO;
				goto error;
			}
			mtk_nand_stop_read();
			sector++;
			len -= read_len;
		}
	}
error:
	NFI_CLN_REG32(NFI_CON_REG16, CON_NFI_BRD);
	return res;
}

static int mtk_nand_write_oob_raw(struct mtd_info *mtd, const uint8_t *buf, int page_addr, int len)
{
	struct nand_chip *chip = mtd->priv;
	u32 col_addr = 0;
	u32 sector = 0;
	int write_len = 0;
	int status;
	int sec_num = 1<<(chip->page_shift - host->hw->nand_sec_shift);
	int spare_per_sector = mtd->oobsize / sec_num;
	u32 sector_size = NAND_SECTOR_SIZE;

	if (gn_devinfo.sectorsize == 1024)
		sector_size = 1024;

	if (len > NAND_MAX_OOBSIZE || len % OOB_AVAI_PER_SECTOR || !buf) {
		pr_warn("[%s] invalid parameter, len: %d, buf: %p\n", __func__, len, buf);
		return -EINVAL;
	}

	while (len > 0) {
		write_len = min(len,  spare_per_sector);
		col_addr = sector * (sector_size +  spare_per_sector) + sector_size;
		if (!mtk_nand_ready_for_write(chip, page_addr, col_addr, false, NULL))
			return -EIO;

		if (!mtk_nand_mcu_write_data(mtd, buf + sector * spare_per_sector, write_len))
			return -EIO;

		(void)mtk_nand_check_RW_count(write_len);
		NFI_CLN_REG32(NFI_CON_REG16, CON_NFI_BWR);
		(void)mtk_nand_set_command(NAND_CMD_PAGEPROG);

		while (DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY)
			;
		status = chip->waitfunc(mtd, chip);
		if (status & NAND_STATUS_FAIL) {
			pr_debug("status: %d\n", status);
			return -EIO;
		}

		len -= write_len;
		sector++;
	}

	return 0;
}

static int mtk_nand_write_oob_hw(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
	int i, iter;

	int sec_num = 1<<(chip->page_shift - host->hw->nand_sec_shift);
	int spare_per_sector = mtd->oobsize / sec_num;

	memcpy(local_oob_buf, chip->oob_poi, mtd->oobsize);

	for (i = 0; i < chip->ecc.layout->eccbytes; i++) {
		iter =
			(i / OOB_AVAI_PER_SECTOR) * spare_per_sector + OOB_AVAI_PER_SECTOR +
			i % OOB_AVAI_PER_SECTOR;
		local_oob_buf[iter] = chip->oob_poi[chip->ecc.layout->eccpos[i]];
	}

	for (i = 0; i < sec_num; i++)
		memcpy(&local_oob_buf[i * spare_per_sector],
			&chip->oob_poi[i * OOB_AVAI_PER_SECTOR], OOB_AVAI_PER_SECTOR);

	return mtk_nand_write_oob_raw(mtd, local_oob_buf, page, mtd->oobsize);
}

static int mtk_nand_write_oob(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
	int page_per_block = 1 << (chip->phys_erase_shift - chip->page_shift);
	u32 block;
	u16 page_in_block;
	u32 mapped_block;

	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);

	if (mapped_block != block)
		set_bad_index_to_oob(chip->oob_poi, block);
	else
		set_bad_index_to_oob(chip->oob_poi, FAKE_INDEX);

	if (mtk_nand_write_oob_hw
		(mtd, chip, page_in_block + mapped_block * page_per_block /* page */)) {
		MSG(INIT, "write oob fail at block: 0x%x, page: 0x%x\n", mapped_block,
			page_in_block);
		if (update_bmt
			((u64) ((u64) page_in_block + (u64) mapped_block * page_per_block) <<
			 chip->page_shift, UPDATE_WRITE_FAIL, NULL, chip->oob_poi)) {
			MSG(INIT, "Update BMT success\n");
			return 0;
		}
			MSG(INIT, "Update BMT fail\n");
			return -EIO;
		}

	return 0;
}

int mtk_nand_block_markbad_hw(struct mtd_info *mtd, loff_t offset)
{
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	struct nand_chip *chip = mtd->priv;
#endif
	int block; /* = (int)(offset / (gn_devinfo.blocksize * 1024)); */
	int page; /* = block * (gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize); */
	int ret;
	loff_t temp;
	u8 buf[8];

	temp = offset;
	do_div(temp, ((gn_devinfo.blocksize * 1024) & 0xFFFFFFFF));
	block = (u32) temp;
	page = block * (gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize);

	memset(buf, 0xFF, 8);
	buf[0] = 0;

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if (mtk_is_normal_tlc_nand())
		ret = mtk_nand_tlc_block_mark(mtd, chip, block);
	else
#endif
	ret = mtk_nand_write_oob_raw(mtd, buf, page, 8);
	return 0;
}

static int mtk_nand_block_markbad(struct mtd_info *mtd, loff_t offset, const uint8_t *buf)
{
	struct nand_chip *chip = mtd->priv;
	int page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;
	u32 block; /*  = (u32)(offset  / (gn_devinfo.blocksize * 1024)); */
	int page; /* = block * (gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize); */
	u32 mapped_block;
	int ret;
	loff_t temp;

	temp = offset;
	do_div(temp, ((gn_devinfo.blocksize * 1024) & 0xFFFFFFFF));
	block = (u32) temp;
	page = block * (gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize);

	nand_get_device(mtd, FL_WRITING);

	page = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);

	if (NULL != buf) {
		MSG(INIT, "write fail at block: 0x%x, page: 0x%x\n", mapped_block, page);
		if (update_bmt
			((u64) ((u64) page + (u64) mapped_block * page_per_block) << chip->page_shift,
			UPDATE_WRITE_FAIL, (u8 *) buf, chip->oob_poi)) {
			pr_err("Update BMT success\n");
		} else {
			pr_err("Update BMT fail\n");
			nand_release_device(mtd);
			return -EIO;
		}
	}
	ret = mtk_nand_block_markbad_hw(mtd, mapped_block * (gn_devinfo.blocksize * 1024));

	nand_release_device(mtd);

	return ret;
}

int mtk_nand_read_oob_hw(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
	int i;
	u8 iter = 0;

	int sec_num = 1<<(chip->page_shift - host->hw->nand_sec_shift);
	int spare_per_sector = mtd->oobsize / sec_num;
#ifdef TESTTIME
	unsigned long long time1, time2;

	time1 = sched_clock();
#endif

	if (mtk_nand_read_oob_raw(mtd, chip->oob_poi, page, mtd->oobsize)) {
		/* pr_err("[%s]mtk_nand_read_oob_raw return failed\n", __func__); */
		return -EIO;
	}
#ifdef TESTTIME
	time2 = sched_clock() - time1;
	if (!readoobflag) {
		readoobflag = 1;
		pr_err("[%s] time is %llu", __func__, time2);
	}
#endif

	/* adjust to ecc physical layout to memory layout */
	/*********************************************************/
	/* FDM0 | ECC0 | FDM1 | ECC1 | FDM2 | ECC2 | FDM3 | ECC3 */
	/*  8B  |  8B  |  8B  |  8B  |  8B  |  8B  |  8B  |  8B  */
	/*********************************************************/

	memcpy(local_oob_buf, chip->oob_poi, mtd->oobsize);

	/* copy ecc data */
	for (i = 0; i < chip->ecc.layout->eccbytes; i++) {
		iter =
			(i / OOB_AVAI_PER_SECTOR) * spare_per_sector + OOB_AVAI_PER_SECTOR +
			i % OOB_AVAI_PER_SECTOR;
		chip->oob_poi[chip->ecc.layout->eccpos[i]] = local_oob_buf[iter];
	}

	/* copy FDM data */
	for (i = 0; i < sec_num; i++)
		memcpy(&chip->oob_poi[i * OOB_AVAI_PER_SECTOR],
			&local_oob_buf[i * spare_per_sector], OOB_AVAI_PER_SECTOR);

	return 0;
}

static int mtk_nand_read_oob(struct mtd_info *mtd, struct nand_chip *chip, int page)
{

	mtk_nand_read_page(mtd, chip, temp_buffer_16_align, page);

	return 0;		/* the return value is sndcmd */
}

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
bool mtk_nand_slc_write_wodata(struct nand_chip *chip, u32 page)
{
	bool bRet = FALSE;
	bool slc_en;
	u32 real_row_addr;
	u32 reg_val;
	struct NFI_TLC_WL_INFO tlc_wl_info;

	NFI_TLC_GetMappedWL(page, &tlc_wl_info);
	real_row_addr = NFI_TLC_GetRowAddr(tlc_wl_info.word_line_idx);

	reg_val = DRV_Reg16(NFI_CNFG_REG16);
	reg_val &= ~CNFG_READ_EN;
	reg_val &= ~CNFG_OP_MODE_MASK;
	reg_val |= CNFG_OP_CUST;
	DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

	mtk_nand_set_command(0xA2);

	reg_val = DRV_Reg32(NFI_CON_REG16);
	reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
	/* issue reset operation */
	DRV_WriteReg32(NFI_CON_REG16, reg_val);

	mtk_nand_set_mode(CNFG_OP_PRGM);
	mtk_nand_set_command(NAND_CMD_SEQIN);

	mtk_nand_set_address(0, real_row_addr, 2, 3);

	mtk_nand_set_command(NAND_CMD_PAGEPROG);

	slc_en = gn_devinfo.tlcControl.slcopmodeEn;
	gn_devinfo.tlcControl.slcopmodeEn = TRUE;
	bRet = !mtk_nand_read_status();
	gn_devinfo.tlcControl.slcopmodeEn = slc_en;

	if (bRet)
		pr_warn("mtk_nand_slc_write_wodata: page %d is bad\n", page);

	return bRet;
}

u64 mtk_nand_device_size(void)
{
	u64 totalsize;

	totalsize = (u64)gn_devinfo.totalsize << 10;
	return totalsize;
}
#endif

int mtk_nand_block_bad_hw(struct mtd_info *mtd, loff_t ofs)
{
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	int page_addr = (int)(ofs >> chip->page_shift);
	u32 block, mapped_block;
	int ret;
	unsigned int page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	bool bRet;
#endif


#if !defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	page_addr &= ~(page_per_block - 1);
#endif
	memset(temp_buffer_16_align, 0xFF, LPAGE);

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		&& (gn_devinfo.vendor == VEND_SANDISK)
		&& mtk_nand_IsBMTPOOL(ofs)) {
		page_addr = mtk_nand_page_transform(mtd, chip, page_addr, &block, &mapped_block);
		bRet = mtk_nand_slc_write_wodata(chip, mapped_block * page_per_block);
		/*pr_warn("after mtk_nand_slc_write_wodata\n");*/
		if (bRet)
			ret = 1;
		else
			ret = 0;
		return ret;
	}
#endif
	ret =
		mtk_nand_read_subpage(mtd, chip, temp_buffer_16_align, (ofs >> chip->page_shift), 0, 1);
	page_addr = mtk_nand_page_transform(mtd, chip, page_addr, &block, &mapped_block);
	if (0 != ret) {
		pr_warn("mtk_nand_read_oob_raw return error %d\n", ret);
		return 1;
	}

	if (chip->oob_poi[0] != 0xff) {
		pr_warn("Bad block detected at 0x%x, oob_buf[0] is 0x%x\n", block * page_per_block,
			chip->oob_poi[0]);
		return 1;
	}
	return 0;		/* everything is OK, good block */
}

static int mtk_nand_block_bad(struct mtd_info *mtd, loff_t ofs, int getchip)
{
	int chipnr = 0;

	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	int block; /* = (int)(ofs / (gn_devinfo.blocksize * 1024));*/
	int mapped_block;
	int page = (int)(ofs >> chip->page_shift);
	int page_in_block;
	int page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;
	loff_t temp;
	int ret;

	temp = ofs;
	do_div(temp, ((gn_devinfo.blocksize * 1024) & 0xFFFFFFFF));
	block = (int) temp;

	if (getchip) {
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		temp = mtk_nand_device_size();
		if (ofs >= temp)
			chipnr = 1;
		else
			chipnr = 0;
#else
		chipnr = (int)(ofs >> chip->chip_shift);
#endif
		nand_get_device(mtd, FL_READING);
		/* Select the NAND device */
		chip->select_chip(mtd, chipnr);
	}

	ret = mtk_nand_block_bad_hw(mtd, ofs);
	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);

	if (ret) {
		MSG(INIT, "Unmapped bad block: 0x%x %d\n", mapped_block, ret);
		if (update_bmt
			((u64) ((u64) page_in_block + (u64) mapped_block * page_per_block) <<
			 chip->page_shift, UPDATE_UNMAPPED_BLOCK, NULL, NULL)) {
			MSG(INIT, "Update BMT success\n");
			ret = 0;
		} else {
			MSG(INIT, "Update BMT fail\n");
			ret = 1;
		}
	}

	if (getchip)
		nand_release_device(mtd);

	return ret;
}

/******************************************************************************
 * mtk_nand_init_size
 *
 * DESCRIPTION:
 *initialize the pagesize, oobsize, blocksize
 *
 * PARAMETERS:
 *struct mtd_info *mtd, struct nand_chip *this, u8 *id_data
 *
 * RETURNS:
 *Buswidth
 *
 * NOTES:
 *None
 *
 ******************************************************************************/

static int mtk_nand_init_size(struct mtd_info *mtd, struct nand_chip *this, u8 *id_data)
{
	/* Get page size */
	mtd->writesize = gn_devinfo.pagesize;

	/* Get oobsize */
	mtd->oobsize = gn_devinfo.sparesize;

	/* Get blocksize. */
	mtd->erasesize = gn_devinfo.blocksize * 1024;
	/* Get buswidth information */
	if (gn_devinfo.iowidth == 16)
		return NAND_BUSWIDTH_16;
	else
		return 0;

}

int mtk_nand_write_tlc_wl_hw(struct mtd_info *mtd, struct nand_chip *chip,
				 uint8_t *buf, u32 wl, enum NFI_TLC_PG_CYCLE program_cycle)
{
	u32 page;
	uint8_t *temp_buf = NULL;
#if defined(CFG_PERFLOG_DEBUG)
	struct timeval stimer, etimer;

	do_gettimeofday(&stimer);
#endif

	gn_devinfo.tlcControl.slcopmodeEn = FALSE;

	tlc_program_cycle = program_cycle;
	page = wl * 3;

	temp_buf = buf;
	memcpy(local_tlc_wl_buffer, temp_buf, mtd->writesize);
	if (mtk_nand_exec_write_page(mtd, page, mtd->writesize, local_tlc_wl_buffer, chip->oob_poi)) {
		MSG(INIT, "write fail at wl: 0x%x, page: 0x%x\n", wl, page);

		return -EIO;
	}

	temp_buf += mtd->writesize;
	memcpy(local_tlc_wl_buffer, temp_buf, mtd->writesize);
	if (mtk_nand_exec_write_page(mtd, page + 1, mtd->writesize, local_tlc_wl_buffer, chip->oob_poi)) {
		MSG(INIT, "write fail at wl: 0x%x, page: 0x%x\n", wl, page);

		return -EIO;
	}

	temp_buf += mtd->writesize;
	memcpy(local_tlc_wl_buffer, temp_buf, mtd->writesize);
	if (mtk_nand_exec_write_page(mtd, page + 2, mtd->writesize, local_tlc_wl_buffer, chip->oob_poi)) {
		MSG(INIT, "write fail at wl: 0x%x, page: 0x%x\n", wl, page);

		return -EIO;
	}
#if defined(CFG_PERFLOG_DEBUG)
	do_gettimeofday(&etimer);
	g_NandPerfLog.WritePageTotalTime += Cal_timediff(&etimer, &stimer);
	g_NandPerfLog.WritePageCount++;
	dump_nand_rwcount();
#endif
	return 0;
}

int mtk_nand_write_tlc_wl(struct mtd_info *mtd, struct nand_chip *chip,
				 uint8_t *buf, u32 wl, enum NFI_TLC_PG_CYCLE program_cycle)
{
	int bRet;

	bRet = mtk_nand_write_tlc_wl_hw(mtd, chip, buf, wl, program_cycle);
	return bRet;
}

int mtk_nand_write_tlc_block_hw(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, u32 mapped_block, u32 page_in_block, u32 size)
{
	int page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;
	u32 index;
	int bRet = 0;
	u32 base_wl_index;
	u32 wl_in_block;
	u32 write_page_number;
	u8 *temp_buf = NULL;

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)

	base_wl_index = mapped_block * page_per_block / 3;
	wl_in_block = page_in_block / 3;
	write_page_number = size / gn_devinfo.pagesize;

	for (index = wl_in_block; index < (wl_in_block + (write_page_number / 3)); index++) {
		if (index == 0) {
			temp_buf = buf + (index * 3 * mtd->writesize);
			bRet = mtk_nand_write_tlc_wl(mtd, chip, temp_buf, base_wl_index + index, PROGRAM_1ST_CYCLE);
			if (bRet != 0)
				break;

			temp_buf = buf + ((index + 1) * 3 * mtd->writesize);
			bRet = mtk_nand_write_tlc_wl(mtd, chip, temp_buf, base_wl_index + index + 1, PROGRAM_1ST_CYCLE);
			if (bRet != 0)
				break;

			temp_buf = buf + (index * 3 * mtd->writesize);
			bRet = mtk_nand_write_tlc_wl(mtd, chip, temp_buf, base_wl_index + index, PROGRAM_2ND_CYCLE);
			if (bRet != 0)
				break;
		}

		if ((index + 2) < (page_per_block / 3)) {
			temp_buf = buf + ((index + 2) * 3 * mtd->writesize);
			bRet = mtk_nand_write_tlc_wl(mtd, chip, temp_buf, base_wl_index + index + 2, PROGRAM_1ST_CYCLE);
			if (bRet != 0)
				break;
		}

		if ((index + 1) < (page_per_block / 3)) {
			temp_buf = buf + ((index + 1) * 3 * mtd->writesize);
			bRet = mtk_nand_write_tlc_wl(mtd, chip, temp_buf, base_wl_index + index + 1, PROGRAM_2ND_CYCLE);
			if (bRet != 0)
				break;
		}

		temp_buf = buf + (index * 3 * mtd->writesize);
		bRet = mtk_nand_write_tlc_wl(mtd, chip, temp_buf, base_wl_index + index, PROGRAM_3RD_CYCLE);
		if (bRet != 0)
			break;

	}
	if (bRet != 0)
		return -EIO;
#else
	base_wl_index = mapped_block * page_per_block;
	for (index = 0; index < page_per_block; index++) {
		temp_buf = buf + (index * mtd->writesize);
		bRet = mtk_nand_exec_write_page(mtd, base_wl_index+index, mtd->writesize, temp_buf, chip->oob_poi);
		if (bRet != 0)
			break;
	}
#endif
	return 0;
}


int mtk_nand_write_tlc_block(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, u32 page, u32 size)
{
	int page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;
	u32 block;
	u32 page_in_block;
	u32 mapped_block;
	int bRet;
#if defined(CFG_PERFLOG_DEBUG)
	struct timeval stimer, etimer;

	do_gettimeofday(&stimer);
#endif
#ifdef __D_PFM__
		struct timeval pfm_time_write;
#endif


	if (gn_devinfo.NAND_FLASH_TYPE != NAND_FLASH_TLC && gn_devinfo.NAND_FLASH_TYPE != NAND_FLASH_MLC_HYBER) {
		MSG(INIT, "error : not tlc nand\n");
		return -EIO;
	}

	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);

/*
	if (page_in_block != 0) {
		MSG(INIT, "error : normal tlc block program is not block aligned\n");
		return -EIO;
	}
*/
#ifdef __D_PFM__
		PFM_DBEGIN(pfm_time_write);
#endif

	memset(chip->oob_poi, 0xff, mtd->oobsize);

	if (mapped_block != block)
		set_bad_index_to_oob(chip->oob_poi, block);
	else
		set_bad_index_to_oob(chip->oob_poi, FAKE_INDEX);

	bRet = mtk_nand_write_tlc_block_hw(mtd, chip, buf, mapped_block, page_in_block, size);

#ifdef __D_PFM__
		PFM_END_DBW(pfm_time_write);
#endif

	if (bRet != 0) {
		MSG(INIT, "write fail at block: 0x%x, page: 0x%x\n", mapped_block, page_in_block);
		if (update_bmt
			((u64)((u64)page_in_block + (u64)mapped_block * page_per_block) << chip->page_shift,
				UPDATE_WRITE_FAIL, (u8 *) buf, chip->oob_poi)) {
			MSG(INIT, "Update BMT success\n");
			return 0;
		}
		MSG(INIT, "Update BMT fail\n");
		return -EIO;
	}
#if defined(CFG_PERFLOG_DEBUG)
	do_gettimeofday(&etimer);
	g_NandPerfLog.WritePageTotalTime += Cal_timediff(&etimer, &stimer);
	g_NandPerfLog.WritePageCount++;
	dump_nand_rwcount();
#endif
	return 0;
}

bool mtk_is_tlc_nand(void)
{
	if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		return TRUE;
	else
		return FALSE;
}

bool mtk_is_normal_tlc_nand(void)
{
	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
			 && (gn_devinfo.tlcControl.normaltlc))
		return TRUE;
	if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER)
		return TRUE;
	return FALSE;
}

int mtk_nand_tlc_wl_mark(struct mtd_info *mtd, struct nand_chip *chip,
				 uint8_t *buf, u32 wl, enum NFI_TLC_PG_CYCLE program_cycle)
{
	u32 page;
#if defined(CFG_PERFLOG_DEBUG)
	struct timeval stimer, etimer;

	do_gettimeofday(&stimer);
#endif

	gn_devinfo.tlcControl.slcopmodeEn = FALSE;

	tlc_program_cycle = program_cycle;
	page = wl * 3;

	if (mtk_nand_exec_write_page(mtd, page, mtd->writesize, buf, (buf + mtd->writesize))) {
		MSG(INIT, "write fail at wl: 0x%x, page: 0x%x\n", wl, page);

		return -EIO;
	}

	if (mtk_nand_exec_write_page(mtd, page + 1, mtd->writesize, buf, (buf + mtd->writesize))) {
		MSG(INIT, "write fail at wl: 0x%x, page: 0x%x\n", wl, page);

		return -EIO;
	}

	if (mtk_nand_exec_write_page(mtd, page + 2, mtd->writesize, buf, (buf + mtd->writesize))) {
		MSG(INIT, "write fail at wl: 0x%x, page: 0x%x\n", wl, page);

		return -EIO;
	}
#if defined(CFG_PERFLOG_DEBUG)
	do_gettimeofday(&etimer);
	g_NandPerfLog.WritePageTotalTime += Cal_timediff(&etimer, &stimer);
	g_NandPerfLog.WritePageCount++;
	dump_nand_rwcount();
#endif
	return 0;
}

int mtk_nand_tlc_block_mark(struct mtd_info *mtd, struct nand_chip *chip, u32 mapped_block)
{
	int page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;
	u32 index;
	int bRet;
	u32 base_wl_index;
	u8 *buf = local_tlc_wl_buffer;

	memset(buf, 0xAA, LPAGE + LSPARE);

	if (gn_devinfo.tlcControl.slcopmodeEn) {
		if (mtk_nand_exec_write_page
				(mtd, mapped_block * page_per_block, mtd->writesize, buf, (buf + mtd->writesize))) {
			MSG(INIT, "mark fail at page: 0x%x\n", mapped_block * page_per_block);

			return -EIO;
		}
	} else {
		base_wl_index = mapped_block * page_per_block / 3;

		for (index = 0; index < (page_per_block / 3); index++) {
			if (index == 0) {
				bRet = mtk_nand_tlc_wl_mark(mtd, chip, buf,
					base_wl_index + index, PROGRAM_1ST_CYCLE);
				if (bRet != 0)
					break;

				bRet = mtk_nand_tlc_wl_mark(mtd, chip, buf,
					base_wl_index + index + 1, PROGRAM_1ST_CYCLE);
				if (bRet != 0)
					break;

				bRet = mtk_nand_tlc_wl_mark(mtd, chip, buf,
					base_wl_index + index, PROGRAM_2ND_CYCLE);
				if (bRet != 0)
					break;
			}

			if ((index + 2) < (page_per_block / 3)) {
				bRet = mtk_nand_tlc_wl_mark(mtd, chip, buf,
					base_wl_index + index + 2, PROGRAM_1ST_CYCLE);
				if (bRet != 0)
					break;
			}

			if ((index + 1) < (page_per_block / 3)) {
				bRet = mtk_nand_tlc_wl_mark(mtd, chip, buf,
					base_wl_index + index + 1, PROGRAM_2ND_CYCLE);
				if (bRet != 0)
					break;
			}

			bRet = mtk_nand_tlc_wl_mark(mtd, chip, buf, base_wl_index + index, PROGRAM_3RD_CYCLE);
			if (bRet != 0)
				break;

		}
		if (bRet != 0)
			return -EIO;
		return 0;
	}
	return 0;
}


/******************************************************************************
 * mtk_nand_verify_buf
 *
 * DESCRIPTION:
 *Verify the NAND write data is correct or not !
 *
 * PARAMETERS:
 *struct mtd_info *mtd, const uint8_t *buf, int len
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
#ifdef CONFIG_MTD_NAND_VERIFY_WRITE

char gacBuf[LPAGE + LSPARE];

static int mtk_nand_verify_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
#if 1
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	struct NAND_CMD *pkCMD = &g_kCMD;
	u32 u4PageSize = mtd->writesize;
	u32 *pSrc, *pDst;
	int i;

	if (((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		|| (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_MLC_HYBER))
		 && (gn_devinfo.tlcControl.normaltlc) && (!mtk_block_istlc(pkCMD.u4RowAddr * mtd->writesize)))
		gn_devinfo.tlcControl.slcopmodeEn = TRUE;
	else
		gn_devinfo.tlcControl.slcopmodeEn = FALSE;

	mtk_nand_exec_read_page(mtd, pkCMD->u4RowAddr, u4PageSize, gacBuf, gacBuf + u4PageSize);

	pSrc = (u32 *) buf;
	pDst = (u32 *) gacBuf;
	len = len / sizeof(u32);
	for (i = 0; i < len; ++i) {
		if (*pSrc != *pDst) {
			MSG(VERIFY, "mtk_nand_verify_buf page fail at page %d\n", pkCMD->u4RowAddr);
			return -1;
		}
		pSrc++;
		pDst++;
	}

	pSrc = (u32 *) chip->oob_poi;
	pDst = (u32 *) (gacBuf + u4PageSize);

	if ((pSrc[0] != pDst[0]) || (pSrc[1] != pDst[1]) || (pSrc[2] != pDst[2])
		|| (pSrc[3] != pDst[3]) || (pSrc[4] != pDst[4]) || (pSrc[5] != pDst[5])) {
		/* TODO: Ask Designer Why? */
		/*(pSrc[6] != pDst[6]) || (pSrc[7] != pDst[7])) */
		MSG(VERIFY, "mtk_nand_verify_buf oob fail at page %d\n", pkCMD->u4RowAddr);
		MSG(VERIFY, "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", pSrc[0], pSrc[1], pSrc[2],
			pSrc[3], pSrc[4], pSrc[5], pSrc[6], pSrc[7]);
		MSG(VERIFY, "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", pDst[0], pDst[1], pDst[2],
			pDst[3], pDst[4], pDst[5], pDst[6], pDst[7]);
		return -1;
	}
	/*
	for (i = 0; i < len; ++i) {
	if (*pSrc != *pDst) {
	pr_err("mtk_nand_verify_buf oob fail at page %d\n", g_kCMD.u4RowAddr);
	return -1;
	}
	pSrc++;
	pDst++;
	}
	 */

	return 0;
#else
	return 0;
#endif
}
#endif

/******************************************************************************
 * mtk_nand_init_hw
 *
 * DESCRIPTION:
 *Initial NAND device hardware component !
 *
 * PARAMETERS:
 *struct mtk_nand_host *host (Initial setting data)
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static void mtk_nand_init_hw(struct mtk_nand_host *host)
{
	struct mtk_nand_host_hw *hw = host->hw;


	g_bInitDone = false;
	g_kCMD.u4OOBRowAddr = (u32) -1;

	/* Set default NFI access timing control */
	DRV_WriteReg32(NFI_ACCCON_REG32, 0x31C083F9);
	DRV_WriteReg16(NFI_CNFG_REG16, 0);
	DRV_WriteReg32(NFI_PAGEFMT_REG16, 4);
	DRV_WriteReg32(NFI_EMPTY_THRESHOLD, 40);

	/* Reset the state machine and data FIFO, because flushing FIFO */
	(void)mtk_nand_reset();

	/* Set the ECC engine */
	if (hw->nand_ecc_mode == NAND_ECC_HW) {
		MSG(INIT, "%s : Use HW ECC\n", MODULE_NAME);
		if (g_bHwEcc)
			NFI_SET_REG32(NFI_CNFG_REG16, CNFG_HW_ECC_EN);

		ECC_Config(host->hw, 4);
		mtk_nand_configure_fdm(8);
	}

	/* Initialize interrupt. Clear interrupt, read clear. */
	DRV_Reg16(NFI_INTR_REG16);

	/* Interrupt arise when read data or program data to/from AHB is done. */
	DRV_WriteReg16(NFI_INTR_EN_REG16, 0);

	/* Enable automatic disable ECC clock when NFI is busy state */
	DRV_WriteReg16(NFI_DEBUG_CON1_REG16, (NFI_BYPASS|WBUF_EN|HWDCM_SWCON_ON));
	NFI_SET_REG32(NFI_DEBUG_CON1_REG16, NFI_BYPASS);
	NFI_SET_REG32(ECC_BYPASS_REG32, ECC_BYPASS);
	DRV_WriteReg16(NFI_NAND_TYPE_CNFG_REG32, 0);

#ifdef CONFIG_PM
	host->saved_para.suspend_flag = 0;
#endif
	/* Reset */
}

/*-------------------------------------------------------------------------------*/
static int mtk_nand_dev_ready(struct mtd_info *mtd)
{
	return !(DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY);
}

/******************************************************************************
 * mtk_nand_proc_read
 *
 * DESCRIPTION:
 *Read the proc file to get the interrupt scheme setting !
 *
 * PARAMETERS:
 *char *page, char **start, off_t off, int count, int *eof, void *data
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
int mtk_nand_proc_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	char *p = buffer;
	int len = 0;
	int i;

	p += sprintf(p, "ID: ");
	for (i = 0; i < gn_devinfo.id_length; i++)
		p += sprintf(p, " 0x%x", gn_devinfo.id[i]);

	p += sprintf(p, "\n");
	p += sprintf(p, "total size: %dMiB; part number: %s\n", gn_devinfo.totalsize,
			 gn_devinfo.devciename);
	p += sprintf(p, "Current working in %s mode\n", g_i4Interrupt ? "interrupt" : "polling");
	p += sprintf(p, "NFI_ACCON = 0x%x\n", DRV_Reg32(NFI_ACCCON_REG32));
	p += sprintf(p, "NFI_NAND_TYPE_CNFG_REG32 = 0x%x\n", DRV_Reg32(NFI_NAND_TYPE_CNFG_REG32));
#ifdef DUMP_PEF
	p += sprintf(p, "Read Page Count: %d, Read Page totalTime: %lu, Avg. RPage: %lu\r\n",
			 g_NandPerfLog.ReadPageCount, g_NandPerfLog.ReadPageTotalTime,
			 g_NandPerfLog.ReadPageCount ? (g_NandPerfLog.ReadPageTotalTime /
							g_NandPerfLog.ReadPageCount) : 0);

	p += sprintf(p, "Read subPage Count: %d, Read subPage totalTime: %lu, Avg. RPage: %lu\r\n",
			 g_NandPerfLog.ReadSubPageCount, g_NandPerfLog.ReadSubPageTotalTime,
			 g_NandPerfLog.ReadSubPageCount ? (g_NandPerfLog.ReadSubPageTotalTime /
							g_NandPerfLog.ReadSubPageCount) : 0);

	p += sprintf(p, "Read Busy Count: %d, Read Busy totalTime: %lu, Avg. R Busy: %lu\r\n",
			 g_NandPerfLog.ReadBusyCount, g_NandPerfLog.ReadBusyTotalTime,
			 g_NandPerfLog.ReadBusyCount ? (g_NandPerfLog.ReadBusyTotalTime /
							g_NandPerfLog.ReadBusyCount) : 0);

	p += sprintf(p, "Read DMA Count: %d, Read DMA totalTime: %lu, Avg. R DMA: %lu\r\n",
			 g_NandPerfLog.ReadDMACount, g_NandPerfLog.ReadDMATotalTime,
			 g_NandPerfLog.ReadDMACount ? (g_NandPerfLog.ReadDMATotalTime /
						g_NandPerfLog.ReadDMACount) : 0);

	p += sprintf(p, "Write Page Count: %d, Write Page totalTime: %lu, Avg. WPage: %lu\r\n",
			 g_NandPerfLog.WritePageCount, g_NandPerfLog.WritePageTotalTime,
			 g_NandPerfLog.WritePageCount ? (g_NandPerfLog.WritePageTotalTime /
							 g_NandPerfLog.WritePageCount) : 0);

	p += sprintf(p, "Write Busy Count: %d, Write Busy totalTime: %lu, Avg. W Busy: %lu\r\n",
			 g_NandPerfLog.WriteBusyCount, g_NandPerfLog.WriteBusyTotalTime,
			 g_NandPerfLog.WriteBusyCount ? (g_NandPerfLog.WriteBusyTotalTime /
							 g_NandPerfLog.WriteBusyCount) : 0);

	p += sprintf(p, "Write DMA Count: %d, Write DMA totalTime: %lu, Avg. W DMA: %lu\r\n",
			 g_NandPerfLog.WriteDMACount, g_NandPerfLog.WriteDMATotalTime,
			 g_NandPerfLog.WriteDMACount ? (g_NandPerfLog.WriteDMATotalTime /
							g_NandPerfLog.WriteDMACount) : 0);

	p += sprintf(p, "EraseBlock Count: %d, EraseBlock totalTime: %lu, Avg. Erase: %lu\r\n",
			 g_NandPerfLog.EraseBlockCount, g_NandPerfLog.EraseBlockTotalTime,
			 g_NandPerfLog.EraseBlockCount ? (g_NandPerfLog.EraseBlockTotalTime /
							  g_NandPerfLog.EraseBlockCount) : 0);

#endif
	len = p - buffer;

	return len < count ? len : count;
}

/******************************************************************************
 * mtk_nand_proc_write
 *
 * DESCRIPTION:
 *Write the proc file to set the interrupt scheme !
 *
 * PARAMETERS:
 *struct file* file, const char* buffer,	unsigned long count, void *data
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
ssize_t  mtk_nand_proc_write(struct file *file, const char *buffer, size_t  count, loff_t *data)
{
	struct mtd_info *mtd = &host->mtd;
	char buf[16];
	char cmd;
	int value;
	int len = (int)count;
	int ret;

	if (len >= sizeof(buf))
		len = sizeof(buf) - 1;

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	ret = sscanf(buf, "%c%x", &cmd, &value);
	if (ret)
		pr_debug("ret = %d\n", ret);

	switch (cmd) {
	case '1':
#ifdef DUMP_PEF
#ifdef CFG_SNAND_ACCESS_PATTERN_LOGGER
		g_NandPerfLog.ReadPageCount = 0;
		g_NandPerfLog.ReadPageTotalTime = 0;
		g_NandPerfLog.ReadBusyCount = 0;
		g_NandPerfLog.ReadBusyTotalTime = 0;
		g_NandPerfLog.ReadDMACount = 0;
		g_NandPerfLog.ReadDMATotalTime = 0;

		g_NandPerfLog.WritePageCount = 0;
		g_NandPerfLog.WritePageTotalTime = 0;
		g_NandPerfLog.WriteBusyCount = 0;
		g_NandPerfLog.WriteBusyTotalTime = 0;
		g_NandPerfLog.WriteDMACount = 0;
		g_NandPerfLog.WriteDMATotalTime = 0;

		g_NandPerfLog.EraseBlockCount = 0;
		g_NandPerfLog.EraseBlockTotalTime = 0;

		g_NandPerfLog.ReadPageCount = 0;

		g_snand_pm_on = 1;
		g_snand_pm_cnt = 0;
		g_snand_pm_wrapped = 0;
#endif
#endif
		break;
	case '2':
#ifdef CFG_SNAND_ACCESS_PATTERN_LOGGER
		nand_get_device(mtd, FL_READING);
		g_snand_pm_on = 0;
		mtk_snand_pm_dump_record();
#ifdef DUMP_PEF
	if (g_NandPerfLog.ReadPageCount != 0)
		pr_err("Read Page Count: %d, Read Page totalTime: %lu, Avg. RPage: %lu\r\n",
			g_NandPerfLog.ReadPageCount, g_NandPerfLog.ReadPageTotalTime,
			g_NandPerfLog.ReadPageTotalTime / g_NandPerfLog.ReadPageCount);

	if (g_NandPerfLog.ReadBusyCount != 0)
		pr_err
			("Read Busy Count: %d, Read Busy totalTime: %lu, Avg. R Busy: %lu\r\n",
			 g_NandPerfLog.ReadBusyCount, g_NandPerfLog.ReadBusyTotalTime,
			 g_NandPerfLog.ReadBusyTotalTime / g_NandPerfLog.ReadBusyCount);

	if (g_NandPerfLog.ReadDMACount != 0)
		pr_err("Read DMA Count: %d, Read DMA totalTime: %lu, Avg. R DMA: %lu\r\n",
			g_NandPerfLog.ReadDMACount, g_NandPerfLog.ReadDMATotalTime,
			g_NandPerfLog.ReadDMATotalTime / g_NandPerfLog.ReadDMACount);

	if (g_NandPerfLog.WritePageCount != 0)
		pr_err
			("Write Page Count: %d, Write Page totalTime: %lu, Avg. WPage: %lu\r\n",
			 g_NandPerfLog.WritePageCount, g_NandPerfLog.WritePageTotalTime,
			 g_NandPerfLog.WritePageTotalTime / g_NandPerfLog.WritePageCount);

	if (g_NandPerfLog.WriteBusyCount != 0)
		pr_err
			("Write Busy Count: %d, Write Busy totalTime: %lu, Avg. W Busy: %lu\r\n",
			 g_NandPerfLog.WriteBusyCount, g_NandPerfLog.WriteBusyTotalTime,
			 g_NandPerfLog.WriteBusyTotalTime / g_NandPerfLog.WriteBusyCount);

	if (g_NandPerfLog.WriteDMACount != 0)
		pr_err("Write DMA Count: %d, Write DMA totalTime: %lu, Avg. W DMA: %lu\r\n",
			g_NandPerfLog.WriteDMACount, g_NandPerfLog.WriteDMATotalTime,
			g_NandPerfLog.WriteDMATotalTime / g_NandPerfLog.WriteDMACount);

	if (g_NandPerfLog.EraseBlockCount != 0)
		pr_err
			("EraseBlock Count: %d, EraseBlock totalTime: %lu, Avg. Erase: %lu\r\n",
			 g_NandPerfLog.EraseBlockCount, g_NandPerfLog.EraseBlockTotalTime,
			 g_NandPerfLog.EraseBlockTotalTime / g_NandPerfLog.EraseBlockCount);
#endif
		nand_release_device(mtd);

#endif

		break;
	case 'A':		/* NFIA driving setting */
		break;
	case 'B':		/* NFIB driving setting */
		break;
	case 'D':
#ifdef _MTK_NAND_DUMMY_DRIVER_
	pr_debug("Enable dummy driver\n");
		dummy_driver_debug = 1;
#endif
		break;
	case 'I':
		if ((value > 0 && !g_i4Interrupt) || (value == 0 && g_i4Interrupt)) {
			nand_get_device(mtd, FL_READING);

			g_i4Interrupt = value;

			if (g_i4Interrupt) {
				DRV_Reg16(NFI_INTR_REG16);
				enable_irq(MT_NFI_IRQ_ID);
			} else
				disable_irq(MT_NFI_IRQ_ID);
			nand_release_device(mtd);
		}
		break;
	case 'P':
#ifdef NAND_PFM
		/* Reset values */
		g_PFM_R = 0;
		g_PFM_W = 0;
		g_PFM_E = 0;
		g_PFM_RD = 0;
		g_PFM_WD = 0;
		g_kCMD.pureReadOOBNum = 0;
#endif
#ifdef __D_PFM__
		g_DPFM_R = 0;
		g_DPFM_RS = 0;
		g_DPFM_WB = 0;
		g_DPFM_W = 0;
		g_DPFM_E = 0;
		g_PFM_RD = 0;
		g_PFM_RDS = 0;
		g_PFM_WD = 0;
		g_PFM_WBD = 0;
		g_PFM_RDN = 0;
		g_PFM_RDSN = 0;
		g_PFM_WDN = 0;
		g_PFM_EDN = 0;
#endif
	case 'R':
#ifdef DUMP_PEF
		g_NandPerfLog.ReadPageCount = 0;
		g_NandPerfLog.ReadPageTotalTime = 0;
		g_NandPerfLog.ReadBusyCount = 0;
		g_NandPerfLog.ReadBusyTotalTime = 0;
		g_NandPerfLog.ReadDMACount = 0;
		g_NandPerfLog.ReadDMATotalTime = 0;
		g_NandPerfLog.ReadSubPageCount = 0;
		g_NandPerfLog.ReadSubPageTotalTime = 0;

		g_NandPerfLog.WritePageCount = 0;
		g_NandPerfLog.WritePageTotalTime = 0;
		g_NandPerfLog.WriteBusyCount = 0;
		g_NandPerfLog.WriteBusyTotalTime = 0;
		g_NandPerfLog.WriteDMACount = 0;
		g_NandPerfLog.WriteDMATotalTime = 0;

		g_NandPerfLog.EraseBlockCount = 0;
		g_NandPerfLog.EraseBlockTotalTime = 0;
#endif
#ifdef __D_PFM__
		PFM_DUMP_D();
		/* dump_log(); */
#endif
		break;
	case 'T':
		nand_get_device(mtd, FL_READING);
		DRV_WriteReg32(NFI_ACCCON_REG32, value);
		nand_release_device(mtd);
		break;
	default:
		break;
	}

	return len;
}

/******************************************************************************
 * mtk_nand_probe
 *
 * DESCRIPTION:
 *register the nand device file operations !
 *
 * PARAMETERS:
 *struct platform_device *pdev : device structure
 *
 * RETURNS:
 *0 : Success
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
#ifdef TLC_UNIT_TEST
__aligned(64)
static u8 temp_buffer_tlc[LPAGE + LSPARE];
__aligned(64)
static u8 temp_buffer_tlc_rd[LPAGE + LSPARE];
int mtk_tlc_unit_test(struct nand_chip *nand_chip, struct mtd_info *mtd)
{
	MSG(INIT, "Begin to Kernel tlc unit test ...\n");
	int err = 0;
	int patternbuff[128] = {
	0x0103D901, 0xFF1802DF, 0x01200400, 0x00000021, 0x02040122, 0x02010122, 0x03020407, 0x1A050103,
	0x00020F1B, 0x08C0C0A1, 0x01550800, 0x201B0AC1, 0x41990155, 0x64F0FFFF, 0x201B0C82, 0x4118EA61,
	0xF00107F6, 0x0301EE1B, 0x0C834118, 0xEA617001, 0x07760301, 0xEE151405, 0x00202020, 0x20202020,
	0x00202020, 0x2000302E, 0x3000FF14, 0x00FF0000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x01D90301, 0xDF0218FF, 0x00042001, 0x21000000, 0x22010402, 0x22010102, 0x07040203, 0x0301051A,
	0x1B0F0200, 0xA1C0C008, 0x00085501, 0xC10A1B20, 0x55019941, 0xFFFFF064, 0x820C1B20, 0x61EA1841,
	0xF60701F0, 0x1BEE0103, 0x1841830C, 0x017061EA, 0x01037607, 0x051415EE, 0x20202000, 0x20202020,
	0x20202000, 0x2E300020, 0x14FF0030, 0x0000FF00, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000
	};
	u32 j, k, p = g_block_size / g_page_size, m;
	u32 test_page;
	u8 *buf = vmalloc(g_block_size);

	pr_warn("[P] %d\n", p);

	for (m = 0; m < 32; m++)
		memcpy(temp_buffer_tlc+(512*m), (u8 *)patternbuff, 512);

	pr_warn("***************read pl***********************\n");
	memset(temp_buffer_tlc_rd, 0xA5, 16384);
	if (mtk_nand_read_page(mtd, nand_chip, temp_buffer_tlc_rd, 1 * p / 3))
		pr_warn("Read page 0x%x fail!\n", 1 * p / 3);
	for (m = 0; m < 32; m++)
		MSG(INIT, "[5]0x%x %x %x %x\n",
			*((int *)temp_buffer_tlc_rd+m*4), *((int *)temp_buffer_tlc_rd+1+m*4),
			*((int *)temp_buffer_tlc_rd+2+m*4), *((int *)temp_buffer_tlc_rd+3+m*4));

#ifdef NAND_PFM
	g_PFM_R = 0;
	g_PFM_W = 0;
	g_PFM_E = 0;
	g_PFM_RNum = 0;
	g_PFM_RD = 0;
	g_PFM_WD = 0;
#endif
#if 1
	pr_warn("***************SLC MODE TEST***********************\n");
	test_page = 84 * p + (p / 3);
	mtk_nand_erase(mtd, test_page);
	for (k = 0; k < (p/3); k++) {
		pr_warn("***************w p %d***********************\n", test_page + k);
		if (mtk_nand_write_page(mtd, nand_chip, 0, 0, temp_buffer_tlc, 0, test_page + k, 0, 0))
			pr_warn("Write page 0x%x fail!\n", test_page + k);

		pr_warn("***************r p %d***********************\n", test_page + k);
		memset(temp_buffer_tlc_rd, 0x00, g_page_size);
		if (mtk_nand_read_page(mtd, nand_chip, temp_buffer_tlc_rd, test_page + k))
			pr_warn("Read page 0x%x fail!\n", test_page + k);

		if (memcmp(temp_buffer_tlc, temp_buffer_tlc_rd, g_page_size)) {
			MSG(INIT, "compare fail!\n");
			err = 1;
			break;
		}
	}
#endif

#ifdef NAND_PFM
	g_PFM_R = 0;
	g_PFM_W = 0;
	g_PFM_E = 0;
	g_PFM_RNum = 0;
	g_PFM_RD = 0;
	g_PFM_WD = 0;
#endif

#if 1
	pr_warn("***************TLC MODE TEST***********************\n");
	test_page = 880 * p;
	mtk_nand_erase(mtd, test_page);

	memset(buf, 0x00, g_block_size);
	for (k = 0; k < p; k++)
		memcpy(buf + (g_page_size * k), temp_buffer_tlc, g_page_size);
	pr_warn("***************w b %d***********************\n", test_page);
	mtk_nand_write_tlc_block(mtd, nand_chip, buf, test_page, (gn_devinfo.blocksize * 1024));

	for (k = 0; k < p; k++) {
		pr_warn("***************r p %d***********************\n", test_page + k);
		memset(temp_buffer_tlc_rd, 0x00, g_page_size);
		if (mtk_nand_read_page(mtd, nand_chip, temp_buffer_tlc_rd, test_page + k))
			pr_warn("Read page 0x%x fail!\n", test_page + k);

		if (memcmp(temp_buffer_tlc, temp_buffer_tlc_rd, g_page_size)) {
			MSG(INIT, "compare fail!\n");
			err = 2;
			break;
		}
	}
#endif

	vfree(buf);
	return err;
}
#endif

#ifdef NAND_FEATURE_TEST
static int mtk_nand_test(struct mtd_info *mtd, struct nand_chip *nand_chip)
{
	u32 page, page1, page2;
	u32 block, block1;
	bool write_test = TRUE;

	while (0) {
		mtk_nand_read_page(mtd, nand_chip, temp_buffer, 0x34408);
		MSG(INIT, "Page: 0x34408 (0x8) bit flip: %d retry count %d\n", correct_count, g_hynix_retry_count);
		mtk_nand_rrtry_func(mtd, gn_devinfo, 0, FALSE);

		mdelay(3);
	}
	do {
		mtk_nand_read_page(mtd, nand_chip, test_buffer, 0x25500);
		mtk_nand_read_page(mtd, nand_chip, temp_buffer, 0x33C00);
		if (!memcmp(temp_buffer, test_buffer, 1000))
			write_test = FALSE;
		if (write_test) {
			MSG(INIT, "First step: erase and program #0 page\n");
			for (page = 0x33C00; page < 0x43C00; page += 256) {
				mtk_nand_erase_hw(mtd, page);
				mtk_nand_write_page(mtd, nand_chip , 0, 16384, test_buffer, 0, page, 0, 0);
			}
			MSG(INIT, "Second step: Create open block\n");
			page = 0x33C00;
			for (block = 1; block < 256; block++) {
				block1 = page/256;
				for (page1 = page + 1; page1 < page + block; page1++) {
					page2 = sandisk_pairpage_mapping((page1%256), TRUE);
					if (page2 != (page1%256))
						mtk_nand_read_page(mtd, nand_chip, temp_buffer, block1*256+page2);
					mtk_nand_write_page(mtd, nand_chip , 0, 16384, test_buffer, 0, page1, 0, 0);
				}
				page += 256;
			}
			MSG(INIT, "Third step: Check bit flip count\n");
		}
		page = 0x33C00;
		for (block = 1; block < 256; block++) {
			block1 = page/256;
			MSG(INIT, "====block 0x%x====\n", block1);

			for (page1 = page; page1 < page + block; page1++) {
				mtk_nand_read_page(mtd, nand_chip, temp_buffer, page1);
				MSG(INIT, "Page: 0x%x (0x%x) bit flip: %d\n", page1, page1%256, correct_count);
			}

			page += 256;
		}
		mtk_nand_read_page(mtd, nand_chip, temp_buffer, 0x43C00);
		write_test = TRUE;
		if (!memcmp(temp_buffer, test_buffer, 1000))
			write_test = FALSE;
		if (write_test) {
			MSG(INIT, "4th step: erase and program #0 page\n");
			for (page = 0x43C00; page < 0x53C00; page += 256) {
				mtk_nand_erase_hw(mtd, page);
				mtk_nand_write_page(mtd, nand_chip , 0, 16384, test_buffer, 0, page, 0, 0);
				mtk_nand_write_page(mtd, nand_chip , 0, 16384, test_buffer, 0, page+1, 0, 0);
				mtk_nand_write_page(mtd, nand_chip , 0, 16384, test_buffer, 0, page+2, 0, 0);
			}
			MSG(INIT, "5th step: Create open block\n");
			page = 0x43C00;
			for (block = 3; block < 256; block++) {
				block1 = page/256;
				for (page1 = page + 3; page1 < page+block; page1++) {
					page2 = sandisk_pairpage_mapping((page1%256), TRUE);
					if (page2 != (page1%256))
						mtk_nand_read_page(mtd, nand_chip, temp_buffer, block1*256+page2);
					mtk_nand_write_page(mtd, nand_chip , 0, 16384, test_buffer, 0, page1, 0, 0);
				}
				page += 256;
			}
			MSG(INIT, "6th step: Check bit flip count\n");
		}
		page = 0x43C00;
		for (block = 1; block < 256; block++) {
			block1 = page/256;
			MSG(INIT, "====WL block 0x%x====\n", block1);

			for (page1 = page; page1 < page+block; page1++) {
				mtk_nand_read_page(mtd, nand_chip, temp_buffer, page1);
				MSG(INIT, "WL Page: 0x%x (0x%x) bit flip: %d\n", page1, page1%256, correct_count);
			}

			page += 256;
		}
		mtk_nand_read_page(mtd, nand_chip, temp_buffer, 0x53C00);
		write_test = TRUE;
		if (!memcmp(temp_buffer, test_buffer, 1000))
			write_test = FALSE;
		if (write_test) {
			MSG(INIT, "7th step: erase and program\n");
			page = 0x53C00;
			for (block = 0; block <= 255; block++) {
				mtk_nand_erase_hw(mtd, page);
				block1 = page/256;
				for (page1 = page; page1 <= page+block; page1++) {
					page2 = sandisk_pairpage_mapping((page1%256), TRUE);
					if (page2 != (page1%256))
						mtk_nand_read_page(mtd, nand_chip, temp_buffer, block1*256+page2);
					mtk_nand_write_page(mtd, nand_chip , 0, 16384, test_buffer, 0, page1, 0, 0);
				}
				page += 256;
			}
			MSG(INIT, "6th step: Check bit flip count\n");
		}
		page = 0x53C00;
		for (block = 1; block < 256; block++) {
			block1 = page/256;
			MSG(INIT, "====Seq block 0x%x====\n", block1);

			for (page1 = page; page1 < page+block; page1++) {
				mtk_nand_read_page(mtd, nand_chip, temp_buffer, page1);
				MSG(INIT, "Seq Page: 0x%x (0x%x) bit flip: %d\n", page1, page1%256, correct_count);
			}

			page += 256;
		}
	} while (0)
		;
while (1)
	;
}
#endif

#define KERNEL_NAND_UNIT_TEST 0
#define NAND_READ_PERFORMANCE 0
#if KERNEL_NAND_UNIT_TEST
__aligned(64)
static u8 temp_buffer_xl[LPAGE + LSPARE];
__aligned(64)
static u8 temp_buffer_xl_rd[LPAGE + LSPARE];

int mtk_nand_unit_test(struct nand_chip *nand_chip, struct mtd_info *mtd)
{

	int err = 0;
	/*int patternbuff[128] = {
		0x0103D901, 0xFF1802DF, 0x01200400, 0x00000021, 0x02040122, 0x02010122, 0x03020407,
		0x1A050103,
		0x00020F1B, 0x08C0C0A1, 0x01550800, 0x201B0AC1, 0x41990155, 0x64F0FFFF, 0x201B0C82,
		0x4118EA61,
		0xF00107F6, 0x0301EE1B, 0x0C834118, 0xEA617001, 0x07760301, 0xEE151405, 0x00202020,
		0x20202020,
		0x00202020, 0x2000302E, 0x3000FF14, 0x00FF0000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000,
		0x01D90301, 0xDF0218FF, 0x00042001, 0x21000000, 0x22010402, 0x22010102, 0x07040203,
		0x0301051A,
		0x1B0F0200, 0xA1C0C008, 0x00085501, 0xC10A1B20, 0x55019941, 0xFFFFF064, 0x820C1B20,
		0x61EA1841,
		0xF60701F0, 0x1BEE0103, 0x1841830C, 0x017061EA, 0x01037607, 0x051415EE, 0x20202000,
		0x20202020,
		0x20202000, 0x2E300020, 0x14FF0030, 0x0000FF00, 0x00000000, 0x00000000, 0x00000000,
		0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000
	};*/
	u32 j, k, p = g_block_size / g_page_size;
	u32 TOTAL = 10;

	pr_err("Begin to Kernel nand unit test ...\n");

	pr_debug("[P] %x\n", p);


#if 0
	for (m = 0; m < 32; m++)
		memcpy(temp_buffer_xl + 512 * m, (u8 *) patternbuff, 512);

	pr_debug("***************read pl***********************\n");
	memset(temp_buffer_xl_rd, 0xA5, 16384);
	if (mtk_nand_read_page(mtd, nand_chip, temp_buffer_xl_rd, 1 * p))
		pr_debug("Read page 0x%x fail!\n", 1 * p);
	for (m = 0; m < 32; m++)
		pr_debug("[5]0x%x %x %x %x\n", *((int *)temp_buffer_xl_rd + m * 4),
			*((int *)temp_buffer_xl_rd + 1 + m * 4),
			*((int *)temp_buffer_xl_rd + 2 + m * 4),
			*((int *)temp_buffer_xl_rd + 3 + m * 4));
#endif

#if 0
			pr_debug("*****Normal mode test %d***********\n", p);

	gn_devinfo.tlcControl.slcopmodeEn = FALSE;
	for (j = 0x400; j < 0x410; j++) {
		/* memset(local_buffer, 0x00, 16384); */
		/* mtk_nand_read_page(mtd, nand_chip, local_buffer, j*p); */
		/* for(m = 0; m < 32; m++) */
		/* MSG(INIT,"[1]0x%x %x %x %x\n",
				*((int *)local_buffer+m*4), *((int *)local_buffer+1+m*4),
				*((int *)local_buffer+2+m*4), *((int *)local_buffer+3+m*4)); */
		mtk_nand_erase(mtd, j * p);
		memset(temp_buffer_xl_rd, 0x00, 16384);
		if (mtk_nand_read_page(mtd, nand_chip, temp_buffer_xl_rd, j * p))
			pr_debug("Read page 0x%x fail!\n", j * p);
		pr_debug("[2]0x%x %x %x %x\n", *(int *)temp_buffer_xl_rd,
			*((int *)temp_buffer_xl_rd + 1), *((int *)temp_buffer_xl_rd + 2),
			*((int *)temp_buffer_xl_rd + 3));
		if (mtk_nand_block_bad(mtd, j * g_block_size, 0)) {
			pr_debug("Bad block at %x\n", j);
			continue;
		}
		for (k = 0; k < p; k++) {
			pr_debug("***************w b***********************\n");

			for (m = 0; m < 32; m++)
				pr_debug("[1]0x%x %x %x %x\n", *((int *)temp_buffer_xl + m * 4),
					*((int *)temp_buffer_xl + 1 + m * 4),
					*((int *)temp_buffer_xl + 2 + m * 4),
					*((int *)temp_buffer_xl + 3 + m * 4));

			if (mtk_nand_write_page(mtd, nand_chip, 0, 0, temp_buffer_xl /*(u8 *)patternbuff */ , 0,
				 j * p + k, 0, 0))
				pr_debug("Write page 0x%x fail!\n", j * p + k);
			/* #if 1 */
			/* } */
			/* TOTAL=1000; */
			/* do{ */
			/* for (k = 0; k < p; k++) */
			/* { */
			/* #endif */
			pr_debug("***************r b***********************\n");
			memset(temp_buffer_xl_rd, 0x00, g_page_size);
			if (mtk_nand_read_page(mtd, nand_chip, temp_buffer_xl_rd, j * p + k))
				pr_debug("Read page 0x%x fail!\n", j * p + k);
			for (m = 0; m < 32; m++)
				pr_debug("[3]0x%x %x %x %x\n", *((int *)temp_buffer_xl_rd + m * 4),
					*((int *)temp_buffer_xl_rd + 1 + m * 4),
					*((int *)temp_buffer_xl_rd + 2 + m * 4),
					*((int *)temp_buffer_xl_rd + 3 + m * 4));
			if (memcmp(temp_buffer_xl /*(u8 *)patternbuff */ , temp_buffer_xl_rd,
				   512 /*g_page_size */)) {
				pr_debug("[KERNEL_NAND_UNIT_TEST] compare fail!\n");
				err = -1;
				while (1)
					;
			} else {
				TOTAL--;
				pr_debug("[KERNEL_NAND_UNIT_TEST] compare OK!\n");
			}
		}
		/* }while(TOTAL); */
#if 0
		mtk_nand_SetFeature(mtd, (u16) feature_set->sfeatureCmd,
					feature_set->Async_timing.address, (u8 *) &val,
					sizeof(feature_set->Async_timing.feature));
		mtk_nand_GetFeature(mtd, feature_set->gfeatureCmd,
					feature_set->Async_timing.address, (u8 *) &val, 4);
		pr_debug("[ASYNC Interface]0x%X\n", val);
		err = mtk_nand_interface_config(mtd);
		MSG(INIT, "[nand_interface_config] %d\n", err);
#endif
	}
		p = (p>>1);
#endif

				pr_err("*****SLC mode test %d***********\n", p);
				force_slc_flag = 1;

	for (j = 0x80; j < 0x100; j++) {
		/* memset(local_buffer, 0x00, 16384); */
		/* mtk_nand_read_page(mtd, nand_chip, local_buffer, j*p); */
		/* for(m = 0; m < 32; m++) */
		/* MSG(INIT,"[1]0x%x %x %x %x\n",
				*((int *)local_buffer+m*4), *((int *)local_buffer+1+m*4),
				*((int *)local_buffer+2+m*4), *((int *)local_buffer+3+m*4)); */
		mtk_nand_erase(mtd, j * p);
		memset(temp_buffer_xl_rd, 0x00, 16384);
		/*pr_debug("[2]0x%x %x %x %x\n", *(int *)temp_buffer_xl_rd,
			*((int *)temp_buffer_xl_rd + 1), *((int *)temp_buffer_xl_rd + 2),
			*((int *)temp_buffer_xl_rd + 3));
		if (mtk_nand_block_bad(mtd, j * g_block_size, 0)) {
			pr_debug("Bad block at %x\n", j);
			continue;
		}*/
		for (k = 0; k < p; k += 2) {
			pr_err("***************w b***********************\n");

			/*for (m = 0; m < 32; m++)
				pr_err("[1]0x%x %x %x %x\n", *((int *)temp_buffer_xl + m * 4),
					*((int *)temp_buffer_xl + 1 + m * 4),
					*((int *)temp_buffer_xl + 2 + m * 4),
					*((int *)temp_buffer_xl + 3 + m * 4)); */

			if (mtk_nand_write_page(mtd, nand_chip, 0, 0, temp_buffer_xl /*(u8 *)patternbuff */ , 0,
				 j * p + k, 0, 0))
				pr_err("Write page 0x%x fail!\n", j * p + k);

			pr_err("***************r b***********************\n");
			memset(temp_buffer_xl_rd, 0x00, g_page_size);
			if (mtk_nand_read_page(mtd, nand_chip, temp_buffer_xl_rd, j * p + k))
				pr_err("Read page 0x%x fail!\n", j * p + k);
			/*for (m = 0; m < 32; m++)
				pr_err("[3]0x%x %x %x %x\n", *((int *)temp_buffer_xl_rd + m * 4),
					*((int *)temp_buffer_xl_rd + 1 + m * 4),
					*((int *)temp_buffer_xl_rd + 2 + m * 4),
					*((int *)temp_buffer_xl_rd + 3 + m * 4));*/
			if (memcmp(temp_buffer_xl /*(u8 *)patternbuff */ , temp_buffer_xl_rd,
				   512 /*g_page_size */)) {
				pr_err("[KERNEL_NAND_UNIT_TEST] compare fail!\n");
				err = -1;
				break;
			}
			TOTAL--;
			pr_err("[KERNEL_NAND_UNIT_TEST] compare OK!\n");
		}

			pr_err("**********Read back check**************\n");

		for (k = 0; k < p; k += 2) {
			if (mtk_nand_read_page(mtd, nand_chip, temp_buffer_xl_rd, j * p + k))
				pr_err("Read page 0x%x fail!\n", j * p + k);

			if (memcmp(temp_buffer_xl /*(u8 *)patternbuff */ , temp_buffer_xl_rd,
				   512 /*g_page_size */)) {
				pr_err("[KERNEL_NAND_UNIT_TEST] compare fail!\n");
				err = -1;
				break;
			}
			TOTAL--;
			pr_err("[KERNEL_NAND_UNIT_TEST] compare OK!\n");
		}
		/* }while(TOTAL); */

	}

					pr_err("*****Unit test END***********\n");

	return err;
}
#endif

static int mtk_nand_probe(struct platform_device *pdev)
{

	struct mtk_nand_host_hw *hw;
	struct mtd_info *mtd;
	struct nand_chip *nand_chip;
	int err = 0;
	int bmt_sz;
	u8 id[NAND_MAX_ID];
	int i;
	u32 sector_size = NAND_SECTOR_SIZE;
	u64 temp;
#ifdef MTK_NAND_CMD_DUMP
	  dbg_inf[0].cmd.cmd_array[0] = 0xFF;
	  dbg_inf[0].cmd.cmd_array[1] = 0xFF;
	  dbg_inf[0].cmd.address[0] = 0xFF;
	  dbg_inf[0].cmd.address[1] = 0xFF;
	  dbg_inf[0].cmd.address[2] = 0xFF;
#endif
	g_b2Die_CS = FALSE;
#if 1
	MSG(INIT, "Enable NFI and NFIECC Clock\n");
	enable_clock(MT_CG_NFI_BUS_SW_CG, "NFI");
	enable_clock(MT_CG_NFI_SW_CG, "NFI");
	enable_clock(MT_CG_NFI2X_SW_CG, "NFI");
	enable_clock(MT_CG_NFIECC_SW_CG, "NFI");
	clkmux_sel(MT_CLKMUX_NFI1X_INFRA_SEL, MT_CG_SYS_26M, "NFI");
#endif

	mtk_nfi_node = of_find_compatible_node(NULL, NULL, "mediatek,NFI");
	mtk_nfi_base = of_iomap(mtk_nfi_node, 0);

	MSG(INIT, "of_iomap for nfi base @ 0x%p\n", mtk_nfi_base);

	if (mtk_nfiecc_node == NULL) {
		mtk_nfiecc_node = of_find_compatible_node(NULL, NULL, "mediatek,NFIECC");
		mtk_nfiecc_base = of_iomap(mtk_nfiecc_node, 0);
		MSG(INIT, "of_iomap for nfiecc base @ 0x%p\n", mtk_nfiecc_base);
	}
	if (mtk_io_node == NULL) {
		mtk_io_node = of_find_compatible_node(NULL, NULL, "mediatek,IOCFG_B");
		mtk_io_base = of_iomap(mtk_io_node, 0);
		MSG(INIT, "of_iomap for io base @ 0x%p\n", mtk_io_base);
	}
	mtk_pm_node = of_find_compatible_node(NULL, NULL,
		"mediatek,mt_pmic_regulator_supply");
	if (nfi_reg_vemc_3v3 == NULL)
		nfi_reg_vemc_3v3 = regulator_get(&(pdev->dev), "vemc3v3");
	nfi_irq = irq_of_parse_and_map(pdev->dev.of_node, 0);

	hw = &mtk_nand_hw;
	BUG_ON(!hw);
	regulator_set_voltage(nfi_reg_vemc_3v3, 3300000, 3300000);
	err = regulator_enable(nfi_reg_vemc_3v3);
	if (err)
		pr_err("nfi ldo enable fail!!\n");

	/* Allocate memory for the device structure (and zero it) */
	host = kzalloc(sizeof(struct mtk_nand_host), GFP_KERNEL);
	if (!host) {
		MSG(INIT, "mtk_nand: failed to allocate device structure.\n");
		nand_disable_clock();
		return -ENOMEM;
	}

	/* Allocate memory for 16 byte aligned buffer */
	local_buffer_16_align = local_buffer;
	temp_buffer_16_align  = temp_buffer;
	pr_debug("Allocate 16 byte aligned buffer: %p\n", local_buffer_16_align);

	host->hw = hw;
	PL_TIME_PROG(10);
	PL_TIME_ERASE(10);
	PL_TIME_PROG_WDT_SET(1);
	PL_TIME_ERASE_WDT_SET(1);

	/* init mtd data structure */
	nand_chip = &host->nand_chip;
	nand_chip->priv = host;	 /* link the private data structures */

	mtd = &host->mtd;
	mtd->priv = nand_chip;
	mtd->owner = THIS_MODULE;
	mtd->name = "MTK-Nand";
	mtd->eraseregions = host->erase_region;

	hw->nand_ecc_mode = NAND_ECC_HW;

	/* Set address of NAND IO lines */
	nand_chip->IO_ADDR_R = (void __iomem *)NFI_DATAR_REG32;
	nand_chip->IO_ADDR_W = (void __iomem *)NFI_DATAW_REG32;
	nand_chip->chip_delay = 20; /* 20us command delay time */
	nand_chip->ecc.mode = hw->nand_ecc_mode;	/* enable ECC */

	nand_chip->read_byte = mtk_nand_read_byte;
	nand_chip->read_buf = mtk_nand_read_buf;
	nand_chip->write_buf = mtk_nand_write_buf;
#ifdef CONFIG_MTD_NAND_VERIFY_WRITE
	nand_chip->verify_buf = mtk_nand_verify_buf;
#endif
	nand_chip->select_chip = mtk_nand_select_chip;
	nand_chip->dev_ready = mtk_nand_dev_ready;
	nand_chip->cmdfunc = mtk_nand_command_bp;
	nand_chip->ecc.read_page = mtk_nand_read_page_hwecc;
	nand_chip->ecc.write_page = mtk_nand_write_page_hwecc;

	nand_chip->ecc.layout = &nand_oob_64;
	nand_chip->ecc.size = hw->nand_ecc_size;
	nand_chip->ecc.bytes = hw->nand_ecc_bytes;

	nand_chip->options = NAND_SKIP_BBTSCAN;

	/* For BMT, we need to revise driver architecture */
	nand_chip->write_page = mtk_nand_write_page;
	nand_chip->read_page = mtk_nand_read_page;
	nand_chip->read_subpage = mtk_nand_read_subpage;
	nand_chip->ecc.write_oob = mtk_nand_write_oob;
	nand_chip->ecc.read_oob = mtk_nand_read_oob;
	nand_chip->block_markbad = mtk_nand_block_markbad;
	nand_chip->erase_hw = mtk_nand_erase;
	nand_chip->block_bad = mtk_nand_block_bad;
	nand_chip->init_size = mtk_nand_init_size;
	mtk_nand_init_hw(host);
	/* Select the device */
	nand_chip->select_chip(mtd, NFI_DEFAULT_CS);

	/*
	 * Reset the chip, required by some chips (e.g. Micron MT29FxGxxxxx)
	 * after power-up
	 */





	DDR_INTERFACE = FALSE;
	mtk_nand_reset();

	DRV_WriteReg16(NFI_CNFG_REG16, CNFG_OP_RESET);
	mtk_nand_set_command(NAND_CMD_RESET);


	*NFI_CNRNB_REG16 = 0xF1;
	while (!(DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY_RETURN))
		;
	mtk_nand_reset();

	/* Send the command for reading device ID */
	nand_chip->cmdfunc(mtd, NAND_CMD_READID, 0x00, -1);

	for (i = 0; i < NAND_MAX_ID; i++)
		id[i] = nand_chip->read_byte(mtd);

	manu_id = id[0];
	dev_id = id[1];

	if (!get_device_info(id, &gn_devinfo))
		MSG(INIT, "Not Support this Device! \r\n");

	if (gn_devinfo.pagesize == 16384) {
		nand_chip->ecc.layout = &nand_oob_128;
		hw->nand_ecc_size = 16384;
	} else if (gn_devinfo.pagesize == 8192) {
		nand_chip->ecc.layout = &nand_oob_128;
		hw->nand_ecc_size = 8192;
	} else if (gn_devinfo.pagesize == 4096) {
		nand_chip->ecc.layout = &nand_oob_128;
		hw->nand_ecc_size = 4096;
	} else if (gn_devinfo.pagesize == 2048) {
		nand_chip->ecc.layout = &nand_oob_64;
		hw->nand_ecc_size = 2048;
	} else if (gn_devinfo.pagesize == 512) {
		nand_chip->ecc.layout = &nand_oob_16;
		hw->nand_ecc_size = 512;
	}
	if (gn_devinfo.sectorsize == 1024) {
		sector_size = 1024;
		hw->nand_sec_shift = 10;
		hw->nand_sec_size = 1024;
		NFI_CLN_REG32(NFI_PAGEFMT_REG16, PAGEFMT_SECTOR_SEL);
	}
	if (gn_devinfo.pagesize <= 4096) {
		nand_chip->ecc.layout->eccbytes =
			gn_devinfo.sparesize -
			OOB_AVAI_PER_SECTOR * (gn_devinfo.pagesize / sector_size);
		hw->nand_ecc_bytes = nand_chip->ecc.layout->eccbytes;
		/* Modify to fit device character	*/
		nand_chip->ecc.size = hw->nand_ecc_size;
		nand_chip->ecc.bytes = hw->nand_ecc_bytes;
	} else {
		nand_chip->ecc.layout->eccbytes = 64;
		/*gn_devinfo.sparesize - OOB_AVAI_PER_SECTOR * (gn_devinfo.pagesize / sector_size); */
		hw->nand_ecc_bytes = nand_chip->ecc.layout->eccbytes;
		/* Modify to fit device character	*/
		nand_chip->ecc.size = hw->nand_ecc_size;
		nand_chip->ecc.bytes = hw->nand_ecc_bytes;
	}
	nand_chip->subpagesize = gn_devinfo.sectorsize;
	nand_chip->subpage_size = gn_devinfo.sectorsize;

	for (i = 0; i < nand_chip->ecc.layout->eccbytes; i++)
		nand_chip->ecc.layout->eccpos[i] =
			OOB_AVAI_PER_SECTOR * (gn_devinfo.pagesize / sector_size) + i;


	if (gn_devinfo.vendor != VEND_NONE)
		mtk_nand_randomizer_config(&gn_devinfo.feature_set.randConfig, 0);
	if ((gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_HYNIX_16NM)
		|| (gn_devinfo.feature_set.FeatureSet.rtype == RTYPE_HYNIX))
		HYNIX_RR_TABLE_READ(&gn_devinfo);
	hw->nfi_bus_width = gn_devinfo.iowidth;
#if 0
	if (gn_devinfo.vendor != VEND_NONE) {
		if (gn_devinfo.feature_set.FeatureSet.Async_timing.feature != 0xFF) {
			struct gFeatureSet *feature_set = &(gn_devinfo.feature_set.FeatureSet);

			mtk_nand_SetFeature(mtd, (u16) feature_set->sfeatureCmd,
						feature_set->Async_timing.address,
						(u8 *) &feature_set->Async_timing.feature,
		sizeof(feature_set->Async_timing.feature));
		}
	}
#endif
	DRV_WriteReg32(NFI_ACCCON_REG32, gn_devinfo.timmingsetting);

	/* 16-bit bus width */
	if (hw->nfi_bus_width == 16) {
		MSG(INIT, "%s : Set the 16-bit I/O settings!\n", MODULE_NAME);
		nand_chip->options |= NAND_BUSWIDTH_16;
	}

	mtk_dev = &pdev->dev;
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	if (dma_set_mask(&pdev->dev, DMA_BIT_MASK(32))) {
		dev_err(&pdev->dev, "set dma mask fail\n");
		pr_warn("[NAND] set dma mask fail\n");
	} else
		pr_debug("[NAND] set dma mask ok\n");

	err = request_irq(MT_NFI_IRQ_ID, mtk_nand_irq_handler, IRQF_TRIGGER_NONE, "mtk-nand", NULL);

	if (0 != err) {
		MSG(INIT, "%s : Request IRQ fail: err = %d\n", MODULE_NAME, err);
		goto out;
	}

	if (g_i4Interrupt)
		enable_irq(MT_NFI_IRQ_ID);
	else
		disable_irq(MT_NFI_IRQ_ID);

#if 0
	if (gn_devinfo.advancedmode & CACHE_READ)
		nand_chip->ecc.read_multi_page_cache = NULL;
	else
		nand_chip->ecc.read_multi_page_cache = NULL;
#endif
	mtd->oobsize = gn_devinfo.sparesize;
	/* Scan to find existence of the device */
	if (nand_scan(mtd, hw->nfi_cs_num)) {
		MSG(INIT, "%s : nand_scan fail.\n", MODULE_NAME);
		err = -ENXIO;
		goto out;
	}

	g_page_size = mtd->writesize;
	g_block_size = gn_devinfo.blocksize << 10;
	PAGES_PER_BLOCK = (u32)(g_block_size / g_page_size);

	temp = nand_chip->chipsize;
	do_div(temp, ((gn_devinfo.blocksize * 1024) & 0xFFFFFFFF));
	bmt_sz = (int)(((u32) temp) / 100 * 6);
	platform_set_drvdata(pdev, host);

	if (hw->nfi_bus_width == 16)
		NFI_SET_REG32(NFI_PAGEFMT_REG16, PAGEFMT_DBYTE_EN);

	nand_chip->select_chip(mtd, 0);
	nand_chip->chipsize -= (bmt_sz * g_block_size);

	mtd->size = nand_chip->chipsize;

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	mtk_pmt_reset();
#endif

	if (!g_bmt) {
		g_bmt = init_bmt(nand_chip, bmt_sz);
		if (!g_bmt) {
			MSG(INIT, "Error: init bmt failed\n");
				nand_disable_clock();
			return 0;
		}
	}

	nand_chip->chipsize -= (PMT_POOL_SIZE) * (gn_devinfo.blocksize * 1024);
	mtd->size = nand_chip->chipsize;
#if KERNEL_NAND_UNIT_TEST
	err = mtk_nand_unit_test(nand_chip, mtd);
	if (err == 0)
		pr_warn("Thanks to GOD, UNIT Test OK!\n");
#endif

#ifdef PMT
	part_init_pmt(mtd, (u8 *) &g_exist_Partition[0]);
/*
	for (i = 0; i < 3000; i++) {
		pr_warn("waste time %d\n", i);
		mtk_nand_block_bad_hw(mtd, 0x17a00000);
	}
*/
	err = mtd_device_register(mtd, g_exist_Partition, part_num);
#else
	err = mtd_device_register(mtd, g_pasStatic_Partition, part_num);
#endif

#ifdef TLC_UNIT_TEST
	err = mtk_tlc_unit_test(nand_chip, mtd);
	switch (err) {
	case 0:
		pr_warn("TLC UNIT Test OK!\n");
		pr_warn("TLC UNIT Test OK!\n");
		pr_warn("TLC UNIT Test OK!\n");
		break;
	case 1:
		pr_warn("TLC UNIT Test fail: SLC mode fail!\n");
		pr_warn("TLC UNIT Test fail: SLC mode fail!\n");
		pr_warn("TLC UNIT Test fail: SLC mode fail!\n");
		break;
	case 2:
		pr_warn("TLC UNIT Test fail: TLC mode fail!\n");
		pr_warn("TLC UNIT Test fail: TLC mode fail!\n");
		pr_warn("TLC UNIT Test fail: TLC mode fail!\n");
		break;
	default:
		pr_warn("TLC UNIT Test fail: wrong return!\n");
		break;
	}
#endif

#ifdef _MTK_NAND_DUMMY_DRIVER_
	dummy_driver_debug = 0;
#endif

#if defined(CFG_SNAND_ACCESS_PATTERN_LOGGER_BOOTUP) && defined(CFG_SNAND_ACCESS_PATTERN_LOGGER)
	g_snand_pm_on = 1;
	g_snand_pm_cnt = 0;
	g_snand_pm_wrapped = 0;
#endif
	/* Successfully!! */
	if (!err) {
		MSG(INIT, "[mtk_nand] probe successfully!\n");
		nand_disable_clock();
		return err;
	}

	/* Fail!! */
out:
	MSG(INIT, "[NFI] mtk_nand_probe fail, err = %d!\n", err);
	nand_release(mtd);
	platform_set_drvdata(pdev, NULL);
	kfree(host);
	nand_disable_clock();
	return err;
}

/******************************************************************************
 * mtk_nand_suspend
 *
 * DESCRIPTION:
 *Suspend the nand device!
 *
 * PARAMETERS:
 *struct platform_device *pdev : device structure
 *
 * RETURNS:
 *0 : Success
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static int mtk_nand_suspend(struct platform_device *pdev, pm_message_t state)
{
	  struct mtk_nand_host *host = platform_get_drvdata(pdev);
	/* backup register */
#ifdef CONFIG_PM

	if (host->saved_para.suspend_flag == 0) {
		nand_enable_clock();
		/* Save NFI register */
		host->saved_para.sNFI_CNFG_REG16 = DRV_Reg16(NFI_CNFG_REG16);
		host->saved_para.sNFI_PAGEFMT_REG16 = DRV_Reg32(NFI_PAGEFMT_REG16);
		host->saved_para.sNFI_CON_REG16 = DRV_Reg32(NFI_CON_REG16);
		host->saved_para.sNFI_ACCCON_REG32 = DRV_Reg32(NFI_ACCCON_REG32);
		host->saved_para.sNFI_INTR_EN_REG16 = DRV_Reg16(NFI_INTR_EN_REG16);
		host->saved_para.sNFI_IOCON_REG16 = DRV_Reg16(NFI_IOCON_REG16);
		host->saved_para.sNFI_CSEL_REG16 = DRV_Reg16(NFI_CSEL_REG16);
		host->saved_para.sNFI_DEBUG_CON1_REG16 = DRV_Reg16(NFI_DEBUG_CON1_REG16);

		/* save ECC register */
		host->saved_para.sECC_ENCCNFG_REG32 = DRV_Reg32(ECC_ENCCNFG_REG32);
		host->saved_para.sECC_DECCNFG_REG32 = DRV_Reg32(ECC_DECCNFG_REG32);

		host->saved_para.suspend_flag = 1;
		mtk_nand_interface_async();
		nand_disable_clock();
		regulator_disable(nfi_reg_vemc_3v3);
	} else
		MSG(POWERCTL, "[NFI] Suspend twice !\n");

#endif

	  MSG(POWERCTL, "[NFI] Suspend !\n");
	  return 0;
}

/******************************************************************************
 * mtk_nand_resume
 *
 * DESCRIPTION:
 *Resume the nand device!
 *
 * PARAMETERS:
 *struct platform_device *pdev : device structure
 *
 * RETURNS:
 *0 : Success
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
static int mtk_nand_resume(struct platform_device *pdev)
{
	struct mtk_nand_host *host = platform_get_drvdata(pdev);
	int ret = 0;

#ifdef CONFIG_PM

	if (host->saved_para.suspend_flag == 1) {
			regulator_set_voltage(nfi_reg_vemc_3v3, 3300000, 3300000);
			ret = regulator_enable(nfi_reg_vemc_3v3);
			if (ret)
				pr_err("nfi ldo enable fail!!\n");
			nand_enable_clock();
		/* restore NFI register */
			DRV_WriteReg16(NFI_CNFG_REG16 , host->saved_para.sNFI_CNFG_REG16);
			DRV_WriteReg32(NFI_PAGEFMT_REG16 , host->saved_para.sNFI_PAGEFMT_REG16);
			DRV_WriteReg32(NFI_CON_REG16 , host->saved_para.sNFI_CON_REG16);
			DRV_WriteReg32(NFI_ACCCON_REG32 , host->saved_para.sNFI_ACCCON_REG32);
			DRV_WriteReg16(NFI_IOCON_REG16 , host->saved_para.sNFI_IOCON_REG16);
			DRV_WriteReg16(NFI_CSEL_REG16 , host->saved_para.sNFI_CSEL_REG16);
			DRV_WriteReg16(NFI_DEBUG_CON1_REG16 , host->saved_para.sNFI_DEBUG_CON1_REG16);

		/* restore ECC register */
			DRV_WriteReg32(ECC_ENCCNFG_REG32 , host->saved_para.sECC_ENCCNFG_REG32);
			DRV_WriteReg32(ECC_DECCNFG_REG32 , host->saved_para.sECC_DECCNFG_REG32);

		/* Reset NFI and ECC state machine */
			/* Reset the state machine and data FIFO, because flushing FIFO */
			(void)mtk_nand_reset();
			DRV_WriteReg16(ECC_DECCON_REG16, DEC_DE);
		while (!DRV_Reg16(ECC_DECIDLE_REG16))
			;
			DRV_WriteReg16(ECC_ENCCON_REG16, ENC_DE);
		while (!DRV_Reg32(ECC_ENCIDLE_REG32))
			;

		/* Initialize interrupt. Clear interrupt, read clear. */
			DRV_Reg16(NFI_INTR_REG16);

			DRV_WriteReg16(NFI_INTR_EN_REG16 , host->saved_para.sNFI_INTR_EN_REG16);
			mtk_nand_device_reset();
			(void)mtk_nand_reset();

			nand_disable_clock();
			host->saved_para.suspend_flag = 0;
	} else {
			MSG(POWERCTL, "[NFI] Resume twice !\n");
		}
#endif
	MSG(POWERCTL, "[NFI] Resume !\n");
	return 0;
}

/******************************************************************************
 * mtk_nand_remove
 *
 * DESCRIPTION:
 *unregister the nand device file operations !
 *
 * PARAMETERS:
 *struct platform_device *pdev : device structure
 *
 * RETURNS:
 *0 : Success
 *
 * NOTES:
 *None
 *
 ******************************************************************************/

static int mtk_nand_remove(struct platform_device *pdev)
{
	struct mtk_nand_host *host = platform_get_drvdata(pdev);
	struct mtd_info *mtd = &host->mtd;

	nand_release(mtd);

	kfree(host);

	nand_disable_clock();

	return 0;
}

/******************************************************************************
 * NAND OTP operations
 * ***************************************************************************/
#if (defined(NAND_OTP_SUPPORT) && SAMSUNG_OTP_SUPPORT)
unsigned int samsung_OTPQueryLength(unsigned int *QLength)
{
	*QLength = SAMSUNG_OTP_PAGE_NUM * g_page_size;
	return 0;
}

unsigned int samsung_OTPRead(unsigned int PageAddr, void *BufferPtr, void *SparePtr)
{
	struct mtd_info *mtd = &host->mtd;
	unsigned int rowaddr, coladdr;
	unsigned int u4Size = g_page_size;
	unsigned int timeout = 0xFFFF;
	unsigned int bRet;
	unsigned int sec_num = mtd->writesize >> host->hw->nand_sec_shift;

	if (PageAddr >= SAMSUNG_OTP_PAGE_NUM)
		return OTP_ERROR_OVERSCOPE;

	/* Col -> Row; LSB first */
	coladdr = 0x00000000;
	rowaddr = Samsung_OTP_Page[PageAddr];

	MSG(OTP, "[%s]:(COLADDR) [0x%08x]/(ROWADDR)[0x%08x]\n", __func__, coladdr, rowaddr);

	/* Power on NFI HW component. */
	nand_get_device(mtd, FL_READING);
	mtk_nand_reset();
	(void)mtk_nand_set_command(0x30);
	mtk_nand_reset();
	(void)mtk_nand_set_command(0x65);

	MSG(OTP, "[%s]: Start to read data from OTP area\n", __func__);

	if (!mtk_nand_reset()) {
		bRet = OTP_ERROR_RESET;
		goto cleanup;
	}

	mtk_nand_set_mode(CNFG_OP_READ);
	NFI_SET_REG16(NFI_CNFG_REG16, CNFG_READ_EN);
	DRV_WriteReg32(NFI_CON_REG16, sec_num << CON_NFI_SEC_SHIFT);

	DRV_WriteReg32(NFI_STRADDR_REG32, __virt_to_phys(BufferPtr));
	NFI_SET_REG16(NFI_CNFG_REG16, CNFG_AHB);

	if (g_bHwEcc)
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
	else
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);

	mtk_nand_set_autoformat(true);
	if (g_bHwEcc)
		ECC_Decode_Start();

	if (!mtk_nand_set_command(NAND_CMD_READ0)) {
		bRet = OTP_ERROR_BUSY;
		goto cleanup;
	}

	if (!mtk_nand_set_address(coladdr, rowaddr, 2, 3)) {
		bRet = OTP_ERROR_BUSY;
		goto cleanup;
	}

	if (!mtk_nand_set_command(NAND_CMD_READSTART)) {
		bRet = OTP_ERROR_BUSY;
		goto cleanup;
	}

	if (!mtk_nand_status_ready(STA_NAND_BUSY)) {
		bRet = OTP_ERROR_BUSY;
		goto cleanup;
	}

	if (!mtk_nand_read_page_data(mtd, BufferPtr, u4Size)) {
		bRet = OTP_ERROR_BUSY;
		goto cleanup;
	}

	if (!mtk_nand_status_ready(STA_NAND_BUSY)) {
		bRet = OTP_ERROR_BUSY;
		goto cleanup;
	}

	mtk_nand_read_fdm_data(SparePtr, sec_num);

	mtk_nand_stop_read();

	MSG(OTP, "[%s]: End to read data from OTP area\n", __func__);

	bRet = OTP_SUCCESS;

cleanup:

	mtk_nand_reset();
	(void)mtk_nand_set_command(0xFF);
	nand_release_device(mtd);
	return bRet;
}

unsigned int samsung_OTPWrite(unsigned int PageAddr, void *BufferPtr, void *SparePtr)
{
	struct mtd_info *mtd = &host->mtd;
	unsigned int rowaddr, coladdr;
	unsigned int u4Size = g_page_size;
	unsigned int timeout = 0xFFFF;
	unsigned int bRet;
	unsigned int sec_num = mtd->writesize >> 9;

	if (PageAddr >= SAMSUNG_OTP_PAGE_NUM)
		return OTP_ERROR_OVERSCOPE;

	/* Col -> Row; LSB first */
	coladdr = 0x00000000;
	rowaddr = Samsung_OTP_Page[PageAddr];

	MSG(OTP, "[%s]:(COLADDR) [0x%08x]/(ROWADDR)[0x%08x]\n", __func__, coladdr, rowaddr);
	nand_get_device(mtd, FL_READING);
	mtk_nand_reset();
	(void)mtk_nand_set_command(0x30);
	mtk_nand_reset();
	(void)mtk_nand_set_command(0x65);

	MSG(OTP, "[%s]: Start to write data to OTP area\n", __func__);

	if (!mtk_nand_reset()) {
		bRet = OTP_ERROR_RESET;
		goto cleanup;
	}

	mtk_nand_set_mode(CNFG_OP_PRGM);

	NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_READ_EN);

	DRV_WriteReg32(NFI_CON_REG16, sec_num << CON_NFI_SEC_SHIFT);

	DRV_WriteReg32(NFI_STRADDR_REG32, __virt_to_phys(BufferPtr));
	NFI_SET_REG16(NFI_CNFG_REG16, CNFG_AHB);

	if (g_bHwEcc)
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
	else
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);

	mtk_nand_set_autoformat(true);

	ECC_Encode_Start();

	if (!mtk_nand_set_command(NAND_CMD_SEQIN)) {
		bRet = OTP_ERROR_BUSY;
		goto cleanup;
	}

	if (!mtk_nand_set_address(coladdr, rowaddr, 2, 3)) {
		bRet = OTP_ERROR_BUSY;
		goto cleanup;
	}

	if (!mtk_nand_status_ready(STA_NAND_BUSY)) {
		bRet = OTP_ERROR_BUSY;
		goto cleanup;
	}

	mtk_nand_write_fdm_data((struct nand_chip *)mtd->priv, BufferPtr, sec_num);
	(void)mtk_nand_write_page_data(mtd, BufferPtr, u4Size);
	if (!mtk_nand_check_RW_count(u4Size)) {
		MSG(OTP, "[%s]: Check RW count timeout !\n", __func__);
		bRet = OTP_ERROR_TIMEOUT;
		goto cleanup;
	}

	mtk_nand_stop_write();
	(void)mtk_nand_set_command(NAND_CMD_PAGEPROG);
	while (DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY)
		;
	bRet = OTP_SUCCESS;

	MSG(OTP, "[%s]: End to write data to OTP area\n", __func__);

cleanup:
	mtk_nand_reset();
	(void)mtk_nand_set_command(NAND_CMD_RESET);
	nand_release_device(mtd);
	return bRet;
}

static int mt_otp_open(struct inode *inode, struct file *filp)
{
	MSG(OTP, "[%s]: (MAJOR)%d: (MINOR)%d\n", __func__, MAJOR(inode->i_rdev),
		MINOR(inode->i_rdev));
	filp->private_data = (int *)OTP_MAGIC_NUM;
	return 0;
}

static int mt_otp_release(struct inode *inode, struct file *filp)
{
	MSG(OTP, "[%s]: (MAJOR)%d: (MINOR)%d\n", __func__, MAJOR(inode->i_rdev),
		MINOR(inode->i_rdev));
	return 0;
}

static int mt_otp_access(unsigned int access_type, unsigned int offset, void *buff_ptr,
			 unsigned int length, unsigned int *status)
{
	unsigned int i = 0, ret = 0;
	char *BufAddr = (char *)buff_ptr;
	unsigned int PageAddr, AccessLength = 0;
	int Status = 0;
	static char *p_D_Buff;
	char S_Buff[64];

	p_D_Buff = kmalloc(g_page_size, GFP_KERNEL);

	if (!p_D_Buff) {
		ret = -ENOMEM;
		*status = OTP_ERROR_NOMEM;
		goto exit;
	}

	MSG(OTP, "[%s]: %s (0x%x) length: (%d bytes) !\n", __func__, access_type ? "WRITE" : "READ",
		offset, length);

	while (1) {
		PageAddr = offset / g_page_size;
		if (FS_OTP_READ == access_type) {
			memset(p_D_Buff, 0xff, g_page_size);
			memset(S_Buff, 0xff, (sizeof(char) * 64));

			MSG(OTP, "[%s]: Read Access of page (%d)\n", __func__, PageAddr);

			Status = g_mtk_otp_fuc.OTPRead(PageAddr, p_D_Buff, &S_Buff);
			*status = Status;

			if (OTP_SUCCESS != Status) {
				MSG(OTP, "[%s]: Read status (%d)\n", __func__, Status);
				break;
			}

			AccessLength = g_page_size - (offset % g_page_size);

			if (length >= AccessLength)
				memcpy(BufAddr, (p_D_Buff + (offset % g_page_size)), AccessLength);
			else
				memcpy(BufAddr, (p_D_Buff + (offset % g_page_size)), length);

		} else if (FS_OTP_WRITE == access_type) {
			AccessLength = g_page_size - (offset % g_page_size);
			memset(p_D_Buff, 0xff, g_page_size);
			memset(S_Buff, 0xff, (sizeof(char) * 64));

			if (length >= AccessLength)
				memcpy((p_D_Buff + (offset % g_page_size)), BufAddr, AccessLength);
			else
				memcpy((p_D_Buff + (offset % g_page_size)), BufAddr, length);

			Status = g_mtk_otp_fuc.OTPWrite(PageAddr, p_D_Buff, &S_Buff);
			*status = Status;

			if (OTP_SUCCESS != Status) {
				MSG(OTP, "[%s]: Write status (%d)\n", __func__, Status);
				break;
			}
		} else {
			MSG(OTP, "[%s]: Error, not either read nor write operations !\n", __func__);
			break;
		}

		offset += AccessLength;
		BufAddr += AccessLength;
		if (length <= AccessLength) {
			length = 0;
			break;
		}
		length -= AccessLength;
		MSG(OTP, "[%s]: Remaining %s (%d) !\n", __func__,
			access_type ? "WRITE" : "READ", length);
	}
error:
	kfree(p_D_Buff);
exit:
	return ret;
}

static long mt_otp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0, i = 0;
	static char *pbuf;

	void __user *uarg = (void __user *)arg;
	struct otp_ctl otpctl;

	/* Lock */
	spin_lock(&g_OTPLock);

	if (copy_from_user(&otpctl, uarg, sizeof(struct otp_ctl))) {
		ret = -EFAULT;
		goto exit;
	}

	if (false == g_bInitDone) {
		MSG(OTP, "ERROR: NAND Flash Not initialized !!\n");
		ret = -EFAULT;
		goto exit;
	}
	pbuf = kmalloc_array(otpctl.Length, sizeof(char), GFP_KERNEL);
	if (!pbuf) {
		ret = -ENOMEM;
		goto exit;
	}

	switch (cmd) {
	case OTP_GET_LENGTH:
	  MSG(OTP, "OTP IOCTL: OTP_GET_LENGTH\n");
	  g_mtk_otp_fuc.OTPQueryLength(&otpctl.QLength);
	  otpctl.status = OTP_SUCCESS;
	  MSG(OTP, "OTP IOCTL: The Length is %d\n", otpctl.QLength);
	  break;
	case OTP_READ:
		MSG(OTP, "OTP IOCTL: OTP_READ Offset(0x%x), Length(0x%x)\n", otpctl.Offset,
		otpctl.Length);
	  memset(pbuf, 0xff, sizeof(char) * otpctl.Length);

	  mt_otp_access(FS_OTP_READ, otpctl.Offset, pbuf, otpctl.Length, &otpctl.status);

		if (copy_to_user(otpctl.BufferPtr, pbuf, (sizeof(char) * otpctl.Length))) {
			MSG(OTP, "OTP IOCTL: Copy to user buffer Error !\n");
			goto error;
		}
	  break;
	case OTP_WRITE:
		MSG(OTP, "OTP IOCTL: OTP_WRITE Offset(0x%x), Length(0x%x)\n", otpctl.Offset,
			otpctl.Length);
		if (copy_from_user(pbuf, otpctl.BufferPtr, (sizeof(char) * otpctl.Length))) {
			MSG(OTP, "OTP IOCTL: Copy from user buffer Error !\n");
			goto error;
		}
	  mt_otp_access(FS_OTP_WRITE, otpctl.Offset, pbuf, otpctl.Length, &otpctl.status);
	  break;
	default:
	  ret = -EINVAL;
	}

	ret = copy_to_user(uarg, &otpctl, sizeof(struct otp_ctl));

error:
	kfree(pbuf);
exit:
	spin_unlock(&g_OTPLock);
	return ret;
}

static const struct file_operations nand_otp_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mt_otp_ioctl,
	.open = mt_otp_open,
	.release = mt_otp_release,
};

static struct miscdevice nand_otp_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "otp",
	.fops = &nand_otp_fops,
};
#endif

/******************************************************************************
Device driver structure
******************************************************************************/
static const struct of_device_id mtk_nand_of_ids[] = {
	{ .compatible = "mediatek,NFI",},
	{}
};

static struct platform_driver mtk_nand_driver = {
	.probe = mtk_nand_probe,
	.remove = mtk_nand_remove,
	.suspend = mtk_nand_suspend,
	.resume = mtk_nand_resume,
	.driver = {
			.name = "mtk-nand",
			.owner = THIS_MODULE,
			.of_match_table = mtk_nand_of_ids,
			},
};

/******************************************************************************
 * mtk_nand_init
 *
 * DESCRIPTION:
 *Init the device driver !
 *
 * PARAMETERS:
 *None
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
#define SEQ_printf(m, x...) \
do { \
	if (m) \
		seq_printf(m, x); \
	else \
		pr_err(x); \
} while (0)

int mtk_nand_proc_show(struct seq_file *m, void *v)
{
	int i;

	SEQ_printf(m, "ID:");
	for (i = 0; i < gn_devinfo.id_length; i++)
		SEQ_printf(m, " 0x%x", gn_devinfo.id[i]);

	SEQ_printf(m, "\n");
	SEQ_printf(m, "total size: %dMiB; part number: %s\n", gn_devinfo.totalsize,
		gn_devinfo.devciename);
	SEQ_printf(m, "Current working in %s mode\n", g_i4Interrupt ? "interrupt" : "polling");
	SEQ_printf(m, "NFI_ACCON = 0x%x\n", DRV_Reg32(NFI_ACCCON_REG32));
	SEQ_printf(m, "NFI_NAND_TYPE_CNFG_REG32= 0x%x\n", DRV_Reg32(NFI_NAND_TYPE_CNFG_REG32));

	return 0;
}


static int mt_nand_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_nand_proc_show, inode->i_private);
}


static const struct file_operations mtk_nand_fops = {
	.open = mt_nand_proc_open,
	.write = mtk_nand_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init mtk_nand_init(void)
{
	struct proc_dir_entry *entry;

	g_i4Interrupt = 1;

#if defined(NAND_OTP_SUPPORT)
	int err = 0;

	MSG(OTP, "OTP: register NAND OTP device ...\n");
	err = misc_register(&nand_otp_dev);
	if (unlikely(err)) {
		MSG(OTP, "OTP: failed to register NAND OTP device!\n");
		return err;
	}
	spin_lock_init(&g_OTPLock);
#endif

#if (defined(NAND_OTP_SUPPORT) && SAMSUNG_OTP_SUPPORT)
	g_mtk_otp_fuc.OTPQueryLength = samsung_OTPQueryLength;
	g_mtk_otp_fuc.OTPRead = samsung_OTPRead;
	g_mtk_otp_fuc.OTPWrite = samsung_OTPWrite;
#endif

	entry = proc_create(PROCNAME, 0664, NULL, &mtk_nand_fops);
#if 0
	if (entry == NULL) {
		MSG(INIT, "MTK Nand : unable to create /proc entry\n");
		return -ENOMEM;
	}
	entry->read_proc = mtk_nand_proc_read;
	entry->write_proc = mtk_nand_proc_write;
#endif
	pr_err("MediaTek Nand driver init, version %s\n", VERSION);
	DDR_INTERFACE = FALSE;


	return platform_driver_register(&mtk_nand_driver);
}

/******************************************************************************
 * mtk_nand_exit
 *
 * DESCRIPTION:
 *Free the device driver !
 *
 * PARAMETERS:
 *None
 *
 * RETURNS:
 *None
 *
 * NOTES:
 *None
 *
 ******************************************************************************/
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
int mtk_nand_cache_read_page(struct mtd_info *mtd, u32 u4RowAddr, u32 u4PageSize, u8 *pPageBuf,
				u8 *pFDMBuf, u32 readSize)
{
	u8 *buf;
	int bRet = ERR_RTN_SUCCESS;
	struct nand_chip *nand = mtd->priv;
	u32 u4SecNum = u4PageSize >> host->hw->nand_sec_shift;
	u32 backup_corrected, backup_failed;
	bool readReady;
	int retryCount = 0;
	u32 tempBitMap, bitMap;
#ifdef NAND_PFM
	struct timeval pfm_time_read;
#endif
	struct NFI_TLC_WL_INFO  tlc_wl_info;
	struct NFI_TLC_WL_INFO  pre_tlc_wl_info;
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	bool tlc_left_plane = TRUE;
	unsigned int phys = 0;
#endif
	u32 reg_val = 0;
	u32 real_row_addr = 0;
	u32 logical_plane_num = 1;
	u32 data_sector_num = 0;
	u8	*temp_byte_ptr = NULL;
	u8	*spare_ptr = NULL;
	u32 readCount;
	u32 rSize = readSize;
	u32 remainSize;
	u8 *dataBuf = pPageBuf;
	u32 read_count;

	PFM_BEGIN(pfm_time_read);
	tempBitMap = 0;

	buf = local_buffer_16_align;
	backup_corrected = mtd->ecc_stats.corrected;
	backup_failed = mtd->ecc_stats.failed;
	bitMap = 0;
	data_sector_num = u4SecNum;
	mtk_nand_interface_switch(mtd);
	logical_plane_num = 1;
	readCount = 0;
	temp_byte_ptr = buf;
	spare_ptr = pFDMBuf;
	tlc_wl_info.wl_pre = WL_LOW_PAGE; /* init for build warning*/
	tlc_wl_info.word_line_idx = u4RowAddr;
	if (likely(gn_devinfo.tlcControl.normaltlc)) {
		NFI_TLC_GetMappedWL(u4RowAddr, &tlc_wl_info);
		real_row_addr = NFI_TLC_GetRowAddr(tlc_wl_info.word_line_idx);
		if (unlikely(gn_devinfo.tlcControl.pPlaneEn)) {
			tlc_left_plane = TRUE;
			logical_plane_num = 2;
			data_sector_num /= 2;
			real_row_addr = NFI_TLC_SetpPlaneAddr(real_row_addr, tlc_left_plane);
		}
				/* MSG(INIT, "mtk_nand_exec_read_page, u4RowAddr: 0x%x real_row_addr 0x%x %d\n",
					u4RowAddr, real_row_addr, gn_devinfo.tlcControl.slcopmodeEn); */
	} else
		real_row_addr = NFI_TLC_GetRowAddr(u4RowAddr);
	pre_tlc_wl_info.wl_pre = tlc_wl_info.wl_pre;
	pre_tlc_wl_info.word_line_idx = tlc_wl_info.word_line_idx;
	read_count = 0;

	do {
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		if (gn_devinfo.tlcControl.slcopmodeEn) {
			if (0xFF != gn_devinfo.tlcControl.en_slc_mode_cmd) {
				reg_val = DRV_Reg16(NFI_CNFG_REG16);
				reg_val &= ~CNFG_READ_EN;
				reg_val &= ~CNFG_OP_MODE_MASK;
				reg_val |= CNFG_OP_CUST;
				DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

				mtk_nand_set_command(gn_devinfo.tlcControl.en_slc_mode_cmd);

				reg_val = DRV_Reg32(NFI_CON_REG16);
				reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
				/* issue reset operation */
				DRV_WriteReg32(NFI_CON_REG16, reg_val);
			}
		} else {
			if (gn_devinfo.tlcControl.normaltlc) {
				reg_val = DRV_Reg16(NFI_CNFG_REG16);
				reg_val &= ~CNFG_READ_EN;
				reg_val &= ~CNFG_OP_MODE_MASK;
				reg_val |= CNFG_OP_CUST;
				DRV_WriteReg16(NFI_CNFG_REG16, reg_val);
				if (tlc_wl_info.wl_pre == WL_LOW_PAGE)
					mtk_nand_set_command(LOW_PG_SELECT_CMD);
				else if (tlc_wl_info.wl_pre == WL_MID_PAGE)
					mtk_nand_set_command(MID_PG_SELECT_CMD);
				else if (tlc_wl_info.wl_pre == WL_HIGH_PAGE)
					mtk_nand_set_command(HIGH_PG_SELECT_CMD);

				reg_val = DRV_Reg32(NFI_CON_REG16);
				reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
				/* issue reset operation */
				DRV_WriteReg32(NFI_CON_REG16, reg_val);
			}
		}
		reg_val = 0;
#endif
	if (gn_devinfo.tlcControl.slcopmodeEn)
		mtk_nand_turn_on_randomizer(mtd, nand, pre_tlc_wl_info.word_line_idx);
	else
		mtk_nand_turn_on_randomizer(mtd, nand,
		(pre_tlc_wl_info.word_line_idx*3+pre_tlc_wl_info.wl_pre));
	if (unlikely(read_count == 0)) {
		readReady = mtk_nand_ready_for_read(nand, real_row_addr, 0, data_sector_num, true, buf, NORMAL_READ);
		read_count++;
#if 1
		pre_tlc_wl_info.wl_pre = tlc_wl_info.wl_pre;
		pre_tlc_wl_info.word_line_idx = tlc_wl_info.word_line_idx;
		if (gn_devinfo.tlcControl.slcopmodeEn) {
			tlc_wl_info.word_line_idx++;
			real_row_addr++;
		} else {
			if (tlc_wl_info.wl_pre == WL_HIGH_PAGE) {
				tlc_wl_info.wl_pre = WL_LOW_PAGE;
				tlc_wl_info.word_line_idx++;
				real_row_addr++;
			} else
				tlc_wl_info.wl_pre++;
		}
		mtk_nand_reset();
		continue;
#endif

	} else if (unlikely(rSize <= u4PageSize)) {
		readReady = mtk_nand_ready_for_read(nand, real_row_addr, 0, data_sector_num, true, buf, AD_CACHE_FINAL);
	} else {
		readReady = mtk_nand_ready_for_read(nand, real_row_addr, 0, data_sector_num, true, buf, AD_CACHE_READ);
	}
	if (likely(readReady)) {
		while (logical_plane_num) {
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
				if (gn_devinfo.tlcControl.needchangecolumn) {
					if (gn_devinfo.tlcControl.pPlaneEn)
						real_row_addr = NFI_TLC_SetpPlaneAddr(real_row_addr, tlc_left_plane);

					reg_val = DRV_Reg32(NFI_CON_REG16);
					reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
					/* issue reset operation */
					DRV_WriteReg32(NFI_CON_REG16, reg_val);
					reg_val = DRV_Reg16(NFI_CNFG_REG16);
					reg_val &= ~CNFG_READ_EN;
					reg_val &= ~CNFG_OP_MODE_MASK;
					reg_val |= CNFG_OP_CUST;
					DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

					mtk_nand_set_command(CHANGE_COLUNM_ADDR_1ST_CMD);
					mtk_nand_set_address(0, real_row_addr, 2, gn_devinfo.addr_cycle - 2);
					mtk_nand_set_command(CHANGE_COLUNM_ADDR_2ND_CMD);

					reg_val = DRV_Reg16(NFI_CNFG_REG16);
					reg_val |= CNFG_READ_EN;
					reg_val &= ~CNFG_OP_MODE_MASK;
					reg_val |= CNFG_OP_READ;
					DRV_WriteReg16(NFI_CNFG_REG16, reg_val);
				}
			}

			mtk_dir = DMA_FROM_DEVICE;
			sg_init_one(&mtk_sg, temp_byte_ptr, (data_sector_num * (1 << host->hw->nand_sec_shift)));
			dma_map_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
			phys = mtk_sg.dma_address;
#if __INTERNAL_USE_AHB_MODE__
			if (!phys)
				pr_warn("[%s]convert virt addr (%lx) to phys add (%x)fail!!!",
					__func__, (unsigned long) temp_byte_ptr, phys);
			else
				DRV_WriteReg32(NFI_STRADDR_REG32, phys);
#endif

			DRV_WriteReg32(NFI_CON_REG16, data_sector_num << CON_NFI_SEC_SHIFT);

			if (g_bHwEcc)
				ECC_Decode_Start();
#endif
			if (!mtk_nand_read_page_data(mtd, temp_byte_ptr,
				data_sector_num * (1 << host->hw->nand_sec_shift))) {
				MSG(INIT, "mtk_nand_read_page_data fail\n");
				bRet = ERR_RTN_FAIL;
			}
			dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
			if (!mtk_nand_status_ready(STA_NAND_BUSY)) {
				MSG(INIT, "mtk_nand_status_ready fail\n");
				bRet = ERR_RTN_FAIL;
			}
			if (g_bHwEcc) {
				if (!mtk_nand_check_dececc_done(data_sector_num)) {
					MSG(INIT, "mtk_nand_check_dececc_done fail\n");
					bRet = ERR_RTN_FAIL;
				}
			}
			/* mtk_nand_read_fdm_data(spare_ptr, data_sector_num); no need*/
			if (g_bHwEcc) {
				if (!mtk_nand_check_bch_error
					(mtd, temp_byte_ptr, spare_ptr, data_sector_num - 1, u4RowAddr, &tempBitMap)) {

					MSG(INIT, "mtk_nand_check_bch_error fail, retryCount: %d\n",
						retryCount);
				bRet = ERR_RTN_BCH_FAIL;
			}
		}
		mtk_nand_stop_read();

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
				if (gn_devinfo.tlcControl.needchangecolumn)
					DRV_WriteReg16(NFI_TLC_RD_WHR2_REG16, 0x055);

				if (2 == logical_plane_num) {
					tlc_left_plane = FALSE;
					spare_ptr += (host->hw->nand_fdm_size * data_sector_num);
					temp_byte_ptr += (data_sector_num * (1 << host->hw->nand_sec_shift));
				}
			}
#endif
			logical_plane_num--;

			if (bRet == ERR_RTN_BCH_FAIL)
				break;
		}
	}
#ifndef CONFIG_MTK_TLC_NAND_SUPPORT
	else
		dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
#endif

		mtk_nand_turn_off_randomizer();

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
		if ((gn_devinfo.tlcControl.slcopmodeEn)
			&& (0xFF != gn_devinfo.tlcControl.dis_slc_mode_cmd)) {
			reg_val = DRV_Reg32(NFI_CON_REG16);
			reg_val |= CON_FIFO_FLUSH|CON_NFI_RST;
			/* issue reset operation */
			DRV_WriteReg32(NFI_CON_REG16, reg_val);

			reg_val = DRV_Reg16(NFI_CNFG_REG16);
			reg_val &= ~CNFG_READ_EN;
			reg_val &= ~CNFG_OP_MODE_MASK;
			reg_val |= CNFG_OP_CUST;
			DRV_WriteReg16(NFI_CNFG_REG16, reg_val);

			mtk_nand_set_command(gn_devinfo.tlcControl.dis_slc_mode_cmd);
		}
#endif
		if (bRet == ERR_RTN_BCH_FAIL)
			break;

		remainSize = min(rSize, u4PageSize);
		memcpy(dataBuf, buf, remainSize);
		readCount++;
		dataBuf += remainSize;
		rSize -= remainSize;
		/* reset row_addr */
		pre_tlc_wl_info.wl_pre = tlc_wl_info.wl_pre;
		pre_tlc_wl_info.word_line_idx = tlc_wl_info.word_line_idx;
		logical_plane_num = 1;
		if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
			if (gn_devinfo.tlcControl.normaltlc) {
				if (gn_devinfo.tlcControl.pPlaneEn) {
					tlc_left_plane = TRUE;
					logical_plane_num = 2;
					data_sector_num /= 2;
					real_row_addr = NFI_TLC_SetpPlaneAddr(real_row_addr, tlc_left_plane);
				}
			}
		}
		if (gn_devinfo.tlcControl.pPlaneEn)
			real_row_addr = NFI_TLC_SetpPlaneAddr(real_row_addr, TRUE);

		if (gn_devinfo.tlcControl.slcopmodeEn) {
			tlc_wl_info.word_line_idx++;
			real_row_addr++;
		} else {
			if (tlc_wl_info.wl_pre == WL_HIGH_PAGE) {
				tlc_wl_info.wl_pre = WL_LOW_PAGE;
				tlc_wl_info.word_line_idx++;
				real_row_addr++;
			} else
				tlc_wl_info.wl_pre++;
		}
	} while (rSize);

	PFM_END_R(pfm_time_read, u4PageSize + 32);

	return bRet;
}

int mtk_nand_read(struct mtd_info *mtd, struct nand_chip *chip, u8 *buf, int page, u32 size)
{
	int page_per_block = gn_devinfo.blocksize * 1024 / gn_devinfo.pagesize;
	u32 block;
	u32 page_in_block;
	u32 mapped_block;
	int bRet = ERR_RTN_SUCCESS;
#ifdef DUMP_PEF
	struct timeval stimer, etimer;

	do_gettimeofday(&stimer);
#endif
	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);

	bRet =
		mtk_nand_cache_read_page(mtd, page_in_block + mapped_block * page_per_block,
					mtd->writesize, buf, chip->oob_poi, size);
	if (bRet == ERR_RTN_SUCCESS) {
#ifdef DUMP_PEF
		do_gettimeofday(&etimer);
		g_NandPerfLog.ReadPageTotalTime += Cal_timediff(&etimer, &stimer);
		g_NandPerfLog.ReadPageCount++;
		dump_nand_rwcount();
#endif
#ifdef CFG_SNAND_ACCESS_PATTERN_LOGGER
		if (g_snand_pm_on == 1)
			mtk_snand_pm_add_drv_record(_SNAND_PM_OP_READ_PAGE,
							page_in_block + mapped_block * page_per_block,
							0, Cal_timediff(&etimer, &stimer));
#endif
		return 0;
	}

	return -EIO;
}
#endif

static void __exit mtk_nand_exit(void)
{
	MSG(INIT, "MediaTek Nand driver exit, version %s\n", VERSION);
#if defined(NAND_OTP_SUPPORT)
	misc_deregister(&nand_otp_dev);
#endif

#ifdef SAMSUNG_OTP_SUPPORT
	g_mtk_otp_fuc.OTPQueryLength = NULL;
	g_mtk_otp_fuc.OTPRead = NULL;
	g_mtk_otp_fuc.OTPWrite = NULL;
#endif

	platform_driver_unregister(&mtk_nand_driver);
	remove_proc_entry(PROCNAME, NULL);
}
late_initcall(mtk_nand_init);
module_exit(mtk_nand_exit);
MODULE_LICENSE("GPL");
