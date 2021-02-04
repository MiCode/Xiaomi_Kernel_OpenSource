/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
/* #include <asm/io.h> */
/* #include <asm/tcm.h> */
#include <linux/proc_fs.h>	/* proc file use */
/*  */
#include <linux/slab.h>
#include <linux/spinlock.h>
/* #include <linux/io.h> */
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/seq_file.h>

/*#include <linux/xlog.h>		 For xlog_printk(). */
/*  */
/*#include <mach/hardware.h>*/
/* #include <mach/mt6593_pll.h> */
#include "inc/camera_wpe.h"
/*#include <mach/irqs.h>*/
/* #include <mach/mt_reg_base.h> */
/* #if defined(CONFIG_MTK_LEGACY) */
/* #include <mach/mt_clkmgr.h> */
/* For clock mgr APIS. enable_clock()/disable_clock(). */
/* #endif */
#include <mt-plat/sync_write.h>	/* For mt65xx_reg_sync_writel(). */
/* #include <mach/mt_spm_idle.h>For spm_enable_sodi()/spm_disable_sodi(). */

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <m4u.h>
#include <cmdq_core.h>
#include <cmdq_record.h>
#include <smi_public.h>
#include <mt-plat/mtk_chip.h>


/*#define __WPE_EP_NO_CLKMGR__*/
/* Measure the kernel performance
 * #define __WPE_KERNEL_PERFORMANCE_MEASURE__
 */
#ifdef __WPE_KERNEL_PERFORMANCE_MEASURE__
#include <linux/met_drv.h>
#include <linux/mtk_ftrace.h>
#endif
#if 0
/* Another Performance Measure Usage */
#include <linux/kallsyms.h>
#include <linux/ftrace_event.h>
static unsigned long __read_mostly tracing_mark_write_addr;
#define _kernel_trace_begin(name) {\
	tracing_mark_write_addr = kallsyms_lookup_name("tracing_mark_write");\
	event_trace_printk(tracing_mark_write_addr,  "B|%d|%s\n", \
			current->tgid, name);\
}
#define _kernel_trace_end() {\
	event_trace_printk(tracing_mark_write_addr,  "E\n");\
}
/* How to Use */
/* char strName[128]; */
/* sprintf(strName, "TAG_K_WAKEUP (%d)",sof_count[_PASS1]); */
/* _kernel_trace_begin(strName); */
/* _kernel_trace_end(); */
#endif


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/*  #include "smi_common.h" */

#include <linux/wakelock.h>

/* DPE Command Queue */
/* #include "../../cmdq/mt6797/cmdq_record.h" */
/* #include "../../cmdq/mt6797/cmdq_core.h" */

/* CCF */
#include <linux/clk.h>
struct WPE_CLK_STRUCT {
	struct clk *CG_IMGSYS_WPE_A;
	struct clk *CG_IMGSYS_WPE_B;
};
struct WPE_CLK_STRUCT wpe_clk;

unsigned int ver;
/*  */
#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif

#define WPE_DEV_NAME                "camera-wpe"
#define EP_NO_CLKMGR
#define BYPASS_REG         (0)
/* #define WPE_WAITIRQ_LOG  */
#define WPE_USE_GCE
/*#define WPE_USE_GCE_IRQ */

/*#define WPE_DEBUG_USE */
/* #define WPE_MULTIPROCESS_TIMEING_ISSUE  */
/*I can' test the situation in FPGA, because the velocity of FPGA is so slow. */
#define MyTag "[WPE]"
#define IRQTag "KEEPER"

#define LOG_VRB(format,	args...) \
pr_debug(MyTag "[%s] " format, __func__, ##args)

#define WPE_DEBUG
#ifdef WPE_DEBUG
#define LOG_DBG(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#else
#define LOG_DBG(format, args...)
#endif

#define LOG_INF(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#define LOG_NOTICE(format, args...) \
pr_notice(MyTag "[%s] " format, __func__, ##args)
#define LOG_WRN(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#define LOG_AST(format, args...) \
pr_debug(MyTag "[%s] " format, __func__, ##args)

/***********************************************************************
 *
 ***********************************************************************/
/* #define WPE_WR32(addr, data)  iowrite32(data, addr) For other projects.*/

#define WPE_WR32(addr, data)    mt_reg_sync_writel(data, addr)
				/* For 89 Only.   // NEED_TUNING_BY_PROJECT */
#define WPE_RD32(addr)          ioread32(addr)
/***********************************************************************
 *
 ***********************************************************************/
/* dynamic log level */
#define WPE_DBG_DBGLOG              (0x00000001)
#define WPE_DBG_INFLOG              (0x00000002)
#define WPE_DBG_INT                 (0x00000004)
#define WPE_DBG_READ_REG            (0x00000008)
#define WPE_DBG_WRITE_REG           (0x00000010)
#define WPE_DBG_TASKLET             (0x00000020)


/* /////////////////////////////////////////////////////////////////// */

/***********************************************************************
 *
 ***********************************************************************/

/*
 *    CAM interrupt status
 */
/* normal siganl : happens to be the same bit as register bit*/
/* #define WPE_INT_ST           (1<<0) */


/*
 *    IRQ signal mask
 */

#define INT_ST_MASK_WPE     ( \
			WPE_INT_ST)

#define CMDQ_REG_MASK 0xffffffff
#define WPE_START      0x2

#define WPE_ENABLE     0x1

#define WPE_IS_BUSY    0x2


/* static irqreturn_t WPE_Irq_CAM_A(signed int  Irq,void *DeviceId); */
static irqreturn_t ISP_Irq_WPE(signed int Irq, void *DeviceId);
static bool ConfigWPE(void);
static signed int ConfigWPEHW(struct WPE_Config *pWpeConfig);
static void WPE_ScheduleWork(struct work_struct *data);



typedef irqreturn_t(*IRQ_CB) (signed int, void *);

struct ISR_TABLE {
	IRQ_CB isr_fp;
	unsigned int int_number;
	char device_name[16];
};

#ifndef CONFIG_OF
const struct ISR_TABLE WPE_IRQ_CB_TBL[WPE_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_WPE, WPE_A_IRQ_BIT_ID, "wpe_a"},
	{ISP_Irq_WPE, WPE_B_IRQ_BIT_ID, "wpe_b"},
};

#else
/* int number is got from kernel api */
const struct ISR_TABLE WPE_IRQ_CB_TBL[WPE_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_WPE, 0, "wpe_a"},
	{ISP_Irq_WPE, 0, "wpe_b"},
};

#endif
/* //////////////////////////////////////////////////////////////////// */
/*  */
typedef void (*tasklet_cb) (unsigned long);
struct Tasklet_table {
	tasklet_cb tkt_cb;
	struct tasklet_struct *pWPE_tkt;
};

struct tasklet_struct Wpetkt[WPE_IRQ_TYPE_AMOUNT];

static void ISP_TaskletFunc_WPE(unsigned long data);

static struct Tasklet_table WPE_tasklet[WPE_IRQ_TYPE_AMOUNT] = {
	{ISP_TaskletFunc_WPE, &Wpetkt[WPE_IRQ_TYPE_INT_WPE_ST]},
	{ISP_TaskletFunc_WPE, &Wpetkt[WPE_IRQ_TYPE_INT_WPEB_ST]},
};

struct wake_lock WPE_wake_lock;


static DEFINE_MUTEX(gWpeMutex);
static DEFINE_MUTEX(gWpeDequeMutex);

#ifdef CONFIG_OF

struct WPE_device {
	void __iomem *regs;
	struct device *dev;
	int irq;
};

static struct WPE_device *WPE_devs;
static int nr_WPE_devs;

/* Get HW modules' base address from device nodes */
#define WPE_DEV_NODE_IDX 0
#define WPE_B_DEV_NODE_IDX 1

/* static unsigned long gISPSYS_Reg[WPE_IRQ_TYPE_AMOUNT]; */


#define ISP_WPE_BASE                  (WPE_devs[WPE_DEV_NODE_IDX].regs)
#define ISP_WPE_B_BASE                (WPE_devs[WPE_B_DEV_NODE_IDX].regs)

/* #define ISP_WPE_BASE                  (gISPSYS_Reg[WPE_DEV_NODE_IDX]) */



#else
#define ISP_WPE_BASE                        (IMGSYS_BASE + 0x2A000)

#endif

static unsigned int g_u4EnableClockCount;
static unsigned int g_u4WpeCnt;


/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32


enum WPE_FRAME_STATUS_ENUM {
	WPE_FRAME_STATUS_EMPTY,	/* 0 */
	WPE_FRAME_STATUS_ENQUE,	/* 1 */
	WPE_FRAME_STATUS_RUNNING,	/* 2 */
	WPE_FRAME_STATUS_FINISHED,	/* 3 */
	WPE_FRAME_STATUS_TOTAL
};


enum WPE_REQUEST_STATE_ENUM {
	WPE_REQUEST_STATE_EMPTY,	/* 0 */
	WPE_REQUEST_STATE_PENDING,	/* 1 */
	WPE_REQUEST_STATE_RUNNING,	/* 2 */
	WPE_REQUEST_STATE_FINISHED,	/* 3 */
	WPE_REQUEST_STATE_TOTAL
};


struct WPE_REQUEST_STRUCT {
	enum WPE_REQUEST_STATE_ENUM State;
	pid_t processID;	/* caller process ID */
	unsigned int callerID;	/* caller thread ID */
	unsigned int enqueReqNum;
		/* to judge it belongs to which frame package */
	signed int FrameWRIdx;	/* Frame write Index */
	signed int FrameRDIdx;	/* Frame read Index */
	enum WPE_FRAME_STATUS_ENUM
			WpeFrameStatus[_SUPPORT_MAX_WPE_FRAME_REQUEST_];
	struct WPE_Config WpeFrameConfig[_SUPPORT_MAX_WPE_FRAME_REQUEST_];
};

struct WPE_REQUEST_RING_STRUCT {
	signed int WriteIdx;	/* enque how many request  */
	signed int ReadIdx;		/* read which request index */
	signed int HWProcessIdx;	/* HWWriteIdx */
	struct WPE_REQUEST_STRUCT
		WPEReq_Struct[_SUPPORT_MAX_WPE_REQUEST_RING_SIZE_];
};

struct WPE_CONFIG_STRUCT {
	struct WPE_Config WpeFrameConfig[_SUPPORT_MAX_WPE_FRAME_REQUEST_];
};

static struct WPE_REQUEST_RING_STRUCT g_WPE_ReqRing;
static struct WPE_CONFIG_STRUCT g_WpeEnqueReq_Struct;
static struct WPE_CONFIG_STRUCT g_WpeDequeReq_Struct;


/***********************************************************************
 *
 ***********************************************************************/
struct WPE_USER_INFO_STRUCT {
	pid_t Pid;
	pid_t Tid;
};
enum WPE_PROCESS_ID_ENUM {
	WPE_PROCESS_ID_NONE,
	WPE_PROCESS_ID_WPE,
	WPE_PROCESS_ID_AMOUNT
};


/***********************************************************************
 *
 ***********************************************************************/
struct WPE_IRQ_INFO_STRUCT {
	unsigned int Status[WPE_IRQ_TYPE_AMOUNT];
	signed int WpeIrqCnt;
	pid_t ProcessID[WPE_PROCESS_ID_AMOUNT];
	unsigned int Mask[WPE_IRQ_TYPE_AMOUNT];
};


struct WPE_INFO_STRUCT {
	spinlock_t SpinLockWPERef;
	spinlock_t SpinLockWPE;
	spinlock_t SpinLockIrq[WPE_IRQ_TYPE_AMOUNT];
	wait_queue_head_t WaitQueueHead;
	wait_queue_head_t WaitDeque;
	struct work_struct ScheduleWpeWork;
	unsigned int UserCount;	/* User Count */
	unsigned int DebugMask;	/* Debug Mask */
	signed int IrqNum;
	struct WPE_IRQ_INFO_STRUCT IrqInfo;
	signed int WriteReqIdx;
	signed int ReadReqIdx;
	pid_t ProcessID[_SUPPORT_MAX_WPE_FRAME_REQUEST_];
};


static struct WPE_INFO_STRUCT WPEInfo;

enum _eLOG_TYPE {
	_LOG_DBG = 0,
	/* currently, only used at ipl_buf_ctrl. to protect critical section */
	_LOG_INF = 1,
	_LOG_ERR = 2,
	_LOG_MAX = 3,
};

#define NORMAL_STR_LEN (512)
#define ERR_PAGE 2
#define DBG_PAGE 2
#define INF_PAGE 4
/* #define SV_LOG_STR_LEN NORMAL_STR_LEN */

#define LOG_PPNUM 2
static unsigned int m_CurrentPPB;
struct SV_LOG_STR {
	unsigned int _cnt[LOG_PPNUM][_LOG_MAX];
	/* char   _str[_LOG_MAX][SV_LOG_STR_LEN]; */
	char *_str[LOG_PPNUM][_LOG_MAX];
};

static void *pLog_kmalloc;
static struct SV_LOG_STR gSvLog[WPE_IRQ_TYPE_AMOUNT];

/*
 *    for irq used,keep log until IRQ_LOG_PRINTER being involked,
 *    limited:
 *    each log must shorter than 512 bytes
 *    total log length in each irq/logtype can't over 1024 bytes
 */
#if 1
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...) do {                         \
	char *ptr;                                                            \
	char *pDes;                                                           \
	int avaLen;                                                           \
	unsigned int *ptr2 = &gSvLog[irq]._cnt[ppb][logT];                    \
	unsigned int str_leng;                                                \
	unsigned int logi;                                                    \
	struct SV_LOG_STR *pSrc = &gSvLog[irq];                               \
	if (logT == _LOG_ERR) {                                               \
		str_leng = NORMAL_STR_LEN*ERR_PAGE;                           \
	} else if (logT == _LOG_DBG) {                                        \
		str_leng = NORMAL_STR_LEN*DBG_PAGE;                           \
	} else if (logT == _LOG_INF) {                                        \
		str_leng = NORMAL_STR_LEN*INF_PAGE;                           \
	} else {                                                              \
		str_leng = 0;                                                 \
	}                                                                     \
	ptr = pDes = (char *)&                                                \
		(gSvLog[irq]._str[ppb][logT][gSvLog[irq]._cnt[ppb][logT]]);   \
	avaLen = str_leng - 1 - gSvLog[irq]._cnt[ppb][logT];                  \
	if (avaLen > 1) {                                                     \
		snprintf((char *)(pDes), avaLen, fmt,                         \
			##__VA_ARGS__);                                       \
		if ('\0' != gSvLog[irq]._str[ppb][logT][str_leng - 1]) {      \
			LOG_ERR("log str over flow(%d)", irq);                \
		}                                                             \
		while (*ptr++ != '\0') {				      \
			(*ptr2)++;                                            \
		}                                                             \
	} else {                                                              \
		LOG_INF("(%d)(%d)log str avalible=0, print log\n", irq, logT);\
		ptr = pSrc->_str[ppb][logT];                                  \
		if (pSrc->_cnt[ppb][logT] != 0) {                             \
			if (logT == _LOG_DBG) {                               \
				for (logi = 0; logi < DBG_PAGE; logi++) {     \
					if (ptr[NORMAL_STR_LEN*(logi+1) - 1]  \
						!= '\0'){                     \
						ptr[NORMAL_STR_LEN *          \
						(logi+1) - 1] = '\0';         \
						LOG_DBG("%s",                 \
						&ptr[NORMAL_STR_LEN*logi]);   \
					} else{                               \
						LOG_DBG("%s",                 \
						&ptr[NORMAL_STR_LEN*logi]);   \
						break;                        \
					}                                     \
				}                                             \
			}                                                     \
			else if (logT == _LOG_INF) {                          \
				for (logi = 0; logi < INF_PAGE; logi++) {     \
					if (ptr[NORMAL_STR_LEN*(logi+1) - 1]  \
						!= '\0'){                     \
						ptr[NORMAL_STR_LEN *          \
						(logi+1) - 1] = '\0';         \
						LOG_INF("%s",                 \
						&ptr[NORMAL_STR_LEN*logi]);   \
					} else{                               \
						LOG_INF("%s",                 \
						&ptr[NORMAL_STR_LEN*logi]);   \
						break;                        \
					}                                     \
				}                                             \
			}                                                     \
			else if (logT == _LOG_ERR) {                          \
				for (logi = 0; logi < ERR_PAGE; logi++) {     \
					if (ptr[NORMAL_STR_LEN*(logi+1) - 1]  \
						!= '\0') {                    \
						ptr[NORMAL_STR_LEN *          \
						(logi+1) - 1] = '\0';         \
						LOG_INF("%s",                 \
						&ptr[NORMAL_STR_LEN*logi]);   \
					} else{                               \
						LOG_INF("%s",                 \
						&ptr[NORMAL_STR_LEN*logi]);   \
						break;                        \
					}                                     \
				}                                             \
			}                                                     \
			else {                                                \
				LOG_INF("N.S.%d", logT);                      \
			}                                                     \
			ptr[0] = '\0';                                        \
			pSrc->_cnt[ppb][logT] = 0;                            \
			avaLen = str_leng - 1;                                \
			ptr = pDes = (char *)&                                \
				(pSrc->_str[ppb][logT][pSrc->_cnt[ppb][logT]]);\
			ptr2 = &(pSrc->_cnt[ppb][logT]);                      \
			snprintf((char *)(pDes), avaLen, fmt, ##__VA_ARGS__); \
			while (*ptr++ != '\0') {                              \
				(*ptr2)++;                                    \
			}                                                     \
		}                                                             \
	}                                                                     \
} while (0)
#endif

#if 1
#define IRQ_LOG_PRINTER(irq, ppb_in, logT_in) do {\
	struct SV_LOG_STR *pSrc = &gSvLog[irq];\
	char *ptr;\
	unsigned int i;\
	signed int ppb = 0;\
	signed int logT = 0;\
	if (ppb_in > 1) {\
		ppb = 1;\
	} else{\
		ppb = ppb_in;\
	} \
	if (logT_in > _LOG_ERR) {\
		logT = _LOG_ERR;\
	} else{\
		logT = logT_in;\
	} \
	ptr = pSrc->_str[ppb][logT];\
	if (pSrc->_cnt[ppb][logT] != 0) {\
		if (logT == _LOG_DBG) {\
			for (i = 0; i < DBG_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else{\
					LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
	else if (logT == _LOG_INF) {\
		for (i = 0; i < INF_PAGE; i++) {\
			if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
				ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
				LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
			} else{\
				LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
				break;\
			} \
		} \
	} \
	else if (logT == _LOG_ERR) {\
		for (i = 0; i < ERR_PAGE; i++) {\
			if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
				ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
				LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]);\
			} else{\
				LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]);\
				break;\
			} \
		} \
	} \
	else {\
		LOG_ERR("N.S.%d", logT);\
	} \
		ptr[0] = '\0';\
		pSrc->_cnt[ppb][logT] = 0;\
	} \
} while (0)


#else
#define IRQ_LOG_PRINTER(irq, ppb, logT)
#endif

/* WPE unmapped base address macro for GCE to access */
#define WPE_WPE_START_HW				(WPE_BASE_HW)
#define WPE_CTL_MOD_EN_HW				(WPE_BASE_HW + 0x0004)
#define WPE_CTL_DMA_EN_HW				(WPE_BASE_HW + 0x0008)
#define WPE_CTL_CFG_HW					(WPE_BASE_HW + 0x0010)
#define WPE_CTL_FMT_SEL_HW				(WPE_BASE_HW + 0x0014)
#define WPE_CTL_INT_EN_HW				(WPE_BASE_HW + 0x0018)
#define WPE_CTL_INT_STATUS_HW				(WPE_BASE_HW + 0x0020)
#define WPE_CTL_INT_STATUSX_HW				(WPE_BASE_HW + 0x0024)
#define WPE_CTL_TDR_TILE_HW				(WPE_BASE_HW + 0x0028)
#define WPE_CTL_TDR_DBG_STATUS_HW			(WPE_BASE_HW + 0x002C)
#define WPE_CTL_TDR_TCM_EN_HW				(WPE_BASE_HW + 0x0030)
#define WPE_CTL_SW_CTL_HW				(WPE_BASE_HW + 0x0034)
#define WPE_CTL_SPARE0_HW				(WPE_BASE_HW + 0x0038)
#define WPE_CTL_SPARE1_HW				(WPE_BASE_HW + 0x003C)
#define WPE_CTL_SPARE2_HW				(WPE_BASE_HW + 0x0040)
#define WPE_CTL_DONE_SEL_HW				(WPE_BASE_HW + 0x0044)
#define WPE_CTL_DBG_SET_HW				(WPE_BASE_HW + 0x0048)
#define WPE_CTL_DBG_PORT_HW				(WPE_BASE_HW + 0x004C)
#define WPE_CTL_DATE_CODE_HW				(WPE_BASE_HW + 0x0050)
#define WPE_CTL_PROJ_CODE_HW				(WPE_BASE_HW + 0x0054)
#define WPE_CTL_WPE_DCM_DIS_HW				(WPE_BASE_HW + 0x0058)
#define WPE_CTL_DMA_DCM_DIS_HW				(WPE_BASE_HW + 0x005C)
#define WPE_CTL_WPE_DCM_STATUS_HW			(WPE_BASE_HW + 0x0060)
#define WPE_CTL_DMA_DCM_STATUS_HW			(WPE_BASE_HW + 0x0064)
#define WPE_CTL_WPE_REQ_STATUS_HW			(WPE_BASE_HW + 0x0068)
#define WPE_CTL_DMA_REQ_STATUS_HW			(WPE_BASE_HW + 0x006C)
#define WPE_CTL_WPE_RDY_STATUS_HW			(WPE_BASE_HW + 0x0070)
#define WPE_CTL_DMA_RDY_STATUS_HW			(WPE_BASE_HW + 0x0074)

#define WPE_VGEN_CTL_HW					(WPE_BASE_HW + 0x00C0)
#define WPE_VGEN_IN_IMG_HW				(WPE_BASE_HW + 0x00C4)
#define WPE_VGEN_OUT_IMG_HW				(WPE_BASE_HW + 0x00C8)
#define WPE_VGEN_HORI_STEP_HW				(WPE_BASE_HW + 0x00CC)
#define WPE_VGEN_VERT_STEP_HW				(WPE_BASE_HW + 0x00D0)
#define WPE_VGEN_HORI_INT_OFST_HW			(WPE_BASE_HW + 0x00D4)
#define WPE_VGEN_HORI_SUB_OFST_HW			(WPE_BASE_HW + 0x00D8)
#define WPE_VGEN_VERT_INT_OFST_HW			(WPE_BASE_HW + 0x00DC)
#define WPE_VGEN_VERT_SUB_OFST_HW			(WPE_BASE_HW + 0x00E0)

#define WPE_VGEN_POST_CTL_HW				(WPE_BASE_HW + 0x00E8)
#define WPE_VGEN_POST_COMP_X_HW				(WPE_BASE_HW + 0x00EC)
#define WPE_VGEN_POST_COMP_Y_HW				(WPE_BASE_HW + 0x00F0)
#define WPE_VGEN_MAX_VEC_HW				(WPE_BASE_HW + 0x00F4)
#define WPE_VFIFO_CTL_HW				(WPE_BASE_HW + 0x00F8)

#define WPE_CFIFO_CTL_HW				(WPE_BASE_HW + 0x0140)

#define WPE_RWCTL_CTL_HW				(WPE_BASE_HW + 0x0150)

#define WPE_CACHI_SPECIAL_FUN_EN_HW			(WPE_BASE_HW + 0x0160)

#define WPE_C24_TILE_EDGE_HW				(WPE_BASE_HW + 0x0170)

#define WPE_MDP_CROP_X_HW				(WPE_BASE_HW + 0x0190)
#define WPE_MDP_CROP_Y_HW				(WPE_BASE_HW + 0x0194)

#define WPE_ISPCROP_CON1_HW				(WPE_BASE_HW + 0x01C0)
#define WPE_ISPCROP_CON2_HW				(WPE_BASE_HW + 0x01C4)

#define WPE_PSP_CTL_HW					(WPE_BASE_HW + 0x01F0)
#define WPE_PSP2_CTL_HW					(WPE_BASE_HW + 0x01F4)

#define WPE_ADDR_GEN_SOFT_RSTSTAT_0_HW			(WPE_BASE_HW + 0x02C0)
#define WPE_ADDR_GEN_BASE_ADDR_0_HW			(WPE_BASE_HW + 0x02C4)
#define WPE_ADDR_GEN_OFFSET_ADDR_0_HW			(WPE_BASE_HW + 0x02C8)
#define WPE_ADDR_GEN_STRIDE_0_HW			(WPE_BASE_HW + 0x02CC)
#define WPE_CACHI_CON_0_HW				(WPE_BASE_HW + 0x02D0)
#define WPE_CACHI_CON2_0_HW				(WPE_BASE_HW + 0x02D4)
#define WPE_CACHI_CON3_0_HW				(WPE_BASE_HW + 0x02D8)
#define WPE_ADDR_GEN_ERR_CTRL_0_HW			(WPE_BASE_HW + 0x02DC)
#define WPE_ADDR_GEN_ERR_STAT_0_HW			(WPE_BASE_HW + 0x02E0)
#define WPE_ADDR_GEN_RSV1_0_HW				(WPE_BASE_HW + 0x02E4)
#define WPE_ADDR_GEN_DEBUG_SEL_0_HW			(WPE_BASE_HW + 0x02E8)

#define WPE_ADDR_GEN_SOFT_RSTSTAT_1_HW			(WPE_BASE_HW + 0x02F0)
#define WPE_ADDR_GEN_BASE_ADDR_1_HW			(WPE_BASE_HW + 0x02F4)
#define WPE_ADDR_GEN_OFFSET_ADDR_1_HW			(WPE_BASE_HW + 0x02F8)
#define WPE_ADDR_GEN_STRIDE_1_HW			(WPE_BASE_HW + 0x02FC)
#define WPE_CACHI_CON_1_HW				(WPE_BASE_HW + 0x0300)
#define WPE_CACHI_CON2_1_HW				(WPE_BASE_HW + 0x0304)
#define WPE_CACHI_CON3_1_HW				(WPE_BASE_HW + 0x0308)
#define WPE_ADDR_GEN_ERR_CTRL_1_HW			(WPE_BASE_HW + 0x030C)
#define WPE_ADDR_GEN_ERR_STAT_1_HW			(WPE_BASE_HW + 0x0310)
#define WPE_ADDR_GEN_RSV1_1_HW				(WPE_BASE_HW + 0x0314)
#define WPE_ADDR_GEN_DEBUG_SEL_1_HW			(WPE_BASE_HW + 0x0318)

#define WPE_ADDR_GEN_SOFT_RSTSTAT_2_HW			(WPE_BASE_HW + 0x0320)
#define WPE_ADDR_GEN_BASE_ADDR_2_HW			(WPE_BASE_HW + 0x0324)
#define WPE_ADDR_GEN_OFFSET_ADDR_2_HW			(WPE_BASE_HW + 0x0328)
#define WPE_ADDR_GEN_STRIDE_2_HW			(WPE_BASE_HW + 0x032C)
#define WPE_CACHI_CON_2_HW				(WPE_BASE_HW + 0x0330)
#define WPE_CACHI_CON2_2_HW				(WPE_BASE_HW + 0x0334)
#define WPE_CACHI_CON3_2_HW				(WPE_BASE_HW + 0x0338)
#define WPE_ADDR_GEN_ERR_CTRL_2_HW			(WPE_BASE_HW + 0x033C)
#define WPE_ADDR_GEN_ERR_STAT_2_HW			(WPE_BASE_HW + 0x0340)
#define WPE_ADDR_GEN_RSV1_2_HW				(WPE_BASE_HW + 0x0344)
#define WPE_ADDR_GEN_DEBUG_SEL_2_HW			(WPE_BASE_HW + 0x0348)

#define WPE_ADDR_GEN_SOFT_RSTSTAT_3_HW			(WPE_BASE_HW + 0x0350)
#define WPE_ADDR_GEN_BASE_ADDR_3_HW			(WPE_BASE_HW + 0x0354)
#define WPE_ADDR_GEN_OFFSET_ADDR_3_HW			(WPE_BASE_HW + 0x0358)
#define WPE_ADDR_GEN_STRIDE_3_HW			(WPE_BASE_HW + 0x035C)
#define WPE_CACHI_CON_3_HW				(WPE_BASE_HW + 0x0360)
#define WPE_CACHI_CON2_3_HW				(WPE_BASE_HW + 0x0364)
#define WPE_CACHI_CON3_3_HW				(WPE_BASE_HW + 0x0368)
#define WPE_ADDR_GEN_ERR_CTRL_3_HW			(WPE_BASE_HW + 0x036C)
#define WPE_ADDR_GEN_ERR_STAT_3_HW			(WPE_BASE_HW + 0x0370)
#define WPE_ADDR_GEN_RSV1_3_HW				(WPE_BASE_HW + 0x0374)
#define WPE_ADDR_GEN_DEBUG_SEL_3_HW			(WPE_BASE_HW + 0x0378)

#define WPE_DMA_SOFT_RSTSTAT_HW				(WPE_BASE_HW + 0x03C0)
#define WPE_TDRI_BASE_ADDR_HW				(WPE_BASE_HW + 0x03C4)
#define WPE_TDRI_OFST_ADDR_HW				(WPE_BASE_HW + 0x03C8)
#define WPE_TDRI_XSIZE_HW				(WPE_BASE_HW + 0x03CC)
#define WPE_VERTICAL_FLIP_EN_HW				(WPE_BASE_HW + 0x03D0)
#define WPE_DMA_SOFT_RESET_HW				(WPE_BASE_HW + 0x03D4)
#define WPE_LAST_ULTRA_EN_HW				(WPE_BASE_HW + 0x03D8)
#define WPE_SPECIAL_FUN_EN_HW				(WPE_BASE_HW + 0x03DC)

#define WPE_WPEO_BASE_ADDR_HW				(WPE_BASE_HW + 0x03F0)

#define WPE_WPEO_OFST_ADDR_HW				(WPE_BASE_HW + 0x03F8)

#define WPE_WPEO_XSIZE_HW				(WPE_BASE_HW + 0x0400)
#define WPE_WPEO_YSIZE_HW				(WPE_BASE_HW + 0x0404)
#define WPE_WPEO_STRIDE_HW				(WPE_BASE_HW + 0x0408)
#define WPE_WPEO_CON_HW					(WPE_BASE_HW + 0x040C)
#define WPE_WPEO_CON2_HW				(WPE_BASE_HW + 0x0410)
#define WPE_WPEO_CON3_HW				(WPE_BASE_HW + 0x0414)
#define WPE_WPEO_CROP_HW				(WPE_BASE_HW + 0x0418)

#define WPE_MSKO_BASE_ADDR_HW				(WPE_BASE_HW + 0x0420)

#define WPE_MSKO_OFST_ADDR_HW				(WPE_BASE_HW + 0x0428)

#define WPE_MSKO_XSIZE_HW				(WPE_BASE_HW + 0x0430)
#define WPE_MSKO_YSIZE_HW				(WPE_BASE_HW + 0x0434)
#define WPE_MSKO_STRIDE_HW				(WPE_BASE_HW + 0x0438)
#define WPE_MSKO_CON_HW					(WPE_BASE_HW + 0x043C)
#define WPE_MSKO_CON2_HW				(WPE_BASE_HW + 0x0440)
#define WPE_MSKO_CON3_HW				(WPE_BASE_HW + 0x0444)
#define WPE_MSKO_CROP_HW				(WPE_BASE_HW + 0x0448)

#define WPE_VECI_BASE_ADDR_HW				(WPE_BASE_HW + 0x0450)

#define WPE_VECI_OFST_ADDR_HW				(WPE_BASE_HW + 0x0458)

#define WPE_VECI_XSIZE_HW				(WPE_BASE_HW + 0x0460)
#define WPE_VECI_YSIZE_HW				(WPE_BASE_HW + 0x0464)
#define WPE_VECI_STRIDE_HW				(WPE_BASE_HW + 0x0468)
#define WPE_VECI_CON_HW					(WPE_BASE_HW + 0x046C)
#define WPE_VECI_CON2_HW				(WPE_BASE_HW + 0x0470)
#define WPE_VECI_CON3_HW				(WPE_BASE_HW + 0x0474)

#define WPE_VEC2I_BASE_ADDR_HW				(WPE_BASE_HW + 0x0480)

#define WPE_VEC2I_OFST_ADDR_HW				(WPE_BASE_HW + 0x0488)

#define WPE_VEC2I_XSIZE_HW				(WPE_BASE_HW + 0x0490)
#define WPE_VEC2I_YSIZE_HW				(WPE_BASE_HW + 0x0494)
#define WPE_VEC2I_STRIDE_HW				(WPE_BASE_HW + 0x0498)
#define WPE_VEC2I_CON_HW				(WPE_BASE_HW + 0x049C)
#define WPE_VEC2I_CON2_HW				(WPE_BASE_HW + 0x04A0)
#define WPE_VEC2I_CON3_HW				(WPE_BASE_HW + 0x04A4)

#define WPE_VEC3I_BASE_ADDR_HW				(WPE_BASE_HW + 0x04B0)

#define WPE_VEC3I_OFST_ADDR_HW				(WPE_BASE_HW + 0x04B8)

#define WPE_VEC3I_XSIZE_HW				(WPE_BASE_HW + 0x04C0)
#define WPE_VEC3I_YSIZE_HW				(WPE_BASE_HW + 0x04C4)
#define WPE_VEC3I_STRIDE_HW				(WPE_BASE_HW + 0x04C8)
#define WPE_VEC3I_CON_HW				(WPE_BASE_HW + 0x04CC)
#define WPE_VEC3I_CON2_HW				(WPE_BASE_HW + 0x04D0)
#define WPE_VEC3I_CON3_HW				(WPE_BASE_HW + 0x04D4)

#define WPE_DMA_ERR_CTRL_HW				(WPE_BASE_HW + 0x04E0)
#define WPE_WPEO_ERR_STAT_HW				(WPE_BASE_HW + 0x04E4)
#define WPE_MSKO_ERR_STAT_HW				(WPE_BASE_HW + 0x04E8)
#define WPE_VECI_ERR_STAT_HW				(WPE_BASE_HW + 0x04EC)
#define WPE_VEC2I_ERR_STAT_HW				(WPE_BASE_HW + 0x04F0)
#define WPE_VEC3I_ERR_STAT_HW				(WPE_BASE_HW + 0x04F4)
#define WPE_DMA_DEBUG_ADDR_HW				(WPE_BASE_HW + 0x04F8)
#define WPE_DMA_RSV1_HW					(WPE_BASE_HW + 0x04FC)
#define WPE_DMA_RSV2_HW					(WPE_BASE_HW + 0x0500)
#define WPE_DMA_RSV3_HW					(WPE_BASE_HW + 0x0504)
#define WPE_DMA_RSV4_HW					(WPE_BASE_HW + 0x0508)
#define WPE_DMA_DEBUG_SEL_HW				(WPE_BASE_HW + 0x050C)

/* WPE B unmapped base address macro for GCE to access */
#define WPE_B_WPE_START_HW				(WPE_B_BASE_HW)
#define WPE_B_CTL_MOD_EN_HW				(WPE_B_BASE_HW + 0x0004)
#define WPE_B_CTL_DMA_EN_HW				(WPE_B_BASE_HW + 0x0008)
#define WPE_B_CTL_CFG_HW				(WPE_B_BASE_HW + 0x0010)
#define WPE_B_CTL_FMT_SEL_HW				(WPE_B_BASE_HW + 0x0014)
#define WPE_B_CTL_INT_EN_HW				(WPE_B_BASE_HW + 0x0018)
#define WPE_B_CTL_INT_STATUS_HW				(WPE_B_BASE_HW + 0x0020)
#define WPE_B_CTL_INT_STATUSX_HW			(WPE_B_BASE_HW + 0x0024)
#define WPE_B_CTL_TDR_TILE_HW				(WPE_B_BASE_HW + 0x0028)
#define WPE_B_CTL_TDR_DBG_STATUS_HW			(WPE_B_BASE_HW + 0x002C)
#define WPE_B_CTL_TDR_TCM_EN_HW				(WPE_B_BASE_HW + 0x0030)
#define WPE_B_CTL_SW_CTL_HW				(WPE_B_BASE_HW + 0x0034)
#define WPE_B_CTL_SPARE0_HW				(WPE_B_BASE_HW + 0x0038)
#define WPE_B_CTL_SPARE1_HW				(WPE_B_BASE_HW + 0x003C)
#define WPE_B_CTL_SPARE2_HW				(WPE_B_BASE_HW + 0x0040)
#define WPE_B_CTL_DONE_SEL_HW				(WPE_B_BASE_HW + 0x0044)
#define WPE_B_CTL_DBG_SET_HW				(WPE_B_BASE_HW + 0x0048)
#define WPE_B_CTL_DBG_PORT_HW				(WPE_B_BASE_HW + 0x004C)
#define WPE_B_CTL_DATE_CODE_HW				(WPE_B_BASE_HW + 0x0050)
#define WPE_B_CTL_PROJ_CODE_HW				(WPE_B_BASE_HW + 0x0054)
#define WPE_B_CTL_WPE_DCM_DIS_HW			(WPE_B_BASE_HW + 0x0058)
#define WPE_B_CTL_DMA_DCM_DIS_HW			(WPE_B_BASE_HW + 0x005C)
#define WPE_B_CTL_WPE_DCM_STATUS_HW			(WPE_B_BASE_HW + 0x0060)
#define WPE_B_CTL_DMA_DCM_STATUS_HW			(WPE_B_BASE_HW + 0x0064)
#define WPE_B_CTL_WPE_REQ_STATUS_HW			(WPE_B_BASE_HW + 0x0068)
#define WPE_B_CTL_DMA_REQ_STATUS_HW			(WPE_B_BASE_HW + 0x006C)
#define WPE_B_CTL_WPE_RDY_STATUS_HW			(WPE_B_BASE_HW + 0x0070)
#define WPE_B_CTL_DMA_RDY_STATUS_HW			(WPE_B_BASE_HW + 0x0074)

#define WPE_B_VGEN_CTL_HW				(WPE_B_BASE_HW + 0x00C0)
#define WPE_B_VGEN_IN_IMG_HW				(WPE_B_BASE_HW + 0x00C4)
#define WPE_B_VGEN_OUT_IMG_HW				(WPE_B_BASE_HW + 0x00C8)
#define WPE_B_VGEN_HORI_STEP_HW				(WPE_B_BASE_HW + 0x00CC)
#define WPE_B_VGEN_VERT_STEP_HW				(WPE_B_BASE_HW + 0x00D0)
#define WPE_B_VGEN_HORI_INT_OFST_HW			(WPE_B_BASE_HW + 0x00D4)
#define WPE_B_VGEN_HORI_SUB_OFST_HW			(WPE_B_BASE_HW + 0x00D8)
#define WPE_B_VGEN_VERT_INT_OFST_HW			(WPE_B_BASE_HW + 0x00DC)
#define WPE_B_VGEN_VERT_SUB_OFST_HW			(WPE_B_BASE_HW + 0x00E0)

#define WPE_B_VGEN_POST_CTL_HW				(WPE_B_BASE_HW + 0x00E8)
#define WPE_B_VGEN_POST_COMP_X_HW			(WPE_B_BASE_HW + 0x00EC)
#define WPE_B_VGEN_POST_COMP_Y_HW			(WPE_B_BASE_HW + 0x00F0)
#define WPE_B_VGEN_MAX_VEC_HW				(WPE_B_BASE_HW + 0x00F4)
#define WPE_B_VFIFO_CTL_HW				(WPE_B_BASE_HW + 0x00F8)

#define WPE_B_CFIFO_CTL_HW				(WPE_B_BASE_HW + 0x0140)

#define WPE_B_RWCTL_CTL_HW				(WPE_B_BASE_HW + 0x0150)

#define WPE_B_CACHI_SPECIAL_FUN_EN_HW			(WPE_B_BASE_HW + 0x0160)

#define WPE_B_C24_TILE_EDGE_HW				(WPE_B_BASE_HW + 0x0170)

#define WPE_B_MDP_CROP_X_HW				(WPE_B_BASE_HW + 0x0190)
#define WPE_B_MDP_CROP_Y_HW				(WPE_B_BASE_HW + 0x0194)

#define WPE_B_ISPCROP_CON1_HW				(WPE_B_BASE_HW + 0x01C0)
#define WPE_B_ISPCROP_CON2_HW				(WPE_B_BASE_HW + 0x01C4)

#define WPE_B_PSP_CTL_HW				(WPE_B_BASE_HW + 0x01F0)
#define WPE_B_PSP2_CTL_HW				(WPE_B_BASE_HW + 0x01F4)

#define WPE_B_ADDR_GEN_SOFT_RSTSTAT_0_HW		(WPE_B_BASE_HW + 0x02C0)
#define WPE_B_ADDR_GEN_BASE_ADDR_0_HW			(WPE_B_BASE_HW + 0x02C4)
#define WPE_B_ADDR_GEN_OFFSET_ADDR_0_HW			(WPE_B_BASE_HW + 0x02C8)
#define WPE_B_ADDR_GEN_STRIDE_0_HW			(WPE_B_BASE_HW + 0x02CC)
#define WPE_B_CACHI_CON_0_HW				(WPE_B_BASE_HW + 0x02D0)
#define WPE_B_CACHI_CON2_0_HW				(WPE_B_BASE_HW + 0x02D4)
#define WPE_B_CACHI_CON3_0_HW				(WPE_B_BASE_HW + 0x02D8)
#define WPE_B_ADDR_GEN_ERR_CTRL_0_HW			(WPE_B_BASE_HW + 0x02DC)
#define WPE_B_ADDR_GEN_ERR_STAT_0_HW			(WPE_B_BASE_HW + 0x02E0)
#define WPE_B_ADDR_GEN_RSV1_0_HW			(WPE_B_BASE_HW + 0x02E4)
#define WPE_B_ADDR_GEN_DEBUG_SEL_0_HW			(WPE_B_BASE_HW + 0x02E8)

#define WPE_B_ADDR_GEN_SOFT_RSTSTAT_1_HW		(WPE_B_BASE_HW + 0x02F0)
#define WPE_B_ADDR_GEN_BASE_ADDR_1_HW			(WPE_B_BASE_HW + 0x02F4)
#define WPE_B_ADDR_GEN_OFFSET_ADDR_1_HW			(WPE_B_BASE_HW + 0x02F8)
#define WPE_B_ADDR_GEN_STRIDE_1_HW			(WPE_B_BASE_HW + 0x02FC)
#define WPE_B_CACHI_CON_1_HW				(WPE_B_BASE_HW + 0x0300)
#define WPE_B_CACHI_CON2_1_HW				(WPE_B_BASE_HW + 0x0304)
#define WPE_B_CACHI_CON3_1_HW				(WPE_B_BASE_HW + 0x0308)
#define WPE_B_ADDR_GEN_ERR_CTRL_1_HW			(WPE_B_BASE_HW + 0x030C)
#define WPE_B_ADDR_GEN_ERR_STAT_1_HW			(WPE_B_BASE_HW + 0x0310)
#define WPE_B_ADDR_GEN_RSV1_1_HW			(WPE_B_BASE_HW + 0x0314)
#define WPE_B_ADDR_GEN_DEBUG_SEL_1_HW			(WPE_B_BASE_HW + 0x0318)

#define WPE_B_ADDR_GEN_SOFT_RSTSTAT_2_HW		(WPE_B_BASE_HW + 0x0320)
#define WPE_B_ADDR_GEN_BASE_ADDR_2_HW			(WPE_B_BASE_HW + 0x0324)
#define WPE_B_ADDR_GEN_OFFSET_ADDR_2_HW			(WPE_B_BASE_HW + 0x0328)
#define WPE_B_ADDR_GEN_STRIDE_2_HW			(WPE_B_BASE_HW + 0x032C)
#define WPE_B_CACHI_CON_2_HW				(WPE_B_BASE_HW + 0x0330)
#define WPE_B_CACHI_CON2_2_HW				(WPE_B_BASE_HW + 0x0334)
#define WPE_B_CACHI_CON3_2_HW				(WPE_B_BASE_HW + 0x0338)
#define WPE_B_ADDR_GEN_ERR_CTRL_2_HW			(WPE_B_BASE_HW + 0x033C)
#define WPE_B_ADDR_GEN_ERR_STAT_2_HW			(WPE_B_BASE_HW + 0x0340)
#define WPE_B_ADDR_GEN_RSV1_2_HW			(WPE_B_BASE_HW + 0x0344)
#define WPE_B_ADDR_GEN_DEBUG_SEL_2_HW			(WPE_B_BASE_HW + 0x0348)

#define WPE_B_ADDR_GEN_SOFT_RSTSTAT_3_HW		(WPE_B_BASE_HW + 0x0350)
#define WPE_B_ADDR_GEN_BASE_ADDR_3_HW			(WPE_B_BASE_HW + 0x0354)
#define WPE_B_ADDR_GEN_OFFSET_ADDR_3_HW			(WPE_B_BASE_HW + 0x0358)
#define WPE_B_ADDR_GEN_STRIDE_3_HW			(WPE_B_BASE_HW + 0x035C)
#define WPE_B_CACHI_CON_3_HW				(WPE_B_BASE_HW + 0x0360)
#define WPE_B_CACHI_CON2_3_HW				(WPE_B_BASE_HW + 0x0364)
#define WPE_B_CACHI_CON3_3_HW				(WPE_B_BASE_HW + 0x0368)
#define WPE_B_ADDR_GEN_ERR_CTRL_3_HW			(WPE_B_BASE_HW + 0x036C)
#define WPE_B_ADDR_GEN_ERR_STAT_3_HW			(WPE_B_BASE_HW + 0x0370)
#define WPE_B_ADDR_GEN_RSV1_3_HW			(WPE_B_BASE_HW + 0x0374)
#define WPE_B_ADDR_GEN_DEBUG_SEL_3_HW			(WPE_B_BASE_HW + 0x0378)

#define WPE_B_DMA_SOFT_RSTSTAT_HW			(WPE_B_BASE_HW + 0x03C0)
#define WPE_B_TDRI_BASE_ADDR_HW				(WPE_B_BASE_HW + 0x03C4)
#define WPE_B_TDRI_OFST_ADDR_HW				(WPE_B_BASE_HW + 0x03C8)
#define WPE_B_TDRI_XSIZE_HW				(WPE_B_BASE_HW + 0x03CC)
#define WPE_B_VERTICAL_FLIP_EN_HW			(WPE_B_BASE_HW + 0x03D0)
#define WPE_B_DMA_SOFT_RESET_HW				(WPE_B_BASE_HW + 0x03D4)
#define WPE_B_LAST_ULTRA_EN_HW				(WPE_B_BASE_HW + 0x03D8)
#define WPE_B_SPECIAL_FUN_EN_HW				(WPE_B_BASE_HW + 0x03DC)

#define WPE_B_WPEO_BASE_ADDR_HW				(WPE_B_BASE_HW + 0x03F0)

#define WPE_B_WPEO_OFST_ADDR_HW				(WPE_B_BASE_HW + 0x03F8)

#define WPE_B_WPEO_XSIZE_HW				(WPE_B_BASE_HW + 0x0400)
#define WPE_B_WPEO_YSIZE_HW				(WPE_B_BASE_HW + 0x0404)
#define WPE_B_WPEO_STRIDE_HW				(WPE_B_BASE_HW + 0x0408)
#define WPE_B_WPEO_CON_HW				(WPE_B_BASE_HW + 0x040C)
#define WPE_B_WPEO_CON2_HW				(WPE_B_BASE_HW + 0x0410)
#define WPE_B_WPEO_CON3_HW				(WPE_B_BASE_HW + 0x0414)
#define WPE_B_WPEO_CROP_HW				(WPE_B_BASE_HW + 0x0418)

#define WPE_B_MSKO_BASE_ADDR_HW				(WPE_B_BASE_HW + 0x0420)

#define WPE_B_MSKO_OFST_ADDR_HW				(WPE_B_BASE_HW + 0x0428)

#define WPE_B_MSKO_XSIZE_HW				(WPE_B_BASE_HW + 0x0430)
#define WPE_B_MSKO_YSIZE_HW				(WPE_B_BASE_HW + 0x0434)
#define WPE_B_MSKO_STRIDE_HW				(WPE_B_BASE_HW + 0x0438)
#define WPE_B_MSKO_CON_HW				(WPE_B_BASE_HW + 0x043C)
#define WPE_B_MSKO_CON2_HW				(WPE_B_BASE_HW + 0x0440)
#define WPE_B_MSKO_CON3_HW				(WPE_B_BASE_HW + 0x0444)
#define WPE_B_MSKO_CROP_HW				(WPE_B_BASE_HW + 0x0448)

#define WPE_B_VECI_BASE_ADDR_HW				(WPE_B_BASE_HW + 0x0450)

#define WPE_B_VECI_OFST_ADDR_HW				(WPE_B_BASE_HW + 0x0458)

#define WPE_B_VECI_XSIZE_HW				(WPE_B_BASE_HW + 0x0460)
#define WPE_B_VECI_YSIZE_HW				(WPE_B_BASE_HW + 0x0464)
#define WPE_B_VECI_STRIDE_HW				(WPE_B_BASE_HW + 0x0468)
#define WPE_B_VECI_CON_HW				(WPE_B_BASE_HW + 0x046C)
#define WPE_B_VECI_CON2_HW				(WPE_B_BASE_HW + 0x0470)
#define WPE_B_VECI_CON3_HW				(WPE_B_BASE_HW + 0x0474)

#define WPE_B_VEC2I_BASE_ADDR_HW			(WPE_B_BASE_HW + 0x0480)

#define WPE_B_VEC2I_OFST_ADDR_HW			(WPE_B_BASE_HW + 0x0488)

#define WPE_B_VEC2I_XSIZE_HW				(WPE_B_BASE_HW + 0x0490)
#define WPE_B_VEC2I_YSIZE_HW				(WPE_B_BASE_HW + 0x0494)
#define WPE_B_VEC2I_STRIDE_HW				(WPE_B_BASE_HW + 0x0498)
#define WPE_B_VEC2I_CON_HW				(WPE_B_BASE_HW + 0x049C)
#define WPE_B_VEC2I_CON2_HW				(WPE_B_BASE_HW + 0x04A0)
#define WPE_B_VEC2I_CON3_HW				(WPE_B_BASE_HW + 0x04A4)

#define WPE_B_VEC3I_BASE_ADDR_HW			(WPE_B_BASE_HW + 0x04B0)

#define WPE_B_VEC3I_OFST_ADDR_HW			(WPE_B_BASE_HW + 0x04B8)

#define WPE_B_VEC3I_XSIZE_HW				(WPE_B_BASE_HW + 0x04C0)
#define WPE_B_VEC3I_YSIZE_HW				(WPE_B_BASE_HW + 0x04C4)
#define WPE_B_VEC3I_STRIDE_HW				(WPE_B_BASE_HW + 0x04C8)
#define WPE_B_VEC3I_CON_HW				(WPE_B_BASE_HW + 0x04CC)
#define WPE_B_VEC3I_CON2_HW				(WPE_B_BASE_HW + 0x04D0)
#define WPE_B_VEC3I_CON3_HW				(WPE_B_BASE_HW + 0x04D4)

#define WPE_B_DMA_ERR_CTRL_HW				(WPE_B_BASE_HW + 0x04E0)
#define WPE_B_WPEO_ERR_STAT_HW				(WPE_B_BASE_HW + 0x04E4)
#define WPE_B_MSKO_ERR_STAT_HW				(WPE_B_BASE_HW + 0x04E8)
#define WPE_B_VECI_ERR_STAT_HW				(WPE_B_BASE_HW + 0x04EC)
#define WPE_B_VEC2I_ERR_STAT_HW				(WPE_B_BASE_HW + 0x04F0)
#define WPE_B_VEC3I_ERR_STAT_HW				(WPE_B_BASE_HW + 0x04F4)
#define WPE_B_DMA_DEBUG_ADDR_HW				(WPE_B_BASE_HW + 0x04F8)
#define WPE_B_DMA_RSV1_HW				(WPE_B_BASE_HW + 0x04FC)
#define WPE_B_DMA_RSV2_HW				(WPE_B_BASE_HW + 0x0500)
#define WPE_B_DMA_RSV3_HW				(WPE_B_BASE_HW + 0x0504)
#define WPE_B_DMA_RSV4_HW				(WPE_B_BASE_HW + 0x0508)
#define WPE_B_DMA_DEBUG_SEL_HW				(WPE_B_BASE_HW + 0x050C)


/*SW Access Registers : using mapped base address from DTS*/
#define WPE_WPE_START_REG				(ISP_WPE_BASE)
#define WPE_CTL_MOD_EN_REG				(ISP_WPE_BASE + 0x0004)
#define WPE_CTL_DMA_EN_REG				(ISP_WPE_BASE + 0x0008)
#define WPE_CTL_CFG_REG					(ISP_WPE_BASE + 0x0010)
#define WPE_CTL_FMT_SEL_REG				(ISP_WPE_BASE + 0x0014)
#define WPE_CTL_INT_EN_REG				(ISP_WPE_BASE + 0x0018)
#define WPE_CTL_INT_STATUS_REG				(ISP_WPE_BASE + 0x0020)
#define WPE_CTL_INT_STATUSX_REG				(ISP_WPE_BASE + 0x0024)
#define WPE_CTL_TDR_TILE_REG				(ISP_WPE_BASE + 0x0028)
#define WPE_CTL_TDR_DBG_STATUS_REG			(ISP_WPE_BASE + 0x002C)
#define WPE_CTL_TDR_TCM_EN_REG				(ISP_WPE_BASE + 0x0030)
#define WPE_CTL_SW_CTL_REG				(ISP_WPE_BASE + 0x0034)
#define WPE_CTL_SPARE0_REG				(ISP_WPE_BASE + 0x0038)
#define WPE_CTL_SPARE1_REG				(ISP_WPE_BASE + 0x003C)
#define WPE_CTL_SPARE2_REG				(ISP_WPE_BASE + 0x0040)
#define WPE_CTL_DONE_SEL_REG				(ISP_WPE_BASE + 0x0044)
#define WPE_CTL_DBG_SET_REG				(ISP_WPE_BASE + 0x0048)
#define WPE_CTL_DBG_PORT_REG				(ISP_WPE_BASE + 0x004C)
#define WPE_CTL_DATE_CODE_REG				(ISP_WPE_BASE + 0x0050)
#define WPE_CTL_PROJ_CODE_REG				(ISP_WPE_BASE + 0x0054)
#define WPE_CTL_WPE_DCM_DIS_REG				(ISP_WPE_BASE + 0x0058)
#define WPE_CTL_DMA_DCM_DIS_REG				(ISP_WPE_BASE + 0x005C)
#define WPE_CTL_WPE_DCM_STATUS_REG			(ISP_WPE_BASE + 0x0060)
#define WPE_CTL_DMA_DCM_STATUS_REG			(ISP_WPE_BASE + 0x0064)
#define WPE_CTL_WPE_REQ_STATUS_REG			(ISP_WPE_BASE + 0x0068)
#define WPE_CTL_DMA_REQ_STATUS_REG			(ISP_WPE_BASE + 0x006C)
#define WPE_CTL_WPE_RDY_STATUS_REG			(ISP_WPE_BASE + 0x0070)
#define WPE_CTL_DMA_RDY_STATUS_REG			(ISP_WPE_BASE + 0x0074)
#define WPE_VGEN_CTL_REG				(ISP_WPE_BASE + 0x00C0)
#define WPE_VGEN_IN_IMG_REG				(ISP_WPE_BASE + 0x00C4)
#define WPE_VGEN_OUT_IMG_REG				(ISP_WPE_BASE + 0x00C8)
#define WPE_VGEN_HORI_STEP_REG				(ISP_WPE_BASE + 0x00CC)
#define WPE_VGEN_VERT_STEP_REG				(ISP_WPE_BASE + 0x00D0)
#define WPE_VGEN_HORI_INT_OFST_REG			(ISP_WPE_BASE + 0x00D4)
#define WPE_VGEN_HORI_SUB_OFST_REG			(ISP_WPE_BASE + 0x00D8)
#define WPE_VGEN_VERT_INT_OFST_REG			(ISP_WPE_BASE + 0x00DC)
#define WPE_VGEN_VERT_SUB_OFST_REG			(ISP_WPE_BASE + 0x00E0)
#define WPE_VGEN_POST_CTL_REG				(ISP_WPE_BASE + 0x00E8)
#define WPE_VGEN_POST_COMP_X_REG			(ISP_WPE_BASE + 0x00EC)
#define WPE_VGEN_POST_COMP_Y_REG			(ISP_WPE_BASE + 0x00F0)
#define WPE_VGEN_MAX_VEC_REG				(ISP_WPE_BASE + 0x00F4)
#define WPE_VFIFO_CTL_REG				(ISP_WPE_BASE + 0x00F8)
#define WPE_CFIFO_CTL_REG				(ISP_WPE_BASE + 0x0140)
#define WPE_RWCTL_CTL_REG				(ISP_WPE_BASE + 0x0150)
#define WPE_CACHI_SPECIAL_FUN_EN_REG			(ISP_WPE_BASE + 0x0160)
#define WPE_C24_TILE_EDGE_REG				(ISP_WPE_BASE + 0x0170)
#define WPE_MDP_CROP_X_REG				(ISP_WPE_BASE + 0x0190)
#define WPE_MDP_CROP_Y_REG				(ISP_WPE_BASE + 0x0194)
#define WPE_ISPCROP_CON1_REG				(ISP_WPE_BASE + 0x01C0)
#define WPE_ISPCROP_CON2_REG				(ISP_WPE_BASE + 0x01C4)
#define WPE_PSP_CTL_REG					(ISP_WPE_BASE + 0x01F0)
#define WPE_PSP2_CTL_REG				(ISP_WPE_BASE + 0x01F4)
#define WPE_ADDR_GEN_SOFT_RSTSTAT_0_REG			(ISP_WPE_BASE + 0x02C0)
#define WPE_ADDR_GEN_BASE_ADDR_0_REG			(ISP_WPE_BASE + 0x02C4)
#define WPE_ADDR_GEN_OFFSET_ADDR_0_REG			(ISP_WPE_BASE + 0x02C8)
#define WPE_ADDR_GEN_STRIDE_0_REG			(ISP_WPE_BASE + 0x02CC)
#define WPE_CACHI_CON_0_REG				(ISP_WPE_BASE + 0x02D0)
#define WPE_CACHI_CON2_0_REG				(ISP_WPE_BASE + 0x02D4)
#define WPE_CACHI_CON3_0_REG				(ISP_WPE_BASE + 0x02D8)
#define WPE_ADDR_GEN_ERR_CTRL_0_REG			(ISP_WPE_BASE + 0x02DC)
#define WPE_ADDR_GEN_ERR_STAT_0_REG			(ISP_WPE_BASE + 0x02E0)
#define WPE_ADDR_GEN_RSV1_0_REG				(ISP_WPE_BASE + 0x02E4)
#define WPE_ADDR_GEN_DEBUG_SEL_0_REG			(ISP_WPE_BASE + 0x02E8)

#define WPE_ADDR_GEN_SOFT_RSTSTAT_1_REG			(ISP_WPE_BASE + 0x02F0)
#define WPE_ADDR_GEN_BASE_ADDR_1_REG			(ISP_WPE_BASE + 0x02F4)
#define WPE_ADDR_GEN_OFFSET_ADDR_1_REG			(ISP_WPE_BASE + 0x02F8)
#define WPE_ADDR_GEN_STRIDE_1_REG			(ISP_WPE_BASE + 0x02FC)
#define WPE_CACHI_CON_1_REG				(ISP_WPE_BASE + 0x0300)
#define WPE_CACHI_CON2_1_REG				(ISP_WPE_BASE + 0x0304)
#define WPE_CACHI_CON3_1_REG				(ISP_WPE_BASE + 0x0308)
#define WPE_ADDR_GEN_ERR_CTRL_1_REG			(ISP_WPE_BASE + 0x030C)
#define WPE_ADDR_GEN_ERR_STAT_1_REG			(ISP_WPE_BASE + 0x0310)
#define WPE_ADDR_GEN_RSV1_1_REG				(ISP_WPE_BASE + 0x0314)
#define WPE_ADDR_GEN_DEBUG_SEL_1_REG			(ISP_WPE_BASE + 0x0318)
#define WPE_ADDR_GEN_SOFT_RSTSTAT_2_REG			(ISP_WPE_BASE + 0x0320)
#define WPE_ADDR_GEN_BASE_ADDR_2_REG			(ISP_WPE_BASE + 0x0324)
#define WPE_ADDR_GEN_OFFSET_ADDR_2_REG			(ISP_WPE_BASE + 0x0328)
#define WPE_ADDR_GEN_STRIDE_2_REG			(ISP_WPE_BASE + 0x032C)
#define WPE_CACHI_CON_2_REG				(ISP_WPE_BASE + 0x0330)
#define WPE_CACHI_CON2_2_REG				(ISP_WPE_BASE + 0x0334)
#define WPE_CACHI_CON3_2_REG				(ISP_WPE_BASE + 0x0338)
#define WPE_ADDR_GEN_ERR_CTRL_2_REG			(ISP_WPE_BASE + 0x033C)
#define WPE_ADDR_GEN_ERR_STAT_2_REG			(ISP_WPE_BASE + 0x0340)
#define WPE_ADDR_GEN_RSV1_2_REG				(ISP_WPE_BASE + 0x0344)
#define WPE_ADDR_GEN_DEBUG_SEL_2_REG			(ISP_WPE_BASE + 0x0348)

#define WPE_ADDR_GEN_SOFT_RSTSTAT_3_REG			(ISP_WPE_BASE + 0x0350)
#define WPE_ADDR_GEN_BASE_ADDR_3_REG			(ISP_WPE_BASE + 0x0354)
#define WPE_ADDR_GEN_OFFSET_ADDR_3_REG			(ISP_WPE_BASE + 0x0358)
#define WPE_ADDR_GEN_STRIDE_3_REG			(ISP_WPE_BASE + 0x035C)
#define WPE_CACHI_CON_3_REG				(ISP_WPE_BASE + 0x0360)
#define WPE_CACHI_CON2_3_REG				(ISP_WPE_BASE + 0x0364)
#define WPE_CACHI_CON3_3_REG				(ISP_WPE_BASE + 0x0368)
#define WPE_ADDR_GEN_ERR_CTRL_3_REG			(ISP_WPE_BASE + 0x036C)
#define WPE_ADDR_GEN_ERR_STAT_3_REG			(ISP_WPE_BASE + 0x0370)
#define WPE_ADDR_GEN_RSV1_3_REG				(ISP_WPE_BASE + 0x0374)
#define WPE_ADDR_GEN_DEBUG_SEL_3_REG			(ISP_WPE_BASE + 0x0378)
#define WPE_DMA_SOFT_RSTSTAT_REG			(ISP_WPE_BASE + 0x03C0)
#define WPE_TDRI_BASE_ADDR_REG				(ISP_WPE_BASE + 0x03C4)
#define WPE_TDRI_OFST_ADDR_REG				(ISP_WPE_BASE + 0x03C8)
#define WPE_TDRI_XSIZE_REG				(ISP_WPE_BASE + 0x03CC)
#define WPE_VERTICAL_FLIP_EN_REG			(ISP_WPE_BASE + 0x03D0)
#define WPE_DMA_SOFT_RESET_REG				(ISP_WPE_BASE + 0x03D4)
#define WPE_LAST_ULTRA_EN_REG				(ISP_WPE_BASE + 0x03D8)
#define WPE_SPECIAL_FUN_EN_REG				(ISP_WPE_BASE + 0x03DC)
#define WPE_WPEO_BASE_ADDR_REG				(ISP_WPE_BASE + 0x03F0)
#define WPE_WPEO_OFST_ADDR_REG				(ISP_WPE_BASE + 0x03F8)
#define WPE_WPEO_XSIZE_REG				(ISP_WPE_BASE + 0x0400)
#define WPE_WPEO_YSIZE_REG				(ISP_WPE_BASE + 0x0404)
#define WPE_WPEO_STRIDE_REG				(ISP_WPE_BASE + 0x0408)
#define WPE_WPEO_CON_REG				(ISP_WPE_BASE + 0x040C)
#define WPE_WPEO_CON2_REG				(ISP_WPE_BASE + 0x0410)
#define WPE_WPEO_CON3_REG				(ISP_WPE_BASE + 0x0414)
#define WPE_WPEO_CROP_REG				(ISP_WPE_BASE + 0x0418)
#define WPE_MSKO_BASE_ADDR_REG				(ISP_WPE_BASE + 0x0420)
#define WPE_MSKO_OFST_ADDR_REG				(ISP_WPE_BASE + 0x0428)
#define WPE_MSKO_XSIZE_REG				(ISP_WPE_BASE + 0x0430)
#define WPE_MSKO_YSIZE_REG				(ISP_WPE_BASE + 0x0434)
#define WPE_MSKO_STRIDE_REG				(ISP_WPE_BASE + 0x0438)
#define WPE_MSKO_CON_REG				(ISP_WPE_BASE + 0x043C)
#define WPE_MSKO_CON2_REG				(ISP_WPE_BASE + 0x0440)
#define WPE_MSKO_CON3_REG				(ISP_WPE_BASE + 0x0444)
#define WPE_MSKO_CROP_REG				(ISP_WPE_BASE + 0x0448)
#define WPE_VECI_BASE_ADDR_REG				(ISP_WPE_BASE + 0x0450)
#define WPE_VECI_OFST_ADDR_REG				(ISP_WPE_BASE + 0x0458)
#define WPE_VECI_XSIZE_REG				(ISP_WPE_BASE + 0x0460)
#define WPE_VECI_YSIZE_REG				(ISP_WPE_BASE + 0x0464)
#define WPE_VECI_STRIDE_REG				(ISP_WPE_BASE + 0x0468)
#define WPE_VECI_CON_REG				(ISP_WPE_BASE + 0x046C)
#define WPE_VECI_CON2_REG				(ISP_WPE_BASE + 0x0470)
#define WPE_VECI_CON3_REG				(ISP_WPE_BASE + 0x0474)
#define WPE_VEC2I_BASE_ADDR_REG				(ISP_WPE_BASE + 0x0480)
#define WPE_VEC2I_OFST_ADDR_REG				(ISP_WPE_BASE + 0x0488)
#define WPE_VEC2I_XSIZE_REG				(ISP_WPE_BASE + 0x0490)
#define WPE_VEC2I_YSIZE_REG				(ISP_WPE_BASE + 0x0494)
#define WPE_VEC2I_STRIDE_REG				(ISP_WPE_BASE + 0x0498)
#define WPE_VEC2I_CON_REG				(ISP_WPE_BASE + 0x049C)
#define WPE_VEC2I_CON2_REG				(ISP_WPE_BASE + 0x04A0)
#define WPE_VEC2I_CON3_REG				(ISP_WPE_BASE + 0x04A4)
#define WPE_VEC3I_BASE_ADDR_REG				(ISP_WPE_BASE + 0x04B0)
#define WPE_VEC3I_OFST_ADDR_REG				(ISP_WPE_BASE + 0x04B8)
#define WPE_VEC3I_XSIZE_REG				(ISP_WPE_BASE + 0x04C0)
#define WPE_VEC3I_YSIZE_REG				(ISP_WPE_BASE + 0x04C4)
#define WPE_VEC3I_STRIDE_REG				(ISP_WPE_BASE + 0x04C8)
#define WPE_VEC3I_CON_REG				(ISP_WPE_BASE + 0x04CC)
#define WPE_VEC3I_CON2_REG				(ISP_WPE_BASE + 0x04D0)
#define WPE_VEC3I_CON3_REG				(ISP_WPE_BASE + 0x04D4)
#define WPE_DMA_ERR_CTRL_REG				(ISP_WPE_BASE + 0x04E0)
#define WPE_WPEO_ERR_STAT_REG				(ISP_WPE_BASE + 0x04E4)
#define WPE_MSKO_ERR_STAT_REG				(ISP_WPE_BASE + 0x04E8)
#define WPE_VECI_ERR_STAT_REG				(ISP_WPE_BASE + 0x04EC)
#define WPE_VEC2I_ERR_STAT_REG				(ISP_WPE_BASE + 0x04F0)
#define WPE_VEC3I_ERR_STAT_REG				(ISP_WPE_BASE + 0x04F4)
#define WPE_DMA_DEBUG_ADDR_REG				(ISP_WPE_BASE + 0x04F8)
#define WPE_DMA_RSV1_REG				(ISP_WPE_BASE + 0x04FC)
#define WPE_DMA_RSV2_REG				(ISP_WPE_BASE + 0x0500)
#define WPE_DMA_RSV3_REG				(ISP_WPE_BASE + 0x0504)
#define WPE_DMA_RSV4_REG				(ISP_WPE_BASE + 0x0508)
#define WPE_DMA_DEBUG_SEL_REG				(ISP_WPE_BASE + 0x050C)

/*SW Access Registers : using mapped base address from DTS*/
#define WPE_B_WPE_START_REG                      (ISP_WPE_B_BASE)
#define WPE_B_CTL_MOD_EN_REG                     (ISP_WPE_B_BASE + 0x0004)
#define WPE_B_CTL_DMA_EN_REG                     (ISP_WPE_B_BASE + 0x0008)
#define WPE_B_CTL_CFG_REG                        (ISP_WPE_B_BASE + 0x0010)
#define WPE_B_CTL_FMT_SEL_REG                    (ISP_WPE_B_BASE + 0x0014)
#define WPE_B_CTL_INT_EN_REG                     (ISP_WPE_B_BASE + 0x0018)
#define WPE_B_CTL_INT_STATUS_REG                 (ISP_WPE_B_BASE + 0x0020)
#define WPE_B_CTL_INT_STATUSX_REG                (ISP_WPE_B_BASE + 0x0024)
#define WPE_B_CTL_TDR_TILE_REG                   (ISP_WPE_B_BASE + 0x0028)
#define WPE_B_CTL_TDR_DBG_STATUS_REG             (ISP_WPE_B_BASE + 0x002C)
#define WPE_B_CTL_TDR_TCM_EN_REG                 (ISP_WPE_B_BASE + 0x0030)
#define WPE_B_CTL_SW_CTL_REG                     (ISP_WPE_B_BASE + 0x0034)
#define WPE_B_CTL_SPARE0_REG                     (ISP_WPE_B_BASE + 0x0038)
#define WPE_B_CTL_SPARE1_REG                     (ISP_WPE_B_BASE + 0x003C)
#define WPE_B_CTL_SPARE2_REG                     (ISP_WPE_B_BASE + 0x0040)
#define WPE_B_CTL_DONE_SEL_REG                   (ISP_WPE_B_BASE + 0x0044)
#define WPE_B_CTL_DBG_SET_REG                    (ISP_WPE_B_BASE + 0x0048)
#define WPE_B_CTL_DBG_PORT_REG                   (ISP_WPE_B_BASE + 0x004C)
#define WPE_B_CTL_DATE_CODE_REG                  (ISP_WPE_B_BASE + 0x0050)
#define WPE_B_CTL_PROJ_CODE_REG                  (ISP_WPE_B_BASE + 0x0054)
#define WPE_B_CTL_WPE_DCM_DIS_REG                (ISP_WPE_B_BASE + 0x0058)
#define WPE_B_CTL_DMA_DCM_DIS_REG                (ISP_WPE_B_BASE + 0x005C)
#define WPE_B_CTL_WPE_DCM_STATUS_REG             (ISP_WPE_B_BASE + 0x0060)
#define WPE_B_CTL_DMA_DCM_STATUS_REG             (ISP_WPE_B_BASE + 0x0064)
#define WPE_B_CTL_WPE_REQ_STATUS_REG             (ISP_WPE_B_BASE + 0x0068)
#define WPE_B_CTL_DMA_REQ_STATUS_REG             (ISP_WPE_B_BASE + 0x006C)
#define WPE_B_CTL_WPE_RDY_STATUS_REG             (ISP_WPE_B_BASE + 0x0070)
#define WPE_B_CTL_DMA_RDY_STATUS_REG             (ISP_WPE_B_BASE + 0x0074)
#define WPE_B_VGEN_CTL_REG                       (ISP_WPE_B_BASE + 0x00C0)
#define WPE_B_VGEN_IN_IMG_REG                    (ISP_WPE_B_BASE + 0x00C4)
#define WPE_B_VGEN_OUT_IMG_REG                   (ISP_WPE_B_BASE + 0x00C8)
#define WPE_B_VGEN_HORI_STEP_REG                 (ISP_WPE_B_BASE + 0x00CC)
#define WPE_B_VGEN_VERT_STEP_REG                 (ISP_WPE_B_BASE + 0x00D0)
#define WPE_B_VGEN_HORI_INT_OFST_REG             (ISP_WPE_B_BASE + 0x00D4)
#define WPE_B_VGEN_HORI_SUB_OFST_REG             (ISP_WPE_B_BASE + 0x00D8)
#define WPE_B_VGEN_VERT_INT_OFST_REG             (ISP_WPE_B_BASE + 0x00DC)
#define WPE_B_VGEN_VERT_SUB_OFST_REG             (ISP_WPE_B_BASE + 0x00E0)
#define WPE_B_VGEN_POST_CTL_REG                  (ISP_WPE_B_BASE + 0x00E8)
#define WPE_B_VGEN_POST_COMP_X_REG               (ISP_WPE_B_BASE + 0x00EC)
#define WPE_B_VGEN_POST_COMP_Y_REG               (ISP_WPE_B_BASE + 0x00F0)
#define WPE_B_VGEN_MAX_VEC_REG                   (ISP_WPE_B_BASE + 0x00F4)
#define WPE_B_VFIFO_CTL_REG                      (ISP_WPE_B_BASE + 0x00F8)
#define WPE_B_CFIFO_CTL_REG                      (ISP_WPE_B_BASE + 0x0140)
#define WPE_B_RWCTL_CTL_REG                      (ISP_WPE_B_BASE + 0x0150)
#define WPE_B_CACHI_SPECIAL_FUN_EN_REG           (ISP_WPE_B_BASE + 0x0160)
#define WPE_B_C24_TILE_EDGE_REG                  (ISP_WPE_B_BASE + 0x0170)
#define WPE_B_MDP_CROP_X_REG                     (ISP_WPE_B_BASE + 0x0190)
#define WPE_B_MDP_CROP_Y_REG                     (ISP_WPE_B_BASE + 0x0194)
#define WPE_B_ISPCROP_CON1_REG                   (ISP_WPE_B_BASE + 0x01C0)
#define WPE_B_ISPCROP_CON2_REG                   (ISP_WPE_B_BASE + 0x01C4)
#define WPE_B_PSP_CTL_REG                        (ISP_WPE_B_BASE + 0x01F0)
#define WPE_B_PSP2_CTL_REG                       (ISP_WPE_B_BASE + 0x01F4)
#define WPE_B_ADDR_GEN_SOFT_RSTSTAT_0_REG        (ISP_WPE_B_BASE + 0x02C0)
#define WPE_B_ADDR_GEN_BASE_ADDR_0_REG           (ISP_WPE_B_BASE + 0x02C4)
#define WPE_B_ADDR_GEN_OFFSET_ADDR_0_REG         (ISP_WPE_B_BASE + 0x02C8)
#define WPE_B_ADDR_GEN_STRIDE_0_REG              (ISP_WPE_B_BASE + 0x02CC)
#define WPE_B_CACHI_CON_0_REG                    (ISP_WPE_B_BASE + 0x02D0)
#define WPE_B_CACHI_CON2_0_REG                   (ISP_WPE_B_BASE + 0x02D4)
#define WPE_B_CACHI_CON3_0_REG                   (ISP_WPE_B_BASE + 0x02D8)
#define WPE_B_ADDR_GEN_ERR_CTRL_0_REG            (ISP_WPE_B_BASE + 0x02DC)
#define WPE_B_ADDR_GEN_ERR_STAT_0_REG            (ISP_WPE_B_BASE + 0x02E0)
#define WPE_B_ADDR_GEN_RSV1_0_REG                (ISP_WPE_B_BASE + 0x02E4)
#define WPE_B_ADDR_GEN_DEBUG_SEL_0_REG           (ISP_WPE_B_BASE + 0x02E8)
#define WPE_B_ADDR_GEN_SOFT_RSTSTAT_1_REG        (ISP_WPE_B_BASE + 0x02F0)
#define WPE_B_ADDR_GEN_BASE_ADDR_1_REG           (ISP_WPE_B_BASE + 0x02F4)
#define WPE_B_ADDR_GEN_OFFSET_ADDR_1_REG         (ISP_WPE_B_BASE + 0x02F8)
#define WPE_B_ADDR_GEN_STRIDE_1_REG              (ISP_WPE_B_BASE + 0x02FC)
#define WPE_B_CACHI_CON_1_REG                    (ISP_WPE_B_BASE + 0x0300)
#define WPE_B_CACHI_CON2_1_REG                   (ISP_WPE_B_BASE + 0x0304)
#define WPE_B_CACHI_CON3_1_REG                   (ISP_WPE_B_BASE + 0x0308)
#define WPE_B_ADDR_GEN_ERR_CTRL_1_REG            (ISP_WPE_B_BASE + 0x030C)
#define WPE_B_ADDR_GEN_ERR_STAT_1_REG            (ISP_WPE_B_BASE + 0x0310)
#define WPE_B_ADDR_GEN_RSV1_1_REG                (ISP_WPE_B_BASE + 0x0314)
#define WPE_B_ADDR_GEN_DEBUG_SEL_1_REG           (ISP_WPE_B_BASE + 0x0318)
#define WPE_B_ADDR_GEN_SOFT_RSTSTAT_2_REG        (ISP_WPE_B_BASE + 0x0320)
#define WPE_B_ADDR_GEN_BASE_ADDR_2_REG           (ISP_WPE_B_BASE + 0x0324)
#define WPE_B_ADDR_GEN_OFFSET_ADDR_2_REG         (ISP_WPE_B_BASE + 0x0328)
#define WPE_B_ADDR_GEN_STRIDE_2_REG              (ISP_WPE_B_BASE + 0x032C)
#define WPE_B_CACHI_CON_2_REG                    (ISP_WPE_B_BASE + 0x0330)
#define WPE_B_CACHI_CON2_2_REG                   (ISP_WPE_B_BASE + 0x0334)
#define WPE_B_CACHI_CON3_2_REG                   (ISP_WPE_B_BASE + 0x0338)
#define WPE_B_ADDR_GEN_ERR_CTRL_2_REG            (ISP_WPE_B_BASE + 0x033C)
#define WPE_B_ADDR_GEN_ERR_STAT_2_REG            (ISP_WPE_B_BASE + 0x0340)
#define WPE_B_ADDR_GEN_RSV1_2_REG                (ISP_WPE_B_BASE + 0x0344)
#define WPE_B_ADDR_GEN_DEBUG_SEL_2_REG           (ISP_WPE_B_BASE + 0x0348)
#define WPE_B_ADDR_GEN_SOFT_RSTSTAT_3_REG        (ISP_WPE_B_BASE + 0x0350)
#define WPE_B_ADDR_GEN_BASE_ADDR_3_REG           (ISP_WPE_B_BASE + 0x0354)
#define WPE_B_ADDR_GEN_OFFSET_ADDR_3_REG         (ISP_WPE_B_BASE + 0x0358)
#define WPE_B_ADDR_GEN_STRIDE_3_REG              (ISP_WPE_B_BASE + 0x035C)
#define WPE_B_CACHI_CON_3_REG                    (ISP_WPE_B_BASE + 0x0360)
#define WPE_B_CACHI_CON2_3_REG                   (ISP_WPE_B_BASE + 0x0364)
#define WPE_B_CACHI_CON3_3_REG                   (ISP_WPE_B_BASE + 0x0368)
#define WPE_B_ADDR_GEN_ERR_CTRL_3_REG            (ISP_WPE_B_BASE + 0x036C)
#define WPE_B_ADDR_GEN_ERR_STAT_3_REG            (ISP_WPE_B_BASE + 0x0370)
#define WPE_B_ADDR_GEN_RSV1_3_REG                (ISP_WPE_B_BASE + 0x0374)
#define WPE_B_ADDR_GEN_DEBUG_SEL_3_REG           (ISP_WPE_B_BASE + 0x0378)
#define WPE_B_DMA_SOFT_RSTSTAT_REG               (ISP_WPE_B_BASE + 0x03C0)
#define WPE_B_TDRI_BASE_ADDR_REG                 (ISP_WPE_B_BASE + 0x03C4)
#define WPE_B_TDRI_OFST_ADDR_REG                 (ISP_WPE_B_BASE + 0x03C8)
#define WPE_B_TDRI_XSIZE_REG                     (ISP_WPE_B_BASE + 0x03CC)
#define WPE_B_VERTICAL_FLIP_EN_REG               (ISP_WPE_B_BASE + 0x03D0)
#define WPE_B_DMA_SOFT_RESET_REG                 (ISP_WPE_B_BASE + 0x03D4)
#define WPE_B_LAST_ULTRA_EN_REG                  (ISP_WPE_B_BASE + 0x03D8)
#define WPE_B_SPECIAL_FUN_EN_REG                 (ISP_WPE_B_BASE + 0x03DC)
#define WPE_B_WPEO_BASE_ADDR_REG                 (ISP_WPE_B_BASE + 0x03F0)
#define WPE_B_WPEO_OFST_ADDR_REG                 (ISP_WPE_B_BASE + 0x03F8)
#define WPE_B_WPEO_XSIZE_REG                     (ISP_WPE_B_BASE + 0x0400)
#define WPE_B_WPEO_YSIZE_REG                     (ISP_WPE_B_BASE + 0x0404)
#define WPE_B_WPEO_STRIDE_REG                    (ISP_WPE_B_BASE + 0x0408)
#define WPE_B_WPEO_CON_REG                       (ISP_WPE_B_BASE + 0x040C)
#define WPE_B_WPEO_CON2_REG                      (ISP_WPE_B_BASE + 0x0410)
#define WPE_B_WPEO_CON3_REG                      (ISP_WPE_B_BASE + 0x0414)
#define WPE_B_WPEO_CROP_REG                      (ISP_WPE_B_BASE + 0x0418)
#define WPE_B_MSKO_BASE_ADDR_REG                 (ISP_WPE_B_BASE + 0x0420)
#define WPE_B_MSKO_OFST_ADDR_REG                 (ISP_WPE_B_BASE + 0x0428)
#define WPE_B_MSKO_XSIZE_REG                     (ISP_WPE_B_BASE + 0x0430)
#define WPE_B_MSKO_YSIZE_REG                     (ISP_WPE_B_BASE + 0x0434)
#define WPE_B_MSKO_STRIDE_REG                    (ISP_WPE_B_BASE + 0x0438)
#define WPE_B_MSKO_CON_REG                       (ISP_WPE_B_BASE + 0x043C)
#define WPE_B_MSKO_CON2_REG                      (ISP_WPE_B_BASE + 0x0440)
#define WPE_B_MSKO_CON3_REG                      (ISP_WPE_B_BASE + 0x0444)
#define WPE_B_MSKO_CROP_REG                      (ISP_WPE_B_BASE + 0x0448)
#define WPE_B_VECI_BASE_ADDR_REG                 (ISP_WPE_B_BASE + 0x0450)
#define WPE_B_VECI_OFST_ADDR_REG                 (ISP_WPE_B_BASE + 0x0458)
#define WPE_B_VECI_XSIZE_REG                     (ISP_WPE_B_BASE + 0x0460)
#define WPE_B_VECI_YSIZE_REG                     (ISP_WPE_B_BASE + 0x0464)
#define WPE_B_VECI_STRIDE_REG                    (ISP_WPE_B_BASE + 0x0468)
#define WPE_B_VECI_CON_REG                       (ISP_WPE_B_BASE + 0x046C)
#define WPE_B_VECI_CON2_REG                      (ISP_WPE_B_BASE + 0x0470)
#define WPE_B_VECI_CON3_REG                      (ISP_WPE_B_BASE + 0x0474)
#define WPE_B_VEC2I_BASE_ADDR_REG                (ISP_WPE_B_BASE + 0x0480)
#define WPE_B_VEC2I_OFST_ADDR_REG                (ISP_WPE_B_BASE + 0x0488)
#define WPE_B_VEC2I_XSIZE_REG                    (ISP_WPE_B_BASE + 0x0490)
#define WPE_B_VEC2I_YSIZE_REG                    (ISP_WPE_B_BASE + 0x0494)
#define WPE_B_VEC2I_STRIDE_REG                   (ISP_WPE_B_BASE + 0x0498)
#define WPE_B_VEC2I_CON_REG                      (ISP_WPE_B_BASE + 0x049C)
#define WPE_B_VEC2I_CON2_REG                     (ISP_WPE_B_BASE + 0x04A0)
#define WPE_B_VEC2I_CON3_REG                     (ISP_WPE_B_BASE + 0x04A4)
#define WPE_B_VEC3I_BASE_ADDR_REG                (ISP_WPE_B_BASE + 0x04B0)
#define WPE_B_VEC3I_OFST_ADDR_REG                (ISP_WPE_B_BASE + 0x04B8)
#define WPE_B_VEC3I_XSIZE_REG                    (ISP_WPE_B_BASE + 0x04C0)
#define WPE_B_VEC3I_YSIZE_REG                    (ISP_WPE_B_BASE + 0x04C4)
#define WPE_B_VEC3I_STRIDE_REG                   (ISP_WPE_B_BASE + 0x04C8)
#define WPE_B_VEC3I_CON_REG                      (ISP_WPE_B_BASE + 0x04CC)
#define WPE_B_VEC3I_CON2_REG                     (ISP_WPE_B_BASE + 0x04D0)
#define WPE_B_VEC3I_CON3_REG                     (ISP_WPE_B_BASE + 0x04D4)
#define WPE_B_DMA_ERR_CTRL_REG                   (ISP_WPE_B_BASE + 0x04E0)
#define WPE_B_WPEO_ERR_STAT_REG                  (ISP_WPE_B_BASE + 0x04E4)
#define WPE_B_MSKO_ERR_STAT_REG                  (ISP_WPE_B_BASE + 0x04E8)
#define WPE_B_VECI_ERR_STAT_REG                  (ISP_WPE_B_BASE + 0x04EC)
#define WPE_B_VEC2I_ERR_STAT_REG                 (ISP_WPE_B_BASE + 0x04F0)
#define WPE_B_VEC3I_ERR_STAT_REG                 (ISP_WPE_B_BASE + 0x04F4)
#define WPE_B_DMA_DEBUG_ADDR_REG                 (ISP_WPE_B_BASE + 0x04F8)
#define WPE_B_DMA_RSV1_REG                       (ISP_WPE_B_BASE + 0x04FC)
#define WPE_B_DMA_RSV2_REG                       (ISP_WPE_B_BASE + 0x0500)
#define WPE_B_DMA_RSV3_REG                       (ISP_WPE_B_BASE + 0x0504)
#define WPE_B_DMA_RSV4_REG                       (ISP_WPE_B_BASE + 0x0508)
#define WPE_B_DMA_DEBUG_SEL_REG                  (ISP_WPE_B_BASE + 0x050C)


/***********************************************************************
 *
 ***********************************************************************/
static inline unsigned int WPE_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

/***********************************************************************
 *
 ***********************************************************************/
static inline unsigned int WPE_UsToJiffies(unsigned int Us)
{
	return (((Us / 1000) * HZ + 512) >> 10);
}

/***********************************************************************
 *
 ***********************************************************************/
static inline unsigned int WPE_GetIRQState
	(unsigned int type, unsigned int userNumber, unsigned int stus,
		enum WPE_PROCESS_ID_ENUM whichReq, int ProcessID)
{
	unsigned int ret = 0;
	unsigned long flags;	/* old: MUINT32 flags; */
				/* FIX to avoid build warning */

	/*  */
	spin_lock_irqsave(&(WPEInfo.SpinLockIrq[type]), flags);
#ifdef WPE_USE_GCE

#ifdef WPE_MULTIPROCESS_TIMEING_ISSUE
	if (stus & WPE_INT_ST) {
		ret = ((WPEInfo.IrqInfo.WpeIrqCnt > 0)
		       && (WPEInfo.ProcessID[WPEInfo.ReadReqIdx] == ProcessID));
	} else {
		LOG_ERR
		    (
		     " WaitIRQ StatusErr, type:%d, userNum:%d, status:%d, whichReq:%d,ProcessID:0x%x, ReadReqIdx:%d\n",
		     type, userNumber, stus, whichReq,
		     ProcessID, WPEInfo.ReadReqIdx);
	}

#else
	if (stus & WPE_INT_ST) {
		ret = ((WPEInfo.IrqInfo.WpeIrqCnt > 0)
		       && (WPEInfo.IrqInfo.ProcessID[whichReq] == ProcessID));
	} else {
		LOG_ERR
		    (
		     "WaitIRQ Status Error, type:%d, userNumber:%d, status:%d, whichReq:%d, ProcessID:0x%x\n",
		     type, userNumber, stus, whichReq, ProcessID);
	}
#endif
#else
	ret = ((WPEInfo.IrqInfo.Status[type] & stus)
	       && (WPEInfo.IrqInfo.ProcessID[whichReq] == ProcessID));
#endif
	spin_unlock_irqrestore(&(WPEInfo.SpinLockIrq[type]), flags);
	/*  */
	return ret;
}


/***********************************************************************
 *
 ***********************************************************************/
static inline unsigned int WPE_JiffiesToMs(unsigned int Jiffies)
{
	return ((Jiffies * 1000) / HZ);
}


#define RegDump(start, end) {\
	unsigned int i;\
	for (i = start; i <= end; i += 0x10) {
		LOG_DBG(
		"[0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]",
	    (unsigned int)(ISP_WPE_BASE + i),
	    (unsigned int)WPE_RD32(ISP_WPE_BASE + i),
	    (unsigned int)(ISP_WPE_BASE + i+0x4),
	    (unsigned int)WPE_RD32(ISP_WPE_BASE + i+0x4),
	    (unsigned int)(ISP_WPE_BASE + i+0x8),
	    (unsigned int)WPE_RD32(ISP_WPE_BASE + i+0x8),
	    (unsigned int)(ISP_WPE_BASE + i+0xc),
	    (unsigned int)WPE_RD32(ISP_WPE_BASE + i+0xc));
	}
}


static bool ConfigWPERequest(signed int ReqIdx)
{
#ifdef WPE_USE_GCE
	unsigned int j;
	unsigned long flags;	/* old: MUINT32 flags; */
				/* FIX to avoid build warning */


	spin_lock_irqsave
	(&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]), flags);
	if (g_WPE_ReqRing.WPEReq_Struct[ReqIdx].State
		== WPE_REQUEST_STATE_PENDING) {
		g_WPE_ReqRing.WPEReq_Struct[ReqIdx].State
			= WPE_REQUEST_STATE_RUNNING;
		for (j = 0; j < _SUPPORT_MAX_WPE_FRAME_REQUEST_; j++) {
			if (WPE_FRAME_STATUS_ENQUE ==
			    g_WPE_ReqRing.WPEReq_Struct[
			    ReqIdx].WpeFrameStatus[j]) {
				g_WPE_ReqRing.WPEReq_Struct[
					ReqIdx].WpeFrameStatus[j] =
				    WPE_FRAME_STATUS_RUNNING;
				spin_unlock_irqrestore(
					&(WPEInfo.SpinLockIrq
					[WPE_IRQ_TYPE_INT_WPE_ST]), flags);
				/* Through MDP add GCE command
				 * ConfigWPEHW(&g_WPE_ReqRing.
				 * WPEReq_Struct[ReqIdx].WpeFrameConfig[j]);
				 */

				spin_lock_irqsave(
				&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]),
				flags);
			}
		}
	} else {
		LOG_ERR(
			"state machine error!!, ReqIdx:%d, State:%d\n",
			ReqIdx, g_WPE_ReqRing.WPEReq_Struct[ReqIdx].State);
	}
	spin_unlock_irqrestore(
		&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]), flags);


	return MTRUE;
#else
	LOG_ERR("Not support GCE mode.!!\n");
	return MFALSE;
#endif
}


static bool ConfigWPE(void)
{
#ifdef WPE_USE_GCE

	unsigned int i, j, k;
	unsigned long flags;	/* old: unsigned int flags; */
				/* FIX to avoid build warning */


	spin_lock_irqsave(
		&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]), flags);
	for (k = 0; k < _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_; k++) {
		i = (g_WPE_ReqRing.HWProcessIdx + k) %
			_SUPPORT_MAX_WPE_REQUEST_RING_SIZE_;
		if (g_WPE_ReqRing.WPEReq_Struct[i].State
			== WPE_REQUEST_STATE_PENDING) {
			g_WPE_ReqRing.WPEReq_Struct[i].State
				= WPE_REQUEST_STATE_RUNNING;
			for (j = 0; j < _SUPPORT_MAX_WPE_FRAME_REQUEST_; j++) {
				if (WPE_FRAME_STATUS_ENQUE ==
				    g_WPE_ReqRing.WPEReq_Struct[i]
				    .WpeFrameStatus[j]) {
					/* break; */
					g_WPE_ReqRing.WPEReq_Struct[i]
					.WpeFrameStatus[j] =
					    WPE_FRAME_STATUS_RUNNING;
					spin_unlock_irqrestore(
					&(WPEInfo.SpinLockIrq
					[WPE_IRQ_TYPE_INT_WPE_ST]), flags);
					ConfigWPEHW(&g_WPE_ReqRing.
					WPEReq_Struct[i].WpeFrameConfig[j]);
					spin_lock_irqsave(
					&(WPEInfo.SpinLockIrq
					[WPE_IRQ_TYPE_INT_WPE_ST]), flags);
				}
			}
			/* LOG_DBG("ConfigWPE idx j:%d\n",j); */
			if (j != _SUPPORT_MAX_WPE_FRAME_REQUEST_) {
				LOG_ERR(
					"WPE Config State is wrong!  idx j(%d), HWProcessIdx(%d), State(%d)\n",
				     j, g_WPE_ReqRing.HWProcessIdx,
				     g_WPE_ReqRing.WPEReq_Struct[i].State);
				/* g_WPE_ReqRing.WPEReq_Struct[*/
				/*i].WpeFrameStatus[j]*/
				/* = WPE_FRAME_STATUS_RUNNING;*/
				/* spin_unlock_irqrestore*/
				/*(&(WPEInfo.SpinLockIrq*/
				/*[WPE_IRQ_TYPE_INT_WPE_ST]), flags); */
				/* ConfigWPEHW(&g_Wpe_ReqRing.*/
				/*WPEReq_Struct[i].WpeFrameConfig[j]); */
				/* return MTRUE; */
				return MFALSE;
			}
			/*else {
			 *      g_WPE_ReqRing.WPEReq_Struct[i].State =
			 *            WPE_REQUEST_STATE_RUNNING;
			 *      LOG_ERR(
			 *	WPE Config State is wrong! HWProcessIdx(%d),
			 *          State(%d)\n",
			 *      g_WPE_ReqRing.HWProcessIdx,
			 *           g_WPE_ReqRing.WPEReq_Struct[i].State);
			 *      g_WPE_ReqRing.HWProcessIdx
			 *          = (g_WPE_ReqRing.HWProcessIdx+1)
			 *      %_SUPPORT_MAX_WPE_REQUEST_RING_SIZE_;
			 *}
			 */
		}
	}
	spin_unlock_irqrestore(
		&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]), flags);
	if (k == _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_)
		LOG_DBG("No any WPE Request in Ring!!\n");

	return MTRUE;


#else				/* #ifdef WPE_USE_GCE */

	unsigned int i, j, k;
	unsigned int flags;

	for (k = 0; k < _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_; k++) {
		i = (g_WPE_ReqRing.HWProcessIdx + k) %
			_SUPPORT_MAX_WPE_REQUEST_RING_SIZE_;
		if (g_WPE_ReqRing.WPEReq_Struct[i].State ==
			WPE_REQUEST_STATE_PENDING) {
			for (j = 0; j < _SUPPORT_MAX_WPE_FRAME_REQUEST_; j++) {
				if (WPE_FRAME_STATUS_ENQUE ==
				    g_WPE_ReqRing.WPEReq_Struct[
				    i].WpeFrameStatus[j]) {
					break;
				}
			}
			LOG_DBG("idx j:%d\n", j);
			if (j != _SUPPORT_MAX_WPE_FRAME_REQUEST_) {
				g_WPE_ReqRing.WPEReq_Struct[
				i].WpeFrameStatus[j] = WPE_FRAME_STATUS_RUNNING;
				ConfigWPEHW(&g_WPE_ReqRing.WPEReq_Struct[
					i].WpeFrameConfig[j]);
				return MTRUE;
			}
			/*else { */
			LOG_ERR(
			"WPE Config State is wrong! HWProcessIdx(%d), State(%d)\n",
			     g_WPE_ReqRing.HWProcessIdx,
			     g_WPE_ReqRing.WPEReq_Struct[i].State);
			g_WPE_ReqRing.HWProcessIdx =
			    (g_WPE_ReqRing.HWProcessIdx + 1) %
			    _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_;
			/*} */
		}
	}
	if (k == _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_)
		LOG_DBG("No any WPE Request in Ring!!\n");

	return MFALSE;

#endif				/* #ifdef WPE_USE_GCE */



}

#ifdef WPE_USE_GCE_IRQ

static bool UpdateWPE(pid_t *ProcessID)
{
#ifdef WPE_USE_GCE
	unsigned int i, j, next_idx;
	bool bFinishRequest = 0;

	for (i = g_WPE_ReqRing.HWProcessIdx;
		i < _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_; i++) {
		if (g_WPE_ReqRing.WPEReq_Struct[i].State ==
			WPE_REQUEST_STATE_RUNNING) {
			for (j = 0; j < _SUPPORT_MAX_WPE_FRAME_REQUEST_; j++) {
				if (WPE_FRAME_STATUS_RUNNING ==
				    g_WPE_ReqRing.WPEReq_Struct[
				    i].WpeFrameStatus[j]) {
					break;
				}
			}
			IRQ_LOG_KEEPER(WPE_IRQ_TYPE_INT_WPE_ST, m_CurrentPPB,
				_LOG_DBG,
				       "idx j:%d\n", j);
			if (j != _SUPPORT_MAX_WPE_FRAME_REQUEST_) {
				next_idx = j + 1;
				g_WPE_ReqRing.WPEReq_Struct[
					i].WpeFrameStatus[j] =
				    WPE_FRAME_STATUS_FINISHED;
				if ((_SUPPORT_MAX_WPE_FRAME_REQUEST_ ==
					(next_idx))
				    || ((_SUPPORT_MAX_WPE_FRAME_REQUEST_ >
				    (next_idx))
					&& (WPE_FRAME_STATUS_EMPTY ==
					    g_WPE_ReqRing.WPEReq_Struct[i].
					    WpeFrameStatus[next_idx]))) {
					bFinishRequest = MTRUE;
					(*ProcessID) =
						g_WPE_ReqRing.WPEReq_Struct[
						i].processID;
					g_WPE_ReqRing.WPEReq_Struct[i].State =
					    WPE_REQUEST_STATE_FINISHED;
					g_WPE_ReqRing.HWProcessIdx =
					(g_WPE_ReqRing.HWProcessIdx + 1) %
					    _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_;
					IRQ_LOG_KEEPER(WPE_IRQ_TYPE_INT_WPE_ST,
						m_CurrentPPB, _LOG_INF,
						"Finish WPE Request i:%d, j:%d, HWProcessIdx:%d\n",
					i, j, g_WPE_ReqRing.HWProcessIdx);
				} else {
					IRQ_LOG_KEEPER(WPE_IRQ_TYPE_INT_WPE_ST,
						m_CurrentPPB, _LOG_DBG,
						"Finish WPE Frame i:%d, j:%d, HWProcessIdx:%d\n",
					i, j, g_WPE_ReqRing.HWProcessIdx);
				}
				break;
			}
			/*else { */
			IRQ_LOG_KEEPER(WPE_IRQ_TYPE_INT_WPE_ST,
					m_CurrentPPB, _LOG_ERR,
				       "WPE State Machine is wrong! HWProcessIdx(%d), State(%d)\n",
				       g_WPE_ReqRing.HWProcessIdx,
				       g_WPE_ReqRing.WPEReq_Struct[i].State);
			g_WPE_ReqRing.WPEReq_Struct[i].State =
				WPE_REQUEST_STATE_FINISHED;
			g_WPE_ReqRing.HWProcessIdx =
			    (g_WPE_ReqRing.HWProcessIdx + 1) %
			    _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_;
			break;
			/*} */
		}
	}

	return bFinishRequest;


#else				/* #ifdef WPE_USE_GCE */
	unsigned int i, j, next_idx;
	bool bFinishRequest = MFALSE;

	for (i = g_WPE_ReqRing.HWProcessIdx;
	i < _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_; i++) {
		if (g_WPE_ReqRing.WPEReq_Struct[i].State ==
			WPE_REQUEST_STATE_PENDING) {
			for (j = 0; j < _SUPPORT_MAX_WPE_FRAME_REQUEST_; j++) {
				if (WPE_FRAME_STATUS_RUNNING ==
				    g_WPE_ReqRing.WPEReq_Struct[i].
				    WpeFrameStatus[j]) {
					break;
				}
			}
			IRQ_LOG_KEEPER(WPE_IRQ_TYPE_INT_WPE_ST,
				m_CurrentPPB, _LOG_DBG,
				       "idx j:%d\n", j);
			if (j != _SUPPORT_MAX_WPE_FRAME_REQUEST_) {
				next_idx = j + 1;
				g_WPE_ReqRing.WPEReq_Struct[
					i].WpeFrameStatus[j] =
				    WPE_FRAME_STATUS_FINISHED;
				if ((_SUPPORT_MAX_WPE_FRAME_REQUEST_
					== (next_idx))
				    || ((_SUPPORT_MAX_WPE_FRAME_REQUEST_
				    > (next_idx))
					&& (WPE_FRAME_STATUS_EMPTY ==
					    g_WPE_ReqRing.WPEReq_Struct[i].
					    WpeFrameStatus[next_idx]))) {
					bFinishRequest = MTRUE;

					(*ProcessID) =
					g_WPE_ReqRing.WPEReq_Struct[i].
						processID;

					g_WPE_ReqRing.WPEReq_Struct[i].State =
					    WPE_REQUEST_STATE_FINISHED;

					g_WPE_ReqRing.HWProcessIdx =
					    (g_WPE_ReqRing.HWProcessIdx + 1) %
					    _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_;
					IRQ_LOG_KEEPER(WPE_IRQ_TYPE_INT_WPE_ST,
						m_CurrentPPB, _LOG_INF,
					"Finish WPE Request i:%d, j:%d, HWProcessIdx:%d\n",
					i, j, g_WPE_ReqRing.HWProcessIdx);
				} else {
					IRQ_LOG_KEEPER(WPE_IRQ_TYPE_INT_WPE_ST,
						m_CurrentPPB, _LOG_DBG,
					"Finish WPE Frame i:%d, j:%d, HWProcessIdx:%d\n",
					i, j, g_WPE_ReqRing.HWProcessIdx);
				}
				break;
			}
			/*else { */
			IRQ_LOG_KEEPER(WPE_IRQ_TYPE_INT_WPE_ST, m_CurrentPPB,
					_LOG_ERR,
				       "WPE State Machine is wrong! HWProcessIdx(%d), State(%d)\n",
				       g_WPE_ReqRing.HWProcessIdx,
				       g_WPE_ReqRing.WPEReq_Struct[i].State);

			g_WPE_ReqRing.WPEReq_Struct[i].State =
				WPE_REQUEST_STATE_FINISHED;

			g_WPE_ReqRing.HWProcessIdx =
			    (g_WPE_ReqRing.HWProcessIdx + 1) %
			    _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_;
			break;
			/*} */
		}
	}

	return bFinishRequest;

#endif				/* #ifdef WPE_USE_GCE */


}
#endif

static signed int ConfigWPEHW(struct WPE_Config *pWPEConfig)
#if !BYPASS_REG
{
#ifdef WPE_USE_GCE
	struct cmdqRecStruct *handle;	/* kernel-4.4 usage */
	/* cmdqRecHandle handle; *//* kernel-3.18 usage */
	uint64_t engineFlag = (1L << CMDQ_ENG_WPEI);
#endif

	if (WPE_DBG_DBGLOG == (WPE_DBG_DBGLOG & WPEInfo.DebugMask)) {

		LOG_DBG("ConfigWPEHW Start!\n");
		LOG_DBG("WPE_CTL_MOD_EN:0x%x!\n", pWPEConfig->WPE_CTL_MOD_EN);
		LOG_DBG("WPE_CTL_DMA_EN:0x%x!\n", pWPEConfig->WPE_CTL_DMA_EN);

		LOG_DBG("WPE_CTL_CFG:0x%x!\n", pWPEConfig->WPE_CTL_CFG);
		LOG_DBG("WPE_CTL_FMT_SEL:0x%x!\n", pWPEConfig->WPE_CTL_FMT_SEL);
		LOG_DBG("WPE_CTL_INT_EN:0x%x!\n", pWPEConfig->WPE_CTL_INT_EN);

		LOG_DBG("WPE_CTL_INT_STATUS:0x%x!\n",
			pWPEConfig->WPE_CTL_INT_STATUS);
		LOG_DBG("WPE_CTL_INT_STATUSX:0x%x!\n",
			pWPEConfig->WPE_CTL_INT_STATUSX);
		LOG_DBG("WPE_CTL_TDR_TILE:0x%x!\n",
			pWPEConfig->WPE_CTL_TDR_TILE);
		LOG_DBG("WPE_CTL_TDR_DBG_STATUS:0x%x!\n",
			pWPEConfig->WPE_CTL_TDR_DBG_STATUS);
		LOG_DBG("WPE_CTL_TDR_TCM_EN:0x%x!\n",
			pWPEConfig->WPE_CTL_TDR_TCM_EN);
		LOG_DBG("WPE_CTL_SW_CTL:0x%x!\n", pWPEConfig->WPE_CTL_SW_CTL);
		LOG_DBG("WPE_CTL_SPARE0:0x%x!\n", pWPEConfig->WPE_CTL_SPARE0);
		LOG_DBG("WPE_CTL_SPARE1:0x%x!\n", pWPEConfig->WPE_CTL_SPARE1);
		LOG_DBG("WPE_CTL_SPARE2:0x%x!\n", pWPEConfig->WPE_CTL_SPARE2);
		LOG_DBG("WPE_CTL_DONE_SEL:0x%x!\n",
			pWPEConfig->WPE_CTL_DONE_SEL);
		LOG_DBG("WPE_CTL_DBG_SET:0x%x!\n",
			pWPEConfig->WPE_CTL_DBG_SET);
		LOG_DBG("WPE_CTL_DBG_PORT:0x%x!\n",
			pWPEConfig->WPE_CTL_DBG_PORT);
		LOG_DBG("WPE_CTL_DATE_CODE:0x%x!\n",
			pWPEConfig->WPE_CTL_DATE_CODE);
		LOG_DBG("WPE_CTL_PROJ_CODE:0x%x!\n",
			pWPEConfig->WPE_CTL_PROJ_CODE);
		LOG_DBG("WPE_CTL_WPE_DCM_DIS:0x%x!\n",
			pWPEConfig->WPE_CTL_WPE_DCM_DIS);
		LOG_DBG("WPE_CTL_DMA_DCM_DIS:0x%x!\n",
			pWPEConfig->WPE_CTL_DMA_DCM_DIS);
		LOG_DBG("WPE_CTL_WPE_DCM_STATUS:0x%x!\n",
			pWPEConfig->WPE_CTL_WPE_DCM_STATUS);
		LOG_DBG("WPE_CTL_DMA_DCM_STATUS:0x%x!\n",
			pWPEConfig->WPE_CTL_DMA_DCM_STATUS);
		LOG_DBG("WPE_CTL_WPE_REQ_STATUS:0x%x!\n",
			pWPEConfig->WPE_CTL_WPE_REQ_STATUS);
		LOG_DBG("WPE_CTL_DMA_REQ_STATUS:0x%x!\n",
			pWPEConfig->WPE_CTL_DMA_REQ_STATUS);
		LOG_DBG("WPE_CTL_WPE_RDY_STATUS:0x%x!\n",
			pWPEConfig->WPE_CTL_WPE_RDY_STATUS);
		LOG_DBG("WPE_CTL_DMA_RDY_STATUS:0x%x!\n",
			pWPEConfig->WPE_CTL_DMA_RDY_STATUS);

		LOG_DBG("WPE_VGEN_CTL:0x%x!\n", pWPEConfig->WPE_VGEN_CTL);
		LOG_DBG("WPE_VGEN_IN_IMG:0x%x!\n",
			pWPEConfig->WPE_VGEN_IN_IMG);
		LOG_DBG("WPE_VGEN_OUT_IMG:0x%x!\n",
			pWPEConfig->WPE_VGEN_OUT_IMG);
		LOG_DBG("WPE_VGEN_HORI_STEP:0x%x!\n",
			pWPEConfig->WPE_VGEN_HORI_STEP);
		LOG_DBG("WPE_VGEN_VERT_STEP:0x%x!\n",
			pWPEConfig->WPE_VGEN_VERT_STEP);
		LOG_DBG("WPE_VGEN_HORI_INT_OFST_REG :0x%x!\n",
			pWPEConfig->WPE_VGEN_HORI_INT_OFST);
		LOG_DBG("WPE_VGEN_HORI_SUB_OFST:0x%x!\n",
			pWPEConfig->WPE_VGEN_HORI_SUB_OFST);
		LOG_DBG("WPE_VGEN_VERT_INT_OFST:0x%x!\n",
			pWPEConfig->WPE_VGEN_VERT_INT_OFST);
		LOG_DBG("WPE_VGEN_VERT_SUB_OFST:0x%x!\n",
			pWPEConfig->WPE_VGEN_VERT_SUB_OFST);

		LOG_DBG("WPE_VGEN_POST_CTL:0x%x!\n",
			pWPEConfig->WPE_VGEN_POST_CTL);
		LOG_DBG("WPE_VGEN_POST_COMP_X:0x%x!\n",
			pWPEConfig->WPE_VGEN_POST_COMP_X);
		LOG_DBG("WPE_VGEN_POST_COMP_Y:0x%x!\n",
			pWPEConfig->WPE_VGEN_POST_COMP_Y);
		LOG_DBG("WPE_VGEN_MAX_VEC:0x%x!\n",
			pWPEConfig->WPE_VGEN_MAX_VEC);
		LOG_DBG("WPE_VFIFO_CTL:0x%x!\n",
			pWPEConfig->WPE_VFIFO_CTL);

		LOG_DBG("WPE_CFIFO_CTL:0x%x!\n",
			pWPEConfig->WPE_CFIFO_CTL);

		LOG_DBG("WPE_RWCTL_CTL:0x%x!\n",
			pWPEConfig->WPE_RWCTL_CTL);

		LOG_DBG("WPE_CACHI_SPECIAL_FUN_EN:0x%x!\n",
			pWPEConfig->WPE_CACHI_SPECIAL_FUN_EN);

		LOG_DBG("WPE_C24_TILE_EDGE:0x%x!\n",
			pWPEConfig->WPE_C24_TILE_EDGE);

		LOG_DBG("WPE_MDP_CROP_X:0x%x!\n",
			pWPEConfig->WPE_MDP_CROP_X);
		LOG_DBG("WPE_MDP_CROP_Y:0x%x!\n",
			pWPEConfig->WPE_MDP_CROP_Y);

		LOG_DBG("WPE_ISPCROP_CON1:0x%x!\n",
			pWPEConfig->WPE_ISPCROP_CON1);
		LOG_DBG("WPE_ISPCROP_CON2:0x%x!\n",
			pWPEConfig->WPE_ISPCROP_CON2);

		LOG_DBG("WPE_PSP_CTL:0x%x!\n", pWPEConfig->WPE_PSP_CTL);
		LOG_DBG("WPE_PSP2_CTL:0x%x!\n", pWPEConfig->WPE_PSP2_CTL);

		LOG_DBG("WPE_ADDR_GEN_SOFT_RSTSTAT_0:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_SOFT_RSTSTAT_0);
		LOG_DBG("WPE_ADDR_GEN_BASE_ADDR_0:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_BASE_ADDR_0);
		LOG_DBG("WPE_ADDR_GEN_OFFSET_ADDR_0:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_OFFSET_ADDR_0);
		LOG_DBG("WPE_ADDR_GEN_STRIDE_0:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_STRIDE_0);
		LOG_DBG("WPE_CACHI_CON_0:0x%x!\n",
			pWPEConfig->WPE_CACHI_CON_0);
		LOG_DBG("WPE_CACHI_CON2_0:0x%x!\n",
			pWPEConfig->WPE_CACHI_CON2_0);
		LOG_DBG("WPE_CACHI_CON3_0:0x%x!\n",
			pWPEConfig->WPE_CACHI_CON3_0);
		LOG_DBG("WPE_ADDR_GEN_ERR_CTRL_0:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_ERR_CTRL_0);
		LOG_DBG("WPE_ADDR_GEN_ERR_STAT_0:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_ERR_STAT_0);
		LOG_DBG("WPE_ADDR_GEN_RSV1_0:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_RSV1_0);
		LOG_DBG("WPE_ADDR_GEN_DEBUG_SEL_0:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_DEBUG_SEL_0);

		LOG_DBG("WPE_ADDR_GEN_SOFT_RSTSTAT_1:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_SOFT_RSTSTAT_1);
		LOG_DBG("WPE_ADDR_GEN_BASE_ADDR_1:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_BASE_ADDR_1);
		LOG_DBG("WPE_ADDR_GEN_OFFSET_ADDR_1:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_OFFSET_ADDR_1);
		LOG_DBG("WPE_ADDR_GEN_STRIDE_1:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_STRIDE_1);
		LOG_DBG("WPE_CACHI_CON_1:0x%x!\n",
			pWPEConfig->WPE_CACHI_CON_1);
		LOG_DBG("WPE_CACHI_CON2_1_REG :0x%x!\n",
			pWPEConfig->WPE_CACHI_CON2_1);
		LOG_DBG("WPE_CACHI_CON3_1:0x%x!\n",
			pWPEConfig->WPE_CACHI_CON3_1);
		LOG_DBG("WPE_ADDR_GEN_ERR_CTRL_1:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_ERR_CTRL_1);
		LOG_DBG("WPE_ADDR_GEN_ERR_STAT_1:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_ERR_STAT_1);
		LOG_DBG("WPE_ADDR_GEN_RSV1_1:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_RSV1_1);
		LOG_DBG("WPE_ADDR_GEN_DEBUG_SEL_1:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_DEBUG_SEL_1);

		LOG_DBG("WPE_ADDR_GEN_SOFT_RSTSTAT_2:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_SOFT_RSTSTAT_2);
		LOG_DBG("WPE_ADDR_GEN_BASE_ADDR_2:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_BASE_ADDR_2);
		LOG_DBG("WPE_ADDR_GEN_OFFSET_ADDR_2:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_OFFSET_ADDR_2);
		LOG_DBG("WPE_ADDR_GEN_STRIDE_2:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_STRIDE_2);
		LOG_DBG("WPE_CACHI_CON_2:0x%x!\n",
			pWPEConfig->WPE_CACHI_CON_2);
		LOG_DBG("WPE_CACHI_CON2_2_REG :0x%x!\n",
			pWPEConfig->WPE_CACHI_CON2_2);
		LOG_DBG("WPE_CACHI_CON3_2:0x%x!\n",
			pWPEConfig->WPE_CACHI_CON3_2);
		LOG_DBG("WPE_ADDR_GEN_ERR_CTRL_2:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_ERR_CTRL_2);
		LOG_DBG("WPE_ADDR_GEN_ERR_STAT_2:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_ERR_STAT_2);
		LOG_DBG("WPE_ADDR_GEN_RSV1_2:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_RSV1_2);
		LOG_DBG("WPE_ADDR_GEN_DEBUG_SEL_2:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_DEBUG_SEL_2);

		LOG_DBG("WPE_ADDR_GEN_SOFT_RSTSTAT_3:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_SOFT_RSTSTAT_3);
		LOG_DBG("WPE_ADDR_GEN_BASE_ADDR_3:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_BASE_ADDR_3);
		LOG_DBG("WPE_ADDR_GEN_OFFSET_ADDR_3:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_OFFSET_ADDR_3);
		LOG_DBG("WPE_ADDR_GEN_STRIDE_3:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_STRIDE_3);
		LOG_DBG("WPE_CACHI_CON_3:0x%x!\n",
			pWPEConfig->WPE_CACHI_CON_3);
		LOG_DBG("WPE_CACHI_CON2_3_REG :0x%x!\n",
			pWPEConfig->WPE_CACHI_CON2_3);
		LOG_DBG("WPE_CACHI_CON3_3:0x%x!\n",
			pWPEConfig->WPE_CACHI_CON3_3);
		LOG_DBG("WPE_ADDR_GEN_ERR_CTRL_3:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_ERR_CTRL_3);
		LOG_DBG("WPE_ADDR_GEN_ERR_STAT_3:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_ERR_STAT_3);
		LOG_DBG("WPE_ADDR_GEN_RSV1_3:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_RSV1_3);
		LOG_DBG("WPE_ADDR_GEN_DEBUG_SEL_3:0x%x!\n",
			pWPEConfig->WPE_ADDR_GEN_DEBUG_SEL_3);

		LOG_DBG("WPE_DMA_SOFT_RSTSTAT:0x%x!\n",
			pWPEConfig->WPE_DMA_SOFT_RSTSTAT);
		LOG_DBG("WPE_TDRI_BASE_ADDR:0x%x!\n",
			pWPEConfig->WPE_TDRI_BASE_ADDR);
		LOG_DBG("WPE_TDRI_OFST_ADDR:0x%x!\n",
			pWPEConfig->WPE_TDRI_OFST_ADDR);
		LOG_DBG("WPE_TDRI_XSIZE:0x%x!\n",
			pWPEConfig->WPE_TDRI_XSIZE);
		LOG_DBG("WPE_VERTICAL_FLIP_EN:0x%x!\n",
			pWPEConfig->WPE_VERTICAL_FLIP_EN);
		LOG_DBG("WPE_DMA_SOFT_RESET:0x%x!\n",
			pWPEConfig->WPE_DMA_SOFT_RESET);
		LOG_DBG("WPE_LAST_ULTRA_EN:0x%x!\n",
			pWPEConfig->WPE_LAST_ULTRA_EN);
		LOG_DBG("WPE_SPECIAL_FUN_EN:0x%x!\n",
			pWPEConfig->WPE_SPECIAL_FUN_EN);

		LOG_DBG("WPE_WPEO_BASE_ADDR:0x%x!\n",
			pWPEConfig->WPE_WPEO_BASE_ADDR);

		LOG_DBG("WPE_WPEO_OFST_ADDR:0x%x!\n",
			pWPEConfig->WPE_WPEO_OFST_ADDR);

		LOG_DBG("WPE_WPEO_XSIZE:0x%x!\n",
			pWPEConfig->WPE_WPEO_XSIZE);
		LOG_DBG("WPE_WPEO_YSIZE:0x%x!\n",
			pWPEConfig->WPE_WPEO_YSIZE);
		LOG_DBG("WPE_WPEO_STRIDE:0x%x!\n",
			pWPEConfig->WPE_WPEO_STRIDE);
		LOG_DBG("WPE_WPEO_CON:0x%x!\n", pWPEConfig->WPE_WPEO_CON);
		LOG_DBG("WPE_WPEO_CON2:0x%x!\n", pWPEConfig->WPE_WPEO_CON2);
		LOG_DBG("WPE_WPEO_CON3:0x%x!\n", pWPEConfig->WPE_WPEO_CON3);
		LOG_DBG("WPE_WPEO_CROP:0x%x!\n", pWPEConfig->WPE_WPEO_CROP);

		LOG_DBG("WPE_MSKO_BASE_ADDR:0x%x!\n",
			pWPEConfig->WPE_MSKO_BASE_ADDR);

		LOG_DBG("WPE_MSKO_OFST_ADDR:0x%x!\n",
			pWPEConfig->WPE_MSKO_OFST_ADDR);

		LOG_DBG("WPE_MSKO_XSIZE:0x%x!\n", pWPEConfig->WPE_MSKO_XSIZE);
		LOG_DBG("WPE_MSKO_YSIZE:0x%x!\n", pWPEConfig->WPE_MSKO_YSIZE);
		LOG_DBG("WPE_MSKO_STRIDE:0x%x!\n",
			pWPEConfig->WPE_MSKO_STRIDE);
		LOG_DBG("WPE_MSKO_CON:0x%x!\n", pWPEConfig->WPE_MSKO_CON);
		LOG_DBG("WPE_MSKO_CON2:0x%x!\n", pWPEConfig->WPE_MSKO_CON2);
		LOG_DBG("WPE_MSKO_CON3:0x%x!\n", pWPEConfig->WPE_MSKO_CON3);
		LOG_DBG("WPE_MSKO_CROP:0x%x!\n", pWPEConfig->WPE_MSKO_CROP);

		LOG_DBG("WPE_VECI_BASE_ADDR:0x%x!\n",
			pWPEConfig->WPE_VECI_BASE_ADDR);

		LOG_DBG("WPE_VECI_OFST_ADDR:0x%x!\n",
			pWPEConfig->WPE_VECI_OFST_ADDR);

		LOG_DBG("WPE_VECI_XSIZE:0x%x!\n", pWPEConfig->WPE_VECI_XSIZE);
		LOG_DBG("WPE_VECI_YSIZE:0x%x!\n", pWPEConfig->WPE_VECI_YSIZE);
		LOG_DBG("WPE_VECI_STRIDE:0x%x!\n",
			pWPEConfig->WPE_VECI_STRIDE);
		LOG_DBG("WPE_VECI_CON:0x%x!\n", pWPEConfig->WPE_VECI_CON);
		LOG_DBG("WPE_VECI_CON2:0x%x!\n", pWPEConfig->WPE_VECI_CON2);
		LOG_DBG("WPE_VECI_CON3:0x%x!\n", pWPEConfig->WPE_VECI_CON3);

		LOG_DBG("WPE_VEC2I_BASE_ADDR:0x%x!\n",
			pWPEConfig->WPE_VEC2I_BASE_ADDR);

		LOG_DBG("WPE_VEC2I_OFST_ADDR:0x%x!\n",
			pWPEConfig->WPE_VEC2I_OFST_ADDR);

		LOG_DBG("WPE_VEC2I_XSIZE:0x%x!\n",
			pWPEConfig->WPE_VEC2I_XSIZE);
		LOG_DBG("WPE_VEC2I_YSIZE:0x%x!\n",
			pWPEConfig->WPE_VEC2I_YSIZE);
		LOG_DBG("WPE_VEC2I_STRIDE:0x%x!\n",
			pWPEConfig->WPE_VEC2I_STRIDE);
		LOG_DBG("WPE_VEC2I_CON:0x%x!\n", pWPEConfig->WPE_VEC2I_CON);
		LOG_DBG("WPE_VEC2I_CON2:0x%x!\n", pWPEConfig->WPE_VEC2I_CON2);
		LOG_DBG("WPE_VEC2I_CON3:0x%x!\n", pWPEConfig->WPE_VEC2I_CON3);

		LOG_DBG("WPE_VEC3I_BASE_ADDR:0x%x!\n",
			pWPEConfig->WPE_VEC3I_BASE_ADDR);

		LOG_DBG("WPE_VEC3I_OFST_ADDR:0x%x!\n",
			pWPEConfig->WPE_VEC3I_OFST_ADDR);

		LOG_DBG("WPE_VEC3I_XSIZE:0x%x!\n",
			pWPEConfig->WPE_VEC3I_XSIZE);
		LOG_DBG("WPE_VEC3I_YSIZE:0x%x!\n",
			pWPEConfig->WPE_VEC3I_YSIZE);
		LOG_DBG("WPE_VEC3I_STRIDE:0x%x!\n",
			pWPEConfig->WPE_VEC3I_STRIDE);
		LOG_DBG("WPE_VEC3I_CON:0x%x!\n", pWPEConfig->WPE_VEC3I_CON);
		LOG_DBG("WPE_VEC3I_CON2:0x%x!\n", pWPEConfig->WPE_VEC3I_CON2);
		LOG_DBG("WPE_VEC3I_CON3:0x%x!\n", pWPEConfig->WPE_VEC3I_CON3);

		LOG_DBG("WPE_DMA_ERR_CTRL_REG :0x%x!\n",
			pWPEConfig->WPE_DMA_ERR_CTRL);
		LOG_DBG("WPE_WPEO_ERR_STAT:0x%x!\n",
			pWPEConfig->WPE_WPEO_ERR_STAT);
		LOG_DBG("WPE_MSKO_ERR_STAT:0x%x!\n",
			pWPEConfig->WPE_MSKO_ERR_STAT);
		LOG_DBG("WPE_VECI_ERR_STAT:0x%x!\n",
			pWPEConfig->WPE_VECI_ERR_STAT);
		LOG_DBG("WPE_VEC2I_ERR_STAT:0x%x!\n",
			pWPEConfig->WPE_VEC2I_ERR_STAT);
		LOG_DBG("WPE_VEC3I_ERR_STAT:0x%x!\n",
			pWPEConfig->WPE_VEC3I_ERR_STAT);
		LOG_DBG("WPE_DMA_DEBUG_ADDR:0x%x!\n",
			pWPEConfig->WPE_DMA_DEBUG_ADDR);

		LOG_DBG("WPE_DMA_DEBUG_SEL:0x%x!\n",
			pWPEConfig->WPE_DMA_DEBUG_SEL);

	}
#ifdef WPE_USE_GCE

#ifdef __WPE_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_begin("ConfigWPEHW");
#endif

	cmdqRecCreate(CMDQ_SCENARIO_KERNEL_CONFIG_GENERAL, &handle);
	/* CMDQ driver dispatches CMDQ HW thread */
	/*and HW thread's priority according to scenario */

	cmdqRecSetEngine(handle, engineFlag);

	cmdqRecReset(handle);

	/* Use command queue to write register */
	cmdqRecWrite(handle, WPE_CTL_MOD_EN_HW,
	pWPEConfig->WPE_CTL_MOD_EN, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_DMA_EN_HW,
		pWPEConfig->WPE_CTL_DMA_EN, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_CTL_CFG_HW,
		pWPEConfig->WPE_CTL_CFG, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_FMT_SEL_HW,
		pWPEConfig->WPE_CTL_FMT_SEL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_INT_EN_HW,
		pWPEConfig->WPE_CTL_INT_EN, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_CTL_INT_STATUS_HW,
		pWPEConfig->WPE_CTL_INT_STATUS, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_INT_STATUSX_HW,
		pWPEConfig->WPE_CTL_INT_STATUSX, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_TDR_TILE_HW,
		pWPEConfig->WPE_CTL_TDR_TILE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_TDR_DBG_STATUS_HW,
		pWPEConfig->WPE_CTL_TDR_DBG_STATUS, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_TDR_TCM_EN_HW,
		pWPEConfig->WPE_CTL_TDR_TCM_EN, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_SW_CTL_HW,
		pWPEConfig->WPE_CTL_SW_CTL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_SPARE0_HW,
		pWPEConfig->WPE_CTL_SPARE0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_SPARE1_HW,
		pWPEConfig->WPE_CTL_SPARE1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_SPARE2_HW,
		pWPEConfig->WPE_CTL_SPARE2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_DONE_SEL_HW,
		pWPEConfig->WPE_CTL_DONE_SEL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_DBG_SET_HW,
		pWPEConfig->WPE_CTL_DBG_SET, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_DBG_PORT_HW,
		pWPEConfig->WPE_CTL_DBG_PORT, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_DATE_CODE_HW,
		pWPEConfig->WPE_CTL_DATE_CODE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_PROJ_CODE_HW,
		pWPEConfig->WPE_CTL_PROJ_CODE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_WPE_DCM_DIS_HW,
		pWPEConfig->WPE_CTL_WPE_DCM_DIS, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_DMA_DCM_DIS_HW,
		pWPEConfig->WPE_CTL_DMA_DCM_DIS, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_WPE_DCM_STATUS_HW,
		pWPEConfig->WPE_CTL_WPE_DCM_STATUS, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_DMA_DCM_STATUS_HW,
		pWPEConfig->WPE_CTL_DMA_DCM_STATUS, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_WPE_REQ_STATUS_HW,
		pWPEConfig->WPE_CTL_WPE_REQ_STATUS, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_DMA_REQ_STATUS_HW,
		pWPEConfig->WPE_CTL_DMA_REQ_STATUS, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_WPE_RDY_STATUS_HW,
		pWPEConfig->WPE_CTL_WPE_RDY_STATUS, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CTL_DMA_RDY_STATUS_HW,
		pWPEConfig->WPE_CTL_DMA_RDY_STATUS, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_VGEN_CTL_HW,
		pWPEConfig->WPE_VGEN_CTL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VGEN_IN_IMG_HW,
		pWPEConfig->WPE_VGEN_IN_IMG, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VGEN_OUT_IMG_HW,
		pWPEConfig->WPE_VGEN_OUT_IMG, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VGEN_HORI_STEP_HW,
		pWPEConfig->WPE_VGEN_HORI_STEP, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VGEN_VERT_STEP_HW,
		pWPEConfig->WPE_VGEN_VERT_STEP, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VGEN_HORI_INT_OFST_HW,
		pWPEConfig->WPE_VGEN_HORI_INT_OFST, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VGEN_HORI_SUB_OFST_HW,
		pWPEConfig->WPE_VGEN_HORI_SUB_OFST, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VGEN_VERT_INT_OFST_HW,
		pWPEConfig->WPE_VGEN_VERT_INT_OFST, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VGEN_VERT_SUB_OFST_HW,
		pWPEConfig->WPE_VGEN_VERT_SUB_OFST, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_VGEN_POST_CTL_HW,
		pWPEConfig->WPE_VGEN_POST_CTL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VGEN_POST_COMP_X_HW,
		pWPEConfig->WPE_VGEN_POST_COMP_X, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VGEN_POST_COMP_Y_HW,
		pWPEConfig->WPE_VGEN_POST_COMP_Y, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VGEN_MAX_VEC_HW,
		pWPEConfig->WPE_VGEN_MAX_VEC, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VFIFO_CTL_HW,
		pWPEConfig->WPE_VFIFO_CTL, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_CFIFO_CTL_HW,
		pWPEConfig->WPE_CFIFO_CTL, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_RWCTL_CTL_HW,
		pWPEConfig->WPE_RWCTL_CTL, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_CACHI_SPECIAL_FUN_EN_HW,
		pWPEConfig->WPE_CACHI_SPECIAL_FUN_EN, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_C24_TILE_EDGE_HW,
		pWPEConfig->WPE_C24_TILE_EDGE, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_MDP_CROP_X_HW,
		pWPEConfig->WPE_MDP_CROP_X, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_MDP_CROP_Y_HW,
		pWPEConfig->WPE_MDP_CROP_Y, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_ISPCROP_CON1_HW,
		pWPEConfig->WPE_ISPCROP_CON1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ISPCROP_CON2_HW,
		pWPEConfig->WPE_ISPCROP_CON2, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_PSP_CTL_HW,
		pWPEConfig->WPE_PSP_CTL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_PSP2_CTL_HW,
		pWPEConfig->WPE_PSP2_CTL, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_ADDR_GEN_SOFT_RSTSTAT_0_HW,
		pWPEConfig->WPE_ADDR_GEN_SOFT_RSTSTAT_0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_BASE_ADDR_0_HW,
		pWPEConfig->WPE_ADDR_GEN_BASE_ADDR_0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_OFFSET_ADDR_0_HW,
		pWPEConfig->WPE_ADDR_GEN_OFFSET_ADDR_0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_STRIDE_0_HW,
		pWPEConfig->WPE_ADDR_GEN_STRIDE_0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CACHI_CON_0_HW,
		pWPEConfig->WPE_CACHI_CON_0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CACHI_CON2_0_HW,
		pWPEConfig->WPE_CACHI_CON2_0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CACHI_CON3_0_HW,
		pWPEConfig->WPE_CACHI_CON3_0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_ERR_CTRL_0_HW,
		pWPEConfig->WPE_ADDR_GEN_ERR_CTRL_0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_ERR_STAT_0_HW,
		pWPEConfig->WPE_ADDR_GEN_ERR_STAT_0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_RSV1_0_HW,
		pWPEConfig->WPE_ADDR_GEN_RSV1_0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_DEBUG_SEL_0_HW,
		pWPEConfig->WPE_ADDR_GEN_DEBUG_SEL_0, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_ADDR_GEN_SOFT_RSTSTAT_1_HW,
		pWPEConfig->WPE_ADDR_GEN_SOFT_RSTSTAT_1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_BASE_ADDR_1_HW,
		pWPEConfig->WPE_ADDR_GEN_BASE_ADDR_1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_OFFSET_ADDR_1_HW,
		pWPEConfig->WPE_ADDR_GEN_OFFSET_ADDR_1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_STRIDE_1_HW,
		pWPEConfig->WPE_ADDR_GEN_STRIDE_1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CACHI_CON_1_HW,
		pWPEConfig->WPE_CACHI_CON_1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CACHI_CON2_1_HW,
		pWPEConfig->WPE_CACHI_CON2_1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CACHI_CON3_1_HW,
		pWPEConfig->WPE_CACHI_CON3_1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_ERR_CTRL_1_HW,
		pWPEConfig->WPE_ADDR_GEN_ERR_CTRL_1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_ERR_STAT_1_HW,
		pWPEConfig->WPE_ADDR_GEN_ERR_STAT_1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_RSV1_1_HW,
		pWPEConfig->WPE_ADDR_GEN_RSV1_1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_DEBUG_SEL_1_HW,
		pWPEConfig->WPE_ADDR_GEN_DEBUG_SEL_1, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_ADDR_GEN_SOFT_RSTSTAT_2_HW,
		pWPEConfig->WPE_ADDR_GEN_SOFT_RSTSTAT_2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_BASE_ADDR_2_HW,
		pWPEConfig->WPE_ADDR_GEN_BASE_ADDR_2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_OFFSET_ADDR_2_HW,
		pWPEConfig->WPE_ADDR_GEN_OFFSET_ADDR_2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_STRIDE_2_HW,
		pWPEConfig->WPE_ADDR_GEN_STRIDE_2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CACHI_CON_2_HW,
		pWPEConfig->WPE_CACHI_CON_2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CACHI_CON2_2_HW,
		pWPEConfig->WPE_CACHI_CON2_2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CACHI_CON3_2_HW,
		pWPEConfig->WPE_CACHI_CON3_2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_ERR_CTRL_2_HW,
		pWPEConfig->WPE_ADDR_GEN_ERR_CTRL_2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_ERR_STAT_2_HW,
		pWPEConfig->WPE_ADDR_GEN_ERR_STAT_2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_RSV1_2_HW,
		pWPEConfig->WPE_ADDR_GEN_RSV1_2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_DEBUG_SEL_2_HW,
		pWPEConfig->WPE_ADDR_GEN_DEBUG_SEL_2, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_ADDR_GEN_SOFT_RSTSTAT_3_HW,
		pWPEConfig->WPE_ADDR_GEN_SOFT_RSTSTAT_3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_BASE_ADDR_3_HW,
		pWPEConfig->WPE_ADDR_GEN_BASE_ADDR_3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_OFFSET_ADDR_3_HW,
		pWPEConfig->WPE_ADDR_GEN_OFFSET_ADDR_3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_STRIDE_3_HW,
		pWPEConfig->WPE_ADDR_GEN_STRIDE_3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CACHI_CON_3_HW,
		pWPEConfig->WPE_CACHI_CON_3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CACHI_CON2_3_HW,
		pWPEConfig->WPE_CACHI_CON2_3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_CACHI_CON3_3_HW,
		pWPEConfig->WPE_CACHI_CON3_3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_ERR_CTRL_3_HW,
		pWPEConfig->WPE_ADDR_GEN_ERR_CTRL_3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_ERR_STAT_3_HW,
		pWPEConfig->WPE_ADDR_GEN_ERR_STAT_3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_RSV1_3_HW,
		pWPEConfig->WPE_ADDR_GEN_RSV1_3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_ADDR_GEN_DEBUG_SEL_3_HW,
		pWPEConfig->WPE_ADDR_GEN_DEBUG_SEL_3, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_DMA_SOFT_RSTSTAT_HW,
		pWPEConfig->WPE_DMA_SOFT_RSTSTAT, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_TDRI_BASE_ADDR_HW,
		pWPEConfig->WPE_TDRI_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_TDRI_OFST_ADDR_HW,
		pWPEConfig->WPE_TDRI_OFST_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_TDRI_XSIZE_HW,
		pWPEConfig->WPE_TDRI_XSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VERTICAL_FLIP_EN_HW,
		pWPEConfig->WPE_VERTICAL_FLIP_EN, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_DMA_SOFT_RESET_HW,
		pWPEConfig->WPE_DMA_SOFT_RESET, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_LAST_ULTRA_EN_HW,
		pWPEConfig->WPE_LAST_ULTRA_EN, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_SPECIAL_FUN_EN_HW,
		pWPEConfig->WPE_SPECIAL_FUN_EN, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_WPEO_BASE_ADDR_HW,
		pWPEConfig->WPE_WPEO_BASE_ADDR, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_WPEO_OFST_ADDR_HW,
		pWPEConfig->WPE_WPEO_OFST_ADDR, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_WPEO_XSIZE_HW,
		pWPEConfig->WPE_WPEO_XSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_WPEO_YSIZE_HW,
		pWPEConfig->WPE_WPEO_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_WPEO_STRIDE_HW,
		pWPEConfig->WPE_WPEO_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_WPEO_CON_HW,
		pWPEConfig->WPE_WPEO_CON, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_WPEO_CON2_HW,
		pWPEConfig->WPE_WPEO_CON2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_WPEO_CON3_HW,
		pWPEConfig->WPE_WPEO_CON3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_WPEO_CROP_HW,
		pWPEConfig->WPE_WPEO_CROP, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_MSKO_BASE_ADDR_HW,
		pWPEConfig->WPE_MSKO_BASE_ADDR, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_MSKO_OFST_ADDR_HW,
		pWPEConfig->WPE_MSKO_OFST_ADDR, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_MSKO_XSIZE_HW,
		pWPEConfig->WPE_MSKO_XSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_MSKO_YSIZE_HW,
		pWPEConfig->WPE_MSKO_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_MSKO_STRIDE_HW,
		pWPEConfig->WPE_MSKO_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_MSKO_CON_HW,
		pWPEConfig->WPE_MSKO_CON, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_MSKO_CON2_HW,
		pWPEConfig->WPE_MSKO_CON2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_MSKO_CON3_HW,
		pWPEConfig->WPE_MSKO_CON3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_MSKO_CROP_HW,
		pWPEConfig->WPE_MSKO_CROP, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_VECI_BASE_ADDR_HW,
		pWPEConfig->WPE_VECI_BASE_ADDR, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_VECI_OFST_ADDR_HW,
		pWPEConfig->WPE_VECI_OFST_ADDR, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_VECI_XSIZE_HW,
		pWPEConfig->WPE_VECI_XSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VECI_YSIZE_HW,
		pWPEConfig->WPE_VECI_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VECI_STRIDE_HW,
		pWPEConfig->WPE_VECI_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VECI_CON_HW,
		pWPEConfig->WPE_VECI_CON, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VECI_CON2_HW,
		pWPEConfig->WPE_VECI_CON2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VECI_CON3_HW,
		pWPEConfig->WPE_VECI_CON3, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_VEC2I_BASE_ADDR_HW,
		pWPEConfig->WPE_VEC2I_BASE_ADDR, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_VEC2I_OFST_ADDR_HW,
		pWPEConfig->WPE_VEC2I_OFST_ADDR, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_VEC2I_XSIZE_HW,
		pWPEConfig->WPE_VEC2I_XSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VEC2I_YSIZE_HW,
		pWPEConfig->WPE_VEC2I_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VEC2I_STRIDE_HW,
		pWPEConfig->WPE_VEC2I_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VEC2I_CON_HW,
		pWPEConfig->WPE_VEC2I_CON, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VEC2I_CON2_HW,
		pWPEConfig->WPE_VEC2I_CON2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VEC2I_CON3_HW,
		pWPEConfig->WPE_VEC2I_CON3, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_VEC3I_BASE_ADDR_HW,
		pWPEConfig->WPE_VEC3I_BASE_ADDR, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_VEC3I_OFST_ADDR_HW,
		pWPEConfig->WPE_VEC3I_OFST_ADDR, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_VEC3I_XSIZE_HW,
		pWPEConfig->WPE_VEC3I_XSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VEC3I_YSIZE_HW,
		pWPEConfig->WPE_VEC3I_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VEC3I_STRIDE_HW,
		pWPEConfig->WPE_VEC3I_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VEC3I_CON_HW,
		pWPEConfig->WPE_VEC3I_CON, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VEC3I_CON2_HW,
		pWPEConfig->WPE_VEC3I_CON2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VEC3I_CON3_HW,
		pWPEConfig->WPE_VEC3I_CON3, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_DMA_ERR_CTRL_HW,
		pWPEConfig->WPE_DMA_ERR_CTRL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_WPEO_ERR_STAT_HW,
		pWPEConfig->WPE_WPEO_ERR_STAT, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_MSKO_ERR_STAT_HW,
		pWPEConfig->WPE_MSKO_ERR_STAT, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VECI_ERR_STAT_HW,
		pWPEConfig->WPE_VECI_ERR_STAT, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VEC2I_ERR_STAT_HW,
		pWPEConfig->WPE_VEC2I_ERR_STAT, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_VEC3I_ERR_STAT_HW,
		pWPEConfig->WPE_VEC3I_ERR_STAT, CMDQ_REG_MASK);
	cmdqRecWrite(handle, WPE_DMA_DEBUG_ADDR_HW,
		pWPEConfig->WPE_DMA_DEBUG_ADDR, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_DMA_DEBUG_SEL_HW,
		pWPEConfig->WPE_DMA_DEBUG_SEL, CMDQ_REG_MASK);

	cmdqRecWrite(handle, WPE_WPE_START_HW, 0x1, CMDQ_REG_MASK);

	cmdqRecWait(handle, CMDQ_EVENT_WPE_A_EOF);

	cmdqRecWrite(handle, WPE_WPE_START_HW, 0x0, CMDQ_REG_MASK);

	/* non-blocking API, Please  use cmdqRecFlushAsync() */
	cmdqRecFlushAsync(handle);
	cmdqRecReset(handle);
	/* if you want to re-use the handle, please reset the handle */
	cmdqRecDestroy(handle);	/* recycle the memory */

#ifdef __WPE_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_end();
#endif

#else

#ifdef __WPE_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_begin("ConfigWPEHW");
#endif

	WPE_WR32(WPE_CTL_MOD_EN_REG, pWPEConfig->WPE_CTL_MOD_EN);
	WPE_WR32(WPE_CTL_DMA_EN_REG, pWPEConfig->WPE_CTL_DMA_EN);

	WPE_WR32(WPE_CTL_CFG_REG, pWPEConfig->WPE_CTL_CFG);
	WPE_WR32(WPE_CTL_FMT_SEL_REG, pWPEConfig->WPE_CTL_FMT_SEL);
	WPE_WR32(WPE_CTL_INT_EN_REG, pWPEConfig->WPE_CTL_INT_EN);

	WPE_WR32(WPE_CTL_INT_STATUS_REG, pWPEConfig->WPE_CTL_INT_STATUS);
	WPE_WR32(WPE_CTL_INT_STATUSX_REG, pWPEConfig->WPE_CTL_INT_STATUSX);
	WPE_WR32(WPE_CTL_TDR_TILE_REG, pWPEConfig->WPE_CTL_TDR_TILE);
	WPE_WR32(WPE_CTL_TDR_DBG_STATUS_REG,
		pWPEConfig->WPE_CTL_TDR_DBG_STATUS);
	WPE_WR32(WPE_CTL_TDR_TCM_EN_REG, pWPEConfig->WPE_CTL_TDR_TCM_EN);
	WPE_WR32(WPE_CTL_SW_CTL_REG, pWPEConfig->WPE_CTL_SW_CTL);
	WPE_WR32(WPE_CTL_SPARE0_REG, pWPEConfig->WPE_CTL_SPARE0);
	WPE_WR32(WPE_CTL_SPARE1_REG, pWPEConfig->WPE_CTL_SPARE1);
	WPE_WR32(WPE_CTL_SPARE2_REG, pWPEConfig->WPE_CTL_SPARE2);
	WPE_WR32(WPE_CTL_DONE_SEL_REG, pWPEConfig->WPE_CTL_DONE_SEL);
	WPE_WR32(WPE_CTL_DBG_SET_REG, pWPEConfig->WPE_CTL_DBG_SET);
	WPE_WR32(WPE_CTL_DBG_PORT_REG, pWPEConfig->WPE_CTL_DBG_PORT);
	WPE_WR32(WPE_CTL_DATE_CODE_REG, pWPEConfig->WPE_CTL_DATE_CODE);
	WPE_WR32(WPE_CTL_PROJ_CODE_REG, pWPEConfig->WPE_CTL_PROJ_CODE);
	WPE_WR32(WPE_CTL_WPE_DCM_DIS_REG, pWPEConfig->WPE_CTL_WPE_DCM_DIS);
	WPE_WR32(WPE_CTL_DMA_DCM_DIS_REG, pWPEConfig->WPE_CTL_DMA_DCM_DIS);
	WPE_WR32(WPE_CTL_WPE_DCM_STATUS_REG,
		pWPEConfig->WPE_CTL_WPE_DCM_STATUS);
	WPE_WR32(WPE_CTL_DMA_DCM_STATUS_REG,
		pWPEConfig->WPE_CTL_DMA_DCM_STATUS);
	WPE_WR32(WPE_CTL_WPE_REQ_STATUS_REG,
		pWPEConfig->WPE_CTL_WPE_REQ_STATUS);
	WPE_WR32(WPE_CTL_DMA_REQ_STATUS_REG,
		pWPEConfig->WPE_CTL_DMA_REQ_STATUS);
	WPE_WR32(WPE_CTL_WPE_RDY_STATUS_REG,
		pWPEConfig->WPE_CTL_WPE_RDY_STATUS);
	WPE_WR32(WPE_CTL_DMA_RDY_STATUS_REG,
		pWPEConfig->WPE_CTL_DMA_RDY_STATUS);

	WPE_WR32(WPE_VGEN_CTL_REG, pWPEConfig->WPE_VGEN_CTL);
	WPE_WR32(WPE_VGEN_IN_IMG_REG, pWPEConfig->WPE_VGEN_IN_IMG);
	WPE_WR32(WPE_VGEN_OUT_IMG_REG, pWPEConfig->WPE_VGEN_OUT_IMG);
	WPE_WR32(WPE_VGEN_HORI_STEP_REG, pWPEConfig->WPE_VGEN_HORI_STEP);
	WPE_WR32(WPE_VGEN_VERT_STEP_REG, pWPEConfig->WPE_VGEN_VERT_STEP);
	WPE_WR32(WPE_VGEN_HORI_INT_OFST_REG,
		pWPEConfig->WPE_VGEN_HORI_INT_OFST);
	WPE_WR32(WPE_VGEN_HORI_SUB_OFST_REG,
		pWPEConfig->WPE_VGEN_HORI_SUB_OFST);
	WPE_WR32(WPE_VGEN_VERT_INT_OFST_REG,
		pWPEConfig->WPE_VGEN_VERT_INT_OFST);
	WPE_WR32(WPE_VGEN_VERT_SUB_OFST_REG,
		pWPEConfig->WPE_VGEN_VERT_SUB_OFST);

	WPE_WR32(WPE_VGEN_POST_CTL_REG, pWPEConfig->WPE_VGEN_POST_CTL);
	WPE_WR32(WPE_VGEN_POST_COMP_X_REG,
		pWPEConfig->WPE_VGEN_POST_COMP_X);
	WPE_WR32(WPE_VGEN_POST_COMP_Y_REG,
		pWPEConfig->WPE_VGEN_POST_COMP_Y);
	WPE_WR32(WPE_VGEN_MAX_VEC_REG, pWPEConfig->WPE_VGEN_MAX_VEC);
	WPE_WR32(WPE_VFIFO_CTL_REG, pWPEConfig->WPE_VFIFO_CTL);

	WPE_WR32(WPE_CFIFO_CTL_REG, pWPEConfig->WPE_CFIFO_CTL);

	WPE_WR32(WPE_RWCTL_CTL_REG, pWPEConfig->WPE_RWCTL_CTL);

	WPE_WR32(WPE_CACHI_SPECIAL_FUN_EN_REG,
		pWPEConfig->WPE_CACHI_SPECIAL_FUN_EN);

	WPE_WR32(WPE_C24_TILE_EDGE_REG,
		pWPEConfig->WPE_C24_TILE_EDGE);

	WPE_WR32(WPE_MDP_CROP_X_REG, pWPEConfig->WPE_MDP_CROP_X);
	WPE_WR32(WPE_MDP_CROP_Y_REG, pWPEConfig->WPE_MDP_CROP_Y);

	WPE_WR32(WPE_ISPCROP_CON1_REG, pWPEConfig->WPE_ISPCROP_CON1);
	WPE_WR32(WPE_ISPCROP_CON2_REG, pWPEConfig->WPE_ISPCROP_CON2);

	WPE_WR32(WPE_PSP_CTL_REG, pWPEConfig->WPE_PSP_CTL);
	WPE_WR32(WPE_PSP2_CTL_REG, pWPEConfig->WPE_PSP2_CTL);

	WPE_WR32(WPE_ADDR_GEN_SOFT_RSTSTAT_0_REG,
		pWPEConfig->WPE_ADDR_GEN_SOFT_RSTSTAT_0);
	WPE_WR32(WPE_ADDR_GEN_BASE_ADDR_0_REG,
		pWPEConfig->WPE_ADDR_GEN_BASE_ADDR_0);
	WPE_WR32(WPE_ADDR_GEN_OFFSET_ADDR_0_REG,
		pWPEConfig->WPE_ADDR_GEN_OFFSET_ADDR_0);
	WPE_WR32(WPE_ADDR_GEN_STRIDE_0_REG,
		pWPEConfig->WPE_ADDR_GEN_STRIDE_0);
	WPE_WR32(WPE_CACHI_CON_0_REG, pWPEConfig->WPE_CACHI_CON_0);
	WPE_WR32(WPE_CACHI_CON2_0_REG, pWPEConfig->WPE_CACHI_CON2_0);
	WPE_WR32(WPE_CACHI_CON3_0_REG, pWPEConfig->WPE_CACHI_CON3_0);
	WPE_WR32(WPE_ADDR_GEN_ERR_CTRL_0_REG,
		pWPEConfig->WPE_ADDR_GEN_ERR_CTRL_0);
	WPE_WR32(WPE_ADDR_GEN_ERR_STAT_0_REG,
		pWPEConfig->WPE_ADDR_GEN_ERR_STAT_0);
	WPE_WR32(WPE_ADDR_GEN_RSV1_0_REG,
		pWPEConfig->WPE_ADDR_GEN_RSV1_0);
	WPE_WR32(WPE_ADDR_GEN_DEBUG_SEL_0_REG,
		pWPEConfig->WPE_ADDR_GEN_DEBUG_SEL_0);

	WPE_WR32(WPE_ADDR_GEN_SOFT_RSTSTAT_1_REG,
		pWPEConfig->WPE_ADDR_GEN_SOFT_RSTSTAT_1);
	WPE_WR32(WPE_ADDR_GEN_BASE_ADDR_1_REG,
		pWPEConfig->WPE_ADDR_GEN_BASE_ADDR_1);
	WPE_WR32(WPE_ADDR_GEN_OFFSET_ADDR_1_REG,
		pWPEConfig->WPE_ADDR_GEN_OFFSET_ADDR_1);
	WPE_WR32(WPE_ADDR_GEN_STRIDE_1_REG, pWPEConfig->WPE_ADDR_GEN_STRIDE_1);
	WPE_WR32(WPE_CACHI_CON_1_REG, pWPEConfig->WPE_CACHI_CON_1);
	WPE_WR32(WPE_CACHI_CON2_1_REG, pWPEConfig->WPE_CACHI_CON2_1);
	WPE_WR32(WPE_CACHI_CON3_1_REG, pWPEConfig->WPE_CACHI_CON3_1);
	WPE_WR32(WPE_ADDR_GEN_ERR_CTRL_1_REG,
		pWPEConfig->WPE_ADDR_GEN_ERR_CTRL_1);
	WPE_WR32(WPE_ADDR_GEN_ERR_STAT_1_REG,
		pWPEConfig->WPE_ADDR_GEN_ERR_STAT_1);
	WPE_WR32(WPE_ADDR_GEN_RSV1_1_REG,
		pWPEConfig->WPE_ADDR_GEN_RSV1_1);
	WPE_WR32(WPE_ADDR_GEN_DEBUG_SEL_1_REG,
		pWPEConfig->WPE_ADDR_GEN_DEBUG_SEL_1);

	WPE_WR32(WPE_ADDR_GEN_SOFT_RSTSTAT_2_REG,
		pWPEConfig->WPE_ADDR_GEN_SOFT_RSTSTAT_2);
	WPE_WR32(WPE_ADDR_GEN_BASE_ADDR_2_REG,
		pWPEConfig->WPE_ADDR_GEN_BASE_ADDR_2);
	WPE_WR32(WPE_ADDR_GEN_OFFSET_ADDR_2_REG,
		pWPEConfig->WPE_ADDR_GEN_OFFSET_ADDR_2);
	WPE_WR32(WPE_ADDR_GEN_STRIDE_2_REG,
		pWPEConfig->WPE_ADDR_GEN_STRIDE_2);
	WPE_WR32(WPE_CACHI_CON_2_REG, pWPEConfig->WPE_CACHI_CON_2);
	WPE_WR32(WPE_CACHI_CON2_2_REG, pWPEConfig->WPE_CACHI_CON2_2);
	WPE_WR32(WPE_CACHI_CON3_2_REG, pWPEConfig->WPE_CACHI_CON3_2);
	WPE_WR32(WPE_ADDR_GEN_ERR_CTRL_2_REG,
		pWPEConfig->WPE_ADDR_GEN_ERR_CTRL_2);
	WPE_WR32(WPE_ADDR_GEN_ERR_STAT_2_REG,
		pWPEConfig->WPE_ADDR_GEN_ERR_STAT_2);
	WPE_WR32(WPE_ADDR_GEN_RSV1_2_REG, pWPEConfig->WPE_ADDR_GEN_RSV1_2);
	WPE_WR32(WPE_ADDR_GEN_DEBUG_SEL_2_REG,
		pWPEConfig->WPE_ADDR_GEN_DEBUG_SEL_2);

	WPE_WR32(WPE_ADDR_GEN_SOFT_RSTSTAT_3_REG,
		pWPEConfig->WPE_ADDR_GEN_SOFT_RSTSTAT_3);
	WPE_WR32(WPE_ADDR_GEN_BASE_ADDR_3_REG,
		pWPEConfig->WPE_ADDR_GEN_BASE_ADDR_3);
	WPE_WR32(WPE_ADDR_GEN_OFFSET_ADDR_3_REG,
		pWPEConfig->WPE_ADDR_GEN_OFFSET_ADDR_3);
	WPE_WR32(WPE_ADDR_GEN_STRIDE_3_REG,
		pWPEConfig->WPE_ADDR_GEN_STRIDE_3);
	WPE_WR32(WPE_CACHI_CON_3_REG, pWPEConfig->WPE_CACHI_CON_3);
	WPE_WR32(WPE_CACHI_CON2_3_REG, pWPEConfig->WPE_CACHI_CON2_3);
	WPE_WR32(WPE_CACHI_CON3_3_REG, pWPEConfig->WPE_CACHI_CON3_3);
	WPE_WR32(WPE_ADDR_GEN_ERR_CTRL_3_REG,
		pWPEConfig->WPE_ADDR_GEN_ERR_CTRL_3);
	WPE_WR32(WPE_ADDR_GEN_ERR_STAT_3_REG,
		pWPEConfig->WPE_ADDR_GEN_ERR_STAT_3);
	WPE_WR32(WPE_ADDR_GEN_RSV1_3_REG, pWPEConfig->WPE_ADDR_GEN_RSV1_3);
	WPE_WR32(WPE_ADDR_GEN_DEBUG_SEL_3_REG,
		pWPEConfig->WPE_ADDR_GEN_DEBUG_SEL_3);

	WPE_WR32(WPE_DMA_SOFT_RSTSTAT_REG, pWPEConfig->WPE_DMA_SOFT_RSTSTAT);
	WPE_WR32(WPE_TDRI_BASE_ADDR_REG, pWPEConfig->WPE_TDRI_BASE_ADDR);
	WPE_WR32(WPE_TDRI_OFST_ADDR_REG, pWPEConfig->WPE_TDRI_OFST_ADDR);
	WPE_WR32(WPE_TDRI_XSIZE_REG, pWPEConfig->WPE_TDRI_XSIZE);
	WPE_WR32(WPE_VERTICAL_FLIP_EN_REG, pWPEConfig->WPE_VERTICAL_FLIP_EN);
	WPE_WR32(WPE_DMA_SOFT_RESET_REG, pWPEConfig->WPE_DMA_SOFT_RESET);
	WPE_WR32(WPE_LAST_ULTRA_EN_REG, pWPEConfig->WPE_LAST_ULTRA_EN);
	WPE_WR32(WPE_SPECIAL_FUN_EN_REG, pWPEConfig->WPE_SPECIAL_FUN_EN);

	WPE_WR32(WPE_WPEO_BASE_ADDR_REG, pWPEConfig->WPE_WPEO_BASE_ADDR);

	WPE_WR32(WPE_WPEO_OFST_ADDR_REG, pWPEConfig->WPE_WPEO_OFST_ADDR);

	WPE_WR32(WPE_WPEO_XSIZE_REG, pWPEConfig->WPE_WPEO_XSIZE);
	WPE_WR32(WPE_WPEO_YSIZE_REG, pWPEConfig->WPE_WPEO_YSIZE);
	WPE_WR32(WPE_WPEO_STRIDE_REG, pWPEConfig->WPE_WPEO_STRIDE);
	WPE_WR32(WPE_WPEO_CON_REG, pWPEConfig->WPE_WPEO_CON);
	WPE_WR32(WPE_WPEO_CON2_REG, pWPEConfig->WPE_WPEO_CON2);
	WPE_WR32(WPE_WPEO_CON3_REG, pWPEConfig->WPE_WPEO_CON3);
	WPE_WR32(WPE_WPEO_CROP_REG, pWPEConfig->WPE_WPEO_CROP);

	WPE_WR32(WPE_MSKO_BASE_ADDR_REG, pWPEConfig->WPE_MSKO_BASE_ADDR);

	WPE_WR32(WPE_MSKO_OFST_ADDR_REG, pWPEConfig->WPE_MSKO_OFST_ADDR);

	WPE_WR32(WPE_MSKO_XSIZE_REG, pWPEConfig->WPE_MSKO_XSIZE);
	WPE_WR32(WPE_MSKO_YSIZE_REG, pWPEConfig->WPE_MSKO_YSIZE);
	WPE_WR32(WPE_MSKO_STRIDE_REG, pWPEConfig->WPE_MSKO_STRIDE);
	WPE_WR32(WPE_MSKO_CON_REG, pWPEConfig->WPE_MSKO_CON);
	WPE_WR32(WPE_MSKO_CON2_REG, pWPEConfig->WPE_MSKO_CON2);
	WPE_WR32(WPE_MSKO_CON3_REG, pWPEConfig->WPE_MSKO_CON3);
	WPE_WR32(WPE_MSKO_CROP_REG, pWPEConfig->WPE_MSKO_CROP);

	WPE_WR32(WPE_VECI_BASE_ADDR_REG, pWPEConfig->WPE_VECI_BASE_ADDR);

	WPE_WR32(WPE_VECI_OFST_ADDR_REG, pWPEConfig->WPE_VECI_OFST_ADDR);

	WPE_WR32(WPE_VECI_XSIZE_REG, pWPEConfig->WPE_VECI_XSIZE);
	WPE_WR32(WPE_VECI_YSIZE_REG, pWPEConfig->WPE_VECI_YSIZE);
	WPE_WR32(WPE_VECI_STRIDE_REG, pWPEConfig->WPE_VECI_STRIDE);
	WPE_WR32(WPE_VECI_CON_REG, pWPEConfig->WPE_VECI_CON);
	WPE_WR32(WPE_VECI_CON2_REG, pWPEConfig->WPE_VECI_CON2);
	WPE_WR32(WPE_VECI_CON3_REG, pWPEConfig->WPE_VECI_CON3);

	WPE_WR32(WPE_VEC2I_BASE_ADDR_REG, pWPEConfig->WPE_VEC2I_BASE_ADDR);

	WPE_WR32(WPE_VEC2I_OFST_ADDR_REG, pWPEConfig->WPE_VEC2I_OFST_ADDR);

	WPE_WR32(WPE_VEC2I_XSIZE_REG, pWPEConfig->WPE_VEC2I_XSIZE);
	WPE_WR32(WPE_VEC2I_YSIZE_REG, pWPEConfig->WPE_VEC2I_YSIZE);
	WPE_WR32(WPE_VEC2I_STRIDE_REG, pWPEConfig->WPE_VEC2I_STRIDE);
	WPE_WR32(WPE_VEC2I_CON_REG, pWPEConfig->WPE_VEC2I_CON);
	WPE_WR32(WPE_VEC2I_CON2_REG, pWPEConfig->WPE_VEC2I_CON2);
	WPE_WR32(WPE_VEC2I_CON3_REG, pWPEConfig->WPE_VEC2I_CON3);

	WPE_WR32(WPE_VEC3I_BASE_ADDR_REG, pWPEConfig->WPE_VEC3I_BASE_ADDR);

	WPE_WR32(WPE_VEC3I_OFST_ADDR_REG, pWPEConfig->WPE_VEC3I_OFST_ADDR);

	WPE_WR32(WPE_VEC3I_XSIZE_REG, pWPEConfig->WPE_VEC3I_XSIZE);
	WPE_WR32(WPE_VEC3I_YSIZE_REG, pWPEConfig->WPE_VEC3I_YSIZE);
	WPE_WR32(WPE_VEC3I_STRIDE_REG, pWPEConfig->WPE_VEC3I_STRIDE);
	WPE_WR32(WPE_VEC3I_CON_REG, pWPEConfig->WPE_VEC3I_CON);
	WPE_WR32(WPE_VEC3I_CON2_REG, pWPEConfig->WPE_VEC3I_CON2);
	WPE_WR32(WPE_VEC3I_CON3_REG, pWPEConfig->WPE_VEC3I_CON3);

	WPE_WR32(WPE_DMA_ERR_CTRL_REG, pWPEConfig->WPE_DMA_ERR_CTRL);
	WPE_WR32(WPE_WPEO_ERR_STAT_REG, pWPEConfig->WPE_WPEO_ERR_STAT);
	WPE_WR32(WPE_MSKO_ERR_STAT_REG, pWPEConfig->WPE_MSKO_ERR_STAT);
	WPE_WR32(WPE_VECI_ERR_STAT_REG, pWPEConfig->WPE_VECI_ERR_STAT);
	WPE_WR32(WPE_VEC2I_ERR_STAT_REG, pWPEConfig->WPE_VEC2I_ERR_STAT);
	WPE_WR32(WPE_VEC3I_ERR_STAT_REG, pWPEConfig->WPE_VEC3I_ERR_STAT);
	WPE_WR32(WPE_DMA_DEBUG_ADDR_REG, pWPEConfig->WPE_DMA_DEBUG_ADDR);

	WPE_WR32(WPE_DMA_DEBUG_SEL_REG, pWPEConfig->WPE_DMA_DEBUG_SEL);
	WPE_WR32(WPE_WPE_START_REG, 0x1);


#ifdef __WPE_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_end();
#endif

#endif
	return 0;
}
#else
{
	return 0;
}
#endif

#define WPE_IS_BUSY    0x2

#ifndef WPE_USE_GCE

static bool Check_WPE_Is_Busy(void)
{
	unsigned int Ctrl_Fsm;
	unsigned int Wpe_Start;

	/* Ctrl_Fsm = WPE_RD32(RSC_DBG_INFO_00_REG);*/
	/* Daniel add need check with SY */
	Wpe_Start = WPE_RD32(WPE_IRQ_REG);
	if ((WPE_IS_BUSY == (Ctrl_Fsm & WPE_IS_BUSY)) ||
		(WPE_START == (WPE_Start & WPE_START)))
		return MTRUE;

	return MFALSE;
}
#endif


/*
 *
 */
static signed int WPE_DumpReg(void)
{
	signed int  Ret = 0;
	unsigned int  i, j;
	/*  */
	LOG_INF("- E.");
	/*  */
	LOG_INF("WPE Config Info\n");
	/* WPE Config0 */
	LOG_INF("[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_WPE_START_HW),
		(unsigned int)WPE_RD32(WPE_WPE_START_REG),
		(unsigned int)(WPE_CTL_MOD_EN_HW),
		(unsigned int)WPE_RD32(WPE_CTL_MOD_EN_REG),
		(unsigned int)(WPE_CTL_DMA_EN_HW),
		(unsigned int)WPE_RD32(WPE_CTL_DMA_EN_REG),
		(unsigned int)(WPE_CTL_CFG_HW),
		(unsigned int)WPE_RD32(WPE_CTL_CFG_REG));
	LOG_INF("[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_CTL_FMT_SEL_HW),
		(unsigned int)WPE_RD32(WPE_CTL_FMT_SEL_REG),
		(unsigned int)(WPE_CTL_INT_EN_HW),
		(unsigned int)WPE_RD32(WPE_CTL_INT_EN_REG),
		(unsigned int)(WPE_CTL_INT_STATUS_HW),
		(unsigned int)WPE_RD32(WPE_CTL_INT_STATUS_REG),
		(unsigned int)(WPE_CTL_INT_STATUSX_HW),
		(unsigned int)WPE_RD32(WPE_CTL_INT_STATUSX_REG));
	LOG_INF(
		"[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X][0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_CTL_TDR_TILE_HW),
		(unsigned int)WPE_RD32(WPE_CTL_TDR_TILE_REG),
		(unsigned int)(WPE_CTL_TDR_DBG_STATUS_HW),
		(unsigned int)WPE_RD32(WPE_CTL_TDR_DBG_STATUS_REG),
		(unsigned int)(WPE_CTL_WPE_REQ_STATUS_HW),
		(unsigned int)WPE_RD32(WPE_CTL_WPE_REQ_STATUS_REG),
		(unsigned int)(WPE_CTL_DMA_REQ_STATUS_HW),
		(unsigned int)WPE_RD32(WPE_CTL_DMA_REQ_STATUS_REG),
		(unsigned int)(WPE_CTL_WPE_RDY_STATUS_HW),
		(unsigned int)WPE_RD32(WPE_CTL_WPE_RDY_STATUS_REG),
		(unsigned int)(WPE_CTL_DMA_RDY_STATUS_HW),
		(unsigned int)WPE_RD32(WPE_CTL_DMA_RDY_STATUS_REG));
	LOG_INF("[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_VGEN_CTL_HW),
		(unsigned int)WPE_RD32(WPE_VGEN_CTL_REG),
		(unsigned int)(WPE_VGEN_IN_IMG_HW),
		(unsigned int)WPE_RD32(WPE_VGEN_IN_IMG_REG),
		(unsigned int)(WPE_VGEN_OUT_IMG_HW),
		(unsigned int)WPE_RD32(WPE_VGEN_OUT_IMG_REG),
		(unsigned int)(WPE_VGEN_HORI_STEP_HW),
		(unsigned int)WPE_RD32(WPE_VGEN_HORI_STEP_REG));
	LOG_INF("[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_VGEN_VERT_STEP_HW),
		(unsigned int)WPE_RD32(WPE_VGEN_VERT_STEP_REG),
		(unsigned int)(WPE_C24_TILE_EDGE_HW),
		(unsigned int)WPE_RD32(WPE_C24_TILE_EDGE_REG),
		(unsigned int)(WPE_MDP_CROP_X_HW),
		(unsigned int)WPE_RD32(WPE_MDP_CROP_X_REG),
		(unsigned int)(WPE_MDP_CROP_Y_HW),
		(unsigned int)WPE_RD32(WPE_MDP_CROP_Y_REG));
	LOG_INF("[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_ISPCROP_CON1_HW),
		(unsigned int)WPE_RD32(WPE_ISPCROP_CON1_REG),
		(unsigned int)(WPE_ISPCROP_CON2_HW),
		(unsigned int)WPE_RD32(WPE_ISPCROP_CON2_REG),
		(unsigned int)(WPE_PSP_CTL_HW),
		(unsigned int)WPE_RD32(WPE_PSP_CTL_REG),
		(unsigned int)(WPE_PSP2_CTL_HW),
		(unsigned int)WPE_RD32(WPE_PSP2_CTL_REG));
	LOG_INF("[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_ADDR_GEN_SOFT_RSTSTAT_0_HW),
		(unsigned int)WPE_RD32(WPE_ADDR_GEN_SOFT_RSTSTAT_0_REG),
		(unsigned int)(WPE_ADDR_GEN_BASE_ADDR_0_HW),
		(unsigned int)WPE_RD32(WPE_ADDR_GEN_BASE_ADDR_0_REG),
		(unsigned int)(WPE_ADDR_GEN_OFFSET_ADDR_0_HW),
		(unsigned int)WPE_RD32(WPE_ADDR_GEN_OFFSET_ADDR_0_REG),
		(unsigned int)(WPE_ADDR_GEN_STRIDE_0_HW),
		(unsigned int)WPE_RD32(WPE_ADDR_GEN_STRIDE_0_REG));
	LOG_INF(
		"[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_WPEO_BASE_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_WPEO_BASE_ADDR_REG),
		(unsigned int)(WPE_WPEO_OFST_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_WPEO_OFST_ADDR_REG),
		(unsigned int)(WPE_WPEO_XSIZE_HW),
		(unsigned int)WPE_RD32(WPE_WPEO_XSIZE_REG),
		(unsigned int)(WPE_WPEO_YSIZE_HW),
		(unsigned int)WPE_RD32(WPE_WPEO_YSIZE_REG),
		(unsigned int)(WPE_WPEO_STRIDE_HW),
		(unsigned int)WPE_RD32(WPE_WPEO_STRIDE_REG));
	LOG_INF(
		"[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X][ 0x%08X %08X]\n",
		(unsigned int)(WPE_VECI_BASE_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_VECI_BASE_ADDR_REG),
		(unsigned int)(WPE_VECI_OFST_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_VECI_OFST_ADDR_REG),
		(unsigned int)(WPE_VECI_XSIZE_HW),
		(unsigned int)WPE_RD32(WPE_VECI_XSIZE_REG),
		(unsigned int)(WPE_VECI_YSIZE_HW),
		(unsigned int)WPE_RD32(WPE_VECI_YSIZE_REG),
		(unsigned int)(WPE_VECI_STRIDE_HW),
		(unsigned int)WPE_RD32(WPE_VECI_STRIDE_REG));
	LOG_INF(
		"[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_VEC2I_BASE_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_VEC2I_BASE_ADDR_REG),
		(unsigned int)(WPE_VEC2I_OFST_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_VEC2I_OFST_ADDR_REG),
		(unsigned int)(WPE_VEC2I_XSIZE_HW),
		(unsigned int)WPE_RD32(WPE_VEC2I_XSIZE_REG),
		(unsigned int)(WPE_VEC2I_YSIZE_HW),
		(unsigned int)WPE_RD32(WPE_VEC2I_YSIZE_REG),
		(unsigned int)(WPE_VEC2I_STRIDE_HW),
		(unsigned int)WPE_RD32(WPE_VEC2I_STRIDE_REG));
	LOG_INF(
		"[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_VEC3I_BASE_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_VEC3I_BASE_ADDR_REG),
		(unsigned int)(WPE_VEC3I_OFST_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_VEC3I_OFST_ADDR_REG),
		(unsigned int)(WPE_VEC3I_XSIZE_HW),
		(unsigned int)WPE_RD32(WPE_VEC3I_XSIZE_REG),
		(unsigned int)(WPE_VEC3I_YSIZE_HW),
		(unsigned int)WPE_RD32(WPE_VEC3I_YSIZE_REG),
		(unsigned int)(WPE_VEC3I_STRIDE_HW),
		(unsigned int)WPE_RD32(WPE_VEC3I_STRIDE_REG));

	LOG_INF(
		"[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_B_WPE_START_HW),
		(unsigned int)WPE_RD32(WPE_B_WPE_START_REG),
		(unsigned int)(WPE_B_CTL_MOD_EN_HW),
		(unsigned int)WPE_RD32(WPE_B_CTL_MOD_EN_REG),
		(unsigned int)(WPE_B_CTL_DMA_EN_HW),
		(unsigned int)WPE_RD32(WPE_B_CTL_DMA_EN_REG),
		(unsigned int)(WPE_B_CTL_CFG_HW),
		(unsigned int)WPE_RD32(WPE_B_CTL_CFG_REG));
	LOG_INF("[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_B_CTL_FMT_SEL_HW),
		(unsigned int)WPE_RD32(WPE_B_CTL_FMT_SEL_REG),
		(unsigned int)(WPE_B_CTL_INT_EN_HW),
		(unsigned int)WPE_RD32(WPE_B_CTL_INT_EN_REG),
		(unsigned int)(WPE_B_CTL_INT_STATUS_HW),
		(unsigned int)WPE_RD32(WPE_B_CTL_INT_STATUS_REG),
		(unsigned int)(WPE_B_CTL_INT_STATUSX_HW),
		(unsigned int)WPE_RD32(WPE_B_CTL_INT_STATUSX_REG));
	LOG_INF(
		"[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X][0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_B_CTL_TDR_TILE_HW),
		(unsigned int)WPE_RD32(WPE_B_CTL_TDR_TILE_REG),
		(unsigned int)(WPE_B_CTL_TDR_DBG_STATUS_HW),
		(unsigned int)WPE_RD32(WPE_B_CTL_TDR_DBG_STATUS_REG),
		(unsigned int)(WPE_B_CTL_WPE_REQ_STATUS_HW),
		(unsigned int)WPE_RD32(WPE_B_CTL_WPE_REQ_STATUS_REG),
		(unsigned int)(WPE_B_CTL_DMA_REQ_STATUS_HW),
		(unsigned int)WPE_RD32(WPE_B_CTL_DMA_REQ_STATUS_REG),
		(unsigned int)(WPE_B_CTL_WPE_RDY_STATUS_HW),
		(unsigned int)WPE_RD32(WPE_B_CTL_WPE_RDY_STATUS_REG),
		(unsigned int)(WPE_B_CTL_DMA_RDY_STATUS_HW),
		(unsigned int)WPE_RD32(WPE_B_CTL_DMA_RDY_STATUS_REG));
	LOG_INF("[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_B_VGEN_CTL_HW),
		(unsigned int)WPE_RD32(WPE_B_VGEN_CTL_REG),
		(unsigned int)(WPE_B_VGEN_IN_IMG_HW),
		(unsigned int)WPE_RD32(WPE_B_VGEN_IN_IMG_REG),
		(unsigned int)(WPE_B_VGEN_OUT_IMG_HW),
		(unsigned int)WPE_RD32(WPE_B_VGEN_OUT_IMG_REG),
		(unsigned int)(WPE_B_VGEN_HORI_STEP_HW),
		(unsigned int)WPE_RD32(WPE_B_VGEN_HORI_STEP_REG));
	LOG_INF("[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_B_VGEN_VERT_STEP_HW),
		(unsigned int)WPE_RD32(WPE_B_VGEN_VERT_STEP_REG),
		(unsigned int)(WPE_B_C24_TILE_EDGE_HW),
		(unsigned int)WPE_RD32(WPE_B_C24_TILE_EDGE_REG),
		(unsigned int)(WPE_B_MDP_CROP_X_HW),
		(unsigned int)WPE_RD32(WPE_B_MDP_CROP_X_REG),
		(unsigned int)(WPE_B_MDP_CROP_Y_HW),
		(unsigned int)WPE_RD32(WPE_B_MDP_CROP_Y_REG));
	LOG_INF("[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_B_ISPCROP_CON1_HW),
		(unsigned int)WPE_RD32(WPE_B_ISPCROP_CON1_REG),
		(unsigned int)(WPE_B_ISPCROP_CON2_HW),
		(unsigned int)WPE_RD32(WPE_B_ISPCROP_CON2_REG),
		(unsigned int)(WPE_B_PSP_CTL_HW),
		(unsigned int)WPE_RD32(WPE_B_PSP_CTL_REG),
		(unsigned int)(WPE_B_PSP2_CTL_HW),
		(unsigned int)WPE_RD32(WPE_B_PSP2_CTL_REG));
	LOG_INF("[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_B_ADDR_GEN_SOFT_RSTSTAT_0_HW),
		(unsigned int)WPE_RD32(WPE_B_ADDR_GEN_SOFT_RSTSTAT_0_REG),
		(unsigned int)(WPE_B_ADDR_GEN_BASE_ADDR_0_HW),
		(unsigned int)WPE_RD32(WPE_B_ADDR_GEN_BASE_ADDR_0_REG),
		(unsigned int)(WPE_B_ADDR_GEN_OFFSET_ADDR_0_HW),
		(unsigned int)WPE_RD32(WPE_B_ADDR_GEN_OFFSET_ADDR_0_REG),
		(unsigned int)(WPE_B_ADDR_GEN_STRIDE_0_HW),
		(unsigned int)WPE_RD32(WPE_B_ADDR_GEN_STRIDE_0_REG));
	LOG_INF(
		"[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_B_WPEO_BASE_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_B_WPEO_BASE_ADDR_REG),
		(unsigned int)(WPE_B_WPEO_OFST_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_B_WPEO_OFST_ADDR_REG),
		(unsigned int)(WPE_B_WPEO_XSIZE_HW),
		(unsigned int)WPE_RD32(WPE_B_WPEO_XSIZE_REG),
		(unsigned int)(WPE_B_WPEO_YSIZE_HW),
		(unsigned int)WPE_RD32(WPE_B_WPEO_YSIZE_REG),
		(unsigned int)(WPE_B_WPEO_STRIDE_HW),
		(unsigned int)WPE_RD32(WPE_B_WPEO_STRIDE_REG));
	LOG_INF(
		"[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X][ 0x%08X %08X]\n",
		(unsigned int)(WPE_B_VECI_BASE_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_B_VECI_BASE_ADDR_REG),
		(unsigned int)(WPE_B_VECI_OFST_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_B_VECI_OFST_ADDR_REG),
		(unsigned int)(WPE_B_VECI_XSIZE_HW),
		(unsigned int)WPE_RD32(WPE_B_VECI_XSIZE_REG),
		(unsigned int)(WPE_B_VECI_YSIZE_HW),
		(unsigned int)WPE_RD32(WPE_B_VECI_YSIZE_REG),
		(unsigned int)(WPE_B_VECI_STRIDE_HW),
		(unsigned int)WPE_RD32(WPE_B_VECI_STRIDE_REG));
	LOG_INF(
		"[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_B_VEC2I_BASE_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_B_VEC2I_BASE_ADDR_REG),
		(unsigned int)(WPE_B_VEC2I_OFST_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_B_VEC2I_OFST_ADDR_REG),
		(unsigned int)(WPE_B_VEC2I_XSIZE_HW),
		(unsigned int)WPE_RD32(WPE_B_VEC2I_XSIZE_REG),
		(unsigned int)(WPE_B_VEC2I_YSIZE_HW),
		(unsigned int)WPE_RD32(WPE_B_VEC2I_YSIZE_REG),
		(unsigned int)(WPE_B_VEC2I_STRIDE_HW),
		(unsigned int)WPE_RD32(WPE_B_VEC2I_STRIDE_REG));
	LOG_INF(
		"[0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X] [0x%08X %08X]\n",
		(unsigned int)(WPE_B_VEC3I_BASE_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_B_VEC3I_BASE_ADDR_REG),
		(unsigned int)(WPE_B_VEC3I_OFST_ADDR_HW),
		(unsigned int)WPE_RD32(WPE_B_VEC3I_OFST_ADDR_REG),
		(unsigned int)(WPE_B_VEC3I_XSIZE_HW),
		(unsigned int)WPE_RD32(WPE_B_VEC3I_XSIZE_REG),
		(unsigned int)(WPE_B_VEC3I_YSIZE_HW),
		(unsigned int)WPE_RD32(WPE_B_VEC3I_YSIZE_REG),
		(unsigned int)(WPE_B_VEC3I_STRIDE_HW),
		(unsigned int)WPE_RD32(WPE_B_VEC3I_STRIDE_REG));

	for (i = 0; i < _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_; i++) {
		LOG_INF(
			"WPE Req:State:%d, procID:0x%08X, callerID:0x%08X, enqReqNum:%d, FraWRIdx:%d, FraRDIdx:%d\n",
		     g_WPE_ReqRing.WPEReq_Struct[i].State,
		     g_WPE_ReqRing.WPEReq_Struct[i].processID,
		     g_WPE_ReqRing.WPEReq_Struct[i].callerID,
		     g_WPE_ReqRing.WPEReq_Struct[i].enqueReqNum,
		     g_WPE_ReqRing.WPEReq_Struct[i].FrameWRIdx,
		     g_WPE_ReqRing.WPEReq_Struct[i].FrameRDIdx);

		for (j = 0; j < _SUPPORT_MAX_WPE_FRAME_REQUEST_;) {
			LOG_INF(
				"WPE:FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d\n",
			     j,
			     g_WPE_ReqRing.WPEReq_Struct[i].WpeFrameStatus[j],
			     j + 1,
			     g_WPE_ReqRing.WPEReq_Struct[
			     i].WpeFrameStatus[j + 1],
			     j + 2,
			     g_WPE_ReqRing.WPEReq_Struct[
			     i].WpeFrameStatus[j + 2],
			     j + 3,
			     g_WPE_ReqRing.WPEReq_Struct[
			     i].WpeFrameStatus[j + 3]);
			j = j + 4;
		}

	}

	LOG_INF("- X.");
	/*  */
	return Ret;
}
#ifndef __WPE_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
static inline void WPE_Prepare_ccf_clock(void)
{
	int ret;
	/* must keep this clk open order: */
	/*CG_SCP_SYS_DIS-> CG_MM_SMI_COMMON ->*/
	/*CG_SCP_SYS_ISP -> WPE clk */
	smi_clk_prepare(SMI_LARB_IMGSYS1, "camera_wpe", 1);
	ret = clk_prepare(wpe_clk.CG_IMGSYS_WPE_A);
	if (ret)
		LOG_ERR("cannot prepare CG_IMGSYS_WPE_A clock\n");
	ret = clk_prepare(wpe_clk.CG_IMGSYS_WPE_B);
	if (ret)
		LOG_ERR("cannot prepare CG_IMGSYS_WPE_B clock\n");
}

static inline void WPE_Enable_ccf_clock(void)
{
	int ret;
	/* must keep this clk open order: */
	/*CG_SCP_SYS_DIS-> CG_MM_SMI_COMMON*/
	/*-> CG_SCP_SYS_ISP -> WPE  clk */

	smi_clk_enable(SMI_LARB_IMGSYS1, "camera_wpe", 1);
	ret = clk_enable(wpe_clk.CG_IMGSYS_WPE_A);
	if (ret)
		LOG_ERR("cannot prepare CG_IMGSYS_WPE_A clock\n");
	ret = clk_enable(wpe_clk.CG_IMGSYS_WPE_B);
	if (ret)
		LOG_ERR("cannot prepare CG_IMGSYS_WPE_B clock\n");
}

static inline void WPE_Prepare_Enable_ccf_clock(void)
{
	int ret;
	/* must keep this clk open order:*/
	/*CG_SCP_SYS_DIS-> CG_MM_SMI_COMMON ->*/
	/*CG_SCP_SYS_ISP -> WPE clk */
	smi_bus_enable(SMI_LARB_IMGSYS1, "camera_wpe");
	ret = clk_prepare_enable(wpe_clk.CG_IMGSYS_WPE_A);
	if (ret)
		LOG_ERR("cannot prepare CG_IMGSYS_WPE_A clock\n");
	ret = clk_prepare_enable(wpe_clk.CG_IMGSYS_WPE_B);
	if (ret)
		LOG_ERR("cannot prepare CG_IMGSYS_WPE_B clock\n");
}

static inline void WPE_Unprepare_ccf_clock(void)
{
	/* must keep this clk close order:*/
	/*WPE clk -> CG_SCP_SYS_ISP ->*/
	/*CG_MM_SMI_COMMON -> CG_SCP_SYS_DIS */
	clk_unprepare(wpe_clk.CG_IMGSYS_WPE_B);
	clk_unprepare(wpe_clk.CG_IMGSYS_WPE_A);
	smi_clk_unprepare(SMI_LARB_IMGSYS1, "camera_wpe", 1);
}

static inline void WPE_Disable_ccf_clock(void)
{
	/* must keep this clk close order:*/
	/*WPE clk -> CG_SCP_SYS_ISP ->*/
	/*CG_MM_SMI_COMMON -> CG_SCP_SYS_DIS */
	clk_disable(wpe_clk.CG_IMGSYS_WPE_B);
	clk_disable(wpe_clk.CG_IMGSYS_WPE_A);
	smi_clk_disable(SMI_LARB_IMGSYS1, "camera_wpe", 1);
}

static inline void WPE_Disable_Unprepare_ccf_clock(void)
{
	/* must keep this clk close order:*/
	/*WPE clk -> CG_SCP_SYS_ISP -> */
	/*CG_MM_SMI_COMMON -> CG_SCP_SYS_DIS */
	clk_disable_unprepare(wpe_clk.CG_IMGSYS_WPE_B);
	clk_disable_unprepare(wpe_clk.CG_IMGSYS_WPE_A);
	smi_bus_disable(SMI_LARB_IMGSYS1, "camera_wpe");
}
#endif
#endif

#define IMGSYS_REG_CG_CLR               (15020000 + 0x8)
#define IMGSYS_REG_CG_SET               (15020000 + 0x4)

/***********************************************************************
 *
 ***********************************************************************/
static void WPE_EnableClock(bool En)
{

	if (En) {		/* Enable clock. */
		/* LOG_DBG("Dpe clock enbled. g_u4EnableClockCount: %d."*/
		/*, g_u4EnableClockCount); */
		switch (g_u4EnableClockCount) {
		case 0:
#ifndef __WPE_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
			WPE_Prepare_Enable_ccf_clock();
#else
			enable_clock(MT_CG_DOWE0_SMI_COMMON, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			/* enable_clock(MT_CG_IMAGE_FD, "CAMERA"); */
			enable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
#endif	/* #if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) */
#endif
			break;
		default:
			break;
		}
		spin_lock(&(WPEInfo.SpinLockWPE));
		g_u4EnableClockCount++;
		spin_unlock(&(WPEInfo.SpinLockWPE));
	} else {		/* Disable clock. */

		/* LOG_DBG("Dpe clock disabled. g_u4EnableClockCount: %d.",*/
		/*g_u4EnableClockCount); */
		spin_lock(&(WPEInfo.SpinLockWPE));
		g_u4EnableClockCount--;
		spin_unlock(&(WPEInfo.SpinLockWPE));
		switch (g_u4EnableClockCount) {
		case 0:
#ifndef __WPE_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
			WPE_Disable_Unprepare_ccf_clock();
#else
			/* do disable clock */
			disable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			disable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			disable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			/* disable_clock(MT_CG_IMAGE_FD, "CAMERA"); */
			disable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
			disable_clock(MT_CG_DOWE0_SMI_COMMON, "CAMERA");
#endif	/* #if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) */
#endif
			break;
		default:
			break;
		}
	}
}

/***********************************************************************
 *
 ***********************************************************************/
static inline void WPE_Reset(void)
{
	LOG_INF("- E.");

	LOG_INF(" WPE Reset start!\n");
	spin_lock(&(WPEInfo.SpinLockWPERef));

	if (WPEInfo.UserCount > 1) {
		spin_unlock(&(WPEInfo.SpinLockWPERef));
		LOG_INF("Curr UserCount(%d) users exist", WPEInfo.UserCount);
	} else {
		spin_unlock(&(WPEInfo.SpinLockWPERef));
		/* Reset WPE flow */
		WPE_WR32(WPE_CTL_SW_CTL_REG, 0x1);
		while ((WPE_RD32(WPE_CTL_SW_CTL_REG) & 0x02) != 0x2)
			LOG_INF("WPE A resetting...\n");
		WPE_WR32(WPE_CTL_SW_CTL_REG, 0x4);
		WPE_WR32(WPE_CTL_SW_CTL_REG, 0x0);

		WPE_WR32(WPE_B_CTL_SW_CTL_REG, 0x1);
		while ((WPE_RD32(WPE_B_CTL_SW_CTL_REG) & 0x02) != 0x2)
			LOG_INF("WPE B resetting...\n");
		WPE_WR32(WPE_B_CTL_SW_CTL_REG, 0x4);
		WPE_WR32(WPE_B_CTL_SW_CTL_REG, 0x0);

		LOG_INF(" WPE Reset end!\n");

	}

}

/***********************************************************************
 *
 ***********************************************************************/
static signed int WPE_ReadReg(struct WPE_REG_IO_STRUCT *pRegIo)
{
	unsigned int i;
	signed int Ret = 0;
	/*  */
	struct WPE_REG_STRUCT reg;
	/* unsigned int* pData = (unsigned int*)pRegIo->Data; */
	struct WPE_REG_STRUCT *pData = (struct WPE_REG_STRUCT *) pRegIo->pData;

	for (i = 0; i < pRegIo->Count; i++) {
		if (get_user(reg.Addr, (unsigned int *) &pData->Addr) != 0) {
			LOG_ERR("get_user failed");
			Ret = -EFAULT;
			goto EXIT;
		}
		/* pData++; */
		/*  */
		if ((ISP_WPE_BASE + reg.Addr >= ISP_WPE_BASE)
		    && (ISP_WPE_BASE + reg.Addr <
		    (ISP_WPE_BASE + WPE_REG_RANGE))
		    && ((reg.Addr & 0x3) == 0)) {
			reg.Val = WPE_RD32(ISP_WPE_BASE + reg.Addr);
		} else {
			LOG_ERR(
				"Wrong address(0x%p)",
				(ISP_WPE_BASE + reg.Addr));
			reg.Val = 0;
		}
		/*  */
		/* LOG_ERR("[KernelRDReg]addr(0x%x),value()0x%x\n",*/
		/*WPEDDR_CAMINF + reg.Addr,reg.Val); */

		if (put_user(reg.Val, (unsigned int *) &(pData->Val)) != 0) {
			LOG_ERR("put_user failed");
			Ret = -EFAULT;
			goto EXIT;
		}
		pData++;
		/*  */
	}
	/*  */
EXIT:
	return Ret;
}


/***********************************************************************
 *
 ***********************************************************************/
/* Can write sensor's test model only, */
/*if need write to other modules, need modify current code flow */
static signed int WPE_WriteRegToHw
	(struct WPE_REG_STRUCT *pReg, unsigned int Count)
{
	signed int Ret = 0;
	unsigned int i;
	bool dbgWriteReg;

	/* Use local variable to store WPEInfo.*/
	/*DebugMask & WPE_DBG_WRITE_REG for saving lock time */
	spin_lock(&(WPEInfo.SpinLockWPE));
	dbgWriteReg = WPEInfo.DebugMask & WPE_DBG_WRITE_REG;
	spin_unlock(&(WPEInfo.SpinLockWPE));

	/*  */
	if (dbgWriteReg)
		LOG_DBG("- E.\n");

	/*  */
	for (i = 0; i < Count; i++) {
		if (dbgWriteReg) {
			LOG_DBG("Addr(0x%lx), Val(0x%x)\n",
				(unsigned long)(ISP_WPE_BASE + pReg[i].Addr),
				(unsigned int) (pReg[i].Val));
		}

		if (((ISP_WPE_BASE + pReg[i].Addr) <
			(ISP_WPE_BASE + WPE_REG_RANGE))
			&& ((pReg[i].Addr & 0x3) == 0)) {
			WPE_WR32(ISP_WPE_BASE + pReg[i].Addr, pReg[i].Val);
		} else {
			LOG_ERR("wrong address(0x%lx)\n",
				(unsigned long)(ISP_WPE_BASE + pReg[i].Addr));
		}
	}

	/*  */
	return Ret;
}



/**********************************************************************
 *
 **********************************************************************/
static signed int WPE_WriteReg(struct WPE_REG_IO_STRUCT *pRegIo)
{
	signed int Ret = 0;
	/*
	 * signed int TimeVd = 0;
	 * signed int TimeExpdone = 0;
	 * signed int TimeTasklet = 0;
	 */
	/* unsigned char pData = NULL; */
	struct WPE_REG_STRUCT *pData = NULL;

	/*  */
	if ((pRegIo->pData == NULL) || (pRegIo->Count == 0) ||
		(pRegIo->Count > 0xFFFFFFFF)) {
		LOG_ERR(
			"ERROR: pRegIo->pData is NULL or Count error:%d\n",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	if (WPEInfo.DebugMask & WPE_DBG_WRITE_REG)
		LOG_DBG("Data(0x%p), Count(%d)\n",
		(pRegIo->pData), (pRegIo->Count));

	/* pData = (MUINT8*)kmalloc((pRegIo->Count) */
	/*	*sizeof(WPE_REG_STRUCT), GFP_ATOMIC); */
	pData = kmalloc((pRegIo->Count) *
		sizeof(struct WPE_REG_STRUCT), GFP_ATOMIC);
	if (pData == NULL) {
		LOG_DBG(
			"ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)\n",
			current->comm, current->pid, current->tgid);
		Ret = -ENOMEM;
		goto EXIT;
	}
	if (copy_from_user
	    (pData, (void __user *)(pRegIo->pData),
	    pRegIo->Count * sizeof(struct WPE_REG_STRUCT)) != 0) {
		LOG_ERR("copy_from_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	Ret = WPE_WriteRegToHw(pData, pRegIo->Count);
	/*  */
EXIT:
	if (pData != NULL) {
		kfree(pData);
		pData = NULL;
	}
	return Ret;
}


/***********************************************************************
 *
 ***********************************************************************/
static signed int WPE_WaitIrq(struct WPE_WAIT_IRQ_STRUCT *WaitIrq)
{

	signed int Ret = 0;
	signed int Timeout = WaitIrq->Timeout;
	enum WPE_PROCESS_ID_ENUM whichReq = WPE_PROCESS_ID_NONE;


	/*MUINT32 i; */
	unsigned long flags;	/* old: MUINT32 flags; */
	/* FIX to avoid build warning */
	unsigned int irqStatus;
	/*int cnt = 0; */
	struct timeval time_getrequest;
	unsigned long long sec = 0;
	unsigned long usec = 0;

	/* do_gettimeofday(&time_getrequest); */
	sec = cpu_clock(0);	/* ns */
	do_div(sec, 1000);	/* usec */
	usec = do_div(sec, 1000000);	/* sec and usec */
	time_getrequest.tv_usec = usec;
	time_getrequest.tv_sec = sec;


	/* Debug interrupt */
	if (WPEInfo.DebugMask & WPE_DBG_INT) {
		if (WaitIrq->Status & WPEInfo.IrqInfo.Mask[WaitIrq->Type]) {
			if (WaitIrq->UserKey > 0) {
				LOG_ERR(
					"+WaitIrqClr(%d),Type(%d),Status(0x%08X),Timeout(%d),user(%d),ProcID(%d)\n",
				     WaitIrq->Clear, WaitIrq->Type,
				     WaitIrq->Status,
				     WaitIrq->Timeout,
				     WaitIrq->UserKey,
				     WaitIrq->ProcessID);
			}
		}
	}

	/* 1. wait type update */
	if (WaitIrq->Clear == WPE_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(
			&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		/* LOG_DBG("WARNING: Clear(%d), Type(%d):
		 *IrqStatus(0x%08X) has been cleared"
		 * ,WaitIrq->EventInfo.Clear,WaitIrq->Type,
		 *WPEInfo.IrqInfo.Status[WaitIrq->Type]);
		 * WPEInfo.IrqInfo.Status[WaitIrq->Type][
		 *	WaitIrq->EventInfo.UserKey] &=
		 * (~WaitIrq->EventInfo.Status);
		 */
		WPEInfo.IrqInfo.Status[WaitIrq->Type] &= (~WaitIrq->Status);
		spin_unlock_irqrestore(
			&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		return Ret;
	}

	if (WaitIrq->Clear == WPE_IRQ_CLEAR_WAIT) {
		spin_lock_irqsave(
			&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		if (WPEInfo.IrqInfo.Status[WaitIrq->Type] & WaitIrq->Status)
			WPEInfo.IrqInfo.Status[WaitIrq->Type] &=
				(~WaitIrq->Status);

		spin_unlock_irqrestore(
			&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);

	} else if (WaitIrq->Clear == WPE_IRQ_CLEAR_ALL) {
		spin_lock_irqsave(
			&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);

		WPEInfo.IrqInfo.Status[WaitIrq->Type] = 0;
		spin_unlock_irqrestore(
			&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
	}
	/* WPE_IRQ_WAIT_CLEAR ==> do nothing */

	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
	spin_lock_irqsave(&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
	irqStatus = WPEInfo.IrqInfo.Status[WaitIrq->Type];
	spin_unlock_irqrestore(&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);

	if (WaitIrq->Status & WPE_INT_ST) {
		whichReq = WPE_PROCESS_ID_WPE;
	} else {
		LOG_ERR(
			"No Such Stats can be waited!! irq Type/User/Sts/Pid(0x%x/%d/0x%x/%d)\n",
			WaitIrq->Type,
			WaitIrq->UserKey,
			WaitIrq->Status,
			WaitIrq->ProcessID);
	}

#ifdef WPE_WAITIRQ_LOG
	LOG_INF(
	"before wait_event: WaitIrq Timeout(%d) Clear(%d), Type(%d), IrqStatus(0x%08X)\n",
		WaitIrq->Timeout,
		WaitIrq->Clear,
		WaitIrq->Type,
		irqStatus);
	LOG_INF(
	"before wait_event: WaitStatus(0x%08X), Timeout(%d),userKey(%d),whichReq(%d),ProcessID(%d)\n",
	     WaitIrq->Status,
	     WaitIrq->Timeout,
	     WaitIrq->UserKey,
	     whichReq,
	     WaitIrq->ProcessID);
	LOG_INF(
	"before wait_event: WpeIrqCnt(0x%08X), WriteReqIdx(0x%08X), ReadReqIdx(0x%08X)\n",
		WPEInfo.IrqInfo.WpeIrqCnt,
		WPEInfo.WriteReqIdx,
		WPEInfo.ReadReqIdx);
#endif

	/* 2. start to wait signal */
	Timeout = wait_event_interruptible_timeout(WPEInfo.WaitQueueHead,
						   WPE_GetIRQState(
							WaitIrq->Type,
							WaitIrq->UserKey,
							WaitIrq->Status,
							whichReq,
							WaitIrq->ProcessID),
							WPE_MsToJiffies(
							WaitIrq->Timeout));

	/* check if user is interrupted by system signal */
	if ((Timeout != 0)
	    &&
	    (!WPE_GetIRQState
		(WaitIrq->Type, WaitIrq->UserKey, WaitIrq->Status,
			whichReq, WaitIrq->ProcessID))) {
		LOG_DBG(
			"int by sys signal, rtn val(%d), irq Type/User/Sts/whichReq/Pid(0x%x/%d/0x%x/%d/%d)\n",
		     Timeout, WaitIrq->Type,
		     WaitIrq->UserKey, WaitIrq->Status, whichReq,
		     WaitIrq->ProcessID);
		Ret = -ERESTARTSYS;	/* actually it should be -ERESTARTSYS */
		goto EXIT;
	}
	/* timeout */
	if (Timeout == 0) {
		/* Store irqinfo status in here to redeuce time*/
		/*of spin_lock_irqsave */
		spin_lock_irqsave(
			&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = WPEInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);

		LOG_ERR(
			"Timeout!!! ERRRR WaitIrq Timeout(%d) Clear(%d), Type(%d), IrqStatus(0x%08X)\n",
		     WaitIrq->Timeout,
		     WaitIrq->Clear,
		     WaitIrq->Type,
		     irqStatus);
		LOG_ERR(
			"Timeout!!! ERRRR WaitStatus(0x%08X), Timeout(%d),userKey(%d),whichReq(%d),ProcessID(%d)\n",
		     WaitIrq->Status,
		     WaitIrq->Timeout,
		     WaitIrq->UserKey,
		     whichReq,
		     WaitIrq->ProcessID);
		LOG_ERR(
			"Timeout!!! ERRRR WpeIrqCnt(0x%08X), WriteReqIdx(0x%08X), ReadReqIdx(0x%08X)\n",
		     WPEInfo.IrqInfo.WpeIrqCnt,
		     WPEInfo.WriteReqIdx,
		     WPEInfo.ReadReqIdx);

		if (WaitIrq->bDumpReg)
		WPE_DumpReg();

		Ret = -EFAULT;
		goto EXIT;
	} else {
		/* Store irqinfo status in here to*/
		/* redeuce time of spin_lock_irqsave */
#ifdef __WPE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("WaitIrq");
#endif

		spin_lock_irqsave(
			&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = WPEInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);

		if (WaitIrq->Clear == WPE_IRQ_WAIT_CLEAR) {
			spin_lock_irqsave(
				&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
#ifdef WPE_USE_GCE

#ifdef WPE_MULTIPROCESS_TIMEING_ISSUE
			WPEInfo.ReadReqIdx =
			    (WPEInfo.ReadReqIdx + 1) %
			    _SUPPORT_MAX_WPE_FRAME_REQUEST_;
			/* actually, it doesn't happen the timging issue!! */
			/* wake_up_interruptible(&WPEInfo.WaitQueueHead); */
#endif
			if (WaitIrq->Status & WPE_INT_ST) {
				WPEInfo.IrqInfo.WpeIrqCnt--;
				if (WPEInfo.IrqInfo.WpeIrqCnt == 0)
					WPEInfo.IrqInfo.Status[WaitIrq->Type]
					&= (~WaitIrq->Status);
			} else {
				LOG_ERR(
					"WPE_IRQ_WAIT_CLEAR Error, Type(%d), WaitStatus(0x%08X)",
					WaitIrq->Type, WaitIrq->Status);
			}
#else
			if (WPEInfo.IrqInfo.Status[WaitIrq->Type] &
				WaitIrq->Status)
				WPEInfo.IrqInfo.Status[WaitIrq->Type]
					&= (~WaitIrq->Status);
#endif
			spin_unlock_irqrestore(
				&(WPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		}
#ifdef WPE_WAITIRQ_LOG
		LOG_INF(
			"no Timeout!!!: WaitIrq Timeout(%d) Clear(%d), Type(%d), IrqStatus(0x%08X)\n",
		     WaitIrq->Timeout,
		     WaitIrq->Clear,
		     WaitIrq->Type,
		     irqStatus);
		LOG_INF(
			"no Timeout!!!: WaitStatus(0x%08X), Timeout(%d),userKey(%d),whichReq(%d),ProcessID(%d)\n",
		     WaitIrq->Status, WaitIrq->Timeout,
		     WaitIrq->UserKey, whichReq,
		     WaitIrq->ProcessID);
		LOG_INF(
			"no Timeout!!!: WpeIrqCnt(0x%08X), WriteReqIdx(0x%08X), ReadReqIdx(0x%08X)\n",
		     WPEInfo.IrqInfo.WpeIrqCnt,
		     WPEInfo.WriteReqIdx,
		     WPEInfo.ReadReqIdx);
#endif

#ifdef __WPE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif

	}


EXIT:


	return Ret;
}

static inline unsigned int WPE_GetFrameState(signed int WPEReadIdx)
{
	unsigned int j;
	unsigned int ret = 0;
	/*unsigned long flags;*/
	LOG_DBG("WPE:WPE_GetFrameState\n");

	/*spin_lock_irqsave(*/
	/*&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]), flags);*/
	if (g_WPE_ReqRing.WPEReq_Struct[WPEReadIdx].State ==
		WPE_REQUEST_STATE_PENDING ||
	    g_WPE_ReqRing.WPEReq_Struct[WPEReadIdx].State ==
		WPE_REQUEST_STATE_RUNNING) {
		for (j = 0; j < _SUPPORT_MAX_WPE_FRAME_REQUEST_; j++) {
			if (WPE_FRAME_STATUS_ENQUE ==
			    g_WPE_ReqRing.WPEReq_Struct[
				WPEReadIdx].WpeFrameStatus[j]) {
				ret = 0;
			} else {
				ret = 1;
			}
		}
	}
	/*spin_unlock_irqrestore(*/
	/*&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]),flags);*/
	LOG_DBG("WPE: Leave WPE_GetFrameState\n");

	return ret;
}

/***********************************************************************
 *
 ***********************************************************************/
static long WPE_ioctl(struct file *pFile,
				unsigned int Cmd, unsigned long Param)
{
	 signed int Ret = 0;

	/*unsigned int pid = 0; */
	struct WPE_REG_IO_STRUCT RegIo;
	struct WPE_WAIT_IRQ_STRUCT IrqInfo;
	struct WPE_CLEAR_IRQ_STRUCT ClearIrq;
	struct WPE_Config wpe_WpeConfig;
	struct WPE_Request wpe_WpeReq;
	signed int WPEWriteIdx = 0;
	signed int WPEReadIdx = -1;
	int idx;
	struct WPE_USER_INFO_STRUCT *pUserInfo;
	int enqueNum;
	int dequeNum;
	unsigned long flags;	/* old: MUINT32 flags; */
				/* FIX to avoid build warning */
	signed int restTime = 0;



	/*  */
	if (pFile->private_data == NULL) {
		LOG_ERR(
			"private_data is NULL,(process, pid, tgid)=(%s, %d, %d)",
			current->comm,
			current->pid,
			current->tgid);
		return -EFAULT;
	}
	/*  */
	pUserInfo = (struct WPE_USER_INFO_STRUCT *) (pFile->private_data);
	/*  */
	switch (Cmd) {
	case WPE_RESET:
		{
			spin_lock(&(WPEInfo.SpinLockWPE));
			WPE_Reset();
			spin_unlock(&(WPEInfo.SpinLockWPE));
			break;
		}

		/*  */
	case WPE_DUMP_REG:
		{
			Ret = WPE_DumpReg();
			break;
		}
	case WPE_DUMP_ISR_LOG:
		{
			unsigned int currentPPB = m_CurrentPPB;

			spin_lock_irqsave(
				&(WPEInfo.SpinLockIrq[
				WPE_IRQ_TYPE_INT_WPE_ST]), flags);

			m_CurrentPPB = (m_CurrentPPB + 1) % LOG_PPNUM;
			spin_unlock_irqrestore(
				&(WPEInfo.SpinLockIrq[
					WPE_IRQ_TYPE_INT_WPE_ST]), flags);

			IRQ_LOG_PRINTER(WPE_IRQ_TYPE_INT_WPE_ST,
				currentPPB, _LOG_INF);
			IRQ_LOG_PRINTER(WPE_IRQ_TYPE_INT_WPE_ST,
				currentPPB, _LOG_ERR);
			break;
		}
	case WPE_READ_REGISTER:
		{
			if (copy_from_user(&RegIo,
				(void *)Param,
				sizeof(struct WPE_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir of copy
				 *from user is implemented in
				 * WPE_ReadReg(...)
				 */
				Ret = WPE_ReadReg(&RegIo);
			} else {
				LOG_ERR(
					"WPE_READ_REGISTER copy_from_user failed"
					);
				Ret = -EFAULT;
			}
			break;
		}
	case WPE_WRITE_REGISTER:
		{
			if (copy_from_user(&RegIo,
				(void *)Param,
				sizeof(struct WPE_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir of copy from*/
				/*user is implemented in WPE_WriteReg(...) */
				Ret = WPE_WriteReg(&RegIo);
			} else {
				LOG_ERR(
					"WPE_WRITE_REGISTER copy_from_user failed"
					);
				Ret = -EFAULT;
			}
			break;
		}
	case WPE_WAIT_IRQ:
		{
			if (copy_from_user(
				&IrqInfo,
				(void *)Param,
				sizeof(struct WPE_WAIT_IRQ_STRUCT)) == 0) {
				/*  */
				if ((IrqInfo.Type >= WPE_IRQ_TYPE_AMOUNT)
					|| (IrqInfo.Type < 0)) {
					Ret = -EFAULT;
					LOG_ERR("invalid type(%d)",
						IrqInfo.Type);
					goto EXIT;
				}

				if ((IrqInfo.UserKey >= IRQ_USER_NUM_MAX) ||
					(IrqInfo.UserKey < 0)) {
					LOG_ERR(
						"invalid userKey(%d), max(%d), force userkey = 0\n",
						IrqInfo.UserKey,
						IRQ_USER_NUM_MAX);
					IrqInfo.UserKey = 0;
				}

				LOG_INF(
					"IRQ clear(%d), type(%d), userKey(%d), timeout(%d), status(%d)\n",
				     IrqInfo.Clear,
				     IrqInfo.Type,
				     IrqInfo.UserKey,
				     IrqInfo.Timeout,
				     IrqInfo.Status);

				IrqInfo.ProcessID = pUserInfo->Pid;
				Ret = WPE_WaitIrq(&IrqInfo);

				if (copy_to_user
				    ((void *)Param,
				    &IrqInfo,
				    sizeof(struct WPE_WAIT_IRQ_STRUCT)) != 0) {
					LOG_ERR("copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR("WPE_WAIT_IRQ copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case WPE_CLEAR_IRQ:
		{
			if (copy_from_user(&ClearIrq,
				(void *)Param,
				sizeof(struct WPE_CLEAR_IRQ_STRUCT)) == 0) {
				LOG_DBG("WPE_CLEAR_IRQ Type(%d)",
					ClearIrq.Type);

				if ((ClearIrq.Type >= WPE_IRQ_TYPE_AMOUNT) ||
					(ClearIrq.Type < 0)) {
					Ret = -EFAULT;
					LOG_ERR("invalid type(%d)",
						ClearIrq.Type);
					goto EXIT;
				}

				/*  */
				if ((ClearIrq.UserKey >= IRQ_USER_NUM_MAX)
				    || (ClearIrq.UserKey < 0)) {
					LOG_ERR("errUserEnum(%d)",
						ClearIrq.UserKey);
					Ret = -EFAULT;
					goto EXIT;
				}

				LOG_DBG(
					"WPE_CLEAR_IRQ:Type(%d),Status(0x%08X),IrqStatus(0x%08X)\n",
					ClearIrq.Type, ClearIrq.Status,
					WPEInfo.IrqInfo.Status[ClearIrq.Type]);
				spin_lock_irqsave(
					&(WPEInfo.SpinLockIrq[ClearIrq.Type]),
					flags);
				WPEInfo.IrqInfo.Status[ClearIrq.Type] &=
					(~ClearIrq.Status);
				spin_unlock_irqrestore(
					&(WPEInfo.SpinLockIrq[ClearIrq.Type]),
						       flags);
			} else {
				LOG_ERR(
					"WPE_CLEAR_IRQ copy_from_user failed\n"
					);
				Ret = -EFAULT;
			}
			break;
		}
	case WPE_ENQNUE_NUM:
		{
			/* enqueNum */
			if (copy_from_user(&enqueNum,
				(void *)Param, sizeof(int)) == 0) {
				if (WPE_REQUEST_STATE_EMPTY ==
				    g_WPE_ReqRing.WPEReq_Struct[
				    g_WPE_ReqRing.WriteIdx].State) {
					spin_lock_irqsave(
						&(WPEInfo.SpinLockIrq
						[WPE_IRQ_TYPE_INT_WPE_ST]),
						flags);

					g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.WriteIdx].processID =
					pUserInfo->Pid;

					g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.WriteIdx].enqueReqNum =
					enqueNum;
					spin_unlock_irqrestore(
						&(WPEInfo.SpinLockIrq
					[WPE_IRQ_TYPE_INT_WPE_ST]), flags);
					if (enqueNum >
					_SUPPORT_MAX_WPE_FRAME_REQUEST_) {
						LOG_ERR(
							"WPE Enque Num is bigger than enqueNum:%d\n",
						     enqueNum);
					}
					LOG_DBG("WPE_ENQNUE_NUM:%d\n",
						enqueNum);
				} else {
					LOG_ERR(
						"WPE Enque request state is not empty:%d, writeIdx:%d, readIdx:%d\n",
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.WriteIdx].State,
					     g_WPE_ReqRing.WriteIdx,
					     g_WPE_ReqRing.ReadIdx);
				}
			} else {
				LOG_ERR(
					"WPE_WPE_EQNUE_NUM copy_from_user failed\n"
					);
				Ret = -EFAULT;
			}

			break;
		}
		/* WPE_Config */
	case WPE_ENQUE:
		{
			if (copy_from_user(&wpe_WpeConfig,
				(void *)Param,
				sizeof(struct WPE_Config)) == 0) {
				/* LOG_DBG("WPE_CLEAR_IRQ:
				 *Type(%d),Status(0x%08X),IrqStatus(0x%08X)",
				 * ClearIrq.Type, ClearIrq.Status,
				 * WPEInfo.IrqInfo.Status[ClearIrq.Type]);
				 */
				spin_lock_irqsave(
				&(WPEInfo.SpinLockIrq[
					WPE_IRQ_TYPE_INT_WPE_ST]), flags);
				if ((WPE_REQUEST_STATE_EMPTY ==
				     g_WPE_ReqRing.WPEReq_Struct[
				     g_WPE_ReqRing.WriteIdx].State)
				    && (g_WPE_ReqRing.WPEReq_Struct[
				    g_WPE_ReqRing.WriteIdx].
					FrameWRIdx <
					g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.WriteIdx].
					enqueReqNum)) {
					g_WPE_ReqRing.
					    WPEReq_Struct[
					    g_WPE_ReqRing.WriteIdx].
					    WpeFrameStatus
					    [g_WPE_ReqRing.
					    WPEReq_Struct[
					    g_WPE_ReqRing.WriteIdx].
					     FrameWRIdx] =
					     WPE_FRAME_STATUS_ENQUE;

					memcpy(&g_WPE_ReqRing.WPEReq_Struct
					       [g_WPE_ReqRing.
						WriteIdx].WpeFrameConfig
						[g_WPE_ReqRing.WPEReq_Struct
						[g_WPE_ReqRing.WriteIdx].
						FrameWRIdx++],
					       &wpe_WpeConfig,
					       sizeof(struct WPE_Config));

					if (g_WPE_ReqRing.WPEReq_Struct
					    [g_WPE_ReqRing.WriteIdx].
					    FrameWRIdx == g_WPE_ReqRing.
					    WPEReq_Struct[g_WPE_ReqRing.
					    WriteIdx].enqueReqNum) {

						g_WPE_ReqRing.WPEReq_Struct[
						g_WPE_ReqRing.WriteIdx].State =
						    WPE_REQUEST_STATE_PENDING;
						g_WPE_ReqRing.WriteIdx =
						    (g_WPE_ReqRing.WriteIdx +
						     1) %
					_SUPPORT_MAX_WPE_REQUEST_RING_SIZE_;
						LOG_DBG("WPE enque done!!\n");
					} else {
						LOG_DBG("WPE enque frame!!\n");
					}
				} else {
					LOG_ERR(
						"NoEmptyWPEBuf! WriteIdx(%d),State(%d),FraWRIdx(%d),enqReqNum(%d)\n",
					     g_WPE_ReqRing.WriteIdx,
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.WriteIdx].State,
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.WriteIdx].FrameWRIdx,
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.WriteIdx].
					     enqueReqNum);
				}
#ifdef WPE_USE_GCE
				spin_unlock_irqrestore(
					&(WPEInfo.SpinLockIrq
					[WPE_IRQ_TYPE_INT_WPE_ST]), flags);
				LOG_INF("ConfigWPE!!\n");
				ConfigWPE();
#else
				/* check the hw is running or not ? */
				if (Check_WPE_Is_Busy() == 0) {
					/* config the WPE hw and run */
					LOG_DBG("ConfigWPE\n");
					ConfigWPE();
				} else {
					LOG_INF("WPE HW is busy!!\n");
				}
				spin_unlock_irqrestore(
					&(WPEInfo.SpinLockIrq
					[WPE_IRQ_TYPE_INT_WPE_ST]), flags);
#endif


			} else {
				LOG_ERR("WPE_WPE_ENQ copy_from_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}
	case WPE_ENQUE_REQ:
		{
			if (copy_from_user(&wpe_WpeReq,
				(void *)Param,
				sizeof(struct WPE_Request)) == 0) {
				LOG_DBG(
					"WPE_ENQNUE_NUM:%d, pid:%d\n",
					wpe_WpeReq.m_ReqNum,
					pUserInfo->Pid);
				if (wpe_WpeReq.m_ReqNum >
					_SUPPORT_MAX_WPE_FRAME_REQUEST_) {
					LOG_ERR(
						"WPE Enque Num is bigger than enqueNum:%d\n",
					wpe_WpeReq.m_ReqNum);
					Ret = -EFAULT;
					goto EXIT;
				}
				if (copy_from_user
				    (g_WpeEnqueReq_Struct.WpeFrameConfig,
				     (void *)wpe_WpeReq.m_pWpeConfig,
				     wpe_WpeReq.m_ReqNum *
				     sizeof(struct WPE_Config)) != 0) {
					LOG_ERR(
						"copy WPEConfig from request is fail!!\n");
					Ret = -EFAULT;
					goto EXIT;
				}

				mutex_lock(&gWpeMutex);
				/* Protect the Multi Process */

				spin_lock_irqsave(
				&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]),
						  flags);
				if (WPE_REQUEST_STATE_EMPTY ==
				    g_WPE_ReqRing.WPEReq_Struct[
				    g_WPE_ReqRing.WriteIdx].State) {
					g_WPE_ReqRing.WPEReq_Struct[
						g_WPE_ReqRing.WriteIdx].
					    processID = pUserInfo->Pid;
					g_WPE_ReqRing.WPEReq_Struct[
						g_WPE_ReqRing.WriteIdx].
					    enqueReqNum = wpe_WpeReq.m_ReqNum;

					for (idx = 0;
						idx < wpe_WpeReq.m_ReqNum;
						idx++) {
						g_WPE_ReqRing.WPEReq_Struct[
						g_WPE_ReqRing.WriteIdx].
						WpeFrameStatus[
						g_WPE_ReqRing.WPEReq_Struct[
						g_WPE_ReqRing.WriteIdx].
						FrameWRIdx] =
						    WPE_FRAME_STATUS_ENQUE;

						memcpy(&g_WPE_ReqRing.
							WPEReq_Struct
						       [g_WPE_ReqRing.
							WriteIdx].
							WpeFrameConfig
						       [g_WPE_ReqRing.
						       WPEReq_Struct
							[g_WPE_ReqRing.
							WriteIdx].
							FrameWRIdx++],
						       &g_WpeEnqueReq_Struct.
						       WpeFrameConfig[idx],
						       sizeof(
						       struct WPE_Config));
					}
					LOG_INF(
						"WPE ENQUE_REQ ProcessID(%d), WriteIdx(%d)\n",
						pUserInfo->Pid,
						g_WPE_ReqRing.WriteIdx);
					g_WPE_ReqRing.WPEReq_Struct[
						g_WPE_ReqRing.WriteIdx].State =
					    WPE_REQUEST_STATE_PENDING;
					WPEWriteIdx = g_WPE_ReqRing.WriteIdx;
					g_WPE_ReqRing.WriteIdx =
					    (g_WPE_ReqRing.WriteIdx + 1) %
					_SUPPORT_MAX_WPE_REQUEST_RING_SIZE_;

				} else {
					LOG_ERR(
						"NoEmptyWPEBuf! WriteIdx(%d),State(%d),FraWRIdx(%d),enqReqNum(%d)\n",
					     g_WPE_ReqRing.WriteIdx,
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.WriteIdx].
					     State,
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.WriteIdx].
					     FrameWRIdx,
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.WriteIdx].
					     enqueReqNum);
				}
				spin_unlock_irqrestore(
					&(WPEInfo.SpinLockIrq
					[WPE_IRQ_TYPE_INT_WPE_ST]),
					flags);
				LOG_DBG("ConfigWPE Request!!\n");
				ConfigWPERequest(WPEWriteIdx);

				mutex_unlock(&gWpeMutex);
			} else {
				LOG_ERR(
					"WPE_ENQUE_REQ copy_from_user failed\n"
					);
				Ret = -EFAULT;
			}
			wake_up_interruptible(&WPEInfo.WaitDeque);
			break;
		}
	case WPE_DEQUE_NUM:
		{
			if (WPE_REQUEST_STATE_FINISHED ==
			    g_WPE_ReqRing.WPEReq_Struct[
			    g_WPE_ReqRing.ReadIdx].State) {
				dequeNum =
				    g_WPE_ReqRing.WPEReq_Struct[
				    g_WPE_ReqRing.ReadIdx].enqueReqNum;
				LOG_DBG("WPE_DEQUE_NUM(%d)\n", dequeNum);
			} else {
				dequeNum = 0;
				LOG_ERR(
					"DeqNum:NoAnyWPEReadyBuf!!ReadIdx(%d),State(%d),FraRDIdx(%d),enqReqNum(%d)\n",
				     g_WPE_ReqRing.ReadIdx,
				     g_WPE_ReqRing.WPEReq_Struct[
				     g_WPE_ReqRing.ReadIdx].State,
				     g_WPE_ReqRing.WPEReq_Struct[
				     g_WPE_ReqRing.ReadIdx].FrameRDIdx,
				     g_WPE_ReqRing.WPEReq_Struct[
				     g_WPE_ReqRing.ReadIdx].
				     enqueReqNum);
			}
			if (copy_to_user((void *)Param,
				&dequeNum,
				sizeof(unsigned int)) != 0) {
				LOG_ERR(
					"WPE_WPE_DEQUE_NUM copy_to_user failed\n"
					);
				Ret = -EFAULT;
			}

			break;
		}

	case WPE_DEQUE:
		{
			spin_lock_irqsave(
				&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]),
				flags);
			if ((WPE_REQUEST_STATE_FINISHED ==
			     g_WPE_ReqRing.WPEReq_Struct[
			     g_WPE_ReqRing.ReadIdx].State)
				&& (g_WPE_ReqRing.WPEReq_Struct[
				g_WPE_ReqRing.ReadIdx].FrameRDIdx <
				g_WPE_ReqRing.WPEReq_Struct[
				g_WPE_ReqRing.ReadIdx].enqueReqNum)) {
				/* dequeNum = g_DVE_RequestRing.*/
				/*DVEReq_Struct[g_DVE_RequestRing.ReadIdx].*/
				/*enqueReqNum; */
				if (g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.ReadIdx].
					WpeFrameStatus[
					g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.ReadIdx].FrameRDIdx]
					== WPE_FRAME_STATUS_FINISHED) {

					memcpy(&wpe_WpeConfig,
					       &g_WPE_ReqRing.WPEReq_Struct[
					       g_WPE_ReqRing.
						ReadIdx].WpeFrameConfig
					       [g_WPE_ReqRing.WPEReq_Struct[
					       g_WPE_ReqRing.ReadIdx].
						FrameRDIdx],
						sizeof(struct WPE_Config));

					g_WPE_ReqRing.
					    WPEReq_Struct[
					    g_WPE_ReqRing.ReadIdx].
					    WpeFrameStatus
					    [g_WPE_ReqRing.WPEReq_Struct[
					    g_WPE_ReqRing.ReadIdx].
					     FrameRDIdx++] =
					     WPE_FRAME_STATUS_EMPTY;

				}
				if (g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.ReadIdx].FrameRDIdx ==
				    g_WPE_ReqRing.WPEReq_Struct[
				    g_WPE_ReqRing.ReadIdx].enqueReqNum) {
					g_WPE_ReqRing.WPEReq_Struct[
						g_WPE_ReqRing.ReadIdx].State =
					    WPE_REQUEST_STATE_EMPTY;
					g_WPE_ReqRing.WPEReq_Struct[
						g_WPE_ReqRing.ReadIdx].
					    FrameWRIdx = 0;
					g_WPE_ReqRing.WPEReq_Struct[
						g_WPE_ReqRing.ReadIdx].
					    FrameRDIdx = 0;
					g_WPE_ReqRing.WPEReq_Struct[
						g_WPE_ReqRing.ReadIdx].
					    enqueReqNum = 0;
					g_WPE_ReqRing.ReadIdx =
					    (g_WPE_ReqRing.ReadIdx + 1) %
					_SUPPORT_MAX_WPE_REQUEST_RING_SIZE_;
					LOG_DBG("WPE ReadIdx(%d)\n",
						g_WPE_ReqRing.ReadIdx);
				}
				spin_unlock_irqrestore(
					&(WPEInfo.SpinLockIrq
					[WPE_IRQ_TYPE_INT_WPE_ST]), flags);
				if (copy_to_user
				    ((void *)Param,
				     &g_WPE_ReqRing.WPEReq_Struct[
				     g_WPE_ReqRing.ReadIdx].WpeFrameConfig
				     [g_WPE_ReqRing.WPEReq_Struct
				      [g_WPE_ReqRing.ReadIdx].FrameRDIdx],
				     sizeof(struct WPE_Config)) != 0) {
					LOG_ERR(
						"WPE_WPE_DEQUE copy_to_user failed\n"
						);
					Ret = -EFAULT;
				}

			} else {
				spin_unlock_irqrestore(
					&(WPEInfo.SpinLockIrq
					[WPE_IRQ_TYPE_INT_WPE_ST]), flags);
				LOG_ERR(
					"No Any WPE Ready Buf!!, ReadIdx(%d), State(%d), FraRDIdx(%d), enqNum(%d)\n",
				     g_WPE_ReqRing.ReadIdx,
				     g_WPE_ReqRing.WPEReq_Struct[
				     g_WPE_ReqRing.ReadIdx].State,
				     g_WPE_ReqRing.WPEReq_Struct[
				     g_WPE_ReqRing.ReadIdx].FrameRDIdx,
				     g_WPE_ReqRing.WPEReq_Struct[
				     g_WPE_ReqRing.ReadIdx].
				     enqueReqNum);
			}

			break;
		}
	case WPE_DEQUE_REQ:
		{
			if (copy_from_user(&wpe_WpeReq,
				(void *)Param,
				sizeof(struct WPE_Request)) == 0) {
				mutex_lock(&gWpeDequeMutex);
				/* Protect the Multi Process */

				spin_lock_irqsave(
				&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]),
				flags);

				if (WPE_REQUEST_STATE_FINISHED ==
				    g_WPE_ReqRing.WPEReq_Struct[
				    g_WPE_ReqRing.ReadIdx].State) {
					dequeNum =
					    g_WPE_ReqRing.WPEReq_Struct[
					    g_WPE_ReqRing.ReadIdx].
					    enqueReqNum;
					LOG_DBG("WPE_DEQUE_NUM(%d)\n",
						dequeNum);
				} else {
					dequeNum = 0;
					LOG_ERR(
						"DeqNum:NoAnyRdyBuf!!RdIdx(%d),Sta(%d),FraRDIdx(%d),enqReqNum(%d)\n",
					     g_WPE_ReqRing.ReadIdx,
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.ReadIdx].
					     State,
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.ReadIdx].
					     FrameRDIdx,
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.ReadIdx].
					     enqueReqNum);
				}
				wpe_WpeReq.m_ReqNum = dequeNum;

				for (idx = 0; idx < dequeNum; idx++) {
					if (WPE_FRAME_STATUS_FINISHED ==
					    g_WPE_ReqRing.WPEReq_Struct[
					    g_WPE_ReqRing.ReadIdx].
					    WpeFrameStatus[
					    g_WPE_ReqRing.WPEReq_Struct[
					    g_WPE_ReqRing.ReadIdx].
					     FrameRDIdx]) {

						memcpy(&g_WpeDequeReq_Struct.
							WpeFrameConfig[idx],
						       &g_WPE_ReqRing.
						       WPEReq_Struct[
						       g_WPE_ReqRing.ReadIdx].
						       WpeFrameConfig[
						       g_WPE_ReqRing.
						       WPEReq_Struct[
						       g_WPE_ReqRing.ReadIdx].
						       FrameRDIdx],
						       sizeof(
						       struct WPE_Config));

						g_WPE_ReqRing.WPEReq_Struct[
							g_WPE_ReqRing.ReadIdx].
							WpeFrameStatus[
							g_WPE_ReqRing.
							WPEReq_Struct[
							g_WPE_ReqRing.ReadIdx].
							FrameRDIdx++] =
						    WPE_FRAME_STATUS_EMPTY;

					} else {
						LOG_ERR(
							"Err!id(%d),deqNum(%d),RdId(%d),FraRDId(%d),WPEFraSta(%d)\n",
						     idx, dequeNum,
						     g_WPE_ReqRing.ReadIdx,
						     g_WPE_ReqRing.
						     WPEReq_Struct[
						     g_WPE_ReqRing.ReadIdx].
						     FrameRDIdx,
						     g_WPE_ReqRing.
						     WPEReq_Struct[
						     g_WPE_ReqRing.ReadIdx].
						     WpeFrameStatus[
						     g_WPE_ReqRing.
						      WPEReq_Struct[
						      g_WPE_ReqRing.ReadIdx].
						      FrameRDIdx]);
					}
				}
				g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.ReadIdx].State =
				    WPE_REQUEST_STATE_EMPTY;
				g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.ReadIdx].FrameWRIdx = 0;
				g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.ReadIdx].FrameRDIdx = 0;
				g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.ReadIdx].enqueReqNum = 0;
				g_WPE_ReqRing.ReadIdx =
				    (g_WPE_ReqRing.ReadIdx +
				     1) % _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_;
				LOG_DBG(
					"WPE DEQUE Done ProcessID(%d), WriteIdx(%d)\n",
					pUserInfo->Pid,
					g_WPE_ReqRing.WriteIdx);

				spin_unlock_irqrestore(
					&(WPEInfo.SpinLockIrq
					[WPE_IRQ_TYPE_INT_WPE_ST]), flags);

				mutex_unlock(&gWpeDequeMutex);

				if (copy_to_user
				    ((void *)wpe_WpeReq.m_pWpeConfig,
				     &g_WpeDequeReq_Struct.WpeFrameConfig[0],
				     dequeNum *
				     sizeof(struct WPE_Config)) != 0) {
					LOG_ERR(
						"WPE_WPE_DEQUE_REQ copy_to_user frameconfig failed\n"
						);
					Ret = -EFAULT;
				}
				if (copy_to_user
				    ((void *)Param,
				    &wpe_WpeReq,
				    sizeof(struct WPE_Request)) != 0) {
					LOG_ERR(
						"WPE_WPE_DEQUE_REQ copy_to_user failed\n"
						);
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR(
					"WPE_CMD_WPE_DEQUE_REQ copy_from_user failed\n"
					);
				Ret = -EFAULT;
			}

			break;
		}
	case WPE_DEQUE_DONE:
		{
			if (copy_from_user(&wpe_WpeReq,
				(void *)Param,
				sizeof(struct WPE_Request)) == 0) {
				mutex_lock(&gWpeDequeMutex);
				/* Protect the Multi Process */

				spin_lock_irqsave(
				&(WPEInfo.SpinLockIrq[
				WPE_IRQ_TYPE_INT_WPE_ST]), flags);

				if (WPE_REQUEST_STATE_RUNNING ==
				    g_WPE_ReqRing.WPEReq_Struct[
				    g_WPE_ReqRing.ReadIdx].State &&
				    g_WPE_ReqRing.WPEReq_Struct[
				    g_WPE_ReqRing.ReadIdx].processID ==
				    pUserInfo->Pid) {
					dequeNum =
					    g_WPE_ReqRing.WPEReq_Struct[
					    g_WPE_ReqRing.ReadIdx].
					    enqueReqNum;
					LOG_DBG(
						"WPE_DEQUE_NUM(%d)\n",
						dequeNum);
				} else {
					dequeNum = 0;
					LOG_ERR(
						"NoRdyBuf!RingPID(%d),RdIdx(%d),Sta(%d),FraRDIdx(%d),EnReqNum(%d)\n",
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.ReadIdx].processID,
					     g_WPE_ReqRing.ReadIdx,
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.ReadIdx].State,
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.ReadIdx].FrameRDIdx,
					     g_WPE_ReqRing.WPEReq_Struct[
					     g_WPE_ReqRing.ReadIdx].
					     enqueReqNum);
				}
				wpe_WpeReq.m_ReqNum = dequeNum;

				for (idx = 0; idx < dequeNum; idx++) {
					if (WPE_FRAME_STATUS_RUNNING ==
					    g_WPE_ReqRing.WPEReq_Struct[
					    g_WPE_ReqRing.ReadIdx].
					    WpeFrameStatus
					    [g_WPE_ReqRing.WPEReq_Struct[
					    g_WPE_ReqRing.ReadIdx].
					     FrameRDIdx]) {

						memcpy(&g_WpeDequeReq_Struct.
							WpeFrameConfig[idx],
						       &g_WPE_ReqRing.
						       WPEReq_Struct[
						       g_WPE_ReqRing.
							ReadIdx].
							WpeFrameConfig[
							g_WPE_ReqRing.
							WPEReq_Struct[
							g_WPE_ReqRing.ReadIdx].
							FrameRDIdx],
						       sizeof
						       (struct WPE_Config));

						g_WPE_ReqRing.WPEReq_Struct[
							g_WPE_ReqRing.
							ReadIdx].WpeFrameStatus[
							g_WPE_ReqRing.
							WPEReq_Struct[
							g_WPE_ReqRing.ReadIdx].
							FrameRDIdx++] =
						    WPE_FRAME_STATUS_EMPTY;
					} else {
						LOG_ERR(
							"Err!id(%d),deqNum(%d),RdId(%d),FraRDId(%d),WPEFraSta(%d)\n",
						     idx, dequeNum,
						     g_WPE_ReqRing.ReadIdx,
						     g_WPE_ReqRing.
						     WPEReq_Struct[
						     g_WPE_ReqRing.ReadIdx].
						     FrameRDIdx,
						     g_WPE_ReqRing.
						     WPEReq_Struct[
						     g_WPE_ReqRing.
							ReadIdx].WpeFrameStatus[
							g_WPE_ReqRing.
						      WPEReq_Struct[
						      g_WPE_ReqRing.ReadIdx].
						      FrameRDIdx]);
					}
				}
				LOG_INF(
					"WPE DEQUE_DONE ProcessID(%d), ReadIdx(%d)\n",
					pUserInfo->Pid,
					g_WPE_ReqRing.ReadIdx);
				g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.ReadIdx].State =
				    WPE_REQUEST_STATE_EMPTY;
				g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.ReadIdx].FrameWRIdx = 0;
				g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.ReadIdx].FrameRDIdx = 0;
				g_WPE_ReqRing.WPEReq_Struct[
					g_WPE_ReqRing.ReadIdx].enqueReqNum = 0;
				g_WPE_ReqRing.ReadIdx =
				    (g_WPE_ReqRing.ReadIdx + 1) %
				    _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_;

				spin_unlock_irqrestore(
					&(WPEInfo.SpinLockIrq
					[WPE_IRQ_TYPE_INT_WPE_ST]), flags);

				mutex_unlock(&gWpeDequeMutex);

				if (copy_to_user
				    ((void *)wpe_WpeReq.m_pWpeConfig,
				     &g_WpeDequeReq_Struct.WpeFrameConfig[0],
				     dequeNum *
				     sizeof(struct WPE_Config)) != 0) {
					LOG_ERR(
						"WPE_WPE_DEQUE_DONE copy_to_user frameconfig failed\n"
						);
					Ret = -EFAULT;
				}
				if (copy_to_user
				    ((void *)Param,
				    &wpe_WpeReq,
				    sizeof(struct WPE_Request)) != 0) {
					LOG_ERR(
						"WPE_WPE_DEQUE_DONE copy_to_user failed\n"
						);
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR(
					"WPE_CMD_WPE_DEQUE_DONE copy_from_user failed\n"
					);
				Ret = -EFAULT;
			}

			break;
		}
	case WPE_WAIT_DEQUE:
		{
			mutex_lock(&gWpeDequeMutex);
			/* Protect the Multi Process */
			spin_lock_irqsave(
				&(WPEInfo.SpinLockIrq
				[WPE_IRQ_TYPE_INT_WPE_ST]), flags);

			if ((WPE_REQUEST_STATE_PENDING ==
			     g_WPE_ReqRing.WPEReq_Struct[
			     g_WPE_ReqRing.ReadIdx].State
			     || WPE_REQUEST_STATE_RUNNING ==
			     g_WPE_ReqRing.WPEReq_Struct[
			     g_WPE_ReqRing.ReadIdx].State)
			    && g_WPE_ReqRing.WPEReq_Struct[
			    g_WPE_ReqRing.ReadIdx].processID ==
			    pUserInfo->Pid) {
				LOG_DBG(
					"WPE_WAIT_DEQUE ProcessID(%d), ReadIdx (%d)\n",
					pUserInfo->Pid,
					g_WPE_ReqRing.ReadIdx);
			} else {
				LOG_ERR(
					"NoAnyRdyBuf!!ProcessID (%d),RdIdx(%d),Sta(%d),FraRDIdx(%d),enqReqNum(%d)\n",
				     g_WPE_ReqRing.WPEReq_Struct[
				     g_WPE_ReqRing.ReadIdx].processID,
				     g_WPE_ReqRing.ReadIdx,
				     g_WPE_ReqRing.WPEReq_Struct[
				     g_WPE_ReqRing.ReadIdx].State,
				     g_WPE_ReqRing.WPEReq_Struct[
				     g_WPE_ReqRing.ReadIdx].FrameRDIdx,
				     g_WPE_ReqRing.WPEReq_Struct[
				     g_WPE_ReqRing.ReadIdx].
				     enqueReqNum);
				Ret = -EFAULT;
				return Ret;
			}
			WPEReadIdx = g_WPE_ReqRing.ReadIdx;
			spin_unlock_irqrestore(
				&(WPEInfo.SpinLockIrq[
				WPE_IRQ_TYPE_INT_WPE_ST]), flags);

			mutex_unlock(&gWpeDequeMutex);
			LOG_DBG("WPE_wait_event_interruptible_timeout\n");

			restTime = wait_event_interruptible_timeout(
					WPEInfo.WaitDeque,
					WPE_GetFrameState(WPEReadIdx),
					WPE_MsToJiffies(5000000));

			if (restTime == 0) {
				LOG_ERR("WPE_WAIT_DEQUE failed\n");
				Ret = -EFAULT;
			}

			break;
		}
	default:
		{
			LOG_ERR("Unknown Cmd(%d)", Cmd);
			LOG_ERR(
				"Fail, Cmd(%d), Dir(%d), Type(%d), Nr(%d),Size(%d)\n",
				Cmd,
				_IOC_DIR(Cmd),
				_IOC_TYPE(Cmd),
				_IOC_NR(Cmd),
				_IOC_SIZE(Cmd));
			Ret = -EPERM;
			break;
		}
	}
	/*  */
EXIT:
	if (Ret != 0) {
		LOG_ERR(
			"Fail, Cmd(%d), Pid(%d), (process, pid, tgid)=(%s, %d, %d)",
			Cmd,
			pUserInfo->Pid,
			current->comm,
			current->pid,
			current->tgid);
	}
	/*  */
	return Ret;
}

#ifdef CONFIG_COMPAT

/***********************************************************************
 *
 ***********************************************************************/
static int compat_get_WPE_read_register_data(
		struct compat_WPE_REG_IO_STRUCT __user *data32,
		struct WPE_REG_IO_STRUCT __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err;

	err = get_user(uptr, &data32->pData);
	err |= put_user(compat_ptr(uptr), &data->pData);
	err |= get_user(count, &data32->Count);
	err |= put_user(count, &data->Count);
	return err;
}

static int compat_put_WPE_read_register_data(
		struct compat_WPE_REG_IO_STRUCT __user *data32,
		struct WPE_REG_IO_STRUCT __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr; */
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->pData); */
	/* err |= put_user(uptr, &data32->pData); */
	err |= get_user(count, &data->Count);
	err |= put_user(count, &data32->Count);
	return err;
}

static int compat_get_WPE_enque_req_data(
		struct compat_WPE_Request __user *data32,
		struct WPE_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pWpeConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pWpeConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_WPE_enque_req_data(
		struct compat_WPE_Request __user *data32,
		struct WPE_Request __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr; */
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pDpeConfig); */
	/* err |= put_user(uptr, &data32->m_pDpeConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}


static int compat_get_WPE_deque_req_data(
		struct compat_WPE_Request __user *data32,
		struct WPE_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pWpeConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pWpeConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_WPE_deque_req_data(
		struct compat_WPE_Request __user *data32,
		struct WPE_Request __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr; */
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pDpeConfig); */
	/* err |= put_user(uptr, &data32->m_pDpeConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}

static long WPE_ioctl_compat(
		struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;


	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		LOG_ERR("no f_op !!!\n");
		return -ENOTTY;
	}
	switch (cmd) {
	case COMPAT_WPE_READ_REGISTER:
		{
			struct compat_WPE_REG_IO_STRUCT __user *data32;
			struct WPE_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_WPE_read_register_data(data32, data);
			if (err) {
				LOG_INF(
					"compat_get_WPE_read_register_data error!!!\n"
					);
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, WPE_READ_REGISTER,
						       (unsigned long)data);
			err = compat_put_WPE_read_register_data(data32, data);
			if (err) {
				LOG_INF(
					"compat_put_WPE_read_register_data error!!!\n"
					);
				return err;
			}
			return ret;
		}
	case COMPAT_WPE_WRITE_REGISTER:
		{
			struct compat_WPE_REG_IO_STRUCT __user *data32;
			struct WPE_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_WPE_read_register_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_WPE_WRITE_REGISTER error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, WPE_WRITE_REGISTER,
						       (unsigned long)data);
			return ret;
		}
	case COMPAT_WPE_ENQUE_REQ:
		{
			struct compat_WPE_Request __user *data32;
			struct WPE_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_WPE_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_WPE_WPE_ENQUE_REQ error!!!\n");
				return err;
			}
			ret = filp->f_op->unlocked_ioctl(filp,
				WPE_ENQUE_REQ, (unsigned long)data);

			err = compat_put_WPE_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_WPE_WPE_ENQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_WPE_DEQUE_REQ:
		{
			struct compat_WPE_Request __user *data32;
			struct WPE_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_WPE_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_WPE_WPE_DEQUE_REQ error!!!\n");
				return err;
			}
			ret = filp->f_op->unlocked_ioctl(filp,
				WPE_DEQUE_REQ, (unsigned long)data);

			err = compat_put_WPE_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_WPE_WPE_DEQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_WPE_DEQUE_DONE:
		{
			struct compat_WPE_Request __user *data32;
			struct WPE_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_WPE_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_WPE_WPE_DEQUE_DONE error!!!\n");
				return err;
			}
			ret = filp->f_op->unlocked_ioctl(filp,
				WPE_DEQUE_DONE, (unsigned long)data);

			err = compat_put_WPE_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_WPE_WPE_DEQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_WPE_WAIT_DEQUE:
	case WPE_WAIT_IRQ:
	case WPE_CLEAR_IRQ:	/* structure (no pointer) */
	case WPE_ENQNUE_NUM:
	case WPE_ENQUE:
	case WPE_DEQUE_NUM:
	case WPE_DEQUE:
	case WPE_RESET:
	case WPE_DUMP_REG:
	case WPE_DUMP_ISR_LOG:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
		/* return WPE_ioctl(filep, cmd, arg); */
	}
}

#endif

/***********************************************************************
 *
 ***********************************************************************/
static signed int WPE_open(struct inode *pInode, struct file *pFile)
{
	signed int Ret = 0;
	unsigned int i, j;
	unsigned long flags;
	/*int q = 0, p = 0; */
	struct WPE_USER_INFO_STRUCT *pUserInfo;

	LOG_DBG("- E. UserCount: %d.", WPEInfo.UserCount);


	/*  */
	spin_lock(&(WPEInfo.SpinLockWPERef));

	pFile->private_data = NULL;
	pFile->private_data = kmalloc(
		sizeof(struct WPE_USER_INFO_STRUCT), GFP_ATOMIC);

	if (pFile->private_data == NULL) {
		LOG_DBG(
			"ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)",
			current->comm,
			current->pid,
			current->tgid);
		Ret = -ENOMEM;
	} else {
		pUserInfo =
			(struct WPE_USER_INFO_STRUCT *) pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}
	/*  */
	if (WPEInfo.UserCount > 0) {
		WPEInfo.UserCount++;
		spin_unlock(&(WPEInfo.SpinLockWPERef));
		LOG_DBG(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			WPEInfo.UserCount, current->comm,
			current->pid, current->tgid);
		goto EXIT;
	} else {
		WPEInfo.UserCount++;
		spin_unlock(&(WPEInfo.SpinLockWPERef));
		LOG_DBG(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), first user",
			WPEInfo.UserCount,
			current->comm,
			current->pid,
			current->tgid);
	}

	/* do wait queue head init when re-enter in camera */
	/*  */
	for (i = 0; i < _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_; i++) {
		/* WPE */
		g_WPE_ReqRing.WPEReq_Struct[i].processID = 0x0;
		g_WPE_ReqRing.WPEReq_Struct[i].callerID = 0x0;
		g_WPE_ReqRing.WPEReq_Struct[i].enqueReqNum = 0x0;
		/* g_WPE_ReqRing.WPEReq_Struct[i].enqueIdx = 0x0; */
		g_WPE_ReqRing.WPEReq_Struct[i].State = WPE_REQUEST_STATE_EMPTY;
		g_WPE_ReqRing.WPEReq_Struct[i].FrameWRIdx = 0x0;
		g_WPE_ReqRing.WPEReq_Struct[i].FrameRDIdx = 0x0;
		for (j = 0; j < _SUPPORT_MAX_WPE_FRAME_REQUEST_; j++)
			g_WPE_ReqRing.WPEReq_Struct[i].WpeFrameStatus[j] =
			WPE_FRAME_STATUS_EMPTY;

	}
	g_WPE_ReqRing.WriteIdx = 0x0;
	g_WPE_ReqRing.ReadIdx = 0x0;
	g_WPE_ReqRing.HWProcessIdx = 0x0;

	/* Enable clock */
	WPE_EnableClock(MTRUE);
	g_u4WpeCnt = 0;
	LOG_INF("WPE open g_u4EnableClockCount: %d", g_u4EnableClockCount);
	/*  */

	spin_lock_irqsave(
		&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]), flags);

	for (i = 0; i < WPE_IRQ_TYPE_AMOUNT; i++)
		WPEInfo.IrqInfo.Status[i] = 0;
	spin_unlock_irqrestore(
		&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]), flags);

	for (i = 0; i < _SUPPORT_MAX_WPE_FRAME_REQUEST_; i++)
		WPEInfo.ProcessID[i] = 0;

	WPEInfo.WriteReqIdx = 0;
	WPEInfo.ReadReqIdx = 0;
	WPEInfo.IrqInfo.WpeIrqCnt = 0;

#define KERNEL_LOG
#ifdef KERNEL_LOG
	/* In EP, Add WPE_DBG_WRITE_REG for debug. Should remove it after EP */
	WPEInfo.DebugMask = (WPE_DBG_INT | WPE_DBG_DBGLOG | WPE_DBG_WRITE_REG);
#endif
	/*  */
EXIT:
	LOG_DBG("- X. Ret: %d. UserCount: %d.", Ret, WPEInfo.UserCount);
	return Ret;

}

/***********************************************************************
 *
 ***********************************************************************/
static signed int WPE_release(struct inode *pInode, struct file *pFile)
{
	struct WPE_USER_INFO_STRUCT *pUserInfo;
	/*MUINT32 Reg; */

	LOG_DBG("- E. UserCount: %d.", WPEInfo.UserCount);

	/*  */
	if (pFile->private_data != NULL) {
		pUserInfo =
			(struct WPE_USER_INFO_STRUCT *) pFile->private_data;

		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	/*  */
	spin_lock(&(WPEInfo.SpinLockWPERef));
	WPEInfo.UserCount--;

	if (WPEInfo.UserCount > 0) {
		spin_unlock(&(WPEInfo.SpinLockWPERef));
		LOG_DBG(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			WPEInfo.UserCount,
			current->comm,
			current->pid,
			current->tgid);
		goto EXIT;
	} else
		spin_unlock(&(WPEInfo.SpinLockWPERef));
	/*  */
	LOG_DBG(
	"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), last user",
		WPEInfo.UserCount,
		current->comm,
		current->pid,
		current->tgid);

	/* Disable clock. */
	WPE_EnableClock(MFALSE);
	LOG_INF("WPE release g_u4EnableClockCount: %d", g_u4EnableClockCount);

	/*  */
EXIT:

	LOG_DBG("- X. UserCount: %d.", WPEInfo.UserCount);
	return 0;
}


/***********************************************************************
 *
 ***********************************************************************/
static signed int WPE_mmap(
	struct file *pFile, struct vm_area_struct *pVma)
{
	unsigned long length = 0;
	unsigned int pfn = 0x0;

	length = pVma->vm_end - pVma->vm_start;
	/*  */
	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
	pfn = pVma->vm_pgoff << PAGE_SHIFT;

	LOG_INF("mmap:pVma->vm_pgoff(0x%lx),pfn(0x%x),phy(0x%lx)\n",
		pVma->vm_pgoff, pfn, pVma->vm_pgoff << PAGE_SHIFT);
	LOG_INF(
		"mmap:pVmapVma->vm_start(0x%lx),pVma->vm_end(0x%lx),length(0x%lx)\n",
		pVma->vm_start, pVma->vm_end, length);

	switch (pfn) {
	case WPE_BASE_HW:
		if (length > WPE_REG_RANGE) {
			LOG_ERR(
				"mmap range error :module:0x%x length(0x%lx),WPE_REG_RANGE(0x%x)!",
				pfn, length, WPE_REG_RANGE);
			return -EAGAIN;
		}
		break;
	default:
		LOG_ERR("Illegal starting HW addr for mmap!");
		return -EAGAIN;
	}
	if (remap_pfn_range
	    (pVma, pVma->vm_start, pVma->vm_pgoff,
		pVma->vm_end - pVma->vm_start, pVma->vm_page_prot)) {
		return -EAGAIN;
	}
	/*  */
	return 0;
}

/***********************************************************************
 *
 ***********************************************************************/

static dev_t WPEDevNo;
static struct cdev *pWPECharDrv;
static struct class *pWPEClass;

static const struct file_operations WPEFileOper = {
	.owner = THIS_MODULE,
	.open = WPE_open,
	.release = WPE_release,
	/* .flush   = mt_WPE_flush, */
	.mmap = WPE_mmap,
	.unlocked_ioctl = WPE_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = WPE_ioctl_compat,
#endif
};

/***********************************************************************
 *
 ***********************************************************************/
static inline void WPE_UnregCharDev(void)
{
	LOG_DBG("- E.");
	/*  */
	/* Release char driver */
	if (pWPECharDrv != NULL) {
		cdev_del(pWPECharDrv);
		pWPECharDrv = NULL;
	}
	/*  */
	unregister_chrdev_region(WPEDevNo, 1);
}

/***********************************************************************
 *
 ***********************************************************************/
static inline signed int WPE_RegCharDev(void)
{
	signed int Ret = 0;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = alloc_chrdev_region(&WPEDevNo, 0, 1, WPE_DEV_NAME);
	if (Ret < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d", Ret);
		return Ret;
	}
	/* Allocate driver */
	pWPECharDrv = cdev_alloc();
	if (pWPECharDrv == NULL) {
		LOG_ERR("cdev_alloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pWPECharDrv, &WPEFileOper);
	/*  */
	pWPECharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pWPECharDrv, WPEDevNo, 1);
	if (Ret < 0) {
		LOG_ERR("Attatch file operation failed, %d", Ret);
		goto EXIT;
	}
	/*  */
EXIT:
	if (Ret < 0)
		WPE_UnregCharDev();

	/*  */

	LOG_DBG("- X.");
	return Ret;
}

/***********************************************************************
 *
 ***********************************************************************/
static signed int WPE_probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	/*struct resource *pRes = NULL; */
	signed int i = 0;
	unsigned char n;
	unsigned int irq_info[3];/* Record interrupts info from device tree */
	struct device *dev = NULL;
	struct WPE_device *_wpe_dev;

#ifdef CONFIG_OF
	struct WPE_device *WPE_dev;
#endif

	LOG_INF("- E. WPE driver probe.");

	/* Check platform_device parameters */
#ifdef CONFIG_OF

	if (pDev == NULL) {
		LOG_ERR(&pDev->dev, "[ERROR]pDev is NULL");
		return -ENXIO;
	}

	nr_WPE_devs += 1;
	_wpe_dev = krealloc(WPE_devs,
		sizeof(struct WPE_device) * nr_WPE_devs, GFP_KERNEL);

	if (!_wpe_dev) {
		LOG_ERR(&pDev->dev, "[ERROR]Unable to allocate WPE_devs\n");
		return -ENOMEM;
	}
	WPE_devs = _wpe_dev;

	WPE_dev = &(WPE_devs[nr_WPE_devs - 1]);
	WPE_dev->dev = &pDev->dev;

	/* iomap registers */
	WPE_dev->regs = of_iomap(pDev->dev.of_node, 0);
	/* gISPSYS_Reg[nr_WPE_devs - 1] = WPE_dev->regs; */

	if (!WPE_dev->regs) {
		LOG_ERR(&pDev->dev,
			"[ERROR]Unable to ioremap registers, of_iomap fail, nr_WPE_devs=%d, devnode(%s).\n",
			nr_WPE_devs, pDev->dev.of_node->name);
		return -ENOMEM;
	}

	LOG_INF("nr_WPE_devs=%d, devnode(%s), map_addr=0x%lx\n", nr_WPE_devs,
		pDev->dev.of_node->name, (unsigned long)WPE_dev->regs);

	/* get IRQ ID and request IRQ */
	WPE_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);

	if (WPE_dev->irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array
		    (pDev->dev.of_node, "interrupts",
		    irq_info, ARRAY_SIZE(irq_info))) {
			LOG_ERR(&pDev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		for (i = 0; i < WPE_IRQ_TYPE_AMOUNT; i++) {
			if (strcmp(pDev->dev.of_node->name,
				WPE_IRQ_CB_TBL[i].device_name) == 0) {
				Ret =
				    request_irq(WPE_dev->irq,
					(irq_handler_t) WPE_IRQ_CB_TBL[i].
					isr_fp, irq_info[2],
					(const char *)WPE_IRQ_CB_TBL[i].
					device_name, NULL);

				if (Ret) {
					LOG_ERR(&pDev->dev,
						"[ERROR]Unable to request IRQ, request_irq fail, nr_WPE_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
						nr_WPE_devs,
						pDev->dev.of_node->name,
						WPE_dev->irq,
						WPE_IRQ_CB_TBL[i].device_name);
					return Ret;
				}

				LOG_INF(
					"nr_WPE_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
					nr_WPE_devs,
					pDev->dev.of_node->name,
					WPE_dev->irq,
					WPE_IRQ_CB_TBL[i].device_name);
				break;
			}
		}

		if (i >= WPE_IRQ_TYPE_AMOUNT) {
			LOG_INF(
				"No corresponding ISR!!: nr_WPE_devs=%d, devnode(%s), irq=%d\n",
				nr_WPE_devs,
				pDev->dev.of_node->name, WPE_dev->irq);
		}

	} else {
		LOG_INF("No IRQ!!: nr_WPE_devs=%d, devnode(%s), irq=%d\n",
			nr_WPE_devs,
			pDev->dev.of_node->name,
			WPE_dev->irq);
	}


#endif

	/* Only register char driver in the 1st time */
	if (nr_WPE_devs == 1) {

		/* Register char driver */
		Ret = WPE_RegCharDev();
		if (Ret) {
			LOG_ERR(&pDev->dev, "[ERROR]register char failed");
			return Ret;
		}
#ifndef __WPE_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
		wpe_clk.CG_IMGSYS_WPE_A =
			devm_clk_get(&pDev->dev, "WPE_CLK_IMG_WPE_A");
		LOG_INF("devm_clk_get WPE_CLK_IMG_WPE_A");

		if (IS_ERR(wpe_clk.CG_IMGSYS_WPE_A)) {
			LOG_ERR("cannot get CG_IMGSYS_WPE_A clock\n");
			return PTR_ERR(wpe_clk.CG_IMGSYS_WPE_A);
		}
#endif
#endif
		/* Create class register */
		pWPEClass = class_create(THIS_MODULE, "WPEdrv");
		if (IS_ERR(pWPEClass)) {
			Ret = PTR_ERR(pWPEClass);
			LOG_ERR("Unable to create class, err = %d", Ret);
			goto EXIT;
		}

		dev = device_create(pWPEClass, NULL,
					WPEDevNo, NULL, WPE_DEV_NAME);
		if (IS_ERR(dev)) {
			Ret = PTR_ERR(dev);
			LOG_ERR(&pDev->dev,
				"[ERROR]Failed to create device: /dev/%s, err = %d",
				WPE_DEV_NAME, Ret);
			goto EXIT;
		}

		/* Init spinlocks */
		spin_lock_init(&(WPEInfo.SpinLockWPERef));
		spin_lock_init(&(WPEInfo.SpinLockWPE));
		for (n = 0; n < WPE_IRQ_TYPE_AMOUNT; n++)
			spin_lock_init(&(WPEInfo.SpinLockIrq[n]));

		/*  */

		init_waitqueue_head(&WPEInfo.WaitDeque);
		init_waitqueue_head(&WPEInfo.WaitQueueHead);
		INIT_WORK(&WPEInfo.ScheduleWpeWork, WPE_ScheduleWork);

		wake_lock_init(&WPE_wake_lock,
			WAKE_LOCK_SUSPEND, "WPE_lock_wakelock");

		for (i = 0; i < WPE_IRQ_TYPE_AMOUNT; i++)
			tasklet_init(WPE_tasklet[i].pWPE_tkt,
				WPE_tasklet[i].tkt_cb, 0);


		/* Init WPEInfo */
		spin_lock(&(WPEInfo.SpinLockWPERef));
		WPEInfo.UserCount = 0;
		spin_unlock(&(WPEInfo.SpinLockWPERef));
		/*  */
		WPEInfo.IrqInfo.Mask[WPE_IRQ_TYPE_INT_WPE_ST] =
							INT_ST_MASK_WPE;
	}
	if (nr_WPE_devs == 2) {

#ifndef __WPE_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
		wpe_clk.CG_IMGSYS_WPE_B = devm_clk_get(&pDev->dev,
							"WPE_CLK_IMG_WPE_B");
		LOG_INF("devm_clk_get WPE_CLK_IMG_WPE_B");

		if (IS_ERR(wpe_clk.CG_IMGSYS_WPE_B)) {
			LOG_ERR("cannot get CG_IMGSYS_WPE_B clock\n");
			return PTR_ERR(wpe_clk.CG_IMGSYS_WPE_B);
		}
#endif
#endif
	}

EXIT:
	if (Ret < 0)
		WPE_UnregCharDev();


	LOG_INF("- X. WPE driver probe.");

	return Ret;
}

/***********************************************************************
 * Called when the device is being detached from the driver
 ***********************************************************************/
static signed int WPE_remove(struct platform_device *pDev)
{
	/*struct resource *pRes; */
	signed int IrqNum;
	int i;
	/*  */
	LOG_DBG("- E.");
	/* unregister char driver. */
	WPE_UnregCharDev();

	/* Release IRQ */
	disable_irq(WPEInfo.IrqNum);
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);

	/* kill tasklet */
	for (i = 0; i < WPE_IRQ_TYPE_AMOUNT; i++)
		tasklet_kill(WPE_tasklet[i].pWPE_tkt);
#if 0
	/* free all registered irq(child nodes) */
	WPE_UnRegister_AllregIrq();
	/* free father nodes of irq user list */
	struct my_list_head *head;
	struct my_list_head *father;

	head = ((struct my_list_head *)(&SupIrqUserListHead.list));
	while (1) {
		father = head;
		if (father->nextirq != father) {
			father = father->nextirq;
			REG_IRQ_NODE *accessNode;

			typeof(((REG_IRQ_NODE *) 0)->list) * __mptr = (father);
			accessNode =
			    ((REG_IRQ_NODE *)
			    ((char *)__mptr - offsetof(REG_IRQ_NODE, list)));

			LOG_INF("free father,reg_T(%d)\n", accessNode->reg_T);
			if (father->nextirq != father) {
				head->nextirq = father->nextirq;
				father->nextirq = father;
			} else {	/* last father node */
				head->nextirq = head;
				LOG_INF("break\n");
				break;
			}
			kfree(accessNode);
		}
	}
#endif
	/*  */
	device_destroy(pWPEClass, WPEDevNo);
	/*  */
	class_destroy(pWPEClass);
	pWPEClass = NULL;
	/*  */
	return 0;
}

/***********************************************************************
 *
 ***********************************************************************/
static signed int bPass1_On_In_Resume_TG1;

static signed int WPE_suspend(
		struct platform_device *pDev, pm_message_t Mesg)
{
	/*signed int ret = 0; */

	LOG_DBG("bPass1_On_In_Resume_TG1(%d)\n", bPass1_On_In_Resume_TG1);

	bPass1_On_In_Resume_TG1 = 0;

	if (g_u4EnableClockCount > 0) {
		WPE_EnableClock(MFALSE);
		g_u4WpeCnt++;
	}


	return 0;
}

/***********************************************************************
 *
 ***********************************************************************/
static signed int WPE_resume(struct platform_device *pDev)
{
	LOG_DBG("bPass1_On_In_Resume_TG1(%d).\n", bPass1_On_In_Resume_TG1);
	if (g_u4WpeCnt > 0) {
		WPE_EnableClock(MTRUE);
		g_u4WpeCnt--;
	}

	return 0;
}

/*---------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------*/
int WPE_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	LOG_DBG("calling %s()\n", __func__);
	LOG_DBG("WPE suspend g_u4EnableClockCount: %d, g_u4WpeCnt: %d",
		g_u4EnableClockCount, g_u4WpeCnt);

	return WPE_suspend(pdev, PMSG_SUSPEND);
}

int WPE_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	LOG_DBG("calling %s()\n", __func__);
	LOG_DBG("WPE resume g_u4EnableClockCount: %d, g_u4WpeCnt: %d",
		g_u4EnableClockCount, g_u4WpeCnt);

	return WPE_resume(pdev);
}

#ifndef CONFIG_OF
/* extern void mt_irq_set_sens(unsigned int irq, unsigned int sens); */
/* extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity); */
#endif
int WPE_pm_restore_noirq(struct device *device)
{
	pr_debug("calling %s()\n", __func__);
#ifndef CONFIG_OF
/*	mt_irq_set_sens(WPE_IRQ_BIT_ID, MT_LEVEL_SENSITIVE); */
/*	mt_irq_set_polarity(WPE_IRQ_BIT_ID, MT_POLARITY_LOW); */
#endif
	return 0;

}

/*---------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------*/
#define WPE_pm_suspend NULL
#define WPE_pm_resume  NULL
#define WPE_pm_restore_noirq NULL
/*---------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------*/
#ifdef CONFIG_OF
/*
 * Note!!! The order and member of .compatible must be the same with that in
 *  "WPE_DEV_NODE_ENUM" in camera_WPE.h
 */
static const struct of_device_id WPE_of_ids[] = {
	{.compatible = "mediatek,wpe_a",},
	{.compatible = "mediatek,wpe_b",},
	{}
};
#endif

const struct dev_pm_ops WPE_pm_ops = {
	.suspend = WPE_pm_suspend,
	.resume = WPE_pm_resume,
	.freeze = WPE_pm_suspend,
	.thaw = WPE_pm_resume,
	.poweroff = WPE_pm_suspend,
	.restore = WPE_pm_resume,
	.restore_noirq = WPE_pm_restore_noirq,
};


/***********************************************************************
 *
 ***********************************************************************/
static struct platform_driver WPEDriver = {
	.probe = WPE_probe,
	.remove = WPE_remove,
	.suspend = WPE_suspend,
	.resume = WPE_resume,
	.driver = {
		   .name = WPE_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = WPE_of_ids,
#endif
#ifdef CONFIG_PM
		   .pm = &WPE_pm_ops,
#endif
		}
};


static int wpe_dump_read(struct seq_file *m, void *v)
{
	int i, j;

	seq_puts(m, "\n============ wpe dump register============\n");
	seq_puts(m, "WPE Config Info\n");

	for (i = 0x400; i < 0x4A8; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(WPE_BASE_HW + i),
			(unsigned int)WPE_RD32(ISP_WPE_BASE + i));
	}
	seq_puts(m, "WPE Debug Info\n");
	for (i = 0x4A8; i < 0x4B8; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(WPE_BASE_HW + i),
			(unsigned int)WPE_RD32(ISP_WPE_BASE + i));
	}

	seq_puts(m, "\n");
	seq_printf(m, "WPE Clock Count:%d\n", g_u4EnableClockCount);

	/*seq_printf(m, "[0x%08X %08X]\n", */
		/*(unsigned int)(DMA_DEBUG_DATA_HW),*/
		  /* (unsigned int)WPE_RD32(DMA_DEBUG_DATA_REG));*/
	/*seq_printf(m, "[0x%08X %08X]\n",*/
		/*(unsigned int)(DMA_DCM_ST_HW),*/
		   /*(unsigned int)WPE_RD32(DMA_DCM_ST_REG));*/
	/*seq_printf(m, "[0x%08X %08X]\n",*/
		/*(unsigned int)(DMA_RDY_ST_HW),*/
		  /* (unsigned int)WPE_RD32(DMA_RDY_ST_REG));*/
	/*seq_printf(m, "[0x%08X %08X]\n", */
		/*(unsigned int)(DMA_REQ_ST_HW),*/
		   /*(unsigned int)WPE_RD32(DMA_REQ_ST_REG));*/

	seq_printf(m, "WPE:HWProcessIdx:%d, WriteIdx:%d, ReadIdx:%d\n",
		   g_WPE_ReqRing.HWProcessIdx,
		   g_WPE_ReqRing.WriteIdx,
		   g_WPE_ReqRing.ReadIdx);

	for (i = 0; i < _SUPPORT_MAX_WPE_REQUEST_RING_SIZE_; i++) {
		seq_printf(m,
			   "WPE:State:%d, processID:0x%08X, callerID:0x%08X, enqueReqNum:%d, FrameWRIdx:%d, FrameRDIdx:%d\n",
			   g_WPE_ReqRing.WPEReq_Struct[i].State,
			   g_WPE_ReqRing.WPEReq_Struct[i].processID,
			   g_WPE_ReqRing.WPEReq_Struct[i].callerID,
			   g_WPE_ReqRing.WPEReq_Struct[i].enqueReqNum,
			   g_WPE_ReqRing.WPEReq_Struct[i].FrameWRIdx,
			   g_WPE_ReqRing.WPEReq_Struct[i].FrameRDIdx);

		for (j = 0; j < _SUPPORT_MAX_WPE_FRAME_REQUEST_;) {
			seq_printf(m,
				   "WPE:FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d\n",
				   j,
				   g_WPE_ReqRing.WPEReq_Struct[i].
				   WpeFrameStatus[j],
				   j + 1,
				   g_WPE_ReqRing.WPEReq_Struct[i].
				   WpeFrameStatus[j + 1],
				   j + 2,
				   g_WPE_ReqRing.WPEReq_Struct[i].
				   WpeFrameStatus[j + 2],
				   j + 3,
				   g_WPE_ReqRing.WPEReq_Struct[i].
				   WpeFrameStatus[j + 3]);
			j = j + 4;
		}
	}

	seq_puts(m, "\n============ WPE dump debug ============\n");

	return 0;
}


static int proc_wpe_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, wpe_dump_read, NULL);
}

static const struct file_operations WPE_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_wpe_dump_open,
	.read = seq_read,
};


static int wpe_reg_read(struct seq_file *m, void *v)
{
	unsigned int i;

	seq_puts(m, "======== read wpe register ========\n");

	for (i = 0x400; i <= 0x4B4; i = i + 4) {
		seq_printf(m, "[0x%08X 0x%08X]\n",
			(unsigned int)(WPE_BASE_HW + i),
			(unsigned int)WPE_RD32(ISP_WPE_BASE + i));
	}

	return 0;
}

/*static int WPE_reg_write*/
/*(struct file *file, const char __user *buffer, size_t count, loff_t *data)*/

static ssize_t wpe_reg_write(
		struct file *file, const char __user *buffer, size_t count,
		loff_t *data)
{
	char desc[128];
	int len = 0;
	/*char *pEnd; */
	char addrSzBuf[24];
	char valSzBuf[24];
	char *pszTmp;
	int addr = 0, val = 0;
	long int tempval;

	if (WPEInfo.UserCount <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%23s %23s", addrSzBuf, valSzBuf) == 2) {
		pszTmp = strstr(addrSzBuf, "0x");
		if (pszTmp == NULL) {
			/*if (sscanf(addrSzBuf, "%d", &addr) != 1) */
			if (kstrtol(addrSzBuf, 10, (long int *)&tempval) != 0)
				LOG_ERR("scan decimal addr is wrong !!:%s",
				addrSzBuf);
		} else {
			if (strlen(addrSzBuf) > 2) {
				if (sscanf(addrSzBuf + 2, "%x", &addr) != 1)
					LOG_ERR(
					"scan hexadecimal addr is wrong !!:%s",
					addrSzBuf);
			} else {
				LOG_INF("WPE Write Addr Error!!:%s", addrSzBuf);
			}
		}

		pszTmp = strstr(valSzBuf, "0x");
		if (pszTmp == NULL) {
			/*if (sscanf(valSzBuf, "%d", &val) != 1) */
			if (kstrtol(valSzBuf, 10, (long int *)&tempval) != 0)
				LOG_ERR(
				"scan decimal value is wrong !!:%s",
				valSzBuf);
		} else {
			if (strlen(valSzBuf) > 2) {
				if (sscanf(valSzBuf + 2, "%x", &val) != 1)
					LOG_ERR(
					"scan hexadecimal value is wrong !!:%s",
					valSzBuf);
			} else {
				LOG_INF("WPE Write Value Error!!:%s\n",
					valSzBuf);
			}
		}

		if ((addr >= WPE_BASE_HW) && (addr <= WPE_DMA_DEBUG_SEL_HW)
			&& ((addr & 0x3) == 0)) {
			LOG_INF("Write Request - addr:0x%x, value:0x%x\n",
				addr, val);
			WPE_WR32((ISP_WPE_BASE + (addr - WPE_BASE_HW)), val);
		} else {
			LOG_INF(
				"Write-Address Range exceeds the size of hw wpe!! addr:0x%x, value:0x%x\n",
			     addr, val);
		}

	} else if (sscanf(desc, "%23s", addrSzBuf)
								== 1) {
		pszTmp = strstr(addrSzBuf, "0x");
		if (pszTmp == NULL) {
			/*if (1 != sscanf(addrSzBuf, "%d", &addr)) */
			if (kstrtol(addrSzBuf, 10, (long int *)&tempval) != 0)
				LOG_ERR("scan decimal addr is wrong !!:%s",
				addrSzBuf);
			else
				addr = tempval;
		} else {
			if (strlen(addrSzBuf) > 2) {
				if (sscanf(addrSzBuf + 2, "%x", &addr) != 1)
					LOG_ERR(
					"scan hexadecimal addr is wrong !!:%s",
					addrSzBuf);
			} else {
				LOG_INF("WPE Read Addr Error!!:%s", addrSzBuf);
			}
		}

		if ((addr >= WPE_BASE_HW) && (addr <= WPE_DMA_DEBUG_SEL_HW)
			&& ((addr & 0x3) == 0)) {
			val = WPE_RD32((ISP_WPE_BASE + (addr - WPE_BASE_HW)));
			LOG_INF("Read Request - addr:0x%x,value:0x%x\n",
				addr, val);
		} else {
			LOG_INF(
				"Read-Address Range exceeds the size of hw WPE!! addr:0x%x, value:0x%x\n",
			     addr, val);
		}

	}


	return count;
}

static int proc_wpe_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, wpe_reg_read, NULL);
}

static const struct file_operations WPE_reg_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_wpe_reg_open,
	.read = seq_read,
	.write = wpe_reg_write,
};


/***********************************************************************
 *
 ***********************************************************************/

int32_t WPE_ClockOnCallback(uint64_t engineFlag)
{
	/* LOG_DBG("WPE_ClockOnCallback"); */
	/* LOG_DBG("+CmdqEn:%d", g_u4EnableClockCount); */
	/* WPE_EnableClock(MTRUE); */
	WPE_EnableClock(1);
	return 0;
}

int32_t WPE_DumpCallback(uint64_t engineFlag, int level)
{
	LOG_DBG("DumpCallback");

	WPE_DumpReg();

	return 0;
}

int32_t WPE_ResetCallback(uint64_t engineFlag)
{
	LOG_DBG("ResetCallback");
	WPE_Reset();

	return 0;
}

int32_t WPE_ClockOffCallback(uint64_t engineFlag)
{
	/* LOG_DBG("WPE_ClockOffCallback"); */
	/* WPE_EnableClock(MFALSE); */
	/* LOG_DBG("-CmdqEn:%d", g_u4EnableClockCount); */
	WPE_EnableClock(0);
	return 0;
}


static signed int __init WPE_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
	/* FIX-ME: linux-3.10 procfs API changed */
	/* use proc_create */
	struct proc_dir_entry *proc_entry;
	struct proc_dir_entry *isp_wpe_dir;


	int i;
	/*  */
	LOG_INF("- E.");
	/*  */
	Ret = platform_driver_register(&WPEDriver);
	if (Ret < 0) {
		LOG_ERR("platform_driver_register fail");
		return Ret;
	}
#if 0
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,WPE");
	if (!node) {
		LOG_ERR("find mediatek,WPE node failed!!!\n");
		return -ENODEV;
	}
	ISP_WPE_BASE = of_iomap(node, 0);
	if (!ISP_WPE_BASE) {
		LOG_ERR("unable to map ISP_WPE_BASE registers!!!\n");
		return -ENODEV;
	}
	LOG_DBG("ISP_WPE_BASE: %lx\n", ISP_WPE_BASE);
#endif

	isp_wpe_dir = proc_mkdir("wpe", NULL);
	if (!isp_wpe_dir) {
		LOG_ERR("[%s]: fail to mkdir /proc/wpe\n", __func__);
		return 0;
	}

	/* proc_entry = */
	/*	proc_create("pll_test", S_IRUGO | */
	/*		S_IWUSR, isp_wpe_dir, &pll_test_proc_fops); */

	proc_entry = proc_create("wpe_dump", 0444, isp_wpe_dir,
					&WPE_dump_proc_fops);

	proc_entry = proc_create("wpe_reg", 0644, isp_wpe_dir,
					&WPE_reg_proc_fops);


	/* isr log */
	if (PAGE_SIZE <
	    ((WPE_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
	    ((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM)) {
		i = 0;
		while (i <
		       ((WPE_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
			 ((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) *
			 LOG_PPNUM)) {
			i += PAGE_SIZE;
		}
	} else {
		i = PAGE_SIZE;
	}
	pLog_kmalloc = kmalloc(i, GFP_KERNEL);
	if (pLog_kmalloc == NULL) {
		LOG_ERR("log mem not enough\n");
		return -ENOMEM;
	}
	memset(pLog_kmalloc, 0x00, i);
	tmp = pLog_kmalloc;
	for (i = 0; i < LOG_PPNUM; i++) {
		for (j = 0; j < WPE_IRQ_TYPE_AMOUNT; j++) {
			gSvLog[j]._str[i][_LOG_DBG] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp + 8*/
			/*(NORMAL_STR_LEN*DBG_PAGE)); */
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * DBG_PAGE));
			gSvLog[j]._str[i][_LOG_INF] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp +*/
				/*(NORMAL_STR_LEN*INF_PAGE)); */
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * INF_PAGE));
			gSvLog[j]._str[i][_LOG_ERR] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp + */
				/*(NORMAL_STR_LEN*ERR_PAGE)); */
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * ERR_PAGE));
		}
		/* tmp = (void*) ((unsigned int)tmp + NORMAL_STR_LEN); */
		/*log buffer ,in case of overflow */
		tmp = (void *)((char *)tmp + NORMAL_STR_LEN);
		/* log buffer ,in case of overflow */
	}

#if 1
	/* Cmdq */
	/* Register WPE callback */
	LOG_DBG("register wpe callback for CMDQ");
	cmdqCoreRegisterCB(CMDQ_GROUP_WPE,
			   WPE_ClockOnCallback,
			   WPE_DumpCallback,
			   WPE_ResetCallback,
			   WPE_ClockOffCallback);
#endif

	LOG_DBG("- X. Ret: %d.", Ret);
	return Ret;
}

/***********************************************************************
 *
 ***********************************************************************/
static void __exit WPE_Exit(void)
{
	/*int i; */

	LOG_DBG("- E.");
	/*  */
	platform_driver_unregister(&WPEDriver);
	/*  */
#if 1
	/* Cmdq */
	/* Unregister WPE callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_WPE, NULL, NULL, NULL, NULL);
#endif

	kfree(pLog_kmalloc);

	/*  */
}

/***********************************************************************
 *
 ***********************************************************************/
static void WPE_ScheduleWork(struct work_struct *data)
{
	if (WPE_DBG_DBGLOG & WPEInfo.DebugMask)
		LOG_DBG("- E.");

#ifdef WPE_USE_GCE
#else
	ConfigWPE();
#endif
}


static irqreturn_t ISP_Irq_WPE(signed int Irq, void *DeviceId)
{

#ifdef WPE_USE_GCE_IRQ

	unsigned int WPEStatus;
	bool bResulst = 0;
	pid_t ProcessID;

	WPEStatus = WPE_RD32(WPE_CTL_INT_STATUS_REG);	/* WPE Status */
	LOG_INF("WPE INTERRUPT Handler reads WPE_INT_STATUS(0x%x)\n",
		WPEStatus);

	spin_lock(&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]));

	if (WPE_INT_ST == (WPE_INT_ST & WPEStatus)) {
		/* Update the frame status. */
#ifdef __WPE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("WPE_irq");
#endif

#ifndef WPE_USE_GCE
		WPE_WR32(WPE_WPE_START_REG, 0x0);/* WPE DCM Start Disable */
#endif
		/* WPE_WR32(WPE_IRQ_REG, 1); *//* For write 1 clear */
		bResulst = UpdateWPE(&ProcessID);
		/* ConfigWPE(); */
		if (bResulst == 1) {
			schedule_work(&WPEInfo.ScheduleWpeWork);
#ifdef WPE_USE_GCE
			WPEInfo.IrqInfo.Status[WPE_IRQ_TYPE_INT_WPE_ST] |=
								WPE_INT_ST;
			WPEInfo.IrqInfo.ProcessID[WPE_PROCESS_ID_WPE] =
								ProcessID;
			WPEInfo.IrqInfo.WpeIrqCnt++;
			WPEInfo.ProcessID[WPEInfo.WriteReqIdx] = ProcessID;
			WPEInfo.WriteReqIdx =
			    (WPEInfo.WriteReqIdx + 1) %
					_SUPPORT_MAX_WPE_FRAME_REQUEST_;
#ifdef WPE_MULTIPROCESS_TIMEING_ISSUE
			/* check the write value is equal to read value ? */
			/* actually, it doesn't happen!! */
			if (WPEInfo.WriteReqIdx == WPEInfo.ReadReqIdx) {
				IRQ_LOG_KEEPER(WPE_IRQ_TYPE_INT_WPE_ST,
					m_CurrentPPB, _LOG_ERR,
					"Irq Err!!, WriteReqIdx:0x%x, ReadReqIdx:0x%x\n",
					       WPEInfo.WriteReqIdx,
					       WPEInfo.ReadReqIdx);
			}
#endif

#else
			WPEInfo.IrqInfo.Status[WPE_IRQ_TYPE_INT_WPE_ST] |=
								WPE_INT_ST;
			WPEInfo.IrqInfo.ProcessID[WPE_PROCESS_ID_WPE] =
								ProcessID;
#endif
		}
#ifdef __WPE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
		/* Config the Next frame */
	}
	spin_unlock(&(WPEInfo.SpinLockIrq[WPE_IRQ_TYPE_INT_WPE_ST]));
	if (bResulst == MTRUE)
		wake_up_interruptible(&WPEInfo.WaitQueueHead);


	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(WPE_IRQ_TYPE_INT_WPE_ST, m_CurrentPPB, _LOG_INF,
		       "Irq:%d, reg 0x%x : 0x%x, bResulst:%d, wpeHWSta:0x%x, wpeIrqCnt:0x%x, WriteReqIdx:0x%x, ReadReqIdx:0x%x\n",
		       Irq, WPE_CTL_INT_STATUS_HW,
		       WPEStatus,
		       bResulst,
		       WPEStatus,
		       WPEInfo.IrqInfo.WpeIrqCnt,
		       WPEInfo.WriteReqIdx,
		       WPEInfo.ReadReqIdx);
	/* IRQ_LOG_KEEPER(WPE_IRQ_TYPE_INT_WPE_ST, m_CurrentPPB, _LOG_INF,
	 *"DveHWSta:0x%x, WPEHWSta:0x%x,DpeDveSta0:0x%x\n",
	 * DveStatus, WPEStatus, DpeDveSta0);
	 */

	if (WPEStatus & WPE_INT_ST)
		tasklet_schedule(WPE_tasklet[WPE_IRQ_TYPE_INT_WPE_ST].pWPE_tkt);
#endif
	return IRQ_HANDLED;
}

static void ISP_TaskletFunc_WPE(unsigned long data)
{
	IRQ_LOG_PRINTER(WPE_IRQ_TYPE_INT_WPE_ST, m_CurrentPPB, _LOG_DBG);
	IRQ_LOG_PRINTER(WPE_IRQ_TYPE_INT_WPE_ST, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(WPE_IRQ_TYPE_INT_WPE_ST, m_CurrentPPB, _LOG_ERR);

}


/***********************************************************************
 *
 ***********************************************************************/
module_init(WPE_Init);
module_exit(WPE_Exit);
MODULE_DESCRIPTION("Camera WPE driver");
MODULE_AUTHOR("MM3SW5");
MODULE_LICENSE("GPL");
