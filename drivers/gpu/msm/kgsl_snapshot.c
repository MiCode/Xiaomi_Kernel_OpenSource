/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/export.h>
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

/* Placeholder for the list of memory objects frozen after a hang */

struct kgsl_snapshot_object {
	unsigned int gpuaddr;
	phys_addr_t ptbase;
	unsigned int size;
	unsigned int offset;
	int type;
	struct kgsl_mem_entry *entry;
	struct list_head node;
};

struct snapshot_obj_itr {
	void *buf;      /* Buffer pointer to write to */
	int pos;        /* Current position in the sequence */
	loff_t offset;  /* file offset to start writing from */
	size_t remain;  /* Bytes remaining in buffer */
	size_t write;   /* Bytes written so far */
};

static void obj_itr_init(struct snapshot_obj_itr *itr, void *buf,
	loff_t offset, size_t remain)
{
	itr->buf = buf;
	itr->offset = offset;
	itr->remain = remain;
	itr->pos = 0;
	itr->write = 0;
}

static int obj_itr_out(struct snapshot_obj_itr *itr, void *src, int size)
{
	if (itr->remain == 0)
		return 0;

	if ((itr->pos + size) <= itr->offset)
		goto done;

	/* Handle the case that offset is in the middle of the buffer */

	if (itr->offset > itr->pos) {
		src += (itr->offset - itr->pos);
		size -= (itr->offset - itr->pos);

		/* Advance pos to the offset start */
		itr->pos = itr->offset;
	}

	if (size > itr->remain)
		size = itr->remain;

	memcpy(itr->buf, src, size);

	itr->buf += size;
	itr->write += size;
	itr->remain -= size;

done:
	itr->pos += size;
	return size;
}

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
	struct kgsl_device *device;

	if (context)
		device = context->device;
	else
		device = (struct kgsl_device *)data;

	header->id = id;

	/* Future-proof for per-context timestamps - for now, just
	 * return the global timestamp for all contexts
	 */

	header->timestamp_queued = kgsl_readtimestamp(device, context,
						      KGSL_TIMESTAMP_QUEUED);
	header->timestamp_retired = kgsl_readtimestamp(device, context,
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
	phys_addr_t temp_ptbase;

	/* Figure out how many active contexts there are - these will
	 * be appended on the end of the structure */

	read_lock(&device->context_lock);
	idr_for_each(&device->context_idr, snapshot_context_count, &ctxtcount);
	read_unlock(&device->context_lock);

	/* Increment ctxcount for the global memstore */
	ctxtcount++;

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

	/* Save the last active context */
	kgsl_sharedmem_readl(&device->memstore, &header->current_context,
		KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL, current_context));


	/* Get the current PT base */
	temp_ptbase = kgsl_mmu_get_current_ptbase(&device->mmu);
	/* Truncate to 32 bits in case LPAE is used */
	header->ptbase = (__u32)temp_ptbase;
	/* And the PID for the task leader */
	pid = header->pid = kgsl_mmu_get_ptname_from_ptbase(&device->mmu,
								temp_ptbase);

	task = find_task_by_vpid(pid);

	if (task)
		get_task_comm(header->comm, task);

	header->ctxtcount = ctxtcount;

	_ctxtptr = snapshot + sizeof(*header);
	/* append information for the global context */
	snapshot_context_info(KGSL_MEMSTORE_GLOBAL, NULL, device);
	/* append information for each context */

	read_lock(&device->context_lock);
	idr_for_each(&device->context_idr, snapshot_context_info, NULL);
	read_unlock(&device->context_lock);

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

#define GPU_OBJ_HEADER_SZ \
	(sizeof(struct kgsl_snapshot_section_header) + \
	 sizeof(struct kgsl_snapshot_gpu_object))

static int kgsl_snapshot_dump_object(struct kgsl_device *device,
	struct kgsl_snapshot_object *obj, struct snapshot_obj_itr *itr)
{
	struct kgsl_snapshot_section_header sect;
	struct kgsl_snapshot_gpu_object header;
	int ret;

	sect.magic = SNAPSHOT_SECTION_MAGIC;
	sect.id = KGSL_SNAPSHOT_SECTION_GPU_OBJECT;

	/*
	 * Header size is in dwords, object size is in bytes -
	 * round up if the object size isn't dword aligned
	 */

	sect.size = GPU_OBJ_HEADER_SZ + ALIGN(obj->size, 4);

	ret = obj_itr_out(itr, &sect, sizeof(sect));
	if (ret == 0)
		return 0;

	header.size = ALIGN(obj->size, 4) >> 2;
	header.gpuaddr = obj->gpuaddr;
	header.ptbase = (__u32)obj->ptbase;
	header.type = obj->type;

	ret = obj_itr_out(itr, &header, sizeof(header));
	if (ret == 0)
		return 0;

	ret = obj_itr_out(itr, obj->entry->memdesc.hostptr + obj->offset,
		obj->size);
	if (ret == 0)
		return 0;

	/* Pad the end to a dword boundary if we need to */

	if (obj->size % 4) {
		unsigned int dummy = 0;
		ret = obj_itr_out(itr, &dummy, obj->size % 4);
	}

	return ret;
}

static void kgsl_snapshot_put_object(struct kgsl_device *device,
	struct kgsl_snapshot_object *obj)
{
	list_del(&obj->node);

	obj->entry->memdesc.priv &= ~KGSL_MEMDESC_FROZEN;
	kgsl_mem_entry_put(obj->entry);

	kfree(obj);
}

/* ksgl_snapshot_have_object - Return 1 if the object has been processed
 *@device - the device that is being snapshotted
 * @ptbase - the pagetable base of the object to freeze
 * @gpuaddr - The gpu address of the object to freeze
 * @size - the size of the object (may not always be the size of the region)
 *
 * Return 1 if the object is already in the list - this can save us from
 * having to parse the sme thing over again.
*/
int kgsl_snapshot_have_object(struct kgsl_device *device, phys_addr_t ptbase,
	unsigned int gpuaddr, unsigned int size)
{
	struct kgsl_snapshot_object *obj;

	list_for_each_entry(obj, &device->snapshot_obj_list, node) {
		if (obj->ptbase != ptbase)
			continue;

		if ((gpuaddr >= obj->gpuaddr) &&
			((gpuaddr + size) <= (obj->gpuaddr + obj->size)))
			return 1;
	}

	return 0;
}
EXPORT_SYMBOL(kgsl_snapshot_have_object);

/* kgsl_snapshot_get_object - Mark a GPU buffer to be frozen
 * @device - the device that is being snapshotted
 * @ptbase - the pagetable base of the object to freeze
 * @gpuaddr - The gpu address of the object to freeze
 * @size - the size of the object (may not always be the size of the region)
 * @type - the type of object being saved (shader, vbo, etc)
 *
 * Mark and freeze a GPU buffer object.  This will prevent it from being
 * freed until it can be copied out as part of the snapshot dump.  Returns the
 * size of the object being frozen
 */

int kgsl_snapshot_get_object(struct kgsl_device *device, phys_addr_t ptbase,
	unsigned int gpuaddr, unsigned int size, unsigned int type)
{
	struct kgsl_mem_entry *entry;
	struct kgsl_snapshot_object *obj;
	int offset;
	int ret = -EINVAL;

	if (!gpuaddr)
		return 0;

	entry = kgsl_get_mem_entry(device, ptbase, gpuaddr, size);

	if (entry == NULL) {
		KGSL_DRV_ERR(device, "Unable to find GPU buffer %8.8X\n",
				gpuaddr);
		return -EINVAL;
	}

	/* We can't freeze external memory, because we don't own it */
	if (entry->memtype != KGSL_MEM_ENTRY_KERNEL) {
		KGSL_DRV_ERR(device,
			"Only internal GPU buffers can be frozen\n");
		goto err_put;
	}

	/*
	 * size indicates the number of bytes in the region to save. This might
	 * not always be the entire size of the region because some buffers are
	 * sub-allocated from a larger region.  However, if size 0 was passed
	 * thats a flag that the caller wants to capture the entire buffer
	 */

	if (size == 0) {
		size = entry->memdesc.size;
		offset = 0;

		/* Adjust the gpuaddr to the start of the object */
		gpuaddr = entry->memdesc.gpuaddr;
	} else {
		offset = gpuaddr - entry->memdesc.gpuaddr;
	}

	if (size + offset > entry->memdesc.size) {
		KGSL_DRV_ERR(device, "Invalid size for GPU buffer %8.8X\n",
				gpuaddr);
		goto err_put;
	}

	/* If the buffer is already on the list, skip it */
	list_for_each_entry(obj, &device->snapshot_obj_list, node) {
		if (obj->gpuaddr == gpuaddr && obj->ptbase == ptbase) {
			/* If the size is different, use the bigger size */
			if (obj->size < size)
				obj->size = size;
			ret = 0;
			goto err_put;
		}
	}

	if (kgsl_memdesc_map(&entry->memdesc) == NULL) {
		KGSL_DRV_ERR(device, "Unable to map GPU buffer %X\n",
				gpuaddr);
		goto err_put;
	}

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	if (obj == NULL) {
		KGSL_DRV_ERR(device, "Unable to allocate memory\n");
		goto err_put;
	}

	obj->type = type;
	obj->entry = entry;
	obj->gpuaddr = gpuaddr;
	obj->ptbase = ptbase;
	obj->size = size;
	obj->offset = offset;

	list_add(&obj->node, &device->snapshot_obj_list);

	/*
	 * Return the size of the entire mem entry that was frozen - this gets
	 * used for tracking how much memory is frozen for a hang.  Also, mark
	 * the memory entry as frozen. If the entry was already marked as
	 * frozen, then another buffer already got to it.  In that case, return
	 * 0 so it doesn't get counted twice
	 */

	ret = (entry->memdesc.priv & KGSL_MEMDESC_FROZEN) ? 0
		: entry->memdesc.size;

	entry->memdesc.priv |= KGSL_MEMDESC_FROZEN;

	return ret;
err_put:
	kgsl_mem_entry_put(entry);
	return ret;
}
EXPORT_SYMBOL(kgsl_snapshot_get_object);

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
	struct kgsl_snapshot_registers_list *list = priv;

	struct kgsl_snapshot_regs *header = snapshot;
	struct kgsl_snapshot_registers *regs;
	unsigned int *data = snapshot + sizeof(*header);
	int count = 0, i, j, k;

	/* Figure out how many registers we are going to dump */

	for (i = 0; i < list->count; i++) {
		regs = &(list->registers[i]);

		for (j = 0; j < regs->count; j++) {
			int start = regs->regs[j * 2];
			int end = regs->regs[j * 2 + 1];

			count += (end - start + 1);
		}
	}

	if (remain < (count * 8) + sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}


	for (i = 0; i < list->count; i++) {
		regs = &(list->registers[i]);
		for (j = 0; j < regs->count; j++) {
			unsigned int start = regs->regs[j * 2];
			unsigned int end = regs->regs[j * 2 + 1];

			for (k = start; k <= end; k++) {
				unsigned int val;

				kgsl_regread(device, k, &val);
				*data++ = k;
				*data++ = val;
			}
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
	struct timespec boot;

	/* increment the hang count (on hang) for good book keeping */
	if (hang)
		device->snapshot_faultcount++;

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

	header->gpuid = kgsl_gpuid(device, &header->chipid);

	/* Get a pointer to the first section (right after the header) */
	snapshot = ((void *) device->snapshot) + sizeof(*header);

	/* Build the Linux specific header */
	snapshot = kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_OS,
		snapshot, &remain, snapshot_os, (void *) hang);

	/* Get the device specific sections */
	if (device->ftbl->snapshot)
		snapshot = device->ftbl->snapshot(device, snapshot, &remain,
			hang);

	/*
	 * The timestamp is the seconds since boot so it is easier to match to
	 * the kernel log
	 */

	getboottime(&boot);
	device->snapshot_timestamp = get_seconds() - boot.tv_sec;
	device->snapshot_size = (int) (snapshot - device->snapshot);

	/* Freeze the snapshot on a hang until it gets read */
	device->snapshot_frozen = (hang) ? 1 : 0;

	/* log buffer info to aid in ramdump fault tolerance */
	KGSL_DRV_ERR(device, "snapshot created at pa %lx size %d\n",
			__pa(device->snapshot),	device->snapshot_size);
	if (hang)
		sysfs_notify(&device->snapshot_kobj, NULL, "timestamp");
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
	struct kgsl_snapshot_object *obj, *tmp;
	struct kgsl_snapshot_section_header head;
	struct snapshot_obj_itr itr;
	int ret;

	if (device == NULL)
		return 0;

	/* Return nothing if we haven't taken a snapshot yet */
	if (device->snapshot_timestamp == 0)
		return 0;

	/* Get the mutex to keep things from changing while we are dumping */
	mutex_lock(&device->mutex);

	obj_itr_init(&itr, buf, off, count);

	ret = obj_itr_out(&itr, device->snapshot, device->snapshot_size);

	if (ret == 0)
		goto done;

	list_for_each_entry(obj, &device->snapshot_obj_list, node)
		kgsl_snapshot_dump_object(device, obj, &itr);

	{
		head.magic = SNAPSHOT_SECTION_MAGIC;
		head.id = KGSL_SNAPSHOT_SECTION_END;
		head.size = sizeof(head);

		obj_itr_out(&itr, &head, sizeof(head));
	}

	/*
	 * Make sure everything has been written out before destroying things.
	 * The best way to confirm this is to go all the way through without
	 * writing any bytes - so only release if we get this far and
	 * itr->write is 0
	 */

	if (itr.write == 0) {
		list_for_each_entry_safe(obj, tmp, &device->snapshot_obj_list,
			node)
			kgsl_snapshot_put_object(device, obj);

		if (device->snapshot_frozen)
			KGSL_DRV_ERR(device, "Snapshot objects released\n");

		device->snapshot_frozen = 0;
	}

done:
	mutex_unlock(&device->mutex);

	return itr.write;
}

/* Show the total number of hangs since device boot */
static ssize_t faultcount_show(struct kgsl_device *device, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", device->snapshot_faultcount);
}

/* Reset the total number of hangs since device boot */
static ssize_t faultcount_store(struct kgsl_device *device, const char *buf,
	size_t count)
{
	if (device && count > 0)
		device->snapshot_faultcount = 0;

	return count;
}

/* Show the timestamp of the last collected snapshot */
static ssize_t timestamp_show(struct kgsl_device *device, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", device->snapshot_timestamp);
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
SNAPSHOT_ATTR(faultcount, 0644, faultcount_show, faultcount_store);

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
		device->snapshot = kzalloc(KGSL_SNAPSHOT_MEMSIZE, GFP_KERNEL);

	if (device->snapshot == NULL)
		return -ENOMEM;

	device->snapshot_maxsize = KGSL_SNAPSHOT_MEMSIZE;
	device->snapshot_timestamp = 0;
	device->snapshot_faultcount = 0;

	INIT_LIST_HEAD(&device->snapshot_obj_list);

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
	if (ret)
		goto done;

	ret  = sysfs_create_file(&device->snapshot_kobj, &attr_faultcount.attr);

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

	kfree(device->snapshot);

	device->snapshot = NULL;
	device->snapshot_maxsize = 0;
	device->snapshot_timestamp = 0;
	device->snapshot_faultcount = 0;
}
EXPORT_SYMBOL(kgsl_device_snapshot_close);
