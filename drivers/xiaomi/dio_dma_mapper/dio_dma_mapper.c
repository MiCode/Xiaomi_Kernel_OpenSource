#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/dma-buf.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/bvec.h>
#include <linux/uio.h>
#include <linux/file.h>
#include <linux/scatterlist.h>

#define IOCTL_ATTACH_DMABUF _IOW('d', 1, int)
//means write to file,DMA_TO_DEVICE
#define IOCTL_DIRECT_IO_READ_FROM_DMABUF _IOW('d', 3, struct dio_dmabuf_operate_args)
//means read from file,DMA_FROM_DEVICE
#define IOCTL_DIRECT_IO_WRITE_TO_DMABUF _IOW('d', 4, struct dio_dmabuf_operate_args)

#define DIO_VERSION_STRING "2"
#define MAX_IO_SIZE (128 * 1024 * 1024UL)  // 128MB limit
#define MAX_BVEC_BATCH  128

struct dio_dmabuf_operate_args {
	int dio_fd;
	int dmabuf_fd;
	loff_t dio_offset;
	loff_t dmabuf_offset;
	size_t length;
};

struct dio_dma_data {
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	struct page **pages;
	struct dma_buf *dmabuf;
	unsigned long pagecount;
	enum dma_data_direction dma_dir;
};

static struct miscdevice dma_buf_misc_device;
static int sg_to_page_addr_arrays(struct sg_table *sgt, struct page **pages,
			   int max_entries)
{
	struct sg_page_iter page_iter;
	struct page **p = pages;
	if (pages) {
		for_each_sgtable_page (sgt, &page_iter, 0) {
			if (p - pages >= max_entries) {
				pr_err("sg page count exceeds max_entries\n");
				return -ENOMEM;
			}
			*p++ = sg_page_iter_page(&page_iter);
		}
	}
	return 0;
}

static void release_dio_dma_data(struct dio_dma_data *data)
{
	if (!data)
		return;

	if (data->dmabuf) {
		if (data->attachment && data->sgt)
			dma_buf_unmap_attachment(data->attachment, data->sgt, data->dma_dir);
		if (data->attachment)
			dma_buf_detach(data->dmabuf, data->attachment);
		dma_buf_put(data->dmabuf);
	}

	data->attachment = NULL;
	data->sgt = NULL;
	if (data->pages) {
		kvfree(data->pages);
		data->pages = NULL;
	}
	data->dmabuf = NULL;
	data->pagecount = 0;
}
static int dio_dma_mapper_attach_dma_fd(struct dio_dma_data *data, int fd,
					 enum dma_data_direction dir)
{
	struct dma_buf *dmabuf;
	struct sg_table *sgt;
	struct dma_buf_attachment *attachment;
	struct page **pages;
	int npages;
	int ret;

	/* Release existing attachment if present */
	if (data->dmabuf) {
		release_dio_dma_data(data);
	}

	/* Get the DMA-BUF object */
	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf))
		goto dma_buf_err;

	/* Attach to the DMA-BUF */
	attachment = dma_buf_attach(dmabuf, dma_buf_misc_device.this_device);
	if (IS_ERR(attachment))
		goto attach_err;

	/* Map the buffer to scatter-gather table */
	sgt = dma_buf_map_attachment(attachment, dir);
	if (IS_ERR(sgt))
		goto map_err;

	/* Pin pages and get the number of pages */
	npages = PAGE_ALIGN(dmabuf->size) / PAGE_SIZE;
	if (npages == 0) {
		pr_err("dmabuf size is zero\n");
		goto pages_err;
	}

	/* Allocate pages array - use kvmalloc for better performance */
	pages = kvmalloc(sizeof(struct page *) * npages, GFP_KERNEL);
	if (!pages)
		goto pages_err;

	/* Populate pages array from SG table */
	ret = sg_to_page_addr_arrays(sgt, pages, npages);
	if (ret)
		goto sg_err;

	data->dmabuf = dmabuf;
	data->attachment = attachment;
	data->sgt = sgt;
	data->pagecount = npages;
	data->pages = pages;
	data->dma_dir = dir;
	return 0;

sg_err:
	pr_err("Failed to convert sg to page array\n");
	kvfree(pages);
pages_err:
	pr_err("Failed to allocate pages array\n");
	if (sgt && !IS_ERR(sgt))
		dma_buf_unmap_attachment(attachment, sgt, dir);
map_err:
	pr_err("Failed to map attachment\n");
	dma_buf_detach(dmabuf, attachment);
attach_err:
	pr_err("Failed to attach to dma_buf\n");
	dma_buf_put(dmabuf);
	return -ENOMEM;
dma_buf_err:
	pr_err("not an available dma buf\n");
	return -EINVAL;
}

static int dio_dma_mapper_io(struct dio_dma_data *data, struct dio_dmabuf_operate_args *args,
					 enum dma_data_direction dir)
{
	struct file *file = NULL;
	struct scatterlist *sg = NULL;
	struct sg_table *sgt = NULL;
	struct bio_vec *bvec;
	struct iov_iter iter;
	struct dma_buf *dmabuf = NULL;
	struct dma_buf_attachment *attachment = NULL;
	int i, bvec_count = 0;
	int ret = 0;
	size_t total_length = args->length;
	size_t accumulated_len = 0;
	loff_t mem_offset = args->dmabuf_offset;
	loff_t file_offset = args->dio_offset;
	size_t total_transferred = 0;
	int file_read_write = (dir == DMA_FROM_DEVICE) ? 0 : 1;

	/* Validate input parameters */
	if (args->dmabuf_fd < 0) {
		pr_err("Invalid dmabuf_fd: %d\n", args->dmabuf_fd);
		return -EINVAL;
	}
	if (args->dio_fd < 0) {
		pr_err("Invalid dio_fd: %d\n", args->dio_fd);
		return -EINVAL;
	}
	if (args->length == 0) {
		pr_err("Invalid length: 0\n");
		return -EINVAL;
	}

	/* Get the DMA-BUF object */
	dmabuf = dma_buf_get(args->dmabuf_fd);
	if (IS_ERR(dmabuf)) {
		pr_err("Failed to get dma_buf from fd=%d\n", args->dmabuf_fd);
		return PTR_ERR(dmabuf);
	}

	/* Attach to the DMA-BUF */
	attachment = dma_buf_attach(dmabuf, dma_buf_misc_device.this_device);
	if (IS_ERR(attachment)) {
		pr_err("Failed to attach to dma_buf\n");
		dma_buf_put(dmabuf);
		return PTR_ERR(attachment);
	}

	/* Map the buffer to scatter-gather table */
	sgt = dma_buf_map_attachment(attachment, dir);
	if (IS_ERR(sgt)) {
		pr_err("Failed to map attachment\n");
		dma_buf_detach(dmabuf, attachment);
		dma_buf_put(dmabuf);
		return PTR_ERR(sgt);
	}

	if (!sgt || !sgt->sgl) {
		pr_err("sg_table is not initialized\n");
		ret = -EINVAL;
		goto out;
	}

	/* Validate dmabuf offset and length against buffer size */
	if (args->dmabuf_offset < 0 ||
	    (size_t)args->dmabuf_offset + args->length > dmabuf->size) {
		pr_err("dmabuf offset+length (%llu+%zu) exceeds buffer size (%zu)\n",
		       (unsigned long long)args->dmabuf_offset, args->length,
		       (size_t)dmabuf->size);
		ret = -EINVAL;
		goto out;
	}

	file = fget(args->dio_fd);
	if (!file) {
		pr_err("dio fd error! fd=%d\n", args->dio_fd);
		ret = -ENOENT;
		goto out;
	}

	bvec = kmalloc_array(MAX_BVEC_BATCH, sizeof(*bvec), GFP_KERNEL);
	if (!bvec) {
		ret = -ENOMEM;
		goto out;
	}

	for_each_sg(sgt->sgl, sg, sgt->orig_nents, i) {
		unsigned int sg_len = sg->length;
		unsigned int rw_len;

		if (mem_offset >= sg_len) {
			mem_offset -= sg_len;
			continue;
		}

		rw_len = min_t(size_t, total_length, sg_len - mem_offset);
		bvec_set_page(&bvec[bvec_count], sg_page(sg), rw_len, mem_offset);
		bvec_count++;
		accumulated_len += rw_len;
		total_length -= rw_len;
		mem_offset = 0; // Set 0 after the first sg

		if (accumulated_len >= MAX_IO_SIZE ||
		    bvec_count >= MAX_BVEC_BATCH ||
		    total_length == 0) {
			iter = (struct iov_iter){
				.iter_type = ITER_BVEC,
				.data_source = file_read_write ? ITER_SOURCE : ITER_DEST,
				.bvec = bvec,
				.nr_segs = bvec_count,
				.iov_offset = 0,
				.count = accumulated_len,
			};

			if (file_read_write)
				ret = vfs_iter_write(file, &iter, &file_offset, 0);
			else
				ret = vfs_iter_read(file, &iter, &file_offset, 0);

			if (ret < 0) {
				pr_err("IO failed at offset %lld, ret=%d\n", file_offset, ret);
				goto io_error;
			}

			/* vfs_iter_* returns bytes transferred on success */
			total_transferred += ret;
			bvec_count = 0;
			accumulated_len = 0;
		}
	}

	/* Check if all requested data was processed */
	if (total_length > 0) {
		pr_err("Length mismatch: input=%zu, remaining=%zu\n",
		       args->length, total_length);
		ret = -EIO;
		goto io_error;
	}

	ret = total_transferred;

io_error:
	kfree(bvec);
out:
	/* Unmap, detach, and put the DMA-BUF */
	if (sgt)
		dma_buf_unmap_attachment(attachment, sgt, dir);
	if (attachment)
		dma_buf_detach(dmabuf, attachment);
	dma_buf_put(dmabuf);
	if (file)
		fput(file);
	return ret;
}

static long dio_dma_mapper_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	int fd;
	struct dio_dma_data *data = file->private_data;
	struct dio_dmabuf_operate_args io_args;

	switch (cmd) {
	case IOCTL_ATTACH_DMABUF:
		/* Get the DMA-BUF file descriptor from userspace */
		if (copy_from_user(&fd, (int __user *)arg, sizeof(fd)))
			return -EFAULT;
		return dio_dma_mapper_attach_dma_fd(data, fd, DMA_BIDIRECTIONAL);
	case IOCTL_DIRECT_IO_READ_FROM_DMABUF:
		/* Get the direct IO arguments from userspace */
		if (copy_from_user(&io_args, (struct dio_dmabuf_operate_args __user *)arg,
				   sizeof(io_args)))
			return -EFAULT;
		return dio_dma_mapper_io(data, &io_args, DMA_TO_DEVICE);
	case IOCTL_DIRECT_IO_WRITE_TO_DMABUF:
		/* Get the direct IO arguments from userspace */
		if (copy_from_user(&io_args, (struct dio_dmabuf_operate_args __user *)arg,
				   sizeof(io_args)))
			return -EFAULT;
		return dio_dma_mapper_io(data, &io_args, DMA_FROM_DEVICE);
	default:
		return -EINVAL;
	}
	return 0;
}
static vm_fault_t dio_dma_mapper_vm_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct dio_dma_data *data = vma->vm_private_data;

	if (!data) {
		pr_err("dio_dma_data is NULL\n");
		return VM_FAULT_SIGBUS;
	}

	if (vmf->pgoff >= data->pagecount) {
		pr_err("%lu is not an valid page\n", vmf->pgoff);
		return VM_FAULT_SIGBUS;
	}
	vmf->page = data->pages[vmf->pgoff];
	return 0;
}
static const struct vm_operations_struct dio_dma_mapper_vm_ops = {
	.fault = dio_dma_mapper_vm_fault,
};
static int dio_dma_mapper_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct dio_dma_data *data = file->private_data;
	if (!data) {
		pr_err("dio_dma_data is NULL\n");
		return -EINVAL;
	}
	if (data->pagecount == 0) {
		pr_err("not attach dma buf yet\n");
		return -EINVAL;
	}
	/* check for overflowing the buffer's size */
	if (vma->vm_pgoff + vma_pages(vma) > data->pagecount) {
		pr_err("%s %lu is not an valid page\n", __func__,
		       vma->vm_pgoff);
		return -EINVAL;
	}
	if (vm_map_pages(vma, data->pages,
			    data->pagecount)) {
		/* fallback to set pte in page fault */
		vma->vm_ops = &dio_dma_mapper_vm_ops;
		vma->vm_private_data = file->private_data;
	}
	return 0;
}
static int dio_dma_mapper_open(struct inode *node, struct file *fp)
{
	struct dio_dma_data *data =
		kzalloc(sizeof(struct dio_dma_data), GFP_KERNEL);
	if (!data) {
		pr_err("Failed to allocate dio_dma_data\n");
		return -ENOMEM;
	}
	fp->private_data = data;
	return 0;
}
static int dio_dma_mapper_release(struct inode *node, struct file *fp)
{
	struct dio_dma_data *data = fp->private_data;
	if (data) {
		release_dio_dma_data(data);
		kfree(data);
		fp->private_data = NULL;
	}
	return 0;
}
static ssize_t dio_dma_mapper_read(struct file *fp, char __user *buf,
				   size_t count, loff_t *ppos)
{
	const char *msg = DIO_VERSION_STRING;
	size_t len = strlen(msg);

	if (*ppos >= len)
		return 0;

	if (count > len - *ppos)
		count = len - *ppos;

	if (copy_to_user(buf, msg + *ppos, count))
		return -EFAULT;

	*ppos += count;
	return count;
}

/* File operations for the misc device */
static const struct file_operations dma_buf_misc_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = dio_dma_mapper_ioctl,
	.open = dio_dma_mapper_open,
	.release = dio_dma_mapper_release,
	.mmap = dio_dma_mapper_mmap,
	.read = dio_dma_mapper_read,
};
/* Misc device structure */
static struct miscdevice dma_buf_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "dio_dma_mapper_misc",
	.fops = &dma_buf_misc_fops,
	.mode = 0600,
};
/* Module initialization */
static int __init dma_buf_misc_init(void)
{
	int ret;
	ret = misc_register(&dma_buf_misc_device);
	if (ret) {
		pr_err("Failed to register misc device\n");
		return ret;
	}
	/* Set the DMA mask for the misc device */
	if (dma_coerce_mask_and_coherent(dma_buf_misc_device.this_device, ~0ULL)) {
		pr_err("Failed to set dio_dma 64-bit DMA mask\n");
	}
	pr_info("DMA-BUF dio_dma mapper misc device registered\n");
	return 0;
}
/* Module exit */
static void __exit dma_buf_misc_exit(void)
{
	misc_deregister(&dma_buf_misc_device);
	pr_info("DMA-BUF misc device unregistered\n");
}
module_init(dma_buf_misc_init);
module_exit(dma_buf_misc_exit);
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_DESCRIPTION(
	"DMA-BUF mapper for DirectIO: attach dmabuf for GUP-compatible mmap and kernel-side vfs direct IO");
