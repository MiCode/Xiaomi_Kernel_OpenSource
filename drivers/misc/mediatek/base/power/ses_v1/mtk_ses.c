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

/**
 * @file	mkt_ses.c
 * @brief   Driver for SupplEyeScan
 *
 */

#define __MTK_SES_C__

/*
 *=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#ifdef __KERNEL__
	#include <linux/topology.h>
	#include <mt-plat/mtk_chip.h>
	#include <mt-plat/mtk_devinfo.h>
	#include <mcupm_ipi_id.h>
	#include <mcupm_driver.h>
	#include <mtk_cpufreq_config.h>
	#include <mtk_cpufreq_hybrid.h>
	#include "mtk_ses.h"
#endif

#ifdef CONFIG_OF
	#include <linux/cpu.h>
	#include <linux/cpu_pm.h>
	#include <linux/of.h>
	#include <linux/of_irq.h>
	#include <linux/of_address.h>
	#include <linux/of_fdt.h>
	#include <mt-plat/aee.h>
#endif

#ifdef CONFIG_OF_RESERVED_MEM
	#include <of_reserved_mem.h>
#endif

/************************************************
 * Marco definition
 ************************************************/


/************************************************
 * bit operation
 ************************************************/
#undef  BIT
#define BIT(bit)	(1U << (bit))

#define MSB(range)	(1 ? range)
#define LSB(range)	(0 ? range)
/**
 * Genearte a mask wher MSB to LSB are all 0b1
 * @r:	Range in the form of MSB:LSB
 */
#define BITMASK(r)	\
	(((unsigned int) -1 >> (31 - MSB(r))) & ~((1U << LSB(r)) - 1))

/**
 * Set value at MSB:LSB. For example, BITS(7:3, 0x5A)
 * will return a value where bit 3 to bit 7 is 0x5A
 * @r:	Range in the form of MSB:LSB
 */
/* BITS(MSB:LSB, value) => Set value at MSB:LSB  */
#define BITS(r, val)	((val << LSB(r)) & BITMASK(r))
#define GET_BITS_VAL(_bits_, _val_)   \
	(((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))

/************************************************
 * Debug print
 ************************************************/

#define CPU_SES_DEBUG
#define CPU_SES_TAG	 "[CPU_SES]"

#ifdef CPU_SES_DEBUG
#define ses_debug(fmt, args...)	\
	pr_info(CPU_SES_TAG"[%s():%d]" fmt, __func__, __LINE__, ##args)
#else
#define ses_debug(fmt, args...)
#endif

/************************************************
 * REG ACCESS
 ************************************************/
#ifdef __KERNEL__
	#define ses_read(addr)	__raw_readl((void __iomem *)(addr))
	#define ses_read_field(addr, range)	\
		((ses_read(addr) & BITMASK(range)) >> LSB(range))
	#define ses_write(addr, val)	mt_reg_sync_writel(val, addr)
#endif
/**
 * Write a field of a register.
 * @addr:	Address of the register
 * @range:	The field bit range in the form of MSB:LSB
 * @val:	The value to be written to the field
 */
#define ses_write_field(addr, range, val)	\
	ses_write(addr, (ses_read(addr) & ~BITMASK(range)) | BITS(range, val))

/************************************************
 * static Variable
 ************************************************/
#define sesNum	9
#define DEVINFO_IDX_0	50

struct drp_ratio_type {
	unsigned int HiCodeRatio;
	unsigned int LoCodeRatio;
	unsigned int HiCodeVolt;
	unsigned int LoCodeVolt;
};

struct drp_ratio_type drp_ratio[sesNum];

static unsigned int ses_ByteSel = 4;

#ifndef CONFIG_FPGA_EARLY_PORTING
static unsigned int state;
#if MTK_SES_DOE
static unsigned int ses0_reg3;
static unsigned int ses1_reg3;
static unsigned int ses2_reg3;
static unsigned int ses3_reg3;
static unsigned int ses4_reg3;
static unsigned int ses5_reg3;
static unsigned int ses6_reg3;
static unsigned int ses7_reg3;
static unsigned int ses8_reg3;
static unsigned int ses0_reg2;
static unsigned int ses1_reg2;
static unsigned int ses2_reg2;
static unsigned int ses3_reg2;
static unsigned int ses4_reg2;
static unsigned int ses5_reg2;
static unsigned int ses6_reg2;
static unsigned int ses7_reg2;
static unsigned int ses8_reg2;
#endif
static unsigned int ses0_drphipct;
static unsigned int ses1_drphipct;
static unsigned int ses2_drphipct;
static unsigned int ses3_drphipct;
static unsigned int ses4_drphipct;
static unsigned int ses5_drphipct;
static unsigned int ses6_drphipct;
static unsigned int ses7_drphipct;
static unsigned int ses8_drphipct;
static unsigned int ses0_drplopct;
static unsigned int ses1_drplopct;
static unsigned int ses2_drplopct;
static unsigned int ses3_drplopct;
static unsigned int ses4_drplopct;
static unsigned int ses5_drplopct;
static unsigned int ses6_drplopct;
static unsigned int ses7_drplopct;
static unsigned int ses8_drplopct;
#endif

/************************************************
 * log dump into reserved memory for AEE
 ************************************************/

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM

/* GAT log use */
phys_addr_t ses_mem_base_phys;
phys_addr_t ses_mem_size;
phys_addr_t ses_mem_base_virt;

static int ses_reserve_memory_dump(char *buf, unsigned int log_offset)
{
	unsigned int i, value[sesNum][5], ses_node = 0, str_len = 0;
	char *aee_log_buf = (char *) __get_free_page(GFP_USER);

	if (!aee_log_buf) {
		ses_debug("unable to get free page!\n");
		return -1;
	}
	ses_debug("buf: 0x%llx, aee_log_buf: 0x%llx\n",
		(unsigned long long)buf, (unsigned long long)aee_log_buf);

	ses_debug("log_offset = %u\n", log_offset);
	if (log_offset == 0) {
		str_len +=
		 snprintf(aee_log_buf + str_len,
		 (unsigned long long)ses_mem_size - str_len,
		 "\n[Kernel Probe]\n");
	} else if (log_offset == 1) {
		str_len +=
		 snprintf(aee_log_buf + str_len,
		 (unsigned long long)ses_mem_size - str_len,
		 "\n[Kernel ??]\n");
	} else if (log_offset == 2) {
		str_len +=
		 snprintf(aee_log_buf + str_len,
		 (unsigned long long)ses_mem_size - str_len,
		 "\n[Kernel ??]\n");
	} else {
		str_len +=
		 snprintf(aee_log_buf + str_len,
		 (unsigned long long)ses_mem_size - str_len,
		 "\n[Kernel ??]\n");
	}

	for (ses_node = 0; ses_node < (sesNum - 1) ; ses_node++) {
		for (i = 0; i < 5; i++)
			value[ses_node][i] = mtk_ses_status(
				SESV6_REG0 + (0x200 * ses_node) + (i * 4));
		str_len +=
			snprintf(aee_log_buf + str_len,
			(unsigned long long)ses_mem_size - str_len,
			"\nCPU(%u) ses_reg:", ses_node);
		for (i = 0; i < 5; i++)
			str_len +=
				snprintf(aee_log_buf + str_len,
				(unsigned long long)ses_mem_size - str_len,
				" 0x%x = 0x%08x",
				SESV6_REG0 + (0x200 * ses_node) + (i * 4),
				value[ses_node][i]);
	}

	for (i = 0; i < 5; i++)
		value[(sesNum - 1)][i] = mtk_ses_status(
			SESV6_DSU_REG0 + (i * 4));
	str_len +=
		snprintf(aee_log_buf + str_len,
		(unsigned long long)ses_mem_size - str_len,
		"\nDSU(%u) ses_reg:", (sesNum - 1));
	for (i = 0; i < 5; i++)
		str_len +=
			snprintf(aee_log_buf + str_len,
			(unsigned long long)ses_mem_size - str_len,
			" 0x%x = 0x%08x",
			SESV6_DSU_REG0 + (i * 4),
			value[(sesNum - 1)][i]);
	str_len +=
		snprintf(aee_log_buf + str_len,
		(unsigned long long)ses_mem_size - str_len,
		"\nSESBG  ses_reg: 0x%x = 0x%08x\n",
		SESV6_BG_CTRL,
		mtk_ses_status(SESV6_BG_CTRL));

	if (str_len > 0)
		memcpy(buf, aee_log_buf, str_len+1);

	ses_debug("\n%s", aee_log_buf);
	ses_debug("\n%s", buf);

	free_page((unsigned long)aee_log_buf);

	return 0;
}

#define EEM_TEMPSPARE0		0x11278F20

static void ses_reserve_memory_init(unsigned int log_offset)
{
	char *buf;

	ses_mem_base_virt = 0;
	ses_mem_size = 0x80000;
	ses_mem_base_phys = ses_read(ioremap(EEM_TEMPSPARE0, 0));

	if ((char *)ses_mem_base_phys != NULL) {
		ses_mem_base_virt =
			(phys_addr_t)(uintptr_t)ioremap_wc(
			ses_mem_base_phys,
			ses_mem_size);
	}

	ses_debug("[ses] phys:0x%llx, size:0x%llx, virt:0x%llx\n",
		(unsigned long long)ses_mem_base_phys,
		(unsigned long long)ses_mem_size,
		(unsigned long long)ses_mem_base_virt);

	if ((char *)ses_mem_base_virt != NULL) {
		buf = (char *)(uintptr_t)
			(ses_mem_base_virt+0x30000+log_offset*0x1000);

		if (buf != NULL) {
			/* dump ses register status into reserved memory */
			ses_reserve_memory_dump(buf, log_offset);
		} else
			ses_debug("ses_mem_base_virt is null !\n");
	}
}

#endif
#endif

/************************************************
 * Global function definition
 ************************************************/
void mtk_ses_init(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SES_CONTROL,
		MTK_SES_INIT, 0, 0, 0, 0, 0, 0, &res);
}

void mtk_ses_trim(unsigned int value,
		unsigned int ses_node)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SES_CONTROL,
		MTK_SES_TRIM,
		value,
		ses_node, 0, 0, 0, 0, &res);
}

void mtk_ses_enable(unsigned int onOff,
		unsigned int ses_node)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SES_CONTROL,
		MTK_SES_ENABLE,
		onOff,
		ses_node, 0, 0, 0, 0, &res);
}

unsigned int mtk_ses_event_count(unsigned int ByteSel,
		unsigned int ses_node)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SES_CONTROL,
			MTK_SES_COUNT,
			ByteSel,
			ses_node, 0, 0, 0, 0, &res);
	return res.a0;
}

void mtk_ses_hwgatepct(unsigned int HwGateSel,
		unsigned int value,
		unsigned int ses_node)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SES_CONTROL,
		MTK_SES_HWGATEPCT,
		HwGateSel,
		value,
		ses_node, 0, 0, 0, &res);
}

void mtk_ses_stepstart(unsigned int HwGateSel,
		unsigned int ses_node)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SES_CONTROL,
		MTK_SES_STEPSTART,
		HwGateSel,
		ses_node, 0, 0, 0, 0, &res);
}

void mtk_ses_steptime(unsigned int value,
		unsigned int ses_node)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SES_CONTROL,
		MTK_SES_STEPTIME,
		value,
		ses_node, 0, 0, 0, 0, &res);
}

void mtk_ses_dly_filt(unsigned int value,
		unsigned int ses_node)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SES_CONTROL,
		MTK_SES_DLYFILT,
		value,
		ses_node, 0, 0, 0, 0, &res);
}

unsigned int mtk_ses_status(unsigned int value)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SES_CONTROL,
		MTK_SES_STATUS,
		value,
		0, 0, 0, 0, 0, &res);
	return res.a0;
}

int mtk_ses_volt_ratio_eb(unsigned int HiRatio,
				unsigned int LoRatio,
				unsigned int ses_node)
{
	struct cdvfs_data cdvfs_d;
	int ret = 0;

	cdvfs_d.cmd = IPI_SES_SET_VOLTAGE_DROP_RATIO;
	cdvfs_d.u.set_fv.arg[0] = ses_node;
	cdvfs_d.u.set_fv.arg[1] = HiRatio;
	cdvfs_d.u.set_fv.arg[2] = LoRatio;

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
	ret = mtk_ipi_send_compl(&mcupm_ipidev,
		CH_S_CPU_DVFS,
		IPI_SEND_POLLING,
		&cdvfs_d,
		sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE,
		2000);
#endif
#endif
	return ret;
}

int mtk_ses_volt_ratio_atf(unsigned int Volt,
				unsigned int HiRatio,
				unsigned int LoRatio,
				unsigned int ses_node)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SES_CONTROL,
		MTK_SES_VOLT_RATIO,
		Volt,
		HiRatio,
		LoRatio,
		ses_node,
		0, 0, &res);
	return res.a0;

}


/************************************************
 * set SES status by procfs interface
 ************************************************/
static ssize_t ses_init_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int init;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &init)) {
		ses_debug("bad argument!! Should input 1 arguments.\n");
		goto out;
	}

	if (init == 1)
		mtk_ses_init();

out:
	free_page((unsigned long)buf);
	return count;
}

static int ses_init_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t ses_trim_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int trim_en, value, ses_node;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u 0x%x %u", &trim_en, &value, &ses_node) != 3) {
		ses_debug("bad argument!! Should input 3 arguments.\n");
		goto out;
	}

	if (trim_en == 1)
		mtk_ses_trim(value, ses_node);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ses_trim_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0;
	unsigned int ses_node = 0;

	value = mtk_ses_status(SESV6_BG_CTRL);
		seq_printf(m, "BG en, rnsel, rdsel, icali = %u %u %u %u\n",
			GET_BITS_VAL(2:2, value),
			GET_BITS_VAL(5:3, value),
			GET_BITS_VAL(8:6, value),
			GET_BITS_VAL(13:9, value));

	for (ses_node = 0; ses_node < sesNum-1; ses_node++) {
		value = mtk_ses_status(SESV6_REG1 + (0x200 * ses_node));
		seq_printf(m, "ses_%u, voff0/1, vrefr, vrefi = %u %u %u %u\n",
			ses_node,
			GET_BITS_VAL(4:0, value),
			GET_BITS_VAL(9:5, value),
			GET_BITS_VAL(14:10, value),
			GET_BITS_VAL(19:15, value));
	}

	value = mtk_ses_status(SESV6_DSU_REG1);
		seq_printf(m, "ses_%u, voff0/1, vrefr, vrefi = %u %u %u %u\n",
			ses_node,
			GET_BITS_VAL(4:0, value),
			GET_BITS_VAL(9:5, value),
			GET_BITS_VAL(14:10, value),
			GET_BITS_VAL(19:15, value));

	return 0;
}


static int ses_enable_proc_show(struct seq_file *m, void *v)
{
	unsigned int status = 0, value, ses_node = 0;

	for (ses_node = 0; ses_node < sesNum-1; ses_node++) {
		value = mtk_ses_status(SESV6_REG2 + (0x200 * ses_node));
		status = status | ((value & 0x01) << ses_node);
	}
	value = mtk_ses_status(SESV6_DSU_REG2);
	status = status | ((value & 0x01) << ses_node);

	seq_printf(m, "%u\n", status);

	return 0;
}

static ssize_t ses_enable_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enable, ses_node;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		ses_debug("bad argument!! Should be \"0\" ~ \"511\"\n");
		goto out;
	}

	for (ses_node = 0; ses_node < sesNum; ses_node++)
		mtk_ses_enable((enable >> ses_node) & 0x01, ses_node);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ses_count_proc_show(struct seq_file *m, void *v)
{
	unsigned int status = 0;
	unsigned int ses_node = 0;

	for (ses_node = 0; ses_node < sesNum; ses_node++) {
		status = mtk_ses_event_count(ses_ByteSel,
		ses_node);

		seq_printf(m, "ses_%u ByteSel_%u count = %u\n",
			ses_node, ses_ByteSel, status);
	}
	return 0;
}

static ssize_t ses_count_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &ses_ByteSel)) {
		ses_debug("bad argument!! Should input 0~4.\n");
		goto out;
	}

out:
	free_page((unsigned long)buf);
	return count;
}

static int ses_hwgatepct_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0;
	unsigned int ses_node = 0;

	for (ses_node = 0; ses_node < sesNum-1; ses_node++) {
		value = mtk_ses_status(SESV6_REG3 + (0x200 * ses_node));
		seq_printf(m, "ses_%u, hwgatepct[0,1,2,3]= %x %x %x %x\n",
			ses_node,
			GET_BITS_VAL(11:9, value),
			GET_BITS_VAL(14:12, value),
			GET_BITS_VAL(17:15, value),
			GET_BITS_VAL(20:18, value));
	}

	value = mtk_ses_status(SESV6_DSU_REG3);
	seq_printf(m, "ses_%u, hwgatepct[0,1,2,3]= %x %x %x %x\n",
		ses_node,
		GET_BITS_VAL(11:9, value),
		GET_BITS_VAL(14:12, value),
		GET_BITS_VAL(17:15, value),
		GET_BITS_VAL(20:18, value));

	return 0;
}

static ssize_t ses_hwgatepct_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int HwGateSel = 0, value = 0, ses_node = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u %u", &HwGateSel, &value, &ses_node) != 3) {
		ses_debug("bad argument!! Should input 3 arguments.\n");
		goto out;
	}

	mtk_ses_hwgatepct(HwGateSel, value, ses_node);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ses_stepstart_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0;
	unsigned int ses_node = 0;

	for (ses_node = 0; ses_node < sesNum-1; ses_node++) {
		value = mtk_ses_status(SESV6_REG3 + (0x200 * ses_node));
		seq_printf(m, "ses_%u, stepstart from hwgatepct[%x]\n",
			ses_node,
			GET_BITS_VAL(8:7, value));

	}

	value = mtk_ses_status(SESV6_DSU_REG3);
	seq_printf(m, "ses_%u, stepstart from hwgatepct[%x]\n",
		ses_node,
		GET_BITS_VAL(8:7, value));

	return 0;
}

static ssize_t ses_stepstart_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int HwGateSel = 0, ses_node = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &HwGateSel, &ses_node) != 2) {
		ses_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_ses_stepstart(HwGateSel, ses_node);

out:
	free_page((unsigned long)buf);
	return count;
}


static int ses_steptime_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, ses_node = 0;

	for (ses_node = 0; ses_node < sesNum-1; ses_node++) {
		value = mtk_ses_status(SESV6_REG3 + (0x200 * ses_node));
		seq_printf(m, "ses_%u, steptime = %x\n",
			ses_node,
			GET_BITS_VAL(6:0, value));

	}

	value = mtk_ses_status(SESV6_DSU_REG3);
	seq_printf(m, "ses_%u, steptime = %x\n",
		ses_node,
		GET_BITS_VAL(6:0, value));

	return 0;
}

static ssize_t ses_steptime_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, ses_node = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &value, &ses_node) != 2) {
		ses_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_ses_steptime(value, ses_node);

out:
	free_page((unsigned long)buf);
	return count;
}


static int ses_dly_filt_proc_show(struct seq_file *m, void *v)
{
	unsigned int value = 0, ses_node = 0;

	for (ses_node = 0; ses_node < sesNum-1; ses_node++) {
		value = mtk_ses_status(SESV6_REG2 + (0x200 * ses_node));
		seq_printf(m, "ses_%u, dly= %u, filt[hi:lo]= %u, %u\n",
			ses_node,
			GET_BITS_VAL(6:2, value),
			GET_BITS_VAL(16:12, value),
			GET_BITS_VAL(11:7, value));
	}

	value = mtk_ses_status(SESV6_DSU_REG2);
	seq_printf(m, "ses_%u, dly= %u, filt[hi:lo]= %u, %u\n",
		ses_node,
		GET_BITS_VAL(6:2, value),
		GET_BITS_VAL(16:12, value),
		GET_BITS_VAL(11:7, value));

	return 0;
}

static ssize_t ses_dly_filt_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int value = 0, ses_node = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "0x%x %u", &value, &ses_node) != 2) {
		ses_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_ses_dly_filt(value, ses_node);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ses_volt_ratio_proc_show(struct seq_file *m, void *v)
{
	unsigned int ses_node;

	for (ses_node = 0; ses_node < sesNum; ses_node++) {
		seq_printf(m, "ses_%u Hi/LoRatio = %u/%u %% (%u/%u mv)\n",
					ses_node,
					drp_ratio[ses_node].HiCodeRatio,
					drp_ratio[ses_node].LoCodeRatio,
					drp_ratio[ses_node].HiCodeVolt,
					drp_ratio[ses_node].LoCodeVolt);
	}
	return 0;
}


static ssize_t ses_volt_ratio_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int HiRatio = 0, LoRatio = 0, ses_node = 0;
	unsigned int atf = 0, Volt = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u %u %u %u", &HiRatio,
			&LoRatio, &ses_node, &atf, &Volt) != 5) {
		ses_debug("bad argument!! Should input 5 arguments.\n");
		goto out;
	}

	if (ses_node >= sesNum)
		goto out;

	if (atf != 2) {
		drp_ratio[ses_node].HiCodeRatio = HiRatio;
		drp_ratio[ses_node].LoCodeRatio = LoRatio;
		drp_ratio[ses_node].HiCodeVolt = 0;
		drp_ratio[ses_node].LoCodeVolt = 0;
	} else {
		drp_ratio[ses_node].HiCodeRatio = 0;
		drp_ratio[ses_node].LoCodeRatio = 0;
		drp_ratio[ses_node].HiCodeVolt = HiRatio;
		drp_ratio[ses_node].LoCodeVolt = LoRatio;
	}

	if (atf == 2) {
		mtk_ses_volt_ratio_eb(100, 100, ses_node);
		mtk_ses_volt_ratio_atf(HiRatio, 0, 0, ses_node);
		mtk_ses_volt_ratio_atf(LoRatio, 100, 0, ses_node);
	} else if (atf == 1) {
		mtk_ses_volt_ratio_eb(100, 100, ses_node);
		mtk_ses_volt_ratio_atf(Volt, HiRatio, LoRatio, ses_node);
	} else
		mtk_ses_volt_ratio_eb(HiRatio, LoRatio, ses_node);

out:
	free_page((unsigned long)buf);
	return count;
}

static int ses_status_dump_proc_show(struct seq_file *m, void *v)
{
	unsigned int i, value[sesNum][5], ses_node = 0;

	for (ses_node = 0; ses_node < (sesNum - 1) ; ses_node++) {
		for (i = 0; i < 5; i++)
			value[ses_node][i] = mtk_ses_status(
				SESV6_REG0 + (0x200 * ses_node) + (i * 4));
		seq_printf(m, "CPU(%u) ses_reg :", ses_node);
		for (i = 0; i < 5; i++)
			seq_printf(m, "\t0x%x = 0x%08x",
				SESV6_REG0 + (0x200 * ses_node) + (i * 4),
				value[ses_node][i]);
		seq_printf(m, "    .%u\n", i);
	}

	for (i = 0; i < 5; i++)
		value[(sesNum - 1)][i] = mtk_ses_status(
			SESV6_DSU_REG0 + (i * 4));
	seq_printf(m, "DSU(%u) ses_reg :", (sesNum - 1));
	for (i = 0; i < 5; i++)
		seq_printf(m, "\t0x%x = 0x%08x",
			SESV6_DSU_REG0 + (i * 4),
			value[(sesNum - 1)][i]);
	seq_printf(m, "    .%u\n", i);

	seq_puts(m, "SESBG  ses_reg :");
	seq_printf(m, "\t0x%x = 0x%08x\n",
			SESV6_BG_CTRL,
			mtk_ses_status(SESV6_BG_CTRL));

	return 0;
}

#define PROC_FOPS_RW(name)						\
	static int name ## _proc_open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));				\
	}								\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
		.write		  = name ## _proc_write,		\
	}

#define PROC_FOPS_RO(name)						\
	static int name ## _proc_open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));				\
	}								\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(ses_init);
PROC_FOPS_RW(ses_trim);
PROC_FOPS_RW(ses_enable);
PROC_FOPS_RW(ses_count);
PROC_FOPS_RW(ses_hwgatepct);
PROC_FOPS_RW(ses_stepstart);
PROC_FOPS_RW(ses_steptime);
PROC_FOPS_RW(ses_dly_filt);
PROC_FOPS_RW(ses_volt_ratio);
PROC_FOPS_RO(ses_status_dump);

static int create_procfs(void)
{
	struct proc_dir_entry *ses_dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry ses_entries[] = {
		PROC_ENTRY(ses_init),
		PROC_ENTRY(ses_trim),
		PROC_ENTRY(ses_enable),
		PROC_ENTRY(ses_count),
		PROC_ENTRY(ses_hwgatepct),
		PROC_ENTRY(ses_stepstart),
		PROC_ENTRY(ses_steptime),
		PROC_ENTRY(ses_dly_filt),
		PROC_ENTRY(ses_volt_ratio),
		PROC_ENTRY(ses_status_dump),
	};

	ses_dir = proc_mkdir("ses", NULL);
	if (!ses_dir) {
		ses_debug("[%s]: mkdir /proc/ses failed\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(ses_entries); i++) {
		if (!proc_create(ses_entries[i].name,
			0664,
			ses_dir,
			ses_entries[i].fops)) {
			ses_debug("[%s]: create /proc/ses/%s failed\n",
				__func__,
				ses_entries[i].name);
			return -3;
		}
	}
	return 0;
}

static int ses_probe(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF_RESERVED_MEM
	struct device_node *node = NULL;
	int rc = 0;
	unsigned int ses_node = 0;

	node = pdev->dev.of_node;
	if (!node) {
		ses_debug("get ses device node err\n");
		return -ENODEV;
	}

	rc = of_property_read_u32(node, "state", &state);
	if (!rc) {
		ses_debug("[cpu_ses] state from DTree; rc(%d) state(0x%x)\n",
			rc,
			state);

	if (state < 256)
		for (ses_node = 0; ses_node < sesNum; ses_node++)
			mtk_ses_enable((state >> ses_node) & 0x01, ses_node);
	}

#if MTK_SES_DOE
	rc = of_property_read_u32(node, "ses0_reg3", &ses0_reg3);
	if (!rc) {
		ses_debug("[cpu_ses] ses0_reg3 from DTree; rc(%d) ses0_reg3(0x%x)\n",
			rc,
			ses0_reg3);

		if (ses0_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses0_reg3), 0);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses0_reg3), 0);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses0_reg3), 0);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses0_reg3), 0);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses0_reg3), 0);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses0_reg3), 0);
		}
	}

	rc = of_property_read_u32(node, "ses1_reg3", &ses1_reg3);
	if (!rc) {
		ses_debug("[cpu_ses] ses1_reg3 from DTree; rc(%d) ses1_reg3(0x%x)\n",
			rc,
			ses1_reg3);

		if (ses1_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses1_reg3), 1);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses1_reg3), 1);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses1_reg3), 1);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses1_reg3), 1);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses1_reg3), 1);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses1_reg3), 1);
		}
	}

	rc = of_property_read_u32(node, "ses2_reg3", &ses2_reg3);
	if (!rc) {
		ses_debug("[cpu_ses] ses2_reg3 from DTree; rc(%d) ses2_reg3(0x%x)\n",
			rc,
			ses2_reg3);

		if (ses2_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses2_reg3), 2);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses2_reg3), 2);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses2_reg3), 2);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses2_reg3), 2);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses2_reg3), 2);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses2_reg3), 2);
		}
	}

	rc = of_property_read_u32(node, "ses3_reg3", &ses3_reg3);
	if (!rc) {
		ses_debug("[cpu_ses] ses3_reg3 from DTree; rc(%d) ses3_reg3(0x%x)\n",
			rc,
			ses3_reg3);

		if (ses3_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses3_reg3), 3);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses3_reg3), 3);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses3_reg3), 3);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses3_reg3), 3);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses3_reg3), 3);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses3_reg3), 3);
		}
	}

	rc = of_property_read_u32(node, "ses4_reg3", &ses4_reg3);
	if (!rc) {
		ses_debug("[cpu_ses] ses4_reg3 from DTree; rc(%d) ses4_reg3(0x%x)\n",
			rc,
			ses4_reg3);

		if (ses4_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses4_reg3), 4);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses4_reg3), 4);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses4_reg3), 4);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses4_reg3), 4);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses4_reg3), 4);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses4_reg3), 4);
		}
	}

	rc = of_property_read_u32(node, "ses5_reg3", &ses5_reg3);
	if (!rc) {
		ses_debug("[cpu_ses] ses5_reg3 from DTree; rc(%d) ses5_reg3(0x%x)\n",
			rc,
			ses5_reg3);

		if (ses5_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses5_reg3), 5);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses5_reg3), 5);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses5_reg3), 5);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses5_reg3), 5);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses5_reg3), 5);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses5_reg3), 5);
		}
	}

	rc = of_property_read_u32(node, "ses6_reg3", &ses6_reg3);
	if (!rc) {
		ses_debug("[cpu_ses] ses6_reg3 from DTree; rc(%d) ses6_reg3(0x%x)\n",
			rc,
			ses6_reg3);

		if (ses6_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses6_reg3), 6);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses6_reg3), 6);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses6_reg3), 6);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses6_reg3), 6);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses6_reg3), 6);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses6_reg3), 6);
		}
	}

	rc = of_property_read_u32(node, "ses7_reg3", &ses7_reg3);
	if (!rc) {
		ses_debug("[cpu_ses] ses7_reg3 from DTree; rc(%d) ses7_reg3(0x%x)\n",
			rc,
			ses7_reg3);

		if (ses7_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses7_reg3), 7);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses7_reg3), 7);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses7_reg3), 7);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses7_reg3), 7);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses7_reg3), 7);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses7_reg3), 7);
		}
	}

	rc = of_property_read_u32(node, "ses8_reg3", &ses8_reg3);
	if (!rc) {
		ses_debug("[cpu_ses] ses8_reg3 from DTree; rc(%d) ses8_reg3(0x%x)\n",
			rc,
			ses8_reg3);

		if (ses8_reg3 != 0) {
			mtk_ses_steptime(GET_BITS_VAL(6:0, ses8_reg3), 8);
			mtk_ses_stepstart(GET_BITS_VAL(8:7, ses8_reg3), 8);
			mtk_ses_hwgatepct(0, GET_BITS_VAL(11:9, ses8_reg3), 8);
			mtk_ses_hwgatepct(1, GET_BITS_VAL(14:12, ses8_reg3), 8);
			mtk_ses_hwgatepct(2, GET_BITS_VAL(17:15, ses8_reg3), 8);
			mtk_ses_hwgatepct(3, GET_BITS_VAL(20:18, ses8_reg3), 8);
		}
	}

	rc = of_property_read_u32(node, "ses0_reg2", &ses0_reg2);
	if (!rc) {
		ses_debug("[cpu_ses] ses0_reg2 from DTree; rc(%d) ses0_reg2(0x%x)\n",
			rc,
			ses0_reg2);

		if (ses0_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses0_reg2), 0);
	}

	rc = of_property_read_u32(node, "ses1_reg2", &ses1_reg2);
	if (!rc) {
		ses_debug("[cpu_ses] ses1_reg2 from DTree; rc(%d) ses1_reg2(0x%x)\n",
			rc,
			ses1_reg2);

		if (ses1_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses1_reg2), 1);
	}

	rc = of_property_read_u32(node, "ses2_reg2", &ses2_reg2);
	if (!rc) {
		ses_debug("[cpu_ses] ses2_reg2 from DTree; rc(%d) ses2_reg2(0x%x)\n",
			rc,
			ses2_reg2);

		if (ses2_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses2_reg2), 2);
	}

	rc = of_property_read_u32(node, "ses3_reg2", &ses3_reg2);
	if (!rc) {
		ses_debug("[cpu_ses] ses3_reg2 from DTree; rc(%d) ses3_reg2(0x%x)\n",
			rc,
			ses3_reg2);

		if (ses3_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses3_reg2), 3);
	}

	rc = of_property_read_u32(node, "ses4_reg2", &ses4_reg2);
	if (!rc) {
		ses_debug("[cpu_ses] ses4_reg2 from DTree; rc(%d) ses4_reg2(0x%x)\n",
			rc,
			ses4_reg2);

		if (ses4_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses4_reg2), 4);
	}


	rc = of_property_read_u32(node, "ses5_reg2", &ses5_reg2);
	if (!rc) {
		ses_debug("[cpu_ses] ses5_reg2 from DTree; rc(%d) ses5_reg2(0x%x)\n",
			rc,
			ses5_reg2);

		if (ses5_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses5_reg2), 5);
	}


	rc = of_property_read_u32(node, "ses6_reg2", &ses6_reg2);
	if (!rc) {
		ses_debug("[cpu_ses] ses6_reg2 from DTree; rc(%d) ses6_reg2(0x%x)\n",
			rc,
			ses6_reg2);

		if (ses6_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses6_reg2), 6);
	}


	rc = of_property_read_u32(node, "ses7_reg2", &ses7_reg2);
	if (!rc) {
		ses_debug("[cpu_ses] ses7_reg2 from DTree; rc(%d) ses7_reg2(0x%x)\n",
			rc,
			ses7_reg2);

		if (ses7_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses7_reg2), 7);
	}


	rc = of_property_read_u32(node, "ses8_reg2", &ses8_reg2);
	if (!rc) {
		ses_debug("[cpu_ses] ses8_reg2 from DTree; rc(%d) ses8_reg2(0x%x)\n",
			rc,
			ses8_reg2);

		if (ses8_reg2 != 0)
			mtk_ses_dly_filt(GET_BITS_VAL(16:2, ses8_reg2), 8);
	}

#endif

	rc = of_property_read_u32(node, "ses0_drphipct", &ses0_drphipct);
	if (!rc) {
		rc = of_property_read_u32(node,
			"ses0_drplopct",
			&ses0_drplopct);
		ses_debug("[cpu_ses]rc(%d) ses0 drphipct(%u) drphipct(%u)\n",
			rc, ses0_drphipct, ses0_drplopct);

		if (ses0_drphipct != 0 && ses0_drplopct != 0)
			mtk_ses_volt_ratio_eb(ses0_drphipct, ses0_drplopct, 0);
	}

	rc = of_property_read_u32(node, "ses1_drphipct", &ses1_drphipct);
	if (!rc) {
		rc = of_property_read_u32(node,
			"ses1_drplopct",
			&ses1_drplopct);
		ses_debug("[cpu_ses]rc(%d) ses1 drphipct(%u) drphipct(%u)\n",
			rc, ses1_drphipct, ses1_drplopct);

		if (ses1_drphipct != 0 && ses1_drplopct != 0)
			mtk_ses_volt_ratio_eb(ses1_drphipct, ses1_drplopct, 1);
	}

	rc = of_property_read_u32(node, "ses2_drphipct", &ses2_drphipct);
	if (!rc) {
		rc = of_property_read_u32(node,
			"ses2_drplopct",
			&ses2_drplopct);
		ses_debug("[cpu_ses]rc(%d) ses2 drphipct(%u) drphipct(%u)\n",
			rc, ses2_drphipct, ses2_drplopct);

		if (ses2_drphipct != 0 && ses2_drplopct != 0)
			mtk_ses_volt_ratio_eb(ses2_drphipct, ses2_drplopct, 2);
	}

	rc = of_property_read_u32(node, "ses3_drphipct", &ses3_drphipct);
	if (!rc) {
		rc = of_property_read_u32(node,
			"ses3_drplopct",
			&ses3_drplopct);
		ses_debug("[cpu_ses]rc(%d) ses3 drphipct(%u) drphipct(%u)\n",
			rc, ses3_drphipct, ses3_drplopct);

		if (ses3_drphipct != 0 && ses3_drplopct != 0)
			mtk_ses_volt_ratio_eb(ses3_drphipct, ses3_drplopct, 3);
	}

	rc = of_property_read_u32(node, "ses4_drphipct", &ses4_drphipct);
	if (!rc) {
		rc = of_property_read_u32(node,
			"ses4_drplopct",
			&ses4_drplopct);
		ses_debug("[cpu_ses]rc(%d) ses4 drphipct(%u) drphipct(%u)\n",
			rc, ses4_drphipct, ses4_drplopct);

		if (ses4_drphipct != 0 && ses4_drplopct != 0)
			mtk_ses_volt_ratio_eb(ses4_drphipct, ses4_drplopct, 4);
	}

	rc = of_property_read_u32(node, "ses5_drphipct", &ses5_drphipct);
	if (!rc) {
		rc = of_property_read_u32(node,
			"ses5_drplopct",
			&ses5_drplopct);
		ses_debug("[cpu_ses]rc(%d) ses5 drphipct(%u) drphipct(%u)\n",
			rc, ses5_drphipct, ses5_drplopct);

		if (ses5_drphipct != 0 && ses5_drplopct != 0)
			mtk_ses_volt_ratio_eb(ses5_drphipct, ses5_drplopct, 5);
	}

	rc = of_property_read_u32(node, "ses6_drphipct", &ses6_drphipct);
	if (!rc) {
		rc = of_property_read_u32(node,
			"ses6_drplopct",
			&ses6_drplopct);
		ses_debug("[cpu_ses]rc(%d) ses6 drphipct(%u) drphipct(%u)\n",
			rc, ses6_drphipct, ses6_drplopct);

		if (ses6_drphipct != 0 && ses6_drplopct != 0)
			mtk_ses_volt_ratio_eb(ses6_drphipct, ses6_drplopct, 6);
	}

	rc = of_property_read_u32(node, "ses7_drphipct", &ses7_drphipct);
	if (!rc) {
		rc = of_property_read_u32(node,
			"ses7_drplopct",
			&ses7_drplopct);
		ses_debug("[cpu_ses]rc(%d) ses7 drphipct(%u) drphipct(%u)\n",
			rc, ses7_drphipct, ses7_drplopct);

		if (ses7_drphipct != 0 && ses7_drplopct != 0)
			mtk_ses_volt_ratio_eb(ses7_drphipct, ses7_drplopct, 7);
	}

	rc = of_property_read_u32(node, "ses8_drphipct", &ses8_drphipct);
	if (!rc) {
		rc = of_property_read_u32(node,
			"ses8_drplopct",
			&ses8_drplopct);
		ses_debug("[cpu_ses]rc(%d) ses8 drphipct(%u) drphipct(%u)\n",
			rc, ses8_drphipct, ses8_drplopct);

		if (ses8_drphipct != 0 && ses8_drplopct != 0)
			mtk_ses_volt_ratio_eb(ses8_drphipct, ses8_drplopct, 8);
	}

#ifdef CONFIG_OF_RESERVED_MEM
	ses_reserve_memory_init(0);
#endif

	ses_debug("ses probe ok!!\n");
#endif
#endif
	return 0;
}

static int ses_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int ses_resume(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_ses_of_match[] = {
	{ .compatible = "mediatek,ses", },
	{},
};
#endif

static struct platform_driver ses_driver = {
	.remove		= NULL,
	.shutdown	= NULL,
	.probe		= ses_probe,
	.suspend	= ses_suspend,
	.resume		= ses_resume,
	.driver		= {
		.name   = "mt-ses",
#ifdef CONFIG_OF
	.of_match_table = mt_ses_of_match,
#endif
	},
};

static int __init ses_init(void)
{
	int err = 0;

#if 0
	unsigned int ptp_ftpgm;

	ptp_ftpgm = get_devinfo_with_index(DEVINFO_IDX_0) & 0xff;

	if (ptp_ftpgm > 2)
		mtk_ses_init();
	else
		ses_debug("[cpu_ses]PTPv%u, SES turn off.\n", ptp_ftpgm);
#endif
	create_procfs();

	err = platform_driver_register(&ses_driver);
	if (err) {
		ses_debug("SES driver callback register failed..\n");
		return err;
	}
	return 0;
}

static void __exit ses_exit(void)
{
	ses_debug("ses de-initialization\n");
}

late_initcall(ses_init);

MODULE_DESCRIPTION("MediaTek SES Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_SES_C__
