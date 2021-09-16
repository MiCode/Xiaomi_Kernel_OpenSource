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

#include <linux/iommu.h>

#include "deferred-free-helper.h"
#include "mtk_heap_priv.h"
#include "mtk_heap.h"

#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#define _HEAP_FD_FLAGS_  (O_CLOEXEC|O_RDWR)
#define DMA_HEAP_CMDLINE_LEN      (30)

/* copy from struct system_heap_buffer */
struct dmaheap_buf_copy {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	int vmap_cnt;
	void *vaddr;
	struct deferred_freelist_item deferred_free;
	bool uncached;
	/* helper function */
	int (*show)(const struct dma_buf *dmabuf, struct seq_file *s);

	/* system heap will not strore sgtable here */
	bool                     mapped[BUF_PRIV_MAX_CNT];
	struct mtk_heap_dev_info dev_info[BUF_PRIV_MAX_CNT];
	struct sg_table          *mapped_table[BUF_PRIV_MAX_CNT];
	struct mutex             map_lock; /* map iova lock */
	pid_t                    pid;
	pid_t                    tid;
	char                     pid_name[TASK_COMM_LEN];
	char                     tid_name[TASK_COMM_LEN];
	unsigned long long       ts; /* us */
};



#if IS_ENABLED(CONFIG_PROC_FS)
/* debug flags */
int vma_dump_flag;
int fd_rb_check;
int dump_all_attach;

struct mtk_heap_dump_s {
	struct seq_file *file;
	struct dma_heap *heap;
	long ret; /* used for storing return value */
};

enum DMA_HEAP_T_CMD {
	DMABUF_T_OOM_TEST,
	DMABUF_T_END,
};

const char *DMA_HEAP_T_CMD_STR[] = {
	"DMABUF_T_OOM_TEST",
	"DMABUF_T_END",
};

struct dma_heap_dbg {
	const char *str;
	int *flag;
};

const struct dma_heap_dbg heap_helper[] = {
	{"vma_dump:", &vma_dump_flag},
	{"fd_rb_debug:", &fd_rb_check},
	{"dump_all_attach:", &dump_all_attach},
};

struct proc_dir_entry *dma_heap_proc_root;
struct proc_dir_entry *dma_heaps_dir;
struct proc_dir_entry *dma_heaps_all_entry;
#endif

int oom_nb_status;

struct heap_status_s {
	const char *heap_name;
	int heap_exist;
};

struct heap_status_s debug_heap_list[] = {
	{"mtk_mm", 0},
	{"system", 0},
	{"mtk_mm-uncached", 0},
	{"system-uncached", 0},
	{"mtk_svp_region", 0},
	{"mtk_svp_region-aligned", 0},
	{"mtk_svp_page-uncached", 0},
	{"mtk_prot_region", 0},
	{"mtk_prot_region-aligned", 0},
	{"mtk_prot_page-uncached", 0},
	{"mtk_2d_fr_region", 0},
	{"mtk_2d_fr_region-aligned", 0},
	{"mtk_wfd_region", 0},
	{"mtk_wfd_region-aligned", 0},
	{"mtk_sapu_data_shm_region", 0},
	{"mtk_sapu_data_shm_region-aligned", 0},
	{"mtk_sapu_engine_shm_region", 0},
	{"mtk_sapu_engine_shm_region-aligned", 0},
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
	struct rb_root fd_root;
	struct rb_root dmabuf_root;
	int err;
	/* can't changed part */
	struct fd_const constd;
};

struct fd_rb_s {
	struct rb_node node;
	unsigned int fd_val;
	unsigned int inode;
};

struct dmabuf_debug_node {
	struct rb_node dmabuf_node;
	struct dma_buf *dmabuf;
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

static inline
unsigned long long get_current_time_ms(void)
{
	unsigned long long cur_ts;

	cur_ts = sched_clock();
	do_div(cur_ts, 1000000);
	return cur_ts;
}

/* common function */
int is_dmabuf_from_heap(const struct dma_buf *dmabuf, struct dma_heap *heap)
{

	struct dmaheap_buf_copy *heap_buf = dmabuf->priv;

	if (!(is_system_heap_dmabuf(dmabuf) ||
	      is_mtk_mm_heap_dmabuf(dmabuf) ||
	      is_mtk_sec_heap_dmabuf(dmabuf)))
		return 0;

	return (heap_buf->heap == heap);
}

int dma_heap_default_attach_dump_cb(const struct dma_buf *dmabuf,
				    void *priv)
{
	struct mtk_heap_dump_s *dump_param = priv;
	struct seq_file *s = dump_param->file;
	struct dma_heap *dump_heap = dump_param->heap;
	struct dmaheap_buf_copy *buf = dmabuf->priv;
	struct dma_buf_attachment *attach_obj;
	int ret;
	dma_addr_t iova = 0x0;
	const char *device_name = NULL;
	int attach_cnt = 0;

	if (dump_heap && !is_dmabuf_from_heap(dmabuf, dump_heap))
		return 0;

	dmabuf_dump(s, "\tinode:%-8d size:%-8ld count:%-2ld cache_sg:%-1d exp:%s\tname:%s\n",
		    file_inode(dmabuf->file)->i_ino,
		    dmabuf->size,
		    file_count(dmabuf->file),
		    dmabuf->ops->cache_sgt_mapping,
		    dmabuf->exp_name?:"NULL",
		    dmabuf->name?:"NULL");

	/* buffer private dump */
	if (buf->show)
		buf->show(dmabuf, s);

	attach_cnt = 0;

	ret = dma_resv_lock(dmabuf->resv, NULL);
	if (ret)
		return 0;

	list_for_each_entry(attach_obj, &dmabuf->attachments, node) {
		iova = (dma_addr_t)0;

		attach_cnt++;
		if (!attach_obj->sgt)
			continue;
		if (!dev_iommu_fwspec_get(attach_obj->dev)) {
			if (!dump_all_attach)
				continue;
		} else {
			iova = sg_dma_address(attach_obj->sgt->sgl);
		}

		device_name = dev_name(attach_obj->dev);
		dmabuf_dump(s,
			    "\t\tattach[%d]: iova:0x%-12lx attr:%-4lx dir:%-2d dev:%s\n",
			    attach_cnt, iova,
			    attach_obj->dma_map_attrs,
			    attach_obj->dir,
			    device_name);
	}
	dmabuf_dump(s, "\t\tTotal %d devices attached\n\n", attach_cnt);
	dma_resv_unlock(dmabuf->resv);

	return 0;

}

/* support @heap == NULL*/
static int dma_heap_total_cb(const struct dma_buf *dmabuf,
			     void *priv)
{
	struct mtk_heap_dump_s *dump_info = (typeof(dump_info))priv;
	struct dma_heap *heap = dump_info->heap;

	if (!heap || is_dmabuf_from_heap(dmabuf, heap))
		dump_info->ret += dmabuf->size;

	return 0;
}

static long get_dma_heap_buffer_total(struct dma_heap *heap)
{
	struct mtk_heap_dump_s dump_info;

	if (!heap)
		return -1;

	dump_info.file = NULL;
	dump_info.heap = heap;
	dump_info.ret = 0; /* used to record total size */

	get_each_dmabuf(dma_heap_total_cb, (void *)&dump_info);

	return dump_info.ret;
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
	unsigned long mmap_size = 0;
	unsigned long total_mmap = 0;

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

		mmap_size = vma->vm_end - vma->vm_start;
		dmabuf_dump(s,
			    "\t\tinode:%d sz:0x%lx va:%08lx-%08lx map_sz:0x%lx prot:%c%c%c%c off:%08llx node:%s\n",
			    file_inode(file)->i_ino,
			    dmabuf->size,
			    vma->vm_start, vma->vm_end,
			    mmap_size,
			    flags & VM_READ ? 'r' : '-',
			    flags & VM_WRITE ? 'w' : '-',
			    flags & VM_EXEC ? 'x' : '-',
			    flags & VM_MAYSHARE ? 's' : 'p',
			    pgoff, path_p);

		total_mmap += mmap_size;
next_vma:
		vma = vma->vm_next;
		mapcount++;
	}
	dmabuf_dump(s, "\tpid:%d(%s)----> total mmap:%lu\n\n",
		    t->pid, t->comm, total_mmap);
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
	struct fd_rb_s *new_node = NULL;
	struct rb_node **ppn, *rb;/* ppn: pointer of parent node */

	dmabuf = file_to_dmabuf(file);
	if (IS_ERR_OR_NULL(dmabuf))
		return 0;

	/* heap is valid but buffer is not from this heap */
	if (heap && !is_dmabuf_from_heap(dmabuf, heap))
		return 0;

	dmabuf_dump(s, "\tpid:%-8d\t%-8d\t%-14zu\t%-8d\t%-8ld\t%-8s\n",
		    p->pid, fd, dmabuf->size,
		    file_inode(dmabuf->file)->i_ino,
		    file_count(dmabuf->file),
		    dmabuf->exp_name?:"NULL");

	new_node = kzalloc(sizeof(*new_node), GFP_ATOMIC);
	if (!new_node) {
		fd_info->err = 1;
		/* error case, add all */
		fd_info->size += dmabuf->size;
		return 0;
	}

	new_node->fd_val = fd;
	new_node->inode = file_inode(dmabuf->file)->i_ino;

	/* link to rb_tree */
	rb = NULL;
	ppn = &fd_info->fd_root.rb_node;
	while (*ppn) {
		struct fd_rb_s *pos;

		rb = *ppn;
		pos = rb_entry(rb, struct fd_rb_s, node);
		if (new_node->inode == pos->inode) {
			/* already added before */
			if (fd_rb_check)
				dmabuf_dump(s, "fd:%d, inode:%d, added before\n",
					    new_node->fd_val, new_node->inode);
			kfree(new_node);
			return 0;
		} else if (new_node->inode > pos->inode) {
			ppn = &rb->rb_right;
		} else {
			ppn = &rb->rb_left;
		}
	}

	rb_link_node(&new_node->node, rb, ppn);
	rb_insert_color(&new_node->node, &fd_info->fd_root);

	if (fd_rb_check)
		dmabuf_dump(s, "add: fd:%d, inode:%lu\n",
			    new_node->fd_val, new_node->inode);

	fd_info->size += dmabuf->size;
	return 0;
}

static void dump_dmabuf_fds(struct seq_file *s, struct dma_heap *heap)
{
	struct task_struct *p;
	struct dump_fd_data fddata;
	int res;

	struct rb_node *tmp_rb;
	struct fd_rb_s *fd_entry;

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

		if (!RB_EMPTY_ROOT(&fddata.fd_root)) {
			dmabuf_dump(s, "err, rb tree not empty\n");
			/* clear fd info before every time iterate */
			tmp_rb = rb_first(&fddata.fd_root);
			while (tmp_rb) {
				fd_entry =
					rb_entry(tmp_rb, struct fd_rb_s, node);
				dmabuf_dump(s, "clear: fd:%d, inode:%lu\n",
					    fd_entry->fd_val, fd_entry->inode);
				rb_erase(tmp_rb, &fddata.fd_root);
				kfree(fd_entry);
				tmp_rb = rb_first(&fddata.fd_root);
			}
		}

		res = iterate_fd(p->files, 0, has_dmabuf_fd, &fddata);
		if (!res) {
			task_unlock(p);
			continue;
		}

		/* start from first dmabuf fd */
		res = iterate_fd(p->files, res, _do_dump_dmabuf_fd, &fddata.constd);
		dmabuf_dump(s, "\tpid:%d(%s) Total size:%d KB, err:%d\n\n",
			    p->pid, p->comm,
			    fddata.size * 4 / PAGE_SIZE,
			    fddata.err);

		/* dump check */
		if (fd_rb_check)
			for (tmp_rb = rb_first(&fddata.fd_root);
			     tmp_rb;
			     tmp_rb = rb_next(tmp_rb)) {
				fd_entry =
					rb_entry(tmp_rb, struct fd_rb_s, node);
				dmabuf_dump(s, "dump: fd:%d, inode:%lu\n",
					    fd_entry->fd_val, fd_entry->inode);
			}

		/* erase rb tree */
		if (fd_rb_check)
			dmabuf_dump(s, "clear rb tree\n");

		/* clear fd info before every time iterate */
		tmp_rb = rb_first(&fddata.fd_root);
		while (tmp_rb) {
			fd_entry = rb_entry(tmp_rb, struct fd_rb_s, node);
			if (fd_rb_check)
				dmabuf_dump(s, "erase: fd:%d, inode:%lu\n",
					    fd_entry->fd_val, fd_entry->inode);
			rb_erase(tmp_rb, &fddata.fd_root);
			kfree(fd_entry);
			tmp_rb = rb_first(&fddata.fd_root);
		}

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
	struct mtk_heap_dump_s dump_param;
	struct dma_heap_export_info *exp_info = (typeof(exp_info))heap;

	dump_param.heap = heap;
	dump_param.file = seq_file;

	/**
	 * buffer total size show
	 * add a heap total count maybe more better
	 */
	dmabuf_dump(s, "[%s] buffer total size: %ld KB\n",
		    heap ? dma_heap_get_name(heap) : "all",
		    get_dma_heap_buffer_total(heap)*4/PAGE_SIZE);

	/* pool data show */
	if (exp_info && exp_info->ops && exp_info->ops->get_pool_size)
		dmabuf_dump(s, "[%s] page_pool size: %ld KB\n",
			    dma_heap_get_name(heap),
			    exp_info->ops->get_pool_size(heap)*4/PAGE_SIZE);
	else
		dmabuf_dump(s, "[%s] No page_pool data\n",
			    dma_heap_get_name(heap));

	if (flag & HEAP_DUMP_SKIP_ATTACH)
		goto attach_done;

	get_each_dmabuf(dma_heap_default_attach_dump_cb, &dump_param);
attach_done:

	if (flag & HEAP_DUMP_SKIP_FD)
		goto fd_dump_done;
	dump_dmabuf_fds(s, heap);
fd_dump_done:

	return;
}

/* Goal: for different heap, show different info */
static void show_help_info(struct dma_heap *heap, struct seq_file *s)
{
	int i = 0;

	dmabuf_dump(s, "%s: help info\n",
		    heap ? dma_heap_get_name(heap) : "all_heaps");

	dmabuf_dump(s, "- Set debug name for dmabuf: %s\n",
		    "https://wiki.mediatek.inc/display/WSDOSS3ME18/How+to+set+dmabuf+debug+name");

	dmabuf_dump(s, "all debug cmd:\n");
	for (i = 0; i < ARRAY_SIZE(heap_helper); i++) {
		dmabuf_dump(s, "- %s %s\n",
			    heap_helper[i].str,
			    *heap_helper[i].flag ? "on" : "off");
	}
	dmabuf_dump(s, "\n");
}

static void mtk_dmabuf_dump_heap(struct dma_heap *heap,
				 struct seq_file *s,
				 int flag)
{
	if (!heap) {
		struct mtk_heap_dump_s dump_param;
		int i = 0;
		long heap_sz = 0;
		long total_sz = 0;
		int local_flag = 0;

		dump_param.heap = heap;
		dump_param.file = s;

		dmabuf_dump(s, "[All heap] dump start @%llu ms\n",
			    get_current_time_ms());

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
		get_each_dmabuf(dma_heap_default_attach_dump_cb, &dump_param);

		/* dump all fds */
		dump_dmabuf_fds(s, NULL);
		dmabuf_dump(s, "dmabuf total:%d KB\n", (total_sz*4)/PAGE_SIZE);
		return;
	}

	dma_heap_default_show(heap, s, flag);
}

static struct mtk_heap_priv_info default_heap_priv = {
	.show = dma_heap_default_show,
};


/* dump all heaps */
static inline void mtk_dmabuf_dump_all(struct seq_file *s, int flag)
{
	return mtk_dmabuf_dump_heap(NULL, s, flag);
}

#if IS_ENABLED(CONFIG_PROC_FS)
static ssize_t dma_heap_all_proc_write(struct file *file, const char *buf,
				       size_t count, loff_t *data)
{
	char cmdline[DMA_HEAP_CMDLINE_LEN];
	int i = 0;
	int helper_len = 0;
	const char *helper_str = NULL;

	if (count >= DMA_HEAP_CMDLINE_LEN)
		return count;

	if (copy_from_user(cmdline, buf, count))
		return -EINVAL;

	cmdline[count] = 0;

	pr_info("%s #%d: set info:%s", __func__, __LINE__, cmdline);

	for (i = 0; i < ARRAY_SIZE(heap_helper); i++) {
		helper_len = strlen(heap_helper[i].str);
		helper_str = heap_helper[i].str;

		if (!strncmp(cmdline, helper_str, helper_len)) {
			if (!strncmp(cmdline + helper_len, "on", 2)) {
				*heap_helper[i].flag = 1;
				pr_info("%s set as 1\n", helper_str);
			} else if (!strncmp(cmdline + helper_len, "off", 3)) {
				*heap_helper[i].flag = 0;
				pr_info("%s set as 0\n", helper_str);
			}

			break;
		}
	}

	return count;
}

static ssize_t dma_heap_proc_write(struct file *file, const char *buf,
				   size_t count, loff_t *data)
{
	struct dma_heap *heap = PDE_DATA(file->f_inode);
	char cmdline[DMA_HEAP_CMDLINE_LEN];
	enum DMA_HEAP_T_CMD cmd = DMABUF_T_END;
	int ret = 0;

	if (count >= DMA_HEAP_CMDLINE_LEN)
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
	struct mtk_heap_priv_info *heap_priv = NULL;

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

		/* add heap show support */
		heap_priv = dma_heap_get_drvdata(heap);

		/* for system heap */
		if (!heap_priv)
			heap_priv = &default_heap_priv;
		/* for mtk heap */
		else if (heap_priv && !heap_priv->show)
			heap_priv->show = default_heap_priv.show;

		dma_heap_put(heap);
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
#else
static inline int dma_buf_init_procfs(void)
{
	return 0;
}
static inline void dma_buf_uninit_procfs(void)
{
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
