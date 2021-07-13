/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _MEM_BUF_H
#define _MEM_BUF_H

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/types.h>
#include <linux/dma-buf.h>
#include <uapi/linux/mem-buf.h>

/**
 * Private definitions; they are just here since the tracepoint logic needs
 * these definitions. Drivers should not use these.
 */

/**
 * enum mem_buf_msg_type: Message types used by the membuf driver for
 * communication.
 * @MEM_BUF_ALLOC_REQ: The message is an allocation request from another VM to
 * the receiving VM
 * @MEM_BUF_ALLOC_RESP: The message is a response from a remote VM to an
 * allocation request issued by the receiving VM
 * @MEM_BUF_ALLOC_RELINQUISH: The message is a notification from another VM
 * that the receiving VM can reclaim the memory.
 */
enum mem_buf_msg_type {
	MEM_BUF_ALLOC_REQ,
	MEM_BUF_ALLOC_RESP,
	MEM_BUF_ALLOC_RELINQUISH,
	MEM_BUF_ALLOC_REQ_MAX,
};

/**
 * struct mem_buf_msg_hdr: The header for all membuf messages
 * @txn_id: The transaction ID for the message. This field is only meaningful
 * for request/response type of messages.
 * @msg_type: The type of message.
 */
struct mem_buf_msg_hdr {
	u32 txn_id;
	u32 msg_type;
} __packed;

/**
 * struct mem_buf_alloc_req: The message format for a memory allocation request
 * to another VM.
 * @hdr: Message header
 * @size: The size of the memory allocation to be performed on the remote VM.
 * @src_mem_type: The type of memory that the remote VM should allocate.
 * @acl_desc: A GH ACL descriptor that describes the VMIDs that will be
 * accessing the memory, as well as what permissions each VMID will have.
 *
 * NOTE: Certain memory types require additional information for the remote VM
 * to interpret. That information should be concatenated with this structure
 * prior to sending the allocation request to the remote VM. For example,
 * with memory type ION, the allocation request message will consist of this
 * structure, as well as the mem_buf_ion_alloc_data structure.
 */
struct mem_buf_alloc_req {
	struct mem_buf_msg_hdr hdr;
	u64 size;
	u32 src_mem_type;
	struct gh_acl_desc acl_desc;
} __packed;

/**
 * struct mem_buf_ion_alloc_data: Represents the data needed to perform
 * an ION allocation on a remote VM.
 * @heap_id: The ID of the heap to allocate from
 */
struct mem_buf_ion_alloc_data {
	u32 heap_id;
} __packed;

/**
 * struct mem_buf_alloc_resp: The message format for a memory allocation
 * request response.
 * @hdr: Message header
 * @ret: Return code from remote VM
 * @hdl: The memparcel handle associated with the memory allocated to the
 * receiving VM. This field is only meaningful if the allocation on the remote
 * VM was carried out successfully, as denoted by @ret.
 */
struct mem_buf_alloc_resp {
	struct mem_buf_msg_hdr hdr;
	s32 ret;
	u32 hdl;
} __packed;

/**
 * struct mem_buf_alloc_relinquish: The message format for a notification
 * that the current VM has relinquished access to the memory lent to it by
 * another VM.
 * @hdr: Message header
 * @hdl: The memparcel handle associated with the memory.
 */
struct mem_buf_alloc_relinquish {
	struct mem_buf_msg_hdr hdr;
	u32 hdl;
} __packed;

/* Public definitions */

/* Used to obtain the underlying vmperm struct of a DMA-BUF */
struct mem_buf_vmperm *to_mem_buf_vmperm(struct dma_buf *dmabuf);

/* Returns true if the local VM has exclusive access and is the owner */
bool mem_buf_dma_buf_exclusive_owner(struct dma_buf *dmabuf);

/*
 * Returns a copy of the Virtual Machine vmids & permissions of the dmabuf.
 * The caller must kfree() when finished.
 */
int mem_buf_dma_buf_copy_vmperm(struct dma_buf *dmabuf, int **vmids, int **perms,
		int *nr_acl_entries);

typedef int (*mem_buf_dma_buf_destructor)(void *dtor_data);
int mem_buf_dma_buf_set_destructor(struct dma_buf *dmabuf,
				   mem_buf_dma_buf_destructor dtor,
				   void *dtor_data);

/**
 * struct mem_buf_allocation_data - Data structure that contains information
 * about a memory buffer allocation request.
 * @size: The size (in bytes) of the memory to be requested from a remote VM
 * @nr_acl_entries: The number of ACL entries in @acl_list
 * @acl_list: A list of VMID and permission pairs that describe what VMIDs will
 * have access to the memory, and with what permissions
 * @src_mem_type: The type of memory that the remote VM should allocate
 * (e.g. ION memory)
 * @src_data: A pointer to memory type specific data that the remote VM may need
 * when performing an allocation (e.g. ION memory allocations require a heap ID)
 * @dst_mem_type: The type of memory that the native VM wants (e.g. ION memory)
 * @dst_data: A pointer to memory type specific data that the native VM may
 * need when adding the memory from the remote VM (e.g. ION memory requires a
 * heap ID to add the memory to).
 */
struct mem_buf_allocation_data {
	size_t size;
	unsigned int nr_acl_entries;
	int *vmids;
	int *perms;
	enum mem_buf_mem_type src_mem_type;
	void *src_data;
	enum mem_buf_mem_type dst_mem_type;
	void *dst_data;
};

struct mem_buf_lend_kernel_arg {
	unsigned int nr_acl_entries;
	int *vmids;
	int *perms;
	gh_memparcel_handle_t memparcel_hdl;
	u32 flags;
	u64 label;
};

int mem_buf_lend(struct dma_buf *dmabuf,
		struct mem_buf_lend_kernel_arg *arg);

/*
 * mem_buf_share
 * Grant the local VM, as well as one or more remote VMs access
 * to the dmabuf. The permissions of the local VM default to RWX
 * unless otherwise specified.
 */
int mem_buf_share(struct dma_buf *dmabuf,
		struct mem_buf_lend_kernel_arg *arg);


struct mem_buf_retrieve_kernel_arg {
	u32 sender_vmid;
	unsigned int nr_acl_entries;
	int *vmids;
	int *perms;
	gh_memparcel_handle_t memparcel_hdl;
	int fd_flags;
};
struct dma_buf *mem_buf_retrieve(struct mem_buf_retrieve_kernel_arg *arg);
int mem_buf_reclaim(struct dma_buf *dmabuf);

#if IS_ENABLED(CONFIG_QCOM_MEM_BUF)

int mem_buf_get_fd(void *membuf_desc);

void mem_buf_put(void *membuf_desc);

void *mem_buf_get(int fd);

#else

static inline int mem_buf_get_fd(void *membuf_desc)
{
	return -ENODEV;
}

static inline void mem_buf_put(void *membuf_desc)
{
}

static inline void *mem_buf_get(int fd)
{
	return ERR_PTR(-ENODEV);
}

#endif /* CONFIG_QCOM_MEM_BUF */
#endif /* _MEM_BUF_H */
