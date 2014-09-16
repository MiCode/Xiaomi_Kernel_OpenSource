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
 */

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "kgsl_snapshot.h"

#include "adreno.h"
#include "adreno_pm4types.h"
#include "a3xx_reg.h"
#include "adreno_cp_parser.h"

/* Number of dwords of ringbuffer history to record */
#define NUM_DWORDS_OF_RINGBUFFER_HISTORY 100

/* Maintain a list of the objects we see during parsing */

#define SNAPSHOT_OBJ_BUFSIZE 64

#define SNAPSHOT_OBJ_TYPE_IB 0

/* Keep track of how many bytes are frozen after a snapshot and tell the user */
static size_t snapshot_frozen_objsize;

static struct kgsl_snapshot_object objbuf[SNAPSHOT_OBJ_BUFSIZE];

/* Pointer to the next open entry in the object list */
static unsigned int objbufptr;

/* Push a new buffer object onto the list */
static void push_object(int type,
	struct kgsl_process_private *process,
	uint32_t gpuaddr, int dwords)
{
	int index;
	struct kgsl_mem_entry *entry;

	if (process == NULL)
		return;

	/*
	 * Sometimes IBs can be reused in the same dump.  Because we parse from
	 * oldest to newest, if we come across an IB that has already been used,
	 * assume that it has been reused and update the list with the newest
	 * size.
	 */

	for (index = 0; index < objbufptr; index++) {
		if (objbuf[index].gpuaddr == gpuaddr &&
			objbuf[index].entry->priv == process) {

			objbuf[index].size = max_t(unsigned int,
						objbuf[index].size,
						dwords << 2);
			return;
		}
	}

	if (objbufptr == SNAPSHOT_OBJ_BUFSIZE) {
		KGSL_CORE_ERR("snapshot: too many snapshot objects\n");
		return;
	}

	entry = kgsl_sharedmem_find_region(process, gpuaddr, dwords << 2);
	if (entry == NULL) {
		KGSL_CORE_ERR("snapshot: Can't find entry for %X\n", gpuaddr);
		return;
	}

	/* Put it on the list of things to parse */
	objbuf[objbufptr].type = type;
	objbuf[objbufptr].gpuaddr = gpuaddr;
	objbuf[objbufptr].size = dwords << 2;
	objbuf[objbufptr++].entry = entry;
}

/*
 * Return a 1 if the specified object is already on the list of buffers
 * to be dumped
 */

static int find_object(int type, unsigned int gpuaddr,
		struct kgsl_process_private *process)
{
	int index;

	for (index = 0; index < objbufptr; index++) {
		if (objbuf[index].gpuaddr == gpuaddr &&
			objbuf[index].entry->priv == process)
			return 1;
	}

	return 0;
}

/*
 * snapshot_freeze_obj_list() - Take a list of ib objects and freeze their
 * memory for snapshot
 * @snapshot: The snapshot data.
 * @process: The process to which the IB belongs
 * @ib_obj_list: List of the IB objects
 * @ib2base: IB2 base address at time of the fault
 *
 * Returns 0 on success else error code
 */
static int snapshot_freeze_obj_list(struct kgsl_snapshot *snapshot,
		struct kgsl_process_private *process,
		struct adreno_ib_object_list *ib_obj_list,
		unsigned int ib2base)
{
	int ret = 0;
	struct adreno_ib_object *ib_objs;
	int i;

	for (i = 0; i < ib_obj_list->num_objs; i++) {
		int temp_ret;
		int index;
		int freeze = 1;

		ib_objs = &(ib_obj_list->obj_list[i]);
		/* Make sure this object is not going to be saved statically */
		for (index = 0; index < objbufptr; index++) {
			if ((objbuf[index].gpuaddr <= ib_objs->gpuaddr) &&
				((objbuf[index].gpuaddr +
				(objbuf[index].size)) >=
				(ib_objs->gpuaddr + ib_objs->size)) &&
				(objbuf[index].entry->priv == process)) {
				freeze = 0;
				break;
			}
		}

		if (freeze) {
			/* Save current IB2 statically */
			if (ib2base == ib_objs->gpuaddr) {
				push_object(SNAPSHOT_OBJ_TYPE_IB,
				process, ib_objs->gpuaddr, ib_objs->size >> 2);
			} else {
				temp_ret = kgsl_snapshot_get_object(snapshot,
					process, ib_objs->gpuaddr,
					ib_objs->size,
					ib_objs->snapshot_obj_type);
				if (temp_ret < 0) {
					if (ret >= 0)
						ret = temp_ret;
				} else {
					snapshot_frozen_objsize += temp_ret;
				}
			}
		}
	}
	return ret;
}

/*
 * We want to store the last executed IB1 and IB2 in the static region to ensure
 * that we get at least some information out of the snapshot even if we can't
 * access the dynamic data from the sysfs file.  Push all other IBs on the
 * dynamic list
 */
static inline int parse_ib(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		struct kgsl_process_private *process,
		unsigned int gpuaddr, unsigned int dwords)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int ib1base;
	int ret = 0;
	struct adreno_ib_object_list *ib_obj_list;

	/*
	 * Check the IB address - if it is either the last executed IB1
	 * then push it into the static blob otherwise put it in the dynamic
	 * list
	 */

	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB1_BASE, &ib1base);

	if (gpuaddr == ib1base) {
		push_object(SNAPSHOT_OBJ_TYPE_IB, process,
			gpuaddr, dwords);
		goto done;
	}

	if (kgsl_snapshot_have_object(snapshot, process,
					gpuaddr, dwords << 2))
		goto done;

	ret = adreno_ib_create_object_list(device, process,
				gpuaddr, dwords, &ib_obj_list);
	if (ret)
		goto done;

	ret = kgsl_snapshot_add_ib_obj_list(snapshot, ib_obj_list);

	if (ret)
		adreno_ib_destroy_obj_list(ib_obj_list);
done:
	return ret;
}

/* Snapshot the ringbuffer memory */
static size_t snapshot_rb(struct kgsl_device *device, u8 *buf,
	size_t remain, void *priv)
{
	struct kgsl_snapshot_rb *header = (struct kgsl_snapshot_rb *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);
	unsigned int rptr, *rbptr, ibbase;
	int index, i;
	int parse_ibs = 0, ib_parse_start;
	struct kgsl_snapshot *snapshot = priv;

	/* Get the current read pointers for the RB */
	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_RPTR, &rptr);

	/* Address of the last processed IB */
	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB1_BASE, &ibbase);

	/*
	 * Figure out the window of ringbuffer data to dump.  First we need to
	 * find where the last processed IB ws submitted.  Start walking back
	 * from the rptr
	 */

	index = rptr;
	rbptr = rb->buffer_desc.hostptr;

	do {
		index--;

		if (index < 0) {
			index = KGSL_RB_DWORDS - 3;

			/* We wrapped without finding what we wanted */
			if (index < rb->wptr) {
				index = rb->wptr;
				break;
			}
		}

		if (adreno_cmd_is_ib(rbptr[index]) &&
			rbptr[index + 1] == ibbase)
			break;
	} while (index != rb->wptr);

	/*
	 * index points at the last submitted IB. We can only trust that the
	 * memory between the context switch and the hanging IB is valid, so
	 * the next step is to find the context switch before the submission
	 */

	while (index != rb->wptr) {
		index--;

		if (index < 0) {
			index = KGSL_RB_DWORDS - 2;

			/*
			 * Wrapped without finding the context switch. This is
			 * harmless - we should still have enough data to dump a
			 * valid state
			 */

			if (index < rb->wptr) {
				index = rb->wptr;
				break;
			}
		}

		/* Break if the current packet is a context switch identifier */
		if ((rbptr[index] == cp_nop_packet(1)) &&
			(rbptr[index + 1] == KGSL_CONTEXT_TO_MEM_IDENTIFIER))
			break;
	}

	/*
	 * Index represents the start of the window of interest.  We will try
	 * to dump all buffers between here and the rptr
	 */

	ib_parse_start = index;

	/*
	 * Dump the entire ringbuffer - the parser can choose how much of it to
	 * process
	 */

	if (remain < KGSL_RB_SIZE + sizeof(*header)) {
		KGSL_CORE_ERR("snapshot: Not enough memory for the rb section");
		return 0;
	}

	/* Write the sub-header for the section */
	header->start = rb->wptr;
	header->end = rb->wptr;
	header->wptr = rb->wptr;
	header->rbsize = KGSL_RB_DWORDS;
	header->count = KGSL_RB_DWORDS;
	adreno_rb_readtimestamp(device, rb, KGSL_TIMESTAMP_QUEUED,
					&header->timestamp_queued);
	adreno_rb_readtimestamp(device, rb, KGSL_TIMESTAMP_RETIRED,
					&header->timestamp_retired);

	/*
	 * Loop through the RB, copying the data and looking for indirect
	 * buffers and MMU pagetable changes
	 */

	index = rb->wptr;
	for (i = 0; i < KGSL_RB_DWORDS; i++) {
		*data = rbptr[index];

		/*
		 * Only parse IBs between the start and the rptr or the next
		 * context switch, whichever comes first
		 */

		if (parse_ibs == 0 && index == ib_parse_start)
			parse_ibs = 1;
		else if (index == rptr || adreno_rb_ctxtswitch(&rbptr[index]))
			parse_ibs = 0;

		if (parse_ibs && adreno_cmd_is_ib(rbptr[index])) {
			unsigned int ibaddr = rbptr[index + 1];
			unsigned int ibsize = rbptr[index + 2];

			/*
			 * Sometimes the kernel generates IBs in global
			 * memory. We dump the interesting global buffers,
			 * so there's no need to parse these IBs.
			 */
			if (!kgsl_search_global_pt_entries(ibaddr, ibsize))
				parse_ib(device, snapshot, snapshot->process,
					ibaddr, ibsize);
		}

		index = index + 1;

		if (index == KGSL_RB_DWORDS)
			index = 0;

		data++;
	}

	/* Return the size of the section */
	return KGSL_RB_SIZE + sizeof(*header);
}

static size_t snapshot_capture_mem_list(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_replay_mem_list *header =
		(struct kgsl_snapshot_replay_mem_list *)buf;
	struct rb_node *node;
	struct kgsl_mem_entry *entry = NULL;
	int num_mem;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	struct kgsl_process_private *process = priv;

	/* we need a process to search! */
	if (process == NULL)
		return 0;

	/* We need to know the number of memory objects that the process has */
	spin_lock(&process->mem_lock);
	for (node = rb_first(&process->mem_rb), num_mem = 0; node; ) {
		entry = rb_entry(node, struct kgsl_mem_entry, node);
		node = rb_next(&entry->node);
		num_mem++;
	}

	if (remain < ((num_mem * 3 * sizeof(unsigned int)) +
			sizeof(*header))) {
		KGSL_CORE_ERR("snapshot: Not enough memory for the mem list");
		spin_unlock(&process->mem_lock);
		return 0;
	}
	header->num_entries = num_mem;
	header->ptbase =
	 (__u32)kgsl_mmu_pagetable_get_ptbase(process->pagetable);
	/*
	 * Walk throught the memory list and store the
	 * tuples(gpuaddr, size, memtype) in snapshot
	 */
	for (node = rb_first(&process->mem_rb); node; ) {
		entry = rb_entry(node, struct kgsl_mem_entry, node);
		node = rb_next(&entry->node);

		*data++ = entry->memdesc.gpuaddr;
		*data++ = entry->memdesc.size;
		*data++ = (entry->memdesc.flags & KGSL_MEMTYPE_MASK) >>
							KGSL_MEMTYPE_SHIFT;
	}
	spin_unlock(&process->mem_lock);
	return sizeof(*header) + (num_mem * 3 * sizeof(unsigned int));
}

struct snapshot_ib_meta {
	struct kgsl_snapshot *snapshot;
	struct kgsl_snapshot_object *obj;
	unsigned int ib1base;
	unsigned int ib1size;
	unsigned int ib2base;
	unsigned int ib2size;
};

/* Snapshot the memory for an indirect buffer */
static size_t snapshot_ib(struct kgsl_device *device, u8 *buf,
	size_t remain, void *priv)
{
	struct kgsl_snapshot_ib *header = (struct kgsl_snapshot_ib *)buf;
	struct snapshot_ib_meta *meta = priv;
	unsigned int *src;
	unsigned int *dst = (unsigned int *)(buf + sizeof(*header));
	struct adreno_ib_object_list *ib_obj_list;
	struct kgsl_snapshot *snapshot;
	struct kgsl_snapshot_object *obj;

	if (meta == NULL || meta->snapshot == NULL || meta->obj == NULL) {
		KGSL_CORE_ERR("snapshot: bad metadata");
		return 0;
	}
	snapshot = meta->snapshot;
	obj = meta->obj;


	src = kgsl_gpuaddr_to_vaddr(&obj->entry->memdesc, obj->gpuaddr);
	if (src == NULL) {
		KGSL_CORE_ERR("snapshot: Unable to map object 0x%X\n",
			obj->gpuaddr);
		return 0;
	}

	if (remain < (obj->size + sizeof(*header))) {
		KGSL_CORE_ERR("snapshot: Not enough memory for the ib\n");
		return 0;
	}

	/* only do this for IB1 because the IB2's are part of IB1 objects */
	if (meta->ib1base == obj->gpuaddr) {
		if (!adreno_ib_create_object_list(device, obj->entry->priv,
					obj->gpuaddr, obj->size >> 2,
					&ib_obj_list)) {
			/* freeze the IB objects in the IB */
			snapshot_freeze_obj_list(snapshot,
						obj->entry->priv,
						ib_obj_list, meta->ib2base);
			adreno_ib_destroy_obj_list(ib_obj_list);
		}
	}

	/* Write the sub-header for the section */
	header->gpuaddr = obj->gpuaddr;
	/*
	 * This loses address bits, but we can't do better until the snapshot
	 * binary format is updated.
	 */
	header->ptbase =
	 (__u32)kgsl_mmu_pagetable_get_ptbase(obj->entry->priv->pagetable);
	header->size = obj->size >> 2;

	/* Write the contents of the ib */
	memcpy((void *)dst, (void *)src, obj->size);
	/* Write the contents of the ib */

	return obj->size + sizeof(*header);
}

/* Dump another item on the current pending list */
static void dump_object(struct kgsl_device *device, int obj,
		struct kgsl_snapshot *snapshot,
		unsigned int ib1base, unsigned int ib1size,
		unsigned int ib2base, unsigned int ib2size)
{
	struct snapshot_ib_meta meta;

	switch (objbuf[obj].type) {
	case SNAPSHOT_OBJ_TYPE_IB:
		meta.snapshot = snapshot;
		meta.obj = &objbuf[obj];
		meta.ib1base = ib1base;
		meta.ib1size = ib1size;
		meta.ib2base = ib2base;
		meta.ib2size = ib2size;

		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_IB,
			snapshot, snapshot_ib, &meta);
		if (objbuf[obj].entry) {
			kgsl_memdesc_unmap(&(objbuf[obj].entry->memdesc));
			kgsl_mem_entry_put(objbuf[obj].entry);
		}
		break;
	default:
		KGSL_CORE_ERR("snapshot: Invalid snapshot object type: %d\n",
			objbuf[obj].type);
		break;
	}
}

/* setup_fault process - Find kgsl_process_private struct that caused the fault
 *
 * Find the faulting process based what the dispatcher thinks happened and
 * what the hardware is using for the current pagetable. The process struct
 * will be used to look up GPU addresses that are encountered while parsing
 * the GPU state.
 */
static void setup_fault_process(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot,
				struct kgsl_process_private *process)
{
	phys_addr_t hw_ptbase, proc_ptbase;

	if (process != NULL && !kgsl_process_private_get(process))
		process = NULL;

	/* Get the physical address of the MMU pagetable */
	hw_ptbase = kgsl_mmu_get_current_ptbase(&device->mmu);

	/* if we have an input process, make sure the ptbases match */
	if (process) {
		proc_ptbase = kgsl_mmu_pagetable_get_ptbase(process->pagetable);
		/* agreement! No need to check further */
		if (hw_ptbase == proc_ptbase)
			goto done;

		kgsl_process_private_put(process);
		process = NULL;
		KGSL_CORE_ERR("snapshot: ptbase mismatch hw %pa sw %pa\n",
				&hw_ptbase, &proc_ptbase);
	}

	/* try to find the right pagetable by walking the process list */
	if (kgsl_mmu_is_perprocess(&device->mmu)) {
		struct kgsl_process_private *tmp_private;

		mutex_lock(&kgsl_driver.process_mutex);
		list_for_each_entry(tmp_private,
				&kgsl_driver.process_list, list) {
			if (kgsl_mmu_pt_equal(&device->mmu,
						tmp_private->pagetable,
						hw_ptbase)
				&& kgsl_process_private_get(tmp_private)) {
					process = tmp_private;
				break;
			}
		}
		mutex_unlock(&kgsl_driver.process_mutex);
	}
done:
	snapshot->process = process;
}

/* Snapshot a global memory buffer */
static size_t snapshot_global(struct kgsl_device *device, u8 *buf,
	size_t remain, void *priv)
{
	struct kgsl_memdesc *memdesc = priv;

	struct kgsl_snapshot_gpu_object *header =
		(struct kgsl_snapshot_gpu_object *)buf;

	u8 *ptr = buf + sizeof(*header);

	if (memdesc->size == 0)
		return 0;

	if (remain < (memdesc->size + sizeof(*header))) {
		KGSL_CORE_ERR("snapshot: Not enough memory for the memdesc\n");
		return 0;
	}

	if (memdesc->hostptr == NULL) {
		KGSL_CORE_ERR("snapshot: no kernel mapping for global %x\n",
				memdesc->gpuaddr);
		return 0;
	}

	header->size = memdesc->size >> 2;
	header->gpuaddr = memdesc->gpuaddr;
	header->ptbase =
	 (__u32)kgsl_mmu_pagetable_get_ptbase(device->mmu.defaultpagetable);
	header->type = SNAPSHOT_GPU_OBJECT_GLOBAL;

	memcpy(ptr, memdesc->hostptr, memdesc->size);

	return memdesc->size + sizeof(*header);
}

/* adreno_snapshot - Snapshot the Adreno GPU state
 * @device - KGSL device to snapshot
 * @snapshot - Pointer to the snapshot instance
 * @context - context that caused the fault, if known by the driver
 * This is a hook function called by kgsl_snapshot to snapshot the
 * Adreno specific information for the GPU snapshot.  In turn, this function
 * calls the GPU specific snapshot function to get core specific information.
 */
void adreno_snapshot(struct kgsl_device *device, struct kgsl_snapshot *snapshot,
			struct kgsl_context *context)
{
	unsigned int i;
	uint32_t ib1base, ib1size;
	uint32_t ib2base, ib2size;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	/* Reset the list of objects */
	objbufptr = 0;

	snapshot_frozen_objsize = 0;

	setup_fault_process(device, snapshot,
			context ? context->proc_priv : NULL);

	/* Dump the ringbuffer */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_RB, snapshot,
			snapshot_rb, snapshot);

	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB1_BASE, &ib1base);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB1_BUFSZ, &ib1size);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB2_BASE, &ib2base);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB2_BUFSZ, &ib2size);
	/* Add GPU specific sections - registers mainly, but other stuff too */
	if (gpudev->snapshot)
		gpudev->snapshot(adreno_dev, snapshot);

	/* Dump selected global buffers */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GPU_OBJECT,
			snapshot, snapshot_global, &adreno_dev->dev.memstore);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GPU_OBJECT,
			snapshot, snapshot_global,
			&adreno_dev->dev.mmu.setstate_memory);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GPU_OBJECT,
			snapshot, snapshot_global,
			&adreno_dev->pwron_fixup);

	/*
	 * Add a section that lists (gpuaddr, size, memtype) tuples of the
	 * hanging process
	 */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_MEMLIST,
			snapshot, snapshot_capture_mem_list, snapshot->process);
	/*
	 * Make sure that the last IB1 that was being executed is dumped.
	 * Since this was the last IB1 that was processed, we should have
	 * already added it to the list during the ringbuffer parse but we
	 * want to be double plus sure.
	 * The problem is that IB size from the register is the unprocessed size
	 * of the buffer not the original size, so if we didn't catch this
	 * buffer being directly used in the RB, then we might not be able to
	 * dump the whole thing. Print a warning message so we can try to
	 * figure how often this really happens.
	 */

	if (!find_object(SNAPSHOT_OBJ_TYPE_IB, ib1base, snapshot->process)
			&& ib1size) {
		push_object(SNAPSHOT_OBJ_TYPE_IB, snapshot->process,
			ib1base, ib1size);
		KGSL_CORE_ERR(
			"CP_IB1_BASE not found. Dumping %x dwords of the buffer.\n",
			ib1size);
	}

	/*
	 * Add the last parsed IB2 to the list. The IB2 should be found as we
	 * parse the objects below, but we try to add it to the list first, so
	 * it too can be parsed.  Don't print an error message in this case - if
	 * the IB2 is found during parsing, the list will be updated with the
	 * correct size.
	 */

	if (!find_object(SNAPSHOT_OBJ_TYPE_IB, ib2base, snapshot->process)
			&& ib2size) {
		push_object(SNAPSHOT_OBJ_TYPE_IB, snapshot->process,
			ib2base, ib2size);
	}

	/*
	 * Go through the list of found objects and dump each one.  As the IBs
	 * are parsed, more objects might be found, and objbufptr will increase
	 */
	for (i = 0; i < objbufptr; i++)
		dump_object(device, i, snapshot, ib1base, ib1size,
			ib2base, ib2size);

	if (snapshot_frozen_objsize)
		KGSL_CORE_ERR("GPU snapshot froze %zdKb of GPU buffers\n",
			snapshot_frozen_objsize / 1024);

}
