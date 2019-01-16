/*
 * kernel/power/incremental.c
 *
 * Copyright (C) 2012 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * This file contains routines related to storing incremental images - that
 * is, retaining an image after an initial cycle and then storing incremental
 * changes on subsequent hibernations.
 */

#include <linux/suspend.h>
#include <linux/highmem.h>
#include <linux/vmalloc.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>

#include "tuxonice_builtin.h"
#include "tuxonice.h"
#include "tuxonice_modules.h"
#include "tuxonice_sysfs.h"
#include "tuxonice_io.h"
#include "tuxonice_ui.h"
#include "tuxonice_alloc.h"

static struct toi_module_ops toi_incremental_ops;
static struct toi_module_ops *next_driver;
static unsigned long toi_incremental_bytes_in, toi_incremental_bytes_out;

static char toi_incremental_slow_cmp_name[32] = "sha1";
static int toi_incremental_digestsize;

static DEFINE_MUTEX(stats_lock);

struct toi_cpu_context {
	u8 *buffer_start;
	struct hash_desc desc;
	struct scatterlist sg[1];
	unsigned char *digest;
};

#define OUT_BUF_SIZE (2 * PAGE_SIZE)

static DEFINE_PER_CPU(struct toi_cpu_context, contexts);

/*
 * toi_crypto_prepare
 *
 * Prepare to do some work by allocating buffers and transforms.
 */
static int toi_incremental_crypto_prepare(void)
{
	int cpu, digestsize = toi_incremental_digestsize;

	if (!*toi_incremental_slow_cmp_name) {
		printk(KERN_INFO "TuxOnIce: Incremental image support enabled but no "
		       "hash algorithm set.\n");
		return 1;
	}

	for_each_online_cpu(cpu) {
		struct toi_cpu_context *this = &per_cpu(contexts, cpu);
		this->desc.tfm = crypto_alloc_hash(toi_incremental_slow_cmp_name, 0, 0);
		if (IS_ERR(this->desc.tfm)) {
			printk(KERN_INFO "TuxOnIce: Failed to initialise the "
			       "%s hashing transform.\n", toi_incremental_slow_cmp_name);
			this->desc.tfm = NULL;
			return 1;
		}

		if (!digestsize) {
			digestsize = crypto_hash_digestsize(this->desc.tfm);
			toi_incremental_digestsize = digestsize;
		}

		this->digest = toi_kzalloc(16, digestsize, GFP_KERNEL);
		if (!this->digest)
			return -ENOMEM;

		this->desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;
	}

	return 0;
}

static int toi_incremental_rw_cleanup(int writing)
{
	int cpu;

	for_each_online_cpu(cpu) {
		struct toi_cpu_context *this = &per_cpu(contexts, cpu);
		if (this->desc.tfm) {
			crypto_free_hash(this->desc.tfm);
			this->desc.tfm = NULL;
		}

		if (this->digest) {
			toi_kfree(16, this->digest, toi_incremental_digestsize);
			this->digest = NULL;
		}
	}

	return 0;
}

/*
 * toi_incremental_init
 */

static int toi_incremental_init(int hibernate_or_resume)
{
	if (!hibernate_or_resume)
		return 0;

	next_driver = toi_get_next_filter(&toi_incremental_ops);

	return next_driver ? 0 : -ECHILD;
}

/*
 * toi_incremental_rw_init()
 */

static int toi_incremental_rw_init(int rw, int stream_number)
{
	if (rw == WRITE && toi_incremental_crypto_prepare()) {
		printk(KERN_ERR "Failed to initialise hashing " "algorithm.\n");
		if (rw == READ) {
			printk(KERN_INFO "Unable to read the image.\n");
			return -ENODEV;
		} else {
			printk(KERN_INFO "Continuing without "
			       " calculating an incremental image.\n");
			toi_incremental_ops.enabled = 0;
		}
	}

	return 0;
}

/*
 * toi_incremental_write_page()
 *
 * Decide whether to write a page to the image. Calculate the SHA1 (or something
 * else if the user changes the hashing algo) of the page and compare it to the
 * previous value (if any). If there was no previous value or the values are
 * different, write the page. Otherwise, skip the write.
 *
 * @TODO: Clear hashes for pages that are no longer in the image!
 *
 * Buffer_page:	Pointer to a buffer of size PAGE_SIZE, containing
 * data to be written.
 *
 * Returns:	0 on success. Otherwise the error is that returned by later
 *		modules, -ECHILD if we have a broken pipeline or -EIO if
 *		zlib errs.
 */
static int toi_incremental_write_page(unsigned long index, int buf_type,
				      void *buffer_page, unsigned int buf_size)
{
	int ret = 0, cpu = smp_processor_id();
	struct toi_cpu_context *ctx = &per_cpu(contexts, cpu);
	int to_write = true;

	if (ctx->desc.tfm) {
		/* char *old_hash; */

		ctx->buffer_start = TOI_MAP(buf_type, buffer_page);

		sg_init_one(&ctx->sg[0], ctx->buffer_start, buf_size);

		ret = crypto_hash_digest(&ctx->desc, &ctx->sg[0], ctx->sg[0].length, ctx->digest);
		/* old_hash = get_old_hash(index); */

		TOI_UNMAP(buf_type, buffer_page);

#if 0
		if (!ret && new_hash == old_hash) {
			to_write = false;
		} else
			store_hash(ctx, index, new_hash);
#endif
	}

	mutex_lock(&stats_lock);

	toi_incremental_bytes_in += buf_size;
	if (ret || to_write)
		toi_incremental_bytes_out += buf_size;

	mutex_unlock(&stats_lock);

	if (ret || to_write) {
		int ret2 = next_driver->write_page(index, buf_type,
						   buffer_page, buf_size);
		if (!ret)
			ret = ret2;
	}

	return ret;
}

/*
 * toi_incremental_read_page()
 * @buffer_page: struct page *. Pointer to a buffer of size PAGE_SIZE.
 *
 * Nothing extra to do here.
 */
static int toi_incremental_read_page(unsigned long *index, int buf_type,
				     void *buffer_page, unsigned int *buf_size)
{
	return next_driver->read_page(index, TOI_PAGE, buffer_page, buf_size);
}

/*
 * toi_incremental_print_debug_stats
 * @buffer: Pointer to a buffer into which the debug info will be printed.
 * @size: Size of the buffer.
 *
 * Print information to be recorded for debugging purposes into a buffer.
 * Returns: Number of characters written to the buffer.
 */

static int toi_incremental_print_debug_stats(char *buffer, int size)
{
	unsigned long pages_in = toi_incremental_bytes_in >> PAGE_SHIFT,
	    pages_out = toi_incremental_bytes_out >> PAGE_SHIFT;
	int len;

	/* Output the size of the incremental image. */
	if (*toi_incremental_slow_cmp_name)
		len = scnprintf(buffer, size, "- Hash algorithm is '%s'.\n",
				toi_incremental_slow_cmp_name);
	else
		len = scnprintf(buffer, size, "- Hash algorithm is not set.\n");

	if (pages_in)
		len += scnprintf(buffer + len, size - len, "  Incremental image "
				 "%lu of %lu bytes (%ld percent).\n",
				 toi_incremental_bytes_out,
				 toi_incremental_bytes_in, pages_out * 100 / pages_in);
	return len;
}

/*
 * toi_incremental_memory_needed
 *
 * Tell the caller how much memory we need to operate during hibernate/resume.
 * Returns: Unsigned long. Maximum number of bytes of memory required for
 * operation.
 */
static int toi_incremental_memory_needed(void)
{
	return 2 * PAGE_SIZE;
}

static int toi_incremental_storage_needed(void)
{
	return 2 * sizeof(unsigned long) + sizeof(int) + strlen(toi_incremental_slow_cmp_name) + 1;
}

/*
 * toi_incremental_save_config_info
 * @buffer: Pointer to a buffer of size PAGE_SIZE.
 *
 * Save informaton needed when reloading the image at resume time.
 * Returns: Number of bytes used for saving our data.
 */
static int toi_incremental_save_config_info(char *buffer)
{
	int len = strlen(toi_incremental_slow_cmp_name) + 1, offset = 0;

	*((unsigned long *)buffer) = toi_incremental_bytes_in;
	offset += sizeof(unsigned long);
	*((unsigned long *)(buffer + offset)) = toi_incremental_bytes_out;
	offset += sizeof(unsigned long);
	*((int *)(buffer + offset)) = len;
	offset += sizeof(int);
	strncpy(buffer + offset, toi_incremental_slow_cmp_name, len);
	return offset + len;
}

/* toi_incremental_load_config_info
 * @buffer: Pointer to the start of the data.
 * @size: Number of bytes that were saved.
 *
 * Description:	Reload information to be retained for debugging info.
 */
static void toi_incremental_load_config_info(char *buffer, int size)
{
	int len, offset = 0;

	toi_incremental_bytes_in = *((unsigned long *)buffer);
	offset += sizeof(unsigned long);
	toi_incremental_bytes_out = *((unsigned long *)(buffer + offset));
	offset += sizeof(unsigned long);
	len = *((int *)(buffer + offset));
	offset += sizeof(int);
	strncpy(toi_incremental_slow_cmp_name, buffer + offset, len);
}

static void toi_incremental_pre_atomic_restore(struct toi_boot_kernel_data *bkd)
{
	bkd->incremental_bytes_in = toi_incremental_bytes_in;
	bkd->incremental_bytes_out = toi_incremental_bytes_out;
}

static void toi_incremental_post_atomic_restore(struct toi_boot_kernel_data *bkd)
{
	toi_incremental_bytes_in = bkd->incremental_bytes_in;
	toi_incremental_bytes_out = bkd->incremental_bytes_out;
}

static void toi_incremental_algo_change(void)
{
	/* Reset so it's gotten from crypto_hash_digestsize afresh */
	toi_incremental_digestsize = 0;
}

/*
 * data for our sysfs entries.
 */
static struct toi_sysfs_data sysfs_params[] = {
	SYSFS_INT("enabled", SYSFS_RW, &toi_incremental_ops.enabled, 0, 1, 0,
		  NULL),
	SYSFS_STRING("algorithm", SYSFS_RW, toi_incremental_slow_cmp_name, 31, 0,
		     toi_incremental_algo_change),
};

/*
 * Ops structure.
 */
static struct toi_module_ops toi_incremental_ops = {
	.type = FILTER_MODULE,
	.name = "incremental",
	.directory = "incremental",
	.module = THIS_MODULE,
	.initialise = toi_incremental_init,
	.memory_needed = toi_incremental_memory_needed,
	.print_debug_info = toi_incremental_print_debug_stats,
	.save_config_info = toi_incremental_save_config_info,
	.load_config_info = toi_incremental_load_config_info,
	.storage_needed = toi_incremental_storage_needed,

	.pre_atomic_restore = toi_incremental_pre_atomic_restore,
	.post_atomic_restore = toi_incremental_post_atomic_restore,

	.rw_init = toi_incremental_rw_init,
	.rw_cleanup = toi_incremental_rw_cleanup,

	.write_page = toi_incremental_write_page,
	.read_page = toi_incremental_read_page,

	.sysfs_data = sysfs_params,
	.num_sysfs_entries = sizeof(sysfs_params) / sizeof(struct toi_sysfs_data),
};

/* ---- Registration ---- */

static __init int toi_incremental_load(void)
{
	return toi_register_module(&toi_incremental_ops);
}

#ifdef MODULE
static __exit void toi_incremental_unload(void)
{
	toi_unregister_module(&toi_incremental_ops);
}
module_init(toi_incremental_load);
module_exit(toi_incremental_unload);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nigel Cunningham");
MODULE_DESCRIPTION("Incremental Image Support for TuxOnIce");
#else
late_initcall(toi_incremental_load);
#endif
