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

#include <linux/module.h>
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

#include <linux/memblock.h>		/* for memblock_free */
#include <linux/arm-smccc.h> /* for smc call*/
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for smc call cases*/

#include <helio-dvfsrc-opp.h>
#ifdef CONFIG_MTK_CPU_FREQ
#include "cpu_ctrl.h"
#endif /* CONFIG_MTK_CPU_FREQ */

#ifdef CONFIG_MTK_SCHED_CPULOAD
#include <mt-plat/mtk_sched.h>
#endif

#include <linux/kernel.h>
#ifndef CONFIG_ARM64
#include <asm/opcodes-sec.h>
#include <asm/opcodes-virt.h>
#endif
#include <mt-plat/mtk_secure_api.h>
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
#define AMMS_STRESS 0

static struct task_struct *amms_task;
bool amms_static_free;
static const struct of_device_id amms_of_ids[] = {
	{ .compatible = "mediatek,amms", },
	{}
};
static unsigned int amms_freq_up = 1;
static unsigned int amms_vcore_up = 1;
static unsigned int amms_force_timeout;
static unsigned int amms_alloc_count;
static unsigned int amms_dealloc_count;
static unsigned int amms_irq_count;
#ifdef CONFIG_MTK_CPU_FREQ
#if AMMS_STRESS
static unsigned int amms_pos_stress_operation;
#endif
#endif

//static int amms_bind_cpu = -1;
static struct device *amms_dev;
static int amms_irq_num;
#ifdef CONFIG_MTK_CPU_FREQ
#if AMMS_STRESS
static struct timer_list amms_pos_stress_timer;
#endif
#endif

static struct cma *ccci_share_cma;
int ccci_share_cma_init;
phys_addr_t pos_addr, pos_length;
unsigned long long amms_seq_id;
#ifdef CONFIG_MTK_CPU_FREQ
static struct ppm_limit_data *amms_freq_to_set;
#endif
static unsigned long long t_before, t_after;
#if 1
static struct pm_qos_request amms_vcore_req;
static struct pm_qos_request amms_ddr_req;
#endif

int check_page_ref(phys_addr_t start, phys_addr_t length
	, int dump)
{
	struct page *page;
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
	struct page *pages;
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

#ifdef CONFIG_PM_SLEEP
	mutex_lock(&pm_mutex);
	amms_cma_restrict_gfp_mask();
#endif
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
#ifdef CONFIG_PM_SLEEP
	amms_cma_restore_gfp_mask();
	mutex_unlock(&pm_mutex);
#endif
		return 0;
	}
#ifdef CONFIG_PM_SLEEP
	amms_cma_restore_gfp_mask();
	mutex_unlock(&pm_mutex);
#endif
	addr = page_to_phys(pages);
	return addr;
}

int amms_cma_free(phys_addr_t addr, unsigned int size)
{
	struct page *pages;
	int count = size / PAGE_SIZE;
	bool rs;

	trace_amms_event_free(amms_seq_id);
	pages = phys_to_page(addr);
	if ((ccci_share_cma_init == 0) || (ccci_share_cma == 0)) {
		pr_info("ccci_share_cma_init failed\n");
		return -1;
	}
	pr_info("%s: addr=0x%pa size=0x%x\n", __func__,
		&addr, size);
	rs = cma_release(ccci_share_cma, pages, count);
	if (rs == false) {
		pr_info("cma_release failed addr=%pa size=%x\n",
		&addr, size);
		return -1;
	}
	return 0;
}

#ifdef CONFIG_MTK_CPU_FREQ
static void amms_freq_hold(void)
{
	int i, cluster_num;

	cluster_num = arch_get_nr_clusters();

	for (i = 0; i < cluster_num; i++) {
		amms_freq_to_set[i].min =
			mt_cpufreq_get_freq_by_idx(i, AMMS_CPU_FREQ_OPP);
		pr_debug("cluster %d opp min=%d\n", i, amms_freq_to_set[i].min);
		amms_freq_to_set[i].max = -1;
	}

	if (update_userlimit_cpu_freq(CPU_KIR_AMMS,
		cluster_num, amms_freq_to_set))
		pr_info("%s: failed\n", __func__);
}

static void amms_freq_release(void)
{
	int i, cluster_num;

	cluster_num = arch_get_nr_clusters();
	pr_info("%s:%d\n", __func__, __LINE__);
	for (i = 0; i < cluster_num; i++) {
		amms_freq_to_set[i].min = -1;
		amms_freq_to_set[i].max = -1;
	}
	if (update_userlimit_cpu_freq(CPU_KIR_AMMS,
		cluster_num, amms_freq_to_set))
		pr_info("%s: failed\n", __func__);

}
#if 0
static int param_set_amms_bind_cpu(const char *val,
		const struct kernel_param *kp)
{
	int retval = 0;
	int old_amms_bind_cpu = amms_bind_cpu;

	retval = param_set_int(val, kp);
	if (retval == 0) {
		if (amms_bind_cpu < num_online_cpus() - 1
			&& (amms_bind_cpu >= -1)) {
			kthread_bind(amms_task, amms_bind_cpu);
		} else {
			amms_bind_cpu = old_amms_bind_cpu;
			return -EINVAL;
		}
	}
	return retval;
}


struct kernel_param_ops param_ops_amms_freq_up = {
	.set = param_set_amms_bind_cpu,
	.get = param_get_uint,
};

module_param(amms_bind_cpu, int, 0644);

#endif

module_param(amms_freq_up, uint, 0644);
module_param(amms_vcore_up, uint, 0644);
module_param(amms_force_timeout, uint, 0644);
module_param(amms_alloc_count, uint, 0644);
module_param(amms_dealloc_count, uint, 0644);
module_param(amms_irq_count, uint, 0644);



#if CONFIG_SYSFS
#if AMMS_STRESS
void amms_pos_stress_timer_call_back(unsigned long data)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	mt_secure_call(MTK_SIP_KERNEL_AMMS_POS_STRESS_TOUCH, 0, 0, 0, 0);
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
#endif
static ssize_t amms_version_show(struct module_attribute *attr,
		struct module_kobject *kobj, char *buf)
{
	return snprintf(buf, 5, "%s\n", "1.0");
}

#if AMMS_STRESS
static ssize_t amms_pos_stress_show(struct module_attribute *attr,
		struct module_kobject *kobj, char *buf)
{
	return snprintf(buf, 3, "%d\n", amms_pos_stress_operation);
}

static ssize_t amms_pos_stress_store(struct module_attribute *attr,
		struct module_kobject *kobj, const char *buf, size_t count)
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
#endif
static struct module_attribute amms_version_attribute =
	__ATTR(amms_version, 0400, amms_version_show, NULL);

#if AMMS_STRESS
static struct module_attribute amms_pos_stress_attribute =
	__ATTR(amms_pos_stress, 0600,
	amms_pos_stress_show, amms_pos_stress_store);
#endif
static struct attribute *attrs[] = {
	&amms_version_attribute.attr,
#if AMMS_STRESS
	&amms_pos_stress_attribute.attr,
#endif
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

RESERVEDMEM_OF_DECLARE(legacy_memory_ccci_share, "mediatek,md_smem_ncache",
			memory_ccci_share_init);
RESERVEDMEM_OF_DECLARE(memory_ccci_share, "mediatek,ap_md_nc_smem",
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
	int ret;
#ifdef CONFIG_MTK_SCHED_CPULOAD
	int cpu;
#endif
	const char msg2[] = "AMMS timeout %lld us .\nCRDISPATCH_KEY:AMMS\n";

	ret = sprintf(msg, msg2, (t_after - t_before));
	if (ret < 0) {
		pr_info("%s: sprintf fail\n", __func__);
		return;
	}
	if ((t_after  - t_before) > TIMEOUT_WARNING) {
		pr_info("%s: timeout happened %lld us\n",
			__func__, (t_after - t_before));
		trace_amms_event_alloc_timeout(amms_seq_id);
/*
		tracing_off();
*/
#ifdef CONFIG_MTK_SCHED_CPULOAD
		for_each_online_cpu(cpu) {
			pr_info("cpu %d, loading %d\n", cpu,
				sched_get_cpu_load(cpu));
		}
#endif
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
	pr_info("%s:pending = %lld\n", __func__, (long long)pending);

	// Not support clear pending for legacy chip
	if ((((long long)pending) != AMMS_PENDING_DRDI_FREE_BIT)
	&& (((long long)pending) != AMMS_PENDING_POS_DEALLOC_BIT)
	&& (((long long)pending) != AMMS_PENDING_POS_ALLOC_BIT)) {
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
			} else {
				pr_info("AMMS: error addr and length is not set properly\n");
				pr_info("can not free_reserved_memory\n");
			}
		} else {
			pr_info("amms: static memory already free, should not happened\n");
		}
		return;
	}

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
		pr_info("amms: finish deallocate pending interrupt addr=0x%pa length=0x%pa\n",
		&pos_addr, &pos_length);
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
			pr_notice("pos_addr=%pa pos_length=%pa pos_alloc_addr=%pa\n",
				&pos_addr, &pos_length
				, &pos_alloc_addr);
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
	unsigned int do_freq_up, do_vcore_up;

	while (1) {
		do_freq_up = amms_freq_up;
		do_vcore_up = amms_vcore_up;
#if 1
		if (do_vcore_up) {
			pm_qos_update_request(&amms_vcore_req, VCORE_OPP_0);
			pm_qos_update_request(&amms_ddr_req, DDR_OPP_0);
		}
#endif

#ifdef CONFIG_MTK_CPU_FREQ
		if (do_freq_up)
			amms_freq_hold();
#endif
		amms_handle_event();
#ifdef CONFIG_MTK_CPU_FREQ
		if (do_freq_up)
			amms_freq_release();
#endif
#if 1
		if (do_vcore_up) {
			pm_qos_update_request(&amms_ddr_req, DDR_OPP_UNREQ);
			pm_qos_update_request(&amms_vcore_req, VCORE_OPP_UNREQ);
		}
#endif
/*
		tracing_off();
*/
		set_current_state(TASK_INTERRUPTIBLE);
		enable_irq(amms_irq_num);
		schedule();
	}
	return 0;
}
static int __init amms_probe(struct platform_device *pdev)
{
#ifdef CONFIG_MTK_CPU_FREQ
	int cluster_num;
#endif
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

#ifdef CONFIG_MTK_CPU_FREQ
	cluster_num = arch_get_nr_clusters();
	amms_freq_to_set = kcalloc(cluster_num,
				sizeof(struct ppm_limit_data), GFP_KERNEL);

	if (!amms_freq_to_set) {
		pr_info("amms:%s: allocate memory failed\n", __func__);
		return -EINVAL;
	}
#endif

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
#if 1
	pm_qos_add_request(&amms_vcore_req,
	PM_QOS_VCORE_OPP, PM_QOS_VCORE_OPP_DEFAULT_VALUE);
	pm_qos_add_request(&amms_ddr_req,
	PM_QOS_DDR_OPP, PM_QOS_DDR_OPP_DEFAULT_VALUE);
#endif
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
#ifdef CONFIG_MTK_CPU_FREQ
#if CONFIG_SYSFS
module_init(amms_sysfs_init);
#endif
#endif

MODULE_DESCRIPTION("MEDIATEK Module AMMS Driver");
MODULE_AUTHOR("<Johnson.Lin@mediatek.com>");
