// SPDX-License-Identifier: GPL-2.0
/*
 * mtk dmabufheap Debug Tool
 *
 * Copyright (C) 2021 MediaTek Inc.
 *
 */

#define pr_fmt(fmt) "dma_heap: mtk_debug "fmt

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device/driver.h>
#include <linux/mod_devicetable.h>
#include <linux/sizes.h>
#include <uapi/linux/dma-heap.h>
#include <linux/sched/clock.h>
#include <linux/of_device.h>
#include <linux/fdtable.h>
#include <linux/oom.h>
#include <linux/notifier.h>

#include "deferred-free-helper.h"
#include "mtk_heap_priv.h"
#include "mtk_heap.h"

#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#define _HEAP_FD_FLAGS_  (O_CLOEXEC|O_RDWR)

int vma_dump_flag;

#if IS_ENABLED(CONFIG_PROC_FS)
enum DMA_HEAP_T_CMD {
	DMABUF_T_OOM_TEST,
	DMABUF_T_END,
};

const char *DMA_HEAP_T_CMD_STR[] = {
	"DMABUF_T_OOM_TEST",
	"DMABUF_T_END",
};

struct proc_dir_entry *dma_heap_proc_root;
struct proc_dir_entry *dma_heaps_dir;
struct proc_dir_entry *dma_heaps_all_entry;
#endif

int oom_nb_status;

struct heap_status_s debug_heap_list[] = {
	{"mtk_mm", 0},
	{"system", 0},
	{"mtk_mm-uncached", 0},
	{"system-uncached", 0},
	{"mtk_svp_region-uncached", 0},
	{"mtk_svp_page-uncached", 0},
	{"mtk_prot_region-uncached", 0},
	{"mtk_prot_page-uncached", 0},
	{"mtk_2d_fr-uncached", 0},
	{"mtk_wfd-uncached", 0},
	{"mtk_sapu_data_shm-uncached", 0},
	{"mtk_sapu_engine_shm-uncached", 0}
};

#define _DEBUG_HEAP_CNT_  (ARRAY_SIZE(debug_heap_list))

struct fd_const {
	struct task_struct *p;
	struct seq_file *s;
	struct dma_heap *heap;
};

struct dump_fd_data {
	/* can be changed part */
	unsigned long size;
	/* can't changed part */
	struct fd_const constd;
};

/* pointr to unsigned long */
static inline struct dump_fd_data *
to_dump_fd_data(const struct fd_const *d)
{
	return container_of(d, struct dump_fd_data, constd);
}

static struct dma_buf *file_to_dmabuf(struct file *file)
{

	if (file && is_dma_buf_file(file))
		return (struct dma_buf *)file->private_data;

	return ERR_PTR(-EINVAL);
}

/** dump all dmabuf userspace va info
 *  Reference code: drivers/misc/mediatek/monitor_hang/hang_detect.c
 */
static void dma_heap_dump_vmas(struct task_struct *t, void *priv)
{
	struct seq_file *s = priv;
	struct vm_area_struct *vma;
	int mapcount = 0;
	int flags;
	struct file *file;
	unsigned long long pgoff = 0;
	char tpath[512];
	char *path_p = NULL;
	struct path base_path;
	struct dma_buf *dmabuf;

	if (!t->mm) {
		dmabuf_dump(s, "\tpid:%d(%s)t->mm is NULL ---->\n",
			    t->pid, t->comm);
		return;
	}

	dmabuf_dump(s, "\tpid:%d(%s) dmabuf vma---->\n",
		    t->pid, t->comm);

	vma = t->mm->mmap;
	while (vma && (mapcount < t->mm->map_count)) {
		if (!vma->vm_file || !is_dma_buf_file(vma->vm_file))
			goto next_vma;

		file = vma->vm_file;
		dmabuf = file->private_data;
		base_path = file->f_path;
		path_p = d_path(&base_path, tpath, 512);
		flags = vma->vm_flags;
		pgoff = ((loff_t)vma->vm_pgoff) << PAGE_SHIFT;

		dmabuf_dump(s,
			    "\t\tinode:%d sz:0x%lx va:%08lx-%08lx map_sz:0x%lx prot:%c%c%c%c off:%08llx node:%s\n",
			    file_inode(file)->i_ino,
			    dmabuf->size,
			    vma->vm_start, vma->vm_end,
			    vma->vm_end - vma->vm_start,
			    flags & VM_READ ? 'r' : '-',
			    flags & VM_WRITE ? 'w' : '-',
			    flags & VM_EXEC ? 'x' : '-',
			    flags & VM_MAYSHARE ? 's' : 'p',
			    pgoff, path_p);
next_vma:
		vma = vma->vm_next;
		mapcount++;
	}
	dmabuf_dump(s, "\n");
}

/* return first dmabuf fd */
static int has_dmabuf_fd(const void *data, struct file *file,
			 unsigned int fd)
{
	struct dma_buf *dmabuf;
	const struct dump_fd_data *d = data;
	struct seq_file *s = d->constd.s;
	struct task_struct *p = d->constd.p;
	struct dma_heap *heap = d->constd.heap;

	dmabuf = file_to_dmabuf(file);
	if (IS_ERR(dmabuf))
		return 0;

	/* heap is valid but buffer is not from this heap */
	if (heap && !is_dmabuf_from_heap(dmabuf, heap))
		return 0;

	dmabuf_dump(s, "pid:%d(%s) -------->\n",
		    p->pid, p->comm);

	dmabuf_dump(s, "\t\t\t%-8s\t%-14s\t%-8s\t%-8s\t%-8s\n",
		    "fd", "size", "inode",
		    "count", "exp_name");

	return fd;
}

static int _do_dump_dmabuf_fd(const void *data, struct file *file,
			      unsigned int fd)
{
	struct dma_buf *dmabuf;
	const struct fd_const *d = data;
	struct seq_file *s = d->s;
	struct task_struct *p = d->p;
	struct dma_heap *heap = d->heap;
	struct dump_fd_data *fd_info = to_dump_fd_data(d);

	dmabuf = file_to_dmabuf(file);
	if (IS_ERR_OR_NULL(dmabuf))
		return 0;

	/* heap is valid but buffer is not from this heap */
	if (heap && !is_dmabuf_from_heap(dmabuf, heap))
		return 0;

	fd_info->size += dmabuf->size;

	dmabuf_dump(s, "\tpid:%-8d\t%-8d\t%-14zu\t%-8d\t%-8ld\t%-8s\n",
		    p->pid, fd, dmabuf->size,
		    file_inode(dmabuf->file)->i_ino,
		    file_count(dmabuf->file),
		    dmabuf->exp_name?:"NULL");

	return 0;
}

static void dump_dmabuf_fds(struct seq_file *s, struct dma_heap *heap)
{
	struct task_struct *p;
	struct dump_fd_data fddata;
	int res;

	if (heap)
		dmabuf_dump(s, "\nShow all dmabuf fds of heap:%s\n",
			    dma_heap_get_name(heap));
	else
		dmabuf_dump(s, "\nShow all dmabuf fds\n");

	read_lock(&tasklist_lock);
	fddata.constd.s = s;
	fddata.constd.heap = heap;
	for_each_process(p) {
		if (fatal_signal_pending(p))
			continue;
		task_lock(p);
		fddata.constd.p = p;
		fddata.size = 0;
		res = iterate_fd(p->files, 0, has_dmabuf_fd, &fddata);
		if (!res) {
			task_unlock(p);
			continue;
		}

		/* start from first dmabuf fd */
		res = iterate_fd(p->files, res, _do_dump_dmabuf_fd, &fddata.constd);
		dmabuf_dump(s, "\tpid:%d(%s) Total size:%d KB\n\n",
			    p->pid, p->comm,
			    fddata.size * 4 / PAGE_SIZE);

		/* dump process all dmabuf maps */
		if (vma_dump_flag)
			dma_heap_dump_vmas(p, s);

		task_unlock(p);
	}
	read_unlock(&tasklist_lock);
}

static void dma_heap_default_show(struct dma_heap *heap,
				  void *seq_file,
				  int flag)
{
	struct seq_file *s = seq_file;
	struct mtk_heap_dump_t dump_param;
	dump_param.heap = heap;
	dump_param.file = seq_file;

	__HEAP_DUMP_START(s, heap);
	__HEAP_TOTAL_BUFFER_SZ_DUMP(s, heap);
	__HEAP_PAGE_POOL_DUMP(s, heap);

	if (flag & HEAP_DUMP_SKIP_ATTACH)
		goto attach_done;

	__HEAP_ATTACH_DUMP_STAT(s, heap);
	get_each_dmabuf(dma_heap_default_attach_dump_cb, &dump_param);
	__HEAP_ATTACH_DUMP_END(s, heap);
attach_done:

	if (flag & HEAP_DUMP_SKIP_FD)
		goto fd_dump_done;
	dump_dmabuf_fds(s, heap);
fd_dump_done:

	__HEAP_DUMP_END(s, heap);

}

/* Goal: for different heap, show different info */
static void show_help_info(struct dma_heap *heap, struct seq_file *s)
{
	dmabuf_dump(s, "%s: help info\n",
		    heap ? dma_heap_get_name(heap) : "all_heaps");

	dmabuf_dump(s, "- Set debug name for dmabuf: %s\n",
		    "https://wiki.mediatek.inc/display/WSDOSS3ME18/How+to+set+dmabuf+debug+name");
	dmabuf_dump(s, "debug:\n");
	dmabuf_dump(s, "- dump_vma:%s\n",
		    vma_dump_flag ? "on" : "off");
	dmabuf_dump(s, "\n");
}

static void mtk_dmabuf_dump_heap(struct dma_heap *heap,
				 struct seq_file *s,
				 int flag)
{
	int i = 0;
	long heap_sz = 0;
	long total_sz = 0;
	int local_flag = 0;

	if (!heap) {
		struct mtk_heap_dump_t dump_param;

		dump_param.heap = heap;
		dump_param.file = s;

		show_help_info(heap, s);
		dmabuf_dump(s, "freelist: %d KB\n", get_freelist_nr_pages() * 4);
		//dump all heaps
		for (; i < _DEBUG_HEAP_CNT_; i++) {
			heap = dma_heap_find(debug_heap_list[i].heap_name);
			local_flag = flag | HEAP_DUMP_SKIP_FD | HEAP_DUMP_SKIP_ATTACH;
			if (heap) {
				mtk_dmabuf_dump_heap(heap, s, local_flag);
				dma_heap_put(heap);
				heap_sz = get_dma_heap_buffer_total(heap);
				total_sz += heap_sz;
			}
		}
		/* dump all dmabuf attachment */
		__HEAP_ATTACH_DUMP_STAT(s, heap);
		get_each_dmabuf(dma_heap_default_attach_dump_cb, &dump_param);
		__HEAP_ATTACH_DUMP_END(s, heap);

		/* dump all fds */
		dump_dmabuf_fds(s, NULL);
		dmabuf_dump(s, "dmabuf total:%d KB\n", (total_sz*4)/PAGE_SIZE);
		return;
	}

	dma_heap_default_show(heap, s, flag);
}

/* dump all heaps */
static inline void mtk_dmabuf_dump_all(struct seq_file *s, int flag)
{
	return mtk_dmabuf_dump_heap(NULL, s, flag);
}

#if IS_ENABLED(CONFIG_PROC_FS)
static ssize_t dma_heap_all_proc_write(struct file *file, const char *buf,
				       size_t count, loff_t *data)
{
#define CMDLINE_LEN   (30)
	char cmdline[CMDLINE_LEN];

	if (count >= CMDLINE_LEN)
		return count;

	if (copy_from_user(cmdline, buf, count))
		return -EINVAL;

	cmdline[count] = 0;

	pr_info("%s #%d: set info:%s", __func__, __LINE__, cmdline);

	if (!strncmp(cmdline, "vma_dump:on", strlen("vma_dump:on"))) {
		vma_dump_flag = 1;
		pr_info("vma_dump status now: on!\n");
	} else if (!strncmp(cmdline, "vma_dump:off", strlen("vma_dump:off"))) {
		vma_dump_flag = 0;
		pr_info("vma_dump status now: off!\n");
	}

	return count;
}

static ssize_t dma_heap_proc_write(struct file *file, const char *buf,
				   size_t count, loff_t *data)
{
#define CMDLINE_LEN   (30)

	struct dma_heap *heap = PDE_DATA(file->f_inode);
	char cmdline[CMDLINE_LEN];
	enum DMA_HEAP_T_CMD cmd = DMABUF_T_END;
	int ret = 0;

	if (count >= CMDLINE_LEN)
		return count;

	if (copy_from_user(cmdline, buf, count))
		return -EINVAL;

	cmdline[count] = 0;

	/* input str from cmd.exe will have \n, no need \n here */
	pr_info("%s #%d: heap_name:%s, set info:%s",
		__func__, __LINE__, dma_heap_get_name(heap),
		cmdline);

	if (!strncmp(cmdline, "test:", strlen("test:"))) {
		/* dma_heap_test(cmdline); */
		//return count;
	}

	ret = sscanf(cmdline, "test:%d", &cmd);
	if (ret < 0) {
		pr_info("cmd error:%s\n", cmdline);
		return -EINVAL;
	}

	if ((unsigned int)cmd >= DMABUF_T_END) {
		pr_info("cmd id error:%s, %d\n", cmdline, cmd);
		return -EINVAL;
	}

	pr_info("%s: test case: %s start======\n",
		__func__, DMA_HEAP_T_CMD_STR[cmd]);

	pr_info("================buffer check before=================\n");
	mtk_dmabuf_dump_heap(heap, NULL, 0);

	switch (cmd) {
	case DMABUF_T_OOM_TEST:
	{
		int i = 0;
		int fd = -1;

		pr_info("%s #%d\n", __func__, __LINE__);
		for (;;) {
			fd = dma_heap_bufferfd_alloc(heap,
					SZ_16M,
					_HEAP_FD_FLAGS_,
					DMA_HEAP_VALID_HEAP_FLAGS);
			if (fd > 0) {
				i++;
				if (1 % 10 == 0)
					pr_info("%s alloc pass, cnt:%d\n", __func__, i);
			} else {
				pr_info("%s alloc failed\n", __func__);
				break;
			}
		}
	}
	break;
	default:
		pr_info("error cmd:%d\n", cmd);
		break;
	}
	pr_info("================buffer check after==================\n");
	mtk_dmabuf_dump_heap(heap, NULL, 0);

	if (cmd < DMABUF_T_END)
		pr_info("%s: test case: end======\n",
			__func__, DMA_HEAP_T_CMD_STR[cmd]);

#undef CMDLINE_LEN
	return count;
}


static int dma_heap_proc_show(struct seq_file *s, void *v)
{
	struct dma_heap *heap;

	if (!s)
		return -EINVAL;

	heap = (struct dma_heap *)s->private;
	dma_heap_default_show(heap, s, 0);
	return 0;
}

static int all_heaps_proc_show(struct seq_file *s, void *v)
{
	if (!s)
		return -EINVAL;

	mtk_dmabuf_dump_all(s, 0);
	return 0;
}

static int dma_heap_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, dma_heap_proc_show, PDE_DATA(inode));
}

static int all_heaps_dump_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, all_heaps_proc_show, PDE_DATA(inode));
}


static const struct proc_ops dma_heap_proc_fops = {
	.proc_open = dma_heap_proc_open,
	.proc_read = seq_read,
	.proc_write = dma_heap_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops all_heaps_dump_proc_fops = {
	.proc_open = all_heaps_dump_proc_open,
	.proc_read = seq_read,
	.proc_write = dma_heap_all_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int dma_buf_init_dma_heaps_procfs(void)
{
	int i = 0;
	const char *heap_name;
	struct dma_heap *heap;
	struct proc_dir_entry *proc_file;

	for (i = 0; i < _DEBUG_HEAP_CNT_; i++) {
		heap_name = debug_heap_list[i].heap_name;

		debug_heap_list[i].heap_exist = 1;
		heap = dma_heap_find(heap_name);
		if (!heap) {
			debug_heap_list[i].heap_exist = 0;
			continue;
		}

		proc_file = proc_create_data(heap_name,
					     S_IFREG | 0664,
					     dma_heaps_dir,
					     &dma_heap_proc_fops,
					     heap);
		if (!proc_file) {
			pr_info("Failed to create %s\n",
				heap_name);
			return -2;
		}
		pr_info("create debug file for %s\n",
			heap_name);
	}

	return 0;

}

static int dma_buf_init_procfs(void)
{
	struct proc_dir_entry *proc_file;
	int ret = 0;

	dma_heap_proc_root = proc_mkdir("dma_heap", NULL);
	if (!dma_heap_proc_root) {
		pr_info("%s failed to create procfs root dir.\n", __func__);
		return -1;
	}

	proc_file = proc_create_data("all_heaps",
				     S_IFREG | 0664,
				     dma_heap_proc_root,
				     &all_heaps_dump_proc_fops,
				     NULL);
	if (!proc_file) {
		pr_info("Failed to create debug file for all_heaps\n");
		return -2;
	}
	pr_info("create debug file for all_heaps\n");


	dma_heaps_dir = proc_mkdir("heaps", dma_heap_proc_root);
	if (!dma_heaps_dir) {
		pr_info("%s failed to create procfs root dir.\n", __func__);
		return -1;
	}

	ret = dma_buf_init_dma_heaps_procfs();

	return ret;
}

static void dma_buf_uninit_procfs(void)
{
	proc_remove(dma_heap_proc_root);
}
#endif

static int dma_heap_oom_notify(struct notifier_block *nb,
			       unsigned long nb_val, void *nb_freed)
{
	/* TODO: reinit debug file, make sure can dump all heaps */
	pr_info("%s in\n", __func__);
	mtk_dmabuf_dump_all(NULL, HEAP_DUMP_SKIP_ATTACH);
	return 0;
}

static struct notifier_block dma_heap_oom_nb = {
	.notifier_call = dma_heap_oom_notify,
};

static int __init mtk_dma_heap_debug(void)
{
	int ret = 0;

	pr_info("GM-T %s\n", __func__);

	oom_nb_status = register_oom_notifier(&dma_heap_oom_nb);
	if (oom_nb_status)
		pr_info("register_oom_notifier failed:%d\n", oom_nb_status);

	ret = dma_buf_init_procfs();
	if (ret) {
		pr_info("%s fail\n", __func__);
		return ret;
	}

	return 0;
}

static void __exit mtk_dma_heap_debug_exit(void)
{
	int ret = 0;

	if (oom_nb_status) {
		ret = unregister_oom_notifier(&dma_heap_oom_nb);
		if (ret)
			pr_info("unregister_oom_notifier failed:%d\n", ret);
	}

	dma_buf_uninit_procfs();

}
module_init(mtk_dma_heap_debug);
module_exit(mtk_dma_heap_debug_exit);
MODULE_LICENSE("GPL v2");
