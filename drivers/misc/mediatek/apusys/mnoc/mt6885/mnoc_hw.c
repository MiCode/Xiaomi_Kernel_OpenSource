/*
 * Copyright (C) 2019 MediaTek Inc.
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

/*
 *=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/printk.h>
#include <linux/io.h>
#include <linux/spinlock.h>

#include "apusys_device.h"
#include "mnoc_hw.h"
#include "mnoc_drv.h"


int apusys_dev_to_core_id(int dev_type, int dev_core)
{
	int ret = -1;

	switch (dev_type) {
	case APUSYS_DEVICE_VPU:
		if (dev_core >= 0 && dev_core < NR_APU_ENGINE_VPU)
			ret = dev_core;
		break;
	case APUSYS_DEVICE_MDLA:
		if (dev_core >= 0 && dev_core < NR_APU_ENGINE_MDLA)
			ret = NR_APU_ENGINE_VPU + dev_core;
		break;
	case APUSYS_DEVICE_EDMA:
		if (dev_core >= 0 && dev_core < NR_APU_ENGINE_EDMA)
			ret = NR_APU_ENGINE_VPU + NR_APU_ENGINE_MDLA + dev_core;
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}


/**
 * MNI offset 0 -> MNI05_QOS_CTRL0
 * MNI offset 1 -> MNI06_QOS_CTRL0
 * MNI offset 2 -> MNI07_QOS_CTRL0
 * MNI offset 3 -> MNI08_QOS_CTRL0
 * MNI offset 4 -> MNI13_QOS_CTRL0
 * MNI offset 5 -> MNI15_QOS_CTRL0
 * MNI offset 6 -> MNI00_QOS_CTRL0
 * MNI offset 7 -> MNI09_QOS_CTRL0
 * MNI offset 8 -> MNI10_QOS_CTRL0
 * MNI offset 9 -> MNI03_QOS_CTRL0
 * MNI offset 10 -> MNI01_QOS_CTRL0
 * MNI offset 11 -> MNI02_QOS_CTRL0
 * MNI offset 12 -> MNI04_QOS_CTRL0
 * MNI offset 13 -> MNI11_QOS_CTRL0
 * MNI offset 14 -> MNI12_QOS_CTRL0
 * MNI offset 15 -> MNI14_QOS_CTRL0
 * VPU0		-> MNI00 -> offset 6
 * VPU1		-> MNI01 -> offset 10
 * VPU2		-> MNI02 -> offset 11
 * MDLA0_0	-> MNI05 -> offset 0
 * MDLA0_1	-> MNI06 -> offset 1
 * MDLA1_0	-> MNI07 -> offset 2
 * MDLA1_1	-> MNI08 -> offset 3
 * EDMA_0	-> MNI09 -> offset 7
 * EDMA_1	-> MNI10 -> offset 8
 * MD32		-> MNI04 -> offset 12
 */
static char mni_map[NR_APU_QOS_MNI] = {6, 10, 11, 0, 1, 2, 3, 7, 8, 12};


/* register to apusys power on callback */
void mnoc_qos_reg_init(void)
{
	int i;
	unsigned long flags;

	LOG_DEBUG("%s enter\n", __func__);

	spin_lock_irqsave(&mnoc_spinlock, flags);

	/* time slot setting */
	for (i = 0; i < NR_APU_QOS_MNI; i++) {
		/* QoS watcher BW time slot set to 1.26 ms */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			2, mni_map[i]), 1:0, 0x1);
		/* QoS guardian BW time slot set to 1.26 ms */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			16, mni_map[i]), 1:0, 0x1);

		/* 26M cycle count = {QW_LT_PRD,8’h0} << QW_LT_PRD_SHF */
		/* QW_LT_PRD = 0x80, QW_LT_PRD_SHF = 0x0 */
		/* QoS watcher LT time slot set to 1.26 ms */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			5, mni_map[i]), 7:0, 0x80);
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			5, mni_map[i]), 10:8, 0x0);
		/* QoS guardian LT time slot set to 1.26 ms */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			19, mni_map[i]), 7:0, 0x80);
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			19, mni_map[i]), 10:8, 0x0);

		/* MNI to SNI path setting */
		/* set QoS guardian to monitor DRAM only */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			31, mni_map[i]), 31:16, 0xF000);
		/* set QoS watcher to monitor DRAM+TCM */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			31, mni_map[i]), 15:0, 0xFF00);

		/* set QW_BW_INT_EN = 1 to enable monitor */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			1, mni_map[i]), 2:2, 0x1);
		/* set QG_BW_INT_EN = 1 to enable monitor */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			15, mni_map[i]), 2:2, 0x1);
		/* set QW_LT_INT_EN = 1 to enable monitor */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			1, mni_map[i]), 4:4, 0x1);
		/* set QG_LT_INT_EN = 1 to enable monitor */
		mnoc_write_field(MNI_QOS_REG(MNI_QOS_CTRL_BASE,
			15, mni_map[i]), 4:4, 0x1);
	}

	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	LOG_DEBUG("%s exit\n", __func__);
}

/* register to apusys power on callback */
void mnoc_reg_init(void)
{
	int i;
	unsigned long flags;

	LOG_DEBUG("+\n");

	spin_lock_irqsave(&mnoc_spinlock, flags);

	/* enable mnoc interrupt */
	mnoc_write_field(MNOC_INT_EN, 1:0, 3);

	/* set request router timeout interrupt */
	for (i = 0; i < NR_MNOC_RT; i++) {
		/* all VC enabled */
		mnoc_write(MNOC_RT_PMU_REG(REQ_RT_PMU_BASE, 3, i), 0xFFFFFFFF);
		mnoc_write(MNOC_RT_PMU_REG(REQ_RT_PMU_BASE, 4, i), 0xFFFFFFFF);
		/* set timeout threshold to 510 cycles */
		mnoc_write_field(MNOC_RT_PMU_REG(REQ_RT_PMU_BASE, 2, i),
			8:0, 510);
		/* enable timeout counting */
		mnoc_write_field(MNOC_RT_PMU_REG(REQ_RT_PMU_BASE, 2, i),
			31:31, 1);
	}

	/* set response router timeout interrupt */
	for (i = 0; i < NR_MNOC_RT; i++) {
		/* all VC enabled */
		mnoc_write(MNOC_RT_PMU_REG(RSP_RT_PMU_BASE, 3, i), 0xFFFFFFFF);
		mnoc_write(MNOC_RT_PMU_REG(RSP_RT_PMU_BASE, 4, i), 0xFFFFFFFF);
		/* set timeout threshold to 510 cycles */
		mnoc_write_field(MNOC_RT_PMU_REG(RSP_RT_PMU_BASE, 2, i),
			8:0, 510);
		/* enable timeout counting */
		mnoc_write_field(MNOC_RT_PMU_REG(RSP_RT_PMU_BASE, 2, i),
			31:31, 1);
	}

	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	LOG_DEBUG("-\n");
}

/*
 * todo: extinguish mnoc irq0 and irq1 for better efficiency?
 * GIC SPI IRQ 406 is shared, need to return IRQ_NONE
 * if not triggered by mnoc
 */
bool mnoc_check_int_status(void)
{
	unsigned long flags;
	bool mnoc_irq_triggered = false;

	LOG_DEBUG("+\n");

	spin_lock_irqsave(&mnoc_spinlock, flags);

	/* prevent register access if apusys power off */
	if (!mnoc_reg_valid)
		return mnoc_irq_triggered;

	if (mnoc_read_field(MNI_QOS_IRQ_FLAG, 15:0) != 0) {
		LOG_ERR("MNI_QOS_IRQ_FLAG = 0x%x\n",
			mnoc_read(MNI_QOS_IRQ_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(MNI_QOS_IRQ_FLAG, 15:0, 0xFFFF);
		mnoc_irq_triggered = true;
	}
	if (mnoc_read_field(ADDR_DEC_ERR_FLAG, 15:0) != 0) {
		LOG_ERR("ADDR_DEC_ERR_FLAG = 0x%x\n",
			mnoc_read(ADDR_DEC_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(ADDR_DEC_ERR_FLAG, 15:0, 0xFFFF);
		mnoc_irq_triggered = true;
	}
	if (mnoc_read_field(MST_PARITY_ERR_FLAG, 15:0) != 0) {
		LOG_ERR("MST_PARITY_ERR_FLAG = 0x%x\n",
			mnoc_read(MST_PARITY_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(MST_PARITY_ERR_FLAG, 15:0, 0xFFFF);
		mnoc_irq_triggered = true;
	}
	if (mnoc_read_field(SLV_PARITY_ERR_FLA, 15:0) != 0) {
		LOG_ERR("SLV_PARITY_ERR_FLA = 0x%x\n",
			mnoc_read(SLV_PARITY_ERR_FLA));
		/* clear interrupt (W1C) */
		mnoc_write_field(SLV_PARITY_ERR_FLA, 15:0, 0xFFFF);
		mnoc_irq_triggered = true;
	}
	if (mnoc_read_field(MST_MISRO_ERR_FLAG, 15:0) != 0) {
		LOG_ERR("MST_MISRO_ERR_FLAG = 0x%x\n",
			mnoc_read(MST_MISRO_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(MST_MISRO_ERR_FLAG, 15:0, 0xFFFF);
		mnoc_irq_triggered = true;
	}
	if (mnoc_read_field(SLV_MISRO_ERR_FLAG, 15:0) != 0) {
		LOG_ERR("SLV_MISRO_ERR_FLAG = 0x%x\n",
			mnoc_read(SLV_MISRO_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(SLV_MISRO_ERR_FLAG, 15:0, 0xFFFF);
		mnoc_irq_triggered = true;
	}

	if (mnoc_read_field(REQRT_MISRO_ERR_FLAG, 4:0) != 0) {
		LOG_ERR("REQRT_MISRO_ERR_FLAG = 0x%x\n",
			mnoc_read(REQRT_MISRO_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(REQRT_MISRO_ERR_FLAG, 4:0, 0x1F);
		mnoc_irq_triggered = true;
	}
	if (mnoc_read_field(RSPRT_MISRO_ERR_FLAG, 4:0) != 0) {
		LOG_ERR("RSPRT_MISRO_ERR_FLAG = 0x%x\n",
			mnoc_read(RSPRT_MISRO_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(RSPRT_MISRO_ERR_FLAG, 4:0, 0x1F);
		mnoc_irq_triggered = true;
	}
	if (mnoc_read_field(REQRT_TO_ERR_FLAG, 4:0) != 0) {
		LOG_ERR("REQRT_TO_ERR_FLAG = 0x%x\n",
			mnoc_read(REQRT_TO_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(REQRT_TO_ERR_FLAG, 4:0, 0x1F);
		mnoc_irq_triggered = true;
	}
	if (mnoc_read_field(RSPRT_TO_ERR_FLAG, 4:0) != 0) {
		LOG_ERR("RSPRT_TO_ERR_FLAG = 0x%x\n",
			mnoc_read(RSPRT_TO_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(RSPRT_TO_ERR_FLAG, 4:0, 0x1F);
		mnoc_irq_triggered = true;
	}

	if (mnoc_read_field(REQRT_CBUF_ERR_FLAG, 4:0) != 0) {
		LOG_ERR("REQRT_CBUF_ERR_FLAG = 0x%x\n",
			mnoc_read(REQRT_CBUF_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(REQRT_CBUF_ERR_FLAG, 4:0, 0x1F);
		mnoc_irq_triggered = true;
	}
	if (mnoc_read_field(RSPRT_CBUF_ERR_FLAG, 4:0) != 0) {
		LOG_ERR("RSPRT_CBUF_ERR_FLAG = 0x%x\n",
			mnoc_read(RSPRT_CBUF_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(RSPRT_CBUF_ERR_FLAG, 4:0, 0x1F);
		mnoc_irq_triggered = true;
	}
	if (mnoc_read_field(MST_CRDT_ERR_FLAG, 15:0) != 0) {
		LOG_ERR("MST_CRDT_ERR_FLAG = 0x%x\n",
			mnoc_read(MST_CRDT_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(MST_CRDT_ERR_FLAG, 15:0, 0xFFFF);
		mnoc_irq_triggered = true;
	}
	if (mnoc_read_field(SLV_CRDT_ERR_FLAG, 15:0) != 0) {
		LOG_ERR("SLV_CRDT_ERR_FLAG = 0x%x\n",
			mnoc_read(SLV_CRDT_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(SLV_CRDT_ERR_FLAG, 15:0, 0xFFFF);
		mnoc_irq_triggered = true;
	}
	if (mnoc_read_field(REQRT_CRDT_ERR_FLAG, 4:0) != 0) {
		LOG_ERR("REQRT_CRDT_ERR_FLAG = 0x%x\n",
			mnoc_read(REQRT_CRDT_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(REQRT_CRDT_ERR_FLAG, 4:0, 0x1F);
		mnoc_irq_triggered = true;
	}
	if (mnoc_read_field(RSPRT_CRDT_ERR_FLAG, 4:0) != 0) {
		LOG_ERR("RSPRT_CRDT_ERR_FLAG = 0x%x\n",
			mnoc_read(RSPRT_CRDT_ERR_FLAG));
		/* clear interrupt (W1C) */
		mnoc_write_field(RSPRT_CRDT_ERR_FLAG, 4:0, 0x1F);
		mnoc_irq_triggered = true;
	}

	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	LOG_DEBUG("-\n");

	return mnoc_irq_triggered;
}

/* read PMU_COUNTER_OUT 0~15 value to pmu buffer */
void mnoc_get_pmu_counter(unsigned int *buf)
{
	int i;
	unsigned long flags;

	LOG_DEBUG("+\n");

	spin_lock_irqsave(&mnoc_spinlock, flags);
	if (mnoc_reg_valid)
		for (i = 0; i < NR_MNOC_PMU_CNTR; i++)
			buf[i] = mnoc_read(PMU_COUNTER0_OUT + 4*i);
	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	LOG_DEBUG("-\n");
}

void mnoc_tcm_hash_set(unsigned int sel, unsigned int en0, unsigned int en1)
{
	unsigned long flags;

	LOG_DEBUG("+\n");

	spin_lock_irqsave(&mnoc_spinlock, flags);
	mnoc_write_field(APU_TCM_HASH_TRUNCATE_CTRL0, 2:0, sel);
	mnoc_write_field(APU_TCM_HASH_TRUNCATE_CTRL0, 6:3, en0);
	mnoc_write_field(APU_TCM_HASH_TRUNCATE_CTRL0, 10:7, en1);
	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	LOG_DEBUG("APU_TCM_HASH_TRUNCATE_CTRL0 = 0x%x\n",
		mnoc_read(APU_TCM_HASH_TRUNCATE_CTRL0));

	LOG_DEBUG("-\n");
}
