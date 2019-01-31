/*
 * Copyright (c) 2015-2016 MediaTek Inc.
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

#include "mach/pseudo_m4u.h"
#include "mtk_iommu_ext.h"
#include "mach/mt_iommu_plat.h"
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/seq_file.h>

#ifdef CONFIG_MTK_AEE_FEATURE
#include <aee.h>
#endif

#include <mmprofile.h>

#define mmu_translation_log_format \
	"\nCRDISPATCH_KEY:M4U_%s\ntranslation fault:port=%s,mva=0x%x,pa=0x%x\n"

#ifdef CONFIG_MTK_AEE_FEATURE
#define mmu_aee_print(string, args...) do {\
	char mmu_name[100];\
	snprintf(mmu_name, 100, "[MTK_IOMMU]"string, ##args); \
	aee_kernel_warning_api(__FILE__, __LINE__, \
		DB_OPT_MMPROFILE_BUFFER | DB_OPT_DUMP_DISPLAY, \
		mmu_name, "[MTK_IOMMU] error"string, ##args); \
	pr_info("[MTK_IOMMU] error:"string, ##args);  \
	} while (0)
#else
#define mmu_aee_print(string, args...) do {\
		char mmu_name[100];\
		snprintf(mmu_name, 100, "[MTK_IOMMU]"string, ##args); \
		pr_info("[MTK_IOMMU] error:"string, ##args);  \
	} while (0)

#endif
#define ERROR_LARB_PORT_ID 0xFF


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
	unsigned int data1;
	unsigned int data2;
	unsigned int data3;
};


struct iommu_global_t {
	unsigned int enable;
	unsigned int dump_enable;
	unsigned int start;
	unsigned int write_pointer;
	unsigned int lock;
	struct iommu_event_t *record;
};

static struct iommu_global_t iommu_globals;

static inline int mtk_iommu_get_tf_larb_port_idx(int tf_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(iommu_port); i++) {
		if (iommu_port[i].tf_id == tf_id)
			return i;
	}
	pr_info("[MTK_IOMMU] do not find index for tf_id %d", tf_id);
	return ERROR_LARB_PORT_ID;
}

static inline int mtk_iommu_larb_port_idx(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(iommu_port); i++) {
		if ((iommu_port[i].larb_id == MTK_M4U_TO_LARB(id)) &&
			(iommu_port[i].larb_port == MTK_M4U_TO_PORT(id)))
			return i;
	}
	pr_info("[MTK_IOMMU] do not find index for id %d", id);
	return ERROR_LARB_PORT_ID;
}

bool report_custom_iommu_fault(
	void __iomem	*base,
	unsigned int	int_state,
	unsigned int	fault_iova,
	unsigned int	fault_pa,
	unsigned int	fault_id) {

	int idx = mtk_iommu_get_tf_larb_port_idx(fault_id);
	int port;

	iommu_globals.enable = 0;

	if (idx >= ARRAY_SIZE(iommu_port)) {
		pr_info("[MTK_IOMMU] fail,iova 0x%x, port %d\n",
			fault_iova, fault_id);
		return false;
	}

	port = MTK_M4U_ID(iommu_port[idx].larb_id, iommu_port[idx].larb_port);
	if (iommu_port[idx].enable_tf && iommu_port[idx].fault_fn)
		iommu_port[idx].fault_fn(port,
				fault_iova,
				iommu_port[idx].fault_data);

	mmu_aee_print(mmu_translation_log_format,
		       iommu_port[idx].name,
		       iommu_port[idx].name, fault_iova,
		       fault_pa);
	return true;
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

	if (idx >= ARRAY_SIZE(iommu_port)) {
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

	if (idx >= ARRAY_SIZE(iommu_port)) {
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

	if (idx >= ARRAY_SIZE(iommu_port)) {
		pr_info("[MTK_IOMMU] %s fail, port=%d\n", __func__, port);
		return -1;
	}

	iommu_port[idx].enable_tf = fgenable;
	return 0;
}

int mtk_smi_larb_get_ext(struct device *larbdev)
{
	struct M4U_PORT_STRUCT sPort;
	int i = 0, ret = 0;

	sPort.ePortID = 0;
	sPort.Virtuality = 1;
	sPort.Security = 0;
	sPort.Distance = 1;
	sPort.Direction = 0;
	pr_info("[MTK_IOMMU] iommu_smi_larb_get larb port!!!\n");
	for (i = 0; i < M4U_PORT_DISP_FAKE0; i++) {
		sPort.ePortID = i;
		ret = m4u_config_port(&sPort);
		if (ret) {
			pr_info("[MTK_IOMMU] config Port(%d) FAIL(ret=%d)\n",
				i, ret);
			return -1;
		}
	}
#ifdef M4U_PORT_DISP_OVL1
	for (i = M4U_PORT_DISP_OVL1; i < M4U_PORT_DISP_FAKE1; i++) {
		sPort.ePortID = i;
		ret = m4u_config_port(&sPort);
		if (ret) {
			pr_info("[MTK_IOMMU] config Port(%d) FAIL(ret=%d)\n",
				i, ret);
			return -1;
		}
	}
#endif
	return ret;

}

void *mtk_iommu_iova_to_va(struct device *dev, dma_addr_t iova, size_t size)
{
#ifdef CONFIG_MTK_M4U
	unsigned long map_va;
	unsigned int map_size;

	m4u_mva_map_kernel((unsigned int)iova,
			   (unsigned int)size,
			   &map_va, &map_size);
	return (void *)map_va;
#else
	return NULL;
#endif
}


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

/* Debug FileSystem Routines */
struct dentry *mtk_iomu_dbgfs;
static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static char debug_buffer[128];
static ssize_t debug_write(struct file *file, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;

	process_dbg_cmd(debug_buffer);

	return ret;
}

static const struct file_operations debug_fops = {
	.write = debug_write,
	.open = debug_open,
};

void mtk_iommu_debug_init(void)
{
	int total_size = IOMMU_MAX_EVENT_COUNT * sizeof(struct iommu_event_t);

	mtk_iomu_dbgfs = debugfs_create_file("mtk_iommu", S_IFREG | 0444, NULL,
					     (void *)0,
					     &debug_fops);
	strncpy(event_mgr[IOMMU_ALLOC].name, "alloc", 10);
	strncpy(event_mgr[IOMMU_DEALLOC].name, "dealloc", 10);
	strncpy(event_mgr[IOMMU_MAP].name, "map", 10);
	strncpy(event_mgr[IOMMU_UNMAP].name, "unmap", 10);
	event_mgr[IOMMU_ALLOC].dump_trace = 1;
	event_mgr[IOMMU_DEALLOC].dump_trace = 1;

	iommu_globals.record = vmalloc(total_size);
	if (!iommu_globals.record) {
		pr_info("[MTK_IOMMU] alloc record buffer fail\n");
		iommu_globals.enable = 0;
		return;
	}

	memset(iommu_globals.record, 0, total_size);
	iommu_globals.enable = 1;
	iommu_globals.dump_enable = 1;
	iommu_globals.write_pointer = 0;
	iommu_globals.lock = 0;
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
		unsigned int end_iova = 0;

		if ((iommu_globals.record[i].time_low == 0) &&
		    (iommu_globals.record[i].time_high == 0))
			break;
		event_id = iommu_globals.record[i].event_id;
		if (event_id <= IOMMU_UNMAP)
			end_iova = iommu_globals.record[i].data1 +
				iommu_globals.record[i].data2 - 1;

		seq_printf(s, "%d.%-7d |%10s |0x%-8x |%9u |0x%-8x |0x%-8x\n",
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
			       unsigned int data1,
			       unsigned int data2,
			       unsigned int data3)
{
	unsigned int index;
	unsigned int lock;
	struct iommu_event_t *p_event = NULL;

	if (iommu_globals.enable == 0)
		return;
	if (event_mgr[event].dump_log)
		pr_info("[MTK_IOMMU] _trace %10s |0x%-8x |%9u |0x%-8x |0x%-8x\n",
			event_mgr[event].name,
			data1, data2, data3, data1 + data3);

	if (event_mgr[event].dump_trace == 0)
		return;

	index = (atomic_inc_return((atomic_t *)
			&(iommu_globals.write_pointer)) - 1)
	    % IOMMU_MAX_EVENT_COUNT;
	lock = atomic_inc_return((atomic_t *)
		&(iommu_globals.lock));
	if (unlikely(lock > 1)) {
		/* Do not reduce lock count since it need
		 * to be marked as invalid.
		 */
		while (1) {
			index =
				(atomic_inc_return((atomic_t *)
				&(iommu_globals.write_pointer)) - 1) %
				IOMMU_MAX_EVENT_COUNT;
			lock =
			    atomic_inc_return((atomic_t *) &
					(iommu_globals.lock));
			/* Do not reduce lock count since it need to be
			 * marked as invalid.
			 */
			if (likely(lock == 1))
				break;
		}
	}
	p_event = (struct iommu_event_t *)
		&(iommu_globals.record[index]);
	mtk_iommu_system_time(&(p_event->time_low), &(p_event->time_high));
	p_event->event_id = event;
	p_event->data1 = (unsigned int)data1;
	p_event->data2 = (unsigned int)data2;
	p_event->data3 = data3;
	lock = atomic_dec_return((atomic_t *) &(iommu_globals.lock));
	if (unlikely(lock > 0)) {
		/* Someone has marked this record as invalid.
		 * Kill this record.
		 */
		p_event->event_id = 0;
		iommu_globals.lock = 0;
	}

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

void mtk_iommu_trace_register(int event, const char *name)
{
	if ((event >= IOMMU_EVENT_MAX) ||
	    (name == NULL)) {
		pr_info("[MTK_IOMMU] parameter error, event-%d, name %p, EVENT_MAX: %d\n",
			event, name, IOMMU_EVENT_MAX);
		return;
	}
	snprintf(event_mgr[event].name, 10, "%s", name);
}

void mtk_iommu_trace_log(int event,
			 unsigned int data1,
			 unsigned int data2,
			 unsigned int data3)
{
	if (event >= IOMMU_EVENT_MAX)
		return;

	if (strlen(event_mgr[event].name) == 0)
		return;

	mtk_iommu_trace_rec_write(event, data1, data2, data3);
}
