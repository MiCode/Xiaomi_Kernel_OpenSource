#include <linux/debugfs.h>
#include <linux/mm.h>
#include <linux/hugetlb.h>
#include <linux/huge_mm.h>
#include <linux/mount.h>
#include <linux/seq_file.h>
#include <linux/highmem.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/mempolicy.h>
#include <linux/rmap.h>
#include <linux/swap.h>
#include <linux/swapops.h>

#include <asm/elf.h>
#include <asm/uaccess.h>
#include <asm/tlbflush.h>
#include "internal.h"

#include <linux/bootmem.h>
#include <linux/kallsyms.h>

static ssize_t
read_page_owner(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long pfn;
	struct page *page;
	char *kbuf;
	int ret = 0;
	ssize_t num_written = 0;
	int blocktype = 0, pagetype = 0;

	page = NULL;
	pfn = min_low_pfn + *ppos;

	/* Find a valid PFN or the start of a MAX_ORDER_NR_PAGES area */
	while (!pfn_valid(pfn) && (pfn & (MAX_ORDER_NR_PAGES - 1)) != 0)
		pfn++;

	//printk("pfn: %ld max_pfn: %ld\n", pfn, max_pfn);
	/* Find an allocated page */
	for (; pfn < max_pfn; pfn++) {
		/*
		 * If the new page is in a new MAX_ORDER_NR_PAGES area,
		 * validate the area as existing, skip it if not
		 */
		if ((pfn & (MAX_ORDER_NR_PAGES - 1)) == 0 && !pfn_valid(pfn)) {
			pfn += MAX_ORDER_NR_PAGES - 1;
			continue;
		}

		/* Check for holes within a MAX_ORDER area */
		if (!pfn_valid_within(pfn))
			continue;

		page = pfn_to_page(pfn);

		/* Catch situations where free pages have a bad ->order  */
		if (page->order >= 0 && PageBuddy(page))
			printk(KERN_WARNING
				"PageOwner info inaccurate for PFN %lu\n",
				pfn);

		/* Stop search if page is allocated and has trace info */
		if (page->order >= 0 && page->trace.nr_entries) {
			//intk("stopped search at pfn: %ld\n", pfn);
			break;
		}
	}

	if (!pfn_valid(pfn))
		return 0;
	/*
	 * If memory does not end at a SECTION_SIZE boundary, then
	 * we might have a pfn_valid() above max_pfn
	 */
	if (pfn >= max_pfn)
		return 0;

	/* Record the next PFN to read in the file offset */
	*ppos = (pfn - min_low_pfn) + 1;

	kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	//printk("page: %p\n", page);
	ret = snprintf(kbuf, count, "Page allocated via order %d, mask 0x%x\n",
			page->order, page->gfp_mask);
	if (ret >= count) {
		ret = -ENOMEM;
		goto out;
	}

	/* Print information relevant to grouping pages by mobility */
	blocktype = get_pageblock_migratetype(page);
	pagetype  = gfpflags_to_migratetype(page->gfp_mask);
	ret += snprintf(kbuf+ret, count-ret,
			"PFN %lu Block %lu type %d %s "
			"Flags %s%s%s%s%s%s%s%s%s%s%s%s\n",
			pfn,
			pfn >> pageblock_order,
			blocktype,
			blocktype != pagetype ? "Fallback" : "        ",
			PageLocked(page)	? "K" : " ",
			PageError(page)		? "E" : " ",
			PageReferenced(page)	? "R" : " ",
			PageUptodate(page)	? "U" : " ",
			PageDirty(page)		? "D" : " ",
			PageLRU(page)		? "L" : " ",
			PageActive(page)	? "A" : " ",
			PageSlab(page)		? "S" : " ",
			PageWriteback(page)	? "W" : " ",
			PageCompound(page)	? "C" : " ",
			PageSwapCache(page)	? "B" : " ",
			PageMappedToDisk(page)	? "M" : " ");
	if (ret >= count) {
		ret = -ENOMEM;
		goto out;
	}

	num_written = ret;

	ret = snprint_stack_trace(kbuf + num_written, count - num_written,
				  &page->trace, 0);
	if (ret >= count - num_written) {
		ret = -ENOMEM;
		goto out;
	}
	num_written += ret;

	ret = snprintf(kbuf + num_written, count - num_written, "\n");
	if (ret >= count - num_written) {
		ret = -ENOMEM;
		goto out;
	}

	num_written += ret;
	ret = num_written;

	if (copy_to_user(buf, kbuf, ret))
		ret = -EFAULT;
out:
	kfree(kbuf);
	return ret;
}

static struct file_operations proc_page_owner_operations = {
	.read		= read_page_owner,
};

static int __init pageowner_init(void)
{
	struct dentry *dentry;

	dentry = debugfs_create_file("page_owner", S_IRUSR, NULL,
			NULL, &proc_page_owner_operations);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);
	return 0;
}
module_init(pageowner_init)
