/* Copyright (c) 2002,2008-2014, The Linux Foundation. All rights reserved.
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

#include <linux/export.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include "kgsl.h"
#include "adreno.h"
#include "kgsl_cffdump.h"
#include "kgsl_sync.h"

static int _active_count_get(void *data, u64 *val)
{
	struct kgsl_device *device = data;
	unsigned int i = atomic_read(&device->active_cnt);

	*val = (u64) i;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(_active_count_fops, _active_count_get, NULL, "%llu\n");

typedef void (*reg_read_init_t)(struct kgsl_device *device);
typedef void (*reg_read_fill_t)(struct kgsl_device *device, int i,
	unsigned int *vals, int linec);


static void sync_event_print(struct seq_file *s,
		struct kgsl_cmdbatch_sync_event *sync_event)
{
	switch (sync_event->type) {
	case KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP: {
		seq_printf(s, "sync: ctx: %d ts: %d",
				sync_event->context->id, sync_event->timestamp);
		break;
	}
	case KGSL_CMD_SYNCPOINT_TYPE_FENCE:
		seq_printf(s, "sync: [%p] %s", sync_event->handle,
		(sync_event->handle && sync_event->handle->fence)
				? sync_event->handle->fence->name : "NULL");
		break;
	default:
		seq_printf(s, "sync: type: %d", sync_event->type);
		break;
	}
}

struct flag_entry {
	unsigned long mask;
	const char *str;
};

static const struct flag_entry cmdbatch_flags[] = {KGSL_CMDBATCH_FLAGS};

static const struct flag_entry cmdbatch_priv[] = {
	{ CMDBATCH_FLAG_SKIP, "skip"},
	{ CMDBATCH_FLAG_FORCE_PREAMBLE, "force_preamble"},
	{ CMDBATCH_FLAG_WFI, "wait_for_idle" },
};

static const struct flag_entry context_flags[] = {KGSL_CONTEXT_FLAGS};

/*
 * Note that the ADRENO_CONTEXT_* flags start at
 * KGSL_CONTEXT_PRIV_DEVICE_SPECIFIC so it is ok to cross the streams here.
 */
static const struct flag_entry context_priv[] = {
	{ KGSL_CONTEXT_PRIV_DETACHED, "detached"},
	{ KGSL_CONTEXT_PRIV_INVALID, "invalid"},
	{ KGSL_CONTEXT_PRIV_PAGEFAULT, "pagefault"},
	{ ADRENO_CONTEXT_FAULT, "fault"},
	{ ADRENO_CONTEXT_GPU_HANG, "gpu_hang"},
	{ ADRENO_CONTEXT_GPU_HANG_FT, "gpu_hang_ft"},
	{ ADRENO_CONTEXT_SKIP_EOF, "skip_end_of_frame" },
	{ ADRENO_CONTEXT_FORCE_PREAMBLE, "force_preamble"},
};

static void print_flags(struct seq_file *s, const struct flag_entry *table,
			size_t table_size, unsigned long flags)
{
	int i;
	int first = 1;

	for (i = 0; i < table_size; i++) {
		if (flags & table[i].mask) {
			seq_printf(s, "%c%s", first ? '\0' : '|', table[i].str);
			flags &= ~(table[i].mask);
			first = 0;
		}
	}
	if (flags) {
		seq_printf(s, "%c0x%lx", first ? '\0' : '|', flags);
		first = 0;
	}
	if (first)
		seq_puts(s, "None");
}

static void cmdbatch_print(struct seq_file *s, struct kgsl_cmdbatch *cmdbatch)
{
	struct kgsl_cmdbatch_sync_event *sync_event;

	/*
	 * print fences first, since they block this cmdbatch.
	 * We may have cmdbatch timer running, which also uses
	 * same lock, take a lock with software interrupt disabled (bh)
	 * to avoid spin lock recursion.
	 */
	spin_lock_bh(&cmdbatch->lock);

	list_for_each_entry(sync_event, &cmdbatch->synclist, node) {
		/*
		 * Timestamp is 0 for KGSL_CONTEXT_SYNC, but print it anyways
		 * so that it is clear if the fence was a separate submit
		 * or part of an IB submit.
		 */
		seq_printf(s, "\t%d ", cmdbatch->timestamp);
		sync_event_print(s, sync_event);
		seq_puts(s, "\n");
	}

	spin_unlock_bh(&cmdbatch->lock);

	/* if this flag is set, there won't be an IB */
	if (cmdbatch->flags & KGSL_CONTEXT_SYNC)
		return;

	seq_printf(s, "\t%d: ib: expires: %lu",
		cmdbatch->timestamp, cmdbatch->expires);

	seq_puts(s, " flags: ");
	print_flags(s, cmdbatch_flags, ARRAY_SIZE(cmdbatch_flags),
		    cmdbatch->flags);

	seq_puts(s, " priv: ");
	print_flags(s, cmdbatch_priv, ARRAY_SIZE(cmdbatch_priv),
		    cmdbatch->priv);

	seq_puts(s, "\n");
}

static const char *ctx_type_str(unsigned int type)
{
	int i;
	struct flag_entry table[] = {KGSL_CONTEXT_TYPES};

	for (i = 0; i < ARRAY_SIZE(table); i++)
		if (type == table[i].mask)
			return table[i].str;
	return "UNKNOWN";
}

static int ctx_print(struct seq_file *s, void *unused)
{
	struct adreno_context *drawctxt = s->private;
	unsigned int i;
	struct kgsl_event *event;
	unsigned int queued = 0, consumed = 0, retired = 0;

	seq_printf(s, "id: %d type: %s priority: %d process: %s (%d) tid: %d\n",
		   drawctxt->base.id,
		   ctx_type_str(drawctxt->type),
		   drawctxt->base.priority,
		   drawctxt->base.proc_priv->comm,
		   drawctxt->base.proc_priv->pid,
		   drawctxt->base.tid);

	seq_puts(s, "flags: ");
	print_flags(s, context_flags, ARRAY_SIZE(context_flags),
		    drawctxt->base.flags & ~(KGSL_CONTEXT_PRIORITY_MASK
						| KGSL_CONTEXT_TYPE_MASK));
	seq_puts(s, " priv: ");
	print_flags(s, context_priv, ARRAY_SIZE(context_priv),
			drawctxt->base.priv);
	seq_puts(s, "\n");

	seq_puts(s, "timestamps: ");
	kgsl_readtimestamp(drawctxt->base.device, &drawctxt->base,
				KGSL_TIMESTAMP_QUEUED, &queued);
	kgsl_readtimestamp(drawctxt->base.device, &drawctxt->base,
				KGSL_TIMESTAMP_CONSUMED, &consumed);
	kgsl_readtimestamp(drawctxt->base.device, &drawctxt->base,
				KGSL_TIMESTAMP_RETIRED, &retired);
	seq_printf(s, "queued: %u consumed: %u retired: %u global:%u\n",
		   queued, consumed, retired,
		   drawctxt->internal_timestamp);

	seq_puts(s, "cmdqueue:\n");

	spin_lock(&drawctxt->lock);
	for (i = drawctxt->cmdqueue_head;
		i != drawctxt->cmdqueue_tail;
		i = CMDQUEUE_NEXT(i, ADRENO_CONTEXT_CMDQUEUE_SIZE))
		cmdbatch_print(s, drawctxt->cmdqueue[i]);
	spin_unlock(&drawctxt->lock);

	seq_puts(s, "events:\n");
	spin_lock(&drawctxt->base.events.lock);
	list_for_each_entry(event, &drawctxt->base.events.events, node)
		seq_printf(s, "\t%d: %pF created: %u\n", event->timestamp,
				event->func, event->created);
	spin_unlock(&drawctxt->base.events.lock);

	return 0;
}

static int ctx_open(struct inode *inode, struct file *file)
{
	int ret;
	unsigned int id = (unsigned int)(unsigned long)inode->i_private;
	struct kgsl_context *context;

	context = kgsl_context_get(kgsl_get_device(KGSL_DEVICE_3D0), id);
	if (context == NULL)
		return -ENODEV;

	ret = single_open(file, ctx_print, context);
	if (ret)
		kgsl_context_put(context);
	return ret;
}

static int ctx_release(struct inode *inode, struct file *file)
{
	struct kgsl_context *context;

	context = ((struct seq_file *)file->private_data)->private;

	kgsl_context_put(context);

	return single_release(inode, file);
}

static const struct file_operations ctx_fops = {
	.open = ctx_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = ctx_release,
};


void
adreno_context_debugfs_init(struct adreno_device *adreno_dev,
			    struct adreno_context *ctx)
{
	unsigned char name[16];

	snprintf(name, sizeof(name), "%d", ctx->base.id);

	ctx->debug_root = debugfs_create_file(name, 0444,
				adreno_dev->ctx_d_debugfs,
				(void *)(unsigned long)ctx->base.id, &ctx_fops);
}

void adreno_debugfs_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;

	if (!device->d_debugfs || IS_ERR(device->d_debugfs))
		return;

	kgsl_cffdump_debugfs_create(device);

	debugfs_create_u32("wait_timeout", 0644, device->d_debugfs,
		&adreno_dev->wait_timeout);
	debugfs_create_u32("ib_check", 0644, device->d_debugfs,
			   &adreno_dev->ib_check_level);
	debugfs_create_file("active_cnt", 0444, device->d_debugfs, device,
			    &_active_count_fops);
	adreno_dev->ctx_d_debugfs = debugfs_create_dir("ctx",
							device->d_debugfs);
}
