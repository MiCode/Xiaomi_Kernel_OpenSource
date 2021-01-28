// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>       /* min() */
#include <linux/memblock.h>
#include <linux/uaccess.h>      /* copy_to_user() */
#include <linux/sched.h>        /* TASK_INTERRUPTIBLE/signal_pending/schedule */
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>
#include <linux/poll.h>
#include <linux/io.h>           /* ioremap() */
#include <linux/cma.h>
#include <linux/suspend.h>
#include <asm/cacheflush.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/seq_file.h>
#include <asm/setup.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <asm/page.h>
#include <linux/atomic.h>
#include <linux/page_owner.h>
#include <linux/irq.h>
#include <linux/syscore_ops.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <asm/pgtable.h>
#include <linux/vmalloc.h>
#include <linux/page-flags.h>
#include <linux/mmzone.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/spinlock.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>


#include <linux/kernel.h>
#include <linux/delay.h>
#define CREATE_TRACE_POINTS
#include <trace/events/mtk_amms.h>
#include "memory-amms.h"

#define AMMS_PENDING_DRDI_FREE_BIT (1<<0)

static struct task_struct *amms_task;
bool amms_static_free;
static const struct of_device_id amms_of_ids[] = {
	{ .compatible = "mediatek,amms", },
	{}
};
static unsigned int amms_irq_count;

//static int amms_bind_cpu = -1;
static struct device *amms_dev;
static int amms_irq_num;

unsigned long long amms_seq_id;

#if CONFIG_SYSFS

module_param(amms_irq_count, uint, 0644);

static ssize_t amms_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 5, "%s\n", "1.0");
}


static struct kobj_attribute amms_version_attribute =
	__ATTR(amms_version, 0600, amms_version_show, NULL);

static struct attribute *attrs[] = {
	&amms_version_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init amms_sysfs_init(void)
{
	struct kobject *kobj;

	kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (kobj) {
		if (sysfs_create_group(kobj, &attr_group)) {
			pr_notice("AMMS: sysfs create sysfs failed\n");
			return -ENOMEM;
		}
	} else {
		pr_notice("AMMS: Cannot find module %s object\n",
				KBUILD_MODNAME);
		return -EINVAL;
	}

	pr_debug("%s: done.\n", __func__);
	return 0;
}

#endif


static irqreturn_t amms_irq_handler(int irq, void *dev_id)
{
	trace_amms_event_receive_irq(amms_seq_id);
	if (amms_task) {
		disable_irq_nosync(irq);
		wake_up_process(amms_task);
	} else
		pr_info("%s:amms_task is null\n", __func__);
	amms_irq_count++;
	return IRQ_HANDLED;
}

int free_reserved_memory(phys_addr_t start_phys,
				phys_addr_t end_phys)
{

	phys_addr_t pos;
	unsigned long pages = 0;

	if (end_phys <= start_phys) {

		pr_notice("%s end_phys is smaller than start_phys start_phys:%pa end_phys:%pa\n"
			, __func__, &start_phys, &end_phys);
		return -1;
	}

	if (!memblock_is_region_reserved(start_phys, end_phys - start_phys)) {
		pr_notice("%s:not reserved memory phys_start:%pa phys_end:%pa\n"
			, __func__, &start_phys, &end_phys);
		return -1;
	}

	memblock_free(start_phys, (end_phys - start_phys));

	for (pos = start_phys; pos < end_phys; pos += PAGE_SIZE, pages++)
		free_reserved_page(phys_to_page(pos));

	if (pages)
		pr_info("Freeing modem memory: %ldK from phys %llx\n",
			pages << (PAGE_SHIFT - 10),
			(unsigned long long)start_phys);

	return 0;
}

void amms_handle_event(void)
{

	phys_addr_t addr = 0, length = 0;
	unsigned long long pending;
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_AMMS_GET_SEQ_ID,
			0, 0, 0, 0, 0, 0, 0, &res);
	amms_seq_id = res.a0;
	arm_smccc_smc(MTK_SIP_KERNEL_AMMS_GET_PENDING,
			0, 0, 0, 0, 0, 0, 0, &res);
	pending = res.a0;
	pr_info("%s:pending = 0x%llx\n", __func__, pending);
	pr_info("%s:pending = %lld\n", __func__, (long long)pending);

	// Not support clear pending for legacy chip
	if (((long long)pending) != AMMS_PENDING_DRDI_FREE_BIT) {
		if (!amms_static_free) {
			arm_smccc_smc(MTK_SIP_KERNEL_AMMS_GET_FREE_ADDR,
			0, 0, 0, 0, 0, 0, 0, &res);
			addr = res.a0;
			arm_smccc_smc(
			MTK_SIP_KERNEL_AMMS_GET_FREE_LENGTH,
			0, 0, 0, 0, 0, 0, 0, &res);
			length = res.a0;
			if (pfn_valid(__phys_to_pfn(addr))
				&& pfn_valid(__phys_to_pfn(
				addr + length - 1))) {
				pr_info("%s:addr=%pa length=%pa\n", __func__,
				&addr, &length);
				free_reserved_memory(addr, addr+length);
				amms_static_free = true;
			} else {
				pr_info("AMMS: error addr and length is not set properly\n");
				pr_info("can not free_reserved_memory\n");
			}
		}
		return;
	}
	if (pending & AMMS_PENDING_DRDI_FREE_BIT) {
		/*below part is for staic memory free */
		if (!amms_static_free) {
			arm_smccc_smc(MTK_SIP_KERNEL_AMMS_GET_FREE_ADDR,
			0, 0, 0, 0, 0, 0, 0, &res);
			addr = res.a0;
			arm_smccc_smc(
			MTK_SIP_KERNEL_AMMS_GET_FREE_LENGTH,
			0, 0, 0, 0, 0, 0, 0, &res);
			length = res.a0;
			if (pfn_valid(__phys_to_pfn(addr))
				&& pfn_valid(__phys_to_pfn(
				addr + length - 1))) {
				pr_info("%s:addr=%pa length=%pa\n", __func__,
				&addr, &length);
				free_reserved_memory(addr, addr+length);
				amms_static_free = true;
				arm_smccc_smc(MTK_SIP_KERNEL_AMMS_ACK_PENDING,
					AMMS_PENDING_DRDI_FREE_BIT, 0,
					0, 0, 0, 0, 0, &res);
			} else {
				pr_info("AMMS: error addr and length is not set properly\n");
				pr_info("can not free_reserved_memory\n");
			}
		} else {
			arm_smccc_smc(MTK_SIP_KERNEL_AMMS_ACK_PENDING,
				AMMS_PENDING_DRDI_FREE_BIT,
				0, 0, 0, 0, 0, 0, &res);
			pr_info("amms: static memory already free, should not happened\n");
		}
	}

	if (pending & (~(AMMS_PENDING_DRDI_FREE_BIT)))
		pr_info("amms:unknown pending interrupt\n");

}


static int amms_task_process(void *p)
{
	while (1) {
		amms_handle_event();
		set_current_state(TASK_INTERRUPTIBLE);
		enable_irq(amms_irq_num);
		schedule();
		if (kthread_should_stop())
			do_exit(0);
	}
	return 0;
}
static int __init amms_probe(struct platform_device *pdev)
{
	int ret;
	struct sched_param param = {.sched_priority = 98 };

	amms_irq_num = platform_get_irq(pdev, 0);
	if (amms_irq_num < 0) {
		pr_info("Fail to get amms irq number from device tree\n");
		WARN_ON(amms_irq_num == -ENXIO);
		return -EINVAL;
	}

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));

	if (ret) {
		pr_info("fatal error dma_set_mask failed %d\n", ret);
		return -EINVAL;
	}

	amms_dev = &pdev->dev;
	pr_info("amms_dev->dma_ops=%pS\n", amms_dev->dma_ops);
	pr_info("amms irq num %d\n", amms_irq_num);

	amms_task = kthread_create(amms_task_process,
			NULL, "amms_task");
	if (!amms_task) {
		pr_info("amms_task thread create failed\n");
		return -EBUSY;
	}

	sched_setscheduler(amms_task, SCHED_FIFO, &param);

	if (request_irq(amms_irq_num, (irq_handler_t)amms_irq_handler,
		IRQF_TRIGGER_NONE, "amms_irq", NULL) != 0) {
		pr_info("Fail to request amms_irq interrupt!\n");
		return -EBUSY;
	}
#if CONFIG_SYSFS
	amms_sysfs_init();
#endif
	return 0;
}

static int amms_remove(struct platform_device *dev)
{
	if (amms_task)
		kthread_stop(amms_task);
	return 0;
}

/* variable with __init* or __refdata (see linux/init.h) or */
/* name the variable *_template, *_timer, *_sht, *_ops, *_probe, */
/* *_probe_one, *_console */
static struct platform_driver amms_driver_probe = {
	.probe = amms_probe,
	.remove = amms_remove,
	.driver = {
		.name = "amms",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = amms_of_ids,
#endif
	},
};

static int __init amms_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&amms_driver_probe);
	if (ret)
		pr_info("amms init FAIL, ret 0x%x!!!\n", ret);

	return ret;
}

/**
 *	vmap_reserved_mem - map reserved memory into virtually contiguous space
 *	@start:		start of reserved memory
 *	@size:		size of reserved memory
 *	@prot:		page protection for the mapping
 */
void *vmap_reserved_mem(phys_addr_t start, phys_addr_t size, pgprot_t prot)
{
	long i;
	long page_count;
	unsigned long pfn;
	void *vaddr = NULL;
	phys_addr_t addr = start;
	struct page *page;
	struct page **pages;

	page_count = DIV_ROUND_UP(size, PAGE_SIZE);
	pages = vmalloc(page_count * sizeof(struct page *));

	if (!pages)
		return NULL;

	for (i = 0; i < page_count; i++) {
		pfn = __phys_to_pfn(addr);
		page = pfn_to_page(pfn);
		pages[i] = page;
		addr += PAGE_SIZE;
	}

	vaddr = vmap(pages, page_count, VM_MAP, prot);
	vfree(pages);
	return vaddr;
}
EXPORT_SYMBOL(vmap_reserved_mem);

device_initcall(amms_init);

MODULE_DESCRIPTION("MEDIATEK Module AMMS Driver");
MODULE_AUTHOR("<johnson.lin@mediatek.com>");
