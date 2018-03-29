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


#define LOG_TAG "IRQ"

#include "ddp_log.h"
#include "ddp_debug.h"

#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/timer.h>

#include "ddp_reg.h"
#include "ddp_irq.h"
#include "ddp_aal.h"
#include "ddp_drv.h"
#include "primary_display.h"

/* IRQ log print kthread */
static struct task_struct *disp_irq_log_task;
static wait_queue_head_t disp_irq_log_wq;
static int disp_irq_log_module;

static int irq_init;

static unsigned int cnt_rdma_underflow[3];
static unsigned int cnt_rdma_abnormal[3];
static unsigned int cnt_ovl_underflow[2];
static unsigned int cnt_wdma_underflow[2];

unsigned long long rdma_start_time[3] = { 0 };
unsigned long long rdma_end_time[3] = { 0 };

#define DISP_MAX_IRQ_CALLBACK   10

static DDP_IRQ_CALLBACK irq_module_callback_table[DISP_MODULE_NUM][DISP_MAX_IRQ_CALLBACK];
static DDP_IRQ_CALLBACK irq_callback_table[DISP_MAX_IRQ_CALLBACK];

int disp_register_irq_callback(DDP_IRQ_CALLBACK cb)
{
	int i = 0;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_callback_table[i] == cb)
			break;
	}
	if (i < DISP_MAX_IRQ_CALLBACK)
		return 0;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_callback_table[i] == NULL)
			break;
	}
	if (i == DISP_MAX_IRQ_CALLBACK) {
		DDPERR("not enough irq callback entries for module\n");
		return -1;
	}
	DDPDBG("register callback on %d", i);
	irq_callback_table[i] = cb;
	return 0;
}

int disp_unregister_irq_callback(DDP_IRQ_CALLBACK cb)
{
	int i;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_callback_table[i] == cb) {
			irq_callback_table[i] = NULL;
			break;
		}
	}
	if (i == DISP_MAX_IRQ_CALLBACK) {
		DDPERR("Try to unregister callback function 0x%lx which was not registered\n",
		       (unsigned long)cb);
		return -1;
	}
	return 0;
}

int disp_register_module_irq_callback(DISP_MODULE_ENUM module, DDP_IRQ_CALLBACK cb)
{
	int i;

	if (module >= DISP_MODULE_NUM) {
		DDPERR("Register IRQ with invalid module ID. module=%d\n", module);
		return -1;
	}
	if (cb == NULL) {
		DDPERR("Register IRQ with invalid cb.\n");
		return -1;
	}
	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_module_callback_table[module][i] == cb)
			break;
	}
	if (i < DISP_MAX_IRQ_CALLBACK) {
		/* Already registered. */
		return 0;
	}
	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_module_callback_table[module][i] == NULL)
			break;
	}
	if (i == DISP_MAX_IRQ_CALLBACK) {
		DDPERR("No enough callback entries for module %d.\n", module);
		return -1;
	}
	irq_module_callback_table[module][i] = cb;
	return 0;
}

int disp_unregister_module_irq_callback(DISP_MODULE_ENUM module, DDP_IRQ_CALLBACK cb)
{
	int i;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_module_callback_table[module][i] == cb) {
			irq_module_callback_table[module][i] = NULL;
			break;
		}
	}
	if (i == DISP_MAX_IRQ_CALLBACK) {
		DDPDBG
		    ("Try to unregister callback function with was not registered. module=%d cb=0x%lx\n",
		     module, (unsigned long)cb);
		return -1;
	}
	return 0;
}

void disp_invoke_irq_callbacks(DISP_MODULE_ENUM module, unsigned int param)
{
	int i;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {

		if (irq_callback_table[i]) {
			/* DDPERR("Invoke callback function. module=%d param=0x%X\n", module, param); */
			irq_callback_table[i] (module, param);
		}

		if (irq_module_callback_table[module][i]) {
			/* DDPERR("Invoke module callback function. module=%d param=0x%X\n", module, param); */
			irq_module_callback_table[module][i] (module, param);
		}
	}
}

/* Mark out for eliminate build warning message, because it is not used */
#if 0
static DISP_MODULE_ENUM find_module_by_irq(int irq)
{
	/* should sort irq_id_to_module_table by numberic sequence */
	int i = 0;
#define DSIP_IRQ_NUM_MAX (16)
	static struct irq_module_map {
		int irq;
		DISP_MODULE_ENUM module;
	} const irq_id_to_module_table[DSIP_IRQ_NUM_MAX] = {
		{MM_MUTEX_IRQ_BIT_ID, DISP_MODULE_MUTEX},
		{DISP_OVL0_IRQ_BIT_ID, DISP_MODULE_OVL0},
		{DISP_OVL1_IRQ_BIT_ID, DISP_MODULE_OVL0},
		{DISP_RDMA0_IRQ_BIT_ID, DISP_MODULE_RDMA0},
		{DISP_RDMA1_IRQ_BIT_ID, DISP_MODULE_RDMA1},
		{DISP_RDMA2_IRQ_BIT_ID, DISP_MODULE_RDMA2},
		{DISP_WDMA0_IRQ_BIT_ID, DISP_MODULE_WDMA0},
		{DISP_WDMA1_IRQ_BIT_ID, DISP_MODULE_WDMA1},
		{DISP_COLOR0_IRQ_BIT_ID, DISP_MODULE_COLOR0},
		{DISP_COLOR1_IRQ_BIT_ID, DISP_MODULE_COLOR1},
		{DISP_AAL_IRQ_BIT_ID, DISP_MODULE_AAL},
		{DISP_GAMMA_IRQ_BIT_ID, DISP_MODULE_GAMMA},
		{DISP_UFOE_IRQ_BIT_ID, DISP_MODULE_UFOE},
		{DSI0_IRQ_BIT_ID, DISP_MODULE_DSI0},
		{DSI1_IRQ_BIT_ID, DISP_MODULE_DSI1},
		{DPI0_IRQ_BIT_ID, DISP_MODULE_DPI0},
	};
	for (i = 0; i < DSIP_IRQ_NUM_MAX; i++) {
		if (irq_id_to_module_table[i].irq == irq) {
			DDPDBG("find module %d by irq %d\n", irq, irq_id_to_module_table[i].module);
			return irq_id_to_module_table[i].module;
		}
	}
	return DISP_MODULE_UNKNOWN;
}
#endif

void disp_dump_emi_status(void)
{
#define INFRA_BASE_PA 0x10001000
#define EMI_BASE_PA   0x10203000

	unsigned long infra_base_va = 0;
	unsigned long emi_base_va = 0;
	unsigned int i = 0;

	infra_base_va = (unsigned long)ioremap_nocache(INFRA_BASE_PA, 0x200);
	emi_base_va = (unsigned long)ioremap_nocache(EMI_BASE_PA, 0x200);
	DDPMSG("dump emi status, infra_base_va=0x%lx, emi_base_va=0x%lx,\n", infra_base_va,
	       emi_base_va);
	pr_err("0x10203000: ");
	for (i = 0; i < 0x158; i += 4) {
		pr_err("0x%x, ", DISP_REG_GET(emi_base_va + i));
		if (i % 32 == 0 && i != 0)
			pr_err("\n 0x%x: ", EMI_BASE_PA + 32 * i);
	}
	pr_err("\n*(0x10001098)=0x%x.\n", DISP_REG_GET(infra_base_va + 0x98));

	iounmap((void *)infra_base_va);
	iounmap((void *)emi_base_va);

}

/* /TODO:  move each irq to module driver */
irqreturn_t disp_irq_handler(int irq, void *dev_id)
{
	DISP_MODULE_ENUM module = DISP_MODULE_UNKNOWN;
	unsigned long reg_val = 0;
	unsigned int index = 0;
	unsigned int mutexID = 0;

	MMProfileLogEx(ddp_mmp_get_events()->DDP_IRQ, MMProfileFlagStart, irq, 0);
	/* printk("disp_irq_handler %d\n", irq); */
	if (irq == ddp_irq_map[DISP_MODULE_DSI0] || irq == ddp_irq_map[DISP_MODULE_DSI1]) {
		index = (irq == ddp_irq_map[DISP_MODULE_DSI0]) ? 0 : 1;
		module =
		    (irq == ddp_irq_map[DISP_MODULE_DSI0]) ? DISP_MODULE_DSI0 : DISP_MODULE_DSI1;
		reg_val =
		    (DISP_REG_GET(DISPSYS_DSI0_BASE + 0xC + index * DISP_INDEX_OFFSET) & 0xff);
		if (primary_display_esd_cust_get() == 1)
			reg_val = reg_val & 0xfffe;
		DISP_CPU_REG_SET(DISPSYS_DSI0_BASE + 0xC + index * DISP_INDEX_OFFSET, ~reg_val);
		/* MMProfileLogEx(ddp_mmp_get_events()->DSI_IRQ[index], MMProfileFlagPulse, reg_val, 0); */
	} else if (irq == ddp_irq_map[DISP_MODULE_DPI0] || irq == ddp_irq_map[DISP_MODULE_DPI1]) {
		/* printk("disp_irq_handler dpi: %d\n", irq); */
		index = (irq == ddp_irq_map[DISP_MODULE_DPI0]) ? 0 : 1;
		module =
		    (irq == ddp_irq_map[DISP_MODULE_DPI0]) ? DISP_MODULE_DPI0 : DISP_MODULE_DPI1;
		reg_val =
		    (DISP_REG_GET(DISPSYS_DPI0_BASE + 0xC + index * DISP_INDEX_OFFSET) & 0xff);
		if (reg_val & (1 << 0))
			/*printk("IRQ: DPI%d VSYNC!\n", index); */
		if (reg_val & (1 << 1))
			/*printk("IRQ: DPI%d VDE!\n", index); */
		if (reg_val & (1 << 2))
			/*printk("IRQ: DPI%d underflow!\n", index); */
		DISP_CPU_REG_SET(DISPSYS_DPI0_BASE + 0xC + index * DISP_INDEX_OFFSET, ~reg_val);
	} else if (irq == ddp_irq_map[DISP_MODULE_OVL0] || irq == ddp_irq_map[DISP_MODULE_OVL1]) {
		index = (irq == ddp_irq_map[DISP_MODULE_OVL0]) ? 0 : 1;
		module =
		    (irq == ddp_irq_map[DISP_MODULE_OVL0]) ? DISP_MODULE_OVL0 : DISP_MODULE_OVL1;
		reg_val = DISP_REG_GET(DISP_REG_OVL_INTSTA + index * DISP_INDEX_OFFSET);
		if (reg_val & (1 << 1))
			DDPIRQ("IRQ: OVL%d frame done!\n", index);
		if (reg_val & (1 << 2)) {
			DDPMSG("IRQ: OVL%d frame underrun! cnt=%d\n", index,
			       cnt_ovl_underflow[index]++);
			disp_irq_log_module |= 1 << module;
		}
		if (reg_val & (1 << 3))
			DDPIRQ("IRQ: OVL%d sw reset done\n", index);

		if (reg_val & (1 << 4))
			DDPIRQ("IRQ: OVL%d hw reset done\n", index);

		if (reg_val & (1 << 5)) {
			DDPMSG("IRQ: OVL%d-RDMA0 not complete until EOF!\n", index);
			disp_irq_log_module |= 1 << module;
		}
		if (reg_val & (1 << 6)) {
			DDPMSG("IRQ: OVL%d-RDMA1 not complete until EOF!\n", index);
			disp_irq_log_module |= 1 << module;
		}

		if (reg_val & (1 << 7)) {
			DDPMSG("IRQ: OVL%d-RDMA2 not complete until EOF!\n", index);
			disp_irq_log_module |= 1 << module;
		}
		if (reg_val & (1 << 8)) {
			DDPMSG("IRQ: OVL%d-RDMA3 not complete until EOF!\n", index);
			disp_irq_log_module |= 1 << module;
		}
		if (reg_val & (1 << 9)) {
			DDPMSG("IRQ: OVL%d-RDMA0 fifo underflow!\n", index);
			disp_irq_log_module |= 1 << module;
		}

		if (reg_val & (1 << 10)) {
			DDPMSG("IRQ: OVL%d-RDMA1 fifo underflow!\n", index);
			disp_irq_log_module |= 1 << module;
		}
		if (reg_val & (1 << 11)) {
			DDPMSG("IRQ: OVL%d-RDMA2 fifo underflow!\n", index);
			disp_irq_log_module |= 1 << module;
		}
		if (reg_val & (1 << 12)) {
			DDPMSG("IRQ: OVL%d-RDMA3 fifo underflow!\n", index);
			disp_irq_log_module |= 1 << module;
		}
		DISP_CPU_REG_SET(DISP_REG_OVL_INTSTA + index * DISP_INDEX_OFFSET, ~reg_val);
		/* MMProfileLogEx(ddp_mmp_get_events()->OVL_IRQ[index], MMProfileFlagPulse, */
		/* reg_val, DISP_REG_GET(DISP_REG_OVL_INTSTA+index*DISP_INDEX_OFFSET)); */
	} else if (irq == ddp_irq_map[DISP_MODULE_WDMA0] || irq == ddp_irq_map[DISP_MODULE_WDMA1]) {
		index = (irq == ddp_irq_map[DISP_MODULE_WDMA0]) ? 0 : 1;
		module =
		    (irq == ddp_irq_map[DISP_MODULE_WDMA0]) ? DISP_MODULE_WDMA0 : DISP_MODULE_WDMA1;
		reg_val = DISP_REG_GET(DISP_REG_WDMA_INTSTA + index * DISP_INDEX_OFFSET);
		if (reg_val & (1 << 0))
			DDPIRQ("IRQ: WDMA%d frame done!\n", index);

		if (reg_val & (1 << 1)) {
			DDPMSG("IRQ: WDMA%d underrun! cnt=%d\n", index,
			       cnt_wdma_underflow[index]++);
			disp_irq_log_module |= 1 << module;
		}
		DISP_CPU_REG_SET(DISP_REG_WDMA_INTSTA + index * DISP_INDEX_OFFSET, ~reg_val);
		MMProfileLogEx(ddp_mmp_get_events()->WDMA_IRQ[index], MMProfileFlagPulse,
			       reg_val, DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE));
	} else if (irq == ddp_irq_map[DISP_MODULE_RDMA0] || irq == ddp_irq_map[DISP_MODULE_RDMA1]
		   || irq == ddp_irq_map[DISP_MODULE_RDMA2]) {
		if (ddp_irq_map[DISP_MODULE_RDMA0] == irq) {
			index = 0;
			module = DISP_MODULE_RDMA0;
		} else if (ddp_irq_map[DISP_MODULE_RDMA1] == irq) {
			index = 1;
			module = DISP_MODULE_RDMA1;
		} else if (ddp_irq_map[DISP_MODULE_RDMA2] == irq) {
			index = 2;
			module = DISP_MODULE_RDMA2;
		}
		reg_val = DISP_REG_GET(DISP_REG_RDMA_INT_STATUS + index * DISP_INDEX_OFFSET);
		if (reg_val & (1 << 0))
			DDPIRQ("IRQ: RDMA%d reg update done!\n", index);

		if (reg_val & (1 << 1)) {
			MMProfileLogEx(ddp_mmp_get_events()->SCREEN_UPDATE[index],
				       MMProfileFlagStart, reg_val, 0);
			MMProfileLogEx(ddp_mmp_get_events()->layer[0], MMProfileFlagPulse,
				       DISP_REG_GET(DISP_REG_OVL_L0_ADDR),
				       DISP_REG_GET(DISP_REG_OVL_SRC_CON) & 0x1);
			MMProfileLogEx(ddp_mmp_get_events()->layer[1], MMProfileFlagPulse,
				       DISP_REG_GET(DISP_REG_OVL_L1_ADDR),
				       DISP_REG_GET(DISP_REG_OVL_SRC_CON) & 0x2);
			MMProfileLogEx(ddp_mmp_get_events()->layer[2], MMProfileFlagPulse,
				       DISP_REG_GET(DISP_REG_OVL_L2_ADDR),
				       DISP_REG_GET(DISP_REG_OVL_SRC_CON) & 0x4);
			MMProfileLogEx(ddp_mmp_get_events()->layer[3], MMProfileFlagPulse,
				       DISP_REG_GET(DISP_REG_OVL_L3_ADDR),
				       DISP_REG_GET(DISP_REG_OVL_SRC_CON) & 0x8);
			rdma_start_time[index] = sched_clock();
			DDPIRQ("IRQ: RDMA%d frame start!\n", index);
		}
		if (reg_val & (1 << 2)) {
			MMProfileLogEx(ddp_mmp_get_events()->SCREEN_UPDATE[index], MMProfileFlagEnd,
				       reg_val, 0);
			rdma_end_time[index] = sched_clock();
			DDPIRQ("IRQ: RDMA%d frame done!\n", index);
		}
		if (reg_val & (1 << 3)) {
			DDPMSG("IRQ: RDMA%d abnormal! cnt=%d\n", index, cnt_rdma_abnormal[index]++);
			disp_irq_log_module |= 1 << module;
		}
		if (reg_val & (1 << 4)) {
			DDPMSG("IRQ: RDMA%d underflow! cnt=%d\n", index,
			       cnt_rdma_underflow[index]++);
			disp_irq_log_module |= module;
		}
		if (reg_val & (1 << 5))
			DDPIRQ("IRQ: RDMA%d target line!\n", index);

		/* clear intr */
		DISP_CPU_REG_SET(DISP_REG_RDMA_INT_STATUS + index * DISP_INDEX_OFFSET, ~reg_val);
		/* MMProfileLogEx(ddp_mmp_get_events()->RDMA_IRQ[index], */
		/* MMProfileFlagPulse, reg_val, DISP_REG_GET(DISP_REG_RDMA_INT_STATUS+index*DISP_INDEX_OFFSET)); */
	} else if (irq == ddp_irq_map[DISP_MODULE_COLOR0] || irq == ddp_irq_map[DISP_MODULE_COLOR1]) {
		index = (irq == ddp_irq_map[DISP_MODULE_COLOR0]) ? 0 : 1;
		module =
		    (irq ==
		     ddp_irq_map[DISP_MODULE_COLOR0]) ? DISP_MODULE_COLOR0 : DISP_MODULE_COLOR1;
		reg_val = 0;
	} else if (irq == ddp_irq_map[DISP_MODULE_MUTEX]) {
		module = DISP_MODULE_MUTEX;
		reg_val = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA) & 0x7C1F;
		for (mutexID = 0; mutexID < 5; mutexID++) {
			if (reg_val & (0x1 << mutexID)) {
				DDPIRQ("IRQ: mutex%d sof!\n", mutexID);
				MMProfileLogEx(ddp_mmp_get_events()->MUTEX_IRQ[mutexID],
					       MMProfileFlagPulse, reg_val, 0);
			}
			if (reg_val & (0x1 << (mutexID + DISP_MUTEX_TOTAL))) {
				DDPIRQ("IRQ: mutex%d eof!\n", mutexID);
				MMProfileLogEx(ddp_mmp_get_events()->MUTEX_IRQ[mutexID],
					       MMProfileFlagPulse, reg_val, 1);
			}
		}
		DISP_CPU_REG_SET(DISP_REG_CONFIG_MUTEX_INTSTA, ~reg_val);
	} else if (irq == ddp_irq_map[DISP_MODULE_AAL]) {
		module = DISP_MODULE_AAL;
		reg_val = DISP_REG_GET(DISP_AAL_INTSTA);
		disp_aal_on_end_of_frame();
	} else {
		module = DISP_MODULE_UNKNOWN;
		reg_val = 0;
		DDPMSG("invalid irq=%d\n ", irq);
	}

	disp_invoke_irq_callbacks(module, reg_val);
	if (disp_irq_log_module != 0)
		wake_up_interruptible(&disp_irq_log_wq);

	MMProfileLogEx(ddp_mmp_get_events()->DDP_IRQ, MMProfileFlagEnd, irq, reg_val);
	return IRQ_HANDLED;
}


static int disp_irq_log_kthread_func(void *data)
{
	unsigned int i = 0;

	while (1) {
		wait_event_interruptible(disp_irq_log_wq, disp_irq_log_module);
		DDPMSG("disp_irq_log_kthread_func dump intr register: disp_irq_log_module=%d\n",
		       disp_irq_log_module);
		if ((disp_irq_log_module & (1 << DISP_MODULE_RDMA0)) != 0) {
			/* ddp_dump_analysis(DISP_MODULE_CONFIG); */
			disp_dump_emi_status();
			ddp_dump_analysis(DISP_MODULE_RDMA0);
			ddp_dump_analysis(DISP_MODULE_OVL0);
			ddp_dump_analysis(DISP_MODULE_OVL1);

			/* dump ultra/preultra related regs */
			DDPMSG("wdma_con1(2c)=0x%x, wdma_con2(0x38)=0x%x, rdma_gmc0(30)=0x%x\n",
			       DISP_REG_GET(DISP_REG_WDMA_BUF_CON1),
			       DISP_REG_GET(DISP_REG_WDMA_BUF_CON2),
			       DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_0));
			DDPMSG("rdma_gmc1(38)=0x%x, fifo_con(40)=0x%x\n",
			       DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_1),
			       DISP_REG_GET(DISP_REG_RDMA_FIFO_CON));
			DDPMSG
			    ("ovl0_gmc: 0x%x, 0x%x, 0x%x, 0x%x, ovl1_gmc: 0x%x, 0x%x, 0x%x, 0x%x\n",
			     DISP_REG_GET(DISP_REG_OVL_RDMA0_MEM_GMC_S2),
			     DISP_REG_GET(DISP_REG_OVL_RDMA1_MEM_GMC_S2),
			     DISP_REG_GET(DISP_REG_OVL_RDMA2_MEM_GMC_S2),
			     DISP_REG_GET(DISP_REG_OVL_RDMA3_MEM_GMC_S2),
			     DISP_REG_GET(DISP_REG_OVL_RDMA0_MEM_GMC_S2 + DISP_OVL_INDEX_OFFSET),
			     DISP_REG_GET(DISP_REG_OVL_RDMA1_MEM_GMC_S2 + DISP_OVL_INDEX_OFFSET),
			     DISP_REG_GET(DISP_REG_OVL_RDMA2_MEM_GMC_S2 + DISP_OVL_INDEX_OFFSET),
			     DISP_REG_GET(DISP_REG_OVL_RDMA3_MEM_GMC_S2 + DISP_OVL_INDEX_OFFSET));

			/* dump smi regs */
			/* smi_dumpDebugMsg(); */
		} else {
			for (i = 0; i < DISP_MODULE_NUM; i++) {
				if ((disp_irq_log_module & (1 << i)) != 0)
					ddp_dump_reg(i);
			}
		}
		disp_irq_log_module = 0;
	}
	return 0;
}


void disp_register_dev_irq(unsigned int irq_num, char *device_name)
{
	if (request_irq(irq_num, (irq_handler_t) disp_irq_handler,
			IRQF_TRIGGER_LOW, device_name, NULL)) {
		DDPERR("ddp register irq %u failed on device %s\n", irq_num, device_name);
	}
}

static unsigned int ddp_intr_need_enable(DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_OVL0:
	case DISP_MODULE_OVL1:
	case DISP_MODULE_RDMA0:
	case DISP_MODULE_RDMA1:
		/* case DISP_MODULE_WDMA0: *//* for primary decouple svp */
	case DISP_MODULE_MUTEX:
	case DISP_MODULE_DSI0:
	case DISP_MODULE_AAL:
	case DISP_MODULE_DPI0:
		return 1;
	case DISP_MODULE_WDMA1:	/* FIXME: WDMA1 intr is abonrmal FPGA so mark first, enable after EVB works */
	case DISP_MODULE_COLOR0:
	case DISP_MODULE_COLOR1:
	case DISP_MODULE_GAMMA:
	case DISP_MODULE_MERGE:
	case DISP_MODULE_UFOE:
	case DISP_MODULE_PWM0:
	case DISP_MODULE_CONFIG:
	case DISP_MODULE_SMI_LARB0:
	case DISP_MODULE_SMI_COMMON:
		return 0;
	case DISP_MODULE_WDMA0:
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		return 0;
#else
		return 1;
#endif

	default:
		return 0;
	}
}

int ddp_irq_init(void)
{
	int i, ret;

	if (irq_init)
		return 0;

	irq_init = 1;
	DDPDBG("ddp_irq_init\n");

#if 0
	/* Register IRQ */
	disp_register_dev_irq(DISP_OVL0_IRQ_BIT_ID, "DISP_OVL0");
	disp_register_dev_irq(DISP_OVL1_IRQ_BIT_ID, "DISP_OVL1");
	disp_register_dev_irq(DISP_RDMA0_IRQ_BIT_ID, "DISP_RDMA0");
	disp_register_dev_irq(DISP_RDMA1_IRQ_BIT_ID, "DISP_RDMA1");
	disp_register_dev_irq(DISP_RDMA2_IRQ_BIT_ID, "DISP_RDMA2");
	disp_register_dev_irq(DISP_WDMA0_IRQ_BIT_ID, "DISP_WDMA0");
	disp_register_dev_irq(DISP_WDMA1_IRQ_BIT_ID, "DISP_WDMA1");
	disp_register_dev_irq(DSI0_IRQ_BIT_ID, "DISP_DSI0");
	/* disp_register_dev_irq(DISP_COLOR0_IRQ_BIT_ID ,device_name); */
	/* disp_register_dev_irq(DISP_COLOR1_IRQ_BIT_ID ,device_name); */
	/* disp_register_dev_irq(DISP_GAMMA_IRQ_BIT_ID  ,device_name ); */
	/* disp_register_dev_irq(DISP_UFOE_IRQ_BIT_ID     ,device_name); */
	disp_register_dev_irq(MM_MUTEX_IRQ_BIT_ID, "DISP_MUTEX");
	disp_register_dev_irq(DISP_AAL_IRQ_BIT_ID, "DISP_AAL");
#endif

	/* create irq log thread */
	init_waitqueue_head(&disp_irq_log_wq);
	disp_irq_log_task = kthread_create(disp_irq_log_kthread_func, NULL, "ddp_irq_log_kthread");
	if (IS_ERR(disp_irq_log_task))
		DDPERR(" can not create disp_irq_log_task kthread\n");

	wake_up_process(disp_irq_log_task);

	for (i = 0; i < DISP_MODULE_NUM; i++) {
		if (ddp_intr_need_enable(i) == 1) {
			ret =
			    request_irq(ddp_irq_map[i], (irq_handler_t) disp_irq_handler,
					IRQF_TRIGGER_NONE, "DISPSYS", NULL);
			if (ret) {
				DDPERR("Unable to request IRQ, request_irq fail, i=%d, irq=%d\n",
				       i, ddp_irq_map[i]);
				return ret;
			}
		}
	}

	return 0;
}
