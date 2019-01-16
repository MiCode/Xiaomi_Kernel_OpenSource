/*
 * kernel/power/tuxonice_prune.c
 *
 * Copyright (C) 2012 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * This file implements a TuxOnIce module that seeks to prune the
 * amount of data written to disk. It builds a table of hashes
 * of the uncompressed data, and writes the pfn of the previous page
 * with the same contents instead of repeating the data when a match
 * is found.
 */

#include <linux/suspend.h>
#include <linux/highmem.h>
#include <linux/vmalloc.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <crypto/hash.h>

#include "tuxonice_builtin.h"
#include "tuxonice.h"
#include "tuxonice_modules.h"
#include "tuxonice_sysfs.h"
#include "tuxonice_io.h"
#include "tuxonice_ui.h"
#include "tuxonice_alloc.h"

/*
 * We never write a page bigger than PAGE_SIZE, so use a large number
 * to indicate that data is a PFN.
 */
#define PRUNE_DATA_IS_PFN (PAGE_SIZE + 100)

static unsigned long toi_pruned_pages;

static struct toi_module_ops toi_prune_ops;
static struct toi_module_ops *next_driver;

static char toi_prune_hash_algo_name[32] = "sha1";

static DEFINE_MUTEX(stats_lock);

struct toi_cpu_context {
	struct shash_desc desc;
	char *digest;
};

#define OUT_BUF_SIZE (2 * PAGE_SIZE)

static DEFINE_PER_CPU(struct toi_cpu_context, contexts);

/*
 * toi_crypto_prepare
 *
 * Prepare to do some work by allocating buffers and transforms.
 */
static int toi_prune_crypto_prepare(void)
{
	int cpu, ret, digestsize;

	if (!*toi_prune_hash_algo_name) {
		printk(KERN_INFO "TuxOnIce: Pruning enabled but no " "hash algorithm set.\n");
		return 1;
	}

	for_each_online_cpu(cpu) {
		struct toi_cpu_context *this = &per_cpu(contexts, cpu);
		this->desc.tfm = crypto_alloc_shash(toi_prune_hash_algo_name, 0, 0);
		if (IS_ERR(this->desc.tfm)) {
			printk(KERN_INFO "TuxOnIce: Failed to allocate the "
			       "%s prune hash algorithm.\n", toi_prune_hash_algo_name);
			this->desc.tfm = NULL;
			return 1;
		}

		if (!digestsize)
			digestsize = crypto_shash_digestsize(this->desc.tfm);

		this->digest = kmalloc(digestsize, GFP_KERNEL);
		if (!this->digest) {
			printk(KERN_INFO "TuxOnIce: Failed to allocate space "
			       "for digest output.\n");
			crypto_free_shash(this->desc.tfm);
			this->desc.tfm = NULL;
		}

		this->desc.flags = 0;

		ret = crypto_shash_init(&this->desc);
		if (ret < 0) {
			printk(KERN_INFO "TuxOnIce: Failed to initialise the "
			       "%s prune hash algorithm.\n", toi_prune_hash_algo_name);
			kfree(this->digest);
			this->digest = NULL;
			crypto_free_shash(this->desc.tfm);
			this->desc.tfm = NULL;
			return 1;
		}
	}

	return 0;
}

static int toi_prune_rw_cleanup(int writing)
{
	int cpu;

	for_each_online_cpu(cpu) {
		struct toi_cpu_context *this = &per_cpu(contexts, cpu);
		if (this->desc.tfm) {
			crypto_free_shash(this->desc.tfm);
			this->desc.tfm = NULL;
		}

		if (this->digest) {
			kfree(this->digest);
			this->digest = NULL;
		}
	}

	return 0;
}

/*
 * toi_prune_init
 */

static int toi_prune_init(int toi_or_resume)
{
	if (!toi_or_resume)
		return 0;

	toi_pruned_pages = 0;

	next_driver = toi_get_next_filter(&toi_prune_ops);

	return next_driver ? 0 : -ECHILD;
}

/*
 * toi_prune_rw_init()
 */

static int toi_prune_rw_init(int rw, int stream_number)
{
	if (toi_prune_crypto_prepare()) {
		printk(KERN_ERR "Failed to initialise prune " "algorithm.\n");
		if (rw == READ) {
			printk(KERN_INFO "Unable to read the image.\n");
			return -ENODEV;
		} else {
			printk(KERN_INFO "Continuing without " "pruning the image.\n");
			toi_prune_ops.enabled = 0;
		}
	}

	return 0;
}

/*
 * toi_prune_write_page()
 *
 * Compress a page of data, buffering output and passing on filled
 * pages to the next module in the pipeline.
 *
 * Buffer_page:	Pointer to a buffer of size PAGE_SIZE, containing
 * data to be checked.
 *
 * Returns:	0 on success. Otherwise the error is that returned by later
 *		modules, -ECHILD if we have a broken pipeline or -EIO if
 *		zlib errs.
 */
static int toi_prune_write_page(unsigned long index, int buf_type,
				void *buffer_page, unsigned int buf_size)
{
	int ret = 0, cpu = smp_processor_id(), write_data = 1;
	struct toi_cpu_context *ctx = &per_cpu(contexts, cpu);
	u8 *output_buffer = buffer_page;
	int output_len = buf_size;
	int out_buf_type = buf_type;
	void *buffer_start;
	u32 buf[4];

	if (ctx->desc.tfm) {

		buffer_start = TOI_MAP(buf_type, buffer_page);
		ctx->len = OUT_BUF_SIZE;

		ret = crypto_shash_digest(&ctx->desc, buffer_start, buf_size, &ctx->digest);
		if (ret) {
			printk(KERN_INFO "TuxOnIce: Failed to calculate digest (%d).\n", ret);
		} else {
			mutex_lock(&stats_lock);

			toi_pruned_pages++;

			mutex_unlock(&stats_lock);

		}

		TOI_UNMAP(buf_type, buffer_page);
	}

	if (write_data)
		ret = next_driver->write_page(index, out_buf_type, output_buffer, output_len);
	else
		ret = next_driver->write_page(index, out_buf_type, output_buffer, output_len);

	return ret;
}

/*
 * toi_prune_read_page()
 * @buffer_page: struct page *. Pointer to a buffer of size PAGE_SIZE.
 *
 * Retrieve data from later modules or from a previously loaded page and
 * fill the input buffer.
 * Zero if successful. Error condition from me or from downstream on failure.
 */
static int toi_prune_read_page(unsigned long *index, int buf_type,
			       void *buffer_page, unsigned int *buf_size)
{
	int ret, cpu = smp_processor_id();
	unsigned int len;
	char *buffer_start;
	struct toi_cpu_context *ctx = &per_cpu(contexts, cpu);

	if (!ctx->desc.tfm)
		return next_driver->read_page(index, TOI_PAGE, buffer_page, buf_size);

	/*
	 * All our reads must be synchronous - we can't handle
	 * data that hasn't been read yet.
	 */

	ret = next_driver->read_page(index, buf_type, buffer_page, &len);

	if (len == PRUNE_DATA_IS_PFN) {
		buffer_start = kmap(buffer_page);
	}

	return ret;
}

/*
 * toi_prune_print_debug_stats
 * @buffer: Pointer to a buffer into which the debug info will be printed.
 * @size: Size of the buffer.
 *
 * Print information to be recorded for debugging purposes into a buffer.
 * Returns: Number of characters written to the buffer.
 */

static int toi_prune_print_debug_stats(char *buffer, int size)
{
	int len;

	/* Output the number of pages pruned. */
	if (*toi_prune_hash_algo_name)
		len = scnprintf(buffer, size, "- Compressor is '%s'.\n", toi_prune_hash_algo_name);
	else
		len = scnprintf(buffer, size, "- Compressor is not set.\n");

	if (toi_pruned_pages)
		len += scnprintf(buffer + len, size - len, "  Pruned "
				 "%lu pages).\n", toi_pruned_pages);
	return len;
}

/*
 * toi_prune_memory_needed
 *
 * Tell the caller how much memory we need to operate during hibernate/resume.
 * Returns: Unsigned long. Maximum number of bytes of memory required for
 * operation.
 */
static int toi_prune_memory_needed(void)
{
	return 2 * PAGE_SIZE;
}

static int toi_prune_storage_needed(void)
{
	return 2 * sizeof(unsigned long) + 2 * sizeof(int) + strlen(toi_prune_hash_algo_name) + 1;
}

/*
 * toi_prune_save_config_info
 * @buffer: Pointer to a buffer of size PAGE_SIZE.
 *
 * Save informaton needed when reloading the image at resume time.
 * Returns: Number of bytes used for saving our data.
 */
static int toi_prune_save_config_info(char *buffer)
{
	int len = strlen(toi_prune_hash_algo_name) + 1, offset = 0;

	*((unsigned long *)buffer) = toi_pruned_pages;
	offset += sizeof(unsigned long);
	*((int *)(buffer + offset)) = len;
	offset += sizeof(int);
	strncpy(buffer + offset, toi_prune_hash_algo_name, len);
	return offset + len;
}

/* toi_prune_load_config_info
 * @buffer: Pointer to the start of the data.
 * @size: Number of bytes that were saved.
 *
 * Description:	Reload information needed for passing back to the
 * resumed kernel.
 */
static void toi_prune_load_config_info(char *buffer, int size)
{
	int len, offset = 0;

	toi_pruned_pages = *((unsigned long *)buffer);
	offset += sizeof(unsigned long);
	len = *((int *)(buffer + offset));
	offset += sizeof(int);
	strncpy(toi_prune_hash_algo_name, buffer + offset, len);
}

static void toi_prune_pre_atomic_restore(struct toi_boot_kernel_data *bkd)
{
	bkd->pruned_pages = toi_pruned_pages;
}

static void toi_prune_post_atomic_restore(struct toi_boot_kernel_data *bkd)
{
	toi_pruned_pages = bkd->pruned_pages;
}

/*
 * toi_expected_ratio
 *
 * Description:	Returns the expected ratio between data passed into this module
 *		and the amount of data output when writing.
 * Returns:	100 - we have no idea how many pages will be pruned.
 */

static int toi_prune_expected_ratio(void)
{
	return 100;
}

/*
 * data for our sysfs entries.
 */
static struct toi_sysfs_data sysfs_params[] = {
	SYSFS_INT("enabled", SYSFS_RW, &toi_prune_ops.enabled, 0, 1, 0,
		  NULL),
	SYSFS_STRING("algorithm", SYSFS_RW, toi_prune_hash_algo_name, 31, 0, NULL),
};

/*
 * Ops structure.
 */
static struct toi_module_ops toi_prune_ops = {
	.type = FILTER_MODULE,
	.name = "prune",
	.directory = "prune",
	.module = THIS_MODULE,
	.initialise = toi_prune_init,
	.memory_needed = toi_prune_memory_needed,
	.print_debug_info = toi_prune_print_debug_stats,
	.save_config_info = toi_prune_save_config_info,
	.load_config_info = toi_prune_load_config_info,
	.storage_needed = toi_prune_storage_needed,
	.expected_compression = toi_prune_expected_ratio,

	.pre_atomic_restore = toi_prune_pre_atomic_restore,
	.post_atomic_restore = toi_prune_post_atomic_restore,

	.rw_init = toi_prune_rw_init,
	.rw_cleanup = toi_prune_rw_cleanup,

	.write_page = toi_prune_write_page,
	.read_page = toi_prune_read_page,

	.sysfs_data = sysfs_params,
	.num_sysfs_entries = sizeof(sysfs_params) / sizeof(struct toi_sysfs_data),
};

/* ---- Registration ---- */

static __init int toi_prune_load(void)
{
	return toi_register_module(&toi_prune_ops);
}

#ifdef MODULE
static __exit void toi_prune_unload(void)
{
	toi_unregister_module(&toi_prune_ops);
}
module_init(toi_prune_load);
module_exit(toi_prune_unload);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nigel Cunningham");
MODULE_DESCRIPTION("Image Pruning Support for TuxOnIce");
#else
late_initcall(toi_prune_load);
#endif
