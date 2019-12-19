#define pr_fmt(fmt)  "misysinfofreader : " fmt

#include <linux/compat.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/falloc.h>
#include <linux/miscdevice.h>
#include <linux/security.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/uaccess.h>
#include <linux/personality.h>
#include <linux/bitops.h>
#include <linux/jiffies.h>
#include <linux/misysinfofreader.h>

#define PROT_MASK		(PROT_EXEC | PROT_READ | PROT_WRITE)

#define MI_SYSINFO_VERSION 1

struct mi_sysinfo {
	u32 size;
	u32 version;
	u32 config_hz;
	u32 jiffies;
	struct {
		u32 count;
		u32 total_duration_ms;
		u32 time_stamp_jiffies;
	} mm_slowpath;
	u32 placeholder[32];
};
static struct mi_sysinfo *sysinfo;
static spinlock_t mm_slowpath_lock;

u32 *misysinfo_jiffies;

void misysinfo_update_slowpath(u64 time_stamp_ns, u64 time_ns)
{
	unsigned long flags;
	u32 time_ms;
	if (sysinfo != NULL) {
		time_ms = time_ns >> 20;
		if (!spin_trylock_irqsave(&mm_slowpath_lock, flags))
			return;
		sysinfo->mm_slowpath.count++;
		sysinfo->mm_slowpath.total_duration_ms += time_ms;
		sysinfo->mm_slowpath.time_stamp_jiffies = sysinfo->jiffies;
		spin_unlock_irqrestore(&mm_slowpath_lock, flags);
	}
}

static int misysinfofreader_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int misysinfofreader_release(struct inode *ignored, struct file *file)
{
	return 0;
}

static int misysinfofreader_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret;

	/* only PROT_READ is acceptable */
	if (unlikely((vma->vm_flags & ~calc_vm_prot_bits(PROT_READ, 0)) &
		     calc_vm_prot_bits(PROT_MASK, 0))) {
		ret = -EPERM;
		return ret;
	}

	ret = remap_pfn_range(vma, vma->vm_start,
			page_to_pfn(virt_to_page(sysinfo)), PAGE_SIZE,
			PAGE_READONLY);
	return ret;
}

static const struct file_operations misysinfofreader_fops = {
	.owner = THIS_MODULE,
	.open = misysinfofreader_open,
	.release = misysinfofreader_release,
	.mmap = misysinfofreader_mmap,
};

static struct miscdevice misysinfofreader_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "misysinfofreader",
	.fops = &misysinfofreader_fops,
};

static int __init misysinfofreader_init(void)
{
	int ret;
	struct page *page;
	struct mi_sysinfo *local_sysinfo;

	page = alloc_page(GFP_KERNEL|__GFP_ZERO);
	if (page == NULL) {
		pr_err("failed to alloc mem\n");
		return -ENOMEM;
	}
	local_sysinfo = page_address(page);
	local_sysinfo->size = PAGE_SIZE;
	local_sysinfo->version = MI_SYSINFO_VERSION;
	local_sysinfo->config_hz = CONFIG_HZ;

	ret = misc_register(&misysinfofreader_misc);
	if (unlikely(ret)) {
		free_page((unsigned long)local_sysinfo);
		pr_err("failed to register misc device!\n");
		return ret;
	}

	sysinfo = local_sysinfo;
	misysinfo_jiffies = &sysinfo->jiffies;

	spin_lock_init(&mm_slowpath_lock);

	pr_info("initialized\n");

	return 0;
}

device_initcall(misysinfofreader_init);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Yang Zhenyu <yangzhenyu@xiaomi.com>");
