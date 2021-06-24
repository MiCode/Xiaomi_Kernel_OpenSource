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

#include "mtk_heap_debug.h"
#include "mtk_heap.h"

#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#define _HEAP_FD_FLAGS_  (O_CLOEXEC|O_RDWR)

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

struct dump_fd_data {
	struct task_struct *p;
	struct seq_file *s;
};

static struct dma_buf *file_to_dmabuf(struct file *file)
{

	if (file && is_dma_buf_file(file))
		return (struct dma_buf *)file->private_data;

	return ERR_PTR(-EINVAL);
}

/* return first dmabuf fd */
static int has_dmabuf_fd(const void *data, struct file *file,
			 unsigned int fd)
{
	struct dma_buf *dmabuf;
	const struct dump_fd_data *d = data;
	struct seq_file *s = d->s;
	struct task_struct *p = d->p;

	dmabuf = file_to_dmabuf(file);
	if (IS_ERR(dmabuf))
		return 0;

	dmabuf_dump(s, "pid:%d(%s) -------->\n",
		    p->pid, p->comm);

	dmabuf_dump(s, "\t\t\t%-8s\t%-8s\t%-8s"
#ifdef CONFIG_DMABUF_SYSFS_STATS
		    "\t%-8s"
#endif
		    "\t%-8s\t%-8s\t%-8s exp_name\n",
		    "fd", "size", "inode",
#ifdef CONFIG_DMABUF_SYSFS_STATS
		    "mmap",
#endif
		    "flags", "mode", "count");

	return fd;
}


static int _do_dump_dmabuf_fd(const void *data, struct file *file,
			      unsigned int fd)
{
	struct dma_buf *dmabuf;
	const struct dump_fd_data *d = data;
	struct seq_file *s = d->s;
	struct task_struct *p = d->p;

	dmabuf = file_to_dmabuf(file);
	if (IS_ERR(dmabuf))
		return 0;

	dmabuf_dump(s, "\tpid:%-8d\t%-8d\t%-8zu\t%-8d"
#ifdef CONFIG_DMABUF_SYSFS_STATS
		   "\t%-8d"
#endif
		   "\t%-8x\t%-8x\t%-8ld\t%-s\n",
		    p->pid, fd, dmabuf->size,
		    file_inode(dmabuf->file)->i_ino,
#ifdef CONFIG_DMABUF_SYSFS_STATS
		    dmabuf->mmap_count,
#endif
		    dmabuf->file->f_flags,
		    dmabuf->file->f_mode,
		    file_count(dmabuf->file),
		    dmabuf->exp_name);

	return 0;
}

static void dump_dmabuf_fds(struct seq_file *s)
{
	struct task_struct *p;
	struct dump_fd_data data;
	int res;

	dmabuf_dump(s, "\nShow all dmabuf fds\n");
	read_lock(&tasklist_lock);
	data.s = s;
	for_each_process(p) {
		task_lock(p);
		data.p = p;
		res = iterate_fd(p->files, 0, has_dmabuf_fd, &data);
		if (!res) {
			task_unlock(p);
			continue;
		}

		/* start from first dmabuf fd */
		res = iterate_fd(p->files, res, _do_dump_dmabuf_fd, &data);
		dmabuf_dump(s, "\n");

		task_unlock(p);
	}
	read_unlock(&tasklist_lock);
}

#ifndef SKIP_DMBUF_BUFFER_DUMP
static char *dma_heap_default_fmt(struct dma_heap *heap)
{
	char *fmt_str = NULL;
	int len = 0;

	fmt_str = kzalloc(sizeof(char) * (DUMP_INFO_LEN_MAX + 1), GFP_KERNEL);
	if (!fmt_str)
		return NULL;

	len += scnprintf(fmt_str + len,
			 DUMP_INFO_LEN_MAX - len,
			 "heap_name \tdmabuf \tsize(hex) \texp_name \tdmabuf_name \tf_flag \tf_mode \tf_count \tino \tpid(name) \ttid(name)");

#ifdef CONFIG_DMABUF_SYSFS_STATS
	len += scnprintf(fmt_str + len,
			 DUMP_INFO_LEN_MAX - len,
			 " \tmmap_cnt");
#endif

	return fmt_str;
}
#endif

static char *dma_heap_default_str(const struct dma_buf *dmabuf,
				  const struct dma_heap *dump_heap)
{
	struct sys_heap_buf_debug_use *buf = dmabuf->priv;
	char *info_str;
	int len = 0;

	/* dmabuf check */
	if (!buf || !buf->heap || buf->heap != dump_heap)
		return NULL;

	info_str = kzalloc(sizeof(char) * (DUMP_INFO_LEN_MAX + 1), GFP_KERNEL);
	if (!info_str)
		return NULL;

	len += scnprintf(info_str + len,
			 DUMP_INFO_LEN_MAX - len,
			 "%s \t%p \t0x%lx \t%s \t%s \t%x \t%x \t%ld \t%lu",
			 dma_heap_get_name(buf->heap),
			 dmabuf,
			 dmabuf->size, dmabuf->exp_name,
			 dmabuf->name ?: "NULL",
			 dmabuf->file->f_flags,
			 dmabuf->file->f_mode,
			 file_count(dmabuf->file),
			 file_inode(dmabuf->file)->i_ino);

	return info_str;
}

int dma_heap_default_buf_info_cb(const struct dma_buf *dmabuf,
				 void *priv)
{
	struct mtk_heap_dump_t *dump_param = priv;
	struct seq_file *s = dump_param->file;
	struct dma_heap *dump_heap = dump_param->heap;
	struct sys_heap_buf_debug_use *buf = dmabuf->priv;
	char *buf_dump_str = NULL;

	/* dmabuf check */
	if (!buf || !buf->heap || buf->heap != dump_heap)
		return 0;

	buf_dump_str = dma_heap_default_str(dmabuf, dump_heap);
	dmabuf_dump(s, "%s\n", buf_dump_str);
	kfree(buf_dump_str);

	return 0;
}

static void dma_heap_default_show(struct dma_heap *heap,
				  void *seq_file)
{
	struct seq_file *s = seq_file;
	struct mtk_heap_dump_t dump_param;
#ifndef SKIP_DMBUF_BUFFER_DUMP
	const char *dump_fmt = NULL;
#endif
	dump_param.heap = heap;
	dump_param.file = seq_file;

	__HEAP_DUMP_START(s, heap);
	__HEAP_TOTAL_BUFFER_SZ_DUMP(s, heap);
	__HEAP_PAGE_POOL_DUMP(s, heap);

#ifndef SKIP_DMBUF_BUFFER_DUMP
	__HEAP_BUF_DUMP_START(s, heap);

	dump_fmt = dma_heap_default_fmt(heap);
	dmabuf_dump(s, "\t%s\n", dump_fmt);
	kfree(dump_fmt);
	get_each_dmabuf(dma_heap_default_buf_info_cb, &dump_param);
#endif

	__HEAP_ATTACH_DUMP_STAT(s, heap);
	get_each_dmabuf(dma_heap_default_attach_dump_cb, &dump_param);
	__HEAP_ATTACH_DUMP_END(s, heap);

	__HEAP_DUMP_END(s, heap);

}

static void mtk_dmabuf_dump_heap(struct dma_heap *heap, struct seq_file *s)
{
	struct mtk_heap_priv_info *heap_priv = NULL;
	int i = 0;
	long heap_sz = 0;
	long total_sz = 0;

	if (!heap) {
		//dump all heaps
		for (; i < _DEBUG_HEAP_CNT_; i++) {
			heap = dma_heap_find(debug_heap_list[i].heap_name);
			if (heap) {
				mtk_dmabuf_dump_heap(heap, s);
				dma_heap_put(heap);
				heap_sz = get_dma_heap_buffer_total(heap);
				total_sz += heap_sz;
			}
		}
		/* dump all fds */
		dump_dmabuf_fds(s);
		dmabuf_dump(s, "dmabuf total:%d\n", (total_sz*4)/PAGE_SIZE);
		return;
	}

	heap_priv = mtk_heap_priv_get(heap);
	if (!heap_priv || !heap_priv->show) {
		dma_heap_default_show(heap, s);
		return;
	}

	heap_priv->show(heap, s);
}

/* dump all heaps */
static inline void mtk_dmabuf_dump_all(struct seq_file *s)
{
	return mtk_dmabuf_dump_heap(NULL, s);
}

#if IS_ENABLED(CONFIG_PROC_FS)
static ssize_t dma_heap_all_proc_write(struct file *file, const char *buf,
				       size_t count, loff_t *data)
{

	return 0;
}

#define CMDLINE_LEN   (30)
static ssize_t dma_heap_proc_write(struct file *file, const char *buf,
				   size_t count, loff_t *data)
{
	struct dma_heap *heap = PDE_DATA(file->f_inode);
	char cmdline[CMDLINE_LEN];
	enum DMA_HEAP_T_CMD cmd = DMABUF_T_END;
	int ret = 0;

	if (count >= CMDLINE_LEN)
		return -EINVAL;

	if (copy_from_user(cmdline, buf, count))
		return -EINVAL;

	cmdline[count] = 0;

	/* input str from cmd.exe will have \n, no need \n here */
	pr_info("%s #%d: heap_name:%s, set info:%s",
		__func__, __LINE__, dma_heap_get_name(heap),
		cmdline);

	if (!strncmp(cmdline, "test:", strlen("test:"))) {
		/* dma_heap_test(cmdline); */
		return ret;
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
	mtk_dmabuf_dump_heap(heap, NULL);

	switch (cmd) {
	case DMABUF_T_OOM_TEST:
	{
		int i = 0;
		int fd = -1;

		for (;;) {
			fd = dma_heap_bufferfd_alloc(heap,
					SZ_16M,
					_HEAP_FD_FLAGS_,
					DMA_HEAP_VALID_HEAP_FLAGS);
			if (fd > 0) {
				i++;
				if (1 % 100 == 0)
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
	mtk_dmabuf_dump_heap(heap, NULL);

	if (cmd < DMABUF_T_END)
		pr_info("%s: test case: end======\n",
			__func__, DMA_HEAP_T_CMD_STR[cmd]);

	return count;
}
#undef CMDLINE_LEN


static int dma_heap_proc_show(struct seq_file *s, void *v)
{
	struct dma_heap *heap;

	if (!s)
		return -EINVAL;

	heap = (struct dma_heap *)s->private;
	mtk_dmabuf_dump_heap(heap, s);
	return 0;
}

static int all_heaps_proc_show(struct seq_file *s, void *v)
{
	if (!s)
		return -EINVAL;

	mtk_dmabuf_dump_all(s);
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
			pr_info("%s heap is not supported on this phone\n",
				heap_name);
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
	mtk_dmabuf_dump_all(NULL);
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
