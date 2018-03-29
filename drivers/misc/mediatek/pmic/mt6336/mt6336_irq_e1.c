/*
* Copyright (C) 2016 MediaTek Inc.
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

#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
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

#include <mach/mt_charging.h>
#include <mt-plat/charging.h>

#include "mt6336.h"
#include "mtk_pmic_common.h"

/*****************************************************************************
 * Global variable
 ******************************************************************************/
int g_chr_irq;


/*****************************************************************************
 * interrupt Setting
 ******************************************************************************/
static struct chr_interrupt_bit interrupt_status0[] = {
	CHR_S_INT_GEN(RG_INT_STATUS_CHR_VBUS_PLUGIN),
	CHR_S_INT_GEN(RG_INT_STATUS_CHR_VBUS_PLUGOUT),
	CHR_S_INT_GEN(RG_INT_STATUS_STATE_BUCK_BACKGROUND),
	CHR_S_INT_GEN(RG_INT_STATUS_STATE_BUCK_EOC),
	CHR_S_INT_GEN(RG_INT_STATUS_STATE_BUCK_PRECC0),
	CHR_S_INT_GEN(RG_INT_STATUS_STATE_BUCK_PRECC1),
	CHR_S_INT_GEN(RG_INT_STATUS_STATE_BUCK_FASTCC),
	CHR_S_INT_GEN(RG_INT_STATUS_CHR_WEAKBUS),
};

static struct chr_interrupt_bit interrupt_status1[] = {
	CHR_S_INT_GEN(RG_INT_STATUS_CHR_SYS_OVP),
	CHR_S_INT_GEN(RG_INT_STATUS_CHR_BAT_OVP),
	CHR_S_INT_GEN(RG_INT_STATUS_CHR_VBUS_OVP),
	CHR_S_INT_GEN(RG_INT_STATUS_CHR_VBUS_UVLO),
	CHR_S_INT_GEN(RG_INT_STATUS_CHR_ICHR_ITERM),
	CHR_S_INT_GEN(RG_INT_STATUS_CHIP_TEMP_OVERHEAT),
	CHR_S_INT_GEN(RG_INT_STATUS_CHIP_MBATPP_DIS_OC_DIG),
	CHR_S_INT_GEN(RG_INT_STATUS_OTG_BVALID),
};

static struct chr_interrupt_bit interrupt_status2[] = {
	CHR_S_INT_GEN(RG_INT_STATUS_OTG_VM_UVLO),
	CHR_S_INT_GEN(RG_INT_STATUS_OTG_VM_OVP),
	CHR_S_INT_GEN(RG_INT_STATUS_OTG_VBAT_UVLO),
	CHR_S_INT_GEN(RG_INT_STATUS_OTG_VM_OLP),
	CHR_S_INT_GEN(RG_INT_STATUS_FLASH_VFLA_UVLO),
	CHR_S_INT_GEN(RG_INT_STATUS_FLASH_VFLA_OVP),
	CHR_S_INT_GEN(RG_INT_STATUS_LED1_SHORT),
	CHR_S_INT_GEN(RG_INT_STATUS_LED1_OPEN),
};

static struct chr_interrupt_bit interrupt_status3[] = {
	CHR_S_INT_GEN(RG_INT_STATUS_LED2_SHORT),
	CHR_S_INT_GEN(RG_INT_STATUS_LED2_OPEN),
	CHR_S_INT_GEN(RG_INT_STATUS_FLASH_TIMEOUT),
	CHR_S_INT_GEN(RG_INT_STATUS_TORCH_TIMEOUT),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_VBUS_IN_VALID),
	CHR_S_INT_GEN(RG_INT_STATUS_WDT_TIMEOUT),
	CHR_S_INT_GEN(RG_INT_STATUS_SAFETY_TIMEOUT),
	CHR_S_INT_GEN(RG_INT_STATUS_CHR_AICC_DONE),
};

static struct chr_interrupt_bit interrupt_status4[] = {
	CHR_S_INT_GEN(RG_INT_STATUS_ADC_TEMP_HT),
	CHR_S_INT_GEN(RG_INT_STATUS_ADC_TEMP_LT),
	CHR_S_INT_GEN(RG_INT_STATUS_ADC_JEITA_HOT),
	CHR_S_INT_GEN(RG_INT_STATUS_ADC_JEITA_WARM),
	CHR_S_INT_GEN(RG_INT_STATUS_ADC_JEITA_COOL),
	CHR_S_INT_GEN(RG_INT_STATUS_ADC_JEITA_COLD),
	CHR_S_INT_GEN(RG_INT_STATUS_VBUS_SOFT_OVP_H),
	CHR_S_INT_GEN(RG_INT_STATUS_VBUS_SOFT_OVP_L),
};

static struct chr_interrupt_bit interrupt_status5[] = {
	CHR_S_INT_GEN(RG_INT_STATUS_CHR_BAT_RECHG),
	CHR_S_INT_GEN(RG_INT_STATUS_BAT_TEMP_H),
	CHR_S_INT_GEN(RG_INT_STATUS_BAT_TEMP_L),
	CHR_S_INT_GEN(RG_INT_STATUS_TYPE_C_L_MIN),
	CHR_S_INT_GEN(RG_INT_STATUS_TYPE_C_L_MAX),
	CHR_S_INT_GEN(RG_INT_STATUS_TYPE_C_H_MIN),
	CHR_S_INT_GEN(RG_INT_STATUS_TYPE_C_H_MAX),
	CHR_S_INT_GEN(RG_INT_STATUS_TYPE_C_CC_IRQ),
};

static struct chr_interrupt_bit interrupt_status6[] = {
	CHR_S_INT_GEN(RG_INT_STATUS_TYPE_C_PD_IRQ),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_PE_STATUS),
	CHR_S_INT_GEN(RG_INT_STATUS_BC12_V2P7_TIMEOUT),
	CHR_S_INT_GEN(RG_INT_STATUS_BC12_V3P2_TIMEOUT),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_BC12_STATUS),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_TOP_RST_SW),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_TOP_RST_GLOBAL),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_TOP_RST_LONG_PRESS),
};

static struct chr_interrupt_bit interrupt_status7[] = {
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_TOP_RST_WDT),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_PLUGOUT_PULSEB_RISING),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_PLUGOUT_PULSEB_LEVEL),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_PLUGIN_PULSEB),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_TOP_RST_SHIP),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_TOP_RST_BAT_OC),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_TOP_RST_BAT_DEAD),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_BUCK_MODE),
};

static struct chr_interrupt_bit interrupt_status8[] = {
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_LOWQ_MODE),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_SHIP_MODE),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_BAT_OC_MODE),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_BAT_DEAD_MODE),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_RST_SW_MODE),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_RST_GLOBAL_MODE),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_RST_WDT_MODE),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_RST_LONG_PRESS_MODE),
};

static struct chr_interrupt_bit interrupt_status9[] = {
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_CHR_SUSPEND_STATE),
	CHR_S_INT_GEN(RG_INT_STATUS_DD_SWCHR_BUCK_PROTECT_STATE),
	CHR_S_INT_GEN(NO_USE),
	CHR_S_INT_GEN(NO_USE),
	CHR_S_INT_GEN(NO_USE),
	CHR_S_INT_GEN(NO_USE),
	CHR_S_INT_GEN(NO_USE),
	CHR_S_INT_GEN(NO_USE),
};

struct chr_interrupts mt6336_interrupts[] = {
	CHR_M_INTS_GEN(MT6336_PMIC_INT_STATUS0, MT6336_PMIC_INT_RAW_STATUS0,
		MT6336_PMIC_INT_CON0, MT6336_PMIC_INT_MASK_CON0, interrupt_status0),
	CHR_M_INTS_GEN(MT6336_PMIC_INT_STATUS1, MT6336_PMIC_INT_RAW_STATUS1,
		MT6336_PMIC_INT_CON1, MT6336_PMIC_INT_MASK_CON1, interrupt_status1),
	CHR_M_INTS_GEN(MT6336_PMIC_INT_STATUS2, MT6336_PMIC_INT_RAW_STATUS2,
		MT6336_PMIC_INT_CON2, MT6336_PMIC_INT_MASK_CON2, interrupt_status2),
	CHR_M_INTS_GEN(MT6336_PMIC_INT_STATUS3, MT6336_PMIC_INT_RAW_STATUS3,
		MT6336_PMIC_INT_CON3, MT6336_PMIC_INT_MASK_CON3, interrupt_status3),
	CHR_M_INTS_GEN(MT6336_PMIC_INT_STATUS4, MT6336_PMIC_INT_RAW_STATUS4,
		MT6336_PMIC_INT_CON4, MT6336_PMIC_INT_MASK_CON4, interrupt_status4),
	CHR_M_INTS_GEN(MT6336_PMIC_INT_STATUS5, MT6336_PMIC_INT_RAW_STATUS5,
		MT6336_PMIC_INT_CON5, MT6336_PMIC_INT_MASK_CON5, interrupt_status5),
	CHR_M_INTS_GEN(MT6336_PMIC_INT_STATUS6, MT6336_PMIC_INT_RAW_STATUS6,
		MT6336_PMIC_INT_CON6, MT6336_PMIC_INT_MASK_CON6, interrupt_status6),
	CHR_M_INTS_GEN(MT6336_PMIC_INT_STATUS7, MT6336_PMIC_INT_RAW_STATUS7,
		MT6336_PMIC_INT_CON7, MT6336_PMIC_INT_MASK_CON7, interrupt_status7),
	CHR_M_INTS_GEN(MT6336_PMIC_INT_STATUS8, MT6336_PMIC_INT_RAW_STATUS8,
		MT6336_PMIC_INT_CON8, MT6336_PMIC_INT_MASK_CON8, interrupt_status8),
	CHR_M_INTS_GEN(MT6336_PMIC_INT_STATUS9, MT6336_PMIC_INT_RAW_STATUS9,
		MT6336_PMIC_INT_CON9, MT6336_PMIC_INT_MASK_CON9, interrupt_status9),
};

unsigned int mt6336_interrupts_size = ARRAY_SIZE(mt6336_interrupts);
/* prior irq CC & PD irq index, CC & PD have the same i index */
unsigned char cc_i = MT6336_INT_TYPE_C_CC_IRQ / CHR_INT_WIDTH;
unsigned char cc_j = MT6336_INT_TYPE_C_CC_IRQ % CHR_INT_WIDTH;
unsigned char pd_i = MT6336_INT_TYPE_C_PD_IRQ / CHR_INT_WIDTH;
unsigned char pd_j = MT6336_INT_TYPE_C_PD_IRQ % CHR_INT_WIDTH;

/*****************************************************************************
 * MT6336 Interrupt service
 ******************************************************************************/
static DEFINE_MUTEX(mt6336_mutex);
struct task_struct *mt6336_thread_handle;

#if !defined CONFIG_HAS_WAKELOCKS
struct wakeup_source mt6336Thread_lock;
#else
struct wake_lock mt6336Thread_lock;
#endif

void wake_up_mt6336(void)
{
	PMICLOG("[%s]\n", __func__);
	if (mt6336_thread_handle != NULL) {
		pmic_wake_lock(&mt6336Thread_lock);
		wake_up_process(mt6336_thread_handle);
	} else {
		pr_err(MT6336TAG "[%s] mt6336_thread_handle not ready\n", __func__);
		return;
	}
}

irqreturn_t mt6336_eint_irq(int irq, void *desc)
{
	disable_irq_nosync(irq);
	PMICLOG("[%s] disable PMIC irq\n", __func__);
	wake_up_mt6336();
	return IRQ_HANDLED;
}

void mt6336_enable_interrupt(unsigned int intNo, char *str)
{
	unsigned int shift, no;

	shift = intNo / CHR_INT_WIDTH;
	no = intNo % CHR_INT_WIDTH;

	if (shift >= ARRAY_SIZE(mt6336_interrupts)) {
		pr_err(MT6336TAG "[mt6336_enable_interrupt] fail intno=%d \r\n", intNo);
		return;
	}

	PMICLOG("[mt6336_enable_interrupt] intno=%d str=%s shf=%d no=%d [0x%x]=0x%x\r\n",
		intNo, str, shift, no, mt6336_interrupts[shift].en,
		mt6336_get_register_value(mt6336_interrupts[shift].en));

	mt6336_config_interface(mt6336_interrupts[shift].set, 0x1, 0x1, no);

	PMICLOG("[mt6336_enable_interrupt] after [0x%x]=0x%x\r\n",
		mt6336_interrupts[shift].en, mt6336_get_register_value(mt6336_interrupts[shift].en));
}

void mt6336_disable_interrupt(unsigned int intNo, char *str)
{
	unsigned int shift, no;

	shift = intNo / CHR_INT_WIDTH;
	no = intNo % CHR_INT_WIDTH;

	if (shift >= ARRAY_SIZE(mt6336_interrupts)) {
		pr_err(MT6336TAG "[mt6336_disable_interrupt] fail intno=%d \r\n", intNo);
		return;
	}

	PMICLOG("[mt6336_disable_interrupt] intno=%d str=%s shf=%d no=%d [0x%x]=0x%x\r\n",
		intNo, str, shift, no, mt6336_interrupts[shift].en,
		mt6336_get_register_value(mt6336_interrupts[shift].en));

	mt6336_config_interface(mt6336_interrupts[shift].clear, 0x1, 0x1, no);

	PMICLOG("[mt6336_disable_interrupt] after [0x%x]=0x%x\r\n",
		mt6336_interrupts[shift].en, mt6336_get_register_value(mt6336_interrupts[shift].en));
}

void mt6336_mask_interrupt(unsigned int intNo, char *str)
{
	unsigned int shift, no;

	shift = intNo / CHR_INT_WIDTH;
	no = intNo % CHR_INT_WIDTH;

	if (shift >= ARRAY_SIZE(mt6336_interrupts)) {
		pr_err(MT6336TAG "[mt6336_mask_interrupt] fail intno=%d \r\n", intNo);
		return;
	}

	PMICLOG("[mt6336_mask_interrupt] intno=%d str=%s shf=%d no=%d [0x%x]=0x%x\r\n",
		intNo, str, shift, no, mt6336_interrupts[shift].mask,
		mt6336_get_register_value(mt6336_interrupts[shift].mask));

	mt6336_config_interface(mt6336_interrupts[shift].mask_set, 0x1, 0x1, no);

	PMICLOG("[mt6336_mask_interrupt] after [0x%x]=0x%x\r\n",
		mt6336_interrupts[shift].mask, mt6336_get_register_value(mt6336_interrupts[shift].mask));
}

void mt6336_unmask_interrupt(unsigned int intNo, char *str)
{
	unsigned int shift, no;

	shift = intNo / CHR_INT_WIDTH;
	no = intNo % CHR_INT_WIDTH;

	if (shift >= ARRAY_SIZE(mt6336_interrupts)) {
		pr_err(MT6336TAG "[mt6336_mask_interrupt] fail intno=%d \r\n", intNo);
		return;
	}

	PMICLOG("[mt6336_mask_interrupt] intno=%d str=%s shf=%d no=%d [0x%x]=0x%x\r\n",
		intNo, str, shift, no, mt6336_interrupts[shift].mask,
		mt6336_get_register_value(mt6336_interrupts[shift].mask));

	mt6336_config_interface(mt6336_interrupts[shift].mask_clear, 0x1, 0x1, no);

	PMICLOG("[mt6336_mask_interrupt] after [0x%x]=0x%x\r\n",
		mt6336_interrupts[shift].mask, mt6336_get_register_value(mt6336_interrupts[shift].mask));
}

void mt6336_register_interrupt_callback(unsigned int intNo, void (EINT_FUNC_PTR) (void))
{
	unsigned int shift, no;

	shift = intNo / CHR_INT_WIDTH;
	no = intNo % CHR_INT_WIDTH;

	if (shift >= ARRAY_SIZE(mt6336_interrupts)) {
		pr_err(MT6336TAG "[mt6336_register_interrupt_callback] fail intno=%d\r\n", intNo);
		return;
	}

	PMICLOG("[mt6336_register_interrupt_callback] intno=%d\r\n", intNo);

	mt6336_interrupts[shift].interrupts[no].callback = EINT_FUNC_PTR;
}

static void mt6336_int_handler(void)
{
	unsigned char i, j;
	unsigned char int_status_vals[mt6336_interrupts_size];
	unsigned short cc_status;
	unsigned short pd_status;
	unsigned int ret;

	ret = mt6336_read_bytes(mt6336_interrupts[0].address, int_status_vals, mt6336_interrupts_size);
	for (i = 0; i < ARRAY_SIZE(mt6336_interrupts); i++) {
		pr_err(MT6336TAG "[CHR_INT] %d status[0x%x]=0x%x [0x%x]=0x%x en[0x%x]=0x%x mask[0x%x]=0x%x\n",
			i,
			mt6336_interrupts[i].address, int_status_vals[i],
			mt6336_interrupts[i].raw_address, mt6336_get_register_value(mt6336_interrupts[i].raw_address),
			mt6336_interrupts[i].en, mt6336_get_register_value(mt6336_interrupts[i].en),
			mt6336_interrupts[i].mask, mt6336_get_register_value(mt6336_interrupts[i].mask)
			);

		for (j = 0; j < CHR_INT_WIDTH; j++) {
			/* handle CC & PD irq first, CC & PD are at the same status register */
			cc_status = mt6336_get_register_value(mt6336_interrupts[cc_i].address);
			if (cc_status & (1 << cc_j)) {
				if (mt6336_interrupts[cc_i].interrupts[cc_j].callback != NULL) {
					mt6336_interrupts[cc_i].interrupts[cc_j].callback();
					mt6336_interrupts[cc_i].interrupts[cc_j].times++;
				}
				ret = mt6336_config_interface(mt6336_interrupts[cc_i].address, 0x1, 0x1, cc_j);
			}
			pd_status = mt6336_get_register_value(mt6336_interrupts[pd_i].address);
			if (pd_status & (1 << pd_j)) {
				if (mt6336_interrupts[pd_i].interrupts[pd_j].callback != NULL) {
					mt6336_interrupts[pd_i].interrupts[pd_j].callback();
					mt6336_interrupts[pd_i].interrupts[pd_j].times++;
				}
				ret = mt6336_config_interface(mt6336_interrupts[pd_i].address, 0x1, 0x1, pd_j);
			}
			if ((i == cc_i && j == cc_j) || (i == pd_i && j == pd_j))
				continue;
			/* handle other irqs */
			if ((int_status_vals[i]) & (1 << j)) {
				PMICLOG("[CHR_INT][%s]\n", mt6336_interrupts[i].interrupts[j].name);
				if (mt6336_interrupts[i].interrupts[j].callback != NULL) {
					mt6336_interrupts[i].interrupts[j].callback();
					mt6336_interrupts[i].interrupts[j].times++;
				}
				ret = mt6336_config_interface(mt6336_interrupts[i].address, 0x1, 0x1, j);
			}
		}
	}
}

/* interrupt service */
int mt6336_thread_kthread(void *x)
{
	unsigned int i;
	unsigned int int_status_val = 0;
	struct sched_param param = {.sched_priority = 98 };

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	PMICLOG("[CHR_INT] enter\n");

	/* Run on a process content */
	while (1) {
		mutex_lock(&mt6336_mutex);

		mt6336_int_handler();
		for (i = 0; i < ARRAY_SIZE(mt6336_interrupts); i++) {
			int_status_val = mt6336_get_register_value(mt6336_interrupts[i].address);
			PMICLOG("[CHR_INT] %d after, status[0x%x]=0x%x\n", i,
				mt6336_interrupts[i].address, int_status_val);
		}
		mdelay(1);

		mutex_unlock(&mt6336_mutex);
		pmic_wake_unlock(&mt6336Thread_lock);

		set_current_state(TASK_INTERRUPTIBLE);
		if (g_chr_irq != 0)
			enable_irq(g_chr_irq);
		schedule();
	}

	return 0;
}

void MT6336_EINT_SETTING(void)
{
	struct device_node *node = NULL;
	int ret = 0;

	/* create MT6336 irq thread handler*/
	mt6336_thread_handle = kthread_create(mt6336_thread_kthread, (void *)NULL, "mt6336_thread");
	if (IS_ERR(mt6336_thread_handle)) {
		mt6336_thread_handle = NULL;
		pr_err(MT6336TAG "[mt6336_thread_handle] creation fails\n");
	} else {
		PMICLOG("[mt6336_thread_handle] kthread_create Done\n");
	}

	/* disable all interrupts */
	mt6336_set_register_value(MT6336_PMIC_INT_CON0_CLR, 0xfc);
	mt6336_set_register_value(MT6336_PMIC_INT_CON1_CLR, 0xff);
	mt6336_set_register_value(MT6336_PMIC_INT_CON2_CLR, 0xff);
	mt6336_set_register_value(MT6336_PMIC_INT_CON3_CLR, 0xff);
	mt6336_set_register_value(MT6336_PMIC_INT_CON4_CLR, 0xff);
	mt6336_set_register_value(MT6336_PMIC_INT_CON5_CLR, 0xff);
	mt6336_set_register_value(MT6336_PMIC_INT_CON6_CLR, 0xff);
	mt6336_set_register_value(MT6336_PMIC_INT_CON7_CLR, 0xff);
	mt6336_set_register_value(MT6336_PMIC_INT_CON8_CLR, 0xff);
	mt6336_set_register_value(MT6336_PMIC_INT_CON9_CLR, 0xff);
	/*
	mt6336_mask_interrupt(6, "mt6336");
	mt6336_mask_interrupt(12, "mt6336");
	mt6336_mask_interrupt(50, "mt6336");
	mt6336_mask_interrupt(51, "mt6336");
	mt6336_mask_interrupt(56, "mt6336");
	*/

	/* for all interrupt events, turn on interrupt module clock (default on)*/
	/*mt6336_set_flag_register_value(CLK_INTRP_CK_PDN, 0);*/

	/* register interrupt callback */
	/*
	mt6336_register_interrupt_callback(1, plugout_int_handler);
	mt6336_register_interrupt_callback(64, lowq_int_handler);
	*/

	/* enable interrupt */


	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6336_chr");
	if (node) {
		g_chr_irq = irq_of_parse_and_map(node, 0);
		ret = request_irq(g_chr_irq, (irq_handler_t) mt6336_eint_irq,
			IRQF_TRIGGER_NONE, "mt6336-eint", NULL);
		if (ret > 0)
			pr_err(MT6336TAG "EINT IRQ LINENNOT AVAILABLE\n");
	} else
		pr_err(MT6336TAG "can't find compatible node\n");
}

MODULE_AUTHOR("Jeter Chen");
MODULE_DESCRIPTION("MT PMIC Interrupt Driver");
MODULE_LICENSE("GPL");
