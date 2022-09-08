// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <mtk_vcodec_intr.h>
#include "mtk_vcodec_drv.h"
#include "val_types_private.h"
#include "hal_types_private.h"
#include "val_api_private.h"
#include "drv_api.h"
#include "videocodec_kernel_driver.h"

void dec_isr(struct mtk_vcodec_dev *dev)
{
	enum VAL_RESULT_T eValRet;
	unsigned long ulFlags, ulFlagsISR, ulFlagsLockHW;

	unsigned int u4TempDecISRCount = 0;
	unsigned int u4TempLockDecHWCount = 0;
	unsigned int u4DecDoneStatus = 0;
	void __iomem *vdec_misc = dev->dec_reg_base[VDEC_BASE] + 0x5000;
	void __iomem *vdec_misc_addr = vdec_misc + MTK_VDEC_IRQ_CFG_REG;

	u4DecDoneStatus = VDO_HW_READ(vdec_misc_addr + MTK_VDEC_IRQ_CFG_REG);
	if ((u4DecDoneStatus & (0x1 << 16)) != 0x10000) {
		pr_info("[VDEC] DEC ISR, Dec done is not 0x1 (0x%08x)",
				u4DecDoneStatus);
		return;
	}


	spin_lock_irqsave(&gDrvInitParams->decISRCountLock, ulFlagsISR);
	gu4DecISRCount++;
	u4TempDecISRCount = gu4DecISRCount;
	spin_unlock_irqrestore(&gDrvInitParams->decISRCountLock, ulFlagsISR);

	spin_lock_irqsave(&gDrvInitParams->lockDecHWCountLock, ulFlagsLockHW);
	u4TempLockDecHWCount = gu4LockDecHWCount;
	spin_unlock_irqrestore(&gDrvInitParams->lockDecHWCountLock, ulFlagsLockHW);

	if (u4TempDecISRCount != u4TempLockDecHWCount) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 * pr_info("[INFO] Dec ISRCount: 0x%x, "
		 * "LockHWCount:0x%x\n",
		 * u4TempDecISRCount, u4TempLockDecHWCount);
		 */
	}

	/* Clear interrupt */
	VDO_HW_WRITE(vdec_misc_addr+41*4,
		VDO_HW_READ(vdec_misc_addr + 41*4) | MTK_VDEC_IRQ_CFG);
	VDO_HW_WRITE(vdec_misc_addr+41*4,
		VDO_HW_READ(vdec_misc_addr + 41*4) & ~MTK_VDEC_IRQ_CLR);


	spin_lock_irqsave(&gDrvInitParams->decIsrLock, ulFlags);
	eValRet = eVideoSetEvent(&gDrvInitParams->DecIsrEvent, sizeof(struct VAL_EVENT_T));
	if (eValRet != VAL_RESULT_NO_ERROR) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		pr_info("[VDEC] ISR set DecIsrEvent error\n");
	}
	spin_unlock_irqrestore(&gDrvInitParams->decIsrLock, ulFlags);
}


void enc_isr(struct mtk_vcodec_dev *dev)
{
	enum VAL_RESULT_T eValRet;
	unsigned long ulFlagsISR, ulFlagsLockHW;


	unsigned int u4TempEncISRCount = 0;
	unsigned int u4TempLockEncHWCount = 0;
	void __iomem *addr;
	/* ---------------------- */

	addr = dev->enc_reg_base[VENC_BASE] + MTK_VENC_IRQ_ACK_OFFSET;

	spin_lock_irqsave(&gDrvInitParams->encISRCountLock, ulFlagsISR);
	gu4EncISRCount++;
	u4TempEncISRCount = gu4EncISRCount;
	spin_unlock_irqrestore(&gDrvInitParams->encISRCountLock, ulFlagsISR);

	spin_lock_irqsave(&gDrvInitParams->lockEncHWCountLock, ulFlagsLockHW);
	u4TempLockEncHWCount = gu4LockEncHWCount;
	spin_unlock_irqrestore(&gDrvInitParams->lockEncHWCountLock, ulFlagsLockHW);

	if (u4TempEncISRCount != u4TempLockEncHWCount) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 * pr_info("[INFO] Enc ISRCount: 0x%xa, "
		 *	"LockHWCount:0x%x\n",
		 * u4TempEncISRCount, u4TempLockEncHWCount);
		 */
	}

	if (CodecHWLock.pvHandle == 0) {
		pr_info("[VENC] NO one Lock Enc HW, please check!!\n");

		/* Clear all status */
		/* VDO_HW_WRITE(KVA_VENC_MP4_IRQ_ACK_ADDR, 1); */
		VDO_HW_WRITE(addr, VENC_IRQ_STATUS_PAUSE);
		/* VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
		 * VENC_IRQ_STATUS_DRAM_VP8);
		 */
		VDO_HW_WRITE(addr, VENC_IRQ_STATUS_SWITCH);
		VDO_HW_WRITE(addr, VENC_IRQ_STATUS_DRAM);
		VDO_HW_WRITE(addr, VENC_IRQ_STATUS_SPS);
		VDO_HW_WRITE(addr, VENC_IRQ_STATUS_PPS);
		VDO_HW_WRITE(addr, VENC_IRQ_STATUS_FRM);
		return;
	}

	if (CodecHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC) {/* hardwire */
		gu4HwVencIrqStatus = VDO_HW_READ(dev->enc_reg_base[VENC_BASE]
					+ (MTK_VENC_IRQ_STATUS_OFFSET));
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PAUSE) {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
			VDO_HW_WRITE(addr,
					VENC_IRQ_STATUS_PAUSE);
		}
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SWITCH) {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
			VDO_HW_WRITE(addr,
					VENC_IRQ_STATUS_SWITCH);
		}
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_DRAM) {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
			VDO_HW_WRITE(addr,
					VENC_IRQ_STATUS_DRAM);
		}
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SPS) {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
			VDO_HW_WRITE(addr,
					VENC_IRQ_STATUS_SPS);
		}
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PPS) {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
			VDO_HW_WRITE(addr,
					VENC_IRQ_STATUS_PPS);
		}
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_FRM) {
			/* Add one line comment for avoid kernel coding style,
			 * WARNING:BRACES:
			 */
			VDO_HW_WRITE(addr,
					VENC_IRQ_STATUS_FRM);
		}
	} else {
		pr_info("[VENC] Invalid lock holder driver type = %d\n",
				CodecHWLock.eDriverType);
	}

	eValRet = eVideoSetEvent(&gDrvInitParams->EncIsrEvent, sizeof(struct VAL_EVENT_T));
	if (eValRet != VAL_RESULT_NO_ERROR) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		pr_info("[VENC] ISR set EncIsrEvent error\n");
	}
}
irqreturn_t video_intr_dlr(int irq, void *priv)
{
	struct mtk_vcodec_dev *dev = priv;

	dec_isr(dev);
	return IRQ_HANDLED;
}

irqreturn_t video_intr_dlr2(int irq, void *priv)
{
	struct mtk_vcodec_dev *dev = priv;

	enc_isr(dev);
	return IRQ_HANDLED;
}

int mtk_vcodec_irq_setup(struct platform_device *pdev, struct mtk_vcodec_dev *dev)
{
	if (request_irq(dev->dec_irq, (irq_handler_t)video_intr_dlr,
			IRQF_TRIGGER_HIGH, VCODEC_DEVNAME, NULL) < 0) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		pr_info("[VCODEC][ERROR] error to request dec irq\n");
	} else {
		pr_debug("[VCODEC] success to request dec irq: %d\n",
				dev->dec_irq);
	}

	if (request_irq(dev->enc_irq, (irq_handler_t)video_intr_dlr2,
			IRQF_TRIGGER_HIGH, VCODEC_DEVNAME, NULL) < 0) {
		/* Add one line comment for avoid kernel coding style,
		 * WARNING:BRACES:
		 */
		pr_debug("[VCODEC][ERROR] error to request enc irq\n");
	} else {
		pr_debug("[VCODEC] success to request enc irq: %d\n",
				dev->enc_irq);
	}

	disable_irq(dev->dec_irq);
	disable_irq(dev->enc_irq);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_irq_setup);

MODULE_LICENSE("GPL v2");
