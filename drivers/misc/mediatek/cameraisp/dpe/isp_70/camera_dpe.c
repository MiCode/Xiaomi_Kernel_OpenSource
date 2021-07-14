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
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
// V4L2
#include <linux/mutex.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-ioctl.h>
#define KERNEL_DMA_BUFFER
#ifdef KERNEL_DMA_BUFFER
#include <media/videobuf2-memops.h>
#include <linux/dma-direction.h>
#include <linux/dma-buf.h>
#endif
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
#include <mt-plat/sync_write.h> /* For mt65xx_reg_sync_writel(). */
/* For spm_enable_sodi()/spm_disable_sodi(). */
/* #include <mach/mt_spm_idle.h> */
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
//#include <cmdq_core.h>
//#include <cmdq_record.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
//! for IOVA to PA
#include <linux/iommu.h>
#define SMI_CLK
#ifdef CONFIG_MTK_IOMMU_V2
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#else /* CONFIG_MTK_IOMMU_V2 */
//#include <m4u.h>
#endif /* CONFIG_MTK_IOMMU_V2 */
#include <smi_public.h>
#include "engine_request.h"
#ifdef KERNEL_DMA_BUFFER
#include "videobuf2-dma-contig.h"
#endif
/*#define DPE_PMQOS_EN*/
#if defined(DPE_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
#include <linux/pm_qos.h>
#endif
/* Measure the kernel performance
 * #define __DPE_KERNEL_PERFORMANCE_MEASURE__
 */
#ifdef __DPE_KERNEL_PERFORMANCE_MEASURE__
#include <linux/met_drv.h>
#include <linux/mtk_ftrace.h>
#endif
#ifdef Another_Performance_Measure
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
struct DPE_CLK_STRUCT {
	struct clk *IMG_IPE;
	struct clk *IPE_DPE;
	struct clk *IPE_SMI_LARB12;
	struct clk *IPE_TOP;
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
//#define EP_NO_CLKMGR
#define BYPASS_REG (0)
#define DUMMY_DPE (0)
#define UT_CASE
/*I can' test the situation in FPGA due to slow FPGA. */
#define MyTag "[DPE]"
#define IRQTag "KEEPER"
#define LOG_VRB(format, args...) pr_debug(MyTag format, ##args)
//#define DPE_DEBUG_USE
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
#define DPE_WR32(addr, data) mt_reg_sync_writel(data, addr)
#define DPE_RD32(addr) ioread32(addr)
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
#define MASK_15 0xF
#define ALIGN16(x) (((x)+MASK_15)&(~(MASK_15)))
#define MAX_NUM_TILE 4
#define TILE_WITH_NUM 3
//#define IOVA_TO_PA
#ifdef CONFIG_MTK_IOMMU_V2
static int DPE_MEM_USE_VIRTUL = 1;
#endif
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
struct wakeup_source DPE_wake_lock;
static DEFINE_MUTEX(gDpeMutex);
static DEFINE_MUTEX(gDpeDequeMutex);
#ifdef CONFIG_OF
struct DPE_device {
	void __iomem *regs;
	struct device *dev;
	struct device		*larb12;
	int irq;
// V4L2
	struct v4l2_device v4l2_dev;
	struct mutex mutex;
	struct video_device vid_dpe_dev;
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
#ifdef SMI_CLK
struct platform_device *DPE_pdev;
#endif
#ifdef KERNEL_DMA_BUFFER
struct device *gdev;
struct dma_buf *dbuf;
struct vb2_dc_buf {
	struct device			*dev;
	void				*vaddr;
	unsigned long			size;
	void				*cookie;
	dma_addr_t			dma_addr;
	unsigned long			attrs;
	enum dma_data_direction		dma_dir;
	struct sg_table			*dma_sgt;
	struct frame_vector		*vec;
	/* MMAP related */
	struct vb2_vmarea_handler	handler;
	refcount_t			refcount;
	struct sg_table			*sgt_base;
	const char *name;
	/* DMABUF related */
	struct dma_buf_attachment	*db_attach;
	struct dma_buf			*dma_buf;
	struct sg_table			*sgt;
};
struct vb2_dc_buf *kernel_dpebuf;
struct vb2_dc_buf *dpebuf;
unsigned int *g_dpewb_dvme_int_Buffer_pa;
unsigned int *g_dpewb_cost_int_Buffer_pa;
unsigned int *g_dpewb_asfrm_Buffer_pa;
unsigned int *g_dpewb_asfrmext_Buffer_pa;
unsigned int *g_dpewb_wmfhf_Buffer_pa;
#else
struct device *gdev;
#endif
static unsigned int g_u4EnableClockCount;
static unsigned int g_SuspendCnt;
/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32
bool g_DPE_PMState;
bool g_isDPELogEnable = MFALSE;
//for cmdq malibox
static struct cmdq_client *dpe_clt;
struct cmdq_base *dpe_clt_base;
u32 dvs_event_id;
u32 dvp_event_id;
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
	unsigned int enqueReqNum;   /* to judge it belongs to which frame package */
	unsigned int FrameWRIdx; /* Frame write Index */
	unsigned int RrameRDIdx; /* Frame read Index */
	enum DPE_FRAME_STATUS_ENUM
		DpeFrameStatus[_SUPPORT_MAX_DPE_FRAME_REQUEST_];
	struct DPE_Config DpeFrameConfig[_SUPPORT_MAX_DPE_FRAME_REQUEST_];
};
struct DPE_REQUEST_RING_STRUCT {
	unsigned int WriteIdx;     /* enque how many request  */
	unsigned int ReadIdx;      /* read which request index */
	unsigned int HWProcessIdx; /* HWWriteIdx */
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
#define PMD_ENTRIES_MAX	512
#define MMU_ION_BUF		BIT(24)
union mmu_table {
	u64		*entries;	/* Array of PTEs */
	/* Array of pages */
	struct page	**pages;
	/* Array of VAs */
	uintptr_t	*vas;
	/* Address of table */
	void		*addr;
	/* Page for table */
	unsigned long	page;
};
//static struct DPE_device *DPE_DVSreg;//!
struct tee_mmu {
	struct iommu_domain	*domain;
	dma_addr_t			dma_addr;
	/* ION case only */
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attach;
	struct sg_table			*sgt;
};
//struct tee_mmu	*mmu;
struct tee_mmu	*DVS_mmu;
struct tee_mmu	*DVP_mmu;
unsigned int DPE_P4_EN;
unsigned int DPE_is16BitMode;
struct DPE_Config *pDpeUserConfig;
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
	signed int DpeIrqCnt[IRQ_USER_NUM_MAX];
	pid_t ProcessID[IRQ_USER_NUM_MAX];
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
#if defined(DPE_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
struct pm_qos_request dpe_pm_qos_request;
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
} *PSV_LOG_STR;
static void *pLog_kmalloc;
static struct SV_LOG_STR gSvLog[DPE_IRQ_TYPE_AMOUNT];
/*
 *   for irq used,keep log until IRQ_LOG_PRINTER being involked,
 *   limited:
 *   each log must shorter than 512 bytes
 *  total log length in each irq/logtype can't over 1024 bytes
 */
#ifdef IRQ_LOG
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
	ptr = pDes = (char *)\
		&(gSvLog[irq]._str[ppb][logT][gSvLog[irq]._cnt[ppb][logT]]);   \
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
		ret = snprintf((char *)(pDes), avaLen, fmt, ##__VA_ARGS__);  \
		if (ret < 0) { \
			LOG_ERR("snprintf fail(%d)\n", ret); \
		} \
			while (*ptr++ != '\0') {\
				(*ptr2)++;\
			} \
		} \
	} \
} while (0)
#else
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...) \
	xlog_printk(ANDROID_LOG_DEBUG,\
"KEEPER", "[%s] " fmt, __func__, ##__VA_ARGS__)
#endif
#ifdef IRQ_LOG
#define IRQ_LOG_PRINTER(irq, ppb_in, logT_in) do {\
	struct SV_LOG_STR *pSrc = &gSvLog[irq];\
	char *ptr;\
	unsigned int i;\
	unsigned int ppb = 0;\
	unsigned int logT = 0;\
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
#else
#define IRQ_LOG_PRINTER(irq, ppb, logT)
#endif
#define IPESYS_REG_CG_CON               (ISP_IPESYS_BASE + 0x0)
#define IPESYS_REG_CG_SET               (ISP_IPESYS_BASE + 0x4)
#define IPESYS_REG_CG_CLR               (ISP_IPESYS_BASE + 0x8)
/* DPE unmapped base address macro for GCE to access */
#define DVS_CTRL00_HW                (DPE_BASE_HW)
#define DVS_CTRL01_HW                (DPE_BASE_HW + 0x004)
#define DVS_CTRL02_HW                (DPE_BASE_HW + 0x008)
#define DVS_CTRL03_HW                (DPE_BASE_HW + 0x00C)
//#define DVS_CTRL04_HW                (DPE_BASE_HW + 0x010)
//#define DVS_CTRL05_HW                (DPE_BASE_HW + 0x014)
#define DVS_CTRL06_HW                (DPE_BASE_HW + 0x018)
#define DVS_CTRL07_HW                (DPE_BASE_HW + 0x01C)
//#define DVS_OCC_PQ_0_HW              (DPE_BASE_HW + 0x030)
//#define DVS_OCC_PQ_1_HW              (DPE_BASE_HW + 0x034)
//#define DVS_OCC_ATPG_HW              (DPE_BASE_HW + 0x038)
#define DVS_IRQ_00_HW                (DPE_BASE_HW + 0x040)
#define DVS_CTRL_STATUS0_HW          (DPE_BASE_HW + 0x050)
//#define DVS_CTRL_STATUS1_HW          (DPE_BASE_HW + 0x054)
#define DVS_CTRL_STATUS2_HW          (DPE_BASE_HW + 0x058)
#define DVS_IRQ_STATUS_HW            (DPE_BASE_HW + 0x05C)
#define DVS_FRM_STATUS0_HW           (DPE_BASE_HW + 0x060)
#define DVS_FRM_STATUS1_HW           (DPE_BASE_HW + 0x064)
//#define DVS_FRM_STATUS2_HW           (DPE_BASE_HW + 0x068)
//#define DVS_FRM_STATUS3_HW           (DPE_BASE_HW + 0x06C)
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
#define DVS_SRC_21_P4_L_DV_ADR_HW    (DPE_BASE_HW + 0x154)
//#define DVS_SRC_22_MEDV0_HW          (DPE_BASE_HW + 0x158)
//#define DVS_SRC_23_MEDV1_HW          (DPE_BASE_HW + 0x15C)
//#define DVS_SRC_24_MEDV2_HW          (DPE_BASE_HW + 0x160)
//#define DVS_SRC_25_MEDV3_HW          (DPE_BASE_HW + 0x164)
#define DVS_SRC_26_OCCDV0_HW         (DPE_BASE_HW + 0x168)
#define DVS_SRC_27_OCCDV1_HW         (DPE_BASE_HW + 0x16C)
#define DVS_SRC_28_OCCDV2_HW         (DPE_BASE_HW + 0x170)
#define DVS_SRC_29_OCCDV3_HW         (DPE_BASE_HW + 0x174)
//#define DVS_SRC_30_DCV_CONF0_HW      (DPE_BASE_HW + 0x178)
//#define DVS_SRC_31_DCV_CONF1_HW      (DPE_BASE_HW + 0x17C)
//#define DVS_SRC_32_DCV_CONF2_HW      (DPE_BASE_HW + 0x180)
//#define DVS_SRC_33_DCV_CONF3_HW      (DPE_BASE_HW + 0x184)
#define DVS_SRC_34_P4_R_DV_ADR_HW     (DPE_BASE_HW + 0x188)
//#define DVS_SRC_35_DCV_L_FRM1_HW     (DPE_BASE_HW + 0x18C)
//#define DVS_SRC_36_DCV_L_FRM2_HW     (DPE_BASE_HW + 0x190)
//#define DVS_SRC_37_DCV_L_FRM3_HW     (DPE_BASE_HW + 0x194)
//#define DVS_SRC_38_DCV_R_FRM0_HW     (DPE_BASE_HW + 0x198)
//#define DVS_SRC_39_DCV_R_FRM1_HW     (DPE_BASE_HW + 0x19C)
//#define DVS_SRC_40_DCV_R_FRM2_HW     (DPE_BASE_HW + 0x1A0)
//#define DVS_SRC_41_DCV_R_FRM3_HW     (DPE_BASE_HW + 0x1A4)
#define DVS_SRC_42_OCCDV_EXT0_HW     (DPE_BASE_HW + 0x1A8)
#define DVS_SRC_43_OCCDV_EXT1_HW     (DPE_BASE_HW + 0x1AC)
#define DVS_SRC_44_OCCDV_EXT2_HW     (DPE_BASE_HW + 0x1B0)
#define DVS_SRC_45_OCCDV_EXT3_HW     (DPE_BASE_HW + 0x1B4)
#define DVS_SRC_46_HW		(DPE_BASE_HW + 0x1B8)
#define DVS_CRC_OUT_0_HW             (DPE_BASE_HW + 0x1C0)
#define DVS_CRC_OUT_1_HW             (DPE_BASE_HW + 0x1C4)
#define DVS_CRC_OUT_2_HW             (DPE_BASE_HW + 0x1C8)
#define DVS_CRC_OUT_3_HW             (DPE_BASE_HW + 0x1CC)
#define DVS_DRAM_SEC_HW              (DPE_BASE_HW + 0x1D0)
//#define DVS_PD_SRC_00_L_FRM0_HW      (DPE_BASE_HW + 0x200)
//#define DVS_PD_SRC_01_L_FRM1_HW      (DPE_BASE_HW + 0x204)
//#define DVS_PD_SRC_02_L_FRM2_HW      (DPE_BASE_HW + 0x208)
//#define DVS_PD_SRC_03_L_FRM3_HW      (DPE_BASE_HW + 0x20C)
//#define DVS_PD_SRC_04_R_FRM0_HW      (DPE_BASE_HW + 0x210)
//#define DVS_PD_SRC_05_R_FRM1_HW      (DPE_BASE_HW + 0x214)
//#define DVS_PD_SRC_06_R_FRM2_HW      (DPE_BASE_HW + 0x218)
//#define DVS_PD_SRC_07_R_FRM3_HW      (DPE_BASE_HW + 0x21C)
//#define DVS_PD_SRC_08_OCCDV0_HW      (DPE_BASE_HW + 0x220)
//#define DVS_PD_SRC_09_OCCDV1_HW      (DPE_BASE_HW + 0x224)
//#define DVS_PD_SRC_10_OCCDV2_HW      (DPE_BASE_HW + 0x228)
//#define DVS_PD_SRC_11_OCCDV3_HW      (DPE_BASE_HW + 0x22C)
//#define DVS_PD_SRC_12_OCCDV_EXT0_HW  (DPE_BASE_HW + 0x230)
//#define DVS_PD_SRC_13_OCCDV_EXT1_HW  (DPE_BASE_HW + 0x234)
//#define DVS_PD_SRC_14_OCCDV_EXT2_HW  (DPE_BASE_HW + 0x238)
//#define DVS_PD_SRC_15_OCCDV_EXT3_HW  (DPE_BASE_HW + 0x23C)
//#define DVS_PD_SRC_16_DCV_CONF0_HW   (DPE_BASE_HW + 0x240)
//#define DVS_PD_SRC_17_DCV_CONF1_HW   (DPE_BASE_HW + 0x244)
//#define DVS_PD_SRC_18_DCV_CONF2_HW   (DPE_BASE_HW + 0x248)
//#define DVS_PD_SRC_19_DCV_CONF3_HW   (DPE_BASE_HW + 0x24C)
#define DVS_CTRL_RESERVED_HW         (DPE_BASE_HW + 0x2F8)
#define DVS_CTRL_ATPG_HW             (DPE_BASE_HW + 0x2FC)
#define DVS_ME_00_HW                 (DPE_BASE_HW + 0x300)
#define DVS_ME_01_HW                 (DPE_BASE_HW + 0x304)
#define DVS_ME_02_HW                 (DPE_BASE_HW + 0x308)
#define DVS_ME_03_HW                 (DPE_BASE_HW + 0x30C)
#define DVS_ME_04_HW                 (DPE_BASE_HW + 0x310)
//#define DVS_ME_05_HW                 (DPE_BASE_HW + 0x314)
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
#define DVS_ME_37_HW                 (DPE_BASE_HW + 0x394)
#define DVS_ME_38_HW                 (DPE_BASE_HW + 0x398)
#define DVS_ME_39_HW                 (DPE_BASE_HW + 0x39C)
//#define DVS_STATUS_00_HW             (DPE_BASE_HW + 0x3E0)
//#define DVS_STATUS_01_HW             (DPE_BASE_HW + 0x3E4)
//#define DVS_STATUS_02_HW             (DPE_BASE_HW + 0x3E8)
//#define DVS_STATUS_03_HW             (DPE_BASE_HW + 0x3EC)
#define DVS_DEBUG_HW                 (DPE_BASE_HW + 0x3F4)
#define DVS_ME_RESERVED_HW           (DPE_BASE_HW + 0x3F8)
#define DVS_ME_ATPG_HW               (DPE_BASE_HW + 0x3FC)
#define DVS_ME_40_HW                 (DPE_BASE_HW + 0x400)
#define DVS_ME_41_HW                 (DPE_BASE_HW + 0x404)
#define DVS_OCC_PQ_0_HW              (DPE_BASE_HW + 0x3A0)
#define DVS_OCC_PQ_1_HW              (DPE_BASE_HW + 0x3A4)
#define DVS_OCC_PQ_2_HW              (DPE_BASE_HW + 0x3A8)
#define DVS_OCC_PQ_3_HW              (DPE_BASE_HW + 0x3AC)
#define DVS_OCC_PQ_4_HW              (DPE_BASE_HW + 0x3B0)
#define DVS_OCC_PQ_5_HW              (DPE_BASE_HW + 0x3B4)
#define DVS_OCC_PQ_6_HW              (DPE_BASE_HW + 0x3B8)
#define DVS_OCC_PQ_7_HW              (DPE_BASE_HW + 0x3BC)
#define DVS_OCC_PQ_8_HW              (DPE_BASE_HW + 0x3C0)
#define DVS_OCC_PQ_9_HW              (DPE_BASE_HW + 0x3C4)
#define DVS_OCC_PQ_10_HW             (DPE_BASE_HW + 0x3C8)
#define DVS_OCC_PQ_11_HW             (DPE_BASE_HW + 0x3CC)
#define DVS_OCC_ATPG_HW              (DPE_BASE_HW + 0x3D0)
#define DVP_CTRL00_HW                (DPE_BASE_HW + 0x800)
#define DVP_CTRL01_HW                (DPE_BASE_HW + 0x804)
#define DVP_CTRL02_HW                (DPE_BASE_HW + 0x808)
#define DVP_CTRL03_HW                (DPE_BASE_HW + 0x80C)
#define DVP_CTRL04_HW                (DPE_BASE_HW + 0x810)
//#define DVP_CTRL05_HW                (DPE_BASE_HW + 0x814)
//#define DVP_CTRL06_HW                (DPE_BASE_HW + 0x818)
#define DVP_CTRL07_HW                (DPE_BASE_HW + 0x81C)
#define DVP_IRQ_00_HW                (DPE_BASE_HW + 0x840)
#define DVP_IRQ_01_HW                (DPE_BASE_HW + 0x844)
#define DVP_CTRL_STATUS0_HW          (DPE_BASE_HW + 0x850)
#define DVP_CTRL_STATUS1_HW          (DPE_BASE_HW + 0x854)
#define DVP_CTRL_STATUS2_HW          (DPE_BASE_HW + 0x858)
#define DVP_IRQ_STATUS_HW            (DPE_BASE_HW + 0x85C)
#define DVP_FRM_STATUS0_HW           (DPE_BASE_HW + 0x860)
//#define DVP_FRM_STATUS1_HW           (DPE_BASE_HW + 0x864)
#define DVP_FRM_STATUS2_HW           (DPE_BASE_HW + 0x868)
//#define DVP_FRM_STATUS3_HW           (DPE_BASE_HW + 0x86C)
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
//#define DVP_CORE_16_HW               (DPE_BASE_HW + 0x940)
//#define DVP_CORE_17_HW               (DPE_BASE_HW + 0x944)
//#define DVP_CORE_18_HW               (DPE_BASE_HW + 0x948)
//#define DVP_CORE_19_HW               (DPE_BASE_HW + 0x94C)
//#define DVP_CORE_20_HW               (DPE_BASE_HW + 0x950)
//#define DVP_CORE_21_HW               (DPE_BASE_HW + 0x954)
//#define DVP_CORE_22_HW               (DPE_BASE_HW + 0x958)
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
//#define DVS_CTRL04_REG               (ISP_DPE_BASE + 0x010)
//#define DVS_CTRL05_REG               (ISP_DPE_BASE + 0x014)
#define DVS_CTRL06_REG               (ISP_DPE_BASE + 0x018)
#define DVS_CTRL07_REG               (ISP_DPE_BASE + 0x01C)
//#define DVS_OCC_PQ_0_REG             (ISP_DPE_BASE + 0x030)
//#define DVS_OCC_PQ_1_REG             (ISP_DPE_BASE + 0x034)
//#define DVS_OCC_ATPG_REG             (ISP_DPE_BASE + 0x038)
#define DVS_IRQ_00_REG               (ISP_DPE_BASE + 0x040)
#define DVS_CTRL_STATUS0_REG         (ISP_DPE_BASE + 0x050)
//#define DVS_CTRL_STATUS1_REG         (ISP_DPE_BASE + 0x054)
#define DVS_CTRL_STATUS2_REG         (ISP_DPE_BASE + 0x058)
#define DVS_IRQ_STATUS_REG           (ISP_DPE_BASE + 0x05C)
#define DVS_FRM_STATUS0_REG          (ISP_DPE_BASE + 0x060)
#define DVS_FRM_STATUS1_REG          (ISP_DPE_BASE + 0x064)
//#define DVS_FRM_STATUS2_REG          (ISP_DPE_BASE + 0x068)
//#define DVS_FRM_STATUS3_REG          (ISP_DPE_BASE + 0x06C)
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
#define DVS_SRC_21_P4_L_DV_ADR_REG    (ISP_DPE_BASE + 0x154)
//#define DVS_SRC_22_MEDV0_REG         (ISP_DPE_BASE + 0x158)
//#define DVS_SRC_23_MEDV1_REG         (ISP_DPE_BASE + 0x15C)
//#define DVS_SRC_24_MEDV2_REG         (ISP_DPE_BASE + 0x160)
//#define DVS_SRC_25_MEDV3_REG         (ISP_DPE_BASE + 0x164)
#define DVS_SRC_26_OCCDV0_REG        (ISP_DPE_BASE + 0x168)
#define DVS_SRC_27_OCCDV1_REG        (ISP_DPE_BASE + 0x16C)
#define DVS_SRC_28_OCCDV2_REG        (ISP_DPE_BASE + 0x170)
#define DVS_SRC_29_OCCDV3_REG        (ISP_DPE_BASE + 0x174)
#define DVS_SRC_34_P4_R_DV_ADR_REG    (ISP_DPE_BASE + 0x188)
//#define DVS_SRC_35_DCV_L_FRM1_REG    (ISP_DPE_BASE + 0x18C)
//#define DVS_SRC_36_DCV_L_FRM2_REG    (ISP_DPE_BASE + 0x190)
//#define DVS_SRC_37_DCV_L_FRM3_REG    (ISP_DPE_BASE + 0x194)
//#define DVS_SRC_38_DCV_R_FRM0_REG    (ISP_DPE_BASE + 0x198)
//#define DVS_SRC_39_DCV_R_FRM1_REG    (ISP_DPE_BASE + 0x19C)
//#define DVS_SRC_40_DCV_R_FRM2_REG    (ISP_DPE_BASE + 0x1A0)
//#define DVS_SRC_41_DCV_R_FRM3_REG    (ISP_DPE_BASE + 0x1A4)
#define DVS_SRC_42_OCCDV_EXT0_REG    (ISP_DPE_BASE + 0x1A8)
#define DVS_SRC_43_OCCDV_EXT1_REG    (ISP_DPE_BASE + 0x1AC)
#define DVS_SRC_44_OCCDV_EXT2_REG    (ISP_DPE_BASE + 0x1B0)
#define DVS_SRC_45_OCCDV_EXT3_REG    (ISP_DPE_BASE + 0x1B4)
#define DVS_SRC_46_REG			(ISP_DPE_BASE + 0x1B8)
#define DVS_CRC_OUT_0_REG            (ISP_DPE_BASE + 0x1C0)
#define DVS_CRC_OUT_1_REG            (ISP_DPE_BASE + 0x1C4)
#define DVS_CRC_OUT_2_REG            (ISP_DPE_BASE + 0x1C8)
#define DVS_CRC_OUT_3_REG            (ISP_DPE_BASE + 0x1CC)
#define DVS_DRAM_SEC_REG             (ISP_DPE_BASE + 0x1D0)
//#define DVS_PD_SRC_00_L_FRM0_REG     (ISP_DPE_BASE + 0x200)
//#define DVS_PD_SRC_01_L_FRM1_REG     (ISP_DPE_BASE + 0x204)
//#define DVS_PD_SRC_02_L_FRM2_REG     (ISP_DPE_BASE + 0x208)
//#define DVS_PD_SRC_03_L_FRM3_REG     (ISP_DPE_BASE + 0x20C)
//#define DVS_PD_SRC_04_R_FRM0_REG     (ISP_DPE_BASE + 0x210)
//#define DVS_PD_SRC_05_R_FRM1_REG     (ISP_DPE_BASE + 0x214)
//#define DVS_PD_SRC_06_R_FRM2_REG     (ISP_DPE_BASE + 0x218)
//#define DVS_PD_SRC_07_R_FRM3_REG     (ISP_DPE_BASE + 0x21C)
//#define DVS_PD_SRC_08_OCCDV0_REG     (ISP_DPE_BASE + 0x220)
//#define DVS_PD_SRC_09_OCCDV1_REG     (ISP_DPE_BASE + 0x224)
//#define DVS_PD_SRC_10_OCCDV2_REG     (ISP_DPE_BASE + 0x228)
//#define DVS_PD_SRC_11_OCCDV3_REG     (ISP_DPE_BASE + 0x22C)
//#define DVS_PD_SRC_12_OCCDV_EXT0_REG (ISP_DPE_BASE + 0x230)
//#define DVS_PD_SRC_13_OCCDV_EXT1_REG (ISP_DPE_BASE + 0x234)
//#define DVS_PD_SRC_14_OCCDV_EXT2_REG (ISP_DPE_BASE + 0x238)
//#define DVS_PD_SRC_15_OCCDV_EXT3_REG (ISP_DPE_BASE + 0x23C)
//#define DVS_PD_SRC_16_DCV_CONF0_REG  (ISP_DPE_BASE + 0x240)
//#define DVS_PD_SRC_17_DCV_CONF1_REG  (ISP_DPE_BASE + 0x244)
//#define DVS_PD_SRC_18_DCV_CONF2_REG  (ISP_DPE_BASE + 0x248)
//#define DVS_PD_SRC_19_DCV_CONF3_REG  (ISP_DPE_BASE + 0x24C)
#define DVS_CTRL_RESERVED_REG        (ISP_DPE_BASE + 0x2F8)
#define DVS_CTRL_ATPG_REG            (ISP_DPE_BASE + 0x2FC)
#define DVS_ME_00_REG                (ISP_DPE_BASE + 0x300)
#define DVS_ME_01_REG                (ISP_DPE_BASE + 0x304)
#define DVS_ME_02_REG                (ISP_DPE_BASE + 0x308)
#define DVS_ME_03_REG                (ISP_DPE_BASE + 0x30C)
#define DVS_ME_04_REG                (ISP_DPE_BASE + 0x310)
//#define DVS_ME_05_REG                (ISP_DPE_BASE + 0x314)
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
#define DVS_ME_37_REG                (ISP_DPE_BASE + 0x394)
#define DVS_ME_38_REG                (ISP_DPE_BASE + 0x398)
#define DVS_ME_39_REG                (ISP_DPE_BASE + 0x39C)
//#define DVS_STATUS_00_REG            (ISP_DPE_BASE + 0x3E0)
//#define DVS_STATUS_01_REG            (ISP_DPE_BASE + 0x3E4)
//#define DVS_STATUS_02_REG            (ISP_DPE_BASE + 0x3E8)
//#define DVS_STATUS_03_REG            (ISP_DPE_BASE + 0x3EC)
#define DVS_DEBUG_REG                (ISP_DPE_BASE + 0x3F4)
#define DVS_ME_RESERVED_REG          (ISP_DPE_BASE + 0x3F8)
#define DVS_ME_ATPG_REG              (ISP_DPE_BASE + 0x3FC)
#define DVS_ME_40_REG                (ISP_DPE_BASE + 0x400)
#define DVS_ME_41_REG                (ISP_DPE_BASE + 0x404)
#define DVS_OCC_PQ_0_REG             (ISP_DPE_BASE + 0x3A0)
#define DVS_OCC_PQ_1_REG             (ISP_DPE_BASE + 0x3A4)
#define DVS_OCC_PQ_2_REG             (ISP_DPE_BASE + 0x3A8)
#define DVS_OCC_PQ_3_REG             (ISP_DPE_BASE + 0x3AC)
#define DVS_OCC_PQ_4_REG             (ISP_DPE_BASE + 0x3B0)
#define DVS_OCC_PQ_5_REG             (ISP_DPE_BASE + 0x3B4)
#define DVS_OCC_PQ_6_REG             (ISP_DPE_BASE + 0x3B8)
#define DVS_OCC_PQ_7_REG             (ISP_DPE_BASE + 0x3BC)
#define DVS_OCC_PQ_8_REG             (ISP_DPE_BASE + 0x3C0)
#define DVS_OCC_PQ_9_REG             (ISP_DPE_BASE + 0x3C4)
#define DVS_OCC_PQ_10_REG            (ISP_DPE_BASE + 0x3C8)
#define DVS_OCC_PQ_11_REG            (ISP_DPE_BASE + 0x3CC)
#define DVS_OCC_ATPG_REG             (ISP_DPE_BASE + 0x3D0)
#define DVP_CTRL00_REG               (ISP_DPE_BASE + 0x800)
#define DVP_CTRL01_REG               (ISP_DPE_BASE + 0x804)
#define DVP_CTRL02_REG               (ISP_DPE_BASE + 0x808)
#define DVP_CTRL03_REG               (ISP_DPE_BASE + 0x80C)
#define DVP_CTRL04_REG               (ISP_DPE_BASE + 0x810)
//#define DVP_CTRL05_REG               (ISP_DPE_BASE + 0x814)
//#define DVP_CTRL06_REG               (ISP_DPE_BASE + 0x818)
#define DVP_CTRL07_REG               (ISP_DPE_BASE + 0x81C)
#define DVP_IRQ_00_REG               (ISP_DPE_BASE + 0x840)
#define DVP_IRQ_01_REG               (ISP_DPE_BASE + 0x844)
#define DVP_CTRL_STATUS0_REG         (ISP_DPE_BASE + 0x850)
#define DVP_CTRL_STATUS1_REG         (ISP_DPE_BASE + 0x854)
#define DVP_CTRL_STATUS2_REG         (ISP_DPE_BASE + 0x858)
#define DVP_IRQ_STATUS_REG           (ISP_DPE_BASE + 0x85C)
#define DVP_FRM_STATUS0_REG          (ISP_DPE_BASE + 0x860)
//#define DVP_FRM_STATUS1_REG          (ISP_DPE_BASE + 0x864)
#define DVP_FRM_STATUS2_REG          (ISP_DPE_BASE + 0x868)
//#define DVP_FRM_STATUS3_REG          (ISP_DPE_BASE + 0x86C)
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
//#define DVP_CORE_16_REG              (ISP_DPE_BASE + 0x940)
//#define DVP_CORE_17_REG              (ISP_DPE_BASE + 0x944)
//#define DVP_CORE_18_REG              (ISP_DPE_BASE + 0x948)
//#define DVP_CORE_19_REG              (ISP_DPE_BASE + 0x94C)
//#define DVP_CORE_20_REG              (ISP_DPE_BASE + 0x950)
//#define DVP_CORE_21_REG              (ISP_DPE_BASE + 0x954)
//#define DVP_CORE_22_REG              (ISP_DPE_BASE + 0x958)
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
	unsigned int p;
	unsigned long flags;

	p = ProcessID % IRQ_USER_NUM_MAX;
	/*  */
	spin_lock_irqsave(&(DPEInfo.SpinLockIrq[type]), flags);
	if (stus & DPE_INT_ST) {
		ret = ((DPEInfo.IrqInfo.DpeIrqCnt[p] > 0) &&
		       (DPEInfo.IrqInfo.ProcessID[p] == ProcessID));
	} else {
		LOG_ERR("EWIRQ,type:%d,u:%d,stat:%d,wReq:%d,PID:0x%x\n",
			type, userNumber, stus, p, ProcessID);
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
static bool dpe_get_dma_buffer(struct tee_mmu *mmu, int fd)
{
	struct dma_buf *buf;

	LOG_INF("get_dma_buffer_fd= %d\n", fd);
	if (fd < 0)
		return false;
	buf = dma_buf_get(fd);
	if (IS_ERR(buf))
		return false;
	LOG_INF("buf_addr = %x\n", buf);
	mmu->dma_buf = buf;
	mmu->attach = dma_buf_attach(mmu->dma_buf, gdev);

	LOG_INF("mmu->attach = %x\n", mmu->attach);

	if (IS_ERR(mmu->attach))
		goto err_attach;

	mmu->sgt = dma_buf_map_attachment(mmu->attach,
	DMA_BIDIRECTIONAL);

	LOG_INF("mmu->sgt = %x\n", mmu->sgt);

	if (IS_ERR(mmu->sgt))
		goto err_map;

	return true;
err_map:
		dma_buf_detach(mmu->dma_buf, mmu->attach);
		LOG_INF("err_map!\n");
err_attach:
	LOG_INF("err_attach!\n");
	dma_buf_put(mmu->dma_buf);
	return false;
}
/**************************************************************
 *
 **************************************************************/
signed int dpe_enque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt, t, ucnt;
	unsigned int pd_frame_num = 0;
	unsigned int Dpe_InBuf_SrcImg_Y_L = 0, Dpe_InBuf_SrcImg_Y_R = 0;
	unsigned int Dpe_InBuf_ValidMap_L = 0, Dpe_InBuf_ValidMap_R = 0;
	unsigned int Dpe_OutBuf_OCC = 0, Dpe_OutBuf_OCC_Ext = 0;
	//!unsigned int Dpe_OutBuf_CONF = 0;
//!ISP 7.0
	unsigned int Dpe_InBuf_SrcImg_Y_L_Pre = 0;
	unsigned int Dpe_InBuf_SrcImg_Y_R_Pre = 0;
	unsigned int Dpe_search_range[TILE_WITH_NUM] = {64, 51, 38};
	unsigned int search_cnt = 0;
	unsigned int DPE_P4_EN = 0;
	unsigned int P4_temp = 0;
	struct tee_mmu	mmu;
#ifdef IOVA_TO_PA
	uint64_t iova_temp = 0x200000000;
	uint64_t pgpa;
	struct iommu_domain *domain;
#endif
	unsigned int success = 0;
//!
	/*TODO: define engine request struct */
	struct DPE_Request *_req;
	struct DPE_Config *pDpeConfig;

	_req = (struct DPE_Request *) req;
	if (frames == NULL || _req == NULL)
		return -1;
#ifdef IOVA_TO_PA
	domain = iommu_get_domain_for_dev(gdev);
#endif

	LOG_INF("dpe_enque star\n");
if (_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.is_pd_mode == 1) {
	//mmu = kzalloc(sizeof(*mmu), GFP_KERNEL);
	DVS_mmu = kzalloc(sizeof(struct tee_mmu) * 10, GFP_KERNEL);
	if (!DVS_mmu)
		return -1;

	P4_temp = (_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.TuningBuf_ME.DVS_ME_28);
	DPE_P4_EN = (((P4_temp) & 0x400) >> 10);

	LOG_INF("Get tile buffer\n");

	success = dpe_get_dma_buffer(&mmu,
	_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_L_fd);
	if (success) {
		_req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_L = (sg_dma_address(mmu.sgt->sgl) +
		(_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_L_Ofs));
		#ifdef IOVA_TO_PA
		iova_temp = Dpe_InBuf_SrcImg_Y_L | iova_temp;
		pgpa = iommu_iova_to_phys(domain, iova_temp);
		iova_temp = 0x200000000;
		#endif
		LOG_INF("Dpe_InBuf_SrcImg_Y_L = %lx\n",
		_req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_L);
		LOG_INF("=========================================================\n");
	} else {
		LOG_INF("get Dpe_InBuf_SrcImg_Y_L_fd fail\n");
	}

	LOG_INF("Dpe_InBuf_SrcImg_Y_R fd= %x\n",
	_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_fd);

	success = dpe_get_dma_buffer(&mmu,
	_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_fd);
	LOG_INF("Dpe_InBuf_SrcImg_Y_R offset = %x ,R_va: %x\n",
	_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_Ofs, mmu.sgt->sgl);
	if (success) {
		_req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_R = (sg_dma_address(mmu.sgt->sgl) +
		(_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_Ofs));
		#ifdef IOVA_TO_PA
		iova_temp = _req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_R | iova_temp;
		pgpa = iommu_iova_to_phys(domain, iova_temp);
		iova_temp = 0x200000000;
		#endif
		LOG_INF("Dpe_InBuf_SrcImg_Y_R = %lx\n",
			_req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_R);
		LOG_INF("=========================================================\n");
	} else {
		LOG_INF("get Dpe_InBuf_SrcImg_Y_R fail\n");
	}
	success = dpe_get_dma_buffer(&mmu,
	_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_ValidMap_L_fd);
	if (success) {
		_req->m_pDpeConfig[ucnt].Dpe_InBuf_ValidMap_L = (sg_dma_address(mmu.sgt->sgl) +
		(_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_ValidMap_L_Ofs));
		LOG_INF("Dpe_InBuf_ValidMap_L = %x\n",
			_req->m_pDpeConfig[ucnt].Dpe_InBuf_ValidMap_L);
		LOG_INF("=========================================================\n");
	} else {
		LOG_INF("get Dpe_InBuf_ValidMap_L fail\n");
	}

	success = dpe_get_dma_buffer(&mmu,
	_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_ValidMap_R_fd);
	if (success) {
		_req->m_pDpeConfig[ucnt].Dpe_InBuf_ValidMap_R = (sg_dma_address(mmu.sgt->sgl) +
		(_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_ValidMap_R_Ofs));
		LOG_INF("Dpe_InBuf_ValidMap_R = %x\n",
			_req->m_pDpeConfig[ucnt].Dpe_InBuf_ValidMap_R);
	LOG_INF("=========================================================\n");
	} else {
		LOG_INF("get Dpe_InBuf_ValidMap_R fail\n");
	}
	success = dpe_get_dma_buffer(&mmu,
	_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_OutBuf_OCC_fd);
	if (success) {
		_req->m_pDpeConfig[ucnt].Dpe_OutBuf_OCC = (sg_dma_address(mmu.sgt->sgl) +
		(_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_OutBuf_OCC_Ofs));
		#ifdef IOVA_TO_PA
		iova_temp = _req->m_pDpeConfig[ucnt].Dpe_OutBuf_OCC | iova_temp;
		pgpa = iommu_iova_to_phys(domain, iova_temp);
		iova_temp = 0x200000000;
		#endif
		LOG_INF("Dpe_OutBuf_OCC = %x\n",
			_req->m_pDpeConfig[ucnt].Dpe_OutBuf_OCC);
		LOG_INF("=========================================================\n");
	} else {
		LOG_INF("get Dpe_OutBuf_OCC fail\n");
	}

	success = dpe_get_dma_buffer(&mmu,
	_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_OutBuf_OCC_Ext_fd);
	if (success) {
		_req->m_pDpeConfig[ucnt].Dpe_OutBuf_OCC_Ext = (sg_dma_address(mmu.sgt->sgl) +
		(_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_OutBuf_OCC_Ext_Ofs));
		LOG_INF("Dpe_OutBuf_OCC_Ext = %x\n",
			_req->m_pDpeConfig[ucnt].Dpe_OutBuf_OCC_Ext);
		LOG_INF("=========================================================\n");
	} else {
		LOG_INF("get Dpe_OutBuf_OCC_Ext fail\n");
	}

	if (DPE_P4_EN == 1) {
		success = dpe_get_dma_buffer(&mmu,
		_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_L_Pre_fd);
		if (success) {
			_req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_L_Pre =
			(sg_dma_address(mmu.sgt->sgl) +
			(_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_L_Pre_Ofs));
			LOG_INF("Dpe_InBuf_SrcImg_Y_L_Pre = %x\n",
				_req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_L_Pre);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_InBuf_SrcImg_Y_L_Pre fail\n");
		}

		success = dpe_get_dma_buffer(&mmu,
		_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_Pre_fd);
		if (success) {
			_req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_R_Pre =
			(sg_dma_address(mmu.sgt->sgl) +
			(_req->m_pDpeConfig[ucnt].DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_Pre_Ofs));
			LOG_INF("Dpe_InBuf_SrcImg_R_L_Pre = %x\n",
				_req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_R_Pre);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_InBuf_SrcImg_R_L_Pre fail\n");
		}
	}
}
//------------
	if (pDpeConfig->Dpe_DVSSettings.dram_out_pitch_en == 0 &&
		pDpeConfig->Dpe_DVSSettings.occ_width > 640)
		LOG_INF("Dram_out_pitch_en not turn on, but occwidth over 640\n");

	if (pDpeConfig->Dpe_DVSSettings.dram_out_pitch_en == 1 &&
		pDpeConfig->Dpe_DVSSettings.occ_width < 640) {
		LOG_INF("Dram_out_pitch_en turn on, but occwidth is smaller than 640\n");
		return 0;
	}
	/*TODO: m_ReqNum is FrmNum; FIFO only thus f starts from 0 */
	ucnt = 0;
	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		if (_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.is_pd_mode) {
			//LOG_INF("start tile\n");
			pd_frame_num =
			_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.pd_frame_num;
			LOG_INF("pd_frame_num = %d\n", pd_frame_num);
			Dpe_InBuf_SrcImg_Y_L =
			_req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_L;
			LOG_INF("Dpe_InBuf_SrcImg_Y_L = 0x%x\n",
				Dpe_InBuf_SrcImg_Y_L);
			Dpe_InBuf_SrcImg_Y_R =
			_req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_R;
			LOG_INF("Dpe_InBuf_SrcImg_Y_R = 0x%x\n",
				Dpe_InBuf_SrcImg_Y_R);
			Dpe_InBuf_ValidMap_L =
			_req->m_pDpeConfig[ucnt].Dpe_InBuf_ValidMap_L;
			LOG_INF("Dpe_InBuf_ValidMap_L = 0x%x\n",
				Dpe_InBuf_ValidMap_L);
			Dpe_InBuf_ValidMap_R =
			_req->m_pDpeConfig[ucnt].Dpe_InBuf_ValidMap_R;
			LOG_INF("Dpe_InBuf_ValidMap_R = 0x%x\n",
				Dpe_InBuf_ValidMap_R);
			Dpe_OutBuf_OCC =
			_req->m_pDpeConfig[ucnt].Dpe_OutBuf_OCC;
			LOG_INF("Dpe_OutBuf_OCC = 0x%x\n",
				Dpe_OutBuf_OCC);
			Dpe_OutBuf_OCC_Ext =
			_req->m_pDpeConfig[ucnt].Dpe_OutBuf_OCC_Ext;
			LOG_INF("Dpe_OutBuf_OCC_Ext = 0x%x\n",
				Dpe_OutBuf_OCC_Ext);


			if (DPE_P4_EN == 1) {
				Dpe_InBuf_SrcImg_Y_L_Pre =
			_	req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_L_Pre;
				LOG_INF("Dpe_InBuf_SrcImg_Y_L_Pre = 0x%x\n",
				Dpe_InBuf_SrcImg_Y_L_Pre);
				Dpe_InBuf_SrcImg_Y_R_Pre =
			_	req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_R_Pre;
				LOG_INF("Dpe_InBuf_SrcImg_Y_R_Pre = 0x%x\n",
				Dpe_InBuf_SrcImg_Y_R_Pre);
			}

		for (t = 0; t < pd_frame_num; t++) {
			/*ISP7 Tile mode*/
			_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.input_offset =
			((t) * _req->m_pDpeConfig[ucnt].Dpe_DVSSettings.occ_width) -
			(((t) == 0) ? 0 : (t == (pd_frame_num-1)) ?
			2*Dpe_search_range[search_cnt] : Dpe_search_range[search_cnt]);
			LOG_INF("tile Input_offset = %d ,t = %d\n",
				_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.input_offset, t);

			_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.occ_start_x =
			(((t) == 0) ? 0 : (t == (pd_frame_num-1)) ?
			2*Dpe_search_range[search_cnt] : Dpe_search_range[search_cnt]);
			LOG_INF("tile occ_start_x = %d\n",
				_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.occ_start_x);

			if (Dpe_InBuf_SrcImg_Y_L != 0) {
				_req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_L =
				Dpe_InBuf_SrcImg_Y_L +
				(_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.pd_st_x +
				_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.input_offset);
				 LOG_INF("SrcImg_Y_L = 0x%x,pd_st_x = 0x%x offset = 0x%x\n",
					_req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_L,
					_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.pd_st_x,
					_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.input_offset);
				}
				if (Dpe_InBuf_SrcImg_Y_R != 0) {
					_req->m_pDpeConfig[
					ucnt].Dpe_InBuf_SrcImg_Y_R =
					Dpe_InBuf_SrcImg_Y_R +
					(_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.pd_st_x +
					_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.input_offset);
					LOG_INF("tile Dpe_InBuf_SrcImg_Y_R = 0x%x\n",
						_req->m_pDpeConfig[ucnt].Dpe_InBuf_SrcImg_Y_R);
				}
				if (Dpe_InBuf_ValidMap_L != 0) {
					_req->m_pDpeConfig[
					ucnt].Dpe_InBuf_ValidMap_L =
					Dpe_InBuf_ValidMap_L +
					(_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.pd_st_x +
					_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.input_offset);
					LOG_INF("tile Dpe_InBuf_ValidMap_L = 0x%x\n",
						_req->m_pDpeConfig[ucnt].Dpe_InBuf_ValidMap_L);
				}
				if (Dpe_InBuf_ValidMap_R != 0) {
					_req->m_pDpeConfig[
					ucnt].Dpe_InBuf_ValidMap_R =
					Dpe_InBuf_ValidMap_R +
					(_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.pd_st_x +
					_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.input_offset);
					LOG_INF("tile Dpe_InBuf_ValidMap_R = 0x%x\n",
						_req->m_pDpeConfig[ucnt].Dpe_InBuf_ValidMap_R);
				}
				if (Dpe_OutBuf_OCC != 0) {
					_req->m_pDpeConfig[
					ucnt].Dpe_OutBuf_OCC =
					Dpe_OutBuf_OCC +
					(t*_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.occ_width);
					LOG_INF("tile Dpe_OutBuf_OCC = 0x%x\n",
						_req->m_pDpeConfig[ucnt].Dpe_OutBuf_OCC);
				}
				if (Dpe_OutBuf_OCC_Ext != 0) {
					_req->m_pDpeConfig[
					ucnt].Dpe_OutBuf_OCC_Ext =
					Dpe_OutBuf_OCC_Ext +
					(t*_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.occ_width);
					LOG_INF("tile Dpe_OutBuf_OCC_Ext = 0x%x\n",
						_req->m_pDpeConfig[ucnt].Dpe_OutBuf_OCC_Ext);
				}
				//!ISP7 P4 on WTA
				if (DPE_P4_EN == 1) {
					if (Dpe_InBuf_SrcImg_Y_L_Pre != 0) {
						_req->m_pDpeConfig[
						ucnt].Dpe_InBuf_SrcImg_Y_L_Pre =
						Dpe_InBuf_SrcImg_Y_L_Pre +
						(_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.pd_st_x +
						_req->m_pDpeConfig[
						ucnt].Dpe_DVSSettings.input_offset);
					}
					if (Dpe_InBuf_SrcImg_Y_R_Pre != 0) {
						_req->m_pDpeConfig[
						ucnt].Dpe_InBuf_SrcImg_Y_R_Pre =
						Dpe_InBuf_SrcImg_Y_R_Pre +
						(_req->m_pDpeConfig[ucnt].Dpe_DVSSettings.pd_st_x +
						_req->m_pDpeConfig[
						ucnt].Dpe_DVSSettings.input_offset);
					}
				}

				memcpy(frames[f+t].data,
				&_req->m_pDpeConfig[ucnt],
				sizeof(struct DPE_Config));
			}
			f += (t-1);
		} else {
			memcpy(frames[f].data, &_req->m_pDpeConfig[ucnt],
						sizeof(struct DPE_Config));
		}
		pDpeConfig = &_req->m_pDpeConfig[ucnt];
		ucnt++;

		//LOG_ERR("[%s] request queued with  frame(%d)", __func__, f);
		//LOG_DBG("[%s] DPE_CTRL_REG:0x%x!\n", __func__,
		//					pDpeConfig->DPE_CTRL);

	}
	return 0;
}
signed int dpe_deque_cb(struct frame *frames, void *req)
{
	unsigned int f, fcnt, ucnt;
	unsigned int pd_frame_num;
	struct DPE_Request *_req;
	struct DPE_Config *pDpeConfig;

	_req = (struct DPE_Request *) req;

	if (frames == NULL || _req == NULL)
		return -1;
	LOG_INF(" dpe_deque start\n");
	/*TODO: m_ReqNum is FrmNum; FIFO only thus f starts from 0 */
	ucnt = 0;
	fcnt = _req->m_ReqNum;
	for (f = 0; f < fcnt; f++) {
		pDpeConfig = (struct DPE_Config *) frames[f].data;
		if (pDpeConfig->Dpe_DVSSettings.is_pd_mode) {
			pd_frame_num = pDpeConfig->Dpe_DVSSettings.pd_frame_num;
			memcpy(&_req->m_pDpeConfig[ucnt], frames[f].data,
						sizeof(struct DPE_Config));
			f += (pd_frame_num-1);
		} else {
			memcpy(&_req->m_pDpeConfig[ucnt], frames[f].data,
						sizeof(struct DPE_Config));
		}
		pDpeConfig = &_req->m_pDpeConfig[ucnt];
		ucnt++;
		//memcpy(&_req->m_pDpeConfig[f], frames[f].data,
		//sizeof(struct DPE_Config));
		LOG_INF("[%s]request dequeued frame(%d/%d).", __func__, f,
									fcnt);
#ifdef dpe_dump_read_en
		LOG_ERR(
		"[%s] request queued with  frame(%d)", __func__, f);
		LOG_DBG(
		"[%s] DPE_CTRL_REG:0x%x!\n",
			__func__, pDpeConfig->DPE_CTRL);
#endif
	}
	_req->m_ReqNum = ucnt;
	return 0;
}
void DPE_Config_DVS(struct DPE_Config *pDpeConfig,
	struct DPE_Kernel_Config *pConfigToKernel)
{
	unsigned int frmWidth = pDpeConfig->Dpe_DVSSettings.frm_width;
	unsigned int frmHeight = pDpeConfig->Dpe_DVSSettings.frm_height;
	unsigned int L_engStartX = pDpeConfig->Dpe_DVSSettings.l_eng_start_x;
	unsigned int R_engStartX = pDpeConfig->Dpe_DVSSettings.r_eng_start_x;
	unsigned int engStartY = pDpeConfig->Dpe_DVSSettings.eng_start_y;
	unsigned int engWidth = pDpeConfig->Dpe_DVSSettings.eng_width;
	unsigned int engHeight = pDpeConfig->Dpe_DVSSettings.eng_height;
	unsigned int occWidth = pDpeConfig->Dpe_DVSSettings.occ_width;
	unsigned int occStartX = pDpeConfig->Dpe_DVSSettings.occ_start_x;
	unsigned int DVS_OUT_ADJ_Dv_En = pDpeConfig->Dpe_DVSSettings.out_adj_dv_en;
	unsigned int DVS_OUT_ADJ_Dv_WIDTH = pDpeConfig->Dpe_DVSSettings.out_adj_dv_width;
	unsigned int DVS_OUT_ADJ_Dv_HIGHT = pDpeConfig->Dpe_DVSSettings.out_adj_dv_high;



	// pitch / 16
	unsigned int pitch = pDpeConfig->Dpe_DVSSettings.dram_pxl_pitch >> 4;
	unsigned int full_tile_width = pDpeConfig->Dpe_DVSSettings.dram_out_pitch >> 4;

	DPE_P4_EN = ((pDpeConfig->Dpe_DVSSettings.TuningBuf_ME.DVS_ME_28 & 0x400)>>10);
	LOG_INF("pDpeConfig->Dpe_DVSSettings.pitch = %x , pitch = %x\n",
		pDpeConfig->Dpe_DVSSettings.dram_pxl_pitch, pitch);
	LOG_INF(" full_tile_width = 0x%x, DPE_P4_EN =0x%x\n", full_tile_width, DPE_P4_EN);

	LOG_INF(
	"DVS param: frm w/h(%d/%d), engStart X_L/X_R/Y(%d/%d/%d), eng w/h(%d/%d), occ w(%d), occ startX(%d), pitch(%d), main eye(%d), 16bit mode(%d), sbf/conf/occ_en(%d/%d/%d), Dpe_InBuf_SrcImg_Y_L: (0x%08x), Dpe_InBuf_SrcImg_Y_R(0x%08x), Dpe_InBuf_ValidMap_L(0x%08x), Dpe_InBuf_ValidMap_R(0x%08x), Dpe_OutBuf_CONF(0x%08x), Dpe_OutBuf_OCC(0x%08x)\n",
	frmWidth, frmHeight, L_engStartX, R_engStartX, engStartY,
	engWidth, engHeight, occWidth, occStartX, pitch,
	pDpeConfig->Dpe_DVSSettings.mainEyeSel, pDpeConfig->Dpe_is16BitMode,
	pDpeConfig->Dpe_DVSSettings.SubModule_EN.sbf_en,
	pDpeConfig->Dpe_DVSSettings.SubModule_EN.conf_en,
	pDpeConfig->Dpe_DVSSettings.SubModule_EN.occ_en,
	pDpeConfig->Dpe_InBuf_SrcImg_Y_L, pDpeConfig->Dpe_InBuf_SrcImg_Y_R,
	pDpeConfig->Dpe_InBuf_ValidMap_L, pDpeConfig->Dpe_InBuf_ValidMap_R,
	pDpeConfig->Dpe_OutBuf_CONF, pDpeConfig->Dpe_OutBuf_OCC);
	LOG_INF(
	"Dpe_InBuf_SrcImg_Y_L: (0x%lx), Dpe_InBuf_SrcImg_Y_R(0x%lx), Dpe_InBuf_ValidMap_L(0x%lx), Dpe_InBuf_ValidMap_R(0x%lx), Dpe_OutBuf_CONF(0x%lx), Dpe_OutBuf_OCC(0x%lx)\n",
	pDpeConfig->Dpe_InBuf_SrcImg_Y_L,
	pDpeConfig->Dpe_InBuf_SrcImg_Y_R,
	pDpeConfig->Dpe_InBuf_ValidMap_L, pDpeConfig->Dpe_InBuf_ValidMap_R,
	pDpeConfig->Dpe_OutBuf_CONF, pDpeConfig->Dpe_OutBuf_OCC);
	if ((frmWidth % 16 != 0))
		LOG_ERR("frame width is not 16 byte align w(%d)\n", frmWidth);
	if ((frmHeight % 2 != 0))
		LOG_ERR("frame height is not 2 byte align h(%d)\n", frmHeight);
	if ((occWidth % 16 != 0))
		LOG_ERR("occ width is not 16 byte align w(%d)\n", occWidth);
	if (L_engStartX < R_engStartX) {
		LOG_ERR("L_engStartX(%d) < R_engStartX(%d)\n",
		L_engStartX, R_engStartX);
	}
	if (pDpeConfig->Dpe_engineSelect == MODE_DVS_DVP_BOTH) {
		pConfigToKernel->DVS_CTRL00 =
		((pDpeConfig->Dpe_DVSSettings.Iteration.y_IterTimes & 0x3) << 0) |
		((pDpeConfig->Dpe_DVSSettings.Iteration.y_IterStartDirect_0 & 0x1) << 2) |
		((pDpeConfig->Dpe_DVSSettings.Iteration.y_IterStartDirect_1 & 0x1) << 3) |
		((pDpeConfig->Dpe_DVSSettings.Iteration.x_IterStartDirect_0 & 0x1) << 8) |
		((pDpeConfig->Dpe_DVSSettings.Iteration.x_IterStartDirect_1 & 0x1) << 9) |
		(0xD << 10) |
		(0x0 << 15) |
		((pDpeConfig->Dpe_DVSSettings.SubModule_EN.sbf_en & 0x1) << 16) |
		((pDpeConfig->Dpe_DVSSettings.SubModule_EN.occ_en & 0x1) << 18) |
		((0x0 & 0x3) << 20) | // c_dvp_cur_frm
		((0x0 & 0x3) << 22) | // c_dvs_cur_frm
		((0x1 & 0x3) << 24) | // c_dvs_prev_frm //!ISP7
		((0x3 & 0x3) << 27) | // c_dvp_trig_en, c_dvs_trig_en
		((0x2 & 0x3) << 29)	| // c_dpe_fw_trig, c_dpe_fw_trig_en
		((0x1 & 0x1) << 31); // c_dvs_en
	} else if (pDpeConfig->Dpe_engineSelect == MODE_DVS_ONLY) {
		pConfigToKernel->DVS_CTRL00 =
		((pDpeConfig->Dpe_DVSSettings.Iteration.y_IterTimes & 0x3) << 0) |
		((pDpeConfig->Dpe_DVSSettings.Iteration.y_IterStartDirect_0 & 0x1) << 2) |
		((pDpeConfig->Dpe_DVSSettings.Iteration.y_IterStartDirect_1 & 0x1) << 3) |
		((pDpeConfig->Dpe_DVSSettings.Iteration.x_IterStartDirect_0 & 0x1) << 8) |
		((pDpeConfig->Dpe_DVSSettings.Iteration.x_IterStartDirect_1 & 0x1) << 9) |
		(0xD << 10) |
		(0x0 << 15) |
		((pDpeConfig->Dpe_DVSSettings.SubModule_EN.sbf_en & 0x1) << 16) |
		//((pDpeConfig->Dpe_DVSSettings.SubModule_EN.conf_en & 0x1) << 17) |
		((pDpeConfig->Dpe_DVSSettings.SubModule_EN.occ_en & 0x1) << 18) |
		((0x0 & 0x3) << 20) | // c_dvp_cur_frm
		((0x0 & 0x3) << 22) | // c_dvs_cur_frm
		((0x1 & 0x3) << 24) | // c_dvs_prev_frm
		((0x2 & 0x3) << 27) | // c_dvp_trig_en, c_dvs_trig_en
		((0x2 & 0x3) << 29) | // c_dpe_fw_trig, c_dpe_fw_trig_en
		((0x1 & 0x1) << 31); // c_dvs_en
	}
	//!ISP7 Tile mode
	if (pDpeConfig->Dpe_DVSSettings.is_pd_mode) {
		pConfigToKernel->DVS_DRAM_PITCH =
		((pitch) & 0x3FF) | ((pitch & 0x3FF) << 10) |
		((pDpeConfig->Dpe_DVSSettings.dram_out_pitch_en & 0x01) << 31) |
		((full_tile_width & 0x3FF) << 20); //!ISP7
		pConfigToKernel->DVS_SRC_00 =
		((pDpeConfig->Dpe_is16BitMode & 0x1) << 0) |
		((pDpeConfig->Dpe_DVSSettings.mainEyeSel & 0x1) << 1) |
		((0x0 & 0x1) << 8) | // c_vmap_in_pxl
		((0x0 & 0x1) << 9); // c_vmap_all_vld
	} else {
		pConfigToKernel->DVS_DRAM_PITCH =
		((pitch) & 0x3FF) | ((pitch & 0x3FF) << 10) |
		((pDpeConfig->Dpe_DVSSettings.dram_out_pitch_en & 0x01) << 31) |
		((full_tile_width & 0x3FF) << 20); //!ISP7

		pConfigToKernel->DVS_SRC_00 =
		((pDpeConfig->Dpe_is16BitMode & 0x1) << 0) |
		((pDpeConfig->Dpe_DVSSettings.mainEyeSel & 0x1) << 1) |
		((0x0 & 0x1) << 8) | // c_vmap_in_pxl
		((0x0 & 0x1) << 9); // c_vmap_all_vld
	}

	pConfigToKernel->DVS_SRC_01 =
	((frmHeight & 0x7FF) << 0) | ((frmWidth & 0x7FF) << 12);
	pConfigToKernel->DVS_SRC_02 =
	((engHeight & 0x7FF) << 0) | ((engWidth & 0x7FF) << 12);
	pConfigToKernel->DVS_SRC_03 =
	((R_engStartX & 0x7FF) << 0) |
	((L_engStartX & 0x7FF) << 12) |
	((engStartY & 0xFF) << 24);
	pConfigToKernel->DVS_SRC_04 =
	((occWidth & 0x7FF) << 0) | ((occStartX & 0x7FF) << 12);
	LOG_INF("DVS_SRC_04 =(0x%lx)", pConfigToKernel->DVS_SRC_04);
	if (pDpeConfig->Dpe_InBuf_SrcImg_Y_L != 0x0) {
		pConfigToKernel->DVS_SRC_05_L_FRM0 =
		pDpeConfig->Dpe_InBuf_SrcImg_Y_L;
	} else
		LOG_ERR("No Left Src Image Y!\n");

	LOG_INF("DVS_SRC_05_L_FRM0 =(0x%lx)",
	pConfigToKernel->DVS_SRC_05_L_FRM0);

	if (pDpeConfig->Dpe_InBuf_SrcImg_Y_R != 0x0) {
		pConfigToKernel->DVS_SRC_09_R_FRM0 =
		pDpeConfig->Dpe_InBuf_SrcImg_Y_R;
	} else
		LOG_ERR("No Right Src Image Y!\n");

	LOG_INF("DVS_SRC_09_R_FRM0 =(0x%lx)",
	pConfigToKernel->DVS_SRC_09_R_FRM0);

	if (DPE_P4_EN == 1) {
		LOG_INF("dpe_p4_enable == 1\n");
		if (pDpeConfig->Dpe_InBuf_SrcImg_Y_L_Pre != 0x0) {
			pConfigToKernel->DVS_SRC_06_L_FRM1 =
			pDpeConfig->Dpe_InBuf_SrcImg_Y_L_Pre;
		} else
			LOG_ERR("No Pre Left Src Image Y!\n");

		if (pDpeConfig->Dpe_InBuf_SrcImg_Y_R_Pre != 0x0) {
			pConfigToKernel->DVS_SRC_10_R_FRM1 =
			pDpeConfig->Dpe_InBuf_SrcImg_Y_R_Pre;
		} else
			LOG_ERR("No Right Src Image Y!\n");
		if (pDpeConfig->Dpe_InBuf_P4_L_DV != 0x0) {
			pConfigToKernel->DVS_SRC_21_P4_L_DV_ADR =
			pDpeConfig->Dpe_InBuf_P4_L_DV;
		} else
			LOG_ERR("No DVS DVS_SRC_21_P4_L_DV Buffer!\n");
		if (pDpeConfig->Dpe_InBuf_P4_R_DV != 0x0) {
			pConfigToKernel->DVS_SRC_34_P4_R_DV_ADR =
			pDpeConfig->Dpe_InBuf_P4_R_DV;
		} else
			LOG_ERR("No DVS DVS_SRC_34_P4_R_DV Buffer!\n");

		#ifdef KERNEL_DMA_BUFFER
		pConfigToKernel->DVS_SRC_21_P4_L_DV_ADR =
		//((uintptr_t)g_dpewb_dvme_int_Buffer_pa & 0xffffffff);
		pDpeConfig->Dpe_InBuf_P4_L_DV;
		#else
		if (pDpeConfig->DVS_SRC_21_P4_L_DV_ADR != 0x0) {
			pConfigToKernel->DVS_SRC_21_P4_L_DV_ADR =
			pDpeConfig->DVS_SRC_21_P4_L_DV_ADR;
		} else
			LOG_ERR("No DVS DVS_SRC_21_INTER_MEDV Buffer!\n");

		LOG_INF("DVS_SRC_21_P4_L_DV_ADR =(0x%lx)",
		pConfigToKernel->DVS_SRC_21_P4_L_DV_ADR);
		#endif

		#ifdef KERNEL_DMA_BUFFER
		pConfigToKernel->DVS_SRC_34_P4_R_DV_ADR =
		//((uintptr_t)g_dpewb_cost_int_Buffer_pa & 0xffffffff);
		pDpeConfig->Dpe_InBuf_P4_R_DV;
		#else
		if (pDpeConfig->DVS_SRC_34_P4_R_DV_ADR != 0x0) {
			pConfigToKernel->DVS_SRC_34_P4_R_DV_ADR =
			pDpeConfig->DVS_SRC_34_P4_R_DV_ADR;
		} else
			LOG_ERR("No DVS DVS_SRC_34_DCV_L_FRM0 Buffer!\n");
		#endif
		LOG_INF("DVS_SRC_34_P4_R_DV_ADR =(0x%lx)",
		pConfigToKernel->DVS_SRC_34_P4_R_DV_ADR);

	}

	if (pDpeConfig->Dpe_InBuf_ValidMap_L != 0x0) {
		pConfigToKernel->DVS_SRC_13_L_VMAP0 =
		pDpeConfig->Dpe_InBuf_ValidMap_L;
		LOG_INF("DVS_SRC_13_L_VMAP0 =(0x%lx)\n",
		pConfigToKernel->DVS_SRC_13_L_VMAP0);
	} else
		LOG_ERR("No Left Valid Map!\n");
	if (pDpeConfig->Dpe_InBuf_ValidMap_R != 0x0) {
		pConfigToKernel->DVS_SRC_17_R_VMAP0 =
		pDpeConfig->Dpe_InBuf_ValidMap_R;
		LOG_INF("DVS_SRC_17_R_VMAP0 =(0x%lx)\n",
		pConfigToKernel->DVS_SRC_17_R_VMAP0);
	} else
		LOG_ERR("No Right Valid Map!\n");

	LOG_INF("DVS_OUT_ADJ_Dv_HIGHT =(0x%lx),
	DVS_OUT_ADJ_Dv_WIDTH=(0x%lx),DVS_OUT_ADJ_Dv_En=(0x%lx)\n",
	DVS_OUT_ADJ_Dv_HIGHT, DVS_OUT_ADJ_Dv_WIDTH, DVS_OUT_ADJ_Dv_En);

	//!ISP7 DVS NN Down-Sample
	pConfigToKernel->DVS_SRC_46 =
	((DVS_OUT_ADJ_Dv_En & 0x1) << 23)	|
	((DVS_OUT_ADJ_Dv_WIDTH & 0x7FF) << 12) |
	((DVS_OUT_ADJ_Dv_HIGHT & 0x7FF) << 0);
	//!
	LOG_INF("DVS_SRC_46 =(0x%lx)",
		pConfigToKernel->DVS_SRC_46);

	pConfigToKernel->DVS_CTRL_ATPG = 0x80000000;
	if (pDpeConfig->Dpe_OutBuf_OCC != 0x0) {
		pConfigToKernel->DVS_SRC_26_OCCDV0 =
		pDpeConfig->Dpe_OutBuf_OCC;
		LOG_INF("DVS_SRC_26_OCCDV0 =(0x%lx)\n",
		pConfigToKernel->DVS_SRC_26_OCCDV0);
	} else
		LOG_ERR("No DVS OCC Output Buffer!\n");

	if (pDpeConfig->Dpe_OutBuf_OCC_Ext != 0x0) {
		pConfigToKernel->DVS_SRC_42_OCCDV_EXT0 =
		pDpeConfig->Dpe_OutBuf_OCC_Ext;
		LOG_INF("DVS_SRC_42_OCCDV_EXT0 =(0x%lx)",
			pConfigToKernel->DVS_SRC_42_OCCDV_EXT0);
	} else
		LOG_ERR("No DVS Ext Output Buffer!\n");

	memcpy(&pConfigToKernel->DVS_ME_00,
	&pDpeConfig->Dpe_DVSSettings.TuningBuf_ME,
	sizeof(pDpeConfig->Dpe_DVSSettings.TuningBuf_ME));
	memcpy(&pConfigToKernel->DVS_OCC_PQ_0,
	&pDpeConfig->Dpe_DVSSettings.TuningBuf_OCC,
	sizeof(pDpeConfig->Dpe_DVSSettings.TuningBuf_OCC));

}
void DPE_Config_DVP(struct DPE_Config *pDpeConfig,
	struct DPE_Kernel_Config *pConfigToKernel)
{
	unsigned int frmWidth, frmHeight, engWidth, engHeight;
	unsigned int occWidth, occHeight;
	unsigned int engStartX, engStartY, occStartX, occStartY, pitch;
	unsigned int engStart_offset_Y, engStart_offset_C;
	unsigned int DVP_ASF_DV16b_EN = pDpeConfig->Dpe_is16BitMode;
	//unsigned int DVP_ASF_DV16b_EN = 0;
	unsigned int DVP_ASF_CONF_EN = pDpeConfig->Dpe_DVPSettings.SubModule_EN.asf_conf_en;
	unsigned int DVP_ASF_DEPTH_MODE = pDpeConfig->Dpe_DVPSettings.SubModule_EN.asf_depth_mode;

	engStartX = pDpeConfig->Dpe_DVPSettings.eng_start_x;
	engStartY = pDpeConfig->Dpe_DVPSettings.eng_start_y;
	frmWidth = pDpeConfig->Dpe_DVPSettings.frm_width - engStartX;
	frmHeight = pDpeConfig->Dpe_DVPSettings.frm_height - (engStartY * 2);
	engWidth = pDpeConfig->Dpe_DVPSettings.frm_width - (engStartX * 2);
	engHeight = frmHeight;
	occStartX = engStartX;
	occStartY = 0;
	occWidth = engWidth;
	occHeight = engHeight;

	DPE_is16BitMode = pDpeConfig->Dpe_is16BitMode;
	// pitch = frame width / 16
	pitch = ALIGN16(pDpeConfig->Dpe_DVPSettings.frm_width) >> 4;
	engStart_offset_Y = engStartY * (pitch << 4);
	engStart_offset_C = engStart_offset_Y >> 1;
	LOG_INF(
	"DVP param: frm w/h(%d/%d), engstart X/Y(%d/%d), eng w/h(%d/%d), occ w/h(%d/%d), occstart X/Y(%d/%d), pitch(%d), eng start offset Y/C(0x%x/0x%x), Dpe_InBuf_SrcImg_Y: (0x%08x), Dpe_InBuf_SrcImg_C(0x%08x), Dpe_InBuf_OCC(0x%08x), Dpe_OutBuf_CRM(0x%08x), Dpe_OutBuf_ASF_RD(0x%08x), Dpe_OutBuf_ASF_HF(0x%08x), DVP_SRC_18_ASF_RMDV(0x%lx)\n",
	frmWidth, frmHeight, engStartX, engStartY, engWidth, engHeight,
	occWidth, occHeight, occStartX, occStartY, pitch,
	engStart_offset_Y, engStart_offset_C,
	pDpeConfig->Dpe_InBuf_SrcImg_Y, pDpeConfig->Dpe_InBuf_SrcImg_C,
	pDpeConfig->Dpe_InBuf_OCC, pDpeConfig->Dpe_OutBuf_CRM,
	pDpeConfig->Dpe_OutBuf_ASF_RD, pDpeConfig->Dpe_OutBuf_ASF_HF,
	((uintptr_t)g_dpewb_asfrm_Buffer_pa & 0xffffffff));

	if ((occWidth % 16 != 0))
		LOG_ERR("occ width is not 16 byte align w (%d)\n", occWidth);

	if (DVP_ASF_DV16b_EN &&
		pDpeConfig->Dpe_DVPSettings.SubModule_EN.wmf_hf_en)
		LOG_ERR("WMF should not enable in 16 bit mode\n");

	if (DVP_ASF_DV16b_EN && DVP_ASF_CONF_EN)
		LOG_ERR("ASF CONF should not enable in 16 bit mode\n");


	// If hf rounds is odd, nb_rounds can't use.
if (pDpeConfig->Dpe_DVPSettings.SubModule_EN.asf_hf_rounds % 2)
	pDpeConfig->Dpe_DVPSettings.SubModule_EN.asf_nb_rounds = 0;

	pConfigToKernel->DVP_CTRL04 =
	(pDpeConfig->Dpe_DVPSettings.SubModule_EN.asf_crm_en << 0) |
	(pDpeConfig->Dpe_DVPSettings.SubModule_EN.asf_rm_en << 1) |
	(pDpeConfig->Dpe_DVPSettings.SubModule_EN.asf_rd_en << 2) |
	(pDpeConfig->Dpe_DVPSettings.SubModule_EN.asf_hf_en << 3) |
	(pDpeConfig->Dpe_DVPSettings.SubModule_EN.wmf_hf_en << 4) |
	(pDpeConfig->Dpe_DVPSettings.SubModule_EN.wmf_filt_en << 5) |
	((pDpeConfig->Dpe_DVPSettings.SubModule_EN.asf_hf_rounds & (0xF)) << 8) |
	((pDpeConfig->Dpe_DVPSettings.SubModule_EN.asf_nb_rounds & (0x3)) << 12) |
	((pDpeConfig->Dpe_DVPSettings.SubModule_EN.wmf_filt_rounds & (0x3)) << 16) |
	(DVP_ASF_DV16b_EN << 18) | //!ISP7.0
	(pDpeConfig->Dpe_DVPSettings.SubModule_EN.asf_recursive_en << 19) |
	(DVP_ASF_CONF_EN << 21)|//!ISP7.0
	(DVP_ASF_DEPTH_MODE << 22);

	pConfigToKernel->DVP_DRAM_PITCH =
	((pitch & 0x7F) << 4) | ((pitch & 0x7F) << 20);
	pConfigToKernel->DVP_SRC_00 =
	((engHeight & 0x3FF) << 0) | ((engWidth & 0x3FF) << 10) |
	((pDpeConfig->Dpe_DVPSettings.Y_only & 0x1) << 29) |
	((pDpeConfig->Dpe_DVPSettings.mainEyeSel & 0x1) << 30);
	pConfigToKernel->DVP_SRC_01 =
	((frmHeight & 0x3FF) << 0) | ((frmWidth & 0x3FF) << 10);
	pConfigToKernel->DVP_SRC_02 =
	((engStartX & 0x3FF) << 0);
	// | ((engStartY & 0x3FF) << 10); for Simulation
	pConfigToKernel->DVP_SRC_03 =
	((occHeight & 0x3FF) << 0) | ((occWidth & 0x3FF) << 10);
	pConfigToKernel->DVP_SRC_04 =
	((occStartX & 0x3FF) << 0);
	// | ((occStartY & 0x3FF) << 10); for Simulation
	if (pDpeConfig->Dpe_DVPSettings.mainEyeSel == RIGHT) {
		if (pDpeConfig->Dpe_InBuf_SrcImg_Y != 0x0) {
			pConfigToKernel->DVP_SRC_05_Y_FRM0 =
			(unsigned int)pDpeConfig->Dpe_InBuf_SrcImg_Y +
			(engStart_offset_Y);
		} else
			LOG_ERR("No DVP Right Src Image Y!\n");

	} else if (pDpeConfig->Dpe_DVPSettings.mainEyeSel == LEFT) {
		if (pDpeConfig->Dpe_InBuf_SrcImg_Y != 0x0) {
			pConfigToKernel->DVP_SRC_05_Y_FRM0 =
			(unsigned int)pDpeConfig->Dpe_InBuf_SrcImg_Y +
			(engStart_offset_Y);
		} else
			LOG_ERR("No DVP Left Src Image Y!\n");
	}
	if (pDpeConfig->Dpe_InBuf_SrcImg_C != 0x0) {
		pConfigToKernel->DVP_SRC_09_C_FRM0 =
		(unsigned int)pDpeConfig->Dpe_InBuf_SrcImg_C +
		(engStart_offset_C);
	} else
		LOG_ERR("No Src Image C!\n");
	if (pDpeConfig->Dpe_InBuf_OCC != 0x0) {
		pConfigToKernel->DVP_SRC_13_OCCDV0 =
		(unsigned int)pDpeConfig->Dpe_InBuf_OCC;
	} else
		LOG_ERR("No DVP OCC In!\n");

	LOG_INF("DVP_SRC_13_OCCDV0(0x%08x)\n",
	pConfigToKernel->DVP_SRC_13_OCCDV0);

	if (pDpeConfig->Dpe_OutBuf_CRM != 0x0) {
		pConfigToKernel->DVP_SRC_17_CRM =
		(unsigned int)pDpeConfig->Dpe_OutBuf_CRM;
	} else
		LOG_ERR("No CRM Output Buffer!\n");

	LOG_INF("DVP_SRC_17_CRM(0x%08x)\n",
	pConfigToKernel->DVP_SRC_17_CRM);

	#ifdef KERNEL_DMA_BUFFER
	LOG_INF("get kernel asf buffer\n");
	pConfigToKernel->DVP_SRC_18_ASF_RMDV =
	((uintptr_t)g_dpewb_asfrm_Buffer_pa & 0xffffffff);
	#else
	if (pDpeConfig->DVP_SRC_18_ASF_RMDV != 0x0) {
		pConfigToKernel->DVP_SRC_18_ASF_RMDV =
		pDpeConfig->DVP_SRC_18_ASF_RMDV;
	} else
		LOG_ERR("No DVS DVP_SRC_18_ASF_RMDV Buffer!\n");
	#endif
	LOG_INF("DVP_SRC_18_ASF_RMDV(0x%08x)\n",
	pConfigToKernel->DVP_SRC_18_ASF_RMDV);

	if (pDpeConfig->Dpe_OutBuf_ASF_RD != 0x0) {
		pConfigToKernel->DVP_SRC_19_ASF_RDDV =
		(unsigned int)pDpeConfig->Dpe_OutBuf_ASF_RD;
	} else
		LOG_ERR("No ASF_RD Output Buffer!\n");

	LOG_INF("Dpe_OutBuf_ASF_RD(0x%08x)\n",
	pConfigToKernel->DVP_SRC_19_ASF_RDDV);
	if (pDpeConfig->Dpe_OutBuf_ASF_HF != 0x0) {
		pConfigToKernel->DVP_SRC_20_ASF_DV0 =
		(unsigned int)pDpeConfig->Dpe_OutBuf_ASF_HF;
	} else
		LOG_ERR("No ASF Output Buffer!\n");

	LOG_INF("Dpe_OutBuf_ASF_HF(0x%08x)\n",
	pConfigToKernel->DVP_SRC_20_ASF_DV0);

	if (pDpeConfig->Dpe_is16BitMode == 0) {//for WMF
		#ifdef KERNEL_DMA_BUFFER
		pConfigToKernel->DVP_SRC_24_WMF_HFDV =
		((uintptr_t)g_dpewb_wmfhf_Buffer_pa & 0xffffffff);
		#else
		if (pDpeConfig->DVP_SRC_24_WMF_HFDV != 0x0) {
			pConfigToKernel->DVP_SRC_24_WMF_HFDV =
			pDpeConfig->DVP_SRC_24_WMF_HFDV;
		} else
			LOG_INF("No DVS DVP_SRC_24_WMF_HFDV Buffer!\n");
		#endif

		if (pDpeConfig->Dpe_OutBuf_WMF_FILT != 0x0) {
			pConfigToKernel->DVP_SRC_25_WMF_DV0 =
			(unsigned int)pDpeConfig->Dpe_OutBuf_WMF_FILT;
		} else
			LOG_INF("No WMF Output Buffer!\n");

	}
		LOG_INF("DVP_ASF_CONF_EN =%d Dpe_is16BitMode =%d DVP_ASF_DV16b_EN = =%d\n",
		DVP_ASF_CONF_EN, pDpeConfig->Dpe_is16BitMode,
		DVP_ASF_DV16b_EN);

		if (((pDpeConfig->Dpe_is16BitMode == 1) && (DVP_ASF_CONF_EN == 0)) ||
			((pDpeConfig->Dpe_is16BitMode == 0) && (DVP_ASF_CONF_EN == 1))) {

			if (pDpeConfig->Dpe_InBuf_OCC_Ext != 0x0) {
				pConfigToKernel->DVP_EXT_SRC_13_OCCDV0 =
				(unsigned int)pDpeConfig->Dpe_InBuf_OCC_Ext;
			} else
				LOG_INF("No DVP Ext OCC Input Buffer!\n");

			#ifdef KERNEL_DMA_BUFFER
			LOG_INF("get kernel asf ext buffer\n");
			pConfigToKernel->DVP_EXT_SRC_18_ASF_RMDV =
			((uintptr_t)g_dpewb_asfrmext_Buffer_pa & 0xffffffff);
			#else
			if (pDpeConfig->DVP_EXT_SRC_18_ASF_RMDV != 0x0) {
				pConfigToKernel->DVP_EXT_SRC_18_ASF_RMDV =
				pDpeConfig->DVP_EXT_SRC_18_ASF_RMDV;
			} else
				LOG_INF("No DVS DVP_EXT_SRC_18_ASF_RMDV Buffer!\n");
			#endif

			if (pDpeConfig->Dpe_OutBuf_ASF_RD_Ext != 0x0) {
				pConfigToKernel->DVP_EXT_SRC_19_ASF_RDDV =
				(unsigned int)pDpeConfig->Dpe_OutBuf_ASF_RD_Ext;
				LOG_INF("set Dpe_OutBuf_ASF_RD_Ext!\n");
			} else
				LOG_INF("No ASF_RD_EXT Output Buffer!\n");

			if (pDpeConfig->Dpe_OutBuf_ASF_HF_Ext != 0x0) {
				pConfigToKernel->DVP_EXT_SRC_20_ASF_DV0 =
				(unsigned int)pDpeConfig->Dpe_OutBuf_ASF_HF_Ext;
				LOG_INF("set Dpe_OutBuf_ASF_HF_Ext!\n");
			} else
				LOG_INF("No DVP Ext ASF Output Buffer!\n");
}
	pConfigToKernel->DVP_CTRL_ATPG = 0x80000000;
	memcpy(&pConfigToKernel->DVP_CORE_00,
	&pDpeConfig->Dpe_DVPSettings.TuningBuf_CORE,
	sizeof(pDpeConfig->Dpe_DVPSettings.TuningBuf_CORE));
}

void DPE_DumpUserSpaceReg(struct DPE_Kernel_Config *pDpeConfig)
{
	LOG_INF("=====DPE Config From User Space========\n");
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
	LOG_INF("DVS_SRC_21 = 0x%08X\n", pDpeConfig->DVS_SRC_21_P4_L_DV_ADR);
	LOG_INF("DVS_SRC_26 = 0x%08X\n", pDpeConfig->DVS_SRC_26_OCCDV0);
	LOG_INF("DVS_SRC_27 = 0x%08X\n", pDpeConfig->DVS_SRC_27_OCCDV1);
	LOG_INF("DVS_SRC_28 = 0x%08X\n", pDpeConfig->DVS_SRC_28_OCCDV2);
	LOG_INF("DVS_SRC_29 = 0x%08X\n", pDpeConfig->DVS_SRC_29_OCCDV3);
	LOG_INF("DVS_SRC_34 = 0x%08X\n", pDpeConfig->DVS_SRC_34_P4_R_DV_ADR);
	LOG_INF("DVS_SRC_42 = 0x%08X\n", pDpeConfig->DVS_SRC_42_OCCDV_EXT0);
	LOG_INF("DVS_SRC_43 = 0x%08X\n", pDpeConfig->DVS_SRC_43_OCCDV_EXT1);
	LOG_INF("DVS_SRC_44 = 0x%08X\n", pDpeConfig->DVS_SRC_44_OCCDV_EXT2);
	LOG_INF("DVS_SRC_45 = 0x%08X\n", pDpeConfig->DVS_SRC_45_OCCDV_EXT3);
	LOG_INF("DVS_SRC_46 = 0x%08X\n", pDpeConfig->DVS_SRC_46);
	LOG_INF("DVS_DRAM_SEC = 0x%08X\n", pDpeConfig->DVS_DRAM_SEC);
	LOG_INF("DVS_CTRL_ATPG = 0x%08X\n", pDpeConfig->DVS_CTRL_ATPG);
	LOG_INF("=====DPE Config end========\n");

	LOG_INF("DVS_PD_SRC_00 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_00_L_FRM0);
	LOG_INF("DVS_PD_SRC_01 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_01_L_FRM1);
	LOG_INF("DVS_PD_SRC_02 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_02_L_FRM2);
	LOG_INF("DVS_PD_SRC_03 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_03_L_FRM3);
	LOG_INF("DVS_PD_SRC_04 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_04_R_FRM0);
	LOG_INF("DVS_PD_SRC_05 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_05_R_FRM1);
	LOG_INF("DVS_PD_SRC_06 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_06_R_FRM2);
	LOG_INF("DVS_PD_SRC_07 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_07_R_FRM3);
	LOG_INF("DVS_PD_SRC_08 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_08_OCCDV0);
	LOG_INF("DVS_PD_SRC_09 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_09_OCCDV1);
	LOG_INF("DVS_PD_SRC_10 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_10_OCCDV2);
	LOG_INF("DVS_PD_SRC_11 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_11_OCCDV3);
	LOG_INF("DVS_PD_SRC_12 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_12_OCCDV_EXT0);
	LOG_INF("DVS_PD_SRC_13 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_13_OCCDV_EXT1);
	LOG_INF("DVS_PD_SRC_14 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_14_OCCDV_EXT2);
	LOG_INF("DVS_PD_SRC_15 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_15_OCCDV_EXT3);
	LOG_INF("DVS_PD_SRC_16 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_16_DCV_CONF0);
	LOG_INF("DVS_PD_SRC_17 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_17_DCV_CONF1);
	LOG_INF("DVS_PD_SRC_18 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_18_DCV_CONF2);
	LOG_INF("DVS_PD_SRC_19 = 0x%08X\n",
	pDpeConfig->DVS_PD_SRC_19_DCV_CONF3);
	LOG_INF("DVS_ME_00 = 0x%08X\n", pDpeConfig->DVS_ME_00);
	LOG_INF("DVS_ME_01 = 0x%08X\n", pDpeConfig->DVS_ME_01);
	LOG_INF("DVS_ME_02 = 0x%08X\n", pDpeConfig->DVS_ME_02);
	LOG_INF("DVS_ME_03 = 0x%08X\n", pDpeConfig->DVS_ME_03);
	LOG_INF("DVS_ME_04 = 0x%08X\n", pDpeConfig->DVS_ME_04);
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
	LOG_INF("DVS_ME_37 = 0x%08X\n", pDpeConfig->DVS_ME_37);
	LOG_INF("DVS_ME_38 = 0x%08X\n", pDpeConfig->DVS_ME_38);
	LOG_INF("DVS_ME_39 = 0x%08X\n", pDpeConfig->DVS_ME_39);
	LOG_INF("DVS_ME_37 = 0x%08X\n", pDpeConfig->DVS_ME_37);
	LOG_INF("DVS_ME_38 = 0x%08X\n", pDpeConfig->DVS_ME_38);
	LOG_INF("DVS_ME_39 = 0x%08X\n", pDpeConfig->DVS_ME_39);

	LOG_INF("DVS_DEBUG = 0x%08X\n", pDpeConfig->DVS_DEBUG);
	LOG_INF("DVS_ME_RESERVED = 0x%08X\n", pDpeConfig->DVS_ME_RESERVED);
	LOG_INF("DVS_ME_ATPG = 0x%08X\n", pDpeConfig->DVS_ME_ATPG);
	LOG_INF("DVS_ME_40 = 0x%08X\n", pDpeConfig->DVS_ME_40);
	LOG_INF("DVS_ME_41 = 0x%08X\n", pDpeConfig->DVS_ME_41);

	LOG_INF("DVS_OCC_PQ_0 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_0);
	LOG_INF("DVS_OCC_PQ_1 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_1);
	LOG_INF("DVS_OCC_PQ_2 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_2);
	LOG_INF("DVS_OCC_PQ_3 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_3);
	LOG_INF("DVS_OCC_PQ_4 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_4);
	LOG_INF("DVS_OCC_PQ_5 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_5);
	LOG_INF("DVS_OCC_PQ_6 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_6);
	LOG_INF("DVS_OCC_PQ_7 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_7);
	LOG_INF("DVS_OCC_PQ_8 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_8);
	LOG_INF("DVS_OCC_PQ_9 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_9);
	LOG_INF("DVS_OCC_PQ_10 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_10);
	LOG_INF("DVS_OCC_PQ_11 = 0x%08X\n", pDpeConfig->DVS_OCC_PQ_11);
	LOG_INF("DVS_OCC_ATPG = 0x%08X\n", pDpeConfig->DVS_OCC_ATPG);

	LOG_INF("DVP_CTRL00 = 0x%08X\n", pDpeConfig->DVP_CTRL00);
	LOG_INF("DVP_CTRL01 = 0x%08X\n", pDpeConfig->DVP_CTRL01);
	LOG_INF("DVP_CTRL02 = 0x%08X\n", pDpeConfig->DVP_CTRL02);
	LOG_INF("DVP_CTRL03 = 0x%08X\n", pDpeConfig->DVP_CTRL03);
	LOG_INF("DVP_CTRL04 = 0x%08X\n", pDpeConfig->DVP_CTRL04);
	LOG_INF("DVP_CTRL07 = 0x%08X\n", pDpeConfig->DVP_CTRL07);
	LOG_INF("DVP_IRQ_00 = 0x%08X\n", pDpeConfig->DVP_IRQ_00);
	LOG_INF("DVP_IRQ_01 = 0x%08X\n", pDpeConfig->DVP_IRQ_01);
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
	LOG_INF("DVP_CORE_16 = 0x%08X\n", pDpeConfig->DVP_CORE_16);
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
void mmu_release(struct tee_mmu *mmu, int fd_cnt)
{
	LOG_INF("fd_cnt = %d\n", fd_cnt);
	if (mmu->dma_buf) {
		LOG_INF("put mmu->dma_buf = %x\n", mmu->dma_buf);
	dma_buf_unmap_attachment(mmu->attach, mmu->sgt,
				MA_BIDIRECTIONAL);
	dma_buf_detach(mmu->dma_buf, mmu->attach);
	dma_buf_put(mmu->dma_buf);
	LOG_INF("put end\n");
	}
}
void cmdq_cb_destroy(struct cmdq_cb_data data)
{
	cmdq_pkt_destroy((struct cmdq_pkt *)data.data);
}
struct sg_table *dpe_dma_buf_map_attachment(struct dma_buf_attachment *attach,
					enum dma_data_direction direction)
{
	struct sg_table *sg_table;

	might_sleep();
	if (WARN_ON(!attach || !attach->dmabuf))
		return ERR_PTR(-EINVAL);
	sg_table = attach->dmabuf->ops->map_dma_buf(attach, direction);
	if (!sg_table)
		sg_table = ERR_PTR(-ENOMEM);
	return sg_table;
}

signed int CmdqDPEHW(struct frame *frame)
{
	struct DPE_Kernel_Config *pDpeConfig;
	struct DPE_Kernel_Config DpeConfig;
	//!struct DPE_Config *pDpeUserConfig;
	struct tee_mmu	mmu;
	//!struct mtk_dpe_dev *fd;
	//!u32 alloc_size;
#ifdef IOVA_TO_PA
	uint64_t iova_temp = 0x200000000;
	uint64_t pgpa;
	struct iommu_domain *domain;
#endif
	unsigned int success = 0;


	struct cmdq_pkt *handle;
	//struct cmdqRecStruct *handle;
	//uint64_t engineFlag = (uint64_t)(1LL << CMDQ_ENG_DPE);
#if defined(DPE_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
	unsigned int w_imgi, h_imgi, w_mvio, h_mvio, w_bvo, h_bvo;
	unsigned int dma_bandwidth, trig_num;
#endif

	if (frame == NULL || frame->data == NULL)
		return -1;
#ifdef IOVA_TO_PA
	domain = iommu_get_domain_for_dev(gdev);
#endif
	LOG_DBG("%s request sent to CMDQ driver", __func__);
	pDpeUserConfig = (struct DPE_Config *) frame->data;
	pDpeConfig = &DpeConfig;
/************** Pass User info to DPE_Kernel_Config **************/
if (pDpeUserConfig->Dpe_DVSSettings.is_pd_mode == 0) {
	//mmu = kzalloc(sizeof(*mmu), GFP_KERNEL);
	DVS_mmu = kzalloc(sizeof(struct tee_mmu) * 10, GFP_KERNEL);
	DVP_mmu = kzalloc(sizeof(struct tee_mmu) * 10, GFP_KERNEL);
	if (!DVS_mmu)
		return -1;

	if ((pDpeUserConfig->Dpe_engineSelect == MODE_DVS_ONLY) ||
		(pDpeUserConfig->Dpe_engineSelect == MODE_DVS_DVP_BOTH)) {

		LOG_INF("Dpe_InBuf_SrcImg_Y_L fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_L_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_L_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_L_fd);
		if (success) {
			pDpeUserConfig->Dpe_InBuf_SrcImg_Y_L = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_L_Ofs));
			#ifdef IOVA_TO_PA
			iova_temp = pDpeUserConfig->Dpe_InBuf_SrcImg_Y_L | iova_temp;
			pgpa = iommu_iova_to_phys(domain, iova_temp);
			LOG_INF("Dpe_InBuf_SrcImg_Y_L pgpa = %lx\n", pgpa);
			iova_temp = 0x200000000;
			LOG_INF("Dpe_InBuf_SrcImg_Y_L = %lx\n",
			pDpeUserConfig->Dpe_InBuf_SrcImg_Y_L);
			LOG_INF("=========================================================\n");
			#endif
		} else
			LOG_INF("get Dpe_InBuf_SrcImg_Y_L_fd fail\n");

		memcpy(&DVS_mmu[0], &mmu, sizeof(struct tee_mmu));
		//LOG_INF("DVS_mmu[0] = %lx\n",&DVS_mmu[0].attach);

		LOG_INF("Dpe_InBuf_SrcImg_Y_R fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_fd);
		//LOG_INF("Dpe_InBuf_SrcImg_Y_R_va: %x\n",mmu->sgt->sgl);
		if (success) {
			pDpeUserConfig->Dpe_InBuf_SrcImg_Y_R = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_Ofs));
			LOG_INF("Dpe_InBuf_SrcImg_Y_R = %lx\n",
			pDpeUserConfig->Dpe_InBuf_SrcImg_Y_R);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_InBuf_SrcImg_Y_R fail\n");
		}
		memcpy(&DVS_mmu[1], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_InBuf_ValidMap_L fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_ValidMap_L_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_ValidMap_L_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_ValidMap_L_fd);
		if (success) {
			pDpeUserConfig->Dpe_InBuf_ValidMap_L = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_ValidMap_L_Ofs));
			LOG_INF("Dpe_InBuf_ValidMap_L = %x\n",
				pDpeUserConfig->Dpe_InBuf_ValidMap_L);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_InBuf_ValidMap_L fail\n");
		}
		memcpy(&DVS_mmu[2], &mmu, sizeof(struct tee_mmu));

		LOG_INF("ValidMap_R fd= %x ,offset = %x\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_ValidMap_R_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_ValidMap_R_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_ValidMap_R_fd);
		if (success) {
			pDpeUserConfig->Dpe_InBuf_ValidMap_R = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_ValidMap_R_Ofs));
			LOG_INF("Dpe_InBuf_ValidMap_R = %x\n",
				pDpeUserConfig->Dpe_InBuf_ValidMap_R);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_InBuf_ValidMap_R fail\n");
		}
		memcpy(&DVS_mmu[3], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_OutBuf_OCC fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_OCC_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_OCC_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_OCC_fd);
		if (success) {
			pDpeUserConfig->Dpe_OutBuf_OCC = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_OCC_Ofs));
			#ifdef IOVA_TO_PA
			iova_temp = pDpeUserConfig->Dpe_OutBuf_OCC | iova_temp;
			pgpa = iommu_iova_to_phys(domain, iova_temp);
			LOG_INF("Dpe_OutBuf_OCC pgpa = %lx\n", pgpa);
			iova_temp = 0x200000000;
			LOG_INF("Dpe_OutBuf_OCC = %x\n", pDpeUserConfig->Dpe_OutBuf_OCC);
			LOG_INF("=========================================================\n");
			#endif
		} else {
			LOG_INF("get Dpe_OutBuf_OCC fail\n");
		}
		memcpy(&DVS_mmu[4], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_OutBuf_OCC_Ext fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_OCC_Ext_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_OCC_Ext_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_OCC_Ext_fd);
		if (success) {
			pDpeUserConfig->Dpe_OutBuf_OCC_Ext = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_OCC_Ext_Ofs));
			LOG_INF("Dpe_OutBuf_OCC_Ext = %x\n", pDpeUserConfig->Dpe_OutBuf_OCC_Ext);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_OutBuf_OCC_Ext fail\n");
		}
		memcpy(&DVS_mmu[5], &mmu, sizeof(struct tee_mmu));

		//LOG_INF("Dpe_OutBuf_CONF fd = %d offset = %d\n",
		//pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_CONF_fd,
		//pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_CONF_Ofs);

		//success = dpe_get_dma_buffer(&mmu,
		//pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_CONF_fd);
		//if (success) {
		//pDpeUserConfig->Dpe_OutBuf_CONF = (sg_dma_address(mmu.sgt->sgl) +
		//(pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_CONF_Ofs));
		//LOG_INF("Dpe_OutBuf_CONF = %x\n",pDpeUserConfig->Dpe_OutBuf_CONF);
		//LOG_INF("=========================================================\n");
		//} else {
		//	LOG_INF("get Dpe_OutBuf_CONF fail\n");
		//}


		LOG_INF("Dpe_InBuf_SrcImg_Y_L_Pre fd= %x ,offset = %x\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_L_Pre_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_L_Pre_Ofs);
		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_L_Pre_fd);
		if (success) {
			pDpeUserConfig->Dpe_InBuf_SrcImg_Y_L_Pre =
			(sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_L_Pre_Ofs));
			LOG_INF("Dpe_InBuf_SrcImg_Y_L_Pre = %x\n",
				pDpeUserConfig->Dpe_InBuf_SrcImg_Y_L_Pre);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_InBuf_SrcImg_Y_L_Pre fail\n");
		}
		memcpy(&DVS_mmu[6], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_InBuf_SrcImg_Y_R_Pre fd= %x ,offset = %x\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_Pre_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_Pre_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_Pre_fd);
		if (success) {
			pDpeUserConfig->Dpe_InBuf_SrcImg_Y_R_Pre =
			(sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_R_Pre_Ofs));
			LOG_INF("Dpe_InBuf_SrcImg_Y_R_Pre = %x\n",
				pDpeUserConfig->Dpe_InBuf_SrcImg_Y_R_Pre);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_InBuf_SrcImg_Y_R_Pre fail\n");
		}
		memcpy(&DVS_mmu[7], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_InBuf_P4_L_DV fd= %x ,offset = %x\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_P4_L_DV_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_P4_L_DV_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_P4_L_DV_fd);
		if (success) {
			pDpeUserConfig->Dpe_InBuf_P4_L_DV =
			(sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_P4_L_DV_Ofs));
			LOG_INF("Dpe_InBuf_P4_L_DV = %x\n", pDpeUserConfig->Dpe_InBuf_P4_L_DV);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_InBuf_P4_L_DV fail\n");
		}
		memcpy(&DVS_mmu[8], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_InBuf_P4_R_DV fd= %x ,offset = %x\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_P4_R_DV_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_P4_R_DV_Ofs);
		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_P4_R_DV_fd);
		if (success) {
			pDpeUserConfig->Dpe_InBuf_P4_R_DV = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_P4_R_DV_Ofs));
			LOG_INF("Dpe_InBuf_P4_R_DV = %x\n", pDpeUserConfig->Dpe_InBuf_P4_R_DV);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_InBuf_P4_R_DV fail\n");
		}
		memcpy(&DVS_mmu[9], &mmu, sizeof(struct tee_mmu));
	}

	if ((pDpeUserConfig->Dpe_engineSelect == MODE_DVP_ONLY) ||
		(pDpeUserConfig->Dpe_engineSelect == MODE_DVS_DVP_BOTH)) {

		LOG_INF("Dpe_InBuf_SrcImg_Y fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_fd);
		if (success) {
			pDpeUserConfig->Dpe_InBuf_SrcImg_Y =
			(sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_Y_Ofs));
		#ifdef IOVA_TO_PA
		iova_temp = pDpeUserConfig->Dpe_InBuf_SrcImg_Y | iova_temp;
		pgpa = iommu_iova_to_phys(domain, iova_temp);
		LOG_INF("Dpe_InBuf_SrcImg_Y pgpa = %lx\n", pgpa);
		iova_temp = 0x200000000;
		LOG_INF("Dpe_InBuf_SrcImg_Y = %lx\n",
			pDpeUserConfig->Dpe_InBuf_SrcImg_Y);
		LOG_INF("=========================================================\n");
		#endif
		} else {
			LOG_INF("get Dpe_InBuf_SrcImg_Y fail\n");
		}
		memcpy(&DVP_mmu[0], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_InBuf_SrcImg_C fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_C_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_C_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_C_fd);
		if (success) {
			pDpeUserConfig->Dpe_InBuf_SrcImg_C =
			(sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_SrcImg_C_Ofs));
			#ifdef IOVA_TO_PA
			iova_temp = pDpeUserConfig->Dpe_InBuf_SrcImg_C | iova_temp;
			pgpa = iommu_iova_to_phys(domain, iova_temp);
			LOG_INF("Dpe_InBuf_SrcImg_C pgpa = %lx\n", pgpa);
			iova_temp = 0x200000000;
			LOG_INF("Dpe_InBuf_SrcImg_C = %x\n", pDpeUserConfig->Dpe_InBuf_SrcImg_C);
			LOG_INF("=========================================================\n");
			#endif
		} else {
			LOG_INF("get Dpe_InBuf_SrcImg_C fail\n");
		}
		memcpy(&DVP_mmu[1], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_InBuf_OCC fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_OCC_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_OCC_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_OCC_fd);
		if (success) {
			pDpeUserConfig->Dpe_InBuf_OCC = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_OCC_Ofs));
			LOG_INF("Dpe_InBuf_OCC = %x\n", pDpeUserConfig->Dpe_InBuf_OCC);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_InBuf_OCC fail\n");
		}
		memcpy(&DVP_mmu[2], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_OutBuf_CRM fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_CRM_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_CRM_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_CRM_fd);
		if (success) {
			pDpeUserConfig->Dpe_OutBuf_CRM = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_CRM_Ofs));
			LOG_INF("Dpe_OutBuf_CRM = %x\n", pDpeUserConfig->Dpe_OutBuf_CRM);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_OutBuf_CRM fail\n");
		}
		memcpy(&DVP_mmu[3], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_OutBuf_ASF_RD fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_RD_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_RD_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_RD_fd);
		if (success) {
			pDpeUserConfig->Dpe_OutBuf_ASF_RD = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_RD_Ofs));
			LOG_INF("Dpe_OutBuf_ASF_RD = %x\n", pDpeUserConfig->Dpe_OutBuf_ASF_RD);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_OutBuf_ASF_RD fail\n");
		}
		memcpy(&DVP_mmu[4], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_OutBuf_ASF_HF fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_HF_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_HF_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_HF_fd);
		if (success) {
			pDpeUserConfig->Dpe_OutBuf_ASF_HF =
			(sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_HF_Ofs));
			LOG_INF("Dpe_OutBuf_ASF_HF = %x\n", pDpeUserConfig->Dpe_OutBuf_ASF_HF);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_OutBuf_ASF_HF fail\n");
		}
		memcpy(&DVP_mmu[5], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_OutBuf_ASF_HF_Ext fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_HF_Ext_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_HF_Ext_Ofs);


		LOG_INF("Dpe_OutBuf_WMF_FILT fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_WMF_FILT_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_WMF_FILT_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_WMF_FILT_fd);
		if (success) {
			pDpeUserConfig->Dpe_OutBuf_WMF_FILT = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_WMF_FILT_Ofs));
			LOG_INF("Dpe_OutBuf_WMF_FILT = %x\n", pDpeUserConfig->Dpe_OutBuf_WMF_FILT);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_OutBuf_WMF_FILT fail\n");
		}
		memcpy(&DVP_mmu[6], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_InBuf_OCC_Ext fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_OCC_Ext_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_OCC_Ext_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_OCC_Ext_fd);
		if (success) {
			pDpeUserConfig->Dpe_InBuf_OCC_Ext = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_InBuf_OCC_Ext_Ofs));
			LOG_INF("Dpe_InBuf_OCC_Ext = %x\n", pDpeUserConfig->Dpe_InBuf_OCC_Ext);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_InBuf_OCC_Ext fail\n");
		}
		memcpy(&DVP_mmu[7], &mmu, sizeof(struct tee_mmu));

		LOG_INF("Dpe_OutBuf_ASF_RD_Ext fd = %d offset = %d\n",
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_RD_Ext_fd,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_RD_Ext_Ofs);

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_RD_Ext_fd);
		if (success) {
			pDpeUserConfig->Dpe_OutBuf_ASF_RD_Ext = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_RD_Ext_Ofs));
			LOG_INF("Dpe_OutBuf_ASF_RD_Ext = %x\n",
				pDpeUserConfig->Dpe_OutBuf_ASF_RD_Ext);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_OutBuf_ASF_RD_Ext fail\n");
		}
		memcpy(&DVP_mmu[8], &mmu, sizeof(struct tee_mmu));

		success = dpe_get_dma_buffer(&mmu,
		pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_HF_Ext_fd);
		if (success) {
			pDpeUserConfig->Dpe_OutBuf_ASF_HF_Ext = (sg_dma_address(mmu.sgt->sgl) +
			(pDpeUserConfig->DPE_DMapSettings.Dpe_OutBuf_ASF_HF_Ext_Ofs));
			LOG_INF("Dpe_OutBuf_ASF_HF_Ext = %x\n",
				pDpeUserConfig->Dpe_OutBuf_ASF_HF_Ext);
			LOG_INF("=========================================================\n");
		} else {
			LOG_INF("get Dpe_OutBuf_ASF_HF_Ext fail\n");
		}
		memcpy(&DVP_mmu[9], &mmu, sizeof(struct tee_mmu));

	}

}

	if (pDpeUserConfig->Dpe_engineSelect == MODE_DVS_DVP_BOTH) {
		DPE_Config_DVS(pDpeUserConfig, pDpeConfig);
		DPE_Config_DVP(pDpeUserConfig, pDpeConfig);
		pDpeConfig->DPE_MODE = 0;
	} else if (pDpeUserConfig->Dpe_engineSelect == MODE_DVS_ONLY) {
		DPE_Config_DVS(pDpeUserConfig, pDpeConfig);
		pDpeConfig->DPE_MODE = 1;
	} else if (pDpeUserConfig->Dpe_engineSelect == MODE_DVP_ONLY) {
		DPE_Config_DVP(pDpeUserConfig, pDpeConfig);
		pDpeConfig->DPE_MODE = 2;
	}



	if (g_isDPELogEnable)
		DPE_DumpUserSpaceReg(pDpeConfig);
	//cmdqRecCreate(CMDQ_SCENARIO_ISP_DPE, &handle);
	//cmdqRecSetEngine(handle, engineFlag);
	//cmdq_pkt_cl_create(&handle, dpe_clt);
	handle = cmdq_pkt_create(dpe_clt);
#define CMDQWR(REG) \
	cmdq_pkt_write(handle, dpe_clt_base, \
	REG ##_HW, pDpeConfig->REG, CMDQ_REG_MASK)
#define CMDQWR_DPE_DRAM_ADDR(REG) \
	cmdq_pkt_write(handle, dpe_clt_base, \
	REG ##_HW, (pDpeConfig->REG)>>4, CMDQ_REG_MASK)
LOG_INF("pDpeConfig->DPE_MODE = %x\n", pDpeConfig->DPE_MODE);
if (pDpeConfig->DPE_MODE != 2) {
	if (pDpeConfig->DPE_MODE == 1) {
		/* DVS Only Mode*/
		/* dvp_en = 1 */
		cmdq_pkt_write(handle, dpe_clt_base,
		DVP_CTRL00_HW, 0x80000000, 0x80000000);
	}
	/* mask trigger bit */
	cmdq_pkt_write(handle, dpe_clt_base,
	DVS_CTRL00_HW, pDpeConfig->DVS_CTRL00, 0xDBF5FC00);
	cmdq_pkt_write(handle, dpe_clt_base,
	DVS_CTRL02_HW, 0x70310001, CMDQ_REG_MASK);

	LOG_INF("0[0x%08X %08X]\n", (unsigned int)(DVS_CTRL00_HW),
		(unsigned int)DPE_RD32(DVS_CTRL00_REG));

	LOG_INF("mask trigger bit\n");
	/* cmdq_pkt_write(handle, dpe_clt_base, */
	/* DVS_CTRL07_HW, 0x0000FF1F, CMDQ_REG_MASK); */
	/* cmdq_pkt_write(handle, dpe_clt_base, */
	/* DVS_SRC_CTRL_HW, 0x00000040, CMDQ_REG_MASK); */
	/* DVS Frame Done IRQ */
	if (pDpeConfig->DPE_MODE == 1) {
		LOG_INF("MODE_DVS_ONLY\n");
		cmdq_pkt_write(handle, dpe_clt_base,
		DVS_IRQ_00_HW, 0x00000E00, 0x00000F00);
	} else {
		LOG_INF("MODE_DVS_DVP_BOTH\n");
		cmdq_pkt_write(handle, dpe_clt_base,
		DVS_IRQ_00_HW, 0x00000F00, 0x00000F00);
	}
	LOG_INF("star CMDQWR\n");
	CMDQWR(DVS_DRAM_PITCH);
	CMDQWR(DVS_SRC_00);
	CMDQWR(DVS_SRC_01);
	CMDQWR(DVS_SRC_02);
	CMDQWR(DVS_SRC_03);
	CMDQWR(DVS_SRC_04);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_05_L_FRM0);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_06_L_FRM1);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_07_L_FRM2);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_08_L_FRM3);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_09_R_FRM0);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_10_R_FRM1);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_11_R_FRM2);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_12_R_FRM3);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_13_L_VMAP0);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_14_L_VMAP1);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_15_L_VMAP2);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_16_L_VMAP3);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_17_R_VMAP0);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_18_R_VMAP1);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_19_R_VMAP2);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_20_R_VMAP3);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_21_P4_L_DV_ADR);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_26_OCCDV0);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_27_OCCDV1);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_28_OCCDV2);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_29_OCCDV3);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_34_P4_R_DV_ADR);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_42_OCCDV_EXT0);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_43_OCCDV_EXT1);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_44_OCCDV_EXT2);
	CMDQWR_DPE_DRAM_ADDR(DVS_SRC_45_OCCDV_EXT3);
	CMDQWR(DVS_SRC_46);
	CMDQWR(DVS_DRAM_SEC);
	CMDQWR(DVS_CTRL_ATPG);

	if (pDpeUserConfig->Dpe_DVSSettings.is_pd_mode) {
		//CMDQWR_DPE_DRAM_ADDR(DVS_PD_SRC_04_R_FRM0);
		//CMDQWR_DPE_DRAM_ADDR(DVS_PD_SRC_08_OCCDV0);
		//CMDQWR_DPE_DRAM_ADDR(DVS_PD_SRC_16_DCV_CONF0);
		//if (pDpeUserConfig->DVP_ASF_CONF_EN != 0) //!ISP7
		//CMDQWR_DPE_DRAM_ADDR(DVS_PD_SRC_12_OCCDV_EXT0);
	}
	CMDQWR(DVS_ME_00);
	CMDQWR(DVS_ME_01);
	CMDQWR(DVS_ME_02);
	CMDQWR(DVS_ME_03);
	CMDQWR(DVS_ME_04);
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
	CMDQWR(DVS_ME_37);
	CMDQWR(DVS_ME_38);
	CMDQWR(DVS_ME_39);
	CMDQWR(DVS_DEBUG);
	CMDQWR(DVS_ME_RESERVED);
	CMDQWR(DVS_ME_ATPG);
	CMDQWR(DVS_ME_40);
	CMDQWR(DVS_ME_41);
	CMDQWR(DVS_OCC_PQ_0);
	CMDQWR(DVS_OCC_PQ_1);
	CMDQWR(DVS_OCC_PQ_2);
	CMDQWR(DVS_OCC_PQ_3);
	CMDQWR(DVS_OCC_PQ_4);
	CMDQWR(DVS_OCC_PQ_5);
	CMDQWR(DVS_OCC_PQ_6);
	CMDQWR(DVS_OCC_PQ_7);
	CMDQWR(DVS_OCC_PQ_8);
	CMDQWR(DVS_OCC_PQ_9);
	CMDQWR(DVS_OCC_PQ_10);
	CMDQWR(DVS_OCC_PQ_11);
	CMDQWR(DVS_OCC_ATPG);
/* CRC EN, CRC SEL =0x0 */
/* cmdq_pkt_write(handle, dpe_clt_base, */
/* DVS_CRC_CTRL_HW, 0x00000001, 0x00000F01); */
/* CRC CLEAR  = 1 */
/* cmdq_pkt_write(handle, dpe_clt_base, */
/* DVS_CRC_CTRL_HW, 0x00000010, 0x00000010); */
/* CRC CLEAR  = 0 */
/* cmdq_pkt_write(handle, dpe_clt_base, */
/* DVS_CRC_CTRL_HW, 0x00000000, 0x00000010); */
}
/*================= DVP Settings ==================*/
if (pDpeConfig->DPE_MODE != 1) {
	if (pDpeConfig->DPE_MODE == 2) {
		/* DVP Only Mode*/
		/* dvs_en = 1, DPE FW Tri En = 1, */
		/* dvp_trig_en = 1, dvs_trig_en = 0  */
		cmdq_pkt_write(handle, dpe_clt_base,
		DVS_CTRL00_HW, 0xC8000000, 0xD8300000);
		cmdq_pkt_write(handle, dpe_clt_base,
		DVS_CTRL02_HW, 0x70310001, CMDQ_REG_MASK);
	}
	cmdq_pkt_write(handle, dpe_clt_base,
	DVP_CTRL00_HW, 0x80000080, CMDQ_REG_MASK);
	cmdq_pkt_write(handle, dpe_clt_base,
	DVP_CTRL02_HW, 0x70310001, CMDQ_REG_MASK);
	/* cmdq_pkt_write(handle, dpe_clt_base, */
	/* DVP_CTRL07_HW, 0x00000707, CMDQ_REG_MASK); */
	/* DVP Frame Done IRQ */
	cmdq_pkt_write(handle, dpe_clt_base,
	DVP_IRQ_00_HW, 0x00000E00, 0x00000F00);
	LOG_INF("star CMDQWR DVP Settings\n");


	CMDQWR(DVP_DRAM_PITCH);
	CMDQWR(DVP_SRC_00);
	CMDQWR(DVP_SRC_01);
	CMDQWR(DVP_SRC_02);
	CMDQWR(DVP_SRC_03);
	CMDQWR(DVP_SRC_04);
	CMDQWR(DVP_CTRL04);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_05_Y_FRM0);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_06_Y_FRM1);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_07_Y_FRM2);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_08_Y_FRM3);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_09_C_FRM0);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_10_C_FRM1);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_11_C_FRM2);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_12_C_FRM3);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_13_OCCDV0);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_14_OCCDV1);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_15_OCCDV2);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_16_OCCDV3);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_17_CRM);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_18_ASF_RMDV);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_19_ASF_RDDV);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_20_ASF_DV0);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_21_ASF_DV1);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_22_ASF_DV2);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_23_ASF_DV3);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_24_WMF_HFDV);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_25_WMF_DV0);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_26_WMF_DV1);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_27_WMF_DV2);
	CMDQWR_DPE_DRAM_ADDR(DVP_SRC_28_WMF_DV3);
	CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_13_OCCDV0);
	CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_14_OCCDV1);
	CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_15_OCCDV2);
	CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_16_OCCDV3);
	CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_18_ASF_RMDV);
	CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_19_ASF_RDDV);
	CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_20_ASF_DV0);
	CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_21_ASF_DV1);
	CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_22_ASF_DV2);
	CMDQWR_DPE_DRAM_ADDR(DVP_EXT_SRC_23_ASF_DV3);
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
	CMDQWR(DVP_CORE_16);
/*CRC EN, CRC SEL = 0x1000*/
/* cmdq_pkt_write(handle, dpe_clt_base, */
/* DVP_CRC_CTRL_HW, 0x00000801, 0x00000F01); */
/* CRC CLEAR  = 1 */
/* cmdq_pkt_write(handle, dpe_clt_base, */
/* DVP_CRC_CTRL_HW, 0x00000010, 0x00000010); */
/* CRC CLEAR  = 0 */
/* cmdq_pkt_write(handle, dpe_clt_base, */
/* DVP_CRC_CTRL_HW, 0x00000000, 0x00000010); */
}
//LOG_INF("[CmdqDPEHW]dvp_event_id %d\n", dvs_event_id);
//LOG_INF("[CmdqDPEHW]dvp_event_id %d\n", dvp_event_id);
/* DPE FW Tri = 1*/
cmdq_pkt_write(handle, dpe_clt_base, DVS_CTRL00_HW, 0x20000000, 0x20000000);
LOG_INF("DPE FW Tri = %x\n", pDpeConfig->DVS_CTRL00);
	if (pDpeConfig->DPE_MODE == 1) /* DVS ONLY MODE */
		cmdq_pkt_wfe(handle, dvs_event_id);
	else
		cmdq_pkt_wfe(handle, dvp_event_id);
cmdq_pkt_write(handle, dpe_clt_base, DVS_CTRL00_HW, 0x00000000, 0x20000000);
#if defined(DPE_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
	LOG_INF("DPE_PMQOS_EN =1\n");
	trig_num = (pDpeConfig->DPE_CTRL & 0x00000F00) >> 8;
	w_imgi = pDpeConfig->DPE_SIZE & 0x000001FF;
	h_imgi = (pDpeConfig->DPE_SIZE & 0x01FF0000) >> 16;
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
	//cmdq_task_flush_async_destroy(handle);
	/* flush and destroy in cmdq */
LOG_INF("cmd_pkt start\n");
	cmdq_pkt_flush_threaded(handle,
	cmdq_cb_destroy, (void *)handle);
LOG_INF("cmd_pkt end\n");
	return 0;
}
signed int dpe_feedback(struct frame *frame)
{
	struct DPE_Config *pDpeConfig;

	pDpeConfig = (struct DPE_Config *) frame->data;
	/* TODO: read statistics and write to the frame data */
	// pDpeConfig->DVS_IRQ_STATUS = DPE_RD32(DVS_IRQ_STATUS_REG);
	return 0;
}
static const struct engine_ops dpe_ops = {
	.req_enque_cb = dpe_enque_cb,
	.req_deque_cb = dpe_deque_cb,
	.frame_handler = CmdqDPEHW,
	.req_feedback_cb = dpe_feedback,
};
#if defined(DPE_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
void cmdq_pm_qos_start(struct TaskStruct *task, struct TaskStruct *task_list[],
								u32 size)
{
	unsigned int dma_bandwidth;

	dma_bandwidth = *(unsigned int *) task->prop_addr;
	pm_qos_update_request(&dpe_pm_qos_request, dma_bandwidth);
	LOG_INF("+ PMQOS Bandwidth : %d MB/sec\n", dma_bandwidth);
}
void cmdq_pm_qos_stop(struct TaskStruct *task, struct TaskStruct *task_list[],
								u32 size)
{
	pm_qos_update_request(&dpe_pm_qos_request, 0);
	LOG_DBG("- PMQOS Bandwidth : %d\n", 0);
}
#endif
unsigned int Compute_Para(struct DPE_Config *pDpeConfig,
	unsigned int tile_occ_width)
{
	unsigned int w_width; //!full_tile_width
	unsigned int tile_num = MAX_NUM_TILE;
	unsigned int egn_st_x = pDpeConfig->Dpe_DVSSettings.l_eng_start_x;

	w_width = (tile_num*tile_occ_width)+(2*egn_st_x);
	while (w_width > pDpeConfig->Dpe_DVSSettings.dram_pxl_pitch) {
		tile_num = tile_num - 1;
		w_width = (tile_num*tile_occ_width)+(2*egn_st_x);
	}
	if (tile_num > 0) {
		pDpeConfig->Dpe_DVSSettings.pd_frame_num = tile_num;
		return w_width;
	}
	pDpeConfig->Dpe_DVSSettings.pd_frame_num = 1;
	return 0;
}
void Get_Tile_Info(struct DPE_Config *pDpeConfig)
{
	unsigned int tile_occ_width[TILE_WITH_NUM] = {640, 512, 384};
	unsigned int w_width[TILE_WITH_NUM] = {0};
	unsigned int tile_num[TILE_WITH_NUM] = {0};
	unsigned int idx = 0, i = 0;
	unsigned int max_width = 0, interval = 0, st_x = 0;
	unsigned int engStart_x_L, engStart_x_R, frmHeight;
	unsigned int engWidth;

	engStart_x_L = pDpeConfig->Dpe_DVSSettings.l_eng_start_x;
	engStart_x_R = pDpeConfig->Dpe_DVSSettings.r_eng_start_x;
	frmHeight = pDpeConfig->Dpe_DVSSettings.frm_height;
	engWidth = pDpeConfig->Dpe_DVSSettings.eng_width;
#if IS_ENABLED(CONFIG_MTK_LEGACY)
	if (pDpeConfig->Dpe_DVSSettings.dram_pxl_pitch <
		(tile_occ_width[TILE_WITH_NUM-1] + (2*engStart_x_L))) {
		LOG_ERR("Frame size [%d] is smaller than 384\n",
		pDpeConfig->Dpe_DVSSettings.dram_pxl_pitch);
		pDpeConfig->Dpe_DVSSettings.pd_frame_num = 1;
	} else {
		for (i = 0; i < TILE_WITH_NUM; i++) {
			w_width[i] =
			Compute_Para(pDpeConfig, tile_occ_width[i]);
			tile_num[i] =
			pDpeConfig->Dpe_DVSSettings.pd_frame_num;
			if (w_width[i] > max_width) {
				max_width = w_width[i];
				idx = i;
			}
		}

		interval = (pDpeConfig->Dpe_DVSSettings.dram_pxl_pitch - w_width[idx])/2;
		st_x = ((interval%16) == 0) ? (interval) : ((interval/16)*16);

		pDpeConfig->Dpe_DVSSettings.frm_width =
		tile_occ_width[idx] + (2*engStart_x_L);
		pDpeConfig->Dpe_DVSSettings.eng_width =
		pDpeConfig->Dpe_DVSSettings.frm_width -
		engStart_x_L - engStart_x_R;
		pDpeConfig->Dpe_DVSSettings.eng_height
		= frmHeight -
		(2*pDpeConfig->Dpe_DVSSettings.eng_start_y);
		pDpeConfig->Dpe_DVSSettings.occ_width = tile_occ_width[idx];
		pDpeConfig->Dpe_DVSSettings.occ_start_x = engStart_x_L;
		pDpeConfig->Dpe_DVSSettings.pd_frame_num = tile_num[idx];
#if defined(UT_CASE)
		pDpeConfig->Dpe_DVSSettings.pd_st_x = 0;
#else
		pDpeConfig->Dpe_DVSSettings.pd_st_x = st_x;
#endif
	}
#else

	if (pDpeConfig->Dpe_DVSSettings.dram_pxl_pitch <
		(tile_occ_width[TILE_WITH_NUM-1] + (2*engStart_x_L))) {
		LOG_ERR("Frame size [%d] is smaller than 384\n",
		pDpeConfig->Dpe_DVSSettings.dram_pxl_pitch);
		pDpeConfig->Dpe_DVSSettings.pd_frame_num = 1;
	} else { //!ISP7 tile mode
		for (i = 0; i < TILE_WITH_NUM; i++) {
			w_width[i] =
			Compute_Para(pDpeConfig, tile_occ_width[i]);
			LOG_INF("a w_width[%d] = %d\n", i, w_width[i]);
			tile_num[i] =
			pDpeConfig->Dpe_DVSSettings.pd_frame_num;
			LOG_INF("a tile_num[%d] = %d\n", i, tile_num[i]);
			if (w_width[i] > max_width) {
				max_width = w_width[i];
				idx = i;
			}
		}

		interval = (pDpeConfig->Dpe_DVSSettings.dram_pxl_pitch - w_width[idx]) / 2;
		st_x = ((interval%16) == 0) ? (interval) : ((interval/16)*16);
		pDpeConfig->Dpe_DVSSettings.frm_width = engWidth;

		pDpeConfig->Dpe_DVSSettings.eng_height
		= frmHeight -
		(2*pDpeConfig->Dpe_DVSSettings.eng_start_y);
		pDpeConfig->Dpe_DVSSettings.occ_width = tile_occ_width[idx];
		pDpeConfig->Dpe_DVSSettings.occ_start_x = engStart_x_L;
		pDpeConfig->Dpe_DVSSettings.pd_frame_num = tile_num[idx];



		if (pDpeConfig->Dpe_DVSSettings.dram_out_pitch_en == 0 &&
			pDpeConfig->Dpe_DVSSettings.occ_width > 640)
			LOG_INF("Dram_out_pitch_en not turn on, but occwidth over 640\n");
		if (pDpeConfig->Dpe_DVSSettings.dram_out_pitch_en == 1 &&
			pDpeConfig->Dpe_DVSSettings.occ_width < 640)
			LOG_INF("Dram_out_pitch_en turn on, but occwidth is smaller than 640\n");

#if defined(UT_CASE)
		pDpeConfig->Dpe_DVSSettings.pd_st_x = 0;
#else
		pDpeConfig->Dpe_DVSSettings.pd_st_x = st_x;
#endif
	}
#endif
}
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
	spin_lock(&(DPEInfo.SpinLockDPE));
	if (g_u4EnableClockCount == 0) {
		spin_unlock(&(DPEInfo.SpinLockDPE));
		return 0;
	}
	spin_unlock(&(DPEInfo.SpinLockDPE));
//#if 1
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
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL06_HW),
		(unsigned int)DPE_RD32(DVS_CTRL06_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL07_HW),
		(unsigned int)DPE_RD32(DVS_CTRL07_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_IRQ_00_HW),
		(unsigned int)DPE_RD32(DVS_IRQ_00_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL_STATUS0_HW),
		(unsigned int)DPE_RD32(DVS_CTRL_STATUS0_REG));
//	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL_STATUS1_HW),
//		(unsigned int)DPE_RD32(DVS_CTRL_STATUS1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL_STATUS2_HW),
		(unsigned int)DPE_RD32(DVS_CTRL_STATUS2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_IRQ_STATUS_HW),
		(unsigned int)DPE_RD32(DVS_IRQ_STATUS_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_FRM_STATUS0_HW),
		(unsigned int)DPE_RD32(DVS_FRM_STATUS0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_FRM_STATUS1_HW),
		(unsigned int)DPE_RD32(DVS_FRM_STATUS1_REG));
//	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_FRM_STATUS2_HW),
//		(unsigned int)DPE_RD32(DVS_FRM_STATUS2_REG));
//	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_FRM_STATUS3_HW),
//		(unsigned int)DPE_RD32(DVS_FRM_STATUS3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CUR_STATUS_HW),
		(unsigned int)DPE_RD32(DVS_CUR_STATUS_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_CTRL_HW),
		(unsigned int)DPE_RD32(DVS_SRC_CTRL_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CRC_CTRL_HW),
		(unsigned int)DPE_RD32(DVS_CRC_CTRL_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CRC_IN_HW),
		(unsigned int)DPE_RD32(DVS_CRC_IN_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_DRAM_STA0_HW),
		(unsigned int)DPE_RD32(DVS_DRAM_STA0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_DRAM_STA1_HW),
		(unsigned int)DPE_RD32(DVS_DRAM_STA1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_DRAM_ULT_HW),
		(unsigned int)DPE_RD32(DVS_DRAM_ULT_REG));
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
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_06_L_FRM1_HW),
		(unsigned int)DPE_RD32(DVS_SRC_06_L_FRM1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_07_L_FRM2_HW),
		(unsigned int)DPE_RD32(DVS_SRC_07_L_FRM2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_08_L_FRM3_HW),
		(unsigned int)DPE_RD32(DVS_SRC_08_L_FRM3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_09_R_FRM0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_09_R_FRM0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_10_R_FRM1_HW),
		(unsigned int)DPE_RD32(DVS_SRC_10_R_FRM1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_11_R_FRM2_HW),
		(unsigned int)DPE_RD32(DVS_SRC_11_R_FRM2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_12_R_FRM3_HW),
		(unsigned int)DPE_RD32(DVS_SRC_12_R_FRM3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_13_L_VMAP0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_13_L_VMAP0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_14_L_VMAP1_HW),
		(unsigned int)DPE_RD32(DVS_SRC_14_L_VMAP1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_15_L_VMAP2_HW),
		(unsigned int)DPE_RD32(DVS_SRC_15_L_VMAP2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_16_L_VMAP3_HW),
		(unsigned int)DPE_RD32(DVS_SRC_16_L_VMAP3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_17_R_VMAP0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_17_R_VMAP0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_18_R_VMAP1_HW),
		(unsigned int)DPE_RD32(DVS_SRC_18_R_VMAP1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_19_R_VMAP2_HW),
		(unsigned int)DPE_RD32(DVS_SRC_19_R_VMAP2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_20_R_VMAP3_HW),
		(unsigned int)DPE_RD32(DVS_SRC_20_R_VMAP3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_21_P4_L_DV_ADR_HW),
		(unsigned int)DPE_RD32(DVS_SRC_21_P4_L_DV_ADR_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_26_OCCDV0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_26_OCCDV0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_27_OCCDV1_HW),
		(unsigned int)DPE_RD32(DVS_SRC_27_OCCDV1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_28_OCCDV2_HW),
		(unsigned int)DPE_RD32(DVS_SRC_28_OCCDV2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_29_OCCDV3_HW),
		(unsigned int)DPE_RD32(DVS_SRC_29_OCCDV3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_34_P4_R_DV_ADR_HW),
		(unsigned int)DPE_RD32(DVS_SRC_34_P4_R_DV_ADR_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_42_OCCDV_EXT0_HW),
		(unsigned int)DPE_RD32(DVS_SRC_42_OCCDV_EXT0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_43_OCCDV_EXT1_HW),
		(unsigned int)DPE_RD32(DVS_SRC_43_OCCDV_EXT1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_44_OCCDV_EXT2_HW),
		(unsigned int)DPE_RD32(DVS_SRC_44_OCCDV_EXT2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_45_OCCDV_EXT3_HW),
		(unsigned int)DPE_RD32(DVS_SRC_45_OCCDV_EXT3_REG));
//!
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_SRC_46_HW),
		(unsigned int)DPE_RD32(DVS_SRC_46_REG));
//!
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CRC_OUT_0_HW),
		(unsigned int)DPE_RD32(DVS_CRC_OUT_0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CRC_OUT_1_HW),
		(unsigned int)DPE_RD32(DVS_CRC_OUT_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CRC_OUT_2_HW),
		(unsigned int)DPE_RD32(DVS_CRC_OUT_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CRC_OUT_3_HW),
		(unsigned int)DPE_RD32(DVS_CRC_OUT_3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_DRAM_SEC_HW),
		(unsigned int)DPE_RD32(DVS_DRAM_SEC_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL_RESERVED_HW),
		(unsigned int)DPE_RD32(DVS_CTRL_RESERVED_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_CTRL_ATPG_HW),
		(unsigned int)DPE_RD32(DVS_CTRL_ATPG_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_00_HW),
		(unsigned int)DPE_RD32(DVS_ME_00_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_01_HW),
		(unsigned int)DPE_RD32(DVS_ME_01_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_02_HW),
		(unsigned int)DPE_RD32(DVS_ME_02_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_03_HW),
		(unsigned int)DPE_RD32(DVS_ME_03_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_04_HW),
		(unsigned int)DPE_RD32(DVS_ME_04_REG));
	//LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_05_HW),
	//	(unsigned int)DPE_RD32(DVS_ME_05_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_06_HW),
		(unsigned int)DPE_RD32(DVS_ME_06_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_07_HW),
		(unsigned int)DPE_RD32(DVS_ME_07_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_08_HW),
		(unsigned int)DPE_RD32(DVS_ME_08_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_09_HW),
		(unsigned int)DPE_RD32(DVS_ME_09_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_10_HW),
		(unsigned int)DPE_RD32(DVS_ME_10_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_11_HW),
		(unsigned int)DPE_RD32(DVS_ME_11_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_12_HW),
		(unsigned int)DPE_RD32(DVS_ME_12_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_13_HW),
		(unsigned int)DPE_RD32(DVS_ME_13_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_14_HW),
		(unsigned int)DPE_RD32(DVS_ME_14_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_15_HW),
		(unsigned int)DPE_RD32(DVS_ME_15_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_16_HW),
		(unsigned int)DPE_RD32(DVS_ME_16_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_17_HW),
		(unsigned int)DPE_RD32(DVS_ME_17_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_18_HW),
		(unsigned int)DPE_RD32(DVS_ME_18_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_19_HW),
		(unsigned int)DPE_RD32(DVS_ME_19_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_20_HW),
		(unsigned int)DPE_RD32(DVS_ME_20_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_21_HW),
		(unsigned int)DPE_RD32(DVS_ME_21_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_22_HW),
		(unsigned int)DPE_RD32(DVS_ME_22_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_23_HW),
		(unsigned int)DPE_RD32(DVS_ME_23_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_24_HW),
		(unsigned int)DPE_RD32(DVS_ME_24_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_25_HW),
		(unsigned int)DPE_RD32(DVS_ME_25_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_26_HW),
		(unsigned int)DPE_RD32(DVS_ME_26_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_27_HW),
		(unsigned int)DPE_RD32(DVS_ME_27_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_28_HW),
		(unsigned int)DPE_RD32(DVS_ME_28_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_29_HW),
		(unsigned int)DPE_RD32(DVS_ME_29_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_30_HW),
		(unsigned int)DPE_RD32(DVS_ME_30_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_31_HW),
		(unsigned int)DPE_RD32(DVS_ME_31_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_32_HW),
		(unsigned int)DPE_RD32(DVS_ME_32_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_33_HW),
		(unsigned int)DPE_RD32(DVS_ME_33_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_34_HW),
		(unsigned int)DPE_RD32(DVS_ME_34_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_35_HW),
		(unsigned int)DPE_RD32(DVS_ME_35_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_36_HW),
		(unsigned int)DPE_RD32(DVS_ME_36_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_37_HW),
		(unsigned int)DPE_RD32(DVS_ME_37_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_38_HW),
		(unsigned int)DPE_RD32(DVS_ME_38_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_39_HW),
		(unsigned int)DPE_RD32(DVS_ME_39_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_DEBUG_HW),
		(unsigned int)DPE_RD32(DVS_DEBUG_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_RESERVED_HW),
		(unsigned int)DPE_RD32(DVS_ME_RESERVED_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_ATPG_HW),
		(unsigned int)DPE_RD32(DVS_ME_ATPG_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_40_HW),
		(unsigned int)DPE_RD32(DVS_ME_40_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_ME_41_HW),
		(unsigned int)DPE_RD32(DVS_ME_41_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_OCC_PQ_0_HW),
		(unsigned int)DPE_RD32(DVS_OCC_PQ_0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_OCC_PQ_1_HW),
		(unsigned int)DPE_RD32(DVS_OCC_PQ_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_OCC_PQ_2_HW),
		(unsigned int)DPE_RD32(DVS_OCC_PQ_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_OCC_PQ_3_HW),
		(unsigned int)DPE_RD32(DVS_OCC_PQ_3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_OCC_PQ_4_HW),
		(unsigned int)DPE_RD32(DVS_OCC_PQ_4_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_OCC_PQ_5_HW),
		(unsigned int)DPE_RD32(DVS_OCC_PQ_5_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_OCC_PQ_6_HW),
		(unsigned int)DPE_RD32(DVS_OCC_PQ_6_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_OCC_PQ_7_HW),
		(unsigned int)DPE_RD32(DVS_OCC_PQ_7_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_OCC_PQ_8_HW),
		(unsigned int)DPE_RD32(DVS_OCC_PQ_8_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_OCC_PQ_9_HW),
		(unsigned int)DPE_RD32(DVS_OCC_PQ_9_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_OCC_PQ_10_HW),
		(unsigned int)DPE_RD32(DVS_OCC_PQ_10_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_OCC_PQ_11_HW),
		(unsigned int)DPE_RD32(DVS_OCC_PQ_11_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVS_OCC_ATPG_HW),
		(unsigned int)DPE_RD32(DVS_OCC_ATPG_REG));
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
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_IRQ_01_HW),
		(unsigned int)DPE_RD32(DVP_IRQ_01_REG));
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
//	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_FRM_STATUS1_HW),
//		(unsigned int)DPE_RD32(DVP_FRM_STATUS1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_FRM_STATUS2_HW),
		(unsigned int)DPE_RD32(DVP_FRM_STATUS2_REG));
//	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_FRM_STATUS3_HW),
//		(unsigned int)DPE_RD32(DVP_FRM_STATUS3_REG));
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
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_06_Y_FRM1_HW),
		(unsigned int)DPE_RD32(DVP_SRC_06_Y_FRM1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_07_Y_FRM2_HW),
		(unsigned int)DPE_RD32(DVP_SRC_07_Y_FRM2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_08_Y_FRM3_HW),
		(unsigned int)DPE_RD32(DVP_SRC_08_Y_FRM3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_09_C_FRM0_HW),
		(unsigned int)DPE_RD32(DVP_SRC_09_C_FRM0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_10_C_FRM1_HW),
		(unsigned int)DPE_RD32(DVP_SRC_10_C_FRM1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_11_C_FRM2_HW),
		(unsigned int)DPE_RD32(DVP_SRC_11_C_FRM2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_12_C_FRM3_HW),
		(unsigned int)DPE_RD32(DVP_SRC_12_C_FRM3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_13_OCCDV0_HW),
		(unsigned int)DPE_RD32(DVP_SRC_13_OCCDV0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_14_OCCDV1_HW),
		(unsigned int)DPE_RD32(DVP_SRC_14_OCCDV1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_15_OCCDV2_HW),
		(unsigned int)DPE_RD32(DVP_SRC_15_OCCDV2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_16_OCCDV3_HW),
		(unsigned int)DPE_RD32(DVP_SRC_16_OCCDV3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_17_CRM_HW),
		(unsigned int)DPE_RD32(DVP_SRC_17_CRM_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_18_ASF_RMDV_HW),
		(unsigned int)DPE_RD32(DVP_SRC_18_ASF_RMDV_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_19_ASF_RDDV_HW),
		(unsigned int)DPE_RD32(DVP_SRC_19_ASF_RDDV_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_20_ASF_DV0_HW),
		(unsigned int)DPE_RD32(DVP_SRC_20_ASF_DV0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_21_ASF_DV1_HW),
		(unsigned int)DPE_RD32(DVP_SRC_21_ASF_DV1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_22_ASF_DV2_HW),
		(unsigned int)DPE_RD32(DVP_SRC_22_ASF_DV2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_23_ASF_DV3_HW),
		(unsigned int)DPE_RD32(DVP_SRC_23_ASF_DV3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_24_WMF_HFDV_HW),
		(unsigned int)DPE_RD32(DVP_SRC_24_WMF_HFDV_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_25_WMF_DV0_HW),
		(unsigned int)DPE_RD32(DVP_SRC_25_WMF_DV0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_26_WMF_DV1_HW),
		(unsigned int)DPE_RD32(DVP_SRC_26_WMF_DV1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_27_WMF_DV2_HW),
		(unsigned int)DPE_RD32(DVP_SRC_27_WMF_DV2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_28_WMF_DV3_HW),
		(unsigned int)DPE_RD32(DVP_SRC_28_WMF_DV3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_00_HW),
		(unsigned int)DPE_RD32(DVP_CORE_00_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_01_HW),
		(unsigned int)DPE_RD32(DVP_CORE_01_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_02_HW),
		(unsigned int)DPE_RD32(DVP_CORE_02_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_03_HW),
		(unsigned int)DPE_RD32(DVP_CORE_03_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_04_HW),
		(unsigned int)DPE_RD32(DVP_CORE_04_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_05_HW),
		(unsigned int)DPE_RD32(DVP_CORE_05_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_06_HW),
		(unsigned int)DPE_RD32(DVP_CORE_06_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_07_HW),
		(unsigned int)DPE_RD32(DVP_CORE_07_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_08_HW),
		(unsigned int)DPE_RD32(DVP_CORE_08_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_09_HW),
		(unsigned int)DPE_RD32(DVP_CORE_09_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_10_HW),
		(unsigned int)DPE_RD32(DVP_CORE_10_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_11_HW),
		(unsigned int)DPE_RD32(DVP_CORE_11_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_12_HW),
		(unsigned int)DPE_RD32(DVP_CORE_12_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_13_HW),
		(unsigned int)DPE_RD32(DVP_CORE_13_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_14_HW),
		(unsigned int)DPE_RD32(DVP_CORE_14_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_15_HW),
		(unsigned int)DPE_RD32(DVP_CORE_15_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_16_HW),
		(unsigned int)DPE_RD32(DVP_CORE_16_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_SRC_CTRL_HW),
		(unsigned int)DPE_RD32(DVP_SRC_CTRL_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CTRL_RESERVED_HW),
		(unsigned int)DPE_RD32(DVP_CTRL_RESERVED_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CTRL_ATPG_HW),
		(unsigned int)DPE_RD32(DVP_CTRL_ATPG_REG));

	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CRC_OUT_0_HW),
		(unsigned int)DPE_RD32(DVP_CRC_OUT_0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CRC_OUT_1_HW),
		(unsigned int)DPE_RD32(DVP_CRC_OUT_1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CRC_OUT_2_HW),
		(unsigned int)DPE_RD32(DVP_CRC_OUT_2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CRC_CTRL_HW),
		(unsigned int)DPE_RD32(DVP_CRC_CTRL_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CRC_OUT_HW),
		(unsigned int)DPE_RD32(DVP_CRC_OUT_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CRC_IN_HW),
		(unsigned int)DPE_RD32(DVP_CRC_IN_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_DRAM_STA_HW),
		(unsigned int)DPE_RD32(DVP_DRAM_STA_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_DRAM_ULT_HW),
		(unsigned int)DPE_RD32(DVP_DRAM_ULT_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_DRAM_PITCH_HW),
		(unsigned int)DPE_RD32(DVP_DRAM_PITCH_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_CORE_CRC_IN_HW),
		(unsigned int)DPE_RD32(DVP_CORE_CRC_IN_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_EXT_SRC_13_OCCDV0_HW),
		(unsigned int)DPE_RD32(DVP_EXT_SRC_13_OCCDV0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_EXT_SRC_14_OCCDV1_HW),
		(unsigned int)DPE_RD32(DVP_EXT_SRC_14_OCCDV1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_EXT_SRC_15_OCCDV2_HW),
		(unsigned int)DPE_RD32(DVP_EXT_SRC_15_OCCDV2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_EXT_SRC_16_OCCDV3_HW),
		(unsigned int)DPE_RD32(DVP_EXT_SRC_16_OCCDV3_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_EXT_SRC_18_ASF_RMDV_HW),
		(unsigned int)DPE_RD32(DVP_EXT_SRC_18_ASF_RMDV_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_EXT_SRC_19_ASF_RDDV_HW),
		(unsigned int)DPE_RD32(DVP_EXT_SRC_19_ASF_RDDV_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_EXT_SRC_20_ASF_DV0_HW),
		(unsigned int)DPE_RD32(DVP_EXT_SRC_20_ASF_DV0_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_EXT_SRC_21_ASF_DV1_HW),
		(unsigned int)DPE_RD32(DVP_EXT_SRC_21_ASF_DV1_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_EXT_SRC_22_ASF_DV2_HW),
		(unsigned int)DPE_RD32(DVP_EXT_SRC_22_ASF_DV2_REG));
	LOG_INF("[0x%08X %08X]\n", (unsigned int)(DVP_EXT_SRC_23_ASF_DV3_HW),
		(unsigned int)DPE_RD32(DVP_EXT_SRC_23_ASF_DV3_REG));
#endif
	LOG_INF("- X.");
	/*  */
	return Ret;
}
#endif
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
static inline void DPE_Prepare_Enable_ccf_clock(void)
{
	int ret;

	LOG_INF("DPE_Prepare_Enable_ccf_clock_star\n");
	//!smi_bus_prepare_enable(SMI_LARB12, DPE_DEV_NAME);
	if (pm_runtime_get_sync(gdev))
		LOG_INF("pm_runtime_get_sync fail\n");

	ret = clk_prepare_enable(dpe_clk.IMG_IPE);
	if (ret)
		LOG_INF("cannot prepare and enable IMG_IPE clock\n");
	ret = clk_prepare_enable(dpe_clk.IPE_DPE);
	if (ret)
		LOG_INF("cannot prepare and enable IPE_DPE clock\n");
	ret = clk_prepare_enable(dpe_clk.IPE_SMI_LARB12);
	if (ret)
		LOG_INF("cannot prepare and enable IPE_SMI_LARB12 clock\n");

	ret = clk_prepare_enable(dpe_clk.IPE_TOP);
	if (ret)
		LOG_INF("cannot prepare and enable IPE_TOP clock\n");
}
static inline void DPE_Disable_Unprepare_ccf_clock(void)
{
	clk_disable_unprepare(dpe_clk.IMG_IPE);
	clk_disable_unprepare(dpe_clk.IPE_DPE);
	clk_disable_unprepare(dpe_clk.IPE_TOP);
	clk_disable_unprepare(dpe_clk.IPE_SMI_LARB12);
	pm_runtime_put_sync(gdev);
}
#endif
#ifdef CONFIG_MTK_IOMMU_V2
static inline int m4u_control_iommu_port(void)
{
	struct M4U_PORT_STRUCT sPort;
	int ret = 0;
	/* LARB19 */
	int count_of_ports = 0;
	int i = 0;

	LOG_INF("DPE m4u_control_iommu start\n");
#if (MTK_DPE_VER == 0)
	count_of_ports = M4U_PORT_L19_IPE_DVP_WDMA_DISP -
		M4U_PORT_L19_IPE_DVS_RDMA_DISP + 1;
	for (i = 0; i < count_of_ports; i++) {
		sPort.ePortID = M4U_PORT_L19_IPE_DVS_RDMA_DISP+i;
		sPort.Virtuality = DPE_MEM_USE_VIRTUL;
		//LOG_INF("config M4U Port ePortID=%d\n", sPort.ePortID);
		#if defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U)
		ret = m4u_config_port(&sPort);
		if (ret == 0) {
			//LOG_INF("config M4U Port %s to %s SUCCESS\n",
			//iommu_get_port_name(M4U_PORT_L19_IPE_DVS_RDMA_DISP+i),
			//DPE_MEM_USE_VIRTUL ? "virtual" : "physical");
		} else {
			LOG_INF("config M4U Port %s to %s FAIL(ret=%d)\n",
			iommu_get_port_name(M4U_PORT_L19_IPE_DVS_RDMA_DISP+i),
			DPE_MEM_USE_VIRTUL ? "virtual" : "physical", ret);
			ret = -1;
		}
		#endif
	}
#else
	count_of_ports = M4U_PORT_L19_IPE_DVP_WDMA -
		M4U_PORT_L19_IPE_DVS_RDMA + 1;
	for (i = 0; i < count_of_ports; i++) {
		sPort.ePortID = M4U_PORT_L19_IPE_DVS_RDMA+i;
		sPort.Virtuality = DPE_MEM_USE_VIRTUL;
		//LOG_INF("config M4U Port ePortID=%d\n", sPort.ePortID);
		#if defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U)
		ret = m4u_config_port(&sPort);
		if (ret == 0) {
			//LOG_INF("config M4U Port %s to %s SUCCESS\n",
			//iommu_get_port_name(M4U_PORT_L19_IPE_DVS_RDMA+i),
			//DPE_MEM_USE_VIRTUL ? "virtual" : "physical");
		} else {
			LOG_INF("config M4U Port %s to %s FAIL(ret=%d)\n",
			iommu_get_port_name(M4U_PORT_L19_IPE_DVS_RDMA+i),
			DPE_MEM_USE_VIRTUL ? "virtual" : "physical", ret);
			ret = -1;
		}
		#endif
	}
//#endif
	return ret;
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
#ifdef CONFIG_MTK_IOMMU_V2
	int ret = 0;
#endif
	if (En) {		/* Enable clock. */
/* LOG_DBG("clock enbled. g_u4EnableClockCount: %d.", g_u4EnableClockCount); */
		//mutex_lock(&gDpeMutex);
		spin_lock(&(DPEInfo.SpinLockDPE));
		switch (g_u4EnableClockCount) {
		case 0:
			g_u4EnableClockCount++;
			spin_unlock(&(DPEInfo.SpinLockDPE));
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifndef EP_NO_CLKMGR
			DPE_Prepare_Enable_ccf_clock();
			//mutex_unlock(&gDpeMutex);//!
#else
			/* Enable clock by hardcode:
			 * 1. CAMSYS_CG_CLR (0x1A000008) = 0xffffffff;
			 * 2. IMG_CG_CLR (0x15000008) = 0xffffffff;
			 */
			LOG_INF("[Debug] It's LDVT load, EP_NO_CLKMGR");
			setReg = 0xFFFFFFFF;
			DPE_WR32(IPESYS_REG_CG_CLR, setReg);
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
			g_u4EnableClockCount++;
			spin_unlock(&(DPEInfo.SpinLockDPE));
			//mutex_unlock(&gDpeMutex);
			break;
		}
#ifdef CONFIG_MTK_IOMMU_V2
		spin_lock(&(DPEInfo.SpinLockDPE));
		if (g_u4EnableClockCount == 1) {
			spin_unlock(&(DPEInfo.SpinLockDPE));
			ret = m4u_control_iommu_port();
			if (ret)
				LOG_ERR("cannot config M4U IOMMU PORTS\n");
		} else {
			spin_unlock(&(DPEInfo.SpinLockDPE));
		}
#endif
	} else {		/* Disable clock. */
		/* LOG_DBG("Dpe clock disabled. g_u4EnableClockCount: %d.",
		 * g_u4EnableClockCount);
		 */
		spin_lock(&(DPEInfo.SpinLockDPE));
		g_u4EnableClockCount--;
		switch (g_u4EnableClockCount) {
		case 0:
			spin_unlock(&(DPEInfo.SpinLockDPE));
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifndef EP_NO_CLKMGR
			DPE_Disable_Unprepare_ccf_clock();
#else
			/* Disable clock by hardcode:
			 *  1. CAMSYS_CG_SET (0x1A000004) = 0xffffffff;
			 *  2. IMG_CG_SET (0x15000004) = 0xffffffff;
			 */
			setReg = 0xFFFFFFFF;
			DPE_WR32(IPESYS_REG_CG_SET, setReg);
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
			spin_unlock(&(DPEInfo.SpinLockDPE));
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
	enum DPE_PROCESS_ID_ENUM whichReq = DPE_PROCESS_ID_NONE;
	/*unsigned int i;*/
	unsigned long flags; /* old: unsigned int flags;*/
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
	p = WaitIrq->ProcessID % IRQ_USER_NUM_MAX;
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
		"WaitIrq Timeout:whichReq(%d),ProcID(%d) DpeIrqCnt[%d](0x%08X) WriteReq(0x%08X) ReadReq(0x%08X)\n",
			whichReq, WaitIrq->ProcessID,
			p, DPEInfo.IrqInfo.DpeIrqCnt[p],
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
				DPEInfo.IrqInfo.DpeIrqCnt[p]--;
				if (DPEInfo.IrqInfo.DpeIrqCnt[p] == 0)
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
					g_DPE_ReqRing.WriteIdx].DpeFrameConfig[
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.WriteIdx].FrameWRIdx++],
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
					g_DPE_ReqRing.ReadIdx].DpeFrameStatus[
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].RrameRDIdx]) {
					memcpy(&dpe_DpeConfig,
					&g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].DpeFrameConfig[
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].RrameRDIdx],
					sizeof(struct DPE_Config));
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].DpeFrameStatus[
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
					g_DPE_ReqRing.ReadIdx].DpeFrameConfig[
					g_DPE_ReqRing.DPEReq_Struct[
					g_DPE_ReqRing.ReadIdx].RrameRDIdx],
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

	LOG_INF("- E. UserCount: %d.", DPEInfo.UserCount);
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
		pUserInfo->Pid = DPEInfo.UserCount;
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
		/* do wait queue head init when re-enter in camera */
		/*  */
		for (i = 0; i < _SUPPORT_MAX_DPE_REQUEST_RING_SIZE_; i++) {
			/* DPE */
			g_DPE_ReqRing.DPEReq_Struct[i].processID = 0x0;
			g_DPE_ReqRing.DPEReq_Struct[i].callerID = 0x0;
			g_DPE_ReqRing.DPEReq_Struct[i].enqueReqNum = 0x0;
			/* g_DPE_ReqRing.DPEReq_Struct[i].enqueIdx = 0x0; */
			g_DPE_ReqRing.DPEReq_Struct[i].State =
				DPE_REQUEST_STATE_EMPTY;
			g_DPE_ReqRing.DPEReq_Struct[i].FrameWRIdx = 0x0;
			g_DPE_ReqRing.DPEReq_Struct[i].RrameRDIdx = 0x0;
			for (j = 0; j < _SUPPORT_MAX_DPE_FRAME_REQUEST_; j++) {
				g_DPE_ReqRing.DPEReq_Struct[i].DpeFrameStatus[
					j] = DPE_FRAME_STATUS_EMPTY;
			}
		}
		g_DPE_ReqRing.WriteIdx = 0x0;
		g_DPE_ReqRing.ReadIdx = 0x0;
		g_DPE_ReqRing.HWProcessIdx = 0x0;
		for (i = 0; i < DPE_IRQ_TYPE_AMOUNT; i++)
			DPEInfo.IrqInfo.Status[i] = 0;
		for (i = 0; i < _SUPPORT_MAX_DPE_FRAME_REQUEST_; i++)
			DPEInfo.ProcessID[i] = 0;
		DPEInfo.WriteReqIdx = 0;
		DPEInfo.ReadReqIdx = 0;
		/* DPEInfo.IrqInfo.DpeIrqCnt = 0; */
		for (i = 0; i < IRQ_USER_NUM_MAX; i++)
			DPEInfo.IrqInfo.DpeIrqCnt[i] = 0;
		/*  */
		dpe_register_requests(&dpe_reqs, sizeof(struct DPE_Config));
		dpe_set_engine_ops(&dpe_reqs, &dpe_ops);
		LOG_DBG("Cur Usr(%d), (proc, pid, tgid)=(%s, %d, %d), 1st user",
			DPEInfo.UserCount, current->comm, current->pid,
								current->tgid);
	}
	/* Enable clock */
	DPE_EnableClock(MTRUE);
	g_SuspendCnt = 0;
	LOG_INF("open g_u4EnableClockCount: %d", g_u4EnableClockCount);
	/*  */
	LOG_INF(
	"open g_dpewb_dvme_int_Buffer_pa(0x%lx), g_dpewb_cost_int_Buffer_pa(0x%lx), g_dpewb_asfrm_Buffer_pa(0x%lx), g_dpewb_asfrmext_Buffer_pa(0x%lx), g_dpewb_wmfhf_Buffer_pa(0x%lx)\n",
	((uintptr_t)g_dpewb_dvme_int_Buffer_pa & 0xfffffffff),
	((uintptr_t)g_dpewb_cost_int_Buffer_pa & 0xfffffffff),
	((uintptr_t)g_dpewb_asfrm_Buffer_pa & 0xfffffffff),
	((uintptr_t)g_dpewb_asfrmext_Buffer_pa & 0xfffffffff),
	((uintptr_t)g_dpewb_wmfhf_Buffer_pa & 0xfffffffff));
/*#define KERNEL_LOG*/
#ifdef KERNEL_LOG
    /* In EP, Add DPE_DBG_WRITE_REG for debug. Should remove it after EP */
	DPEInfo.DebugMask = (DPE_DBG_INT | DPE_DBG_DBGLOG | DPE_DBG_WRITE_REG);
#endif
EXIT:
	LOG_INF("- X. Ret: %d. UserCount: %d.", Ret, DPEInfo.UserCount);
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
	} else {
		spin_unlock(&(DPEInfo.SpinLockDPERef));
		dpe_unregister_requests(&dpe_reqs);
	}
	/*  */
	LOG_INF("Curr UsrCnt(%d), (process, pid, tgid)=(%s, %d, %d), last user",
		DPEInfo.UserCount, current->comm, current->pid, current->tgid);
	/* Disable clock. */
	DPE_EnableClock(MFALSE);
	LOG_INF("DPE release g_u4EnableClockCount: %d", g_u4EnableClockCount);
	/*  */
EXIT:
	LOG_DBG("- X. UserCount: %d.", DPEInfo.UserCount);
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
	/* .mmap = DPE_mmap, */
	.unlocked_ioctl = DPE_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = DPE_ioctl_compat,
#endif
};
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
 * V4L2
 ******************************************************************************/
static int dpe_fop_open(struct file *filp)
{
	DPE_open(NULL, filp);
	return 0;
}
static int dpe_fop_release(struct file *file)
{
	DPE_release(NULL, file);
	return 0;
}
unsigned int dpe_fop_poll(struct file *file, poll_table *wait)
{
	//struct DPE_Kernel_Config *pDpeConfig;
	//struct DPE_Kernel_Config DpeConfig;
	struct DPE_USER_INFO_STRUCT *pUserInfo;
	unsigned int buf_rdy;
	unsigned long flags;
	unsigned int p;

	//pDpeConfig = &DpeConfig;

	LOG_INF("DPE fop poll\n");

	//DPE_DumpUserSpaceReg(pDpeConfig);
	//LOG_INF("DPE_DumpReg star dpe_fop_poll!\n");
	//DPE_DumpReg();
	pUserInfo = (struct DPE_USER_INFO_STRUCT *) (file->private_data);
	poll_wait(file, &DPEInfo.WaitQueueHead, wait);
	buf_rdy = DPE_GetIRQState(DPE_IRQ_TYPE_INT_DVP_ST,
				0x0,
				DPE_INT_ST, DPE_PROCESS_ID_DPE,
				pUserInfo->Pid);
	p = pUserInfo->Pid % IRQ_USER_NUM_MAX;
	LOG_INF("buf_rdy = %d\n", buf_rdy);
	if (buf_rdy) {
		spin_lock_irqsave
		(&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]), flags);
		DPEInfo.IrqInfo.DpeIrqCnt[p]--;
if (DPEInfo.IrqInfo.DpeIrqCnt[p] == 0)
	DPEInfo.IrqInfo.Status[DPE_IRQ_TYPE_INT_DVP_ST] &= (~DPE_INT_ST);
spin_unlock_irqrestore(&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]), flags);
		return POLLIN | POLLRDNORM;
	} else
		return 0;
}
static const struct v4l2_file_operations vid_fops = {
	.owner          = THIS_MODULE,
	.open           = dpe_fop_open,
	.release        = dpe_fop_release,
	.poll           = dpe_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	/* TODO */
	//.mmap           = vb2_fop_mmap,
	//.read           = vb2_fop_read,
	//.write          = vb2_fop_write,
};
static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	/*struct video_device *vdev = video_devdata(file);*/
	unsigned long ret;
	struct DPE_USER_INFO_STRUCT *pUserInfo;
	struct DPE_Request ureq;
	struct DPE_Request kreq;
	/* size of cfgs = 3 owing to call stact limitation*/
	struct DPE_Config cfgs[3];//[MAX_FRAMES_PER_REQUEST];
	struct DPE_Config *pcfgs;
	unsigned long flags;
	unsigned int m_real_ReqNum, f;

	LOG_DBG("[%s]buf address/len = 0x%lx/0x%x\n",
		__func__, p->m.userptr,  p->length);
	pUserInfo = (struct DPE_USER_INFO_STRUCT *) (file->private_data);
	ret = copy_from_user(&ureq, (void __user *)p->m.userptr, sizeof(ureq));
	if (ret != 0)
		goto EXIT;
	//if (ureq.m_ReqNum > MAX_FRAMES_PER_REQUEST)
	if (ureq.m_ReqNum > 3)
		goto EXIT;
	LOG_INF("[%s]This request has %d configs.\n", __func__, ureq.m_ReqNum);
	if (ureq.m_pDpeConfig == NULL)
		goto EXIT;
	ret = copy_from_user(&cfgs[0], (void __user *)ureq.m_pDpeConfig,
				ureq.m_ReqNum * sizeof(struct DPE_Config));
	if (ret != 0)
		goto EXIT;
	m_real_ReqNum = ureq.m_ReqNum;
	for (f = 0; f < ureq.m_ReqNum; f++) {
		if (cfgs[f].Dpe_DVSSettings.is_pd_mode) {
			pcfgs = &cfgs[f];
			Get_Tile_Info(pcfgs);
		m_real_ReqNum += (cfgs[f].Dpe_DVSSettings.pd_frame_num-1);
		}
	}
	kreq.m_pDpeConfig = cfgs;
	kreq.m_ReqNum = m_real_ReqNum;
	//kreq.m_ReqNum = ureq.m_ReqNum;
	mutex_lock(&gDpeMutex);	/* Protect the Multi Process */
	spin_lock_irqsave(&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
			       flags);
	dpe_enque_request(&dpe_reqs, kreq.m_ReqNum, &kreq, pUserInfo->Pid);
	spin_unlock_irqrestore(&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
			       flags);
	/* Use a workqueue to set CMDQ to prevent HW CMDQ request
	 *  consuming speed from being faster than SW frame-queue update speed.
	 */
	if (!dpe_request_running(&dpe_reqs)) {
		LOG_DBG("direct request_handler\n");
		dpe_request_handler(&dpe_reqs,
		&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]));
	}
	mutex_unlock(&gDpeMutex);
EXIT:
	return ret;
}
static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	/*struct video_device *vdev = video_devdata(file);*/
	signed int Ret = 0;
	struct DPE_Request ureq;
	struct DPE_Request kreq;
	/* size of cfgs = 3 owing to call stact limitation*/
	struct DPE_Config cfgs[3];//[MAX_FRAMES_PER_REQUEST];
	unsigned long flags;
	int i;
	//struct DPE_Config *pDpeConfig;
	LOG_INF("DPE_DumpReg vidioc dpbuf\n");
	//DPE_DumpReg();//!test
	//LOG_INF("DPE_DumpReg end\n");
	if (copy_from_user
		(&ureq, (void __user *)p->m.userptr, sizeof(ureq)) == 0) {
		mutex_lock(&gDpeDequeMutex);
spin_lock_irqsave(&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
				flags);
		kreq.m_pDpeConfig = cfgs;
		dpe_deque_request(&dpe_reqs, &kreq.m_ReqNum, &kreq);
spin_unlock_irqrestore(&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]),
				flags);
		mutex_unlock(&gDpeDequeMutex);
		ureq.m_ReqNum = kreq.m_ReqNum;
		if (ureq.m_pDpeConfig == NULL) {
			LOG_ERR("NULL user pointer");
			Ret = -EFAULT;
			goto EXIT;
		}
		if (copy_to_user
		    ((void *)ureq.m_pDpeConfig, kreq.m_pDpeConfig,
		     kreq.m_ReqNum * sizeof(struct DPE_Config)) != 0) {
			LOG_ERR
			    ("DPE_DEQUE_REQ copy_to_user frameconfig failed\n");
			Ret = -EFAULT;
		}
		if (copy_to_user
		    ((void *)p->m.userptr, &ureq, sizeof(ureq)) != 0) {
			LOG_ERR("DPE_DEQUE_REQ copy_to_user failed\n");
			Ret = -EFAULT;
		}
	} else {
		LOG_ERR("DPE_CMD_DPE_DEQUE_REQ copy_from_user failed\n");
		Ret = -EFAULT;
	}
//!--------put fd
	if ((pDpeUserConfig->Dpe_engineSelect == MODE_DVS_ONLY) ||
	(pDpeUserConfig->Dpe_engineSelect == MODE_DVS_DVP_BOTH)) {
		for (i = 0 ; i < 6 ; i++)
			mmu_release(&DVS_mmu[i], i);

		if (DPE_P4_EN == 1) {
			for (i = 6 ; i < 10 ; i++)
				mmu_release(&DVS_mmu[i], i);
		}
		kfree((struct tee_mmu *)DVS_mmu);
	}
	if ((pDpeUserConfig->Dpe_engineSelect == MODE_DVP_ONLY) ||
	(pDpeUserConfig->Dpe_engineSelect == MODE_DVS_DVP_BOTH)) {
		for (i = 0 ; i < 7 ; i++)
			mmu_release(&DVP_mmu[i], i);

		if (DPE_is16BitMode == 1) {
			for (i = 7 ; i < 10 ; i++)
				mmu_release(&DVP_mmu[i], i);
		}
		kfree((struct tee_mmu *)DVP_mmu);
	}

	LOG_DBG("[%s]buf address/len = 0x%lx/0x%x\n",
		__func__, p->m.userptr,  p->length);
EXIT:
	return 0;
}
static int vidioc_querycap(struct file *file, void  *priv,
					struct v4l2_capability *cap)
{
	struct DPE_device *dev = video_drvdata(file);
	/*struct video_device *vdev = video_devdata(file);*/
	strlcpy(cap->driver, "dpe", sizeof(cap->driver));
	strlcpy(cap->card, "dpe", sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
			"platform:%s", dev->v4l2_dev.name);
	LOG_DBG("[%s]\n", __func__);
cap->device_caps =
	V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}
int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	/*struct DPE_device *dev = video_drvdata(file);*/
	f->fmt.pix.width       = 777;
	f->fmt.pix.height      = 555;
	f->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	f->fmt.pix.field       = V4L2_FIELD_INTERLACED;
	f->fmt.pix.sizeimage = f->fmt.pix.width * f->fmt.pix.height;
	LOG_ERR("[%s] sizeimage(%d)\n", __func__, f->fmt.pix.sizeimage);
	return 0;
}
static const struct v4l2_ioctl_ops vid_ioctl_ops = {
	.vidioc_querycap		   = vidioc_querycap,
	.vidioc_g_fmt_vid_cap	= vidioc_g_fmt_vid_cap,
	.vidioc_qbuf			   = vidioc_qbuf,
	.vidioc_dqbuf			   = vidioc_dqbuf,
	//.vidioc_reqbufs			= vb2_ioctl_reqbufs,
	//.vidioc_create_bufs		= vb2_ioctl_create_bufs,
	//.vidioc_prepare_buf		= vb2_ioctl_prepare_buf,
	//.vidioc_querybuf		   = vb2_ioctl_querybuf,
	//.vidioc_expbuf			   = vb2_ioctl_expbuf,
	//.vidioc_streamon		   = vb2_ioctl_streamon,
	//.vidioc_streamoff		   = vb2_ioctl_streamoff,
};
static void dpe_dev_release(struct v4l2_device *v4l2_dev)
{
	struct DPE_device *dev =
		container_of(v4l2_dev, struct DPE_device, v4l2_dev);
	v4l2_device_unregister(&dev->v4l2_dev);
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
	struct video_device *vfd = NULL;
	struct device_node *node;
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
							GFP_KERNEL|__GFP_ZERO);
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
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
		*(DPE_dev->dev->dma_mask) =
			(u64)DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT);
		DPE_dev->dev->coherent_dma_mask =
			(u64)DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT);
#endif
	LOG_INF("nr_DPE_devs=%d, devnode(%s), map_addr=0x%lx\n", nr_DPE_devs,
		pDev->dev.of_node->name, (unsigned long)DPE_dev->regs);
	//for cmdq malibox
	if (nr_DPE_devs == 1) {
		/* register device by node */
		//dpe_clt_base = cmdq_register_device(&pDev->dev);
		dpe_clt_base = NULL;
		/* request thread by index (in dts) 0 */
		dpe_clt = cmdq_mbox_create(&pDev->dev, 0);
		LOG_INF("[Debug]cmdq_mbox_create 0x%lx\n",
			(unsigned long)dpe_clt);
/* parse hardware event */
//dvs_event_id = cmdq_dev_get_event(&pDev->dev, "EVENT_IPE_DVS_DONE");
		of_property_read_u32(pDev->dev.of_node,
				"dvs_done_async_shot",
				&dvs_event_id);
		LOG_INF("[Debug]dvs_event_id %d\n", dvs_event_id);
	} else if (nr_DPE_devs == 2) {
/* parse hardware event */
//dvp_event_id = cmdq_dev_get_event(&pDev->dev, "EVENT_IPE_DVP_DONE");
		of_property_read_u32(pDev->dev.of_node,
				"dvp_done_async_shot",
				&dvp_event_id);
		LOG_INF("[Debug]dvp_event_id %d\n", dvp_event_id);
	}
	/* get IRQ ID and request IRQ */
	DPE_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);
if (DPE_dev->irq > 0) {
	/* Get IRQ Flag from device node */
	if (of_property_read_u32_array(pDev->dev.of_node,
		"interrupts",
		irq_info,
		ARRAY_SIZE(irq_info))) {
		dev_dbg(&pDev->dev, "get irq flags from DTS fail!!\n");
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
	if (nr_DPE_devs == 2) {
		/* Register char driver */
		Ret = DPE_RegCharDev();
		if (Ret) {
			dev_dbg(&pDev->dev, "register char failed");
			return Ret;
		}
#ifndef EP_NO_CLKMGR
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifdef SMI_CLK
		LOG_INF("nr_DPE_devs=%d, devnode(%s)\n", nr_DPE_devs,
		pDev->dev.of_node->name);
	/*larb12*/
		node = of_parse_phandle(pDev->dev.of_node, "mediatek,larb", 0);
		LOG_INF("larb12 node =%x\n", node);
		if (!node) {
			LOG_INF("no get larb12 node\n");
			return -EINVAL;
		}

		DPE_pdev = of_find_device_by_node(node);
		if (WARN_ON(!DPE_pdev)) {
			of_node_put(node);
			return -EINVAL;
		}
		of_node_put(node);
		DPE_devs->larb12 = &DPE_pdev->dev;
#endif
		/*CCF: Grab clock pointer (struct clk*) */
		LOG_INF(" get clock node star\n");
		dpe_clk.IMG_IPE = devm_clk_get(&pDev->dev,
							"IMG_IPE");
		if (IS_ERR(dpe_clk.IMG_IPE)) {
			LOG_INF("cannot get IMG_IPE clock\n");
			return PTR_ERR(dpe_clk.IMG_IPE);
		}
		dpe_clk.IPE_DPE = devm_clk_get(&pDev->dev,
							"IPE_DPE");
		if (IS_ERR(dpe_clk.IPE_DPE)) {
			LOG_INF("cannot get IPE_DPE clock\n");
			return PTR_ERR(dpe_clk.IPE_DPE);
		}
		dpe_clk.IPE_SMI_LARB12 = devm_clk_get(&pDev->dev,
							"IPE_SMI_LARB12");
		if (IS_ERR(dpe_clk.IPE_SMI_LARB12)) {
			LOG_INF("cannot get IPE_SMI_LARB12 clock\n");
			return PTR_ERR(dpe_clk.IPE_SMI_LARB12);
		}

		dpe_clk.IPE_TOP = devm_clk_get(&pDev->dev,
							"IPE_TOP");
		if (IS_ERR(dpe_clk.IPE_TOP)) {
			LOG_INF("cannot get IPE_TOP clock\n");
			return PTR_ERR(dpe_clk.IPE_TOP);
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
#ifdef KERNEL_DMA_BUFFER
	//!02/22
	gdev = &pDev->dev;
	if (dma_set_mask_and_coherent(gdev, DMA_BIT_MASK(34)))
		LOG_INF("%s: No suitable DMA available\n", __func__);

	kernel_dpebuf =
	vb2_dc_alloc(gdev, DMA_ATTR_WRITE_BARRIER, WB_TOTAL_SIZE,
	DMA_FROM_DEVICE, 0);
	dbuf = vb2_dc_get_dmabuf(kernel_dpebuf, O_RDWR);
	refcount_dec(&kernel_dpebuf->refcount);
	dpebuf =
	vb2_dc_attach_dmabuf(gdev, dbuf, WB_TOTAL_SIZE, DMA_FROM_DEVICE);
	if (vb2_dc_map_dmabuf(dpebuf) != 0)
		LOG_INF("Allocate Buffer Fail!");
	g_dpewb_dvme_int_Buffer_pa = (unsigned int *)dpebuf->dma_addr;
	g_dpewb_cost_int_Buffer_pa =
		(unsigned int *)(((uintptr_t)g_dpewb_dvme_int_Buffer_pa) +
		WB_INT_MEDV_SIZE);
	g_dpewb_asfrm_Buffer_pa =
		(unsigned int *)(((uintptr_t)g_dpewb_cost_int_Buffer_pa) +
		WB_DCV_L_SIZE);
	g_dpewb_asfrmext_Buffer_pa =
		(unsigned int *)(((uintptr_t)g_dpewb_asfrm_Buffer_pa) +
		WB_ASFRM_SIZE);
	g_dpewb_wmfhf_Buffer_pa =
		(unsigned int *)(((uintptr_t)g_dpewb_asfrmext_Buffer_pa) +
		WB_ASFRMExt_SIZE);
#endif
	pm_runtime_enable(gdev);








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
		//!wakeup_source_init(&DPE_wake_lock, "dpe_lock_wakelock"); //!
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
#if defined(DPE_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
		pm_qos_add_request(&dpe_pm_qos_request,
			PM_QOS_MM_MEMORY_BANDWIDTH, PM_QOS_DEFAULT_VALUE);
		cmdqCoreRegisterTaskCycleCB(CMDQ_GROUP_DPE, cmdq_pm_qos_start,
							cmdq_pm_qos_stop);
#endif
		seqlock_init(&(dpe_reqs.seqlock));
		snprintf(DPE_dev->v4l2_dev.name, sizeof(DPE_dev->v4l2_dev.name),
			"%s-%03d", DPE_DEV_NAME, 0);
		Ret = v4l2_device_register(&pDev->dev, &DPE_dev->v4l2_dev);

		if (Ret) {
			LOG_INF("Failed to register v4l2 device\n");
			return Ret;
		}
		LOG_INF("get v4l2_device_register = %d\n", Ret);

		DPE_dev->v4l2_dev.release = dpe_dev_release;
		/* initialize locks */
		mutex_init(&DPE_dev->mutex);
		vfd = &DPE_dev->vid_dpe_dev;
		memset(vfd, 0, sizeof(*vfd));
		strlcpy(vfd->name, "dpe-vid", sizeof(vfd->name));
		vfd->fops = &vid_fops;
		vfd->ioctl_ops = &vid_ioctl_ops;
		vfd->release = video_device_release_empty;
		vfd->v4l2_dev = &DPE_dev->v4l2_dev;
		vfd->vfl_dir = VFL_DIR_M2M;
		vfd->device_caps =
		V4L2_CAP_VIDEO_CAPTURE |
		V4L2_CAP_STREAMING |
		V4L2_CAP_READWRITE;
		/*
		 * Provide a mutex to v4l2 core. It will be used to protect
		 * all fops and v4l2 ioctls.
		 */
		vfd->lock = &DPE_dev->mutex;
		video_set_drvdata(vfd, DPE_dev);
		Ret = video_register_device(vfd, VFL_TYPE_GRABBER, -1);

		LOG_INF("video_register_device = %d\n", Ret);
		if (Ret < 0) {
			video_unregister_device(vfd);
			LOG_INF("video_register_device failed\n");
		}
	}
	g_DPE_PMState = 0;
EXIT:
	if (Ret < 0)
		DPE_UnregCharDev();
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
#ifdef KERNEL_DMA_BUFFER
	vb2_dc_unmap_dmabuf(dpebuf);
	vb2_dc_detach_dmabuf(dpebuf);
	vb2_dc_put(kernel_dpebuf);
	dpebuf = NULL;
	kernel_dpebuf = NULL;
	dbuf = NULL;
	gdev = NULL;
#endif
	/*  */
	device_destroy(pDPEClass, DPEDevNo);
	/*  */
	class_destroy(pDPEClass);
	pDPEClass = NULL;
	/*  */
#if defined(DPE_PMQOS_EN) && defined(CONFIG_MTK_QOS_SUPPORT)
	pm_qos_remove_request(&dpe_pm_qos_request);
#endif
	//video_unregister_device(&DPE_devs[nr_DPE_devs - 1].vid_dpe_dev);
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
/* fix unexpected close clock issue */
#ifdef dpe_dump_read_en
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
//	seq_printf(m, "[0x%08X %08X]\n", (unsigned int)(DVS_FRM_STATUS2_HW),
//		   (unsigned int)DPE_RD32(DVS_FRM_STATUS2_REG));
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
#endif
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
/* fix unexpected close clock issue */
#ifdef dpe_dump_read_en
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
#endif
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
	spin_lock(&(DPEInfo.SpinLockDPE));
	if (g_u4EnableClockCount == 0) {
		spin_unlock(&(DPEInfo.SpinLockDPE));
		return 0;
	}
	spin_unlock(&(DPEInfo.SpinLockDPE));
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
#ifdef dpe_dump_read_en
			if (kstrtoint(addrSzBuf, 0, &addr) != 0)
				LOG_ERR("scan decimal addr is wrong !!:%s",
								addrSzBuf);
#else
				LOG_ERR("hex address only:%s", addrSzBuf);
#endif
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
#ifdef dpe_dump_read_en
			if (kstrtoint(valSzBuf, 0, &val) != 0)
				LOG_ERR("scan decimal value is wrong !!:%s",
								valSzBuf);
#else
				LOG_ERR("HEX address only :%s", valSzBuf);
#endif
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
#ifdef dpe_dump_read_en
			if (kstrtoint(addrSzBuf, 0, &addr) != 0)
				LOG_ERR("scan decimal addr is wrong !!:%s",
								addrSzBuf);
#else
				LOG_ERR("HEX address only :%s", addrSzBuf);
#endif
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
/**************************************************************
 *
 **************************************************************/
#ifdef CONFIG_MTK_M4U
#ifdef CONFIG_MTK_IOMMU_V2
enum mtk_iommu_callback_ret_t DPE_M4U_TranslationFault_callback(int port,
	unsigned long mva, void *data)
#else
enum m4u_callback_ret_t DPE_M4U_TranslationFault_callback(int port,
	unsigned int mva, void *data)
#endif
{
	pr_info("[ISP_M4U]fault call port=%d, mva=0x%lx", port, mva);
	switch (port) {
	default:
		DPE_DumpReg();
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
static signed int __init DPE_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
	/* FIX-ME: linux-3.10 procfs API changed */
	/* use proc_create */
#ifdef dpe_dump_read_en
	struct proc_dir_entry *proc_entry;
	struct proc_dir_entry *isp_dpe_dir;
#endif
	int i;
	/*  */
	LOG_INF("- E. DPE Init");
	/*  */
	Ret = platform_driver_register(&DPEDriver);
	if (Ret < 0) {
		LOG_ERR("platform_driver_register fail");
		return Ret;
	}
#ifdef dpe_dump_read_en
	//struct device_node *node = NULL;
	//node = of_find_compatible_node(NULL, NULL, "mediatek,DPE");
	//if (!node) {
		//LOG_ERR("find mediatek,DPE node failed!!!\n");
		//return -ENODEV;
	//}
	//ISP_DPE_BASE = of_iomap(node, 0);
	//if (!ISP_DPE_BASE) {
		//LOG_ERR("unable to map ISP_DPE_BASE registers!!!\n");
		//return -ENODEV;
	//}
	//LOG_DBG("ISP_DPE_BASE: %lx\n", ISP_DPE_BASE);
#endif
#ifdef dpe_dump_read_en
	//isp_dpe_dir = proc_mkdir("dpe", NULL);
	//if (!isp_dpe_dir) {
		//LOG_ERR("[%s]: fail to mkdir /proc/dpe\n", __func__);
		//return 0;
	//}
	//proc_entry = proc_create("dpe_dump", 0444, isp_dpe_dir,
							//&dpe_dump_proc_fops);
	//proc_entry = proc_create("dpe_reg", 0644, isp_dpe_dir,
							//&dpe_reg_proc_fops);
#endif
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
#ifdef dpe_dump_read_en
	/* Cmdq */
	/* Register DPE callback */
	LOG_INF("register dpe callback for CMDQ");
	cmdqCoreRegisterCB(CMDQ_GROUP_DPE,
			   DPE_ClockOnCallback,
			   DPE_DumpCallback, DPE_ResetCallback,
							DPE_ClockOffCallback);
#endif
LOG_INF("CONFIG_MTK_M4U");
#ifdef CONFIG_MTK_M4U
LOG_INF("MTK_DPE_VER = %d", MTK_DPE_VER);
#ifdef CONFIG_MTK_IOMMU_V2
	#if (MTK_DPE_VER == 0)
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IMG_DVP_WDMA,
					  DPE_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IMG_DVP_RDMA,
					  DPE_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IMG_DVS_WDMA,
					  DPE_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IMG_DVS_RDMA,
					  DPE_M4U_TranslationFault_callback,
					  NULL);
	#else
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IMG_DVP_RDMA,
					  DPE_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IMG_DVP_WDMA,
					  DPE_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IMG_DVS_WDMA,
					  DPE_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IMG_DVS_RDMA,
					  DPE_M4U_TranslationFault_callback,
					  NULL);
	#endif
#else
	#if (MTK_DPE_VER == 0)
	LOG_INF("m4u_register_fault_callback");
	m4u_register_fault_callback(M4U_PORT_L12_IMG_DVS_RDMA,
			DPE_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_L12_IMG_DVS_WDMA,
			DPE_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_L12_IMG_DVP_RDMA,
			DPE_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_L12_IMG_DVP_WDMA,
			DPE_M4U_TranslationFault_callback, NULL);
	#endif
#endif
#endif
	LOG_INF("- X. DPE Init Ret: %d.", Ret);
	return Ret;
}
/*******************************************************************************
 *
 ******************************************************************************/
static void __exit DPE_Exit(void)
{
	/*int i;*/
	LOG_DBG("- E.");
	/*  */
	platform_driver_unregister(&DPEDriver);
	/*  */
	/* Cmdq */
	/* Unregister DPE callback */
	//cmdqCoreRegisterCB(CMDQ_GROUP_DPE, NULL, NULL, NULL, NULL);
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
	unsigned int DvsStatus, DvpStatus;
	bool bResulst = MFALSE;
	bool isDvpDone = MFALSE;
	pid_t ProcessID;
	unsigned int p = 0;

	DvsStatus = DPE_RD32(DVS_CTRL_STATUS0_REG);	/* DVS Status */
	DvpStatus = DPE_RD32(DVP_CTRL_STATUS0_REG);	/* DVP Status */
	 LOG_INF("DVP IRQ, DvsStatus: 0x%08x, DvpStatus: 0x%08x\n",
	 DvsStatus, DvpStatus);
	/* DPE done status may rise later, so can't use done status now  */
	/* if (DPE_INT_ST == (DPE_INT_ST & DvpStatus)) { */
		DPE_WR32(DVP_IRQ_00_REG, 0x040000F0); /* Clear DVP IRQ */
		DPE_WR32(DVP_IRQ_00_REG, 0x04000E00);
		isDvpDone = MTRUE;
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
			p = ProcessID % IRQ_USER_NUM_MAX;
			DPEInfo.IrqInfo.Status[DPE_IRQ_TYPE_INT_DVP_ST] |=
				DPE_INT_ST;
			DPEInfo.IrqInfo.ProcessID[p] =
				ProcessID;
			DPEInfo.IrqInfo.DpeIrqCnt[p]++;
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
	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(
		DPE_IRQ_TYPE_INT_DVP_ST,
		m_CurrentPPB,
		_LOG_INF,
		"IRQ:%d,0x%x:0x%x,0x%x:0x%x,Result:%d\n",
		Irq,
		DVS_CTRL_STATUS0_HW,
		DvsStatus,
		DVP_CTRL_STATUS0_HW,
		DvpStatus,
		bResulst);
	IRQ_LOG_KEEPER(
		DPE_IRQ_TYPE_INT_DVP_ST,
		m_CurrentPPB,
		_LOG_INF,
		"IrqCnt[%d]:0x%x,WReq:0x%x,RReq:0x%x\n",
		p, DPEInfo.IrqInfo.DpeIrqCnt[p],
		DPEInfo.WriteReqIdx,
		DPEInfo.ReadReqIdx);
	#if (REQUEST_REGULATION == FRAME_BASE_REGULATION)
	/* schedule_work(&DPEInfo.ScheduleDpeWork); */
	queue_work(DPEInfo.wkqueue, &DPEInfo.ScheduleDpeWork);
	#endif
	if (isDvpDone == MTRUE)

		schedule_work(&logWork);

		//tasklet_schedule(DPE_tasklet[DPE_IRQ_TYPE_INT_DVP_ST].pDPE_tkt);

	return IRQ_HANDLED;
}
static irqreturn_t ISP_Irq_DVS(signed int Irq, void *DeviceId)
{
	unsigned int DvsStatus, DvpStatus;
	bool bResulst = MFALSE;
	bool isDvsDone = MFALSE;
	pid_t ProcessID;
	unsigned int p = 0;

	DvsStatus = DPE_RD32(DVS_CTRL_STATUS0_REG);	/* DVS Status */
	DvpStatus = DPE_RD32(DVP_CTRL_STATUS0_REG);	/* DVP Status */
	 LOG_INF("DVS IRQ, DvsStatus: 0x%08x, DvpStatus: 0x%08x\n",
	DvsStatus, DvpStatus);
	/* DPE done status may rise later, so can't use done status now  */
	/* if (DPE_INT_ST == (DPE_INT_ST & DvsStatus)) { */
		DPE_WR32(DVS_IRQ_00_REG, 0x040000F0); /* Clear DVS IRQ */
		DPE_WR32(DVS_IRQ_00_REG, 0x04000E00);
		isDvsDone = MTRUE;
	/* } */
	spin_lock(&(DPEInfo.SpinLockIrq[DPE_IRQ_TYPE_INT_DVP_ST]));
	if (isDvsDone == MTRUE) {
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
			p = ProcessID % IRQ_USER_NUM_MAX;
			DPEInfo.IrqInfo.Status[DPE_IRQ_TYPE_INT_DVP_ST] |=
				DPE_INT_ST;
			DPEInfo.IrqInfo.ProcessID[p] =
				ProcessID;
			DPEInfo.IrqInfo.DpeIrqCnt[p]++;
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
	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(
		DPE_IRQ_TYPE_INT_DVS_ST,
		m_CurrentPPB,
		_LOG_INF,
		"IRQ:%d,0x%x:0x%x,0x%x:0x%x,Result:%d\n",
		Irq,
		DVS_CTRL_STATUS0_HW,
		DvsStatus,
		DVP_CTRL_STATUS0_HW,
		DvpStatus,
		bResulst);
	IRQ_LOG_KEEPER(
		DPE_IRQ_TYPE_INT_DVS_ST,
		m_CurrentPPB,
		_LOG_INF,
		"IrqCnt[%d]:0x%x,WReq:0x%x,RReq:0x%x\n",
		p, DPEInfo.IrqInfo.DpeIrqCnt[p],
		DPEInfo.WriteReqIdx,
		DPEInfo.ReadReqIdx);
	#if (REQUEST_REGULATION == FRAME_BASE_REGULATION)
	/* schedule_work(&DPEInfo.ScheduleDpeWork); */
	queue_work(DPEInfo.wkqueue, &DPEInfo.ScheduleDpeWork);
	#endif
	if (isDvsDone == MTRUE)
//#if 1
		schedule_work(&logWork);
//#else
		//tasklet_schedule(DPE_tasklet[DPE_IRQ_TYPE_INT_DVP_ST].pDPE_tkt);
//#endif
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
MODULE_AUTHOR("MM3SW2");
MODULE_LICENSE("GPL");
