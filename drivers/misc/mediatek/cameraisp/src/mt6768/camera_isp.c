/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

/******************************************************************************
 * camera_isp.c - MT6768 Linux ISP Device Driver
 *
 * DESCRIPTION:
 *     This file provid the other drivers ISP relative functions
 *
 *****************************************************************************/
#define MyTag "[ISP]"
#define IRQTag "KEEPER"
#define pr_fmt(fmt) MyTag "[%s] " fmt, __func__

#define ISP_DEBUG
#ifdef ISP_DEBUG
#define LOG_DBG(format, args...)    pr_info(MyTag "[%s] " format, \
	 __func__, ##args)
#else
#define LOG_DBG(format, args...)
#endif

#define LOG_INF(format, args...)       pr_info(MyTag "[%s] " format, \
	 __func__, ##args)
#define LOG_NOTICE(format, args...)    pr_notice(MyTag "[%s] " format, \
	 __func__, ##args)

/* MET: define to enable MET*/
//#define ISP_MET_READY

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
#include <linux/printk.h>

#include <mt-plat/sync_write.h> /* For mt65xx_reg_sync_writel(). */

#ifdef COFNIG_MTK_IOMMU
#include <mtk_iommu.h>
#else
#include <m4u.h>
#endif

/*#define EP_CODE_MARK_CMDQ*/
#ifndef EP_CODE_MARK_CMDQ
#include <cmdq_core.h>
#endif

/*#define EP_MARK_SMI*/ /* disable SMI related for EP */
#include <smi_public.h>

/*for kernel log count*/
#define _K_LOG_ADJUST (0)//wait log owner rdy

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_platform.h>  /* for device tree */
#include <linux/of_irq.h>       /* for device tree */
#include <linux/of_address.h>   /* for device tree */
#endif

#if defined(ISP_MET_READY)
/*MET:met mmsys profile*/
#include <mt-plat/met_drv.h>
#endif

/*#define EP_STAGE*/
#ifdef EP_STAGE
#define EP_MARK_SMI
#define DUMMY_INT    /*for EP if load dont need to use camera*/
#define EP_NO_PMQOS /* If PMQoS is not ready on EP stage */
//#define EP_NO_CLKMGR /* for clkmgr*/
#endif
//#define EP_NO_PMQOS /* If PMQoS is not ready on EP stage */
#include "inc/camera_isp.h"

#ifndef EP_NO_PMQOS /* EP_NO_PMQOS is equivalent to EP_MARK_MMDVFS */
//#include <mmdvfs_mgr.h>
#include <mmdvfs_pmqos.h>
#include <linux/pm_qos.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
/* Use this qos request to control camera dynamic frequency change */
struct mtk_pm_qos_request isp_qos;
struct mtk_pm_qos_request camsys_qos_request[ISP_IRQ_TYPE_INT_CAM_B_ST+1];
static struct ISP_PM_QOS_STRUCT G_PM_QOS[ISP_IRQ_TYPE_INT_CAM_B_ST+1];
static u32 PMQoS_BW_value;
static u32 target_clk;
#endif

#define CAMSV_DBG
#ifdef CAMSV_DBG
#define CAM_TAG "CAM:"
#define CAMSV_TAG "SV1:"
#define CAMSV2_TAG "SV2:"
#else
#define CAMSV_TAG ""
#define CAMSV2_TAG ""
#define CAM_TAG ""
#endif

#include <archcounter_timesync.h>
#include <ccu_inc.h>

/*  */
#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif

#define ISP_DEV_NAME           "camera-isp"
#define SMI_LARB_MMU_CTL       (1)

/*#define ENABLE_WAITIRQ_LOG*/       /* wait irq debug logs */
/*#define ENABLE_STT_IRQ_LOG*/       /*show STT irq debug logs */
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

/******************************************************************************
 *
 *****************************************************************************/
/* #define ISP_WR32(addr, data)    iowrite32(data, addr) // For other proj. */
#define ISP_WR32(addr, data) do {\
			if ((addr) != NULL) {\
				mt_reg_sync_writel((data), (addr));\
			} \
		} while (0)
#define ISP_RD32(addr)                  (((addr) != NULL) ? ioread32((void *)(addr)) : 0)
/* #define ISP_SET_BIT(reg, bit)
 *	((*(volatile unsigned int*)(reg)) |= (unsigned int)(1 << (bit)))
 */
/* #define ISP_CLR_BIT(reg, bit)
 *	((*(volatile unsigned int*)(reg)) &= ~((unsigned int)(1 << (bit))))
 */
/******************************************************************************
 *
 *****************************************************************************/
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
 *****************************************************************************/
#define DUMP_GCE_TPIPE  0


/**
 *    CAM interrupt status
 */
/* normal siganl */
#define VS_INT_ST           (1L<<0)
#define TG_INT1_ST          (1L<<1)
#define TG_INT2_ST          (1L<<2)
#define EXPDON_ST           (1L<<3)
#define HW_PASS1_DON_ST     (1L<<11)
#define SOF_INT_ST          (1L<<12)
#define SW_PASS1_DON_ST     (1L<<30)
/* err status */
#define TG_ERR_ST           (1L<<4)
#define TG_GBERR_ST         (1L<<5)
#define CQ_CODE_ERR_ST      (1L<<6)
#define CQ_APB_ERR_ST       (1L<<7)
#define CQ_VS_ERR_ST        (1L<<8)
#define AMX_ERR_ST          (1L<<15)
#define RMX_ERR_ST          (1L<<16)
#define BMX_ERR_ST          (1L<<17)
#define RRZO_ERR_ST         (1L<<18)
#define AFO_ERR_ST          (1L<<19)
#define IMGO_ERR_ST         (1L<<20)
#define AAO_ERR_ST          (1L<<21)
#define PSO_ERR_ST          (1L<<22)
#define LCSO_ERR_ST         (1L<<23)
#define BNR_ERR_ST          (1L<<24)
#define LSC_ERR_ST          (1L<<25)
#define CAC_ERR_ST          (1L<<26)
#define DMA_ERR_ST          (1L<<29)
/**
 *    CAM DMA done status
 */
#define IMGO_DONE_ST        (1L<<0)
#define UFEO_DONE_ST        (1L<<1)
#define RRZO_DONE_ST        (1L<<2)
#define EISO_DONE_ST        (1L<<3)
#define FLKO_DONE_ST        (1L<<4)
#define AFO_DONE_ST         (1L<<5)
#define LCSO_DONE_ST        (1L<<6)
#define AAO_DONE_ST         (1L<<7)
#define BPCI_DONE_ST        (1L<<9)
#define LSCI_DONE_ST        (1L<<10)
/* #define CACI_DONE_ST        (1L<<11) */
#define PDO_DONE_ST         (1L<<13)
#define PSO_DONE_ST         (1L<<14)
/**
 *    CAMSV interrupt status
 */
/* normal signal */
#define SV_VS1_ST           (1L<<0)
#define SV_TG_ST1           (1L<<1)
#define SV_TG_ST2           (1L<<2)
#define SV_EXPDON_ST        (1L<<3)
#define SV_SOF_INT_ST       (1L<<7)
#define SV_HW_PASS1_DON_ST  (1L<<10)
#define SV_SW_PASS1_DON_ST  (1L<<20)
/* err status */
#define SV_TG_ERR           (1L<<4)
#define SV_TG_GBERR         (1L<<5)
#define SV_IMGO_ERR         (1L<<16)
#define SV_IMGO_OVERRUN     (1L<<17)

/**
 *    IRQ signal mask
 */
#define INT_ST_MASK_CAM     ( \
			      VS_INT_ST |\
			      TG_INT1_ST |\
			      TG_INT2_ST |\
			      EXPDON_ST |\
			      HW_PASS1_DON_ST |\
			      SOF_INT_ST |\
			      SW_PASS1_DON_ST)
/**
 *    dma done mask
 */
#define DMA_ST_MASK_CAM     (\
			     IMGO_DONE_ST |\
			     UFEO_DONE_ST |\
			     RRZO_DONE_ST |\
			     EISO_DONE_ST |\
			     FLKO_DONE_ST |\
			     AFO_DONE_ST |\
			     LCSO_DONE_ST |\
			     AAO_DONE_ST |\
			     BPCI_DONE_ST |\
			     LSCI_DONE_ST |\
			     PDO_DONE_ST | \
			     PSO_DONE_ST)

/**
 *    IRQ Warning Mask
 */
#define INT_ST_MASK_CAM_WARN    (\
				 RRZO_ERR_ST |\
				 AFO_ERR_ST |\
				 IMGO_ERR_ST |\
				 AAO_ERR_ST |\
				 PSO_ERR_ST | \
				 LCSO_ERR_ST |\
				 BNR_ERR_ST |\
				 LSC_ERR_ST)

/**
 *    IRQ Error Mask
 */
#define INT_ST_MASK_CAM_ERR     (\
				 TG_ERR_ST |\
				 TG_GBERR_ST |\
				 CQ_CODE_ERR_ST |\
				 CQ_APB_ERR_ST |\
				 CQ_VS_ERR_ST |\
				 AMX_ERR_ST |\
				 RMX_ERR_ST |\
				 BMX_ERR_ST |\
				 BNR_ERR_ST |\
				 LSC_ERR_ST |\
				 DMA_ERR_ST)


/**
 *    IRQ signal mask
 */
#define INT_ST_MASK_CAMSV       (\
				 SV_VS1_ST |\
				 SV_TG_ST1 |\
				 SV_TG_ST2 |\
				 SV_EXPDON_ST |\
				 SV_SOF_INT_ST |\
				 SV_HW_PASS1_DON_ST |\
				 SV_SW_PASS1_DON_ST)
/**
 *    IRQ Error Mask
 */
#define INT_ST_MASK_CAMSV_ERR   (\
				 SV_TG_ERR |\
				 SV_TG_GBERR |\
				 SV_IMGO_ERR |\
				 SV_IMGO_OVERRUN)

static irqreturn_t ISP_Irq_CAM_A(signed int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAM_B(signed int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAMSV_0(signed int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAMSV_1(signed int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAMSV_2(signed int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAMSV_3(signed int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAMSV_4(signed int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_CAMSV_5(signed int  Irq, void *DeviceId);
static irqreturn_t ISP_Irq_DIP_A(signed int  Irq, void *DeviceId);


typedef irqreturn_t (*IRQ_CB)(signed int, void *);

struct ISR_TABLE {
	IRQ_CB          isr_fp;
	unsigned int    int_number;
	char            device_name[16];
};

static DEFINE_MUTEX(gDipMutex);

#ifndef CONFIG_OF
const struct ISR_TABLE IRQ_CB_TBL[ISP_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_CAM_A,     CAM0_IRQ_BIT_ID,    "CAM_A"},
	{NULL,                            0,    "CAM_B"},
	{NULL,                            0,    "DIP_A"},
	{ISP_Irq_CAMSV_0,   CAM_SV0_IRQ_BIT_ID, "CAMSV_0"},
	{ISP_Irq_CAMSV_1,   CAM_SV1_IRQ_BIT_ID, "CAMSV_1"},
	{NULL,                               0,     "UNI"}
};

#else
/* int number is got from kernel api */
/* Must be the same name with that in device node. */
const struct ISR_TABLE IRQ_CB_TBL[ISP_IRQ_TYPE_AMOUNT] = {
#ifdef DUMMY_INT
	{ISP_Irq_CAM_A,     0,  "cam2-dummy"},
	{ISP_Irq_CAM_B,     0,  "cam3-dummy"},
	{ISP_Irq_DIP_A,     0,  "dip-dummy"},
	{ISP_Irq_CAMSV_0,   0,  "camsv1-dummy"},
	{ISP_Irq_CAMSV_1,   0,  "camsv2-dummy"},
	{ISP_Irq_CAMSV_2,   0,  "camsv3-dummy"},
	{ISP_Irq_CAMSV_3,   0,  "camsv4-dummy"},
	{ISP_Irq_CAMSV_4,   0,  "camsv5-dummy"},
	{ISP_Irq_CAMSV_5,   0,  "camsv6-dummy"},
	{NULL,              0,  "cam1-dummy"} /* UNI */
#else
	{ISP_Irq_CAM_A,     0,  "cam2"},
	{ISP_Irq_CAM_B,     0,  "cam3"},
	{ISP_Irq_DIP_A,     0,  "dip"},
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
 * Remider: Add "mediatek,dip1" node manually in .dtsi
 */
static const struct of_device_id isp_of_ids[] = {
	{ .compatible = "mediatek,imgsys", },
	{ .compatible = "mediatek,dip1", },
	{ .compatible = "mediatek,camsys", },
	{ .compatible = "mediatek,cam1", },
	{ .compatible = "mediatek,cam2", },
	{ .compatible = "mediatek,cam3", },
	{ .compatible = "mediatek,camsv1", },
	{ .compatible = "mediatek,camsv2", },
	{ .compatible = "mediatek,camsv3", },
	{ .compatible = "mediatek,camsv4", },
	/*{ .compatible = "mediatek,camsv5", },*/
	/*{ .compatible = "mediatek,camsv6", },*/
	{}
};

#endif
/* ///////////////////////////////////////////////////////////////////////// */
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
	{NULL,                  &tkt[ISP_IRQ_TYPE_INT_DIP_A_ST]},
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
	{ISP_IRQ_TYPE_INT_DIP_A_ST},
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

#ifndef CONFIG_MTK_CLKMGR /*CCF*/
#include <linux/clk.h>
struct ISP_CLK_STRUCT {
	struct clk *ISP_SCP_SYS_DIS;
	struct clk *ISP_SCP_SYS_ISP;
	struct clk *ISP_SCP_SYS_CAM;
	struct clk *ISP_IMG_DIP;
	struct clk *ISP_CAM_CAMSYS;
	struct clk *ISP_CAM_CAMTG;
	struct clk *ISP_CAM_SENINF;
	struct clk *ISP_CAM_CAMSV0;
	struct clk *ISP_CAM_CAMSV1;
	struct clk *ISP_CAM_CAMSV2;
};
struct ISP_CLK_STRUCT isp_clk;
#endif

/*static unsigned long gISPSYS_Irq[ISP_IRQ_TYPE_AMOUNT];*/
/*static unsigned long gISPSYS_Reg[ISP_CAM_BASEADDR_NUM];*/

#ifdef CONFIG_OF
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
static struct isp_sec_dapc_reg lock_reg;
static unsigned int sec_on;
#endif

#define AEE_DUMP_BY_USING_ION_MEMORY
#define AEE_DUMP_REDUCE_MEMORY
#ifdef AEE_DUMP_REDUCE_MEMORY
/* ion */

#ifdef AEE_DUMP_BY_USING_ION_MEMORY
#include <ion.h>
#include <mtk/ion_drv.h>
#include <mtk/mtk_ion.h>

struct isp_imem_memory {
	void *handle;
	int ion_fd;
	uint64_t va;
	uint32_t pa;
	uint32_t length;
};

static struct ion_client *isp_p2_ion_client;
static struct isp_imem_memory g_isp_p2_imem_buf;
#endif
static bool g_bIonBufferAllocated;
static unsigned int *g_pPhyISPBuffer;
/* Kernel Warning */
static unsigned int *g_pKWTpipeBuffer;
static unsigned int *g_pKWCmdqBuffer;
static unsigned int *g_pKWVirISPBuffer;
/* Navtive Exception */
static unsigned int *g_pTuningBuffer;
static unsigned int *g_pTpipeBuffer;
static unsigned int *g_pVirISPBuffer;
static unsigned int *g_pCmdqBuffer;
#else
/* Kernel Warning */
static unsigned int g_KWTpipeBuffer[(MAX_ISP_TILE_TDR_HEX_NO >> 2)];
static unsigned int g_KWCmdqBuffer[(MAX_ISP_CMDQ_BUFFER_SIZE >> 2)];
static unsigned int g_KWVirISPBuffer[(ISP_DIP_REG_SIZE >> 2)];
/* Navtive Exception */
static unsigned int g_PhyISPBuffer[(ISP_DIP_REG_SIZE >> 2)];
static unsigned int g_TuningBuffer[(ISP_DIP_REG_SIZE >> 2)];
static unsigned int g_TpipeBuffer[(MAX_ISP_TILE_TDR_HEX_NO >> 2)];
static unsigned int g_VirISPBuffer[(ISP_DIP_REG_SIZE >> 2)];
static unsigned int g_CmdqBuffer[(MAX_ISP_CMDQ_BUFFER_SIZE >> 2)];
#endif
static bool g_bUserBufIsReady = MFALSE;
static unsigned int DumpBufferField;
static bool g_bDumpPhyISPBuf = MFALSE;
static unsigned int g_tdriaddr = 0xffffffff;
static unsigned int g_cmdqaddr = 0xffffffff;
static struct ISP_GET_DUMP_INFO_STRUCT g_dumpInfo
		= {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
static struct ISP_MEM_INFO_STRUCT g_TpipeBaseAddrInfo = {0x0, 0x0, NULL, 0x0};
static struct ISP_MEM_INFO_STRUCT g_CmdqBaseAddrInfo = {0x0, 0x0, NULL, 0x0};
static unsigned int m_CurrentPPB;

#ifdef CONFIG_PM_SLEEP
struct wakeup_source *isp_wake_lock;
#endif
static int g_WaitLockCt;
/*
 * static void __iomem *g_isp_base_dase;
 * static void __iomem *g_isp_inner_base_dase;
 * static void __iomem *g_imgsys_config_base_dase;
 */

/* Get HW modules' base address from device nodes */
#define ISP_CAMSYS_CONFIG_BASE          (isp_devs[ISP_CAMSYS_CONFIG_IDX].regs)
#define ISP_CAM_A_BASE                  (isp_devs[ISP_CAM_A_IDX].regs)
#define ISP_CAM_B_BASE                  (isp_devs[ISP_CAM_B_IDX].regs)
#define ISP_CAMSV0_BASE                 (isp_devs[ISP_CAMSV0_IDX].regs)
#define ISP_CAMSV1_BASE                 (isp_devs[ISP_CAMSV1_IDX].regs)
#define ISP_CAMSV2_BASE                 (isp_devs[ISP_CAMSV2_IDX].regs)
#define ISP_CAMSV3_BASE                 (isp_devs[ISP_CAMSV3_IDX].regs)
#define ISP_CAMSV4_BASE                 (isp_devs[ISP_CAMSV4_IDX].regs)
#define ISP_CAMSV5_BASE                 (isp_devs[ISP_CAMSV5_IDX].regs)
#define ISP_CAM_UNI_BASE                (isp_devs[ISP_UNI_A_IDX].regs)
#define ISP_IMGSYS_CONFIG_BASE          (isp_devs[ISP_IMGSYS_CONFIG_IDX].regs)
#define ISP_DIP_A_BASE                  (isp_devs[ISP_DIP_A_IDX].regs)

void __iomem *ISP_SENINF0_BASE;
void __iomem *ISP_SENINF1_BASE;
void __iomem *ISP_SENINF2_BASE;
void __iomem *ISP_SENINF3_BASE;

void __iomem *CLOCK_CELL_BASE;

void __iomem *ISP_MMSYS_CONFIG_BASE;

#if (SMI_LARB_MMU_CTL == 1)
void __iomem *SMI_LARB_BASE[8];
#endif


/* TODO: Remove start, Jessy */
#define ISP_ADDR                      (gISPSYS_Reg[ISP_BASE_ADDR])
#define ISP_IMGSYS_BASE               (gISPSYS_Reg[ISP_IMGSYS_CONFIG_BASE_ADDR])
#define ISP_ADDR_CAMINF               (gISPSYS_Reg[ISP_IMGSYS_CONFIG_BASE_ADDR])

#define ISP_MIPI_ANA_ADDR             (gISPSYS_Reg[ISP_MIPI_ANA_BASE_ADDR])

#define ISP_IMGSYS_BASE_PHY           0x1A000000

#else
#define ISP_ADDR                      (IMGSYS_BASE + 0x4000)
#define ISP_IMGSYS_BASE               IMGSYS_BASE
#define ISP_ADDR_CAMINF               IMGSYS_BASE
#define ISP_MIPI_ANA_ADDR             0x10217000

#endif
/* TODO: Remove end, Jessy */


#define ISP_REG_SW_CTL_RST_CAM_P1       (1)
#define ISP_REG_SW_CTL_RST_CAM_P2       (2)
#define ISP_REG_SW_CTL_RST_CAMSV        (3)
#define ISP_REG_SW_CTL_RST_CAMSV2       (4)

struct S_START_T {
	unsigned int sec;
	unsigned int usec;
};

/* QQ, remove later */
/* record remain node count(success/fail)
 * excludes head when enque/deque control
 */
static unsigned int g_regScen = 0xa5a5a5a5; /* remove later */


static /*volatile*/ wait_queue_head_t P2WaitQueueHead_WaitDeque;
static /*volatile*/ wait_queue_head_t P2WaitQueueHead_WaitFrame;
static /*volatile*/ wait_queue_head_t P2WaitQueueHead_WaitFrameEQDforDQ;
static spinlock_t      SpinLock_P2FrameList;
#define _MAX_SUPPORT_P2_FRAME_NUM_ 512
#define _MAX_SUPPORT_P2_BURSTQ_NUM_ 8
#define _MAX_SUPPORT_P2_PACKAGE_NUM_ \
	(_MAX_SUPPORT_P2_FRAME_NUM_/_MAX_SUPPORT_P2_BURSTQ_NUM_)
struct ISP_P2_BUFQUE_IDX_STRUCT {
	/* starting index for frames in the ring list */
	/*volatile */signed int start;
	/* current index for running frame in the ring list */
	/*volatile */signed int curr;
	/* ending index for frames in the ring list */
	/*volatile */signed int end;
};

struct ISP_P2_FRAME_UNIT_STRUCT {
	/*volatile */unsigned int           processID; /* caller process ID */
	/*volatile */unsigned int           callerID; /* caller thread ID */
	/* QQ. optional -> to judge cq combination(for judging frame) */
	/*volatile */unsigned int           cqMask;

	/*volatile */enum ISP_P2_BUF_STATE_ENUM  bufSts; /* buffer status */
};

static /*volatile */struct ISP_P2_BUFQUE_IDX_STRUCT
	P2_FrameUnit_List_Idx[ISP_P2_BUFQUE_PROPERTY_NUM];
static /*volatile */struct ISP_P2_FRAME_UNIT_STRUCT
	P2_FrameUnit_List[ISP_P2_BUFQUE_PROPERTY_NUM]
		[_MAX_SUPPORT_P2_FRAME_NUM_];

struct ISP_P2_FRAME_PACKAGE_STRUCT {
	/*volatile */unsigned int           processID;/* caller process ID */
	/*volatile */unsigned int           callerID; /* caller thread ID */
	/* to judge it belongs to which frame package */
	/*volatile */unsigned int           dupCQIdx;
	/*volatile */signed int             frameNum;
	/* number of dequed buffer no matter deque success or fail */
	/*volatile */signed int             dequedNum;
};
static /*volatile */struct ISP_P2_BUFQUE_IDX_STRUCT
	P2_FramePack_List_Idx[ISP_P2_BUFQUE_PROPERTY_NUM];
static /*volatile */struct ISP_P2_FRAME_PACKAGE_STRUCT
	P2_FramePackage_List[ISP_P2_BUFQUE_PROPERTY_NUM]
		[_MAX_SUPPORT_P2_PACKAGE_NUM_];




static  spinlock_t      SpinLockRegScen;

/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32
static  spinlock_t      SpinLock_UserKey;

#if (TIMESTAMP_QUEUE_EN == 1)
static void ISP_GetDmaPortsStatus(enum ISP_DEV_NODE_ENUM reg_module,
	unsigned int *DmaPortsStats);
static enum CAM_FrameST Irq_CAM_SttFrameStatus(enum ISP_DEV_NODE_ENUM module,
	enum ISP_IRQ_TYPE_ENUM irq_mod, unsigned int dma_id,
	unsigned int delayCheck);
static int32_t ISP_PushBufTimestamp(unsigned int module, unsigned int dma_id,
	unsigned int sec, unsigned int usec, unsigned int frmPeriod);
static int32_t ISP_PopBufTimestamp(unsigned int module, unsigned int dma_id,
	struct S_START_T *pTstp);
static int32_t ISP_WaitTimestampReady(unsigned int module, unsigned int dma_id);
#endif

/******************************************************************************
 *
 *****************************************************************************/
/* internal data */
/* pointer to the kmalloc'd area, rounded up to a page boundary */
static int *pTbl_RTBuf[ISP_IRQ_TYPE_AMOUNT];
static int Tbl_RTBuf_MMPSize[ISP_IRQ_TYPE_AMOUNT];

/* original pointer for kmalloc'd area as returned by kmalloc */
static void *pBuf_kmalloc[ISP_IRQ_TYPE_AMOUNT];
/*  */
static struct ISP_RT_BUF_STRUCT *pstRTBuf[ISP_IRQ_TYPE_AMOUNT] = {NULL};

/* // Marked to remove build warning.
 * static struct ISP_DEQUE_BUF_INFO_STRUCT g_deque_buf = {0,{}};
 */

unsigned long g_Flash_SpinLock;


static unsigned int G_u4EnableClockCount;
static atomic_t G_u4DevNodeCt;

int pr_detect_count;

/*save ion fd*/
#define ENABLE_KEEP_ION_HANDLE

#ifdef ENABLE_KEEP_ION_HANDLE
#define _ion_keep_max_   (64)/*32*/
#include "ion_drv.h" /*g_ion_device*/
static struct ion_client *pIon_client;
static signed int G_WRDMA_IonCt[2][_dma_max_wr_*_ion_keep_max_] = { {0}, {0} };
static signed int G_WRDMA_IonFd[2][_dma_max_wr_*_ion_keep_max_] = { {0}, {0} };
static struct ion_handle *G_WRDMA_IonHnd[2][_dma_max_wr_*_ion_keep_max_]
			= { {NULL}, {NULL} };
/* protect G_WRDMA_IonHnd & G_WRDMA_IonFd */
static spinlock_t SpinLock_IonHnd[2][_dma_max_wr_];

struct T_ION_TBL {
	enum ISP_DEV_NODE_ENUM node;
	signed int *pIonCt;
	signed int *pIonFd;
	struct ion_handle **pIonHnd;
	spinlock_t *pLock;
};

static struct T_ION_TBL gION_TBL[ISP_DEV_NODE_NUM] = {
	{ISP_DEV_NODE_NUM, NULL, NULL, NULL, NULL},
	{ISP_DEV_NODE_NUM, NULL, NULL, NULL, NULL},
	{ISP_DEV_NODE_NUM, NULL, NULL, NULL, NULL},
	{ISP_DEV_NODE_NUM, NULL, NULL, NULL, NULL},
	{ISP_CAM_A_IDX, (signed int *)G_WRDMA_IonCt[0],
	 (signed int *)G_WRDMA_IonFd[0],
	 (struct ion_handle **)G_WRDMA_IonHnd[0],
	 (spinlock_t *)SpinLock_IonHnd[0]},
	{ISP_CAM_B_IDX, (signed int *)G_WRDMA_IonCt[1],
	 (signed int *)G_WRDMA_IonFd[1],
	 (struct ion_handle **)G_WRDMA_IonHnd[1],
	 (spinlock_t *)SpinLock_IonHnd[1]},
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
 *****************************************************************************/
struct ISP_USER_INFO_STRUCT {
	pid_t   Pid;
	pid_t   Tid;
};

/******************************************************************************
 *
 *****************************************************************************/
#define ISP_BUF_SIZE            (4096)
#define ISP_BUF_SIZE_WRITE      1024
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
 *****************************************************************************/
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

static signed int FirstUnusedIrqUserKey = 1;
#define USERKEY_STR_LEN 128

struct UserKeyInfo {
	/* for the user that register a userKey */
	char userName[USERKEY_STR_LEN];
	/* the user key for that user */
	int userKey;
};
/* array for recording the user name for a specific user key */
static struct UserKeyInfo IrqUserKey_UserInfo[IRQ_USER_NUM_MAX];

struct ISP_IRQ_INFO_STRUCT {
	/* Add an extra index for status type in 6797 -> signal or dma */
	unsigned int    Status[ISP_IRQ_TYPE_AMOUNT][ISP_IRQ_ST_AMOUNT]
				[IRQ_USER_NUM_MAX];
	unsigned int    Mask[ISP_IRQ_TYPE_AMOUNT][ISP_IRQ_ST_AMOUNT];
	unsigned int    ErrMask[ISP_IRQ_TYPE_AMOUNT][ISP_IRQ_ST_AMOUNT];
	unsigned int    WarnMask[ISP_IRQ_TYPE_AMOUNT][ISP_IRQ_ST_AMOUNT];
	/* flag for indicating that user do mark for a interrupt or not */
	unsigned int    MarkedFlag[ISP_IRQ_TYPE_AMOUNT][ISP_IRQ_ST_AMOUNT]
				[IRQ_USER_NUM_MAX];
	/* time for marking a specific interrupt */
	unsigned int    MarkedTime_sec[ISP_IRQ_TYPE_AMOUNT][32]
				[IRQ_USER_NUM_MAX];
	/* time for marking a specific interrupt */
	unsigned int    MarkedTime_usec[ISP_IRQ_TYPE_AMOUNT][32]
				[IRQ_USER_NUM_MAX];
	/* number of a specific signal that passed by */
	signed int     PassedBySigCnt[ISP_IRQ_TYPE_AMOUNT][32]
				[IRQ_USER_NUM_MAX];
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
		unsigned int   WrIndex; /* increase when p1done or dmao done */
		unsigned int   RdIndex; /* increase when user deque */
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
	({                                                          \
		int i = 0, cnt = 0;                                  \
		for (i = 0; i < 32; i++) {                            \
			if ((value>>i) & (0x00000001)) {   \
				break;                                      \
			} else {                                            \
				cnt++;  \
			}                                      \
		}                                                    \
		cnt;                                                \
	})


static unsigned int g_ISPIntErr[ISP_IRQ_TYPE_AMOUNT] = {0};
static unsigned int g_ISPIntErr_SMI[ISP_IRQ_TYPE_AMOUNT] = {0};
static unsigned int g_DmaErr_CAM[ISP_IRQ_TYPE_AMOUNT][_cam_max_] = {{0} };

enum ISP_WAITQ_HEAD_IRQ_ENUM {
	ISP_WAITQ_HEAD_IRQ_SOF = 0,
	ISP_WAITQ_HEAD_IRQ_SW_P1_DONE,
	ISP_WAITQ_HEAD_IRQ_AAO_DONE,
	ISP_WAITQ_HEAD_IRQ_FLKO_DONE,
	ISP_WAITQ_HEAD_IRQ_AFO_DONE,
	ISP_WAITQ_HEAD_IRQ_PSO_DONE,
	ISP_WAITQ_HEAD_IRQ_PDO_DONE,
	ISP_WAITQ_HEAD_IRQ_AMOUNT
};

enum ISP_WAIT_QUEUE_HEAD_IRQ_SV_ENUM {
	ISP_WAITQ_HEAD_IRQ_SV_SOF = 0,
	ISP_WAITQ_HEAD_IRQ_SV_SW_P1_DONE,
	ISP_WAITQ_HEAD_IRQ_SV_AMOUNT
};

#define CAM_AMOUNT		(ISP_IRQ_TYPE_INT_CAM_B_ST-\
						ISP_IRQ_TYPE_INT_CAM_A_ST+1)
#define CAMSV_AMOUNT	(ISP_IRQ_TYPE_INT_CAMSV_5_ST-\
						ISP_IRQ_TYPE_INT_CAMSV_0_ST+1)

struct ISP_INFO_STRUCT {
	spinlock_t                      SpinLockIspRef;
	spinlock_t                      SpinLockIsp;
	spinlock_t                      SpinLockIrq[ISP_IRQ_TYPE_AMOUNT];
	spinlock_t                      SpinLockIrqCnt[ISP_IRQ_TYPE_AMOUNT];
	spinlock_t                      SpinLockRTBC;
	spinlock_t                      SpinLockClock;
	wait_queue_head_t               WaitQueueHead[ISP_IRQ_TYPE_AMOUNT];
	wait_queue_head_t	WaitQHeadCam[
						CAM_AMOUNT][
						ISP_WAITQ_HEAD_IRQ_AMOUNT];
	wait_queue_head_t	WaitQHeadCamsv[
						CAMSV_AMOUNT][
						ISP_WAITQ_HEAD_IRQ_SV_AMOUNT];
	unsigned int                         UserCount;
	unsigned int                         DebugMask;
	signed int							IrqNum;
	struct ISP_IRQ_INFO_STRUCT			IrqInfo;
	struct ISP_IRQ_ERR_WAN_CNT_STRUCT		IrqCntInfo;
	struct ISP_BUF_INFO_STRUCT			BufInfo;
	struct ISP_TIME_LOG_STRUCT             TimeLog;
	#if (TIMESTAMP_QUEUE_EN == 1)
	struct ISP_TIMESTPQ_INFO_STRUCT        TstpQInfo[ISP_IRQ_TYPE_AMOUNT];
	#endif
};

static struct ISP_INFO_STRUCT IspInfo;
static bool    SuspnedRecord[ISP_DEV_NODE_NUM] = {0};

/* currently, _LOG_DBG only used at ipl_buf_ctrl to protect critical section */
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
struct SV_LOG_STR {
	unsigned int _cnt[LOG_PPNUM][_LOG_MAX];
	/* char   _str[_LOG_MAX][SV_LOG_STR_LEN]; */
	char *_str[LOG_PPNUM][_LOG_MAX];
	struct S_START_T   _lastIrqTime;
};

static void *pLog_kmalloc;
static struct SV_LOG_STR gSvLog[ISP_IRQ_TYPE_AMOUNT];

/**
 *   for irq used, keep log until IRQ_LOG_PRINTER being involked.
 *   limited:
 *   each log must shorter than 512 bytes,
 *   total log length in each irq/logtype can't over 1024 bytes.
 */
#define IRQ_LOG_KEEPER_T(sec, usec) {\
	sec = ktime_get();     \
	do_div(sec, 1000);    \
	usec = do_div(sec, 1000000);\
}

#if 1
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...) do {\
	char *ptr; \
	char *pDes;\
	signed int avaLen;\
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
	ptr = pDes = (char *)\
		&(gSvLog[irq]._str[ppb][logT][gSvLog[irq]._cnt[ppb][logT]]);  \
	avaLen = str_leng - 1 - gSvLog[irq]._cnt[ppb][logT];\
	if (avaLen > 1) {\
		if (snprintf((char *)(pDes), avaLen, "[%d.%06d]" fmt,\
			gSvLog[irq]._lastIrqTime.sec, \
			gSvLog[irq]._lastIrqTime.usec,\
			##__VA_ARGS__) < 0) {\
			pr_info("[Error] %s: snprintf failed", __func__);\
		} \
		if ('\0' != gSvLog[irq]._str[ppb][logT][str_leng - 1]) {\
			pr_info("log str over flow(%d)", irq);\
		} \
		while (*ptr++ != '\0') {        \
			(*ptr2)++;\
		}     \
	} else { \
		pr_info("(%d)(%d)log str avalible=0, print log\n", irq, logT);\
		ptr = pSrc->_str[ppb][logT];\
		if (pSrc->_cnt[ppb][logT] != 0) {\
			if (logT == _LOG_DBG) {\
				for (i = 0; i < DBG_PAGE; i++) {\
					if (ptr[NORMAL_STR_LEN*(i+1) - 1] != \
					'\0') {\
						ptr[NORMAL_STR_LEN*(i+1) - 1] =\
							'\0';\
						pr_info("%s", \
						  &ptr[NORMAL_STR_LEN*i]);\
					} else{\
						pr_info("%s", \
						  &ptr[NORMAL_STR_LEN*i]);\
						break;\
					} \
				} \
			} \
			else if (logT == _LOG_INF) {\
				for (i = 0; i < INF_PAGE; i++) {\
					if (ptr[NORMAL_STR_LEN*(i+1) - 1] != \
					'\0') {\
						ptr[NORMAL_STR_LEN*(i+1) - 1] =\
							'\0';\
						pr_info("%s", \
						  &ptr[NORMAL_STR_LEN*i]);\
					} else{\
						pr_info("%s", \
						  &ptr[NORMAL_STR_LEN*i]);\
						break;\
					} \
				} \
			} \
			else {\
				pr_info("N.S.%d", logT);\
			} \
			ptr[0] = '\0';\
			pSrc->_cnt[ppb][logT] = 0;\
			avaLen = str_leng - 1;\
			ptr = pDes = (char *)\
			    &(pSrc->_str[ppb][logT][pSrc->_cnt[ppb][logT]]);\
			ptr2 = &(pSrc->_cnt[ppb][logT]);\
			if (snprintf((char *)(pDes),\
				avaLen, fmt, ##__VA_ARGS__) < 0) {\
				pr_info("[Error] %s: snprintf failed",\
					__func__);\
			} \
			while (*ptr++ != '\0') {\
				(*ptr2)++;\
			} \
		} \
	} \
} while (0)

#else
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, args...) \
		pr_info(IRQTag fmt,  ##args)
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
					pr_info("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else{\
					pr_info("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else if (logT == _LOG_INF) {\
			for (i = 0; i < INF_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					pr_info("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else{\
					pr_info("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else {\
			pr_info("N.S.%d", logT);\
		} \
		ptr[0] = '\0';\
		pSrc->_cnt[ppb][logT] = 0;\
	} \
} while (0)

#else
#define IRQ_LOG_PRINTER(irq, ppb, logT)
#endif

/* //////////////////////////////////////////////////// */
union FBC_CTRL_1 {
	struct { /* 0x18004110 */
		unsigned int  FBC_NUM      :  6;      /*  0.. 5, 0x0000003F */
		unsigned int  rsv_6        :  9;      /*  6..14, 0x00007FC0 */
		unsigned int  FBC_EN       :  1;      /* 15..15, 0x00008000 */
		unsigned int  FBC_MODE     :  1;      /* 16..16, 0x00010000 */
		unsigned int  LOCK_EN      :  1;      /* 17..17, 0x00020000 */
		unsigned int  rsv_18       :  2;      /* 18..19, 0x000C0000 */
		unsigned int  DROP_TIMING  :  1;      /* 20..20, 0x00100000 */
		unsigned int  rsv_21       :  3;      /* 21..23, 0x00E00000 */
		unsigned int  SUB_RATIO    :  8;      /* 24..31, 0xFF000000 */
	} Bits;
	unsigned int Raw;
};  /* CAM_A_FBC_IMGO_CTL1 */

union FBC_CTRL_2 {
	struct { /* 0x18004114 */
		unsigned int  FBC_CNT      :  7;      /*  0.. 6, 0x0000007F */
		unsigned int  rsv_7        :  1;      /*  7.. 7, 0x00000080 */
		unsigned int  RCNT         :  6;      /*  8..13, 0x00003F00 */
		unsigned int  rsv_14       :  2;      /* 14..15, 0x0000C000 */
		unsigned int  WCNT         :  6;      /* 16..21, 0x003F0000 */
		unsigned int  rsv_22       :  2;      /* 22..23, 0x00C00000 */
		unsigned int  DROP_CNT     :  8;      /* 24..31, 0xFF000000 */
	} Bits;
	unsigned int Raw;
};  /* CAM_A_FBC_IMGO_CTL2 */


struct _isp_bk_reg_t {
	unsigned int  CAM_TG_INTER_ST;                                 /* 453C*/
};

static struct _isp_bk_reg_t g_BkReg[ISP_IRQ_TYPE_AMOUNT];

/* 6797 top registers */
#define CAMSYS_REG_CG_CON               (ISP_CAMSYS_CONFIG_BASE + 0x0)
#define IMGSYS_REG_CG_CON               (ISP_IMGSYS_CONFIG_BASE + 0x0)
#define CAMSYS_REG_CG_SET               (ISP_CAMSYS_CONFIG_BASE + 0x4)
#define IMGSYS_REG_CG_SET               (ISP_IMGSYS_CONFIG_BASE + 0x4)
#define CAMSYS_REG_CG_CLR               (ISP_CAMSYS_CONFIG_BASE + 0x8)
#define IMGSYS_REG_CG_CLR               (ISP_IMGSYS_CONFIG_BASE + 0x8)

/* 6768 CAMSYS */
#define CAMSYS_REG_HALT1_EN             (ISP_CAMSYS_CONFIG_BASE + 0x190)
#define CAMSYS_REG_HALT2_EN             (ISP_CAMSYS_CONFIG_BASE + 0x194)
#define CAMSYS_REG_HALT3_EN             (ISP_CAMSYS_CONFIG_BASE + 0x198)
#define CAMSYS_REG_HALT4_EN             (ISP_CAMSYS_CONFIG_BASE + 0x200)



static inline void *SecureError_CTL_EN(void)
{
	pr_info("ERROR:CAM_REG_CTL_EN is secure protect region can't read/write in camera-isp\n");
	return NULL;
}

static inline void *SecureError_DMA_EN(void)
{
	pr_info("ERROR:CAM_REG_CTL_DMA_EN is secure protect region can't read/write in camera-isp\n");
	return NULL;
}


static inline void *SecureError_CTL_SEL(void)
{
	pr_info("ERROR:CAM_REG_CTL_SEL is secure protect region can't read/write in camera-isp\n");
	return NULL;
}



/* 6797 CAM registers */
#define CAM_REG_CTL_START(module)  (isp_devs[module].regs + 0x0000)
#define CAM_REG_CTL_EN(module)  ((!sec_on) ? (isp_devs[module].regs + 0x0004) \
				: SecureError_CTL_EN())
#define CAM_REG_CTL_DMA_EN(module)  ((!sec_on) ? (isp_devs[module].regs + 0x0008) \
				: SecureError_DMA_EN())
#define CAM_REG_CTL_FMT_SEL(module)  (isp_devs[module].regs + 0x000C)
#define CAM_REG_CTL_SEL(module)  ((!sec_on) ? (isp_devs[module].regs + 0x0010) \
				: SecureError_CTL_SEL())
#define CAM_REG_CTL_MISC(module)  (isp_devs[module].regs + 0x0014)
#define CAM_REG_CTL_RAW_INT_EN(module)  (isp_devs[module].regs + 0x0020)
#define CAM_REG_CTL_RAW_INT_STATUS(module)  (isp_devs[module].regs + 0x0024)
#define CAM_REG_CTL_RAW_INT_STATUSX(module)  (isp_devs[module].regs + 0x0028)
#define CAM_REG_CTL_RAW_INT2_EN(module)  (isp_devs[module].regs + 0x0030)
#define CAM_REG_CTL_RAW_INT2_STATUS(module)  (isp_devs[module].regs + 0x0034)
#define CAM_REG_CTL_RAW_INT2_STATUSX(module)  (isp_devs[module].regs + 0x0038)
#define CAM_REG_CTL_SW_CTL(module)  (isp_devs[module].regs + 0x0040)
#define CAM_REG_CTL_AB_DONE_SEL(module)  (isp_devs[module].regs + 0x0044)
#define CAM_REG_CTL_CD_DONE_SEL(module)  (isp_devs[module].regs + 0x0048)
#define CAM_REG_CTL_UNI_DONE_SEL(module)  (isp_devs[module].regs + 0x004C)
#define CAM_REG_CTL_TWIN_STATUS(module)  (isp_devs[module].regs + 0x0050)
#define CAM_REG_CTL_SPARE1(module)  (isp_devs[module].regs + 0x0054)
#define CAM_REG_CTL_SPARE2(module)  (isp_devs[module].regs + 0x0058)
#define CAM_REG_CTL_SW_PASS1_DONE(module)  (isp_devs[module].regs + 0x005C)
#define CAM_REG_CTL_FBC_RCNT_INC(module)  (isp_devs[module].regs + 0x0060)
#define CAM_REG_CTL_DBG_SET(module)  (isp_devs[module].regs + 0x0070)
#define CAM_REG_CTL_DBG_PORT(module)  (isp_devs[module].regs + 0x0074)
#define CAM_REG_CTL_DATE_CODE(module)  (isp_devs[module].regs + 0x0078)
#define CAM_REG_CTL_PROJ_CODE(module)  (isp_devs[module].regs + 0x007C)
#define CAM_REG_CTL_RAW_DCM_DIS(module)  (isp_devs[module].regs + 0x0080)
#define CAM_REG_CTL_DMA_DCM_DIS(module)  (isp_devs[module].regs + 0x0084)
#define CAM_REG_CTL_TOP_DCM_DIS(module)  (isp_devs[module].regs + 0x0088)
#define CAM_REG_CTL_RAW_DCM_STATUS(module)  (isp_devs[module].regs + 0x0090)
#define CAM_REG_CTL_DMA_DCM_STATUS(module)  (isp_devs[module].regs + 0x0094)
#define CAM_REG_CTL_TOP_DCM_STATUS(module)  (isp_devs[module].regs + 0x0098)
#define CAM_REG_CTL_RAW_REQ_STATUS(module)  (isp_devs[module].regs + 0x00A0)
#define CAM_REG_CTL_DMA_REQ_STATUS(module)  (isp_devs[module].regs + 0x00A4)
#define CAM_REG_CTL_RAW_RDY_STATUS(module)  (isp_devs[module].regs + 0x00A8)
#define CAM_REG_CTL_DMA_RDY_STATUS(module)  (isp_devs[module].regs + 0x00AC)
#define CAM_REG_CTL_UNI_B_DONE_SEL(module)  (isp_devs[module].regs + 0x00D8)
#define CAM_REG_FBC_IMGO_CTL1(module)  (isp_devs[module].regs + 0x0110)
#define CAM_REG_FBC_IMGO_CTL2(module)  (isp_devs[module].regs + 0x0114)
#define CAM_REG_FBC_RRZO_CTL1(module)  (isp_devs[module].regs + 0x0118)
#define CAM_REG_FBC_RRZO_CTL2(module)  (isp_devs[module].regs + 0x011C)
#define CAM_REG_FBC_UFEO_CTL1(module)  (isp_devs[module].regs + 0x0120)
#define CAM_REG_FBC_UFEO_CTL2(module)  (isp_devs[module].regs + 0x0124)
#define CAM_REG_FBC_LCSO_CTL1(module)  (isp_devs[module].regs + 0x0128)
#define CAM_REG_FBC_LCSO_CTL2(module)  (isp_devs[module].regs + 0x012C)
#define CAM_REG_FBC_AFO_CTL1(module)  (isp_devs[module].regs + 0x0130)
#define CAM_REG_FBC_AFO_CTL2(module)  (isp_devs[module].regs + 0x0134)
#define CAM_REG_FBC_AAO_CTL1(module)  (isp_devs[module].regs + 0x0138)
#define CAM_REG_FBC_AAO_CTL2(module)  (isp_devs[module].regs + 0x013C)
#define CAM_REG_FBC_PDO_CTL1(module)  (isp_devs[module].regs + 0x0140)
#define CAM_REG_FBC_PDO_CTL2(module)  (isp_devs[module].regs + 0x0144)
#define CAM_REG_FBC_PSO_CTL1(module)  (isp_devs[module].regs + 0x0148)
#define CAM_REG_FBC_PSO_CTL2(module)  (isp_devs[module].regs + 0x014C)
#define CAM_REG_CQ_EN(module)  (isp_devs[module].regs + 0x0160)
#define CAM_REG_CQ_THR0_CTL(module)  (isp_devs[module].regs + 0x0164)
#define CAM_REG_CQ_THR0_BASEADDR(module)  (isp_devs[module].regs + 0x0168)
#define CAM_REG_CQ_THR0_DESC_SIZE(module)  (isp_devs[module].regs + 0x016C)
#define CAM_REG_CQ_THR1_CTL(module)  (isp_devs[module].regs + 0x0170)
#define CAM_REG_CQ_THR1_BASEADDR(module)  (isp_devs[module].regs + 0x0174)
#define CAM_REG_CQ_THR1_DESC_SIZE(module)  (isp_devs[module].regs + 0x0178)
#define CAM_REG_CQ_THR2_CTL(module)  (isp_devs[module].regs + 0x017C)
#define CAM_REG_CQ_THR2_BASEADDR(module)  (isp_devs[module].regs + 0x0180)
#define CAM_REG_CQ_THR2_DESC_SIZE(module)  (isp_devs[module].regs + 0x0184)
#define CAM_REG_CQ_THR3_CTL(module)  (isp_devs[module].regs + 0x0188)
#define CAM_REG_CQ_THR3_BASEADDR(module)  (isp_devs[module].regs + 0x018C)
#define CAM_REG_CQ_THR3_DESC_SIZE(module)  (isp_devs[module].regs + 0x0190)
#define CAM_REG_CQ_THR4_CTL(module)  (isp_devs[module].regs + 0x0194)
#define CAM_REG_CQ_THR4_BASEADDR(module)  (isp_devs[module].regs + 0x0198)
#define CAM_REG_CQ_THR4_DESC_SIZE(module)  (isp_devs[module].regs + 0x019C)
#define CAM_REG_CQ_THR5_CTL(module)  (isp_devs[module].regs + 0x01A0)
#define CAM_REG_CQ_THR5_BASEADDR(module)  (isp_devs[module].regs + 0x01A4)
#define CAM_REG_CQ_THR5_DESC_SIZE(module)  (isp_devs[module].regs + 0x01A8)
#define CAM_REG_CQ_THR6_CTL(module)  (isp_devs[module].regs + 0x01AC)
#define CAM_REG_CQ_THR6_BASEADDR(module)  (isp_devs[module].regs + 0x01B0)
#define CAM_REG_CQ_THR6_DESC_SIZE(module)  (isp_devs[module].regs + 0x01B4)
#define CAM_REG_CQ_THR7_CTL(module)  (isp_devs[module].regs + 0x01B8)
#define CAM_REG_CQ_THR7_BASEADDR(module)  (isp_devs[module].regs + 0x01BC)
#define CAM_REG_CQ_THR7_DESC_SIZE(module)  (isp_devs[module].regs + 0x01C0)
#define CAM_REG_CQ_THR8_CTL(module)  (isp_devs[module].regs + 0x01C4)
#define CAM_REG_CQ_THR8_BASEADDR(module)  (isp_devs[module].regs + 0x01C8)
#define CAM_REG_CQ_THR8_DESC_SIZE(module)  (isp_devs[module].regs + 0x01CC)
#define CAM_REG_CQ_THR9_CTL(module)  (isp_devs[module].regs + 0x01D0)
#define CAM_REG_CQ_THR9_BASEADDR(module)  (isp_devs[module].regs + 0x01D4)
#define CAM_REG_CQ_THR9_DESC_SIZE(module)  (isp_devs[module].regs + 0x01D8)
#define CAM_REG_CQ_THR10_CTL(module)  (isp_devs[module].regs + 0x01DC)
#define CAM_REG_CQ_THR10_BASEADDR(module)  (isp_devs[module].regs + 0x01E0)
#define CAM_REG_CQ_THR10_DESC_SIZE(module)  (isp_devs[module].regs + 0x01E4)
#define CAM_REG_CQ_THR11_CTL(module)  (isp_devs[module].regs + 0x01E8)
#define CAM_REG_CQ_THR11_BASEADDR(module)  (isp_devs[module].regs + 0x01EC)
#define CAM_REG_CQ_THR11_DESC_SIZE(module)  (isp_devs[module].regs + 0x01F0)
#define CAM_REG_CQ_THR12_CTL(module)  (isp_devs[module].regs + 0x01F4)
#define CAM_REG_CQ_THR12_BASEADDR(module)  (isp_devs[module].regs + 0x01F8)
#define CAM_REG_CQ_THR12_DESC_SIZE(module)  (isp_devs[module].regs + 0x01FC)
#define CAM_REG_DMA_SOFT_RSTSTAT(module)  (isp_devs[module].regs + 0x0200)
#define CAM_REG_CQ0I_BASE_ADDR(module)  (isp_devs[module].regs + 0x0204)
#define CAM_REG_CQ0I_XSIZE(module)  (isp_devs[module].regs + 0x0208)
#define CAM_REG_VERTICAL_FLIP_EN(module)  (isp_devs[module].regs + 0x020C)
#define CAM_REG_DMA_SOFT_RESET(module)  (isp_devs[module].regs + 0x0210)
#define CAM_REG_LAST_ULTRA_EN(module)  (isp_devs[module].regs + 0x0214)
#define CAM_REG_SPECIAL_FUN_EN(module)  (isp_devs[module].regs + 0x0218)
#define CAM_REG_IMGO_BASE_ADDR(module)  (isp_devs[module].regs + 0x0220)
#define CAM_REG_IMGO_OFST_ADDR(module)  (isp_devs[module].regs + 0x0228)
#define CAM_REG_IMGO_XSIZE(module)  (isp_devs[module].regs + 0x0230)
#define CAM_REG_IMGO_YSIZE(module)  (isp_devs[module].regs + 0x0234)
#define CAM_REG_IMGO_STRIDE(module)  (isp_devs[module].regs + 0x0238)
#define CAM_REG_IMGO_CON(module)  (isp_devs[module].regs + 0x023C)
#define CAM_REG_IMGO_CON2(module)  (isp_devs[module].regs + 0x0240)
#define CAM_REG_IMGO_CON3(module)  (isp_devs[module].regs + 0x0244)
#define CAM_REG_IMGO_CROP(module)  (isp_devs[module].regs + 0x0248)
#define CAM_REG_RRZO_BASE_ADDR(module)  (isp_devs[module].regs + 0x0250)
#define CAM_REG_RRZO_OFST_ADDR(module)  (isp_devs[module].regs + 0x0258)
#define CAM_REG_RRZO_XSIZE(module)  (isp_devs[module].regs + 0x0260)
#define CAM_REG_RRZO_YSIZE(module)  (isp_devs[module].regs + 0x0264)
#define CAM_REG_RRZO_STRIDE(module)  (isp_devs[module].regs + 0x0268)
#define CAM_REG_RRZO_CON(module)  (isp_devs[module].regs + 0x026C)
#define CAM_REG_RRZO_CON2(module)  (isp_devs[module].regs + 0x0270)
#define CAM_REG_RRZO_CON3(module)  (isp_devs[module].regs + 0x0274)
#define CAM_REG_RRZO_CROP(module)  (isp_devs[module].regs + 0x0278)
#define CAM_REG_AAO_BASE_ADDR(module)  (isp_devs[module].regs + 0x0280)
#define CAM_REG_AAO_OFST_ADDR(module)  (isp_devs[module].regs + 0x0288)
#define CAM_REG_AAO_XSIZE(module)  (isp_devs[module].regs + 0x0290)
#define CAM_REG_AAO_YSIZE(module)  (isp_devs[module].regs + 0x0294)
#define CAM_REG_AAO_STRIDE(module)  (isp_devs[module].regs + 0x0298)
#define CAM_REG_AAO_CON(module)  (isp_devs[module].regs + 0x029C)
#define CAM_REG_AAO_CON2(module)  (isp_devs[module].regs + 0x02A0)
#define CAM_REG_AAO_CON3(module)  (isp_devs[module].regs + 0x02A4)
#define CAM_REG_AFO_BASE_ADDR(module)  (isp_devs[module].regs + 0x02B0)
#define CAM_REG_AFO_OFST_ADDR(module)  (isp_devs[module].regs + 0x02B8)
#define CAM_REG_AFO_XSIZE(module)  (isp_devs[module].regs + 0x02C0)
#define CAM_REG_AFO_YSIZE(module)  (isp_devs[module].regs + 0x02C4)
#define CAM_REG_AFO_STRIDE(module)  (isp_devs[module].regs + 0x02C8)
#define CAM_REG_AFO_CON(module)  (isp_devs[module].regs + 0x02CC)
#define CAM_REG_AFO_CON2(module)  (isp_devs[module].regs + 0x02D0)
#define CAM_REG_AFO_CON3(module)  (isp_devs[module].regs + 0x02D4)
#define CAM_REG_LCSO_BASE_ADDR(module)  (isp_devs[module].regs + 0x02E0)
#define CAM_REG_LCSO_OFST_ADDR(module)  (isp_devs[module].regs + 0x02E8)
#define CAM_REG_LCSO_XSIZE(module)  (isp_devs[module].regs + 0x02F0)
#define CAM_REG_LCSO_YSIZE(module)  (isp_devs[module].regs + 0x02F4)
#define CAM_REG_LCSO_STRIDE(module)  (isp_devs[module].regs + 0x02F8)
#define CAM_REG_LCSO_CON(module)  (isp_devs[module].regs + 0x02FC)
#define CAM_REG_LCSO_CON2(module)  (isp_devs[module].regs + 0x0300)
#define CAM_REG_LCSO_CON3(module)  (isp_devs[module].regs + 0x0304)
#define CAM_REG_UFEO_BASE_ADDR(module)  (isp_devs[module].regs + 0x0310)
#define CAM_REG_UFEO_OFST_ADDR(module)  (isp_devs[module].regs + 0x0318)
#define CAM_REG_UFEO_XSIZE(module)  (isp_devs[module].regs + 0x0320)
#define CAM_REG_UFEO_YSIZE(module)  (isp_devs[module].regs + 0x0324)
#define CAM_REG_UFEO_STRIDE(module)  (isp_devs[module].regs + 0x0328)
#define CAM_REG_UFEO_CON(module)  (isp_devs[module].regs + 0x032C)
#define CAM_REG_UFEO_CON2(module)  (isp_devs[module].regs + 0x0330)
#define CAM_REG_UFEO_CON3(module)  (isp_devs[module].regs + 0x0334)
#define CAM_REG_PDO_BASE_ADDR(module)  (isp_devs[module].regs + 0x0340)
#define CAM_REG_PDO_OFST_ADDR(module)  (isp_devs[module].regs + 0x0348)
#define CAM_REG_PDO_XSIZE(module)  (isp_devs[module].regs + 0x0350)
#define CAM_REG_PDO_YSIZE(module)  (isp_devs[module].regs + 0x0354)
#define CAM_REG_PDO_STRIDE(module)  (isp_devs[module].regs + 0x0358)
#define CAM_REG_PDO_CON(module)  (isp_devs[module].regs + 0x035C)
#define CAM_REG_PDO_CON2(module)  (isp_devs[module].regs + 0x0360)
#define CAM_REG_PDO_CON3(module)  (isp_devs[module].regs + 0x0364)
#define CAM_REG_BPCI_BASE_ADDR(module)  (isp_devs[module].regs + 0x0370)
#define CAM_REG_BPCI_OFST_ADDR(module)  (isp_devs[module].regs + 0x0378)
#define CAM_REG_BPCI_XSIZE(module)  (isp_devs[module].regs + 0x0380)
#define CAM_REG_BPCI_YSIZE(module)  (isp_devs[module].regs + 0x0384)
#define CAM_REG_BPCI_STRIDE(module)  (isp_devs[module].regs + 0x0388)
#define CAM_REG_BPCI_CON(module)  (isp_devs[module].regs + 0x038C)
#define CAM_REG_BPCI_CON2(module)  (isp_devs[module].regs + 0x0390)
#define CAM_REG_BPCI_CON3(module)  (isp_devs[module].regs + 0x0394)
#define CAM_REG_CACI_BASE_ADDR(module)  (isp_devs[module].regs + 0x03A0)
#define CAM_REG_CACI_OFST_ADDR(module)  (isp_devs[module].regs + 0x03A8)
#define CAM_REG_CACI_XSIZE(module)  (isp_devs[module].regs + 0x03B0)
#define CAM_REG_CACI_YSIZE(module)  (isp_devs[module].regs + 0x03B4)
#define CAM_REG_CACI_STRIDE(module)  (isp_devs[module].regs + 0x03B8)
#define CAM_REG_CACI_CON(module)  (isp_devs[module].regs + 0x03BC)
#define CAM_REG_CACI_CON2(module)  (isp_devs[module].regs + 0x03C0)
#define CAM_REG_CACI_CON3(module)  (isp_devs[module].regs + 0x03C4)
#define CAM_REG_LSCI_BASE_ADDR(module)  (isp_devs[module].regs + 0x03D0)
#define CAM_REG_LSCI_OFST_ADDR(module)  (isp_devs[module].regs + 0x03D8)
#define CAM_REG_LSCI_XSIZE(module)  (isp_devs[module].regs + 0x03E0)
#define CAM_REG_LSCI_YSIZE(module)  (isp_devs[module].regs + 0x03E4)
#define CAM_REG_LSCI_STRIDE(module)  (isp_devs[module].regs + 0x03E8)
#define CAM_REG_LSCI_CON(module)  (isp_devs[module].regs + 0x03EC)
#define CAM_REG_LSCI_CON2(module)  (isp_devs[module].regs + 0x03F0)
#define CAM_REG_LSCI_CON3(module)  (isp_devs[module].regs + 0x03F4)
#define CAM_REG_LSC3I_BASE_ADDR(module)  (isp_devs[module].regs + 0x0400)
#define CAM_REG_LSC3I_OFST_ADDR(module)  (isp_devs[module].regs + 0x0408)
#define CAM_REG_LSC3I_XSIZE(module)  (isp_devs[module].regs + 0x0410)
#define CAM_REG_LSC3I_YSIZE(module)  (isp_devs[module].regs + 0x0414)
#define CAM_REG_LSC3I_STRIDE(module)  (isp_devs[module].regs + 0x0418)
#define CAM_REG_LSC3I_CON(module)  (isp_devs[module].regs + 0x041C)
#define CAM_REG_LSC3I_CON2(module)  (isp_devs[module].regs + 0x0420)
#define CAM_REG_LSC3I_CON3(module)  (isp_devs[module].regs + 0x0424)
#define CAM_REG_DMA_ERR_CTRL(module)  (isp_devs[module].regs + 0x0440)
#define CAM_REG_IMGO_ERR_STAT(module)  (isp_devs[module].regs + 0x0444)
#define CAM_REG_RRZO_ERR_STAT(module)  (isp_devs[module].regs + 0x0448)
#define CAM_REG_AAO_ERR_STAT(module)  (isp_devs[module].regs + 0x044C)
#define CAM_REG_AFO_ERR_STAT(module)  (isp_devs[module].regs + 0x0450)
#define CAM_REG_LCSO_ERR_STAT(module)  (isp_devs[module].regs + 0x0454)
#define CAM_REG_UFEO_ERR_STAT(module)  (isp_devs[module].regs + 0x0458)
#define CAM_REG_PDO_ERR_STAT(module)  (isp_devs[module].regs + 0x045C)
#define CAM_REG_BPCI_ERR_STAT(module)  (isp_devs[module].regs + 0x0460)
#define CAM_REG_CACI_ERR_STAT(module)  (isp_devs[module].regs + 0x0464)
#define CAM_REG_LSCI_ERR_STAT(module)  (isp_devs[module].regs + 0x0468)
#define CAM_REG_LSC3I_ERR_STAT(module)  (isp_devs[module].regs + 0x046C)
#define CAM_REG_DMA_DEBUG_ADDR(module)  (isp_devs[module].regs + 0x0470)
#define CAM_REG_PSO_ERR_STAT(module)  (isp_devs[module].regs + 0x0DAC)
#define CAM_REG_DMA_RSV1(module)  (isp_devs[module].regs + 0x0470)
#define CAM_REG_DMA_RSV2(module)  (isp_devs[module].regs + 0x0474)
#define CAM_REG_DMA_RSV3(module)  (isp_devs[module].regs + 0x0478)
#define CAM_REG_DMA_RSV4(module)  (isp_devs[module].regs + 0x047C)
#define CAM_REG_DMA_RSV5(module)  (isp_devs[module].regs + 0x0480)
#define CAM_REG_DMA_RSV6(module)  (isp_devs[module].regs + 0x0484)
#define CAM_REG_DMA_DEBUG_SEL(module)  (isp_devs[module].regs + 0x0488)
#define CAM_REG_DMA_BW_SELF_TEST(module)  (isp_devs[module].regs + 0x048C)
#define CAM_REG_TG_SEN_MODE(module)  (isp_devs[module].regs + 0x0500)
#define CAM_REG_TG_VF_CON(module)  (isp_devs[module].regs + 0x0504)
#define CAM_REG_TG_SEN_GRAB_PXL(module)  (isp_devs[module].regs + 0x0508)
#define CAM_REG_TG_SEN_GRAB_LIN(module)  (isp_devs[module].regs + 0x050C)
#define CAM_REG_TG_PATH_CFG(module)  (isp_devs[module].regs + 0x0510)
#define CAM_REG_TG_MEMIN_CTL(module)  (isp_devs[module].regs + 0x0514)
#define CAM_REG_TG_INT1(module)  (isp_devs[module].regs + 0x0518)
#define CAM_REG_TG_INT2(module)  (isp_devs[module].regs + 0x051C)
#define CAM_REG_TG_SOF_CNT(module)  (isp_devs[module].regs + 0x0520)
#define CAM_REG_TG_SOT_CNT(module)  (isp_devs[module].regs + 0x0524)
#define CAM_REG_TG_EOT_CNT(module)  (isp_devs[module].regs + 0x0528)
#define CAM_REG_TG_ERR_CTL(module)  (isp_devs[module].regs + 0x052C)
#define CAM_REG_TG_DAT_NO(module)  (isp_devs[module].regs + 0x0530)
#define CAM_REG_TG_FRM_CNT_ST(module)  (isp_devs[module].regs + 0x0534)
#define CAM_REG_TG_FRMSIZE_ST(module)  (isp_devs[module].regs + 0x0538)
#define CAM_REG_TG_INTER_ST(module)  (isp_devs[module].regs + 0x053C)
#define CAM_REG_TG_FLASHA_CTL(module)  (isp_devs[module].regs + 0x0540)
#define CAM_REG_TG_FLASHA_LINE_CNT(module)  (isp_devs[module].regs + 0x0544)
#define CAM_REG_TG_FLASHA_POS(module)  (isp_devs[module].regs + 0x0548)
#define CAM_REG_TG_FLASHB_CTL(module)  (isp_devs[module].regs + 0x054C)
#define CAM_REG_TG_FLASHB_LINE_CNT(module)  (isp_devs[module].regs + 0x0550)
#define CAM_REG_TG_FLASHB_POS(module)  (isp_devs[module].regs + 0x0554)
#define CAM_REG_TG_FLASHB_POS1(module)  (isp_devs[module].regs + 0x0558)
#define CAM_REG_TG_I2C_CQ_TRIG(module)  (isp_devs[module].regs + 0x0560)
#define CAM_REG_TG_CQ_TIMING(module)  (isp_devs[module].regs + 0x0564)
#define CAM_REG_TG_CQ_NUM(module)  (isp_devs[module].regs + 0x0568)
#define CAM_REG_TG_TIME_STAMP(module)  (isp_devs[module].regs + 0x0570)
#define CAM_REG_TG_SUB_PERIOD(module)  (isp_devs[module].regs + 0x0574)
#define CAM_REG_TG_DAT_NO_R(module)  (isp_devs[module].regs + 0x0578)
#define CAM_REG_TG_FRMSIZE_ST_R(module)  (isp_devs[module].regs + 0x057C)
#define CAM_REG_DMX_CTL(module)  (isp_devs[module].regs + 0x0580)
#define CAM_REG_DMX_CROP(module)  (isp_devs[module].regs + 0x0584)
#define CAM_REG_DMX_VSIZE(module)  (isp_devs[module].regs + 0x0588)
#define CAM_REG_RMG_HDR_CFG(module)  (isp_devs[module].regs + 0x05A0)
#define CAM_REG_RMG_HDR_GAIN(module)  (isp_devs[module].regs + 0x05A4)
#define CAM_REG_RMG_HDR_CFG2(module)  (isp_devs[module].regs + 0x005A8)
#define CAM_REG_LCS25_CON(module)  (isp_devs[module].regs + 0x0880)
#define CAM_REG_LCS25_ST(module)  (isp_devs[module].regs + 0x0884)
#define CAM_REG_LCS25_AWS(module)  (isp_devs[module].regs + 0x0888)
#define CAM_REG_LCS25_FLR(module)  (isp_devs[module].regs + 0x088C)
#define CAM_REG_LCS25_LRZR_1(module)  (isp_devs[module].regs + 0x0890)
#define CAM_REG_LCS25_LRZR_2(module)  (isp_devs[module].regs + 0x0894)
#define CAM_REG_LCS25_SATU_1(module)  (isp_devs[module].regs + 0x0898)
#define CAM_REG_LCS25_SATU_2(module)  (isp_devs[module].regs + 0x089C)
#define CAM_REG_LCS25_GAIN_1(module)  (isp_devs[module].regs + 0x08A0)
#define CAM_REG_LCS25_GAIN_2(module)  (isp_devs[module].regs + 0x08A4)
#define CAM_REG_LCS25_OFST_1(module)  (isp_devs[module].regs + 0x08A8)
#define CAM_REG_LCS25_OFST_2(module)  (isp_devs[module].regs + 0x08AC)
#define CAM_REG_LCS25_G2G_CNV_1(module)  (isp_devs[module].regs + 0x08B0)
#define CAM_REG_LCS25_G2G_CNV_2(module)  (isp_devs[module].regs + 0x08B4)
#define CAM_REG_LCS25_G2G_CNV_3(module)  (isp_devs[module].regs + 0x08B8)
#define CAM_REG_LCS25_G2G_CNV_4(module)  (isp_devs[module].regs + 0x08BC)
#define CAM_REG_LCS25_G2G_CNV_5(module)  (isp_devs[module].regs + 0x08C0)
#define CAM_REG_LCS25_LPF(module)  (isp_devs[module].regs + 0x08C4)
#define CAM_REG_RMM_OSC(module)  (isp_devs[module].regs + 0x05C0)
#define CAM_REG_RMM_MC(module)  (isp_devs[module].regs + 0x05C4)
#define CAM_REG_RMM_REVG_1(module)  (isp_devs[module].regs + 0x05C8)
#define CAM_REG_RMM_REVG_2(module)  (isp_devs[module].regs + 0x05CC)
#define CAM_REG_RMM_LEOS(module)  (isp_devs[module].regs + 0x05D0)
#define CAM_REG_RMM_MC2(module)  (isp_devs[module].regs + 0x05D4)
#define CAM_REG_RMM_DIFF_LB(module)  (isp_devs[module].regs + 0x05D8)
#define CAM_REG_RMM_MA(module)  (isp_devs[module].regs + 0x05DC)
#define CAM_REG_RMM_TUNE(module)  (isp_devs[module].regs + 0x05E0)
#define CAM_REG_OBC_OFFST0(module)  (isp_devs[module].regs + 0x05F0)
#define CAM_REG_OBC_OFFST1(module)  (isp_devs[module].regs + 0x05F4)
#define CAM_REG_OBC_OFFST2(module)  (isp_devs[module].regs + 0x05F8)
#define CAM_REG_OBC_OFFST3(module)  (isp_devs[module].regs + 0x05FC)
#define CAM_REG_OBC_GAIN0(module)  (isp_devs[module].regs + 0x0600)
#define CAM_REG_OBC_GAIN1(module)  (isp_devs[module].regs + 0x0604)
#define CAM_REG_OBC_GAIN2(module)  (isp_devs[module].regs + 0x0608)
#define CAM_REG_OBC_GAIN3(module)  (isp_devs[module].regs + 0x060C)
#define CAM_REG_BNR_BPC_CON(module)  (isp_devs[module].regs + 0x0620)
#define CAM_REG_BNR_BPC_TH1(module)  (isp_devs[module].regs + 0x0624)
#define CAM_REG_BNR_BPC_TH2(module)  (isp_devs[module].regs + 0x0628)
#define CAM_REG_BNR_BPC_TH3(module)  (isp_devs[module].regs + 0x062C)
#define CAM_REG_BNR_BPC_TH4(module)  (isp_devs[module].regs + 0x0630)
#define CAM_REG_BNR_BPC_DTC(module)  (isp_devs[module].regs + 0x0634)
#define CAM_REG_BNR_BPC_COR(module)  (isp_devs[module].regs + 0x0638)
#define CAM_REG_BNR_BPC_TBLI1(module)  (isp_devs[module].regs + 0x063C)
#define CAM_REG_BNR_BPC_TBLI2(module)  (isp_devs[module].regs + 0x0640)
#define CAM_REG_BNR_BPC_TH1_C(module)  (isp_devs[module].regs + 0x0644)
#define CAM_REG_BNR_BPC_TH2_C(module)  (isp_devs[module].regs + 0x0648)
#define CAM_REG_BNR_BPC_TH3_C(module)  (isp_devs[module].regs + 0x064C)
#define CAM_REG_BNR_NR1_CON(module)  (isp_devs[module].regs + 0x0650)
#define CAM_REG_BNR_NR1_CT_CON(module)  (isp_devs[module].regs + 0x0654)
#define CAM_REG_BNR_RSV1(module)  (isp_devs[module].regs + 0x0658)
#define CAM_REG_BNR_RSV2(module)  (isp_devs[module].regs + 0x065C)
#define CAM_REG_BNR_PDC_CON(module)  (isp_devs[module].regs + 0x0660)
#define CAM_REG_BNR_PDC_GAIN_L0(module)  (isp_devs[module].regs + 0x0664)
#define CAM_REG_BNR_PDC_GAIN_L1(module)  (isp_devs[module].regs + 0x0668)
#define CAM_REG_BNR_PDC_GAIN_L2(module)  (isp_devs[module].regs + 0x066C)
#define CAM_REG_BNR_PDC_GAIN_L3(module)  (isp_devs[module].regs + 0x0670)
#define CAM_REG_BNR_PDC_GAIN_L4(module)  (isp_devs[module].regs + 0x0674)
#define CAM_REG_BNR_PDC_GAIN_R0(module)  (isp_devs[module].regs + 0x0678)
#define CAM_REG_BNR_PDC_GAIN_R1(module)  (isp_devs[module].regs + 0x067C)
#define CAM_REG_BNR_PDC_GAIN_R2(module)  (isp_devs[module].regs + 0x0680)
#define CAM_REG_BNR_PDC_GAIN_R3(module)  (isp_devs[module].regs + 0x0684)
#define CAM_REG_BNR_PDC_GAIN_R4(module)  (isp_devs[module].regs + 0x0688)
#define CAM_REG_BNR_PDC_TH_GB(module)  (isp_devs[module].regs + 0x068C)
#define CAM_REG_BNR_PDC_TH_IA(module)  (isp_devs[module].regs + 0x0690)
#define CAM_REG_BNR_PDC_TH_HD(module)  (isp_devs[module].regs + 0x0694)
#define CAM_REG_BNR_PDC_SL(module)  (isp_devs[module].regs + 0x0698)
#define CAM_REG_BNR_PDC_POS(module)  (isp_devs[module].regs + 0x069C)
#define CAM_REG_LSC_CTL1(module)  (isp_devs[module].regs + 0x06A0)
#define CAM_REG_LSC_CTL2(module)  (isp_devs[module].regs + 0x06A4)
#define CAM_REG_LSC_CTL3(module)  (isp_devs[module].regs + 0x06A8)
#define CAM_REG_LSC_LBLOCK(module)  (isp_devs[module].regs + 0x06AC)
#define CAM_REG_LSC_RATIO_0(module)  (isp_devs[module].regs + 0x07E0)
#define CAM_REG_LSC_RATIO(module)  (isp_devs[module].regs + 0x06B0)
#define CAM_REG_LSC_TPIPE_OFST(module)  (isp_devs[module].regs + 0x06B4)
#define CAM_REG_LSC_TPIPE_SIZE(module)  (isp_devs[module].regs + 0x06B8)
#define CAM_REG_LSC_GAIN_TH(module)  (isp_devs[module].regs + 0x06BC)
#define CAM_REG_LSC_RATIO_1(module)  (isp_devs[module].regs + 0x07F0)
#define CAM_REG_RPG_SATU_1(module)  (isp_devs[module].regs + 0x06C0)
#define CAM_REG_RPG_SATU_2(module)  (isp_devs[module].regs + 0x06C4)
#define CAM_REG_RPG_GAIN_1(module)  (isp_devs[module].regs + 0x06C8)
#define CAM_REG_RPG_GAIN_2(module)  (isp_devs[module].regs + 0x06CC)
#define CAM_REG_RPG_OFST_1(module)  (isp_devs[module].regs + 0x06D0)
#define CAM_REG_RPG_OFST_2(module)  (isp_devs[module].regs + 0x06D4)
#define CAM_REG_RRZ_CTL(module)  (isp_devs[module].regs + 0x06E0)
#define CAM_REG_RRZ_IN_IMG(module)  (isp_devs[module].regs + 0x06E4)
#define CAM_REG_RRZ_OUT_IMG(module)  (isp_devs[module].regs + 0x06E8)
#define CAM_REG_RRZ_HORI_STEP(module)  (isp_devs[module].regs + 0x06EC)
#define CAM_REG_RRZ_VERT_STEP(module)  (isp_devs[module].regs + 0x06F0)
#define CAM_REG_RRZ_HORI_INT_OFST(module)  (isp_devs[module].regs + 0x06F4)
#define CAM_REG_RRZ_HORI_SUB_OFST(module)  (isp_devs[module].regs + 0x06F8)
#define CAM_REG_RRZ_VERT_INT_OFST(module)  (isp_devs[module].regs + 0x06FC)
#define CAM_REG_RRZ_VERT_SUB_OFST(module)  (isp_devs[module].regs + 0x0700)
#define CAM_REG_RRZ_MODE_TH(module)  (isp_devs[module].regs + 0x0704)
#define CAM_REG_RRZ_MODE_CTL(module)  (isp_devs[module].regs + 0x0708)
#define CAM_REG_RMX_CTL(module)  (isp_devs[module].regs + 0x0740)
#define CAM_REG_RMX_CROP(module)  (isp_devs[module].regs + 0x0744)
#define CAM_REG_RMX_VSIZE(module)  (isp_devs[module].regs + 0x0748)
#define CAM_REG_SGG5_PGN(module)  (isp_devs[module].regs + 0x0760)
#define CAM_REG_SGG5_GMRC_1(module)  (isp_devs[module].regs + 0x0764)
#define CAM_REG_SGG5_GMRC_2(module)  (isp_devs[module].regs + 0x0768)
#define CAM_REG_BMX_CTL(module)  (isp_devs[module].regs + 0x0780)
#define CAM_REG_BMX_CROP(module)  (isp_devs[module].regs + 0x0784)
#define CAM_REG_BMX_VSIZE(module)  (isp_devs[module].regs + 0x0788)
#define CAM_REG_UFE_CON(module)  (isp_devs[module].regs + 0x07C0)
#define CAM_REG_LCS_CON(module)  (isp_devs[module].regs + 0x07E0)
#define CAM_REG_LCS_ST(module)  (isp_devs[module].regs + 0x07E4)
#define CAM_REG_LCS_AWS(module)  (isp_devs[module].regs + 0x07E8)
#define CAM_REG_LCS_FLR(module)  (isp_devs[module].regs + 0x07EC)
#define CAM_REG_LCS_LRZR_1(module)  (isp_devs[module].regs + 0x07F0)
#define CAM_REG_LCS_LRZR_2(module)  (isp_devs[module].regs + 0x07F4)
#define CAM_REG_AF_CON(module)  (isp_devs[module].regs + 0x0800)
#define CAM_REG_AF_TH_0(module)  (isp_devs[module].regs + 0x0804)
#define CAM_REG_AF_TH_1(module)  (isp_devs[module].regs + 0x0808)
#define CAM_REG_AF_FLT_1(module)  (isp_devs[module].regs + 0x080C)
#define CAM_REG_AF_FLT_2(module)  (isp_devs[module].regs + 0x0810)
#define CAM_REG_AF_FLT_3(module)  (isp_devs[module].regs + 0x0814)
#define CAM_REG_AF_FLT_4(module)  (isp_devs[module].regs + 0x0818)
#define CAM_REG_AF_FLT_5(module)  (isp_devs[module].regs + 0x081C)
#define CAM_REG_AF_FLT_6(module)  (isp_devs[module].regs + 0x0820)
#define CAM_REG_AF_FLT_7(module)  (isp_devs[module].regs + 0x0824)
#define CAM_REG_AF_FLT_8(module)  (isp_devs[module].regs + 0x0828)
#define CAM_REG_AF_SIZE(module)  (isp_devs[module].regs + 0x0830)
#define CAM_REG_AF_VLD(module)  (isp_devs[module].regs + 0x0834)
#define CAM_REG_AF_BLK_0(module)  (isp_devs[module].regs + 0x0838)
#define CAM_REG_AF_BLK_1(module)  (isp_devs[module].regs + 0x083C)
#define CAM_REG_AF_TH_2(module)  (isp_devs[module].regs + 0x0840)
#define CAM_REG_RCP_CROP_CON1(module)  (isp_devs[module].regs + 0x08F0)
#define CAM_REG_RCP_CROP_CON2(module)  (isp_devs[module].regs + 0x08F4)
#define CAM_REG_SGG1_PGN(module)  (isp_devs[module].regs + 0x0900)
#define CAM_REG_SGG1_GMRC_1(module)  (isp_devs[module].regs + 0x0904)
#define CAM_REG_SGG1_GMRC_2(module)  (isp_devs[module].regs + 0x0908)
#define CAM_REG_QBN2_MODE(module)  (isp_devs[module].regs + 0x0910)
#define CAM_REG_AWB_WIN_ORG(module)  (isp_devs[module].regs + 0x0920)
#define CAM_REG_AWB_WIN_SIZE(module)  (isp_devs[module].regs + 0x0924)
#define CAM_REG_AWB_WIN_PIT(module)  (isp_devs[module].regs + 0x0928)
#define CAM_REG_AWB_WIN_NUM(module)  (isp_devs[module].regs + 0x092C)
#define CAM_REG_AWB_GAIN1_0(module)  (isp_devs[module].regs + 0x0930)
#define CAM_REG_AWB_GAIN1_1(module)  (isp_devs[module].regs + 0x0934)
#define CAM_REG_AWB_LMT1_0(module)  (isp_devs[module].regs + 0x0938)
#define CAM_REG_AWB_LMT1_1(module)  (isp_devs[module].regs + 0x093C)
#define CAM_REG_AWB_LOW_THR(module)  (isp_devs[module].regs + 0x0940)
#define CAM_REG_AWB_HI_THR(module)  (isp_devs[module].regs + 0x0944)
#define CAM_REG_AWB_PIXEL_CNT0(module)  (isp_devs[module].regs + 0x0948)
#define CAM_REG_AWB_PIXEL_CNT1(module)  (isp_devs[module].regs + 0x094C)
#define CAM_REG_AWB_PIXEL_CNT2(module)  (isp_devs[module].regs + 0x0950)
#define CAM_REG_AWB_ERR_THR(module)  (isp_devs[module].regs + 0x0954)
#define CAM_REG_AWB_ROT(module)  (isp_devs[module].regs + 0x0958)
#define CAM_REG_AWB_L0_X(module)  (isp_devs[module].regs + 0x095C)
#define CAM_REG_AWB_L0_Y(module)  (isp_devs[module].regs + 0x0960)
#define CAM_REG_AWB_L1_X(module)  (isp_devs[module].regs + 0x0964)
#define CAM_REG_AWB_L1_Y(module)  (isp_devs[module].regs + 0x0968)
#define CAM_REG_AWB_L2_X(module)  (isp_devs[module].regs + 0x096C)
#define CAM_REG_AWB_L2_Y(module)  (isp_devs[module].regs + 0x0970)
#define CAM_REG_AWB_L3_X(module)  (isp_devs[module].regs + 0x0974)
#define CAM_REG_AWB_L3_Y(module)  (isp_devs[module].regs + 0x0978)
#define CAM_REG_AWB_L4_X(module)  (isp_devs[module].regs + 0x097C)
#define CAM_REG_AWB_L4_Y(module)  (isp_devs[module].regs + 0x0980)
#define CAM_REG_AWB_L5_X(module)  (isp_devs[module].regs + 0x0984)
#define CAM_REG_AWB_L5_Y(module)  (isp_devs[module].regs + 0x0988)
#define CAM_REG_AWB_L6_X(module)  (isp_devs[module].regs + 0x098C)
#define CAM_REG_AWB_L6_Y(module)  (isp_devs[module].regs + 0x0990)
#define CAM_REG_AWB_L7_X(module)  (isp_devs[module].regs + 0x0994)
#define CAM_REG_AWB_L7_Y(module)  (isp_devs[module].regs + 0x0998)
#define CAM_REG_AWB_L8_X(module)  (isp_devs[module].regs + 0x099C)
#define CAM_REG_AWB_L8_Y(module)  (isp_devs[module].regs + 0x09A0)
#define CAM_REG_AWB_L9_X(module)  (isp_devs[module].regs + 0x09A4)
#define CAM_REG_AWB_L9_Y(module)  (isp_devs[module].regs + 0x09A8)
#define CAM_REG_AWB_SPARE(module)  (isp_devs[module].regs + 0x09AC)
#define CAM_REG_AWB_MOTION_THR(module)  (isp_devs[module].regs + 0x09B0)
#define CAM_REG_AWB_RC_CNV_0(module)  (isp_devs[module].regs + 0x09B4)
#define CAM_REG_AWB_RC_CNV_1(module)  (isp_devs[module].regs + 0x09B8)
#define CAM_REG_AWB_RC_CNV_2(module)  (isp_devs[module].regs + 0x09BC)
#define CAM_REG_AWB_RC_CNV_3(module)  (isp_devs[module].regs + 0x09C0)
#define CAM_REG_AWB_RC_CNV_4(module)  (isp_devs[module].regs + 0x09C4)
#define CAM_REG_AE_HST_CTL(module)  (isp_devs[module].regs + 0x09C0)
#define CAM_REG_AE_GAIN2_0(module)  (isp_devs[module].regs + 0x09E0)
#define CAM_REG_AE_GAIN2_1(module)  (isp_devs[module].regs + 0x09E4)
#define CAM_REG_AE_LMT2_0(module)  (isp_devs[module].regs + 0x09E8)
#define CAM_REG_AE_LMT2_1(module)  (isp_devs[module].regs + 0x09EC)
#define CAM_REG_AE_RC_CNV_0(module)  (isp_devs[module].regs + 0x09F0)
#define CAM_REG_AE_RC_CNV_1(module)  (isp_devs[module].regs + 0x09F4)
#define CAM_REG_AE_RC_CNV_2(module)  (isp_devs[module].regs + 0x09F8)
#define CAM_REG_AE_RC_CNV_3(module)  (isp_devs[module].regs + 0x09FC)
#define CAM_REG_AE_RC_CNV_4(module)  (isp_devs[module].regs + 0x0A00)
#define CAM_REG_AE_YGAMMA_0(module)  (isp_devs[module].regs + 0x0A04)
#define CAM_REG_AE_YGAMMA_1(module)  (isp_devs[module].regs + 0x0A08)
#define CAM_REG_AE_HST0_RNG(module)  (isp_devs[module].regs + 0x09F4)
#define CAM_REG_AE_HST1_RNG(module)  (isp_devs[module].regs + 0x09F8)
#define CAM_REG_AE_HST2_RNG(module)  (isp_devs[module].regs + 0x09FC)
#define CAM_REG_AE_HST3_RNG(module)  (isp_devs[module].regs + 0x0A00)
#define CAM_REG_AE_OVER_EXPO_CFG(module)  (isp_devs[module].regs + 0x0A0C)
#define CAM_REG_AE_PIX_HST_CTL(module)    (isp_devs[module].regs + 0x0A10)
#define CAM_REG_AE_PIX_HST_SET(module)    (isp_devs[module].regs + 0x0A14)
#define CAM_REG_AE_PIX_HST0_YRNG(module)  (isp_devs[module].regs + 0x0A1C)
#define CAM_REG_AE_PIX_HST0_XRNG(module)  (isp_devs[module].regs + 0x0A20)
#define CAM_REG_AE_PIX_HST1_YRNG(module)  (isp_devs[module].regs + 0x0A24)
#define CAM_REG_AE_PIX_HST1_XRNG(module)  (isp_devs[module].regs + 0x0A28)
#define CAM_REG_AE_PIX_HST2_YRNG(module)  (isp_devs[module].regs + 0x0A2C)
#define CAM_REG_AE_PIX_HST2_XRNG(module)  (isp_devs[module].regs + 0x0A30)
#define CAM_REG_AE_PIX_HST3_YRNG(module)  (isp_devs[module].regs + 0x0A34)
#define CAM_REG_AE_PIX_HST3_XRNG(module)  (isp_devs[module].regs + 0x0A38)
#define CAM_REG_AE_STAT_EN(module)        (isp_devs[module].regs + 0x0A3C)
#define CAM_REG_AE_YCOEF(module)          (isp_devs[module].regs + 0x0A40)
#define CAM_REG_AE_CCU_HST_END_Y(module)  (isp_devs[module].regs + 0x0A44)
#define CAM_REG_AE_SPARE(module)          (isp_devs[module].regs + 0x0A48)
#define CAM_REG_QBN1_MODE(module)  (isp_devs[module].regs + 0x0AC0)
#define CAM_REG_CPG_SATU_1(module)  (isp_devs[module].regs + 0x0AD0)
#define CAM_REG_CPG_SATU_2(module)  (isp_devs[module].regs + 0x0AD4)
#define CAM_REG_CPG_GAIN_1(module)  (isp_devs[module].regs + 0x0AD8)
#define CAM_REG_CPG_GAIN_2(module)  (isp_devs[module].regs + 0x0ADC)
#define CAM_REG_CPG_OFST_1(module)  (isp_devs[module].regs + 0x0AE0)
#define CAM_REG_CPG_OFST_2(module)  (isp_devs[module].regs + 0x0AE4)
#define CAM_REG_CAC_TILE_SIZE(module)  (isp_devs[module].regs + 0x0AF0)
#define CAM_REG_CAC_TILE_OFFSET(module)  (isp_devs[module].regs + 0x0AF4)
#define CAM_REG_PMX_CTL(module)  (isp_devs[module].regs + 0x0B00)
#define CAM_REG_PMX_CROP(module)  (isp_devs[module].regs + 0x0B04)
#define CAM_REG_PMX_VSIZE(module)  (isp_devs[module].regs + 0x0B08)
#define CAM_REG_VBN_GAIN(module)  (isp_devs[module].regs + 0x0B40)
#define CAM_REG_VBN_OFST(module)  (isp_devs[module].regs + 0x0B44)
#define CAM_REG_VBN_TYPE(module)  (isp_devs[module].regs + 0x0B48)
#define CAM_REG_VBN_SPARE(module)  (isp_devs[module].regs + 0x0B4C)
#define CAM_REG_AMX_CTL(module)  (isp_devs[module].regs + 0x0B60)
#define CAM_REG_AMX_CROP(module)  (isp_devs[module].regs + 0x0B64)
#define CAM_REG_AMX_VSIZE(module)  (isp_devs[module].regs + 0x0B68)
#define CAM_REG_DBS_SIGMA(module)  (isp_devs[module].regs + 0x0B40)
#define CAM_REG_DBS_BSTBL_0(module)  (isp_devs[module].regs + 0x0B44)
#define CAM_REG_DBS_BSTBL_1(module)  (isp_devs[module].regs + 0x0B48)
#define CAM_REG_DBS_BSTBL_2(module)  (isp_devs[module].regs + 0x0B4C)
#define CAM_REG_DBS_BSTBL_3(module)  (isp_devs[module].regs + 0x0B50)
#define CAM_REG_DBS_CTL(module)  (isp_devs[module].regs + 0x0B54)
#define CAM_REG_DBS_CTL_2(module)  (isp_devs[module].regs + 0x0B58)
#define CAM_REG_DBS_SIGMA_2(module)  (isp_devs[module].regs + 0x0C1C)
#define CAM_REG_DBS_YGN(module)  (isp_devs[module].regs + 0x0C20)
#define CAM_REG_DBS_SL_Y12(module)  (isp_devs[module].regs + 0x0C24)
#define CAM_REG_DBS_SL_Y34(module)  (isp_devs[module].regs + 0x0C28)
#define CAM_REG_DBS_SL_G12(module)  (isp_devs[module].regs + 0x0C2C)
#define CAM_REG_DBS_SL_G34(module)  (isp_devs[module].regs + 0x0C30)
#define CAM_REG_BIN_CTL(module)  (isp_devs[module].regs + 0x0B80)
#define CAM_REG_BIN_FTH(module)  (isp_devs[module].regs + 0x0B84)
#define CAM_REG_BIN_SPARE(module)  (isp_devs[module].regs + 0x0B88)
#define CAM_REG_DBN_GAIN(module)  (isp_devs[module].regs + 0x0BA0)
#define CAM_REG_DBN_OFST(module)  (isp_devs[module].regs + 0x0BA4)
#define CAM_REG_DBN_SPARE(module)  (isp_devs[module].regs + 0x0BA8)
#define CAM_REG_PBN_TYPE(module)  (isp_devs[module].regs + 0x0BB0)
#define CAM_REG_PBN_LST(module)  (isp_devs[module].regs + 0x0BB4)
#define CAM_REG_PBN_VSIZE(module)  (isp_devs[module].regs + 0x0BB8)
#define CAM_REG_RCP3_CROP_CON1(module)  (isp_devs[module].regs + 0x0BC0)
#define CAM_REG_RCP3_CROP_CON2(module)  (isp_devs[module].regs + 0x0BC4)
#define CAM_REG_QBN4_MODE(module)  (isp_devs[module].regs + 0x0D00)
#define CAM_REG_PS_AWB_WIN_ORG(module)  (isp_devs[module].regs + 0x0D10)
#define CAM_REG_PS_AWB_WIN_SIZE(module)  (isp_devs[module].regs + 0x0D14)
#define CAM_REG_PS_AWB_WIN_PIT(module)  (isp_devs[module].regs + 0x0D18)
#define CAM_REG_PS_AWB_WIN_NUM(module)  (isp_devs[module].regs + 0x0D1C)
#define CAM_REG_PS_AWB_PIXEL_CNT0(module)  (isp_devs[module].regs + 0x0D20)
#define CAM_REG_PS_AWB_PIXEL_CNT1(module)  (isp_devs[module].regs + 0x0D24)
#define CAM_REG_PS_AWB_PIXEL_CNT2(module)  (isp_devs[module].regs + 0x0D28)
#define CAM_REG_PS_AWB_PIXEL_CNT3(module)  (isp_devs[module].regs + 0x0D2C)
#define CAM_REG_PS_AE_YCOEF0(module)  (isp_devs[module].regs + 0x0D30)
#define CAM_REG_PS_AE_YCOEF1(module)  (isp_devs[module].regs + 0x0D34)
#define CAM_REG_PDI_BASE_ADDR(module)  (isp_devs[module].regs + 0x0D50)
#define CAM_REG_PDI_OFST_ADDR(module)  (isp_devs[module].regs + 0x0D54)
#define CAM_REG_PDI_DRS(module)  (isp_devs[module].regs + 0x0D58)
#define CAM_REG_PDI_XSIZE(module)  (isp_devs[module].regs + 0x0D5C)
#define CAM_REG_PDI_YSIZE(module)  (isp_devs[module].regs + 0x0D60)
#define CAM_REG_PDI_STRIDE(module)  (isp_devs[module].regs + 0x0D64)
#define CAM_REG_PDI_CON(module)  (isp_devs[module].regs + 0x0D68)
#define CAM_REG_PDI_CON2(module)  (isp_devs[module].regs + 0x0D6C)
#define CAM_REG_PDI_CON3(module)  (isp_devs[module].regs + 0x0D70)
#define CAM_REG_PDI_CON4(module)  (isp_devs[module].regs + 0x0D74)
#define CAM_REG_PDI_ERR_STAT(module)  (isp_devs[module].regs + 0x0D78)
#define CAM_REG_PSO_BASE_ADDR(module)  (isp_devs[module].regs + 0x0D80)
#define CAM_REG_PSO_OFST_ADDR(module)  (isp_devs[module].regs + 0x0D88)
#define CAM_REG_PSO_XSIZE(module)  (isp_devs[module].regs + 0x0D90)
#define CAM_REG_PSO_YSIZE(module)  (isp_devs[module].regs + 0x0D94)
#define CAM_REG_PSO_STRIDE(module)  (isp_devs[module].regs + 0x0D98)
#define CAM_REG_PSO_CON(module)  (isp_devs[module].regs + 0x0D9C)
#define CAM_REG_PSO_CON2(module)  (isp_devs[module].regs + 0x0DA0)
#define CAM_REG_PSO_CON3(module)  (isp_devs[module].regs + 0x0DA4)
#define CAM_REG_PSO_CON4(module)  (isp_devs[module].regs + 0x0DA8)
#define CAM_REG_PSO_ERR_STAT(module)  (isp_devs[module].regs + 0x0DAC)
#define CAM_REG_PSO_FH_SPARE_2(module)  (isp_devs[module].regs + 0x0DB0)
#define CAM_REG_PSO_FH_SPARE_3(module)  (isp_devs[module].regs + 0x0DB4)
#define CAM_REG_PSO_FH_SPARE_4(module)  (isp_devs[module].regs + 0x0DB8)
#define CAM_REG_PSO_FH_SPARE_5(module)  (isp_devs[module].regs + 0x0DBC)
#define CAM_REG_PSO_FH_SPARE_6(module)  (isp_devs[module].regs + 0x0DC0)
#define CAM_REG_PSO_FH_SPARE_7(module)  (isp_devs[module].regs + 0x0DC4)
#define CAM_REG_PSO_FH_SPARE_8(module)  (isp_devs[module].regs + 0x0DC8)
#define CAM_REG_PSO_FH_SPARE_9(module)  (isp_devs[module].regs + 0x0DCC)
#define CAM_REG_PSO_FH_SPARE_10(module)  (isp_devs[module].regs + 0x0DD0)
#define CAM_REG_PSO_FH_SPARE_11(module)  (isp_devs[module].regs + 0x0DD4)
#define CAM_REG_PSO_FH_SPARE_12(module)  (isp_devs[module].regs + 0x0DD8)
#define CAM_REG_PSO_FH_SPARE_13(module)  (isp_devs[module].regs + 0x0DDC)
#define CAM_REG_PSO_FH_SPARE_14(module)  (isp_devs[module].regs + 0x0DE0)
#define CAM_REG_PSO_FH_SPARE_15(module)  (isp_devs[module].regs + 0x0DE4)
#define CAM_REG_PSO_FH_SPARE_16(module)  (isp_devs[module].regs + 0x0DE8)
#define CAM_REG_DMA_FRAME_HEADER_EN(module)  (isp_devs[module].regs + 0x0E00)
#define CAM_REG_IMGO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E04)
#define CAM_REG_RRZO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E08)
#define CAM_REG_AAO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E0C)
#define CAM_REG_AFO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E10)
#define CAM_REG_LCSO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E14)
#define CAM_REG_UFEO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E18)
#define CAM_REG_PDO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E1C)
#define CAM_REG_PSO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E20)
#define CAM_REG_IMGO_FH_SPARE_2(module)  (isp_devs[module].regs + 0x0E30)
#define CAM_REG_IMGO_FH_SPARE_3(module)  (isp_devs[module].regs + 0x0E34)
#define CAM_REG_IMGO_FH_SPARE_4(module)  (isp_devs[module].regs + 0x0E38)
#define CAM_REG_IMGO_FH_SPARE_5(module)  (isp_devs[module].regs + 0x0E3C)
#define CAM_REG_IMGO_FH_SPARE_6(module)  (isp_devs[module].regs + 0x0E40)
#define CAM_REG_IMGO_FH_SPARE_7(module)  (isp_devs[module].regs + 0x0E44)
#define CAM_REG_IMGO_FH_SPARE_8(module)  (isp_devs[module].regs + 0x0E48)
#define CAM_REG_IMGO_FH_SPARE_9(module)  (isp_devs[module].regs + 0x0E4C)
#define CAM_REG_IMGO_FH_SPARE_10(module)  (isp_devs[module].regs + 0x0E50)
#define CAM_REG_IMGO_FH_SPARE_11(module)  (isp_devs[module].regs + 0x0E54)
#define CAM_REG_IMGO_FH_SPARE_12(module)  (isp_devs[module].regs + 0x0E58)
#define CAM_REG_IMGO_FH_SPARE_13(module)  (isp_devs[module].regs + 0x0E5C)
#define CAM_REG_IMGO_FH_SPARE_14(module)  (isp_devs[module].regs + 0x0E60)
#define CAM_REG_IMGO_FH_SPARE_15(module)  (isp_devs[module].regs + 0x0E64)
#define CAM_REG_IMGO_FH_SPARE_16(module)  (isp_devs[module].regs + 0x0E68)
#define CAM_REG_RRZO_FH_SPARE_2(module)  (isp_devs[module].regs + 0x0E70)
#define CAM_REG_RRZO_FH_SPARE_3(module)  (isp_devs[module].regs + 0x0E74)
#define CAM_REG_RRZO_FH_SPARE_4(module)  (isp_devs[module].regs + 0x0E78)
#define CAM_REG_RRZO_FH_SPARE_5(module)  (isp_devs[module].regs + 0x0E7C)
#define CAM_REG_RRZO_FH_SPARE_6(module)  (isp_devs[module].regs + 0x0E80)
#define CAM_REG_RRZO_FH_SPARE_7(module)  (isp_devs[module].regs + 0x0E84)
#define CAM_REG_RRZO_FH_SPARE_8(module)  (isp_devs[module].regs + 0x0E88)
#define CAM_REG_RRZO_FH_SPARE_9(module)  (isp_devs[module].regs + 0x0E8C)
#define CAM_REG_RRZO_FH_SPARE_10(module)  (isp_devs[module].regs + 0x0E90)
#define CAM_REG_RRZO_FH_SPARE_11(module)  (isp_devs[module].regs + 0x0E94)
#define CAM_REG_RRZO_FH_SPARE_12(module)  (isp_devs[module].regs + 0x0E98)
#define CAM_REG_RRZO_FH_SPARE_13(module)  (isp_devs[module].regs + 0x0E9C)
#define CAM_REG_RRZO_FH_SPARE_14(module)  (isp_devs[module].regs + 0x0EA0)
#define CAM_REG_RRZO_FH_SPARE_15(module)  (isp_devs[module].regs + 0x0EA4)
#define CAM_REG_RRZO_FH_SPARE_16(module)  (isp_devs[module].regs + 0x0EA8)
#define CAM_REG_AAO_FH_SPARE_2(module)  (isp_devs[module].regs + 0x0EB0)
#define CAM_REG_AAO_FH_SPARE_3(module)  (isp_devs[module].regs + 0x0EB4)
#define CAM_REG_AAO_FH_SPARE_4(module)  (isp_devs[module].regs + 0x0EB8)
#define CAM_REG_AAO_FH_SPARE_5(module)  (isp_devs[module].regs + 0x0EBC)
#define CAM_REG_AAO_FH_SPARE_6(module)  (isp_devs[module].regs + 0x0EC0)
#define CAM_REG_AAO_FH_SPARE_7(module)  (isp_devs[module].regs + 0x0EC4)
#define CAM_REG_AAO_FH_SPARE_8(module)  (isp_devs[module].regs + 0x0EC8)
#define CAM_REG_AAO_FH_SPARE_9(module)  (isp_devs[module].regs + 0x0ECC)
#define CAM_REG_AAO_FH_SPARE_10(module)  (isp_devs[module].regs + 0x0ED0)
#define CAM_REG_AAO_FH_SPARE_11(module)  (isp_devs[module].regs + 0x0ED4)
#define CAM_REG_AAO_FH_SPARE_12(module)  (isp_devs[module].regs + 0x0ED8)
#define CAM_REG_AAO_FH_SPARE_13(module)  (isp_devs[module].regs + 0x0EDC)
#define CAM_REG_AAO_FH_SPARE_14(module)  (isp_devs[module].regs + 0x0EE0)
#define CAM_REG_AAO_FH_SPARE_15(module)  (isp_devs[module].regs + 0x0EE4)
#define CAM_REG_AAO_FH_SPARE_16(module)  (isp_devs[module].regs + 0x0EE8)
#define CAM_REG_AFO_FH_SPARE_2(module)  (isp_devs[module].regs + 0x0EF0)
#define CAM_REG_AFO_FH_SPARE_3(module)  (isp_devs[module].regs + 0x0EF4)
#define CAM_REG_AFO_FH_SPARE_4(module)  (isp_devs[module].regs + 0x0EF8)
#define CAM_REG_AFO_FH_SPARE_5(module)  (isp_devs[module].regs + 0x0EFC)
#define CAM_REG_AFO_FH_SPARE_6(module)  (isp_devs[module].regs + 0x0F00)
#define CAM_REG_AFO_FH_SPARE_7(module)  (isp_devs[module].regs + 0x0F04)
#define CAM_REG_AFO_FH_SPARE_8(module)  (isp_devs[module].regs + 0x0F08)
#define CAM_REG_AFO_FH_SPARE_9(module)  (isp_devs[module].regs + 0x0F0C)
#define CAM_REG_AFO_FH_SPARE_10(module)  (isp_devs[module].regs + 0x0F10)
#define CAM_REG_AFO_FH_SPARE_11(module)  (isp_devs[module].regs + 0x0F14)
#define CAM_REG_AFO_FH_SPARE_12(module)  (isp_devs[module].regs + 0x0F18)
#define CAM_REG_AFO_FH_SPARE_13(module)  (isp_devs[module].regs + 0x0F1C)
#define CAM_REG_AFO_FH_SPARE_14(module)  (isp_devs[module].regs + 0x0F20)
#define CAM_REG_AFO_FH_SPARE_15(module)  (isp_devs[module].regs + 0x0F24)
#define CAM_REG_AFO_FH_SPARE_16(module)  (isp_devs[module].regs + 0x0F28)
#define CAM_REG_LCSO_FH_SPARE_2(module)  (isp_devs[module].regs + 0x0F30)
#define CAM_REG_LCSO_FH_SPARE_3(module)  (isp_devs[module].regs + 0x0F34)
#define CAM_REG_LCSO_FH_SPARE_4(module)  (isp_devs[module].regs + 0x0F38)
#define CAM_REG_LCSO_FH_SPARE_5(module)  (isp_devs[module].regs + 0x0F3C)
#define CAM_REG_LCSO_FH_SPARE_6(module)  (isp_devs[module].regs + 0x0F40)
#define CAM_REG_LCSO_FH_SPARE_7(module)  (isp_devs[module].regs + 0x0F44)
#define CAM_REG_LCSO_FH_SPARE_8(module)  (isp_devs[module].regs + 0x0F48)
#define CAM_REG_LCSO_FH_SPARE_9(module)  (isp_devs[module].regs + 0x0F4C)
#define CAM_REG_LCSO_FH_SPARE_10(module)  (isp_devs[module].regs + 0x0F50)
#define CAM_REG_LCSO_FH_SPARE_11(module)  (isp_devs[module].regs + 0x0F54)
#define CAM_REG_LCSO_FH_SPARE_12(module)  (isp_devs[module].regs + 0x0F58)
#define CAM_REG_LCSO_FH_SPARE_13(module)  (isp_devs[module].regs + 0x0F5C)
#define CAM_REG_LCSO_FH_SPARE_14(module)  (isp_devs[module].regs + 0x0F60)
#define CAM_REG_LCSO_FH_SPARE_15(module)  (isp_devs[module].regs + 0x0F64)
#define CAM_REG_LCSO_FH_SPARE_16(module)  (isp_devs[module].regs + 0x0F68)
#define CAM_REG_UFEO_FH_SPARE_2(module)  (isp_devs[module].regs + 0x0F70)
#define CAM_REG_UFEO_FH_SPARE_3(module)  (isp_devs[module].regs + 0x0F74)
#define CAM_REG_UFEO_FH_SPARE_4(module)  (isp_devs[module].regs + 0x0F78)
#define CAM_REG_UFEO_FH_SPARE_5(module)  (isp_devs[module].regs + 0x0F7C)
#define CAM_REG_UFEO_FH_SPARE_6(module)  (isp_devs[module].regs + 0x0F80)
#define CAM_REG_UFEO_FH_SPARE_7(module)  (isp_devs[module].regs + 0x0F84)
#define CAM_REG_UFEO_FH_SPARE_8(module)  (isp_devs[module].regs + 0x0F88)
#define CAM_REG_UFEO_FH_SPARE_9(module)  (isp_devs[module].regs + 0x0F8C)
#define CAM_REG_UFEO_FH_SPARE_10(module)  (isp_devs[module].regs + 0x0F90)
#define CAM_REG_UFEO_FH_SPARE_11(module)  (isp_devs[module].regs + 0x0F94)
#define CAM_REG_UFEO_FH_SPARE_12(module)  (isp_devs[module].regs + 0x0F98)
#define CAM_REG_UFEO_FH_SPARE_13(module)  (isp_devs[module].regs + 0x0F9C)
#define CAM_REG_UFEO_FH_SPARE_14(module)  (isp_devs[module].regs + 0x0FA0)
#define CAM_REG_UFEO_FH_SPARE_15(module)  (isp_devs[module].regs + 0x0FA4)
#define CAM_REG_UFEO_FH_SPARE_16(module)  (isp_devs[module].regs + 0x0FA8)
#define CAM_REG_PDO_FH_SPARE_2(module)  (isp_devs[module].regs + 0x0FB0)
#define CAM_REG_PDO_FH_SPARE_3(module)  (isp_devs[module].regs + 0x0FB4)
#define CAM_REG_PDO_FH_SPARE_4(module)  (isp_devs[module].regs + 0x0FB8)
#define CAM_REG_PDO_FH_SPARE_5(module)  (isp_devs[module].regs + 0x0FBC)
#define CAM_REG_PDO_FH_SPARE_6(module)  (isp_devs[module].regs + 0x0FC0)
#define CAM_REG_PDO_FH_SPARE_7(module)  (isp_devs[module].regs + 0x0FC4)
#define CAM_REG_PDO_FH_SPARE_8(module)  (isp_devs[module].regs + 0x0FC8)
#define CAM_REG_PDO_FH_SPARE_9(module)  (isp_devs[module].regs + 0x0FCC)
#define CAM_REG_PDO_FH_SPARE_10(module)  (isp_devs[module].regs + 0x0FD0)
#define CAM_REG_PDO_FH_SPARE_11(module)  (isp_devs[module].regs + 0x0FD4)
#define CAM_REG_PDO_FH_SPARE_12(module)  (isp_devs[module].regs + 0x0FD8)
#define CAM_REG_PDO_FH_SPARE_13(module)  (isp_devs[module].regs + 0x0FDC)
#define CAM_REG_PDO_FH_SPARE_14(module)  (isp_devs[module].regs + 0x0FE0)
#define CAM_REG_PDO_FH_SPARE_15(module)  (isp_devs[module].regs + 0x0FE4)
#define CAM_REG_PDO_FH_SPARE_16(module)  (isp_devs[module].regs + 0x0FE8)

/* CAM_UNI */
#define CAM_UNI_REG_TOP_CTL(module)  (isp_devs[module].regs + 0x0000)
#define CAM_UNI_REG_TOP_MISC(module)  (isp_devs[module].regs + 0x0004)
#define CAM_UNI_REG_TOP_SW_CTL(module)  (isp_devs[module].regs + 0x0008)
#define CAM_UNI_REG_TOP_RAWI_TRIG(module)  (isp_devs[module].regs + 0x000C)
#define CAM_UNI_REG_TOP_MOD_EN(module)  (isp_devs[module].regs + 0x0010)
#define CAM_UNI_REG_TOP_DMA_EN(module)  (isp_devs[module].regs + 0x0014)
#define CAM_UNI_REG_TOP_PATH_SEL(module)  (isp_devs[module].regs + 0x0018)
#define CAM_UNI_REG_TOP_FMT_SEL(module)  (isp_devs[module].regs + 0x001C)
#define CAM_UNI_REG_TOP_DMA_INT_EN(module)  (isp_devs[module].regs + 0x0020)
#define CAM_UNI_REG_TOP_DMA_INT_STATUS(module)  (isp_devs[module].regs + 0x0024)
#define CAM_UNI_REG_TOP_DMA_INT_STATUSX(module) (isp_devs[module].regs + 0x0028)
#define CAM_UNI_REG_TOP_DBG_SET(module)  (isp_devs[module].regs + 0x002C)
#define CAM_UNI_REG_TOP_DBG_PORT(module)  (isp_devs[module].regs + 0x0030)
#define CAM_UNI_REG_TOP_MOD_DCM_DIS(module)  (isp_devs[module].regs + 0x0040)
#define CAM_UNI_REG_TOP_DMA_DCM_DIS(module)  (isp_devs[module].regs + 0x0044)
#define CAM_UNI_REG_TOP_MOD_DCM_STATUS(module)  (isp_devs[module].regs + 0x0050)
#define CAM_UNI_REG_TOP_DMA_DCM_STATUS(module)  (isp_devs[module].regs + 0x0054)
#define CAM_UNI_REG_TOP_MOD_REQ_STATUS(module)  (isp_devs[module].regs + 0x0060)
#define CAM_UNI_REG_TOP_DMA_REQ_STATUS(module)  (isp_devs[module].regs + 0x0064)
#define CAM_UNI_REG_TOP_MOD_RDY_STATUS(module)  (isp_devs[module].regs + 0x0070)
#define CAM_UNI_REG_TOP_DMA_RDY_STATUS(module)  (isp_devs[module].regs + 0x0074)
#define CAM_UNI_REG_FBC_FLKO_A_CTL1(module)  (isp_devs[module].regs + 0x0080)
#define CAM_UNI_REG_FBC_FLKO_A_CTL2(module)  (isp_devs[module].regs + 0x0084)
#define CAM_UNI_REG_FBC_EISO_A_CTL1(module)  (isp_devs[module].regs + 0x0088)
#define CAM_UNI_REG_FBC_EISO_A_CTL2(module)  (isp_devs[module].regs + 0x008C)
#define CAM_UNI_REG_FBC_RSSO_A_CTL1(module)  (isp_devs[module].regs + 0x0090)
#define CAM_UNI_REG_FBC_RSSO_A_CTL2(module)  (isp_devs[module].regs + 0x0094)
#define CAM_UNI_REG_DMA_SOFT_RSTSTAT(module)  (isp_devs[module].regs + 0x0200)
#define CAM_UNI_REG_VERTICAL_FLIP_EN(module)  (isp_devs[module].regs + 0x0204)
#define CAM_UNI_REG_DMA_SOFT_RESET(module)  (isp_devs[module].regs + 0x0208)
#define CAM_UNI_REG_LAST_ULTRA_EN(module)  (isp_devs[module].regs + 0x020C)
#define CAM_UNI_REG_SPECIAL_FUN_EN(module)  (isp_devs[module].regs + 0x0210)
#define CAM_UNI_REG_EISO_BASE_ADDR(module)  (isp_devs[module].regs + 0x0220)
#define CAM_UNI_REG_EISO_OFST_ADDR(module)  (isp_devs[module].regs + 0x0228)
#define CAM_UNI_REG_EISO_XSIZE(module)  (isp_devs[module].regs + 0x0230)
#define CAM_UNI_REG_EISO_YSIZE(module)  (isp_devs[module].regs + 0x0234)
#define CAM_UNI_REG_EISO_STRIDE(module)  (isp_devs[module].regs + 0x0238)
#define CAM_UNI_REG_EISO_CON(module)  (isp_devs[module].regs + 0x023C)
#define CAM_UNI_REG_EISO_CON2(module)  (isp_devs[module].regs + 0x0240)
#define CAM_UNI_REG_EISO_CON3(module)  (isp_devs[module].regs + 0x0244)
#define CAM_UNI_REG_FLKO_BASE_ADDR(module)  (isp_devs[module].regs + 0x0250)
#define CAM_UNI_REG_FLKO_OFST_ADDR(module)  (isp_devs[module].regs + 0x0258)
#define CAM_UNI_REG_FLKO_XSIZE(module)  (isp_devs[module].regs + 0x0260)
#define CAM_UNI_REG_FLKO_YSIZE(module)  (isp_devs[module].regs + 0x0264)
#define CAM_UNI_REG_FLKO_STRIDE(module)  (isp_devs[module].regs + 0x0268)
#define CAM_UNI_REG_FLKO_CON(module)  (isp_devs[module].regs + 0x026C)
#define CAM_UNI_REG_FLKO_CON2(module)  (isp_devs[module].regs + 0x0270)
#define CAM_UNI_REG_FLKO_CON3(module)  (isp_devs[module].regs + 0x0274)
#define CAM_UNI_REG_RSSO_A_BASE_ADDR(module)  (isp_devs[module].regs + 0x0280)
#define CAM_UNI_REG_RSSO_A_OFST_ADDR(module)  (isp_devs[module].regs + 0x0288)
#define CAM_UNI_REG_RSSO_A_XSIZE(module)  (isp_devs[module].regs + 0x0290)
#define CAM_UNI_REG_RSSO_A_YSIZE(module)  (isp_devs[module].regs + 0x0294)
#define CAM_UNI_REG_RSSO_A_STRIDE(module)  (isp_devs[module].regs + 0x0298)
#define CAM_UNI_REG_RSSO_A_CON(module)  (isp_devs[module].regs + 0x029C)
#define CAM_UNI_REG_RSSO_A_CON2(module)  (isp_devs[module].regs + 0x02A0)
#define CAM_UNI_REG_RSSO_A_CON3(module)  (isp_devs[module].regs + 0x02A4)
#define CAM_UNI_REG_RSSO_B_BASE_ADDR(module)  (isp_devs[module].regs + 0x02B0)
#define CAM_UNI_REG_RSSO_B_OFST_ADDR(module)  (isp_devs[module].regs + 0x02B8)
#define CAM_UNI_REG_RSSO_B_XSIZE(module)  (isp_devs[module].regs + 0x02C0)
#define CAM_UNI_REG_RSSO_B_YSIZE(module)  (isp_devs[module].regs + 0x02C4)
#define CAM_UNI_REG_RSSO_B_STRIDE(module)  (isp_devs[module].regs + 0x02C8)
#define CAM_UNI_REG_RSSO_B_CON(module)  (isp_devs[module].regs + 0x02CC)
#define CAM_UNI_REG_RSSO_B_CON2(module)  (isp_devs[module].regs + 0x02D0)
#define CAM_UNI_REG_RSSO_B_CON3(module)  (isp_devs[module].regs + 0x02D4)
#define CAM_UNI_REG_RAWI_BASE_ADDR(module)  (isp_devs[module].regs + 0x0340)
#define CAM_UNI_REG_RAWI_OFST_ADDR(module)  (isp_devs[module].regs + 0x0348)
#define CAM_UNI_REG_RAWI_XSIZE(module)  (isp_devs[module].regs + 0x0350)
#define CAM_UNI_REG_RAWI_YSIZE(module)  (isp_devs[module].regs + 0x0354)
#define CAM_UNI_REG_RAWI_STRIDE(module)  (isp_devs[module].regs + 0x0358)
#define CAM_UNI_REG_RAWI_CON(module)  (isp_devs[module].regs + 0x035C)
#define CAM_UNI_REG_RAWI_CON2(module)  (isp_devs[module].regs + 0x0360)
#define CAM_UNI_REG_RAWI_CON3(module)  (isp_devs[module].regs + 0x0364)
#define CAM_UNI_REG_DMA_ERR_CTRL(module)  (isp_devs[module].regs + 0x0370)
#define CAM_UNI_REG_EISO_ERR_STAT(module)  (isp_devs[module].regs + 0x0374)
#define CAM_UNI_REG_FLKO_ERR_STAT(module)  (isp_devs[module].regs + 0x0378)
#define CAM_UNI_REG_RSSO_A_ERR_STAT(module)  (isp_devs[module].regs + 0x037C)
#define CAM_UNI_REG_RSSO_B_ERR_STAT(module)  (isp_devs[module].regs + 0x0380)
#define CAM_UNI_REG_RAWI_ERR_STAT(module)  (isp_devs[module].regs + 0x0384)
#define CAM_UNI_REG_DMA_DEBUG_ADDR(module)  (isp_devs[module].regs + 0x0388)
#define CAM_UNI_REG_DMA_RSV1(module)  (isp_devs[module].regs + 0x038C)
#define CAM_UNI_REG_DMA_RSV2(module)  (isp_devs[module].regs + 0x0390)
#define CAM_UNI_REG_DMA_RSV3(module)  (isp_devs[module].regs + 0x0394)
#define CAM_UNI_REG_DMA_RSV4(module)  (isp_devs[module].regs + 0x0398)
#define CAM_UNI_REG_DMA_RSV5(module)  (isp_devs[module].regs + 0x039C)
#define CAM_UNI_REG_DMA_RSV6(module)  (isp_devs[module].regs + 0x03A0)
#define CAM_UNI_REG_DMA_DEBUG_SEL(module)  (isp_devs[module].regs + 0x03A4)
#define CAM_UNI_REG_DMA_BW_SELF_TEST(module)  (isp_devs[module].regs + 0x03A8)
#define CAM_UNI_REG_DMA_FRAME_HEADER_EN(module) (isp_devs[module].regs + 0x03C0)
#define CAM_UNI_REG_EISO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x03C4)
#define CAM_UNI_REG_FLKO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x03C8)
#define CAM_UNI_REG_RSSO_A_FH_BASE_ADDR(module) (isp_devs[module].regs + 0x03CC)
#define CAM_UNI_REG_RSSO_B_FH_BASE_ADDR(module) (isp_devs[module].regs + 0x03D0)
#define CAM_UNI_REG_EISO_FH_SPARE_2(module)  (isp_devs[module].regs + 0x03E0)
#define CAM_UNI_REG_EISO_FH_SPARE_3(module)  (isp_devs[module].regs + 0x03E4)
#define CAM_UNI_REG_EISO_FH_SPARE_4(module)  (isp_devs[module].regs + 0x03E8)
#define CAM_UNI_REG_EISO_FH_SPARE_5(module)  (isp_devs[module].regs + 0x03EC)
#define CAM_UNI_REG_EISO_FH_SPARE_6(module)  (isp_devs[module].regs + 0x03F0)
#define CAM_UNI_REG_EISO_FH_SPARE_7(module)  (isp_devs[module].regs + 0x03F4)
#define CAM_UNI_REG_EISO_FH_SPARE_8(module)  (isp_devs[module].regs + 0x03F8)
#define CAM_UNI_REG_EISO_FH_SPARE_9(module)  (isp_devs[module].regs + 0x03FC)
#define CAM_UNI_REG_EISO_FH_SPARE_10(module)  (isp_devs[module].regs + 0x0400)
#define CAM_UNI_REG_EISO_FH_SPARE_11(module)  (isp_devs[module].regs + 0x0404)
#define CAM_UNI_REG_EISO_FH_SPARE_12(module)  (isp_devs[module].regs + 0x0408)
#define CAM_UNI_REG_EISO_FH_SPARE_13(module)  (isp_devs[module].regs + 0x040C)
#define CAM_UNI_REG_EISO_FH_SPARE_14(module)  (isp_devs[module].regs + 0x0410)
#define CAM_UNI_REG_EISO_FH_SPARE_15(module)  (isp_devs[module].regs + 0x0414)
#define CAM_UNI_REG_EISO_FH_SPARE_16(module)  (isp_devs[module].regs + 0x0418)
#define CAM_UNI_REG_FLKO_FH_SPARE_2(module)  (isp_devs[module].regs + 0x0420)
#define CAM_UNI_REG_FLKO_FH_SPARE_3(module)  (isp_devs[module].regs + 0x0424)
#define CAM_UNI_REG_FLKO_FH_SPARE_4(module)  (isp_devs[module].regs + 0x0428)
#define CAM_UNI_REG_FLKO_FH_SPARE_5(module)  (isp_devs[module].regs + 0x042C)
#define CAM_UNI_REG_FLKO_FH_SPARE_6(module)  (isp_devs[module].regs + 0x0430)
#define CAM_UNI_REG_FLKO_FH_SPARE_7(module)  (isp_devs[module].regs + 0x0434)
#define CAM_UNI_REG_FLKO_FH_SPARE_8(module)  (isp_devs[module].regs + 0x0438)
#define CAM_UNI_REG_FLKO_FH_SPARE_9(module)  (isp_devs[module].regs + 0x043C)
#define CAM_UNI_REG_FLKO_FH_SPARE_10(module)  (isp_devs[module].regs + 0x0440)
#define CAM_UNI_REG_FLKO_FH_SPARE_11(module)  (isp_devs[module].regs + 0x0444)
#define CAM_UNI_REG_FLKO_FH_SPARE_12(module)  (isp_devs[module].regs + 0x0448)
#define CAM_UNI_REG_FLKO_FH_SPARE_13(module)  (isp_devs[module].regs + 0x044C)
#define CAM_UNI_REG_FLKO_FH_SPARE_14(module)  (isp_devs[module].regs + 0x0450)
#define CAM_UNI_REG_FLKO_FH_SPARE_15(module)  (isp_devs[module].regs + 0x0454)
#define CAM_UNI_REG_FLKO_FH_SPARE_16(module)  (isp_devs[module].regs + 0x0458)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_2(module)  (isp_devs[module].regs + 0x0460)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_3(module)  (isp_devs[module].regs + 0x0464)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_4(module)  (isp_devs[module].regs + 0x0468)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_5(module)  (isp_devs[module].regs + 0x046C)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_6(module)  (isp_devs[module].regs + 0x0470)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_7(module)  (isp_devs[module].regs + 0x0474)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_8(module)  (isp_devs[module].regs + 0x0478)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_9(module)  (isp_devs[module].regs + 0x047C)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_10(module)  (isp_devs[module].regs + 0x0480)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_11(module)  (isp_devs[module].regs + 0x0484)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_12(module)  (isp_devs[module].regs + 0x0488)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_13(module)  (isp_devs[module].regs + 0x048C)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_14(module)  (isp_devs[module].regs + 0x0490)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_15(module)  (isp_devs[module].regs + 0x0494)
#define CAM_UNI_REG_RSSO_A_FH_SPARE_16(module)  (isp_devs[module].regs + 0x0498)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_2(module)  (isp_devs[module].regs + 0x04A0)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_3(module)  (isp_devs[module].regs + 0x04A4)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_4(module)  (isp_devs[module].regs + 0x04A8)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_5(module)  (isp_devs[module].regs + 0x04AC)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_6(module)  (isp_devs[module].regs + 0x04B0)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_7(module)  (isp_devs[module].regs + 0x04B4)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_8(module)  (isp_devs[module].regs + 0x04B8)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_9(module)  (isp_devs[module].regs + 0x04BC)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_10(module)  (isp_devs[module].regs + 0x04C0)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_11(module)  (isp_devs[module].regs + 0x04C4)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_12(module)  (isp_devs[module].regs + 0x04C8)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_13(module)  (isp_devs[module].regs + 0x04CC)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_14(module)  (isp_devs[module].regs + 0x04D0)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_15(module)  (isp_devs[module].regs + 0x04D4)
#define CAM_UNI_REG_RSSO_B_FH_SPARE_16(module)  (isp_devs[module].regs + 0x04D8)
#define CAM_UNI_REG_UNP2_A_OFST(module)  (isp_devs[module].regs + 0x0500)
#define CAM_UNI_REG_QBN3_A_MODE(module)  (isp_devs[module].regs + 0x0510)
#define CAM_UNI_REG_SGG3_A_PGN(module)  (isp_devs[module].regs + 0x0520)
#define CAM_UNI_REG_SGG3_A_GMRC_1(module)  (isp_devs[module].regs + 0x0524)
#define CAM_UNI_REG_SGG3_A_GMRC_2(module)  (isp_devs[module].regs + 0x0528)
#define CAM_UNI_REG_FLK_A_CON(module)  (isp_devs[module].regs + 0x0530)
#define CAM_UNI_REG_FLK_A_OFST(module)  (isp_devs[module].regs + 0x0534)
#define CAM_UNI_REG_FLK_A_SIZE(module)  (isp_devs[module].regs + 0x0538)
#define CAM_UNI_REG_FLK_A_NUM(module)  (isp_devs[module].regs + 0x053C)
#define CAM_UNI_REG_EIS_A_PREP_ME_CTRL1(module) (isp_devs[module].regs + 0x0550)
#define CAM_UNI_REG_EIS_A_PREP_ME_CTRL2(module) (isp_devs[module].regs + 0x0554)
#define CAM_UNI_REG_EIS_A_LMV_TH(module)  (isp_devs[module].regs + 0x0558)
#define CAM_UNI_REG_EIS_A_FL_OFFSET(module)  (isp_devs[module].regs + 0x055C)
#define CAM_UNI_REG_EIS_A_MB_OFFSET(module)  (isp_devs[module].regs + 0x0560)
#define CAM_UNI_REG_EIS_A_MB_INTERVAL(module)  (isp_devs[module].regs + 0x0564)
#define CAM_UNI_REG_EIS_A_GMV(module)  (isp_devs[module].regs + 0x0568)
#define CAM_UNI_REG_EIS_A_ERR_CTRL(module)  (isp_devs[module].regs + 0x056C)
#define CAM_UNI_REG_EIS_A_IMAGE_CTRL(module)  (isp_devs[module].regs + 0x0570)
#define CAM_UNI_REG_SGG2_A_PGN(module)  (isp_devs[module].regs + 0x0590)
#define CAM_UNI_REG_SGG2_A_GMRC_1(module)  (isp_devs[module].regs + 0x0594)
#define CAM_UNI_REG_SGG2_A_GMRC_2(module)  (isp_devs[module].regs + 0x0598)
#define CAM_UNI_REG_HDS_A_MODE(module)  (isp_devs[module].regs + 0x05A0)
#define CAM_UNI_REG_RSS_A_CONTROL(module)  (isp_devs[module].regs + 0x05C0)
#define CAM_UNI_REG_RSS_A_IN_IMG(module)  (isp_devs[module].regs + 0x05C4)
#define CAM_UNI_REG_RSS_A_OUT_IMG(module)  (isp_devs[module].regs + 0x05C8)
#define CAM_UNI_REG_RSS_A_HORI_STEP(module)  (isp_devs[module].regs + 0x05CC)
#define CAM_UNI_REG_RSS_A_VERT_STEP(module)  (isp_devs[module].regs + 0x05D0)
#define CAM_UNI_REG_RSS_A_HORI_INT_OFST(module) (isp_devs[module].regs + 0x05D4)
#define CAM_UNI_REG_RSS_A_HORI_SUB_OFST(module) (isp_devs[module].regs + 0x05D8)
#define CAM_UNI_REG_RSS_A_VERT_INT_OFST(module) (isp_devs[module].regs + 0x05DC)
#define CAM_UNI_REG_RSS_A_VERT_SUB_OFST(module) (isp_devs[module].regs + 0x05E0)
#define CAM_UNI_REG_RSS_A_SPARE_1(module)  (isp_devs[module].regs + 0x05F4)
#define CAM_UNI_REG_B_TOP_SW_CTL(module)  (isp_devs[module].regs + 0x0108)
#define CAM_UNI_REG_B_TOP_DMA_EN(module)  (isp_devs[module].regs + 0x0114)
#define CAM_UNI_REG_B_TOP_PATH_SEL(module)  (isp_devs[module].regs + 0x0118)
#define CAM_UNI_REG_FBC_FLKO_B_CTL1(module)  (isp_devs[module].regs + 0x0180)
#define CAM_UNI_REG_FBC_FLKO_B_CTL2(module)  (isp_devs[module].regs + 0x0184)
#define CAM_UNI_REG_FBC_EISO_B_CTL1(module)  (isp_devs[module].regs + 0x0188)
#define CAM_UNI_REG_FBC_EISO_B_CTL2(module)  (isp_devs[module].regs + 0x018C)
#define CAM_UNI_REG_FBC_RSSO_B_CTL1(module)  (isp_devs[module].regs + 0x0190)
#define CAM_UNI_REG_FBC_RSSO_B_CTL2(module)  (isp_devs[module].regs + 0x0194)
#define CAM_UNI_REG_B_EISO_ERR_STAT(module)  (isp_devs[module].regs + 0x0774)
#define CAM_UNI_REG_B_FLKO_ERR_STAT(module)  (isp_devs[module].regs + 0x0778)
#define CAM_UNI_REG_B_RSSO_A_ERR_STAT(module)  (isp_devs[module].regs + 0x077C)
#define CAM_UNI_REG_B_RAWI_ERR_STAT(module)  (isp_devs[module].regs + 0x0384)
/* CAMSV */
#define CAMSV_REG_TOP_DEBUG(module)  (isp_devs[module].regs + 0x0000)
#define CAMSV_REG_MODULE_EN(module)  (isp_devs[module].regs + 0x0010)
#define CAMSV_REG_FMT_SEL(module)  (isp_devs[module].regs + 0x0014)
#define CAMSV_REG_INT_EN(module)  (isp_devs[module].regs + 0x0018)
#define CAMSV_REG_INT_STATUS(module)  (isp_devs[module].regs + 0x001C)
#define CAMSV_REG_SW_CTL(module)  (isp_devs[module].regs + 0x0020)
#define CAMSV_REG_SPARE0(module)  (isp_devs[module].regs + 0x0024)
#define CAMSV_REG_SPARE1(module)  (isp_devs[module].regs + 0x0028)
#define CAMSV_REG_IMGO_FBC(module)  (isp_devs[module].regs + 0x002C)
#define CAMSV_REG_CLK_EN(module)  (isp_devs[module].regs + 0x0030)
#define CAMSV_REG_DBG_SET(module)  (isp_devs[module].regs + 0x0034)
#define CAMSV_REG_DBG_PORT(module)  (isp_devs[module].regs + 0x0038)
#define CAMSV_REG_DATE_CODE(module)  (isp_devs[module].regs + 0x003C)
#define CAMSV_REG_PROJ_CODE(module)  (isp_devs[module].regs + 0x0040)
#define CAMSV_REG_DCM_DIS(module)  (isp_devs[module].regs + 0x0044)
#define CAMSV_REG_DCM_STATUS(module)  (isp_devs[module].regs + 0x0048)
#define CAMSV_REG_PAK(module)  (isp_devs[module].regs + 0x004C)
#define CAMSV_REG_FBC_IMGO_CTL1(module)  (isp_devs[module].regs + 0x0110)
#define CAMSV_REG_FBC_IMGO_CTL2(module)  (isp_devs[module].regs + 0x0114)
#define CAMSV_REG_FBC_IMGO_ENQ_ADDR(module)  (isp_devs[module].regs + 0x0118)
#define CAMSV_REG_FBC_IMGO_CUR_ADDR(module)  (isp_devs[module].regs + 0x011C)
#define CAMSV_REG_DMA_SOFT_RSTSTAT(module)  (isp_devs[module].regs + 0x0200)
#define CAMSV_REG_CQ0I_BASE_ADDR(module)  (isp_devs[module].regs + 0x0204)
#define CAMSV_REG_CQ0I_XSIZE(module)  (isp_devs[module].regs + 0x0208)
#define CAMSV_REG_VERTICAL_FLIP_EN(module)  (isp_devs[module].regs + 0x020C)
#define CAMSV_REG_DMA_SOFT_RESET(module)  (isp_devs[module].regs + 0x0210)
#define CAMSV_REG_LAST_ULTRA_EN(module)  (isp_devs[module].regs + 0x0214)
#define CAMSV_REG_SPECIAL_FUN_EN(module)  (isp_devs[module].regs + 0x0218)
#define CAMSV_REG_IMGO_BASE_ADDR(module)  (isp_devs[module].regs + 0x0220)
#define CAMSV_REG_IMGO_OFST_ADDR(module)  (isp_devs[module].regs + 0x0228)
#define CAMSV_REG_IMGO_XSIZE(module)  (isp_devs[module].regs + 0x0230)
#define CAMSV_REG_IMGO_YSIZE(module)  (isp_devs[module].regs + 0x0234)
#define CAMSV_REG_IMGO_STRIDE(module)  (isp_devs[module].regs + 0x0238)
#define CAMSV_REG_IMGO_CON(module)  (isp_devs[module].regs + 0x023C)
#define CAMSV_REG_IMGO_CON2(module)  (isp_devs[module].regs + 0x0240)
#define CAMSV_REG_IMGO_CON3(module)  (isp_devs[module].regs + 0x0244)
#define CAMSV_REG_IMGO_CROP(module)  (isp_devs[module].regs + 0x0248)
#define CAMSV_REG_DMA_ERR_CTRL(module)  (isp_devs[module].regs + 0x0440)
#define CAMSV_REG_IMGO_ERR_STAT(module)  (isp_devs[module].regs + 0x0444)
#define CAMSV_REG_DMA_DEBUG_ADDR(module)  (isp_devs[module].regs + 0x046C)
#define CAMSV_REG_DMA_RSV1(module)  (isp_devs[module].regs + 0x0470)
#define CAMSV_REG_DMA_RSV2(module)  (isp_devs[module].regs + 0x0474)
#define CAMSV_REG_DMA_RSV3(module)  (isp_devs[module].regs + 0x0478)
#define CAMSV_REG_DMA_RSV4(module)  (isp_devs[module].regs + 0x047C)
#define CAMSV_REG_DMA_RSV5(module)  (isp_devs[module].regs + 0x0480)
#define CAMSV_REG_DMA_RSV6(module)  (isp_devs[module].regs + 0x0484)
#define CAMSV_REG_DMA_DEBUG_SEL(module)  (isp_devs[module].regs + 0x0488)
#define CAMSV_REG_DMA_BW_SELF_TEST(module)  (isp_devs[module].regs + 0x048C)
#define CAMSV_REG_TG_SEN_MODE(module)  (isp_devs[module].regs + 0x0500)
#define CAMSV_REG_TG_VF_CON(module)  (isp_devs[module].regs + 0x0504)
#define CAMSV_REG_TG_SEN_GRAB_PXL(module)  (isp_devs[module].regs + 0x0508)
#define CAMSV_REG_TG_SEN_GRAB_LIN(module)  (isp_devs[module].regs + 0x050C)
#define CAMSV_REG_TG_PATH_CFG(module)  (isp_devs[module].regs + 0x0510)
#define CAMSV_REG_TG_MEMIN_CTL(module)  (isp_devs[module].regs + 0x0514)
#define CAMSV_REG_TG_INT1(module)  (isp_devs[module].regs + 0x0518)
#define CAMSV_REG_TG_INT2(module)  (isp_devs[module].regs + 0x051C)
#define CAMSV_REG_TG_SOF_CNT(module)  (isp_devs[module].regs + 0x0520)
#define CAMSV_REG_TG_SOT_CNT(module)  (isp_devs[module].regs + 0x0524)
#define CAMSV_REG_TG_EOT_CNT(module)  (isp_devs[module].regs + 0x0528)
#define CAMSV_REG_TG_ERR_CTL(module)  (isp_devs[module].regs + 0x052C)
#define CAMSV_REG_TG_DAT_NO(module)  (isp_devs[module].regs + 0x0530)
#define CAMSV_REG_TG_FRM_CNT_ST(module)  (isp_devs[module].regs + 0x0534)
#define CAMSV_REG_TG_FRMSIZE_ST(module)  (isp_devs[module].regs + 0x0538)
#define CAMSV_REG_TG_INTER_ST(module)  (isp_devs[module].regs + 0x053C)
#define CAMSV_REG_TG_FLASHA_CTL(module)  (isp_devs[module].regs + 0x0540)
#define CAMSV_REG_TG_FLASHA_LINE_CNT(module)  (isp_devs[module].regs + 0x0544)
#define CAMSV_REG_TG_FLASHA_POS(module)  (isp_devs[module].regs + 0x0548)
#define CAMSV_REG_TG_FLASHB_CTL(module)  (isp_devs[module].regs + 0x054C)
#define CAMSV_REG_TG_FLASHB_LINE_CNT(module)  (isp_devs[module].regs + 0x0550)
#define CAMSV_REG_TG_FLASHB_POS(module)  (isp_devs[module].regs + 0x0554)
#define CAMSV_REG_TG_FLASHB_POS1(module)  (isp_devs[module].regs + 0x0558)
#define CAMSV_REG_TG_I2C_CQ_TRIG(module)  (isp_devs[module].regs + 0x0560)
#define CAMSV_REG_TG_CQ_TIMING(module)  (isp_devs[module].regs + 0x0564)
#define CAMSV_REG_TG_CQ_NUM(module)  (isp_devs[module].regs + 0x0568)
#define CAMSV_REG_TG_TIME_STAMP(module)  (isp_devs[module].regs + 0x0570)
#define CAMSV_REG_TG_SUB_PERIOD(module)  (isp_devs[module].regs + 0x0574)
#define CAMSV_REG_TG_DAT_NO_R(module)  (isp_devs[module].regs + 0x0578)
#define CAMSV_REG_TG_FRMSIZE_ST_R(module)  (isp_devs[module].regs + 0x057C)
#define CAMSV_REG_DMA_FRAME_HEADER_EN(module)  (isp_devs[module].regs + 0x0E00)
#define CAMSV_REG_IMGO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E04)
#define CAMSV_REG_RRZO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E08)
#define CAMSV_REG_AAO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E0C)
#define CAMSV_REG_AFO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E10)
#define CAMSV_REG_LCSO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E14)
#define CAMSV_REG_UFEO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E18)
#define CAMSV_REG_PDO_FH_BASE_ADDR(module)  (isp_devs[module].regs + 0x0E1C)
#define CAMSV_REG_IMGO_FH_SPARE_2(module)  (isp_devs[module].regs + 0x0E30)
#define CAMSV_REG_IMGO_FH_SPARE_3(module)  (isp_devs[module].regs + 0x0E34)
#define CAMSV_REG_IMGO_FH_SPARE_4(module)  (isp_devs[module].regs + 0x0E38)
#define CAMSV_REG_IMGO_FH_SPARE_5(module)  (isp_devs[module].regs + 0x0E3C)
#define CAMSV_REG_IMGO_FH_SPARE_6(module)  (isp_devs[module].regs + 0x0E40)
#define CAMSV_REG_IMGO_FH_SPARE_7(module)  (isp_devs[module].regs + 0x0E44)
#define CAMSV_REG_IMGO_FH_SPARE_8(module)  (isp_devs[module].regs + 0x0E48)
#define CAMSV_REG_IMGO_FH_SPARE_9(module)  (isp_devs[module].regs + 0x0E4C)
#define CAMSV_REG_IMGO_FH_SPARE_10(module)  (isp_devs[module].regs + 0x0E50)
#define CAMSV_REG_IMGO_FH_SPARE_11(module)  (isp_devs[module].regs + 0x0E54)
#define CAMSV_REG_IMGO_FH_SPARE_12(module)  (isp_devs[module].regs + 0x0E58)
#define CAMSV_REG_IMGO_FH_SPARE_13(module)  (isp_devs[module].regs + 0x0E5C)
#define CAMSV_REG_IMGO_FH_SPARE_14(module)  (isp_devs[module].regs + 0x0E60)
#define CAMSV_REG_IMGO_FH_SPARE_15(module)  (isp_devs[module].regs + 0x0E64)
#define CAMSV_REG_IMGO_FH_SPARE_16(module)  (isp_devs[module].regs + 0x0E68)



#define ISP_REG_ADDR_EN1                (ISP_ADDR + 0x4)
#define ISP_REG_ADDR_INT_P1_ST          (ISP_ADDR + 0x4C)
#define CAM_REG_ADDR_DMA_ST             (ISP_ADDR + 0x4C)
#define ISP_REG_ADDR_INT_P1_ST2         (ISP_ADDR + 0x54)
#define ISP_REG_ADDR_INT_P1_ST_D        (ISP_ADDR + 0x5C)
#define ISP_REG_ADDR_INT_P1_ST2_D       (ISP_ADDR + 0x64)
#define ISP_REG_ADDR_INT_P2_ST          (ISP_ADDR + 0x6C)
#define ISP_REG_ADDR_INT_STATUSX        (ISP_ADDR + 0x70)
#define ISP_REG_ADDR_INT_STATUS2X       (ISP_ADDR + 0x74)
#define ISP_REG_ADDR_INT_STATUS3X       (ISP_ADDR + 0x78)
#define ISP_REG_ADDR_CAM_SW_CTL         (ISP_ADDR + 0x8C)
#define CAM_REG_ADDR_IMGO_FBC_1           (ISP_ADDR + 0xF0)
#define CAM_REG_ADDR_RRZO_FBC_1           (ISP_ADDR + 0xF4)
#define CAM_REG_ADDR_IMGO_FBC_2           (ISP_ADDR + 0xF0)
#define CAM_REG_ADDR_RRZO_FBC_2           (ISP_ADDR + 0xF4)


#define ISP_REG_ADDR_IMGO_D_FBC         (ISP_ADDR + 0xF8)
#define ISP_REG_ADDR_RRZO_D_FBC         (ISP_ADDR + 0xFC)
#define ISP_REG_ADDR_TG_VF_CON          (ISP_ADDR + 0x414)
#define ISP_REG_ADDR_TG_INTER_ST        (ISP_ADDR + 0x44C)
#define ISP_REG_ADDR_TG2_VF_CON         (ISP_ADDR + 0x2414)
#define ISP_REG_ADDR_TG2_INTER_ST       (ISP_ADDR + 0x244C)
#define ISP_REG_ADDR_IMGO_BASE_ADDR     (ISP_ADDR + 0x3300)
#define ISP_REG_ADDR_RRZO_BASE_ADDR     (ISP_ADDR + 0x3320)
#define ISP_REG_ADDR_IMGO_D_BASE_ADDR   (ISP_ADDR + 0x34D4)
#define ISP_REG_ADDR_RRZO_D_BASE_ADDR   (ISP_ADDR + 0x34F4)
#define ISP_REG_ADDR_SENINF1_INT        (ISP_ADDR + 0x4128)
#define ISP_REG_ADDR_SENINF2_INT        (ISP_ADDR + 0x4528)
#define ISP_REG_ADDR_SENINF3_INT        (ISP_ADDR + 0x4928)
#define ISP_REG_ADDR_SENINF4_INT        (ISP_ADDR + 0x4D28)
#define ISP_REG_ADDR_CAMSV_FMT_SEL      (ISP_ADDR + 0x5004)
#define ISP_REG_ADDR_CAMSV_INT          (ISP_ADDR + 0x500C)
#define ISP_REG_ADDR_CAMSV_SW_CTL       (ISP_ADDR + 0x5010)
#define ISP_REG_ADDR_CAMSV_TG_INTER_ST  (ISP_ADDR + 0x544C)
#define ISP_REG_ADDR_CAMSV2_FMT_SEL     (ISP_ADDR + 0x5804)
#define ISP_REG_ADDR_CAMSV2_INT         (ISP_ADDR + 0x580C)
#define ISP_REG_ADDR_CAMSV2_SW_CTL      (ISP_ADDR + 0x5810)
#define ISP_REG_ADDR_CAMSV_TG2_INTER_ST (ISP_ADDR + 0x5C4C)
#define ISP_REG_ADDR_CAMSV_IMGO_FBC     (ISP_ADDR + 0x501C)
#define ISP_REG_ADDR_CAMSV2_IMGO_FBC    (ISP_ADDR + 0x581C)
#define ISP_REG_ADDR_IMGO_SV_BASE_ADDR  (ISP_ADDR + 0x5208)
#define ISP_REG_ADDR_IMGO_SV_XSIZE      (ISP_ADDR + 0x5210)
#define ISP_REG_ADDR_IMGO_SV_YSIZE      (ISP_ADDR + 0x5214)
#define ISP_REG_ADDR_IMGO_SV_STRIDE     (ISP_ADDR + 0x5218)
#define ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR    (ISP_ADDR + 0x5228)
#define ISP_REG_ADDR_IMGO_SV_D_XSIZE    (ISP_ADDR + 0x5230)
#define ISP_REG_ADDR_IMGO_SV_D_YSIZE    (ISP_ADDR + 0x5234)
#define ISP_REG_ADDR_IMGO_SV_D_STRIDE   (ISP_ADDR + 0x5238)
#define TG_REG_ADDR_GRAB_W              (ISP_ADDR + 0x418)
#define TG2_REG_ADDR_GRAB_W             (ISP_ADDR + 0x2418)
#define TG_REG_ADDR_GRAB_H              (ISP_ADDR + 0x41C)
#define TG2_REG_ADDR_GRAB_H             (ISP_ADDR + 0x241C)

#define ISP_REG_ADDR_CAMSV_TG_VF_CON    (ISP_ADDR + 0x5414)
#define ISP_REG_ADDR_CAMSV_TG2_VF_CON   (ISP_ADDR + 0x5C14)
#define CAMSV_REG_ADDR_IMGO_FBC_1           (ISP_ADDR + 0xF0)
#define CAMSV_REG_ADDR_IMGO_FBC_2           (ISP_ADDR + 0xF4)
#define CAMSV_REG_ADDR_TG_INTER_ST      (0)
/* spare register */
/* #define ISP_REG_ADDR_TG_MAGIC_0         (ISP_ADDR + 0x94) */
/* #define ISP_REG_ADDR_TG_MAGIC_1         (ISP_ADDR + 0x9C) */
/* New define by 20131114 */
#define ISP_REG_ADDR_TG_MAGIC_0         (ISP_IMGSYS_BASE + 0x75DC) /* 0088 */

#define ISP_REG_ADDR_TG2_MAGIC_0        (ISP_IMGSYS_BASE + 0x75E4) /* 0090 */

/* for rrz input crop size */
#define ISP_REG_ADDR_TG_RRZ_CROP_IN     (ISP_IMGSYS_BASE + 0x75E0)
#define ISP_REG_ADDR_TG_RRZ_CROP_IN_D   (ISP_IMGSYS_BASE + 0x75E8)

/* for rrz destination widt
 * (in twin mode, ISP_INNER_REG_ADDR_RRZO_XSIZE < RRZO width)
 */
#define ISP_REG_ADDR_RRZ_W         (ISP_ADDR_CAMINF + 0x4094)
#define ISP_REG_ADDR_RRZ_W_D       (ISP_ADDR_CAMINF + 0x409C)
/*
 * CAM_REG_CTL_SPARE1              CAM_CTL_SPARE1;                 //4094
 * CAM_REG_CTL_SPARE2              CAM_CTL_SPARE2;                 //409C
 * CAM_REG_CTL_SPARE3              CAM_CTL_SPARE3;                 //4100
 * CAM_REG_AE_SPARE                 CAM_AE_SPARE;                   //4694
 * CAM_REG_DM_O_SPARE             CAM_DM_O_SPARE;                 //48F0
 * CAM_REG_MIX1_SPARE              CAM_MIX1_SPARE;                 //4C98
 * CAM_REG_MIX2_SPARE              CAM_MIX2_SPARE;                 //4CA8
 * CAM_REG_MIX3_SPARE              CAM_MIX3_SPARE;                 //4CB8
 * CAM_REG_NR3D_SPARE0            CAM_NR3D_SPARE0;                //4D04
 * CAM_REG_AWB_D_SPARE           CAM_AWB_D_SPARE;                //663C
 * CAM_REG_AE_D_SPARE              CAM_AE_D_SPARE;                 //6694
 * CAMSV_REG_CAMSV_SPARE0      CAMSV_CAMSV_SPARE0;             //9014
 * CAMSV_REG_CAMSV_SPARE1      CAMSV_CAMSV_SPARE1;             //9018
 * CAMSV_REG_CAMSV2_SPARE0    CAMSV_CAMSV2_SPARE0;            //9814
 * CAMSV_REG_CAMSV2_SPARE1    CAMSV_CAMSV2_SPARE1;            //9818
 */

/* inner register */
/* 1500_d000 ==> 1500_4000 */
/* 1500_e000 ==> 1500_6000 */
/* 1500_f000 ==> 1500_7000 */
#define ISP_INNER_REG_ADDR_FMT_SEL_P1       (ISP_ADDR + 0x0028)
#define ISP_INNER_REG_ADDR_FMT_SEL_P1_D     (ISP_ADDR + 0x002C)
#define ISP_INNER_REG_ADDR_CAM_CTRL_SEL_P1       (ISP_ADDR + 0x0034)
#define ISP_INNER_REG_ADDR_CAM_CTRL_SEL_P1_D     (ISP_ADDR + 0x0038)
/* #define ISP_INNER_REG_ADDR_FMT_SEL_P1       (ISP_ADDR_CAMINF + 0xD028) */
/* #define ISP_INNER_REG_ADDR_FMT_SEL_P1_D     (ISP_ADDR_CAMINF + 0xD02C) */
#define ISP_INNER_REG_ADDR_IMGO_XSIZE       (ISP_ADDR_CAMINF + 0xF308)
#define ISP_INNER_REG_ADDR_IMGO_YSIZE       (ISP_ADDR_CAMINF + 0xF30C)
#define ISP_INNER_REG_ADDR_IMGO_STRIDE      (ISP_ADDR_CAMINF + 0xF310)
#define ISP_INNER_REG_ADDR_IMGO_CROP        (ISP_ADDR_CAMINF + 0xF31C)
#define ISP_INNER_REG_ADDR_RRZO_XSIZE       (ISP_ADDR_CAMINF + 0xF328)
#define ISP_INNER_REG_ADDR_RRZO_YSIZE       (ISP_ADDR_CAMINF + 0xF32C)
#define ISP_INNER_REG_ADDR_RRZO_STRIDE      (ISP_ADDR_CAMINF + 0xF330)
#define ISP_INNER_REG_ADDR_RRZO_CROP        (ISP_ADDR_CAMINF + 0xF33C)
#define ISP_INNER_REG_ADDR_IMGO_D_XSIZE     (ISP_ADDR_CAMINF + 0xF4DC)
#define ISP_INNER_REG_ADDR_IMGO_D_YSIZE     (ISP_ADDR_CAMINF + 0xF4E0)
#define ISP_INNER_REG_ADDR_IMGO_D_STRIDE    (ISP_ADDR_CAMINF + 0xF4E4)
#define ISP_INNER_REG_ADDR_IMGO_D_CROP      (ISP_ADDR_CAMINF + 0xF4F0)
#define ISP_INNER_REG_ADDR_RRZO_D_XSIZE     (ISP_ADDR_CAMINF + 0xF4FC)
#define ISP_INNER_REG_ADDR_RRZO_D_YSIZE     (ISP_ADDR_CAMINF + 0xF500)
#define ISP_INNER_REG_ADDR_RRZO_D_STRIDE    (ISP_ADDR_CAMINF + 0xF504)
#define ISP_INNER_REG_ADDR_RRZO_D_CROP      (ISP_ADDR_CAMINF + 0xF510)

#define ISP_INNER_REG_ADDR_RRZ_HORI_INT_OFST (ISP_ADDR_CAMINF + 0xD7B4)
#define ISP_INNER_REG_ADDR_RRZ_VERT_INT_OFST (ISP_ADDR_CAMINF + 0xD7BC)
#define ISP_INNER_REG_ADDR_RRZ_IN_IMG        (ISP_ADDR_CAMINF + 0xD7A4)
#define ISP_INNER_REG_ADDR_RRZ_OUT_IMG       (ISP_ADDR_CAMINF + 0xD7A8)

#define ISP_INNER_REG_ADDR_RRZ_D_HORI_INT_OFST (ISP_ADDR_CAMINF + 0xE7B4)
#define ISP_INNER_REG_ADDR_RRZ_D_VERT_INT_OFST (ISP_ADDR_CAMINF + 0xE7BC)
#define ISP_INNER_REG_ADDR_RRZ_D_IN_IMG        (ISP_ADDR_CAMINF + 0xE7A4)
#define ISP_INNER_REG_ADDR_RRZ_D_OUT_IMG       (ISP_ADDR_CAMINF + 0xE7A8)


#define ISP_TPIPE_ADDR                  (0x15004000)

/* CAM_CTL_SW_CTL */
#define ISP_REG_SW_CTL_SW_RST_P1_MASK   (0x00000007)
#define ISP_REG_SW_CTL_SW_RST_TRIG      (0x00000001)
#define ISP_REG_SW_CTL_SW_RST_STATUS    (0x00000002)
#define ISP_REG_SW_CTL_HW_RST           (0x00000004)
#define ISP_REG_SW_CTL_SW_RST_P2_MASK   (0x00000070)
#define ISP_REG_SW_CTL_SW_RST_P2_TRIG   (0x00000010)
#define ISP_REG_SW_CTL_SW_RST_P2_STATUS (0x00000020)
#define ISP_REG_SW_CTL_HW_RST_P2        (0x00000040)



/* if isp has been suspend, frame cnt needs to add previous value*/
#define ISP_RD32_TG_CAM_FRM_CNT(IrqType, reg_module) ({\
	unsigned int _regVal;\
	_regVal = ISP_RD32(CAM_REG_TG_INTER_ST(reg_module));\
	_regVal = ((_regVal & 0x00FF0000) >> 16) + \
			g_BkReg[IrqType].CAM_TG_INTER_ST;\
	if (_regVal > 255) { \
		_regVal -= 256;\
	} \
	_regVal;\
})

#if 0
/******************************************************************************
 * file shrink
 *****************************************************************************/
#include "camera_isp_reg.c"
#include "camera_isp_isr.c"
#endif

/******************************************************************************
 *
 *****************************************************************************/
static inline unsigned int ISP_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

/******************************************************************************
 *
 *****************************************************************************/
static inline unsigned int ISP_UsToJiffies(unsigned int Us)
{
	return (((Us / 1000) * HZ + 512) >> 10);
}

/******************************************************************************
 *
 *****************************************************************************/
static inline unsigned int
ISP_GetIRQState(unsigned int type, unsigned int stType,
		unsigned int userNumber, unsigned int stus)
{
	unsigned int ret;
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
 *****************************************************************************/
static inline unsigned int ISP_JiffiesToMs(unsigned int Jiffies)
{
	return ((Jiffies * 1000) / HZ);
}

/******************************************************************************
 *
 *****************************************************************************/

static void ISP_DumpDmaDeepDbg(enum ISP_IRQ_TYPE_ENUM module)
{
	unsigned int uni_path;
	unsigned int flk2_sel;
	unsigned int hds2_sel;
	unsigned int dmaerr[_cam_max_];
	enum ISP_DEV_NODE_ENUM regModule; /* for read/write register */

	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
		regModule = ISP_CAM_A_IDX;
		break;
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
		regModule = ISP_CAM_B_IDX;
		break;
	default:
		LOG_NOTICE("unsupported module:0x%x\n",
			module);
		return;
	}


	dmaerr[_imgo_] = (unsigned int)
		ISP_RD32(CAM_REG_IMGO_ERR_STAT(regModule));
	dmaerr[_rrzo_] = (unsigned int)
		ISP_RD32(CAM_REG_RRZO_ERR_STAT(regModule));
	dmaerr[_aao_] = (unsigned int)
		ISP_RD32(CAM_REG_AAO_ERR_STAT(regModule));
	dmaerr[_afo_] = (unsigned int)
		ISP_RD32(CAM_REG_AFO_ERR_STAT(regModule));
	dmaerr[_lcso_] = (unsigned int)
		ISP_RD32(CAM_REG_LCSO_ERR_STAT(regModule));
	dmaerr[_ufeo_] = (unsigned int)
		ISP_RD32(CAM_REG_UFEO_ERR_STAT(regModule));
	dmaerr[_bpci_] = (unsigned int)
		ISP_RD32(CAM_REG_BPCI_ERR_STAT(regModule));
	dmaerr[_lsci_] = (unsigned int)
		ISP_RD32(CAM_REG_LSCI_ERR_STAT(regModule));
	dmaerr[_pdo_] = (unsigned int)
		ISP_RD32(CAM_REG_PDO_ERR_STAT(regModule));
	dmaerr[_pso_] = (unsigned int)
		ISP_RD32(CAM_REG_PSO_ERR_STAT(regModule));
	dmaerr[_pdi_] = (unsigned int)
		ISP_RD32(CAM_REG_PDI_ERR_STAT(regModule));

	if (regModule == ISP_CAM_A_IDX) {
		dmaerr[_eiso_] = (unsigned int)
			ISP_RD32(CAM_UNI_REG_EISO_ERR_STAT(ISP_UNI_A_IDX));
		dmaerr[_flko_] = (unsigned int)
			ISP_RD32(CAM_UNI_REG_FLKO_ERR_STAT(ISP_UNI_A_IDX));
		dmaerr[_rsso_] = (unsigned int)
			ISP_RD32(CAM_UNI_REG_RSSO_A_ERR_STAT(ISP_UNI_A_IDX));
		dmaerr[_rawi_] = (unsigned int)
			ISP_RD32(CAM_UNI_REG_RAWI_ERR_STAT(ISP_UNI_A_IDX));
	}
	if (regModule == ISP_CAM_B_IDX) {
		dmaerr[_eiso_] = (unsigned int)
			ISP_RD32(CAM_UNI_REG_B_EISO_ERR_STAT(ISP_UNI_A_IDX));
		dmaerr[_flko_] = (unsigned int)
			ISP_RD32(CAM_UNI_REG_B_FLKO_ERR_STAT(ISP_UNI_A_IDX));
		dmaerr[_rsso_] = (unsigned int)
			ISP_RD32(CAM_UNI_REG_B_RSSO_A_ERR_STAT(ISP_UNI_A_IDX));
		dmaerr[_rawi_] = (unsigned int)
			ISP_RD32(CAM_UNI_REG_B_RAWI_ERR_STAT(ISP_UNI_A_IDX));
	}

	IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
		"mmsys:0x%x, imgsys:0x%x, camsys:0x%x\n",
		ISP_RD32(ISP_MMSYS_CONFIG_BASE + 0x100),
		ISP_RD32(ISP_IMGSYS_CONFIG_BASE),
		ISP_RD32(ISP_CAMSYS_CONFIG_BASE));

	if (regModule == ISP_CAM_A_IDX)
		uni_path = ISP_RD32(CAM_UNI_REG_TOP_PATH_SEL(ISP_UNI_A_IDX));
	if (regModule == ISP_CAM_B_IDX)
		uni_path = ISP_RD32(CAM_UNI_REG_B_TOP_PATH_SEL(ISP_UNI_A_IDX));

	hds2_sel = (uni_path & 0x3);
	flk2_sel = ((uni_path >> 8) & 0x3);

	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
		IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
	"CAM_A:IMGO:0x%x,RRZO:0x%x,AAO=0x%x,AFO=0x%x,LCSO=0x%x,UFEO=0x%x,PDO=0x%x,PSO=0x%x\n",
			dmaerr[_imgo_],
			dmaerr[_rrzo_],
			dmaerr[_aao_],
			dmaerr[_afo_],
			dmaerr[_lcso_],
			dmaerr[_ufeo_],
			dmaerr[_pdo_],
			dmaerr[_pso_]);
		IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
	"CAM_A:BPCI:0x%x,LSCI=0x%x,PDI=0x%x\n",
			dmaerr[_bpci_],
			dmaerr[_lsci_],
			dmaerr[_pdi_]);
		if (flk2_sel == 0) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
				"CAM_A+UNI: FLKO:0x%x, RAWI:0x%x\n",
				dmaerr[_flko_], dmaerr[_rawi_]);
		}
		if (hds2_sel == 0) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
				"CAM_A+UNI: EISO:0x%x, RSSO_A:0x%x, RAWI:0x%x\n",
				dmaerr[_eiso_], dmaerr[_rsso_], dmaerr[_rawi_]);
		}
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
		g_DmaErr_CAM[module][_eiso_] |= dmaerr[_eiso_];
		g_DmaErr_CAM[module][_rsso_] |= dmaerr[_rsso_];
		g_DmaErr_CAM[module][_rawi_] |= dmaerr[_rawi_];
		g_DmaErr_CAM[module][_pdi_] |= dmaerr[_pdi_];
		break;
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
		IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
	"CAM_B:IMGO:0x%x,RRZO:0x%x,AAO=0x%x,AFO=0x%x,LCSO=0x%x,UFEO=0x%x,PDO=0x%x,PSO=0x%x\n",
			dmaerr[_imgo_],
			dmaerr[_rrzo_],
			dmaerr[_aao_],
			dmaerr[_afo_],
			dmaerr[_lcso_],
			dmaerr[_ufeo_],
			dmaerr[_pdo_],
			dmaerr[_pso_]);
		IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
	"CAM_B:BPCI:0x%x,LSCI=0x%x,PDI=0x%x\n",
			dmaerr[_bpci_],
			dmaerr[_lsci_],
			dmaerr[_pdi_]);
		if (flk2_sel == 1) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
				"CAM_B+UNI: FLKO:0x%x, RAWI:0x%x\n",
				dmaerr[_flko_], dmaerr[_rawi_]);
		}
		if (hds2_sel == 1) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
				"CAM_B+UNI: EISO:0x%x, RSSO_A:0x%x, RAWI:0x%x\n",
				dmaerr[_eiso_], dmaerr[_rsso_], dmaerr[_rawi_]);
		}
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
		g_DmaErr_CAM[module][_eiso_] |= dmaerr[_eiso_];
		g_DmaErr_CAM[module][_rsso_] |= dmaerr[_rsso_];
		g_DmaErr_CAM[module][_rawi_] |= dmaerr[_rawi_];
		g_DmaErr_CAM[module][_pdi_] |= dmaerr[_pdi_];
		break;
	default:
		LOG_NOTICE("unsupported module:0x%x\n", module);
		break;
	}
}


#define RegDump(start, end) {\
	unsigned int i;\
	for (i = start; i <= end; i += 0x10) {\
		pr_info(\
		"QQ [0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]\n",\
			(unsigned int)(ISP_TPIPE_ADDR + i),\
			(unsigned int)ISP_RD32(ISP_ADDR + i),\
			(unsigned int)(ISP_TPIPE_ADDR + i+0x4),\
			(unsigned int)ISP_RD32(ISP_ADDR + i+0x4),\
			(unsigned int)(ISP_TPIPE_ADDR + i+0x8),\
			(unsigned int)ISP_RD32(ISP_ADDR + i+0x8),\
			(unsigned int)(ISP_TPIPE_ADDR + i+0xc),\
			(unsigned int)ISP_RD32(ISP_ADDR + i+0xc));\
	} \
}


static signed int ISP_DumpSeninfReg(void)
{
	signed int Ret = 0;
	/*  */
	pr_info("- E.");
	/*Sensor interface Top mux and Package counter*/
	pr_info(
	"seninf:0008(0x%x)-0010(0x%x)-0a10(0x%x)-1a10(0x%x) 0a1c(0x%x)-1a1c(0x%x)-0a1c(0x%x)-1a1c(0x%x)\n",
		ISP_RD32(ISP_SENINF0_BASE + 0x0008),
		ISP_RD32(ISP_SENINF0_BASE + 0x0010),
		ISP_RD32(ISP_SENINF0_BASE + 0x0a10),
		ISP_RD32(ISP_SENINF1_BASE + 0x0a10),
		ISP_RD32(ISP_SENINF0_BASE + 0x0a1c),
		ISP_RD32(ISP_SENINF1_BASE + 0x0a1c),
		ISP_RD32(ISP_SENINF0_BASE + 0x0a1c),
		ISP_RD32(ISP_SENINF1_BASE + 0x0a1c));
	/*Sensor interface0 control*/
	pr_info(
	"seninf:0200(0x%x)-0204(0x%x)-0a00(0x%x)-0a14(0x%x) 0a3c(0x%x)-0a44(0x%x)-0af0(0x%x)-0af4(0x%x)\n",
		ISP_RD32(ISP_SENINF0_BASE + 0x0200),
		ISP_RD32(ISP_SENINF0_BASE + 0x0204),
		ISP_RD32(ISP_SENINF0_BASE + 0x0a00),
		ISP_RD32(ISP_SENINF0_BASE + 0x0a14),
		ISP_RD32(ISP_SENINF0_BASE + 0x0a3c),
		ISP_RD32(ISP_SENINF0_BASE + 0x0a44),
		ISP_RD32(ISP_SENINF0_BASE + 0x0af0),
		ISP_RD32(ISP_SENINF0_BASE + 0x0af4));
	/*Sensor interface1 control*/
	pr_info(
	"seninf:1200(0x%x)-1204(0x%x)-1a00(0x%x)-1a14(0x%x) 1a3c(0x%x)-1a44(0x%x)-1af0(0x%x)-1af4(0x%x)\n",
		ISP_RD32(ISP_SENINF1_BASE + 0x0200),
		ISP_RD32(ISP_SENINF1_BASE + 0x0204),
		ISP_RD32(ISP_SENINF1_BASE + 0x0a00),
		ISP_RD32(ISP_SENINF1_BASE + 0x0a14),
		ISP_RD32(ISP_SENINF1_BASE + 0x0a3c),
		ISP_RD32(ISP_SENINF1_BASE + 0x0a44),
		ISP_RD32(ISP_SENINF1_BASE + 0x0af0),
		ISP_RD32(ISP_SENINF1_BASE + 0x0af4));
	/*Sensor interface mux*/
	pr_info(
	"seninf:0d00(0x%x)-0d08(0x%x)-0d14(0x%x)-0d18(0x%x) 1d00(0x%x)-1d08(0x%x)-1d14(0x%x)-1d18(0x%x)\n",
		ISP_RD32(ISP_SENINF0_BASE + 0x0d00),
		ISP_RD32(ISP_SENINF0_BASE + 0x0d08),
		ISP_RD32(ISP_SENINF0_BASE + 0x0d14),
		ISP_RD32(ISP_SENINF0_BASE + 0x0d18),
		ISP_RD32(ISP_SENINF1_BASE + 0x0d00),
		ISP_RD32(ISP_SENINF1_BASE + 0x0d08),
		ISP_RD32(ISP_SENINF1_BASE + 0x0d14),
		ISP_RD32(ISP_SENINF1_BASE + 0x0d18));
	pr_info(
	"seninf:2d00(0x%x)-2d08(0x%x)-2d14(0x%x)-2d18(0x%x) 3d00(0x%x)-3d08(0x%x)-3d14(0x%x)-3d18(0x%x)\n",
		ISP_RD32(ISP_SENINF2_BASE + 0x0d00),
		ISP_RD32(ISP_SENINF2_BASE + 0x0d08),
		ISP_RD32(ISP_SENINF2_BASE + 0x0d14),
		ISP_RD32(ISP_SENINF2_BASE + 0x0d18),
		ISP_RD32(ISP_SENINF3_BASE + 0x0d00),
		ISP_RD32(ISP_SENINF3_BASE + 0x0d08),
		ISP_RD32(ISP_SENINF3_BASE + 0x0d14),
		ISP_RD32(ISP_SENINF3_BASE + 0x0d18));
	/* IMGPLL frequency */
	pr_info("IMGPLL frequency(0x%x)[HPM:0x114EC5, LPM:0xC8000]",
		ISP_RD32(CLOCK_CELL_BASE + 0x0264));

	/*  */
	return Ret;

}
static signed int ISP_DumpReg(void)
{
	signed int Ret = 0;

#if 0
	/*  */
	/* spin_lock_irqsave(&(IspInfo.SpinLock), flags); */

	/* tile tool parse range */
	/* #define ISP_ADDR_START  0x15004000 */
	/* #define ISP_ADDR_END    0x15006000 */
	/*  */
	/* N3D control */
	ISP_WR32((ISP_ADDR + 0x40c0), 0x746);
	pr_info("[0x%08X %08X] [0x%08X %08X]",
		(unsigned int)(ISP_TPIPE_ADDR + 0x40c0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x40c0),
		(unsigned int)(ISP_TPIPE_ADDR + 0x40d8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x40d8));
	ISP_WR32((ISP_ADDR + 0x40c0), 0x946);
	pr_info("[0x%08X %08X] [0x%08X %08X]",
		(unsigned int)(ISP_TPIPE_ADDR + 0x40c0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x40c0),
		(unsigned int)(ISP_TPIPE_ADDR + 0x40d8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x40d8));

	/* isp top */
	RegDump(0x0, 0x200);
	/* dump p1 dma reg */
	RegDump(0x3200, 0x3570);
	/* dump all isp dma reg */
	RegDump(0x3300, 0x3400);
	/* dump all isp dma err reg */
	RegDump(0x3560, 0x35e0);
	/* TG1 */
	RegDump(0x410, 0x4a0);
	/* TG2 */
	RegDump(0x2410, 0x2450);
	/* hbin */
	pr_info("[%s] [0x%08X %08X],[0x%08X %08X]",
		(unsigned int)(ISP_TPIPE_ADDR + 0x4f0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x534),
		(unsigned int)(ISP_TPIPE_ADDR + 0x4f4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x538));
	/* LSC */
	RegDump(0x530, 0x550);
	/* awb win */
	RegDump(0x5b0, 0x5d0);
	/* ae win */
	RegDump(0x650, 0x690);
	/* af win */
	RegDump(0x6b0, 0x700);
	/* flk */
	RegDump(0x770, 0x780);
	/* rrz */
	RegDump(0x7a0, 0x7d0);
	/* eis */
	RegDump(0xdc0, 0xdf0);
	/* dmx/rmx/bmx */
	RegDump(0xe00, 0xe30);
	/* Mipi source */
	pr_info("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(0x10217000),
		(unsigned int)ISP_RD32(ISP_MIPI_ANA_ADDR),
		(unsigned int)(0x10217004),
		(unsigned int)ISP_RD32(ISP_MIPI_ANA_ADDR + 0x4));
	pr_info("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(0x10217008),
		(unsigned int)ISP_RD32(ISP_MIPI_ANA_ADDR + 0x8),
		(unsigned int)(0x1021700c),
		(unsigned int)ISP_RD32(ISP_MIPI_ANA_ADDR + 0xc));
	pr_info("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(0x10217030),
		(unsigned int)ISP_RD32(ISP_MIPI_ANA_ADDR + 0x30),
		(unsigned int)(0x10219030),
		(unsigned int)ISP_RD32(ISP_MIPI_ANA_ADDR + 0x2030));

	/* NSCI2 1 debug */
	ISP_WR32((ISP_ADDR + 0x43B8), 0x02);
	pr_info("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x43B8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x43B8));
	pr_info("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x43BC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x43BC));
	ISP_WR32((ISP_ADDR + 0x43B8), 0x12);
	pr_info("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x43B8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x43B8));
	pr_info("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x43BC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x43BC));
	/* NSCI2 3 debug */
	ISP_WR32((ISP_ADDR + 0x4BB8), 0x02);
	pr_info("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB8));
	pr_info("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x4BBC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x43BC));
	ISP_WR32((ISP_ADDR + 0x4BB8), 0x12);
	pr_info("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x43B8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB8));
	pr_info("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x4BBC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BBC));

	/* seninf1 */
	pr_info("[0x%08X %08X],[0x%08X %08X]",
		(unsigned int)(ISP_TPIPE_ADDR + 0x4008),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4008),
		(unsigned int)(ISP_TPIPE_ADDR + 0x4100),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4100));
	RegDump(0x4120, 0x4160);
	RegDump(0x4360, 0x43f0)
	/* seninf2 */
	pr_info("[0x%08X %08X],[0x%08X %08X]",
		(unsigned int)(ISP_TPIPE_ADDR + 0x4008),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4008),
		(unsigned int)(ISP_TPIPE_ADDR + 0x4100),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4100));
	RegDump(0x4520, 0x4560);
	RegDump(0x4600, 0x4610);
	RegDump(0x4760, 0x47f0);
	/* LSC_D */
	RegDump(0x2530, 0x2550);
	/* awb_d */
	RegDump(0x25b0, 0x25d0);
	/* ae_d */
	RegDump(0x2650, 0x2690);
	/* af_d */
	RegDump(0x26b0, 0x2700);
	/* rrz_d */
	RegDump(0x27a0, 0x27d0);
	/* rmx_d/bmx_d/dmx_d */
	RegDump(0x2e00, 0x2e30);

	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x800),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x800));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x880),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x880));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x884),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x884));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x888),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x888));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x8A0),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x8A0));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x920),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x920));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x924),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x924));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x928),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x928));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x92C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x92C));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x930),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x930));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x934),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x934));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x938),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x938));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x93C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x93C));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x960),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x960));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x9C4),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x9C4));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x9E4),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x9E4));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x9E8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x9E8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x9EC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x9EC));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA00),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xA00));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA04),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xA04));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA08),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xA08));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA0C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xA0C));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA10),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xA10));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA14),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xA14));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA20),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xA20));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xAA0),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xAA0));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xACC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xACC));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB00),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB00));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB04),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB04));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB08),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB08));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB0C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB0C));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB10),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB10));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB14),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB14));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB18),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB18));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB1C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB1C));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB20),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB20));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB44),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB44));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB48),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB48));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB4C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB4C));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB50),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB50));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB54),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB54));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB58),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB58));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB5C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB5C));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB60),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xB60));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBA0),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xBA0));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBA4),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xBA4));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBA8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xBA8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBAC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xBAC));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBB0),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xBB0));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBB4),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xBB4));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBB8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xBB8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBBC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xBBC));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBC0),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xBC0));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xC20),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xC20));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCC0),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xCC0));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCE4),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xCE4));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCE8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xCE8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCEC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xCEC));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCF0),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xCF0));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCF4),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xCF4));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCF8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xCF8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCFC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xCFC));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD24),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xD24));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD28),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xD28));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD2C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xD2c));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD40),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xD40));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD64),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xD64));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD68),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xD68));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD6C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xD6c));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD70),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xD70));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD74),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xD74));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD78),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xD78));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD7C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xD7C));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xDA4),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xDA4));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xDA8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xDA8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0xDAC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xDAC));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2410),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x2410));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2414),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x2414));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2418),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x2418));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x241C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x241C));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2420),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x2420));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x243C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x243C));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2440),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x2440));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2444),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x2444));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2448),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x2448));

	/* seninf3 */
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4900),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4900));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4920),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4920));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4924),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4924));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4928),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4928));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x492C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x492C));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4930),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4930));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4934),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4934));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4938),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4938));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BA0),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BA0));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BA4),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BA4));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BA8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BA8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BAC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BAC));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB0),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB0));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB4),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB4));
	ISP_WR32((ISP_ADDR + 0x4BB8), 0x10);
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BBC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BBC));
	ISP_WR32((ISP_ADDR + 0x4BB8), 0x11);
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BBC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BBC));
	ISP_WR32((ISP_ADDR + 0x4BB8), 0x12);
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BBC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4BBC));
	/* seninf4 */
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D00),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4D00));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D20),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4D20));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D24),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4D24));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D28),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4D28));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D2C),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4D2C));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D30),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4D30));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D34),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4D34));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D38),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4D38));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FA0),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4FA0));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FA4),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4FA4));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FA8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4FA8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FAC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4FAC));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB0),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB0));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB4),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB4));
	ISP_WR32((ISP_ADDR + 0x4FB8), 0x10);
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FBC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4FBC));
	ISP_WR32((ISP_ADDR + 0x4FB8), 0x11);
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FBC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4FBC));
	ISP_WR32((ISP_ADDR + 0x4FB8), 0x12);
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB8),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB8));
	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FBC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x4FBC));

	pr_info("0x%08X %08X", (unsigned int)(ISP_TPIPE_ADDR + 0x35FC),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x35FC));
	pr_info("end MT6593");

	/*  */
	pr_info("0x%08X %08X ", (unsigned int)ISP_ADDR_CAMINF,
	(unsigned int)ISP_RD32(ISP_ADDR_CAMINF));
	pr_info("0x%08X %08X ", (unsigned int)(ISP_TPIPE_ADDR + 0x150),
	(unsigned int)ISP_RD32(ISP_ADDR + 0x150));
	/*  */
	/* debug msg for direct link */


	/* mdp crop */
	pr_info("MDPCROP Related");
	pr_info("0x%08X %08X", (unsigned int)(ISP_ADDR + 0xd10),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xd10));
	pr_info("0x%08X %08X", (unsigned int)(ISP_ADDR + 0xd20),
	(unsigned int)ISP_RD32(ISP_ADDR + 0xd20));
	/* cq */
	pr_info("CQ Related");
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x6000);
	pr_info("0x%08X %08X (0x15004160=6000)",
	(unsigned int)(ISP_IMGSYS_BASE + 0x4164),
	(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x7000);
	pr_info("0x%08X %08X (0x15004160=7000)",
	(unsigned int)(ISP_IMGSYS_BASE + 0x4164),
	(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x8000);
	pr_info("0x%08X %08X (0x15004160=8000)",
	(unsigned int)(ISP_IMGSYS_BASE + 0x4164),
	(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	/* imgi_debug */
	pr_info("IMGI_DEBUG Related");
	ISP_WR32(ISP_IMGSYS_BASE + 0x75f4, 0x001e);
	pr_info("0x%08X %08X (0x150075f4=001e)",
	(unsigned int)(ISP_IMGSYS_BASE + 0x4164),
	(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x75f4, 0x011e);
	pr_info("0x%08X %08X (0x150075f4=011e)",
	(unsigned int)(ISP_IMGSYS_BASE + 0x4164),
	(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x75f4, 0x021e);
	pr_info("0x%08X %08X (0x150075f4=021e)",
	(unsigned int)(ISP_IMGSYS_BASE + 0x4164),
	(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x75f4, 0x031e);
	pr_info("0x%08X %08X (0x150075f4=031e)",
	(unsigned int)(ISP_IMGSYS_BASE + 0x4164),
	(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	/* yuv */
	pr_info("yuv-mdp crop Related");
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x3014);
	pr_info("0x%08X %08X (0x15004160=3014)",
	(unsigned int)(ISP_IMGSYS_BASE + 0x4164),
	(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	pr_info("yuv-c24b out Related");
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x301e);
	pr_info("0x%08X %08X (0x15004160=301e)",
	(unsigned int)(ISP_IMGSYS_BASE + 0x4164),
	(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x301f);
	pr_info("0x%08X %08X (0x15004160=301f)",
	(unsigned int)(ISP_IMGSYS_BASE + 0x4164),
	(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x3020);
	pr_info("0x%08X %08X (0x15004160=3020)",
	(unsigned int)(ISP_IMGSYS_BASE + 0x4164),
	(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x3021);
	pr_info("0x%08X %08X (0x15004160=3021)",
	(unsigned int)(ISP_IMGSYS_BASE + 0x4164),
	(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));


#if 0 /* _mt6593fpga_dvt_use_ */
	{
		int tpipePA = ISP_RD32(ISP_ADDR + 0x204);
		int ctlStart = ISP_RD32(ISP_ADDR + 0x000);
		int ctlTcm = ISP_RD32(ISP_ADDR + 0x054);
		int map_va = 0, map_size;
		int i;
		int *pMapVa;
#define TPIPE_DUMP_SIZE    200

		if ((ctlStart & 0x01) && (tpipePA) && (ctlTcm & 0x80000000)) {
			map_va = 0;
			m4u_mva_map_kernel(tpipePA, TPIPE_DUMP_SIZE, 0,
						&map_va, &map_size);
			pMapVa = map_va;
			pr_info("pMapVa(0x%x),map_size(0x%x)",
				pMapVa, map_size);
			pr_info("ctlStart(0x%x),tpipePA(0x%x),ctlTcm(0x%x)",
				ctlStart, tpipePA, ctlTcm);
			if (pMapVa) {
				for (i = 0; i < TPIPE_DUMP_SIZE; i += 10) {
					pr_info(
					"[idx(%d)]%08X-%08X-%08X-%08X-%08X-%08X-%08X-%08X-%08X-%08X",
					i,
					pMapVa[i], pMapVa[i + 1],
					pMapVa[i + 2], pMapVa[i + 3],
					pMapVa[i + 4], pMapVa[i + 5],
					pMapVa[i + 6], pMapVa[i + 7],
					pMapVa[i + 8], pMapVa[i + 9]);
				}
			}
			m4u_mva_unmap_kernel(tpipePA, map_size, map_va);
		}
	}
#endif

	/* spin_unlock_irqrestore(&(IspInfo.SpinLock), flags); */
	/*  */
#endif
	pr_info("- X.");
	/*  */
	return Ret;
}

static signed int ISP_DumpDIPReg(void)
{
	signed int Ret = 0;
	unsigned int i, cmdqidx = 0, dipdmacmd = 0;
	unsigned int smidmacmd = 0, dmaidx = 0;
	unsigned int fifodmacmd = 0;
#ifdef AEE_DUMP_REDUCE_MEMORY
	unsigned int offset = 0;
	uintptr_t OffsetAddr = 0;
	unsigned int ctrl_start;
#else
	unsigned int offset = 0;
	uintptr_t OffsetAddr = 0;
	unsigned int ctrl_start;
#endif
	/*  */
	CMDQ_ERR("- E.");
	CMDQ_ERR(
	"g_bDumpPhyISPBuf:(0x%x), g_pPhyISPBuffer:(0x%p), g_bIonBuf:(0x%x)\n",
		g_bDumpPhyISPBuf, g_pPhyISPBuffer, g_bIonBufferAllocated);
#ifdef AEE_DUMP_REDUCE_MEMORY
	if (g_bDumpPhyISPBuf == MFALSE) {
		ctrl_start = ISP_RD32(ISP_DIP_A_BASE + 0x0000);
		if (g_bIonBufferAllocated == MFALSE) {
			if (g_pPhyISPBuffer != NULL) {
				CMDQ_ERR(
				"g_pPhyISPBuffer is not NULL:(0x%pK)\n",
					g_pPhyISPBuffer);
				vfree(g_pPhyISPBuffer);
				g_pPhyISPBuffer = NULL;
			}
			g_pPhyISPBuffer = vmalloc(ISP_DIP_REG_SIZE);
			/* //mark for checkpatch, unnecessart msg
			 * if (g_pPhyISPBuffer == NULL)
			 *	CMDQ_ERR(
			 *	"ERROR: g_pPhyISPBuffer kmalloc failed\n");
			 */
			if (g_pKWTpipeBuffer != NULL) {
				CMDQ_ERR(
				"g_pKWTpipeBuffer is not NULL:(0x%pK)\n",
					g_pKWTpipeBuffer);
				vfree(g_pKWTpipeBuffer);
				g_pKWTpipeBuffer = NULL;
			}
			g_pKWTpipeBuffer = vmalloc(MAX_ISP_TILE_TDR_HEX_NO);
			/* //mark for checkpatch, unnecessart msg
			 * if (g_pKWTpipeBuffer == NULL)
			 *	CMDQ_ERR(
			 *	"ERROR: g_pKWTpipeBuffer kmalloc failed\n");
			 */
			if (g_pKWCmdqBuffer != NULL) {
				CMDQ_ERR(
				"g_KWCmdqBuffer is not NULL:(0x%pK)\n",
					g_pKWCmdqBuffer);
				vfree(g_pKWCmdqBuffer);
				g_pKWCmdqBuffer = NULL;
			}
			g_pKWCmdqBuffer = vmalloc(MAX_ISP_CMDQ_BUFFER_SIZE);
			/* //mark for checkpatch, unnecessart msg
			 * if (g_pKWCmdqBuffer == NULL)
			 *	CMDQ_ERR(
			 *	"ERROR: g_KWCmdqBuffer kmalloc failed\n");
			 */
			if (g_pKWVirISPBuffer != NULL) {
				CMDQ_ERR(
				"g_KWVirISPBuffer is not NULL:(0x%pK)\n",
					g_pKWVirISPBuffer);
				vfree(g_pKWVirISPBuffer);
				g_pKWVirISPBuffer = NULL;
			}
			g_pKWVirISPBuffer = vmalloc(ISP_DIP_REG_SIZE);
			/* //mark for checkpatch, unnecessart msg
			 * if (g_pKWVirISPBuffer == NULL)
			 *	CMDQ_ERR(
			 *	"ERROR: g_KWVirISPBuffer kmalloc failed\n");
			 */
		}

		if (g_pPhyISPBuffer != NULL) {
			for (i = 0;
			i < (ISP_DIP_PHYSICAL_REG_SIZE >> 2); i = i + 4) {
				g_pPhyISPBuffer[i] =
					ISP_RD32(ISP_DIP_A_BASE + (i*4));
				g_pPhyISPBuffer[i+1] =
					ISP_RD32(ISP_DIP_A_BASE + ((i+1)*4));
				g_pPhyISPBuffer[i+2] =
					ISP_RD32(ISP_DIP_A_BASE + ((i+2)*4));
				g_pPhyISPBuffer[i+3] =
					ISP_RD32(ISP_DIP_A_BASE + ((i+3)*4));
			}
		} else {
			CMDQ_ERR("g_pPhyISPBuffer:(0x%pK)\n", g_pPhyISPBuffer);
		}
		g_dumpInfo.tdri_baseaddr = ISP_RD32(ISP_DIP_A_BASE + 0x204);
		g_dumpInfo.imgi_baseaddr = ISP_RD32(ISP_DIP_A_BASE + 0x400);
		g_dumpInfo.dmgi_baseaddr = ISP_RD32(ISP_DIP_A_BASE + 0x520);
		g_tdriaddr = g_dumpInfo.tdri_baseaddr;
		for (cmdqidx = 0; cmdqidx < 32 ; cmdqidx++) {
			if (ctrl_start & (0x1<<cmdqidx)) {
				g_cmdqaddr = ISP_RD32(
					ISP_DIP_A_BASE + 0x108 + (cmdqidx*12));
				break;
			}
		}
		if ((g_TpipeBaseAddrInfo.MemPa != 0)
			&& (g_TpipeBaseAddrInfo.MemVa != NULL)
			&& (g_pKWTpipeBuffer != NULL)) {
			/* to get frame tdri baseaddress, otherwide you
			 * possible get one of the tdr bade addr
			 */
			offset = ((g_tdriaddr &
				  (~(g_TpipeBaseAddrInfo.MemSizeDiff-1)))-
				  g_TpipeBaseAddrInfo.MemPa);
			OffsetAddr = ((uintptr_t)g_TpipeBaseAddrInfo.MemVa) +
					offset;
			if (copy_from_user(g_pKWTpipeBuffer,
			    (void __user *)(OffsetAddr),
			    MAX_ISP_TILE_TDR_HEX_NO) != 0) {
				CMDQ_ERR(
				"cpy tpipe fail. tdriaddr:0x%x, MemVa:0x%lx,MemPa:0x%x, offset:0x%x\n",
				g_tdriaddr,
				(uintptr_t)g_TpipeBaseAddrInfo.MemVa,
				g_TpipeBaseAddrInfo.MemPa, offset);
			}
		}
		CMDQ_ERR(
		"tdraddr:0x%x,MemVa:0x%lx,MemPa:0x%x,MemSizeDiff:0x%x,offset:0x%x,g_pKWTpipeBuffer:0x%pK\n",
		g_tdriaddr, (uintptr_t)g_TpipeBaseAddrInfo.MemVa,
		g_TpipeBaseAddrInfo.MemPa,
		g_TpipeBaseAddrInfo.MemSizeDiff, offset, g_pKWTpipeBuffer);
		if ((g_CmdqBaseAddrInfo.MemPa != 0)
				&& (g_CmdqBaseAddrInfo.MemVa != NULL)
				&& (g_pKWCmdqBuffer != NULL)
				&& (g_pKWVirISPBuffer != NULL)) {
			offset = (g_cmdqaddr-g_CmdqBaseAddrInfo.MemPa);
			OffsetAddr = ((uintptr_t)g_CmdqBaseAddrInfo.MemVa)+
					(g_cmdqaddr-g_CmdqBaseAddrInfo.MemPa);
			if (copy_from_user(g_pKWCmdqBuffer,
			    (void __user *)(OffsetAddr),
			    MAX_ISP_CMDQ_BUFFER_SIZE) != 0) {
				CMDQ_ERR(
				"cpy cmdq fail. cmdqaddr:0x%x, MemVa:0x%lx, MemPa:0x%x, offset:0x%x\n",
					g_cmdqaddr,
					(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
					g_CmdqBaseAddrInfo.MemPa, offset);
			}
			CMDQ_ERR(
			"cmdqidx:0x%x, cmdqaddr:0x%x, MemVa:0x%lx, MemPa:0x%x, offset:0x%x\n",
			cmdqidx, g_cmdqaddr,
			(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
			g_CmdqBaseAddrInfo.MemPa, offset);
			offset = offset + g_CmdqBaseAddrInfo.MemSizeDiff;
			OffsetAddr = ((uintptr_t)g_CmdqBaseAddrInfo.MemVa) +
					offset;
			if (copy_from_user(g_pKWVirISPBuffer,
			    (void __user *)(OffsetAddr),
			    ISP_DIP_REG_SIZE) != 0) {
				CMDQ_ERR(
				"cpy vir isp fail.cmdqaddr:0x%x,MVa:0x%lx,MPa:0x%x,MSzDiff:0x%x,offset:0x%x\n",
				g_cmdqaddr, (uintptr_t)g_CmdqBaseAddrInfo.MemVa,
				g_CmdqBaseAddrInfo.MemPa,
				g_CmdqBaseAddrInfo.MemSizeDiff, offset);
			}
			CMDQ_ERR(
			"cmdqaddr:0x%x,MVa:0x%lx,MPa:0x%x,MSzDiff:0x%x\n",
				g_cmdqaddr, (uintptr_t)g_CmdqBaseAddrInfo.MemVa,
				g_CmdqBaseAddrInfo.MemPa,
				g_CmdqBaseAddrInfo.MemSizeDiff);
			CMDQ_ERR("ofset:0x%x,KWCmdBuf:0x%pK,KWTdrBuf:0x%pK\n",
				offset, g_pKWCmdqBuffer, g_pKWTpipeBuffer);
		} else {
			CMDQ_ERR(
			"cmdqadd:0x%x,MVa:0x%lx,MPa:0x%x,MSzDiff:0x%x,KWCmdBuf:0x%pK,KWTdrBuf:0x%pK\n",
				g_cmdqaddr, (uintptr_t)g_CmdqBaseAddrInfo.MemVa,
				g_CmdqBaseAddrInfo.MemPa,
				g_CmdqBaseAddrInfo.MemSizeDiff, g_pKWCmdqBuffer,
				g_pKWTpipeBuffer);
		}
		g_bDumpPhyISPBuf = MTRUE;
	}

#else
		if (g_bDumpPhyISPBuf == MFALSE) {
			ctrl_start = ISP_RD32(ISP_DIP_A_BASE + 0x0000);
			if (g_pPhyISPBuffer != NULL) {
				for (i = 0;
				i < (ISP_DIP_PHYSICAL_REG_SIZE >> 2);
				i = i + 4) {
					g_pPhyISPBuffer[i] =
					  ISP_RD32(ISP_DIP_A_BASE + (i*4));
					g_pPhyISPBuffer[i+1] =
					  ISP_RD32(ISP_DIP_A_BASE + ((i+1)*4));
					g_pPhyISPBuffer[i+2] =
					  ISP_RD32(ISP_DIP_A_BASE + ((i+2)*4));
					g_pPhyISPBuffer[i+3] =
					  ISP_RD32(ISP_DIP_A_BASE + ((i+3)*4));
				}
			}
		} else {
			CMDQ_ERR("g_pPhyISPBuffer:(0x%pK)\n", g_pPhyISPBuffer);
		}
		g_dumpInfo.tdri_baseaddr = ISP_RD32(ISP_DIP_A_BASE + 0x204);
		g_dumpInfo.imgi_baseaddr = ISP_RD32(ISP_DIP_A_BASE + 0x400);
		g_dumpInfo.dmgi_baseaddr = ISP_RD32(ISP_DIP_A_BASE + 0x520);
		g_tdriaddr = g_dumpInfo.tdri_baseaddr;
		for (cmdqidx = 0; cmdqidx < 32 ; cmdqidx++) {
			if (ctrl_start & (0x1<<cmdqidx)) {
				g_cmdqaddr = ISP_RD32(
					ISP_DIP_A_BASE + 0x108 + (cmdqidx*12));
				break;
			}
		}
		if ((g_TpipeBaseAddrInfo.MemPa != 0) &&
		    (g_TpipeBaseAddrInfo.MemVa != NULL)) {
			/* to get frame tdri baseaddress, otherwide you
			 * possible get one of the tdr bade addr
			 */
			offset = ((g_tdriaddr &
				  (~(g_TpipeBaseAddrInfo.MemSizeDiff-1)))-
				 g_TpipeBaseAddrInfo.MemPa);
			OffsetAddr = ((uintptr_t)g_TpipeBaseAddrInfo.MemVa)+
					offset;
			if (copy_from_user(g_KWTpipeBuffer,
			    (void __user *)(OffsetAddr),
			    MAX_ISP_TILE_TDR_HEX_NO) != 0) {
				CMDQ_ERR(
				"cpy tpipe fail.tdriaddr:0x%x,MVa:0x%lx,MPa:0x%x,ofset:0x%x\n",
					g_tdriaddr,
					(uintptr_t)g_TpipeBaseAddrInfo.MemVa,
					g_TpipeBaseAddrInfo.MemPa, offset);
			}
			CMDQ_ERR(
			"tdriaddr:0x%x, MemVa:0x%lx, MemPa:0x%x, MemSizeDiff:0x%x, offset:0x%x\n",
				g_tdriaddr,
				(uintptr_t)g_TpipeBaseAddrInfo.MemVa,
				g_TpipeBaseAddrInfo.MemPa,
				g_TpipeBaseAddrInfo.MemSizeDiff, offset);
		}
		if ((g_CmdqBaseAddrInfo.MemPa != 0) &&
		    (g_CmdqBaseAddrInfo.MemVa != NULL)) {
			offset = (g_cmdqaddr-g_CmdqBaseAddrInfo.MemPa);
			OffsetAddr = ((uintptr_t)g_CmdqBaseAddrInfo.MemVa)+
					(g_cmdqaddr-g_CmdqBaseAddrInfo.MemPa);
			if (copy_from_user(g_KWCmdqBuffer,
			(void __user *)(OffsetAddr),
			MAX_ISP_CMDQ_BUFFER_SIZE) != 0) {
				CMDQ_ERR(
				"cpy cmdq fail. cmdqaddr:0x%x, MemVa:0x%lx, MemPa:0x%x, offset:0x%x\n",
					g_cmdqaddr,
					(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
					g_CmdqBaseAddrInfo.MemPa, offset);
			}
			CMDQ_ERR(
			"cmdqidx:0x%x, cmdqaddr:0x%x, MemVa:0x%lx, MemPa:0x%x, offset:0x%x\n",
				cmdqidx, g_cmdqaddr,
				(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
				g_CmdqBaseAddrInfo.MemPa, offset);
			offset = offset + g_CmdqBaseAddrInfo.MemSizeDiff;
			OffsetAddr = ((uintptr_t)g_CmdqBaseAddrInfo.MemVa) +
					offset;
			if (copy_from_user(g_KWVirISPBuffer,
			    (void __user *)(OffsetAddr),
				ISP_DIP_REG_SIZE) != 0) {
				CMDQ_ERR(
				"cpy vir isp fail.cmdqaddr:0x%x,MVa:0x%lx,MPa:0x%x,MSzDiff:0x%x,ofset:0x%x\n",
				g_cmdqaddr, (uintptr_t)g_CmdqBaseAddrInfo.MemVa,
				g_CmdqBaseAddrInfo.MemPa,
				g_CmdqBaseAddrInfo.MemSizeDiff, offset);
			}
			CMDQ_ERR(
			"cmdqaddr:0x%x, MemVa:0x%lx, MemPa:0x%x, MemSizeDiff:0x%x, offset:0x%x\n",
			g_cmdqaddr, (uintptr_t)g_CmdqBaseAddrInfo.MemVa,
			g_CmdqBaseAddrInfo.MemPa,
			g_CmdqBaseAddrInfo.MemSizeDiff, offset);
		}
		g_bDumpPhyISPBuf = MTRUE;
	}
#endif

	CMDQ_ERR(
	"direct link:15020030(0x%x),g_bDumpPhyISPBuf:(0x%x),cmdqidx(0x%x),cmdqaddr(0x%x),tdriaddr(0x%x)\n",
		ISP_RD32(ISP_IMGSYS_CONFIG_BASE + 0x0030), g_bDumpPhyISPBuf,
		cmdqidx, g_cmdqaddr, g_tdriaddr);
	CMDQ_ERR(
	"isp: 15022000(0x%x)-15022004(0x%x)-15022008(0x%x)-1502200C(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0000),
		ISP_RD32(ISP_DIP_A_BASE + 0x0004),
		ISP_RD32(ISP_DIP_A_BASE + 0x0008),
		ISP_RD32(ISP_DIP_A_BASE + 0x000C));
	CMDQ_ERR(
	"isp: 15022010(0x%x)-15022014(0x%x)-15022018(0x%x)-1502201C(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0010),
		ISP_RD32(ISP_DIP_A_BASE + 0x0014),
		ISP_RD32(ISP_DIP_A_BASE + 0x0018),
		ISP_RD32(ISP_DIP_A_BASE + 0x001C));
	CMDQ_ERR(
	"isp: 15022D20(0x%x)-15022D24(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0D20),
		ISP_RD32(ISP_DIP_A_BASE + 0x0D24));
	CMDQ_ERR(
	"isp: 15022408(0x%x)-15022204(0x%x)-15022208(0x%x)-1502220C(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0408),
		ISP_RD32(ISP_DIP_A_BASE + 0x0204),
		ISP_RD32(ISP_DIP_A_BASE + 0x0208),
		ISP_RD32(ISP_DIP_A_BASE + 0x020C));
	CMDQ_ERR(
	"isp: 15022050(0x%x)-15022054(0x%x)-15022058(0x%x)-1502205C(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0050),
		ISP_RD32(ISP_DIP_A_BASE + 0x0054),
		ISP_RD32(ISP_DIP_A_BASE + 0x0058),
		ISP_RD32(ISP_DIP_A_BASE + 0x005C));
	CMDQ_ERR(
	"isp: 150220B8(0x%x)-150220BC(0x%x)-150220C0(0x%x)-150220C4(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x00B8),
		ISP_RD32(ISP_DIP_A_BASE + 0x00BC),
		ISP_RD32(ISP_DIP_A_BASE + 0x00C0),
		ISP_RD32(ISP_DIP_A_BASE + 0x00C4));
	CMDQ_ERR(
	"isp: 150220C8(0x%x)-150220CC(0x%x)-150220D0(0x%x)-150220D4(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x00C8),
		ISP_RD32(ISP_DIP_A_BASE + 0x00CC),
		ISP_RD32(ISP_DIP_A_BASE + 0x00D0),
		ISP_RD32(ISP_DIP_A_BASE + 0x00D4));
	CMDQ_ERR(
	"isp: 15022104(0x%x)-15022108(0x%x)-1502211C(0x%x)-15022120(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0104),
		ISP_RD32(ISP_DIP_A_BASE + 0x0108),
		ISP_RD32(ISP_DIP_A_BASE + 0x011C),
		ISP_RD32(ISP_DIP_A_BASE + 0x0120));
	CMDQ_ERR(
	"isp: 15022128(0x%x)-1502212C(0x%x)-15022134(0x%x)-15022138(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0128),
		ISP_RD32(ISP_DIP_A_BASE + 0x012C),
		ISP_RD32(ISP_DIP_A_BASE + 0x0134),
		ISP_RD32(ISP_DIP_A_BASE + 0x0138));
	CMDQ_ERR(
	"isp: 15022140(0x%x)-15022144(0x%x)-1502214C(0x%x)-15022150(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0140),
		ISP_RD32(ISP_DIP_A_BASE + 0x0144),
		ISP_RD32(ISP_DIP_A_BASE + 0x014C),
		ISP_RD32(ISP_DIP_A_BASE + 0x0150));
	CMDQ_ERR(
	"isp: 15022158(0x%x)-1502215C(0x%x)-15022164(0x%x)-15022168(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0158),
		ISP_RD32(ISP_DIP_A_BASE + 0x015C),
		ISP_RD32(ISP_DIP_A_BASE + 0x0164),
		ISP_RD32(ISP_DIP_A_BASE + 0x0168));
	CMDQ_ERR(
	"isp: 15022170(0x%x)-15022174(0x%x)-1502217C(0x%x)-15022180(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0170),
		ISP_RD32(ISP_DIP_A_BASE + 0x0174),
		ISP_RD32(ISP_DIP_A_BASE + 0x017C),
		ISP_RD32(ISP_DIP_A_BASE + 0x0180));
	CMDQ_ERR(
	"isp: 15022188(0x%x)-1502218C(0x%x)-15022194(0x%x)-15022198(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0188),
		ISP_RD32(ISP_DIP_A_BASE + 0x018C),
		ISP_RD32(ISP_DIP_A_BASE + 0x0194),
		ISP_RD32(ISP_DIP_A_BASE + 0x0198));
	CMDQ_ERR(
	"isp: 15022060(0x%x)-15022064(0x%x)-15022068(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0060),
		ISP_RD32(ISP_DIP_A_BASE + 0x0064),
		ISP_RD32(ISP_DIP_A_BASE + 0x0068));

	/* 0080, 0x15022080, DIP_A_CTL_DBG_SET */
	ISP_WR32(ISP_DIP_A_BASE + 0x80, 0x0);
	dipdmacmd = 0x00000013;
	for (i = 0; i < 68 ; i++) {
		/* 0x150210A8, DIPDMATOP_REG_D1A_DIPDMATOP_DMA_DEBUG_SEL */
		switch (i) {
		case 0x0:
			dmaidx = 1;
			CMDQ_ERR("imgi dma debug\n");
			break;
		case 0x4:
			dmaidx = 2;
			CMDQ_ERR("imgbi dma debug\n");
			break;
		case 0x8:
			dmaidx = 4;
			CMDQ_ERR("vipi dma debug\n");
			break;
		case 0xc:
			dmaidx = 5;
			CMDQ_ERR("vip2i dma debug\n");
			break;
		case 0x10:
			dmaidx = 11;
			CMDQ_ERR("img2o dma debug\n");
			break;
		case 0x14:
			dmaidx = 12;
			CMDQ_ERR("img2bo dma debug\n");
			break;
		case 0x18:
			dmaidx = 13;
			CMDQ_ERR("img3o dma debug\n");
			break;
		case 0x1c:
			dmaidx = 14;
			CMDQ_ERR("img3bo dma debug\n");
			break;
		case 0x20:
			dmaidx = 15;
			CMDQ_ERR("img3co dma debug\n");
			break;
		case 0x24:
			dmaidx = 16;
			CMDQ_ERR("feo dma debug\n");
			break;
		case 0x28:
			dmaidx = 17;
			CMDQ_ERR("mfbo dma debug\n");
			break;
		case 0x2c:
			dmaidx = 3;
			CMDQ_ERR("imgci dma debug\n");
			break;
		case 0x30:
			dmaidx = 6;
			CMDQ_ERR("vip3i dma debug\n");
			break;
		case 0x34:
			dmaidx = 7;
			CMDQ_ERR("dmgi dma debug\n");
			break;
		case 0x38:
			dmaidx = 8;
			CMDQ_ERR("depi dma debug\n");
			break;
		case 0x3c:
			dmaidx = 9;
			CMDQ_ERR("lcei dma debug\n");
			break;
		case 0x40:
			dmaidx = 10;
			CMDQ_ERR("ufdi dma debug\n");
			break;
		default:
			break;
		}

		if (i <= 0x1c) {
			dipdmacmd = dipdmacmd & 0xFFFF00FF;
			dipdmacmd = dipdmacmd | (i << 8);
			/* 06BC, 0x150226BC, DIP_A_DMA_DEBUG_SEL */
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, dipdmacmd);
			/* 0084, 0x15022084, DIP_A_CTL_DBG_PORT */
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				dipdmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));
			i++;
			dipdmacmd = dipdmacmd & 0xFFFF00FF;
			dipdmacmd = dipdmacmd | (i << 8);
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, dipdmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				dipdmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));
			i++;
			dipdmacmd = dipdmacmd & 0xFFFF00FF;
			dipdmacmd = dipdmacmd | (i << 8);
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, dipdmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				dipdmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));
			i++;
			dipdmacmd = dipdmacmd & 0xFFFF00FF;
			dipdmacmd = dipdmacmd | (i << 8);
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, dipdmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				dipdmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));
		}

		if (((dmaidx >= 11) && (dmaidx <= 17))) {
			smidmacmd = 0x00080400;
			smidmacmd = smidmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, smidmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				smidmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));

			smidmacmd = 0x00090400;
			smidmacmd = smidmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, smidmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				smidmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));

			smidmacmd = 0x00000400;
			smidmacmd = smidmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, smidmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				smidmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));

			smidmacmd = 0x00010400;
			smidmacmd = smidmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, smidmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				smidmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));

			fifodmacmd = 0x00000300;
			fifodmacmd = fifodmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));

			fifodmacmd = 0x00010300;
			fifodmacmd = fifodmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));

			fifodmacmd = 0x00020300;
			fifodmacmd = fifodmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));

			fifodmacmd = 0x00030300;
			fifodmacmd = fifodmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));

		} else {
			smidmacmd = 0x00080100;
			smidmacmd = smidmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, smidmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				smidmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));

			smidmacmd = 0x00000100;
			smidmacmd = smidmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, smidmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				smidmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));

			fifodmacmd = 0x00000200;
			fifodmacmd = fifodmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));

			fifodmacmd = 0x00010200;
			fifodmacmd = fifodmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));

			fifodmacmd = 0x00020200;
			fifodmacmd = fifodmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));

			fifodmacmd = 0x00030200;
			fifodmacmd = fifodmacmd | dmaidx;
			ISP_WR32(ISP_DIP_A_BASE + 0x6BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, ISP_RD32(ISP_DIP_A_BASE + 0x084));
		}
	}
		/* 0080, 0x15022080, DIP_A_CTL_DBG_SET */
		ISP_WR32(ISP_DIP_A_BASE + 0x80, 0x0);
		/* 06BC, 0x150226BC, DIP_A_DMA_DEBUG_SEL */
		ISP_WR32(ISP_DIP_A_BASE + 0x6BC, 0x000013);
		/* 0084, 0x15022084, DIP_A_CTL_DBG_PORT */
		CMDQ_ERR("0x000013 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x6BC, 0x000113);
		CMDQ_ERR("0x000113 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x6BC, 0x000213);
		CMDQ_ERR("0x000213 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x6BC, 0x000313);
		CMDQ_ERR("0x000313 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		/* IMG2O */
		/* 06BC, 0x150226BC, DIP_A_DMA_DEBUG_SEL */
		ISP_WR32(ISP_DIP_A_BASE + 0x6BC, 0x001013);
		/* 0084, 0x15022084, DIP_A_CTL_DBG_PORT */
		CMDQ_ERR("0x001013 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x6BC, 0x001113);
		CMDQ_ERR("0x001113 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x6BC, 0x001213);
		CMDQ_ERR("0x001213 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x6BC, 0x001313);
		CMDQ_ERR("0x001313 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		/*IMG3O */
		/* 06BC, 0x150226BC, DIP_A_DMA_DEBUG_SEL */
		ISP_WR32(ISP_DIP_A_BASE + 0x6BC, 0x001813);
		/* 0084, 0x15022084, DIP_A_CTL_DBG_PORT */
		CMDQ_ERR("0x001813 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x6BC, 0x001913);
		CMDQ_ERR("0x001913 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x6BC, 0x001A13);
		CMDQ_ERR("0x001A13 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x6BC, 0x001B13);
		CMDQ_ERR("0x001B13 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x080, 0x003016);
		CMDQ_ERR("0x003016 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x080, 0x003017);
		CMDQ_ERR("0x003017 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x080, 0x003018);
		CMDQ_ERR("0x003018 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x080, 0x003019);
		CMDQ_ERR("0x003019 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		ISP_WR32(ISP_DIP_A_BASE + 0x080, 0x005100);
		CMDQ_ERR("0x005100 : isp: 0x15022084(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x084));
		/* DMA Error */
		CMDQ_ERR("img2o  0x15022644(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x644));
		CMDQ_ERR("img2bo 0x15022648(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x648));
		CMDQ_ERR("img3o  0x1502264C(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x64C));
		CMDQ_ERR("img3bo 0x15022650(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x650));
		CMDQ_ERR("img3Co 0x15022654(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x654));
		CMDQ_ERR("feo    0x15022658(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x658));
		CMDQ_ERR("mfbo   0x1502265C(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x65C));
		CMDQ_ERR("imgi   0x15022660(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x660));
		CMDQ_ERR("imgbi  0x15022664(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x664));
		CMDQ_ERR("imgci  0x15022668(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x668));
		CMDQ_ERR("vipi   0x1502266c(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x66c));
		CMDQ_ERR("vip2i  0x15022670(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x670));
		CMDQ_ERR("vip3i  0x15022674(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x674));
		CMDQ_ERR("dmgi   0x15022678(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x678));
		CMDQ_ERR("depi   0x1502267c(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x67C));
		CMDQ_ERR("lcei   0x15022680(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x680));
		CMDQ_ERR("ufdi   0x15022684(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x684));
		CMDQ_ERR("CTL_INT_STATUSX      0x15022040(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x040));
		CMDQ_ERR("CTL_CQ_INT_STATUSX   0x15022044(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x044));
		CMDQ_ERR("CTL_CQ_INT2_STATUSX  0x15022048(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x048));
		CMDQ_ERR("CTL_CQ_INT3_STATUSX  0x1502204C(0x%x)",
			ISP_RD32(ISP_DIP_A_BASE + 0x04C));
	CMDQ_ERR(
	"img3o: 0x15022290(0x%x)-0x15022298(0x%x)-0x150222A0(0x%x)-0x150222A4(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x290),
		ISP_RD32(ISP_DIP_A_BASE + 0x0298),
		ISP_RD32(ISP_DIP_A_BASE + 0x2A0),
		ISP_RD32(ISP_DIP_A_BASE + 0x02A4));
	CMDQ_ERR(
	"img3o: 0x150222A8(0x%x)-0x150222AC(0x%x)-0x150222B0(0x%x)-0x150222B4(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x02A8),
		ISP_RD32(ISP_DIP_A_BASE + 0x02AC),
		ISP_RD32(ISP_DIP_A_BASE + 0x02B0),
		ISP_RD32(ISP_DIP_A_BASE + 0x02B4));
	CMDQ_ERR(
	"img3o: 0x150222B8(0x%x)\n", ISP_RD32(ISP_DIP_A_BASE + 0x02B8));

	CMDQ_ERR(
	"imgi: 0x15022400(0x%x)-0x15022408(0x%x)-0x15022410(0x%x)-0x15022414(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x400),
		ISP_RD32(ISP_DIP_A_BASE + 0x408),
		ISP_RD32(ISP_DIP_A_BASE + 0x410),
		ISP_RD32(ISP_DIP_A_BASE + 0x414));
	CMDQ_ERR(
	"imgi: 0x15022418(0x%x)-0x1502241C(0x%x)-0x15022420(0x%x)-0x15022424(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x418),
		ISP_RD32(ISP_DIP_A_BASE + 0x41C),
		ISP_RD32(ISP_DIP_A_BASE + 0x420),
		ISP_RD32(ISP_DIP_A_BASE + 0x424));

	CMDQ_ERR(
	"mfbo: 0x15022350(0x%x)-0x15022358(0x%x)-0x15022360(0x%x)-0x15022364(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x350),
		ISP_RD32(ISP_DIP_A_BASE + 0x358),
		ISP_RD32(ISP_DIP_A_BASE + 0x360),
		ISP_RD32(ISP_DIP_A_BASE + 0x364));
	CMDQ_ERR(
	"mfbo: 0x15022368(0x%x)-0x1502236C(0x%x)-0x15022370(0x%x)-0x15022374(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x368),
		ISP_RD32(ISP_DIP_A_BASE + 0x36C),
		ISP_RD32(ISP_DIP_A_BASE + 0x370),
		ISP_RD32(ISP_DIP_A_BASE + 0x374));
	CMDQ_ERR("mfbo: 0x15022378(0x%x)\n", ISP_RD32(ISP_DIP_A_BASE + 0x378));

	CMDQ_ERR(
	"img2o: 0x15022230(0x%x)-0x15022238(0x%x)-0x15022240(0x%x)-0x15022244(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x230),
		ISP_RD32(ISP_DIP_A_BASE + 0x238),
		ISP_RD32(ISP_DIP_A_BASE + 0x240),
		ISP_RD32(ISP_DIP_A_BASE + 0x244));
	CMDQ_ERR(
	"img2o: 0x15022248(0x%x)-0x1502224C(0x%x)-0x15022250(0x%x)-0x15022254(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0248),
		ISP_RD32(ISP_DIP_A_BASE + 0x024C),
		ISP_RD32(ISP_DIP_A_BASE + 0x0250),
		ISP_RD32(ISP_DIP_A_BASE + 0x0254));
	CMDQ_ERR(
	"img2o: 0x15022258(0x%x)\n", ISP_RD32(ISP_DIP_A_BASE + 0x0258));

	CMDQ_ERR(
	"lcei: 0x15022580(0x%x)-0x15022588(0x%x)-0x15022590(0x%x)-0x15022594(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x580),
		ISP_RD32(ISP_DIP_A_BASE + 0x588),
		ISP_RD32(ISP_DIP_A_BASE + 0x590),
		ISP_RD32(ISP_DIP_A_BASE + 0x594));
	CMDQ_ERR(
	"lcei: 0x15022598(0x%x)-0x1502259C(0x%x)-0x150225A0(0x%x)-0x150225A4(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0598),
		ISP_RD32(ISP_DIP_A_BASE + 0x059C),
		ISP_RD32(ISP_DIP_A_BASE + 0x05A0),
		ISP_RD32(ISP_DIP_A_BASE + 0x05A4));

	CMDQ_ERR(
	"crz: 0x15022C10(0x%x)-0x15022C14(0x%x)-0x15022C18(0x%x)-0x15022C1C(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0xC10),
		ISP_RD32(ISP_DIP_A_BASE + 0xC14),
		ISP_RD32(ISP_DIP_A_BASE + 0xC18),
		ISP_RD32(ISP_DIP_A_BASE + 0xC1C));
	CMDQ_ERR(
	"crz: 0x15022C20(0x%x)-0x15022C24(0x%x)-0x15022C28(0x%x)-0x15022C2C(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0C20),
		ISP_RD32(ISP_DIP_A_BASE + 0x0C24),
		ISP_RD32(ISP_DIP_A_BASE + 0x0C28),
		ISP_RD32(ISP_DIP_A_BASE + 0x0C2C));
	CMDQ_ERR(
	"crz: 0x15022C30(0x%x)-0x15022C34(0x%x)-0x15022C38(0x%x)-0x15022C3C(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0C30),
		ISP_RD32(ISP_DIP_A_BASE + 0x0C34),
		ISP_RD32(ISP_DIP_A_BASE + 0x0C38),
		ISP_RD32(ISP_DIP_A_BASE + 0x0C3C));
	CMDQ_ERR("crz: 0x15022C40(0x%x)-0x15022C44(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0C40),
		ISP_RD32(ISP_DIP_A_BASE + 0x0C44));

	CMDQ_ERR(
	"mfb: 0x15022F60(0x%x)-0x15022F64(0x%x)-0x15022F68(0x%x)-0x15022F6C(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0xF60),
		ISP_RD32(ISP_DIP_A_BASE + 0xF64),
		ISP_RD32(ISP_DIP_A_BASE + 0xF68),
		ISP_RD32(ISP_DIP_A_BASE + 0xF6C));
	CMDQ_ERR(
	"mfb: 0x15022F70(0x%x)-0x15022F74(0x%x)-0x15022F78(0x%x)-0x15022F7C(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0F70),
		ISP_RD32(ISP_DIP_A_BASE + 0x0F74),
		ISP_RD32(ISP_DIP_A_BASE + 0x0F78),
		ISP_RD32(ISP_DIP_A_BASE + 0x0F7C));
	CMDQ_ERR(
	"mfb: 0x15022F80(0x%x)-0x15022F84(0x%x)-0x15022F88(0x%x)-0x15022F8C(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0F80),
		ISP_RD32(ISP_DIP_A_BASE + 0x0F84),
		ISP_RD32(ISP_DIP_A_BASE + 0x0F88),
		ISP_RD32(ISP_DIP_A_BASE + 0x0F8C));
	CMDQ_ERR(
	"mfb: 0x15022F90(0x%x)-0x15022F94(0x%x)-0x15022F98(0x%x)-0x15022F9C(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0F90),
		ISP_RD32(ISP_DIP_A_BASE + 0x0F94),
		ISP_RD32(ISP_DIP_A_BASE + 0x0F98),
		ISP_RD32(ISP_DIP_A_BASE + 0x0F9C));
	CMDQ_ERR(
	"mfb: 0x15022FA0(0x%x)-0x15022FA4(0x%x)-0x15022FA8(0x%x)-0x15022FAC(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0FA0),
		ISP_RD32(ISP_DIP_A_BASE + 0x0FA4),
		ISP_RD32(ISP_DIP_A_BASE + 0x0FA8),
		ISP_RD32(ISP_DIP_A_BASE + 0x0FAC));
	CMDQ_ERR(
	"mfb: 0x15022FB0(0x%x)-0x15022FB4(0x%x)-0x15022FB8(0x%x)\n",
		ISP_RD32(ISP_DIP_A_BASE + 0x0FB0),
		ISP_RD32(ISP_DIP_A_BASE + 0x0FB4),
		ISP_RD32(ISP_DIP_A_BASE + 0x0FB8));

	CMDQ_ERR("- X.");
	/*  */
	return Ret;
}

#ifndef CONFIG_MTK_CLKMGR /*CCF*/

static inline void Prepare_Enable_ccf_clock(void)
{
	int ret;
	/* must keep this clk open order: CG_SCP_SYS_DIS-> CG_DISP0_SMI_COMMON
	 * -> CG_SCP_SYS_ISP/CAM -> ISP clk
	 */

	#ifndef EP_MARK_SMI
	/* enable through smi API */
	/* pr_info("enable CG/MTCMOS through SMI CLK API\n"); */
	smi_bus_prepare_enable(SMI_LARB3, ISP_DEV_NAME);
	smi_bus_prepare_enable(SMI_LARB2, ISP_DEV_NAME);
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

	ret = clk_prepare_enable(isp_clk.ISP_IMG_DIP);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_IMG_DIP clock\n");

	ret = clk_prepare_enable(isp_clk.ISP_CAM_CAMSYS);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_CAM_CAMSYS clock\n");

	ret = clk_prepare_enable(isp_clk.ISP_CAM_CAMTG);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_CAM_CAMTG clock\n");

	ret = clk_prepare_enable(isp_clk.ISP_CAM_SENINF);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_CAM_SENINF clock\n");

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
	/* must keep this clk close order: ISP clk
	 * -> CG_SCP_SYS_ISP/CAM -> CG_DISP0_SMI_COMMON -> CG_SCP_SYS_DIS
	 */
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMSV2);
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMSV1);
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMSV0);
	clk_disable_unprepare(isp_clk.ISP_CAM_SENINF);
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMTG);
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMSYS);
	clk_disable_unprepare(isp_clk.ISP_IMG_DIP);
	clk_disable_unprepare(isp_clk.ISP_SCP_SYS_CAM);
	clk_disable_unprepare(isp_clk.ISP_SCP_SYS_ISP);
	clk_disable_unprepare(isp_clk.ISP_SCP_SYS_DIS);

	#ifndef EP_MARK_SMI
	/* pr_info("disable CG/MTCMOS through SMI CLK API\n"); */
	smi_bus_disable_unprepare(SMI_LARB2, ISP_DEV_NAME);
	smi_bus_disable_unprepare(SMI_LARB3, ISP_DEV_NAME);
	#endif
}

/* only for suspend/resume, disable isp cg but no MTCMOS*/
static inline void Prepare_Enable_cg_clock(void)
{
	int ret;

	#ifndef EP_MARK_SMI
	pr_info("enable CG through SMI CLK API\n");
	smi_bus_prepare_enable(SMI_LARB3, ISP_DEV_NAME);
	#endif

	ret = clk_prepare_enable(isp_clk.ISP_IMG_DIP);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_IMG_DIP clock\n");

	ret = clk_prepare_enable(isp_clk.ISP_CAM_CAMSYS);
	if (ret)
		LOG_NOTICE("cannot pre-en ISP_CAM_CAMSYS clock\n");

/*	ret = clk_prepare_enable(isp_clk.ISP_CAM_CAMTG);
 *	if (ret)
 *		LOG_NOTICE("cannot pre-en ISP_CAM_CAMTG clock\n");
 *
 *	ret = clk_prepare_enable(isp_clk.ISP_CAM_SENINF);
 *	if (ret)
 *		LOG_NOTICE("cannot pre-en ISP_CAM_SENINF clock\n");
 */

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

static inline void Disable_Unprepare_cg_clock(void)
{
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMSV2);
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMSV1);
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMSV0);
	/*
	 * clk_disable_unprepare(isp_clk.ISP_CAM_SENINF);
	 * clk_disable_unprepare(isp_clk.ISP_CAM_CAMTG);
	 */
	clk_disable_unprepare(isp_clk.ISP_CAM_CAMSYS);
	clk_disable_unprepare(isp_clk.ISP_IMG_DIP);

	#ifndef EP_MARK_SMI
	pr_info("disable CG through SMI CLK API\n");
	smi_bus_disable_unprepare(SMI_LARB3, ISP_DEV_NAME);
	#endif
}

#endif

/******************************************************************************
 *
 *****************************************************************************/

void ISP_Halt_Mask(unsigned int isphaltMask)
{
	unsigned int setReg;

	setReg = ISP_RD32(ISP_CAMSYS_CONFIG_BASE + 0x120) &
		~((unsigned int)(1 << (isphaltMask)));

	ISP_WR32(ISP_CAMSYS_CONFIG_BASE + 0x120, setReg);

	pr_info("ISP halt_en for dvfs:0x%x\n",
		ISP_RD32(ISP_CAMSYS_CONFIG_BASE + 0x120));
}
EXPORT_SYMBOL(ISP_Halt_Mask);

/******************************************************************************
 *
 *****************************************************************************/
static void ISP_EnableClock(bool En)
{
#if defined(EP_NO_CLKMGR)
	unsigned int setReg;
#endif
	if (En) {
#if defined(EP_NO_CLKMGR) || defined(CONFIG_MTK_CLKMGR)
		spin_lock(&(IspInfo.SpinLockClock));
		/* pr_info("Camera clock enbled. G_u4EnableClockCount: %d.",
		 *         G_u4EnableClockCount);
		 */
		switch (G_u4EnableClockCount) {
		case 0:
#ifdef EP_NO_CLKMGR  /*FPGA test*/
			/* Enable clock by hardcode:
			 * 1. CAMSYS_CG_CLR (0x1A000008) = 0xffffffff;
			 * 2. IMG_CG_CLR (0x15000008) = 0xffffffff;
			 */
			setReg = 0xFFFFFFFF;
			ISP_WR32(CAMSYS_REG_CG_CLR, setReg);
			ISP_WR32(IMGSYS_REG_CG_CLR, setReg);
#else /* 6797 not support CLKMGR, only CCF!!*/
			/*pr_info("MTK_LEGACY:enable clk");*/
			enable_clock(MT_CG_DISP0_SMI_COMMON, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			/* enable_clock(MT_CG_IMAGE_FD, "CAMERA"); */
			enable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
#endif
			break;
		default:
			break;
		}
		G_u4EnableClockCount++;
		spin_unlock(&(IspInfo.SpinLockClock));
#else/*CCF*/
		/*pr_info("CCF:prepare_enable clk");*/
		spin_lock(&(IspInfo.SpinLockClock));
		if (G_u4EnableClockCount == 0) {
			unsigned int _reg = ISP_RD32(CLOCK_CELL_BASE);

			ISP_WR32(CLOCK_CELL_BASE, _reg|(1<<6));
		}
		G_u4EnableClockCount++;
		spin_unlock(&(IspInfo.SpinLockClock));
		Prepare_Enable_ccf_clock(); /* can't be used in spinlock! */
#endif
	/* Disable CAMSYS_HALT1_EN: LSCI&BPCI, To avoid ISP halt keep arise */
		#if 0/* TBD */
	ISP_WR32(ISP_CAMSYS_CONFIG_BASE + 0x120, 0xFFFFFF4F);
		#endif
	} else {                /* Disable clock. */
#if defined(EP_NO_CLKMGR) || defined(CONFIG_MTK_CLKMGR)
		spin_lock(&(IspInfo.SpinLockClock));
		/* pr_info("Camera clock disabled. G_u4EnableClockCount: %d.",
		 *         G_u4EnableClockCount);
		 */
		G_u4EnableClockCount--;
		switch (G_u4EnableClockCount) {
		case 0:
#ifdef EP_NO_CLKMGR

			/* Disable clock by hardcode:
			 * 1. CAMSYS_CG_SET (0x1A000004) = 0xffffffff;
			 * 2. IMG_CG_SET (0x15000004) = 0xffffffff;
			 */
			setReg = 0xFFFFFFFF;
			ISP_WR32(CAMSYS_REG_CG_SET, setReg);
			ISP_WR32(IMGSYS_REG_CG_SET, setReg);
#else
			/*pr_info("MTK_LEGACY:disable clk");*/
			/* do disable clock     */
			disable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			disable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			disable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			/* disable_clock(MT_CG_IMAGE_FD, "CAMERA"); */
			disable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
			disable_clock(MT_CG_DISP0_SMI_COMMON, "CAMERA");
#endif
			break;
		default:
			break;
		}
		spin_unlock(&(IspInfo.SpinLockClock));
#else
		/*pr_info("CCF:disable_unprepare clk\n");*/
		spin_lock(&(IspInfo.SpinLockClock));
		if (G_u4EnableClockCount == 0) {
			spin_unlock(&(IspInfo.SpinLockClock));
			pr_info(
			"G_u4EnableClockCount aleady be 0, do nothing\n");
			return;
		}

		G_u4EnableClockCount--;
		if (G_u4EnableClockCount == 0) {
			unsigned int _reg = ISP_RD32(CLOCK_CELL_BASE);

			ISP_WR32(CLOCK_CELL_BASE, _reg&(~(1<<6)));
		}
		spin_unlock(&(IspInfo.SpinLockClock));
		Disable_Unprepare_ccf_clock(); /* can't be used in spinlock! */
#endif
	}
}



/******************************************************************************
 *
 *****************************************************************************/
static inline void ISP_Reset(signed int module)
{
	/*    unsigned int Reg;*/
	/*    unsigned int setReg;*/

	pr_info("- E.\n");

	pr_info(
	"Reset module(%d), CAMSYS clk gate(0x%x), IMGSYS clk gate(0x%x)\n",
		module, ISP_RD32(CAMSYS_REG_CG_CON),
		ISP_RD32(IMGSYS_REG_CG_CON));

	switch (module) {
	case ISP_CAM_A_IDX:
	case ISP_CAM_B_IDX: {
		/* Reset CAM flow */
		ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x2);
		ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x1);
#if 0
		while (ISP_RD32(CAM_REG_CTL_SW_CTL(module)) != 0x2)
			pr_info("CAM resetting... module:%d\n", module);
#else
		mdelay(1);
#endif
		ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x4);
		ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x0);

		break;
	}
	case ISP_CAMSV0_IDX:
	case ISP_CAMSV1_IDX:
	case ISP_CAMSV2_IDX:
	case ISP_CAMSV3_IDX:
	case ISP_CAMSV4_IDX:
	case ISP_CAMSV5_IDX: {
		/* Reset CAMSV flow */
		ISP_WR32(CAMSV_REG_SW_CTL(module), 0x4); /* SW_RST: 1 */
		ISP_WR32(CAMSV_REG_SW_CTL(module), 0x0); /* SW_RST: 0 */
		ISP_WR32(CAMSV_REG_SW_CTL(module), 0x1); /* IMGO_RST_TRIG: 1 */
		/* Polling IMGO_RST_ST to 1 */
		while (ISP_RD32(CAMSV_REG_SW_CTL(module)) != 0x3)
			pr_info("CAMSV resetting... module:%d\n", module);

		ISP_WR32(CAMSV_REG_SW_CTL(module), 0x0); /* IMGO_RST_TRIG: 0 */

		break;
	}
	case ISP_UNI_A_IDX: {
		/* Reset UNI flow */
		ISP_WR32(CAM_UNI_REG_TOP_SW_CTL(module), 0x222);
		ISP_WR32(CAM_UNI_REG_TOP_SW_CTL(module), 0x111);
		ISP_WR32(CAM_UNI_REG_B_TOP_SW_CTL(module), 0x222);
		ISP_WR32(CAM_UNI_REG_B_TOP_SW_CTL(module), 0x222);
#if 0
		while (ISP_RD32(CAM_UNI_REG_TOP_SW_CTL(module)) != 0x222)
			pr_info("UNI resetting... module:%d\n", module);
#else
		mdelay(1);
#endif
		ISP_WR32(CAM_UNI_REG_TOP_SW_CTL(module), 0x0444);
		ISP_WR32(CAM_UNI_REG_TOP_SW_CTL(module), 0x0);
		ISP_WR32(CAM_UNI_REG_B_TOP_SW_CTL(module), 0x0444);
		ISP_WR32(CAM_UNI_REG_B_TOP_SW_CTL(module), 0x0444);
		break;
	}
	case ISP_DIP_A_IDX: {

		/* Reset DIP flow */

		break;
	}
	default:
		LOG_NOTICE("Not support reset module:%d\n", module);
		break;
	}

}

/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_ReadReg(struct ISP_REG_IO_STRUCT *pRegIo)
{
	unsigned int i;
	signed int Ret = 0;

	unsigned int module;
	void __iomem *regBase;

	/*  */
	struct ISP_REG_STRUCT reg;
	/* unsigned int* pData = (unsigned int*)pRegIo->Data; */
	struct ISP_REG_STRUCT *pData = (struct ISP_REG_STRUCT *)pRegIo->pData;

	if ((pRegIo->pData == NULL) ||
			(pRegIo->Count == 0) ||
			(pRegIo->Count > ISP_REG_RANGE)) {
		LOG_NOTICE(
			"pRegIo->pData is NULL, Count:%d!!\n",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}

	if (get_user(module, (unsigned int *)&pData->module) != 0) {
		LOG_NOTICE("get_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}

	switch (module) {
	case ISP_CAM_A_IDX:
		regBase = ISP_CAM_A_BASE;
		break;
	case ISP_CAM_B_IDX:
		regBase = ISP_CAM_B_BASE;
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
	case ISP_DIP_A_IDX:
		regBase = ISP_DIP_A_BASE;
		break;
	case ISP_UNI_A_IDX:
		regBase = ISP_CAM_UNI_BASE;
		break;
	case 0xFF:
		regBase = ISP_SENINF0_BASE;
		break;
	case 0xFE:
		regBase = ISP_SENINF1_BASE;
		break;
	default:
		LOG_NOTICE("Unsupported module(%x) !!!\n", module);
		Ret = -EFAULT;
		goto EXIT;
	}


	for (i = 0; i < pRegIo->Count; i++) {
		if (get_user(reg.Addr, (unsigned int *)&pData->Addr) != 0) {
			LOG_NOTICE("get_user failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}
		/* pData++; */
		/*  */
		if (((regBase + reg.Addr) < (regBase + PAGE_SIZE))
			&& ((reg.Addr & 0x3) == 0)) {
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
 *****************************************************************************/
/* Note: Can write sensor's test model only,
 * if need write to other modules, need modify current code flow
 */
static signed int ISP_WriteRegToHw(
	struct ISP_REG_STRUCT *pReg,
	unsigned int         Count)
{
	signed int Ret = 0;
	unsigned int i;
	bool dbgWriteReg;
	unsigned int module;
	void __iomem *regBase;

	/* Use local variable to store IspInfo.DebugMask & ISP_DBG_WRITE_REG
	 * for saving lock time
	 */
	spin_lock(&(IspInfo.SpinLockIsp));
	dbgWriteReg = IspInfo.DebugMask & ISP_DBG_WRITE_REG;
	spin_unlock(&(IspInfo.SpinLockIsp));

	module = pReg->module;


	switch (module) {
	case ISP_CAM_A_IDX:
		regBase = ISP_CAM_A_BASE;
		break;
	case ISP_CAM_B_IDX:
		regBase = ISP_CAM_B_BASE;
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
	case ISP_DIP_A_IDX:
		regBase = ISP_DIP_A_BASE;
		break;
	case ISP_UNI_A_IDX:
		regBase = ISP_CAM_UNI_BASE;
		break;
	case 0xFF:
		regBase = ISP_SENINF0_BASE;
		break;
	case 0xFE:
		regBase = ISP_SENINF1_BASE;
		break;
	default:
		LOG_NOTICE("Unsupported module(%x) !!!\n", module);
		return -EFAULT;
	}

	/*  */
	if (dbgWriteReg)
		pr_info("- E.\n");

	/*  */
	for (i = 0; i < Count; i++) {
		if (dbgWriteReg)
			pr_info(
			"module(%d), base(0x%lx),Addr(0x%lx), Val(0x%x)\n",
				module, (unsigned long)regBase,
				(unsigned long)(pReg[i].Addr),
				(unsigned int)(pReg[i].Val));

		if (((regBase + pReg[i].Addr) < (regBase + PAGE_SIZE))
			&& ((pReg[i].Addr & 0x3) == 0)) {
			ISP_WR32(regBase + pReg[i].Addr, pReg[i].Val);
		} else {
			pr_notice("wrong address(0x%lx)\n",
				(unsigned long)(regBase + pReg[i].Addr));
		}
	}

	/*  */
	return Ret;
}



/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_WriteReg(struct ISP_REG_IO_STRUCT *pRegIo)
{
	signed int Ret = 0;
	/*    signed int TimeVd = 0;*/
	/*    signed int TimeExpdone = 0;*/
	/*    signed int TimeTasklet = 0;*/
	/* unsigned char* pData = NULL; */
	struct ISP_REG_STRUCT *pData = NULL;

	if ((pRegIo->Count * sizeof(struct ISP_REG_STRUCT)) > 0xFFFFF000) {
		LOG_NOTICE("pRegIo->Count error");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_WRITE_REG)
		pr_info("Data(0x%pK), Count(%d)\n",
			(pRegIo->pData), (pRegIo->Count));

	/* pData = (unsigned char*)kmalloc(
	 *  (pRegIo->Count)*sizeof(struct ISP_REG_STRUCT), GFP_ATOMIC);
	 */
	pData = kmalloc((pRegIo->Count) * sizeof(struct ISP_REG_STRUCT),
			GFP_ATOMIC);
	if (pData == NULL) {
		/* //mark for checkpatch, unnecessart msg
		 * pr_info(
		 *"ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)\n",
		 *	current->comm, current->pid, current->tgid);
		 */
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
 *****************************************************************************/
static int32_t ISP_CheckUseCamWaitQ(enum ISP_IRQ_TYPE_ENUM type,
	enum ISP_ST_ENUM st_type, unsigned int status)
{
	if (type >= ISP_IRQ_TYPE_INT_CAM_A_ST &&
		type <= ISP_IRQ_TYPE_INT_CAM_B_ST) {
		if (st_type == SIGNAL_INT) {
			if (status == SOF_INT_ST ||
				status == SW_PASS1_DON_ST) {
				return 1;
			}
		} else if (st_type == DMA_INT) {
			if (status == AAO_DONE_ST ||
				status == FLKO_DONE_ST ||
				status == AFO_DONE_ST ||
				status == PDO_DONE_ST ||
				status == PSO_DONE_ST) {
				return 1;
			}
		}
	}

	return 0;
}

/******************************************************************************
 *
 *****************************************************************************/
static int32_t ISP_CheckUseCamsvWaitQ(enum ISP_IRQ_TYPE_ENUM type,
	enum ISP_ST_ENUM st_type, unsigned int status)
{
	if (type >= ISP_IRQ_TYPE_INT_CAMSV_0_ST &&
		type <= ISP_IRQ_TYPE_INT_CAMSV_5_ST) {
		if (st_type == SIGNAL_INT) {
			if (status == SV_SOF_INT_ST ||
				status == SV_SW_PASS1_DON_ST)
				return 1;
		}
	}

	return 0;
}

/******************************************************************************
 *
 *****************************************************************************/
static int32_t ISP_GetWaitQCamIndex(enum ISP_IRQ_TYPE_ENUM type)
{
	int32_t index = type - ISP_IRQ_TYPE_INT_CAM_A_ST;

	if (index >= CAM_AMOUNT)
		pr_info("waitq cam index out of range:%d", index);

	return index;
}

/******************************************************************************
 *
 *****************************************************************************/
static int32_t ISP_GetWaitQCamsvIndex(enum ISP_IRQ_TYPE_ENUM type)
{
	int32_t index = type - ISP_IRQ_TYPE_INT_CAMSV_0_ST;

	if (index >= CAMSV_AMOUNT)
		pr_info("waitq camsv index out of range:%d", index);

	return index;
}

/******************************************************************************
 *
 *****************************************************************************/
static int32_t ISP_GetWaitQCamIrqIndex(
	enum ISP_ST_ENUM st_type, unsigned int status)
{
	int32_t index = ISP_WAITQ_HEAD_IRQ_AMOUNT;

	if (st_type == SIGNAL_INT) {
		if (status == SOF_INT_ST)
			index = ISP_WAITQ_HEAD_IRQ_SOF;
		else if (status == SW_PASS1_DON_ST)
			index = ISP_WAITQ_HEAD_IRQ_SW_P1_DONE;
	} else if (st_type == DMA_INT) {
		if (status == AAO_DONE_ST)
			index = ISP_WAITQ_HEAD_IRQ_AAO_DONE;
		else if (status == FLKO_DONE_ST)
			index = ISP_WAITQ_HEAD_IRQ_FLKO_DONE;
		else if (status == AFO_DONE_ST)
			index = ISP_WAITQ_HEAD_IRQ_AFO_DONE;
		else if (status == PSO_DONE_ST)
			index = ISP_WAITQ_HEAD_IRQ_PSO_DONE;
		else if (status == PDO_DONE_ST)
			index = ISP_WAITQ_HEAD_IRQ_PDO_DONE;
	}

	if (index == ISP_WAITQ_HEAD_IRQ_AMOUNT)
		pr_info("waitq cam irq index out of range:%d_%d",
				st_type, status);

	return index;
}

/******************************************************************************
 *
 *****************************************************************************/
static int32_t ISP_GetWaitQCamsvIrqIndex(
	enum ISP_ST_ENUM st_type, unsigned int status)
{
	int32_t index = ISP_WAITQ_HEAD_IRQ_SV_AMOUNT;

	if (st_type == SIGNAL_INT) {
		if (status == SV_SOF_INT_ST)
			index = ISP_WAITQ_HEAD_IRQ_SV_SOF;
		else if (status == SV_SW_PASS1_DON_ST)
			index = ISP_WAITQ_HEAD_IRQ_SV_SW_P1_DONE;
	}

	if (index == ISP_WAITQ_HEAD_IRQ_SV_AMOUNT)
		pr_info("waitq camsv irq index out of range:%d_%d",
				st_type, status);

	return index;
}

/******************************************************************************
 *
 *****************************************************************************/
#ifdef AEE_DUMP_BY_USING_ION_MEMORY
static signed int isp_allocbuf(struct isp_imem_memory *pMemInfo)
{
	int ret = 0;
	struct ion_mm_data mm_data;
	struct ion_sys_data sys_data;
	struct ion_handle *handle = NULL;

	if (pMemInfo == NULL) {
		LOG_NOTICE("pMemInfo is NULL!!\n");
		ret = -ENOMEM;
		goto isp_allocbuf_exit;
	}

	if (isp_p2_ion_client == NULL) {
		LOG_NOTICE("isp_p2_ion_client is NULL!!\n");
		ret = -ENOMEM;
		goto isp_allocbuf_exit;
	}

	handle = ion_alloc(isp_p2_ion_client, pMemInfo->length, 0,
				ION_HEAP_MULTIMEDIA_MASK, 0);
	if (handle == NULL) {
		LOG_NOTICE("fail to alloc ion buffer, ret=%d\n", ret);
		ret = -ENOMEM;
		goto isp_allocbuf_exit;
	}
	pMemInfo->handle = (void *) handle;

	pMemInfo->va = (uintptr_t) ion_map_kernel(isp_p2_ion_client, handle);
	if (pMemInfo->va == 0) {
		LOG_NOTICE("fail to map va of buffer!\n");
		ret = -ENOMEM;
		goto isp_allocbuf_exit;
	}

	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.config_buffer_param.module_id = 0;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 1;
	ret = ion_kernel_ioctl(isp_p2_ion_client, ION_CMD_MULTIMEDIA,
				(unsigned long)&mm_data);
	if (ret) {
		LOG_NOTICE("fail to config ion buffer, ret=%d\n", ret);
		ret = -ENOMEM;
		goto isp_allocbuf_exit;
	}

	sys_data.sys_cmd = ION_SYS_GET_PHYS;
	sys_data.get_phys_param.kernel_handle = handle;
	ret = ion_kernel_ioctl(isp_p2_ion_client, ION_CMD_SYSTEM,
				(unsigned long)&sys_data);
	pMemInfo->pa = sys_data.get_phys_param.phy_addr;

isp_allocbuf_exit:

	if (ret < 0) {
		if (handle)
			ion_free(isp_p2_ion_client, handle);
	}

	return ret;
}

/******************************************************************************
 *
 *****************************************************************************/
static void isp_freebuf(struct isp_imem_memory *pMemInfo)
{
	struct ion_handle *handle;

	if (pMemInfo == NULL) {
		LOG_NOTICE("pMemInfo is NULL!!\n");
		return;
	}

	handle = (struct ion_handle *) pMemInfo->handle;
	if (handle != NULL) {
		ion_unmap_kernel(isp_p2_ion_client, handle);
		ion_free(isp_p2_ion_client, handle);
	}

}
#endif

/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_DumpBuffer(struct ISP_DUMP_BUFFER_STRUCT *pDumpBufStruct)
{
	signed int Ret = 0;

	if (pDumpBufStruct->BytesofBufferSize > 0xFFFFFFFF) {
		LOG_NOTICE("pDumpTuningBufStruct->BytesofBufferSize error");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	if ((void __user *)(pDumpBufStruct->pBuffer) == NULL) {
		LOG_NOTICE("NULL pDumpBufStruct->pBuffer");
		Ret = -EFAULT;
		goto EXIT;
	}
    /* Native Exception */
	switch (pDumpBufStruct->DumpCmd) {
	case ISP_DUMP_TPIPEBUF_CMD:
		if (pDumpBufStruct->BytesofBufferSize >
		    MAX_ISP_TILE_TDR_HEX_NO) {
			LOG_NOTICE("tpipe size error");
			Ret = -EFAULT;
			goto EXIT;
		}
#ifdef AEE_DUMP_REDUCE_MEMORY
		if (g_bIonBufferAllocated == MFALSE) {
			if (g_pTpipeBuffer == NULL)
				g_pTpipeBuffer =
					vmalloc(MAX_ISP_TILE_TDR_HEX_NO);
			else
				LOG_NOTICE("g_pTpipeBuffer:0x%pK is not NULL!!",
					g_pTpipeBuffer);
		}
		if (g_pTpipeBuffer != NULL) {
			if (copy_from_user(g_pTpipeBuffer,
			    (void __user *)(pDumpBufStruct->pBuffer),
			    pDumpBufStruct->BytesofBufferSize) != 0) {
				LOG_NOTICE(
				"copy_from_user g_pTpipeBuffer failed\n");
				Ret = -EFAULT;
				goto EXIT;
			}
		} else {
			LOG_NOTICE(
			"g_pTpipeBuffer kmalloc failed, g_bIonBufAllocated:%d\n"
				, g_bIonBufferAllocated);
		}
#else
		if (copy_from_user(g_TpipeBuffer,
		    (void __user *)(pDumpBufStruct->pBuffer),
		    pDumpBufStruct->BytesofBufferSize) != 0) {
			LOG_NOTICE("copy_from_user g_TpipeBuffer failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}
#endif
		pr_info("copy dumpbuf::0x%p tpipebuf:0x%p is done!!\n",
			pDumpBufStruct->pBuffer, g_pTpipeBuffer);
		DumpBufferField = DumpBufferField | 0x1;
		break;
	case ISP_DUMP_TUNINGBUF_CMD:
		if (pDumpBufStruct->BytesofBufferSize > ISP_DIP_REG_SIZE) {
			LOG_NOTICE("tuning buf size error, size:0x%x",
				pDumpBufStruct->BytesofBufferSize);
			Ret = -EFAULT;
			goto EXIT;
		}
#ifdef AEE_DUMP_REDUCE_MEMORY
		if (g_bIonBufferAllocated == MFALSE) {
			if (g_pTuningBuffer == NULL)
				g_pTuningBuffer = vmalloc(ISP_DIP_REG_SIZE);
			else
				LOG_NOTICE("g_TuningBuffer:0x%pK is not NULL!!",
					g_pTuningBuffer);
		}
		if (g_pTuningBuffer != NULL) {
			if (copy_from_user(g_pTuningBuffer,
			    (void __user *)(pDumpBufStruct->pBuffer),
			    pDumpBufStruct->BytesofBufferSize) != 0) {
				LOG_NOTICE(
					"copy_from_user g_pTuningBuffer failed\n");
				Ret = -EFAULT;
				goto EXIT;
			}
		} else {
			LOG_NOTICE("ERROR: g_TuningBuffer kmalloc failed\n");
		}
#else
		if (copy_from_user(g_TuningBuffer,
		    (void __user *)(pDumpBufStruct->pBuffer),
		    pDumpBufStruct->BytesofBufferSize) != 0) {
			LOG_NOTICE("copy_from_user g_TuningBuffer failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}
#endif
		pr_info("copy dumpbuf::0x%p tuningbuf:0x%p is done!!\n",
			pDumpBufStruct->pBuffer, g_pTuningBuffer);
		DumpBufferField = DumpBufferField | 0x2;
		break;
	case ISP_DUMP_ISPVIRBUF_CMD:
		if (pDumpBufStruct->BytesofBufferSize > ISP_DIP_REG_SIZE) {
			LOG_NOTICE("vir isp buffer size error, size:0x%x",
				pDumpBufStruct->BytesofBufferSize);
			Ret = -EFAULT;
			goto EXIT;
		}
#ifdef AEE_DUMP_REDUCE_MEMORY
		if (g_bIonBufferAllocated == MFALSE) {
			if (g_pVirISPBuffer == NULL)
				g_pVirISPBuffer = vmalloc(ISP_DIP_REG_SIZE);
			else
				LOG_NOTICE(
					"g_pVirISPBuffer:0x%pK is not NULL!!",
					g_pVirISPBuffer);
		}
		if (g_pVirISPBuffer != NULL) {
			if (copy_from_user(g_pVirISPBuffer,
			    (void __user *)(pDumpBufStruct->pBuffer),
			    pDumpBufStruct->BytesofBufferSize) != 0) {
				LOG_NOTICE(
					"copy_from_user g_pVirISPBuffer failed\n");
				Ret = -EFAULT;
				goto EXIT;
			}
		} else {
			LOG_NOTICE("ERROR: g_pVirISPBuffer kmalloc failed\n");
		}
#else
		if (copy_from_user(g_VirISPBuffer,
		    (void __user *)(pDumpBufStruct->pBuffer),
		    pDumpBufStruct->BytesofBufferSize) != 0) {
			LOG_NOTICE("copy_from_user g_VirISPBuffer failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}
#endif
		pr_info("copy dumpbuf::0x%p virispbuf:0x%p is done!!\n",
			pDumpBufStruct->pBuffer, g_pVirISPBuffer);
		DumpBufferField = DumpBufferField | 0x4;
		break;
	case ISP_DUMP_CMDQVIRBUF_CMD:
		if (pDumpBufStruct->BytesofBufferSize >
		    MAX_ISP_CMDQ_BUFFER_SIZE) {
			LOG_NOTICE("cmdq buffer size error, size:0x%x",
				pDumpBufStruct->BytesofBufferSize);
			Ret = -EFAULT;
			goto EXIT;
		}
#ifdef AEE_DUMP_REDUCE_MEMORY
		if (g_bIonBufferAllocated == MFALSE) {
			if (g_pCmdqBuffer == NULL)
				g_pCmdqBuffer =
					vmalloc(MAX_ISP_CMDQ_BUFFER_SIZE);
			else
				LOG_NOTICE("g_pCmdqBuffer:0x%pK is not NULL!!",
					g_pCmdqBuffer);
		}
		if (g_pCmdqBuffer != NULL) {
			if (copy_from_user(g_pCmdqBuffer,
			    (void __user *)(pDumpBufStruct->pBuffer),
			    pDumpBufStruct->BytesofBufferSize) != 0) {
				LOG_NOTICE(
					"copy_from_user g_pCmdqBuffer failed\n");
				Ret = -EFAULT;
				goto EXIT;
			}
		} else {
			LOG_NOTICE("ERROR: g_pCmdqBuffer kmalloc failed\n");
		}
#else
		if (copy_from_user(g_CmdqBuffer,
		    (void __user *)(pDumpBufStruct->pBuffer),
		    pDumpBufStruct->BytesofBufferSize) != 0) {
			LOG_NOTICE("copy_from_user g_VirISPBuffer failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}
#endif
		pr_info("copy dumpbuf::0x%p cmdqbuf:0x%p is done!!\n",
			pDumpBufStruct->pBuffer, g_pCmdqBuffer);
		DumpBufferField = DumpBufferField | 0x8;
		break;
	default:
		LOG_NOTICE("error dump buffer cmd:%d", pDumpBufStruct->DumpCmd);
		break;
	}
	if (g_bUserBufIsReady == MFALSE) {
		if ((DumpBufferField & 0xf) == 0xf) {
			g_bUserBufIsReady = MTRUE;
			DumpBufferField = 0;
			pr_info(
			"DumpBufferField:0x%x, g_bUserBufIsReady:%d!!\n",
				DumpBufferField, g_bUserBufIsReady);
		}
	}
	/*  */
EXIT:

	return Ret;
}

/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_SetMemInfo(struct ISP_MEM_INFO_STRUCT *pMemInfoStruct)
{
	signed int Ret = 0;
	/*  */
	if ((void __user *)(pMemInfoStruct->MemVa) == NULL) {
		LOG_NOTICE("NULL pMemInfoStruct->MemVa");
		Ret = -EFAULT;
		goto EXIT;
	}
	switch (pMemInfoStruct->MemInfoCmd) {
	case ISP_MEMORY_INFO_TPIPE_CMD:
		memcpy(&g_TpipeBaseAddrInfo, pMemInfoStruct,
			sizeof(struct ISP_MEM_INFO_STRUCT));
		pr_info("set tpipe memory info is done!!\n");
		break;
	case ISP_MEMORY_INFO_CMDQ_CMD:
		memcpy(&g_CmdqBaseAddrInfo, pMemInfoStruct,
			sizeof(struct ISP_MEM_INFO_STRUCT));
		pr_info("set comq memory info is done!!\n");
		break;
	default:
		LOG_NOTICE("error set memory info cmd:%d",
			pMemInfoStruct->MemInfoCmd);
		break;
	}
	/*  */
EXIT:

	return Ret;
}

/******************************************************************************
 *
 *****************************************************************************/
static atomic_t g_imem_ref_cnt[ISP_REF_CNT_ID_MAX];
/*  */
/* static long ISP_REF_CNT_CTRL_FUNC(unsigned int Param) */
static long ISP_REF_CNT_CTRL_FUNC(unsigned long Param)
{
	signed int Ret = 0;
	struct ISP_REF_CNT_CTRL_STRUCT ref_cnt_ctrl;
	signed int imem_ref_cnt = 0;

	/* pr_info("[rc]+ QQ"); */ /* for memory corruption check */


	/* //////////////////---add lock here */
	/* spin_lock_irq(&(IspInfo.SpinLock)); */
	/* ////////////////// */
	/*  */
	if (IspInfo.DebugMask & ISP_DBG_REF_CNT_CTRL)
		pr_info("[rc]+");

	/*  */
	if (NULL == (void __user *)Param)  {
		LOG_NOTICE("[rc]NULL Param");
		/* //////////////////---add unlock here */
		/* spin_unlock_irqrestore(&(IspInfo.SpinLock), flags); */
		/* ////////////////// */
		return -EFAULT;
	}
	/*  */
	if (copy_from_user(&ref_cnt_ctrl, (void __user *)Param,
	    sizeof(struct ISP_REF_CNT_CTRL_STRUCT)) == 0) {


		if (IspInfo.DebugMask & ISP_DBG_REF_CNT_CTRL)
			pr_info("[rc]ctrl(%d),id(%d)",
				ref_cnt_ctrl.ctrl, ref_cnt_ctrl.id);

		/*  */
		if ((ref_cnt_ctrl.id < ISP_REF_CNT_ID_MAX) &&
		    (ref_cnt_ctrl.id >= 0)) {
			/* //////////////////---add lock here */
			spin_lock(&(IspInfo.SpinLockIspRef));
			/* ////////////////// */
			/*  */
			switch (ref_cnt_ctrl.ctrl) {
			case ISP_REF_CNT_GET:
				break;
			case ISP_REF_CNT_INC:
				atomic_inc(&g_imem_ref_cnt[ref_cnt_ctrl.id]);
				/* g_imem_ref_cnt++; */
				break;
			case ISP_REF_CNT_DEC:
			case ISP_REF_CNT_DEC_AND_RESET_P1_P2_IF_LAST_ONE:
			case ISP_REF_CNT_DEC_AND_RESET_P1_IF_LAST_ONE:
			case ISP_REF_CNT_DEC_AND_RESET_P2_IF_LAST_ONE:
				atomic_dec(&g_imem_ref_cnt[ref_cnt_ctrl.id]);
				/* g_imem_ref_cnt--; */
				break;
			default:
			case ISP_REF_CNT_MAX:/* Add to aviod build warning */
				/* Do nothing. */
				break;
			}
			/*  */
			imem_ref_cnt = (signed int)atomic_read(
				&g_imem_ref_cnt[ref_cnt_ctrl.id]);

			if (imem_ref_cnt == 0) {
				/* No user left and ctrl is RESET_IF_LAST_ONE,
				 * do ISP reset.
				 */
				if (ref_cnt_ctrl.ctrl ==
				  ISP_REF_CNT_DEC_AND_RESET_P1_P2_IF_LAST_ONE ||
				ref_cnt_ctrl.ctrl ==
				  ISP_REF_CNT_DEC_AND_RESET_P1_IF_LAST_ONE) {
					ISP_Reset(ISP_REG_SW_CTL_RST_CAM_P1);
					pr_info("Reset P1\n");
				}

				if (ref_cnt_ctrl.ctrl ==
				  ISP_REF_CNT_DEC_AND_RESET_P1_P2_IF_LAST_ONE ||
				ref_cnt_ctrl.ctrl ==
				  ISP_REF_CNT_DEC_AND_RESET_P2_IF_LAST_ONE)
					ISP_Reset(ISP_REG_SW_CTL_RST_CAM_P2);

			}
			/* //////////////////---add unlock here */
			spin_unlock(&(IspInfo.SpinLockIspRef));
			/* ////////////////// */

			/*  */
			if (IspInfo.DebugMask & ISP_DBG_REF_CNT_CTRL)
				pr_info("[rc]ref_cnt(%d)", imem_ref_cnt);

			/*  */
			if ((void __user *)(ref_cnt_ctrl.data_ptr) == NULL) {
				LOG_NOTICE("NULL data_ptr");
				return -EFAULT;
			}
			if (put_user(imem_ref_cnt,
			    (signed int *)ref_cnt_ctrl.data_ptr) != 0) {
				LOG_NOTICE("[rc][GET]:copy_to_user failed");
				Ret = -EFAULT;
			}
		} else {
			LOG_NOTICE("[rc]:id(%d) exceed", ref_cnt_ctrl.id);
			Ret = -EFAULT;
		}


	} else {
		LOG_NOTICE("[rc]copy_from_user failed");
		Ret = -EFAULT;
	}

	/*  */
	if (IspInfo.DebugMask & ISP_DBG_REF_CNT_CTRL)
		pr_info("[rc]-");

	/* pr_info("[rc]QQ return value:(%d)", Ret); */
	/*  */
	/* //////////////////---add unlock here */
	/* spin_unlock_irqrestore(&(IspInfo.SpinLock), flags); */
	/* ////////////////// */
	return Ret;
}
/******************************************************************************
 *
 *****************************************************************************/

/*  */
/* isr dbg log , sw isr response counter , +1 when sw receive 1 sof isr. */
static unsigned int sof_count[ISP_IRQ_TYPE_AMOUNT] = {0};
static int Vsync_cnt[2] = {0, 0};

/* keep current frame status */
static enum CAM_FrameST FrameStatus[ISP_IRQ_TYPE_AMOUNT] = {0};

/* current invoked time is at 1st sof or not during each streaming,
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
#define _MAX_FRM_CNT_ 0xFF

#define _UNCERTAIN_MAGIC_NUM_FLAG_ 0x40000000
#define _DUMMY_MAGIC_              0x20000000
static unsigned int m_LastMNum[_cam_max_] = {0}; /* imgo/rrzo */

#endif
/* static long ISP_Buf_CTRL_FUNC(unsigned int Param) */
static long ISP_Buf_CTRL_FUNC(unsigned long Param)
{
	signed int Ret = 0;
	enum _isp_dma_enum_ rt_dma;
	unsigned int i = 0;
	/*    unsigned int x = 0;*/
	/*    unsigned int iBuf = 0;*/
	/*    unsigned int size = 0;*/
	/*    unsigned int bWaitBufRdy = 0;*/
	struct ISP_BUFFER_CTRL_STRUCT         rt_buf_ctrl;
	/*    unsigned int flags;*/
	/*    struct ISP_RT_BUF_INFO_STRUCT       rt_buf_info;*/
	/*    struct ISP_DEQUE_BUF_INFO_STRUCT    deque_buf;*/
	/*    enum ISP_IRQ_TYPE_ENUM irqT = ISP_IRQ_TYPE_AMOUNT;*/
	/*    enum ISP_IRQ_TYPE_ENUM irqT_Lock = ISP_IRQ_TYPE_AMOUNT;*/
	/*    bool CurVF_En = MFALSE;*/
	/*  */
	if ((void __user *)Param == NULL)  {
		LOG_NOTICE("[rtbc]NULL Param");
		return -EFAULT;
	}
	/*  */
	if (copy_from_user(&rt_buf_ctrl, (void __user *)Param,
	    sizeof(struct ISP_BUFFER_CTRL_STRUCT)) == 0) {
		if ((rt_buf_ctrl.module >= ISP_IRQ_TYPE_AMOUNT) ||
		    (rt_buf_ctrl.module < 0)) {
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
		if ((rt_dma >= _cam_max_) ||
		    (rt_dma < 0)) {
			LOG_NOTICE("[rtbc]buf_id error:0x%x\n", rt_dma);
			return -EFAULT;
		}

		/*  */
		switch (rt_buf_ctrl.ctrl) {
		case ISP_RT_BUF_CTRL_CLEAR:
			/*  */
			if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL)
				pr_info("[rtbc][%d][CLEAR]:rt_dma(%d)\n",
					rt_buf_ctrl.module, rt_dma);
			/*  */

			memset((void *)IspInfo.IrqInfo.LastestSigTime_usec
				[rt_buf_ctrl.module],
				0, sizeof(unsigned int) * 32);
			memset((void *)IspInfo.IrqInfo.LastestSigTime_sec
				[rt_buf_ctrl.module],
				0, sizeof(unsigned int) * 32);
			/* remove, cause clear will be involked only when
			 * current module r totally stopped
			 */
			/* spin_lock_irqsave(
			 * &(IspInfo.SpinLockIrq[irqT_Lock]), flags);
			 */

			/* reset active record*/
			pstRTBuf[rt_buf_ctrl.module]->ring_buf[rt_dma].active =
			MFALSE;
			memset((char *)
				&pstRTBuf[rt_buf_ctrl.module]->ring_buf[rt_dma],
				0x00,
				sizeof(struct ISP_RT_RING_BUF_INFO_STRUCT));
/* init. frmcnt before vf_en */
for (i = 0; i < ISP_RT_BUF_SIZE; i++)
	pstRTBuf[rt_buf_ctrl.module]->ring_buf[rt_dma].data[i].image.frm_cnt
	=	_INVALID_FRM_CNT_;

			switch (rt_buf_ctrl.module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
		if ((pstRTBuf[rt_buf_ctrl.module]->ring_buf[_imgo_].active
		== MFALSE) &&
		(pstRTBuf[rt_buf_ctrl.module]->ring_buf[_rrzo_].active
		== MFALSE)) {
			sof_count[rt_buf_ctrl.module] = 0;
			g1stSof[rt_buf_ctrl.module] = MTRUE;
			#if (TSTMP_SUBSAMPLE_INTPL == 1)
			g1stSwP1Done[rt_buf_ctrl.module] =
				MTRUE;
			gPrevSofTimestp[rt_buf_ctrl.module] = 0;
			#endif
			g_ISPIntErr[rt_buf_ctrl.module] = 0;
			g_ISPIntErr_SMI[rt_buf_ctrl.module] = 0;
			pstRTBuf[rt_buf_ctrl.module]->dropCnt =
				0;
			pstRTBuf[rt_buf_ctrl.module]->state = 0;
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
		if (
		pstRTBuf[rt_buf_ctrl.module]->ring_buf[_camsv_imgo_].active
		== MFALSE) {
			sof_count[rt_buf_ctrl.module] = 0;
			g1stSof[rt_buf_ctrl.module] = MTRUE;
			g_ISPIntErr[rt_buf_ctrl.module] = 0;
			g_ISPIntErr_SMI[rt_buf_ctrl.module] = 0;
			pstRTBuf[rt_buf_ctrl.module]->dropCnt =
				0;
			pstRTBuf[rt_buf_ctrl.module]->state = 0;
		}

		break;
			case ISP_IRQ_TYPE_INT_DIP_A_ST:
			case ISP_IRQ_TYPE_INT_UNI_A_ST:
				sof_count[rt_buf_ctrl.module] = 0;
				g1stSof[rt_buf_ctrl.module] = MTRUE;
				g_ISPIntErr[rt_buf_ctrl.module] = 0;
				g_ISPIntErr_SMI[rt_buf_ctrl.module] = 0;
				pstRTBuf[rt_buf_ctrl.module]->dropCnt = 0;
				pstRTBuf[rt_buf_ctrl.module]->state = 0;
				break;
			default:
				LOG_NOTICE("unsupported module:0x%x\n",
					rt_buf_ctrl.module);
				break;
			}

#ifdef _MAGIC_NUM_ERR_HANDLING_
			m_LastMNum[rt_dma] = 0;
#endif

			/* spin_unlock_irqrestore(
			 * &(IspInfo.SpinLockIrq[irqT_Lock]), flags);
			 */

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
			pstRTBuf[rt_buf_ctrl.module]->ring_buf[z].active
					= array[z];
			if (IspInfo.DebugMask &
					  ISP_DBG_BUF_CTRL)
				pr_info(
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
		case ISP_RT_BUF_CTRL_MAX:/* Add this to remove build warning */
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
		bw_cal = (G_PM_QOS[module].bw_sum * G_PM_QOS[module].fps)
			/ 1000000; /* MByte/s */
		break;
	}
	default:
		LOG_NOTICE("unsupport cmd:%d", cmd);
		Ret = -1;
		break;
	}
	mtk_pm_qos_update_request(&camsys_qos_request[module], bw_cal);

	if (PMQoS_BW_value != bw_cal) {
		pr_info(
			"PM_QoS: module[%d], cmd[%d], bw[%d], fps[%d], total bw = %d MB/s\n",
			module, cmd,
			G_PM_QOS[module].bw_sum, G_PM_QOS[module].fps, bw_cal);
		PMQoS_BW_value = bw_cal;
	}

	return Ret;
}
#endif

/******************************************************************************
 * update current idnex to working frame
 *****************************************************************************/
static signed int ISP_P2_BufQue_Update_ListCIdx(
	enum ISP_P2_BUFQUE_PROPERTY property,
	enum ISP_P2_BUFQUE_LIST_TAG listTag)
{
	signed int ret = 0;
	signed int tmpIdx = 0;
	signed int cnt = 0;
	bool stop = false;
	int i = 0;
	enum ISP_P2_BUF_STATE_ENUM cIdxSts = ISP_P2_BUF_STATE_NONE;

	switch (listTag) {
	case ISP_P2_BUFQUE_LIST_TAG_UNIT:
		/* [1] check global pointer current sts */
		//0831-s
		if ((property < 0) || (P2_FrameUnit_List_Idx[property].curr < 0)) {
			LOG_NOTICE("property(%d) || curr < 0", property);
			return -EFAULT;
		}
		///0831-e
		cIdxSts = P2_FrameUnit_List[property]
				[P2_FrameUnit_List_Idx[property].curr].bufSts;
		/* ///////////////////////////////////////////////////////// */
		/* Assume we have the buffer list in the following situation */
		/* ++++++         ++++++         ++++++ */
		/* +  vss +         +  prv +         +  prv + */
		/* ++++++         ++++++         ++++++ */
		/* not deque         erased           enqued */
		/* done */
		/*  */
		/* if the vss deque is done, we should update the CurBufIdx */
		/* to the next "enqued" buffer node instead of just moving */
		/* to the next buffer node */
		/* ///////////////////////////////////////////////////////// */
		/* [2]traverse count needed */
		if (P2_FrameUnit_List_Idx[property].start <=
		P2_FrameUnit_List_Idx[property].end) {
			cnt = P2_FrameUnit_List_Idx[property].end -
				P2_FrameUnit_List_Idx[property].start;
		} else {
			cnt = _MAX_SUPPORT_P2_FRAME_NUM_ -
				P2_FrameUnit_List_Idx[property].start;
			cnt += P2_FrameUnit_List_Idx[property].end;
		}

		/* [3] update current index for frame unit list */
		tmpIdx = P2_FrameUnit_List_Idx[property].curr;
		switch (cIdxSts) {
		case ISP_P2_BUF_STATE_ENQUE:
			P2_FrameUnit_List[property]
				[P2_FrameUnit_List_Idx[property].curr].bufSts =
				ISP_P2_BUF_STATE_RUNNING;
			break;
		case ISP_P2_BUF_STATE_DEQUE_SUCCESS:
		do { /* to find the newest cur index */
			tmpIdx = (tmpIdx + 1) %
				_MAX_SUPPORT_P2_FRAME_NUM_;
			switch (
			P2_FrameUnit_List[property][tmpIdx].bufSts) {
			case ISP_P2_BUF_STATE_ENQUE:
			case ISP_P2_BUF_STATE_RUNNING:
				P2_FrameUnit_List[property][tmpIdx].bufSts
				  = ISP_P2_BUF_STATE_RUNNING;
				P2_FrameUnit_List_Idx[property].curr =
					tmpIdx;
				stop = true;
				break;
			case ISP_P2_BUF_STATE_WAIT_DEQUE_FAIL:
			case ISP_P2_BUF_STATE_DEQUE_SUCCESS:
			case ISP_P2_BUF_STATE_DEQUE_FAIL:
			case ISP_P2_BUF_STATE_NONE:
			default:
				break;
			}
			i++;
		} while ((i < cnt) && (!stop));
			/* ///////////////////////////////////////////////// */
			/* Assume we have the buffer list in the */
			/* following situation */
			/* ++++++         ++++++         ++++++ */
			/* +  vss +         +  prv +         +  prv + */
			/* ++++++         ++++++         ++++++ */
			/* not deque         erased           erased */
			/* done */
			/*  */
			/* all the buffer node are deque done in the */
			/* current moment, should update*/
			/* current index to the last node */
			/* if the vss deque is done, we should update the */
			/* CurBufIdx to the last buffer node */
			/* ///////////////////////////////////////////////// */
			if ((!stop) && (i == (cnt)))
				P2_FrameUnit_List_Idx[property].curr =
					P2_FrameUnit_List_Idx[property].end;

			break;
		case ISP_P2_BUF_STATE_WAIT_DEQUE_FAIL:
		case ISP_P2_BUF_STATE_DEQUE_FAIL:
			/* QQ. ADD ASSERT */
			break;
		case ISP_P2_BUF_STATE_NONE:
		case ISP_P2_BUF_STATE_RUNNING:
		default:
			break;
		}
		break;
	case ISP_P2_BUFQUE_LIST_TAG_PACKAGE:
	default:
		LOG_NOTICE("Wrong List tag(%d)\n", listTag);
		break;
	}
	return ret;
}
/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_P2_BufQue_Erase(enum ISP_P2_BUFQUE_PROPERTY property,
enum ISP_P2_BUFQUE_LIST_TAG listTag, signed int idx)
{
	signed int ret =  -1;
	bool stop = false;
	int i = 0;
	signed int cnt = 0;
	int tmpIdx = 0;

	//0831-s
	if (property < ISP_P2_BUFQUE_PROPERTY_DIP) {
		LOG_NOTICE("property < ISP_P2_BUFQUE_PROPERTY_DIP");
		return ret;
	}
	//0831-e
	switch (listTag) {
	case ISP_P2_BUFQUE_LIST_TAG_PACKAGE:
		tmpIdx = P2_FramePack_List_Idx[property].start;
		//0831-s
		if ((tmpIdx < 0) || (idx < 0)) {
			LOG_NOTICE("LIST_TAG_PACKAGE:tmpIdx(%d) or idx(%d) < 0\n", tmpIdx, idx);
			return ret;
		}
		//0831-e
		/* [1] clear buffer status */
		P2_FramePackage_List[property][idx].processID = 0x0;
		P2_FramePackage_List[property][idx].callerID = 0x0;
		P2_FramePackage_List[property][idx].dupCQIdx =  -1;
		P2_FramePackage_List[property][idx].frameNum = 0;
		P2_FramePackage_List[property][idx].dequedNum = 0;
		/* [2] update first index */
		if (P2_FramePackage_List[property][tmpIdx].dupCQIdx == -1) {
			/* traverse count needed, cuz user may erase the */
			/* element but not the one at first idx */
			/* (pip or vss scenario) */
			if (P2_FramePack_List_Idx[property].start <=
			P2_FramePack_List_Idx[property].end) {
				cnt = P2_FramePack_List_Idx[property].end -
					P2_FramePack_List_Idx[property].start;
			} else {
				cnt = _MAX_SUPPORT_P2_PACKAGE_NUM_ -
					P2_FramePack_List_Idx[property].start;
				cnt += P2_FramePack_List_Idx[property].end;
			}
		do { /* to find the newest first lindex */
			tmpIdx = (tmpIdx + 1) %
				_MAX_SUPPORT_P2_PACKAGE_NUM_;
			switch (
			P2_FramePackage_List[property][tmpIdx].dupCQIdx){
			case (-1):
				break;
			default:
				stop = true;
				P2_FramePack_List_Idx[property].start =
					tmpIdx;
				break;
			}
			i++;
		} while ((i < cnt) && (!stop));
			/* current last erased element in list is the one */
			/* firstBufindex point at and all the buffer node */
			/* are deque done in the current moment, should */
			/* update first index to the last node */
			if ((!stop) && (i == cnt))
				P2_FramePack_List_Idx[property].start =
					P2_FramePack_List_Idx[property].end;

		}
		break;
	case ISP_P2_BUFQUE_LIST_TAG_UNIT:
		tmpIdx = P2_FrameUnit_List_Idx[property].start;
		//0831-s
		if ((tmpIdx < 0) || (idx < 0)) {
			LOG_NOTICE("LIST_TAG_UNIT:tmpIdx(%d) or idx(%d) < 0\n", tmpIdx, idx);
			return ret;
		}
		//0831-e
		/* [1] clear buffer status */
		P2_FrameUnit_List[property][idx].processID = 0x0;
		P2_FrameUnit_List[property][idx].callerID = 0x0;
		P2_FrameUnit_List[property][idx].cqMask =  0x0;
		P2_FrameUnit_List[property][idx].bufSts = ISP_P2_BUF_STATE_NONE;
		/* [2]update first index */
		if (P2_FrameUnit_List[property][tmpIdx].bufSts ==
		ISP_P2_BUF_STATE_NONE) {
			/* traverse count needed, cuz user may erase the */
			/* element but not the one at first idx */
			if (P2_FrameUnit_List_Idx[property].start <=
			P2_FrameUnit_List_Idx[property].end) {
				cnt = P2_FrameUnit_List_Idx[property].end -
					P2_FrameUnit_List_Idx[property].start;
			} else {
				cnt = _MAX_SUPPORT_P2_FRAME_NUM_ -
					P2_FrameUnit_List_Idx[property].start;
				cnt += P2_FrameUnit_List_Idx[property].end;
			}
			/* to find the newest first lindex */
			do {
				tmpIdx = (tmpIdx + 1) %
					_MAX_SUPPORT_P2_FRAME_NUM_;
				switch (
				P2_FrameUnit_List[property][tmpIdx].bufSts) {
				case ISP_P2_BUF_STATE_ENQUE:
				case ISP_P2_BUF_STATE_RUNNING:
				case ISP_P2_BUF_STATE_DEQUE_SUCCESS:
					stop = true;
					P2_FrameUnit_List_Idx[property].start =
						tmpIdx;
					break;
				case ISP_P2_BUF_STATE_WAIT_DEQUE_FAIL:
				case ISP_P2_BUF_STATE_DEQUE_FAIL:
					/* ASSERT */
					break;
				case ISP_P2_BUF_STATE_NONE:
				default:
					break;
				}
				i++;
			} while ((i < cnt) && (!stop));
			/* current last erased element in list is the one */
			/* firstBufindex point at and all the buffer node are */
			/* deque done in the current moment, should */
			/* update first index to the last node */
			if ((!stop) && (i == (cnt)))
				P2_FrameUnit_List_Idx[property].start =
					P2_FrameUnit_List_Idx[property].end;

		}
		break;
	default:
		break;
	}
	return ret;
}

/******************************************************************************
 * get first matched element idnex
 *****************************************************************************/
static signed int ISP_P2_BufQue_GetMatchIdx(struct ISP_P2_BUFQUE_STRUCT param,
		enum ISP_P2_BUFQUE_MATCH_TYPE matchType,
		enum ISP_P2_BUFQUE_LIST_TAG listTag)
{
	int idx = -1;
	int i = 0;
	//0831-s
	enum ISP_P2_BUFQUE_PROPERTY property;
	//0831-e

	if (param.property >= ISP_P2_BUFQUE_PROPERTY_NUM) {
		LOG_NOTICE("property err(%d)\n", param.property);
		return idx;
	}
	property = param.property;

	switch (matchType) {
	case ISP_P2_BUFQUE_MATCH_TYPE_WAITDQ:
		/* traverse for finding the frame unit which had not beed */
		/* dequeued of the process */
	if (P2_FrameUnit_List_Idx[property].start <=
	P2_FrameUnit_List_Idx[property].end) {
		for (i = P2_FrameUnit_List_Idx[property].start;
		i <= P2_FrameUnit_List_Idx[property].end; i++) {
			if ((P2_FrameUnit_List[property][i].processID ==
			    param.processID) &&
			((P2_FrameUnit_List[property][i].bufSts ==
			    ISP_P2_BUF_STATE_ENQUE) ||
			(P2_FrameUnit_List[property][i].bufSts ==
			    ISP_P2_BUF_STATE_RUNNING))) {
				idx = i;
				break;
			}
		}
	} else {
		for (i = P2_FrameUnit_List_Idx[property].start;
		i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
			if ((P2_FrameUnit_List[property][i].processID ==
			    param.processID) &&
			((P2_FrameUnit_List[property][i].bufSts ==
			    ISP_P2_BUF_STATE_ENQUE) ||
			(P2_FrameUnit_List[property][i].bufSts ==
			    ISP_P2_BUF_STATE_RUNNING))) {
				idx = i;
				break;
			}
		}
		if (idx !=  -1) {
			/*get in the first for loop*/
		} else {
			for (i = 0;
			i <= P2_FrameUnit_List_Idx[property].end; i++) {
				if ((P2_FrameUnit_List[property][i].processID
				== param.processID) &&
				((P2_FrameUnit_List[property][i].bufSts
				== ISP_P2_BUF_STATE_ENQUE) ||
				(P2_FrameUnit_List[property][i].bufSts
				== ISP_P2_BUF_STATE_RUNNING))) {
					idx = i;
					break;
				}
			}
		}
	}
	break;
	case ISP_P2_BUFQUE_MATCH_TYPE_WAITFM:
	if (P2_FramePack_List_Idx[property].start <=
	P2_FramePack_List_Idx[property].end) {
		for (i = P2_FramePack_List_Idx[property].start;
		i <= P2_FramePack_List_Idx[property].end; i++) {
			if ((P2_FramePackage_List[property][i].processID
			== param.processID) &&
			(P2_FramePackage_List[property][i].callerID ==
			param.callerID)) {
				idx = i;
				break;
			}
		}
	} else {
		for (i = P2_FramePack_List_Idx[property].start;
		i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
			if ((P2_FramePackage_List[property][i].processID
			== param.processID) &&
			(P2_FramePackage_List[property][i].callerID ==
			param.callerID)) {
				idx = i;
				break;
			}
		}
		if (idx !=  -1) {
			/*get in the first for loop*/
		} else {
			for (i = 0;
			i <= P2_FramePack_List_Idx[property].end; i++) {
				if ((P2_FramePackage_List[property][i].processID
				== param.processID) &&
				(P2_FramePackage_List[property][i].callerID
				== param.callerID)) {
					idx = i;
					break;
				}
			}
		}
	}
	break;
	case ISP_P2_BUFQUE_MATCH_TYPE_FRAMEOP:
		/* deque done notify */
	if (listTag == ISP_P2_BUFQUE_LIST_TAG_PACKAGE) {
		if (P2_FramePack_List_Idx[property].start <=
		P2_FramePack_List_Idx[property].end) {
			/* avoid race that dupCQ_1 of buffer2 enqued */
			/* while dupCQ_1 of buffer1 have beend deque */
			/* done but not been erased yet */
			for (i = P2_FramePack_List_Idx[property].start;
			i <= P2_FramePack_List_Idx[property].end; i++) {
				if ((P2_FramePackage_List[property][i].processID
				== param.processID) &&
				(P2_FramePackage_List[property][i].callerID
				== param.callerID) &&
				(P2_FramePackage_List[property][i].dupCQIdx
				== param.dupCQIdx) &&
				(P2_FramePackage_List[property][i].dequedNum <
				P2_FramePackage_List[property][i].frameNum)) {
					idx = i;
					break;
				}
			}
		} else {
			for (i = P2_FramePack_List_Idx[property].start;
			i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
				if ((P2_FramePackage_List[property][i].processID
				== param.processID) &&
				(P2_FramePackage_List[property][i].callerID
				== param.callerID) &&
				(P2_FramePackage_List[property][i].dupCQIdx
				== param.dupCQIdx) &&
				(P2_FramePackage_List[property][i].dequedNum <
				P2_FramePackage_List[property][i].frameNum)) {
					idx = i;
					break;
				}
			}
			if (idx !=  -1) {
				/*get in the first for loop*/
				break;
			}
			for (i = 0;
			i <= P2_FramePack_List_Idx[property].end; i++) {
				if (
				(P2_FramePackage_List[property][i].processID
				== param.processID) &&
				(P2_FramePackage_List[property][i].callerID
				== param.callerID) &&
				(P2_FramePackage_List[property][i].dupCQIdx
				== param.dupCQIdx) &&
				(P2_FramePackage_List[property][i].dequedNum <
				P2_FramePackage_List[property][i].frameNum)) {
					idx = i;
					break;
				}
			}
		}
	} else {
		if (P2_FrameUnit_List_Idx[property].start <=
			  P2_FrameUnit_List_Idx[property].end) {
			for (i = P2_FrameUnit_List_Idx[property].start;
			i <= P2_FrameUnit_List_Idx[property].end; i++) {
				if ((P2_FrameUnit_List[property][i].processID
				== param.processID) &&
				(P2_FrameUnit_List[property][i].callerID
				== param.callerID)) {
					idx = i;
					break;
				}
			}
		} else {
			for (i = P2_FrameUnit_List_Idx[property].start;
				   i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
				if ((P2_FrameUnit_List[property][i].processID
				== param.processID) &&
				(P2_FrameUnit_List[property][i].callerID
				== param.callerID)) {
					idx = i;
					break;
				}
			}
if (idx !=  -1) {
	/*get in the first for loop*/
	break;
}
for (i = 0; i <=
P2_FrameUnit_List_Idx[property].end; i++) {
	if ((P2_FrameUnit_List[property][i].processID
	== param.processID) &&
	(P2_FrameUnit_List[property][i].callerID
	== param.callerID)) {
		idx = i;
		break;
	}
}
		}
	}
	break;
	default:
		break;
	}

	return idx;
}

/******************************************************************************
 *
 *****************************************************************************/
static inline unsigned int ISP_P2_BufQue_WaitEventState(
		struct ISP_P2_BUFQUE_STRUCT param,
		enum ISP_P2_BUFQUE_MATCH_TYPE type, signed int *idx)
{
	unsigned int ret = MFALSE;
	signed int index = -1;
	enum ISP_P2_BUFQUE_PROPERTY property;

	if (param.property >= ISP_P2_BUFQUE_PROPERTY_NUM) {
		LOG_NOTICE("property err(%d)\n", param.property);
		return ret;
	}

	//0831-s
	if (idx == NULL) {
		LOG_NOTICE("idx is NULL\n");
		return ret;
	}
	//0831-e
	property = param.property;
	/*  */
	switch (type) {
	case ISP_P2_BUFQUE_MATCH_TYPE_WAITDQ:
		index = *idx;
		//0831-s
		if (index < 0) {
			LOG_NOTICE("*idx err(%d) in WAITDQ\n", *idx);
			return ret;
		}
		//0831-e
		spin_lock(&(SpinLock_P2FrameList));
		if (P2_FrameUnit_List[property][index].bufSts ==
		    ISP_P2_BUF_STATE_RUNNING)
			ret = MTRUE;

		spin_unlock(&(SpinLock_P2FrameList));
		break;
	case ISP_P2_BUFQUE_MATCH_TYPE_WAITFM:
		index = *idx;
		//0831-s
		if (index < 0) {
			LOG_NOTICE("*idx err(%d) in WAITFM\n", *idx);
			return ret;
		}
		//0831-e
		spin_lock(&(SpinLock_P2FrameList));
		if (P2_FramePackage_List[property][index].dequedNum ==
		    P2_FramePackage_List[property][index].frameNum)
			ret = MTRUE;

		spin_unlock(&(SpinLock_P2FrameList));
		break;
	case ISP_P2_BUFQUE_MATCH_TYPE_WAITFMEQD:
		spin_lock(&(SpinLock_P2FrameList));
		/* pr_info("check bf(%d_0x%x_0x%x/%d_%d)", param.property,
		 * param.processID, param.callerID,index, *idx);
		 */
		index = ISP_P2_BufQue_GetMatchIdx(param,
				ISP_P2_BUFQUE_MATCH_TYPE_WAITFM,
				ISP_P2_BUFQUE_LIST_TAG_PACKAGE);
		if (index == -1) {
			/* pr_info("check bf(%d_0x%x_0x%x / %d_%d) ",
			 * param.property,
			 * param.processID, param.callerID,index, *idx);
			 */
			spin_unlock(&(SpinLock_P2FrameList));
			ret = MFALSE;
		} else {
			*idx = index;
			/* pr_info("check bf(%d_0x%x_0x%x / %d_%d) ",
			 * param.property,
			 * param.processID, param.callerID,index, *idx);
			 */
			spin_unlock(&(SpinLock_P2FrameList));
			ret = MTRUE;
		}
		break;
	default:
		break;
	}
	/*  */
	return ret;
}


/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_P2_BufQue_CTRL_FUNC(struct ISP_P2_BUFQUE_STRUCT param)
{
	signed int ret = 0;
	int i = 0, q = 0;
	int idx =  -1, idx2 =  -1;
	signed int restTime = 0;
	//0831-s
	enum ISP_P2_BUFQUE_PROPERTY property;
	//0831-e

	if (param.property >= ISP_P2_BUFQUE_PROPERTY_NUM) {
		LOG_NOTICE("property err(%d)\n", param.property);
		ret = -EFAULT;
		return ret;
	}
	property = param.property;

	switch (param.ctrl) {
	/* signal that a specific buffer is enqueued */
	case ISP_P2_BUFQUE_CTRL_ENQUE_FRAME:
		spin_lock(&(SpinLock_P2FrameList));
		/* (1) check the ring buffer list is full or not */
		if (((P2_FramePack_List_Idx[property].end + 1) %
		_MAX_SUPPORT_P2_PACKAGE_NUM_) ==
		P2_FramePack_List_Idx[property].start &&
		(P2_FramePack_List_Idx[property].end !=  -1)) {
			LOG_NOTICE(
			"pty(%d), F/L(%d_%d,%d), dCQ(%d,%d), RF/C/L(%d,%d,%d), sts(%d,%d,%d)",
				property, param.frameNum,
				P2_FramePack_List_Idx[property].start,
				P2_FramePack_List_Idx[property].end,
				P2_FramePackage_List[property]
					[P2_FramePack_List_Idx[property].start].
					dupCQIdx,
				P2_FramePackage_List[property]
					[P2_FramePack_List_Idx[property].end].
					dupCQIdx,
				P2_FrameUnit_List_Idx[property].start,
				P2_FrameUnit_List_Idx[property].curr,
				P2_FrameUnit_List_Idx[property].end,
				P2_FrameUnit_List[property]
					[P2_FrameUnit_List_Idx[property].start].
					bufSts,
				P2_FrameUnit_List[property]
					[P2_FrameUnit_List_Idx[property].curr].
					bufSts,
				P2_FrameUnit_List[property]
					[P2_FrameUnit_List_Idx[property].end].
					bufSts);
			spin_unlock(&(SpinLock_P2FrameList));
			LOG_NOTICE(
				"p2 frame package list is full, enque Fail.");
			ret =  -EFAULT;
			return ret;
		}
		{
			/*(2) add new to the last of the frame unit list */
			unsigned int cqmask = (param.dupCQIdx << 2) |
						(param.cQIdx << 1) |
						(param.burstQIdx);

			if (P2_FramePack_List_Idx[property].end < 0 ||
			    P2_FrameUnit_List_Idx[property].end < 0) {
#ifdef P2_DBG_LOG
				IRQ_LOG_KEEPER(ISP_IRQ_TYPE_INT_DIP_A_ST, 0,
				_LOG_DBG,
				"pty(%d) pD(0x%x_0x%x)MF/L(%d_%d %d) (%d %d) RF/C/L(%d %d %d) (%d %d %d) cqmsk(0x%x)\n",
				property, param.processID, param.callerID,
				param.frameNum,
				P2_FramePack_List_Idx[property].start,
				P2_FramePack_List_Idx[property].end,
				P2_FramePackage_List[property]
					[P2_FramePack_List_Idx[property].start].
					dupCQIdx,
				P2_FramePackage_List[property][0].dupCQIdx,
				P2_FrameUnit_List_Idx[property].start,
				P2_FrameUnit_List_Idx[property].curr,
				P2_FrameUnit_List_Idx[property].end,
				P2_FrameUnit_List[property]
					[P2_FrameUnit_List_Idx[property].start].
					bufSts,
				P2_FrameUnit_List[property]
					[P2_FrameUnit_List_Idx[property].curr].
					bufSts,
				P2_FrameUnit_List[property][0].bufSts, cqmask);
#endif
			} else {
#ifdef P2_DBG_LOG
				IRQ_LOG_KEEPER(ISP_IRQ_TYPE_INT_DIP_A_ST, 0,
				_LOG_DBG,
				"pty(%d) pD(0x%x_0x%x) MF/L(%d_%d %d)(%d %d) RF/C/L(%d %d %d) (%d %d %d) cqmsk(0x%x)\n",
				property, param.processID, param.callerID,
				param.frameNum,
				P2_FramePack_List_Idx[property].start,
				P2_FramePack_List_Idx[property].end,
				P2_FramePackage_List[property]
					[P2_FramePack_List_Idx[property].start].
					dupCQIdx,
				P2_FramePackage_List[property]
					[P2_FramePack_List_Idx[property].end].
					dupCQIdx,
				P2_FrameUnit_List_Idx[property].start,
				P2_FrameUnit_List_Idx[property].curr,
				P2_FrameUnit_List_Idx[property].end,
				P2_FrameUnit_List[property]
					[P2_FrameUnit_List_Idx[property].start].
					bufSts,
				P2_FrameUnit_List[property]
					[P2_FrameUnit_List_Idx[property].curr].
					bufSts,
				P2_FrameUnit_List[property]
					[P2_FrameUnit_List_Idx[property].end].
					bufSts,
				cqmask);
#endif
			}
			if (P2_FrameUnit_List_Idx[property].start ==
			  P2_FrameUnit_List_Idx[property].end &&
			  P2_FrameUnit_List[property]
			    [P2_FrameUnit_List_Idx[property].start].bufSts ==
			   ISP_P2_BUF_STATE_NONE) {
				/* frame unit list is empty */
				P2_FrameUnit_List_Idx[property].end =
				  (P2_FrameUnit_List_Idx[property].end + 1) %
				  _MAX_SUPPORT_P2_FRAME_NUM_;
				P2_FrameUnit_List_Idx[property].start =
				  P2_FrameUnit_List_Idx[property].end;
				P2_FrameUnit_List_Idx[property].curr =
				  P2_FrameUnit_List_Idx[property].end;
			} else if (P2_FrameUnit_List_Idx[property].curr ==
				P2_FrameUnit_List_Idx[property].end &&
				P2_FrameUnit_List[property]
				[P2_FrameUnit_List_Idx[property].curr].bufSts ==
				ISP_P2_BUF_STATE_NONE) {
				/* frame unit list is not empty, but
				 * current/last is empty. (all the enqueued
				 * frame is done but user have not called
				 * dequeue)
				 */
				P2_FrameUnit_List_Idx[property].end =
				  (P2_FrameUnit_List_Idx[property].end + 1) %
				  _MAX_SUPPORT_P2_FRAME_NUM_;
				P2_FrameUnit_List_Idx[property].curr =
				  P2_FrameUnit_List_Idx[property].end;
			} else {
				P2_FrameUnit_List_Idx[property].end =
				  (P2_FrameUnit_List_Idx[property].end + 1) %
				  _MAX_SUPPORT_P2_FRAME_NUM_;
			}
			P2_FrameUnit_List[property]
				[P2_FrameUnit_List_Idx[property].end].processID
				= param.processID;
			P2_FrameUnit_List[property]
				[P2_FrameUnit_List_Idx[property].end].callerID
				= param.callerID;
			P2_FrameUnit_List[property]
				[P2_FrameUnit_List_Idx[property].end].cqMask
				= cqmask;
			P2_FrameUnit_List[property]
				[P2_FrameUnit_List_Idx[property].end].bufSts
				= ISP_P2_BUF_STATE_ENQUE;

			/* [3] add new frame package in list */
			if (param.burstQIdx == 0) {
				if (P2_FramePack_List_Idx[property].start ==
				P2_FramePack_List_Idx[property].end &&
				P2_FramePackage_List[property]
				  [P2_FramePack_List_Idx[property].start].
				  dupCQIdx == -1) {
					/* all managed buffer node is empty */
					P2_FramePack_List_Idx[property].end =
					  (P2_FramePack_List_Idx[property].end
					  + 1) % _MAX_SUPPORT_P2_PACKAGE_NUM_;
					P2_FramePack_List_Idx[property].start =
					  P2_FramePack_List_Idx[property].end;
				} else {
					P2_FramePack_List_Idx[property].end =
					  (P2_FramePack_List_Idx[property].end
					  + 1) % _MAX_SUPPORT_P2_PACKAGE_NUM_;
				}
				P2_FramePackage_List[property]
					[P2_FramePack_List_Idx[property].end].
					processID = param.processID;
				P2_FramePackage_List[property]
					[P2_FramePack_List_Idx[property].end].
					callerID = param.callerID;
				P2_FramePackage_List[property]
					[P2_FramePack_List_Idx[property].end].
					dupCQIdx = param.dupCQIdx;
				P2_FramePackage_List[property]
					[P2_FramePack_List_Idx[property].end].
					frameNum = param.frameNum;
				P2_FramePackage_List[property]
					[P2_FramePack_List_Idx[property].end].
					dequedNum = 0;
			}
		}
		/* [4]update global index */
		ISP_P2_BufQue_Update_ListCIdx(property,
			ISP_P2_BUFQUE_LIST_TAG_UNIT);
		spin_unlock(&(SpinLock_P2FrameList));
		IRQ_LOG_PRINTER(ISP_IRQ_TYPE_INT_DIP_A_ST, 0, _LOG_DBG);
		/* [5] wake up thread that wait for deque */
		wake_up_interruptible_all(&P2WaitQueueHead_WaitDeque);
		wake_up_interruptible_all(&P2WaitQueueHead_WaitFrameEQDforDQ);
		break;
	/* a dequeue thread is waiting to do dequeue */
	case ISP_P2_BUFQUE_CTRL_WAIT_DEQUE:
		spin_lock(&(SpinLock_P2FrameList));
		idx = ISP_P2_BufQue_GetMatchIdx(param,
			ISP_P2_BUFQUE_MATCH_TYPE_WAITDQ,
			ISP_P2_BUFQUE_LIST_TAG_UNIT);
		spin_unlock(&(SpinLock_P2FrameList));
		if (idx ==  -1) {
			LOG_NOTICE(
			"Do not find match buffer (pty/pid/cid: %d/0x%x/0x%x) to deque!",
			    param.property, param.processID, param.callerID);
			ret =  -EFAULT;
			return ret;
		}
		{
			restTime = wait_event_interruptible_timeout(
					   P2WaitQueueHead_WaitDeque,
					   ISP_P2_BufQue_WaitEventState(param,
					       ISP_P2_BUFQUE_MATCH_TYPE_WAITDQ,
					       &idx),
					   ISP_UsToJiffies(15 * 1000000));
					/* wait 15s */
			if (restTime == 0) {
				LOG_NOTICE(
				"Wait Deque fail, idx(%d), pty(%d), pID(0x%x),cID(0x%x)",
				idx, param.property, param.processID,
				param.callerID);
				ret =  -EFAULT;
			} else if (restTime == -512) {
				LOG_NOTICE("be stopped, restime(%d)", restTime);
				ret =  -EFAULT;
				break;
			}
		}
		break;
	/* signal that a buffer is dequeued(success) */
	case ISP_P2_BUFQUE_CTRL_DEQUE_SUCCESS:
		if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL)
			pr_info("dq cm(%d),pID(0x%x),cID(0x%x)\n",
				param.ctrl, param.processID, param.callerID);

		spin_lock(&(SpinLock_P2FrameList));
		/* [1]update buffer status for the current buffer */
		/* //////////////////////////////////////////////////////////*/
		/* Assume we have the buffer list in the following situation */
		/* ++++++    ++++++ */
		/* +  vss +    +  prv + */
		/* ++++++    ++++++ */
		/*  */
		/* if the vss deque is not done(not blocking deque),*/
		/* dequeThread in userspace would change to deque prv buffer */
		/* (block deque) immediately to decrease ioctrl count. */
		/* -> vss buffer would be deque at next turn,*/
		/*    so curBuf is still at vss buffer node */
		/* -> we should use param to find the current buffer index in*/
		/*    Rlikst to update the buffer status */
		/*cuz deque success/fail may not be the first buffer in Rlist*/
		/* //////////////////////////////////////////////////////////*/
		idx2 = ISP_P2_BufQue_GetMatchIdx(param,
				ISP_P2_BUFQUE_MATCH_TYPE_FRAMEOP,
				ISP_P2_BUFQUE_LIST_TAG_UNIT);
		if (idx2 == -1) {
			spin_unlock(&(SpinLock_P2FrameList));
			LOG_NOTICE(
				"ERRRRRRRRRRR findmatch index 2 fail (%d_0x%x_0x%x_%d, %d_%d)",
				param.property, param.processID, param.callerID,
				param.frameNum, param.cQIdx, param.dupCQIdx);
			ret =  -EFAULT;
			return ret;
		}
		if (param.ctrl == ISP_P2_BUFQUE_CTRL_DEQUE_SUCCESS)
			P2_FrameUnit_List[property][idx2].bufSts =
				ISP_P2_BUF_STATE_DEQUE_SUCCESS;
		else
			P2_FrameUnit_List[property][idx2].bufSts =
				ISP_P2_BUF_STATE_DEQUE_FAIL;

		/* [2]update dequeued num in managed buffer list */
		idx = ISP_P2_BufQue_GetMatchIdx(param,
			ISP_P2_BUFQUE_MATCH_TYPE_FRAMEOP,
			ISP_P2_BUFQUE_LIST_TAG_PACKAGE);
		if (idx == -1) {
			spin_unlock(&(SpinLock_P2FrameList));
			LOG_NOTICE(
			"ERRRRRRRRRRR findmatch index 1 fail (%d_0x%x_0x%x_%d, %d_%d)",
				param.property, param.processID, param.callerID,
				param.frameNum, param.cQIdx, param.dupCQIdx);
			ret =  -EFAULT;
			return ret;
		}
		P2_FramePackage_List[property][idx].dequedNum++;
		/* [3]update global pointer */
		ISP_P2_BufQue_Update_ListCIdx(property,
			ISP_P2_BUFQUE_LIST_TAG_UNIT);
		/* [4]erase node in ring buffer list */
		ISP_P2_BufQue_Erase(property, ISP_P2_BUFQUE_LIST_TAG_UNIT,
			idx2);
		spin_unlock(&(SpinLock_P2FrameList));
		/* [5]wake up thread user that wait for a specific buffer
		 * and the thread that wait for deque
		 */
		wake_up_interruptible_all(&P2WaitQueueHead_WaitFrame);
		wake_up_interruptible_all(&P2WaitQueueHead_WaitDeque);
		break;
	/* signal that a buffer is dequeued(fail) */
	case ISP_P2_BUFQUE_CTRL_DEQUE_FAIL:
		/* QQ. ASSERT*/
		break;
	case ISP_P2_BUFQUE_CTRL_WAIT_FRAME:/* wait for a specific buffer */
		/* [1]find first match buffer */
		/*pr_info(
		 * "P2_BUFQUE_%d, before pty/pID/cID(%d/0x%x/0x%x),idx(%d)",
		 * param.ctrl, property, param.processID, param.callerID, idx);
		 */
		/* wait for frame enqued due to user might call deque api
		 * before the frame is enqued to kernel
		 */
		restTime = wait_event_interruptible_timeout(
				   P2WaitQueueHead_WaitFrameEQDforDQ,
				   ISP_P2_BufQue_WaitEventState(param,
					ISP_P2_BUFQUE_MATCH_TYPE_WAITFMEQD,
					&idx),
				   ISP_UsToJiffies(15 * 1000000));
				/* wait 15s to get paired frame */
		if (restTime == 0) {
			LOG_NOTICE(
			"could not find match buffer restTime(%d), pty/pID/cID (%d/0x%x/0x%x),idx(%d)",
				restTime, property, param.processID,
				param.callerID, idx);
			ret =  -EFAULT;
			return ret;
		} else if (restTime == -512) {
			LOG_NOTICE("be stopped, restime(%d)", restTime);
			ret =  -EFAULT;
			return ret;
		}

#ifdef P2_DBG_LOG
		pr_info("ISP_P2_BUFQUE_CTRL_WAIT_FRAME, after pty/pID/cID (%d/0x%x/0x%x),idx(%d)",
		property, param.processID, param.callerID, idx);
#endif
		spin_lock(&(SpinLock_P2FrameList));
		/* [2]check the buffer is dequeued or not */
		if (P2_FramePackage_List[property][idx].dequedNum ==
		    P2_FramePackage_List[property][idx].frameNum) {
			ISP_P2_BufQue_Erase(property,
				ISP_P2_BUFQUE_LIST_TAG_PACKAGE, idx);
			spin_unlock(&(SpinLock_P2FrameList));
			ret = 0;
#ifdef P2_DBG_LOG
			pr_info(
			"Frame is alreay dequeued, return user, pd(%d/0x%x/0x%x),idx(%d)",
				property, param.processID, param.callerID, idx);
#endif
			return ret;
		}
		{
			spin_unlock(&(SpinLock_P2FrameList));
			if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL)
				pr_info("=pd(%d/0x%x/0x%x_%d)wait(%d s)=\n",
					property, param.processID,
					param.callerID, idx, param.timeoutIns);

			/* [3]if not,
			 *    goto wait event and wait for a signal to check
			 */
			restTime = wait_event_interruptible_timeout(
				P2WaitQueueHead_WaitFrame,
				ISP_P2_BufQue_WaitEventState(param,
					ISP_P2_BUFQUE_MATCH_TYPE_WAITFM, &idx),
				ISP_UsToJiffies(param.timeoutIns * 1000000));
			if (restTime == 0) {
				LOG_NOTICE(
				"Dequeue Buffer fail, rT(%d),idx(%d) pty(%d), pID(0x%x),cID(0x%x)\n",
					restTime, idx, property,
					param.processID, param.callerID);
				ret =  -EFAULT;
				break;
			}
			if (restTime == -512) {
				LOG_NOTICE("be stopped, restime(%d)", restTime);
				ret =  -EFAULT;
				break;
			}
			{
				pr_info(
				"Dequeue Buffer ok, rT(%d),idx(%d) pty(%d), pID(0x%x),cID(0x%x)\n",
					restTime, idx, property,
					param.processID, param.callerID);
				spin_lock(&(SpinLock_P2FrameList));
				ISP_P2_BufQue_Erase(property,
					ISP_P2_BUFQUE_LIST_TAG_PACKAGE, idx);
				spin_unlock(&(SpinLock_P2FrameList));
			}
		}
		break;
	/* wake all slept users to check buffer is dequeued or not */
	case ISP_P2_BUFQUE_CTRL_WAKE_WAITFRAME:
		wake_up_interruptible_all(&P2WaitQueueHead_WaitFrame);
		break;
	/* free all recored dequeued buffer */
	case ISP_P2_BUFQUE_CTRL_CLAER_ALL:
		spin_lock(&(SpinLock_P2FrameList));
		for (q = 0; q < ISP_P2_BUFQUE_PROPERTY_NUM; q++) {
			for (i = 0; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
				P2_FrameUnit_List[q][i].processID = 0x0;
				P2_FrameUnit_List[q][i].callerID = 0x0;
				P2_FrameUnit_List[q][i].cqMask = 0x0;
				P2_FrameUnit_List[q][i].bufSts =
							ISP_P2_BUF_STATE_NONE;
			}
			P2_FrameUnit_List_Idx[q].start = 0;
			P2_FrameUnit_List_Idx[q].curr = 0;
			P2_FrameUnit_List_Idx[q].end =  -1;
			/*  */
			for (i = 0; i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
				P2_FramePackage_List[q][i].processID = 0x0;
				P2_FramePackage_List[q][i].callerID = 0x0;
				P2_FramePackage_List[q][i].dupCQIdx =  -1;
				P2_FramePackage_List[q][i].frameNum = 0;
				P2_FramePackage_List[q][i].dequedNum = 0;
			}
			P2_FramePack_List_Idx[q].start = 0;
			P2_FramePack_List_Idx[q].curr = 0;
			P2_FramePack_List_Idx[q].end =  -1;
		}
		spin_unlock(&(SpinLock_P2FrameList));
		break;
	default:
		LOG_NOTICE("do not support this ctrl cmd(%d)", param.ctrl);
		break;
	}
	return ret;
}

/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_REGISTER_IRQ_USERKEY(char *userName)
{
	int key =  -1;
	int i = 0;
	int length = 0;

	if (userName == NULL) {
		LOG_NOTICE(" [regUser] userName is NULL\n");
	} else {
		/*get UserName from user space */
		length = strnlen(userName, USERKEY_STR_LEN);
		if (length == 0) {
			LOG_NOTICE("[regUser] userName address is not valid\n");
			return key;
		}

		/*user key len at most 128*/
		length = (length > USERKEY_STR_LEN) ? USERKEY_STR_LEN : length;

		spin_lock((spinlock_t *)(&SpinLock_UserKey));
		/*check String length, add end */
		if (length == USERKEY_STR_LEN) {
			/*string length too long */
			userName[length - 1] = '\0';
			if (IspInfo.DebugMask & ISP_DBG_INT) {
				pr_debug(" [regUser] userName(%s) is too long (>%d)\n",
						userName, USERKEY_STR_LEN);
			}
		}

		if (IspInfo.DebugMask & ISP_DBG_INT)
			pr_info(" [regUser] UserName (%s)\n", userName);

		/* 1. check the current users is full or not */
		if (FirstUnusedIrqUserKey >= IRQ_USER_NUM_MAX ||
			FirstUnusedIrqUserKey < 0) {
			key = -1;
		} else {
			/* 2. check the user had registered or not */
			for (i = 1; i < FirstUnusedIrqUserKey; i++) {
			/* index 0 is for all the users
			 * that do not register irq first
			 */
				if (strcmp((void *)
						IrqUserKey_UserInfo[i].userName,
						userName) == 0) {
					key = IrqUserKey_UserInfo[i].userKey;
					break;
				}
			}

			/* 3.return new userkey for user
			 *   if the user had not registered before
			 */
			if (key < 0) {
				/* IrqUserKey_UserInfo[i].userName=userName; */
				memset((void *)
					IrqUserKey_UserInfo[i].userName,
					0,
					sizeof(IrqUserKey_UserInfo[i].userName)
				);
				strncpy(
					(void *)IrqUserKey_UserInfo[i].userName,
					userName,
					sizeof(IrqUserKey_UserInfo[i].userName)
						-1);
				IrqUserKey_UserInfo[i].userKey =
					FirstUnusedIrqUserKey;
				key = FirstUnusedIrqUserKey;
				FirstUnusedIrqUserKey++;
			}
		}

		spin_unlock((spinlock_t *)(&SpinLock_UserKey));
	}
	pr_info("User(%s)key(%d)\n", userName, key);
	return key;
}

/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_MARK_IRQ(struct ISP_WAIT_IRQ_STRUCT *irqinfo)
{
	unsigned long flags;
	int idx = my_get_pow_idx(irqinfo->EventInfo.Status);

	unsigned long long  sec = 0;
	unsigned long       usec = 0;

	if ((irqinfo->Type >= ISP_IRQ_TYPE_AMOUNT) ||
	    (irqinfo->Type < 0)) {
		LOG_NOTICE("MARK_IRQ: type error(%d)", irqinfo->Type);
		return -EFAULT;
	}

	if ((irqinfo->EventInfo.St_type >= ISP_IRQ_ST_AMOUNT) ||
	    (irqinfo->EventInfo.St_type < 0)) {
		LOG_NOTICE("MARK_IRQ: sq_type error(%d)",
				irqinfo->EventInfo.St_type);
		return -EFAULT;
	}

	if (irqinfo->EventInfo.UserKey >= IRQ_USER_NUM_MAX ||
	    irqinfo->EventInfo.UserKey < 0) {
		LOG_NOTICE("MARK_IRQ: userkey error(%d)",
			irqinfo->EventInfo.UserKey);
		return -EFAULT;
	}

	if ((idx < 0) || (idx >= 32)) {
		LOG_NOTICE("[Error] %s: Invalid idx(%d)", __func__, idx);
		return -EFAULT;
	}

	if (irqinfo->EventInfo.St_type == SIGNAL_INT) {
		/* 1. enable marked flag */
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqinfo->Type]), flags);
		IspInfo.IrqInfo.MarkedFlag[irqinfo->Type]
			[irqinfo->EventInfo.St_type][irqinfo->EventInfo.UserKey]
			|= irqinfo->EventInfo.Status;
		spin_unlock_irqrestore(
			&(IspInfo.SpinLockIrq[irqinfo->Type]), flags);

		/* 2. record mark time */
		sec = cpu_clock(0);     /* ns */
		do_div(sec, 1000);    /* usec */
		usec = do_div(sec, 1000000);    /* sec and usec */

		spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqinfo->Type]), flags);
		IspInfo.IrqInfo.MarkedTime_usec[irqinfo->Type][idx]
			[irqinfo->EventInfo.UserKey] = (unsigned int)usec;
		IspInfo.IrqInfo.MarkedTime_sec[irqinfo->Type][idx]
			[irqinfo->EventInfo.UserKey] = (unsigned int)sec;
		spin_unlock_irqrestore(
			&(IspInfo.SpinLockIrq[irqinfo->Type]), flags);

		/* 3. clear passed by signal count */
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqinfo->Type]), flags);
		IspInfo.IrqInfo.PassedBySigCnt[irqinfo->Type][idx]
					      [irqinfo->EventInfo.UserKey] = 0;
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqinfo->Type]),
					flags);

		pr_debug("[MARK]  key/type/sts/idx (%d/%d/0x%x/%d), t(%d/%d)\n",
		irqinfo->EventInfo.UserKey, irqinfo->Type,
		irqinfo->EventInfo.Status, idx, (int)sec, (int)usec);

	} else {
		LOG_NOTICE(
			"Not support DMA interrupt type(%d), Only support signal interrupt!!!",
			irqinfo->EventInfo.St_type);
		return -EFAULT;
	}

	return 0;
}


/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_GET_MARKtoQEURY_TIME(struct ISP_WAIT_IRQ_STRUCT *irqinfo)
{
	signed int Ret = 0;
	/*    unsigned int flags;*/
	/*    struct timeval time_getrequest;*/
	/*    struct timeval time_ready2return;*/

	/*    unsigned long long  sec = 0;*/
	/*    unsigned long       usec = 0;*/

#if 0
	if (irqinfo->EventInfo.St_type == SIGNAL_INT) {


		/* do_gettimeofday(&time_ready2return); */
		sec = cpu_clock(0);     /* ns */
		do_div(sec, 1000);    /* usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		time_ready2return.tv_usec = usec;
		time_ready2return.tv_sec = sec;

		int idx = my_get_pow_idx(irqinfo->EventInfo.Status);


spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqinfo->Type]), flags);
if (irqinfo->EventInfo.Status & IspInfo.IrqInfo.MarkedFlag
		[irqinfo->Type][irqinfo->EventInfo.UserKey]) {
	/*  */
	irqinfo->EventInfo.TimeInfo.passedbySigcnt =
	IspInfo.IrqInfo.PassedBySigCnt[irqinfo->Type][idx]
				[irqinfo->EventInfo.UserKey];
	/*  */
	irqinfo->EventInfo.TimeInfo.tMark2WaitSig_usec =
	(time_ready2return.tv_usec -
	IspInfo.IrqInfo.MarkedTime_usec[irqinfo->Type]
			[idx][irqinfo->EventInfo.UserKey]);
	irqinfo->EventInfo.TimeInfo.tMark2WaitSig_sec =
	(time_ready2return.tv_sec -
	IspInfo.IrqInfo.MarkedTime_sec[irqinfo->Type][idx]
				[irqinfo->EventInfo.UserKey]);
	if ((int)(irqinfo->EventInfo.TimeInfo.tMark2WaitSig_usec) < 0) {
		irqinfo->EventInfo.TimeInfo.tMark2WaitSig_sec =
		irqinfo->EventInfo.TimeInfo.tMark2WaitSig_sec - 1;
		if ((int)(irqinfo->EventInfo.TimeInfo.tMark2WaitSig_sec)
		< 0)
			irqinfo->EventInfo.TimeInfo.tMark2WaitSig_sec
			= 0;

		irqinfo->EventInfo.TimeInfo.tMark2WaitSig_usec =
		1 * 1000000 +
		irqinfo->EventInfo.TimeInfo.tMark2WaitSig_usec;
	}
	/*  */
	if (irqinfo->EventInfo.TimeInfo.passedbySigcnt > 0) {
		irqinfo->EventInfo.TimeInfo.tLastSig2GetSig_usec
		= (time_ready2return.tv_usec -
		IspInfo.IrqInfo.LastestSigTime_usec
			[irqinfo->Type][idx]);
		irqinfo->EventInfo.TimeInfo.tLastSig2GetSig_sec
		= (time_ready2return.tv_sec -
		IspInfo.IrqInfo.LastestSigTime_sec
			[irqinfo->Type][idx]);
		if ((int)(irqinfo->EventInfo.TimeInfo.tLastSig2GetSig_usec)
		    < 0) {
			irqinfo->EventInfo.TimeInfo.tLastSig2GetSig_sec =
			irqinfo->EventInfo.TimeInfo.tLastSig2GetSig_sec - 1;
			if ((int)
			    (irqinfo->EventInfo.TimeInfo.tLastSig2GetSig_sec)
			    < 0)
				irqinfo->EventInfo.TimeInfo.tLastSig2GetSig_sec
				= 0;

			irqinfo->EventInfo.TimeInfo.tLastSig2GetSig_usec
			= 1 * 1000000 +
			irqinfo->EventInfo.TimeInfo.tLastSig2GetSig_usec;
		}
	} else {
		irqinfo->EventInfo.TimeInfo.tLastSig2GetSig_usec
		= 0;
		irqinfo->EventInfo.TimeInfo.tLastSig2GetSig_sec
		= 0;
	}
} else {
	LOG_NOTICE(
		"plz mark irq first, userKey/Type/Status (%d/%d/0x%x)",
		irqinfo->EventInfo.UserKey, irqinfo->Type,
		irqinfo->EventInfo.Status);
	Ret = -EFAULT;
}
		spin_unlock_irqrestore(
			&(IspInfo.SpinLockIrq[irqinfo->Type]), flags);
		pr_debug(
			"MKtoQT:u/t/i(%d/%d/%d) (%d/%d) (%d/%d) (%d/%d) sig(%d)\n",
			irqinfo->EventInfo.UserKey, irqinfo->Type, idx,
			IspInfo.IrqInfo.MarkedTime_sec
			    [irqinfo->Type][idx][irqinfo->EventInfo.UserKey],
			IspInfo.IrqInfo.MarkedTime_usec
			    [irqinfo->Type][idx][irqinfo->EventInfo.UserKey],
			IspInfo.IrqInfo.LastestSigTime_sec[irqinfo->Type][idx],
			IspInfo.IrqInfo.LastestSigTime_usec[irqinfo->Type][idx],
			(int)time_ready2return.tv_sec,
			(int)time_ready2return.tv_usec,
			IspInfo.IrqInfo.PassedBySigCnt
			    [irqinfo->Type][idx][irqinfo->EventInfo.UserKey]);
		return Ret;
	}
	{
		LOG_NOTICE(
			"Not support DMA interrupt type(%d), Only support signal interrupt!!!",
			irqinfo->EventInfo.St_type);
		Ret = -EFAULT;
	}
#endif

	return Ret;

}

/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_FLUSH_IRQ(struct ISP_WAIT_IRQ_STRUCT *irqinfo)
{
	unsigned long flags;

	pr_info("type(%d)userKey(%d)St_type(%d)St(0x%x)",
		irqinfo->Type, irqinfo->EventInfo.UserKey,
		irqinfo->EventInfo.St_type, irqinfo->EventInfo.Status);

	if ((irqinfo->Type >= ISP_IRQ_TYPE_AMOUNT) ||
	    (irqinfo->Type < 0)) {
		LOG_NOTICE("FLUSH_IRQ: type error(%d)", irqinfo->Type);
		return -EFAULT;
	}

	if ((irqinfo->EventInfo.St_type >= ISP_IRQ_ST_AMOUNT) ||
	    (irqinfo->EventInfo.St_type < 0)) {
		LOG_NOTICE("FLUSH_IRQ: st_type error(%d)",
			irqinfo->EventInfo.St_type);
		return -EFAULT;
	}

	if ((irqinfo->EventInfo.UserKey >= IRQ_USER_NUM_MAX) ||
	    (irqinfo->EventInfo.UserKey < 0)) {
		LOG_NOTICE("FLUSH_IRQ: userkey error(%d)",
			irqinfo->EventInfo.UserKey);
		return -EFAULT;
	}

	/* 1. enable signal */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqinfo->Type]), flags);
	IspInfo.IrqInfo.Status[irqinfo->Type][irqinfo->EventInfo.St_type]
			      [irqinfo->EventInfo.UserKey] |=
	irqinfo->EventInfo.Status;
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqinfo->Type]), flags);

	/* 2. force to wake up the user that are waiting for that signal */
	if (ISP_CheckUseCamWaitQ(irqinfo->Type,
		irqinfo->EventInfo.St_type,
		irqinfo->EventInfo.Status)) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(irqinfo->Type)]
			[ISP_GetWaitQCamIrqIndex(
			irqinfo->EventInfo.St_type,
			irqinfo->EventInfo.Status)]);
	} else if (ISP_CheckUseCamsvWaitQ(irqinfo->Type,
				irqinfo->EventInfo.St_type,
				irqinfo->EventInfo.Status)) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCamsv
			[ISP_GetWaitQCamsvIndex(irqinfo->Type)]
			[ISP_GetWaitQCamsvIrqIndex(
			irqinfo->EventInfo.St_type,
			irqinfo->EventInfo.Status)]);
	} else {
		wake_up_interruptible(&IspInfo.WaitQueueHead[irqinfo->Type]);
	}

	return 0;
}


/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_WaitIrq(struct ISP_WAIT_IRQ_STRUCT *WaitIrq)
{

	signed int Ret = 0, Timeout = WaitIrq->EventInfo.Timeout;
	unsigned long flags;
	unsigned int irqStatus;
	unsigned int idx;
	bool freeze_passbysigcnt = false;

	if ((WaitIrq->Type >= ISP_IRQ_TYPE_AMOUNT) ||
	    (WaitIrq->Type < 0)) {
		pr_info("invalid Type(%d), max(%d)\n",
			WaitIrq->Type, ISP_IRQ_TYPE_AMOUNT);
		return -EINVAL;
	}

	if ((WaitIrq->EventInfo.St_type >= ISP_IRQ_ST_AMOUNT) ||
	    (WaitIrq->EventInfo.St_type < 0)) {
		pr_info("invalid St_type(%d), max(%d)\n",
			WaitIrq->EventInfo.St_type, ISP_IRQ_ST_AMOUNT);
		return -EINVAL;
	}

	if ((WaitIrq->EventInfo.UserKey >= IRQ_USER_NUM_MAX) ||
	    (WaitIrq->EventInfo.UserKey < 0)) {
		pr_info("invalid userKey(%d), max(%d)\n",
			WaitIrq->EventInfo.UserKey, IRQ_USER_NUM_MAX);
		return -EINVAL;
	}

#ifdef ENABLE_WAITIRQ_LOG
	/* Debug interrupt */
	if (IspInfo.DebugMask & ISP_DBG_INT) {
		if (WaitIrq->EventInfo.Status &
		    IspInfo.IrqInfo.Mask[WaitIrq->Type]
					[WaitIrq->EventInfo.St_type]) {
			if (WaitIrq->EventInfo.UserKey > 0) {
				pr_info("+WaitIrq Clear(%d), Type(%d), Status(0x%08X), Timeout(%d/%d),user(%d)\n",
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
		/* pr_info(
		 * "WARNING:Clear(%d),Type(%d),IrqStat(0x%08X)has been cleared"
		 * ,WaitIrq->EventInfo.Clear,WaitIrq->Type,
		 * IspInfo.IrqInfo.Status[WaitIrq->Type]);
		 */
		IspInfo.IrqInfo.Status[WaitIrq->Type]
				      [WaitIrq->EventInfo.St_type]
				      [WaitIrq->EventInfo.UserKey]
				&= (~WaitIrq->EventInfo.Status);
		spin_unlock_irqrestore(
			&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
		return Ret;
	}
	{
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
		if (WaitIrq->EventInfo.Status &
		IspInfo.IrqInfo.MarkedFlag[WaitIrq->Type]
					  [WaitIrq->EventInfo.St_type]
					  [WaitIrq->EventInfo.UserKey]) {
			spin_unlock_irqrestore(
				&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
			/* force to be non_clear wait if marked before, */
			/* and check the request wait timing */
			/* if the entry time of wait request after mark */
			/* is before signal, */
			/* we freese the counting for passby signal */
			/*  */
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

			freeze_passbysigcnt = !(ISP_GetIRQState(WaitIrq->Type,
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
				if (IspInfo.IrqInfo.Status[WaitIrq->Type]
						[WaitIrq->EventInfo.St_type]
						[WaitIrq->EventInfo.UserKey]
				& WaitIrq->EventInfo.Status)
					IspInfo.IrqInfo.Status[WaitIrq->Type][
						WaitIrq->EventInfo.St_type]
						[WaitIrq->EventInfo.UserKey] &=
						(~WaitIrq->EventInfo.Status);

				spin_unlock_irqrestore(
				  &(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
			} else if (WaitIrq->EventInfo.Clear ==
			ISP_IRQ_CLEAR_ALL) {
				spin_lock_irqsave(
					&(IspInfo.SpinLockIrq[WaitIrq->Type]),
					flags);

				IspInfo.IrqInfo.Status[WaitIrq->Type]
					[WaitIrq->EventInfo.St_type]
					[WaitIrq->EventInfo.UserKey] = 0;
				spin_unlock_irqrestore(
					&(IspInfo.SpinLockIrq[WaitIrq->Type]),
					flags);
			}
		}
	}

#ifdef ENABLE_WAITIRQ_LOG
	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
	irqStatus = IspInfo.IrqInfo.Status[WaitIrq->Type]
					  [WaitIrq->EventInfo.St_type]
					  [WaitIrq->EventInfo.UserKey];
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
#endif

	if (WaitIrq->EventInfo.Clear == ISP_IRQ_CLEAR_NONE) {
		if (IspInfo.IrqInfo.Status[WaitIrq->Type]
					  [WaitIrq->EventInfo.St_type]
					  [WaitIrq->EventInfo.UserKey]
			& WaitIrq->EventInfo.Status) {
#ifdef ENABLE_WAITIRQ_LOG
			pr_info("%s%s%s%s",
			"Already have irq!!!: ",
			"WaitIrq Timeout(%d) Clear(%d), Type(%d), StType(%d), ",
			"IrqStatus(0x%08X), WaitStatus(0x%08X), Timeout(%d), ",
			"userKey(%d)\n",
				WaitIrq->EventInfo.Timeout,
				WaitIrq->EventInfo.Clear,
				WaitIrq->Type,
				WaitIrq->EventInfo.St_type,
				irqStatus,
				WaitIrq->EventInfo.Status,
				WaitIrq->EventInfo.Timeout,
				WaitIrq->EventInfo.UserKey);
			}
#endif

			goto NON_CLEAR_WAIT;
		}
	}

#ifdef ENABLE_WAITIRQ_LOG
	pr_info("before wait: Clear(%d) Type(%d) StType(%d) Sts(0x%08X) WaitSts(0x%08X) Timeout(%d) userKey(%d)\n",
		WaitIrq->EventInfo.Clear,
		WaitIrq->Type,
		WaitIrq->EventInfo.St_type,
		irqStatus,
		WaitIrq->EventInfo.Status,
		WaitIrq->EventInfo.Timeout,
		WaitIrq->EventInfo.UserKey);
#endif

	/* 2. start to wait signal */
	if (ISP_CheckUseCamWaitQ(WaitIrq->Type,
		WaitIrq->EventInfo.St_type,
		WaitIrq->EventInfo.Status)) {
		Timeout = wait_event_interruptible_timeout(
				  IspInfo.WaitQHeadCam
				  [ISP_GetWaitQCamIndex(WaitIrq->Type)]
				  [ISP_GetWaitQCamIrqIndex(
				  WaitIrq->EventInfo.St_type,
				  WaitIrq->EventInfo.Status)],
				  ISP_GetIRQState(WaitIrq->Type,
				  WaitIrq->EventInfo.St_type,
				  WaitIrq->EventInfo.UserKey,
				  WaitIrq->EventInfo.Status),
				  ISP_MsToJiffies(
				  WaitIrq->EventInfo.Timeout));
	} else if (ISP_CheckUseCamsvWaitQ(WaitIrq->Type,
				WaitIrq->EventInfo.St_type,
				WaitIrq->EventInfo.Status)) {
		Timeout = wait_event_interruptible_timeout(
				  IspInfo.WaitQHeadCamsv
				  [ISP_GetWaitQCamsvIndex(WaitIrq->Type)]
				  [ISP_GetWaitQCamsvIrqIndex(
				  WaitIrq->EventInfo.St_type,
				  WaitIrq->EventInfo.Status)],
				  ISP_GetIRQState(WaitIrq->Type,
				  WaitIrq->EventInfo.St_type,
				  WaitIrq->EventInfo.UserKey,
				  WaitIrq->EventInfo.Status),
				  ISP_MsToJiffies(
				  WaitIrq->EventInfo.Timeout));
	} else {
		Timeout = wait_event_interruptible_timeout(
				  IspInfo.WaitQueueHead[WaitIrq->Type],
				  ISP_GetIRQState(WaitIrq->Type,
				  WaitIrq->EventInfo.St_type,
				  WaitIrq->EventInfo.UserKey,
				  WaitIrq->EventInfo.Status),
				  ISP_MsToJiffies(
				  WaitIrq->EventInfo.Timeout));
	}

	/* check if user is interrupted by system signal */
	if ((Timeout != 0) && (!ISP_GetIRQState(WaitIrq->Type,
	    WaitIrq->EventInfo.St_type, WaitIrq->EventInfo.UserKey,
	    WaitIrq->EventInfo.Status))) {
		pr_info("interrupted by system signal,return value(%d),irq Type/User/Sts(0x%x/%d/0x%x)\n",
			Timeout, WaitIrq->Type, WaitIrq->EventInfo.UserKey,
			WaitIrq->EventInfo.Status);
		/* actually it should be -ERESTARTSYS */
		Ret = -SIG_ERESTARTSYS;
		goto EXIT;
	} else if (Timeout == 0) { /* timeout */
		/* Store irqStatus here to redeuce time of spin_lock_irqsave */
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus =
		IspInfo.IrqInfo.Status[WaitIrq->Type]
				      [WaitIrq->EventInfo.St_type]
				      [WaitIrq->EventInfo.UserKey];
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[WaitIrq->Type]),
					flags);

		LOG_NOTICE(
		"ERRRR WaitIrq Clear(%d) Type(%d) StType(%d) Status(0x%08X) WaitStatus(0x%08X) Timeout(%d) key(%d)\n",
		WaitIrq->EventInfo.Clear,
		WaitIrq->Type,
		WaitIrq->EventInfo.St_type,
		irqStatus,
		WaitIrq->EventInfo.Status,
		WaitIrq->EventInfo.Timeout,
		WaitIrq->EventInfo.UserKey);

		if (WaitIrq->bDumpReg)
			ISP_DumpReg();

		if (WaitIrq->bDumpReg &&
			((WaitIrq->EventInfo.Status == SOF_INT_ST) ||
			 (WaitIrq->EventInfo.Status == SW_PASS1_DON_ST))) {
			ISP_DumpSeninfReg();
		}
		Ret = -EFAULT;
		goto EXIT;
	}

#ifdef ENABLE_WAITIRQ_LOG
	else {
		/* Store irqStatus here to redeuce time of spin_lock_irqsave */
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus =
			IspInfo.IrqInfo.Status[WaitIrq->Type]
					      [WaitIrq->EventInfo.St_type]
					      [WaitIrq->EventInfo.UserKey];
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[WaitIrq->Type]),
					flags);

		pr_info(
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
	 *    that would be return to user
	 */

	spin_lock_irqsave(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
	/* clear the status if someone get the irq */
	IspInfo.IrqInfo.Status[WaitIrq->Type][WaitIrq->EventInfo.St_type]
			      [WaitIrq->EventInfo.UserKey]
		&= (~WaitIrq->EventInfo.Status);
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);

EXIT:
	/* 4. clear mark flag / reset marked time /
	 *    reset time related infor and passedby signal count
	 */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);
	if (WaitIrq->EventInfo.Status &
	    IspInfo.IrqInfo.MarkedFlag[WaitIrq->Type]
				      [WaitIrq->EventInfo.St_type]
				      [WaitIrq->EventInfo.UserKey]) {
		idx = my_get_pow_idx(WaitIrq->EventInfo.Status);
		if ((idx < 0) || (idx >= 32)) {
			LOG_NOTICE("[Error] %s: Invalid idx", __func__);
			Ret = -EFAULT;
			return Ret;
		}
		IspInfo.IrqInfo.MarkedFlag[WaitIrq->Type]
					  [WaitIrq->EventInfo.St_type]
					  [WaitIrq->EventInfo.UserKey] &=
						(~WaitIrq->EventInfo.Status);
		IspInfo.IrqInfo.MarkedTime_usec[WaitIrq->Type][idx]
					      [WaitIrq->EventInfo.UserKey] = 0;
		IspInfo.IrqInfo.MarkedTime_sec[WaitIrq->Type][idx]
					      [WaitIrq->EventInfo.UserKey] = 0;
		IspInfo.IrqInfo.PassedBySigCnt[WaitIrq->Type][idx]
					      [WaitIrq->EventInfo.UserKey] = 0;
	}
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[WaitIrq->Type]), flags);


	return Ret;
}


#ifdef ENABLE_KEEP_ION_HANDLE
/******************************************************************************
 *
 *****************************************************************************/
static void ISP_ion_init(void)
{
	if (!pIon_client && g_ion_device)
		pIon_client = ion_client_create(g_ion_device, "camera_isp");

	if (!pIon_client) {
		LOG_NOTICE("invalid ion client!\n");
		return;
	}

	if (IspInfo.DebugMask & ISP_DBG_ION_CTRL)
		pr_info("create ion client 0x%pK\n", pIon_client);
}

/******************************************************************************
 *
 *****************************************************************************/
static void ISP_ion_uninit(void)
{
	if (!pIon_client) {
		LOG_NOTICE("invalid ion client!\n");
		return;
	}

	if (IspInfo.DebugMask & ISP_DBG_ION_CTRL)
		pr_info("destroy ion client 0x%pK\n", pIon_client);

	ion_client_destroy(pIon_client);

	pIon_client = NULL;
}

/******************************************************************************
 *
 *****************************************************************************/
static struct ion_handle *ISP_ion_import_handle(struct ion_client *client,
						int fd)
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
		pr_info("[ion_import_hd] Hd(0x%pK)\n", handle);
	return handle;
}

/******************************************************************************
 *
 *****************************************************************************/
static void ISP_ion_free_handle(struct ion_client *client,
				struct ion_handle *handle)
{
	if (!client) {
		LOG_NOTICE("invalid ion client!\n");
		return;
	}
	if (!handle)
		return;

	if (IspInfo.DebugMask & ISP_DBG_ION_CTRL)
		pr_info("[ion_free_hd] Hd(0x%pK)\n", handle);

	ion_free(client, handle);

}

/******************************************************************************
 *
 *****************************************************************************/
static void ISP_ion_free_handle_by_module(unsigned int module)
{
	int i, j;
	signed int nFd;
	struct ion_handle *p_IonHnd;
	struct T_ION_TBL *ptbl = &gION_TBL[module];

	if (IspInfo.DebugMask & ISP_DBG_ION_CTRL)
		pr_info("[ion_free_hd_by_module]%d\n", module);

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
				pr_info(
					"ion_free: dev(%d)dma(%d)j(%d)fd(%d)Hnd(0x%pK)\n",
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
 *****************************************************************************/
static long ISP_ioctl(struct file *pFile, unsigned int Cmd, unsigned long Param)
{
	signed int Ret = 0;
	/*  */
	/*    bool   HoldEnable = MFALSE;*/
	unsigned int DebugFlag[3] = {0};
	unsigned int Dapc_Reg[6] = {0};
	/*    unsigned int pid = 0;*/
	struct ISP_REG_IO_STRUCT       RegIo;
	struct ISP_DUMP_BUFFER_STRUCT DumpBufStruct;
	struct ISP_MEM_INFO_STRUCT MemInfoStruct;
	struct ISP_WAIT_IRQ_STRUCT     IrqInfo;
	struct ISP_USER_INFO_STRUCT *pUserInfo;
	struct ISP_P2_BUFQUE_STRUCT    p2QueBuf;
	unsigned int                 regScenInfo_value = 0xa5a5a5a5;
	/*    signed int                  burstQNum;*/
	unsigned int                 wakelock_ctrl;
	unsigned int                 module;
	unsigned long flags;
	int userKey =  -1;

	int i;
	#ifdef ENABLE_KEEP_ION_HANDLE
	struct ISP_DEV_ION_NODE_STRUCT IonNode;
	struct ion_handle *handle;
	struct ion_handle *p_IonHnd;
	#endif

	struct ISP_CLK_INFO ispclks;
	unsigned int lv = 0;
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
			LOG_NOTICE("get ISP_WAKELOCK_CTRL from user fail\n");
			Ret = -EFAULT;
		} else {
			if (wakelock_ctrl == 1) {/* Enable wakelock */
				if (g_WaitLockCt) {
					g_WaitLockCt++;
					pr_info("add wakelock cnt(%d)\n",
						g_WaitLockCt);
				} else {
#ifdef CONFIG_PM_SLEEP
					__pm_stay_awake(isp_wake_lock);
#endif
					g_WaitLockCt++;
					pr_info("wakelock enable!! cnt(%d)\n",
						g_WaitLockCt);
				}
			} else {        /* Disable wakelock */
				if (g_WaitLockCt)
					g_WaitLockCt--;

				if (g_WaitLockCt)
					pr_info("subtract wakelock cnt(%d)\n",
						g_WaitLockCt);
				else {
#ifdef CONFIG_PM_SLEEP
					__pm_relax(isp_wake_lock);
#endif
					pr_info("wakelock disable!! cnt(%d)\n",
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
			switch (DebugFlag[0]) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				spin_lock_irqsave(&(IspInfo.SpinLockIrq[
					    ISP_IRQ_TYPE_INT_CAM_A_ST]), flags);
				DebugFlag[1] = FrameStatus[
						ISP_IRQ_TYPE_INT_CAM_A_ST];
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[
					    ISP_IRQ_TYPE_INT_CAM_A_ST]), flags);
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				spin_lock_irqsave(&(IspInfo.SpinLockIrq[
					    ISP_IRQ_TYPE_INT_CAM_B_ST]), flags);
				DebugFlag[1] = FrameStatus[
						ISP_IRQ_TYPE_INT_CAM_B_ST];
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[
					    ISP_IRQ_TYPE_INT_CAM_B_ST]), flags);
				break;
			case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
				spin_lock_irqsave(&(IspInfo.SpinLockIrq[
					  ISP_IRQ_TYPE_INT_CAMSV_0_ST]), flags);
				DebugFlag[1] = FrameStatus[
						ISP_IRQ_TYPE_INT_CAMSV_0_ST];
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[
					  ISP_IRQ_TYPE_INT_CAMSV_0_ST]), flags);
				break;
			case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
				spin_lock_irqsave(&(IspInfo.SpinLockIrq[
					  ISP_IRQ_TYPE_INT_CAMSV_1_ST]), flags);
				DebugFlag[1] = FrameStatus[
						ISP_IRQ_TYPE_INT_CAMSV_1_ST];
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[
					  ISP_IRQ_TYPE_INT_CAMSV_1_ST]), flags);
				break;
			default:
				LOG_NOTICE("err TG(0x%x)\n", DebugFlag[0]);
				Ret = -EFAULT;
				break;
			}
			if (copy_to_user((void *)Param, &DebugFlag[1],
			    sizeof(unsigned int)) != 0) {
				LOG_NOTICE("copy to user fail\n");
				Ret = -EFAULT;
			}
		}
		break;
	case ISP_GET_INT_ERR:
		if (copy_to_user((void *)Param, (void *)g_ISPIntErr,
		    sizeof(unsigned int)*ISP_IRQ_TYPE_AMOUNT) != 0)
			LOG_NOTICE("get int err fail\n");
		else
			memset((void *)g_ISPIntErr, 0, sizeof(g_ISPIntErr));

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
			if (copy_to_user((void *)Param,
			    &g_DmaErr_CAM[DebugFlag[0]],
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
#if 0
			switch (DebugFlag[0]) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				DebugFlag[1] = ISP_RD32_TG_CAM_FRM_CNT(
				  ISP_IRQ_TYPE_INT_CAM_A_ST, ISP_CAM_A_IDX);
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				DebugFlag[1] = ISP_RD32_TG_CAM_FRM_CNT(
				  ISP_IRQ_TYPE_INT_CAM_B_ST, ISP_CAM_B_IDX));
				break;
			case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
				DebugFlag[1] = ISP_RD32_TG_CAM_FRM_CNT(
				  ISP_IRQ_TYPE_INT_CAMSV_0_ST, ISP_CAMSV0_IDX);
				break;
			case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
				DebugFlag[1] =  ISP_RD32_TG_CAM_FRM_CNT(
				  ISP_IRQ_TYPE_INT_CAMSV_1_ST, ISP_CAMSV1_IDX);
				break;
			default:
				LOG_NOTICE("err TG(0x%x)\n", DebugFlag[0]);
				Ret = -EFAULT;
				break;
			}
#else
			if (DebugFlag[0] >= ISP_IRQ_TYPE_AMOUNT) {
				LOG_NOTICE("cursof: error type(%d)\n",
					DebugFlag[0]);
				Ret = -EFAULT;
				break;
			}
			DebugFlag[1] = sof_count[DebugFlag[0]];
#endif
		}
		if (copy_to_user((void *)Param, &DebugFlag[1],
		    sizeof(unsigned int)) != 0) {
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
			/* 2nd layer behavoir of copy from user is implemented
			 * in ISP_ReadReg(...)
			 */
			if ((RegIo.Count * sizeof(struct ISP_REG_STRUCT)) > 0xFFFFF000) {
				Ret = -EFAULT;
				LOG_NOTICE("RegIo.Count error\n");
				goto EXIT;
			}
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
			/* 2nd layer behavoir of copy from user is implemented
			 * in ISP_WriteReg(...)
			 */
			//0831-s
			if ((RegIo.Count * sizeof(struct ISP_REG_STRUCT)) > 0xFFFFF000) {
				Ret = -EFAULT;
				LOG_NOTICE("RegIo.Count error\n");
				goto EXIT;
			}
			//0831-e
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
			pr_info(
				"IRQ type(%d), userKey(%d), timeout(%d), userkey(%d), st_status(%d), status(%d)\n",
				IrqInfo.Type, IrqInfo.EventInfo.UserKey,
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
		struct ISP_CLEAR_IRQ_STRUCT    ClearIrq;

		if (copy_from_user(&ClearIrq, (void *)Param,
		    sizeof(struct ISP_CLEAR_IRQ_STRUCT)) == 0) {
			pr_info("ISP_CLEAR_IRQ Type(%d)\n", ClearIrq.Type);

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

			/*assign i to avoid line over 80 char per line */
			i = ClearIrq.EventInfo.UserKey;
			pr_info(
				"ISP_CLEAR_IRQ:Type(%d),Status(0x%x),st_status(%d),IrqStatus(0x%x)\n",
				ClearIrq.Type, ClearIrq.EventInfo.Status,
				ClearIrq.EventInfo.St_type,
				IspInfo.IrqInfo.Status[ClearIrq.Type]
					[ClearIrq.EventInfo.St_type][i]);
			spin_lock_irqsave(&(IspInfo.SpinLockIrq[ClearIrq.Type]),
						flags);
			IspInfo.IrqInfo.Status[ClearIrq.Type]
				[ClearIrq.EventInfo.St_type]
				[ClearIrq.EventInfo.UserKey] &=
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
	case ISP_REGISTER_IRQ_USER_KEY: {
		struct ISP_REGISTER_USERKEY_STRUCT RegUserKey;

		if (copy_from_user(&RegUserKey, (void *)Param,
		    sizeof(struct ISP_REGISTER_USERKEY_STRUCT)) == 0) {
			//to avoid strncpy violation, force end to '\0'
			RegUserKey.userName[32-1] = 0;
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
	}
	/*  */
	case ISP_MARK_IRQ_REQUEST:
		if (copy_from_user(&IrqInfo, (void *)Param,
		    sizeof(struct ISP_WAIT_IRQ_STRUCT)) == 0) {
			if ((IrqInfo.EventInfo.UserKey >= IRQ_USER_NUM_MAX) ||
			    (IrqInfo.EventInfo.UserKey < 1)) {
				LOG_NOTICE("invalid userKey(%d), max(%d)\n",
					IrqInfo.EventInfo.UserKey,
					IRQ_USER_NUM_MAX);
				Ret = -EFAULT;
				break;
			}
			if ((IrqInfo.Type >= ISP_IRQ_TYPE_AMOUNT) ||
			    (IrqInfo.Type < 0)) {
				LOG_NOTICE("invalid type(%d), max(%d)\n",
					IrqInfo.Type, ISP_IRQ_TYPE_AMOUNT);
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
			Ret = ISP_MARK_IRQ(&IrqInfo);
		} else {
			LOG_NOTICE("ISP_MARK_IRQ, copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	/*  */
	case ISP_GET_MARK2QUERY_TIME:
		if (copy_from_user(&IrqInfo, (void *)Param,
		    sizeof(struct ISP_WAIT_IRQ_STRUCT)) == 0) {
			if ((IrqInfo.EventInfo.UserKey >= IRQ_USER_NUM_MAX) ||
			    (IrqInfo.EventInfo.UserKey < 1)) {
				LOG_NOTICE("invalid userKey(%d), max(%d)\n",
					IrqInfo.EventInfo.UserKey,
					IRQ_USER_NUM_MAX);
				Ret = -EFAULT;
				break;
			}
			if ((IrqInfo.Type >= ISP_IRQ_TYPE_AMOUNT) ||
			    (IrqInfo.Type < 0)) {
				LOG_NOTICE("invalid type(%d), max(%d)\n",
					IrqInfo.Type, ISP_IRQ_TYPE_AMOUNT);
				Ret = -EFAULT;
				break;
			}
			Ret = ISP_GET_MARKtoQEURY_TIME(&IrqInfo);
			/*  */
			if (copy_to_user((void *)Param, &IrqInfo,
			    sizeof(struct ISP_WAIT_IRQ_STRUCT)) != 0) {
				LOG_NOTICE("copy_to_user failed\n");
				Ret = -EFAULT;
			}
		} else {
			LOG_NOTICE(
				"ISP_GET_MARK2QUERY_TIME, copy_from_user failed\n");
			Ret = -EFAULT;
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
					IrqInfo.Type, ISP_IRQ_TYPE_AMOUNT);
				Ret = -EFAULT;
				break;
			}
			if ((IrqInfo.EventInfo.St_type >= ISP_IRQ_ST_AMOUNT) ||
			    (IrqInfo.EventInfo.St_type < 0)) {
				pr_notice(
					"invalid St_type(%d), max(%d), force St_type = 0\n",
					IrqInfo.EventInfo.St_type,
					ISP_IRQ_ST_AMOUNT);
				IrqInfo.EventInfo.St_type = 0;
			}

			Ret = ISP_FLUSH_IRQ(&IrqInfo);
		}
		break;
	/*  */
	case ISP_P2_BUFQUE_CTRL:
		if (copy_from_user(&p2QueBuf, (void *)Param,
		    sizeof(struct ISP_P2_BUFQUE_STRUCT)) == 0) {
			p2QueBuf.processID = pUserInfo->Pid;
			Ret = ISP_P2_BufQue_CTRL_FUNC(p2QueBuf);
		} else {
			LOG_NOTICE("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	/*  */
	case ISP_UPDATE_REGSCEN:
		if (copy_from_user(&regScenInfo_value, (void *)Param,
		    sizeof(unsigned int)) == 0) {
			spin_lock((spinlock_t *)(&SpinLockRegScen));
			g_regScen = regScenInfo_value;
			spin_unlock((spinlock_t *)(&SpinLockRegScen));
		} else {
			LOG_NOTICE("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	case ISP_QUERY_REGSCEN:
		spin_lock((spinlock_t *)(&SpinLockRegScen));
		regScenInfo_value = g_regScen;
		spin_unlock((spinlock_t *)(&SpinLockRegScen));
		/*      */
		if (copy_to_user((void *)Param, &regScenInfo_value,
		    sizeof(unsigned int)) != 0) {
			LOG_NOTICE("copy_to_user failed\n");
			Ret = -EFAULT;
		}
		break;
	/*  */
	case ISP_UPDATE_BURSTQNUM:
#if 0 /* QQ, remove later*/
		if (copy_from_user(&burstQNum, (void *)Param,
		    sizeof(signed int)) == 0) {
			spin_lock(&SpinLockRegScen);
			P2_Support_BurstQNum = burstQNum;
			spin_unlock(&SpinLockRegScen);
			pr_info("new BurstQNum(%d)", P2_Support_BurstQNum);
		} else {
			LOG_NOTICE("copy_from_user failed");
			Ret = -EFAULT;
		}
#endif
		break;
	case ISP_QUERY_BURSTQNUM:
#if 0 /* QQ, remove later*/
		spin_lock(&SpinLockRegScen);
		burstQNum = P2_Support_BurstQNum;
		spin_unlock(&SpinLockRegScen);
		/*  */
		if (copy_to_user((void *)Param, &burstQNum,
		    sizeof(unsigned int)) != 0) {
			LOG_NOTICE("copy_to_user failed");
			Ret = -EFAULT;
		}
#endif
		break;
	/*  */
	case ISP_DUMP_REG:
		/* TODO: modify this... to 6797 */
		Ret = ISP_DumpReg();
		break;
	case ISP_DEBUG_FLAG:
		if (copy_from_user(DebugFlag, (void *)Param,
		    sizeof(unsigned int)) == 0) {

			IspInfo.DebugMask = DebugFlag[0];

			/* pr_info("FBC kernel debug level = %x\n",
			 *          IspInfo.DebugMask);
			 */
		} else {
			LOG_NOTICE("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	case ISP_BUFFER_CTRL:
		Ret = ISP_Buf_CTRL_FUNC(Param);
		break;
	case ISP_REF_CNT_CTRL:
		Ret = ISP_REF_CNT_CTRL_FUNC(Param);
		break;
	case ISP_DUMP_ISR_LOG:
		if (copy_from_user(DebugFlag, (void *)Param,
		    sizeof(unsigned int)) == 0) {
			unsigned int currentPPB = m_CurrentPPB;
			unsigned int lock_key = ISP_IRQ_TYPE_AMOUNT;

			if (DebugFlag[0] >= ISP_IRQ_TYPE_AMOUNT) {
				LOG_NOTICE("unsupported module:0x%x\n",
					DebugFlag[0]);
				Ret = -EFAULT;
				break;
			}

			if (DebugFlag[0] == ISP_IRQ_TYPE_INT_CAM_B_ST)
				lock_key = ISP_IRQ_TYPE_INT_CAM_A_ST;
			else
				lock_key = DebugFlag[0];

			spin_lock_irqsave(&(IspInfo.SpinLockIrq[lock_key]),
						flags);
			m_CurrentPPB = (m_CurrentPPB + 1) % LOG_PPNUM;
			spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[lock_key]),
						flags);

			IRQ_LOG_PRINTER(DebugFlag[0], currentPPB, _LOG_INF);
			IRQ_LOG_PRINTER(DebugFlag[0], currentPPB, _LOG_ERR);

		} else {
			LOG_NOTICE("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	case ISP_VF_LOG:
		if (copy_from_user(DebugFlag, (void *)Param,
		    sizeof(unsigned int) * 2) == 0) {
			unsigned int vf;

			switch (DebugFlag[0]) {
			case 1: {
				unsigned int module = ISP_IRQ_TYPE_INT_CAM_A_ST;
				unsigned int cam_dmao = 0;
				unsigned int uni_dmao;
				unsigned int hds2_sel;

				switch (DebugFlag[1]) {
				case ISP_CAM_A_IDX:
					uni_dmao = ISP_RD32(
						CAM_UNI_REG_TOP_DMA_EN(
						  ISP_UNI_A_IDX));
					hds2_sel = ((ISP_RD32(
						CAM_UNI_REG_TOP_PATH_SEL(
						  ISP_UNI_A_IDX))) & 0x3);
					pr_info("CAM_A viewFinder is ON (SecOn:0x%x)\n",
						sec_on);

					if (sec_on) {
						cam_dmao =
						    lock_reg.CAM_REG_CTL_DMA_EN
						    [ISP_CAM_A_IDX];
					} else {
						cam_dmao =
						    ISP_RD32(
						    CAM_REG_CTL_DMA_EN(
						    ISP_CAM_A_IDX));
					}

					pr_info("CAM_A:[DMA_EN]:0x%x\n",
						cam_dmao);
					vf = ISP_RD32(CAM_REG_TG_VF_CON(
						ISP_CAM_A_IDX));
					if (vf & 0x1)
						pr_notice(
							"module_%d: vf already enabled\n",
							ISP_CAM_A_IDX);
					else
						ISP_WR32(CAM_REG_TG_VF_CON(
						    ISP_CAM_A_IDX), (vf+0x1));

					if (hds2_sel == 0) {
						pstRTBuf[
						    ISP_IRQ_TYPE_INT_CAM_A_ST]->
						    ring_buf[_eiso_].active =
						    ((uni_dmao & 0x4) ?
						    (MTRUE) : (MFALSE));
						pstRTBuf[
						    ISP_IRQ_TYPE_INT_CAM_A_ST]->
						    ring_buf[_rsso_].active =
						    ((uni_dmao & 0x8) ?
						    (MTRUE) : (MFALSE));

						pstRTBuf[
						    ISP_IRQ_TYPE_INT_CAM_B_ST]->
						    ring_buf[_eiso_].active =
						    MFALSE;
						pstRTBuf[
						    ISP_IRQ_TYPE_INT_CAM_B_ST]->
						    ring_buf[_rsso_].active =
						    MFALSE;
					}
					module = ISP_IRQ_TYPE_INT_CAM_A_ST;

					#if (TIMESTAMP_QUEUE_EN == 1)
					memset((void *)&(IspInfo.TstpQInfo[
					    ISP_IRQ_TYPE_INT_CAM_A_ST]), 0,
					    sizeof(
					      struct ISP_TIMESTPQ_INFO_STRUCT));
					g1stSwP1Done[
						ISP_IRQ_TYPE_INT_CAM_A_ST]
						= MTRUE;
					gPrevSofTimestp[
						ISP_IRQ_TYPE_INT_CAM_A_ST] = 0;
					#endif
					break;
				case ISP_CAM_B_IDX:
					uni_dmao = ISP_RD32(
						CAM_UNI_REG_B_TOP_DMA_EN(
							ISP_UNI_A_IDX));
					hds2_sel = ((ISP_RD32(
						CAM_UNI_REG_B_TOP_PATH_SEL(
							ISP_UNI_A_IDX))) & 0x3);
					pr_info("CAM_B viewFinder is ON\n");
					cam_dmao = ISP_RD32(CAM_REG_CTL_DMA_EN(
							ISP_CAM_B_IDX));
					pr_info("CAM_B:[DMA_EN]:0x%x\n",
						cam_dmao);
					vf = ISP_RD32(CAM_REG_TG_VF_CON(
						ISP_CAM_B_IDX));
					if (vf & 0x1)
						pr_notice(
							"module_%d: vf already enabled\n",
							ISP_CAM_B_IDX);
					else
						ISP_WR32(CAM_REG_TG_VF_CON(
						    ISP_CAM_B_IDX), (vf+0x1));

					if (hds2_sel == 1) {
						pstRTBuf[
						    ISP_IRQ_TYPE_INT_CAM_A_ST]->
						    ring_buf[_eiso_].active =
						    MFALSE;
						pstRTBuf[
						    ISP_IRQ_TYPE_INT_CAM_A_ST]->
						    ring_buf[_rsso_].active =
						    MFALSE;

						pstRTBuf[
						    ISP_IRQ_TYPE_INT_CAM_B_ST]->
						    ring_buf[_eiso_].active =
						    ((uni_dmao & 0x4) ?
						     (MTRUE) : (MFALSE));
						pstRTBuf[
						    ISP_IRQ_TYPE_INT_CAM_B_ST]->
						    ring_buf[_rsso_].active =
						    ((uni_dmao & 0x8) ?
						     (MTRUE) : (MFALSE));
					}

					module = ISP_IRQ_TYPE_INT_CAM_B_ST;

					#if (TIMESTAMP_QUEUE_EN == 1)
					memset((void *)&(IspInfo.TstpQInfo[
					    ISP_IRQ_TYPE_INT_CAM_B_ST]), 0,
					    sizeof(
					     struct ISP_TIMESTPQ_INFO_STRUCT));
					g1stSwP1Done[ISP_IRQ_TYPE_INT_CAM_B_ST]
						= MTRUE;
					gPrevSofTimestp[
						ISP_IRQ_TYPE_INT_CAM_B_ST] = 0;
					#endif

					break;
				default:
					LOG_NOTICE("unsupported module:0x%x\n",
						DebugFlag[1]);
					break;
				}
				pstRTBuf[module]->ring_buf[_imgo_].active =
				    ((cam_dmao & 0x1) ? (MTRUE) : (MFALSE));
				pstRTBuf[module]->ring_buf[_rrzo_].active =
				    ((cam_dmao & 0x4) ? (MTRUE) : (MFALSE));
				pstRTBuf[module]->ring_buf[_lcso_].active =
				    ((cam_dmao & 0x10) ? (MTRUE) : (MFALSE));
				/*reset 1st sof flag when vf is enabled*/
				g1stSof[module] = MTRUE;
				break;
			}
			case 0: {
				switch (DebugFlag[1]) {
				case ISP_CAM_A_IDX:
					pr_info("CAM_A viewFinder is OFF\n");
					vf = ISP_RD32(CAM_REG_TG_VF_CON(
							ISP_CAM_A_IDX));
					if (vf & 0x1) {
						ISP_WR32(CAM_REG_TG_VF_CON(
							ISP_CAM_A_IDX), (vf-0x1));
					} else {
						pr_notice(
							"module_%d: vf already disabled\n",
							ISP_CAM_A_IDX);
					}
					break;
				case ISP_CAM_B_IDX:
					pr_info("CAM_B viewFinder is OFF\n");
					vf = ISP_RD32(CAM_REG_TG_VF_CON(
							ISP_CAM_B_IDX));
					if (vf & 0x1) {
						ISP_WR32(CAM_REG_TG_VF_CON(
						    ISP_CAM_B_IDX), (vf-0x1));
					} else {
						pr_notice(
							"module_%d: vf already disabled\n",
							ISP_CAM_B_IDX);
					}
					break;
				default:
					LOG_NOTICE("unsupported module:0x%x\n",
						DebugFlag[1]);
					break;
				}
				break;
			}
			/* CAMSV */
			case 11: {
				unsigned int module =
						    ISP_IRQ_TYPE_INT_CAMSV_0_ST;
				unsigned int cam_dmao = 0;

				switch (DebugFlag[1]) {
				case ISP_CAMSV0_IDX:
					pr_info("CAMSV_0 viewFinder is ON\n");
					cam_dmao = (ISP_RD32(CAMSV_REG_MODULE_EN
						(ISP_CAMSV0_IDX)) & 0x10);
					pr_info("CAMSV_0:[DMA_EN]:0x%x\n",
						cam_dmao);
					vf = ISP_RD32(CAMSV_REG_TG_VF_CON(
						ISP_CAMSV0_IDX));
					if (vf & 0x1) {
						pr_notice(
							"module_%d: vf already enabled\n",
							ISP_CAMSV0_IDX);
					} else {
						ISP_WR32(CAMSV_REG_TG_VF_CON(
						    ISP_CAMSV0_IDX), (vf+0x1));
					}
					module = ISP_IRQ_TYPE_INT_CAMSV_0_ST;
					break;
				case ISP_CAMSV1_IDX:
					pr_info("CAMSV_1 viewFinder is ON\n");
					cam_dmao = (ISP_RD32(CAMSV_REG_MODULE_EN
						(ISP_CAMSV1_IDX)) & 0x10);
					pr_info("CAMSV_1:[DMA_EN]:0x%x\n",
						cam_dmao);
					vf = ISP_RD32(CAMSV_REG_TG_VF_CON(
						ISP_CAMSV1_IDX));
					if (vf & 0x1) {
						pr_notice(
							"module_%d: vf already enabled\n",
							ISP_CAMSV1_IDX);
					} else {
						ISP_WR32(CAMSV_REG_TG_VF_CON(
						    ISP_CAMSV1_IDX), (vf+0x1));
					}
					module = ISP_IRQ_TYPE_INT_CAMSV_0_ST;
					break;
				case ISP_CAMSV2_IDX:
					pr_info("CAMSV_2 viewFinder is ON\n");
					cam_dmao = (ISP_RD32(CAMSV_REG_MODULE_EN
						(ISP_CAMSV2_IDX)) & 0x10);
					pr_info("CAMSV_2:[DMA_EN]:0x%x\n",
						cam_dmao);
					vf = ISP_RD32(CAMSV_REG_TG_VF_CON(
						ISP_CAMSV2_IDX));
					if (vf & 0x1) {
						pr_notice(
							"module_%d: vf already enabled\n",
							ISP_CAMSV2_IDX);
					} else {
						ISP_WR32(CAMSV_REG_TG_VF_CON(
						    ISP_CAMSV2_IDX), (vf+0x1));
					}
					module = ISP_IRQ_TYPE_INT_CAMSV_2_ST;
					break;
				case ISP_CAMSV3_IDX:
					pr_info("CAMSV_3 viewFinder is ON\n");
					cam_dmao = (ISP_RD32(CAMSV_REG_MODULE_EN
						(ISP_CAMSV3_IDX)) & 0x10);
					pr_info("CAMSV_3:[DMA_EN]:0x%x\n",
						cam_dmao);
					vf = ISP_RD32(CAMSV_REG_TG_VF_CON(
							ISP_CAMSV3_IDX));
					if (vf & 0x1) {
						pr_notice(
							"module_%d: vf already enabled\n",
							ISP_CAMSV3_IDX);
					} else {
						ISP_WR32(CAMSV_REG_TG_VF_CON(
						    ISP_CAMSV3_IDX), (vf+0x1));
					}
					module = ISP_IRQ_TYPE_INT_CAMSV_3_ST;
					break;
				case ISP_CAMSV4_IDX:
					pr_info("CAMSV_4 viewFinder is ON\n");
					cam_dmao = (ISP_RD32(CAMSV_REG_MODULE_EN
						(ISP_CAMSV4_IDX)) & 0x10);
					pr_info("CAMSV_4:[DMA_EN]:0x%x\n",
						cam_dmao);
					vf = ISP_RD32(CAMSV_REG_TG_VF_CON(
							ISP_CAMSV4_IDX));
					if (vf & 0x1) {
						pr_notice(
							"module_%d: vf already enabled\n",
							ISP_CAMSV4_IDX);
					} else {
						ISP_WR32(CAMSV_REG_TG_VF_CON(
						    ISP_CAMSV4_IDX), (vf+0x1));
					}
					module = ISP_IRQ_TYPE_INT_CAMSV_4_ST;
					break;
				case ISP_CAMSV5_IDX:
					pr_info("CAMSV_5 viewFinder is ON\n");
					cam_dmao = (ISP_RD32(CAMSV_REG_MODULE_EN
						(ISP_CAMSV5_IDX)) & 0x10);
					pr_info("CAMSV_5:[DMA_EN]:0x%x\n",
						cam_dmao);
					vf = ISP_RD32(CAMSV_REG_TG_VF_CON(
							ISP_CAMSV5_IDX));
					if (vf & 0x1) {
						pr_notice(
							"module_%d: vf already enabled\n",
							ISP_CAMSV5_IDX);
					} else {
						ISP_WR32(CAMSV_REG_TG_VF_CON(
						    ISP_CAMSV5_IDX), (vf+0x1));
					}
					module = ISP_IRQ_TYPE_INT_CAMSV_5_ST;
					break;
				default:
					LOG_NOTICE("unsupported module:0x%x\n",
						DebugFlag[1]);
					break;
				}
				pstRTBuf[module]->ring_buf[_camsv_imgo_].active
					= ((cam_dmao & 0x10) ?
					  (MTRUE) : (MFALSE));
				/*reset 1st sof flag when vf is enabled*/
				g1stSof[module] = MTRUE;
				break;
			}
			case 10: {
				switch (DebugFlag[1]) {
				case ISP_CAMSV0_IDX:
					pr_info("CAMSV_0 viewFinder is OFF\n");
					vf = ISP_RD32(CAMSV_REG_TG_VF_CON(
							ISP_CAMSV0_IDX));
					if (vf & 0x1) {
						ISP_WR32(CAMSV_REG_TG_VF_CON(
						    ISP_CAMSV0_IDX), (vf-0x1));
					} else {
						pr_notice(
							"module_%d: vf already disalbed\n",
							ISP_CAMSV0_IDX);
					}
					break;
				case ISP_CAMSV1_IDX:
					pr_info("CAMSV_1 viewFinder is OFF\n");
					vf = ISP_RD32(CAMSV_REG_TG_VF_CON(
							ISP_CAMSV1_IDX));
					if (vf & 0x1) {
						ISP_WR32(CAMSV_REG_TG_VF_CON(
						    ISP_CAMSV1_IDX), (vf-0x1));
					} else {
						pr_notice(
							"module_%d: vf already disalbed\n",
							ISP_CAMSV1_IDX);
					}
					break;
				case ISP_CAMSV2_IDX:
					pr_info("CAMSV_2 viewFinder is OFF\n");
					vf = ISP_RD32(CAMSV_REG_TG_VF_CON(
							ISP_CAMSV2_IDX));
					if (vf & 0x1) {
						ISP_WR32(CAMSV_REG_TG_VF_CON(
						    ISP_CAMSV2_IDX), (vf-0x1));
					} else {
						pr_notice(
							"module_%d: vf already disalbed\n",
							ISP_CAMSV2_IDX);
					}
					break;
				case ISP_CAMSV3_IDX:
					pr_info("CAMSV_3 viewFinder is OFF\n");
					vf = ISP_RD32(CAMSV_REG_TG_VF_CON(
							ISP_CAMSV3_IDX));
					if (vf & 0x1) {
						ISP_WR32(CAMSV_REG_TG_VF_CON(
						    ISP_CAMSV3_IDX), (vf-0x1));
					} else {
						pr_notice(
							"module_%d: vf already disalbed\n",
							ISP_CAMSV3_IDX);
					}
					break;
				case ISP_CAMSV4_IDX:
					pr_info("CAMSV_4 viewFinder is OFF\n");
					vf = ISP_RD32(CAMSV_REG_TG_VF_CON(
							ISP_CAMSV4_IDX));
					if (vf & 0x1) {
						ISP_WR32(CAMSV_REG_TG_VF_CON(
						    ISP_CAMSV4_IDX), (vf-0x1));
					} else {
						pr_notice(
							"module_%d: vf already disalbed\n",
							ISP_CAMSV4_IDX);
					}
					break;
				case ISP_CAMSV5_IDX:
					pr_info("CAMSV_5 viewFinder is OFF\n");
					vf = ISP_RD32(CAMSV_REG_TG_VF_CON(
							ISP_CAMSV5_IDX));
					if (vf & 0x1) {
						ISP_WR32(CAMSV_REG_TG_VF_CON(
						    ISP_CAMSV5_IDX), (vf-0x1));
					} else {
						pr_notice(
							"module_%d: vf already disalbed\n",
							ISP_CAMSV5_IDX);
					}
					break;
				default:
					LOG_NOTICE("unsupported module:0x%x\n",
						DebugFlag[1]);
					break;
				}
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
			struct S_START_T *pTstp = NULL;

			#if (TIMESTAMP_QUEUE_EN == 1)
			struct S_START_T tstp;
			unsigned int dma_id = DebugFlag[1];

			if (_cam_max_ == DebugFlag[1]) {
				/* only for wait timestamp to ready */
				Ret = ISP_WaitTimestampReady(DebugFlag[0],
						DebugFlag[2]);
				break;
			}

			switch (DebugFlag[0]) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				pTstp = &tstp;

				if (ISP_PopBufTimestamp(DebugFlag[0], dma_id,
				    pTstp) != 0)
					LOG_NOTICE(
						"Get Buf sof timestamp fail");

				break;
			case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
				pTstp = &gSTime[DebugFlag[0]];
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
			    DebugFlag[0] > ISP_IRQ_TYPE_INT_UNI_A_ST) {
				Ret = -EFAULT;
				break;
			}
			pTstp = &gSTime[DebugFlag[0]];
			#endif

			switch (DebugFlag[0]) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
			case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
				if (copy_to_user((void *)Param, pTstp,
				    sizeof(struct S_START_T)) != 0) {
					LOG_NOTICE("copy_to_user failed");
					Ret = -EFAULT;
				}
				break;
			default:
				LOG_NOTICE("unsupported module:0x%x\n",
					DebugFlag[0]);
				Ret = -EFAULT;
				break;
			}
		}
		break;

	case ISP_TRANSFOR_CCU_REG:
		{
			uint32_t hwTickCnt_ccu_direct[2];

			unsigned int globaltime[2];
			unsigned long long reg_trans_Time;
			unsigned long long sum;

			ccu_get_timestamp(&hwTickCnt_ccu_direct[0],
				&hwTickCnt_ccu_direct[1]);

			pr_debug("hwTickCnt_ccu_direct[0]:%u,hwTickCnt_ccu_direct[1]:%u",
					hwTickCnt_ccu_direct[0],
					hwTickCnt_ccu_direct[1]);

			sum =
			(unsigned long long)hwTickCnt_ccu_direct[0] +
			((unsigned long long)hwTickCnt_ccu_direct[1]<<32);

			pr_debug("sum of hwTickCnt:%llu", sum);

			if (sum == 0) {
				globaltime[0] = 0;
				globaltime[1] = 0;
			} else {
				reg_trans_Time =
					archcounter_timesync_to_boot(sum);
				globaltime[1] =
					do_div(reg_trans_Time, 1000000000);
				globaltime[1] =
					globaltime[1]/1000;
				globaltime[0] =
					reg_trans_Time;
			}
			if (copy_to_user((void *)Param, globaltime,
				sizeof(unsigned int)*2) != 0) {
				Ret = -EFAULT;
			}
		}
		break;

#ifdef EP_NO_PMQOS
	case ISP_DFS_CTRL:
		/* Fall through */
	case ISP_DFS_UPDATE:
		/* Fall through */
	case ISP_GET_CUR_ISP_CLOCK:
		/* Fall through */
	case ISP_SET_PM_QOS_INFO:
		/* Fall through */
	case ISP_SET_PM_QOS:
		break;
	case ISP_GET_SUPPORTED_ISP_CLOCKS:

		/* Set a default clk for EP */
		ispclks.clklevelcnt = 1;
		ispclks.clklevel[lv] = 546;
		pr_info("Default DFS Clk level:%d for EP",
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
						mtk_pm_qos_add_request(
						  &isp_qos, PM_QOS_CAM_FREQ, 0);
						pr_debug(
						  "CAMSYS PMQoS turn on");
					}
				} else {
					if (--camsys_qos == 0) {
						mtk_pm_qos_remove_request(&isp_qos);
						pr_debug(
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
				mtk_pm_qos_update_request(&isp_qos, dfs_update);
				target_clk = dfs_update;
				pr_debug("Set clock level:%d", dfs_update);
			} else {
				LOG_NOTICE(
					"ISP_DFS_UPDATE copy_from_user failed");
				Ret = -EFAULT;
			}
		}
		break;
	case ISP_GET_SUPPORTED_ISP_CLOCKS:
		{
			int result = 0;
			u64 freq_steps[ISP_CLK_LEVEL_CNT] = {0};

			/* Call mmdvfs_qos_get_freq_steps
			 * to get supported frequency
			 */
			result = mmdvfs_qos_get_freq_steps(PM_QOS_CAM_FREQ,
				    freq_steps, (u32 *)&ispclks.clklevelcnt);

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
				pr_debug("DFS Clk level[%d]:%d",
					lv, ispclks.clklevel[lv]);
			}

			target_clk = ispclks.clklevel[ispclks.clklevelcnt - 1];

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

			getclk.curClk =
				(u32)mmdvfs_qos_get_freq(PM_QOS_CAM_FREQ);
			getclk.targetClk = target_clk;

			pr_debug("Get current clock level:%d, target clock:%d",
				getclk.curClk, getclk.targetClk);

			if (copy_to_user((void *)Param, &getclk,
			    sizeof(struct ISP_GET_CLK_INFO)) != 0) {
				LOG_NOTICE("copy_to_user failed");
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
				      ISP_IRQ_TYPE_INT_CAM_B_ST) {
					LOG_NOTICE("HW_module error:%d",
						pm_qos_info.module);
					Ret = -EFAULT;
					break;
				}
				G_PM_QOS[pm_qos_info.module].bw_sum =
							pm_qos_info.bw_value;
				G_PM_QOS[pm_qos_info.module].fps =
							pm_qos_info.fps/10;
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
				static int bw_request[
						   ISP_IRQ_TYPE_INT_CAM_B_ST+1];

				if (DebugFlag[1] < ISP_IRQ_TYPE_INT_CAM_A_ST ||
				    DebugFlag[1] > ISP_IRQ_TYPE_INT_CAM_B_ST) {
					LOG_NOTICE("HW_module error:%d",
						DebugFlag[1]);
					Ret = -EFAULT;
					break;
				}
				if (DebugFlag[0] == 1) {
					if (++bw_request[DebugFlag[1]] == 1) {
						mtk_pm_qos_add_request(
						  &camsys_qos_request[
							DebugFlag[1]],
						  PM_QOS_MM_MEMORY_BANDWIDTH,
						  PM_QOS_DEFAULT_VALUE);
					}
					Ret = ISP_SetPMQOS(DebugFlag[0],
							   DebugFlag[1]);
				} else {
					if (bw_request[DebugFlag[1]] == 0)
						break;
					Ret = ISP_SetPMQOS(DebugFlag[0],
							   DebugFlag[1]);
					mtk_pm_qos_remove_request(
					    &camsys_qos_request[DebugFlag[1]]);
					bw_request[DebugFlag[1]] = 0;
				}
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
			switch (DebugFlag[0]) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				DebugFlag[1] = Vsync_cnt[0];
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				DebugFlag[1] = Vsync_cnt[1];
				break;
			default:
				LOG_NOTICE("err TG(0x%x)\n", DebugFlag[0]);
				Ret = -EFAULT;
				break;
			}
		}
		if (copy_to_user((void *)Param, &DebugFlag[1],
		    sizeof(unsigned int)) != 0) {
			LOG_NOTICE("copy to user fail");
			Ret = -EFAULT;
		}
		break;
	case ISP_RESET_VSYNC_CNT:
		Vsync_cnt[0] = Vsync_cnt[1] = 0;
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
					pr_info(
						"ion_import: already exist: dev(%d)dma(%d)i(%d)fd(%d)Hnd(0x%pK)\n",
						IonNode.devNode,
						IonNode.dmaPort, i,
						IonNode.memID,
						ptbl->pIonHnd[jump + i]);
				}
				/* User might allocate a big memory
				 * and divid it into many buffers,
				 * the ion FD of these buffers is the same,
				 * so we must check there has no users take
				 * this memory
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
			/* User might allocate a big memory
			 * and divid it into many buffers,
			 * the ion FD of these buffers is the same,
			 * so we must check there has no users take this memory
			 */
			for (i = 0; i < _ion_keep_max_; i++) {
				if (ptbl->pIonFd[jump + i] == 0) {
					ptbl->pIonFd[jump + i] = IonNode.memID;
					ptbl->pIonHnd[jump + i] = handle;
					ptbl->pIonCt[jump + i]++;//

					if (IspInfo.DebugMask &
					    ISP_DBG_ION_CTRL) {
						pr_info(
							"ion_import: dev(%d)dma(%d)i(%d)fd(%d)Hnd(0x%pK)\n",
							IonNode.devNode,
							IonNode.dmaPort, i,
							IonNode.memID, handle);
					}
					break;
				}
			}
			spin_unlock(&(ptbl->pLock[IonNode.dmaPort]));
			/* */
			if (i == _ion_keep_max_) {
				LOG_NOTICE(
					"ion_import: dma(%d)no empty space in list(%d_%d)\n",
					IonNode.dmaPort, IonNode.memID,
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
				LOG_NOTICE("ion_free: dmaport error:%d(0~%d)\n",
					IonNode.dmaPort, _dma_max_wr_);
				Ret = -EFAULT;
				break;
			}
			jump = IonNode.dmaPort*_ion_keep_max_;
			if (IonNode.memID <= 0) {
				LOG_NOTICE("ion_free: invalid ion fd(%d)\n",
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
					"ion_free: can't find ion: dev(%d)dma(%d)fd(%d) in list\n",
					IonNode.devNode, IonNode.dmaPort,
					IonNode.memID);
				Ret = -EFAULT;

				break;
			}
			/* User might allocate a big memory
			 * and divid it into many buffers,
			 * the ion FD of these buffers is the same,
			 * so we must check there has no users take this memory.
			 */
			if (--ptbl->pIonCt[jump + i] > 0) {
				spin_unlock(&(ptbl->pLock[IonNode.dmaPort]));
				if (IspInfo.DebugMask & ISP_DBG_ION_CTRL) {
					pr_info(
						"ion_free: user ct(%d): dev(%d)dma(%d)i(%d)fd(%d)\n",
						ptbl->pIonCt[jump + i],
						IonNode.devNode,
						IonNode.dmaPort, i,
						IonNode.memID);
				}
				break;
			} else if (ptbl->pIonCt[jump + i] < 0) {
				spin_unlock(&(ptbl->pLock[IonNode.dmaPort]));
				LOG_NOTICE(
					"ion_free: free more than import (%d):dev(%d)dma(%d)i(%d)fd(%d)\n",
						ptbl->pIonCt[jump + i],
						IonNode.devNode,
						IonNode.dmaPort, i,
						IonNode.memID);
				Ret = -EFAULT;
				break;
			}

			if (IspInfo.DebugMask & ISP_DBG_ION_CTRL) {
				pr_info(
					"ion_free: dev(%d)dma(%d)i(%d)fd(%d)Hnd(0x%pK)Ct(%d)\n",
					IonNode.devNode, IonNode.dmaPort, i,
					IonNode.memID,
					ptbl->pIonHnd[jump + i],
					ptbl->pIonCt[jump + i]);
			}

			p_IonHnd = ptbl->pIonHnd[jump + i];
			ptbl->pIonFd[jump + i] = 0;
			ptbl->pIonHnd[jump + i] = NULL;
			spin_unlock(&(ptbl->pLock[IonNode.dmaPort]));
			/* can't in spin_lock*/
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
	case ISP_CQ_SW_PATCH: {
			static unsigned int Addr[2] = {0, 0};

			if (copy_from_user(DebugFlag, (void *)Param,
			    sizeof(unsigned int)*2) == 0) {
				switch (DebugFlag[0]) {
				case ISP_IRQ_TYPE_INT_CAM_A_ST:
					Addr[0] = DebugFlag[1];
					break;
				case ISP_IRQ_TYPE_INT_CAM_B_ST:
					Addr[1] = DebugFlag[1];
					break;
				default:
					LOG_NOTICE("unsupported module:0x%x\n",
						DebugFlag[0]);
					break;
				}
			}
			if ((Addr[0] != 0) && (Addr[1] != 0)) {
				unsigned long flags;

				spin_lock_irqsave(&IspInfo.SpinLockIrq
					[ISP_IRQ_TYPE_INT_CAM_A_ST], flags);
				ISP_WR32(CAM_REG_CTL_CD_DONE_SEL
					(ISP_CAM_A_IDX), 0x1);
				ISP_WR32(CAM_REG_CTL_UNI_DONE_SEL
					(ISP_CAM_A_IDX), 0x1);
				ISP_WR32(CAM_REG_CTL_UNI_B_DONE_SEL
					(ISP_CAM_A_IDX), 0x1);
				ISP_WR32(CAM_REG_CQ_THR0_BASEADDR
					(ISP_CAM_B_IDX), Addr[1]);
				ISP_WR32(CAM_REG_CQ_THR0_BASEADDR
					(ISP_CAM_A_IDX), Addr[0]);
				ISP_WR32(CAM_REG_CTL_UNI_DONE_SEL
					(ISP_CAM_A_IDX), 0x0);
				ISP_WR32(CAM_REG_CTL_UNI_B_DONE_SEL
					(ISP_CAM_A_IDX), 0x0);
				Addr[0] = Addr[1] = 0;
				spin_unlock_irqrestore(&IspInfo.SpinLockIrq
					[ISP_IRQ_TYPE_INT_CAM_A_ST], flags);
			}
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
		case 2:
		case 3:
		case 5:
		case 6:
			break;
		default:
			LOG_NOTICE(
				"Wrong SMI_LARB port=%d\n", larbInfo.LarbNum);
			Ret = -EFAULT;
			goto EXIT;
		}

		if ((SMI_LARB_BASE[larbInfo.LarbNum] == NULL) ||
		    (larbInfo.regOffset >= 0x1000)) {
			LOG_NOTICE(
				"Wrong SMI_LARB port=%d base addr=%pK offset=0x%x\n",
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
	case ISP_GET_DUMP_INFO: {
		if (copy_to_user((void *)Param, &g_dumpInfo,
		    sizeof(struct ISP_GET_DUMP_INFO_STRUCT)) != 0) {
			LOG_NOTICE("ISP_GET_DUMP_INFO copy to user fail");
			Ret = -EFAULT;
		}
		break;
	}
	case ISP_DUMP_BUFFER: {
		if (copy_from_user(&DumpBufStruct, (void *)Param,
		    sizeof(struct ISP_DUMP_BUFFER_STRUCT)) == 0) {
			/* 2nd layer behavoir of copy from user is implemented
			 * in ISP_DumpTuningBuffer(...)
			 */
			Ret = ISP_DumpBuffer(&DumpBufStruct);
		} else {
			LOG_NOTICE(
			    "ISP_DUMP_TUNING_BUFFER copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	}
	case ISP_SET_MEM_INFO: {
		if (copy_from_user(&MemInfoStruct, (void *)Param,
		    sizeof(struct ISP_MEM_INFO_STRUCT)) == 0) {
			/* 2nd layer behavoir of copy from user is implemented
			 * in ISP_SetMemInfo(...)
			 */
			Ret = ISP_SetMemInfo(&MemInfoStruct);
		} else {
			LOG_NOTICE("ISP_SET_MEM_INFO copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	}
	case ISP_SET_SEC_DAPC_REG:
		if (copy_from_user(
		    Dapc_Reg,
		    (void *)Param,
		    sizeof(unsigned int) * 6) != 0) {
			LOG_NOTICE("get DAPC_REG from user fail\n");
			Ret = -EFAULT;
		} else {
			if (Dapc_Reg[0] < ISP_CAMSYS_CONFIG_IDX ||
				Dapc_Reg[0] >= ISP_DEV_NODE_NUM) {
				LOG_NOTICE("module index(0x%x) error\n",
					Dapc_Reg[0]);
				Ret = -EFAULT;
				break;
			}
			if (Dapc_Reg[1] == MTRUE) {
				sec_on = Dapc_Reg[1];
				lock_reg.CAM_REG_CTL_EN[Dapc_Reg[0]] =
								Dapc_Reg[2];
				lock_reg.CAM_REG_CTL_DMA_EN[Dapc_Reg[0]] =
								Dapc_Reg[3];
				lock_reg.CAM_REG_CTL_SEL[Dapc_Reg[0]] =
								Dapc_Reg[4];
				lock_reg.CAM_REG_CTL_EN2[Dapc_Reg[0]] =
								Dapc_Reg[5];

				pr_info(
				"[DAPC]EN:0x%x DMA_EN:0x%x SEL:0x%x\n",
				    lock_reg.CAM_REG_CTL_EN[Dapc_Reg[0]],
				    lock_reg.CAM_REG_CTL_DMA_EN[Dapc_Reg[0]],
				    lock_reg.CAM_REG_CTL_SEL[Dapc_Reg[0]]);
			} else {
				LOG_NOTICE("get wrong sec status (0x%x)\n",
					Dapc_Reg[1]);
				Ret = -EFAULT;
			}
		}
		break;
	case ISP_SET_SEC_ENABLE:
		if (copy_from_user(&sec_on, (void *)Param,
				sizeof(unsigned int)) != 0) {
			LOG_NOTICE("ISP_SET_SEC_ENABLE from user fail\n");
			Ret = -EFAULT;
			break;
		}
		LOG_NOTICE("ISP_SET_SEC_ENABLE sec_on = %d\n", sec_on);
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
			Cmd, pUserInfo->Pid, current->comm, current->pid,
			current->tgid);
	/*  */
	return Ret;
}

#ifdef CONFIG_COMPAT

/******************************************************************************
 *
 *****************************************************************************/
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

static int compat_get_isp_dump_buffer(
	struct compat_ISP_DUMP_BUFFER_STRUCT __user *data32,
	struct ISP_DUMP_BUFFER_STRUCT __user *data)
{
	compat_uint_t count;
	compat_uint_t cmd;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(cmd, &data32->DumpCmd);
	err |= put_user(cmd, &data->DumpCmd);
	err |= get_user(uptr, &data32->pBuffer);
	err |= put_user(compat_ptr(uptr), &data->pBuffer);
	err |= get_user(count, &data32->BytesofBufferSize);
	err |= put_user(count, &data->BytesofBufferSize);
	return err;
}

static int compat_get_isp_mem_info(
	struct compat_ISP_MEM_INFO_STRUCT __user *data32,
	struct ISP_MEM_INFO_STRUCT __user *data)
{
	compat_uint_t cmd;
	compat_uint_t mempa;
	compat_uptr_t uptr;
	compat_uint_t size;
	int err = 0;

	err = get_user(cmd, &data32->MemInfoCmd);
	err |= put_user(cmd, &data->MemInfoCmd);
	err |= get_user(mempa, &data32->MemPa);
	err |= put_user(mempa, &data->MemPa);
	err |= get_user(uptr, &data32->MemVa);
	err |= put_user(compat_ptr(uptr), &data->MemVa);
	err |= get_user(size, &data32->MemSizeDiff);
	err |= put_user(size, &data->MemSizeDiff);
	return err;
}

#if 0
static int compat_get_isp_register_userkey_struct_data(
	compat_ISP_REGISTER_USERKEY_STRUCT __user *data32,
	struct ISP_REGISTER_USERKEY_STRUCT __user *data)
{
	compat_uint_t tmp;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(tmp, &data32->userKey);
	err |= put_user(tmp, &data->userKey);
	err |= get_user(uptr, &data32->userName);
	/* err |= put_user(compat_ptr(uptr), &data->userName); */

	return err;
}

static int compat_put_isp_register_userkey_struct_data(
	compat_ISP_REGISTER_USERKEY_STRUCT __user *data32,
	struct ISP_REGISTER_USERKEY_STRUCT __user *data)
{
	compat_uint_t tmp;
	/*      compat_uptr_t uptr;*/
	int err = 0;

	err = get_user(tmp, &data->userKey);
	err |= put_user(tmp, &data32->userKey);
	/* Assume data pointer is unchanged. */
	/* err |= get_user(uptr, &data->userName); */
	/* err |= put_user(uptr, &data32->userName); */

	return err;
}
#endif

static long ISP_ioctl_compat(struct file *filp, unsigned int cmd,
			unsigned long arg)
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
			pr_info("compat_get_isp_read_register_data error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, ISP_READ_REGISTER,
			(unsigned long)data);
		err = compat_put_isp_read_register_data(data32, data);
		if (err) {
			pr_info("compat_put_isp_read_register_data error!!!\n");
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
			pr_info("COMPAT_ISP_WRITE_REGISTER error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, ISP_WRITE_REGISTER,
			(unsigned long)data);
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
			pr_info("compat_get_isp_buf_ctrl error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, ISP_BUFFER_CTRL,
			(unsigned long)data);
		err = compat_put_isp_buf_ctrl_struct_data(data32, data);

		if (err) {
			pr_info("compat_put_isp_buf_ctrl error!!!\n");
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
			pr_info("compat_get_isp_ref_cnt_ctrl error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, ISP_REF_CNT_CTRL,
			(unsigned long)data);
		err = compat_put_isp_ref_cnt_ctrl_struct_data(data32, data);
		if (err) {
			pr_info("compat_put_isp_ref_cnt_ctrl_ error!!!\n");
			return err;
		}
		return ret;
	}
#if 0
	case COMPAT_ISP_REGISTER_IRQ_USER_KEY: {
		compat_ISP_REGISTER_USERKEY_STRUCT __user *data32;
		struct ISP_REGISTER_USERKEY_STRUCT __user *data;

		int err = 0;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_register_userkey_struct_data(data32, data);
		if (err) {
			pr_info("compat_get_isp_register_userkey error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp,
				ISP_REGISTER_IRQ_USER_KEY, (unsigned long)data);
		err = compat_put_isp_register_userkey_struct_data(data32, data);
		if (err) {
			pr_info("compat_put_isp_register_userkey error!!!\n");
			return err;
		}
		return ret;
	}
#endif
	case COMPAT_ISP_DEBUG_FLAG: {
		/* compat_ptr(arg) will convert the arg */
		ret = filp->f_op->unlocked_ioctl(filp, ISP_DEBUG_FLAG,
			(unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_GET_INT_ERR: {
		/* compat_ptr(arg) will convert the arg */
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_GET_INT_ERR,
					   (unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_GET_DMA_ERR: {
		/* compat_ptr(arg) will convert the arg */
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_GET_DMA_ERR,
					   (unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_WAKELOCK_CTRL: {
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_WAKELOCK_CTRL,
					   (unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_GET_DROP_FRAME: {
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_GET_DROP_FRAME,
					   (unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_GET_CUR_SOF: {
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_GET_CUR_SOF,
					   (unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_RESET_BY_HWMODULE: {
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_RESET_BY_HWMODULE,
					   (unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_DUMP_ISR_LOG: {
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_DUMP_ISR_LOG,
					   (unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_VF_LOG: {
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_VF_LOG,
					   (unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_GET_START_TIME: {
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_GET_START_TIME,
					   (unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_DUMP_BUFFER: {
		struct compat_ISP_DUMP_BUFFER_STRUCT __user *data32;
		struct ISP_DUMP_BUFFER_STRUCT __user *data;

		int err = 0;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_dump_buffer(data32, data);
		if (err) {
			pr_info("COMPAT_ISP_DUMP_BUFFER error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, ISP_DUMP_BUFFER,
			(unsigned long)data);
		return ret;
	}
	case COMPAT_ISP_SET_MEM_INFO: {
		struct compat_ISP_MEM_INFO_STRUCT __user *data32;
		struct ISP_MEM_INFO_STRUCT __user *data;
		int err = 0;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;
		err = compat_get_isp_mem_info(data32, data);
		if (err) {
			pr_info("COMPAT_ISP_SET_MEM_INFO error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, ISP_SET_MEM_INFO,
			(unsigned long)data);
		return ret;
	}
	case ISP_GET_DUMP_INFO:
	case ISP_WAIT_IRQ:
	case ISP_CLEAR_IRQ: /* structure (no pointer) */
	case ISP_REGISTER_IRQ_USER_KEY:
	case ISP_MARK_IRQ_REQUEST:
	case ISP_GET_MARK2QUERY_TIME:
	case ISP_FLUSH_IRQ_REQUEST:
	case ISP_P2_BUFQUE_CTRL:/* structure (no pointer) */
	case ISP_UPDATE_REGSCEN:
	case ISP_QUERY_REGSCEN:
	case ISP_UPDATE_BURSTQNUM:
	case ISP_QUERY_BURSTQNUM:
	case ISP_DUMP_REG:
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
	case ISP_SET_PM_QOS_INFO:
	case ISP_SET_PM_QOS:
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
 *****************************************************************************/
static signed int ISP_open(
	struct inode *pInode,
	struct file *pFile)
{
	signed int Ret = 0;
	unsigned int i, j;
	int q = 0, p = 0;
	struct ISP_USER_INFO_STRUCT *pUserInfo;

	pr_info("- E. UserCount: %d.\n", IspInfo.UserCount);

	mutex_lock(&gDipMutex);  /* Protect the Multi Process */

	/*  */
	spin_lock(&(IspInfo.SpinLockIspRef));

	pFile->private_data = NULL;
	pFile->private_data = kmalloc(sizeof(struct ISP_USER_INFO_STRUCT),
		GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		/* //mark for checkpatch, unnecessart msg
		 *pr_notice(
		 *"ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)\n",
		 *current->comm, current->pid, current->tgid);
		 */
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
		pr_info(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist\n",
			IspInfo.UserCount, current->comm, current->pid,
			current->tgid);
		goto EXIT;
	} else {
		IspInfo.UserCount++;
		spin_unlock(&(IspInfo.SpinLockIspRef));

		/* kernel log limit to (current+150) lines per second */
	#if (_K_LOG_ADJUST == 1)
		pr_detect_count = get_detect_count();
		i = pr_detect_count + 150;
		set_detect_count(i);

		pr_info(
			"Curr UserCount(%d),(process, pid, tgid)=(%s, %d, %d), log_limit_line(%d), first user\n",
			IspInfo.UserCount, current->comm, current->pid,
			current->tgid, i);
	#else
		pr_info(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), first user\n",
			IspInfo.UserCount, current->comm, current->pid,
			current->tgid);
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
	/*  */
	spin_lock(&(SpinLock_P2FrameList));
	for (q = 0; q < ISP_P2_BUFQUE_PROPERTY_NUM; q++) {
		for (i = 0; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
			P2_FrameUnit_List[q][i].processID = 0x0;
			P2_FrameUnit_List[q][i].callerID = 0x0;
			P2_FrameUnit_List[q][i].cqMask =  0x0;
			P2_FrameUnit_List[q][i].bufSts = ISP_P2_BUF_STATE_NONE;
		}
		P2_FrameUnit_List_Idx[q].start = 0;
		P2_FrameUnit_List_Idx[q].curr = 0;
		P2_FrameUnit_List_Idx[q].end =  -1;
		/*  */
		for (i = 0; i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
			P2_FramePackage_List[q][i].processID = 0x0;
			P2_FramePackage_List[q][i].callerID = 0x0;
			P2_FramePackage_List[q][i].dupCQIdx =  -1;
			P2_FramePackage_List[q][i].frameNum = 0;
			P2_FramePackage_List[q][i].dequedNum = 0;
		}
		P2_FramePack_List_Idx[q].start = 0;
		P2_FramePack_List_Idx[q].curr = 0;
		P2_FramePack_List_Idx[q].end =  -1;
	}
	spin_unlock(&(SpinLock_P2FrameList));

	/*  */
	spin_lock((spinlock_t *)(&SpinLockRegScen));
	g_regScen = 0xa5a5a5a5;
	spin_unlock((spinlock_t *)(&SpinLockRegScen));
	/*  */
	IspInfo.BufInfo.Read.pData = kmalloc(ISP_BUF_SIZE, GFP_ATOMIC);
	IspInfo.BufInfo.Read.Size = ISP_BUF_SIZE;
	IspInfo.BufInfo.Read.Status = ISP_BUF_STATUS_EMPTY;
	if (IspInfo.BufInfo.Read.pData == NULL) {
		pr_info("ERROR: BufRead kmalloc failed\n");
		Ret = -ENOMEM;
		goto EXIT;
	}

	/* mutex_lock(&gDipMutex); */  /* Protect the Multi Process */
	g_bIonBufferAllocated = MFALSE;
#ifdef AEE_DUMP_BY_USING_ION_MEMORY
	g_isp_p2_imem_buf.handle = NULL;
	g_isp_p2_imem_buf.ion_fd = 0;
	g_isp_p2_imem_buf.va = 0;
	g_isp_p2_imem_buf.pa = 0;
	g_isp_p2_imem_buf.length = ((4*ISP_DIP_REG_SIZE)
	 + (2*MAX_ISP_TILE_TDR_HEX_NO)
	 + (2*MAX_ISP_CMDQ_BUFFER_SIZE) + (8*0x400));
	isp_p2_ion_client = NULL;
	if ((isp_p2_ion_client == NULL) && (g_ion_device))
		isp_p2_ion_client = ion_client_create(g_ion_device, "isp_p2");
	if (isp_p2_ion_client == NULL) {
		LOG_NOTICE("invalid isp_p2_ion_client client!\n");
	} else {
		if (isp_allocbuf(&g_isp_p2_imem_buf) >= 0)
			g_bIonBufferAllocated = MTRUE;
	}
	if (g_bIonBufferAllocated == MTRUE)
		g_pPhyISPBuffer = (unsigned int *)(uintptr_t)
			(g_isp_p2_imem_buf.va);

#endif
	if (g_bIonBufferAllocated == MTRUE) {
		g_pTuningBuffer = (unsigned int *)((
			(uintptr_t)g_pPhyISPBuffer) + ISP_DIP_REG_SIZE);
		g_pTpipeBuffer = (unsigned int *)((
			(uintptr_t)g_pTuningBuffer) + ISP_DIP_REG_SIZE);
		g_pVirISPBuffer = (unsigned int *)((
			(uintptr_t)g_pTpipeBuffer) + MAX_ISP_TILE_TDR_HEX_NO);
		g_pCmdqBuffer = (unsigned int *)((
			(uintptr_t)g_pVirISPBuffer) + ISP_DIP_REG_SIZE);
		/* Kernel Exception */
		g_pKWTpipeBuffer = (unsigned int *)((
			(uintptr_t)g_pCmdqBuffer) + MAX_ISP_CMDQ_BUFFER_SIZE);
		g_pKWCmdqBuffer = (unsigned int *)((
			(uintptr_t)g_pKWTpipeBuffer) + MAX_ISP_TILE_TDR_HEX_NO);
		g_pKWVirISPBuffer = (unsigned int *)((
			(uintptr_t)g_pKWCmdqBuffer) + MAX_ISP_CMDQ_BUFFER_SIZE);
	} else {
		/* Navtive Exception */
		g_pPhyISPBuffer = NULL;
		g_pTuningBuffer = NULL;
		g_pTpipeBuffer = NULL;
		g_pVirISPBuffer = NULL;
		g_pCmdqBuffer = NULL;
		/* Kernel Exception */
		g_pKWTpipeBuffer = NULL;
		g_pKWCmdqBuffer = NULL;
		g_pKWVirISPBuffer = NULL;
	}
	g_bUserBufIsReady = MFALSE;
	g_bDumpPhyISPBuf = MFALSE;
	g_dumpInfo.tdri_baseaddr = 0xFFFFFFFF;/* 0x15022204 */
	g_dumpInfo.imgi_baseaddr = 0xFFFFFFFF;/* 0x15022400 */
	g_dumpInfo.dmgi_baseaddr = 0xFFFFFFFF;/* 0x15022520 */
	g_tdriaddr = 0xffffffff;
	g_cmdqaddr = 0xffffffff;
	DumpBufferField = 0;
	g_TpipeBaseAddrInfo.MemInfoCmd = 0x0;
	g_TpipeBaseAddrInfo.MemPa = 0x0;
	g_TpipeBaseAddrInfo.MemVa = NULL;
	g_TpipeBaseAddrInfo.MemSizeDiff = 0x0;
	g_CmdqBaseAddrInfo.MemInfoCmd = 0x0;
	g_CmdqBaseAddrInfo.MemPa = 0x0;
	g_CmdqBaseAddrInfo.MemVa = NULL;
	g_CmdqBaseAddrInfo.MemSizeDiff = 0x0;

	/* mutex_unlock(&gDipMutex); */
	/*  */
	for (i = 0; i < ISP_REF_CNT_ID_MAX; i++)
		atomic_set(&g_imem_ref_cnt[i], 0);

/*  */
for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++) {
	for (j = 0; j < ISP_IRQ_ST_AMOUNT; j++) {
		for (q = 0; q < IRQ_USER_NUM_MAX; q++) {
			IspInfo.IrqInfo.Status[i][j][q] = 0;
			IspInfo.IrqInfo.MarkedFlag[i][j][q] = 0;
			for (p = 0; p < 32; p++) {
				IspInfo.IrqInfo.MarkedTime_sec[i][p][q]
					  = 0;
				IspInfo.IrqInfo.MarkedTime_usec[i][p][q]
					  = 0;
				IspInfo.IrqInfo.PassedBySigCnt[i][p][q]
					  = 0;
				IspInfo.IrqInfo.LastestSigTime_sec[i][p]
					  = 0;
				IspInfo.IrqInfo.LastestSigTime_usec[i][p]
					  = 0;
			}
		}
	}
}
	/* reset backup regs*/
	memset(g_BkReg, 0, sizeof(struct _isp_bk_reg_t) * ISP_IRQ_TYPE_AMOUNT);

#ifdef ENABLE_KEEP_ION_HANDLE
	/* create ion client*/
	ISP_ion_init();
#endif

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

		if (IspInfo.UserCount == 1) {
			ISP_WR32(CAMSYS_REG_HALT1_EN, 0x00000001);
			ISP_WR32(CAMSYS_REG_HALT2_EN, 0x00000001);
			ISP_WR32(CAMSYS_REG_HALT3_EN, 0x00000001);
			ISP_WR32(CAMSYS_REG_HALT4_EN, 0x00000001);
		}
	}
	mutex_unlock(&gDipMutex);

	pr_info("- X. Ret: %d. UserCount: %d. G_u4EnableClockCount:%d\n", Ret,
		IspInfo.UserCount, G_u4EnableClockCount);
	return Ret;

}

/******************************************************************************
 *
 *****************************************************************************/
static inline void ISP_StopHW(signed int module)
{
	unsigned int regTGSt, loopCnt;
	signed int ret = 0;
	struct ISP_WAIT_IRQ_STRUCT waitirq;
	unsigned long long  sec = 0, m_sec = 0;
	unsigned long long  timeoutMs = 500000000;/*500ms*/
	char moduleName[128];

	if (module == ISP_CAM_A_IDX)
		strncpy(moduleName, "CAMA", 5);
	else
		strncpy(moduleName, "CAMB", 5);

	/* wait TG idle*/
	loopCnt = 3;
	waitirq.Type = (module == ISP_CAM_A_IDX) ?
		ISP_IRQ_TYPE_INT_CAM_A_ST : ISP_IRQ_TYPE_INT_CAM_B_ST;
	waitirq.EventInfo.Clear = ISP_IRQ_CLEAR_WAIT;
	waitirq.EventInfo.Status = VS_INT_ST;
	waitirq.EventInfo.St_type = SIGNAL_INT;
	waitirq.EventInfo.Timeout = 0x100;
	waitirq.EventInfo.UserKey = 0x0;
	waitirq.bDumpReg = 0;

	do {
		regTGSt = (ISP_RD32(CAM_REG_TG_INTER_ST(module)) & 0x00003F00)
			   >> 8;
		if (regTGSt == 1 || regTGSt == 0)
			break;

		pr_info("%s: wait 1VD (%d)\n", moduleName, loopCnt);
		ret = ISP_WaitIrq(&waitirq);
		/* first wait is clear wait, others are non-clear wait */
		waitirq.EventInfo.Clear = ISP_IRQ_CLEAR_NONE;
	} while (--loopCnt);

	if (-ERESTARTSYS == ret) {
		pr_info("%s: interrupt by system signal, wait idle\n",
			moduleName);
		/* timer*/
		m_sec = ktime_get();

		if (regTGSt == 1)
			pr_info("%s: wait idle done\n", moduleName);
		else if (regTGSt == 0)
			pr_info("plz check regTGSt value");
		else
			pr_info("%s: wait idle timeout(%lld)\n", moduleName,
				(sec - m_sec));
	}

	if (-EFAULT == ret || regTGSt != 1) {
		pr_info("%s: reset\n", moduleName);
		/* timer*/
		m_sec = ktime_get();

		/* Reset*/
		ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x1);
		while (ISP_RD32(CAM_REG_CTL_SW_CTL(module)) != 0x2) {
			/*pr_info("%s resetting...\n", moduleName);*/
			/*timer*/
			sec = ktime_get();
			/* wait time>timeoutMs, break */
			if ((sec  - m_sec) > timeoutMs) {
				pr_info("%s: wait SW idle timeout\n",
					moduleName);
				break;
			}
		}
		ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x4);
		ISP_WR32(CAM_REG_CTL_SW_CTL(module), 0x0);
		regTGSt = (ISP_RD32(CAM_REG_TG_INTER_ST(module)) & 0x00003F00)
			   >> 8;
		pr_info("%s_TG_ST(%d)_SW_ST(0x%x)\n", moduleName, regTGSt,
			ISP_RD32(CAM_REG_CTL_SW_CTL(module)));
	}

	/*disable CMOS*/
	ISP_WR32(CAM_REG_TG_SEN_MODE(module),
		(ISP_RD32(CAM_REG_TG_SEN_MODE(module))&0xfffffffe));

}

/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_release(
	struct inode *pInode,
	struct file *pFile)
{
	struct ISP_USER_INFO_STRUCT *pUserInfo;
	unsigned int Reg;
	unsigned int i = 0;

	pr_info("- E. UserCount: %d.\n", IspInfo.UserCount);

	if (pFile->private_data != NULL) {
		pUserInfo = (struct ISP_USER_INFO_STRUCT *)pFile->private_data;
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	mutex_lock(&gDipMutex);  /* Protect the Multi Process */
	/*      */
	spin_lock(&(IspInfo.SpinLockIspRef));
	IspInfo.UserCount--;
	if (IspInfo.UserCount > 0) {
		spin_unlock(&(IspInfo.SpinLockIspRef));
		pr_info(
			"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d),users exist",
			IspInfo.UserCount, current->comm, current->pid,
			current->tgid);
		goto EXIT;
	} else {
		spin_unlock(&(IspInfo.SpinLockIspRef));
	}

	/* kernel log limit back to default */
#if (_K_LOG_ADJUST == 1)
	set_detect_count(pr_detect_count);
#endif
	/*      */

	pr_info(
		"Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), log_limit_line(%d), last user",
		IspInfo.UserCount, current->comm, current->pid, current->tgid,
		pr_detect_count);


	/* Close VF when ISP_release.
	 * reason of close vf is to make sure camera can serve regular after
	 * previous abnormal exit
	 */

	Reg = ISP_RD32(CAM_REG_TG_VF_CON(ISP_CAM_A_IDX));
	Reg &= 0xfffffffE;/* close Vfinder */
	ISP_WR32(CAM_REG_TG_VF_CON(ISP_CAM_A_IDX), Reg);

	Reg = ISP_RD32(CAM_REG_TG_VF_CON(ISP_CAM_B_IDX));
	Reg &= 0xfffffffE;/* close Vfinder */
	ISP_WR32(CAM_REG_TG_VF_CON(ISP_CAM_B_IDX), Reg);


	for (i = ISP_CAMSV0_IDX; i <= ISP_CAMSV3_IDX; i++) {
		Reg = ISP_RD32(CAM_REG_TG_VF_CON(i));
		Reg &= 0xfffffffE;
		ISP_WR32(CAM_REG_TG_VF_CON(i), Reg);
	}


	/* Set DMX_SEL = 0 when ISP_release.
	 * Reson:
	 * If twin is enabled, the twin module's DMX_SEL will be set to 1.
	 * It will encounter err when run single path
	 * and other module dmx_sel = 1
	 */
	if (!sec_on) {
		Reg = ISP_RD32(CAM_REG_CTL_SEL(ISP_CAM_A_IDX));
		Reg &= 0xfffffff8;/* set dmx to 0 */
		ISP_WR32(CAM_REG_CTL_SEL(ISP_CAM_A_IDX), Reg);

		Reg = ISP_RD32(CAM_REG_CTL_SEL(ISP_CAM_B_IDX));
		Reg &= 0xfffffff8;/* set dmx to 0 */
		ISP_WR32(CAM_REG_CTL_SEL(ISP_CAM_B_IDX), Reg);
	}
	/* Reset Twin status.
	 * If previous camera run in twin mode,
	 * then mediaserver died, no one clear this status.
	 * Next camera runs in single mode, and it will not update CQ0
	 */
	ISP_WR32(CAM_REG_CTL_TWIN_STATUS(ISP_CAM_A_IDX), 0x0);
	ISP_WR32(CAM_REG_CTL_TWIN_STATUS(ISP_CAM_B_IDX), 0x0);

	/* why i add this wake_unlock here,
	 * because the Ap is not expected to be dead.
	 * The driver must releae the wakelock, ozws the system will not enter
	 */
	/* the power-saving mode */
	if (g_WaitLockCt) {
		pr_info("wakelock disable!! cnt(%d)\n", g_WaitLockCt);
#ifdef CONFIG_PM_SLEEP
		__pm_relax(isp_wake_lock);
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
	/* mutex_lock(&gDipMutex); */  /* Protect the Multi Process */
	if (g_bIonBufferAllocated == MFALSE) {
		/* Native Exception */
		if (g_pPhyISPBuffer != NULL) {
			vfree(g_pPhyISPBuffer);
			g_pPhyISPBuffer = NULL;
		}
		if (g_pTuningBuffer != NULL) {
			vfree(g_pTuningBuffer);
			g_pTuningBuffer = NULL;
		}
		if (g_pTpipeBuffer != NULL) {
			vfree(g_pTpipeBuffer);
			g_pTpipeBuffer = NULL;
		}
		if (g_pVirISPBuffer != NULL) {
			vfree(g_pVirISPBuffer);
			g_pVirISPBuffer = NULL;
		}
		if (g_pCmdqBuffer != NULL) {
			vfree(g_pCmdqBuffer);
			g_pCmdqBuffer = NULL;
		}
		/* Kernel Exception */
		if (g_pKWTpipeBuffer != NULL) {
			vfree(g_pKWTpipeBuffer);
			g_pKWTpipeBuffer = NULL;
		}
		if (g_pKWCmdqBuffer != NULL) {
			vfree(g_pKWCmdqBuffer);
			g_pKWCmdqBuffer = NULL;
		}
		if (g_pKWVirISPBuffer != NULL) {
			vfree(g_pKWVirISPBuffer);
			g_pKWVirISPBuffer = NULL;
		}
	} else {
#ifdef AEE_DUMP_BY_USING_ION_MEMORY
		if (g_isp_p2_imem_buf.handle != NULL) {
			isp_freebuf(&g_isp_p2_imem_buf);
			g_isp_p2_imem_buf.handle = NULL;
			g_isp_p2_imem_buf.ion_fd = 0;
			g_isp_p2_imem_buf.va = 0;
			g_isp_p2_imem_buf.pa = 0;
		}
		g_bIonBufferAllocated = MFALSE;
		/* Navtive Exception */
		g_pPhyISPBuffer = NULL;
		g_pTuningBuffer = NULL;
		g_pTpipeBuffer = NULL;
		g_pVirISPBuffer = NULL;
		g_pCmdqBuffer = NULL;
		/* Kernel Exception */
		g_pKWTpipeBuffer = NULL;
		g_pKWCmdqBuffer = NULL;
		g_pKWVirISPBuffer = NULL;
#endif
	}
	/* mutex_unlock(&gDipMutex); */

#ifdef AEE_DUMP_BY_USING_ION_MEMORY
	if (isp_p2_ion_client != NULL) {
		ion_client_destroy(isp_p2_ion_client);
		isp_p2_ion_client = NULL;
	} else {
		LOG_NOTICE("isp_p2_ion_client is NULL!!\n");
	}
#endif
	/* reset backup regs*/
	memset(g_BkReg, 0, sizeof(struct _isp_bk_reg_t) * ISP_IRQ_TYPE_AMOUNT);

	/* reset secure cam info*/
	if (sec_on) {
		memset(&lock_reg, 0, sizeof(struct isp_sec_dapc_reg));
		sec_on = 0;
	}

	/*  */
	pr_info("Start ISP_StopHW");
	ISP_StopHW(ISP_CAM_A_IDX);
	ISP_StopHW(ISP_CAM_B_IDX);
	pr_info("End ISP_StopHW");

#ifdef ENABLE_KEEP_ION_HANDLE
	/* free keep ion handles, then destroy ion client*/
	for (i = 0; i < ISP_DEV_NODE_NUM; i++) {
		if (gION_TBL[i].node != ISP_DEV_NODE_NUM)
			ISP_ion_free_handle_by_module(i);
	}
	pr_info("Start ISP_ion_uninit");
	ISP_ion_uninit();
	pr_info("End ISP_ion_uninit");
#endif

	/*  */
	/* pr_info("Before spm_enable_sodi()."); */
	/* Enable sodi (Multi-Core Deep Idle). */

#if 0 /* _mt6593fpga_dvt_use_ */
	spm_enable_sodi();
#endif

	/* Disable clock.
	 * 1. clkmgr: G_u4EnableClockCount=0, call clk_enable/disable
	 * 2. CCF: call clk_enable/disable every time
	 *    -> when IspInfo.UserCount, disable all ISP clk
	 */
	spin_lock(&(IspInfo.SpinLockClock));
	i = G_u4EnableClockCount;
	spin_unlock(&(IspInfo.SpinLockClock));
	while (i > 0) {
		ISP_EnableClock(MFALSE);
		i--;
	}

EXIT:
	mutex_unlock(&gDipMutex);
	pr_info("- X. UserCount: %d. G_u4EnableClockCount:%d",
		IspInfo.UserCount, G_u4EnableClockCount);
	return 0;
}


/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_mmap(struct file *pFile, struct vm_area_struct *pVma)
{
	unsigned long length = 0;
	unsigned long pfn = 0x0;

	/*pr_info("- E.");*/
	length = (pVma->vm_end - pVma->vm_start);
	/*  */
	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
	pfn = pVma->vm_pgoff << PAGE_SHIFT;
	/*pr_info(
	 * "vm_pgo(0x%lx),pfn(0x%x),phy(0x%lx),vm(0x%lx_0x%lx),len(0x%lx)\n",
	 * pVma->vm_pgoff, pfn, pVma->vm_pgoff << PAGE_SHIFT,
	 * pVma->vm_start, pVma->vm_end, length);
	 */

	switch (pfn) {
	case CAM_A_BASE_HW:
	case CAM_B_BASE_HW:
	case CAMSV_0_BASE_HW:
	case CAMSV_1_BASE_HW:
	case CAMSV_2_BASE_HW:
	case CAMSV_3_BASE_HW:
	case CAMSV_4_BASE_HW:
	case CAMSV_5_BASE_HW:
	case UNI_A_BASE_HW:
		if (length > ISP_REG_RANGE) {
			LOG_NOTICE(
				"mmap range error :module(0x%x) length(0x%lx), ISP_REG_RANGE(0x%lx)!\n",
				pfn, length, ISP_REG_RANGE);
			return -EAGAIN;
		}
		break;
	case DIP_A_BASE_HW:
		if (length > ISP_REG_PER_DIP_RANGE) {
			LOG_NOTICE(
				"mmap range error :module(0x%x),length(0x%lx), ISP_REG_PER_DIP_RANGE(0x%lx)!\n",
				pfn, length, ISP_REG_PER_DIP_RANGE);
			return -EAGAIN;
		}
		break;
	case SENINF_BASE_HW:
		if (length > 0x8000) {
			LOG_NOTICE(
				"mmap range error :module(0x%x),length(0x%lx), SENINF_BASE_RANGE(0x%x)!\n",
				pfn, length, 0x4000);
			return -EAGAIN;
		}
		break;
	case MIPI_RX_BASE_HW:
		if (length > 0x6000) {
			LOG_NOTICE(
				"mmap range error :module(0x%x),length(0x%lx), MIPI_RX_RANGE(0x%x)!\n",
				pfn, length, 0x6000);
			return -EAGAIN;
		}
		break;
	default:
		LOG_NOTICE("Illegal starting HW addr for mmap!\n");
		return -EAGAIN;
	}
	if (remap_pfn_range(pVma, pVma->vm_start, pVma->vm_pgoff,
	    pVma->vm_end - pVma->vm_start, pVma->vm_page_prot)) {
		pr_info("remap_pfn_range fail");
		return -EAGAIN;
	}

	/*  */
	return 0;
}

/******************************************************************************
 *
 *****************************************************************************/

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
 *****************************************************************************/
static inline void ISP_UnregCharDev(void)
{
	pr_info("- E.");
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
 *****************************************************************************/
static inline signed int ISP_RegCharDev(void)
{
	signed int Ret = 0;
	/*  */
	pr_info("- E.\n");
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


	/*      */

	pr_info("- X.\n");
	return Ret;
}

/******************************************************************************
 *
 *****************************************************************************/
static signed int ISP_probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	/*    struct resource *pRes = NULL;*/
	signed int i = 0, j = 0;
	unsigned char n;
	unsigned int irq_info[3]; /* Record interrupts info from device tree */
	struct isp_device *_ispdev = NULL;

#ifdef CONFIG_OF
	struct isp_device *isp_dev;
	struct device *dev = NULL;
#endif

	pr_info("- E. ISP driver probe.\n");

	/* Get platform_device parameters */
#ifdef CONFIG_OF

	if (pDev == NULL) {
		dev_err(&pDev->dev, "pDev is NULL");
		return -ENXIO;
	}

	nr_isp_devs += 1;
	atomic_inc(&G_u4DevNodeCt);
#if 1
	_ispdev = krealloc(isp_devs, sizeof(struct isp_device) * nr_isp_devs,
		GFP_KERNEL);
	if (!_ispdev) {
		dev_err(&pDev->dev, "Unable to allocate isp_devs\n");
		return -ENOMEM;
	}
	isp_devs = _ispdev;


	#if defined(ISP_MET_READY)
	/*MET: met mmsys profile*/
	if (met_mmsys_config_isp_base_addr)
		met_mmsys_config_isp_base_addr((unsigned long *)isp_devs);
	#endif


#else
	/* WARNING:KREALLOC_ARG_REUSE:
	 * Reusing the krealloc arg is almost always a bug
	 */
	isp_devs = KREALLOC(isp_devs, sizeof(struct isp_device) * nr_isp_devs,
		GFP_KERNEL);
	if (!isp_devs) {
		dev_err(&pDev->dev, "Unable to allocate isp_devs\n");
		return -ENOMEM;
	}
#endif

	isp_dev = &(isp_devs[nr_isp_devs - 1]);
	isp_dev->dev = &pDev->dev;

	/* iomap registers */
	isp_dev->regs = of_iomap(pDev->dev.of_node, 0);
	if (!isp_dev->regs) {
		dev_err(&pDev->dev,
			"Unable to ioremap registers, of_iomap fail, nr_isp_devs=%d, devnode(%s).\n",
			nr_isp_devs, pDev->dev.of_node->name);
		return -ENOMEM;
	}

	pr_info("nr_isp_devs=%d, devnode(%s), map_addr=0x%lx\n",
		nr_isp_devs, pDev->dev.of_node->name,
		(unsigned long)isp_dev->regs);

	/* get IRQ ID and request IRQ */
	isp_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);

	if (isp_dev->irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array(pDev->dev.of_node, "interrupts",
		    irq_info, ARRAY_SIZE(irq_info))) {
			dev_err(&pDev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++) {
			if ((strcmp(pDev->dev.of_node->name,
			     IRQ_CB_TBL[i].device_name) == 0) &&
			    (IRQ_CB_TBL[i].isr_fp != NULL)) {
				Ret = request_irq(isp_dev->irq,
					(irq_handler_t)IRQ_CB_TBL[i].isr_fp,
					irq_info[2],
					(const char *)IRQ_CB_TBL[i].device_name,
					NULL);
				if (Ret) {
					dev_err(&pDev->dev,
					  "request_irq fail, nr_isp_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
					  nr_isp_devs, pDev->dev.of_node->name,
					  isp_dev->irq,
					  IRQ_CB_TBL[i].device_name
					);
					return Ret;
				}

				pr_info(
					"nr_isp_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
					nr_isp_devs, pDev->dev.of_node->name,
					isp_dev->irq,
					IRQ_CB_TBL[i].device_name);
				break;
			}
		}

		if (i >= ISP_IRQ_TYPE_AMOUNT)
			pr_info(
				"No corresponding ISR!!: nr_isp_devs=%d, devnode(%s), irq=%d\n",
				nr_isp_devs, pDev->dev.of_node->name,
				isp_dev->irq);


	} else {
		pr_info("No IRQ!!: nr_isp_devs=%d, devnode(%s), irq=%d\n",
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
		dev = device_create(pIspClass, NULL, IspDevNo, NULL,
				ISP_DEV_NAME);

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

		spin_lock_init(&(SpinLock_P2FrameList));
		spin_lock_init(&(SpinLockRegScen));
		spin_lock_init(&(SpinLock_UserKey));
		#ifdef ENABLE_KEEP_ION_HANDLE
		for (i = 0; i < ISP_DEV_NODE_NUM; i++) {
			if (gION_TBL[i].node != ISP_DEV_NODE_NUM) {
				for (n = 0; n < _dma_max_wr_; n++)
					spin_lock_init(&(gION_TBL[i].pLock[n]));
			}
		}
		#endif

#ifndef EP_NO_CLKMGR

#ifdef CONFIG_MTK_CLKMGR
#else
		/*CCF: Grab clock pointer (struct clk*) */

		isp_clk.ISP_SCP_SYS_DIS =
			devm_clk_get(&pDev->dev, "ISP_SCP_SYS_DIS");
		isp_clk.ISP_SCP_SYS_ISP =
			devm_clk_get(&pDev->dev, "ISP_SCP_SYS_ISP");
		isp_clk.ISP_SCP_SYS_CAM =
			devm_clk_get(&pDev->dev, "ISP_SCP_SYS_CAM");
		isp_clk.ISP_IMG_DIP =
			devm_clk_get(&pDev->dev, "ISP_CLK_IMG_DIP");
		isp_clk.ISP_CAM_CAMSYS =
			devm_clk_get(&pDev->dev, "ISP_CLK_CAM");
		isp_clk.ISP_CAM_CAMTG =
			devm_clk_get(&pDev->dev, "ISP_CLK_CAMTG");
		isp_clk.ISP_CAM_SENINF =
			devm_clk_get(&pDev->dev, "ISP_CLK_CAM_SENINF");
		isp_clk.ISP_CAM_CAMSV0 =
			devm_clk_get(&pDev->dev, "ISP_CLK_CAMSV0");
		isp_clk.ISP_CAM_CAMSV1 =
			devm_clk_get(&pDev->dev, "ISP_CLK_CAMSV1");
		isp_clk.ISP_CAM_CAMSV2 =
			devm_clk_get(&pDev->dev, "ISP_CLK_CAMSV2");

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
		if (IS_ERR(isp_clk.ISP_IMG_DIP)) {
			LOG_NOTICE("cannot get IMG_DIP clock\n");
			return PTR_ERR(isp_clk.ISP_IMG_DIP);
		}
		if (IS_ERR(isp_clk.ISP_CAM_CAMSYS)) {
			LOG_NOTICE("cannot get ISP_CLK_CAM clock\n");
			return PTR_ERR(isp_clk.ISP_CAM_CAMSYS);
		}
		if (IS_ERR(isp_clk.ISP_CAM_CAMTG)) {
			LOG_NOTICE("cannot get ISP_CLK_CAMTG clock\n");
			return PTR_ERR(isp_clk.ISP_CAM_CAMTG);
		}
		if (IS_ERR(isp_clk.ISP_CAM_SENINF)) {
			LOG_NOTICE("cannot get ISP_CLK_CAM_SENINF clock\n");
			return PTR_ERR(isp_clk.ISP_CAM_SENINF);
		}
		if (IS_ERR(isp_clk.ISP_CAM_CAMSV0)) {
			LOG_NOTICE("cannot get ISP_CLK_CAMSV0 clock\n");
			return PTR_ERR(isp_clk.ISP_CAM_CAMSV0);
		}
		if (IS_ERR(isp_clk.ISP_CAM_CAMSV1)) {
			LOG_NOTICE("cannot get ISP_CLK_CAMSV1 clock\n");
			return PTR_ERR(isp_clk.ISP_CAM_CAMSV1);
		}
		if (IS_ERR(isp_clk.ISP_CAM_CAMSV2)) {
			LOG_NOTICE("cannot get ISP_CLK_CAMSV2 clock\n");
			return PTR_ERR(isp_clk.ISP_CAM_CAMSV2);
		}

#endif
#endif
	/*  */
	for (i = 0 ; i < ISP_IRQ_TYPE_AMOUNT; i++)
		init_waitqueue_head(&IspInfo.WaitQueueHead[i]);
	/*	*/
	for (i = 0 ; i < CAM_AMOUNT ; i++) {
		for (j = 0 ; j < ISP_WAITQ_HEAD_IRQ_AMOUNT ; j++)
			init_waitqueue_head(&IspInfo.WaitQHeadCam[i][j]);
	}
	/*	*/
	for (i = 0 ; i < CAMSV_AMOUNT ; i++) {
		for (j = 0 ; j < ISP_WAITQ_HEAD_IRQ_SV_AMOUNT ; j++)
			init_waitqueue_head(&IspInfo.WaitQHeadCamsv[i][j]);
	}

#ifdef CONFIG_PM_SLEEP
		isp_wake_lock = wakeup_source_register(&pDev->dev, "isp_lock_wakelock");
		// wakeup_source_init(&isp_wake_lock, "isp_lock_wakelock");
#endif

		/* enqueue/dequeue control in ihalpipe wrapper */
		init_waitqueue_head(&P2WaitQueueHead_WaitDeque);
		init_waitqueue_head(&P2WaitQueueHead_WaitFrame);
		init_waitqueue_head(&P2WaitQueueHead_WaitFrameEQDforDQ);

		for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++)
			tasklet_init(isp_tasklet[i].pIsp_tkt,
				isp_tasklet[i].tkt_cb, 0);

#if (ISP_BOTTOMHALF_WORKQ == 1)
		for (i = 0 ; i < ISP_IRQ_TYPE_AMOUNT; i++) {
			isp_workque[i].module = i;
			memset((void *)&(isp_workque[i].isp_bh_work), 0,
				sizeof(isp_workque[i].isp_bh_work));
			INIT_WORK(&(isp_workque[i].isp_bh_work),
				ISP_BH_Workqueue);
		}
#endif


		/* Init IspInfo*/
		spin_lock(&(IspInfo.SpinLockIspRef));
		IspInfo.UserCount = 0;
		spin_unlock(&(IspInfo.SpinLockIspRef));
		/*  */
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAM_A_ST][SIGNAL_INT]
			= INT_ST_MASK_CAM;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAM_B_ST][SIGNAL_INT]
			= INT_ST_MASK_CAM;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV_0_ST][SIGNAL_INT]
			= INT_ST_MASK_CAMSV;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV_1_ST][SIGNAL_INT]
			= INT_ST_MASK_CAMSV;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV_2_ST][SIGNAL_INT]
			= INT_ST_MASK_CAMSV;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV_3_ST][SIGNAL_INT]
			= INT_ST_MASK_CAMSV;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV_4_ST][SIGNAL_INT]
			= INT_ST_MASK_CAMSV;
		IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV_5_ST][SIGNAL_INT]
			= INT_ST_MASK_CAMSV;
		/* only cam have warning mask */
		IspInfo.IrqInfo.WarnMask[ISP_IRQ_TYPE_INT_CAM_A_ST][SIGNAL_INT]
			= INT_ST_MASK_CAM_WARN;
		IspInfo.IrqInfo.WarnMask[ISP_IRQ_TYPE_INT_CAM_B_ST][SIGNAL_INT]
			= INT_ST_MASK_CAM_WARN;
		/*  */
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAM_A_ST][SIGNAL_INT]
			= INT_ST_MASK_CAM_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAM_B_ST][SIGNAL_INT]
			= INT_ST_MASK_CAM_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV_0_ST][SIGNAL_INT]
			= INT_ST_MASK_CAMSV_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV_1_ST][SIGNAL_INT]
			= INT_ST_MASK_CAMSV_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV_2_ST][SIGNAL_INT]
			= INT_ST_MASK_CAMSV_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV_3_ST][SIGNAL_INT]
			= INT_ST_MASK_CAMSV_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV_4_ST][SIGNAL_INT]
			= INT_ST_MASK_CAMSV_ERR;
		IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV_5_ST][SIGNAL_INT]
			= INT_ST_MASK_CAMSV_ERR;

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

	pr_info("- X. ISP driver probe.\n");

	return Ret;
}

/******************************************************************************
 * Called when the device is being detached from the driver
 *****************************************************************************/
static signed int ISP_remove(struct platform_device *pDev)
{
	/*    struct resource *pRes;*/
	signed int IrqNum;
	int i;
	/*  */
	pr_info("- E.");
	/* unregister char driver. */
	ISP_UnregCharDev();

	/* Release IRQ */
	disable_irq(IspInfo.IrqNum);
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);

	/* kill tasklet */
	for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++)
		tasklet_kill(isp_tasklet[i].pIsp_tkt);

#if 0
	/* free all registered irq(child nodes) */
	ISP_UnRegister_AllregIrq();
	/* free father nodes of irq user list */
	struct my_list_head *head;
	struct my_list_head *father;

	head = ((struct my_list_head *)(&SupIrqUserListHead.list));
	while (1) {
		father = head;
		if (father->nextirq != father) {
			father = father->nextirq;
			REG_IRQ_NODE *accessNode;

			typeof(((REG_IRQ_NODE *)0)->list) * __mptr = (father);
			accessNode = ((REG_IRQ_NODE *)
			    ((char *)__mptr - offsetof(REG_IRQ_NODE, list)));
			pr_info("free father,reg_T(%d)\n", accessNode->reg_T);
			if (father->nextirq != father) {
				head->nextirq = father->nextirq;
				father->nextirq = father;
			} else {
				/* last father node */
				head->nextirq = head;
				pr_info("break\n");
				break;
			}
			kfree(accessNode);
		}
	}
#endif
	/*  */
	device_destroy(pIspClass, IspDevNo);
	/*  */
	class_destroy(pIspClass);
	pIspClass = NULL;
	/*  */
	return 0;
}

static signed int ISP_suspend(
	struct platform_device *pDev,
	pm_message_t            Mesg
)
{
	unsigned int regVal;
	signed int IrqType, ret, module;
	char moduleName[128];

	unsigned int regTGSt, loopCnt;
	struct ISP_WAIT_IRQ_STRUCT waitirq;
	unsigned long long  sec = 0, m_sec = 0;
	unsigned long long  timeoutMs = 500000000;/*500ms*/

	ret = 0;
	module = -1;
	strncpy(moduleName, pDev->dev.of_node->name, sizeof(moduleName)-1);
	moduleName[sizeof(moduleName)-1] = '\0';

	/* update device node count*/
	atomic_dec(&G_u4DevNodeCt);

	/* Check clock counter instead of check IspInfo.UserCount
	 * for ensuring current clocks are on or off
	 */
	spin_lock(&(IspInfo.SpinLockClock));
	if (!G_u4EnableClockCount) {
		spin_unlock(&(IspInfo.SpinLockClock));
		/* Only print cama log */
		if (strcmp(moduleName,
		    IRQ_CB_TBL[ISP_IRQ_TYPE_INT_CAM_A_ST].device_name) == 0) {
			pr_info("%s - X. UserCount=%d,wakelock:%d,devct:%d\n",
				moduleName, IspInfo.UserCount,
				G_u4EnableClockCount,
				atomic_read(&G_u4DevNodeCt));
		} else if (IspInfo.UserCount != 0) {
			pr_info(
				"%s - X. UserCount=%d,G_u4EnableClockCount=0,wakelock:%d,devct:%d\n",
				moduleName, IspInfo.UserCount,
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
	/*pr_info("%s: Rs_TG(0x%08x)\n", moduleName, regVal);*/

	if (regVal & 0x01) {
		pr_info("%s_suspend,disable VF,wakelock:%d,clk:%d,devct:%d\n",
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
			regTGSt = (ISP_RD32(CAM_REG_TG_INTER_ST(module))
				   & 0x00003F00) >> 8;
			if (regTGSt == 1)
				break;

			pr_info("%s: wait 1VD (%d)\n", moduleName, loopCnt);
			ret = ISP_WaitIrq(&waitirq);
			/* first wait is clear wait,
			 * others are non-clear wait
			 */
			waitirq.EventInfo.Clear = ISP_IRQ_CLEAR_NONE;
		} while (--loopCnt);

		if (-ERESTARTSYS == ret) {
			pr_info("%s: interrupt by system signal, wait idle\n",
				moduleName);
			/* timer*/
			m_sec = ktime_get();

			while (regTGSt != 1) {
				regTGSt = (ISP_RD32(CAM_REG_TG_INTER_ST(module))
					   & 0x00003F00) >> 8;
				/*timer*/
				sec = ktime_get();
				/* wait time>timeoutMs, break */
				if ((sec - m_sec) > timeoutMs)
					break;
			}
			if (regTGSt == 1)
				pr_info("%s: wait idle done\n", moduleName);
			else
				pr_info("%s: wait idle timeout(%lld)\n",
					moduleName, (sec - m_sec));
		}

		/*backup: frame CNT
		 * After VF enable, The frame count will be 0 at next VD;
		 * if it has P1_DON after set vf disable,
		 * g_BkReg no need to add 1
		 */
		regTGSt = ISP_RD32_TG_CAM_FRM_CNT(IrqType, module);
		g_BkReg[IrqType].CAM_TG_INTER_ST = regTGSt;
		regVal = ISP_RD32(CAM_REG_TG_SEN_MODE(module));
		ISP_WR32(CAM_REG_TG_SEN_MODE(module), (regVal & (~0x01)));
	} else {
		pr_info("%s_suspend,wakelock:%d,clk:%d,devct:%d\n",
			moduleName, g_WaitLockCt,
			G_u4EnableClockCount, atomic_read(&G_u4DevNodeCt));
		SuspnedRecord[module] = 0;
	}

EXIT:
	/* last dev node will disable clk "G_u4EnableClockCount" times */
	if (!atomic_read(&G_u4DevNodeCt)) {
		spin_lock(&(IspInfo.SpinLockClock));
		loopCnt = G_u4EnableClockCount;
		spin_unlock(&(IspInfo.SpinLockClock));

		pr_info("%s - X. wakelock:%d, last dev node,disable clk:%d\n",
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
 *****************************************************************************/
static int ISP_resume(struct platform_device *pDev)
{
	unsigned int regVal;
	signed int IrqType, ret, module;
	char moduleName[128];

	ret = 0;
	module = -1;
	strncpy(moduleName, pDev->dev.of_node->name, sizeof(moduleName)-1);
	moduleName[sizeof(moduleName)-1] = '\0';

	/* update device node count*/
	atomic_inc(&G_u4DevNodeCt);

	if (IspInfo.UserCount == 0) {
		/* Only print cama log */
		if (strcmp(moduleName,
		    IRQ_CB_TBL[ISP_IRQ_TYPE_INT_CAM_A_ST].device_name) == 0)
			pr_info("%s - X. UserCount=0\n", moduleName);

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

	ISP_EnableClock(MTRUE);

	if (SuspnedRecord[module]) {
		pr_info("%s_resume,enable VF,wakelock:%d,clk:%d,devct:%d\n",
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
		pr_info("%s_resume,wakelock:%d,clk:%d,devct:%d\n", moduleName,
			g_WaitLockCt, G_u4EnableClockCount,
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
 *****************************************************************************/
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
 *****************************************************************************/
/*
 * ssize_t (*read) (struct file *, char __user *, size_t, loff_t *)
 */
static ssize_t ISP_DumpRegToProc(
	struct file *pFile,
	char *pStart,
	size_t off,
	loff_t *Count)
{
	LOG_NOTICE("%s : Not implement", __func__);
	return 0;
}

/******************************************************************************
 *
 *****************************************************************************/
/*
 * ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *)
 */
static ssize_t ISP_RegDebug(
	struct file *pFile,
	const char *pBuffer,
	size_t Count,
	loff_t *pData)
{
	LOG_NOTICE("%s : Not implement", __func__);
	return 0;
}

/*
 * ssize_t (*read) (struct file *, char __user *, size_t, loff_t *)
 */
static ssize_t CAMIO_DumpRegToProc(
	struct file *pFile,
	char *pStart,
	size_t off,
	loff_t *Count)
{
	LOG_NOTICE("%s : Not implement", __func__);
	return 0;
}


/******************************************************************************
 *
 *****************************************************************************/
/*
 * ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *)
 */
static ssize_t CAMIO_RegDebug(
	struct file *pFile,
	const char *pBuffer,
	size_t Count,
	loff_t *pData)
{
	LOG_NOTICE("%s : Not implement", __func__);
	return 0;
}

/******************************************************************************
 *
 *****************************************************************************/
static int isp_p2_ke_dump_read(struct seq_file *m, void *v)
{
#ifdef AEE_DUMP_REDUCE_MEMORY
	int i;

	pr_info("isp p2 ke dump start!! g_bDumpPhyISPBuf:%d\n",
		g_bDumpPhyISPBuf);
	pr_info("g_bDumpPhyISPBuf:%d, g_tdriaddr:0x%x, g_cmdqaddr:0x%x\n",
		g_bDumpPhyISPBuf, g_tdriaddr, g_cmdqaddr);
	seq_puts(m, "============ isp p2 ke dump register============\n");
	seq_printf(m,
		"===isp p2 you can trust below info: g_bDumpPhyISPBuf:%d===\n",
		g_bDumpPhyISPBuf);
	seq_printf(m,
		"===isp p2 g_bDumpPhyISPBuf:%d, g_tdriaddr:0x%x,g_cmdqaddr:0x%x===\n",
		g_bDumpPhyISPBuf,
		g_tdriaddr, g_cmdqaddr);
	seq_puts(m, "===isp p2 hw physical register===\n");
	if (g_bDumpPhyISPBuf == MFALSE)
		return 0;
	mutex_lock(&gDipMutex);  /* Protect the Multi Process */
	if (g_pPhyISPBuffer != NULL) {
		for (i = 0; i < (ISP_DIP_PHYSICAL_REG_SIZE >> 2); i = i + 4) {
			seq_printf(m,
				"(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)\n",
					DIP_A_BASE_HW+4*i,
					(unsigned int)g_pPhyISPBuffer[i],
					DIP_A_BASE_HW+4*(i+1),
					(unsigned int)g_pPhyISPBuffer[i+1],
					DIP_A_BASE_HW+4*(i+2),
					(unsigned int)g_pPhyISPBuffer[i+2],
					DIP_A_BASE_HW+4*(i+3),
					(unsigned int)g_pPhyISPBuffer[i+3]);
		}
	} else {
		pr_info("g_pPhyISPBuffer:(0x%pK)\n", g_pPhyISPBuffer);
	}
	seq_puts(m, "===isp p2 tpipe buffer Info===\n");
	if (g_pKWTpipeBuffer != NULL) {
		for (i = 0; i < (MAX_ISP_TILE_TDR_HEX_NO >> 2); i = i + 4) {
			seq_printf(m, "0x%08X\n0x%08X\n0x%08X\n0x%08X\n",
					(unsigned int)g_pKWTpipeBuffer[i],
					(unsigned int)g_pKWTpipeBuffer[i+1],
					(unsigned int)g_pKWTpipeBuffer[i+2],
					(unsigned int)g_pKWTpipeBuffer[i+3]);
		}
	}
	seq_puts(m, "===isp p2 cmdq buffer Info===\n");
	if (g_pKWCmdqBuffer != NULL) {
		for (i = 0; i < (MAX_ISP_CMDQ_BUFFER_SIZE >> 2); i = i + 4) {
			seq_printf(m, "[0x%08X 0x%08X 0x%08X 0x%08X]\n",
					(unsigned int)g_pKWCmdqBuffer[i],
					(unsigned int)g_pKWCmdqBuffer[i+1],
					(unsigned int)g_pKWCmdqBuffer[i+2],
					(unsigned int)g_pKWCmdqBuffer[i+3]);
		}
	}
	seq_puts(m, "===isp p2 vir isp buffer Info===\n");
	if (g_pKWVirISPBuffer != NULL) {
		for (i = 0; i < (ISP_DIP_REG_SIZE >> 2); i = i + 4) {
			seq_printf(m,
				"(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)\n",
					DIP_A_BASE_HW+4*i,
					(unsigned int)g_pKWVirISPBuffer[i],
					DIP_A_BASE_HW+4*(i+1),
					(unsigned int)g_pKWVirISPBuffer[i+1],
					DIP_A_BASE_HW+4*(i+2),
					(unsigned int)g_pKWVirISPBuffer[i+2],
					DIP_A_BASE_HW+4*(i+3),
					(unsigned int)g_pKWVirISPBuffer[i+3]);
		}
	}
	mutex_unlock(&gDipMutex);
	seq_puts(m, "============ isp p2 ke dump debug ============\n");
	pr_info("isp p2 ke dump end\n");
#else
	int i;

	seq_puts(m, "============ isp p2 ke dump register============\n");
	seq_puts(m, "isp p2 hw physical register\n");
	for (i = 0; i < (ISP_DIP_REG_SIZE >> 2); i = i + 4) {
		seq_printf(m,
		   "(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)\n",
		   DIP_A_BASE_HW+4*i, (unsigned int)g_PhyISPBuffer[i],
		   DIP_A_BASE_HW+4*(i+1), (unsigned int)g_PhyISPBuffer[i+1],
		   DIP_A_BASE_HW+4*(i+2), (unsigned int)g_PhyISPBuffer[i+2],
		   DIP_A_BASE_HW+4*(i+3), (unsigned int)g_PhyISPBuffer[i+3]);
	}
	seq_puts(m, "isp p2 tpipe buffer Info\n");
	for (i = 0; i < (MAX_ISP_TILE_TDR_HEX_NO >> 2); i = i + 4) {
		seq_printf(m, "0x%08X\n0x%08X\n0x%08X\n0x%08X\n",
				(unsigned int)g_KWTpipeBuffer[i],
				(unsigned int)g_KWTpipeBuffer[i+1],
				(unsigned int)g_KWTpipeBuffer[i+2],
				(unsigned int)g_KWTpipeBuffer[i+3]);
	}
	seq_puts(m, "isp p2 cmdq buffer Info\n");
	for (i = 0; i < (MAX_ISP_CMDQ_BUFFER_SIZE >> 2); i = i + 4) {
		seq_printf(m, "[0x%08X 0x%08X 0x%08X 0x%08X]\n",
				(unsigned int)g_KWCmdqBuffer[i],
				(unsigned int)g_KWCmdqBuffer[i+1],
				(unsigned int)g_KWCmdqBuffer[i+2],
				(unsigned int)g_KWCmdqBuffer[i+3]);
	}
	seq_puts(m, "isp p2 vir isp buffer Info\n");
	for (i = 0; i < (ISP_DIP_REG_SIZE >> 2); i = i + 4) {
		seq_printf(m,
		    "(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)\n",
		    DIP_A_BASE_HW+4*i, (unsigned int)g_KWVirISPBuffer[i],
		    DIP_A_BASE_HW+4*(i+1), (unsigned int)g_KWVirISPBuffer[i+1],
		    DIP_A_BASE_HW+4*(i+2), (unsigned int)g_KWVirISPBuffer[i+2],
		    DIP_A_BASE_HW+4*(i+3), (unsigned int)g_KWVirISPBuffer[i+3]);
	}
	seq_puts(m, "============ isp p2 ke dump debug ============\n");
#endif
	return 0;
}
static int proc_isp_p2_ke_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, isp_p2_ke_dump_read, NULL);
}
static const struct file_operations isp_p2_ke_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_isp_p2_ke_dump_open,
	.read = seq_read,
	.release = single_release,
};

/******************************************************************************
 *
 *****************************************************************************/
static int isp_p2_dump_read(struct seq_file *m, void *v)
{
#ifdef AEE_DUMP_REDUCE_MEMORY
	int i;

	pr_info(
		"isp p2 ne dump start!! g_bUserBufIsReady:%d, g_bIonBufferAllocated:%d\n",
		g_bUserBufIsReady, g_bIonBufferAllocated);
	pr_info(
		"isp p2 g_bDumpPhyB:%d, tdriadd:0x%x,imgiadd:0x%x,dmgiadd:0x%x\n",
		g_bDumpPhyISPBuf, g_dumpInfo.tdri_baseaddr,
		g_dumpInfo.imgi_baseaddr, g_dumpInfo.dmgi_baseaddr);
	seq_puts(m, "============ isp p2 ne dump register============\n");
	seq_printf(m,
		"===isp p2 you can trust below info:UserBufIsReady:%d===\n",
		g_bUserBufIsReady);
	seq_printf(m,
		"===isp p2 g_bDumpPhyB:%d,tdriadd:0x%x,imgiadd:0x%x,dmgiadd:0x%x===\n",
		g_bDumpPhyISPBuf, g_dumpInfo.tdri_baseaddr,
		g_dumpInfo.imgi_baseaddr, g_dumpInfo.dmgi_baseaddr);
	seq_puts(m, "===isp p2 hw physical register===\n");
	if (g_bUserBufIsReady == MFALSE)
		return 0;
	mutex_lock(&gDipMutex);  /* Protect the Multi Process */
	if (g_pPhyISPBuffer != NULL) {
		for (i = 0; i < (ISP_DIP_PHYSICAL_REG_SIZE >> 2); i = i + 4) {
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)\n",
					DIP_A_BASE_HW+4*i,
					(unsigned int)g_pPhyISPBuffer[i],
					DIP_A_BASE_HW+4*(i+1),
					(unsigned int)g_pPhyISPBuffer[i+1],
					DIP_A_BASE_HW+4*(i+2),
					(unsigned int)g_pPhyISPBuffer[i+2],
					DIP_A_BASE_HW+4*(i+3),
					(unsigned int)g_pPhyISPBuffer[i+3]);
		}
	} else {
		pr_info("g_pPhyISPBuffer:(0x%pK)\n", g_pPhyISPBuffer);
	}
	seq_puts(m, "===isp p2 tpipe buffer Info===\n");
	if (g_pTpipeBuffer != NULL) {
		for (i = 0; i < (MAX_ISP_TILE_TDR_HEX_NO >> 2); i = i + 4) {
			seq_printf(m, "0x%08X\n0x%08X\n0x%08X\n0x%08X\n",
					(unsigned int)g_pTpipeBuffer[i],
					(unsigned int)g_pTpipeBuffer[i+1],
					(unsigned int)g_pTpipeBuffer[i+2],
					(unsigned int)g_pTpipeBuffer[i+3]);
		}
	}
	seq_puts(m, "===isp p2 cmdq buffer Info===\n");
	if (g_pCmdqBuffer != NULL) {
		for (i = 0; i < (MAX_ISP_CMDQ_BUFFER_SIZE >> 2); i = i + 4) {
			seq_printf(m, "[0x%08X 0x%08X 0x%08X 0x%08X]\n",
					(unsigned int)g_pCmdqBuffer[i],
					(unsigned int)g_pCmdqBuffer[i+1],
					(unsigned int)g_pCmdqBuffer[i+2],
					(unsigned int)g_pCmdqBuffer[i+3]);
		}
	}
	seq_puts(m, "===isp p2 vir isp buffer Info===\n");
	if (g_pVirISPBuffer != NULL) {
		for (i = 0; i < (ISP_DIP_REG_SIZE >> 2); i = i + 4) {
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)\n",
					DIP_A_BASE_HW+4*i,
					(unsigned int)g_pVirISPBuffer[i],
					DIP_A_BASE_HW+4*(i+1),
					(unsigned int)g_pVirISPBuffer[i+1],
					DIP_A_BASE_HW+4*(i+2),
					(unsigned int)g_pVirISPBuffer[i+2],
					DIP_A_BASE_HW+4*(i+3),
					(unsigned int)g_pVirISPBuffer[i+3]);
		}
	}
	seq_puts(m, "===isp p2 tuning buffer Info===\n");
	if (g_pTuningBuffer != NULL) {
		for (i = 0; i < (ISP_DIP_REG_SIZE >> 2); i = i + 4) {
			seq_printf(m,
				"(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)\n",
					DIP_A_BASE_HW+4*i,
					(unsigned int)g_pTuningBuffer[i],
					DIP_A_BASE_HW+4*(i+1),
					(unsigned int)g_pTuningBuffer[i+1],
					DIP_A_BASE_HW+4*(i+2),
					(unsigned int)g_pTuningBuffer[i+2],
					DIP_A_BASE_HW+4*(i+3),
					(unsigned int)g_pTuningBuffer[i+3]);
		}
	}
	mutex_unlock(&gDipMutex);
	seq_puts(m, "============ isp p2 ne dump debug ============\n");
	pr_info("isp p2 ne dump end\n");
#else
	int i;

	seq_puts(m, "============ isp p2 ne dump register============\n");
	seq_puts(m, "isp p2 hw physical register\n");
	for (i = 0; i < (ISP_DIP_REG_SIZE >> 2); i = i + 4) {
		seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)\n",
		    DIP_A_BASE_HW+4*i, (unsigned int)g_PhyISPBuffer[i],
		    DIP_A_BASE_HW+4*(i+1), (unsigned int)g_PhyISPBuffer[i+1],
		    DIP_A_BASE_HW+4*(i+2), (unsigned int)g_PhyISPBuffer[i+2],
		    DIP_A_BASE_HW+4*(i+3), (unsigned int)g_PhyISPBuffer[i+3]);
	}

	seq_puts(m, "isp p2 tpipe buffer Info\n");
	for (i = 0; i < (MAX_ISP_TILE_TDR_HEX_NO >> 2); i = i + 4) {
		seq_printf(m, "0x%08X\n0x%08X\n0x%08X\n0x%08X\n",
			   (unsigned int)g_TpipeBuffer[i],
			   (unsigned int)g_TpipeBuffer[i+1],
			   (unsigned int)g_TpipeBuffer[i+2],
			   (unsigned int)g_TpipeBuffer[i+3]);
	}

	seq_puts(m, "isp p2 cmdq buffer Info\n");
	for (i = 0; i < (MAX_ISP_CMDQ_BUFFER_SIZE >> 2); i = i + 4) {
		seq_printf(m, "[0x%08X 0x%08X 0x%08X 0x%08X]\n",
			   (unsigned int)g_CmdqBuffer[i],
			   (unsigned int)g_CmdqBuffer[i+1],
			   (unsigned int)g_CmdqBuffer[i+2],
			   (unsigned int)g_CmdqBuffer[i+3]);
	}

	seq_puts(m, "isp p2 vir isp buffer Info\n");
	for (i = 0; i < (ISP_DIP_REG_SIZE >> 2); i = i + 4) {
		seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)\n",
		    DIP_A_BASE_HW+4*i, (unsigned int)g_VirISPBuffer[i],
		    DIP_A_BASE_HW+4*(i+1), (unsigned int)g_VirISPBuffer[i+1],
		    DIP_A_BASE_HW+4*(i+2), (unsigned int)g_VirISPBuffer[i+2],
		    DIP_A_BASE_HW+4*(i+3), (unsigned int)g_VirISPBuffer[i+3]);
	}
	seq_puts(m, "isp p2 tuning buffer Info\n");
	for (i = 0; i < (ISP_DIP_REG_SIZE >> 2); i = i + 4) {
		seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)\n",
		    DIP_A_BASE_HW+4*i, (unsigned int)g_TuningBuffer[i],
		    DIP_A_BASE_HW+4*(i+1), (unsigned int)g_TuningBuffer[i+1],
		    DIP_A_BASE_HW+4*(i+2), (unsigned int)g_TuningBuffer[i+2],
		    DIP_A_BASE_HW+4*(i+3), (unsigned int)g_TuningBuffer[i+3]);
	}
	seq_puts(m, "============ kernel warning ============\n");
	seq_puts(m, "ke:isp p2 tpipe buffer Info\n");
	for (i = 0; i < (MAX_ISP_TILE_TDR_HEX_NO >> 2); i = i + 4) {
		seq_printf(m, "0x%08X\n0x%08X\n0x%08X\n0x%08X\n",
				(unsigned int)g_KWTpipeBuffer[i],
				(unsigned int)g_KWTpipeBuffer[i+1],
				(unsigned int)g_KWTpipeBuffer[i+2],
				(unsigned int)g_KWTpipeBuffer[i+3]);
	}
	seq_puts(m, "ke:isp p2 cmdq buffer Info\n");
	for (i = 0; i < (MAX_ISP_CMDQ_BUFFER_SIZE >> 2); i = i + 4) {
		seq_printf(m, "[0x%08X 0x%08X 0x%08X 0x%08X]\n",
				(unsigned int)g_KWCmdqBuffer[i],
				(unsigned int)g_KWCmdqBuffer[i+1],
				(unsigned int)g_KWCmdqBuffer[i+2],
				(unsigned int)g_KWCmdqBuffer[i+3]);
	}
	seq_puts(m, "ke:isp p2 vir isp buffer Info\n");
	for (i = 0; i < (ISP_DIP_REG_SIZE >> 2); i = i + 4) {
		seq_printf(m,
			"(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)(0x%08X,0x%08X)\n",
		    DIP_A_BASE_HW+4*i, (unsigned int)g_KWVirISPBuffer[i],
		    DIP_A_BASE_HW+4*(i+1), (unsigned int)g_KWVirISPBuffer[i+1],
		    DIP_A_BASE_HW+4*(i+2), (unsigned int)g_KWVirISPBuffer[i+2],
		    DIP_A_BASE_HW+4*(i+3), (unsigned int)g_KWVirISPBuffer[i+3]);
	}
	seq_puts(m, "============ isp p2 ne dump debug ============\n");
#endif
	return 0;
}

static int proc_isp_p2_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, isp_p2_dump_read, NULL);
}

static const struct file_operations isp_p2_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_isp_p2_dump_open,
	.read = seq_read,
	.release = single_release,
};
/******************************************************************************
 *
 *****************************************************************************/
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
 *****************************************************************************/

static signed int __init ISP_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
	struct device_node *node = NULL;
	struct proc_dir_entry *proc_entry;
	struct proc_dir_entry *isp_p2_dir;

	int i;
	/*  */
	pr_info("- E.");
	atomic_set(&G_u4DevNodeCt, 0);
	/*  */
	Ret = platform_driver_register(&IspDriver);
	if ((Ret) < 0) {
		LOG_NOTICE("platform_driver_register fail");
		return Ret;
	}
	/*  */

	/* Use of_find_compatible_node() sensor registers from device tree */
	/* Don't use compatitble define in probe(). Otherwise, probe() of Seninf
	 * driver cannot be called.
	 */
	#if (SMI_LARB_MMU_CTL == 1)
	do {
		char *comp_str = NULL;

		comp_str = kmalloc(64, GFP_KERNEL);
		if (comp_str == NULL)
			return -ENOMEM;

		for (i = 0; i < ARRAY_SIZE(SMI_LARB_BASE); i++) {

			snprintf(comp_str, 64, "mediatek,smi_larb%d", i);
			pr_info("Finding SMI_LARB compatible: %s\n", comp_str);

			node = of_find_compatible_node(NULL, NULL, comp_str);
			if (!node) {
				LOG_NOTICE(
					"find %s node failed!!!\n", comp_str);
				SMI_LARB_BASE[i] = 0;
				continue;
			}
			SMI_LARB_BASE[i] = of_iomap(node, 0);
			if (!SMI_LARB_BASE[i]) {
				LOG_NOTICE(
					"unable to map ISP_SENINF0_BASE registers!!!\n");
				break;
			}
			pr_info("SMI_LARB%d_BASE: %pK\n", i, SMI_LARB_BASE[i]);
		}

		/* if (comp_str) coverity: no need if, kfree is safe */
		kfree(comp_str);
	} while (0);
	#endif
	node = of_find_compatible_node(NULL, NULL, "mediatek,seninf1");
	if (!node) {
		LOG_NOTICE("find mediatek,seninf1 node failed!!!\n");
		return -ENODEV;
	}
	ISP_SENINF0_BASE = of_iomap(node, 0);
	if (!ISP_SENINF0_BASE) {
		LOG_NOTICE("unable to map ISP_SENINF0_BASE registers!!!\n");
		return -ENODEV;
	}
	pr_info("ISP_SENINF0_BASE: %p\n", ISP_SENINF0_BASE);

	node = of_find_compatible_node(NULL, NULL, "mediatek,seninf2");
	if (!node) {
		LOG_NOTICE("find mediatek,seninf2 node failed!!!\n");
		return -ENODEV;
	}
	ISP_SENINF1_BASE = of_iomap(node, 0);
	if (!ISP_SENINF1_BASE) {
		LOG_NOTICE("unable to map ISP_SENINF1_BASE registers!!!\n");
		return -ENODEV;
	}
	pr_info("ISP_SENINF1_BASE: %p\n", ISP_SENINF1_BASE);

	node = of_find_compatible_node(NULL, NULL, "mediatek,seninf3");
	if (!node) {
		LOG_NOTICE("find mediatek,seninf3 node failed!!!\n");
		return -ENODEV;
	}
	ISP_SENINF2_BASE = of_iomap(node, 0);
	if (!ISP_SENINF2_BASE) {
		LOG_NOTICE("unable to map ISP_SENINF2_BASE registers!!!\n");
		return -ENODEV;
	}
	pr_info("ISP_SENINF2_BASE: %p\n", ISP_SENINF2_BASE);

	node = of_find_compatible_node(NULL, NULL, "mediatek,seninf4");
	if (!node) {
		LOG_NOTICE("find mediatek,seninf4 node failed!!!\n");
		return -ENODEV;
	}
	ISP_SENINF3_BASE = of_iomap(node, 0);
	if (!ISP_SENINF3_BASE) {
		LOG_NOTICE("unable to map ISP_SENINF3_BASE registers!!!\n");
		return -ENODEV;
	}
	pr_info("ISP_SENINF3_BASE: %p\n", ISP_SENINF3_BASE);

	node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
	if (!node) {
		LOG_NOTICE("find mediatek,apmixed node failed!!!\n");
		return -ENODEV;
	}
	CLOCK_CELL_BASE = of_iomap(node, 0);
	if (!CLOCK_CELL_BASE) {
		LOG_NOTICE("unable to map CLOCK_CELL_BASE registers!!!\n");
		return -ENODEV;
	}
	pr_info("CLOCK_CELL_BASE: %p\n", CLOCK_CELL_BASE);

	node = of_find_compatible_node(NULL, NULL, "mediatek,mmsys_config");
	if (!node) {
		LOG_NOTICE("find mmsys_config node failed!!!\n");
		return -ENODEV;
	}
	ISP_MMSYS_CONFIG_BASE = of_iomap(node, 0);
	if (!ISP_MMSYS_CONFIG_BASE) {
		LOG_NOTICE(
			"unable to map ISP_MMSYS_CONFIG_BASE registers!!!\n");
		return -ENODEV;
	}
	pr_info("ISP_MMSYS_CONFIG_BASE: %p\n", ISP_MMSYS_CONFIG_BASE);

	/* FIX-ME: linux-3.10 procfs API changed */
	proc_create("driver/isp_reg", 0444, NULL, &fcameraisp_proc_fops);
	proc_create("driver/camio_reg", 0444, NULL, &fcameraio_proc_fops);

	isp_p2_dir = proc_mkdir("isp_p2", NULL);
	if (!isp_p2_dir) {
		LOG_NOTICE("fail to mkdir /proc/isp_p2\n");
		return 0;
	}
	proc_entry = proc_create("isp_p2_dump", 0444, isp_p2_dir,
				&isp_p2_dump_proc_fops);
	proc_entry = proc_create("isp_p2_kedump", 0444, isp_p2_dir,
				&isp_p2_ke_dump_proc_fops);
	for (j = 0; j < ISP_IRQ_TYPE_AMOUNT; j++) {
		switch (j) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
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

				pBuf_kmalloc[j] = kmalloc(
					i + 2 * PAGE_SIZE, GFP_KERNEL);
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
			pTbl_RTBuf[j] = (int *)((((unsigned long)
				pBuf_kmalloc[j]) + PAGE_SIZE - 1) & PAGE_MASK);
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
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * DBG_PAGE));
			gSvLog[j]._str[i][_LOG_INF] = (char *)tmp;
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * INF_PAGE));
			gSvLog[j]._str[i][_LOG_ERR] = (char *)tmp;
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * ERR_PAGE));
		}
		/* log buffer ,in case of overflow */
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

#ifndef EP_CODE_MARK_CMDQ
	/* Register ISP callback */
	pr_info("register isp callback for MDP");
	cmdqCoreRegisterCB(CMDQ_GROUP_ISP,
			   ISP_MDPClockOnCallback,
			   ISP_MDPDumpCallback,
			   ISP_MDPResetCallback,
			   ISP_MDPClockOffCallback);
	/* Register GCE callback for dumping ISP register */
	pr_info("register isp callback for GCE");
	cmdqCoreRegisterDebugRegDumpCB(ISP_BeginGCECallback,
				ISP_EndGCECallback);
#endif
	/* m4u_enable_tf(M4U_PORT_CAM_IMGI, 0);*/

#ifdef _MAGIC_NUM_ERR_HANDLING_
	pr_info("init m_LastMNum");
	for (i = 0; i < _cam_max_; i++)
		m_LastMNum[i] = 0;

#endif


	for (i = 0; i < ISP_DEV_NODE_NUM; i++)
		SuspnedRecord[i] = 0;

	pr_info("- X. Ret: %d.", Ret);
	return Ret;
}

/******************************************************************************
 *
 *****************************************************************************/
static void __exit ISP_Exit(void)
{
	int i, j;

	pr_info("- E.");
	/*  */
	platform_driver_unregister(&IspDriver);
	/*  */
#ifndef EP_CODE_MARK_CMDQ
	/* Unregister ISP callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_ISP,
			   NULL,
			   NULL,
			   NULL,
			   NULL);
	/* Un-Register GCE callback */
	pr_info("Un-register isp callback for GCE");
	cmdqCoreRegisterDebugRegDumpCB(NULL, NULL);
#endif


	for (j = 0; j < ISP_IRQ_TYPE_AMOUNT; j++) {
		if (pTbl_RTBuf[j] != NULL) {
			/* unreserve the pages */
			for (i = 0; i < Tbl_RTBuf_MMPSize[j] * PAGE_SIZE;
			     i += PAGE_SIZE)
				ClearPageReserved(virt_to_page(
					((unsigned long)pTbl_RTBuf[j]) + i));

			/* free the memory areas */
			kfree(pBuf_kmalloc[j]);
		}
	}

	/* free the memory areas */
	kfree(pLog_kmalloc);

	/*  */
}

int32_t ISP_MDPClockOnCallback(uint64_t engineFlag)
{
	/* pr_info("ISP_MDPClockOnCallback"); */
	/*pr_info("+MDPEn:%d", G_u4EnableClockCount);*/
	ISP_EnableClock(MTRUE);

	return 0;
}

int32_t ISP_MDPDumpCallback(uint64_t engineFlag, int level)
{
	pr_info("ISP_MDPDumpCallback");

	ISP_DumpDIPReg();

	return 0;
}
int32_t ISP_MDPResetCallback(uint64_t engineFlag)
{
	pr_info("ISP_MDPResetCallback");

	ISP_Reset(ISP_REG_SW_CTL_RST_CAM_P2);

	return 0;
}

int32_t ISP_MDPClockOffCallback(uint64_t engineFlag)
{
	/* pr_info("ISP_MDPClockOffCallback"); */
	ISP_EnableClock(MFALSE);
	/*pr_info("-MDPEn:%d", G_u4EnableClockCount);*/
	return 0;
}


#define ISP_IMGSYS_BASE_PHY_KK 0x15022000

static uint32_t addressToDump[] = {
#if 1
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x000),
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x004),
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x008),
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x00C),
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x010),
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x014),
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x018),
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x204),
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x208),
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x20C),
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x400),
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x408),
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x410),
(uint32_t)(ISP_IMGSYS_BASE_PHY_KK + 0x414),
#endif
};

int32_t ISP_BeginGCECallback(uint32_t taskID, uint32_t *regCount,
		uint32_t **regAddress)
{
	pr_info("+,taskID(%d)", taskID);

	*regCount = sizeof(addressToDump) / sizeof(uint32_t);
	*regAddress = (uint32_t *)addressToDump;

	pr_info("-,*regCount(%d)", *regCount);

	return 0;
}

int32_t ISP_EndGCECallback(uint32_t taskID, uint32_t regCount,
		uint32_t *regValues)
{
	#define PER_LINE_LOG_SIZE   10
	int32_t i, j, pos;
	/* uint32_t add[PER_LINE_LOG_SIZE]; */
	uint32_t add[PER_LINE_LOG_SIZE];
	uint32_t val[PER_LINE_LOG_SIZE];

#if DUMP_GCE_TPIPE
	int32_t tpipePA;
	int32_t ctlStart;
	unsigned long map_va = 0;
	uint32_t map_size;
	int32_t *pMapVa;
	#define TPIPE_DUMP_SIZE    200
#endif

	pr_info("End taskID(%d),regCount(%d)", taskID, regCount);

	for (i = 0; i < regCount; i += PER_LINE_LOG_SIZE) {
		for (j = 0; j < PER_LINE_LOG_SIZE; j++) {
			pos = i + j;
			if (pos < regCount) {
				add[j] = addressToDump[pos];
				val[j] = regValues[pos];
			}
		}

		pr_info("[0x%08x,0x%08x][0x%08x,0x%08x][0x%08x,0x%08x][0x%08x,0x%08x][0x%08x,0x%08x]\n",
			add[0], val[0], add[1], val[1], add[2], val[2],
			add[3], val[3], add[4], val[4]);
		pr_info("[0x%08x,0x%08x][0x%08x,0x%08x][0x%08x,0x%08x][0x%08x,0x%08x][0x%08x,0x%08x]\n",
			add[5], val[5], add[6], val[6], add[7], val[7],
			add[8], val[8], add[9], val[9]);
	}

#if DUMP_GCE_TPIPE
	/* tpipePA = ISP_RD32(ISP_IMGSYS_BASE_PHY_KK + 0x204); */
	tpipePA = val[7];
	/* ctlStart = ISP_RD32(ISP_IMGSYS_BASE_PHY_KK + 0x000); */
	ctlStart = val[0];

	pr_info("kk:tpipePA(0x%x), ctlStart(0x%x)", tpipePA, ctlStart);

	if ((tpipePA)) {
#ifdef AEE_DUMP_BY_USING_ION_MEMORY
		tpipePA = tpipePA&0xfffff000;
		struct isp_imem_memory isp_p2GCEdump_imem_buf;

		struct ion_client *isp_p2GCEdump_ion_client;

		isp_p2GCEdump_imem_buf.handle = NULL;
		isp_p2GCEdump_imem_buf.ion_fd = 0;
		isp_p2GCEdump_imem_buf.va = 0;
		isp_p2GCEdump_imem_buf.pa = 0;
		isp_p2GCEdump_imem_buf.length = TPIPE_DUMP_SIZE;
		if ((isp_p2_ion_client == NULL) && (g_ion_device)) {
			isp_p2_ion_client =
				ion_client_create(g_ion_device, "isp_p2");
		}
		if (isp_p2_ion_client == NULL)
			LOG_NOTICE("invalid isp_p2_ion_client client!\n");
		if (isp_allocbuf(&isp_p2GCEdump_imem_buf) >= 0) {
			pMapVa = (int *)isp_p2GCEdump_imem_buf.va;
		pr_info("ctlStart(0x%x),tpipePA(0x%x)", ctlStart, tpipePA);

		if (pMapVa) {
			for (i = 0; i < TPIPE_DUMP_SIZE; i += 10) {
				pr_info("[idx(%d)]%08X-%08X-%08X-%08X-%08X-%08X-%08X-%08X-%08X-%08X",
					i, pMapVa[i], pMapVa[i+1], pMapVa[i+2],
					pMapVa[i+3], pMapVa[i+4],
					pMapVa[i+5], pMapVa[i+6], pMapVa[i+7],
					pMapVa[i+8], pMapVa[i+9]);
			}
		}
			isp_freebuf(&isp_p2GCEdump_imem_buf);
			isp_p2GCEdump_imem_buf.handle = NULL;
			isp_p2GCEdump_imem_buf.ion_fd = 0;
			isp_p2GCEdump_imem_buf.va = 0;
			isp_p2GCEdump_imem_buf.pa = 0;
		}
#endif
	}
#endif

	return 0;
}

/* #define _debug_dma_err_ */
#if defined(_debug_dma_err_)
#define bit(x) (0x1<<(x))

unsigned int DMA_ERR[3 * 12] = {
	bit(1), 0xF50043A8, 0x00000011, /* IMGI */
	bit(2), 0xF50043AC, 0x00000021, /* IMGCI */
	bit(4), 0xF50043B0, 0x00000031, /* LSCI */
	bit(5), 0xF50043B4, 0x00000051, /* FLKI */
	bit(6), 0xF50043B8, 0x00000061, /* LCEI */
	bit(7), 0xF50043BC, 0x00000071, /* VIPI */
	bit(8), 0xF50043C0, 0x00000081, /* VIP2I */
	bit(9), 0xF50043C4, 0x00000194, /* IMGO */
	bit(10), 0xF50043C8, 0x000001a4, /* IMG2O */
	bit(11), 0xF50043CC, 0x000001b4, /* LCSO */
	bit(12), 0xF50043D0, 0x000001c4, /* ESFKO */
	bit(13), 0xF50043D4, 0x000001d4, /* AAO */
};

static signed int DMAErrHandler(void)
{
	unsigned int err_ctrl = ISP_RD32(0xF50043A4);

	pr_info("err_ctrl(0x%08x)", err_ctrl);

	unsigned int i = 0;

	unsigned int *pErr = DMA_ERR;

	for (i = 0; i < 12; i++) {
		unsigned int addr = 0;

#if 1
		if (err_ctrl & (*pErr)) {
			ISP_WR32(0xF5004160, pErr[2]);
			addr = pErr[1];

			pr_info("(0x%08x, 0x%08x), dbg(0x%08x, 0x%08x)",
				addr, ISP_RD32(addr),
				ISP_RD32(0xF5004160), ISP_RD32(0xF5004164));
		}
#else
		addr = pErr[1];
		unsigned int status = ISP_RD32(addr);

		if (status & 0x0000FFFF) {
			ISP_WR32(0xF5004160, pErr[2]);
			addr = pErr[1];

			pr_info("(0x%08x, 0x%08x), dbg(0x%08x, 0x%08x)",
				addr, status,
				ISP_RD32(0xF5004160), ISP_RD32(0xF5004164));
		}
#endif
		pErr = pErr + 3;
	}

}
#endif


void IRQ_INT_ERR_CHECK_CAM(unsigned int WarnStatus, unsigned int ErrStatus,
			enum ISP_IRQ_TYPE_ENUM module)
{
	/* ERR print */
	/* unsigned int i = 0; */
	if (ErrStatus) {
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			g_ISPIntErr[ISP_IRQ_TYPE_INT_CAM_A_ST] |=
				(ErrStatus|WarnStatus);
			g_ISPIntErr_SMI[ISP_IRQ_TYPE_INT_CAM_A_ST] =
				g_ISPIntErr[ISP_IRQ_TYPE_INT_CAM_A_ST];

			LOG_NOTICE(
				"[ISP][ERR_CHECK_CAM]CAM_A:int_err:0x%x_0x%x\n",
				WarnStatus, ErrStatus);

			/* DMA ERR print */
			if (ErrStatus & DMA_ERR_ST)
				ISP_DumpDmaDeepDbg(module);

			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			g_ISPIntErr[ISP_IRQ_TYPE_INT_CAM_B_ST] |=
				(ErrStatus|WarnStatus);
			g_ISPIntErr_SMI[ISP_IRQ_TYPE_INT_CAM_B_ST] =
				g_ISPIntErr[ISP_IRQ_TYPE_INT_CAM_B_ST];

			LOG_NOTICE(
				"[ISP][ERR_CHECK_CAM]CAM_B:int_err:0x%x_0x%x\n",
				WarnStatus, ErrStatus);

			/* DMA ERR print */
			if (ErrStatus & DMA_ERR_ST)
				ISP_DumpDmaDeepDbg(module);

			break;
		case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
			g_ISPIntErr[ISP_IRQ_TYPE_INT_CAMSV_0_ST] |=
				(ErrStatus|WarnStatus);
			g_ISPIntErr_SMI[ISP_IRQ_TYPE_INT_CAMSV_0_ST] =
				g_ISPIntErr[ISP_IRQ_TYPE_INT_CAMSV_0_ST];
			LOG_NOTICE(
				"[ISP][ERR_CHECK_CAM]CAMSV0:int_err:0x%x_0x%x\n",
				WarnStatus, ErrStatus);
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
			g_ISPIntErr[ISP_IRQ_TYPE_INT_CAMSV_1_ST] |=
				(ErrStatus|WarnStatus);
			g_ISPIntErr_SMI[ISP_IRQ_TYPE_INT_CAMSV_1_ST] =
				g_ISPIntErr[ISP_IRQ_TYPE_INT_CAMSV_1_ST];
			LOG_NOTICE(
				"[ISP][ERR_CHECK_CAM]CAMSV1:int_err:0x%x_0x%x\n",
				WarnStatus, ErrStatus);
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
			g_ISPIntErr[ISP_IRQ_TYPE_INT_CAMSV_2_ST] |=
				(ErrStatus|WarnStatus);
			g_ISPIntErr_SMI[ISP_IRQ_TYPE_INT_CAMSV_2_ST] =
				g_ISPIntErr[ISP_IRQ_TYPE_INT_CAMSV_2_ST];
			LOG_NOTICE(
				"[ISP][ERR_CHECK_CAM]CAMSV2:int_err:0x%x_0x%x\n",
				WarnStatus, ErrStatus);
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
			g_ISPIntErr[ISP_IRQ_TYPE_INT_CAMSV_3_ST] |=
				(ErrStatus|WarnStatus);
			g_ISPIntErr_SMI[ISP_IRQ_TYPE_INT_CAMSV_3_ST] =
				g_ISPIntErr[ISP_IRQ_TYPE_INT_CAMSV_3_ST];
			LOG_NOTICE(
				"[ISP][ERR_CHECK_CAM]CAMSV3:int_err:0x%x_0x%x\n",
				WarnStatus, ErrStatus);
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
			g_ISPIntErr[ISP_IRQ_TYPE_INT_CAMSV_4_ST] |=
				(ErrStatus|WarnStatus);
			g_ISPIntErr_SMI[ISP_IRQ_TYPE_INT_CAMSV_4_ST] =
				g_ISPIntErr[ISP_IRQ_TYPE_INT_CAMSV_4_ST];
			LOG_NOTICE(
				"[ISP][ERR_CHECK_CAM]CAMSV4:int_err:0x%x_0x%x\n",
				WarnStatus, ErrStatus);
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
			g_ISPIntErr[ISP_IRQ_TYPE_INT_CAMSV_5_ST] |=
				(ErrStatus|WarnStatus);
			g_ISPIntErr_SMI[ISP_IRQ_TYPE_INT_CAMSV_5_ST] =
				g_ISPIntErr[ISP_IRQ_TYPE_INT_CAMSV_5_ST];
			LOG_NOTICE(
				"[ISP][ERR_CHECK_CAM]CAMSV5:int_err:0x%x_0x%x\n",
				WarnStatus, ErrStatus);
			break;
		case ISP_IRQ_TYPE_INT_DIP_A_ST:
			LOG_NOTICE(
				"[ISP][ERR_CHECK_CAM]DIP_A:int_err:0x%x_0x%x\n",
				WarnStatus, ErrStatus);
			break;
		default:
			break;
		}
	}
}


enum CAM_FrameST Irq_CAM_FrameStatus(enum ISP_DEV_NODE_ENUM module,
	enum ISP_IRQ_TYPE_ENUM irq_mod, unsigned int delayCheck)
{
	unsigned int dma_arry_map[_cam_max_] = {
		/*      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,*/
		0, /* _imgo_*/
		1, /* _rrzo_ */
		2, /* _ufeo_ */
	_cam_max_, /* _aao_ */
	_cam_max_, /* _afo_ */
		3, /* _lcso_ */
	_cam_max_, /* _pdo_ */
		4, /* _eiso_ */
	_cam_max_, /* _flko_ */
		5, /* _rsso_ */
	_cam_max_  /* _pso_ */
	};

	unsigned int dma_en;
	unsigned int uni_dma_en;
	union FBC_CTRL_1 fbc_ctrl1[_cam_max_ + 1];
	union FBC_CTRL_2 fbc_ctrl2[_cam_max_ + 1];
	unsigned int hds2_sel;
	bool bQueMode = MFALSE;
	unsigned int product = 1;
	unsigned int i;

	if (module == ISP_CAM_A_IDX)
		hds2_sel = (ISP_RD32(CAM_UNI_REG_TOP_PATH_SEL(ISP_UNI_A_IDX))
			    & 0x3);
	if (module == ISP_CAM_B_IDX)
		hds2_sel = (ISP_RD32(CAM_UNI_REG_B_TOP_PATH_SEL(ISP_UNI_A_IDX))
			    & 0x3);

	if ((module != ISP_CAM_A_IDX) && (module != ISP_CAM_B_IDX)) {
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

	if (((hds2_sel == 0) && (module == ISP_CAM_A_IDX)) ||
	    ((hds2_sel == 1) && (module == ISP_CAM_B_IDX))) {
		if (module == ISP_CAM_A_IDX)
			uni_dma_en = ISP_RD32(
				CAM_UNI_REG_TOP_DMA_EN(ISP_UNI_A_IDX));
		if (module == ISP_CAM_B_IDX)
			uni_dma_en = ISP_RD32(
				CAM_UNI_REG_B_TOP_DMA_EN(ISP_UNI_A_IDX));

		if (uni_dma_en & 0x4) {
			if (module == ISP_CAM_A_IDX) {
				fbc_ctrl1[dma_arry_map[_eiso_]].Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_EISO_A_CTL1(ISP_UNI_A_IDX));
				fbc_ctrl2[dma_arry_map[_eiso_]].Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_EISO_A_CTL2(ISP_UNI_A_IDX));
			}
			if (module == ISP_CAM_B_IDX) {
				fbc_ctrl1[dma_arry_map[_eiso_]].Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_EISO_B_CTL1(ISP_UNI_A_IDX));
				fbc_ctrl2[dma_arry_map[_eiso_]].Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_EISO_B_CTL2(ISP_UNI_A_IDX));
			}
		} else {
			fbc_ctrl1[dma_arry_map[_eiso_]].Raw = 0x0;
			fbc_ctrl2[dma_arry_map[_eiso_]].Raw = 0x0;
		}

		if (uni_dma_en & 0x8) {
			if (module == ISP_CAM_A_IDX) {
				fbc_ctrl1[dma_arry_map[_rsso_]].Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_RSSO_A_CTL1(ISP_UNI_A_IDX));
				fbc_ctrl2[dma_arry_map[_rsso_]].Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_RSSO_A_CTL2(ISP_UNI_A_IDX));
			}
			if (module == ISP_CAM_B_IDX) {
				fbc_ctrl1[dma_arry_map[_rsso_]].Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_RSSO_B_CTL1(ISP_UNI_A_IDX));
				fbc_ctrl2[dma_arry_map[_rsso_]].Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_RSSO_B_CTL2(ISP_UNI_A_IDX));
			}
		} else {
			fbc_ctrl1[dma_arry_map[_rsso_]].Raw = 0x0;
			fbc_ctrl2[dma_arry_map[_rsso_]].Raw = 0x0;
		}
	} else {
		fbc_ctrl1[dma_arry_map[_eiso_]].Raw = 0x0;
		fbc_ctrl2[dma_arry_map[_eiso_]].Raw = 0x0;
		fbc_ctrl1[dma_arry_map[_rsso_]].Raw = 0x0;
		fbc_ctrl2[dma_arry_map[_rsso_]].Raw = 0x0;
	}

	for (i = 0; i < _cam_max_; i++) {
		if (dma_arry_map[i] !=  _cam_max_) {
			if (fbc_ctrl1[dma_arry_map[i]].Raw != 0) {
				bQueMode =
				    fbc_ctrl1[dma_arry_map[i]].Bits.FBC_MODE;
				break;
			}
		}
	}

	if (bQueMode) {
		for (i = 0; i < _cam_max_; i++) {
			if (dma_arry_map[i] == _cam_max_)
				continue;

			if (fbc_ctrl1[dma_arry_map[i]].Raw != 0) {
				product *=
				    fbc_ctrl2[dma_arry_map[i]].Bits.FBC_CNT;
				if (product == 0)
					return CAM_FST_DROP_FRAME;
			}

		}
	} else {
		for (i = 0; i < _cam_max_; i++) {
			if (dma_arry_map[i] == _cam_max_)
				continue;

			if (fbc_ctrl1[dma_arry_map[i]].Raw != 0) {
				product *=
				    (fbc_ctrl1[dma_arry_map[i]].Bits.FBC_NUM -
				    fbc_ctrl2[dma_arry_map[i]].Bits.FBC_CNT);
				if (product == 0)
					return CAM_FST_DROP_FRAME;
			}

		}
	}

	if (product == 1)
		return CAM_FST_LAST_WORKING_FRAME;
	else
		return CAM_FST_NORMAL;
}

#if (TIMESTAMP_QUEUE_EN == 1)
static void ISP_GetDmaPortsStatus(enum ISP_DEV_NODE_ENUM reg_module,
			unsigned int *DmaPortsStats)
{
	unsigned int dma_en = 0;
	unsigned int hds2_sel;
	unsigned int flk2_sel;
	unsigned int uni_dma_en = 0;

	if (sec_on)
		dma_en = lock_reg.CAM_REG_CTL_DMA_EN[reg_module];
	else
		dma_en = ISP_RD32(CAM_REG_CTL_DMA_EN(reg_module));

	if (reg_module == ISP_CAM_A_IDX) {
		hds2_sel = (ISP_RD32(CAM_UNI_REG_TOP_PATH_SEL(ISP_UNI_A_IDX))
			    & 0x3);
		flk2_sel = ((ISP_RD32(CAM_UNI_REG_TOP_PATH_SEL(ISP_UNI_A_IDX))
			    >> 8) & 0x3);
	}
	if (reg_module == ISP_CAM_B_IDX) {
		hds2_sel = (ISP_RD32(CAM_UNI_REG_B_TOP_PATH_SEL(ISP_UNI_A_IDX))
			    & 0x3);
		flk2_sel = ((ISP_RD32(CAM_UNI_REG_B_TOP_PATH_SEL(ISP_UNI_A_IDX))
			    >> 8) & 0x3);
	}

	DmaPortsStats[_imgo_] = ((dma_en & 0x01) ? 1 : 0);
	DmaPortsStats[_ufeo_] = ((dma_en & 0x02) ? 1 : 0);
	DmaPortsStats[_rrzo_] = ((dma_en & 0x04) ? 1 : 0);
	DmaPortsStats[_lcso_] = ((dma_en & 0x10) ? 1 : 0);

	uni_dma_en = 0;
	if (((hds2_sel == 0) && (reg_module == ISP_CAM_A_IDX)))
		uni_dma_en = ISP_RD32(CAM_UNI_REG_TOP_DMA_EN(ISP_UNI_A_IDX));

	if (((hds2_sel == 1) && (reg_module == ISP_CAM_B_IDX)))
		uni_dma_en = ISP_RD32(CAM_UNI_REG_B_TOP_DMA_EN(ISP_UNI_A_IDX));

	DmaPortsStats[_eiso_] = ((uni_dma_en & 0x04) ? 1 : 0);
	DmaPortsStats[_rsso_] = ((uni_dma_en & 0x08) ? 1 : 0);

	DmaPortsStats[_aao_] = ((dma_en & 0x20) ? 1 : 0);
	DmaPortsStats[_pso_] = ((dma_en & 0x40) ? 1 : 0);
	DmaPortsStats[_afo_] = ((dma_en & 0x08) ? 1 : 0);
	DmaPortsStats[_pdo_] = ((dma_en & 0x400) ? 1 : 0);


	uni_dma_en = 0;
	if (((flk2_sel == 0) && (reg_module == ISP_CAM_A_IDX)))
		uni_dma_en = ISP_RD32(CAM_UNI_REG_TOP_DMA_EN(ISP_UNI_A_IDX));

	if (((flk2_sel == 1) && (reg_module == ISP_CAM_B_IDX)))
		uni_dma_en = ISP_RD32(CAM_UNI_REG_B_TOP_DMA_EN(ISP_UNI_A_IDX));

	DmaPortsStats[_flko_] = ((uni_dma_en & 0x02) ? 1 : 0);

}

static enum CAM_FrameST Irq_CAM_SttFrameStatus(enum ISP_DEV_NODE_ENUM module,
	enum ISP_IRQ_TYPE_ENUM irq_mod, unsigned int dma_id,
					unsigned int delayCheck)
{
	static const signed int dma_arry_map[_cam_max_] = {
		-1, /* _imgo_*/
		-1, /* _rrzo_ */
		-1, /* _ufeo_ */
		 0, /* _aao_ */
		 1, /* _afo_ */
		-1, /* _lcso_ */
		 2, /* _pdo_ */
		-1, /* _eiso_ */
		 3, /* _flko_ */
		-1, /* _rsso_ */
		 4  /* _pso_ */
	};

	unsigned int     dma_en;
	unsigned int     uni_dma_en;
	union FBC_CTRL_1  fbc_ctrl1;
	union FBC_CTRL_2  fbc_ctrl2;
	unsigned int     flk2_sel;
	bool       bQueMode = MFALSE;
	unsigned int     product = 1;
	/* TSTP_V3 unsigned int     frmPeriod = 1; */

	if (module == ISP_CAM_A_IDX)
		flk2_sel = ((ISP_RD32(CAM_UNI_REG_TOP_PATH_SEL(ISP_UNI_A_IDX))
			    >> 8) & 0x3);
	if (module == ISP_CAM_B_IDX)
		flk2_sel = ((ISP_RD32(CAM_UNI_REG_B_TOP_PATH_SEL(ISP_UNI_A_IDX))
			    >> 8) & 0x3);

	switch (module) {
	case ISP_CAM_A_IDX:
	case ISP_CAM_B_IDX:
		if (dma_id >= _cam_max_) {
			LOG_NOTICE(
				"LINE_%d ERROR: unsupported module:0x%x dma:%d\n"
				, __LINE__, module, dma_id);
			return CAM_FST_DROP_FRAME;
		}
		if (dma_arry_map[dma_id] < 0) {
			LOG_NOTICE(
				"LINE_%d ERROR: unsupported module:0x%x dma:%d\n"
				, __LINE__, module, dma_id);
			return CAM_FST_DROP_FRAME;
		}
		break;
	default:
		LOG_NOTICE("LINE_%d ERROR: unsupported module:0x%x dma:%d\n",
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
		if ((flk2_sel == 0) && (module == ISP_CAM_A_IDX)) {
			uni_dma_en = ISP_RD32(
				CAM_UNI_REG_TOP_DMA_EN(ISP_UNI_A_IDX));
			if (uni_dma_en & 0x2) {
				fbc_ctrl1.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_FLKO_A_CTL1(ISP_UNI_A_IDX));
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_FLKO_A_CTL2(ISP_UNI_A_IDX));
			}
		}
		if ((flk2_sel == 1) && (module == ISP_CAM_B_IDX)) {
			uni_dma_en =
			    ISP_RD32(CAM_UNI_REG_B_TOP_DMA_EN(ISP_UNI_A_IDX));
			if (uni_dma_en & 0x2) {
				fbc_ctrl1.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_FLKO_B_CTL1(ISP_UNI_A_IDX));
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_FLKO_B_CTL2(ISP_UNI_A_IDX));
			}
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
			product *= fbc_ctrl2.Bits.FBC_CNT;

			if (product == 0)
				return CAM_FST_DROP_FRAME;
		} else
			return CAM_FST_DROP_FRAME;
	} else {
		if (fbc_ctrl1.Raw != 0) {
			product *=
			    (fbc_ctrl1.Bits.FBC_NUM - fbc_ctrl2.Bits.FBC_CNT);

			if (product == 0)
				return CAM_FST_DROP_FRAME;
		} else
			return CAM_FST_DROP_FRAME;
	}

	if (product == 1)
		return CAM_FST_LAST_WORKING_FRAME;
	else
		return CAM_FST_NORMAL;

}

static int32_t ISP_PushBufTimestamp(unsigned int module, unsigned int dma_id,
	unsigned int sec, unsigned int usec, unsigned int frmPeriod)
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
	default:
		LOG_NOTICE("Unsupport module:x%x\n", module);
		return -EFAULT;
	}

	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
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
		case _eiso_:
			if (reg_module == ISP_CAM_A_IDX)
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_EISO_A_CTL2(ISP_UNI_A_IDX));
			if (reg_module == ISP_CAM_B_IDX)
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_EISO_B_CTL2(ISP_UNI_A_IDX));
			break;
		case _lcso_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_LCSO_CTL2(reg_module));
			break;
		case _rsso_:
			if (reg_module == ISP_CAM_A_IDX)
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_RSSO_A_CTL2(ISP_UNI_A_IDX));
			if (reg_module == ISP_CAM_B_IDX)
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_RSSO_B_CTL2(ISP_UNI_A_IDX));
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
			if (reg_module == ISP_CAM_A_IDX)
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_FLKO_A_CTL2(ISP_UNI_A_IDX));
			if (reg_module == ISP_CAM_B_IDX)
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_FLKO_B_CTL2(ISP_UNI_A_IDX));
			break;
		case _pdo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_PDO_CTL2(reg_module));
			break;
		case _pso_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_PSO_CTL2(reg_module));
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
		fbc_ctrl2.Bits.WCNT = (fbc_ctrl2.Bits.WCNT / frmPeriod) *
					frmPeriod;

	if (((fbc_ctrl2.Bits.WCNT + frmPeriod) & 63) ==
		IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt) {
		IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
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

static int32_t ISP_PopBufTimestamp(unsigned int module, unsigned int dma_id,
			struct S_START_T *pTstp)
{
	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
		switch (dma_id) {
		case _imgo_:
		case _rrzo_:
		case _ufeo_:
		case _eiso_:
		case _lcso_:

		case _flko_:
		case _aao_:
		case _afo_:
		case _rsso_:
		case _pdo_:
		case _pso_:
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
		*pTstp = IspInfo.TstpQInfo[module].Dmao[dma_id].TimeQue
		[IspInfo.TstpQInfo[module].Dmao[dma_id].RdIndex];

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

	pr_info("Wait module:%d dma:%d timestamp ready W/R:%d/%d\n",
	    module, dma_id,
	    (unsigned int)IspInfo.TstpQInfo[module].Dmao[dma_id].TotalWrCnt,
	    (unsigned int)IspInfo.TstpQInfo[module].Dmao[dma_id].TotalRdCnt);

	#if 1
	for (wait_cnt = 3; wait_cnt > 0; wait_cnt--) {
		_timeout = wait_event_interruptible_timeout(
			IspInfo.WaitQueueHead[module],
			(IspInfo.TstpQInfo[module].Dmao[dma_id].TotalWrCnt >
			 IspInfo.TstpQInfo[module].Dmao[dma_id].TotalRdCnt),
			ISP_MsToJiffies(2000));
		/* check if user is interrupted by system signal */
		if ((_timeout != 0) &&
		    (!(IspInfo.TstpQInfo[module].Dmao[dma_id].TotalWrCnt >
		       IspInfo.TstpQInfo[module].Dmao[dma_id].TotalRdCnt))) {
			pr_info(
			"interrupted by system signal, return value(%d)\n",
				_timeout);
			return -ERESTARTSYS;
		}

		if (_timeout > 0)
			break;

		pr_info("WARNING: cam:%d dma:%d wait left count %d\n",
			module, dma_id, wait_cnt);
	}
	if (wait_cnt == 0) {
		LOG_NOTICE("ERROR: cam:%d dma:%d wait timestamp timeout!!!\n",
			module, dma_id);
		return -EFAULT;
	}
	#else
	while (IspInfo.TstpQInfo[module].Dmao[dma_id].TotalWrCnt <=
		IspInfo.TstpQInfo[module].Dmao[dma_id].TotalRdCnt) {

		msleep(20);

if ((wait_cnt & 0x7) == 0x7)
	pr_info("WARNING: module:%d dma:%d wait long %d W/R:%d/%d\n",
		module, dma_id, wait_cnt,
		(unsigned int)
		IspInfo.TstpQInfo[module].Dmao[dma_id].TotalWrCnt,
		(unsigned int)
		IspInfo.TstpQInfo[module].Dmao[dma_id].TotalRdCnt);

if (wait_cnt > 3000) {
	pr_info("ERROR: module:%d dma:%d wait timeout %d W/R:%d/%d\n",
		module, dma_id, wait_cnt,
		(unsigned int)
		IspInfo.TstpQInfo[module].Dmao[dma_id].TotalWrCnt,
		(unsigned int)
		IspInfo.TstpQInfo[module].Dmao[dma_id].TotalRdCnt);
	break;
}

		wait_cnt++;
	}
	#endif

	return 0;
}

static int32_t ISP_CompensateMissingSofTime(enum ISP_DEV_NODE_ENUM reg_module,
			unsigned int module, unsigned int dma_id,
			unsigned int sec, unsigned int usec,
			unsigned int frmPeriod)
{
	union FBC_CTRL_2  fbc_ctrl2;
	unsigned int     delta_wcnt = 0, wridx = 0
	unsigned int     wridx_prev1 = 0, wridx_prev2 = 0, i = 0;
	unsigned int     delta_time = 0, max_delta_time = 0;
	struct S_START_T   time_prev1, time_prev2;
	bool dmao_mask = MFALSE;/*To shrink err log, only rrzo print err log*/
	/*
	 * Patch timestamp and WCNT base on current HW WCNT and
	 * previous SW WCNT value, and calculate difference
	 */

	fbc_ctrl2.Raw = 0;

	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
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
		case _eiso_:
			if (reg_module == ISP_CAM_A_IDX)
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_EISO_A_CTL2(ISP_UNI_A_IDX));
			if (reg_module == ISP_CAM_B_IDX)
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_EISO_B_CTL2(ISP_UNI_A_IDX));
			break;
		case _lcso_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_LCSO_CTL2(reg_module));
			break;
		case _rsso_:
			if (reg_module == ISP_CAM_A_IDX)
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_RSSO_A_CTL2(ISP_UNI_A_IDX));
			if (reg_module == ISP_CAM_B_IDX)
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_RSSO_B_CTL2(ISP_UNI_A_IDX));
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
			if (reg_module == ISP_CAM_A_IDX)
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_FLKO_A_CTL2(ISP_UNI_A_IDX));
			if (reg_module == ISP_CAM_B_IDX)
				fbc_ctrl2.Raw = ISP_RD32(
				    CAM_UNI_REG_FBC_FLKO_B_CTL2(ISP_UNI_A_IDX));
			break;
		case _pdo_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_PDO_CTL2(reg_module));
			break;
		case _pso_:
			fbc_ctrl2.Raw =
				ISP_RD32(CAM_REG_FBC_PSO_CTL2(reg_module));
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
		fbc_ctrl2.Bits.WCNT = (fbc_ctrl2.Bits.WCNT / frmPeriod) *
					frmPeriod;

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
			LOG_NOTICE("ERROR: Cam:%d dma:%d WRONG WCNT:%d_%d_%d\n",
			    module, dma_id, delta_wcnt,
			    IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt,
			    fbc_ctrl2.Bits.WCNT);
		return -EFAULT;
	} else if (delta_wcnt > 6) {
		if (dmao_mask)
			pr_info("WARNING: Cam:%d dma:%d SUSPICIOUS WCNT: %d_%d_%d\n",
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

time_prev1.sec =
	IspInfo.TstpQInfo[module].Dmao[dma_id].TimeQue[wridx_prev1].sec;
time_prev1.usec =
	IspInfo.TstpQInfo[module].Dmao[dma_id].TimeQue[wridx_prev1].usec;

time_prev2.sec =
	IspInfo.TstpQInfo[module].Dmao[dma_id].TimeQue[wridx_prev2].sec;
time_prev2.usec =
	IspInfo.TstpQInfo[module].Dmao[dma_id].TimeQue[wridx_prev2].usec;

	if ((sec > time_prev1.sec) ||
		((sec == time_prev1.sec) && (usec > time_prev1.usec))) {
		max_delta_time = ((sec - time_prev1.sec)*1000000 + usec) -
				 time_prev1.usec;
	} else {
		if (dmao_mask)
			pr_info("ERROR: Cam:%d dma:%d current timestamp: cur: %d.%06d prev1: %d.%06d\n",
				module, dma_id, sec, usec, time_prev1.sec,
				time_prev1.usec);
		max_delta_time = 0;
	}

	if ((time_prev1.sec > time_prev2.sec) ||
		((time_prev1.sec == time_prev2.sec) &&
		 (time_prev1.usec > time_prev2.usec)))
		delta_time = ((time_prev1.sec - time_prev2.sec)*1000000 +
				time_prev1.usec) - time_prev2.usec;
	else {
		if (dmao_mask)
			pr_info(
				"ERROR: Cam:%d dma:%d previous timestamp:prev1: %d.%06d prev2: %d.%06d\n",
				module, dma_id, time_prev1.sec, time_prev1.usec,
				time_prev2.sec, time_prev2.usec);
		delta_time = 0;
	}

	if (delta_time > (max_delta_time / delta_wcnt)) {
		if (dmao_mask)
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"WARNING: Cam:%d dma:%d delta time too large: cur %dus max %dus patch wcnt: %d\n",
				module, dma_id, delta_time, max_delta_time,
				delta_wcnt);
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
		IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
			"Cam:%d dma:%d wcnt:%d_%d_%d T:%d.%06d_.%06d_%d.%06d\n",
			module, dma_id, delta_wcnt,
			IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt,
			fbc_ctrl2.Bits.WCNT, sec, usec, delta_time,
			time_prev1.sec, time_prev1.usec);

	if (IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt !=
	    fbc_ctrl2.Bits.WCNT) {
		if (dmao_mask)
			pr_info(
			    "ERROR: Cam:%d dma:%d strange WCNT SW_HW: %d_%d\n",
			    module, dma_id,
			    IspInfo.TstpQInfo[module].Dmao[dma_id].PrevFbcWCnt,
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
	unsigned int target_wridx = 0, curr_wridx = 0, frm_dt = 0;
	unsigned int last_frm_dt = 0, i = 1;

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
		pr_info("WARNING: timestamp delta too small: %d\n",
		(int)(cur_tstp - prev_tstp));

	i = 0;
while (target_wridx != curr_wridx) {

	if (i > frmPeriod) {
		LOG_NOTICE("Error: too many intpl in sub-sample period %d_%d\n",
			target_wridx, curr_wridx);
		return -EFAULT;
	}

IspInfo.TstpQInfo[module].Dmao[dma_id].TimeQue[target_wridx].usec
	+= (frm_dt * i);

while (IspInfo.TstpQInfo[module].Dmao[dma_id].TimeQue[target_wridx].usec
	>= 1000000) {

	IspInfo.TstpQInfo[module].Dmao[dma_id].TimeQue[target_wridx].usec
		-= 1000000;
	IspInfo.TstpQInfo[module].Dmao[dma_id].TimeQue[target_wridx].sec++;
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

irqreturn_t ISP_Irq_DIP_A(signed int  Irq, void *DeviceId)
{
	int i = 0;
	unsigned int IrqINTStatus = 0x0;
	unsigned int IrqCQStatus = 0x0;
	unsigned int IrqCQLDStatus = 0x0;

	/*pr_info("ISP_Irq_DIP_A:%d\n", Irq);*/

	/* Avoid touch hwmodule when clock is disable. DEVAPC will moniter this
	 * kind of err
	 */
	if (G_u4EnableClockCount == 0)
		return IRQ_HANDLED;

	spin_lock(&(IspInfo.SpinLockIrq[ISP_IRQ_TYPE_INT_DIP_A_ST]));
	IrqINTStatus = ISP_RD32(ISP_DIP_A_BASE + 0x030); /* _INT_STATUS */
	IrqCQStatus = ISP_RD32(ISP_DIP_A_BASE + 0x034); /* _CQ_INT_STATUS */
	IrqCQLDStatus = ISP_RD32(ISP_DIP_A_BASE + 0x038);

	for (i = 0; i < IRQ_USER_NUM_MAX; i++)
		IspInfo.IrqInfo.Status[ISP_IRQ_TYPE_INT_DIP_A_ST][SIGNAL_INT][i]
			|= IrqINTStatus;

	spin_unlock(&(IspInfo.SpinLockIrq[ISP_IRQ_TYPE_INT_DIP_A_ST]));

	/*pr_info("ISP_Irq_DIP_A:%d, reg 0x%p : 0x%x, reg 0x%p : 0x%x\n",
	 * Irq, (ISP_DIP_A_BASE + 0x030), IrqINTStatus,
	 * (ISP_DIP_A_BASE + 0x034), IrqCQStatus);
	 */

	/*  */
	wake_up_interruptible(
		&IspInfo.WaitQueueHead[ISP_IRQ_TYPE_INT_DIP_A_ST]);

	return IRQ_HANDLED;

}

irqreturn_t ISP_Irq_CAMSV_0(signed int  Irq, void *DeviceId)
{
	/* pr_info("ISP_IRQ_CAM_SV:0x%x\n",Irq); */
	unsigned int module = ISP_IRQ_TYPE_INT_CAMSV_0_ST;
	unsigned int reg_module = ISP_CAMSV0_IDX;
	unsigned int i;
	unsigned int IrqStatus, ErrStatus, WarnStatus;

	union FBC_CTRL_1 fbc_ctrl1[2];
	/*  */
	union FBC_CTRL_2 fbc_ctrl2[2];
	/*  */
	unsigned int time_stamp;
	/*  */
	unsigned int cur_v_cnt = 0;
	struct timeval time_frmb;
	unsigned long long  sec = 0;
	unsigned long       usec = 0;

	/* Avoid touch hwmodule when clock is disable. DEVAPC will moniter this
	 * kind of err
	 */
	if (G_u4EnableClockCount == 0)
		return IRQ_HANDLED;

	/*  */
	/* do_gettimeofday(&time_frmb); */
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
	WarnStatus = IrqStatus & IspInfo.IrqInfo.WarnMask[module][SIGNAL_INT];
	IrqStatus = IrqStatus & IspInfo.IrqInfo.Mask[module][SIGNAL_INT];

	/*  */
	IRQ_INT_ERR_CHECK_CAM(WarnStatus, ErrStatus, module);

	fbc_ctrl1[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL1(reg_module));
	fbc_ctrl2[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL2(reg_module));
	time_stamp       = ISP_RD32(CAMSV_REG_TG_TIME_STAMP(reg_module));

	/* sof , done order chech . */
	if ((IrqStatus & SV_HW_PASS1_DON_ST) || (IrqStatus & SV_SOF_INT_ST))
		cur_v_cnt = ISP_RD32_TG_CAM_FRM_CNT(module, reg_module);

	if ((IrqStatus & SV_HW_PASS1_DON_ST) && (IrqStatus & SV_SOF_INT_ST)) {
		if (cur_v_cnt != sof_count[module])
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"isp sof_don block, %d_%d\n", cur_v_cnt,
				sof_count[module]);
	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	if (IrqStatus & SV_SW_PASS1_DON_ST) {
		sec = cpu_clock(0);     /* ns */
		do_div(sec, 1000);    /* usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		/* update pass1 done time stamp for eis user(need match with the
		 * time stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][10] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][10] =
			(unsigned int)(sec);

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV0 P1_DON_%d(0x%08x_0x%08x) ts[0x%08x]\n",
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

#if 0 /* always return CAM_FST_DROP_FRAME for CAMSV0~CAMSV5 */
		/* chk this frame have EOF or not, dynimic dma port chk */
		FrameStatus[module] = Irq_CAM_FrameStatus(reg_module);
		if (FrameStatus[module] == CAM_FST_DROP_FRAME) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV0 Lost p1 done_%d (0x%08x): ",
				sof_count[module], cur_v_cnt);
		}
#endif

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			static unsigned int m_sec = 0, m_usec;
#if 0
			unsigned int magic_num;

			if (pstRTBuf[module]->ring_buf[_camsv_imgo_].active)
				magic_num =
				ISP_RD32(CAMSV_REG_IMGO_FH_SPARE_3(reg_module));
			else
				magic_num =
				ISP_RD32(CAMSV_REG_IMGO_FH_SPARE_3(reg_module));
#endif
			if (g1stSof[module]) {
				m_sec = sec;
				m_usec = usec;
				gSTime[module].sec = sec;
				gSTime[module].usec = usec;
			}

			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV0 P1_SOF_%d_%d(0x%08x_0x%08x,0x%08x),int_us:0x%08x, timestamp[0x%08x]\n",
				sof_count[module], cur_v_cnt,
				ISP_RD32(CAMSV_REG_FBC_IMGO_CTL1(reg_module)),
				ISP_RD32(CAMSV_REG_FBC_IMGO_CTL2(reg_module)),
				ISP_RD32(CAMSV_REG_IMGO_BASE_ADDR(reg_module)),
				(unsigned int)((sec * 1000000 + usec) -
					(1000000 * m_sec + m_usec)),
				time_stamp);

			/* keep current time */
			m_sec = sec;
			m_usec = usec;

			/* dbg information only */
			if (cur_v_cnt !=
			    ((ISP_RD32(CAMSV_REG_TG_INTER_ST(reg_module))
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
					IspInfo.IrqInfo.LastestSigTime_usec
					    [module][cnt] =
						(unsigned int)time_frmb.tv_usec;
					IspInfo.IrqInfo.LastestSigTime_sec
					    [module][cnt] =
						(unsigned int) time_frmb.tv_sec;
					IspInfo.IrqInfo.PassedBySigCnt
					    [module][cnt][i]++;
				}
				tmp = tmp >> 1;
				cnt++;
			}
		} else {
			/* no any interrupt is not marked and in read mask in
			 * this irq type
			 */
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[module]));
	/*  */
	if (IrqStatus & SV_SOF_INT_ST) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCamsv
			[ISP_GetWaitQCamsvIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SV_SOF]);
	}
	if (IrqStatus & SV_SW_PASS1_DON_ST) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCamsv
			[ISP_GetWaitQCamsvIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SV_SW_P1_DONE]);
	}
	wake_up_interruptible(&IspInfo.WaitQueueHead[module]);

	/* dump log, use tasklet */
	if (IrqStatus & (SV_SOF_INT_ST | SV_SW_PASS1_DON_ST | SV_VS1_ST)) {
		#if (ISP_BOTTOMHALF_WORKQ == 1)
		schedule_work(&isp_workque[module].isp_bh_work);
		#else
		tasklet_schedule(isp_tasklet[module].pIsp_tkt);
		#endif
	}

	return IRQ_HANDLED;

}

irqreturn_t ISP_Irq_CAMSV_1(signed int  Irq, void *DeviceId)
{
	/* pr_info("ISP_IRQ_CAM_SV:0x%x\n",Irq); */
	unsigned int module = ISP_IRQ_TYPE_INT_CAMSV_1_ST;
	unsigned int reg_module = ISP_CAMSV1_IDX;
	unsigned int i;
	unsigned int IrqStatus, ErrStatus, WarnStatus;
	union FBC_CTRL_1 fbc_ctrl1[2];
	/* */
	union FBC_CTRL_2 fbc_ctrl2[2];
	/* */
	unsigned int time_stamp;
	/* */
	unsigned int cur_v_cnt = 0;
	struct timeval time_frmb;
	unsigned long long  sec = 0;
	unsigned long       usec = 0;

	/* Avoid touch hwmodule when clock is disable. DEVAPC will moniter this
	 * kind of err
	 */
	if (G_u4EnableClockCount == 0)
		return IRQ_HANDLED;

	/*  */
	/* do_gettimeofday(&time_frmb); */
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
	WarnStatus = IrqStatus & IspInfo.IrqInfo.WarnMask[module][SIGNAL_INT];
	IrqStatus = IrqStatus & IspInfo.IrqInfo.Mask[module][SIGNAL_INT];
	/*  */
	IRQ_INT_ERR_CHECK_CAM(WarnStatus, ErrStatus, module);

	fbc_ctrl1[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL1(reg_module));
	fbc_ctrl2[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL2(reg_module));
	time_stamp       = ISP_RD32(CAMSV_REG_TG_TIME_STAMP(reg_module));

	/* sof , done order chech . */
	if ((IrqStatus & SV_HW_PASS1_DON_ST) || (IrqStatus & SV_SOF_INT_ST))
		cur_v_cnt = ISP_RD32_TG_CAM_FRM_CNT(module, reg_module);

	if ((IrqStatus & SV_HW_PASS1_DON_ST) && (IrqStatus & SV_SOF_INT_ST)) {
		if (cur_v_cnt != sof_count[module])
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"isp sof_don block, %d_%d\n", cur_v_cnt,
				sof_count[module]);
	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	if (IrqStatus & SV_SW_PASS1_DON_ST) {
		sec = cpu_clock(0);     /* ns */
		do_div(sec, 1000);    /* usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		/* update pass1 done time stamp for eis user(need match with the
		 * time stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][10] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][10] =
			(unsigned int)(sec);

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV1 P1_DON_%d(0x%08x_0x%08x) ts[0x%08x]\n",
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

#if 0 /* always return CAM_FST_DROP_FRAME for CAMSV0~CAMSV5 */
		/* chk this frame have EOF or not, dynimic dma port chk */
		FrameStatus[module] = Irq_CAM_FrameStatus(reg_module);
		if (FrameStatus[module] == CAM_FST_DROP_FRAME) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV1 Lost p1 done_%d (0x%08x): ",
				sof_count[module], cur_v_cnt);
		}
#endif

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			static unsigned int m_sec = 0, m_usec;
#if 0
			unsigned int magic_num;

			if (pstRTBuf[module]->ring_buf[_camsv_imgo_].active)
				magic_num =
				ISP_RD32(CAMSV_REG_IMGO_FH_SPARE_3(reg_module));
			else
				magic_num =
				ISP_RD32(CAMSV_REG_IMGO_FH_SPARE_3(reg_module));
#endif
			if (g1stSof[module]) {
				m_sec = sec;
				m_usec = usec;
				gSTime[module].sec = sec;
				gSTime[module].usec = usec;
			}

			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV1 P1_SOF_%d_%d(0x%08x_0x%08x,0x%08x),int_us:0x%08x, timestamp[0x%08x]\n",
				sof_count[module], cur_v_cnt,
				ISP_RD32(CAMSV_REG_FBC_IMGO_CTL1(reg_module)),
				ISP_RD32(CAMSV_REG_FBC_IMGO_CTL2(reg_module)),
				ISP_RD32(CAMSV_REG_IMGO_BASE_ADDR(reg_module)),
				(unsigned int)((sec * 1000000 + usec) -
					(1000000 * m_sec + m_usec)),
				time_stamp);
			/* keep current time */
			m_sec = sec;
			m_usec = usec;

			/* dbg information only */
			if (cur_v_cnt !=
			    ((ISP_RD32(CAMSV_REG_TG_INTER_ST(reg_module))
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
					IspInfo.IrqInfo.LastestSigTime_usec
					    [module][cnt] =
						(unsigned int)time_frmb.tv_usec;
					IspInfo.IrqInfo.LastestSigTime_sec
					    [module][cnt] =
						(unsigned int) time_frmb.tv_sec;
					IspInfo.IrqInfo.PassedBySigCnt
					    [module][cnt][i]++;
				}
				tmp = tmp >> 1;
				cnt++;
			}
		} else {
			/* no any interrupt is not marked and in read mask in
			 * this irq type
			 */
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[module]));
	/*  */
	if (IrqStatus & SV_SOF_INT_ST) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCamsv
			[ISP_GetWaitQCamsvIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SV_SOF]);
	}
	if (IrqStatus & SV_SW_PASS1_DON_ST) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCamsv
			[ISP_GetWaitQCamsvIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SV_SW_P1_DONE]);
	}
	wake_up_interruptible(&IspInfo.WaitQueueHead[module]);

	/* dump log, use tasklet */
	if (IrqStatus & (SV_SOF_INT_ST | SV_SW_PASS1_DON_ST | SV_VS1_ST)) {
		#if (ISP_BOTTOMHALF_WORKQ == 1)
		schedule_work(&isp_workque[module].isp_bh_work);
		#else
		tasklet_schedule(isp_tasklet[module].pIsp_tkt);
		#endif
	}

	return IRQ_HANDLED;

}

irqreturn_t ISP_Irq_CAMSV_2(signed int  Irq, void *DeviceId)
{
	/* pr_info("ISP_IRQ_CAM_SV:0x%x\n",Irq); */
	unsigned int module = ISP_IRQ_TYPE_INT_CAMSV_2_ST;
	unsigned int reg_module = ISP_CAMSV2_IDX;
	unsigned int i;
	unsigned int IrqStatus, ErrStatus, WarnStatus;
	union FBC_CTRL_1 fbc_ctrl1[2];
	/* */
	union FBC_CTRL_2 fbc_ctrl2[2];
	/* */
	unsigned int time_stamp;
	/* */
	unsigned int cur_v_cnt = 0;
	struct timeval time_frmb;
	unsigned long long  sec = 0;
	unsigned long       usec = 0;

	/* Avoid touch hwmodule when clock is disable. DEVAPC will moniter this
	 * kind of err
	 */
	if (G_u4EnableClockCount == 0)
		return IRQ_HANDLED;

	/*  */
	/* do_gettimeofday(&time_frmb); */
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
	WarnStatus = IrqStatus & IspInfo.IrqInfo.WarnMask[module][SIGNAL_INT];
	IrqStatus = IrqStatus & IspInfo.IrqInfo.Mask[module][SIGNAL_INT];
	/*  */
	IRQ_INT_ERR_CHECK_CAM(WarnStatus, ErrStatus, module);

	fbc_ctrl1[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL1(reg_module));
	fbc_ctrl2[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL2(reg_module));
	time_stamp       = ISP_RD32(CAMSV_REG_TG_TIME_STAMP(reg_module));

	/* sof , done order chech . */
	if ((IrqStatus & SV_HW_PASS1_DON_ST) || (IrqStatus & SV_SOF_INT_ST))
		cur_v_cnt = ISP_RD32_TG_CAM_FRM_CNT(module, reg_module);

	if ((IrqStatus & SV_HW_PASS1_DON_ST) && (IrqStatus & SV_SOF_INT_ST)) {
		if (cur_v_cnt != sof_count[module])
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"isp sof_don block, %d_%d\n", cur_v_cnt,
				sof_count[module]);
	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	if (IrqStatus & SV_SW_PASS1_DON_ST) {
		sec = cpu_clock(0);     /* ns */
		do_div(sec, 1000);    /* usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		/* update pass1 done time stamp for eis user(need match with
		 * the time stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][10] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][10] =
			(unsigned int)(sec);

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV2 P1_DON_%d(0x%08x_0x%08x) ts[0x%08x]\n",
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

#if 0 /* always return CAM_FST_DROP_FRAME for CAMSV0~CAMSV5 */
		/* chk this frame have EOF or not, dynimic dma port chk */
		FrameStatus[module] = Irq_CAM_FrameStatus(reg_module);
		if (FrameStatus[module] == CAM_FST_DROP_FRAME) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV2 Lost p1 done_%d (0x%08x): ",
				sof_count[module], cur_v_cnt);
		}
#endif


		if (IspInfo.DebugMask & ISP_DBG_INT) {
			static unsigned int m_sec = 0, m_usec;
#if 0
			unsigned int magic_num;

			if (pstRTBuf[module]->ring_buf[_camsv_imgo_].active)
				magic_num =
				ISP_RD32(CAMSV_REG_IMGO_FH_SPARE_3(reg_module));
			else
				magic_num =
				ISP_RD32(CAMSV_REG_IMGO_FH_SPARE_3(reg_module));
#endif
			if (g1stSof[module]) {
				m_sec = sec;
				m_usec = usec;
				gSTime[module].sec = sec;
				gSTime[module].usec = usec;
			}

			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV2 P1_SOF_%d_%d(0x%08x_0x%08x,0x%08x),int_us:0x%08x, timestamp[0x%08x]\n",
				sof_count[module], cur_v_cnt,
				ISP_RD32(CAMSV_REG_FBC_IMGO_CTL1(reg_module)),
				ISP_RD32(CAMSV_REG_FBC_IMGO_CTL2(reg_module)),
				ISP_RD32(CAMSV_REG_IMGO_BASE_ADDR(reg_module)),
				(unsigned int)((sec * 1000000 + usec) -
					(1000000 * m_sec + m_usec)),
				time_stamp);
			/* keep current time */
			m_sec = sec;
			m_usec = usec;

			/* dbg information only */
			if (cur_v_cnt !=
			    ((ISP_RD32(CAMSV_REG_TG_INTER_ST(reg_module))
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
					IspInfo.IrqInfo.LastestSigTime_usec
					    [module][cnt] =
						(unsigned int)time_frmb.tv_usec;
					IspInfo.IrqInfo.LastestSigTime_sec
					    [module][cnt] =
						(unsigned int) time_frmb.tv_sec;
					IspInfo.IrqInfo.PassedBySigCnt
					    [module][cnt][i]++;
				}
				tmp = tmp >> 1;
				cnt++;
			}
		} else {
			/* no any interrupt is not marked and in read mask
			 * in this irq type
			 */
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[module]));
	/*  */
	if (IrqStatus & SV_SOF_INT_ST) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCamsv
			[ISP_GetWaitQCamsvIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SV_SOF]);
	}
	if (IrqStatus & SV_SW_PASS1_DON_ST) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCamsv
			[ISP_GetWaitQCamsvIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SV_SW_P1_DONE]);
	}
	wake_up_interruptible(&IspInfo.WaitQueueHead[module]);

	/* dump log, use tasklet */
	if (IrqStatus & (SV_SOF_INT_ST | SV_SW_PASS1_DON_ST | SV_VS1_ST)) {
		#if (ISP_BOTTOMHALF_WORKQ == 1)
		schedule_work(&isp_workque[module].isp_bh_work);
		#else
		tasklet_schedule(isp_tasklet[module].pIsp_tkt);
		#endif
	}

	return IRQ_HANDLED;
}

irqreturn_t ISP_Irq_CAMSV_3(signed int  Irq, void *DeviceId)
{
	/* pr_info("ISP_IRQ_CAM_SV:0x%x\n",Irq); */
	unsigned int module = ISP_IRQ_TYPE_INT_CAMSV_3_ST;
	unsigned int reg_module = ISP_CAMSV3_IDX;
	unsigned int i;
	unsigned int IrqStatus, ErrStatus, WarnStatus;
	union FBC_CTRL_1 fbc_ctrl1[2];
	/* */
	union FBC_CTRL_2 fbc_ctrl2[2];
	/* */
	unsigned int time_stamp;
	/* */
	unsigned int cur_v_cnt = 0;
	struct timeval time_frmb;
	unsigned long long  sec = 0;
	unsigned long       usec = 0;

	/* Avoid touch hwmodule when clock is disable.
	 * DEVAPC will moniter this kind of err
	 */
	if (G_u4EnableClockCount == 0)
		return IRQ_HANDLED;

	/*  */
	/* do_gettimeofday(&time_frmb); */
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
	WarnStatus = IrqStatus & IspInfo.IrqInfo.WarnMask[module][SIGNAL_INT];
	IrqStatus = IrqStatus & IspInfo.IrqInfo.Mask[module][SIGNAL_INT];
	/*  */
	IRQ_INT_ERR_CHECK_CAM(WarnStatus, ErrStatus, module);

	fbc_ctrl1[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL1(reg_module));
	fbc_ctrl2[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL2(reg_module));
	time_stamp       = ISP_RD32(CAMSV_REG_TG_TIME_STAMP(reg_module));

	/* sof , done order chech . */
	if ((IrqStatus & SV_HW_PASS1_DON_ST) || (IrqStatus & SV_SOF_INT_ST))
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
		/* update pass1 done time stamp for eis user(need match with
		 * the time stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][10] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][10] =
			(unsigned int)(sec);

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV3 P1_DON_%d(0x%08x_0x%08x) ts[0x%08x]\n",
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

#if 0 /* always return CAM_FST_DROP_FRAME for CAMSV0~CAMSV5 */
		/* chk this frame have EOF or not, dynimic dma port chk */
		FrameStatus[module] = Irq_CAM_FrameStatus(reg_module);
		if (FrameStatus[module] == CAM_FST_DROP_FRAME) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV3 Lost p1 done_%d (0x%08x): ",
				sof_count[module], cur_v_cnt);
		}
#endif

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			static unsigned int m_sec = 0, m_usec;
#if 0
			unsigned int magic_num;

			if (pstRTBuf[module]->ring_buf[_camsv_imgo_].active)
				magic_num =
				ISP_RD32(CAMSV_REG_IMGO_FH_SPARE_3(reg_module));
			else
				magic_num =
				ISP_RD32(CAMSV_REG_IMGO_FH_SPARE_3(reg_module));
#endif
			if (g1stSof[module]) {
				m_sec = sec;
				m_usec = usec;
				gSTime[module].sec = sec;
				gSTime[module].usec = usec;
			}

			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV3 P1_SOF_%d_%d(0x%08x_0x%08x,0x%08x),int_us:0x%08x, timestamp[0x%08x]\n",
				sof_count[module], cur_v_cnt,
				ISP_RD32(CAMSV_REG_FBC_IMGO_CTL1(reg_module)),
				ISP_RD32(CAMSV_REG_FBC_IMGO_CTL2(reg_module)),
				ISP_RD32(CAMSV_REG_IMGO_BASE_ADDR(reg_module)),
				(unsigned int)((sec * 1000000 + usec) -
					(1000000 * m_sec + m_usec)),
				time_stamp);
			/* keep current time */
			m_sec = sec;
			m_usec = usec;

			/* dbg information only */
			if (cur_v_cnt !=
			    ((ISP_RD32(CAMSV_REG_TG_INTER_ST(reg_module))
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
					IspInfo.IrqInfo.LastestSigTime_usec
					    [module][cnt] =
						(unsigned int)time_frmb.tv_usec;
					IspInfo.IrqInfo.LastestSigTime_sec
					    [module][cnt] =
						(unsigned int) time_frmb.tv_sec;
					IspInfo.IrqInfo.PassedBySigCnt
					    [module][cnt][i]++;
				}
				tmp = tmp >> 1;
				cnt++;
			}
		} else {
			/* no any interrupt is not marked and  in read mask in
			 * this irq type
			 */
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[module]));
	/*  */
	if (IrqStatus & SV_SOF_INT_ST) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCamsv
			[ISP_GetWaitQCamsvIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SV_SOF]);
	}
	if (IrqStatus & SV_SW_PASS1_DON_ST) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCamsv
			[ISP_GetWaitQCamsvIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SV_SW_P1_DONE]);
	}
	wake_up_interruptible(&IspInfo.WaitQueueHead[module]);

	/* dump log, use tasklet */
	if (IrqStatus & (SV_SOF_INT_ST | SV_SW_PASS1_DON_ST | SV_VS1_ST)) {
		#if (ISP_BOTTOMHALF_WORKQ == 1)
		schedule_work(&isp_workque[module].isp_bh_work);
		#else
		tasklet_schedule(isp_tasklet[module].pIsp_tkt);
		#endif
	}

	return IRQ_HANDLED;
}

irqreturn_t ISP_Irq_CAMSV_4(signed int  Irq, void *DeviceId)
{
	/* pr_info("ISP_IRQ_CAM_SV:0x%x\n",Irq); */
	unsigned int module = ISP_IRQ_TYPE_INT_CAMSV_4_ST;
	unsigned int reg_module = ISP_CAMSV4_IDX;
	unsigned int i;
	unsigned int IrqStatus, ErrStatus, WarnStatus;
	union FBC_CTRL_1 fbc_ctrl1[2];
	/* */
	union FBC_CTRL_2 fbc_ctrl2[2];
	/* */
	unsigned int time_stamp;
	/* */
	unsigned int cur_v_cnt = 0;
	struct timeval time_frmb;
	unsigned long long  sec = 0;
	unsigned long       usec = 0;

	/* Avoid touch hwmodule when clock is disable. DEVAPC will moniter
	 * this kind of err
	 */
	if (G_u4EnableClockCount == 0)
		return IRQ_HANDLED;

	/*  */
	/* do_gettimeofday(&time_frmb); */
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
	WarnStatus = IrqStatus & IspInfo.IrqInfo.WarnMask[module][SIGNAL_INT];
	IrqStatus = IrqStatus & IspInfo.IrqInfo.Mask[module][SIGNAL_INT];
	/*  */
	IRQ_INT_ERR_CHECK_CAM(WarnStatus, ErrStatus, module);

	fbc_ctrl1[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL1(reg_module));
	fbc_ctrl2[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL2(reg_module));
	time_stamp       = ISP_RD32(CAMSV_REG_TG_TIME_STAMP(reg_module));

	/* sof , done order chech . */
	if ((IrqStatus & SV_HW_PASS1_DON_ST) || (IrqStatus & SV_SOF_INT_ST))
		cur_v_cnt = ISP_RD32_TG_CAM_FRM_CNT(module, reg_module);

	if ((IrqStatus & SV_HW_PASS1_DON_ST) && (IrqStatus & SV_SOF_INT_ST)) {
		if (cur_v_cnt != sof_count[module])
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"isp sof_don block, %d_%d\n", cur_v_cnt,
				sof_count[module]);
	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	if (IrqStatus & SV_SW_PASS1_DON_ST) {
		sec = cpu_clock(0);     /* ns */
		do_div(sec, 1000);    /* usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		/* update pass1 done time stamp for eis user(need match with the
		 * time stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][10] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][10] =
			(unsigned int)(sec);

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV4 P1_DON_%d(0x%08x_0x%08x) ts[0x%08x]\n",
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

#if 0 /* always return CAM_FST_DROP_FRAME for CAMSV0~CAMSV5 */
		/* chk this frame have EOF or not, dynimic dma port chk */
		FrameStatus[module] = Irq_CAM_FrameStatus(reg_module);
		if (FrameStatus[module] == CAM_FST_DROP_FRAME) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV4 Lost p1 done_%d (0x%08x): ",
				sof_count[module], cur_v_cnt);
		}
#endif

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			static unsigned int m_sec = 0, m_usec;
#if 0
			unsigned int magic_num;

			if (pstRTBuf[module]->ring_buf[_camsv_imgo_].active)
				magic_num =
				ISP_RD32(CAMSV_REG_IMGO_FH_SPARE_3(reg_module));
			else
				magic_num =
				ISP_RD32(CAMSV_REG_IMGO_FH_SPARE_3(reg_module));
#endif
			if (g1stSof[module]) {
				m_sec = sec;
				m_usec = usec;
				gSTime[module].sec = sec;
				gSTime[module].usec = usec;
			}

			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV4 P1_SOF_%d_%d(0x%08x_0x%08x,0x%08x),int_us:0x%08x, timestamp[0x%08x]\n",
				sof_count[module], cur_v_cnt,
				ISP_RD32(CAMSV_REG_FBC_IMGO_CTL1(reg_module)),
				ISP_RD32(CAMSV_REG_FBC_IMGO_CTL2(reg_module)),
				ISP_RD32(CAMSV_REG_IMGO_BASE_ADDR(reg_module)),
				(unsigned int)((sec * 1000000 + usec) -
					(1000000 * m_sec + m_usec)),
				time_stamp);
			/* keep current time */
			m_sec = sec;
			m_usec = usec;

			/* dbg information only */
			if (cur_v_cnt !=
			    ((ISP_RD32(CAMSV_REG_TG_INTER_ST(reg_module))
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
		if (IspInfo.IrqInfo.MarkedFlag[module][SIGNAL_INT][i]&
		    IspInfo.IrqInfo.Mask[module][SIGNAL_INT]) {
			unsigned int cnt = 0, tmp = IrqStatus;

			while (tmp) {
				if (tmp & 0x1) {
					IspInfo.IrqInfo.LastestSigTime_usec
					    [module][cnt] =
						(unsigned int)time_frmb.tv_usec;
					IspInfo.IrqInfo.LastestSigTime_sec
					    [module][cnt] =
						(unsigned int) time_frmb.tv_sec;
					IspInfo.IrqInfo.PassedBySigCnt
					    [module][cnt][i]++;
				}
				tmp = tmp >> 1;
				cnt++;
			}
		} else {
			/* no any interrupt is not marked and in read mask in
			 * this irq type
			 */
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[module]));
	/*  */
	if (IrqStatus & SV_SOF_INT_ST) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCamsv
			[ISP_GetWaitQCamsvIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SV_SOF]);
	}
	if (IrqStatus & SV_SW_PASS1_DON_ST) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCamsv
			[ISP_GetWaitQCamsvIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SV_SW_P1_DONE]);
	}
	wake_up_interruptible(&IspInfo.WaitQueueHead[module]);

	/* dump log, use tasklet */
	if (IrqStatus & (SV_SOF_INT_ST | SV_SW_PASS1_DON_ST | SV_VS1_ST)) {
		#if (ISP_BOTTOMHALF_WORKQ == 1)
		schedule_work(&isp_workque[module].isp_bh_work);
		#else
		tasklet_schedule(isp_tasklet[module].pIsp_tkt);
		#endif
	}

	return IRQ_HANDLED;

}

irqreturn_t ISP_Irq_CAMSV_5(signed int  Irq, void *DeviceId)
{
	/* pr_info("ISP_IRQ_CAM_SV:0x%x\n",Irq); */
	unsigned int module = ISP_IRQ_TYPE_INT_CAMSV_5_ST;
	unsigned int reg_module = ISP_CAMSV5_IDX;
	unsigned int i;
	unsigned int IrqStatus, ErrStatus, WarnStatus;
	union FBC_CTRL_1 fbc_ctrl1[2];
	/* */
	union FBC_CTRL_2 fbc_ctrl2[2];
	/* */
	unsigned int time_stamp;
	/* */
	unsigned int cur_v_cnt = 0;
	struct timeval time_frmb;
	unsigned long long  sec = 0;
	unsigned long       usec = 0;

	/* Avoid touch hwmodule when clock is disable.
	 * DEVAPC will moniter this kind of err
	 */
	if (G_u4EnableClockCount == 0)
		return IRQ_HANDLED;

	/*  */
	/* do_gettimeofday(&time_frmb); */
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
	WarnStatus = IrqStatus & IspInfo.IrqInfo.WarnMask[module][SIGNAL_INT];
	IrqStatus = IrqStatus & IspInfo.IrqInfo.Mask[module][SIGNAL_INT];
	/*  */
	IRQ_INT_ERR_CHECK_CAM(WarnStatus, ErrStatus, module);

	fbc_ctrl1[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL1(reg_module));
	fbc_ctrl2[0].Raw = ISP_RD32(CAMSV_REG_FBC_IMGO_CTL2(reg_module));
	time_stamp       = ISP_RD32(CAMSV_REG_TG_TIME_STAMP(reg_module));

	/* sof , done order chech . */
	if ((IrqStatus & SV_HW_PASS1_DON_ST) || (IrqStatus & SV_SOF_INT_ST))
		cur_v_cnt = ISP_RD32_TG_CAM_FRM_CNT(module, reg_module);

	if ((IrqStatus & SV_HW_PASS1_DON_ST) && (IrqStatus & SV_SOF_INT_ST)) {
		if (cur_v_cnt != sof_count[module])
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"isp sof_don block, %d_%d\n", cur_v_cnt,
				sof_count[module]);
	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	if (IrqStatus & SV_SW_PASS1_DON_ST) {
		sec = cpu_clock(0);     /* ns */
		do_div(sec, 1000);    /* usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		/* update pass1 done time stamp for eis user(need match with the
		 * time stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][10] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][10] =
			(unsigned int)(sec);

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV5 P1_DON_%d(0x%08x_0x%08x) ts[0x%08x]\n",
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

#if 0 /* always return CAM_FST_DROP_FRAME for CAMSV0~CAMSV5 */
		/* chk this frame have EOF or not, dynimic dma port chk */
		FrameStatus[module] = Irq_CAM_FrameStatus(reg_module);
		if (FrameStatus[module] == CAM_FST_DROP_FRAME) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV5 Lost p1 done_%d (0x%08x): ",
				sof_count[module], cur_v_cnt);
		}
#endif

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			static unsigned int m_sec = 0, m_usec;
#if 0
			unsigned int magic_num;

			if (pstRTBuf[module]->ring_buf[_camsv_imgo_].active)
				magic_num =
				ISP_RD32(CAMSV_REG_IMGO_FH_SPARE_3(reg_module));
			else
				magic_num =
				ISP_RD32(CAMSV_REG_IMGO_FH_SPARE_3(reg_module));
#endif
			if (g1stSof[module]) {
				m_sec = sec;
				m_usec = usec;
				gSTime[module].sec = sec;
				gSTime[module].usec = usec;
			}

			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMSV5 P1_SOF_%d_%d(0x%08x_0x%08x,0x%08x),int_us:0x%08x,ts[0x%08x]\n",
				sof_count[module], cur_v_cnt,
				ISP_RD32(CAMSV_REG_FBC_IMGO_CTL1(reg_module)),
				ISP_RD32(CAMSV_REG_FBC_IMGO_CTL2(reg_module)),
				ISP_RD32(CAMSV_REG_IMGO_BASE_ADDR(reg_module)),
				(unsigned int)((sec * 1000000 + usec) -
					(1000000 * m_sec + m_usec)),
				time_stamp);
			/* keep current time */
			m_sec = sec;
			m_usec = usec;

			/* dbg information only */
			if (cur_v_cnt !=
			    ((ISP_RD32(CAMSV_REG_TG_INTER_ST(reg_module))
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
					IspInfo.IrqInfo.LastestSigTime_usec
					    [module][cnt] =
						(unsigned int)time_frmb.tv_usec;
					IspInfo.IrqInfo.LastestSigTime_sec
					    [module][cnt] =
						(unsigned int) time_frmb.tv_sec;
					IspInfo.IrqInfo.PassedBySigCnt
					    [module][cnt][i]++;
				}
				tmp = tmp >> 1;
				cnt++;
			}
		} else {
			/* no any interrupt is not marked and in read mask in
			 * this irq type
			 */
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[module]));
	/*  */
	if (IrqStatus & SV_SOF_INT_ST) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCamsv
			[ISP_GetWaitQCamsvIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SV_SOF]);
	}
	if (IrqStatus & SV_SW_PASS1_DON_ST) {
		wake_up_interruptible(
			&IspInfo.WaitQHeadCamsv
			[ISP_GetWaitQCamsvIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SV_SW_P1_DONE]);
	}
	wake_up_interruptible(&IspInfo.WaitQueueHead[module]);

	/* dump log, use tasklet */
	if (IrqStatus & (SV_SOF_INT_ST | SV_SW_PASS1_DON_ST | SV_VS1_ST)) {
		#if (ISP_BOTTOMHALF_WORKQ == 1)
		schedule_work(&isp_workque[module].isp_bh_work);
		#else
		tasklet_schedule(isp_tasklet[module].pIsp_tkt);
		#endif
	}

	return IRQ_HANDLED;

}

irqreturn_t ISP_Irq_CAM_A(signed int Irq, void *DeviceId)
{
	unsigned int module = ISP_IRQ_TYPE_INT_CAM_A_ST;
	unsigned int reg_module = ISP_CAM_A_IDX;
	unsigned int i;
	unsigned int IrqStatus, ErrStatus, WarnStatus;
	unsigned int DmaStatus;
	union FBC_CTRL_1 fbc_ctrl1[2];
	union FBC_CTRL_2 fbc_ctrl2[2];
	unsigned int cur_v_cnt = 0;
	struct timeval time_frmb;
	unsigned long long  sec = 0;
	unsigned long       usec = 0;
	unsigned int IrqEnableOrig, IrqEnableNew;

	/* Avoid touch hwmodule when clock is disable. DEVAPC will moniter this
	 * kind of err
	 */
	if (G_u4EnableClockCount == 0)
		return IRQ_HANDLED;

	/*	*/
	/* do_gettimeofday(&time_frmb); */
	sec = cpu_clock(0);  /* ns */
	do_div(sec, 1000);    /* usec */
	usec = do_div(sec, 1000000);    /* sec and usec */
	time_frmb.tv_usec = usec;
	time_frmb.tv_sec = sec;

	#if (ISP_BOTTOMHALF_WORKQ == 1)
	gSvLog[module]._lastIrqTime.sec = sec;
	gSvLog[module]._lastIrqTime.usec = usec;
	#endif

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	IrqStatus = ISP_RD32(CAM_REG_CTL_RAW_INT_STATUS(reg_module));
	DmaStatus = ISP_RD32(CAM_REG_CTL_RAW_INT2_STATUS(reg_module));
	spin_unlock(&(IspInfo.SpinLockIrq[module]));

	ErrStatus = IrqStatus & IspInfo.IrqInfo.ErrMask[module][SIGNAL_INT];
	WarnStatus = IrqStatus & IspInfo.IrqInfo.WarnMask[module][SIGNAL_INT];
	IrqStatus = IrqStatus & IspInfo.IrqInfo.Mask[module][SIGNAL_INT];

	/* Check ERR/WRN ISR times, if it occur too frequently, mark it for
	 * avoding keep enter ISR. It will happen KE
	 */
	for (i = 0; i < ISP_ISR_MAX_NUM; i++) {
		/* Only check irq that un marked yet */
		if (!((IspInfo.IrqCntInfo.m_err_int_mark[module] & (1 << i))
		  || (IspInfo.IrqCntInfo.m_warn_int_mark[module] & (1 << i)))) {

			if (ErrStatus & (1 << i))
				IspInfo.IrqCntInfo.m_err_int_cnt[module][i]++;

			if (WarnStatus & (1 << i))
				IspInfo.IrqCntInfo.m_warn_int_cnt[module][i]++;


			if (usec - IspInfo.IrqCntInfo.m_int_usec[module]
			    < INT_ERR_WARN_TIMER_THREAS) {
				if (IspInfo.IrqCntInfo.m_err_int_cnt[module][i]
				    >= INT_ERR_WARN_MAX_TIME)
					IspInfo.IrqCntInfo.m_err_int_mark
					    [module] |= (1 << i);

				if (IspInfo.IrqCntInfo.m_warn_int_cnt[module][i]
				    >= INT_ERR_WARN_MAX_TIME)
					IspInfo.IrqCntInfo.m_warn_int_mark
					    [module] |= (1 << i);

			} else {
				IspInfo.IrqCntInfo.m_int_usec[module] = usec;
				IspInfo.IrqCntInfo.m_err_int_cnt[module][i] = 0;
				IspInfo.IrqCntInfo.m_warn_int_cnt[module][i] =
									      0;
			}
		}

	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	IrqEnableOrig = ISP_RD32(CAM_REG_CTL_RAW_INT_EN(reg_module));
	spin_unlock(&(IspInfo.SpinLockIrq[module]));

	IrqEnableNew = IrqEnableOrig &
		~(IspInfo.IrqCntInfo.m_err_int_mark[module]
		| IspInfo.IrqCntInfo.m_warn_int_mark[module]);
	ISP_WR32(CAM_REG_CTL_RAW_INT_EN(reg_module), IrqEnableNew);

	/*	*/
	IRQ_INT_ERR_CHECK_CAM(WarnStatus, ErrStatus, module);

	fbc_ctrl1[0].Raw = ISP_RD32(CAM_REG_FBC_IMGO_CTL1(reg_module));
	fbc_ctrl1[1].Raw = ISP_RD32(CAM_REG_FBC_RRZO_CTL1(reg_module));
	fbc_ctrl2[0].Raw = ISP_RD32(CAM_REG_FBC_IMGO_CTL2(reg_module));
	fbc_ctrl2[1].Raw = ISP_RD32(CAM_REG_FBC_RRZO_CTL2(reg_module));

	#if defined(ISP_MET_READY)
	/*MET:ISP EOF*/
	if (IrqStatus & SW_PASS1_DON_ST) {
		if (met_mmsys_event_isp_pass1_end)
			met_mmsys_event_isp_pass1_end(0);
	}

	if (IrqStatus & SOF_INT_ST) {
		/*met mmsys profile*/
		if (met_mmsys_event_isp_pass1_begin)
			met_mmsys_event_isp_pass1_begin(0);
	}
	#endif

	/* sof , done order chech . */
	if ((IrqStatus & HW_PASS1_DON_ST) || (IrqStatus & SOF_INT_ST))
		cur_v_cnt = ISP_RD32_TG_CAM_FRM_CNT(module, reg_module);

	if ((IrqStatus & HW_PASS1_DON_ST) && (IrqStatus & SOF_INT_ST)) {
		if (cur_v_cnt != sof_count[module])
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"isp sof_don block, %d_%d\n",
				cur_v_cnt, sof_count[module]);
	}

	if ((IrqStatus & HW_PASS1_DON_ST) &&
	    (IspInfo.DebugMask & ISP_DBG_HW_DON)) {
		IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
			    "CAMA P1_HW_DON_%d\n",
			    (sof_count[module]) ?
			    (sof_count[module] - 1) : (sof_count[module]));
	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	if (IrqStatus & VS_INT_ST) {
		Vsync_cnt[0]++;
		/*pr_info("CAMA N3D:0x%x\n", Vsync_cnt[0]);*/
	}
	if (IrqStatus & SW_PASS1_DON_ST) {
		sec = ktime_get();
		do_div(sec, 1000);	  /* usec */
		usec = do_div(sec, 1000000);	/* sec and usec */
		/* update pass1 done time stamp for eis user(need match with
		 * the time stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][10] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][10] =
			(unsigned int)(sec);

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			/*SW p1_don is not reliable*/
			if (FrameStatus[module] != CAM_FST_DROP_FRAME) {
				IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
					"CAMA P1_DON_%d(0x%x_0x%x,0x%x_0x%x)\n",
					(sof_count[module]) ?
					  (sof_count[module] - 1) :
					  (sof_count[module]),
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
			unsigned int frmPeriod = ((ISP_RD32(
				CAM_REG_TG_SUB_PERIOD(reg_module)) >> 8)
				& 0x1F) + 1;

			if (frmPeriod > 1) {
				ISP_PatchTimestamp(module, _imgo_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _rrzo_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _ufeo_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _eiso_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _lcso_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _rsso_, frmPeriod,
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
	}

	if (IrqStatus & SOF_INT_ST) {
		unsigned int frmPeriod =
			((ISP_RD32(CAM_REG_TG_SUB_PERIOD(reg_module)) >> 8)
			 & 0x1F) + 1;
		unsigned int irqDelay = 0;

		sec = ktime_get();
		do_div(sec, 1000);    /* usec */
		usec = do_div(sec, 1000000);    /* sec and usec */

		if (frmPeriod == 0) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
				"ERROR: Wrong sub-sample period: 0");
			goto LB_CAMA_SOF_IGNORE;
		}

		/* chk this frame have EOF or not, dynimic dma port chk */
		FrameStatus[module] =
			Irq_CAM_FrameStatus(reg_module, module, irqDelay);

		if (FrameStatus[module] == CAM_FST_DROP_FRAME) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMA Lost p1 done_%d (0x%x): ",
				sof_count[module], cur_v_cnt);
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
			static unsigned int m_sec = 0, m_usec;
			unsigned int magic_num;

			if (pstRTBuf[module]->ring_buf[_imgo_].active)
				magic_num =
				  ISP_RD32(CAM_REG_IMGO_FH_SPARE_2(reg_module));
			else
				magic_num =
				  ISP_RD32(CAM_REG_RRZO_FH_SPARE_2(reg_module));

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
			enum CAM_FrameST FrmStat_aao, FrmStat_afo, FrmStat_flko;
			enum CAM_FrameST FrmStat_pdo, FrmStat_pso;

			ISP_GetDmaPortsStatus(reg_module,
					IspInfo.TstpQInfo[module].DmaEnStatus);

			/* Prevent WCNT increase after
			 * ISP_CompensateMissingSofTime around P1_DON
			 * and FBC_CNT decrease to 0, following drop frame is
			 * checked becomes true, then SOF timestamp will
			 * missing for current frame
			 */
			if (IspInfo.TstpQInfo[module].DmaEnStatus[_aao_])
				FrmStat_aao = Irq_CAM_SttFrameStatus(reg_module,
						module, _aao_, irqDelay);
			else
				FrmStat_aao = CAM_FST_DROP_FRAME;

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_afo_])
				FrmStat_afo = Irq_CAM_SttFrameStatus(reg_module,
						module, _afo_, irqDelay);
			else
				FrmStat_afo = CAM_FST_DROP_FRAME;

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_flko_])
				FrmStat_flko = Irq_CAM_SttFrameStatus(reg_module
						, module, _flko_, irqDelay);
			else
				FrmStat_flko = CAM_FST_DROP_FRAME;

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pdo_])
				FrmStat_pdo = Irq_CAM_SttFrameStatus(reg_module,
						module, _pdo_, irqDelay);
			else
				FrmStat_pdo = CAM_FST_DROP_FRAME;
			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pso_])
				FrmStat_pso = Irq_CAM_SttFrameStatus(reg_module,
						module, _pso_, irqDelay);
			else
				FrmStat_pso = CAM_FST_DROP_FRAME;

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_imgo_])
				ISP_CompensateMissingSofTime(reg_module,
					module, _imgo_, sec, usec, frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_rrzo_])
				ISP_CompensateMissingSofTime(reg_module,
					module, _rrzo_, sec, usec, frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_ufeo_])
				ISP_CompensateMissingSofTime(reg_module,
					module, _ufeo_, sec, usec, frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_eiso_])
				ISP_CompensateMissingSofTime(reg_module,
					module, _eiso_, sec, usec, frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_rsso_])
				ISP_CompensateMissingSofTime(reg_module,
					module, _rsso_, sec, usec, frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_lcso_])
				ISP_CompensateMissingSofTime(reg_module,
					module, _lcso_, sec, usec, frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_aao_])
				ISP_CompensateMissingSofTime(reg_module,
					module, _aao_, sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_afo_])
				ISP_CompensateMissingSofTime(reg_module,
					module, _afo_, sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_flko_])
				ISP_CompensateMissingSofTime(reg_module,
					module, _flko_, sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pdo_])
				ISP_CompensateMissingSofTime(reg_module,
					module, _pdo_, sec, usec, 1);
			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pso_])
				ISP_CompensateMissingSofTime(reg_module,
					module, _pso_, sec, usec, 1);

if (FrameStatus[module] != CAM_FST_DROP_FRAME) {
	for (subFrm = 0; subFrm < frmPeriod; subFrm++) {
		/* Current frame is NOT DROP FRAME */
		if (IspInfo.TstpQInfo[module].DmaEnStatus[_imgo_])
			ISP_PushBufTimestamp(module,
				_imgo_, sec, usec, frmPeriod);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_rrzo_])
			ISP_PushBufTimestamp(module,
				_rrzo_, sec, usec, frmPeriod);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_ufeo_])
			ISP_PushBufTimestamp(module,
				_ufeo_, sec, usec, frmPeriod);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_eiso_])
			ISP_PushBufTimestamp(module,
				_eiso_, sec, usec, frmPeriod);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_rsso_])
			ISP_PushBufTimestamp(module,
				_rsso_, sec, usec, frmPeriod);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_lcso_])
			ISP_PushBufTimestamp(module,
				_lcso_, sec, usec, frmPeriod);
	}

	/* for slow motion sub-sample */
	/* must after current ISP_PushBufTimestamp() */
	#if (TSTMP_SUBSAMPLE_INTPL == 1)
	if ((frmPeriod > 1) &&
			(g1stSwP1Done[module] == MFALSE)) {
		if (IspInfo.TstpQInfo[module].DmaEnStatus[_imgo_])
			ISP_PatchTimestamp(module,
					_imgo_, frmPeriod,
					cur_timestp,
					gPrevSofTimestp[module]);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_rrzo_])
			ISP_PatchTimestamp(module,
					_rrzo_, frmPeriod,
					cur_timestp,
					gPrevSofTimestp[module]);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_ufeo_])
			ISP_PatchTimestamp(module,
					_ufeo_, frmPeriod,
					cur_timestp,
					gPrevSofTimestp[module]);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_eiso_])
			ISP_PatchTimestamp(module,
					_eiso_, frmPeriod,
					cur_timestp,
					gPrevSofTimestp[module]);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_rsso_])
			ISP_PatchTimestamp(module,
					_rsso_, frmPeriod,
					cur_timestp,
					gPrevSofTimestp[module]);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_lcso_])
			ISP_PatchTimestamp(module,
					_lcso_, frmPeriod,
					cur_timestp,
					gPrevSofTimestp[module]);
	}
	#endif
}

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_aao_])
				if (FrmStat_aao != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp(module, _aao_,
					    sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_afo_])
				if (FrmStat_afo != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp(module, _afo_,
					    sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_flko_])
				if (FrmStat_flko != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp(module, _flko_,
					    sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pdo_])
				if (FrmStat_pdo != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp(module, _pdo_,
					    sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pso_])
				if (FrmStat_pso != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp(module, _pso_,
					    sec, usec, 1);
			#if (TSTMP_SUBSAMPLE_INTPL == 1)
			gPrevSofTimestp[module] = cur_timestp;
			#endif

			}
			#endif /* (TIMESTAMP_QUEUE_EN == 1) */

			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMA P1_SOF_%d_%d(0x%x_0x%x,0x%x_0x%x,0x%x,0x%x,0x%x),int_us:%d,cq:0x%x\n",
				   sof_count[module], cur_v_cnt,
				   ISP_RD32(CAM_REG_FBC_IMGO_CTL1(reg_module)),
				   ISP_RD32(CAM_REG_FBC_IMGO_CTL2(reg_module)),
				   ISP_RD32(CAM_REG_FBC_RRZO_CTL1(reg_module)),
				   ISP_RD32(CAM_REG_FBC_RRZO_CTL2(reg_module)),
				   ISP_RD32(CAM_REG_IMGO_BASE_ADDR(reg_module)),
				   ISP_RD32(CAM_REG_RRZO_BASE_ADDR(reg_module)),
				   magic_num,
				   (unsigned int)((sec * 1000000 + usec) -
					(1000000 * m_sec + m_usec)),
				   ISP_RD32(
				       CAM_REG_CQ_THR0_BASEADDR(reg_module)));

#ifdef ENABLE_STT_IRQ_LOG /*STT addr*/
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMA_aa(0x%x_0x%x_0x%x)af(0x%x_0x%x_0x%x),pd(0x%x_0x%x_0x%x),ps(0x%x_0x%x_0x%x)\n",
				    ISP_RD32(CAM_REG_AAO_BASE_ADDR(reg_module)),
				    ISP_RD32(CAM_REG_FBC_AAO_CTL1(reg_module)),
				    ISP_RD32(CAM_REG_FBC_AAO_CTL2(reg_module)),
				    ISP_RD32(CAM_REG_AFO_BASE_ADDR(reg_module)),
				    ISP_RD32(CAM_REG_FBC_AFO_CTL1(reg_module)),
				    ISP_RD32(CAM_REG_FBC_AFO_CTL2(reg_module)),
				    ISP_RD32(CAM_REG_PDO_BASE_ADDR(reg_module)),
				    ISP_RD32(CAM_REG_FBC_PDO_CTL1(reg_module)),
				    ISP_RD32(CAM_REG_FBC_PDO_CTL2(reg_module)),
				    ISP_RD32(CAM_REG_PSO_BASE_ADDR(reg_module)),
				    ISP_RD32(CAM_REG_FBC_PSO_CTL1(reg_module)),
				    ISP_RD32(CAM_REG_FBC_PSO_CTL2(reg_module)));
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

		/* update SOF time stamp for eis user(need match with the time
		 * stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][12] =
		    (unsigned int)(sec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][12] =
		    (unsigned int)(usec);

		#if 0
		/* sw sof counter */
		if (irqDelay == 1)
			sof_count[module] = sofCntGrpHw;
		#endif

		sof_count[module] += frmPeriod;
		/* for match vsync cnt */
		if (sof_count[module] > 255)
			sof_count[module] -= 256;

		g1stSof[module] = MFALSE;
	}
LB_CAMA_SOF_IGNORE:

#ifdef ENABLE_STT_IRQ_LOG
	if (DmaStatus & (AAO_DONE_ST|AFO_DONE_ST|PDO_DONE_ST|PSO_DONE_ST)) {
		IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
			"CAMA_STT_Done_%d_0x%x\n",
			(sof_count[module]) ?
			(sof_count[module] - 1) : (sof_count[module]),
			DmaStatus);
	}
#endif


	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		/* 1. update interrupt status to all users */
		IspInfo.IrqInfo.Status[module][SIGNAL_INT][i] |= IrqStatus;
		IspInfo.IrqInfo.Status[module][DMA_INT][i] |= DmaStatus;

		/* 2. update signal time and passed by signal count */
		if (IspInfo.IrqInfo.MarkedFlag[module][SIGNAL_INT][i] &
		    IspInfo.IrqInfo.Mask[module][SIGNAL_INT]) {
			unsigned int cnt = 0, tmp = IrqStatus;

			while (tmp) {
				if (tmp & 0x1) {
					IspInfo.IrqInfo.LastestSigTime_usec
					    [module][cnt] =
						(unsigned int)time_frmb.tv_usec;
					IspInfo.IrqInfo.LastestSigTime_sec
					    [module][cnt] =
						(unsigned int) time_frmb.tv_sec;
					IspInfo.IrqInfo.PassedBySigCnt
					    [module][cnt][i]++;
				}
				tmp = tmp >> 1;
				cnt++;
			}
		} else {
			/* no any interrupt is not marked and  in read mask in
			 * this irq type
			 */
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[module]));
	/*  */
	if (IrqStatus & SOF_INT_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SOF]);
	}
	if (IrqStatus & SW_PASS1_DON_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SW_P1_DONE]);
	}
	if (DmaStatus & AAO_DONE_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_AAO_DONE]);
	}
	if (DmaStatus & FLKO_DONE_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_FLKO_DONE]);
	}
	if (DmaStatus & AFO_DONE_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_AFO_DONE]);
	}
	if (DmaStatus & PSO_DONE_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_PSO_DONE]);
	}
	if (DmaStatus & PDO_DONE_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_PDO_DONE]);
	}
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

irqreturn_t ISP_Irq_CAM_B(signed int  Irq, void *DeviceId)
{
	unsigned int module = ISP_IRQ_TYPE_INT_CAM_B_ST;
	unsigned int reg_module = ISP_CAM_B_IDX;
	unsigned int i;
	unsigned int IrqStatus, ErrStatus, WarnStatus;
	unsigned int DmaStatus;
	union FBC_CTRL_1 fbc_ctrl1[2];
	union FBC_CTRL_2 fbc_ctrl2[2];
	unsigned int cur_v_cnt = 0;
	struct timeval time_frmb;
	unsigned long long  sec = 0;
	unsigned long       usec = 0;
	unsigned int IrqEnableOrig, IrqEnableNew;

	/* Avoid touch hwmodule when clock is disable. DEVAPC will moniter this
	 * kind of err
	 */
	if (G_u4EnableClockCount == 0)
		return IRQ_HANDLED;

	/*	*/
	/* do_gettimeofday(&time_frmb); */
	sec = cpu_clock(0);  /* ns */
	do_div(sec, 1000);    /* usec */
	usec = do_div(sec, 1000000);    /* sec and usec */
	time_frmb.tv_usec = usec;
	time_frmb.tv_sec = sec;

	#if (ISP_BOTTOMHALF_WORKQ == 1)
	gSvLog[module]._lastIrqTime.sec = sec;
	gSvLog[module]._lastIrqTime.usec = usec;
	#endif

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	IrqStatus = ISP_RD32(CAM_REG_CTL_RAW_INT_STATUS(reg_module));
	DmaStatus = ISP_RD32(CAM_REG_CTL_RAW_INT2_STATUS(reg_module));
	spin_unlock(&(IspInfo.SpinLockIrq[module]));

	ErrStatus = IrqStatus & IspInfo.IrqInfo.ErrMask[module][SIGNAL_INT];
	WarnStatus = IrqStatus & IspInfo.IrqInfo.WarnMask[module][SIGNAL_INT];
	IrqStatus = IrqStatus & IspInfo.IrqInfo.Mask[module][SIGNAL_INT];

	/* Check ERR/WRN ISR times, if it occur too frequently, mark it for
	 * avoding keep enter ISR. It will happen KE
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
				if (IspInfo.IrqCntInfo.m_err_int_cnt[module][i]
				    >= INT_ERR_WARN_MAX_TIME)
					IspInfo.IrqCntInfo.m_err_int_mark
					    [module] |= (1 << i);

				if (IspInfo.IrqCntInfo.m_warn_int_cnt[module][i]
				    >= INT_ERR_WARN_MAX_TIME)
					IspInfo.IrqCntInfo.m_warn_int_mark
					    [module] |= (1 << i);

			} else {
				IspInfo.IrqCntInfo.m_int_usec[module] = usec;
				IspInfo.IrqCntInfo.m_err_int_cnt[module][i] = 0;
				IspInfo.IrqCntInfo.m_warn_int_cnt[module][i]
				    = 0;
			}
		}

	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	IrqEnableOrig = ISP_RD32(CAM_REG_CTL_RAW_INT_EN(reg_module));
	spin_unlock(&(IspInfo.SpinLockIrq[module]));

	IrqEnableNew = IrqEnableOrig &
		~(IspInfo.IrqCntInfo.m_err_int_mark[module]
		| IspInfo.IrqCntInfo.m_warn_int_mark[module]);
	ISP_WR32(CAM_REG_CTL_RAW_INT_EN(reg_module), IrqEnableNew);

	/*	*/
	IRQ_INT_ERR_CHECK_CAM(WarnStatus, ErrStatus, module);

	fbc_ctrl1[0].Raw = ISP_RD32(CAM_REG_FBC_IMGO_CTL1(reg_module));
	fbc_ctrl1[1].Raw = ISP_RD32(CAM_REG_FBC_RRZO_CTL1(reg_module));
	fbc_ctrl2[0].Raw = ISP_RD32(CAM_REG_FBC_IMGO_CTL2(reg_module));
	fbc_ctrl2[1].Raw = ISP_RD32(CAM_REG_FBC_RRZO_CTL2(reg_module));


	#if defined(ISP_MET_READY)
	/*CLS:Test*/
	if (IrqStatus & SW_PASS1_DON_ST) {
		if (met_mmsys_event_isp_pass1_end)
			met_mmsys_event_isp_pass1_end(1);
	}

	if (IrqStatus & SOF_INT_ST) {
		/*met mmsys profile*/
		if (met_mmsys_event_isp_pass1_begin)
			met_mmsys_event_isp_pass1_begin(1);
	}
	#endif

	/* sof , done order chech . */
	if ((IrqStatus & HW_PASS1_DON_ST) || (IrqStatus & SOF_INT_ST))
		cur_v_cnt = ISP_RD32_TG_CAM_FRM_CNT(module, reg_module);

	if ((IrqStatus & HW_PASS1_DON_ST) && (IrqStatus & SOF_INT_ST)) {
		if (cur_v_cnt != sof_count[module])
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"isp sof_don block, %d_%d\n", cur_v_cnt,
				sof_count[module]);
	}

	if ((IrqStatus & HW_PASS1_DON_ST) &&
	    (IspInfo.DebugMask & ISP_DBG_HW_DON)) {
		IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
			"CAMB P1_HW_DON_%d\n",
			(sof_count[module]) ?
			  (sof_count[module] - 1) : (sof_count[module]));
	}

	spin_lock(&(IspInfo.SpinLockIrq[module]));
	if (IrqStatus & VS_INT_ST) {
		Vsync_cnt[1]++;
		/* pr_info("CAMB N3D:0x%x\n",Vsync_cnt[1]); */
	}
	if (IrqStatus & SW_PASS1_DON_ST) {
		sec = ktime_get();
		do_div(sec, 1000); /* usec */
		usec = do_div(sec, 1000000); /* sec and usec */
		/* update pass1 done time stamp for eis user(need match with
		 * the time stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][10] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][10] =
			(unsigned int)(sec);

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			/*SW p1_don is not reliable*/
			if (FrameStatus[module] != CAM_FST_DROP_FRAME) {
				IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
					"CAMB P1_DON_%d(0x%x_0x%x,0x%x_0x%x)\n",
					(sof_count[module]) ?
					  (sof_count[module] - 1) :
					  (sof_count[module]),
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
			    ((ISP_RD32(CAM_REG_TG_SUB_PERIOD(reg_module)) >> 8)
			     & 0x1F) + 1;

			if (frmPeriod > 1) {
				ISP_PatchTimestamp(module, _imgo_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _rrzo_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _ufeo_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _eiso_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _rsso_, frmPeriod,
					cur_timestp, gPrevSofTimestp[module]);
				ISP_PatchTimestamp(module, _lcso_, frmPeriod,
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

	}

	if (IrqStatus & SOF_INT_ST) {
		unsigned int frmPeriod = (
			(ISP_RD32(CAM_REG_TG_SUB_PERIOD(reg_module)) >> 8)
			 & 0x1F) + 1;
		unsigned int irqDelay = 0;

		sec = ktime_get();
		do_div(sec, 1000);    /* usec */
		usec = do_div(sec, 1000000);    /* sec and usec */

		if (frmPeriod == 0) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_ERR,
				"ERROR: Wrong sub-sample period: 0");
			goto LB_CAMB_SOF_IGNORE;
		}

		/* chk this frame have EOF or not, dynimic dma port chk */
		FrameStatus[module] =
			Irq_CAM_FrameStatus(reg_module, module, irqDelay);
		if (FrameStatus[module] == CAM_FST_DROP_FRAME) {
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMB Lost p1 done_%d (0x%x): ",
				sof_count[module], cur_v_cnt);
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
			static unsigned int m_sec = 0, m_usec;
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
			enum CAM_FrameST FrmStat_aao, FrmStat_afo, FrmStat_flko;
			enum CAM_FrameST FrmStat_pdo, FrmStat_pso;

			ISP_GetDmaPortsStatus(reg_module,
					IspInfo.TstpQInfo[module].DmaEnStatus);

			/* Prevent WCNT increase after
			 * ISP_CompensateMissingSofTime around P1_DON
			 * and FBC_CNT decrease to 0, following drop frame is
			 * checked becomes true, then SOF timestamp will
			 * missing for current frame
			 */
			if (IspInfo.TstpQInfo[module].DmaEnStatus[_aao_])
				FrmStat_aao = Irq_CAM_SttFrameStatus(reg_module,
					module, _aao_, irqDelay);
			else
				FrmStat_aao = CAM_FST_DROP_FRAME;

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_afo_])
				FrmStat_afo = Irq_CAM_SttFrameStatus(reg_module,
					module, _afo_, irqDelay);
			else
				FrmStat_afo = CAM_FST_DROP_FRAME;

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_flko_])
				FrmStat_flko = Irq_CAM_SttFrameStatus(reg_module
					, module, _flko_, irqDelay);
			else
				FrmStat_flko = CAM_FST_DROP_FRAME;

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pdo_])
				FrmStat_pdo = Irq_CAM_SttFrameStatus(reg_module,
					module, _pdo_, irqDelay);
			else
				FrmStat_pdo = CAM_FST_DROP_FRAME;
			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pso_])
				FrmStat_pso = Irq_CAM_SttFrameStatus(reg_module,
					module, _pso_, irqDelay);
			else
				FrmStat_pso = CAM_FST_DROP_FRAME;

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_imgo_])
				ISP_CompensateMissingSofTime(reg_module, module,
					_imgo_, sec, usec, frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_rrzo_])
				ISP_CompensateMissingSofTime(reg_module, module,
					_rrzo_, sec, usec, frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_ufeo_])
				ISP_CompensateMissingSofTime(reg_module, module,
					_ufeo_, sec, usec, frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_eiso_])
				ISP_CompensateMissingSofTime(reg_module, module,
					_eiso_, sec, usec, frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_rsso_])
				ISP_CompensateMissingSofTime(reg_module, module,
					_rsso_, sec, usec, frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_lcso_])
				ISP_CompensateMissingSofTime(reg_module, module,
					_lcso_, sec, usec, frmPeriod);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_aao_])
				ISP_CompensateMissingSofTime(reg_module, module,
					_aao_, sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_afo_])
				ISP_CompensateMissingSofTime(reg_module, module,
					_afo_, sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_flko_])
				ISP_CompensateMissingSofTime(reg_module, module,
					_flko_, sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pdo_])
				ISP_CompensateMissingSofTime(reg_module, module,
					_pdo_, sec, usec, 1);
			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pso_])
				ISP_CompensateMissingSofTime(reg_module, module,
					_pso_, sec, usec, 1);

if (FrameStatus[module] != CAM_FST_DROP_FRAME) {
	for (subFrm = 0; subFrm < frmPeriod; subFrm++) {
		/* Current frame is NOT DROP FRAME */
		if (IspInfo.TstpQInfo[module].DmaEnStatus[_imgo_])
			ISP_PushBufTimestamp(module,
				_imgo_, sec, usec, frmPeriod);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_rrzo_])
			ISP_PushBufTimestamp(module,
				_rrzo_, sec, usec, frmPeriod);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_ufeo_])
			ISP_PushBufTimestamp(module,
				_ufeo_, sec, usec, frmPeriod);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_eiso_])
			ISP_PushBufTimestamp(module,
				_eiso_, sec, usec, frmPeriod);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_rsso_])
			ISP_PushBufTimestamp(module,
				_rsso_, sec, usec, frmPeriod);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_lcso_])
			ISP_PushBufTimestamp(module,
				_lcso_, sec, usec, frmPeriod);
	}

	/* for slow motion sub-sample */
	/* must after current ISP_PushBufTimestamp() */
	#if (TSTMP_SUBSAMPLE_INTPL == 1)
	if ((frmPeriod > 1) &&
			(g1stSwP1Done[module] == MFALSE)) {
		if (IspInfo.TstpQInfo[module].DmaEnStatus[_imgo_])
			ISP_PatchTimestamp(module,
					_imgo_, frmPeriod,
					cur_timestp,
					gPrevSofTimestp[module]);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_rrzo_])
			ISP_PatchTimestamp(module,
					_rrzo_, frmPeriod,
					cur_timestp,
					gPrevSofTimestp[module]);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_ufeo_])
			ISP_PatchTimestamp(module,
					_ufeo_, frmPeriod,
					cur_timestp,
					gPrevSofTimestp[module]);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_eiso_])
			ISP_PatchTimestamp(module,
					_eiso_, frmPeriod,
					cur_timestp,
					gPrevSofTimestp[module]);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_rsso_])
			ISP_PatchTimestamp(module,
					_rsso_, frmPeriod,
					cur_timestp,
					gPrevSofTimestp[module]);

		if (IspInfo.TstpQInfo[module].DmaEnStatus[_lcso_])
			ISP_PatchTimestamp(module,
					_lcso_, frmPeriod,
					cur_timestp,
					gPrevSofTimestp[module]);
	}
	#endif
}

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_aao_])
				if (FrmStat_aao != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp
					    (module, _aao_, sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_afo_])
				if (FrmStat_afo != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp
					    (module, _afo_, sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_flko_])
				if (FrmStat_flko != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp
					    (module, _flko_, sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pdo_])
				if (FrmStat_pdo != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp
					    (module, _pdo_, sec, usec, 1);

			if (IspInfo.TstpQInfo[module].DmaEnStatus[_pso_])
				if (FrmStat_pso != CAM_FST_DROP_FRAME)
					ISP_PushBufTimestamp
					    (module, _pso_, sec, usec, 1);
			#if (TSTMP_SUBSAMPLE_INTPL == 1)
			gPrevSofTimestp[module] = cur_timestp;
			#endif

			}
			#endif /* (TIMESTAMP_QUEUE_EN == 1) */

			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMB P1_SOF_%d_%d (0x%x_0x%x,0x%x_0x%x,0x%x,0x%x,0x%x), int_us:%d,cq:0x%x\n",
				   sof_count[module], cur_v_cnt,
				   ISP_RD32(CAM_REG_FBC_IMGO_CTL1(reg_module)),
				   ISP_RD32(CAM_REG_FBC_IMGO_CTL2(reg_module)),
				   ISP_RD32(CAM_REG_FBC_RRZO_CTL1(reg_module)),
				   ISP_RD32(CAM_REG_FBC_RRZO_CTL2(reg_module)),
				   ISP_RD32(CAM_REG_IMGO_BASE_ADDR(reg_module)),
				   ISP_RD32(CAM_REG_RRZO_BASE_ADDR(reg_module)),
				   magic_num,
				   (unsigned int)((sec * 1000000 + usec) -
					(1000000 * m_sec + m_usec)),
				   ISP_RD32(CAM_REG_CQ_THR0_BASEADDR
				       (reg_module)));

#ifdef ENABLE_STT_IRQ_LOG /*STT addr*/
			IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
				"CAMB_aa(0x%x_0x%x_0x%x)af(0x%x_0x%x_0x%x), pd(0x%x_0x%x_0x%x),ps(0x%x_0x%x_0x%x)\n",
				    ISP_RD32(CAM_REG_AAO_BASE_ADDR(reg_module)),
				    ISP_RD32(CAM_REG_FBC_AAO_CTL1(reg_module)),
				    ISP_RD32(CAM_REG_FBC_AAO_CTL2(reg_module)),
				    ISP_RD32(CAM_REG_AFO_BASE_ADDR(reg_module)),
				    ISP_RD32(CAM_REG_FBC_AFO_CTL1(reg_module)),
				    ISP_RD32(CAM_REG_FBC_AFO_CTL2(reg_module)),
				    ISP_RD32(CAM_REG_PDO_BASE_ADDR(reg_module)),
				    ISP_RD32(CAM_REG_FBC_PDO_CTL1(reg_module)),
				    ISP_RD32(CAM_REG_FBC_PDO_CTL2(reg_module)),
				    ISP_RD32(CAM_REG_PSO_BASE_ADDR(reg_module)),
				    ISP_RD32(CAM_REG_FBC_PSO_CTL1(reg_module)),
				    ISP_RD32(CAM_REG_FBC_PSO_CTL2(reg_module)));
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

		/* update SOF time stamp for eis user(need match with the time
		 * stamp in image header)
		 */
		IspInfo.IrqInfo.LastestSigTime_usec[module][12] =
			(unsigned int)(sec);
		IspInfo.IrqInfo.LastestSigTime_sec[module][12] =
			(unsigned int)(usec);

		#if 0
		/* sw sof counter */
		if (irqDelay == 1)
			sof_count[module] = sofCntGrpHw;
		#endif

		sof_count[module] += frmPeriod;
		/* for match vsync cnt */
		if (sof_count[module] > 255)
			sof_count[module] -= 256;

		g1stSof[module] = MFALSE;
	}
LB_CAMB_SOF_IGNORE:

#ifdef ENABLE_STT_IRQ_LOG
	if (DmaStatus & (AAO_DONE_ST|AFO_DONE_ST|PDO_DONE_ST|PSO_DONE_ST)) {
		IRQ_LOG_KEEPER(module, m_CurrentPPB, _LOG_INF,
			"CAMB_STT_Done_%d_0x%x\n",
			(sof_count[module]) ?
			(sof_count[module] - 1) : (sof_count[module]),
			DmaStatus);
	}
#endif

	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		/* 1. update interrupt status to all users */
		IspInfo.IrqInfo.Status[module][SIGNAL_INT][i] |= IrqStatus;
		IspInfo.IrqInfo.Status[module][DMA_INT][i] |= DmaStatus;

		/* 2. update signal time and passed by signal count */
		if (IspInfo.IrqInfo.MarkedFlag[module][SIGNAL_INT][i] &
		    IspInfo.IrqInfo.Mask[module][SIGNAL_INT]) {
			unsigned int cnt = 0, tmp = IrqStatus;

			while (tmp) {
				if (tmp & 0x1) {
					IspInfo.IrqInfo.LastestSigTime_usec
					    [module][cnt] =
					    (unsigned int) time_frmb.tv_usec;
					IspInfo.IrqInfo.LastestSigTime_sec
					    [module][cnt] =
					    (unsigned int) time_frmb.tv_sec;
					IspInfo.IrqInfo.PassedBySigCnt
					    [module][cnt][i]++;
				}
				tmp = tmp >> 1;
				cnt++;
			}
		} else {
			/* no any interrupt is not marked and in read mask in
			 * this irq type
			 */
		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[module]));
	/*  */
	if (IrqStatus & SOF_INT_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SOF]);
	}
	if (IrqStatus & SW_PASS1_DON_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_SW_P1_DONE]);
	}
	if (DmaStatus & AAO_DONE_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_AAO_DONE]);
	}
	if (DmaStatus & FLKO_DONE_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_FLKO_DONE]);
	}
	if (DmaStatus & AFO_DONE_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_AFO_DONE]);
	}
	if (DmaStatus & PSO_DONE_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_PSO_DONE]);
	}
	if (DmaStatus & PDO_DONE_ST) {
		wake_up_interruptible(&IspInfo.WaitQHeadCam
			[ISP_GetWaitQCamIndex(module)]
			[ISP_WAITQ_HEAD_IRQ_PDO_DONE]);
	}
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
 *****************************************************************************/

static void SMI_INFO_DUMP(enum ISP_IRQ_TYPE_ENUM irq_module)
{
#ifndef EP_MARK_SMI
	switch (irq_module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
		if (g_ISPIntErr_SMI[irq_module] &
			(DMA_ERR_ST | INT_ST_MASK_CAM_WARN | CQ_VS_ERR_ST)) {
			pr_info("ERR:SMI_DUMP by module:%d\n", irq_module);
			smi_debug_bus_hang_detect(false, ISP_DEV_NAME);
			g_ISPIntErr_SMI[irq_module] = 0;
		}
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
		if (g_ISPIntErr_SMI[irq_module] &
			(SV_IMGO_ERR | SV_IMGO_OVERRUN)) {
			pr_info("ERR:SMI_DUMP by module:%d\n", irq_module);
			smi_debug_bus_hang_detect(false, ISP_DEV_NAME);

			g_ISPIntErr_SMI[irq_module] = 0;
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

	IRQ_LOG_PRINTER(pWorkTable->module, m_CurrentPPB, _LOG_INF);
	SMI_INFO_DUMP(pWorkTable->module);
}
#endif

/******************************************************************************
 *
 *****************************************************************************/
module_init(ISP_Init);
module_exit(ISP_Exit);
MODULE_DESCRIPTION("Camera ISP driver");
MODULE_AUTHOR("ME8");
MODULE_LICENSE("GPL");
