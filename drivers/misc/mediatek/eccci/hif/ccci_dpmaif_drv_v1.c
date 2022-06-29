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
#include "ccci_dpmaif_reg_v1.h"

#define TAG "drv1"


static struct dpmaif_clk_node g_clk_tbs[] = {
	{ NULL, "infra-dpmaif-clk"},
	{ NULL, NULL},
};



/* =======================================================
 *
 * Descriptions: State part (1/3): Init(ISR)
 *
 * ========================================================
 */
static int drv1_intr_hw_init(void)
{
	int count = 0;

	/* UL/TX interrupt init */
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, 0xFFFFFFFF);
	/* 2. set interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TICR0, AP_UL_L2INTR_En_Msk);
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISR0, ~(AP_UL_L2INTR_En_Msk));

	/* 3. check mask sts*/
	count = 0;
	while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
			AP_UL_L2INTR_En_Msk) == AP_UL_L2INTR_En_Msk) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "[%s] error: 3rd fail\n", __func__);
			return HW_REG_TIME_OUT;
		}
	}

	/*Set DL/RX interrupt*/
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0, 0xFFFFFFFF);
	/* 2. clear interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMCR0, AP_DL_L2INTR_En_Msk);
	/*set interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMSR0, ~(AP_DL_L2INTR_En_Msk));

	ccci_drv_set_dl_interrupt_mask(~(AP_DL_L2INTR_En_Msk));

	/* 3. check mask sts*/
	count = 0;
	while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
			AP_DL_L2INTR_En_Msk) == AP_DL_L2INTR_En_Msk) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "[%s] error: 4th fail\n", __func__);
			return HW_REG_TIME_OUT;
		}
	}

	/* Set AP IP busy */
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_IP_BUSY, 0xFFFFFFFF);
	/* 2. set IP busy unmask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DLUL_IP_BUSY_MASK, 0);

	return 0;
}

void ccci_drv1_dl_set_pit_chknum(void)
{
	unsigned int value;
	unsigned int number = DPMAIF_HW_CHK_PIT_NUM;

	/* 2.1 bit 31~24: pit threadhold, < number will pit_err_irq, curr: 2 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2);

	value &= ~(DPMAIF_PIT_CHK_NUM_MSK);
	value |= ((number << 24) & DPMAIF_PIT_CHK_NUM_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2, value);
}

void ccci_drv1_dl_set_bat_chk_thres(void)
{
	unsigned int value;

	/* 2.2 bit 21~16: bat threadhold, < size will len_err_irq2, curr: 1 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);

	value &= ~(DPMAIF_BAT_CHECK_THRES_MSK);
	value |= ((DPMAIF_HW_CHK_BAT_NUM << 16) & DPMAIF_BAT_CHECK_THRES_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

void ccci_drv1_dl_set_ao_frag_check_thres(void)
{
	unsigned int value;

	/* 2.2 bit 21~16: bat threadhold, < size will len_err_irq2, curr: 1 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
	value &= ~(DPMAIF_FRG_CHECK_THRES_MSK);

	value |= ((DPMAIF_HW_CHK_FRG_NUM) & DPMAIF_FRG_CHECK_THRES_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES, value);
}

void ccci_drv3_ul_arb_en(unsigned char q_num, bool enable)
{
	unsigned int ul_arb_en;

	ul_arb_en = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0);

	if (enable == true)
		ul_arb_en |= (1<<(q_num+8));
	else
		ul_arb_en &= ~(1<<(q_num+8));

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0, ul_arb_en);
}

static void drv1_ul_arb_en(unsigned char q_num, bool enable)
{
	unsigned int ul_arb_en;

	ul_arb_en = DPMA_READ_PD_UL(DPMAIF_PD_UL_CHNL_ARB0);

	if (enable == true)
		ul_arb_en |= (1<<(q_num+8));
	else
		ul_arb_en &= ~(1<<(q_num+8));

	DPMA_WRITE_PD_UL(DPMAIF_PD_UL_CHNL_ARB0, ul_arb_en);
}

static void drv1_ul_update_drb_size(unsigned char q_num, unsigned int size)
{
	unsigned int old_size, set_size;

	/* 1. bit 15~0: DRB count, in word(4 bytes) curr: 512*8/4 */
	set_size = size/4;
	old_size = DPMA_READ_PD_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));

	old_size &= ~DPMAIF_DRB_SIZE_MSK;
	old_size |= (set_size & DPMAIF_DRB_SIZE_MSK);

	DPMA_WRITE_PD_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num), old_size);
}

static void drv1_ul_update_drb_base_addr(unsigned char q_num,
	unsigned int lb_addr, unsigned int hb_addr)
{
	unsigned int old_addr;

	/* 2 bit 31~0: drb base addr low 32bits, curr: lb_addr */
	DPMA_WRITE_PD_UL(DPMAIF_ULQSAR_n(q_num), lb_addr);

	old_addr = DPMA_READ_PD_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));

	old_addr &= ~DPMAIF_DRB_ADDRH_MSK;
	old_addr |= ((hb_addr<<24) & DPMAIF_DRB_ADDRH_MSK);

	/* 2. bit 31~24: drb base addr high 8bits, curr: hb_addr */
	DPMA_WRITE_PD_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num), old_addr);
}

static void drv1_ul_rdy_en(unsigned char q_num, bool ready)
{
	unsigned int ul_rdy_en;

	ul_rdy_en = DPMA_READ_PD_UL(DPMAIF_PD_UL_CHNL_ARB0);

	if (ready == true)
		ul_rdy_en |= (1<<q_num);
	else
		ul_rdy_en &= ~(1<<q_num);

	DPMA_WRITE_PD_UL(DPMAIF_PD_UL_CHNL_ARB0, ul_rdy_en);
}

static unsigned int drv1_ul_get_rwidx(unsigned char q_num)
{
	return DPMA_READ_AO_UL(DPMAIF_ULQ_STA0_n(q_num));
}

unsigned int drv1_ul_get_rdidx(unsigned char q_num)
{
	return DPMA_READ_PD_UL(DPMAIF_ULQ_STA0_n(q_num)) & 0x0000FFFF;
}

void ccci_drv1_mask_ul_que_interrupt(unsigned char q_num)
{
	unsigned int ui_que_done_mask;

	ui_que_done_mask = DPMAIF_UL_INT_DONE(q_num) & DPMAIF_UL_INT_QDONE_MSK;

	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISR0, ui_que_done_mask);

	/* check mask sts */
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
	 * ui_que_done_mask)
	 * != ui_que_done_mask);
	 */
}

static inline void drv1_irq_tx_done(unsigned int tx_done_isr)
{
	int i;
	unsigned int intr_ul_que_done;
	struct dpmaif_tx_queue *txq;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		intr_ul_que_done = tx_done_isr & (1 << (i + UL_INT_DONE_OFFSET));
		if (intr_ul_que_done) {
			txq = &dpmaif_ctl->txq[i];

			if (atomic_read(&txq->txq_resume_done)) {
				atomic_set(&txq->txq_resume_done, 0);

				CCCI_NOTICE_LOG(0, TAG,
					"clear txq%d_resume_done: 0x%x, 0x%x, 0x%x\n",
					i, drv1_ul_get_rwidx(0),
					drv1_ul_get_rwidx(1),
					drv1_ul_get_rwidx(3));
			}

			ccci_drv1_mask_ul_que_interrupt(i);

			hrtimer_start(&txq->txq_done_timer,
					ktime_set(0, 500000), HRTIMER_MODE_REL);

		}
	}
}

static void drv1_set_dl_interrupt_mask(unsigned int mask)
{
	unsigned int value;

	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);

	value &= ~(DPMAIF_AO_DL_ISR_MSK);
	value |= ((mask) & DPMAIF_AO_DL_ISR_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

static void drv1_mask_dl_interrupt(void)
{
	unsigned int ui_que_done_mask;

	/* set mask register: bit1s */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMSR0, (DPMAIF_DL_INT_QDONE_MSK));

	ui_que_done_mask = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	ui_que_done_mask |= DPMAIF_DL_INT_QDONE_MSK;

	drv1_set_dl_interrupt_mask(ui_que_done_mask);
	/*check mask sts: should be 1s*/
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
	 * ui_que_done_mask) != ui_que_done_mask);
	 */
}

static void drv1_unmask_dl_interrupt(void)
{
	unsigned int ui_que_done_mask = DPMAIF_DL_INT_QDONE_MSK;

	/* set unmask/clear_mask register: bit0s */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMCR0, DPMAIF_DL_INT_QDONE_MSK);

	ui_que_done_mask = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	ui_que_done_mask &= ~DPMAIF_DL_INT_QDONE_MSK;

	drv1_set_dl_interrupt_mask(ui_que_done_mask);
	/*check mask sts: should be 0s */
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
	 * ui_que_done_mask) == ui_que_done_mask);
	 */
}

static void drv1_unmask_ul_interrupt(unsigned char q_num)
{
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TICR0,
		(DPMAIF_UL_INT_DONE(q_num) & DPMAIF_UL_INT_QDONE_MSK));

	/*check mask sts*/
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
	 *	ui_que_done_mask) == ui_que_done_mask);
	 */
}

static unsigned int drv1_dl_get_wridx(unsigned char q_num)
{
	return (DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA2) & DPMAIF_DL_PIT_WRIDX_MSK);
}

static unsigned int drv1_get_dl_interrupt_mask(void)
{
	return DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0);
}

static irqreturn_t drv1_isr(int irq, void *data)
{
	unsigned int L2RISAR0 = ccci_drv_get_dl_isr_event();
	unsigned int L2RIMR0  = drv1_get_dl_interrupt_mask();
	unsigned int L2TISAR0 = ccci_drv_get_ul_isr_event();
	unsigned int L2TIMR0  = DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0);

	/* clear IP busy register wake up cpu case */
	ccci_drv_clear_ip_busy();

	if (atomic_read(&dpmaif_ctl->wakeup_src) == 1)
		CCCI_NOTICE_LOG(0, TAG, "[%s] wake up by MD0 HIF L2(%x/%x)(%x/%x)!\n",
			__func__, L2TISAR0, L2TIMR0, L2RISAR0, L2RIMR0);

	/* TX interrupt */
	if (L2TISAR0) {
		L2TISAR0 &= ~(L2TIMR0);

		DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, L2TISAR0);

		/* this log may be printed frequently, so first cancel it*/
		if (L2TISAR0 & (DPMAIF_UL_INT_MD_NOTREADY_MSK|DPMAIF_UL_INT_MD_PWR_NOTREADY_MSK))
			CCCI_REPEAT_LOG(0, TAG, "[%s] dpmaif: ul info: L2(%x)\n",
				__func__, L2TISAR0);
		else if (L2TISAR0 & AP_UL_L2INTR_ERR_En_Msk)
			CCCI_ERROR_LOG(0, TAG, "[%s] dpmaif: ul error L2(%x)\n",
				__func__, L2TISAR0);

		/* tx done */
		if (L2TISAR0 & DPMAIF_UL_INT_QDONE_MSK)
			drv1_irq_tx_done(L2TISAR0 & DPMAIF_UL_INT_QDONE_MSK);
	}

	/* RX interrupt */
	if (L2RISAR0) {
		L2RISAR0 &= ~(L2RIMR0);

		if (L2RISAR0 & AP_DL_L2INTR_ERR_En_Msk)
			ccci_irq_rx_lenerr_handler(L2RISAR0 & AP_DL_L2INTR_ERR_En_Msk);

		/* ACK interrupt after lenerr_handler*/
		/* ACK RX interrupt */
		DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0, L2RISAR0);

		if (L2RISAR0 & DPMAIF_DL_INT_QDONE_MSK) {
			/* disable RX_DONE  interrupt */
			drv1_mask_dl_interrupt();

			/*always start work due to no napi*/
			/*for (i = 0; i < DPMAIF_HW_MAX_DLQ_NUM; i++)*/
			tasklet_hi_schedule(&dpmaif_ctl->rxq[0].rxq_task);
		}
	}

	return IRQ_HANDLED;
}

static void drv1_ul_all_queue_en(bool enable)
{
	unsigned long ul_arb_en;

	ul_arb_en = DPMA_READ_PD_UL(DPMAIF_PD_UL_CHNL_ARB0);

	if (enable == true)
		ul_arb_en |= DPMAIF_UL_ALL_QUE_ARB_EN;
	else
		ul_arb_en &= ~DPMAIF_UL_ALL_QUE_ARB_EN;

	DPMA_WRITE_PD_UL(DPMAIF_PD_UL_CHNL_ARB0, ul_arb_en);
}

static unsigned int drv1_ul_idle_check(void)
{
	unsigned long idle_sts;

	idle_sts = ((DPMA_READ_PD_UL(NRL2_DPMAIF_UL_DBG_STA2) >> DPMAIF_UL_STS_CUR_SHIFT)
		& DPMAIF_UL_IDLE_STS_MSK);

	if (idle_sts == DPMAIF_UL_IDLE_STS)
		return 0;
	else
		return 1;
}

static void drv1_hw_reset(void)
{
	unsigned int reg_value = 0;
	int count = 0, ret;

	/* pre- DPMAIF HW reset: bus-protect */
	regmap_write(dpmaif_ctl->infra_ao_base, INFRA_TOPAXI_PROTECTEN_1_SET,
			DPMAIF_SLEEP_PROTECT_CTRL);

	while (1) {
		ret = regmap_read(dpmaif_ctl->infra_ao_base,
				INFRA_TOPAXI_PROTECT_READY_STA1_1, &reg_value);
		if (ret) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s]-%d error: read infra_ao_base fail; ret=%d; value: 0x%x\n",
				__func__, __LINE__, ret, reg_value);
			continue;
		}
		if ((reg_value & (1 << 4)) == (1 << 4))
			break;

		udelay(1);

		count++;
		if (count >= 1000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: DPMAIF pre-reset timeout.\n",
				__func__);
			break;
		}
	}

	ret = regmap_read(dpmaif_ctl->infra_ao_base, INFRA_TOPAXI_PROTECTEN_1, &reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: read infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);

	CCCI_NORMAL_LOG(0, TAG, "[%s] infra_topaxi_protecten_1: 0x%x\n", __func__, reg_value);

	/* DPMAIF HW reset */
	/* reset dpmaif hw: AO Domain */
	ret = regmap_read(dpmaif_ctl->infra_ao_base, INFRA_RST0_REG_AO, &reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: read infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);

	reg_value &= ~(DPMAIF_AO_RST_MASK); /* the bits in reg is WO, */
	reg_value |= (DPMAIF_AO_RST_MASK);/* so only this bit effective */

	regmap_write(dpmaif_ctl->infra_ao_base, INFRA_RST0_REG_AO, reg_value);
	CCCI_BOOTUP_LOG(0, TAG, "[%s] clear reset\n", __func__);

	/* reset dpmaif clr */
	ret = regmap_read(dpmaif_ctl->infra_ao_base, INFRA_RST1_REG_AO, &reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: read infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);
	reg_value &= ~(DPMAIF_AO_RST_MASK);/* read no use, maybe a time delay */
	reg_value |= (DPMAIF_AO_RST_MASK);

	regmap_write(dpmaif_ctl->infra_ao_base, INFRA_RST1_REG_AO, reg_value);
	CCCI_BOOTUP_LOG(0, TAG, "[%s] 1 reset clr done\n", __func__);

	/* reset dpmaif hw: PD Domain */
	ret = regmap_read(dpmaif_ctl->infra_ao_base, INFRA_RST0_REG_PD, &reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: read infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);
	reg_value &= ~(DPMAIF_PD_RST_MASK);
	reg_value |= (DPMAIF_PD_RST_MASK);

	regmap_write(dpmaif_ctl->infra_ao_base, INFRA_RST0_REG_PD, reg_value);
	CCCI_BOOTUP_LOG(0, TAG, "[%s] clear reset\n", __func__);
	/* reset dpmaif clr */

	ret = regmap_read(dpmaif_ctl->infra_ao_base, INFRA_RST1_REG_PD, &reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: read infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);
	reg_value &= ~(DPMAIF_PD_RST_MASK);
	reg_value |= (DPMAIF_PD_RST_MASK);

	regmap_write(dpmaif_ctl->infra_ao_base, INFRA_RST1_REG_PD, reg_value);
	CCCI_BOOTUP_LOG(0, TAG, "[%s] 2 reset clr done\n", __func__);

	/* post- DPMAIF HW reset: bus-protect */
	regmap_write(dpmaif_ctl->infra_ao_base, INFRA_TOPAXI_PROTECTEN_1_CLR,
			DPMAIF_SLEEP_PROTECT_CTRL);
}

static int drv1_start(void)
{
	return drv1_intr_hw_init();
}


static bool drv1_dpmaif_check_power_down(void)
{
	if (DPMA_READ_PD_UL(DPMAIF_ULQSAR_n(0)) == 0)
		return true;
	else
		return false;
}

static int drv1_suspend_noirq(struct device *dev)
{
	return 0;
}

static inline int drv1_dl_restore(unsigned int mask)
{
	int count = 0;

	/*Set DL/RX interrupt*/
	/* 2. clear interrupt enable mask*/
	/*DPMAIF_WriteReg32(DPMAIF_PD_AP_DL_L2TICR0, AP_DL_L2INTR_En_Msk);*/
	/*set interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMSR0, (mask));

	ccci_drv_set_dl_interrupt_mask(mask);

	/* 3. check mask sts*/
	while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) & mask) != mask) {
		count++;
		if (count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: ckeck mask sts fail\n", __func__);
			return HW_REG_TIME_OUT;
		}
	}

	return 0;
}

static inline void drv1_init_ul_intr(void)
{
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, 0xFFFFFFFF);

	/* 2. set interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TICR0, AP_UL_L2INTR_En_Msk);
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISR0, ~(AP_UL_L2INTR_En_Msk));
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_IP_BUSY, 0xFFFFFFFF);
	/* 2. set IP busy unmask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DLUL_IP_BUSY_MASK, 0);
}

static void drv1_dump_register(int buf_type)
{
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_ul_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_ul_base + DPMAIF_PD_UL_ADD_DESC,
		DPMAIF_PD_UL_ADD_DESC_CH - DPMAIF_PD_UL_ADD_DESC + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_ul_base + DPMAIF_PD_UL_ADD_DESC,
		DPMAIF_PD_UL_ADD_DESC_CH - DPMAIF_PD_UL_ADD_DESC + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF ao_ul_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->ao_ul_base + DPMAIF_AO_UL_CHNL0_STA,
		DPMAIF_AO_UL_CHNL3_STA - DPMAIF_AO_UL_CHNL0_STA + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->ao_ul_base + DPMAIF_AO_UL_CHNL0_STA,
		DPMAIF_AO_UL_CHNL3_STA - DPMAIF_AO_UL_CHNL0_STA + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_dl_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_BAT_INIT,
		DPMAIF_PD_DL_MISC_CON0 - DPMAIF_PD_DL_BAT_INIT + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_BAT_INIT,
		DPMAIF_PD_DL_MISC_CON0 - DPMAIF_PD_DL_BAT_INIT + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_dl_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_STA0,
		DPMAIF_PD_DL_DBG_STA14 - DPMAIF_PD_DL_STA0 + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_STA0,
		DPMAIF_PD_DL_DBG_STA14 - DPMAIF_PD_DL_STA0 + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_dl_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_dl_base + 0x100, 0xC8);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_dl_base + 0x100, 0xC8);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_dl_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_dl_base + 0x200, 0x58 + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_dl_base + 0x200, 0x58 + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF ao_dl_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->ao_dl_base + DPMAIF_AO_DL_PKTINFO_CONO,
		DPMAIF_AO_DL_FRGBAT_STA2 - DPMAIF_AO_DL_PKTINFO_CONO + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->ao_dl_base + DPMAIF_AO_DL_PKTINFO_CONO,
		DPMAIF_AO_DL_FRGBAT_STA2 - DPMAIF_AO_DL_PKTINFO_CONO + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_misc_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_misc_base + DPMAIF_PD_AP_UL_L2TISAR0,
		DPMAIF_PD_AP_CODA_VER - DPMAIF_PD_AP_UL_L2TISAR0 + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_misc_base + DPMAIF_PD_AP_UL_L2TISAR0,
		DPMAIF_PD_AP_CODA_VER - DPMAIF_PD_AP_UL_L2TISAR0 + 4);

	/* open sram clock for debug sram needs sram clock. */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_CG_EN, 0x36);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_sram_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_sram_base + 0x00, 0x184);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_sram_base + 0x00, 0x184);
}

static void drv1_txq_hw_init(struct dpmaif_tx_queue *txq)
{
	unsigned long long base_addr;

	if (txq->started == false) {
		drv1_ul_arb_en(txq->index, false);
		return;
	}

	base_addr = (unsigned long long)txq->drb_phy_addr;

	/* 1. BAT buffer parameters setting */
	drv1_ul_update_drb_size(txq->index,
		txq->drb_cnt * sizeof(struct dpmaif_drb_pd));
	drv1_ul_update_drb_base_addr(txq->index,
		(base_addr&0xFFFFFFFF), ((base_addr>>32)&0xFFFFFFFF));

	drv1_ul_rdy_en(txq->index, true);
	drv1_ul_arb_en(txq->index, true);
}

static int drv1_resume_noirq(struct device *dev)
{
	struct dpmaif_tx_queue *txq = NULL;
	int i, ret = 0;
	unsigned int rel_cnt = 0;

	/*IP power down before and need to restore*/
	CCCI_NORMAL_LOG(0, TAG,
		"[%s] sys_resume need to restore(0x%x, 0x%x, 0x%x)\n",
		__func__, drv1_ul_get_rwidx(0), drv1_ul_get_rwidx(1), drv1_ul_get_rwidx(3));

	/*flush and release UL descriptor*/
	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		txq = &dpmaif_ctl->txq[i];

		if (atomic_read(&txq->drb_rd_idx) != atomic_read(&txq->drb_wr_idx)) {
			CCCI_NORMAL_LOG(0, TAG,
				"[%s] txq(%d) warning: md not read all skb: rel/r/w(%d,%d,%d)\n",
				__func__, i, atomic_read(&txq->drb_rel_rd_idx),
				atomic_read(&txq->drb_rd_idx), atomic_read(&txq->drb_wr_idx));
		}

		if (atomic_read(&txq->drb_wr_idx) != atomic_read(&txq->drb_rel_rd_idx)) {
			rel_cnt = get_ringbuf_release_cnt(txq->drb_cnt,
						atomic_read(&txq->drb_rel_rd_idx),
						atomic_read(&txq->drb_wr_idx));
			ccci_dpmaif_txq_release_skb(txq, rel_cnt);
		}

		atomic_set(&txq->drb_rd_idx, 0);
		atomic_set(&txq->drb_wr_idx, 0);
		atomic_set(&txq->drb_rel_rd_idx, 0);
	}

	/* there are some inter for init para. check. */
	/* maybe need changed to drv_dpmaif_intr_hw_init();*/
	ret = drv1_dl_restore(dpmaif_ctl->suspend_reg_int_mask_bak);
	if (ret)
		return ret;

	drv1_init_ul_intr();

	/*flush and release UL descriptor*/
	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		txq = &dpmaif_ctl->txq[i];

		drv1_txq_hw_init(txq);
		atomic_set(&txq->txq_resume_done, 1);
	}

	return 0;
}

int ccci_dpmaif_drv1_init(void)
{
	dpmaif_ctl->clk_tbs = g_clk_tbs;

	if (dpmaif_ctl->dl_bat_entry_size == 0)
		dpmaif_ctl->dl_bat_entry_size = DPMAIF_DL_BAT_ENTRY_SIZE;
	dpmaif_ctl->dl_pit_entry_size = dpmaif_ctl->dl_bat_entry_size * 2;
	dpmaif_ctl->dl_pit_byte_size  = DPMAIF_DL_PIT_BYTE_SIZE;

	drv.pit_size_msk = DPMAIF_PIT_SIZE_MSK;
	drv.dl_pit_wridx_msk = DPMAIF_DL_PIT_WRIDX_MSK;
	drv.ul_int_md_not_ready_msk = DPMAIF_UL_INT_MD_NOTREADY_MSK |
				DPMAIF_UL_INT_MD_PWR_NOTREADY_MSK;
	drv.ap_ul_l2intr_err_en_msk = AP_UL_L2INTR_ERR_En_Msk;
	drv.normal_pit_size = sizeof(struct dpmaif_normal_pit_v1);
	drv.ul_int_qdone_msk = DPMAIF_UL_INT_QDONE_MSK;
	drv.dl_idle_sts = DPMAIF_DL_IDLE_STS;

	dpmaif_ctl->rxq[0].rxq_isr = &drv1_isr;
	dpmaif_ctl->rxq[0].rxq_drv_dl_add_pit_remain_cnt = &ccci_drv_dl_add_pit_remain_cnt;
	ops.drv_start = &drv1_start;
	ops.drv_suspend_noirq = drv1_suspend_noirq;
	ops.drv_resume_noirq = drv1_resume_noirq;

	dpmaif_ctl->rxq[0].rxq_drv_unmask_dl_interrupt = &drv1_unmask_dl_interrupt;
	ops.drv_unmask_ul_interrupt = &drv1_unmask_ul_interrupt;
	ops.drv_dl_get_wridx = &drv1_dl_get_wridx;
	ops.drv_ul_get_rwidx = &drv1_ul_get_rwidx;
	ops.drv_ul_get_rdidx = &drv1_ul_get_rdidx;
	ops.drv_ul_all_queue_en = &drv1_ul_all_queue_en;
	ops.drv_ul_idle_check = &drv1_ul_idle_check;
	ops.drv_hw_reset = &drv1_hw_reset;
	ops.drv_check_power_down = &drv1_dpmaif_check_power_down;
	ops.drv_get_dl_interrupt_mask = &drv1_get_dl_interrupt_mask;
	ops.drv_txq_hw_init = &drv1_txq_hw_init;
	ops.drv_dump_register = &drv1_dump_register;

	return 0;
}


