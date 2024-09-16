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

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#ifndef CONFIG_X86
#include <asm/memory.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of.h>
#endif

#include "gl_os.h"
#include "mt6630_reg.h"
#include "sdio.h"
#include "gl_rst.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* #define MTK_DMA_BUF_MEMCPY_SUP no virt_to_phys() use */

#define NIC_TX_PAGE_SIZE                        128	/* in unit of bytes */

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

static int HifAhbProbe(VOID);

static int HifAhbRemove(VOID);

static int HifAhbBusCntGet(VOID);

static int HifAhbBusCntClr(VOID);

static int HifAhbIsWifiDrvOwn(VOID);

static int HifTxCnt;

#if (CONF_HIF_DEV_MISC == 1)
static ssize_t HifAhbMiscRead(IN struct file *Filp, OUT char __user *DstBuf, IN size_t Size, IN loff_t *Ppos);

static ssize_t HifAhbMiscWrite(IN struct file *Filp, IN const char __user *SrcBuf, IN size_t Size, IN loff_t *Ppos);

static int HifAhbMiscIoctl(IN struct file *Filp, IN unsigned int Cmd, IN unsigned long arg);

static int HifAhbMiscOpen(IN struct inode *Inodep, IN struct file *Filp);

static int HifAhbMiscClose(IN struct inode *Inodep, IN struct file *Filp);
#else

static int HifAhbPltmProbe(IN struct platform_device *pDev);

static int HifAhbPltmRemove(IN struct platform_device *pDev);

#ifdef CONFIG_PM
static int HifAhbPltmSuspend(IN struct platform_device *pDev, pm_message_t message);

static int HifAhbPltmResume(IN struct platform_device *pDev);
#endif /* CONFIG_PM */

#endif /* CONF_HIF_DEV_MISC */

#if (CONF_HIF_LOOPBACK_AUTO == 1)	/* only for development test */
static VOID HifAhbLoopbkAuto(IN unsigned long arg);
#endif /* CONF_HIF_LOOPBACK_AUTO */

static irqreturn_t HifAhbISR(IN int irq, IN void *arg);

#if (CONF_HIF_DMA_INT == 1)
static irqreturn_t HifDmaISR(IN int irq, IN void *arg);
#endif /* CONF_HIF_DMA_INT */

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
DEFINE_SPINLOCK(HifLock);
DEFINE_SPINLOCK(HifSdioLock);

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/* initialiation function from other module */
static probe_card pfWlanProbe;

/* release function from other module */
static remove_card pfWlanRemove;

static BOOLEAN WlanDmaFatalErr;

int sdioDisableRefCnt;

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
	{.compatible = "mediatek,wifi",},
	{}
};
#endif

struct platform_driver MtkAhbPltmDriver = {
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
	WLAN_STATUS ret;

	ASSERT(pfProbe);
	ASSERT(pfRemove);

	pfWlanProbe = pfProbe;	/* wlan card initialization in other modules = wlanProbe() */
	pfWlanRemove = pfRemove;

#if (CONF_HIF_DEV_MISC == 1)
	ret = misc_register(&MtkAhbDriver);
	if (ret != 0)
		return ret;
	HifAhbProbe();
#else
	ret = platform_driver_register(&MtkAhbPltmDriver);
#endif /* CONF_HIF_DEV_MISC */

	return ret;
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
	platform_driver_unregister(&MtkAhbPltmDriver);
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
UINT_8 **g_pHifRegBaseAddr;

VOID glSetHifInfo(GLUE_INFO_T *GlueInfo, ULONG ulCookie)
{
	GL_HIF_INFO_T *HifInfo;
	UINT_32	val = HIF_DRV_BASE, val_h = 0;

	/* Init WIFI HIF */
	ASSERT(GlueInfo);
	HifInfo = &GlueInfo->rHifInfo;
#if (CONF_HIF_DEV_MISC == 1)
	HifInfo->Dev = MtkAhbDriver.this_device;
#else
	HifInfo->Dev = &HifAhbPDev->dev;
#endif /* CONF_HIF_DEV_MISC */
	SET_NETDEV_DEV(GlueInfo->prDevHandler, HifInfo->Dev);

#ifdef CONFIG_OF
	HifInfo->HifRegBaseAddr = (PUINT_8)of_iomap(HifInfo->Dev->of_node, 0);
	if (of_property_read_u32_index(HifInfo->Dev->of_node, "reg", 0, &val_h) ||
	    of_property_read_u32_index(HifInfo->Dev->of_node, "reg", 1, &val)) {
		DBGLOG(INIT, ERROR, "Failed to get WIFI-HIF base addr from DT!! Tx/Rx maybe abnormal!!\n");
	}
#if __BITS_PER_LONG == 32
	HifInfo->HifRegPhyBase = (ULONG)val;
#else
	HifInfo->HifRegPhyBase = (((ULONG)val_h << 16) << 16) | (ULONG)val;
#endif
	HifInfo->InfraRegBaseAddr = (PUINT_8)of_iomap(HifInfo->Dev->of_node, 2);
	HifInfo->ConnCfgRegBaseAddr = (PUINT_8)of_iomap(HifInfo->Dev->of_node, 3);
#else
	HifInfo->HifRegBaseAddr = ioremap(HIF_DRV_BASE, HIF_DRV_LENGTH);
	HifInfo->HifRegPhyBase = HIF_DRV_BASE;
	HifInfo->InfraRegBaseAddr = ioremap(INFRA_AO_DRV_BASE, INFRA_AO_DRV_LENGTH);
	HifInfo->ConnCfgRegBaseAddr = ioremap(CONN_REMAP_CONF_BASE, CONN_REMAP_CONF_LENGTH);
#endif
#if defined(MT6797)
	HifInfo->confRegBaseAddr = ioremap(DYNAMIC_REMAP_CONF_BASE, DYNAMIC_REMAP_CONF_LENGTH);
#endif
	g_pHifRegBaseAddr = &(HifInfo->HifRegBaseAddr);

	DBGLOG(INIT, INFO, "HifRegBaseAddr = %p, HifRegPhyBase = 0x%lx\n",
	       HifInfo->HifRegBaseAddr, HifInfo->HifRegPhyBase);

	/* default disable DMA */
	HifInfo->fgDmaEnable = FALSE;
	HifInfo->DmaRegBaseAddr = 0;
	HifInfo->DmaOps = NULL;

	sdio_open();

	/* Init DMA */
	WlanDmaFatalErr = 0;	/* reset error flag */
	sdioDisableRefCnt = 0;
#if (CONF_MTK_AHB_DMA == 1)
	HifDmaInit(HifInfo);
#endif /* CONF_MTK_AHB_DMA */

	/* Start loopback test after 10 seconds */
#if (CONF_HIF_LOOPBACK_AUTO == 1)	/* only for development test */
	{
		init_timer(&(HifInfo->HifTmrLoopbkFn));
		HifInfo->HifTmrLoopbkFn.function = HifAhbLoopbkAuto;
		HifInfo->HifTmrLoopbkFn.data = (unsigned long)GlueInfo;

		init_waitqueue_head(&HifInfo->HifWaitq);
		/* TODO: implement kalDevLoopbkThread to enable loopback function */
		/*HifInfo->HifTaskLoopbkFn = kthread_run(kalDevLoopbkThread, GlueInfo->prDevHandler, "LoopbkThread");*/
		HifInfo->HifLoopbkFlg = 0;

		/* Note: in FPGA, clock is not accuracy so 3000 here, not 10000 */
		HifInfo->HifTmrLoopbkFn.expires = jiffies + MSEC_TO_SYSTIME(30000);
		add_timer(&(HifInfo->HifTmrLoopbkFn));

		DBGLOG(HAL, INFO, "Start loopback test after 10 seconds (jiffies = %u)...\n", jiffies);
	}
#endif /* CONF_HIF_LOOPBACK_AUTO */

#if (CONF_HIF_DMA_INT == 1)
	init_waitqueue_head(&HifInfo->HifDmaWaitq);
	HifInfo->HifDmaFinishFlag = 0;
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
#if (CONF_MTK_AHB_DMA == 1)
	HifDmaUnInit(&GlueInfo->rHifInfo);
#endif
	iounmap(GlueInfo->rHifInfo.HifRegBaseAddr);
	if (GlueInfo->rHifInfo.InfraRegBaseAddr)
		iounmap(GlueInfo->rHifInfo.InfraRegBaseAddr);
	if (GlueInfo->rHifInfo.ConnCfgRegBaseAddr)
		iounmap(GlueInfo->rHifInfo.ConnCfgRegBaseAddr);
#if defined(MT6797)
	iounmap(GlueInfo->rHifInfo.confRegBaseAddr);
#endif
}				/* end of glClearHifInfo() */

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
INT_32 glBusSetIrq(PVOID pvData, PVOID pfnIsr, PVOID pvCookie)
{
#ifdef CONFIG_OF
	struct device_node *node = NULL;
#endif
	struct net_device *prNetDevice;
	GLUE_INFO_T *prGlueInfo;
	unsigned int irq_id;
	unsigned int irq_flags = 0;
#if (CONF_HIF_DMA_INT == 1)
	unsigned int dma_irq_id;
#endif

	if (!pvData) {
		DBGLOG(INIT, ERROR, "Invalid pvData!\n");
		return -1;
	}
	prNetDevice = (struct net_device *)pvData;

	prGlueInfo = (GLUE_INFO_T *) pvCookie;
	if (!prGlueInfo || !prGlueInfo->rHifInfo.Dev) {
		DBGLOG(INIT, ERROR, "No glue info!\n");
		return -1;
	}

#ifdef CONFIG_OF
	node = prGlueInfo->rHifInfo.Dev->of_node;
	irq_id = irq_of_parse_and_map(node, 0);
	DBGLOG(INIT, TRACE, "WIFI-HIF irq %d\n", irq_id);

	/* Get the interrupt flags and then used it for request_irq, but this can be skiped
	 * and just set IRQF_TRIGGER_NONE in request_irq since device tree has already set it.
	 */
	if (of_property_read_u32_index(node, "interrupts", 2, &irq_flags))
		DBGLOG(INIT, ERROR, "Failed to get WIFI-HIF irq flags from DT!\n");
	else
		DBGLOG(INIT, TRACE, "WIFI-HIF irq flags 0x%x\n", irq_flags);

#else
	irq_id = MT_WF_HIF_IRQ_ID;
	irq_flags = IRQF_TRIGGER_LOW;
#endif

	/* Register HIF IRQ */
	if (request_irq(irq_id, HifAhbISR, irq_flags, HIF_MOD_NAME, prNetDevice)) {
		DBGLOG(INIT, ERROR, "Failed to request irq %d!\n", irq_id);
		return -1;
	}

	prGlueInfo->rHifInfo.HifIRQ = irq_id;

#if (CONF_HIF_DMA_INT == 1)
#ifdef CONFIG_OF
	dma_irq_id = irq_of_parse_and_map(node, 1);
	DBGLOG(INIT, INFO, "WIFI-HIF DMA irq %d\n", dma_irq_id);

	if (of_property_read_u32_index(node, "interrupts", 5, &irq_flags))
		DBGLOG(INIT, ERROR, "Failed to get WIFI-HIF DMA irq flags from DT!\n");
	else
		DBGLOG(INIT, INFO, "WIFI-HIF DMA irq flags 0x%x\n", irq_flags);

#else
	dma_irq_id = MT_WF_HIF_DMA_IRQ_ID;
	irq_flags = IRQF_TRIGGER_LOW;
#endif

	/* Register HIF DMA IRQ */
	if (request_irq(dma_irq_id, HifDmaISR, irq_flags, "AHB_DMA", prNetDevice)) {
		DBGLOG(INIT, ERROR, "Failed to request irq %d!\n", dma_irq_id);
		free_irq(irq_id, prNetDevice);
		return -1;
	}

	prGlueInfo->rHifInfo.HifDmaIRQ = dma_irq_id;
#endif /* CONF_HIF_DMA_INT */

	return 0;
}

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
	GLUE_INFO_T *prGlueInfo;

	if (!pvData) {
		DBGLOG(INIT, ERROR, "Invalid pvData!\n");
		return;
	}
	prNetDevice = (struct net_device *)pvData;

	prGlueInfo = (GLUE_INFO_T *) pvCookie;
	if (!prGlueInfo) {
		DBGLOG(INIT, ERROR, "No glue info!\n");
		return;
	}

	/* Free the IRQ */
	free_irq(prGlueInfo->rHifInfo.HifIRQ, prNetDevice);

#if (CONF_HIF_DMA_INT == 1)
	free_irq(prGlueInfo->rHifInfo.HifDmaIRQ, prNetDevice);
#endif /* CONF_HIF_DMA_INT */

}

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

	ASSERT(GlueInfo);
	ASSERT(pu4Value);
	HifInfo = &GlueInfo->rHifInfo;

	/* PIO mode to read HIF controller driver domain register */
	*pu4Value = HIF_REG_READL(HifInfo, RegOffset);

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

	ASSERT(GlueInfo);
	HifInfo = &GlueInfo->rHifInfo;

	/* PIO mode to write HIF controller driver domain register */
	HIF_REG_WRITEL(HifInfo, RegOffset, RegValue);

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
#endif /* CONF_MTK_AHB_DMA */
	UINT_32 idx, u4DwNum;
	UINT_32 *p;
	BOOLEAN ret = TRUE;

	sdio_gen3_cmd53_info info;
	struct sdio_func *func = &g_sdio_func;
	UINT_32 count;

	/* sanity check */
	if (WlanDmaFatalErr) {
		DBGLOG(HAL, ERROR, "WlanDmaFatalErr: %d\n", WlanDmaFatalErr);
		return FALSE;
	}

	ASSERT(GlueInfo);
	HifInfo = &GlueInfo->rHifInfo;

	ASSERT(Buf);
	ASSERT(Size <= MaxBufSize);

	/**********       SDIO like operation       **********/
	count = ALIGN_4(Size);

	/* 1. Setup command information */
	info.word = 0;
	info.field.rw_flag = SDIO_GEN3_READ;
	info.field.func_num = func->num;

	/* CMD53 port mode to read n-byte, if count >= block size => block mode, otherwise => byte mode */
	if (count >= func->cur_blksize) {  /* block mode */
		info.field.block_mode = SDIO_GEN3_BLOCK_MODE;
		info.field.count = count/func->cur_blksize;
		if (count % func->cur_blksize > 0)
			info.field.count++;
		count = info.field.count * func->cur_blksize;
		if (count > MaxBufSize) {
			DBGLOG(HAL, ERROR, "blk mode count(%d->%d), MaxSz(%d)\n", Size, count, MaxBufSize);
			ASSERT(0);
		}
	} else { /* byte mode */
		if (func->use_dma && (Port != MCR_WHISR)) /* safe for reading 4 bytes WHISR */
			count = ((Size + 7) & ~7u); /* if DMA mode, RX 8 bytes alignment is required */
		info.field.block_mode = SDIO_GEN3_BYTE_MODE;
		info.field.count = count;
		if (count > MaxBufSize) {
			DBGLOG(HAL, ERROR, "byte mode count(%d->%d), MaxSz(%d)\n", Size, count, MaxBufSize);
			ASSERT(0);
		}
	}

	info.field.op_mode = SDIO_GEN3_FIXED_PORT_MODE; /* fix mode */
	info.field.addr = Port;

	DBGLOG(HAL, TRACE, "readsb use_dma(%d), count(%d->%d), blk_size(%d), port(0x%x), CMD_SETUP(0x%08x)\n",
	       func->use_dma, Size, count, func->cur_blksize, Port, info.word);

#if (CONF_MTK_AHB_DMA == 1)
	if (func->use_dma && (HifInfo->fgDmaEnable == TRUE) && (HifInfo->DmaOps != NULL)
		&& ((Port == MCR_WRDR0) || (Port == MCR_WRDR1))) {
		/* move forward since clk_prepare_enable can only be called in non-atomic context */
		HifInfo->DmaOps->DmaClockCtrl(HifInfo, TRUE);
	}
#endif

	my_sdio_disable(HifLock);
	__disable_irq();

	writel(info.word, (volatile UINT_32 *)(*g_pHifRegBaseAddr + SDIO_GEN3_CMD_SETUP));
	wmb();

	/* 2. Read CMD53 port */
#if (CONF_MTK_AHB_DMA == 1)
	if (func->use_dma && (HifInfo->fgDmaEnable == TRUE) && (HifInfo->DmaOps != NULL)
		&& ((Port == MCR_WRDR0) || (Port == MCR_WRDR1))) {
		/* only for data port */
#ifdef MTK_DMA_BUF_MEMCPY_SUP
		VOID *DmaVBuf = NULL, *DmaPBuf = NULL;
#endif /* MTK_DMA_BUF_MEMCPY_SUP */

		/* 2.1 config DMA for data transmission */
		DmaConf.Count = count;
		DmaConf.Dir = HIF_DMA_DIR_RX;
		DmaConf.Src = HifInfo->HifRegPhyBase  + SDIO_GEN3_CMD53_DATA;	/* must be physical addr */

#ifdef MTK_DMA_BUF_MEMCPY_SUP
		DmaConf.Dst = kalIOPhyAddrGet(Buf);	/* must be physical addr */

		/* TODO: use virt_to_phys() */
		if (DmaConf.Dst == NULL) {
			ASSERT(count <= CFG_RX_MAX_PKT_SIZE);

			kalDmaBufGet(&DmaVBuf, &DmaPBuf);
			DmaConf.Dst = (ULONG) DmaPBuf;
		}
#else
		/*
		 * http://kernelnewbies.org/KernelMemoryAllocation
		 * Since the cache-coherent mapping may be expensive, also a streaming allocation exists.
		 *
		 * This is a buffer for one-way communication, which means coherency is limited to
		 * flushing the data from the cache after a write finishes. The buffer has to be
		 * pre-allocated (e.g. using kmalloc()). DMA for it is set up with dma_map_single().
		 *
		 * When the DMA is finished (e.g. when the device has sent an interrupt signaling end of
		 * DMA), call dma_unmap_single(). Between map and unmap, the device is in control of the
		 * buffer: if you write to the device, do it before dma_map_single(), if you read from
		 * it, do it after dma_unmap_single().
		 */
		/* DMA_FROM_DEVICE invalidated (without writeback) the cache */
		/* TODO: if dst_off was not cacheline aligned? */

		DmaConf.Dst = dma_map_single(HifInfo->Dev, Buf, count, DMA_FROM_DEVICE);

#endif /* MTK_DMA_BUF_MEMCPY_SUP */

		HifInfo->DmaOps->DmaConfig(HifInfo, &DmaConf);
		/* 2.2 start DMA */
		HifInfo->DmaOps->DmaStart(HifInfo);

		/* 2.3 wait for DMA finish */
#if (CONF_HIF_DMA_INT == 1)
		if (wait_event_interruptible_timeout(HifInfo->HifDmaWaitq, HifInfo->HifDmaFinishFlag != 0, 1000) <= 0) {
			if (HifInfo->DmaOps->DmaRegDump != NULL)
				HifInfo->DmaOps->DmaRegDump(HifInfo);
			DBGLOG(HAL, ERROR, "fatal error! reset DMA!\n");
			if (HifInfo->DmaOps->DmaReset != NULL)
				HifInfo->DmaOps->DmaReset(HifInfo);
			ret = FALSE;
			goto DMA_DONE;
		}
		HifInfo->HifDmaFinishFlag = 0;
#else
		{
		ULONG PollTimeout = jiffies + HZ * 5;
		do {
			if (time_before(jiffies, PollTimeout)) {
				/* Do nothing */
				/* not timeout, continue to poll */
			} else {
				if (HifInfo->DmaOps->DmaRegDump != NULL)
					HifInfo->DmaOps->DmaRegDump(HifInfo);

#if (CONF_HIF_CONNSYS_DBG == 0)
				DBGLOG(HAL, ERROR, "fatal error! reset DMA!\n");
				if (HifInfo->DmaOps->DmaReset != NULL)
					HifInfo->DmaOps->DmaReset(HifInfo);
				ret = FALSE;
				goto DMA_DONE;
#else
				/*
				 * Never break and just wait for response from HIF
				 *
				 * Because when we use ICE on CONNSYS, we will break CONSYS CPU and do debug,
				 * but maybe AP side continues to send packets to HIF, to prevent HIF buffer
				 * from being full and CONNSYS reset, never break here, just stuck in loop.
				 */
				DBGLOG(HAL, WARN, "DMA timeout 5s... (%lu %lu)\n", jiffies, PollTimeout);
				WlanDmaFatalErr = 1;

#endif /* CONF_HIF_CONNSYS_DBG */
			}
		} while (!HifInfo->DmaOps->DmaPollIntr(HifInfo));
		}
#endif /* CONF_HIF_DMA_INT */

		/* 2.4 ack DMA interrupt */
		HifInfo->DmaOps->DmaAckIntr(HifInfo);
		HifInfo->DmaOps->DmaStop(HifInfo);

#if (CONF_HIF_DMA_INT == 1)
		enable_irq(HifInfo->HifDmaIRQ);
#endif

DMA_DONE:
#ifdef MTK_DMA_BUF_MEMCPY_SUP
		if (DmaVBuf != NULL)
			kalMemCopy(Buf, DmaVBuf, count);
#else
		dma_unmap_single(HifInfo->Dev, DmaConf.Dst, count, DMA_FROM_DEVICE);
#endif /* MTK_DMA_BUF_MEMCPY_SUP */

		__enable_irq();
		my_sdio_enable(HifLock);

		/* move behind since clk_disable_unprepare can only be called in non-atomic context */
		HifInfo->DmaOps->DmaClockCtrl(HifInfo, FALSE);

		DBGLOG(HAL, TRACE, "DMA RX %s!\n", ret ? "OK" : "FAIL");
	} else
#endif /* CONF_MTK_AHB_DMA */
	{
		/* PIO mode */
		u4DwNum = count >> 2;
		p = (UINT_32 *) Buf;

		for (idx = 0; idx < u4DwNum; idx++) {
			*p = readl((volatile UINT_32 *)(*g_pHifRegBaseAddr + SDIO_GEN3_CMD53_DATA));
			DBGLOG(HAL, LOUD, "idx = %d, val = 0x%08x\n", idx, *p);
			p++;
		}

		__enable_irq();
		my_sdio_enable(HifLock);

		DBGLOG(HAL, TRACE, "PIO RX OK!\n");
	}

	return ret;
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
* \retval FALSE			operation fail
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
kalDevPortWrite(IN P_GLUE_INFO_T GlueInfo, IN UINT_16 Port, IN UINT_32 Size, IN PUINT_8 Buf, IN UINT_32 MaxBufSize)
{
	GL_HIF_INFO_T *HifInfo;
#if (CONF_MTK_AHB_DMA == 1)
	MTK_WCN_HIF_DMA_CONF DmaConf;
#endif /* CONF_MTK_AHB_DMA */
	UINT_32 idx, u4DwNum;
	UINT_32 *p;
	BOOLEAN ret = TRUE;

	sdio_gen3_cmd53_info info;
	struct sdio_func *func = &g_sdio_func;
	UINT_32 count;

	/* sanity check */
	if (WlanDmaFatalErr) {
		DBGLOG(HAL, ERROR, "WlanDmaFatalErr: %d\n", WlanDmaFatalErr);
		return FALSE;
	}

	ASSERT(GlueInfo);
	HifInfo = &GlueInfo->rHifInfo;

	ASSERT(Buf);
	ASSERT(Size <= MaxBufSize);

	HifTxCnt++;

	/**********       SDIO like operation       **********/
	count = ALIGN_4(Size);

	/* 1. Setup command information */
	info.word = 0;
	info.field.rw_flag = SDIO_GEN3_WRITE;
	info.field.func_num = func->num;

	/* CMD53 port mode to write n-byte, if count >= block size => block mode, otherwise => byte mode */
	if (count >= func->cur_blksize) { /* block mode */
		info.field.block_mode = SDIO_GEN3_BLOCK_MODE;
		info.field.count = count/func->cur_blksize;
		if (count % func->cur_blksize > 0)
			info.field.count++;
		count = info.field.count * func->cur_blksize;
		if (count > MaxBufSize) {
			DBGLOG(HAL, ERROR, "blk mode count(%d->%d), MaxSz(%d)\n", Size, count, MaxBufSize);
			ASSERT(0);
		}
	} else { /* byte mode */
		info.field.block_mode = SDIO_GEN3_BYTE_MODE;
		info.field.count = count;
	}

	info.field.op_mode = SDIO_GEN3_FIXED_PORT_MODE; /* fix mode */
	info.field.addr = Port;

	DBGLOG(HAL, TRACE, "writesb use_dma(%d), count(%d->%d), blk_size(%d), port(0x%x), CMD_SETUP(0x%08x)\n",
	       func->use_dma, Size, count, func->cur_blksize, Port, info.word);

#if (CONF_MTK_AHB_DMA == 1)
	if (func->use_dma && (HifInfo->fgDmaEnable == TRUE) && (HifInfo->DmaOps != NULL) &&
		(Port == MCR_WTDR1)) {
		/* move forward since clk_prepare_enable can only be called in non-atomic context */
		HifInfo->DmaOps->DmaClockCtrl(HifInfo, TRUE);
	}
#endif

	my_sdio_disable(HifLock);
	__disable_irq();

	writel(info.word, (volatile UINT_32 *)(*g_pHifRegBaseAddr + SDIO_GEN3_CMD_SETUP));
	wmb();

	/* 2. Write CMD53 port */
#if (CONF_MTK_AHB_DMA == 1)
	if (func->use_dma && (HifInfo->fgDmaEnable == TRUE) && (HifInfo->DmaOps != NULL) &&
		(Port == MCR_WTDR1)) {
		/* only for data port */
#ifdef MTK_DMA_BUF_MEMCPY_SUP
		VOID *DmaVBuf = NULL, *DmaPBuf = NULL;
#endif /* MTK_DMA_BUF_MEMCPY_SUP */

		/* 2.1 config DMA for data transmission */
		DmaConf.Count = count;
		DmaConf.Dir = HIF_DMA_DIR_TX;
		DmaConf.Dst = HifInfo->HifRegPhyBase  + SDIO_GEN3_CMD53_DATA;	/* must be physical addr */

#ifdef MTK_DMA_BUF_MEMCPY_SUP
		DmaConf.Src = kalIOPhyAddrGet(Buf);	/* must be physical addr */

		/* TODO: use virt_to_phys() */
		if (DmaConf.Src == NULL) {
			ASSERT(count <= CFG_RX_MAX_PKT_SIZE);

			kalDmaBufGet(&DmaVBuf, &DmaPBuf);
			DmaConf.Src = (ULONG) DmaPBuf;

			kalMemCopy(DmaVBuf, Buf, count);
		}
#else
		/* DMA_TO_DEVICE writeback the cache */
		DmaConf.Src = dma_map_single(HifInfo->Dev, Buf, count, DMA_TO_DEVICE);

#endif /* MTK_DMA_BUF_MEMCPY_SUP */

		HifInfo->DmaOps->DmaConfig(HifInfo, &DmaConf);
		/* 2.2 start DMA */
		HifInfo->DmaOps->DmaStart(HifInfo);

		/* 2.3 wait for DMA finish */
#if (CONF_HIF_DMA_INT == 1)
		if (wait_event_interruptible_timeout(HifInfo->HifDmaWaitq, HifInfo->HifDmaFinishFlag != 0, 1000) <= 0) {
			if (HifInfo->DmaOps->DmaRegDump != NULL)
				HifInfo->DmaOps->DmaRegDump(HifInfo);
			DBGLOG(HAL, ERROR, "fatal error! reset DMA!\n");
			if (HifInfo->DmaOps->DmaReset != NULL)
				HifInfo->DmaOps->DmaReset(HifInfo);
			ret = FALSE;
			goto DMA_DONE;
		}
		HifInfo->HifDmaFinishFlag = 0;
#else
		{
		ULONG PollTimeout = jiffies + HZ * 5;

		do {
			if (time_before(jiffies, PollTimeout)) {
				/* Do nothing */
				/* not timeout, continue to poll */
			} else {
				if (HifInfo->DmaOps->DmaRegDump != NULL)
					HifInfo->DmaOps->DmaRegDump(HifInfo);

#if (CONF_HIF_CONNSYS_DBG == 0)
				DBGLOG(HAL, ERROR, "fatal error! reset DMA!\n");
				if (HifInfo->DmaOps->DmaReset != NULL)
					HifInfo->DmaOps->DmaReset(HifInfo);
				ret = FALSE;
				goto DMA_DONE;
#else
				DBGLOG(HAL, WARN, "DMA timeout 5s... (%lu %lu)\n", jiffies, PollTimeout);
				WlanDmaFatalErr = 1;

#endif /* CONF_HIF_CONNSYS_DBG */
			}
		} while (!HifInfo->DmaOps->DmaPollIntr(HifInfo));
		}
#endif /* CONF_HIF_DMA_INT */

		/* 2.4 ack DMA interrupt */
		HifInfo->DmaOps->DmaAckIntr(HifInfo);
		HifInfo->DmaOps->DmaStop(HifInfo);

#if (CONF_HIF_DMA_INT == 1)
		enable_irq(HifInfo->HifDmaIRQ);
#endif

DMA_DONE:
#ifndef MTK_DMA_BUF_MEMCPY_SUP
		dma_unmap_single(HifInfo->Dev, DmaConf.Src, count, DMA_TO_DEVICE);
#endif /* MTK_DMA_BUF_MEMCPY_SUP */

		__enable_irq();
		my_sdio_enable(HifLock);

		/* move behind since clk_disable_unprepare can only be called in non-atomic context */
		HifInfo->DmaOps->DmaClockCtrl(HifInfo, FALSE);

		DBGLOG(HAL, TRACE, "DMA TX %s!\n", ret ? "OK" : "FAIL");
	} else
#endif /* CONF_MTK_AHB_DMA */
	{
		/* PIO mode */
		u4DwNum = count >> 2;
		p = (UINT_32 *) Buf;

		for (idx = 0; idx < u4DwNum; idx++) {
			writel(*p, (volatile UINT_32 *)(*g_pHifRegBaseAddr + SDIO_GEN3_CMD53_DATA));
			DBGLOG(HAL, LOUD, "idx = %d, val = 0x%08x\n", idx, *p);
			p++;
		}

		__enable_irq();
		my_sdio_enable(HifLock);

		DBGLOG(HAL, TRACE, "PIO TX OK!\n");
	}

	return ret;
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
static irqreturn_t HifAhbISR(IN int irq, IN void *arg)
{
	struct net_device *prNetDevice = (struct net_device *)arg;
	GLUE_INFO_T *prGlueInfo;

	ASSERT(prNetDevice);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDevice));
	if (!prGlueInfo)
		return IRQ_HANDLED;

	prGlueInfo->u8HifIntTime = sched_clock();
	prGlueInfo->IsrCnt++;
	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		__disable_irq();
		return IRQ_HANDLED;
	}

	__disable_irq();

	/* lock 100ms to avoid suspend */
	/* MT6797 TODO */
	/* kalHifAhbKalWakeLockTimeout(GlueInfo); */

	set_bit(GLUE_FLAG_INT_BIT, &prGlueInfo->ulFlag);

	/* when we got HIF interrupt, we wake up hif thread */
#if CFG_SUPPORT_MULTITHREAD
	wake_up_interruptible(&prGlueInfo->waitq_hif);
#else
	wake_up_interruptible(&prGlueInfo->waitq);
#endif

	prGlueInfo->IsrPassCnt++;
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

static irqreturn_t HifDmaISR(IN int irq, IN void *arg)
{
	struct net_device *prNetDevice = (struct net_device *)arg;
	GLUE_INFO_T *prGlueInfo;
	GL_HIF_INFO_T *prHifInfo;

	ASSERT(prNetDevice);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDevice));
	if (!prGlueInfo)
		return IRQ_HANDLED;

	prHifInfo = &prGlueInfo->rHifInfo;

	/* disable interrupt */
	disable_irq_nosync(irq);

	set_bit(1, &prHifInfo->HifDmaFinishFlag);

	/* When we got DMA finish interrupt, we wake up hif thread */
	wake_up_interruptible(&prHifInfo->HifDmaWaitq);

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

static int HifAhbProbe(VOID)
{
	int ret = 0;

	DBGLOG(INIT, TRACE, "HifAhbProbe()\n");

	/* power on WiFi TX PA 3.3V and HIF GDMA clock */
	mtk_wcn_consys_hw_wifi_paldo_ctrl(1);	/* switch to HW mode */

#if (CONF_HIF_DEV_MISC == 1)
	if (pfWlanProbe((PVOID) &MtkAhbDriver.this_device) != WLAN_STATUS_SUCCESS) {
#else
	if (pfWlanProbe((PVOID) &HifAhbPDev->dev) != WLAN_STATUS_SUCCESS) {
#endif /* CONF_HIF_DEV_MISC */

		pfWlanRemove();
		ret = -1;
	}

	return ret;
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

	mtk_wcn_consys_hw_wifi_paldo_ctrl(0);	/* switch to SW mode */

	return 0;
}

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

static int hifAhbSetMpuProtect(BOOLEAN enable)
{
	kalSetEmiMpuProtection(gConEmiPhyBase, WIFI_EMI_MEM_SIZE, enable);
	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function check the status of wifi driver
*
* \param[in] None
*
* \return 1: drv_own, 0: fw_own
*/
/*----------------------------------------------------------------------------*/
static int HifAhbIsWifiDrvOwn(VOID)
{
	return (wlanIsFwOwn() == FALSE) ? 1 : 0;
}

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
static int HifAhbPltmProbe(IN struct platform_device *pDev)
{
	struct MTK_WCN_WMT_WLAN_CB_INFO rWmtCb;

	DBGLOG(INIT, INFO, "HifAhbPltmProbe\n");

	HifAhbPDev = pDev;

#if (CONF_HIF_PMIC_TEST == 1)
	wmt_set_jtag_for_mcu();
	wmt_set_jtag_for_gps();
#endif /* CONF_HIF_PMIC_TEST */
	kalMemZero(&rWmtCb, sizeof(struct MTK_WCN_WMT_WLAN_CB_INFO));
	/* Register WIFI probe/remove functions to WMT */
	rWmtCb.wlan_probe_cb = HifAhbProbe;
	rWmtCb.wlan_remove_cb = HifAhbRemove;
	rWmtCb.wlan_bus_cnt_get_cb = HifAhbBusCntGet;
	rWmtCb.wlan_bus_cnt_clr_cb = HifAhbBusCntClr;
	rWmtCb.wlan_emi_mpu_set_protection_cb = hifAhbSetMpuProtect;
	rWmtCb.wlan_is_wifi_drv_own_cb = HifAhbIsWifiDrvOwn;
	mtk_wcn_wmt_wlan_reg(&rWmtCb);

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
static int HifAhbPltmRemove(IN struct platform_device *pDev)
{
	mtk_wcn_wmt_wlan_unreg();
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
static int HifAhbPltmSuspend(IN struct platform_device *pDev, pm_message_t message)
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
static int HifAhbPltmResume(IN struct platform_device *pDev)
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

	DBGLOG(HAL, INFO, "Trigger to do loopback test...\n");

	set_bit(GLUE_FLAG_HIF_LOOPBK_AUTO_BIT, &HifInfo->HifLoopbkFlg);
	wake_up_interruptible(&HifInfo->HifWaitq);
}
#endif /* CONF_HIF_LOOPBACK_AUTO */
/*----------------------------------------------------------------------------*/
/*!
* \brief This function is used to dump hif ahb information for DE to debug.
*
* \param prGlueInfo      Pointer of GLUE_INFO_T Data Structure
* \param u2ChipID        Chip id
*
* return (none)
*/
/*----------------------------------------------------------------------------*/
void kalDumpAhbDebugInfo(P_GLUE_INFO_T prGlueInfo, UINT_16 u2ChipID)
{
	/* Only call this in hif_thread.
	 * Multi-thread operate HIF CR may cause HIF abnormal.
	 */

	UINT_32 val;
	UINT_32 u4InfraPseOffset = 0x0394;

	DBGLOG(HAL, ERROR, "HIF_DBGCR00:0x%08X HIF_DBGCR01:0x%08X\n",
		CONNSYS_REG_READ(prGlueInfo->rHifInfo.HifRegBaseAddr,
			CONN_HIF_DBGCR00),
		CONNSYS_REG_READ(prGlueInfo->rHifInfo.HifRegBaseAddr,
			CONN_HIF_DBGCR01));
	DBGLOG(HAL, ERROR, "HIF_DBGCR02:0x%08X HIF_DBGCR04:0x%08X\n",
		CONNSYS_REG_READ(prGlueInfo->rHifInfo.HifRegBaseAddr,
			CONN_HIF_DBGCR02),
		CONNSYS_REG_READ(prGlueInfo->rHifInfo.HifRegBaseAddr,
			CONN_HIF_DBGCR04));
	DBGLOG(HAL, ERROR, "HIF_DBGCR08:0x%08X HIF_DBGCR10:0x%08X\n",
		CONNSYS_REG_READ(prGlueInfo->rHifInfo.HifRegBaseAddr,
			CONN_HIF_DBGCR08),
		CONNSYS_REG_READ(prGlueInfo->rHifInfo.HifRegBaseAddr,
			CONN_HIF_DBGCR10));
	DBGLOG(HAL, ERROR, "HIF_DBGCR11:0x%08X HIF_DBGCR12:0x%08X\n",
		CONNSYS_REG_READ(prGlueInfo->rHifInfo.HifRegBaseAddr,
			CONN_HIF_DBGCR11),
		CONNSYS_REG_READ(prGlueInfo->rHifInfo.HifRegBaseAddr,
			CONN_HIF_DBGCR12));

	/* SET INFRA AO REMAPPING PSE Client REG according to Chip ID */
	switch (u2ChipID) {
	case 0x6797:
		u4InfraPseOffset = 0x0340;
		break;
	case 0x6759:
	case 0x6758:
	case 0x6775:
		u4InfraPseOffset = 0x0384;
		break;
	case 0x6771:
		u4InfraPseOffset = 0x0394;
		break;
	default:
		DBGLOG(HAL, WARN, "Using default offset 0x%04x for chip id 0x%04x\n",
		       u4InfraPseOffset, u2ChipID);
		break;
	}
	if (u2ChipID == 0x6771) {
		val = 0x800c;
		CONNSYS_REG_WRITE(prGlueInfo->rHifInfo.InfraRegBaseAddr,
				 u4InfraPseOffset, val);
	} else {
		val = 0x800c << 16;
		CONNSYS_REG_WRITE(prGlueInfo->rHifInfo.InfraRegBaseAddr, u4InfraPseOffset,
				  CONNSYS_REG_READ(prGlueInfo->rHifInfo.InfraRegBaseAddr,
						   u4InfraPseOffset) | val);
	}
	val = 0x6;
	CONNSYS_REG_WRITE(prGlueInfo->rHifInfo.ConnCfgRegBaseAddr,
			  CONN_REMAP_PSE_CLIENT_DBGCR, val);
	DBGLOG(HAL, ERROR,
	       "PSE Client debug CR: 0x%08x\n",
	       CONNSYS_REG_READ(prGlueInfo->rHifInfo.ConnCfgRegBaseAddr,
				CONN_REMAP_PSE_CLIENT_DBGCR));

	val = 0x3;
	CONNSYS_REG_WRITE(prGlueInfo->rHifInfo.ConnCfgRegBaseAddr,
			  CONN_REMAP_PSE_CLIENT_DBGCR, val);
	DBGLOG(HAL, ERROR,
	       "PSE Client debug CR: 0x%08x\n",
	       CONNSYS_REG_READ(prGlueInfo->rHifInfo.ConnCfgRegBaseAddr,
				CONN_REMAP_PSE_CLIENT_DBGCR));

	val = 0x4;
	CONNSYS_REG_WRITE(prGlueInfo->rHifInfo.ConnCfgRegBaseAddr,
			  CONN_REMAP_PSE_CLIENT_DBGCR, val);
	DBGLOG(HAL, ERROR,
	       "PSE Client debug CR: 0x%08x\n",
	       CONNSYS_REG_READ(prGlueInfo->rHifInfo.ConnCfgRegBaseAddr,
				CONN_REMAP_PSE_CLIENT_DBGCR));

	val = 0x12;
	CONNSYS_REG_WRITE(prGlueInfo->rHifInfo.ConnCfgRegBaseAddr,
			  CONN_REMAP_PSE_CLIENT_DBGCR, val);
	DBGLOG(HAL, ERROR,
	       "PSE Client debug CR: 0x%08x\n",
	       CONNSYS_REG_READ(prGlueInfo->rHifInfo.ConnCfgRegBaseAddr,
				CONN_REMAP_PSE_CLIENT_DBGCR));

	val = 0x17;
	CONNSYS_REG_WRITE(prGlueInfo->rHifInfo.ConnCfgRegBaseAddr,
			  CONN_REMAP_PSE_CLIENT_DBGCR, val);
	DBGLOG(HAL, ERROR,
	       "PSE Client debug CR: 0x%08x\n",
	       CONNSYS_REG_READ(prGlueInfo->rHifInfo.ConnCfgRegBaseAddr,
				CONN_REMAP_PSE_CLIENT_DBGCR));

}

#if defined(MT6797)
PUINT_8 glRemapConnsysAddr(P_GLUE_INFO_T prGlueInfo, UINT_32 consysAddr, UINT_32 remapLength)
{
	/* 0x180E0000 is the customized address and can be remaped to any connsys address */
	PUINT_8 pucRemapCrAddr = NULL;
	GL_HIF_INFO_T *hifInfo = &prGlueInfo->rHifInfo;
	UINT_32 u4ConfCrValue = 0;

	u4ConfCrValue = readl(hifInfo->confRegBaseAddr);
	if ((u4ConfCrValue & 0xFFFF0000) != 0x180E0000) {
		DBGLOG(HAL, ERROR, "remap CR is used by others, value is %u\n", u4ConfCrValue);
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
		DBGLOG(HAL, ERROR,
		       "remap configure CR is changed during we are using! new value is %u\n",
		       u4ConfCrValue);
		return;
	}
	u4ConfCrValue &= 0xFFFF;
	u4ConfCrValue |= DYNAMIC_REMAP_BASE;
	writel(u4ConfCrValue, hifInfo->confRegBaseAddr);
}
#endif
