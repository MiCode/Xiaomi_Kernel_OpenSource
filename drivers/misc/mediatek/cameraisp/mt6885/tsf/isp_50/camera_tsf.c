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
#include "smi_public.h"

/*#include <linux/xlog.h>		 For xlog_printk(). */
/*  */
/*#include <mach/hardware.h>*/
/* #include <mach/mt6593_pll.h> */
#include "camera_tsf.h"
/* #include <mach/irqs.h> */
/* #include <mach/mt_reg_base.h> */
/* #if defined(CONFIG_MTK_LEGACY) */
/* #include <mach/mt_clkmgr.h> */
/* #endif */
#include <mt-plat/sync_write.h>	/* For mt65xx_reg_sync_writel(). */
/* #include <mach/mt_spm_idle.h>  For spm_enable_sodi()/spm_disable_sodi(). */

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <m4u.h>
#include <cmdq_core.h>
#include <cmdq_record.h>

/** Measure the kernel performance
 * #define __TSF_KERNEL_PERFORMANCE_MEASURE__
 */
#ifdef __TSF_KERNEL_PERFORMANCE_MEASURE__
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

#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#ifdef CONFIG_PM_WAKELOCKS
struct wakeup_source tsf_wake_lock;
#else
struct wake_lock tsf_wake_lock;
#endif

/*  */

/* TSF Command Queue */
/* #include "../../cmdq/mt6797/cmdq_record.h" */
/* #include "../../cmdq/mt6797/cmdq_core.h" */

/* CCF */
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#include <linux/clk.h>
struct TSF_CLK_STRUCT {
	struct clk *CG_IMGSYS_TSF;
};
struct TSF_CLK_STRUCT TSF_clk;
#endif	/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */

/*  */
#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif

#define TSF_DEV_NAME                "camera-tsf"

/* #define TSF_WAITIRQ_LOG  */
#define TSF_USE_GCE
/* #define TSF_DEBUG_USE */
/* #define TSF_MULTIPROCESS_TIMEING_ISSUE  */
/*I can' test the situation in FPGA, because the velocity of FPGA is so slow. */
#define MyTag "[TSF]"
#define IRQTag "KEEPER"

#define LOG_VRB(format,	args...)    pr_debug(MyTag format, ##args)

#ifdef TSF_DEBUG_USE
#define LOG_DBG(format, args...)    pr_info(MyTag format, ##args)
#else
#define LOG_DBG(format, args...)
#endif
#define LOG_ERR(format, args...)    pr_info(MyTag format,  ##args)
#define LOG_INF(format, args...)    pr_info(MyTag format,  ##args)
#define LOG_NOTICE(format, args...) pr_notice(MyTag format,  ##args)


/******************************************************************************
 *
 ******************************************************************************/
/* #define TSF_WR32(addr, data)  iowrite32(data, addr) // For other projects. */
#define TSF_WR32(addr, data)    mt_reg_sync_writel(data, addr)
#define TSF_RD32(addr)          ioread32(addr)
/******************************************************************************
 *
 ******************************************************************************/
/* dynamic log level */
#define TSF_DBG_DBGLOG              (0x00000001)
#define TSF_DBG_INFLOG              (0x00000002)
#define TSF_DBG_INT                 (0x00000004)
#define TSF_DBG_READ_REG            (0x00000008)
#define TSF_DBG_WRITE_REG           (0x00000010)
#define TSF_DBG_TASKLET             (0x00000020)


/* ///////////////////////////////////////////////////////////////// */

/******************************************************************************
 *
 ******************************************************************************/




/**
 * IRQ signal mask
 */

#define INT_ST_MASK_TSF     ( \
			TSF_INT_ST)


/* static irqreturn_t TSF_Irq_CAM_A(signed int  Irq,void *DeviceId); */
static irqreturn_t ISP_Irq_TSF(signed int Irq, void *DeviceId);

static void TSF_ScheduleWork(struct work_struct *data);

typedef irqreturn_t(*IRQ_CB) (signed int, void *);

struct ISR_TABLE {
	IRQ_CB isr_fp;
	unsigned int int_number;
	char device_name[16];
};

#ifndef CONFIG_OF
const struct ISR_TABLE TSF_IRQ_CB_TBL[TSF_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_TSF, TSF_IRQ_BIT_ID, "tsf"},
};

#else
/* int number is got from kernel api */
const struct ISR_TABLE TSF_IRQ_CB_TBL[TSF_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_TSF, 0, "tsf"},
};

#endif
/* ////////////////////////////////////////////////////////////////////////// */
/*  */
typedef void (*tasklet_cb) (unsigned long);
struct Tasklet_table {
	tasklet_cb tkt_cb;
	struct tasklet_struct *pTSF_tkt;
};

struct tasklet_struct TSFtkt[TSF_IRQ_TYPE_AMOUNT];

static void ISP_TaskletFunc_TSF(unsigned long data);

static struct Tasklet_table TSF_tasklet[TSF_IRQ_TYPE_AMOUNT] = {
	{ISP_TaskletFunc_TSF, &TSFtkt[TSF_IRQ_TYPE_INT_TSF_ST]},
};

static DEFINE_MUTEX(gTSFDveMutex);
static DEFINE_MUTEX(gTSFDveDequeMutex);

static DEFINE_MUTEX(gTSFWmfeMutex);
static DEFINE_MUTEX(gTSFWmfeDequeMutex);

#ifdef CONFIG_OF

struct TSF_device {
	void __iomem *regs;
	struct device *dev;
	int irq;
};

static struct TSF_device *TSF_devs;
static int nr_TSF_devs;

/* Get HW modules' base address from device nodes */
#define TSF_DEV_NODE_IDX 0

/* static unsigned long gISPSYS_Reg[TSF_IRQ_TYPE_AMOUNT]; */


#define ISP_TSF_BASE                  (TSF_devs[TSF_DEV_NODE_IDX].regs)
/* #define ISP_TSF_BASE                  (gISPSYS_Reg[TSF_DEV_NODE_IDX]) */

#else
#define ISP_TSF_BASE                        (IMGSYS_BASE + 0x2800)

#endif


static unsigned int g_u4EnableClockCount;
static unsigned int g_u4TsfCnt;

/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32


/******************************************************************************
 *
 ******************************************************************************/
struct TSF_USER_INFO_STRUCT {
	pid_t Pid;
	pid_t Tid;
};


/******************************************************************************
 *
 ******************************************************************************/
struct TSF_IRQ_INFO_STRUCT {
	unsigned int Status[TSF_IRQ_TYPE_AMOUNT];
	unsigned int Mask[TSF_IRQ_TYPE_AMOUNT];
};


struct TSF_INFO_STRUCT {
	spinlock_t SpinLockTSFRef;
	spinlock_t SpinLockTSF;
	spinlock_t SpinLockIrq[TSF_IRQ_TYPE_AMOUNT];
	wait_queue_head_t WaitQueueHead;
	struct work_struct ScheduleTsfWork;
	unsigned int UserCount;	/* User Count */
	unsigned int DebugMask;	/* Debug Mask */
	signed int IrqNum;
	struct TSF_IRQ_INFO_STRUCT IrqInfo;
};


static struct TSF_INFO_STRUCT TSFInfo;

enum _eLOG_TYPE {
	_LOG_DBG = 0,		/* currently, only used at ipl_buf_ctrl.
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
static struct SV_LOG_STR gSvLog[TSF_IRQ_TYPE_AMOUNT];

/**
 * for irq used,keep log until IRQ_LOG_PRINTER being involked,
 * limited:
 * each log must shorter than 512 bytes
 * total log length in each irq/logtype can't over 1024 bytes
 */
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
#else
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...) xlog_printk(ANDROID_LOG_DEBUG,\
"KEEPER", "[%s] " fmt, __func__, ##__VA_ARGS__)
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
				LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
			} else{\
				LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
				break;\
			} \
		} \
	} \
	else {\
		LOG_INF("N.S.%d", logT);\
	} \
		ptr[0] = '\0';\
		pSrc->_cnt[ppb][logT] = 0;\
	} \
} while (0)


#else
#define IRQ_LOG_PRINTER(irq, ppb, logT)
#endif


/* TSF registers */
#define TSF_START_HW                  (TSF_BASE_HW + 0xC00)
#define TSF_INT_EN_HW                 (TSF_BASE_HW + 0xC04)
#define TSF_INT_HW                    (TSF_BASE_HW + 0xC08)
#define TSF_CTRL_HW                   (TSF_BASE_HW + 0xC0C)

#define TSF_COEFF_1_HW                (TSF_BASE_HW + 0xC10)
#define TSF_COEFF_2_HW                (TSF_BASE_HW + 0xC14)
#define TSF_COEFF_3_HW                (TSF_BASE_HW + 0xC18)
#define TSF_COEFF_4_HW                (TSF_BASE_HW + 0xC1C)

#define TSF_CRYPTION_HW               (TSF_BASE_HW + 0xC20)
#define TSF_DEBUG_INFO_1_HW           (TSF_BASE_HW + 0xC24)
#define TSF_DEBUG_INFO_2_HW           (TSF_BASE_HW + 0xC28)
#define TSF_SPARE_CELL_HW             (TSF_BASE_HW + 0xC2C)


#define TSFO_BASE_ADDR_HW             (TSF_BASE_HW + 0x460)
#define TSFO_OFST_ADDR_HW             (TSF_BASE_HW + 0x468)
#define TSFO_XSIZE_HW                 (TSF_BASE_HW + 0x470)
#define TSFO_YSIZE_HW                 (TSF_BASE_HW + 0x474)
#define TSFO_STRIDE_HW                (TSF_BASE_HW + 0x478)
#define TSFO_CON_HW                   (TSF_BASE_HW + 0x47C)
#define TSFO_CON2_HW                  (TSF_BASE_HW + 0x480)
#define TSFO_CON3_HW                  (TSF_BASE_HW + 0x484)
#define TSFO_CROP_HW                  (TSF_BASE_HW + 0x488)

#define TSFI_BASE_ADDR_HW             (TSF_BASE_HW + 0x4C0)
#define TSFI_OFST_ADDR_HW             (TSF_BASE_HW + 0x4C8)
#define TSFI_XSIZE_HW                 (TSF_BASE_HW + 0x4D0)
#define TSFI_YSIZE_HW                 (TSF_BASE_HW + 0x4D4)
#define TSFI_STRIDE_HW                (TSF_BASE_HW + 0x4D8)
#define TSFI_CON_HW                   (TSF_BASE_HW + 0x4DC)
#define TSFI_CON2_HW                  (TSF_BASE_HW + 0x4E0)
#define TSFI_CON3_HW                  (TSF_BASE_HW + 0x4E4)


#define TSFO_ERR_STAT_HW              (TSF_BASE_HW + 0x528)
#define TSFI_ERR_STAT_HW              (TSF_BASE_HW + 0x530)
#define DMA_DEBUG_SEL_HW              (TSF_BASE_HW + 0x54C)

#define TSF_START_REG                 (ISP_TSF_BASE + 0xC00)
#define TSF_INT_EN_REG                (ISP_TSF_BASE + 0xC04)
#define TSF_INT_REG                   (ISP_TSF_BASE + 0xC08)
#define TSF_CTRL_REG                  (ISP_TSF_BASE + 0xC0C)

#define TSF_COEFF_1_REG               (ISP_TSF_BASE + 0xC10)
#define TSF_COEFF_2_REG               (ISP_TSF_BASE + 0xC14)
#define TSF_COEFF_3_REG               (ISP_TSF_BASE + 0xC18)
#define TSF_COEFF_4_REG               (ISP_TSF_BASE + 0xC1C)

#define TSF_CRYPTION_REG              (ISP_TSF_BASE + 0xC20)
#define TSF_DEBUG_INFO_1_REG          (ISP_TSF_BASE + 0xC24)
#define TSF_DEBUG_INFO_2_REG          (ISP_TSF_BASE + 0xC28)
#define TSF_SPARE_CELL_REG            (ISP_TSF_BASE + 0xC2C)


#define TSFO_BASE_ADDR_REG            (ISP_TSF_BASE + 0x460)
#define TSFO_OFST_ADDR_REG            (ISP_TSF_BASE + 0x468)
#define TSFO_XSIZE_REG                (ISP_TSF_BASE + 0x470)
#define TSFO_YSIZE_REG                (ISP_TSF_BASE + 0x474)
#define TSFO_STRIDE_REG               (ISP_TSF_BASE + 0x478)
#define TSFO_CON_REG                  (ISP_TSF_BASE + 0x47C)
#define TSFO_CON2_REG                 (ISP_TSF_BASE + 0x480)
#define TSFO_CON3_REG                 (ISP_TSF_BASE + 0x484)
#define TSFO_CROP_REG                 (ISP_TSF_BASE + 0x488)

#define TSFI_BASE_ADDR_REG            (ISP_TSF_BASE + 0x4C0)
#define TSFI_OFST_ADDR_REG            (ISP_TSF_BASE + 0x4C8)
#define TSFI_XSIZE_REG                (ISP_TSF_BASE + 0x4D0)
#define TSFI_YSIZE_REG                (ISP_TSF_BASE + 0x4D4)
#define TSFI_STRIDE_REG               (ISP_TSF_BASE + 0x4D8)
#define TSFI_CON_REG                  (ISP_TSF_BASE + 0x4DC)
#define TSFI_CON2_REG                 (ISP_TSF_BASE + 0x4E0)
#define TSFI_CON3_REG                 (ISP_TSF_BASE + 0x4E4)
#define TSFO_ERR_STAT_REG             (ISP_TSF_BASE + 0x528)
#define TSFI_ERR_STAT_REG             (ISP_TSF_BASE + 0x530)
#define DMA_DEBUG_SEL_REG             (ISP_TSF_BASE + 0x54C)


/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int TSF_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int TSF_UsToJiffies(unsigned int Us)
{
	return (((Us / 1000) * HZ + 512) >> 10);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int TSF_GetIRQState(
	unsigned int type, unsigned int userNumber,
	unsigned int stus, int ProcessID)
{
	unsigned int ret = 0;
	unsigned long flags;	/* old: unsigned int flags; */
				/* FIX to avoid build warning */

	/*  */
	spin_lock_irqsave(&(TSFInfo.SpinLockIrq[type]), flags);

	if (stus & TSF_INT_ST) {
		ret = (TSFInfo.IrqInfo.Status[type] & stus);
	} else {
		LOG_INF(
		    "WaitIRQ Status Error, type:%d, userNumber:%d, status:%d, ProcessID:0x%x\n",
		     type, userNumber, stus, ProcessID);
	}

	spin_unlock_irqrestore(&(TSFInfo.SpinLockIrq[type]), flags);
	/*  */
	return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int TSF_JiffiesToMs(unsigned int Jiffies)
{
	return ((Jiffies * 1000) / HZ);
}


#define RegDump(start, end) {\
	unsigned int i;\
	for (i = start; i <= end; i += 0x10) {\
		LOG_DBG(\
	    "[0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]",\
	    (unsigned int)(ISP_TSF_BASE + i),\
	    (unsigned int)TSF_RD32(ISP_TSF_BASE + i),\
	    (unsigned int)(ISP_TSF_BASE + i+0x4),\
	    (unsigned int)TSF_RD32(ISP_TSF_BASE + i+0x4),\
	    (unsigned int)(ISP_TSF_BASE + i+0x8),\
	    (unsigned int)TSF_RD32(ISP_TSF_BASE + i+0x8),\
	    (unsigned int)(ISP_TSF_BASE + i+0xc),\
	    (unsigned int)TSF_RD32(ISP_TSF_BASE + i+0xc));\
	} \
}


/*
 *
 */
static signed int TSF_DumpReg(void)
{
	signed int Ret = 0;

	/*  */
	LOG_INF("- E.");
	/*  */
	LOG_INF("TSF Debug Info\n");
	TSF_WR32(TSFO_ERR_STAT_REG, 0xFFFF0000);
	TSF_WR32(TSFI_ERR_STAT_REG, 0xFFFF0000);
	LOG_INF("TSFO_ERR_STAT_REG: [0x%08X %08X]\n",
		(unsigned int)(TSFO_ERR_STAT_HW),
		(unsigned int)TSF_RD32(TSFO_ERR_STAT_REG));
	LOG_INF("TSFI_ERR_STAT_REG: [0x%08X %08X]\n",
		(unsigned int)(TSFI_ERR_STAT_HW),
		(unsigned int)TSF_RD32(TSFI_ERR_STAT_REG));

	TSF_WR32(DMA_DEBUG_SEL_REG, 0x413);
	LOG_INF("0x413 [0x%08X %08X]\n", (unsigned int)(TSF_DEBUG_INFO_2_HW),
		(unsigned int)TSF_RD32(TSF_DEBUG_INFO_2_REG));
	TSF_WR32(DMA_DEBUG_SEL_REG, 0x513);
	LOG_INF("0x513 [0x%08X %08X]\n", (unsigned int)(TSF_DEBUG_INFO_2_HW),
		(unsigned int)TSF_RD32(TSF_DEBUG_INFO_2_REG));
	TSF_WR32(DMA_DEBUG_SEL_REG, 0x613);
	LOG_INF("0x613 [0x%08X %08X]\n", (unsigned int)(TSF_DEBUG_INFO_2_HW),
		(unsigned int)TSF_RD32(TSF_DEBUG_INFO_2_REG));
	TSF_WR32(DMA_DEBUG_SEL_REG, 0x713);
	LOG_INF("0x713 [0x%08X %08X]\n", (unsigned int)(TSF_DEBUG_INFO_2_HW),
		(unsigned int)TSF_RD32(TSF_DEBUG_INFO_2_REG));

	TSF_WR32(DMA_DEBUG_SEL_REG, 0x1413);
	LOG_INF("0x1413 [0x%08X %08X]\n", (unsigned int)(TSF_DEBUG_INFO_2_HW),
		(unsigned int)TSF_RD32(TSF_DEBUG_INFO_2_REG));

	TSF_WR32(DMA_DEBUG_SEL_REG, 0x1513);
	LOG_INF("0x1513 [0x%08X %08X]\n", (unsigned int)(TSF_DEBUG_INFO_2_HW),
		(unsigned int)TSF_RD32(TSF_DEBUG_INFO_2_REG));

	TSF_WR32(DMA_DEBUG_SEL_REG, 0x1613);
	LOG_INF("0x1613 [0x%08X %08X]\n", (unsigned int)(TSF_DEBUG_INFO_2_HW),
		(unsigned int)TSF_RD32(TSF_DEBUG_INFO_2_REG));

	TSF_WR32(DMA_DEBUG_SEL_REG, 0x1713);
	LOG_INF("0x1713 [0x%08X %08X]\n", (unsigned int)(TSF_DEBUG_INFO_2_HW),
		(unsigned int)TSF_RD32(TSF_DEBUG_INFO_2_REG));
	LOG_INF("TSF Info\n");
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSF_START_HW),
		(unsigned int)TSF_RD32(TSF_START_REG));

	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSF_INT_EN_HW),
		(unsigned int)TSF_RD32(TSF_INT_EN_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSF_CTRL_HW),
		(unsigned int)TSF_RD32(TSF_CTRL_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSF_COEFF_1_HW),
		(unsigned int)TSF_RD32(TSF_COEFF_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSF_COEFF_2_HW),
		(unsigned int)TSF_RD32(TSF_COEFF_2_REG));

	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSF_COEFF_3_HW),
		(unsigned int)TSF_RD32(TSF_COEFF_3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSF_COEFF_4_HW),
		(unsigned int)TSF_RD32(TSF_COEFF_4_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSF_CRYPTION_HW),
		(unsigned int)TSF_RD32(TSF_CRYPTION_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSF_DEBUG_INFO_1_HW),
		(unsigned int)TSF_RD32(TSF_DEBUG_INFO_1_REG));

	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSF_DEBUG_INFO_2_HW),
		(unsigned int)TSF_RD32(TSF_DEBUG_INFO_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSF_SPARE_CELL_HW),
		(unsigned int)TSF_RD32(TSF_SPARE_CELL_REG));


	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFO_BASE_ADDR_HW),
		(unsigned int)TSF_RD32(TSFO_BASE_ADDR_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFO_OFST_ADDR_HW),
		(unsigned int)TSF_RD32(TSFO_OFST_ADDR_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFO_XSIZE_HW),
		(unsigned int)TSF_RD32(TSFO_XSIZE_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFO_YSIZE_HW),
		(unsigned int)TSF_RD32(TSFO_YSIZE_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFO_STRIDE_HW),
		(unsigned int)TSF_RD32(TSFO_STRIDE_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFO_CON_HW),
		(unsigned int)TSF_RD32(TSFO_CON_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFO_CON2_HW),
		(unsigned int)TSF_RD32(TSFO_CON2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFO_CON3_HW),
		(unsigned int)TSF_RD32(TSFO_CON3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFO_CROP_HW),
		(unsigned int)TSF_RD32(TSFO_CROP_REG));


	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFI_BASE_ADDR_HW),
		(unsigned int)TSF_RD32(TSFI_BASE_ADDR_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFI_OFST_ADDR_HW),
		(unsigned int)TSF_RD32(TSFI_OFST_ADDR_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFI_XSIZE_HW),
		(unsigned int)TSF_RD32(TSFI_XSIZE_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFI_YSIZE_HW),
		(unsigned int)TSF_RD32(TSFI_YSIZE_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFI_STRIDE_HW),
		(unsigned int)TSF_RD32(TSFI_STRIDE_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFI_CON_HW),
		(unsigned int)TSF_RD32(TSFI_CON_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFI_CON2_HW),
		(unsigned int)TSF_RD32(TSFI_CON2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(TSFI_CON3_HW),
		(unsigned int)TSF_RD32(TSFI_CON3_REG));


	LOG_INF("- X.");
	/*  */
	return Ret;
}

#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
static inline void TSF_Prepare_Enable_ccf_clock(void)
{
	int ret;
	/* must keep this clk open order:
	 * CG_SCP_SYS_DIS-> CG_MM_SMI_COMMON -> CG_SCP_SYS_ISP -> TSF clk
	 */
	smi_bus_prepare_enable(SMI_LARB5, TSF_DEV_NAME);
	ret = clk_prepare_enable(TSF_clk.CG_IMGSYS_TSF);
	if (ret)
		LOG_INF("cannot prepare and enable CG_IMGSYS_TSF clock\n");

}

static inline void TSF_Disable_Unprepare_ccf_clock(void)
{
	/* must keep this clk close order:
	 * TSF clk -> CG_SCP_SYS_ISP -> CG_MM_SMI_COMMON -> CG_SCP_SYS_DIS
	 */
	clk_disable_unprepare(TSF_clk.CG_IMGSYS_TSF);
	smi_bus_disable_unprepare(SMI_LARB5, TSF_DEV_NAME);
}
#endif

/******************************************************************************
 *
 ******************************************************************************/
static void TSF_EnableClock(bool En)
{
	if (En) {		/* Enable clock. */
		/* LOG_DBG("TSF clock enbled. g_u4EnableClockCount: %d.",*/
		/*	g_u4EnableClockCount); */
		switch (g_u4EnableClockCount) {
		case 0:
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
			    TSF_Prepare_Enable_ccf_clock();
#endif	/* #if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */
			break;
		default:
			break;
		}
		spin_lock(&(TSFInfo.SpinLockTSF));
		g_u4EnableClockCount++;
		spin_unlock(&(TSFInfo.SpinLockTSF));
	} else {		/* Disable clock. */
		/* LOG_DBG("TSF clock disabled. g_u4EnableClockCount: %d.", */
		/*	g_u4EnableClockCount); */
		spin_lock(&(TSFInfo.SpinLockTSF));
		g_u4EnableClockCount--;
		spin_unlock(&(TSFInfo.SpinLockTSF));
		switch (g_u4EnableClockCount) {
		case 0:
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
			    TSF_Disable_Unprepare_ccf_clock();
#endif	/* #if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) */
			break;
		default:
			break;
		}
	}
}

/******************************************************************************
 *
 ******************************************************************************/
static inline void TSF_Reset(void)
{
	LOG_DBG("- E.");

	LOG_DBG(" TSF Reset start!\n");
	spin_lock(&(TSFInfo.SpinLockTSFRef));

	if (TSFInfo.UserCount > 1) {
		spin_unlock(&(TSFInfo.SpinLockTSFRef));
		LOG_DBG("Curr UserCount(%d) users exist", TSFInfo.UserCount);
	} else {
		spin_unlock(&(TSFInfo.SpinLockTSFRef));

		/* Reset TSF flow */
		TSF_WR32(TSF_START_REG, 0x00010000);
		while (((TSF_RD32(TSF_START_REG) >> 24) & 0x01) != 0x0)
			LOG_DBG("TSF resetting...\n");

		TSF_WR32(TSF_START_REG, 0x00100000);
		TSF_WR32(TSF_START_REG, 0x00000000);
		LOG_DBG(" TSF Reset end!\n");
	}

}

/******************************************************************************
 *
 ******************************************************************************/
static signed int TSF_ReadReg(struct TSF_REG_IO_STRUCT *pRegIo)
{
	unsigned int i;
	signed int Ret = 0;
	/*  */
	/* unsigned int* pData = (unsigned int*)pRegIo->Data; */
	struct TSF_REG_STRUCT *pData = NULL, *pTmpData = NULL;

	if ((pRegIo->pData == NULL) || (pRegIo->Count == 0) ||
	    (pRegIo->Count > (TSF_REG_RANGE>>2))) {
		LOG_INF("%s pRegIo->pData is NULL, Count:%d!!",
			__func__, pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	pData = kmalloc(
		(pRegIo->Count) * sizeof(struct TSF_REG_STRUCT), GFP_KERNEL);
	if (pData == NULL) {
		LOG_INF("ERROR: %s kmalloc failed, cnt:%d\n",
			__func__, pRegIo->Count);
		Ret = -ENOMEM;
		goto EXIT;
	}
	pTmpData = pData;
	if (copy_from_user(pData, (void *)pRegIo->pData,
	    (pRegIo->Count) * sizeof(struct TSF_REG_STRUCT)) == 0) {
		for (i = 0; i < pRegIo->Count; i++) {
			if ((ISP_TSF_BASE + pData->Addr >= ISP_TSF_BASE) &&
			    (ISP_TSF_BASE + pData->Addr <
			    (ISP_TSF_BASE + TSF_REG_RANGE))) {
				pData->Val =
					TSF_RD32(ISP_TSF_BASE + pData->Addr);
			} else {
				LOG_INF("Wrong address(0x%p)",
					(ISP_TSF_BASE + pData->Addr));
				pData->Val = 0;
			}
			pData++;
		}
		pData = pTmpData;
		if (copy_to_user((void *)pRegIo->pData, pData,
		    (pRegIo->Count) * sizeof(struct TSF_REG_STRUCT)) != 0) {
			LOG_INF("copy_to_user failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}
	} else {
		LOG_INF("TSF_READ_REGISTER copy_from_user failed");
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
static signed int TSF_WriteRegToHw(
	struct TSF_REG_STRUCT *pReg, unsigned int Count)
{
	signed int Ret = 0;
	unsigned int i;
	bool dbgWriteReg;

	/* Use local variable to store TSFInfo.DebugMask & TSF_DBG_WRITE_REG
	 * for saving lock time
	 */
	spin_lock(&(TSFInfo.SpinLockTSF));
	dbgWriteReg = TSFInfo.DebugMask & TSF_DBG_WRITE_REG;
	spin_unlock(&(TSFInfo.SpinLockTSF));

	/*  */
	if (dbgWriteReg)
		LOG_DBG("- E.\n");

	/*  */
	for (i = 0; i < Count; i++) {
		if (dbgWriteReg) {
			LOG_DBG("Addr(0x%lx), Val(0x%x)\n",
				(unsigned long)(ISP_TSF_BASE + pReg[i].Addr),
				(unsigned int) (pReg[i].Val));
		}

		if (((ISP_TSF_BASE + pReg[i].Addr) <
			(ISP_TSF_BASE + TSF_REG_RANGE)) &&
			(pReg[i].Addr < TSF_REG_RANGE)) {
			TSF_WR32(ISP_TSF_BASE + pReg[i].Addr, pReg[i].Val);
		} else {
			LOG_INF("wrong tsf address(0x%lx)\n",
				(unsigned long)(ISP_TSF_BASE + pReg[i].Addr));
		}
	}

	/*  */
	return Ret;
}



/******************************************************************************
 *
 ******************************************************************************/
static signed int TSF_WriteReg(struct TSF_REG_IO_STRUCT *pRegIo)
{
	signed int Ret = 0;
	/**
	 **   signed int TimeVd = 0;
	 **   signed int TimeExpdone = 0;
	 **   signed int TimeTasklet = 0;
	 **/
	/* MUINT8* pData = NULL; */
	struct TSF_REG_STRUCT *pData = NULL;
	/*  */
	if (TSFInfo.DebugMask & TSF_DBG_WRITE_REG)
		LOG_DBG("Data(0x%p), Count(%d)\n",
			(pRegIo->pData), (pRegIo->Count));

	if ((pRegIo->pData == NULL) || (pRegIo->Count == 0) ||
	    (pRegIo->Count > (TSF_REG_RANGE>>2))) {
		LOG_INF(
		    "ERROR: pRegIo->pData is NULL or Count:%d\n",
		    pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	/* pData = (MUINT8*)kmalloc( */
	/*	(pRegIo->Count)*sizeof(TSF_REG_STRUCT), GFP_ATOMIC); */
	pData = kmalloc(
		(pRegIo->Count) * sizeof(struct TSF_REG_STRUCT), GFP_KERNEL);
	if (pData == NULL) {
		LOG_DBG(
		  "ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)\n",
		  current->comm,
		  current->pid,
		  current->tgid);
		Ret = -ENOMEM;
		goto EXIT;
	}
	/*  */
	if (copy_from_user(pData, (void __user *)(pRegIo->pData),
	    pRegIo->Count * sizeof(struct TSF_REG_STRUCT)) != 0) {
		LOG_INF("copy_from_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	Ret = TSF_WriteRegToHw(pData, pRegIo->Count);
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
static signed int TSF_WaitIrq(struct TSF_WAIT_IRQ_STRUCT *WaitIrq)
{

	signed int Ret = 0;
	signed int Timeout = WaitIrq->Timeout;

	/*unsigned int i; */
	unsigned long flags;	/* old: unsigned int flags; */
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
	if (TSFInfo.DebugMask & TSF_DBG_INT) {
		if (WaitIrq->Status & TSFInfo.IrqInfo.Mask[WaitIrq->Type]) {
			if (WaitIrq->UserKey > 0) {
				LOG_DBG("+WaitIrq Clr(%d),Type(%d)\n",
				    WaitIrq->Clear, WaitIrq->Type);
				LOG_DBG("+WaitIrq Status(0x%08X)\n",
				    WaitIrq->Status);
				LOG_DBG("+WaitIrq Timeout(%d),user(%d)\n",
				    WaitIrq->Timeout, WaitIrq->UserKey);
				LOG_DBG("+WaitIrq ProcessID(%d)\n",
				    WaitIrq->ProcessID);
			}
		}
	}


	/* 1. wait type update */
	if (WaitIrq->Clear == TSF_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);
		#if 0
		LOG_DBG(
		    "WARNING: Clear(%d), Type(%d): IrqStatus(0x%08X) has been cleared",
		    WaitIrq->EventInfo.Clear, WaitIrq->Type,
		    TSFInfo.IrqInfo.Status[WaitIrq->Type]);

		 TSFInfo.IrqInfo.Status[WaitIrq->Type][
			WaitIrq->EventInfo.UserKey] &=
			(~WaitIrq->EventInfo.Status);
		#endif

		TSFInfo.IrqInfo.Status[WaitIrq->Type] &= (~WaitIrq->Status);
		spin_unlock_irqrestore(
			&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);
		return Ret;
	}

	if (WaitIrq->Clear == TSF_IRQ_CLEAR_WAIT) {
		spin_lock_irqsave(&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);
		if (TSFInfo.IrqInfo.Status[WaitIrq->Type] & WaitIrq->Status)
			TSFInfo.IrqInfo.Status[
				WaitIrq->Type] &= (~WaitIrq->Status);

		spin_unlock_irqrestore(
			&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);
	} else if (WaitIrq->Clear == TSF_IRQ_CLEAR_ALL) {
		spin_lock_irqsave(&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);

		TSFInfo.IrqInfo.Status[WaitIrq->Type] = 0;
		spin_unlock_irqrestore(
			&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);
	}
	/* TSF_IRQ_WAIT_CLEAR ==> do nothing */


	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
	spin_lock_irqsave(&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);
	irqStatus = TSFInfo.IrqInfo.Status[WaitIrq->Type];
	spin_unlock_irqrestore(&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);

	if (!(WaitIrq->Status & TSF_INT_ST)) {
		LOG_INF(
		    "No Such Stats can be waited!! irq Type/User/Sts/Pid(0x%x/%d/0x%x/%d)\n",
		    WaitIrq->Type, WaitIrq->UserKey,
		    WaitIrq->Status, WaitIrq->ProcessID);
	}


#ifdef TSF_WAITIRQ_LOG
	LOG_INF("bef wait_event: Timeout(%d),Clr(%d),Type(%d)\n",
		WaitIrq->Timeout, WaitIrq->Clear, WaitIrq->Type);
	LOG_INF("bef wait_event: IrqStatus(0x%08X),WaitStatus(0x%08X)\n",
		irqStatus, WaitIrq->Status);
	LOG_INF("bef wait_event: userKey(%d), ProcessID(%d)\n",
		WaitIrq->UserKey, WaitIrq->ProcessID);

#endif

	/* 2. start to wait signal */
	Timeout = wait_event_interruptible_timeout(
			TSFInfo.WaitQueueHead,
			TSF_GetIRQState(WaitIrq->Type, WaitIrq->UserKey,
			WaitIrq->Status,
			WaitIrq->ProcessID),
			TSF_MsToJiffies(WaitIrq->Timeout));

	/* check if user is interrupted by system signal */
	if ((Timeout != 0) && (!TSF_GetIRQState(
	    WaitIrq->Type, WaitIrq->UserKey,
	    WaitIrq->Status, WaitIrq->ProcessID))) {
		LOG_DBG("interrupted by system signal,return value(%d)\n",
		    Timeout);
		LOG_DBG("irq Type/User(0x%x/%d)\n",
		    WaitIrq->Type, WaitIrq->UserKey);
		LOG_DBG("irq Sts/Pid(0x%x/%d)\n",
		    WaitIrq->Status, WaitIrq->ProcessID);
		Ret = -ERESTARTSYS;	/* actually it should be -ERESTARTSYS */
		goto EXIT;
	}
	/* timeout */
	if (Timeout == 0) {
		/* Store irqinfo status in here to redeuce time
		 * of spin_lock_irqsave
		 */
		spin_lock_irqsave(&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = TSFInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);

		LOG_INF("Timeout!!! ERRRR WaitIrq Timeout(%d) Clear(%d)\n",
			WaitIrq->Timeout, WaitIrq->Clear);

		LOG_INF("Type(%d), IrqStatus(0x%08X), WaitStatus(0x%08X)\n",
			WaitIrq->Type, irqStatus, WaitIrq->Status);

		LOG_INF("userKey(%d), ProcessID(%d)\n",
			WaitIrq->UserKey, WaitIrq->ProcessID);


		if (WaitIrq->bDumpReg)
			TSF_DumpReg();

		Ret = -EFAULT;
		goto EXIT;
	} else {
	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
#ifdef __TSF_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("TSF_WaitIrq");
#endif

		spin_lock_irqsave(
			&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = TSFInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);

		if (WaitIrq->Clear == TSF_IRQ_WAIT_CLEAR) {
			spin_lock_irqsave(
				&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);

			if (WaitIrq->Status & TSF_INT_ST) {
				TSFInfo.IrqInfo.Status[
					WaitIrq->Type] &= (~WaitIrq->Status);
			} else {
				LOG_INF(
				    "TSF_IRQ_WAIT_CLEAR Error, Type(%d), WaitStatus(0x%08X)",
				    WaitIrq->Type, WaitIrq->Status);
			}
			spin_unlock_irqrestore(
				&(TSFInfo.SpinLockIrq[WaitIrq->Type]), flags);
		}
#ifdef TSF_WAITIRQ_LOG
		LOG_INF("no Timeout!!!: WaitIrq Timeout(%d) Clear(%d)\n",
			WaitIrq->Timeout, WaitIrq->Clear);

		LOG_INF("Type(%d), IrqStatus(0x%08X), WaitStatus(0x%08X)\n",
			WaitIrq->Type, irqStatus, WaitIrq->Status);

		LOG_INF("userKey(%d),ProcessID(%d)",
			WaitIrq->UserKey, WaitIrq->ProcessID);

#endif

#ifdef __TSF_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif

	}


EXIT:


	return Ret;
}


/******************************************************************************
 *
 ******************************************************************************/
static long TSF_ioctl(struct file *pFile, unsigned int Cmd, unsigned long Param)
{
	signed int Ret = 0;
	unsigned long flags;	/* old: unsigned int flags; */
				/* FIX to avoid build warning */

	/*unsigned int pid = 0; */
	struct TSF_REG_IO_STRUCT RegIo;
	struct TSF_WAIT_IRQ_STRUCT IrqInfo;
	struct TSF_CLEAR_IRQ_STRUCT ClearIrq;
	struct TSF_USER_INFO_STRUCT *pUserInfo;

	/*  */
	if (pFile->private_data == NULL) {
		LOG_INF(
		    "private_data is NULL,(process, pid, tgid)=(%s, %d, %d)",
		    current->comm,
		    current->pid,
		    current->tgid);
		return -EFAULT;
	}
	/*  */
	pUserInfo = (struct TSF_USER_INFO_STRUCT *) (pFile->private_data);
	/*  */
	switch (Cmd) {
	case TSF_RESET:
		{
			TSF_Reset();
			break;
		}

		/*  */
	case TSF_DUMP_REG:
		{
			Ret = TSF_DumpReg();
			break;
		}
	case TSF_DUMP_ISR_LOG:
		{
			unsigned int currentPPB = m_CurrentPPB;

			spin_lock_irqsave(
				&(TSFInfo.SpinLockIrq[TSF_IRQ_TYPE_INT_TSF_ST]),
				flags);
			m_CurrentPPB = (m_CurrentPPB + 1) % LOG_PPNUM;
			spin_unlock_irqrestore(
				&(TSFInfo.SpinLockIrq[TSF_IRQ_TYPE_INT_TSF_ST]),
				flags);

			IRQ_LOG_PRINTER(
				TSF_IRQ_TYPE_INT_TSF_ST,
				currentPPB, _LOG_INF);
			IRQ_LOG_PRINTER(
				TSF_IRQ_TYPE_INT_TSF_ST,
				currentPPB, _LOG_ERR);
			break;
		}
	case TSF_READ_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param,
			    sizeof(struct TSF_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir of copy from user is
				 * implemented in TSF_ReadReg(...)
				 */
				Ret = TSF_ReadReg(&RegIo);
			} else {
				LOG_INF(
				    "TSF_READ_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case TSF_WRITE_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param,
			    sizeof(struct TSF_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir of copy from user is
				 * implemented in TSF_WriteReg(...)
				 */
				if ((RegIo.pData != NULL) &&
				    (RegIo.Count <= (TSF_REG_RANGE >> 2)))
					Ret = TSF_WriteReg(&RegIo);
			} else {
				LOG_INF(
				    "TSF_WRITE_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case TSF_WAIT_IRQ:
		{
			if (copy_from_user(&IrqInfo, (void *)Param,
			    sizeof(struct TSF_WAIT_IRQ_STRUCT)) == 0) {
				/*  */
				if ((IrqInfo.Type >= TSF_IRQ_TYPE_AMOUNT) ||
				    (IrqInfo.Type < 0)) {
					Ret = -EFAULT;
					LOG_INF("invalid type(%d)",
					    IrqInfo.Type);
					goto EXIT;
				}

				if ((IrqInfo.UserKey >= IRQ_USER_NUM_MAX) ||
				    (IrqInfo.UserKey < 0)) {
					LOG_INF(
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
				Ret = TSF_WaitIrq(&IrqInfo);

				if (copy_to_user((void *)Param, &IrqInfo,
				    sizeof(struct TSF_WAIT_IRQ_STRUCT)) != 0) {
					LOG_INF("copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				LOG_INF("TSF_WAIT_IRQ copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case TSF_CLEAR_IRQ:
		{
			if (copy_from_user(&ClearIrq, (void *)Param,
			    sizeof(struct TSF_CLEAR_IRQ_STRUCT)) == 0) {
				LOG_DBG("TSF_CLEAR_IRQ Type(%d)",
				    ClearIrq.Type);

				if ((ClearIrq.Type >= TSF_IRQ_TYPE_AMOUNT) ||
				    (ClearIrq.Type < 0)) {
					Ret = -EFAULT;
					LOG_INF("invalid type(%d)",
					    ClearIrq.Type);
					goto EXIT;
				}

				/*  */
				if ((ClearIrq.UserKey >= IRQ_USER_NUM_MAX)
				    || (ClearIrq.UserKey < 0)) {
					LOG_INF("errUserEnum(%d)",
					     ClearIrq.UserKey);
					Ret = -EFAULT;
					goto EXIT;
				}

				LOG_DBG(
				    "TSF_CLEAR_IRQ:Type(%d),Status(0x%08X),IrqStatus(0x%08X)\n",
				    ClearIrq.Type, ClearIrq.Status,
				    TSFInfo.IrqInfo.Status[ClearIrq.Type]);
				spin_lock_irqsave(
					&(TSFInfo.SpinLockIrq[ClearIrq.Type]),
					flags);
				TSFInfo.IrqInfo.Status[ClearIrq.Type] &=
					(~ClearIrq.Status);
				spin_unlock_irqrestore(
					&(TSFInfo.SpinLockIrq[ClearIrq.Type]),
					flags);
			} else {
				LOG_INF(
				    "TSF_CLEAR_IRQ copy_from_user failed\n");
				Ret = -EFAULT;
			}
			break;
		}

	default:
		{
			LOG_INF("Unknown Cmd(%d)", Cmd);
			LOG_INF(
			    "Fail, Cmd(%d), Dir(%d), Type(%d), Nr(%d),Size(%d)\n",
			    Cmd,
			    _IOC_DIR(Cmd), _IOC_TYPE(Cmd),
			    _IOC_NR(Cmd), _IOC_SIZE(Cmd));
			Ret = -EPERM;
			break;
		}
	}
	/*  */
EXIT:
	if (Ret != 0) {
		LOG_INF(
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
static int compat_get_TSF_read_register_data(
	struct compat_TSF_REG_IO_STRUCT __user *data32,
	struct TSF_REG_IO_STRUCT __user *data)
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

static int compat_put_TSF_read_register_data(
	struct compat_TSF_REG_IO_STRUCT __user *data32,
	struct TSF_REG_IO_STRUCT __user *data)
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

static long TSF_ioctl_compat(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;


	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		LOG_INF("no f_op !!!\n");
		return -ENOTTY;
	}
	switch (cmd) {
	case COMPAT_TSF_READ_REGISTER:
		{
			struct compat_TSF_REG_IO_STRUCT __user *data32;
			struct TSF_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_TSF_read_register_data(data32, data);
			if (err) {
				LOG_INF(
				"compat_get_TSF_read_register_data error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, TSF_READ_REGISTER,
						       (unsigned long)data);
			err = compat_put_TSF_read_register_data(data32, data);
			if (err) {
				LOG_INF(
				"compat_put_TSF_read_register_data error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_TSF_WRITE_REGISTER:
		{
			struct compat_TSF_REG_IO_STRUCT __user *data32;
			struct TSF_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_TSF_read_register_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_TSF_WRITE_REGISTER error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(
				filp, TSF_WRITE_REGISTER,
				(unsigned long)data);
			return ret;
		}

	case TSF_WAIT_IRQ:
	case TSF_CLEAR_IRQ:	/* structure (no pointer) */
	case TSF_RESET:
	case TSF_DUMP_REG:
	case TSF_DUMP_ISR_LOG:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
		/* return TSF_ioctl(filep, cmd, arg); */
	}
}

#endif

/******************************************************************************
 *
 ******************************************************************************/
static signed int TSF_open(struct inode *pInode, struct file *pFile)
{
	signed int Ret = 0;
	unsigned int i;
	unsigned long flags;	/* old: unsigned int flags; */
				/* FIX to avoid build warning */
	/*int q = 0, p = 0; */
	struct TSF_USER_INFO_STRUCT *pUserInfo;

	LOG_DBG("- E. UserCount: %d.", TSFInfo.UserCount);


	/*  */
	spin_lock(&(TSFInfo.SpinLockTSFRef));

	pFile->private_data = NULL;
	pFile->private_data = kmalloc(
		sizeof(struct TSF_USER_INFO_STRUCT), GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		LOG_DBG(
		  "ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)",
		  current->comm,
		  current->pid,
		  current->tgid);
		Ret = -ENOMEM;
	} else {
		pUserInfo = (struct TSF_USER_INFO_STRUCT *) pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}
	/*  */
	if (TSFInfo.UserCount > 0) {
		TSFInfo.UserCount++;
		spin_unlock(&(TSFInfo.SpinLockTSFRef));
		LOG_DBG(
		    "Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
		    TSFInfo.UserCount, current->comm,
		    current->pid, current->tgid);
		goto EXIT;
	} else {
		TSFInfo.UserCount++;
		spin_unlock(&(TSFInfo.SpinLockTSFRef));
		LOG_DBG(
		    "Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), first user",
		    TSFInfo.UserCount, current->comm,
		    current->pid, current->tgid);
	}

	/* do wait queue head init when re-enter in camera */
	/* Enable clock */
#ifdef CONFIG_PM_WAKELOCKS
	__pm_stay_awake(&tsf_wake_lock);
#else
	wake_lock(&tsf_wake_lock);
#endif
	TSF_EnableClock(MTRUE);
	g_u4TsfCnt = 0;
#ifdef CONFIG_PM_WAKELOCKS
	__pm_relax(&tsf_wake_lock);
#else
	wake_unlock(&tsf_wake_lock);
#endif
	LOG_INF("TSF open g_u4EnableClockCount: %d", g_u4EnableClockCount);
	/*  */

	spin_lock_irqsave(
		&(TSFInfo.SpinLockIrq[TSF_IRQ_TYPE_INT_TSF_ST]), flags);
	for (i = 0; i < TSF_IRQ_TYPE_AMOUNT; i++)
		TSFInfo.IrqInfo.Status[i] = 0;
	spin_unlock_irqrestore(
		&(TSFInfo.SpinLockIrq[TSF_IRQ_TYPE_INT_TSF_ST]), flags);

#ifdef KERNEL_LOG
	/* In EP, Add TSF_DBG_WRITE_REG for debug. Should remove it after EP */
	TSFInfo.DebugMask = (TSF_DBG_INT | TSF_DBG_DBGLOG | TSF_DBG_WRITE_REG);
#endif
	/*  */
EXIT:




	LOG_DBG("- X. Ret: %d. UserCount: %d.", Ret, TSFInfo.UserCount);
	return Ret;

}

/******************************************************************************
 *
 ******************************************************************************/
static signed int TSF_release(struct inode *pInode, struct file *pFile)
{
	struct TSF_USER_INFO_STRUCT *pUserInfo;
	/*unsigned int Reg; */

	LOG_DBG("- E. UserCount: %d.", TSFInfo.UserCount);

	/*  */
	if (pFile->private_data != NULL) {
		pUserInfo =
		    (struct TSF_USER_INFO_STRUCT *) pFile->private_data;
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	/*  */
	spin_lock(&(TSFInfo.SpinLockTSFRef));
	TSFInfo.UserCount--;

	if (TSFInfo.UserCount > 0) {
		spin_unlock(&(TSFInfo.SpinLockTSFRef));
		LOG_DBG(
		    "Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
		    TSFInfo.UserCount, current->comm,
		    current->pid, current->tgid);
		goto EXIT;
	} else
		spin_unlock(&(TSFInfo.SpinLockTSFRef));
	/*  */
	LOG_DBG(
	    "Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), last user",
	    TSFInfo.UserCount, current->comm,
	    current->pid, current->tgid);


	/* Disable clock. */
#ifdef CONFIG_PM_WAKELOCKS
	__pm_stay_awake(&tsf_wake_lock);
#else
	wake_lock(&tsf_wake_lock);
#endif
	TSF_EnableClock(MFALSE);
#ifdef CONFIG_PM_WAKELOCKS
	__pm_relax(&tsf_wake_lock);
#else
	wake_unlock(&tsf_wake_lock);
#endif

	LOG_INF("TSF release g_u4EnableClockCount: %d", g_u4EnableClockCount);

	/*  */
EXIT:


	LOG_DBG("- X. UserCount: %d.", TSFInfo.UserCount);
	return 0;
}


/******************************************************************************
 *
 ******************************************************************************/
static signed int TSF_mmap(struct file *pFile, struct vm_area_struct *pVma)
{
	unsigned long length = 0;
	unsigned int pfn = 0x0;

	length = pVma->vm_end - pVma->vm_start;
	/*  */
	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
	pfn = pVma->vm_pgoff << PAGE_SHIFT;

	LOG_INF("%s: pVma->vm_pgoff(0x%lx)",
		__func__, pVma->vm_pgoff);
	LOG_INF("%s: pfn(0x%x),phy(0x%lx)",
		__func__, pfn, pVma->vm_pgoff << PAGE_SHIFT);
	LOG_INF("pVmapVma->vm_start(0x%lx)",
		pVma->vm_start);
	LOG_INF("pVma->vm_end(0x%lx),length(0x%lx)",
		pVma->vm_end, length);


	switch (pfn) {
	case TSF_BASE_HW:
		if (length > TSF_REG_RANGE) {
			LOG_INF(
			    "mmap range error :module:0x%x length(0x%lx),TSF_REG_RANGE(0x%x)!",
			    pfn, length, TSF_REG_RANGE);
			return -EAGAIN;
		}
		break;
	default:
		LOG_INF("Illegal starting HW addr for mmap!");
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

static dev_t TSFDevNo;
static struct cdev *pTSFCharDrv;
static struct class *pTSFClass;

static const struct file_operations TSFFileOper = {
	.owner = THIS_MODULE,
	.open = TSF_open,
	.release = TSF_release,
	/* .flush   = mt_TSF_flush, */
	.mmap = TSF_mmap,
	.unlocked_ioctl = TSF_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = TSF_ioctl_compat,
#endif
};

/******************************************************************************
 *
 ******************************************************************************/
static inline void TSF_UnregCharDev(void)
{
	LOG_DBG("- E.");
	/*  */
	/* Release char driver */
	if (pTSFCharDrv != NULL) {
		cdev_del(pTSFCharDrv);
		pTSFCharDrv = NULL;
	}
	/*  */
	unregister_chrdev_region(TSFDevNo, 1);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline signed int TSF_RegCharDev(void)
{
	signed int Ret = 0;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = alloc_chrdev_region(&TSFDevNo, 0, 1, TSF_DEV_NAME);
	if (Ret < 0) {
		LOG_INF("alloc_chrdev_region failed, %d", Ret);
		return Ret;
	}
	/* Allocate driver */
	pTSFCharDrv = cdev_alloc();
	if (pTSFCharDrv == NULL) {
		LOG_INF("cdev_alloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pTSFCharDrv, &TSFFileOper);
	/*  */
	pTSFCharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pTSFCharDrv, TSFDevNo, 1);
	if (Ret < 0) {
		LOG_INF("Attatch file operation failed, %d", Ret);
		goto EXIT;
	}
	/*  */
EXIT:
	if (Ret < 0)
		TSF_UnregCharDev();

	/*  */

	LOG_DBG("- X.");
	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int TSF_probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	/*struct resource *pRes = NULL; */
	signed int i = 0;
	unsigned int n;
	/* Record interrupts info from device tree */
	unsigned int irq_info[3];
	struct device *dev = NULL;
	struct TSF_device *_tsfdev = NULL;

#ifdef CONFIG_OF
	struct TSF_device *TSF_dev;
#endif
	LOG_INF("- E. TSF driver probe.");

	/* Check platform_device parameters */
#ifdef CONFIG_OF

	if (pDev == NULL) {
		dev_info(&pDev->dev, "pDev is NULL");
		return -ENXIO;
	}

	nr_TSF_devs += 1;
	_tsfdev = krealloc(TSF_devs,
		sizeof(struct TSF_device) * nr_TSF_devs, GFP_KERNEL);
	if (!_tsfdev) {
		dev_info(&pDev->dev, "Unable to allocate TSF_devs\n");
		return -ENOMEM;
	}
	TSF_devs = _tsfdev;

	TSF_dev = &(TSF_devs[nr_TSF_devs - 1]);
	TSF_dev->dev = &pDev->dev;

	/* iomap registers */
	TSF_dev->regs = of_iomap(pDev->dev.of_node, 0);
	/* gISPSYS_Reg[nr_TSF_devs - 1] = TSF_dev->regs; */

	if (!TSF_dev->regs) {
		dev_info(&pDev->dev,
			"Unable to ioremap registers, of_iomap fail, nr_TSF_devs=%d, devnode(%s).\n",
			nr_TSF_devs, pDev->dev.of_node->name);
		return -ENOMEM;
	}

	LOG_INF("nr_TSF_devs=%d, devnode(%s), map_addr=0x%lx\n", nr_TSF_devs,
		pDev->dev.of_node->name, (unsigned long)TSF_dev->regs);

	/* get IRQ ID and request IRQ */
	TSF_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);

	if (TSF_dev->irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array(
		    pDev->dev.of_node, "interrupts",
		    irq_info, ARRAY_SIZE(irq_info))) {
			dev_info(&pDev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		for (i = 0; i < TSF_IRQ_TYPE_AMOUNT; i++) {
			if (strcmp(pDev->dev.of_node->name,
			    TSF_IRQ_CB_TBL[i].device_name) == 0) {
				Ret = request_irq(
				    TSF_dev->irq,
				    (irq_handler_t) TSF_IRQ_CB_TBL[i].isr_fp,
				    irq_info[2],
				    (const char *)TSF_IRQ_CB_TBL[i].device_name,
				    NULL);

				if (Ret) {
					dev_info(
					    &pDev->dev,
					    "Unable to request IRQ, request_irq fail, nr_TSF_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
					    nr_TSF_devs,
					    pDev->dev.of_node->name,
					    TSF_dev->irq,
					    TSF_IRQ_CB_TBL[i].device_name);
					return Ret;
				}

				LOG_INF(
					"nr_TSF_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
					nr_TSF_devs,
					pDev->dev.of_node->name,
					TSF_dev->irq,
					TSF_IRQ_CB_TBL[i].device_name);
				break;
			}
		}

		if (i > TSF_IRQ_TYPE_AMOUNT) {
			LOG_INF(
				"No corresponding ISR!!: nr_TSF_devs=%d, devnode(%s), irq=%d\n",
				nr_TSF_devs,
				pDev->dev.of_node->name,
				TSF_dev->irq);
		}


	} else {
		LOG_INF("No IRQ!!: nr_TSF_devs=%d, devnode(%s), irq=%d\n",
			nr_TSF_devs,
			pDev->dev.of_node->name,
			TSF_dev->irq);
	}


#endif

	/* Only register char driver in the 1st time */
	if (nr_TSF_devs == 1) {

		/* Register char driver */
		Ret = TSF_RegCharDev();
		if (Ret) {
			dev_info(&pDev->dev, "register char failed");
			return Ret;
		}
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
		/*CCF: Grab clock pointer (struct clk*) */
		TSF_clk.CG_IMGSYS_TSF = devm_clk_get(
				&pDev->dev, "TSF_CAMSYS_TSF_CGPDN");
		if (IS_ERR(TSF_clk.CG_IMGSYS_TSF)) {
			LOG_INF("cannot get CG_IMGSYS_TSF clock\n");
			return PTR_ERR(TSF_clk.CG_IMGSYS_TSF);
		}
#endif	/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */


		/* Create class register */
		pTSFClass = class_create(THIS_MODULE, "TSFdrv");
		if (IS_ERR(pTSFClass)) {
			Ret = PTR_ERR(pTSFClass);
			LOG_INF("Unable to create class, err = %d", Ret);
			goto EXIT;
		}

		dev = device_create(
			pTSFClass, NULL,
			TSFDevNo, NULL,
			TSF_DEV_NAME);

		if (IS_ERR(dev)) {
			Ret = PTR_ERR(dev);
			dev_info(
			    &pDev->dev,
			    "Failed to create device: /dev/%s, err = %d",
			    TSF_DEV_NAME, Ret);
			goto EXIT;
		}

		/* Init spinlocks */
		spin_lock_init(&(TSFInfo.SpinLockTSFRef));
		spin_lock_init(&(TSFInfo.SpinLockTSF));
		for (n = 0; n < TSF_IRQ_TYPE_AMOUNT; n++)
			spin_lock_init(&(TSFInfo.SpinLockIrq[n]));

#ifdef CONFIG_PM_WAKELOCKS
		wakeup_source_init(&tsf_wake_lock, "tsf_lock_wakelock");
#else
		wake_lock_init(
			&tsf_wake_lock,
			WAKE_LOCK_SUSPEND,
			"tsf_lock_wakelock");
#endif
		/*  */
		init_waitqueue_head(&TSFInfo.WaitQueueHead);
		INIT_WORK(&TSFInfo.ScheduleTsfWork, TSF_ScheduleWork);

		for (i = 0; i < TSF_IRQ_TYPE_AMOUNT; i++)
			tasklet_init(
				TSF_tasklet[i].pTSF_tkt,
				TSF_tasklet[i].tkt_cb, 0);

		/* Init TSFInfo */
		spin_lock(&(TSFInfo.SpinLockTSFRef));
		TSFInfo.UserCount = 0;
		spin_unlock(&(TSFInfo.SpinLockTSFRef));
		/*  */
		TSFInfo.IrqInfo.Mask[
			TSF_IRQ_TYPE_INT_TSF_ST] = INT_ST_MASK_TSF;

	}

EXIT:
	if (Ret < 0)
		TSF_UnregCharDev();


	LOG_INF("- X. TSF driver probe.");

	return Ret;
}

/******************************************************************************
 * Called when the device is being detached from the driver
 ******************************************************************************/
static signed int TSF_remove(struct platform_device *pDev)
{
	/*struct resource *pRes; */
	signed int IrqNum;
	int i;
	/*  */
	LOG_DBG("- E.");
	/* unregister char driver. */
	TSF_UnregCharDev();

	/* Release IRQ */
	disable_irq(TSFInfo.IrqNum);
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);

	/* kill tasklet */
	for (i = 0; i < TSF_IRQ_TYPE_AMOUNT; i++)
		tasklet_kill(TSF_tasklet[i].pTSF_tkt);
#if 0
	/* free all registered irq(child nodes) */
	TSF_UnRegister_AllregIrq();
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
			    ((REG_IRQ_NODE *) ((char *)__mptr -
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
	device_destroy(pTSFClass, TSFDevNo);
	/*  */
	class_destroy(pTSFClass);
	pTSFClass = NULL;
	/*  */
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/

static signed int TSF_suspend(struct platform_device *pDev, pm_message_t Mesg)
{
	if (g_u4EnableClockCount > 0) {
		TSF_EnableClock(MFALSE);
		g_u4TsfCnt++;
	}
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int TSF_resume(struct platform_device *pDev)
{
	if (g_u4TsfCnt > 0) {
		TSF_EnableClock(MTRUE);
		g_u4TsfCnt--;
	}
	return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int TSF_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	LOG_INF("TSF suspend g_u4EnableClockCount: %d, g_u4TsfCnt: %d",
		g_u4EnableClockCount, g_u4TsfCnt);

	return TSF_suspend(pdev, PMSG_SUSPEND);
}

int TSF_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	LOG_INF("TSF resume g_u4EnableClockCount: %d, g_u4TsfCnt: %d",
		g_u4EnableClockCount, g_u4TsfCnt);

	return TSF_resume(pdev);
}

#ifndef CONFIG_OF
/* extern void mt_irq_set_sens(unsigned int irq, unsigned int sens); */
/* extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity); */
#endif
int TSF_pm_restore_noirq(struct device *device)
{
	pr_debug("calling %s()\n", __func__);
#ifndef CONFIG_OF
	mt_irq_set_sens(TSF_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(TSF_IRQ_BIT_ID, MT_POLARITY_LOW);
#endif
	return 0;

}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define TSF_pm_suspend NULL
#define TSF_pm_resume  NULL
#define TSF_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_OF
/*
 * Note!!! The order and member of .compatible must be the same with that in
 *  "TSF_DEV_NODE_ENUM" in camera_TSF.h
 */
static const struct of_device_id TSF_of_ids[] = {
	{.compatible = "mediatek,tsf",},
	{}
};
#endif

const struct dev_pm_ops TSF_pm_ops = {
	.suspend = TSF_pm_suspend,
	.resume = TSF_pm_resume,
	.freeze = TSF_pm_suspend,
	.thaw = TSF_pm_resume,
	.poweroff = TSF_pm_suspend,
	.restore = TSF_pm_resume,
	.restore_noirq = TSF_pm_restore_noirq,
};


/******************************************************************************
 *
 ******************************************************************************/
static struct platform_driver TSFDriver = {
	.probe = TSF_probe,
	.remove = TSF_remove,
	.suspend = TSF_suspend,
	.resume = TSF_resume,
	.driver = {
		   .name = TSF_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = TSF_of_ids,
#endif
#ifdef CONFIG_PM
		   .pm = &TSF_pm_ops,
#endif
	}
};


static int TSF_dump_read(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "\n============ TSF dump register============\n");
	seq_puts(m, "TSF Config Info\n");

	if (TSFInfo.UserCount > 0) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(TSF_BASE_HW + 0x4),
			(unsigned int)TSF_RD32(ISP_TSF_BASE + 0x4));


		for (i = 0x80C; i < 0x82C; i = i + 4) {
			seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(TSF_BASE_HW + i),
				(unsigned int)TSF_RD32(ISP_TSF_BASE + i));
		}
		seq_puts(m, "TSF DMA Debug Info\n");
		for (i = 0x60; i < 0x88; i = i + 4) {
			seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(TSF_BASE_HW + i),
				(unsigned int)TSF_RD32(ISP_TSF_BASE + i));
		}

		for (i = 0xC0; i < 0xE4; i = i + 4) {
			seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(TSF_BASE_HW + i),
				(unsigned int)TSF_RD32(ISP_TSF_BASE + i));
		}
	}
	seq_puts(m, "\n============ TSF dump debug ============\n");

	return 0;
}


static int proc_TSF_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, TSF_dump_read, NULL);
}

static const struct file_operations TSF_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_TSF_dump_open,
	.read = seq_read,
};


static int TSF_reg_read(struct seq_file *m, void *v)
{
	unsigned int i;

	seq_puts(m, "======== read TSF register ========\n");

	if (TSFInfo.UserCount > 0) {

		seq_printf(m, "[0x%08X 0x%08X]\n",
			(unsigned int)(TSF_BASE_HW + 0x4),
			(unsigned int)TSF_RD32(TSF_INT_EN_REG));

		for (i = 0x80C; i <= 0x82C; i = i + 4) {
			seq_printf(m, "[0x%08X 0x%08X]\n",
				(unsigned int)(TSF_BASE_HW + i),
				(unsigned int)TSF_RD32(TSF_START_REG + i));
		}

		for (i = 0x60; i <= 0x88; i = i + 4) {
			seq_printf(m, "[0x%08X 0x%08X]\n",
				(unsigned int)(TSF_BASE_HW + i),
				(unsigned int)TSF_RD32(TSF_START_REG + i));
		}

		for (i = 0xC0; i <= 0xE4; i = i + 4) {
			seq_printf(m, "[0x%08X 0x%08X]\n",
				(unsigned int)(TSF_BASE_HW + i),
				(unsigned int)TSF_RD32(TSF_START_REG + i));
		}
	}

	return 0;
}

/*static int TSF_reg_write(struct file *file, const char __user *buffer, */
/*	size_t count, loff_t *data)*/

static ssize_t TSF_reg_write(
	struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	char desc[128];
	int len = 0;
	char addrSzBuf[24];
	char valSzBuf[24];
	char *pszTmp;
	int addr = 0, val = 0;
	long int tempval;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	if (TSFInfo.UserCount <= 0)
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%23s %23s", addrSzBuf, valSzBuf) == 2) {
		pszTmp = strstr(addrSzBuf, "0x");
		if (pszTmp == NULL) {
			if (kstrtol(addrSzBuf, 10, (long int *)&tempval) != 0)
				LOG_INF(
				    "scan decimal addr is wrong !!:%s",
				    addrSzBuf);
			else
				addr = tempval;
		} else {
			if (strlen(addrSzBuf) > 2) {
				if (sscanf(addrSzBuf + 2, "%x", &addr) != 1)
					LOG_INF(
					  "scan hexadecimal addr is wrong !!:%s",
					  addrSzBuf);
			} else {
				LOG_INF(
				  "TSF Write Addr Error!!:%s",
				  addrSzBuf);
			}
		}

		pszTmp = strstr(valSzBuf, "0x");
		if (pszTmp == NULL) {
			if (kstrtol(valSzBuf, 10, (long int *)&tempval) != 0)
				LOG_INF(
				    "scan decimal value is wrong !!:%s",
				    valSzBuf);
			else
				val = tempval;
		} else {
			if (strlen(valSzBuf) > 2) {
				if (sscanf(valSzBuf + 2, "%x", &val) != 1)
					LOG_INF(
					  "scan hexadecimal value is wrong!:%s",
					  valSzBuf);
			} else {
				LOG_INF(
				    "TSF Write Value Error!!:%s\n",
				    valSzBuf);
			}
		}

		if ((addr >= TSF_BASE_HW) && (addr <= TSF_SPARE_CELL_HW)) {
			LOG_INF(
			    "Write Request - addr:0x%x, value:0x%x\n",
			    addr, val);
			TSF_WR32((ISP_TSF_BASE + (addr - TSF_BASE_HW)), val);
		} else {
			LOG_INF(
			    "Write-Address Range exceeds the size of hw TSF!! addr:0x%x, value:0x%x\n",
			    addr, val);
		}

	} else if (sscanf(desc, "%23s", addrSzBuf) == 1) {
		pszTmp = strstr(addrSzBuf, "0x");
		if (pszTmp == NULL) {
			if (kstrtol(addrSzBuf, 10, (long int *)&tempval) != 0)
				LOG_INF(
				    "scan decimal addr is wrong !!:%s",
				    addrSzBuf);
			else
				addr = tempval;
		} else {
			if (strlen(addrSzBuf) > 2) {
				if (sscanf(addrSzBuf + 2, "%x", &addr) != 1)
					LOG_INF(
					  "scan hexadecimal addr is wrong!:%s",
					  addrSzBuf);
			} else {
				LOG_INF("TSF Read Addr Error!!:%s", addrSzBuf);
			}
		}

		if ((addr >= TSF_BASE_HW) && (addr <= TSF_SPARE_CELL_HW)) {
			val = TSF_RD32((ISP_TSF_BASE + (addr - TSF_BASE_HW)));
			LOG_INF(
			    "Read Request - addr:0x%x,value:0x%x\n",
			    addr, val);
		} else {
			LOG_INF(
			    "Read-Address Range exceeds the size of hw TSF!! addr:0x%x, value:0x%x\n",
			    addr, val);
		}

	}


	return count;
}

static int proc_TSF_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, TSF_reg_read, NULL);
}

static const struct file_operations TSF_reg_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_TSF_reg_open,
	.read = seq_read,
	.write = TSF_reg_write,
};


/******************************************************************************
 *
 ******************************************************************************/
static signed int __init TSF_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
	/* FIX-ME: linux-3.10 procfs API changed */
	/* use proc_create */
	struct proc_dir_entry *proc_entry;
	struct proc_dir_entry *isp_TSF_dir;

	int i;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = platform_driver_register(&TSFDriver);
	if (Ret < 0) {
		LOG_INF("platform_driver_register fail");
		return Ret;
	}
#if 0
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,TSF");
	if (!node) {
		LOG_INF("find mediatek,TSF node failed!!!\n");
		return -ENODEV;
	}
	ISP_TSF_BASE = of_iomap(node, 0);
	if (!ISP_TSF_BASE) {
		LOG_INF("unable to map ISP_TSF_BASE registers!!!\n");
		return -ENODEV;
	}
	LOG_DBG("ISP_TSF_BASE: %lx\n", ISP_TSF_BASE);
#endif

	isp_TSF_dir = proc_mkdir("TSF", NULL);
	if (!isp_TSF_dir) {
		LOG_INF("[%s]: fail to mkdir /proc/TSF\n", __func__);
		return 0;
	}

	/* proc_entry = proc_create(
	 *	"pll_test", 0644,
	 *	isp_TSF_dir,
	 *	&pll_test_proc_fops);
	 */

	proc_entry = proc_create(
			"TSF_dump", 0444,
			isp_TSF_dir,
			&TSF_dump_proc_fops);

	proc_entry = proc_create(
			"TSF_reg", 0644,
			isp_TSF_dir,
			&TSF_reg_proc_fops);


	/* isr log */
	if (PAGE_SIZE <
	    ((TSF_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
	    ((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) *
	     LOG_PPNUM)) {
		i = 0;
		while (i <
		       ((TSF_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
			 ((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM)) {
			i += PAGE_SIZE;
		}
	} else {
		i = PAGE_SIZE;
	}
	pLog_kmalloc = kmalloc(i, GFP_KERNEL);
	if (pLog_kmalloc == NULL) {
		LOG_INF("log mem not enough\n");
		return -ENOMEM;
	}
	memset(pLog_kmalloc, 0x00, i);
	tmp = pLog_kmalloc;
	for (i = 0; i < LOG_PPNUM; i++) {
		for (j = 0; j < TSF_IRQ_TYPE_AMOUNT; j++) {
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
		//log buffer ,in case of overflow
		/* tmp = (void*) ((unsigned int)tmp + NORMAL_STR_LEN); */
		/* log buffer ,in case of overflow */
		tmp = (void *)((char *)tmp + NORMAL_STR_LEN);
	}


	LOG_DBG("- X. Ret: %d.", Ret);
	return Ret;
}


/******************************************************************************
 *
 ******************************************************************************/
static void __exit TSF_Exit(void)
{
	/*int i; */

	LOG_DBG("- E.");
	/*  */
	platform_driver_unregister(&TSFDriver);
	/*  */
	kfree(pLog_kmalloc);

	/*  */
}


/******************************************************************************
 *
 ******************************************************************************/
static void TSF_ScheduleWork(struct work_struct *data)
{
	if (TSF_DBG_DBGLOG & TSFInfo.DebugMask)
		LOG_DBG("- E.");
}



static irqreturn_t ISP_Irq_TSF(signed int Irq, void *DeviceId)
{
	unsigned int TSFIntStatus;

	TSFIntStatus = TSF_RD32(TSF_INT_REG);	/* TSF_INT_STATUS */
	spin_lock(&(TSFInfo.SpinLockIrq[TSF_IRQ_TYPE_INT_TSF_ST]));
	if (TSF_INT_ST == (TSF_INT_ST & TSFIntStatus)) {
#ifdef __TSF_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("TSF_dve_irq");
#endif
		/* Update the frame status. */
		schedule_work(&TSFInfo.ScheduleTsfWork);
		TSFInfo.IrqInfo.Status[TSF_IRQ_TYPE_INT_TSF_ST] |= TSF_INT_ST;

#ifdef __TSF_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
	}
	spin_unlock(&(TSFInfo.SpinLockIrq[TSF_IRQ_TYPE_INT_TSF_ST]));
	wake_up_interruptible(&TSFInfo.WaitQueueHead);

	/* dump log, use tasklet */
	/* IRQ_LOG_KEEPER( */
	/*	TSF_IRQ_TYPE_INT_TSF_ST, m_CurrentPPB, _LOG_INF, */
	/*	"ISP_Irq_TSF:%d, reg 0x%x : 0x%x\n", */
	/*	Irq, TSF_INT_HW, TSFIntStatus); */

	/* IRQ_LOG_KEEPER(TSF_IRQ_TYPE_INT_TSF_ST, m_CurrentPPB,
	 *	_LOG_INF, "DveHWSta:0x%x, WmfeHWSta:0x%x,
	 *	*TSFDveSta0:0x%x\n", DveStatus, WmfeStatus, TSFDveSta0);
	 */

	/* if (TSFIntStatus & TSF_INT_ST) */
	/*	tasklet_schedule( */
	/*		TSF_tasklet[TSF_IRQ_TYPE_INT_TSF_ST].pTSF_tkt); */

	return IRQ_HANDLED;
}

static void ISP_TaskletFunc_TSF(unsigned long data)
{
	IRQ_LOG_PRINTER(TSF_IRQ_TYPE_INT_TSF_ST, m_CurrentPPB, _LOG_DBG);
	IRQ_LOG_PRINTER(TSF_IRQ_TYPE_INT_TSF_ST, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(TSF_IRQ_TYPE_INT_TSF_ST, m_CurrentPPB, _LOG_ERR);

}


/******************************************************************************
 *
 ******************************************************************************/
module_init(TSF_Init);
module_exit(TSF_Exit);
MODULE_DESCRIPTION("Camera TSF driver");
MODULE_AUTHOR("MM3/SW5");
MODULE_LICENSE("GPL");
