// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>
#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/stackdepot.h>
#include <linux/stacktrace.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#define DMA_BUF_STACK_DEPTH (16)

struct dma_buf_list {
	struct list_head head;
	struct mutex lock;
};

static struct dma_buf_list msm_db_list;

/*
 * msm_dma_buf is a struct dma_buf extention.
 *
 * The main usage of this is to store and export call stack of
 * allocation of dma_buf, which can be helpful for debugging
 * dma_buf memleak.
 *
 * Param:
 *       dma_buf: pointer to struct dma_buf node
 *       refs: head of call trace list
 *       list_node: msm_dma_buf itself should be added to msm_db_list
*/

struct msm_dma_buf {
	struct dma_buf *dma_buf;
	struct list_head refs;
	struct list_head list_node;
};

struct dma_buf_ref {
	struct list_head list;
	depot_stack_handle_t handle;
	int count;
};

void dma_buf_ref_init(struct msm_dma_buf *msm_dma_buf)
{
	INIT_LIST_HEAD(&msm_dma_buf->refs);
}

void dma_buf_ref_destroy(struct msm_dma_buf *msm_dma_buf)
{
	struct dma_buf_ref *r, *n;
	struct dma_buf *dmabuf = msm_dma_buf->dma_buf;

	mutex_lock(&dmabuf->lock);
	list_for_each_entry_safe(r, n, &msm_dma_buf->refs, list) {
		list_del(&r->list);
		kfree(r);
	}
	mutex_unlock(&dmabuf->lock);
}

static void dma_buf_ref_insert_handle(struct msm_dma_buf *msm_dma_buf,
				      depot_stack_handle_t handle,
				      int count)
{
	struct dma_buf_ref *r;
	struct dma_buf *dmabuf = msm_dma_buf->dma_buf;

	mutex_lock(&dmabuf->lock);
	list_for_each_entry(r, &msm_dma_buf->refs, list) {
		if (r->handle == handle) {
			r->count += count;
			goto out;
		}
	}

	r = kzalloc(sizeof(*r), GFP_KERNEL);
	if (!r)
		goto out;

	INIT_LIST_HEAD(&r->list);
	r->handle = handle;
	r->count = count;
	list_add(&r->list, &msm_dma_buf->refs);

out:
	mutex_unlock(&dmabuf->lock);
}

void dma_buf_ref_mod(struct msm_dma_buf *msm_dma_buf, int nr)
{
	unsigned long entries[DMA_BUF_STACK_DEPTH];
	int nr_entries;
	depot_stack_handle_t handle;

	nr_entries = stack_trace_save(entries, ARRAY_SIZE(entries), 1);

	if (nr_entries != 0 && entries[nr_entries - 1] == ULONG_MAX)
		nr_entries--;

	handle = stack_depot_save(entries, nr_entries, GFP_KERNEL);
	if (!handle)
		return;

	dma_buf_ref_insert_handle(msm_dma_buf, handle, nr);
}
EXPORT_SYMBOL(dma_buf_ref_mod);

/**
 * Called with dmabuf->lock held
 */
int dma_buf_ref_show(struct seq_file *s, struct msm_dma_buf *msm_dma_buf)
{
	char *buf;
	struct dma_buf_ref *ref;
	int count = 0;

	buf = (void *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	list_for_each_entry(ref, &msm_dma_buf->refs, list) {
		unsigned long *entries;
		int nr_entries;

		count += ref->count;

		seq_printf(s, "References: %d\n", ref->count);
		nr_entries = stack_depot_fetch(ref->handle, &entries);

		stack_trace_snprint(buf, PAGE_SIZE, entries, nr_entries, 0);
		seq_puts(s, buf);
		seq_putc(s, '\n');
	}

	seq_printf(s, "Total references: %d\n\n\n", count);
	free_page((unsigned long)buf);

	return 0;
}

struct msm_dma_buf *msm_dma_buf_find(struct dma_buf *dmabuf)
{
	struct msm_dma_buf *mbuf_obj, *found = NULL;

	mutex_lock(&msm_db_list.lock);
	list_for_each_entry(mbuf_obj, &msm_db_list.head, list_node) {
		if(mbuf_obj->dma_buf == dmabuf) {
			found = mbuf_obj;
			break;
		}
	}
	mutex_unlock(&msm_db_list.lock);

	return found;
}

struct msm_dma_buf *msm_dma_buf_create(struct dma_buf *dma_buf)
{
	struct msm_dma_buf *m_dmabuf;
	size_t alloc_size = sizeof(struct msm_dma_buf);

	m_dmabuf = kzalloc(alloc_size, GFP_KERNEL);
	if (!m_dmabuf) {
		return ERR_PTR(-ENOMEM);
	}

	m_dmabuf->dma_buf = dma_buf;
	dma_buf_ref_init(m_dmabuf);

	mutex_lock(&msm_db_list.lock);
	list_add(&m_dmabuf->list_node, &msm_db_list.head);
	mutex_unlock(&msm_db_list.lock);

	return m_dmabuf;
}
EXPORT_SYMBOL(msm_dma_buf_create);

void msm_dma_buf_destroy(struct dma_buf *dma_buf)
{
	struct msm_dma_buf *m_dmabuf;

	if(dma_buf == NULL)
		return;

	m_dmabuf = msm_dma_buf_find(dma_buf);
	if(m_dmabuf == NULL)
		return;

	dma_buf_ref_destroy(m_dmabuf);

	mutex_lock(&msm_db_list.lock);
	list_del(&m_dmabuf->list_node);
	mutex_unlock(&msm_db_list.lock);

	kfree(m_dmabuf);
}
EXPORT_SYMBOL(msm_dma_buf_destroy);

int dma_buf_refs_show(struct seq_file *s, void *v)
{
	struct msm_dma_buf *mbuf_obj;
	struct dma_buf *buf_obj;
	struct dma_buf_attachment *attach_obj;
	int count = 0, attach_count;
	size_t size = 0;

	mutex_lock_interruptible(&msm_db_list.lock);

	seq_puts(s, "Dma-buf Objects:\n\n");

	list_for_each_entry(mbuf_obj, &msm_db_list.head, list_node) {
		seq_printf(s, "%-8s\t%-8s\t%-8s\t%-8s\t%-12s\t%-8s\n",
				"size", "flags", "mode", "count", "exp_name", "inode");
		buf_obj = mbuf_obj->dma_buf;

		mutex_lock_interruptible(&buf_obj->lock);

		seq_printf(s, "%08zu\t%08x\t%08x\t%08ld\t%-12s\t%-8lu\n",
				buf_obj->size,
				buf_obj->file->f_flags, buf_obj->file->f_mode,
				file_count(buf_obj->file),
				buf_obj->exp_name, file_inode(buf_obj->file)->i_ino);

		seq_puts(s, "Attached Devices:\n");
		attach_count = 0;

		list_for_each_entry(attach_obj, &buf_obj->attachments, node) {
			seq_printf(s, "\t%s\n", dev_name(attach_obj->dev));
			attach_count++;
		}

		seq_printf(s, "Total %d devices attached\n\n",
				attach_count);

		dma_buf_ref_show(s, mbuf_obj);

		count++;
		size += buf_obj->size;

		mutex_unlock(&buf_obj->lock);
	}
	seq_printf(s, "\nTotal %d objects, %zu bytes\n", count, size);
	mutex_unlock(&msm_db_list.lock);

	return 0;
}

static int proc_create_dma_buf_ref()
{
	struct proc_dir_entry *entry_dbg = NULL;
	int ret = -ENOMEM;

	/* create /proc/dma_buf_dbg dir */
	entry_dbg = proc_mkdir("dma_buf_dbg", NULL);
	if (entry_dbg == NULL) {
		pr_err("%s: Can't create dma_buf debug proc entry\n", __func__);
		return ret;
	}
	proc_create_single("dma_buf_refs", 0, entry_dbg, dma_buf_refs_show);

	return 0;
}

static int __init dmabuf_ref_init(void)
{
	mutex_init(&msm_db_list.lock);
	INIT_LIST_HEAD(&msm_db_list.head);

	return proc_create_dma_buf_ref();
}
module_init(dmabuf_ref_init);


MODULE_DESCRIPTION("dma_buf_ref_debug");
MODULE_LICENSE("GPL v2");
