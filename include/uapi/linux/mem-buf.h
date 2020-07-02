/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _UAPI_LINUX_MEM_BUF_H
#define _UAPI_LINUX_MEM_BUF_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define MEM_BUF_IOC_MAGIC 'M'

/**
 * enum mem_buf_mem_type: Types of memory that can be allocated from and to
 * @MEM_BUF_ION_MEM_TYPE: The memory for the source or destination is ION memory
 */
enum mem_buf_mem_type {
	MEM_BUF_ION_MEM_TYPE,
	MEM_BUF_MAX_MEM_TYPE,
};

/* The mem-buf values that represent VMIDs for an ACL. */
#define MEM_BUF_VMID_PRIMARY_VM 0
#define	MEM_BUF_VMID_TRUSTED_VM 1

#define MEM_BUF_PERM_FLAG_READ (1U << 0)
#define MEM_BUF_PERM_FLAG_WRITE (1U << 1)
#define MEM_BUF_PERM_FLAG_EXEC (1U << 2)
#define MEM_BUF_PERM_VALID_FLAGS\
	(MEM_BUF_PERM_FLAG_READ | MEM_BUF_PERM_FLAG_WRITE |\
	 MEM_BUF_PERM_FLAG_EXEC)

/**
 * struct acl_entry: Represents the access control permissions for a VMID.
 * @vmid: The mem-buf VMID specifier associated with the VMID that will access
 * the memory.
 * @perms: The access permissions for the VMID in @vmid. This flag is
 * interpreted as a bitmap, and thus, should be a combination of one or more
 * of the MEM_BUF_PERM_FLAG_* flags.
 */
struct acl_entry {
	__u32 vmid;
	__u32 perms;
};

/**
 * struct mem_buf_ion_data: Data that is unique to memory that is of type
 * MEM_BUF_ION_MEM_TYPE.
 * @heap_id: The heap ID of where memory should be allocated from or added to.
 */
struct mem_buf_ion_data {
	__u32 heap_id;
};

/**
 * struct mem_buf_alloc_ioctl_arg: An request to allocate memory from another
 * VM to other VMs.
 * @size: The size of the allocation.
 * @nr_acl_entries: The number of ACL entries in @acl_list.
 * @acl_list: An array of structures, where each structure specifies a VMID
 * and the access permissions that the VMID will have to the memory to be
 * allocated.
 * @src_mem_type: The type of memory that the source VM should allocate from.
 * This should be one of the mem_buf_mem_type enum values.
 * @src_data: A pointer to data that the source VM should interpret when
 * performing the allocation.
 * @dst_mem_type: The type of memory that the destination VM should treat the
 * incoming allocation from the source VM as. This should be one of the
 * mem_buf_mem_type enum values.
 * @dst_data: A pointer to data that the destination VM should interpret when
 * adding the memory to the current VM.
 * @mem_buf_fd: A file descriptor representing the memory that was allocated
 * from the source VM and added to the current VM. Calling close() on this file
 * descriptor will deallocate the memory from the current VM, and return it
 * to the source VM.
 *
 * All reserved fields must be zeroed out by the caller prior to invoking the
 * allocation IOCTL command with this argument.
 */
struct mem_buf_alloc_ioctl_arg {
	__u64 size;
	__u32 nr_acl_entries;
	__u64 acl_list;
	__u32 src_mem_type;
	__u64 src_data;
	__u32 dst_mem_type;
	__u64 dst_data;
	__u32 mem_buf_fd;
	__u64 reserved0;
	__u64 reserved1;
	__u64 reserved2;
};

#define MEM_BUF_IOC_ALLOC		_IOWR(MEM_BUF_IOC_MAGIC, 0,\
					      struct mem_buf_alloc_ioctl_arg)

#endif /* _UAPI_LINUX_MEM_BUF_H */
