// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched/clock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#include <linux/clk.h>

#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "modem_sys.h"
#include "ccci_bm.h"
#include "ccci_hif_cldma.h"
#include "md_sys1_platform.h"
#include "cldma_reg.h"
#include "modem_reg_base.h"
#include "ccci_fsm.h"
#include "ccci_port.h"
#include "ccci_cldma_plat.h"
#include "ccci_platform.h"
#include "ccci_hif_cldma.h"

#define TAG "cldma"

void cldma_plat_hw_reset(unsigned char md_id)
{
	unsigned int reg_value;
	//struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	CCCI_NORMAL_LOG(md_id, TAG, "%s:rst cldma\n", __func__);

	/* reset cldma hw: AO Domain */
	reg_value = regmap_read(cldma_ctrl->plat_val.infra_ao_base,
		INFRA_RST0_REG_AO, &reg_value);
	reg_value &= ~(CLDMA_AO_RST_MASK); /* the bits in reg is WO, */
	reg_value |= (CLDMA_AO_RST_MASK);/* so only this bit effective */
	regmap_write(cldma_ctrl->plat_val.infra_ao_base,
		INFRA_RST0_REG_AO, reg_value);
	CCCI_BOOTUP_LOG(md_id, TAG, "%s:clear reset\n", __func__);

	/* reset cldma clr */
	reg_value = regmap_read(cldma_ctrl->plat_val.infra_ao_base,
		INFRA_RST1_REG_AO, &reg_value);
	reg_value &= ~(CLDMA_AO_RST_MASK);/* read no use, maybe a time delay */
	reg_value |= (CLDMA_AO_RST_MASK);
	regmap_write(cldma_ctrl->plat_val.infra_ao_base,
		INFRA_RST1_REG_AO, reg_value);
	CCCI_BOOTUP_LOG(md_id, TAG, "%s:done\n", __func__);

	/* reset cldma hw: PD Domain */
	reg_value = regmap_read(cldma_ctrl->plat_val.infra_ao_base,
		INFRA_RST0_REG_PD, &reg_value);
	reg_value &= ~(CLDMA_PD_RST_MASK);
	reg_value |= (CLDMA_PD_RST_MASK);
	regmap_write(cldma_ctrl->plat_val.infra_ao_base,
		INFRA_RST0_REG_PD, reg_value);
	CCCI_BOOTUP_LOG(md_id, TAG, "%s:clear reset\n", __func__);

	/* reset cldma clr */
	reg_value = regmap_read(cldma_ctrl->plat_val.infra_ao_base,
		INFRA_RST1_REG_PD, &reg_value);
	reg_value &= ~(CLDMA_PD_RST_MASK);
	reg_value |= (CLDMA_PD_RST_MASK);
	regmap_write(cldma_ctrl->plat_val.infra_ao_base,
		INFRA_RST1_REG_PD, reg_value);
	CCCI_DEBUG_LOG(md_id, TAG, "%s:done\n", __func__);

	/* set cldma wakeup source mask */
	reg_value = regmap_read(cldma_ctrl->plat_val.infra_ao_base,
		INFRA_CLDMA_CTRL_REG, &reg_value);
	reg_value |= (CLDMA_IP_BUSY_MASK);
	regmap_write(cldma_ctrl->plat_val.infra_ao_base,
		INFRA_CLDMA_CTRL_REG, reg_value);
	CCCI_DEBUG_LOG(md_id, TAG, "set cldma ctrl reg as:0x%x\n", reg_value);
}


void cldma_plat_set_clk_cg(unsigned char md_id, unsigned int on)
{
	int idx = 0;
	int ret = 0;

	CCCI_NORMAL_LOG(md_id, TAG, "%s: on=%d\n", __func__, on);

	for (idx = 0; idx < CLDMA_CLOCK_COUNT; idx++) {
		if (cldma_clk_table[idx].clk_ref == NULL)
			continue;

		if (on) {
			ret = clk_prepare_enable(cldma_clk_table[idx].clk_ref);
			if (ret)
				CCCI_ERROR_LOG(md_id, TAG,
					"%s: on=%d,ret=%d\n",
					__func__, on, ret);
			devapc_check_flag = 1;

		} else {
			devapc_check_flag = 0;
			clk_disable_unprepare(cldma_clk_table[idx].clk_ref);
		}
	}
}

int cldma_plat_suspend(unsigned char md_id)
{
	CCCI_NORMAL_LOG(md_id, TAG, "[%s]\n", __func__);

	return 0;
}

void cldma_plat_resume(unsigned char md_id)
{
	//struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	//enum MD_STATE md_state = ccci_fsm_get_md_state(md->index);
	int i;
	unsigned long flags;
	unsigned int val = 0;
	dma_addr_t bk_addr = 0;

	CCCI_NORMAL_LOG(md_id, TAG, "%s\n", __func__);

//	if (md_state == GATED ||
//			md_state == WAITING_TO_STOP ||
//			md_state == INVALID) {
//		CCCI_NORMAL_LOG(md->index, TAG,
//			"Resume no need reset cldma for md_state=%d\n"
//			, md_state);
//		return;
//	}

	if (cldma_ctrl->cldma_state != HIF_CLDMA_STATE_PWRON) {
		CCCI_NORMAL_LOG(md_id, TAG,
			"Resume no need reset cldma for md_state=%d\n",
			cldma_ctrl->cldma_state);
		return;
	}

	//cldma_write32(md_info->ap_ccif_base,
	//	APCCIF_CON, 0x01);	/* arbitration */

	if (cldma_read32(cldma_ctrl->cldma_ap_pdn_base, CLDMA_AP_TQSAR(0))
		|| cldma_reg_get_4msb_val(cldma_ctrl->cldma_ap_ao_base,
		CLDMA_AP_UL_START_ADDR_4MSB, cldma_ctrl->txq[0].index)) {
		CCCI_NORMAL_LOG(md_id, TAG,
			"Resume cldma pdn register: No need  ...\n");
		spin_lock_irqsave(&cldma_ctrl->cldma_timeout_lock, flags);
		if (!(cldma_read32(cldma_ctrl->cldma_ap_ao_base,
			CLDMA_AP_SO_STATUS))) {
			cldma_write32(cldma_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_SO_RESUME_CMD,
				CLDMA_BM_ALL_QUEUE & 0x1);
			cldma_read32(cldma_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_SO_RESUME_CMD); /* dummy read */
		} else
			CCCI_NORMAL_LOG(md_id, TAG,
				"Resume cldma ao register: No need  ...\n");
		spin_unlock_irqrestore(&cldma_ctrl->cldma_timeout_lock, flags);
	} else {
		CCCI_NORMAL_LOG(md_id, TAG,
			"Resume cldma pdn register ...11\n");
		spin_lock_irqsave(&cldma_ctrl->cldma_timeout_lock, flags);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		/* re-config 8G mode flag for pd register*/
		cldma_write32(cldma_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_CFG,
		cldma_read32(cldma_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_UL_CFG) | 0x40);
#endif
		cldma_write32(cldma_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_SO_RESUME_CMD, CLDMA_BM_ALL_QUEUE & 0x1);
		cldma_read32(cldma_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_SO_RESUME_CMD); /* dummy read */

		/* set start address */
		for (i = 0; i < QUEUE_LEN(cldma_ctrl->txq); i++) {
			if (cldma_read32(cldma_ctrl->cldma_ap_ao_base,
				CLDMA_AP_TQCPBAK(cldma_ctrl->txq[i].index)) == 0
				&& cldma_reg_get_4msb_val(
						cldma_ctrl->cldma_ap_ao_base,
					CLDMA_AP_UL_CURRENT_ADDR_BK_4MSB,
					cldma_ctrl->txq[i].index) == 0) {
				if (i != 7) /* Queue 7 not used currently */
					CCCI_DEBUG_LOG(md_id, TAG,
					"Resume CH(%d) current bak:== 0\n", i);
				cldma_reg_set_tx_start_addr(
						cldma_ctrl->cldma_ap_pdn_base,
						cldma_ctrl->txq[i].index,
						cldma_ctrl->txq[i].tr_done->gpd_addr);
				cldma_reg_set_tx_start_addr_bk(
						cldma_ctrl->cldma_ap_ao_base,
						cldma_ctrl->txq[i].index,
						cldma_ctrl->txq[i].tr_done->gpd_addr);
			} else {
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
				val = cldma_reg_get_4msb_val(
					cldma_ctrl->cldma_ap_ao_base,
					CLDMA_AP_UL_CURRENT_ADDR_BK_4MSB,
					cldma_ctrl->txq[i].index);
				/*set high bits*/
				bk_addr = val;
				bk_addr <<= 32;
#else
				bk_addr = 0;
#endif
				/*set low bits*/
				val = cldma_read32(cldma_ctrl->cldma_ap_ao_base,
				 CLDMA_AP_TQCPBAK(cldma_ctrl->txq[i].index));
				bk_addr |= val;
				cldma_reg_set_tx_start_addr(
					cldma_ctrl->cldma_ap_pdn_base,
					cldma_ctrl->txq[i].index, bk_addr);
				cldma_reg_set_tx_start_addr_bk(
					cldma_ctrl->cldma_ap_ao_base,
					cldma_ctrl->txq[i].index, bk_addr);
			}
		}
		/* wait write done*/
		wmb();
		/* start all Tx and Rx queues */
		cldma_ctrl->txq_started = 0;
		cldma_ctrl->txq_active |= CLDMA_BM_ALL_QUEUE;

		ccci_write32(cldma_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMCR0,
			CLDMA_TX_INT_DONE |
			CLDMA_TX_INT_QUEUE_EMPTY |
			CLDMA_TX_INT_ERROR);
		/* enable all L3 interrupts */
		cldma_write32(cldma_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_L3TIMCR0, CLDMA_BM_INT_ALL);
		cldma_write32(cldma_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_L3TIMCR1, CLDMA_BM_INT_ALL);
		cldma_write32(cldma_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_L3RIMCR0, CLDMA_BM_INT_ALL);
		cldma_write32(cldma_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_L3RIMCR1, CLDMA_BM_INT_ALL);
		spin_unlock_irqrestore(&cldma_ctrl->cldma_timeout_lock, flags);
		CCCI_NORMAL_LOG(md_id, TAG,
			"Resume cldma pdn register done\n");
	}
}

