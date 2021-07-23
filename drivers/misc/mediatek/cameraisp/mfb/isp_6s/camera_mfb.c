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

/**************************************************************
 * camera_MFB.c - Linux MFB Device Driver
 *
 * DESCRIPTION:
 *     This file provid the other drivers MFB relative functions
 *
 **************************************************************/
#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
/* #include <asm/io.h> */
/* #include <asm/tcm.h> */
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
#include <linux/dma-mapping.h>
/*#include <linux/xlog.h>		 For xlog_printk(). */
/*  */
/*#include <mach/hardware.h>*/
/* #include <mach/mt6593_pll.h> */
#include "camera_mfb.h"
#include "engine_request.h"
/*#include <mach/irqs.h>*/
/* #include <mach/mt_reg_base.h> */

#include <mt-plat/sync_write.h>	/* For mt65xx_reg_sync_writel(). */
/* #include <mach/mt_spm_idle.h> For spm_enable_sodi()/spm_disable_sodi(). */

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#ifdef CONFIG_MTK_IOMMU_V2
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#else /* CONFIG_MTK_IOMMU_V2 */
#ifdef CONFIG_MTK_M4U
#include <m4u.h>
#endif /* CONFIG_MTK_IOMMU_V2 */
#endif

//#include <cmdq_core.h>
//#include <cmdq_record.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <smi_public.h>
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
#include <cmdq-util.h>
#endif

#define MFB_PMQOS
#ifdef MFB_PMQOS
#include <linux/pm_qos.h>
#include <mmdvfs_pmqos.h>
#endif

#define USE_SW_TOKEN

//#define __MFB_EP_NO_CLKMGR__
/* #define BYPASS_REG */
/* Measure the kernel performance */
/* #define __MFB_KERNEL_PERFORMANCE_MEASURE__ */
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
#include <linux/met_drv.h>
#include <linux/mtk_ftrace.h>
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

/* MFB Command Queue */
/* #include "../../cmdq/mt6797/cmdq_record.h" */
/* #include "../../cmdq/mt6797/cmdq_core.h" */

/* CCF */
#ifndef __MFB_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#include <linux/clk.h>
#if (MTK_MFB_REG_VERSION == 3)
struct MFB_CLK_STRUCT {
	struct clk *CG_IMG2_LARB11;
	struct clk *CG_IMG2_MSS;
	struct clk *CG_IMG2_MFB;
	struct clk *CG_IMG1_GALS;
};
struct MFB_CLK_STRUCT mfb_clk;
#elif (MTK_MFB_REG_VERSION == 2)
struct MFB_CLK_STRUCT {
	struct clk *CG_IMG2_LARB11;
	struct clk *CG_IMG2_MSS;
	struct clk *CG_IMG2_MFB;
};
struct MFB_CLK_STRUCT mfb_clk;
#else
struct MFB_CLK_STRUCT {
	struct clk *CG_IMG1_LARB9;
	struct clk *CG_IMG1_MSS;
	struct clk *CG_IMG1_MFB;
};
struct MFB_CLK_STRUCT mfb_clk;
#endif
#endif	/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */
#endif
/*  */
#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif

#define MFB_DEV_NAME                "camera-mfb"

/* #define MFB_WAITIRQ_LOG */
#define MFB_USE_GCE
/* #define MFB_DEBUG_USE */
/*I can' test the situation in FPGA, because the velocity of FPGA is so slow. */
#define MyTag "[MFB]"
#define IRQTag "KEEPER"

#define LOG_VRB(format,	args...)    pr_debug(MyTag format, ##args)

#ifdef MFB_DEBUG_USE
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
/* #define MFB_WR32(addr, data)  iowrite32(data, addr) // For other projects. */
/* For 89 Only.   // NEED_TUNING_BY_PROJECT */
#define MFB_WR32(addr, data)    mt_reg_sync_writel(data, addr)
#define MFB_RD32(addr)          ioread32(addr)
/******************************************************************************
 *
 ******************************************************************************/
/* dynamic log level *//*YWtodo*/
#define MFB_DBG_DBGLOG              (0x00000001)
#define MFB_DBG_INFLOG              (0x00000002)
#define MFB_DBG_INT                 (0x00000004)
#define MFB_DBG_READ_REG            (0x00000008)
#define MFB_DBG_WRITE_REG           (0x00000010)
#define MFB_DBG_TASKLET             (0x00000020)


/* ///////////////////////////////////////////////////////////////// */

/******************************************************************************
 *
 ******************************************************************************/

/* CAM interrupt status */
/* IRQ signal mask */

#define INT_ST_MASK_MSS     ( \
			MSS_INT_ST)
#define INT_ST_MASK_MSF     ( \
			MSF_INT_ST)

#define CMDQ_REG_MASK 0xffffffff

static irqreturn_t ISP_Irq_MSS(signed int Irq, void *DeviceId);
static irqreturn_t ISP_Irq_MSF(signed int Irq, void *DeviceId);
static void MFB_ScheduleMssWork(struct work_struct *data);
static void vmss_do_work(struct work_struct *data);
static void MFB_ScheduleMsfWork(struct work_struct *data);
static void vmsf_do_work(struct work_struct *data);
static signed int MSS_DumpReg(void);
static signed int MSF_DumpReg(void);

typedef irqreturn_t(*IRQ_CB) (signed int, void *);

struct ISR_TABLE {
	IRQ_CB isr_fp;
	unsigned int int_number;
	char device_name[16];
};

#ifndef CONFIG_OF
const struct ISR_TABLE MFB_IRQ_CB_TBL[MFB_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_MSS, MSS_IRQ_BIT_ID, "mss"},
	{ISP_Irq_MSF, MSF_IRQ_BIT_ID, "msf"},
};

#else
/* int number is got from kernel api */
const struct ISR_TABLE MFB_IRQ_CB_TBL[MFB_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_MSS, 0, "mss"},
	{ISP_Irq_MSF, 0, "msf"},
};

#endif
/* ////////////////////////////////////////////////////////////////////////// */
/*  */
typedef void (*tasklet_cb) (unsigned long);
struct Tasklet_table {
	tasklet_cb tkt_cb;
	struct tasklet_struct *pMFB_tkt;
};

struct tasklet_struct Mfbtkt[MFB_IRQ_TYPE_AMOUNT];

static void ISP_TaskletFunc_MSS(unsigned long data);
static void ISP_TaskletFunc_MSF(unsigned long data);

static struct Tasklet_table MFB_tasklet[MFB_IRQ_TYPE_AMOUNT] = {
	{ISP_TaskletFunc_MSS, &Mfbtkt[MFB_IRQ_TYPE_INT_MSS_ST]},
	{ISP_TaskletFunc_MSF, &Mfbtkt[MFB_IRQ_TYPE_INT_MSF_ST]},
};
static struct work_struct logWork;
static void logPrint(struct work_struct *data);

struct wakeup_source MSS_wake_lock;
struct wakeup_source MSF_wake_lock;

static DEFINE_MUTEX(gMfbMssMutex);
static DEFINE_MUTEX(gMfbMssDequeMutex);

static DEFINE_MUTEX(gMfbMsfMutex);
static DEFINE_MUTEX(gMfbMsfDequeMutex);

static DEFINE_MUTEX(MutexMFBRef);

#ifdef CONFIG_OF

struct MFB_device {
	void __iomem *regs;
	struct device *dev;
	int irq;
};

static struct MFB_device *MFB_devs;
static int nr_MFB_devs;

static struct device *MFB_cmdq_dev;
static struct cmdq_client *mss_clt, *msf_clt;
static u16 mss_done_event_id, msf_done_event_id;
static u16 mss_token_id, msf_token_id;


/* Get HW modules' base address from device nodes */
#define MSS_DEV_NODE_IDX 0
#define MSF_DEV_NODE_IDX 1
#define IMGSYS_DEV_MODE_IDX 2

/* static unsigned long gISPSYS_Reg[MFB_IRQ_TYPE_AMOUNT]; */


#define ISP_MSS_BASE		(MFB_devs[MSS_DEV_NODE_IDX].regs)
#define ISP_MSF_BASE		(MFB_devs[MSF_DEV_NODE_IDX].regs)
#define ISP_IMGSYS_BASE		(MFB_devs[IMGSYS_DEV_MODE_IDX].regs)
/* #define ISP_MFB_BASE	(gISPSYS_Reg[MFB_DEV_NODE_IDX]) */

#else
#define ISP_MSS_BASE		(0x15012000)/*YWtodo*/
#define ISP_MSF_BASE		(0x15010000)/*YWtodo*/

#endif


static unsigned int g_u4EnableClockCount;
static unsigned int g_SuspendCnt;

/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 31

#ifdef MFB_PMQOS
static struct pm_qos_request mfb_pmqos_request;
static u64 max_img_freq[4];
struct plist_head module_request_list;  /* all module list */
struct mm_qos_request mfb_mmqos_request;

static spinlock_t SpinLockMfbPmqos;
static unsigned int qos_scen[4];
static unsigned int qos_total;
#endif

struct  MSS_CONFIG_STRUCT {
	struct MFB_MSSConfig MssFrameConfig[_SUPPORT_MAX_MFB_FRAME_REQUEST_];
	struct MFB_MSSConfig vMssFrameConfig[_SUPPORT_MAX_MFB_FRAME_REQUEST_];
};

static struct MSS_CONFIG_STRUCT g_MssEnqueReq_Struct;
static struct MSS_CONFIG_STRUCT g_MssDequeReq_Struct;

struct  MSF_CONFIG_STRUCT {
	struct MFB_MSFConfig MsfFrameConfig[_SUPPORT_MAX_MFB_FRAME_REQUEST_];
};

static struct MSF_CONFIG_STRUCT g_MsfEnqueReq_Struct;
static struct MSF_CONFIG_STRUCT g_MsfDequeReq_Struct;

static struct engine_requests mss_reqs;
static struct engine_requests vmss_reqs;
static struct MFB_MSSRequest kMssReq;
static struct engine_requests msf_reqs;
static struct engine_requests vmsf_reqs;
static struct MFB_MSFRequest kMsfReq;

#ifdef CONFIG_MTK_IOMMU_V2
static int MFB_MEM_USE_VIRTUL = 1;
#endif

/******************************************************************************
 *
 ******************************************************************************/
enum MFB_PROCESS_ID_ENUM {
	MFB_PROCESS_ID_NONE,
	MFB_PROCESS_ID_MSS,
	MFB_PROCESS_ID_vMSS,
	MFB_PROCESS_ID_MSF,
	MFB_PROCESS_ID_vMSF,
	MFB_PROCESS_ID_AMOUNT
};

struct  MFB_USER_INFO_STRUCT {
	pid_t Pid;
	pid_t Tid;
	struct engine_requests *reqs;
	enum MFB_PROCESS_ID_ENUM streamtag;
};

/******************************************************************************
 *
 ******************************************************************************/
struct MFB_IRQ_INFO_STRUCT {
	unsigned int Status[MFB_IRQ_TYPE_AMOUNT];
	signed int MssIrqCnt[IRQ_USER_NUM_MAX];
	signed int MssIrqUse[IRQ_USER_NUM_MAX];
	signed int MsfIrqCnt[IRQ_USER_NUM_MAX];
	pid_t ProcessID[IRQ_USER_NUM_MAX];
	pid_t ProcessFD[IRQ_USER_NUM_MAX];
	unsigned int Mask[MFB_IRQ_TYPE_AMOUNT];
};

struct MFB_INFO_STRUCT {
	spinlock_t SpinLockMFB;
	spinlock_t SpinLockIrq[MFB_IRQ_TYPE_AMOUNT];
	wait_queue_head_t WaitQueueHeadMss;
	wait_queue_head_t WaitQueueHeadMsf;
	struct work_struct ScheduleMssWork;
	struct work_struct vmsswork;
	struct work_struct ScheduleMsfWork;
	struct work_struct vmsfwork;
	struct workqueue_struct *wkqueueMss;
	struct workqueue_struct *wkqueueMsf;
	unsigned int UserCount;	/* User Count */
	unsigned int DebugMaskMss;	/* Debug Mask */
	unsigned int DebugMaskMsf;	/* Debug Mask */
	struct MFB_IRQ_INFO_STRUCT IrqInfo;
};

static struct MFB_INFO_STRUCT MFBInfo;

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
static struct SV_LOG_STR gSvLog[MFB_IRQ_TYPE_AMOUNT];

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
	int ret; \
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
		ret = snprintf((char *)(pDes), avaLen, fmt,\
			##__VA_ARGS__);   \
		if (ret < 0) { \
			LOG_ERR("snprintf fail(%d)\n", ret); \
		} \
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
		ret = snprintf((char *)(pDes), avaLen, fmt, ##__VA_ARGS__);\
		if (ret < 0) { \
			LOG_ERR("snprintf fail(%d)\n", ret); \
		} \
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
	unsigned int ppb = 0;\
	unsigned int logT = 0;\
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

/******************************************************************************
 *
 ******************************************************************************/
#define IMGSYS_REG_CG_CON      (ISP_IMGSYS_BASE + 0x0)
#define IMGSYS_REG_CG_SET      (ISP_IMGSYS_BASE + 0x4)
#define IMGSYS_REG_CG_CLR      (ISP_IMGSYS_BASE + 0x8)
#define IMGSYS_REG_SW_RST      (ISP_IMGSYS_BASE + 0xc)

#define MFB_MSS_INT_STATUS_REG  (ISP_MSS_BASE + 0x508)
#define MFB_MSF_INT_STATUS_REG  (ISP_MSF_BASE + 0x7c8)

#define MFB_MSS_TDRI_BASE_REG  (ISP_MSS_BASE + 0x804)
#define MFB_MSF_TDRI_BASE_REG  (ISP_MSF_BASE + 0x804)

#define MFB_MSS_TDRI_OFST_REG  (ISP_MSS_BASE + 0x808)
#define MFB_MSF_TDRI_OFST_REG  (ISP_MSF_BASE + 0x808)

#define MFB_MSS_CMDQ_BASE_ADDR_REG  (ISP_MSS_BASE + 0x500)
#define MFB_MSF_CMDQ_BASE_ADDR_REG  (ISP_MSF_BASE + 0x7C0)

#define MFB_MSS_CQLP_CMD_NUM_REG  (ISP_MSS_BASE + 0x600)
#define MFB_MSF_CQLP_CMD_NUM_REG  (ISP_MSF_BASE + 0x7F0)

#define MSSTOP_DMA_STOP_REG     (ISP_MSS_BASE + 0x414)
#define MSSTOP_DMA_STOP_STA_REG (ISP_MSS_BASE + 0x418)

#define MFBTOP_DMA_STOP_REG     (ISP_MSF_BASE + 0x4A4)
#define MFBTOP_DMA_STOP_STA_REG (ISP_MSF_BASE + 0x4A8)
#define MFBTOP_RESET_REG        (ISP_MSF_BASE + 0x4AC)
#define MFB_MSF_CMDQ_ENABLE_REG (ISP_MSF_BASE + 0x7C4)

#define MFB_MSF_TOP_DEBUG_SEL   (ISP_MSF_BASE + 0x4D0)
#define MFB_MSF_TOP_DEBUG_STA   (ISP_MSF_BASE + 0x4D4)
#define MFB_MSF_DMA_DEBUG_SEL   (ISP_MSF_BASE + 0x888)
#define MFB_MSF_TOP_CRC_1       (ISP_MSF_BASE + 0x4CC)

#define MFB_MSS_TOP_DBG_CTL0    (ISP_MSS_BASE + 0x434)
#define MFB_MSS_TOP_DBG_OUT2    (ISP_MSS_BASE + 0x440)
#define MFB_MSS_TOP_DBG_OUT3    (ISP_MSS_BASE + 0x444)
#define MFB_MSS_DMA_DEBUG_SEL   (ISP_MSS_BASE + 0x888)

#if (MTK_MFB_REG_VERSION >= 2)
#define MSS_BASE 0x15812000
#define MSF_BASE 0x15810000
#else
#define MSS_BASE 0x15012000
#define MSF_BASE 0x15010000
#endif

#define MFB_MSS_INT_STATUS_HW  (MSS_BASE + 0x508)
#define MFB_MSF_INT_STATUS_HW  (MSF_BASE + 0x7c8)

#define ISP_MSS_BASE_HW    (MSS_BASE + 0x000)
#define ISP_MSF_BASE_HW    (MSF_BASE + 0x000)

#define MFB_MSS_START_HW   (MSS_BASE + 0x50c)
#define MFB_MSF_START_HW   (MSF_BASE + 0x7cc)

#define MFB_MSS_INT_STA_HW  (MSS_BASE + 0x508)
#define MFB_MSF_INT_STA_HW  (MSF_BASE + 0x7c8)

#define MFB_MSS_CMDQ_ENABLE_HW  (MSS_BASE + 0x504)
#define MFB_MSF_CMDQ_ENABLE_HW  (MSF_BASE + 0x7C4)

#define MFB_MSS_CMDQ_BASE_ADDR_HW  (MSS_BASE + 0x500)
#define MFB_MSF_CMDQ_BASE_ADDR_HW  (MSF_BASE + 0x7C0)

#define MFB_MSS_CQLP_CMD_NUM_HW  (MSS_BASE + 0x600)
#define MFB_MSF_CQLP_CMD_NUM_HW  (MSF_BASE + 0x7F0)

#define MFB_MSS_CQLP_ENG_EN_HW  (MSS_BASE + 0x604)
#define MFB_MSF_CQLP_ENG_EN_HW  (MSF_BASE + 0x7F4)

#define MFB_MSS_TDRI_BASE_HW  (MSS_BASE + 0x804)
#define MFB_MSS_TDRI_OFST_HW  (MSS_BASE + 0x808)
#define MFB_MSF_TDRI_BASE_HW  (MSF_BASE + 0x804)
#define MFB_MSF_TDRI_OFST_HW  (MSF_BASE + 0x808)

#define MFB_MSS_IY_BASE_HW  (MSS_BASE + 0xA00)
#define MFB_MSS_IC_BASE_HW  (MSS_BASE + 0xA40)
#define MFB_MSS_OY_BASE_HW  (MSS_BASE + 0x900)
#define MFB_MSS_OC_BASE_HW  (MSS_BASE + 0x940)

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int MFB_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int MFB_UsToJiffies(unsigned int Us)
{
	return (((Us / 1000) * HZ + 512) >> 10);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int MSS_GetIRQState(
	unsigned int type, unsigned int *userNumber, unsigned int stus,
	enum MFB_PROCESS_ID_ENUM whichReq, int ProcessID)
{
	unsigned int ret = 0;
	unsigned int p;
	unsigned long flags; /* old: unsigned int flags;*/
			     /* FIX to avoid build warning */

	/*  */
	p = ProcessID % IRQ_USER_NUM_MAX;
	spin_lock_irqsave(&(MFBInfo.SpinLockIrq[type]), flags);
	if (stus & MSS_INT_ST) {
		LOG_DBG("%s MssIrqCnt[%d] is %d for pid %d", __func__,
			p,
			MFBInfo.IrqInfo.MssIrqCnt[p],
			MFBInfo.IrqInfo.ProcessID[p]);
		ret = ((MFBInfo.IrqInfo.MssIrqCnt[p] > 0)
		       && (MFBInfo.IrqInfo.ProcessID[p] == ProcessID));
	} else {
		LOG_ERR(
		"WaitIRQ Status Error, type:%d, userNumber:%d, status:%d, whichReq:%d, ProcessID:0x%x\n",
		     type, *userNumber, stus, p, ProcessID);
	}
	*userNumber = ret;
	if (ret == 1 && MFBInfo.IrqInfo.MssIrqCnt[p] == 1) {
		LOG_DBG("%s last mssirqcnt %d clearing pid %d for proc %d",
			__func__,
			MFBInfo.IrqInfo.MssIrqCnt[p],
			MFBInfo.IrqInfo.ProcessID[p],
			p);
		MFBInfo.IrqInfo.ProcessID[p] = 0;
	}
	spin_unlock_irqrestore(&(MFBInfo.SpinLockIrq[type]), flags);
	/*  */
	return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int MSF_GetIRQState(
	unsigned int type, unsigned int userNumber, unsigned int stus,
	enum MFB_PROCESS_ID_ENUM whichReq, int ProcessID)
{
	unsigned int ret = 0;
	unsigned int p;
	unsigned long flags; /* old: unsigned int flags;*/
			     /* FIX to avoid build warning */

	/*  */
	p = ProcessID % IRQ_USER_NUM_MAX;
	spin_lock_irqsave(&(MFBInfo.SpinLockIrq[type]), flags);
	if (stus & MSF_INT_ST) {
		LOG_DBG("%s MsfIrqCnt is %d for pid %d", __func__,
			MFBInfo.IrqInfo.MsfIrqCnt[p],
			MFBInfo.IrqInfo.ProcessFD[p]);
		ret = ((MFBInfo.IrqInfo.MsfIrqCnt[p] > 0)
		       && (MFBInfo.IrqInfo.ProcessFD[p] == ProcessID));
	} else {
		LOG_ERR(
		"WaitIRQ Status Error, type:%d, userNumber:%d, status:%d, whichReq:%d, ProcessID:0x%x\n",
		     type, userNumber, stus, p, ProcessID);
	}
	spin_unlock_irqrestore(&(MFBInfo.SpinLockIrq[type]), flags);
	if (ret == 1) {
		LOG_DBG("%s last msfirqcnt %d not clearing pid %d for proc %d",
			__func__,
			MFBInfo.IrqInfo.MsfIrqCnt[p],
			MFBInfo.IrqInfo.ProcessFD[p],
			p);
	}
	/*  */
	return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int MFB_JiffiesToMs(unsigned int Jiffies)
{
	return ((Jiffies * 1000) / HZ);
}

#ifdef MFB_PMQOS
void MFBQOS_Init(void)
{
	s32 result = 0;
	u64 img_freq_steps[MAX_FREQ_STEP];
	u32 step_size;

	/* Call pm_qos_add_request when initialize module or driver prob */
	pm_qos_add_request(
		&mfb_pmqos_request,
		PM_QOS_IMG_FREQ,
		PM_QOS_MM_FREQ_DEFAULT_VALUE);

	/* Call mmdvfs_qos_get_freq_steps to get supported frequency */
	result = mmdvfs_qos_get_freq_steps(
		PM_QOS_IMG_FREQ,
		img_freq_steps,
		&step_size);

	if (result < 0 || step_size == 0)
		LOG_INF("get MMDVFS freq steps failed, result: %d\n", result);
	else {
		max_img_freq[0] = img_freq_steps[0];
		max_img_freq[1] = img_freq_steps[1];
		max_img_freq[2] = img_freq_steps[2];
		max_img_freq[3] = img_freq_steps[3];
	}

	/* Initialize owner list */
	plist_head_init(&module_request_list);

	/* Call mm_qos_add_request */
	/* when initialize module or driver prob */
#if (MTK_MFB_REG_VERSION >= 2)
	mm_qos_add_request(&module_request_list,
		&mfb_mmqos_request, M4U_PORT_L11_IMG_MFB_RDMA0);
#else
	mm_qos_add_request(&module_request_list,
		&mfb_mmqos_request, M4U_PORT_L9_IMG_MFB_RDMA0_MDP);
#endif
}

void MFBQOS_Uninit(void)
{
	pm_qos_remove_request(&mfb_pmqos_request);

	/* Call mm_qos_remove_request */
	/* when de-initialize module or driver remove */
	mm_qos_remove_all_request(&module_request_list);
}

void MFBQOS_Update(bool start, unsigned int scen, unsigned long bw)
{
	LOG_DBG("start: %d, MFB scen: %d, bw: %lu", start, scen, bw);
	if (start) { /* start MFB, configure MMDVFS to highest CLK */
		LOG_DBG("MFB total: %ld", qos_total);
		spin_lock(&(SpinLockMfbPmqos));
		if (bw != 0)
			qos_scen[scen] = bw;
		qos_total = qos_total + bw;
		if (qos_total > 600000000) {
			spin_unlock(&(SpinLockMfbPmqos));
			pm_qos_update_request(&mfb_pmqos_request,
						max_img_freq[0]);
		} else if (qos_total > 300000000) {
			spin_unlock(&(SpinLockMfbPmqos));
			pm_qos_update_request(&mfb_pmqos_request,
						max_img_freq[1]);
		} else if (qos_total > 100000000) {
			spin_unlock(&(SpinLockMfbPmqos));
			pm_qos_update_request(&mfb_pmqos_request,
						max_img_freq[2]);
		} else {
			spin_unlock(&(SpinLockMfbPmqos));
			pm_qos_update_request(&mfb_pmqos_request, 0);
		}
	} else { /* finish MFB, config MMDVFS to lowest CLK */
		LOG_DBG("MFB total: %ld", qos_total);
		spin_lock(&(SpinLockMfbPmqos));
		qos_total = qos_total - qos_scen[scen];
		if (qos_total > 600000000) {
			spin_unlock(&(SpinLockMfbPmqos));
			pm_qos_update_request(&mfb_pmqos_request,
						max_img_freq[0]);
		} else if (qos_total > 300000000) {
			spin_unlock(&(SpinLockMfbPmqos));
			pm_qos_update_request(&mfb_pmqos_request,
						max_img_freq[1]);
		} else if (qos_total > 100000000) {
			spin_unlock(&(SpinLockMfbPmqos));
			pm_qos_update_request(&mfb_pmqos_request,
						max_img_freq[2]);
		} else {
			spin_unlock(&(SpinLockMfbPmqos));
			pm_qos_update_request(&mfb_pmqos_request, 0);
		}
	}
#if 0 /*YWtodo*/
	if (start) {
		/* Call mm_qos_set_request API to setup estimated data bw */
		mm_qos_set_request(&mfb_mmqos_request,
					bw/1000000, 0, BW_COMP_NONE);
		/* Call mm_qos_update_all_requests API */
		/* update necessary HW configuration for MM BW */
		mm_qos_update_all_request(&module_request_list);
	} else {
		mm_qos_set_request(&mfb_mmqos_request, 0, 0, BW_COMP_NONE);
		mm_qos_update_all_request(&module_request_list);
	}
#endif
}
#endif

#if 0/*YWtodo*/
static int cmdq_engine_secured(struct cmdqRecStruct *handle,
						enum CMDQ_ENG_ENUM engine)
{
	cmdqRecSetSecure(handle, 1);
	cmdqRecSecureEnablePortSecurity(handle, (1LL << engine));
	cmdqRecSecureEnableDAPC(handle, (1LL << engine));

	return 0;
}
#endif

/*TODO : M4U_PORT */
#if 0
static int cmdq_sec_base(struct cmdqRecStruct *handle, unsigned int dma_sec,
			unsigned int reg, unsigned int val, unsigned int size)
{
	if (dma_sec != 0)
		cmdqRecWriteSecure(handle, reg, CMDQ_SAM_H_2_MVA, val, 0, size,
							M4U_PORT_MFB_RDMA);
	else
		cmdqRecWrite(handle, reg, val, CMDQ_REG_MASK);

	return 0;
}
#endif

static void mss_pkt_tcmds(struct cmdq_pkt *handle,
				struct MFB_MSSConfig *pMssConfig)
{
	unsigned int t;

	if (pMssConfig->tpipe_used > TPIPE_NUM_PER_FRAME) {
		pMssConfig->tpipe_used = TPIPE_NUM_PER_FRAME;
		LOG_ERR("tpipe_used %d over limit of %d",
				pMssConfig->tpipe_used, TPIPE_NUM_PER_FRAME);
	}

	/* fixed 0x0000 0001: [0] MSS, [1] P2*/
	cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSS_CQLP_ENG_EN_HW,
			pMssConfig->MSSCQLP_ENG_EN, CMDQ_REG_MASK);
	for (t = 0; t < pMssConfig->tpipe_used; t++) {
		/* cq */
		if (pMssConfig->MSSCMDQ_BASE[t] == 0) {
			LOG_ERR("tpipe_used = %d MSSCMDQ_BASE = 0",
					pMssConfig->tpipe_used);
			break;
		}
		cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSS_CMDQ_ENABLE_HW,
				pMssConfig->MSSCMDQ_ENABLE[t], CMDQ_REG_MASK);
		cmdq_pkt_write(handle, NULL,
				(dma_addr_t)MFB_MSS_CMDQ_BASE_ADDR_HW,
				pMssConfig->MSSCMDQ_BASE[t], CMDQ_REG_MASK);
		cmdq_pkt_write(handle, NULL,
				(dma_addr_t)MFB_MSS_CQLP_CMD_NUM_HW,
				pMssConfig->MSSCQLP_CMD_NUM[t], CMDQ_REG_MASK);
		/* tdr */
		cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSS_TDRI_BASE_HW,
				pMssConfig->MSSDMT_TDRI_BASE[t], CMDQ_REG_MASK);

		/* I/O */
		if (pMssConfig->update_dma_en[t] == 1) {
			cmdq_pkt_write(handle, NULL,
				(dma_addr_t)MFB_MSS_IY_BASE_HW,
				pMssConfig->dmas[t].MSSDMT_IY_BASE,
				CMDQ_REG_MASK);
			cmdq_pkt_write(handle, NULL,
				(dma_addr_t)MFB_MSS_IC_BASE_HW,
				pMssConfig->dmas[t].MSSDMT_IC_BASE,
				CMDQ_REG_MASK);
			cmdq_pkt_write(handle, NULL,
				(dma_addr_t)MFB_MSS_OY_BASE_HW,
				pMssConfig->dmas[t].MSSDMT_OY_BASE,
				CMDQ_REG_MASK);
			cmdq_pkt_write(handle, NULL,
				(dma_addr_t)MFB_MSS_OC_BASE_HW,
				pMssConfig->dmas[t].MSSDMT_OC_BASE,
				CMDQ_REG_MASK);
		}
		/* start */
		cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSS_START_HW,
				0x1, CMDQ_REG_MASK);
		/* wfe */
		cmdq_pkt_wfe(handle, mss_done_event_id);
		cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSS_INT_STA_HW,
				0x1, CMDQ_REG_MASK);
		LOG_DBG("MSS_CMDQ_ENABLE%d = 0x%x", t,
						pMssConfig->MSSCMDQ_ENABLE[t]);
		LOG_DBG("MSSCMDQ_BASE%d = 0x%x", t,
						pMssConfig->MSSCMDQ_BASE[t]);
		LOG_DBG("MSSCQLP_CMD_NUM%d = 0x%x", t,
						pMssConfig->MSSCQLP_CMD_NUM[t]);
		LOG_DBG("MSSDMT_TDRI_BASE%d = 0x%x", t,
					pMssConfig->MSSDMT_TDRI_BASE[t]);
	}
	LOG_DBG("%s: tpipe_used is %d", __func__, pMssConfig->tpipe_used);
	LOG_INF("mss cmdq write done %d", pMssConfig->tpipe_used);

}

static void mss_norm_sirq(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt;
	bool bResulst = MFALSE;
	pid_t ProcessID;
	unsigned long flag;
	unsigned int p = 0;

	pkt = (struct cmdq_pkt *) data.data;
	if (data.err < 0) {
		mfb_request_dump(&mss_reqs);
		MSS_DumpReg();
		LOG_ERR("%s_mss: call back error(%d)", __func__, data.err);
		goto EXIT;
	}

	spin_lock_irqsave(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]),
									flag);
	if (mfb_update_request(&mss_reqs, &ProcessID) == 0)
		bResulst = MTRUE;
	/* Config the Next frame */
	if (bResulst == MTRUE) {
		#if REQUEST_REGULATION == REQUEST_BASE_REGULATION
		queue_work(MFBInfo.wkqueueMss, &MFBInfo.ScheduleMssWork);
		#endif
		p = ProcessID % IRQ_USER_NUM_MAX;
		MFBInfo.IrqInfo
		    .Status[MFB_IRQ_TYPE_INT_MSS_ST] |= MSS_INT_ST;
		MFBInfo.IrqInfo
		    .ProcessID[p] = ProcessID;
		MFBInfo.IrqInfo.MssIrqCnt[p]++;
	}
	spin_unlock_irqrestore(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]),
									flag);
	if (bResulst == MTRUE) {
#ifdef MFB_PMQOS
		MFBQOS_Update(0, 0, 0);
#endif
		wake_up_interruptible(&MFBInfo.WaitQueueHeadMss);
	}

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(MFB_IRQ_TYPE_INT_MSS_ST, m_CurrentPPB, _LOG_INF,
		       "%s: bResulst:%d, MssIrqCnt[%d]:0x%x\n",
		       __func__, bResulst, p,
			MFBInfo.IrqInfo.MssIrqCnt[p]);

	#if (REQUEST_REGULATION == FRAME_BASE_REGULATION)
	queue_work(MFBInfo.wkqueueMss, &MFBInfo.ScheduleMssWork);
	#endif

	tasklet_schedule(MFB_tasklet[MFB_IRQ_TYPE_INT_MSS_ST].pMFB_tkt);
EXIT:
	cmdq_pkt_destroy(pkt);
}

signed int CmdqMSSHW(struct frame *frame)
{
	struct MFB_MSSConfig *pMssConfig;
	struct cmdq_pkt *handle;

	if (frame == NULL || frame->data == NULL)
		return -1;

	LOG_DBG("%s request sent to CMDQ driver", __func__);
	pMssConfig = (struct MFB_MSSConfig *) frame->data;

	if (MFB_DBG_DBGLOG == (MFB_DBG_DBGLOG & MFBInfo.DebugMaskMss))
		LOG_DBG("ConfigMSSHW Start!\n");

	/*cmdq_pkt_cl_create(&handle, mss_clt);*/
	handle = cmdq_pkt_create(mss_clt);

	handle->priority = 20;

	/*if (pMssConfig->eng_secured == 1)*//*YWtodo*/
		/*cmdq_engine_secured(handle, CMDQ_ENG_MSS);*/
#ifdef USE_SW_TOKEN
	cmdq_pkt_acquire_event(handle, mss_token_id);
#endif
#define TPIPE_MODE_PREVIEW
#ifdef TPIPE_MODE_PREVIEW
	mss_pkt_tcmds(handle, pMssConfig);
#else
	cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSS_CMDQ_ENABLE_HW,
			pMssConfig->MSSCMDQ_ENABLE[0], CMDQ_REG_MASK);
	cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSS_CMDQ_BASE_ADDR_HW,
			pMssConfig->MSSCMDQ_BASE[0], CMDQ_REG_MASK);
	cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSS_CQLP_CMD_NUM_HW,
			pMssConfig->MSSCQLP_CMD_NUM[0], CMDQ_REG_MASK);
	/* fixed 0x0000 0001: [0] MSS, [1] P2*/
	cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSS_CQLP_ENG_EN_HW,
			pMssConfig->MSSCQLP_ENG_EN, CMDQ_REG_MASK);
#ifndef BYPASS_REG
	cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSS_START_HW,
			0x1, CMDQ_REG_MASK);
	cmdq_pkt_wfe(handle, mss_done_event_id);
	/* non-blocking API, Please  use cmdqRecFlushAsync() */
#endif
#endif
#ifdef USE_SW_TOKEN
	cmdq_pkt_clear_event(handle, mss_token_id);
#endif
#ifdef MFB_PMQOS
	MFBQOS_Update(1, 0, pMssConfig->qos);
#endif
	cmdq_pkt_flush_threaded(handle, mss_norm_sirq, (void *)handle);

	return 0;
}

signed int mss_enque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt;
	struct MFB_MSSRequest *_req;
	struct MFB_MSSConfig *pcfg;

	_req = (struct MFB_MSSRequest *) req;

	if (frames == NULL || _req == NULL)
		return -1;

	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		LOG_DBG("[%s]request enque frame(%d/%d) 0x%p",
					__func__, f, fcnt, frames[f].data);
		memcpy(frames[f].data, &_req->m_pMssConfig[f],
						sizeof(struct MFB_MSSConfig));
		pcfg = &_req->m_pMssConfig[f];
	}

	return 0;
}

signed int mss_deque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt;
	struct MFB_MSSRequest *_req;
	struct MFB_MSSConfig *pcfg;

	_req = (struct MFB_MSSRequest *) req;

	if (frames == NULL || _req == NULL)
		return -1;

	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		memcpy(&_req->m_pMssConfig[f], frames[f].data,
						sizeof(struct MFB_MSSConfig));
		LOG_DBG("[%s]request deque frame(%d/%d).", __func__, f, fcnt);
		pcfg = &_req->m_pMssConfig[f];
	}

	return 0;
}

static void mss_vss_sirq(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt;
	bool bResulst = MFALSE;
	pid_t ProcessID;
	unsigned long flag;
	unsigned int p = 0;

	pkt = (struct cmdq_pkt *) data.data;
	if (data.err < 0) {
		mfb_request_dump(&vmss_reqs);
		MSS_DumpReg();
		LOG_ERR("%s_mss: call back error(%d)", __func__, data.err);
		goto EXIT;
	}

	spin_lock_irqsave(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]),
									flag);
	if (mfb_update_request(&vmss_reqs, &ProcessID) == 0)
		bResulst = MTRUE;
	/* Config the Next frame */
	if (bResulst == MTRUE) {
		#if REQUEST_REGULATION == REQUEST_BASE_REGULATION
		queue_work(MFBInfo.wkqueueMss, &MFBInfo.vmsswork);
		#endif
		p = ProcessID % IRQ_USER_NUM_MAX;
		MFBInfo.IrqInfo
		    .Status[MFB_IRQ_TYPE_INT_MSS_ST] |= MSS_INT_ST;
		MFBInfo.IrqInfo
		    .ProcessID[p] = ProcessID;
		MFBInfo.IrqInfo.MssIrqCnt[p]++;
	}
	spin_unlock_irqrestore(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]),
									flag);
	if (bResulst == MTRUE) {
#ifdef MFB_PMQOS
		MFBQOS_Update(0, 1, 0);
#endif
		wake_up_interruptible(&MFBInfo.WaitQueueHeadMss);
	}

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(MFB_IRQ_TYPE_INT_MSS_ST, m_CurrentPPB, _LOG_INF,
		       "%s: bResulst:%d, MssIrqCnt[%d]:0x%x\n",
		       __func__, bResulst, p,
			MFBInfo.IrqInfo.MssIrqCnt[p]);
	tasklet_schedule(MFB_tasklet[MFB_IRQ_TYPE_INT_MSS_ST].pMFB_tkt);

	#if (REQUEST_REGULATION == FRAME_BASE_REGULATION)
	queue_work(MFBInfo.wkqueueMss, &MFBInfo.vmsswork);
	#endif
EXIT:
	cmdq_pkt_destroy(pkt);
}

signed int vCmdqMSSHW(struct frame *frame)
{
	struct MFB_MSSConfig *pMssConfig;
	struct cmdq_pkt *handle;

	if (frame == NULL || frame->data == NULL)
		return -1;

	LOG_DBG("%s request sent to CMDQ driver", __func__);
	pMssConfig = (struct MFB_MSSConfig *) frame->data;

	if (MFB_DBG_DBGLOG == (MFB_DBG_DBGLOG & MFBInfo.DebugMaskMss))
		LOG_DBG("ConfigMSSHW Start!\n");

	/*cmdq_pkt_cl_create(&handle, mss_clt);*/
	handle = cmdq_pkt_create(mss_clt);
	handle->priority = 0;
#ifdef USE_SW_TOKEN
	cmdq_pkt_acquire_event(handle, mss_token_id);
#endif
	mss_pkt_tcmds(handle, pMssConfig);
#ifdef USE_SW_TOKEN
	cmdq_pkt_clear_event(handle, mss_token_id);
#endif
#ifdef MFB_PMQOS
	MFBQOS_Update(1, 1, pMssConfig->qos);
#endif
	cmdq_pkt_flush_threaded(handle, mss_vss_sirq, (void *)handle);

	return 0;
}

static const struct engine_ops mss_ops = {
	.req_enque_cb = mss_enque_cb,
	.req_deque_cb = mss_deque_cb,
	.frame_handler = CmdqMSSHW,
	.req_feedback_cb = NULL,
};

static const struct engine_ops vmss_ops = {
	.req_enque_cb = mss_enque_cb,
	.req_deque_cb = mss_deque_cb,
	.frame_handler = vCmdqMSSHW,
	.req_feedback_cb = NULL,
};


static void msf_pkt_tcmds(struct cmdq_pkt *handle,
				struct MFB_MSFConfig *pMsfConfig)
{
	unsigned int t;

	if (pMsfConfig->tpipe_used > TPIPE_NUM_PER_FRAME) {
		pMsfConfig->tpipe_used = TPIPE_NUM_PER_FRAME;
		LOG_ERR("tpipe_used %d over limit of %d",
				pMsfConfig->tpipe_used, TPIPE_NUM_PER_FRAME);
	}

	/* fixed 0x0000 0001: [0] MSS, [1] P2*/
	cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSF_CQLP_ENG_EN_HW,
			pMsfConfig->MSFCQLP_ENG_EN, CMDQ_REG_MASK);
	for (t = 0; t < pMsfConfig->tpipe_used; t++) {
		cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSF_CMDQ_ENABLE_HW,
				pMsfConfig->MSFCMDQ_ENABLE[t], CMDQ_REG_MASK);
		cmdq_pkt_write(handle, NULL,
				(dma_addr_t)MFB_MSF_CMDQ_BASE_ADDR_HW,
				pMsfConfig->MSFCMDQ_BASE[t], CMDQ_REG_MASK);
		cmdq_pkt_write(handle, NULL,
				(dma_addr_t)MFB_MSF_CQLP_CMD_NUM_HW,
				pMsfConfig->MSFCQLP_CMD_NUM[t], CMDQ_REG_MASK);
		/* tdr */
		cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSF_TDRI_BASE_HW,
				pMsfConfig->MFBDMT_TDRI_BASE[t], CMDQ_REG_MASK);
		/* start */
		cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSF_START_HW,
				0x1, CMDQ_REG_MASK);
		/* wfe */
		cmdq_pkt_wfe(handle, msf_done_event_id);
		cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSF_INT_STA_HW,
				0x1, CMDQ_REG_MASK);
		LOG_DBG("MSF_CMDQ_ENABLE%d = 0x%x", t,
						pMsfConfig->MSFCMDQ_ENABLE[t]);
		LOG_DBG("MSFCMDQ_BASE%d = 0x%x", t,
						pMsfConfig->MSFCMDQ_BASE[t]);
		LOG_DBG("MSFCQLP_CMD_NUM%d = 0x%x", t,
						pMsfConfig->MSFCQLP_CMD_NUM[t]);
		LOG_DBG("MSFDMT_TDRI_BASE%d = 0x%x", t,
					pMsfConfig->MFBDMT_TDRI_BASE[t]);
	}
	LOG_DBG("%s: tpipe_used is %d", __func__, pMsfConfig->tpipe_used);
}

static void msf_norm_sirq(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt;
	bool bResulst = MFALSE;
	pid_t ProcessID;
	unsigned long flag;
	unsigned int p = 0;

	pkt = (struct cmdq_pkt *) data.data;
	if (data.err < 0) {
		mfb_request_dump(&msf_reqs);
		MSF_DumpReg();
		LOG_ERR("%s: call back error(%d)", __func__, data.err);
		goto EXIT;
	}

	spin_lock_irqsave(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]),
									flag);
	if (mfb_update_request(&msf_reqs, &ProcessID) == 0)
		bResulst = MTRUE;
	/* Config the Next frame */
	if (bResulst == MTRUE) {
		#if REQUEST_REGULATION == REQUEST_BASE_REGULATION
		queue_work(MFBInfo.wkqueueMsf, &MFBInfo.ScheduleMsfWork);
		#endif
		p = ProcessID % IRQ_USER_NUM_MAX;
		MFBInfo.IrqInfo
		    .Status[MFB_IRQ_TYPE_INT_MSF_ST] |= MSF_INT_ST;
		MFBInfo.IrqInfo
		    .ProcessFD[p] = ProcessID;
		MFBInfo.IrqInfo.MsfIrqCnt[p]++;
	}
	spin_unlock_irqrestore(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]),
									flag);
	if (bResulst == MTRUE) {
#ifdef MFB_PMQOS
		MFBQOS_Update(0, 2, 0);
#endif
		wake_up_interruptible(&MFBInfo.WaitQueueHeadMsf);
	}

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(MFB_IRQ_TYPE_INT_MSF_ST, m_CurrentPPB, _LOG_INF,
		       "%s, bResulst:%d, MsfIrqCnt[%d]:0x%x\n",
		       __func__, bResulst, p, MFBInfo.IrqInfo.MsfIrqCnt[p]);
	tasklet_schedule(MFB_tasklet[MFB_IRQ_TYPE_INT_MSF_ST].pMFB_tkt);

	#if (REQUEST_REGULATION == FRAME_BASE_REGULATION)
	queue_work(MFBInfo.wkqueueMsf, &MFBInfo.ScheduleMsfWork);
	#endif
EXIT:
	cmdq_pkt_destroy(pkt);

}

signed int CmdqMSFHW(struct frame *frame)
{
	struct MFB_MSFConfig *pMsfConfig;
	struct cmdq_pkt *handle;

	if (frame == NULL || frame->data == NULL)
		return -1;

	LOG_DBG("%s request sent to CMDQ driver", __func__);
	pMsfConfig = (struct MFB_MSFConfig *) frame->data;

	if (MFB_DBG_DBGLOG == (MFB_DBG_DBGLOG & MFBInfo.DebugMaskMsf))
		LOG_DBG("ConfigMSFHW Start!\n");

	/*cmdq_pkt_cl_create(&handle, msf_clt);*/
	handle = cmdq_pkt_create(msf_clt);

	/*if (pMssConfig->eng_secured == 1)*//*YWtodo*/
		/*cmdq_engine_secured(handle, CMDQ_ENG_MSS);*/
	handle->priority = 20;
#ifdef USE_SW_TOKEN
	cmdq_pkt_acquire_event(handle, msf_token_id);
#endif
#ifdef TPIPE_MODE_PREVIEW
	msf_pkt_tcmds(handle, pMsfConfig);
#else
	cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSF_CMDQ_ENABLE_HW,
			pMsfConfig->MSFCMDQ_ENABLE[0], CMDQ_REG_MASK);
	cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSF_CMDQ_BASE_ADDR_HW,
			pMsfConfig->MSFCMDQ_BASE[0], CMDQ_REG_MASK);
	cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSF_CQLP_CMD_NUM_HW,
			pMsfConfig->MSFCQLP_CMD_NUM[0], CMDQ_REG_MASK);
	cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSF_CQLP_ENG_EN_HW,
			pMsfConfig->MSFCQLP_ENG_EN, CMDQ_REG_MASK);

#ifndef BYPASS_REG
	cmdq_pkt_write(handle, NULL, (dma_addr_t)MFB_MSF_START_HW,
			0x1, CMDQ_REG_MASK);
	cmdq_pkt_wfe(handle, msf_done_event_id);

	/* non-blocking API, Please  use cmdqRecFlushAsync() */
#endif
#endif
#ifdef USE_SW_TOKEN
	cmdq_pkt_clear_event(handle, msf_token_id);
#endif
#ifdef MFB_PMQOS
	MFBQOS_Update(1, 2, pMsfConfig->qos);
#endif
	cmdq_pkt_flush_threaded(handle, msf_norm_sirq, (void *)handle);

	return 0;
}

signed int msf_enque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt;
	struct MFB_MSFRequest *_req;
	struct MFB_MSFConfig *pcfg;

	_req = (struct MFB_MSFRequest *) req;

	if (frames == NULL || _req == NULL)
		return -1;

	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		memcpy(frames[f].data, &_req->m_pMsfConfig[f],
			sizeof(struct MFB_MSFConfig));
		pcfg = &_req->m_pMsfConfig[f];
	}

	return 0;
}

signed int msf_deque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt;
	struct MFB_MSFRequest *_req;
	struct MFB_MSFConfig *pcfg;

	_req = (struct MFB_MSFRequest *) req;

	if (frames == NULL || _req == NULL)
		return -1;

	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		memcpy(&_req->m_pMsfConfig[f], frames[f].data,
			sizeof(struct MFB_MSFConfig));
		LOG_DBG("[%s]request dequeued frame(%d/%d).",
			__func__, f, fcnt);
		pcfg = &_req->m_pMsfConfig[f];
	}

	return 0;
}

static void msf_vss_sirq(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt;
	bool bResulst = MFALSE;
	pid_t ProcessID;
	unsigned long flag;
	unsigned int p = 0;

	pkt = (struct cmdq_pkt *) data.data;
	if (data.err < 0) {
		mfb_request_dump(&vmsf_reqs);
		MSF_DumpReg();
		LOG_ERR("%s: call back error(%d)", __func__, data.err);
		goto EXIT;
	}

	spin_lock_irqsave(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]),
									flag);
	if (mfb_update_request(&vmsf_reqs, &ProcessID) == 0)
		bResulst = MTRUE;
	/* Config the Next frame */
	if (bResulst == MTRUE) {
		#if REQUEST_REGULATION == REQUEST_BASE_REGULATION
		queue_work(MFBInfo.wkqueueMsf, &MFBInfo.vmsfwork);
		#endif
		p = ProcessID % IRQ_USER_NUM_MAX;
		MFBInfo.IrqInfo
		    .Status[MFB_IRQ_TYPE_INT_MSF_ST] |= MSF_INT_ST;
		MFBInfo.IrqInfo
		    .ProcessFD[p] = ProcessID;
		MFBInfo.IrqInfo.MsfIrqCnt[p]++;
	}
	spin_unlock_irqrestore(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]),
									flag);
	if (bResulst == MTRUE) {
#ifdef MFB_PMQOS
		MFBQOS_Update(0, 3, 0);
#endif
		wake_up_interruptible(&MFBInfo.WaitQueueHeadMsf);
	}

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(MFB_IRQ_TYPE_INT_MSF_ST, m_CurrentPPB, _LOG_INF,
		       "%s: bResulst:%d, MsfIrqCnt[%d]:0x%x\n",
		       __func__, bResulst, p, MFBInfo.IrqInfo.MsfIrqCnt[p]);
	tasklet_schedule(MFB_tasklet[MFB_IRQ_TYPE_INT_MSF_ST].pMFB_tkt);

	#if (REQUEST_REGULATION == FRAME_BASE_REGULATION)
	queue_work(MFBInfo.wkqueueMsf, &MFBInfo.vmsfwork);
	#endif
EXIT:
	cmdq_pkt_destroy(pkt);
}

signed int vCmdqMSFHW(struct frame *frame)
{
	struct MFB_MSFConfig *pMsfConfig;
	struct cmdq_pkt *handle;

	if (frame == NULL || frame->data == NULL)
		return -1;

	LOG_DBG("%s request sent to CMDQ driver", __func__);
	pMsfConfig = (struct MFB_MSFConfig *) frame->data;

	if (MFB_DBG_DBGLOG == (MFB_DBG_DBGLOG & MFBInfo.DebugMaskMsf))
		LOG_DBG("ConfigMSFHW Start!\n");

	/*cmdq_pkt_cl_create(&handle, msf_clt);*/
	handle = cmdq_pkt_create(msf_clt);
	handle->priority = 0;
#ifdef USE_SW_TOKEN
	cmdq_pkt_acquire_event(handle, msf_token_id);
#endif
	msf_pkt_tcmds(handle, pMsfConfig);
#ifdef USE_SW_TOKEN
	cmdq_pkt_clear_event(handle, msf_token_id);
#endif
#ifdef MFB_PMQOS
	MFBQOS_Update(1, 3, pMsfConfig->qos);
#endif
	cmdq_pkt_flush_threaded(handle, msf_vss_sirq, (void *)handle);

	return 0;
}

static const struct engine_ops msf_ops = {
	.req_enque_cb = msf_enque_cb,
	.req_deque_cb = msf_deque_cb,
	.frame_handler = CmdqMSFHW,
	.req_feedback_cb = NULL,
};

static const struct engine_ops vmsf_ops = {
	.req_enque_cb = msf_enque_cb,
	.req_deque_cb = msf_deque_cb,
	.frame_handler = vCmdqMSFHW,
	.req_feedback_cb = NULL,
};

/*
 *
 */
static signed int MSS_DumpReg(void)
{
	signed int Ret = 0;
	unsigned int i = 0;

	cmdq_util_err("- E.");
	cmdq_util_err("MSS Config Info\n");
	if (g_u4EnableClockCount == 0)
		return Ret;
	for (i = 0; i < MSS_REG_RANGE; i = i + 0x10) {
		cmdq_util_err(
		"[0x%08X %08X][0x%08X %08X][0x%08X %08X][0x%08X %08X]",
		ISP_MSS_BASE_HW + i + 0x0, MFB_RD32(ISP_MSS_BASE + i + 0x0),
		ISP_MSS_BASE_HW + i + 0x4, MFB_RD32(ISP_MSS_BASE + i + 0x4),
		ISP_MSS_BASE_HW + i + 0x8, MFB_RD32(ISP_MSS_BASE + i + 0x8),
		ISP_MSS_BASE_HW + i + 0xC, MFB_RD32(ISP_MSS_BASE + i + 0xC));
	}
#if 0 /*YWtodo sel*/
for (i = 0; i < 3; i++) {
	for (j = 1; j < 7; j++) {
		MFB_WR32(MFB_MSS_DMA_DEBUG_SEL,
			0x000F3F1F & ((i << 16) + (j << 8) + 0));
		cmdq_util_err("debug_mux_data_%d 0x%08X\n",
			i, MFB_RD32(MFB_MSS_TOP_DBG_OUT2));
	}
}
for (i = 1; i < 7; i++) {
	MFB_WR32(MFB_MSS_DMA_DEBUG_SEL, 0x0000001F & i);
	cmdq_util_err("dma_debug_data_%d 0x%08X\n",
			i, MFB_RD32(MFB_MSS_TOP_DBG_OUT2));
}
for (i = 0; i < 16; i++) {
	MFB_WR32(MFB_MSS_DMA_DEBUG_SEL, 0x000F001F & ((i << 16) + 7));
	cmdq_util_err("debug_cnt_data_%d 0x%08X\n",
			i, MFB_RD32(MFB_MSS_TOP_DBG_OUT2));
}
for (i = 0; i < 24; i++) {
	MFB_WR32(MFB_MSS_DMA_DEBUG_SEL, 0x00003F1F & ((i << 8) + 8));
	cmdq_util_err("check_sum_debug_data_%d 0x%08X\n",
			i, MFB_RD32(MFB_MSS_TOP_DBG_OUT2));
}
for (i = 0; i < 15; i++) {
	MFB_WR32(MFB_MSS_TOP_DBG_CTL0, 0x0000FF00 & (0x0 << 8));
	cmdq_util_err("mod_debug_%d 0x%08X\n",
			i, MFB_RD32(MFB_MSS_TOP_DBG_OUT3));
}
#endif
	cmdq_util_err("- X.");
	/*  */
	return Ret;
}

static signed int MSF_DumpReg(void)
{
	signed int Ret = 0;
	unsigned int i = 0;

	cmdq_util_err("- E.");
	cmdq_util_err("MSF Config Info\n");
	if (g_u4EnableClockCount == 0)
		return Ret;
	for (i = 0; i < MSF_REG_RANGE; i = i + 0x10) {
		cmdq_util_err(
		"[0x%08X %08X][0x%08X %08X][0x%08X %08X][0x%08X %08X]",
		ISP_MSF_BASE_HW + i + 0x0, MFB_RD32(ISP_MSF_BASE + i + 0x0),
		ISP_MSF_BASE_HW + i + 0x4, MFB_RD32(ISP_MSF_BASE + i + 0x4),
		ISP_MSF_BASE_HW + i + 0x8, MFB_RD32(ISP_MSF_BASE + i + 0x8),
		ISP_MSF_BASE_HW + i + 0xC, MFB_RD32(ISP_MSF_BASE + i + 0xC));
	}
#if 0 /*YWtodo sel*/
	for (i = 28; i < 32; i++) {
		MFB_WR32(MFB_MSF_TOP_DEBUG_SEL, 0xFF000000 & (i << 24));
		cmdq_util_err("mfb_top_rdy_ack_debug_%d 0x%08X\n",
				i, MFB_RD32(MFB_MSF_TOP_DEBUG_STA));
	}
	for (i = 1; i < 28; i++) {
		MFB_WR32(MFB_MSF_TOP_DEBUG_SEL, 0xFF000000 & (i << 24));
		cmdq_util_err("submodule_debug_sel_%d 0x%08X\n",
				i, MFB_RD32(MFB_MSF_TOP_DEBUG_STA));
	}
	MFB_WR32(MFB_MSF_TOP_DEBUG_SEL, 0xFF000000 & (0x0 << 24));
	MFB_WR32(MFB_MSF_DMA_DEBUG_SEL, 0x0000001F & 18);
	cmdq_util_err("dma_req_st 0x%08X\n", MFB_RD32(MFB_MSF_TOP_DEBUG_STA));
	MFB_WR32(MFB_MSF_DMA_DEBUG_SEL, 0x0000001F & 19);
	cmdq_util_err("dma_rdy_st 0x%08X\n", MFB_RD32(MFB_MSF_TOP_DEBUG_STA));
	for (i = 0; i < 3; i++) {
		for (j = 1; j < 20; j++) {
			MFB_WR32(MFB_MSF_DMA_DEBUG_SEL,
				0x000F3F1F & ((i << 16) + (j << 8) + 0));
			cmdq_util_err("debug_mux_data_%d 0x%08X\n",
				i, MFB_RD32(MFB_MSF_TOP_DEBUG_STA));
		}
	}
	for (i = 0; i < 16; i++) {
		MFB_WR32(MFB_MSF_DMA_DEBUG_SEL, 0x000F001F & ((i << 16) + 16));
		cmdq_util_err("debug_cnt_data_%d 0x%08X\n",
				i, MFB_RD32(MFB_MSF_TOP_DEBUG_STA));
	}
	for (i = 0; i < 60; i++) {
		MFB_WR32(MFB_MSF_DMA_DEBUG_SEL, 0x00003F1F & ((i << 8) + 17));
		cmdq_util_err("check_sum_debug_data_%d 0x%08X\n",
				i, MFB_RD32(MFB_MSF_TOP_DEBUG_STA));
	}
	for (i = 1; i < 16; i++) {
		MFB_WR32(MFB_MSF_DMA_DEBUG_SEL, 0x0000001F & i);
		cmdq_util_err("dma_debug_data_%d 0x%08X\n",
				i, MFB_RD32(MFB_MSF_TOP_DEBUG_STA));
	}
	MFB_WR32(MFB_MSF_DMA_DEBUG_SEL, 0x0000001F & 20);
	cmdq_util_err("wdma_otf_overflow 0x%08X\n",
				MFB_RD32(MFB_MSF_TOP_DEBUG_STA));

	for (i = 0; i < 112; i++) {
		MFB_WR32(MFB_MSF_TOP_DEBUG_SEL,
				0xFFFF0000 & ((3 << 24) + (i << 16)));
		cmdq_util_err("submodule_check_sum_%d 0x%08X\n",
				i, MFB_RD32(MFB_MSF_TOP_CRC_1));
	}
	for (i = 0; i < 29; i++) {
		MFB_WR32(MFB_MSF_TOP_DEBUG_SEL,
				0xFFFF0000 & ((4 << 24) + (i << 16)));
		cmdq_util_err("submodule_CRC_%d 0x%08X\n",
				i, MFB_RD32(MFB_MSF_TOP_CRC_1));
	}
#endif
	cmdq_util_err("- X.");
	/*  */
	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
#ifndef __MFB_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
static inline void MFB_Prepare_Enable_ccf_clock(void)
{
	int ret;
	/* must keep this clk open order:
	 * CG_SCP_SYS_DIS-> CG_MM_SMI_COMMON -> CG_SCP_SYS_ISP -> MFB clk
	 */
#if (MTK_MFB_REG_VERSION == 3)
	smi_bus_prepare_enable(SMI_LARB11, MFB_DEV_NAME);
	ret = clk_prepare_enable(mfb_clk.CG_IMG2_LARB11);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IMG2_LARB11 clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_IMG2_MSS);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IMG2_MSS clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_IMG2_MFB);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IMG2_MFB clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_IMG1_GALS);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IMG1_GALS clock\n");
#elif (MTK_MFB_REG_VERSION == 2)
	smi_bus_prepare_enable(SMI_LARB11, MFB_DEV_NAME);
	ret = clk_prepare_enable(mfb_clk.CG_IMG2_LARB11);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IMG2_LARB11 clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_IMG2_MSS);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IMG2_MSS clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_IMG2_MFB);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IMG2_MFB clock\n");
#else
	smi_bus_prepare_enable(SMI_LARB9, MFB_DEV_NAME);
	ret = clk_prepare_enable(mfb_clk.CG_IMG1_LARB9);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IMG1_LARB9 clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_IMG1_MSS);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IMG1_MSS clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_IMG1_MFB);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IMG1_MFB clock\n");
#endif
}

static inline void MFB_Disable_Unprepare_ccf_clock(void)
{
	/* must keep this clk close order:
	 * MFB clk -> CG_SCP_SYS_ISP -> CG_MM_SMI_COMMON -> CG_SCP_SYS_DIS
	 */
#if (MTK_MFB_REG_VERSION == 3)
	clk_disable_unprepare(mfb_clk.CG_IMG1_GALS);
	clk_disable_unprepare(mfb_clk.CG_IMG2_MFB);
	clk_disable_unprepare(mfb_clk.CG_IMG2_MSS);
	clk_disable_unprepare(mfb_clk.CG_IMG2_LARB11);
	smi_bus_disable_unprepare(SMI_LARB11, MFB_DEV_NAME);
#elif (MTK_MFB_REG_VERSION == 2)
	clk_disable_unprepare(mfb_clk.CG_IMG2_MFB);
	clk_disable_unprepare(mfb_clk.CG_IMG2_MSS);
	clk_disable_unprepare(mfb_clk.CG_IMG2_LARB11);
	smi_bus_disable_unprepare(SMI_LARB11, MFB_DEV_NAME);
#else
	clk_disable_unprepare(mfb_clk.CG_IMG1_MFB);
	clk_disable_unprepare(mfb_clk.CG_IMG1_MSS);
	clk_disable_unprepare(mfb_clk.CG_IMG1_LARB9);
	smi_bus_disable_unprepare(SMI_LARB9, MFB_DEV_NAME);
#endif
}
#endif
#endif

/******************************************************************************
 *
 ******************************************************************************/
#ifdef CONFIG_MTK_IOMMU_V2
static inline int m4u_control_iommu_port(void)
{
	struct M4U_PORT_STRUCT sPort;
	int ret = 0;
	int count_of_ports = 0;
	int i = 0;

#if (MTK_MFB_REG_VERSION >= 2)
	/* LARB11 */
	count_of_ports = M4U_PORT_L11_IMG_MFB_WDMA1 -
		M4U_PORT_L11_IMG_MFB_RDMA0 + 1;
	for (i = 0; i < count_of_ports; i++) {
		sPort.ePortID = M4U_PORT_L11_IMG_MFB_RDMA0+i;
		sPort.Virtuality = MFB_MEM_USE_VIRTUL;
		LOG_INF("config M4U Port ePortID=%d\n", sPort.ePortID);
#if defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U)
		ret = m4u_config_port(&sPort);

		if (ret == 0) {
			LOG_INF("config M4U Port %s to %s SUCCESS\n",
			iommu_get_port_name(M4U_PORT_L11_IMG_MFB_RDMA0+i),
			MFB_MEM_USE_VIRTUL ? "virtual" : "physical");
		} else {
			LOG_INF("config M4U Port %s to %s FAIL(ret=%d)\n",
			iommu_get_port_name(M4U_PORT_L11_IMG_MFB_RDMA0+i),
			MFB_MEM_USE_VIRTUL ? "virtual" : "physical", ret);
			ret = -1;
		}
#endif
	}
#else
	/* LARB9 */
	count_of_ports = M4U_PORT_L9_IMG_MFB_WDMA1_MDP -
		M4U_PORT_L9_IMG_MFB_RDMA0_MDP + 1;
	for (i = 0; i < count_of_ports; i++) {
		sPort.ePortID = M4U_PORT_L9_IMG_MFB_RDMA0_MDP+i;
		sPort.Virtuality = MFB_MEM_USE_VIRTUL;
		LOG_INF("config M4U Port ePortID=%d\n", sPort.ePortID);
#if defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U)
		ret = m4u_config_port(&sPort);

		if (ret == 0) {
			LOG_INF("config M4U Port %s to %s SUCCESS\n",
			iommu_get_port_name(M4U_PORT_L9_IMG_MFB_RDMA0_MDP+i),
			MFB_MEM_USE_VIRTUL ? "virtual" : "physical");
		} else {
			LOG_INF("config M4U Port %s to %s FAIL(ret=%d)\n",
			iommu_get_port_name(M4U_PORT_L9_IMG_MFB_RDMA0_MDP+i),
			MFB_MEM_USE_VIRTUL ? "virtual" : "physical", ret);
			ret = -1;
		}
#endif
	}
#endif
	return ret;
}
#endif

/******************************************************************************
 *
 ******************************************************************************/
static void MFB_EnableClock(bool En)
{
#ifdef CONFIG_MTK_IOMMU_V2
	int ret = 0;
#endif

	if (En) {/* Enable clock. */
		/* LOG_DBG("Mfb clock enbled. g_u4EnableClockCount: %d.", */
		/* g_u4EnableClockCount); */
		switch (g_u4EnableClockCount) {
		case 0:
#ifndef __MFB_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
			MFB_Prepare_Enable_ccf_clock();
#else/*YWtodo*/
			enable_clock(MT_CG_DOWE0_SMI_COMMON, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			/* enable_clock(MT_CG_IMAGE_FD, "CAMERA"); */
			enable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
#endif	/* #if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */

#else
			/*MFB_WR32(IMGSYS_REG_CG_CLR, 0xFFFFFFFF);*//*YWtodo*/
#endif
			break;
		default:
			break;
		}
		spin_lock(&(MFBInfo.SpinLockMFB));
		g_u4EnableClockCount++;
		spin_unlock(&(MFBInfo.SpinLockMFB));

#ifdef CONFIG_MTK_IOMMU_V2
		if (g_u4EnableClockCount == 1) {
			ret = m4u_control_iommu_port();
			if (ret)
				LOG_ERR("cannot config M4U IOMMU PORTS\n");
		}
#endif

	} else {		/* Disable clock. */

		/* LOG_DBG("Mfb clock disabled. g_u4EnableClockCount: %d.", */
		/* g_u4EnableClockCount); */
		spin_lock(&(MFBInfo.SpinLockMFB));
		g_u4EnableClockCount--;
		spin_unlock(&(MFBInfo.SpinLockMFB));
		switch (g_u4EnableClockCount) {
		case 0:
#ifndef __MFB_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
			MFB_Disable_Unprepare_ccf_clock();
#else/*YWtodo*/
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

#else
			/*MFB_WR32(IMGSYS_REG_CG_SET, 0xFFFFFFFF);*//*YWtodo*/
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
static inline void MSS_Reset(void)
{
#if 0/*YWtodo*/
	LOG_DBG("- E.");
	LOG_DBG(" MSS Reset start!\n");
	mutex_lock(&(MutexMFBRef));

	if (MFBInfo.UserCount > 1) {
		mutex_unlock(&(MutexMFBRef));
		LOG_DBG("Curr UserCount(%d) users exist", MFBInfo.UserCount);
	} else {
		mutex_unlock(&(MutexMFBRef));

		/* Reset MSS flow */
		MFB_WR32(MSSTOP_DMA_STOP_REG, 0x100);
		while ((MFB_RD32(MSSTOP_DMA_STOP_STA_REG) & 0x100) != 0x100)
			LOG_DBG("MSS resetting...\n");
		/*Use IMGSYS1*/
		MFB_WR32(IMGSYS_REG_SW_RST, 0x10000);
		udelay(1);
		MFB_WR32(MSSTOP_DMA_STOP_REG, 0x0);
		udelay(1);
		MFB_WR32(IMGSYS_REG_SW_RST, 0x0);
		LOG_DBG(" MSS Reset end!\n");
	}
#endif
}

/*****************************************************************************
 *
 ******************************************************************************/
static inline void MSF_Reset(void)
{
#if 0/*YWtodo*/
	unsigned int temp;

	LOG_DBG("- E.");
	LOG_DBG(" MSF Reset start!\n");
	mutex_lock(&(MutexMFBRef));

	if (MFBInfo.UserCount > 1) {
		mutex_unlock(&(MutexMFBRef));
		LOG_DBG("Curr UserCount(%d) users exist", MFBInfo.UserCount);
	} else {
		mutex_unlock(&(MutexMFBRef));

		/* Reset MSF flow */
		MFB_WR32(MFBTOP_DMA_STOP_REG, 0x80000000);
		while ((MFB_RD32(MFBTOP_DMA_STOP_STA_REG) & 0x80000000)
				!= 0x80000000)
			LOG_DBG("MSF resetting...\n");

		MFB_WR32(MFBTOP_RESET_REG, 0x1);
		udelay(1);
		temp = MFB_RD32(MFB_MSF_CMDQ_ENABLE_REG);
		MFB_WR32(MFB_MSF_CMDQ_ENABLE_REG, temp | 0x100);
		udelay(1);
		MFB_WR32(MFBTOP_DMA_STOP_REG, 0x0);
		udelay(1);
		MFB_WR32(MFBTOP_RESET_REG, 0x0);
		udelay(1);
		MFB_WR32(MFB_MSF_CMDQ_ENABLE_REG, temp & 0xFFFFFEFF);
		LOG_DBG(" MSF Reset end!\n");
	}
#endif
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int MSS_ReadReg(struct MFB_REG_IO_STRUCT *pRegIo)
{
	unsigned int i;
	signed int Ret = 0;
	/*  */
	/* unsigned int* pData = (unsigned int*)pRegIo->Data; */
	struct MFB_REG_STRUCT *pData = NULL;
	struct MFB_REG_STRUCT *pTmpData = NULL;

	if ((pRegIo->pData == NULL) || (pRegIo->Count == 0) ||
		(pRegIo->Count > (MSS_REG_RANGE>>2))) {
		LOG_ERR("pRegIo->pData is NULL, Count:%d!!",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	pData = kmalloc(
		(pRegIo->Count) * sizeof(struct MFB_REG_STRUCT), GFP_KERNEL);
	if (pData == NULL) {
		LOG_ERR("ERROR: kmalloc failed, cnt:%d\n",
			pRegIo->Count);
		Ret = -ENOMEM;
		goto EXIT;
	}
	pTmpData = pData;
	if (copy_from_user(pData, (void *)pRegIo->pData,
		(pRegIo->Count) * sizeof(struct MFB_REG_STRUCT)) == 0) {
		for (i = 0; i < pRegIo->Count; i++) {
			if ((ISP_MSS_BASE + pData->Addr >= ISP_MSS_BASE)
			    && (ISP_MSS_BASE + pData->Addr <
				(ISP_MSS_BASE + MSS_REG_RANGE))
			    && ((pData->Addr & 0x3) == 0)) {
				pData->Val =
					MFB_RD32(ISP_MSS_BASE + pData->Addr);
			} else {
				LOG_ERR("Wrong address(0x%p)",
					(ISP_MSS_BASE + pData->Addr));
				pData->Val = 0;
			}
			pData++;
		}
		pData = pTmpData;
		if (copy_to_user((void *)pRegIo->pData, pData,
			(pRegIo->Count) * sizeof(struct MFB_REG_STRUCT)) != 0) {
			LOG_ERR("copy_to_user failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}
	} else {
		LOG_ERR("MFB_READ_REGISTER copy_from_user failed");
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
static signed int MSF_ReadReg(struct MFB_REG_IO_STRUCT *pRegIo)
{
	unsigned int i;
	signed int Ret = 0;
	/*  */
	/* unsigned int* pData = (unsigned int*)pRegIo->Data; */
	struct MFB_REG_STRUCT *pData = NULL;
	struct MFB_REG_STRUCT *pTmpData = NULL;

	if ((pRegIo->pData == NULL) || (pRegIo->Count == 0) ||
		(pRegIo->Count > (MSF_REG_RANGE>>2))) {
		LOG_ERR("pRegIo->pData is NULL, Count:%d!!",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	pData = kmalloc(
		(pRegIo->Count) * sizeof(struct MFB_REG_STRUCT), GFP_KERNEL);
	if (pData == NULL) {
		LOG_ERR("ERROR: kmalloc failed, cnt:%d\n",
			pRegIo->Count);
		Ret = -ENOMEM;
		goto EXIT;
	}
	pTmpData = pData;
	if (copy_from_user(pData, (void *)pRegIo->pData,
		(pRegIo->Count) * sizeof(struct MFB_REG_STRUCT)) == 0) {
		for (i = 0; i < pRegIo->Count; i++) {
			if ((ISP_MSF_BASE + pData->Addr >= ISP_MSF_BASE)
			    && (ISP_MSF_BASE + pData->Addr <
				(ISP_MSF_BASE + MSF_REG_RANGE))
			    && ((pData->Addr & 0x3) == 0)) {
				pData->Val =
					MFB_RD32(ISP_MSF_BASE + pData->Addr);
			} else {
				LOG_ERR("Wrong address(0x%p)",
					(ISP_MSF_BASE + pData->Addr));
				pData->Val = 0;
			}
			pData++;
		}
		pData = pTmpData;
		if (copy_to_user((void *)pRegIo->pData, pData,
			(pRegIo->Count) * sizeof(struct MFB_REG_STRUCT)) != 0) {
			LOG_ERR("copy_to_user failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}
	} else {
		LOG_ERR("MFB_READ_REGISTER copy_from_user failed");
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
static signed int MSS_WriteRegToHw(
	struct MFB_REG_STRUCT *pReg, unsigned int Count)
{
	signed int Ret = 0;
	unsigned int i;
	bool dbgWriteReg;

	/* Use local variable to store MFBInfo.DebugMask & MFB_DBG_WRITE_REG
	 * for saving lock time
	 */
	spin_lock(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]));
	dbgWriteReg = MFBInfo.DebugMaskMss & MFB_DBG_WRITE_REG;
	spin_unlock(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]));

	/*  */
	if (dbgWriteReg)
		LOG_DBG("- E.\n");

	/*  */
	for (i = 0; i < Count; i++) {
		if (dbgWriteReg) {
			LOG_DBG("Addr(0x%lx), Val(0x%x)\n",
				(unsigned long)(ISP_MSS_BASE + pReg[i].Addr),
				(unsigned int) (pReg[i].Val));
		}

		if (((ISP_MSS_BASE + pReg[i].Addr) <
			(ISP_MSS_BASE + MSS_REG_RANGE))
			&& ((pReg[i].Addr & 0x3) == 0)) {
			MFB_WR32(ISP_MSS_BASE + pReg[i].Addr, pReg[i].Val);
		} else {
			LOG_ERR("wrong address(0x%lx)\n",
				(unsigned long)(ISP_MSS_BASE + pReg[i].Addr));
		}
	}

	/*  */
	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
/* Can write sensor's test model only, if need write to other modules,
 * need modify current code flow
 */
static signed int MSF_WriteRegToHw(
	struct MFB_REG_STRUCT *pReg, unsigned int Count)
{
	signed int Ret = 0;
	unsigned int i;
	bool dbgWriteReg;

	/* Use local variable to store MFBInfo.DebugMask & MFB_DBG_WRITE_REG
	 * for saving lock time
	 */
	spin_lock(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]));
	dbgWriteReg = MFBInfo.DebugMaskMsf & MFB_DBG_WRITE_REG;
	spin_unlock(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]));

	/*  */
	if (dbgWriteReg)
		LOG_DBG("- E.\n");

	/*  */
	for (i = 0; i < Count; i++) {
		if (dbgWriteReg) {
			LOG_DBG("Addr(0x%lx), Val(0x%x)\n",
				(unsigned long)(ISP_MSF_BASE + pReg[i].Addr),
				(unsigned int) (pReg[i].Val));
		}

		if (((ISP_MSF_BASE + pReg[i].Addr) <
			(ISP_MSF_BASE + MSF_REG_RANGE))
			&& ((pReg[i].Addr & 0x3) == 0)) {
			MFB_WR32(ISP_MSF_BASE + pReg[i].Addr, pReg[i].Val);
		} else {
			LOG_ERR("wrong address(0x%lx)\n",
				(unsigned long)(ISP_MSF_BASE + pReg[i].Addr));
		}
	}

	/*  */
	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int MSS_WriteReg(struct MFB_REG_IO_STRUCT *pRegIo)
{
	signed int Ret = 0;
	/* unsigned char* pData = NULL; */
	struct MFB_REG_STRUCT *pData = NULL;
	/*  */
	if (MFBInfo.DebugMaskMss & MFB_DBG_WRITE_REG)
		LOG_DBG("Data(0x%p), Count(%d)\n",
			(pRegIo->pData), (pRegIo->Count));

	if ((pRegIo->pData == NULL) || (pRegIo->Count == 0) ||
			(pRegIo->Count > (MSS_REG_RANGE>>2))) {
		LOG_ERR("ERROR: pRegIo->pData is NULL or Count:%d\n",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	/* pData = (unsigned char*)kmalloc( */
	/*	(pRegIo->Count)*sizeof(MFB_REG_STRUCT), GFP_ATOMIC); */
	pData = kmalloc(
		(pRegIo->Count) * sizeof(struct MFB_REG_STRUCT), GFP_KERNEL);
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
		pRegIo->Count * sizeof(struct MFB_REG_STRUCT)) != 0) {
		LOG_ERR("copy_from_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	Ret = MSS_WriteRegToHw(pData, pRegIo->Count);
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
static signed int MSF_WriteReg(struct MFB_REG_IO_STRUCT *pRegIo)
{
	signed int Ret = 0;
	/* unsigned char* pData = NULL; */
	struct MFB_REG_STRUCT *pData = NULL;
	/*  */
	if (MFBInfo.DebugMaskMsf & MFB_DBG_WRITE_REG)
		LOG_DBG("Data(0x%p), Count(%d)\n",
			(pRegIo->pData), (pRegIo->Count));

	if ((pRegIo->pData == NULL) || (pRegIo->Count == 0) ||
			(pRegIo->Count > (MSF_REG_RANGE>>2))) {
		LOG_ERR("ERROR: pRegIo->pData is NULL or Count:%d\n",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	/* pData = (unsigned char*)kmalloc( */
	/*	(pRegIo->Count)*sizeof(MFB_REG_STRUCT), GFP_ATOMIC); */
	pData = kmalloc(
		(pRegIo->Count) * sizeof(struct MFB_REG_STRUCT), GFP_KERNEL);
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
		pRegIo->Count * sizeof(struct MFB_REG_STRUCT)) != 0) {
		LOG_ERR("copy_from_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	Ret = MSF_WriteRegToHw(pData, pRegIo->Count);
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
static signed int MSS_WaitIrq(struct MFB_WAIT_IRQ_STRUCT *WaitIrq,
					enum MFB_PROCESS_ID_ENUM whichReq)
{

	signed int Ret = 0;
	signed int Timeout = WaitIrq->Timeout;

	/*unsigned int i;*/
	unsigned long flags; /* old: unsigned int flags;*/
			     /* FIX to avoid build warning */
	unsigned int irqStatus;
	/*int cnt = 0;*/
	struct timeval time_getrequest;
	unsigned long long sec = 0;
	unsigned long usec = 0;
	unsigned int p;

	/* do_gettimeofday(&time_getrequest); */
	sec = cpu_clock(0);	/* ns */
	do_div(sec, 1000);	/* usec */
	usec = do_div(sec, 1000000);	/* sec and usec */
	time_getrequest.tv_usec = usec;
	time_getrequest.tv_sec = sec;


	/* Debug interrupt */
	if (MFBInfo.DebugMaskMss & MFB_DBG_INT) {
		if (WaitIrq->Status & MFBInfo.IrqInfo.Mask[WaitIrq->Type]) {
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
	if (WaitIrq->Clear == MFB_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		MFBInfo.IrqInfo.Status[WaitIrq->Type] &= (~WaitIrq->Status);
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		return Ret;
	}

	if (WaitIrq->Clear == MFB_IRQ_CLEAR_WAIT) {
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		if (MFBInfo.IrqInfo.Status[WaitIrq->Type] & WaitIrq->Status)
			MFBInfo.IrqInfo.Status[WaitIrq->Type] &=
				(~WaitIrq->Status);

		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
	} else if (WaitIrq->Clear == MFB_IRQ_CLEAR_ALL) {
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

		MFBInfo.IrqInfo.Status[WaitIrq->Type] = 0;
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
	}
	/* MFB_IRQ_WAIT_CLEAR ==> do nothing */


	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
	spin_lock_irqsave(&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
	irqStatus = MFBInfo.IrqInfo.Status[WaitIrq->Type];
	spin_unlock_irqrestore(&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

	if (WaitIrq->Type != MFB_IRQ_TYPE_INT_MSS_ST) {
		LOG_ERR(
			"No Such Stats can be waited!! irq Type/User/Sts/Pid(0x%x/%d/0x%x/%d)\n",
			WaitIrq->Type,
			WaitIrq->UserKey,
			WaitIrq->Status,
			WaitIrq->ProcessID);
	}


#ifdef MFB_WAITIRQ_LOG
	LOG_INF(
		"before wait_event! Timeout(%d)Clear(%d),Type(%d),IrqSta(0x%08X), WaitSta(0x%08X)\n",
		WaitIrq->Timeout, WaitIrq->Clear,
		WaitIrq->Type, irqStatus, WaitIrq->Status);
	LOG_INF(
		"urKey(%d),whReq(%d),PID(%d)\n",
		WaitIrq->UserKey, whichReq, WaitIrq->ProcessID);
	LOG_INF(
		"MssIrqCnt(0x%08X)\n",
		MFBInfo.IrqInfo.MssIrqCnt);
#endif

	/* 2. start to wait signal */
	Timeout = wait_event_interruptible_timeout(MFBInfo.WaitQueueHeadMss,
				MSS_GetIRQState(WaitIrq->Type,
				&WaitIrq->UserKey,
				WaitIrq->Status, whichReq,
				WaitIrq->ProcessID),
				MFB_MsToJiffies(WaitIrq->Timeout));

	/* check if user is interrupted by system signal */
	if ((Timeout != 0) &&
	    (!WaitIrq->UserKey)) {
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
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = MFBInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

		LOG_ERR(
			"ERRRR Timeout!Timeout(%d)Clear(%d),Type(%d),IrqSta(0x%08X), WaitSta(0x%08X)\n",
			WaitIrq->Timeout, WaitIrq->Clear,
			WaitIrq->Type, irqStatus, WaitIrq->Status);
		LOG_ERR("urKey(%d),whReq(%d),PID(%d)\n",
			WaitIrq->UserKey, whichReq, WaitIrq->ProcessID);
		LOG_ERR(
			"MssIrqCnt(0x%08X)\n",
			MFBInfo.IrqInfo.MssIrqCnt[whichReq]);

		if (WaitIrq->bDumpReg)
			MSS_DumpReg();

		Ret = -EFAULT;
		goto EXIT;
	} else {
		/* Store irqinfo status in here to redeuce time of
		 * spin_lock_irqsave
		 */
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("MFB WaitIrq");
#endif

		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = MFBInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

		if (WaitIrq->Clear == MFB_IRQ_WAIT_CLEAR) {
			spin_lock_irqsave(
				&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

			if (WaitIrq->Status & MSS_INT_ST) {
				p = WaitIrq->ProcessID % IRQ_USER_NUM_MAX;
				MFBInfo.IrqInfo.MssIrqCnt[p]--;
				if (MFBInfo.IrqInfo.MssIrqCnt[p] == 0)
					MFBInfo.IrqInfo.Status[WaitIrq->Type] &=
						(~WaitIrq->Status);
			} else {
				LOG_ERR(
				"MFB_IRQ_WAIT_CLEAR Error, Type(%d), WaitStatus(0x%08X)",
					WaitIrq->Type, WaitIrq->Status);
			}
			spin_unlock_irqrestore(
				&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		}

#ifdef MFB_WAITIRQ_LOG
		LOG_INF(
			"no Timeout!Timeout(%d)Clear(%d),Type(%d),IrqSta(0x%08X), WaitSta(0x%08X)\n",
			WaitIrq->Timeout, WaitIrq->Clear,
			WaitIrq->Type, irqStatus, WaitIrq->Status);
		LOG_INF("urKey(%d),whReq(%d),PID(%d)\n",
			WaitIrq->UserKey, whichReq, WaitIrq->ProcessID);
		LOG_INF(
			"MssIrqCnt(0x%08X)\n",
			MFBInfo.IrqInfo.MssIrqCnt);

#endif

#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif

	}


EXIT:


	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int MSF_WaitIrq(struct MFB_WAIT_IRQ_STRUCT *WaitIrq)
{

	signed int Ret = 0;
	signed int Timeout = WaitIrq->Timeout;
	enum MFB_PROCESS_ID_ENUM whichReq = MFB_PROCESS_ID_MSF;

	/*unsigned int i;*/
	unsigned long flags; /* old: unsigned int flags;*/
			     /* FIX to avoid build warning */
	unsigned int irqStatus;
	/*int cnt = 0;*/
	struct timeval time_getrequest;
	unsigned long long sec = 0;
	unsigned long usec = 0;
	unsigned int p;

	/* do_gettimeofday(&time_getrequest); */
	sec = cpu_clock(0);	/* ns */
	do_div(sec, 1000);	/* usec */
	usec = do_div(sec, 1000000);	/* sec and usec */
	time_getrequest.tv_usec = usec;
	time_getrequest.tv_sec = sec;


	/* Debug interrupt */
	if (MFBInfo.DebugMaskMsf & MFB_DBG_INT) {
		if (WaitIrq->Status & MFBInfo.IrqInfo.Mask[WaitIrq->Type]) {
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
	if (WaitIrq->Clear == MFB_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		MFBInfo.IrqInfo.Status[WaitIrq->Type] &= (~WaitIrq->Status);
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		return Ret;
	}

	if (WaitIrq->Clear == MFB_IRQ_CLEAR_WAIT) {
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		if (MFBInfo.IrqInfo.Status[WaitIrq->Type] & WaitIrq->Status)
			MFBInfo.IrqInfo.Status[WaitIrq->Type] &=
				(~WaitIrq->Status);

		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
	} else if (WaitIrq->Clear == MFB_IRQ_CLEAR_ALL) {
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

		MFBInfo.IrqInfo.Status[WaitIrq->Type] = 0;
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
	}
	/* MFB_IRQ_WAIT_CLEAR ==> do nothing */


	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
	spin_lock_irqsave(&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
	irqStatus = MFBInfo.IrqInfo.Status[WaitIrq->Type];
	spin_unlock_irqrestore(&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

	if (WaitIrq->Type != MFB_IRQ_TYPE_INT_MSF_ST) {
		LOG_ERR(
			"No Such Stats can be waited!! irq Type/User/Sts/Pid(0x%x/%d/0x%x/%d)\n",
			WaitIrq->Type,
			WaitIrq->UserKey,
			WaitIrq->Status,
			WaitIrq->ProcessID);
	}


#ifdef MFB_WAITIRQ_LOG
	LOG_INF(
		"before wait_event! Timeout(%d)Clear(%d),Type(%d),IrqSta(0x%08X), WaitSta(0x%08X)\n",
		WaitIrq->Timeout, WaitIrq->Clear,
		WaitIrq->Type, irqStatus, WaitIrq->Status);
	LOG_INF(
		"urKey(%d),whReq(%d),PID(%d)\n",
		WaitIrq->UserKey, whichReq, WaitIrq->ProcessID);
	LOG_INF(
		"MsfIrqCnt(0x%08X)\n",
		MFBInfo.IrqInfo.MsfIrqCnt);
#endif

	/* 2. start to wait signal */
	Timeout = wait_event_interruptible_timeout(MFBInfo.WaitQueueHeadMsf,
				MSF_GetIRQState(WaitIrq->Type, WaitIrq->UserKey,
				WaitIrq->Status, whichReq,
				WaitIrq->ProcessID),
				MFB_MsToJiffies(WaitIrq->Timeout));

	p = WaitIrq->ProcessID % IRQ_USER_NUM_MAX;
	/* check if user is interrupted by system signal */
	if ((Timeout != 0) &&
	    (!MSF_GetIRQState(
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
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = MFBInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

		LOG_ERR(
			"ERRRR Timeout!Timeout(%d)Clear(%d),Type(%d),IrqSta(0x%08X), WaitSta(0x%08X)\n",
			WaitIrq->Timeout, WaitIrq->Clear,
			WaitIrq->Type, irqStatus, WaitIrq->Status);
		LOG_ERR("urKey(%d),whReq(%d),PID(%d)\n",
			WaitIrq->UserKey, whichReq, WaitIrq->ProcessID);
		LOG_ERR(
			"MsfIrqCnt(0x%08X)\n",
			MFBInfo.IrqInfo.MsfIrqCnt[p]);

		if (WaitIrq->bDumpReg)
			MSF_DumpReg();

		Ret = -EFAULT;
		goto EXIT;
	} else {
		/* Store irqinfo status in here to redeuce time of
		 * spin_lock_irqsave
		 */
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("MFB WaitIrq");
#endif

		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = MFBInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

		if (WaitIrq->Clear == MFB_IRQ_WAIT_CLEAR) {
			spin_lock_irqsave(
				&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

			if (WaitIrq->Status & MSF_INT_ST) {
				MFBInfo.IrqInfo.MsfIrqCnt[p]--;
				if (MFBInfo.IrqInfo.MsfIrqCnt[p] == 0)
					MFBInfo.IrqInfo.Status[WaitIrq->Type] &=
						(~WaitIrq->Status);
			} else {
				LOG_ERR(
				"MFB_IRQ_WAIT_CLEAR Error, Type(%d), WaitStatus(0x%08X)",
					WaitIrq->Type, WaitIrq->Status);
			}
			spin_unlock_irqrestore(
				&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		}

#ifdef MFB_WAITIRQ_LOG
		LOG_INF(
			"no Timeout!Timeout(%d)Clear(%d),Type(%d),IrqSta(0x%08X), WaitSta(0x%08X)\n",
			WaitIrq->Timeout, WaitIrq->Clear,
			WaitIrq->Type, irqStatus, WaitIrq->Status);
		LOG_INF("urKey(%d),whReq(%d),PID(%d)\n",
			WaitIrq->UserKey, whichReq, WaitIrq->ProcessID);
		LOG_INF(
			"MsfIrqCnt(0x%08X)\n",
			MFBInfo.IrqInfo.MsfIrqCnt);

#endif

#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif

	}


EXIT:


	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static enum MFB_PROCESS_ID_ENUM mss_get_reqs(enum exec_mode exec,
					struct engine_requests **reqs)
{
	enum MFB_PROCESS_ID_ENUM tag;

	switch (exec) {
	case EXEC_MODE_NORM:
		*reqs = &mss_reqs;
		tag = MFB_PROCESS_ID_MSS;
		break;
	case EXEC_MODE_VSS:
		*reqs = &vmss_reqs;
		tag = MFB_PROCESS_ID_vMSS;
		break;
	default:
		tag = MFB_PROCESS_ID_NONE;
		LOG_ERR("invalid tile irq mode\n");
		break;
	}

	return tag;
}

static unsigned int msf_get_reqs(enum exec_mode exec,
					struct engine_requests **reqs)
{
	unsigned int ret = 0;

	switch (exec) {
	case EXEC_MODE_NORM:
		*reqs = &msf_reqs;
		break;
	case EXEC_MODE_VSS:
		*reqs = &vmsf_reqs;
		break;
	default:
		ret = 1;
		LOG_ERR("invalid tile irq mode\n");
		break;
	}

	return ret;
}


static long MFB_ioctl(struct file *pFile, unsigned int Cmd, unsigned long Param)
{
	signed int Ret = 0;

	/*unsigned int pid = 0;*/
	struct MFB_REG_IO_STRUCT RegIo;
	struct MFB_WAIT_IRQ_STRUCT IrqInfo;
	struct MFB_CLEAR_IRQ_STRUCT ClearIrq;
	struct MFB_MSSRequest mfb_MssReq;
	struct MFB_MSFRequest mfb_MsfReq;
	struct MFB_USER_INFO_STRUCT *pUserInfo;
	struct engine_requests *reqs = NULL;
	struct MFB_MSSConfig *msscfgs = NULL;
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
	pUserInfo = (struct MFB_USER_INFO_STRUCT *) (pFile->private_data);
	/*  */
	switch (Cmd) {
	case MFB_MSS_RESET:
		{
			spin_lock(
			&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]));
			MSS_Reset();
			spin_unlock(
			&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]));
			break;
		}
	case MFB_MSF_RESET:
		{
			spin_lock(
			&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]));
			MSF_Reset();
			spin_unlock(
			&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]));
			break;
		}
		/*  */
	case MFB_MSS_DUMP_REG:
		{
			Ret = MSS_DumpReg();
			break;
		}
	case MFB_MSF_DUMP_REG:
		{
			Ret = MSF_DumpReg();
			break;
		}
		/*  */
	case MFB_MSS_DUMP_ISR_LOG:
		{
			unsigned int currentPPB = m_CurrentPPB;

			spin_lock_irqsave(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]),
				flags);
			m_CurrentPPB = (m_CurrentPPB + 1) % LOG_PPNUM;
			spin_unlock_irqrestore(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]),
				flags);

			IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MSS_ST,
				currentPPB, _LOG_INF);
			IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MSS_ST,
				currentPPB, _LOG_ERR);
			break;
		}
	case MFB_MSF_DUMP_ISR_LOG:
		{
			unsigned int currentPPB = m_CurrentPPB;

			spin_lock_irqsave(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]),
				flags);
			m_CurrentPPB = (m_CurrentPPB + 1) % LOG_PPNUM;
			spin_unlock_irqrestore(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]),
				flags);

			IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MSF_ST,
				currentPPB, _LOG_INF);
			IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MSF_ST,
				currentPPB, _LOG_ERR);
			break;
		}
	case MFB_MSS_READ_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param,
				sizeof(struct MFB_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir of copy from user is
				 * implemented in MFB_ReadReg(...)
				 */
				Ret = MSS_ReadReg(&RegIo);
			} else {
				LOG_ERR(
					"MFB_MSS_READ_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case MFB_MSF_READ_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param,
				sizeof(struct MFB_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir of copy from user is
				 * implemented in MFB_ReadReg(...)
				 */
				Ret = MSF_ReadReg(&RegIo);
			} else {
				LOG_ERR(
					"MFB_MSF_READ_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case MFB_MSS_WRITE_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param,
				sizeof(struct MFB_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir of copy from user is
				 * implemented in MFB_WriteReg(...)
				 */
				Ret = MSS_WriteReg(&RegIo);
			} else {
				LOG_ERR(
					"MFB_MSS_WRITE_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case MFB_MSF_WRITE_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param,
				sizeof(struct MFB_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir of copy from user is
				 * implemented in MFB_WriteReg(...)
				 */
				Ret = MSF_WriteReg(&RegIo);
			} else {
				LOG_ERR(
					"MFB_MSF_WRITE_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case MFB_MSS_WAIT_IRQ:
		{
			if (copy_from_user(&IrqInfo, (void *)Param,
				sizeof(struct MFB_WAIT_IRQ_STRUCT)) == 0) {
				/*  */
				if ((IrqInfo.Type >= MFB_IRQ_TYPE_AMOUNT) ||
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

				LOG_DBG(
					"MSS IRQ clear(%d), type(%d), userKey(%d), timeout(%d), status(%d), streamtag(%d)\n",
					IrqInfo.Clear, IrqInfo.Type,
					IrqInfo.UserKey, IrqInfo.Timeout,
					IrqInfo.Status, pUserInfo->streamtag);

				IrqInfo.ProcessID = pUserInfo->Pid;
				Ret = MSS_WaitIrq(&IrqInfo,
							pUserInfo->streamtag);
				if (Ret < 0) {
					mfb_request_dump(&mss_reqs);
					mfb_request_dump(&vmss_reqs);
					MSS_DumpReg();
				}

				if (copy_to_user((void *)Param, &IrqInfo,
				    sizeof(struct MFB_WAIT_IRQ_STRUCT)) != 0) {
					LOG_ERR("copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR("MFB_WAIT_IRQ copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case MFB_MSF_WAIT_IRQ:
		{
			if (copy_from_user(&IrqInfo, (void *)Param,
				sizeof(struct MFB_WAIT_IRQ_STRUCT)) == 0) {
				/*  */
				if ((IrqInfo.Type >= MFB_IRQ_TYPE_AMOUNT) ||
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
					"MSF IRQ clear(%d), type(%d), userKey(%d), timeout(%d), status(%d)\n",
					IrqInfo.Clear, IrqInfo.Type,
					IrqInfo.UserKey, IrqInfo.Timeout,
					IrqInfo.Status);

				IrqInfo.ProcessID = pUserInfo->Pid;
				Ret = MSF_WaitIrq(&IrqInfo);
				if (Ret < 0) {
					mfb_request_dump(&vmsf_reqs);
					mfb_request_dump(&msf_reqs);
					MSF_DumpReg();
				}

				if (copy_to_user((void *)Param, &IrqInfo,
				    sizeof(struct MFB_WAIT_IRQ_STRUCT)) != 0) {
					LOG_ERR("copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR("MFB_WAIT_IRQ copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case MFB_MSS_CLEAR_IRQ:
		{
			if (copy_from_user(&ClearIrq, (void *)Param,
				sizeof(struct MFB_CLEAR_IRQ_STRUCT)) == 0) {
				LOG_DBG("MFB_MSS_CLEAR_IRQ Type(%d)",
					ClearIrq.Type);

				if ((ClearIrq.Type >= MFB_IRQ_TYPE_AMOUNT) ||
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
					"MFB_MSS_CLEAR_IRQ:Type(%d),Status(0x%08X),IrqStatus(0x%08X)\n",
					ClearIrq.Type, ClearIrq.Status,
					MFBInfo.IrqInfo.Status[ClearIrq.Type]);
				spin_lock_irqsave(
					&(MFBInfo.SpinLockIrq[ClearIrq.Type]),
					flags);
				MFBInfo.IrqInfo.Status[ClearIrq.Type] &=
					(~ClearIrq.Status);
				spin_unlock_irqrestore(
					&(MFBInfo.SpinLockIrq[ClearIrq.Type]),
					flags);
			} else {
				LOG_ERR(
					"MFB_MSS_CLEAR_IRQ copy_from_user failed\n");
				Ret = -EFAULT;
			}
			break;
		}
	case MFB_MSF_CLEAR_IRQ:
		{
			if (copy_from_user(&ClearIrq, (void *)Param,
				sizeof(struct MFB_CLEAR_IRQ_STRUCT)) == 0) {
				LOG_DBG("MFB_MSF_CLEAR_IRQ Type(%d)",
					ClearIrq.Type);

				if ((ClearIrq.Type >= MFB_IRQ_TYPE_AMOUNT) ||
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
					"MFB_MSF_CLEAR_IRQ:Type(%d),Status(0x%08X),IrqStatus(0x%08X)\n",
					ClearIrq.Type, ClearIrq.Status,
					MFBInfo.IrqInfo.Status[ClearIrq.Type]);
				spin_lock_irqsave(
					&(MFBInfo.SpinLockIrq[ClearIrq.Type]),
					flags);
				MFBInfo.IrqInfo.Status[ClearIrq.Type] &=
					(~ClearIrq.Status);
				spin_unlock_irqrestore(
					&(MFBInfo.SpinLockIrq[ClearIrq.Type]),
					flags);
			} else {
				LOG_ERR(
					"MFB_MSF_CLEAR_IRQ copy_from_user failed\n");
				Ret = -EFAULT;
			}
			break;
		}
	case MFB_MSS_ENQUE_REQ:
		{
		if (copy_from_user(&mfb_MssReq, (void *)Param,
			sizeof(struct MFB_MSSRequest)) == 0) {
			LOG_DBG("MSS_ENQNUE_NUM:%d, pid:%d\n",
				mfb_MssReq.m_ReqNum,
				pUserInfo->Pid);
			if (mfb_MssReq.m_ReqNum >
				_SUPPORT_MAX_MFB_FRAME_REQUEST_) {
				LOG_ERR(
					"MSS Enque Num is bigger than enqueNum:%d\n",
					mfb_MssReq.m_ReqNum);
				Ret = -EFAULT;
				goto EXIT;
			}

			if (mfb_MssReq.m_pMssConfig == NULL) {
				LOG_ERR("NULL MSS user Config\n");
				Ret = -EFAULT;
				goto EXIT;
			}

			mutex_lock(&gMfbMssMutex);/* Protect the Multi Process*/
			switch (mfb_MssReq.exec) {
			case EXEC_MODE_NORM:
				msscfgs = g_MssEnqueReq_Struct.MssFrameConfig;
				break;
			case EXEC_MODE_VSS:
				msscfgs = g_MssEnqueReq_Struct.vMssFrameConfig;
				break;
			default:
				msscfgs = g_MssEnqueReq_Struct.MssFrameConfig;
				LOG_WRN("invalid irq mode\n");
				break;
			}

			if (copy_from_user(msscfgs,
				(void *)mfb_MssReq.m_pMssConfig,
				mfb_MssReq.m_ReqNum * sizeof(
					struct MFB_MSSConfig)
				) != 0) {
				LOG_ERR(
					"copy MSSConfig from request is fail!!\n");
				Ret = -EFAULT;
				goto EXIT;
			}

			pUserInfo->streamtag = mss_get_reqs(mfb_MssReq.exec,
									&reqs);
			pUserInfo->reqs = reqs;

			spin_lock_irqsave(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]),
				flags);
			kMssReq.m_ReqNum = mfb_MssReq.m_ReqNum;
			kMssReq.m_pMssConfig = msscfgs;
			mfb_enque_request(reqs, kMssReq.m_ReqNum, &kMssReq,
								pUserInfo->Pid);
			spin_unlock_irqrestore(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]),
				flags);
			LOG_DBG("ConfigMSS Request!!\n");
			if (!mfb_request_running(reqs)) {
				LOG_DBG("direct mfb_request_handler\n");
				mfb_request_handler(reqs,
						&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MSS_ST]));
			}
			mutex_unlock(&gMfbMssMutex);
		} else {
			LOG_ERR("MFB_MSS_ENQUE copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
		}
	case MFB_MSS_DEQUE_REQ:
		{
			if (copy_from_user(&mfb_MssReq, (void *)Param,
					sizeof(struct MFB_MSSRequest)) == 0) {
				reqs = pUserInfo->reqs;
				mutex_lock(&gMfbMssDequeMutex);
				/* Protect the Multi Process */

				spin_lock_irqsave(&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MSS_ST]),
						  flags);
				kMssReq.m_pMssConfig =
					g_MssDequeReq_Struct.MssFrameConfig;
				mfb_deque_request(reqs, &kMssReq.m_ReqNum,
					&kMssReq);
				dequeNum = kMssReq.m_ReqNum;
				mfb_MssReq.m_ReqNum = dequeNum;
				spin_unlock_irqrestore(
					&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MSS_ST]),
					flags);

				mutex_unlock(&gMfbMssDequeMutex);

				if (mfb_MssReq.m_pMssConfig == NULL) {
					LOG_ERR("NULL MSS user Config\n");
					Ret = -EFAULT;
					goto EXIT;
				}

				if (copy_to_user(
					(void *)mfb_MssReq.m_pMssConfig,
					&g_MssDequeReq_Struct.MssFrameConfig[0],
					dequeNum * sizeof(
						struct MFB_MSSConfig)) != 0) {
					LOG_ERR(
						"MFB_CMD_MSS_DEQUE_REQ copy_to_user frameconfig failed\n");
					Ret = -EFAULT;
				}
				if (copy_to_user((void *)Param, &mfb_MssReq,
					sizeof(struct MFB_MSSRequest)) != 0) {
					LOG_ERR(
						"MFB_CMD_MSS_DEQUE_REQ copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR(
					"MFB_CMD_MSS_DEQUE_REQ copy_from_user failed\n");
				Ret = -EFAULT;
			}
			break;
		}
	case MFB_MSF_ENQUE_REQ:
		{
		if (copy_from_user(&mfb_MsfReq, (void *)Param,
			sizeof(struct MFB_MSFRequest)) == 0) {
			LOG_DBG("MSF_ENQNUE_NUM:%d, pid:%d\n",
				mfb_MsfReq.m_ReqNum,
				pUserInfo->Pid);
			if (mfb_MsfReq.m_ReqNum >
				_SUPPORT_MAX_MFB_FRAME_REQUEST_) {
				LOG_ERR(
					"MSF Enque Num is bigger than enqueNum:%d\n",
					mfb_MsfReq.m_ReqNum);
				Ret = -EFAULT;
				goto EXIT;
			}

			if (mfb_MsfReq.m_pMsfConfig == NULL) {
				LOG_ERR("NULL MSF user Config\n");
				Ret = -EFAULT;
				goto EXIT;
			}

			if (copy_from_user(
				g_MsfEnqueReq_Struct.MsfFrameConfig,
				(void *)mfb_MsfReq.m_pMsfConfig,
				mfb_MsfReq.m_ReqNum *
					sizeof(struct MFB_MSFConfig)) != 0) {
				LOG_ERR(
					"copy MSFConfig from request is fail!!\n");
				Ret = -EFAULT;
				goto EXIT;
			}
			msf_get_reqs(mfb_MsfReq.exec, &reqs);
			pUserInfo->reqs = reqs;

			/* Protect the Multi Process */
			mutex_lock(&gMfbMsfMutex);

			spin_lock_irqsave(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]),
				flags);
			kMsfReq.m_ReqNum = mfb_MsfReq.m_ReqNum;
			kMsfReq.m_pMsfConfig =
				g_MsfEnqueReq_Struct.MsfFrameConfig;
			mfb_enque_request(reqs,
				kMsfReq.m_ReqNum,
				&kMsfReq, pUserInfo->Pid);
			spin_unlock_irqrestore(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]),
				flags);

			LOG_DBG("ConfigMSF Request!!\n");
			if (!mfb_request_running(reqs)) {
				LOG_DBG("direct mfb_request_handler\n");
				mfb_request_handler(
					reqs,
					&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MSF_ST])
					);
			}
			mutex_unlock(&gMfbMsfMutex);
		} else {
			LOG_ERR("MFB_MSF_ENQUE copy_from_user failed\n");
			Ret = -EFAULT;
		}

			break;
		}
	case MFB_MSF_DEQUE_REQ:
		{
			if (copy_from_user(&mfb_MsfReq, (void *)Param,
				sizeof(struct MFB_MSFRequest)) == 0) {
				reqs = pUserInfo->reqs;
				mutex_lock(&gMfbMsfDequeMutex);
				/* Protect the Multi Process */
				spin_lock_irqsave(
					&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MSF_ST]),
					flags);
				kMsfReq.m_pMsfConfig =
					g_MsfDequeReq_Struct.MsfFrameConfig;
				mfb_deque_request(
					reqs,
					&kMsfReq.m_ReqNum,
					&kMsfReq);
				dequeNum = kMsfReq.m_ReqNum;
				mfb_MsfReq.m_ReqNum = dequeNum;

				spin_unlock_irqrestore(
					&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MSF_ST]),
					flags);

				mutex_unlock(&gMfbMsfDequeMutex);

				if (mfb_MsfReq.m_pMsfConfig == NULL) {
					LOG_ERR("NULL MSF user Config\n");
					Ret = -EFAULT;
					goto EXIT;
				}

				if (copy_to_user(
					(void *)mfb_MsfReq.m_pMsfConfig,
				     &g_MsfDequeReq_Struct.MsfFrameConfig[0],
				     dequeNum *
					sizeof(struct MFB_MSFConfig)) != 0) {
					LOG_ERR(
						"MFB_MSF_DEQUE_REQ copy_to_user frameconfig failed\n");
					Ret = -EFAULT;
				}
				if (copy_to_user(
					(void *)Param, &mfb_MsfReq,
					sizeof(struct MFB_MSFRequest)) != 0) {
					LOG_ERR(
						"MFB_MSF_DEQUE_REQ copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				LOG_ERR(
					"MFB_CMD_MSF_DEQUE_REQ copy_from_user failed\n");
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
static int compat_get_MFB_read_register_data(
	struct compat_MFB_REG_IO_STRUCT __user *data32,
	struct MFB_REG_IO_STRUCT __user *data)
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

static int compat_put_MFB_read_register_data(
	struct compat_MFB_REG_IO_STRUCT __user *data32,
	struct MFB_REG_IO_STRUCT __user *data)
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

static int compat_get_MFB_mss_enque_req_data(
	struct compat_MFB_MSSRequest __user *data32,
	struct MFB_MSSRequest __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pMssConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pMssConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_MFB_mss_enque_req_data(
	struct compat_MFB_MSSRequest __user *data32,
	struct MFB_MSSRequest __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pMssConfig); */
	/* err |= put_user(uptr, &data32->m_pMssConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}


static int compat_get_MFB_mss_deque_req_data(
	struct compat_MFB_MSSRequest __user *data32,
	struct MFB_MSSRequest __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pMssConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pMssConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_MFB_mss_deque_req_data(
	struct compat_MFB_MSSRequest __user *data32,
	struct MFB_MSSRequest __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pMssConfig); */
	/* err |= put_user(uptr, &data32->m_pMssConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}

static int compat_get_MFB_msf_enque_req_data(
	struct compat_MFB_MSFRequest __user *data32,
	struct MFB_MSFRequest __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pMsfConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pMsfConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_MFB_msf_enque_req_data(
	struct compat_MFB_MSFRequest __user *data32,
	struct MFB_MSFRequest __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pMsfConfig); */
	/* err |= put_user(uptr, &data32->m_pMsfConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}


static int compat_get_MFB_msf_deque_req_data(
	struct compat_MFB_MSFRequest __user *data32,
	struct MFB_MSFRequest __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pMsfConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pMsfConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_MFB_msf_deque_req_data(
	struct compat_MFB_MSFRequest __user *data32,
	struct MFB_MSFRequest __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pMsfConfig); */
	/* err |= put_user(uptr, &data32->m_pMsfConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}

static long MFB_ioctl_compat(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	long ret;


	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		LOG_ERR("no f_op !!!\n");
		return -ENOTTY;
	}
	switch (cmd) {
	case COMPAT_MFB_MSS_READ_REGISTER:
		{
			struct compat_MFB_REG_IO_STRUCT __user *data32;
			struct MFB_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_read_register_data(data32, data);
			if (err) {
				LOG_INF(
					"compat_get_MFB_MSS_read_register_data error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp,
				MFB_MSS_READ_REGISTER, (unsigned long)data);
			err = compat_put_MFB_read_register_data(data32, data);
			if (err) {
				LOG_INF(
					"compat_put_MFB_MSS_read_register_data error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_MFB_MSF_READ_REGISTER:
		{
			struct compat_MFB_REG_IO_STRUCT __user *data32;
			struct MFB_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_read_register_data(data32, data);
			if (err) {
				LOG_INF(
					"compat_get_MFB_read_register_data error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp,
				MFB_MSF_READ_REGISTER, (unsigned long)data);
			err = compat_put_MFB_read_register_data(data32, data);
			if (err) {
				LOG_INF(
					"compat_put_MFB_read_register_data error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_MFB_MSS_WRITE_REGISTER:
		{
			struct compat_MFB_REG_IO_STRUCT __user *data32;
			struct MFB_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_read_register_data(data32, data);
			if (err) {
				LOG_INF(
				"COMPAT_MFB_MSS_WRITE_REGISTER error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp,
				MFB_MSS_WRITE_REGISTER, (unsigned long)data);
			return ret;
		}
	case COMPAT_MFB_MSF_WRITE_REGISTER:
		{
			struct compat_MFB_REG_IO_STRUCT __user *data32;
			struct MFB_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_read_register_data(data32, data);
			if (err) {
				LOG_INF(
				"COMPAT_MFB_MSF_WRITE_REGISTER error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp,
				MFB_MSF_WRITE_REGISTER, (unsigned long)data);
			return ret;
		}
	case COMPAT_MFB_MSS_ENQUE_REQ:
		{
			struct compat_MFB_MSSRequest __user *data32;
			struct MFB_MSSRequest __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_mss_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_MFB_MSS_ENQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, MFB_MSS_ENQUE_REQ,
						       (unsigned long)data);
			err = compat_put_MFB_mss_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_MFB_MSS_ENQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_MFB_MSS_DEQUE_REQ:
		{
			struct compat_MFB_MSSRequest __user *data32;
			struct MFB_MSSRequest __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_mss_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_MFB_MSS_DEQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, MFB_MSS_DEQUE_REQ,
						       (unsigned long)data);
			err = compat_put_MFB_mss_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_MFB_MSS_DEQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}

	case COMPAT_MFB_MSF_ENQUE_REQ:
		{
			struct compat_MFB_MSFRequest __user *data32;
			struct MFB_MSFRequest __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_msf_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_MFB_MSF_ENQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, MFB_MSF_ENQUE_REQ,
						       (unsigned long)data);
			err = compat_put_MFB_msf_enque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_MFB_MSF_ENQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_MFB_MSF_DEQUE_REQ:
		{
			struct compat_MFB_MSFRequest __user *data32;
			struct MFB_MSFRequest __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_msf_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_MFB_MSF_DEQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, MFB_MSF_DEQUE_REQ,
						       (unsigned long)data);
			err = compat_put_MFB_msf_deque_req_data(data32, data);
			if (err) {
				LOG_INF("COMPAT_MFB_MSF_DEQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}

	case MFB_MSS_WAIT_IRQ:
	case MFB_MSF_WAIT_IRQ:
	case MFB_MSS_CLEAR_IRQ:	/* structure (no pointer) */
	case MFB_MSF_CLEAR_IRQ:	/* structure (no pointer) */
	case MFB_MSS_RESET:
	case MFB_MSF_RESET:
	case MFB_MSS_DUMP_REG:
	case MFB_MSF_DUMP_REG:
	case MFB_MSS_DUMP_ISR_LOG:
	case MFB_MSF_DUMP_ISR_LOG:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
		/* return MFB_ioctl(filep, cmd, arg); */
	}
}

#endif

/******************************************************************************
 *
 ******************************************************************************/
static signed int MFB_open(struct inode *pInode, struct file *pFile)
{
	signed int Ret = 0, i = 0;
	/*int q = 0, p = 0;*/
	struct MFB_USER_INFO_STRUCT *pUserInfo = NULL;
	unsigned long flags;

	LOG_DBG("- E. UserCount: %d.", MFBInfo.UserCount);


	/*  */
	mutex_lock(&(MutexMFBRef));

	pFile->private_data = NULL;
	pFile->private_data =
		kmalloc(sizeof(struct MFB_USER_INFO_STRUCT), GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		LOG_DBG(
			"ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)",
			current->comm,
			current->pid,
			current->tgid);
		Ret = -ENOMEM;
		mutex_unlock(&(MutexMFBRef));
		goto EXIT;
	} else {
		pUserInfo = (struct MFB_USER_INFO_STRUCT *) pFile->private_data;
		pUserInfo->Tid = current->tgid;
		pUserInfo->streamtag = MFB_PROCESS_ID_NONE;
	}
	/*  */
	if (MFBInfo.UserCount > 0) {
		MFBInfo.UserCount++;
		for (i = 0;  i < IRQ_USER_NUM_MAX; i++) {
			if (MFBInfo.IrqInfo.MssIrqUse[i] == -1)
				break;
		}
		if (i == IRQ_USER_NUM_MAX) {
			mutex_unlock(&(MutexMFBRef));
			LOG_DBG("ERROR: MssIrqUse is full (%d)", i);
			goto EXIT;
		}
		MFBInfo.IrqInfo.MssIrqUse[i] = 1;
		pUserInfo->Pid = i;
		mutex_unlock(&(MutexMFBRef));
		LOG_DBG(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			MFBInfo.UserCount, current->comm,
			current->pid, current->tgid);
		goto EXIT;
	} else {
		MFBInfo.UserCount++;

		for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
			MFBInfo.IrqInfo.MssIrqCnt[i] = 0;
			MFBInfo.IrqInfo.MsfIrqCnt[i] = 0;
			MFBInfo.IrqInfo.MssIrqUse[i] = -1;
		}
		MFBInfo.IrqInfo.MssIrqUse[0] = 1;
		pUserInfo->Pid = 0;
		/*  */
		mfb_register_requests(&mss_reqs, sizeof(struct MFB_MSSConfig));
		mfb_set_engine_ops(&mss_reqs, &mss_ops);
		mfb_register_requests(&vmss_reqs, sizeof(struct MFB_MSSConfig));
		mfb_set_engine_ops(&vmss_reqs, &vmss_ops);

		mfb_register_requests(&msf_reqs, sizeof(struct MFB_MSFConfig));
		mfb_set_engine_ops(&msf_reqs, &msf_ops);
		mfb_register_requests(&vmsf_reqs, sizeof(struct MFB_MSFConfig));
		mfb_set_engine_ops(&vmsf_reqs, &vmsf_ops);

#ifdef MFB_PMQOS
		qos_total = 0;
		for (i = 0; i < 4; i++)
			qos_scen[i] = 0;
#endif
		mutex_unlock(&(MutexMFBRef));
		LOG_INF(
			"%s + 1st UserCount(%d), (process, pid, tgid)=(%s, %d, %d)",
			__func__, MFBInfo.UserCount, current->comm,
			current->pid, current->tgid);
	}

	/* Enable clock */
	MFB_EnableClock(MTRUE);
	g_SuspendCnt = 0;
	LOG_INF("MFB open g_u4EnableClockCount: %d", g_u4EnableClockCount);
	/*  */

	spin_lock_irqsave(
		&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]), flags);
		MFBInfo.IrqInfo.Status[MFB_IRQ_TYPE_INT_MSS_ST] = 0;
	spin_unlock_irqrestore(
		&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]), flags);

	spin_lock_irqsave(
		&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]), flags);
		MFBInfo.IrqInfo.Status[MFB_IRQ_TYPE_INT_MSF_ST] = 0;
	spin_unlock_irqrestore(
		&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]), flags);

#ifdef KERNEL_LOG
    /* In EP, Add MFB_DBG_WRITE_REG for debug. Should remove it after EP */
	MFBInfo.DebugMaskMss =
		(MFB_DBG_INT | MFB_DBG_DBGLOG | MFB_DBG_WRITE_REG);
	MFBInfo.DebugMaskMsf =
		(MFB_DBG_INT | MFB_DBG_DBGLOG | MFB_DBG_WRITE_REG);
#endif

EXIT:
	LOG_DBG("- X. Ret: %d. UserCount: %d.", Ret, MFBInfo.UserCount);
	return Ret;

}

/******************************************************************************
 *
 ******************************************************************************/
static signed int MFB_release(struct inode *pInode, struct file *pFile)
{
	struct MFB_USER_INFO_STRUCT *pUserInfo = NULL;
	/*unsigned int Reg;*/

	LOG_DBG("- E. UserCount: %d.", MFBInfo.UserCount);

	/*  */
	if (pFile->private_data != NULL)
		pUserInfo = (struct MFB_USER_INFO_STRUCT *) pFile->private_data;
	else
		goto EXIT;
	/*  */
	mutex_lock(&(MutexMFBRef));
	MFBInfo.UserCount--;
	MFBInfo.IrqInfo.MssIrqUse[pUserInfo->Pid] = -1;

	if (MFBInfo.UserCount > 0) {
		mutex_unlock(&(MutexMFBRef));
		LOG_DBG(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			MFBInfo.UserCount, current->comm,
			current->pid, current->tgid);
		/*  */
		if (pFile->private_data != NULL) {
			kfree(pFile->private_data);
			pFile->private_data = NULL;
		}
		goto EXIT;
	} else {
		/*  */
		mfb_unregister_requests(&mss_reqs);
		mfb_unregister_requests(&msf_reqs);
		mfb_unregister_requests(&vmss_reqs);
		mfb_unregister_requests(&vmsf_reqs);

		mutex_unlock(&(MutexMFBRef));
		LOG_INF(
			"%s - last UserCount(%d), (process, pid, tgid)=(%s, %d, %d)",
			__func__, MFBInfo.UserCount, current->comm,
			current->pid, current->tgid);
		/*  */
		if (pFile->private_data != NULL) {
			kfree(pFile->private_data);
			pFile->private_data = NULL;
		}
	}

	/* Disable clock. */
	MFB_EnableClock(MFALSE);
	LOG_DBG("MFB release g_u4EnableClockCount: %d", g_u4EnableClockCount);

EXIT:

	LOG_DBG("- X. UserCount: %d.", MFBInfo.UserCount);
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/

static dev_t MFBDevNo;
static struct cdev *pMFBCharDrv;
static struct class *pMFBClass;

static const struct file_operations MFBFileOper = {
	.owner = THIS_MODULE,
	.open = MFB_open,
	.release = MFB_release,
	/* .flush   = mt_MFB_flush, */
	/* .mmap = MFB_mmap, */
	.unlocked_ioctl = MFB_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = MFB_ioctl_compat,
#endif
};

/******************************************************************************
 *
 ******************************************************************************/
static inline void MFB_UnregCharDev(void)
{
	LOG_DBG("- E.");
	/*  */
	/* Release char driver */
	if (pMFBCharDrv != NULL) {
		cdev_del(pMFBCharDrv);
		pMFBCharDrv = NULL;
	}
	/*  */
	unregister_chrdev_region(MFBDevNo, 1);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline signed int MFB_RegCharDev(void)
{
	signed int Ret = 0;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = alloc_chrdev_region(&MFBDevNo, 0, 1, MFB_DEV_NAME);
	if (Ret < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d", Ret);
		return Ret;
	}
	/* Allocate driver */
	pMFBCharDrv = cdev_alloc();
	if (pMFBCharDrv == NULL) {
		LOG_ERR("cdev_alloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pMFBCharDrv, &MFBFileOper);
	/*  */
	pMFBCharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pMFBCharDrv, MFBDevNo, 1);
	if (Ret < 0) {
		LOG_ERR("Attatch file operation failed, %d", Ret);
		goto EXIT;
	}
	/*  */
EXIT:
	if (Ret < 0)
		MFB_UnregCharDev();

	/*  */

	LOG_DBG("- X.");
	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int MFB_probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	/*struct resource *pRes = NULL;*/
	signed int i = 0;
	unsigned char n;
	unsigned int irq_info[3];/* Record interrupts info from device tree */
	struct device *dev = NULL;
	struct MFB_device *_mfb_dev = NULL;


#ifdef CONFIG_OF
	struct MFB_device *MFB_dev;
#endif

	LOG_INF("- E. MFB driver probe. nr_MFB_devs : %d.", nr_MFB_devs);

	/* Check platform_device parameters */
#ifdef CONFIG_OF

	if (pDev == NULL) {
		dev_dbg(&pDev->dev, "pDev is NULL");
		return -ENXIO;
	}

	nr_MFB_devs += 1;
	_mfb_dev = krealloc(MFB_devs,
		sizeof(struct MFB_device) * nr_MFB_devs, GFP_KERNEL);
	if (!_mfb_dev) {
		dev_dbg(&pDev->dev, "Unable to allocate MFB_devs\n");
		return -ENOMEM;
	}
	MFB_devs = _mfb_dev;

	MFB_dev = &(MFB_devs[nr_MFB_devs - 1]);
	MFB_dev->dev = &pDev->dev;

	/* iomap registers */
	MFB_dev->regs = of_iomap(pDev->dev.of_node, 0);
	/* gISPSYS_Reg[nr_MFB_devs - 1] = MFB_dev->regs; */

	if (!MFB_dev->regs) {
		dev_dbg(&pDev->dev,
			"Unable to ioremap registers, of_iomap fail, nr_MFB_devs=%d, devnode(%s).\n",
			nr_MFB_devs, pDev->dev.of_node->name);
		return -ENOMEM;
	}

#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
		(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
			*(MFB_dev->dev->dma_mask) =
				(u64)DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT);
			MFB_dev->dev->coherent_dma_mask =
				(u64)DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT);
#endif

	LOG_INF("nr_MFB_devs=%d, devnode(%s), map_addr=0x%lx\n", nr_MFB_devs,
		pDev->dev.of_node->name, (unsigned long)MFB_dev->regs);

	/* get IRQ ID and request IRQ */
	MFB_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);

	if (MFB_dev->irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array(
			pDev->dev.of_node, "interrupts",
			irq_info, ARRAY_SIZE(irq_info))) {
			dev_dbg(&pDev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		for (i = 0; i < MFB_IRQ_TYPE_AMOUNT; i++) {
			if (strcmp(pDev->dev.of_node->name,
				MFB_IRQ_CB_TBL[i].device_name) == 0) {
				Ret = request_irq(MFB_dev->irq,
				    (irq_handler_t) MFB_IRQ_CB_TBL[i].isr_fp,
				    irq_info[2],
				    (const char *)MFB_IRQ_CB_TBL[i].device_name,
				    NULL);

				if (Ret) {
					dev_dbg(&pDev->dev,
						"Unable to request IRQ, request_irq fail, nr_MFB_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
						nr_MFB_devs,
						pDev->dev.of_node->name,
						MFB_dev->irq,
						MFB_IRQ_CB_TBL[i].device_name);
					return Ret;
				}

				LOG_INF(
					"nr_MFB_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
					nr_MFB_devs, pDev->dev.of_node->name,
					MFB_dev->irq,
					MFB_IRQ_CB_TBL[i].device_name);
				break;
			}
		}

		if (i >= MFB_IRQ_TYPE_AMOUNT) {
			LOG_INF(
				"No corresponding ISR!!: nr_MFB_devs=%d, devnode(%s), irq=%d\n",
				nr_MFB_devs, pDev->dev.of_node->name,
				MFB_dev->irq);
		}


	} else {
		LOG_INF("No IRQ!!: nr_MFB_devs=%d, devnode(%s), irq=%d\n",
			nr_MFB_devs,
			pDev->dev.of_node->name, MFB_dev->irq);
	}

	/*cmdq*/
	MFB_cmdq_dev = &pDev->dev;
	if (nr_MFB_devs == 1) {
		mss_clt = cmdq_mbox_create(MFB_cmdq_dev, 0);
		LOG_INF("mss_clt: 0x%p\n", mss_clt);
		of_property_read_u16(MFB_cmdq_dev->of_node,
				"mss_frame_done", &mss_done_event_id);
		of_property_read_u16(MFB_cmdq_dev->of_node,
				"mss_token", &mss_token_id);

	}
	if (nr_MFB_devs == 2) {
		msf_clt = cmdq_mbox_create(MFB_cmdq_dev, 0);
		LOG_INF("msf_clt: 0x%p\n", msf_clt);
		of_property_read_u16(MFB_cmdq_dev->of_node,
				"msf_frame_done", &msf_done_event_id);
		of_property_read_u16(MFB_cmdq_dev->of_node,
				"msf_token", &msf_token_id);
	}

#endif

	/* Only register char driver in the 1st time */
	if (nr_MFB_devs == 2) {/*YWtodo*/

		/* Register char driver */
		Ret = MFB_RegCharDev();
		if (Ret) {
			dev_dbg(&pDev->dev, "register char failed");
			return Ret;
		}
#ifndef __MFB_EP_NO_CLKMGR__
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
		    /*CCF: Grab clock pointer (struct clk*) */
#if (MTK_MFB_REG_VERSION == 3)
		mfb_clk.CG_IMG2_LARB11 = devm_clk_get(&pDev->dev,
				"MFB_CG_IMG2_LARB11");
		mfb_clk.CG_IMG2_MSS = devm_clk_get(&pDev->dev,
				"MFB_CG_IMG2_MSS");
		mfb_clk.CG_IMG2_MFB = devm_clk_get(&pDev->dev,
				"MFB_CG_IMG2_MFB");
		mfb_clk.CG_IMG1_GALS = devm_clk_get(&pDev->dev,
				"MFB_CG_IMG1_GALS");

		if (IS_ERR(mfb_clk.CG_IMG2_LARB11)) {
			LOG_ERR("cannot get CG_IMG2_LARB11 clock\n");
			return PTR_ERR(mfb_clk.CG_IMG2_LARB11);
		}
		if (IS_ERR(mfb_clk.CG_IMG2_MSS)) {
			LOG_ERR("cannot get CG_IMG2_MSS clock\n");
			return PTR_ERR(mfb_clk.CG_IMG2_MSS);
		}
		if (IS_ERR(mfb_clk.CG_IMG2_MFB)) {
			LOG_ERR("cannot get CG_IMG2_MFB clock\n");
			return PTR_ERR(mfb_clk.CG_IMG2_MFB);
		}
		if (IS_ERR(mfb_clk.CG_IMG1_GALS)) {
			LOG_ERR("cannot get CG_IMG1_GALS clock\n");
			return PTR_ERR(mfb_clk.CG_IMG1_GALS);
		}
#elif (MTK_MFB_REG_VERSION == 2)
		mfb_clk.CG_IMG2_LARB11 = devm_clk_get(&pDev->dev,
				"MFB_CG_IMG2_LARB11");
		mfb_clk.CG_IMG2_MSS = devm_clk_get(&pDev->dev,
				"MFB_CG_IMG2_MSS");
		mfb_clk.CG_IMG2_MFB = devm_clk_get(&pDev->dev,
				"MFB_CG_IMG2_MFB");

		if (IS_ERR(mfb_clk.CG_IMG2_LARB11)) {
			LOG_ERR("cannot get CG_IMG2_LARB11 clock\n");
			return PTR_ERR(mfb_clk.CG_IMG2_LARB11);
		}
		if (IS_ERR(mfb_clk.CG_IMG2_MSS)) {
			LOG_ERR("cannot get CG_IMG2_MSS clock\n");
			return PTR_ERR(mfb_clk.CG_IMG2_MSS);
		}
		if (IS_ERR(mfb_clk.CG_IMG2_MFB)) {
			LOG_ERR("cannot get CG_IMG2_MFB clock\n");
			return PTR_ERR(mfb_clk.CG_IMG2_MFB);
		}
#else
		mfb_clk.CG_IMG1_LARB9 = devm_clk_get(&pDev->dev,
				"MFB_CG_IMG1_LARB9");
		mfb_clk.CG_IMG1_MSS = devm_clk_get(&pDev->dev,
				"MFB_CG_IMG1_MSS");
		mfb_clk.CG_IMG1_MFB = devm_clk_get(&pDev->dev,
				"MFB_CG_IMG1_MFB");

		if (IS_ERR(mfb_clk.CG_IMG1_LARB9)) {
			LOG_ERR("cannot get CG_IMG1_LARB9 clock\n");
			return PTR_ERR(mfb_clk.CG_IMG1_LARB9);
		}
		if (IS_ERR(mfb_clk.CG_IMG1_MSS)) {
			LOG_ERR("cannot get CG_IMG1_MSS clock\n");
			return PTR_ERR(mfb_clk.CG_IMG1_MSS);
		}
		if (IS_ERR(mfb_clk.CG_IMG1_MFB)) {
			LOG_ERR("cannot get CG_IMG1_MFB clock\n");
			return PTR_ERR(mfb_clk.CG_IMG1_MFB);
		}
#endif
#endif	/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */
#endif

		/* Create class register */
		pMFBClass = class_create(THIS_MODULE, "MFBdrv");
		if (IS_ERR(pMFBClass)) {
			Ret = PTR_ERR(pMFBClass);
			LOG_ERR("Unable to create class, err = %d", Ret);
			goto EXIT;
		}

		dev = device_create(
			pMFBClass, NULL, MFBDevNo, NULL, MFB_DEV_NAME);

		if (IS_ERR(dev)) {
			Ret = PTR_ERR(dev);
			dev_dbg(&pDev->dev, "Failed to create device: /dev/%s, err = %d",
				MFB_DEV_NAME, Ret);
			goto EXIT;
		}

		/* Init spinlocks */
		spin_lock_init(&(MFBInfo.SpinLockMFB));
		for (n = 0; n < MFB_IRQ_TYPE_AMOUNT; n++)
			spin_lock_init(&(MFBInfo.SpinLockIrq[n]));
#ifdef MFB_PMQOS
		spin_lock_init(&(SpinLockMfbPmqos));
#endif
		/*  */
		init_waitqueue_head(&MFBInfo.WaitQueueHeadMss);
		init_waitqueue_head(&MFBInfo.WaitQueueHeadMsf);
		INIT_WORK(&MFBInfo.ScheduleMssWork, MFB_ScheduleMssWork);
		INIT_WORK(&MFBInfo.vmsswork, vmss_do_work);
		INIT_WORK(&MFBInfo.ScheduleMsfWork, MFB_ScheduleMsfWork);
		INIT_WORK(&MFBInfo.vmsfwork, vmsf_do_work);

		MFBInfo.wkqueueMss =
			create_singlethread_workqueue("MSS-CMDQ-WQ");
		if (!MFBInfo.wkqueueMss)
			LOG_ERR("NULL MSS-CMDQ-WQ\n");
		MFBInfo.wkqueueMsf =
			create_singlethread_workqueue("MSF-CMDQ-WQ");
		if (!MFBInfo.wkqueueMsf)
			LOG_ERR("NULL MSF-CMDQ-WQ\n");

		wakeup_source_init(&MSS_wake_lock, "mss_lock_wakelock");
		wakeup_source_init(&MSF_wake_lock, "msf_lock_wakelock");
		INIT_WORK(&logWork, logPrint);

		for (i = 0; i < MFB_IRQ_TYPE_AMOUNT; i++)
			tasklet_init(
				MFB_tasklet[i].pMFB_tkt,
				MFB_tasklet[i].tkt_cb, 0);

		/* Init MFBInfo */
		mutex_lock(&(MutexMFBRef));
		MFBInfo.UserCount = 0;
		mutex_unlock(&(MutexMFBRef));
		/*  */
		MFBInfo.IrqInfo.Mask[MFB_IRQ_TYPE_INT_MSS_ST] = INT_ST_MASK_MSS;
		MFBInfo.IrqInfo.Mask[MFB_IRQ_TYPE_INT_MSF_ST] = INT_ST_MASK_MSF;
	}

	seqlock_init(&(mss_reqs.seqlock));
	seqlock_init(&(vmss_reqs.seqlock));
	seqlock_init(&(msf_reqs.seqlock));
	seqlock_init(&(vmsf_reqs.seqlock));

EXIT:
	if (Ret < 0)
		MFB_UnregCharDev();


	LOG_INF("- X. MFB driver probe.");

	return Ret;
}

/******************************************************************************
 * Called when the device is being detached from the driver
 ******************************************************************************/
static signed int MFB_remove(struct platform_device *pDev)
{
	/*struct resource *pRes;*/
	signed int IrqNum;
	int i;
	/*  */
	LOG_DBG("- E.");

	destroy_workqueue(MFBInfo.wkqueueMss);
	MFBInfo.wkqueueMss = NULL;
	destroy_workqueue(MFBInfo.wkqueueMsf);
	MFBInfo.wkqueueMsf = NULL;

	/* unregister char driver. */
	MFB_UnregCharDev();

	/* Release IRQ */
	/*disable_irq(MFBInfo.IrqNum);*//*YWtoclr*/
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);

	/* kill tasklet */
	for (i = 0; i < MFB_IRQ_TYPE_AMOUNT; i++)
		tasklet_kill(MFB_tasklet[i].pMFB_tkt);
#if 0
	/* free all registered irq(child nodes) */
	MFB_UnRegister_AllregIrq();
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
	device_destroy(pMFBClass, MFBDevNo);
	/*  */
	class_destroy(pMFBClass);
	pMFBClass = NULL;
	/*  */
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int bPass1_On_In_Resume_TG1;/*YWtodo*/

static signed int MFB_suspend(struct platform_device *pDev, pm_message_t Mesg)
{
	/*signed int ret = 0;*/

	if (g_u4EnableClockCount > 0) {
		MFB_EnableClock(MFALSE);
		g_SuspendCnt++;
	}
	bPass1_On_In_Resume_TG1 = 0;
	LOG_DBG("%s:g_u4EnableClockCount(%d) g_SuspendCnt(%d).\n", __func__,
				g_u4EnableClockCount, g_SuspendCnt);


	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int MFB_resume(struct platform_device *pDev)
{
	LOG_DBG("bPass1_On_In_Resume_TG1(%d).\n", bPass1_On_In_Resume_TG1);
	if (g_SuspendCnt > 0) {
		MFB_EnableClock(MTRUE);
		g_SuspendCnt--;
	}
	LOG_DBG("%s:g_u4EnableClockCount(%d) g_SuspendCnt(%d).\n", __func__,
				g_u4EnableClockCount, g_SuspendCnt);

	return 0;
}


/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int MFB_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	LOG_DBG("calling %s()\n", __func__);

	return MFB_suspend(pdev, PMSG_SUSPEND);
}

int MFB_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	LOG_DBG("calling %s()\n", __func__);

	return MFB_resume(pdev);
}
#ifndef CONFIG_OF
/* extern void mt_irq_set_sens(unsigned int irq, unsigned int sens); */
/* extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity); */
#endif
int MFB_pm_restore_noirq(struct device *device)
{
	pr_debug("calling %s()\n", __func__);
#ifndef CONFIG_OF
	mt_irq_set_sens(MFB_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(MFB_IRQ_BIT_ID, MT_POLARITY_LOW);
#endif
	return 0;

}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define MFB_pm_suspend NULL
#define MFB_pm_resume  NULL
#define MFB_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_OF
/*
 * Note!!! The order and member of .compatible must be the same with that in
 *  "MFB_DEV_NODE_ENUM" in camera_MFB.h
 */
#if (MTK_MFB_REG_VERSION >= 2)
static const struct of_device_id MFB_of_ids[] = {
	{.compatible = "mediatek,mss_b",},
	{.compatible = "mediatek,msf_b",},
	{.compatible = "mediatek,imgsys_mfb_b",},
	{}
};
#else
static const struct of_device_id MFB_of_ids[] = {
	{.compatible = "mediatek,mss",},
	{.compatible = "mediatek,msf",},
	{.compatible = "mediatek,imgsys_mfb",},
	{}
};
#endif
#endif

const struct dev_pm_ops MFB_pm_ops = {
	.suspend = MFB_pm_suspend,
	.resume = MFB_pm_resume,
	.freeze = MFB_pm_suspend,
	.thaw = MFB_pm_resume,
	/*.pmfbroff = MFB_pm_suspend,*//*YWtodo*/
	.restore = MFB_pm_resume,
	.restore_noirq = MFB_pm_restore_noirq,
};


/******************************************************************************
 *
 ******************************************************************************/
static struct platform_driver MFBDriver = {
	.probe = MFB_probe,
	.remove = MFB_remove,
	.suspend = MFB_suspend,
	.resume = MFB_resume,
	.driver = {
		   .name = MFB_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = MFB_of_ids,
#endif
#ifdef CONFIG_PM
		   .pm = &MFB_pm_ops,
#endif
	}
};

/******************************************************************************
 *
 ******************************************************************************/

int32_t MFB_ClockOnCallback(uint64_t engineFlag)
{
	/* LOG_DBG("MFB_ClockOnCallback"); */
	/* LOG_DBG("+CmdqEn:%d", g_u4EnableClockCount); */
	/* MFB_EnableClock(MTRUE); */

	return 0;
}

int32_t MFB_DumpCallback(uint64_t engineFlag, int level)
{
	LOG_DBG("MFB_DumpCallback");
	MSS_DumpReg();
	MSF_DumpReg();

	return 0;
}

int32_t MFB_ResetCallback(uint64_t engineFlag)
{
	LOG_DBG("MSS_ResetCallback");
	MSS_Reset();
	MSF_Reset();

	return 0;
}

int32_t MFB_ClockOffCallback(uint64_t engineFlag)
{
	/* LOG_DBG("MFB_ClockOffCallback"); */
	/* MFB_EnableClock(MFALSE); */
	/* LOG_DBG("-CmdqEn:%d", g_u4EnableClockCount); */
	return 0;
}

static signed int __init MFB_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
	int i;
	/*  */
	LOG_DBG("- E.");
	/*  */
	Ret = platform_driver_register(&MFBDriver);
	if (Ret < 0) {
		LOG_ERR("platform_driver_register fail");
		return Ret;
	}

#if 0
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,MFB");
	if (!node) {
		LOG_ERR("find mediatek,MFB node failed!!!\n");
		return -ENODEV;
	}
	ISP_MFB_BASE = of_iomap(node, 0);
	if (!ISP_MFB_BASE) {
		LOG_ERR("unable to map ISP_MFB_BASE registers!!!\n");
		return -ENODEV;
	}
	LOG_DBG("ISP_MFB_BASE: %lx\n", ISP_MFB_BASE);
#endif

	/* isr log */
	if (PAGE_SIZE < ((MFB_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN * ((
	    DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM)) {
		i = 0;
		while (i <
		       ((MFB_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
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
		for (j = 0; j < MFB_IRQ_TYPE_AMOUNT; j++) {
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
	/* Register MFB callback */
	LOG_DBG("register mfb callback for CMDQ");
#if 0/*YWtodo*/
	cmdqCoreRegisterCB(CMDQ_GROUP_MDP,
			   MFB_ClockOnCallback,
			   MFB_DumpCallback, MFB_ResetCallback,
			   MFB_ClockOffCallback);
#endif

#ifdef MFB_PMQOS
	MFBQOS_Init();
#endif
	LOG_DBG("- X. Ret: %d.", Ret);
	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static void __exit MFB_Exit(void)
{
	/*int i;*/

	LOG_DBG("- E.");

#ifdef MFB_PMQOS
	MFBQOS_Uninit();
#endif
	/*  */
	platform_driver_unregister(&MFBDriver);
	/*  */
	/* Cmdq */
	/* Unregister MFB callback */
	/*cmdqCoreRegisterCB(CMDQ_GROUP_MDP, NULL, NULL, NULL, NULL);YWtodo*/

	kfree(pLog_kmalloc);

	/*  */
}


/******************************************************************************
 *
 ******************************************************************************/
static void MFB_ScheduleMssWork(struct work_struct *data)
{
	if (MFB_DBG_DBGLOG & MFBInfo.DebugMaskMss)
		LOG_DBG("- E.");
	mfb_request_handler(&mss_reqs, &(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MSS_ST]));
	if (!mfb_request_running(&mss_reqs))
		LOG_DBG("[%s]no more requests", __func__);
}

static void vmss_do_work(struct work_struct *data)
{
	if (MFB_DBG_DBGLOG & MFBInfo.DebugMaskMss)
		LOG_DBG("- E.");
	mfb_request_handler(&vmss_reqs, &(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MSS_ST]));
	if (!mfb_request_running(&vmss_reqs))
		LOG_DBG("[%s]no more requests", __func__);
}

/******************************************************************************
 *
 ******************************************************************************/
static void MFB_ScheduleMsfWork(struct work_struct *data)
{
	if (MFB_DBG_DBGLOG & MFBInfo.DebugMaskMsf)
		LOG_DBG("- E.");
	mfb_request_handler(&msf_reqs,
		&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]));
	if (!mfb_request_running(&msf_reqs))
		LOG_DBG("[%s]no more requests", __func__);
}

static void vmsf_do_work(struct work_struct *data)
{
	if (MFB_DBG_DBGLOG & MFBInfo.DebugMaskMsf)
		LOG_DBG("- E.");
	mfb_request_handler(&vmsf_reqs, &(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MSF_ST]));
	if (!mfb_request_running(&vmsf_reqs))
		LOG_DBG("[%s]no more requests", __func__);
}


static irqreturn_t ISP_Irq_MSS(signed int Irq, void *DeviceId)
{
#if 1
	/*unsigned int MssStatus;*/

	/*MssStatus = MFB_RD32(MFB_MSS_INT_STATUS_REG);*//* MSS Status */
	/*MFB_WR32(MFB_MSS_INT_STATUS_REG, MssStatus);*//* MSS Status */
	LOG_DBG("%s:0x%x = 0x%x ", __func__,
			MFB_MSS_INT_STATUS_HW, MssStatus);

#else
	unsigned int MssStatus;
	bool bResulst = MFALSE;
	pid_t ProcessID;

	MssStatus = MFB_RD32(MFB_MSS_INT_STATUS_REG);	/* MSS Status */
	MFB_WR32(MFB_MSS_INT_STATUS_REG, MssStatus);	/* MSS Status */

	spin_lock(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]));
	if (MSS_INT_ST == (MSS_INT_ST & MssStatus)) {
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("mfb_mss_irq");
#endif

		if (mfb_update_request(&mss_reqs, &ProcessID) == 0)
			bResulst = MTRUE;
		/* Config the Next frame */
		if (bResulst == MTRUE) {
			#if REQUEST_REGULATION == REQUEST_BASE_REGULATION
			/* schedule_work(&&MFBInfo.ScheduleMssWork); */
			queue_work(MFBInfo.wkqueue, &MFBInfo.ScheduleMssWork);
			#endif

			MFBInfo.IrqInfo
			    .Status[MFB_IRQ_TYPE_INT_MSS_ST] |= MSS_INT_ST;
			MFBInfo.IrqInfo
			    .ProcessID[MFB_PROCESS_ID_MSS] = ProcessID;
			MFBInfo.IrqInfo.MssIrqCnt++;
		}
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
	}
	spin_unlock(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSS_ST]));
	if (bResulst == MTRUE)
		wake_up_interruptible(&MFBInfo.WaitQueueHeadMss);

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(MFB_IRQ_TYPE_INT_MSS_ST, m_CurrentPPB, _LOG_INF,
		       "Irq_MSS:%d, reg 0x%x : 0x%x, bResulst:%d, MssIrqCnt:0x%x\n",
		       Irq, MFB_MSS_INT_STATUS_REG, MssStatus,
		       bResulst, MFBInfo.IrqInfo.MssIrqCnt);

	#if (REQUEST_REGULATION == FRAME_BASE_REGULATION)
	/* schedule_work(&MFBInfo.ScheduleMssWork); */
	if (MSS_INT_ST == (MSS_INT_ST & MssStatus))
		queue_work(MFBInfo.wkqueueMss, &MFBInfo.ScheduleMssWork);
	#endif

	if (MssStatus & MSS_INT_ST)
		tasklet_schedule(MFB_tasklet[MFB_IRQ_TYPE_INT_MSS_ST].pMFB_tkt);

#endif
	return IRQ_HANDLED;
}

static irqreturn_t ISP_Irq_MSF(signed int Irq, void *DeviceId)
{
#if 1
	/*unsigned int MsfStatus;*/

	/*MsfStatus = MFB_RD32(MFB_MSF_INT_STATUS_REG);*//* MSF Status */
	/*MFB_WR32(MFB_MSF_INT_STATUS_REG, MsfStatus);*//* MSF Status */
	LOG_DBG("%s:0x%x = 0x%x ", __func__,
			MFB_MSF_INT_STATUS_HW, MsfStatus);
#else
	unsigned int MsfStatus;
	bool bResulst = MFALSE;
	pid_t ProcessID;

	MsfStatus = MFB_RD32(MFB_MSF_INT_STATUS_REG);	/* MSF Status */
	MFB_WR32(MFB_MSF_INT_STATUS_REG, MsfStatus);	/* MSF Status */

	spin_lock(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]));
	if (MSF_INT_ST == (MSF_INT_ST & MsfStatus)) {
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("mfb_msf_irq");
#endif

		if (mfb_update_request(&msf_reqs, &ProcessID) == 0)
			bResulst = MTRUE;
		/* Config the Next frame */
		if (bResulst == MTRUE) {
			#if REQUEST_REGULATION == REQUEST_BASE_REGULATION
			/* schedule_work(&&MFBInfo.ScheduleMsfWork); */
			queue_work(MFBInfo.wkqueue, &MFBInfo.ScheduleMsfWork);
			#endif

			MFBInfo.IrqInfo
			    .Status[MFB_IRQ_TYPE_INT_MSF_ST] |= MSF_INT_ST;
			MFBInfo.IrqInfo
			    .ProcessID[MFB_PROCESS_ID_MSF] = ProcessID;
			MFBInfo.IrqInfo.MsfIrqCnt++;
		}
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
	}
	spin_unlock(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MSF_ST]));
	if (bResulst == MTRUE)
		wake_up_interruptible(&MFBInfo.WaitQueueHeadMsf);

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(MFB_IRQ_TYPE_INT_MSF_ST, m_CurrentPPB, _LOG_INF,
		       "Irq_MSF:%d, reg 0x%x : 0x%x, bResulst:%d, MsfIrqCnt:0x%x\n",
		       Irq, MFB_MSF_INT_STATUS_REG, MsfStatus,
		       bResulst, MFBInfo.IrqInfo.MsfIrqCnt);

	#if (REQUEST_REGULATION == FRAME_BASE_REGULATION)
	/* schedule_work(&MFBInfo.ScheduleMsfWork); */
	if (MSF_INT_ST == (MSF_INT_ST & MsfStatus))
		queue_work(MFBInfo.wkqueueMsf, &MFBInfo.ScheduleMsfWork);
	#endif

	if (MsfStatus & MSF_INT_ST)
		tasklet_schedule(MFB_tasklet[MFB_IRQ_TYPE_INT_MSF_ST].pMFB_tkt);


#endif
	return IRQ_HANDLED;
}

static void ISP_TaskletFunc_MSS(unsigned long data)
{
	IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MSS_ST, m_CurrentPPB, _LOG_DBG);
	IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MSS_ST, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MSS_ST, m_CurrentPPB, _LOG_ERR);

}

static void ISP_TaskletFunc_MSF(unsigned long data)
{
	IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MSF_ST, m_CurrentPPB, _LOG_DBG);
	IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MSF_ST, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MSF_ST, m_CurrentPPB, _LOG_ERR);

}
static void logPrint(struct work_struct *data)
{
	unsigned long arg = 0;

	ISP_TaskletFunc_MSS(arg);
	ISP_TaskletFunc_MSF(arg);
}

/******************************************************************************
 *
 ******************************************************************************/
module_init(MFB_Init);
module_exit(MFB_Exit);
MODULE_DESCRIPTION("Camera MFB driver");
MODULE_AUTHOR("MM3SW5");
MODULE_LICENSE("GPL");
