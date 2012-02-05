/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/vmalloc.h>
#include <linux/time.h>
#include <linux/sysfs.h>
#include <linux/utsname.h>
#include <linux/sched.h>
#include <linux/idr.h>

#include "kgsl.h"
#include "kgsl_log.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"
#include "kgsl_snapshot.h"

/* idr_for_each function to count the number of contexts */

static int snapshot_context_count(int id, void *ptr, void *data)
{
	int *count = data;
	*count = *count + 1;

	return 0;
}

/*
 * To simplify the iterator loop use a global pointer instead of trying
 * to pass around double star references to the snapshot data
 */

static void *_ctxtptr;

static int snapshot_context_info(int id, void *ptr, void *data)
{
	struct kgsl_snapshot_linux_context *header = _ctxtptr;
	struct kgsl_context *context = ptr;
	struct kgsl_device *device = context->dev_priv->device;

	header->id = id;

	/* Future-proof for per-context timestamps - for now, just
	 * return the global timestamp for all contexts
	 */

	header->timestamp_queued = -1;
	header->timestamp_retired = device->ftbl->readtimestamp(device,
		KGSL_TIMESTAMP_RETIRED);

	_ctxtptr += sizeof(struct kgsl_snapshot_linux_context);

	return 0;
}

/* Snapshot the Linux specific information */
static int snapshot_os(struct kgsl_device *device,
	void *snapshot, int remain, void *priv)
{
	struct kgsl_snapshot_linux *header = snapshot;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct task_struct *task;
	pid_t pid;
	int hang = (int) priv;
	int ctxtcount = 0;
	int size = sizeof(*header);

	/* Figure out how many active contexts there are - these will
	 * be appended on the end of the structure */

	idr_for_each(&device->context_idr, snapshot_context_count, &ctxtcount);

	size += ctxtcount * sizeof(struct kgsl_snapshot_linux_context);

	/* Make sure there is enough room for the data */
	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "OS");
		return 0;
	}

	memset(header, 0, sizeof(*header));

	header->osid = KGSL_SNAPSHOT_OS_LINUX;

	header->state = hang ? SNAPSHOT_STATE_HUNG : SNAPSHOT_STATE_RUNNING;

	/* Get the kernel build information */
	strlcpy(header->release, utsname()->release, sizeof(header->release));
	strlcpy(header->version, utsname()->version, sizeof(header->version));

	/* Get the Unix time for the timestamp */
	header->seconds = get_seconds();

	/* Remember the power information */
	header->power_flags = pwr->power_flags;
	header->power_level = pwr->active_pwrlevel;
	header->power_interval_timeout = pwr->interval_timeout;
	header->grpclk = kgsl_get_clkrate(pwr->grp_clks[0]);
	header->busclk = kgsl_get_clkrate(pwr->ebi1_clk);

	/* Future proof for per-context timestamps */
	header->current_context = -1;

	/* Get the current PT base */
	header->ptbase = kgsl_mmu_get_current_ptbase(device);
	/* And the PID for the task leader */
	pid = header->pid = kgsl_mmu_get_ptname_from_ptbase(header->ptbase);

	task = find_task_by_vpid(pid);

	if (task)
		get_task_comm(header->comm, task);

	header->ctxtcount = ctxtcount;

	/* append information for each context */
	_ctxtptr = snapshot + sizeof(*header);
	idr_for_each(&device->context_idr, snapshot_context_info, NULL);

	/* Return the size of the data segment */
	return size;
}
/*
 * kgsl_snapshot_dump_indexed_regs - helper function to dump indexed registers
 * @device - the device to dump registers from
 * @snapshot - pointer to the start of the region of memory for the snapshot
 * @remain - a pointer to the number of bytes remaining in the snapshot
 * @priv - A pointer to the kgsl_snapshot_indexed_registers data
 *
 * Given a indexed register cmd/data pair and a count, dump each indexed
 * register
 */

static int kgsl_snapshot_dump_indexed_regs(struct kgsl_device *device,
	void *snapshot, int remain, void *priv)
{
	struct kgsl_snapshot_indexed_registers *iregs = priv;
	struct kgsl_snapshot_indexed_regs *header = snapshot;
	unsigned int *data = snapshot + sizeof(*header);
	int i;

	if (remain < (iregs->count * 4) + sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "INDEXED REGS");
		return 0;
	}

	header->index_reg = iregs->index;
	header->data_reg = iregs->data;
	header->count = iregs->count;
	header->start = iregs->start;

	for (i = 0; i < iregs->count; i++) {
		kgsl_regwrite(device, iregs->index, iregs->start + i);
		kgsl_regread(device, iregs->data, &data[i]);
	}

	return (iregs->count * 4) + sizeof(*header);
}

/*
 * kgsl_snapshot_dump_regs - helper function to dump device registers
 * @device - the device to dump registers from
 * @snapshot - pointer to the start of the region of memory for the snapshot
 * @remain - a pointer to the number of bytes remaining in the snapshot
 * @priv - A pointer to the kgsl_snapshot_registers data
 *
 * Given an array of register ranges pairs (start,end [inclusive]), dump the
 * registers into a snapshot register section.  The snapshot region stores a
 * part of dwords for each register - the word address of the register, and
 * the value.
 */
int kgsl_snapshot_dump_regs(struct kgsl_device *device, void *snapshot,
	int remain, void *priv)
{
	struct kgsl_snapshot_regs *header = snapshot;
	struct kgsl_snapshot_registers *regs = priv;
	unsigned int *data = snapshot + sizeof(*header);
	int count = 0, i, j;

	/* Figure out how many registers we are going to dump */

	for (i = 0; i < regs->count; i++) {
		int start = regs->regs[i * 2];
		int end = regs->regs[i * 2 + 1];

		count += (end - start + 1);
	}

	if (remain < (count * 8) + sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	for (i = 0; i < regs->count; i++) {
		unsigned int start = regs->regs[i * 2];
		unsigned int end = regs->regs[i * 2 + 1];

		for (j = start; j <= end; j++) {
			unsigned int val;

			kgsl_regread(device, j, &val);
			*data++ = j;
			*data++ = val;
		}
	}

	header->count = count;

	/* Return the size of the section */
	return (count * 8) + sizeof(*header);
}
EXPORT_SYMBOL(kgsl_snapshot_dump_regs);

void *kgsl_snapshot_indexed_registers(struct kgsl_device *device,
		void *snapshot, int *remain,
		unsigned int index, unsigned int data, unsigned int start,
		unsigned int count)
{
	struct kgsl_snapshot_indexed_registers iregs;
	iregs.index = index;
	iregs.data = data;
	iregs.start = start;
	iregs.count = count;

	return kgsl_snapshot_add_section(device,
		 KGSL_SNAPSHOT_SECTION_INDEXED_REGS, snapshot,
		 remain, kgsl_snapshot_dump_indexed_regs, &iregs);
}
EXPORT_SYMBOL(kgsl_snapshot_indexed_registers);

/*
 * kgsl_snapshot - construct a device snapshot
 * @device - device to snapshot
 * @hang - set to 1 if the snapshot was triggered following a hnag
 * Given a device, construct a binary snapshot dump of the current device state
 * and store it in the device snapshot memory.
 */
int kgsl_device_snapshot(struct kgsl_device *device, int hang)
{
	struct kgsl_snapshot_header *header = device->snapshot;
	int remain = device->snapshot_maxsize - sizeof(*header);
	void *snapshot;

	/*
	 * The first hang is always the one we are interested in. To
	 * avoid a subsequent hang blowing away the first, the snapshot
	 * is frozen until it is dumped via sysfs.
	 *
	 * Note that triggered snapshots are always taken regardless
	 * of the state and never frozen.
	 */

	if (hang && device->snapshot_frozen == 1)
		return 0;

	if (device->snapshot == NULL) {
		KGSL_DRV_ERR(device,
			"snapshot: No snapshot memory available\n");
		return -ENOMEM;
	}

	if (remain < sizeof(*header)) {
		KGSL_DRV_ERR(device,
			"snapshot: Not enough memory for the header\n");
		return -ENOMEM;
	}

	header->magic = SNAPSHOT_MAGIC;

	header->gpuid = kgsl_gpuid(device);

	/* Get a pointer to the first section (right after the header) */
	snapshot = ((void *) device->snapshot) + sizeof(*header);

	/* Build the Linux specific header */
	snapshot = kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_OS,
		snapshot, &remain, snapshot_os, (void *) hang);

	/* Get the device specific sections */
	if (device->ftbl->snapshot)
		snapshot = device->ftbl->snapshot(device, snapshot, &remain,
			hang);

	/* Add the empty end section to let the parser know we are done */
	snapshot = kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_END,
		snapshot, &remain, NULL, NULL);

	device->snapshot_timestamp = get_seconds();
	device->snapshot_size = (int) (snapshot - device->snapshot);

	/* Freeze the snapshot on a hang until it gets read */
	device->snapshot_frozen = (hang) ? 1 : 0;

	return 0;
}
EXPORT_SYMBOL(kgsl_device_snapshot);

/* An attribute for showing snapshot details */
struct kgsl_snapshot_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kgsl_device *device, char *buf);
	ssize_t (*store)(struct kgsl_device *device, const char *buf,
		size_t count);
};

#define to_snapshot_attr(a) \
container_of(a, struct kgsl_snapshot_attribute, attr)

#define kobj_to_device(a) \
container_of(a, struct kgsl_device, snapshot_kobj)

/* Dump the sysfs binary data to the user */
static ssize_t snapshot_show(struct file *filep, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off,
	size_t count)
{
	struct kgsl_device *device = kobj_to_device(kobj);

	if (device == NULL)
		return 0;

	/* Return nothing if we haven't taken a snapshot yet */
	if (device->snapshot_timestamp == 0)
		return 0;

	/* Get the mutex to keep things from changing while we are dumping */
	mutex_lock(&device->mutex);

	/*
	 * Release the freeze on the snapshot the first time the buffer is read
	 */

	device->snapshot_frozen = 0;

	if (off >= device->snapshot_size) {
		count = 0;
		goto exit;
	}

	if (off + count > device->snapshot_size)
		count = device->snapshot_size - off;

	memcpy(buf, device->snapshot + off, count);

exit:
	mutex_unlock(&device->mutex);
	return count;
}

/* Show the timestamp of the last collected snapshot */
static ssize_t timestamp_show(struct kgsl_device *device, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%x\n", device->snapshot_timestamp);
}

/* manually trigger a new snapshot to be collected */
static ssize_t trigger_store(struct kgsl_device *device, const char *buf,
	size_t count)
{
	if (device && count > 0) {
		mutex_lock(&device->mutex);
		kgsl_device_snapshot(device, 0);
		mutex_unlock(&device->mutex);
	}

	return count;
}

static struct bin_attribute snapshot_attr = {
	.attr.name = "dump",
	.attr.mode = 0444,
	.size = 0,
	.read = snapshot_show
};

#define SNAPSHOT_ATTR(_name, _mode, _show, _store) \
struct kgsl_snapshot_attribute attr_##_name = { \
	.attr = { .name = __stringify(_name), .mode = _mode }, \
	.show = _show, \
	.store = _store, \
}

SNAPSHOT_ATTR(trigger, 0600, NULL, trigger_store);
SNAPSHOT_ATTR(timestamp, 0444, timestamp_show, NULL);

static void snapshot_sysfs_release(struct kobject *kobj)
{
}

static ssize_t snapshot_sysfs_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	struct kgsl_snapshot_attribute *pattr = to_snapshot_attr(attr);
	struct kgsl_device *device = kobj_to_device(kobj);
	ssize_t ret;

	if (device && pattr->show)
		ret = pattr->show(device, buf);
	else
		ret = -EIO;

	return ret;
}

static ssize_t snapshot_sysfs_store(struct kobject *kobj,
	struct attribute *attr, const char *buf, size_t count)
{
	struct kgsl_snapshot_attribute *pattr = to_snapshot_attr(attr);
	struct kgsl_device *device = kobj_to_device(kobj);
	ssize_t ret;

	if (device && pattr->store)
		ret = pattr->store(device, buf, count);
	else
		ret = -EIO;

	return ret;
}

static const struct sysfs_ops snapshot_sysfs_ops = {
	.show = snapshot_sysfs_show,
	.store = snapshot_sysfs_store,
};

static struct kobj_type ktype_snapshot = {
	.sysfs_ops = &snapshot_sysfs_ops,
	.default_attrs = NULL,
	.release = snapshot_sysfs_release,
};

/* kgsl_device_snapshot_init - Add resources for the device GPU snapshot
 * @device - The device to initalize
 *
 * Allocate memory for a GPU snapshot for the specified device,
 * and create the sysfs files to manage it
 */

int kgsl_device_snapshot_init(struct kgsl_device *device)
{
	int ret;

	if (device->snapshot == NULL)
		device->snapshot = vmalloc(KGSL_SNAPSHOT_MEMSIZE);

	if (device->snapshot == NULL)
		return -ENOMEM;

	device->snapshot_maxsize = KGSL_SNAPSHOT_MEMSIZE;
	device->snapshot_timestamp = 0;

	ret = kobject_init_and_add(&device->snapshot_kobj, &ktype_snapshot,
		&device->dev->kobj, "snapshot");
	if (ret)
		goto done;

	ret = sysfs_create_bin_file(&device->snapshot_kobj, &snapshot_attr);
	if (ret)
		goto done;

	ret  = sysfs_create_file(&device->snapshot_kobj, &attr_trigger.attr);
	if (ret)
		goto done;

	ret  = sysfs_create_file(&device->snapshot_kobj, &attr_timestamp.attr);

done:
	return ret;
}
EXPORT_SYMBOL(kgsl_device_snapshot_init);

/* kgsl_device_snapshot_close - Take down snapshot memory for a device
 * @device - Pointer to the kgsl_device
 *
 * Remove the sysfs files and free the memory allocated for the GPU
 * snapshot
 */

void kgsl_device_snapshot_close(struct kgsl_device *device)
{
	sysfs_remove_bin_file(&device->snapshot_kobj, &snapshot_attr);
	sysfs_remove_file(&device->snapshot_kobj, &attr_trigger.attr);
	sysfs_remove_file(&device->snapshot_kobj, &attr_timestamp.attr);

	kobject_put(&device->snapshot_kobj);

	vfree(device->snapshot);

	device->snapshot = NULL;
	device->snapshot_maxsize = 0;
	device->snapshot_timestamp = 0;
}
EXPORT_SYMBOL(kgsl_device_snapshot_close);
