#include <linux/module.h>
#include <linux/fs.h>
#include <linux/fsnotify.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/ramfs.h>
#include <linux/parser.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/qpnp/power-on.h>

#define TZLOG_OCIMEM_SIZE (0x2000)

#define OCIMEM_LOG_DDR_BASE 0xb4000000
#define OCIMEM_LOG_DDR_SIZE 0x100000

#define LAST_TZ_MAGIC_NUM (0x4c53545a)

static void *last_tz_vaddr;



static void *last_tz_ram_vmap(phys_addr_t start, size_t size,
		unsigned int memtype)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	if (memtype)
		prot = pgprot_noncached(PAGE_KERNEL);
	else
		prot = pgprot_writecombine(PAGE_KERNEL);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n",
				__func__, page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);

	return vaddr;
}

static ssize_t last_tz_read(struct file *file, char __user *buf,
		size_t len, loff_t *offset)
{
	int is_warm_reset = 0;
	char *cold_boot_buf = "no last tzlog, not warm boot";

	if (last_tz_vaddr == NULL)
		return 0;

	is_warm_reset = qpnp_pon_is_warm_reset();

	/*if code boot or pon read error*/
	if (is_warm_reset <= 0) {
		return simple_read_from_buffer(buf, len, offset,
				cold_boot_buf, strlen(cold_boot_buf));
	}

	return simple_read_from_buffer(buf, len, offset,
			last_tz_vaddr, TZLOG_OCIMEM_SIZE);
}

static const struct file_operations last_tz_fops = {
	.owner		= THIS_MODULE,
	.read		= last_tz_read,
	.llseek		= default_llseek,
};

static int __init init_last_tz(void)
{
	int err = 0;
	struct proc_dir_entry *last_tz_entry = NULL;
	unsigned int last_tz_magic = LAST_TZ_MAGIC_NUM;

	last_tz_entry = proc_create_data("last_tz", S_IFREG | S_IRUGO,
			NULL, &last_tz_fops, NULL);
	if (!last_tz_entry) {
		pr_err("Failed to create last_tz entry\n");
		goto out;
	}

	/*map memory*/
	last_tz_vaddr = last_tz_ram_vmap(OCIMEM_LOG_DDR_BASE, TZLOG_OCIMEM_SIZE, 0);
	if (last_tz_vaddr == NULL) {
		pr_err("last tz map memory failed\n");
		goto out;
	}

	pr_info("last tz map memory region 0x%lx\n", (unsigned long)last_tz_vaddr);

	/*set magic number*/
	memcpy(last_tz_vaddr, &last_tz_magic, sizeof(last_tz_magic));
	pr_info("last tz set magic 0x%x\n", last_tz_magic);
out:
	return err;
}

module_init(init_last_tz)

MODULE_AUTHOR("Zhenhua <mazhenhua@xiaomi.com>");
MODULE_LICENSE("GPL");
