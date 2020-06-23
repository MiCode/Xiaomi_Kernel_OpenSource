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

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>       /* min() */
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
#include <mt-plat/mtk_secure_api.h>
#include <mt-plat/aee.h>
#include <asm/pgtable.h>
#include <linux/vmalloc.h>
#include <linux/page-flags.h>
#include <linux/mmzone.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/spinlock.h>


#include <linux/kernel.h>
#ifndef CONFIG_ARM64
#include <asm/opcodes-sec.h>
#include <asm/opcodes-virt.h>
#endif
#include <linux/delay.h>
#define CREATE_TRACE_POINTS
#include <trace/events/mtk_amms.h>

#define AMMS_PENDING_DRDI_FREE_BIT (1<<0)
#define AMMS_PENDING_POS_DEALLOC_BIT (1<<1)
#define AMMS_PENDING_POS_ALLOC_BIT (1<<2)
#define AMMS_CMA_RETRY_COUNT 6
#define AMMS_CPU_FREQ_OPP (0)
#define EMI_MPU_ALIGN_ORDER (0)
/*timeout warning set to 80ms*/
#define TIMEOUT_WARNING (80000000)

static struct task_struct *amms_task;
bool amms_static_free;
static const struct of_device_id amms_of_ids[] = {
	{ .compatible = "mediatek,amms", },
	{}
};
static unsigned int amms_force_timeout;
static unsigned int amms_alloc_count;
static unsigned int amms_dealloc_count;
static unsigned int amms_irq_count;
static unsigned int amms_pos_stress_operation;

//static int amms_bind_cpu = -1;
static struct device *amms_dev;
static int amms_irq_num;
static struct timer_list amms_pos_stress_timer;

static struct cma *ccci_share_cma;
int ccci_share_cma_init;
phys_addr_t pos_addr, pos_length;
unsigned long long amms_seq_id;
static unsigned long long t_before, t_after;

int check_page_ref(phys_addr_t start, phys_addr_t length
	, int dump)
{
	struct page *page = NULL;
	int i = length >> PAGE_SHIFT;
	int count = 0;
	int pin_magic = 2;

	for (i--; i >= 0; i--) {
		page = phys_to_page(start + (phys_addr_t)(i * 4096));
		if (PageCompound(page) && PageHead(page)
			&& page_ref_count(page) > pin_magic)
			count += (1<<compound_order(page));
		else if (page_ref_count(page) > pin_magic)
			count++;
		if (dump) {
#ifdef CONFIG_PAGE_OWNER
			if (page_ref_count(page) > pin_magic) {
				pr_info("dump pin page owner\n");
				dump_page_owner(page);
			}
#endif
		}
	}
	return count;
}


phys_addr_t amms_cma_allocate(unsigned int size)
{
	int count = size >> PAGE_SHIFT;
	struct page *pages = NULL;
	struct page **array_pages;
	phys_addr_t addr;
	int use_count;
	int retry_count = 0, i;
	dma_addr_t dma_addr;

	pr_info("%s:%d size=0x%x\n", __func__, __LINE__, size);
	if ((ccci_share_cma_init == 0) || (ccci_share_cma == 0)) {
		pr_info("ccci_share_cma_init failed\n");
		return (phys_addr_t)0;
	}

	if (size % PAGE_SIZE != 0)
		pr_info("%s:not page alignment\n", __func__);

retry:
	use_count = check_page_ref(pos_addr, pos_length, 0);
	trace_amms_event_cma_alloc(amms_seq_id, use_count, count);
	pages = cma_alloc(ccci_share_cma, count, EMI_MPU_ALIGN_ORDER,
		GFP_KERNEL);
	trace_amms_event_cma_alloc_end(amms_seq_id);
	pr_info("%s:%d use_count=%d\n", __func__, __LINE__, use_count);

	if (pages) {
		array_pages = kmalloc_array(count,
			sizeof(struct page *), GFP_KERNEL);
		pr_debug("checking if array_pages allocated success\n");
		if (!array_pages) {
			pr_info("%s prepare array_pages failed can not flush cache\n",
				__func__);
		} else {
			for (i = 0; i < count; i++)
				array_pages[i] = pages+i;
			pr_debug("flush cache now\n");
			for (i = 0; i < count; i++) {
				dma_addr = dma_map_page(amms_dev,
					array_pages[i], 0, PAGE_SIZE,
						DMA_TO_DEVICE);
				if (!dma_addr)
					pr_info("dma_map_page failed\n");
				dma_unmap_page(amms_dev, dma_addr,
					PAGE_SIZE, DMA_TO_DEVICE);
			}
			pr_debug("flush cache done\n");
			kfree(array_pages);
		}
		trace_amms_event_alloc_success(amms_seq_id);
	} else {
		pr_info("%s:cma_alloc failed\n", __func__);
		trace_amms_event_cma_alloc_failed(amms_seq_id);
		usleep_range(10000, 50000);
		if (retry_count <= AMMS_CMA_RETRY_COUNT) {
			retry_count++;
			use_count = check_page_ref(pos_addr, pos_length, 1);
			pr_info("%s:%d use_count=%d\n", __func__,
				__LINE__, use_count);
			pr_info("%s:go retry\n", __func__);
			goto retry;
		}
		return 0;
	}
	addr = page_to_phys(pages);
	return addr;
}

int amms_cma_free(phys_addr_t addr, unsigned int size)
{
	struct page *pages = NULL;
	int count = size / PAGE_SIZE;
	bool rs = false;

	trace_amms_event_free(amms_seq_id);
	pages = phys_to_page(addr);
	if ((ccci_share_cma_init == 0) || (ccci_share_cma == 0)) {
		pr_info("ccci_share_cma_init failed\n");
		return -1;
	}
	pr_info("%s: addr=0x%p size=0x%x\n", __func__,
		(void *)addr, size);
	rs = cma_release(ccci_share_cma, pages, count);
	if (rs == false) {
		pr_info("cma_release failed addr=%p size=%x\n",
		(void *)addr, size);
		return -1;
	}
	return 0;
}


module_param(amms_force_timeout, uint, 0644);
module_param(amms_alloc_count, uint, 0644);
module_param(amms_dealloc_count, uint, 0644);
module_param(amms_irq_count, uint, 0644);



#if CONFIG_SYSFS

void amms_pos_stress_timer_call_back(unsigned long data)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	/*mt_secure_call(MTK_SIP_KERNEL_AMMS_POS_STRESS_TOUCH, 0, 0, 0, 0);*/
	if (mod_timer(&amms_pos_stress_timer,
		jiffies + msecs_to_jiffies(1000*60)))
		pr_info("%s:Error\n", __func__);
}

void amms_pos_stress_timer_control(int operation)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (operation) {
		init_timer_deferrable(&amms_pos_stress_timer);
		amms_pos_stress_timer.function =
			amms_pos_stress_timer_call_back;
		amms_pos_stress_timer.data = 0;
		amms_pos_stress_timer.expires = 0;
		if (mod_timer(&amms_pos_stress_timer,
			jiffies + msecs_to_jiffies(1000*10)))
			pr_info("%s:Error\n", __func__);
	} else {
		del_timer_sync(&amms_pos_stress_timer);
	}
}

static ssize_t amms_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 5, "%s\n", "1.0");
}

static ssize_t amms_pos_stress_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 3, "%d\n", amms_pos_stress_operation);
}

static ssize_t amms_pos_stress_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long operation = 0;
	int status;

	if (!ccci_share_cma_init) {
		pr_info("%s:!ccci_share_cma_init\n", __func__);
		return -EINVAL;
	}
	if (count > 2)
		return -EINVAL;
	if (buf[0] != '0' && buf[0] != '1')
		return -EINVAL;
	status = kstrtoul(buf, 0, &operation);
	if (status)
		return -EINVAL;
	if (operation) {
		if (amms_pos_stress_operation) {
			pr_info("%s:stress already started\n", __func__);
			return -EINVAL;
		}
		amms_pos_stress_timer_control(1);

	} else {
		if (!amms_pos_stress_operation) {
			pr_info("%s:stress already stopped\n", __func__);
			return -EINVAL;
		}
		amms_pos_stress_timer_control(0);
	}
	amms_pos_stress_operation = operation;
	return count;
}

static struct kobj_attribute amms_version_attribute =
	__ATTR(amms_version, 0600, amms_version_show, NULL);

static struct kobj_attribute amms_pos_stress_attribute =
	__ATTR(amms_pos_stress, 0600,
	amms_pos_stress_show, amms_pos_stress_store);
static struct attribute *attrs[] = {
	&amms_version_attribute.attr,
	&amms_pos_stress_attribute.attr,
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

	pr_info("%s: done.\n", __func__);
	return 0;
}

#endif

static int __init memory_ccci_share_init(struct reserved_mem *rmem)
{
	int ret;

	pr_info("%s, name: %s, base: 0x%pa, size: 0x%pa\n",
		 __func__, rmem->name,
		 &rmem->base, &rmem->size);


	pos_addr = mt_secure_call(
		MTK_SIP_KERNEL_AMMS_GET_MD_POS_ADDR, 0, 0, 0, 0);
	pos_length = mt_secure_call(
		MTK_SIP_KERNEL_AMMS_GET_MD_POS_LENGTH, 0, 0, 0, 0);
	pr_info("pos_addr=%pa pos_length=%pa\n",
		&pos_addr, &pos_length);

	if ((long long)pos_addr == -1 || (long long)pos_addr == 0xffffffff) {
		pr_info("%s: no support POS\n", __func__);
		return 0;
	}

	if (pos_addr != 0)
	/* init cma area */
		ret = cma_init_reserved_mem(rmem->base,
		rmem->size, 0, "AMMS_POS", &ccci_share_cma);
	else {
		pr_info("%s MD_POS no support,no CMA init\n", __func__);
		return 0;
	}

	if (ret) {
		pr_info("%s cma failed, ret: %d\n", __func__, ret);
		return 1;
	}
	ccci_share_cma_init = 1;
	return 0;
}

RESERVEDMEM_OF_DECLARE(memory_ccci_share, "mediatek,md_smem_ncache",
			memory_ccci_share_init);
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

static inline void check_amms_timeout_warn(void)
{
	char msg[128];
	const char msg2[] = "AMMS timeout %lld us .\nCRDISPATCH_KEY:AMMS\n";

	sprintf(msg, msg2, (t_after - t_before));
	if ((t_after  - t_before) > TIMEOUT_WARNING) {
		pr_info("%s: timeout happened %lld us\n",
			__func__, (t_after - t_before));
		trace_amms_event_alloc_timeout(amms_seq_id);

/*		tracing_off(); */

		pr_info("amms_cma_alloc timeout %lld\n", t_after - t_before);
		aee_kernel_warning_api(__FILE__, __LINE__,
			DB_OPT_DEFAULT|DB_OPT_DUMPSYS_ACTIVITY
			|DB_OPT_LOW_MEMORY_KILLER
			| DB_OPT_PID_MEMORY_INFO /*for smaps and hprof*/
			| DB_OPT_PROCESS_COREDUMP
			| DB_OPT_DUMPSYS_SURFACEFLINGER
			| DB_OPT_DUMPSYS_GFXINFO
			| DB_OPT_DUMPSYS_PROCSTATS,
/*			|DB_OPT_FTRACE,*/
			msg,
			"[AMMS ALLOCATE TIMEOUT]: AMMS allocate timeout\n");
	}
}

void amms_handle_event(void)
{

	phys_addr_t addr = 0, length = 0;
	unsigned long long pending;
	phys_addr_t pos_alloc_addr;
	int use_count;

	amms_seq_id = mt_secure_call(MTK_SIP_KERNEL_AMMS_GET_SEQ_ID
			, 0, 0, 0, 0);
	pending = mt_secure_call(MTK_SIP_KERNEL_AMMS_GET_PENDING, 0, 0, 0, 0);
	pr_info("%s:pending = 0x%llx\n", __func__, pending);
	if (pending & AMMS_PENDING_DRDI_FREE_BIT) {
		/*below part is for staic memory free */
		if (!amms_static_free) {
			addr = mt_secure_call(MTK_SIP_KERNEL_AMMS_GET_FREE_ADDR,
			0, 0, 0, 0);
			length = mt_secure_call(
			MTK_SIP_KERNEL_AMMS_GET_FREE_LENGTH,
			0, 0, 0, 0);
			if (pfn_valid(__phys_to_pfn(addr))
				&& pfn_valid(__phys_to_pfn(
				addr + length - 1))) {
				pr_info("%s:addr=%pa length=%pa\n", __func__,
				&addr, &length);
				free_reserved_memory(addr, addr+length);
				amms_static_free = true;
				mt_secure_call(MTK_SIP_KERNEL_AMMS_ACK_PENDING,
					(AMMS_PENDING_DRDI_FREE_BIT), 0, 0, 0);
			} else {
				pr_info("AMMS: error addr and length is not set properly\n");
				pr_info("can not free_reserved_memory\n");
			}
		} else {
			mt_secure_call(MTK_SIP_KERNEL_AMMS_ACK_PENDING,
				(AMMS_PENDING_DRDI_FREE_BIT), 0, 0, 0);
			pr_info("amms: static memory already free, should not happened\n");
		}
	} else if (pending & AMMS_PENDING_POS_DEALLOC_BIT) {
		if (!ccci_share_cma_init) {
			pr_info("not ccci_share_cma_init, not apply\n");
			return;
		}
		mt_secure_call(MTK_SIP_KERNEL_AMMS_ACK_PENDING,
			(AMMS_PENDING_POS_DEALLOC_BIT),
			0, 0, 0);
		amms_cma_free(pos_addr, pos_length);
		amms_dealloc_count++;
		pr_info("amms: finish deallocate pending interrupt addr=0x%p length=0x%p\n",
		(void *)pos_addr, (void *)pos_length);
	} else if (pending & AMMS_PENDING_POS_ALLOC_BIT)  {

		pr_info("amms: receive allocate pending interrupt\n");
		if (!ccci_share_cma_init) {
			pr_info("not ccci_share_cma_init, not apply\n");
			return;
		}
		t_before = sched_clock();
		pos_alloc_addr = amms_cma_allocate(pos_length);
		/*force timeout but not modify result*/
		if (amms_force_timeout)
			msleep(120);
		/*force timeout but modify result to allocate failed*/
		if (amms_force_timeout == 2) {
			if (pos_alloc_addr) {
				amms_cma_free(pos_alloc_addr, pos_length);
				pos_alloc_addr = 0;
			}
		}

		/*one shut force timeout and result in allocate failed*/
		if (amms_force_timeout == 3) {
			if (pos_alloc_addr) {
				amms_cma_free(pos_alloc_addr, pos_length);
				pos_alloc_addr = 0;
				amms_force_timeout  = 0;
			}
		}

		t_after = sched_clock();
		amms_alloc_count++;

		if (pos_alloc_addr && pos_addr == pos_alloc_addr) {
			pr_notice("amms: allocate the same address");
			pr_info("%s:success ack pending\n", __func__);
			mt_secure_call(MTK_SIP_KERNEL_AMMS_ACK_PENDING,
				(AMMS_PENDING_POS_ALLOC_BIT), 0, 0, 0);
			check_amms_timeout_warn();

		} else if (pos_alloc_addr && pos_addr != pos_alloc_addr) {
			pr_notice("amms: allocate different address");
			pr_notice("pos_addr=%p pos_length=%p pos_alloc_addr=%p\n",
				(void *)pos_addr, (void *)pos_length
				, (void *)pos_alloc_addr);
			pos_addr = pos_alloc_addr;
			mt_secure_call(MTK_SIP_KERNEL_AMMS_MD_POS_ADDR,
				pos_addr, 0, 0, 0);
			mt_secure_call(MTK_SIP_KERNEL_AMMS_ACK_PENDING,
				(AMMS_PENDING_POS_ALLOC_BIT), 0, 0, 0);
			check_amms_timeout_warn();
		} else {
			use_count = check_page_ref(pos_addr, pos_length, 1);
			pr_info("amms:%s cma allocate failed use_count=%d\n",
				__func__, use_count);
			aee_kernel_warning_api(__FILE__, __LINE__,
				DB_OPT_DEFAULT|DB_OPT_DUMPSYS_ACTIVITY
				|DB_OPT_LOW_MEMORY_KILLER
				| DB_OPT_PID_MEMORY_INFO /*for smaps and hprof*/
				| DB_OPT_PROCESS_COREDUMP
				| DB_OPT_DUMPSYS_SURFACEFLINGER
				| DB_OPT_DUMPSYS_GFXINFO
				| DB_OPT_DUMPSYS_PROCSTATS,
				"AMMS allocate memory failed.\nCRDISPATCH_KEY:AMMS\n",
				"[AMMS ALLOCATE FAILED]: AMMS allocate failed\n");
		}
	}

	if (pending & (~(AMMS_PENDING_POS_ALLOC_BIT|
		AMMS_PENDING_POS_DEALLOC_BIT|AMMS_PENDING_DRDI_FREE_BIT)))
		pr_info("amms:unknown pending interrupt\n");

}


static int amms_task_process(void *p)
{
	while (1) {

		amms_handle_event();

/*		tracing_off(); */
		set_current_state(TASK_INTERRUPTIBLE);
		enable_irq(amms_irq_num);
		schedule();
	}
	return 0;
}
static int __init amms_probe(struct platform_device *pdev)
{
	int ret;

	amms_irq_num = platform_get_irq(pdev, 0);
	if (amms_irq_num == -ENXIO) {
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
	pr_info("amms_dev->dma_ops=0x%pS\n", amms_dev->dma_ops);

	pr_info("amms irq num %d\n", amms_irq_num);

	if (request_irq(amms_irq_num, (irq_handler_t)amms_irq_handler,
		IRQF_TRIGGER_NONE, "amms_irq", NULL) != 0) {
		pr_info("Fail to request amms_irq interrupt!\n");
		return -EBUSY;
	}

	return 0;
}

static void __exit amms_exit(void)
{
	pr_notice("amms: exited");
}
static int amms_remove(struct platform_device *dev)
{
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
	struct sched_param param = {.sched_priority = 98 };

	amms_task = kthread_create(amms_task_process,
			NULL, "amms_task");
	if (!amms_task)
		pr_info("amms_task thread create failed\n");

	sched_setscheduler(amms_task, SCHED_FIFO, &param);
	/*kthread_bind(amms_task, 0);*/
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

arch_initcall(amms_init);
#if CONFIG_SYSFS
module_init(amms_sysfs_init);
#endif

MODULE_DESCRIPTION("MEDIATEK Module AMMS Driver");
MODULE_AUTHOR("<johnson.lin@mediatek.com>");
