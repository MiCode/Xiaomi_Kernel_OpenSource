/*
 * kernel/power/compression.c
 *
 * Copyright (C) 2003-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * This file contains data compression routines for TuxOnIce,
 * using cryptoapi.
 */

#include <linux/suspend.h>
#include <linux/highmem.h>
#include <linux/vmalloc.h>
#include <linux/crypto.h>

#include "tuxonice_builtin.h"
#include "tuxonice.h"
#include "tuxonice_modules.h"
#include "tuxonice_sysfs.h"
#include "tuxonice_io.h"
#include "tuxonice_ui.h"
#include "tuxonice_alloc.h"

static int toi_expected_compression;
#ifdef CONFIG_TOI_ENHANCE
static int toi_actual_compression;
#endif

static struct toi_module_ops toi_compression_ops;
static struct toi_module_ops *next_driver;

static char toi_compressor_name[32] = "lzo";

static DEFINE_MUTEX(stats_lock);

struct toi_cpu_context {
	u8 *page_buffer;
	struct crypto_comp *transform;
	unsigned int len;
	u8 *buffer_start;
	u8 *output_buffer;
};

#define OUT_BUF_SIZE (2 * PAGE_SIZE)

static DEFINE_PER_CPU(struct toi_cpu_context, contexts);

/*
 * toi_crypto_prepare
 *
 * Prepare to do some work by allocating buffers and transforms.
 */
static int toi_compress_crypto_prepare(void)
{
	int cpu;

	if (!*toi_compressor_name) {
		printk(KERN_INFO "TuxOnIce: Compression enabled but no " "compressor name set.\n");
		return 1;
	}

	for_each_online_cpu(cpu) {
		struct toi_cpu_context *this = &per_cpu(contexts, cpu);
		this->transform = crypto_alloc_comp(toi_compressor_name, 0, 0);
		if (IS_ERR(this->transform)) {
			printk(KERN_INFO "TuxOnIce: Failed to initialise the "
			       "%s compression transform.\n", toi_compressor_name);
			this->transform = NULL;
			return 1;
		}

		this->page_buffer = (char *)toi_get_zeroed_page(16, TOI_ATOMIC_GFP);

		if (!this->page_buffer) {
			printk(KERN_ERR
			       "Failed to allocate a page buffer for TuxOnIce "
			       "compression driver.\n");
			return -ENOMEM;
		}

		this->output_buffer = (char *)vmalloc_32(OUT_BUF_SIZE);

		if (!this->output_buffer) {
			printk(KERN_ERR
			       "Failed to allocate a output buffer for TuxOnIce "
			       "compression driver.\n");
			return -ENOMEM;
		}
	}

	return 0;
}

static int toi_compress_rw_cleanup(int writing)
{
	int cpu;

	for_each_online_cpu(cpu) {
		struct toi_cpu_context *this = &per_cpu(contexts, cpu);
		if (this->transform) {
			crypto_free_comp(this->transform);
			this->transform = NULL;
		}

		if (this->page_buffer)
			toi_free_page(16, (unsigned long)this->page_buffer);

		this->page_buffer = NULL;

		if (this->output_buffer)
			vfree(this->output_buffer);

		this->output_buffer = NULL;
	}

	return 0;
}

/*
 * toi_compress_init
 */

static int toi_compress_init(int toi_or_resume)
{
	if (!toi_or_resume)
		return 0;

	toi_compress_bytes_in = 0;
	toi_compress_bytes_out = 0;

	next_driver = toi_get_next_filter(&toi_compression_ops);

	return next_driver ? 0 : -ECHILD;
}

/*
 * toi_compress_rw_init()
 */

static int toi_compress_rw_init(int rw, int stream_number)
{
	if (toi_compress_crypto_prepare()) {
		printk(KERN_ERR "Failed to initialise compression " "algorithm.\n");
		if (rw == READ) {
			printk(KERN_INFO "Unable to read the image.\n");
			return -ENODEV;
		} else {
			printk(KERN_INFO "Continuing without " "compressing the image.\n");
			toi_compression_ops.enabled = 0;
		}
	}

	return 0;
}

/*
 * toi_compress_write_page()
 *
 * Compress a page of data, buffering output and passing on filled
 * pages to the next module in the pipeline.
 *
 * Buffer_page:	Pointer to a buffer of size PAGE_SIZE, containing
 * data to be compressed.
 *
 * Returns:	0 on success. Otherwise the error is that returned by later
 *		modules, -ECHILD if we have a broken pipeline or -EIO if
 *		zlib errs.
 */
static int toi_compress_write_page(unsigned long index, int buf_type,
				   void *buffer_page, unsigned int buf_size)
{
	int ret = 0, cpu = smp_processor_id();
	struct toi_cpu_context *ctx = &per_cpu(contexts, cpu);
	u8 *output_buffer = buffer_page;
	int output_len = buf_size;
	int out_buf_type = buf_type;

	if (ctx->transform) {

		ctx->buffer_start = TOI_MAP(buf_type, buffer_page);
		ctx->len = OUT_BUF_SIZE;

		ret = crypto_comp_compress(ctx->transform,
					   ctx->buffer_start, buf_size,
					   ctx->output_buffer, &ctx->len);

		TOI_UNMAP(buf_type, buffer_page);

		toi_message(TOI_COMPRESS, TOI_VERBOSE, 0,
			    "CPU %d, index %lu: %d bytes", cpu, index, ctx->len);

		if (!ret && ctx->len < buf_size) {	/* some compression */
			output_buffer = ctx->output_buffer;
			output_len = ctx->len;
			out_buf_type = TOI_VIRT;
		}

	}

	mutex_lock(&stats_lock);

	toi_compress_bytes_in += buf_size;
	toi_compress_bytes_out += output_len;

	mutex_unlock(&stats_lock);

	if (!ret)
		ret = next_driver->write_page(index, out_buf_type, output_buffer, output_len);

	return ret;
}

/*
 * toi_compress_read_page()
 * @buffer_page: struct page *. Pointer to a buffer of size PAGE_SIZE.
 *
 * Retrieve data from later modules and decompress it until the input buffer
 * is filled.
 * Zero if successful. Error condition from me or from downstream on failure.
 */
static int toi_compress_read_page(unsigned long *index, int buf_type,
				  void *buffer_page, unsigned int *buf_size)
{
	int ret, cpu = smp_processor_id();
	unsigned int len;
	unsigned int outlen = PAGE_SIZE;
	char *buffer_start;
	struct toi_cpu_context *ctx = &per_cpu(contexts, cpu);

	if (!ctx->transform)
		return next_driver->read_page(index, TOI_PAGE, buffer_page, buf_size);

	/*
	 * All our reads must be synchronous - we can't decompress
	 * data that hasn't been read yet.
	 */

	ret = next_driver->read_page(index, TOI_VIRT, ctx->page_buffer, &len);

	buffer_start = kmap(buffer_page);

	/* Error or uncompressed data */
	if (ret || len == PAGE_SIZE) {
		memcpy(buffer_start, ctx->page_buffer, len);
		goto out;
	}

	ret = crypto_comp_decompress(ctx->transform, ctx->page_buffer, len, buffer_start, &outlen);

	toi_message(TOI_COMPRESS, TOI_VERBOSE, 0,
		    "CPU %d, index %lu: %d=>%d (%d).", cpu, *index, len, outlen, ret);

	if (ret)
		abort_hibernate(TOI_FAILED_IO, "Compress_read returned %d.\n", ret);
	else if (outlen != PAGE_SIZE) {
		abort_hibernate(TOI_FAILED_IO,
				"Decompression yielded %d bytes instead of %ld.\n",
				outlen, PAGE_SIZE);
		printk(KERN_ERR "Decompression yielded %d bytes instead of "
		       "%ld.\n", outlen, PAGE_SIZE);
		ret = -EIO;
		*buf_size = outlen;
	}
 out:
	TOI_UNMAP(buf_type, buffer_page);
	return ret;
}

/*
 * toi_compress_print_debug_stats
 * @buffer: Pointer to a buffer into which the debug info will be printed.
 * @size: Size of the buffer.
 *
 * Print information to be recorded for debugging purposes into a buffer.
 * Returns: Number of characters written to the buffer.
 */

static int toi_compress_print_debug_stats(char *buffer, int size)
{
	unsigned long pages_in = toi_compress_bytes_in >> PAGE_SHIFT,
	    pages_out = toi_compress_bytes_out >> PAGE_SHIFT;
	int len;

	/* Output the compression ratio achieved. */
	if (*toi_compressor_name)
		len = scnprintf(buffer, size, "- Compressor is '%s'.\n", toi_compressor_name);
	else
		len = scnprintf(buffer, size, "- Compressor is not set.\n");

	if (pages_in)
		len += scnprintf(buffer + len, size - len, "  Compressed "
				 "%lu bytes into %lu (%ld percent compression).\n",
				 toi_compress_bytes_in,
				 toi_compress_bytes_out, (pages_in - pages_out) * 100 / pages_in);
	return len;
}

/*
 * toi_compress_compression_memory_needed
 *
 * Tell the caller how much memory we need to operate during hibernate/resume.
 * Returns: Unsigned long. Maximum number of bytes of memory required for
 * operation.
 */
static int toi_compress_memory_needed(void)
{
	return 2 * PAGE_SIZE;
}

static int toi_compress_storage_needed(void)
{
	return 2 * sizeof(unsigned long) + 2 * sizeof(int) + strlen(toi_compressor_name) + 1;
}

/*
 * toi_compress_save_config_info
 * @buffer: Pointer to a buffer of size PAGE_SIZE.
 *
 * Save informaton needed when reloading the image at resume time.
 * Returns: Number of bytes used for saving our data.
 */
static int toi_compress_save_config_info(char *buffer)
{
	int len = strlen(toi_compressor_name) + 1, offset = 0;

	*((unsigned long *)buffer) = toi_compress_bytes_in;
	offset += sizeof(unsigned long);
	*((unsigned long *)(buffer + offset)) = toi_compress_bytes_out;
	offset += sizeof(unsigned long);
	*((int *)(buffer + offset)) = toi_expected_compression;
	offset += sizeof(int);
	*((int *)(buffer + offset)) = len;
	offset += sizeof(int);
	strncpy(buffer + offset, toi_compressor_name, len);
	return offset + len;
}

/* toi_compress_load_config_info
 * @buffer: Pointer to the start of the data.
 * @size: Number of bytes that were saved.
 *
 * Description:	Reload information needed for decompressing the image at
 * resume time.
 */
static void toi_compress_load_config_info(char *buffer, int size)
{
	int len, offset = 0;

	toi_compress_bytes_in = *((unsigned long *)buffer);
	offset += sizeof(unsigned long);
	toi_compress_bytes_out = *((unsigned long *)(buffer + offset));
	offset += sizeof(unsigned long);
	toi_expected_compression = *((int *)(buffer + offset));
	offset += sizeof(int);
	len = *((int *)(buffer + offset));
	offset += sizeof(int);
	strncpy(toi_compressor_name, buffer + offset, len);
}

static void toi_compress_pre_atomic_restore(struct toi_boot_kernel_data *bkd)
{
	bkd->compress_bytes_in = toi_compress_bytes_in;
	bkd->compress_bytes_out = toi_compress_bytes_out;
}

static void toi_compress_post_atomic_restore(struct toi_boot_kernel_data *bkd)
{
	toi_compress_bytes_in = bkd->compress_bytes_in;
	toi_compress_bytes_out = bkd->compress_bytes_out;
}

/*
 * toi_expected_compression_ratio
 *
 * Description:	Returns the expected ratio between data passed into this module
 *		and the amount of data output when writing.
 * Returns:	100 if the module is disabled. Otherwise the value set by the
 *		user via our sysfs entry.
 */

static int toi_compress_expected_ratio(void)
{
	if (!toi_compression_ops.enabled)
		return 100;
	else
		return 100 - toi_expected_compression;
}

#ifdef CONFIG_TOI_ENHANCE
/*
 * toi_actual_compression_ratio
 *
 * Description:	Returns the actual ratio of the lastest compression result.
 * Returns:	0 if the module is disabled.
 */
static int toi_compress_actual_ratio(void)
{
	unsigned long pages_in = toi_compress_bytes_in >> PAGE_SHIFT,
	    pages_out = toi_compress_bytes_out >> PAGE_SHIFT;

	toi_actual_compression = 0;
	if (!toi_compression_ops.enabled)
		toi_actual_compression = 0;
	else if (pages_in > 0 && (pages_in - pages_out >= 0))
		toi_actual_compression = (pages_in - pages_out) * 100 / pages_in;

	pr_warn("[%s] actual compressed ratio %d (%lu/%lu)\n", __func__,
			toi_actual_compression, pages_in, pages_out);
	return toi_actual_compression;
}
#endif

/*
 * data for our sysfs entries.
 */
static struct toi_sysfs_data sysfs_params[] = {
	SYSFS_INT("expected_compression", SYSFS_RW, &toi_expected_compression,
		  0, 99, 0, NULL),
	SYSFS_INT("enabled", SYSFS_RW, &toi_compression_ops.enabled, 0, 1, 0,
		  NULL),
	SYSFS_STRING("algorithm", SYSFS_RW, toi_compressor_name, 31, 0, NULL),
#ifdef CONFIG_TOI_ENHANCE
	SYSFS_INT("actual_compression", SYSFS_READONLY, &toi_actual_compression,
		  0, 99, 0, NULL),
#endif
};

/*
 * Ops structure.
 */
static struct toi_module_ops toi_compression_ops = {
	.type = FILTER_MODULE,
	.name = "compression",
	.directory = "compression",
	.module = THIS_MODULE,
	.initialise = toi_compress_init,
	.memory_needed = toi_compress_memory_needed,
	.print_debug_info = toi_compress_print_debug_stats,
	.save_config_info = toi_compress_save_config_info,
	.load_config_info = toi_compress_load_config_info,
	.storage_needed = toi_compress_storage_needed,
	.expected_compression = toi_compress_expected_ratio,
#ifdef CONFIG_TOI_ENHANCE
	.actual_compression = toi_compress_actual_ratio,
#endif

	.pre_atomic_restore = toi_compress_pre_atomic_restore,
	.post_atomic_restore = toi_compress_post_atomic_restore,

	.rw_init = toi_compress_rw_init,
	.rw_cleanup = toi_compress_rw_cleanup,

	.write_page = toi_compress_write_page,
	.read_page = toi_compress_read_page,

	.sysfs_data = sysfs_params,
	.num_sysfs_entries = sizeof(sysfs_params) / sizeof(struct toi_sysfs_data),
};

/* ---- Registration ---- */

static __init int toi_compress_load(void)
{
	return toi_register_module(&toi_compression_ops);
}

#ifdef MODULE
static __exit void toi_compress_unload(void)
{
	toi_unregister_module(&toi_compression_ops);
}
module_init(toi_compress_load);
module_exit(toi_compress_unload);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nigel Cunningham");
MODULE_DESCRIPTION("Compression Support for TuxOnIce");
#else
late_initcall(toi_compress_load);
#endif
