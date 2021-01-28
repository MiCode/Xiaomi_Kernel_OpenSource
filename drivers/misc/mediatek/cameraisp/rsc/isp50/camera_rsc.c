// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

/******************************************************************************
 * camera_RSC.c - Linux RSC Device Driver
 *
 * DESCRIPTION:
 *     This file provid the other drivers RSC relative functions
 *
 ******************************************************************************/
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/types.h>
/* #include <asm/io.h> */
/* #include <asm/tcm.h> */
#include <linux/proc_fs.h> /* proc file use */
/*  */
#include <linux/slab.h>
#include <linux/spinlock.h>
/* #include <linux/io.h> */
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/sched/clock.h>

/*#include <linux/xlog.h>		 For xlog_printk(). */
/*  */
/*#include <mach/hardware.h>*/
/* #include <mach/mt6593_pll.h> */
#include "camera_rsc.h"
/*#include <mach/irqs.h>*/
/* #include <mach/mt_reg_base.h> */
/* #if defined(CONFIG_MTK_LEGACY) */
/* For clock mgr APIS. enable_clock()/disable_clock(). */
/* #include <mach/mt_clkmgr.h> */
/* #endif */
#ifdef COFNIG_MTK_IOMMU
#include "mtk_iommu.h"
#elif defined(CONFIG_MTK_M4U)
#include <m4u.h>
#endif

#ifdef CONFIG_MTK_SMI_EXT
#include <smi_public.h>
#endif
/* For spm_enable_sodi()/spm_disable_sodi(). */
/* #include <mach/mt_spm_idle.h> */
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

#include <cmdq_core.h>
#include <cmdq_record.h>
#include "../engine_request.h"

#define CMDQ_COMMON
#ifdef CMDQ_COMMON
#define RSC_PMQOS_EN
#endif
#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
#include <linux/pm_qos.h>
#endif

/* Measure the kernel performance
 * #define __RSC_KERNEL_PERFORMANCE_MEASURE__
 */
#ifdef __RSC_KERNEL_PERFORMANCE_MEASURE__
#include <linux/met_drv.h>
#include <linux/mtk_ftrace.h>
#endif
#ifdef PERF_EN
/* Another Performance Measure Usage */
#include <linux/ftrace_event.h>
#include <linux/kallsyms.h>
static unsigned long __read_mostly tracing_mark_write_addr;
#define _kernel_trace_begin(name)                                              \
	{                                                                      \
		tracing_mark_write_addr =                                      \
			kallsyms_lookup_name("tracing_mark_write");            \
		event_trace_printk(tracing_mark_write_addr, "B|%d|%s\n",       \
				   current->tgid, name);                       \
	}
#define _kernel_trace_end()                                                    \
	{                                                                      \
		event_trace_printk(tracing_mark_write_addr, "E\n");            \
	}
/* How to Use */
/* char strName[128]; */
/* sprintf(strName, "TAG_K_WAKEUP (%d)",sof_count[_PASS1]); */
/* _kernel_trace_begin(strName); */
/* _kernel_trace_end(); */
#endif

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/compat.h>
#include <linux/fs.h>
#endif

/*  #include "smi_common.h" */

#include <linux/pm_wakeup.h>
/* CCF */
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#include <linux/clk.h>
struct RSC_CLK_STRUCT {
/* TODO */
#ifndef CONFIG_MTK_SMI_EXT
	struct clk *CG_SCP_SYS_MM0;
	struct clk *CG_SCP_SYS_ISP;
	struct clk *CG_MM_SMI_COMMON;
	struct clk *CG_MM_SMI_COMMON_2X;
	struct clk *CG_MM_SMI_COMMON_GALS_M0_2X;
	struct clk *CG_MM_SMI_COMMON_GALS_M1_2X;
	struct clk *CG_MM_SMI_COMMON_UPSZ0;
	struct clk *CG_MM_SMI_COMMON_UPSZ1;
	struct clk *CG_MM_SMI_COMMON_FIFO0;
	struct clk *CG_MM_SMI_COMMON_FIFO1;
	struct clk *CG_MM_LARB5;
	struct clk *CG_IMGSYS_LARB;
#endif
	struct clk *CG_IPESYS_RSC;
};
struct RSC_CLK_STRUCT rsc_clk;
#endif
/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */

#ifndef MTRUE
#define MTRUE 1
#endif
#ifndef MFALSE
#define MFALSE 0
#endif

#define RSC_DEV_NAME "camera-rsc"
/*#define EP_NO_CLKMGR*/
#define BYPASS_REG (0)
/*#define RSC_DEBUG_USE*/
#define DUMMY_RSC (0)
/*I can' test the situation in FPGA due to slow FPGA. */
#define MyTag "[RSC]"
#define IRQTag "KEEPER"

#define LOG_VRB(format, args...) pr_debug(MyTag format, ##args)

#ifdef RSC_DEBUG_USE
#define LOG_DBG(format, args...) pr_debug(MyTag format, ##args)
#else
#define LOG_DBG(format, args...)
#endif

#define LOG_INF(format, args...) pr_info(MyTag format, ##args)
#define LOG_NOTICE(format, args...) pr_notice(MyTag format, ##args)
#define LOG_WRN(format, args...) pr_info(MyTag format, ##args)
#define LOG_ERR(format, args...) pr_info(MyTag format, ##args)
#define LOG_AST(format, args...) pr_info(MyTag format, ##args)

bool g_RSC_PMState;
/*******************************************************************************
 *
 ******************************************************************************/
/* #define RSC_WR32(addr, data)    iowrite32(data, addr) // For other projects.
 */
#define RSC_WR32(addr, data) writel(data, addr)
#define RSC_RD32(addr) readl(addr)
/*******************************************************************************
 *
 ******************************************************************************/
/* dynamic log level */
#define RSC_DBG_DBGLOG (0x00000001)
#define RSC_DBG_INFLOG (0x00000002)
#define RSC_DBG_INT (0x00000004)
#define RSC_DBG_READ_REG (0x00000008)
#define RSC_DBG_WRITE_REG (0x00000010)
#define RSC_DBG_TASKLET (0x00000020)

/*
 *    CAM interrupt status
 */

/* normal siganl : happens to be the same bit as register bit*/
/*#define RSC_INT_ST           (1<<0)*/

/*
 *   IRQ signal mask
 */

#define INT_ST_MASK_RSC (RSC_INT_ST)

#define CMDQ_REG_MASK 0xffffffff
#define RSC_START 0x1

#define RSC_ENABLE 0x1


/* static irqreturn_t RSC_Irq_CAM_A(signed int  Irq,void *DeviceId); */
static irqreturn_t ISP_Irq_RSC(signed int Irq, void *DeviceId);
static void RSC_ScheduleWork(struct work_struct *data);

typedef irqreturn_t (*IRQ_CB)(signed int, void *);

struct ISR_TABLE {
	IRQ_CB isr_fp;
	unsigned int int_number;
	char device_name[16];
};

#ifndef CONFIG_OF
const struct ISR_TABLE RSC_IRQ_CB_TBL[RSC_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_RSC, RSC_IRQ_BIT_ID, "rsc"},
};

#else
/* int number is got from kernel api */
const struct ISR_TABLE RSC_IRQ_CB_TBL[RSC_IRQ_TYPE_AMOUNT] = {
#if DUMMY_RSC
	{ISP_Irq_RSC, 0, "rsc-dummy"},
#else
	{ISP_Irq_RSC, 0, "rsc"},
#endif
};
#endif
/*
 */
/*  */
typedef void (*tasklet_cb)(unsigned long);
struct Tasklet_table {
	tasklet_cb tkt_cb;
	struct tasklet_struct *pRSC_tkt;
};

struct tasklet_struct Rsctkt[RSC_IRQ_TYPE_AMOUNT];

static void ISP_TaskletFunc_RSC(unsigned long data);

static struct Tasklet_table RSC_tasklet[RSC_IRQ_TYPE_AMOUNT] = {
	{ISP_TaskletFunc_RSC, &Rsctkt[RSC_IRQ_TYPE_INT_RSC_ST]},
};
static struct work_struct logWork;
static void logPrint(struct work_struct *data);


static DEFINE_MUTEX(gRscMutex);
static DEFINE_MUTEX(gRscDequeMutex);

#ifdef CONFIG_OF

struct RSC_device {
	void __iomem *regs;
	struct device *dev;
	int irq;
};

static struct RSC_device *RSC_devs;
static int nr_RSC_devs;

/* Get HW modules' base address from device nodes */
#define RSC_DEV_NODE_IDX 1
#define IPESYS_DEV_MODE_IDX 0


#define ISP_RSC_BASE (RSC_devs[RSC_DEV_NODE_IDX].regs)
#define ISP_IPESYS_BASE (RSC_devs[IPESYS_DEV_MODE_IDX].regs)




#else
#define ISP_RSC_BASE (IMGSYS_BASE + 0x2800)

#endif

static unsigned int g_u4EnableClockCount;
static unsigned int g_SuspendCnt;

/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32

enum RSC_FRAME_STATUS_ENUM {
	RSC_FRAME_STATUS_EMPTY,    /* 0 */
	RSC_FRAME_STATUS_ENQUE,    /* 1 */
	RSC_FRAME_STATUS_RUNNING,  /* 2 */
	RSC_FRAME_STATUS_FINISHED, /* 3 */
	RSC_FRAME_STATUS_TOTAL
};

enum RSC_REQUEST_STATE_ENUM {
	RSC_REQUEST_STATE_EMPTY,    /* 0 */
	RSC_REQUEST_STATE_PENDING,  /* 1 */
	RSC_REQUEST_STATE_RUNNING,  /* 2 */
	RSC_REQUEST_STATE_FINISHED, /* 3 */
	RSC_REQUEST_STATE_TOTAL
};

struct RSC_REQUEST_STRUCT {
	enum RSC_REQUEST_STATE_ENUM State;
	pid_t processID;       /* caller process ID */
	unsigned int callerID; /* caller thread ID */

	unsigned int
		enqueReqNum;   /* to judge it belongs to which frame package */
	signed int FrameWRIdx; /* Frame write Index */
	signed int RrameRDIdx; /* Frame read Index */
	enum RSC_FRAME_STATUS_ENUM
		RscFrameStatus[_SUPPORT_MAX_RSC_FRAME_REQUEST_];
	struct RSC_Config RscFrameConfig[_SUPPORT_MAX_RSC_FRAME_REQUEST_];
};

struct RSC_REQUEST_RING_STRUCT {
	signed int WriteIdx;     /* enque how many request  */
	signed int ReadIdx;      /* read which request index */
	signed int HWProcessIdx; /* HWWriteIdx */
	struct RSC_REQUEST_STRUCT
		RSCReq_Struct[_SUPPORT_MAX_RSC_REQUEST_RING_SIZE_];
};

struct RSC_CONFIG_STRUCT {
	struct RSC_Config RscFrameConfig[_SUPPORT_MAX_RSC_FRAME_REQUEST_];
};

static struct RSC_REQUEST_RING_STRUCT g_RSC_ReqRing;
static struct RSC_CONFIG_STRUCT g_RscEnqueReq_Struct;
static struct RSC_CONFIG_STRUCT g_RscDequeReq_Struct;
static struct engine_requests rsc_reqs;
static struct RSC_Request kRscReq;
/******************************************************************************
 *
 ******************************************************************************/
struct RSC_USER_INFO_STRUCT {
	pid_t Pid;
	pid_t Tid;
};
enum RSC_PROCESS_ID_ENUM {
	RSC_PROCESS_ID_NONE,
	RSC_PROCESS_ID_RSC,
	RSC_PROCESS_ID_AMOUNT
};

/******************************************************************************
 *
 ******************************************************************************/
struct RSC_IRQ_INFO_STRUCT {
	unsigned int Status[RSC_IRQ_TYPE_AMOUNT];
	signed int RscIrqCnt;
	pid_t ProcessID[RSC_PROCESS_ID_AMOUNT];
	unsigned int Mask[RSC_IRQ_TYPE_AMOUNT];
};

struct RSC_INFO_STRUCT {
	spinlock_t SpinLockRSCRef;
	spinlock_t SpinLockRSC;
	spinlock_t SpinLockIrq[RSC_IRQ_TYPE_AMOUNT];
	wait_queue_head_t WaitQueueHead;
	struct work_struct ScheduleRscWork;
	struct workqueue_struct *wkqueue;
	unsigned int UserCount; /* User Count */
	unsigned int DebugMask; /* Debug Mask */
	signed int IrqNum;
	struct RSC_IRQ_INFO_STRUCT IrqInfo;
	signed int WriteReqIdx;
	signed int ReadReqIdx;
	pid_t ProcessID[_SUPPORT_MAX_RSC_FRAME_REQUEST_];
};

static struct RSC_INFO_STRUCT RSCInfo;

#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
struct pm_qos_request rsc_pm_qos_request;
#endif

enum eLOG_TYPE {
	_LOG_DBG = 0,
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
static struct SV_LOG_STR gSvLog[RSC_IRQ_TYPE_AMOUNT];

/*
 *   for irq used,keep log until IRQ_LOG_PRINTER being involked,
 *   limited:
 *   each log must shorter than 512 bytes
 *  total log length in each irq/logtype can't over 1024 bytes
 */
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
	ptr = pDes = (char *)\
		&(gSvLog[irq]._str[ppb][logT][gSvLog[irq]._cnt[ppb][logT]]);   \
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
						LOG_DBG("%s", &ptr[\
							NORMAL_STR_LEN*logi]);\
					} else {\
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
						LOG_INF("%s",		       \
						    &ptr[NORMAL_STR_LEN*logi]);\
					} else {\
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
					} else {\
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
			ptr = pDes = (char *)\
			&(pSrc->_str[ppb][logT][pSrc->_cnt[ppb][logT]]);\
			ptr2 = &(pSrc->_cnt[ppb][logT]);\
			snprintf((char *)(pDes), avaLen, fmt, ##__VA_ARGS__);  \
			while (*ptr++ != '\0') {\
				(*ptr2)++;\
			} \
		} \
	} \
} while (0)

#define IRQ_LOG_PRINTER(irq, ppb_in, logT_in) do {\
	struct SV_LOG_STR *pSrc = &gSvLog[irq];\
	char *ptr;\
	unsigned int i;\
	signed int ppb = 0;\
	signed int logT = 0;\
	if (ppb_in > 1) {\
		ppb = 1;\
	} else {\
		ppb = ppb_in;\
	} \
	if (logT_in > _LOG_ERR) {\
		logT = _LOG_ERR;\
	} else {\
		logT = logT_in;\
	} \
	ptr = pSrc->_str[ppb][logT];\
	if (pSrc->_cnt[ppb][logT] != 0) {\
		if (logT == _LOG_DBG) {\
			for (i = 0; i < DBG_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else {\
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
			} else {\
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
			} else {\
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



#define IPESYS_REG_CG_CON               (ISP_IPESYS_BASE + 0x0)
#define IPESYS_REG_CG_SET               (ISP_IPESYS_BASE + 0x4)
#define IPESYS_REG_CG_CLR               (ISP_IPESYS_BASE + 0x8)

/* RSC unmapped base address macro for GCE to access */
#define RSC_RST_HW                    (RSC_BASE_HW)
#define RSC_START_HW                  (RSC_BASE_HW + 0x04)

#define RSC_DCM_CTL_HW                (RSC_BASE_HW + 0x08)
#define RSC_DCM_STAUS_HW              (RSC_BASE_HW + 0x0C)

#define RSC_INT_CTL_HW                (RSC_BASE_HW + 0x10)
#define RSC_INT_STATUS_HW             (RSC_BASE_HW + 0x14)
#define RSC_CTRL_HW                   (RSC_BASE_HW + 0x18)
#define RSC_SIZE_HW                   (RSC_BASE_HW + 0x1C)

#define RSC_SR_HW                     (RSC_BASE_HW + 0x20)
#define RSC_BR_HW                     (RSC_BASE_HW + 0x24)
#define RSC_MV_OFFSET_HW              (RSC_BASE_HW + 0x28)
#define RSC_GMV_OFFSET_HW             (RSC_BASE_HW + 0x2c)
#define RSC_PREPARE_MV_CTRL_HW        (RSC_BASE_HW + 0x30)
#define RSC_CAND_NUM_HW               (RSC_BASE_HW + 0x34)
#define RSC_EVEN_CAND_SEL_0_HW        (RSC_BASE_HW + 0x38)
#define RSC_EVEN_CAND_SEL_1_HW        (RSC_BASE_HW + 0x3c)
#define RSC_EVEN_CAND_SEL_2_HW        (RSC_BASE_HW + 0x40)
#define RSC_EVEN_CAND_SEL_3_HW        (RSC_BASE_HW + 0x44)
#define RSC_EVEN_CAND_SEL_4_HW        (RSC_BASE_HW + 0x48)
#define RSC_ODD_CAND_SEL_0_HW         (RSC_BASE_HW + 0x4C)
#define RSC_ODD_CAND_SEL_1_HW         (RSC_BASE_HW + 0x50)
#define RSC_ODD_CAND_SEL_2_HW         (RSC_BASE_HW + 0x54)
#define RSC_ODD_CAND_SEL_3_HW         (RSC_BASE_HW + 0x58)
#define RSC_ODD_CAND_SEL_4_HW         (RSC_BASE_HW + 0x5C)
#define RSC_RAND_HORZ_LUT_HW          (RSC_BASE_HW + 0x60)
#define RSC_RAND_VERT_LUT_HW          (RSC_BASE_HW + 0x64)
#define RSC_CURR_BLK_CTRL_HW          (RSC_BASE_HW + 0x68)
#define RSC_SAD_CTRL_HW               (RSC_BASE_HW + 0x6C)
#define RSC_SAD_EDGE_GAIN_CTRL_HW     (RSC_BASE_HW + 0x70)
#define RSC_SAD_CRNR_GAIN_CTRL_HW     (RSC_BASE_HW + 0x74)
#define RSC_STILL_STRIP_CTRL_0_HW     (RSC_BASE_HW + 0x78)
#define RSC_STILL_STRIP_CTRL_1_HW     (RSC_BASE_HW + 0x7C)
#define RSC_MV_PNLTY_CTRL_HW          (RSC_BASE_HW + 0x80)
#define RSC_ZERO_PNLTY_CTRL_HW        (RSC_BASE_HW + 0x84)
#define RSC_RAND_PNLTY_CTRL_HW        (RSC_BASE_HW + 0x88)
#define RSC_RAND_PNLTY_GAIN_CTRL_0_HW (RSC_BASE_HW + 0x8C)
#define RSC_RAND_PNLTY_GAIN_CTRL_1_HW (RSC_BASE_HW + 0x90)
#define RSC_TMPR_PNLTY_GAIN_CTRL_0_HW (RSC_BASE_HW + 0x94)
#define RSC_TMPR_PNLTY_GAIN_CTRL_1_HW (RSC_BASE_HW + 0x98)

#define RSC_IMGI_C_BASE_ADDR_HW       (RSC_BASE_HW + 0x9C)
#define RSC_IMGI_C_STRIDE_HW          (RSC_BASE_HW + 0xA0)
#define RSC_IMGI_P_BASE_ADDR_HW       (RSC_BASE_HW + 0xA4)
#define RSC_IMGI_P_STRIDE_HW          (RSC_BASE_HW + 0xA8)
#define RSC_MVI_BASE_ADDR_HW          (RSC_BASE_HW + 0xAC)
#define RSC_MVI_STRIDE_HW             (RSC_BASE_HW + 0xB0)
#define RSC_APLI_C_BASE_ADDR_HW       (RSC_BASE_HW + 0xB4)
#define RSC_APLI_P_BASE_ADDR_HW       (RSC_BASE_HW + 0xB8)
#define RSC_MVO_BASE_ADDR_HW          (RSC_BASE_HW + 0xBC)
#define RSC_MVO_STRIDE_HW             (RSC_BASE_HW + 0xC0)
#define RSC_BVO_BASE_ADDR_HW          (RSC_BASE_HW + 0xC4)
#define RSC_BVO_STRIDE_HW             (RSC_BASE_HW + 0xC8)

#define RSC_STA_0_HW                  (RSC_BASE_HW + 0x100)
#define RSC_DBG_INFO_00_HW            (RSC_BASE_HW + 0x120)
#define RSC_DBG_INFO_01_HW            (RSC_BASE_HW + 0x124)
#define RSC_DBG_INFO_02_HW            (RSC_BASE_HW + 0x128)
#define RSC_DBG_INFO_03_HW            (RSC_BASE_HW + 0x12C)
#define RSC_DBG_INFO_04_HW            (RSC_BASE_HW + 0x130)
#define RSC_DBG_INFO_05_HW            (RSC_BASE_HW + 0x134)
#define RSC_DBG_INFO_06_HW            (RSC_BASE_HW + 0x138)

#define RSC_SPARE_0_HW                (RSC_BASE_HW + 0x1F8)
#define RSC_SPARE_1_HW                (RSC_BASE_HW + 0x1FC)
#define RSC_DMA_DBG_HW                (RSC_BASE_HW + 0x7F4)
#define RSC_DMA_REQ_STATUS_HW         (RSC_BASE_HW + 0x7F8)
#define RSC_DMA_RDY_STATUS_HW         (RSC_BASE_HW + 0x7FC)

#define RSC_DMA_DMA_SOFT_RSTSTAT_HW   (RSC_BASE_HW + 0x800)
#define RSC_DMA_VERTICAL_FLIP_EN_HW   (RSC_BASE_HW + 0x804)
#define RSC_DMA_DMA_SOFT_RESET_HW     (RSC_BASE_HW + 0x808)
#define RSC_DMA_LAST_ULTRA_HW         (RSC_BASE_HW + 0x80C)
#define RSC_DMA_SPECIAL_FUN_HW        (RSC_BASE_HW + 0x810)
#define RSC_DMA_RSCO_BASE_ADDR_HW     (RSC_BASE_HW + 0x830)
#define RSC_DMA_RSCO_BASE_ADDR_2_HW   (RSC_BASE_HW + 0x834)
#define RSC_DMA_RSCO_OFST_ADDR_HW     (RSC_BASE_HW + 0x838)
#define RSC_DMA_RSCO_OFST_ADDR_2_HW   (RSC_BASE_HW + 0x83C)
#define RSC_DMA_RSCO_XSIZE_HW         (RSC_BASE_HW + 0x840)
#define RSC_DMA_RSCO_YSIZE_HW         (RSC_BASE_HW + 0x844)
#define RSC_DMA_RSCO_STRIDE_HW        (RSC_BASE_HW + 0x848)
#define RSC_DMA_RSCO_CON_HW           (RSC_BASE_HW + 0x84C)
#define RSC_DMA_RSCO_CON2_HW          (RSC_BASE_HW + 0x850)
#define RSC_DMA_RSCO_CON3_HW          (RSC_BASE_HW + 0x854)
#define RSC_DMA_RSCI_BASE_ADDR_HW     (RSC_BASE_HW + 0x890)
#define RSC_DMA_RSCI_BASE_ADDR_2_HW   (RSC_BASE_HW + 0x894)
#define RSC_DMA_RSCI_OFST_ADDR_HW     (RSC_BASE_HW + 0x898)
#define RSC_DMA_RSCI_OFST_ADDR_2_HW   (RSC_BASE_HW + 0x89C)
#define RSC_DMA_RSCI_XSIZE_HW         (RSC_BASE_HW + 0x8A0)
#define RSC_DMA_RSCI_YSIZE_HW         (RSC_BASE_HW + 0x8A4)
#define RSC_DMA_RSCI_STRIDE_HW        (RSC_BASE_HW + 0x8A8)
#define RSC_DMA_RSCI_CON_HW           (RSC_BASE_HW + 0x8AC)
#define RSC_DMA_RSCI_CON2_HW          (RSC_BASE_HW + 0x8B0)
#define RSC_DMA_RSCI_CON3_HW          (RSC_BASE_HW + 0x8B4)
#define RSC_DMA_DMA_ERR_CTRL_HW       (RSC_BASE_HW + 0x900)
#define RSC_DMA_RSCO_ERR_STAT_HW      (RSC_BASE_HW + 0x904)
#define RSC_DMA_RSCI_ERR_STAT_HW      (RSC_BASE_HW + 0x908)
#define RSC_DMA_DMA_DEBUG_ADDR_HW     (RSC_BASE_HW + 0x90C)
#define RSC_DMA_DMA_DEBUG_SEL_HW      (RSC_BASE_HW + 0x928)
#define RSC_DMA_DMA_BW_SELF_TEST_HW   (RSC_BASE_HW + 0x92C)

/*SW Access Registers : using mapped base address from DTS*/
#define RSC_RST_REG                     (ISP_RSC_BASE)
#define RSC_START_REG                  (ISP_RSC_BASE + 0x04)

#define RSC_DCM_CTL_REG                (ISP_RSC_BASE + 0x08)
#define RSC_DCM_STAUS_REG              (ISP_RSC_BASE + 0x0C)

#define RSC_INT_CTL_REG                (ISP_RSC_BASE + 0x10)
#define RSC_INT_STATUS_REG             (ISP_RSC_BASE + 0x14)
#define RSC_CTRL_REG                   (ISP_RSC_BASE + 0x18)
#define RSC_SIZE_REG                   (ISP_RSC_BASE + 0x1C)

#define RSC_SR_REG                     (ISP_RSC_BASE + 0x20)
#define RSC_BR_REG                     (ISP_RSC_BASE + 0x24)
#define RSC_MV_OFFSET_REG              (ISP_RSC_BASE + 0x28)
#define RSC_GMV_OFFSET_REG             (ISP_RSC_BASE + 0x2c)
#define RSC_PREPARE_MV_CTRL_REG        (ISP_RSC_BASE + 0x30)
#define RSC_CAND_NUM_REG               (ISP_RSC_BASE + 0x34)
#define RSC_EVEN_CAND_SEL_0_REG        (ISP_RSC_BASE + 0x38)
#define RSC_EVEN_CAND_SEL_1_REG        (ISP_RSC_BASE + 0x3c)
#define RSC_EVEN_CAND_SEL_2_REG        (ISP_RSC_BASE + 0x40)
#define RSC_EVEN_CAND_SEL_3_REG        (ISP_RSC_BASE + 0x44)
#define RSC_EVEN_CAND_SEL_4_REG        (ISP_RSC_BASE + 0x48)
#define RSC_ODD_CAND_SEL_0_REG         (ISP_RSC_BASE + 0x4C)
#define RSC_ODD_CAND_SEL_1_REG         (ISP_RSC_BASE + 0x50)
#define RSC_ODD_CAND_SEL_2_REG         (ISP_RSC_BASE + 0x54)
#define RSC_ODD_CAND_SEL_3_REG         (ISP_RSC_BASE + 0x58)
#define RSC_ODD_CAND_SEL_4_REG         (ISP_RSC_BASE + 0x5C)
#define RSC_RAND_HORZ_LUT_REG          (ISP_RSC_BASE + 0x60)
#define RSC_RAND_VERT_LUT_REG          (ISP_RSC_BASE + 0x64)
#define RSC_CURR_BLK_CTRL_REG          (ISP_RSC_BASE + 0x68)
#define RSC_SAD_CTRL_REG               (ISP_RSC_BASE + 0x6C)
#define RSC_SAD_EDGE_GAIN_CTRL_REG     (ISP_RSC_BASE + 0x70)
#define RSC_SAD_CRNR_GAIN_CTRL_REG     (ISP_RSC_BASE + 0x74)
#define RSC_STILL_STRIP_CTRL_0_REG     (ISP_RSC_BASE + 0x78)
#define RSC_STILL_STRIP_CTRL_1_REG     (ISP_RSC_BASE + 0x7C)
#define RSC_MV_PNLTY_CTRL_REG          (ISP_RSC_BASE + 0x80)
#define RSC_ZERO_PNLTY_CTRL_REG        (ISP_RSC_BASE + 0x84)
#define RSC_RAND_PNLTY_CTRL_REG        (ISP_RSC_BASE + 0x88)
#define RSC_RAND_PNLTY_GAIN_CTRL_0_REG (ISP_RSC_BASE + 0x8C)
#define RSC_RAND_PNLTY_GAIN_CTRL_1_REG (ISP_RSC_BASE + 0x90)
#define RSC_TMPR_PNLTY_GAIN_CTRL_0_REG (ISP_RSC_BASE + 0x94)
#define RSC_TMPR_PNLTY_GAIN_CTRL_1_REG (ISP_RSC_BASE + 0x98)

#define RSC_IMGI_C_BASE_ADDR_REG       (ISP_RSC_BASE + 0x9C)
#define RSC_IMGI_C_STRIDE_REG          (ISP_RSC_BASE + 0xA0)
#define RSC_IMGI_P_BASE_ADDR_REG       (ISP_RSC_BASE + 0xA4)
#define RSC_IMGI_P_STRIDE_REG          (ISP_RSC_BASE + 0xA8)
#define RSC_MVI_BASE_ADDR_REG          (ISP_RSC_BASE + 0xAC)
#define RSC_MVI_STRIDE_REG             (ISP_RSC_BASE + 0xB0)
#define RSC_APLI_C_BASE_ADDR_REG       (ISP_RSC_BASE + 0xB4)
#define RSC_APLI_P_BASE_ADDR_REG       (ISP_RSC_BASE + 0xB8)
#define RSC_MVO_BASE_ADDR_REG          (ISP_RSC_BASE + 0xBC)
#define RSC_MVO_STRIDE_REG             (ISP_RSC_BASE + 0xC0)
#define RSC_BVO_BASE_ADDR_REG          (ISP_RSC_BASE + 0xC4)
#define RSC_BVO_STRIDE_REG             (ISP_RSC_BASE + 0xC8)

#define RSC_STA_0_REG                  (ISP_RSC_BASE + 0x100)
#define RSC_DBG_INFO_00_REG            (ISP_RSC_BASE + 0x120)
#define RSC_DBG_INFO_01_REG            (ISP_RSC_BASE + 0x124)
#define RSC_DBG_INFO_02_REG            (ISP_RSC_BASE + 0x128)
#define RSC_DBG_INFO_03_REG            (ISP_RSC_BASE + 0x12C)
#define RSC_DBG_INFO_04_REG            (ISP_RSC_BASE + 0x130)
#define RSC_DBG_INFO_05_REG            (ISP_RSC_BASE + 0x134)
#define RSC_DBG_INFO_06_REG            (ISP_RSC_BASE + 0x138)

#define RSC_SPARE_0_REG                (ISP_RSC_BASE + 0x1F8)
#define RSC_SPARE_1_REG                (ISP_RSC_BASE + 0x1FC)
#define RSC_DMA_DBG_REG                (ISP_RSC_BASE + 0x7F4)
#define RSC_DMA_REQ_STATUS_REG         (ISP_RSC_BASE + 0x7F8)
#define RSC_DMA_RDY_STATUS_REG         (ISP_RSC_BASE + 0x7FC)

#define RSC_DMA_DMA_SOFT_RSTSTAT_REG   (ISP_RSC_BASE + 0x800)
#define RSC_DMA_VERTICAL_FLIP_EN_REG   (ISP_RSC_BASE + 0x804)
#define RSC_DMA_DMA_SOFT_RESET_REG     (ISP_RSC_BASE + 0x808)
#define RSC_DMA_LAST_ULTRA_REG         (ISP_RSC_BASE + 0x80C)
#define RSC_DMA_SPECIAL_FUN_REG        (ISP_RSC_BASE + 0x810)
#define RSC_DMA_RSCO_BASE_ADDR_REG     (ISP_RSC_BASE + 0x830)
#define RSC_DMA_RSCO_BASE_ADDR_2_REG   (ISP_RSC_BASE + 0x834)
#define RSC_DMA_RSCO_OFST_ADDR_REG     (ISP_RSC_BASE + 0x838)
#define RSC_DMA_RSCO_OFST_ADDR_2_REG   (ISP_RSC_BASE + 0x83C)
#define RSC_DMA_RSCO_XSIZE_REG         (ISP_RSC_BASE + 0x840)
#define RSC_DMA_RSCO_YSIZE_REG         (ISP_RSC_BASE + 0x844)
#define RSC_DMA_RSCO_STRIDE_REG        (ISP_RSC_BASE + 0x848)
#define RSC_DMA_RSCO_CON_REG           (ISP_RSC_BASE + 0x84C)
#define RSC_DMA_RSCO_CON2_REG          (ISP_RSC_BASE + 0x850)
#define RSC_DMA_RSCO_CON3_REG          (ISP_RSC_BASE + 0x854)
#define RSC_DMA_RSCI_BASE_ADDR_REG     (ISP_RSC_BASE + 0x890)
#define RSC_DMA_RSCI_BASE_ADDR_2_REG   (ISP_RSC_BASE + 0x894)
#define RSC_DMA_RSCI_OFST_ADDR_REG     (ISP_RSC_BASE + 0x898)
#define RSC_DMA_RSCI_OFST_ADDR_2_REG   (ISP_RSC_BASE + 0x89C)
#define RSC_DMA_RSCI_XSIZE_REG         (ISP_RSC_BASE + 0x8A0)
#define RSC_DMA_RSCI_YSIZE_REG         (ISP_RSC_BASE + 0x8A4)
#define RSC_DMA_RSCI_STRIDE_REG        (ISP_RSC_BASE + 0x8A8)
#define RSC_DMA_RSCI_CON_REG           (ISP_RSC_BASE + 0x8AC)
#define RSC_DMA_RSCI_CON2_REG          (ISP_RSC_BASE + 0x8B0)
#define RSC_DMA_RSCI_CON3_REG          (ISP_RSC_BASE + 0x8B4)
#define RSC_DMA_DMA_ERR_CTRL_REG       (ISP_RSC_BASE + 0x900)
#define RSC_DMA_RSCO_ERR_STAT_REG      (ISP_RSC_BASE + 0x904)
#define RSC_DMA_RSCI_ERR_STAT_REG      (ISP_RSC_BASE + 0x908)
#define RSC_DMA_DMA_DEBUG_ADDR_REG     (ISP_RSC_BASE + 0x90C)
#define RSC_DMA_DMA_DEBUG_SEL_REG      (ISP_RSC_BASE + 0x928)
#define RSC_DMA_DMA_BW_SELF_TEST_REG   (ISP_RSC_BASE + 0x92C)

/*******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int RSC_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

/*******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int RSC_UsToJiffies(unsigned int Us)
{
	return (((Us / 1000) * HZ + 512) >> 10);
}

/*******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int
RSC_GetIRQState(unsigned int type, unsigned int userNumber, unsigned int stus,
		enum RSC_PROCESS_ID_ENUM whichReq, int ProcessID)
{
	unsigned int ret = 0;
	unsigned long flags;

	/*  */
	spin_lock_irqsave(&(RSCInfo.SpinLockIrq[type]), flags);

	if (stus & RSC_INT_ST) {
		ret = ((RSCInfo.IrqInfo.RscIrqCnt > 0) &&
		       (RSCInfo.IrqInfo.ProcessID[whichReq] == ProcessID));
	} else {
		LOG_ERR("EWIRQ,type:%d,u:%d,stat:%d,wReq:%d,PID:0x%x\n",
			type, userNumber, stus, whichReq, ProcessID);
	}
	spin_unlock_irqrestore(&(RSCInfo.SpinLockIrq[type]), flags);
	/*  */
	return ret;
}


/*******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int RSC_JiffiesToMs(unsigned int Jiffies)
{
	return ((Jiffies * 1000) / HZ);
}

#define RegDump(start, end)                                                \
{                                                                          \
unsigned int i;                                                            \
for (i = start; i <= end; i += 0x10) {                                     \
	LOG_DBG("[0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]", \
		(unsigned int)(ISP_RSC_BASE + i),                          \
		(unsigned int)RSC_RD32(ISP_RSC_BASE + i),                  \
		(unsigned int)(ISP_RSC_BASE + i + 0x4),                    \
		(unsigned int)RSC_RD32(ISP_RSC_BASE + i +                  \
				       0x4),                               \
		(unsigned int)(ISP_RSC_BASE + i + 0x8),                    \
		(unsigned int)RSC_RD32(ISP_RSC_BASE + i +                  \
				       0x8),                               \
		(unsigned int)(ISP_RSC_BASE + i + 0xc),                    \
		(unsigned int)RSC_RD32(ISP_RSC_BASE + i +                  \
				       0xc));                              \
}                                                                          \
}

signed int rsc_enque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt;
	/*TODO: define engine request struct */
	struct RSC_Request *_req;
	struct RSC_Config *pRscConfig;

	_req = (struct RSC_Request *) req;

	if (frames == NULL || _req == NULL)
		return -1;

	/*TODO: m_ReqNum is FrmNum; FIFO only thus f starts from 0 */
	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		memcpy(frames[f].data, &_req->m_pRscConfig[f],
						sizeof(struct RSC_Config));

		pRscConfig = &_req->m_pRscConfig[f];
#ifdef RSC_DEBUG_USE
		LOG_ERR("[%s] request queued with  frame(%d)", __func__, f);
		LOG_DBG("[%s] RSC_CTRL_REG:0x%x!\n", __func__,
							pRscConfig->RSC_CTRL);
		LOG_DBG("[%s] RSC_SIZE_REG:0x%x!\n", __func__,
							pRscConfig->RSC_SIZE);
		LOG_DBG("[%s] RSC_IMGI_C_BASE_ADDR_REG:0x%x!\n", __func__,
					pRscConfig->RSC_IMGI_C_BASE_ADDR);
		LOG_DBG("[%s] RSC_IMGI_C_STRIDE_REG:0x%x!\n", __func__,
						pRscConfig->RSC_IMGI_C_STRIDE);
		LOG_DBG("[%s] RSC_IMGI_P_BASE_ADDR_REG:0x%x!\n", __func__,
					pRscConfig->RSC_IMGI_P_BASE_ADDR);
		LOG_DBG("[%s] RSC_IMGI_P_STRIDE_REG:0x%x!\n", __func__,
						pRscConfig->RSC_IMGI_P_STRIDE);
		LOG_DBG("[%s] RSC_MVI_C_BASE_ADDR_REG:0x%x!\n", __func__,
						pRscConfig->RSC_MVI_BASE_ADDR);
		LOG_DBG("[%s] RSC_MVI_C_STRIDE_REG:0x%x!\n", __func__,
						pRscConfig->RSC_MVI_STRIDE);
		LOG_DBG("[%s] RSC_MVO_BASE_ADDR_REG:0x%x!\n", __func__,
						pRscConfig->RSC_MVO_BASE_ADDR);
		LOG_DBG("[%s] RSC_MVO_STRIDE_REG:0x%x!\n", __func__,
						pRscConfig->RSC_MVO_STRIDE);
		LOG_DBG("[%s] RSC_BVO_BASE_ADDR_REG:0x%x!\n", __func__,
						pRscConfig->RSC_BVO_BASE_ADDR);
		LOG_DBG("[%s] RSC_BVO_STRIDE_REG:0x%x!\n", __func__,
						pRscConfig->RSC_BVO_STRIDE);
		LOG_DBG("[%s] RSC_APLI_C_BASE_ADDR_REG:0x%x!\n", __func__,
					pRscConfig->RSC_APLI_C_BASE_ADDR);
		LOG_DBG("[%s] RSC_APLI_P_BASE_ADDR_REG:0x%x!\n", __func__,
					pRscConfig->RSC_APLI_P_BASE_ADDR);
#endif
	}

	return 0;
}

signed int rsc_deque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt;
	struct RSC_Request *_req;
	struct RSC_Config *pRscConfig;

	_req = (struct RSC_Request *) req;

	if (frames == NULL || _req == NULL)
		return -1;

	/*TODO: m_ReqNum is FrmNum; FIFO only thus f starts from 0 */
	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		memcpy(&_req->m_pRscConfig[f], frames[f].data,
						sizeof(struct RSC_Config));
		LOG_DBG("[%s]request dequeued frame(%d/%d).", __func__, f,
									fcnt);

		pRscConfig = &_req->m_pRscConfig[f];
#ifdef RSC_DEBUG_USE
		LOG_ERR(
		"[%s] request queued with  frame(%d)", __func__, f);
		LOG_DBG(
		"[%s] RSC_CTRL_REG:0x%x!\n",
			__func__, pRscConfig->RSC_CTRL);
		LOG_DBG(
		"[%s] RSC_SIZE_REG:0x%x!\n",
			__func__, pRscConfig->RSC_SIZE);
		LOG_DBG(
		"[%s] RSC_IMGI_C_BASE_ADDR_REG:0x%x!\n",
			__func__, pRscConfig->RSC_IMGI_C_BASE_ADDR);
		LOG_DBG(
		"[%s] RSC_IMGI_C_STRIDE_REG:0x%x!\n",
			__func__, pRscConfig->RSC_IMGI_C_STRIDE);
		LOG_DBG(
		"[%s] RSC_IMGI_P_BASE_ADDR_REG:0x%x!\n",
			__func__, pRscConfig->RSC_IMGI_P_BASE_ADDR);
		LOG_DBG(
		"[%s] RSC_IMGI_P_STRIDE_REG:0x%x!\n",
			__func__, pRscConfig->RSC_IMGI_P_STRIDE);
		LOG_DBG(
		"[%s] RSC_MVI_C_BASE_ADDR_REG:0x%x!\n",
			__func__, pRscConfig->RSC_MVI_BASE_ADDR);
		LOG_DBG(
		"[%s] RSC_MVI_C_STRIDE_REG:0x%x!\n",
			__func__, pRscConfig->RSC_MVI_STRIDE);
		LOG_DBG(
		"[%s] RSC_MVO_BASE_ADDR_REG:0x%x!\n",
			__func__, pRscConfig->RSC_MVO_BASE_ADDR);
		LOG_DBG(
		"[%s] RSC_MVO_STRIDE_REG:0x%x!\n",
			__func__, pRscConfig->RSC_MVO_STRIDE);
		LOG_DBG(
		"[%s] RSC_BVO_BASE_ADDR_REG:0x%x!\n",
			__func__, pRscConfig->RSC_BVO_BASE_ADDR);
		LOG_DBG(
		"[%s] RSC_BVO_STRIDE_REG:0x%x!\n",
			__func__, pRscConfig->RSC_BVO_STRIDE);
		LOG_DBG(
		"[%s] RSC_APLI_C_BASE_ADDR_REG:0x%x!\n",
			__func__, pRscConfig->RSC_APLI_C_BASE_ADDR);
		LOG_DBG(
		"[%s] RSC_APLI_P_BASE_ADDR_REG:0x%x!\n",
			__func__, pRscConfig->RSC_APLI_P_BASE_ADDR);
#endif
	}

	return 0;
}

signed int CmdqRSCHW(struct frame *frame)
#ifdef CMDQ_COMMON
{
	return 0;
}
#else
{
	struct RSC_Config *pRscConfig;
	struct cmdqRecStruct *handle;
	uint64_t engineFlag = (uint64_t)(1LL << CMDQ_ENG_RSC);
#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
	unsigned int w_imgi, h_imgi, w_mvio, h_mvio, w_bvo, h_bvo;
	unsigned int dma_bandwidth, trig_num;
#endif
	if (frame == NULL || frame->data == NULL)
		return -1;

	LOG_DBG("%s request sent to CMDQ driver", __func__);
	pRscConfig = (struct RSC_Config *) frame->data;

	LOG_DBG("RSC_CTRL_REG:0x%x!\n", pRscConfig->RSC_CTRL);
	LOG_DBG("RSC_SIZE_REG:0x%x!\n", pRscConfig->RSC_SIZE);
	LOG_DBG("RSC_IMGI_C_BASE_ADDR_REG:0x%x!\n",
					pRscConfig->RSC_IMGI_C_BASE_ADDR);
	LOG_DBG("RSC_IMGI_C_STRIDE_REG:0x%x!\n",
					pRscConfig->RSC_IMGI_C_STRIDE);
	LOG_DBG("RSC_IMGI_P_BASE_ADDR_REG:0x%x!\n",
					pRscConfig->RSC_IMGI_P_BASE_ADDR);
	LOG_DBG("RSC_IMGI_P_STRIDE_REG:0x%x!\n",
					pRscConfig->RSC_IMGI_P_STRIDE);
	LOG_DBG("RSC_MVI_C_BASE_ADDR_REG:0x%x!\n",
					pRscConfig->RSC_MVI_BASE_ADDR);
	LOG_DBG("RSC_MVI_C_STRIDE_REG:0x%x!\n",
					pRscConfig->RSC_MVI_STRIDE);
	LOG_DBG("RSC_MVO_BASE_ADDR_REG:0x%x!\n",
					pRscConfig->RSC_MVO_BASE_ADDR);
	LOG_DBG("RSC_MVO_STRIDE_REG:0x%x!\n",
					pRscConfig->RSC_MVO_STRIDE);
	LOG_DBG("RSC_BVO_BASE_ADDR_REG:0x%x!\n",
					pRscConfig->RSC_BVO_BASE_ADDR);
	LOG_DBG("RSC_BVO_STRIDE_REG:0x%x!\n",
					pRscConfig->RSC_BVO_STRIDE);
	LOG_DBG("RSC_APLI_C_BASE_ADDR_REG:0x%x!\n",
					pRscConfig->RSC_APLI_C_BASE_ADDR);
	LOG_DBG("RSC_APLI_P_BASE_ADDR_REG:0x%x!\n",
					pRscConfig->RSC_APLI_P_BASE_ADDR);


	cmdqRecCreate(CMDQ_SCENARIO_ISP_RSC, &handle);

	cmdqRecSetEngine(handle, engineFlag);

	cmdqRecWrite(handle, RSC_INT_CTL_HW, 0x1, CMDQ_REG_MASK);
	/* RSC Interrupt read-clear mode */

	cmdqRecWrite(handle, RSC_CTRL_HW, pRscConfig->RSC_CTRL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_SIZE_HW, pRscConfig->RSC_SIZE, CMDQ_REG_MASK);

	cmdqRecWrite(handle, RSC_APLI_C_BASE_ADDR_HW,
			pRscConfig->RSC_APLI_C_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_APLI_P_BASE_ADDR_HW,
			pRscConfig->RSC_APLI_P_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_IMGI_C_BASE_ADDR_HW,
			pRscConfig->RSC_IMGI_C_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_IMGI_P_BASE_ADDR_HW,
			pRscConfig->RSC_IMGI_P_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_IMGI_C_STRIDE_HW,
			pRscConfig->RSC_IMGI_C_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_IMGI_P_STRIDE_HW,
			pRscConfig->RSC_IMGI_P_STRIDE, CMDQ_REG_MASK);

	cmdqRecWrite(handle, RSC_MVI_BASE_ADDR_HW,
			pRscConfig->RSC_MVI_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_MVI_STRIDE_HW,
			pRscConfig->RSC_MVI_STRIDE, CMDQ_REG_MASK);

	cmdqRecWrite(handle, RSC_MVO_BASE_ADDR_HW,
			pRscConfig->RSC_MVO_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_MVO_STRIDE_HW,
			pRscConfig->RSC_MVO_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_BVO_BASE_ADDR_HW,
			pRscConfig->RSC_BVO_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_BVO_STRIDE_HW,
			pRscConfig->RSC_BVO_STRIDE, CMDQ_REG_MASK);
#ifdef RSC_TUNABLE
	cmdqRecWrite(handle, RSC_MV_OFFSET_HW,
			pRscConfig->RSC_MV_OFFSET, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_GMV_OFFSET_HW,
			pRscConfig->RSC_GMV_OFFSET, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_CAND_NUM_HW,
			pRscConfig->RSC_CAND_NUM, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_RAND_HORZ_LUT_HW,
			pRscConfig->RSC_RAND_HORZ_LUT, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_RAND_VERT_LUT_HW,
			pRscConfig->RSC_RAND_VERT_LUT, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_SAD_CTRL_HW,
			pRscConfig->RSC_SAD_CTRL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_SAD_EDGE_GAIN_CTRL_HW,
			pRscConfig->RSC_SAD_EDGE_GAIN_CTRL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_SAD_CRNR_GAIN_CTRL_HW,
			pRscConfig->RSC_SAD_CRNR_GAIN_CTRL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_STILL_STRIP_CTRL_0_HW,
			pRscConfig->RSC_STILL_STRIP_CTRL0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_STILL_STRIP_CTRL_1_HW,
			pRscConfig->RSC_STILL_STRIP_CTRL1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_RAND_PNLTY_CTRL_HW,
			pRscConfig->RSC_RAND_PNLTY_CTRL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_RAND_PNLTY_GAIN_CTRL_0_HW,
			pRscConfig->RSC_RAND_PNLTY_GAIN_CTRL0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, RSC_RAND_PNLTY_GAIN_CTRL_1_HW,
			pRscConfig->RSC_RAND_PNLTY_GAIN_CTRL1, CMDQ_REG_MASK);
#endif
	cmdqRecWrite(handle, RSC_DCM_CTL_HW, 0x1111, CMDQ_REG_MASK);

	cmdqRecWrite(handle, RSC_START_HW, 0x1, CMDQ_REG_MASK);
	/* RSC Interrupt read-clear mode */

	cmdqRecWait(handle, CMDQ_EVENT_RSC_EOF);
	cmdqRecWrite(handle, RSC_START_HW, 0x0, CMDQ_REG_MASK);
	/* RSC Interrupt read-clear mode */

#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
	trig_num = (pRscConfig->RSC_CTRL & 0x00000F00) >> 8;
	w_imgi = pRscConfig->RSC_SIZE & 0x000001FF;
	h_imgi = (pRscConfig->RSC_SIZE & 0x01FF0000) >> 16;

	w_mvio = ((w_imgi + 1) >> 1) - 1;
	w_mvio = ((w_mvio / 7) << 4) + (((((w_mvio % 7) + 1) * 18) + 7) >> 3);
	h_mvio = (h_imgi + 1) >> 1;

	w_bvo =  (w_imgi + 1) >> 1;
	h_bvo =  (h_imgi + 1) >> 1;

	dma_bandwidth = ((w_imgi * h_imgi) * 2 + (w_mvio * h_mvio) * 2 * 16 +
			(w_bvo * h_bvo)) * trig_num * 30 / 1000000;
	cmdq_task_update_property(handle, &dma_bandwidth, sizeof(unsigned int));
#endif
	/* non-blocking API, Please  use cmdqRecFlushAsync() */
	cmdq_task_flush_async_destroy(handle);	/* flush and destry in cmdq*/

	return 0;
}
#endif

signed int rsc_feedback(struct frame *frame)
{
	struct RSC_Config *pRscConfig;

	pRscConfig = (struct RSC_Config *) frame->data;

	/* TODO: read statistics and write to the frame data */
	pRscConfig->RSC_STA_0 = RSC_RD32(RSC_STA_0_REG);
	return 0;
}

static const struct engine_ops rsc_ops = {
	.req_enque_cb = rsc_enque_cb,
	.req_deque_cb = rsc_deque_cb,
	.frame_handler = CmdqRSCHW,
	.req_feedback_cb = rsc_feedback,
};

#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
void cmdq_pm_qos_start(struct TaskStruct *task, struct TaskStruct *task_list[],
								u32 size)
{
	unsigned int dma_bandwidth;

	dma_bandwidth = *(unsigned int *) task->prop_addr;
	pm_qos_update_request(&rsc_pm_qos_request, dma_bandwidth);
	LOG_INF("+ PMQOS Bandwidth : %d MB/sec\n", dma_bandwidth);
}

void cmdq_pm_qos_stop(struct TaskStruct *task, struct TaskStruct *task_list[],
								u32 size)
{
	pm_qos_update_request(&rsc_pm_qos_request, 0);
	LOG_DBG("- PMQOS Bandwidth : %d\n", 0);
}
#endif




/*
 *
 */
static signed int RSC_DumpReg(void)
#if BYPASS_REG
{
	return 0;
}
#else
{
	signed int Ret = 0;
	/*  */
	LOG_INF("- E.");
	/*  */
	LOG_INF("RSC Config Info\n");
	/* RSC Config0 */
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_CTRL_HW),
		(unsigned int)RSC_RD32(RSC_CTRL_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_SIZE_HW),
		(unsigned int)RSC_RD32(RSC_SIZE_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_IMGI_C_BASE_ADDR_HW),
		(unsigned int)RSC_RD32(RSC_IMGI_C_BASE_ADDR_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_IMGI_C_STRIDE_HW),
		(unsigned int)RSC_RD32(RSC_IMGI_C_STRIDE_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_IMGI_P_BASE_ADDR_HW),
		(unsigned int)RSC_RD32(RSC_IMGI_P_BASE_ADDR_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_IMGI_P_STRIDE_HW),
		(unsigned int)RSC_RD32(RSC_IMGI_P_STRIDE_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_MVI_BASE_ADDR_HW),
		(unsigned int)RSC_RD32(RSC_MVI_BASE_ADDR_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_MVI_STRIDE_HW),
		(unsigned int)RSC_RD32(RSC_MVI_STRIDE_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_APLI_C_BASE_ADDR_HW),
		(unsigned int)RSC_RD32(RSC_APLI_C_BASE_ADDR_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_APLI_P_BASE_ADDR_HW),
		(unsigned int)RSC_RD32(RSC_APLI_P_BASE_ADDR_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_MVO_BASE_ADDR_HW),
		(unsigned int)RSC_RD32(RSC_MVO_BASE_ADDR_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_MVO_STRIDE_HW),
		(unsigned int)RSC_RD32(RSC_MVO_STRIDE_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_BVO_BASE_ADDR_HW),
		(unsigned int)RSC_RD32(RSC_BVO_BASE_ADDR_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_BVO_STRIDE_HW),
		(unsigned int)RSC_RD32(RSC_BVO_STRIDE_REG));




	LOG_INF("RSC Debug Info\n");
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DBG_INFO_00_HW),
		(unsigned int)RSC_RD32(RSC_DBG_INFO_00_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DBG_INFO_01_HW),
		(unsigned int)RSC_RD32(RSC_DBG_INFO_01_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DBG_INFO_02_HW),
		(unsigned int)RSC_RD32(RSC_DBG_INFO_02_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DBG_INFO_03_HW),
		(unsigned int)RSC_RD32(RSC_DBG_INFO_03_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DBG_INFO_04_HW),
		(unsigned int)RSC_RD32(RSC_DBG_INFO_04_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DBG_INFO_05_HW),
		(unsigned int)RSC_RD32(RSC_DBG_INFO_05_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DBG_INFO_06_HW),
		(unsigned int)RSC_RD32(RSC_DBG_INFO_06_REG));

	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DMA_DBG_HW),
		(unsigned int)RSC_RD32(RSC_DMA_DBG_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DMA_REQ_STATUS_HW),
		(unsigned int)RSC_RD32(RSC_DMA_REQ_STATUS_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DMA_RDY_STATUS_HW),
		(unsigned int)RSC_RD32(RSC_DMA_RDY_STATUS_REG));

	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DMA_DMA_ERR_CTRL_HW),
		(unsigned int)RSC_RD32(RSC_DMA_DMA_ERR_CTRL_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DMA_RSCI_ERR_STAT_HW),
		(unsigned int)RSC_RD32(RSC_DMA_RSCI_ERR_STAT_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DMA_RSCO_ERR_STAT_HW),
		(unsigned int)RSC_RD32(RSC_DMA_RSCO_ERR_STAT_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(RSC_DCM_STAUS_HW),
		(unsigned int)RSC_RD32(RSC_DCM_STAUS_REG));


	LOG_INF("- X.");
	/*  */
	return Ret;
}
#endif
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/

static inline void RSC_Prepare_Enable_ccf_clock(void)
{
	int ret;
	/* open order:CG_SCP_SYS_MM0>CG_MM_SMI_COMMON>CG_SCP_SYS_ISP>RSC clk */
#ifdef CONFIG_MTK_SMI_EXT
	smi_bus_prepare_enable(SMI_LARB8_REG_INDX, "camera_rsc", true);
#endif
	ret = clk_prepare_enable(rsc_clk.CG_IPESYS_RSC);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IPESYS_RSC clock\n");

}

static inline void RSC_Disable_Unprepare_ccf_clock(void)
{
	/* close order:RSC clk>CG_SCP_SYS_ISP>CG_MM_SMI_COMMON>CG_SCP_SYS_MM0 */
	clk_disable_unprepare(rsc_clk.CG_IPESYS_RSC);
#ifdef CONFIG_MTK_SMI_EXT
	smi_bus_disable_unprepare(SMI_LARB8_REG_INDX, "camera_rsc", true);
#endif
}
#endif

/*******************************************************************************
 *
 ******************************************************************************/
static void RSC_EnableClock(bool En)
{
#if defined(EP_NO_CLKMGR)
	unsigned int setReg;
#endif

	if (En) {		/* Enable clock. */
/* LOG_DBG("clock enbled. g_u4EnableClockCount: %d.", g_u4EnableClockCount); */
		switch (g_u4EnableClockCount) {
		case 0:
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifndef EP_NO_CLKMGR
			RSC_Prepare_Enable_ccf_clock();
#else
			/* Enable clock by hardcode:
			 * 1. CAMSYS_CG_CLR (0x1A000008) = 0xffffffff;
			 * 2. IMG_CG_CLR (0x15000008) = 0xffffffff;
			 */
			setReg = 0xFFFFFFFF;
			RSC_WR32(IPESYS_REG_CG_CLR, setReg);
#endif
#else
			enable_clock(MT_CG_DRSC0_SMI_COMMON, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			/* enable_clock(MT_CG_IMAGE_FD, "CAMERA"); */
			enable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
#endif	/* #if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */
			break;
		default:
			break;
		}
		spin_lock(&(RSCInfo.SpinLockRSC));
		g_u4EnableClockCount++;
		spin_unlock(&(RSCInfo.SpinLockRSC));
	} else {		/* Disable clock. */

		/* LOG_DBG("Dpe clock disabled. g_u4EnableClockCount: %d.",
		 * g_u4EnableClockCount);
		 */
		spin_lock(&(RSCInfo.SpinLockRSC));
		g_u4EnableClockCount--;
		spin_unlock(&(RSCInfo.SpinLockRSC));
		switch (g_u4EnableClockCount) {
		case 0:
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifndef EP_NO_CLKMGR
			RSC_Disable_Unprepare_ccf_clock();
#else
			/* Disable clock by hardcode:
			 *  1. CAMSYS_CG_SET (0x1A000004) = 0xffffffff;
			 *  2. IMG_CG_SET (0x15000004) = 0xffffffff;
			 */
			setReg = 0xFFFFFFFF;
			RSC_WR32(IPESYS_REG_CG_SET, setReg);
#endif
#else
			/* do disable clock */
			disable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			disable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			disable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			/* disable_clock(MT_CG_IMAGE_FD, "CAMERA"); */
			disable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
			disable_clock(MT_CG_DRSC0_SMI_COMMON, "CAMERA");
#endif
			break;
		default:
			break;
		}
	}
}

/*******************************************************************************
 *
 ******************************************************************************/
static inline void RSC_Reset(void)
{
	LOG_DBG("- E.");

	LOG_DBG(" RSC Reset start!\n");
	spin_lock(&(RSCInfo.SpinLockRSCRef));

	if (RSCInfo.UserCount > 1) {
		spin_unlock(&(RSCInfo.SpinLockRSCRef));
		LOG_DBG("Curr UserCount(%d) users exist", RSCInfo.UserCount);
	} else {
		spin_unlock(&(RSCInfo.SpinLockRSCRef));

		/* Reset RSC flow */
		RSC_WR32(RSC_RST_REG, 0x1);
		while ((RSC_RD32(RSC_RST_REG) & 0x02) != 0x2)
			LOG_DBG("RSC resetting...\n");

		RSC_WR32(RSC_RST_REG, 0x11);
		RSC_WR32(RSC_RST_REG, 0x10);
		RSC_WR32(RSC_RST_REG, 0x0);
		RSC_WR32(RSC_START_REG, 0);
		LOG_DBG(" RSC Reset end!\n");
	}

}

/*******************************************************************************
 *
 ******************************************************************************/
static signed int RSC_ReadReg(struct RSC_REG_IO_STRUCT *pRegIo)
{
	unsigned int i;
	signed int Ret = 0;
	/*  */
	struct RSC_REG_STRUCT reg;
	/* unsigned int* pData = (unsigned int*)pRegIo->Data; */
	struct RSC_REG_STRUCT *pData = (struct RSC_REG_STRUCT *) pRegIo->pData;

	if ((pRegIo->pData == NULL) || (pRegIo->Count == 0) ||
		(pRegIo->Count > (RSC_REG_RANGE>>2))) {
		LOG_ERR(
			"ERROR: pRegIo->pData is NULL or Count error:%d\n",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}

	for (i = 0; i < pRegIo->Count; i++) {
		if (get_user(reg.Addr, (unsigned int *) &pData->Addr) != 0) {
			LOG_ERR("get_user failed");
			Ret = -EFAULT;
			goto EXIT;
		}
		/* pData++; */
		/*  */
		if ((ISP_RSC_BASE + reg.Addr >= ISP_RSC_BASE)
		    && (ISP_RSC_BASE + reg.Addr <
						(ISP_RSC_BASE + RSC_REG_RANGE))
			&& ((reg.Addr & 0x3) == 0)) {
			reg.Val = RSC_RD32(ISP_RSC_BASE + reg.Addr);
		} else {
			LOG_DBG(
			"Wrong address(0x%p)", (ISP_RSC_BASE + reg.Addr));
			reg.Val = 0;
		}
		/*  */

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


/*******************************************************************************
 *
 ******************************************************************************/
static signed int RSC_WriteRegToHw(struct RSC_REG_STRUCT *pReg,
							unsigned int Count)
{
	signed int Ret = 0;
	unsigned int i;
	bool dbgWriteReg;

	spin_lock(&(RSCInfo.SpinLockRSC));
	dbgWriteReg = RSCInfo.DebugMask & RSC_DBG_WRITE_REG;
	spin_unlock(&(RSCInfo.SpinLockRSC));

	/*  */
	if (dbgWriteReg)
		LOG_DBG("- E.\n");

	/*  */
	for (i = 0; i < Count; i++) {
		if (dbgWriteReg) {
			LOG_DBG("Addr(0x%lx), Val(0x%x)\n",
				(unsigned long)(ISP_RSC_BASE + pReg[i].Addr),
				(unsigned int) (pReg[i].Val));
		}

		if (((ISP_RSC_BASE + pReg[i].Addr) <
						(ISP_RSC_BASE + RSC_REG_RANGE))
			&& ((pReg[i].Addr & 0x3) == 0)) {
			RSC_WR32(ISP_RSC_BASE + pReg[i].Addr, pReg[i].Val);
		} else {
			LOG_DBG("wrong address(0x%lx)\n",
				(unsigned long)(ISP_RSC_BASE + pReg[i].Addr));
		}
	}

	/*  */
	return Ret;
}



/*******************************************************************************
 *
 ******************************************************************************/
static signed int RSC_WriteReg(struct RSC_REG_IO_STRUCT *pRegIo)
{
	signed int Ret = 0;
	/*
	 *  signed int TimeVd = 0;
	 *  signed int TimeExpdone = 0;
	 *  signed int TimeTasklet = 0;
	 */
	/* unsigned char* pData = NULL; */
	struct RSC_REG_STRUCT *pData = NULL;
	/*  */
	if (RSCInfo.DebugMask & RSC_DBG_WRITE_REG)
		LOG_DBG("Data(0x%p), Count(%d)\n", (pRegIo->pData),
							(pRegIo->Count));

	pData = kmalloc((pRegIo->Count) * sizeof(struct RSC_REG_STRUCT),
								GFP_ATOMIC);
	if (pData == NULL) {
		LOG_DBG(
		"ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)\n",
				current->comm, current->pid, current->tgid);
		Ret = -ENOMEM;
		goto EXIT;
	}
	/*  */
	if ((pRegIo->pData == NULL) || (pRegIo->Count == 0) ||
		(pRegIo->Count > (RSC_REG_RANGE>>2))) {
		LOG_ERR(
			"ERROR: pRegIo->pData is NULL or Count error:%d\n",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	if (copy_from_user
	    (pData, (void __user *)(pRegIo->pData),
			pRegIo->Count * sizeof(struct RSC_REG_STRUCT)) != 0) {
		LOG_ERR("copy_from_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	Ret = RSC_WriteRegToHw(pData, pRegIo->Count);
	/*  */
EXIT:
	if (pData != NULL) {
		kfree(pData);
		pData = NULL;
	}
	return Ret;
}


/*******************************************************************************
 *
 ******************************************************************************/
static signed int RSC_WaitIrq(struct RSC_WAIT_IRQ_STRUCT *WaitIrq)
{

	signed int Ret = 0;
	signed int Timeout = WaitIrq->Timeout;
	enum RSC_PROCESS_ID_ENUM whichReq = RSC_PROCESS_ID_NONE;

	/*unsigned int i;*/
	unsigned long flags; /* old: unsigned int flags;*/
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
	if (RSCInfo.DebugMask & RSC_DBG_INT) {
		if (WaitIrq->Status & RSCInfo.IrqInfo.Mask[WaitIrq->Type]) {
			if (WaitIrq->UserKey > 0) {
				LOG_DBG(
				"+WaitIrq clr(%d), Type(%d), Stat(0x%08X), Timeout(%d),usr(%d), ProcID(%d)\n",
				WaitIrq->Clear, WaitIrq->Type, WaitIrq->Status,
				WaitIrq->Timeout, WaitIrq->UserKey,
				WaitIrq->ProcessID);
			}
		}
	}


	/* 1. wait type update */
	if (WaitIrq->Clear == RSC_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(&(RSCInfo.SpinLockIrq[WaitIrq->Type]), flags);
		/* LOG_DBG(
		 * "WARNING: Clear(%d), Type(%d): IrqStatus(0x%08X) has been
		 * cleared" ,WaitIrq->EventInfo.Clear,WaitIrq->Type,
		 * RSCInfo.IrqInfo.Status[WaitIrq->Type]);
		 * RSCInfo.IrqInfo.Status[WaitIrq->Type][
		 * WaitIrq->EventInfo.UserKey] &= (~WaitIrq->EventInfo.Status);
		 */
		RSCInfo.IrqInfo.Status[WaitIrq->Type] &= (~WaitIrq->Status);
		spin_unlock_irqrestore(&(RSCInfo.SpinLockIrq[WaitIrq->Type]),
									flags);
		return Ret;
	}

	if (WaitIrq->Clear == RSC_IRQ_CLEAR_WAIT) {
		spin_lock_irqsave(&(RSCInfo.SpinLockIrq[WaitIrq->Type]), flags);
		if (RSCInfo.IrqInfo.Status[WaitIrq->Type] & WaitIrq->Status)
			RSCInfo.IrqInfo.Status[WaitIrq->Type] &=
							(~WaitIrq->Status);

		spin_unlock_irqrestore(&(RSCInfo.SpinLockIrq[WaitIrq->Type]),
									flags);
	} else if (WaitIrq->Clear == RSC_IRQ_CLEAR_ALL) {
		spin_lock_irqsave(&(RSCInfo.SpinLockIrq[WaitIrq->Type]), flags);

		RSCInfo.IrqInfo.Status[WaitIrq->Type] = 0;
		spin_unlock_irqrestore(&(RSCInfo.SpinLockIrq[WaitIrq->Type]),
									flags);
	}
	/* RSC_IRQ_WAIT_CLEAR ==> do nothing */


	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
	spin_lock_irqsave(&(RSCInfo.SpinLockIrq[WaitIrq->Type]), flags);
	irqStatus = RSCInfo.IrqInfo.Status[WaitIrq->Type];
	spin_unlock_irqrestore(&(RSCInfo.SpinLockIrq[WaitIrq->Type]), flags);

	if (WaitIrq->Status & RSC_INT_ST) {
		whichReq = RSC_PROCESS_ID_RSC;
	} else {
		LOG_ERR(
		"No Such Stats can be waited!! irq Type/User/Sts/Pid(0x%x/%d/0x%x/%d)\n",
			WaitIrq->Type, WaitIrq->UserKey, WaitIrq->Status,
			WaitIrq->ProcessID);
	}



	/* 2. start to wait signal */
	Timeout = wait_event_interruptible_timeout(RSCInfo.WaitQueueHead,
						   RSC_GetIRQState(
							WaitIrq->Type,
							WaitIrq->UserKey,
							WaitIrq->Status,
							whichReq,
							WaitIrq->ProcessID),
						   RSC_MsToJiffies(
							WaitIrq->Timeout));

	/* check if user is interrupted by system signal */
	if ((Timeout != 0) &&
		(!RSC_GetIRQState(WaitIrq->Type, WaitIrq->UserKey,
			WaitIrq->Status, whichReq, WaitIrq->ProcessID))) {
		LOG_ERR(
		"interrupted by system, timeout(%d),irq Type/User/Sts/whichReq/Pid(0x%x/%d/0x%x/%d/%d)\n",
		Timeout, WaitIrq->Type, WaitIrq->UserKey, WaitIrq->Status,
						whichReq, WaitIrq->ProcessID);
		Ret = -ERESTARTSYS;	/* actually it should be -ERESTARTSYS */
		goto EXIT;
	}
	/* timeout */
	if (Timeout == 0) {
		spin_lock_irqsave(&(RSCInfo.SpinLockIrq[WaitIrq->Type]),
									flags);
		irqStatus = RSCInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(&(RSCInfo.SpinLockIrq[WaitIrq->Type]),
									flags);

		LOG_ERR(
		"WaitIrq Timeout:Tout(%d) clr(%d) Type(%d) IrqStat(0x%08X) WaitStat(0x%08X) usrKey(%d)\n",
		     WaitIrq->Timeout, WaitIrq->Clear, WaitIrq->Type, irqStatus,
			WaitIrq->Status, WaitIrq->UserKey);
		LOG_ERR(
		"WaitIrq Timeout:whichReq(%d),ProcID(%d) RscIrqCnt(0x%08X) WriteReq(0x%08X) ReadReq(0x%08X)\n",
		     whichReq, WaitIrq->ProcessID, RSCInfo.IrqInfo.RscIrqCnt,
			RSCInfo.WriteReqIdx, RSCInfo.ReadReqIdx);

		if (WaitIrq->bDumpReg) {
			RSC_DumpReg();
			request_dump(&rsc_reqs);
		}

		Ret = -EFAULT;
		goto EXIT;
	} else {
#ifdef __RSC_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("%s", __func__);
#endif

		spin_lock_irqsave(&(RSCInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = RSCInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(&(RSCInfo.SpinLockIrq[WaitIrq->Type]),
									flags);

		if (WaitIrq->Clear == RSC_IRQ_WAIT_CLEAR) {
			spin_lock_irqsave(&(RSCInfo.SpinLockIrq[WaitIrq->Type]),
									flags);

			if (WaitIrq->Status & RSC_INT_ST) {
				RSCInfo.IrqInfo.RscIrqCnt--;
				if (RSCInfo.IrqInfo.RscIrqCnt == 0)
					RSCInfo.IrqInfo.Status[WaitIrq->Type] &=
							(~WaitIrq->Status);
			} else {
				LOG_ERR(
				"RSC_IRQ_WAIT_CLEAR Error, Type(%d), WaitStatus(0x%08X)",
					WaitIrq->Type, WaitIrq->Status);
			}
			spin_unlock_irqrestore(
				&(RSCInfo.SpinLockIrq[WaitIrq->Type]), flags);
		}


#ifdef __RSC_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif

	}


EXIT:


	return Ret;
}


/*******************************************************************************
 *
 ******************************************************************************/
static long RSC_ioctl(struct file *pFile, unsigned int Cmd, unsigned long Param)
{
	signed int Ret = 0;

	/*unsigned int pid = 0;*/
	struct RSC_REG_IO_STRUCT RegIo;
	struct RSC_WAIT_IRQ_STRUCT IrqInfo;
	struct RSC_CLEAR_IRQ_STRUCT ClearIrq;
	struct RSC_Request rsc_RscReq;
	signed int enqnum;
	struct RSC_USER_INFO_STRUCT *pUserInfo;
	int enqueNum;
	int dequeNum;
	unsigned long flags;
	unsigned int currentPPB = m_CurrentPPB;

	/* old: unsigned int flags;*//* FIX to avoid build warning */



	/*  */
	if (pFile->private_data == NULL) {
		LOG_WRN("private_data NULL,(process, pid, tgid)=(%s, %d, %d)",
			current->comm,
				current->pid, current->tgid);
		return -EFAULT;
	}
	/*  */
	pUserInfo = (struct RSC_USER_INFO_STRUCT *) (pFile->private_data);
	/*  */
	switch (Cmd) {
	case RSC_RESET:
		spin_lock(&(RSCInfo.SpinLockRSC));
		RSC_Reset();
		spin_unlock(&(RSCInfo.SpinLockRSC));
		break;
		/*  */
	case RSC_DUMP_REG:
		Ret = RSC_DumpReg();
		break;
	case RSC_DUMP_ISR_LOG:
		spin_lock_irqsave(
			&(RSCInfo.SpinLockIrq[RSC_IRQ_TYPE_INT_RSC_ST]),
								flags);
		m_CurrentPPB = (m_CurrentPPB + 1) % LOG_PPNUM;
		spin_unlock_irqrestore(
			&(RSCInfo.SpinLockIrq[RSC_IRQ_TYPE_INT_RSC_ST]),
								flags);

		IRQ_LOG_PRINTER(RSC_IRQ_TYPE_INT_RSC_ST, currentPPB,
							_LOG_INF);
		IRQ_LOG_PRINTER(RSC_IRQ_TYPE_INT_RSC_ST, currentPPB,
							_LOG_ERR);
		break;
	case RSC_READ_REGISTER:
		if (copy_from_user(&RegIo, (void *)Param,
			sizeof(struct RSC_REG_IO_STRUCT))) {
			LOG_ERR(
			"RSC_READ_REGISTER copy_from_user failed");
			Ret = -EFAULT;
			goto EXIT;
		}

		Ret = RSC_ReadReg(&RegIo);

		break;
	case RSC_WRITE_REGISTER:
		if (copy_from_user(&RegIo, (void *)Param,
			sizeof(struct RSC_REG_IO_STRUCT))) {
			LOG_ERR(
			"RSC_WRITE_REGISTER copy_from_user failed");
			Ret = -EFAULT;
			goto EXIT;
		}
		Ret = RSC_WriteReg(&RegIo);

		break;
	case RSC_WAIT_IRQ:
		if (copy_from_user(&IrqInfo, (void *)Param,
			sizeof(struct RSC_WAIT_IRQ_STRUCT))) {
			LOG_ERR("RSC_WAIT_IRQ copy_from_user failed");
			Ret = -EFAULT;
			goto EXIT;
		}
		/*  */
		if ((IrqInfo.Type >= RSC_IRQ_TYPE_AMOUNT) ||
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
		Ret = RSC_WaitIrq(&IrqInfo);

		if (copy_to_user((void *)Param, &IrqInfo,
		sizeof(struct RSC_WAIT_IRQ_STRUCT)) != 0) {
			LOG_ERR("copy_to_user failed\n");
			Ret = -EFAULT;
		}

		break;
	case RSC_CLEAR_IRQ:
		if (copy_from_user(&ClearIrq, (void *)Param,
			sizeof(struct RSC_CLEAR_IRQ_STRUCT))) {
			LOG_ERR(
			"RSC_CLEAR_IRQ copy_from_user failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}
		LOG_DBG("RSC_CLEAR_IRQ Type(%d)",
						ClearIrq.Type);

		if ((ClearIrq.Type >= RSC_IRQ_TYPE_AMOUNT) ||
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
		"RSC_CLEAR_IRQ:Type(%d),Status(0x%08X),IrqStatus(0x%08X)\n",
			ClearIrq.Type, ClearIrq.Status,
			RSCInfo.IrqInfo.Status[ClearIrq.Type]);
		spin_lock_irqsave(
		&(RSCInfo.SpinLockIrq[ClearIrq.Type]), flags);
		RSCInfo.IrqInfo.Status[ClearIrq.Type] &=
					(~ClearIrq.Status);
		spin_unlock_irqrestore(
		&(RSCInfo.SpinLockIrq[ClearIrq.Type]), flags);

		break;
	case RSC_ENQNUE_NUM:
		/* enqueNum */
		if (copy_from_user(&enqueNum, (void *)Param,
						sizeof(int))) {
			LOG_ERR(
			"RSC_EQNUE_NUM copy_from_user failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}
		if (RSC_REQUEST_STATE_EMPTY ==
		    g_RSC_ReqRing.RSCReq_Struct[
					g_RSC_ReqRing.WriteIdx].
		    State) {
			spin_lock_irqsave(
			&(RSCInfo.SpinLockIrq[
				RSC_IRQ_TYPE_INT_RSC_ST]),
							flags);
			g_RSC_ReqRing.RSCReq_Struct[
				g_RSC_ReqRing.WriteIdx].
					processID =
						pUserInfo->Pid;
			g_RSC_ReqRing.RSCReq_Struct[
				g_RSC_ReqRing.WriteIdx].
					enqueReqNum = enqueNum;
			spin_unlock_irqrestore(
			&(RSCInfo.SpinLockIrq[
			RSC_IRQ_TYPE_INT_RSC_ST]), flags);
			if (enqueNum >
			_SUPPORT_MAX_RSC_FRAME_REQUEST_) {
				LOG_ERR(
				"RSC Enque Num is bigger than enqueNum:%d\n",
				     enqueNum);
			}
			LOG_DBG(
			"RSC_ENQNUE_NUM:%d\n", enqueNum);
		} else {
			LOG_ERR(
			"WFME Enque request state is not empty:%d, writeIdx:%d, readIdx:%d\n",
			     g_RSC_ReqRing.RSCReq_Struct[
				g_RSC_ReqRing.WriteIdx].State,
				g_RSC_ReqRing.WriteIdx,
				g_RSC_ReqRing.ReadIdx);
		}

		break;
		/* struct RSC_Config */
	case RSC_ENQUE_REQ:
		if (copy_from_user(&rsc_RscReq, (void *)Param,
				sizeof(struct RSC_Request))) {
			LOG_ERR(
			"RSC_ENQUE_REQ copy_from_user failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}

		LOG_DBG("RSC_ENQNUE_NUM:%d, pid:%d\n",
			rsc_RscReq.m_ReqNum, pUserInfo->Pid);

		if (rsc_RscReq.m_ReqNum >
			_SUPPORT_MAX_RSC_FRAME_REQUEST_) {
			LOG_ERR(
			"RSC Enque Num is bigger than enqueNum:%d\n",
				rsc_RscReq.m_ReqNum);
			Ret = -EFAULT;
			goto EXIT;
		}
		if (copy_from_user
		    (g_RscEnqueReq_Struct.RscFrameConfig,
		     (void *)rsc_RscReq.m_pRscConfig,
		     rsc_RscReq.m_ReqNum *
			sizeof(struct RSC_Config)) != 0) {
			LOG_ERR(
			"copy RSCConfig from request fail!!\n");
			Ret = -EFAULT;
			goto EXIT;
		}

		mutex_lock(&gRscMutex);

		spin_lock_irqsave(
		&(RSCInfo.SpinLockIrq[RSC_IRQ_TYPE_INT_RSC_ST]),
				  flags);
		kRscReq.m_ReqNum = rsc_RscReq.m_ReqNum;
		kRscReq.m_pRscConfig =
			g_RscEnqueReq_Struct.RscFrameConfig;
		enqnum = enque_request(&rsc_reqs,
		kRscReq.m_ReqNum, &kRscReq, pUserInfo->Pid);

		spin_unlock_irqrestore(
		&(RSCInfo.SpinLockIrq[RSC_IRQ_TYPE_INT_RSC_ST]),
				       flags);
		LOG_DBG("Config RSC Request!!\n");

		/* Use a workqueue to set CMDQ to prevent
		 * HW CMDQ request consuming speed from being
		 * faster than SW frame-queue update speed.
		 */
		if (!request_running(&rsc_reqs)) {
			LOG_DBG("direct request_handler\n");
			request_handler(&rsc_reqs,
			&(RSCInfo.SpinLockIrq[
				RSC_IRQ_TYPE_INT_RSC_ST]));
		}
		mutex_unlock(&gRscMutex);

		break;
	case RSC_DEQUE_NUM:
		if (RSC_REQUEST_STATE_FINISHED ==
		    g_RSC_ReqRing.RSCReq_Struct[
			g_RSC_ReqRing.ReadIdx].State) {
			dequeNum =
			    g_RSC_ReqRing.RSCReq_Struct[
				g_RSC_ReqRing.ReadIdx].enqueReqNum;
			LOG_DBG("RSC_DEQUE_NUM(%d)\n", dequeNum);
		} else {
			dequeNum = 0;
			LOG_ERR(
			"DEQUE_NUM:No Buffer: ReadIdx(%d) State(%d) RrameRDIdx(%d) enqueReqNum(%d)\n",
			     g_RSC_ReqRing.ReadIdx,
			     g_RSC_ReqRing.RSCReq_Struct[
				g_RSC_ReqRing.ReadIdx].State,
			     g_RSC_ReqRing.RSCReq_Struct[
				g_RSC_ReqRing.ReadIdx].RrameRDIdx,
			     g_RSC_ReqRing.RSCReq_Struct[
				g_RSC_ReqRing.ReadIdx].enqueReqNum);
		}
		if (copy_to_user((void *)Param, &dequeNum,
					sizeof(unsigned int)) != 0) {
			LOG_ERR("RSC_DEQUE_NUM copy_to_user failed\n");
			Ret = -EFAULT;
		}

		break;
	case RSC_DEQUE_REQ:
		if (copy_from_user(&rsc_RscReq, (void *)Param,
				sizeof(struct RSC_Request))) {
			LOG_ERR("RSC_CMD_RSC_DEQUE_REQ failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}
		mutex_lock(&gRscDequeMutex);

		spin_lock_irqsave(
		&(RSCInfo.SpinLockIrq[RSC_IRQ_TYPE_INT_RSC_ST]),
				  flags);
		kRscReq.m_pRscConfig =
			g_RscDequeReq_Struct.RscFrameConfig;
		deque_request(&rsc_reqs, &kRscReq.m_ReqNum,
						&kRscReq);
		dequeNum = kRscReq.m_ReqNum;
		rsc_RscReq.m_ReqNum = dequeNum;

		spin_unlock_irqrestore(
		&(RSCInfo.SpinLockIrq[RSC_IRQ_TYPE_INT_RSC_ST]),
				       flags);

		mutex_unlock(&gRscDequeMutex);
		if (rsc_RscReq.m_pRscConfig == NULL) {
			LOG_ERR("NULL ptr:RscReq.m_pRscConfig");
			Ret = -EFAULT;
			goto EXIT;
		}
		if (copy_to_user
		    ((void *)rsc_RscReq.m_pRscConfig,
		     &g_RscDequeReq_Struct.RscFrameConfig[0],
		     dequeNum *
			sizeof(struct RSC_Config)) != 0) {
			LOG_ERR(
			"RSC_DEQUE_REQ frmcfg failed\n");
			Ret = -EFAULT;
		}
		if (copy_to_user
		    ((void *)Param, &rsc_RscReq,
			sizeof(struct RSC_Request)) != 0) {
			LOG_ERR("RSC_DEQUE_REQ RscReq fail\n");
			Ret = -EFAULT;
		}

		break;
	case RSC_ENQUE:
	case RSC_DEQUE:
	default:
		LOG_ERR("Unknown Cmd(%d)", Cmd);
		LOG_ERR("Cmd(%d),Dir(%d),Typ(%d),Nr(%d),Size(%d)\n",
			Cmd, _IOC_DIR(Cmd),
			_IOC_TYPE(Cmd), _IOC_NR(Cmd), _IOC_SIZE(Cmd));
		Ret = -EPERM;
		break;
	}
	/*  */
EXIT:
	if (Ret != 0) {
		LOG_ERR("Fail Cmd(%d), Pid(%d), (proc, pid, tgid)=(%s, %d, %d)",
			Cmd,
			pUserInfo->Pid, current->comm, current->pid,
								current->tgid);
	}
	/*  */
	return Ret;
}

#ifdef CONFIG_COMPAT

/*******************************************************************************
 *
 ******************************************************************************/
static int compat_get_RSC_read_register_data(
			struct compat_RSC_REG_IO_STRUCT __user *data32,
					struct RSC_REG_IO_STRUCT __user *data)
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

static int compat_put_RSC_read_register_data(
			struct compat_RSC_REG_IO_STRUCT __user *data32,
					struct RSC_REG_IO_STRUCT __user *data)
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

static int compat_get_RSC_enque_req_data(
			struct compat_RSC_Request __user *data32,
					      struct RSC_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pRscConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pRscConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_RSC_enque_req_data(
			struct compat_RSC_Request __user *data32,
					      struct RSC_Request __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pDpeConfig); */
	/* err |= put_user(uptr, &data32->m_pDpeConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}


static int compat_get_RSC_deque_req_data(
			struct compat_RSC_Request __user *data32,
					      struct RSC_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pRscConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pRscConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_RSC_deque_req_data(
		struct compat_RSC_Request __user *data32,
					struct RSC_Request __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pDpeConfig); */
	/* err |= put_user(uptr, &data32->m_pDpeConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}

static long RSC_ioctl_compat(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	long ret;


	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		LOG_ERR("no f_op !!!\n");
		return -ENOTTY;
	}
	switch (cmd) {
	case COMPAT_RSC_READ_REGISTER:
		{
			struct compat_RSC_REG_IO_STRUCT __user *data32;
			struct RSC_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_RSC_read_register_data(data32, data);
			if (err) {
				LOG_INF("compat_get_read_register_data err.\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, RSC_READ_REGISTER,
						       (unsigned long)data);
			err = compat_put_RSC_read_register_data(data32, data);
			if (err) {
				LOG_INF("compat_put_read_register_data err.\n");
				return err;
			}
			return ret;
		}
	case COMPAT_RSC_WRITE_REGISTER:
		{
			struct compat_RSC_REG_IO_STRUCT __user *data32;
			struct RSC_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_RSC_read_register_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_RSC_WRITE_REGISTER error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, RSC_WRITE_REGISTER,
						       (unsigned long)data);
			return ret;
		}
	case COMPAT_RSC_ENQUE_REQ:
		{
			struct compat_RSC_Request __user *data32;
			struct RSC_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_RSC_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_RSC_ENQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, RSC_ENQUE_REQ,
						       (unsigned long)data);
			err = compat_put_RSC_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_RSC_ENQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_RSC_DEQUE_REQ:
		{
			struct compat_RSC_Request __user *data32;
			struct RSC_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_RSC_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_RSC_DEQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, RSC_DEQUE_REQ,
						       (unsigned long)data);
			err = compat_put_RSC_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_RSC_DEQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}

	case RSC_WAIT_IRQ:
	case RSC_CLEAR_IRQ:	/* structure (no pointer) */
	case RSC_ENQNUE_NUM:
	case RSC_ENQUE:
	case RSC_DEQUE_NUM:
	case RSC_DEQUE:
	case RSC_RESET:
	case RSC_DUMP_REG:
	case RSC_DUMP_ISR_LOG:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
		/* return RSC_ioctl(filep, cmd, arg); */
	}
}

#endif

/*******************************************************************************
 *
 ******************************************************************************/
static signed int RSC_open(struct inode *pInode, struct file *pFile)
{
	signed int Ret = 0;
	unsigned int i, j;
	/*int q = 0, p = 0;*/
	struct RSC_USER_INFO_STRUCT *pUserInfo;

	LOG_DBG("- E. UserCount: %d.", RSCInfo.UserCount);


	/*  */
	spin_lock(&(RSCInfo.SpinLockRSCRef));

	pFile->private_data = NULL;
	pFile->private_data = kmalloc(sizeof(struct RSC_USER_INFO_STRUCT),
								GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		LOG_DBG("kmalloc failed, (proc, pid, tgid)=(%s, %d, %d)",
								current->comm,
						current->pid, current->tgid);
		Ret = -ENOMEM;
	} else {
		pUserInfo = (struct RSC_USER_INFO_STRUCT *) pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}
	/*  */
	if (RSCInfo.UserCount > 0) {
		RSCInfo.UserCount++;
		spin_unlock(&(RSCInfo.SpinLockRSCRef));
		LOG_DBG("Cur Usr(%d), (proc, pid, tgid)=(%s, %d, %d), exist",
			RSCInfo.UserCount, current->comm, current->pid,
								current->tgid);
		goto EXIT;
	} else {
		RSCInfo.UserCount++;
		spin_unlock(&(RSCInfo.SpinLockRSCRef));
		LOG_DBG("Cur Usr(%d), (proc, pid, tgid)=(%s, %d, %d), 1st user",
			RSCInfo.UserCount, current->comm, current->pid,
								current->tgid);
	}

	/* do wait queue head init when re-enter in camera */
	/*  */
	for (i = 0; i < _SUPPORT_MAX_RSC_REQUEST_RING_SIZE_; i++) {
		/* RSC */
		g_RSC_ReqRing.RSCReq_Struct[i].processID = 0x0;
		g_RSC_ReqRing.RSCReq_Struct[i].callerID = 0x0;
		g_RSC_ReqRing.RSCReq_Struct[i].enqueReqNum = 0x0;
		/* g_RSC_ReqRing.RSCReq_Struct[i].enqueIdx = 0x0; */
		g_RSC_ReqRing.RSCReq_Struct[i].State = RSC_REQUEST_STATE_EMPTY;
		g_RSC_ReqRing.RSCReq_Struct[i].FrameWRIdx = 0x0;
		g_RSC_ReqRing.RSCReq_Struct[i].RrameRDIdx = 0x0;
		for (j = 0; j < _SUPPORT_MAX_RSC_FRAME_REQUEST_; j++) {
			g_RSC_ReqRing.RSCReq_Struct[i].RscFrameStatus[j] =
			    RSC_FRAME_STATUS_EMPTY;
		}

	}
	g_RSC_ReqRing.WriteIdx = 0x0;
	g_RSC_ReqRing.ReadIdx = 0x0;
	g_RSC_ReqRing.HWProcessIdx = 0x0;

	/* Enable clock */
	RSC_EnableClock(MTRUE);
	g_SuspendCnt = 0;
	LOG_INF("RSC open g_u4EnableClockCount: %d", g_u4EnableClockCount);
	/*  */

	for (i = 0; i < RSC_IRQ_TYPE_AMOUNT; i++)
		RSCInfo.IrqInfo.Status[i] = 0;

	for (i = 0; i < _SUPPORT_MAX_RSC_FRAME_REQUEST_; i++)
		RSCInfo.ProcessID[i] = 0;

	RSCInfo.WriteReqIdx = 0;
	RSCInfo.ReadReqIdx = 0;
	RSCInfo.IrqInfo.RscIrqCnt = 0;

/*#define KERNEL_LOG*/
#ifdef KERNEL_LOG
    /* In EP, Add RSC_DBG_WRITE_REG for debug. Should remove it after EP */
	RSCInfo.DebugMask = (RSC_DBG_INT | RSC_DBG_DBGLOG | RSC_DBG_WRITE_REG);
#endif
	/*  */
	register_requests(&rsc_reqs, sizeof(struct RSC_Config));
	set_engine_ops(&rsc_reqs, &rsc_ops);


EXIT:




	LOG_DBG("- X. Ret: %d. UserCount: %d.", Ret, RSCInfo.UserCount);
	return Ret;

}

/******************************************************************************
 *
 ******************************************************************************/
static signed int RSC_release(struct inode *pInode, struct file *pFile)
{
	struct RSC_USER_INFO_STRUCT *pUserInfo;
	/*unsigned int Reg;*/

	LOG_DBG("- E. UserCount: %d.", RSCInfo.UserCount);

	/*  */
	if (pFile->private_data != NULL) {
		pUserInfo =
			(struct  RSC_USER_INFO_STRUCT *) pFile->private_data;
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	/*  */
	spin_lock(&(RSCInfo.SpinLockRSCRef));
	RSCInfo.UserCount--;

	if (RSCInfo.UserCount > 0) {
		spin_unlock(&(RSCInfo.SpinLockRSCRef));
		LOG_DBG("Cur UsrCnt(%d), (proc, pid, tgid)=(%s, %d, %d), exist",
			RSCInfo.UserCount, current->comm, current->pid,
								current->tgid);
		goto EXIT;
	} else
		spin_unlock(&(RSCInfo.SpinLockRSCRef));
	/*  */
	LOG_INF("Curr UsrCnt(%d), (process, pid, tgid)=(%s, %d, %d), last user",
		RSCInfo.UserCount, current->comm, current->pid, current->tgid);


	/* Disable clock. */
	RSC_EnableClock(MFALSE);
	LOG_DBG("RSC release g_u4EnableClockCount: %d", g_u4EnableClockCount);

	/*  */
	unregister_requests(&rsc_reqs);


EXIT:


	LOG_DBG("- X. UserCount: %d.", RSCInfo.UserCount);
	return 0;
}


/*******************************************************************************
 *
 ******************************************************************************/
/*
static signed int RSC_mmap(struct file *pFile, struct vm_area_struct *pVma)
{
	unsigned long length = 0;
	unsigned int pfn = 0x0;

	length = pVma->vm_end - pVma->vm_start;
	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
	pfn = pVma->vm_pgoff << PAGE_SHIFT;

	LOG_INF(
		"mmap:vm_pgoff(0x%lx) pfn(0x%x) phy(0x%lx) vm_start(0x%lx) vm_end(0x%lx) length(0x%lx)",
		pVma->vm_pgoff, pfn, pVma->vm_pgoff << PAGE_SHIFT,
			pVma->vm_start, pVma->vm_end, length);

	switch (pfn) {
	case RSC_BASE_HW:
		if (length > RSC_REG_RANGE) {
			LOG_ERR("mmap err:mod:0x%x len(0x%lx),REG_RANGE(0x%x)!",
				pfn, length, RSC_REG_RANGE);
			return -EAGAIN;
		}
		break;
	default:
		LOG_ERR("Illegal starting HW addr for mmap!");
		return -EAGAIN;
	}
	if (remap_pfn_range
	    (pVma, pVma->vm_start, pVma->vm_pgoff,
					pVma->vm_end - pVma->vm_start,
							pVma->vm_page_prot)) {
		return -EAGAIN;
	}

	return 0;
}
*/
/*******************************************************************************
 *
 ******************************************************************************/

static dev_t RSCDevNo;
static struct cdev *pRSCCharDrv;
static struct class *pRSCClass;

static const struct file_operations RSCFileOper = {
	.owner = THIS_MODULE,
	.open = RSC_open,
	.release = RSC_release,
	/* .flush   = mt_RSC_flush, */
	/* .mmap = RSC_mmap, */
	.unlocked_ioctl = RSC_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = RSC_ioctl_compat,
#endif
};

/*******************************************************************************
 *
 ******************************************************************************/
static inline void RSC_UnregCharDev(void)
{
	LOG_DBG("- E.");
	/*  */
	/* Release char driver */
	if (pRSCCharDrv != NULL) {
		cdev_del(pRSCCharDrv);
		pRSCCharDrv = NULL;
	}
	/*  */
	unregister_chrdev_region(RSCDevNo, 1);
}

/*******************************************************************************
 *
 ******************************************************************************/
static inline signed int RSC_RegCharDev(void)
{
	signed int Ret = 0;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = alloc_chrdev_region(&RSCDevNo, 0, 1, RSC_DEV_NAME);
	if (Ret < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d", Ret);
		return Ret;
	}
	/* Allocate driver */
	pRSCCharDrv = cdev_alloc();
	if (pRSCCharDrv == NULL) {
		LOG_ERR("cdev_alloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pRSCCharDrv, &RSCFileOper);
	/*  */
	pRSCCharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pRSCCharDrv, RSCDevNo, 1);
	if (Ret < 0) {
		LOG_ERR("Attatch file operation failed, %d", Ret);
		goto EXIT;
	}
	/*  */
EXIT:
	if (Ret < 0)
		RSC_UnregCharDev();

	/*  */

	LOG_DBG("- X.");
	return Ret;
}

/*******************************************************************************
 *
 ******************************************************************************/
static signed int RSC_probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	/*struct resource *pRes = NULL;*/
	signed int i = 0;
	unsigned char n;
	unsigned int irq_info[3];
	struct device *dev = NULL;
	struct RSC_device *_rsc_dev;


#ifdef CONFIG_OF
	struct RSC_device *RSC_dev;
#endif

	LOG_INF("- E. RSC driver probe.\n");

	/* Check platform_device parameters */
#ifdef CONFIG_OF

	if (pDev == NULL) {
		dev_dbg(&pDev->dev, "pDev is NULL");
		return -ENXIO;
	}

	nr_RSC_devs += 1;
	_rsc_dev = krealloc(RSC_devs, sizeof(struct RSC_device) * nr_RSC_devs,
								GFP_KERNEL);
	if (!_rsc_dev) {
		dev_dbg(&pDev->dev, "Unable to allocate RSC_devs\n");
		return -ENOMEM;
	}
	RSC_devs = _rsc_dev;

	RSC_dev = &(RSC_devs[nr_RSC_devs - 1]);
	RSC_dev->dev = &pDev->dev;

	/* iomap registers */
	RSC_dev->regs = of_iomap(pDev->dev.of_node, 0);

	if (!RSC_dev->regs) {
		dev_dbg(&pDev->dev,
			"of_iomap fail, nr_RSC_devs=%d, devnode(%s).\n",
			nr_RSC_devs, pDev->dev.of_node->name);
		return -ENOMEM;
	}

	LOG_INF("nr_RSC_devs=%d, devnode(%s), map_addr=0x%lx\n", nr_RSC_devs,
		pDev->dev.of_node->name, (unsigned long)RSC_dev->regs);

	/* get IRQ ID and request IRQ */
	RSC_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);

	if (RSC_dev->irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array
		    (pDev->dev.of_node, "interrupts", irq_info,
						ARRAY_SIZE(irq_info))) {
			dev_dbg(&pDev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		for (i = 0; i < RSC_IRQ_TYPE_AMOUNT; i++) {
			if (strcmp(pDev->dev.of_node->name,
					RSC_IRQ_CB_TBL[i].device_name) == 0) {
				Ret =
				    request_irq(RSC_dev->irq,
						(irq_handler_t)
						RSC_IRQ_CB_TBL[i].isr_fp,
						irq_info[2],
						(const char *)RSC_IRQ_CB_TBL[i]
							.device_name, NULL);
				if (Ret) {
					dev_dbg(&pDev->dev,
						"nr_RSC_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
						nr_RSC_devs,
					pDev->dev.of_node->name, RSC_dev->irq,
						RSC_IRQ_CB_TBL[i].device_name);
					return Ret;
				}

				LOG_INF(
				"nr_RSC_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
					nr_RSC_devs, pDev->dev.of_node->name,
					RSC_dev->irq,
					RSC_IRQ_CB_TBL[i].device_name);
				break;
			}
		}

		if (i >= RSC_IRQ_TYPE_AMOUNT) {
			LOG_INF("No ISR:nr_RSC_devs=%d, devnode(%s), irq=%d\n",
				nr_RSC_devs, pDev->dev.of_node->name,
								RSC_dev->irq);
		}


	} else {
		LOG_INF("No IRQ!!: nr_RSC_devs=%d, devnode(%s), irq=%d\n",
			nr_RSC_devs,
			pDev->dev.of_node->name, RSC_dev->irq);
	}


#endif

	/* Only register char driver in the 1st time */
	if (nr_RSC_devs == 2) {

		/* Register char driver */
		Ret = RSC_RegCharDev();
		if (Ret) {
			dev_dbg(&pDev->dev, "register char failed");
			return Ret;
		}
#ifndef EP_NO_CLKMGR
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
		    /*CCF: Grab clock pointer (struct clk*) */
		rsc_clk.CG_IPESYS_RSC = devm_clk_get(&pDev->dev,
							"RSC_CLK_IPE_RSC");

		if (IS_ERR(rsc_clk.CG_IPESYS_RSC)) {
			LOG_ERR("cannot get CG_IPESYS_RSC clock\n");
			return PTR_ERR(rsc_clk.CG_IPESYS_RSC);
		}
#endif
#endif

		/* Create class register */
		pRSCClass = class_create(THIS_MODULE, "RSCdrv");
		if (IS_ERR(pRSCClass)) {
			Ret = PTR_ERR(pRSCClass);
			LOG_ERR("Unable to create class, err = %d", Ret);
			goto EXIT;
		}

		dev = device_create(pRSCClass, NULL, RSCDevNo, NULL,
								RSC_DEV_NAME);
		if (IS_ERR(dev)) {
			Ret = PTR_ERR(dev);
			dev_dbg(&pDev->dev, "create dev err: /dev/%s, err = %d",
				RSC_DEV_NAME, Ret);
			goto EXIT;
		}

		/* Init spinlocks */
		spin_lock_init(&(RSCInfo.SpinLockRSCRef));
		spin_lock_init(&(RSCInfo.SpinLockRSC));
		for (n = 0; n < RSC_IRQ_TYPE_AMOUNT; n++)
			spin_lock_init(&(RSCInfo.SpinLockIrq[n]));

		/*  */
		init_waitqueue_head(&RSCInfo.WaitQueueHead);
		INIT_WORK(&RSCInfo.ScheduleRscWork, RSC_ScheduleWork);
		RSCInfo.wkqueue = create_singlethread_workqueue("RSC-CMDQ-WQ");
		if (!RSCInfo.wkqueue)
			LOG_ERR("NULL RSC-CMDQ-WQ\n");


		INIT_WORK(&logWork, logPrint);
		for (i = 0; i < RSC_IRQ_TYPE_AMOUNT; i++)
			tasklet_init(RSC_tasklet[i].pRSC_tkt,
					RSC_tasklet[i].tkt_cb, 0);




		/* Init RSCInfo */
		spin_lock(&(RSCInfo.SpinLockRSCRef));
		RSCInfo.UserCount = 0;
		spin_unlock(&(RSCInfo.SpinLockRSCRef));
		/*  */
		RSCInfo.IrqInfo.Mask[RSC_IRQ_TYPE_INT_RSC_ST] = INT_ST_MASK_RSC;

#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
		pm_qos_add_request(&rsc_pm_qos_request,
			PM_QOS_MM_MEMORY_BANDWIDTH, PM_QOS_DEFAULT_VALUE);
		cmdqCoreRegisterTaskCycleCB(CMDQ_GROUP_RSC, cmdq_pm_qos_start,
							cmdq_pm_qos_stop);
#endif
		seqlock_init(&(rsc_reqs.seqlock));
	}

	g_RSC_PMState = 0;
EXIT:
	if (Ret < 0)
		RSC_UnregCharDev();


	LOG_INF("- X. RSC driver probe.");

	return Ret;
}

/*******************************************************************************
 * Called when the device is being detached from the driver
 ******************************************************************************/
static signed int RSC_remove(struct platform_device *pDev)
{
	/*struct resource *pRes;*/
	signed int IrqNum;
	int i;
	/*  */
	LOG_DBG("- E.");

	/* wait for unfinished works in the workqueue. */
	destroy_workqueue(RSCInfo.wkqueue);
	RSCInfo.wkqueue = NULL;

	/* unregister char driver. */
	RSC_UnregCharDev();

	/* Release IRQ */
	disable_irq(RSCInfo.IrqNum);
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);

	/* kill tasklet */
	for (i = 0; i < RSC_IRQ_TYPE_AMOUNT; i++)
		tasklet_kill(RSC_tasklet[i].pRSC_tkt);
	/*  */
	device_destroy(pRSCClass, RSCDevNo);
	/*  */
	class_destroy(pRSCClass);
	pRSCClass = NULL;
	/*  */

#if defined(RSC_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
	pm_qos_remove_request(&rsc_pm_qos_request);
#endif

	return 0;
}

/*******************************************************************************
 *
 ******************************************************************************/
static signed int bPass1_On_In_Resume_TG1;

static signed int RSC_suspend(struct platform_device *pDev, pm_message_t Mesg)
{
	/*signed int ret = 0;*/

	LOG_DBG("bPass1_On_In_Resume_TG1(%d)\n", bPass1_On_In_Resume_TG1);
	if (g_u4EnableClockCount > 0) {
		RSC_EnableClock(MFALSE);
		g_SuspendCnt++;
	}
	bPass1_On_In_Resume_TG1 = 0;
	if (g_RSC_PMState == 0) {
		LOG_INF("%s:g_u4EnableClockCount(%d) g_SuspendCnt(%d).\n",
			__func__, g_u4EnableClockCount, g_SuspendCnt);
		g_RSC_PMState = 1;
	}
	return 0;
}

/*******************************************************************************
 *
 ******************************************************************************/
static signed int RSC_resume(struct platform_device *pDev)
{
	LOG_DBG("bPass1_On_In_Resume_TG1(%d).\n", bPass1_On_In_Resume_TG1);
	if (g_SuspendCnt > 0) {
		RSC_EnableClock(MTRUE);
		g_SuspendCnt--;
	}
	if (g_RSC_PMState == 1) {
		LOG_INF("%s:g_u4EnableClockCount(%d) g_SuspendCnt(%d).\n",
			__func__, g_u4EnableClockCount, g_SuspendCnt);
		g_RSC_PMState = 0;
	}
	return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int RSC_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	LOG_DBG("calling %s()\n", __func__);

	return RSC_suspend(pdev, PMSG_SUSPEND);
}

int RSC_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	LOG_DBG("calling %s()\n", __func__);

	return RSC_resume(pdev);
}
#ifndef CONFIG_OF
/*extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);*/
/*extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);*/
#endif
int RSC_pm_restore_noirq(struct device *device)
{
	pr_debug("calling %s()\n", __func__);
#ifndef CONFIG_OF
/*	mt_irq_set_sens(RSC_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);*/
/*	mt_irq_set_polarity(RSC_IRQ_BIT_ID, MT_POLARITY_LOW);*/
#endif
	return 0;

}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define RSC_pm_suspend NULL
#define RSC_pm_resume  NULL
#define RSC_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_OF



static const struct of_device_id RSC_of_ids[] = {
	{.compatible = "mediatek,ipesyscq",},
	{.compatible = "mediatek,rsc",},
	{}
};
#endif

const struct dev_pm_ops RSC_pm_ops = {
	.suspend = RSC_pm_suspend,
	.resume = RSC_pm_resume,
	.freeze = RSC_pm_suspend,
	.thaw = RSC_pm_resume,
	.poweroff = RSC_pm_suspend,
	.restore = RSC_pm_resume,
	.restore_noirq = RSC_pm_restore_noirq,
};


/*******************************************************************************
 *
 ******************************************************************************/
static struct platform_driver RSCDriver = {
	.probe = RSC_probe,
	.remove = RSC_remove,
	.suspend = RSC_suspend,
	.resume = RSC_resume,
	.driver = {
		   .name = RSC_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = RSC_of_ids,
#endif
#ifdef CONFIG_PM
		   .pm = &RSC_pm_ops,
#endif
		}
};


static int rsc_dump_read(struct seq_file *m, void *v)
{
	int i, j;

	if (RSCInfo.UserCount <= 0)
		return 0;

	seq_puts(m, "\n============ rsc dump register============\n");
	seq_puts(m, "RSC Config Info\n");

	for (i = 0x2C; i < 0x8C; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(RSC_BASE_HW + i),
			   (unsigned int)RSC_RD32(ISP_RSC_BASE + i));
	}
	seq_puts(m, "RSC Debug Info\n");
	for (i = 0x120; i < 0x148; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(RSC_BASE_HW + i),
			   (unsigned int)RSC_RD32(ISP_RSC_BASE + i));
	}

	seq_puts(m, "RSC Config Info\n");
	for (i = 0x230; i < 0x2D8; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(RSC_BASE_HW + i),
			   (unsigned int)RSC_RD32(ISP_RSC_BASE + i));
	}
	seq_puts(m, "RSC Debug Info\n");
	for (i = 0x2F4; i < 0x30C; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(RSC_BASE_HW + i),
			   (unsigned int)RSC_RD32(ISP_RSC_BASE + i));
	}

	seq_puts(m, "\n");
	seq_printf(m, "Dpe Clock Count:%d\n", g_u4EnableClockCount);

	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(RSC_DMA_DBG_HW),
		   (unsigned int)RSC_RD32(RSC_DMA_DBG_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(RSC_DMA_REQ_STATUS_HW),
		   (unsigned int)RSC_RD32(RSC_DMA_REQ_STATUS_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(RSC_DMA_RDY_STATUS_HW),
		   (unsigned int)RSC_RD32(RSC_DMA_RDY_STATUS_REG));

	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(RSC_DMA_RDY_STATUS_HW),
		   (unsigned int)RSC_RD32(RSC_DMA_RDY_STATUS_REG));


	seq_printf(m, "RSC:HWProcessIdx:%d, WriteIdx:%d, ReadIdx:%d\n",
		   g_RSC_ReqRing.HWProcessIdx, g_RSC_ReqRing.WriteIdx,
		   g_RSC_ReqRing.ReadIdx);

	for (i = 0; i < _SUPPORT_MAX_RSC_REQUEST_RING_SIZE_; i++) {
		seq_printf(m,
			   "RSC:State:%d, processID:0x%08X, callerID:0x%08X, enqueReqNum:%d, FrameWRIdx:%d, RrameRDIdx:%d\n",
			   g_RSC_ReqRing.RSCReq_Struct[i].State,
			   g_RSC_ReqRing.RSCReq_Struct[i].processID,
			   g_RSC_ReqRing.RSCReq_Struct[i].callerID,
			   g_RSC_ReqRing.RSCReq_Struct[i].enqueReqNum,
			   g_RSC_ReqRing.RSCReq_Struct[i].FrameWRIdx,
			   g_RSC_ReqRing.RSCReq_Struct[i].RrameRDIdx);

		for (j = 0; j < _SUPPORT_MAX_RSC_FRAME_REQUEST_;) {
			seq_printf(m,
				   "RSC:FrmStat[%d]:%d, FrmStat[%d]:%d\n",
				   j,
			g_RSC_ReqRing.RSCReq_Struct[i].RscFrameStatus[j],
				j + 1,
			g_RSC_ReqRing.RSCReq_Struct[i].RscFrameStatus[j + 1]);
				j = j + 2;
		}
	}

	seq_puts(m, "\n============ rsc dump debug ============\n");

	return 0;
}


static int proc_rsc_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, rsc_dump_read, NULL);
}

static const struct file_operations rsc_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_rsc_dump_open,
	.read = seq_read,
};


static int rsc_reg_read(struct seq_file *m, void *v)
{
	unsigned int i;

	if (RSCInfo.UserCount <= 0)
		return 0;

	seq_puts(m, "======== read rsc register ========\n");

	for (i = 0x1C; i <= 0x308; i = i + 4) {
		seq_printf(m, "[0x%08X 0x%08X]\n",
				(unsigned int)(RSC_BASE_HW + i),
				(unsigned int)RSC_RD32(ISP_RSC_BASE + i));
	}

	seq_printf(m, "[0x%08X 0x%08X]\n", (unsigned int)(RSC_DMA_DBG_HW),
		   (unsigned int)RSC_RD32(RSC_DMA_DBG_REG));
	seq_printf(m, "[0x%08X 0x%08X]\n",
		(unsigned int)(RSC_DMA_REQ_STATUS_HW),
		   (unsigned int)RSC_RD32(RSC_DMA_REQ_STATUS_REG));
	seq_printf(m, "[0x%08X 0x%08X]\n", (unsigned int)(RSC_BASE_HW + 0x7FC),
		   (unsigned int)RSC_RD32(RSC_DMA_RDY_STATUS_REG));

	return 0;
}


static ssize_t rsc_reg_write(struct file *file, const char __user *buffer,
						size_t count, loff_t *data)
{
	char desc[128];
	int len = 0;
	/*char *pEnd;*/
	char addrSzBuf[24];
	char valSzBuf[24];
	char *pszTmp;
	int addr = 0, val = 0;

	if (RSCInfo.UserCount <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';
	addrSzBuf[23] = '\0';
	valSzBuf[23] = '\0';

	if (sscanf(desc, "%23s %23s", addrSzBuf, valSzBuf) == 2) {
		pszTmp = strstr(addrSzBuf, "0x");
		if (pszTmp == NULL) {
			/*if (1 != sscanf(addrSzBuf, "%d", &addr))*/
			LOG_ERR("hex address only:%s", addrSzBuf);

		} else {
			if (strlen(addrSzBuf) > 2) {
				if (sscanf(addrSzBuf + 2, "%x", &addr) != 1)
					LOG_ERR("error hex addr:%s", addrSzBuf);
			} else {
				LOG_INF("RSC Write Addr Error!!:%s", addrSzBuf);
			}
		}

		pszTmp = strstr(valSzBuf, "0x");
		if (pszTmp == NULL) {
			/*if (1 != sscanf(valSzBuf, "%d", &val))*/
			LOG_ERR("HEX address only :%s", valSzBuf);
		} else {
			if (strlen(valSzBuf) > 2) {
				if (sscanf(valSzBuf + 2, "%x", &val) != 1)
					LOG_ERR("error hex val:%s", valSzBuf);
			} else {
				LOG_INF("RSC Write Value Error!!:%s\n",
								valSzBuf);
			}
		}

		if ((addr >= RSC_BASE_HW) && (addr <= RSC_DMA_RDY_STATUS_HW)
			&& ((addr & 0x3) == 0)) {
			LOG_INF("Write Request - addr:0x%x, value:0x%x\n", addr,
									val);
			RSC_WR32((ISP_RSC_BASE + (addr - RSC_BASE_HW)), val);
		} else {
			LOG_INF
			    ("write add out-of-range addr:0x%x, value:0x%x\n",
			     addr, val);
		}

	} else if (sscanf(desc, "%23s", addrSzBuf) == 1) {
		pszTmp = strstr(addrSzBuf, "0x");
		if (pszTmp == NULL) {
			/*if (1 != sscanf(addrSzBuf, "%d", &addr))*/
			LOG_ERR("HEX address only :%s", addrSzBuf);
		} else {
			if (strlen(addrSzBuf) > 2) {
				if (sscanf(addrSzBuf + 2, "%x", &addr) != 1)
					LOG_ERR("hex addr err:%s", addrSzBuf);
			} else {
				LOG_INF("RSC Read Addr Error!!:%s", addrSzBuf);
			}
		}

		if ((addr >= RSC_BASE_HW) && (addr <= RSC_DMA_RDY_STATUS_HW)
			&& ((addr & 0x3) == 0)) {
			val = RSC_RD32((ISP_RSC_BASE + (addr - RSC_BASE_HW)));
			LOG_INF("Read Request - addr:0x%x,value:0x%x\n", addr,
									val);
		} else {
			LOG_INF
			    ("Read-Addr out-of-range!! addr:0x%x, value:0x%x\n",
			     addr, val);
		}

	}


	return count;
}

static int proc_rsc_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, rsc_reg_read, NULL);
}

static const struct file_operations rsc_reg_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_rsc_reg_open,
	.read = seq_read,
	.write = rsc_reg_write,
};


/*******************************************************************************
 *
 ******************************************************************************/

int32_t RSC_ClockOnCallback(uint64_t engineFlag)
{
	/* LOG_DBG("RSC_ClockOnCallback"); */
	/* LOG_DBG("+CmdqEn:%d", g_u4EnableClockCount); */
	/* RSC_EnableClock(MTRUE); */

	return 0;
}

int32_t RSC_DumpCallback(uint64_t engineFlag, int level)
{
	LOG_DBG("%s", __func__);

	RSC_DumpReg();
	request_dump(&rsc_reqs);

	return 0;
}

int32_t RSC_ResetCallback(uint64_t engineFlag)
{
	LOG_DBG("%s", __func__);
	RSC_Reset();

	return 0;
}

int32_t RSC_ClockOffCallback(uint64_t engineFlag)
{
	/* LOG_DBG("RSC_ClockOffCallback"); */
	/* RSC_EnableClock(MFALSE); */
	/* LOG_DBG("-CmdqEn:%d", g_u4EnableClockCount); */
	return 0;
}


static signed int __init RSC_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
	/* FIX-ME: linux-3.10 procfs API changed */
	/* use proc_create */
	struct proc_dir_entry *proc_entry;
	struct proc_dir_entry *isp_rsc_dir;


	int i;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = platform_driver_register(&RSCDriver);
	if (Ret < 0) {
		LOG_ERR("platform_driver_register fail");
		return Ret;
	}

	isp_rsc_dir = proc_mkdir("rsc", NULL);
	if (!isp_rsc_dir) {
		LOG_ERR("[%s]: fail to mkdir /proc/rsc\n", __func__);
		return 0;
	}


	proc_entry = proc_create("rsc_dump", 0444, isp_rsc_dir,
							&rsc_dump_proc_fops);

	proc_entry = proc_create("rsc_reg", 0644, isp_rsc_dir,
							&rsc_reg_proc_fops);


	/* isr log */
	if (PAGE_SIZE <
	    ((RSC_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
		((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM)) {
		i = 0;
		while (i <
		       ((RSC_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
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
		for (j = 0; j < RSC_IRQ_TYPE_AMOUNT; j++) {
			gSvLog[j]._str[i][_LOG_DBG] = (char *)tmp;
			tmp = (void *)((char *)tmp +
						(NORMAL_STR_LEN * DBG_PAGE));
			gSvLog[j]._str[i][_LOG_INF] = (char *)tmp;
			tmp = (void *)((char *)tmp +
						(NORMAL_STR_LEN * INF_PAGE));
			gSvLog[j]._str[i][_LOG_ERR] = (char *)tmp;
			tmp = (void *)((char *)tmp +
						(NORMAL_STR_LEN * ERR_PAGE));
		}
		/* tmp = (void*) ((unsigned int)tmp + NORMAL_STR_LEN); */
		tmp = (void *)((char *)tmp + NORMAL_STR_LEN);	/* overflow */
	}

#ifndef CMDQ_COMMON
	/* Cmdq */
	/* Register RSC callback */
	LOG_DBG("register rsc callback for CMDQ");
	cmdqCoreRegisterCB(CMDQ_GROUP_RSC,
			   RSC_ClockOnCallback,
			   RSC_DumpCallback, RSC_ResetCallback,
							RSC_ClockOffCallback);
#endif

	LOG_DBG("- X. Ret: %d.", Ret);
	return Ret;
}

/*******************************************************************************
 *
 ******************************************************************************/
static void __exit RSC_Exit(void)
{
	/*int i;*/

	LOG_DBG("- E.");
	/*  */
	platform_driver_unregister(&RSCDriver);
	/*  */
	/* Cmdq */
	/* Unregister RSC callback */
#ifndef CMDQ_COMMON
	cmdqCoreRegisterCB(CMDQ_GROUP_RSC, NULL, NULL, NULL, NULL);
#endif
	kfree(pLog_kmalloc);

	/*  */
}


/*******************************************************************************
 *
 ******************************************************************************/
void RSC_ScheduleWork(struct work_struct *data)
{
	if (RSC_DBG_DBGLOG & RSCInfo.DebugMask)
		LOG_DBG("- E.");

	request_handler(&rsc_reqs,
			&(RSCInfo.SpinLockIrq[RSC_IRQ_TYPE_INT_RSC_ST]));
	if (!request_running(&rsc_reqs))
		LOG_DBG("[%s]no more requests", __func__);
}


static irqreturn_t ISP_Irq_RSC(signed int Irq, void *DeviceId)
{
	unsigned int RscStatus;
	bool bResulst = MFALSE;
	pid_t ProcessID;

	RscStatus = RSC_RD32(RSC_INT_STATUS_REG);	/* RSC Status */

	spin_lock(&(RSCInfo.SpinLockIrq[RSC_IRQ_TYPE_INT_RSC_ST]));

	if (RSC_INT_ST == (RSC_INT_ST & RscStatus)) {
		/* Update the frame status. */
#ifdef __RSC_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("rsc_irq");
#endif


		if (update_request(&rsc_reqs, &ProcessID) == 0)
			bResulst = MTRUE;
		if (bResulst == MTRUE) {
			#if REQUEST_REGULATION == REQUEST_BASE_REGULATION
			/* schedule_work(&RSCInfo.ScheduleRscWork); */
			queue_work(RSCInfo.wkqueue, &RSCInfo.ScheduleRscWork);
			#endif
			RSCInfo.IrqInfo.Status[RSC_IRQ_TYPE_INT_RSC_ST] |=
								RSC_INT_ST;
			RSCInfo.IrqInfo.ProcessID[RSC_PROCESS_ID_RSC] =
								ProcessID;
			RSCInfo.IrqInfo.RscIrqCnt++;
			RSCInfo.ProcessID[RSCInfo.WriteReqIdx] = ProcessID;
			RSCInfo.WriteReqIdx =
			    (RSCInfo.WriteReqIdx + 1) %
						_SUPPORT_MAX_RSC_FRAME_REQUEST_;
		}
#ifdef __RSC_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
		/* Config the Next frame */
	}
	spin_unlock(&(RSCInfo.SpinLockIrq[RSC_IRQ_TYPE_INT_RSC_ST]));
	if (bResulst == MTRUE)
		wake_up_interruptible(&RSCInfo.WaitQueueHead);

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(
		RSC_IRQ_TYPE_INT_RSC_ST, m_CurrentPPB, _LOG_INF,
		"%s:%d, reg 0x%x : 0x%x, bResulst:%d, RscHWSta:0x%x, RscIrqCnt:0x%x, WriteReqIdx:0x%x, ReadReqIdx:0x%x\n",
		       __func__, Irq, RSC_INT_STATUS_HW, RscStatus, bResulst,
			RscStatus, RSCInfo.IrqInfo.RscIrqCnt,
		       RSCInfo.WriteReqIdx, RSCInfo.ReadReqIdx);
	/* IRQ_LOG_KEEPER(RSC_IRQ_TYPE_INT_RSC_ST, m_CurrentPPB, _LOG_INF,
	 * "RscHWSta:0x%x, RscHWSta:0x%x, DpeDveSta0:0x%x\n",
	 * DveStatus, RscStatus, DpeDveSta0);
	 */
	#if (REQUEST_REGULATION == FRAME_BASE_REGULATION)
	/* schedule_work(&RSCInfo.ScheduleRscWork); */
	queue_work(RSCInfo.wkqueue, &RSCInfo.ScheduleRscWork);
	#endif

	if (RscStatus & RSC_INT_ST)
		schedule_work(&logWork);

	return IRQ_HANDLED;
}

static void ISP_TaskletFunc_RSC(unsigned long data)
{
	IRQ_LOG_PRINTER(RSC_IRQ_TYPE_INT_RSC_ST, m_CurrentPPB, _LOG_DBG);
	IRQ_LOG_PRINTER(RSC_IRQ_TYPE_INT_RSC_ST, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(RSC_IRQ_TYPE_INT_RSC_ST, m_CurrentPPB, _LOG_ERR);

}

static void logPrint(struct work_struct *data)
{
	unsigned long arg = 0;

	ISP_TaskletFunc_RSC(arg);
}

/******************************************************************************
 *
 ******************************************************************************/
module_init(RSC_Init);
module_exit(RSC_Exit);
MODULE_DESCRIPTION("Camera RSC driver");
MODULE_AUTHOR("MM3SW5");
MODULE_LICENSE("GPL");
