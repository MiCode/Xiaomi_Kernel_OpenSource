// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/sched/task.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/sched/signal.h>
#include <linux/fdtable.h>
#include "mtk_ion.h"
#ifdef CONFIG_MTK_IOMMU_MISC_DBG
#include "m4u_debug.h"
#endif

struct dump_fd_data {
	struct task_struct *p;
	struct seq_file *s;
};

static int __do_dump_share_fd(const void *data, struct file *file,
			      unsigned int fd)
{
	const struct dump_fd_data *d = data;
	struct seq_file *s = d->s;
	struct task_struct *p = d->p;
	struct ion_buffer *buffer;
	struct dma_buf *dmabuf;

	buffer = ion_drv_file_to_buffer(file);
	if (IS_ERR_OR_NULL(buffer))
		return 0;

	dmabuf = (struct dma_buf *)file->private_data;
	seq_printf(s, "0x%p 0x%p %8zu %5d %5d %16s %4d\n",
		   buffer, dmabuf, buffer->size, p->pid,
		   p->tgid, p->comm, fd);

	return 0;
}

static int ion_dump_all_buf_fds(struct seq_file *s)
{
	struct task_struct *p;
	int res;
	struct dump_fd_data data;

	/* function is not available, just return */
	if (ion_drv_file_to_buffer(NULL) == ERR_PTR(-EPERM))
		return 0;

	seq_printf(s, "%18s %18s %8s %5s %5s %16s %4s\n",
		   "buffer", "dmabuf", "size", "pid",
		   "tgid", "process", "fd");
	data.s = s;

	read_lock(&tasklist_lock);
	for_each_process(p) {
		task_lock(p);
		data.p = p;
		res = iterate_fd(p->files, 0, __do_dump_share_fd, &data);
		if (res)
			pr_err("%s failed somehow\n", __func__);
		task_unlock(p);
	}
	read_unlock(&tasklist_lock);
	return 0;
}

int ion_sys_heap_debug_show(struct seq_file *s, void *unused)
{
	struct ion_heap *heap = s->private;
	struct ion_device *dev = heap->dev;
	struct rb_node *n;
	unsigned long long current_ts;
	size_t total_size = 0, system_heap_size = 0, custom_heap_size = 0;

	current_ts = sched_clock();
	do_div(current_ts, 1000000);
	seq_printf(s, "step 1 current time %lld ms\n", current_ts);

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		seq_printf(s, "sys_heap_freelist total_size=%zu\n",
			   ion_heap_freelist_size(heap));
	else
		seq_puts(s, "sys_heap defer free disabled\n");

	seq_puts(s, "----------------------------------------------------\n");

	mutex_lock(&dev->buffer_lock);
	/* maybe miss some buffer if fd is closed */
	ion_dump_all_buf_fds(s);

	current_ts = sched_clock();
	do_div(current_ts, 1000000);
	seq_printf(s, "\nstep 2 current time %lld ms\n", current_ts);

	seq_puts(s, "----------------------------------------------------\n");

	seq_printf(s, "%18.s %8.s %16.s\n",
		   "buffer", "size", "heap_name");

	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer
		*buffer = rb_entry(n, struct ion_buffer, node);

		seq_printf(s, "0x%p %8zu %s\n",
			   buffer, buffer->size, buffer->heap->name);

		if (buffer->heap->type == ION_HEAP_TYPE_SYSTEM)
			system_heap_size += buffer->size;
		else if (buffer->heap->type == ION_HEAP_TYPE_CUSTOM)
			custom_heap_size += buffer->size;

		total_size += buffer->size;
	}
	mutex_unlock(&dev->buffer_lock);

	current_ts = sched_clock();
	do_div(current_ts, 1000000);
	seq_printf(s, "\nstep 3 current time %lld ms\n", current_ts);
	seq_printf(s, "%16.s %16zu\n", "total size", total_size);
	seq_printf(s, "%16.s %16zu\n", "system_heap", system_heap_size);
	seq_printf(s, "%16.s %16zu\n", "custom_heap", custom_heap_size);

	seq_puts(s, "----------------------------------------------------\n");

#ifdef CONFIG_MTK_IOMMU_MISC_DBG
	mtk_iova_dbg_dump(s);
	current_ts = sched_clock();
	do_div(current_ts, 1000000);
	seq_printf(s, "step 4 current time %lld ms\n\n", current_ts);
#endif
	if (heap->debug_show)
		heap->debug_show(heap, s, unused);

	return 0;
}

