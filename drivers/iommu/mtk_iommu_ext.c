/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "mtk_iommu_ext.h"
#include "mach/pseudo_m4u.h"
#include "mach/mt_iommu_port.h"

#include <linux/slab.h>
#include <linux/sched/clock.h>
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/seq_file.h>
#include <mmprofile.h>

#define mmu_translation_log_format \
	"\nCRDISPATCH_KEY:M4U_%s\n<<TRANSLATION FAULT>> port=%s,iova=0x%lx,pa=0x%lx\n"

#define mmu_translation_log_format_secure \
	"\nCRDISPATCH_KEY:M4U_%s\n<<SECURE TRANSLATION FAULT>> port=%s,iova=0x%lx\n"

#define mmu_leakage_log_format \
	"\nCRDISPATCH_KEY:M4U_%s\n<<IOVA LEAKAGE>> port=%s size=%uKB\n"

#define config_port_log_format \
	"\nCRDISPATCH_KEY:M4U_%s\n<<CONFIG INVALID PORT NAME>> name=%s id=%u\n"

#define ERROR_LARB_PORT_ID M4U_PORT_NR

#define IOMMU_MAX_EVENT_COUNT 1024
struct dentry *iomu_dbgfs;

struct iommu_event_mgr_t {
	char name[11];
	unsigned int dump_trace;
	unsigned int dump_log;
};

static struct iommu_event_mgr_t event_mgr[IOMMU_EVENT_MAX];

struct iommu_event_t {
	unsigned int event_id;
	unsigned int time_low;
	unsigned int time_high;
	unsigned long data1;
	unsigned long data2;
	unsigned long data3;
};


struct iommu_global_t {
	unsigned int enable;
	unsigned int dump_enable;
	unsigned int start;
	unsigned int write_pointer;
	spinlock_t	lock;
	struct iommu_event_t *record;
};

static struct iommu_global_t iommu_globals;

static inline int mtk_iommu_get_tf_larb_port_idx(unsigned int m4uid, int tf_id)
{
	int i;

	for (i = 0; i < (M4U_PORT_NR + 1); i++) {
		if (iommu_port[i].tf_id == tf_id &&
		    iommu_port[i].m4u_id == m4uid)
			return i;
		if (((iommu_port[i].tf_id & 0xf80) >> 7) == TF_CCU_DISP &&
		    iommu_port[i].m4u_id == m4uid)
			return i;
		if (((iommu_port[i].tf_id & 0xf80) >> 7) == TF_CCU_MDP &&
		    iommu_port[i].m4u_id == m4uid)
			return i;
	}
	pr_notice("%s, %d err tf_id:0x%x", __func__, __LINE__, tf_id);
	return ERROR_LARB_PORT_ID;
}

int mtk_iommu_get_larb_port(unsigned int tf_id, unsigned int m4uid,
		unsigned int *larb, unsigned int *port)
{
	unsigned int idx;
#ifdef APU_IOMMU_INDEX
	if (m4uid >= APU_IOMMU_INDEX) {
		*larb = MTK_IOMMU_TO_LARB(M4U_PORT_APU);
		*port = MTK_IOMMU_TO_PORT(M4U_PORT_APU);
		return M4U_PORT_APU;
	}
#endif

	idx = mtk_iommu_get_tf_larb_port_idx(m4uid, tf_id);
	if (idx == ERROR_LARB_PORT_ID) {
		pr_notice("%s, %d err tf_id:0x%x", __func__, __LINE__, tf_id);
		return -1;
	}

	*larb = iommu_port[idx].larb_id;
	*port = iommu_port[idx].larb_port;

	return MTK_M4U_ID(*larb, *port);
}

char *mtk_iommu_get_mm_port_name(unsigned int m4uid, unsigned int tf_id)
{
	unsigned int idx;

	idx = mtk_iommu_get_tf_larb_port_idx(m4uid, tf_id);
	if (idx == ERROR_LARB_PORT_ID) {
		pr_notice("%s, %d err tf_id:0x%x, m4u:%d",
			  __func__, __LINE__, tf_id, m4uid);
		return "m4u_port_unknown";
	}

	return iommu_port[idx].name;
}

char *mtk_iommu_get_vpu_port_name(unsigned int tf_id)
{
	int i;

	for (i = 0; i < IOMMU_APU_AXI_PORT_NR; i++) {
		if (((tf_id & vpu_axi_bus_mask[i]) >> 7) ==
		    vpu_axi_bus_id[i])
			return vpu_axi_bus_name[i];
	}
	return "APU_UNKNOWN";
}

char *mtk_iommu_get_port_name(unsigned int m4u_id,
		unsigned int tf_id)
{
#ifdef APU_IOMMU_INDEX
	if (m4u_id >= APU_IOMMU_INDEX)
		return mtk_iommu_get_vpu_port_name(tf_id);
#endif

	return mtk_iommu_get_mm_port_name(m4u_id, tf_id);
}

static inline int mtk_iommu_larb_port_idx(int id)
{
	unsigned int larb, port;
	int index = -1;

	larb = MTK_IOMMU_TO_LARB(id);
	port = MTK_IOMMU_TO_PORT(id);

	if (larb >= MTK_IOMMU_LARB_NR)
		return ERROR_LARB_PORT_ID;

	if (mtk_iommu_larb_distance[larb] >= 0)
		index = mtk_iommu_larb_distance[larb] + port;

	if ((index >= M4U_PORT_NR) ||
	    (index < 0))
		return ERROR_LARB_PORT_ID;

	if ((iommu_port[index].larb_id == larb) &&
		(iommu_port[index].larb_port == port))
		return index;

	pr_info("[MTK_IOMMU] do not find index for id %d\n", id);
	return ERROR_LARB_PORT_ID;
}

char *iommu_get_port_name(int port)
{
	int idx;

	idx = mtk_iommu_larb_port_idx(port);
	if (idx >= M4U_PORT_NR ||
	    idx < 0) {
		pr_info("[MTK_IOMMU] %s fail, port=%d\n", __func__, port);
		return "m4u_port_unknown";
	}
	return iommu_port[idx].name;
}

bool report_custom_iommu_fault(
	unsigned int m4uid,
	void __iomem	*base,
	unsigned long	fault_iova,
	unsigned long	fault_pa,
	unsigned int	fault_id,
	bool is_vpu,
	bool is_sec)
{
	int idx;
	int port;
	char *name;

	if (is_vpu) {
		port = M4U_PORT_APU;
		idx = mtk_iommu_larb_port_idx(port);
		if (idx >= M4U_PORT_NR ||
		    idx < 0) {
			pr_info("[MTK_IOMMU] fail,iova 0x%lx, port %d\n",
				fault_iova, port);
			return -1;
		}
		name = mtk_iommu_get_vpu_port_name(fault_id);
	} else {
		idx = mtk_iommu_get_tf_larb_port_idx(m4uid, fault_id);
		if (idx == ERROR_LARB_PORT_ID) {
			pr_info("[MTK_IOMMU] fail,iova 0x%lx, port %d\n",
				fault_iova, fault_id);
			return false;
		}

		port = MTK_M4U_ID(iommu_port[idx].larb_id,
				  iommu_port[idx].larb_port);
		name = iommu_port[idx].name;
	}
	iommu_globals.enable = 0;

	if (iommu_port[idx].enable_tf && iommu_port[idx].fault_fn)
		iommu_port[idx].fault_fn(port,
				fault_iova,
				iommu_port[idx].fault_data);

	if (is_sec)
		mmu_aee_print(mmu_translation_log_format_secure,
		       name, name, fault_iova);
	else
		mmu_aee_print(mmu_translation_log_format,
		       name, name,
		       fault_iova, fault_pa);

	return true;
}

void report_custom_iommu_leakage(char *port_name,
	unsigned int size)
{
	if (!port_name)
		return;

	mmu_aee_print(mmu_leakage_log_format,
		      port_name, port_name, size);
}

void report_custom_config_port(char *port_name,
	char *err_name,
	unsigned int portid)
{
	if (!port_name || !err_name)
		return;

	mmu_aee_print(config_port_log_format,
		      port_name, err_name, portid);
}

bool enable_custom_tf_report(void)
{
	return true;
}

int mtk_iommu_register_fault_callback(int port,
				      mtk_iommu_fault_callback_t fn,
				      void *cb_data)
{
	int idx = mtk_iommu_larb_port_idx(port);

	if (idx >= M4U_PORT_NR ||
	    idx < 0) {
		pr_info("[MTK_IOMMU] %s fail, port=%d\n", __func__, port);
		return -1;
	}
	iommu_port[idx].fault_fn = fn;
	iommu_port[idx].fault_data = cb_data;
	return 0;
}

int mtk_iommu_unregister_fault_callback(int port)
{
	int idx = mtk_iommu_larb_port_idx(port);

	if (idx >= M4U_PORT_NR ||
	    idx < 0) {
		pr_info("[MTK_IOMMU] %s fail, port=%d\n", __func__, port);
		return -1;
	}
	iommu_port[idx].fault_fn = NULL;
	iommu_port[idx].fault_data = NULL;
	return 0;
}

int mtk_iommu_enable_tf(int port, bool fgenable)
{
	int idx = mtk_iommu_larb_port_idx(port);

	if (idx >= M4U_PORT_NR ||
	    idx < 0) {
		pr_info("[MTK_IOMMU] %s fail, port=%d\n", __func__, port);
		return -1;
	}

	iommu_port[idx].enable_tf = fgenable;
	return 0;
}

int mtk_iommu_iova_to_pa(struct device *dev,
			 dma_addr_t iova, unsigned long *pa)
{
	struct iommu_domain *domain;

	if (dev == NULL) {
		pr_notice("%s, %d, dev is null\n",
		       __func__, __LINE__);
		return -1;
	}

	domain = iommu_get_domain_for_dev(dev);

	if (domain)
		*pa = (unsigned long)iommu_iova_to_phys(domain, iova);

	if (!domain || !pa)
		return -1;

	return 0;
}

int mtk_iommu_iova_to_va(struct device *dev,
			 dma_addr_t iova,
			 unsigned long *map_va,
			 size_t size)
{
	struct iommu_domain *domain;
	unsigned int page_count;
	unsigned int i = 0;
	struct page **pages;
	void *va = NULL;
	phys_addr_t pa = 0;
	int ret = 0;

	if (map_va == NULL)
		return 1;

	if (dev == NULL || iova == 0) {
		pr_notice("%s, %d, invalid dev/iova:0x%lx\n",
		       __func__, __LINE__, iova);
		*map_va = 0;
		return 1;
	}

	domain = iommu_get_domain_for_dev(dev);

	if (domain)
		pa = iommu_iova_to_phys(domain, iova);

	if ((domain == NULL) || (pa == 0)) {
#ifdef IOMMU_DEBUG_ENABLED
		pr_notice("func %s dom: %p, iova:0x%lx, pa: 0x%lx\n",
		       __func__, domain, iova, (unsigned long)pa);
#endif
		*map_va = 0;
		return 1;
	}

	page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	pages = kmalloc((sizeof(struct page *) * page_count), GFP_KERNEL);
	if (!pages) {
		pr_notice("%s:alloc pages fail\n", __func__);
		*map_va = 0;
		return 1;
	}

	for (i = 0; i < page_count; i++) {
		pa = iommu_iova_to_phys(domain, iova + i * PAGE_SIZE);
		if (pa == 0) {
			ret = -1;
			pr_notice("func %s dom:%p, i:%u,  iova:0x%lx, pa: 0x%lx\n",
				  __func__, domain, i, iova + i * PAGE_SIZE, (unsigned long)pa);
			break;
		}
		pages[i] = pfn_to_page(pa >> PAGE_SHIFT);
	}

	if (ret) {
		*map_va = 0;
		kfree(pages);
		return 2;
	}

	va = vmap(pages, page_count, VM_MAP, PAGE_KERNEL);
#ifdef IOMMU_DEBUG_ENABLED
	if (va == 0)
		pr_notice("func %s map iova(0x%lx) fail to null\n",
		       __func__, (unsigned long)iova);
#endif
	*map_va = (uintptr_t)va;

	kfree(pages);
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS) || IS_ENABLED(CONFIG_PROC_FS)
static void process_dbg_opt(const char *opt)
{

}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	pr_debug("[extd] %s\n", cmd);

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

static char debug_buffer[128];
static ssize_t process_write(struct file *file, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	size_t ret = 0;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;

	process_dbg_cmd(debug_buffer);

	return ret;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
/* Debug FileSystem Routines */
struct dentry *mtk_iomu_dbgfs;
static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t debug_write(struct file *file, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	return process_write(file, ubuf, count, ppos);
}

static const struct file_operations debug_fops = {
	.write = debug_write,
	.open = debug_open,
};
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
/* Proc FileSystem Routines */
struct proc_dir_entry *mtk_iomu_procfs;
static int proc_open(struct inode *inode, struct file *file)
{
	file->private_data = PDE_DATA(inode);
	return 0;
}

static ssize_t proc_write(struct file *file, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	return process_write(file, ubuf, count, ppos);
}

static const struct file_operations proc_fops = {
	.write = proc_write,
	.open = proc_open,
};
#endif
#endif

void mtk_iommu_debug_init(void)
{
	int total_size = IOMMU_MAX_EVENT_COUNT * sizeof(struct iommu_event_t);

#if IS_ENABLED(CONFIG_DEBUG_FS)
	mtk_iomu_dbgfs = debugfs_create_file("mtk_iommu", S_IFREG | 0444, NULL,
					     (void *)0,
					     &debug_fops);
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
	mtk_iomu_procfs = proc_create_data("mtk_iommu",
					   S_IFREG | 0444,
					   NULL,
					   &proc_fops,
					   (void *)0);
#endif

	strncpy(event_mgr[IOMMU_ALLOC].name, "alloc", 10);
	strncpy(event_mgr[IOMMU_DEALLOC].name, "dealloc", 10);
	strncpy(event_mgr[IOMMU_MAP].name, "map", 10);
	strncpy(event_mgr[IOMMU_UNMAP].name, "unmap", 10);
	event_mgr[IOMMU_ALLOC].dump_trace = 1;
	event_mgr[IOMMU_DEALLOC].dump_trace = 1;

	iommu_globals.record = vmalloc(total_size);
	if (!iommu_globals.record) {
		iommu_globals.enable = 0;
		return;
	}

	memset(iommu_globals.record, 0, total_size);
	iommu_globals.enable = 1;
	iommu_globals.dump_enable = 1;
	iommu_globals.write_pointer = 0;

	spin_lock_init(&iommu_globals.lock);
}

void mtk_iommu_debug_reset(void)
{
	iommu_globals.enable = 1;
}

void mtk_iommu_log_dump(void *seq_file)
{
	int event_id;
	int i = 0;
	struct seq_file *s = NULL;

	if (iommu_globals.dump_enable == 0)
		return;

	if (!seq_file)
		return;

	s = (struct seq_file *)seq_file;
	seq_puts(s, "---------------------------------------------------\n");
	seq_puts(s, "Time  | Action |iova_start | size  | port |iova_end\n");
	for (i = 0; i < IOMMU_MAX_EVENT_COUNT; i++) {
		unsigned long end_iova = 0;

		if ((iommu_globals.record[i].time_low == 0) &&
		    (iommu_globals.record[i].time_high == 0))
			break;
		event_id = iommu_globals.record[i].event_id;
		if (event_id < 0 || event_id >= IOMMU_EVENT_MAX)
			continue;

		if (event_id <= IOMMU_UNMAP)
			end_iova = iommu_globals.record[i].data1 +
				iommu_globals.record[i].data2 - 1;

		seq_printf(s, "%d.%-7d |%10s |0x%-8lx |%9lu |0x%-8lx |0x%-8lx\n",
			   iommu_globals.record[i].time_high,
			   iommu_globals.record[i].time_low,
			   event_mgr[event_id].name,
			   iommu_globals.record[i].data1,
			   iommu_globals.record[i].data2,
			   iommu_globals.record[i].data3,
			   end_iova);
	}
}

static void mtk_iommu_system_time(unsigned int *low, unsigned int *high)
{
	unsigned long long temp;

	temp = sched_clock();
	do_div(temp, 1000);
	*low = do_div(temp, 1000000);
	*high = (unsigned int)temp;
}

void mtk_iommu_trace_rec_write(int event,
			       unsigned long data1,
			       unsigned long data2,
			       unsigned long data3)
{
	unsigned int index;
	struct iommu_event_t *p_event = NULL;
	unsigned long flags;

	if (iommu_globals.enable == 0)
		return;
	if ((event >= IOMMU_EVENT_MAX) ||
	    (event < 0))
		return;

	if (event_mgr[event].dump_log)
		pr_info("[MTK_IOMMU] _trace %10s |0x%-8lx |%9lu |0x%-8lx |0x%-8lx\n",
			event_mgr[event].name,
			data1, data2, data3, data1 + data3);

	if (event_mgr[event].dump_trace == 0)
		return;

	index = (atomic_inc_return((atomic_t *)
			&(iommu_globals.write_pointer)) - 1)
	    % IOMMU_MAX_EVENT_COUNT;

	spin_lock_irqsave(&iommu_globals.lock, flags);

	p_event = (struct iommu_event_t *)
		&(iommu_globals.record[index]);
	mtk_iommu_system_time(&(p_event->time_low), &(p_event->time_high));
	p_event->event_id = event;
	p_event->data1 = data1;
	p_event->data2 = data2;
	p_event->data3 = data3;

	spin_unlock_irqrestore(&iommu_globals.lock, flags);
}

void mtk_iommu_trace_map(unsigned long orig_iova,
			 phys_addr_t orig_pa,
			 size_t size)
{
	mtk_iommu_trace_rec_write(IOMMU_MAP, orig_iova, size, orig_pa);
}

void mtk_iommu_trace_unmap(unsigned long orig_iova,
			   size_t size,
			   size_t unmapped)
{
	mtk_iommu_trace_rec_write(IOMMU_UNMAP, orig_iova, size, unmapped);
}

int mtk_iommu_trace_register(int event, const char *name)
{
	int n = 0;

	if ((event >= IOMMU_EVENT_MAX) ||
	    (event < 0) ||
	    (name == NULL)) {
		pr_info("[MTK_IOMMU] parameter error, event-%d, name %p, EVENT_MAX: %d\n",
			event, name, IOMMU_EVENT_MAX);
		return -1;
	}
	n = snprintf(event_mgr[event].name, 10, "%s", name);
	if (n <= 0)
		pr_info("[MTK_IOMMU] failed to record event name\n");

	return n;
}

void mtk_iommu_trace_log(int event,
			 unsigned long data1,
			 unsigned long data2,
			 unsigned long data3)
{
	if (event >= IOMMU_EVENT_MAX ||
	    event < 0)
		return;

	if (strlen(event_mgr[event].name) == 0)
		return;

	mtk_iommu_trace_rec_write(event, data1, data2, data3);
}

int m4u_user2kernel_port(int userport)
{
#ifdef MTK_IOMMU_PORT_TRANSFER_DISABLE
	return userport;
#else
	unsigned int larb_id;
	unsigned int port;

	if (userport < 0 ||
	    userport >= ARRAY_SIZE(iommu_port) - 1) {
		pr_notice("%s, %d, invalid port id:%d\n",
			  __func__, __LINE__, userport);
		return -1;
	}

	larb_id = iommu_port[userport].larb_id;
	port = iommu_port[userport].larb_port;
	pr_debug("transfer larb_id=%d, port=%d(%d)\n",
		larb_id, port, userport);
	return MTK_M4U_ID(larb_id, port);
#endif
}

unsigned int mtk_get_iommu_index(unsigned int larb)
{
	int i;

	for (i = 0; i < M4U_PORT_NR; i++) {
		if (iommu_port[i].larb_id == larb)
			return iommu_port[i].m4u_id;
	}
	pr_notice("[MTK_IOMMU] do not find index for larb %d\n", larb);

	return (unsigned int)-1;
}

unsigned int mtk_iommu_get_larb_port_count(unsigned int larb)
{
	if (larb >= MTK_IOMMU_LARB_NR)
		return 0;

	return mtk_iommu_larb_port_count[larb];
}
