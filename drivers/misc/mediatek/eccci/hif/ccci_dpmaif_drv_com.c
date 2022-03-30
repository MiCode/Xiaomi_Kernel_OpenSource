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
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>

#include "ccci_dpmaif_com.h"



#define TAG "drv"

struct dpmaif_plat_drv g_plat_drv;
struct dpmaif_plat_ops g_plat_ops;


void ccci_drv_set_dl_interrupt_mask(unsigned int mask)
{
	unsigned int value;

	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);

	value &= ~(DPMAIF_AO_DL_ISR_MSK);
	value |= ((mask) & DPMAIF_AO_DL_ISR_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

void ccci_drv_dl_set_pit_base_addr(dma_addr_t base_addr)
{
	unsigned int value;

	/* 2.4 bit 31~0: pit base addr low 32bits, curr: lb_addr */
	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON0, (unsigned int)base_addr);

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON1);

	value &= ~(DPMAIF_PIT_ADDRH_MSK);
	/* ((base_addr >> 32) << 24) = (base_addr >> 8) */
	value |= ((base_addr >> 8) & DPMAIF_PIT_ADDRH_MSK);

	/* 2.4 bit 31~24: pit base addr high 8bits, curr: hb_addr */
	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON1, value);
}

void ccci_drv_dl_set_pit_size(unsigned int size)
{
	unsigned int value;

	/* 2.6 bit 15~0: pit count, unit: 12byte one pit,
	 * curr: DPMAIF_DL_PIT_ENTRY_SIZE 256
	 */
	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON1);

	value &= ~(drv.pit_size_msk);
	value |= (size & drv.pit_size_msk);

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON1, value);
}

void ccci_drv_dl_pit_en(bool enable)
{
	unsigned int value;

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON3);

	if (enable == true)
		value |= DPMAIF_PIT_EN_MSK;
	else
		value &= ~DPMAIF_PIT_EN_MSK;

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON3, value);
}

void ccci_drv_dl_pit_init_done(void)
{
	unsigned int dl_pit_init = 0;
	int count = 0;

	dl_pit_init |= DPMAIF_DL_PIT_INIT_ALLSET;
	dl_pit_init |= DPMAIF_DL_PIT_INIT_EN;

	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) &
				DPMAIF_DL_PIT_INIT_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT, dl_pit_init);
			break;
		}
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "[%s] error: 1st fail\n", __func__);
			//dpmaif_ctrl->ops->dump_status(
			//	DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return;
		}
	}

	count = 0;
	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) & DPMAIF_DL_PIT_INIT_NOT_READY)
			== DPMAIF_DL_PIT_INIT_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "[%s] error: 2nd fail\n", __func__);
			//dpmaif_ctrl->ops->dump_status(
			//	DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return;
		}
	}
}

void ccci_drv_dl_set_bat_base_addr(dma_addr_t base_addr)
{
	unsigned int value;

	/* 2.3 bit 31~0: bat base addr low 32bits, curr: lb_addr */
	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON0, (unsigned int)base_addr);

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1);

	value &= ~(DPMAIF_BAT_ADDRH_MSK);
	/* ((base_addr >> 32) << 24) = (base_addr >> 8) */
	value |= ((base_addr >> 8) & DPMAIF_BAT_ADDRH_MSK);
	/* 2.3 bit 31~24: bat base addr high 8bits, curr: hb_addr */
	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1, value);
}

void ccci_drv_dl_set_bat_size(unsigned int size)
{
	unsigned int value;

	/* 2.5 bit 15~0: bat count, unit:8byte one BAT,
	 * curr: DPMAIF_DL_BAT_ENTRY_SIZE 128
	 */
	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1);

	value &= ~(DPMAIF_BAT_SIZE_MSK);
	value |= (size & DPMAIF_BAT_SIZE_MSK);

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1, value);
}

void ccci_drv_dl_bat_en(bool enable)
{
	unsigned int value;

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1);

	if (enable == true)
		value |= DPMAIF_BAT_EN_MSK;
	else
		value &= ~DPMAIF_BAT_EN_MSK;

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1, value);
}

int ccci_drv_dl_bat_init_done(bool frg_en)
{
	unsigned int dl_bat_init = 0;
	int count = 0;

	if (frg_en == true)
		dl_bat_init |= DPMAIF_DL_BAT_FRG_INIT;

	/* update  all bat settings. */
	dl_bat_init |= DPMAIF_DL_BAT_INIT_ALLSET;
	dl_bat_init |= DPMAIF_DL_BAT_INIT_EN;

	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) & DPMAIF_DL_BAT_INIT_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT, dl_bat_init);
			break;
		}
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "[%s] error: 1s fail\n", __func__);
			//dpmaif_ctrl->ops->dump_status(
			//	DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	count = 0;
	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) & DPMAIF_DL_BAT_INIT_NOT_READY)
			== DPMAIF_DL_BAT_INIT_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "[%s] error: 2nd fail\n", __func__);
			//dpmaif_ctrl->ops->dump_status(
			//	DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	return 0;
}

int ccci_drv_dl_add_frg_bat_cnt(unsigned short frg_entry_cnt)
{
	unsigned int dl_bat_update;
	int count = 0;

	dl_bat_update = (frg_entry_cnt & 0xFFFF);
	dl_bat_update |= (DPMAIF_DL_ADD_UPDATE|DPMAIF_DL_BAT_FRG_ADD);

	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_ADD) & DPMAIF_DL_ADD_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_ADD, dl_bat_update);
			break;
		}

		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "[%s] error: cost too long\n", __func__);
			//dpmaif_ctrl->ops->dump_status(
			//	DPMAIF_HIF_ID, DUMP_FLAG_REG, -1);
			return HW_REG_TIME_OUT;
		}
	}

	return 0;
}

int ccci_drv_dl_add_bat_cnt(unsigned short bat_entry_cnt)
{
	unsigned int dl_bat_update;
	int count = 0;

	dl_bat_update = (bat_entry_cnt & 0xFFFF);
	dl_bat_update |= DPMAIF_DL_ADD_UPDATE;

	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_ADD) & DPMAIF_DL_ADD_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_ADD, dl_bat_update);
			break;
		}

		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "[%s] error: cost too long\n", __func__);
			//dpmaif_ctrl->ops->dump_status(
			//	DPMAIF_HIF_ID, DUMP_FLAG_REG, -1);
			return HW_REG_TIME_OUT;
		}
	}

	return 0;
}

int ccci_drv_dl_all_frg_queue_en(bool enable)
{
	unsigned long dl_bat_init = 0;
	unsigned long value;
	int count = 0;

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1);

	if (enable == true)
		value |= DPMAIF_BAT_EN_MSK;
	else
		value &= ~DPMAIF_BAT_EN_MSK;

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1, value);

	/* only update bat_en bit. */
	dl_bat_init |= DPMAIF_DL_BAT_INIT_ONLY_ENABLE_BIT;
	dl_bat_init |= (DPMAIF_DL_BAT_INIT_EN|DPMAIF_DL_BAT_FRG_INIT);

	/*update DL bat setting to HW*/
	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) &
				DPMAIF_DL_BAT_INIT_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT, dl_bat_init);
			break;
		}

		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "[%s] error: 1st fail\n", __func__);
			//dpmaif_ctrl->ops->dump_status(
			//	DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	count = 0;
	/*wait HW updating*/
	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) &
		DPMAIF_DL_BAT_INIT_NOT_READY) == DPMAIF_DL_BAT_INIT_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "[%s] error: 2nd fail\n", __func__);
			//dpmaif_ctrl->ops->dump_status(
			//	DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	return 0;
}

void ccci_drv_clear_ip_busy(void)
{
	if (DPMA_READ_PD_MISC(DPMAIF_PD_AP_IP_BUSY))
		DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_IP_BUSY, DPMA_READ_PD_MISC(DPMAIF_PD_AP_IP_BUSY));
}

int ccci_drv_dl_add_pit_remain_cnt(unsigned short pit_remain_cnt)
{
	unsigned int dl_update;
	int count = 0;

	if (g_dpmf_ver == 3)
		dl_update = (pit_remain_cnt & 0x0003FFFF);
	else
		dl_update = (pit_remain_cnt & 0x0000FFFF);

	dl_update |= DPMAIF_DL_ADD_UPDATE;

	while (1) {
		if ((DPMA_READ_PD_DL(NRL2_DPMAIF_DL_PIT_ADD) &
				DPMAIF_DL_ADD_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_PIT_ADD, dl_update);
			break;
		}
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: 1st DPMAIF_PD_DL_PIT_ADD read fail\n", __func__);
			//dpmaif_ctrl->ops->dump_status(
			//	DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	count = 0;
	while ((DPMA_READ_PD_DL(NRL2_DPMAIF_DL_PIT_ADD) &
			DPMAIF_DL_ADD_NOT_READY) == DPMAIF_DL_ADD_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				 "[%s] error: 2nd DPMAIF_PD_DL_PIT_ADD read fail\n", __func__);
			//dpmaif_ctrl->ops->dump_status(
			//	DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	return 0;
}

int ccci_drv_ul_add_wcnt(unsigned char q_num, unsigned short drb_wcnt)
{
	unsigned int ul_update;
	int count = 0;

	ul_update = (drb_wcnt & 0x0000ffff);
	ul_update |= DPMAIF_UL_ADD_UPDATE;

	while (1) {
		if ((DPMA_READ_PD_UL(DPMAIF_ULQ_ADD_DESC_CH_n(q_num)) &
				DPMAIF_UL_ADD_NOT_READY) == 0) {
			DPMA_WRITE_PD_UL(DPMAIF_ULQ_ADD_DESC_CH_n(q_num), ul_update);
			break;
		}

		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: drb_add rdy poll fail: 0x%x\n",
				__func__, DPMA_READ_PD_UL(NRL2_DPMAIF_UL_DBG_STA2));
			//dpmaif_ctrl->ops->dump_status(DPMAIF_HIF_ID,
			//	DUMP_FLAG_REG, NULL, -1);
			return HW_REG_CHK_FAIL;
		}
	}

	count = 0;
	while ((DPMA_READ_PD_UL(DPMAIF_ULQ_ADD_DESC_CH_n(q_num)) &
			DPMAIF_UL_ADD_NOT_READY) == DPMAIF_UL_ADD_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: drb_add fail: 0x%x\n",
				__func__, DPMA_READ_PD_UL(NRL2_DPMAIF_UL_DBG_STA2));
			break;
		}
	}

	return 0;
}

int ccci_drv_dl_all_queue_en(bool enable)
{
	unsigned long dl_bat_init = 0;
	unsigned long value;
	int count = 0;

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1);

	if (enable == true)
		value |= DPMAIF_BAT_EN_MSK;
	else
		value &= ~DPMAIF_BAT_EN_MSK;

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1, value);

	/* only update bat_en bit. */
	dl_bat_init |= DPMAIF_DL_BAT_INIT_ONLY_ENABLE_BIT;
	dl_bat_init |= DPMAIF_DL_BAT_INIT_EN;

	/*update DL bat setting to HW*/
	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) &
				DPMAIF_DL_BAT_INIT_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT, dl_bat_init);
			break;
		}

		count++;
		if (count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] 1st DPMAIF_PD_DL_BAT_INIT read/write fail\n",
				__func__);
			//dpmaif_ctrl->ops->dump_status(
			//	DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	/*wait HW updating*/
	count = 0;
	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) &
		DPMAIF_DL_BAT_INIT_NOT_READY) == DPMAIF_DL_BAT_INIT_NOT_READY) {

		count++;
		if (count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] 2nd DPMAIF_PD_DL_BAT_INIT read fail\n",
				__func__);
			//dpmaif_ctrl->ops->dump_status(
			//	DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	return 0;
}

unsigned int ccci_drv_dl_idle_check(void)
{
	unsigned int ret;

	ret = (DPMA_READ_PD_DL(NRL2_DPMAIF_DL_DBG_STA1) & drv.dl_idle_sts);

	if (ret == drv.dl_idle_sts)
		return 0; /* idle */
	else
		return 1;
}
