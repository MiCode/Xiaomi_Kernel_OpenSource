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

#define MEM_BUF_MAX_NR_ACL_ENTS 16

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
 * struct mem_buf_alloc_ioctl_arg: A request to allocate memory from another
 * VM to other VMs.
 * @size: The size of the allocation.
 * @acl_list: An array of structures, where each structure specifies a VMID
 * and the access permissions that the VMID will have to the memory to be
 * allocated.
 * @nr_acl_entries: The number of ACL entries in @acl_list.
 * @src_mem_type: The type of memory that the source VM should allocate from.
 * This should be one of the mem_buf_mem_type enum values.
 * @src_data: A pointer to data that the source VM should interpret when
 * performing the allocation.
 * @dst_mem_type: The type of memory that the destination VM should treat the
 * incoming allocation from the source VM as. This should be one of the
 * mem_buf_mem_type enum values.
 * @mem_buf_fd: A file descriptor representing the memory that was allocated
 * from the source VM and added to the current VM. Calling close() on this file
 * descriptor will deallocate the memory from the current VM, and return it
 * to the source VM.
 * * @dst_data: A pointer to data that the destination VM should interpret when
 * adding the memory to the current VM.
 *
 * All reserved fields must be zeroed out by the caller prior to invoking the
 * allocation IOCTL command with this argument.
 */
struct mem_buf_alloc_ioctl_arg {
	__u64 size;
	__u64 acl_list;
	__u32 nr_acl_entries;
	__u32 src_mem_type;
	__u64 src_data;
	__u32 dst_mem_type;
	__u32 mem_buf_fd;
	__u64 dst_data;
	__u64 reserved0;
	__u64 reserved1;
	__u64 reserved2;
};

#define MEM_BUF_IOC_ALLOC		_IOWR(MEM_BUF_IOC_MAGIC, 0,\
					      struct mem_buf_alloc_ioctl_arg)

/**
 * struct mem_buf_export_ioctl_arg: An request to allocate memory from another
 * VM to other VMs.
 * @dma_buf_fd: The fd of the dma-buf that will be exported to another VM.
 * @nr_acl_entries: The number of ACL entries in @acl_list.
 * @acl_list: An array of structures, where each structure specifies a VMID
 * and the access permissions that the VMID will have to the memory to be
 * exported.
 * @export_fd: An fd that corresponds to the buffer that was exported. This fd
 * must be kept open until it is no longer required to export the memory to
 * another VM.
 * @memparcel_hdl: The handle associated with the memparcel that was created by
 * granting access to the dma-buf for the VMIDs specified in @acl_list.
 *
 * Note: The buffer must not be mmap'ed by any process prior to invoking this
 * IOCTL. The buffer must also be a cached buffer from a non-secure ION heap.
 *
 * All reserved fields must be zeroed out by the caller prior to invoking the
 * export IOCTL command with this argument.
 */
struct mem_buf_export_ioctl_arg {
	__u32 dma_buf_fd;
	__u32 nr_acl_entries;
	__u64 acl_list;
	__u32 export_fd;
	__u32 memparcel_hdl;
	__u64 reserved0;
	__u64 reserved1;
	__u64 reserved2;
};

#define MEM_BUF_IOC_EXPORT		_IOWR(MEM_BUF_IOC_MAGIC, 1,\
					      struct mem_buf_export_ioctl_arg)

/**
 * struct mem_buf_import_ioctl_arg: A request to import memory from another
 * VM as a dma-buf
 * @memparcel_hdl: The handle that corresponds to the memparcel we are
 * importing.
 * @nr_acl_entries: The number of ACL entries in @acl_list.
 * @acl_list: An array of structures, where each structure specifies a VMID
 * and the access permissions that the VMID should have for the memparcel.
 * @dma_buf_import_fd: A dma-buf file descriptor that the client can use to
 * access the buffer. This fd must be closed to release the memory.
 *
 * All reserved fields must be zeroed out by the caller prior to invoking the
 * import IOCTL command with this argument.
 */
struct mem_buf_import_ioctl_arg {
	__u32 memparcel_hdl;
	__u32 nr_acl_entries;
	__u64 acl_list;
	__u32 dma_buf_import_fd;
	__u32 reserved0;
	__u64 reserved1;
	__u64 reserved2;
	__u64 reserved3;
};

#define MEM_BUF_IOC_IMPORT		_IOWR(MEM_BUF_IOC_MAGIC, 2,\
					      struct mem_buf_import_ioctl_arg)

#endif /* _UAPI_LINUX_MEM_BUF_H */
