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

#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/sched/clock.h>

#if defined(CONFIG_MTK_SMI_EXT)
#include <smi_public.h>
#endif

/* #include <mach/mt_irq.h> */
#include "disp_drv_platform.h"	/* must be at the top-most */
#include "ddp_log.h"
#include "ddp_debug.h"
#include "ddp_reg.h"
#include "ddp_irq.h"
#include "ddp_aal.h"
#include "ddp_drv.h"
#include "disp_helper.h"
#include "ddp_dsi.h"
#include "ddp_postmask.h"
#include "disp_drv_log.h"
#include "primary_display.h"
//#include "smi_debug.h"
#include "disp_lowpower.h"
#include "layering_rule.h"

#include <asm/arch_timer.h>

/* IRQ log print kthread */
static struct task_struct *disp_irq_log_task;
static wait_queue_head_t disp_irq_log_wq;
static unsigned long disp_irq_log_module;
static int disp_irq_rdma_underflow;
static int irq_init;

static unsigned int cnt_rdma_underflow[2];
static unsigned int cnt_rdma_abnormal[2];
static unsigned int cnt_ovl_underflow[OVL_NUM];
static unsigned int cnt_wdma_underflow[2];
static unsigned int cnt_postmask_abnormal;
static unsigned int cnt_postmask_underflow;

unsigned long long rdma_start_time[2] = { 0 };
unsigned long long rdma_end_time[2] = { 0 };

#define DISP_MAX_IRQ_CALLBACK   10

static DDP_IRQ_CALLBACK
	irq_module_callback_table[DISP_MODULE_NUM][DISP_MAX_IRQ_CALLBACK];
static DDP_IRQ_CALLBACK irq_callback_table[DISP_MAX_IRQ_CALLBACK];

atomic_t ESDCheck_byCPU = ATOMIC_INIT(0);

/* dsi read by cpu should keep esd_check_bycmdq = 0.  */
/* dsi read by cmdq should keep esd_check_bycmdq = 1. */
static atomic_t esd_check_bycmdq = ATOMIC_INIT(1);

void disp_irq_esd_cust_bycmdq(int enable)
{
	atomic_set(&esd_check_bycmdq, enable);
}

int disp_irq_esd_cust_get(void)
{
	return atomic_read(&esd_check_bycmdq);
}

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
	DDPMSG("register callback on %d\n", i);
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
		DDPERR("%p which was not registered\n",
			cb);
		return -1;
	}
	return 0;
}

int disp_register_module_irq_callback(enum DISP_MODULE_ENUM module,
	DDP_IRQ_CALLBACK cb)
{
	int i;

	if (module >= DISP_MODULE_NUM) {
		DDPERR("Register IRQ with invalid module ID. module=%d\n",
			module);
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
	if (i < DISP_MAX_IRQ_CALLBACK)
		return 0;

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

int disp_unregister_module_irq_callback(enum DISP_MODULE_ENUM module,
	DDP_IRQ_CALLBACK cb)
{
	int i;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_module_callback_table[module][i] == cb) {
			irq_module_callback_table[module][i] = NULL;
			break;
		}
	}
	if (i == DISP_MAX_IRQ_CALLBACK) {
		DDPERR(
			"Try to unregister callback function with was not registered. module=%d cb=%p\n",
		     module, cb);
		return -1;
	}
	return 0;
}

void disp_invoke_irq_callbacks(enum DISP_MODULE_ENUM module, unsigned int param)
{
	int i;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {

		if (irq_callback_table[i])
			irq_callback_table[i](module, param);

		if (irq_module_callback_table[module][i])
			irq_module_callback_table[module][i] (module, param);
	}
}

/* TODO:  move each irq to module driver */
unsigned int rdma_start_irq_cnt[2] = { 0, 0 };
unsigned int rdma_done_irq_cnt[2] = { 0, 0 };
unsigned int rdma_underflow_irq_cnt[2] = { 0, 0 };
unsigned int rdma_targetline_irq_cnt[2] = { 0, 0 };

irqreturn_t disp_irq_handler(int irq, void *dev_id)
{
	enum DISP_MODULE_ENUM module = DISP_MODULE_UNKNOWN;
	unsigned int reg_val = 0;
	unsigned int index = 0;
	unsigned int m_id = 0;
	unsigned int reg_temp_val = 0;

	//DDPDBG("%s:%d\n", __func__, __LINE__);

	if (irq == ddp_get_module_irq(DISP_MODULE_DSI0)) {
		if (ddp_get_module_irq(DISP_MODULE_DSI0) == irq) {
			index = 0;
			module = DISP_MODULE_DSI0;
		} else if (ddp_get_module_irq(DISP_MODULE_DSI1) == irq) {
			index = 1;
			module = DISP_MODULE_DSI1;
		}

		if (module == DISP_MODULE_DSI0)
			reg_val =
			(DISP_REG_GET(DISPSYS_DSI0_BASE + 0xC) & 0xffff);
		else
			reg_val =
			(DISP_REG_GET(DISPSYS_DSI1_BASE + 0xC) & 0xffff);

		DDPIRQ("%s irq_status = 0x%x\n",
			ddp_get_module_name(module), reg_val);
		reg_temp_val = reg_val;
		/* rd_rdy don't clear and wait for ESD &
		 * Read LCM will clear the bit.
		 */
		if (disp_irq_esd_cust_get() == 1)
			reg_temp_val = reg_val & 0xfffe;
		if (module == DISP_MODULE_DSI0)
			DISP_CPU_REG_SET(DISPSYS_DSI0_BASE + 0xC,
				~reg_temp_val);
		else
			DISP_CPU_REG_SET(DISPSYS_DSI1_BASE + 0xC,
				~reg_temp_val);

	} else if (irq == ddp_get_module_irq(DISP_MODULE_OVL0) ||
		irq == ddp_get_module_irq(DISP_MODULE_OVL0_2L)) {
/*
 *		not use this module
 *		irq == ddp_get_module_irq(DISP_MODULE_OVL1_2L)) {
 */
		module = disp_irq_to_module(irq);
		index = ovl_to_index(module);
		reg_val = DISP_REG_GET(DISP_REG_OVL_INTSTA +
			ovl_base_addr(module));

		DDPIRQ("%s irq_status = 0x%x\n",
			ddp_get_module_name(module), reg_val);

		if (reg_val & (1 << 0))
			DDPIRQ("IRQ: %s reg commit!\n",
				ddp_get_module_name(module));

		if (reg_val & (1 << 1))
			DDPIRQ("IRQ: %s frame done!\n",
				ddp_get_module_name(module));

		if (reg_val & (1 << 2))
			DDPERR("IRQ: %s frame underflow! cnt=%d\n",
				ddp_get_module_name(module),
				cnt_ovl_underflow[index]++);

		if (reg_val & (1 << 3))
			DDPIRQ("IRQ: %s sw reset done\n",
				ddp_get_module_name(module));

		if (reg_val & (1 << 4))
			DDPERR("IRQ: %s hw reset done\n",
				ddp_get_module_name(module));

		if (reg_val & (1 << 5))
			DDPERR("IRQ: %s-L0 not complete until EOF!\n",
			       ddp_get_module_name(module));

		if (reg_val & (1 << 6))
			DDPERR("IRQ: %s-L1 not complete until EOF!\n",
			       ddp_get_module_name(module));

		if (reg_val & (1 << 7))
			DDPERR("IRQ: %s-L2 not complete until EOF!\n",
			       ddp_get_module_name(module));

		if (reg_val & (1 << 8))
			DDPERR("IRQ: %s-L3 not complete until EOF!\n",
			       ddp_get_module_name(module));
		if (reg_val & (1 << 13))
			DDPERR("IRQ: %s abnormal SOF!\n",
				ddp_get_module_name(module));
		if (reg_val & (1 << 14))
			DDPIRQ("IRQ: %s frame start!\n",
				ddp_get_module_name(module));

		DISP_CPU_REG_SET(
			DISP_REG_OVL_INTSTA + ovl_base_addr(module), ~reg_val);
		mmprofile_log_ex(ddp_mmp_get_events()->OVL_IRQ[index],
			MMPROFILE_FLAG_PULSE, reg_val, 0);
		if (reg_val & 0x1e0)
			mmprofile_log_ex(
				ddp_mmp_get_events()->ddp_abnormal_irq,
				MMPROFILE_FLAG_PULSE,
				(index << 16) | reg_val, module);

	} else if (irq == ddp_get_module_irq(DISP_MODULE_WDMA0)) {
		index = 0;
		module = DISP_MODULE_WDMA0;

		reg_val = DISP_REG_GET(DISP_REG_WDMA_INTSTA);

		DDPIRQ("%s irq_status = 0x%x\n",
			ddp_get_module_name(module), reg_val);

		if (reg_val & (1 << 0))
			DDPIRQ("IRQ: WDMA%d frame done!\n", index);

		if (reg_val & (1 << 1)) {
			DDPERR("IRQ: WDMA%d underrun! cnt=%d\n", index,
			       cnt_wdma_underflow[index]++);

			disp_irq_log_module |= 1UL << module;
		}
		/* clear intr */
		DISP_CPU_REG_SET(DISP_REG_WDMA_INTSTA, ~reg_val);
		mmprofile_log_ex(ddp_mmp_get_events()->WDMA_IRQ[index],
			MMPROFILE_FLAG_PULSE, reg_val,
			DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE));
		if (reg_val & 0x2)
			mmprofile_log_ex(ddp_mmp_get_events()->ddp_abnormal_irq,
				MMPROFILE_FLAG_PULSE,
				(cnt_wdma_underflow[index] << 24) |
				(index << 16) | reg_val,
				module);
	} else if (irq == ddp_get_module_irq(DISP_MODULE_RDMA0)) {

		if (ddp_get_module_irq(DISP_MODULE_RDMA0) == irq) {
			index = 0;
			module = DISP_MODULE_RDMA0;
		}

		reg_val = DISP_REG_GET(DISP_REG_RDMA_INT_STATUS +
			index * DISP_RDMA_INDEX_OFFSET);

		DDPIRQ("%s irq_status = 0x%x\n",
			ddp_get_module_name(module), reg_val);

		if (reg_val & (1 << 0))
			DDPIRQ("IRQ: RDMA%d reg update done!\n", index);

		if (reg_val & (1 << 2)) {
			mmprofile_log_ex(
				ddp_mmp_get_events()->SCREEN_UPDATE[index],
				MMPROFILE_FLAG_END, reg_val,
				DISP_REG_GET(DISPSYS_RDMA0_BASE + 0x4));
			rdma_end_time[index] = sched_clock();
			DDPIRQ("IRQ: RDMA%d frame done!\n", index);
			rdma_done_irq_cnt[index]++;
		}
		if (reg_val & (1 << 1)) {
			mmprofile_log_ex(
				ddp_mmp_get_events()->SCREEN_UPDATE[index],
				MMPROFILE_FLAG_START, reg_val,
				DISP_REG_GET(DISPSYS_RDMA0_BASE + 0x4));
			rdma_start_time[index] = sched_clock();
			DDPIRQ("IRQ: RDMA%d frame start!\n", index);
			rdma_start_irq_cnt[index]++;
		}
		if (reg_val & (1 << 3)) {
			mmprofile_log_ex(
				ddp_mmp_get_events()->SCREEN_UPDATE[index],
				MMPROFILE_FLAG_PULSE, reg_val,
				DISP_REG_GET(DISPSYS_RDMA0_BASE + 0x4));

			DDPERR("IRQ: RDMA%d abnormal! cnt=%d\n",
				index, cnt_rdma_abnormal[index]++);
			disp_irq_log_module |= 1UL << module;

		}
		if (reg_val & (1 << 4)) {

			mmprofile_log_ex(
				ddp_mmp_get_events()->SCREEN_UPDATE[index],
				MMPROFILE_FLAG_PULSE, reg_val, 1);

			cnt_rdma_underflow[index]++;
			DDPERR(
				"IRQ: RDMA%d underflow! cnt=%d pix(%d,%d,%d,%d)\n",
					index, cnt_rdma_underflow[index],
			       DISP_REG_GET(DISP_REG_RDMA_IN_P_CNT +
					    DISP_RDMA_INDEX_OFFSET * index),
			       DISP_REG_GET(DISP_REG_RDMA_IN_LINE_CNT +
					    DISP_RDMA_INDEX_OFFSET * index),
			       DISP_REG_GET(DISP_REG_RDMA_OUT_P_CNT +
					    DISP_RDMA_INDEX_OFFSET * index),
			       DISP_REG_GET(DISP_REG_RDMA_OUT_LINE_CNT +
					    DISP_RDMA_INDEX_OFFSET * index));
			disp_irq_log_module |= 1UL << module;
			rdma_underflow_irq_cnt[index]++;
			disp_irq_rdma_underflow = 1;
		}
		if (reg_val & (1 << 5)) {
			DDPIRQ("IRQ: RDMA%d target line!\n", index);
			rdma_targetline_irq_cnt[index]++;
		}
		/* clear intr */
		DISP_CPU_REG_SET(DISP_REG_RDMA_INT_STATUS +
			index * DISP_RDMA_INDEX_OFFSET, ~reg_val);
		mmprofile_log_ex(ddp_mmp_get_events()->RDMA_IRQ[index],
			MMPROFILE_FLAG_PULSE, reg_val, 0);
		if (reg_val & 0x18)
			mmprofile_log_ex(ddp_mmp_get_events()->ddp_abnormal_irq,
				MMPROFILE_FLAG_PULSE,
				(rdma_underflow_irq_cnt[index] << 24) |
				(index << 16) | reg_val, module);

	} else if (irq == ddp_get_module_irq(DISP_MODULE_MUTEX)) {
		/* mutex0: perimary disp */
		/* mutex1: sub disp */
		/* mutex2: aal */
		unsigned int reg_val1 = 0;
		unsigned int reg_val2 = 0;

		module = DISP_MODULE_MUTEX;
		reg_val = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA) &
			DISP_MUTEX_INT_MSK;
		reg_val1 = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA_1);
		reg_val2 = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTEN);

		DDPIRQ("%s,irq_status=0x%x,irq_status1=0x%x mutex_INTEN=0x%x\n",
			ddp_get_module_name(module),
			reg_val, reg_val1, reg_val2);

		for (m_id = 0; m_id < DISP_MUTEX_DDP_COUNT; m_id++) {
			if (reg_val & (0x1 << m_id)) {
				DDPIRQ("IRQ: mutex%d sof!\n", m_id);
				mmprofile_log_ex(
					ddp_mmp_get_events()->MUTEX_IRQ[m_id],
					MMPROFILE_FLAG_PULSE, reg_val, 0);
			}
			if (reg_val & (0x1 << (m_id + DISP_MUTEX_TOTAL))) {
				DDPIRQ("IRQ: mutex%d eof!\n", m_id);
				mmprofile_log_ex(
					ddp_mmp_get_events()->MUTEX_IRQ[m_id],
					MMPROFILE_FLAG_PULSE, reg_val, 1);
			}
		}
		DISP_CPU_REG_SET(DISP_REG_CONFIG_MUTEX_INTSTA, ~reg_val);
		DISP_CPU_REG_SET(DISP_REG_CONFIG_MUTEX_INTSTA_1, ~reg_val1);
	} else if (irq == ddp_get_module_irq(DISP_MODULE_AAL0)) {
		module = DISP_MODULE_AAL0;
		reg_val = DISP_REG_GET(DISP_AAL_INTSTA);
		disp_aal_on_end_of_frame();
	} else if (irq == ddp_get_module_irq(DISP_MODULE_CCORR0)) {
		module = DISP_MODULE_CCORR0;
		reg_val = DISP_REG_GET(DISP_REG_CCORR_INTSTA);
		disp_ccorr_on_end_of_frame();
	} else if (irq == ddp_get_module_irq(DISP_MODULE_CONFIG)) {
		/* MMSYS error intr */
		reg_val = DISP_REG_GET(DISP_REG_CONFIG_MMSYS_INTSTA) & 0x7;
		if (reg_val & (1 << 0))
			DDPERR("MMSYS to MFG APB TX Error!\n");

		if (reg_val & (1 << 1))
			DDPERR("MMSYS to MJC APB TX Error!\n");

		if (reg_val & (1 << 2))
			DDPERR("PWM APB TX Error!\n");

		DISP_CPU_REG_SET(DISP_REG_CONFIG_MMSYS_INTSTA, ~reg_val);
	} else if (irq == ddp_get_module_irq(DISP_MODULE_POSTMASK0)) {
		module = DISP_MODULE_POSTMASK0;
		reg_val = DISP_REG_GET(DISP_REG_POSTMASK_INTSTA +
				postmask_base_addr(module));

		DDPIRQ("%s irq_status = 0x%x\n",
			ddp_get_module_name(module), reg_val);

		if (reg_val & (1 << 0))
			DDPIRQ("IRQ: %s input frame end!\n",
				ddp_get_module_name(module));

		if (reg_val & (1 << 1))
			DDPIRQ("IRQ: %s output frame end!\n",
				ddp_get_module_name(module));

		if (reg_val & (1 << 2))
			DDPIRQ("IRQ: %s frame start!\n",
				ddp_get_module_name(module));

		if (reg_val & (1 << 4)) {
			DDPERR("IRQ: %s abnormal SOF! cnt=%d\n",
					ddp_get_module_name(module),
					cnt_postmask_abnormal);

			cnt_postmask_abnormal++;
		}

		if (reg_val & (1 << 8)) {
			DDPERR("IRQ: %s frame underflow! cnt=%d\n",
			       ddp_get_module_name(module),
			       cnt_postmask_underflow);

			cnt_postmask_underflow++;
		}

		DISP_CPU_REG_SET(DISP_REG_POSTMASK_INTSTA +
					postmask_base_addr(module), ~reg_val);
		mmprofile_log_ex(ddp_mmp_get_events()->POSTMASK_IRQ,
					MMPROFILE_FLAG_PULSE, reg_val, 0);
		if (reg_val & 0x110)
			mmprofile_log_ex(ddp_mmp_get_events()->ddp_abnormal_irq,
					 MMPROFILE_FLAG_PULSE, reg_val, module);
	} else {
		module = DISP_MODULE_UNKNOWN;
		reg_val = 0;
		DDPERR("invalid irq=%d\n ", irq);
	}

	disp_invoke_irq_callbacks(module, reg_val);

	if (disp_irq_log_module != 0)
		wake_up_interruptible(&disp_irq_log_wq);

	mmprofile_log_ex(ddp_mmp_get_events()->DDP_IRQ,
		MMPROFILE_FLAG_PULSE, module, reg_val);
	return IRQ_HANDLED;
}

static void disp_irq_rdma_underflow_aee_trigger(void)
{
	static unsigned long long last_timer;
	static unsigned int considerable_cnt;

	if (disp_irq_rdma_underflow) {
		/* Request highest dvfs */
		#ifdef MTK_FB_MMDVFS_SUPPORT
		primary_display_request_dvfs_perf(0,
				HRT_LEVEL_LEVEL2);
		#endif

		if (disp_helper_get_option(DISP_OPT_RDMA_UNDERFLOW_AEE)) {
			/* Just count underflow which happens more frequently */
			if (last_timer != 0) {
				unsigned long long freq = 1000 * 1000000;

				do_div(freq, sched_clock() - last_timer);
				if (freq > 0)
					considerable_cnt++;
				else
					considerable_cnt = 0;
			}
			/* Should trigger AEE as */
			/* more than 5 times continuous underflow happens */
			/* TODO: need more precise data from test */
			/*need trigger aee when RDMA underflow*/
			/*trigger sspm to collect SMI, EMI debug info */
			/*increase cnt to 20 to avoid too many underflow aee*/
			if (considerable_cnt >= 20) {
				primary_display_diagnose(__func__, __LINE__);
#if 0	/*SHANG: TODO: wait smi offer this API */
				smi_dumpDebugMsg();
#endif
				DDPAEE("RDMA0 underflow!cnt=%d,sys_tim=%u\n",
					cnt_rdma_underflow[0],
					(u32)arch_counter_get_cntvct());
				considerable_cnt = 0;
			}
			last_timer = sched_clock();
		}
		disp_irq_rdma_underflow = 0;
	}

}

static int disp_irq_log_kthread_func(void *data)
{
	unsigned int i = 0;

	while (1) {
		wait_event_interruptible(disp_irq_log_wq, disp_irq_log_module);
		DDPMSG("%s dump intr register: disp_irq_log_module=%ld\n",
		       __func__, disp_irq_log_module);
		for (i = 0; i < DISP_MODULE_NUM; i++) {
			if ((disp_irq_log_module & (1UL << i)) != 0)
				ddp_dump_reg(i);

		}
		disp_irq_log_module = 0;

		/* rdma underflow trigger aee */
		disp_irq_rdma_underflow_aee_trigger();
	}
	return 0;
}


int disp_init_irq(void)
{
	if (irq_init)
		return 0;

	irq_init = 1;
	DDPMSG("%s\n", __func__);

	/* create irq log thread */
	init_waitqueue_head(&disp_irq_log_wq);
	disp_irq_log_task = kthread_create(disp_irq_log_kthread_func,
		NULL, "ddp_irq_log_kthread");
	if (IS_ERR(disp_irq_log_task))
		DDPERR(" can not create disp_irq_log_task kthread\n");

	/* wake_up_process(disp_irq_log_task); */
	return 0;
}
