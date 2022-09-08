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
#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#include "deferred-free-helper.h"
#include "mtk_heap_priv.h"
#include "mtk_heap.h"

/* debug flags */
int vma_dump_enable;
int dmabuf_rb_check;
int dump_all_attach;


#define _HEAP_FD_FLAGS_           (O_CLOEXEC|O_RDWR)
#define DMA_HEAP_CMDLINE_LEN      (30)
#define DMA_HEAP_DUMP_ALLOC_GFP   (GFP_ATOMIC)
#define OOM_DUMP_INTERVAL         (2000)  /* unit: ms */
#define SET_PID_CMDLINE_LEN       (16)

/* Bit map for error */
#define DBG_ALLOC_MEM_FAIL        (1 << 0)
#define PID_ALLOC_MEM_FAIL        (1 << 1)
#define FD_ALLOC_MEM_FAIL         (1 << 2)
#define VMA_ALLOC_MEM_FAIL        (1 << 3)
#define VMA_MMAP_LOCK_FAIL        (1 << 4)


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
	bool                     mapped[MTK_M4U_TAB_NR_MAX][MTK_M4U_DOM_NR_MAX];
	struct mtk_heap_dev_info dev_info[MTK_M4U_TAB_NR_MAX][MTK_M4U_DOM_NR_MAX];
	struct sg_table          *mapped_table[MTK_M4U_TAB_NR_MAX][MTK_M4U_DOM_NR_MAX];
	struct mutex             map_lock; /* map iova lock */
	pid_t                    pid;
	pid_t                    tid;
	char                     pid_name[TASK_COMM_LEN];
	char                     tid_name[TASK_COMM_LEN];
	unsigned long long       ts; /* us */
};

enum stats_type {
	STATS_KRN_RSS,
	STATS_PSS,
	STATS_RSS,
	STATS_INVALID,
};

enum DMA_HEAP_T_CMD {
	DMABUF_T_OOM_TEST,
	DMABUF_T_END,
};

const char *DMA_HEAP_T_CMD_STR[] = {
	"DMABUF_T_OOM_TEST",
	"DMABUF_T_END",
};

struct mtk_heap_dump_s {
	struct seq_file *file;
	struct dma_heap *heap;
	long ret; /* used for storing return value */
	unsigned long flag;
};

struct dma_heap_dbg {
	const char *str;
	int *flag;
};

const struct dma_heap_dbg heap_helper[] = {
	{"vma_dump:", &vma_dump_enable},
	{"dmabuf_rb_check:", &dmabuf_rb_check},
	{"dump_all_attach:", &dump_all_attach},
};

struct heap_status_s {
	const char *heap_name;
	struct dma_heap *heap;
	int heap_exist;
};

struct fd_const {
	struct task_struct *p;
	struct seq_file *s;
	struct dma_heap *heap;
};

struct pid_map {
	pid_t id;
	char name[TASK_COMM_LEN];
};

/* 100 is enough for most cases */
#define TOTAL_PID_CNT   (100)
struct dump_fd_data {
	/* can be changed part */
	spinlock_t splock;/* lock for dmabuf_root */
	struct rb_root dmabuf_root;

	int cache_idx_r;
	int cache_idx_w;
	struct pid_map pid_map[TOTAL_PID_CNT];

	int err;
	int ret;
	/* can't changed part */
	struct fd_const constd;
};

struct dmabuf_fd_res {
	struct list_head fd_res;

	int fd_val;
	struct dmabuf_pid_res *p;
};

struct dmabuf_vm_res {
	struct list_head vm_res;

	unsigned long vm_start;
	unsigned long vm_size;
	struct dmabuf_pid_res *p;
};

struct dmabuf_pid_res {
	struct list_head pid_res;

	spinlock_t splock;
	struct list_head fds_head;
	struct list_head vms_head;
	pid_t pid;
	int fd_cnt;
	size_t RSS;
	int vm_cnt;
	unsigned long map_size;
	struct dmabuf_debug_node *p;
};

struct dmabuf_debug_node {
	struct rb_node dmabuf_node;
	struct list_head pids_res;

	spinlock_t splock;
	const struct dma_buf *dmabuf;
	unsigned long inode;
	int fd_cnt_total;
	int vm_cnt_total;
	unsigned long mmap_size;
};

#if IS_ENABLED(CONFIG_PROC_FS)
struct proc_dir_entry *dma_heap_proc_root;
struct proc_dir_entry *dma_heaps_dir;
struct proc_dir_entry *dma_heaps_all_entry;
struct proc_dir_entry *dma_heaps_stats;
struct proc_dir_entry *dma_heaps_stat_pid;
#endif


int oom_nb_status; /* 0 means register pass */
unsigned long long last_oom_time;/* ms */
unsigned long debug_alloc_sz;

struct heap_status_s debug_heap_list[] = {
	{"mtk_mm", NULL, 0},
	{"system", NULL, 0},
	{"mtk_mm-uncached", NULL, 0},
	{"system-uncached", NULL, 0},
	{"mtk_svp_region", NULL, 0},
	{"mtk_svp_region-aligned", NULL, 0},
	{"mtk_svp_page-uncached", NULL, 0},
	{"mtk_prot_region", NULL, 0},
	{"mtk_prot_region-aligned", NULL, 0},
	{"mtk_prot_page-uncached", NULL, 0},
	{"mtk_2d_fr_region", NULL, 0},
	{"mtk_2d_fr_region-aligned", NULL, 0},
	{"mtk_wfd_region", NULL, 0},
	{"mtk_wfd_region-aligned", NULL, 0},
	{"mtk_wfd_page-uncached", NULL, 0},
	{"mtk_sapu_data_shm_region", NULL, 0},
	{"mtk_sapu_data_shm_region-aligned", NULL, 0},
	{"mtk_sapu_engine_shm_region", NULL, 0},
	{"mtk_sapu_engine_shm_region-aligned", NULL, 0},
};
#define _DEBUG_HEAP_CNT_  (ARRAY_SIZE(debug_heap_list))

static inline struct dump_fd_data *
fd_const_to_dump_fd_data(const struct fd_const *d)
{
	return container_of(d, struct dump_fd_data, constd);
}

static inline struct dump_fd_data *
dmabuf_root_to_dump_fd_data(const struct rb_root *root)
{
	return container_of(root, struct dump_fd_data, dmabuf_root);
}

void dump_pid_map(struct dump_fd_data *fd_data)
{
	struct pid_map *map = fd_data->pid_map;
	struct seq_file *s = fd_data->constd.s;
	int i;

	dmabuf_dump(s, "pid table:\n");
	for (i = 0; i < TOTAL_PID_CNT; i++) {
		if (!map[i].id)
			break;
		dmabuf_dump(s, "\tpid:%-6d name:%s\n", map[i].id, map[i].name);
	}
	dmabuf_dump(s, "\n");
}

char *pid_map_get_name(struct dump_fd_data *fd_data, pid_t num)
{
	struct pid_map *map = fd_data->pid_map;
	int idx = fd_data->cache_idx_r;
	char *name = NULL;
	int i = 0;

	if (map[idx].id == num)
		return map[idx].name;

	for (i = 0; i < TOTAL_PID_CNT; i++) {
		if (!map[i].id)
			break;
		if (map[i].id == num) {
			name = map[i].name;
			/* update cache_idx */
			fd_data->cache_idx_r = i;
		}
	}

	return name;
}

/* add pid to given pid_map
 * return 0 means pass
 * return error code when fail
 */
int add_pid_map_entry(struct dump_fd_data *fddata, pid_t num, const char *comm)
{
	struct pid_map *map = fddata->pid_map;
	struct seq_file *s = fddata->constd.s;
	int idx = fddata->cache_idx_w;

	if (idx >= TOTAL_PID_CNT) {
		/* full */
		dmabuf_dump(s, "%s err, entry is full, %d\n", __func__, idx);
		dump_pid_map(fddata);
		return -ENOMEM;
	}

	if (map[idx].id == 0) {
		map[idx].id = num;
		strncpy(map[idx].name, comm, TASK_COMM_LEN - 1);
		/* update idx */
		fddata->cache_idx_w++;
	}

	return 0;
}

/*
 * This API will increase a file count of dmabuf
 *
 * add 'noinline' to let it show in callstack
 */
static noinline
struct dma_buf *get_dmabuf_from_file(struct file *file)
{
	struct file tmp_file;

	/*
	 * 1. add invalid pointer check
	 * 2. atomic check(file count != 0) and add 1 reference
	 */
	if (!get_kernel_nofault(tmp_file, file) &&
	    is_dma_buf_file(file) &&
	    get_file_rcu(file))
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

static int is_dmabuf_from_dma_heap(const struct dma_buf *dmabuf)
{
	if (is_mtk_mm_heap_dmabuf(dmabuf) ||
	    is_system_heap_dmabuf(dmabuf))
		return 1;
#if IS_ENABLED(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)
	if (is_mtk_sec_heap_dmabuf(dmabuf))
		return 1;
#endif
	return 0;
}

/* not support @heap == NULL */
int is_dmabuf_from_heap(const struct dma_buf *dmabuf, struct dma_heap *heap)
{
	struct dmaheap_buf_copy *heap_buf = dmabuf->priv;

	if (!heap || !is_dmabuf_from_dma_heap(dmabuf))
		return 0;

	return (heap_buf->heap == heap);
}

static struct dmabuf_vm_res *
dmabuf_rbtree_vmres_add_return(struct dump_fd_data *fd_data,
			       struct dmabuf_pid_res *node,
			       struct vm_area_struct *vma)
{
	struct seq_file *s = fd_data->constd.s;
	struct dmabuf_vm_res *vm_res = NULL;
	struct list_head *new = NULL;

	if (vma->vm_file->private_data != node->p->dmabuf)
		return ERR_PTR(-EINVAL);

	list_for_each(new, &node->vms_head) {
		vm_res = list_entry(new, struct dmabuf_vm_res, vm_res);
		if (vm_res->vm_start == vma->vm_start)
			return vm_res;
	}

	vm_res = kzalloc(sizeof(*vm_res), DMA_HEAP_DUMP_ALLOC_GFP);
	if (!vm_res)
		return ERR_PTR(-ENOMEM);

	debug_alloc_sz += sizeof(*vm_res);

	vm_res->vm_start = vma->vm_start;
	vm_res->vm_size = vma->vm_end - vma->vm_start;
	vm_res->p = node;

	spin_lock(&node->splock);
	list_add(&vm_res->vm_res, &node->vms_head);
	node->vm_cnt++;

	/* process mmap size */
	node->map_size += vm_res->vm_size;
	node->p->vm_cnt_total++;

	/* buffer total mmap size */
	node->p->mmap_size += vm_res->vm_size;
	spin_unlock(&node->splock);

	if (dmabuf_rb_check)
		dmabuf_dump(s,
			    "[A] [VA] inode:%lu pid[%d:%s] va:0x%lx-0x%lx map_sz:%lu inode:%lu buf_sz:%zu\n",
			    node->p->inode,
			    node->pid, pid_map_get_name(fd_data, node->pid),
			    vm_res->vm_start,
			    vm_res->vm_start + vm_res->vm_size,
			    vm_res->vm_size,
			    node->p->inode,
			    node->p->dmabuf->size);

	return vm_res;
}

static struct dmabuf_fd_res *
dmabuf_rbtree_fdres_add_return(struct dump_fd_data *fd_data,
			       struct dmabuf_pid_res *node,
			       int fd)
{
	struct dmabuf_fd_res *fd_res_entry = NULL;
	struct list_head *new = NULL;
	struct seq_file *s = fd_data->constd.s;

	list_for_each(new, &node->fds_head) {
		fd_res_entry = list_entry(new, struct dmabuf_fd_res, fd_res);
		if (fd_res_entry->fd_val == fd)
			return fd_res_entry;
	}

	fd_res_entry = kzalloc(sizeof(*fd_res_entry), DMA_HEAP_DUMP_ALLOC_GFP);
	if (!fd_res_entry)
		return ERR_PTR(-ENOMEM);

	debug_alloc_sz += sizeof(*fd_res_entry);

	fd_res_entry->fd_val = fd;
	fd_res_entry->p = node;

	spin_lock(&node->splock);
	list_add(&fd_res_entry->fd_res, &node->fds_head);
	node->fd_cnt++;
	node->RSS += node->p->dmabuf->size;
	node->p->fd_cnt_total++;
	spin_unlock(&node->splock);

	if (dmabuf_rb_check)
		dmabuf_dump(s, "[A] [FD] inode:%lu pid[%d] fd:%d sz:%zu\n",
			    node->p->inode,
			    node->pid,
			    fd,
			    node->p->dmabuf->size);

	return fd_res_entry;
}

static struct dmabuf_pid_res *
dmabuf_rbtree_pidres_add_return(struct dump_fd_data *fd_data,
				struct dmabuf_debug_node *dbg_node,
				struct task_struct *p)
{
	struct dmabuf_pid_res *pid_entry = NULL;
	struct list_head *node = NULL;
	struct seq_file *s = fd_data->constd.s;

	list_for_each(node, &dbg_node->pids_res) {
		pid_entry = list_entry(node, struct dmabuf_pid_res, pid_res);
		if (p->pid == pid_entry->pid)
			return pid_entry;
	}

	pid_entry = kzalloc(sizeof(*pid_entry), DMA_HEAP_DUMP_ALLOC_GFP);
	if (!pid_entry)
		return ERR_PTR(-ENOMEM);

	debug_alloc_sz += sizeof(*pid_entry);

	pid_entry->pid = p->pid;
	pid_entry->p = dbg_node;

	INIT_LIST_HEAD(&pid_entry->fds_head);
	INIT_LIST_HEAD(&pid_entry->vms_head);
	spin_lock_init(&pid_entry->splock);

	spin_lock(&dbg_node->splock);
	list_add(&pid_entry->pid_res, &dbg_node->pids_res);
	spin_unlock(&dbg_node->splock);
	if (dmabuf_rb_check)
		dmabuf_dump(s, "[A] [PID] inode:%lu pid[%d:%s]\n",
			    pid_entry->p->inode,
			    pid_entry->pid,
			    pid_map_get_name(fd_data, pid_entry->pid));

	return pid_entry;
}

static struct dmabuf_debug_node *
dmabuf_rbtree_dbg_find(struct dump_fd_data *fd_data, unsigned long ino)
{
	struct rb_root *root = &fd_data->dmabuf_root;
	struct rb_node **ppn = NULL, *rb = NULL;

	rb = NULL;
	ppn = &root->rb_node;
	while (*ppn) {
		struct dmabuf_debug_node *pos = NULL;

		rb = *ppn;
		pos = rb_entry(rb, struct dmabuf_debug_node, dmabuf_node);
		if (ino == pos->inode)
			return pos;
		else if (ino > pos->inode)
			ppn = &rb->rb_right;
		else
			ppn = &rb->rb_left;
	}

	return NULL;
}


static struct dmabuf_debug_node *
dmabuf_rbtree_dbg_add_return(struct dump_fd_data *fd_data, const struct dma_buf *dmabuf)
{
	struct rb_root *root = &fd_data->dmabuf_root;
	unsigned long ino = file_inode(dmabuf->file)->i_ino;
	struct dmabuf_debug_node *dbg_node = NULL;
	struct rb_node **ppn = NULL, *rb = NULL;

	rb = NULL;
	ppn = &root->rb_node;
	while (*ppn) {
		struct dmabuf_debug_node *pos = NULL;

		rb = *ppn;
		pos = rb_entry(rb, struct dmabuf_debug_node, dmabuf_node);
		if (ino > pos->inode) {
			ppn = &rb->rb_right;
		} else if (ino < pos->inode) {
			ppn = &rb->rb_left;
		} else {
			WARN_ON(1);
			return pos;
		}
	}

	dbg_node = kzalloc(sizeof(*dbg_node), DMA_HEAP_DUMP_ALLOC_GFP);
	if (!dbg_node) {
		/* alloc memory fail, decrease 1 count added before */
		dma_buf_put((struct dma_buf *)dmabuf);
		return ERR_PTR(-ENOMEM);
	}

	debug_alloc_sz += sizeof(*dbg_node);

	dbg_node->dmabuf = dmabuf;
	dbg_node->inode = ino;
	spin_lock_init(&dbg_node->splock);

	INIT_LIST_HEAD(&dbg_node->pids_res);

	rb_link_node(&dbg_node->dmabuf_node, rb, ppn);
	rb_insert_color(&dbg_node->dmabuf_node, root);
	if (dmabuf_rb_check) {
		spin_lock((spinlock_t *)&dmabuf->name_lock);
		dmabuf_dump(fd_data->constd.s,
			    "add2rbtree done| inode:%lu, sz:%zu, cnt:%ld, name:%s\n",
			    file_inode(dmabuf->file)->i_ino,
			    dmabuf->size,
			    file_count(dmabuf->file),
			    dmabuf->name?:"NULL");
		spin_unlock((spinlock_t *)&dmabuf->name_lock);
	}

	return dbg_node;
}

/* clear rbtree before every time iterate */
unsigned long dmabuf_dbg_rbtree_clear(struct dump_fd_data *fd_data)
{
	struct rb_root *root = &fd_data->dmabuf_root;
	struct seq_file *s = fd_data->constd.s;
	const struct dma_buf *dmabuf = NULL;
	struct dmabuf_debug_node *entry = NULL;
	struct rb_node *tmp_rb = NULL;
	unsigned long free_size = 0;
	struct dma_buf tmp_dmabuf;
	struct file tmp_file;

	spin_lock(&fd_data->splock);
	while (rb_first(root)) {

		tmp_rb = rb_first(root);
		entry = rb_entry(tmp_rb, struct dmabuf_debug_node, dmabuf_node);
		dmabuf = entry->dmabuf;

		if (!get_kernel_nofault(tmp_dmabuf, dmabuf) &&
		    !get_kernel_nofault(tmp_file, dmabuf->file) &&
		    is_dma_buf_file(dmabuf->file))
			/* add 1 ref when add to rb tree, put it after dump */
			dma_buf_put((struct dma_buf *)dmabuf);

		if (dmabuf_rb_check)
			dmabuf_dump(s, "[R] [INODE] clear inode:%lu\n", entry->inode);

		/* clear pid res */
		while (!list_empty(&entry->pids_res)) {
			struct dmabuf_pid_res *pid_entry = NULL;

			pid_entry = list_first_entry(&entry->pids_res,
						     struct dmabuf_pid_res,
						     pid_res);
			if (dmabuf_rb_check)
				dmabuf_dump(s, "\t[R] [PID] inode %ld, pid[%d:%s]\n",
					    pid_entry->p->inode,
					    pid_entry->pid,
					    pid_map_get_name(fd_data, pid_entry->pid));

			while (!list_empty(&pid_entry->fds_head)) {
				struct dmabuf_fd_res *fd_entry = NULL;

				fd_entry = list_first_entry(&pid_entry->fds_head,
							    struct dmabuf_fd_res,
							    fd_res);
				spin_lock(&pid_entry->splock);
				list_del(&fd_entry->fd_res);
				spin_unlock(&pid_entry->splock);
				if (dmabuf_rb_check)
					dmabuf_dump(s, "\t\t[R] [FD] inode:%lu pid[%d:%s] fd:%d\n",
						    fd_entry->p->p->inode,
						    fd_entry->p->pid,
						    pid_map_get_name(fd_data, fd_entry->p->pid),
						    fd_entry->fd_val);
				free_size += sizeof(*fd_entry);
				kfree(fd_entry);
			}

			while (!list_empty(&pid_entry->vms_head)) {
				struct dmabuf_vm_res *vm_entry = NULL;

				vm_entry = list_first_entry(&pid_entry->vms_head,
							    struct dmabuf_vm_res,
							    vm_res);
				spin_lock(&pid_entry->splock);
				list_del(&vm_entry->vm_res);
				spin_unlock(&pid_entry->splock);
				if (dmabuf_rb_check)
					dmabuf_dump(s,
						    "\t\t[R] [VA] inode:0x%lx pid[%d:%s] va:0x%lx sz:0x%lx\n",
						    vm_entry->p->p->inode,
						    vm_entry->p->pid,
						    pid_map_get_name(fd_data, vm_entry->p->pid),
						    vm_entry->vm_start,
						    vm_entry->vm_size);
				free_size += sizeof(*vm_entry);
				kfree(vm_entry);
			}
			spin_lock(&entry->splock);
			list_del(&pid_entry->pid_res);
			spin_unlock(&entry->splock);
			free_size += sizeof(*pid_entry);
			kfree(pid_entry);
		}
		rb_erase(tmp_rb, root);
		free_size += sizeof(*entry);

		kfree(entry);
	}
	spin_unlock(&fd_data->splock);
	free_size += sizeof(*fd_data);
	kfree(fd_data);

	return free_size;
}

static unsigned long dmabuf_rbtree_get_stats(struct rb_root *root, pid_t pid,
					     enum stats_type type)
{
	struct rb_root *rbroot = root;
	struct rb_node *tmp_rb;
	const struct dma_buf *dmabuf;

	struct dmabuf_debug_node *dbg_node;
	struct dmabuf_pid_res *pid_info;
	struct list_head *pid_node;

	unsigned long pss = 0;
	unsigned long rss = 0;
	unsigned long buf_pss = 0;
	unsigned long krn_rss = 0;

	for (tmp_rb = rb_first(rbroot); tmp_rb; tmp_rb = rb_next(tmp_rb)) {
		dbg_node = rb_entry(tmp_rb, struct dmabuf_debug_node, dmabuf_node);
		buf_pss = 0;
		dmabuf = dbg_node->dmabuf;

		/* pid=0 means kerel rss */
		if (!pid && !dbg_node->fd_cnt_total && !dbg_node->vm_cnt_total) {
			krn_rss += dmabuf->size;
			continue;
		}

		list_for_each(pid_node, &dbg_node->pids_res) {
			pid_info = list_entry(pid_node, struct dmabuf_pid_res, pid_res);
			if (pid_info->pid == pid) {
				rss += dmabuf->size;
				if (!dbg_node->mmap_size)
					continue;

				buf_pss = dmabuf->size * pid_info->map_size / dbg_node->mmap_size;
				break;
			}
		}
		pss += buf_pss;
	}

	switch (type) {
	case STATS_KRN_RSS:   return krn_rss;
	case STATS_RSS:       return rss;
	case STATS_PSS:       return pss;
	default:              return 0;
	}

	return 0;
}

/*
 * dump all dmabuf userspace va info
 *
 * Reference code: drivers/misc/mediatek/monitor_hang/hang_detect.c
 *
 * return 0 means pass
 * return error code when fail
 */
static int dmabuf_rbtree_add_vmas(struct dump_fd_data *fd_data)
{
	struct seq_file *s = fd_data->constd.s;
	struct dma_heap *heap = fd_data->constd.heap;
	struct task_struct *t = fd_data->constd.p;

	struct vm_area_struct *vma;
	int mapcount = 0;
	struct file *file;
	struct dma_buf *dmabuf;

	if (!t->mm || t->flags & PF_KTHREAD)
		return 0;

	/* get mmap_lock to prevent vma disappear */
	if (!mmap_read_trylock(t->mm)) {
		fd_data->err |= VMA_MMAP_LOCK_FAIL;
		return 0;
	}

	vma = t->mm->mmap;
	while (vma && (mapcount < t->mm->map_count)) {
		struct dmabuf_debug_node *dbg_node = NULL;
		struct dmabuf_pid_res *pid_res = NULL;
		struct dmabuf_vm_res *vm_res = NULL;
		struct vm_area_struct vma_val;

		if (get_kernel_nofault(vma_val, vma))
			goto next_vma;

		file = vma->vm_file;
		if (IS_ERR_OR_NULL(get_dmabuf_from_file(file)))
			goto next_vma;

		dmabuf = file->private_data;

		/* heap is valid but buffer is not from this heap */
		if (heap && !is_dmabuf_from_heap(dmabuf, heap)) {
			dma_buf_put(dmabuf);
			goto next_vma;
		}

		/* found vma */
		fd_data->ret = 1;

		dbg_node = dmabuf_rbtree_dbg_find(fd_data, file_inode(dmabuf->file)->i_ino);
		dma_buf_put(dmabuf);
		if (!dbg_node) {
			dmabuf_dump(s, "%s#%d err:%ld\n", __func__, __LINE__, PTR_ERR(dbg_node));
			goto out;
		}
		pid_res = dmabuf_rbtree_pidres_add_return(fd_data, dbg_node, t);
		if (IS_ERR_OR_NULL(pid_res)) {
			dmabuf_dump(s, "%s#%d err:%ld\n", __func__, __LINE__, PTR_ERR(pid_res));
			fd_data->err |= PID_ALLOC_MEM_FAIL;
			goto out;
		}
		vm_res = dmabuf_rbtree_vmres_add_return(fd_data, pid_res, vma);
		if (IS_ERR_OR_NULL(vm_res)) {
			dmabuf_dump(s, "%s#%d err:%ld\n", __func__, __LINE__, PTR_ERR(vm_res));
			fd_data->err |= VMA_ALLOC_MEM_FAIL;
			goto out;
		}
next_vma:
		vma = vma->vm_next;
		mapcount++;
	}
out:
	mmap_read_unlock(t->mm);
	return fd_data->err;
}

static void dma_heap_priv_dump(const struct dma_buf *dmabuf,
			       struct dma_heap *dump_heap,
			       struct seq_file *s)
{
	struct dmaheap_buf_copy *buf = dmabuf->priv;

	if (dump_heap) {
		if (is_dmabuf_from_heap(dmabuf, dump_heap) && buf->show)
			buf->show(dmabuf, s);
	} else {
		if (is_dmabuf_from_dma_heap(dmabuf) && buf->show)
			buf->show(dmabuf, s);
	}
}

static void dma_heap_attach_dump(const struct dma_buf *dmabuf,
				 struct seq_file *s)
{
	struct dma_buf_attachment *attach_obj;
	int attach_cnt = 0;
	dma_addr_t iova = 0x0;
	const char *device_name = NULL;

	list_for_each_entry(attach_obj, &dmabuf->attachments, node) {
		iova = (dma_addr_t)0;

		attach_cnt++;
		if (!attach_obj->sgt)
			if (!dump_all_attach)
				continue;
		if (!dev_iommu_fwspec_get(attach_obj->dev)) {
			if (!dump_all_attach)
				continue;
		} else if (attach_obj->sgt) {
			iova = sg_dma_address(attach_obj->sgt->sgl);
		}

		device_name = dev_name(attach_obj->dev);
		dmabuf_dump(s,
			    "\tattach[%d]: iova:0x%-14lx attr:%-4lx dir:%-2d dev:%s\n",
			    attach_cnt, iova,
			    attach_obj->dma_map_attrs,
			    attach_obj->dir,
			    device_name);
	}
	//dmabuf_dump(s, "\tTotal %d devices attached\n", attach_cnt);

}

static int dma_heap_buf_dump_cb(const struct dma_buf *dmabuf, void *priv)
{
	struct mtk_heap_dump_s *dump_param = priv;
	struct seq_file *s = dump_param->file;
	struct dma_heap *dump_heap = dump_param->heap;
	unsigned long flag = dump_param->flag;
	int delta = 0;

	/* heap is valid but buffer is not from this heap */
	if (dump_heap && !is_dmabuf_from_heap(dmabuf, dump_heap))
		return 0;

	if (flag & HEAP_DUMP_DEC_1_REF)
		delta = 1;

	spin_lock((spinlock_t *)&dmabuf->name_lock);
	dmabuf_dump(s, "inode:%-8lu size(Byte):%-10zu count:%-2ld cache_sg:%d  exp:%s\tname:%s\n",
		    file_inode(dmabuf->file)->i_ino,
		    dmabuf->size,
		    file_count(dmabuf->file) - delta,
		    dmabuf->ops->cache_sgt_mapping,
		    dmabuf->exp_name?:"NULL",
		    dmabuf->name?:"NULL");
	spin_unlock((spinlock_t *)&dmabuf->name_lock);

	dma_heap_priv_dump(dmabuf, dump_heap, s);

	if (!dma_resv_trylock(dmabuf->resv)) {
		dmabuf_dump(s, "\tget lock fail, maybe is using, skip attach dump\n");
		return 0;
	}

	if (!(flag & HEAP_DUMP_SKIP_ATTACH))
		dma_heap_attach_dump(dmabuf, s);

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

/* support NULL heap, means dump all */
static long get_dma_heap_buffer_total(struct dma_heap *heap)
{
	struct mtk_heap_dump_s dump_info;

	dump_info.file = NULL;
	dump_info.heap = heap;
	dump_info.ret = 0; /* used to record total size */

	get_each_dmabuf(dma_heap_total_cb, (void *)&dump_info);

	return dump_info.ret;
}

/*
 * only return 0 for this function.
 * check error by fd_data->err
 */
static int dmabuf_rbtree_add_fd_cb(const void *data, struct file *file,
				   unsigned int fd)
{
	struct dma_buf *dmabuf;
	const struct fd_const *d = data;
	struct seq_file *s = d->s;
	struct task_struct *p = d->p;
	struct dma_heap *heap = d->heap;
	struct dump_fd_data *fd_data = fd_const_to_dump_fd_data(d);
	unsigned long inode = 0;

	struct dmabuf_debug_node *dbg_node = NULL;
	struct dmabuf_pid_res    *pid_res = NULL;
	struct dmabuf_fd_res     *fd_res = NULL;

	dmabuf = get_dmabuf_from_file(file);
	if (IS_ERR_OR_NULL(dmabuf))
		return 0;

	/* heap is valid but buffer is not from this heap */
	if (heap && !is_dmabuf_from_heap(dmabuf, heap)) {
		dma_buf_put(dmabuf);
		return 0;
	}

	inode = file_inode(dmabuf->file)->i_ino;

	/* found_fd */
	fd_data->ret = 1;

	if (dmabuf_rb_check)
		dmabuf_dump(s, "\tpid:%-8d\t%-8d\t%-14zu\t%-8lu\t%-8ld%s\n",
			    p->pid, fd, dmabuf->size, inode,
			    file_count(dmabuf->file),
			    dmabuf->exp_name);

	dbg_node = dmabuf_rbtree_dbg_find(fd_data, inode);
	dma_buf_put(dmabuf);
	if (!dbg_node) {
		dmabuf_dump(s, "dbg_node add err:%ld\n", PTR_ERR(dbg_node));
		return 0;
	}
	pid_res = dmabuf_rbtree_pidres_add_return(fd_data, dbg_node, p);
	if (IS_ERR(pid_res)) {
		dmabuf_dump(s, "pid_res add err:%ld\n", PTR_ERR(pid_res));
		fd_data->err |= PID_ALLOC_MEM_FAIL;
		return 0;
	}
	fd_res = dmabuf_rbtree_fdres_add_return(fd_data, pid_res, fd);
	if (IS_ERR(fd_res)) {
		dmabuf_dump(s, "fdres add err:%ld\n", PTR_ERR(fd_res));
		fd_data->err |= FD_ALLOC_MEM_FAIL;
		return 0;
	}

	return 0;
}

int dmabuf_rbtree_dbg_add_cb(const struct dma_buf *dmabuf, void *priv)
{
	struct dmabuf_debug_node *node;
	struct dump_fd_data *fd_data = priv;
	struct seq_file *s = fd_data->constd.s;
	struct dma_heap *heap = fd_data->constd.heap;

	/* The dmabuf pointer is valid here, no need check it */
	dmabuf = get_dmabuf_from_file(dmabuf->file);
	if (IS_ERR_OR_NULL(dmabuf))
		return 0;

	/* heap is valid but buffer is not from this heap */
	if (heap && !is_dmabuf_from_heap(dmabuf, heap)) {
		dma_buf_put((struct dma_buf *)dmabuf);
		return 0;
	}

	node = dmabuf_rbtree_dbg_add_return(fd_data, dmabuf);
	if (IS_ERR_OR_NULL(node))
		dmabuf_dump(s, "%s #%d:get err:%ld\n",
			    __func__, __LINE__, PTR_ERR(node));
	return 0;
}

static noinline
void dmabuf_rbtree_add_all_pid(struct dump_fd_data *fddata,
			       struct dma_heap *heap,
			       struct seq_file *s, int pid)
{
	int ret = 0;
	int found_vma = 0;
	int found_fd = 0;
	struct pid *pid_s;
	struct task_struct *p;

	pid_s = find_get_pid(pid);
	if (!pid_s) {
		dmabuf_dump(s, "%s: fail pid:%d\n", __func__, pid);
		return;
	}

	p = get_pid_task(pid_s, PIDTYPE_PID);
	if (!p) {
		put_pid(pid_s);
		dmabuf_dump(s, "%s: fail pid:%d\n", __func__, pid);
		return;
	}

	/* spin lock */
	task_lock(p);
	fddata->constd.p = p;

	fddata->err = 0;
	fddata->ret = 0;
	ret = dmabuf_rbtree_add_vmas(fddata);
	found_vma = fddata->ret;
	if (ret)
		dmabuf_dump(s, "%s: add vma fail:%d\n", __func__, ret);

	fddata->ret = 0;
	fddata->err = 0;
	iterate_fd(p->files, 0, dmabuf_rbtree_add_fd_cb, &fddata->constd);
	found_fd = fddata->ret;
	if (fddata->err)
		dmabuf_dump(s, "[E] %s#%d pid:%d(%s) err:%d\n",
			    __func__, __LINE__, p->pid, p->comm, fddata->err);

	if (found_vma || found_fd)
		if (!pid_map_get_name(fddata, p->pid))
			ret = add_pid_map_entry(fddata, p->pid, p->comm);

	if (dmabuf_rb_check)
		dmabuf_dump(s, "\tpid:%d(%s) added, err val:%d\n\n",
			    p->pid, p->comm,
			    fddata->err);

	task_unlock(p);
	put_task_struct(p);
	put_pid(pid_s);
}

/* add 'noinline' to let it show in callstack */
static noinline
struct dump_fd_data *dmabuf_rbtree_add_all(struct dma_heap *heap,
					   struct seq_file *s, int pid)
{
	struct task_struct *p;
	struct dump_fd_data *fddata;
	int *pids;
	unsigned int pid_max = 0;
	unsigned int pid_count = 0;
	unsigned long long cur_ts1;
	unsigned long long cur_ts2;

	fddata = kzalloc(sizeof(*fddata), DMA_HEAP_DUMP_ALLOC_GFP);
	if (!fddata)
		return ERR_PTR(-ENOMEM);

	debug_alloc_sz += sizeof(*fddata);

	fddata->constd.s = s;
	fddata->constd.heap = heap;
	fddata->dmabuf_root = RB_ROOT;
	spin_lock_init(&fddata->splock);

	get_each_dmabuf(dmabuf_rbtree_dbg_add_cb, fddata);
	if (pid > 0) {
		dmabuf_rbtree_add_all_pid(fddata, heap, s, pid);
		return fddata;
	}

	cur_ts1 = sched_clock();
	rcu_read_lock();
	for_each_process(p)
		pid_max++;

	if (pid_max <= 0) {
		rcu_read_unlock();
		return fddata;
	}
	pids = kcalloc(pid_max, sizeof(*pids), DMA_HEAP_DUMP_ALLOC_GFP);
	if (!pids) {
		rcu_read_unlock();
		kfree(fddata);
		dmabuf_dump(s, "%s: no memory\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	for_each_process(p) {
		if (fatal_signal_pending(p))
			continue;

		pids[pid_count++] = p->pid;
		if (pid_count >= pid_max)
			break;
	}
	rcu_read_unlock();
	cur_ts2 = sched_clock();
	if (dmabuf_rb_check)
		dmabuf_dump(s, "%s: time:%lu max:%d count:%d\n", __func__,
			    cur_ts2 - cur_ts1, pid_max, pid_count);

	while (pid_count) {
		dmabuf_rbtree_add_all_pid(fddata, heap, s, pids[pid_count-1]);
		pid_count--;
	}

	kfree(pids);
	return fddata;
}

void dmabuf_rbtree_dump_stats(struct dump_fd_data *fd_data)
{
	unsigned long pss = 0, rss = 0;
	unsigned long krn_rss = 0;
	unsigned long total_rss = 0, total_pss = 0;
	struct rb_root *rbroot = &fd_data->dmabuf_root;
	struct seq_file *s = fd_data->constd.s;
	int i = 0;

	/* used for memtrack
	 *      file: aidl/default/Memtrack.cpp
	 *      function: getMemory_GRAPHICS
	 * make sure memtrack can get correct data from below PSS dump,
	 * please DO NOT change the format unilateral.
	 */
	dmabuf_dump(s, "PID     PSS(KB)   RSS(KB)\n");

	total_rss = 0;
	for (i = 0; i < TOTAL_PID_CNT; i++) {
		int pid = fd_data->pid_map[i].id;

		if (pid == 0)
			break;

		pss = dmabuf_rbtree_get_stats(rbroot, pid, STATS_PSS);
		rss = dmabuf_rbtree_get_stats(rbroot, pid, STATS_RSS);
		total_rss += rss;
		total_pss += pss;

		dmabuf_dump(s, "%-5d   %-7ld   %ld\n",
			    pid, pss/1024, rss/1024);
	}
	krn_rss = dmabuf_rbtree_get_stats(rbroot, 0, STATS_KRN_RSS);

	dmabuf_dump(s, "-----EGL memtrack data end\n");
	dmabuf_dump(s, "--sum: userspace_pss:%ld KB rss:%ld KB\n",
		    total_pss/1024, total_rss/1024);
	dmabuf_dump(s, "--sum: kernel rss: %ld KB\n\n", krn_rss/1024);

	/* dump pid map below for debugging more easier.*/
	dump_pid_map(fd_data);
}

static void dmabuf_rbtree_dump_buf(struct dump_fd_data *fddata, unsigned long flag)
{
	struct rb_node *tmp_rb;
	const struct dma_buf *dmabuf;
	struct dmabuf_debug_node *dbg_node;
	struct dmabuf_pid_res *pid_info;
	struct mtk_heap_dump_s dump_param;
	struct seq_file *s = fddata->constd.s;
	struct rb_root *rbroot = &fddata->dmabuf_root;

	memset(&dump_param, 0, sizeof(dump_param));

	dump_param.file = s;
	dump_param.flag = flag | HEAP_DUMP_DEC_1_REF;

	for (tmp_rb = rb_first(rbroot); tmp_rb; tmp_rb = rb_next(tmp_rb)) {
		struct list_head *pidnode = NULL;

		dbg_node = rb_entry(tmp_rb, struct dmabuf_debug_node, dmabuf_node);
		dmabuf = dbg_node->dmabuf;

		dma_heap_buf_dump_cb(dmabuf, &dump_param);

		list_for_each(pidnode, &dbg_node->pids_res) {
			struct dmabuf_fd_res *fd_info;
			struct dmabuf_vm_res *vm_info;
			struct list_head *fdnode;
			struct list_head *vmnode;
			char tpath[100];
			char *path_p = NULL;
			struct path base_path;
			char fd_list_str[100];
			int len = 0;

			memset(fd_list_str, 0, sizeof(fd_list_str));
			pid_info = list_entry(pidnode, struct dmabuf_pid_res, pid_res);

			if (pid_info->fd_cnt) {
				len = scnprintf(fd_list_str, 99, "fd_list:");
				list_for_each(fdnode, &pid_info->fds_head) {
					fd_info = list_entry(fdnode, struct dmabuf_fd_res, fd_res);
					len += scnprintf(fd_list_str + len, 99 - len,
							 "%d ", fd_info->fd_val);
				}
			}

			dmabuf_dump(s,
				    "\tpid:%d(%s) \tinode:%lu size:%-8zu fd_cnt:%d mmap_cnt:%d %s\n",
				    pid_info->pid,
				    pid_map_get_name(fddata, pid_info->pid),
				    pid_info->p->inode,
				    dmabuf->size,
				    pid_info->fd_cnt,
				    pid_info->vm_cnt,
				    fd_list_str);

			if (pid_info->vm_cnt && !vma_dump_enable)
				continue;

			list_for_each(vmnode, &pid_info->vms_head) {
				vm_info = list_entry(vmnode, struct dmabuf_vm_res, vm_res);

				base_path = pid_info->p->dmabuf->file->f_path;
				path_p = d_path(&base_path, tpath, 100);

				dmabuf_dump(s,
					    "\t\tva:0x%08lx-0x%08lx map_sz:%lu buf_sz:%lu node:%s\n",
					    vm_info->vm_start, vm_info->vm_start + vm_info->vm_size,
					    vm_info->vm_size,
					    dmabuf->size, path_p);
			}
		}
		dmabuf_dump(s, "\n");
	}
}

/* add 'noinline' to let it show in callstack */
static noinline
void dmabuf_rbtree_dump_all(struct dma_heap *heap, unsigned long flag,
			    struct seq_file *s, int pid)
{
	struct dump_fd_data *fddata;
	unsigned long long time1, time2, time3;
	unsigned long free_size = 0;

	debug_alloc_sz = 0;
	time1 = get_current_time_ms();

	fddata = dmabuf_rbtree_add_all(heap, s, pid);
	if (IS_ERR_OR_NULL(fddata)) {
		dmabuf_dump(s, "[%s]err: no memory\n", __func__);
		return;
	}

	time2 = get_current_time_ms();

	dmabuf_rbtree_dump_stats(fddata);
	if (!(flag & HEAP_DUMP_STATS))
		dmabuf_rbtree_dump_buf(fddata, flag);

	/* In case of log too much, Dump again after buffer dump */
	if (flag & HEAP_DUMP_OOM) {
		dmabuf_dump(s, "dump again after buffer dump\n");
		dmabuf_rbtree_dump_stats(fddata);
	}
	time3 = get_current_time_ms();
	free_size = dmabuf_dbg_rbtree_clear(fddata);

	if (!(flag & HEAP_DUMP_STATS)) {
		dmabuf_dump(s, "allocated for debug dump: %lu KB\n", debug_alloc_sz / 1024);
		dmabuf_dump(s, "freed     for debug dump: %lu KB\n", free_size / 1024);
		dmabuf_dump(s, "start:%lums add_done:%lums dump_done:%lums end:%lums\n",
			    time1, time2, time3, get_current_time_ms());
	}
}

/* add 'noinline' to let it show in callstack */
static noinline
void dma_heap_default_show(struct dma_heap *heap,
				  void *seq_file,
				  int flag)
{
	struct seq_file *s = seq_file;
	struct mtk_heap_dump_s dump_param;
	struct dma_heap_export_info *exp_info = (typeof(exp_info))heap;

	dump_param.heap = heap;
	dump_param.file = seq_file;
	dump_param.flag = flag;

	if (flag & HEAP_DUMP_HEAP_SKIP_POOL)
		goto pool_dump_done;

	dmabuf_dump(s, "[%s] buffer total size: %ld KB\n",
		    heap ? dma_heap_get_name(heap) : "all",
		    get_dma_heap_buffer_total(heap) * 4 / PAGE_SIZE);

	/* pool data show */
	if (exp_info && exp_info->ops && exp_info->ops->get_pool_size)
		dmabuf_dump(s, "--->%s_heap_pool size: %ld KB\n",
			    dma_heap_get_name(heap),
			    exp_info->ops->get_pool_size(heap)*4/PAGE_SIZE);
pool_dump_done:

	if (flag & HEAP_DUMP_SKIP_RB_DUMP)
		goto rb_dump_done;

	dmabuf_dump(s, "\n");
	dmabuf_rbtree_dump_all(heap, flag, s, -1);
rb_dump_done:

	return;
}

/* Goal: for different heap, show different info */
static void show_help_info(struct dma_heap *heap, struct seq_file *s)
{
	int i = 0;

	dmabuf_dump(s, "help info\n");

	dmabuf_dump(s, "\t|- How to use this debug file %s\n",
		    "https://wiki.mediatek.inc/pages/viewpage.action?pageId=881824213 ");

	dmabuf_dump(s, "\t|- Set debug name for dmabuf: %s\n",
		    "https://wiki.mediatek.inc/display/WSDOSS3ME18/How+to+set+dmabuf+debug+name ");

	dmabuf_dump(s, "debug cmd:\n");
	for (i = 0; i < ARRAY_SIZE(heap_helper); i++) {
		dmabuf_dump(s, "\t|- %s %s\n",
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
		long dmabuf_sz = 0;

		dump_param.heap = heap;
		dump_param.file = s;

		dmabuf_dump(s, "[All heap] dump start @%llu ms\n",
			    get_current_time_ms());

		show_help_info(heap, s);
		dmabuf_dump(s, "freelist: %lu KB\n", get_freelist_nr_pages() * 4);
		dmabuf_sz = get_dma_heap_buffer_total(heap);
		dmabuf_dump(s, "dmabuf buffer total:%ld KB\n", (dmabuf_sz * 4) / PAGE_SIZE);
		dmabuf_dump(s, "\t|-normal dma_heap buffer total:%ld KB\n\n",
			    (atomic64_read(&dma_heap_normal_total) * 4) / PAGE_SIZE);

		//dump all heaps
		dmabuf_dump(s, "[heap info]\n",
			    get_current_time_ms());
		for (; i < _DEBUG_HEAP_CNT_; i++) {
			heap = dma_heap_find(debug_heap_list[i].heap_name);
			if (heap) {
				dma_heap_default_show(heap, s, HEAP_DUMP_SKIP_RB_DUMP);
				dma_heap_put(heap);
				heap_sz += get_dma_heap_buffer_total(heap);
			}
		}
		dmabuf_dump(s, "\nnon-dma_heap buffer total:%ld KB\n",
			    (dmabuf_sz - heap_sz) * 4 / PAGE_SIZE);

		dma_heap_default_show(NULL, s, flag | HEAP_DUMP_HEAP_SKIP_POOL);
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

static void mtk_dmabuf_stats_show(struct seq_file *s, int flag)
{
	dmabuf_rbtree_dump_all(NULL, flag | HEAP_DUMP_STATS, s, -1);
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

static int heap_stats_proc_show(struct seq_file *s, void *v)
{
	if (!s)
		return -EINVAL;

	mtk_dmabuf_stats_show(s, 0);
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

static int heap_stats_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, heap_stats_proc_show, PDE_DATA(inode));
}

static ssize_t heap_stats_proc_write(struct file *file, const char *buf,
				     size_t count, loff_t *data)
{
	return count;
}

static int g_stat_pid;
static int heap_stat_pid_proc_show(struct seq_file *s, void *v)
{
	int pid = g_stat_pid;

	if (!s)
		return -EINVAL;

	if (pid > 0) {
		g_stat_pid = 0;
		dmabuf_rbtree_dump_all(NULL, HEAP_DUMP_STATS, s, pid);
	}

	return 0;
}


static int heap_stat_pid_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, heap_stat_pid_proc_show, PDE_DATA(inode));
}

static ssize_t heap_stat_pid_proc_write(struct file *file, const char *buf,
					size_t count, loff_t *data)
{
	int pid = 0;
	char line[SET_PID_CMDLINE_LEN];

	if (count <= (strlen("pid:\n")) || count >= SET_PID_CMDLINE_LEN)
		return -EINVAL;

	if (copy_from_user(line, buf, count))
		return -EFAULT;

	line[count] = '\0';

	if (!strstarts(line, "pid:"))
		return -EINVAL;

	if (sscanf(line, "pid:%u\n", &pid) != 1)
		return -EINVAL;

	if (pid < 0)
		return -EINVAL;

	g_stat_pid = pid;
	return count;
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

static const struct proc_ops heap_stats_proc_fops = {
	.proc_open = heap_stats_proc_open,
	.proc_read = seq_read,
	.proc_write = heap_stats_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops heap_stat_pid_proc_fops = {
	.proc_open = heap_stat_pid_proc_open,
	.proc_read = seq_read,
	.proc_write = heap_stat_pid_proc_write,
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
	kuid_t uid;
	kgid_t gid;

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
		pr_info("Failed to create debug file for all_heaps:%ld\n",
			PTR_ERR(proc_file));
		return -2;
	}
	pr_info("create debug file for all_heaps\n");


	dma_heaps_dir = proc_mkdir("heaps", dma_heap_proc_root);
	if (!dma_heaps_dir) {
		pr_info("%s failed to create procfs heaps dir.\n",
			__func__, PTR_ERR(dma_heaps_dir));
		return -1;
	}

	ret = dma_buf_init_dma_heaps_procfs();

	dma_heaps_stats = proc_create_data("stats",
					   S_IFREG | 0664,
					   dma_heap_proc_root,
					   &heap_stats_proc_fops,
					   NULL);
	if (!dma_heaps_stats) {
		pr_info("%s failed to create procfs stats dir:%ld\n",
			__func__, PTR_ERR(dma_heaps_stats));
		return -1;
	}
	pr_info("create debug file for stats\n");

	dma_heaps_stat_pid = proc_create_data("rss_pid",
					      S_IFREG | 0660,
					      dma_heap_proc_root,
					      &heap_stat_pid_proc_fops,
					      NULL);
	if (!dma_heaps_stat_pid) {
		pr_info("%s failed to create procfs rss_pid dir:%ld\n",
			__func__, PTR_ERR(dma_heaps_stat_pid));
		return -1;
	}
	pr_info("create debug file for stats_pid\n");

	/* set group of proc file rss_pid to system */
	uid = make_kuid(&init_user_ns, 0);
	gid = make_kgid(&init_user_ns, 1000);
	proc_set_user(dma_heaps_stat_pid, uid, gid);

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
	unsigned long long oom_time = get_current_time_ms();

	pr_info("%s in\n", __func__);

	if (oom_time - last_oom_time < OOM_DUMP_INTERVAL) {
		last_oom_time = oom_time;
		pr_info("%s: less than %dms since last dump, return\n",
			__func__, OOM_DUMP_INTERVAL);
		return 0;
	}

	mtk_dmabuf_dump_all(NULL, HEAP_DUMP_OOM);

	last_oom_time = oom_time;

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

	if (!oom_nb_status) {
		ret = unregister_oom_notifier(&dma_heap_oom_nb);
		if (ret)
			pr_info("unregister_oom_notifier failed:%d\n", ret);
	}

	dma_buf_uninit_procfs();

}
module_init(mtk_dma_heap_debug);
module_exit(mtk_dma_heap_debug_exit);
MODULE_LICENSE("GPL v2");
