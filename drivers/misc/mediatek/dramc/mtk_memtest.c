/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
/* #include <mach/mtk_clkmgr.h> */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/highmem.h>
#include <asm/setup.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_meminfo.h>
#include <mt-plat/mtk_chip.h>
#include <mt-plat/aee.h>

#include "mtk_dramc.h"
#include "dramc.h"

#ifdef CONFIG_OF_RESERVED_MEM
#define DRAM_R0_MEMTEST_RESERVED_KEY "reserve-memory-dram_r0_memtest"
#define DRAM_R1_MEMTEST_RESERVED_KEY "reserve-memory-dram_r1_memtest"
#include <linux/of_reserved_mem.h>
#include <mt-plat/mtk_memcfg.h>
#endif

#ifdef DRAMC_MEMTEST_DEBUG_SUPPORT
#define MAX_TEST_CLIENT	20

enum {
	FAIL_REGION_RANK0,
	FAIL_REGION_RANK1,
	FAIL_REGION_VIRT,
	FAIL_REGION_NUM,
};

enum {
	TEST_RESULT_INIT,
	TEST_RESULT_ONGOING,
	TEST_RESULT_FAILED,
	TEST_RESULT_PASS,
};

struct memtest_client {
	struct list_head node;
	pid_t pid;
	unsigned int rank;
};

static void __iomem *(*get_emi_base)(void);
static phys_addr_t dram_addr;

static int result;
static DEFINE_MUTEX(test_result_mutex);
static DEFINE_MUTEX(test_mem0_mutex);
static DEFINE_MUTEX(test_mem1_mutex);
static DEFINE_MUTEX(test_client_mutex);

static struct dentry *memtest_dir;
static struct dentry *read_mr4, *read_mr5, *read_dram_addr;
static struct dentry *memtest_result, *memtest_v2p;
static struct dentry *memtest_mem0, *memtest_mem1;

static phys_addr_t memtest_rank0_addr, memtest_rank1_addr;
static unsigned int memtest_rank0_size, memtest_rank1_size;

static LIST_HEAD(test_client);
static unsigned int test_client_num;
static unsigned int test_fail_region[FAIL_REGION_NUM];

#define Reg_Sync_Writel(addr, val)   writel(val, IOMEM(addr))
#define Reg_Readl(addr) readl(IOMEM(addr))

#ifdef CONFIG_OF_RESERVED_MEM
int dram_memtest_reserve_mem_of_init(struct reserved_mem *rmem)
{
	phys_addr_t rptr = 0;
	unsigned int rsize = 0;

	rptr = rmem->base;
	rsize = (unsigned int)rmem->size;

	if (strstr(DRAM_R0_MEMTEST_RESERVED_KEY, rmem->name)) {
		memtest_rank0_addr = rptr;
		memtest_rank0_size = rsize;
	}

	if (strstr(DRAM_R1_MEMTEST_RESERVED_KEY, rmem->name)) {
		memtest_rank1_addr = rptr;
		memtest_rank1_size = rsize;
	}

	return 0;
}
RESERVEDMEM_OF_DECLARE(dram_reserve_r0_memtest_init,
DRAM_R0_MEMTEST_RESERVED_KEY,
			dram_memtest_reserve_mem_of_init);
RESERVEDMEM_OF_DECLARE(dram_reserve_r1_memtest_init,
DRAM_R1_MEMTEST_RESERVED_KEY,
			dram_memtest_reserve_mem_of_init);
#endif

static ssize_t read_mr4_read(struct file *file,
	char __user *user_buf, size_t len, loff_t *offset)
{
	void __iomem *emi_base;
	void __iomem *dramc_nao_base;
	unsigned int rank, channel, rank_max, channel_num;
	unsigned int emi_cona, mr4;
	unsigned char buf[64];
	ssize_t ret;

	ret = 0;

	emi_base = get_emi_base();
	if (emi_base == NULL) {
		pr_info("[DRAMC] can't find EMI base\n");
		return -1;
	}

	emi_cona = Reg_Readl(emi_base+0x000);

	channel_num = mt_dramc_chn_get(emi_cona);

	rank_max = mt_dramc_ta_support_ranks();

	ret += snprintf(buf + ret, sizeof(buf) - ret, "MR4:");

	for (rank = 0; rank < rank_max; rank++) {
		for (channel = 0; channel < channel_num; channel++) {
			dramc_nao_base = mt_dramc_nao_chn_base_get(channel);
			if (!dramc_nao_base)
				continue;
			mr4 = Reg_Readl(dramc_nao_base + 0x90) & 0xFFFF;
			ret += snprintf(buf + ret, sizeof(buf) - ret,
				" R%uCH%c=0x%x,", rank, 'A' + channel, mr4);
		}
	}

	ret += snprintf(buf + ret, sizeof(buf) - ret, "\n");

	return simple_read_from_buffer(user_buf, len, offset, buf, ret);
}

static const struct file_operations read_mr4_fops = {
	.owner = THIS_MODULE,
	.read = read_mr4_read,
};

__weak unsigned char get_ddr_mr(unsigned int index)
{
	return 0;
}

static ssize_t read_mr5_read(struct file *file,
	char __user *user_buf, size_t len, loff_t *offset)
{
	unsigned char buf[64];
	ssize_t ret;

	ret = 0;

	ret += snprintf(buf + ret, sizeof(buf) - ret,
		"MR5:0x%x\n", get_ddr_mr(5));

	return simple_read_from_buffer(user_buf, len, offset, buf, ret);
}

static const struct file_operations read_mr5_fops = {
	.owner = THIS_MODULE,
	.read = read_mr5_read,
};

__weak unsigned int mt_dramc_col_size_get(unsigned int emi_cona,
	unsigned int rank)
{
	unsigned int col;

	if (rank == 0)
		col = (emi_cona >> 4) & 0x3;
	else
		col = (emi_cona >> 6) & 0x3;

	return col + 9;
}

__weak unsigned int mt_dramc_row_size_get(unsigned int emi_cona,
	unsigned int rank)
{
	unsigned int row;

	if (rank == 0)
		row = ((emi_cona >> 22) & 0x4) | ((emi_cona >> 12) & 0x3);
	else
		row = ((emi_cona >> 23) & 0x4) | ((emi_cona >> 14) & 0x3);

	return row + 13;
}
void dramc_addr_descramble(phys_addr_t *addr, unsigned int emi_conf)
{
	unsigned int bit_scramble, bit_xor, bit_shift;

	/* calculate DRAM base address (addr) */
	for (bit_scramble = 11; bit_scramble < 17; bit_scramble++) {
		bit_xor = (emi_conf >> (4*(bit_scramble-11))) & 0xf;
		bit_xor &= *addr >> 16;
		for (bit_shift = 0; bit_shift < 4; bit_shift++)
			*addr ^= ((bit_xor>>bit_shift)&0x1) << bit_scramble;
	}

}

int dramc_dram_address_get(phys_addr_t phys_addr,
	unsigned int *rank, unsigned int *row,
	unsigned int *bank, unsigned int *col, unsigned int *ch)
{
	void __iomem *emi_base;
	unsigned int emi_cona, emi_conf;
	unsigned int ch_pos, channel_num;
	unsigned int ch_width, col_width, row_width;
	unsigned int bit_shift;
	unsigned int temp, rank_max;
	unsigned int ddr_type;
	int r;
	phys_addr_t rank_base;

	emi_base = get_emi_base();
	if (emi_base == NULL) {
		pr_info("[DRAMC] can't find EMI base\n");
		return -1;
	}
	emi_cona = Reg_Readl(emi_base+0x000);
	emi_conf = Reg_Readl(emi_base+0x028)>>8;

	channel_num = mt_dramc_chn_get(emi_cona);

	rank_max = mt_dramc_ta_support_ranks();

	if (rank_max > 2) {
		pr_info("[DRAMC] invalid rank num (rank_max = %u)\n", rank_max);
		return -1;
	}

	ddr_type = get_ddr_type();

	for (r = rank_max - 1; r >= 0; r--) {
		rank_base = mt_dramc_rankbase_get(r);

		if (rank_base <= phys_addr) {
			*rank = r;
			phys_addr -= rank_base;
			break;
		}
	}

	if (r < 0) {
		pr_info("[DRAMC] invalid rank\n");
		return -1;
	}

	phys_addr &= 0xFFFFFFFF;

	dramc_addr_descramble(&phys_addr, emi_conf);

	/*
	 * pr_info("[LastDRAMC] reserved address after emi: %llx\n", phys_addr);
	 */

	*ch = 0;

	if (channel_num > 1) {
		ch_pos = mt_dramc_chp_get(emi_cona);

		*ch = (phys_addr >> ch_pos) & 0x1;

		for (ch_width = bit_shift = 0; bit_shift < 4;
				bit_shift++) {
			if ((1 << bit_shift) >= channel_num)
				break;
			ch_width++;
		}

		temp = (phys_addr & ~(((0x1<<ch_width)-1)<<ch_pos)) >> ch_width;
		phys_addr = temp | (phys_addr & ((0x1<<ch_pos)-1));
	}

	col_width = mt_dramc_col_size_get(emi_cona, *rank);
	row_width = mt_dramc_row_size_get(emi_cona, *rank);

	if ((ddr_type == TYPE_LPDDR4) || (ddr_type == TYPE_LPDDR4X))
		phys_addr = phys_addr >> 1;
	else if (ddr_type == TYPE_LPDDR3)
		phys_addr = phys_addr >> 2;
	else {
		pr_info("[DRAMC] undefined DRAM type\n");
		return -1;
	}

	/* now here is row.bank.col */

	*col = phys_addr & ((1 << col_width) - 1);
	*bank = (phys_addr >> col_width) & 0x7;
	*row = (phys_addr >> (col_width + 3)) & ((1 << row_width) - 1);

	return 0;
}

static int dramc_format_dram_addr(phys_addr_t addr, char *buf, unsigned int len)
{
	unsigned int rank, row, bank, col, ch;
	int sz;

	sz = 0;

	if (dramc_dram_address_get(addr, &rank, &row, &bank, &col, &ch))
		return 0;

#ifdef CONFIG_PHYS_ADDR_T_64BIT
	sz = snprintf(buf, len, "addr: 0x%llx ", addr);
#else
	sz = snprintf(buf, len, "addr: 0x%x ", addr);
#endif

	sz += snprintf(buf + sz, len - sz,
		"(rank=0x%x, row=0x%x, bank=0x%x, col=0x%x, ch=0x%x)\n",
		rank, row, bank, col, ch);

	return sz;
}

struct read_dram_addr_ioctl_s {
	unsigned long long pa;
	unsigned int buf_sz;
	unsigned char __user *buf;
};

static ssize_t read_dram_addr_read(struct file *file,
	char __user *user_buf, size_t len, loff_t *offset)
{
	unsigned char buf[128];
	ssize_t sz;

	sz = dramc_format_dram_addr(dram_addr, buf, sizeof(buf));

	if (sz > 0)
		return simple_read_from_buffer(user_buf, len, offset, buf, sz);

	return 0;
}

static ssize_t read_dram_addr_write(struct file *file,
	const char __user *user_buf, size_t len, loff_t *offset)
{
	unsigned long long adr;
	unsigned char buf[32];
	int sz;

	if (len) {
		sz = simple_write_to_buffer(buf, sizeof(buf), offset,
			user_buf, len);

		if (sz != len)
			return -EIO;

		if (sz == sizeof(buf))
			sz--;

		buf[sz] = '\0';

		if (kstrtoull(buf, 0, &adr) != 0)
			return -EINVAL;

		dram_addr = adr;
	}
	return len;
}

static long read_dram_addr_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	struct read_dram_addr_ioctl_s info;
	unsigned char buf[128];
	ssize_t sz;

	if (copy_from_user(&info, (unsigned long *)arg, sizeof(info)))
		return -EFAULT;

	sz = dramc_format_dram_addr(info.pa, buf, sizeof(buf));

	if (sz) {
		if (sz > info.buf_sz)
			sz = info.buf_sz;

		if (copy_to_user(info.buf, &buf, sz))
			return -EFAULT;
	}

	return 0;
}

static const struct file_operations read_dram_addr_fops = {
	.owner = THIS_MODULE,
	.read = read_dram_addr_read,
	.write = read_dram_addr_write,
	.unlocked_ioctl = read_dram_addr_ioctl,
};

static ssize_t test_result_read(struct file *file,
	char __user *user_buf, size_t len, loff_t *offset)
{
	char const *str[] = { "initial", "on-going", "fail", "pass" };
	char buf[64];
	int ret, sz, sz2, i;

	ret = result;

	sz = snprintf(buf, sizeof(buf), "%s\n", str[ret]);

	if (!*offset && ret == TEST_RESULT_FAILED) {
		sz2 = sz;
		sz2 += snprintf(buf + sz2, sizeof(buf) - sz2, "Fail region: ");
		for (i = 0; i < FAIL_REGION_VIRT; i++) {
			if (test_fail_region[i])
				sz2 += snprintf(buf + sz2, sizeof(buf) - sz2,
						"rank %d, ", i);
		}

		if (test_fail_region[FAIL_REGION_VIRT])
			sz2 += snprintf(buf + sz2, sizeof(buf) - sz2,
					"virtual\n");

		aee_kernel_warning("DRAM_MEMTEST", buf);
	}

	return simple_read_from_buffer(user_buf, len, offset, buf, sz);
}

static ssize_t test_result_write(struct file *file,
	const char __user *user_buf, size_t len, loff_t *offset)
{
	struct memtest_client *client;
	struct list_head *p;
	char buf[32];
	int ret, sz, region;

	sz =  simple_write_to_buffer(buf, sizeof(buf), offset, user_buf, len);

	if (sz != len)
		return -EIO;

	if (sz == sizeof(buf))
		sz--;

	buf[sz] = '\0';

	if (!strncmp(buf, "fail", 4))
		ret = TEST_RESULT_FAILED;
	else if (!strncmp(buf, "pass", 4))
		ret = TEST_RESULT_PASS;
	else
		ret = TEST_RESULT_ONGOING;

	mutex_lock(&test_result_mutex);

	if (result != TEST_RESULT_FAILED && result != TEST_RESULT_PASS)
		result = ret;

	mutex_unlock(&test_result_mutex);

	region = FAIL_REGION_VIRT;

	mutex_lock(&test_client_mutex);
	list_for_each(p, &test_client) {
		client = list_entry(p, struct memtest_client, node);
		if (client->pid == current->pid) {
			region = client->rank;
			break;
		}
	}
	mutex_unlock(&test_client_mutex);

	test_fail_region[region] = 1;

	return sz;
}

static const struct file_operations test_result_fops = {
	.owner = THIS_MODULE,
	.read = test_result_read,
	.write = test_result_write,
};

static phys_addr_t memtest_user_v2p(unsigned long va)
{
	unsigned long pageOffset = (va & (PAGE_SIZE - 1));
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	phys_addr_t pa;

	if (current == NULL) {
		pr_info("warning: memtest_user_v2p, current is NULL!\n");
		return 0;
	}
	if (current->mm == NULL) {
		pr_info("warning: memtest_user_v2p, current->mm is NULL!\n");
		pr_info("tgid=0x%x, name=%s\n", current->tgid, current->comm);
		return 0;
	}

	pgd = pgd_offset(current->mm, va);       /* what is tsk->mm */
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		pr_info("memtest_user_v2p, va=0x%lx, pgd invalid!\n", va);
		return 0;
	}

	pud = pud_offset(pgd, va);
	if (pud_none(*pud) || pud_bad(*pud)) {
		pr_info("memtest_user_v2p, va=0x%lx, pud invalid!\n", va);
		return 0;
	}

	pmd = pmd_offset(pud, va);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		pr_info("(memtest_user_v2p, va=0x%lx, pmd invalid!\n", va);
		return 0;
	}

	pte = pte_offset_map(pmd, va);
	if (pte_present(*pte)) {
		/* pa=(pte_val(*pte) & (PAGE_MASK)) | pageOffset; */
		pa = (pte_val(*pte) & (PHYS_MASK) & (~((phys_addr_t) 0xfff))) |
			pageOffset;
		pte_unmap(pte);
		return pa;
	}

	pte_unmap(pte);

	pr_info("memtest_user_v2p, va=0x%lx, pte invalid!\n", va);
	return 0;
}

static long test_v2p_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	unsigned long long va = 0x0L;
	phys_addr_t pa = 0x0L;

	if (copy_from_user(&va, (unsigned long *)arg, sizeof(va)))
		return -EFAULT;

	pa = memtest_user_v2p(va);

	if (copy_to_user((unsigned long *)arg, &pa, sizeof(pa)))
		return -EFAULT;

	return 0;
}

static const struct file_operations test_v2p_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = test_v2p_ioctl,
};

static int test_mem_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return nonseekable_open(inode, file);
}

static void mmap_mem_open(struct vm_area_struct *vma)
{
	unsigned long rank = (unsigned long) vma->vm_file->private_data;
	struct memtest_client *client;

	mutex_lock(&test_client_mutex);
	if (test_client_num <= MAX_TEST_CLIENT) {
		client = kmalloc(sizeof(*client), GFP_KERNEL);

		if (client) {
			client->pid = current->pid;
			client->rank = rank;
			list_add_tail(&client->node, &test_client);
			test_client_num++;
		}
	}
	mutex_unlock(&test_client_mutex);
}

static void mmap_mem_close(struct vm_area_struct *vma)
{
	/*size_t size = vma->vm_end - vma->vm_start;*/
	struct memtest_client *client;
	struct list_head *p;

	/*
	 *pr_info("%s(): 0x%lx~0x%lx pgoff=0x%lx %x\n", __func__,
	 *		vma->vm_start, vma->vm_end, vma->vm_pgoff, size);
	 */

	mutex_lock(&test_client_mutex);
	list_for_each(p, &test_client) {
		client = list_entry(p, struct memtest_client, node);
		if (client->pid == current->pid) {
			list_del(&client->node);
			test_client_num--;

			kfree(client);
			break;
		}
	}
	mutex_unlock(&test_client_mutex);
}

static const struct vm_operations_struct mmap_mem_ops = {
	.open = mmap_mem_open,
	.close = mmap_mem_close,
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys,
#endif
};

static ssize_t test_mem_read(struct file *file,
	char __user *user_buf, size_t len, loff_t *offset)
{
	static char buf[32];
	unsigned long rank = (unsigned long) file->private_data;
	phys_addr_t *addr;
	unsigned int *sz;
	int size;

	if (rank == 0) {
		addr = &memtest_rank0_addr;
		sz = &memtest_rank0_size;
	} else {
		addr = &memtest_rank1_addr;
		sz = &memtest_rank1_size;
	}

	size = snprintf(buf, sizeof(buf), "0x%lx 0x%x\n", (unsigned long) *addr,
			(unsigned int) *sz);

	return simple_read_from_buffer(user_buf, len, offset, buf, size);
}

static int test_mem_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long pfn;
	size_t size = vma->vm_end - vma->vm_start;
	unsigned long rank = (unsigned long) file->private_data;
	phys_addr_t *addr;
	struct mutex *lock;
	unsigned int *sz;
	struct memtest_client *client;
	int ret = 0;

	if (rank == 0) {
		addr = &memtest_rank0_addr;
		sz = &memtest_rank0_size;
		lock = &test_mem0_mutex;
	} else {
		addr = &memtest_rank1_addr;
		sz = &memtest_rank1_size;
		lock = &test_mem1_mutex;
	}

	mutex_lock(lock);


	/* pr_info("%s(0): pgoff=0x%lx 0x%lx %lu\n", __func__, */
	/* vma->vm_pgoff, size, (unsigned long) file->private_data); */

	if (((vma->vm_pgoff << PAGE_SHIFT) + size) >= *sz) {
		ret = -EINVAL;
		goto end;
	}

	pfn = *addr >> PAGE_SHIFT;
	vma->vm_pgoff += pfn;
	/* pr_info("%s(1): pgoff=0x%lx 0x%lx %lu\n", __func__, */
	/* vma->vm_pgoff, size, (unsigned long) file->private_data); */

	if (!valid_mmap_phys_addr_range(vma->vm_pgoff, size)) {
		ret = -EINVAL;
		goto end;
	}

	if (!(vma->vm_flags & VM_MAYSHARE)) {
		ret = -EINVAL;
		goto end;
	}

	vma->vm_page_prot = phys_mem_access_prot(file, vma->vm_pgoff,
						 size,
						 vma->vm_page_prot);

	vma->vm_ops = &mmap_mem_ops;

	/* Remap-pfn-range will mark the range VM_IO */
	if (remap_pfn_range(vma,
			    vma->vm_start,
			    vma->vm_pgoff,
			    size,
			    vma->vm_page_prot)) {
		ret = -EAGAIN;
		goto end;
	}

end:
	mutex_unlock(lock);

	if (!ret) {
		mutex_lock(&test_client_mutex);
		if (test_client_num <= MAX_TEST_CLIENT) {
			client = kmalloc(sizeof(*client), GFP_KERNEL);

			if (client) {
				client->pid = current->pid;
				client->rank = rank;
				list_add_tail(&client->node, &test_client);
				test_client_num++;
			}
		}
		mutex_unlock(&test_client_mutex);
	}

	return ret;
}

static const struct file_operations test_mem_fops = {
	.owner = THIS_MODULE,
	.open = test_mem_open,
	.read = test_mem_read,
	.mmap = test_mem_mmap,
};

static int __init dram_memtest_interface_init(void)
{
	void __iomem *emi_base;

	result = TEST_RESULT_INIT;

	get_emi_base = (void __iomem *)symbol_get(mt_emi_base_get);
	if (get_emi_base == NULL) {
		pr_info("%s: mt_emi_base_get is NULL\n", __func__);
		return -1;
	}

	emi_base = get_emi_base();
	if (emi_base == NULL) {
		pr_info("%s: can't find EMI base\n", __func__);
		return -1;
	}

	memtest_dir = debugfs_create_dir("memtest", NULL);
	if (!memtest_dir) {
		pr_info("%s: create dir fail\n", __func__);
		return -1;
	}

	read_mr4 = debugfs_create_file("read_mr4", 0444,
			memtest_dir, NULL, &read_mr4_fops);
	if (!read_mr4) {
		pr_info("%s: create read_mr4 interface fail\n", __func__);
		return -1;
	}

	read_mr5 = debugfs_create_file("read_mr5", 0444,
			memtest_dir, NULL, &read_mr5_fops);
	if (!read_mr5) {
		pr_info("%s: create read_mr5 interface fail\n", __func__);
		return -1;
	}

	read_dram_addr = debugfs_create_file("read_dram_addr",
			0666,
			memtest_dir, NULL, &read_dram_addr_fops);
	if (!read_dram_addr) {
		pr_info("%s: create read_dram_addr interface fail\n", __func__);
		return -1;
	}

	memtest_result = debugfs_create_file("result",
			0666,
			memtest_dir, NULL, &test_result_fops);
	if (!memtest_result) {
		pr_info("%s: create result interface fail\n", __func__);
		return -1;
	}

	memtest_v2p = debugfs_create_file("v2p", 0444,
			memtest_dir, NULL, &test_v2p_fops);
	if (!memtest_v2p) {
		pr_info("%s: create v2p interface fail\n", __func__);
		return -1;
	}

	if (memtest_rank0_addr) {
		memtest_mem0 = debugfs_create_file("mem0",
				0666,
				memtest_dir, (void *) 0, &test_mem_fops);
		if (!memtest_mem0) {
			pr_info("%s: create mem0 interface fail\n", __func__);
			return -1;
		}
	}

	if (memtest_rank1_addr) {
		memtest_mem1 = debugfs_create_file("mem1",
				0666,
				memtest_dir, (void *) 1, &test_mem_fops);
		if (!memtest_mem1) {
			pr_info("%s: create mem1 interface fail\n", __func__);
			return -1;
		}
	}

	return 0;
}

late_initcall(dram_memtest_interface_init);
#endif
