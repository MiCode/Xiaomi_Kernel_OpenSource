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

/******************************************************************************
 * camera_isp.c - MT6769 Linux ISP Device Driver
 *
 * DESCRIPTION:
 *     This file provid the other drivers ISP relative functions
 *
 ******************************************************************************/
#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>  /* proc file use */
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h> /*for kernel log reduction*/
#include <mt-plat/sync_write.h> /* For mt65xx_reg_sync_writel(). */


/* MET: define to enable MET*/
/*#define ISP_MET_READY*/

#define EP_NO_K_LOG_ADJUST
#ifdef EP_STAGE
/* disable SMI related for EP */
#define EP_MARK_SMI
/* For early if load dont need to use camera*/
#define DUMMY_INT
/* If PMQoS is not ready on EP stage */
#define EP_NO_PMQOS
/* Clkmgr is not ready in early porting, en/disable clock  by hardcode */
#define EP_NO_CLKMGR
/* EP no need to adjust upper bound of kernel log count */
#define EP_NO_K_LOG_ADJUST
#endif

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_platform.h>  /* for device tree */
#include <linux/of_irq.h>       /* for device tree */
#include <linux/of_address.h>   /* for device tree */
#endif

#if defined(ISP_MET_READY)
#define CREATE_TRACE_POINTS
#include "inc/met_events_camsys.h"
#endif

#ifndef EP_MARK_SMI
#include <smi_public.h>
/*for SMI BW debug log*/
#endif

#include "inc/camera_isp.h"

#ifndef EP_NO_PMQOS /* EP_NO_PMQOS is equivalent to EP_MARK_MMDVFS */
//#include <mmdvfs_mgr.h>
#ifdef CONFIG_MTK_QOS_SUPPORT
#include <mmdvfs_pmqos.h>
#endif
#include <linux/pm_qos.h>
/* Use this qos request to control camera dynamic frequency change */
#ifdef CONFIG_MTK_QOS_SUPPORT
struct pm_qos_request isp_qos;
struct pm_qos_request camsys_qos_request[ISP_IRQ_TYPE_INT_CAM_C_ST+1];
static struct ISP_PM_QOS_STRUCT G_PM_QOS[ISP_IRQ_TYPE_INT_CAM_C_ST+1];
static u32 PMQoS_BW_value;
#else
struct mmdvfs_pm_qos_request isp_qos;
#endif

static u32 target_clk;
#endif

#include <archcounter_timesync.h>

/*  */
#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif

#define ISP_DEV_NAME                "camera-isp"
#define SMI_LARB_MMU_CTL            (1)
/*#define ENABLE_WAITIRQ_LOG*/ /* wait irq debug logs */
/*#define ENABLE_STT_IRQ_LOG*/  /*show STT irq debug logs */

/* Queue timestamp for deque. Update when non-drop frame @SOF */
#define TIMESTAMP_QUEUE_EN          (0)
#if (TIMESTAMP_QUEUE_EN == 1)
#define TSTMP_SUBSAMPLE_INTPL		(1)
#else
#define TSTMP_SUBSAMPLE_INTPL		(0)
#endif
#define ISP_BOTTOMHALF_WORKQ		(1)

#if (ISP_BOTTOMHALF_WORKQ == 1)
#include <linux/workqueue.h>
#endif

/* ---------------------------------------------------------------------------*/

#define MyTag "[ISP]"
#define IRQTag "KEEPER"

#define LOG_VRB(format, args...) \
		pr_debug(MyTag "[%s] " format, __func__, ##args)

#define ISP_DEBUG
#ifdef ISP_DEBUG
#define LOG_DBG(format, args...) \
		pr_info(MyTag "[%s] " format, __func__, ##args)
#else
#define LOG_DBG(format, args...)
#endif

#define LOG_INF(format, args...) \
		pr_info(MyTag "[%s] " format, __func__, ##args)
#define LOG_NOTICE(format, args...) \
		pr_notice(MyTag "[%s] " format, __func__, ##args)


/******************************************************************************
 *
 ******************************************************************************/
/* #define ISP_WR32(addr, data)    iowrite32(data, addr) //For other projects.*/
/* For 89 Only.*/   /* NEED_TUNING_BY_PROJECT */
#define ISP_WR32(addr, data)    mt_reg_sync_writel(data, addr)
#define ISP_RD32(addr)                  ioread32((void *)addr)

/******************************************************************************
 *
 ******************************************************************************/
/* dynamic log level */
#define ISP_DBG_INT                 (0x00000001)
#define ISP_DBG_READ_REG            (0x00000004)
#define ISP_DBG_WRITE_REG           (0x00000008)
#define ISP_DBG_CLK                 (0x00000010)
#define ISP_DBG_TASKLET             (0x00000020)
#define ISP_DBG_SCHEDULE_WORK       (0x00000040)
#define ISP_DBG_BUF_WRITE           (0x00000080)
#define ISP_DBG_BUF_CTRL            (0x00000100)
#define ISP_DBG_REF_CNT_CTRL        (0x00000200)
#define ISP_DBG_INT_2               (0x00000400)
#define ISP_DBG_INT_3               (0x00000800)
#define ISP_DBG_HW_DON              (0x00001000)
#define ISP_DBG_ION_CTRL            (0x00002000)

/******************************************************************************
 *
 ******************************************************************************/
static irqreturn_t ISP_Irq_CAM(enum ISP_IRQ_TYPE_ENUM irq_module);
static irqreturn_t ISP_Irq_CAM_A(int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAM_B(int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAM_C(int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAMSV(enum ISP_IRQ_TYPE_ENUM irq_module,
		enum ISP_DEV_NODE_ENUM cam_idx, const char *str);
static irqreturn_t ISP_Irq_CAMSV_0(int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAMSV_1(int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAMSV_2(int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAMSV_3(int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAMSV_4(int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAMSV_5(int  Irq, void *DeviceId);


typedef irqreturn_t (*IRQ_CB)(int, void *);

struct ISR_TABLE {
	IRQ_CB          isr_fp;
	unsigned int    int_number;
	char            device_name[16];
};

#ifndef CONFIG_OF
const struct ISR_TABLE IRQ_CB_TBL[ISP_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_CAM_A,     CAM0_IRQ_BIT_ID,    "CAM_A"},
	{NULL,                            0,    "CAM_B"},
	{ISP_Irq_CAMSV_0,   CAM_SV0_IRQ_BIT_ID, "CAMSV_0"},
	{ISP_Irq_CAMSV_1,   CAM_SV1_IRQ_BIT_ID, "CAMSV_1"},
	{NULL,                               0,     "UNI"}
};

#else
/* int number is got from kernel api */

const struct ISR_TABLE IRQ_CB_TBL[ISP_IRQ_TYPE_AMOUNT] = {
#ifdef DUMMY_INT
	/* Must be the same name with that in device node. */
	{ISP_Irq_CAM_A,     0,  "cam2-dummy"},
	{ISP_Irq_CAM_B,     0,  "cam3-dummy"},
	{ISP_Irq_CAM_C,     0,  "cam4-dummy"},
	{ISP_Irq_CAMSV_0,   0,  "camsv1-dummy"},
	{ISP_Irq_CAMSV_1,   0,  "camsv2-dummy"},
	{ISP_Irq_CAMSV_2,   0,  "camsv3-dummy"},
	{ISP_Irq_CAMSV_3,   0,  "camsv4-dummy"},
	{ISP_Irq_CAMSV_4,   0,  "camsv5-dummy"},
	{ISP_Irq_CAMSV_5,   0,  "camsv6-dummy"},
	{NULL,              0,  "cam1-dummy"} /* UNI */
#else
	/* Must be the same name with that in device node. */
	{ISP_Irq_CAM_A,     0,  "cam2"},
	{ISP_Irq_CAM_B,     0,  "cam3"},
	{ISP_Irq_CAM_C,     0,  "cam4"},
	{ISP_Irq_CAMSV_0,   0,  "camsv1"},
	{ISP_Irq_CAMSV_1,   0,  "camsv2"},
	{ISP_Irq_CAMSV_2,   0,  "camsv3"},
	{ISP_Irq_CAMSV_3,   0,  "camsv4"},
	{ISP_Irq_CAMSV_4,   0,  "camsv5"},
	{ISP_Irq_CAMSV_5,   0,  "camsv6"},
	{NULL,              0,  "cam1"} /* UNI */
#endif
};

/*
 * Note!!! The order and member of .compatible must be the same with that in
 *  "ISP_DEV_NODE_ENUM" in camera_isp.h
 */
static const struct of_device_id isp_of_ids[] = {
	{ .compatible = "mediatek,camsys", },
	{ .compatible = "mediatek,cam1", },
	{ .compatible = "mediatek,cam2", },
	{ .compatible = "mediatek,cam3", },
	{ .compatible = "mediatek,cam4", },
	{ .compatible = "mediatek,camsv1", },
	{ .compatible = "mediatek,camsv2", },
	{ .compatible = "mediatek,camsv3", },
	{ .compatible = "mediatek,camsv4", },
	{ .compatible = "mediatek,camsv5", },
	{ .compatible = "mediatek,camsv6", },
	{}
};

#endif
/* ///////////////////////////////////////////////////////////////////////////*/
/*  */
typedef void (*tasklet_cb)(unsigned long);
struct Tasklet_table {
	tasklet_cb tkt_cb;
	struct tasklet_struct  *pIsp_tkt;
};

struct tasklet_struct tkt[ISP_IRQ_TYPE_AMOUNT];

static void ISP_TaskletFunc_CAM_A(unsigned long data);
static void ISP_TaskletFunc_CAM_B(unsigned long data);
static void ISP_TaskletFunc_SV_0(unsigned long data);
static void ISP_TaskletFunc_SV_1(unsigned long data);
static void ISP_TaskletFunc_SV_2(unsigned long data);
static void ISP_TaskletFunc_SV_3(unsigned long data);
static void ISP_TaskletFunc_SV_4(unsigned long data);
static void ISP_TaskletFunc_SV_5(unsigned long data);

static struct Tasklet_table isp_tasklet[ISP_IRQ_TYPE_AMOUNT] = {
	{ISP_TaskletFunc_CAM_A, &tkt[ISP_IRQ_TYPE_INT_CAM_A_ST]},
	{ISP_TaskletFunc_CAM_B, &tkt[ISP_IRQ_TYPE_INT_CAM_B_ST]},
	{NULL,                  &tkt[ISP_IRQ_TYPE_INT_CAM_C_ST]},
	{ISP_TaskletFunc_SV_0,  &tkt[ISP_IRQ_TYPE_INT_CAMSV_0_ST]},
	{ISP_TaskletFunc_SV_1,  &tkt[ISP_IRQ_TYPE_INT_CAMSV_1_ST]},
	{ISP_TaskletFunc_SV_2,  &tkt[ISP_IRQ_TYPE_INT_CAMSV_2_ST]},
	{ISP_TaskletFunc_SV_3,  &tkt[ISP_IRQ_TYPE_INT_CAMSV_3_ST]},
	{ISP_TaskletFunc_SV_4,  &tkt[ISP_IRQ_TYPE_INT_CAMSV_4_ST]},
	{ISP_TaskletFunc_SV_5,  &tkt[ISP_IRQ_TYPE_INT_CAMSV_5_ST]},
	{NULL,                  &tkt[ISP_IRQ_TYPE_INT_UNI_A_ST]}
};

#if (ISP_BOTTOMHALF_WORKQ == 1)
struct IspWorkqueTable {
	enum ISP_IRQ_TYPE_ENUM	module;
	struct work_struct  isp_bh_work;
};

static void ISP_BH_Workqueue(struct work_struct *pWork);

static struct IspWorkqueTable isp_workque[ISP_IRQ_TYPE_AMOUNT] = {
	{ISP_IRQ_TYPE_INT_CAM_A_ST},
	{ISP_IRQ_TYPE_INT_CAM_B_ST},
	{ISP_IRQ_TYPE_INT_CAM_C_ST},
	{ISP_IRQ_TYPE_INT_CAMSV_0_ST},
	{ISP_IRQ_TYPE_INT_CAMSV_1_ST},
	{ISP_IRQ_TYPE_INT_CAMSV_2_ST},
	{ISP_IRQ_TYPE_INT_CAMSV_3_ST},
	{ISP_IRQ_TYPE_INT_CAMSV_4_ST},
	{ISP_IRQ_TYPE_INT_CAMSV_5_ST},
	{ISP_IRQ_TYPE_INT_UNI_A_ST}
};
#endif


#ifdef CONFIG_OF

#include <linux/clk.h>
struct ISP_CLK_STRUCT {
	struct clk *ISP_SCP_SYS_DIS;
	struct clk *ISP_SCP_SYS_ISP;
	struct clk *ISP_SCP_SYS_CAM;
	struct clk *ISP_CAM_CAMSYS;
	struct clk *ISP_CAM_CAMTG;
	struct clk *ISP_CAM_CAMSV0;
	struct clk *ISP_CAM_CAMSV1;
	struct clk *ISP_CAM_CAMSV2;
};
struct ISP_CLK_STRUCT isp_clk;

struct isp_device {
	void __iomem *regs;
	struct device *dev;
	int irq;
};

struct isp_sec_dapc_reg {
	unsigned int CAM_REG_CTL_EN[ISP_DEV_NODE_NUM];
	unsigned int CAM_REG_CTL_DMA_EN[ISP_DEV_NODE_NUM];
	unsigned int CAM_REG_CTL_SEL[ISP_DEV_NODE_NUM];
	unsigned int CAM_REG_CTL_EN2[ISP_DEV_NODE_NUM];
};

static struct isp_device *isp_devs;
static int nr_isp_devs;
static unsigned int m_CurrentPPB;
static struct isp_sec_dapc_reg lock_reg;
static unsigned int sec_on;

#ifdef CONFIG_PM_WAKELOCKS
struct wakeup_source isp_wake_lock;
#else
struct wake_lock isp_wake_lock;
#endif
static int g_WaitLockCt;


/* Get HW modules' base address from device nodes */
#define ISP_CAMSYS_CONFIG_BASE          (isp_devs[ISP_CAMSYS_CONFIG_IDX].regs)
#define ISP_CAM_A_BASE                  (isp_devs[ISP_CAM_A_IDX].regs)
#define ISP_CAM_B_BASE                  (isp_devs[ISP_CAM_B_IDX].regs)
#define ISP_CAM_C_BASE                  (isp_devs[ISP_CAM_C_IDX].regs)
#define ISP_CAMSV0_BASE                 (isp_devs[ISP_CAMSV0_IDX].regs)
#define ISP_CAMSV1_BASE                 (isp_devs[ISP_CAMSV1_IDX].regs)
#define ISP_CAMSV2_BASE                 (isp_devs[ISP_CAMSV2_IDX].regs)
#define ISP_CAMSV3_BASE                 (isp_devs[ISP_CAMSV3_IDX].regs)
#define ISP_CAMSV4_BASE                 (isp_devs[ISP_CAMSV4_IDX].regs)
#define ISP_CAMSV5_BASE                 (isp_devs[ISP_CAMSV5_IDX].regs)
#define ISP_CAM_UNI_BASE                (isp_devs[ISP_UNI_A_IDX].regs)

#include "inc/cam_regs.h"

#if (SMI_LARB_MMU_CTL == 1)
void __iomem *SMI_LARB_BASE[8];
#endif

#endif

struct S_START_T {
	unsigned int sec;
	unsigned int usec;
};


/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32
static  spinlock_t      SpinLock_UserKey;


#if (TIMESTAMP_QUEUE_EN == 1)
static int32_t ISP_PopBufTimestamp(
		unsigned int module,
		unsigned int dma_id,
		struct S_START_T *pTstp);
static int32_t ISP_WaitTimestampReady(
		unsigned int module,
		unsigned int dma_id);
#endif

/******************************************************************************
 *
 ******************************************************************************/
/* internal data */
/* pointer to the kmalloc'd area, rounded up to a page boundary */
static int *pTbl_RTBuf[ISP_IRQ_TYPE_AMOUNT];
static int Tbl_RTBuf_MMPSize[ISP_IRQ_TYPE_AMOUNT];

/* original pointer for kmalloc'd area as returned by kmalloc */
static void *pBuf_kmalloc[ISP_IRQ_TYPE_AMOUNT];
/*  */
static struct ISP_RT_BUF_STRUCT *pstRTBuf[ISP_IRQ_TYPE_AMOUNT] = {NULL};

static unsigned int G_u4EnableClockCount;
static atomic_t G_u4DevNodeCt;

int pr_detect_count;

/*save ion fd*/
#define ENABLE_KEEP_ION_HANDLE

#ifdef ENABLE_KEEP_ION_HANDLE
#define _ion_keep_max_   (64)/*32*/
#include "ion_drv.h" /*g_ion_device*/
static struct ion_client *pIon_client;
static struct mutex ion_client_mutex;
static int G_WRDMA_IonCt[ISP_CAMSV0_IDX - ISP_CAM_A_IDX]
			[_dma_max_wr_*_ion_keep_max_];
static int G_WRDMA_IonFd[ISP_CAMSV0_IDX - ISP_CAM_A_IDX]
			[_dma_max_wr_*_ion_keep_max_];
static struct ion_handle *G_WRDMA_IonHnd[ISP_CAMSV0_IDX - ISP_CAM_A_IDX]
					[_dma_max_wr_*_ion_keep_max_];
/* protect G_WRDMA_IonHnd & G_WRDMA_IonFd */
static spinlock_t SpinLock_IonHnd[ISP_CAMSV0_IDX - ISP_CAM_A_IDX][_dma_max_wr_];

struct T_ION_TBL {
	enum ISP_DEV_NODE_ENUM node;
	int *pIonCt;
	int *pIonFd;
	struct ion_handle **pIonHnd;
	spinlock_t *pLock;
};

static struct T_ION_TBL gION_TBL[ISP_DEV_NODE_NUM] = {
	{ISP_DEV_NODE_NUM, NULL, NULL, NULL, NULL},
	{ISP_DEV_NODE_NUM, NULL, NULL, NULL, NULL},
	{
	  ISP_CAM_A_IDX, (int *)G_WRDMA_IonCt[ISP_CAM_A_IDX - ISP_CAM_A_IDX],
	  (int *)G_WRDMA_IonFd[ISP_CAM_A_IDX - ISP_CAM_A_IDX],
	  (struct ion_handle **)G_WRDMA_IonHnd[ISP_CAM_A_IDX - ISP_CAM_A_IDX],
	  (spinlock_t *)SpinLock_IonHnd[ISP_CAM_A_IDX - ISP_CAM_A_IDX]
	},
	{
	  ISP_CAM_B_IDX, (int *)G_WRDMA_IonCt[ISP_CAM_B_IDX - ISP_CAM_A_IDX],
	  (int *)G_WRDMA_IonFd[ISP_CAM_B_IDX - ISP_CAM_A_IDX],
	  (struct ion_handle **)G_WRDMA_IonHnd[ISP_CAM_B_IDX - ISP_CAM_A_IDX],
	  (spinlock_t *)SpinLock_IonHnd[ISP_CAM_B_IDX - ISP_CAM_A_IDX]
	},
	{
	  ISP_CAM_C_IDX, (int *)G_WRDMA_IonCt[ISP_CAM_C_IDX - ISP_CAM_A_IDX],
	  (int *)G_WRDMA_IonFd[ISP_CAM_C_IDX - ISP_CAM_A_IDX],
	  (struct ion_handle **)G_WRDMA_IonHnd[ISP_CAM_C_IDX - ISP_CAM_A_IDX],
	  (spinlock_t *)SpinLock_IonHnd[ISP_CAM_C_IDX - ISP_CAM_A_IDX]
	},
	{ISP_DEV_NODE_NUM, NULL, NULL, NULL, NULL},
	{ISP_DEV_NODE_NUM, NULL, NULL, NULL, NULL},
	{ISP_DEV_NODE_NUM, NULL, NULL, NULL, NULL},
	{ISP_DEV_NODE_NUM, NULL, NULL, NULL, NULL},
	{ISP_DEV_NODE_NUM, NULL, NULL, NULL, NULL},
	{ISP_DEV_NODE_NUM, NULL, NULL, NULL, NULL}
};
#endif
/******************************************************************************
 *
 ******************************************************************************/
struct ISP_USER_INFO_STRUCT {
	pid_t   Pid;
	pid_t   Tid;
};

/******************************************************************************
 *
 ******************************************************************************/
#define ISP_BUF_SIZE            (4096)
#define ISP_BUF_WRITE_AMOUNT    6

enum ISP_BUF_STATUS_ENUM {
	ISP_BUF_STATUS_EMPTY,
	ISP_BUF_STATUS_HOLD,
	ISP_BUF_STATUS_READY
};

struct ISP_BUF_STRUCT {
	enum ISP_BUF_STATUS_ENUM Status;
	unsigned int                Size;
	unsigned char *pData;
};

struct ISP_BUF_INFO_STRUCT {
	struct ISP_BUF_STRUCT      Read;
	struct ISP_BUF_STRUCT      Write[ISP_BUF_WRITE_AMOUNT];
};


/******************************************************************************
 *
 ******************************************************************************/
#define ISP_ISR_MAX_NUM 32
#define INT_ERR_WARN_TIMER_THREAS 1000
#define INT_ERR_WARN_MAX_TIME 1

struct ISP_IRQ_ERR_WAN_CNT_STRUCT {
	/* cnt for each err int # */
	unsigned int m_err_int_cnt[ISP_IRQ_TYPE_AMOUNT][ISP_ISR_MAX_NUM];
	/* cnt for each warning int # */
	unsigned int m_warn_int_cnt[ISP_IRQ_TYPE_AMOUNT][ISP_ISR_MAX_NUM];
	/* mark for err int, where its cnt > threshold */
	unsigned int m_err_int_mark[ISP_IRQ_TYPE_AMOUNT];
	/* mark for warn int, where its cnt > threshold */
	unsigned int m_warn_int_mark[ISP_IRQ_TYPE_AMOUNT];
	unsigned long m_int_usec[ISP_IRQ_TYPE_AMOUNT];
};

static int FirstUnusedIrqUserKey = 1;
#define USERKEY_STR_LEN 128

struct UserKeyInfo {
	/* name for the user that register a userKey */
	char userName[USERKEY_STR_LEN];
	/* the user key for that user */
	int userKey;
};
/* array for recording the user name for a specific user key */
static struct UserKeyInfo IrqUserKey_UserInfo[IRQ_USER_NUM_MAX];

struct ISP_IRQ_INFO_STRUCT {
	/* Add an extra index for status type -> signal or dma */
	unsigned int Status[ISP_IRQ_TYPE_AMOUNT]
			   [ISP_IRQ_ST_AMOUNT]
			   [IRQ_USER_NUM_MAX];
	unsigned int    Mask[ISP_IRQ_TYPE_AMOUNT][ISP_IRQ_ST_AMOUNT];
	unsigned int    ErrMask[ISP_IRQ_TYPE_AMOUNT][ISP_IRQ_ST_AMOUNT];
	unsigned int    WarnMask[ISP_IRQ_TYPE_AMOUNT][ISP_IRQ_ST_AMOUNT];
	unsigned int    Warn2Mask[ISP_IRQ_TYPE_AMOUNT][ISP_IRQ_ST_AMOUNT];
	/* flag for indicating that user do mark for a interrupt or not */
	unsigned int    MarkedFlag[ISP_IRQ_TYPE_AMOUNT]
				  [ISP_IRQ_ST_AMOUNT]
				  [IRQ_USER_NUM_MAX];
	/* time for marking a specific interrupt */
	unsigned int    MarkedTime_sec[ISP_IRQ_TYPE_AMOUNT]
				      [32]
				      [IRQ_USER_NUM_MAX];
	/* time for marking a specific interrupt */
	unsigned int    MarkedTime_usec[ISP_IRQ_TYPE_AMOUNT]
				       [32]
				       [IRQ_USER_NUM_MAX];
	/* number of a specific signal that passed by */
	int             PassedBySigCnt
		[ISP_IRQ_TYPE_AMOUNT][32][IRQ_USER_NUM_MAX];
	/* */
	unsigned int    LastestSigTime_sec[ISP_IRQ_TYPE_AMOUNT][32];
	/* latest time for each interrupt */
	unsigned int    LastestSigTime_usec[ISP_IRQ_TYPE_AMOUNT][32];
	/* latest time for each interrupt */
};

struct ISP_TIME_LOG_STRUCT {
	unsigned int     Vd;
	unsigned int     Expdone;
	unsigned int     WorkQueueVd;
	unsigned int     WorkQueueExpdone;
	unsigned int     TaskletVd;
	unsigned int     TaskletExpdone;
};

#if (TIMESTAMP_QUEUE_EN == 1)
#define ISP_TIMESTPQ_DEPTH      (256)
struct ISP_TIMESTPQ_INFO_STRUCT {
	struct {
		struct S_START_T   TimeQue[ISP_TIMESTPQ_DEPTH];
		/* increase when p1done or dmao done */
		unsigned int     WrIndex;
		/* increase when user deque */
		unsigned int     RdIndex;
		unsigned long long  TotalWrCnt;
		unsigned long long  TotalRdCnt;
		/* TSTP_V3 unsigned int	    PrevFbcDropCnt; */
		unsigned int     PrevFbcWCnt;
	} Dmao[_cam_max_];
	unsigned int  DmaEnStatus[_cam_max_];
};
#endif

/**********************************************************************/
#define my_get_pow_idx(value)      \
	({                                                \
		int i = 0, cnt = 0;                       \
		for (i = 0; i < 32; i++) {                \
			if ((value>>i) & (0x00000001)) {  \
				break;                    \
			} else {                          \
				cnt++;                    \
			}                                 \
		}                                         \
		cnt;                                      \
	})

static struct ISP_RAW_INT_STATUS g_ISPIntStatus[ISP_IRQ_TYPE_AMOUNT];
static struct ISP_RAW_INT_STATUS g_ISPIntStatus_SMI[ISP_IRQ_TYPE_AMOUNT];

static unsigned int g_DmaErr_CAM[ISP_IRQ_TYPE_AMOUNT][_cam_max_] = {{0} };

#define SUPPORT_MAX_IRQ 32
struct ISP_INFO_STRUCT {
	spinlock_t                      SpinLockIspRef;
	spinlock_t                      SpinLockIsp;
	spinlock_t                      SpinLockIrq[ISP_IRQ_TYPE_AMOUNT];
	spinlock_t                      SpinLockIrqCnt[ISP_IRQ_TYPE_AMOUNT];
	spinlock_t                      SpinLockRTBC;
	spinlock_t                      SpinLockClock;
	wait_queue_head_t               WaitQueueHead[ISP_IRQ_TYPE_AMOUNT];
	/* wait_queue_head_t*              WaitQHeadList; */
	wait_queue_head_t      WaitQHeadList[SUPPORT_MAX_IRQ];
	unsigned int                         UserCount;
	unsigned int                         DebugMask;
	int					IrqNum;
	struct ISP_IRQ_INFO_STRUCT		IrqInfo;
	struct ISP_IRQ_ERR_WAN_CNT_STRUCT	IrqCntInfo;
	struct ISP_BUF_INFO_STRUCT		BufInfo;
	struct ISP_TIME_LOG_STRUCT		TimeLog;
	#if (TIMESTAMP_QUEUE_EN == 1)
	struct ISP_TIMESTPQ_INFO_STRUCT        TstpQInfo[ISP_IRQ_TYPE_AMOUNT];
	#endif
};

static struct ISP_INFO_STRUCT IspInfo;
static bool    SuspnedRecord[ISP_DEV_NODE_NUM] = {0};

enum eLOG_TYPE {
	/* currently, only used at ipl_buf_ctrl. to protect critical section */
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
struct SV_LOG_STR {
	unsigned int _cnt[LOG_PPNUM][_LOG_MAX];
	/* char   _str[_LOG_MAX][SV_LOG_STR_LEN]; */
	char *_str[LOG_PPNUM][_LOG_MAX];
	struct S_START_T   _lastIrqTime;
};

static void *pLog_kmalloc;
static struct SV_LOG_STR gSvLog[ISP_IRQ_TYPE_AMOUNT];

/**
 *   for irq used,keep log until IRQ_LOG_PRINTER being involked,
 *   limited:
 *   each log must shorter than 512 bytes
 *   total log length in each irq/logtype can't over 1024 bytes
 */
#define IRQ_LOG_KEEPER_T(sec, usec) {\
		sec = ktime_get();  \
		do_div(sec, 1000);    \
		usec = do_div(sec, 1000000);\
	}

#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...) do {\
	char *ptr; \
	char *pDes;\
	int avaLen;\
	unsigned int *ptr2 = &gSvLog[irq]._cnt[ppb][logT];\
	unsigned int str_leng;\
	unsigned int i;\
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
	ptr = pDes = (char *)&(\
		gSvLog[irq]._str[ppb][logT][gSvLog[irq]._cnt[ppb][logT]]);\
	avaLen = str_leng - 1 - gSvLog[irq]._cnt[ppb][logT];\
	if (avaLen > 1) {\
		snprintf((char *)(pDes), avaLen, "[%d.%06d]" fmt,\
			gSvLog[irq]._lastIrqTime.sec, gSvLog[irq]\
			._lastIrqTime.usec,\
			##__VA_ARGS__);   \
		if ('\0' != gSvLog[irq]._str[ppb][logT][str_leng - 1]) {\
			LOG_NOTICE("log str over flow(%d)", irq);\
		} \
		while (*ptr++ != '\0') {\
			(*ptr2)++;\
		}     \
	} else { \
		LOG_INF("(%d)(%d)log str avalible=0, print log\n", irq, logT);\
		ptr = pSrc->_str[ppb][logT];\
		if (pSrc->_cnt[ppb][logT] != 0) {\
			if (logT == _LOG_DBG) {\
				for (i = 0; i < DBG_PAGE; i++) {\
					if (ptr[NORMAL_STR_LEN*(i+1) - 1] !=\
					    '\0') {\
						ptr[NORMAL_STR_LEN*(i+1) - 1] =\
						    '\0';\
						LOG_DBG("%s",\
						    &ptr[NORMAL_STR_LEN*i]);\
					} else{\
						LOG_DBG("%s",\
						    &ptr[NORMAL_STR_LEN*i]);\
						break;\
					} \
				} \
			} \
			else if (logT == _LOG_INF) {\
				for (i = 0; i < INF_PAGE; i++) {\
					if (ptr[NORMAL_STR_LEN*(i+1) - 1] !=\
					    '\0') {\
						ptr[NORMAL_STR_LEN*(i+1) - 1] =\
						    '\0';\
						LOG_INF("%s",\
						    &ptr[NORMAL_STR_LEN*i]);\
					} else{\
						LOG_INF("%s",\
						    &ptr[NORMAL_STR_LEN*i]);\
						break;\
					} \
				} \
			} \
			else if (logT == _LOG_ERR) {\
				for (i = 0; i < ERR_PAGE; i++) {\
					if (ptr[NORMAL_STR_LEN*(i+1) - 1] !=\
					    '\0') {\
						ptr[NORMAL_STR_LEN*(i+1) - 1] =\
						    '\0';\
						LOG_NOTICE("%s",\
						    &ptr[NORMAL_STR_LEN*i]);\
					} else{\
						LOG_NOTICE("%s",\
						    &ptr[NORMAL_STR_LEN*i]);\
						break;\
					} \
				} \
			} \
			else {\
				LOG_NOTICE("N.S.%d", logT);\
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


#if 1
#define IRQ_LOG_PRINTER(irq, ppb_in, logT_in) do {\
	struct SV_LOG_STR *pSrc = &gSvLog[irq];\
	char *ptr;\
	unsigned int i;\
	int ppb;\
	int logT;\
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
					LOG_NOTICE("%s",\
					    &ptr[NORMAL_STR_LEN*i]);\
				} else{\
					LOG_NOTICE("%s",\
					    &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else {\
			LOG_NOTICE("N.S.%d", logT);\
		} \
		ptr[0] = '\0';\
		pSrc->_cnt[ppb][logT] = 0;\
	} \
} while (0)


#else
#define IRQ_LOG_PRINTER(irq, ppb, logT)
#endif

/* //////////////////////////////////////////////////// */

struct _isp_bk_reg_t {
	unsigned int  CAM_TG_INTER_ST;                              /* 453C*/
};

static struct _isp_bk_reg_t g_BkReg[ISP_IRQ_TYPE_AMOUNT];

/* if isp has been suspend, frame cnt needs to add previous value*/
#define ISP_RD32_TG_CAM_FRM_CNT(IrqType, reg_module) ({\
	unsigned int _regVal;\
	_regVal = ISP_RD32(CAM_REG_TG_INTER_ST(reg_module));\
	_regVal = ((_regVal & 0x00FF0000) >> 16)\
			+ g_BkReg[IrqType].CAM_TG_INTER_ST;\
	if (_regVal > 255) { \
		_regVal -= 256;\
	} \
	_regVal;\
})

/******************************************************************************
 * Add MET ftrace event for power profilling.
 * CAM_enter when SOF and CAM_leave when P1_Done
 ******************************************************************************/
#if defined(ISP_MET_READY)
int MET_Event_Get_BPP(enum _isp_dma_enum_ dmao, unsigned int reg_module)
{
	unsigned int fmt_sel = ISP_RD32(CAM_REG_CTL_FMT_SEL(reg_module));
	int ret = 0;

	if (dmao == _imgo_) {
		switch ((fmt_sel >> 4) & 0x1F) {
		case 0:
		case 12:
		case 13:
		case 14:
		case 15:
			ret = 16;
			break;
		case 8:
			ret = 8;
			break;
		case 9:
		case 16:
			ret = 10;
			break;
		case 10:
			ret = 12;
			break;
		case 11:
			ret = 14;
			break;
		default:
			LOG_NOTICE("get imgo bpp error fmt_sel:0x%x value=%x\n",
				fmt_sel, (fmt_sel >> 4) & 0x1F);
			break;
		}
	} else if (dmao == _rrzo_) {
		switch ((fmt_sel >> 2) & 0x3) {
		case 0:
			ret = 8;
			break;
		case 1:
			ret = 10;
			break;
		case 2:
			ret = 12;
			break;
		default:
			LOG_NOTICE("get rrzo bpp error fmt_sel:0x%x value=%x\n",
				fmt_sel, (fmt_sel >> 2) & 0x3);
			break;
		}
	}

	return ret;
}

void CAMSYS_MET_Events_Trace(
		bool enter, u32 reg_module, enum ISP_IRQ_TYPE_ENUM cam)
{
	if (enter) {
		int imgo_en = 0, rrzo_en = 0;
		int imgo_bpp, rrzo_bpp, imgo_xsize, imgo_ysize;
		int rrzo_xsize, rrzo_ysize, rrz_src_w, rrz_src_h, rrz_dst_w;
		int rrz_dst_h, rrz_hori_step, rrz_vert_step;
		u32 ctl_dma_en, rrz_in, rrz_out;
		u32 ctl_en, ctl_en2;

		if (sec_on) {
			ctl_dma_en = lock_reg.CAM_REG_CTL_DMA_EN[reg_module];
			ctl_en = lock_reg.CAM_REG_CTL_EN[reg_module];
			ctl_en2 = lock_reg.CAM_REG_CTL_EN2[reg_module];
		} else {
			ctl_dma_en = ISP_RD32(CAM_REG_CTL_DMA_EN(reg_module));
			ctl_en = ISP_RD32(CAM_REG_CTL_EN(reg_module));
			ctl_en2 = ISP_RD32(CAM_REG_CTL_EN2(reg_module));
		}

		rrz_in = ISP_RD32(CAM_REG_RRZ_IN_IMG(reg_module));
		rrz_out = ISP_RD32(CAM_REG_RRZ_OUT_IMG(reg_module));
		imgo_en = ctl_dma_en & 0x1;
		rrzo_en = ctl_dma_en & 0x4;
		imgo_bpp = MET_Event_Get_BPP(_imgo_, reg_module);
		rrzo_bpp = MET_Event_Get_BPP(_rrzo_, reg_module);
		imgo_xsize =
		  (int)(ISP_RD32(CAM_REG_IMGO_XSIZE(reg_module)) & 0xFFFF);
		imgo_ysize =
		  (int)(ISP_RD32(CAM_REG_IMGO_YSIZE(reg_module)) & 0xFFFF);
		rrzo_xsize =
		  (int)(ISP_RD32(CAM_REG_RRZO_XSIZE(reg_module)) & 0xFFFF);
		rrzo_ysize =
		  (int)(ISP_RD32(CAM_REG_RRZO_YSIZE(reg_module)) & 0xFFFF);
		rrz_src_w = rrz_in & 0xFFFF;
		rrz_src_h = (rrz_in >> 16) & 0xFFFF;
		rrz_dst_w = rrz_out & 0xFFFF;
		rrz_dst_h = (rrz_out >> 16) & 0xFFFF;
		rrz_hori_step =
		  (int)(ISP_RD32(CAM_REG_RRZ_HORI_STEP(reg_module)) & 0x3FFFF);
		rrz_vert_step =
		  (int)(ISP_RD32(CAM_REG_RRZ_VERT_STEP(reg_module)) & 0x3FFFF);

		trace_ISP__Pass1_CAM_enter(cam, imgo_en, rrzo_en, imgo_bpp,
		rrzo_bpp, imgo_xsize, imgo_ysize, rrzo_xsize, rrzo_ysize,
		rrz_src_w, rrz_src_h, rrz_dst_w, rrz_dst_h, rrz_hori_step,
		rrz_vert_step, ctl_en, ctl_dma_en, ctl_en2);
	} else {
		trace_ISP__Pass1_CAM_leave(cam, 0);
	}
}
#endif

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int ISP_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int ISP_UsToJiffies(unsigned int Us)
{
	return (((Us / 1000) * HZ + 512) >> 10);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int
ISP_GetIRQState(
		unsigned int type, unsigned int stType,
		unsigned int userNumber, unsigned int stus)
{
	unsigned int ret;
	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;

	/*  */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[type]), flags);
	ret = (IspInfo.IrqInfo.Status[type][stType][userNumber] & stus);
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[type]), flags);
	/*  */
	return ret;
}



/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int ISP_JiffiesToMs(unsigned int Jiffies)
{
	return ((Jiffies * 1000) / HZ);
}

/******************************************************************************
 *
 ******************************************************************************/

static void ISP_DumpDmaDeepDbg(enum ISP_IRQ_TYPE_ENUM module)
{
	unsigned int dmaerr[_cam_max_];
	char cam[10] = {'\0'};
	enum ISP_DEV_NODE_ENUM regModule; /* for read/write register */

	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
		regModule = ISP_CAM_A_IDX;
		strncpy(cam, "CAM_A", sizeof("CAM_A"));
		break;
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
		regModule = ISP_CAM_B_IDX;
		strncpy(cam, "CAM_B", sizeof("CAM_B"));
		break;
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		regModule = ISP_CAM_C_IDX;
		strncpy(cam, "CAM_C", sizeof("CAM_C"));
		break;
	default:
		LOG_NOTICE("unsupported module:0x%x\n", module);
		return;
	}


	dmaerr[_imgo_] =
	    (unsigned int)ISP_RD32(CAM_REG_IMGO_ERR_STAT(regModule));
	dmaerr[_rrzo_] =
	    (unsigned int)ISP_RD32(CAM_REG_RRZO_ERR_STAT(regModule));
	dmaerr[_aao_] =
	    (unsigned int)ISP_RD32(CAM_REG_AAO_ERR_STAT(regModule));
	dmaerr[_afo_] =
	    (unsigned int)ISP_RD32(CAM_REG_AFO_ERR_STAT(regModule));
	dmaerr[_lcso_] =
	    (unsigned int)ISP_RD32(CAM_REG_LCSO_ERR_STAT(regModule));
	dmaerr[_ufeo_] =
	    (unsigned int)ISP_RD32(CAM_REG_UFEO_ERR_STAT(regModule));
	dmaerr[_bpci_] =
	    (unsigned int)ISP_RD32(CAM_REG_BPCI_ERR_STAT(regModule));
	dmaerr[_lsci_] =
	    (unsigned int)ISP_RD32(CAM_REG_LSCI_ERR_STAT(regModule));
	dmaerr[_pdo_] =
	    (unsigned int)ISP_RD32(CAM_REG_PDO_ERR_STAT(regModule));
	dmaerr[_pso_] =
	    (unsigned int)ISP_RD32(CAM_REG_PSO_ERR_STAT(regModule));
	dmaerr[_pdi_] =
	    (unsigned int)ISP_RD32(CAM_REG_PDI_ERR_STAT(regModule));

	dmaerr[_lmvo_] =
	    (unsigned int)ISP_RD32(CAM_REG_LMVO_ERR_STAT(regModule));
	dmaerr[_flko_] =
	    (unsigned int)ISP_RD32(CAM_REG_FLKO_ERR_STAT(regModule));
	dmaerr[_rsso_] =
	    (unsigned int)ISP_RD32(CAM_REG_RSSO_A_ERR_STAT(regModule));
	dmaerr[_ufgo_] =
	    (unsigned int)ISP_RD32(CAM_REG_UFGO_ERR_STAT(regModule));
	dmaerr[_rawi_] =
	    (unsigned int)ISP_RD32(CAM_UNI_REG_RAWI_ERR_STAT(ISP_UNI_A_IDX));

	IRQ_LOG_KEEPER(
	    module, m_CurrentPPB, _LOG_ERR,
	    "camsys:0x%x", ISP_RD32(ISP_CAMSYS_CONFIG_BASE));

	IRQ_LOG_KEEPER(
	    module, m_CurrentPPB, _LOG_ERR,
	    "%s:IMGO:0x%x,RRZO:0x%x,AAO=0x%x,AFO=0x%x,LCSO=0x%x,UFEO=0x%x,PDO=0x%x,PSO=0x%x\n,EISO=0x%x,RSSO=0x%x,UFGO=0x%x,FLKO=0x%x DMA_DBG_SEL=0x%x TOP_DBG_PORT=0x%x\n",
	    cam,
	    dmaerr[_imgo_],
	    dmaerr[_rrzo_],
	    dmaerr[_aao_],
	    dmaerr[_afo_],
	    dmaerr[_lcso_],
	    dmaerr[_ufeo_],
	    dmaerr[_pdo_],
	    dmaerr[_pso_],
	    dmaerr[_lmvo_],
	    dmaerr[_rsso_],
	    dmaerr[_ufgo_],
	    dmaerr[_flko_],
	    (unsigned int)ISP_RD32(CAM_REG_DMA_DEBUG_SEL(regModule)),
	    (unsigned int)ISP_RD32(CAM_UNI_REG_TOP_DBG_PORT(ISP_UNI_A_IDX)));

	/* CAM_UNI_TOP_DBG_SET [15:12] = 4'd0; for smi dbg dump */
	ISP_WR32(CAM_UNI_REG_TOP_DBG_SET(ISP_UNI_A_IDX),
	      (ISP_RD32(CAM_UNI_REG_TOP_DBG_SET(ISP_UNI_A_IDX)) & 0xFFFF0FFF));

	ISP_WR32(CAM_REG_DMA_DEBUG_SEL(regModule), 0x00080403);
	IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
	    "(IMGO1:DMA_DBG_SEL=0x%x DBG_PORT=0x%x)",
	    (unsigned int)ISP_RD32(CAM_REG_DMA_DEBUG_SEL(regModule)),
	    (unsigned int)ISP_RD32(CAM_UNI_REG_TOP_DBG_PORT(ISP_UNI_A_IDX)));

	ISP_WR32(CAM_REG_DMA_DEBUG_SEL(regModule), 0x00000403);
	IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
	    "(IMGO2:DMA_DBG_SEL=0x%x DBG_PORT=0x%x)",
	    (unsigned int)ISP_RD32(CAM_REG_DMA_DEBUG_SEL(regModule)),
	    (unsigned int)ISP_RD32(CAM_UNI_REG_TOP_DBG_PORT(ISP_UNI_A_IDX)));

	ISP_WR32(CAM_REG_DMA_DEBUG_SEL(regModule), 0x00010403);
	IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
	    "(IMGO3:DMA_DBG_SEL=0x%x DBG_PORT=0x%x)",
	    (unsigned int)ISP_RD32(CAM_REG_DMA_DEBUG_SEL(regModule)),
	    (unsigned int)ISP_RD32(CAM_UNI_REG_TOP_DBG_PORT(ISP_UNI_A_IDX)));

	ISP_WR32(CAM_REG_DMA_DEBUG_SEL(regModule), 0x00080404);
	IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
	    "(RRZO1:DMA_DBG_SEL=0x%x DBG_PORT=0x%x)",
	    (unsigned int)ISP_RD32(CAM_REG_DMA_DEBUG_SEL(regModule)),
	    (unsigned int)ISP_RD32(CAM_UNI_REG_TOP_DBG_PORT(ISP_UNI_A_IDX)));

	ISP_WR32(CAM_REG_DMA_DEBUG_SEL(regModule), 0x00000404);
	IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
	    "(RRZO2:DMA_DBG_SEL=0x%x DBG_PORT=0x%x)",
	    (unsigned int)ISP_RD32(CAM_REG_DMA_DEBUG_SEL(regModule)),
	    (unsigned int)ISP_RD32(CAM_UNI_REG_TOP_DBG_PORT(ISP_UNI_A_IDX)));

	ISP_WR32(CAM_REG_DMA_DEBUG_SEL(regModule), 0x00010404);
	IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
	    "(RRZO3:DMA_DBG_SEL=0x%x DBG_PORT=0x%x)",
	    (unsigned int)ISP_RD32(CAM_REG_DMA_DEBUG_SEL(regModule)),
	    (unsigned int)ISP_RD32(CAM_UNI_REG_TOP_DBG_PORT(ISP_UNI_A_IDX)));

	IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
	    "%s:BPCI:0x%x,LSCI=0x%x,PDI=0x%x,RAWI=0x%x\n",
	    cam,
	    dmaerr[_bpci_],
	    dmaerr[_lsci_],
	    dmaerr[_pdi_],
	    dmaerr[_rawi_]);

	g_DmaErr_CAM[module][_imgo_] |= dmaerr[_imgo_];
	g_DmaErr_CAM[module][_rrzo_] |= dmaerr[_rrzo_];
	g_DmaErr_CAM[module][_aao_] |= dmaerr[_aao_];
	g_DmaErr_CAM[module][_afo_] |= dmaerr[_afo_];
	g_DmaErr_CAM[module][_lcso_] |= dmaerr[_lcso_];
	g_DmaErr_CAM[module][_ufeo_] |= dmaerr[_ufeo_];
	g_DmaErr_CAM[module][_bpci_] |= dmaerr[_bpci_];
	g_DmaErr_CAM[module][_lsci_] |= dmaerr[_lsci_];
	g_DmaErr_CAM[module][_pdo_] |= dmaerr[_pdo_];
	g_DmaErr_CAM[module][_pso_] |= dmaerr[_pso_];
	g_DmaErr_CAM[module][_flko_] |= dmaerr[_flko_];
	g_DmaErr_CAM[module][_lmvo_] |= dmaerr[_lmvo_];
	g_DmaErr_CAM[module][_rsso_] |= dmaerr[_rsso_];
	g_DmaErr_CAM[module][_ufgo_] |= dmaerr[_ufgo_];
	g_DmaErr_CAM[module][_rawi_] |= dmaerr[_rawi_];
	g_DmaErr_CAM[module][_pdi_] |= dmaerr[_pdi_];
}

static inline void Prepare_Enable_ccf_clock(void)
{
	int ret;
	/* must keep this clk open order:
	 * CG_SCP_SYS_DIS->CG_DISP0_SMI_COMMON->CG_SCP_SYS_ISP/CAM->ISP clk
	 */

	#ifndef EP_MARK_SMI /* enable through smi API */
	LOG_INF("enable CG/MTCMOS through SMI CLK API\n");
	smi_bus_prepare_enable(SMI_LARB6, ISP_DEV_NAME);
	smi_bus_prepare_enable(SMI_LARB3, ISP_DEV_NAME);
	#endif

	ret = clk_prepare_enable(isp_clk.ISP_SCP_SYS_DIS);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_SCP_SYS_DIS clock\n");

	ret = clk_prepare_enable(isp_clk.ISP_SCP_SYS_ISP);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_SCP_SYS_ISP clock\n");

	ret = clk_prepare_enable(isp_clk.ISP_SCP_SYS_CAM);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_SCP_SYS_CAM clock\n");

	ret = clk_prepare_enable(isp_clk.ISP_CAM_CAMSYS);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_CAM_CAMSYS clock\n");

	ret = clk_prepare_enable(isp_clk.ISP_CAM_CAMTG);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_CAM_CAMTG clock\n");

	ret = clk_prepare_enable(isp_clk.ISP_CAM_CAMSV0);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_CAM_CAMSV0 clock\n");

	ret = clk_prepare_enable(isp_clk.ISP_CAM_CAMSV1);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_CAM_CAMSV1 clock\n");

	ret = clk_prepare_enable(isp_clk.ISP_CAM_CAMSV2);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_CAM_CAMSV2 clock\n");

}

static inline void Disable_Unprepare_ccf_clock(void)
{
	/* must keep this clk close order:
	 * ISP clk->CG_SCP_SYS_ISP/CAM->CG_DISP0_SMI_COMMON->CG_SCP_SYS_DIS
	 */
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMSV0);
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMSV1);
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMSV2);
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMTG);
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMSYS);
	clk_disable_unprepare(isp_clk.ISP_SCP_SYS_CAM);
	clk_disable_unprepare(isp_clk.ISP_SCP_SYS_ISP);
	clk_disable_unprepare(isp_clk.ISP_SCP_SYS_DIS);

	#ifndef EP_MARK_SMI
	LOG_INF("disable CG/MTCMOS through SMI CLK API\n");
	smi_bus_disable_unprepare(SMI_LARB6, ISP_DEV_NAME);
	smi_bus_disable_unprepare(SMI_LARB3, ISP_DEV_NAME);
	#endif
}

/******************************************************************************
 *
 ******************************************************************************/

void ISP_Halt_Mask(unsigned int isphaltMask)
{
	unsigned int setReg;

	setReg = ISP_RD32(ISP_CAMSYS_CONFIG_BASE + 0x120) &
		~((unsigned int)(1 << (isphaltMask)));

	ISP_WR32(ISP_CAMSYS_CONFIG_BASE + 0x120, setReg);

	LOG_INF("ISP halt_en for dvfs:0x%x\n",
		ISP_RD32(ISP_CAMSYS_CONFIG_BASE + 0x120));
}
EXPORT_SYMBOL(ISP_Halt_Mask);

/******************************************************************************
 *
 ******************************************************************************/
static void ISP_ConfigDMAControl(void)
{
	enum ISP_DEV_NODE_ENUM module = ISP_CAM_A_IDX;

	ISP_WR32(CAM_UNI_REG_RAWI_CON(ISP_UNI_A_IDX), 0x80a0a0a0);
	ISP_WR32(CAM_UNI_REG_RAWI_CON2(ISP_UNI_A_IDX), 0x00a0a000);
	ISP_WR32(CAM_UNI_REG_RAWI_CON3(ISP_UNI_A_IDX), 0x00a0a000);

	for (; module < ISP_CAMSV0_IDX; (u32)module++) {
		ISP_WR32(CAM_REG_IMGO_CON(module), 0x80000180);
		ISP_WR32(CAM_REG_IMGO_CON2(module), 0x00400040);
		ISP_WR32(CAM_REG_IMGO_CON3(module), 0x00200020);

		ISP_WR32(CAM_REG_RRZO_CON(module), 0x80000180);
		ISP_WR32(CAM_REG_RRZO_CON2(module), 0x00800080);
		ISP_WR32(CAM_REG_RRZO_CON3(module), 0x00400040);

		ISP_WR32(CAM_REG_AAO_CON(module), 0x80000080);
		ISP_WR32(CAM_REG_AAO_CON2(module), 0x00200020);
		ISP_WR32(CAM_REG_AAO_CON3(module), 0x00100010);

		ISP_WR32(CAM_REG_AFO_CON(module), 0x80000100);
		ISP_WR32(CAM_REG_AFO_CON2(module), 0x00800080);
		ISP_WR32(CAM_REG_AFO_CON3(module), 0x00550055);

		ISP_WR32(CAM_REG_LCSO_CON(module), 0x80000020);
		ISP_WR32(CAM_REG_LCSO_CON2(module), 0x00100010);
		ISP_WR32(CAM_REG_LCSO_CON3(module), 0x000A000A);

		ISP_WR32(CAM_REG_UFEO_CON(module), 0x80000040);
		ISP_WR32(CAM_REG_UFEO_CON2(module), 0x00200020);
		ISP_WR32(CAM_REG_UFEO_CON3(module), 0x00150015);

		ISP_WR32(CAM_REG_PDO_CON(module), 0x80000100);
		ISP_WR32(CAM_REG_PDO_CON2(module), 0x00800080);
		ISP_WR32(CAM_REG_PDO_CON3(module), 0x00550055);

		ISP_WR32(CAM_REG_PSO_CON(module), 0x80000080);
		ISP_WR32(CAM_REG_PSO_CON2(module), 0x00400040);
		ISP_WR32(CAM_REG_PSO_CON3(module), 0x002A002A);

		ISP_WR32(CAM_REG_LMVO_CON(module), 0x80000020);
		ISP_WR32(CAM_REG_LMVO_CON2(module), 0x00100010);
		ISP_WR32(CAM_REG_LMVO_CON3(module), 0x000A000A);

		ISP_WR32(CAM_REG_FLKO_CON(module), 0x80000020);
		ISP_WR32(CAM_REG_FLKO_CON2(module), 0x00100010);
		ISP_WR32(CAM_REG_FLKO_CON3(module), 0x000A000A);

		ISP_WR32(CAM_REG_RSSO_CON(module), 0x80000040);
		ISP_WR32(CAM_REG_RSSO_CON2(module), 0x00200020);
		ISP_WR32(CAM_REG_RSSO_CON3(module), 0x00150015);

		ISP_WR32(CAM_REG_UFGO_CON(module), 0x80000040);
		ISP_WR32(CAM_REG_UFGO_CON2(module), 0x00200020);
		ISP_WR32(CAM_REG_UFGO_CON3(module), 0x00150015);

		ISP_WR32(CAM_REG_BPCI_CON(module), 0x80000020);
		ISP_WR32(CAM_REG_BPCI_CON2(module), 0x00040004);
		ISP_WR32(CAM_REG_BPCI_CON3(module), 0x00020002);

		ISP_WR32(CAM_REG_LSCI_CON(module), 0x80000040);
		ISP_WR32(CAM_REG_LSCI_CON2(module), 0x00040004);
		ISP_WR32(CAM_REG_LSCI_CON3(module), 0x00020002);

		ISP_WR32(CAM_REG_PDI_CON(module), 0x80000020);
		ISP_WR32(CAM_REG_PDI_CON2(module), 0x00100010);
		ISP_WR32(CAM_REG_PDI_CON3(module), 0x000A000A);
	}
}

/******************************************************************************
 *
 ******************************************************************************/
static void ISP_EnableClock(bool En)
{
	if (En) {
#if defined(EP_NO_CLKMGR)
		spin_lock(&(IspInfo.SpinLockClock));
		/* LOG_DBG("Camera clock enbled. G_u4EnableClockCount: %d.", */
		/*	G_u4EnableClockCount); */
		switch (G_u4EnableClockCount) {
		case 0:
			/* Enable clock by hardcode:
			 * 1. CAMSYS_CG_CLR (0x1A000008) = 0xffffffff;
			 * 2. IMG_CG_CLR (0x15000008) = 0xffffffff;
			 */
			ISP_WR32(CAMSYS_REG_CG_CLR, 0xFFFFFFFF);
			break;
		default:
			break;
		}
		G_u4EnableClockCount++;
		spin_unlock(&(IspInfo.SpinLockClock));
#else/*CCF*/
		/*LOG_INF("CCF:prepare_enable clk");*/
		spin_lock(&(IspInfo.SpinLockClock));
		G_u4EnableClockCount++;
		spin_unlock(&(IspInfo.SpinLockClock));
		/* !!cannot be used in spinlock!! */
		Prepare_Enable_ccf_clock();
#endif
		/* Disable CAMSYS_HALT1_EN: LSCI & BPCI*/
		/* To avoid ISP halt keep arise */
		#if 1/* ALSK TBD */
		LOG_INF("# NEED UPDATE CAMSYS_HALT1_EN: LSCI & BPCI SETTING #");
		#else
		ISP_WR32(ISP_CAMSYS_CONFIG_BASE + 0x120, 0xFFFFFF4F);
		#endif
	} else {                /* Disable clock. */
#if defined(EP_NO_CLKMGR)
		spin_lock(&(IspInfo.SpinLockClock));
		/* LOG_DBG("Camera clock disabled. G_u4EnableClockCount: %d.",*/
		/* G_u4EnableClockCount); */
		G_u4EnableClockCount--;
		switch (G_u4EnableClockCount) {
		case 0:
			/* Disable clock by hardcode:
			 * 1. CAMSYS_CG_SET (0x1A000004) = 0xffffffff;
			 * 2. IMG_CG_SET (0x15000004) = 0xffffffff;
			 */
			ISP_WR32(CAMSYS_REG_CG_SET, 0xFFFFFEBF);
			break;
		default:
			break;
		}
		spin_unlock(&(IspInfo.SpinLockClock));
#else
		/*LOG_INF("CCF:disable_unprepare clk\n");*/
		spin_lock(&(IspInfo.SpinLockClock));
		if (G_u4EnableClockCount == 0) {
			spin_unlock(&(IspInfo.SpinLockClock));
			LOG_INF(
			  "G_u4EnableClockCount aleady be 0, do nothing\n");
			return;
		}

		G_u4EnableClockCount--;
		spin_unlock(&(IspInfo.SpinLockClock));
		/* !!cannot be used in spinlock!! */
		Disable_Unprepare_ccf_clock();
#endif
	}
}

/******************************************************************************
 *
 ******************************************************************************/
static inline void ISP_Reset(int module)
{
	LOG_DBG(" Reset module(%d), CAMSYS clk gate(0x%x)\n",
		module, ISP_RD32(CAMSYS_REG_CG_CON));

	switch (module) {
	case ISP_CAM_A_IDX:
	case ISP_CAM_B_IDX:
	case ISP_CAM_C_IDX:
		/* Reset CAM flow */
		ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x2);
		ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x1);
#if 0
		while (ISP_RD32(CAM_REG_CTL_SW_CTL(module)) != 0x2)
			LOG_DBG("CAM resetting... module:%d\n", module);
#else
		mdelay(1);
#endif
		ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x4);
		ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x0);

		break;
	case ISP_CAMSV0_IDX:
	case ISP_CAMSV1_IDX:
	case ISP_CAMSV2_IDX:
	case ISP_CAMSV3_IDX:
	case ISP_CAMSV4_IDX:
	case ISP_CAMSV5_IDX:
		/* Reset CAMSV flow */
		ISP_WR32(CAMSV_REG_SW_CTL(module), 0x4); /* SW_RST: 1 */
		ISP_WR32(CAMSV_REG_SW_CTL(module), 0x0); /* SW_RST: 0 */
		ISP_WR32(CAMSV_REG_SW_CTL(module), 0x1); /* IMGO_RST_TRIG: 1 */
		/* Polling IMGO_RST_ST to 1 */
		while (ISP_RD32(CAMSV_REG_SW_CTL(module)) != 0x3)
			LOG_DBG("CAMSV resetting... module:%d\n", module);
		ISP_WR32(CAMSV_REG_SW_CTL(module), 0x0); /* IMGO_RST_TRIG: 0 */
		break;
	case ISP_UNI_A_IDX:
		/* Reset UNI flow */
		ISP_WR32(CAM_UNI_REG_TOP_SW_CTL(module), 0x222);
		ISP_WR32(CAM_UNI_REG_TOP_SW_CTL(module), 0x111);
#if 0
		while (ISP_RD32(CAM_UNI_REG_TOP_SW_CTL(module)) != 0x222)
			LOG_DBG("UNI resetting... module:%d\n", module);
#else
		mdelay(1);
#endif
		ISP_WR32(CAM_UNI_REG_TOP_SW_CTL(module), 0x0444);
		ISP_WR32(CAM_UNI_REG_TOP_SW_CTL(module), 0x0);
		break;
	default:
		LOG_NOTICE("Not support reset module:%d\n", module);
		break;
	}
}

/******************************************************************************
 *
 ******************************************************************************/
static int ISP_ReadReg(struct ISP_REG_IO_STRUCT *pRegIo)
{
	unsigned int i, ispRange = 0;
	int Ret = 0;

	void __iomem *regBase;

	/*  */
	struct ISP_REG_STRUCT reg;
	struct ISP_REG_STRUCT *pData = (struct ISP_REG_STRUCT *)pRegIo->pData;

	switch (pData->module) {
	case ISP_CAM_A_IDX:
		regBase = ISP_CAM_A_BASE;
		break;
	case ISP_CAM_B_IDX:
		regBase = ISP_CAM_B_BASE;
		break;
	case ISP_CAM_C_IDX:
		regBase = ISP_CAM_C_BASE;
		break;
	case ISP_CAMSV0_IDX:
		regBase = ISP_CAMSV0_BASE;
		break;
	case ISP_CAMSV1_IDX:
		regBase = ISP_CAMSV1_BASE;
		break;
	case ISP_CAMSV2_IDX:
		regBase = ISP_CAMSV2_BASE;
		break;
	case ISP_CAMSV3_IDX:
		regBase = ISP_CAMSV3_BASE;
		break;
	case ISP_CAMSV4_IDX:
		regBase = ISP_CAMSV4_BASE;
		break;
	case ISP_CAMSV5_IDX:
		regBase = ISP_CAMSV5_BASE;
		break;
	case ISP_UNI_A_IDX:
		regBase = ISP_CAM_UNI_BASE;
		break;
	default:
		LOG_NOTICE("Unsupported module(%x) !!!\n", pData->module);
		Ret = -EFAULT;
		goto EXIT;
	}

	if (regBase < ISP_CAMSV0_BASE)
		ispRange = ISP_REG_RANGE;
	else
		ispRange = PAGE_SIZE;

	for (i = 0; i < pRegIo->Count; i++) {
		if (get_user(reg.Addr, (unsigned int *)&pData->Addr) != 0) {
			LOG_NOTICE("get_user failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}

		if ((regBase + reg.Addr) < (regBase + ispRange)) {
			reg.Val = ISP_RD32(regBase + reg.Addr);
		} else {
			LOG_NOTICE("Wrong address(0x%lx)\n",
				(unsigned long)(regBase + reg.Addr));
			reg.Val = 0;
		}

		if (put_user(reg.Val, (unsigned int *) &(pData->Val)) != 0) {
			LOG_NOTICE("put_user failed\n");
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


/******************************************************************************
 *
 ******************************************************************************/
/*
 * Note: Can write sensor's test model only, if need write to other modules,
 * need modify current code flow
 */
static int ISP_WriteRegToHw(
	struct ISP_REG_STRUCT *pReg,
	unsigned int         Count)
{
	int Ret = 0;
	unsigned int i, ispRange = 0;
	bool dbgWriteReg;
	void __iomem *regBase;

	/*
	 * Use local variable to store IspInfo.DebugMask & ISP_DBG_WRITE_REG
	 * for saving lock time
	 */
	spin_lock(&(IspInfo.SpinLockIsp));
	dbgWriteReg = IspInfo.DebugMask & ISP_DBG_WRITE_REG;
	spin_unlock(&(IspInfo.SpinLockIsp));

	switch (pReg->module) {
	case ISP_CAM_A_IDX:
		regBase = ISP_CAM_A_BASE;
		break;
	case ISP_CAM_B_IDX:
		regBase = ISP_CAM_B_BASE;
		break;
	case ISP_CAM_C_IDX:
		regBase = ISP_CAM_C_BASE;
		break;
	case ISP_CAMSV0_IDX:
		regBase = ISP_CAMSV0_BASE;
		break;
	case ISP_CAMSV1_IDX:
		regBase = ISP_CAMSV1_BASE;
		break;
	case ISP_CAMSV2_IDX:
		regBase = ISP_CAMSV2_BASE;
		break;
	case ISP_CAMSV3_IDX:
		regBase = ISP_CAMSV3_BASE;
		break;
	case ISP_CAMSV4_IDX:
		regBase = ISP_CAMSV4_BASE;
		break;
	case ISP_CAMSV5_IDX:
		regBase = ISP_CAMSV5_BASE;
		break;
	case ISP_UNI_A_IDX:
		regBase = ISP_CAM_UNI_BASE;
		break;
	default:
		LOG_NOTICE("Unsupported module(%x) !!!\n", pReg->module);
		return -EFAULT;
	}

	/*  */
	if (dbgWriteReg)
		LOG_DBG("- E.\n");

	if (regBase < ISP_CAMSV0_BASE)
		ispRange = ISP_REG_RANGE;
	else
		ispRange = PAGE_SIZE;

	/*  */
	for (i = 0; i < Count; i++) {
		if (dbgWriteReg)
			LOG_DBG(
			  "module(%d), base(0x%lx),Addr(0x%lx), Val(0x%x)\n",
			  pReg->module,
			  (unsigned long)regBase,
			  (unsigned long)(pReg[i].Addr),
			  (unsigned int)(pReg[i].Val));

		if (((regBase + pReg[i].Addr) < (regBase + ispRange)))
			ISP_WR32(regBase + pReg[i].Addr, pReg[i].Val);
		else
			LOG_NOTICE("wrong address(0x%lx)\n",
			  (unsigned long)(regBase + pReg[i].Addr));

	}

	/*  */
	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static int ISP_WriteReg(struct ISP_REG_IO_STRUCT *pRegIo)
{
	int Ret = 0;
	struct ISP_REG_STRUCT *pData = NULL;

	if (pRegIo->Count > 0xFFFFFFFF) {
		LOG_NOTICE("pRegIo->Count error");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_WRITE_REG)
		LOG_DBG("Data(0x%p), Count(%d)\n",
		  (pRegIo->pData), (pRegIo->Count));

	pData = kmalloc(
		  (pRegIo->Count) * sizeof(struct ISP_REG_STRUCT), GFP_ATOMIC);
	if (pData == NULL) {
		LOG_DBG(
		"ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)\n",
		current->comm, current->pid, current->tgid);
		Ret = -ENOMEM;
		goto EXIT;
	}

	if ((void __user *)(pRegIo->pData) == NULL) {
		LOG_NOTICE("NULL pData");
		Ret = -EFAULT;
		goto EXIT;
	}

	/*  */
	if (copy_from_user(pData, (void __user *)(pRegIo->pData),
		  pRegIo->Count * sizeof(struct ISP_REG_STRUCT)) != 0) {
		LOG_NOTICE("copy_from_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	Ret = ISP_WriteRegToHw(
		      pData,
		      pRegIo->Count);
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

/* isr dbg log , sw isr response counter , +1 when sw receive 1 sof isr. */
static unsigned int sof_count[ISP_IRQ_TYPE_AMOUNT] = {0};
static int Vsync_cnt[ISP_IRQ_TYPE_AMOUNT] = {0};

/* keep current frame status */
static enum CAM_FrameST FrameStatus[ISP_IRQ_TYPE_AMOUNT] = {0};

/*
 * current invoked time is at 1st sof or not during each streaming,
 * reset when streaming off
 */
static bool g1stSof[ISP_IRQ_TYPE_AMOUNT] = {0};
#if (TSTMP_SUBSAMPLE_INTPL == 1)
static bool g1stSwP1Done[ISP_IRQ_TYPE_AMOUNT] = {0};
static unsigned long long gPrevSofTimestp[ISP_IRQ_TYPE_AMOUNT];
#endif

static struct S_START_T gSTime[ISP_IRQ_TYPE_AMOUNT] = {{0} };

#ifdef _MAGIC_NUM_ERR_HANDLING_
#define _INVALID_FRM_CNT_ 0xFFFF
#endif

static long ISP_Buf_CTRL_FUNC(unsigned long Param)
{
	int Ret = 0;
	enum _isp_dma_enum_ rt_dma;
	unsigned int i = 0;
	struct ISP_BUFFER_CTRL_STRUCT         rt_buf_ctrl;

	/*  */
	if ((void __user *)Param == NULL)  {
		LOG_NOTICE("[rtbc]NULL Param");
		return -EFAULT;
	}
	/*  */
	if (copy_from_user(&rt_buf_ctrl, (void __user *)Param,
		  sizeof(struct ISP_BUFFER_CTRL_STRUCT)) == 0) {
		if (rt_buf_ctrl.module >= ISP_IRQ_TYPE_AMOUNT) {
			LOG_NOTICE("[rtbc]not supported module:0x%x\n",
			  rt_buf_ctrl.module);
			return -EFAULT;
		}

		if (pstRTBuf[rt_buf_ctrl.module] == NULL)  {
			LOG_NOTICE("[rtbc]NULL pstRTBuf, module:0x%x\n",
			  rt_buf_ctrl.module);
			return -EFAULT;
		}

		rt_dma = rt_buf_ctrl.buf_id;
		if (rt_dma >= _cam_max_) {
			LOG_NOTICE("[rtbc]buf_id error:0x%x\n", rt_dma);
			return -EFAULT;
		}

		/*  */
		switch (rt_buf_ctrl.ctrl) {
		case ISP_RT_BUF_CTRL_CLEAR:
			/*  */
			if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL)
				LOG_INF("[rtbc][%d][CLEAR]:rt_dma(%d)\n",
				  rt_buf_ctrl.module, rt_dma);
			/*  */

			memset((void *)IspInfo.IrqInfo.LastestSigTime_usec[
				rt_buf_ctrl.module],
				0, sizeof(unsigned int) * 32);
			memset((void *)IspInfo.IrqInfo.LastestSigTime_sec[
				rt_buf_ctrl.module],
				0, sizeof(unsigned int) * 32);
			/*
			 * remove, cause clear will be involked only
			 * when current module r totally stopped
			 */
			/*spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT_Lock]),*/
			/*	flags);*/

			/* reset active record*/
			pstRTBuf[rt_buf_ctrl.module]
				->ring_buf[rt_dma].active = MFALSE;
			memset((char *)&pstRTBuf[rt_buf_ctrl.module]
				->ring_buf[rt_dma], 0x00,
				sizeof(struct ISP_RT_RING_BUF_INFO_STRUCT));
			/* init. frmcnt before vf_en */
			for (i = 0; i < ISP_RT_BUF_SIZE; i++)
				pstRTBuf[rt_buf_ctrl.module]
				  ->ring_buf[rt_dma].data[i].image.frm_cnt
				  = _INVALID_FRM_CNT_;

			switch (rt_buf_ctrl.module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				if ((pstRTBuf[rt_buf_ctrl.module]
				  ->ring_buf[_imgo_].active == MFALSE) &&
				  (pstRTBuf[rt_buf_ctrl.module]
				  ->ring_buf[_rrzo_].active == MFALSE)) {
					sof_count[rt_buf_ctrl.module] = 0;
					g1stSof[rt_buf_ctrl.module] = MTRUE;
					#if (TSTMP_SUBSAMPLE_INTPL == 1)
					g1stSwP1Done[
					  rt_buf_ctrl.module] = MTRUE;
					gPrevSofTimestp[
					  rt_buf_ctrl.module] = 0;
					#endif
					g_ISPIntStatus[
					  rt_buf_ctrl.module].ispIntErr = 0;
					g_ISPIntStatus[
					  rt_buf_ctrl.module].ispInt3Err = 0;
					g_ISPIntStatus_SMI[
					  rt_buf_ctrl.module].ispIntErr = 0;
					g_ISPIntStatus_SMI[
					  rt_buf_ctrl.module].ispInt3Err = 0;
					pstRTBuf[
					  rt_buf_ctrl.module]->dropCnt = 0;
					pstRTBuf[
					  rt_buf_ctrl.module]->state = 0;
				}

				memset((void *)g_DmaErr_CAM[rt_buf_ctrl.module],
					0, sizeof(unsigned int)*_cam_max_);
				break;
			case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
				if (pstRTBuf[rt_buf_ctrl.module]
				  ->ring_buf[_camsv_imgo_].active == MFALSE) {
					sof_count[rt_buf_ctrl.module] = 0;
					g1stSof[rt_buf_ctrl.module] = MTRUE;
					g_ISPIntStatus[
					  rt_buf_ctrl.module].ispIntErr = 0;
					g_ISPIntStatus[
					  rt_buf_ctrl.module].ispInt3Err = 0;
					g_ISPIntStatus_SMI[
					  rt_buf_ctrl.module].ispIntErr = 0;
					g_ISPIntStatus_SMI[
					  rt_buf_ctrl.module].ispInt3Err = 0;
					pstRTBuf[
					  rt_buf_ctrl.module]->dropCnt = 0;
					pstRTBuf[
					  rt_buf_ctrl.module]->state = 0;
				}

				break;
			case ISP_IRQ_TYPE_INT_UNI_A_ST:
				sof_count[rt_buf_ctrl.module] = 0;
				g1stSof[rt_buf_ctrl.module] = MTRUE;
				g_ISPIntStatus[
				  rt_buf_ctrl.module].ispIntErr = 0;
				g_ISPIntStatus[
				  rt_buf_ctrl.module].ispInt3Err = 0;
				g_ISPIntStatus_SMI[
				  rt_buf_ctrl.module].ispIntErr = 0;
				g_ISPIntStatus_SMI[
				  rt_buf_ctrl.module].ispInt3Err = 0;
				pstRTBuf[rt_buf_ctrl.module]->dropCnt = 0;
				pstRTBuf[rt_buf_ctrl.module]->state = 0;
				break;
			default:
				LOG_NOTICE(
				  "unsupported module:0x%x\n",
				  rt_buf_ctrl.module);
				break;
			}

			/* spin_unlock_irqrestore( */
			/*   &(IspInfo.SpinLockIrq[irqT_Lock]), flags); */

			break;
		case ISP_RT_BUF_CTRL_DMA_EN: {
			unsigned char array[_cam_max_];
			unsigned int z;
			unsigned char *pExt;

			if (rt_buf_ctrl.pExtend == NULL) {
				LOG_NOTICE("NULL pExtend");
				Ret = -EFAULT;
				break;
			}

			pExt = (unsigned char *)(rt_buf_ctrl.pExtend);
			for (z = 0; z < _cam_max_; z++) {
				if (get_user(array[z],
				  (unsigned char *)pExt) == 0) {
					pstRTBuf[rt_buf_ctrl.module]
					  ->ring_buf[z].active = array[z];
					if (IspInfo.DebugMask
					  & ISP_DBG_BUF_CTRL)
						LOG_INF(
						  "[rtbc][DMA_EN]:dma_%d:%d",
						  z, array[z]);
				} else {
					LOG_NOTICE(
					  "[rtbc][DMA_EN]:get_user failed(%d)",
					  z);
					Ret = -EFAULT;
				}
				pExt++;
			}
		}
		break;
		/* Add this to remove build warning. */
		case ISP_RT_BUF_CTRL_MAX:
			/* Do nothing. */
			break;

		}
	} else {
		LOG_NOTICE("[rtbc]copy_from_user failed");
		Ret = -EFAULT;
	}

	return Ret;
}

#ifndef EP_NO_PMQOS
#ifdef CONFIG_MTK_QOS_SUPPORT
static int ISP_SetPMQOS(unsigned int cmd, unsigned int module)
{
	#define bit 8

	unsigned int bw_cal = 0;
	int Ret = 0;

	switch (cmd) {
	case 0: {
		G_PM_QOS[module].bw_sum = 0;
		G_PM_QOS[module].fps = 0;
		break;
	}
	case 1: {
		 /* MByte/s */
		bw_cal =
		  (G_PM_QOS[module].bw_sum * G_PM_QOS[module].fps) / 1000000;
		break;
	}
	default:
		LOG_NOTICE("unsupport cmd:%d", cmd);
		Ret = -1;
		break;
	}
	pm_qos_update_request(&camsys_qos_request[module], bw_cal);

	if (PMQoS_BW_value != bw_cal) {
		LOG_INF(
		"PM_QoS:module[%d],cmd[%d],bw[%d],fps[%d],total bw = %d MB/s\n",
		module, cmd, G_PM_QOS[module].bw_sum,
		G_PM_QOS[module].fps, bw_cal);
		PMQoS_BW_value = bw_cal;
	}

	return Ret;
}
#endif
#endif

/******************************************************************************
 *
 ******************************************************************************/
static int ISP_REGISTER_IRQ_USERKEY(char *userName)
{
	int key =  -1;
	int i = 0;

	spin_lock((spinlock_t *)(&SpinLock_UserKey));

	/* 1. check the current users is full or not */
	if (FirstUnusedIrqUserKey == IRQ_USER_NUM_MAX) {
		key = -1;
	} else {
		/* 2. check the user had registered or not */
		for (i = 1; i < FirstUnusedIrqUserKey; i++) {
			/*
			 * index 0 is for all the users
			 * that do not register irq first
			 */
			if (strcmp((void *)IrqUserKey_UserInfo[i].userName,
				userName) == 0) {
				key = IrqUserKey_UserInfo[i].userKey;
				break;
			}
		}

		/*
		 * 3.return new userkey for user if the user had not
		 * registered before
		 */
		if (key < 0) {
			/* IrqUserKey_UserInfo[i].userName=userName; */
			memset((void *)IrqUserKey_UserInfo[i].userName,
				0, sizeof(IrqUserKey_UserInfo[i].userName));
			strncpy((void *)IrqUserKey_UserInfo[i].userName,
				userName, USERKEY_STR_LEN-1);
			IrqUserKey_UserInfo[i].userKey = FirstUnusedIrqUserKey;
			key = FirstUnusedIrqUserKey;
			FirstUnusedIrqUserKey++;
		}
	}

	spin_unlock((spinlock_t *)(&SpinLock_UserKey));
	LOG_INF("User(%s)key(%d)\n", userName, key);
	return key;
}

/******************************************************************************
 *
 ******************************************************************************/
static int ISP_FLUSH_IRQ(struct ISP_WAIT_IRQ_STRUCT *irqinfo)
{
	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;

	LOG_INF("type(%d)userKey(%d)St_type(%d)St(0x%x)",
		irqinfo->Type, irqinfo->EventInfo.UserKey,
		irqinfo->EventInfo.St_type, irqinfo->EventInfo.Status);

	if (irqinfo->Type >= ISP_IRQ_TYPE_AMOUNT) {
		LOG_NOTICE("FLUSH_IRQ: type error(%d)", irqinfo->Type);
		return -EFAULT;
	}

	if (irqinfo->EventInfo.St_type >= ISP_IRQ_ST_AMOUNT) {
		LOG_NOTICE("FLUSH_IRQ: st_type error(%d)",
			irqinfo->EventInfo.St_type);
		return -EFAULT;
	}

	if (irqinfo->EventInfo.UserKey >= IRQ_USER_NUM_MAX ||
		irqinfo->EventInfo.UserKey < 0) {
		LOG_NOTICE("FLUSH_IRQ: userkey error(%d)",
			irqinfo->EventInfo.UserKey);
		return -EFAULT;
	}

	/* 1. enable signal */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqinfo->Type]), flags);
	IspInfo.IrqInfo.Status[irqinfo->Type][irqinfo->EventInfo.St_type][
	irqinfo->EventInfo.UserKey] |= irqinfo->EventInfo.Status;
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqinfo->Type]), flags);

	/* 2. force to wake up the user that are waiting for that signal */
	wake_up_interruptible(&IspInfo.WaitQueueHead[irqinfo->Type]);

	return 0;
}


/******************************************************************************
 *
 ******************************************************************************/
static int ISP_WaitIrq(struct ISP_WAIT_IRQ_STRUCT *WaitIrq)
{
	int Ret = 0, Timeout = WaitIrq->EventInfo.Timeout;
	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;
	unsigned int irqStatus;

	int idx = my_get_pow_idx(WaitIrq->EventInfo.Status);
	struct timeval time_getrequest;
	struct timeval time_ready2return;
	bool freeze_passbysigcnt = false;
	unsigned long long  sec = 0;
	unsigned long       usec = 0;

	/* do_gettimeofday(&time_getrequest); */
	sec = cpu_clock(0);     /* ns */
	do_div(sec, 1000);    /* usec */
	usec = do_div(sec, 1000000);    /* sec and usec */
	time_getrequest.tv_usec = (unsigned int)usec;
	time_getrequest.tv_sec = (unsigned int)sec;

	if (WaitIrq->Type >= ISP_IRQ_TYPE_AMOUNT) {
		LOG_NOTICE("WaitIrq: type error(%d)", WaitIrq->Type);
		return -EFAULT;
	}

	if (WaitIrq->EventInfo.St_type >= ISP_IRQ_ST_AMOUNT) {
		LOG_NOTICE("WaitIrq: st_type error(%d)",
			WaitIrq->EventInfo.St_type);
		return -EFAULT;
	}

	if (WaitIrq->EventInfo.UserKey >= IRQ_USER_NUM_MAX ||
		WaitIrq->EventInfo.UserKey < 0) {
		LOG_NOTICE("WaitIrq: userkey error(%d)",
			WaitIrq->EventInfo.UserKey);
		return -EFAULT;
	}

#ifdef ENABLE_WAITIRQ_LOG
	/* Debug interrupt */
	if (IspInfo.DebugMask & ISP_DBG_INT) {
		if (WaitIrq->EventInfo.Status & IspInfo.IrqInfo.Mask[
			WaitIrq->Type][WaitIrq->EventInfo.St_type]) {
			if (WaitIrq->EventInfo.UserKey > 0) {
				LOG_DBG(
				    "+WaitIrq Clear(%d), Type(%d), Status(0x%08X), Timeout(%d/%d),user(%d)\n",
				    WaitIrq->EventInfo.Clear,
				    WaitIrq->Type,
				    WaitIrq->EventInfo.Status,
				    Timeout, WaitIrq->EventInfo.Timeout,
				    WaitIrq->EventInfo.UserKey);
			}
		}
	}
#endif

	/* 1. wait type update */
	if (WaitIrq->EventInfo.Clear == ISP_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
		/* LOG_DBG("Clear(%d),Type(%d):IrqStatus(0x%08X) cleared",
		 * WaitIrq->EventInfo.Clear,WaitIrq->Type,
		 * IspInfo.IrqInfo.Status[WaitIrq->Type]);
		 */
		IspInfo.IrqInfo.Status[WaitIrq->Type][
			WaitIrq->EventInfo.St_type][
			WaitIrq->EventInfo.UserKey] &=
			(~WaitIrq->EventInfo.Status);

		spin_unlock_irqrestore(
			&(IspInfo.SpinLockIrq[WaitIrq->Type]),
			flags);
		return Ret;
	}
	{
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
		if (WaitIrq->EventInfo.Status &
		    IspInfo.IrqInfo.MarkedFlag[
		    WaitIrq->Type][WaitIrq->EventInfo.St_type][
		    WaitIrq->EventInfo.UserKey]) {
			spin_unlock_irqrestore(
				&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);

			/* force to be non_clear wait if marked before,
			 * and check the request wait timing
			 * if the entry time of wait request after mark
			 * is before signal, we freese the counting for
			 * passby signal
			 */

			/* v : kernel receive mark request */
			/* o : kernel receive wait request */
			/* : return to user */
			/*  */
			/* case: freeze is true, and passby signal count = 0 */
			/*  */
			/* |                                              | */
			/* |                                  (wait)    | */
			/* |       v-------------o++++++ | */
			/* |                                              | */
			/* Sig                                            Sig */
			/*  */
			/* case: freeze is false, and passby signal count = 1 */
			/*  */
			/* |                                              | */
			/* |                                              | */
			/* |       v---------------------- |-o  (return) */
			/* |                                              | */
			/* Sig                                            Sig */
			/*  */

			freeze_passbysigcnt = !(ISP_GetIRQState(
						WaitIrq->Type,
						WaitIrq->EventInfo.St_type,
						WaitIrq->EventInfo.UserKey,
						WaitIrq->EventInfo.Status));
		} else {
			spin_unlock_irqrestore(
				&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);

			if (WaitIrq->EventInfo.Clear == ISP_IRQ_CLEAR_WAIT) {
				spin_lock_irqsave(
					&(IspInfo.SpinLockIrq[WaitIrq->Type]),
					flags);

				if (IspInfo.IrqInfo.Status[WaitIrq->Type][
				    WaitIrq->EventInfo.St_type][
				    WaitIrq->EventInfo.UserKey] &
				    WaitIrq->EventInfo.Status)
					IspInfo.IrqInfo.Status[WaitIrq->Type][
					WaitIrq->EventInfo.St_type][
					WaitIrq->EventInfo.UserKey] &=
					(~WaitIrq->EventInfo.Status);

				spin_unlock_irqrestore(
					&(IspInfo.SpinLockIrq[WaitIrq->Type]),
					flags);
			} else if (WaitIrq->EventInfo.Clear ==
				  ISP_IRQ_CLEAR_ALL) {
				spin_lock_irqsave(
					&(IspInfo.SpinLockIrq[WaitIrq->Type]),
					flags);

				IspInfo.IrqInfo.Status[WaitIrq->Type][
					WaitIrq->EventInfo.St_type][
					WaitIrq->EventInfo.UserKey] = 0;
				spin_unlock_irqrestore(
					&(IspInfo.SpinLockIrq[WaitIrq->Type]),
					flags);
			}
		}
	}

	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
	irqStatus = IspInfo.IrqInfo.Status[WaitIrq->Type][
		WaitIrq->EventInfo.St_type][WaitIrq->EventInfo.UserKey];
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);

	if (WaitIrq->EventInfo.Clear == ISP_IRQ_CLEAR_NONE) {
		if (IspInfo.IrqInfo.Status[WaitIrq->Type][
		    WaitIrq->EventInfo.St_type][WaitIrq->EventInfo.UserKey] &
		    WaitIrq->EventInfo.Status) {
#ifdef ENABLE_WAITIRQ_LOG
			LOG_INF("%s,%s",
			"Already have irq!!!: WaitIrq Timeout(%d) Clear(%d), Type(%d), StType(%d)",
			", IrqStatus(0x%08X), WaitStatus(0x%08X), Timeout(%d), userKey(%d)\n",
				WaitIrq->EventInfo.Timeout,
				WaitIrq->EventInfo.Clear,
				WaitIrq->Type,
				WaitIrq->EventInfo.St_type,
				irqStatus,
				WaitIrq->EventInfo.Status,
				WaitIrq->EventInfo.Timeout,
				WaitIrq->EventInfo.UserKey);
#endif
			goto NON_CLEAR_WAIT;
		}
	}

#ifdef ENABLE_WAITIRQ_LOG
	LOG_INF(
	    "before wait: Clear(%d) Type(%d) StType(%d) Sts(0x%08X) WaitSts(0x%08X) Timeout(%d) userKey(%d)\n",
	    WaitIrq->EventInfo.Clear,
	    WaitIrq->Type,
	    WaitIrq->EventInfo.St_type,
	    irqStatus,
	    WaitIrq->EventInfo.Status,
	    WaitIrq->EventInfo.Timeout,
	    WaitIrq->EventInfo.UserKey);
#endif

	/* 2. start to wait signal */
	Timeout = wait_event_interruptible_timeout(
			  IspInfo.WaitQueueHead[WaitIrq->Type],
			  ISP_GetIRQState(WaitIrq->Type,
			  WaitIrq->EventInfo.St_type,
			  WaitIrq->EventInfo.UserKey,
			  WaitIrq->EventInfo.Status),
			  ISP_MsToJiffies(WaitIrq->EventInfo.Timeout));
	/* check if user is interrupted by system signal */
	if ((Timeout != 0) && (!ISP_GetIRQState(WaitIrq->Type,
	    WaitIrq->EventInfo.St_type, WaitIrq->EventInfo.UserKey,
	    WaitIrq->EventInfo.Status))) {
		LOG_DBG(
		    "interrupted by system signal,return value(%d),irq Type/User/Sts(0x%x/%d/0x%x)\n",
		    Timeout, WaitIrq->Type,
		    WaitIrq->EventInfo.UserKey,
		    WaitIrq->EventInfo.Status);
		Ret = -ERESTARTSYS;  /* actually it should be -ERESTARTSYS */
		goto EXIT;
	}
	/* timeout */
	if (Timeout == 0) {
		/* Store irqinfo status in here
		 * to redeuce time of spin_lock_irqsave
		 */
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = IspInfo.IrqInfo.Status[
			    WaitIrq->Type][
			    WaitIrq->EventInfo.St_type][
			    WaitIrq->EventInfo.UserKey];

		spin_unlock_irqrestore(
			&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);

		LOG_NOTICE(
			"ERRRR WaitIrq Clear(%d) Type(%d) StType(%d) Status(0x%08X) WaitStatus(0x%08X) Timeout(%d) key(%d)\n",
			WaitIrq->EventInfo.Clear,
			WaitIrq->Type,
			WaitIrq->EventInfo.St_type,
			irqStatus,
			WaitIrq->EventInfo.Status,
			WaitIrq->EventInfo.Timeout,
			WaitIrq->EventInfo.UserKey);

		Ret = -EFAULT;
		goto EXIT;
	}
#ifdef ENABLE_WAITIRQ_LOG
	else {
		/* Store irqinfo status in here
		 * to redeuce time of spin_lock_irqsave
		 */
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = IspInfo.IrqInfo.Status[WaitIrq->Type][
			WaitIrq->EventInfo.St_type][WaitIrq->EventInfo.UserKey];
		spin_unlock_irqrestore(
			&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);

		LOG_INF(
		    "Done WaitIrq Clear(%d) Type(%d) StType(%d) Status(0x%08X) WaitStatus(0x%08X) Timeout(%d) key(%d)\n",
		    WaitIrq->EventInfo.Clear,
		    WaitIrq->Type,
		    WaitIrq->EventInfo.St_type,
		    irqStatus,
		    WaitIrq->EventInfo.Status,
		    WaitIrq->EventInfo.Timeout,
		    WaitIrq->EventInfo.UserKey);
	}
#endif

NON_CLEAR_WAIT:
	/* 3. get interrupt and update time related information
	 * that would be return to user do_gettimeofday(&time_ready2return);
	 */
	sec = cpu_clock(0);     /* ns */
	do_div(sec, 1000);    /* usec */
	usec = do_div(sec, 1000000);    /* sec and usec */
	time_ready2return.tv_usec = usec;
	time_ready2return.tv_sec = sec;


	spin_lock_irqsave(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
	/* clear the status if someone get the irq */
	IspInfo.IrqInfo.Status[WaitIrq->Type][WaitIrq->EventInfo.St_type][
		WaitIrq->EventInfo.UserKey] &= (~WaitIrq->EventInfo.Status);
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);

#if 0
	/* time period for 3A */
	if (WaitIrq->EventInfo.Status & IspInfo.IrqInfo.MarkedFlag[
	    WaitIrq->Type][WaitIrq->EventInfo.UserKey]) {
		WaitIrq->EventInfo.TimeInfo.tMark2WaitSig_usec =
			(time_getrequest.tv_usec -
			IspInfo.IrqInfo.MarkedTime_usec[WaitIrq->Type][
			idx][WaitIrq->EventInfo.UserKey]);
		WaitIrq->EventInfo.TimeInfo.tMark2WaitSig_sec =
			(time_getrequest.tv_sec -
			IspInfo.IrqInfo.MarkedTime_sec[WaitIrq->Type][
			idx][WaitIrq->EventInfo.UserKey]));
		if ((int)(WaitIrq->EventInfo.TimeInfo.tMark2WaitSig_usec) < 0) {
			WaitIrq->EventInfo.TimeInfo.tMark2WaitSig_sec =
			    WaitIrq->EventInfo.TimeInfo.tMark2WaitSig_sec - 1;
			if ((int)(WaitIrq->EventInfo
			    .TimeInfo.tMark2WaitSig_sec) < 0)
				WaitIrq->EventInfo
				    .TimeInfo.tMark2WaitSig_sec = 0;

			WaitIrq->EventInfo.TimeInfo.tMark2WaitSig_usec =
				1 * 1000000 +
				WaitIrq->EventInfo.TimeInfo.tMark2WaitSig_usec;
		}
		/*  */
		WaitIrq->EventInfo.TimeInfo.tLastSig2GetSig_usec =
			(time_ready2return.tv_usec -
			IspInfo.IrqInfo.LastestSigTime_usec[
			WaitIrq->Type][idx]);
		WaitIrq->EventInfo.TimeInfo.tLastSig2GetSig_sec =
			(time_ready2return.tv_sec -
			IspInfo.IrqInfo.LastestSigTime_sec[WaitIrq->Type][idx]);
		if ((int)(WaitIrq->EventInfo
		    .TimeInfo.tLastSig2GetSig_usec) < 0) {
			WaitIrq->EventInfo.TimeInfo.tLastSig2GetSig_sec =
			    WaitIrq->EventInfo
			    .TimeInfo.tLastSig2GetSig_sec - 1;
			if ((int)(WaitIrq->EventInfo
			    .TimeInfo.tLastSig2GetSig_sec) < 0)
				WaitIrq->EventInfo
				    .TimeInfo.tLastSig2GetSig_sec = 0;

			WaitIrq->EventInfo.TimeInfo.tLastSig2GetSig_usec =
			  1 * 1000000 +
			  WaitIrq->EventInfo.TimeInfo.tLastSig2GetSig_usec;
		}
		/*  */
		if (freeze_passbysigcnt)
			WaitIrq->EventInfo.TimeInfo.passedbySigcnt =
				IspInfo.IrqInfo.PassedBySigCnt[WaitIrq->Type][
				idx][WaitIrq->EventInfo.UserKey] - 1;
		else
			WaitIrq->EventInfo.TimeInfo.passedbySigcnt =
				IspInfo.IrqInfo.PassedBySigCnt[WaitIrq->Type][
				idx][WaitIrq->EventInfo.UserKey];

	}
	IspInfo.IrqInfo.Status[
		WaitIrq->Type][WaitIrq->EventInfo.UserKey] &=
		(~WaitIrq->EventInfo.Status);
	/* clear the status if someone get the irq */
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
	if (WaitIrq->EventInfo.UserKey > 0) {
		LOG_VRB(
		    "[WAITIRQv3]user(%d) mark sec/usec (%d/%d),last irq sec/usec(%d/%d),enterwait(%d/%d),getIRQ(%d/%d)\n",
		    WaitIrq->EventInfo.UserKey,
		    IspInfo.IrqInfo.MarkedTime_sec[WaitIrq->Type][
			idx][WaitIrq->EventInfo.UserKey],
		    IspInfo.IrqInfo.MarkedTime_usec[WaitIrq->Type][
			idx][WaitIrq->EventInfo.UserKey],
		    IspInfo.IrqInfo.LastestSigTime_sec[WaitIrq->Type][idx],
		    IspInfo.IrqInfo.LastestSigTime_usec[WaitIrq->Type][idx],
		    (int)(time_getrequest.tv_sec),
		    (int)(time_getrequest.tv_usec),
		    (int)(time_ready2return.tv_sec),
		    (int)(time_ready2return.tv_usec));
		LOG_VRB(
		    "[WAITIRQv3]user(%d)  sigNum(%d/%d), mark sec/usec (%d/%d), irq sec/usec (%d/%d),user(0x%x)\n",
		    WaitIrq->EventInfo.UserKey,
		    IspInfo.IrqInfo.PassedBySigCnt[WaitIrq->Type][
			idx][WaitIrq->EventInfo.UserKey],
		    WaitIrq->EventInfo.TimeInfo.passedbySigcnt,
		    WaitIrq->EventInfo.TimeInfo.tMark2WaitSig_sec,
		    WaitIrq->EventInfo.TimeInfo.tMark2WaitSig_usec,
		    WaitIrq->EventInfo.TimeInfo.tLastSig2GetSig_sec,
		    WaitIrq->EventInfo.TimeInfo.tLastSig2GetSig_usec,
		    WaitIrq->EventInfo.UserKey);
	}
#endif

EXIT:
	/* 4. clear mark flag / reset marked time / reset time related
	 * info and passedby signal count
	 */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
	if (WaitIrq->EventInfo.Status &
	    IspInfo.IrqInfo.MarkedFlag[
	    WaitIrq->Type][
	    WaitIrq->EventInfo.St_type][
	    WaitIrq->EventInfo.UserKey]) {
		IspInfo.IrqInfo.MarkedFlag[
		    WaitIrq->Type][
		    WaitIrq->EventInfo.St_type][
		    WaitIrq->EventInfo.UserKey] &=
		    (~WaitIrq->EventInfo.Status);
		IspInfo.IrqInfo.MarkedTime_usec[WaitIrq->Type][
		  idx][WaitIrq->EventInfo.UserKey] = 0;
		IspInfo.IrqInfo.MarkedTime_sec[WaitIrq->Type][
		  idx][WaitIrq->EventInfo.UserKey] = 0;
		IspInfo.IrqInfo.PassedBySigCnt[WaitIrq->Type][
		  idx][WaitIrq->EventInfo.UserKey] = 0;
	}
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);


	return Ret;
}


#ifdef ENABLE_KEEP_ION_HANDLE
/******************************************************************************
 *
 ******************************************************************************/
static void ISP_ion_init(void)
{
	if (!pIon_client && g_ion_device)
		pIon_client = ion_client_create(g_ion_device, "camera_isp");

	if (!pIon_client) {
		LOG_NOTICE("invalid ion client!\n");
		return;
	}

	if (IspInfo.DebugMask & ISP_DBG_ION_CTRL)
		LOG_INF("create ion client 0x%p\n", pIon_client);
}

/******************************************************************************
 *
 ******************************************************************************/
static void ISP_ion_uninit(void)
{
	if (!pIon_client) {
		LOG_NOTICE("invalid ion client!\n");
		return;
	}

	if (IspInfo.DebugMask & ISP_DBG_ION_CTRL)
		LOG_INF("destroy ion client 0x%p\n", pIon_client);

	ion_client_destroy(pIon_client);

	pIon_client = NULL;
}

/******************************************************************************
 *
 ******************************************************************************/
static struct ion_handle *ISP_ion_import_handle(
	struct ion_client *client, int fd)
{
	struct ion_handle *handle = NULL;

	if (!client) {
		LOG_NOTICE("invalid ion client!\n");
		return handle;
	}
	if (fd == -1) {
		LOG_NOTICE("invalid ion fd!\n");
		return handle;
	}

	handle = ion_import_dma_buf_fd(client, fd);

	if (IS_ERR(handle)) {
		LOG_NOTICE("import ion handle failed!\n");
		return NULL;
	}

	if (IspInfo.DebugMask & ISP_DBG_ION_CTRL)
		LOG_INF("[ion_import_hd] Hd(0x%p)\n", handle);
	return handle;
}

/******************************************************************************
 *
 ******************************************************************************/
static void ISP_ion_free_handle(
	struct ion_client *client, struct ion_handle *handle)
{
	if (!client) {
		LOG_NOTICE("invalid ion client!\n");
		return;
	}
	if (!handle)
		return;

	if (IspInfo.DebugMask & ISP_DBG_ION_CTRL)
		LOG_INF("[ion_free_hd] Hd(0x%p)\n", handle);

	ion_free(client, handle);

}

/*****************************************************************************
 *
 ****************************************************************************/
static void ISP_ion_free_handle_by_module(unsigned int module)
{
	int i, j;
	int nFd;
	struct ion_handle *p_IonHnd;
	struct T_ION_TBL *ptbl = &gION_TBL[module];

	if (IspInfo.DebugMask & ISP_DBG_ION_CTRL)
		LOG_INF("[ion_free_hd_by_module]%d\n", module);

	for (i = 0; i < _dma_max_wr_; i++) {
		unsigned int jump = i*_ion_keep_max_;

		for (j = 0; j < _ion_keep_max_ ; j++) {
			spin_lock(&(ptbl->pLock[i]));
			/* */
			if (ptbl->pIonFd[jump + j] == 0) {
				spin_unlock(&(ptbl->pLock[i]));
				continue;
			}
			nFd = ptbl->pIonFd[jump + j];
			p_IonHnd = ptbl->pIonHnd[jump + j];
			/* */
			ptbl->pIonFd[jump + j] = 0;
			ptbl->pIonHnd[jump + j] = NULL;
			ptbl->pIonCt[jump + j] = 0;
			spin_unlock(&(ptbl->pLock[i]));
			/* */
			if (IspInfo.DebugMask & ISP_DBG_ION_CTRL) {
				LOG_INF(
				"ion_free:dev(%d)dma(%d)j(%d)fd(%d)Hnd(0x%p)\n",
				module, i, j, nFd, p_IonHnd);
			}
			/*can't in spin_lock*/
			ISP_ion_free_handle(pIon_client, p_IonHnd);
		}
	}
}

#endif


/******************************************************************************
 *
 ******************************************************************************/
static long ISP_ioctl(struct file *pFile, unsigned int Cmd, unsigned long Param)
{
	int Ret = 0;
	/*  */
	unsigned int DebugFlag[3] = {0};
	unsigned int Dapc_Reg[6] = {0};
	struct ISP_REG_IO_STRUCT       RegIo;
	struct ISP_WAIT_IRQ_STRUCT     IrqInfo;
	struct ISP_CLEAR_IRQ_STRUCT    ClearIrq;
	struct ISP_USER_INFO_STRUCT *pUserInfo;
	unsigned int                 wakelock_ctrl;
	unsigned int                 module;
	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;
	int userKey =  -1;
	struct ISP_REGISTER_USERKEY_STRUCT RegUserKey;
	int i;
	#ifdef ENABLE_KEEP_ION_HANDLE
	struct ISP_DEV_ION_NODE_STRUCT IonNode;
	struct ion_handle *handle;
	struct ion_handle *p_IonHnd;
	#endif

	struct ISP_CLK_INFO ispclks;
	u32 lv = 0;

	/*  */
	if (pFile->private_data == NULL) {
		LOG_NOTICE(
		  "private_data is NULL,(process, pid, tgid)=(%s, %d, %d)\n",
		  current->comm, current->pid, current->tgid);
		return -EFAULT;
	}
	/*  */
	pUserInfo = (struct ISP_USER_INFO_STRUCT *)(pFile->private_data);
	/*  */
	switch (Cmd) {
	case ISP_WAKELOCK_CTRL:
		if (copy_from_user(&wakelock_ctrl, (void *)Param,
		    sizeof(unsigned int)) != 0) {
			LOG_NOTICE(
			  "get ISP_WAKELOCK_CTRL from user fail\n");
			Ret = -EFAULT;
		} else {
			if (wakelock_ctrl == 1) {       /* Enable wakelock */
				if (g_WaitLockCt) {
					g_WaitLockCt++;
					LOG_DBG("add wakelock cnt(%d)\n",
					  g_WaitLockCt);
				} else {
#ifdef CONFIG_PM_WAKELOCKS
					__pm_stay_awake(&isp_wake_lock);
#else
					wake_lock(&isp_wake_lock);
#endif
					g_WaitLockCt++;
					LOG_DBG("wakelock enable!! cnt(%d)\n",
					  g_WaitLockCt);
				}
			} else {        /* Disable wakelock */
				if (g_WaitLockCt)
					g_WaitLockCt--;

				if (g_WaitLockCt)
					LOG_DBG("subtract wakelock cnt(%d)\n",
					  g_WaitLockCt);
				else {
#ifdef CONFIG_PM_WAKELOCKS
					__pm_relax(&isp_wake_lock);
#else
					wake_unlock(&isp_wake_lock);
#endif
					LOG_DBG("wakelock disable!! cnt(%d)\n",
					  g_WaitLockCt);
				}
			}
		}
		break;
	case ISP_GET_DROP_FRAME:
		if (copy_from_user(&DebugFlag[0], (void *)Param,
		    sizeof(unsigned int)) != 0) {
			LOG_NOTICE("get irq from user fail\n");
			Ret = -EFAULT;
		} else {

			if (DebugFlag[0] < ISP_IRQ_TYPE_INT_CAM_A_ST ||
				DebugFlag[0] > ISP_IRQ_TYPE_INT_CAMSV_1_ST) {
				LOG_NOTICE("err TG(0x%x)\n", DebugFlag[0]);
				Ret = -EFAULT;
				break;
			}

			spin_lock_irqsave(
				&(IspInfo.SpinLockIrq[DebugFlag[0]]), flags);
			DebugFlag[1] = FrameStatus[DebugFlag[0]];
			spin_unlock_irqrestore(
				&(IspInfo.SpinLockIrq[DebugFlag[0]]), flags);

			if (copy_to_user((void *)Param,
				&DebugFlag[1], sizeof(unsigned int)) != 0) {
				LOG_NOTICE("copy to user fail\n");
				Ret = -EFAULT;
			}
		}
		break;
	case ISP_GET_INT_ERR:
		if (copy_to_user((void *)Param, (void *)g_ISPIntStatus,
		    sizeof(struct ISP_RAW_INT_STATUS)*ISP_IRQ_TYPE_AMOUNT) != 0)
			LOG_NOTICE("get int err fail\n");
		else
			memset((void *)g_ISPIntStatus,
				0, sizeof(g_ISPIntStatus));

		break;
	case ISP_GET_DMA_ERR:
		if (copy_from_user(&DebugFlag[0], (void *)Param,
		    sizeof(unsigned int)) != 0) {
			LOG_NOTICE("get module fail\n");
			Ret = -EFAULT;
		} else {
			if (DebugFlag[0] >= (ISP_IRQ_TYPE_AMOUNT)) {
				LOG_NOTICE("module error(%d)\n", DebugFlag[0]);
				Ret = -EFAULT;
				break;
			}
			if (copy_to_user(
				(void *)Param, &g_DmaErr_CAM[DebugFlag[0]],
				sizeof(unsigned int)*_cam_max_) != 0)
				LOG_NOTICE("get dma_err fail\n");

		}
		break;
	case ISP_GET_CUR_SOF:
		if (copy_from_user(&DebugFlag[0], (void *)Param,
		    sizeof(unsigned int)) != 0) {
			LOG_NOTICE("get cur sof from user fail\n");
			Ret = -EFAULT;
		} else {
			if (DebugFlag[0] < ISP_IRQ_TYPE_INT_CAM_A_ST ||
				DebugFlag[0] >= ISP_IRQ_TYPE_AMOUNT) {
				LOG_NOTICE("cursof: error type(%d)\n"
					, DebugFlag[0]);
				Ret = -EFAULT;
				break;
			}
			DebugFlag[1] = sof_count[DebugFlag[0]];
		}
		if (copy_to_user((void *)Param,
		    &DebugFlag[1], sizeof(unsigned int)) != 0) {
			LOG_NOTICE("copy to user fail\n");
			Ret = -EFAULT;
		}
		break;
	case ISP_RESET_BY_HWMODULE: {
		if (copy_from_user(&module, (void *)Param,
		    sizeof(unsigned int)) != 0) {
			LOG_NOTICE("get hwmodule from user fail\n");
			Ret = -EFAULT;
		} else {
			ISP_Reset(module);
		}
		break;
	}
	case ISP_READ_REGISTER: {
		if (copy_from_user(&RegIo, (void *)Param,
		    sizeof(struct ISP_REG_IO_STRUCT)) == 0) {
			/* 2nd layer behavoir of copy from user
			 * is implemented in ISP_ReadReg(...)
			 */
			Ret = ISP_ReadReg(&RegIo);
		} else {
			LOG_NOTICE("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	}
	case ISP_WRITE_REGISTER: {
		if (copy_from_user(&RegIo, (void *)Param,
		    sizeof(struct ISP_REG_IO_STRUCT)) == 0) {
			/* 2nd layer behavoir of copy from user
			 * is implemented in ISP_WriteReg(...)
			 */
			Ret = ISP_WriteReg(&RegIo);
		} else {
			LOG_NOTICE("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	}
	case ISP_WAIT_IRQ: {
		if (copy_from_user(&IrqInfo, (void *)Param,
		    sizeof(struct ISP_WAIT_IRQ_STRUCT)) == 0) {
			/*  */
			if ((IrqInfo.Type >= ISP_IRQ_TYPE_AMOUNT) ||
				(IrqInfo.Type < 0)) {
				Ret = -EFAULT;
				LOG_NOTICE("invalid type(%d)\n", IrqInfo.Type);
				goto EXIT;
			}

			if ((IrqInfo.EventInfo.St_type >= ISP_IRQ_ST_AMOUNT) ||
				(IrqInfo.EventInfo.St_type < 0)) {
				LOG_NOTICE(
				    "invalid St_type(%d), max(%d), force St_type = 0\n",
				    IrqInfo.EventInfo.St_type,
				    ISP_IRQ_ST_AMOUNT);
				IrqInfo.EventInfo.St_type = 0;
			}

			if ((IrqInfo.EventInfo.UserKey >= IRQ_USER_NUM_MAX) ||
				(IrqInfo.EventInfo.UserKey < 0)) {
				LOG_NOTICE(
				    "invalid userKey(%d), max(%d), force userkey = 0\n",
				    IrqInfo.EventInfo.UserKey,
				    IRQ_USER_NUM_MAX);
				IrqInfo.EventInfo.UserKey = 0;
			}
#ifdef ENABLE_WAITIRQ_LOG
			LOG_INF(
			    "IRQ type(%d), userKey(%d), timeout(%d), userkey(%d), st_status(%d), status(%d)\n",
			    IrqInfo.Type,
			    IrqInfo.EventInfo.UserKey,
			    IrqInfo.EventInfo.Timeout,
			    IrqInfo.EventInfo.UserKey,
			    IrqInfo.EventInfo.St_type,
			    IrqInfo.EventInfo.Status);
#endif
			Ret = ISP_WaitIrq(&IrqInfo);
		} else {
			LOG_NOTICE("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	}
	case ISP_CLEAR_IRQ: {
		if (copy_from_user(&ClearIrq, (void *)Param,
		    sizeof(struct ISP_CLEAR_IRQ_STRUCT)) == 0) {
			LOG_DBG("ISP_CLEAR_IRQ Type(%d)\n", ClearIrq.Type);

			if ((ClearIrq.Type >= ISP_IRQ_TYPE_AMOUNT) ||
				(ClearIrq.Type < 0)) {
				Ret = -EFAULT;
				LOG_NOTICE("invalid type(%d)\n", ClearIrq.Type);
				goto EXIT;
			}

			if ((ClearIrq.EventInfo.St_type >= ISP_IRQ_ST_AMOUNT) ||
				(ClearIrq.EventInfo.St_type < 0)) {
				LOG_NOTICE(
				    "invalid St_type(%d), max(%d), force St_type = 0\n",
				    ClearIrq.EventInfo.St_type,
				    ISP_IRQ_ST_AMOUNT);
				ClearIrq.EventInfo.St_type = 0;
			}

			/*  */
			if ((ClearIrq.EventInfo.UserKey >= IRQ_USER_NUM_MAX) ||
				(ClearIrq.EventInfo.UserKey < 0)) {
				LOG_NOTICE("errUserEnum(%d)",
					ClearIrq.EventInfo.UserKey);
				Ret = -EFAULT;
				goto EXIT;
			}

			/*avoid line over 120 char per line */
			i = ClearIrq.EventInfo.UserKey;
			LOG_DBG(
			    "ISP_CLEAR_IRQ:Type(%d),Status(0x%x),st_status(%d),IrqStatus(0x%x)\n",
			    ClearIrq.Type,
			    ClearIrq.EventInfo.Status,
			    ClearIrq.EventInfo.St_type,
			    IspInfo.IrqInfo.Status[ClearIrq.Type][
			    ClearIrq.EventInfo.St_type][i]);
			spin_lock_irqsave(
				&(IspInfo.SpinLockIrq[ClearIrq.Type]), flags);
			IspInfo.IrqInfo.Status[
				ClearIrq.Type][
				ClearIrq.EventInfo.St_type][
				ClearIrq.EventInfo.UserKey] &=
				(~ClearIrq.EventInfo.Status);
			spin_unlock_irqrestore(
				&(IspInfo.SpinLockIrq[ClearIrq.Type]), flags);
		} else {
			LOG_NOTICE("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	}
	/*  */
	case ISP_REGISTER_IRQ_USER_KEY:
		if (copy_from_user(&RegUserKey, (void *)Param,
		    sizeof(struct ISP_REGISTER_USERKEY_STRUCT)) == 0) {
			userKey = ISP_REGISTER_IRQ_USERKEY(RegUserKey.userName);
			RegUserKey.userKey = userKey;
			if (copy_to_user((void *)Param, &RegUserKey,
			    sizeof(struct ISP_REGISTER_USERKEY_STRUCT)) != 0)
				LOG_NOTICE("copy_to_user failed\n");

			if (RegUserKey.userKey < 0) {
				LOG_NOTICE("query irq user key fail\n");
				Ret = -1;
			}
		} else {
			LOG_NOTICE("copy from user fail\n");
		}

		break;
	/*  */
	case ISP_FLUSH_IRQ_REQUEST:
		if (copy_from_user(&IrqInfo, (void *)Param,
		    sizeof(struct ISP_WAIT_IRQ_STRUCT)) == 0) {
			if ((IrqInfo.EventInfo.UserKey >= IRQ_USER_NUM_MAX) ||
				(IrqInfo.EventInfo.UserKey < 0)) {
				LOG_NOTICE("invalid userKey(%d), max(%d)\n",
					IrqInfo.EventInfo.UserKey,
					IRQ_USER_NUM_MAX);
				Ret = -EFAULT;
				break;
			}
			if ((IrqInfo.Type >= ISP_IRQ_TYPE_AMOUNT) ||
				(IrqInfo.Type < 0)) {
				LOG_NOTICE("invalid type(%d), max(%d)\n",
					IrqInfo.Type,
					ISP_IRQ_TYPE_AMOUNT);
				Ret = -EFAULT;
				break;
			}
			if ((IrqInfo.EventInfo.St_type >= ISP_IRQ_ST_AMOUNT) ||
				(IrqInfo.EventInfo.St_type < 0)) {
				LOG_NOTICE(
				    "invalid St_type(%d), max(%d), force St_type = 0\n",
				    IrqInfo.EventInfo.St_type,
				    ISP_IRQ_ST_AMOUNT);
				IrqInfo.EventInfo.St_type = 0;
			}

			Ret = ISP_FLUSH_IRQ(&IrqInfo);
		}
		break;
	case ISP_DEBUG_FLAG:
		if (copy_from_user(DebugFlag, (void *)Param,
		    sizeof(unsigned int)) == 0) {

			IspInfo.DebugMask = DebugFlag[0];

			/* LOG_DBG("FBC kernel debug level = %x\n" */
			/*	,IspInfo.DebugMask); */
		} else {
			LOG_NOTICE("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	case ISP_BUFFER_CTRL:
		Ret = ISP_Buf_CTRL_FUNC(Param);
		break;
	case ISP_VF_LOG:
		if (copy_from_user(DebugFlag, (void *)Param,
		    sizeof(unsigned int) * 3) == 0) {
			unsigned int vf,
				module = ISP_IRQ_TYPE_INT_CAM_A_ST,
				cam_dmao = 0;

			if (DebugFlag[1] < ISP_CAMSYS_CONFIG_IDX ||
				DebugFlag[1] > ISP_CAMSV5_IDX) {
				LOG_NOTICE("CAM Index is out of range:%d",
					DebugFlag[1]);
				Ret = -EFAULT;
				break;
			}

			switch (DebugFlag[1]) {
			case ISP_CAM_A_IDX:
				module = ISP_IRQ_TYPE_INT_CAM_A_ST;
				break;
			case ISP_CAM_B_IDX:
				module = ISP_IRQ_TYPE_INT_CAM_B_ST;
				break;
			case ISP_CAM_C_IDX:
				module = ISP_IRQ_TYPE_INT_CAM_C_ST;
				break;
			case ISP_CAMSV0_IDX:
				module = ISP_IRQ_TYPE_INT_CAMSV_0_ST;
				break;
			case ISP_CAMSV1_IDX:
				module = ISP_IRQ_TYPE_INT_CAMSV_1_ST;
				break;
			case ISP_CAMSV2_IDX:
				module = ISP_IRQ_TYPE_INT_CAMSV_2_ST;
				break;
			case ISP_CAMSV3_IDX:
				module = ISP_IRQ_TYPE_INT_CAMSV_3_ST;
				break;
			case ISP_CAMSV4_IDX:
				module = ISP_IRQ_TYPE_INT_CAMSV_4_ST;
				break;
			case ISP_CAMSV5_IDX:
				module = ISP_IRQ_TYPE_INT_CAMSV_5_ST;
				break;
			}

			vf = ISP_RD32(CAM_REG_TG_VF_CON(DebugFlag[1]));

			switch (DebugFlag[0]) {
			case 1: {
				if (sec_on)
					cam_dmao = lock_reg.CAM_REG_CTL_DMA_EN[
							DebugFlag[1]];
				else
					cam_dmao = ISP_RD32(
						    CAM_REG_CTL_DMA_EN(
						    DebugFlag[1]));

				LOG_INF("CAM_%d viewFinder is ON(SecOn:0x%x)\n",
					module, sec_on);

				if (vf & 0x1)
					LOG_NOTICE(
					    "CAM_%d: vf already enabled\n",
					    module);
				else
					ISP_WR32(
						CAM_REG_TG_VF_CON(DebugFlag[1]),
						(vf+0x1));

#if (TIMESTAMP_QUEUE_EN == 1)
				memset(
				    (void *)&(IspInfo.TstpQInfo[module]), 0,
				    sizeof(struct ISP_TIMESTPQ_INFO_STRUCT));
				g1stSwP1Done[module] = MTRUE;
				gPrevSofTimestp[module] = 0;
#endif
				pstRTBuf[module]->ring_buf[_imgo_].active =
				    ((cam_dmao & 0x1) ? (MTRUE) : (MFALSE));
				pstRTBuf[module]->ring_buf[_rrzo_].active =
				    ((cam_dmao & 0x4) ? (MTRUE) : (MFALSE));
				pstRTBuf[module]->ring_buf[_lcso_].active =
				    ((cam_dmao & 0x10) ? (MTRUE) : (MFALSE));
				pstRTBuf[module]->ring_buf[_lmvo_].active =
				    ((cam_dmao & 0x4000) ? (MTRUE) : (MFALSE));
				pstRTBuf[module]->ring_buf[_rsso_].active =
				    ((cam_dmao & 0x8000) ? (MTRUE) : (MFALSE));

				/*reset 1st sof flag when vf is enabled*/
				g1stSof[module] = MTRUE;
				break;
			}
			case 0: {
				LOG_INF("CAM_%d viewFinder is OFF\n", module);

				if (vf & 0x1)
					ISP_WR32(
					    CAM_REG_TG_VF_CON(DebugFlag[1]),
					    (vf-0x1));
				else
					LOG_NOTICE(
					    "CAM_%d: vf already disabled\n",
					    module);
				break;
			}
			/* CAMSV */
			case 11: {
				LOG_INF("CAMSV_%d viewFinder is ON\n", module);
				cam_dmao = (ISP_RD32(
					CAMSV_REG_MODULE_EN(DebugFlag[1]))
					& 0x10);
				LOG_INF("CAMSV_%d:[DMA_EN]:0x%x\n",
					module, cam_dmao);
				vf = ISP_RD32(
					CAMSV_REG_TG_VF_CON(DebugFlag[1]));

				if (vf & 0x1)
					LOG_NOTICE(
					    "CAMSV_%d: vf already enabled\n",
					    DebugFlag[1]);
				else
					ISP_WR32(
					    CAMSV_REG_TG_VF_CON(DebugFlag[1]),
					    (vf+0x1));

				pstRTBuf[module]->ring_buf[
					_camsv_imgo_].active =
					((cam_dmao & 0x10)
					? (MTRUE) : (MFALSE));
				/*reset 1st sof flag when vf is enabled*/
				g1stSof[module] = MTRUE;
				break;
			}
			case 10: {
				LOG_INF("CAMSV_%d viewFinder is OFF\n",
					DebugFlag[1]);
				vf = ISP_RD32(
					CAMSV_REG_TG_VF_CON(DebugFlag[1]));

				if (vf & 0x1)
					ISP_WR32(
					    CAMSV_REG_TG_VF_CON(DebugFlag[1]),
					    (vf-0x1));
				else
					LOG_NOTICE(
					    "CAMSV_%d: vf already disalbed\n",
					    DebugFlag[1]);
				break;
			}
			}
		} else {
			LOG_NOTICE("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	case ISP_GET_START_TIME:
		if (copy_from_user(DebugFlag, (void *)Param,
		    sizeof(unsigned int) * 3) == 0) {
			struct S_START_T Tstp = {0,};

			#if (TIMESTAMP_QUEUE_EN == 1)
			unsigned int dma_id = DebugFlag[1];

			if (_cam_max_ == DebugFlag[1]) {
				/* only for wait timestamp to ready */
				Ret = ISP_WaitTimestampReady(
					DebugFlag[0],
					DebugFlag[2]);
				break;
			}

			switch (DebugFlag[0]) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				if (ISP_PopBufTimestamp(DebugFlag[0],
				    dma_id, Tstp) != 0)
					LOG_NOTICE(
					    "Get Buf sof timestamp fail");

				break;
			case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
				Tstp.sec = gSTime[DebugFlag[0]].sec;
				Tstp.usec = gSTime[DebugFlag[0]].usec;
				break;
			default:
				LOG_NOTICE("unsupported module:0x%x\n",
					DebugFlag[0]);
				Ret = -EFAULT;
				break;
			}
			if (Ret != 0)
				break;
			#else
			if (DebugFlag[0] < ISP_IRQ_TYPE_INT_CAM_A_ST ||
			    DebugFlag[0] > ISP_IRQ_TYPE_INT_CAMSV_5_ST) {
				LOG_NOTICE(
					"unsupported module:0x%x",
					DebugFlag[0]);
				Ret = -EFAULT;
				break;
			}

			if (g1stSof[DebugFlag[0]] == MFALSE) {
				Tstp.sec = gSTime[DebugFlag[0]].sec;
				Tstp.usec = gSTime[DebugFlag[0]].usec;
			} else
				Tstp.sec = Tstp.usec = 0;

			#endif

			if (copy_to_user((void *)Param, &Tstp,
			    sizeof(struct S_START_T)) != 0) {
				LOG_NOTICE("copy_to_user failed");
				Ret = -EFAULT;
			}
		}
		break;
#ifdef EP_NO_PMQOS
	case ISP_DFS_CTRL:
	case ISP_DFS_UPDATE:
	case ISP_GET_CUR_ISP_CLOCK:
	case ISP_SET_PM_QOS_INFO:
	case ISP_SET_PM_QOS:
		break;
	case ISP_GET_SUPPORTED_ISP_CLOCKS:

		/* Set a default clk for EP */
		ispclks.clklevelcnt = 1;
		ispclks.clklevel[lv] = 546;
		LOG_NOTICE("Default DFS Clk level:%d for EP",
			ispclks.clklevel[lv]);

		if (copy_to_user((void *)Param, &ispclks,
		    sizeof(struct ISP_CLK_INFO)) != 0) {
			LOG_NOTICE("copy_to_user failed");
			Ret = -EFAULT;
		}
		break;
#else
	case ISP_DFS_CTRL:
		{
			static unsigned int camsys_qos;
			unsigned int dfs_ctrl;

			if (copy_from_user(&dfs_ctrl, (void *)Param,
			    sizeof(unsigned int)) == 0) {
				if (dfs_ctrl == MTRUE) {
					if (++camsys_qos == 1) {
						#ifdef CONFIG_MTK_QOS_SUPPORT
						pm_qos_add_request(
						  &isp_qos,
						  PM_QOS_CAM_FREQ,
						  0);
						#else
						mmdvfs_pm_qos_add_request(
						  &isp_qos,
						  MMDVFS_PM_QOS_SUB_SYS_CAMERA,
						  0);
						#endif
						LOG_VRB("CAMSYS PMQoS turn on");
					}
				} else {
					if (--camsys_qos == 0) {
						#ifdef CONFIG_MTK_QOS_SUPPORT
						pm_qos_remove_request(
						    &isp_qos);
						#else
						mmdvfs_pm_qos_remove_request(
						    &isp_qos);
						#endif
						LOG_VRB(
						  "CAMSYS PMQoS turn off");
					}
				}
			} else {
				LOG_NOTICE(
				    "ISP_DFS_CTRL copy_from_user failed\n");
				Ret = -EFAULT;
			}
		}
		break;
	case ISP_DFS_UPDATE:
		{
			unsigned int dfs_update;

			if (copy_from_user(&dfs_update, (void *)Param,
			    sizeof(unsigned int)) == 0) {
				#ifdef CONFIG_MTK_QOS_SUPPORT
				pm_qos_update_request(
					&isp_qos,
					dfs_update);
				#else
				mmdvfs_pm_qos_update_request(
					&isp_qos,
					MMDVFS_PM_QOS_SUB_SYS_CAMERA,
					dfs_update);
				#endif
				target_clk = dfs_update;
				LOG_DBG("Set clock level:%d", dfs_update);
			} else {
				LOG_NOTICE(
				    "ISP_DFS_UPDATE copy_from_user failed\n");
				Ret = -EFAULT;
			}
		}
		break;
	case ISP_GET_SUPPORTED_ISP_CLOCKS:
		{
		#ifdef CONFIG_MTK_QOS_SUPPORT
			int result = 0;
			u64 freq_steps[ISP_CLK_LEVEL_CNT];

			/* Call mmdvfs_qos_get_freq_steps to
			 * get supported frequency
			 */
			result = mmdvfs_qos_get_freq_steps(
					PM_QOS_CAM_FREQ,
					freq_steps,
					(u32 *)&ispclks.clklevelcnt);

			if (result < 0) {
				LOG_NOTICE(
				    "get MMDVFS freq steps failed, result: %d\n",
				    result);
				Ret = -EFAULT;
				break;
			}

			if (ispclks.clklevelcnt > ISP_CLK_LEVEL_CNT) {
				LOG_NOTICE("clklevelcnt is exceeded");
				Ret = -EFAULT;
				break;
			}

			for (; lv < ispclks.clklevelcnt; lv++) {
				/* Save clk from low to high */
				ispclks.clklevel[lv] = freq_steps[lv];
				LOG_VRB(
				    "DFS Clk level[%d]:%d",
				    lv, ispclks.clklevel[lv]);
			}

			target_clk = ispclks.clklevel[ispclks.clklevelcnt - 1];
		#else
			/* To get how many clk levels this platform
			 * is supported
			 */
			ispclks.clklevelcnt = mmdvfs_qos_get_thres_count(
						&isp_qos,
						MMDVFS_PM_QOS_SUB_SYS_CAMERA);

			if (ispclks.clklevelcnt > ISP_CLK_LEVEL_CNT) {
				LOG_NOTICE("clklevelcnt is exceeded");
				Ret = -EFAULT;
				break;
			}

			for (; lv < ispclks.clklevelcnt; lv++) {
				/* To get all clk level on this platform */
				ispclks.clklevel[lv] =
					mmdvfs_qos_get_thres_value(
						&isp_qos,
						MMDVFS_PM_QOS_SUB_SYS_CAMERA,
						lv);
				LOG_VRB("DFS Clk level[%d]:%d",
					lv, ispclks.clklevel[lv]);
			}

			target_clk = ispclks.clklevel[ispclks.clklevelcnt - 1];
		#endif
			if (copy_to_user((void *)Param, &ispclks,
				sizeof(struct ISP_CLK_INFO)) != 0) {
				LOG_NOTICE("copy_to_user failed");
				Ret = -EFAULT;
			}
		}
		break;
	case ISP_GET_CUR_ISP_CLOCK:
		{
			struct ISP_GET_CLK_INFO getclk;

			#ifdef CONFIG_MTK_QOS_SUPPORT
			getclk.curClk = (u32)mmdvfs_qos_get_freq(
						PM_QOS_CAM_FREQ);
			#else
			getclk.curClk = mmdvfs_qos_get_cur_thres(
						&isp_qos,
						MMDVFS_PM_QOS_SUB_SYS_CAMERA);
			#endif
			getclk.targetClk = target_clk;
			LOG_VRB("Get current clock level:%d, target clock:%d",
				getclk.curClk, getclk.targetClk);

			if (copy_to_user((void *)Param, &getclk,
			    sizeof(struct ISP_GET_CLK_INFO)) != 0) {
				LOG_NOTICE("copy_to_user failed");
				Ret = -EFAULT;
			}
		}
		break;
	case ISP_GET_GLOBAL_TIME:
		{
#ifdef TS_BOOT_T
			#define TS_TYPE	(2)
#else
			#define TS_TYPE	(1)
#endif
			u64 hwTickCnt[TS_TYPE], globalTime[TS_TYPE];

			if (copy_from_user(hwTickCnt, (void *)Param,
			    sizeof(u64)*TS_TYPE) == 0) {
				globalTime[0] =
					archcounter_timesync_to_monotonic(
					hwTickCnt[0]); /* ns */
				do_div(globalTime[0], 1000); /* ns to us */
#ifdef TS_BOOT_T
				globalTime[1] =
					archcounter_timesync_to_boot(
					hwTickCnt[0]); /* ns */
				do_div(globalTime[1], 1000); /* ns to us */
#endif
				if (copy_to_user((void *)Param,
					globalTime, sizeof(u64)*TS_TYPE) != 0) {
					LOG_NOTICE(
					    "ISP_GET_GLOBAL_TIME copy_to_user failed");
					Ret = -EFAULT;
				}
			} else {
				LOG_NOTICE(
				    "ISP_GET_GLOBAL_TIME copy_from_user failed\n");
				Ret = -EFAULT;
			}
		}
		break;
	case ISP_SET_PM_QOS_INFO:
		{
			struct ISP_PM_QOS_INFO_STRUCT pm_qos_info;

			if (copy_from_user(&pm_qos_info, (void *)Param,
			    sizeof(struct ISP_PM_QOS_INFO_STRUCT)) == 0) {

				if (pm_qos_info.module <
				    ISP_IRQ_TYPE_INT_CAM_A_ST ||
				    pm_qos_info.module >
				    ISP_IRQ_TYPE_INT_CAM_C_ST) {
					LOG_NOTICE("HW_module error:%d",
					pm_qos_info.module);
					Ret = -EFAULT;
					break;
				}
				#ifdef CONFIG_MTK_QOS_SUPPORT
				G_PM_QOS[pm_qos_info.module].bw_sum =
					pm_qos_info.bw_value;
				G_PM_QOS[pm_qos_info.module].fps =
					pm_qos_info.fps/10;
				#else
				LOG_NOTICE(
				"ISP_SET_PM_QOS_INFO is not supported\n");
				#endif
			} else {
				LOG_NOTICE(
				"ISP_SET_PM_QOS_INFO copy_from_user failed\n");
				Ret = -EFAULT;
			}
		}
		break;
	case ISP_SET_PM_QOS:
		{
			if (copy_from_user(DebugFlag, (void *)Param,
			    sizeof(unsigned int) * 2) == 0) {
				#ifdef CONFIG_MTK_QOS_SUPPORT
				static int bw_request[
						ISP_IRQ_TYPE_INT_CAM_C_ST+1];

				if (DebugFlag[1] < ISP_IRQ_TYPE_INT_CAM_A_ST ||
				    DebugFlag[1] > ISP_IRQ_TYPE_INT_CAM_C_ST) {
					LOG_NOTICE(
					  "HW_module error:%d", DebugFlag[1]);
					Ret = -EFAULT;
					break;
				}
				if (DebugFlag[0] == 1) {
					if (++bw_request[DebugFlag[1]] == 1) {
						pm_qos_add_request(
						    &camsys_qos_request[
						    DebugFlag[1]],
						    PM_QOS_MM_MEMORY_BANDWIDTH,
						    PM_QOS_DEFAULT_VALUE);
					}
					Ret = ISP_SetPMQOS(
						DebugFlag[0], DebugFlag[1]);
				} else {
					if (bw_request[DebugFlag[1]] == 0)
						break;
					Ret = ISP_SetPMQOS(
						DebugFlag[0],
						DebugFlag[1]);
					pm_qos_remove_request(
						&camsys_qos_request[
						DebugFlag[1]]);
					bw_request[DebugFlag[1]] = 0;
				}
				#else
				LOG_NOTICE("ISP_SET_PM_QOS is not supported\n");
				break;
				#endif
			} else {
				LOG_NOTICE(
				    "ISP_SET_PM_QOS copy_from_user failed\n");
				Ret = -EFAULT;
			}
		}
		break;
#endif
	case ISP_GET_VSYNC_CNT:
		if (copy_from_user(&DebugFlag[0], (void *)Param,
			sizeof(unsigned int)) != 0) {
			LOG_NOTICE("get cur sof from user fail");
			Ret = -EFAULT;
		} else {

			if (DebugFlag[0] < ISP_IRQ_TYPE_INT_CAM_A_ST ||
			    DebugFlag[0] >= ISP_IRQ_TYPE_AMOUNT) {
				LOG_NOTICE("err TG(0x%x)\n", DebugFlag[0]);
				Ret = -EFAULT;
				break;
			}

			DebugFlag[1] = Vsync_cnt[DebugFlag[0]];
		}
		if (copy_to_user((void *)Param, &DebugFlag[1],
			sizeof(unsigned int)) != 0) {
			LOG_NOTICE("copy to user fail");
			Ret = -EFAULT;
		}
		break;
	case ISP_RESET_VSYNC_CNT: {
			enum ISP_IRQ_TYPE_ENUM i = ISP_IRQ_TYPE_INT_CAM_A_ST;

			for (i = ISP_IRQ_TYPE_INT_CAM_A_ST;
				 i < ISP_IRQ_TYPE_AMOUNT; i++)
				Vsync_cnt[i] = 0;
		}
		break;
	#ifdef ENABLE_KEEP_ION_HANDLE
	case ISP_ION_IMPORT:
		if (copy_from_user(&IonNode, (void *)Param,
			sizeof(struct ISP_DEV_ION_NODE_STRUCT)) == 0) {
			struct T_ION_TBL *ptbl = NULL;
			unsigned int jump;

			if (!pIon_client) {
				LOG_NOTICE("ion_import: invalid ion client!\n");
				Ret = -EFAULT;
				break;
			}

			if (IonNode.devNode >= ISP_DEV_NODE_NUM) {
				LOG_NOTICE(
				    "[ISP_ION_IMPORT]devNode should be smaller than ISP_DEV_NODE_NUM");
				Ret = -EFAULT;
				break;
			}

			ptbl = &gION_TBL[IonNode.devNode];
			if (ptbl->node != IonNode.devNode) {
				LOG_NOTICE(
				    "ion_import: devNode not support(%d)!\n",
				    IonNode.devNode);
				Ret = -EFAULT;
				break;
			}
			if (IonNode.dmaPort < 0 ||
				IonNode.dmaPort >= _dma_max_wr_) {
				LOG_NOTICE(
				    "ion_import: dmaport error:%d(0~%d)\n",
				    IonNode.dmaPort, _dma_max_wr_);
				Ret = -EFAULT;
				break;
			}
			jump = IonNode.dmaPort*_ion_keep_max_;
			if (IonNode.memID <= 0) {
				LOG_NOTICE(
				    "ion_import: dma(%d)invalid ion fd(%d)\n",
				    IonNode.dmaPort, IonNode.memID);
				Ret = -EFAULT;
				break;
			}
			spin_lock(&(ptbl->pLock[IonNode.dmaPort]));
			/* */
			/* check if memID is exist */
			for (i = 0; i < _ion_keep_max_; i++) {
				if (ptbl->pIonFd[jump + i] == IonNode.memID)
					break;
			}
			spin_unlock(&(ptbl->pLock[IonNode.dmaPort]));
			/* */
			if (i < _ion_keep_max_) {
				if (IspInfo.DebugMask & ISP_DBG_ION_CTRL) {
					LOG_INF(
					    "ion_import: already exist: dev(%d)dma(%d)i(%d)fd(%d)Hnd(0x%p)\n",
					    IonNode.devNode,
					    IonNode.dmaPort,
					    i, IonNode.memID,
					    ptbl->pIonHnd[jump + i]);
				}
				/* User might allocate a big memory and divid
				 * it into many buffers, the ion FD of these
				 * buffers is the same, so we must check there
				 * has no users take this memory
				 */
				ptbl->pIonCt[jump + i]++;

				break;
			}
			/* */
			handle = ISP_ion_import_handle(
					pIon_client, IonNode.memID);
			if (!handle) {
				Ret = -EFAULT;
				break;
			}
			/* */
			spin_lock(&(ptbl->pLock[IonNode.dmaPort]));
			for (i = 0; i < _ion_keep_max_; i++) {
				if (ptbl->pIonFd[jump + i] == 0) {
					ptbl->pIonFd[jump + i] = IonNode.memID;
					ptbl->pIonHnd[jump + i] = handle;

					/* User might allocate a big memory and
					 * divid it into many buffers, the ion
					 * FD of these buffers is the same, so
					 * we must check there has no users take
					 * this memory
					 */
					ptbl->pIonCt[jump + i]++;

					if (IspInfo.DebugMask
						& ISP_DBG_ION_CTRL) {
						LOG_INF(
						    "ion_import: dev(%d)dma(%d)i(%d)fd(%d)Hnd(0x%p)\n",
						    IonNode.devNode,
						    IonNode.dmaPort,
						    i,
						    IonNode.memID,
						    handle);
					}
					break;
				}
			}
			spin_unlock(&(ptbl->pLock[IonNode.dmaPort]));
			/* */
			if (i == _ion_keep_max_) {
				LOG_NOTICE(
				    "ion_import: dma(%d)no empty space in list(%d_%d)\n",
				    IonNode.dmaPort,
				    IonNode.memID,
				    _ion_keep_max_);
				/*can't in spin_lock*/
				ISP_ion_free_handle(pIon_client, handle);
				Ret = -EFAULT;
			}
		} else {
			LOG_NOTICE("[ion import]copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	case ISP_ION_FREE:
		if (copy_from_user(&IonNode, (void *)Param,
			sizeof(struct ISP_DEV_ION_NODE_STRUCT)) == 0) {
			struct T_ION_TBL *ptbl = NULL;
			unsigned int jump;

			if (!pIon_client) {
				LOG_NOTICE("ion_free: invalid ion client!\n");
				Ret = -EFAULT;
				break;
			}

			if (IonNode.devNode >= ISP_DEV_NODE_NUM) {
				LOG_NOTICE(
				    "[ISP_ION_FREE]devNode should be smaller than ISP_DEV_NODE_NUM");
				Ret = -EFAULT;
				break;
			}

			ptbl = &gION_TBL[IonNode.devNode];
			if (ptbl->node != IonNode.devNode) {
				LOG_NOTICE(
				    "ion_free: devNode not support(%d)!\n",
				    IonNode.devNode);
				Ret = -EFAULT;
				break;
			}
			if (IonNode.dmaPort < 0 ||
			    IonNode.dmaPort >= _dma_max_wr_) {
				LOG_NOTICE(
				    "ion_free: dmaport error:%d(0~%d)\n",
				    IonNode.dmaPort,
				    _dma_max_wr_);
				Ret = -EFAULT;
				break;
			}
			jump = IonNode.dmaPort*_ion_keep_max_;
			if (IonNode.memID <= 0) {
				LOG_NOTICE(
				    "ion_free: invalid ion fd(%d)\n",
				    IonNode.memID);
				Ret = -EFAULT;
				break;
			}

			/* check if memID is exist */
			spin_lock(&(ptbl->pLock[IonNode.dmaPort]));
			for (i = 0; i < _ion_keep_max_; i++) {
				if (ptbl->pIonFd[jump + i] == IonNode.memID)
					break;
			}
			if (i == _ion_keep_max_) {
				spin_unlock(&(ptbl->pLock[IonNode.dmaPort]));
				LOG_NOTICE(
				    "ion_free: can't find ion dev(%d)dma(%d)fd(%d) in list\n",
				    IonNode.devNode,
				    IonNode.dmaPort,
				    IonNode.memID);
				Ret = -EFAULT;

				break;
			}
			/* User might allocate a big memory and divid it into
			 * many buffers, the ion FD of these buffers is the same
			 * so we must check there has no users take this memory
			 */
			if (--ptbl->pIonCt[jump + i] > 0) {
				spin_unlock(&(ptbl->pLock[IonNode.dmaPort]));
				if (IspInfo.DebugMask & ISP_DBG_ION_CTRL) {
					LOG_INF(
					    "ion_free: user ct(%d): dev(%d)dma(%d)i(%d)fd(%d)\n",
					    ptbl->pIonCt[jump + i],
					    IonNode.devNode,
					    IonNode.dmaPort,
					    i,
					    IonNode.memID);
				}
				break;
			} else if (ptbl->pIonCt[jump + i] < 0) {
				spin_unlock(&(ptbl->pLock[IonNode.dmaPort]));
				LOG_NOTICE(
				    "ion_free: free more than import (%d): dev(%d)dma(%d)i(%d)fd(%d)\n",
				    ptbl->pIonCt[jump + i],
				    IonNode.devNode,
				    IonNode.dmaPort,
				    i,
				    IonNode.memID);
				Ret = -EFAULT;
				break;
			}

			if (IspInfo.DebugMask & ISP_DBG_ION_CTRL) {
				LOG_INF(
				    "ion_free: dev(%d)dma(%d)i(%d)fd(%d)Hnd(0x%p)Ct(%d)\n",
					IonNode.devNode, IonNode.dmaPort, i,
					IonNode.memID,
					ptbl->pIonHnd[jump + i],
					ptbl->pIonCt[jump + i]);
			}

			p_IonHnd = ptbl->pIonHnd[jump + i];
			ptbl->pIonFd[jump + i] = 0;
			ptbl->pIonHnd[jump + i] = NULL;
			spin_unlock(&(ptbl->pLock[IonNode.dmaPort]));
			/* */
			ISP_ion_free_handle(pIon_client, p_IonHnd);
		} else {
			LOG_NOTICE("[ion free]copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	case ISP_ION_FREE_BY_HWMODULE:
		if (copy_from_user(&module, (void *)Param,
			sizeof(unsigned int)) == 0) {
			if (module >= ISP_DEV_NODE_NUM) {
				LOG_NOTICE(
				   "[ISP_ION_FREE_BY_HWMODULE]module should be smaller than ISP_DEV_NODE_NUM");
				Ret = -EFAULT;
				break;
			}
			if (gION_TBL[module].node != module) {
				LOG_NOTICE("module error(%d)\n", module);
				Ret = -EFAULT;
				break;
			}

			ISP_ion_free_handle_by_module(module);
		} else {
			LOG_NOTICE(
			    "[ion free by module]copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	#endif
	case ISP_CQ_SW_PATCH:
		{
			static struct ISP_MULTI_RAW_CONFIG
						multiRAWConfig = {0,};
			static uintptr_t
				Addr[ISP_IRQ_TYPE_INT_CAMSV_0_ST] = {0,};

			if (copy_from_user(&multiRAWConfig, (void *)Param,
			    sizeof(struct ISP_MULTI_RAW_CONFIG)) == 0) {
				if (multiRAWConfig.HWmodule < 0 ||
				    multiRAWConfig.HWmodule >
				    (ISP_IRQ_TYPE_INT_CAMSV_0_ST - 1)) {
					LOG_NOTICE("Wrong HWmodule:%d",
						multiRAWConfig.HWmodule);
					Ret = -EFAULT;
					break;
				}

				Addr[multiRAWConfig.HWmodule] =
					multiRAWConfig.cq_base_pAddr;
			}

			switch (multiRAWConfig.slave_cam_num) {
			case 1: /* 2 RAWS to 1 tg for static twin mode */
				if (multiRAWConfig.master_module ==
				    ISP_IRQ_TYPE_INT_CAM_A_ST &&
				    multiRAWConfig.twin_module ==
				    ISP_IRQ_TYPE_INT_CAM_C_ST) {
					if ((Addr[ISP_IRQ_TYPE_INT_CAM_A_ST]
						!= 0) &&
					    (Addr[ISP_IRQ_TYPE_INT_CAM_C_ST]
						!= 0)) {
						unsigned long flags;

						spin_lock_irqsave(
							&IspInfo.SpinLockIrq[0],
							flags);
						ISP_WR32(
						    CAM_REG_CTL_CD_DONE_SEL(
							ISP_CAM_A_IDX),
						    0x10000);
						ISP_WR32(
						    CAM_REG_CTL_UNI_DONE_SEL(
							ISP_CAM_A_IDX),
						    0x1);
						ISP_WR32(
						    CAM_REG_CQ_THR0_BASEADDR(
							ISP_CAM_C_IDX),
						    Addr[2]);
						ISP_WR32(
						    CAM_REG_CQ_THR0_BASEADDR(
							ISP_CAM_A_IDX),
						    Addr[0]);
						ISP_WR32(
						    CAM_REG_CTL_UNI_DONE_SEL(
							ISP_CAM_A_IDX),
						    0x0);
						Addr[ISP_IRQ_TYPE_INT_CAM_A_ST]
							= 0;
						Addr[ISP_IRQ_TYPE_INT_CAM_C_ST]
							= 0;
						spin_unlock_irqrestore(
							&IspInfo.SpinLockIrq[0],
							flags);
					}
				} else if (multiRAWConfig.master_module ==
				    ISP_IRQ_TYPE_INT_CAM_B_ST &&
				    multiRAWConfig.twin_module ==
				    ISP_IRQ_TYPE_INT_CAM_C_ST) {
					if ((Addr[ISP_IRQ_TYPE_INT_CAM_B_ST]
						!= 0) &&
					    (Addr[ISP_IRQ_TYPE_INT_CAM_C_ST]
						!= 0)) {
						unsigned long flags;

						spin_lock_irqsave(
						&IspInfo.SpinLockIrq[1], flags);
						ISP_WR32(
						    CAM_REG_CTL_CD_DONE_SEL(
							ISP_CAM_B_IDX),
						    0x10000);
						ISP_WR32(
						    CAM_REG_CTL_UNI_DONE_SEL(
							ISP_CAM_B_IDX),
						    0x1);
						ISP_WR32(
						    CAM_REG_CQ_THR0_BASEADDR(
							ISP_CAM_C_IDX),
						    Addr[2]);
						ISP_WR32(
						    CAM_REG_CQ_THR0_BASEADDR(
							ISP_CAM_B_IDX),
						    Addr[1]);
						ISP_WR32(
						    CAM_REG_CTL_UNI_DONE_SEL(
							ISP_CAM_B_IDX),
						    0x0);
						Addr[ISP_IRQ_TYPE_INT_CAM_B_ST]
							= 0;
						Addr[ISP_IRQ_TYPE_INT_CAM_C_ST]
							= 0;
						spin_unlock_irqrestore(
							&IspInfo.SpinLockIrq[1],
							flags);
					}
				} else {
					if ((Addr[ISP_IRQ_TYPE_INT_CAM_A_ST]
						!= 0) &&
					    (Addr[ISP_IRQ_TYPE_INT_CAM_B_ST]
						!= 0)) {
						unsigned long flags;

						spin_lock_irqsave(
						&IspInfo.SpinLockIrq[0], flags);
						ISP_WR32(
						    CAM_REG_CTL_CD_DONE_SEL(
							ISP_CAM_A_IDX),
						    0x10000);
						ISP_WR32(
						    CAM_REG_CTL_UNI_DONE_SEL(
							ISP_CAM_A_IDX),
						    0x1);
						ISP_WR32(
						    CAM_REG_CQ_THR0_BASEADDR(
							ISP_CAM_B_IDX),
						    Addr[1]);
						ISP_WR32(
						    CAM_REG_CQ_THR0_BASEADDR(
							ISP_CAM_A_IDX),
						    Addr[0]);
						ISP_WR32(
						    CAM_REG_CTL_UNI_DONE_SEL(
							ISP_CAM_A_IDX),
						    0x0);
						Addr[ISP_IRQ_TYPE_INT_CAM_A_ST]
							= 0;
						Addr[ISP_IRQ_TYPE_INT_CAM_B_ST]
							= 0;
						spin_unlock_irqrestore(
						&IspInfo.SpinLockIrq[0], flags);
					}
				}
				break;
			case 2: /* 3 RAWS to 1 tg for static twin mode */
				if ((Addr[ISP_IRQ_TYPE_INT_CAM_A_ST] != 0) &&
				    (Addr[ISP_IRQ_TYPE_INT_CAM_B_ST] != 0) &&
				    (Addr[ISP_IRQ_TYPE_INT_CAM_C_ST] != 0)) {
					unsigned long flags;

					spin_lock_irqsave(
						&IspInfo.SpinLockIrq[
						    ISP_IRQ_TYPE_INT_CAM_A_ST],
						flags);
					ISP_WR32(CAM_REG_CTL_CD_DONE_SEL(
						ISP_CAM_A_IDX), 0x10000);
					ISP_WR32(CAM_REG_CTL_UNI_DONE_SEL(
						ISP_CAM_A_IDX), 0x1);
					ISP_WR32(CAM_REG_CQ_THR0_BASEADDR(
						ISP_CAM_C_IDX), Addr[2]);
					ISP_WR32(CAM_REG_CQ_THR0_BASEADDR(
						ISP_CAM_B_IDX), Addr[1]);
					ISP_WR32(CAM_REG_CQ_THR0_BASEADDR(
						ISP_CAM_A_IDX), Addr[0]);
					ISP_WR32(CAM_REG_CTL_UNI_DONE_SEL(
						ISP_CAM_A_IDX), 0x0);
					Addr[ISP_IRQ_TYPE_INT_CAM_A_ST] = 0;
					Addr[ISP_IRQ_TYPE_INT_CAM_B_ST] = 0;
					Addr[ISP_IRQ_TYPE_INT_CAM_C_ST] = 0;
					spin_unlock_irqrestore(
						&IspInfo.SpinLockIrq[
						    ISP_IRQ_TYPE_INT_CAM_A_ST],
						flags);
				}
				break;
			};
		}
		break;
	#if (SMI_LARB_MMU_CTL == 1)
	case ISP_LARB_MMU_CTL: {

		struct ISP_LARB_MMU_STRUCT larbInfo;

		if (copy_from_user(&larbInfo, (void *)Param,
		    sizeof(struct ISP_LARB_MMU_STRUCT)) != 0) {
			LOG_NOTICE("copy_from_user LARB_MMU_CTL failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}

		switch (larbInfo.LarbNum) {
		case 0:
		case 6:
			break;
		default:
			LOG_NOTICE("Wrong SMI_LARB port=%d\n",
				larbInfo.LarbNum);
			Ret = -EFAULT;
			goto EXIT;
		}

		if ((SMI_LARB_BASE[larbInfo.LarbNum] == NULL) ||
			(larbInfo.regOffset >= 0x1000)) {
			LOG_NOTICE(
			    "Wrong SMI_LARB port=%d base addr=%p offset=0x%x\n",
			    larbInfo.LarbNum,
			    SMI_LARB_BASE[larbInfo.LarbNum],
			    larbInfo.regOffset);
			Ret = -EFAULT;
			goto EXIT;
		}

		*(unsigned int *)((char *)SMI_LARB_BASE[larbInfo.LarbNum] +
			larbInfo.regOffset) = larbInfo.regVal;

		}
		break;
	#endif
	case ISP_SET_SEC_DAPC_REG:
		if (copy_from_user(Dapc_Reg, (void *)Param,
			sizeof(unsigned int) * 6) != 0) {
			LOG_NOTICE("get ISP_SET_SEC_DAPC_REG from user fail\n");
			Ret = -EFAULT;
		} else {
			sec_on = Dapc_Reg[1];
			lock_reg.CAM_REG_CTL_EN[Dapc_Reg[0]] = Dapc_Reg[2];
			lock_reg.CAM_REG_CTL_DMA_EN[Dapc_Reg[0]] = Dapc_Reg[3];
			lock_reg.CAM_REG_CTL_SEL[Dapc_Reg[0]] = Dapc_Reg[4];
			lock_reg.CAM_REG_CTL_EN2[Dapc_Reg[0]] = Dapc_Reg[5];
			LOG_INF(
			    "[DAPC]CAM_CTL_EN:0x%x CAM_CTL_DMA_EN:0x%x CAM_CTL_SEL:0x%x CAM_CTL_EN2:0x%x",
			    lock_reg.CAM_REG_CTL_EN[Dapc_Reg[0]],
			    lock_reg.CAM_REG_CTL_DMA_EN[Dapc_Reg[0]],
			    lock_reg.CAM_REG_CTL_SEL[Dapc_Reg[0]],
			    lock_reg.CAM_REG_CTL_EN2[Dapc_Reg[0]]);
		}
		break;
	default:
	{
		LOG_NOTICE("Unknown Cmd(%d)\n", Cmd);
		Ret = -EPERM;
		break;
	}
	}
	/*  */
EXIT:
	if (Ret != 0)
		LOG_NOTICE(
		    "Fail, Cmd(%d), Pid(%d), (process, pid, tgid)=(%s, %d, %d)\n",
		    Cmd, pUserInfo->Pid, current->comm,
		    current->pid, current->tgid);
	/*  */
	return Ret;
}

#ifdef CONFIG_COMPAT

/******************************************************************************
 *
 ******************************************************************************/
static int compat_get_isp_read_register_data(
	struct compat_ISP_REG_IO_STRUCT __user *data32,
	struct ISP_REG_IO_STRUCT __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->pData);
	err |= put_user(compat_ptr(uptr), &data->pData);
	err |= get_user(count, &data32->Count);
	err |= put_user(count, &data->Count);
	return err;
}

static int compat_put_isp_read_register_data(
	struct compat_ISP_REG_IO_STRUCT __user *data32,
	struct ISP_REG_IO_STRUCT __user *data)
{
	compat_uint_t count;
	/*      compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr),     &data->pData); */
	/* err |= put_user(uptr, &data32->pData); */
	err |= get_user(count, &data->Count);
	err |= put_user(count, &data32->Count);
	return err;
}

static int compat_get_isp_buf_ctrl_struct_data(
	struct compat_ISP_BUFFER_CTRL_STRUCT __user *data32,
	struct ISP_BUFFER_CTRL_STRUCT __user *data)
{
	compat_uint_t tmp, tmp2, tmp3;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(tmp, &data32->ctrl);
	err |= put_user(tmp, &data->ctrl);
	err |= get_user(tmp2, &data32->module);
	err |= put_user(tmp2, &data->module);
	err |= get_user(tmp3, &data32->buf_id);
	err |= put_user(tmp3, &data->buf_id);
	err |= get_user(uptr, &data32->data_ptr);
	err |= put_user(compat_ptr(uptr), &data->data_ptr);
	err |= get_user(uptr, &data32->ex_data_ptr);
	err |= put_user(compat_ptr(uptr), &data->ex_data_ptr);
	err |= get_user(uptr, &data32->pExtend);
	err |= put_user(compat_ptr(uptr), &data->pExtend);

	return err;
}

static int compat_put_isp_buf_ctrl_struct_data(
	struct compat_ISP_BUFFER_CTRL_STRUCT __user *data32,
	struct ISP_BUFFER_CTRL_STRUCT __user *data)
{
	compat_uint_t tmp, tmp2, tmp3;
	/*      compat_uptr_t uptr;*/
	int err = 0;

	err = get_user(tmp, &data->ctrl);
	err |= put_user(tmp, &data32->ctrl);
	err |= get_user(tmp2, &data->module);
	err |= put_user(tmp2, &data32->module);
	err |= get_user(tmp3, &data->buf_id);
	err |= put_user(tmp3, &data32->buf_id);
	/* Assume data pointer is unchanged. */
	/* err |= get_user(compat_ptr(uptr), &data->data_ptr); */
	/* err |= put_user(uptr, &data32->data_ptr); */
	/* err |= get_user(compat_ptr(uptr), &data->ex_data_ptr); */
	/* err |= put_user(uptr, &data32->ex_data_ptr); */
	/* err |= get_user(compat_ptr(uptr), &data->pExtend); */
	/* err |= put_user(uptr, &data32->pExtend); */

	return err;
}

static int compat_get_isp_ref_cnt_ctrl_struct_data(
	struct compat_ISP_REF_CNT_CTRL_STRUCT __user *data32,
	struct ISP_REF_CNT_CTRL_STRUCT __user *data)
{
	compat_uint_t tmp, tmp2;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(tmp, &data32->ctrl);
	err |= put_user(tmp, &data->ctrl);
	err |= get_user(tmp2, &data32->id);
	err |= put_user(tmp2, &data->id);
	err |= get_user(uptr, &data32->data_ptr);
	err |= put_user(compat_ptr(uptr), &data->data_ptr);

	return err;
}

static int compat_put_isp_ref_cnt_ctrl_struct_data(
	struct compat_ISP_REF_CNT_CTRL_STRUCT __user *data32,
	struct ISP_REF_CNT_CTRL_STRUCT __user *data)
{
	compat_uint_t tmp, tmp2;
	/*      compat_uptr_t uptr;*/
	int err = 0;

	err = get_user(tmp, &data->ctrl);
	err |= put_user(tmp, &data32->ctrl);
	err |= get_user(tmp2, &data->id);
	err |= put_user(tmp2, &data32->id);
	/* Assume data pointer is unchanged. */
	/* err |= get_user(compat_ptr(uptr), &data->data_ptr); */
	/* err |= put_user(uptr, &data32->data_ptr); */

	return err;
}

static long ISP_ioctl_compat(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_ISP_READ_REGISTER: {
		struct compat_ISP_REG_IO_STRUCT __user *data32;
		struct ISP_REG_IO_STRUCT __user *data;

		int err = 0;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_read_register_data(data32, data);
		if (err) {
			LOG_INF("compat_get_isp_read_register_data error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp,
			ISP_READ_REGISTER, (unsigned long)data);
		err = compat_put_isp_read_register_data(data32, data);
		if (err) {
			LOG_INF("compat_put_isp_read_register_data error!!!\n");
			return err;
		}
		return ret;
	}
	case COMPAT_ISP_WRITE_REGISTER: {
		struct compat_ISP_REG_IO_STRUCT __user *data32;
		struct ISP_REG_IO_STRUCT __user *data;

		int err = 0;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_read_register_data(data32, data);
		if (err) {
			LOG_INF("COMPAT_ISP_WRITE_REGISTER error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp,
			ISP_WRITE_REGISTER, (unsigned long)data);
		return ret;
	}
	case COMPAT_ISP_BUFFER_CTRL: {
		struct compat_ISP_BUFFER_CTRL_STRUCT __user *data32;
		struct ISP_BUFFER_CTRL_STRUCT __user *data;

		int err = 0;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_buf_ctrl_struct_data(data32, data);
		if (err)
			return err;

		if (err) {
			LOG_INF(
				"compat_get_isp_buf_ctrl_struct_data error!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp,
					ISP_BUFFER_CTRL, (unsigned long)data);
		err = compat_put_isp_buf_ctrl_struct_data(data32, data);

		if (err) {
			LOG_INF("compat_put_isp_buf_ctrl_struct_data error!\n");
			return err;
		}
		return ret;

	}
	case COMPAT_ISP_REF_CNT_CTRL: {
		struct compat_ISP_REF_CNT_CTRL_STRUCT __user *data32;
		struct ISP_REF_CNT_CTRL_STRUCT __user *data;

		int err = 0;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_ref_cnt_ctrl_struct_data(data32, data);
		if (err) {
			LOG_INF(
			  "compat_get_isp_ref_cnt_ctrl_struct_data error!\n");
			return err;
		}

		err = compat_put_isp_ref_cnt_ctrl_struct_data(data32, data);
		if (err) {
			LOG_INF(
			  "compat_put_isp_ref_cnt_ctrl_struct_data error!!!\n");
			return err;
		}
		return ret;
	}
	case COMPAT_ISP_DEBUG_FLAG: {
		/* compat_ptr(arg) will convert the arg */
		ret = filp->f_op->unlocked_ioctl(
				filp,
				ISP_DEBUG_FLAG,
				(unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_GET_DMA_ERR: {
		/* compat_ptr(arg) will convert the arg */
		ret = filp->f_op->unlocked_ioctl(
				filp, ISP_GET_DMA_ERR,
				(unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_WAKELOCK_CTRL: {
		ret = filp->f_op->unlocked_ioctl(
				filp, ISP_WAKELOCK_CTRL,
				(unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_GET_DROP_FRAME: {
		ret =
			filp->f_op->unlocked_ioctl(
				filp, ISP_GET_DROP_FRAME,
				(unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_GET_CUR_SOF: {
		ret =
			filp->f_op->unlocked_ioctl(
				filp, ISP_GET_CUR_SOF,
				(unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_RESET_BY_HWMODULE: {
		ret =
			filp->f_op->unlocked_ioctl(
				filp, ISP_RESET_BY_HWMODULE,
				(unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_VF_LOG: {
		ret =
			filp->f_op->unlocked_ioctl(
				filp, ISP_VF_LOG,
				(unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_GET_START_TIME: {
		ret =
			filp->f_op->unlocked_ioctl(
				filp, ISP_GET_START_TIME,
				(unsigned long)compat_ptr(arg));
		return ret;
	}
	case ISP_GET_DUMP_INFO:
	case ISP_WAIT_IRQ:
	case ISP_CLEAR_IRQ: /* structure (no pointer) */
	case ISP_REGISTER_IRQ_USER_KEY:
	case ISP_FLUSH_IRQ_REQUEST:
	case ISP_GET_VSYNC_CNT:
	case ISP_RESET_VSYNC_CNT:
	case ISP_ION_IMPORT:
	case ISP_ION_FREE:
	case ISP_ION_FREE_BY_HWMODULE:
	case ISP_CQ_SW_PATCH:
	case ISP_LARB_MMU_CTL:
	case ISP_DFS_CTRL:
	case ISP_DFS_UPDATE:
	case ISP_GET_SUPPORTED_ISP_CLOCKS:
	case ISP_GET_CUR_ISP_CLOCK:
	case ISP_GET_GLOBAL_TIME:
	case ISP_SET_PM_QOS_INFO:
	case ISP_SET_PM_QOS:
	case ISP_GET_INT_ERR:
	case ISP_SET_SEC_DAPC_REG:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
		/* return ISP_ioctl(filep, cmd, arg); */
	}
}

#endif

/******************************************************************************
 *
 ******************************************************************************/
static int ISP_open(
	struct inode *pInode,
	struct file *pFile)
{
	int Ret = 0;
	unsigned int i, j;
	int q = 0, p = 0;
	struct ISP_USER_INFO_STRUCT *pUserInfo;

	LOG_DBG("- E. UserCount: %d.\n", IspInfo.UserCount);


	/*  */
	spin_lock(&(IspInfo.SpinLockIspRef));

	pFile->private_data = NULL;
	pFile->private_data =
		kmalloc(sizeof(struct ISP_USER_INFO_STRUCT), GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		LOG_DBG(
		    "ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)\n",
		    current->comm, current->pid, current->tgid);
		Ret = -ENOMEM;
	} else {
		pUserInfo = (struct ISP_USER_INFO_STRUCT *)pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}
	/*  */
	if (IspInfo.UserCount > 0) {
		IspInfo.UserCount++;
		spin_unlock(&(IspInfo.SpinLockIspRef));
		LOG_DBG(
		    "Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist\n",
		    IspInfo.UserCount, current->comm,
		    current->pid, current->tgid);
		goto EXIT;
	} else {
		IspInfo.UserCount++;
		spin_unlock(&(IspInfo.SpinLockIspRef));

		/* kernel log limit to (current+150) lines per second */
	#ifndef EP_NO_K_LOG_ADJUST
		pr_detect_count = get_detect_count();
		i = pr_detect_count + 150;
		set_detect_count(i);

		LOG_DBG(
		    "Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), log_limit_line(%d), first user\n",
		    IspInfo.UserCount, current->comm,
		    current->pid, current->tgid, i);
	#else
		LOG_DBG(
		    "Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), first user\n",
		    IspInfo.UserCount, current->comm,
		    current->pid, current->tgid);
	#endif
	}

	/* do wait queue head init when re-enter in camera */
	/*  */
	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		FirstUnusedIrqUserKey = 1;
		strncpy((void *)IrqUserKey_UserInfo[i].userName,
			"DefaultUserNametoAllocMem", USERKEY_STR_LEN);
		IrqUserKey_UserInfo[i].userKey = -1;
	}

	IspInfo.BufInfo.Read.pData = kmalloc(ISP_BUF_SIZE, GFP_ATOMIC);
	IspInfo.BufInfo.Read.Size = ISP_BUF_SIZE;
	IspInfo.BufInfo.Read.Status = ISP_BUF_STATUS_EMPTY;
	if (IspInfo.BufInfo.Read.pData == NULL) {
		LOG_DBG("ERROR: BufRead kmalloc failed\n");
		Ret = -ENOMEM;
		goto EXIT;
	}

	/*  */
	for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++) {
		for (j = 0; j < ISP_IRQ_ST_AMOUNT; j++) {
			for (q = 0; q < IRQ_USER_NUM_MAX; q++) {
				IspInfo.IrqInfo.Status[i][j][q] = 0;
				IspInfo.IrqInfo.MarkedFlag[i][j][q] = 0;
				for (p = 0; p < 32; p++) {
					IspInfo.IrqInfo
						.MarkedTime_sec[i][p][q] = 0;
					IspInfo.IrqInfo
						.MarkedTime_usec[i][p][q] = 0;
					IspInfo.IrqInfo
						.PassedBySigCnt[i][p][q] = 0;
					IspInfo.IrqInfo
						.LastestSigTime_sec[i][p] = 0;
					IspInfo.IrqInfo
						.LastestSigTime_usec[i][p] = 0;
				}
			}
		}
	}
	/* reset backup regs*/
	memset(g_BkReg, 0, sizeof(struct _isp_bk_reg_t) * ISP_IRQ_TYPE_AMOUNT);

#ifdef ENABLE_KEEP_ION_HANDLE
	/* create ion client*/
	mutex_lock(&ion_client_mutex);
	ISP_ion_init();
	mutex_unlock(&ion_client_mutex);
#endif

	archcounter_timesync_init(MTRUE); /* Global timer enable */

#ifdef KERNEL_LOG
	IspInfo.DebugMask = (ISP_DBG_INT);
#endif
	/*  */
EXIT:
	if (Ret < 0) {
		if (IspInfo.BufInfo.Read.pData != NULL) {
			kfree(IspInfo.BufInfo.Read.pData);
			IspInfo.BufInfo.Read.pData = NULL;
		}
	} else {
		/* Enable clock */
		ISP_EnableClock(MTRUE);

		if (IspInfo.UserCount == 1)
			ISP_ConfigDMAControl();

		LOG_DBG("isp open G_u4EnableClockCount: %d\n",
			G_u4EnableClockCount);
	}

	LOG_INF(
	    "- X. Ret: %d. UserCount: %d. G_u4EnableClockCount:%d\n", Ret,
	    IspInfo.UserCount, G_u4EnableClockCount);

	return Ret;

}

/******************************************************************************
 *
 ******************************************************************************/
static inline void ISP_StopHW(int module)
{
	unsigned int regTGSt, loopCnt;
	int ret = 0;
	struct ISP_WAIT_IRQ_STRUCT waitirq;
	unsigned long long  sec = 0, m_sec = 0;
	unsigned long long  timeoutMs = 500000000;/*500ms*/
	char moduleName[128];

	/* wait TG idle*/
	loopCnt = 3;
	switch (module) {
	case ISP_CAM_A_IDX:
	    strncpy(moduleName, "CAMA", 5);
	    waitirq.Type = ISP_IRQ_TYPE_INT_CAM_A_ST;
		break;
	case ISP_CAM_B_IDX:
	    strncpy(moduleName, "CAMB", 5);
	    waitirq.Type = ISP_IRQ_TYPE_INT_CAM_B_ST;
		break;
	default:
	    strncpy(moduleName, "CAMC", 5);
		goto RESET;
	}
	waitirq.EventInfo.Clear = ISP_IRQ_CLEAR_WAIT;
	waitirq.EventInfo.Status = VS_INT_ST;
	waitirq.EventInfo.St_type = SIGNAL_INT;
	waitirq.EventInfo.Timeout = 0x100;
	waitirq.EventInfo.UserKey = 0x0;
	waitirq.bDumpReg = 0;

	do {
		regTGSt = (ISP_RD32(
			CAM_REG_TG_INTER_ST(module)) & 0x00003F00) >> 8;
		if (regTGSt == 1)
			break;

		LOG_INF("%s: wait 1VD (%d)\n", moduleName, loopCnt);
		ret = ISP_WaitIrq(&waitirq);
		/* first wait is clear wait, others are non-clear wait */
		waitirq.EventInfo.Clear = ISP_IRQ_CLEAR_NONE;
	} while (--loopCnt);

	if (-ERESTARTSYS == ret) {
		LOG_INF("%s: interrupt by system signal, wait idle\n",
			moduleName);
		/* timer*/
		m_sec = ktime_get();

		while (regTGSt != 1) {
			regTGSt = (ISP_RD32(
				CAM_REG_TG_INTER_ST(module)) & 0x00003F00) >> 8;
			/*timer*/
			sec = ktime_get();
			/* wait time>timeoutMs, break */
			if ((sec - m_sec) > timeoutMs)
				break;
		}
		if (regTGSt == 1)
			LOG_INF("%s: wait idle done\n", moduleName);
		else
			LOG_INF("%s: wait idle timeout(%lld)\n",
				moduleName, (sec - m_sec));
	}

RESET:
	LOG_INF("%s: reset\n", moduleName);
	/* timer*/
	m_sec = ktime_get();

	/* Reset*/
	ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x0);
	ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x1);
	while (ISP_RD32(CAM_REG_CTL_SW_CTL(module)) != 0x2) {
		/*LOG_DBG("%s resetting...\n", moduleName);*/
		/*timer*/
		sec = ktime_get();
		/* wait time>timeoutMs, break */
		if ((sec  - m_sec) > timeoutMs) {
			LOG_INF("%s: wait SW idle timeout\n", moduleName);
			break;
		}
	}

	ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x4);
	ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x0);
	regTGSt = (ISP_RD32(CAM_REG_TG_INTER_ST(module)) & 0x00003F00) >> 8;
	LOG_DBG("%s_TG_ST(%d)_SW_ST(0x%x)\n", moduleName, regTGSt,
		ISP_RD32(CAM_REG_CTL_SW_CTL(module)));

	ISP_WR32(CAM_UNI_REG_TOP_SW_CTL(ISP_UNI_A_IDX), 0x1001);

	m_sec = ktime_get();
	while ((ISP_RD32(CAM_UNI_REG_TOP_SW_CTL(
		ISP_UNI_A_IDX)) & 0x00000002) != 0x2) {
		sec = ktime_get();
		/* wait time>timeoutMs, break */
		if ((sec  - m_sec) > (timeoutMs/50000)) {
			LOG_INF("%s: wait SW RST ST 50000 timeout\n",
				moduleName);
			break;
		}
	}

	ISP_WR32(CAM_UNI_REG_TOP_SW_CTL(ISP_UNI_A_IDX), 0x4);
	ISP_WR32(CAM_UNI_REG_TOP_SW_CTL(ISP_UNI_A_IDX), 0x0);
	/*disable CMOS*/
	ISP_WR32(CAM_REG_TG_SEN_MODE(module),
		(ISP_RD32(CAM_REG_TG_SEN_MODE(module))&0xfffffffe));

}

/******************************************************************************
 *
 ******************************************************************************/
static inline void ISP_StopSVHW(int module)
{
	unsigned int regTGSt, loopCnt;
	int ret = 0;
	struct ISP_WAIT_IRQ_STRUCT waitirq;
	unsigned long long  sec = 0, m_sec = 0;
	unsigned long long  timeoutMs = 500000000;/*500ms*/
	char moduleName[128];

	/* wait TG idle*/
	loopCnt = 3;
	switch (module) {
	case ISP_CAMSV0_IDX:
		strncpy(moduleName, "CAMSV0", 7);
		waitirq.Type = ISP_IRQ_TYPE_INT_CAMSV_0_ST;
		break;
	case ISP_CAMSV1_IDX:
		strncpy(moduleName, "CAMSV1", 7);
		waitirq.Type = ISP_IRQ_TYPE_INT_CAMSV_1_ST;
		break;
	case ISP_CAMSV2_IDX:
		strncpy(moduleName, "CAMSV2", 7);
		waitirq.Type = ISP_IRQ_TYPE_INT_CAMSV_2_ST;
		break;
	case ISP_CAMSV3_IDX:
		strncpy(moduleName, "CAMSV3", 7);
		waitirq.Type = ISP_IRQ_TYPE_INT_CAMSV_3_ST;
		break;
	case ISP_CAMSV4_IDX:
		strncpy(moduleName, "CAMSV4", 7);
		waitirq.Type = ISP_IRQ_TYPE_INT_CAMSV_4_ST;
		break;
	default:
		strncpy(moduleName, "CAMSV5", 7);
		waitirq.Type = ISP_IRQ_TYPE_INT_CAMSV_5_ST;
		break;
	}
	waitirq.EventInfo.Clear = ISP_IRQ_CLEAR_WAIT;
	waitirq.EventInfo.Status = VS_INT_ST;
	waitirq.EventInfo.St_type = SIGNAL_INT;
	waitirq.EventInfo.Timeout = 0x100;
	waitirq.EventInfo.UserKey = 0x0;
	waitirq.bDumpReg = 0;

	do {
		regTGSt = (ISP_RD32(
			    CAMSV_REG_TG_INTER_ST(module)) & 0x00003F00) >> 8;
		if (regTGSt == 1)
			break;

		LOG_INF("%s: wait 1VD (%d)\n", moduleName, loopCnt);
		ret = ISP_WaitIrq(&waitirq);
		/* first wait is clear wait, others are non-clear wait */
		waitirq.EventInfo.Clear = ISP_IRQ_CLEAR_NONE;
	} while (--loopCnt);

	if (-ERESTARTSYS == ret) {
		LOG_INF("%s: interrupt by system signal, wait idle\n",
			moduleName);
		/* timer*/
		m_sec = ktime_get();

		while (regTGSt != 1) {
			regTGSt = (ISP_RD32(
			    CAMSV_REG_TG_INTER_ST(module)) & 0x00003F00) >> 8;
			/*timer*/
			sec = ktime_get();
			/* wait time>timeoutMs, break */
			if ((sec - m_sec) > timeoutMs)
				break;
		}
		if (regTGSt == 1)
			LOG_INF("%s: wait idle done\n", moduleName);
		else
			LOG_INF("%s: wait idle timeout(%lld)\n",
				moduleName, (sec - m_sec));
	}

	LOG_INF("%s: reset\n", moduleName);
	/* timer*/
	m_sec = ktime_get();

	/* Reset*/
	ISP_WR32(CAMSV_REG_SW_CTL(module), 0x4);
	ISP_WR32(CAMSV_REG_SW_CTL(module), 0x0);
	ISP_WR32(CAMSV_REG_SW_CTL(module), 0x1);
	while (ISP_RD32(CAMSV_REG_SW_CTL(module)) != 0x3) {
		/*LOG_DBG("%s resetting...\n", moduleName);*/
		/*timer*/
		sec = ktime_get();
		/* wait time>timeoutMs, break */
		if ((sec  - m_sec) > timeoutMs) {
			LOG_INF("%s: wait SW idle timeout\n", moduleName);
			break;
		}
	}
	ISP_WR32(CAMSV_REG_SW_CTL(module), 0x0);
	regTGSt = (ISP_RD32(CAMSV_REG_TG_INTER_ST(module)) & 0x00003F00) >> 8;
	LOG_DBG("%s_TG_ST(%d)_SW_ST(0x%x)\n", moduleName, regTGSt,
		ISP_RD32(CAMSV_REG_SW_CTL(module)));

	/*disable CMOS*/
	ISP_WR32(CAMSV_REG_TG_SEN_MODE(module),
		(ISP_RD32(CAMSV_REG_TG_SEN_MODE(module))&0xfffffffe));

}

/******************************************************************************
 *
 ******************************************************************************/
static int ISP_release(
	struct inode *pInode,
	struct file *pFile)
{
	struct ISP_USER_INFO_STRUCT *pUserInfo;
	unsigned int Reg;
	unsigned int i = 0;

	LOG_DBG("- E. UserCount: %d.\n", IspInfo.UserCount);

	/*  */
	/* LOG_DBG("UserCount(%d)",IspInfo.UserCount); */
	/*  */
	if (pFile->private_data != NULL) {
		pUserInfo = (struct ISP_USER_INFO_STRUCT *)pFile->private_data;
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	/*      */
	spin_lock(&(IspInfo.SpinLockIspRef));
	IspInfo.UserCount--;
	if (IspInfo.UserCount > 0) {
		spin_unlock(&(IspInfo.SpinLockIspRef));
		LOG_DBG(
		    "Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
		    IspInfo.UserCount, current->comm,
		    current->pid, current->tgid);
		goto EXIT;
	} else {
		spin_unlock(&(IspInfo.SpinLockIspRef));
	}

	/* kernel log limit back to default */
#ifndef EP_NO_K_LOG_ADJUST
	set_detect_count(pr_detect_count);
#endif
	/*      */
	LOG_DBG(
	    "Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), log_limit_line(%d),	last user",
	    IspInfo.UserCount, current->comm, current->pid,
	    current->tgid, pr_detect_count);

	for (i = ISP_CAM_A_IDX; i < ISP_CAMSV0_IDX; i++) {
		/* Close VF when ISP_release */
		/* reason of close vf is to make sure camera can serve
		 * regular after previous abnormal exit
		 */
		Reg = ISP_RD32(CAM_REG_TG_VF_CON(i));
		Reg &= 0xfffffffE;/* close Vfinder */
		ISP_WR32(CAM_REG_TG_VF_CON(i), Reg);

		/* Set DMX_SEL = 0 when ISP_release */
		/* Reson: If twin is enabled, the twin module's DMX_SEL will
		 *        be set to 1. It will encounter err when run single
		 *        path and other module dmx_sel = 1
		 */
		if (!sec_on) {
			Reg = ISP_RD32(CAM_REG_CTL_SEL(i));
			Reg &= 0xfffffff8;/* set dmx to 0 */
			ISP_WR32(CAM_REG_CTL_SEL(i), Reg);
		}
		/* Reset Twin status.
		 *  If previous camera run in twin mode,
		 *  then mediaserver died, no one clear this status.
		 *  Next camera runs in single mode, and it will not update CQ0
		 */
		ISP_WR32(CAM_REG_CTL_TWIN_STATUS(i), 0x0);
	}

	for (i = ISP_CAMSV0_IDX; i <= ISP_CAMSV5_IDX; i++) {
		Reg = ISP_RD32(CAMSV_REG_TG_VF_CON(i));
		Reg &= 0xfffffffE;/* close Vfinder */
		ISP_WR32(CAMSV_REG_TG_VF_CON(i), Reg);
	}

	/* why i add this wake_unlock here, because the Ap is not expected
	 * to be dead. The driver must releae the wakelock, otherwise the
	 * system will not enter
	 */
	/* the power-saving mode */
	if (g_WaitLockCt) {
		LOG_INF("wakelock disable!! cnt(%d)\n", g_WaitLockCt);
#ifdef CONFIG_PM_WAKELOCKS
		__pm_relax(&isp_wake_lock);
#else
		wake_unlock(&isp_wake_lock);
#endif
		g_WaitLockCt = 0;
	}
	/* reset */
	/*      */
	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		FirstUnusedIrqUserKey = 1;
		strncpy((void *)IrqUserKey_UserInfo[i].userName,
			"DefaultUserNametoAllocMem", USERKEY_STR_LEN);
		IrqUserKey_UserInfo[i].userKey = -1;
	}
	if (IspInfo.BufInfo.Read.pData != NULL) {
		kfree(IspInfo.BufInfo.Read.pData);
		IspInfo.BufInfo.Read.pData = NULL;
		IspInfo.BufInfo.Read.Size = 0;
		IspInfo.BufInfo.Read.Status = ISP_BUF_STATUS_EMPTY;
	}

	/* reset backup regs*/
	memset(g_BkReg, 0, sizeof(struct _isp_bk_reg_t) * ISP_IRQ_TYPE_AMOUNT);

	/* reset secure cam info*/
	if (sec_on) {
		memset(&lock_reg, 0, sizeof(struct isp_sec_dapc_reg));
		sec_on = 0;
	}

	/*  */
#ifdef ENABLE_KEEP_ION_HANDLE
	for (i = ISP_CAMSV0_IDX; i < ISP_CAMSV4_IDX; i++)
		ISP_StopSVHW(i);

	ISP_StopHW(ISP_CAM_A_IDX);
	ISP_StopHW(ISP_CAM_B_IDX);
	ISP_StopHW(ISP_CAM_C_IDX);

	/* free keep ion handles, then destroy ion client*/
	for (i = 0; i < ISP_DEV_NODE_NUM; i++) {
		if (gION_TBL[i].node != ISP_DEV_NODE_NUM)
			ISP_ion_free_handle_by_module(i);
	}

	mutex_lock(&ion_client_mutex);
	ISP_ion_uninit();
	mutex_unlock(&ion_client_mutex);
#endif

	/* Disable clock.
	 *  1. clkmgr: G_u4EnableClockCount=0, call clk_enable/disable
	 *  2. CCF: call clk_enable/disable every time
	 *     -> when IspInfo.UserCount, disable all ISP clk
	 */
	spin_lock(&(IspInfo.SpinLockClock));
	i = G_u4EnableClockCount;
	spin_unlock(&(IspInfo.SpinLockClock));
	while (i > 0) {
		ISP_EnableClock(MFALSE);
		i--;
	}

EXIT:

	LOG_INF("- X. UserCount: %d. G_u4EnableClockCount:%d",
		IspInfo.UserCount,
		G_u4EnableClockCount);
	return 0;
}


/******************************************************************************
 *
 ******************************************************************************/
static int ISP_mmap(struct file *pFile, struct vm_area_struct *pVma)
{
	unsigned long length = 0;
	unsigned int pfn = 0x0;

	/*LOG_DBG("- E.");*/
	length = (pVma->vm_end - pVma->vm_start);
	/*  */
	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
	pfn = pVma->vm_pgoff << PAGE_SHIFT;

	/*LOG_INF("ISP_mmap: vm_pgoff(0x%lx),pfn(0x%x),phy(0x%lx),
	 *	vm_start(0x%lx),vm_end(0x%lx),length(0x%lx)\n",
	 *	pVma->vm_pgoff, pfn, pVma->vm_pgoff << PAGE_SHIFT,
	 *	pVma->vm_start, pVma->vm_end, length);
	 */

	switch (pfn) {
	case CAM_A_BASE_HW:
	case CAM_B_BASE_HW:
	case CAM_C_BASE_HW:
		if (length > ISP_REG_RANGE) {
			LOG_NOTICE(
			    "mmap range error :module(0x%x) length(0x%lx),ISP_REG_RANGE(0x%lx)!\n",
			    pfn, length, ISP_REG_RANGE);
			return -EAGAIN;
		}
		break;
	case CAMSV_0_BASE_HW:
	case CAMSV_1_BASE_HW:
	case CAMSV_2_BASE_HW:
	case CAMSV_3_BASE_HW:
	case CAMSV_4_BASE_HW:
	case CAMSV_5_BASE_HW:
	case UNI_A_BASE_HW:
		if (length > ISP_REG_RANGE/2) {
			LOG_NOTICE(
			    "mmap range error :module(0x%x) length(0x%lx),ISP_REG_RANGE(0x%lx)!\n",
			    pfn, length, ISP_REG_RANGE/2);
			return -EAGAIN;
		}
		break;
	default:
		LOG_NOTICE("Illegal starting HW addr for mmap!\n");
		return -EAGAIN;
	}
	if (remap_pfn_range(
		pVma, pVma->vm_start, pVma->vm_pgoff,
		pVma->vm_end - pVma->vm_start, pVma->vm_page_prot))
		return -EAGAIN;

	/*  */
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/

static dev_t IspDevNo;
static struct cdev *pIspCharDrv;
static struct class *pIspClass;

static const struct file_operations IspFileOper = {
	.owner = THIS_MODULE,
	.open = ISP_open,
	.release = ISP_release,
	/* .flush       = mt_isp_flush, */
	.mmap = ISP_mmap,
	.unlocked_ioctl = ISP_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ISP_ioctl_compat,
#endif
};

/******************************************************************************
 *
 ******************************************************************************/
static inline void ISP_UnregCharDev(void)
{
	LOG_DBG("- E.");
	/*      */
	/* Release char driver */
	if (pIspCharDrv != NULL) {
		cdev_del(pIspCharDrv);
		pIspCharDrv = NULL;
	}
	/*      */
	unregister_chrdev_region(IspDevNo, 1);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline int ISP_RegCharDev(void)
{
	int Ret = 0;
	/*  */
	LOG_DBG("- E.\n");
	/*  */
	Ret = alloc_chrdev_region(&IspDevNo, 0, 1, ISP_DEV_NAME);
	if ((Ret) < 0) {
		LOG_NOTICE("alloc_chrdev_region failed, %d\n", Ret);
		return Ret;
	}
	/* Allocate driver */
	pIspCharDrv = cdev_alloc();
	if (pIspCharDrv == NULL) {
		LOG_NOTICE("cdev_alloc failed\n");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pIspCharDrv, &IspFileOper);
	/*  */
	pIspCharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pIspCharDrv, IspDevNo, 1);
	if ((Ret) < 0) {
		LOG_NOTICE("Attatch file operation failed, %d\n", Ret);
		goto EXIT;
	}
	/*  */
EXIT:
	if (Ret < 0)
		ISP_UnregCharDev();

	LOG_DBG("- X.\n");
	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static int ISP_probe(struct platform_device *pDev)
{
	int Ret = 0;
	/*    struct resource *pRes = NULL;*/
	int i = 0, j = 0;
	unsigned char n;
	unsigned int irq_info[3]; /* Record interrupts info from device tree */
	struct isp_device *_ispdev = NULL;

#ifdef CONFIG_OF
	struct isp_device *isp_dev;
	struct device *dev = NULL;
#endif

	LOG_INF("- E. ISP driver probe.\n");

	/* Get platform_device parameters */
#ifdef CONFIG_OF
	if (pDev == NULL) {
		dev_err(&pDev->dev, "pDev is NULL");
		return -ENXIO;
	}

	nr_isp_devs += 1;
	atomic_inc(&G_u4DevNodeCt);

	_ispdev = krealloc(
		isp_devs, sizeof(struct isp_device) * nr_isp_devs, GFP_KERNEL);
	if (!_ispdev) {
		dev_err(&pDev->dev, "Unable to allocate isp_devs\n");
		return -ENOMEM;
	}
	isp_devs = _ispdev;


	isp_dev = &(isp_devs[nr_isp_devs - 1]);
	isp_dev->dev = &pDev->dev;

	/* iomap registers */
	isp_dev->regs = of_iomap(pDev->dev.of_node, 0);
	if (!isp_dev->regs) {
		dev_err(&pDev->dev, "Unable to ioremap registers, of_iomap fail, nr_isp_devs=%d, devnode(%s).\n",
			nr_isp_devs, pDev->dev.of_node->name);
		return -ENOMEM;
	}

	LOG_INF("nr_isp_devs=%d, devnode(%s), map_addr=0x%lx\n",
		nr_isp_devs, pDev->dev.of_node->name,
		(unsigned long)isp_dev->regs);

	/* get IRQ ID and request IRQ */
	isp_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);

	if (isp_dev->irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array(
		    pDev->dev.of_node,
		    "interrupts", irq_info, ARRAY_SIZE(irq_info))) {
			dev_err(&pDev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++) {
			if ((strcmp(pDev->dev.of_node->name,
				IRQ_CB_TBL[i].device_name) == 0) &&
				(IRQ_CB_TBL[i].isr_fp != NULL)) {

				Ret = request_irq(
					isp_dev->irq,
					(irq_handler_t)IRQ_CB_TBL[i].isr_fp,
					irq_info[2],
					(const char *)IRQ_CB_TBL[i].device_name,
					NULL);
				if (Ret) {
					dev_err(&pDev->dev,
					"request_irq fail, nr_isp_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
					nr_isp_devs,
					pDev->dev.of_node->name,
					isp_dev->irq,
					IRQ_CB_TBL[i].device_name);
					return Ret;
				}

				LOG_INF(
				    "nr_isp_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
				    nr_isp_devs,
				    pDev->dev.of_node->name,
				    isp_dev->irq,
				    IRQ_CB_TBL[i].device_name);
				break;
			}
		}

		if (i >= ISP_IRQ_TYPE_AMOUNT)
			LOG_INF(
			    "No corresponding ISR!!: nr_isp_devs=%d, devnode(%s), irq=%d\n",
			    nr_isp_devs,
			    pDev->dev.of_node->name,
			    isp_dev->irq);


	} else {
		LOG_INF("No IRQ!!: nr_isp_devs=%d, devnode(%s), irq=%d\n",
			nr_isp_devs, pDev->dev.of_node->name, isp_dev->irq);
	}

	/* Only register char driver in the 1st time */
	if (nr_isp_devs == 1) {
		/* Register char driver */
		Ret = ISP_RegCharDev();
		if ((Ret)) {
			dev_err(&pDev->dev, "register char failed");
			return Ret;
		}

		/* Create class register */
		pIspClass = class_create(THIS_MODULE, "ispdrv");
		if (IS_ERR(pIspClass)) {
			Ret = PTR_ERR(pIspClass);
			LOG_NOTICE("Unable to create class, err = %d\n", Ret);
			goto EXIT;
		}
		dev = device_create(
			pIspClass, NULL,
			IspDevNo, NULL, ISP_DEV_NAME);

		if (IS_ERR(dev)) {
			Ret = PTR_ERR(dev);
			dev_err(&pDev->dev,
				"Failed to create device: /dev/%s, err = %d",
				ISP_DEV_NAME, Ret);
			goto EXIT;
		}

#endif

		/* Init spinlocks */
		spin_lock_init(&(IspInfo.SpinLockIspRef));
		spin_lock_init(&(IspInfo.SpinLockIsp));
		for (n = 0; n < ISP_IRQ_TYPE_AMOUNT; n++) {
			spin_lock_init(&(IspInfo.SpinLockIrq[n]));
			spin_lock_init(&(IspInfo.SpinLockIrqCnt[n]));
		}
		spin_lock_init(&(IspInfo.SpinLockRTBC));
		spin_lock_init(&(IspInfo.SpinLockClock));

		spin_lock_init(&(SpinLock_UserKey));
		#ifdef ENABLE_KEEP_ION_HANDLE
		for (i = 0; i < ISP_DEV_NODE_NUM; i++) {
			if (gION_TBL[i].node != ISP_DEV_NODE_NUM) {
				for (n = 0; n < _dma_max_wr_; n++)
					spin_lock_init(&(gION_TBL[i].pLock[n]));
			}
		}
		#endif

#ifndef EP_NO_CLKMGR /* CCF */
		isp_clk.ISP_SCP_SYS_DIS = devm_clk_get(
					&pDev->dev, "ISP_SCP_SYS_DIS");
		isp_clk.ISP_SCP_SYS_ISP = devm_clk_get(
					&pDev->dev, "ISP_SCP_SYS_ISP");
		isp_clk.ISP_SCP_SYS_CAM = devm_clk_get(
					&pDev->dev, "ISP_SCP_SYS_CAM");
		isp_clk.ISP_CAM_CAMSYS = devm_clk_get(
					&pDev->dev, "CAMSYS_CAM_CGPDN");
		isp_clk.ISP_CAM_CAMTG = devm_clk_get(
					&pDev->dev, "CAMSYS_CAMTG_CGPDN");
		isp_clk.ISP_CAM_CAMSV0 = devm_clk_get(
					&pDev->dev, "CAMSYS_CAMSV0_CGPDN");
		isp_clk.ISP_CAM_CAMSV1 = devm_clk_get(
					&pDev->dev, "CAMSYS_CAMSV1_CGPDN");
		isp_clk.ISP_CAM_CAMSV2 = devm_clk_get(
					&pDev->dev, "CAMSYS_CAMSV2_CGPDN");

		if (IS_ERR(isp_clk.ISP_SCP_SYS_DIS)) {
			LOG_NOTICE("cannot get ISP_SCP_SYS_DIS clock\n");
			return PTR_ERR(isp_clk.ISP_SCP_SYS_DIS);
		}
		if (IS_ERR(isp_clk.ISP_SCP_SYS_ISP)) {
			LOG_NOTICE("cannot get ISP_SCP_SYS_ISP clock\n");
			return PTR_ERR(isp_clk.ISP_SCP_SYS_ISP);
		}
		if (IS_ERR(isp_clk.ISP_SCP_SYS_CAM)) {
			LOG_NOTICE("cannot get ISP_SCP_SYS_CAM clock\n");
			return PTR_ERR(isp_clk.ISP_SCP_SYS_CAM);
		}
		if (IS_ERR(isp_clk.ISP_CAM_CAMSYS)) {
			LOG_NOTICE("cannot get ISP_CAM_CAMSYS clock\n");
			return PTR_ERR(isp_clk.ISP_CAM_CAMSYS);
		}
		if (IS_ERR(isp_clk.ISP_CAM_CAMTG)) {
			LOG_NOTICE("cannot get ISP_CAM_CAMTG clock\n");
			return PTR_ERR(isp_clk.ISP_CAM_CAMTG);
		}
		if (IS_ERR(isp_clk.ISP_CAM_CAMSV0)) {
			LOG_NOTICE("cannot get ISP_CAM_CAMSV0 clock\n");
			return PTR_ERR(isp_clk.ISP_CAM_CAMSV0);
		}
		if (IS_ERR(isp_clk.ISP_CAM_CAMSV1)) {
			LOG_NOTICE("cannot get ISP_CAM_CAMSV1 clock\n");
			return PTR_ERR(isp_clk.ISP_CAM_CAMSV1);
		}
		if (IS_ERR(isp_clk.ISP_CAM_CAMSV2)) {
			LOG_NOTICE("cannot get ISP_CAM_CAMSV2 clock\n");
			return PTR_ERR(isp_clk.ISP_CAM_CAMSV2);
		}
#endif
		/*  */
		for (i = 0 ; i < ISP_IRQ_TYPE_AMOUNT; i++)
			init_waitqueue_head(&IspInfo.WaitQueueHead[i]);

#ifdef CONFIG_PM_WAKELOCKS
		wakeup_source_init(&isp_wake_lock, "isp_lock_wakelock");
#else
		wake_lock_init(&isp_wake_lock,
					WAKE_LOCK_SUSPEND, "isp_lock_wakelock");
#endif

		for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++)
			tasklet_init(isp_tasklet[i].pIsp_tkt,
						isp_tasklet[i].tkt_cb, 0);

#if (ISP_BOTTOMHALF_WORKQ == 1)
		for (i = 0 ; i < ISP_IRQ_TYPE_AMOUNT; i++) {
			isp_workque[i].module = i;
			memset((void *)&(isp_workque[i].isp_bh_work), 0,
				sizeof(isp_workque[i].isp_bh_work));
			INIT_WORK(
			    &(isp_workque[i].isp_bh_work), ISP_BH_Workqueue);
		}
#endif


		/* Init IspInfo*/
		spin_lock(&(IspInfo.SpinLockIspRef));
		IspInfo.UserCount = 0;
		spin_unlock(&(IspInfo.SpinLockIspRef));
		/*  */
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAM_A_ST][SIGNAL_INT] =
			INT_ST_MASK_CAM;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAM_B_ST][SIGNAL_INT] =
			INT_ST_MASK_CAM;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAM_C_ST][SIGNAL_INT] =
			INT_ST_MASK_CAM;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV_0_ST][SIGNAL_INT] =
			INT_ST_MASK_CAMSV;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV_1_ST][SIGNAL_INT] =
			INT_ST_MASK_CAMSV;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV_2_ST][SIGNAL_INT] =
			INT_ST_MASK_CAMSV;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV_3_ST][SIGNAL_INT] =
			INT_ST_MASK_CAMSV;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV_4_ST][SIGNAL_INT] =
			INT_ST_MASK_CAMSV;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV_5_ST][SIGNAL_INT] =
			INT_ST_MASK_CAMSV;
		/* only cam have warning mask */
		IspInfo.IrqInfo.WarnMask[ISP_IRQ_TYPE_INT_CAM_A_ST][
			SIGNAL_INT] = INT_ST_MASK_CAM_WARN;
		IspInfo.IrqInfo.WarnMask[ISP_IRQ_TYPE_INT_CAM_B_ST][
			SIGNAL_INT] = INT_ST_MASK_CAM_WARN;
		IspInfo.IrqInfo.WarnMask[ISP_IRQ_TYPE_INT_CAM_C_ST][
			SIGNAL_INT] = INT_ST_MASK_CAM_WARN;
		IspInfo.IrqInfo.Warn2Mask[ISP_IRQ_TYPE_INT_CAM_A_ST][
			SIGNAL_INT] = INT_ST_MASK_CAM_WARN_2;
		IspInfo.IrqInfo.Warn2Mask[ISP_IRQ_TYPE_INT_CAM_B_ST][
			SIGNAL_INT] = INT_ST_MASK_CAM_WARN_2;
		IspInfo.IrqInfo.Warn2Mask[ISP_IRQ_TYPE_INT_CAM_C_ST][
			SIGNAL_INT] = INT_ST_MASK_CAM_WARN_2;
		/*  */
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAM_A_ST][SIGNAL_INT] =
			INT_ST_MASK_CAM_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAM_B_ST][SIGNAL_INT] =
			INT_ST_MASK_CAM_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAM_C_ST][SIGNAL_INT] =
			INT_ST_MASK_CAM_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV_0_ST][
			SIGNAL_INT] = INT_ST_MASK_CAMSV_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV_1_ST][
			SIGNAL_INT] = INT_ST_MASK_CAMSV_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV_2_ST][
			SIGNAL_INT] = INT_ST_MASK_CAMSV_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV_3_ST][
			SIGNAL_INT] = INT_ST_MASK_CAMSV_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV_4_ST][
			SIGNAL_INT] = INT_ST_MASK_CAMSV_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV_5_ST][
			SIGNAL_INT] = INT_ST_MASK_CAMSV_ERR;

		/* Init IrqCntInfo */
		for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++) {
			for (j = 0; j < ISP_ISR_MAX_NUM; j++) {
				IspInfo.IrqCntInfo.m_err_int_cnt[i][j] = 0;
				IspInfo.IrqCntInfo.m_warn_int_cnt[i][j] = 0;
			}
			IspInfo.IrqCntInfo.m_err_int_mark[i] = 0;
			IspInfo.IrqCntInfo.m_warn_int_mark[i] = 0;

			IspInfo.IrqCntInfo.m_int_usec[i] = 0;
		}


EXIT:
		if (Ret < 0)
			ISP_UnregCharDev();

	}

	LOG_INF("- X. ISP driver probe.\n");

	return Ret;
}

/******************************************************************************
 * Called when the device is being detached from the driver
 ******************************************************************************/
static int ISP_remove(struct platform_device *pDev)
{
	/*    struct resource *pRes;*/
	int IrqNum;
	int i;
	/*  */
	LOG_DBG("- E.");
	/* unregister char driver. */
	ISP_UnregCharDev();

	/* Release IRQ */
	disable_irq(IspInfo.IrqNum);
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);

	/* kill tasklet */
	for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++)
		tasklet_kill(isp_tasklet[i].pIsp_tkt);

	/*  */
	device_destroy(pIspClass, IspDevNo);
	/*  */
	class_destroy(pIspClass);
	pIspClass = NULL;
	/*  */
	return 0;
}

static int ISP_suspend(
	struct platform_device *pDev,
	pm_message_t            Mesg
)
{
	unsigned int regVal;
	int IrqType, ret, module;
	char moduleName[128];

	unsigned int regTGSt, loopCnt;
	struct ISP_WAIT_IRQ_STRUCT waitirq;
	ktime_t sec = 0, m_sec = 0;
	unsigned long long  timeoutMs = 500000000;/*500ms*/

	ret = 0;
	module = -1;
	strncpy(moduleName, pDev->dev.of_node->name, 127);

	/* update device node count*/
	atomic_dec(&G_u4DevNodeCt);

	/* Check clock counter instead of check IspInfo.UserCount
	 *  for ensuring current clocks are on or off
	 */
	spin_lock(&(IspInfo.SpinLockClock));
	if (!G_u4EnableClockCount) {
		spin_unlock(&(IspInfo.SpinLockClock));
		/* Only print cama log */
		if (strcmp(moduleName,
		    IRQ_CB_TBL[ISP_IRQ_TYPE_INT_CAM_A_ST].device_name) == 0) {
			LOG_DBG("%s - X. UserCount=%d,wakelock:%d,devct:%d\n",
				moduleName, IspInfo.UserCount,
				G_u4EnableClockCount,
				atomic_read(&G_u4DevNodeCt));
		} else if (IspInfo.UserCount != 0) {
			LOG_INF(
			    "%s - X. UserCount=%d,G_u4EnableClockCount=0,wakelock:%d,devct:%d\n",
			    moduleName,
			    IspInfo.UserCount,
			    G_u4EnableClockCount,
			    atomic_read(&G_u4DevNodeCt));
		}

		return ret;
	}
	spin_unlock(&(IspInfo.SpinLockClock));

	for (IrqType = 0; IrqType < ISP_IRQ_TYPE_AMOUNT; IrqType++) {
		if (strcmp(moduleName, IRQ_CB_TBL[IrqType].device_name) == 0)
			break;
	}

	switch (IrqType) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
		module = ISP_CAM_A_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
		module = ISP_CAM_B_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		module = ISP_CAM_C_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
		module = ISP_CAMSV0_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
		module = ISP_CAMSV1_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
		module = ISP_CAMSV2_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
		module = ISP_CAMSV3_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
		module = ISP_CAMSV4_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
		module = ISP_CAMSV5_IDX;
		break;
	case ISP_IRQ_TYPE_AMOUNT:
		LOG_NOTICE("dev name is not found (%s)", moduleName);
		break;
	case ISP_IRQ_TYPE_INT_UNI_A_ST:
	default:
		/*don nothing*/
		break;
	}

	if (module < 0)
		goto EXIT;

	regVal = ISP_RD32(CAM_REG_TG_VF_CON(module));
	/*LOG_DBG("%s: Rs_TG(0x%08x)\n", moduleName, regVal);*/

	if (regVal & 0x01) {
		LOG_INF("%s_suspend,disable VF,wakelock:%d,clk:%d,devct:%d\n",
			moduleName, g_WaitLockCt, G_u4EnableClockCount,
			atomic_read(&G_u4DevNodeCt));
		SuspnedRecord[module] = 1;
		/* disable VF */
		ISP_WR32(CAM_REG_TG_VF_CON(module), (regVal & (~0x01)));

		/* wait TG idle*/
		loopCnt = 3;
		waitirq.Type = IrqType;
		waitirq.EventInfo.Clear = ISP_IRQ_CLEAR_WAIT;
		waitirq.EventInfo.Status = VS_INT_ST;
		waitirq.EventInfo.St_type = SIGNAL_INT;
		waitirq.EventInfo.Timeout = 0x100;
		waitirq.EventInfo.UserKey = 0x0;
		waitirq.bDumpReg = 0;

		do {
			regTGSt = (ISP_RD32(
				CAM_REG_TG_INTER_ST(module)) & 0x00003F00) >> 8;
			if (regTGSt == 1)
				break;

			LOG_INF("%s: wait 1VD (%d)\n", moduleName, loopCnt);
			ret = ISP_WaitIrq(&waitirq);
			/* first wait is clear wait, others are non-clear wait*/
			waitirq.EventInfo.Clear = ISP_IRQ_CLEAR_NONE;
		} while (--loopCnt);

		if (-ERESTARTSYS == ret) {
			LOG_INF("%s: interrupt by system signal, wait idle\n",
				moduleName);
			/* timer*/
			m_sec = ktime_get();

			while (regTGSt != 1) {
				regTGSt = (ISP_RD32(CAM_REG_TG_INTER_ST(
						module)) & 0x00003F00) >> 8;
				/*timer*/
				sec = ktime_get();
				/* wait time>timeoutMs, break */
				if ((sec - m_sec) > timeoutMs)
					break;
			}
			if (regTGSt == 1)
				LOG_INF("%s: wait idle done\n", moduleName);
			else
				LOG_INF("%s: wait idle timeout(%lld)\n",
					moduleName, (sec - m_sec));
		}

		/*backup: frame CNT
		 * After VF enable, The frame count will be 0 at next VD;
		 * if it has P1_DON after set vf disable, g_BkReg no need
		 * to add 1
		 */
		regTGSt = ISP_RD32_TG_CAM_FRM_CNT(IrqType, module);
		g_BkReg[IrqType].CAM_TG_INTER_ST = regTGSt;
		regVal = ISP_RD32(CAM_REG_TG_SEN_MODE(module));
		ISP_WR32(CAM_REG_TG_SEN_MODE(module), (regVal & (~0x01)));
	} else {
		LOG_INF("%s_suspend,wakelock:%d,clk:%d,devct:%d\n",
			moduleName, g_WaitLockCt, G_u4EnableClockCount,
			atomic_read(&G_u4DevNodeCt));
		SuspnedRecord[module] = 0;
	}

EXIT:
	/* last dev node will disable clk "G_u4EnableClockCount" times */
	if (!atomic_read(&G_u4DevNodeCt)) {
		spin_lock(&(IspInfo.SpinLockClock));
		loopCnt = G_u4EnableClockCount;
		spin_unlock(&(IspInfo.SpinLockClock));

		LOG_INF("%s - X. wakelock:%d, last dev node,disable clk:%d\n",
			moduleName, g_WaitLockCt, loopCnt);
		while (loopCnt > 0) {
			ISP_EnableClock(MFALSE);
			loopCnt--;
		}
	}

	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static int ISP_resume(struct platform_device *pDev)
{
	unsigned int regVal;
	int IrqType, ret, module;
	char moduleName[128];

	ret = 0;
	module = -1;
	strncpy(moduleName, pDev->dev.of_node->name, 127);

	/* update device node count*/
	atomic_inc(&G_u4DevNodeCt);

	if (IspInfo.UserCount == 0) {
		/* Only print cama log */
		if (strcmp(moduleName,
			IRQ_CB_TBL[ISP_IRQ_TYPE_INT_CAM_A_ST].device_name) == 0)
			LOG_DBG("%s - X. UserCount=0\n", moduleName);

		return 0;
	}

	for (IrqType = 0; IrqType < ISP_IRQ_TYPE_AMOUNT; IrqType++) {
		if (strcmp(moduleName, IRQ_CB_TBL[IrqType].device_name) == 0)
			break;
	}

	switch (IrqType) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
		module = ISP_CAM_A_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
		module = ISP_CAM_B_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		module = ISP_CAM_C_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
		module = ISP_CAMSV0_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
		module = ISP_CAMSV1_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
		module = ISP_CAMSV2_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
		module = ISP_CAMSV3_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
		module = ISP_CAMSV4_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
		module = ISP_CAMSV5_IDX;
		break;
	case ISP_IRQ_TYPE_AMOUNT:
		LOG_NOTICE("dev name is not found (%s)", moduleName);
		break;
	case ISP_IRQ_TYPE_INT_UNI_A_ST:
	default:
		/*don nothing*/
		break;
	}

	if (module < 0)
		return ret;

	archcounter_timesync_init(MTRUE); /* Global timer enable */

	ISP_EnableClock(MTRUE);

	if (SuspnedRecord[module]) {
		LOG_INF("%s_resume,enable VF,wakelock:%d,clk:%d,devct:%d\n",
			moduleName, g_WaitLockCt, G_u4EnableClockCount,
			atomic_read(&G_u4DevNodeCt));
		SuspnedRecord[module] = 0;

		/*cmos*/
		regVal = ISP_RD32(CAM_REG_TG_SEN_MODE(module));
		ISP_WR32(CAM_REG_TG_SEN_MODE(module), (regVal | 0x01));
		/*vf*/
		regVal = ISP_RD32(CAM_REG_TG_VF_CON(module));
		ISP_WR32(CAM_REG_TG_VF_CON(module), (regVal | 0x01));
	} else {
		LOG_INF("%s_resume,wakelock:%d,clk:%d,devct:%d\n",
			moduleName, g_WaitLockCt, G_u4EnableClockCount,
			atomic_read(&G_u4DevNodeCt));
	}

	return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int ISP_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	/*pr_debug("calling %s()\n", __func__);*/

	return ISP_suspend(pdev, PMSG_SUSPEND);
}

int ISP_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	/*pr_debug("calling %s()\n", __func__);*/

	return ISP_resume(pdev);
}

/* extern void mt_irq_set_sens(unsigned int irq, unsigned int sens); */
/* extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity); */
int ISP_pm_restore_noirq(struct device *device)
{
	/*pr_debug("calling %s()\n", __func__);*/
#ifndef CONFIG_OF
	mt_irq_set_sens(CAM0_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(CAM0_IRQ_BIT_ID, MT_POLARITY_LOW);
#endif
	return 0;

}
/*---------------------------------------------------------------------------*/
#else /*CONFIG_PM*/
/*---------------------------------------------------------------------------*/
#define ISP_pm_suspend NULL
#define ISP_pm_resume  NULL
#define ISP_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif /*CONFIG_PM*/
/*---------------------------------------------------------------------------*/

const struct dev_pm_ops ISP_pm_ops = {
	.suspend = ISP_pm_suspend,
	.resume = ISP_pm_resume,
	.freeze = ISP_pm_suspend,
	.thaw = ISP_pm_resume,
	.poweroff = ISP_pm_suspend,
	.restore = ISP_pm_resume,
	.restore_noirq = ISP_pm_restore_noirq,
};


/******************************************************************************
 *
 ******************************************************************************/
static struct platform_driver IspDriver = {
	.probe   = ISP_probe,
	.remove  = ISP_remove,
	.suspend = ISP_suspend,
	.resume  = ISP_resume,
	.driver  = {
		.name  = ISP_DEV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = isp_of_ids,
#endif
#ifdef CONFIG_PM
		.pm     = &ISP_pm_ops,
#endif
	}
};

/******************************************************************************
 *
 ******************************************************************************/
static ssize_t ISP_DumpRegToProc(
	struct file *pFile,
	char *pStart,
	size_t off,
	loff_t *Count)
{
	LOG_NOTICE("Not implement");
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static ssize_t ISP_RegDebug(
	struct file *pFile,
	const char *pBuffer,
	size_t Count,
	loff_t *pData)
{
	LOG_NOTICE("Not implement");
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static ssize_t CAMIO_DumpRegToProc(
	struct file *pFile,
	char *pStart,
	size_t off,
	loff_t *Count)
{
	LOG_NOTICE("Not implement");
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static ssize_t CAMIO_RegDebug(
	struct file *pFile,
	const char *pBuffer,
	size_t Count,
	loff_t *pData)
{
	LOG_NOTICE("Not implement");
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static const struct file_operations fcameraisp_proc_fops = {
	.read = ISP_DumpRegToProc,
	.write = ISP_RegDebug,
};
static const struct file_operations fcameraio_proc_fops = {
	.read = CAMIO_DumpRegToProc,
	.write = CAMIO_RegDebug,
};
/******************************************************************************
 *
 ******************************************************************************/

static int __init ISP_Init(void)
{
	int Ret = 0, i, j;
	void *tmp;
	struct device_node *node = NULL;

	/*  */
	LOG_DBG("- E.");
	/*  */
	mutex_init(&ion_client_mutex);
	/*  */
	atomic_set(&G_u4DevNodeCt, 0);
	/*  */
	Ret = platform_driver_register(&IspDriver);
	if ((Ret) < 0) {
		LOG_NOTICE("platform_driver_register fail");
		return Ret;
	}

	/* Use of_find_compatible_node() sensor registers from device tree */
	/* Don't use compatitble define in probe(). Otherwise, probe() of
	 * Seninf driver cannot be called.
	 */
	#if (SMI_LARB_MMU_CTL == 1)
	do {
		char *comp_str = NULL;

		comp_str = kmalloc(64, GFP_KERNEL);
		if (comp_str == NULL) {
			LOG_NOTICE("kmalloc failed for finding compatible\n");
			break;
		}

		for (i = 0; i < ARRAY_SIZE(SMI_LARB_BASE); i++) {

			snprintf(comp_str, 64, "mediatek,smi_larb%d", i);
			LOG_INF("Finding SMI_LARB compatible: %s\n", comp_str);

			node = of_find_compatible_node(NULL, NULL, comp_str);
			if (!node) {
				LOG_NOTICE("find %s node failed!\n", comp_str);
				SMI_LARB_BASE[i] = 0;
				continue;
			}
			SMI_LARB_BASE[i] = of_iomap(node, 0);
			if (!SMI_LARB_BASE[i]) {
				LOG_NOTICE(
				    "unable to map SMI_LARB_BASE registers!\n");
				break;
			}
			LOG_INF("SMI_LARB%d_BASE: %p\n", i, SMI_LARB_BASE[i]);
		}

		/* if (comp_str) coverity: no need if, kfree is safe */
		kfree(comp_str);
	} while (0);
	#endif
	node = of_find_compatible_node(NULL, NULL, "mediatek,mmsys_config");
	if (!node) {
		LOG_NOTICE("find mmsys_config node failed!!!\n");
		return -ENODEV;
	}

	/* FIX-ME: linux-3.10 procfs API changed */
	proc_create("driver/isp_reg", 0444, NULL, &fcameraisp_proc_fops);
	proc_create("driver/camio_reg", 0444, NULL, &fcameraio_proc_fops);

	for (j = 0; j < ISP_IRQ_TYPE_AMOUNT; j++) {
		switch (j) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
		case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
		case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
		case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
		case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
		case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
		case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
			if (sizeof(struct ISP_RT_BUF_STRUCT) >
				((RT_BUF_TBL_NPAGES) * PAGE_SIZE)) {
				i = 0;
				while (i < sizeof(struct ISP_RT_BUF_STRUCT))
					i += PAGE_SIZE;

				pBuf_kmalloc[j] =
					kmalloc(i + 2 * PAGE_SIZE, GFP_KERNEL);
				if ((pBuf_kmalloc[j]) == NULL) {
					LOG_NOTICE("mem not enough\n");
					return -ENOMEM;
				}
				memset(pBuf_kmalloc[j], 0x00, i);
				Tbl_RTBuf_MMPSize[j] = i;
			} else {
				pBuf_kmalloc[j] = kmalloc(
					(RT_BUF_TBL_NPAGES + 2) * PAGE_SIZE,
					GFP_KERNEL);
				if ((pBuf_kmalloc[j]) == NULL) {
					LOG_NOTICE("mem not enough\n");
					return -ENOMEM;
				}
				memset(pBuf_kmalloc[j], 0x00,
					(RT_BUF_TBL_NPAGES + 2)*PAGE_SIZE);
				Tbl_RTBuf_MMPSize[j] = (RT_BUF_TBL_NPAGES + 2);

			}
			/* round it up to the page bondary */
			pTbl_RTBuf[j] = (int *)(((
				(unsigned long)pBuf_kmalloc[j]) + PAGE_SIZE - 1)
				& PAGE_MASK);
			pstRTBuf[j] = (struct ISP_RT_BUF_STRUCT *)pTbl_RTBuf[j];
			pstRTBuf[j]->state = ISP_RTBC_STATE_INIT;
			break;
		default:
			pBuf_kmalloc[j] = NULL;
			pTbl_RTBuf[j] = NULL;
			Tbl_RTBuf_MMPSize[j] = 0;
			break;
		}
	}


	/* isr log */
	if (PAGE_SIZE < ((ISP_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
		((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1))*LOG_PPNUM)) {
		i = 0;
		while (i < ((ISP_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
			((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1))*LOG_PPNUM))
			i += PAGE_SIZE;

	} else {
		i = PAGE_SIZE;
	}
	pLog_kmalloc = kmalloc(i, GFP_KERNEL);
	if ((pLog_kmalloc) == NULL) {
		LOG_NOTICE("mem not enough\n");
		return -ENOMEM;
	}
	memset(pLog_kmalloc, 0x00, i);
	tmp = pLog_kmalloc;
	for (i = 0; i < LOG_PPNUM; i++) {
		for (j = 0; j < ISP_IRQ_TYPE_AMOUNT; j++) {
			gSvLog[j]._str[i][_LOG_DBG] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp + */
			/* (NORMAL_STR_LEN*DBG_PAGE)); */
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * DBG_PAGE));
			gSvLog[j]._str[i][_LOG_INF] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp + */
			/* (NORMAL_STR_LEN*INF_PAGE)); */
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * INF_PAGE));
			gSvLog[j]._str[i][_LOG_ERR] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp + */
			/* (NORMAL_STR_LEN*ERR_PAGE)); */
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * ERR_PAGE));
		}
		/* log buffer ,in case of overflow */
		/* tmp = (void*) ((unsigned int)tmp + NORMAL_STR_LEN); */
		tmp = (void *)((char *)tmp + NORMAL_STR_LEN);
	}
	/* mark the pages reserved , FOR MMAP*/
	for (j = 0; j < ISP_IRQ_TYPE_AMOUNT; j++) {
		if (pTbl_RTBuf[j] != NULL) {
			for (i = 0; i < Tbl_RTBuf_MMPSize[j] * PAGE_SIZE;
				i += PAGE_SIZE)
				SetPageReserved(virt_to_page(
				   ((unsigned long)pTbl_RTBuf[j]) + i));

		}
	}

	for (i = 0; i < ISP_DEV_NODE_NUM; i++)
		SuspnedRecord[i] = 0;

	LOG_DBG("- E. Ret: %d.", Ret);
	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static void __exit ISP_Exit(void)
{
	int i, j;

	LOG_DBG("- B.");
	/*  */
	platform_driver_unregister(&IspDriver);
	/*  */

	for (j = 0; j < ISP_IRQ_TYPE_AMOUNT; j++) {
		if (pTbl_RTBuf[j] != NULL) {
			/* unreserve the pages */
			for (i = 0; i <
			    Tbl_RTBuf_MMPSize[j] * PAGE_SIZE; i += PAGE_SIZE)
				ClearPageReserved(virt_to_page(
					((unsigned long)pTbl_RTBuf[j]) + i));

			/* free the memory areas */
			kfree(pBuf_kmalloc[j]);
		}
	}

	/* free the memory areas */
	kfree(pLog_kmalloc);

	LOG_DBG("- E.");

}

void IRQ_INT_ERR_CHECK_CAM(unsigned int WarnStatus, unsigned int ErrStatus,
	unsigned int warnTwo, enum ISP_IRQ_TYPE_ENUM module)
{
	/* ERR print */
	if (ErrStatus) {
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAM_A_ST]
				.ispIntErr |= (ErrStatus|WarnStatus);
			g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAM_A_ST]
				.ispInt3Err |= (warnTwo);
			g_ISPIntStatus_SMI[
			  ISP_IRQ_TYPE_INT_CAM_A_ST].ispIntErr =
			  g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAM_A_ST].ispIntErr;
			g_ISPIntStatus_SMI[
			  ISP_IRQ_TYPE_INT_CAM_A_ST].ispInt3Err =
			  g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAM_A_ST].ispInt3Err;

			IRQ_LOG_KEEPER(
			    module, m_CurrentPPB, _LOG_ERR,
			    "CAM_A:raw_int_err:0x%x_0x%x, raw_int3_err:0x%x\n",
			    WarnStatus, ErrStatus, warnTwo);

			/* DMA ERR print */
			if (ErrStatus & DMA_ERR_ST)
				ISP_DumpDmaDeepDbg(module);

			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAM_B_ST].ispIntErr |=
			  (ErrStatus|WarnStatus);
			g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAM_B_ST].ispInt3Err |=
			  (warnTwo);
			g_ISPIntStatus_SMI[
			  ISP_IRQ_TYPE_INT_CAM_B_ST].ispIntErr =
			  g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAM_B_ST].ispIntErr;
			g_ISPIntStatus_SMI[
			  ISP_IRQ_TYPE_INT_CAM_B_ST].ispInt3Err =
			  g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAM_B_ST].ispInt3Err;

			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
			    "CAM_B:raw_int_err:0x%x_0x%x, raw_int3_err:0x%x\n",
			    WarnStatus, ErrStatus, warnTwo);

			/* DMA ERR print */
			if (ErrStatus & DMA_ERR_ST)
				ISP_DumpDmaDeepDbg(module);

			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAM_C_ST].ispIntErr |=
			  (ErrStatus|WarnStatus);
			g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAM_C_ST].ispInt3Err |=
			  (warnTwo);
			g_ISPIntStatus_SMI[
			  ISP_IRQ_TYPE_INT_CAM_C_ST].ispIntErr =
			  g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAM_C_ST].ispIntErr;
			g_ISPIntStatus_SMI[
			  ISP_IRQ_TYPE_INT_CAM_C_ST].ispInt3Err =
			  g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAM_C_ST].ispInt3Err;

			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
			  "CAM_C:raw_int_err:0x%x_0x%x, raw_int3_err:0x%x\n",
			  WarnStatus, ErrStatus, warnTwo);

			/* DMA ERR print */
			if (ErrStatus & DMA_ERR_ST)
				ISP_DumpDmaDeepDbg(module);

			break;
		case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
			g_ISPIntStatus[
			  ISP_IRQ_TYPE_INT_CAMSV_0_ST].ispIntErr |=
			  (ErrStatus|WarnStatus);
			g_ISPIntStatus_SMI[
			  ISP_IRQ_TYPE_INT_CAMSV_0_ST].ispIntErr =
			  g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAMSV_0_ST].ispIntErr;
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
			  "CAMSV0:int_err:0x%x_0x%x\n", WarnStatus, ErrStatus);
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
			g_ISPIntStatus[
			  ISP_IRQ_TYPE_INT_CAMSV_1_ST].ispIntErr |=
			  (ErrStatus|WarnStatus);
			g_ISPIntStatus_SMI[
			  ISP_IRQ_TYPE_INT_CAMSV_1_ST].ispIntErr =
			  g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAMSV_1_ST].ispIntErr;

			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
			  "CAMSV1:int_err:0x%x_0x%x\n", WarnStatus, ErrStatus);
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
			g_ISPIntStatus[
			  ISP_IRQ_TYPE_INT_CAMSV_2_ST].ispIntErr |=
			  (ErrStatus|WarnStatus);
			g_ISPIntStatus_SMI[
			  ISP_IRQ_TYPE_INT_CAMSV_2_ST].ispIntErr =
			  g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAMSV_2_ST].ispIntErr;

			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
			  "CAMSV2:int_err:0x%x_0x%x\n", WarnStatus, ErrStatus);
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
			g_ISPIntStatus[
			  ISP_IRQ_TYPE_INT_CAMSV_3_ST].ispIntErr |=
			  (ErrStatus|WarnStatus);
			g_ISPIntStatus_SMI[
			  ISP_IRQ_TYPE_INT_CAMSV_3_ST].ispIntErr =
			  g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAMSV_3_ST].ispIntErr;

			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
			  "CAMSV3:int_err:0x%x_0x%x\n", WarnStatus, ErrStatus);
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
			g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAMSV_4_ST].ispIntErr |=
			  (ErrStatus|WarnStatus);
			g_ISPIntStatus_SMI[
			  ISP_IRQ_TYPE_INT_CAMSV_4_ST].ispIntErr =
			  g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAMSV_4_ST].ispIntErr;
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
			  "CAMSV4:int_err:0x%x_0x%x\n", WarnStatus, ErrStatus);
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
			g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAMSV_5_ST].ispIntErr |=
			  (ErrStatus|WarnStatus);
			g_ISPIntStatus_SMI[
			  ISP_IRQ_TYPE_INT_CAMSV_5_ST].ispIntErr =
			  g_ISPIntStatus[ISP_IRQ_TYPE_INT_CAMSV_5_ST].ispIntErr;
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
			  "CAMSV5:int_err:0x%x_0x%x\n", WarnStatus, ErrStatus);
			break;
		default:
			break;
		}
	}
}


enum CAM_FrameST Irq_CAM_FrameStatus(
				enum ISP_DEV_NODE_ENUM module,
				enum ISP_IRQ_TYPE_ENUM irq_mod,
				unsigned int delayCheck)
{
	int dma_arry_map[_cam_max_] = {
		/*      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,*/
		 0, /* _imgo_*/
		 1, /* _rrzo_ */
		 2, /* _ufeo_ */
		-1, /* _aao_ */
		-1, /* _afo_ */
		 3, /* _lcso_ */
		-1, /* _pdo_ */
		 4, /* _lmvo_ */
		-1, /* _flko_ */
		 5, /* _rsso_ */
		-1, /* _pso_ */
		 6 /* _ufgo_ */
	};

	unsigned int dma_en;
	union FBC_CTRL_1 fbc_ctrl1[7];
	union FBC_CTRL_2 fbc_ctrl2[7];
	bool bQueMode = MFALSE;
	unsigned int product = 1;
	/* TSTP_V3 unsigned int frmPeriod = */
	/* ((ISP_RD32(CAM_REG_TG_SUB_PERIOD(module)) >> 8) & 0x1F) + 1; */
	unsigned int i;

	if ((module < ISP_CAM_A_IDX) || (module >= ISP_CAMSV0_IDX)) {
		LOG_NOTICE("unsupported module:0x%x\n", module);
		return CAM_FST_DROP_FRAME;
	}

	if (sec_on)
		dma_en = lock_reg.CAM_REG_CTL_DMA_EN[module];
	else
		dma_en = ISP_RD32(CAM_REG_CTL_DMA_EN(module));

	if (dma_en & 0x1) {
		fbc_ctrl1[dma_arry_map[_imgo_]].Raw =
			ISP_RD32(CAM_REG_FBC_IMGO_CTL1(module));
		fbc_ctrl2[dma_arry_map[_imgo_]].Raw =
			ISP_RD32(CAM_REG_FBC_IMGO_CTL2(module));
	} else {
		fbc_ctrl1[dma_arry_map[_imgo_]].Raw = 0x0;
		fbc_ctrl2[dma_arry_map[_imgo_]].Raw = 0x0;
	}

	if (dma_en & 0x2) {
		fbc_ctrl1[dma_arry_map[_ufeo_]].Raw =
			ISP_RD32(CAM_REG_FBC_UFEO_CTL1(module));
		fbc_ctrl2[dma_arry_map[_ufeo_]].Raw =
			ISP_RD32(CAM_REG_FBC_UFEO_CTL2(module));
	} else {
		fbc_ctrl1[dma_arry_map[_ufeo_]].Raw = 0x0;
		fbc_ctrl2[dma_arry_map[_ufeo_]].Raw = 0x0;
	}


	if (dma_en & 0x4) {
		fbc_ctrl1[dma_arry_map[_rrzo_]].Raw =
			ISP_RD32(CAM_REG_FBC_RRZO_CTL1(module));
		fbc_ctrl2[dma_arry_map[_rrzo_]].Raw =
			ISP_RD32(CAM_REG_FBC_RRZO_CTL2(module));
	} else {
		fbc_ctrl1[dma_arry_map[_rrzo_]].Raw = 0x0;
		fbc_ctrl2[dma_arry_map[_rrzo_]].Raw = 0x0;
	}

	if (dma_en & 0x10) {
		fbc_ctrl1[dma_arry_map[_lcso_]].Raw =
			ISP_RD32(CAM_REG_FBC_LCSO_CTL1(module));
		fbc_ctrl2[dma_arry_map[_lcso_]].Raw =
			ISP_RD32(CAM_REG_FBC_LCSO_CTL2(module));
	} else {
		fbc_ctrl1[dma_arry_map[_lcso_]].Raw = 0x0;
		fbc_ctrl2[dma_arry_map[_lcso_]].Raw = 0x0;
	}

	if (dma_en & 0x4000) {
		fbc_ctrl1[dma_arry_map[_lmvo_]].Raw =
			ISP_RD32(CAM_REG_FBC_LMVO_CTL1(module));
		fbc_ctrl2[dma_arry_map[_lmvo_]].Raw =
			ISP_RD32(CAM_REG_FBC_LMVO_CTL2(module));
	} else {
		fbc_ctrl1[dma_arry_map[_lmvo_]].Raw = 0x0;
		fbc_ctrl2[dma_arry_map[_lmvo_]].Raw = 0x0;
	}

	if (dma_en & 0x8000) {
		fbc_ctrl1[dma_arry_map[_rsso_]].Raw =
			ISP_RD32(CAM_REG_FBC_RSSO_CTL1(module));
		fbc_ctrl2[dma_arry_map[_rsso_]].Raw =
			ISP_RD32(CAM_REG_FBC_RSSO_CTL2(module));
	} else {
		fbc_ctrl1[dma_arry_map[_rsso_]].Raw = 0x0;
		fbc_ctrl2[dma_arry_map[_rsso_]].Raw = 0x0;
	}

	if (dma_en & 0x10000) {
		fbc_ctrl1[dma_arry_map[_ufgo_]].Raw =
			ISP_RD32(CAM_REG_FBC_UFGO_CTL1(module));
		fbc_ctrl2[dma_arry_map[_ufgo_]].Raw =
			ISP_RD32(CAM_REG_FBC_UFGO_CTL2(module));
	} else {
		fbc_ctrl1[dma_arry_map[_ufgo_]].Raw = 0x0;
		fbc_ctrl2[dma_arry_map[_ufgo_]].Raw = 0x0;
	}

	for (i = 0; i < _cam_max_; i++) {
		if (dma_arry_map[i] >= 0) {
			if (fbc_ctrl1[dma_arry_map[i]].Raw != 0) {
				bQueMode =
				    fbc_ctrl1[dma_arry_map[i]].Bits.FBC_MODE;
				break;
			}
		}
	}

	if (bQueMode) {
		for (i = 0; i < _cam_max_; i++) {
			if (dma_arry_map[i] < 0)
				continue;

			if (fbc_ctrl1[dma_arry_map[i]].Raw != 0) {
#if 0 /* TSTP_V3 (TIMESTAMP_QUEUE_EN == 1) */
				if (delayCheck == 0) {
					if (fbc_ctrl2[dma_arry_map[i]]
						.Bits.DROP_CNT !=
					    IspInfo.TstpQInfo[irq_mod]
						.Dmao[i].PrevFbcDropCnt) {
						IspInfo.TstpQInfo[irq_mod]
						    .Dmao[i].PrevFbcDropCnt =
						((IspInfo.TstpQInfo[irq_mod]
						    .Dmao[i].PrevFbcDropCnt
						    + frmPeriod) & 0xFF);
						product = 0;
					}
					/* Prevent *0 for SOF ISR delayed
					 * after P1_DON
					 */
					if (fbc_ctrl2[dma_arry_map[i]]
						.Bits.FBC_CNT == 0)
						product *= 1;
					else
						product *=
						    fbc_ctrl2[dma_arry_map[
							i]].Bits.FBC_CNT;
				} else {
					LOG_INF(
					"cam:%d dma:%d overwrite preveFbcDropCnt %d <= %d subsample:%d\n",
					irq_mod, i,
					IspInfo.TstpQInfo[irq_mod]
						.Dmao[i].PrevFbcDropCnt,
					fbc_ctrl2[dma_arry_map[i]]
						.Bits.DROP_CNT,
					frmPeriod);

					IspInfo.TstpQInfo[irq_mod]
						.Dmao[i].PrevFbcDropCnt =
					(frmPeriod > 1) ?
					(fbc_ctrl2[dma_arry_map[i]]
					  .Bits.DROP_CNT/frmPeriod)*frmPeriod :
					fbc_ctrl2[dma_arry_map[i]]
					  .Bits.DROP_CNT;

					product *= fbc_ctrl2[dma_arry_map[i]]
							.Bits.FBC_CNT;
				}
#else
				product *= fbc_ctrl2[dma_arry_map[i]]
								.Bits.FBC_CNT;
				if (product == 0)
					return CAM_FST_DROP_FRAME;
#endif
			}

		}
	} else {
		for (i = 0; i < _cam_max_; i++) {
			if (dma_arry_map[i] < 0)
				continue;

			if (fbc_ctrl1[dma_arry_map[i]].Raw != 0) {
#if 0 /* TSTP_V3 (TIMESTAMP_QUEUE_EN == 1) */
				if (delayCheck == 0) {
					if (fbc_ctrl2[dma_arry_map[i]]
					    .Bits.DROP_CNT !=
					    IspInfo.TstpQInfo[irq_mod]
					    .Dmao[i].PrevFbcDropCnt) {
						IspInfo.TstpQInfo[irq_mod]
						    .Dmao[i].PrevFbcDropCnt =
						((IspInfo.TstpQInfo[irq_mod]
						    .Dmao[i].PrevFbcDropCnt +
							frmPeriod) & 0xFF);
						product = 0;
					}
					if ((fbc_ctrl1[dma_arry_map[i]]
					    .Bits.FBC_NUM -
					    fbc_ctrl2[dma_arry_map[i]]
					    .Bits.FBC_CNT) == 0)
						product *= 1;
					else
						product *= (fbc_ctrl1[
							    dma_arry_map[i]]
							      .Bits.FBC_NUM -
							    fbc_ctrl2[
							      dma_arry_map[i]]
							      .Bits.FBC_CNT);
				} else {
					IspInfo.TstpQInfo[irq_mod]
						.Dmao[i].PrevFbcDropCnt =
					(frmPeriod > 1) ?
					(fbc_ctrl2[dma_arry_map[i]]
					  .Bits.DROP_CNT/frmPeriod)*frmPeriod :
					 fbc_ctrl2[dma_arry_map[i]]
					  .Bits.DROP_CNT;

					product *= (fbc_ctrl1[dma_arry_map[i]]
							.Bits.FBC_NUM -
						    fbc_ctrl2[dma_arry_map[i]]
							.Bits.FBC_CNT);
				}
#else
				product *= (fbc_ctrl1[dma_arry_map[i]].
						Bits.FBC_NUM -
					    fbc_ctrl2[dma_arry_map[i]].
						Bits.FBC_CNT);
				if (product == 0)
					return CAM_FST_DROP_FRAME;
#endif
			}

		}
	}

	if (product == 1)
		return CAM_FST_LAST_WORKING_FRAME;
	else
		return CAM_FST_NORMAL;
}

#if (TIMESTAMP_QUEUE_EN == 1)
static void ISP_GetDmaPortsStatus
	(enum ISP_DEV_NODE_ENUM reg_module, unsigned int *DmaPortsStats)
{
	unsigned int dma_en = 0;

	if (sec_on)
		dma_en = lock_reg.CAM_REG_CTL_DMA_EN[reg_module];
	else
		dma_en = ISP_RD32(CAM_REG_CTL_DMA_EN(reg_module));

	DmaPortsStats[_imgo_] = ((dma_en & 0x01) ? 1 : 0);
	DmaPortsStats[_ufeo_] = ((dma_en & 0x02) ? 1 : 0);
	DmaPortsStats[_rrzo_] = ((dma_en & 0x04) ? 1 : 0);
	DmaPortsStats[_lcso_] = ((dma_en & 0x10) ? 1 : 0);

	DmaPortsStats[_aao_] = ((dma_en & 0x20) ? 1 : 0);
	DmaPortsStats[_pso_] = ((dma_en & 0x40) ? 1 : 0);
	DmaPortsStats[_afo_] = ((dma_en & 0x08) ? 1 : 0);
	DmaPortsStats[_pdo_] = ((dma_en & 0x400) ? 1 : 0);

	DmaPortsStats[_flko_] = ((dma_en & 0x2000) ? 1 : 0);
	DmaPortsStats[_lmvo_] = ((dma_en & 0x4000) ? 1 : 0);
	DmaPortsStats[_rsso_] = ((dma_en & 0x8000) ? 1 : 0);
	DmaPortsStats[_ufgo_] = ((dma_en & 0x10000) ? 1 : 0);
}

static enum CAM_FrameST Irq_CAM_SttFrameStatus(
					enum ISP_DEV_NODE_ENUM module,
					enum ISP_IRQ_TYPE_ENUM irq_mod,
					unsigned int dma_id,
					unsigned int delayCheck)
{
	static const int dma_arry_map[_cam_max_] = {
		-1, /* _imgo_*/
		-1, /* _rrzo_ */
		-1, /* _ufeo_ */
		 0, /* _aao_ */
		 1, /* _afo_ */
		-1, /* _lcso_ */
		 2, /* _pdo_ */
		-1, /* _lmvo_ */
		 3, /* _flko_ */
		-1, /* _rsso_ */
		 4,  /* _pso_ */
		-1 /* _ufgo_ */
	};

	unsigned int     dma_en;
	union FBC_CTRL_1  fbc_ctrl1;
	union FBC_CTRL_2  fbc_ctrl2;
	bool       bQueMode = MFALSE;
	unsigned int     product = 1;
	/* TSTP_V3 unsigned int     frmPeriod = 1; */

	switch (module) {
	case ISP_CAM_A_IDX:
	case ISP_CAM_B_IDX:
	case ISP_CAM_C_IDX:
		if (dma_id >= _cam_max_) {
			LOG_NOTICE(
			    "LINE_%d ERROR: unsupported module:0x%x dma:%d\n",
			    __LINE__, module, dma_id);
			return CAM_FST_DROP_FRAME;
		}
		if (dma_arry_map[dma_id] < 0) {
			LOG_NOTICE(
			    "LINE_%d ERROR: unsupported module:0x%x dma:%d\n",
			    __LINE__, module, dma_id);
			return CAM_FST_DROP_FRAME;
		}
		break;
	default:
		LOG_NOTICE(
		    "LINE_%d ERROR: unsupported module:0x%x dma:%d\n",
		    __LINE__, module, dma_id);
		return CAM_FST_DROP_FRAME;
	}

	fbc_ctrl1.Raw = 0x0;
	fbc_ctrl2.Raw = 0x0;

	if (sec_on)
		dma_en = lock_reg.CAM_REG_CTL_DMA_EN[module];
	else
		dma_en = ISP_RD32(CAM_REG_CTL_DMA_EN(module));

	if (_aao_ == dma_id) {
		if (dma_en & 0x20) {
			fbc_ctrl1.Raw = ISP_RD32(CAM_REG_FBC_AAO_CTL1(module));
			fbc_ctrl2.Raw = ISP_RD32(CAM_REG_FBC_AAO_CTL2(module));
		}
	}

	if (_afo_ == dma_id) {
		if (dma_en & 0x8) {
			fbc_ctrl1.Raw = ISP_RD32(CAM_REG_FBC_AFO_CTL1(module));
			fbc_ctrl2.Raw = ISP_RD32(CAM_REG_FBC_AFO_CTL2(module));
		}
	}

	if (_pdo_ == dma_id) {
		if (dma_en & 0x400) {
			fbc_ctrl1.Raw = ISP_RD32(CAM_REG_FBC_PDO_CTL1(module));
			fbc_ctrl2.Raw = ISP_RD32(CAM_REG_FBC_PDO_CTL2(module));
		}
	}

	if (_flko_ == dma_id) {
		if (dma_en & 0x2000) {
			fbc_ctrl1.Raw = ISP_RD32(CAM_REG_FBC_FLKO_CTL1(module));
			fbc_ctrl2.Raw = ISP_RD32(CAM_REG_FBC_FLKO_CTL2(module));
		}
	}

	if (_pso_ == dma_id) {
		if (dma_en & 0x40) {
			fbc_ctrl1.Raw = ISP_RD32(CAM_REG_FBC_PSO_CTL1(module));
			fbc_ctrl2.Raw = ISP_RD32(CAM_REG_FBC_PSO_CTL2(module));
		}
	}

	bQueMode = fbc_ctrl1.Bits.FBC_MODE;

	if (bQueMode) {
		if (fbc_ctrl1.Raw != 0) {
#if 0 /* TSTP_V3 (TIMESTAMP_QUEUE_EN == 1) */
			if (delayCheck == 0) {
				if (fbc_ctrl2.Bits.DROP_CNT !=
					IspInfo.TstpQInfo[irq_mod]
						.Dmao[dma_id].PrevFbcDropCnt) {
					IspInfo.TstpQInfo[irq_mod]
						.Dmao[dma_id].PrevFbcDropCnt =
					((IspInfo.TstpQInfo[irq_mod]
						.Dmao[dma_id].PrevFbcDropCnt +
					frmPeriod) & 0xFF);
					product = 0;
				}
				/* Prevent *0 for SOF ISR delayed after P1_DON*/
				if (fbc_ctrl2.Bits.FBC_CNT == 0)
					product *= 1;
				else
					product *= fbc_ctrl2.Bits.FBC_CNT;
			} else {
				LOG_INF(
				    "cam:%d dma:%d overwrite preveFbcDropCnt %d <= %d\n",
				    irq_mod, dma_id,
				    IspInfo.TstpQInfo[irq_mod]
					.Dmao[dma_id].PrevFbcDropCnt,
				    fbc_ctrl2.Bits.DROP_CNT);

				IspInfo.TstpQInfo[irq_mod]
					.Dmao[dma_id].PrevFbcDropCnt =
				(frmPeriod > 1) ?
				(fbc_ctrl2.Bits.DROP_CNT/frmPeriod)*frmPeriod :
				fbc_ctrl2.Bits.DROP_CNT;

				product *= fbc_ctrl2.Bits.FBC_CNT;
			}
#else
			product *= fbc_ctrl2.Bits.FBC_CNT;

			if (product == 0)
				return CAM_FST_DROP_FRAME;
#endif
		} else
			return CAM_FST_DROP_FRAME;
	} else {
		if (fbc_ctrl1.Raw != 0) {
#if 0 /* TSTP_V3 (TIMESTAMP_QUEUE_EN == 1) */
			if (delayCheck == 0) {
				if (fbc_ctrl2.Bits.DROP_CNT !=
					IspInfo.TstpQInfo[irq_mod]
						.Dmao[dma_id].PrevFbcDropCnt) {
					IspInfo.TstpQInfo[irq_mod]
						.Dmao[dma_id].PrevFbcDropCnt =
					((IspInfo.TstpQInfo[irq_mod]
						.Dmao[dma_id].PrevFbcDropCnt +
					frmPeriod) & 0xFF);
					product = 0;
				}
				/* Prevent *0 for SOF ISR delayed after P1_DON*/
				if ((fbc_ctrl1.Bits.FBC_NUM -
						fbc_ctrl2.Bits.FBC_CNT) == 0)
					product *= 1;
				else
					product *= (fbc_ctrl1.Bits.FBC_NUM -
						fbc_ctrl2.Bits.FBC_CNT);
			} else {
				IspInfo.TstpQInfo[irq_mod]
					.Dmao[dma_id].PrevFbcDropCnt =
				(frmPeriod > 1) ?
				(fbc_ctrl2.Bits.DROP_CNT/frmPeriod)*frmPeriod :
				fbc_ctrl2.Bits.DROP_CNT;

				product *= (fbc_ctrl1.Bits.FBC_NUM -
					fbc_ctrl2.Bits.FBC_CNT);
			}
#else
			product *= (fbc_ctrl1.Bits.FBC_NUM -
						fbc_ctrl2.Bits.FBC_CNT);

			if (product == 0)
				return CAM_FST_DROP_FRAME;
#endif
		} else
			return CAM_FST_DROP_FRAME;
	}

	if (product == 1)
		return CAM_FST_LAST_WORKING_FRAME;
	else
		return CAM_FST_NORMAL;

}

static int32_t ISP_PushBufTimestamp(unsigned int module,
	unsigned int dma_id, unsigned int sec,
	unsigned int usec, unsigned int frmPeriod)
{
	unsigned int wridx = 0;
	union FBC_CTRL_2 fbc_ctrl2;
	enum ISP_DEV_NODE_ENUM reg_module;

	fbc_ctrl2.Raw = 0x0;

	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
		reg_module = ISP_CAM_A_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
		reg_module = ISP_CAM_B_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		reg_module = ISP_CAM_C_IDX;
		break;
	default:
		LOG_NOTICE("Unsupport module:x%x\n", module);
		return -EFAULT;
	}

	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		switch (dma_id) {
		case _imgo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_IMGO_CTL2(reg_module));
			break;
		case _rrzo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_RRZO_CTL2(reg_module));
			break;
		case _ufeo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_UFEO_CTL2(reg_module));
			break;
		case _lmvo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_LMVO_CTL2(reg_module));
			break;
		case _lcso_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_LCSO_CTL2(reg_module));
			break;
		case _rsso_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_RSSO_CTL2(reg_module));
			break;
		case _aao_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_AAO_CTL2(reg_module));
			break;
		case _afo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_AFO_CTL2(reg_module));
			break;
		case _flko_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_FLKO_CTL2(reg_module));
			break;
		case _pdo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_PDO_CTL2(reg_module));
			break;
		case _pso_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_PSO_CTL2(reg_module));
			break;
		case _ufgo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_UFGO_CTL2(reg_module));
			break;
		default:
			LOG_NOTICE("Unsupport dma:x%x\n", dma_id);
			return -EFAULT;
		}
		break;
	default:
		return -EFAULT;
	}

	if (frmPeriod > 1)
		fbc_ctrl2.Bits.WCNT =
			(fbc_ctrl2.Bits.WCNT / frmPeriod) * frmPeriod;

	if (((fbc_ctrl2.Bits.WCNT + frmPeriod) & 63) ==
		IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt) {
		IRQ_LOG_KEEPER(
			module, m_CurrentPPB, _LOG_INF,
			"Cam:%d dma:%d ignore push wcnt_%d_%d\n",
			module, dma_id,
			IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt,
			fbc_ctrl2.Bits.WCNT);
		return 0;
	}

	wridx = IspInfo.TstpQInfo[module].Dmao[dma_id].WrIndex;

	IspInfo.TstpQInfo[module].Dmao[dma_id].TimeQue[wridx].sec = sec;
	IspInfo.TstpQInfo[module].Dmao[dma_id].TimeQue[wridx].usec = usec;

	if (IspInfo.TstpQInfo[module].Dmao[dma_id].WrIndex >=
		(ISP_TIMESTPQ_DEPTH-1))
		IspInfo.TstpQInfo[module].Dmao[dma_id].WrIndex = 0;
	else
		IspInfo.TstpQInfo[module].Dmao[dma_id].WrIndex++;

	IspInfo.TstpQInfo[module].Dmao[dma_id].TotalWrCnt++;

	/* Update WCNT for patch timestamp when SOF ISR missing */
	IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt =
	    ((IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt + 1) & 0x3F);

	return 0;
}

static int32_t ISP_PopBufTimestamp(
	unsigned int module, unsigned int dma_id, struct S_START_T *pTstp)
{
	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		switch (dma_id) {
		case _imgo_:
		case _rrzo_:
		case _ufeo_:
		case _lmvo_:
		case _lcso_:

		case _flko_:
		case _aao_:
		case _afo_:
		case _rsso_:
		case _pdo_:
		case _pso_:
		case _ufgo_:
			break;
		default:
			LOG_NOTICE("Unsupport dma:x%x\n", dma_id);
			return -EFAULT;
		}
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
		switch (dma_id) {
		case _camsv_imgo_:
			break;
		default:
			LOG_NOTICE("Unsupport dma:x%x\n", dma_id);
			return -EFAULT;
		}
		break;
	default:
		LOG_NOTICE("Unsupport module:x%x\n", module);
		return -EFAULT;
	}

	if (pTstp)
		*pTstp = IspInfo.TstpQInfo[module].Dmao[dma_id]
			    .TimeQue[IspInfo.TstpQInfo[module]
			    .Dmao[dma_id].RdIndex];

	if (IspInfo.TstpQInfo[module].Dmao[dma_id].RdIndex >=
		(ISP_TIMESTPQ_DEPTH-1))
		IspInfo.TstpQInfo[module].Dmao[dma_id].RdIndex = 0;
	else
		IspInfo.TstpQInfo[module].Dmao[dma_id].RdIndex++;

	IspInfo.TstpQInfo[module].Dmao[dma_id].TotalRdCnt++;

	return 0;
}


static int32_t ISP_WaitTimestampReady(unsigned int module, unsigned int dma_id)
{
	unsigned int _timeout = 0;
	unsigned int wait_cnt = 0;

	if (IspInfo.TstpQInfo[module].Dmao[dma_id].TotalWrCnt >
		IspInfo.TstpQInfo[module].Dmao[dma_id].TotalRdCnt)
		return 0;

	LOG_INF("Wait module:%d dma:%d timestamp ready W/R:%d/%d\n",
	    module, dma_id,
	    (unsigned int)IspInfo.TstpQInfo[module].Dmao[dma_id].TotalWrCnt,
	    (unsigned int)IspInfo.TstpQInfo[module].Dmao[dma_id].TotalRdCnt);

	for (wait_cnt = 3; wait_cnt > 0; wait_cnt--) {
		_timeout = wait_event_interruptible_timeout(
			    IspInfo.WaitQueueHead[module],
			    (IspInfo.TstpQInfo[module].Dmao[dma_id].TotalWrCnt >
				IspInfo.TstpQInfo[module]
				.Dmao[dma_id].TotalRdCnt),
			    ISP_MsToJiffies(2000));
		/* check if user is interrupted by system signal */
		if ((_timeout != 0) &&
		    (!(IspInfo.TstpQInfo[module].Dmao[dma_id].TotalWrCnt >
		    IspInfo.TstpQInfo[module].Dmao[dma_id].TotalRdCnt))) {
			LOG_INF(
			    "interrupted by system signal, return value(%d)\n",
			    _timeout);
			return -ERESTARTSYS;
		}

		if (_timeout > 0)
			break;

		LOG_INF("WARNING: cam:%d dma:%d wait left count %d\n",
			module, dma_id, wait_cnt);
	}
	if (wait_cnt == 0) {
		LOG_NOTICE("ERROR: cam:%d dma:%d wait timestamp timeout!!!\n",
			module, dma_id);
		return -EFAULT;
	}

	return 0;
}

static int32_t ISP_CompensateMissingSofTime(enum ISP_DEV_NODE_ENUM reg_module,
	unsigned int module, unsigned int dma_id, unsigned int sec,
	unsigned int usec, unsigned int frmPeriod)
{
	union FBC_CTRL_2  fbc_ctrl2;
	unsigned int     delta_wcnt = 0, wridx = 0,
			 wridx_prev1 = 0, wridx_prev2 = 0, i = 0;
	unsigned int     delta_time = 0, max_delta_time = 0;
	struct S_START_T   time_prev1, time_prev2;
	/*To shrink error log, only rrzo print error log*/
	bool dmao_mask = MFALSE;
	/*
	 * Patch timestamp and WCNT base on current HW WCNT and
	 * previous SW WCNT value, and calculate difference
	 */

	fbc_ctrl2.Raw = 0;

	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		switch (dma_id) {
		case _imgo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_IMGO_CTL2(reg_module));
			break;
		case _rrzo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_RRZO_CTL2(reg_module));
			dmao_mask = MTRUE;
			break;
		case _ufeo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_UFEO_CTL2(reg_module));
			break;
		case _lmvo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_LMVO_CTL2(reg_module));
			break;
		case _lcso_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_LCSO_CTL2(reg_module));
			break;
		case _rsso_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_RSSO_CTL2(reg_module));
			break;
		case _aao_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_AAO_CTL2(reg_module));
			break;
		case _afo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_AFO_CTL2(reg_module));
			break;
		case _flko_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_FLKO_CTL2(reg_module));
			break;
		case _pdo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_PDO_CTL2(reg_module));
			break;
		case _pso_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_PSO_CTL2(reg_module));
			break;
		case _ufgo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_UFGO_CTL2(reg_module));
			break;
		default:
			LOG_NOTICE("Unsupport dma:x%x\n", dma_id);
			return -EFAULT;
		}
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
	default:
		LOG_NOTICE("Unsupport module:x%x\n", module);
		return -EFAULT;
	}

	if (frmPeriod > 1)
		fbc_ctrl2.Bits.WCNT =
			(fbc_ctrl2.Bits.WCNT / frmPeriod) * frmPeriod;

	if (((fbc_ctrl2.Bits.WCNT + frmPeriod) & 63) ==
		IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt) {
		if (dmao_mask)
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
			    "Cam:%d dma:%d ignore compensate wcnt_%d_%d\n",
			    module, dma_id,
			    IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt,
			    fbc_ctrl2.Bits.WCNT);
		return 0;
	}

	if (IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt >
		fbc_ctrl2.Bits.WCNT)
		delta_wcnt = fbc_ctrl2.Bits.WCNT + 64 -
			IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt;
	else
		delta_wcnt = fbc_ctrl2.Bits.WCNT -
			IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt;

	if (delta_wcnt > 255) {
		if (dmao_mask)
			LOG_NOTICE(
			    "ERROR: Cam:%d dma:%d WRONG WCNT:%d_%d_%d\n",
			    module, dma_id, delta_wcnt,
			    IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt,
			    fbc_ctrl2.Bits.WCNT);
		return -EFAULT;
	} else if (delta_wcnt > 6) {
		if (dmao_mask)
			LOG_NOTICE(
			    "WARNING: Cam:%d dma:%d SUSPICIOUS WCNT:%d_%d_%d\n",
			    module, dma_id, delta_wcnt,
			    IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt,
			    fbc_ctrl2.Bits.WCNT);
	} else if (delta_wcnt == 0) {
		return 0;
	}

	/* delta_wcnt *= frmPeriod; */

	/* Patch missing SOF timestamp */
	wridx = IspInfo.TstpQInfo[module].Dmao[dma_id].WrIndex;
	wridx_prev1 = (wridx == 0) ? (ISP_TIMESTPQ_DEPTH - 1) : (wridx - 1);
	wridx_prev2 = (wridx_prev1 == 0) ?
			(ISP_TIMESTPQ_DEPTH - 1) : (wridx_prev1 - 1);

	time_prev1.sec = IspInfo.TstpQInfo[module]
				.Dmao[dma_id].TimeQue[wridx_prev1].sec;
	time_prev1.usec = IspInfo.TstpQInfo[module]
				.Dmao[dma_id].TimeQue[wridx_prev1].usec;

	time_prev2.sec = IspInfo.TstpQInfo[module]
				.Dmao[dma_id].TimeQue[wridx_prev2].sec;
	time_prev2.usec = IspInfo.TstpQInfo[module]
				.Dmao[dma_id].TimeQue[wridx_prev2].usec;

	if ((sec > time_prev1.sec) ||
		((sec == time_prev1.sec) && (usec > time_prev1.usec))) {
		max_delta_time = ((sec - time_prev1.sec)*1000000 + usec) -
					time_prev1.usec;
	} else {
		if (dmao_mask)
			LOG_NOTICE(
			    "ERROR: Cam:%d dma:%d current timestamp: cur: %d.%06d prev1: %d.%06d\n",
			    module, dma_id, sec, usec,
			    time_prev1.sec, time_prev1.usec);
		max_delta_time = 0;
	}

	if ((time_prev1.sec > time_prev2.sec) ||
		((time_prev1.sec == time_prev2.sec) &&
		(time_prev1.usec > time_prev2.usec)))
		delta_time = ((time_prev1.sec - time_prev2.sec)*1000000
					+ time_prev1.usec) - time_prev2.usec;
	else {
		if (dmao_mask)
			LOG_NOTICE(
			    "ERROR: Cam:%d dma:%d previous timestamp: prev1: %d.%06d prev2: %d.%06d\n",
			    module, dma_id, time_prev1.sec,
			    time_prev1.usec, time_prev2.sec, time_prev2.usec);
		delta_time = 0;
	}

	if (delta_time > (max_delta_time / delta_wcnt)) {
		if (dmao_mask)
			IRQ_LOG_KEEPER(
			    module, m_CurrentPPB, _LOG_INF,
			    "WARNING: Cam:%d dma:%d delta time too large: cur %dus max %dus patch wcnt: %d\n",
			    module, dma_id, delta_time,
			    max_delta_time, delta_wcnt);
		delta_time = max_delta_time / delta_wcnt;
	}

	for (i = 0; i < delta_wcnt; i++) {
		time_prev1.usec += delta_time;
		while (time_prev1.usec >= 1000000) {
			time_prev1.usec -= 1000000;
			time_prev1.sec++;
		}
		/* WCNT will be increase in this API */
		ISP_PushBufTimestamp(module, dma_id, time_prev1.sec,
			time_prev1.usec, frmPeriod);
	}

	if (dmao_mask)
		IRQ_LOG_KEEPER(
			module, m_CurrentPPB, _LOG_INF,
			"Cam:%d dma:%d wcnt:%d_%d_%d T:%d.%06d_.%06d_%d.%06d\n",
			module, dma_id, delta_wcnt,
			IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt,
			fbc_ctrl2.Bits.WCNT, sec, usec, delta_time,
			time_prev1.sec, time_prev1.usec);

	if (IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt
		!= fbc_ctrl2.Bits.WCNT) {
		if (dmao_mask)
			LOG_NOTICE(
			    "ERROR: Cam:%d dma:%d strange WCNT SW_HW: %d_%d\n",
			    module,
			    dma_id,
			    IspInfo.TstpQInfo[module]
				.Dmao[dma_id].PrevFbcWCnt,
			    fbc_ctrl2.Bits.WCNT);
		IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt =
			fbc_ctrl2.Bits.WCNT;
	}

	return 0;
}

#if (TSTMP_SUBSAMPLE_INTPL == 1)
static int32_t ISP_PatchTimestamp(unsigned int module, unsigned int dma_id,
	unsigned int frmPeriod, unsigned long long refTimestp,
	unsigned long long prevTimestp)
{
	unsigned long long prev_tstp = prevTimestp, cur_tstp = refTimestp;
	unsigned int target_wridx = 0, curr_wridx = 0,
		     frm_dt = 0, last_frm_dt = 0, i = 1;

	/* Only sub-sample case needs patch */
	if (frmPeriod <= 1)
		return 0;

	curr_wridx = IspInfo.TstpQInfo[module].Dmao[dma_id].WrIndex;

	if (curr_wridx < frmPeriod)
		target_wridx = (curr_wridx + ISP_TIMESTPQ_DEPTH - frmPeriod);
	else
		target_wridx = curr_wridx - frmPeriod;

	frm_dt = (((unsigned int)(cur_tstp - prev_tstp)) / frmPeriod);
	last_frm_dt = ((cur_tstp - prev_tstp) - frm_dt*(frmPeriod-1));

	if (frm_dt == 0)
		LOG_INF("WARNING: timestamp delta too small: %d\n",
			(int)(cur_tstp - prev_tstp));

	i = 0;
	while (target_wridx != curr_wridx) {

		if (i > frmPeriod) {
			LOG_NOTICE(
			    "Error: too many intpl in sub-sample period %d_%d\n",
			    target_wridx, curr_wridx);
			return -EFAULT;
		}

		IspInfo.TstpQInfo[module].Dmao[dma_id]
			.TimeQue[target_wridx].usec += (frm_dt * i);

		while (IspInfo.TstpQInfo[module].Dmao[dma_id]
			.TimeQue[target_wridx].usec >= 1000000) {

			IspInfo.TstpQInfo[module].Dmao[dma_id]
				.TimeQue[target_wridx].usec -= 1000000;
			IspInfo.TstpQInfo[module].Dmao[dma_id]
				.TimeQue[target_wridx].sec++;
		}

		i++;
		target_wridx++; /* patch from 2nd time */
		if (target_wridx >= ISP_TIMESTPQ_DEPTH)
			target_wridx = 0;
	}

	return 0;
}
#endif

#endif

irqreturn_t ISP_Irq_CAMSV_0(int  Irq, void *DeviceId)
{
	return ISP_Irq_CAMSV(
		ISP_IRQ_TYPE_INT_CAMSV_0_ST, ISP_CAMSV0_IDX, "CAMSV0");
}

irqreturn_t ISP_Irq_CAMSV_1(int  Irq, void *DeviceId)
{
	return ISP_Irq_CAMSV(
		ISP_IRQ_TYPE_INT_CAMSV_1_ST, ISP_CAMSV1_IDX, "CAMSV1");
}

irqreturn_t ISP_Irq_CAMSV_2(int  Irq, void *DeviceId)
{
	return ISP_Irq_CAMSV(
		ISP_IRQ_TYPE_INT_CAMSV_2_ST, ISP_CAMSV2_IDX, "CAMSV2");
}

irqreturn_t ISP_Irq_CAMSV_3(int  Irq, void *DeviceId)
{
	return ISP_Irq_CAMSV(
		ISP_IRQ_TYPE_INT_CAMSV_3_ST, ISP_CAMSV3_IDX, "CAMSV3");
}

irqreturn_t ISP_Irq_CAMSV_4(int  Irq, void *DeviceId)
{
	return ISP_Irq_CAMSV(
		ISP_IRQ_TYPE_INT_CAMSV_4_ST, ISP_CAMSV4_IDX, "CAMSV4");
}

irqreturn_t ISP_Irq_CAMSV_5(int  Irq, void *DeviceId)
{
	return ISP_Irq_CAMSV(
		ISP_IRQ_TYPE_INT_CAMSV_5_ST, ISP_CAMSV5_IDX, "CAMSV5");
}

irqreturn_t ISP_Irq_CAMSV(
	enum ISP_IRQ_TYPE_ENUM irq_module,
	enum ISP_DEV_NODE_ENUM cam_idx, const char *str)
{
	unsigned int module = irq_module;
	unsigned int reg_module = cam_idx;
	unsigned int i, IrqStatus, ErrStatus, time_stamp, cur_v_cnt = 0;
	unsigned int IrqEnableOrig, IrqEnableNew;

	union FBC_CTRL_1 fbc_ctrl1[2];
	/* */
	union FBC_CTRL_2 fbc_ctrl2[2];

	struct timeval time_frmb;
	unsigned long long  sec = 0;
	unsigned long       usec = 0;

	/* Avoid touch hwmodule when clock is disable.
	 * DEVAPC will moniter this kind of err
	 */
	if (G_u4EnableClockCount == 0)
		return IRQ_HANDLED;

	/*  */
	sec = cpu_clock(0);     /* ns */
	do_div(sec, 1000);    /* usec */
	usec = do_div(sec, 1000000);    /* sec and usec */
	time_frmb.tv_usec = usec;
	time_frmb.tv_sec = sec;

	#if (ISP_BOTTOMHALF_WORKQ == 1)
	gSvLog[module]._lastIrqTime.sec = sec;
	gSvLog[module]._lastIrqTime.usec = usec;
	#endif

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	IrqStatus = ISP_RD32(CAMSV_REG_INT_STATUS(reg_module));
	spin_unlock(&(IspInfo.SpinLockIrq[module]));

	ErrStatus = IrqStatus & IspInfo.IrqInfo.ErrMask[module][SIGNAL_INT];
	IrqStatus = IrqStatus & IspInfo.IrqInfo.Mask[module][SIGNAL_INT];

	/* Check ERR/WRN ISR times, if it occur too frequently,
	 * mark it for avoding keep enter ISR
	 * It will happen KE
	 */
	for (i = 0; i < ISP_ISR_MAX_NUM; i++) {
		/* Only check irq that un marked yet */
		if (!(IspInfo.IrqCntInfo.m_err_int_mark[module] & (1 << i))) {

			if (ErrStatus & (1 << i))
				IspInfo.IrqCntInfo.m_err_int_cnt[module][i]++;

			if (usec - IspInfo.IrqCntInfo.m_int_usec[module] <
				INT_ERR_WARN_TIMER_THREAS) {
				if (IspInfo.IrqCntInfo.m_err_int_cnt[
					module][i] >= INT_ERR_WARN_MAX_TIME)
					IspInfo.IrqCntInfo.m_err_int_mark[
						module] |= (1 << i);

			} else {
				IspInfo.IrqCntInfo.m_int_usec[module] = usec;
				IspInfo.IrqCntInfo.m_err_int_cnt[module][i] = 0;
			}
		}

	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	IrqEnableOrig = ISP_RD32(CAMSV_REG_INT_EN(reg_module));
	spin_unlock(&(IspInfo.SpinLockIrq[module]));

	IrqEnableNew = IrqEnableOrig &
		~(IspInfo.IrqCntInfo.m_err_int_mark[module]);
	ISP_WR32(CAMSV_REG_INT_EN(reg_module), IrqEnableNew);
	/*  */
	IRQ_INT_ERR_CHECK_CAM(0, ErrStatus, 0, module);

	fbc_ctrl1[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL1(reg_module));
	fbc_ctrl2[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL2(reg_module));
	time_stamp       = ISP_RD32(CAMSV_REG_TG_TIME_STAMP(reg_module));

	/* sof , done order chech . */
	if ((IrqStatus & SV_HW_PASS1_DON_ST) || (IrqStatus & SV_SOF_INT_ST))
		/*cur_v_cnt = ((ISP_RD32(CAMSV_REG_TG_INTER_ST(reg_module)) */
		/*		& 0x00FF0000) >> 16);*/
		cur_v_cnt = ISP_RD32_TG_CAM_FRM_CNT(module, reg_module);

	if ((IrqStatus & SV_HW_PASS1_DON_ST) && (IrqStatus & SV_SOF_INT_ST)) {
		if (cur_v_cnt != sof_count[module])
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"isp sof_don block, %d_%d\n",
				cur_v_cnt, sof_count[module]);
	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	if (IrqStatus & SV_SW_PASS1_DON_ST) {
		sec = cpu_clock(0);     /* ns */
		do_div(sec, 1000);    /* usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		/* update pass1 done time stamp for eis user
		 *(need match with the time stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][10] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][10] =
			(unsigned int)(sec);

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"%s P1_DON_%d(0x%08x_0x%08x) stamp[0x%08x]\n",
				str,
				(sof_count[module]) ?
				(sof_count[module] - 1) : (sof_count[module]),
				(unsigned int)(fbc_ctrl1[0].Raw),
				(unsigned int)(fbc_ctrl2[0].Raw),
				time_stamp);
		}

		/* for dbg log only */
		if (pstRTBuf[module]->ring_buf[_camsv_imgo_].active)
			pstRTBuf[module]->ring_buf[_camsv_imgo_].img_cnt =
				sof_count[module];

	}

	if (IrqStatus & SV_SOF_INT_ST) {
		sec = ktime_get();
		do_div(sec, 1000);    /* usec */
		usec = do_div(sec, 1000000);    /* sec and usec */

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			static unsigned int m_sec = 0, m_usec;

			if (g1stSof[module]) {
				m_sec = sec;
				m_usec = usec;
				gSTime[module].sec = sec;
				gSTime[module].usec = usec;
			}

			IRQ_LOG_KEEPER(
				module, m_CurrentPPB, _LOG_INF,
				"%s P1_SOF_%d_%d(0x%08x_0x%08x,0x%08x),int_us:0x%08x, stamp[0x%08x]\n",
				str,
				sof_count[module], cur_v_cnt,
				(unsigned int)(ISP_RD32(
				    CAMSV_REG_FBC_IMGO_CTL1(reg_module))),
				(unsigned int)(ISP_RD32(
				    CAMSV_REG_FBC_IMGO_CTL2(reg_module))),
				ISP_RD32(CAMSV_REG_IMGO_BASE_ADDR(reg_module)),
				(unsigned int)((sec * 1000000 + usec) -
				    (1000000 * m_sec + m_usec)),
				time_stamp);
			/* keep current time */
			m_sec = sec;
			m_usec = usec;

			/* dbg information only */
			if (cur_v_cnt != ((ISP_RD32(
				CAMSV_REG_TG_INTER_ST(reg_module))
				& 0x00FF0000) >> 16))
				IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"SW ISR right on next hw p1_done\n");

		}

		/* update SOF time stamp for eis user(need match with the time
		 * stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][12] =
			(unsigned int)(sec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][12] =
			(unsigned int)(usec);

		/* sw sof counter */
		sof_count[module]++;
		/* for match vsync cnt */
		if (sof_count[module] > 255)
			sof_count[module] -= 256;

		g1stSof[module] = MFALSE;
	}

	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		/* 1. update interrupt status to all users */
		IspInfo.IrqInfo.Status[module][SIGNAL_INT][i] |= IrqStatus;

		/* 2. update signal time and passed by signal count */
		if (IspInfo.IrqInfo.MarkedFlag[module][SIGNAL_INT][i] &
			IspInfo.IrqInfo.Mask[module][SIGNAL_INT]) {
			unsigned int cnt = 0, tmp = IrqStatus;

			while (tmp) {
				if (tmp & 0x1) {
					IspInfo.IrqInfo
					    .LastestSigTime_usec[module][cnt] =
					    (unsigned int)time_frmb.tv_usec;
					IspInfo.IrqInfo
					    .LastestSigTime_sec[module][cnt] =
					    (unsigned int) time_frmb.tv_sec;
					IspInfo.IrqInfo
					    .PassedBySigCnt[module][cnt][i]++;
				}
				tmp = tmp >> 1;
				cnt++;
			}
		} else {
		/* no any interrupt is not marked and in
		 * read mask in this irq type
		 */
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[module]));
	/*  */
	wake_up_interruptible(&IspInfo.WaitQueueHead[module]);

	/* dump log, use tasklet */
	if (IrqStatus &
		(SV_SOF_INT_ST | SV_SW_PASS1_DON_ST | SV_VS1_ST)) {
		#if (ISP_BOTTOMHALF_WORKQ == 1)
		schedule_work(&isp_workque[module].isp_bh_work);
		#else
		tasklet_schedule(isp_tasklet[module].pIsp_tkt);
		#endif
	}

	return IRQ_HANDLED;
}

irqreturn_t ISP_Irq_CAM_A(int Irq, void *DeviceId)
{
	return ISP_Irq_CAM(ISP_IRQ_TYPE_INT_CAM_A_ST);
}

irqreturn_t ISP_Irq_CAM_B(int  Irq, void *DeviceId)
{
	return ISP_Irq_CAM(ISP_IRQ_TYPE_INT_CAM_B_ST);
}

irqreturn_t ISP_Irq_CAM_C(int  Irq, void *DeviceId)
{
	return ISP_Irq_CAM(ISP_IRQ_TYPE_INT_CAM_C_ST);
}

irqreturn_t ISP_Irq_CAM(enum ISP_IRQ_TYPE_ENUM irq_module)
{
	unsigned int module = irq_module;
	unsigned int reg_module = ISP_CAM_A_IDX;
	unsigned int i, cardinalNum = 0, IrqStatus, ErrStatus, WarnStatus,
			DmaStatus, WarnStatus_2, cur_v_cnt = 0;

	union FBC_CTRL_1 fbc_ctrl1[2];
	union FBC_CTRL_2 fbc_ctrl2[2];

	struct timeval time_frmb;
	unsigned long long  sec = 0;
	unsigned long       usec = 0;
	unsigned int IrqEnableOrig, IrqEnableNew;

	/* Avoid touch hwmodule when clock is disable.
	 * DEVAPC will moniter this kind of err
	 */
	if (G_u4EnableClockCount == 0)
		return IRQ_HANDLED;

	/*	*/
	sec = cpu_clock(0);  /* ns */
	do_div(sec, 1000);    /* usec */
	usec = do_div(sec, 1000000);    /* sec and usec */
	time_frmb.tv_usec = usec;
	time_frmb.tv_sec = sec;

	#if (ISP_BOTTOMHALF_WORKQ == 1)
	gSvLog[module]._lastIrqTime.sec = sec;
	gSvLog[module]._lastIrqTime.usec = usec;
	#endif

	switch (irq_module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
		reg_module = ISP_CAM_A_IDX;
		cardinalNum = 0;
		break;
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
		reg_module = ISP_CAM_B_IDX;
		cardinalNum = 1;
		break;
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		reg_module = ISP_CAM_C_IDX;
		cardinalNum = 2;
		break;
	default:
		LOG_NOTICE("Wrong IRQ module: %d", (unsigned int)module);
		return IRQ_HANDLED;
	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	IrqStatus = ISP_RD32(CAM_REG_CTL_RAW_INT_STATUS(reg_module));
	DmaStatus = ISP_RD32(CAM_REG_CTL_RAW_INT2_STATUS(reg_module));
	WarnStatus_2 = ISP_RD32(CAM_REG_CTL_RAW_INT3_STATUS(reg_module));
	spin_unlock(&(IspInfo.SpinLockIrq[module]));

	ErrStatus = IrqStatus & IspInfo.IrqInfo.ErrMask[module][SIGNAL_INT];
	WarnStatus = IrqStatus & IspInfo.IrqInfo.WarnMask[module][SIGNAL_INT];
	WarnStatus_2 = WarnStatus_2 &
		IspInfo.IrqInfo.Warn2Mask[module][SIGNAL_INT];
	IrqStatus = IrqStatus & IspInfo.IrqInfo.Mask[module][SIGNAL_INT];

	/* Check ERR/WRN ISR times, if it occur too frequently,
	 * mark it for avoding keep enter ISR
	 * It will happen KE
	 */
	for (i = 0; i < ISP_ISR_MAX_NUM; i++) {
		/* Only check irq that un marked yet */
		if (!((IspInfo.IrqCntInfo.m_err_int_mark[module] & (1 << i)) ||
		    (IspInfo.IrqCntInfo.m_warn_int_mark[module] & (1 << i)))) {

			if (ErrStatus & (1 << i))
				IspInfo.IrqCntInfo.m_err_int_cnt[module][i]++;

			if (WarnStatus & (1 << i))
				IspInfo.IrqCntInfo.m_warn_int_cnt[module][i]++;


			if (usec - IspInfo.IrqCntInfo.m_int_usec[module] <
				INT_ERR_WARN_TIMER_THREAS) {
				if (IspInfo.IrqCntInfo
				    .m_err_int_cnt[module][i] >=
				    INT_ERR_WARN_MAX_TIME)
					IspInfo.IrqCntInfo
					  .m_err_int_mark[module] |= (1 << i);

				if (IspInfo.IrqCntInfo
				    .m_warn_int_cnt[module][i] >=
				    INT_ERR_WARN_MAX_TIME)
					IspInfo.IrqCntInfo
					  .m_warn_int_mark[module] |= (1 << i);

			} else {
				IspInfo.IrqCntInfo.m_int_usec[module] = usec;
				IspInfo.IrqCntInfo.m_err_int_cnt[
							module][i] = 0;
				IspInfo.IrqCntInfo.m_warn_int_cnt[
							module][i] = 0;
			}
		}

	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	IrqEnableOrig = ISP_RD32(CAM_REG_CTL_RAW_INT_EN(reg_module));
	spin_unlock(&(IspInfo.SpinLockIrq[module]));

	IrqEnableNew =
		IrqEnableOrig & ~(IspInfo.IrqCntInfo.m_err_int_mark[module]
		| IspInfo.IrqCntInfo.m_warn_int_mark[module]);
	ISP_WR32(CAM_REG_CTL_RAW_INT_EN(reg_module), IrqEnableNew);

	/*	*/
	IRQ_INT_ERR_CHECK_CAM(WarnStatus, ErrStatus, WarnStatus_2, module);

	fbc_ctrl1[0].Raw = ISP_RD32(CAM_REG_FBC_IMGO_CTL1(reg_module));
	fbc_ctrl1[1].Raw = ISP_RD32(CAM_REG_FBC_RRZO_CTL1(reg_module));
	fbc_ctrl2[0].Raw = ISP_RD32(CAM_REG_FBC_IMGO_CTL2(reg_module));
	fbc_ctrl2[1].Raw = ISP_RD32(CAM_REG_FBC_RRZO_CTL2(reg_module));

	#if defined(ISP_MET_READY)
	if (trace_ISP__Pass1_CAM_enter_enabled()) {
		/*MET:ISP EOF*/
		if (IrqStatus & HW_PASS1_DON_ST)
			CAMSYS_MET_Events_Trace(0, reg_module, irq_module);

		if (IrqStatus & SOF_INT_ST)
			CAMSYS_MET_Events_Trace(1, reg_module, irq_module);
	}
	#endif

	/* sof , done order chech . */
	if ((IrqStatus & HW_PASS1_DON_ST) || (IrqStatus & SOF_INT_ST))
		/*cur_v_cnt = ((ISP_RD32( */
		/*	CAM_REG_TG_INTER_ST(reg_module)) & 0x00FF0000) >> 16);*/
		cur_v_cnt = ISP_RD32_TG_CAM_FRM_CNT(module, reg_module);

	if ((IrqStatus & HW_PASS1_DON_ST) && (IrqStatus & SOF_INT_ST)) {
		if (cur_v_cnt != sof_count[module])
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
			   "isp sof_don block, %d_%d\n",
			   cur_v_cnt, sof_count[module]);
	}

	if ((IrqStatus & HW_PASS1_DON_ST) &&
		(IspInfo.DebugMask & ISP_DBG_HW_DON)) {
		IRQ_LOG_KEEPER(
			module, m_CurrentPPB, _LOG_INF,
			"CAM%c P1_HW_DON_%d\n",
			'A'+cardinalNum,
			(sof_count[module]) ?
			(sof_count[module] - 1) : (sof_count[module]));
	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	if (IrqStatus & VS_INT_ST) {
		Vsync_cnt[cardinalNum]++;
		/*LOG_INF("CAMA N3D:0x%x\n", Vsync_cnt[0]);*/
	}
	if (IrqStatus & SW_PASS1_DON_ST) {
		sec = ktime_get(); /* ns */
		do_div(sec, 1000);	  /* usec */
		usec = do_div(sec, 1000000);	/* sec and usec */
		/* update pass1 done time stamp for eis user
		 * (need match with the time stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][10] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][10] =
			(unsigned int)(sec);

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			/*SW p1_don is not reliable*/
			if (FrameStatus[module] != CAM_FST_DROP_FRAME) {
				IRQ_LOG_KEEPER(
				  module, m_CurrentPPB, _LOG_INF,
				  "CAM%c P1_DON_%d(0x%x_0x%x,0x%x_0x%x)\n",
				  'A'+cardinalNum,
				  (sof_count[module]) ?
				  (sof_count[module] - 1) : (sof_count[module]),
				  (unsigned int)(fbc_ctrl1[0].Raw),
				  (unsigned int)(fbc_ctrl2[0].Raw),
				  (unsigned int)(fbc_ctrl1[1].Raw),
				  (unsigned int)(fbc_ctrl2[1].Raw));
			}
		}

		#if (TSTMP_SUBSAMPLE_INTPL == 1)
		if (g1stSwP1Done[module] == MTRUE) {
			unsigned long long cur_timestp =
				(unsigned long long)sec*1000000 + usec;
			unsigned int frmPeriod =
				((ISP_RD32(CAM_REG_TG_SUB_PERIOD(
				reg_module)) >> 8) & 0x1F) + 1;

			if (frmPeriod > 1) {
				ISP_PatchTimestamp(module, _imgo_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _rrzo_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _ufeo_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _lmvo_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _lcso_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _ufgo_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
			}

			g1stSwP1Done[module] = MFALSE;
		}
		#endif

		/* for dbg log only */
		if (pstRTBuf[module]->ring_buf[_imgo_].active)
			pstRTBuf[module]->ring_buf[_imgo_].img_cnt =
				sof_count[module];

		if (pstRTBuf[module]->ring_buf[_rrzo_].active)
			pstRTBuf[module]->ring_buf[_rrzo_].img_cnt =
				sof_count[module];

		if (pstRTBuf[module]->ring_buf[_ufeo_].active)
			pstRTBuf[module]->ring_buf[_ufeo_].img_cnt =
				sof_count[module];

		if (pstRTBuf[module]->ring_buf[_ufgo_].active)
			pstRTBuf[module]->ring_buf[_ufgo_].img_cnt =
				sof_count[module];
	}

	if (IrqStatus & SOF_INT_ST) {
		unsigned int frmPeriod = ((ISP_RD32(
			CAM_REG_TG_SUB_PERIOD(reg_module)) >> 8) & 0x1F) + 1;
		unsigned int irqDelay = 0;

		sec = ktime_get(); /* ns */
		do_div(sec, 1000);    /* usec */
		usec = do_div(sec, 1000000);    /* sec and usec */

		if (frmPeriod == 0) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
				"ERROR: Wrong sub-sample period: 0");
			goto LB_CAM_SOF_IGNORE;
		}

		/* chk this frame have EOF or not, dynimic dma port chk */
		FrameStatus[module] = Irq_CAM_FrameStatus(
					reg_module, module, irqDelay);

		if (FrameStatus[module] == CAM_FST_DROP_FRAME) {
			IRQ_LOG_KEEPER(
				module, m_CurrentPPB, _LOG_INF,
				"CAM%c Lost p1 done_%d (0x%x): ",
				'A'+cardinalNum, sof_count[module],
				cur_v_cnt);
		}

		/* During SOF, re-enable that err/warn irq had been marked and
		 * reset IrqCntInfo
		 */
		IrqEnableNew = ISP_RD32(CAM_REG_CTL_RAW_INT_EN(reg_module));
		IrqEnableNew |= (IspInfo.IrqCntInfo.m_err_int_mark[module] |
			IspInfo.IrqCntInfo.m_warn_int_mark[module]);
		ISP_WR32(CAM_REG_CTL_RAW_INT_EN(reg_module), IrqEnableNew);

		IspInfo.IrqCntInfo.m_err_int_mark[module] = 0;
		IspInfo.IrqCntInfo.m_warn_int_mark[module] = 0;
		IspInfo.IrqCntInfo.m_int_usec[module] = 0;

		for (i = 0; i < ISP_ISR_MAX_NUM; i++) {
			IspInfo.IrqCntInfo.m_err_int_cnt[module][i] = 0;
			IspInfo.IrqCntInfo.m_warn_int_cnt[module][i] = 0;
		}

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			static ktime_t m_sec = 0, m_usec;
			unsigned int magic_num;

			if (pstRTBuf[module]->ring_buf[_imgo_].active)
				magic_num = ISP_RD32(
					CAM_REG_IMGO_FH_SPARE_2(reg_module));
			else
				magic_num = ISP_RD32(
					CAM_REG_RRZO_FH_SPARE_2(reg_module));

			if (g1stSof[module]) {
				m_sec = sec;
				m_usec = usec;
				gSTime[module].sec = sec;
				gSTime[module].usec = usec;
			}

			#if (TIMESTAMP_QUEUE_EN == 1)
			{
			unsigned long long cur_timestp =
				(unsigned long long)sec*1000000 + usec;
			unsigned int subFrm = 0;
			enum CAM_FrameST FrmStat_aao, FrmStat_afo,
					 FrmStat_flko, FrmStat_pdo;
			enum CAM_FrameST FrmStat_pso;

			ISP_GetDmaPortsStatus(
					reg_module,
					IspInfo.TstpQInfo[module].DmaEnStatus);

			/* Prevent WCNT increase after
			 * ISP_CompensateMissingSofTime around P1_DON
			 * and FBC_CNT decrease to 0, following drop frame
			 * is checked becomes true, then SOF timestamp will
			 * missing for current frame
			 */
			if (IspInfo.TstpQInfo[module].DmaEnStatus[_aao_])
				FrmStat_aao = Irq_CAM_SttFrameStatus(
					reg_module, module,
					_aao_, irqDelay);
			else
				FrmStat_aao = CAM_FST_DROP_FRAME;

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_afo_])
				FrmStat_afo = Irq_CAM_SttFrameStatus(
					reg_module, module,
					_afo_, irqDelay);
			else
				FrmStat_afo = CAM_FST_DROP_FRAME;

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_flko_])
				FrmStat_flko = Irq_CAM_SttFrameStatus(
					reg_module, module,
					_flko_, irqDelay);
			else
				FrmStat_flko = CAM_FST_DROP_FRAME;

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pdo_])
				FrmStat_pdo = Irq_CAM_SttFrameStatus(
					reg_module, module,
					_pdo_, irqDelay);
			else
				FrmStat_pdo = CAM_FST_DROP_FRAME;
			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pso_])
				FrmStat_pso = Irq_CAM_SttFrameStatus(
					reg_module, module,
					_pso_, irqDelay);
			else
				FrmStat_pso = CAM_FST_DROP_FRAME;

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_imgo_])
				ISP_CompensateMissingSofTime(
					reg_module,
					module, _imgo_,
					sec, usec,
					frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_rrzo_])
				ISP_CompensateMissingSofTime(
					reg_module,
					module, _rrzo_,
					sec, usec,
					frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_ufeo_])
				ISP_CompensateMissingSofTime(
					reg_module,
					module, _ufeo_,
					sec, usec,
					frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_ufgo_])
				ISP_CompensateMissingSofTime(
					reg_module,
					module, _ufgo_,
					sec, usec,
					frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_lmvo_])
				ISP_CompensateMissingSofTime(
					reg_module,
					module, _lmvo_,
					sec, usec,
					frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_lcso_])
				ISP_CompensateMissingSofTime(
					reg_module,
					module, _lcso_,
					sec, usec,
					frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_aao_])
				ISP_CompensateMissingSofTime(
					reg_module,
					module, _aao_,
					sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_afo_])
				ISP_CompensateMissingSofTime(
					reg_module,
					module, _afo_,
					sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_flko_])
				ISP_CompensateMissingSofTime(
					reg_module,
					module, _flko_,
					sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pdo_])
				ISP_CompensateMissingSofTime(
					reg_module,
					module, _pdo_,
					sec, usec, 1);
			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pso_])
				ISP_CompensateMissingSofTime(
					reg_module,
					module, _pso_,
					sec, usec, 1);

			if (FrameStatus[module] != CAM_FST_DROP_FRAME) {
				for (subFrm = 0; subFrm < frmPeriod; subFrm++) {
					/* Current frame is NOT DROP FRAME */
					if (IspInfo.TstpQInfo[
					    module].DmaEnStatus[_imgo_])
						ISP_PushBufTimestamp(
						    module, _imgo_,
						    sec, usec,
						    frmPeriod);

					if (IspInfo.TstpQInfo[
					    module].DmaEnStatus[_rrzo_])
						ISP_PushBufTimestamp(
						    module, _rrzo_,
						    sec, usec,
						    frmPeriod);

					if (IspInfo.TstpQInfo[
					    module].DmaEnStatus[_ufeo_])
						ISP_PushBufTimestamp(
						    module, _ufeo_,
						    sec, usec,
						    frmPeriod);

					if (IspInfo.TstpQInfo[
					    module].DmaEnStatus[_ufgo_])
						ISP_PushBufTimestamp(
						    module, _ufgo_,
						    sec, usec,
						    frmPeriod);

					if (IspInfo.TstpQInfo[
					    module].DmaEnStatus[_lmvo_])
						ISP_PushBufTimestamp(
						    module, _lmvo_,
						    sec, usec,
						    frmPeriod);

					if (IspInfo.TstpQInfo[
					    module].DmaEnStatus[_lcso_])
						ISP_PushBufTimestamp(
						    module, _lcso_,
						    sec, usec,
						    frmPeriod);
				}

				/* for slow motion sub-sample */
				/* must after current ISP_PushBufTimestamp() */
				#if (TSTMP_SUBSAMPLE_INTPL == 1)
				if ((frmPeriod > 1) &&
				    (g1stSwP1Done[module] == MFALSE)) {
					if (IspInfo.TstpQInfo[
					    module].DmaEnStatus[_imgo_])
						ISP_PatchTimestamp(
						    module, _imgo_,
						    frmPeriod,
						    cur_timestp,
						    gPrevSofTimestp[module]);

					if (IspInfo.TstpQInfo[
					    module].DmaEnStatus[_rrzo_])
						ISP_PatchTimestamp(
						    module, _rrzo_,
						    frmPeriod,
						    cur_timestp,
						    gPrevSofTimestp[module]);

					if (IspInfo.TstpQInfo[
					    module].DmaEnStatus[_ufeo_])
						ISP_PatchTimestamp(
						    module, _ufeo_,
						    frmPeriod,
						    cur_timestp,
						    gPrevSofTimestp[module]);

					if (IspInfo.TstpQInfo[
					    module].DmaEnStatus[_ufgo_])
						ISP_PatchTimestamp(
						    module, _ufgo_,
						    frmPeriod,
						    cur_timestp,
						    gPrevSofTimestp[module]);

					if (IspInfo.TstpQInfo[
					    module].DmaEnStatus[_lmvo_])
						ISP_PatchTimestamp(
						    module, _lmvo_,
						    frmPeriod,
						    cur_timestp,
						    gPrevSofTimestp[module]);

					if (IspInfo.TstpQInfo[
					    module].DmaEnStatus[_lcso_])
						ISP_PatchTimestamp(
						    module, _lcso_,
						    frmPeriod,
						    cur_timestp,
						    gPrevSofTimestp[module]);
				}
				#endif
			}

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_aao_])
				if (FrmStat_aao != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp(
					    module, _aao_,
					    sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_afo_])
				if (FrmStat_afo != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp(
					    module, _afo_,
					    sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_flko_])
				if (FrmStat_flko != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp(
					    module, _flko_,
					    sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pdo_])
				if (FrmStat_pdo != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp(
					    module, _pdo_,
					    sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pso_])
				if (FrmStat_pso != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp(
					    module, _pso_,
					    sec, usec, 1);
			#if (TSTMP_SUBSAMPLE_INTPL == 1)
			gPrevSofTimestp[module] = cur_timestp;
			#endif

			}
			#endif /* (TIMESTAMP_QUEUE_EN == 1) */

			IRQ_LOG_KEEPER(
			    module, m_CurrentPPB, _LOG_INF,
			    "CAM%c P1_SOF_%d_%d(0x%x_0x%x,0x%x_0x%x,0x%x,0x%x,0x%x),int_us:%d,cq:0x%x\n",
			    'A'+cardinalNum, sof_count[module], cur_v_cnt,
			    (unsigned int)(ISP_RD32(
				CAM_REG_FBC_IMGO_CTL1(reg_module))),
			    (unsigned int)(ISP_RD32(
				CAM_REG_FBC_IMGO_CTL2(reg_module))),
			    (unsigned int)(ISP_RD32(
				CAM_REG_FBC_RRZO_CTL1(reg_module))),
			    (unsigned int)(ISP_RD32(
				CAM_REG_FBC_RRZO_CTL2(reg_module))),
			    ISP_RD32(CAM_REG_IMGO_BASE_ADDR(reg_module)),
			    ISP_RD32(CAM_REG_RRZO_BASE_ADDR(reg_module)),
			    magic_num,
			    (unsigned int)((sec * 1000000 + usec) -
			      (1000000 * m_sec + m_usec)),
			    ISP_RD32(CAM_REG_CQ_THR0_BASEADDR(reg_module)));

#ifdef ENABLE_STT_IRQ_LOG /*STT addr*/
			IRQ_LOG_KEEPER(
			    module, m_CurrentPPB, _LOG_INF,
			    "CAM%c_aa(0x%x_0x%x_0x%x)af(0x%x_0x%x_0x%x),pd(0x%x_0x%x_0x%x),ps(0x%x_0x%x_0x%x)\n",
			    'A'+cardinalNum,
			    ISP_RD32(CAM_REG_AAO_BASE_ADDR(reg_module)),
			    (unsigned int)(ISP_RD32(
				CAM_REG_FBC_AAO_CTL1(reg_module))),
			    (unsigned int)(ISP_RD32(
				CAM_REG_FBC_AAO_CTL2(reg_module))),
			    ISP_RD32(CAM_REG_AFO_BASE_ADDR(reg_module)),
			    (unsigned int)(ISP_RD32(
				CAM_REG_FBC_AFO_CTL1(reg_module))),
			    (unsigned int)(ISP_RD32(
				CAM_REG_FBC_AFO_CTL2(reg_module))),
			    ISP_RD32(CAM_REG_PDO_BASE_ADDR(reg_module)),
			    (unsigned int)(ISP_RD32(
				CAM_REG_FBC_PDO_CTL1(reg_module))),
			    (unsigned int)(ISP_RD32(
				CAM_REG_FBC_PDO_CTL2(reg_module))),
			    ISP_RD32(CAM_REG_PSO_BASE_ADDR(reg_module)),
			    (unsigned int)(ISP_RD32(
				CAM_REG_FBC_PSO_CTL1(reg_module))),
			    (unsigned int)(ISP_RD32(
				CAM_REG_FBC_PSO_CTL2(reg_module))));
#endif
			/* keep current time */
			m_sec = sec;
			m_usec = usec;

			/* dbg information only */
			if (cur_v_cnt !=
			    ISP_RD32_TG_CAM_FRM_CNT(module, reg_module))
				IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"SW ISR right on next hw p1_done\n");

		}

		/* update SOF time stamp for eis user
		 * (need match with the time stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][12] =
			(unsigned int)(sec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][12] =
			(unsigned int)(usec);

		sof_count[module] += frmPeriod;
		/* for match vsync cnt */
		if (sof_count[module] > 255)
			sof_count[module] -= 256;

		g1stSof[module] = MFALSE;
	}
LB_CAM_SOF_IGNORE:

#ifdef ENABLE_STT_IRQ_LOG
	if (DmaStatus & (AAO_DONE_ST|AFO_DONE_ST|PDO_DONE_ST|PSO_DONE_ST)) {
		IRQ_LOG_KEEPER(
		    module, m_CurrentPPB, _LOG_INF,
		    "CAM%c_STT_Done_%d_0x%x\n",
		    'A'+cardinalNum,
		    (sof_count[module]) ?
		    (sof_count[module] - 1) : (sof_count[module]), DmaStatus);
	}
#endif

	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		/* 1. update interrupt status to all users */
		IspInfo.IrqInfo.Status[module][SIGNAL_INT][i] |= IrqStatus;
		IspInfo.IrqInfo.Status[module][DMA_INT][i] |= DmaStatus;

		/* 2. update signal time and passed by signal count */
		if (IspInfo.IrqInfo.MarkedFlag[module][SIGNAL_INT][i]
		    & IspInfo.IrqInfo.Mask[module][SIGNAL_INT]) {
			unsigned int cnt = 0, tmp = IrqStatus;

			while (tmp) {
				if (tmp & 0x1) {
					IspInfo.IrqInfo
					    .LastestSigTime_usec[module][cnt] =
					    (unsigned int)time_frmb.tv_usec;
					IspInfo.IrqInfo
					    .LastestSigTime_sec[module][cnt] =
					    (unsigned int) time_frmb.tv_sec;
					IspInfo.IrqInfo
					    .PassedBySigCnt[module][cnt][i]++;
				}
				tmp = tmp >> 1;
				cnt++;
			}
		} else {
		/* no any interrupt is not marked and
		 * in read mask in this irq type
		 */
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[module]));
	/*  */
	wake_up_interruptible(&IspInfo.WaitQueueHead[module]);

	/* dump log, use tasklet */
	if (IrqStatus & (SOF_INT_ST | SW_PASS1_DON_ST | VS_INT_ST)) {
		#if (ISP_BOTTOMHALF_WORKQ == 1)
		schedule_work(&isp_workque[module].isp_bh_work);
		#else
		tasklet_schedule(isp_tasklet[module].pIsp_tkt);
		#endif
	}

	return IRQ_HANDLED;
}


/******************************************************************************
 *
 ******************************************************************************/
static void SMI_INFO_DUMP(enum ISP_IRQ_TYPE_ENUM irq_module)
{
#ifndef EP_MARK_SMI
	switch (irq_module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		if (g_ISPIntStatus_SMI[irq_module].ispIntErr &
		    DMA_ERR_ST) {
			if ((g_ISPIntStatus_SMI[irq_module].ispIntErr &
			    INT_ST_MASK_CAM_WARN) ||
			    (g_ISPIntStatus_SMI[irq_module].ispInt3Err &
			    INT_ST_MASK_CAM_WARN_2)) {
				LOG_NOTICE(
				    "ERR:SMI_DUMP by module:%d\n", irq_module);
				smi_debug_bus_hang_detect(false, ISP_DEV_NAME);
			}

			g_ISPIntStatus_SMI[irq_module].ispIntErr =
				g_ISPIntStatus_SMI[irq_module].ispInt3Err = 0;
		} else if (g_ISPIntStatus_SMI[irq_module].ispIntErr &
		    CQ_VS_ERR_ST) {
			LOG_NOTICE("ERR:SMI_DUMP by module:%d\n", irq_module);
			smi_debug_bus_hang_detect(false, ISP_DEV_NAME);

			g_ISPIntStatus_SMI[irq_module].ispIntErr =
				g_ISPIntStatus_SMI[irq_module].ispInt3Err = 0;
		}
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
		if (g_ISPIntStatus_SMI[irq_module].ispIntErr &
			SV_IMGO_ERR) {
			if (g_ISPIntStatus_SMI[irq_module].ispIntErr &
				SV_IMGO_OVERRUN) {
				LOG_NOTICE(
					"ERR:SMI_DUMP by module:%d\n",
					irq_module);
				smi_debug_bus_hang_detect(false, ISP_DEV_NAME);
			}

			g_ISPIntStatus_SMI[irq_module].ispIntErr = 0;
		}
		break;
	default:
		LOG_NOTICE("error:unsupported module:%d\n", irq_module);
		break;
	}
#endif
}
static void ISP_TaskletFunc_CAM_A(unsigned long data)
{
	IRQ_LOG_PRINTER(ISP_IRQ_TYPE_INT_CAM_A_ST, m_CurrentPPB, _LOG_INF);
	SMI_INFO_DUMP(ISP_IRQ_TYPE_INT_CAM_A_ST);
}

static void ISP_TaskletFunc_CAM_B(unsigned long data)
{
	IRQ_LOG_PRINTER(ISP_IRQ_TYPE_INT_CAM_B_ST, m_CurrentPPB, _LOG_INF);
	SMI_INFO_DUMP(ISP_IRQ_TYPE_INT_CAM_B_ST);
}

static void ISP_TaskletFunc_SV_0(unsigned long data)
{
	IRQ_LOG_PRINTER(ISP_IRQ_TYPE_INT_CAMSV_0_ST, m_CurrentPPB, _LOG_INF);
	SMI_INFO_DUMP(ISP_IRQ_TYPE_INT_CAMSV_0_ST);
}

static void ISP_TaskletFunc_SV_1(unsigned long data)
{
	IRQ_LOG_PRINTER(ISP_IRQ_TYPE_INT_CAMSV_1_ST, m_CurrentPPB, _LOG_INF);
	SMI_INFO_DUMP(ISP_IRQ_TYPE_INT_CAMSV_1_ST);

}

static void ISP_TaskletFunc_SV_2(unsigned long data)
{
	IRQ_LOG_PRINTER(ISP_IRQ_TYPE_INT_CAMSV_2_ST, m_CurrentPPB, _LOG_INF);
	SMI_INFO_DUMP(ISP_IRQ_TYPE_INT_CAMSV_2_ST);

}

static void ISP_TaskletFunc_SV_3(unsigned long data)
{
	IRQ_LOG_PRINTER(ISP_IRQ_TYPE_INT_CAMSV_3_ST, m_CurrentPPB, _LOG_INF);
	SMI_INFO_DUMP(ISP_IRQ_TYPE_INT_CAMSV_3_ST);

}

static void ISP_TaskletFunc_SV_4(unsigned long data)
{
	IRQ_LOG_PRINTER(ISP_IRQ_TYPE_INT_CAMSV_4_ST, m_CurrentPPB, _LOG_INF);
	SMI_INFO_DUMP(ISP_IRQ_TYPE_INT_CAMSV_4_ST);

}

static void ISP_TaskletFunc_SV_5(unsigned long data)
{
	IRQ_LOG_PRINTER(ISP_IRQ_TYPE_INT_CAMSV_5_ST, m_CurrentPPB, _LOG_INF);
	SMI_INFO_DUMP(ISP_IRQ_TYPE_INT_CAMSV_5_ST);

}

#if (ISP_BOTTOMHALF_WORKQ == 1)
static void ISP_BH_Workqueue(struct work_struct *pWork)
{
	struct IspWorkqueTable *pWorkTable =
		container_of(pWork, struct IspWorkqueTable, isp_bh_work);

	IRQ_LOG_PRINTER(pWorkTable->module, m_CurrentPPB, _LOG_ERR);
	IRQ_LOG_PRINTER(pWorkTable->module, m_CurrentPPB, _LOG_INF);
}
#endif

/******************************************************************************
 *
 ******************************************************************************/
module_init(ISP_Init);
module_exit(ISP_Exit);
MODULE_DESCRIPTION("Camera ISP driver");
MODULE_AUTHOR("SW2");
MODULE_LICENSE("GPL");
