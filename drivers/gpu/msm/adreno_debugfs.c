/* Copyright (c) 2002,2008-2017, The Linux Foundation. All rights reserved.
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

static int _isdb_set(void *data, u64 val)
{
	struct kgsl_device *device = data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* Once ISDB goes enabled it stays enabled */
	if (test_bit(ADRENO_DEVICE_ISDB_ENABLED, &adreno_dev->priv))
		return 0;

	mutex_lock(&device->mutex);

	/*
	 * Bring down the GPU so we can bring it back up with the correct power
	 * and clock settings
	 */
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);
	set_bit(ADRENO_DEVICE_ISDB_ENABLED, &adreno_dev->priv);
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

	mutex_unlock(&device->mutex);

	return 0;
}

static int _isdb_get(void *data, u64 *val)
{
	struct kgsl_device *device = data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	*val = (u64) test_bit(ADRENO_DEVICE_ISDB_ENABLED, &adreno_dev->priv);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(_isdb_fops, _isdb_get, _isdb_set, "%llu\n");

static int _lm_limit_set(void *data, u64 val)
{
	struct kgsl_device *device = data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM))
		return 0;

	/* assure value is between 3A and 10A */
	if (val > 10000)
		val = 10000;
	else if (val < 3000)
		val = 3000;

	adreno_dev->lm_limit = val;

	if (test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag)) {
		mutex_lock(&device->mutex);
		kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);
		kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);
		mutex_unlock(&device->mutex);
	}

	return 0;
}

static int _lm_limit_get(void *data, u64 *val)
{
	struct kgsl_device *device = data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM))
		*val = 0;

	*val = (u64) adreno_dev->lm_limit;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(_lm_limit_fops, _lm_limit_get, _lm_limit_set, "%llu\n");

static int _lm_threshold_count_get(void *data, u64 *val)
{
	struct kgsl_device *device = data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM))
		*val = 0;
	else
		*val = (u64) adreno_dev->lm_threshold_cross;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(_lm_threshold_fops, _lm_threshold_count_get,
	NULL, "%llu\n");

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
		struct kgsl_drawobj_sync_event *sync_event)
{
	switch (sync_event->type) {
	case KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP: {
		seq_printf(s, "sync: ctx: %d ts: %d",
				sync_event->context->id, sync_event->timestamp);
		break;
	}
	case KGSL_CMD_SYNCPOINT_TYPE_FENCE:
		seq_printf(s, "sync: [%pK] %s", sync_event->handle,
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

static const struct flag_entry drawobj_flags[] = {KGSL_DRAWOBJ_FLAGS};

static const struct flag_entry cmdobj_priv[] = {
	{ CMDOBJ_SKIP, "skip"},
	{ CMDOBJ_FORCE_PREAMBLE, "force_preamble"},
	{ CMDOBJ_WFI, "wait_for_idle" },
};

static const struct flag_entry context_flags[] = {KGSL_CONTEXT_FLAGS};

/*
 * Note that the ADRENO_CONTEXT_* flags start at
 * KGSL_CONTEXT_PRIV_DEVICE_SPECIFIC so it is ok to cross the streams here.
 */
static const struct flag_entry context_priv[] = {
	{ KGSL_CONTEXT_PRIV_SUBMITTED, "submitted"},
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

static void syncobj_print(struct seq_file *s,
			struct kgsl_drawobj_sync *syncobj)
{
	struct kgsl_drawobj_sync_event *event;
	unsigned int i;

	seq_puts(s, " syncobj ");

	for (i = 0; i < syncobj->numsyncs; i++) {
		event = &syncobj->synclist[i];

		if (!kgsl_drawobj_event_pending(syncobj, i))
			continue;

		sync_event_print(s, event);
		seq_puts(s, "\n");
	}
}

static void cmdobj_print(struct seq_file *s,
			struct kgsl_drawobj_cmd *cmdobj)
{
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);

	if (drawobj->type == CMDOBJ_TYPE)
		seq_puts(s, " cmdobj ");
	else
		seq_puts(s, " markerobj ");

	seq_printf(s, "\t %d ", drawobj->timestamp);

	seq_puts(s, " priv: ");
	print_flags(s, cmdobj_priv, ARRAY_SIZE(cmdobj_priv),
				cmdobj->priv);
}

static void drawobj_print(struct seq_file *s,
			struct kgsl_drawobj *drawobj)
{
	if (drawobj->type == SYNCOBJ_TYPE)
		syncobj_print(s, SYNCOBJ(drawobj));
	else if ((drawobj->type == CMDOBJ_TYPE) ||
			(drawobj->type == MARKEROBJ_TYPE))
		cmdobj_print(s, CMDOBJ(drawobj));

	seq_puts(s, " flags: ");
	print_flags(s, drawobj_flags, ARRAY_SIZE(drawobj_flags),
		    drawobj->flags);

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

	seq_puts(s, "drawqueue:\n");

	spin_lock(&drawctxt->lock);
	for (i = drawctxt->drawqueue_head;
		i != drawctxt->drawqueue_tail;
		i = DRAWQUEUE_NEXT(i, ADRENO_CONTEXT_DRAWQUEUE_SIZE))
		drawobj_print(s, drawctxt->drawqueue[i]);
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
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!device->d_debugfs || IS_ERR(device->d_debugfs))
		return;

	kgsl_cffdump_debugfs_create(device);

	debugfs_create_file("active_cnt", 0444, device->d_debugfs, device,
			    &_active_count_fops);
	adreno_dev->ctx_d_debugfs = debugfs_create_dir("ctx",
							device->d_debugfs);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_LM)) {
		debugfs_create_file("lm_limit", 0644, device->d_debugfs, device,
			&_lm_limit_fops);
		debugfs_create_file("lm_threshold_count", 0444,
			device->d_debugfs, device, &_lm_threshold_fops);
	}

	if (adreno_is_a5xx(adreno_dev))
		debugfs_create_file("isdb", 0644, device->d_debugfs,
			device, &_isdb_fops);
}
