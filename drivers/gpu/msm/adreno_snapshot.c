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

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "kgsl_snapshot.h"

#include "adreno.h"
#include "adreno_pm4types.h"
#include "a2xx_reg.h"

/* Number of dwords of ringbuffer history to record */
#define NUM_DWORDS_OF_RINGBUFFER_HISTORY 100

/* Maintain a list of the objects we see during parsing */

#define SNAPSHOT_OBJ_BUFSIZE 64

#define SNAPSHOT_OBJ_TYPE_IB 0

static struct kgsl_snapshot_obj {
	int type;
	uint32_t gpuaddr;
	uint32_t ptbase;
	void *ptr;
	int dwords;
} objbuf[SNAPSHOT_OBJ_BUFSIZE];

/* Pointer to the next open entry in the object list */
static int objbufptr;

/* Push a new buffer object onto the list */
static void push_object(struct kgsl_device *device, int type, uint32_t ptbase,
	uint32_t gpuaddr, int dwords)
{
	int index;
	void *ptr;

	/* Go through the list and see that object has already been seen */
	for (index = 0; index < objbufptr; index++) {
		if (objbuf[index].gpuaddr == gpuaddr &&
			objbuf[index].ptbase == ptbase)
			return;
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

/* Snapshot the istore memory */
static int snapshot_istore(struct kgsl_device *device, void *snapshot,
	int remain, void *priv)
{
	struct kgsl_snapshot_istore *header = snapshot;
	unsigned int *data = snapshot + sizeof(*header);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int count, i;

	count = adreno_dev->istore_size * ADRENO_ISTORE_WORDS;

	if (remain < (count * 4) + sizeof(*header)) {
		KGSL_DRV_ERR(device,
			"snapshot: Not enough memory for the istore section");
		return 0;
	}

	header->count = adreno_dev->istore_size;

	for (i = 0; i < count; i++)
		kgsl_regread(device, ADRENO_ISTORE_START + i, &data[i]);

	return (count * 4) + sizeof(*header);
}

/* Snapshot the ringbuffer memory */
static int snapshot_rb(struct kgsl_device *device, void *snapshot,
	int remain, void *priv)
{
	struct kgsl_snapshot_rb *header = snapshot;
	unsigned int *data = snapshot + sizeof(*header);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;
	unsigned int rbbase, ptbase, rptr, *rbptr;
	int start, stop, index;
	int numitems, size;

	/* Get the GPU address of the ringbuffer */
	kgsl_regread(device, REG_CP_RB_BASE, &rbbase);

	/* Get the physical address of the MMU pagetable */
	ptbase = kgsl_mmu_get_current_ptbase(device);

	/* Get the current read pointers for the RB */
	kgsl_regread(device, REG_CP_RB_RPTR, &rptr);

	/* start the dump at the rptr minus some history */
	start = (int) rptr - NUM_DWORDS_OF_RINGBUFFER_HISTORY;
	if (start < 0)
		start += rb->sizedwords;

	/*
	 * Stop the dump at the point where the software last wrote.  Don't use
	 * the hardware value here on the chance that it didn't get properly
	 * updated
	 */

	stop = (int) rb->wptr + 16;
	if (stop > rb->sizedwords)
		stop -= rb->sizedwords;

	/* Set up the header for the section */

	numitems = (stop > start) ? stop - start :
		(rb->sizedwords - start) + stop;

	size = (numitems << 2);

	if (remain < size + sizeof(*header)) {
		KGSL_DRV_ERR(device,
			"snapshot: Not enough memory for the rb section");
		return 0;
	}

	/* Write the sub-header for the section */
	header->start = start;
	header->end = stop;
	header->wptr = rb->wptr;
	header->rbsize = rb->sizedwords;
	header->count = numitems;

	index = start;
	rbptr = rb->buffer_desc.hostptr;

	/*
	 * Loop through the RB, copying the data and looking for indirect
	 * buffers and MMU pagetable changes
	 */

	while (index != rb->wptr) {
		*data = rbptr[index];

		if (rbptr[index] == cp_type3_packet(CP_INDIRECT_BUFFER_PFD, 2))
			push_object(device, SNAPSHOT_OBJ_TYPE_IB, ptbase,
				rbptr[index + 1], rbptr[index + 2]);

		/*
		 * FIXME: Handle upcoming MMU pagetable changes, but only
		 * between the rptr and the wptr
		 */

		index = index + 1;

		if (index == rb->sizedwords)
			index = 0;

		data++;
	}

	/* Dump 16 dwords past the wptr, but don't  bother interpeting it */

	while (index != stop) {
		*data = rbptr[index];
		index = index + 1;

		if (index == rb->sizedwords)
			index = 0;

		data++;
	}

	/* Return the size of the section */
	return size + sizeof(*header);
}

/* Snapshot the memory for an indirect buffer */
static int snapshot_ib(struct kgsl_device *device, void *snapshot,
	int remain, void *priv)
{
	struct kgsl_snapshot_ib *header = snapshot;
	struct kgsl_snapshot_obj *obj = priv;
	unsigned int *src = obj->ptr;
	unsigned int *dst = snapshot + sizeof(*header);
	int i;

	if (remain < (obj->dwords << 2) + sizeof(*header)) {
		KGSL_DRV_ERR(device,
			"snapshot: Not enough memory for the ib section");
		return 0;
	}

	/* Write the sub-header for the section */
	header->gpuaddr = obj->gpuaddr;
	header->ptbase = obj->ptbase;
	header->size = obj->dwords;

	/* Write the contents of the ib */
	for (i = 0; i < obj->dwords; i++) {
		*dst = *src;
		/* If another IB is discovered, then push it on the list too */

		if (*src == cp_type3_packet(CP_INDIRECT_BUFFER_PFD, 2)) {
			push_object(device, SNAPSHOT_OBJ_TYPE_IB, obj->ptbase,
				*(src + 1), *(src + 2));
		}

		src++;
		dst++;
	}

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
	uint32_t ptbase, ibbase, ibsize;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* Reset the list of objects */
	objbufptr = 0;

	/* Get the physical address of the MMU pagetable */
	ptbase = kgsl_mmu_get_current_ptbase(device);

	/* Dump the ringbuffer */
	snapshot = kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_RB,
		snapshot, remain, snapshot_rb, NULL);

	/*
	 * Make sure that the IBs described in the CP registers are on the
	 * list of objects
	 */
	kgsl_regread(device, REG_CP_IB1_BASE, &ibbase);
	kgsl_regread(device, REG_CP_IB1_BUFSZ, &ibsize);

	if (ibsize)
		push_object(device, SNAPSHOT_OBJ_TYPE_IB, ptbase,
			ibbase, ibsize);

	kgsl_regread(device, REG_CP_IB2_BASE, &ibbase);
	kgsl_regread(device, REG_CP_IB2_BUFSZ, &ibsize);

	if (ibsize)
		push_object(device, SNAPSHOT_OBJ_TYPE_IB, ptbase,
			ibbase, ibsize);

	/*
	 * Go through the list of found objects and dump each one.  As the IBs
	 * are parsed, more objects might be found, and objbufptr will increase
	 */
	for (i = 0; i < objbufptr; i++)
		snapshot = dump_object(device, i, snapshot, remain);

	/*
	 * Only dump the istore on a hang - reading it on a running system
	 * has a non 0 chance of hanging the GPU
	 */

	if (hang) {
		snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_ISTORE, snapshot, remain,
			snapshot_istore, NULL);
	}

	/* Add GPU specific sections - registers mainly, but other stuff too */
	if (adreno_dev->gpudev->snapshot)
		snapshot = adreno_dev->gpudev->snapshot(adreno_dev, snapshot,
			remain, hang);

	return snapshot;
}
