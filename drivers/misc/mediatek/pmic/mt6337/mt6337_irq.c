/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#endif
#include <linux/uaccess.h>

/*#include <mach/eint.h> TBD*/
#include <mach/mt_pmic_wrap.h>
#include "mt6337.h"
#include "mtk_pmic_common.h"

#include <mt-plat/aee.h>

/*---IPI Mailbox define---*/
/*
#define IPIMB
*/

/*****************************************************************************
 * Global variable
 ******************************************************************************/
int g_mt6337_irq;
unsigned int g_eint_mt6337_num = 177;
unsigned int g_cust_eint_mt6337_debounce_cn = 1;
unsigned int g_cust_eint_mt6337_type = 4;
unsigned int g_cust_eint_mt6337_debounce_en = 1;

/*****************************************************************************
 * PMIC extern function
 ******************************************************************************/

#define PMIC_DEBUG_PR_DBG

/*****************************************************************************
 * interrupt Setting
 ******************************************************************************/
static struct mt6337_interrupt_bit interrupt_status0[] = {
	MT6337_S_INT_GEN(RG_INT_EN_THR_H),
	MT6337_S_INT_GEN(RG_INT_EN_THR_L),
	MT6337_S_INT_GEN(RG_INT_EN_AUDIO),
	MT6337_S_INT_GEN(RG_INT_EN_MAD),
	MT6337_S_INT_GEN(RG_INT_EN_ACCDET),
	MT6337_S_INT_GEN(RG_INT_EN_ACCDET_EINT),
	MT6337_S_INT_GEN(RG_INT_EN_ACCDET_EINT1),
	MT6337_S_INT_GEN(RG_INT_EN_ACCDET_NEGV),
	MT6337_S_INT_GEN(RG_INT_EN_PMU_THR),
	MT6337_S_INT_GEN(RG_INT_EN_LDO_VA18_OC),
	MT6337_S_INT_GEN(RG_INT_EN_LDO_VA25_OC),
	MT6337_S_INT_GEN(RG_INT_EN_LDO_VA18_PG),
	MT6337_S_INT_GEN(NO_USE),/*RG_INT_EN_CON0*/
	MT6337_S_INT_GEN(NO_USE),/*RG_INT_EN_CON0*/
	MT6337_S_INT_GEN(NO_USE),/*RG_INT_EN_CON0*/
	MT6337_S_INT_GEN(NO_USE),/*RG_INT_EN_CON0*/
};

struct mt6337_interrupts sub_interrupts[] = {
	MT6337_M_INTS_GEN(MT6337_INT_STATUS0, MT6337_INT_CON0,
			MT6337_INT_CON1, interrupt_status0)
};

/*****************************************************************************
 * General OC Int Handler
 ******************************************************************************/
static void oc_int_handler(MT6337_IRQ_ENUM intNo, const char *int_name)
{
	PMICLOG("[general_oc_int_handler] int name=%s\n", int_name);
	switch (intNo) {
	default:
		/* issue AEE exception and disable OC interrupt */
		/* TBD: dump_exception_reg*/
		aee_kernel_warning("PMIC OC", "\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s", int_name);
		mt6337_enable_interrupt(intNo, 0, "PMIC");
		pr_err(MT6337TAG "[PMIC_INT] disable OC interrupt: %s\n", int_name);
		break;
	}
}

/*****************************************************************************
 * PMIC Interrupt service
 ******************************************************************************/
static DEFINE_MUTEX(mt6337_mutex);
struct task_struct *mt6337_thread_handle;

#if !defined CONFIG_HAS_WAKELOCKS
struct wakeup_source pmicThread_lock_mt6337;
#else
struct wake_lock pmicThread_lock_mt6337;
#endif

void wake_up_pmic_mt6337(void)
{
	PMICLOG("[%s]\n", __func__);
	if (mt6337_thread_handle != NULL) {
		pmic_wake_lock(&pmicThread_lock_mt6337);
		wake_up_process(mt6337_thread_handle);
	} else {
		pr_err(MT6337TAG "[%s] mt6337_thread_handle not ready\n", __func__);
		return;
	}
}

irqreturn_t mt6337_eint_irq(int irq, void *desc)
{
	disable_irq_nosync(irq);
	PMICLOG("[%s] disable PMIC irq\n", __func__);
	wake_up_pmic_mt6337();
	return IRQ_HANDLED;
}

void mt6337_enable_interrupt(MT6337_IRQ_ENUM intNo, unsigned int en, char *str)
{
	unsigned int shift, no;

	shift = intNo / MT6337_INT_WIDTH;
	no = intNo % MT6337_INT_WIDTH;

	if (shift >= ARRAY_SIZE(sub_interrupts)) {
		pr_err(MT6337TAG "[mt6337_enable_interrupt] fail intno=%d \r\n", intNo);
		return;
	}

	PMICLOG("[mt6337_enable_interrupt] intno=%d en=%d str=%s shf=%d no=%d [0x%x]=0x%x\r\n",
		intNo, en, str, shift, no, sub_interrupts[shift].en,
		mt6337_upmu_get_reg_value(sub_interrupts[shift].en));

	if (en == 1)
		mt6337_config_interface(sub_interrupts[shift].set, 0x1, 0x1, no);
	else if (en == 0)
		mt6337_config_interface(sub_interrupts[shift].clear, 0x1, 0x1, no);

	PMICLOG("[mt6337_enable_interrupt] after [0x%x]=0x%x\r\n",
		sub_interrupts[shift].en, mt6337_upmu_get_reg_value(sub_interrupts[shift].en));

}

void mt6337_mask_interrupt(MT6337_IRQ_ENUM intNo, char *str)
{
	unsigned int shift, no;

	shift = intNo / MT6337_INT_WIDTH;
	no = intNo % MT6337_INT_WIDTH;

	if (shift >= ARRAY_SIZE(sub_interrupts)) {
		pr_err(MT6337TAG "[mt6337_mask_interrupt] fail intno=%d \r\n", intNo);
		return;
	}

	/*---the mask in MT6337 needs 'logical not'---*/
	PMICLOG("[mt6337_mask_interrupt] intno=%d str=%s shf=%d no=%d [0x%x]=0x%x\r\n",
		intNo, str, shift, no, sub_interrupts[shift].mask,
		~(mt6337_upmu_get_reg_value(sub_interrupts[shift].mask)));

	/*---To mask interrupt needs to clear mask bit---*/
	mt6337_config_interface(sub_interrupts[shift].mask_clear, 0x1, 0x1, no);

	/*---the mask in MT6337 needs 'logical not'---*/
	PMICLOG("[mt6337_mask_interrupt] after [0x%x]=0x%x\r\n",
		sub_interrupts[shift].mask_set, ~(mt6337_upmu_get_reg_value(sub_interrupts[shift].mask)));
}

void mt6337_unmask_interrupt(MT6337_IRQ_ENUM intNo, char *str)
{
	unsigned int shift, no;

	shift = intNo / MT6337_INT_WIDTH;
	no = intNo % MT6337_INT_WIDTH;

	if (shift >= ARRAY_SIZE(sub_interrupts)) {
		pr_err(MT6337TAG "[mt6337_unmask_interrupt] fail intno=%d\r\n", intNo);
		return;
	}

	/*---the mask in MT6337 needs 'logical not'---*/
	PMICLOG("[mt6337_unmask_interrupt] intno=%d str=%s shf=%d no=%d [0x%x]=0x%x\r\n",
		intNo, str, shift, no, sub_interrupts[shift].mask,
		~(mt6337_upmu_get_reg_value(sub_interrupts[shift].mask)));

	/*---To unmask interrupt needs to set mask bit---*/
	mt6337_config_interface(sub_interrupts[shift].mask_set, 0x1, 0x1, no);

	/*---the mask in MT6337 needs 'logical not'---*/
	PMICLOG("[mt6337_mask_interrupt] after [0x%x]=0x%x\r\n",
		sub_interrupts[shift].mask_set, ~(mt6337_upmu_get_reg_value(sub_interrupts[shift].mask)));
}

void mt6337_register_interrupt_callback(MT6337_IRQ_ENUM intNo, void (EINT_FUNC_PTR) (void))
{
	unsigned int shift, no;

	shift = intNo / MT6337_INT_WIDTH;
	no = intNo % MT6337_INT_WIDTH;

	if (shift >= ARRAY_SIZE(sub_interrupts)) {
		pr_err(MT6337TAG "[mt6337_register_interrupt_callback] fail intno=%d\n", intNo);
		return;
	}

	PMICLOG("[mt6337_register_interrupt_callback] intno=%d\n", intNo);

	sub_interrupts[shift].interrupts[no].callback = EINT_FUNC_PTR;

}

#define ENABLE_MT6337_ALL_OC_IRQ 0
/* register general oc interrupt handler */
void mt6337_register_oc_interrupt_callback(MT6337_IRQ_ENUM intNo)
{
	unsigned int shift, no;

	shift = intNo / MT6337_INT_WIDTH;
	no = intNo % MT6337_INT_WIDTH;

	if (shift >= ARRAY_SIZE(sub_interrupts)) {
		pr_err(MT6337TAG "[mt6337_register_oc_interrupt_callback] fail intno=%d\n", intNo);
		return;
	}
	PMICLOG("[pmic_register_oc_interrupt_callback] intno=%d\r\n", intNo);
	sub_interrupts[shift].interrupts[no].oc_callback = oc_int_handler;
}
#if ENABLE_MT6337_ALL_OC_IRQ
/* register and enable all oc interrupt */
static void register_all_oc_interrupts(void)
{
	MT6337_IRQ_ENUM oc_interrupt = INT_LDO_VA18_OC;

	for (; oc_interrupt <= INT_LDO_VA25_OC; oc_interrupt++) {
		mt6337_register_oc_interrupt_callback(oc_interrupt);
		mt6337_enable_interrupt(oc_interrupt, 1, "PMIC");
	}
}
#endif

static void mt6337_int_handler(void)
{
	unsigned char i, j;
	unsigned int ret;

	for (i = 0; i < ARRAY_SIZE(sub_interrupts); i++) {
		unsigned int int_status_val = 0;

		/*if (sub_interrupts[i].address != 0) {*/
		int_status_val = mt6337_upmu_get_reg_value(sub_interrupts[i].address);
		pr_err(MT6337TAG "[MT6337_INT] addr[0x%x]=0x%x\n",
			sub_interrupts[i].address, int_status_val);
		for (j = 0; j < MT6337_INT_WIDTH; j++) {
			if ((int_status_val) & (1 << j)) {
				PMICLOG("[MT6337_INT][%s]\n", sub_interrupts[i].interrupts[j].name);
				sub_interrupts[i].interrupts[j].times++;

				if (sub_interrupts[i].interrupts[j].callback != NULL)
					sub_interrupts[i].interrupts[j].callback();
				if (sub_interrupts[i].interrupts[j].oc_callback != NULL) {
					sub_interrupts[i].interrupts[j].oc_callback(
						(i * MT6337_INT_WIDTH + j), sub_interrupts[i].interrupts[j].name);
				}
				ret = mt6337_config_interface(sub_interrupts[i].address, 0x1, 0x1, j);
			}
		}
		/*}*/
	}
}

int mt6337_thread_kthread(void *x)
{
	unsigned int i;
	unsigned int int_status_val = 0;
#ifdef IPIMB
#else
	unsigned int pwrap_eint_status = 0;
#endif
	struct sched_param param = {.sched_priority = 98 };

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	PMICLOG("[MT6337_INT] enter\n");

	/* Run on a process content */
	while (1) {
		mutex_lock(&mt6337_mutex);
#ifdef IPIMB
#else
		pwrap_eint_status = pmic_wrap_eint_status();
		PMICLOG("[MT6337_INT] pwrap_eint_status=0x%x\n", pwrap_eint_status);
#endif

		mt6337_int_handler();

#ifdef IPIMB
#else
		pmic_wrap_eint_clr(0x0);
#endif

		for (i = 0; i < ARRAY_SIZE(sub_interrupts); i++) {
			/*if (sub_interrupts[i].address != 0) {*/
			int_status_val = mt6337_upmu_get_reg_value(sub_interrupts[i].address);
			PMICLOG("[MT6337_INT] after ,int_status_val[0x%x]=0x%x\n",
				sub_interrupts[i].address, int_status_val);
			/*}*/
		}

		mdelay(1);

		mutex_unlock(&mt6337_mutex);
		pmic_wake_unlock(&pmicThread_lock_mt6337);

		set_current_state(TASK_INTERRUPTIBLE);
		if (g_mt6337_irq != 0)
			enable_irq(g_mt6337_irq);
		schedule();
	}

	return 0;
}

void MT6337_EINT_SETTING(void)
{
	struct device_node *node = NULL;
	int ret = 0;
	u32 ints[2] = { 0, 0 };

	mt6337_thread_handle = kthread_create(mt6337_thread_kthread, (void *)NULL, "mt6337_thread");
	if (IS_ERR(mt6337_thread_handle)) {
		mt6337_thread_handle = NULL;
		pr_err(MT6337TAG "[mt6337_thread_kthread] creation fails\n");
	} else
		PMICLOG("[mt6337_thread_kthread] kthread_create Done\n");

#if ENABLE_MT6337_ALL_OC_IRQ
	register_all_oc_interrupts();
#endif

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6337-pmic");
	if (node) {
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		g_mt6337_irq = irq_of_parse_and_map(node, 0);
		ret = request_irq(g_mt6337_irq, (irq_handler_t) mt6337_eint_irq,
			IRQF_TRIGGER_NONE, "mt6337-eint", NULL);
		if (ret > 0)
			pr_err(MT6337TAG "EINT IRQ LINENNOT AVAILABLE\n");
		enable_irq_wake(g_mt6337_irq);
	} else
		pr_err(MT6337TAG "can't find compatible node\n");

	PMICLOG("[CUST_EINT_MT6337] CUST_EINT_MT_PMIC_MT6337_NUM=%d\n", g_eint_mt6337_num);
	PMICLOG("[CUST_EINT_MT6337] CUST_EINT_PMIC_DEBOUNCE_CN=%d\n", g_cust_eint_mt6337_debounce_cn);
	PMICLOG("[CUST_EINT_MT6337] CUST_EINT_PMIC_TYPE=%d\n", g_cust_eint_mt6337_type);
	PMICLOG("[CUST_EINT_MT6337] CUST_EINT_PMIC_DEBOUNCE_EN=%d\n", g_cust_eint_mt6337_debounce_en);

}

MODULE_AUTHOR("Jimmy-YJ Huang");
MODULE_DESCRIPTION("MT PMIC Interrupt Driver");
MODULE_LICENSE("GPL");
