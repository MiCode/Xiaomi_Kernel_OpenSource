/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_Kernelc
 *
 * Project:
 * --------
 *   MT6583  Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/
#include "AudDrv_BTCVSD.h"
#include "AudDrv_BTCVSD_ioctl.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/mutex.h>
/* #include <linux/xlog.h> */
#ifndef _IRQS_H_NOT_SUPPORT
#include <mach/irqs.h>
/* #include <mach/mt_irq.h> */
#include <mach/irqs.h>
#endif
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
/* #include <mach/mt_reg_base.h> */
#include <asm/div64.h>
/* #include <linux/aee.h> */
#include <mt-plat/aee.h>
#include <linux/dma-mapping.h>
#include <linux/compat.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

/*#define TEST_PACKETLOSS*/

/*****************************************************************************
*           DEFINE AND CONSTANT
******************************************************************************
*/

#define AUDDRV_BTCVSD_NAME   "MediaTek Audio BTCVSD Driver"
#define AUDDRV_AUTHOR "MediaTek WCX"

#define MASK_ALL          (0xFFFFFFFF)

/*****************************************************************************
*           V A R I A B L E     D E L A R A T I O N
*******************************************************************************/

static const char auddrv_btcvsd_name[] = "AudioMTKBTCVSD";

static kal_uint32 writeToBT_cnt;
static kal_uint32 readFromBT_cnt;
static kal_uint32 disableBTirq;

static struct device *mDev;

#ifdef TEST_PACKETLOSS
kal_uint8 uSilencePattern[SCO_RX_PLC_SIZE];
static kal_uint16 packet_cnt;
#endif

/* to mask BT CVSD IRQ when AP-side CVSD disable. Note: 72 is bit1 */
static volatile void *INFRA_MISC_ADDRESS;

static const kal_uint32 btsco_PacketValidMask[BT_SCO_CVSD_MAX][BT_SCO_CVSD_MAX] = {
{0x1, 0x1 << 1, 0x1 << 2, 0x1 << 3, 0x1 << 4, 0x1 << 5},	/*30*/
{0x1, 0x1, 0x2, 0x2, 0x4, 0x4},	/*60*/
{0x1, 0x1, 0x1, 0x2, 0x2, 0x2},	/*90*/
{0x1, 0x1, 0x1, 0x1, 0, 0},	/*120*/
{0x7, 0x7 << 3, 0x7 << 6, 0x7 << 9, 0x7 << 12, 0x7 << 15},	/*10*/
{0x3, 0x3 << 1, 0x3 << 3, 0x3 << 4, 0x3 << 6, 0x3 << 7}
};				/*20*/

static const kal_uint8 btsco_PacketInfo[BT_SCO_CVSD_MAX][BT_SCO_CVSD_MAX] = {
{30, 6, BT_SCO_PACKET_180 / SCO_TX_ENCODE_SIZE, BT_SCO_PACKET_180 / SCO_RX_PLC_SIZE},	/*30*/
{60, 3, BT_SCO_PACKET_180 / SCO_TX_ENCODE_SIZE, BT_SCO_PACKET_180 / SCO_RX_PLC_SIZE},	/*60*/
{90, 2, BT_SCO_PACKET_180 / SCO_TX_ENCODE_SIZE, BT_SCO_PACKET_180 / SCO_RX_PLC_SIZE},	/*90*/
{120, 1, BT_SCO_PACKET_120 / SCO_TX_ENCODE_SIZE, BT_SCO_PACKET_120 / SCO_RX_PLC_SIZE},	/*120*/
{10, 18, BT_SCO_PACKET_180 / SCO_TX_ENCODE_SIZE, BT_SCO_PACKET_180 / SCO_RX_PLC_SIZE},	/*10*/
{20, 9, BT_SCO_PACKET_180 / SCO_TX_ENCODE_SIZE, BT_SCO_PACKET_180 / SCO_RX_PLC_SIZE}
};				/*20*/


static struct {
	BT_SCO_TX_T *pTX;
	BT_SCO_RX_T *pRX;
	kal_uint8 *pStructMemory;
	kal_uint8 *pWorkingMemory;
	kal_uint16 uAudId;
	CVSD_STATE uTXState;
	CVSD_STATE uRXState;
	kal_bool fIsStructMemoryOnMED;
} btsco;

static volatile kal_uint32 *bt_hw_REG_PACKET_W, *bt_hw_REG_PACKET_R, *bt_hw_REG_CONTROL;

static DEFINE_SPINLOCK(auddrv_BTCVSDTX_lock);
static DEFINE_SPINLOCK(auddrv_BTCVSDRX_lock);

static kal_uint32 BTCVSD_write_wait_queue_flag;
static kal_uint32 BTCVSD_read_wait_queue_flag;


DECLARE_WAIT_QUEUE_HEAD(BTCVSD_Write_Wait_Queue);
DECLARE_WAIT_QUEUE_HEAD(BTCVSD_Read_Wait_Queue);


#ifdef CONFIG_OF
static int Auddrv_BTCVSD_Irq_Map(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,audio_bt_cvsd");
	if (node == NULL)
		pr_debug("BTCVSD get node failed\n");

	/*get btcvsd irq num */
	btcvsd_irq_number = irq_of_parse_and_map(node, 0);
	pr_debug("[BTCVSD] btcvsd_irq_number=%d\n", btcvsd_irq_number);
	if (!btcvsd_irq_number) {
		pr_debug("[BTCVSD] get btcvsd_irq_number failed!!!\n");
		return -1;
	}
	return 0;
}

static int Auddrv_BTCVSD_Address_Map(void)
{
	struct device_node *node = NULL;
	void __iomem *base;
	u32 offset[5] = { 0, 0, 0, 0, 0 };

	node = of_find_compatible_node(NULL, NULL, "mediatek,audio_bt_cvsd");
	if (node == NULL)
		pr_debug("BTCVSD get node failed\n");

	/*get INFRA_MISC offset, conn_bt_cvsd_mask, cvsd_mcu_read_offset, write_offset, packet_indicator */
	of_property_read_u32_array(node, "offset", offset, ARRAY_SIZE(offset));
	infra_misc_offset = offset[0];
	conn_bt_cvsd_mask = offset[1];
	cvsd_mcu_read_offset = offset[2];
	cvsd_mcu_write_offset = offset[3];
	cvsd_packet_indicator = offset[4];

	/*get infra base address */
	base = of_iomap(node, 0);
	infra_base = (unsigned long)base;

	/*get btcvsd sram address */
	base = of_iomap(node, 1);
	btsys_pkv_physical_base = (unsigned long)base;
	base = of_iomap(node, 2);
	btsys_sram_bank2_physical_base = (unsigned long)base;

	/*print for debug */
	pr_err("[BTCVSD] Auddrv_BTCVSD_Address_Map:\n");
	pr_err("[BTCVSD] infra_misc_offset=0x%lx\n", infra_misc_offset);
	pr_err("[BTCVSD] conn_bt_cvsd_mask=0x%lx\n", conn_bt_cvsd_mask);
	pr_err("[BTCVSD] read_off=0x%lx\n", cvsd_mcu_read_offset);
	pr_err("[BTCVSD] write_off=0x%lx\n", cvsd_mcu_write_offset);
	pr_err("[BTCVSD] packet_ind=0x%lx\n", cvsd_packet_indicator);
	pr_err("[BTCVSD] infra_base=0x%lx\n", infra_base);
	pr_err("[BTCVSD] btsys_pkv_physical_base=0x%lx\n", btsys_pkv_physical_base);
	pr_err("[BTCVSD] btsys_sram_bank2_physical_base=0x%lx\n", btsys_sram_bank2_physical_base);

	if (!infra_base) {
		pr_err("[BTCVSD] get infra_base failed!!!\n");
		return -1;
	}
	if (!btsys_pkv_physical_base) {
		pr_err("[BTCVSD] get btsys_pkv_physical_base failed!!!\n");
		return -1;
	}
	if (!btsys_sram_bank2_physical_base) {
		pr_err("[BTCVSD] get btsys_sram_bank2_physical_base failed!!!\n");
		return -1;
	}
	return 0;
}

#endif

static void Disable_CVSD_Wakeup(void)
{
	volatile kal_uint32 *INFRA_MISC_REGISTER = (volatile kal_uint32 *)(INFRA_MISC_ADDRESS);
	*INFRA_MISC_REGISTER |= conn_bt_cvsd_mask;
	pr_err("Disable_CVSD_Wakeup\n");
}

static void Enable_CVSD_Wakeup(void)
{
	volatile kal_uint32 *INFRA_MISC_REGISTER = (volatile kal_uint32 *)(INFRA_MISC_ADDRESS);
	*INFRA_MISC_REGISTER &= ~(conn_bt_cvsd_mask);
	pr_err("Enable_CVSD_Wakeup\n");
}

static int AudDrv_btcvsd_Allocate_Buffer(struct file *fp, kal_uint8 isRX)
{
	pr_debug("AudDrv_btcvsd_Allocate_Buffer(+) isRX=%d\n", isRX);

	if (isRX == 1) {
		readFromBT_cnt = 0;
		BT_CVSD_Mem.u4RXBufferSize = sizeof(BT_SCO_RX_T);

		if ((BT_CVSD_Mem.pucRXVirtBufAddr == NULL)
		    && (BT_CVSD_Mem.pucRXPhysBufAddr == 0)) {
			BT_CVSD_Mem.pucRXVirtBufAddr = dma_alloc_coherent(mDev,
									  BT_CVSD_Mem.u4RXBufferSize,
									  &BT_CVSD_Mem.pucRXPhysBufAddr,
									  GFP_KERNEL);
			if ((0 == BT_CVSD_Mem.pucRXPhysBufAddr)
			    || (NULL == BT_CVSD_Mem.pucRXVirtBufAddr)) {
				pr_debug
				    ("AudDrv_btcvsd_Allocate_Buffer dma_alloc_coherent RX fail\n");
				return -1;
			}
		}
		memset((void *)BT_CVSD_Mem.pucRXVirtBufAddr, 0, BT_CVSD_Mem.u4RXBufferSize);

		PRINTK_AUDDRV("BT_CVSD_Mem.pucRXVirtBufAddr = %p BT_CVSD_Mem.pucRXPhysBufAddr = 0x%x\n",
		     BT_CVSD_Mem.pucRXVirtBufAddr, BT_CVSD_Mem.pucRXPhysBufAddr);

		btsco.pRX = (BT_SCO_RX_T *) (BT_CVSD_Mem.pucRXVirtBufAddr);
		btsco.pRX->u4BufferSize = SCO_RX_PACKER_BUF_NUM * (SCO_RX_PLC_SIZE +
								   BTSCO_CVSD_PACKET_VALID_SIZE);
	} else {
		writeToBT_cnt = 0;
		BT_CVSD_Mem.u4TXBufferSize = sizeof(BT_SCO_TX_T);

		if ((BT_CVSD_Mem.pucTXVirtBufAddr == NULL)
		    && (BT_CVSD_Mem.pucTXPhysBufAddr == 0)) {
			BT_CVSD_Mem.pucTXVirtBufAddr = dma_alloc_coherent(mDev,
									  BT_CVSD_Mem.u4TXBufferSize,
									  &BT_CVSD_Mem.pucTXPhysBufAddr,
									  GFP_KERNEL);
			if ((0 == BT_CVSD_Mem.pucTXPhysBufAddr)
			    || (NULL == BT_CVSD_Mem.pucTXVirtBufAddr)) {
				pr_debug
				    ("AudDrv_btcvsd_Allocate_Buffer dma_alloc_coherent TX fail\n");
				return -1;
			}
		}
		memset((void *)BT_CVSD_Mem.pucTXVirtBufAddr, 0, BT_CVSD_Mem.u4TXBufferSize);

		PRINTK_AUDDRV("BT_CVSD_Mem.pucTXVirtBufAddr = 0x%p BT_CVSD_Mem.pucTXPhysBufAddr = 0x%x\n",
		     BT_CVSD_Mem.pucTXVirtBufAddr, BT_CVSD_Mem.pucTXPhysBufAddr);

		btsco.pTX = (BT_SCO_TX_T *) (BT_CVSD_Mem.pucTXVirtBufAddr);
		btsco.pTX->u4BufferSize = SCO_TX_PACKER_BUF_NUM * SCO_TX_ENCODE_SIZE;
	}
	pr_debug("AudDrv_btcvsd_Allocate_Buffer(-)\n");
	return 0;
}

static int AudDrv_btcvsd_Free_Buffer(struct file *fp, kal_uint8 isRX)
{
	pr_debug("AudDrv_btcvsd_Free_Buffer(+) isRX=%d\n", isRX);

	if (isRX == 1) {
		if ((BT_CVSD_Mem.pucRXVirtBufAddr != NULL)
		    && (BT_CVSD_Mem.pucRXPhysBufAddr != 0)) {
			PRINTK_AUDDRV("AudDrv_btcvsd_Free_Buffer dma_free_coherent RXVirtBufAddr=%p,RXPhysBufAddr=%x",
			     BT_CVSD_Mem.pucRXVirtBufAddr, BT_CVSD_Mem.pucRXPhysBufAddr);
			btsco.pRX = NULL;
			dma_free_coherent(0, BT_CVSD_Mem.u4RXBufferSize,
					  BT_CVSD_Mem.pucRXVirtBufAddr,
					  BT_CVSD_Mem.pucRXPhysBufAddr);
			BT_CVSD_Mem.u4RXBufferSize = 0;
			BT_CVSD_Mem.pucRXVirtBufAddr = NULL;
			BT_CVSD_Mem.pucRXPhysBufAddr = 0;

		} else {
			PRINTK_AUDDRV("btcvsd_Free_Buffer can't dma_free_coherent RXVBufAddr=%p,RXPhBufAddr=0x%x\n",
				BT_CVSD_Mem.pucRXVirtBufAddr, BT_CVSD_Mem.pucRXPhysBufAddr);
			return -1;
		}
	} else {
		if ((BT_CVSD_Mem.pucTXVirtBufAddr != NULL) && (BT_CVSD_Mem.pucTXPhysBufAddr != 0)) {
			/*pr_err("btcvsd_Free_Buffer dma_free_coherent pucTXVirtBufAddr = %p,pucTXPhysBufAddr = %x\n",
				BT_CVSD_Mem.pucTXVirtBufAddr, BT_CVSD_Mem.pucTXPhysBufAddr);*/
			btsco.pTX = NULL;
			dma_free_coherent(0, BT_CVSD_Mem.u4TXBufferSize,
					  BT_CVSD_Mem.pucTXVirtBufAddr,
					  BT_CVSD_Mem.pucTXPhysBufAddr);
			BT_CVSD_Mem.u4TXBufferSize = 0;
			BT_CVSD_Mem.pucTXVirtBufAddr = NULL;
			BT_CVSD_Mem.pucTXPhysBufAddr = 0;

		} else {
			PRINTK_AUDDRV("btcvsd_Free_Buffer cannot dma_free_coherent TXVBufAddr=%p,TXPhBufAddr=0x%x\n",
				BT_CVSD_Mem.pucTXVirtBufAddr, BT_CVSD_Mem.pucTXPhysBufAddr);
			return -1;
		}
	}
	pr_debug("AudDrv_btcvsd_Free_Buffer(-)\n");
	return 0;
}


/*****************************************************************************
 * FILE OPERATION FUNCTION
 *  AudDrv_btcvsd_ioctl
 *
 * DESCRIPTION
 *  IOCTL Msg handle
 *
 *****************************************************************************
 */
static long AudDrv_btcvsd_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	pr_debug("AudDrv_btcvsd_ioctl cmd = 0x%x arg = %lu\n", cmd, arg);

	switch (cmd) {

	case ALLOCATE_FREE_BTCVSD_BUF:{
			/* 0: allocate TX bufs*/
			/* 1: free TX buf*/
			/* 2: allocate RX buf*/
			/* 3: free TX buf*/
			if (arg == 0)
				ret = AudDrv_btcvsd_Allocate_Buffer(fp, 0);
			else if (arg == 1)
				pr_debug("%s(), do nothing\n", __func__);
				/*ret = AudDrv_btcvsd_Free_Buffer(fp, 0);*/
			else if (arg == 2)
				ret = AudDrv_btcvsd_Allocate_Buffer(fp, 1);
			else if (arg == 3)
				pr_debug("%s(), do nothing\n", __func__);
				/*ret = AudDrv_btcvsd_Free_Buffer(fp, 1);*/
			break;
		}

	case SET_BTCVSD_STATE:{
			pr_debug("AudDrv SET_BTCVSD_STATE\n");
			if (arg == BT_SCO_TXSTATE_DIRECT_LOOPBACK) {
				btsco.uTXState = arg;
				btsco.uRXState = arg;
			} else if ((arg & 0x10) == 0) {	/*TX state*/
				btsco.uTXState = arg;
				pr_debug("SET_BTCVSD_STATE set btsco.uTXState to 0x%lu\n", arg);
			} else {	/*RX state*/
				btsco.uRXState = arg;
				pr_debug("SET_BTCVSD_STATE set btsco.uRXState to %lu\n", arg);
			}
			if (btsco.uTXState == BT_SCO_TXSTATE_IDLE
			    && btsco.uRXState == BT_SCO_RXSTATE_IDLE) {
				pr_debug("SET_BTCVSD_STATE disable BT IRQ disableBTirq = %d\n",
					 disableBTirq);
				if (disableBTirq == 0) {
					disable_irq(btcvsd_irq_number);
					Disable_CVSD_Wakeup();
					disableBTirq = 1;
				}
			} else {
				if (disableBTirq == 1) {
					pr_debug
					    ("SET_BTCVSD_STATE enable BT IRQ disableBTirq = %d\n",
					     disableBTirq);
					enable_irq(btcvsd_irq_number);
					Enable_CVSD_Wakeup();
					disableBTirq = 0;
				}
			}
			break;
		}
	case GET_BTCVSD_STATE:{
			break;
		}
	default:{
			pr_debug("AudDrv_btcvsd_ioctl Fail command: %x\n", cmd);
			ret = -1;
			break;
		}
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long AudDrv_btcvsd_compat_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	long ret;

	pr_debug("AudDrv_btcvsd_compat_ioctl cmd = 0x%x arg = %lu\n", cmd, arg);

	if (!fp->f_op || !fp->f_op->unlocked_ioctl)
		return -ENOTTY;

	ret = fp->f_op->unlocked_ioctl(fp, cmd, arg);
	if (ret < 0)
		pr_debug("AudDrv_btcvsd_compat_ioctl Fail\n");
	else
		pr_debug("-AudDrv_btcvsd_compat_ioctl\n");

	return ret;
}
#endif
/*=============================================================================================
      BT SCO Internal Function
  =============================================================================================*/

static void AudDrv_BTCVSD_DataTransfer(BT_SCO_DIRECT uDir, kal_uint8 *pSrc,
				       kal_uint8 *pDst, kal_uint32 uBlockSize,
				       kal_uint32 uBlockNum, CVSD_STATE uState)
{
	kal_int32 i, j;

	if (uBlockSize == 60 || uBlockSize == 120 || uBlockSize == 20) {
		kal_uint32 *pSrc32 = (kal_uint32 *) pSrc;
		kal_uint32 *pDst32 = (kal_uint32 *) pDst;

		for (i = 0; i < (uBlockSize * uBlockNum / 4); i++)
			*pDst32++ = *pSrc32++;
	} else {
		kal_uint16 *pSrc16 = (kal_uint16 *) pSrc;
		kal_uint16 *pDst16 = (kal_uint16 *) pDst;

		for (j = 0; j < uBlockNum; j++) {
			for (i = 0; i < (uBlockSize / 2); i++)
				*pDst16++ = *pSrc16++;

			if (uDir == BT_SCO_DIRECT_BT2ARM)
				pSrc16++;
			else
				pDst16++;
		}
	}
}

static void AudDrv_BTCVSD_ReadFromBT(kal_uint8 uLen,
				     kal_uint32 uPacketLength, kal_uint32 uPacketNumber,
				     kal_uint32 uBlockSize, kal_uint32 uControl)
{
	kal_int32 i;
	kal_uint16 pv;
	kal_uint8 *pSrc;
	kal_uint8 *pPacketBuf;
	unsigned long flags;
	unsigned long connsys_addr_rx, ap_addr_rx;

	PRINTK_AUDDRV("AudDrv_BTCVSD_ReadFromBT(+) btsco.pRX->iPacket_w=%d\n",
		      btsco.pRX->iPacket_w);

	connsys_addr_rx = *bt_hw_REG_PACKET_R;
	ap_addr_rx = (unsigned long)BTSYS_SRAM_BANK2_BASE_ADDRESS + (connsys_addr_rx & 0xFFFF);
	PRINTK_AUDDRV("AudDrv_BTCVSD_ReadFromBT connsys_addr_rx=0x%lx,ap_addr_rx=0x%lx\n",
		      connsys_addr_rx, ap_addr_rx);
	pSrc = (kal_uint8 *) ap_addr_rx;

	PRINTK_AUDDRV("AudDrv_BTCVSD_ReadFromBT()uPacketLength=%d,uPacketNumber=%d, btsco.uRXState=%d\n",
	     uPacketLength, uPacketNumber, btsco.uRXState);
	AudDrv_BTCVSD_DataTransfer(BT_SCO_DIRECT_BT2ARM, pSrc, btsco.pRX->TempPacketBuf,
				   uPacketLength, uPacketNumber, btsco.uRXState);
	PRINTK_AUDDRV("AudDrv_BTCVSD_ReadFromBT()AudDrv_BTCVSD_DataTransfer DONE!!!,uControl=0x%x,uLen=%d\n",
	     uControl, uLen);

	spin_lock_irqsave(&auddrv_BTCVSDRX_lock, flags);
	for (i = 0; i < uBlockSize; i++) {
#ifdef TEST_PACKETLOSS
		packet_cnt++;
		if (packet_cnt == 30) {
			pr_debug("AudDrv_BTCVSD_ReadFromBT()Test Packet Loss\n");
			memset((void *)uSilencePattern, 0x55, SCO_RX_PLC_SIZE);
			memcpy(btsco.pRX->PacketBuf[btsco.pRX->iPacket_w & SCO_RX_PACKET_MASK],
			       (void *)&uSilencePattern, SCO_RX_PLC_SIZE);
			pv = 0;
			packet_cnt = 0;
		} else {
			memcpy(btsco.pRX->PacketBuf[btsco.pRX->iPacket_w & SCO_RX_PACKET_MASK],
			       btsco.pRX->TempPacketBuf + (SCO_RX_PLC_SIZE * i), SCO_RX_PLC_SIZE);
			if ((uControl & btsco_PacketValidMask[uLen][i]) ==
			    btsco_PacketValidMask[uLen][i]) {
				pv = 1;
			} else {
				pv = 0;
			}
		}
#else
		memcpy(btsco.pRX->PacketBuf[btsco.pRX->iPacket_w & SCO_RX_PACKET_MASK],
		       btsco.pRX->TempPacketBuf + (SCO_RX_PLC_SIZE * i), SCO_RX_PLC_SIZE);

		BUG_ON(uLen >= BT_SCO_CVSD_MAX);
		if ((uControl & btsco_PacketValidMask[uLen][i]) == btsco_PacketValidMask[uLen][i])
			pv = 1;
		else
			pv = 0;
#endif

		pPacketBuf = (kal_uint8 *) btsco.pRX->PacketBuf + (btsco.pRX->iPacket_w &
								   SCO_RX_PACKET_MASK) *
		    (SCO_RX_PLC_SIZE + BTSCO_CVSD_PACKET_VALID_SIZE) + SCO_RX_PLC_SIZE;
		memcpy((void *)pPacketBuf, (void *)&pv, BTSCO_CVSD_PACKET_VALID_SIZE);
		btsco.pRX->iPacket_w++;
	}
	spin_unlock_irqrestore(&auddrv_BTCVSDRX_lock, flags);
	PRINTK_AUDDRV("AudDrv_BTCVSD_ReadFromBT(-) btsco.pRX->iPacket_w=%d\n",
		      btsco.pRX->iPacket_w);
}

static void AudDrv_BTCVSD_WriteToBT(BT_SCO_PACKET_LEN uLen,
				    kal_uint32 uPacketLength, kal_uint32 uPacketNumber,
				    kal_uint32 uBlockSize)
{
	kal_int32 i;
	unsigned long flags;
	kal_uint8 *pDst;
	unsigned long connsys_addr_tx, ap_addr_tx;

	/*pr_debug("AudDrv_BTCVSD_WriteToBT(+) btsco.pTX->iPacket_r=%d\n",btsco.pTX->iPacket_r);*/

	spin_lock_irqsave(&auddrv_BTCVSDTX_lock, flags);
	if (btsco.pTX != NULL) {
		for (i = 0; i < uBlockSize; i++) {
			memcpy((void *)(btsco.pTX->TempPacketBuf + (SCO_TX_ENCODE_SIZE * i)),
			       (void *)(btsco.
					pTX->PacketBuf[btsco.pTX->iPacket_r & SCO_TX_PACKET_MASK]),
			       SCO_TX_ENCODE_SIZE);
			btsco.pTX->iPacket_r++;
		}
	}
	spin_unlock_irqrestore(&auddrv_BTCVSDTX_lock, flags);

	connsys_addr_tx = *bt_hw_REG_PACKET_W;
	ap_addr_tx = (unsigned long)BTSYS_SRAM_BANK2_BASE_ADDRESS + (connsys_addr_tx & 0xFFFF);
	PRINTK_AUDDRV("AudDrv_BTCVSD_WriteToBT connsys_addr_tx=0x%lx,ap_addr_tx=0x%lx\n",
		      connsys_addr_tx, ap_addr_tx);
	pDst = (kal_uint8 *) ap_addr_tx;

	if (btsco.pTX != NULL) {
		AudDrv_BTCVSD_DataTransfer(BT_SCO_DIRECT_ARM2BT, btsco.pTX->TempPacketBuf, pDst,
					   uPacketLength, uPacketNumber, btsco.uTXState);
	}
	/*pr_debug("AudDrv_BTCVSD_WriteToBT(-),btsco.pTX->iPacket_r=%d\n",btsco.pTX->iPacket_r);*/
}

static int AudDrv_BTCVSD_IRQ_handler(void)
{
	kal_uint32 uPacketNumber, uPacketLength, uBufferCount_TX,
	    uBufferCount_RX, uControl;
	kal_uint8 uPacketType;

	PRINTK_AUDDRV("AudDrv_BTCVSD_IRQ_handler FILL PACKETBUF\n");
	if ((btsco.uRXState != BT_SCO_RXSTATE_RUNNING && btsco.uRXState != BT_SCO_RXSTATE_ENDING)
	    && (btsco.uTXState != BT_SCO_TXSTATE_RUNNING && btsco.uTXState != BT_SCO_TXSTATE_ENDING)
	    && (btsco.uTXState != BT_SCO_TXSTATE_DIRECT_LOOPBACK)) {
		pr_debug
		    ("AudDrv_BTCVSD_IRQ_handler in idle state: btsco.uRXState: %d, btsco.uTXState: %d\n",
		     btsco.uRXState, btsco.uTXState);
		*bt_hw_REG_CONTROL &= ~BT_CVSD_CLEAR;
		goto AudDrv_BTCVSD_IRQ_handler_exit;
	}
	uControl = *bt_hw_REG_CONTROL;
	uPacketType = (uControl >> 18) & 0x7;
	PRINTK_AUDDRV("AudDrv_BTCVSD_IRQ_handler BT uControl =0x%x, BT uPacketType=%d\n",
		      uControl, uPacketType);

	if (((uControl >> 31) & 1) == 0) {
		*bt_hw_REG_CONTROL &= ~BT_CVSD_CLEAR;
		goto AudDrv_BTCVSD_IRQ_handler_exit;
	}

	BUG_ON(uPacketType >= BT_SCO_CVSD_MAX);
	uPacketLength = (kal_uint32) btsco_PacketInfo[uPacketType][0];
	uPacketNumber = (kal_uint32) btsco_PacketInfo[uPacketType][1];
	uBufferCount_TX = (kal_uint32) btsco_PacketInfo[uPacketType][2];
	uBufferCount_RX = (kal_uint32) btsco_PacketInfo[uPacketType][3];

	PRINTK_AUDDRV("AudDrv_BTCVSD_IRQ_handler uPacketLength=%d, uPacketNumber=%d, uBufferCount_TX=%d,_RX=%d\n",
	     uPacketLength, uPacketNumber, uBufferCount_TX, uBufferCount_RX);
	PRINTK_AUDDRV("btsco.uTXState=0x%x,btsco.uRXState=0x%x\n", btsco.uTXState, btsco.uRXState);

	if (btsco.pTX && btsco.uTXState == BT_SCO_TXSTATE_DIRECT_LOOPBACK) {
		kal_uint8 *pSrc, *pDst;
		unsigned long connsys_addr_rx, ap_addr_rx, connsys_addr_tx, ap_addr_tx;

		connsys_addr_rx = *bt_hw_REG_PACKET_R;
		ap_addr_rx = (unsigned long)BTSYS_SRAM_BANK2_BASE_ADDRESS +
		    (connsys_addr_rx & 0xFFFF);
		PRINTK_AUDDRV("AudDrv_BTCVSD_ReadFromBT connsys_addr_rx=0x%lx,ap_addr_rx=0x%lx\n",
			      connsys_addr_rx, ap_addr_rx);
		pSrc = (kal_uint8 *) ap_addr_rx;

		connsys_addr_tx = *bt_hw_REG_PACKET_W;
		ap_addr_tx = (unsigned long)BTSYS_SRAM_BANK2_BASE_ADDRESS +
		    (connsys_addr_tx & 0xFFFF);
		PRINTK_AUDDRV("AudDrv_BTCVSD_WriteToBT connsys_addr_tx=0x%lx,ap_addr_tx=0x%lx\n",
			      connsys_addr_tx, ap_addr_tx);
		pDst = (kal_uint8 *) ap_addr_tx;

		AudDrv_BTCVSD_DataTransfer(BT_SCO_DIRECT_BT2ARM, pSrc, btsco.pTX->TempPacketBuf,
					   uPacketLength, uPacketNumber, BT_SCO_RXSTATE_RUNNING);
		AudDrv_BTCVSD_DataTransfer(BT_SCO_DIRECT_ARM2BT, btsco.pTX->TempPacketBuf, pDst,
					   uPacketLength, uPacketNumber, BT_SCO_TXSTATE_RUNNING);
		writeToBT_cnt++;
		readFromBT_cnt++;
	} else {
		if (btsco.pRX) {
			if (btsco.uRXState == BT_SCO_RXSTATE_RUNNING
			    || btsco.uRXState == BT_SCO_RXSTATE_ENDING) {
				PRINTK_AUDDRV("AudDrv_BTCVSD_IRQ_handler Overflow=%d,Pck_w=%d,_r=%d,BufCnt=%d\n",
				     btsco.pRX->fOverflow, btsco.pRX->iPacket_w,
				     btsco.pRX->iPacket_r, uBufferCount_RX);
				if (btsco.pRX->fOverflow) {
					if (btsco.pRX->iPacket_w - btsco.pRX->iPacket_r <=
					    SCO_RX_PACKER_BUF_NUM - 2 * uBufferCount_RX) {
						/*free space is larger then twice interrupt rx data size*/
						btsco.pRX->fOverflow = false; /* KAL_FALSE; */
						pr_debug("AudDrv_BTCVSD_IRQ_handler pRX->fOverflow FALSE!!!\n");
					}
				}

				if (!btsco.pRX->fOverflow
				    && (btsco.pRX->iPacket_w - btsco.pRX->iPacket_r <=
					SCO_RX_PACKER_BUF_NUM - uBufferCount_RX)) {
					AudDrv_BTCVSD_ReadFromBT(uPacketType, uPacketLength,
								 uPacketNumber, uBufferCount_RX,
								 uControl);
					readFromBT_cnt++;
				} else {
					btsco.pRX->fOverflow = true; /* KAL_TRUE; */
					pr_debug
					    ("AudDrv_BTCVSD_IRQ_handler pRX->fOverflow TRUE!!!\n");
				}
			}
		}

		if (btsco.pTX) {
			if (btsco.uTXState == BT_SCO_TXSTATE_RUNNING
			    || btsco.uTXState == BT_SCO_TXSTATE_ENDING) {
				PRINTK_AUDDRV("AudDrv_BTCVSD_IRQ_handler TX->Underflow=%d,Pck_w=%d,_r=%d,BufCnt=%d\n",
				     btsco.pTX->fUnderflow, btsco.pTX->iPacket_w,
				     btsco.pTX->iPacket_r, uBufferCount_TX);
				if (btsco.pTX->fUnderflow) {
					/*prepared data is larger then twice interrupt tx data size*/
					if (btsco.pTX->iPacket_w - btsco.pTX->iPacket_r >=
					    2 * uBufferCount_TX) {
						btsco.pTX->fUnderflow = false; /* KAL_FALSE; */
						pr_debug
						    ("AudDrv_BTCVSD_IRQ_handler pTX->fUnderflow FALSE!!!\n");
					}
				}

				if ((!btsco.pTX->fUnderflow
				     && (btsco.pTX->iPacket_w - btsco.pTX->iPacket_r >=
					 uBufferCount_TX))
				    || btsco.uTXState == BT_SCO_TXSTATE_ENDING) {
					AudDrv_BTCVSD_WriteToBT(uPacketType, uPacketLength,
								uPacketNumber, uBufferCount_TX);
					writeToBT_cnt++;
				} else {
					btsco.pTX->fUnderflow = true; /* KAL_TRUE; */
					pr_debug
					    ("AudDrv_BTCVSD_IRQ_handler pTX->fUnderflow TRUE!!!\n");
				}
			}
		}
	}
	PRINTK_AUDDRV("writeToBT_cnt=%d, readFromBT_cnt=%d\n", writeToBT_cnt, readFromBT_cnt);

	*bt_hw_REG_CONTROL &= ~BT_CVSD_CLEAR;

	BTCVSD_read_wait_queue_flag = 1;
	wake_up_interruptible(&BTCVSD_Read_Wait_Queue);
	BTCVSD_write_wait_queue_flag = 1;
	wake_up_interruptible(&BTCVSD_Write_Wait_Queue);

AudDrv_BTCVSD_IRQ_handler_exit:
	return IRQ_HANDLED;
}

static int AudDrv_btcvsd_probe(struct platform_device *dev)
{
	int ret = 0;

	pr_debug("AudDrv_btcvsd_probe\n");

	dev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!dev->dev.dma_mask)
		dev->dev.dma_mask = &dev->dev.coherent_dma_mask;

	mDev = &dev->dev;


#ifdef CONFIG_OF
	Auddrv_BTCVSD_Irq_Map();
#endif
	ret =
		request_irq(btcvsd_irq_number, (irq_handler_t) AudDrv_BTCVSD_IRQ_handler,
			  IRQF_TRIGGER_LOW /*IRQF_TRIGGER_FALLING */ , "BTCVSD_ISR_Handle", dev);

	if (ret < 0) {
		pr_debug("AudDrv_btcvsd_probe request_irq btcvsd_irq_number(%d) Fail\n",
			 btcvsd_irq_number);
	}
	/* inremap to INFRA sys */
#ifdef CONFIG_OF
	Auddrv_BTCVSD_Address_Map();
	INFRA_MISC_ADDRESS = (volatile kal_uint32 *)(infra_base + infra_misc_offset);
#else
	AUDIO_INFRA_BASE_VIRTUAL = ioremap_nocache(AUDIO_INFRA_BASE_PHYSICAL, 0x1000);
	INFRA_MISC_ADDRESS = (volatile kal_uint32 *)(AUDIO_INFRA_BASE_VIRTUAL + INFRA_MISC_OFFSET);
#endif
	pr_debug("[BTCVSD probe] INFRA_MISC_ADDRESS = %p\n", INFRA_MISC_ADDRESS);

	pr_debug("AudDrv_btcvsd_probe disable BT IRQ disableBTirq = %d\n", disableBTirq);
	if (disableBTirq == 0) {
		disable_irq(btcvsd_irq_number);
		Disable_CVSD_Wakeup();
		disableBTirq = 1;
	}
	/* init */
	memset((void *)&BT_CVSD_Mem, 0, sizeof(CVSD_MEMBLOCK_T));

	memset((void *)&btsco, 0, sizeof(btsco));
	btsco.uTXState = BT_SCO_TXSTATE_IDLE;
	btsco.uRXState = BT_SCO_RXSTATE_IDLE;

	/* ioremap to BT HW register base address */
#ifdef CONFIG_OF
	BTSYS_PKV_BASE_ADDRESS = (void *)btsys_pkv_physical_base;
	BTSYS_SRAM_BANK2_BASE_ADDRESS = (void *)btsys_sram_bank2_physical_base;
	bt_hw_REG_PACKET_R = BTSYS_PKV_BASE_ADDRESS + cvsd_mcu_read_offset;
	bt_hw_REG_PACKET_W = BTSYS_PKV_BASE_ADDRESS + cvsd_mcu_write_offset;
	bt_hw_REG_CONTROL = BTSYS_PKV_BASE_ADDRESS + cvsd_packet_indicator;
#else
	BTSYS_PKV_BASE_ADDRESS = ioremap_nocache(AUDIO_BTSYS_PKV_PHYSICAL_BASE, 0x10000);
	BTSYS_SRAM_BANK2_BASE_ADDRESS =
	    ioremap_nocache(AUDIO_BTSYS_SRAM_BANK2_PHYSICAL_BASE, 0x10000);
	bt_hw_REG_PACKET_R = (volatile kal_uint32 *)(BTSYS_PKV_BASE_ADDRESS + CVSD_MCU_READ_OFFSET);
	bt_hw_REG_PACKET_W = (volatile kal_uint32 *)(BTSYS_PKV_BASE_ADDRESS +
						     CVSD_MCU_WRITE_OFFSET);
	bt_hw_REG_CONTROL = (volatile kal_uint32 *)(BTSYS_PKV_BASE_ADDRESS + CVSD_PACKET_INDICATOR);
#endif
	pr_debug("[BTCVSD probe] BTSYS_PKV_BASE_ADDRESS = %p BTSYS_SRAM_BANK2_BASE_ADDRESS = %p\n",
	       BTSYS_PKV_BASE_ADDRESS, BTSYS_SRAM_BANK2_BASE_ADDRESS);

	pr_debug("-AudDrv_btcvsd_probe\n");
	return 0;
}


static int AudDrv_btcvsd_open(struct inode *inode, struct file *fp)
{
	PRINTK_AUDDRV(ANDROID_LOG_INFO, "Sound",
		      "AudDrv_btcvsd_open do nothing inode:%p, file:%pss\n", inode, fp);
	return 0;
}

static ssize_t AudDrv_btcvsd_write(struct file *fp, const char __user *data,
				   size_t count, loff_t *offset)
{
	int written_size = count, ret = 0, copy_size = 0, BTSCOTX_WriteIdx;
	unsigned long flags;
	char *data_w_ptr = (char *)data;
	kal_uint64 write_timeout_limit;
	int max_timeout_trial = 2;

	if ((btsco.pTX == NULL) || (btsco.pTX->u4BufferSize == 0)) {
		pr_err("AudDrv_btcvsd_write btsco.pTX||pTX->u4BufferSize==0!!!\n");
		msleep(60);
		return written_size;
	}

	/*ns*/
	write_timeout_limit =
	((kal_uint64) SCO_TX_PACKER_BUF_NUM * SCO_TX_ENCODE_SIZE * 16 * 1000000000) / 2 / 2 / 64000;

	while (count) {
		/*pr_debug("AudDrv_btcvsd_write btsco.pTX->iPacket_w=%d, btsco.pTX->iPacket_r=%d\n",
		btsco.pTX->iPacket_w, btsco.pTX->iPacket_r);*/
		spin_lock_irqsave(&auddrv_BTCVSDTX_lock, flags);
		/*  free space of TX packet buffer */
		copy_size = btsco.pTX->u4BufferSize - (btsco.pTX->iPacket_w - btsco.pTX->iPacket_r)
		* SCO_TX_ENCODE_SIZE;
		spin_unlock_irqrestore(&auddrv_BTCVSDTX_lock, flags);
		if (count <= (kal_uint32) copy_size)
			copy_size = count;
		/*pr_debug("AudDrv_btcvsd_write count=%d, copy_size=%d\n",count, copy_size);*/

		/*copysize must be multiple of SCO_TX_ENCODE_SIZE*/
		BUG_ON(!(copy_size % SCO_TX_ENCODE_SIZE == 0));

		if (copy_size != 0) {
			spin_lock_irqsave(&auddrv_BTCVSDTX_lock, flags);
			BTSCOTX_WriteIdx = (btsco.pTX->iPacket_w & SCO_TX_PACKET_MASK) *
			    SCO_TX_ENCODE_SIZE;
			spin_unlock_irqrestore(&auddrv_BTCVSDTX_lock, flags);

			if (BTSCOTX_WriteIdx + copy_size < btsco.pTX->u4BufferSize) {	/* copy once */
				if (!access_ok(VERIFY_READ, data_w_ptr, copy_size)) {
					pr_debug
					    ("AudDrv_btcvsd_write 0ptr invalid data_w_ptr=%lx, size=%d\n",
					     (unsigned long)data_w_ptr, copy_size);
					pr_debug
					    ("AudDrv_btcvsd_write u4BufferSize=%d, BTSCOTX_WriteIdx=%d\n",
					     btsco.pTX->u4BufferSize, BTSCOTX_WriteIdx);
				} else {
					/*PRINTK_AUDDRV("mcmcpy btsco.pTX->PacketBuf+BTSCOTX_WriteIdx= %x,
					data_w_ptr = %p, copy_size = %x\n", btsco.pTX->PacketBuf+BTSCOTX_WriteIdx,
					data_w_ptr,copy_size);*/
					if (copy_from_user
					    ((void *)((kal_uint8 *) btsco.pTX->PacketBuf +
						      BTSCOTX_WriteIdx),
					     (const void __user *)data_w_ptr, copy_size)) {
						pr_debug
						    ("AudDrv_btcvsd_write Fail copy_from_user\n");
						return -1;
					}
				}

				spin_lock_irqsave(&auddrv_BTCVSDTX_lock, flags);
				btsco.pTX->iPacket_w += copy_size / SCO_TX_ENCODE_SIZE;
				spin_unlock_irqrestore(&auddrv_BTCVSDTX_lock, flags);
				data_w_ptr += copy_size;
				count -= copy_size;
				/*pr_debug("AudDrv_btcvsd_write finish1, copy_size:%d, pTX->iPacket_w:%d,
				pTX->iPacket_r=%d, count=%d \r\n",  copy_size,btsco.pTX->iPacket_w,
				btsco.pTX->iPacket_r,count);*/
			} else {	/* copy twice */
				kal_int32 size_1 = 0, size_2 = 0;

				size_1 = btsco.pTX->u4BufferSize - BTSCOTX_WriteIdx;
				size_2 = copy_size - size_1;
				/*pr_debug("AudDrv_btcvsd_write size_1=%d, size_2=%d\n",size_1,size_2);*/
				BUG_ON(!(size_1 % SCO_TX_ENCODE_SIZE == 0));
				BUG_ON(!(size_2 % SCO_TX_ENCODE_SIZE == 0));
				if (!access_ok(VERIFY_READ, data_w_ptr, size_1)) {
					pr_debug
					    ("AudDrv_btcvsd_write 1ptr invalid data_w_ptr=%lx, size_1=%d\n",
					     (unsigned long)data_w_ptr, size_1);
					pr_debug
					    ("AudDrv_btcvsd_write u4BufferSize=%d, BTSCOTX_WriteIdx=%d\n",
					     btsco.pTX->u4BufferSize, BTSCOTX_WriteIdx);
				} else {
					/*PRINTK_AUDDRV("mcmcpy btsco.pTX->PacketBuf+BTSCOTX_WriteIdx= %x
					data_w_ptr = %p size_1 = %x\n",  btsco.pTX->PacketBuf+BTSCOTX_WriteIdx,
					data_w_ptr,size_1);*/
					if ((copy_from_user
					     ((void *)((kal_uint8 *) btsco.pTX->PacketBuf +
						       BTSCOTX_WriteIdx),
					      (const void __user *)data_w_ptr, size_1))) {
						pr_debug("AudDrv_write Fail 1 copy_from_user\n");
						return -1;
					}
				}
				spin_lock_irqsave(&auddrv_BTCVSDTX_lock, flags);
				btsco.pTX->iPacket_w += size_1 / SCO_TX_ENCODE_SIZE;
				spin_unlock_irqrestore(&auddrv_BTCVSDTX_lock, flags);

				if (!access_ok(VERIFY_READ, data_w_ptr + size_1, size_2)) {
					pr_debug
					    ("AudDrv_btcvsd_write 2ptr invalid data_w_ptr=%lx, size_1=%d, size_2=%d\n",
					     (unsigned long)data_w_ptr, size_1, size_2);
					pr_debug
					    ("AudDrv_btcvsd_write u4BufferSize=%d, pTX->iPacket_w=%d\n",
					     btsco.pTX->u4BufferSize, btsco.pTX->iPacket_w);
				} else {
					/*PRINTK_AUDDRV("mcmcpy btsco.pTX->PacketBuf+BTSCOTX_WriteIdx+size_1= %x,
					data_w_ptr+size_1 = %p, size_2 = %x\n"
					, btsco.pTX->PacketBuf+BTSCOTX_WriteIdx+size_1, data_w_ptr+size_1,size_2);*/
					if ((copy_from_user
					     ((void *)((kal_uint8 *) btsco.pTX->PacketBuf),
					      (const void __user *)(data_w_ptr + size_1),
					      size_2))) {
						pr_debug
						    ("AudDrv_btcvsd_write Fail 2 copy_from_user\n");
						return -1;
					}
				}

				spin_lock_irqsave(&auddrv_BTCVSDTX_lock, flags);

				btsco.pTX->iPacket_w += size_2 / SCO_TX_ENCODE_SIZE;
				spin_unlock_irqrestore(&auddrv_BTCVSDTX_lock, flags);
				count -= copy_size;
				data_w_ptr += copy_size;
				/*pr_debug("AudDrv_btcvsd_write finish2, copy size:%d, pTX->iPacket_w=%d,
				pTX->iPacket_r=%d, count:%d\r\n", copy_size,btsco.pTX->iPacket_w,
				btsco.pTX->iPacket_r,count );*/
			}
		}

		if (count != 0) {
			kal_uint64 t1, t2;
			/*pr_debug("AudDrv_btcvsd_write WAITING...btsco.pTX->iPacket_w=%d,
			count=%d\n",btsco.pTX->iPacket_w,count);*/
			t1 = sched_clock();
			BTCVSD_write_wait_queue_flag = 0;
			ret = wait_event_interruptible_timeout(BTCVSD_Write_Wait_Queue,
							       BTCVSD_write_wait_queue_flag,
							       nsecs_to_jiffies(write_timeout_limit));
			t2 = sched_clock();
			/*pr_debug("AudDrv_btcvsd_write WAKEUP...count=%d\n",count);*/
			t2 = t2 - t1;	/* in ns (10^9) */

			PRINTK_AUDDRV("%s(), WAKEUP...wait event interrupt, ret = %d, flag = %d\n",
			      __func__,
			      ret,
			      BTCVSD_write_wait_queue_flag);

			if (t2 > write_timeout_limit) {
				pr_warn("%s timeout, %llu ns, timeout_limit %llu, ret %d, flag %d\n",
					__func__,
					t2, write_timeout_limit,
					ret,
					BTCVSD_write_wait_queue_flag);
			}

			if (ret < 0) {
				/* error, -ERESTARTSYS if it was interrupted by a signal */
				max_timeout_trial--;
				pr_err("%s(), error, trial left %d, written_size %d\n",
				       __func__,
				       max_timeout_trial,
				       written_size);

				if (max_timeout_trial <= 0)
					return written_size;
			} else if (ret == 0) {
				/* conidtion is false after timeout */
				max_timeout_trial--;
				pr_err("%s(), error, timeout, condition is false, trial left %d, written_size %d\n",
				       __func__,
				       max_timeout_trial,
				       written_size);

				if (max_timeout_trial <= 0)
					return written_size;
			} else if (ret == 1) {
				/* condition is true after timeout */
				pr_debug("%s(), timeout, condition is true\n", __func__);
			} else {
				pr_debug("%s(), condition is true before timeout\n", __func__);
			}
		}
		/* here need to wait for interrupt handler */
	}
	PRINTK_AUDDRV("AudDrv_btcvsd_write written_size = %d, write_timeout_limit=%llu\n",
		      written_size, write_timeout_limit);
	return written_size;
}


static ssize_t AudDrv_btcvsd_read(struct file *fp, char __user *data,
				  size_t count, loff_t *offset)
{
	char *Read_Data_Ptr = (char *)data;
	int ret;
	ssize_t read_size = 0, read_count = 0, BTSCORX_ReadIdx_tmp;
	unsigned long u4DataRemained;
	unsigned long flags;
	kal_uint64 read_timeout_limit;
	int max_timeout_trial = 2;

	if ((btsco.pRX == NULL) || (btsco.pRX->u4BufferSize == 0)) {
		pr_err("AudDrv_btcvsd_read btsco.pRX || pRX->u4BufferSize == 0!\n");
		msleep(60);
		return -1;
	}

	read_timeout_limit = ((kal_uint64) SCO_RX_PACKER_BUF_NUM * SCO_RX_PLC_SIZE * 16 *
			      1000000000) / 2 / 2 / 64000;

	while (count) {
		PRINTK_AUDDRV("AudDrv_btcvsd_read btsco.pRX->iPacket_w=%d,iPacket_r=%d,count=%zu\n",
		     btsco.pRX->iPacket_w, btsco.pRX->iPacket_r, count);

		spin_lock_irqsave(&auddrv_BTCVSDRX_lock, flags);
		/*  available data in RX packet buffer */
		u4DataRemained = (btsco.pRX->iPacket_w - btsco.pRX->iPacket_r)*
		(SCO_RX_PLC_SIZE+BTSCO_CVSD_PACKET_VALID_SIZE);
		if (count > u4DataRemained)
			read_size = u4DataRemained;
		else
			read_size = count;

		BTSCORX_ReadIdx_tmp = (btsco.pRX->iPacket_r & SCO_RX_PACKET_MASK) *
		    (SCO_RX_PLC_SIZE + BTSCO_CVSD_PACKET_VALID_SIZE);
		spin_unlock_irqrestore(&auddrv_BTCVSDRX_lock, flags);

		BUG_ON(!(read_size % (SCO_RX_PLC_SIZE + BTSCO_CVSD_PACKET_VALID_SIZE) == 0));

		PRINTK_AUDDRV("AudDrv_btcvsd_read read_size=%zu, BTSCORX_ReadIdx_tmp=%zu\n",
			      read_size, BTSCORX_ReadIdx_tmp);

		if (BTSCORX_ReadIdx_tmp + read_size < btsco.pRX->u4BufferSize) {
			/* copy once */
			PRINTK_AUDDRV("AudDrv_btcvsd_read 1 copy_to_user target=0x%p,source=0x%p,read_size=%zu\n",
			     Read_Data_Ptr,
			     ((unsigned char *)btsco.pRX->PacketBuf + BTSCORX_ReadIdx_tmp),
			     read_size);
			if (copy_to_user((void __user *)Read_Data_Ptr,
			     (void *)((kal_uint8 *) btsco.pRX->PacketBuf + BTSCORX_ReadIdx_tmp), read_size)) {
				pr_debug("AudDrv_btcvsd_read Fail 1 copy to user Ptr:%p,PcktBuf:%p,RIdx_tmp:%zu,r_sz:%zu",
				     Read_Data_Ptr, (kal_uint8 *) btsco.pRX->PacketBuf, BTSCORX_ReadIdx_tmp, read_size);
				if (read_count == 0)
					return -1;
				else
					return read_count;
			}

			read_count += read_size;
			spin_lock_irqsave(&auddrv_BTCVSDRX_lock, flags);
			/* 2 byte is packetvalid info */
			btsco.pRX->iPacket_r += read_size / (SCO_RX_PLC_SIZE + BTSCO_CVSD_PACKET_VALID_SIZE);
			spin_unlock_irqrestore(&auddrv_BTCVSDRX_lock, flags);

			Read_Data_Ptr += read_size;
			count -= read_size;
			PRINTK_AUDDRV("AudDrv_btcvsd_read finish1, r_sz:%zu, iPacket_r:0x%x,iPacket_w:%x,count:%zu\r\n",
			    read_size, btsco.pRX->iPacket_r, btsco.pRX->iPacket_w, count);
		}
		/* copy twice */
		else {
			unsigned long size_1 = btsco.pRX->u4BufferSize - BTSCORX_ReadIdx_tmp;
			unsigned long size_2 = read_size - size_1;

			PRINTK_AUDDRV("AudDrv_btcvsd_read 2-2 copy_to_user target=%p, source=0x%p, size_1=%zu\n",
				Read_Data_Ptr, ((unsigned char *)btsco.pRX->PacketBuf + BTSCORX_ReadIdx_tmp), size_1);
			if (copy_to_user
			    ((void __user *)Read_Data_Ptr,
			     (void *)((kal_uint8 *) btsco.pRX->PacketBuf + BTSCORX_ReadIdx_tmp),
			     size_1)) {
				pr_debug("AudDrv_btcvsd_read Fail 2 copy to user R_Ptr:%p, PacketBuf:%p,RIdx_tmp:0x%zu, r_sz:%zu",
				     Read_Data_Ptr, btsco.pRX->PacketBuf, BTSCORX_ReadIdx_tmp, read_size);
				if (read_count == 0)
					return -1;
				else
					return read_count;
			}

			read_count += size_1;
			spin_lock_irqsave(&auddrv_BTCVSDRX_lock, flags);
			/* 2 byte is packetvalid info */
			btsco.pRX->iPacket_r += size_1 / (SCO_RX_PLC_SIZE + BTSCO_CVSD_PACKET_VALID_SIZE);
			spin_unlock_irqrestore(&auddrv_BTCVSDRX_lock, flags);

			PRINTK_AUDDRV("AudDrv_btcvsd_read 2-2 copy_to_user target=0x%p, source=0x%p,size_2=%zu\n",
			     (Read_Data_Ptr + size_1),
			     ((unsigned char *)btsco.pRX->PacketBuf + BTSCORX_ReadIdx_tmp + size_1),
			     size_2);
			if (copy_to_user
			    ((void __user *)(Read_Data_Ptr + size_1),
			     (void *)((kal_uint8 *) btsco.pRX->PacketBuf), size_2)) {

				if (read_count == 0)
					return -1;
				else
					return read_count;
			}

			read_count += size_2;
			spin_lock_irqsave(&auddrv_BTCVSDRX_lock, flags);
			/* 2 byte is packetvalid info */
			btsco.pRX->iPacket_r += size_2 / (SCO_RX_PLC_SIZE + BTSCO_CVSD_PACKET_VALID_SIZE);
			spin_unlock_irqrestore(&auddrv_BTCVSDRX_lock, flags);

			count -= read_size;
			Read_Data_Ptr += read_size;
			/*PRINTK_AUDDRV("AudDrv_btcvsd_read finish3, copy size_2:%zu, pRX->iPacket_r:0x%x,
				pRX->iPacket_w:0x%x u4DataRemained:%zu\r\n",
			     size_2, btsco.pRX->iPacket_r, btsco.pRX->iPacket_w, u4DataRemained);*/
		}

		if (count != 0) {
			kal_uint64 t1, t2;

			PRINTK_AUDDRV("AudDrv_btcvsd_read WAITING... pRX->iPacket_r=0x%x, count=%zu\n",
			     btsco.pRX->iPacket_r, count);
			t1 = sched_clock();
			BTCVSD_read_wait_queue_flag = 0;
			ret = wait_event_interruptible_timeout(BTCVSD_Read_Wait_Queue,
							       BTCVSD_read_wait_queue_flag,
							       nsecs_to_jiffies(read_timeout_limit));
			t2 = sched_clock();
			PRINTK_AUDDRV("AudDrv_btcvsd_read WAKEUP...count=%zu\n", count);
			t2 = t2 - t1;	/* in ns (10^9) */

			PRINTK_AUDDRV("%s(), WAKEUP...wait event interrupt, ret = %d, flag = %d\n",
			      __func__,
			      ret,
			      BTCVSD_read_wait_queue_flag);

			if (t2 > read_timeout_limit) {
				pr_warn("%s timeout, %llu ns, timeout_limit %llu, ret %d, flag %d\n",
					__func__,
					t2, read_timeout_limit,
					ret,
					BTCVSD_read_wait_queue_flag);
			}

			if (ret < 0) {
				/* error, -ERESTARTSYS if it was interrupted by a signal */
				max_timeout_trial--;
				pr_err("%s(), error, trial left %d, read_count %zd\n",
				       __func__,
				       max_timeout_trial,
				       read_count);

				if (max_timeout_trial <= 0)
					return read_count;
			} else if (ret == 0) {
				/* conidtion is false after timeout */
				max_timeout_trial--;
				pr_err("%s(), error, timeout, condition is false, trial left %d, read_count %zd\n",
				       __func__,
				       max_timeout_trial,
				       read_count);

				if (max_timeout_trial <= 0)
					return read_count;
			} else if (ret == 1) {
				/* condition is true after timeout */
				pr_debug("%s(), timeout, condition is true\n", __func__);
			} else {
				pr_debug("%s(), condition is true before timeout\n", __func__);
			}

		}
	}
	PRINTK_AUDDRV("AudDrv_btcvsd_read read_count = %zu,read_timeout_limit=%llu\n",
		      read_count, read_timeout_limit);
	return read_count;
}


/**************************************************************************
 * STRUCT
 *  File Operations and misc device
 *
 **************************************************************************/

static const struct file_operations AudDrv_btcvsd_fops = {
	.owner = THIS_MODULE,
	.open = AudDrv_btcvsd_open,
	.unlocked_ioctl = AudDrv_btcvsd_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = AudDrv_btcvsd_compat_ioctl,
#endif
	.write = AudDrv_btcvsd_write,
	.read = AudDrv_btcvsd_read,
};

static struct miscdevice AudDrv_btcvsd_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ebc",
	.fops = &AudDrv_btcvsd_fops,
};

/***************************************************************************
 * FUNCTION
 *  AudDrv_btcvsd_mod_init / AudDrv_btcvsd_mod_exit
 *
 * DESCRIPTION
 *  Module init and de-init (only be called when system boot up)
 *
 **************************************************************************/

#ifdef CONFIG_OF
static const struct of_device_id audio_bt_cvsd_of_ids[] = {
	{.compatible = "mediatek,audio_bt_cvsd",},
	{}
};
#endif

static struct platform_driver AudDrv_btcvsd = {
	.probe = AudDrv_btcvsd_probe,
	.driver = {
		   .name = auddrv_btcvsd_name,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = audio_bt_cvsd_of_ids,
#endif
		   },
};

#ifndef CONFIG_OF
static struct platform_device *mtk_btcvsd_dev;
#endif

static int AudDrv_btcvsd_mod_init(void)
{
	int ret = 0;

	pr_debug("+AudDrv_btcvsd_mod_init\n");

#ifndef CONFIG_OF
	mtk_btcvsd_dev = platform_device_alloc(auddrv_btcvsd_name, -1);
	if (!mtk_btcvsd_dev) {
		pr_debug("-AudDrv_btcvsd_mod_init, platform_device_alloc() fail, return\n");
		return -ENOMEM;
	}

	ret = platform_device_add(mtk_btcvsd_dev);
	if (ret != 0) {
		pr_debug("-AudDrv_btcvsd_mod_init, platform_device_add() fail, return\n");
		platform_device_put(mtk_btcvsd_dev);
		return ret;
	}
#endif

	/* Register platform DRIVER */
	ret = platform_driver_register(&AudDrv_btcvsd);
	if (ret) {
		pr_debug("AudDrv Fail:%d - Register DRIVER\n", ret);
		return ret;
	}
	/* register MISC device */
	if (ret == misc_register(&AudDrv_btcvsd_device)) {
		pr_debug("AudDrv_btcvsd_mod_init misc_register Fail:%d\n", ret);
		return ret;
	}

	pr_debug("-AudDrv_btcvsd_mod_init\n");
	return 0;
}

static void AudDrv_btcvsd_mod_exit(void)
{
	PRINTK_AUDDRV("+AudDrv_btcvsd_mod_exit\n");

	AudDrv_btcvsd_Free_Buffer(NULL, 0);
	AudDrv_btcvsd_Free_Buffer(NULL, 1);
	/*
	   remove_proc_entry("audio", NULL);
	   platform_driver_unregister(&AudDrv_btcvsd);
	 */

	PRINTK_AUDDRV("-AudDrv_btcvsd_mod_exit\n");
}


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(AUDDRV_BTCVSD_NAME);
MODULE_AUTHOR(AUDDRV_AUTHOR);

module_init(AudDrv_btcvsd_mod_init);
module_exit(AudDrv_btcvsd_mod_exit);
