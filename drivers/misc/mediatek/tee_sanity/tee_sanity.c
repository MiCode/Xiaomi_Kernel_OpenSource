// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irqchip/mtk-gic-extend.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <mt-plat/mtk_secure_api.h>
#include <tee_sanity.h>
#include <linux/sched/clock.h>
#include <archcounter_timesync.h>
#include <asm/arch_timer.h>
#include <sched_ctl.h>

#define CREATE_TRACE_POINTS
#include <trace_tee.h>

#define TEE_TASK_MIN_UTIL	5

static u64 boot_to_kernel_ns;

void mtk_set_prefer_bigcore(struct task_struct *current_task)
{
#ifdef CONFIG_MTK_SCHED_BOOST
	sched_set_cpuprefer(current->pid, SCHED_PREFER_BIG);
#endif
}

void mtk_set_task_basic_util(struct task_struct *current_task)
{
	set_task_util_min_pct(current->pid, TEE_TASK_MIN_UTIL);
}

static void set_boot_to_kernel_time(void)
{
	u64 gud_init_time;
	u64 boot_time;

	gud_init_time = sched_clock();
	boot_time = mtk_get_archcounter_time(arch_counter_get_cntvct());

	boot_to_kernel_ns = boot_time - gud_init_time;
}

static void convert_kernel_time(char *tee_timestamp, char *output,
		size_t output_sz)
{
	u64 boot_to_kernel_us;
	u64 kernel_time_us;
	u64 tee_time_us;
	u64 tee_time_s;
	int err;

	if (!tee_timestamp || !output || !output_sz) {
		pr_err(PFX "param check failed\n");
		return;
	}

	err = kstrtou64(tee_timestamp, 10, &tee_time_us);
	if (err) {
		pr_err(PFX "kstrtou64 failed\n");
		return;
	}

	boot_to_kernel_us = div_u64(boot_to_kernel_ns, NSEC_PER_USEC);
	kernel_time_us = tee_time_us - boot_to_kernel_us;

	div_u64_rem(kernel_time_us, USEC_PER_SEC, (u32 *)&tee_time_us);
	tee_time_s = div_u64(kernel_time_us, USEC_PER_SEC);

	snprintf(output, output_sz, "%llu.%06u", tee_time_s, (u32)tee_time_us);
}

int32_t mtk_tee_log_tracing(u32 cpuid, u16 tee_pid, char *line, u32 line_len)
{
	struct tee_trace_struct tee_trace_info;
	char ktimestamp[128] = {0};
	char trace_buf[128] = {0};
	char *trace_buf_ptr;
	char delim[] = ":";
	char *prefix;
	char *postfix;
	char *timestamp;

	bool trace_start = false;
	bool trace_end = false;

	if (unlikely(line == NULL)) {
		pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
		return TEE_TRACE_PREFIX_NOT_MATCH;
	}

	prefix = strstr(line, TEE_BEGIN_TRACE);
	postfix = strstr(line, TEE_END_TRACE);

	if (prefix) {
		trace_start = true;
		line = prefix;

	} else if (postfix) {
		trace_end = true;
		line = postfix;

	} else
		return TEE_TRACE_PREFIX_NOT_MATCH;

	strncpy(trace_buf, line, (line_len >= sizeof(trace_buf)) ?
			sizeof(trace_buf) : line_len);

	trace_buf_ptr = trace_buf;
	prefix = strsep(&trace_buf_ptr, delim);
	if (!prefix) {
		pr_err(PFX "strsep prefix failed\n");
		return TEE_TRACE_PARSE_FAILED;
	}

	timestamp = strsep(&trace_buf_ptr, delim);
	if (!timestamp) {
		pr_err(PFX "strsep timestamp failed\n");
		return TEE_TRACE_PARSE_FAILED;
	}

	postfix = strsep(&trace_buf_ptr, delim);
	if (!postfix) {
		pr_err(PFX "strsep postfix failed\n");
		return TEE_TRACE_PARSE_FAILED;
	}

	convert_kernel_time(timestamp, ktimestamp, sizeof(ktimestamp));

	tee_trace_info.cpuid = cpuid;
	tee_trace_info.ktimestamp = ktimestamp;
	tee_trace_info.tee_pid = tee_pid;
	tee_trace_info.tee_postfix = postfix;

	if (trace_start)
		trace_tee_sched_start(&tee_trace_info);
	else if (trace_end)
		trace_tee_sched_end(&tee_trace_info);

	return TEE_TRACE_OK;
}

static bool enable_ut;
static bool enable_read_hwirq;
static uint32_t tee_sanity_irq;
static uint32_t tee_sanity_hwirq;

static void tee_ut(uint32_t cmd)
{
	struct arm_smccc_res res;

	pr_info(PFX "%s, cmd=0x%x\n", __func__, cmd);
	if (cmd == TEE_UT_READ_INTR) {
		pr_info(PFX "tee_sanity_hwirq:0x%x\n", tee_sanity_hwirq);
		enable_read_hwirq = true;

	} else if (cmd == TEE_UT_TRIGGER_INTR) {
		pr_info(PFX "trigger interrupt(0x%x)...\n", tee_sanity_hwirq);

		arm_smccc_smc(MTK_SIP_KERNEL_TEE_CONTROL, TEE_OP_ID_SET_PENDING,
				0, 0, 0, 0, 0, 0, &res);

		pr_info(PFX "trigger interrupt done!\n");

	} else {
		pr_err(PFX "%s, cmd=0x%x is not supported\n", __func__, cmd);
	}
}

static irqreturn_t tee_sanity_isr(int intr, void *args)
{
	if (intr == tee_sanity_irq)
		pr_info(PFX "receive interrupt success!\n");

	return IRQ_HANDLED;
}

ssize_t tee_sanity_dbg_read(struct file *file, char __user *buffer,
	size_t count, loff_t *ppos)
{
	char msg_buf[1024] = {0};
	char *p = msg_buf;
	int len;
	int ret;

	if (enable_read_hwirq) {
		tee_log(p, msg_buf, "%d\n", tee_sanity_hwirq);

	} else {
		tee_log(p, msg_buf, "tee_sanity debug status:\n");
		tee_log(p, msg_buf, "\tenable_ut: %d\n", enable_ut);
		tee_log(p, msg_buf, "\ttee_sanity_irq: %d\n", tee_sanity_irq);
		tee_log(p, msg_buf, "\ttee_sanity_hwirq: %d\n",
				tee_sanity_hwirq);
		tee_log(p, msg_buf, "\n");
	}

	len = p - msg_buf;
	ret = simple_read_from_buffer(buffer, count, ppos, msg_buf, len);

	if (!ret)
		enable_read_hwirq = false;

	return ret;
}

ssize_t tee_sanity_dbg_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	unsigned long param = 0;
	char input[32] = {0};
	char *parm_str;
	char *cmd_str;
	char *pinput;
	int err, len;

	len = (count < (sizeof(input) - 1)) ? count : (sizeof(input) - 1);
	if (copy_from_user(input, buffer, len)) {
		pr_err(PFX "copy from user failed!\n");
		return -EFAULT;
	}

	input[len] = '\0';
	pinput = input;

	cmd_str = strsep(&pinput, " ");
	if (cmd_str == NULL)
		return -EINVAL;

	parm_str = strsep(&pinput, " ");
	if (parm_str == NULL)
		return -EINVAL;

	err = kstrtol(parm_str, 10, &param);
	if (err != 0)
		return err;

	if (!strncmp(cmd_str, "enable_ut", sizeof("enable_ut"))) {
		enable_ut = (param != 0);
		pr_info(PFX "enable_ut = %s\n",
			enable_ut ? "enable" : "disable");
		return count;

	} else if (!strncmp(cmd_str, "tee_ut", sizeof("tee_ut"))) {
		if (enable_ut)
			tee_ut(param);
		else
			pr_info(PFX "enable_ut is not enabled\n");

		return count;

	} else {
		return -EINVAL;
	}

	return count;
}

static const struct file_operations tee_sanity_fops = {
	.owner = THIS_MODULE,
	.write = tee_sanity_dbg_write,
	.read = tee_sanity_dbg_read,
};

static const struct of_device_id tee_sanity_dt_match[] = {
	{ .compatible = "mediatek,tee_sanity" },
	{},
};

static int tee_sanity_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int ret;

	pr_info(PFX "driver registered\n");

	if (IS_ERR(node)) {
		pr_err(PFX "cannot find device node\n");
		return -ENODEV;
	}

	tee_sanity_irq = irq_of_parse_and_map(node, 0);
	if (!tee_sanity_irq) {
		pr_err(PFX "Failed to parse and map the interrupt.\n");
		return -EINVAL;
	}

	tee_sanity_hwirq = virq_to_hwirq(tee_sanity_irq);
	pr_debug(PFX "tee_sanity_irq: 0x%x, hwirq: 0x%x\n",
			tee_sanity_irq, tee_sanity_hwirq);

	ret = devm_request_irq(&pdev->dev, tee_sanity_irq,
			(irq_handler_t)tee_sanity_isr,
			IRQF_TRIGGER_RISING, "tee_sanity", NULL);
	if (ret) {
		pr_err(PFX "Failed to request tee_sanity irq, ret(%d)\n", ret);
		return ret;
	}

	proc_create("tee_sanity", 0664, NULL, &tee_sanity_fops);

	/* Init boot to kernel time */
	set_boot_to_kernel_time();

	return 0;
}

static struct platform_driver tee_sanity_driver = {
	.probe = tee_sanity_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = tee_sanity_dt_match,
	},
};

module_platform_driver(tee_sanity_driver);

MODULE_DESCRIPTION("Mediatek TEE sanity Driver");
MODULE_AUTHOR("Neal Liu <neal.liu@mediatek.com>");
MODULE_LICENSE("GPL");
