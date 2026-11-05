// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for recording irq and task schedule trace log
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */

#define pr_fmt(fmt) "mi_trace: "fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/init.h>
#include <linux/sched/clock.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/panic_notifier.h>
#include <trace/events/sched.h>
#include <trace/events/irq.h>
#include <soc/qcom/minidump.h>

#include "mi_trace.h"

static struct mi_trace_priv {
	char *mi_mode_name[TR_MAX];
	struct proc_dir_entry *mi_trace_proc_dir;
	struct mi_trace_buffer_info mi_trace_buf[TR_MAX];
	struct mi_trace_mem_node mi_trace_mem[TR_MAX];
	atomic_t mi_trace_hook_on[TR_MAX];
	struct md_region md_entry[TR_MAX];
	unsigned int mi_t_event_len[TR_MAX];
} mtp = {
	.mi_trace_hook_on = {ATOMIC_INIT(0)},
	.mi_mode_name = {
		"KIRQTRACE",
		"KTASKTRACE"
	},
	.mi_t_event_len = {
		MI_TRACE_IRQ_INFO_SIZE,
		MI_TRACE_TASK_INFO_SIZE
	}
};

static unsigned int enabled = 0;

static int irqtrace_show(struct seq_file *m, void *v)
{
	u32 cpu_num = num_possible_cpus();
	struct mi_trace_irq_info *irq_info = NULL;
	struct mi_trace_pinfo *cpu_pinfo = NULL;
	struct mi_trace_buffer_info *buffer = &mtp.mi_trace_buf[TR_IRQ];

	for (int i = 0; i < cpu_num; i++) {
		cpu_pinfo = (struct mi_trace_pinfo *)buffer->percpu_addr[i];
		seq_printf(m, "------- CPU%d --------\n", i);
		seq_printf(m, "max_num: %u field_len: %u rear: %u is_full: %u\n",
					cpu_pinfo->max_num,
					cpu_pinfo->field_len,
					cpu_pinfo->rear,
					cpu_pinfo->is_full);
		seq_puts(m, "timestamp\tirq\n");
		irq_info = (struct mi_trace_irq_info *)&cpu_pinfo->data;
		for (int j = 0; j < cpu_pinfo->max_num; j++) {
			struct mi_trace_irq_info *ii = irq_info + j;
			seq_printf(m, "%llu\t0x%x\n",
						ii->clock,
						ii->irq);
		}
	}

	return 0;
}

static int tasktrace_show(struct seq_file *m, void *v)
{
	u32 cpu_num = num_possible_cpus();
	struct mi_trace_task_info *task_info = NULL;
	struct mi_trace_pinfo *cpu_pinfo = NULL;
	struct mi_trace_buffer_info *buffer = &mtp.mi_trace_buf[TR_TASK];

	for (int i = 0; i < cpu_num; i++) {
		cpu_pinfo = (struct mi_trace_pinfo *)buffer->percpu_addr[i];
		seq_printf(m, "------- CPU%d --------\n", i);
		seq_printf(m, "max_num: %u field_len: %u rear: %u is_full: %u\n",
					cpu_pinfo->max_num,
					cpu_pinfo->field_len,
					cpu_pinfo->rear,
					cpu_pinfo->is_full);
		seq_puts(m, "timestamp\tpid\tcomm\n");
		task_info = (struct mi_trace_task_info *)&cpu_pinfo->data;
		for (int j = 0; j < cpu_pinfo->max_num; j++) {
			struct mi_trace_task_info *ti = task_info + j;
			seq_printf(m, "%llu\t%llu\t%s\n",
						ti->clock,
						ti->pid,
						ti->comm);
		}
	}

	return 0;
}

#define KS 32
char kstring[KS];	/* should be less sloppy about overflows :) */
static ssize_t enabled_read(struct file *file, char __user *buf, size_t lbuf, loff_t *ppos)
{
	int nbytes = sprintf(kstring, "%u\n", enabled);

	return simple_read_from_buffer(buf, lbuf, ppos, kstring, nbytes);
}

static ssize_t enabled_write(struct file *file, const char __user *buf, size_t lbuf, loff_t *ppos)
{
	ssize_t rc;

	rc = simple_write_to_buffer(kstring, lbuf, ppos, buf, lbuf);
	if (sscanf(kstring, "%d", &enabled) != 1)
		return -EINVAL;
	pr_err("enabled has been set to %u\n", enabled);

	return rc;
}

static const struct proc_ops enable_pops = {
	.proc_read = enabled_read,
	.proc_write = enabled_write,
};

static int mi_trace_create_procfs(void)
{
	struct proc_dir_entry *entry;

	mtp.mi_trace_proc_dir = proc_mkdir("mi_trace", NULL);
	if (!mtp.mi_trace_proc_dir)
		return -ENOMEM;

	entry = proc_create("enabled", S_IWUGO | S_IRUGO, mtp.mi_trace_proc_dir, &enable_pops);
	if (!entry)
		goto fail1;

	entry = proc_create_single("irqtrace", S_IRUGO, mtp.mi_trace_proc_dir, irqtrace_show);
	if (!entry)
		goto fail2;

	entry = proc_create_single("tasktrace", S_IRUGO, mtp.mi_trace_proc_dir, tasktrace_show);
	if (!entry)
		goto fail3;

	return 0;

fail3:
	remove_proc_entry("irqtrace", mtp.mi_trace_proc_dir);
fail2:
	remove_proc_entry("enabled", mtp.mi_trace_proc_dir);
fail1:
	remove_proc_entry("mi_trace", NULL);

	return -ENOMEM;
}

static void mi_trace_destory_procfs(void)
{
	remove_proc_entry("tasktrace", mtp.mi_trace_proc_dir);
	remove_proc_entry("irqtrace", mtp.mi_trace_proc_dir);
	remove_proc_entry("enabled", mtp.mi_trace_proc_dir);
	remove_proc_entry("mi_trace", NULL);
}

static int mi_trace_panic_notifier(struct notifier_block *this, unsigned long event, void *ptr)
{
	enabled = 0;
	return NOTIFY_DONE;
}

static struct notifier_block mi_trace_panic_blk = {
	.notifier_call  = mi_trace_panic_notifier,
	.priority = INT_MAX,
};

static int mi_trace_should_log(enum mi_trace_type type)
{
	return enabled && atomic_read(&mtp.mi_trace_hook_on[type]);
}

static int mi_trace_percup_buffer_init(struct mi_trace_pinfo *q, unsigned int bytes, unsigned int len)
{
	unsigned int pbuf_max;

	if (!q)
		return -EFAULT;

	if (bytes < (sizeof(struct mi_trace_pinfo) + sizeof(u8) * len))
		return -ENOMEM;

	pbuf_max = bytes - sizeof(struct mi_trace_pinfo);

	q->max_num = pbuf_max / (sizeof(u8) * len);
	q->rear = 0;
	q->is_full = 0;
	q->field_len = len;

	return 0;
}

static void mi_trace_percup_buffer_write(struct mi_trace_pinfo *q, u8 *element, enum mi_trace_type mode)
{
	if (!q || !element)
		return;
	if (q->rear >= q->max_num) {
		q->is_full = 1;
		q->rear = 0;
	}

	if (mode == TR_IRQ) {
		struct mi_trace_irq_info* info = (struct mi_trace_irq_info *)&q->data;
		memcpy((void *)(info + q->rear++),
			(void *)element, q->field_len * sizeof(u8));
	} else if (mode == TR_TASK) {
		struct mi_trace_task_info* info = (struct mi_trace_task_info *)&q->data;
		memcpy((void *)(info + q->rear++),
			(void *)element, q->field_len * sizeof(u8));
	} else {
		return;
	}
}

static void mi_trace_irq_write(void *ignore, int irq, struct irqaction *action)
{
	u8 cpu;
	struct mi_trace_irq_info info;
	struct mi_trace_pinfo *cpu_pinfo = NULL;

	if (!mi_trace_should_log(TR_IRQ))
		return;

	memset(&info, 0, sizeof(info));
	cpu = (u8)smp_processor_id();
	info.clock = sched_clock();
	info.irq = (u32)irq;

	cpu_pinfo = (struct mi_trace_pinfo *)mtp.mi_trace_buf[TR_IRQ].percpu_addr[cpu];
	mi_trace_percup_buffer_write(cpu_pinfo, (u8 *)&info, TR_IRQ);
}

static void mi_trace_task_write(void *ignore, bool preempt, struct task_struct *prev, struct task_struct *next, unsigned int prev_state)
{
	u8 cpu;
	struct mi_trace_task_info info;
	struct mi_trace_pinfo *cpu_pinfo = NULL;

	if (!mi_trace_should_log(TR_TASK))
		return;
	if (!next)
		return;

	memset(&info, 0, sizeof(info));
	cpu = (u8)smp_processor_id();
	info.clock = sched_clock();
	info.pid = (u64)next->pid;
	(void)strncpy(info.comm, next->comm, sizeof(next->comm) - 1);
	info.comm[TASK_COMM_LEN - 1] = '\0';
	info.stack = (uintptr_t)next->stack;

	cpu_pinfo = (struct mi_trace_pinfo *)mtp.mi_trace_buf[TR_TASK].percpu_addr[cpu];
	mi_trace_percup_buffer_write(cpu_pinfo, (u8 *)&info, TR_TASK);
}

static int mi_trace_buffer_init(struct mi_trace_buffer_info *buffer_info,
	enum mi_trace_type mode)
{
	int i;
	int ret;
	u32 cpu_num = num_possible_cpus();
	struct mi_trace_buffer_info *buffer = buffer_info;
	struct mi_trace_pinfo *cpu_pinfo = NULL;

	if (!buffer_info)
		return -EFAULT;

	if (mode >= TR_MAX) {
		pr_err("Wrong mode\n");
		return -ENOMEM;
	}

	buffer->buffer_addr = (unsigned char *)mtp.mi_trace_mem[mode].vaddr;
	buffer->percpu_length = mtp.mi_trace_mem[mode].size / cpu_num;
	buffer->buffer_size = mtp.mi_trace_mem[mode].size;

	for (i = 0; i < TR_MAX; i++) {
		if (i == TR_IRQ)
			mtp.mi_t_event_len[i] = sizeof(struct mi_trace_irq_info);
		if (i == TR_TASK)
			mtp.mi_t_event_len[i] = sizeof(struct mi_trace_task_info);
	}

	for (i = 0; i < (int)cpu_num; i++) {
		buffer->percpu_addr[i] =
			(unsigned char *)(mtp.mi_trace_mem[mode].vaddr +
			(uintptr_t)(i * (buffer->percpu_length)));
		cpu_pinfo = (struct mi_trace_pinfo *)buffer->percpu_addr[i];
		ret = mi_trace_percup_buffer_init(cpu_pinfo,
			buffer->percpu_length, mtp.mi_t_event_len[mode]);
		if (ret) {
			pr_err("cpu %d ringbuffer init failed\n", i);
			return ret;
		}
	}

	atomic_set(&mtp.mi_trace_hook_on[mode], MI_TRACE_HOOK_ON);

	return 0;
}

static int hook_mi_trace_events(void)
{
	int ret;

	ret = register_trace_irq_handler_entry(mi_trace_irq_write, NULL);
	if (ret) {
		pr_err("trace_irq_handler_entry registration failed\n");
		unregister_trace_irq_handler_entry(mi_trace_irq_write, NULL);
		return -EINVAL;
	}

	ret = register_trace_sched_switch(mi_trace_task_write, NULL);
	if (ret) {
		pr_err("trace_sched_switch registration failed\n");
		unregister_trace_irq_handler_entry(mi_trace_irq_write, NULL);
		unregister_trace_sched_switch(mi_trace_task_write, NULL);
		return -EINVAL;
	}

	return 0;
}

static void unhook_mi_trace_events(void)
{
	unregister_trace_sched_switch(mi_trace_task_write, NULL);
	unregister_trace_irq_handler_entry(mi_trace_irq_write, NULL);
}

static void mi_trace_init(enum mi_trace_type type, dma_addr_t paddr_start, void *vaddr_start, int size)
{
	struct md_region *md_entry = &mtp.md_entry[type];
	struct mi_trace_mem_node *mi_trace_mem = &mtp.mi_trace_mem[type];
	struct mi_trace_buffer_info *mi_trace_buf = &mtp.mi_trace_buf[type];
	char *mi_mode_name = mtp.mi_mode_name[type];

	if (!vaddr_start || !paddr_start)
		return;

	mi_trace_mem->paddr = paddr_start;
	mi_trace_mem->size = size;
	mi_trace_mem->vaddr = (uintptr_t)vaddr_start;
	pr_err("%s, 0x%lx, 0x%lx\n", mi_mode_name, mi_trace_mem->paddr, mi_trace_mem->vaddr);

	strscpy(md_entry->name, mi_mode_name, sizeof(md_entry->name));
	md_entry->virt_addr = mi_trace_mem->vaddr;
	md_entry->phys_addr = mi_trace_mem->paddr;
	md_entry->size = size;

	mi_trace_buffer_init(mi_trace_buf, type);

	if (msm_minidump_add_region(md_entry) < 0)
		pr_err("%s trace fail to add minidump\n", mi_mode_name);

	pr_err("%s success\n", mi_mode_name);
}

static int mi_trace_probe(struct platform_device *pdev)
{
	u64 size;
	dma_addr_t phys_addr;
	void *vaddr;
	int i, ret;

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node, "mt-size", (u32 *)&size);
		if (ret < 0)
			return ret;
	} else
		return -EINVAL;

	if (size <= 0 || size > SZ_1M)
		return -EINVAL;

	pr_err("total size: 0x%llx\n", size);

	vaddr = dmam_alloc_coherent(&pdev->dev, size, &phys_addr, GFP_KERNEL);
	if (!vaddr)
		return -ENOMEM;

	for (i = 0; i < TR_MAX; i++)
		mi_trace_init(i, phys_addr + i * size / 2, vaddr + i * size / 2, size / 2);

	ret = hook_mi_trace_events();
	if (ret < 0)
		goto hook_events_fail;

	atomic_notifier_chain_register(&panic_notifier_list, &mi_trace_panic_blk);

	ret = mi_trace_create_procfs();
	if (ret)
		goto procfs_create_fail;

	return 0;

procfs_create_fail:
	atomic_notifier_chain_unregister(&panic_notifier_list, &mi_trace_panic_blk);
	unhook_mi_trace_events();

hook_events_fail:
	for (i = 0; i < TR_MAX; i++)
		msm_minidump_remove_region(&mtp.md_entry[i]);

	return ret;
}

static int mi_trace_remove(struct platform_device *pdev)
{
	enabled = 0;
	mi_trace_destory_procfs();
	atomic_notifier_chain_unregister(&panic_notifier_list, &mi_trace_panic_blk);
	unhook_mi_trace_events();
	for (int i = 0; i < TR_MAX; i++)
		msm_minidump_remove_region(&mtp.md_entry[i]);

	pr_err("%s\n", __func__);
	return 0;
}

static const struct of_device_id mi_trace_match_table[] = {
	{.compatible = "xiaomi,mi_trace"},
	{},
};

static struct platform_driver mi_trace_driver = {
	.probe = mi_trace_probe,
	.remove = mi_trace_remove,
	.driver         = {
		.name = "xiaomi_mi_trace",
		.of_match_table = mi_trace_match_table
	},
};

module_platform_driver(mi_trace_driver);
MODULE_DESCRIPTION("Register Mi Trace driver");
MODULE_LICENSE("GPL v2");
