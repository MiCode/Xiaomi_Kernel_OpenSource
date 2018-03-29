/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/******************************************************************************
*[File]             ahb.c
*[Version]          v1.0
*[Revision Date]    2013-01-16
*[Author]
*[Description]
*    The program provides AHB HIF driver
*[Copyright]
*    Copyright (C) 2013 MediaTek Incorporation. All Rights Reserved.
******************************************************************************/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/interrupt.h>
/* #include <linux/kernel.h> */
#include <linux/device.h>
/* #include <linux/errno.h> */
#include <linux/platform_device.h>
/* #include <linux/fs.h> */
/* #include <linux/cdev.h> */
/* #include <linux/poll.h> */

#include <linux/mm.h>
#ifndef CONFIG_X86
#include <asm/memory.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of.h>
#else

#endif

/* #include <mach/mt_pm_ldo.h>
#include <mach/mt_gpt.h> */

#include "gl_os.h"

#if defined(MT6797)
#include "mt6630_reg.h"
#include "sdio.h"
#define NIC_TX_PAGE_SIZE                        128	/* in unit of bytes */
DEFINE_SPINLOCK(HifLock);
#endif

#if !defined(CONFIG_MTK_CLKMGR)
#include <linux/clk.h>
#endif

/* #define MTK_DMA_BUF_MEMCPY_SUP */ /* no virt_to_phys() use */
/* #define HIF_DEBUG_SUP */
/* #define HIF_DEBUG_SUP_TX */

#ifdef HIF_DEBUG_SUP
#define HIF_DBG(msg)	(printk msg)
#else
#define HIF_DBG(msg)
#endif /* HIF_DEBUG_SUP */

#ifdef HIF_DEBUG_SUP_TX
#define HIF_DBG_TX(msg)	(printk msg)
#else
#define HIF_DBG_TX(msg)
#endif /* HIF_DEBUG_SUP */

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

static irqreturn_t HifAhbISR(IN int Irq, IN void *Arg);

static int HifAhbProbe(VOID);

static int HifAhbRemove(VOID);

#if (MTK_WCN_SINGLE_MODULE == 0)
static int HifAhbBusCntGet(VOID);

static int HifAhbBusCntClr(VOID);

static int HifTxCnt;
#endif /* MTK_WCN_SINGLE_MODULE */

#if (CONF_HIF_DEV_MISC == 1)
static ssize_t HifAhbMiscRead(IN struct file *Filp, OUT char __user *DstBuf, IN size_t Size, IN loff_t *Ppos);

static ssize_t HifAhbMiscWrite(IN struct file *Filp, IN const char __user *SrcBuf, IN size_t Size, IN loff_t *Ppos);

static int HifAhbMiscIoctl(IN struct file *Filp, IN unsigned int Cmd, IN unsigned long arg);

static int HifAhbMiscOpen(IN struct inode *Inodep, IN struct file *Filp);

static int HifAhbMiscClose(IN struct inode *Inodep, IN struct file *Filp);
#else

static int HifAhbPltmProbe(IN struct platform_device *PDev);

static int __exit HifAhbPltmRemove(IN struct platform_device *PDev);

#ifdef CONFIG_PM
static int HifAhbPltmSuspend(IN struct platform_device *PDev, pm_message_t Message);

static int HifAhbPltmResume(IN struct platform_device *PDev);
#endif /* CONFIG_PM */

#endif /* CONF_HIF_DEV_MISC */

#if (CONF_HIF_LOOPBACK_AUTO == 1)	/* only for development test */
static VOID HifAhbLoopbkAuto(IN unsigned long arg);
#endif /* CONF_HIF_LOOPBACK_AUTO */

#if (CONF_HIF_DMA_INT == 1)
static irqreturn_t HifDmaISR(IN int Irq, IN void *Arg);
#endif /* CONF_HIF_DMA_INT */

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/* initialiation function from other module */
static probe_card pfWlanProbe;

/* release function from other module */
static remove_card pfWlanRemove;

/* DMA operator */
static GL_HIF_DMA_OPS_T *pfWlanDmaOps;

static BOOLEAN WlanDmaFatalErr;

#if (CONF_HIF_DEV_MISC == 1)
static const struct file_operations MtkAhbOps = {
	.owner = THIS_MODULE,
	.read = HifAhbMiscRead,
	.write = HifAhbMiscWrite,
	.unlocked_ioctl = HifAhbMiscIoctl,
	.compat_ioctl = HifAhbMiscIoctl,
	.open = HifAhbMiscOpen,
	.release = HifAhbMiscClose,
};

static struct miscdevice MtkAhbDriver = {
	.minor = MISC_DYNAMIC_MINOR,	/* any minor number */
	.name = HIF_MOD_NAME,
	.fops = &MtkAhbOps,
};
#else

#ifdef CONFIG_OF
static const struct of_device_id apwifi_of_ids[] = {
	{.compatible = "mediatek,WIFI",},
	{}
};
#endif

struct platform_driver MtkPltmAhbDriver = {
	.driver = {
		   .name = "mt-wifi",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = apwifi_of_ids,
#endif
		   },
	.probe = HifAhbPltmProbe,
#ifdef CONFIG_PM
	.suspend = HifAhbPltmSuspend,
	.resume = HifAhbPltmResume,
#else
	.suspend = NULL,
	.resume = NULL,
#endif /* CONFIG_PM */
	.remove = __exit_p(HifAhbPltmRemove),
};

UINT_32 IsrCnt = 0, IsrPassCnt = 0, TaskIsrCnt = 0; /* MT6797 */ 
static struct platform_device *HifAhbPDev;

#endif /* CONF_HIF_DEV_MISC */

/*******************************************************************************
*                       P U B L I C   F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will register sdio bus to the os
*
* \param[in] pfProbe    Function pointer to detect card
* \param[in] pfRemove   Function pointer to remove card
*
* \return The result of registering HIF driver (WLAN_STATUS_SUCCESS = 0)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS glRegisterBus(probe_card pfProbe, remove_card pfRemove)
{
	WLAN_STATUS Ret;

	ASSERT(pfProbe);
	ASSERT(pfRemove);

	pfWlanProbe = pfProbe;	/* wlan card initialization in other modules = wlanProbe() */
	pfWlanRemove = pfRemove;

#if (CONF_HIF_DEV_MISC == 1)
	Ret = misc_register(&MtkAhbDriver);
	if (Ret != 0)
		return Ret;
	HifAhbProbe();
#else
	Ret = platform_driver_register(&MtkPltmAhbDriver);
#endif /* CONF_HIF_DEV_MISC */

	return Ret;

}				/* end of glRegisterBus() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will unregister sdio bus to the os
*
* \param[in] pfRemove   Function pointer to remove card
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glUnregisterBus(remove_card pfRemove)
{
	ASSERT(pfRemove);

	pfRemove();

#if (CONF_HIF_DEV_MISC == 1)
	HifAhbRemove();

	if ((misc_deregister(&MtkAhbDriver)) != 0)
		;
#else

	platform_driver_unregister(&MtkPltmAhbDriver);
#endif /* CONF_HIF_DEV_MISC */

	return;

}				/* end of glUnregisterBus() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will inform us whole chip reset start event.
*
* \param[in] GlueInfo   Pointer to glue info structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glResetHif(GLUE_INFO_T *GlueInfo)
{
	GL_HIF_INFO_T *HifInfo;

	ASSERT(GlueInfo);
	HifInfo = &GlueInfo->rHifInfo;
	if (HifInfo->DmaOps)
		HifInfo->DmaOps->DmaReset(HifInfo);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function stores hif related info, which is initialized before.
*
* \param[in] GlueInfo Pointer to glue info structure
* \param[in] u4Cookie   Pointer to UINT_32 memory base variable for _HIF_HPI
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
UINT_8 **g_pHifRegBaseAddr = NULL;

VOID glSetHifInfo(GLUE_INFO_T *GlueInfo, ULONG ulCookie)
{
	GL_HIF_INFO_T *HifInfo;
	UINT_32		   val = 0;

	/* Init HIF */
	ASSERT(GlueInfo);
	HifInfo = &GlueInfo->rHifInfo;
#if (CONF_HIF_DEV_MISC == 1)
	HifInfo->Dev = MtkAhbDriver.this_device;
#else
	HifInfo->Dev = &HifAhbPDev->dev;
#endif /* CONF_HIF_DEV_MISC */
	SET_NETDEV_DEV(GlueInfo->prDevHandler, HifInfo->Dev);

	HifInfo->HifRegBaseAddr = ioremap(HIF_DRV_BASE, HIF_DRV_LENGTH);
	HifInfo->McuRegBaseAddr = ioremap(CONN_MCU_DRV_BASE, CONN_MCU_REG_LENGTH);
#if defined(MT6797)
	HifInfo->confRegBaseAddr = ioremap(DYNAMIC_REMAP_CONF_BASE, DYNAMIC_REMAP_CONF_LENGTH);
#endif
	g_pHifRegBaseAddr = &(HifInfo->HifRegBaseAddr);

	DBGLOG(INIT, INFO, "[WiFi/HIF]HifInfo->HifRegBaseAddr=0x%p, HifInfo->McuRegBaseAddr=0x%p\n",
	       HifInfo->HifRegBaseAddr, HifInfo->McuRegBaseAddr);

	/* default disable DMA */
	HifInfo->fgDmaEnable = FALSE;
	HifInfo->DmaRegBaseAddr = 0;
	HifInfo->DmaOps = NULL;

#if defined(MT6797)
	sdio_open();
#endif

	/* read chip ID */
	val = HIF_REG_READL(HifInfo, MCR_WCIR);
	HifInfo->ChipID = val & 0xFFFF;

	if (HifInfo->ChipID == 0x0321 || HifInfo->ChipID == 0x0335 || HifInfo->ChipID == 0x0337)
		HifInfo->ChipID = 0x6735;	/* Denali ChipID transition */
	if (HifInfo->ChipID == 0x0326)
		HifInfo->ChipID = 0x6755;
	if (HifInfo->ChipID == 0x0279)	/* Everest */
		HifInfo->ChipID = 0x6797;
	DBGLOG(INIT, INFO, "[WiFi/HIF] ChipID = 0x%x\n", HifInfo->ChipID);

#ifdef CONFIG_OF
#if !defined(CONFIG_MTK_CLKMGR)
	HifInfo->clk_wifi_dma = devm_clk_get(&HifAhbPDev->dev, "wifi-dma");
	if (IS_ERR(HifInfo->clk_wifi_dma)) {
		DBGLOG(INIT, ERROR, "[WiFi/HIF][CCF]cannot get HIF clk_wifi_dma clock.\n");
		/* return PTR_ERR(HifInfo->clk_wifi_dma); */
	}
	DBGLOG(INIT, INFO, "[WiFi/HIF][CCF]HIF clk_wifi_dma=0x%p\n", HifInfo->clk_wifi_dma);
#endif
#endif

	/* Init DMA */
	WlanDmaFatalErr = 0;	/* reset error flag */

#if (CONF_MTK_AHB_DMA == 1)
	spin_lock_init(&HifInfo->DdmaLock);

	HifPdmaInit(HifInfo);

	pfWlanDmaOps = HifInfo->DmaOps;
#endif /* CONF_MTK_AHB_DMA */

	/* Start loopback test after 10 seconds */
#if (CONF_HIF_LOOPBACK_AUTO == 1)	/* only for development test */
	{
		init_timer(&(HifInfo->HifTmrLoopbkFn));
		HifInfo->HifTmrLoopbkFn.function = HifAhbLoopbkAuto;
		HifInfo->HifTmrLoopbkFn.data = (unsigned long)GlueInfo;

		init_waitqueue_head(&HifInfo->HifWaitq);
		HifInfo->HifTaskLoopbkFn = kthread_run(kalDevLoopbkThread, GlueInfo->prDevHandler, "LoopbkThread");
		HifInfo->HifLoopbkFlg = 0;

		/* Note: in FPGA, clock is not accuracy so 3000 here, not 10000 */
		HifInfo->HifTmrLoopbkFn.expires = jiffies + MSEC_TO_SYSTIME(30000);
		add_timer(&(HifInfo->HifTmrLoopbkFn));

		HIF_DBG(("[WiFi/HIF] Start loopback test after 10 seconds (jiffies = %u)...\n", jiffies));
	}
#endif /* CONF_HIF_LOOPBACK_AUTO */

#if (CONF_HIF_DMA_INT == 1)
	init_waitqueue_head(&HifInfo->HifDmaWaitq);
	HifInfo->HifDmaWaitFlg = 0;
#endif /* CONF_HIF_DMA_INT */

}				/* end of glSetHifInfo() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function clears hif related info.
*
* \param[in] GlueInfo Pointer to glue info structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glClearHifInfo(GLUE_INFO_T *GlueInfo)
{
	iounmap(GlueInfo->rHifInfo.HifRegBaseAddr);
	iounmap(GlueInfo->rHifInfo.DmaRegBaseAddr);
	iounmap(GlueInfo->rHifInfo.McuRegBaseAddr);
#ifdef MT6797
	iounmap(GlueInfo->rHifInfo.confRegBaseAddr);
#endif
	return;

}				/* end of glClearHifInfo() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function clears hif related info.
*
* \param[in] GlueInfo Pointer to glue info structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glGetChipInfo(GLUE_INFO_T *GlueInfo, UINT_8 *pucChipBuf)
{
	GL_HIF_INFO_T *HifInfo;

	HifInfo = &GlueInfo->rHifInfo;
	DBGLOG(INIT, TRACE, "glGetChipInfo ChipID = 0x%x\n", HifInfo->ChipID);
	switch (HifInfo->ChipID) {
	case MTK_CHIP_ID_6571:
	case MTK_CHIP_ID_8127:
	case MTK_CHIP_ID_6752:
	case MTK_CHIP_ID_8163:
	case MTK_CHIP_ID_6735:
	case MTK_CHIP_ID_6580:
	case MTK_CHIP_ID_6755:
		kalSprintf(pucChipBuf, "%04x", HifInfo->ChipID);
		break;
	default:
		kalMemCopy(pucChipBuf, "SOC", strlen("SOC"));
	}
}				/* end of glGetChipInfo() */

#if CFG_SPM_WORKAROUND_FOR_HOTSPOT
/*----------------------------------------------------------------------------*/
/*!
* \brief This function to check if we need wakelock under Hotspot mode.
*
* \param[in] GlueInfo Pointer to glue info structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN glIsChipNeedWakelock(GLUE_INFO_T *GlueInfo)
{
	GL_HIF_INFO_T *HifInfo;

	HifInfo = &GlueInfo->rHifInfo;
	if (HifInfo->ChipID == MTK_CHIP_ID_6572 || HifInfo->ChipID == MTK_CHIP_ID_6582)
		return TRUE;
	else
		return FALSE;
}				/* end of glIsChipNeedWakelock() */
#endif /* CFG_SPM_WORKAROUND_FOR_HOTSPOT */

/*----------------------------------------------------------------------------*/
/*!
* \brief Initialize bus operation and hif related information, request resources.
*
* \param[out] pvData    A pointer to HIF-specific data type buffer.
*                       For eHPI, pvData is a pointer to UINT_32 type and stores a
*                       mapped base address.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN glBusInit(PVOID pvData)
{
	return TRUE;
}				/* end of glBusInit() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Stop bus operation and release resources.
*
* \param[in] pvData A pointer to struct net_device.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glBusRelease(PVOID pvData)
{
}				/* end of glBusRelease() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Setup bus interrupt operation and interrupt handler for os.
*
* \param[in] pvData     A pointer to struct net_device.
* \param[in] pfnIsr     A pointer to interrupt handler function.
* \param[in] pvCookie   Private data for pfnIsr function.
*
* \retval WLAN_STATUS_SUCCESS   if success
*         NEGATIVE_VALUE   if fail
*/
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
INT_32 glBusSetIrq(PVOID pvData, PVOID pfnIsr, PVOID pvCookie)
{
	struct device_node *node = NULL;
	unsigned int irq_info[3] = { 0, 0, 0 };
	/* unsigned int phy_base; */
	unsigned int irq_id = 0;
	unsigned int irq_flags = 0;

	struct net_device *prNetDevice;

	ASSERT(pvData);
	if (!pvData)
		return -1;
	prNetDevice = (struct net_device *)pvData;

	node = of_find_compatible_node(NULL, NULL, "mediatek,WIFI");
	if (node) {
		irq_id = irq_of_parse_and_map(node, 0);
		DBGLOG(INIT, INFO, "WIFI-OF: get wifi irq(%d)\n", irq_id);
	} else {
		DBGLOG(INIT, ERROR, "WIFI-OF: get wifi device node fail\n");
	}

	/* get the interrupt line behaviour */
	if (of_property_read_u32_array(node, "interrupts", irq_info, ARRAY_SIZE(irq_info))) {
		DBGLOG(INIT, ERROR, "WIFI-OF: get interrupt flag from DTS fail\n");
	} else {
		irq_flags = irq_info[2];
		DBGLOG(INIT, LOUD, "WIFI-OF: get interrupt flag(0x%x)\n", irq_flags);
	}

	/* Register AHB IRQ */
	if (request_irq(irq_id, HifAhbISR, irq_flags, HIF_MOD_NAME, prNetDevice)) {
		DBGLOG(INIT, ERROR, "WIFI-OF: request irq %d fail!\n", irq_id);
		return -1;
	}

	return 0;
}

VOID glBusFreeIrq(PVOID pvData, PVOID pvCookie)
{
	struct device_node *node = NULL;
	unsigned int irq_info[3] = { 0, 0, 0 };
	/* unsigned int phy_base; */
	unsigned int irq_id = 0;
	unsigned int irq_flags = 0;

	struct net_device *prNetDevice;

	/* Init */
	ASSERT(pvData);
	if (!pvData)
		return;
	prNetDevice = (struct net_device *)pvData;

	node = of_find_compatible_node(NULL, NULL, "mediatek,WIFI");
	if (node) {
		irq_id = irq_of_parse_and_map(node, 0);
		DBGLOG(INIT, INFO, "WIFI-OF: get wifi irq(%d)\n", irq_id);
	} else {
		DBGLOG(INIT, ERROR, "WIFI-OF: get wifi device node fail\n");
	}

	/* get the interrupt line behaviour */
	if (of_property_read_u32_array(node, "interrupts", irq_info, ARRAY_SIZE(irq_info))) {
		DBGLOG(INIT, ERROR, "WIFI-OF: get interrupt flag from DTS fail\n");
	} else {
		irq_flags = irq_info[2];
		DBGLOG(INIT, LOUD, "WIFI-OF: get interrupt flag(0x%x)\n", irq_flags);
	}

	/* Free the IRQ */
	free_irq(irq_id, prNetDevice);
	return;

}
#else
/* the name is different in 72 and 82 */
#ifndef MT_WF_HIF_IRQ_ID	/* for MT6572/82/92 */
#define MT_WF_HIF_IRQ_ID   WF_HIF_IRQ_ID
#endif /* MT_WF_HIF_IRQ_ID */

INT_32 glBusSetIrq(PVOID pvData, PVOID pfnIsr, PVOID pvCookie)
{
	int ret = 0;
	struct net_device *prNetDevice;
	GLUE_INFO_T *GlueInfo;
	GL_HIF_INFO_T *HifInfo;

	/* Init */
	ASSERT(pvData);
	if (!pvData)
		return -1;

	prNetDevice = (struct net_device *)pvData;
	GlueInfo = (GLUE_INFO_T *) pvCookie;
	ASSERT(GlueInfo);
	if (!GlueInfo) {
		DBGLOG(INIT, ERROR, "GlueInfo == NULL!\n");
		return -1;
	}

	HifInfo = &GlueInfo->rHifInfo;

	/* Register AHB IRQ */
	if (request_irq(MT_WF_HIF_IRQ_ID, HifAhbISR, IRQF_TRIGGER_LOW, HIF_MOD_NAME, prNetDevice)) {
		DBGLOG(INIT, ERROR, "request irq %d fail!\n", MT_WF_HIF_IRQ_ID);
		return -1;
	}
#if (CONF_HIF_DMA_INT == 1)
	if (request_irq(MT_GDMA2_IRQ_ID, HifDmaISR, IRQF_TRIGGER_LOW, "AHB_DMA", prNetDevice)) {
		DBGLOG(INIT, ERROR, "request irq %d fail!\n", MT_GDMA2_IRQ_ID);
		free_irq(MT_WF_HIF_IRQ_ID, prNetDevice);
		return -1;
	}
#endif /* CONF_HIF_DMA_INT */

	return ret;

}				/* end of glBusSetIrq() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Stop bus interrupt operation and disable interrupt handling for os.
*
* \param[in] pvData     A pointer to struct net_device.
* \param[in] pvCookie   Private data for pfnIsr function.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glBusFreeIrq(PVOID pvData, PVOID pvCookie)
{
	struct net_device *prNetDevice;
	GLUE_INFO_T *GlueInfo;
	GL_HIF_INFO_T *HifInfo;

	/* Init */
	ASSERT(pvData);
	if (!pvData)
		return;

	prNetDevice = (struct net_device *)pvData;
	GlueInfo = (GLUE_INFO_T *) pvCookie;
	ASSERT(GlueInfo);
	if (!GlueInfo)
		return;

	HifInfo = &GlueInfo->rHifInfo;

	/* Free the IRQ */
	free_irq(MT_WF_HIF_IRQ_ID, prNetDevice);
	return;

}				/* end of glBusreeIrq() */
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Read a 32-bit device register
*
* \param[in] GlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] RegOffset Register offset
* \param[in] pu4Value   Pointer to variable used to store read value
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalDevRegRead(IN GLUE_INFO_T *GlueInfo, IN UINT_32 RegOffset, OUT UINT_32 *pu4Value)
{
	GL_HIF_INFO_T *HifInfo;

	/* sanity check and init */
	ASSERT(GlueInfo);
	ASSERT(pu4Value);
	HifInfo = &GlueInfo->rHifInfo;

	/* use PIO mode to read register */
	*pu4Value = HIF_REG_READL(HifInfo, RegOffset);

	HIF_DBG(("[WiFi/HIF] kalDevRegRead 0x%x = 0x%x\n", RegOffset, *pu4Value));

	return TRUE;

}				/* end of kalDevRegRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Write a 32-bit device register
*
* \param[in] GlueInfo   Pointer to the GLUE_INFO_T structure.
* \param[in] RegOffset  Register offset
* \param[in] RegValue   RegValue to be written
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalDevRegWrite(IN GLUE_INFO_T *GlueInfo, IN UINT_32 RegOffset, IN UINT_32 RegValue)
{
	GL_HIF_INFO_T *HifInfo;

	/* sanity check and init */
	ASSERT(GlueInfo);
	HifInfo = &GlueInfo->rHifInfo;

	/* use PIO mode to write register */
	HIF_REG_WRITEL(HifInfo, RegOffset, RegValue);

	HIF_DBG(("[WiFi/HIF] kalDevRegWrite 0x%x = 0x%x\n", RegOffset, RegValue));

	return TRUE;

}				/* end of kalDevRegWrite() */


/*----------------------------------------------------------------------------*/
/*!
* \brief Read device I/O port
*
* \param[in] GlueInfo   Pointer to the GLUE_INFO_T structure.
* \param[in] Port       I/O port offset
* \param[in] Size       Length to be read
* \param[out] Buf       Pointer to read buffer
* \param[in] MaxBufSize Length of the buffer valid to be accessed
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
kalDevPortRead(IN P_GLUE_INFO_T GlueInfo, IN UINT_16 Port, IN UINT_32 Size, OUT PUINT_8 Buf, IN UINT_32 MaxBufSize)
{
	GL_HIF_INFO_T *HifInfo;
#if (CONF_MTK_AHB_DMA == 1)
	MTK_WCN_HIF_DMA_CONF DmaConf;
	UINT_32 LoopCnt;
#endif /* CONF_MTK_AHB_DMA */
	UINT_32 IdLoop, MaxLoop;
	UINT_32 *LoopBuf;
	unsigned long PollTimeout;

    sdio_gen3_cmd53_info info;
	struct sdio_func *func = &g_sdio_func;
	UINT_32 count;


	/* sanity check */
	if ((WlanDmaFatalErr == 1) || (fgIsResetting == TRUE) || (HifIsFwOwn(GlueInfo->prAdapter) == TRUE)) {
		/* #ifdef CONF_HIF_CONNSYS_DBG */
#if 0
		/* avoid log is overwrited */
		if (WlanDmaFatalErr == 1) {
			/* dump information every 20 times */
			static int testrx;
			HifInfo = &GlueInfo->rHifInfo;
			if (testrx++ >= 20) {
				testrx = 0;
				goto LabelErr;
			}
		}
#endif /* CONF_HIF_CONNSYS_DBG */

		return TRUE;
	}

	/* Init */
	ASSERT(GlueInfo);
	HifInfo = &GlueInfo->rHifInfo;

	ASSERT(Buf);
	ASSERT(Size <= MaxBufSize);


	{/* sdio like operation */

	/* CMD53 port mode to write n-byte, if count >= block size => block mode, otherwise =>	byte mode  */
	count = ALIGN_4(Size);

	/* 1. Setup command information */
	info.word = 0;
	info.field.rw_flag = SDIO_GEN3_READ;
	info.field.func_num = func->num;

	if (count >= func->cur_blksize) { /* DMA/PIO block mode */
		info.field.block_mode = SDIO_GEN3_BLOCK_MODE;
		info.field.count = count/func->cur_blksize;
		if (count % func->cur_blksize > 0)
			info.field.count++;
		count = info.field.count * func->cur_blksize;
		if (count > MaxBufSize) {
			DBGLOG(RX, ERROR, "blk mode rxCnt 0x%x, MaxSz 0x%x, OrigSz 0x%x\n", count, MaxBufSize, Size);
			ASSERT(0);
		}
	} else { /* DMA/PIO byte mode */
		if (func->use_dma && (Port != MCR_WHISR)) /* safe for reading 4 bytes WHISR */
			count = ((Size + 7) & ~7u); /* if DMA mode, RX 8 bytes alignment is required */
		info.field.block_mode = SDIO_GEN3_BYTE_MODE;
		info.field.count = count;
		if (count > MaxBufSize) {
			DBGLOG(RX, ERROR, "byte mode rxCnt 0x%x, MaxSz 0x%x, OrigSz 0x%x\n", count, MaxBufSize, Size);
			ASSERT(0);
		}
	}

	info.field.op_mode = SDIO_GEN3_FIXED_PORT_MODE; /* fix mode */
	info.field.addr = Port;


	DBGLOG(RX, TRACE, "use_dma(%d), count(%d->%d), blk size(%d), CMD_SETUP(0x%x)\n",
		 func->use_dma, Size, count, func->cur_blksize, info.word);

#if (CONF_MTK_AHB_DMA == 1)
	if (func->use_dma && (HifInfo->fgDmaEnable == TRUE) && (HifInfo->DmaOps != NULL)
		&& ((Port == MCR_WRDR0) || (Port == MCR_WRDR1))) {

		if (pfWlanDmaOps != NULL)
			pfWlanDmaOps->DmaClockCtrl(TRUE);
	}
#endif

	my_sdio_disable(HifLock);
	__disable_irq();

	writel(info.word, (volatile UINT_32 *)(*g_pHifRegBaseAddr + SDIO_GEN3_CMD_SETUP));
	wmb();
	DBGLOG(RX, TRACE, "basic writel CmdInfo addr = %x, info = %x, HifBase = %p\n", Port, info.word, HifInfo->HifRegBaseAddr);

	}


	/* Read */

#if (CONF_MTK_AHB_DMA == 1)
	if (func->use_dma && (HifInfo->fgDmaEnable == TRUE) && (HifInfo->DmaOps != NULL)
		&& ((Port == MCR_WRDR0) || (Port == MCR_WRDR1))) {
		/* only for data port */
#ifdef MTK_DMA_BUF_MEMCPY_SUP
		VOID *DmaVBuf = NULL, *DmaPBuf = NULL;
#endif /* MTK_DMA_BUF_MEMCPY_SUP */

		/* config DMA, Port = MCR_WRDR0 or MCR_WRDR1 */
		DmaConf.Count = count;
		DmaConf.Dir = HIF_DMA_DIR_RX;
		DmaConf.Src = HIF_DRV_BASE  + SDIO_GEN3_CMD53_DATA;	/* must be physical addr */

		/* TODO: use dma_unmap_single(struct device *dev, dma_addr_t handle,
		   size_t size, enum dma_data_direction dir) */

#ifdef MTK_DMA_BUF_MEMCPY_SUP
		DmaConf.Dst = kalIOPhyAddrGet(Buf);	/* must be physical addr */

		/* TODO: use virt_to_phys() */
		if (DmaConf.Dst == NULL) {
			HIF_DBG(("[WiFi/HIF] Use Dma Buffer to RX packet (%d %d)...\n", Size, CFG_RX_MAX_PKT_SIZE));
			ASSERT(Size <= CFG_RX_MAX_PKT_SIZE);

			kalDmaBufGet(&DmaVBuf, &DmaPBuf);
			DmaConf.Dst = (ULONG) DmaPBuf;
		}
#else
		/*
		   http://kernelnewbies.org/KernelMemoryAllocation
		   Since the cache-coherent mapping may be expensive, also a streaming allocation exists.

		   This is a buffer for one-way communication, which means coherency is limited to
		   flushing the data from the cache after a write finishes. The buffer has to be
		   pre-allocated (e.g. using kmalloc()). DMA for it is set up with dma_map_single().

		   When the DMA is finished (e.g. when the device has sent an interrupt signaling end of
		   DMA), call dma_unmap_single(). Between map and unmap, the device is in control of the
		   buffer: if you write to the device, do it before dma_map_single(), if you read from
		   it, do it after dma_unmap_single().
		 */
		/* DMA_FROM_DEVICE invalidated (without writeback) the cache */
		/* TODO: if dst_off was not cacheline aligned? */

		DmaConf.Dst = dma_map_single(HifInfo->Dev, Buf, count, DMA_FROM_DEVICE);

#endif /* MTK_DMA_BUF_MEMCPY_SUP */

		/* start to read data */
		AP_DMA_HIF_LOCK(HifInfo);	/* lock to avoid other codes config GDMA */

		HifInfo->DmaOps->DmaConfig(HifInfo, &DmaConf);
		HifInfo->DmaOps->DmaStart(HifInfo);

		/* TODO: use interrupt mode */
		/* TODO: use timeout, not loopcnt */
		/* TODO: use semaphore */
#if (CONF_HIF_DMA_INT == 1)
		RtnVal = wait_event_interruptible_timeout(HifInfo->HifDmaWaitq, (HifInfo->HifDmaWaitFlg != 0), 1000);
		if (RtnVal <= 0)
			DBGLOG(RX, ERROR, "fatal error1! reset DMA!\n");
		HifInfo->HifDmaWaitFlg = 0;
#else

		LoopCnt = 0;
		PollTimeout = jiffies + HZ * 5;

		do {
			if (time_before(jiffies, PollTimeout)) {
				/* Do nothing */
				/* not timeout, continue to poll */
			} else {

#if (CONF_HIF_CONNSYS_DBG == 0)
				/* TODO: impossible! reset DMA */
				DBGLOG(RX, ERROR, "fatal error1! reset DMA!\n");
				break;
#else
				/*
				   never break and just wait for response from HIF

				   because when we use ICE on CONNSYS, we will break the CPU and do debug,
				   but maybe AP side continues to send packets to HIF, HIF buffer of CONNSYS
				   will be full and we will do DMA reset here then CONNSYS CPU will also be reset.
				 */
				DBGLOG(RX, WARN, "DMA LoopCnt > 100000... (%lu %lu)\n", jiffies, PollTimeout);
/* LabelErr: */
				if (HifInfo->DmaOps->DmaRegDump != NULL)
					HifInfo->DmaOps->DmaRegDump(HifInfo);
				/* HifRegDump(GlueInfo->prAdapter); */
				LoopCnt = 0;
				WlanDmaFatalErr = 1;

#if (CONF_HIF_DMA_DBG == 0)
				{
				/* UCHAR AeeBuffer[100]; */
				UINT_32 FwCnt;
				/* UINT_32 RegValChip, RegValLP; */
				for (FwCnt = 0; FwCnt < 512; FwCnt++) {
					/* DBGLOG(RX, WARN, "0x%08x ", MCU_REG_READL(HifInfo, CONN_MCU_CPUPCR)); */
					/* CONSYS_REG_READ(CONSYS_CPUPCR_REG) */
					if ((FwCnt + 1) % 16 == 0)
						DBGLOG(RX, WARN, "\n");
				}
				DBGLOG(RX, WARN, "\n\n");
				return TRUE;
				}
#endif /* CONF_HIF_DMA_DBG */
#endif /* CONF_HIF_CONNSYS_DBG */
			}
		} while (!HifInfo->DmaOps->DmaPollIntr(HifInfo));
#endif /* CONF_HIF_DMA_INT */

		HifInfo->DmaOps->DmaAckIntr(HifInfo);
		HifInfo->DmaOps->DmaStop(HifInfo);

		LoopCnt = 0;
		do {
			if (LoopCnt++ > 100000) {
				/* TODO: impossible! reset DMA */
				DBGLOG(RX, ERROR, "fatal error2! reset DMA!\n");
				break;
			}
		} while (HifInfo->DmaOps->DmaPollStart(HifInfo) != 0);

		AP_DMA_HIF_UNLOCK(HifInfo);

#ifdef MTK_DMA_BUF_MEMCPY_SUP
		if (DmaVBuf != NULL)
			kalMemCopy(Buf, DmaVBuf, Size);
#else

		dma_unmap_single(HifInfo->Dev, DmaConf.Dst, count, DMA_FROM_DEVICE);
#endif /* MTK_DMA_BUF_MEMCPY_SUP */

		__enable_irq();
		my_sdio_enable(HifLock);

		if (pfWlanDmaOps != NULL)
			pfWlanDmaOps->DmaClockCtrl(FALSE);

		HIF_DBG(("[WiFi/HIF] DMA RX OK!\n"));
	} else
#endif /* CONF_MTK_AHB_DMA */
	{
		/* default PIO mode */
		MaxLoop = count >> 2;
		LoopBuf = (UINT_32 *) Buf;

		for (IdLoop = 0; IdLoop < MaxLoop; IdLoop++) {
			*LoopBuf = readl((volatile UINT_32 *)(*g_pHifRegBaseAddr + SDIO_GEN3_CMD53_DATA));
			DBGLOG(RX, TRACE, "basic readl idx = %x, addr = %x, rVal = %x, HifBase = %p\n", IdLoop, Port, *LoopBuf, HifInfo->HifRegBaseAddr);
			LoopBuf++;
		}

		__enable_irq();
		my_sdio_enable(HifLock);

	}

	return TRUE;
}				/* end of kalDevPortRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Write device I/O port
*
* \param[in] GlueInfo	Pointer to the GLUE_INFO_T structure.
* \param[in] Port		I/O port offset
* \param[in] Size		Length to be write
* \param[in] Buf		Pointer to write buffer
* \param[in] MaxBufSize Length of the buffer valid to be accessed
*
* \retval TRUE			operation success
* \retval FALSE 		operation fail
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
kalDevPortWrite(IN P_GLUE_INFO_T GlueInfo, IN UINT_16 Port, IN UINT_32 Size, IN PUINT_8 Buf, IN UINT_32 MaxBufSize)
{
	GL_HIF_INFO_T *HifInfo;
#if (CONF_MTK_AHB_DMA == 1)
	MTK_WCN_HIF_DMA_CONF DmaConf;
	UINT_32 LoopCnt;
#endif /* CONF_MTK_AHB_DMA */
/* UINT8 *BufSrc, *BufLast; */
/* UINT8 Last4B[4]; */
	unsigned long PollTimeout;

    sdio_gen3_cmd53_info info;
	struct sdio_func *func = &g_sdio_func;
	UINT_32 count;

#if DBG
	/* DBGLOG(INIT, INFO,
	(KERN_INFO DRV_NAME"++kalDevPortWrite++ buf:0x%p, port:0x%x, length:%d\n", Buf, Port, Size)); */
#endif
	/* DBGLOG(INIT, INFO,
	("++kalDevPortWrite++ buf:0x%p, port:0x%x, length:%d\n", Buf, Port, Size)); //samp */

	/* sanity check */
	if ((WlanDmaFatalErr == 1) || (fgIsResetting == TRUE) || (HifIsFwOwn(GlueInfo->prAdapter) == TRUE)) {
		DBGLOG(RX, ERROR, "WlanDmaFatalErr: %d, fgIsResetting: %d, HifIsFwOwn: %d\n",
				WlanDmaFatalErr, fgIsResetting, HifIsFwOwn(GlueInfo->prAdapter));
		return FALSE;
	}

	/* Init */
#ifdef HIF_DEBUG_SUP_TX
/* dumpMemory8(Buf, Size); */
#endif /* HIF_DEBUG_SUP_TX */

	ASSERT(GlueInfo);
	HifInfo = &GlueInfo->rHifInfo;

	ASSERT(Buf);
	ASSERT(Size <= MaxBufSize);

#if (MTK_WCN_SINGLE_MODULE == 0)
	HifTxCnt++;
#endif


	{/* sdio like operation */
    /* CMD53 port mode to write n-byte, if count >= block size => block mode, otherwise =>  byte mode  */
    count = ALIGN_4(Size);

    /* 1. Setup command information */
    info.word = 0;
    info.field.rw_flag = SDIO_GEN3_WRITE;
    info.field.func_num = func->num;

	/* MT6797 TODO */
	if (count >= func->cur_blksize) {
		info.field.block_mode = SDIO_GEN3_BLOCK_MODE; /* block  mode */
		info.field.count = count/func->cur_blksize;
		if (count % func->cur_blksize > 0)
			info.field.count++;
		count = info.field.count * func->cur_blksize;
		if (count > MaxBufSize) {
			DBGLOG(TX, ERROR, "blk mode txCnt 0x%x, MaxSz 0x%x, origSz 0x%x\n", count, MaxBufSize, Size);
			ASSERT(0);
		}
	} else {
		info.field.block_mode = SDIO_GEN3_BYTE_MODE; /* byte  mode */
		info.field.count = count;
    }

	info.field.op_mode = SDIO_GEN3_FIXED_PORT_MODE; /* fix mode */
	info.field.addr = Port;


	DBGLOG(TX, TRACE, "use_dma(%d), count(%d->%d), blk size(%d), CMD_SETUP(0x%x)\n",
		func->use_dma, Size, count, func->cur_blksize, info.word);

#if (CONF_MTK_AHB_DMA == 1)
	if (func->use_dma && (HifInfo->fgDmaEnable == TRUE) && (HifInfo->DmaOps != NULL) &&
		(Port == MCR_WTDR1))

	{
		if (pfWlanDmaOps != NULL)
			pfWlanDmaOps->DmaClockCtrl(TRUE);
	}
#endif

	my_sdio_disable(HifLock);
	__disable_irq();

	writel(info.word, (volatile UINT_32 *)(*g_pHifRegBaseAddr + SDIO_GEN3_CMD_SETUP));
	wmb();
	DBGLOG(TX, TRACE, "basic writel CmdInfo addr = %x, info = %x, HifBase = %p\n", Port, info.word, HifInfo->HifRegBaseAddr);	
	}



	/* Write */
#if (CONF_MTK_AHB_DMA == 1)
	if (func->use_dma && (HifInfo->fgDmaEnable == TRUE) && (HifInfo->DmaOps != NULL) &&
		(Port == MCR_WTDR1))

	{
		/* only for data port */
#ifdef MTK_DMA_BUF_MEMCPY_SUP
		VOID *DmaVBuf = NULL, *DmaPBuf = NULL;
#endif /* MTK_DMA_BUF_MEMCPY_SUP */

		/* config GDMA */
		HIF_DBG_TX(("[WiFi/HIF/DMA] Prepare to send data...\n"));
		DmaConf.Count = count;
		DmaConf.Dir = HIF_DMA_DIR_TX;
		DmaConf.Dst = HIF_DRV_BASE  + SDIO_GEN3_CMD53_DATA;	/* must be physical addr */

#ifdef MTK_DMA_BUF_MEMCPY_SUP
		DmaConf.Src = kalIOPhyAddrGet(Buf);	/* must be physical addr */

		/* TODO: use virt_to_phys() */
		if (DmaConf.Src == NULL) {
			HIF_DBG_TX(("[WiFi/HIF] Use Dma Buffer to TX packet (%d %d)...\n", Size, CFG_RX_MAX_PKT_SIZE));
			ASSERT(Size <= CFG_RX_MAX_PKT_SIZE);

			kalDmaBufGet(&DmaVBuf, &DmaPBuf);
			DmaConf.Src = (ULONG) DmaPBuf;

			kalMemCopy(DmaVBuf, Buf, Size);
		}
#else

		/* DMA_TO_DEVICE writeback the cache */
		DmaConf.Src = dma_map_single(HifInfo->Dev, Buf, count, DMA_TO_DEVICE);

#endif /* MTK_DMA_BUF_MEMCPY_SUP */

		/* start to write */
		AP_DMA_HIF_LOCK(HifInfo);

		HifInfo->DmaOps->DmaConfig(HifInfo, &DmaConf);
		HifInfo->DmaOps->DmaStart(HifInfo);

		/* TODO: use interrupt mode */
		/* TODO: use timeout, not loopcnt */
		/* TODO: use semaphore */
#if (CONF_HIF_DMA_INT == 1)
		RtnVal = wait_event_interruptible_timeout(HifInfo->HifDmaWaitq, (HifInfo->HifDmaWaitFlg != 0), 1000);
		if (RtnVal <= 0) {
			/* HifRegDump(GlueInfo->prAdapter); */
			/* if (HifInfo->DmaOps->DmaRegDump != NULL) */
			/* HifInfo->DmaOps->DmaRegDump(HifInfo); */
			/* kalSendAeeWarning( */
			while (1)
				DBGLOG(TX, ERROR, "fatal error1! reset DMA!\n");
		}
		HifInfo->HifDmaWaitFlg = 0;
#else

		LoopCnt = 0;
		PollTimeout = jiffies + HZ * 5;

		do {

			if (time_before(jiffies, PollTimeout)) {
				/* Do nothing */
				/* not timeout, continue to poll */
			} else {

#if (CONF_HIF_CONNSYS_DBG == 0)
				/* TODO: impossible! reset DMA */
				DBGLOG(TX, ERROR, "fatal error1! reset DMA!\n");
				break;
#else

				DBGLOG(TX, INFO, "DMA LoopCnt > 100000... (%lu %lu)\n", jiffies, PollTimeout);
/* LabelErr: */
				if (HifInfo->DmaOps->DmaRegDump != NULL)
					HifInfo->DmaOps->DmaRegDump(HifInfo);
				LoopCnt = 0;
				/* HifRegDump(GlueInfo->prAdapter); */
				WlanDmaFatalErr = 1;

#if (CONF_HIF_DMA_DBG == 0)
				{
/* UCHAR AeeBuffer[100]; */
					UINT_32 FwCnt;
					/* UINT_32 RegValChip, RegValLP; */
					DBGLOG(TX, WARN, "CONNSYS FW CPUINFO:\n");
					for (FwCnt = 0; FwCnt < 512; FwCnt++) {
						/* DBGLOG(TX, WARN, "0x%08x ", MCU_REG_READL(HifInfo, CONN_MCU_CPUPCR)); */
					if ((FwCnt + 1) % 16 == 0)
						DBGLOG(TX, WARN, "\n");
					}

					return TRUE;
				}
#endif /* CONF_HIF_DMA_DBG */
#endif /* CONF_HIF_CONNSYS_DBG */
			}
		} while (!HifInfo->DmaOps->DmaPollIntr(HifInfo));
#endif /* CONF_HIF_DMA_INT */

		HifInfo->DmaOps->DmaAckIntr(HifInfo);
		HifInfo->DmaOps->DmaStop(HifInfo);

		LoopCnt = 0;
		do {
			if (LoopCnt++ > 100000) {
				DBGLOG(TX, ERROR, "fatal error2! reset DMA!\n");
				break;
			}
		} while (HifInfo->DmaOps->DmaPollStart(HifInfo) != 0);

		AP_DMA_HIF_UNLOCK(HifInfo);

#ifndef MTK_DMA_BUF_MEMCPY_SUP
		dma_unmap_single(HifInfo->Dev, DmaConf.Src, count, DMA_TO_DEVICE);
#endif /* MTK_DMA_BUF_MEMCPY_SUP */

		__enable_irq();
		my_sdio_enable(HifLock);

		if (pfWlanDmaOps != NULL)
			pfWlanDmaOps->DmaClockCtrl(FALSE);

		HIF_DBG_TX(("[WiFi/HIF] DMA TX OK!\n"));
	} else
#endif /* CONF_MTK_AHB_DMA */
	{
		UINT_32 IdLoop, MaxLoop;
		UINT_32 *LoopBuf;

		/* PIO mode */
		MaxLoop = count >> 2;
		LoopBuf = (UINT_32 *) Buf;

		HIF_DBG_TX(("[WiFi/HIF/PIO] Prepare to send data (%d 0x%p-0x%p)...\n",
			    Size, LoopBuf, (((UINT8 *) LoopBuf) + (Size & (~0x03)))));


		for (IdLoop = 0; IdLoop < MaxLoop; IdLoop++) {
			writel(*LoopBuf, (volatile UINT_32 *)(*g_pHifRegBaseAddr + SDIO_GEN3_CMD53_DATA));
			DBGLOG(TX, TRACE, "basic writel idx = %x, addr = %x, wVal = %x, HifBase = %p\n", IdLoop, Port, *LoopBuf, HifInfo->HifRegBaseAddr);
			LoopBuf++;
		}

		__enable_irq();
		my_sdio_enable(HifLock);

		HIF_DBG_TX(("\n\n"));
	}

	return TRUE;

}				/* end of kalDevPortWrite() */

/*******************************************************************************
*                       P R I V A T E   F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a SDIO interrupt callback function
*
* \param[in] func  pointer to SDIO handle
*
* \return void
*/
/*----------------------------------------------------------------------------*/
static irqreturn_t HifAhbISR(IN int Irq, IN void *Arg)
{
	struct net_device *prNetDevice = (struct net_device *)Arg;
	GLUE_INFO_T *GlueInfo;
	GL_HIF_INFO_T *HifInfo;

	/* Init */
	IsrCnt++;
	ASSERT(prNetDevice);
	GlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDevice));
	ASSERT(GlueInfo);

	if (!GlueInfo)
		return IRQ_HANDLED;

	HifInfo = &GlueInfo->rHifInfo;
	GlueInfo->IsrCnt++;
	if (GlueInfo->ulFlag & GLUE_FLAG_HALT) {
		__disable_irq();
		return IRQ_HANDLED;
	}

	__disable_irq();

	/* lock 100ms to avoid suspend */
    /* MT6797 TODO */
	/* kalHifAhbKalWakeLockTimeout(GlueInfo); */

	/* Wake up main thread */
	set_bit(GLUE_FLAG_INT_BIT, &GlueInfo->ulFlag);

	/* when we got sdio interrupt, we wake up the tx servie thread */

	wake_up_interruptible(&GlueInfo->waitq_hif);

	IsrPassCnt++;
	GlueInfo->IsrPassCnt++;
	return IRQ_HANDLED;

}

#if (CONF_HIF_DMA_INT == 1)
/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a SDIO interrupt callback function
*
* \param[in] func  pointer to SDIO handle
*
* \return void
*/
/*----------------------------------------------------------------------------*/

static irqreturn_t HifDmaISR(IN int Irq, IN void *Arg)
{
	struct net_device *prNetDevice = (struct net_device *)Arg;
	GLUE_INFO_T *GlueInfo;
	GL_HIF_INFO_T *HifInfo;

	/* Init */
	ASSERT(prNetDevice);
	GlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDevice));
	ASSERT(GlueInfo);

	if (!GlueInfo)
		return IRQ_HANDLED;
	HifInfo = &GlueInfo->rHifInfo;

	/* disable interrupt */
	HifInfo->DmaOps->DmaAckIntr(HifInfo);

	/* Wake up main thread */
	set_bit(1, &HifInfo->HifDmaWaitFlg);

	/* when we got sdio interrupt, we wake up the tx servie thread */
	wake_up_interruptible(&HifInfo->HifDmaWaitq);

	return IRQ_HANDLED;

}
#endif /* CONF_HIF_DMA_INT */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a SDIO probe function
*
* \param[in] func   pointer to SDIO handle
* \param[in] id     pointer to SDIO device id table
*
* \return void
*/
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_MTK_CLKMGR)
#if defined(MTK_EXTERNAL_LDO) || defined(MTK_ALPS_BOX_SUPPORT)
#include <mach/mt_gpio.h>
#endif
#endif

static int HifAhbProbe(VOID)
{
	int Ret = 0;

	DBGLOG(INIT, INFO, "HifAhbProbe()\n");

	/* power on WiFi TX PA 3.3V and HIF GDMA clock */
	{
#ifdef CONFIG_MTK_PMIC_MT6397
#if defined(CONFIG_MTK_CLKMGR)
#ifdef MTK_EXTERNAL_LDO
		/* for 8127 tablet */
		mt_set_gpio_mode(GPIO51, GPIO_MODE_04);
		mt_set_gpio_dir(GPIO51, GPIO_DIR_OUT);
		mt_set_gpio_pull_enable(GPIO51, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO51, GPIO_PULL_UP);
#elif defined(MTK_ALPS_BOX_SUPPORT)
		/* for 8127 box */
		mt_set_gpio_mode(GPIO89, GPIO_MODE_04);
		mt_set_gpio_dir(GPIO89, GPIO_DIR_OUT);
		mt_set_gpio_pull_enable(GPIO89, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO89, GPIO_PULL_UP);
#else
		hwPowerOn(MT65XX_POWER_LDO_VGP4, VOL_3300, "WLAN");
#endif
#endif
#else
#ifdef CONFIG_OF		/*for MT6752 */
		mtk_wcn_consys_hw_wifi_paldo_ctrl(1);	/* switch to HW mode */
#else				/*for MT6572/82/92 */
		hwPowerOn(MT6323_POWER_LDO_VCN33_WIFI, VOL_3300, "WLAN");
		upmu_set_vcn33_on_ctrl_wifi(1);	/* switch to HW mode */
#endif
#endif

	}

#if (CONF_HIF_DEV_MISC == 1)
	if (pfWlanProbe((PVOID) &MtkAhbDriver.this_device) != WLAN_STATUS_SUCCESS) {
#else
	if (pfWlanProbe((PVOID) &HifAhbPDev->dev) != WLAN_STATUS_SUCCESS) {
#endif /* CONF_HIF_DEV_MISC */

		pfWlanRemove();
		Ret = -1;
	}

	return Ret;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will do module remove.
*
* \param[in] None
*
* \return The result of remove (WLAN_STATUS_SUCCESS = 0)
*/
/*----------------------------------------------------------------------------*/
static int HifAhbRemove(VOID)
{

	pfWlanRemove();

	{
#ifdef CONFIG_MTK_PMIC_MT6397
#if defined(CONFIG_MTK_CLKMGR)
#ifdef MTK_EXTERNAL_LDO
		/* for 8127 tablet */
		mt_set_gpio_mode(GPIO51, GPIO_MODE_04);
		mt_set_gpio_dir(GPIO51, GPIO_DIR_OUT);
		mt_set_gpio_pull_enable(GPIO51, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO51, GPIO_PULL_DOWN);
#elif defined(MTK_ALPS_BOX_SUPPORT)
		/* for 8127 box */
		mt_set_gpio_mode(GPIO89, GPIO_MODE_04);
		mt_set_gpio_dir(GPIO89, GPIO_DIR_OUT);
		mt_set_gpio_pull_enable(GPIO89, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO89, GPIO_PULL_DOWN);
#else
		hwPowerDown(MT65XX_POWER_LDO_VGP4, "WLAN");
#endif
#endif
#else
#ifdef CONFIG_OF		/*for MT6752 */
		mtk_wcn_consys_hw_wifi_paldo_ctrl(0);	/* switch to SW mode */
#else				/*for MT6572/82/92 */
		upmu_set_vcn33_on_ctrl_wifi(0);	/* switch to SW mode */
		hwPowerDown(MT6323_POWER_LDO_VCN33_WIFI, "WLAN");
#endif
#endif

	}

	return 0;
}

#if (MTK_WCN_SINGLE_MODULE == 0)
/*----------------------------------------------------------------------------*/
/*!
* \brief This function gets the TX count pass through HIF AHB bus.
*
* \param[in] None
*
* \return TX count
*/
/*----------------------------------------------------------------------------*/
static int HifAhbBusCntGet(VOID)
{
	return HifTxCnt;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function resets the TX count pass through HIF AHB bus.
*
* \param[in] None
*
* \return 0
*/
/*----------------------------------------------------------------------------*/
static int HifAhbBusCntClr(VOID)
{
	HifTxCnt = 0;
	return 0;
}
#endif /* MTK_WCN_SINGLE_MODULE */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function configs the DMA TX/RX settings before any real TX/RX.
*
* \param[in] GlueInfo       Pointer to the GLUE_INFO_T structure.
* \param[in] BurstLen       0(1DW), 1(4DW), 2(8DW), Others(Reserved)
* \param[in] PortId         0(TXD0), 1(TXD1), 2(RXD0), 3(RXD1), 4(WHISR enhance)
* \param[in] TransByte      Should be 4-byte align.
*
* \return void
*/
/*----------------------------------------------------------------------------*/

VOID glSetPowerState(IN GLUE_INFO_T *GlueInfo, IN UINT_32 ePowerMode)
{

}

#if (CONF_HIF_DEV_MISC == 1)
/* no use */
static ssize_t HifAhbMiscRead(IN struct file *Filp, OUT char __user *DstBuf, IN size_t Size, IN loff_t *Ppos)
{
	return 0;
}

static ssize_t HifAhbMiscWrite(IN struct file *Filp, IN const char __user *SrcBuf, IN size_t Size, IN loff_t *Ppos)
{
	return 0;
}

static int HifAhbMiscIoctl(IN struct file *Filp, IN unsigned int Cmd, IN unsigned long arg)
{
	return 0;
}

static int HifAhbMiscOpen(IN struct inode *Inodep, IN struct file *Filp)
{
	return 0;
}

static int HifAhbMiscClose(IN struct inode *Inodep, IN struct file *Filp)
{
	return 0;
}
#else

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called by OS platform device module.
*
* \param[in] PDev           Pointer to the platform device structure.
*
* \return 0
*/
/*----------------------------------------------------------------------------*/
static int HifAhbPltmProbe(IN struct platform_device *PDev)
{
	HifAhbPDev = PDev;

	DBGLOG(INIT, INFO, "HifAhbPltmProbe\n");

#if (CONF_HIF_PMIC_TEST == 1)
	wmt_set_jtag_for_mcu();
	wmt_set_jtag_for_gps();

#endif /* CONF_HIF_PMIC_TEST */

#if (MTK_WCN_SINGLE_MODULE == 1)
	HifAhbProbe();		/* only for test purpose without WMT module */

#else

	/* register WiFi function to WMT */
	DBGLOG(INIT, INFO, "mtk_wcn_wmt_wlan_reg\n");
	{
		MTK_WCN_WMT_WLAN_CB_INFO WmtCb;

		WmtCb.wlan_probe_cb = HifAhbProbe;
		WmtCb.wlan_remove_cb = HifAhbRemove;
		WmtCb.wlan_bus_cnt_get_cb = HifAhbBusCntGet;
		WmtCb.wlan_bus_cnt_clr_cb = HifAhbBusCntClr;
		mtk_wcn_wmt_wlan_reg(&WmtCb);
	}
#endif /* MTK_WCN_SINGLE_MODULE */
	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called by OS platform device module.
*
* \param[in] PDev           Pointer to the platform device structure.
*
* \return 0
*/
/*----------------------------------------------------------------------------*/
static int __exit HifAhbPltmRemove(IN struct platform_device *PDev)
{
#if (MTK_WCN_SINGLE_MODULE == 0)
	mtk_wcn_wmt_wlan_unreg();
#endif /* MTK_WCN_SINGLE_MODULE */
	return 0;
}

#ifdef CONFIG_PM
/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called by OS platform device module.
*
* \param[in] PDev           Pointer to the platform device structure.
* \param[in] Message
*
* \return 0
*/
/*----------------------------------------------------------------------------*/
static int HifAhbPltmSuspend(IN struct platform_device *PDev, pm_message_t Message)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called by OS platform device module.
*
* \param[in] PDev           Pointer to the platform device structure.
*
* \return 0
*/
/*----------------------------------------------------------------------------*/
static int HifAhbPltmResume(IN struct platform_device *PDev)
{
	return 0;
}
#endif /* CONFIG_PM */

#endif /* CONF_HIF_DEV_MISC */

#if (CONF_HIF_LOOPBACK_AUTO == 1)
/*----------------------------------------------------------------------------*/
/*!
* \brief Trigger to do HIF loopback test.
*
* \param[in] arg   Pointer to the GLUE_INFO_T structure.
*
* \retval None
*/
/*----------------------------------------------------------------------------*/
static VOID HifAhbLoopbkAuto(IN unsigned long arg)
{

	P_GLUE_INFO_T GlueInfo = (P_GLUE_INFO_T) arg;
	GL_HIF_INFO_T *HifInfo = &GlueInfo->rHifInfo;

	ASSERT(GlueInfo);

	HIF_DBG(("[WiFi/HIF] Trigger to do loopback test...\n"));

	set_bit(GLUE_FLAG_HIF_LOOPBK_AUTO_BIT, &HifInfo->HifLoopbkFlg);
	wake_up_interruptible(&HifInfo->HifWaitq);

}
#endif /* CONF_HIF_LOOPBACK_AUTO */

VOID glDumpConnSysCpuInfo(P_GLUE_INFO_T prGlueInfo)
{
/* MT6797 TODO */
/*	GL_HIF_INFO_T *prHifInfo = &prGlueInfo->rHifInfo; */
	unsigned short j;

	for (j = 0; j < 512; j++) {
/*		DBGLOG(INIT, WARN, "0x%08x ", MCU_REG_READL(prHifInfo, CONN_MCU_CPUPCR)); */
		if ((j + 1) % 16 == 0)
			DBGLOG(INIT, WARN, "\n");
	}
}

PUINT_8 glRemapConnsysAddr(P_GLUE_INFO_T prGlueInfo, UINT_32 consysAddr, UINT_32 remapLength)
{
	/* 0x180E0000 is the customized address and can be remaped to any connsys address */
	PUINT_8 pucRemapCrAddr = NULL;
	GL_HIF_INFO_T *hifInfo = &prGlueInfo->rHifInfo;
	UINT_32 u4ConfCrValue = 0;

	u4ConfCrValue = readl(hifInfo->confRegBaseAddr);
	if ((u4ConfCrValue & 0xFFFF0000) != 0x180E0000) {
		DBGLOG(RX, ERROR, "remap CR is used by others, value is %u\n", u4ConfCrValue);
		return NULL;
	}
	u4ConfCrValue &= 0xFFFF; /* don't touch low 16 bits, since it is used by others */
	u4ConfCrValue |= consysAddr; /* the start address in connsys side */
	writel(u4ConfCrValue, hifInfo->confRegBaseAddr);
	pucRemapCrAddr = ioremap(DYNAMIC_REMAP_BASE, remapLength);
	return pucRemapCrAddr;
}

VOID glUnmapConnsysAddr(P_GLUE_INFO_T prGlueInfo, PUINT_8 remapAddr, UINT_32 consysAddr)
{
	UINT_32 u4ConfCrValue = 0;
	GL_HIF_INFO_T *hifInfo = &prGlueInfo->rHifInfo;

	iounmap(remapAddr);
	u4ConfCrValue = readl(hifInfo->confRegBaseAddr);
	if ((u4ConfCrValue & 0xFFFF0000) != consysAddr) {
		DBGLOG(RX, ERROR,
			"remap configure CR is changed during we are using! new value is %u\n",
			u4ConfCrValue);
		return;
	}
	u4ConfCrValue &= 0xFFFF;
	u4ConfCrValue |= DYNAMIC_REMAP_BASE;
	writel(u4ConfCrValue, hifInfo->confRegBaseAddr);
}
/* End of ahb.c */
