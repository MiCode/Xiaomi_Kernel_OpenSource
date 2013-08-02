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

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "kgsl_snapshot.h"

#include "adreno.h"
#include "adreno_pm4types.h"
#include "a2xx_reg.h"
#include "a3xx_reg.h"
#include "adreno_cp_parser.h"

/* Number of dwords of ringbuffer history to record */
#define NUM_DWORDS_OF_RINGBUFFER_HISTORY 100

/* Maintain a list of the objects we see during parsing */

#define SNAPSHOT_OBJ_BUFSIZE 64

#define SNAPSHOT_OBJ_TYPE_IB 0

/* Keep track of how many bytes are frozen after a snapshot and tell the user */
static int snapshot_frozen_objsize;

static struct kgsl_snapshot_obj {
	int type;
	uint32_t gpuaddr;
	phys_addr_t ptbase;
	void *ptr;
	int dwords;
} objbuf[SNAPSHOT_OBJ_BUFSIZE];

/* Pointer to the next open entry in the object list */
static int objbufptr;

/* Push a new buffer object onto the list */
static void push_object(struct kgsl_device *device, int type,
	phys_addr_t ptbase,
	uint32_t gpuaddr, int dwords)
{
	int index;
	void *ptr;

	/*
	 * Sometimes IBs can be reused in the same dump.  Because we parse from
	 * oldest to newest, if we come across an IB that has already been used,
	 * assume that it has been reused and update the list with the newest
	 * size.
	 */

	for (index = 0; index < objbufptr; index++) {
		if (objbuf[index].gpuaddr == gpuaddr &&
			objbuf[index].ptbase == ptbase) {
				objbuf[index].dwords = dwords;
				return;
			}
	}

	if (objbufptr == SNAPSHOT_OBJ_BUFSIZE) {
		KGSL_DRV_ERR(device, "snapshot: too many snapshot objects\n");
		return;
	}

	/*
	 * adreno_convertaddr verifies that the IB size is valid - at least in
	 * the context of it being smaller then the allocated memory space
	 */
	ptr = adreno_convertaddr(device, ptbase, gpuaddr, dwords << 2);

	if (ptr == NULL) {
		KGSL_DRV_ERR(device,
			"snapshot: Can't find GPU address for %x\n", gpuaddr);
		return;
	}

	/* Put it on the list of things to parse */
	objbuf[objbufptr].type = type;
	objbuf[objbufptr].gpuaddr = gpuaddr;
	objbuf[objbufptr].ptbase = ptbase;
	objbuf[objbufptr].dwords = dwords;
	objbuf[objbufptr++].ptr = ptr;
}

/*
 * Return a 1 if the specified object is already on the list of buffers
 * to be dumped
 */

static int find_object(int type, unsigned int gpuaddr, phys_addr_t ptbase)
{
	int index;

	for (index = 0; index < objbufptr; index++) {
		if (objbuf[index].gpuaddr == gpuaddr &&
			objbuf[index].ptbase == ptbase &&
			objbuf[index].type == type)
			return 1;
	}

	return 0;
}

/*
 * snapshot_freeze_obj_list() - Take a list of ib objects and freeze their
 * memory for snapshot
 * @device: Device being snapshotted
 * @ptbase: The pagetable base of the process to which IB belongs
 * @ib_obj_list: List of the IB objects
 *
 * Returns 0 on success else error code
 */
static int snapshot_freeze_obj_list(struct kgsl_device *device,
		phys_addr_t ptbase, struct adreno_ib_object_list *ib_obj_list)
{
	int ret = 0;
	struct adreno_ib_object *ib_objs;
	unsigned int ib2base;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i;

	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB2_BASE, &ib2base);

	for (i = 0; i < ib_obj_list->num_objs; i++) {
		int temp_ret;
		int index;
		int freeze = 1;

		ib_objs = &(ib_obj_list->obj_list[i]);
		/* Make sure this object is not going to be saved statically */
		for (index = 0; index < objbufptr; index++) {
			if ((objbuf[index].gpuaddr <= ib_objs->gpuaddr) &&
				((objbuf[index].gpuaddr +
				(objbuf[index].dwords << 2)) >=
				(ib_objs->gpuaddr + ib_objs->size)) &&
				(objbuf[index].ptbase == ptbase)) {
				freeze = 0;
				break;
			}
		}

		if (freeze) {
			/* Save current IB2 statically */
			if (ib2base == ib_objs->gpuaddr) {
				push_object(device, SNAPSHOT_OBJ_TYPE_IB,
				ptbase, ib_objs->gpuaddr, ib_objs->size);
			} else {
				temp_ret = kgsl_snapshot_get_object(device,
					ptbase, ib_objs->gpuaddr, ib_objs->size,
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
static inline int parse_ib(struct kgsl_device *device, phys_addr_t ptbase,
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
		push_object(device, SNAPSHOT_OBJ_TYPE_IB, ptbase,
			gpuaddr, dwords);
		goto done;
	}

	if (kgsl_snapshot_have_object(device, ptbase, gpuaddr, dwords << 2))
		goto done;

	ret = adreno_ib_create_object_list(device, ptbase,
				gpuaddr, dwords, &ib_obj_list);
	if (ret)
		goto done;

	ret = kgsl_snapshot_add_ib_obj_list(device, ptbase, ib_obj_list);

	if (ret)
		adreno_ib_destroy_obj_list(ib_obj_list);
done:
	return ret;
}

/* Snapshot the ringbuffer memory */
static int snapshot_rb(struct kgsl_device *device, void *snapshot,
	int remain, void *priv)
{
	struct kgsl_snapshot_rb *header = snapshot;
	unsigned int *data = snapshot + sizeof(*header);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;
	unsigned int rptr, *rbptr, ibbase;
	phys_addr_t ptbase;
	int index, size, i;
	int parse_ibs = 0, ib_parse_start;

	/* Get the physical address of the MMU pagetable */
	ptbase = kgsl_mmu_get_current_ptbase(&device->mmu);

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
			index = rb->sizedwords - 3;

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
			index = rb->sizedwords - 2;

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

	size = (rb->sizedwords << 2);

	if (remain < size + sizeof(*header)) {
		KGSL_DRV_ERR(device,
			"snapshot: Not enough memory for the rb section");
		return 0;
	}

	/* Write the sub-header for the section */
	header->start = rb->wptr;
	header->end = rb->wptr;
	header->wptr = rb->wptr;
	header->rbsize = rb->sizedwords;
	header->count = rb->sizedwords;

	/*
	 * Loop through the RB, copying the data and looking for indirect
	 * buffers and MMU pagetable changes
	 */

	index = rb->wptr;
	for (i = 0; i < rb->sizedwords; i++) {
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
			 * This will return non NULL if the IB happens to be
			 * part of the context memory (i.e - context switch
			 * command buffers)
			 */

			struct kgsl_memdesc *memdesc =
				adreno_find_ctxtmem(device, ptbase, ibaddr,
					ibsize << 2);

			/* IOMMU uses a NOP IB placed in setsate memory */
			if (NULL == memdesc)
				if (kgsl_gpuaddr_in_memdesc(
						&device->mmu.setstate_memory,
						ibaddr, ibsize << 2))
					memdesc = &device->mmu.setstate_memory;
			/*
			 * The IB from CP_IB1_BASE and the IBs for legacy
			 * context switch go into the snapshot all
			 * others get marked at GPU objects
			 */

			if (memdesc != NULL)
				push_object(device, SNAPSHOT_OBJ_TYPE_IB,
					ptbase, ibaddr, ibsize);
			else
				parse_ib(device, ptbase, ibaddr, ibsize);
		}

		index = index + 1;

		if (index == rb->sizedwords)
			index = 0;

		data++;
	}

	/* Return the size of the section */
	return size + sizeof(*header);
}

static int snapshot_capture_mem_list(struct kgsl_device *device, void *snapshot,
			int remain, void *priv)
{
	struct kgsl_snapshot_replay_mem_list *header = snapshot;
	struct kgsl_process_private *private = NULL;
	struct kgsl_process_private *tmp_private;
	phys_addr_t ptbase;
	struct rb_node *node;
	struct kgsl_mem_entry *entry = NULL;
	int num_mem;
	unsigned int *data = snapshot + sizeof(*header);

	ptbase = kgsl_mmu_get_current_ptbase(&device->mmu);
	mutex_lock(&kgsl_driver.process_mutex);
	list_for_each_entry(tmp_private, &kgsl_driver.process_list, list) {
		if (kgsl_mmu_pt_equal(&device->mmu, tmp_private->pagetable,
			ptbase)) {
			private = tmp_private;
			break;
		}
	}
	mutex_unlock(&kgsl_driver.process_mutex);
	if (!private) {
		KGSL_DRV_ERR(device,
		"Failed to get pointer to process private structure\n");
		return 0;
	}
	/* We need to know the number of memory objects that the process has */
	spin_lock(&private->mem_lock);
	for (node = rb_first(&private->mem_rb), num_mem = 0; node; ) {
		entry = rb_entry(node, struct kgsl_mem_entry, node);
		node = rb_next(&entry->node);
		num_mem++;
	}

	if (remain < ((num_mem * 3 * sizeof(unsigned int)) +
			sizeof(*header))) {
		KGSL_DRV_ERR(device,
			"snapshot: Not enough memory for the mem list section");
		spin_unlock(&private->mem_lock);
		return 0;
	}
	header->num_entries = num_mem;
	header->ptbase = (__u32)ptbase;
	/*
	 * Walk throught the memory list and store the
	 * tuples(gpuaddr, size, memtype) in snapshot
	 */
	for (node = rb_first(&private->mem_rb); node; ) {
		entry = rb_entry(node, struct kgsl_mem_entry, node);
		node = rb_next(&entry->node);

		*data++ = entry->memdesc.gpuaddr;
		*data++ = entry->memdesc.size;
		*data++ = (entry->memdesc.priv & KGSL_MEMTYPE_MASK) >>
							KGSL_MEMTYPE_SHIFT;
	}
	spin_unlock(&private->mem_lock);
	return sizeof(*header) + (num_mem * 3 * sizeof(unsigned int));
}

/* Snapshot the memory for an indirect buffer */
static int snapshot_ib(struct kgsl_device *device, void *snapshot,
	int remain, void *priv)
{
	struct kgsl_snapshot_ib *header = snapshot;
	struct kgsl_snapshot_obj *obj = priv;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int *src = obj->ptr;
	unsigned int *dst = snapshot + sizeof(*header);
	struct adreno_ib_object_list *ib_obj_list;
	unsigned int ib1base;

	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB1_BASE, &ib1base);

	if (remain < (obj->dwords << 2) + sizeof(*header)) {
		KGSL_DRV_ERR(device,
			"snapshot: Not enough memory for the ib section");
		return 0;
	}

	/* only do this for IB1 because the IB2's are part of IB1 objects */
	if (ib1base == obj->gpuaddr) {
		if (!adreno_ib_create_object_list(device, obj->ptbase,
					obj->gpuaddr, obj->dwords,
					&ib_obj_list)) {
			/* freeze the IB objects in the IB */
			snapshot_freeze_obj_list(device, obj->ptbase,
						ib_obj_list);
			adreno_ib_destroy_obj_list(ib_obj_list);
		}
	}

	/* Write the sub-header for the section */
	header->gpuaddr = obj->gpuaddr;
	header->ptbase = (__u32)obj->ptbase;
	header->size = obj->dwords;

	/* Write the contents of the ib */
	memcpy((void *)dst, (void *)src, obj->dwords << 2);
	/* Write the contents of the ib */

	return (obj->dwords << 2) + sizeof(*header);
}

/* Dump another item on the current pending list */
static void *dump_object(struct kgsl_device *device, int obj, void *snapshot,
	int *remain)
{
	switch (objbuf[obj].type) {
	case SNAPSHOT_OBJ_TYPE_IB:
		snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_IB, snapshot, remain,
			snapshot_ib, &objbuf[obj]);
		break;
	default:
		KGSL_DRV_ERR(device,
			"snapshot: Invalid snapshot object type: %d\n",
			objbuf[obj].type);
		break;
	}

	return snapshot;
}

/* adreno_snapshot - Snapshot the Adreno GPU state
 * @device - KGSL device to snapshot
 * @snapshot - Pointer to the start of memory to write into
 * @remain - A pointer to how many bytes of memory are remaining in the snapshot
 * @hang - set if this snapshot was automatically triggered by a GPU hang
 * This is a hook function called by kgsl_snapshot to snapshot the
 * Adreno specific information for the GPU snapshot.  In turn, this function
 * calls the GPU specific snapshot function to get core specific information.
 */

void *adreno_snapshot(struct kgsl_device *device, void *snapshot, int *remain,
		int hang)
{
	int i;
	uint32_t ibbase, ibsize;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	phys_addr_t ptbase;

	/* Reset the list of objects */
	objbufptr = 0;

	snapshot_frozen_objsize = 0;

	/* Get the physical address of the MMU pagetable */
	ptbase = kgsl_mmu_get_current_ptbase(&device->mmu);

	/* Dump the ringbuffer */
	snapshot = kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_RB,
		snapshot, remain, snapshot_rb, NULL);

	/*
	 * Add a section that lists (gpuaddr, size, memtype) tuples of the
	 * hanging process
	 */
	snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_MEMLIST, snapshot, remain,
			snapshot_capture_mem_list, NULL);
	/*
	 * Make sure that the last IB1 that was being executed is dumped.
	 * Since this was the last IB1 that was processed, we should have
	 * already added it to the list during the ringbuffer parse but we
	 * want to be double plus sure.
	 */

	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB1_BASE, &ibbase);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB1_BUFSZ, &ibsize);

	/*
	 * The problem is that IB size from the register is the unprocessed size
	 * of the buffer not the original size, so if we didn't catch this
	 * buffer being directly used in the RB, then we might not be able to
	 * dump the whle thing. Print a warning message so we can try to
	 * figure how often this really happens.
	 */

	if (!find_object(SNAPSHOT_OBJ_TYPE_IB, ibbase, ptbase) && ibsize) {
		push_object(device, SNAPSHOT_OBJ_TYPE_IB, ptbase,
			ibbase, ibsize);
		KGSL_DRV_ERR(device, "CP_IB1_BASE not found in the ringbuffer. "
			"Dumping %x dwords of the buffer.\n", ibsize);
	}

	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB2_BASE, &ibbase);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB2_BUFSZ, &ibsize);

	/*
	 * Add the last parsed IB2 to the list. The IB2 should be found as we
	 * parse the objects below, but we try to add it to the list first, so
	 * it too can be parsed.  Don't print an error message in this case - if
	 * the IB2 is found during parsing, the list will be updated with the
	 * correct size.
	 */

	if (!find_object(SNAPSHOT_OBJ_TYPE_IB, ibbase, ptbase) && ibsize) {
		push_object(device, SNAPSHOT_OBJ_TYPE_IB, ptbase,
			ibbase, ibsize);
	}

	/*
	 * Go through the list of found objects and dump each one.  As the IBs
	 * are parsed, more objects might be found, and objbufptr will increase
	 */
	for (i = 0; i < objbufptr; i++)
		snapshot = dump_object(device, i, snapshot, remain);

	/* Add GPU specific sections - registers mainly, but other stuff too */
	if (adreno_dev->gpudev->snapshot)
		snapshot = adreno_dev->gpudev->snapshot(adreno_dev, snapshot,
			remain, hang);

	if (snapshot_frozen_objsize)
		KGSL_DRV_ERR(device, "GPU snapshot froze %dKb of GPU buffers\n",
			snapshot_frozen_objsize / 1024);

	/*
	 * Queue a work item that will save the IB data in snapshot into
	 * static memory to prevent loss of data due to overwriting of
	 * memory
	 */
	queue_work(device->work_queue, &device->snapshot_obj_ws);

	return snapshot;
}
