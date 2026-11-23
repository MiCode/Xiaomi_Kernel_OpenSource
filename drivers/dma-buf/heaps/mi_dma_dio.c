#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/kernel_read_file.h>
#include <uapi/linux/dma-heap.h>

#include "mi_dma_dio.h"

#define DEVNAME "mi_dma_dio"
static dev_t dma_dio_devt;
static struct cdev dma_dio_cdev;
static struct class *dma_dio_class;

static struct dma_dio_file_control *dma_dio_fctl;
extern unsigned long debug_symbol_lookup_name(const char *name);
extern bool debug_symbol_available(void);
ssize_t (*kernel_read_file_symbol)(struct file *file, loff_t offset, void **buf,
				   size_t buf_size, size_t *file_size,
				   enum kernel_read_file_id id);

/**
 * struct dma_dio_allocation_data - metadata passed from userspace for allocations
 *
 * @fd:			direct io fd of the file to be read
 * @bfd:		buffer io fd of the file to be read. Specify when you need
 * 			to read to the end of a file and the file size is not aligned
 * @rfd:		will be populated with a fd which provides the
 *			handle to the allocated dma-buf
 * @offset:		read offset of the file
 * @size:		size of the allocation, read size will be less than or equal to it
 * @fd_flags:		file descriptor flags used when allocating
 * @heap_flags:		flags passed to heap
 *
 * Provided by userspace as an argument to the ioctl
 */
struct dma_dio_allocation_data {
	__u32 fd;
	__u32 bfd;
	__u32 rfd;
	__u64 offset;
	__u64 size;
	__u32 fd_flags;
	__u64 heap_flags;
};

#define DMA_DIO_ALLOC_IOC_MAGIC 'Z'
#define DMA_DIO_IOCTL_ALLOC	_IOWR(DMA_DIO_ALLOC_IOC_MAGIC, 0x0,\
				      struct dma_dio_allocation_data)

/**
 * struct dma_dio_file - wrap the file, read task for dma_heap allocate use.
 * @file:		direct io file to read from.
 * @bfile:		buffer io file to read from.
 * @cred:		kthread use, user cred copy to use for the read.
 * @glimit:		The size limit for gathering. Whenever the page of the
 *			gather reaches the limit, file I/O is triggered.
 *			This is the maximum limit for the current ALLOC_AND_READ
 *			operation.
 * @offset:		The offset of the file to be read.
 * @read_size:		The size of the file to be read.
 * @alloc_size:		The size of the allocation.
 */
struct dma_dio_file {
	struct file *file;
	struct file *bfile;
	struct cred *cred;
	size_t glimit;
	size_t offset;
	size_t read_size;
	size_t alloc_size;
};

/**
 * struct dma_dio_file_task - represents a dma_dio file read process
 * @ref:		current file work counter, if zero, allocate and read
 *			done.
 *
 * @roffset:		last relative read offset, current prepared work' begin file
 *			start offset.
 *
 * @rsize:		current allocated page size use to read, if reach rbatch,
 *			trigger commit.
 *
 * @nr_gathered:	current gathered page, Take the minimum value
 *			between the @glimit and the remaining allocation amount.
 *
 * @dio_file:		current dma_dio_file
 *
 * @parray:		used for vmap, size is @dma_dio_file's batch's number
 *			pages.(this is maximum). Due to single thread file read,
 *			one page array reuse in ftask prepare is OK.
 *			Each index in parray is PAGE_SIZE.(vmap need)
 *
 * @pindex:		current allocated page filled in @parray's index.
 *
 * @fail:		any work failed when file read?
 *
 * dma_dio_file_task is the production of file read, will prepare each work
 * during allocate dma_buf pages, if match current batch, then trigger commit
 * and prepare next work. After all batch queued, user going on prepare dma_buf
 * and so on, but before return dma_buf fd, need to wait file read end and
 * check read result.
 */
struct dma_dio_file_task {
	atomic_t ref;
	size_t roffset;
	size_t rsize;
	size_t nr_gathered;
	struct dma_dio_file *dio_file;
	struct page **parray;
	unsigned int pindex;
	bool fail;
};

/**
 * struct dma_dio_file_work - represents a dma_dio file read real work.
 * @vaddr:		contigous virtual address alloc by vmap, file read need.
 *
 * @start_size:		file read start offset, same to @dma_dio_file_task->roffset.
 *
 * @need_size:		file read need size, same to @dma_dio_file_task->rsize.
 *
 * @dio_file:		file wrapper.
 *
 * @list:		child node of @dma_dio_file_control->works.
 *
 * @refp:		same @dma_dio_file_task->ref, if end of read, put ref.
 *
 * @failp:		if any work io failed, set it true, pointp @dma_dio_file_task->fail.
 */
struct dma_dio_file_work {
	void *vaddr;
	ssize_t start_size;
	ssize_t need_size;
	struct dma_dio_file *dio_file;
	struct list_head list;
	atomic_t *refp;
	bool *failp;
};

/**
 * struct dma_dio_file_control - global control of dma_dio file read.
 * @works:		@dma_dio_file_work's list head.
 *
 * @threadwq:		wait queue for @work_thread, if commit work, @work_thread
 *			wakeup and read this work's file contains.
 *
 * @workwq:		used for main thread wait for file read end, if allocation
 *			end before file read. @dma_dio_file_task ref effect this.
 *
 * @work_thread:	file read kthread. the dma_dio_file_task work's consumer.
 *
 * @dio_fwork_cachep:	@dma_dio_file_work's cachep, it's alloc/free frequently.
 *
 * @nr_work:		global number of how many work committed.
 */
struct dma_dio_file_control {
	struct list_head works;
	spinlock_t lock; /* only lock for @works. */
	wait_queue_head_t threadwq;
	wait_queue_head_t workwq;
	struct task_struct *work_thread;
	struct kmem_cache *dio_fwork_cachep;
	atomic_t nr_work;
};

static bool search_dma_dio_symbol(void)
{
	unsigned long ret;

	if (!debug_symbol_available()) {
		kernel_read_file_symbol = NULL;
		pr_err("Kernel symbol is unavailable, dmabuf dio is not supported!\n");
		return false;
	}

	ret = debug_symbol_lookup_name("kernel_read_file");
	if (ret == 0) {
		kernel_read_file_symbol = NULL;
		pr_err("kernel_read_file is NULL, dmabuf dio is not supported!\n");
		return false;
	}

	kernel_read_file_symbol = (void*)ret;
	return true;
}

static struct dma_dio_file_work *init_file_work(struct dma_dio_file_task *dio_ftask)
{
	struct dma_dio_file_work *dio_fwork;
	struct dma_dio_file *dio_file = dio_ftask->dio_file;

	if (READ_ONCE(dio_ftask->fail))
		return NULL;

	dio_fwork = kmem_cache_alloc(dma_dio_fctl->dio_fwork_cachep, GFP_KERNEL);
	if (unlikely(!dio_fwork))
		return NULL;

	/**
	 * Map the gathered page to the vmalloc area.
	 * So we get a continuous virtual address, even if the physical address
	 * is scatter, can use this to trigger file read, if use direct I/O,
	 * all content can direct read into dma-buf pages without extra copy.
	 *
	 * Now that we get vaddr page, cached pages can return to original user, so we
	 * will not effect dma-buf export even if file read not end.
	 */
	dio_fwork->vaddr = vmap(dio_ftask->parray, dio_ftask->pindex, VM_MAP,
				PAGE_KERNEL);
	if (unlikely(!dio_fwork->vaddr)) {
		pr_err("%s: Failed to vmap\n", __func__);
		kmem_cache_free(dma_dio_fctl->dio_fwork_cachep, dio_fwork);
		return NULL;
	}

	dio_fwork->dio_file = dio_file;
	dio_fwork->start_size = dio_ftask->roffset + dio_file->offset;
	dio_fwork->need_size = dio_ftask->rsize;
	dio_fwork->refp = &dio_ftask->ref;
	dio_fwork->failp = &dio_ftask->fail;
	atomic_inc(&dio_ftask->ref);
	return dio_fwork;
}

static void deinit_file_work(struct dma_dio_file_work *dio_fwork)
{
	vunmap(dio_fwork->vaddr);
	atomic_dec(dio_fwork->refp);
	kmem_cache_free(dma_dio_fctl->dio_fwork_cachep, dio_fwork);
}

static int dma_dio_read_tail(struct dma_dio_file_task *dio_ftask)
{
	struct dma_dio_file *dio_file = dio_ftask->dio_file;
	struct file *bfile = dio_file->bfile;
	size_t size = dio_file->read_size;
	size_t start = dio_ftask->roffset;
	struct page *last = NULL;
	size_t file_size, ret;
	void *buffer;

	if (!bfile)
		return -EINVAL;

	start = PAGE_ALIGN_DOWN(size) + dio_file->offset;

	last = dio_ftask->parray[dio_ftask->pindex - 1];
	/* use page's kaddr as file read buffer. */
	buffer = kmap_local_page(last);
	ret = (*kernel_read_file_symbol)(bfile, start, &buffer, size - PAGE_ALIGN_DOWN(size),
					 &file_size, READING_POLICY);
	kunmap_local(buffer);

	return ret < 0 ? ret : 0;
}

/**
 * dma_dio_submit_file_read -  prepare collect enough memory, going to trigger IO
 * @dio_ftask:			info that current IO needs
 *
 * This will also check if reach to tail read.
 * For direct I/O submissions, it is necessary to pay attention to file reads
 * that are not page-aligned. For the unaligned portion of the read, buffer IO
 * needs to be triggered.
 * Returns:
 *   0 if all right, negative if something wrong
 */
static int dma_dio_submit_file_read(struct dma_dio_file_task *dio_ftask)
{
	struct dma_dio_file_work *dio_fwork = init_file_work(dio_ftask);
	struct dma_dio_file *dio_file = dio_ftask->dio_file;
	size_t size = dio_file->read_size;
	int ret;

	if (unlikely(!dio_fwork))
		return -ENOMEM;

	/* Note: roffset and rsize are page-aligned. */
	if (dio_ftask->roffset + dio_ftask->rsize <= size)
		goto add_work;

	/**
	 * If file size is not page aligned, direct io can't process the tail.
	 * So, if reach to tail, remain the last page use buffer read.
	 */
	dio_fwork->need_size -= PAGE_SIZE;
	ret = dma_dio_read_tail(dio_ftask);
	if (ret) {
		pr_err("%s: Failed to read tail: %d\n", __func__, ret);
		deinit_file_work(dio_fwork);
		return ret;
	}

add_work:
	spin_lock(&dma_dio_fctl->lock);
	list_add_tail(&dio_fwork->list, &dma_dio_fctl->works);
	spin_unlock(&dma_dio_fctl->lock);
	atomic_inc(&dma_dio_fctl->nr_work);

	wake_up(&dma_dio_fctl->threadwq);

	dio_ftask->roffset += dio_ftask->rsize;
	dio_ftask->rsize = 0;
	dio_ftask->pindex = 0;
	dio_ftask->nr_gathered = min_t(size_t, PAGE_ALIGN(size) - dio_ftask->roffset,
				       dio_ftask->nr_gathered);
	return 0;
}

static int dma_dio_gather_file_page(struct dma_dio_file_task *dio_ftask, struct page *page)
{
	struct page **array = dio_ftask->parray;
	int num = compound_nr(page), i;
	int ret;

	/* Nothing to read, skip. */
	if (dio_ftask->nr_gathered == 0)
		return 0;

	for (i = 0; i < num; ++i) {
		dio_ftask->rsize += PAGE_SIZE;
		array[dio_ftask->pindex++] = &page[i];
		/* already reach to limit, trigger file read. */
		if (dio_ftask->rsize >= dio_ftask->nr_gathered) {
			ret = dma_dio_submit_file_read(dio_ftask);
			if (ret)
				return ret;
			if (dio_ftask->nr_gathered == 0)
				break;
		}
	}

	return 0;
}

static int dma_dio_wait_for_file_read(struct dma_dio_file_task *dio_ftask)
{
	wait_event_freezable(dma_dio_fctl->workwq,
			     atomic_read(&dio_ftask->ref) == 0);
	return dio_ftask->fail ? -EIO : 0;
}

static int dma_dio_end_file_read(struct dma_dio_file_task *dio_ftask)
{
	int ret;

	ret = dma_dio_wait_for_file_read(dio_ftask);
	kvfree(dio_ftask->parray);
	kfree(dio_ftask);

	return ret;
}

static struct dma_dio_file_task *dma_dio_declare_file_read(struct dma_dio_file *dio_file)
{
	struct dma_dio_file_task *dio_ftask =
		kzalloc(sizeof(*dio_ftask), GFP_KERNEL);

	if (unlikely(!dio_ftask))
		return NULL;

	/**
	 * glimit is the maximum size which we prepare work will meet.
	 * So, direct alloc this number's page array is OK.
	 */
	dio_ftask->parray = kvmalloc_array(dio_file->glimit >> PAGE_SHIFT,
					   sizeof(struct page *), GFP_KERNEL);
	if (unlikely(!dio_ftask->parray)) {
		pr_err("%s: Failed to kvmalloc array\n", __func__);
		goto put;
	}

	dio_ftask->dio_file = dio_file;
	dio_ftask->nr_gathered = dio_file->glimit;
	return dio_ftask;

put:
	kfree(dio_ftask);
	return NULL;
}

static void __work_this_io(struct dma_dio_file_work *dio_fwork)
{
	struct dma_dio_file *dio_file = dio_fwork->dio_file;
	struct file *file = dio_file->file;
	ssize_t start = dio_fwork->start_size;
	ssize_t size = dio_fwork->need_size;
	void *buffer = dio_fwork->vaddr;
	const struct cred *old_cred;
	ssize_t err;
	size_t file_size;

	/* use real task's cred to read this file. */
	old_cred = override_creds(dio_file->cred);
	err = (*kernel_read_file_symbol)(file, start, &buffer, size, &file_size,
					 READING_POLICY);
	if (err < 0) {
		WRITE_ONCE(*dio_fwork->failp, true);
		pr_err("%s: Failed to read file, start: %zd, size: %zd, ret: %zd\n",
		       __func__, start, size, err);
	}
	revert_creds(old_cred);
}

static int dma_dio_file_work_thread(void *data)
{
	struct dma_dio_file_control *dma_dio_fctl =
		(struct dma_dio_file_control *)data;
	struct dma_dio_file_work *worker, *tmp;
	int nr_work;

	LIST_HEAD(pages);
	LIST_HEAD(workers);

	while (true) {
		wait_event_freezable(dma_dio_fctl->threadwq,
				     atomic_read(&dma_dio_fctl->nr_work) > 0);
recheck:
		spin_lock(&dma_dio_fctl->lock);
		list_splice_init(&dma_dio_fctl->works, &workers);
		spin_unlock(&dma_dio_fctl->lock);

		if (unlikely(kthread_should_stop())) {
			list_for_each_entry_safe(worker, tmp, &workers, list) {
				list_del(&worker->list);
				deinit_file_work(worker);
				wake_up(&dma_dio_fctl->workwq);
			}
			break;
		}

		nr_work = 0;
		list_for_each_entry_safe(worker, tmp, &workers, list) {
			++nr_work;
			list_del(&worker->list);
			__work_this_io(worker);

			deinit_file_work(worker);
			wake_up(&dma_dio_fctl->workwq);
		}

		if (atomic_sub_return(nr_work, &dma_dio_fctl->nr_work) > 0)
			goto recheck;
	}
	return 0;
}

static size_t dma_dio_alloc_size(struct dma_dio_file *dio_file)
{
	return dio_file->alloc_size;
}

#define DEFAULT_DMA_BUF_DIO_GATHER_LIMIT (128 << 20)
static int dma_buf_dio_gather_limit = DEFAULT_DMA_BUF_DIO_GATHER_LIMIT;
module_param_named(gather_limit, dma_buf_dio_gather_limit, int, 0644);
MODULE_PARM_DESC(gather_limit, "Asynchronous file reading, with a maximum limit on the amount to be gathered");

static int init_dma_dio_file(struct dma_dio_file *dio_file, u32 fd, u32 bfd,
			     size_t offset, size_t size)
{
	struct file *file = NULL, *bfile = NULL;
	size_t fsz, len;
	int ret;

	if (!PAGE_ALIGNED(offset) || size == 0)
		return -EINVAL;

	file = fget(fd);
	if (unlikely(!file))
		return -EINVAL;
	if (!(file->f_flags & O_DIRECT)) {
		ret = -EINVAL;
		goto err;
	}

	fsz = i_size_read(file_inode(file));
	if (offset >= fsz) {
		ret = -EINVAL;
		goto err;
	}

	/* If reach the tail, do not page-align size. */
	len = min_t(size_t, fsz - offset, PAGE_ALIGN(size));

	/*
	 * If reach the tail and the read length is not aligned,
	 * it needs to be read through buffer IO.
	 */
	if (!PAGE_ALIGNED(len)) {
		bfile = fget(bfd);
		if (unlikely(!bfile)) {
			ret = -EINVAL;
			goto err;
		}
		if (bfile->f_flags & O_DIRECT) {
			ret = -EINVAL;
			goto err_buffer;
		}
	}

	/**
	 * Selinux block our read, but actually we are reading the stand-in
	 * for this file.
	 * So save current's cred and when going to read, override mine, and
	 * end of read, revert.
	 */
	dio_file->cred = prepare_kernel_cred(current);
	if (unlikely(!dio_file->cred)) {
		pr_err("%s: Failed to init prepare kernel cred\n", __func__);
		ret = -ENOMEM;
		goto err_buffer;
	}

	dio_file->file = file;
	dio_file->bfile = bfile;
	dio_file->glimit = min_t(size_t, PAGE_ALIGN(len), PAGE_ALIGN(dma_buf_dio_gather_limit));
	dio_file->offset = offset;
	dio_file->read_size = len;
	dio_file->alloc_size = size;

	return 0;

err_buffer:
	if (bfile)
		fput(bfile);
err:
	fput(file);
	return ret;
}

static void deinit_dma_dio_file(struct dma_dio_file *dio_file)
{
	fput(dio_file->file);
	if (dio_file->bfile)
		fput(dio_file->bfile);
	put_cred(dio_file->cred);
}

static int init_dma_dio_fctl(void)
{
	int ret;

	dma_dio_fctl = kzalloc(sizeof(*dma_dio_fctl), GFP_KERNEL);
	if (unlikely(!dma_dio_fctl))
		return -ENOMEM;

	INIT_LIST_HEAD(&dma_dio_fctl->works);
	init_waitqueue_head(&dma_dio_fctl->threadwq);
	init_waitqueue_head(&dma_dio_fctl->workwq);

	dma_dio_fctl->work_thread = kthread_run(dma_dio_file_work_thread,
						dma_dio_fctl, "dma_dio_fwork_t");
	if (IS_ERR(dma_dio_fctl->work_thread)) {
		ret = PTR_ERR(dma_dio_fctl->work_thread);
		goto fail_thread;
	}

	dma_dio_fctl->dio_fwork_cachep = KMEM_CACHE(dma_dio_file_work, 0);
	if (unlikely(!dma_dio_fctl->dio_fwork_cachep)) {
		ret = -ENOMEM;
		goto fail_cache;
	}

	return 0;

fail_cache:
	kthread_stop(dma_dio_fctl->work_thread);
fail_thread:
	kfree(dma_dio_fctl);
	pr_err("%s: Failed to init dma_dio_fctl: %d\n", __func__, ret);
	return ret;
}

static int dma_dio_open(struct inode *inode, struct file *file)
{
	struct dma_heap *heap;

	heap = dma_heap_find("qcom,system");
	if (!heap) {
		pr_err("%s: Failed to find qcom,system heap\n", __func__);
		return -ENODEV;
	}

	/* instance data as context */
	file->private_data = heap;
	nonseekable_open(inode, file);

	return 0;
}

static int dma_dio_release(struct inode *inode, struct file *file)
{
	struct dma_heap *heap = file->private_data;

	dma_heap_put(heap);
	return 0;
}

static struct dma_dio_operations dio_ops = {
	.dma_dio_declare_file_read = dma_dio_declare_file_read,
	.dma_dio_gather_file_page = dma_dio_gather_file_page,
	.dma_dio_end_file_read = dma_dio_end_file_read,
	.dma_dio_alloc_size = dma_dio_alloc_size,
};

extern struct dma_buf *
system_heap_allocate_async_read(struct dma_dio_operations *ops, struct dma_heap *heap,
				struct dma_dio_file *dio_file, u32 fd_flags, u64 heap_flags);
static struct dma_buf *dma_dio_buffer_alloc(struct dma_heap *heap, u32 fd, u32 bfd,
					    size_t offset, size_t size, u32 fd_flags,
					    u64 heap_flags)
{
	struct dma_dio_file dio_file;
	struct dma_buf *dma_buf;
	int ret;

	if (fd_flags & ~DMA_HEAP_VALID_FD_FLAGS)
		return ERR_PTR(-EINVAL);

	if (heap_flags & ~DMA_HEAP_VALID_HEAP_FLAGS)
		return ERR_PTR(-EINVAL);

	ret = init_dma_dio_file(&dio_file, fd, bfd, offset, size);
	if (ret) {
		pr_err("%s: Failed to init dma heap file: %d\n", __func__, ret);
		return ERR_PTR(ret);
	}

	dma_buf = system_heap_allocate_async_read(&dio_ops, heap, &dio_file,
						  fd_flags, heap_flags);
	deinit_dma_dio_file(&dio_file);

	return dma_buf;
}

static int dma_dio_bufferfd_alloc(struct dma_heap *heap, u32 fd, u32 bfd, size_t offset,
				  size_t size, u32 fd_flags, u64 heap_flags)
{
	struct dma_buf *dmabuf;
	int rfd;

	dmabuf = dma_dio_buffer_alloc(heap, fd, bfd, offset, size, fd_flags, heap_flags);

	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	rfd = dma_buf_fd(dmabuf, fd_flags);
	if (rfd < 0) {
		dma_buf_put(dmabuf);
		/* just return, as put will call release and that will free */
	}

	return rfd;
}

static long dma_dio_ioctl_allocate(struct file *file, void *data)
{
	struct dma_dio_allocation_data *dio_allocation = data;
	struct dma_heap *heap = file->private_data;
	int rfd;

	if (dio_allocation->rfd)
		return -EINVAL;

	rfd = dma_dio_bufferfd_alloc(heap, dio_allocation->fd, dio_allocation->bfd,
				     dio_allocation->offset, dio_allocation->size,
				     dio_allocation->fd_flags, dio_allocation->heap_flags);
	if (rfd < 0)
		return rfd;

	dio_allocation->rfd = rfd;

	return 0;
}

static unsigned int dma_dio_ioctl_cmds[] = {
	DMA_DIO_IOCTL_ALLOC,
};

static long dma_dio_ioctl(struct file *file, unsigned int ucmd, unsigned long arg)
{
	char stack_kdata[128];
	char *kdata = stack_kdata;
	unsigned int kcmd;
	unsigned int in_size, out_size, drv_size, ksize;
	int nr = _IOC_NR(ucmd);
	int ret = 0;

	if (nr >= ARRAY_SIZE(dma_dio_ioctl_cmds))
		return -EINVAL;

	nr = array_index_nospec(nr, ARRAY_SIZE(dma_dio_ioctl_cmds));
	/* Get the kernel ioctl cmd that matches */
	kcmd = dma_dio_ioctl_cmds[nr];

	/* Figure out the delta between user cmd size and kernel cmd size */
	drv_size = _IOC_SIZE(kcmd);
	out_size = _IOC_SIZE(ucmd);
	in_size = out_size;
	if ((ucmd & kcmd & IOC_IN) == 0)
		in_size = 0;
	if ((ucmd & kcmd & IOC_OUT) == 0)
		out_size = 0;
	ksize = max(max(in_size, out_size), drv_size);

	/* If necessary, allocate buffer for ioctl argument */
	if (ksize > sizeof(stack_kdata)) {
		kdata = kmalloc(ksize, GFP_KERNEL);
		if (!kdata)
			return -ENOMEM;
	}

	if (copy_from_user(kdata, (void __user *)arg, in_size) != 0) {
		ret = -EFAULT;
		goto err;
	}

	/* zero out any difference between the kernel/user structure size */
	if (ksize > in_size)
		memset(kdata + in_size, 0, ksize - in_size);

	switch (kcmd) {
	case DMA_DIO_IOCTL_ALLOC:
		ret = dma_dio_ioctl_allocate(file, kdata);
		break;
	default:
		ret = -ENOTTY;
		goto err;
	}

	if (copy_to_user((void __user *)arg, kdata, out_size) != 0)
		ret = -EFAULT;
err:
	if (kdata != stack_kdata)
		kfree(kdata);
	return ret;
}

static const struct file_operations dma_dio_fops = {
	.owner	 	= THIS_MODULE,
	.open		= dma_dio_open,
	.unlocked_ioctl = dma_dio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= dma_dio_ioctl,
#endif
	.release	= dma_dio_release,
};

static int __init dma_dio_init(void)
{
	int ret;
	struct device *dev;

	if (!search_dma_dio_symbol())
		return -ENOTSUPP;

	ret = alloc_chrdev_region(&dma_dio_devt, 0, 1, DEVNAME);
	if (ret)
		return ret;

	dma_dio_class = class_create(DEVNAME);
	if (IS_ERR(dma_dio_class)) {
		ret = PTR_ERR(dma_dio_class);
		goto err_class;
	}

	cdev_init(&dma_dio_cdev, &dma_dio_fops);
	ret = cdev_add(&dma_dio_cdev, dma_dio_devt, 1);
	if (ret < 0) {
		pr_err("%s: Failed to to add char device: %d\n", __func__, ret);
		goto err_add;
	}

	dev = device_create(dma_dio_class, NULL, dma_dio_devt, NULL, DEVNAME);
	if (IS_ERR(dev)) {
		pr_err("%s: Failed to to create device: %d\n", __func__, ret);
		ret = PTR_ERR(dev);
		goto err_create;
	}

	ret = init_dma_dio_fctl();
	if (ret) {
		device_destroy(dma_dio_class, MKDEV(MAJOR(dma_dio_devt), 0));
		pr_err("%s: Failed to to init dam_dio_fctl: %d\n", __func__, ret);
		goto err_create;
	}

	return 0;

err_create:
	cdev_del(&dma_dio_cdev);
err_add:
	class_destroy(dma_dio_class);
err_class:
	unregister_chrdev_region(dma_dio_devt, 1);
	return ret;
}
module_init(dma_dio_init);

static void __exit dma_dio_exit(void)
{
	device_destroy(dma_dio_class, MKDEV(MAJOR(dma_dio_devt), 0));
	cdev_del(&dma_dio_cdev);
	class_destroy(dma_dio_class);
	unregister_chrdev_region(dma_dio_devt, 1);
}
module_exit(dma_dio_exit);

MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);