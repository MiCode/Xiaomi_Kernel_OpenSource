// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

/**************************************************************
 * camera_DPE.c - Linux DPE Device Driver
 *
 * DESCRIPTION:
 *     This file provid the other drivers DPE relative functions
 *
 **************************************************************/
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
#include "camera_dpe.h"
/*#include <mach/irqs.h>*/
/* #include <mach/mt_reg_base.h> */
/* #if defined(CONFIG_MTK_LEGACY) */
/* For clock mgr APIS. enable_clock()/disable_clock(). */
/* #include <mach/mt_clkmgr.h> */
/* #endif */
#define TODO
#ifndef TODO
#include <mt-plat/sync_write.h> /* For mt65xx_reg_sync_writel(). */
#endif
/* For spm_enable_sodi()/spm_disable_sodi(). */
/* #include <mach/mt_spm_idle.h> */
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

#include <cmdq_core.h>
#include <cmdq_record.h>
#ifdef COFNIG_MTK_IOMMU
#include "mtk_iommu.h"
#elif defined(CONFIG_MTK_M4U)
#include <m4u.h>
#endif

#ifdef CONFIG_MTK_SMI_EXT
#include <smi_public.h>
#endif
#include "engine_request.h"

#define CMDQ_COMMON

//#define DPE_PMQOS
#ifdef DPE_PMQOS
#include <linux/pm_qos.h>
#include <mmdvfs_pmqos.h>
#endif

/* Measure the kernel performance
 * #define __DPE_KERNEL_PERFORMANCE_MEASURE__
 */
#ifdef __DPE_KERNEL_PERFORMANCE_MEASURE__
#include <linux/met_drv.h>
#include <linux/mtk_ftrace.h>
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
struct DPE_CLK_STRUCT {
	struct clk *CG_IPESYS_DPE;
	struct clk *CG_TOP_MUX_DPE;
};
struct DPE_CLK_STRUCT dpe_clk;
#endif
/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */

#ifndef MTRUE
#define MTRUE 1
#endif
#ifndef MFALSE
#define MFALSE 0
#endif

#define DPE_DEV_NAME "camera-dpe"
/* #define EP_NO_CLKMGR */
#define BYPASS_REG (0)
#define DUMMY_DPE (0)
/*I can' test the situation in FPGA due to slow FPGA. */
#define MyTag "[DPE]"
#define IRQTag "KEEPER"

#define LOG_VRB(format, args...) pr_debug(MyTag format, ##args)

/* #define DPE_DEBUG_USE */
#ifdef DPE_DEBUG_USE
#define LOG_DBG(format, args...) pr_info(MyTag format, ##args)
#else
#define LOG_DBG(format, args...)
#endif

#define LOG_INF(format, args...) pr_info(MyTag format, ##args)
#define LOG_NOTICE(format, args...) pr_notice(MyTag format, ##args)
#define LOG_WRN(format, args...) pr_info(MyTag format, ##args)
#define LOG_ERR(format, args...) pr_info(MyTag format, ##args)
#define LOG_AST(format, args...) pr_info(MyTag format, ##args)

/**************************************************************
 *
 **************************************************************/
/* #define DPE_WR32(addr, data) iowrite32(data, addr) */
#define DPE_WR32(addr, data) writel(data, addr)
#define DPE_RD32(addr) readl(addr)
#define DPE_MASKWR(addr, data, mask) \
	DPE_WR32(addr, ((DPE_RD32(addr) & ~(mask)) | data))

/**************************************************************
 *
 **************************************************************/
/* dynamic log level */
#define DPE_DBG_DBGLOG (0x00000001)
#define DPE_DBG_INFLOG (0x00000002)
#define DPE_DBG_INT (0x00000004)
#define DPE_DBG_READ_REG (0x00000008)
#define DPE_DBG_WRITE_REG (0x00000010)
#define DPE_DBG_TASKLET (0x00000020)

/*
 *    CAM interrupt status
 */

/* normal siganl : happens to be the same bit as register bit*/
/*#define DPE_INT_ST           (1<<0)*/

/*
 *   IRQ signal mask
 */

#define INT_ST_MASK_DPE (DPE_INT_ST)

#define CMDQ_REG_MASK 0xffffffff
#define DPE_START 0x1

#define DPE_ENABLE 0x1

cmdqBackupSlotHandle DPE_slot;
//static u32 DPE_counter;
u32 DPE_val;

/* static irqreturn_t DPE_Irq_CAM_A(signed int  Irq,void *DeviceId); */
static irqreturn_t ISP_Irq_DVP(signed int Irq, void *DeviceId);
static irqreturn_t ISP_Irq_DVS(signed int Irq, void *DeviceId);
static void DPE_ScheduleWork(struct work_struct *data);

typedef irqreturn_t (*IRQ_CB)(signed int, void *);

struct ISR_TABLE {
	IRQ_CB isr_fp;
	unsigned int int_number;
	char device_name[16];
};

#ifndef CONFIG_OF
const struct ISR_TABLE DPE_IRQ_CB_TBL[DPE_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_DVP, DPE_IRQ_BIT_ID, "dvp"},
	{ISP_Irq_DVS, DPE_IRQ_BIT_ID, "dvs"},
};

#else
/* int number is got from kernel api */
const struct ISR_TABLE DPE_IRQ_CB_TBL[DPE_IRQ_TYPE_AMOUNT] = {
#if DUMMY_DPE
	{ISP_Irq_DVP, 0, "dvp-dummy"},
	{ISP_Irq_DVS, 0, "dvs-dummy"},
#else
	{ISP_Irq_DVP, 0, "dvp"},
	{ISP_Irq_DVS, 0, "dvs"},
#endif
};
#endif
/*
 */
/*  */
typedef void (*tasklet_cb)(unsigned long);
struct Tasklet_table {
	tasklet_cb tkt_cb;
	struct tasklet_struct *pDPE_tkt;
};

struct tasklet_struct Dpetkt[DPE_IRQ_TYPE_AMOUNT];

static void ISP_TaskletFunc_DVP(unsigned long data);
static void ISP_TaskletFunc_DVS(unsigned long data);

static struct Tasklet_table DPE_tasklet[DPE_IRQ_TYPE_AMOUNT] = {
	{ISP_TaskletFunc_DVP, &Dpetkt[DPE_IRQ_TYPE_INT_DVP_ST]},
	{ISP_TaskletFunc_DVS, &Dpetkt[DPE_IRQ_TYPE_INT_DVS_ST]},
};
static struct work_struct logWork;
static void logPrint(struct work_struct *data);

static DEFINE_MUTEX(gDpeMutex);
static DEFINE_MUTEX(gDpeDequeMutex);

#ifdef CONFIG_OF

struct DPE_device {
	void __iomem *regs;
	struct device *dev;
	int irq;
};

static struct DPE_device *DPE_devs;
static int nr_DPE_devs;

/* Get HW modules' base address from device nodes */
#define DPE_DEV_NODE_IDX 0
#define IPESYS_DEV_MODE_IDX 1

#define ISP_DPE_BASE (DPE_devs[DPE_DEV_NODE_IDX].regs)
#define ISP_IPESYS_BASE (DPE_devs[IPESYS_DEV_MODE_IDX].regs)

#else
#define ISP_DPE_BASE (IPESYS_BASE + 0x100000)

#endif

static unsigned int g_u4EnableClockCount;
static unsigned int g_SuspendCnt;

/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32

bool g_isDvpInUse;
bool g_DPE_PMState;

#ifdef DPE_PMQOS
static struct pm_qos_request dpe_qos_request;
static u64 max_dpe_freq;
static u64 mid_dpe_freq;
#endif

enum DPE_FRAME_STATUS_ENUM {
	DPE_FRAME_STATUS_EMPTY,    /* 0 */
	DPE_FRAME_STATUS_ENQUE,    /* 1 */
	DPE_FRAME_STATUS_RUNNING,  /* 2 */
	DPE_FRAME_STATUS_FINISHED, /* 3 */
	DPE_FRAME_STATUS_TOTAL
};

enum DPE_REQUEST_STATE_ENUM {
	DPE_REQUEST_STATE_EMPTY,    /* 0 */
	DPE_REQUEST_STATE_PENDING,  /* 1 */
	DPE_REQUEST_STATE_RUNNING,  /* 2 */
	DPE_REQUEST_STATE_FINISHED, /* 3 */
	DPE_REQUEST_STATE_TOTAL
};

struct DPE_REQUEST_STRUCT {
	enum DPE_REQUEST_STATE_ENUM State;
	pid_t processID;       /* caller process ID */
	unsigned int callerID; /* caller thread ID */

	unsigned int
		enqueReqNum;   /* to judge it belongs to which frame package */
	signed int FrameWRIdx; /* Frame write Index */
	signed int RrameRDIdx; /* Frame read Index */
	enum DPE_FRAME_STATUS_ENUM
		DpeFrameStatus[_SUPPORT_MAX_DPE_FRAME_REQUEST_];
	struct DPE_Config DpeFrameConfig[_SUPPORT_MAX_DPE_FRAME_REQUEST_];
};

struct DPE_REQUEST_RING_STRUCT {
	signed int WriteIdx;     /* enque how many request  */
	signed int ReadIdx;      /* read which request index */
	signed int HWProcessIdx; /* HWWriteIdx */
	struct DPE_REQUEST_STRUCT
		DPEReq_Struct[_SUPPORT_MAX_DPE_REQUEST_RING_SIZE_];
};

struct DPE_CONFIG_STRUCT {
	struct DPE_Config DpeFrameConfig[_SUPPORT_MAX_DPE_FRAME_REQUEST_];
};

static struct DPE_REQUEST_RING_STRUCT g_DPE_ReqRing;
static struct DPE_CONFIG_STRUCT g_DpeEnqueReq_Struct;
static struct DPE_CONFIG_STRUCT g_DpeDequeReq_Struct;
static struct engine_requests dpe_reqs;
static struct DPE_Request kDpeReq;
/**************************************************************
 *
 **************************************************************/
struct DPE_USER_INFO_STRUCT {
	pid_t Pid;
	pid_t Tid;
};
enum DPE_PROCESS_ID_ENUM {
	DPE_PROCESS_ID_NONE,
	DPE_PROCESS_ID_DPE,
	DPE_PROCESS_ID_AMOUNT
};

/**************************************************************
 *
 **************************************************************/
struct DPE_IRQ_INFO_STRUCT {
	unsigned int Status[DPE_IRQ_TYPE_AMOUNT];
	signed int DpeIrqCnt;
	pid_t ProcessID[DPE_PROCESS_ID_AMOUNT];
	unsigned int Mask[DPE_IRQ_TYPE_AMOUNT];
};

struct DPE_INFO_STRUCT {
	spinlock_t SpinLockDPERef;
	spinlock_t SpinLockDPE;
	spinlock_t SpinLockIrq[DPE_IRQ_TYPE_AMOUNT];
	wait_queue_head_t WaitQueueHead;
	struct work_struct ScheduleDpeWork;
	struct workqueue_struct *wkqueue;
	unsigned int UserCount; /* User Count */
	unsigned int DebugMask; /* Debug Mask */
	signed int IrqNum;
	struct DPE_IRQ_INFO_STRUCT IrqInfo;
	signed int WriteReqIdx;
	signed int ReadReqIdx;
	pid_t ProcessID[_SUPPORT_MAX_DPE_FRAME_REQUEST_];
};

static struct DPE_INFO_STRUCT DPEInfo;

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
} *PSV_LOG_STR;

static void *pLog_kmalloc;
static struct SV_LOG_STR gSvLog[DPE_IRQ_TYPE_AMOUNT];

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

/* DPE unmapped base address macro for GCE to access */
#define DVS_CTRL00_HW                (DPE_BASE_HW)
#define DVS_CTRL01_HW                (DPE_BASE_HW + 0x004)
#define DVS_CTRL02_HW                (DPE_BASE_HW + 0x008)
#define DVS_CTRL03_HW                (DPE_BASE_HW + 0x00C)
#define DVS_CTRL04_HW                (DPE_BASE_HW + 0x010)
#define DVS_CTRL05_HW                (DPE_BASE_HW + 0x014)
#define DVS_CTRL06_HW                (DPE_BASE_HW + 0x018)
#define DVS_CTRL07_HW                (DPE_BASE_HW + 0x01C)
#define DVS_OCC_PQ_0_HW              (DPE_BASE_HW + 0x030)
#define	DVS_OCC_PQ_1_HW              (DPE_BASE_HW + 0x034)
#define	DVS_OCC_ATPG_HW              (DPE_BASE_HW + 0x038)
#define DVS_IRQ_00_HW                (DPE_BASE_HW + 0x040)
#define DVS_CTRL_STATUS0_HW          (DPE_BASE_HW + 0x050)
#define DVS_CTRL_STATUS1_HW          (DPE_BASE_HW + 0x054)
#define DVS_CTRL_STATUS2_HW          (DPE_BASE_HW + 0x058)
#define DVS_IRQ_STATUS_HW            (DPE_BASE_HW + 0x05C)
#define DVS_FRM_STATUS0_HW           (DPE_BASE_HW + 0x060)
#define DVS_FRM_STATUS1_HW           (DPE_BASE_HW + 0x064)
#define DVS_FRM_STATUS2_HW           (DPE_BASE_HW + 0x068)
#define DVS_FRM_STATUS3_HW           (DPE_BASE_HW + 0x06C)
#define DVS_CUR_STATUS_HW            (DPE_BASE_HW + 0x070)
#define DVS_SRC_CTRL_HW              (DPE_BASE_HW + 0x074)
#define DVS_CRC_CTRL_HW              (DPE_BASE_HW + 0x080)
#define DVS_CRC_IN_HW                (DPE_BASE_HW + 0x08C)
#define DVS_DRAM_STA0_HW             (DPE_BASE_HW + 0x090)
#define DVS_DRAM_STA1_HW             (DPE_BASE_HW + 0x094)
#define DVS_DRAM_ULT_HW              (DPE_BASE_HW + 0x098)
#define DVS_DRAM_PITCH_HW            (DPE_BASE_HW + 0x09C)
#define DVS_SRC_00_HW                (DPE_BASE_HW + 0x100)
#define DVS_SRC_01_HW                (DPE_BASE_HW + 0x104)
#define DVS_SRC_02_HW                (DPE_BASE_HW + 0x108)
#define DVS_SRC_03_HW                (DPE_BASE_HW + 0x10C)
#define DVS_SRC_04_HW                (DPE_BASE_HW + 0x110)
#define DVS_SRC_05_L_FRM0_HW         (DPE_BASE_HW + 0x114)
#define DVS_SRC_06_L_FRM1_HW         (DPE_BASE_HW + 0x118)
#define DVS_SRC_07_L_FRM2_HW         (DPE_BASE_HW + 0x11C)
#define DVS_SRC_08_L_FRM3_HW         (DPE_BASE_HW + 0x120)
#define DVS_SRC_09_R_FRM0_HW         (DPE_BASE_HW + 0x124)
#define DVS_SRC_10_R_FRM1_HW         (DPE_BASE_HW + 0x128)
#define DVS_SRC_11_R_FRM2_HW         (DPE_BASE_HW + 0x12C)
#define DVS_SRC_12_R_FRM3_HW         (DPE_BASE_HW + 0x130)
#define DVS_SRC_13_L_VMAP0_HW        (DPE_BASE_HW + 0x134)
#define DVS_SRC_14_L_VMAP1_HW        (DPE_BASE_HW + 0x138)
#define DVS_SRC_15_L_VMAP2_HW        (DPE_BASE_HW + 0x13C)
#define DVS_SRC_16_L_VMAP3_HW        (DPE_BASE_HW + 0x140)
#define DVS_SRC_17_R_VMAP0_HW        (DPE_BASE_HW + 0x144)
#define DVS_SRC_18_R_VMAP1_HW        (DPE_BASE_HW + 0x148)
#define DVS_SRC_19_R_VMAP2_HW        (DPE_BASE_HW + 0x14C)
#define DVS_SRC_20_R_VMAP3_HW        (DPE_BASE_HW + 0x150)
#define DVS_SRC_21_INTER_MEDV_HW     (DPE_BASE_HW + 0x154)
#define DVS_SRC_22_MEDV0_HW          (DPE_BASE_HW + 0x158)
#define DVS_SRC_23_MEDV1_HW          (DPE_BASE_HW + 0x15C)
#define DVS_SRC_24_MEDV2_HW          (DPE_BASE_HW + 0x160)
#define DVS_SRC_25_MEDV3_HW          (DPE_BASE_HW + 0x164)
#define DVS_SRC_26_OCCDV0_HW         (DPE_BASE_HW + 0x168)
#define DVS_SRC_27_OCCDV1_HW         (DPE_BASE_HW + 0x16C)
#define DVS_SRC_28_OCCDV2_HW         (DPE_BASE_HW + 0x170)
#define DVS_SRC_29_OCCDV3_HW         (DPE_BASE_HW + 0x174)
#define DVS_SRC_30_DCV_CONF0_HW      (DPE_BASE_HW + 0x178)
#define DVS_SRC_31_DCV_CONF1_HW      (DPE_BASE_HW + 0x17C)
#define DVS_SRC_32_DCV_CONF2_HW      (DPE_BASE_HW + 0x180)
#define DVS_SRC_33_DCV_CONF3_HW      (DPE_BASE_HW + 0x184)
#define DVS_SRC_34_DCV_L_FRM0_HW     (DPE_BASE_HW + 0x188)
#define DVS_SRC_35_DCV_L_FRM1_HW     (DPE_BASE_HW + 0x18C)
#define DVS_SRC_36_DCV_L_FRM2_HW     (DPE_BASE_HW + 0x190)
#define DVS_SRC_37_DCV_L_FRM3_HW     (DPE_BASE_HW + 0x194)
#define DVS_SRC_38_DCV_R_FRM0_HW     (DPE_BASE_HW + 0x198)
#define DVS_SRC_39_DCV_R_FRM1_HW     (DPE_BASE_HW + 0x19C)
#define DVS_SRC_40_DCV_R_FRM2_HW     (DPE_BASE_HW + 0x1A0)
#define DVS_SRC_41_DCV_R_FRM3_HW     (DPE_BASE_HW + 0x1A4)
#define DVS_SRC_42_OCCDV_EXT0_HW     (DPE_BASE_HW + 0x1A8)
#define DVS_SRC_43_OCCDV_EXT1_HW     (DPE_BASE_HW + 0x1AC)
#define DVS_SRC_44_OCCDV_EXT2_HW     (DPE_BASE_HW + 0x1B0)
#define DVS_SRC_45_OCCDV_EXT3_HW     (DPE_BASE_HW + 0x1B4)
#define DVS_CRC_OUT_0_HW             (DPE_BASE_HW + 0x1C0)
#define DVS_CRC_OUT_1_HW             (DPE_BASE_HW + 0x1C4)
#define DVS_CRC_OUT_2_HW             (DPE_BASE_HW + 0x1C8)
#define DVS_CRC_OUT_3_HW             (DPE_BASE_HW + 0x1CC)
#define DVS_CTRL_RESERVED_HW         (DPE_BASE_HW + 0x2F8)
#define DVS_CTRL_ATPG_HW             (DPE_BASE_HW + 0x2FC)
#define DVS_ME_00_HW                 (DPE_BASE_HW + 0x300)
#define DVS_ME_01_HW                 (DPE_BASE_HW + 0x304)
#define DVS_ME_02_HW                 (DPE_BASE_HW + 0x308)
#define DVS_ME_03_HW                 (DPE_BASE_HW + 0x30C)
#define DVS_ME_04_HW                 (DPE_BASE_HW + 0x310)
#define DVS_ME_05_HW                 (DPE_BASE_HW + 0x314)
#define DVS_ME_06_HW                 (DPE_BASE_HW + 0x318)
#define DVS_ME_07_HW                 (DPE_BASE_HW + 0x31C)
#define DVS_ME_08_HW                 (DPE_BASE_HW + 0x320)
#define DVS_ME_09_HW                 (DPE_BASE_HW + 0x324)
#define DVS_ME_10_HW                 (DPE_BASE_HW + 0x328)
#define DVS_ME_11_HW                 (DPE_BASE_HW + 0x32C)
#define DVS_ME_12_HW                 (DPE_BASE_HW + 0x330)
#define DVS_ME_13_HW                 (DPE_BASE_HW + 0x334)
#define DVS_ME_14_HW                 (DPE_BASE_HW + 0x338)
#define DVS_ME_15_HW                 (DPE_BASE_HW + 0x33C)
#define DVS_ME_16_HW                 (DPE_BASE_HW + 0x340)
#define DVS_ME_17_HW                 (DPE_BASE_HW + 0x344)
#define DVS_ME_18_HW                 (DPE_BASE_HW + 0x348)
#define DVS_ME_19_HW                 (DPE_BASE_HW + 0x34C)
#define DVS_ME_20_HW                 (DPE_BASE_HW + 0x350)
#define DVS_ME_21_HW                 (DPE_BASE_HW + 0x354)
#define DVS_ME_22_HW                 (DPE_BASE_HW + 0x358)
#define DVS_ME_23_HW                 (DPE_BASE_HW + 0x35C)
#define DVS_ME_24_HW                 (DPE_BASE_HW + 0x360)
#define DVS_ME_25_HW                 (DPE_BASE_HW + 0x364)
#define DVS_ME_26_HW                 (DPE_BASE_HW + 0x368)
#define DVS_ME_27_HW                 (DPE_BASE_HW + 0x36C)
#define DVS_ME_28_HW                 (DPE_BASE_HW + 0x370)
#define DVS_ME_29_HW                 (DPE_BASE_HW + 0x374)
#define DVS_ME_30_HW                 (DPE_BASE_HW + 0x378)
#define DVS_ME_31_HW                 (DPE_BASE_HW + 0x37C)
#define DVS_ME_32_HW                 (DPE_BASE_HW + 0x380)
#define DVS_ME_33_HW                 (DPE_BASE_HW + 0x384)
#define DVS_ME_34_HW                 (DPE_BASE_HW + 0x388)
#define DVS_ME_35_HW                 (DPE_BASE_HW + 0x38C)
#define DVS_ME_36_HW                 (DPE_BASE_HW + 0x390)
#define DVS_STATUS_00_HW             (DPE_BASE_HW + 0x3E0)
#define DVS_STATUS_01_HW             (DPE_BASE_HW + 0x3E4)
#define DVS_STATUS_02_HW             (DPE_BASE_HW + 0x3E8)
#define DVS_STATUS_03_HW             (DPE_BASE_HW + 0x3EC)
#define DVS_DEBUG_HW                 (DPE_BASE_HW + 0x3F4)
#define DVS_ME_RESERVED_HW           (DPE_BASE_HW + 0x3F8)
#define DVS_ME_ATPG_HW               (DPE_BASE_HW + 0x3FC)

#define DVP_CTRL00_HW                (DPE_BASE_HW + 0x800)
#define DVP_CTRL01_HW                (DPE_BASE_HW + 0x804)
#define DVP_CTRL02_HW                (DPE_BASE_HW + 0x808)
#define DVP_CTRL03_HW                (DPE_BASE_HW + 0x80C)
#define DVP_CTRL04_HW                (DPE_BASE_HW + 0x810)
#define DVP_CTRL05_HW                (DPE_BASE_HW + 0x814)
#define DVP_CTRL06_HW                (DPE_BASE_HW + 0x818)
#define DVP_CTRL07_HW                (DPE_BASE_HW + 0x81C)
#define DVP_IRQ_00_HW                (DPE_BASE_HW + 0x840)
#define DVP_CTRL_STATUS0_HW          (DPE_BASE_HW + 0x850)
#define DVP_CTRL_STATUS1_HW          (DPE_BASE_HW + 0x854)
#define DVP_CTRL_STATUS2_HW          (DPE_BASE_HW + 0x858)
#define DVP_IRQ_STATUS_HW            (DPE_BASE_HW + 0x85C)
#define DVP_FRM_STATUS0_HW           (DPE_BASE_HW + 0x860)
#define DVP_FRM_STATUS1_HW           (DPE_BASE_HW + 0x864)
#define DVP_FRM_STATUS2_HW           (DPE_BASE_HW + 0x868)
#define DVP_FRM_STATUS3_HW           (DPE_BASE_HW + 0x86C)
#define DVP_CUR_STATUS_HW            (DPE_BASE_HW + 0x870)
#define DVP_SRC_00_HW                (DPE_BASE_HW + 0x880)
#define DVP_SRC_01_HW                (DPE_BASE_HW + 0x884)
#define DVP_SRC_02_HW                (DPE_BASE_HW + 0x888)
#define DVP_SRC_03_HW                (DPE_BASE_HW + 0x88C)
#define DVP_SRC_04_HW                (DPE_BASE_HW + 0x890)
#define DVP_SRC_05_Y_FRM0_HW         (DPE_BASE_HW + 0x894)
#define DVP_SRC_06_Y_FRM1_HW         (DPE_BASE_HW + 0x898)
#define DVP_SRC_07_Y_FRM2_HW         (DPE_BASE_HW + 0x89C)
#define DVP_SRC_08_Y_FRM3_HW         (DPE_BASE_HW + 0x8A0)
#define DVP_SRC_09_C_FRM0_HW         (DPE_BASE_HW + 0x8A4)
#define DVP_SRC_10_C_FRM1_HW         (DPE_BASE_HW + 0x8A8)
#define DVP_SRC_11_C_FRM2_HW         (DPE_BASE_HW + 0x8AC)
#define DVP_SRC_12_C_FRM3_HW         (DPE_BASE_HW + 0x8B0)
#define DVP_SRC_13_OCCDV0_HW         (DPE_BASE_HW + 0x8B4)
#define DVP_SRC_14_OCCDV1_HW         (DPE_BASE_HW + 0x8B8)
#define DVP_SRC_15_OCCDV2_HW         (DPE_BASE_HW + 0x8BC)
#define DVP_SRC_16_OCCDV3_HW         (DPE_BASE_HW + 0x8C0)
#define DVP_SRC_17_CRM_HW            (DPE_BASE_HW + 0x8C4)
#define DVP_SRC_18_ASF_RMDV_HW       (DPE_BASE_HW + 0x8C8)
#define DVP_SRC_19_ASF_RDDV_HW       (DPE_BASE_HW + 0x8CC)
#define DVP_SRC_20_ASF_DV0_HW        (DPE_BASE_HW + 0x8D0)
#define DVP_SRC_21_ASF_DV1_HW        (DPE_BASE_HW + 0x8D4)
#define DVP_SRC_22_ASF_DV2_HW        (DPE_BASE_HW + 0x8D8)
#define DVP_SRC_23_ASF_DV3_HW        (DPE_BASE_HW + 0x8DC)
#define DVP_SRC_24_WMF_HFDV_HW       (DPE_BASE_HW + 0x8E0)
#define DVP_SRC_25_WMF_DV0_HW        (DPE_BASE_HW + 0x8E4)
#define DVP_SRC_26_WMF_DV1_HW        (DPE_BASE_HW + 0x8E8)
#define DVP_SRC_27_WMF_DV2_HW        (DPE_BASE_HW + 0x8EC)
#define DVP_SRC_28_WMF_DV3_HW        (DPE_BASE_HW + 0x8F0)
#define DVP_CORE_00_HW               (DPE_BASE_HW + 0x900)
#define DVP_CORE_01_HW               (DPE_BASE_HW + 0x904)
#define DVP_CORE_02_HW               (DPE_BASE_HW + 0x908)
#define DVP_CORE_03_HW               (DPE_BASE_HW + 0x90C)
#define DVP_CORE_04_HW               (DPE_BASE_HW + 0x910)
#define DVP_CORE_05_HW               (DPE_BASE_HW + 0x914)
#define DVP_CORE_06_HW               (DPE_BASE_HW + 0x918)
#define DVP_CORE_07_HW               (DPE_BASE_HW + 0x91C)
#define DVP_CORE_08_HW               (DPE_BASE_HW + 0x920)
#define DVP_CORE_09_HW               (DPE_BASE_HW + 0x924)
#define DVP_CORE_10_HW               (DPE_BASE_HW + 0x928)
#define DVP_CORE_11_HW               (DPE_BASE_HW + 0x92C)
#define DVP_CORE_12_HW               (DPE_BASE_HW + 0x930)
#define DVP_CORE_13_HW               (DPE_BASE_HW + 0x934)
#define DVP_CORE_14_HW               (DPE_BASE_HW + 0x938)
#define DVP_CORE_15_HW               (DPE_BASE_HW + 0x93C)
#define DVP_CORE_16_HW               (DPE_BASE_HW + 0x940)
#define DVP_CORE_17_HW               (DPE_BASE_HW + 0x944)
#define DVP_CORE_18_HW               (DPE_BASE_HW + 0x948)
#define DVP_CORE_19_HW               (DPE_BASE_HW + 0x94C)
#define DVP_CORE_20_HW               (DPE_BASE_HW + 0x950)
#define DVP_CORE_21_HW               (DPE_BASE_HW + 0x954)
#define DVP_CORE_22_HW               (DPE_BASE_HW + 0x958)
#define DVP_SRC_CTRL_HW              (DPE_BASE_HW + 0x9F4)
#define DVP_CTRL_RESERVED_HW         (DPE_BASE_HW + 0x9F8)
#define DVP_CTRL_ATPG_HW             (DPE_BASE_HW + 0x9FC)
#define DVP_CRC_OUT_0_HW             (DPE_BASE_HW + 0xA00)
#define DVP_CRC_OUT_1_HW             (DPE_BASE_HW + 0xA04)
#define DVP_CRC_OUT_2_HW             (DPE_BASE_HW + 0xA08)
#define DVP_CRC_CTRL_HW              (DPE_BASE_HW + 0xA60)
#define DVP_CRC_OUT_HW               (DPE_BASE_HW + 0xA64)
#define DVP_CRC_IN_HW                (DPE_BASE_HW + 0xA6C)
#define DVP_DRAM_STA_HW              (DPE_BASE_HW + 0xA70)
#define DVP_DRAM_ULT_HW              (DPE_BASE_HW + 0xA74)
#define DVP_DRAM_PITCH_HW            (DPE_BASE_HW + 0xA78)
#define DVP_CORE_CRC_IN_HW           (DPE_BASE_HW + 0xA8C)
#define DVP_EXT_SRC_13_OCCDV0_HW     (DPE_BASE_HW + 0xBB4)
#define DVP_EXT_SRC_14_OCCDV1_HW     (DPE_BASE_HW + 0xBB8)
#define DVP_EXT_SRC_15_OCCDV2_HW     (DPE_BASE_HW + 0xBBC)
#define DVP_EXT_SRC_16_OCCDV3_HW     (DPE_BASE_HW + 0xBC0)
#define DVP_EXT_SRC_18_ASF_RMDV_HW   (DPE_BASE_HW + 0xBC8)
#define DVP_EXT_SRC_19_ASF_RDDV_HW   (DPE_BASE_HW + 0xBCC)
#define DVP_EXT_SRC_20_ASF_DV0_HW    (DPE_BASE_HW + 0xBD0)
#define DVP_EXT_SRC_21_ASF_DV1_HW    (DPE_BASE_HW + 0xBD4)
#define DVP_EXT_SRC_22_ASF_DV2_HW    (DPE_BASE_HW + 0xBD8)
#define DVP_EXT_SRC_23_ASF_DV3_HW    (DPE_BASE_HW + 0xBDC)

/*SW Access Registers : using mapped base address from DTS*/
#define DVS_CTRL00_REG               (ISP_DPE_BASE)
#define DVS_CTRL01_REG               (ISP_DPE_BASE + 0x004)
#define DVS_CTRL02_REG               (ISP_DPE_BASE + 0x008)
#define DVS_CTRL03_REG               (ISP_DPE_BASE + 0x00C)
#define DVS_CTRL04_REG               (ISP_DPE_BASE + 0x010)
#define DVS_CTRL05_REG               (ISP_DPE_BASE + 0x014)
#define DVS_CTRL06_REG               (ISP_DPE_BASE + 0x018)
#define DVS_CTRL07_REG               (ISP_DPE_BASE + 0x01C)
#define DVS_OCC_PQ_0_REG             (ISP_DPE_BASE + 0x030)
#define	DVS_OCC_PQ_1_REG             (ISP_DPE_BASE + 0x034)
#define	DVS_OCC_ATPG_REG             (ISP_DPE_BASE + 0x038)
#define DVS_IRQ_00_REG               (ISP_DPE_BASE + 0x040)
#define DVS_CTRL_STATUS0_REG         (ISP_DPE_BASE + 0x050)
#define DVS_CTRL_STATUS1_REG         (ISP_DPE_BASE + 0x054)
#define DVS_CTRL_STATUS2_REG         (ISP_DPE_BASE + 0x058)
#define DVS_IRQ_STATUS_REG           (ISP_DPE_BASE + 0x05C)
#define DVS_FRM_STATUS0_REG          (ISP_DPE_BASE + 0x060)
#define DVS_FRM_STATUS1_REG          (ISP_DPE_BASE + 0x064)
#define DVS_FRM_STATUS2_REG          (ISP_DPE_BASE + 0x068)
#define DVS_FRM_STATUS3_REG          (ISP_DPE_BASE + 0x06C)
#define DVS_CUR_STATUS_REG           (ISP_DPE_BASE + 0x070)
#define DVS_SRC_CTRL_REG             (ISP_DPE_BASE + 0x074)
#define DVS_CRC_CTRL_REG             (ISP_DPE_BASE + 0x080)
#define DVS_CRC_IN_REG               (ISP_DPE_BASE + 0x08C)
#define DVS_DRAM_STA0_REG            (ISP_DPE_BASE + 0x090)
#define DVS_DRAM_STA1_REG            (ISP_DPE_BASE + 0x094)
#define DVS_DRAM_ULT_REG             (ISP_DPE_BASE + 0x098)
#define DVS_DRAM_PITCH_REG           (ISP_DPE_BASE + 0x09C)
#define DVS_SRC_00_REG               (ISP_DPE_BASE + 0x100)
#define DVS_SRC_01_REG               (ISP_DPE_BASE + 0x104)
#define DVS_SRC_02_REG               (ISP_DPE_BASE + 0x108)
#define DVS_SRC_03_REG               (ISP_DPE_BASE + 0x10C)
#define DVS_SRC_04_REG               (ISP_DPE_BASE + 0x110)
#define DVS_SRC_05_L_FRM0_REG        (ISP_DPE_BASE + 0x114)
#define DVS_SRC_06_L_FRM1_REG        (ISP_DPE_BASE + 0x118)
#define DVS_SRC_07_L_FRM2_REG        (ISP_DPE_BASE + 0x11C)
#define DVS_SRC_08_L_FRM3_REG        (ISP_DPE_BASE + 0x120)
#define DVS_SRC_09_R_FRM0_REG        (ISP_DPE_BASE + 0x124)
#define DVS_SRC_10_R_FRM1_REG        (ISP_DPE_BASE + 0x128)
#define DVS_SRC_11_R_FRM2_REG        (ISP_DPE_BASE + 0x12C)
#define DVS_SRC_12_R_FRM3_REG        (ISP_DPE_BASE + 0x130)
#define DVS_SRC_13_L_VMAP0_REG       (ISP_DPE_BASE + 0x134)
#define DVS_SRC_14_L_VMAP1_REG       (ISP_DPE_BASE + 0x138)
#define DVS_SRC_15_L_VMAP2_REG       (ISP_DPE_BASE + 0x13C)
#define DVS_SRC_16_L_VMAP3_REG       (ISP_DPE_BASE + 0x140)
#define DVS_SRC_17_R_VMAP0_REG       (ISP_DPE_BASE + 0x144)
#define DVS_SRC_18_R_VMAP1_REG       (ISP_DPE_BASE + 0x148)
#define DVS_SRC_19_R_VMAP2_REG       (ISP_DPE_BASE + 0x14C)
#define DVS_SRC_20_R_VMAP3_REG       (ISP_DPE_BASE + 0x150)
#define DVS_SRC_21_INTER_MEDV_REG    (ISP_DPE_BASE + 0x154)
#define DVS_SRC_22_MEDV0_REG         (ISP_DPE_BASE + 0x158)
#define DVS_SRC_23_MEDV1_REG         (ISP_DPE_BASE + 0x15C)
#define DVS_SRC_24_MEDV2_REG         (ISP_DPE_BASE + 0x160)
#define DVS_SRC_25_MEDV3_REG         (ISP_DPE_BASE + 0x164)
#define DVS_SRC_26_OCCDV0_REG        (ISP_DPE_BASE + 0x168)
#define DVS_SRC_27_OCCDV1_REG        (ISP_DPE_BASE + 0x16C)
#define DVS_SRC_28_OCCDV2_REG        (ISP_DPE_BASE + 0x170)
#define DVS_SRC_29_OCCDV3_REG        (ISP_DPE_BASE + 0x174)
#define DVS_SRC_30_DCV_CONF0_REG     (ISP_DPE_BASE + 0x178)
#define DVS_SRC_31_DCV_CONF1_REG     (ISP_DPE_BASE + 0x17C)
#define DVS_SRC_32_DCV_CONF2_REG     (ISP_DPE_BASE + 0x180)
#define DVS_SRC_33_DCV_CONF3_REG     (ISP_DPE_BASE + 0x184)
#define DVS_SRC_34_DCV_L_FRM0_REG    (ISP_DPE_BASE + 0x188)
#define DVS_SRC_35_DCV_L_FRM1_REG    (ISP_DPE_BASE + 0x18C)
#define DVS_SRC_36_DCV_L_FRM2_REG    (ISP_DPE_BASE + 0x190)
#define DVS_SRC_37_DCV_L_FRM3_REG    (ISP_DPE_BASE + 0x194)
#define DVS_SRC_38_DCV_R_FRM0_REG    (ISP_DPE_BASE + 0x198)
#define DVS_SRC_39_DCV_R_FRM1_REG    (ISP_DPE_BASE + 0x19C)
#define DVS_SRC_40_DCV_R_FRM2_REG    (ISP_DPE_BASE + 0x1A0)
#define DVS_SRC_41_DCV_R_FRM3_REG    (ISP_DPE_BASE + 0x1A4)
#define DVS_SRC_42_OCCDV_EXT0_REG    (ISP_DPE_BASE + 0x1A8)
#define DVS_SRC_43_OCCDV_EXT1_REG    (ISP_DPE_BASE + 0x1AC)
#define DVS_SRC_44_OCCDV_EXT2_REG    (ISP_DPE_BASE + 0x1B0)
#define DVS_SRC_45_OCCDV_EXT3_REG    (ISP_DPE_BASE + 0x1B4)
#define DVS_CRC_OUT_0_REG            (ISP_DPE_BASE + 0x1C0)
#define DVS_CRC_OUT_1_REG            (ISP_DPE_BASE + 0x1C4)
#define DVS_CRC_OUT_2_REG            (ISP_DPE_BASE + 0x1C8)
#define DVS_CRC_OUT_3_REG            (ISP_DPE_BASE + 0x1CC)
#define DVS_CTRL_RESERVED_REG        (ISP_DPE_BASE + 0x2F8)
#define DVS_CTRL_ATPG_REG            (ISP_DPE_BASE + 0x2FC)
#define DVS_ME_00_REG                (ISP_DPE_BASE + 0x300)
#define DVS_ME_01_REG                (ISP_DPE_BASE + 0x304)
#define DVS_ME_02_REG                (ISP_DPE_BASE + 0x308)
#define DVS_ME_03_REG                (ISP_DPE_BASE + 0x30C)
#define DVS_ME_04_REG                (ISP_DPE_BASE + 0x310)
#define DVS_ME_05_REG                (ISP_DPE_BASE + 0x314)
#define DVS_ME_06_REG                (ISP_DPE_BASE + 0x318)
#define DVS_ME_07_REG                (ISP_DPE_BASE + 0x31C)
#define DVS_ME_08_REG                (ISP_DPE_BASE + 0x320)
#define DVS_ME_09_REG                (ISP_DPE_BASE + 0x324)
#define DVS_ME_10_REG                (ISP_DPE_BASE + 0x328)
#define DVS_ME_11_REG                (ISP_DPE_BASE + 0x32C)
#define DVS_ME_12_REG                (ISP_DPE_BASE + 0x330)
#define DVS_ME_13_REG                (ISP_DPE_BASE + 0x334)
#define DVS_ME_14_REG                (ISP_DPE_BASE + 0x338)
#define DVS_ME_15_REG                (ISP_DPE_BASE + 0x33C)
#define DVS_ME_16_REG                (ISP_DPE_BASE + 0x340)
#define DVS_ME_17_REG                (ISP_DPE_BASE + 0x344)
#define DVS_ME_18_REG                (ISP_DPE_BASE + 0x348)
#define DVS_ME_19_REG                (ISP_DPE_BASE + 0x34C)
#define DVS_ME_20_REG                (ISP_DPE_BASE + 0x350)
#define DVS_ME_21_REG                (ISP_DPE_BASE + 0x354)
#define DVS_ME_22_REG                (ISP_DPE_BASE + 0x358)
#define DVS_ME_23_REG                (ISP_DPE_BASE + 0x35C)
#define DVS_ME_24_REG                (ISP_DPE_BASE + 0x360)
#define DVS_ME_25_REG                (ISP_DPE_BASE + 0x364)
#define DVS_ME_26_REG                (ISP_DPE_BASE + 0x368)
#define DVS_ME_27_REG                (ISP_DPE_BASE + 0x36C)
#define DVS_ME_28_REG                (ISP_DPE_BASE + 0x370)
#define DVS_ME_29_REG                (ISP_DPE_BASE + 0x374)
#define DVS_ME_30_REG                (ISP_DPE_BASE + 0x378)
#define DVS_ME_31_REG                (ISP_DPE_BASE + 0x37C)
#define DVS_ME_32_REG                (ISP_DPE_BASE + 0x380)
#define DVS_ME_33_REG                (ISP_DPE_BASE + 0x384)
#define DVS_ME_34_REG                (ISP_DPE_BASE + 0x388)
#define DVS_ME_35_REG                (ISP_DPE_BASE + 0x38C)
#define DVS_ME_36_REG                (ISP_DPE_BASE + 0x390)
#define DVS_STATUS_00_REG            (ISP_DPE_BASE + 0x3E0)
#define DVS_STATUS_01_REG            (ISP_DPE_BASE + 0x3E4)
#define DVS_STATUS_02_REG            (ISP_DPE_BASE + 0x3E8)
#define DVS_STATUS_03_REG            (ISP_DPE_BASE + 0x3EC)
#define DVS_DEBUG_REG                (ISP_DPE_BASE + 0x3F4)
#define DVS_ME_RESERVED_REG          (ISP_DPE_BASE + 0x3F8)
#define DVS_ME_ATPG_REG              (ISP_DPE_BASE + 0x3FC)

#define DVP_CTRL00_REG               (ISP_DPE_BASE + 0x800)
#define DVP_CTRL01_REG               (ISP_DPE_BASE + 0x804)
#define DVP_CTRL02_REG               (ISP_DPE_BASE + 0x808)
#define DVP_CTRL03_REG               (ISP_DPE_BASE + 0x80C)
#define DVP_CTRL04_REG               (ISP_DPE_BASE + 0x810)
#define DVP_CTRL05_REG               (ISP_DPE_BASE + 0x814)
#define DVP_CTRL06_REG               (ISP_DPE_BASE + 0x818)
#define DVP_CTRL07_REG               (ISP_DPE_BASE + 0x81C)
#define DVP_IRQ_00_REG               (ISP_DPE_BASE + 0x840)
#define DVP_CTRL_STATUS0_REG         (ISP_DPE_BASE + 0x850)
#define DVP_CTRL_STATUS1_REG         (ISP_DPE_BASE + 0x854)
#define DVP_CTRL_STATUS2_REG         (ISP_DPE_BASE + 0x858)
#define DVP_IRQ_STATUS_REG           (ISP_DPE_BASE + 0x85C)
#define DVP_FRM_STATUS0_REG          (ISP_DPE_BASE + 0x860)
#define DVP_FRM_STATUS1_REG          (ISP_DPE_BASE + 0x864)
#define DVP_FRM_STATUS2_REG          (ISP_DPE_BASE + 0x868)
#define DVP_FRM_STATUS3_REG          (ISP_DPE_BASE + 0x86C)
#define DVP_CUR_STATUS_REG           (ISP_DPE_BASE + 0x870)
#define DVP_SRC_00_REG               (ISP_DPE_BASE + 0x880)
#define DVP_SRC_01_REG               (ISP_DPE_BASE + 0x884)
#define DVP_SRC_02_REG               (ISP_DPE_BASE + 0x888)
#define DVP_SRC_03_REG               (ISP_DPE_BASE + 0x88C)
#define DVP_SRC_04_REG               (ISP_DPE_BASE + 0x890)
#define DVP_SRC_05_Y_FRM0_REG        (ISP_DPE_BASE + 0x894)
#define DVP_SRC_06_Y_FRM1_REG        (ISP_DPE_BASE + 0x898)
#define DVP_SRC_07_Y_FRM2_REG        (ISP_DPE_BASE + 0x89C)
#define DVP_SRC_08_Y_FRM3_REG        (ISP_DPE_BASE + 0x8A0)
#define DVP_SRC_09_C_FRM0_REG        (ISP_DPE_BASE + 0x8A4)
#define DVP_SRC_10_C_FRM1_REG        (ISP_DPE_BASE + 0x8A8)
#define DVP_SRC_11_C_FRM2_REG        (ISP_DPE_BASE + 0x8AC)
#define DVP_SRC_12_C_FRM3_REG        (ISP_DPE_BASE + 0x8B0)
#define DVP_SRC_13_OCCDV0_REG        (ISP_DPE_BASE + 0x8B4)
#define DVP_SRC_14_OCCDV1_REG        (ISP_DPE_BASE + 0x8B8)
#define DVP_SRC_15_OCCDV2_REG        (ISP_DPE_BASE + 0x8BC)
#define DVP_SRC_16_OCCDV3_REG        (ISP_DPE_BASE + 0x8C0)
#define DVP_SRC_17_CRM_REG           (ISP_DPE_BASE + 0x8C4)
#define DVP_SRC_18_ASF_RMDV_REG      (ISP_DPE_BASE + 0x8C8)
#define DVP_SRC_19_ASF_RDDV_REG      (ISP_DPE_BASE + 0x8CC)
#define DVP_SRC_20_ASF_DV0_REG       (ISP_DPE_BASE + 0x8D0)
#define DVP_SRC_21_ASF_DV1_REG       (ISP_DPE_BASE + 0x8D4)
#define DVP_SRC_22_ASF_DV2_REG       (ISP_DPE_BASE + 0x8D8)
#define DVP_SRC_23_ASF_DV3_REG       (ISP_DPE_BASE + 0x8DC)
#define DVP_SRC_24_WMF_HFDV_REG      (ISP_DPE_BASE + 0x8E0)
#define DVP_SRC_25_WMF_DV0_REG       (ISP_DPE_BASE + 0x8E4)
#define DVP_SRC_26_WMF_DV1_REG       (ISP_DPE_BASE + 0x8E8)
#define DVP_SRC_27_WMF_DV2_REG       (ISP_DPE_BASE + 0x8EC)
#define DVP_SRC_28_WMF_DV3_REG       (ISP_DPE_BASE + 0x8F0)
#define DVP_CORE_00_REG              (ISP_DPE_BASE + 0x900)
#define DVP_CORE_01_REG              (ISP_DPE_BASE + 0x904)
#define DVP_CORE_02_REG              (ISP_DPE_BASE + 0x908)
#define DVP_CORE_03_REG              (ISP_DPE_BASE + 0x90C)
#define DVP_CORE_04_REG              (ISP_DPE_BASE + 0x910)
#define DVP_CORE_05_REG              (ISP_DPE_BASE + 0x914)
#define DVP_CORE_06_REG              (ISP_DPE_BASE + 0x918)
#define DVP_CORE_07_REG              (ISP_DPE_BASE + 0x91C)
#define DVP_CORE_08_REG              (ISP_DPE_BASE + 0x920)
#define DVP_CORE_09_REG              (ISP_DPE_BASE + 0x924)
#define DVP_CORE_10_REG              (ISP_DPE_BASE + 0x928)
#define DVP_CORE_11_REG              (ISP_DPE_BASE + 0x92C)
#define DVP_CORE_12_REG              (ISP_DPE_BASE + 0x930)
#define DVP_CORE_13_REG              (ISP_DPE_BASE + 0x934)
#define DVP_CORE_14_REG              (ISP_DPE_BASE + 0x938)
#define DVP_CORE_15_REG              (ISP_DPE_BASE + 0x93C)
#define DVP_CORE_16_REG              (ISP_DPE_BASE + 0x940)
#define DVP_CORE_17_REG              (ISP_DPE_BASE + 0x944)
#define DVP_CORE_18_REG              (ISP_DPE_BASE + 0x948)
#define DVP_CORE_19_REG              (ISP_DPE_BASE + 0x94C)
#define DVP_CORE_20_REG              (ISP_DPE_BASE + 0x950)
#define DVP_CORE_21_REG              (ISP_DPE_BASE + 0x954)
#define DVP_CORE_22_REG              (ISP_DPE_BASE + 0x958)
#define DVP_SRC_CTRL_REG             (ISP_DPE_BASE + 0x9F4)
#define DVP_CTRL_RESERVED_REG        (ISP_DPE_BASE + 0x9F8)
#define DVP_CTRL_ATPG_REG            (ISP_DPE_BASE + 0x9FC)
#define DVP_CRC_OUT_0_REG            (ISP_DPE_BASE + 0xA00)
#define DVP_CRC_OUT_1_REG            (ISP_DPE_BASE + 0xA04)
#define DVP_CRC_OUT_2_REG            (ISP_DPE_BASE + 0xA08)
#define DVP_CRC_CTRL_REG             (ISP_DPE_BASE + 0xA60)
#define DVP_CRC_OUT_REG              (ISP_DPE_BASE + 0xA64)
#define DVP_CRC_IN_REG               (ISP_DPE_BASE + 0xA6C)
#define DVP_DRAM_STA_REG             (ISP_DPE_BASE + 0xA70)
#define DVP_DRAM_ULT_REG             (ISP_DPE_BASE + 0xA74)
#define DVP_DRAM_PITCH_REG           (ISP_DPE_BASE + 0xA78)
#define DVP_CORE_CRC_IN_REG          (ISP_DPE_BASE + 0xA8C)
#define DVP_EXT_SRC_13_OCCDV0_REG    (ISP_DPE_BASE + 0xBB4)
#define DVP_EXT_SRC_14_OCCDV1_REG    (ISP_DPE_BASE + 0xBB8)
#define DVP_EXT_SRC_15_OCCDV2_REG    (ISP_DPE_BASE + 0xBBC)
#define DVP_EXT_SRC_16_OCCDV3_REG    (ISP_DPE_BASE + 0xBC0)
#define DVP_EXT_SRC_18_ASF_RMDV_REG  (ISP_DPE_BASE + 0xBC8)
#define DVP_EXT_SRC_19_ASF_RDDV_REG  (ISP_DPE_BASE + 0xBCC)
#define DVP_EXT_SRC_20_ASF_DV0_REG   (ISP_DPE_BASE + 0xBD0)
#define DVP_EXT_SRC_21_ASF_DV1_REG   (ISP_DPE_BASE + 0xBD4)
#define DVP_EXT_SRC_22_ASF_DV2_REG   (ISP_DPE_BASE + 0xBD8)
#define DVP_EXT_SRC_23_ASF_DV3_REG   (ISP_DPE_BASE + 0xBDC)

#define DPE_MAX_REG_CNT              (0xBE0 >> 2)
/**************************************************************
 *
 **************************************************************/
static inline unsigned int DPE_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

/**************************************************************
 *
 **************************************************************/
static inline unsigned int DPE_UsToJiffies(unsigned int Us)
{
	return (((Us / 1000) * HZ + 512) >> 10);
}

/**************************************************************
 *
 **************************************************************/
static inline unsigned int
DPE_GetIRQState(unsigned int type, unsigned int userNumber, unsigned int stus,
		enum DPE_PROCESS_ID_ENUM whichReq, int ProcessID)
{
	unsigned int ret = 0;
	unsigned long flags;

	/*  */
	spin_lock_irqsave(&(DPEInfo.SpinLockIrq[type]), flags);

	if (stus & DPE_INT_ST) {
		ret = ((DPEInfo.IrqInfo.DpeIrqCnt > 0) &&
		       (DPEInfo.IrqInfo.ProcessID[whichReq] == ProcessID));
	} else {
		LOG_ERR("EWIRQ,type:%d,u:%d,stat:%d,wReq:%d,PID:0x%x\n",
			type, userNumber, stus, whichReq, ProcessID);
	}
	spin_unlock_irqrestore(&(DPEInfo.SpinLockIrq[type]), flags);
	/*  */
	return ret;
}


/**************************************************************
 *
 **************************************************************/
static inline unsigned int DPE_JiffiesToMs(unsigned int Jiffies)
{
	return ((Jiffies * 1000) / HZ);
}

#define RegDump(start, end)                                                \
{                                                                          \
unsigned int i;                                                            \
for (i = start; i <= end; i += 0x10) {                                     \
	LOG_DBG("[0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]", \
		(unsigned int)(ISP_DPE_BASE + i),                          \
		(unsigned int)DPE_RD32(ISP_DPE_BASE + i),                  \
		(unsigned int)(ISP_DPE_BASE + i + 0x4),                    \
		(unsigned int)DPE_RD32(ISP_DPE_BASE + i +                  \
				       0x4),                               \
		(unsigned int)(ISP_DPE_BASE + i + 0x8),                    \
		(unsigned int)DPE_RD32(ISP_DPE_BASE + i +                  \
				       0x8),                               \
		(unsigned int)(ISP_DPE_BASE + i + 0xc),                    \
		(unsigned int)DPE_RD32(ISP_DPE_BASE + i +                  \
				       0xc));                              \
}                                                                          \
}

signed int dpe_enque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt;
	/*TODO: define engine request struct */
	struct DPE_Request *_req;
	struct DPE_Config *pDpeConfig;

	_req = (struct DPE_Request *) req;

	if (frames == NULL || _req == NULL)
		return -1;

	/*TODO: m_ReqNum is FrmNum; FIFO only thus f starts from 0 */
	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		memcpy(frames[f].data, &_req->m_pDpeConfig[f],
						sizeof(struct DPE_Config));

		pDpeConfig = &_req->m_pDpeConfig[f];
	}

	return 0;
}

signed int dpe_deque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt;
	struct DPE_Request *_req;
	struct DPE_Config *pDpeConfig;

	_req = (struct DPE_Request *) req;

	if (frames == NULL || _req == NULL)
		return -1;

	/*TODO: m_ReqNum is FrmNum; FIFO only thus f starts from 0 */
	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		memcpy(&_req->m_pDpeConfig[f], frames[f].data,
						sizeof(struct DPE_Config));
		LOG_DBG("[%s]request dequeued frame(%d/%d).", __func__, f,
									fcnt);

		pDpeConfig = &_req->m_pDpeConfig[f];
	}

	return 0;
}

#ifdef DPE_PMQOS
void DPEQOS_Init(void)
{
	s32 result = 0;
	u64 dpe_freq_steps[MAX_FREQ_STEP];
	u32 step_size;

	/* Call pm_qos_add_request when initialize module or driver prob */
	pm_qos_add_request(
		&dpe_qos_request,
		PM_QOS_DPE_FREQ,
		PM_QOS_MM_FREQ_DEFAULT_VALUE);

	/* Call mmdvfs_qos_get_freq_steps to get supported frequency */
	result = mmdvfs_qos_get_freq_steps(
		PM_QOS_DPE_FREQ,
		dpe_freq_steps,
		&step_size);

	if (result < 0 || step_size == 0)
		LOG_INF("get MMDVFS freq steps failed, result: %d\n", result);
	else {
		max_dpe_freq = dpe_freq_steps[0];
		mid_dpe_freq = dpe_freq_steps[1];
	}
}

void DPEQOS_Uninit(void)
{
	pm_qos_remove_request(&dpe_qos_request);
}

void DPEQOS_UpdateDpeFreq(bool start, unsigned int vopp)
{
	/* start DPE, configure MMDVFS to highest CLK */
	if (start) {
		if (vopp == 0)
			pm_qos_update_request(&dpe_qos_request, max_dpe_freq);
		else
			pm_qos_update_request(&dpe_qos_request, mid_dpe_freq);
	} else /* finish DPE, config MMDVFS to lowest CLK */
		pm_qos_update_request(&dpe_qos_request, 0);
}
#endif

void DPE_DumpUserSpaceReg(struct DPE_Config *pDpeConfig)
{
	LOG_INF("DPE Config From User Space\n");
	LOG_INF("DVS_CTRL00 = 0x%08X\n", pDpeConfig->DVS_CTRL00);
	LOG_INF("DVS_CTRL01 = 0x%08X\n", pDpeConfig->DVS_CTRL01);
	LOG_INF("DVS_CTRL02 = 0x%08X\n", pDpeConfig->DVS_CTRL02);
	LOG_INF("DVS_CTRL03 = 0x%08X\n", pDpeConfig->DVS_CTRL03);
	LOG_INF("DVS_CTRL06 = 0x%08X\n", pDpeConfig->DVS_CTRL06);
	LOG_INF("DVS_CTRL07 = 0x%08X\n", pDpeConfig->DVS_CTRL07);
	LOG_INF("DVS_IRQ_00 = 0x%08X\n", pDpeConfig->DVS_IRQ_00);
	LOG_INF("DVS_SRC_CTRL = 0x%08X\n", pDpeConfig->DVS_SRC_CTRL);
	LOG_INF("DVS_CRC_CTRL = 0x%08X\n", pDpeConfig->DVS_CRC_CTRL);
	LOG_INF("DVS_DRAM_ULT = 0x%08X\n", pDpeConfig->DVS_DRAM_ULT);
	LOG_INF("DVS_DRAM_PITCH = 0x%08X\n", pDpeConfig->DVS_DRAM_PITCH);
	LOG_INF("DVS_SRC_00 = 0x%08X\n", pDpeConfig->DVS_SRC_00);
	LOG_INF("DVS_SRC_01 = 0x%08X\n", pDpeConfig->DVS_SRC_01);
	LOG_INF("DVS_SRC_02 = 0x%08X\n", pDpeConfig->DVS_SRC_02);
	LOG_INF("DVS_SRC_03 = 0x%08X\n", pDpeConfig->DVS_SRC_03);
	LOG_INF("DVS_SRC_04 = 0x%08X\n", pDpeConfig->DVS_SRC_04);
	LOG_INF("DVS_SRC_05 = 0x%08X\n", pDpeConfig->DVS_SRC_05_L_FRM0);
	LOG_INF("DVS_SRC_06 = 0x%08X\n", pDpeConfig->DVS_SRC_06_L_FRM1);
	LOG_INF("DVS_SRC_07 = 0x%08X\n", pDpeConfig->DVS_SRC_07_L_FRM2);
	LOG_INF("DVS_SRC_08 = 0x%08X\n", pDpeConfig->DVS_SRC_08_L_FRM3);
	LOG_INF("DVS_SRC_09 = 0x%08X\n", pDpeConfig->DVS_SRC_09_R_FRM0);
	LOG_INF("DVS_SRC_10 = 0x%08X\n", pDpeConfig->DVS_SRC_10_R_FRM1);
	LOG_INF("DVS_SRC_11 = 0x%08X\n", pDpeConfig->DVS_SRC_11_R_FRM2);
	LOG_INF("DVS_SRC_12 = 0x%08X\n", pDpeConfig->DVS_SRC_12_R_FRM3);
	LOG_INF("DVS_SRC_13 = 0x%08X\n", pDpeConfig->DVS_SRC_13_L_VMAP0);
	LOG_INF("DVS_SRC_14 = 0x%08X\n", pDpeConfig->DVS_SRC_14_L_VMAP1);
	LOG_INF("DVS_SRC_15 = 0x%08X\n", pDpeConfig->DVS_SRC_15_L_VMAP2);
	LOG_INF("DVS_SRC_16 = 0x%08X\n", pDpeConfig->DVS_SRC_16_L_VMAP3);
	LOG_INF("DVS_SRC_17 = 0x%08X\n", pDpeConfig->DVS_SRC_17_R_VMAP0);
	LOG_INF("DVS_SRC_18 = 0x%08X\n", pDpeConfig->DVS_SRC_18_R_VMAP1);
	LOG_INF("DVS_SRC_19 = 0x%08X\n", pDpeConfig->DVS_SRC_19_R_VMAP2);
	LOG_INF("DVS_SRC_20 = 0x%08X\n", pDpeConfig->DVS_SRC_20_R_VMAP3);
	LOG_INF("DVS_SRC_21 = 0x%08X\n", pDpeConfig->DVS_SRC_21_INTER_MEDV);
	LOG_INF("DVS_SRC_22 = 0x%08X\n", pDpeConfig->DVS_SRC_22_MEDV0);
	LOG_INF("DVS_SRC_23 = 0x%08X\n", pDpeConfig->DVS_SRC_23_MEDV1);
	LOG_INF("DVS_SRC_24 = 0x%08X\n", pDpeConfig->DVS_SRC_24_MEDV2);
	LOG_INF("DVS_SRC_25 = 0x%08X\n", pDpeConfig->DVS_SRC_25_MEDV3);
	LOG_INF("DVS_SRC_26 = 0x%08X\n", pDpeConfig->DVS_SRC_26_OCCDV0);
	LOG_INF("DVS_SRC_27 = 0x%08X\n", pDpeConfig->DVS_SRC_27_OCCDV1);
	LOG_INF("DVS_SRC_28 = 0x%08X\n", pDpeConfig->DVS_SRC_28_OCCDV2);
	LOG_INF("DVS_SRC_29 = 0x%08X\n", pDpeConfig->DVS_SRC_29_OCCDV3);
	LOG_INF("DVS_SRC_30 = 0x%08X\n", pDpeConfig->DVS_SRC_30_DCV_CONF0);
	LOG_INF("DVS_SRC_31 = 0x%08X\n", pDpeConfig->DVS_SRC_31_DCV_CONF1);
	LOG_INF("DVS_SRC_32 = 0x%08X\n", pDpeConfig->DVS_SRC_32_DCV_CONF2);
	LOG_INF("DVS_SRC_33 = 0x%08X\n", pDpeConfig->DVS_SRC_33_DCV_CONF3);
	LOG_INF("DVS_SRC_34 = 0x%08X\n", pDpeConfig->DVS_SRC_34_DCV_L_FRM0);
	LOG_INF("DVS_SRC_35 = 0x%08X\n", pDpeConfig->DVS_SRC_35_DCV_L_FRM1);
	LOG_INF("DVS_SRC_36 = 0x%08X\n", pDpeConfig->DVS_SRC_36_DCV_L_FRM2);
	LOG_INF("DVS_SRC_37 = 0x%08X\n", pDpeConfig->DVS_SRC_37_DCV_L_FRM3);
	LOG_INF("DVS_SRC_38 = 0x%08X\n", pDpeConfig->DVS_SRC_38_DCV_R_FRM0);
	LOG_INF("DVS_SRC_39 = 0x%08X\n", pDpeConfig->DVS_SRC_39_DCV_R_FRM1);
	LOG_INF("DVS_SRC_40 = 0x%08X\n", pDpeConfig->DVS_SRC_40_DCV_R_FRM2);
	LOG_INF("DVS_SRC_41 = 0x%08X\n", pDpeConfig->DVS_SRC_41_DCV_R_FRM3);
	LOG_INF("DVS_SRC_42 = 0x%08X\n", pDpeConfig->DVS_SRC_42_OCCDV_EXT0);
	LOG_INF("DVS_SRC_43 = 0x%08X\n", pDpeConfig->DVS_SRC_43_OCCDV_EXT1);
	LOG_INF("DVS_SRC_44 = 0x%08X\n", pDpeConfig->DVS_SRC_44_OCCDV_EXT2);
	LOG_INF("DVS_SRC_45 = 0x%08X\n", pDpeConfig->DVS_SRC_45_OCCDV_EXT3);

	LOG_INF("DVS_ME_00 = 0x%08X\n", pDpeConfig->DVS_ME_00);
	LOG_INF("DVS_ME_01 = 0x%08X\n", pDpeConfig->DVS_ME_01);
	LOG_INF("DVS_ME_02 = 0x%08X\n", pDpeConfig->DVS_ME_02);
	LOG_INF("DVS_ME_03 = 0x%08X\n", pDpeConfig->DVS_ME_03);
	LOG_INF("DVS_ME_04 = 0x%08X\n", pDpeConfig->DVS_ME_04);
	LOG_INF("DVS_ME_05 = 0x%08X\n", pDpeConfig->DVS_ME_05);
	LOG_INF("DVS_ME_06 = 0x%08X\n", pDpeConfig->DVS_ME_06);
	LOG_INF("DVS_ME_07 = 0x%08X\n", pDpeConfig->DVS_ME_07);
	LOG_INF("DVS_ME_08 = 0x%08X\n", pDpeConfig->DVS_ME_08);
	LOG_INF("DVS_ME_09 = 0x%08X\n", pDpeConfig->DVS_ME_09);
	LOG_INF("DVS_ME_10 = 0x%08X\n", pDpeConfig->DVS_ME_10);
	LOG_INF("DVS_ME_11 = 0x%08X\n", pDpeConfig->DVS_ME_11);
	LOG_INF("DVS_ME_12 = 0x%08X\n", pDpeConfig->DVS_ME_12);
	LOG_INF("DVS_ME_13 = 0x%08X\n", pDpeConfig->DVS_ME_13);
	LOG_INF("DVS_ME_14 = 0x%08X\n", pDpeConfig->DVS_ME_14);
	LOG_INF("DVS_ME_15 = 0x%08X\n", pDpeConfig->DVS_ME_15);
	LOG_INF("DVS_ME_16 = 0x%08X\n", pDpeConfig->DVS_ME_16);
	LOG_INF("DVS_ME_17 = 0x%08X\n", pDpeConfig->DVS_ME_17);
	LOG_INF("DVS_ME_18 = 0x%08X\n", pDpeConfig->DVS_ME_18);
	LOG_INF("DVS_ME_19 = 0x%08X\n", pDpeConfig->DVS_ME_19);
	LOG_INF("DVS_ME_20 = 0x%08X\n", pDpeConfig->DVS_ME_20);
	LOG_INF("DVS_ME_21 = 0x%08X\n", pDpeConfig->DVS_ME_21);
	LOG_INF("DVS_ME_22 = 0x%08X\n", pDpeConfig->DVS_ME_22);
	LOG_INF("DVS_ME_23 = 0x%08X\n", pDpeConfig->DVS_ME_23);
	LOG_INF("DVS_ME_24 = 0x%08X\n", pDpeConfig->DVS_ME_24);
	LOG_INF("DVS_ME_25 = 0x%08X\n", pDpeConfig->DVS_ME_25);
	LOG_INF("DVS_ME_26 = 0x%08X\n", pDpeConfig->DVS_ME_26);
	LOG_INF("DVS_ME_27 = 0x%08X\n", pDpeConfig->DVS_ME_27);
	LOG_INF("DVS_ME_28 = 0x%08X\n", pDpeConfig->DVS_ME_28);
	LOG_INF("DVS_ME_29 = 0x%08X\n", pDpeConfig->DVS_ME_29);
	LOG_INF("DVS_ME_30 = 0x%08X\n", pDpeConfig->DVS_ME_30);
	LOG_INF("DVS_ME_31 = 0x%08X\n", pDpeConfig->DVS_ME_31);
	LOG_INF("DVS_ME_32 = 0x%08X\n", pDpeConfig->DVS_ME_32);
	LOG_INF("DVS_ME_33 = 0x%08X\n", pDpeConfig->DVS_ME_33);
	LOG_INF("DVS_ME_34 = 0x%08X\n", pDpeConfig->DVS_ME_34);
	LOG_INF("DVS_ME_35 = 0x%08X\n", pDpeConfig->DVS_ME_35);
	LOG_INF("DVS_ME_36 = 0x%08X\n", pDpeConfig->DVS_ME_36);

	LOG_INF("DVS_OCC_PQ_0 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_0);
	LOG_INF("DVS_OCC_PQ_1 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_1);

	LOG_INF("DVP_CTRL00 = 0x%08X\n", pDpeConfig->DVP_CTRL00);
	LOG_INF("DVP_CTRL01 = 0x%08X\n", pDpeConfig->DVP_CTRL01);
	LOG_INF("DVP_CTRL02 = 0x%08X\n", pDpeConfig->DVP_CTRL02);
	LOG_INF("DVP_CTRL03 = 0x%08X\n", pDpeConfig->DVP_CTRL03);
	LOG_INF("DVP_CTRL04 = 0x%08X\n", pDpeConfig->DVP_CTRL04);
	LOG_INF("DVP_CTRL07 = 0x%08X\n", pDpeConfig->DVP_CTRL07);
	LOG_INF("DVP_IRQ_00 = 0x%08X\n", pDpeConfig->DVP_IRQ_00);
	LOG_INF("DVP_SRC_00 = 0x%08X\n", pDpeConfig->DVP_SRC_00);
	LOG_INF("DVP_SRC_01 = 0x%08X\n", pDpeConfig->DVP_SRC_01);
	LOG_INF("DVP_SRC_02 = 0x%08X\n", pDpeConfig->DVP_SRC_02);
	LOG_INF("DVP_SRC_03 = 0x%08X\n", pDpeConfig->DVP_SRC_03);
	LOG_INF("DVP_SRC_04 = 0x%08X\n", pDpeConfig->DVP_SRC_04);
	LOG_INF("DVP_SRC_05 = 0x%08X\n", pDpeConfig->DVP_SRC_05_Y_FRM0);
	LOG_INF("DVP_SRC_06 = 0x%08X\n", pDpeConfig->DVP_SRC_06_Y_FRM1);
	LOG_INF("DVP_SRC_07 = 0x%08X\n", pDpeConfig->DVP_SRC_07_Y_FRM2);
	LOG_INF("DVP_SRC_08 = 0x%08X\n", pDpeConfig->DVP_SRC_08_Y_FRM3);
	LOG_INF("DVP_SRC_09 = 0x%08X\n", pDpeConfig->DVP_SRC_09_C_FRM0);
	LOG_INF("DVP_SRC_10 = 0x%08X\n", pDpeConfig->DVP_SRC_10_C_FRM1);
	LOG_INF("DVP_SRC_11 = 0x%08X\n", pDpeConfig->DVP_SRC_11_C_FRM2);
	LOG_INF("DVP_SRC_12 = 0x%08X\n", pDpeConfig->DVP_SRC_12_C_FRM3);
	LOG_INF("DVP_SRC_13 = 0x%08X\n", pDpeConfig->DVP_SRC_13_OCCDV0);
	LOG_INF("DVP_SRC_14 = 0x%08X\n", pDpeConfig->DVP_SRC_14_OCCDV1);
	LOG_INF("DVP_SRC_15 = 0x%08X\n", pDpeConfig->DVP_SRC_15_OCCDV2);
	LOG_INF("DVP_SRC_16 = 0x%08X\n", pDpeConfig->DVP_SRC_16_OCCDV3);
	LOG_INF("DVP_SRC_17 = 0x%08X\n", pDpeConfig->DVP_SRC_17_CRM);
	LOG_INF("DVP_SRC_18 = 0x%08X\n", pDpeConfig->DVP_SRC_18_ASF_RMDV);
	LOG_INF("DVP_SRC_19 = 0x%08X\n", pDpeConfig->DVP_SRC_19_ASF_RDDV);
	LOG_INF("DVP_SRC_20 = 0x%08X\n", pDpeConfig->DVP_SRC_20_ASF_DV0);
	LOG_INF("DVP_SRC_21 = 0x%08X\n", pDpeConfig->DVP_SRC_21_ASF_DV1);
	LOG_INF("DVP_SRC_22 = 0x%08X\n", pDpeConfig->DVP_SRC_22_ASF_DV2);
	LOG_INF("DVP_SRC_23 = 0x%08X\n", pDpeConfig->DVP_SRC_23_ASF_DV3);
	LOG_INF("DVP_SRC_24 = 0x%08X\n", pDpeConfig->DVP_SRC_24_WMF_HFDV);
	LOG_INF("DVP_SRC_25 = 0x%08X\n", pDpeConfig->DVP_SRC_25_WMF_DV0);
	LOG_INF("DVP_SRC_26 = 0x%08X\n", pDpeConfig->DVP_SRC_26_WMF_DV1);
	LOG_INF("DVP_SRC_27 = 0x%08X\n", pDpeConfig->DVP_SRC_27_WMF_DV2);
	LOG_INF("DVP_SRC_28 = 0x%08X\n", pDpeConfig->DVP_SRC_28_WMF_DV3);
	LOG_INF("DVP_CORE_00 = 0x%08X\n", pDpeConfig->DVP_CORE_00);
	LOG_INF("DVP_CORE_01 = 0x%08X\n", pDpeConfig->DVP_CORE_01);
	LOG_INF("DVP_CORE_02 = 0x%08X\n", pDpeConfig->DVP_CORE_02);
	LOG_INF("DVP_CORE_03 = 0x%08X\n", pDpeConfig->DVP_CORE_03);
	LOG_INF("DVP_CORE_04 = 0x%08X\n", pDpeConfig->DVP_CORE_04);
	LOG_INF("DVP_CORE_05 = 0x%08X\n", pDpeConfig->DVP_CORE_05);
	LOG_INF("DVP_CORE_06 = 0x%08X\n", pDpeConfig->DVP_CORE_06);
	LOG_INF("DVP_CORE_07 = 0x%08X\n", pDpeConfig->DVP_CORE_07);
	LOG_INF("DVP_CORE_08 = 0x%08X\n", pDpeConfig->DVP_CORE_08);
	LOG_INF("DVP_CORE_09 = 0x%08X\n", pDpeConfig->DVP_CORE_09);
	LOG_INF("DVP_CORE_10 = 0x%08X\n", pDpeConfig->DVP_CORE_10);
	LOG_INF("DVP_CORE_11 = 0x%08X\n", pDpeConfig->DVP_CORE_11);
	LOG_INF("DVP_CORE_12 = 0x%08X\n", pDpeConfig->DVP_CORE_12);
	LOG_INF("DVP_CORE_13 = 0x%08X\n", pDpeConfig->DVP_CORE_13);
	LOG_INF("DVP_CORE_14 = 0x%08X\n", pDpeConfig->DVP_CORE_14);
	LOG_INF("DVP_CORE_15 = 0x%08X\n", pDpeConfig->DVP_CORE_15);
	LOG_INF("DVP_SRC_CTRL = 0x%08X\n", pDpeConfig->DVP_SRC_CTRL);
	LOG_INF("DVP_CRC_CTRL = 0x%08X\n", pDpeConfig->DVP_CRC_CTRL);
	LOG_INF("DVP_DRAM_ULT = 0x%08X\n", pDpeConfig->DVP_DRAM_ULT);
	LOG_INF("DVP_DRAM_PITCH = 0x%08X\n", pDpeConfig->DVP_DRAM_PITCH);
	LOG_INF("DVP_EXT_SRC_13_OCCDV0 = 0x%08X\n",
		pDpeConfig->DVP_EXT_SRC_13_OCCDV0);
	LOG_INF("DVP_EXT_SRC_14_OCCDV1 = 0x%08X\n",
		pDpeConfig->DVP_EXT_SRC_14_OCCDV1);
	LOG_INF("DVP_EXT_SRC_15_OCCDV2 = 0x%08X\n",
		pDpeConfig->DVP_EXT_SRC_15_OCCDV2);
	LOG_INF("DVP_EXT_SRC_16_OCCDV3 = 0x%08X\n",
		pDpeConfig->DVP_EXT_SRC_16_OCCDV3);
	LOG_INF("DVP_EXT_SRC_18_ASF_RMDV = 0x%08X\n",
		pDpeConfig->DVP_EXT_SRC_18_ASF_RMDV);
	LOG_INF("DVP_EXT_SRC_19_ASF_RDDV = 0x%08X\n",
		pDpeConfig->DVP_EXT_SRC_19_ASF_RDDV);
	LOG_INF("DVP_EXT_SRC_20_ASF_DV0 = 0x%08X\n",
		pDpeConfig->DVP_EXT_SRC_20_ASF_DV0);
	LOG_INF("DVP_EXT_SRC_21_ASF_DV1 = 0x%08X\n",
		pDpeConfig->DVP_EXT_SRC_21_ASF_DV1);
	LOG_INF("DVP_EXT_SRC_22_ASF_DV2 = 0x%08X\n",
		pDpeConfig->DVP_EXT_SRC_22_ASF_DV2);
	LOG_INF("DVP_EXT_SRC_23_ASF_DV3 = 0x%08X\n",
		pDpeConfig->DVP_EXT_SRC_23_ASF_DV3);

	LOG_INF("DVP_CORE_00 = 0x%08X\n", pDpeConfig->DVP_CORE_00);
	LOG_INF("DVP_CORE_01 = 0x%08X\n", pDpeConfig->DVP_CORE_01);
	LOG_INF("DVP_CORE_02 = 0x%08X\n", pDpeConfig->DVP_CORE_02);
	LOG_INF("DVP_CORE_03 = 0x%08X\n", pDpeConfig->DVP_CORE_03);
	LOG_INF("DVP_CORE_04 = 0x%08X\n", pDpeConfig->DVP_CORE_04);
	LOG_INF("DVP_CORE_05 = 0x%08X\n", pDpeConfig->DVP_CORE_05);
	LOG_INF("DVP_CORE_06 = 0x%08X\n", pDpeConfig->DVP_CORE_06);
	LOG_INF("DVP_CORE_07 = 0x%08X\n", pDpeConfig->DVP_CORE_07);
	LOG_INF("DVP_CORE_08 = 0x%08X\n", pDpeConfig->DVP_CORE_08);
	LOG_INF("DVP_CORE_09 = 0x%08X\n", pDpeConfig->DVP_CORE_09);
	LOG_INF("DVP_CORE_10 = 0x%08X\n", pDpeConfig->DVP_CORE_10);
	LOG_INF("DVP_CORE_11 = 0x%08X\n", pDpeConfig->DVP_CORE_11);
	LOG_INF("DVP_CORE_12 = 0x%08X\n", pDpeConfig->DVP_CORE_12);
	LOG_INF("DVP_CORE_13 = 0x%08X\n", pDpeConfig->DVP_CORE_13);
	LOG_INF("DVP_CORE_14 = 0x%08X\n", pDpeConfig->DVP_CORE_14);
	LOG_INF("DVP_CORE_15 = 0x%08X\n", pDpeConfig->DVP_CORE_15);
}

signed int CmdqDPEHW(struct frame *frame)
#ifdef CMDQ_COMMON
{
	return 0;
}
#else
{
	struct DPE_Config *pDpeConfig;
	unsigned int prevFrm;
	unsigned int curFrm;

	struct cmdqRecStruct *handle;
	uint64_t engineFlag = (uint64_t)(1LL << CMDQ_ENG_DPE);

	if (frame == NULL || frame->data == NULL)
		return -1;

	LOG_DBG("%s request sent to CMDQ driver", __func__);
	pDpeConfig = (struct DPE_Config *) frame->data;

	prevFrm = (pDpeConfig->DVS_CTRL00 & 0x01800000) >> 23;
	curFrm = (pDpeConfig->DVS_CTRL00 & 0x00600000) >> 21;
	LOG_DBG("prevFrm: %d, curFrm: %d\n", prevFrm, curFrm);

	if (pDpeConfig->USERDUMP_EN == 1)
		DPE_DumpUserSpaceReg(pDpeConfig);

	cmdqRecCreate(CMDQ_SCENARIO_ISP_DPE, &handle);

	cmdqRecSetEngine(handle, engineFlag);

#define CMDQWR(REG) \
	cmdqRecWrite(handle, REG ##_HW, pDpeConfig->REG, CMDQ_REG_MASK)
#define CMDQWR_DPE_DRAM_ADDR(REG) \
	cmdqRecWrite(handle, REG ##_HW, (pDpeConfig->REG)>>4, CMDQ_REG_MASK)

if (pDpeConfig->DPE_MODE != 2) {
	/* mask trigger bit */
	cmdqRecWrite(handle, DVS_CTRL00_HW, pDpeConfig->DVS_CTRL00, 0xBFE007FF);
	cmdqRecWrite(handle, DVS_CTRL02_HW, 0x70310000, CMDQ_REG_MASK);
	/* cmdqRecWrite(handle, DVS_CTRL07_HW, 0x0000FF1F, CMDQ_REG_MASK); */
	/* cmdqRecWrite(handle, DVS_SRC_CTRL_HW, 0x00000040, CMDQ_REG_MASK); */
	/* DVS Frame Done IRQ */
	cmdqRecWrite(handle, DVS_IRQ_00_HW, 0x00000E00, 0x00000F00);

	CMDQWR(DVS_DRAM_PITCH);
	CMDQWR(DVS_SRC_00);
	CMDQWR(DVS_SRC_01);
	CMDQWR(DVS_SRC_02);
	CMDQWR(DVS_SRC_03);
	CMDQWR(DVS_SRC_04);

	if (curFrm == 0) {
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_05_L_FRM0);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_06_L_FRM1);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_09_R_FRM0);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_10_R_FRM1);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_13_L_VMAP0);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_14_L_VMAP1);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_17_R_VMAP0);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_18_R_VMAP1);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_21_INTER_MEDV);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_22_MEDV0);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_23_MEDV1);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_26_OCCDV0);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_27_OCCDV1);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_30_DCV_CONF0);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_31_DCV_CONF1);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_34_DCV_L_FRM0);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_35_DCV_L_FRM1);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_38_DCV_R_FRM0);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_39_DCV_R_FRM1);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_42_OCCDV_EXT0);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_43_OCCDV_EXT1);
	} else if (curFrm == 2) {
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_07_L_FRM2);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_08_L_FRM3);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_11_R_FRM2);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_12_R_FRM3);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_15_L_VMAP2);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_16_L_VMAP3);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_19_R_VMAP2);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_20_R_VMAP3);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_21_INTER_MEDV);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_24_MEDV2);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_25_MEDV3);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_28_OCCDV2);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_29_OCCDV3);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_32_DCV_CONF2);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_33_DCV_CONF3);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_36_DCV_L_FRM2);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_37_DCV_L_FRM3);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_40_DCV_R_FRM2);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_41_DCV_R_FRM3);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_44_OCCDV_EXT2);
		CMDQWR_DPE_DRAM_ADDR(DVS_SRC_45_OCCDV_EXT3);
	}

/* For First Frame  */
if (prevFrm == curFrm)
	cmdqRecWrite(handle, DVS_SRC_21_INTER_MEDV_HW, 0x80000000, 0x80000000);

	CMDQWR(DVS_ME_00);
	CMDQWR(DVS_ME_01);
	CMDQWR(DVS_ME_02);
	CMDQWR(DVS_ME_03);
	CMDQWR(DVS_ME_04);
	CMDQWR(DVS_ME_05);
	CMDQWR(DVS_ME_06);
	CMDQWR(DVS_ME_07);
	CMDQWR(DVS_ME_08);
	CMDQWR(DVS_ME_09);
	CMDQWR(DVS_ME_10);
	CMDQWR(DVS_ME_11);
	CMDQWR(DVS_ME_12);
	CMDQWR(DVS_ME_13);
	CMDQWR(DVS_ME_14);
	CMDQWR(DVS_ME_15);
	CMDQWR(DVS_ME_16);
	CMDQWR(DVS_ME_17);
	CMDQWR(DVS_ME_18);
	CMDQWR(DVS_ME_19);
	CMDQWR(DVS_ME_20);
	CMDQWR(DVS_ME_21);
	CMDQWR(DVS_ME_22);
	CMDQWR(DVS_ME_23);
	CMDQWR(DVS_ME_24);
	CMDQWR(DVS_ME_25);
	CMDQWR(DVS_ME_26);
	CMDQWR(DVS_ME_27);
	CMDQWR(DVS_ME_28);
	CMDQWR(DVS_ME_29);
	CMDQWR(DVS_ME_30);
	CMDQWR(DVS_ME_31);
	CMDQWR(DVS_ME_32);
	CMDQWR(DVS_ME_33);
	CMDQWR(DVS_ME_34);
	CMDQWR(DVS_ME_35);
	CMDQWR(DVS_ME_36);
	CMDQWR(DVS_OCC_PQ_0);
	CMDQWR(DVS_OCC_PQ_1);
	/* CRC EN, CRC SEL =0x0 */
	/* cmdqRecWrite(handle, DVS_CRC_CTRL_HW, 0x00000001, 0x00000F01); */
	/* CRC CLEAR  = 1 */
	/* cmdqRecWrite(handle, DVS_CRC_CTRL_HW, 0x00000010, 0x00000010); */
	/* CRC CLEAR  = 0 */
	/* cmdqRecWrite(handle, DVS_CRC_CTRL_HW, 0x00000000, 0x00000010); */
}
	/*================= DVP Settings ==================*/
if (g_isDvpInUse == 0) {
	cmdqRecWrite(handle, DVP_CTRL00_HW, 0x80000080, CMDQ_REG_MASK);
	/* DVP Config Mode SEL = 1 */
	cmdqRecWrite(handle, DVP_CTRL01_HW, 0x00040000, 0x00040000);
	cmdqRecWrite(handle, DVP_CTRL02_HW, 0x70310001, CMDQ_REG_MASK);
	/* cmdqRecWrite(handle, DVP_CTRL07_HW, 0x00000707, CMDQ_REG_MASK); */
	/* DVP Frame Done IRQ */
	cmdqRecWrite(handle, DVP_IRQ_00_HW, 0x00000E00, 0x00000F00);

	CMDQWR(DVP_CORE_00);
	CMDQWR(DVP_CORE_01);
	CMDQWR(DVP_CORE_02);
	CMDQWR(DVP_CORE_03);
	CMDQWR(DVP_CORE_04);
	CMDQWR(DVP_CORE_05);
	CMDQWR(DVP_CORE_06);
	CMDQWR(DVP_CORE_07);
	CMDQWR(DVP_CORE_08);
	CMDQWR(DVP_CORE_09);
	CMDQWR(DVP_CORE_10);
	CMDQWR(DVP_CORE_11);
	CMDQWR(DVP_CORE_12);
	CMDQWR(DVP_CORE_13);
	CMDQWR(DVP_CORE_14);
	CMDQWR(DVP_CORE_15);
	CMDQWR(DVP_DRAM_PITCH);

	CMDQWR(DVP_SRC_00);
	CMDQWR(DVP_SRC_01);
	CMDQWR(DVP_SRC_02);
	CMDQWR(DVP_SRC_03);
	CMDQWR(DVP_SRC_04);
	CMDQWR(DVP_CTRL04);
	/*CRC EN, CRC SEL = 0x1000*/
	/* cmdqRecWrite(handle, DVP_CRC_CTRL_HW, 0x00000801, 0x00000F01); */
	/* CRC CLEAR  = 1 */
	/* cmdqRecWrite(handle, DVP_CRC_CTRL_HW, 0x00000010, 0x00000010); */
	/* CRC CLEAR  = 0 */
	/* cmdqRecWrite(handle, DVP_CRC_CTRL_HW, 0x00000000, 0x00000010); */
	spin_lock_irq(&(DPEInfo.SpinLockDPE));
	g_isDvpInUse = 1;
	spin_unlock_irq(&(DPEInfo.SpinLockDPE));
}
	/* Double Buffer Mask = 1 */
	cmdqRecWrite(handle, DVP_CTRL01_HW, 0x00020000, 0x00020000);
	if (curFrm == 0) {
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_05_Y_FRM0);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_06_Y_FRM1);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_09_C_FRM0);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_10_C_FRM1);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_13_OCCDV0);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_14_OCCDV1);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_17_CRM);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_18_ASF_RMDV);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_19_ASF_RDDV);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_20_ASF_DV0);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_21_ASF_DV1);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_24_WMF_HFDV);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_25_WMF_DV0);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_26_WMF_DV1);
		CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_13_OCCDV0);
		CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_14_OCCDV1);
		CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_18_ASF_RMDV);
		CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_19_ASF_RDDV);
		CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_20_ASF_DV0);
		CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_21_ASF_DV1);
	} else if (curFrm == 2) {
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_07_Y_FRM2);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_08_Y_FRM3);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_11_C_FRM2);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_12_C_FRM3);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_15_OCCDV2);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_16_OCCDV3);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_17_CRM);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_18_ASF_RMDV);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_19_ASF_RDDV);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_22_ASF_DV2);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_23_ASF_DV3);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_24_WMF_HFDV);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_27_WMF_DV2);
		CMDQWR_DPE_DRAM_ADDR(DVP_SRC_28_WMF_DV3);
		CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_15_OCCDV2);
		CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_16_OCCDV3);
		CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_18_ASF_RMDV);
		CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_19_ASF_RDDV);
		CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_22_ASF_DV2);
		CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_23_ASF_DV3);
	}
	/* Double Buffer Mask = 0 */
	cmdqRecWrite(handle, DVP_CTRL01_HW, 0x00000000, 0x00020000);
	/* Double Buffer Config Ready = 1 */
	cmdqRecWrite(handle, DVP_CTRL01_HW, 0x00010000, 0x00010000);

	if (pDpeConfig->DPE_MODE == 2) {
		/* DVP Only Mode*/
		/* DVP Mode Sel = 0 */
		cmdqRecWrite(handle, DVP_CTRL01_HW, 0x00000000, 0x00040000);
		/* DVP FW Tri En = 1 */
		cmdqRecWrite(handle, DVP_CTRL00_HW, 0x40000000, 0x40000000);
		/* DVP FW Tri = 1*/
		cmdqRecWrite(handle, DVP_CTRL00_HW, 0x20000000, 0x20000000);
		cmdqRecWait(handle, CMDQ_EVENT_DVP_DONE_ASYNC_SHOT);
	} else {
		cmdqRecWrite(handle, DVS_CTRL00_HW, 0x40000000, 0x40000000);
		cmdqRecWait(handle, CMDQ_EVENT_DVS_DONE_ASYNC_SHOT);
		cmdqRecWrite(handle, DVS_CTRL00_HW, 0x00000000, 0x40000000);
	}

#ifdef DPE_PMQOS
	if (((pDpeConfig->DVS_SRC_00 & 0x20000000) == 0x20000000) ||
		((pDpeConfig->DVP_CTRL04 & 0x00040000) == 0x00040000))
		/*  16Bit Mode, use high vopp */
		DPEQOS_UpdateDpeFreq(1, 0);
	else
		/*  Non-16Bit Mode, use mid vopp */
		DPEQOS_UpdateDpeFreq(1, 1);
#endif

	if (curFrm == 0) {
		LOG_INF(
		"DVS_SRC05_L: 0x%08X, DVP05_SRC_Y: 0x%08X, DPE_counter: %d\n",
		pDpeConfig->DVS_SRC_05_L_FRM0,
		pDpeConfig->DVP_SRC_05_Y_FRM0,
		DPE_counter);
	} else if (curFrm == 2) {
		LOG_INF(
		"DVS_SRC07_L: 0x%08X, DVP_SRC07_Y: 0x%08X, DPE_counter: %d\n",
		pDpeConfig->DVS_SRC_07_L_FRM2,
		pDpeConfig->DVP_SRC_07_Y_FRM2,
		DPE_counter);
	}

	/* each flush */
	cmdq_op_write_mem(handle, DPE_slot, 0, DPE_counter++);

	/* non-blocking API, Please  use cmdqRecFlushAsync() */
	cmdq_task_flush_async_destroy(handle);	/* flush and destroy in cmdq */

	return 0;
}
#endif
signed int dpe_feedback(struct frame *frame)
{
	struct DPE_Config *pDpeConfig;

	pDpeConfig = (struct DPE_Config *) frame->data;

	/* TODO: read statistics and write to the frame data */
	pDpeConfig->DVS_IRQ_STATUS = DPE_RD32(DVS_IRQ_STATUS_REG);
	return 0;
}

static const struct engine_ops dpe_ops = {
	.req_enque_cb = dpe_enque_cb,
	.req_deque_cb = dpe_deque_cb,
	.frame_handler = CmdqDPEHW,
	.req_feedback_cb = dpe_feedback,
};

/*
 *
 */
static signed int DPE_DumpReg(void)
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
	LOG_INF("DPE Config Info\n");
	/* DPE Config0 */
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL00_HW),
		(unsigned int)DPE_RD32(DVS_CTRL00_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL01_HW),
		(unsigned int)DPE_RD32(DVS_CTRL01_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL02_HW),
		(unsigned int)DPE_RD32(DVS_CTRL02_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL03_HW),
		(unsigned int)DPE_RD32(DVS_CTRL03_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL07_HW),
		(unsigned int)DPE_RD32(DVS_CTRL07_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_IRQ_00_HW),
		(unsigned int)DPE_RD32(DVS_IRQ_00_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL_STATUS0_HW),
		(unsigned int)DPE_RD32(DVS_CTRL_STATUS0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL_STATUS1_HW),
		(unsigned int)DPE_RD32(DVS_CTRL_STATUS1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL_STATUS2_HW),
		(unsigned int)DPE_RD32(DVS_CTRL_STATUS2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_IRQ_STATUS_HW),
		(unsigned int)DPE_RD32(DVS_IRQ_STATUS_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_FRM_STATUS0_HW),
		(unsigned int)DPE_RD32(DVS_FRM_STATUS0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_FRM_STATUS1_HW),
		(unsigned int)DPE_RD32(DVS_FRM_STATUS1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_FRM_STATUS2_HW),
		(unsigned int)DPE_RD32(DVS_FRM_STATUS2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_FRM_STATUS3_HW),
		(unsigned int)DPE_RD32(DVS_FRM_STATUS3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CUR_STATUS_HW),
		(unsigned int)DPE_RD32(DVS_CUR_STATUS_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_CTRL_HW),
		(unsigned int)DPE_RD32(DVS_SRC_CTRL_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL00_HW),
		(unsigned int)DPE_RD32(DVS_CTRL00_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_DRAM_PITCH_HW),
		(unsigned int)DPE_RD32(DVS_DRAM_PITCH_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_00_HW),
		(unsigned int)DPE_RD32(DVS_SRC_00_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_01_HW),
		(unsigned int)DPE_RD32(DVS_SRC_01_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_02_HW),
		(unsigned int)DPE_RD32(DVS_SRC_02_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_03_HW),
		(unsigned int)DPE_RD32(DVS_SRC_03_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_04_HW),
		(unsigned int)DPE_RD32(DVS_SRC_04_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_05_L_FRM0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_05_L_FRM0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_09_R_FRM0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_09_R_FRM0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_13_L_VMAP0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_13_L_VMAP0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_17_R_VMAP0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_17_R_VMAP0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_21_INTER_MEDV_HW),
		(unsigned int)DPE_RD32(DVS_SRC_21_INTER_MEDV_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_22_MEDV0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_22_MEDV0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_26_OCCDV0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_26_OCCDV0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_30_DCV_CONF0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_30_DCV_CONF0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_34_DCV_L_FRM0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_34_DCV_L_FRM0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_38_DCV_R_FRM0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_38_DCV_R_FRM0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_42_OCCDV_EXT0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_42_OCCDV_EXT0_REG));

	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CTRL00_HW),
		(unsigned int)DPE_RD32(DVP_CTRL00_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CTRL01_HW),
		(unsigned int)DPE_RD32(DVP_CTRL01_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CTRL02_HW),
		(unsigned int)DPE_RD32(DVP_CTRL02_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CTRL03_HW),
		(unsigned int)DPE_RD32(DVP_CTRL03_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CTRL04_HW),
		(unsigned int)DPE_RD32(DVP_CTRL04_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CTRL07_HW),
		(unsigned int)DPE_RD32(DVP_CTRL07_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_IRQ_00_HW),
		(unsigned int)DPE_RD32(DVP_IRQ_00_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CTRL_STATUS0_HW),
		(unsigned int)DPE_RD32(DVP_CTRL_STATUS0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CTRL_STATUS1_HW),
		(unsigned int)DPE_RD32(DVP_CTRL_STATUS1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CTRL_STATUS2_HW),
		(unsigned int)DPE_RD32(DVP_CTRL_STATUS2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_IRQ_STATUS_HW),
		(unsigned int)DPE_RD32(DVP_IRQ_STATUS_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_FRM_STATUS0_HW),
		(unsigned int)DPE_RD32(DVP_FRM_STATUS0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_FRM_STATUS1_HW),
		(unsigned int)DPE_RD32(DVP_FRM_STATUS1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_FRM_STATUS2_HW),
		(unsigned int)DPE_RD32(DVP_FRM_STATUS2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_FRM_STATUS3_HW),
		(unsigned int)DPE_RD32(DVP_FRM_STATUS3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CUR_STATUS_HW),
		(unsigned int)DPE_RD32(DVP_CUR_STATUS_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_00_HW),
		(unsigned int)DPE_RD32(DVP_SRC_00_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_01_HW),
		(unsigned int)DPE_RD32(DVP_SRC_01_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_02_HW),
		(unsigned int)DPE_RD32(DVP_SRC_02_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_03_HW),
		(unsigned int)DPE_RD32(DVP_SRC_03_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_04_HW),
		(unsigned int)DPE_RD32(DVP_SRC_04_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_05_Y_FRM0_HW),
		(unsigned int)DPE_RD32(DVP_SRC_05_Y_FRM0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_09_C_FRM0_HW),
		(unsigned int)DPE_RD32(DVP_SRC_09_C_FRM0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_13_OCCDV0_HW),
		(unsigned int)DPE_RD32(DVP_SRC_13_OCCDV0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_17_CRM_HW),
		(unsigned int)DPE_RD32(DVP_SRC_17_CRM_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_18_ASF_RMDV_HW),
		(unsigned int)DPE_RD32(DVP_SRC_18_ASF_RMDV_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_19_ASF_RDDV_HW),
		(unsigned int)DPE_RD32(DVP_SRC_19_ASF_RDDV_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_20_ASF_DV0_HW),
		(unsigned int)DPE_RD32(DVP_SRC_20_ASF_DV0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_24_WMF_HFDV_HW),
		(unsigned int)DPE_RD32(DVP_SRC_24_WMF_HFDV_REG));

	LOG_INF("- X.");
	/*  */
	return Ret;
}
#endif
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/

static inline void DPE_Prepare_Enable_ccf_clock(void)
{
	int ret;

	ret = clk_prepare_enable(dpe_clk.CG_TOP_MUX_DPE);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_TOP_MUX_DPE clock\n");
#ifdef CONFIG_MTK_SMI_EXT
	smi_bus_prepare_enable(SMI_LARB7_REG_INDX, DPE_DEV_NAME, true);
#endif
	ret = clk_prepare_enable(dpe_clk.CG_IPESYS_DPE);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IPESYS_DPE clock\n");

}

static inline void DPE_Disable_Unprepare_ccf_clock(void)
{
	clk_disable_unprepare(dpe_clk.CG_IPESYS_DPE);
#ifdef CONFIG_MTK_SMI_EXT
	smi_bus_disable_unprepare(SMI_LARB7_REG_INDX, DPE_DEV_NAME, true);
#endif
	clk_disable_unprepare(dpe_clk.CG_TOP_MUX_DPE);
}
#endif

/**************************************************************
 *
 **************************************************************/
static void DPE_EnableClock(bool En)
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
			DPE_Prepare_Enable_ccf_clock();
#else
			/* Enable clock by hardcode:
			 * 1. CAMSYS_CG_CLR (0x1A000008) = 0xffffffff;
			 * 2. IMG_CG_CLR (0x15000008) = 0xffffffff;
			 */
			setReg = 0xFFFFFFFF;
			//DPE_WR32(IPESYS_REG_CG_CLR, setReg);
#endif
#else
			enable_clock(MT_CG_DDPE0_SMI_COMMON, "CAMERA");
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
		spin_lock(&(DPEInfo.SpinLockDPE));
		g_u4EnableClockCount++;
		spin_unlock(&(DPEInfo.SpinLockDPE));
	} else {		/* Disable clock. */

		/* LOG_DBG("Dpe clock disabled. g_u4EnableClockCount: %d.",
		 * g_u4EnableClockCount);
		 */
		spin_lock(&(DPEInfo.SpinLockDPE));
		g_u4EnableClockCount--;
		spin_unlock(&(DPEInfo.SpinLockDPE));
		switch (g_u4EnableClockCount) {
		case 0:
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifndef EP_NO_CLKMGR
			DPE_Disable_Unprepare_ccf_clock();
#else
			/* Disable clock by hardcode:
			 *  1. CAMSYS_CG_SET (0x1A000004) = 0xffffffff;
			 *  2. IMG_CG_SET (0x15000004) = 0xffffffff;
			 */
			setReg = 0xFFFFFFFF;
			//DPE_WR32(IPESYS_REG_CG_SET, setReg);
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
			disable_clock(MT_CG_DDPE0_SMI_COMMON, "CAMERA");
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
static inline void DPE_Reset(void)
{
	LOG_DBG("- E.");

	LOG_DBG(" DPE Reset start!\n");
	spin_lock(&(DPEInfo.SpinLockDPERef));

	if (DPEInfo.UserCount > 1) {
		spin_unlock(&(DPEInfo.SpinLockDPERef));
		LOG_DBG("Curr UserCount(%d) users exist", DPEInfo.UserCount);
	} else {
		spin_unlock(&(DPEInfo.SpinLockDPERef));

		/* Reset DPE flow */
		DPE_MASKWR(DVS_CTRL01_REG, 0x70000000, 0x70000000);
		DPE_MASKWR(DVP_CTRL01_REG, 0x70000000, 0x70000000);
		DPE_MASKWR(DVS_CTRL01_REG, 0x00000000, 0x70000000);
		DPE_MASKWR(DVP_CTRL01_REG, 0x00000000, 0x70000000);

		LOG_DBG(" DPE Reset end!\n");
	}
}

/*******************************************************************************
 *
 ******************************************************************************/
static signed int DPE_ReadReg(struct DPE_REG_IO_STRUCT *pRegIo)
{
	unsigned int i;
	signed int Ret = 0;
	/*  */
	struct DPE_REG_STRUCT reg;
	/* unsigned int* pData = (unsigned int*)pRegIo->Data; */
	struct DPE_REG_STRUCT *pData = (struct DPE_REG_STRUCT *) pRegIo->pData;

	if ((pRegIo->pData == NULL) ||
		(pRegIo->Count == 0) ||
		(pRegIo->Count > DPE_MAX_REG_CNT)) {
		LOG_ERR("ERROR: pRegIo->pData is NULL or Count:%d\n",
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
		if ((ISP_DPE_BASE + reg.Addr >= ISP_DPE_BASE)
		    && (ISP_DPE_BASE + reg.Addr <
						(ISP_DPE_BASE + DPE_REG_RANGE))
			&& ((reg.Addr & 0x3) == 0)) {
			reg.Val = DPE_RD32(ISP_DPE_BASE + reg.Addr);
		} else {
			LOG_ERR(
			"Wrong address(0x%p)", (ISP_DPE_BASE + reg.Addr));
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
static signed int DPE_WriteRegToHw(struct DPE_REG_STRUCT *pReg,
							unsigned int Count)
{
	signed int Ret = 0;
	unsigned int i;
	bool dbgWriteReg;

	spin_lock(&(DPEInfo.SpinLockDPE));
	dbgWriteReg = DPEInfo.DebugMask & DPE_DBG_WRITE_REG;
	spin_unlock(&(DPEInfo.SpinLockDPE));

	/*  */
	if (dbgWriteReg)
		LOG_DBG("- E.\n");

	/*  */
	for (i = 0; i < Count; i++) {
		if (dbgWriteReg) {
			LOG_DBG("Addr(0x%lx), Val(0x%x)\n",
				(unsigned long)(ISP_DPE_BASE + pReg[i].Addr),
				(unsigned int) (pReg[i].Val));
		}

		if (((ISP_DPE_BASE + pReg[i].Addr) <
						(ISP_DPE_BASE + DPE_REG_RANGE))
			&& ((pReg[i].Addr & 0x3) == 0)) {
			DPE_WR32(ISP_DPE_BASE + pReg[i].Addr, pReg[i].Val);
		} else {
			LOG_ERR("wrong address(0x%lx)\n",
				(unsigned long)(ISP_DPE_BASE + pReg[i].Addr));
		}
	}

	/*  */
	return Ret;
}



/*******************************************************************************
 *
 ******************************************************************************/
static signed int DPE_WriteReg(struct DPE_REG_IO_STRUCT *pRegIo)
{
	signed int Ret = 0;
	/*
	 *  signed int TimeVd = 0;
	 *  signed int TimeExpdone = 0;
	 *  signed int TimeTasklet = 0;
	 */
	/* unsigned char* pData = NULL; */
	struct DPE_REG_STRUCT *pData = NULL;
	/*  */
	if (DPEInfo.DebugMask & DPE_DBG_WRITE_REG)
		LOG_DBG("Data(0x%p), Count(%d)\n", (pRegIo->pData),
							(pRegIo->Count));
	/*  */
	if ((pRegIo->pData == NULL) ||
		(pRegIo->Count == 0) ||
		(pRegIo->Count > DPE_MAX_REG_CNT)) {
		LOG_ERR("ERROR: pRegIo->pData is NULL or Count:%d\n",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}

	pData = kmalloc((pRegIo->Count) *
		sizeof(struct DPE_REG_STRUCT),
		GFP_KERNEL); /* Use GFP_KERNEL instead of GFP_ATOMIC */
if (pData == NULL) {
	LOG_INF("ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)\n",
		current->comm,
		current->pid,
		current->tgid);
		Ret = -ENOMEM;
		goto EXIT;
}

	if (copy_from_user(pData,
		(void __user *)(pRegIo->pData),
		pRegIo->Count * sizeof(struct DPE_REG_STRUCT)) != 0) {
		LOG_ERR("copy_from_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	Ret = DPE_WriteRegToHw(pData, pRegIo->Count);
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
static signed int DPE_WaitIrq(struct DPE_WAIT_IRQ_STRUCT *WaitIrq)
{
	signed int Ret = 0;
	signed int Timeout = WaitIrq->Timeout;
	pid_t ProcessID;
	enum DPE_PROCESS_ID_ENUM whichReq = DPE_PROCESS_ID_NONE;

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
	if (DPEInfo.DebugMask & DPE_DBG_INT) {
		if (WaitIrq->Status & DPEInfo.IrqInfo.Mask[WaitIrq->Type]) {
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
	if (WaitIrq->Clear == DPE_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(&(DPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		/* LOG_DBG(
		 * "WARNING: Clear(%d), Type(%d): IrqStatus(0x%08X) has been
		 * cleared" ,WaitIrq->EventInfo.Clear,WaitIrq->Type,
		 * DPEInfo.IrqInfo.Status[WaitIrq->Type]);
		 * DPEInfo.IrqInfo.Status[WaitIrq->Type][
		 * WaitIrq->EventInfo.UserKey] &= (~WaitIrq->EventInfo.Status);
		 */
		DPEInfo.IrqInfo.Status[WaitIrq->Type] &= (~WaitIrq->Status);
		spin_unlock_irqrestore(&(DPEInfo.SpinLockIrq[WaitIrq->Type]),
									flags);
		return Ret;
	}

	if (WaitIrq->Clear == DPE_IRQ_CLEAR_WAIT) {
		spin_lock_irqsave(&(DPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		if (DPEInfo.IrqInfo.Status[WaitIrq->Type] & WaitIrq->Status)
			DPEInfo.IrqInfo.Status[WaitIrq->Type] &=
							(~WaitIrq->Status);

		spin_unlock_irqrestore(&(DPEInfo.SpinLockIrq[WaitIrq->Type]),
									flags);
	} else if (WaitIrq->Clear == DPE_IRQ_CLEAR_ALL) {
		spin_lock_irqsave(&(DPEInfo.SpinLockIrq[WaitIrq->Type]), flags);

		DPEInfo.IrqInfo.Status[WaitIrq->Type] = 0;
		spin_unlock_irqrestore(&(DPEInfo.SpinLockIrq[WaitIrq->Type]),
									flags);
	}
	/* DPE_IRQ_WAIT_CLEAR ==> do nothing */


	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
	spin_lock_irqsave(&(DPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
	irqStatus = DPEInfo.IrqInfo.Status[WaitIrq->Type];
	spin_unlock_irqrestore(&(DPEInfo.SpinLockIrq[WaitIrq->Type]), flags);

	if (WaitIrq->Status & DPE_INT_ST) {
		whichReq = DPE_PROCESS_ID_DPE;
	} else {
		LOG_ERR(
		"No Such Stats can be waited!! irq Type/User/Sts/Pid(0x%x/%d/0x%x/%d)\n",
			WaitIrq->Type, WaitIrq->UserKey, WaitIrq->Status,
			WaitIrq->ProcessID);
	}



	/* 2. start to wait signal */
	Timeout = wait_event_interruptible_timeout(DPEInfo.WaitQueueHead,
						   DPE_GetIRQState(
							WaitIrq->Type,
							WaitIrq->UserKey,
							WaitIrq->Status,
							whichReq,
							WaitIrq->ProcessID),
						   DPE_MsToJiffies(
							WaitIrq->Timeout));

	/* check if user is interrupted by system signal */
	if ((Timeout != 0) &&
		(!DPE_GetIRQState(WaitIrq->Type, WaitIrq->UserKey,
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
		LOG_INF("DPE Queue Index(icnt): %d\n", dpe_reqs.req_ctl.icnt);
		LOG_INF("DPE Queue Index(gcnt): %d\n", dpe_reqs.req_ctl.gcnt);
		if ((dpe_reqs.req_ctl.icnt) != (dpe_reqs.req_ctl.gcnt)) {
			LOG_INF("DPE IRQ Recover\n");
		spin_lock_irq(&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]));
			dpe_update_request(&dpe_reqs, &ProcessID);

			DPEInfo.IrqInfo.Status[DPE_IRQ_TYPE_INT_DVP_ST] |=
				DPE_INT_ST;
			DPEInfo.IrqInfo.ProcessID[DPE_PROCESS_ID_DPE] =
				ProcessID;
			DPEInfo.IrqInfo.DpeIrqCnt++;
			DPEInfo.ProcessID[DPEInfo.WriteReqIdx] = ProcessID;
			DPEInfo.WriteReqIdx =
				(DPEInfo.WriteReqIdx + 1) %
				_SUPPORT_MAX_DPE_FRAME_REQUEST_;
			wake_up_interruptible(&DPEInfo.WaitQueueHead);
		spin_unlock_irq(
			&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]));
		Ret = 0;
		goto EXIT;
		}
		spin_lock_irqsave(&(DPEInfo.SpinLockIrq[WaitIrq->Type]),
									flags);
		irqStatus = DPEInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(&(DPEInfo.SpinLockIrq[WaitIrq->Type]),
									flags);

		LOG_ERR(
		"WaitIrq Timeout:Tout(%d) clr(%d) Type(%d) IrqStat(0x%08X) WaitStat(0x%08X) usrKey(%d)\n",
		     WaitIrq->Timeout, WaitIrq->Clear, WaitIrq->Type, irqStatus,
			WaitIrq->Status, WaitIrq->UserKey);
		LOG_ERR(
		"WaitIrq Timeout:whichReq(%d),ProcID(%d) DpeIrqCnt(0x%08X) WriteReq(0x%08X) ReadReq(0x%08X)\n",
		     whichReq, WaitIrq->ProcessID, DPEInfo.IrqInfo.DpeIrqCnt,
			DPEInfo.WriteReqIdx, DPEInfo.ReadReqIdx);

		if (WaitIrq->bDumpReg) {
			DPE_DumpReg();
			dpe_request_dump(&dpe_reqs);
		}

		Ret = -EFAULT;
		goto EXIT;
	} else {
#ifdef __DPE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("DPE WaitIrq");
#endif

		spin_lock_irqsave(&(DPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = DPEInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(&(DPEInfo.SpinLockIrq[WaitIrq->Type]),
									flags);

		if (WaitIrq->Clear == DPE_IRQ_WAIT_CLEAR) {
			spin_lock_irqsave(&(DPEInfo.SpinLockIrq[WaitIrq->Type]),
									flags);

			if (WaitIrq->Status & DPE_INT_ST) {
				DPEInfo.IrqInfo.DpeIrqCnt--;
				if (DPEInfo.IrqInfo.DpeIrqCnt == 0)
					DPEInfo.IrqInfo.Status[WaitIrq->Type] &=
							(~WaitIrq->Status);
			} else {
				LOG_ERR(
				"DPE_IRQ_WAIT_CLEAR Error, Type(%d), WaitStatus(0x%08X)",
					WaitIrq->Type, WaitIrq->Status);
			}
			spin_unlock_irqrestore(
				&(DPEInfo.SpinLockIrq[WaitIrq->Type]), flags);
		}


#ifdef __DPE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif

	}


EXIT:

	return Ret;
}


/*******************************************************************************
 *
 ******************************************************************************/
static long DPE_ioctl(struct file *pFile, unsigned int Cmd, unsigned long Param)
{
	signed int Ret = 0;

	/*unsigned int pid = 0;*/
	struct DPE_REG_IO_STRUCT RegIo;
	struct DPE_WAIT_IRQ_STRUCT IrqInfo;
	struct DPE_CLEAR_IRQ_STRUCT ClearIrq;
	struct DPE_Config dpe_DpeConfig;
	struct DPE_Request dpe_DpeReq;
	signed int enqnum;
	struct DPE_USER_INFO_STRUCT *pUserInfo;
	int enqueNum;
	int dequeNum;
	unsigned long flags;
	/* old: unsigned int flags;*//* FIX to avoid build warning */



	/*  */
	if (pFile->private_data == NULL) {
		LOG_WRN("private_data NULL,(process, pid, tgid)=(%s, %d, %d)",
			current->comm,
				current->pid, current->tgid);
		return -EFAULT;
	}
	/*  */
	pUserInfo = (struct DPE_USER_INFO_STRUCT *) (pFile->private_data);
	/*  */
	switch (Cmd) {
	case DPE_RESET:
		{
			spin_lock(&(DPEInfo.SpinLockDPE));
			DPE_Reset();
			spin_unlock(&(DPEInfo.SpinLockDPE));
			break;
		}

		/*  */
	case DPE_DUMP_REG:
		{
			Ret = DPE_DumpReg();
			break;
		}
	case DPE_DUMP_ISR_LOG:
		{
			unsigned int currentPPB = m_CurrentPPB;

			spin_lock_irqsave(
				&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
									flags);
			m_CurrentPPB = (m_CurrentPPB + 1) % LOG_PPNUM;
			spin_unlock_irqrestore(
				&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
									flags);

			IRQ_LOG_PRINTER(DPE_IRQ_TYPE_INT_DVP_ST, currentPPB,
								_LOG_INF);
			IRQ_LOG_PRINTER(DPE_IRQ_TYPE_INT_DVP_ST, currentPPB,
								_LOG_ERR);
			break;
		}
	case DPE_READ_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param,
				sizeof(struct DPE_REG_IO_STRUCT)) == 0) {
				Ret = DPE_ReadReg(&RegIo);
			} else {
				LOG_ERR(
				"DPE_READ_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case DPE_WRITE_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param,
				sizeof(struct DPE_REG_IO_STRUCT)) == 0) {
				Ret = DPE_WriteReg(&RegIo);
			} else {
				LOG_ERR(
				"DPE_WRITE_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case DPE_WAIT_IRQ:
		{
			if (copy_from_user(&IrqInfo, (void *)Param,
				sizeof(struct DPE_WAIT_IRQ_STRUCT)) == 0) {
				/*  */
				if ((IrqInfo.Type >= DPE_IRQ_TYPE_AMOUNT) ||
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
				"IRQ clear(%d), type(%d), userKey(%d), timeout(%d), status(0x%x)\n",
					IrqInfo.Clear, IrqInfo.Type,
					IrqInfo.UserKey, IrqInfo.Timeout,
					IrqInfo.Status);
				IrqInfo.ProcessID = pUserInfo->Pid;
				Ret = DPE_WaitIrq(&IrqInfo);

				if (copy_to_user((void *)Param, &IrqInfo,
				sizeof(struct DPE_WAIT_IRQ_STRUCT)) != 0) {
					LOG_ERR("copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR("DPE_WAIT_IRQ copy_from_user failed");
				Ret = -EFAULT;
			}
#ifdef DPE_PMQOS
	DPEQOS_UpdateDpeFreq(0, 0);
#endif
			break;
		}
	case DPE_CLEAR_IRQ:
		{
			if (copy_from_user(&ClearIrq, (void *)Param,
				sizeof(struct DPE_CLEAR_IRQ_STRUCT)) == 0) {
				LOG_DBG("DPE_CLEAR_IRQ Type(%d)",
								ClearIrq.Type);

				if ((ClearIrq.Type >= DPE_IRQ_TYPE_AMOUNT) ||
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
				"DPE_CLEAR_IRQ:Type(%d),Status(0x%08X),IrqStatus(0x%08X)\n",
					ClearIrq.Type, ClearIrq.Status,
					DPEInfo.IrqInfo.Status[ClearIrq.Type]);
				spin_lock_irqsave(
				&(DPEInfo.SpinLockIrq[ClearIrq.Type]), flags);
				DPEInfo.IrqInfo.Status[ClearIrq.Type] &=
							(~ClearIrq.Status);
				spin_unlock_irqrestore(
				&(DPEInfo.SpinLockIrq[ClearIrq.Type]), flags);
			} else {
				LOG_ERR(
				"DPE_CLEAR_IRQ copy_from_user failed\n");
				Ret = -EFAULT;
			}
			break;
		}
	case DPE_ENQNUE_NUM:
		{
			if (copy_from_user(&enqueNum, (void *)Param,
							sizeof(int)) == 0) {
				if (DPE_REQUEST_STATE_EMPTY ==
				    g_DPE_ReqRing.DPEReq_Struct[
							g_DPE_ReqRing.WriteIdx].
				    State) {
					spin_lock_irqsave(
					&(DPEInfo.SpinLockIrq[
						DPE_IRQ_TYPE_INT_DVP_ST]),
									flags);
					g_DPE_ReqRing.DPEReq_Struct[
						g_DPE_ReqRing.WriteIdx].
							processID =
								pUserInfo->Pid;
					g_DPE_ReqRing.DPEReq_Struct[
						g_DPE_ReqRing.WriteIdx].
							enqueReqNum = enqueNum;
					spin_unlock_irqrestore(
					&(DPEInfo.SpinLockIrq[
					DPE_IRQ_TYPE_INT_DVP_ST]), flags);
					if (enqueNum >
					_SUPPORT_MAX_DPE_FRAME_REQUEST_) {
						LOG_ERR(
						"DPE Enque Num is bigger than enqueNum:%d\n",
						     enqueNum);
					}
					LOG_DBG(
					"DPE_ENQNUE_NUM:%d\n", enqueNum);
				} else {
					LOG_ERR(
					"WFME Enque request state is not empty:%d, writeIdx:%d, readIdx:%d\n",
					     g_DPE_ReqRing.DPEReq_Struct[
						g_DPE_ReqRing.WriteIdx].State,
						g_DPE_ReqRing.WriteIdx,
						g_DPE_ReqRing.ReadIdx);
				}
			} else {
				LOG_ERR(
				"DPE_EQNUE_NUM copy_from_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}
		/* struct DPE_Config */
	case DPE_ENQUE:
		{
			if (copy_from_user(&dpe_DpeConfig, (void *)Param,
					sizeof(struct DPE_Config)) == 0) {
				/* LOG_DBG(
				 * "DPE_CLEAR_IRQ:Type(%d),Status(0x%08X),
				 * IrqStatus(0x%08X)",
				 * ClearIrq.Type, ClearIrq.Status,
				 * DPEInfo.IrqInfo.Status[ClearIrq.Type]);
				 */
				spin_lock_irqsave(
				&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
						  flags);
				if ((DPE_REQUEST_STATE_EMPTY ==
				     g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.WriteIdx].State)
				    && (g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.WriteIdx].FrameWRIdx <
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.WriteIdx].enqueReqNum)) {
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.WriteIdx].DpeFrameStatus[
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.WriteIdx].FrameWRIdx] =
					DPE_FRAME_STATUS_ENQUE;
				memcpy(&g_DPE_ReqRing.DPEReq_Struct[
						g_DPE_ReqRing.WriteIdx].
						DpeFrameConfig[
						g_DPE_ReqRing.DPEReq_Struct[
						g_DPE_ReqRing.WriteIdx].
						FrameWRIdx++],
						&dpe_DpeConfig,
						sizeof(struct DPE_Config));
				if (g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.WriteIdx].FrameWRIdx ==
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.WriteIdx].enqueReqNum) {
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.WriteIdx].State =
					DPE_REQUEST_STATE_PENDING;
					g_DPE_ReqRing.WriteIdx =
					(g_DPE_ReqRing.WriteIdx +
					1) %
					_SUPPORT_MAX_DPE_REQUEST_RING_SIZE_;
					LOG_DBG("DPE enque done!!\n");
				} else {
					LOG_DBG("DPE enque frame!!\n");
				}
				} else {
					LOG_ERR(
					"No Buffer! WriteIdx(%d), Stat(%d), FrameWRIdx(%d), enqueReqNum(%d)\n",
					     g_DPE_ReqRing.WriteIdx,
					     g_DPE_ReqRing.DPEReq_Struct[
						g_DPE_ReqRing.WriteIdx].State,
					     g_DPE_ReqRing.DPEReq_Struct[
						g_DPE_ReqRing.WriteIdx].
								FrameWRIdx,
					     g_DPE_ReqRing.DPEReq_Struct[
							g_DPE_ReqRing.WriteIdx].
								enqueReqNum);
				}

				spin_unlock_irqrestore(
				&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
						       flags);
				LOG_ERR("ConfigDPE Not Support\n");


			} else {
				LOG_ERR("DPE_ENQUE copy_from_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}
	case DPE_ENQUE_REQ:
		{
			if (copy_from_user(&dpe_DpeReq, (void *)Param,
					sizeof(struct DPE_Request)) == 0) {
				LOG_DBG("DPE_ENQNUE_NUM:%d, pid:%d\n",
					dpe_DpeReq.m_ReqNum, pUserInfo->Pid);
				if (dpe_DpeReq.m_ReqNum >
					_SUPPORT_MAX_DPE_FRAME_REQUEST_) {
					LOG_ERR(
					"DPE Enque Num is bigger than enqueNum:%d\n",
						dpe_DpeReq.m_ReqNum);
					Ret = -EFAULT;
					goto EXIT;
				}
				if (copy_from_user
				    (g_DpeEnqueReq_Struct.DpeFrameConfig,
				     (void *)dpe_DpeReq.m_pDpeConfig,
				     dpe_DpeReq.m_ReqNum *
					sizeof(struct DPE_Config)) != 0) {
					LOG_ERR(
					"copy DPEConfig from request fail!!\n");
					Ret = -EFAULT;
					goto EXIT;
				}

				mutex_lock(&gDpeMutex);

				spin_lock_irqsave(
				&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
						  flags);
				kDpeReq.m_ReqNum = dpe_DpeReq.m_ReqNum;
				kDpeReq.m_pDpeConfig =
					g_DpeEnqueReq_Struct.DpeFrameConfig;
				enqnum = dpe_enque_request(&dpe_reqs,
				kDpeReq.m_ReqNum, &kDpeReq, pUserInfo->Pid);

				spin_unlock_irqrestore(
				&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
						       flags);
				LOG_DBG("Config DPE Request!!\n");

				/* Use a workqueue to set CMDQ to prevent
				 * HW CMDQ request consuming speed from being
				 * faster than SW frame-queue update speed.
				 */
				if (!dpe_request_running(&dpe_reqs)) {
					LOG_DBG("direct request_handler\n");
					dpe_request_handler(&dpe_reqs,
					&(DPEInfo.SpinLockIrq[
					DPE_IRQ_TYPE_INT_DVP_ST]));
				}
				mutex_unlock(&gDpeMutex);
			} else {
				LOG_ERR(
				"DPE_ENQUE_REQ copy_from_user failed\n");
				Ret = -EFAULT;
			}
			break;
		}
	case DPE_DEQUE_NUM:
		{
			if (DPE_REQUEST_STATE_FINISHED ==
			    g_DPE_ReqRing.DPEReq_Struct[
				g_DPE_ReqRing.ReadIdx].State) {
				dequeNum =
				    g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].enqueReqNum;
				LOG_DBG("DPE_DEQUE_NUM(%d)\n", dequeNum);
			} else {
				dequeNum = 0;
				LOG_ERR(
				"DEQUE_NUM:No Buffer: ReadIdx(%d) State(%d) RrameRDIdx(%d) enqueReqNum(%d)\n",
				     g_DPE_ReqRing.ReadIdx,
				     g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].State,
				     g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].RrameRDIdx,
				     g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].enqueReqNum);
			}
			if (copy_to_user((void *)Param, &dequeNum,
						sizeof(unsigned int)) != 0) {
				LOG_ERR("DPE_DEQUE_NUM copy_to_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}

	case DPE_DEQUE:
		{
			spin_lock_irqsave(
			&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]), flags);
			if ((DPE_REQUEST_STATE_FINISHED ==
			     g_DPE_ReqRing.DPEReq_Struct[
				 g_DPE_ReqRing.ReadIdx].State)
			    && (g_DPE_ReqRing.DPEReq_Struct[
				g_DPE_ReqRing.ReadIdx].RrameRDIdx <
				g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].enqueReqNum)) {
				if (DPE_FRAME_STATUS_FINISHED ==
				    g_DPE_ReqRing.DPEReq_Struct[
							g_DPE_ReqRing.ReadIdx].
				    DpeFrameStatus[g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].RrameRDIdx]) {
					memcpy(&dpe_DpeConfig,
					&g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].
					DpeFrameConfig[
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].RrameRDIdx],
					sizeof(struct DPE_Config));
					g_DPE_ReqRing.DPEReq_Struct[
						g_DPE_ReqRing.ReadIdx].
						DpeFrameStatus[
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].RrameRDIdx++] =
					    DPE_FRAME_STATUS_EMPTY;
				}
				if (g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].RrameRDIdx ==
				    g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].enqueReqNum) {
					g_DPE_ReqRing.DPEReq_Struct[
						g_DPE_ReqRing.ReadIdx].State =
						DPE_REQUEST_STATE_EMPTY;
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].FrameWRIdx = 0;
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].RrameRDIdx = 0;
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].enqueReqNum = 0;
					g_DPE_ReqRing.ReadIdx =
					(g_DPE_ReqRing.ReadIdx + 1) %
					_SUPPORT_MAX_DPE_REQUEST_RING_SIZE_;
					LOG_DBG("DPE ReadIdx(%d)\n",
							g_DPE_ReqRing.ReadIdx);
				}
				spin_unlock_irqrestore(
				&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
						       flags);
			if (copy_to_user
				((void *)Param,
				&g_DPE_ReqRing.DPEReq_Struct[
				g_DPE_ReqRing.ReadIdx].DpeFrameConfig
				[g_DPE_ReqRing.DPEReq_Struct
				[g_DPE_ReqRing.ReadIdx].RrameRDIdx],
				sizeof(struct DPE_Config)) != 0) {
				LOG_ERR(
				"DPE_DEQUE copy_to_user fail\n");
				Ret = -EFAULT;
			}

			} else {
				spin_unlock_irqrestore(
				&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
						       flags);
			LOG_ERR("DPE_DEQUE No Buf: (%d)(%d)(%d)(%d)\n",
				g_DPE_ReqRing.ReadIdx,
				g_DPE_ReqRing.DPEReq_Struct[
				g_DPE_ReqRing.ReadIdx].State,
				g_DPE_ReqRing.DPEReq_Struct[
				g_DPE_ReqRing.ReadIdx].RrameRDIdx,
				g_DPE_ReqRing.DPEReq_Struct[
				g_DPE_ReqRing.ReadIdx].enqueReqNum);
			}

			break;
		}
	case DPE_DEQUE_REQ:
		{
			if (copy_from_user(&dpe_DpeReq, (void *)Param,
					sizeof(struct DPE_Request)) == 0) {
				mutex_lock(&gDpeDequeMutex);

				spin_lock_irqsave(
				&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
						  flags);
				kDpeReq.m_pDpeConfig =
					g_DpeDequeReq_Struct.DpeFrameConfig;
				dpe_deque_request(&dpe_reqs, &kDpeReq.m_ReqNum,
								&kDpeReq);
				dequeNum = kDpeReq.m_ReqNum;
				dpe_DpeReq.m_ReqNum = dequeNum;

				spin_unlock_irqrestore(
				&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
						       flags);

				mutex_unlock(&gDpeDequeMutex);
				if (dpe_DpeReq.m_pDpeConfig == NULL) {
					LOG_ERR("NULL ptr:DpeReq.m_pDpeConfig");
					Ret = -EFAULT;
					goto EXIT;
				}
				if (copy_to_user
				    ((void *)dpe_DpeReq.m_pDpeConfig,
				     &g_DpeDequeReq_Struct.DpeFrameConfig[0],
				     dequeNum *
					sizeof(struct DPE_Config)) != 0) {
					LOG_ERR
					    ("DPE_DEQUE_REQ frmcfg failed\n");
					Ret = -EFAULT;
				}
				if (copy_to_user
				    ((void *)Param, &dpe_DpeReq,
					sizeof(struct DPE_Request)) != 0) {
					LOG_ERR("DPE_DEQUE_REQ DpeReq fail\n");
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR("DPE_CMD_DPE_DEQUE_REQ failed\n");
				Ret = -EFAULT;
			}
			break;
		}
	default:
		{
			LOG_ERR("Unknown Cmd(%d)", Cmd);
			LOG_ERR("Cmd(%d),Dir(%d),Typ(%d),Nr(%d),Size(%d)\n",
				Cmd, _IOC_DIR(Cmd),
				_IOC_TYPE(Cmd), _IOC_NR(Cmd), _IOC_SIZE(Cmd));
			Ret = -EPERM;
			break;
		}
	}
	/*  */
EXIT:
	if (Ret != 0) {
		LOG_ERR("Fail Cmd(%d), Pid(%d), (proc, pid, tgid)=(%s, %d, %d)",
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

/*******************************************************************************
 *
 ******************************************************************************/
static int compat_get_DPE_read_register_data(
			struct compat_DPE_REG_IO_STRUCT __user *data32,
					struct DPE_REG_IO_STRUCT __user *data)
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

static int compat_put_DPE_read_register_data(
			struct compat_DPE_REG_IO_STRUCT __user *data32,
					struct DPE_REG_IO_STRUCT __user *data)
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

static int compat_get_DPE_enque_req_data(
			struct compat_DPE_Request __user *data32,
					      struct DPE_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pDpeConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pDpeConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_DPE_enque_req_data(
			struct compat_DPE_Request __user *data32,
					      struct DPE_Request __user *data)
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


static int compat_get_DPE_deque_req_data(
			struct compat_DPE_Request __user *data32,
					      struct DPE_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pDpeConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pDpeConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_DPE_deque_req_data(
		struct compat_DPE_Request __user *data32,
					struct DPE_Request __user *data)
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

static long DPE_ioctl_compat(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	long ret;


	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		LOG_ERR("no f_op !!!\n");
		return -ENOTTY;
	}
	switch (cmd) {
	case COMPAT_DPE_READ_REGISTER:
		{
			struct compat_DPE_REG_IO_STRUCT __user *data32;
			struct DPE_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_DPE_read_register_data(data32, data);
			if (err) {
				LOG_INF("compat_get_read_register_data err.\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, DPE_READ_REGISTER,
						       (unsigned long)data);
			err = compat_put_DPE_read_register_data(data32, data);
			if (err) {
				LOG_INF("compat_put_read_register_data err.\n");
				return err;
			}
			return ret;
		}
	case COMPAT_DPE_WRITE_REGISTER:
		{
			struct compat_DPE_REG_IO_STRUCT __user *data32;
			struct DPE_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_DPE_read_register_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_DPE_WRITE_REGISTER error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, DPE_WRITE_REGISTER,
						       (unsigned long)data);
			return ret;
		}
	case COMPAT_DPE_ENQUE_REQ:
		{
			struct compat_DPE_Request __user *data32;
			struct DPE_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_DPE_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_DPE_ENQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, DPE_ENQUE_REQ,
						       (unsigned long)data);
			err = compat_put_DPE_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_DPE_ENQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_DPE_DEQUE_REQ:
		{
			struct compat_DPE_Request __user *data32;
			struct DPE_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_DPE_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_DPE_DEQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, DPE_DEQUE_REQ,
						       (unsigned long)data);
			err = compat_put_DPE_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_DPE_DEQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}

	case DPE_WAIT_IRQ:
	case DPE_CLEAR_IRQ:	/* structure (no pointer) */
	case DPE_ENQNUE_NUM:
	case DPE_ENQUE:
	case DPE_DEQUE_NUM:
	case DPE_DEQUE:
	case DPE_RESET:
	case DPE_DUMP_REG:
	case DPE_DUMP_ISR_LOG:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
		/* return DPE_ioctl(filep, cmd, arg); */
	}
}

#endif

/*******************************************************************************
 *
 ******************************************************************************/
static signed int DPE_open(struct inode *pInode, struct file *pFile)
{
	signed int Ret = 0;
	unsigned int i, j;
	/*int q = 0, p = 0;*/
	struct DPE_USER_INFO_STRUCT *pUserInfo;

	LOG_DBG("- E. UserCount: %d.", DPEInfo.UserCount);


	/*  */
	spin_lock(&(DPEInfo.SpinLockDPERef));

	pFile->private_data = NULL;
	pFile->private_data = kmalloc(sizeof(struct DPE_USER_INFO_STRUCT),
								GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		LOG_DBG("kmalloc failed, (proc, pid, tgid)=(%s, %d, %d)",
								current->comm,
						current->pid, current->tgid);
		Ret = -ENOMEM;
	} else {
		pUserInfo = (struct DPE_USER_INFO_STRUCT *) pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}
	/*  */
	if (DPEInfo.UserCount > 0) {
		DPEInfo.UserCount++;
		spin_unlock(&(DPEInfo.SpinLockDPERef));
		LOG_DBG("Cur Usr(%d), (proc, pid, tgid)=(%s, %d, %d), exist",
			DPEInfo.UserCount, current->comm, current->pid,
								current->tgid);
		goto EXIT;
	} else {
		DPEInfo.UserCount++;
		spin_unlock(&(DPEInfo.SpinLockDPERef));
		LOG_DBG("Cur Usr(%d), (proc, pid, tgid)=(%s, %d, %d), 1st user",
			DPEInfo.UserCount, current->comm, current->pid,
								current->tgid);
	}

	/* do wait queue head init when re-enter in camera */
	/*  */
	for (i = 0; i < _SUPPORT_MAX_DPE_REQUEST_RING_SIZE_; i++) {
		/* DPE */
		g_DPE_ReqRing.DPEReq_Struct[i].processID = 0x0;
		g_DPE_ReqRing.DPEReq_Struct[i].callerID = 0x0;
		g_DPE_ReqRing.DPEReq_Struct[i].enqueReqNum = 0x0;
		/* g_DPE_ReqRing.DPEReq_Struct[i].enqueIdx = 0x0; */
		g_DPE_ReqRing.DPEReq_Struct[i].State = DPE_REQUEST_STATE_EMPTY;
		g_DPE_ReqRing.DPEReq_Struct[i].FrameWRIdx = 0x0;
		g_DPE_ReqRing.DPEReq_Struct[i].RrameRDIdx = 0x0;
		for (j = 0; j < _SUPPORT_MAX_DPE_FRAME_REQUEST_; j++) {
			g_DPE_ReqRing.DPEReq_Struct[i].DpeFrameStatus[j] =
			    DPE_FRAME_STATUS_EMPTY;
		}

	}
	g_DPE_ReqRing.WriteIdx = 0x0;
	g_DPE_ReqRing.ReadIdx = 0x0;
	g_DPE_ReqRing.HWProcessIdx = 0x0;

	/* Enable clock */
	DPE_EnableClock(MTRUE);
	g_SuspendCnt = 0;
	LOG_INF("DPE open g_u4EnableClockCount: %d", g_u4EnableClockCount);
	/*  */

	for (i = 0; i < DPE_IRQ_TYPE_AMOUNT; i++)
		DPEInfo.IrqInfo.Status[i] = 0;

	for (i = 0; i < _SUPPORT_MAX_DPE_FRAME_REQUEST_; i++)
		DPEInfo.ProcessID[i] = 0;

	DPEInfo.WriteReqIdx = 0;
	DPEInfo.ReadReqIdx = 0;
	DPEInfo.IrqInfo.DpeIrqCnt = 0;

/*#define KERNEL_LOG*/
#ifdef KERNEL_LOG
    /* In EP, Add DPE_DBG_WRITE_REG for debug. Should remove it after EP */
	DPEInfo.DebugMask = (DPE_DBG_INT | DPE_DBG_DBGLOG | DPE_DBG_WRITE_REG);
#endif
	/*  */
	dpe_register_requests(&dpe_reqs, sizeof(struct DPE_Config));
	dpe_set_engine_ops(&dpe_reqs, &dpe_ops);


EXIT:




	LOG_DBG("- X. Ret: %d. UserCount: %d.", Ret, DPEInfo.UserCount);
	return Ret;

}

/******************************************************************************
 *
 ******************************************************************************/
static signed int DPE_release(struct inode *pInode, struct file *pFile)
{
	struct DPE_USER_INFO_STRUCT *pUserInfo;
	/*unsigned int Reg;*/

	LOG_DBG("- E. UserCount: %d.", DPEInfo.UserCount);

	spin_lock_irq(&(DPEInfo.SpinLockDPE));
	g_isDvpInUse = 0;
	spin_unlock_irq(&(DPEInfo.SpinLockDPE));
	/*  */
	if (pFile->private_data != NULL) {
		pUserInfo =
			(struct  DPE_USER_INFO_STRUCT *) pFile->private_data;
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	/*  */
	spin_lock(&(DPEInfo.SpinLockDPERef));
	DPEInfo.UserCount--;

	if (DPEInfo.UserCount > 0) {
		spin_unlock(&(DPEInfo.SpinLockDPERef));
		LOG_DBG("Cur UsrCnt(%d), (proc, pid, tgid)=(%s, %d, %d), exist",
			DPEInfo.UserCount, current->comm, current->pid,
								current->tgid);
		goto EXIT;
	} else
		spin_unlock(&(DPEInfo.SpinLockDPERef));
	/*  */
	LOG_INF("Curr UsrCnt(%d), (process, pid, tgid)=(%s, %d, %d), last user",
		DPEInfo.UserCount, current->comm, current->pid, current->tgid);


	/* Disable clock. */
	DPE_EnableClock(MFALSE);
	LOG_DBG("DPE release g_u4EnableClockCount: %d", g_u4EnableClockCount);

	/*  */
	dpe_unregister_requests(&dpe_reqs);


EXIT:


	LOG_DBG("- X. UserCount: %d.", DPEInfo.UserCount);
	return 0;
}


/*******************************************************************************
 *
 ******************************************************************************/
static signed int DPE_mmap(struct file *pFile, struct vm_area_struct *pVma)
{
	long length = 0;
	unsigned int pfn = 0x0;

	length = pVma->vm_end - pVma->vm_start;
	/*  */
	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
	pfn = pVma->vm_pgoff << PAGE_SHIFT;

	LOG_INF(
		"mmap:vm_pgoff(0x%lx) pfn(0x%x) phy(0x%lx) vm_start(0x%lx) vm_end(0x%lx) length(0x%lx)",
		pVma->vm_pgoff, pfn, pVma->vm_pgoff << PAGE_SHIFT,
			pVma->vm_start, pVma->vm_end, length);

	switch (pfn) {
	case DPE_BASE_HW:
		if (length > DPE_REG_RANGE) {
			LOG_ERR("mmap err:mod:0x%x len(0x%lx),REG_RANGE(0x%x)!",
				pfn, length, DPE_REG_RANGE);
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
	/*  */
	return 0;
}

/*******************************************************************************
 *
 ******************************************************************************/

static dev_t DPEDevNo;
static struct cdev *pDPECharDrv;
static struct class *pDPEClass;

static const struct file_operations DPEFileOper = {
	.owner = THIS_MODULE,
	.open = DPE_open,
	.release = DPE_release,
	/* .flush   = mt_DPE_flush, */
	.mmap = DPE_mmap,
	.unlocked_ioctl = DPE_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = DPE_ioctl_compat,
#endif
};

/**************************************************************
 *
 **************************************************************/
#ifndef TODO
#ifdef CONFIG_MTK_IOMMU_V2
enum mtk_iommu_callback_ret_t DPE_M4U_TranslationFault_callback(int port,
	unsigned int mva, void *data)
#else
enum m4u_callback_ret_t DPE_M4U_TranslationFault_callback(int port,
	unsigned int mva, void *data)
#endif
{

	pr_info("[DPE_M4U]fault call port=%d, mva=0x%x", port, mva);

	switch (port) {
	case M4U_PORT_DVS_RDMA:
	case M4U_PORT_DVS_WDMA:
	case M4U_PORT_DVP_RDMA:
	case M4U_PORT_DVP_WDMA:
	default:  //ISP_DPE_BASE = 0x1b001000
		pr_info("DVS_SRC_05_L_FRM0:0x%08x, DVS_SRC_06_L_FRM1:0x%08x, DVS_SRC_07_L_FRM2:0x%08x, DVS_SRC_08_L_FRM3:0x%08x\n",
			DPE_RD32(DVS_SRC_05_L_FRM0_REG),
			DPE_RD32(DVS_SRC_06_L_FRM1_REG),
			DPE_RD32(DVS_SRC_07_L_FRM2_REG),
			DPE_RD32(DVS_SRC_08_L_FRM3_REG));
		pr_info("DVS_SRC_09_R_FRM0:0x%08x, DVS_SRC_10_R_FRM1:0x%08x, DVS_SRC_11_R_FRM2:0x%08x, DVS_SRC_12_R_FRM3:0x%08x\n",
			DPE_RD32(DVS_SRC_09_R_FRM0_REG),
			DPE_RD32(DVS_SRC_10_R_FRM1_REG),
			DPE_RD32(DVS_SRC_11_R_FRM2_REG),
			DPE_RD32(DVS_SRC_12_R_FRM3_REG));
		pr_info("DVS_SRC_13_L_VMAP0:0x%08x, DVS_SRC_14_L_VMAP1:0x%08x, DVS_SRC_15_L_VMAP2:0x%08x, DVS_SRC_16_L_VMAP3:0x%08x\n",
			DPE_RD32(DVS_SRC_13_L_VMAP0_REG),
			DPE_RD32(DVS_SRC_14_L_VMAP1_REG),
			DPE_RD32(DVS_SRC_15_L_VMAP2_REG),
			DPE_RD32(DVS_SRC_16_L_VMAP3_REG));
		pr_info("DVS_SRC_17_R_VMAP0:0x%08x, DVS_SRC_18_R_VMAP1:0x%08x, DVS_SRC_19_R_VMAP2:0x%08x, DVS_SRC_20_R_VMAP3:0x%08x\n",
			DPE_RD32(DVS_SRC_17_R_VMAP0_REG),
			DPE_RD32(DVS_SRC_18_R_VMAP1_REG),
			DPE_RD32(DVS_SRC_19_R_VMAP2_REG),
			DPE_RD32(DVS_SRC_20_R_VMAP3_REG));
		pr_info("DVS_SRC_21_INTER_MEDV:0x%08x\n",
			DPE_RD32(DVS_SRC_21_INTER_MEDV_REG));
		pr_info("DVS_SRC_22_MEDV0:0x%08x, DVS_SRC_23_MEDV1:0x%08x, DVS_SRC_24_MEDV2:0x%08x, DVS_SRC_25_MEDV3:0x%08x\n",
			DPE_RD32(DVS_SRC_22_MEDV0_REG),
			DPE_RD32(DVS_SRC_23_MEDV1_REG),
			DPE_RD32(DVS_SRC_24_MEDV2_REG),
			DPE_RD32(DVS_SRC_25_MEDV3_REG));
		pr_info("DVS_SRC_26_OCCDV0:0x%08x, DVS_SRC_27_OCCDV1:0x%08x, DVS_SRC_28_OCCDV2:0x%08x, DVS_SRC_29_OCCDV3:0x%08x\n",
			DPE_RD32(DVS_SRC_26_OCCDV0_REG),
			DPE_RD32(DVS_SRC_27_OCCDV1_REG),
			DPE_RD32(DVS_SRC_28_OCCDV2_REG),
			DPE_RD32(DVS_SRC_29_OCCDV3_REG));
		pr_info("DVS_SRC_30_DCV_CONF0:0x%08x, DVS_SRC_31_DCV_CONF1:0x%08x, DVS_SRC_32_DCV_CONF2:0x%08x, DVS_SRC_33_DCV_CONF3:0x%08x\n",
			DPE_RD32(DVS_SRC_30_DCV_CONF0_REG),
			DPE_RD32(DVS_SRC_31_DCV_CONF1_REG),
			DPE_RD32(DVS_SRC_32_DCV_CONF2_REG),
			DPE_RD32(DVS_SRC_33_DCV_CONF3_REG));
		pr_info("DVS_SRC_34_DCV_L_FRM0:0x%08x, DVS_SRC_35_DCV_L_FRM1:0x%08x, DVS_SRC_36_DCV_L_FRM2:0x%08x, DVS_SRC_37_DCV_L_FRM3:0x%08x\n",
			DPE_RD32(DVS_SRC_34_DCV_L_FRM0_REG),
			DPE_RD32(DVS_SRC_35_DCV_L_FRM1_REG),
			DPE_RD32(DVS_SRC_36_DCV_L_FRM2_REG),
			DPE_RD32(DVS_SRC_37_DCV_L_FRM3_REG));
		pr_info("DVS_SRC_38_DCV_R_FRM0:0x%08x, DVS_SRC_39_DCV_R_FRM1:0x%08x, DVS_SRC_40_DCV_R_FRM2:0x%08x, DVS_SRC_41_DCV_R_FRM3:0x%08x\n",
			DPE_RD32(DVS_SRC_38_DCV_R_FRM0_REG),
			DPE_RD32(DVS_SRC_39_DCV_R_FRM1_REG),
			DPE_RD32(DVS_SRC_40_DCV_R_FRM2_REG),
			DPE_RD32(DVS_SRC_41_DCV_R_FRM3_REG));
		pr_info("DVS_SRC_42_OCCDV_EXT0:0x%08x, DVS_SRC_43_OCCDV_EXT1:0x%08x, DVS_SRC_44_OCCDV_EXT2:0x%08x, DVS_SRC_45_OCCDV_EXT3:0x%08x\n",
			DPE_RD32(DVS_SRC_42_OCCDV_EXT0_REG),
			DPE_RD32(DVS_SRC_43_OCCDV_EXT1_REG),
			DPE_RD32(DVS_SRC_44_OCCDV_EXT2_REG),
			DPE_RD32(DVS_SRC_45_OCCDV_EXT3_REG));

		pr_info("DVP_SRC_05_Y_FRM0:0x%08x, DVP_SRC_06_Y_FRM1:0x%08x, DVP_SRC_07_Y_FRM2:0x%08x, DVP_SRC_08_Y_FRM3:0x%08x\n",
			DPE_RD32(DVP_SRC_05_Y_FRM0_REG),
			DPE_RD32(DVP_SRC_06_Y_FRM1_REG),
			DPE_RD32(DVP_SRC_07_Y_FRM2_REG),
			DPE_RD32(DVP_SRC_08_Y_FRM3_REG));
		pr_info("DVP_SRC_09_C_FRM0:0x%08x, DVP_SRC_10_C_FRM1:0x%08x, DVP_SRC_11_C_FRM2:0x%08x, DVP_SRC_12_C_FRM3:0x%08x\n",
			DPE_RD32(DVP_SRC_09_C_FRM0_REG),
			DPE_RD32(DVP_SRC_10_C_FRM1_REG),
			DPE_RD32(DVP_SRC_11_C_FRM2_REG),
			DPE_RD32(DVP_SRC_12_C_FRM3_REG));
		pr_info("DVP_SRC_13_OCCDV0:0x%08x, DVP_SRC_14_OCCDV1:0x%08x, DVP_SRC_15_OCCDV2:0x%08x, DVP_SRC_16_OCCDV3:0x%08x\n",
			DPE_RD32(DVP_SRC_13_OCCDV0_REG),
			DPE_RD32(DVP_SRC_14_OCCDV1_REG),
			DPE_RD32(DVP_SRC_15_OCCDV2_REG),
			DPE_RD32(DVP_SRC_16_OCCDV3_REG));
		pr_info("DVP_SRC_17_CRM:0x%08x\n",
			DPE_RD32(DVP_SRC_17_CRM_REG));
		pr_info("DVP_SRC_18_ASF_RMDV:0x%08x, DVP_SRC_19_ASF_RDDV:0x%08x\n",
			DPE_RD32(DVP_SRC_18_ASF_RMDV_REG),
			DPE_RD32(DVP_SRC_19_ASF_RDDV_REG));
		pr_info("DVP_SRC_20_ASF_DV0:0x%08x, DVP_SRC_21_ASF_DV1:0x%08x, DVP_SRC_22_ASF_DV2:0x%08x, DVP_SRC_23_ASF_DV3:0x%08x\n",
			DPE_RD32(DVP_SRC_20_ASF_DV0_REG),
			DPE_RD32(DVP_SRC_21_ASF_DV1_REG),
			DPE_RD32(DVP_SRC_22_ASF_DV2_REG),
			DPE_RD32(DVP_SRC_23_ASF_DV3_REG));
		pr_info("DVP_SRC_24_WMF_HFDV:0x%08x\n",
			DPE_RD32(DVP_SRC_24_WMF_HFDV_REG));
		pr_info("DVP_SRC_25_WMF_DV0:0x%08x, DVP_SRC_26_WMF_DV1:0x%08x, DVP_SRC_27_WMF_DV2:0x%08x, DVP_SRC_28_WMF_DV3:0x%08x\n",
			DPE_RD32(DVP_SRC_25_WMF_DV0_REG),
			DPE_RD32(DVP_SRC_26_WMF_DV1_REG),
			DPE_RD32(DVP_SRC_27_WMF_DV2_REG),
			DPE_RD32(DVP_SRC_28_WMF_DV3_REG));
		pr_info("DVP_EXT_SRC_13_OCCDV0:0x%08x, DVP_EXT_SRC_14_OCCDV1:0x%08x, DVP_EXT_SRC_15_OCCDV2:0x%08x, DVP_EXT_SRC_16_OCCDV3:0x%08x\n",
			DPE_RD32(DVP_EXT_SRC_13_OCCDV0_REG),
			DPE_RD32(DVP_EXT_SRC_14_OCCDV1_REG),
			DPE_RD32(DVP_EXT_SRC_15_OCCDV2_REG),
			DPE_RD32(DVP_EXT_SRC_16_OCCDV3_REG));
		pr_info("DVP_EXT_SRC_18_ASF_RMDV:0x%08x, DVP_EXT_SRC_19_ASF_RDDV:0x%08x\n",
			DPE_RD32(DVP_EXT_SRC_18_ASF_RMDV_REG),
			DPE_RD32(DVP_EXT_SRC_19_ASF_RDDV_REG));
		pr_info("DVP_EXT_SRC_20_ASF_DV0:0x%08x, DVP_EXT_SRC_21_ASF_DV1:0x%08x, DVP_EXT_SRC_22_ASF_DV2:0x%08x, DVP_EXT_SRC_23_ASF_DV3:0x%08x\n",
			DPE_RD32(DVP_EXT_SRC_20_ASF_DV0_REG),
			DPE_RD32(DVP_EXT_SRC_21_ASF_DV1_REG),
			DPE_RD32(DVP_EXT_SRC_22_ASF_DV2_REG),
			DPE_RD32(DVP_EXT_SRC_23_ASF_DV3_REG));
	break;
	}
#ifdef CONFIG_MTK_IOMMU_V2
	return MTK_IOMMU_CALLBACK_HANDLED;
#else
	return M4U_CALLBACK_HANDLED;
#endif
}
#endif
/*******************************************************************************
 *
 ******************************************************************************/
static inline void DPE_UnregCharDev(void)
{
	LOG_DBG("- E.");
	/*  */
	/* Release char driver */
	if (pDPECharDrv != NULL) {
		cdev_del(pDPECharDrv);
		pDPECharDrv = NULL;
	}
	/*  */
	unregister_chrdev_region(DPEDevNo, 1);
}

/*******************************************************************************
 *
 ******************************************************************************/
static inline signed int DPE_RegCharDev(void)
{
	signed int Ret = 0;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = alloc_chrdev_region(&DPEDevNo, 0, 1, DPE_DEV_NAME);
	if (Ret < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d", Ret);
		return Ret;
	}
	/* Allocate driver */
	pDPECharDrv = cdev_alloc();
	if (pDPECharDrv == NULL) {
		LOG_ERR("cdev_alloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pDPECharDrv, &DPEFileOper);
	/*  */
	pDPECharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pDPECharDrv, DPEDevNo, 1);
	if (Ret < 0) {
		LOG_ERR("Attatch file operation failed, %d", Ret);
		goto EXIT;
	}
	/*  */
EXIT:
	if (Ret < 0)
		DPE_UnregCharDev();

	/*  */

	LOG_DBG("- X.");
	return Ret;
}

/*******************************************************************************
 *
 ******************************************************************************/
static signed int DPE_probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	/*struct resource *pRes = NULL;*/
	signed int i = 0;
	unsigned char n;
	unsigned int irq_info[3];
	struct device *dev = NULL;
	struct DPE_device *_dpe_dev;


#ifdef CONFIG_OF
	struct DPE_device *DPE_dev;
#endif

	LOG_INF("- E. DPE driver probe.\n");

	/* Check platform_device parameters */
#ifdef CONFIG_OF

	if (pDev == NULL) {
		dev_dbg(&pDev->dev, "pDev is NULL");
		return -ENXIO;
	}

	nr_DPE_devs += 1;
	_dpe_dev = krealloc(DPE_devs, sizeof(struct DPE_device) * nr_DPE_devs,
								GFP_KERNEL);
	if (!_dpe_dev) {
		dev_dbg(&pDev->dev, "Unable to allocate DPE_devs\n");
		return -ENOMEM;
	}
	DPE_devs = _dpe_dev;

	DPE_dev = &(DPE_devs[nr_DPE_devs - 1]);
	DPE_dev->dev = &pDev->dev;
	/* iomap registers */
	DPE_dev->regs = of_iomap(pDev->dev.of_node, 0);

	if (!DPE_dev->regs) {
		dev_dbg(&pDev->dev,
			"of_iomap fail, nr_DPE_devs=%d, devnode(%s).\n",
			nr_DPE_devs, pDev->dev.of_node->name);
		return -ENOMEM;
	}

	LOG_INF("nr_DPE_devs=%d, devnode(%s), map_addr=0x%lx\n", nr_DPE_devs,
		pDev->dev.of_node->name, (unsigned long)DPE_dev->regs);

	/* get IRQ ID and request IRQ */
	DPE_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);
	if (DPE_dev->irq > 0) {
	/* Get IRQ Flag from device node */
		if (of_property_read_u32_array(pDev->dev.of_node,
		"interrupts",
		irq_info,
		ARRAY_SIZE(irq_info))) {
			dev_dbg(&pDev->dev,
			"get irq flags from DTS fail!!\n");
			return -ENODEV;
	}

	for (i = 0; i < DPE_IRQ_TYPE_AMOUNT; i++) {
		if (strcmp(pDev->dev.of_node->name,
			DPE_IRQ_CB_TBL[i].device_name) == 0) {
			Ret =
			request_irq(DPE_dev->irq,
				(irq_handler_t)
				DPE_IRQ_CB_TBL[i].isr_fp,
				irq_info[2],
				(const char *)DPE_IRQ_CB_TBL[i].device_name,
				NULL);
			if (Ret) {
				dev_dbg(&pDev->dev,
			"devdbg: nr_DPE_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
				nr_DPE_devs,
				pDev->dev.of_node->name,
				DPE_dev->irq,
				DPE_IRQ_CB_TBL[i].device_name);
				return Ret;
			}

			LOG_INF(
			"nr_DPE_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
			nr_DPE_devs,
			pDev->dev.of_node->name,
			DPE_dev->irq,
			DPE_IRQ_CB_TBL[i].device_name);
			break;
		}
	}

	if (i >= DPE_IRQ_TYPE_AMOUNT) {
		LOG_INF("No ISR:nr_DPE_devs=%d, devnode(%s), irq=%d\n",
			nr_DPE_devs, pDev->dev.of_node->name,
							DPE_dev->irq);
	}
	} else {
		LOG_INF("No IRQ!!: nr_DPE_devs=%d, devnode(%s), irq=%d\n",
			nr_DPE_devs,
			pDev->dev.of_node->name, DPE_dev->irq);
	}


#endif

	/* Only register char driver in the 1st time */
	if (nr_DPE_devs == 1) {

		/* Register char driver */
		Ret = DPE_RegCharDev();
		if (Ret) {
			dev_dbg(&pDev->dev, "register char failed");
			return Ret;
		}
#ifndef EP_NO_CLKMGR
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
		    /*CCF: Grab clock pointer (struct clk*) */
		dpe_clk.CG_TOP_MUX_DPE = devm_clk_get(&pDev->dev,
							"DPE_TOP_MUX");
		if (IS_ERR(dpe_clk.CG_TOP_MUX_DPE)) {
			LOG_ERR("cannot get CG_TOP_MUX_DPE clock\n");
			return PTR_ERR(dpe_clk.CG_TOP_MUX_DPE);
		}
		dpe_clk.CG_IPESYS_DPE = devm_clk_get(&pDev->dev,
							"DPE_CLK_IPE_DPE");
		if (IS_ERR(dpe_clk.CG_IPESYS_DPE)) {
			LOG_ERR("cannot get CG_IPESYS_DPE clock\n");
			return PTR_ERR(dpe_clk.CG_IPESYS_DPE);
		}
#endif
#endif

		/* Create class register */
		pDPEClass = class_create(THIS_MODULE, "DPEdrv");
		if (IS_ERR(pDPEClass)) {
			Ret = PTR_ERR(pDPEClass);
			LOG_ERR("Unable to create class, err = %d", Ret);
			goto EXIT;
		}

		dev = device_create(pDPEClass, NULL, DPEDevNo, NULL,
								DPE_DEV_NAME);
		if (IS_ERR(dev)) {
			Ret = PTR_ERR(dev);
			dev_dbg(&pDev->dev, "create dev err: /dev/%s, err = %d",
				DPE_DEV_NAME, Ret);
			goto EXIT;
		}

		/* Init spinlocks */
		spin_lock_init(&(DPEInfo.SpinLockDPERef));
		spin_lock_init(&(DPEInfo.SpinLockDPE));
		for (n = 0; n < DPE_IRQ_TYPE_AMOUNT; n++)
			spin_lock_init(&(DPEInfo.SpinLockIrq[n]));

		/*  */
		init_waitqueue_head(&DPEInfo.WaitQueueHead);
		INIT_WORK(&DPEInfo.ScheduleDpeWork, DPE_ScheduleWork);
		DPEInfo.wkqueue = create_singlethread_workqueue("DPE-CMDQ-WQ");
		if (!DPEInfo.wkqueue)
			LOG_ERR("NULL DPE-CMDQ-WQ\n");

		INIT_WORK(&logWork, logPrint);
		for (i = 0; i < DPE_IRQ_TYPE_AMOUNT; i++)
			tasklet_init(DPE_tasklet[i].pDPE_tkt,
					DPE_tasklet[i].tkt_cb, 0);




		/* Init DPEInfo */
		spin_lock(&(DPEInfo.SpinLockDPERef));
		DPEInfo.UserCount = 0;
		spin_unlock(&(DPEInfo.SpinLockDPERef));
		/*  */
		DPEInfo.IrqInfo.Mask[DPE_IRQ_TYPE_INT_DVP_ST] = INT_ST_MASK_DPE;

		seqlock_init(&(dpe_reqs.seqlock));
	}

	g_DPE_PMState = 0;
EXIT:
	if (Ret < 0)
		DPE_UnregCharDev();


	/* only once */
	cmdq_alloc_mem(&DPE_slot, 1);

	LOG_INF("- X. DPE driver probe.");

	return Ret;
}

/*******************************************************************************
 * Called when the device is being detached from the driver
 ******************************************************************************/
static signed int DPE_remove(struct platform_device *pDev)
{
	/*struct resource *pRes;*/
	signed int IrqNum;
	int i;
	/*  */
	LOG_DBG("- E.");

	/* wait for unfinished works in the workqueue. */
	destroy_workqueue(DPEInfo.wkqueue);
	DPEInfo.wkqueue = NULL;

	/* unregister char driver. */
	DPE_UnregCharDev();

	/* Release IRQ */
	disable_irq(DPEInfo.IrqNum);
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);

	/* kill tasklet */
	for (i = 0; i < DPE_IRQ_TYPE_AMOUNT; i++)
		tasklet_kill(DPE_tasklet[i].pDPE_tkt);
	/*  */
	device_destroy(pDPEClass, DPEDevNo);
	/*  */
	class_destroy(pDPEClass);
	pDPEClass = NULL;
	/*  */

	return 0;
}

/*******************************************************************************
 *
 ******************************************************************************/
static signed int bPass1_On_In_Resume_TG1;

static signed int DPE_suspend(struct platform_device *pDev, pm_message_t Mesg)
{
	/*signed int ret = 0;*/

	LOG_DBG("bPass1_On_In_Resume_TG1(%d)\n", bPass1_On_In_Resume_TG1);
	if (g_u4EnableClockCount > 0) {
		DPE_EnableClock(MFALSE);
		g_SuspendCnt++;
	}
	bPass1_On_In_Resume_TG1 = 0;
if (g_DPE_PMState == 0) {
	LOG_INF("%s:g_u4EnableClockCount(%d) g_SuspendCnt(%d).\n", __func__,
				g_u4EnableClockCount, g_SuspendCnt);
		g_DPE_PMState = 1;
}
	return 0;
}

/*******************************************************************************
 *
 ******************************************************************************/
static signed int DPE_resume(struct platform_device *pDev)
{
	LOG_DBG("bPass1_On_In_Resume_TG1(%d).\n", bPass1_On_In_Resume_TG1);
	if (g_SuspendCnt > 0) {
		DPE_EnableClock(MTRUE);
		g_SuspendCnt--;
	}
if (g_DPE_PMState == 1) {
	LOG_INF("%s:g_u4EnableClockCount(%d) g_SuspendCnt(%d).\n", __func__,
				g_u4EnableClockCount, g_SuspendCnt);
		g_DPE_PMState = 0;
}
	return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int DPE_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	LOG_DBG("calling %s()\n", __func__);

	return DPE_suspend(pdev, PMSG_SUSPEND);
}

int DPE_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	LOG_DBG("calling %s()\n", __func__);

	return DPE_resume(pdev);
}
#ifndef CONFIG_OF
/*extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);*/
/*extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);*/
#endif
int DPE_pm_restore_noirq(struct device *device)
{
	pr_debug("calling %s()\n", __func__);
#ifndef CONFIG_OF
/*	mt_irq_set_sens(DPE_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);*/
/*	mt_irq_set_polarity(DPE_IRQ_BIT_ID, MT_POLARITY_LOW);*/
#endif
	return 0;

}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define DPE_pm_suspend NULL
#define DPE_pm_resume  NULL
#define DPE_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_OF

static const struct of_device_id DPE_of_ids[] = {
	//{.compatible = "mediatek,ipesys_config",},
	{.compatible = "mediatek,dvp",},
	{.compatible = "mediatek,dvs",},
	{}
};
#endif

const struct dev_pm_ops DPE_pm_ops = {
	.suspend = DPE_pm_suspend,
	.resume = DPE_pm_resume,
	.freeze = DPE_pm_suspend,
	.thaw = DPE_pm_resume,
	.poweroff = DPE_pm_suspend,
	.restore = DPE_pm_resume,
	.restore_noirq = DPE_pm_restore_noirq,
};


/*******************************************************************************
 *
 ******************************************************************************/
static struct platform_driver DPEDriver = {
	.probe = DPE_probe,
	.remove = DPE_remove,
	.suspend = DPE_suspend,
	.resume = DPE_resume,
	.driver = {
		   .name = DPE_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = DPE_of_ids,
#endif
#ifdef CONFIG_PM
		   .pm = &DPE_pm_ops,
#endif
		}
};


static int dpe_dump_read(struct seq_file *m, void *v)
{
	int i, j;

	if (DPEInfo.UserCount <= 0)
		return 0;

	seq_puts(m, "\n============ dpe dump register============\n");
	seq_puts(m, "DPE Config Info\n");

	for (i = 0x2C; i < 0x8C; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(DPE_BASE_HW + i),
			   (unsigned int)DPE_RD32(ISP_DPE_BASE + i));
	}
	seq_puts(m, "DPE Debug Info\n");
	for (i = 0x120; i < 0x148; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(DPE_BASE_HW + i),
			   (unsigned int)DPE_RD32(ISP_DPE_BASE + i));
	}

	seq_puts(m, "DPE Config Info\n");
	for (i = 0x230; i < 0x2D8; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(DPE_BASE_HW + i),
			   (unsigned int)DPE_RD32(ISP_DPE_BASE + i));
	}
	seq_puts(m, "DPE Debug Info\n");
	for (i = 0x2F4; i < 0x30C; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(DPE_BASE_HW + i),
			   (unsigned int)DPE_RD32(ISP_DPE_BASE + i));
	}

	seq_puts(m, "\n");
	seq_printf(m, "Dpe Clock Count:%d\n", g_u4EnableClockCount);

	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVS_IRQ_STATUS_HW),
		   (unsigned int)DPE_RD32(DVS_IRQ_STATUS_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVS_CTRL_STATUS0_HW),
		   (unsigned int)DPE_RD32(DVS_CTRL_STATUS0_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVS_CTRL_STATUS2_HW),
		   (unsigned int)DPE_RD32(DVS_CTRL_STATUS2_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVS_CUR_STATUS_HW),
		   (unsigned int)DPE_RD32(DVS_CUR_STATUS_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVS_FRM_STATUS0_HW),
		   (unsigned int)DPE_RD32(DVS_FRM_STATUS0_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVS_FRM_STATUS2_HW),
		   (unsigned int)DPE_RD32(DVS_FRM_STATUS2_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVS_DRAM_STA0_HW),
		   (unsigned int)DPE_RD32(DVS_DRAM_STA0_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVS_DRAM_STA1_HW),
		   (unsigned int)DPE_RD32(DVS_DRAM_STA1_REG));

	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVP_IRQ_STATUS_HW),
		   (unsigned int)DPE_RD32(DVP_IRQ_STATUS_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVP_CTRL_STATUS0_HW),
		   (unsigned int)DPE_RD32(DVP_CTRL_STATUS0_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVP_CTRL_STATUS1_HW),
		   (unsigned int)DPE_RD32(DVP_CTRL_STATUS1_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVP_CTRL_STATUS2_HW),
		   (unsigned int)DPE_RD32(DVP_CTRL_STATUS2_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVP_CUR_STATUS_HW),
		   (unsigned int)DPE_RD32(DVP_CUR_STATUS_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVP_FRM_STATUS0_HW),
		   (unsigned int)DPE_RD32(DVP_FRM_STATUS0_REG));
	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVP_FRM_STATUS2_HW),
		   (unsigned int)DPE_RD32(DVP_FRM_STATUS2_REG));


	seq_printf(m, "DPE:HWProcessIdx:%d, WriteIdx:%d, ReadIdx:%d\n",
		   g_DPE_ReqRing.HWProcessIdx, g_DPE_ReqRing.WriteIdx,
		   g_DPE_ReqRing.ReadIdx);

	for (i = 0; i < _SUPPORT_MAX_DPE_REQUEST_RING_SIZE_; i++) {
		seq_printf(m,
			   "DPE:State:%d, processID:0x%08X, callerID:0x%08X, enqueReqNum:%d, FrameWRIdx:%d, RrameRDIdx:%d\n",
			   g_DPE_ReqRing.DPEReq_Struct[i].State,
			   g_DPE_ReqRing.DPEReq_Struct[i].processID,
			   g_DPE_ReqRing.DPEReq_Struct[i].callerID,
			   g_DPE_ReqRing.DPEReq_Struct[i].enqueReqNum,
			   g_DPE_ReqRing.DPEReq_Struct[i].FrameWRIdx,
			   g_DPE_ReqRing.DPEReq_Struct[i].RrameRDIdx);

		for (j = 0; j < _SUPPORT_MAX_DPE_FRAME_REQUEST_;) {
			seq_printf(m,
				   "DPE:FrmStat[%d]:%d, FrmStat[%d]:%d\n",
				   j,
			g_DPE_ReqRing.DPEReq_Struct[i].DpeFrameStatus[j],
				j + 1,
			g_DPE_ReqRing.DPEReq_Struct[i].DpeFrameStatus[j + 1]);
				j = j + 2;
		}
	}

	seq_puts(m, "\n============ dpe dump debug ============\n");

	return 0;
}


static int proc_dpe_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, dpe_dump_read, NULL);
}

static const struct file_operations dpe_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_dpe_dump_open,
	.read = seq_read,
};


static int dpe_reg_read(struct seq_file *m, void *v)
{
	unsigned int i;

	if (DPEInfo.UserCount <= 0)
		return 0;

	seq_puts(m, "======== read dpe register ========\n");

	for (i = 0x000; i <= 0x1CC; i = i + 4) {
		seq_printf(m, "[0x%08X 0x%08X]\n",
				(unsigned int)(DPE_BASE_HW + i),
				(unsigned int)DPE_RD32(ISP_DPE_BASE + i));
	}
	for (i = 0x800; i <= 0xBDC; i = i + 4) {
		seq_printf(m, "[0x%08X 0x%08X]\n",
				(unsigned int)(DPE_BASE_HW + i),
				(unsigned int)DPE_RD32(ISP_DPE_BASE + i));
	}

	return 0;
}


static ssize_t dpe_reg_write(struct file *file, const char __user *buffer,
						size_t count, loff_t *data)
{
	char desc[128];
	int len = 0;
	/*char *pEnd;*/
	char addrSzBuf[24];
	char valSzBuf[24];
	char *pszTmp;
	int addr = 0, val = 0;

	if (DPEInfo.UserCount <= 0)
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
				LOG_INF("DPE Write Addr Error!!:%s", addrSzBuf);
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
				LOG_INF("DPE Write Value Error!!:%s\n",
								valSzBuf);
			}
		}

		if ((addr >= DPE_BASE_HW) && (addr <= DVP_CTRL_ATPG_HW)
			&& ((addr & 0x3) == 0)) {
			LOG_INF("Write Request - addr:0x%x, value:0x%x\n", addr,
									val);
			DPE_WR32((ISP_DPE_BASE + (addr - DPE_BASE_HW)), val);
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
				LOG_INF("DPE Read Addr Error!!:%s", addrSzBuf);
			}
		}

		if ((addr >= DPE_BASE_HW) && (addr <= DVP_CTRL_ATPG_HW)
			&& ((addr & 0x3) == 0)) {
			val = DPE_RD32((ISP_DPE_BASE + (addr - DPE_BASE_HW)));
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

static int proc_dpe_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, dpe_reg_read, NULL);
}

static const struct file_operations dpe_reg_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_dpe_reg_open,
	.read = seq_read,
	.write = dpe_reg_write,
};

#ifndef CMDQ_COMMON
/*******************************************************************************
 *
 ******************************************************************************/

int32_t DPE_ClockOnCallback(uint64_t engineFlag)
{
	/* LOG_DBG("DPE_ClockOnCallback"); */
	/* LOG_DBG("+CmdqEn:%d", g_u4EnableClockCount); */
	/* DPE_EnableClock(MTRUE); */

	return 0;
}

int32_t DPE_DumpCallback(uint64_t engineFlag, int level)
{
	LOG_DBG("DPE DumpCallback");

	DPE_DumpReg();
	dpe_request_dump(&dpe_reqs);

	return 0;
}

int32_t DPE_ResetCallback(uint64_t engineFlag)
{
	LOG_DBG("DPE ResetCallback");
	DPE_Reset();

	return 0;
}

int32_t DPE_ClockOffCallback(uint64_t engineFlag)
{
	/* LOG_DBG("DPE_ClockOffCallback"); */
	/* DPE_EnableClock(MFALSE); */
	/* LOG_DBG("-CmdqEn:%d", g_u4EnableClockCount); */
	return 0;
}
#endif

static signed int __init DPE_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
	/* FIX-ME: linux-3.10 procfs API changed */
	/* use proc_create */
	struct proc_dir_entry *proc_entry;
	struct proc_dir_entry *isp_dpe_dir;


	int i;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = platform_driver_register(&DPEDriver);
	if (Ret < 0) {
		LOG_ERR("platform_driver_register fail");
		return Ret;
	}

	spin_lock_irq(&(DPEInfo.SpinLockDPE));
	g_isDvpInUse = 0;
	spin_unlock_irq(&(DPEInfo.SpinLockDPE));

	isp_dpe_dir = proc_mkdir("dpe", NULL);
	if (!isp_dpe_dir) {
		LOG_ERR("[%s]: fail to mkdir /proc/dpe\n", __func__);
		return 0;
	}


	proc_entry = proc_create("dpe_dump", 0444, isp_dpe_dir,
							&dpe_dump_proc_fops);

	proc_entry = proc_create("dpe_reg", 0644, isp_dpe_dir,
							&dpe_reg_proc_fops);


	/* isr log */
	if (PAGE_SIZE <
	    ((DPE_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
		((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM)) {
		i = 0;
		while (i <
		       ((DPE_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
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
		for (j = 0; j < DPE_IRQ_TYPE_AMOUNT; j++) {
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
	/* Register DPE callback */
	LOG_DBG("register dpe callback for CMDQ");
	cmdqCoreRegisterCB(CMDQ_GROUP_DPE,
			   DPE_ClockOnCallback,
			   DPE_DumpCallback, DPE_ResetCallback,
							DPE_ClockOffCallback);
#endif

#ifndef TODO
#ifdef CONFIG_MTK_IOMMU_V2
	mtk_iommu_register_fault_callback(M4U_PORT_DVS_RDMA,
					  DPE_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_DVS_WDMA,
					  DPE_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_DVP_RDMA,
					  DPE_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_DVP_WDMA,
					  DPE_M4U_TranslationFault_callback,
					  NULL);
#else
	m4u_register_fault_callback(M4U_PORT_DVS_RDMA,
			DPE_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_DVS_WDMA,
			DPE_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_DVP_RDMA,
			DPE_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_DVP_WDMA,
			DPE_M4U_TranslationFault_callback, NULL);
#endif
#endif
#ifdef DPE_PMQOS
	DPEQOS_Init();
#endif

	LOG_DBG("- X. Ret: %d.", Ret);
	return Ret;
}

/*******************************************************************************
 *
 ******************************************************************************/
static void __exit DPE_Exit(void)
{
	/*int i;*/

	LOG_DBG("- E.");

#ifdef DPE_PMQOS
	DPEQOS_Uninit();
#endif
	/*  */
	platform_driver_unregister(&DPEDriver);
	/*  */
	/* Cmdq */
	/* Unregister DPE callback */
#ifndef CMDQ_COMMON
	cmdqCoreRegisterCB(CMDQ_GROUP_DPE, NULL, NULL, NULL, NULL);
#endif
	kfree(pLog_kmalloc);

	/*  */
}


/*******************************************************************************
 *
 ******************************************************************************/
void DPE_ScheduleWork(struct work_struct *data)
{
	if (DPE_DBG_DBGLOG & DPEInfo.DebugMask)
		LOG_DBG("- E.");

	dpe_request_handler(&dpe_reqs,
			&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]));
	if (!dpe_request_running(&dpe_reqs))
		LOG_DBG("[%s]no more requests", __func__);
}


static irqreturn_t ISP_Irq_DVP(signed int Irq, void *DeviceId)
{
	unsigned int DvsStatus, DvpStatus, DvpSrc05, DvpSrc07;
	bool bResulst = MFALSE;
	//bool isDvsDone = MFALSE;
	bool isDvpDone = MFALSE;
	pid_t ProcessID;

	DvsStatus = DPE_RD32(DVS_CTRL_STATUS0_REG);	/* DVS Status */
	DvpStatus = DPE_RD32(DVP_CTRL_STATUS0_REG);	/* DVP Status */
	DvpSrc05 = DPE_RD32(DVP_SRC_05_Y_FRM0_REG);
	DvpSrc07 = DPE_RD32(DVP_SRC_07_Y_FRM2_REG);

	/* LOG_INF("DVP IRQ, DvsStatus: 0x%08x, DvpStatus: 0x%08x\n", */
	/*	DvsStatus, DvpStatus); */

	/* DPE done status may rise later, so can't use done status now  */
	/* if (DPE_INT_ST == (DPE_INT_ST & DvpStatus)) { */
		DPE_WR32(DVP_IRQ_00_REG, 0x00000FF0); /* Clear DVP IRQ */
		DPE_WR32(DVP_IRQ_00_REG, 0x00000E00); /* Clear DVP IRQ */
		isDvpDone = MTRUE;
		/*
		 *spin_lock_irq(&(DPEInfo.SpinLockDPE));
		 *g_isDvpInUse = 0;
		 *spin_unlock_irq(&(DPEInfo.SpinLockDPE));
		 */
	/* } */

	spin_lock(&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]));
	if (isDvpDone == MTRUE) {
		/* Update the frame status. */
#ifdef __DPE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("dpe_irq");
#endif
		if (dpe_update_request(&dpe_reqs, &ProcessID) == 0)
			bResulst = MTRUE;
		if (bResulst == MTRUE) {
			#if REQUEST_REGULATION == REQUEST_BASE_REGULATION
			/* schedule_work(&DPEInfo.ScheduleDpeWork); */
			queue_work(DPEInfo.wkqueue, &DPEInfo.ScheduleDpeWork);
			#endif
			DPEInfo.IrqInfo.Status[DPE_IRQ_TYPE_INT_DVP_ST] |=
				DPE_INT_ST;
			DPEInfo.IrqInfo.ProcessID[DPE_PROCESS_ID_DPE] =
				ProcessID;
			DPEInfo.IrqInfo.DpeIrqCnt++;
			DPEInfo.ProcessID[DPEInfo.WriteReqIdx] = ProcessID;
			DPEInfo.WriteReqIdx =
				(DPEInfo.WriteReqIdx + 1) %
				_SUPPORT_MAX_DPE_FRAME_REQUEST_;
		}
#ifdef __DPE_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
		/* Config the Next frame */
	}
	spin_unlock(&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]));

	if (bResulst == MTRUE)
		wake_up_interruptible(&DPEInfo.WaitQueueHead);

	/* after irq or timeout */
	cmdq_cpu_read_mem(DPE_slot, 0, &DPE_val);

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(
		DPE_IRQ_TYPE_INT_DVP_ST,
		m_CurrentPPB,
		_LOG_INF,
		"IRQ:%d,0x%x:0x%x,0x%x:0x%x,DvpSrc05:0x%X0,DvpSrc07:0x%X0,DPE_val:%u\n",
		Irq,
		DVS_CTRL_STATUS0_HW,
		DvsStatus,
		DVP_CTRL_STATUS0_HW,
		DvpStatus,
		DvpSrc05,
		DvpSrc07,
		DPE_val);
	IRQ_LOG_KEEPER(
		DPE_IRQ_TYPE_INT_DVP_ST,
		m_CurrentPPB,
		_LOG_INF,
		"IrqCnt:0x%x,Result:%d,WReq:0x%x,RReq:0x%x\n",
		DPEInfo.IrqInfo.DpeIrqCnt,
		bResulst,
		DPEInfo.WriteReqIdx,
		DPEInfo.ReadReqIdx);

	if (isDvpDone == MTRUE)
		schedule_work(&logWork);

	return IRQ_HANDLED;
}

static irqreturn_t ISP_Irq_DVS(signed int Irq, void *DeviceId)
{
	unsigned int DvsStatus, DvpStatus, DvsSrc05, DvsSrc07;
	bool isDvsDone = MFALSE;

	DvsStatus = DPE_RD32(DVS_CTRL_STATUS0_REG);	/* DVS Status */
	DvpStatus = DPE_RD32(DVP_CTRL_STATUS0_REG);	/* DVP Status */
	DvsSrc05 = DPE_RD32(DVS_SRC_05_L_FRM0_REG);
	DvsSrc07 = DPE_RD32(DVS_SRC_07_L_FRM2_REG);

	/* LOG_INF("DVS IRQ, DvsStatus: 0x%08x, DvpStatus: 0x%08x\n", */
	/*	DvsStatus, DvpStatus); */

	/* DPE done status may rise later, so can't use done status now  */
	/* if (DPE_INT_ST == (DPE_INT_ST & DvsStatus)) { */
		DPE_WR32(DVS_IRQ_00_REG, 0x00000FF0); /* Clear DVS IRQ */
		DPE_WR32(DVS_IRQ_00_REG, 0x00000E00); /* Clear DVS IRQ */
		isDvsDone = MTRUE;
	/* } */

	/* after irq or timeout */
	cmdq_cpu_read_mem(DPE_slot, 0, &DPE_val);

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(
		DPE_IRQ_TYPE_INT_DVP_ST,
		m_CurrentPPB,
		_LOG_INF,
		"IRQ:%d,0x%x:0x%x,0x%x:0x%x,DvsSrc05:0x%X0,DvsSrc07:0x%X0,DPE_val:%u\n",
		Irq,
		DVS_CTRL_STATUS0_HW,
		DvsStatus,
		DVP_CTRL_STATUS0_HW,
		DvpStatus,
		DvsSrc05,
		DvsSrc07,
		DPE_val);
	IRQ_LOG_KEEPER(
		DPE_IRQ_TYPE_INT_DVP_ST,
		m_CurrentPPB,
		_LOG_INF,
		"IrqCnt:0x%x,WReq:0x%x,RReq:0x%x\n",
		DPEInfo.IrqInfo.DpeIrqCnt,
		DPEInfo.WriteReqIdx,
		DPEInfo.ReadReqIdx);

	#if (REQUEST_REGULATION == FRAME_BASE_REGULATION)
	if (isDvsDone == MTRUE) {
		/* schedule_work(&DPEInfo.ScheduleDpeWork); */
		queue_work(DPEInfo.wkqueue, &DPEInfo.ScheduleDpeWork);
	}
	#endif

	if (isDvsDone == MTRUE)
		schedule_work(&logWork);

	return IRQ_HANDLED;
}

static void ISP_TaskletFunc_DVP(unsigned long data)
{
	IRQ_LOG_PRINTER(DPE_IRQ_TYPE_INT_DVP_ST, m_CurrentPPB, _LOG_DBG);
	IRQ_LOG_PRINTER(DPE_IRQ_TYPE_INT_DVP_ST, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(DPE_IRQ_TYPE_INT_DVP_ST, m_CurrentPPB, _LOG_ERR);

}
static void ISP_TaskletFunc_DVS(unsigned long data)
{
	IRQ_LOG_PRINTER(DPE_IRQ_TYPE_INT_DVS_ST, m_CurrentPPB, _LOG_DBG);
	IRQ_LOG_PRINTER(DPE_IRQ_TYPE_INT_DVS_ST, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(DPE_IRQ_TYPE_INT_DVS_ST, m_CurrentPPB, _LOG_ERR);

}
static void logPrint(struct work_struct *data)
{
	unsigned long arg = 0;

	ISP_TaskletFunc_DVP(arg);
	ISP_TaskletFunc_DVS(arg);
}

/******************************************************************************
 *
 ******************************************************************************/
module_init(DPE_Init);
module_exit(DPE_Exit);
MODULE_DESCRIPTION("Camera DPE driver");
MODULE_AUTHOR("MM3SW5");
MODULE_LICENSE("GPL");
