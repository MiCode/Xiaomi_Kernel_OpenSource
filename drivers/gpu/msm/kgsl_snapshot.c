// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/utsname.h>

#include "adreno_cp_parser.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"
#include "kgsl_snapshot.h"
#include "kgsl_util.h"

static void kgsl_snapshot_save_frozen_objs(struct work_struct *work);

/* Placeholder for list of ib objects that contain all objects in that IB */

struct kgsl_snapshot_cp_obj {
	struct adreno_ib_object_list *ib_obj_list;
	struct list_head node;
};

struct snapshot_obj_itr {
	u8 *buf;      /* Buffer pointer to write to */
	int pos;        /* Current position in the sequence */
	loff_t offset;  /* file offset to start writing from */
	size_t remain;  /* Bytes remaining in buffer */
	size_t write;   /* Bytes written so far */
};

static inline u64 snapshot_phy_addr(struct kgsl_device *device)
{
	return device->snapshot_memory.dma_handle ?
		device->snapshot_memory.dma_handle : __pa(device->snapshot_memory.ptr);
}

static inline u64 atomic_snapshot_phy_addr(struct kgsl_device *device)
{
	return device->snapshot_memory_atomic.ptr == device->snapshot_memory.ptr ?
		snapshot_phy_addr(device) : __pa(device->snapshot_memory_atomic.ptr);
}

static void obj_itr_init(struct snapshot_obj_itr *itr, u8 *buf,
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

static void kgsl_snapshot_put_object(struct kgsl_snapshot_object *obj)
{
	list_del(&obj->node);

	obj->entry->memdesc.priv &= ~KGSL_MEMDESC_FROZEN;
	obj->entry->memdesc.priv &= ~KGSL_MEMDESC_SKIP_RECLAIM;
	kgsl_mem_entry_put(obj->entry);

	kfree(obj);
}

/**
 * kgsl_snapshot_have_object() - return 1 if the object has been processed
 * @snapshot: the snapshot data
 * @process: The process that owns the the object to freeze
 * @gpuaddr: The gpu address of the object to freeze
 * @size: the size of the object (may not always be the size of the region)
 *
 * Return 1 if the object is already in the list - this can save us from
 * having to parse the same thing over again. There are 2 lists that are
 * tracking objects so check for the object in both lists
 */
int kgsl_snapshot_have_object(struct kgsl_snapshot *snapshot,
	struct kgsl_process_private *process,
	uint64_t gpuaddr, uint64_t size)
{
	struct kgsl_snapshot_object *obj;
	struct kgsl_snapshot_cp_obj *obj_cp;
	struct adreno_ib_object *ib_obj;
	int i;

	/* Check whether the object is tracked already in ib list */
	list_for_each_entry(obj_cp, &snapshot->cp_list, node) {
		if (obj_cp->ib_obj_list == NULL
			|| obj_cp->ib_obj_list->num_objs == 0)
			continue;

		ib_obj = &(obj_cp->ib_obj_list->obj_list[0]);
		if (ib_obj->entry == NULL || ib_obj->entry->priv != process)
			continue;

		for (i = 0; i < obj_cp->ib_obj_list->num_objs; i++) {
			ib_obj = &(obj_cp->ib_obj_list->obj_list[i]);
			if ((gpuaddr >= ib_obj->gpuaddr) &&
				((gpuaddr + size) <=
				(ib_obj->gpuaddr + ib_obj->size)))
				return 1;
		}
	}

	list_for_each_entry(obj, &snapshot->obj_list, node) {
		if (obj->entry == NULL || obj->entry->priv != process)
			continue;

		if ((gpuaddr >= obj->gpuaddr) &&
			((gpuaddr + size) <= (obj->gpuaddr + obj->size)))
			return 1;
	}

	return 0;
}

/**
 * kgsl_snapshot_get_object() - Mark a GPU buffer to be frozen
 * @snapshot: The snapshot data
 * @process: The process that owns the object we want to freeze
 * @gpuaddr: The gpu address of the object to freeze
 * @size: the size of the object (may not always be the size of the region)
 * @type: the type of object being saved (shader, vbo, etc)
 *
 * Mark and freeze a GPU buffer object.  This will prevent it from being
 * freed until it can be copied out as part of the snapshot dump.  Returns the
 * size of the object being frozen
 */
int kgsl_snapshot_get_object(struct kgsl_snapshot *snapshot,
	struct kgsl_process_private *process, uint64_t gpuaddr,
	uint64_t size, unsigned int type)
{
	struct kgsl_mem_entry *entry;
	struct kgsl_snapshot_object *obj;
	uint64_t offset;
	int ret = -EINVAL;
	unsigned int mem_type;

	if (!gpuaddr)
		return 0;

	entry = kgsl_sharedmem_find(process, gpuaddr);

	if (entry == NULL)
		return -EINVAL;

	/* We can't freeze external memory, because we don't own it */
	if (entry->memdesc.flags & KGSL_MEMFLAGS_USERMEM_MASK)
		goto err_put;
	/*
	 * Do not save texture and render targets in snapshot,
	 * they can be just too big
	 */

	mem_type = kgsl_memdesc_get_memtype(&entry->memdesc);
	if (mem_type == KGSL_MEMTYPE_TEXTURE ||
		mem_type == KGSL_MEMTYPE_EGL_SURFACE ||
		mem_type == KGSL_MEMTYPE_EGL_IMAGE) {
		ret = 0;
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
		dev_err(snapshot->device->dev,
			"snapshot: invalid size for GPU buffer 0x%016llx\n",
			gpuaddr);
		goto err_put;
	}

	/* If the buffer is already on the list, skip it */
	list_for_each_entry(obj, &snapshot->obj_list, node) {
		/* combine the range with existing object if they overlap */
		if (obj->entry->priv == process && obj->type == type &&
			kgsl_addr_range_overlap(obj->gpuaddr, obj->size,
				gpuaddr, size)) {
			uint64_t end1 = obj->gpuaddr + obj->size;
			uint64_t end2 = gpuaddr + size;

			if (obj->gpuaddr > gpuaddr)
				obj->gpuaddr = gpuaddr;
			if (end1 > end2)
				obj->size = end1 - obj->gpuaddr;
			else
				obj->size = end2 - obj->gpuaddr;
			obj->offset = obj->gpuaddr - entry->memdesc.gpuaddr;
			ret = 0;
			goto err_put;
		}
	}

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	if (obj == NULL)
		goto err_put;

	obj->type = type;
	obj->entry = entry;
	obj->gpuaddr = gpuaddr;
	obj->size = size;
	obj->offset = offset;

	list_add(&obj->node, &snapshot->obj_list);

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
	entry->memdesc.priv &= ~KGSL_MEMDESC_SKIP_RECLAIM;
	kgsl_mem_entry_put(entry);
	return ret;
}

/**
 * kgsl_snapshot_dump_registers - helper function to dump device registers
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
size_t kgsl_snapshot_dump_registers(struct kgsl_device *device, u8 *buf,
	size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header = (struct kgsl_snapshot_regs *)buf;
	struct kgsl_snapshot_registers *regs = priv;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int count = 0, j, k;

	/* Figure out how many registers we are going to dump */

	for (j = 0; j < regs->count; j++) {
		int start = regs->regs[j * 2];
		int end = regs->regs[j * 2 + 1];

		count += (end - start + 1);
	}

	if (remain < (count * 8) + sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

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

	header->count = count;

	/* Return the size of the section */
	return (count * 8) + sizeof(*header);
}

struct kgsl_snapshot_indexed_registers {
	unsigned int index;
	unsigned int data;
	unsigned int start;
	unsigned int count;
};

static size_t kgsl_snapshot_dump_indexed_regs(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_indexed_registers *iregs = priv;
	struct kgsl_snapshot_indexed_regs *header =
		(struct kgsl_snapshot_indexed_regs *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));

	if (remain < (iregs->count * 4) + sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "INDEXED REGS");
		return 0;
	}

	header->index_reg = iregs->index;
	header->data_reg = iregs->data;
	header->count = iregs->count;
	header->start = iregs->start;

	kgsl_regmap_read_indexed_interleaved(&device->regmap, iregs->index,
		iregs->data, data, iregs->start, iregs->count);

	return (iregs->count * 4) + sizeof(*header);
}

/**
 * kgsl_snapshot_indexed_registers - Add a set of indexed registers to the
 * snapshot
 * @device: Pointer to the KGSL device being snapshotted
 * @snapshot: Snapshot instance
 * @index: Offset for the index register
 * @data: Offset for the data register
 * @start: Index to start reading
 * @count: Number of entries to read
 *
 * Dump the values from an indexed register group into the snapshot
 */
void kgsl_snapshot_indexed_registers(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		unsigned int index, unsigned int data,
		unsigned int start,
		unsigned int count)
{
	struct kgsl_snapshot_indexed_registers iregs;

	iregs.index = index;
	iregs.data = data;
	iregs.start = start;
	iregs.count = count;

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_INDEXED_REGS,
		snapshot, kgsl_snapshot_dump_indexed_regs, &iregs);
}

/**
 * kgsl_snapshot_add_section() - Add a new section to the GPU snapshot
 * @device: the KGSL device being snapshotted
 * @id: the section id
 * @snapshot: pointer to the snapshot instance
 * @func:  Function pointer to fill the section
 * @priv: Private pointer to pass to the function
 *
 * Set up a KGSL snapshot header by filling the memory with the callback
 * function and adding the standard section header
 */
void kgsl_snapshot_add_section(struct kgsl_device *device, u16 id,
	struct kgsl_snapshot *snapshot,
	size_t (*func)(struct kgsl_device *, u8 *, size_t, void *),
	void *priv)
{
	struct kgsl_snapshot_section_header *header =
		(struct kgsl_snapshot_section_header *)snapshot->ptr;
	u8 *data = snapshot->ptr + sizeof(*header);
	size_t ret = 0;

	/*
	 * Sanity check to make sure there is enough for the header.  The
	 * callback will check to make sure there is enough for the rest
	 * of the data.  If there isn't enough room then don't advance the
	 * pointer.
	 */

	if (snapshot->remain < sizeof(*header))
		return;

	/* It is legal to have no function (i.e. - make an empty section) */
	if (func) {
		ret = func(device, data, snapshot->remain - sizeof(*header),
			priv);

		/*
		 * If there wasn't enough room for the data then don't bother
		 * setting up the header.
		 */

		if (ret == 0)
			return;
	}

	header->magic = SNAPSHOT_SECTION_MAGIC;
	header->id = id;
	header->size = ret + sizeof(*header);

	snapshot->ptr += header->size;
	snapshot->remain -= header->size;
	snapshot->size += header->size;
}

static void kgsl_free_snapshot(struct kgsl_snapshot *snapshot)
{
	struct kgsl_snapshot_object *obj, *tmp;
	struct kgsl_device *device = snapshot->device;

	wait_for_completion(&snapshot->dump_gate);

	list_for_each_entry_safe(obj, tmp,
				&snapshot->obj_list, node)
		kgsl_snapshot_put_object(obj);

	if (snapshot->mempool)
		vfree(snapshot->mempool);

	kfree(snapshot);
	dev_err(device->dev, "snapshot: objects released\n");
}

#define SP0_ISDB_ISDB_BRKPT_CFG 0x40014
#define SP0_ISDB_ISDB_EN 0x40004
#define SP0_ISDB_ISDB_CMD 0x4000C

static void isdb_write(void __iomem *base, u32 offset)
{
	/* To set the SCHBREAKTYPE bit */
	__raw_writel(0x801, base + SP0_ISDB_ISDB_BRKPT_CFG + offset);

	/*
	 * ensure the configurations are set before
	 * enabling ISDB
	 */
	wmb();
	/* To set the ISDBCLKON and ISDB_EN bits*/
	__raw_writel(0x03, base + SP0_ISDB_ISDB_EN + offset);

	/*
	 * ensure previous write to enable isdb posts
	 * before issuing the break command
	 */
	wmb();
	/*To issue ISDB_0_ISDB_CMD_BREAK*/
	__raw_writel(0x1, base + SP0_ISDB_ISDB_CMD + offset);
}

static void set_isdb_breakpoint_registers(struct kgsl_device *device)
{
	struct clk *clk;
	int ret;

	if (!device->set_isdb_breakpoint || device->ftbl->is_hwcg_on(device)
					|| device->qdss_gfx_virt == NULL)
		return;

	clk = clk_get(&device->pdev->dev, "apb_pclk");

	if (IS_ERR(clk)) {
		dev_err(device->dev, "Unable to get QDSS clock\n");
		goto err;
	}

	ret = clk_prepare_enable(clk);

	if (ret) {
		dev_err(device->dev, "QDSS Clock enable error: %d\n", ret);
		clk_put(clk);
		goto err;
	}

	/* Issue break command for all eight SPs */
	isdb_write(device->qdss_gfx_virt, 0x0000);
	isdb_write(device->qdss_gfx_virt, 0x1000);
	isdb_write(device->qdss_gfx_virt, 0x2000);
	isdb_write(device->qdss_gfx_virt, 0x3000);
	isdb_write(device->qdss_gfx_virt, 0x4000);
	isdb_write(device->qdss_gfx_virt, 0x5000);
	isdb_write(device->qdss_gfx_virt, 0x6000);
	isdb_write(device->qdss_gfx_virt, 0x7000);

	clk_disable_unprepare(clk);
	clk_put(clk);

	return;

err:
	/* Do not force kernel panic if isdb writes did not go through */
	device->force_panic = false;
}

static void kgsl_device_snapshot_atomic(struct kgsl_device *device)
{
	struct kgsl_snapshot *snapshot;
	struct timespec64 boot;

	if (device->snapshot && device->force_panic)
		return;

	if (!atomic_read(&device->active_cnt)) {
		dev_err(device->dev, "snapshot: device is powered off\n");
		return;
	}

	device->snapshot_memory_atomic.size = device->snapshot_memory.size;
	if (!device->snapshot_faultcount) {
		/* Use non-atomic snapshot memory if it is unused */
		device->snapshot_memory_atomic.ptr = device->snapshot_memory.ptr;
	} else {
		/* Limit size to 3MB to avoid failure for atomic snapshot memory */
		if (device->snapshot_memory_atomic.size > (SZ_2M + SZ_1M))
			device->snapshot_memory_atomic.size = (SZ_2M + SZ_1M);

		device->snapshot_memory_atomic.ptr = devm_kzalloc(&device->pdev->dev,
					device->snapshot_memory_atomic.size, GFP_ATOMIC);

		/* If we fail to allocate more than 1MB fall back to 1MB */
		if (WARN_ON((!device->snapshot_memory_atomic.ptr) &&
			device->snapshot_memory_atomic.size > SZ_1M)) {
			device->snapshot_memory_atomic.size = SZ_1M;
			device->snapshot_memory_atomic.ptr = devm_kzalloc(&device->pdev->dev,
					device->snapshot_memory_atomic.size, GFP_ATOMIC);
		}

		if (!device->snapshot_memory_atomic.ptr) {
			dev_err(device->dev,
				"Failed to allocate memory for atomic snapshot\n");
			return;
		}
	}

	/* Allocate memory for the snapshot instance */
	snapshot = kzalloc(sizeof(*snapshot), GFP_ATOMIC);
	if (snapshot == NULL)
		return;

	device->snapshot_atomic = true;
	INIT_LIST_HEAD(&snapshot->obj_list);
	INIT_LIST_HEAD(&snapshot->cp_list);

	snapshot->start = device->snapshot_memory_atomic.ptr;
	snapshot->ptr = device->snapshot_memory_atomic.ptr;
	snapshot->remain = device->snapshot_memory_atomic.size;

	/*
	 * Trigger both GPU and GMU snapshot. GPU specific code
	 * will take care of whether to dumps full state or only
	 * GMU state based on current GPU power state.
	 */
	if (device->ftbl->snapshot)
		device->ftbl->snapshot(device, snapshot, NULL);

	/*
	 * The timestamp is the seconds since boot so it is easier to match to
	 * the kernel log
	 */
	getboottime64(&boot);
	snapshot->timestamp = get_seconds() - boot.tv_sec;

	kgsl_add_to_minidump("ATOMIC_GPU_SNAPSHOT", (u64) device->snapshot_memory_atomic.ptr,
		atomic_snapshot_phy_addr(device), device->snapshot_memory_atomic.size);

	/* log buffer info to aid in ramdump fault tolerance */
	dev_err(device->dev, "Atomic GPU snapshot created at pa %llx++0x%zx\n",
			atomic_snapshot_phy_addr(device), snapshot->size);
}

/**
 * kgsl_snapshot() - construct a device snapshot
 * @device: device to snapshot
 * @context: the context that is hung, might be NULL if unknown.
 * @gmu_fault: whether this snapshot is triggered by a GMU fault.
 *
 * Given a device, construct a binary snapshot dump of the current device state
 * and store it in the device snapshot memory.
 */
void kgsl_device_snapshot(struct kgsl_device *device,
		struct kgsl_context *context, bool gmu_fault)
{
	struct kgsl_snapshot *snapshot;
	struct timespec64 boot;

	set_isdb_breakpoint_registers(device);

	if (device->snapshot_memory.ptr == NULL) {
		dev_err(device->dev,
			     "snapshot: no snapshot memory available\n");
		return;
	}

	if (WARN(!kgsl_state_is_awake(device),
		"snapshot: device is powered off\n"))
		return;

	/* increment the hang count for good book keeping */
	device->snapshot_faultcount++;

	if (device->snapshot != NULL) {

		/*
		 * Snapshot over-write policy:
		 * 1. By default, don't over-write the very first snapshot,
		 *    be it a gmu or gpu fault.
		 * 2. Never over-write existing snapshot on a gpu fault.
		 * 3. Never over-write a snapshot that we didn't recover from.
		 * 4. In order to over-write a new gmu fault snapshot with a
		 *    previously recovered fault, then set the sysfs knob
		 *    prioritize_recoverable to true.
		 */
		if (!device->prioritize_unrecoverable ||
			!device->snapshot->recovered || !gmu_fault)
			return;

		/*
		 * If another thread is currently reading it, that thread
		 * will free it, otherwise free it now.
		 */
		if (!device->snapshot->sysfs_read)
			kgsl_free_snapshot(device->snapshot);
		device->snapshot = NULL;
	}

	/* Allocate memory for the snapshot instance */
	snapshot = kzalloc(sizeof(*snapshot), GFP_KERNEL);
	if (snapshot == NULL)
		return;

	init_completion(&snapshot->dump_gate);
	INIT_LIST_HEAD(&snapshot->obj_list);
	INIT_LIST_HEAD(&snapshot->cp_list);
	INIT_WORK(&snapshot->work, kgsl_snapshot_save_frozen_objs);

	snapshot->start = device->snapshot_memory.ptr;
	snapshot->ptr = device->snapshot_memory.ptr;
	snapshot->remain = device->snapshot_memory.size;
	snapshot->recovered = false;
	snapshot->first_read = true;
	snapshot->sysfs_read = 0;

	device->ftbl->snapshot(device, snapshot, context);

	/*
	 * The timestamp is the seconds since boot so it is easier to match to
	 * the kernel log
	 */

	getboottime64(&boot);
	snapshot->timestamp = get_seconds() - boot.tv_sec;

	/* Store the instance in the device until it gets dumped */
	device->snapshot = snapshot;
	snapshot->device = device;

	/* log buffer info to aid in ramdump fault tolerance */
	dev_err(device->dev, "%s snapshot created at pa %llx++0x%zx\n",
			gmu_fault ? "GMU" : "GPU", snapshot_phy_addr(device),
			snapshot->size);

	kgsl_add_to_minidump("GPU_SNAPSHOT", (u64) device->snapshot_memory.ptr,
			snapshot_phy_addr(device), device->snapshot_memory.size);

	if (device->skip_ib_capture)
		BUG_ON(device->force_panic);

	sysfs_notify(&device->snapshot_kobj, NULL, "timestamp");

	/*
	 * Queue a work item that will save the IB data in snapshot into
	 * static memory to prevent loss of data due to overwriting of
	 * memory.
	 *
	 */
	kgsl_schedule_work(&snapshot->work);
}

/* An attribute for showing snapshot details */
struct kgsl_snapshot_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kgsl_device *device, char *buf);
	ssize_t (*store)(struct kgsl_device *device, const char *buf,
		size_t count);
};

/**
 * kgsl_snapshot_process_ib_obj_list() - Go through the list of IB's which need
 * to be dumped for snapshot and move them to the global snapshot list so
 * they will get dumped when the global list is dumped
 * @device: device being snapshotted
 */
static void kgsl_snapshot_process_ib_obj_list(struct kgsl_snapshot *snapshot)
{
	struct kgsl_snapshot_cp_obj *obj, *obj_temp;
	struct adreno_ib_object *ib_obj;
	int i;

	list_for_each_entry_safe(obj, obj_temp, &snapshot->cp_list,
			node) {
		for (i = 0; i < obj->ib_obj_list->num_objs; i++) {
			ib_obj = &(obj->ib_obj_list->obj_list[i]);
			kgsl_snapshot_get_object(snapshot, ib_obj->entry->priv,
				ib_obj->gpuaddr, ib_obj->size,
				ib_obj->snapshot_obj_type);
		}
		list_del(&obj->node);
		adreno_ib_destroy_obj_list(obj->ib_obj_list);
		kfree(obj);
	}
}

#define to_snapshot_attr(a) \
container_of(a, struct kgsl_snapshot_attribute, attr)

#define kobj_to_device(a) \
container_of(a, struct kgsl_device, snapshot_kobj)

static int snapshot_release(struct kgsl_device *device,
	struct kgsl_snapshot *snapshot)
{
	bool snapshot_free = false;
	int ret = 0;

	mutex_lock(&device->mutex);
	snapshot->sysfs_read--;

	/*
	 * If someone's replaced the snapshot, return an error and free
	 * the snapshot if this is the last thread to read it.
	 */
	if (device->snapshot != snapshot) {
		ret = -EIO;
		if (!snapshot->sysfs_read)
			snapshot_free = true;
	}
	mutex_unlock(&device->mutex);
	if (snapshot_free)
		kgsl_free_snapshot(snapshot);
	return ret;
}

/* Dump the sysfs binary data to the user */
static ssize_t snapshot_show(struct file *filep, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off,
	size_t count)
{
	struct kgsl_device *device = kobj_to_device(kobj);
	struct kgsl_snapshot *snapshot;
	struct kgsl_snapshot_section_header head;
	struct snapshot_obj_itr itr;
	int ret = 0;

	mutex_lock(&device->mutex);
	snapshot = device->snapshot;
	if (snapshot != NULL) {
		/*
		 * If we're reading at a non-zero offset from a new snapshot,
		 * that means we want to read from the previous snapshot (which
		 * was overwritten), so return an error
		 */
		if (snapshot->first_read) {
			if (off)
				ret = -EIO;
			else
				snapshot->first_read = false;
		}
		if (!ret)
			snapshot->sysfs_read++;
	}
	mutex_unlock(&device->mutex);

	if (ret)
		return ret;

	/* Return nothing if we haven't taken a snapshot yet */
	if (snapshot == NULL)
		return 0;

	/*
	 * Wait for the dump worker to finish. This is interruptible
	 * to allow userspace to bail if things go horribly wrong.
	 */
	ret = wait_for_completion_interruptible(&snapshot->dump_gate);
	if (ret) {
		snapshot_release(device, snapshot);
		return ret;
	}

	obj_itr_init(&itr, buf, off, count);

	ret = obj_itr_out(&itr, snapshot->start, snapshot->size);
	if (ret == 0)
		goto done;

	/* Dump the memory pool if it exists */
	if (snapshot->mempool) {
		ret = obj_itr_out(&itr, snapshot->mempool,
				snapshot->mempool_size);
		if (ret == 0)
			goto done;
	}

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
	 * itr->write is 0 and there are no concurrent reads pending
	 */

	if (itr.write == 0) {
		bool snapshot_free = false;

		mutex_lock(&device->mutex);
		if (--snapshot->sysfs_read == 0) {
			if (device->snapshot == snapshot)
				device->snapshot = NULL;
			snapshot_free = true;
		}
		mutex_unlock(&device->mutex);

		if (snapshot_free)
			kgsl_free_snapshot(snapshot);
		return 0;
	}

done:
	ret = snapshot_release(device, snapshot);
	return (ret < 0) ? ret : itr.write;
}

/* Show the total number of hangs since device boot */
static ssize_t faultcount_show(struct kgsl_device *device, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", device->snapshot_faultcount);
}

/* Reset the total number of hangs since device boot */
static ssize_t faultcount_store(struct kgsl_device *device, const char *buf,
	size_t count)
{
	if (count)
		device->snapshot_faultcount = 0;

	return count;
}

/* Show the force_panic request status */
static ssize_t force_panic_show(struct kgsl_device *device, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", device->force_panic);
}

/* Store the panic request value to force_panic */
static ssize_t force_panic_store(struct kgsl_device *device, const char *buf,
	size_t count)
{
	if (strtobool(buf, &device->force_panic))
		return -EINVAL;
	return count;
}

/* Show the break_ib request status */
static ssize_t skip_ib_capture_show(struct kgsl_device *device, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", device->skip_ib_capture);
}

/* Store the panic request value to break_ib */
static ssize_t skip_ib_capture_store(struct kgsl_device *device,
						const char *buf, size_t count)
{
	int ret;

	ret = kstrtobool(buf, &device->skip_ib_capture);
	return ret ? ret : count;
}

/* Show the prioritize_unrecoverable status */
static ssize_t prioritize_unrecoverable_show(
		struct kgsl_device *device, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
			device->prioritize_unrecoverable);
}

/* Store the priority value to prioritize unrecoverable */
static ssize_t prioritize_unrecoverable_store(
		struct kgsl_device *device, const char *buf, size_t count)
{
	if (strtobool(buf, &device->prioritize_unrecoverable))
		return -EINVAL;

	return count;
}

/* Show the snapshot_crashdumper request status */
static ssize_t snapshot_crashdumper_show(struct kgsl_device *device, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", device->snapshot_crashdumper);
}


/* Store the value to snapshot_crashdumper*/
static ssize_t snapshot_crashdumper_store(struct kgsl_device *device,
	const char *buf, size_t count)
{
	if (strtobool(buf, &device->snapshot_crashdumper))
		return -EINVAL;
	return count;
}

/* Show the timestamp of the last collected snapshot */
static ssize_t timestamp_show(struct kgsl_device *device, char *buf)
{
	unsigned long timestamp;

	mutex_lock(&device->mutex);
	timestamp = device->snapshot ? device->snapshot->timestamp : 0;
	mutex_unlock(&device->mutex);
	return scnprintf(buf, PAGE_SIZE, "%lu\n", timestamp);
}

static ssize_t snapshot_legacy_show(struct kgsl_device *device, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", device->snapshot_legacy);
}

static ssize_t snapshot_legacy_store(struct kgsl_device *device,
	const char *buf, size_t count)
{
	if (strtobool(buf, &device->snapshot_legacy))
		return -EINVAL;

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

static SNAPSHOT_ATTR(timestamp, 0444, timestamp_show, NULL);
static SNAPSHOT_ATTR(faultcount, 0644, faultcount_show, faultcount_store);
static SNAPSHOT_ATTR(force_panic, 0644, force_panic_show, force_panic_store);
static SNAPSHOT_ATTR(prioritize_unrecoverable, 0644,
		prioritize_unrecoverable_show, prioritize_unrecoverable_store);
static SNAPSHOT_ATTR(snapshot_crashdumper, 0644, snapshot_crashdumper_show,
	snapshot_crashdumper_store);
static SNAPSHOT_ATTR(snapshot_legacy, 0644, snapshot_legacy_show,
	snapshot_legacy_store);
static SNAPSHOT_ATTR(skip_ib_capture, 0644, skip_ib_capture_show,
		skip_ib_capture_store);

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
	ssize_t ret = -EIO;

	if (pattr->store)
		ret = pattr->store(device, buf, count);

	return ret;
}

static const struct sysfs_ops snapshot_sysfs_ops = {
	.show = snapshot_sysfs_show,
	.store = snapshot_sysfs_store,
};

static struct kobj_type ktype_snapshot = {
	.sysfs_ops = &snapshot_sysfs_ops,
};

static const struct attribute *snapshot_attrs[] = {
	&attr_timestamp.attr,
	&attr_faultcount.attr,
	&attr_force_panic.attr,
	&attr_prioritize_unrecoverable.attr,
	&attr_snapshot_crashdumper.attr,
	&attr_snapshot_legacy.attr,
	&attr_skip_ib_capture.attr,
	NULL,
};

static int kgsl_panic_notifier_callback(struct notifier_block *nb,
		unsigned long action, void *unused)
{
	struct kgsl_device *device = container_of(nb, struct kgsl_device,
							panic_nb);

	/* To send NMI to GMU */
	device->gmu_fault = true;
	kgsl_device_snapshot_atomic(device);

	return NOTIFY_OK;
}

void kgsl_device_snapshot_probe(struct kgsl_device *device, u32 size)
{
	device->snapshot_memory.size = size;

	device->snapshot_memory.ptr = dma_alloc_coherent(&device->pdev->dev,
		device->snapshot_memory.size, &device->snapshot_memory.dma_handle,
		GFP_KERNEL);
	/*
	 * If we fail to allocate more than 1MB for snapshot fall back
	 * to 1MB
	 */
	if (WARN_ON((!device->snapshot_memory.ptr) && size > SZ_1M)) {
		device->snapshot_memory.size = SZ_1M;
		device->snapshot_memory.ptr = devm_kzalloc(&device->pdev->dev,
			device->snapshot_memory.size, GFP_KERNEL);
	}

	if (!device->snapshot_memory.ptr) {
		dev_err(device->dev,
			"KGSL failed to allocate memory for snapshot\n");
		return;
	}

	device->snapshot = NULL;
	device->snapshot_faultcount = 0;
	device->force_panic = false;
	device->snapshot_crashdumper = true;
	device->snapshot_legacy = false;

	device->snapshot_atomic = false;
	device->panic_nb.notifier_call = kgsl_panic_notifier_callback;
	device->panic_nb.priority = 1;
	device->snapshot_ctxt_record_size = 64 * 1024;

	/*
	 * Set this to false so that we only ever keep the first snapshot around
	 * If we want to over-write with a gmu snapshot, then set it to true
	 * via sysfs
	 */
	device->prioritize_unrecoverable = false;

	if (kobject_init_and_add(&device->snapshot_kobj, &ktype_snapshot,
		&device->dev->kobj, "snapshot"))
		return;

	WARN_ON(sysfs_create_bin_file(&device->snapshot_kobj, &snapshot_attr));
	WARN_ON(sysfs_create_files(&device->snapshot_kobj, snapshot_attrs));
	atomic_notifier_chain_register(&panic_notifier_list,
			&device->panic_nb);
}

/**
 * kgsl_device_snapshot_close() - take down snapshot memory for a device
 * @device: Pointer to the kgsl_device
 *
 * Remove the sysfs files and free the memory allocated for the GPU
 * snapshot
 */
void kgsl_device_snapshot_close(struct kgsl_device *device)
{
	kgsl_remove_from_minidump("GPU_SNAPSHOT", (u64) device->snapshot_memory.ptr,
			snapshot_phy_addr(device), device->snapshot_memory.size);

	sysfs_remove_bin_file(&device->snapshot_kobj, &snapshot_attr);
	sysfs_remove_files(&device->snapshot_kobj, snapshot_attrs);

	kobject_put(&device->snapshot_kobj);

	if (device->snapshot_memory.dma_handle)
		dma_free_coherent(&device->pdev->dev, device->snapshot_memory.size,
			device->snapshot_memory.ptr, device->snapshot_memory.dma_handle);
}

/**
 * kgsl_snapshot_add_ib_obj_list() - Add a IB object list to the snapshot
 * object list
 * @device: the device that is being snapshotted
 * @ib_obj_list: The IB list that has objects required to execute an IB
 * @num_objs: Number of IB objects
 * @ptbase: The pagetable base in which the IB is mapped
 *
 * Adds a new IB to the list of IB objects maintained when getting snapshot
 * Returns 0 on success else -ENOMEM on error
 */
int kgsl_snapshot_add_ib_obj_list(struct kgsl_snapshot *snapshot,
	struct adreno_ib_object_list *ib_obj_list)
{
	struct kgsl_snapshot_cp_obj *obj;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return -ENOMEM;
	obj->ib_obj_list = ib_obj_list;
	list_add(&obj->node, &snapshot->cp_list);
	return 0;
}

static size_t _mempool_add_object(struct kgsl_snapshot *snapshot, u8 *data,
		struct kgsl_snapshot_object *obj)
{
	struct kgsl_snapshot_section_header *section =
		(struct kgsl_snapshot_section_header *)data;
	struct kgsl_snapshot_gpu_object_v2 *header =
		(struct kgsl_snapshot_gpu_object_v2 *)(data + sizeof(*section));
	u8 *dest = data + sizeof(*section) + sizeof(*header);
	uint64_t size;

	size = obj->size;

	if (!kgsl_memdesc_map(&obj->entry->memdesc)) {
		dev_err(snapshot->device->dev,
			"snapshot: failed to map GPU object\n");
		return 0;
	}

	section->magic = SNAPSHOT_SECTION_MAGIC;
	section->id = KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2;
	section->size = size + sizeof(*header) + sizeof(*section);

	header->size = size >> 2;
	header->gpuaddr = obj->gpuaddr;
	header->ptbase =
		kgsl_mmu_pagetable_get_ttbr0(obj->entry->priv->pagetable);
	header->type = obj->type;

	if (kgsl_addr_range_overlap(obj->gpuaddr, obj->size,
				snapshot->ib1base, snapshot->ib1size))
		snapshot->ib1dumped = true;

	if (kgsl_addr_range_overlap(obj->gpuaddr, obj->size,
				snapshot->ib2base, snapshot->ib2size))
		snapshot->ib2dumped = true;

	memcpy(dest, obj->entry->memdesc.hostptr + obj->offset, size);
	kgsl_memdesc_unmap(&obj->entry->memdesc);

	return section->size;
}

/**
 * kgsl_snapshot_save_frozen_objs() - Save the objects frozen in snapshot into
 * memory so that the data reported in these objects is correct when snapshot
 * is taken
 * @work: The work item that scheduled this work
 */
static void kgsl_snapshot_save_frozen_objs(struct work_struct *work)
{
	struct kgsl_snapshot *snapshot = container_of(work,
				struct kgsl_snapshot, work);
	struct kgsl_snapshot_object *obj, *tmp;
	size_t size = 0;
	void *ptr;

	if (snapshot->device->gmu_fault)
		goto gmu_only;

	kgsl_snapshot_process_ib_obj_list(snapshot);

	list_for_each_entry(obj, &snapshot->obj_list, node) {
		obj->size = ALIGN(obj->size, 4);

		size += ((size_t) obj->size +
			sizeof(struct kgsl_snapshot_gpu_object_v2) +
			sizeof(struct kgsl_snapshot_section_header));
	}

	if (size == 0)
		goto done;

	snapshot->mempool = vmalloc(size);

	ptr = snapshot->mempool;
	snapshot->mempool_size = 0;

	/* even if vmalloc fails, make sure we clean up the obj_list */
	list_for_each_entry_safe(obj, tmp, &snapshot->obj_list, node) {
		if (snapshot->mempool) {
			size_t ret = _mempool_add_object(snapshot, ptr, obj);

			ptr += ret;
			snapshot->mempool_size += ret;
		}

		kgsl_snapshot_put_object(obj);
	}
done:
	/*
	 * Get rid of the process struct here, so that it doesn't sit
	 * around until someone bothers to read the snapshot file.
	 */
	kgsl_process_private_put(snapshot->process);
	snapshot->process = NULL;

	if (snapshot->ib1base && !snapshot->ib1dumped)
		dev_err(snapshot->device->dev,
				"snapshot: Active IB1:%016llx not dumped\n",
				snapshot->ib1base);
	else if (snapshot->ib2base && !snapshot->ib2dumped)
		dev_err(snapshot->device->dev,
			       "snapshot: Active IB2:%016llx not dumped\n",
				snapshot->ib2base);

gmu_only:
	BUG_ON(!snapshot->device->skip_ib_capture &&
				snapshot->device->force_panic);
	complete_all(&snapshot->dump_gate);
}
