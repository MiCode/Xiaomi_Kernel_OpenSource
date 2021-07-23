/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <linux/sched/clock.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include "smi_public.h"
/*#include <linux/xlog.h>		 For xlog_printk(). */
/*  */
/*#include <mach/hardware.h>*/
/* #include <mach/mt6593_pll.h> */
#include "camera_owe.h"
#include "engine_request.h"
/*#include <mach/irqs.h>*/
/* #include <mach/mt_reg_base.h> */

#include <mt-plat/sync_write.h>	/* For mt65xx_reg_sync_writel(). */
/* #include <mach/mt_spm_idle.h> For spm_enable_sodi()/spm_disable_sodi(). */

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <m4u.h>
#include <cmdq_core.h>
#include <cmdq_record.h>

/* #define __OWE_EP_NO_CLKMGR__ */
/* #define BYPASS_REG */
/* Measure the kernel performance */
/* #define __OWE_KERNEL_PERFORMANCE_MEASURE__ */
#ifdef __OWE_KERNEL_PERFORMANCE_MEASURE__
#include <linux/met_drv.h>
#include <linux/mtk_ftrace.h>
#endif
#if 0
/* Another Performance Measure Usage */
#include <linux/kallsyms.h>
#include <linux/ftrace_event.h>
static unsigned long __read_mostly tracing_mark_write_addr;
#define _kernel_trace_begin(name) {\
	tracing_mark_write_addr =\
		kallsyms_lookup_name("tracing_mark_write");\
	event_trace_printk(tracing_mark_write_addr,\
		"B|%d|%s\n", current->tgid, name);\
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

#include <linux/pm_wakeup.h>

/* OWE Command Queue */
/* #include "../../cmdq/mt6797/cmdq_record.h" */
/* #include "../../cmdq/mt6797/cmdq_core.h" */

/* CCF */
#ifndef __OWE_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#include <linux/clk.h>
struct OWE_CLK_STRUCT {
	struct clk *CG_IMGSYS_OWE;
};
struct OWE_CLK_STRUCT owe_clk;
#endif	/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */
#endif
/*  */
#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif

#define OWE_DEV_NAME                "camera-owe"

/* #define OWE_WAITIRQ_LOG  */
#define OWE_USE_GCE
/* #define OWE_DEBUG_USE */
/* #define OWE_MULTIPROCESS_TIMEING_ISSUE  */
/*I can' test the situation in FPGA, because the velocity of FPGA is so slow. */
#define MyTag "[OWE]"
#define IRQTag "KEEPER"

#define LOG_VRB(format,	args...)    pr_debug(MyTag format, ##args)

#ifdef OWE_DEBUG_USE
#define LOG_DBG(format, args...)    pr_info(MyTag format, ##args)
#else
#define LOG_DBG(format, args...)
#endif

#define LOG_INF(format, args...)    pr_info(MyTag format,  ##args)
#define LOG_NOTICE(format, args...) pr_notice(MyTag format,  ##args)
#define LOG_WRN(format, args...)    pr_info(MyTag format,  ##args)
#define LOG_ERR(format, args...)    pr_info(MyTag format,  ##args)
#define LOG_AST(format, args...)    pr_info(MyTag format, ##args)


/******************************************************************************
 *
 ******************************************************************************/
/* #define OWE_WR32(addr, data)  iowrite32(data, addr) // For other projects. */
/* For 89 Only.   // NEED_TUNING_BY_PROJECT */
#define OWE_WR32(addr, data)    mt_reg_sync_writel(data, addr)
#define OWE_RD32(addr)          ioread32(addr)
/******************************************************************************
 *
 ******************************************************************************/
/* dynamic log level */
#define OWE_DBG_DBGLOG              (0x00000001)
#define OWE_DBG_INFLOG              (0x00000002)
#define OWE_DBG_INT                 (0x00000004)
#define OWE_DBG_READ_REG            (0x00000008)
#define OWE_DBG_WRITE_REG           (0x00000010)
#define OWE_DBG_TASKLET             (0x00000020)


/* ///////////////////////////////////////////////////////////////// */

/******************************************************************************
 *
 ******************************************************************************/

/* CAM interrupt status */
/* normal siganl */
#define OWE_INT_ST           (1<<0)
#define OCC_INT_ST           (1<<0)
#define WMFE_INT_ST          (1<<0)


/* IRQ signal mask */

#define INT_ST_MASK_OWE     ( \
			OWE_INT_ST)

#define CMDQ_REG_MASK 0xffffffff
#define OCC_START       0x1
#define WMFE_START      0x1

#define WMFE_ENABLE     0x1

#define OCC_IS_BUSY     0x1
#define WMFE_IS_BUSY    0x2


/* static irqreturn_t OWE_Irq_CAM_A(signed int  Irq,void *DeviceId); */
static irqreturn_t ISP_Irq_OWE(signed int Irq, void *DeviceId);
static void OWE_ScheduleOccWork(struct work_struct *data);
static void OWE_ScheduleWmfeWork(struct work_struct *data);



typedef irqreturn_t(*IRQ_CB) (signed int, void *);

struct ISR_TABLE {
	IRQ_CB isr_fp;
	unsigned int int_number;
	char device_name[16];
};

#ifndef CONFIG_OF
const struct ISR_TABLE OWE_IRQ_CB_TBL[OWE_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_OWE, OWE_IRQ_BIT_ID, "owe"},
};

#else
/* int number is got from kernel api */
const struct ISR_TABLE OWE_IRQ_CB_TBL[OWE_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_OWE, 0, "owe"},
};

#endif
/* ////////////////////////////////////////////////////////////////////////// */
/*  */
typedef void (*tasklet_cb) (unsigned long);
struct Tasklet_table {
	tasklet_cb tkt_cb;
	struct tasklet_struct *pOWE_tkt;
};

struct tasklet_struct Owetkt[OWE_IRQ_TYPE_AMOUNT];

static void ISP_TaskletFunc_OWE(unsigned long data);

static struct Tasklet_table OWE_tasklet[OWE_IRQ_TYPE_AMOUNT] = {
	{ISP_TaskletFunc_OWE, &Owetkt[OWE_IRQ_TYPE_INT_OWE_ST]},
};

struct wakeup_source OWE_wake_lock;

static DEFINE_MUTEX(gOweOccMutex);
static DEFINE_MUTEX(gOweOccDequeMutex);

static DEFINE_MUTEX(gOweWmfeMutex);
static DEFINE_MUTEX(gOweWmfeDequeMutex);

#ifdef CONFIG_OF

struct OWE_device {
	void __iomem *regs;
	struct device *dev;
	int irq;
};

static struct OWE_device *OWE_devs;
static int nr_OWE_devs;

/* Get HW modules' base address from device nodes */
#define OWE_DEV_NODE_IDX 0

/* static unsigned long gISPSYS_Reg[OWE_IRQ_TYPE_AMOUNT]; */


#define ISP_OWE_BASE                  (OWE_devs[OWE_DEV_NODE_IDX].regs)
/* #define ISP_OWE_BASE                  (gISPSYS_Reg[OWE_DEV_NODE_IDX]) */



#else
#define ISP_OWE_BASE                        (IMGSYS_BASE + 0x2800)

#endif


static unsigned int g_u4EnableClockCount;
static unsigned int g_SuspendCnt;

/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32


enum OWE_FRAME_STATUS_ENUM {
	OWE_FRAME_STATUS_EMPTY,	/* 0 */
	OWE_FRAME_STATUS_ENQUE,	/* 1 */
	OWE_FRAME_STATUS_RUNNING,	/* 2 */
	OWE_FRAME_STATUS_FINISHED,	/* 3 */
	OWE_FRAME_STATUS_TOTAL
};


enum OWE_REQUEST_STATE_ENUM {
	OWE_REQUEST_STATE_EMPTY,	/* 0 */
	OWE_REQUEST_STATE_PENDING,	/* 1 */
	OWE_REQUEST_STATE_RUNNING,	/* 2 */
	OWE_REQUEST_STATE_FINISHED,	/* 3 */
	OWE_REQUEST_STATE_TOTAL
};


struct OCC_REQUEST_STRUCT {
	enum OWE_REQUEST_STATE_ENUM RequestState;
	pid_t processID;	/* caller process ID */
	unsigned int callerID;	/* caller thread ID */
	unsigned int enqueReqNum;/* to judge it belongs to which frame package*/
	signed int FrameWRIdx;	/* Frame write Index */
	signed int FrameRDIdx;	/* Frame read Index */
	enum OWE_FRAME_STATUS_ENUM
			OccFrameStatus[_SUPPORT_MAX_OWE_FRAME_REQUEST_];
	struct OWE_OCCConfig OccFrameConfig[_SUPPORT_MAX_OWE_FRAME_REQUEST_];
};

struct OCC_REQUEST_RING_STRUCT {
	signed int WriteIdx;	/* enque how many request  */
	signed int ReadIdx;		/* read which request index */
	signed int HWProcessIdx;	/* HWWriteIdx */
	struct OCC_REQUEST_STRUCT
			OCCReq_Struct[_SUPPORT_MAX_OWE_REQUEST_RING_SIZE_];
};

struct OCC_CONFIG_STRUCT {
	struct OWE_OCCConfig OccFrameConfig[_SUPPORT_MAX_OWE_FRAME_REQUEST_];
};


static struct OCC_REQUEST_RING_STRUCT g_OCC_RequestRing;
static struct OCC_CONFIG_STRUCT g_OccEnqueReq_Struct;
static struct OCC_CONFIG_STRUCT g_OccDequeReq_Struct;


struct WMFE_REQUEST_STRUCT {
	enum OWE_REQUEST_STATE_ENUM RequestState;
	pid_t processID;	/* caller process ID */
	unsigned int callerID;	/* caller thread ID */
	unsigned int enqueReqNum;/* to judge it belongs to which frame package*/
	signed int FrameWRIdx;	/* Frame write Index */
	signed int FrameRDIdx;	/* Frame read Index */
	enum OWE_FRAME_STATUS_ENUM
		WmfeFrameStatus[_SUPPORT_MAX_OWE_FRAME_REQUEST_];
	struct OWE_WMFEConfig WmfeFrameConfig[_SUPPORT_MAX_OWE_FRAME_REQUEST_];
};

struct WMFE_REQUEST_RING_STRUCT {
	signed int WriteIdx;	/* enque how many request  */
	signed int ReadIdx;		/* read which request index */
	signed int HWProcessIdx;	/* HWWriteIdx */
	struct WMFE_REQUEST_STRUCT
		WMFEReq_Struct[_SUPPORT_MAX_OWE_REQUEST_RING_SIZE_];
};

struct WMFE_CONFIG_STRUCT {
	struct OWE_WMFEConfig WmfeFrameConfig[_SUPPORT_MAX_OWE_FRAME_REQUEST_];
};

static struct WMFE_REQUEST_RING_STRUCT g_WMFE_ReqRing;
static struct WMFE_CONFIG_STRUCT g_WmfeEnqueReq_Struct;
static struct WMFE_CONFIG_STRUCT g_WmfeDequeReq_Struct;
static struct engine_requests wmfe_reqs;
static struct OWE_WMFERequest kWmfeReq;

static struct engine_requests occ_reqs;
static struct OWE_OCCRequest kOccReq;


/******************************************************************************
 *
 ******************************************************************************/
struct OWE_USER_INFO_STRUCT {
	pid_t Pid;
	pid_t Tid;
};
enum OWE_PROCESS_ID_ENUM {
	OWE_PROCESS_ID_NONE,
	OWE_PROCESS_ID_OCC,
	OWE_PROCESS_ID_WMFE,
	OWE_PROCESS_ID_AMOUNT
};


/******************************************************************************
 *
 ******************************************************************************/
struct OWE_IRQ_INFO_STRUCT {
	unsigned int Status[OWE_IRQ_TYPE_AMOUNT];
	signed int OccIrqCnt;
	signed int WmfeIrqCnt;
	pid_t ProcessID[OWE_PROCESS_ID_AMOUNT];
	unsigned int Mask[OWE_IRQ_TYPE_AMOUNT];
};


struct OWE_INFO_STRUCT {
	spinlock_t SpinLockOWERef;
	spinlock_t SpinLockOWE;
	spinlock_t SpinLockIrq[OWE_IRQ_TYPE_AMOUNT];
	wait_queue_head_t WaitQueueHead;
	struct work_struct ScheduleOccWork;
	struct work_struct ScheduleWmfeWork;
	struct workqueue_struct *wkqueue;
	unsigned int UserCount;	/* User Count */
	unsigned int DebugMask;	/* Debug Mask */
	signed int IrqNum;
	struct OWE_IRQ_INFO_STRUCT IrqInfo;
	signed int WriteReqIdx;
	signed int ReadReqIdx;
	pid_t ProcessID[_SUPPORT_MAX_OWE_FRAME_REQUEST_];
};


static struct OWE_INFO_STRUCT OWEInfo;

enum _eLOG_TYPE {
	_LOG_DBG = 0,	/* currently, only used at ipl_buf_ctrl.
			 * to protect critical section
			 */
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
static struct SV_LOG_STR gSvLog[OWE_IRQ_TYPE_AMOUNT];

/* for irq used,keep log until IRQ_LOG_PRINTER being involked, */
/*    limited: */
/*    each log must shorter than 512 bytes */
/*    total log length in each irq/logtype can't over 1024 bytes */

#if 1
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...) do {\
	char *ptr; \
	char *pDes;\
	int avaLen;\
	unsigned int *ptr2 = &gSvLog[irq]._cnt[ppb][logT];\
	unsigned int str_leng;\
	unsigned int logi;\
	struct SV_LOG_STR *pSrc = &gSvLog[irq];\
	if (logT == _LOG_ERR) {\
		str_leng = NORMAL_STR_LEN*ERR_PAGE; \
	} else if (logT == _LOG_DBG) {\
		str_leng = NORMAL_STR_LEN*DBG_PAGE; \
	} else if (logT == _LOG_INF) {\
		str_leng = NORMAL_STR_LEN*INF_PAGE;\
	} else {\
		str_leng = 0;\
	} \
	ptr = pDes = (char *)&(gSvLog[irq].\
			_str[ppb][logT][gSvLog[irq].\
			_cnt[ppb][logT]]);    \
	avaLen = str_leng - 1 - gSvLog[irq]._cnt[ppb][logT];\
	if (avaLen > 1) {\
		snprintf((char *)(pDes), avaLen, fmt,\
			##__VA_ARGS__);   \
		if ('\0' != gSvLog[irq]._str[ppb][logT][str_leng - 1]) {\
			LOG_ERR("log str over flow(%d)", irq);\
		} \
		while (*ptr++ != '\0') {        \
			(*ptr2)++;\
		}     \
	} else { \
		LOG_INF("(%d)(%d)log str avalible=0, print log\n", irq, logT);\
		ptr = pSrc->_str[ppb][logT];\
		if (pSrc->_cnt[ppb][logT] != 0) {\
			if (logT == _LOG_DBG) {\
				for (logi = 0; logi < DBG_PAGE; logi++) {\
					if (ptr[NORMAL_STR_LEN*(logi+1) - 1] !=\
					    '\0') {\
						ptr[NORMAL_STR_LEN*(logi+1)\
						    - 1] = '\0';\
						LOG_DBG("%s",\
						    &ptr[NORMAL_STR_LEN*logi]);\
					} else{\
						LOG_DBG("%s",\
						    &ptr[NORMAL_STR_LEN*logi]);\
						break;\
					} \
				} \
			} \
			else if (logT == _LOG_INF) {\
				for (logi = 0; logi < INF_PAGE; logi++) {\
					if (ptr[NORMAL_STR_LEN*(logi+1) - 1] !=\
					    '\0') {\
						ptr[NORMAL_STR_LEN*(logi+1)\
						    - 1] = '\0';\
						LOG_INF("%s",\
						    &ptr[NORMAL_STR_LEN*logi]);\
					} else{\
						LOG_INF("%s",\
						    &ptr[NORMAL_STR_LEN*logi]);\
						break;\
					} \
				} \
			} \
			else if (logT == _LOG_ERR) {\
				for (logi = 0; logi < ERR_PAGE; logi++) {\
					if (ptr[NORMAL_STR_LEN*(logi+1) - 1] !=\
					    '\0') {\
						ptr[NORMAL_STR_LEN*(logi+1)\
						    - 1] = '\0';\
						LOG_INF("%s",\
						    &ptr[NORMAL_STR_LEN*logi]);\
					} else{\
						LOG_INF("%s",\
						    &ptr[NORMAL_STR_LEN*logi]);\
						break;\
					} \
				} \
			} \
			else {\
				LOG_INF("N.S.%d", logT);\
			} \
			ptr[0] = '\0';\
			pSrc->_cnt[ppb][logT] = 0;\
			avaLen = str_leng - 1;\
			ptr = pDes = (char *)&(\
			     pSrc->_str[ppb][logT][pSrc->_cnt[ppb][logT]]);\
			ptr2 = &(pSrc->_cnt[ppb][logT]);\
			snprintf((char *)(pDes), avaLen, fmt, ##__VA_ARGS__);\
			while (*ptr++ != '\0') {\
				(*ptr2)++;\
			} \
		} \
	} \
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


/* OWE registers */
#define OWE_RST_HW                    (OWE_BASE_HW)
#define OWE_INT_CTL_HW                (OWE_BASE_HW + 0x08)
#define OWE_INT_STATUS_HW             (OWE_BASE_HW + 0x18)
#define OWE_DBG_INFO_0_HW             (OWE_BASE_HW + 0x1C)

#define OWE_OCC_START_HW              (OWE_BASE_HW + 0x0020)
#define OWE_OCC_INT_CTRL_HW           (OWE_BASE_HW + 0x0024)
#define OWE_OCC_INT_STATUS_HW         (OWE_BASE_HW + 0x0028)
#define DPE_OCC_CTRL_0_HW             (OWE_BASE_HW + 0x0030)
#define DPE_OCC_CTRL_1_HW             (OWE_BASE_HW + 0x0034)
#define DPE_OCC_CTRL_2_HW             (OWE_BASE_HW + 0x0038)
#define DPE_OCC_CTRL_3_HW             (OWE_BASE_HW + 0x003C)
#define DPE_OCC_REF_VEC_BASE_HW       (OWE_BASE_HW + 0x0040)
#define DPE_OCC_REF_VEC_STRIDE_HW     (OWE_BASE_HW + 0x0044)
#define DPE_OCC_REF_PXL_BASE_HW       (OWE_BASE_HW + 0x0048)
#define DPE_OCC_REF_PXL_STRIDE_HW     (OWE_BASE_HW + 0x004C)
#define DPE_OCC_MAJ_VEC_BASE_HW       (OWE_BASE_HW + 0x0050)
#define DPE_OCC_MAJ_VEC_STRIDE_HW     (OWE_BASE_HW + 0x0054)
#define DPE_OCC_MAJ_PXL_BASE_HW       (OWE_BASE_HW + 0x0058)
#define DPE_OCC_MAJ_PXL_STRIDE_HW     (OWE_BASE_HW + 0x005C)
#define DPE_OCC_WDMA_BASE_HW          (OWE_BASE_HW + 0x0060)
#define DPE_OCC_WDMA_STRIDE_HW        (OWE_BASE_HW + 0x0064)
#define DPE_OCC_PQ_0_HW               (OWE_BASE_HW + 0x0068)
#define DPE_OCC_PQ_1_HW               (OWE_BASE_HW + 0x006C)
#define DPE_OCC_SPARE_HW              (OWE_BASE_HW + 0x0070)
#define DPE_OCC_DFT_HW                (OWE_BASE_HW + 0x0074)

#define OWE_WMFE_START_HW             (OWE_BASE_HW + 0x220)
#define OWE_WMFE_INT_CTRL_HW          (OWE_BASE_HW + 0x224)
#define OWE_WMFE_INT_STATUS_HW        (OWE_BASE_HW + 0x228)

#define OWE_WMFE_CTRL_0_HW            (OWE_BASE_HW + 0x230)
#define OWE_WMFE_SIZE_0_HW            (OWE_BASE_HW + 0x234)
#define OWE_WMFE_IMGI_BASE_ADDR_0_HW  (OWE_BASE_HW + 0x238)
#define OWE_WMFE_IMGI_STRIDE_0_HW     (OWE_BASE_HW + 0x23C)
#define OWE_WMFE_DPI_BASE_ADDR_0_HW   (OWE_BASE_HW + 0x240)
#define OWE_WMFE_DPI_STRIDE_0_HW      (OWE_BASE_HW + 0x244)
#define OWE_WMFE_TBLI_BASE_ADDR_0_HW  (OWE_BASE_HW + 0x248)
#define OWE_WMFE_TBLI_STRIDE_0_HW     (OWE_BASE_HW + 0x24C)
#define OWE_WMFE_MASKI_BASE_ADDR_0_HW (OWE_BASE_HW + 0x250)
#define OWE_WMFE_MASKI_STRIDE_0_HW    (OWE_BASE_HW + 0x254)
#define OWE_WMFE_DPO_BASE_ADDR_0_HW   (OWE_BASE_HW + 0x258)
#define OWE_WMFE_DPO_STRIDE_0_HW      (OWE_BASE_HW + 0x25C)


#define OWE_WMFE_CTRL_1_HW            (OWE_BASE_HW + 0x270)
#define OWE_WMFE_SIZE_1_HW            (OWE_BASE_HW + 0x274)
#define OWE_WMFE_IMGI_BASE_ADDR_1_HW  (OWE_BASE_HW + 0x278)
#define OWE_WMFE_IMGI_STRIDE_1_HW     (OWE_BASE_HW + 0x27C)
#define OWE_WMFE_DPI_BASE_ADDR_1_HW   (OWE_BASE_HW + 0x280)
#define OWE_WMFE_DPI_STRIDE_1_HW      (OWE_BASE_HW + 0x284)
#define OWE_WMFE_TBLI_BASE_ADDR_1_HW  (OWE_BASE_HW + 0x288)
#define OWE_WMFE_TBLI_STRIDE_1_HW     (OWE_BASE_HW + 0x28C)
#define OWE_WMFE_MASKI_BASE_ADDR_1_HW (OWE_BASE_HW + 0x290)
#define OWE_WMFE_MASKI_STRIDE_1_HW    (OWE_BASE_HW + 0x294)
#define OWE_WMFE_DPO_BASE_ADDR_1_HW   (OWE_BASE_HW + 0x298)
#define OWE_WMFE_DPO_STRIDE_1_HW      (OWE_BASE_HW + 0x29C)

#define OWE_WMFE_CTRL_2_HW            (OWE_BASE_HW + 0x2B0)
#define OWE_WMFE_SIZE_2_HW            (OWE_BASE_HW + 0x2B4)
#define OWE_WMFE_IMGI_BASE_ADDR_2_HW  (OWE_BASE_HW + 0x2B8)
#define OWE_WMFE_IMGI_STRIDE_2_HW     (OWE_BASE_HW + 0x2BC)
#define OWE_WMFE_DPI_BASE_ADDR_2_HW   (OWE_BASE_HW + 0x2C0)
#define OWE_WMFE_DPI_STRIDE_2_HW      (OWE_BASE_HW + 0x2C4)
#define OWE_WMFE_TBLI_BASE_ADDR_2_HW  (OWE_BASE_HW + 0x2C8)
#define OWE_WMFE_TBLI_STRIDE_2_HW     (OWE_BASE_HW + 0x2CC)
#define OWE_WMFE_MASKI_BASE_ADDR_2_HW (OWE_BASE_HW + 0x2D0)
#define OWE_WMFE_MASKI_STRIDE_2_HW    (OWE_BASE_HW + 0x2D4)
#define OWE_WMFE_DPO_BASE_ADDR_2_HW   (OWE_BASE_HW + 0x2D8)
#define OWE_WMFE_DPO_STRIDE_2_HW      (OWE_BASE_HW + 0x2DC)

#define OWE_WMFE_CTRL_3_HW            (OWE_BASE_HW + 0x2F0)
#define OWE_WMFE_SIZE_3_HW            (OWE_BASE_HW + 0x2F4)
#define OWE_WMFE_IMGI_BASE_ADDR_3_HW  (OWE_BASE_HW + 0x2F8)
#define OWE_WMFE_IMGI_STRIDE_3_HW     (OWE_BASE_HW + 0x2FC)
#define OWE_WMFE_DPI_BASE_ADDR_3_HW   (OWE_BASE_HW + 0x300)
#define OWE_WMFE_DPI_STRIDE_3_HW      (OWE_BASE_HW + 0x304)
#define OWE_WMFE_TBLI_BASE_ADDR_3_HW  (OWE_BASE_HW + 0x308)
#define OWE_WMFE_TBLI_STRIDE_3_HW     (OWE_BASE_HW + 0x30C)
#define OWE_WMFE_MASKI_BASE_ADDR_3_HW (OWE_BASE_HW + 0x310)
#define OWE_WMFE_MASKI_STRIDE_3_HW    (OWE_BASE_HW + 0x314)
#define OWE_WMFE_DPO_BASE_ADDR_3_HW   (OWE_BASE_HW + 0x318)
#define OWE_WMFE_DPO_STRIDE_3_HW      (OWE_BASE_HW + 0x31C)

#define OWE_WMFE_CTRL_4_HW            (OWE_BASE_HW + 0x330)
#define OWE_WMFE_SIZE_4_HW            (OWE_BASE_HW + 0x334)
#define OWE_WMFE_IMGI_BASE_ADDR_4_HW  (OWE_BASE_HW + 0x338)
#define OWE_WMFE_IMGI_STRIDE_4_HW     (OWE_BASE_HW + 0x33C)
#define OWE_WMFE_DPI_BASE_ADDR_4_HW   (OWE_BASE_HW + 0x340)
#define OWE_WMFE_DPI_STRIDE_4_HW      (OWE_BASE_HW + 0x344)
#define OWE_WMFE_TBLI_BASE_ADDR_4_HW  (OWE_BASE_HW + 0x348)
#define OWE_WMFE_TBLI_STRIDE_4_HW     (OWE_BASE_HW + 0x34C)
#define OWE_WMFE_MASKI_BASE_ADDR_4_HW (OWE_BASE_HW + 0x350)
#define OWE_WMFE_MASKI_STRIDE_4_HW    (OWE_BASE_HW + 0x354)
#define OWE_WMFE_DPO_BASE_ADDR_4_HW   (OWE_BASE_HW + 0x358)
#define OWE_WMFE_DPO_STRIDE_4_HW      (OWE_BASE_HW + 0x35C)



#define OWE_WMFE_DBG_INFO_00_HW       (OWE_BASE_HW + 0x400)
#define OWE_WMFE_DBG_INFO_01_HW       (OWE_BASE_HW + 0x404)
#define OWE_WMFE_DBG_INFO_02_HW       (OWE_BASE_HW + 0x408)
#define OWE_WMFE_DBG_INFO_03_HW       (OWE_BASE_HW + 0x40C)
#define OWE_WMFE_DBG_INFO_04_HW       (OWE_BASE_HW + 0x410)
#define OWE_WMFE_DBG_INFO_05_HW       (OWE_BASE_HW + 0x414)
#define OWE_WMFE_DBG_INFO_06_HW       (OWE_BASE_HW + 0x418)
#define OWE_WMFE_DBG_INFO_07_HW       (OWE_BASE_HW + 0x41C)
#define OWE_WMFE_DBG_INFO_08_HW       (OWE_BASE_HW + 0x420)
#define OWE_WMFE_DBG_INFO_09_HW       (OWE_BASE_HW + 0x424)

#define OWE_DMA_DBG_HW                (OWE_BASE_HW + 0x7F4)
#define OWE_DMA_REQ_STATUS_HW         (OWE_BASE_HW + 0x7F8)
#define OWE_DMA_RDY_STATUS_HW         (OWE_BASE_HW + 0x7FC)





#define OWE_RST_REG                    (ISP_OWE_BASE)
#define OWE_INT_CTL_REG                (ISP_OWE_BASE + 0x08)
#define OWE_INT_STATUS_REG             (ISP_OWE_BASE + 0x18)
#define OWE_DBG_INFO_0_REG             (ISP_OWE_BASE + 0x1C)

#define OWE_OCC_START_REG              (ISP_OWE_BASE + 0x0020)
#define OWE_OCC_INT_CTRL_REG           (ISP_OWE_BASE + 0x0024)
#define OWE_OCC_INT_STATUS_REG         (ISP_OWE_BASE + 0x0028)
#define DPE_OCC_CTRL_0_REG             (ISP_OWE_BASE + 0x0030)
#define DPE_OCC_CTRL_1_REG             (ISP_OWE_BASE + 0x0034)
#define DPE_OCC_CTRL_2_REG             (ISP_OWE_BASE + 0x0038)
#define DPE_OCC_CTRL_3_REG             (ISP_OWE_BASE + 0x003C)
#define DPE_OCC_REF_VEC_BASE_REG       (ISP_OWE_BASE + 0x0040)
#define DPE_OCC_REF_VEC_STRIDE_REG     (ISP_OWE_BASE + 0x0044)
#define DPE_OCC_REF_PXL_BASE_REG       (ISP_OWE_BASE + 0x0048)
#define DPE_OCC_REF_PXL_STRIDE_REG     (ISP_OWE_BASE + 0x004C)
#define DPE_OCC_MAJ_VEC_BASE_REG       (ISP_OWE_BASE + 0x0050)
#define DPE_OCC_MAJ_VEC_STRIDE_REG     (ISP_OWE_BASE + 0x0054)
#define DPE_OCC_MAJ_PXL_BASE_REG       (ISP_OWE_BASE + 0x0058)
#define DPE_OCC_MAJ_PXL_STRIDE_REG     (ISP_OWE_BASE + 0x005C)
#define DPE_OCC_WDMA_BASE_REG          (ISP_OWE_BASE + 0x0060)
#define DPE_OCC_WDMA_STRIDE_REG        (ISP_OWE_BASE + 0x0064)
#define DPE_OCC_PQ_0_REG               (ISP_OWE_BASE + 0x0068)
#define DPE_OCC_PQ_1_REG               (ISP_OWE_BASE + 0x006C)
#define DPE_OCC_SPARE_REG              (ISP_OWE_BASE + 0x0070)
#define DPE_OCC_DFT_REG                (ISP_OWE_BASE + 0x0074)

#define OWE_WMFE_START_REG             (ISP_OWE_BASE + 0x220)
#define OWE_WMFE_INT_CTRL_REG          (ISP_OWE_BASE + 0x224)
#define OWE_WMFE_INT_STATUS_REG        (ISP_OWE_BASE + 0x228)

#define OWE_WMFE_CTRL_0_REG            (ISP_OWE_BASE + 0x230)
#define OWE_WMFE_SIZE_0_REG            (ISP_OWE_BASE + 0x234)
#define OWE_WMFE_IMGI_BASE_ADDR_0_REG  (ISP_OWE_BASE + 0x238)
#define OWE_WMFE_IMGI_STRIDE_0_REG     (ISP_OWE_BASE + 0x23C)
#define OWE_WMFE_DPI_BASE_ADDR_0_REG   (ISP_OWE_BASE + 0x240)
#define OWE_WMFE_DPI_STRIDE_0_REG      (ISP_OWE_BASE + 0x244)
#define OWE_WMFE_TBLI_BASE_ADDR_0_REG  (ISP_OWE_BASE + 0x248)
#define OWE_WMFE_TBLI_STRIDE_0_REG     (ISP_OWE_BASE + 0x24C)
#define OWE_WMFE_MASKI_BASE_ADDR_0_REG (ISP_OWE_BASE + 0x250)
#define OWE_WMFE_MASKI_STRIDE_0_REG    (ISP_OWE_BASE + 0x254)
#define OWE_WMFE_DPO_BASE_ADDR_0_REG   (ISP_OWE_BASE + 0x258)
#define OWE_WMFE_DPO_STRIDE_0_REG      (ISP_OWE_BASE + 0x25C)

#define OWE_WMFE_CTRL_1_REG            (ISP_OWE_BASE + 0x270)
#define OWE_WMFE_SIZE_1_REG            (ISP_OWE_BASE + 0x274)
#define OWE_WMFE_IMGI_BASE_ADDR_1_REG  (ISP_OWE_BASE + 0x278)
#define OWE_WMFE_IMGI_STRIDE_1_REG     (ISP_OWE_BASE + 0x27C)
#define OWE_WMFE_DPI_BASE_ADDR_1_REG   (ISP_OWE_BASE + 0x280)
#define OWE_WMFE_DPI_STRIDE_1_REG      (ISP_OWE_BASE + 0x284)
#define OWE_WMFE_TBLI_BASE_ADDR_1_REG  (ISP_OWE_BASE + 0x288)
#define OWE_WMFE_TBLI_STRIDE_1_REG     (ISP_OWE_BASE + 0x28C)
#define OWE_WMFE_MASKI_BASE_ADDR_1_REG (ISP_OWE_BASE + 0x290)
#define OWE_WMFE_MASKI_STRIDE_1_REG    (ISP_OWE_BASE + 0x294)
#define OWE_WMFE_DPO_BASE_ADDR_1_REG   (ISP_OWE_BASE + 0x298)
#define OWE_WMFE_DPO_STRIDE_1_REG      (ISP_OWE_BASE + 0x29C)


#define OWE_WMFE_CTRL_2_REG            (ISP_OWE_BASE + 0x2B0)
#define OWE_WMFE_SIZE_2_REG            (ISP_OWE_BASE + 0x2B4)
#define OWE_WMFE_IMGI_BASE_ADDR_2_REG  (ISP_OWE_BASE + 0x2B8)
#define OWE_WMFE_IMGI_STRIDE_2_REG     (ISP_OWE_BASE + 0x2BC)
#define OWE_WMFE_DPI_BASE_ADDR_2_REG   (ISP_OWE_BASE + 0x2C0)
#define OWE_WMFE_DPI_STRIDE_2_REG      (ISP_OWE_BASE + 0x2C4)
#define OWE_WMFE_TBLI_BASE_ADDR_2_REG  (ISP_OWE_BASE + 0x2C8)
#define OWE_WMFE_TBLI_STRIDE_2_REG     (ISP_OWE_BASE + 0x2CC)
#define OWE_WMFE_MASKI_BASE_ADDR_2_REG (ISP_OWE_BASE + 0x2D0)
#define OWE_WMFE_MASKI_STRIDE_2_REG    (ISP_OWE_BASE + 0x2D4)
#define OWE_WMFE_DPO_BASE_ADDR_2_REG   (ISP_OWE_BASE + 0x2D8)
#define OWE_WMFE_DPO_STRIDE_2_REG      (ISP_OWE_BASE + 0x2DC)

#define OWE_WMFE_CTRL_3_REG            (ISP_OWE_BASE + 0x2F0)
#define OWE_WMFE_SIZE_3_REG            (ISP_OWE_BASE + 0x2F4)
#define OWE_WMFE_IMGI_BASE_ADDR_3_REG  (ISP_OWE_BASE + 0x2F8)
#define OWE_WMFE_IMGI_STRIDE_3_REG     (ISP_OWE_BASE + 0x2FC)
#define OWE_WMFE_DPI_BASE_ADDR_3_REG   (ISP_OWE_BASE + 0x300)
#define OWE_WMFE_DPI_STRIDE_3_REG      (ISP_OWE_BASE + 0x304)
#define OWE_WMFE_TBLI_BASE_ADDR_3_REG  (ISP_OWE_BASE + 0x308)
#define OWE_WMFE_TBLI_STRIDE_3_REG     (ISP_OWE_BASE + 0x30C)
#define OWE_WMFE_MASKI_BASE_ADDR_3_REG (ISP_OWE_BASE + 0x310)
#define OWE_WMFE_MASKI_STRIDE_3_REG    (ISP_OWE_BASE + 0x314)
#define OWE_WMFE_DPO_BASE_ADDR_3_REG   (ISP_OWE_BASE + 0x318)
#define OWE_WMFE_DPO_STRIDE_3_REG      (ISP_OWE_BASE + 0x31C)

#define OWE_WMFE_CTRL_4_REG            (ISP_OWE_BASE + 0x330)
#define OWE_WMFE_SIZE_4_REG            (ISP_OWE_BASE + 0x334)
#define OWE_WMFE_IMGI_BASE_ADDR_4_REG  (ISP_OWE_BASE + 0x338)
#define OWE_WMFE_IMGI_STRIDE_4_REG     (ISP_OWE_BASE + 0x33C)
#define OWE_WMFE_DPI_BASE_ADDR_4_REG   (ISP_OWE_BASE + 0x340)
#define OWE_WMFE_DPI_STRIDE_4_REG      (ISP_OWE_BASE + 0x344)
#define OWE_WMFE_TBLI_BASE_ADDR_4_REG  (ISP_OWE_BASE + 0x348)
#define OWE_WMFE_TBLI_STRIDE_4_REG     (ISP_OWE_BASE + 0x34C)
#define OWE_WMFE_MASKI_BASE_ADDR_4_REG (ISP_OWE_BASE + 0x350)
#define OWE_WMFE_MASKI_STRIDE_4_REG    (ISP_OWE_BASE + 0x354)
#define OWE_WMFE_DPO_BASE_ADDR_4_REG   (ISP_OWE_BASE + 0x358)
#define OWE_WMFE_DPO_STRIDE_4_REG      (ISP_OWE_BASE + 0x35C)


#define OWE_WMFE_DBG_INFO_00_REG       (ISP_OWE_BASE + 0x400)
#define OWE_WMFE_DBG_INFO_01_REG       (ISP_OWE_BASE + 0x404)
#define OWE_WMFE_DBG_INFO_02_REG       (ISP_OWE_BASE + 0x408)
#define OWE_WMFE_DBG_INFO_03_REG       (ISP_OWE_BASE + 0x40C)
#define OWE_WMFE_DBG_INFO_04_REG       (ISP_OWE_BASE + 0x410)
#define OWE_WMFE_DBG_INFO_05_REG       (ISP_OWE_BASE + 0x414)
#define OWE_WMFE_DBG_INFO_06_REG       (ISP_OWE_BASE + 0x418)
#define OWE_WMFE_DBG_INFO_07_REG       (ISP_OWE_BASE + 0x41C)
#define OWE_WMFE_DBG_INFO_08_REG       (ISP_OWE_BASE + 0x420)
#define OWE_WMFE_DBG_INFO_09_REG       (ISP_OWE_BASE + 0x424)

#define OWE_DMA_DBG_REG                (ISP_OWE_BASE + 0x7F4)
#define OWE_DMA_REQ_STATUS_REG         (ISP_OWE_BASE + 0x7F8)
#define OWE_DMA_RDY_STATUS_REG         (ISP_OWE_BASE + 0x7FC)

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int OWE_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int OWE_UsToJiffies(unsigned int Us)
{
	return (((Us / 1000) * HZ + 512) >> 10);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int OWE_GetIRQState(
	unsigned int type, unsigned int userNumber, unsigned int stus,
	enum OWE_PROCESS_ID_ENUM whichReq, int ProcessID)
{
	unsigned int ret = 0;
	unsigned long flags; /* old: unsigned int flags;*/
			     /* FIX to avoid build warning */

	/*  */
	spin_lock_irqsave(&(OWEInfo.SpinLockIrq[type]), flags);

#ifdef OWE_MULTIPROCESS_TIMEING_ISSUE
	if (stus & OWE_OCC_INT_ST) {
		ret = ((OWEInfo.IrqInfo.OccIrqCnt > 0)
		       && (OWEInfo.ProcessID[OWEInfo.ReadReqIdx] == ProcessID));
	} else if (stus & OWE_WMFE_INT_ST) {
		ret = ((OWEInfo.IrqInfo.WmfeIrqCnt > 0)
		       && (OWEInfo.ProcessID[OWEInfo.ReadReqIdx] == ProcessID));
	} else {
		LOG_ERR(
		"WaitIRQ Err,type:%d,urNum:%d,sta:%d,whReq:%d,PID:0x%x, RdReqIdx:%d\n",
		type, userNumber, stus, whichReq,
		ProcessID, OWEInfo.ReadReqIdx);
	}

#else
	if (stus & OWE_OCC_INT_ST) {
		ret = ((OWEInfo.IrqInfo.OccIrqCnt > 0)
		       && (OWEInfo.IrqInfo.ProcessID[whichReq] == ProcessID));
	} else if (stus & OWE_WMFE_INT_ST) {
		ret = ((OWEInfo.IrqInfo.WmfeIrqCnt > 0)
		       && (OWEInfo.IrqInfo.ProcessID[whichReq] == ProcessID));
	} else {
		LOG_ERR(
		"WaitIRQ Status Error, type:%d, userNumber:%d, status:%d, whichReq:%d, ProcessID:0x%x\n",
		     type, userNumber, stus, whichReq, ProcessID);
	}
#endif
	spin_unlock_irqrestore(&(OWEInfo.SpinLockIrq[type]), flags);
	/*  */
	return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int OWE_JiffiesToMs(unsigned int Jiffies)
{
	return ((Jiffies * 1000) / HZ);
}


#define RegDump(start, end) {\
	unsigned int i;\
	for (i = start; i <= end; i += 0x10) {\
		LOG_DBG(\
		  "[0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]",\
		  (unsigned int)(ISP_OWE_BASE + i),\
		  (unsigned int)OWE_RD32(ISP_OWE_BASE + i),\
		  (unsigned int)(ISP_OWE_BASE + i+0x4),\
		  (unsigned int)OWE_RD32(ISP_OWE_BASE + i+0x4),\
		  (unsigned int)(ISP_OWE_BASE + i+0x8),\
		  (unsigned int)OWE_RD32(ISP_OWE_BASE + i+0x8),\
		  (unsigned int)(ISP_OWE_BASE + i+0xc),\
		  (unsigned int)OWE_RD32(ISP_OWE_BASE + i+0xc));\
	} \
}

signed int occ_enque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt;
	/*TODO: define engine request struct */
	struct OWE_OCCRequest *_req;
	struct OWE_OCCConfig *pcfg;

	_req = (struct OWE_OCCRequest *) req;

	if (frames == NULL || _req == NULL)
		return -1;

	/*TODO: m_ReqNum is FrmNum; FIFO only thus f starts from 0 */
	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		memcpy(frames[f].data, &_req->m_pOweConfig[f],
						sizeof(struct OWE_OCCConfig));
		pcfg = &_req->m_pOweConfig[f];
	}

	return 0;
}

signed int occ_deque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt;
	struct OWE_OCCRequest *_req;
	struct OWE_OCCConfig *pcfg;

	_req = (struct OWE_OCCRequest *) req;

	if (frames == NULL || _req == NULL)
		return -1;

	/*TODO: m_ReqNum is FrmNum; FIFO only thus f starts from 0 */
	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		memcpy(&_req->m_pOweConfig[f], frames[f].data,
						sizeof(struct OWE_OCCConfig));
		LOG_DBG("[%s]request deque frame(%d/%d).", __func__, f, fcnt);
		pcfg = &_req->m_pOweConfig[f];
	}

	return 0;
}

static int cmdq_engine_secured(struct cmdqRecStruct *handle,
						enum CMDQ_ENG_ENUM engine)
{
	cmdqRecSetSecure(handle, 1);
	cmdqRecSecureEnablePortSecurity(handle, (1LL << engine));
	cmdqRecSecureEnableDAPC(handle, (1LL << engine));

	return 0;
}

/*TODO : M4U_PORT */
#if 0
static int cmdq_sec_base(struct cmdqRecStruct *handle, unsigned int dma_sec,
			unsigned int reg, unsigned int val, unsigned int size)
{
	if (dma_sec != 0)
		cmdqRecWriteSecure(handle, reg, CMDQ_SAM_H_2_MVA, val, 0, size,
							M4U_PORT_CAM_OWE_RDMA);
	else
		cmdqRecWrite(handle, reg, val, CMDQ_REG_MASK);

	return 0;
}
#endif
signed int CmdqOCCHW(struct frame *frame)
{
	struct OWE_OCCConfig *pOccConfig;
	struct cmdqRecStruct *handle;
	unsigned int size_sec = 0;
	uint64_t engineFlag = (uint64_t)(1LL << CMDQ_ENG_OWE);

	if (frame == NULL || frame->data == NULL)
		return -1;

	LOG_DBG("%s request sent to CMDQ driver", __func__);
	pOccConfig = (struct OWE_OCCConfig *) frame->data;

	if (OWE_DBG_DBGLOG == (OWE_DBG_DBGLOG & OWEInfo.DebugMask)) {
		LOG_DBG("ConfigOCCHW Start!\n");
#ifndef BYPASS_REG
#define PRINT_DBG(REG) LOG_DBG(#REG ":0x%x!\n", pOccConfig->REG)
		PRINT_DBG(DPE_OCC_CTRL_0);
		PRINT_DBG(DPE_OCC_CTRL_1);
		PRINT_DBG(DPE_OCC_CTRL_2);
		PRINT_DBG(DPE_OCC_CTRL_3);
		PRINT_DBG(DPE_OCC_REF_VEC_BASE);
		PRINT_DBG(DPE_OCC_REF_VEC_STRIDE);
		PRINT_DBG(DPE_OCC_REF_PXL_BASE);
		PRINT_DBG(DPE_OCC_REF_PXL_STRIDE);
		PRINT_DBG(DPE_OCC_MAJ_VEC_BASE);
		PRINT_DBG(DPE_OCC_MAJ_VEC_STRIDE);
		PRINT_DBG(DPE_OCC_MAJ_PXL_BASE);
		PRINT_DBG(DPE_OCC_MAJ_PXL_STRIDE);
		PRINT_DBG(DPE_OCC_WDMA_BASE);
		PRINT_DBG(DPE_OCC_WDMA_STRIDE);
		PRINT_DBG(DPE_OCC_PQ_0);
		PRINT_DBG(DPE_OCC_PQ_1);
		PRINT_DBG(DPE_OCC_SPARE);
		PRINT_DBG(DPE_OCC_DFT);
#endif
	}


	cmdqRecCreate(CMDQ_SCENARIO_KERNEL_CONFIG_GENERAL, &handle);
	/* CMDQ driver dispatches CMDQ HW thread
	 * and HW thread's priority according to scenario
	 */

	cmdqRecSetEngine(handle, engineFlag);

	cmdqRecReset(handle);

	if (pOccConfig->eng_secured == 1)
		cmdq_engine_secured(handle, CMDQ_ENG_OWE);

#ifndef BYPASS_REG
#define CMDQWR(REG) \
	cmdqRecWrite(handle, REG ##_HW, pOccConfig->REG, CMDQ_REG_MASK)
	/* Use command queue to write register */
	cmdqRecWrite(handle, OWE_INT_CTL_HW, 0x1, CMDQ_REG_MASK);
	/* OWE Interrupt read-clear mode */
	cmdqRecWrite(handle, OWE_OCC_INT_CTRL_HW, 0x1, CMDQ_REG_MASK);
	/* OCC Interrupt read-clear mode */

	CMDQWR(DPE_OCC_CTRL_0);
	CMDQWR(DPE_OCC_CTRL_1);
	CMDQWR(DPE_OCC_CTRL_2);
	CMDQWR(DPE_OCC_CTRL_3);

	size_sec = pOccConfig->dma_sec_size[OCC_DMA_REF_VEC];
	if (size_sec != 0)
		cmdqRecWriteSecure(handle, DPE_OCC_REF_VEC_BASE_HW,
			CMDQ_SAM_H_2_MVA,
			pOccConfig->DPE_OCC_REF_VEC_BASE, 0, size_sec,
			M4U_PORT_CAM_OWE_RDMA);
	else
		CMDQWR(DPE_OCC_REF_VEC_BASE);

	size_sec = pOccConfig->dma_sec_size[OCC_DMA_REF_PXL];
	if (size_sec != 0)
		cmdqRecWriteSecure(handle, DPE_OCC_REF_PXL_BASE_HW,
			CMDQ_SAM_H_2_MVA,
			pOccConfig->DPE_OCC_REF_PXL_BASE, 0, size_sec,
			M4U_PORT_CAM_OWE_RDMA);
	else
		CMDQWR(DPE_OCC_REF_PXL_BASE);

	size_sec = pOccConfig->dma_sec_size[OCC_DMA_MAJ_VEC];
	if (size_sec != 0)
		cmdqRecWriteSecure(handle, DPE_OCC_MAJ_VEC_BASE_HW,
			CMDQ_SAM_H_2_MVA,
			pOccConfig->DPE_OCC_MAJ_VEC_BASE, 0, size_sec,
			M4U_PORT_CAM_OWE_RDMA);
	else
		CMDQWR(DPE_OCC_MAJ_VEC_BASE);

	size_sec = pOccConfig->dma_sec_size[OCC_DMA_MAJ_PXL];
	if (size_sec != 0)
		cmdqRecWriteSecure(handle, DPE_OCC_MAJ_PXL_BASE_HW,
			CMDQ_SAM_H_2_MVA,
			pOccConfig->DPE_OCC_MAJ_PXL_BASE, 0, size_sec,
			M4U_PORT_CAM_OWE_RDMA);
	else
		CMDQWR(DPE_OCC_MAJ_PXL_BASE);


	size_sec = pOccConfig->dma_sec_size[OCC_DMA_WDMA];
	if (size_sec != 0)
		cmdqRecWriteSecure(handle, DPE_OCC_WDMA_BASE_HW,
			CMDQ_SAM_H_2_MVA,
			pOccConfig->DPE_OCC_WDMA_BASE, 0, size_sec,
			M4U_PORT_CAM_OWE_WDMA);
	else
		CMDQWR(DPE_OCC_WDMA_BASE);

	CMDQWR(DPE_OCC_REF_VEC_STRIDE);
	CMDQWR(DPE_OCC_REF_PXL_STRIDE);
	CMDQWR(DPE_OCC_MAJ_VEC_STRIDE);
	CMDQWR(DPE_OCC_MAJ_PXL_STRIDE);
	CMDQWR(DPE_OCC_WDMA_STRIDE);
	CMDQWR(DPE_OCC_PQ_0);
	CMDQWR(DPE_OCC_PQ_1);

	cmdqRecWrite(handle, OWE_OCC_START_HW, 0x1, CMDQ_REG_MASK);
	/* OWE Interrupt read-clear mode */
	cmdqRecWait(handle, CMDQ_EVENT_OCC_DONE);
	cmdqRecWrite(handle, OWE_OCC_START_HW, 0x0, CMDQ_REG_MASK);
	/* OWE Interrupt read-clear mode */

	/* non-blocking API, Please  use cmdqRecFlushAsync() */
	cmdqRecFlushAsync(handle);
	/* if you want to re-use the handle, please reset the handle */
	cmdqRecReset(handle);
	cmdqRecDestroy(handle);
#endif


	return 0;
}
static const struct engine_ops occ_ops = {
	.req_enque_cb = occ_enque_cb,
	.req_deque_cb = occ_deque_cb,
	.frame_handler = CmdqOCCHW,
	.req_feedback_cb = NULL,
};

signed int wmfe_enque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt;
	/*TODO: define engine request struct */
	struct OWE_WMFERequest *_req;
	struct OWE_WMFEConfig *pcfg;

	_req = (struct OWE_WMFERequest *) req;

	if (frames == NULL || _req == NULL)
		return -1;

	/*TODO: m_ReqNum is FrmNum; FIFO only thus f starts from 0 */
	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		memcpy(frames[f].data, &_req->m_pWmfeConfig[f],
			sizeof(struct OWE_WMFEConfig));
		pcfg = &_req->m_pWmfeConfig[f];
	}

	return 0;
}

signed int wmfe_deque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt;
	struct OWE_WMFERequest *_req;
	struct OWE_WMFEConfig *pcfg;

	_req = (struct OWE_WMFERequest *) req;

	if (frames == NULL || _req == NULL)
		return -1;

	/*TODO: m_ReqNum is FrmNum; FIFO only thus f starts from 0 */
	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		memcpy(&_req->m_pWmfeConfig[f], frames[f].data,
			sizeof(struct OWE_WMFEConfig));
		LOG_DBG("[%s]request dequeued frame(%d/%d).",
			__func__, f, fcnt);
		pcfg = &_req->m_pWmfeConfig[f];
	}

	return 0;
}

signed int CmdqWMFEHW(struct frame *frame)
{
	struct OWE_WMFEConfig *pWmfeCfg;
	struct cmdqRecStruct *handle;
	uint64_t engineFlag = (uint64_t)(1LL << CMDQ_ENG_OWE);
	unsigned int i = 0;
	unsigned int size_sec = 0;

	if (frame == NULL || frame->data == NULL)
		return -1;

	LOG_DBG("%s request sent to CMDQ driver", __func__);
	pWmfeCfg = (struct OWE_WMFEConfig *) frame->data;

	if (OWE_DBG_DBGLOG == (OWE_DBG_DBGLOG & OWEInfo.DebugMask)) {

		LOG_DBG("ConfigWMFEHW Start!\n");
		for (i = 0; i < pWmfeCfg->WmfeCtrlSize; i++) {
			LOG_DBG("WMFE_CTRL_%d_REG:0x%x!\n",
				i, pWmfeCfg->WmfeCtrl[i].WMFE_CTRL);
			LOG_DBG("WMFE_SIZE_%d_REG:0x%x!\n",
				i, pWmfeCfg->WmfeCtrl[i].WMFE_SIZE);
			LOG_DBG("WMFE_IMGI_BASE_ADDR_%d_REG:0x%x!\n",
				i, pWmfeCfg->WmfeCtrl[i].WMFE_IMGI_BASE_ADDR);
			LOG_DBG("WMFE_IMGI_STRIDE_%d_REG:0x%x!\n",
				i, pWmfeCfg->WmfeCtrl[i].WMFE_IMGI_STRIDE);
			LOG_DBG("WMFE_DPI_BASE_ADDR_%d_REG:0x%x!\n",
				i, pWmfeCfg->WmfeCtrl[i].WMFE_DPI_BASE_ADDR);
			LOG_DBG("WMFE_DPI_STRIDE_%d_REG:0x%x!\n",
				i, pWmfeCfg->WmfeCtrl[i].WMFE_DPI_STRIDE);
			LOG_DBG("WMFE_TBLI_BASE_ADDR_%d_REG:0x%x!\n",
				i, pWmfeCfg->WmfeCtrl[i].WMFE_TBLI_BASE_ADDR);
			LOG_DBG("WMFE_TBLI_STRIDE_%d_REG:0x%x!\n",
				i, pWmfeCfg->WmfeCtrl[i].WMFE_TBLI_STRIDE);
			LOG_DBG("WMFE_MASKI_BASE_ADDR_%d_REG:0x%x!\n",
				i, pWmfeCfg->WmfeCtrl[i].WMFE_MASKI_BASE_ADDR);
			LOG_DBG("WMFE_MASKI_STRIDE_%d_REG:0x%x!\n",
				i, pWmfeCfg->WmfeCtrl[i].WMFE_MASKI_STRIDE);
			LOG_DBG("WMFE_DPO_BASE_ADDR_%d_REG:0x%x!\n",
				i, pWmfeCfg->WmfeCtrl[i].WMFE_DPO_BASE_ADDR);
			LOG_DBG("WMFE_DPO_STRIDE_%d_REG:0x%x!\n",
				i, pWmfeCfg->WmfeCtrl[i].WMFE_DPO_STRIDE);
		}
	}

	cmdqRecCreate(CMDQ_SCENARIO_KERNEL_CONFIG_GENERAL, &handle);
	/* CMDQ driver dispatches CMDQ HW thread and HW thread's
	 * priority according to scenario
	 */

	cmdqRecSetEngine(handle, engineFlag);

	cmdqRecReset(handle);

	if (pWmfeCfg->WmfeCtrl[0].eng_secured == 1)
		cmdq_engine_secured(handle, CMDQ_ENG_OWE);

#ifndef BYPASS_REG
	/* Use command queue to write register */
	cmdqRecWrite(handle,
		OWE_INT_CTL_HW,
		0x1, CMDQ_REG_MASK);	/* OWE Interrupt read-clear mode */
	cmdqRecWrite(handle,
		OWE_WMFE_INT_CTRL_HW,
		0x1, CMDQ_REG_MASK);	/* WMFE Interrupt read-clear mode */

	for (i = 0; i < pWmfeCfg->WmfeCtrlSize; i++) {
		LOG_DBG("OWE_WMFE_CTRL_%d_REG:0x%x!\n",
			i, pWmfeCfg->WmfeCtrl[i].WMFE_CTRL);

		if (WMFE_ENABLE ==
			(pWmfeCfg->WmfeCtrl[i].WMFE_CTRL & WMFE_ENABLE)) {
			cmdqRecWrite(handle,
				OWE_WMFE_CTRL_0_HW + (i*0x40),
				pWmfeCfg->WmfeCtrl[i].WMFE_CTRL,
				CMDQ_REG_MASK);
			cmdqRecWrite(handle,
				OWE_WMFE_SIZE_0_HW + (i*0x40),
				pWmfeCfg->WmfeCtrl[i].WMFE_SIZE,
				CMDQ_REG_MASK);
			cmdqRecWrite(handle,
				OWE_WMFE_IMGI_STRIDE_0_HW + (i*0x40),
				pWmfeCfg->WmfeCtrl[i].WMFE_IMGI_STRIDE,
				CMDQ_REG_MASK);
			cmdqRecWrite(handle,
				OWE_WMFE_DPI_STRIDE_0_HW + (i*0x40),
				pWmfeCfg->WmfeCtrl[i].WMFE_DPI_STRIDE,
				CMDQ_REG_MASK);
			cmdqRecWrite(handle,
				OWE_WMFE_TBLI_STRIDE_0_HW + (i*0x40),
				pWmfeCfg->WmfeCtrl[i].WMFE_TBLI_STRIDE,
				CMDQ_REG_MASK);
			cmdqRecWrite(handle,
				OWE_WMFE_MASKI_STRIDE_0_HW + (i*0x40),
				pWmfeCfg->WmfeCtrl[i].WMFE_MASKI_STRIDE,
				CMDQ_REG_MASK);
			cmdqRecWrite(handle,
				OWE_WMFE_DPO_STRIDE_0_HW + (i*0x40),
				pWmfeCfg->WmfeCtrl[i].WMFE_DPO_STRIDE,
				CMDQ_REG_MASK);

			size_sec =
				pWmfeCfg->WmfeCtrl[i].dma_sec_size[
								WMFE_DMA_IMGI];
			if (size_sec != 0)
				cmdqRecWriteSecure(handle,
					OWE_WMFE_IMGI_BASE_ADDR_0_HW + (i*0x40),
					CMDQ_SAM_H_2_MVA,
					pWmfeCfg->WmfeCtrl[
							i].WMFE_IMGI_BASE_ADDR,
					0, size_sec, M4U_PORT_CAM_OWE_RDMA);
			else
				cmdqRecWrite(handle,
					OWE_WMFE_IMGI_BASE_ADDR_0_HW + (i*0x40),
					pWmfeCfg->WmfeCtrl[
							i].WMFE_IMGI_BASE_ADDR,
					CMDQ_REG_MASK);

			size_sec =
				pWmfeCfg->WmfeCtrl[i].dma_sec_size[
								WMFE_DMA_DPI];
			if (size_sec != 0)
				cmdqRecWriteSecure(handle,
					OWE_WMFE_DPI_BASE_ADDR_0_HW + (i*0x40),
					CMDQ_SAM_H_2_MVA,
					pWmfeCfg->WmfeCtrl[
							i].WMFE_DPI_BASE_ADDR,
					0, size_sec, M4U_PORT_CAM_OWE_RDMA);
			else
				cmdqRecWrite(handle,
					OWE_WMFE_DPI_BASE_ADDR_0_HW + (i*0x40),
					pWmfeCfg->WmfeCtrl[
						i].WMFE_DPI_BASE_ADDR,
					CMDQ_REG_MASK);

			size_sec =
				pWmfeCfg->WmfeCtrl[i].dma_sec_size[
								WMFE_DMA_TBLI];
			if (size_sec != 0)
				cmdqRecWriteSecure(handle,
					OWE_WMFE_TBLI_BASE_ADDR_0_HW + (i*0x40),
					CMDQ_SAM_H_2_MVA,
					pWmfeCfg->WmfeCtrl[
						i].WMFE_TBLI_BASE_ADDR,
					0, size_sec, M4U_PORT_CAM_OWE_RDMA);
			else
				cmdqRecWrite(handle,
					OWE_WMFE_TBLI_BASE_ADDR_0_HW + (i*0x40),
					pWmfeCfg->WmfeCtrl[
						i].WMFE_TBLI_BASE_ADDR,
					CMDQ_REG_MASK);

			size_sec = pWmfeCfg->WmfeCtrl[i].dma_sec_size[
								WMFE_DMA_MASKI];
			if (size_sec != 0)
				cmdqRecWriteSecure(handle,
				    OWE_WMFE_MASKI_BASE_ADDR_0_HW + (i*0x40),
				    CMDQ_SAM_H_2_MVA,
				    pWmfeCfg->WmfeCtrl[
					i].WMFE_MASKI_BASE_ADDR,
				    0, size_sec, M4U_PORT_CAM_OWE_RDMA);
			else
				cmdqRecWrite(handle,
				    OWE_WMFE_MASKI_BASE_ADDR_0_HW + (i*0x40),
				    pWmfeCfg->WmfeCtrl[
					i].WMFE_MASKI_BASE_ADDR,
				    CMDQ_REG_MASK);

			size_sec = pWmfeCfg->WmfeCtrl[i].dma_sec_size[
								WMFE_DMA_DPO];
			if (size_sec != 0)
				cmdqRecWriteSecure(handle,
					OWE_WMFE_DPO_BASE_ADDR_0_HW + (i*0x40),
					CMDQ_SAM_H_2_MVA,
					pWmfeCfg->WmfeCtrl[
						i].WMFE_DPO_BASE_ADDR, 0,
					size_sec, M4U_PORT_CAM_OWE_WDMA);
			else
				cmdqRecWrite(handle,
					OWE_WMFE_DPO_BASE_ADDR_0_HW + (i*0x40),
					pWmfeCfg->WmfeCtrl[
						i].WMFE_DPO_BASE_ADDR,
					CMDQ_REG_MASK);

		}
	}
#endif
	cmdqRecWrite(handle,
		OWE_WMFE_START_HW,
		0x1, CMDQ_REG_MASK);	/* OWE Interrupt read-clear mode */

	cmdqRecWait(handle, CMDQ_EVENT_WMF_EOF);
	cmdqRecWrite(handle,
		OWE_WMFE_START_HW,
		0x0, CMDQ_REG_MASK);	/* OWE Interrupt read-clear mode */
	/* non-blocking API, Please  use cmdqRecFlushAsync() */
	cmdqRecFlushAsync(handle);
	cmdqRecReset(handle);	/* if you want to re-use the handle,
				 * please reset the handle
				 */
	cmdqRecDestroy(handle);	/* recycle the memory */

	return 0;
}
static const struct engine_ops wmfe_ops = {
	.req_enque_cb = wmfe_enque_cb,
	.req_deque_cb = wmfe_deque_cb,
	.frame_handler = CmdqWMFEHW,
	.req_feedback_cb = NULL,
};

/*
 *
 */
static signed int OWE_DumpReg(void)
{
	signed int Ret = 0;
	/*  */
	LOG_INF("- E.");
	/*  */
	LOG_INF("OCC Config Info\n");

#define PRINT_INF(REG) LOG_INF("[0x%08X %08X]\n", (unsigned int)(REG ##_HW), \
	(unsigned int)OWE_RD32(REG ##_REG))
	PRINT_INF(OWE_OCC_START);
	PRINT_INF(OWE_OCC_INT_CTRL);
	PRINT_INF(OWE_OCC_INT_STATUS);
	PRINT_INF(DPE_OCC_CTRL_0);
	PRINT_INF(DPE_OCC_CTRL_1);
	PRINT_INF(DPE_OCC_CTRL_2);
	PRINT_INF(DPE_OCC_CTRL_3);
	PRINT_INF(DPE_OCC_REF_VEC_BASE);
	PRINT_INF(DPE_OCC_REF_VEC_STRIDE);
	PRINT_INF(DPE_OCC_REF_PXL_BASE);
	PRINT_INF(DPE_OCC_REF_PXL_STRIDE);
	PRINT_INF(DPE_OCC_MAJ_VEC_BASE);
	PRINT_INF(DPE_OCC_MAJ_VEC_STRIDE);
	PRINT_INF(DPE_OCC_MAJ_PXL_BASE);
	PRINT_INF(DPE_OCC_MAJ_PXL_STRIDE);
	PRINT_INF(DPE_OCC_WDMA_BASE);
	PRINT_INF(DPE_OCC_WDMA_STRIDE);
	PRINT_INF(DPE_OCC_PQ_0);
	PRINT_INF(DPE_OCC_PQ_1);
	PRINT_INF(DPE_OCC_SPARE);
	PRINT_INF(DPE_OCC_DFT);
	LOG_INF("OCC Debug Info\n");



	LOG_INF("WMFE Config Info\n");
	/* WMFE Config0 */
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_START_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_START_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_CTRL_0_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_CTRL_0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_SIZE_0_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_SIZE_0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_IMGI_BASE_ADDR_0_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_IMGI_BASE_ADDR_0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_IMGI_STRIDE_0_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_IMGI_STRIDE_0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPI_BASE_ADDR_0_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPI_BASE_ADDR_0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPI_STRIDE_0_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPI_STRIDE_0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_TBLI_BASE_ADDR_0_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_TBLI_BASE_ADDR_0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_TBLI_STRIDE_0_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_TBLI_STRIDE_0_REG));
	LOG_INF("[0x%08X %08X]\n",
		(unsigned int)(OWE_WMFE_MASKI_BASE_ADDR_0_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_MASKI_BASE_ADDR_0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_MASKI_STRIDE_0_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_MASKI_STRIDE_0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPO_BASE_ADDR_0_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPO_BASE_ADDR_0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPO_STRIDE_0_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPO_STRIDE_0_REG));


	/* WMFE Config1 */
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_CTRL_1_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_CTRL_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_SIZE_1_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_SIZE_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_IMGI_BASE_ADDR_1_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_IMGI_BASE_ADDR_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_IMGI_STRIDE_1_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_IMGI_STRIDE_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPI_BASE_ADDR_1_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPI_BASE_ADDR_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPI_STRIDE_1_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPI_STRIDE_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_TBLI_BASE_ADDR_1_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_TBLI_BASE_ADDR_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_TBLI_STRIDE_1_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_TBLI_STRIDE_1_REG));
	LOG_INF("[0x%08X %08X]\n",
		(unsigned int)(OWE_WMFE_MASKI_BASE_ADDR_1_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_MASKI_BASE_ADDR_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_MASKI_STRIDE_1_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_MASKI_STRIDE_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPO_BASE_ADDR_1_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPO_BASE_ADDR_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPO_STRIDE_1_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPO_STRIDE_1_REG));

	/* WMFE Config2 */
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_CTRL_2_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_CTRL_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_SIZE_2_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_SIZE_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_IMGI_BASE_ADDR_2_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_IMGI_BASE_ADDR_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_IMGI_STRIDE_2_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_IMGI_STRIDE_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPI_BASE_ADDR_2_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPI_BASE_ADDR_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPI_STRIDE_2_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPI_STRIDE_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_TBLI_BASE_ADDR_2_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_TBLI_BASE_ADDR_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_TBLI_STRIDE_2_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_TBLI_STRIDE_2_REG));
	LOG_INF("[0x%08X %08X]\n",
		(unsigned int)(OWE_WMFE_MASKI_BASE_ADDR_2_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_MASKI_BASE_ADDR_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_MASKI_STRIDE_2_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_MASKI_STRIDE_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPO_BASE_ADDR_2_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPO_BASE_ADDR_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPO_STRIDE_2_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPO_STRIDE_2_REG));

	/* WMFE Config3 */
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_CTRL_3_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_CTRL_3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_SIZE_3_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_SIZE_3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_IMGI_BASE_ADDR_3_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_IMGI_BASE_ADDR_3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_IMGI_STRIDE_3_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_IMGI_STRIDE_3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPI_BASE_ADDR_3_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPI_BASE_ADDR_3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPI_STRIDE_3_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPI_STRIDE_3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_TBLI_BASE_ADDR_3_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_TBLI_BASE_ADDR_3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_TBLI_STRIDE_3_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_TBLI_STRIDE_3_REG));
	LOG_INF("[0x%08X %08X]\n",
		(unsigned int)(OWE_WMFE_MASKI_BASE_ADDR_3_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_MASKI_BASE_ADDR_3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_MASKI_STRIDE_3_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_MASKI_STRIDE_3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPO_BASE_ADDR_3_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPO_BASE_ADDR_3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPO_STRIDE_3_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPO_STRIDE_3_REG));

	/* WMFE Config4 */
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_CTRL_4_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_CTRL_4_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_SIZE_4_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_SIZE_4_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_IMGI_BASE_ADDR_4_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_IMGI_BASE_ADDR_4_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_IMGI_STRIDE_4_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_IMGI_STRIDE_4_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPI_BASE_ADDR_4_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPI_BASE_ADDR_4_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPI_STRIDE_4_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPI_STRIDE_4_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_TBLI_BASE_ADDR_4_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_TBLI_BASE_ADDR_4_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_TBLI_STRIDE_4_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_TBLI_STRIDE_4_REG));
	LOG_INF("[0x%08X %08X]\n",
		(unsigned int)(OWE_WMFE_MASKI_BASE_ADDR_4_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_MASKI_BASE_ADDR_4_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_MASKI_STRIDE_4_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_MASKI_STRIDE_4_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPO_BASE_ADDR_4_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPO_BASE_ADDR_4_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DPO_STRIDE_4_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DPO_STRIDE_4_REG));


	LOG_INF("WMFE Debug Info\n");
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DBG_INFO_00_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DBG_INFO_00_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DBG_INFO_01_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DBG_INFO_01_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DBG_INFO_02_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DBG_INFO_02_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DBG_INFO_03_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DBG_INFO_03_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DBG_INFO_04_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DBG_INFO_04_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DBG_INFO_05_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DBG_INFO_05_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DBG_INFO_06_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DBG_INFO_06_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DBG_INFO_07_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DBG_INFO_07_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DBG_INFO_08_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DBG_INFO_08_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_WMFE_DBG_INFO_09_HW),
		(unsigned int)OWE_RD32(OWE_WMFE_DBG_INFO_09_REG));

	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_DMA_DBG_HW),
		(unsigned int)OWE_RD32(OWE_DMA_DBG_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_DMA_REQ_STATUS_HW),
		(unsigned int)OWE_RD32(OWE_DMA_REQ_STATUS_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(OWE_DMA_RDY_STATUS_HW),
		(unsigned int)OWE_RD32(OWE_DMA_RDY_STATUS_REG));

	request_dump(&occ_reqs);
	request_dump(&wmfe_reqs);

	LOG_INF("- X.");
	/*  */
	return Ret;
}
#ifndef __OWE_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
static inline void OWE_Prepare_Enable_ccf_clock(void)
{
	int ret;
	/* must keep this clk open order:
	 * CG_SCP_SYS_DIS-> CG_MM_SMI_COMMON -> CG_SCP_SYS_ISP -> OWE clk
	 */
	smi_bus_prepare_enable(SMI_LARB5, OWE_DEV_NAME);
	ret = clk_prepare_enable(owe_clk.CG_IMGSYS_OWE);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IMGSYS_OWE clock\n");

}

static inline void OWE_Disable_Unprepare_ccf_clock(void)
{
	/* must keep this clk close order:
	 * OWE clk -> CG_SCP_SYS_ISP -> CG_MM_SMI_COMMON -> CG_SCP_SYS_DIS
	 */

	clk_disable_unprepare(owe_clk.CG_IMGSYS_OWE);
	smi_bus_disable_unprepare(SMI_LARB5, OWE_DEV_NAME);
}
#endif
#endif
/******************************************************************************
 *
 ******************************************************************************/
static void OWE_EnableClock(bool En)
{
	if (En) {/* Enable clock. */
		/* LOG_DBG("Owe clock enbled. g_u4EnableClockCount: %d.", */
		/* g_u4EnableClockCount); */
		switch (g_u4EnableClockCount) {
		case 0:
#ifndef __OWE_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
			    OWE_Prepare_Enable_ccf_clock();
#else
			enable_clock(MT_CG_DOWE0_SMI_COMMON, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			/* enable_clock(MT_CG_IMAGE_FD, "CAMERA"); */
			enable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
#endif	/* #if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */
#endif
			break;
		default:
			break;
		}
		spin_lock(&(OWEInfo.SpinLockOWE));
		g_u4EnableClockCount++;
		spin_unlock(&(OWEInfo.SpinLockOWE));
	} else {		/* Disable clock. */

		/* LOG_DBG("Owe clock disabled. g_u4EnableClockCount: %d.", */
		/* g_u4EnableClockCount); */
		spin_lock(&(OWEInfo.SpinLockOWE));
		g_u4EnableClockCount--;
		spin_unlock(&(OWEInfo.SpinLockOWE));
		switch (g_u4EnableClockCount) {
		case 0:
#ifndef __OWE_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
			    OWE_Disable_Unprepare_ccf_clock();
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

/*****************************************************************************
 *
 ******************************************************************************/
static inline void OWE_Reset(void)
{
	LOG_DBG("- E.");

	LOG_DBG(" OWE Reset start!\n");
	spin_lock(&(OWEInfo.SpinLockOWERef));

	if (OWEInfo.UserCount > 1) {
		spin_unlock(&(OWEInfo.SpinLockOWERef));
		LOG_DBG("Curr UserCount(%d) users exist", OWEInfo.UserCount);
	} else {
		spin_unlock(&(OWEInfo.SpinLockOWERef));

		/* Reset OWE flow */
		OWE_WR32(OWE_RST_REG, 0x1);
		while ((OWE_RD32(OWE_RST_REG) & 0x02) != 0x2)
			LOG_DBG("OWE resetting...\n");

		OWE_WR32(OWE_RST_REG, 0x11);
		OWE_WR32(OWE_RST_REG, 0x10);
		OWE_WR32(OWE_RST_REG, 0x0);
		OWE_WR32(OWE_OCC_START_REG, 0);
		OWE_WR32(OWE_WMFE_START_REG, 0);
		LOG_DBG(" OWE Reset end!\n");
	}

}

/******************************************************************************
 *
 ******************************************************************************/
static signed int OWE_ReadReg(struct OWE_REG_IO_STRUCT *pRegIo)
{
	unsigned int i;
	signed int Ret = 0;
	/*  */
	/* unsigned int* pData = (unsigned int*)pRegIo->Data; */
	struct OWE_REG_STRUCT *pData = NULL;
	struct OWE_REG_STRUCT *pTmpData = NULL;

	if ((pRegIo->pData == NULL) || (pRegIo->Count == 0) ||
		(pRegIo->Count > (OWE_REG_RANGE>>2))) {
		LOG_ERR("pRegIo->pData is NULL, Count:%d!!",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	pData = kmalloc(
		(pRegIo->Count) * sizeof(struct OWE_REG_STRUCT), GFP_KERNEL);
	if (pData == NULL) {
		LOG_ERR("ERROR: kmalloc failed, cnt:%d\n",
			pRegIo->Count);
		Ret = -ENOMEM;
		goto EXIT;
	}
	pTmpData = pData;
	if (copy_from_user(pData, (void *)pRegIo->pData,
		(pRegIo->Count) * sizeof(struct OWE_REG_STRUCT)) == 0) {
		for (i = 0; i < pRegIo->Count; i++) {
			if ((ISP_OWE_BASE + pData->Addr >= ISP_OWE_BASE)
			    && (ISP_OWE_BASE + pData->Addr <
				(ISP_OWE_BASE + OWE_REG_RANGE))
			    && ((pData->Addr & 0x3) == 0)) {
				pData->Val =
					OWE_RD32(ISP_OWE_BASE + pData->Addr);
			} else {
				LOG_ERR("Wrong address(0x%p)",
					(ISP_OWE_BASE + pData->Addr));
				pData->Val = 0;
			}
			pData++;
		}
		pData = pTmpData;
		if (copy_to_user((void *)pRegIo->pData, pData,
			(pRegIo->Count) * sizeof(struct OWE_REG_STRUCT)) != 0) {
			LOG_ERR("copy_to_user failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}
	} else {
		LOG_ERR("OWE_READ_REGISTER copy_from_user failed");
		Ret = -EFAULT;
		goto EXIT;
	}

	/*  */
EXIT:
	if (pData != NULL) {
		kfree(pData);
		pData = NULL;
	}
	return Ret;
}


/******************************************************************************
 *
 ******************************************************************************/
/* Can write sensor's test model only, if need write to other modules,
 * need modify current code flow
 */
static signed int OWE_WriteRegToHw(
	struct OWE_REG_STRUCT *pReg, unsigned int Count)
{
	signed int Ret = 0;
	unsigned int i;
	bool dbgWriteReg;

	/* Use local variable to store OWEInfo.DebugMask & OWE_DBG_WRITE_REG
	 * for saving lock time
	 */
	spin_lock(&(OWEInfo.SpinLockOWE));
	dbgWriteReg = OWEInfo.DebugMask & OWE_DBG_WRITE_REG;
	spin_unlock(&(OWEInfo.SpinLockOWE));

	/*  */
	if (dbgWriteReg)
		LOG_DBG("- E.\n");

	/*  */
	for (i = 0; i < Count; i++) {
		if (dbgWriteReg) {
			LOG_DBG("Addr(0x%lx), Val(0x%x)\n",
				(unsigned long)(ISP_OWE_BASE + pReg[i].Addr),
				(unsigned int) (pReg[i].Val));
		}

		if (((ISP_OWE_BASE + pReg[i].Addr) <
			(ISP_OWE_BASE + OWE_REG_RANGE))
			&& ((pReg[i].Addr & 0x3) == 0)) {
			OWE_WR32(ISP_OWE_BASE + pReg[i].Addr, pReg[i].Val);
		} else {
			LOG_ERR("wrong address(0x%lx)\n",
				(unsigned long)(ISP_OWE_BASE + pReg[i].Addr));
		}
	}

	/*  */
	return Ret;
}



/******************************************************************************
 *
 ******************************************************************************/
static signed int OWE_WriteReg(struct OWE_REG_IO_STRUCT *pRegIo)
{
	signed int Ret = 0;
	/* unsigned char* pData = NULL; */
	struct  OWE_REG_STRUCT *pData = NULL;
	/*  */
	if (OWEInfo.DebugMask & OWE_DBG_WRITE_REG)
		LOG_DBG("Data(0x%p), Count(%d)\n",
			(pRegIo->pData), (pRegIo->Count));

	if ((pRegIo->pData == NULL) || (pRegIo->Count == 0) ||
			(pRegIo->Count > (OWE_REG_RANGE>>2))) {
		LOG_ERR("ERROR: pRegIo->pData is NULL or Count:%d\n",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	/* pData = (unsigned char*)kmalloc( */
	/*	(pRegIo->Count)*sizeof(OWE_REG_STRUCT), GFP_ATOMIC); */
	pData = kmalloc(
		(pRegIo->Count) * sizeof(struct OWE_REG_STRUCT), GFP_KERNEL);
	if (pData == NULL) {
		LOG_ERR(
			"ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)\n",
			current->comm,
			current->pid, current->tgid);
		Ret = -ENOMEM;
		goto EXIT;
	}
	/*  */
	if (copy_from_user(pData, (void __user *)(pRegIo->pData),
		pRegIo->Count * sizeof(struct OWE_REG_STRUCT)) != 0) {
		LOG_ERR("copy_from_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	Ret = OWE_WriteRegToHw(pData, pRegIo->Count);
	/*  */
EXIT:
	if (pData != NULL) {
		kfree(pData);
		pData = NULL;
	}
	return Ret;
}


/******************************************************************************
 *
 ******************************************************************************/
static signed int OWE_WaitIrq(struct OWE_WAIT_IRQ_STRUCT *WaitIrq)
{

	signed int Ret = 0;
	signed int Timeout = WaitIrq->Timeout;
	enum OWE_PROCESS_ID_ENUM whichReq = OWE_PROCESS_ID_NONE;

	/*unsigned int i;*/
	unsigned long flags; /* old: unsigned int flags;*/
			     /* FIX to avoid build warning */
	unsigned int irqStatus;
	/*int cnt = 0;*/
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
	if (OWEInfo.DebugMask & OWE_DBG_INT) {
		if (WaitIrq->Status & OWEInfo.IrqInfo.Mask[WaitIrq->Type]) {
			if (WaitIrq->UserKey > 0) {
				LOG_DBG(
					"+WaitIrq Clr(%d),Type(%d),Sta(0x%08X),Timeout(%d),user(%d),PID(%d)\n",
					WaitIrq->Clear,
					WaitIrq->Type,
					WaitIrq->Status,
					WaitIrq->Timeout,
					WaitIrq->UserKey,
					WaitIrq->ProcessID);
			}
		}
	}


	/* 1. wait type update */
	if (WaitIrq->Clear == OWE_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(
			&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		OWEInfo.IrqInfo.Status[WaitIrq->Type] &= (~WaitIrq->Status);
		spin_unlock_irqrestore(
			&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		return Ret;
	}

	if (WaitIrq->Clear == OWE_IRQ_CLEAR_WAIT) {
		spin_lock_irqsave(
			&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		if (OWEInfo.IrqInfo.Status[WaitIrq->Type] & WaitIrq->Status)
			OWEInfo.IrqInfo.Status[WaitIrq->Type] &=
				(~WaitIrq->Status);

		spin_unlock_irqrestore(
			&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);
	} else if (WaitIrq->Clear == OWE_IRQ_CLEAR_ALL) {
		spin_lock_irqsave(
			&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);

		OWEInfo.IrqInfo.Status[WaitIrq->Type] = 0;
		spin_unlock_irqrestore(
			&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);
	}
	/* OWE_IRQ_WAIT_CLEAR ==> do nothing */


	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
	spin_lock_irqsave(&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);
	irqStatus = OWEInfo.IrqInfo.Status[WaitIrq->Type];
	spin_unlock_irqrestore(&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);

	if (WaitIrq->Status & OWE_OCC_INT_ST) {
		whichReq = OWE_PROCESS_ID_OCC;
	} else if (WaitIrq->Status & OWE_WMFE_INT_ST) {
		whichReq = OWE_PROCESS_ID_WMFE;
	} else {
		LOG_ERR(
			"No Such Stats can be waited!! irq Type/User/Sts/Pid(0x%x/%d/0x%x/%d)\n",
			WaitIrq->Type,
			WaitIrq->UserKey,
			WaitIrq->Status,
			WaitIrq->ProcessID);
	}


#ifdef OWE_WAITIRQ_LOG
	LOG_INF(
		"before wait_event! Timeout(%d)Clear(%d),Type(%d),IrqSta(0x%08X), WaitSta(0x%08X)\n",
		WaitIrq->Timeout, WaitIrq->Clear,
		WaitIrq->Type, irqStatus, WaitIrq->Status);
	LOG_INF(
		"urKey(%d),whReq(%d),PID(%d)\n",
		WaitIrq->UserKey, whichReq, WaitIrq->ProcessID);
	LOG_INF(
		"OccIrqCnt(0x%08X),WmfeIrqCnt(0x%08X),WriteReqIdx(0x%08X),ReadReqIdx(0x%08X)\n",
		OWEInfo.IrqInfo.OccIrqCnt, OWEInfo.IrqInfo.WmfeIrqCnt,
		OWEInfo.WriteReqIdx, OWEInfo.ReadReqIdx);
#endif

	/* 2. start to wait signal */
	Timeout = wait_event_interruptible_timeout(OWEInfo.WaitQueueHead,
				OWE_GetIRQState(WaitIrq->Type, WaitIrq->UserKey,
				WaitIrq->Status, whichReq,
				WaitIrq->ProcessID),
				OWE_MsToJiffies(WaitIrq->Timeout));

	/* check if user is interrupted by system signal */
	if ((Timeout != 0) &&
	    (!OWE_GetIRQState(
		WaitIrq->Type,
		WaitIrq->UserKey,
		WaitIrq->Status,
		whichReq,
		WaitIrq->ProcessID))) {
		LOG_INF(
			"waked up by sys. signal,ret(%d),irq Type/User/Sts/whReq/Pid(0x%x/%d/0x%x/%d/%d)\n",
			Timeout, WaitIrq->Type, WaitIrq->UserKey,
			WaitIrq->Status, whichReq,
			WaitIrq->ProcessID);
		Ret = -ERESTARTSYS;	/* actually it should be -ERESTARTSYS */
		goto EXIT;
	}
	/* timeout */
	if (Timeout == 0) {
		/* Store irqinfo status in here to redeuce time of
		 * spin_lock_irqsave
		 */
		spin_lock_irqsave(
			&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = OWEInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);

		LOG_ERR(
			"ERRRR Timeout!Timeout(%d)Clear(%d),Type(%d),IrqSta(0x%08X), WaitSta(0x%08X)\n",
			WaitIrq->Timeout, WaitIrq->Clear,
			WaitIrq->Type, irqStatus, WaitIrq->Status);
		LOG_ERR("urKey(%d),whReq(%d),PID(%d)\n",
			WaitIrq->UserKey, whichReq, WaitIrq->ProcessID);
		LOG_ERR(
			"OccIrqCnt(0x%08X),WmfeIrqCnt(0x%08X),WriteReqIdx(0x%08X),ReadReqIdx(0x%08X)\n",
			OWEInfo.IrqInfo.OccIrqCnt, OWEInfo.IrqInfo.WmfeIrqCnt,
			OWEInfo.WriteReqIdx, OWEInfo.ReadReqIdx);

		if (WaitIrq->bDumpReg)
			OWE_DumpReg();

		Ret = -EFAULT;
		goto EXIT;
	} else {
		/* Store irqinfo status in here to redeuce time of
		 * spin_lock_irqsave
		 */
#ifdef __OWE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("OWE_WaitIrq");
#endif

		spin_lock_irqsave(
			&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = OWEInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);

		if (WaitIrq->Clear == OWE_IRQ_WAIT_CLEAR) {
			spin_lock_irqsave(
				&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);

#ifdef OWE_MULTIPROCESS_TIMEING_ISSUE
			OWEInfo.ReadReqIdx = (OWEInfo.ReadReqIdx + 1) %
						_SUPPORT_MAX_OWE_FRAME_REQUEST_;
			/* actually, it doesn't happen the timging issue!! */
			/* wake_up_interruptible(&OWEInfo.WaitQueueHead); */
#endif
			if (WaitIrq->Status & OWE_OCC_INT_ST) {
				OWEInfo.IrqInfo.OccIrqCnt--;
				if (OWEInfo.IrqInfo.OccIrqCnt == 0)
					OWEInfo.IrqInfo.Status[WaitIrq->Type] &=
						(~WaitIrq->Status);
			} else if (WaitIrq->Status & OWE_WMFE_INT_ST) {
				OWEInfo.IrqInfo.WmfeIrqCnt--;
				if (OWEInfo.IrqInfo.WmfeIrqCnt == 0)
					OWEInfo.IrqInfo.Status[WaitIrq->Type] &=
						(~WaitIrq->Status);
			} else {
				LOG_ERR(
				"OWE_IRQ_WAIT_CLEAR Error, Type(%d), WaitStatus(0x%08X)",
					WaitIrq->Type, WaitIrq->Status);
			}
			spin_unlock_irqrestore(
				&(OWEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		}

#ifdef OWE_WAITIRQ_LOG
		LOG_INF(
			"no Timeout!Timeout(%d)Clear(%d),Type(%d),IrqSta(0x%08X), WaitSta(0x%08X)\n",
			WaitIrq->Timeout, WaitIrq->Clear,
			WaitIrq->Type, irqStatus, WaitIrq->Status);
		LOG_INF("urKey(%d),whReq(%d),PID(%d)\n",
			WaitIrq->UserKey, whichReq, WaitIrq->ProcessID);
		LOG_INF(
			"OccIrqCnt(0x%08X),WmfeIrqCnt(0x%08X),WriteReqIdx(0x%08X),ReadReqIdx(0x%08X)\n",
			OWEInfo.IrqInfo.OccIrqCnt, OWEInfo.IrqInfo.WmfeIrqCnt,
			OWEInfo.WriteReqIdx, OWEInfo.ReadReqIdx);

#endif

#ifdef __OWE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif

	}


EXIT:


	return Ret;
}


/******************************************************************************
 *
 ******************************************************************************/
static long OWE_ioctl(struct file *pFile, unsigned int Cmd, unsigned long Param)
{
	signed int Ret = 0;

	/*unsigned int pid = 0;*/
	struct OWE_REG_IO_STRUCT RegIo;
	struct OWE_WAIT_IRQ_STRUCT IrqInfo;
	struct OWE_CLEAR_IRQ_STRUCT ClearIrq;
	struct OWE_OCCRequest owe_OccReq;
	struct OWE_WMFERequest owe_WmfeReq;
	struct OWE_USER_INFO_STRUCT *pUserInfo;
	int dequeNum;
	unsigned long flags; /* old: unsigned int flags;*/
			     /* FIX to avoid build warning */

	/*  */
	if (pFile->private_data == NULL) {
		LOG_WRN(
			"private_data is NULL,(process, pid, tgid)=(%s, %d, %d)",
			current->comm,
			current->pid, current->tgid);
		return -EFAULT;
	}
	/*  */
	pUserInfo = (struct OWE_USER_INFO_STRUCT *) (pFile->private_data);
	/*  */
	switch (Cmd) {
	case OWE_RESET:
		{
			spin_lock(&(OWEInfo.SpinLockOWE));
			OWE_Reset();
			spin_unlock(&(OWEInfo.SpinLockOWE));
			break;
		}

		/*  */
	case OWE_DUMP_REG:
		{
			Ret = OWE_DumpReg();
			break;
		}
	case OWE_DUMP_ISR_LOG:
		{
			unsigned int currentPPB = m_CurrentPPB;

			spin_lock_irqsave(
				&(OWEInfo.SpinLockIrq[OWE_IRQ_TYPE_INT_OWE_ST]),
				flags);
			m_CurrentPPB = (m_CurrentPPB + 1) % LOG_PPNUM;
			spin_unlock_irqrestore(
				&(OWEInfo.SpinLockIrq[OWE_IRQ_TYPE_INT_OWE_ST]),
				flags);

			IRQ_LOG_PRINTER(OWE_IRQ_TYPE_INT_OWE_ST,
				currentPPB, _LOG_INF);
			IRQ_LOG_PRINTER(OWE_IRQ_TYPE_INT_OWE_ST,
				currentPPB, _LOG_ERR);
			break;
		}
	case OWE_READ_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param,
				sizeof(struct OWE_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir of copy from user is
				 * implemented in OWE_ReadReg(...)
				 */
				Ret = OWE_ReadReg(&RegIo);
			} else {
				LOG_ERR(
					"OWE_READ_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case OWE_WRITE_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param,
				sizeof(struct OWE_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir of copy from user is
				 * implemented in OWE_WriteReg(...)
				 */
				Ret = OWE_WriteReg(&RegIo);
			} else {
				LOG_ERR(
					"OWE_WRITE_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case OWE_WAIT_IRQ:
		{
			if (copy_from_user(&IrqInfo, (void *)Param,
				sizeof(struct OWE_WAIT_IRQ_STRUCT)) == 0) {
				/*  */
				if ((IrqInfo.Type >= OWE_IRQ_TYPE_AMOUNT) ||
					(IrqInfo.Type < 0)) {
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
					IrqInfo.Clear, IrqInfo.Type,
					IrqInfo.UserKey, IrqInfo.Timeout,
					IrqInfo.Status);

				IrqInfo.ProcessID = pUserInfo->Pid;
				Ret = OWE_WaitIrq(&IrqInfo);
				if (Ret < 0)
					OWE_DumpReg();

				if (copy_to_user((void *)Param, &IrqInfo,
				    sizeof(struct OWE_WAIT_IRQ_STRUCT)) != 0) {
					LOG_ERR("copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR("OWE_WAIT_IRQ copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case OWE_CLEAR_IRQ:
		{
			if (copy_from_user(&ClearIrq, (void *)Param,
				sizeof(struct OWE_CLEAR_IRQ_STRUCT)) == 0) {
				LOG_DBG("OWE_CLEAR_IRQ Type(%d)",
					ClearIrq.Type);

				if ((ClearIrq.Type >= OWE_IRQ_TYPE_AMOUNT) ||
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
					"OWE_CLEAR_IRQ:Type(%d),Status(0x%08X),IrqStatus(0x%08X)\n",
					ClearIrq.Type, ClearIrq.Status,
					OWEInfo.IrqInfo.Status[ClearIrq.Type]);
				spin_lock_irqsave(
					&(OWEInfo.SpinLockIrq[ClearIrq.Type]),
					flags);
				OWEInfo.IrqInfo.Status[ClearIrq.Type] &=
					(~ClearIrq.Status);
				spin_unlock_irqrestore(
					&(OWEInfo.SpinLockIrq[ClearIrq.Type]),
					flags);
			} else {
				LOG_ERR(
					"OWE_CLEAR_IRQ copy_from_user failed\n");
				Ret = -EFAULT;
			}
			break;
		}
	case OWE_OCC_ENQUE_REQ:
		{
		if (copy_from_user(&owe_OccReq, (void *)Param,
			sizeof(struct OWE_OCCRequest)) == 0) {
			LOG_DBG("OCC_ENQNUE_NUM:%d, pid:%d\n",
				owe_OccReq.m_ReqNum,
				pUserInfo->Pid);
			if (owe_OccReq.m_ReqNum >
				_SUPPORT_MAX_OWE_FRAME_REQUEST_) {
				LOG_ERR(
					"OCC Enque Num is bigger than enqueNum:%d\n",
					owe_OccReq.m_ReqNum);
				Ret = -EFAULT;
				goto EXIT;
			}

			if (owe_OccReq.m_pOweConfig == NULL) {
				LOG_ERR("NULL OCC user Config\n");
				Ret = -EFAULT;
				goto EXIT;
			}

			if (copy_from_user(g_OccEnqueReq_Struct.OccFrameConfig,
				(void *)owe_OccReq.m_pOweConfig,
				owe_OccReq.m_ReqNum * sizeof(
					struct OWE_OCCConfig)
				) != 0) {
				LOG_ERR(
					"copy OCCConfig from request is fail!!\n");
				Ret = -EFAULT;
				goto EXIT;
			}

			mutex_lock(&gOweOccMutex);/* Protect the Multi Process*/

			spin_lock_irqsave(
				&(OWEInfo.SpinLockIrq[OWE_IRQ_TYPE_INT_OWE_ST]),
				flags);
			kOccReq.m_ReqNum = owe_OccReq.m_ReqNum;
			kOccReq.m_pOweConfig =
					g_OccEnqueReq_Struct.OccFrameConfig;
			enque_request(&occ_reqs, kOccReq.m_ReqNum, &kOccReq,
								pUserInfo->Pid);
			spin_unlock_irqrestore(
				&(OWEInfo.SpinLockIrq[OWE_IRQ_TYPE_INT_OWE_ST]),
				flags);
			LOG_DBG("ConfigOCC Request!!\n");
			if (!request_running(&occ_reqs)) {
				LOG_DBG("direct request_handler\n");
				request_handler(&occ_reqs,
						&(OWEInfo.SpinLockIrq[
						OWE_IRQ_TYPE_INT_OWE_ST]));
			}
			mutex_unlock(&gOweOccMutex);
		} else {
			LOG_ERR("OWE_OCC_ENQUE copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
		}
	case OWE_OCC_DEQUE_REQ:
		{
			if (copy_from_user(&owe_OccReq, (void *)Param,
					sizeof(struct OWE_OCCRequest)) == 0) {
				mutex_lock(&gOweOccDequeMutex);
				/* Protect the Multi Process */

				spin_lock_irqsave(&(OWEInfo.SpinLockIrq[
						OWE_IRQ_TYPE_INT_OWE_ST]),
						  flags);
				kOccReq.m_pOweConfig =
					g_OccDequeReq_Struct.OccFrameConfig;
				deque_request(&occ_reqs, &kOccReq.m_ReqNum,
					&kOccReq);
				dequeNum = kOccReq.m_ReqNum;
				owe_OccReq.m_ReqNum = dequeNum;
				spin_unlock_irqrestore(
					&(OWEInfo.SpinLockIrq[
						OWE_IRQ_TYPE_INT_OWE_ST]),
					flags);

				mutex_unlock(&gOweOccDequeMutex);

				if (owe_OccReq.m_pOweConfig == NULL) {
					LOG_ERR("NULL OCC user Config\n");
					Ret = -EFAULT;
					goto EXIT;
				}

				if (copy_to_user(
					(void *)owe_OccReq.m_pOweConfig,
					&g_OccDequeReq_Struct.OccFrameConfig[0],
					dequeNum * sizeof(
						struct OWE_OCCConfig)) != 0) {
					LOG_ERR(
						"OWE_CMD_OCC_DEQUE_REQ copy_to_user frameconfig failed\n");
					Ret = -EFAULT;
				}
				if (copy_to_user((void *)Param, &owe_OccReq,
					sizeof(struct OWE_OCCRequest)) != 0) {
					LOG_ERR(
						"OWE_CMD_OCC_DEQUE_REQ copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR(
					"OWE_CMD_OCC_DEQUE_REQ copy_from_user failed\n");
				Ret = -EFAULT;
			}
			break;
		}
	case OWE_WMFE_ENQUE_REQ:
		{
		if (copy_from_user(&owe_WmfeReq, (void *)Param,
			sizeof(struct OWE_WMFERequest)) == 0) {
			LOG_DBG("WMFE_ENQNUE_NUM:%d, pid:%d\n",
				owe_WmfeReq.m_ReqNum,
				pUserInfo->Pid);
			if (owe_WmfeReq.m_ReqNum >
				_SUPPORT_MAX_OWE_FRAME_REQUEST_) {
				LOG_ERR(
					"WMFE Enque Num is bigger than enqueNum:%d\n",
					owe_WmfeReq.m_ReqNum);
				Ret = -EFAULT;
				goto EXIT;
			}

			if (owe_WmfeReq.m_pWmfeConfig == NULL) {
				LOG_ERR("NULL WMFE user Config\n");
				Ret = -EFAULT;
				goto EXIT;
			}

			if (copy_from_user(
				g_WmfeEnqueReq_Struct.WmfeFrameConfig,
				(void *)owe_WmfeReq.m_pWmfeConfig,
				owe_WmfeReq.m_ReqNum *
					sizeof(struct OWE_WMFEConfig)) != 0) {
				LOG_ERR(
					"copy WMFEConfig from request is fail!!\n");
				Ret = -EFAULT;
				goto EXIT;
			}

			/* Protect the Multi Process */
			mutex_lock(&gOweWmfeMutex);

			spin_lock_irqsave(
				&(OWEInfo.SpinLockIrq[OWE_IRQ_TYPE_INT_OWE_ST]),
				flags);
			kWmfeReq.m_ReqNum = owe_WmfeReq.m_ReqNum;
			kWmfeReq.m_pWmfeConfig =
				g_WmfeEnqueReq_Struct.WmfeFrameConfig;
			enque_request(&wmfe_reqs,
				kWmfeReq.m_ReqNum,
				&kWmfeReq, pUserInfo->Pid);
			spin_unlock_irqrestore(
				&(OWEInfo.SpinLockIrq[OWE_IRQ_TYPE_INT_OWE_ST]),
				flags);

			LOG_DBG("ConfigWMFE Request!!\n");
			if (!request_running(&wmfe_reqs)) {
				LOG_DBG("direct request_handler\n");
				request_handler(
					&wmfe_reqs,
					&(OWEInfo.SpinLockIrq[
						OWE_IRQ_TYPE_INT_OWE_ST])
					);
			}
			mutex_unlock(&gOweWmfeMutex);
		} else {
			LOG_ERR("OWE_OCC_ENQUE copy_from_user failed\n");
			Ret = -EFAULT;
		}

			break;
		}
	case OWE_WMFE_DEQUE_REQ:
		{
			if (copy_from_user(&owe_WmfeReq, (void *)Param,
				sizeof(struct OWE_WMFERequest)) == 0) {
				mutex_lock(&gOweWmfeDequeMutex);
				/* Protect the Multi Process */

				spin_lock_irqsave(
					&(OWEInfo.SpinLockIrq[
						OWE_IRQ_TYPE_INT_OWE_ST]),
					flags);
				kWmfeReq.m_pWmfeConfig =
					g_WmfeDequeReq_Struct.WmfeFrameConfig;
				deque_request(
					&wmfe_reqs,
					&kWmfeReq.m_ReqNum,
					&kWmfeReq);
				dequeNum = kWmfeReq.m_ReqNum;
				owe_WmfeReq.m_ReqNum = dequeNum;

				spin_unlock_irqrestore(
					&(OWEInfo.SpinLockIrq[
						OWE_IRQ_TYPE_INT_OWE_ST]),
					flags);

				mutex_unlock(&gOweWmfeDequeMutex);

				if (owe_WmfeReq.m_pWmfeConfig == NULL) {
					LOG_ERR("NULL WMFE user Config\n");
					Ret = -EFAULT;
					goto EXIT;
				}

				if (copy_to_user(
					(void *)owe_WmfeReq.m_pWmfeConfig,
				     &g_WmfeDequeReq_Struct.WmfeFrameConfig[0],
				     dequeNum *
					sizeof(struct OWE_WMFEConfig)) != 0) {
					LOG_ERR(
						"OWE_WMFE_DEQUE_REQ copy_to_user frameconfig failed\n");
					Ret = -EFAULT;
				}
				if (copy_to_user(
					(void *)Param, &owe_WmfeReq,
					sizeof(struct OWE_WMFERequest)) != 0) {
					LOG_ERR(
						"OWE_WMFE_DEQUE_REQ copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR(
					"OWE_CMD_WMFE_DEQUE_REQ copy_from_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}
	default:
		{
			LOG_ERR("Unknown Cmd(%d)", Cmd);
			LOG_ERR(
				"Fail, Cmd(%d), Dir(%d), Type(%d), Nr(%d),Size(%d)\n",
				Cmd, _IOC_DIR(Cmd),
				_IOC_TYPE(Cmd), _IOC_NR(Cmd), _IOC_SIZE(Cmd));
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
			pUserInfo->Pid, current->comm,
			current->pid, current->tgid);
	}
	/*  */
	return Ret;
}

#ifdef CONFIG_COMPAT

/******************************************************************************
 *
 ******************************************************************************/
static int compat_get_OWE_read_register_data(
	struct compat_OWE_REG_IO_STRUCT __user *data32,
	struct OWE_REG_IO_STRUCT __user *data)
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

static int compat_put_OWE_read_register_data(
	struct compat_OWE_REG_IO_STRUCT __user *data32,
	struct OWE_REG_IO_STRUCT __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->pData); */
	/* err |= put_user(uptr, &data32->pData); */
	err |= get_user(count, &data->Count);
	err |= put_user(count, &data32->Count);
	return err;
}

static int compat_get_OWE_occ_enque_req_data(
	struct compat_OWE_OCCRequest __user *data32,
	struct OWE_OCCRequest __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pOweConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pOweConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_OWE_occ_enque_req_data(
	struct compat_OWE_OCCRequest __user *data32,
	struct OWE_OCCRequest __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pOweConfig); */
	/* err |= put_user(uptr, &data32->m_pOweConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}


static int compat_get_OWE_occ_deque_req_data(
	struct compat_OWE_OCCRequest __user *data32,
	struct OWE_OCCRequest __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pOweConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pOweConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_OWE_occ_deque_req_data(
	struct compat_OWE_OCCRequest __user *data32,
	struct OWE_OCCRequest __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pOweConfig); */
	/* err |= put_user(uptr, &data32->m_pOweConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}

static int compat_get_OWE_wmfe_enque_req_data(
	struct compat_OWE_WMFERequest __user *data32,
	struct OWE_WMFERequest __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pWmfeConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pWmfeConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_OWE_wmfe_enque_req_data(
	struct compat_OWE_WMFERequest __user *data32,
	struct OWE_WMFERequest __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pOweConfig); */
	/* err |= put_user(uptr, &data32->m_pOweConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}


static int compat_get_OWE_wmfe_deque_req_data(
	struct compat_OWE_WMFERequest __user *data32,
	struct OWE_WMFERequest __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pWmfeConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pWmfeConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_OWE_wmfe_deque_req_data(
	struct compat_OWE_WMFERequest __user *data32,
	struct OWE_WMFERequest __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pOweConfig); */
	/* err |= put_user(uptr, &data32->m_pOweConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}

static long OWE_ioctl_compat(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	long ret;


	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		LOG_ERR("no f_op !!!\n");
		return -ENOTTY;
	}
	switch (cmd) {
	case COMPAT_OWE_READ_REGISTER:
		{
			struct compat_OWE_REG_IO_STRUCT __user *data32;
			struct OWE_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_OWE_read_register_data(data32, data);
			if (err) {
				LOG_INF(
					"compat_get_OWE_read_register_data error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, OWE_READ_REGISTER,
						       (unsigned long)data);
			err = compat_put_OWE_read_register_data(data32, data);
			if (err) {
				LOG_INF(
					"compat_put_OWE_read_register_data error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_OWE_WRITE_REGISTER:
		{
			struct compat_OWE_REG_IO_STRUCT __user *data32;
			struct OWE_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_OWE_read_register_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_OWE_WRITE_REGISTER error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, OWE_WRITE_REGISTER,
						       (unsigned long)data);
			return ret;
		}
	case COMPAT_OWE_OCC_ENQUE_REQ:
		{
			struct compat_OWE_OCCRequest __user *data32;
			struct OWE_OCCRequest __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_OWE_occ_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_OWE_OCC_ENQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, OWE_OCC_ENQUE_REQ,
						       (unsigned long)data);
			err = compat_put_OWE_occ_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_OWE_OCC_ENQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_OWE_OCC_DEQUE_REQ:
		{
			struct compat_OWE_OCCRequest __user *data32;
			struct OWE_OCCRequest __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_OWE_occ_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_OWE_OCC_DEQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, OWE_OCC_DEQUE_REQ,
						       (unsigned long)data);
			err = compat_put_OWE_occ_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_OWE_OCC_DEQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}

	case COMPAT_OWE_WMFE_ENQUE_REQ:
		{
			struct compat_OWE_WMFERequest __user *data32;
			struct OWE_WMFERequest __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_OWE_wmfe_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_OWE_WMFE_ENQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, OWE_WMFE_ENQUE_REQ,
						       (unsigned long)data);
			err = compat_put_OWE_wmfe_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_OWE_WMFE_ENQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_OWE_WMFE_DEQUE_REQ:
		{
			struct compat_OWE_WMFERequest __user *data32;
			struct OWE_WMFERequest __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_OWE_wmfe_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_OWE_WMFE_DEQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, OWE_WMFE_DEQUE_REQ,
						       (unsigned long)data);
			err = compat_put_OWE_wmfe_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_OWE_WMFE_DEQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}

	case OWE_WAIT_IRQ:
	case OWE_CLEAR_IRQ:	/* structure (no pointer) */
	case OWE_RESET:
	case OWE_DUMP_REG:
	case OWE_DUMP_ISR_LOG:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
		/* return OWE_ioctl(filep, cmd, arg); */
	}
}

#endif

/******************************************************************************
 *
 ******************************************************************************/
static signed int OWE_open(struct inode *pInode, struct file *pFile)
{
	signed int Ret = 0;
	unsigned int i, j;
	/*int q = 0, p = 0;*/
	struct OWE_USER_INFO_STRUCT *pUserInfo;
	unsigned long flags;

	LOG_DBG("- E. UserCount: %d.", OWEInfo.UserCount);


	/*  */
	spin_lock(&(OWEInfo.SpinLockOWERef));

	pFile->private_data = NULL;
	pFile->private_data =
		kmalloc(sizeof(struct OWE_USER_INFO_STRUCT), GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		LOG_DBG(
			"ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)",
			current->comm,
			current->pid,
			current->tgid);
		Ret = -ENOMEM;
	} else {
		pUserInfo = (struct OWE_USER_INFO_STRUCT *) pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}
	/*  */
	if (OWEInfo.UserCount > 0) {
		OWEInfo.UserCount++;
		spin_unlock(&(OWEInfo.SpinLockOWERef));
		LOG_DBG(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			OWEInfo.UserCount, current->comm,
			current->pid, current->tgid);
		goto EXIT;
	} else {
		OWEInfo.UserCount++;
		spin_unlock(&(OWEInfo.SpinLockOWERef));
		LOG_DBG(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), first user",
			OWEInfo.UserCount, current->comm,
			current->pid, current->tgid);
	}

	/* do wait queue head init when re-enter in camera */
	/*  */
	for (i = 0; i < _SUPPORT_MAX_OWE_REQUEST_RING_SIZE_; i++) {
		g_OCC_RequestRing.OCCReq_Struct[i].processID = 0x0;
		g_OCC_RequestRing.OCCReq_Struct[i].callerID = 0x0;
		g_OCC_RequestRing.OCCReq_Struct[i].enqueReqNum = 0x0;
		/* g_OCC_RequestRing.OCCReq_Struct[i].enqueIdx = 0x0; */
		g_OCC_RequestRing.OCCReq_Struct[i].RequestState =
						OWE_REQUEST_STATE_EMPTY;
		g_OCC_RequestRing.OCCReq_Struct[i].FrameWRIdx = 0x0;
		g_OCC_RequestRing.OCCReq_Struct[i].FrameRDIdx = 0x0;
		/* WMFE */
		g_WMFE_ReqRing.WMFEReq_Struct[i].processID = 0x0;
		g_WMFE_ReqRing.WMFEReq_Struct[i].callerID = 0x0;
		g_WMFE_ReqRing.WMFEReq_Struct[i].enqueReqNum = 0x0;
		/* g_WMFE_ReqRing.WMFEReq_Struct[i].enqueIdx = 0x0; */
		g_WMFE_ReqRing.WMFEReq_Struct[i].RequestState =
						OWE_REQUEST_STATE_EMPTY;
		g_WMFE_ReqRing.WMFEReq_Struct[i].FrameWRIdx = 0x0;
		g_WMFE_ReqRing.WMFEReq_Struct[i].FrameRDIdx = 0x0;
		for (j = 0; j < _SUPPORT_MAX_OWE_FRAME_REQUEST_; j++) {
			g_OCC_RequestRing.OCCReq_Struct[i].OccFrameStatus[j] =
			    OWE_FRAME_STATUS_EMPTY;
			g_WMFE_ReqRing.WMFEReq_Struct[i].WmfeFrameStatus[j] =
			    OWE_FRAME_STATUS_EMPTY;
		}

	}
	g_OCC_RequestRing.WriteIdx = 0x0;
	g_OCC_RequestRing.ReadIdx = 0x0;
	g_OCC_RequestRing.HWProcessIdx = 0x0;
	g_WMFE_ReqRing.WriteIdx = 0x0;
	g_WMFE_ReqRing.ReadIdx = 0x0;
	g_WMFE_ReqRing.HWProcessIdx = 0x0;

	/* Enable clock */
	OWE_EnableClock(MTRUE);
	g_SuspendCnt = 0;
	LOG_INF("OWE open g_u4EnableClockCount: %d", g_u4EnableClockCount);
	/*  */

	spin_lock_irqsave(
		&(OWEInfo.SpinLockIrq[OWE_IRQ_TYPE_INT_OWE_ST]), flags);
	for (i = 0; i < OWE_IRQ_TYPE_AMOUNT; i++)
		OWEInfo.IrqInfo.Status[i] = 0;
	spin_unlock_irqrestore(
		&(OWEInfo.SpinLockIrq[OWE_IRQ_TYPE_INT_OWE_ST]), flags);

	for (i = 0; i < _SUPPORT_MAX_OWE_FRAME_REQUEST_; i++)
		OWEInfo.ProcessID[i] = 0;

	OWEInfo.WriteReqIdx = 0;
	OWEInfo.ReadReqIdx = 0;
	OWEInfo.IrqInfo.OccIrqCnt = 0;
	OWEInfo.IrqInfo.WmfeIrqCnt = 0;

#ifdef KERNEL_LOG
    /* In EP, Add OWE_DBG_WRITE_REG for debug. Should remove it after EP */
	OWEInfo.DebugMask = (OWE_DBG_INT | OWE_DBG_DBGLOG | OWE_DBG_WRITE_REG);
#endif
	/*  */
	register_requests(&wmfe_reqs, sizeof(struct OWE_WMFEConfig));
	set_engine_ops(&wmfe_reqs, &wmfe_ops);

	register_requests(&occ_reqs, sizeof(struct OWE_OCCConfig));
	set_engine_ops(&occ_reqs, &occ_ops);

EXIT:




	LOG_DBG("- X. Ret: %d. UserCount: %d.", Ret, OWEInfo.UserCount);
	return Ret;

}

/******************************************************************************
 *
 ******************************************************************************/
static signed int OWE_release(struct inode *pInode, struct file *pFile)
{
	struct OWE_USER_INFO_STRUCT *pUserInfo;
	/*unsigned int Reg;*/

	LOG_DBG("- E. UserCount: %d.", OWEInfo.UserCount);

	/*  */
	if (pFile->private_data != NULL) {
		pUserInfo = (struct OWE_USER_INFO_STRUCT *) pFile->private_data;
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	/*  */
	spin_lock(&(OWEInfo.SpinLockOWERef));
	OWEInfo.UserCount--;

	if (OWEInfo.UserCount > 0) {
		spin_unlock(&(OWEInfo.SpinLockOWERef));
		LOG_DBG(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			OWEInfo.UserCount, current->comm,
			current->pid, current->tgid);
		goto EXIT;
	} else
		spin_unlock(&(OWEInfo.SpinLockOWERef));
	/*  */
	LOG_DBG(
		"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), last user",
		OWEInfo.UserCount, current->comm,
		current->pid, current->tgid);


	/* Disable clock. */
	OWE_EnableClock(MFALSE);
	LOG_INF("OWE release g_u4EnableClockCount: %d", g_u4EnableClockCount);

	/*  */
	unregister_requests(&wmfe_reqs);
	unregister_requests(&occ_reqs);

EXIT:

	LOG_DBG("- X. UserCount: %d.", OWEInfo.UserCount);
	return 0;
}


/******************************************************************************
 *
 ******************************************************************************/
static signed int OWE_mmap(struct file *pFile, struct vm_area_struct *pVma)
{
	unsigned long length = 0;
	unsigned int pfn = 0x0;

	length = pVma->vm_end - pVma->vm_start;
	/*  */
	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
	pfn = pVma->vm_pgoff << PAGE_SHIFT;


	LOG_INF("mmap: pVma->vm_pgoff(0x%lx)", pVma->vm_pgoff);
	LOG_INF("mmap: pfn(0x%x),phy(0x%lx)", pfn,
		pVma->vm_pgoff << PAGE_SHIFT);
	LOG_INF("pVmapVma->vm_start(0x%lx)", pVma->vm_start);
	LOG_INF("pVma->vm_end(0x%lx),length(0x%lx)", pVma->vm_end, length);

	switch (pfn) {
	case OWE_BASE_HW:
		if (length > OWE_REG_RANGE) {
			LOG_ERR(
				"mmap range error :module:0x%x length(0x%lx),OWE_REG_RANGE(0x%x)!",
				pfn, length, OWE_REG_RANGE);
			return -EAGAIN;
		}
		break;
	default:
		LOG_ERR("Illegal starting HW addr for mmap!");
		return -EAGAIN;
	}
	if (remap_pfn_range(
		pVma, pVma->vm_start, pVma->vm_pgoff,
		pVma->vm_end - pVma->vm_start,
		pVma->vm_page_prot)) {
		return -EAGAIN;
	}
	/*  */
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/

static dev_t OWEDevNo;
static struct cdev *pOWECharDrv;
static struct class *pOWEClass;

static const struct file_operations OWEFileOper = {
	.owner = THIS_MODULE,
	.open = OWE_open,
	.release = OWE_release,
	/* .flush   = mt_OWE_flush, */
	.mmap = OWE_mmap,
	.unlocked_ioctl = OWE_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = OWE_ioctl_compat,
#endif
};

/******************************************************************************
 *
 ******************************************************************************/
static inline void OWE_UnregCharDev(void)
{
	LOG_DBG("- E.");
	/*  */
	/* Release char driver */
	if (pOWECharDrv != NULL) {
		cdev_del(pOWECharDrv);
		pOWECharDrv = NULL;
	}
	/*  */
	unregister_chrdev_region(OWEDevNo, 1);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline signed int OWE_RegCharDev(void)
{
	signed int Ret = 0;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = alloc_chrdev_region(&OWEDevNo, 0, 1, OWE_DEV_NAME);
	if (Ret < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d", Ret);
		return Ret;
	}
	/* Allocate driver */
	pOWECharDrv = cdev_alloc();
	if (pOWECharDrv == NULL) {
		LOG_ERR("cdev_alloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pOWECharDrv, &OWEFileOper);
	/*  */
	pOWECharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pOWECharDrv, OWEDevNo, 1);
	if (Ret < 0) {
		LOG_ERR("Attatch file operation failed, %d", Ret);
		goto EXIT;
	}
	/*  */
EXIT:
	if (Ret < 0)
		OWE_UnregCharDev();

	/*  */

	LOG_DBG("- X.");
	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int OWE_probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	/*struct resource *pRes = NULL;*/
	signed int i = 0;
	unsigned char n;
	unsigned int irq_info[3];/* Record interrupts info from device tree */
	struct device *dev = NULL;
	struct OWE_device *_owedev = NULL;

#ifdef CONFIG_OF
	struct OWE_device *OWE_dev;
#endif

	LOG_INF("- E. OWE driver probe.");

	/* Check platform_device parameters */
#ifdef CONFIG_OF

	if (pDev == NULL) {
		dev_dbg(&pDev->dev, "pDev is NULL");
		return -ENXIO;
	}

	nr_OWE_devs += 1;
	_owedev = krealloc(OWE_devs,
		sizeof(struct OWE_device) * nr_OWE_devs, GFP_KERNEL);
	if (!_owedev) {
		dev_dbg(&pDev->dev, "Unable to allocate OWE_devs\n");
		return -ENOMEM;
	}
	OWE_devs = _owedev;

	OWE_dev = &(OWE_devs[nr_OWE_devs - 1]);
	OWE_dev->dev = &pDev->dev;

	/* iomap registers */
	OWE_dev->regs = of_iomap(pDev->dev.of_node, 0);
	/* gISPSYS_Reg[nr_OWE_devs - 1] = OWE_dev->regs; */

	if (!OWE_dev->regs) {
		dev_dbg(&pDev->dev,
			"Unable to ioremap registers, of_iomap fail, nr_OWE_devs=%d, devnode(%s).\n",
			nr_OWE_devs, pDev->dev.of_node->name);
		return -ENOMEM;
	}

	LOG_INF("nr_OWE_devs=%d, devnode(%s), map_addr=0x%lx\n", nr_OWE_devs,
		pDev->dev.of_node->name, (unsigned long)OWE_dev->regs);

	/* get IRQ ID and request IRQ */
	OWE_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);

	if (OWE_dev->irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array(
			pDev->dev.of_node, "interrupts",
			irq_info, ARRAY_SIZE(irq_info))) {
			dev_dbg(&pDev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		for (i = 0; i < OWE_IRQ_TYPE_AMOUNT; i++) {
			if (strcmp(pDev->dev.of_node->name,
				OWE_IRQ_CB_TBL[i].device_name) == 0) {
				Ret = request_irq(OWE_dev->irq,
				    (irq_handler_t) OWE_IRQ_CB_TBL[i].isr_fp,
				    irq_info[2],
				    (const char *)OWE_IRQ_CB_TBL[i].device_name,
				    NULL);

				if (Ret) {
					dev_dbg(&pDev->dev,
						"Unable to request IRQ, request_irq fail, nr_OWE_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
						nr_OWE_devs,
						pDev->dev.of_node->name,
						OWE_dev->irq,
						OWE_IRQ_CB_TBL[i].device_name);
					return Ret;
				}

				LOG_INF(
					"nr_OWE_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
					nr_OWE_devs, pDev->dev.of_node->name,
					OWE_dev->irq,
					OWE_IRQ_CB_TBL[i].device_name);
				break;
			}
		}

		if (i >= OWE_IRQ_TYPE_AMOUNT) {
			LOG_INF(
				"No corresponding ISR!!: nr_OWE_devs=%d, devnode(%s), irq=%d\n",
				nr_OWE_devs, pDev->dev.of_node->name,
				OWE_dev->irq);
		}


	} else {
		LOG_INF("No IRQ!!: nr_OWE_devs=%d, devnode(%s), irq=%d\n",
			nr_OWE_devs,
			pDev->dev.of_node->name, OWE_dev->irq);
	}


#endif

	/* Only register char driver in the 1st time */
	if (nr_OWE_devs == 1) {

		/* Register char driver */
		Ret = OWE_RegCharDev();
		if (Ret) {
			dev_dbg(&pDev->dev, "register char failed");
			return Ret;
		}
#ifndef __OWE_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
		    /*CCF: Grab clock pointer (struct clk*) */
		owe_clk.CG_IMGSYS_OWE = devm_clk_get(&pDev->dev,
				"OWE_CLK_IMG_OWE");

		if (IS_ERR(owe_clk.CG_IMGSYS_OWE)) {
			LOG_ERR("cannot get CG_IMGSYS_OWE clock\n");
			return PTR_ERR(owe_clk.CG_IMGSYS_OWE);
		}

#endif	/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */
#endif

		/* Create class register */
		pOWEClass = class_create(THIS_MODULE, "OWEdrv");
		if (IS_ERR(pOWEClass)) {
			Ret = PTR_ERR(pOWEClass);
			LOG_ERR("Unable to create class, err = %d", Ret);
			goto EXIT;
		}

		dev = device_create(
			pOWEClass, NULL, OWEDevNo, NULL, OWE_DEV_NAME);

		if (IS_ERR(dev)) {
			Ret = PTR_ERR(dev);
			dev_dbg(&pDev->dev, "Failed to create device: /dev/%s, err = %d",
				OWE_DEV_NAME, Ret);
			goto EXIT;
		}

		/* Init spinlocks */
		spin_lock_init(&(OWEInfo.SpinLockOWERef));
		spin_lock_init(&(OWEInfo.SpinLockOWE));
		for (n = 0; n < OWE_IRQ_TYPE_AMOUNT; n++)
			spin_lock_init(&(OWEInfo.SpinLockIrq[n]));

		/*  */
		init_waitqueue_head(&OWEInfo.WaitQueueHead);
		INIT_WORK(&OWEInfo.ScheduleOccWork, OWE_ScheduleOccWork);
		INIT_WORK(&OWEInfo.ScheduleWmfeWork, OWE_ScheduleWmfeWork);
		OWEInfo.wkqueue = create_singlethread_workqueue("WMFE-CMDQ-WQ");
		if (!OWEInfo.wkqueue)
			LOG_ERR("NULL WMFE-CMDQ-WQ\n");

		wakeup_source_init(&OWE_wake_lock, "owe_lock_wakelock");

		for (i = 0; i < OWE_IRQ_TYPE_AMOUNT; i++)
			tasklet_init(
				OWE_tasklet[i].pOWE_tkt,
				OWE_tasklet[i].tkt_cb, 0);

		/* Init OWEInfo */
		spin_lock(&(OWEInfo.SpinLockOWERef));
		OWEInfo.UserCount = 0;
		spin_unlock(&(OWEInfo.SpinLockOWERef));
		/*  */
		OWEInfo.IrqInfo.Mask[OWE_IRQ_TYPE_INT_OWE_ST] = INT_ST_MASK_OWE;

		seqlock_init(&(wmfe_reqs.seqlock));
		seqlock_init(&(occ_reqs.seqlock));
	}

EXIT:
	if (Ret < 0)
		OWE_UnregCharDev();


	LOG_INF("- X. OWE driver probe.");

	return Ret;
}

/******************************************************************************
 * Called when the device is being detached from the driver
 ******************************************************************************/
static signed int OWE_remove(struct platform_device *pDev)
{
	/*struct resource *pRes;*/
	signed int IrqNum;
	int i;
	/*  */
	LOG_DBG("- E.");

	destroy_workqueue(OWEInfo.wkqueue);
	OWEInfo.wkqueue = NULL;

	/* unregister char driver. */
	OWE_UnregCharDev();

	/* Release IRQ */
	disable_irq(OWEInfo.IrqNum);
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);

	/* kill tasklet */
	for (i = 0; i < OWE_IRQ_TYPE_AMOUNT; i++)
		tasklet_kill(OWE_tasklet[i].pOWE_tkt);
#if 0
	/* free all registered irq(child nodes) */
	OWE_UnRegister_AllregIrq();
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
			    ((REG_IRQ_NODE *) (
			    (char *)__mptr -
			    offsetof(REG_IRQ_NODE, list)));
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
	device_destroy(pOWEClass, OWEDevNo);
	/*  */
	class_destroy(pOWEClass);
	pOWEClass = NULL;
	/*  */
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int bPass1_On_In_Resume_TG1;

static signed int OWE_suspend(struct platform_device *pDev, pm_message_t Mesg)
{
	/*signed int ret = 0;*/

	if (g_u4EnableClockCount > 0) {
		OWE_EnableClock(MFALSE);
		g_SuspendCnt++;
	}
	bPass1_On_In_Resume_TG1 = 0;
	LOG_INF("%s:g_u4EnableClockCount(%d) g_SuspendCnt(%d).\n", __func__,
				g_u4EnableClockCount, g_SuspendCnt);


	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int OWE_resume(struct platform_device *pDev)
{
	LOG_DBG("bPass1_On_In_Resume_TG1(%d).\n", bPass1_On_In_Resume_TG1);
	if (g_SuspendCnt > 0) {
		OWE_EnableClock(MTRUE);
		g_SuspendCnt--;
	}
	LOG_INF("%s:g_u4EnableClockCount(%d) g_SuspendCnt(%d).\n", __func__,
				g_u4EnableClockCount, g_SuspendCnt);

	return 0;
}


/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int OWE_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	LOG_DBG("calling %s()\n", __func__);

	return OWE_suspend(pdev, PMSG_SUSPEND);
}

int OWE_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	LOG_DBG("calling %s()\n", __func__);

	return OWE_resume(pdev);
}
#ifndef CONFIG_OF
/* extern void mt_irq_set_sens(unsigned int irq, unsigned int sens); */
/* extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity); */
#endif
int OWE_pm_restore_noirq(struct device *device)
{
	pr_debug("calling %s()\n", __func__);
#ifndef CONFIG_OF
	mt_irq_set_sens(OWE_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(OWE_IRQ_BIT_ID, MT_POLARITY_LOW);
#endif
	return 0;

}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define OWE_pm_suspend NULL
#define OWE_pm_resume  NULL
#define OWE_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_OF
/*
 * Note!!! The order and member of .compatible must be the same with that in
 *  "OWE_DEV_NODE_ENUM" in camera_OWE.h
 */
static const struct of_device_id OWE_of_ids[] = {
	{.compatible = "mediatek,owe",},
	{}
};
#endif

const struct dev_pm_ops OWE_pm_ops = {
	.suspend = OWE_pm_suspend,
	.resume = OWE_pm_resume,
	.freeze = OWE_pm_suspend,
	.thaw = OWE_pm_resume,
	.poweroff = OWE_pm_suspend,
	.restore = OWE_pm_resume,
	.restore_noirq = OWE_pm_restore_noirq,
};


/******************************************************************************
 *
 ******************************************************************************/
static struct platform_driver OWEDriver = {
	.probe = OWE_probe,
	.remove = OWE_remove,
	.suspend = OWE_suspend,
	.resume = OWE_resume,
	.driver = {
		   .name = OWE_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = OWE_of_ids,
#endif
#ifdef CONFIG_PM
		   .pm = &OWE_pm_ops,
#endif
	}
};


static int owe_dump_read(struct seq_file *m, void *v)
{
	int i, j;

	if (OWEInfo.UserCount <= 0)
		return 0;

	seq_puts(m, "\n============ owe dump register============\n");
	seq_puts(m, "OCC Config Info\n");

	for (i = 0x2C; i < 0x8C; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(OWE_BASE_HW + i),
			(unsigned int)OWE_RD32(ISP_OWE_BASE + i));
	}
	seq_puts(m, "OCC Debug Info\n");
	for (i = 0x120; i < 0x148; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(OWE_BASE_HW + i),
			(unsigned int)OWE_RD32(ISP_OWE_BASE + i));
	}

	seq_puts(m, "WMFE Config Info\n");
	for (i = 0x230; i < 0x2D8; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(OWE_BASE_HW + i),
			(unsigned int)OWE_RD32(ISP_OWE_BASE + i));
	}
	seq_puts(m, "WMFE Debug Info\n");
	for (i = 0x2F4; i < 0x30C; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(OWE_BASE_HW + i),
			(unsigned int)OWE_RD32(ISP_OWE_BASE + i));
	}

	seq_puts(m, "\n");
	seq_printf(m, "Owe Clock Count:%d\n", g_u4EnableClockCount);

	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(OWE_DMA_DBG_HW),
		   (unsigned int)OWE_RD32(OWE_DMA_DBG_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(OWE_DMA_REQ_STATUS_HW),
		   (unsigned int)OWE_RD32(OWE_DMA_REQ_STATUS_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(OWE_DMA_RDY_STATUS_HW),
		   (unsigned int)OWE_RD32(OWE_DMA_RDY_STATUS_REG));

	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(OWE_DMA_RDY_STATUS_HW),
		   (unsigned int)OWE_RD32(OWE_DMA_RDY_STATUS_REG));


	seq_printf(m, "OCC:HWProcessIdx:%d, WriteIdx:%d, ReadIdx:%d\n",
		   g_OCC_RequestRing.HWProcessIdx, g_OCC_RequestRing.WriteIdx,
		   g_OCC_RequestRing.ReadIdx);

	for (i = 0; i < _SUPPORT_MAX_OWE_REQUEST_RING_SIZE_; i++) {
		seq_printf(m,
			   "OCC:RequestState:%d, processID:0x%08X, callerID:0x%08X, enqueReqNum:%d, FrameWRIdx:%d, FrameRDIdx:%d\n",
			   g_OCC_RequestRing.OCCReq_Struct[i].RequestState,
			   g_OCC_RequestRing.OCCReq_Struct[i].processID,
			   g_OCC_RequestRing.OCCReq_Struct[i].callerID,
			   g_OCC_RequestRing.OCCReq_Struct[i].enqueReqNum,
			   g_OCC_RequestRing.OCCReq_Struct[i].FrameWRIdx,
			   g_OCC_RequestRing.OCCReq_Struct[i].FrameRDIdx);

		for (j = 0; j < _SUPPORT_MAX_OWE_FRAME_REQUEST_;) {
			seq_printf(m,
				   "OCC:FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d\n",
				   j,
				   g_OCC_RequestRing
					.OCCReq_Struct[i]
					.OccFrameStatus[j],
				   j + 1,
				   g_OCC_RequestRing
					.OCCReq_Struct[i]
					.OccFrameStatus[j + 1],
				   j + 2,
				   g_OCC_RequestRing
					.OCCReq_Struct[i]
					.OccFrameStatus[j + 2]);
			j = j + 3;
		}
	}


	seq_printf(m, "WMFE:HWProcessIdx:%d, WriteIdx:%d, ReadIdx:%d\n",
		   g_WMFE_ReqRing.HWProcessIdx,
		   g_WMFE_ReqRing.WriteIdx,
		   g_WMFE_ReqRing.ReadIdx);

	for (i = 0; i < _SUPPORT_MAX_OWE_REQUEST_RING_SIZE_; i++) {
		seq_printf(m,
			   "WMFE:RequestState:%d, processID:0x%08X, callerID:0x%08X, enqueReqNum:%d, FrameWRIdx:%d, FrameRDIdx:%d\n",
			   g_WMFE_ReqRing.WMFEReq_Struct[i].RequestState,
			   g_WMFE_ReqRing.WMFEReq_Struct[i].processID,
			   g_WMFE_ReqRing.WMFEReq_Struct[i].callerID,
			   g_WMFE_ReqRing.WMFEReq_Struct[i].enqueReqNum,
			   g_WMFE_ReqRing.WMFEReq_Struct[i].FrameWRIdx,
			   g_WMFE_ReqRing.WMFEReq_Struct[i].FrameRDIdx);

		for (j = 0; j < _SUPPORT_MAX_OWE_FRAME_REQUEST_;) {
			seq_printf(m,
				   "WMFE:FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d\n",
				   j,
				   g_WMFE_ReqRing
					.WMFEReq_Struct[i].WmfeFrameStatus[j],
				   j + 1,
				   g_WMFE_ReqRing
					.WMFEReq_Struct[i]
					.WmfeFrameStatus[j + 1],
				   j + 2,
				   g_WMFE_ReqRing.WMFEReq_Struct[i]
						.WmfeFrameStatus[j + 2]);
			j = j + 3;
		}
	}

	seq_puts(m, "\n============ owe dump debug ============\n");

	return 0;
}


static int proc_owe_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, owe_dump_read, NULL);
}

static const struct file_operations owe_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_owe_dump_open,
	.read = seq_read,
};


static int owe_reg_read(struct seq_file *m, void *v)
{
	unsigned int i;

	if (OWEInfo.UserCount <= 0)
		return 0;

	seq_puts(m, "======== read owe register ========\n");

	for (i = 0x1C; i <= 0x308; i = i + 4) {
		seq_printf(m, "[0x%08X 0x%08X]\n",
			(unsigned int)(OWE_BASE_HW + i),
			(unsigned int)OWE_RD32(ISP_OWE_BASE + i));
	}

	seq_printf(m, "[0x%08X 0x%08X]\n",
		(unsigned int)(OWE_BASE_HW + 0x7F4),
		(unsigned int)OWE_RD32(OWE_DMA_DBG_REG));
	seq_printf(m, "[0x%08X 0x%08X]\n",
		(unsigned int)(OWE_BASE_HW + 0x7F8),
		(unsigned int)OWE_RD32(OWE_DMA_REQ_STATUS_REG));
	seq_printf(m, "[0x%08X 0x%08X]\n",
		(unsigned int)(OWE_BASE_HW + 0x7FC),
		(unsigned int)OWE_RD32(OWE_DMA_RDY_STATUS_REG));

	return 0;
}

/*static int owe_reg_write(struct file *file,*/
/*	const char __user *buffer, size_t count, loff_t *data)*/

static ssize_t owe_reg_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[128];
	int len = 0;
	/*char *pEnd;*/
	char addrSzBuf[24];
	char valSzBuf[24];
	char *pszTmp;
	int addr = 0, val = 0;
	long int tempval;

	if (OWEInfo.UserCount <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%23s %23s", addrSzBuf, valSzBuf) == 2) {
		pszTmp = strstr(addrSzBuf, "0x");
		if (pszTmp == NULL) {
			if (kstrtol(addrSzBuf, 10,
			    (long int *)&tempval) != 0)
				LOG_ERR(
				"scan decimal addr is wrong !!:%s",
				addrSzBuf);
		} else {
			if (strlen(addrSzBuf) > 2) {
				if (sscanf(addrSzBuf + 2, "%x", &addr) != 1)
					LOG_ERR(
					"scan hexadecimal addr is wrong !!:%s",
					addrSzBuf);
			} else {
				LOG_INF("OWE Write Addr Error!!:%s", addrSzBuf);
			}
		}

		pszTmp = strstr(valSzBuf, "0x");
		if (pszTmp == NULL) {
			if (kstrtol(valSzBuf, 10, (long int *)&tempval) != 0)
				LOG_ERR("scan decimal value is wrong !!:%s",
					valSzBuf);
		} else {
			if (strlen(valSzBuf) > 2) {
				if (sscanf(valSzBuf + 2, "%x", &val) != 1)
					LOG_ERR(
					"scan hexadecimal value is wrong !!:%s",
					valSzBuf);
			} else {
				LOG_INF("OWE Write Value Error!!:%s\n",
					valSzBuf);
			}
		}

		if ((addr >= OWE_BASE_HW) && (addr <= OWE_DMA_RDY_STATUS_HW)
			&& ((addr & 0x3) == 0)) {
			LOG_INF("Write Request - addr:0x%x, value:0x%x\n",
				addr, val);
			OWE_WR32((ISP_OWE_BASE + (addr - OWE_BASE_HW)), val);
		} else {
			LOG_INF(
				"Write-Address Range exceeds the size of hw owe!! addr:0x%x, value:0x%x\n",
			     addr, val);
		}

	} else if (sscanf(desc, "%23s", addrSzBuf) == 1) {
		pszTmp = strstr(addrSzBuf, "0x");
		if (pszTmp == NULL) {
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
				LOG_INF("OWE Read Addr Error!!:%s", addrSzBuf);
			}
		}

		if ((addr >= OWE_BASE_HW) && (addr <= OWE_DMA_RDY_STATUS_HW)
			&& ((addr & 0x3) == 0)) {
			val = OWE_RD32((ISP_OWE_BASE + (addr - OWE_BASE_HW)));
			LOG_INF("Read Request - addr:0x%x,value:0x%x\n",
				addr, val);
		} else {
			LOG_INF(
				"Read-Address Range exceeds the size of hw owe!! addr:0x%x, value:0x%x\n",
			     addr, val);
		}

	}


	return count;
}

static int proc_owe_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, owe_reg_read, NULL);
}

static const struct file_operations owe_reg_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_owe_reg_open,
	.read = seq_read,
	.write = owe_reg_write,
};


/******************************************************************************
 *
 ******************************************************************************/

int32_t OWE_ClockOnCallback(uint64_t engineFlag)
{
	/* LOG_DBG("OWE_ClockOnCallback"); */
	/* LOG_DBG("+CmdqEn:%d", g_u4EnableClockCount); */
	/* OWE_EnableClock(MTRUE); */

	return 0;
}

int32_t OWE_DumpCallback(uint64_t engineFlag, int level)
{
	LOG_DBG("OWE_DumpCallback");

	OWE_DumpReg();

	return 0;
}

int32_t OWE_ResetCallback(uint64_t engineFlag)
{
	LOG_DBG("OWE_ResetCallback");
	OWE_Reset();

	return 0;
}

int32_t OWE_ClockOffCallback(uint64_t engineFlag)
{
	/* LOG_DBG("OWE_ClockOffCallback"); */
	/* OWE_EnableClock(MFALSE); */
	/* LOG_DBG("-CmdqEn:%d", g_u4EnableClockCount); */
	return 0;
}


static signed int __init OWE_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
	/* FIX-ME: linux-3.10 procfs API changed */
	/* use proc_create */
	struct proc_dir_entry *proc_entry;
	struct proc_dir_entry *isp_owe_dir;


	int i;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = platform_driver_register(&OWEDriver);
	if (Ret < 0) {
		LOG_ERR("platform_driver_register fail");
		return Ret;
	}

#if 0
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,OWE");
	if (!node) {
		LOG_ERR("find mediatek,OWE node failed!!!\n");
		return -ENODEV;
	}
	ISP_OWE_BASE = of_iomap(node, 0);
	if (!ISP_OWE_BASE) {
		LOG_ERR("unable to map ISP_OWE_BASE registers!!!\n");
		return -ENODEV;
	}
	LOG_DBG("ISP_OWE_BASE: %lx\n", ISP_OWE_BASE);
#endif

	isp_owe_dir = proc_mkdir("owe", NULL);
	if (!isp_owe_dir) {
		LOG_ERR("[%s]: fail to mkdir /proc/owe\n", __func__);
		return 0;
	}

	/* proc_entry = proc_create("pll_test", */
	/* S_IRUGO | S_IWUSR, isp_owe_dir, &pll_test_proc_fops); */

	proc_entry = proc_create("owe_dump",
		0444, isp_owe_dir, &owe_dump_proc_fops);

	proc_entry = proc_create("owe_reg", 0644,
		isp_owe_dir, &owe_reg_proc_fops);


	/* isr log */
	if (PAGE_SIZE < ((OWE_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN * ((
	    DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM)) {
		i = 0;
		while (i <
		       ((OWE_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
			 ((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM)) {
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
		for (j = 0; j < OWE_IRQ_TYPE_AMOUNT; j++) {
			gSvLog[j]._str[i][_LOG_DBG] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp + */
			/*	(NORMAL_STR_LEN*DBG_PAGE)); */
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * DBG_PAGE));
			gSvLog[j]._str[i][_LOG_INF] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp + */
			/*	(NORMAL_STR_LEN*INF_PAGE)); */
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * INF_PAGE));
			gSvLog[j]._str[i][_LOG_ERR] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp + */
			/*	(NORMAL_STR_LEN*ERR_PAGE)); */
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * ERR_PAGE));
		}
		/* tmp = (void*) ((unsigned int)tmp + NORMAL_STR_LEN); */
		/* log buffer ,in case of overflow */
		tmp = (void *)((char *)tmp + NORMAL_STR_LEN);
		/* log buffer ,in case of overflow */
	}

	/* Cmdq */
	/* Register OWE callback */
	LOG_DBG("register owe callback for CMDQ");
	cmdqCoreRegisterCB(CMDQ_GROUP_OWE,
			   OWE_ClockOnCallback,
			   OWE_DumpCallback, OWE_ResetCallback,
			   OWE_ClockOffCallback);
	LOG_DBG("- X. Ret: %d.", Ret);
	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static void __exit OWE_Exit(void)
{
	/*int i;*/

	LOG_DBG("- E.");
	/*  */
	platform_driver_unregister(&OWEDriver);
	/*  */
	/* Cmdq */
	/* Unregister OWE callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_OWE, NULL, NULL, NULL, NULL);

	kfree(pLog_kmalloc);

	/*  */
}


/******************************************************************************
 *
 ******************************************************************************/
static void OWE_ScheduleOccWork(struct work_struct *data)
{
	if (OWE_DBG_DBGLOG & OWEInfo.DebugMask)
		LOG_DBG("- E.");
	request_handler(&occ_reqs, &(OWEInfo.SpinLockIrq[
						OWE_IRQ_TYPE_INT_OWE_ST]));
	if (!request_running(&occ_reqs))
		LOG_DBG("[%s]no more requests", __func__);
}

/******************************************************************************
 *
 ******************************************************************************/
static void OWE_ScheduleWmfeWork(struct work_struct *data)
{
	if (OWE_DBG_DBGLOG & OWEInfo.DebugMask)
		LOG_DBG("- E.");
	request_handler(&wmfe_reqs,
		&(OWEInfo.SpinLockIrq[OWE_IRQ_TYPE_INT_OWE_ST]));
	if (!request_running(&wmfe_reqs))
		LOG_DBG("[%s]no more requests", __func__);
}


static irqreturn_t ISP_Irq_OWE(signed int Irq, void *DeviceId)
{
	unsigned int OweIntStatus;
	unsigned int OccStatus;
	unsigned int WmfeStatus;
	bool bResulst = MFALSE;
	pid_t ProcessID;

	OweIntStatus = OWE_RD32(OWE_INT_STATUS_REG);	/* OWE_INT_STATUS */
	OccStatus = OWE_RD32(OWE_OCC_INT_STATUS_REG);	/* OCC Status */
	WmfeStatus = OWE_RD32(OWE_WMFE_INT_STATUS_REG);	/* WMFE Status */
	spin_lock(&(OWEInfo.SpinLockIrq[OWE_IRQ_TYPE_INT_OWE_ST]));
	if (OCC_INT_ST == (OCC_INT_ST & OccStatus)) {
#ifdef __OWE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("owe_occ_irq");
#endif

		if (update_request(&occ_reqs, &ProcessID) == 0)
			bResulst = MTRUE;
		/* Config the Next frame */
		if (bResulst == MTRUE) {
			#if REQUEST_REGULATION == REQUEST_BASE_REGULATION
			/* schedule_work(&&OWEInfo.ScheduleOccWork); */
			queue_work(OWEInfo.wkqueue, &OWEInfo.ScheduleOccWork);
			#endif

			OWEInfo.IrqInfo
			    .Status[OWE_IRQ_TYPE_INT_OWE_ST] |= OWE_OCC_INT_ST;
			OWEInfo.IrqInfo
			    .ProcessID[OWE_PROCESS_ID_OCC] = ProcessID;
			OWEInfo.IrqInfo.OccIrqCnt++;
			OWEInfo.ProcessID[OWEInfo.WriteReqIdx] = ProcessID;
			OWEInfo.WriteReqIdx =
			    (OWEInfo.WriteReqIdx + 1) %
			    _SUPPORT_MAX_OWE_FRAME_REQUEST_;
#ifdef OWE_MULTIPROCESS_TIMEING_ISSUE
			/* check the write value is equal to read value ? */
			/* actually, it doesn't happen!! */
			if (OWEInfo.WriteReqIdx == OWEInfo.ReadReqIdx) {
				IRQ_LOG_KEEPER(
				    OWE_IRQ_TYPE_INT_OWE_ST,
				    m_CurrentPPB,
				    _LOG_ERR,
				    "Err!!, WriteReqIdx:0x%x, ReadReqIdx:0x%x\n",
				    OWEInfo.WriteReqIdx,
				    OWEInfo.ReadReqIdx);
			}
#endif
		}
#ifdef __OWE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
	}

	if (WMFE_INT_ST == (WMFE_INT_ST & WmfeStatus)) {
		/* Update the frame status. */
#ifdef __OWE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("owe_wmfe_irq");
#endif

		if (update_request(&wmfe_reqs, &ProcessID) == 0)
			bResulst = MTRUE;
		if (bResulst == MTRUE) {
			#if REQUEST_REGULATION == REQUEST_BASE_REGULATION
			/* schedule_work(&&OWEInfo.ScheduleWmfeWork); */
			queue_work(OWEInfo.wkqueue, &OWEInfo.ScheduleWmfeWork);
			#endif

			OWEInfo.IrqInfo
				.Status[OWE_IRQ_TYPE_INT_OWE_ST] |=
				OWE_WMFE_INT_ST;
			OWEInfo.IrqInfo
				.ProcessID[OWE_PROCESS_ID_WMFE] = ProcessID;
			OWEInfo.IrqInfo.WmfeIrqCnt++;
			OWEInfo.ProcessID[OWEInfo.WriteReqIdx] = ProcessID;
			OWEInfo.WriteReqIdx =
			    (OWEInfo.WriteReqIdx + 1) %
			    _SUPPORT_MAX_OWE_FRAME_REQUEST_;
#ifdef OWE_MULTIPROCESS_TIMEING_ISSUE
			/* check the write value is equal to read value ? */
			/* actually, it doesn't happen!! */
			if (OWEInfo.WriteReqIdx == OWEInfo.ReadReqIdx) {
				IRQ_LOG_KEEPER(OWE_IRQ_TYPE_INT_OWE_ST,
						m_CurrentPPB, _LOG_ERR,
					       "ISP_OWE Err!!, WriteReqIdx:0x%x, ReadReqIdx:0x%x\n",
					       OWEInfo.WriteReqIdx,
						OWEInfo.ReadReqIdx);
			}
#endif
		}
#ifdef __OWE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
		/* Config the Next frame */
	}
	spin_unlock(&(OWEInfo.SpinLockIrq[OWE_IRQ_TYPE_INT_OWE_ST]));
	if (bResulst == MTRUE)
		wake_up_interruptible(&OWEInfo.WaitQueueHead);

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(OWE_IRQ_TYPE_INT_OWE_ST, m_CurrentPPB, _LOG_INF,
		       "Irq_OWE:%d, reg 0x%x : 0x%x, bResulst:%d, OccHWSta:0x%x, WmfeHWSta:0x%x, OccIrqCnt:0x%x, WmfeIrqCnt:0x%x, WriteReqIdx:0x%x, ReadReqIdx:0x%x\n",
		       Irq, OWE_INT_STATUS_HW, OweIntStatus,
		       bResulst, OccStatus, WmfeStatus,
		       OWEInfo.IrqInfo.OccIrqCnt, OWEInfo.IrqInfo.WmfeIrqCnt,
		       OWEInfo.WriteReqIdx, OWEInfo.ReadReqIdx);

	#if (REQUEST_REGULATION == FRAME_BASE_REGULATION)
	/* schedule_work(&OWEInfo.ScheduleWmfeWork); */
	if (OCC_INT_ST == (OCC_INT_ST & OccStatus))
		queue_work(OWEInfo.wkqueue, &OWEInfo.ScheduleOccWork);

	if (WMFE_INT_ST == (WMFE_INT_ST & WmfeStatus))
		queue_work(OWEInfo.wkqueue, &OWEInfo.ScheduleWmfeWork);
	#endif

	if (OweIntStatus & OWE_INT_ST)
		tasklet_schedule(OWE_tasklet[OWE_IRQ_TYPE_INT_OWE_ST].pOWE_tkt);

	return IRQ_HANDLED;
}

static void ISP_TaskletFunc_OWE(unsigned long data)
{
	IRQ_LOG_PRINTER(OWE_IRQ_TYPE_INT_OWE_ST, m_CurrentPPB, _LOG_DBG);
	IRQ_LOG_PRINTER(OWE_IRQ_TYPE_INT_OWE_ST, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(OWE_IRQ_TYPE_INT_OWE_ST, m_CurrentPPB, _LOG_ERR);

}


/******************************************************************************
 *
 ******************************************************************************/
module_init(OWE_Init);
module_exit(OWE_Exit);
MODULE_DESCRIPTION("Camera OWE driver");
MODULE_AUTHOR("ME8");
MODULE_LICENSE("GPL");
