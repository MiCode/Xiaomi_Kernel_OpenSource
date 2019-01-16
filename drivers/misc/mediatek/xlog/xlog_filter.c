#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/security.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/uaccess.h>
#include <linux/personality.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/shmem_fs.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "xlog_internal.h"

u32 *xLogMem;
u32 xlog_global_tag_level = XLOGF_DEFAULT_LEVEL;

static DEFINE_SPINLOCK(xLog_avl_mutex);

struct avl modulemap[XLOG_MODULE_MAX];
int empty = 0;
struct avl *root = NULL;

static int xLog_insert(const char *name);

int xLog_isOn(const char *name, int level)
{
	if (xLogMem != NULL) {
		int idx = xLog_insert(name);
		if ((idx >= 0) && (idx < XLOG_MODULE_MAX)) {
			u32 level_setting = xLogMem[idx];
			return (level >= (level_setting & 0xf)) ? 1 : 0;
		}
	}
	return 1;
}

static void xLog_redepth(struct avl *node)
{
	int left, right;
	if (!node)
		return;
	left = node->left ? node->left->depth : 0;
	right = node->right ? node->right->depth : 0;
	node->depth = (left > right ? left : right) + 1;
}

static void xLog_balance(struct avl *node)
{
	struct avl *left, *right, **store;
	int depth;
	while (node) {
		xLog_redepth(node);
		left = node->left;
		right = node->right;
		if (!node->parent)
			store = &root;
		else
			store =
			    ((node->parent->left ==
			      node) ? &(node->parent->left) : &(node->parent->right));
		depth = (left ? left->depth : 0) - (right ? right->depth : 0);
		if (depth > 1) {
			if ((left->left ? left->left->depth : 0) >
			    (left->right ? left->right->depth : 0)) {
				node->left = left->right;
				left->right = node;
				left->parent = node->parent;
				node->parent = left;
				if (node->left)
					node->left->parent = node;
				*store = left;
				xLog_redepth(node);
				xLog_redepth(left);
			} else {
				right = left->right;
				node->left = right->right;
				right->right = node;
				left->right = right->left;
				right->left = left;
				right->parent = node->parent;
				node->parent = right;
				left->parent = right;
				if (node->left)
					node->left->parent = node;
				if (left->right)
					left->right->parent = left;
				*store = right;
				xLog_redepth(node);
				xLog_redepth(left);
				xLog_redepth(right);
			}
		} else if (depth < -1) {
			if ((right->right ? right->right->depth : 0) >
			    (right->left ? right->left->depth : 0)) {
				node->right = right->left;
				right->left = node;
				right->parent = node->parent;
				node->parent = right;
				if (node->right)
					node->right->parent = node;
				*store = right;
				xLog_redepth(node);
				xLog_redepth(right);
			} else {
				left = right->left;
				node->right = left->left;
				left->left = node;
				right->left = left->right;
				left->right = right;
				left->parent = node->parent;
				node->parent = left;
				right->parent = left;
				if (node->right)
					node->right->parent = node;
				if (right->left)
					right->left->parent = right;
				*store = left;
				xLog_redepth(node);
				xLog_redepth(right);
				xLog_redepth(left);
			}
		}
		node = node->parent;
	}
}

static int xLog_insert(const char *name)
{
	int cmp, offset;
	struct avl **ptr = &(root);
	struct avl *parent = NULL;
	unsigned long flags;

	spin_lock_irqsave(&xLog_avl_mutex, flags);

	while (*ptr != NULL) {
		cmp = strcmp((*ptr)->name, name);
		if (cmp == 0) {
			offset = ((*ptr)->offset);
			goto insert_out;
		}
		if (cmp < 0)
			parent = *ptr, ptr = &((*ptr)->left);
		if (cmp > 0)
			parent = *ptr, ptr = &((*ptr)->right);
	}
	if (empty >= XLOG_MODULE_MAX) {
		offset = -1;
		goto insert_out;
	}
	strncpy(modulemap[empty].name, name, sizeof(modulemap[empty].name) - 1);
	modulemap[empty].offset = empty;	/* value; */
	modulemap[empty].left = NULL;
	modulemap[empty].right = NULL;
	modulemap[empty].parent = parent;
	*ptr = &(modulemap[empty++]);
	xLog_balance(*ptr);
	offset = modulemap[empty - 1].offset;

	xLogMem[offset] = xlog_global_tag_level;

 insert_out:
	spin_unlock_irqrestore(&xLog_avl_mutex, flags);
	return offset;
}

static int xLog_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page = NULL;
	unsigned long offset;
	offset =
	    (((unsigned long)vmf->virtual_address - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT));
	if (offset > PAGE_SIZE << 4)
		goto nopage_out;
	page = virt_to_page(xLogMem + offset);
	vmf->page = page;
	get_page(page);
 nopage_out:
	return 0;
}

static struct vm_operations_struct xLog_vmops = {
	.fault = xLog_fault,
};

static int xlog_open(struct inode *inode, struct file *file)
{
	int ret;
	ret = nonseekable_open(inode, file);
	if (unlikely(ret))
		return ret;
	file->private_data = NULL;
	return 0;
}

static int xlog_release(struct inode *ignored, struct file *file)
{
	return 0;
}

static int xlog_mmap(struct file *file, struct vm_area_struct *vma)
{
	vma->vm_ops = &xLog_vmops;
	vma->vm_flags |= VM_IO;
	vma->vm_private_data = file->private_data;
	return 0;
}

long xlog_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct xlogf_tag_offset xLog_parcel;

	switch (cmd) {
	case XLOGF_FIND_MODULE:
		if (copy_from_user
		    ((void *)&xLog_parcel, (const void __user *)arg, sizeof(xLog_parcel)))
			return -EFAULT;
		xLog_parcel.offset = xLog_insert(xLog_parcel.name);
		if (copy_to_user((void __user *)arg, (void *)&xLog_parcel, sizeof(xLog_parcel)))
			return -EFAULT;
		return 0;
		break;

	case XLOGF_GET_LEVEL:
		if (copy_to_user((void __user *)arg, (void *)&xlog_global_tag_level, sizeof(u32)));
		break;

	case XLOGF_SET_LEVEL:{
			int i;
			xlog_global_tag_level = arg;

			for (i = 0; i < XLOG_MODULE_MAX; i++) {
				xLogMem[i] = xlog_global_tag_level;
			}
			break;
		}

	case XLOGF_TAG_SET_LEVEL:{
			struct xlogf_tag_entry ent;
			int offset;

			if (copy_from_user
			    ((void *)&ent, (const void __user *)arg,
			     sizeof(struct xlogf_tag_entry)))
				return -EFAULT;
			offset = xLog_insert(ent.name);

			if ((offset > 0) && (offset < XLOG_MODULE_MAX)) {
				xLogMem[offset] = ent.level;
			} else {
				ret = -EINVAL;
			}
			break;
		}

	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static struct file_operations xlog_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = xlog_ioctl,
	.compat_ioctl = xlog_ioctl,
	.mmap = xlog_mmap,
	.open = xlog_open,
	.release = xlog_release,
};

static struct miscdevice xlog_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "xLog",
	.fops = &xlog_fops,
	.mode = 0644,
};

#define XLOG_FILTER_FILE "filters"
#define XLOG_SETFIL_FILE "setfil"

static int proc_xlog_filters_show(struct seq_file *p, void *v)
{
	int i;
	seq_printf(p, "count %d, level %x\n", empty, xlog_global_tag_level);
	for (i = 0; i < empty; i++) {
		seq_printf(p, " TAG:\"%s\" level:%08x\n", modulemap[i].name, xLogMem[i]);
	}
	return 0;
}

static int proc_xlog_filters_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_xlog_filters_show, NULL);
}

static const struct file_operations proc_xlog_filter_operations = {
	.open = proc_xlog_filters_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations proc_xlog_setfil_operations = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = xlog_ioctl,
	.compat_ioctl = xlog_ioctl,
	.mmap = xlog_mmap,
	.open = xlog_open,
	.release = xlog_release,
};

static struct proc_dir_entry *xlog_proc_dir;
static struct proc_dir_entry *xlog_filter_file;
static struct proc_dir_entry *xlog_setfil_file;

static int __init xlog_init(void)
{
	int err, i;

	xlog_proc_dir = proc_mkdir("xlog", NULL);
	if (xlog_proc_dir == NULL) {
		printk("xlog proc_mkdir failed\n");
		return -ENOMEM;
	}

	xlog_filter_file = proc_create(XLOG_FILTER_FILE, 0400, xlog_proc_dir,
				       &proc_xlog_filter_operations);
	if (xlog_filter_file == NULL) {
		printk(KERN_ERR "xlog proc_create failed at %s\n", XLOG_FILTER_FILE);
		return -ENOMEM;
	}

	xlog_setfil_file = proc_create(XLOG_SETFIL_FILE, 0444, xlog_proc_dir,
				       &proc_xlog_setfil_operations);
	if (xlog_setfil_file == NULL) {
		printk(KERN_ERR "xlog proc_create failed at %s\n", XLOG_SETFIL_FILE);
		return -ENOMEM;
	}
	/* TODO check if it is correct */
	xLogMem = (u32 *) __get_free_pages(GFP_KERNEL, 1);
	for (i = 0; i < XLOG_MODULE_MAX; i++) {
		xLogMem[i] = XLOGF_DEFAULT_LEVEL;
	}

	err = misc_register(&xlog_dev);
	if (unlikely(err)) {
		printk(KERN_ERR "xLog: failed to unregister device\n");
		return -1;
	} else {
		printk(KERN_INFO "xLog: inited.\n");
		return 0;
	}
}

static void __exit xlog_exit(void)
{
	int err;

	free_pages((long unsigned)xLogMem, 4);

	err = misc_deregister(&xlog_dev);
	if (unlikely(err))
		printk(KERN_ERR "xLog: failed to unregister device!\n");
	printk("xLog: exited.\n");
}
module_init(xlog_init);
module_exit(xlog_exit);

/* TODO module license & information */

MODULE_DESCRIPTION("MEDIATEK Module Log Filtering Driver");
MODULE_AUTHOR("Kirby Wu<kirby.wu@mediatek.com>");
MODULE_LICENSE("GPL");
