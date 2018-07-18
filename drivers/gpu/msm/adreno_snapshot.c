/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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
#include "adreno_snapshot.h"
#include "adreno_a5xx.h"

#define VPC_MEMORY_BANKS 4

/* Maintain a list of the objects we see during parsing */

#define SNAPSHOT_OBJ_BUFSIZE 64

/* Used to print error message if an IB has too many objects in it */
static int ib_max_objs;

struct snapshot_rb_params {
	struct kgsl_snapshot *snapshot;
	struct adreno_ringbuffer *rb;
};

/* Keep track of how many bytes are frozen after a snapshot and tell the user */
static size_t snapshot_frozen_objsize;

static struct kgsl_snapshot_object objbuf[SNAPSHOT_OBJ_BUFSIZE];

/* Pointer to the next open entry in the object list */
static unsigned int objbufptr;

static inline int adreno_rb_ctxtswitch(struct adreno_device *adreno_dev,
				   unsigned int *cmd)
{
	return cmd[0] == cp_packet(adreno_dev, CP_NOP, 1) &&
		cmd[1] == KGSL_CONTEXT_TO_MEM_IDENTIFIER;
}

/* Push a new buffer object onto the list */
void kgsl_snapshot_push_object(struct kgsl_process_private *process,
	uint64_t gpuaddr, uint64_t dwords)
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

			objbuf[index].size = max_t(uint64_t,
						objbuf[index].size,
						dwords << 2);
			return;
		}
	}

	if (objbufptr == SNAPSHOT_OBJ_BUFSIZE) {
		KGSL_CORE_ERR("snapshot: too many snapshot objects\n");
		return;
	}

	entry = kgsl_sharedmem_find(process, gpuaddr);
	if (entry == NULL) {
		KGSL_CORE_ERR("snapshot: Can't find entry for 0x%016llX\n",
			gpuaddr);
		return;
	}

	if (!kgsl_gpuaddr_in_memdesc(&entry->memdesc, gpuaddr, dwords << 2)) {
		KGSL_CORE_ERR("snapshot: Mem entry 0x%016llX is too small\n",
			gpuaddr);
		kgsl_mem_entry_put(entry);
		return;
	}

	/* Put it on the list of things to parse */
	objbuf[objbufptr].gpuaddr = gpuaddr;
	objbuf[objbufptr].size = dwords << 2;
	objbuf[objbufptr++].entry = entry;
}

/*
 * Returns index of the specified object is already on the list of buffers
 * to be dumped
 */

static int find_object(uint64_t gpuaddr, struct kgsl_process_private *process)
{
	int index;

	for (index = 0; index < objbufptr; index++) {
		if (objbuf[index].gpuaddr == gpuaddr &&
			objbuf[index].entry->priv == process)
			return index;
	}
	return -ENOENT;
}

/*
 * snapshot_freeze_obj_list() - Take a list of ib objects and freeze their
 * memory for snapshot
 * @snapshot: The snapshot data.
 * @process: The process to which the IB belongs
 * @ib_obj_list: List of the IB objects
 *
 * Returns 0 on success else error code
 */
static int snapshot_freeze_obj_list(struct kgsl_snapshot *snapshot,
		struct kgsl_process_private *process,
		struct adreno_ib_object_list *ib_obj_list)
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
	return ret;
}

/*
 * We want to store the last executed IB1 and IB2 in the static region to ensure
 * that we get at least some information out of the snapshot even if we can't
 * access the dynamic data from the sysfs file.  Push all other IBs on the
 * dynamic list
 */
static inline void parse_ib(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		struct kgsl_process_private *process,
		uint64_t gpuaddr, uint64_t dwords)
{
	struct adreno_ib_object_list *ib_obj_list;

	/*
	 * Check the IB address - if it is either the last executed IB1
	 * then push it into the static blob otherwise put it in the dynamic
	 * list
	 */
	if (gpuaddr == snapshot->ib1base) {
		kgsl_snapshot_push_object(process, gpuaddr, dwords);
		return;
	}

	if (kgsl_snapshot_have_object(snapshot, process,
					gpuaddr, dwords << 2))
		return;

	if (-E2BIG == adreno_ib_create_object_list(device, process,
				gpuaddr, dwords, snapshot->ib2base,
				&ib_obj_list))
		ib_max_objs = 1;

	if (ib_obj_list)
		kgsl_snapshot_add_ib_obj_list(snapshot, ib_obj_list);

}

static inline bool iommu_is_setstate_addr(struct kgsl_device *device,
		uint64_t gpuaddr, uint64_t size)
{
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);

	if (kgsl_mmu_get_mmutype(device) != KGSL_MMU_TYPE_IOMMU)
		return false;

	return kgsl_gpuaddr_in_memdesc(&iommu->setstate, gpuaddr,
			size);
}

static void dump_all_ibs(struct kgsl_device *device,
			struct adreno_ringbuffer *rb,
			struct kgsl_snapshot *snapshot)
{
	int index = 0;
	unsigned int *rbptr;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	rbptr = rb->buffer_desc.hostptr;

	for (index = 0; index < KGSL_RB_DWORDS;) {

		if (adreno_cmd_is_ib(adreno_dev, rbptr[index])) {
			uint64_t ibaddr;
			uint64_t ibsize;

			if (ADRENO_LEGACY_PM4(adreno_dev)) {
				ibaddr = rbptr[index + 1];
				ibsize = rbptr[index + 2];
				index += 3;
			} else {
				ibaddr = rbptr[index + 2];
				ibaddr = ibaddr << 32 | rbptr[index + 1];
				ibsize = rbptr[index + 3];
				index += 4;
			}

			/* Don't parse known global IBs */
			if (iommu_is_setstate_addr(device, ibaddr, ibsize))
				continue;

			if (kgsl_gpuaddr_in_memdesc(&adreno_dev->pwron_fixup,
				ibaddr, ibsize))
				continue;

			parse_ib(device, snapshot, snapshot->process, ibaddr,
				ibsize);
		} else
			index = index + 1;
	}
}

/**
 * snapshot_rb_ibs() - Dump rb data and capture the IB's in the RB as well
 * @device: Pointer to a KGSL device
 * @rb: The RB to dump
 * @data: Pointer to memory where the RB data is to be dumped
 * @snapshot: Pointer to information about the current snapshot being taken
 */
static void snapshot_rb_ibs(struct kgsl_device *device,
		struct adreno_ringbuffer *rb,
		struct kgsl_snapshot *snapshot)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int rptr, *rbptr;
	int index, i;
	int parse_ibs = 0, ib_parse_start;

	/* Get the current read pointers for the RB */
	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_RPTR, &rptr);

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
			if (ADRENO_LEGACY_PM4(adreno_dev))
				index = KGSL_RB_DWORDS - 3;
			else
				index = KGSL_RB_DWORDS - 4;

			/* We wrapped without finding what we wanted */
			if (index < rb->wptr) {
				index = rb->wptr;
				break;
			}
		}

		if (adreno_cmd_is_ib(adreno_dev, rbptr[index])) {
			if (ADRENO_LEGACY_PM4(adreno_dev)) {
				if (rbptr[index + 1] == snapshot->ib1base)
					break;
			} else {
				uint64_t ibaddr;

				ibaddr = rbptr[index + 2];
				ibaddr = ibaddr << 32 | rbptr[index + 1];
				if (ibaddr == snapshot->ib1base)
					break;
			}
		}
	} while (index != rb->wptr);

	/*
	 * If the ib1 was not found, for example, if ib1base was restored
	 * incorrectly after preemption, then simply dump the entire
	 * ringbuffer along with all the IBs in the ringbuffer.
	 */

	if (index == rb->wptr) {
		dump_all_ibs(device, rb, snapshot);
		return;
	}

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
		if ((rbptr[index] == cp_packet(adreno_dev, CP_NOP, 1)) &&
			(rbptr[index + 1] == KGSL_CONTEXT_TO_MEM_IDENTIFIER))
			break;
	}

	/*
	 * Index represents the start of the window of interest.  We will try
	 * to dump all buffers between here and the rptr
	 */

	ib_parse_start = index;

	/*
	 * Loop through the RB, looking for indirect buffers and MMU pagetable
	 * changes
	 */

	index = rb->wptr;
	for (i = 0; i < KGSL_RB_DWORDS; i++) {
		/*
		 * Only parse IBs between the start and the rptr or the next
		 * context switch, whichever comes first
		 */

		if (parse_ibs == 0 && index == ib_parse_start)
			parse_ibs = 1;
		else if (index == rptr || adreno_rb_ctxtswitch(adreno_dev,
							&rbptr[index]))
			parse_ibs = 0;

		if (parse_ibs && adreno_cmd_is_ib(adreno_dev, rbptr[index])) {
			uint64_t ibaddr;
			uint64_t ibsize;

			if (ADRENO_LEGACY_PM4(adreno_dev)) {
				ibaddr = rbptr[index + 1];
				ibsize = rbptr[index + 2];
			} else {
				ibaddr = rbptr[index + 2];
				ibaddr = ibaddr << 32 | rbptr[index + 1];
				ibsize = rbptr[index + 3];
			}

			/* Don't parse known global IBs */
			if (iommu_is_setstate_addr(device, ibaddr, ibsize))
				continue;

			if (kgsl_gpuaddr_in_memdesc(&adreno_dev->pwron_fixup,
				ibaddr, ibsize))
				continue;

			parse_ib(device, snapshot, snapshot->process,
				ibaddr, ibsize);
		}

		index = (index + 1) % KGSL_RB_DWORDS;
	}

}

/* Snapshot the ringbuffer memory */
static size_t snapshot_rb(struct kgsl_device *device, u8 *buf,
	size_t remain, void *priv)
{
	struct kgsl_snapshot_rb_v2 *header = (struct kgsl_snapshot_rb_v2 *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct snapshot_rb_params *snap_rb_params = priv;
	struct kgsl_snapshot *snapshot = snap_rb_params->snapshot;
	struct adreno_ringbuffer *rb = snap_rb_params->rb;

	/*
	 * Dump the entire ringbuffer - the parser can choose how much of it to
	 * process
	 */

	if (remain < KGSL_RB_SIZE + sizeof(*header)) {
		KGSL_CORE_ERR("snapshot: Not enough memory for the rb section");
		return 0;
	}

	/* Write the sub-header for the section */
	header->start = 0;
	header->end = KGSL_RB_DWORDS;
	header->wptr = rb->wptr;
	header->rptr = adreno_get_rptr(rb);
	header->rbsize = KGSL_RB_DWORDS;
	header->count = KGSL_RB_DWORDS;
	adreno_rb_readtimestamp(adreno_dev, rb, KGSL_TIMESTAMP_QUEUED,
					&header->timestamp_queued);
	adreno_rb_readtimestamp(adreno_dev, rb, KGSL_TIMESTAMP_RETIRED,
					&header->timestamp_retired);
	header->gpuaddr = rb->buffer_desc.gpuaddr;
	header->id = rb->id;

	if (rb == adreno_dev->cur_rb)
		snapshot_rb_ibs(device, rb, snapshot);

	/* Just copy the ringbuffer, there are no active IBs */
	memcpy(data, rb->buffer_desc.hostptr, KGSL_RB_SIZE);

	/* Return the size of the section */
	return KGSL_RB_SIZE + sizeof(*header);
}

static int _count_mem_entries(int id, void *ptr, void *data)
{
	int *count = data;
	*count = *count + 1;
	return 0;
}

struct mem_entry {
	uint64_t gpuaddr;
	uint64_t size;
	unsigned int type;
} __packed;

static int _save_mem_entries(int id, void *ptr, void *data)
{
	struct kgsl_mem_entry *entry = ptr;
	struct mem_entry *m = (struct mem_entry *) data;
	unsigned int index = id - 1;

	m[index].gpuaddr = entry->memdesc.gpuaddr;
	m[index].size = entry->memdesc.size;
	m[index].type = kgsl_memdesc_get_memtype(&entry->memdesc);

	return 0;
}

static size_t snapshot_capture_mem_list(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_mem_list_v2 *header =
		(struct kgsl_snapshot_mem_list_v2 *)buf;
	int num_mem = 0;
	int ret = 0;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	struct kgsl_process_private *process = priv;

	/* we need a process to search! */
	if (process == NULL)
		return 0;

	spin_lock(&process->mem_lock);

	/* We need to know the number of memory objects that the process has */
	idr_for_each(&process->mem_idr, _count_mem_entries, &num_mem);

	if (num_mem == 0)
		goto out;

	if (remain < ((num_mem * sizeof(struct mem_entry)) + sizeof(*header))) {
		KGSL_CORE_ERR("snapshot: Not enough memory for the mem list");
		goto out;
	}

	header->num_entries = num_mem;
	header->ptbase = kgsl_mmu_pagetable_get_ttbr0(process->pagetable);

	/*
	 * Walk through the memory list and store the
	 * tuples(gpuaddr, size, memtype) in snapshot
	 */
	idr_for_each(&process->mem_idr, _save_mem_entries, data);

	ret = sizeof(*header) + (num_mem * sizeof(struct mem_entry));
out:
	spin_unlock(&process->mem_lock);
	return ret;
}

struct snapshot_ib_meta {
	struct kgsl_snapshot *snapshot;
	struct kgsl_snapshot_object *obj;
	uint64_t ib1base;
	uint64_t ib1size;
	uint64_t ib2base;
	uint64_t ib2size;
};

void kgsl_snapshot_add_active_ib_obj_list(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot)
{
	struct adreno_ib_object_list *ib_obj_list;
	int index = -ENOENT;

	if (!snapshot->ib1dumped)
		index = find_object(snapshot->ib1base, snapshot->process);

	/* only do this for IB1 because the IB2's are part of IB1 objects */
	if ((index != -ENOENT) &&
			(snapshot->ib1base == objbuf[index].gpuaddr)) {
		if (-E2BIG == adreno_ib_create_object_list(device,
					objbuf[index].entry->priv,
					objbuf[index].gpuaddr,
					objbuf[index].size >> 2,
					snapshot->ib2base,
					&ib_obj_list))
			ib_max_objs = 1;
		if (ib_obj_list) {
			/* freeze the IB objects in the IB */
			snapshot_freeze_obj_list(snapshot,
					objbuf[index].entry->priv,
					ib_obj_list);
			adreno_ib_destroy_obj_list(ib_obj_list);
		}
	} else {
		/* Get the IB2 index from parsed object */
		index = find_object(snapshot->ib2base, snapshot->process);

		if (index != -ENOENT)
			parse_ib(device, snapshot, snapshot->process,
				snapshot->ib2base, objbuf[index].size >> 2);
	}
}

/*
 * active_ib_is_parsed() - Checks if active ib is already parsed
 * @gpuaddr: Active IB base address at the time of fault
 * @size: Active IB size
 * @process: The process to which the IB belongs
 *
 * Function returns true if the active is already is parsed
 * else false
 */
static bool active_ib_is_parsed(uint64_t gpuaddr, uint64_t size,
		struct kgsl_process_private *process)
{
	int  index;
	/* go through the static list for gpuaddr is in list or not */
	for (index = 0; index < objbufptr; index++) {
		if ((objbuf[index].gpuaddr <= gpuaddr) &&
				((objbuf[index].gpuaddr +
				  (objbuf[index].size)) >=
				 (gpuaddr + size)) &&
				(objbuf[index].entry->priv == process))
			return true;
	}
	return false;
}
/* Snapshot the memory for an indirect buffer */
static size_t snapshot_ib(struct kgsl_device *device, u8 *buf,
	size_t remain, void *priv)
{
	struct kgsl_snapshot_ib_v2 *header = (struct kgsl_snapshot_ib_v2 *)buf;
	struct snapshot_ib_meta *meta = priv;
	unsigned int *src;
	unsigned int *dst = (unsigned int *)(buf + sizeof(*header));
	struct adreno_ib_object_list *ib_obj_list;
	struct kgsl_snapshot *snapshot;
	struct kgsl_snapshot_object *obj;
	struct kgsl_memdesc *memdesc;

	if (meta == NULL || meta->snapshot == NULL || meta->obj == NULL) {
		KGSL_CORE_ERR("snapshot: bad metadata");
		return 0;
	}
	snapshot = meta->snapshot;
	obj = meta->obj;
	memdesc = &obj->entry->memdesc;

	/* If size is zero get it from the medesc size */
	if (!obj->size)
		obj->size = (memdesc->size - (obj->gpuaddr - memdesc->gpuaddr));

	if (remain < (obj->size + sizeof(*header))) {
		KGSL_CORE_ERR("snapshot: Not enough memory for the ib\n");
		return 0;
	}

	src = kgsl_gpuaddr_to_vaddr(memdesc, obj->gpuaddr);
	if (src == NULL) {
		KGSL_DRV_ERR(device,
			"snapshot: Unable to map GPU memory object 0x%016llX into the kernel\n",
			obj->gpuaddr);
		return 0;
	}

	/* only do this for IB1 because the IB2's are part of IB1 objects */
	if (meta->ib1base == obj->gpuaddr) {

		snapshot->ib1dumped = active_ib_is_parsed(obj->gpuaddr,
					obj->size, obj->entry->priv);
		if (-E2BIG == adreno_ib_create_object_list(device,
				obj->entry->priv,
				obj->gpuaddr, obj->size >> 2,
				snapshot->ib2base,
				&ib_obj_list))
			ib_max_objs = 1;
		if (ib_obj_list) {
			/* freeze the IB objects in the IB */
			snapshot_freeze_obj_list(snapshot,
						obj->entry->priv,
						ib_obj_list);
			adreno_ib_destroy_obj_list(ib_obj_list);
		}
	}


	if (meta->ib2base == obj->gpuaddr)
		snapshot->ib2dumped = active_ib_is_parsed(obj->gpuaddr,
					obj->size, obj->entry->priv);

	/* Write the sub-header for the section */
	header->gpuaddr = obj->gpuaddr;
	header->ptbase =
		kgsl_mmu_pagetable_get_ttbr0(obj->entry->priv->pagetable);
	header->size = obj->size >> 2;

	/* Write the contents of the ib */
	memcpy((void *)dst, (void *)src, (size_t) obj->size);
	/* Write the contents of the ib */

	return obj->size + sizeof(*header);
}

/* Dump another item on the current pending list */
static void dump_object(struct kgsl_device *device, int obj,
		struct kgsl_snapshot *snapshot)
{
	struct snapshot_ib_meta meta;

	meta.snapshot = snapshot;
	meta.obj = &objbuf[obj];
	meta.ib1base = snapshot->ib1base;
	meta.ib1size = snapshot->ib1size;
	meta.ib2base = snapshot->ib2base;
	meta.ib2size = snapshot->ib2size;

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_IB_V2,
			snapshot, snapshot_ib, &meta);
	if (objbuf[obj].entry) {
		kgsl_memdesc_unmap(&(objbuf[obj].entry->memdesc));
		kgsl_mem_entry_put(objbuf[obj].entry);
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
	u64 hw_ptbase, proc_ptbase;

	if (process != NULL && !kgsl_process_private_get(process))
		process = NULL;

	/* Get the physical address of the MMU pagetable */
	hw_ptbase = kgsl_mmu_get_current_ttbr0(&device->mmu);

	/* if we have an input process, make sure the ptbases match */
	if (process) {
		proc_ptbase = kgsl_mmu_pagetable_get_ttbr0(process->pagetable);
		/* agreement! No need to check further */
		if (hw_ptbase == proc_ptbase)
			goto done;

		kgsl_process_private_put(process);
		process = NULL;
		KGSL_CORE_ERR("snapshot: ptbase mismatch hw %llx sw %llx\n",
				hw_ptbase, proc_ptbase);
	}

	/* try to find the right pagetable by walking the process list */
	if (kgsl_mmu_is_perprocess(&device->mmu)) {
		struct kgsl_process_private *tmp;

		mutex_lock(&kgsl_driver.process_mutex);
		list_for_each_entry(tmp, &kgsl_driver.process_list, list) {
			u64 pt_ttbr0;

			pt_ttbr0 = kgsl_mmu_pagetable_get_ttbr0(tmp->pagetable);
			if ((pt_ttbr0 == hw_ptbase)
			    && kgsl_process_private_get(tmp)) {
				process = tmp;
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

	struct kgsl_snapshot_gpu_object_v2 *header =
		(struct kgsl_snapshot_gpu_object_v2 *)buf;

	u8 *ptr = buf + sizeof(*header);

	if (memdesc->size == 0)
		return 0;

	if (remain < (memdesc->size + sizeof(*header))) {
		KGSL_CORE_ERR("snapshot: Not enough memory for the memdesc\n");
		return 0;
	}

	if (memdesc->hostptr == NULL) {
		KGSL_CORE_ERR(
		"snapshot: no kernel mapping for global object 0x%016llX\n",
		memdesc->gpuaddr);
		return 0;
	}

	header->size = memdesc->size >> 2;
	header->gpuaddr = memdesc->gpuaddr;
	header->ptbase = MMU_DEFAULT_TTBR0(device);
	header->type = SNAPSHOT_GPU_OBJECT_GLOBAL;

	memcpy(ptr, memdesc->hostptr, memdesc->size);

	return memdesc->size + sizeof(*header);
}

/* Snapshot IOMMU specific buffers */
static void adreno_snapshot_iommu(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
		snapshot, snapshot_global, &iommu->setstate);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION))
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
			snapshot, snapshot_global, &iommu->smmu_info);
}

static void adreno_snapshot_ringbuffer(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot, struct adreno_ringbuffer *rb)
{
	struct snapshot_rb_params params = {
		.snapshot = snapshot,
		.rb = rb,
	};

	if (rb == NULL)
		return;

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_RB_V2, snapshot,
		snapshot_rb, &params);
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
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	ib_max_objs = 0;
	/* Reset the list of objects */
	objbufptr = 0;

	snapshot_frozen_objsize = 0;

	setup_fault_process(device, snapshot,
			context ? context->proc_priv : NULL);

	/* Add GPU specific sections - registers mainly, but other stuff too */
	if (gpudev->snapshot)
		gpudev->snapshot(adreno_dev, snapshot);

	/* Dumping these buffers is useless if the GX is not on */
	if (gpudev->gx_is_on)
		if (!gpudev->gx_is_on(adreno_dev))
			return;

	adreno_readreg64(adreno_dev, ADRENO_REG_CP_IB1_BASE,
			ADRENO_REG_CP_IB1_BASE_HI, &snapshot->ib1base);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB1_BUFSZ, &snapshot->ib1size);
	adreno_readreg64(adreno_dev, ADRENO_REG_CP_IB2_BASE,
			ADRENO_REG_CP_IB2_BASE_HI, &snapshot->ib2base);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB2_BUFSZ, &snapshot->ib2size);

	snapshot->ib1dumped = false;
	snapshot->ib2dumped = false;

	adreno_snapshot_ringbuffer(device, snapshot, adreno_dev->cur_rb);

	/* Dump the prev ringbuffer */
	if (adreno_dev->prev_rb != adreno_dev->cur_rb)
		adreno_snapshot_ringbuffer(device, snapshot,
			adreno_dev->prev_rb);

	if ((adreno_dev->next_rb != adreno_dev->prev_rb) &&
		 (adreno_dev->next_rb != adreno_dev->cur_rb))
		adreno_snapshot_ringbuffer(device, snapshot,
			adreno_dev->next_rb);

	/* Dump selected global buffers */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
			snapshot, snapshot_global, &device->memstore);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
			snapshot, snapshot_global,
			&adreno_dev->pwron_fixup);

	if (kgsl_mmu_get_mmutype(device) == KGSL_MMU_TYPE_IOMMU)
		adreno_snapshot_iommu(device, snapshot);

	/*
	 * Add a section that lists (gpuaddr, size, memtype) tuples of the
	 * hanging process
	 */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_MEMLIST_V2,
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

	if (-ENOENT == find_object(snapshot->ib1base, snapshot->process) &&
			snapshot->ib1size) {
		kgsl_snapshot_push_object(snapshot->process, snapshot->ib1base,
				snapshot->ib1size);
		KGSL_CORE_ERR(
		"CP_IB1_BASE not found in the ringbuffer.Dumping %x dwords of the buffer.\n",
		snapshot->ib1size);
	}

	/*
	 * Add the last parsed IB2 to the list. The IB2 should be found as we
	 * parse the objects below, but we try to add it to the list first, so
	 * it too can be parsed.  Don't print an error message in this case - if
	 * the IB2 is found during parsing, the list will be updated with the
	 * correct size.
	 */

	if (-ENOENT == find_object(snapshot->ib2base, snapshot->process)) {
		kgsl_snapshot_push_object(snapshot->process, snapshot->ib2base,
				snapshot->ib2size);
	}

	/*
	 * Go through the list of found objects and dump each one.  As the IBs
	 * are parsed, more objects might be found, and objbufptr will increase
	 */
	for (i = 0; i < objbufptr; i++)
		dump_object(device, i, snapshot);

	/*
	 * Incase snapshot static blob is running out of memory, Add Active IB1
	 * and IB2 entries to obj_list so that active ib's can be dumped to
	 * snapshot dynamic blob.
	 */
	if (!snapshot->ib1dumped || !snapshot->ib2dumped)
		kgsl_snapshot_add_active_ib_obj_list(device, snapshot);

	if (ib_max_objs)
		KGSL_CORE_ERR("Max objects found in IB\n");
	if (snapshot_frozen_objsize)
		KGSL_CORE_ERR("GPU snapshot froze %zdKb of GPU buffers\n",
			snapshot_frozen_objsize / 1024);

}

/* adreno_snapshot_gmu - Snapshot the Adreno GMU state
 * @device - KGSL device to snapshot
 * @snapshot - Pointer to the snapshot instance
 * This is a hook function called by kgsl_snapshot to snapshot the
 * Adreno specific information for the GMU snapshot.  In turn, this function
 * calls the GMU specific snapshot function to get core specific information.
 */
void adreno_snapshot_gmu(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	/* Add GMU specific sections */
	if (GMU_DEV_OP_VALID(gmu_dev_ops, snapshot))
		gmu_dev_ops->snapshot(adreno_dev, snapshot);

	if (gpudev->snapshot_debugbus)
		gpudev->snapshot_debugbus(adreno_dev, snapshot);

}

/*
 * adreno_snapshot_cp_roq - Dump CP merciu data in snapshot
 * @device: Device being snapshotted
 * @remain: Bytes remaining in snapshot memory
 * @priv: Size of merciu data in Dwords
 */
size_t adreno_snapshot_cp_merciu(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int i, size = *((int *)priv);

	/* The MERCIU data is two dwords per entry */
	size = size << 1;

	if (remain < DEBUG_SECTION_SZ(size)) {
		SNAPSHOT_ERR_NOMEM(device, "CP MERCIU DEBUG");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_CP_MERCIU;
	header->size = size;

	adreno_writereg(adreno_dev, ADRENO_REG_CP_MERCIU_ADDR, 0x0);

	for (i = 0; i < size; i++) {
		adreno_readreg(adreno_dev, ADRENO_REG_CP_MERCIU_DATA,
			&data[(i * 2)]);
		adreno_readreg(adreno_dev, ADRENO_REG_CP_MERCIU_DATA2,
			&data[(i * 2) + 1]);
	}

	return DEBUG_SECTION_SZ(size);
}

/*
 * adreno_snapshot_cp_roq - Dump ROQ data in snapshot
 * @device: Device being snapshotted
 * @remain: Bytes remaining in snapshot memory
 * @priv: Size of ROQ data in Dwords
 */
size_t adreno_snapshot_cp_roq(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int i, size = *((int *)priv);

	if (remain < DEBUG_SECTION_SZ(size)) {
		SNAPSHOT_ERR_NOMEM(device, "CP ROQ DEBUG");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_CP_ROQ;
	header->size = size;

	adreno_writereg(adreno_dev, ADRENO_REG_CP_ROQ_ADDR, 0x0);
	for (i = 0; i < size; i++)
		adreno_readreg(adreno_dev, ADRENO_REG_CP_ROQ_DATA, &data[i]);

	return DEBUG_SECTION_SZ(size);
}

/*
 * adreno_snapshot_cp_pm4_ram() - Dump PM4 data in snapshot
 * @device: Device being snapshotted
 * @buf: Snapshot memory
 * @remain: Number of bytes left in snapshot memory
 * @priv: Unused
 */
size_t adreno_snapshot_cp_pm4_ram(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int i;
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_PM4);
	size_t size = fw->size - 1;

	if (remain < DEBUG_SECTION_SZ(size)) {
		SNAPSHOT_ERR_NOMEM(device, "CP PM4 RAM DEBUG");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_CP_PM4_RAM;
	header->size = size;

	/*
	 * Read the firmware from the GPU rather than use our cache in order to
	 * try to catch mis-programming or corruption in the hardware.  We do
	 * use the cached version of the size, however, instead of trying to
	 * maintain always changing hardcoded constants
	 */

	adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_RAM_RADDR, 0x0);
	for (i = 0; i < size; i++)
		adreno_readreg(adreno_dev, ADRENO_REG_CP_ME_RAM_DATA, &data[i]);

	return DEBUG_SECTION_SZ(size);
}

/*
 * adreno_snapshot_cp_pfp_ram() - Dump the PFP data on snapshot
 * @device: Device being snapshotted
 * @buf: Snapshot memory
 * @remain: Amount of butes left in snapshot memory
 * @priv: Unused
 */
size_t adreno_snapshot_cp_pfp_ram(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int i;
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_PFP);
	int size = fw->size - 1;

	if (remain < DEBUG_SECTION_SZ(size)) {
		SNAPSHOT_ERR_NOMEM(device, "CP PFP RAM DEBUG");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_CP_PFP_RAM;
	header->size = size;

	/*
	 * Read the firmware from the GPU rather than use our cache in order to
	 * try to catch mis-programming or corruption in the hardware.  We do
	 * use the cached version of the size, however, instead of trying to
	 * maintain always changing hardcoded constants
	 */
	adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_ADDR, 0x0);
	for (i = 0; i < size; i++)
		adreno_readreg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_DATA,
				&data[i]);

	return DEBUG_SECTION_SZ(size);
}

/*
 * adreno_snapshot_vpc_memory() - Save VPC data in snapshot
 * @device: Device being snapshotted
 * @buf: Snapshot memory
 * @remain: Number of bytes left in snapshot memory
 * @priv: Private data for VPC if any
 */
size_t adreno_snapshot_vpc_memory(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int vpc_mem_size = *((int *)priv);
	size_t size = VPC_MEMORY_BANKS * vpc_mem_size;
	int bank, addr, i = 0;

	if (remain < DEBUG_SECTION_SZ(size)) {
		SNAPSHOT_ERR_NOMEM(device, "VPC MEMORY");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_VPC_MEMORY;
	header->size = size;

	for (bank = 0; bank < VPC_MEMORY_BANKS; bank++) {
		for (addr = 0; addr < vpc_mem_size; addr++) {
			unsigned int val = bank | (addr << 4);

			adreno_writereg(adreno_dev,
				ADRENO_REG_VPC_DEBUG_RAM_SEL, val);
			adreno_readreg(adreno_dev,
				ADRENO_REG_VPC_DEBUG_RAM_READ, &data[i++]);
		}
	}

	return DEBUG_SECTION_SZ(size);
}

/*
 * adreno_snapshot_cp_meq() - Save CP MEQ data in snapshot
 * @device: Device being snapshotted
 * @buf: Snapshot memory
 * @remain: Number of bytes left in snapshot memory
 * @priv: Contains the size of MEQ data
 */
size_t adreno_snapshot_cp_meq(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int i;
	int cp_meq_sz = *((int *)priv);

	if (remain < DEBUG_SECTION_SZ(cp_meq_sz)) {
		SNAPSHOT_ERR_NOMEM(device, "CP MEQ DEBUG");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_CP_MEQ;
	header->size = cp_meq_sz;

	adreno_writereg(adreno_dev, ADRENO_REG_CP_MEQ_ADDR, 0x0);
	for (i = 0; i < cp_meq_sz; i++)
		adreno_readreg(adreno_dev, ADRENO_REG_CP_MEQ_DATA, &data[i]);

	return DEBUG_SECTION_SZ(cp_meq_sz);
}

static const struct adreno_vbif_snapshot_registers *vbif_registers(
		struct adreno_device *adreno_dev,
		const struct adreno_vbif_snapshot_registers *list,
		unsigned int count)
{
	unsigned int version;
	unsigned int i;

	adreno_readreg(adreno_dev, ADRENO_REG_VBIF_VERSION, &version);

	for (i = 0; i < count; i++) {
		if ((list[i].version & list[i].mask) ==
				(version & list[i].mask))
			return &list[i];
	}

	KGSL_CORE_ERR(
		"snapshot: Registers for VBIF version %X register were not dumped\n",
		version);

	return NULL;
}

void adreno_snapshot_registers(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		const unsigned int *regs, unsigned int count)
{
	struct kgsl_snapshot_registers r;

	r.regs = regs;
	r.count = count;

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS, snapshot,
		kgsl_snapshot_dump_registers, &r);
}

void adreno_snapshot_vbif_registers(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		const struct adreno_vbif_snapshot_registers *list,
		unsigned int count)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_registers regs;
	const struct adreno_vbif_snapshot_registers *vbif;

	vbif = vbif_registers(adreno_dev, list, count);

	if (vbif != NULL) {
		regs.regs = vbif->registers;
		regs.count = vbif->count;

		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
			snapshot, kgsl_snapshot_dump_registers, &regs);
	}
}
