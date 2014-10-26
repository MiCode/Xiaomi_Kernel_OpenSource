/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/idr.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/completion.h>

#include <mach/msm_ipc_logging.h>

#include "ipc_logging.h"

static LIST_HEAD(ipc_log_context_list);
static DEFINE_RWLOCK(ipc_log_context_list_lock);
static atomic_t next_log_id = ATOMIC_INIT(0);
static void *get_deserialization_func(struct ipc_log_context *ilctxt,
				      int type);

static struct ipc_log_page *get_first_page(struct ipc_log_context *ilctxt)
{
	struct ipc_log_page_header *p_pghdr;
	struct ipc_log_page *pg = NULL;

	if (!ilctxt)
		return NULL;
	p_pghdr = list_first_entry(&ilctxt->page_list,
				   struct ipc_log_page_header, list);
	pg = container_of(p_pghdr, struct ipc_log_page, hdr);
	return pg;
}

/**
 * is_ilctxt_empty - Returns true if no data is available to read in log
 *
 * @ilctxt: logging context
 * @returns: > 1 if context is empty; 0 if not empty; <0 for failure
 */
static int is_ilctxt_empty(struct ipc_log_context *ilctxt)
{
	if (!ilctxt)
		return -EINVAL;

	return ((ilctxt->read_page == ilctxt->write_page) &&
		(ilctxt->read_page->hdr.read_offset ==
		 ilctxt->write_page->hdr.write_offset));
}

static struct ipc_log_page *get_next_page(struct ipc_log_context *ilctxt,
					  struct ipc_log_page *cur_pg)
{
	struct ipc_log_page_header *p_pghdr;
	struct ipc_log_page *pg = NULL;

	if (!ilctxt || !cur_pg)
		return NULL;

	if (ilctxt->last_page == cur_pg)
		return ilctxt->first_page;

	p_pghdr = list_first_entry(&cur_pg->hdr.list,
			struct ipc_log_page_header, list);
	pg = container_of(p_pghdr, struct ipc_log_page, hdr);

	return pg;
}

/* If data == NULL, drop the log of size data_size*/
static void ipc_log_read(struct ipc_log_context *ilctxt,
			 void *data, int data_size)
{
	int bytes_to_read;

	bytes_to_read = MIN(((PAGE_SIZE - sizeof(struct ipc_log_page_header))
				- ilctxt->read_page->hdr.read_offset),
			      data_size);
	if (data)
		memcpy(data, (ilctxt->read_page->data +
			ilctxt->read_page->hdr.read_offset), bytes_to_read);
	if (bytes_to_read != data_size) {
		ilctxt->read_page->hdr.read_offset = 0xFFFF;
		ilctxt->read_page = get_next_page(ilctxt, ilctxt->read_page);
		ilctxt->read_page->hdr.read_offset = 0;
		if (data)
			memcpy((data + bytes_to_read),
			       (ilctxt->read_page->data +
				ilctxt->read_page->hdr.read_offset),
			       (data_size - bytes_to_read));
		bytes_to_read = (data_size - bytes_to_read);
	}
	ilctxt->read_page->hdr.read_offset += bytes_to_read;
	ilctxt->write_avail += data_size;
}

/*
 * Reads a message.
 *
 * If a message is read successfully, then the the message context
 * will be set to:
 *     .hdr    message header .size and .type values
 *     .offset beginning of message data
 *
 * @ectxt   Message context and if NULL, drops the message.
 *
 * @returns 0  - no message available
 *          1  - message read
 */
int msg_read(struct ipc_log_context *ilctxt,
	     struct encode_context *ectxt)
{
	struct tsv_header hdr;

	ipc_log_read(ilctxt, &hdr, sizeof(hdr));
	if (ectxt) {
		ectxt->hdr.type = hdr.type;
		ectxt->hdr.size = hdr.size;
		ectxt->offset = sizeof(hdr);
		ipc_log_read(ilctxt, (ectxt->buff + ectxt->offset),
			     (int)hdr.size);
	} else {
		ipc_log_read(ilctxt, NULL, (int)hdr.size);
	}
	return sizeof(hdr) + (int)hdr.size;
}

/*
 * Commits messages to the FIFO.  If the FIFO is full, then enough
 * messages are dropped to create space for the new message.
 */
void ipc_log_write(void *ctxt, struct encode_context *ectxt)
{
	struct ipc_log_context *ilctxt = (struct ipc_log_context *)ctxt;
	int bytes_to_write;
	unsigned long flags;

	if (!ilctxt || !ectxt) {
		pr_err("%s: Invalid ipc_log or encode context\n", __func__);
		return;
	}

	read_lock_irqsave(&ipc_log_context_list_lock, flags);
	spin_lock(&ilctxt->ipc_log_context_lock);
	while (ilctxt->write_avail < ectxt->offset)
		msg_read(ilctxt, NULL);

	bytes_to_write = MIN(((PAGE_SIZE - sizeof(struct ipc_log_page_header))
				- ilctxt->write_page->hdr.write_offset),
				ectxt->offset);
	memcpy((ilctxt->write_page->data +
		ilctxt->write_page->hdr.write_offset),
		ectxt->buff, bytes_to_write);
	if (bytes_to_write != ectxt->offset) {
		ilctxt->write_page->hdr.write_offset = 0xFFFF;
		ilctxt->write_page = get_next_page(ilctxt, ilctxt->write_page);
		ilctxt->write_page->hdr.write_offset = 0;
		memcpy((ilctxt->write_page->data +
			ilctxt->write_page->hdr.write_offset),
		       (ectxt->buff + bytes_to_write),
		       (ectxt->offset - bytes_to_write));
		bytes_to_write = (ectxt->offset - bytes_to_write);
	}
	ilctxt->write_page->hdr.write_offset += bytes_to_write;
	ilctxt->write_avail -= ectxt->offset;
	complete(&ilctxt->read_avail);
	spin_unlock(&ilctxt->ipc_log_context_lock);
	read_unlock_irqrestore(&ipc_log_context_list_lock, flags);
}
EXPORT_SYMBOL(ipc_log_write);

/*
 * Starts a new message after which you can add serialized data and
 * then complete the message by calling msg_encode_end().
 */
void msg_encode_start(struct encode_context *ectxt, uint32_t type)
{
	if (!ectxt) {
		pr_err("%s: Invalid encode context\n", __func__);
		return;
	}

	ectxt->hdr.type = type;
	ectxt->hdr.size = 0;
	ectxt->offset = sizeof(ectxt->hdr);
}
EXPORT_SYMBOL(msg_encode_start);

/*
 * Completes the message
 */
void msg_encode_end(struct encode_context *ectxt)
{
	if (!ectxt) {
		pr_err("%s: Invalid encode context\n", __func__);
		return;
	}

	/* finalize data size */
	ectxt->hdr.size = ectxt->offset - sizeof(ectxt->hdr);
	BUG_ON(ectxt->hdr.size > MAX_MSG_SIZE);
	memcpy(ectxt->buff, &ectxt->hdr, sizeof(ectxt->hdr));
}
EXPORT_SYMBOL(msg_encode_end);

/*
 * Helper funtion used to write data to a message context.
 *
 * @ectxt context initialized by calling msg_encode_start()
 * @data  data to write
 * @size  number of bytes of data to write
 */
static inline int tsv_write_data(struct encode_context *ectxt,
				 void *data, uint32_t size)
{
	if (!ectxt) {
		pr_err("%s: Invalid encode context\n", __func__);
		return -EINVAL;
	}
	if ((ectxt->offset + size) > MAX_MSG_SIZE) {
		pr_err("%s: No space to encode further\n", __func__);
		return -EINVAL;
	}

	memcpy((void *)(ectxt->buff + ectxt->offset), data, size);
	ectxt->offset += size;
	return 0;
}

/*
 * Helper function that writes a type to the context.
 *
 * @ectxt context initialized by calling msg_encode_start()
 * @type  primitive type
 * @size  size of primitive in bytes
 */
static inline int tsv_write_header(struct encode_context *ectxt,
				   uint32_t type, uint32_t size)
{
	struct tsv_header hdr;

	hdr.type = (unsigned char)type;
	hdr.size = (unsigned char)size;
	return tsv_write_data(ectxt, &hdr, sizeof(hdr));
}

/*
 * Writes the current timestamp count.
 *
 * @ectxt   context initialized by calling msg_encode_start()
 */
int tsv_timestamp_write(struct encode_context *ectxt)
{
	int ret;
	unsigned long long t_now = sched_clock();

	ret = tsv_write_header(ectxt, TSV_TYPE_TIMESTAMP, sizeof(t_now));
	if (ret)
		return ret;
	return tsv_write_data(ectxt, &t_now, sizeof(t_now));
}
EXPORT_SYMBOL(tsv_timestamp_write);

/*
 * Writes a data pointer.
 *
 * @ectxt   context initialized by calling msg_encode_start()
 * @pointer pointer value to write
 */
int tsv_pointer_write(struct encode_context *ectxt, void *pointer)
{
	int ret;
	ret = tsv_write_header(ectxt, TSV_TYPE_POINTER, sizeof(pointer));
	if (ret)
		return ret;
	return tsv_write_data(ectxt, &pointer, sizeof(pointer));
}
EXPORT_SYMBOL(tsv_pointer_write);

/*
 * Writes a 32-bit integer value.
 *
 * @ectxt context initialized by calling msg_encode_start()
 * @n     integer to write
 */
int tsv_int32_write(struct encode_context *ectxt, int32_t n)
{
	int ret;
	ret = tsv_write_header(ectxt, TSV_TYPE_INT32, sizeof(n));
	if (ret)
		return ret;
	return tsv_write_data(ectxt, &n, sizeof(n));
}
EXPORT_SYMBOL(tsv_int32_write);

/*
 * Writes a byte array.
 *
 * @ectxt context initialized by calling msg_write_start()
 * @data  Beginning address of data
 * @data_size Size of data to be written
 */
int tsv_byte_array_write(struct encode_context *ectxt,
			 void *data, int data_size)
{
	int ret;
	ret = tsv_write_header(ectxt, TSV_TYPE_BYTE_ARRAY, data_size);
	if (ret)
		return ret;
	return tsv_write_data(ectxt, data, data_size);
}
EXPORT_SYMBOL(tsv_byte_array_write);

/*
 * Helper function to log a string
 *
 * @ilctxt ipc_log_context created using ipc_log_context_create()
 * @fmt Data specified using format specifiers
 */
int ipc_log_string(void *ilctxt, const char *fmt, ...)
{
	struct encode_context ectxt;
	int avail_size, data_size, hdr_size = sizeof(struct tsv_header);
	va_list arg_list;

	if (!ilctxt)
		return -EINVAL;

	msg_encode_start(&ectxt, TSV_TYPE_STRING);
	tsv_timestamp_write(&ectxt);
	avail_size = (MAX_MSG_SIZE - (ectxt.offset + hdr_size));
	va_start(arg_list, fmt);
	data_size = vsnprintf((ectxt.buff + ectxt.offset + hdr_size),
			      avail_size, fmt, arg_list);
	va_end(arg_list);
	tsv_write_header(&ectxt, TSV_TYPE_BYTE_ARRAY, data_size);
	ectxt.offset += data_size;
	msg_encode_end(&ectxt);
	ipc_log_write(ilctxt, &ectxt);
	return 0;
}
EXPORT_SYMBOL(ipc_log_string);

/**
 * ipc_log_extract - Reads and deserializes log
 *
 * @ctxt:  logging context
 * @buff:    buffer to receive the data
 * @size:    size of the buffer
 * @returns: 0 if no data read; >0 number of bytes read; < 0 error
 *
 * If no data is available to be read, then the ilctxt::read_avail
 * completion is reinitialized.  This allows clients to block
 * until new log data is save.
 */
int ipc_log_extract(void *ctxt, char *buff, int size)
{
	struct encode_context ectxt;
	struct decode_context dctxt;
	void (*deserialize_func)(struct encode_context *ectxt,
				 struct decode_context *dctxt);
	struct ipc_log_context *ilctxt = (struct ipc_log_context *)ctxt;
	unsigned long flags;

	if (size < MAX_MSG_DECODED_SIZE)
		return -EINVAL;

	dctxt.output_format = OUTPUT_DEBUGFS;
	dctxt.buff = buff;
	dctxt.size = size;
	read_lock_irqsave(&ipc_log_context_list_lock, flags);
	spin_lock(&ilctxt->ipc_log_context_lock);
	while (dctxt.size >= MAX_MSG_DECODED_SIZE &&
	       !is_ilctxt_empty(ilctxt)) {
		msg_read(ilctxt, &ectxt);
		deserialize_func = get_deserialization_func(ilctxt,
							ectxt.hdr.type);
		spin_unlock(&ilctxt->ipc_log_context_lock);
		read_unlock_irqrestore(&ipc_log_context_list_lock, flags);
		if (deserialize_func)
			deserialize_func(&ectxt, &dctxt);
		else
			pr_err("%s: unknown message 0x%x\n",
				__func__, ectxt.hdr.type);
		read_lock_irqsave(&ipc_log_context_list_lock, flags);
		spin_lock(&ilctxt->ipc_log_context_lock);
	}
	if ((size - dctxt.size) == 0)
		init_completion(&ilctxt->read_avail);
	spin_unlock(&ilctxt->ipc_log_context_lock);
	read_unlock_irqrestore(&ipc_log_context_list_lock, flags);
	return size - dctxt.size;
}
EXPORT_SYMBOL(ipc_log_extract);

/*
 * Helper funtion used to read data from a message context.
 *
 * @ectxt  context initialized by calling msg_read()
 * @data  data to read
 * @size  number of bytes of data to read
 */
static void tsv_read_data(struct encode_context *ectxt,
			  void *data, uint32_t size)
{
	BUG_ON((ectxt->offset + size) > MAX_MSG_SIZE);
	memcpy(data, (ectxt->buff + ectxt->offset), size);
	ectxt->offset += size;
}

/*
 * Helper function that reads a type from the context and updates the
 * context pointers.
 *
 * @ectxt  context initialized by calling msg_read()
 * @hdr   type header
 */
static void tsv_read_header(struct encode_context *ectxt,
			    struct tsv_header *hdr)
{
	BUG_ON((ectxt->offset + sizeof(*hdr)) > MAX_MSG_SIZE);
	memcpy(hdr, (ectxt->buff + ectxt->offset), sizeof(*hdr));
	ectxt->offset += sizeof(*hdr);
}

/*
 * Reads a timestamp.
 *
 * @ectxt   context initialized by calling msg_read()
 * @dctxt   deserialization context
 * @format output format (appended to %6u.%09u timestamp format)
 */
void tsv_timestamp_read(struct encode_context *ectxt,
			struct decode_context *dctxt, const char *format)
{
	struct tsv_header hdr;
	unsigned long long val;
	unsigned long nanosec_rem;

	tsv_read_header(ectxt, &hdr);
	BUG_ON(hdr.type != TSV_TYPE_TIMESTAMP);
	tsv_read_data(ectxt, &val, sizeof(val));
	nanosec_rem = do_div(val, 1000000000U);
	IPC_SPRINTF_DECODE(dctxt, "[%6u.%09lu]%s",
			(unsigned)val, nanosec_rem, format);
}
EXPORT_SYMBOL(tsv_timestamp_read);

/*
 * Reads a data pointer.
 *
 * @ectxt   context initialized by calling msg_read()
 * @dctxt   deserialization context
 * @format output format
 */
void tsv_pointer_read(struct encode_context *ectxt,
		      struct decode_context *dctxt, const char *format)
{
	struct tsv_header hdr;
	void *val;

	tsv_read_header(ectxt, &hdr);
	BUG_ON(hdr.type != TSV_TYPE_POINTER);
	tsv_read_data(ectxt, &val, sizeof(val));

	IPC_SPRINTF_DECODE(dctxt, format, val);
}
EXPORT_SYMBOL(tsv_pointer_read);

/*
 * Reads a 32-bit integer value.
 *
 * @ectxt   context initialized by calling msg_read()
 * @dctxt   deserialization context
 * @format output format
 */
int32_t tsv_int32_read(struct encode_context *ectxt,
		       struct decode_context *dctxt, const char *format)
{
	struct tsv_header hdr;
	int32_t val;

	tsv_read_header(ectxt, &hdr);
	BUG_ON(hdr.type != TSV_TYPE_INT32);
	tsv_read_data(ectxt, &val, sizeof(val));

	IPC_SPRINTF_DECODE(dctxt, format, val);
	return val;
}
EXPORT_SYMBOL(tsv_int32_read);

/*
 * Reads a byte array/string.
 *
 * @ectxt   context initialized by calling msg_read()
 * @dctxt   deserialization context
 * @format output format
 */
void tsv_byte_array_read(struct encode_context *ectxt,
			 struct decode_context *dctxt, const char *format)
{
	struct tsv_header hdr;

	tsv_read_header(ectxt, &hdr);
	BUG_ON(hdr.type != TSV_TYPE_BYTE_ARRAY);
	tsv_read_data(ectxt, dctxt->buff, hdr.size);
	dctxt->buff += hdr.size;
	dctxt->size -= hdr.size;
}
EXPORT_SYMBOL(tsv_byte_array_read);

int add_deserialization_func(void *ctxt, int type,
			void (*dfunc)(struct encode_context *,
				      struct decode_context *))
{
	struct ipc_log_context *ilctxt = (struct ipc_log_context *)ctxt;
	struct dfunc_info *df_info;
	unsigned long flags;

	if (!ilctxt || !dfunc)
		return -EINVAL;

	df_info = kmalloc(sizeof(struct dfunc_info), GFP_KERNEL);
	if (!df_info)
		return -ENOSPC;

	read_lock_irqsave(&ipc_log_context_list_lock, flags);
	spin_lock(&ilctxt->ipc_log_context_lock);
	df_info->type = type;
	df_info->dfunc = dfunc;
	list_add_tail(&df_info->list, &ilctxt->dfunc_info_list);
	spin_unlock(&ilctxt->ipc_log_context_lock);
	read_unlock_irqrestore(&ipc_log_context_list_lock, flags);
	return 0;
}
EXPORT_SYMBOL(add_deserialization_func);

static void *get_deserialization_func(struct ipc_log_context *ilctxt,
				      int type)
{
	struct dfunc_info *df_info = NULL;

	if (!ilctxt)
		return NULL;

	list_for_each_entry(df_info, &ilctxt->dfunc_info_list, list) {
		if (df_info->type == type)
			return df_info->dfunc;
	}
	return NULL;
}

/**
 * ipc_log_context_create: Create a debug log context
 *                         Should not be called from atomic context
 *
 * @max_num_pages: Number of pages of logging space required (max. 10)
 * @mod_name     : Name of the directory entry under DEBUGFS
 * @user_version : Version number of user-defined message formats
 *
 * returns context id on success, NULL on failure
 */
void *ipc_log_context_create(int max_num_pages,
			     const char *mod_name, uint16_t user_version)
{
	struct ipc_log_context *ctxt;
	struct ipc_log_page *pg = NULL;
	int page_cnt, local_log_id;
	unsigned long flags;

	ctxt = kzalloc(sizeof(struct ipc_log_context), GFP_KERNEL);
	if (!ctxt) {
		pr_err("%s: cannot create ipc_log_context\n", __func__);
		return 0;
	}

	local_log_id = atomic_add_return(1, &next_log_id);
	init_completion(&ctxt->read_avail);
	INIT_LIST_HEAD(&ctxt->page_list);
	INIT_LIST_HEAD(&ctxt->dfunc_info_list);
	spin_lock_init(&ctxt->ipc_log_context_lock);
	for (page_cnt = 0; page_cnt < max_num_pages; page_cnt++) {
		pg = kzalloc(sizeof(struct ipc_log_page), GFP_KERNEL);
		if (!pg) {
			pr_err("%s: cannot create ipc_log_page\n", __func__);
			goto release_ipc_log_context;
		}
		pg->hdr.magic = IPC_LOGGING_MAGIC_NUM;
		pg->hdr.nmagic = ~(IPC_LOGGING_MAGIC_NUM);
		pg->hdr.log_id = (uint32_t)local_log_id;
		pg->hdr.page_num = page_cnt;
		pg->hdr.read_offset = 0xFFFF;
		pg->hdr.write_offset = 0xFFFF;
		spin_lock_irqsave(&ctxt->ipc_log_context_lock, flags);
		list_add_tail(&pg->hdr.list, &ctxt->page_list);
		spin_unlock_irqrestore(&ctxt->ipc_log_context_lock, flags);
	}
	ctxt->user_version = user_version;
	ctxt->first_page = get_first_page(ctxt);
	ctxt->last_page = pg;
	ctxt->write_page = ctxt->first_page;
	ctxt->read_page = ctxt->first_page;
	ctxt->write_page->hdr.write_offset = 0;
	ctxt->read_page->hdr.read_offset = 0;
	ctxt->write_avail = max_num_pages * (PAGE_SIZE -
					sizeof(struct ipc_log_page_header));

	create_ctx_debugfs(ctxt, mod_name);

	write_lock_irqsave(&ipc_log_context_list_lock, flags);
	list_add_tail(&ctxt->list, &ipc_log_context_list);
	write_unlock_irqrestore(&ipc_log_context_list_lock, flags);
	return (void *)ctxt;

release_ipc_log_context:
	while (page_cnt-- > 0) {
		pg = get_first_page(ctxt);
		list_del(&pg->hdr.list);
		kfree(pg);
	}
	kfree(ctxt);
	return 0;
}
EXPORT_SYMBOL(ipc_log_context_create);

/*
 * Destroy debug log context
 *
 * @ctxt: debug log context created by calling ipc_log_context_create API.
 */
int ipc_log_context_destroy(void *ctxt)
{
	struct ipc_log_context *ilctxt = (struct ipc_log_context *)ctxt;
	struct ipc_log_page *pg = NULL;
	unsigned long flags;

	if (!ilctxt)
		return 0;

	while (!list_empty(&ilctxt->page_list)) {
		pg = get_first_page(ctxt);
		list_del(&pg->hdr.list);
		kfree(pg);
	}

	write_lock_irqsave(&ipc_log_context_list_lock, flags);
	list_del(&ilctxt->list);
	write_unlock_irqrestore(&ipc_log_context_list_lock, flags);

	kfree(ilctxt);
	return 0;
}
EXPORT_SYMBOL(ipc_log_context_destroy);

static int __init ipc_logging_init(void)
{
	check_and_create_debugfs();
	return 0;
}

module_init(ipc_logging_init);

MODULE_DESCRIPTION("ipc logging");
MODULE_LICENSE("GPL v2");
