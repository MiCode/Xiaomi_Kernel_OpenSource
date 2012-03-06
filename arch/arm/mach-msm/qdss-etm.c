/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/wakelock.h>
#include <linux/pm_qos_params.h>
#include <asm/atomic.h>

#include "qdss.h"

#define etm_writel(etm, cpu, val, off)	\
			__raw_writel((val), etm.base + (SZ_4K * cpu) + off)
#define etm_readl(etm, cpu, off)	\
			__raw_readl(etm.base + (SZ_4K * cpu) + off)

/*
 * Device registers:
 * 0x000 - 0x2FC: Trace		registers
 * 0x300 - 0x314: Management	registers
 * 0x318 - 0xEFC: Trace		registers
 *
 * Coresight registers
 * 0xF00 - 0xF9C: Management	registers
 * 0xFA0 - 0xFA4: Management	registers in PFTv1.0
 *		  Trace		registers in PFTv1.1
 * 0xFA8 - 0xFFC: Management	registers
 */

/* Trace registers (0x000-0x2FC) */
#define ETMCR			(0x000)
#define ETMCCR			(0x004)
#define ETMTRIGGER		(0x008)
#define ETMSR			(0x010)
#define ETMSCR			(0x014)
#define ETMTSSCR		(0x018)
#define ETMTEEVR		(0x020)
#define ETMTECR1		(0x024)
#define ETMFFLR			(0x02C)
#define ETMACVRn(n)		(0x040 + (n * 4))
#define ETMACTRn(n)		(0x080 + (n * 4))
#define ETMCNTRLDVRn(n)		(0x140 + (n * 4))
#define ETMCNTENRn(n)		(0x150 + (n * 4))
#define ETMCNTRLDEVRn(n)	(0x160 + (n * 4))
#define ETMCNTVRn(n)		(0x170 + (n * 4))
#define ETMSQ12EVR		(0x180)
#define ETMSQ21EVR		(0x184)
#define ETMSQ23EVR		(0x188)
#define ETMSQ31EVR		(0x18C)
#define ETMSQ32EVR		(0x190)
#define ETMSQ13EVR		(0x194)
#define ETMSQR			(0x19C)
#define ETMEXTOUTEVRn(n)	(0x1A0 + (n * 4))
#define ETMCIDCVRn(n)		(0x1B0 + (n * 4))
#define ETMCIDCMR		(0x1BC)
#define ETMIMPSPEC0		(0x1C0)
#define ETMIMPSPEC1		(0x1C4)
#define ETMIMPSPEC2		(0x1C8)
#define ETMIMPSPEC3		(0x1CC)
#define ETMIMPSPEC4		(0x1D0)
#define ETMIMPSPEC5		(0x1D4)
#define ETMIMPSPEC6		(0x1D8)
#define ETMIMPSPEC7		(0x1DC)
#define ETMSYNCFR		(0x1E0)
#define ETMIDR			(0x1E4)
#define ETMCCER			(0x1E8)
#define ETMEXTINSELR		(0x1EC)
#define ETMTESSEICR		(0x1F0)
#define ETMEIBCR		(0x1F4)
#define ETMTSEVR		(0x1F8)
#define ETMAUXCR		(0x1FC)
#define ETMTRACEIDR		(0x200)
#define ETMVMIDCVR		(0x240)
/* Management registers (0x300-0x314) */
#define ETMOSLAR		(0x300)
#define ETMOSLSR		(0x304)
#define ETMOSSRR		(0x308)
#define ETMPDCR			(0x310)
#define ETMPDSR			(0x314)

#define ETM_LOCK(cpu)							\
do {									\
	mb();								\
	etm_writel(etm, cpu, 0x0, CS_LAR);				\
} while (0)
#define ETM_UNLOCK(cpu)							\
do {									\
	etm_writel(etm, cpu, CS_UNLOCK_MAGIC, CS_LAR);			\
	mb();								\
} while (0)


/* Forward declarations */
static void etm_cfg_rw_init(void);

#ifdef CONFIG_MSM_QDSS_ETM_DEFAULT_ENABLE
static int trace_on_boot = 1;
#else
static int trace_on_boot;
#endif
module_param_named(
	trace_on_boot, trace_on_boot, int, S_IRUGO
);

struct etm_config {
	/* read only config registers */
	uint32_t	config_code;
	/* derived values */
	uint8_t		nr_addr_comp;
	uint8_t		nr_cntr;
	uint8_t		nr_ext_input;
	uint8_t		nr_ext_output;
	uint8_t		nr_context_id_comp;

	uint32_t	config_code_extn;
	/* derived values */
	uint8_t		nr_extnd_ext_input_sel;
	uint8_t		nr_instr_resources;

	uint32_t	system_config;
	/* derived values */
	uint8_t		fifofull_supported;
	uint8_t		nr_procs_supported;

	/* read-write registers */
	uint32_t	main_control;
	uint32_t	trigger_event;
	uint32_t	te_start_stop_control;
	uint32_t	te_event;
	uint32_t	te_control;
	uint32_t	fifofull_level;
	uint32_t	addr_comp_value[16];
	uint32_t	addr_comp_access_type[16];
	uint32_t	cntr_reload_value[4];
	uint32_t	cntr_enable_event[4];
	uint32_t	cntr_reload_event[4];
	uint32_t	cntr_value[4];
	uint32_t	seq_state_12_event;
	uint32_t	seq_state_21_event;
	uint32_t	seq_state_23_event;
	uint32_t	seq_state_32_event;
	uint32_t	seq_state_13_event;
	uint32_t	seq_state_31_event;
	uint32_t	current_seq_state;
	uint32_t	ext_output_event[4];
	uint32_t	context_id_comp_value[3];
	uint32_t	context_id_comp_mask;
	uint32_t	sync_freq;
	uint32_t	extnd_ext_input_sel;
	uint32_t	ts_event;
	uint32_t	aux_control;
	uint32_t	coresight_trace_id;
	uint32_t	vmid_comp_value;
};

struct etm_ctx {
	struct etm_config		cfg;
	void __iomem			*base;
	bool				trace_enabled;
	struct wake_lock		wake_lock;
	struct pm_qos_request_list	qos_req;
	atomic_t			in_use;
	struct device			*dev;
};

static struct etm_ctx etm;


/* ETM clock is derived from the processor clock and gets enabled on a
 * logical OR of below items on Krait (pass2 onwards):
 * 1.CPMR[ETMCLKEN] is 1
 * 2.ETMCR[PD] is 0
 * 3.ETMPDCR[PU] is 1
 * 4.Reset is asserted (core or debug)
 * 5.APB memory mapped requests (eg. EDAP access)
 *
 * 1., 2. and 3. above are permanent enables whereas 4. and 5. are temporary
 * enables
 *
 * We rely on 5. to be able to access ETMCR and then use 2. above for ETM
 * clock vote in the driver and the save-restore code uses 1. above
 * for its vote
 */
static void etm_set_powerdown(int cpu)
{
	uint32_t etmcr;

	etmcr = etm_readl(etm, cpu, ETMCR);
	etmcr |= BIT(0);
	etm_writel(etm, cpu, etmcr, ETMCR);
}

static void etm_clear_powerdown(int cpu)
{
	uint32_t etmcr;

	etmcr = etm_readl(etm, cpu, ETMCR);
	etmcr &= ~BIT(0);
	etm_writel(etm, cpu, etmcr, ETMCR);
}

static void etm_set_prog(int cpu)
{
	uint32_t etmcr;
	int count;

	etmcr = etm_readl(etm, cpu, ETMCR);
	etmcr |= BIT(10);
	etm_writel(etm, cpu, etmcr, ETMCR);

	for (count = TIMEOUT_US; BVAL(etm_readl(etm, cpu, ETMSR), 1) != 1
				&& count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while setting prog bit\n");
}

static void etm_clear_prog(int cpu)
{
	uint32_t etmcr;
	int count;

	etmcr = etm_readl(etm, cpu, ETMCR);
	etmcr &= ~BIT(10);
	etm_writel(etm, cpu, etmcr, ETMCR);

	for (count = TIMEOUT_US; BVAL(etm_readl(etm, cpu, ETMSR), 1) != 0
				&& count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while clearing prog bit\n");
}

static void __etm_trace_enable(int cpu)
{
	int i;

	ETM_UNLOCK(cpu);
	/* Vote for ETM power/clock enable */
	etm_clear_powerdown(cpu);
	etm_set_prog(cpu);

	etm_writel(etm, cpu, etm.cfg.main_control | BIT(10), ETMCR);
	etm_writel(etm, cpu, etm.cfg.trigger_event, ETMTRIGGER);
	etm_writel(etm, cpu, etm.cfg.te_start_stop_control, ETMTSSCR);
	etm_writel(etm, cpu, etm.cfg.te_event, ETMTEEVR);
	etm_writel(etm, cpu, etm.cfg.te_control, ETMTECR1);
	etm_writel(etm, cpu, etm.cfg.fifofull_level, ETMFFLR);
	for (i = 0; i < etm.cfg.nr_addr_comp; i++) {
		etm_writel(etm, cpu, etm.cfg.addr_comp_value[i], ETMACVRn(i));
		etm_writel(etm, cpu, etm.cfg.addr_comp_access_type[i],
							ETMACTRn(i));
	}
	for (i = 0; i < etm.cfg.nr_cntr; i++) {
		etm_writel(etm, cpu, etm.cfg.cntr_reload_value[i],
							ETMCNTRLDVRn(i));
		etm_writel(etm, cpu, etm.cfg.cntr_enable_event[i],
							ETMCNTENRn(i));
		etm_writel(etm, cpu, etm.cfg.cntr_reload_event[i],
							ETMCNTRLDEVRn(i));
		etm_writel(etm, cpu, etm.cfg.cntr_value[i], ETMCNTVRn(i));
	}
	etm_writel(etm, cpu, etm.cfg.seq_state_12_event, ETMSQ12EVR);
	etm_writel(etm, cpu, etm.cfg.seq_state_21_event, ETMSQ21EVR);
	etm_writel(etm, cpu, etm.cfg.seq_state_23_event, ETMSQ23EVR);
	etm_writel(etm, cpu, etm.cfg.seq_state_32_event, ETMSQ32EVR);
	etm_writel(etm, cpu, etm.cfg.seq_state_13_event, ETMSQ13EVR);
	etm_writel(etm, cpu, etm.cfg.seq_state_31_event, ETMSQ31EVR);
	etm_writel(etm, cpu, etm.cfg.current_seq_state, ETMSQR);
	for (i = 0; i < etm.cfg.nr_ext_output; i++)
		etm_writel(etm, cpu, etm.cfg.ext_output_event[i],
							ETMEXTOUTEVRn(i));
	for (i = 0; i < etm.cfg.nr_context_id_comp; i++)
		etm_writel(etm, cpu, etm.cfg.context_id_comp_value[i],
							ETMCIDCVRn(i));
	etm_writel(etm, cpu, etm.cfg.context_id_comp_mask, ETMCIDCMR);
	etm_writel(etm, cpu, etm.cfg.sync_freq, ETMSYNCFR);
	etm_writel(etm, cpu, etm.cfg.extnd_ext_input_sel, ETMEXTINSELR);
	etm_writel(etm, cpu, etm.cfg.ts_event, ETMTSEVR);
	etm_writel(etm, cpu, etm.cfg.aux_control, ETMAUXCR);
	etm_writel(etm, cpu, cpu+1, ETMTRACEIDR);
	etm_writel(etm, cpu, etm.cfg.vmid_comp_value, ETMVMIDCVR);

	etm_clear_prog(cpu);
	ETM_LOCK(cpu);
}

static int etm_trace_enable(void)
{
	int ret, cpu;

	ret = qdss_clk_enable();
	if (ret)
		return ret;

	wake_lock(&etm.wake_lock);
	/* 1. causes all online cpus to come out of idle PC
	 * 2. prevents idle PC until save restore flag is enabled atomically
	 *
	 * we rely on the user to prevent hotplug on/off racing with this
	 * operation and to ensure cores where trace is expected to be turned
	 * on are already hotplugged on
	 */
	pm_qos_update_request(&etm.qos_req, 0);

	etb_disable();
	tpiu_disable();
	/* enable ETB first to avoid loosing any trace data */
	etb_enable();
	funnel_enable(0x0, 0x3);
	for_each_online_cpu(cpu)
		__etm_trace_enable(cpu);

	etm.trace_enabled = true;

	pm_qos_update_request(&etm.qos_req, PM_QOS_DEFAULT_VALUE);
	wake_unlock(&etm.wake_lock);

	return 0;
}

static void __etm_trace_disable(int cpu)
{
	ETM_UNLOCK(cpu);
	etm_set_prog(cpu);

	/* program trace enable to low by using always false event */
	etm_writel(etm, cpu, 0x6F | BIT(14), ETMTEEVR);

	/* Vote for ETM power/clock disable */
	etm_set_powerdown(cpu);
	ETM_LOCK(cpu);
}

static void etm_trace_disable(void)
{
	int cpu;

	wake_lock(&etm.wake_lock);
	/* 1. causes all online cpus to come out of idle PC
	 * 2. prevents idle PC until save restore flag is disabled atomically
	 *
	 * we rely on the user to prevent hotplug on/off racing with this
	 * operation and to ensure cores where trace is expected to be turned
	 * off are already hotplugged on
	 */
	pm_qos_update_request(&etm.qos_req, 0);

	for_each_online_cpu(cpu)
		__etm_trace_disable(cpu);
	etb_dump();
	etb_disable();
	funnel_disable(0x0, 0x3);

	etm.trace_enabled = false;

	pm_qos_update_request(&etm.qos_req, PM_QOS_DEFAULT_VALUE);
	wake_unlock(&etm.wake_lock);

	qdss_clk_disable();
}

static int etm_open(struct inode *inode, struct file *file)
{
	if (atomic_cmpxchg(&etm.in_use, 0, 1))
		return -EBUSY;

	dev_dbg(etm.dev, "%s: successfully opened\n", __func__);
	return 0;
}

static void etm_range_filter(char range, uint32_t reg1,
				uint32_t addr1, uint32_t reg2, uint32_t addr2)
{
	etm.cfg.addr_comp_value[reg1] = addr1;
	etm.cfg.addr_comp_value[reg2] = addr2;

	etm.cfg.te_control |= (1 << (reg1/2));
	if (range == 'i')
		etm.cfg.te_control &= ~BIT(24);
	else if (range == 'e')
		etm.cfg.te_control |= BIT(24);
}

static void etm_start_stop_filter(char start_stop,
				uint32_t reg, uint32_t addr)
{
	etm.cfg.addr_comp_value[reg] = addr;

	if (start_stop == 's')
		etm.cfg.te_start_stop_control |= (1 << reg);
	else if (start_stop == 't')
		etm.cfg.te_start_stop_control |= (1 << (reg + 16));

	etm.cfg.te_control |= BIT(25);
}

#define MAX_COMMAND_STRLEN  40
static ssize_t etm_write(struct file *file, const char __user *data,
				size_t len, loff_t *ppos)
{
	char command[MAX_COMMAND_STRLEN];
	int str_len;
	unsigned long reg1, reg2;
	unsigned long addr1, addr2;

	str_len = strnlen_user(data, MAX_COMMAND_STRLEN);
	dev_dbg(etm.dev, "string length: %d", str_len);
	if (str_len == 0 || str_len == (MAX_COMMAND_STRLEN+1)) {
		dev_err(etm.dev, "error in str_len: %d", str_len);
		return -EFAULT;
	}
	/* includes the null character */
	if (copy_from_user(command, data, str_len)) {
		dev_err(etm.dev, "error in copy_from_user: %d", str_len);
		return -EFAULT;
	}

	dev_dbg(etm.dev, "input = %s", command);

	switch (command[0]) {
	case '0':
		if (etm.trace_enabled) {
			etm_trace_disable();
			dev_info(etm.dev, "tracing disabled\n");
		} else
			dev_err(etm.dev, "trace already disabled\n");

		break;
	case '1':
		if (!etm.trace_enabled) {
			if (!etm_trace_enable())
				dev_info(etm.dev, "tracing enabled\n");
			else
				dev_err(etm.dev, "error enabling trace\n");
		} else
			dev_err(etm.dev, "trace already enabled\n");
		break;
	case 'f':
		switch (command[2]) {
		case 'i':
			switch (command[4]) {
			case 'i':
				if (sscanf(&command[6], "%lx:%lx:%lx:%lx\\0",
					&reg1, &addr1, &reg2, &addr2) != 4)
					goto err_out;
				if (reg1 > 7 || reg2 > 7 || (reg1 % 2))
					goto err_out;
				etm_range_filter('i',
						reg1, addr1, reg2, addr2);
				break;
			case 'e':
				if (sscanf(&command[6], "%lx:%lx:%lx:%lx\\0",
					&reg1, &addr1, &reg2, &addr2) != 4)
					goto err_out;
				if (reg1 > 7 || reg2 > 7 || (reg1 % 2)
					|| command[2] == 'd')
					goto err_out;
				etm_range_filter('e',
						reg1, addr1, reg2, addr2);
				break;
			case 's':
				if (sscanf(&command[6], "%lx:%lx\\0",
					&reg1, &addr1) != 2)
					goto err_out;
				if (reg1 > 7)
					goto err_out;
				etm_start_stop_filter('s', reg1, addr1);
				break;
			case 't':
				if (sscanf(&command[6], "%lx:%lx\\0",
						&reg1, &addr1) != 2)
					goto err_out;
				if (reg1 > 7)
					goto err_out;
				etm_start_stop_filter('t', reg1, addr1);
				break;
			default:
				goto err_out;
			}
			break;
		case 'r':
			etm_cfg_rw_init();
			break;
		default:
			goto err_out;
		}
		break;
	default:
		goto err_out;
	}

	return len;

err_out:
	return -EFAULT;
}

static int etm_release(struct inode *inode, struct file *file)
{
	atomic_set(&etm.in_use, 0);
	dev_dbg(etm.dev, "%s: released\n", __func__);
	return 0;
}

static const struct file_operations etm_fops = {
	.owner =	THIS_MODULE,
	.open =		etm_open,
	.write =	etm_write,
	.release =	etm_release,
};

static struct miscdevice etm_misc = {
	.name =		"msm_etm",
	.minor =	MISC_DYNAMIC_MINOR,
	.fops =		&etm_fops,
};

static void etm_cfg_rw_init(void)
{
	int i;

	etm.cfg.main_control =				0x00001000;
	etm.cfg.trigger_event =				0x0000406F;
	etm.cfg.te_start_stop_control =			0x00000000;
	etm.cfg.te_event =				0x0000006F;
	etm.cfg.te_control =				0x01000000;
	etm.cfg.fifofull_level =			0x00000028;
	for (i = 0; i < etm.cfg.nr_addr_comp; i++) {
		etm.cfg.addr_comp_value[i] =		0x00000000;
		etm.cfg.addr_comp_access_type[i] =	0x00000000;
	}
	for (i = 0; i < etm.cfg.nr_cntr; i++) {
		etm.cfg.cntr_reload_value[i] =		0x00000000;
		etm.cfg.cntr_enable_event[i] =		0x0000406F;
		etm.cfg.cntr_reload_event[i] =		0x0000406F;
		etm.cfg.cntr_value[i] =			0x00000000;
	}
	etm.cfg.seq_state_12_event =			0x0000406F;
	etm.cfg.seq_state_21_event =			0x0000406F;
	etm.cfg.seq_state_23_event =			0x0000406F;
	etm.cfg.seq_state_32_event =			0x0000406F;
	etm.cfg.seq_state_13_event =			0x0000406F;
	etm.cfg.seq_state_31_event =			0x0000406F;
	etm.cfg.current_seq_state =			0x00000000;
	for (i = 0; i < etm.cfg.nr_ext_output; i++)
		etm.cfg.ext_output_event[i] =		0x0000406F;
	for (i = 0; i < etm.cfg.nr_context_id_comp; i++)
		etm.cfg.context_id_comp_value[i] =	0x00000000;
	etm.cfg.context_id_comp_mask =			0x00000000;
	etm.cfg.sync_freq =				0x00000080;
	etm.cfg.extnd_ext_input_sel =			0x00000000;
	etm.cfg.ts_event =				0x0000406F;
	etm.cfg.aux_control =				0x00000000;
	etm.cfg.vmid_comp_value =			0x00000000;
}

/* Memory mapped writes to clear os lock not supported */
static void etm_os_unlock(void *unused)
{
	unsigned long value = 0x0;

	asm("mcr p14, 1, %0, c1, c0, 4\n\t" : : "r" (value));
	asm("isb\n\t");
}

static void etm_cfg_ro_init(void)
{
	/* use cpu 0 for setup */
	int cpu = 0;

	/* Unlock OS lock first to allow memory mapped reads and writes */
	etm_os_unlock(NULL);
	smp_call_function(etm_os_unlock, NULL, 1);
	ETM_UNLOCK(cpu);
	/* Vote for ETM power/clock enable */
	etm_clear_powerdown(cpu);
	etm_set_prog(cpu);

	/* find all capabilities */
	etm.cfg.config_code	=	etm_readl(etm, cpu, ETMCCR);
	etm.cfg.nr_addr_comp =		BMVAL(etm.cfg.config_code, 0, 3) * 2;
	etm.cfg.nr_cntr =		BMVAL(etm.cfg.config_code, 13, 15);
	etm.cfg.nr_ext_input =		BMVAL(etm.cfg.config_code, 17, 19);
	etm.cfg.nr_ext_output =		BMVAL(etm.cfg.config_code, 20, 22);
	etm.cfg.nr_context_id_comp =	BMVAL(etm.cfg.config_code, 24, 25);

	etm.cfg.config_code_extn =	etm_readl(etm, cpu, ETMCCER);
	etm.cfg.nr_extnd_ext_input_sel =
					BMVAL(etm.cfg.config_code_extn, 0, 2);
	etm.cfg.nr_instr_resources =	BMVAL(etm.cfg.config_code_extn, 13, 15);

	etm.cfg.system_config =		etm_readl(etm, cpu, ETMSCR);
	etm.cfg.fifofull_supported =	BVAL(etm.cfg.system_config, 8);
	etm.cfg.nr_procs_supported =	BMVAL(etm.cfg.system_config, 12, 14);

	/* Vote for ETM power/clock disable */
	etm_set_powerdown(cpu);
	ETM_LOCK(cpu);
}

static int __devinit etm_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		goto err_res;
	}

	etm.base = ioremap_nocache(res->start, resource_size(res));
	if (!etm.base) {
		ret = -EINVAL;
		goto err_ioremap;
	}

	etm.dev = &pdev->dev;

	ret = misc_register(&etm_misc);
	if (ret)
		goto err_misc;

	ret = qdss_clk_enable();
	if (ret)
		goto err_clk;

	etm_cfg_ro_init();
	etm_cfg_rw_init();

	etm.trace_enabled = false;

	wake_lock_init(&etm.wake_lock, WAKE_LOCK_SUSPEND, "msm_etm");
	pm_qos_add_request(&etm.qos_req, PM_QOS_CPU_DMA_LATENCY,
						PM_QOS_DEFAULT_VALUE);
	atomic_set(&etm.in_use, 0);

	qdss_clk_disable();

	dev_info(etm.dev, "ETM intialized.\n");

	if (trace_on_boot) {
		if (!etm_trace_enable())
			dev_info(etm.dev, "tracing enabled\n");
		else
			dev_err(etm.dev, "error enabling trace\n");
	}

	return 0;

err_clk:
	misc_deregister(&etm_misc);
err_misc:
	iounmap(etm.base);
err_ioremap:
err_res:
	return ret;
}

static int etm_remove(struct platform_device *pdev)
{
	if (etm.trace_enabled)
		etm_trace_disable();
	pm_qos_remove_request(&etm.qos_req);
	wake_lock_destroy(&etm.wake_lock);
	misc_deregister(&etm_misc);
	iounmap(etm.base);

	return 0;
}

static struct platform_driver etm_driver = {
	.probe          = etm_probe,
	.remove         = etm_remove,
	.driver         = {
		.name   = "msm_etm",
	},
};

int __init etm_init(void)
{
	return platform_driver_register(&etm_driver);
}

void etm_exit(void)
{
	platform_driver_unregister(&etm_driver);
}
