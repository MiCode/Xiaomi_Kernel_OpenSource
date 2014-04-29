#undef pr_fmt
#define pr_fmt(fmt) "mm :" fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/compat.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include "mei_dev.h"
#include "mm-txe.h"



#ifndef __phys_to_pfn
#define __phys_to_pfn(phys) ((phys) >> PAGE_SHIFT)
#endif

#ifndef VM_DONTDUMP
#define VM_DONTDUMP VM_RESERVED
#endif

struct mei_mm_pool {
	void *vaddr;
	phys_addr_t paddr;
	size_t size;
	size_t offset;
};

struct mei_mm_client {
	size_t size;
	phys_addr_t paddr;
	bool q;
};
struct mei_mm_device {
	struct miscdevice dev;
	struct mutex lock;
	struct mei_mm_client client;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *dbgfs;
#endif /* CONFIG_DEBUG_FS */
	struct mei_mm_pool pool;
};


#define meimm_warn(__dev, fmt, ...) \
	dev_warn((__dev)->dev.parent, "Warn: %s[%d]: " pr_fmt(fmt),  \
		__func__, __LINE__, ##__VA_ARGS__)

#define meimm_info(__dev, fmt, ...) \
	dev_info((__dev)->dev.parent, "Info: " pr_fmt(fmt), ##__VA_ARGS__)

#define meimm_err(__dev, fmt, ...) \
	dev_err((__dev)->dev.parent, "Error: " pr_fmt(fmt), ##__VA_ARGS__)

#define meimm_dbg(__dev, fmt, ...) \
	dev_dbg((__dev)->dev.parent, "%s[%d]: " pr_fmt(fmt), \
		__func__, __LINE__, ##__VA_ARGS__)

/**
 * mei_mm_open - the open function
 *
 * @inode: pointer to inode structure
 * @file: pointer to file structure
 *
 * returns 0 on success, <0 on error
 */
static int mei_mm_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct mei_mm_device *mdev =
			container_of(miscdev, struct mei_mm_device, dev);

	mutex_lock(&mdev->lock);
	if (mdev->client.q) {
		mutex_unlock(&mdev->lock);
		return -EBUSY;
	}
	mdev->client.q = true;
	mutex_unlock(&mdev->lock);

	file->private_data = &mdev->client;

	meimm_info(mdev, "device opened\n");

	return nonseekable_open(inode, file);

}

/**
 * mei_mm_release - the release function
 *
 * @inode: pointer to inode structure
 * @file: pointer to file structure
 *
 * returns 0 on success, <0 on error
 */
static int mei_mm_release(struct inode *inode, struct file *file)
{
	struct mei_mm_client *client = file->private_data;
	struct mei_mm_device *mdev =
			container_of(client, struct mei_mm_device, client);
	size_t len =  client->size;

	mutex_lock(&mdev->lock);

	if (!client->q)
		goto out;

	if (mdev->pool.offset < len)  {
		meimm_info(mdev, "pool smaller then requested size...truncating");
		len = mdev->pool.offset;
	}
	mdev->pool.offset -= len;

	client->q = false;
	meimm_info(mdev, "meimm: release %zd\n", len);
out:
	mutex_unlock(&mdev->lock);
	return 0;
}
/**
 * mei_mm_alloc - distributing memory chunk to Sec Application (shim library)
 *
 * @file: pointer to file structure
 * @cmd: ioctl command
 * @data: pointer to shim memory structure
 *
 * returns 0 on success , <0 on error
 */

static int mei_mm_alloc(struct mei_mm_device *mdev, unsigned long arg)
{

	struct mei_mm_data req;
	phys_addr_t paddr;
	int ret;
	size_t aligned_size; /* for mmap to work we need page multipliers */

	if (copy_from_user(&req, (char __user *)arg, sizeof(req))) {
		meimm_err(mdev, "failed to copy data from userland\n");
		ret = -EFAULT;
		goto err;
	}

	aligned_size = PAGE_ALIGN(req.size);

	mutex_lock(&mdev->lock);

	if (aligned_size > mdev->pool.size - mdev->pool.offset) {
		meimm_err(mdev, "can't allocate mem from chunk: %zd > %zd - %zd\n",
			aligned_size, mdev->pool.size, mdev->pool.offset);
		ret = -ENOMEM;
		goto err_unlock;
	}

	paddr = mdev->pool.paddr + mdev->pool.offset;
	req.size = aligned_size;

	meimm_info(mdev, "Allocate mem from chunk: paddr=%llud size=%llu\n",
			(unsigned long long)paddr, req.size);

	if (copy_to_user((char __user *)arg, &req, sizeof(req))) {
		meimm_err(mdev, "failed to copy data to userland\n");
		ret = -EFAULT;
		goto err_unlock;
	}

	mdev->pool.offset += aligned_size;
	mdev->client.size = aligned_size;
	mdev->client.paddr = paddr;

	mutex_unlock(&mdev->lock);

	return 0;

err_unlock:
	mutex_unlock(&mdev->lock);
err:
	return ret;
}

/**
 * mei_mm_free - returns memory chunk from Sec Application (shim library
 *
 * @file: pointer to file structure
 * @cmd: ioctl command
 * @data: pointer to shim memory structure
 *
 * returns 0 on success , <0 on error
 */
static int mei_mm_free(struct mei_mm_device *mdev, unsigned long arg)
{
	struct mei_mm_data req;
	int ret;

	if (copy_from_user(&req, (char __user *)arg, sizeof(req))) {
		meimm_err(mdev, "failed to copy data from userland\n");
		return -EFAULT;
	}

	mutex_lock(&mdev->lock);

	if (mdev->pool.offset < req.size) {
		meimm_err(mdev, "cannot free 0x%lld bytes - offset=0x%zd\n",
			  req.size, mdev->pool.offset);
		ret = -EINVAL;
		goto out;
	}

	mdev->pool.offset -= req.size;
	mdev->client.size = 0;
	mdev->client.paddr = 0LL;
	ret = 0;
out:
	mutex_unlock(&mdev->lock);
	return ret;

}

/**
 * mei_ioctl - the IOCTL function
 *
 * @file: pointer to file structure
 * @cmd: ioctl command
 * @data: pointer to mei message structure
 *
 * returns 0 on success , <0 on error
 */

static long mei_mm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mei_mm_client *client = file->private_data;
	struct mei_mm_device *mdev =
			container_of(client, struct mei_mm_device, client);
	int ret;

	/* don't even decode wrong cmds: better returning  ENOTTY than EFAULT */
	if (_IOC_TYPE(cmd) != 'H') {
		meimm_err(mdev, "Wrong IOCTL type: Got %c wanted %c\n",
					_IOC_TYPE(cmd), 'H');
		ret = -ENOTTY;
		goto out;
	}
	if (_IOC_NR(cmd) > MEI_IOC_MAXNR) {
		meimm_err(mdev, "%s: Wrong IOCTL num. Got %d wanted max %d\n",
			"mei_mm", _IOC_NR(cmd), MEI_IOC_MAXNR);
		ret = -ENOTTY;
		goto out;
	}
	if (!access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd))) {
		ret = -EFAULT;
		goto out;
	}

	switch (cmd) {
	case IOCTL_MEI_MM_ALLOC:
		meimm_dbg(mdev, "IOCTL_MEI_ALLOC_MEM_CALL\n");
		ret = mei_mm_alloc(mdev, arg);
	break;
	case IOCTL_MEI_MM_FREE:
		meimm_dbg(mdev, "IOCTL_MEI_FREE_MEM_CALL\n");
		ret = mei_mm_free(mdev, arg);
	break;
	default:
		ret = -EINVAL;
		meimm_err(mdev, "Invalid IOCTL command %d\n", cmd);
		break;
	}
out:
	return ret;
}

static int mei_mm_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct mei_mm_client *client = file->private_data;
	struct mei_mm_device *mdev =
			container_of(client, struct mei_mm_device, client);
	size_t vsize = vma->vm_end - vma->vm_start;
	size_t off = vma->vm_pgoff << PAGE_SHIFT;
	int ret;

	meimm_dbg(mdev, "vm_start=0x%016lX vm_end=0x%016lX vm_pgoff=0x%016lX off=%zd\n",
		vma->vm_start,  vma->vm_end, vma->vm_pgoff, off);

	mutex_lock(&mdev->lock);
	if (vsize > client->size || off > client->paddr + client->size) {
		meimm_err(mdev, "%s: trying to map larger area than available r.size=%zd a.size=%zd vm->pg_off=%ld\n",
			__func__, vsize, client->size, vma->vm_pgoff);
		ret = -EINVAL;
		goto err;
	}


	vma->vm_flags |= VM_DONTDUMP | VM_READ |
			VM_WRITE | VM_SHARED | VM_DONTEXPAND;
	vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	if (remap_pfn_range(vma, vma->vm_start,
			__phys_to_pfn(client->paddr) + vma->vm_pgoff,
			vsize, vma->vm_page_prot)) {
		ret = -EAGAIN;
		goto err;
	}

	mutex_unlock(&mdev->lock);

	vma->vm_private_data = client;
	return 0;

err:
	mutex_unlock(&mdev->lock);
	return ret;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static ssize_t mei_mm_dbgfs_pool_read(struct file *file,
			char __user *user_buf,
			size_t count, loff_t *ppos)
{
	struct mei_mm_device *mdev = file->private_data;
	/* if the buffer is not mapped into kernel space */
	if (!mdev->pool.vaddr)
		return 0;
	return simple_read_from_buffer(user_buf, count, ppos,
				mdev->pool.vaddr, 256);
}
static const struct file_operations mei_mm_dbgfs_pool_ops = {
	.read = mei_mm_dbgfs_pool_read,
	.open = simple_open,
	.llseek = generic_file_llseek,
};
#endif /* CONFIG_DEBUG_FS */


/**
 * mei_compat_ioctl - the compat IOCTL function
 *
 * @file: pointer to file structure
 * @cmd: ioctl command
 * @data: pointer to mei message structure
 *
 * returns 0 on success , <0 on error
 */
#ifdef CONFIG_COMPAT
static long mei_mm_compat_ioctl(struct file *file,
		      unsigned int cmd, unsigned long data)
{
	return mei_mm_ioctl(file, cmd, (unsigned long)compat_ptr(data));
}
#endif
/*
 * file operations structure will be used for mei char device.
 */
static const struct file_operations mei_mm_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mei_mm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mei_mm_compat_ioctl,
#endif /* CONFIG_COMPAT */
	.open = mei_mm_open,
	.release = mei_mm_release,
	.mmap = mei_mm_mmap,
	/*	.poll = mei_mmpoll,*/
	.llseek = no_llseek
};

struct mei_mm_device *mei_mm_init(struct device *dev, void *vaddr,
				dma_addr_t paddr, size_t size)
{

	struct mei_mm_device *mdev;
	int ret;

	if (!dev || !paddr || size == 0)
		return ERR_PTR(-EINVAL);

	mdev = kzalloc(sizeof(struct mei_mm_device), GFP_KERNEL);
	if (!mdev)
		return ERR_PTR(-ENOMEM);

	mdev->dev.minor = MISC_DYNAMIC_MINOR;
	mdev->dev.name = "meimm";
	mdev->dev.fops = &mei_mm_fops;
	mdev->dev.parent = dev;
	/* init pci module */
	ret = misc_register(&mdev->dev);
	if (ret) {
		kzfree(mdev);
		dev_err(dev, "can't register misc device.\n");
		return ERR_PTR(ret);
	}

	mutex_init(&mdev->lock);

	mdev->pool.vaddr = vaddr;
	mdev->pool.paddr = paddr;
	mdev->pool.size = size;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	mdev->dbgfs = debugfs_create_dir("meimm", NULL);
	if (IS_ERR_OR_NULL(mdev->dbgfs)) {
		meimm_err(mdev, "failed to create debug files.\n");
		goto out;
	}


	debugfs_create_file("pool", S_IRUSR, mdev->dbgfs,
			mdev, &mei_mm_dbgfs_pool_ops);

	debugfs_create_size_t("size", S_IRUSR, mdev->dbgfs, &mdev->pool.size);
out:
#endif /* CONFIG_DEBUG_FS */

	return mdev;
}



/**
 * mei_dma_deinit - De-Init Routine for mei_dma misc device
 *
 * mei_dma_deinit is called by release function of mei module.
 */
void mei_mm_deinit(struct mei_mm_device *mdev)
{

	if (!mdev)
		return;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_remove_recursive(mdev->dbgfs);
	mdev->dbgfs = NULL;
#endif /* CONFIG_DEBUG_FS */

	misc_deregister(&mdev->dev);
	kfree(mdev);
}


