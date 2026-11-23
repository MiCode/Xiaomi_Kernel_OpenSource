
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/dma-buf.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#define IOCTL_ATTACH_DMABUF _IOW('d', 1, int)
#define IOCTL_UNATTACH_DMABUF _IOW('d', 2, int)
struct dio_dma_data {
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	struct page **pages;
	struct dma_buf *dmabuf;
	unsigned long pagecount;
};
static struct miscdevice dma_buf_misc_device;
static int sg_to_page_addr_arrays(struct sg_table *sgt, struct page **pages,
			   int max_entries)
{
	struct sg_page_iter page_iter;
	struct page **p = pages;
	if (pages) {
		for_each_sgtable_page (sgt, &page_iter, 0) {
			if (WARN_ON(p - pages >= max_entries))
				return -1;
			*p++ = sg_page_iter_page(&page_iter);
		}
	}
	return 0;
}
static long dio_dma_mapper_attach_dma_fd(struct dio_dma_data *data, int fd)
{
	struct dma_buf *dmabuf;
	struct sg_table *sgt;
	struct dma_buf_attachment *attachment;
	struct page **pages;
	int npages;
	/* Get the DMA-BUF object */
	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf))
		goto dma_buf_err;
	/* Attach to the DMA-BUF */
	attachment = dma_buf_attach(dmabuf, dma_buf_misc_device.this_device);
	if (IS_ERR(attachment))
		goto attach_err;
	/* Map the buffer to scatter-gather table */
	sgt = dma_buf_map_attachment(attachment, DMA_FROM_DEVICE);
	if (IS_ERR(sgt))
		goto map_err;
	/* Pin pages and get the number of pages */
	npages = PAGE_ALIGN(dmabuf->size) / PAGE_SIZE;
	//pages = kcalloc(npages, sizeof(*pages), GFP_KERNEL);
	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		goto pages_err;
	sg_to_page_addr_arrays(sgt, pages, npages);
	data->dmabuf = dmabuf;
	data->attachment = attachment;
	data->sgt = sgt;
	data->pagecount = npages;
	data->pages = pages;
	return 0;
pages_err:
	pr_err("Failed to allocate pages array\n");
	dma_buf_unmap_attachment(attachment, sgt, DMA_BIDIRECTIONAL);
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
static long dio_dma_mapper_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	int fd;
	struct dio_dma_data *data = file->private_data;
	switch (cmd) {
	case IOCTL_ATTACH_DMABUF:
		/* Get the DMA-BUF file descriptor from userspace */
		if (copy_from_user(&fd, (int __user *)arg, sizeof(fd)))
			return -EFAULT;
		return dio_dma_mapper_attach_dma_fd(data, fd);
	default:
		return -EINVAL;
	}
	return 0;
}
static vm_fault_t dma_heap_vm_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct dio_dma_data *data = vma->vm_private_data;
	if (vmf->pgoff > data->pagecount) {
		pr_err("%lu is not an valid page\n", vmf->pgoff);
		return VM_FAULT_SIGBUS;
	}
	vmf->page = data->pages[vmf->pgoff];
	get_page(vmf->page);
	return 0;
}
static const struct vm_operations_struct dio_dma_heap_vm_ops = {
	.fault = dma_heap_vm_fault,
};
static int dio_dma_mapper_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct dio_dma_data *data = file->private_data;
	if (data && data->pagecount == 0) {
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
		vma->vm_ops = &dio_dma_heap_vm_ops;
		vma->vm_private_data = file->private_data;
	}
	return 0;
}
static int dio_dma_mapper_open(struct inode *node, struct file *fp)
{
	struct dio_dma_data *data =
		kzalloc(sizeof(struct dio_dma_data), GFP_KERNEL);
	fp->private_data = data;
	return 0;
}
static void release_dio_dma_data(struct dio_dma_data *data)
{
	struct dma_buf *dmabuf = data->dmabuf;
	dma_buf_unmap_attachment(data->attachment, data->sgt,
				 DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, data->attachment);
	dma_buf_put(dmabuf);
	if (data->pages) {
		vfree(data->pages);
		data->pages = NULL;
	}
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
/* File operations for the misc device */
static const struct file_operations dma_buf_misc_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = dio_dma_mapper_ioctl,
	.open = dio_dma_mapper_open,
	.release = dio_dma_mapper_release,
	.mmap = dio_dma_mapper_mmap,
};
/* Misc device structure */
static struct miscdevice dma_buf_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "dio_dma_mapper_misc",
	.fops = &dma_buf_misc_fops,
	.mode = 0660,
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
	"A misc driver to attach to DMA-BUF and get pages to match gup requirement");
