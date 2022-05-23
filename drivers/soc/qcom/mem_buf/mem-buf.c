// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/anon_inodes.h>
#include <linux/cdev.h>
#include <linux/dma-buf.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/gunyah/gh_msgq.h>
#include <linux/ion.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mem-buf.h>
#include <linux/memblock.h>
#include <linux/memory_hotplug.h>
#include <linux/module.h>
#include <linux/msm_ion.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mem-buf-exporter.h>
#include <linux/qcom_dma_heap.h>

#include <soc/qcom/secure_buffer.h>
#include <uapi/linux/mem-buf.h>

#include "../../../../drivers/dma-buf/heaps/qcom_sg_ops.h"
#include "mem-buf-dev.h"
#include "mem-buf-ids.h"
#include "trace-mem-buf.h"

#define MEM_BUF_MAX_DEVS 1
#define MEM_BUF_MHP_ALIGNMENT (1UL << SUBSECTION_SHIFT)
#define MEM_BUF_TIMEOUT_MS 3500
#define to_rmt_msg(_work) container_of(_work, struct mem_buf_rmt_msg, work)

/* Data structures for requesting/maintaining memory from other VMs */
static dev_t mem_buf_dev_no;
static struct class *mem_buf_class;
static struct cdev mem_buf_char_dev;

/* Maintains a list of memory buffers requested from other VMs */
static DEFINE_MUTEX(mem_buf_list_lock);
static LIST_HEAD(mem_buf_list);

/*
 * Data structures for tracking request/reply transactions, as well as message
 * queue usage
 */
static DEFINE_MUTEX(mem_buf_idr_mutex);
static DEFINE_IDR(mem_buf_txn_idr);
static struct task_struct *mem_buf_msgq_recv_thr;
static void *mem_buf_gh_msgq_hdl;
static struct workqueue_struct *mem_buf_wq;

static size_t mem_buf_get_sgl_buf_size(struct gh_sgl_desc *sgl_desc);
static int mem_buf_acl_to_vmid_perms_list(unsigned int nr_acl_entries,
					  const void __user *acl_entries,
					  int **dst_vmids, int **dst_perms,
					  bool lookup_fd);
/**
 * struct mem_buf_txn: Represents a transaction (request/response pair) in the
 * membuf driver.
 * @txn_id: Transaction ID used to match requests and responses (i.e. a new ID
 * is allocated per request, and the response will have a matching ID).
 * @txn_ret: The return value of the transaction.
 * @txn_done: Signals that a response has arrived.
 * @resp_buf: A pointer to a buffer where the response should be decoded into.
 */
struct mem_buf_txn {
	int txn_id;
	int txn_ret;
	struct completion txn_done;
	void *resp_buf;
};

/* Maintains a list of memory buffers lent out to other VMs */
static DEFINE_MUTEX(mem_buf_xfer_mem_list_lock);
static LIST_HEAD(mem_buf_xfer_mem_list);

/**
 * struct mem_buf_rmt_msg: Represents a message sent from a remote VM
 * @msg: A pointer to the message buffer
 * @msg_size: The size of the message
 * @work: work structure for dispatching the message processing to a worker
 * thread, so as to not block the message queue receiving thread.
 */
struct mem_buf_rmt_msg {
	void *msg;
	size_t msg_size;
	struct work_struct work;
};

/**
 * struct mem_buf_xfer_mem: Represents a memory buffer lent out or transferred
 * to another VM.
 * @size: The size of the memory buffer
 * @mem_type: The type of memory that was allocated and transferred
 * @mem_type_data: Data associated with the type of memory
 * @mem_sgt: An SG-Table representing the memory transferred
 * @secure_alloc: Denotes if the memory was assigned to the targeted VMs as part
 * of the allocation step
 * @hdl: The memparcel handle associated with the memory
 * @gh_rm_trans_type: Whether handle is Lent/Shared/Donated
 * @entry: List entry for maintaining a list of memory buffers that are lent
 * out.
 * @nr_acl_entries: The number of VMIDs and permissions associated with the
 * memory
 * @dst_vmids: The VMIDs that have access to the memory
 * @dst_perms: The access permissions for the VMIDs that can access the memory
 */
struct mem_buf_xfer_mem {
	size_t size;
	enum mem_buf_mem_type mem_type;
	void *mem_type_data;
	struct sg_table *mem_sgt;
	bool secure_alloc;
	int gh_rm_trans_type;
	gh_memparcel_handle_t hdl;
	struct list_head entry;
	u32 nr_acl_entries;
	int *dst_vmids;
	int *dst_perms;
	int obj_id;
};

/**
 * struct mem_buf_desc - Internal data structure, which contains information
 * about a particular memory buffer.
 * @size: The size of the memory buffer
 * @acl_desc: A GH ACL descriptor that describes the VMIDs that have access to
 * the memory, as well as the permissions each VMID has.
 * @sgl_desc: An GH SG-List descriptor that describes the IPAs of the memory
 * associated with the memory buffer that was allocated from another VM.
 * @memparcel_hdl: The handle associated with the memparcel that represents the
 * memory buffer.
 * @src_mem_type: The type of memory that was allocated on the remote VM
 * @src_data: Memory type specific data used by the remote VM when performing
 * the allocation.
 * @dst_mem_type: The memory type of the memory buffer on the native VM
 * @dst_data: Memory type specific data used by the native VM when adding the
 * memory to the system.
 * @filp: Pointer to the file structure for the membuf
 * @entry: List head for maintaing a list of memory buffers that have been
 * provided by remote VMs.
 * @obj_id: Unique identifier issued by PVM.
 * @add_mem_complete: Boolean indicating whether mem_buf_remove_mem() needs to be
 * called on object teardown.
 */
struct mem_buf_desc {
	size_t size;
	struct gh_acl_desc *acl_desc;
	struct gh_sgl_desc *sgl_desc;
	gh_memparcel_handle_t memparcel_hdl;
	int gh_rm_trans_type;
	enum mem_buf_mem_type src_mem_type;
	void *src_data;
	enum mem_buf_mem_type dst_mem_type;
	void *dst_data;
	struct file *filp;
	struct list_head entry;
	int obj_id;
	bool add_mem_complete;
};
static DEFINE_IDR(mem_buf_obj_idr);

struct mem_buf_xfer_dmaheap_mem {
	char name[MEM_BUF_MAX_DMAHEAP_NAME_LEN];
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
};

static int mem_buf_init_txn(struct mem_buf_txn *txn, void *resp_buf)
{
	int ret;

	mutex_lock(&mem_buf_idr_mutex);
	ret = idr_alloc_cyclic(&mem_buf_txn_idr, txn, 0, INT_MAX, GFP_KERNEL);
	mutex_unlock(&mem_buf_idr_mutex);
	if (ret < 0) {
		pr_err("%s: failed to allocate transaction id rc: %d\n",
		       __func__, ret);
		return ret;
	}

	txn->txn_id = ret;
	init_completion(&txn->txn_done);
	txn->resp_buf = resp_buf;
	return 0;
}

static int mem_buf_msg_send(void *msg, size_t msg_size)
{
	int ret;

	ret = gh_msgq_send(mem_buf_gh_msgq_hdl, msg, msg_size, 0);
	if (ret < 0)
		pr_err("%s: failed to send allocation request rc: %d\n",
		       __func__, ret);
	else
		pr_debug("%s: alloc request sent\n", __func__);

	return ret;
}

static int mem_buf_txn_wait(struct mem_buf_txn *txn)
{
	int ret;

	pr_debug("%s: waiting for allocation response\n", __func__);
	ret = wait_for_completion_timeout(&txn->txn_done,
					  msecs_to_jiffies(MEM_BUF_TIMEOUT_MS));
	if (ret == 0) {
		pr_err("%s: timed out waiting for allocation response\n",
		       __func__);
		return -ETIMEDOUT;
	} else {
		pr_debug("%s: alloc response received\n", __func__);
	}

	return txn->txn_ret;
}

static void mem_buf_destroy_txn(struct mem_buf_txn *txn)
{
	mutex_lock(&mem_buf_idr_mutex);
	idr_remove(&mem_buf_txn_idr, txn->txn_id);
	mutex_unlock(&mem_buf_idr_mutex);
}

static int mem_buf_alloc_obj_id(void)
{
	int ret;

	mutex_lock(&mem_buf_idr_mutex);
	ret = idr_alloc_cyclic(&mem_buf_obj_idr, NULL, 0, INT_MAX, GFP_KERNEL);
	mutex_unlock(&mem_buf_idr_mutex);
	if (ret < 0) {
		pr_err("%s: failed to allocate obj id rc: %d\n",
		       __func__, ret);
		return ret;
	}
	return ret;
}

static void mem_buf_destroy_obj_id(int obj_id)
{
	mutex_lock(&mem_buf_idr_mutex);
	idr_remove(&mem_buf_obj_idr, obj_id);
	mutex_unlock(&mem_buf_idr_mutex);
}

/* Functions invoked when treating allocation requests from other VMs. */
static int mem_buf_rmt_alloc_dmaheap_mem(struct mem_buf_xfer_mem *xfer_mem)
{
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *mem_sgt;
	struct mem_buf_xfer_dmaheap_mem *dmaheap_mem_data = xfer_mem->mem_type_data;
	int flags = O_RDWR | O_CLOEXEC;
	struct dma_heap *heap;
	char *name = dmaheap_mem_data->name;

	pr_debug("%s: Starting DMAHEAP allocation\n", __func__);
	heap = dma_heap_find(name);
	if (!heap) {
		pr_err("%s no such heap %s\n", __func__, name);
		return -EINVAL;
	}

	dmabuf = dma_heap_buffer_alloc(heap, xfer_mem->size, flags, 0);
	if (IS_ERR(dmabuf)) {
		pr_err("%s dmaheap_alloc failure sz: 0x%x heap: %s flags: 0x%x rc: %d\n",
		       __func__, xfer_mem->size, name, flags,
		       PTR_ERR(dmabuf));
		return PTR_ERR(dmabuf);
	}

	attachment = dma_buf_attach(dmabuf, mem_buf_dev);
	if (IS_ERR(attachment)) {
		pr_err("%s dma_buf_attach failure rc: %d\n",  __func__,
		       PTR_ERR(attachment));
		dma_buf_put(dmabuf);
		return PTR_ERR(attachment);
	}

	mem_sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(mem_sgt)) {
		pr_err("%s dma_buf_map_attachment failure rc: %d\n", __func__,
		       PTR_ERR(mem_sgt));
		dma_buf_detach(dmabuf, attachment);
		dma_buf_put(dmabuf);
		return PTR_ERR(mem_sgt);
	}

	dmaheap_mem_data->dmabuf = dmabuf;
	dmaheap_mem_data->attachment = attachment;
	xfer_mem->mem_sgt = mem_sgt;
	xfer_mem->secure_alloc = false;

	pr_debug("%s: DMAHEAP allocation complete\n", __func__);
	return 0;
}

static int mem_buf_rmt_alloc_mem(struct mem_buf_xfer_mem *xfer_mem)
{
	int ret = -EINVAL;

	if (xfer_mem->mem_type == MEM_BUF_DMAHEAP_MEM_TYPE)
		ret = mem_buf_rmt_alloc_dmaheap_mem(xfer_mem);

	return ret;
}

static void mem_buf_rmt_free_dmaheap_mem(struct mem_buf_xfer_mem *xfer_mem)
{
	struct mem_buf_xfer_dmaheap_mem *dmaheap_mem_data = xfer_mem->mem_type_data;
	struct dma_buf *dmabuf = dmaheap_mem_data->dmabuf;
	struct dma_buf_attachment *attachment = dmaheap_mem_data->attachment;
	struct sg_table *mem_sgt = xfer_mem->mem_sgt;

	pr_debug("%s: Freeing DMAHEAP memory\n", __func__);
	dma_buf_unmap_attachment(attachment, mem_sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attachment);

	dma_buf_put(dmaheap_mem_data->dmabuf);
	/*
	 * No locks should be held at this point, as flush_delayed_fput may call the
	 * release callbacks of arbitrary files. It should be safe for us since we
	 * know this function is called only from our recv kthread, so we have control
	 * over what locks are currently held.
	 */
	flush_delayed_fput();
	pr_debug("%s: DMAHEAP memory freed\n", __func__);
}

static void mem_buf_rmt_free_mem(struct mem_buf_xfer_mem *xfer_mem)
{
	if (xfer_mem->mem_type == MEM_BUF_DMAHEAP_MEM_TYPE)
		mem_buf_rmt_free_dmaheap_mem(xfer_mem);
}

static
struct mem_buf_xfer_dmaheap_mem *mem_buf_alloc_dmaheap_xfer_mem_type_data(
								void *rmt_data)
{
	struct mem_buf_xfer_dmaheap_mem *dmaheap_mem_data;

	dmaheap_mem_data = kzalloc(sizeof(*dmaheap_mem_data), GFP_KERNEL);
	if (!dmaheap_mem_data)
		return ERR_PTR(-ENOMEM);

	strlcpy(dmaheap_mem_data->name, (char *)rmt_data,
		MEM_BUF_MAX_DMAHEAP_NAME_LEN);
	pr_debug("%s: DMAHEAP source heap: %s\n", __func__,
		dmaheap_mem_data->name);
	return dmaheap_mem_data;
}

static void *mem_buf_alloc_xfer_mem_type_data(enum mem_buf_mem_type type,
					      void *rmt_data)
{
	void *data = ERR_PTR(-EINVAL);

	if (type == MEM_BUF_DMAHEAP_MEM_TYPE)
		data = mem_buf_alloc_dmaheap_xfer_mem_type_data(rmt_data);

	return data;
}

static
void mem_buf_free_dmaheap_xfer_mem_type_data(struct mem_buf_xfer_dmaheap_mem *mem)
{
	kfree(mem);
}

static void mem_buf_free_xfer_mem_type_data(enum mem_buf_mem_type type,
					    void *data)
{
	if (type == MEM_BUF_DMAHEAP_MEM_TYPE)
		mem_buf_free_dmaheap_xfer_mem_type_data(data);
}

static
struct mem_buf_xfer_mem *mem_buf_prep_xfer_mem(void *req_msg)
{
	int ret;
	struct mem_buf_xfer_mem *xfer_mem;
	struct mem_buf_alloc_req *req = req_msg;
	u32 nr_acl_entries;
	size_t alloc_req_msg_size;
	void *arb_payload;
	void *mem_type_data;

	nr_acl_entries = req->acl_desc.n_acl_entries;
	if (nr_acl_entries != 1)
		return ERR_PTR(-EINVAL);

	alloc_req_msg_size = offsetof(struct mem_buf_alloc_req,
					acl_desc.acl_entries[nr_acl_entries]);
	arb_payload = req_msg + alloc_req_msg_size;

	xfer_mem = kzalloc(sizeof(*xfer_mem), GFP_KERNEL);
	if (!xfer_mem)
		return ERR_PTR(-ENOMEM);

	xfer_mem->obj_id = mem_buf_alloc_obj_id();
	if (xfer_mem->obj_id < 0) {
		ret = xfer_mem->obj_id;
		goto free_xfer_mem;
	}
	xfer_mem->size = req->size;
	xfer_mem->mem_type = req->src_mem_type;
	xfer_mem->nr_acl_entries = req->acl_desc.n_acl_entries;
	ret = mem_buf_gh_acl_desc_to_vmid_perm_list(&req->acl_desc,
						    &xfer_mem->dst_vmids,
						    &xfer_mem->dst_perms);
	if (ret) {
		pr_err("%s failed to create VMID and permissions list: %d\n",
		       __func__, ret);
		goto free_obj_id;
	}
	mem_type_data = mem_buf_alloc_xfer_mem_type_data(req->src_mem_type,
							 arb_payload);
	if (IS_ERR(mem_type_data)) {
		pr_err("%s: failed to allocate mem type specific data: %d\n",
		       __func__, PTR_ERR(mem_type_data));
		ret = PTR_ERR(mem_type_data);
		goto free_acl;
	}
	xfer_mem->mem_type_data = mem_type_data;
	INIT_LIST_HEAD(&xfer_mem->entry);
	return xfer_mem;
free_acl:
	kfree(xfer_mem->dst_vmids);
	kfree(xfer_mem->dst_perms);
free_obj_id:
	mem_buf_destroy_obj_id(xfer_mem->obj_id);
free_xfer_mem:
	kfree(xfer_mem);
	return ERR_PTR(ret);
}

static void mem_buf_free_xfer_mem(struct mem_buf_xfer_mem *xfer_mem)
{
	mem_buf_free_xfer_mem_type_data(xfer_mem->mem_type,
					xfer_mem->mem_type_data);
	kfree(xfer_mem->dst_vmids);
	kfree(xfer_mem->dst_perms);
	mem_buf_destroy_obj_id(xfer_mem->obj_id);
	kfree(xfer_mem);
}

/*
 * @owner_vmid: Owner of the memparcel handle which has @vmids and @perms
 */
static int __maybe_unused mem_buf_get_mem_xfer_type(int *vmids, int *perms,
				unsigned int nr_acl_entries, int owner_vmid)
{
	u32 i;

	for (i = 0; i < nr_acl_entries; i++)
		if (vmids[i] == owner_vmid &&
		    perms[i] != 0)
			return GH_RM_TRANS_TYPE_SHARE;

	return GH_RM_TRANS_TYPE_LEND;
}

/*
 * @owner_vmid: Owner of the memparcel handle which has @acl_desc
 */
static int mem_buf_get_mem_xfer_type_gh(struct gh_acl_desc *acl_desc, int owner_vmid)
{
	u32 i, nr_acl_entries = acl_desc->n_acl_entries;

	for (i = 0; i < nr_acl_entries; i++)
		if (acl_desc->acl_entries[i].vmid == owner_vmid &&
		    acl_desc->acl_entries[i].perms != 0)
			return GH_RM_TRANS_TYPE_SHARE;

	return GH_RM_TRANS_TYPE_LEND;
}

/*
 * Check whether donate operation is supported. If not, use
 * Lend instead. Share is not supported for remotealloc.
 */
static int get_alloc_req_xfer_type(struct mem_buf_xfer_mem *xfer_mem)
{
	static bool initialized;
	static int alloc_req_xfer_type;
	struct mem_buf_lend_kernel_arg arg;
	int vmids[] = {VMID_TUIVM};
	int perms[] = {PERM_READ | PERM_WRITE | PERM_EXEC};
	int ret;

	if (initialized)
		return alloc_req_xfer_type;

	arg.nr_acl_entries = ARRAY_SIZE(vmids);
	arg.vmids = vmids;
	arg.perms = perms;
	arg.flags = 0;
	arg.label = 0;

	ret = mem_buf_assign_mem(GH_RM_TRANS_TYPE_DONATE, xfer_mem->mem_sgt, &arg);
	if (ret) {
		initialized = true;
		alloc_req_xfer_type = GH_RM_TRANS_TYPE_LEND;
	} else {
		initialized = true;
		alloc_req_xfer_type = GH_RM_TRANS_TYPE_DONATE;

		mem_buf_unassign_mem(xfer_mem->mem_sgt, vmids, ARRAY_SIZE(vmids),
					arg.memparcel_hdl);
	}
	pr_info("%s: xfer_type set to %d\n", __func__, alloc_req_xfer_type);
	return alloc_req_xfer_type;
}

static struct mem_buf_xfer_mem *mem_buf_process_alloc_req(void *req)
{
	int ret, xfer_type;
	struct mem_buf_alloc_req *req_msg = req;
	struct mem_buf_xfer_mem *xfer_mem;
	struct mem_buf_lend_kernel_arg arg = {0};

	xfer_mem = mem_buf_prep_xfer_mem(req_msg);
	if (IS_ERR(xfer_mem))
		return xfer_mem;

	ret = mem_buf_rmt_alloc_mem(xfer_mem);
	if (ret < 0)
		goto err_rmt_alloc;

	if (!xfer_mem->secure_alloc) {
		xfer_type = get_alloc_req_xfer_type(xfer_mem);

		arg.nr_acl_entries = xfer_mem->nr_acl_entries;
		arg.vmids = xfer_mem->dst_vmids;
		arg.perms = xfer_mem->dst_perms;
		ret = mem_buf_assign_mem(xfer_type, xfer_mem->mem_sgt, &arg);
		if (ret < 0)
			goto err_assign_mem;

		xfer_mem->hdl = arg.memparcel_hdl;
		xfer_mem->gh_rm_trans_type = xfer_type;
	}

	mutex_lock(&mem_buf_xfer_mem_list_lock);
	list_add(&xfer_mem->entry, &mem_buf_xfer_mem_list);
	mutex_unlock(&mem_buf_xfer_mem_list_lock);

	return xfer_mem;

err_assign_mem:
	if (ret != -EADDRNOTAVAIL)
		mem_buf_rmt_free_mem(xfer_mem);
err_rmt_alloc:
	mem_buf_free_xfer_mem(xfer_mem);
	return ERR_PTR(ret);
}

static void mem_buf_cleanup_alloc_req(struct mem_buf_xfer_mem *xfer_mem,
				gh_memparcel_handle_t memparcel_hdl)
{
	int ret;

	if (!xfer_mem->secure_alloc) {
		if (memparcel_hdl == xfer_mem->hdl) {
			ret = mem_buf_unassign_mem(xfer_mem->mem_sgt,
						   xfer_mem->dst_vmids,
						   xfer_mem->nr_acl_entries,
						   xfer_mem->hdl);
			if (ret < 0)
				return;
		} else {
			struct gh_sgl_desc *sgl_desc;
			struct gh_acl_desc *acl_desc;
			size_t size;

			size = struct_size(acl_desc, acl_entries, 1);
			acl_desc = kzalloc(size, GFP_KERNEL);
			if (!acl_desc)
				return;

			acl_desc->n_acl_entries = 1;
			acl_desc->acl_entries[0].vmid = VMID_HLOS;
			acl_desc->acl_entries[0].perms = GH_RM_ACL_X | GH_RM_ACL_W | GH_RM_ACL_R;


			sgl_desc  = mem_buf_map_mem_s2(GH_RM_TRANS_TYPE_DONATE,
					&memparcel_hdl, acl_desc, VMID_TUIVM);
			if (IS_ERR(sgl_desc)) {
				kfree(acl_desc);
				return;
			}
			kfree(sgl_desc);
			kfree(acl_desc);
		}

	}
	mem_buf_rmt_free_mem(xfer_mem);
	mem_buf_free_xfer_mem(xfer_mem);
}

static void mem_buf_alloc_req_work(struct work_struct *work)
{
	struct mem_buf_rmt_msg *rmt_msg = to_rmt_msg(work);
	struct mem_buf_alloc_req *req_msg = rmt_msg->msg;
	struct mem_buf_alloc_resp *resp_msg;
	struct mem_buf_xfer_mem *xfer_mem;
	int ret = 0;

	trace_receive_alloc_req(req_msg);
	resp_msg = kzalloc(sizeof(*resp_msg), GFP_KERNEL);
	if (!resp_msg)
		goto err_alloc_resp;
	resp_msg->hdr.txn_id = req_msg->hdr.txn_id;
	resp_msg->hdr.msg_type = MEM_BUF_ALLOC_RESP;

	xfer_mem = mem_buf_process_alloc_req(rmt_msg->msg);
	if (IS_ERR(xfer_mem)) {
		ret = PTR_ERR(xfer_mem);
		pr_err("%s: failed to process rmt memory alloc request: %d\n",
		       __func__, ret);
		xfer_mem = NULL;
	} else {
		resp_msg->hdl = xfer_mem->hdl;
		resp_msg->obj_id = xfer_mem->obj_id;
		resp_msg->gh_rm_trans_type = xfer_mem->gh_rm_trans_type;
	}

	resp_msg->ret = ret;
	trace_send_alloc_resp_msg(resp_msg);
	ret = gh_msgq_send(mem_buf_gh_msgq_hdl, resp_msg, sizeof(*resp_msg), 0);

	/*
	 * Free the buffer regardless of the return value as the hypervisor
	 * would have consumed the data in the case of a success.
	 */
	kfree(resp_msg);

	if (ret < 0) {
		pr_err("%s: failed to send memory allocation response rc: %d\n",
		       __func__, ret);
		if (xfer_mem) {
			mutex_lock(&mem_buf_xfer_mem_list_lock);
			list_del(&xfer_mem->entry);
			mutex_unlock(&mem_buf_xfer_mem_list_lock);
			mem_buf_cleanup_alloc_req(xfer_mem, xfer_mem->hdl);
		}
	} else {
		pr_debug("%s: Allocation response sent\n", __func__);
	}

err_alloc_resp:
	kfree(rmt_msg->msg);
	kfree(rmt_msg);
}

static void mem_buf_relinquish_work(struct work_struct *work)
{
	struct mem_buf_xfer_mem *xfer_mem_iter, *tmp, *xfer_mem = NULL;
	struct mem_buf_rmt_msg *rmt_msg = to_rmt_msg(work);
	struct mem_buf_alloc_relinquish *relinquish_msg = rmt_msg->msg;
	struct mem_buf_alloc_relinquish resp_msg = {0};
	int ret;

	trace_receive_relinquish_msg(relinquish_msg);
	mutex_lock(&mem_buf_xfer_mem_list_lock);
	list_for_each_entry_safe(xfer_mem_iter, tmp, &mem_buf_xfer_mem_list,
				 entry)
		if (xfer_mem_iter->obj_id == relinquish_msg->obj_id) {
			xfer_mem = xfer_mem_iter;
			list_del(&xfer_mem->entry);
			break;
		}
	mutex_unlock(&mem_buf_xfer_mem_list_lock);

	if (xfer_mem)
		mem_buf_cleanup_alloc_req(xfer_mem, relinquish_msg->hdl);
	else
		pr_err("%s: transferred memory with obj_id 0x%x not found\n",
		       __func__, relinquish_msg->obj_id);

	resp_msg.hdr.txn_id = relinquish_msg->hdr.txn_id;
	resp_msg.hdr.msg_type = MEM_BUF_ALLOC_RELINQUISH_RESP;

	trace_send_relinquish_resp_msg(&resp_msg);
	ret = gh_msgq_send(mem_buf_gh_msgq_hdl, &resp_msg, sizeof(resp_msg), 0);
	if (ret < 0)
		pr_err("%s failed to send memory relinquish resp message rc: %d\n",
		       __func__, ret);
	else
		pr_debug("%s: relinquish resp message sent\n", __func__);

	kfree(rmt_msg->msg);
	kfree(rmt_msg);
}

static int mem_buf_decode_alloc_resp(void *buf, size_t size,
				     struct mem_buf_desc *membuf)
{
	struct mem_buf_alloc_resp *alloc_resp = buf;

	if (size != sizeof(*alloc_resp)) {
		pr_err("%s response received is not of correct size\n",
		       __func__);
		return -EINVAL;
	}

	trace_receive_alloc_resp_msg(alloc_resp);
	if (alloc_resp->ret < 0) {
		pr_err("%s remote allocation failed rc: %d\n", __func__,
		       alloc_resp->ret);
	} else {
		membuf->memparcel_hdl = alloc_resp->hdl;
		membuf->gh_rm_trans_type = alloc_resp->gh_rm_trans_type;
		membuf->obj_id = alloc_resp->obj_id;
	}

	return alloc_resp->ret;
}

static void __mem_buf_relinquish_mem(int obj_id, u32 memparcel_hdl);

static void mem_buf_process_alloc_resp(struct mem_buf_msg_hdr *hdr, void *buf,
				       size_t size)
{
	struct mem_buf_txn *txn;

	mutex_lock(&mem_buf_idr_mutex);
	txn = idr_find(&mem_buf_txn_idr, hdr->txn_id);
	if (!txn) {
		struct mem_buf_desc tmp;

		pr_err("%s no txn associated with id: %d\n", __func__,
		       hdr->txn_id);
		/*
		 * If this was a legitimate allocation, we should let the
		 * allocator know that the memory is not in use, so that
		 * it can be reclaimed.
		 */
		if (!mem_buf_decode_alloc_resp(buf, size, &tmp))
			__mem_buf_relinquish_mem(tmp.obj_id, tmp.memparcel_hdl);
	} else {
		txn->txn_ret = mem_buf_decode_alloc_resp(buf, size,
							 txn->resp_buf);
		complete(&txn->txn_done);
	}
	mutex_unlock(&mem_buf_idr_mutex);
}

static void mem_buf_process_relinquish_resp(struct mem_buf_msg_hdr *hdr,
					    void *buf, size_t size)
{
	struct mem_buf_txn *txn;
	struct mem_buf_alloc_relinquish *relinquish_resp_msg = buf;

	if (size != sizeof(*relinquish_resp_msg)) {
		pr_err("%s response received is not of correct size\n",
		       __func__);
		return;
	}
	trace_receive_relinquish_resp_msg(relinquish_resp_msg);

	mutex_lock(&mem_buf_idr_mutex);
	txn = idr_find(&mem_buf_txn_idr, hdr->txn_id);
	if (!txn)
		pr_err("%s no txn associated with id: %d\n", __func__,
		       hdr->txn_id);
	else
		complete(&txn->txn_done);
	mutex_unlock(&mem_buf_idr_mutex);
}

static void mem_buf_process_msg(void *buf, size_t size)
{
	struct mem_buf_msg_hdr *hdr = buf;
	struct mem_buf_rmt_msg *rmt_msg;
	work_func_t work_fn;

	pr_debug("%s: mem-buf message received\n", __func__);
	if (size < sizeof(*hdr)) {
		pr_err("%s: message received is not of a proper size: 0x%lx\n",
		       __func__, size);
		kfree(buf);
		return;
	}

	if ((hdr->msg_type == MEM_BUF_ALLOC_RESP) &&
	    (mem_buf_capability & MEM_BUF_CAP_CONSUMER)) {
		mem_buf_process_alloc_resp(hdr, buf, size);
		kfree(buf);
	} else if ((hdr->msg_type == MEM_BUF_ALLOC_REQ ||
		    hdr->msg_type == MEM_BUF_ALLOC_RELINQUISH) &&
		   (mem_buf_capability & MEM_BUF_CAP_SUPPLIER)) {
		rmt_msg = kmalloc(sizeof(*rmt_msg), GFP_KERNEL);
		if (!rmt_msg) {
			kfree(buf);
			return;
		}
		rmt_msg->msg = buf;
		rmt_msg->msg_size = size;
		work_fn = hdr->msg_type == MEM_BUF_ALLOC_REQ ?
			mem_buf_alloc_req_work : mem_buf_relinquish_work;
		INIT_WORK(&rmt_msg->work, work_fn);
		queue_work(mem_buf_wq, &rmt_msg->work);
	} else if ((hdr->msg_type == MEM_BUF_ALLOC_RELINQUISH_RESP) &&
		   (mem_buf_capability & MEM_BUF_CAP_CONSUMER)) {
		mem_buf_process_relinquish_resp(hdr, buf, size);
		kfree(buf);
	} else {
		pr_err("%s: received message of unknown type: %d\n", __func__,
		       hdr->msg_type);
		kfree(buf);
		return;
	}
}

static int mem_buf_msgq_recv_fn(void *unused)
{
	void *buf;
	size_t size;
	int ret;

	while (!kthread_should_stop()) {
		buf = kzalloc(GH_MSGQ_MAX_MSG_SIZE_BYTES, GFP_KERNEL);
		if (!buf)
			continue;

		ret = gh_msgq_recv(mem_buf_gh_msgq_hdl, buf,
					GH_MSGQ_MAX_MSG_SIZE_BYTES, &size, 0);
		if (ret < 0) {
			kfree(buf);
			pr_err_ratelimited("%s failed to receive message rc: %d\n",
					   __func__, ret);
		} else {
			mem_buf_process_msg(buf, size);
		}
	}

	return 0;
}

/* Functions invoked when treating allocation requests to other VMs. */
static size_t mem_buf_get_mem_type_alloc_req_size(enum mem_buf_mem_type type)
{
	if (type == MEM_BUF_DMAHEAP_MEM_TYPE)
		return MEM_BUF_MAX_DMAHEAP_NAME_LEN;

	return 0;
}

static void mem_buf_populate_alloc_req_arb_payload(void *dst, void *src,
						   enum mem_buf_mem_type type)
{
	if (type == MEM_BUF_DMAHEAP_MEM_TYPE)
		strlcpy(dst, src, MEM_BUF_MAX_DMAHEAP_NAME_LEN);
}

static void *mem_buf_construct_alloc_req(struct mem_buf_desc *membuf,
					 int txn_id, size_t *msg_size_ptr)
{
	size_t tot_size, alloc_req_size, acl_desc_size;
	void *req_buf, *arb_payload;
	unsigned int nr_acl_entries = membuf->acl_desc->n_acl_entries;
	struct mem_buf_alloc_req *req;

	alloc_req_size = offsetof(struct mem_buf_alloc_req,
				  acl_desc.acl_entries[nr_acl_entries]);
	tot_size = alloc_req_size +
		   mem_buf_get_mem_type_alloc_req_size(membuf->src_mem_type);

	req_buf = kzalloc(tot_size, GFP_KERNEL);
	if (!req_buf)
		return ERR_PTR(-ENOMEM);

	*msg_size_ptr = tot_size;

	req = req_buf;
	req->hdr.txn_id = txn_id;
	req->hdr.msg_type = MEM_BUF_ALLOC_REQ;
	req->size = membuf->size;
	req->src_mem_type = membuf->src_mem_type;
	acl_desc_size = offsetof(struct gh_acl_desc,
				 acl_entries[nr_acl_entries]);
	memcpy(&req->acl_desc, membuf->acl_desc, acl_desc_size);

	arb_payload = req_buf + alloc_req_size;
	mem_buf_populate_alloc_req_arb_payload(arb_payload, membuf->src_data,
					       membuf->src_mem_type);

	trace_send_alloc_req(req);
	return req_buf;
}

static int mem_buf_request_mem(struct mem_buf_desc *membuf)
{
	struct mem_buf_txn txn;
	void *alloc_req_msg;
	size_t msg_size;
	int ret;

	ret = mem_buf_init_txn(&txn, membuf);
	if (ret)
		return ret;

	alloc_req_msg = mem_buf_construct_alloc_req(membuf, txn.txn_id,
						    &msg_size);
	if (IS_ERR(alloc_req_msg)) {
		ret = PTR_ERR(alloc_req_msg);
		goto out;
	}

	ret = mem_buf_msg_send(alloc_req_msg, msg_size);

	/*
	 * Free the buffer regardless of the return value as the hypervisor
	 * would have consumed the data in the case of a success.
	 */
	kfree(alloc_req_msg);

	if (ret < 0)
		goto out;

	ret = mem_buf_txn_wait(&txn);
	if (ret < 0)
		goto out;

out:
	mem_buf_destroy_txn(&txn);
	return ret;
}

static void __mem_buf_relinquish_mem(int obj_id, gh_memparcel_handle_t memparcel_hdl)
{
	struct mem_buf_alloc_relinquish *msg;
	struct mem_buf_txn txn;
	int ret;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg)
		return;

	if (mem_buf_init_txn(&txn, NULL) < 0) {
		kfree(msg);
		return;
	}

	msg->hdr.txn_id = txn.txn_id;
	msg->hdr.msg_type = MEM_BUF_ALLOC_RELINQUISH;
	msg->hdl = memparcel_hdl;
	msg->obj_id = obj_id;

	trace_send_relinquish_msg(msg);
	ret = gh_msgq_send(mem_buf_gh_msgq_hdl, msg, sizeof(*msg), 0);

	/*
	 * Free the buffer regardless of the return value as the hypervisor
	 * would have consumed the data in the case of a success.
	 */
	kfree(msg);

	if (ret < 0) {
		pr_err("%s failed to send memory relinquish message rc: %d\n",
		       __func__, ret);
		goto out;
	} else {
		pr_debug("%s: allocation relinquish message sent\n", __func__);
	}

	mem_buf_txn_wait(&txn);
out:
	mem_buf_destroy_txn(&txn);
}

/*
 * Check if membuf already has a valid handle. If it doesn't, then create one.
 */
static void mem_buf_relinquish_mem(struct mem_buf_desc *membuf)
{
	int ret;
	int vmids[] = {VMID_HLOS};
	int perms[] = {PERM_READ | PERM_WRITE | PERM_EXEC};
	struct sg_table *sgt;
	struct mem_buf_lend_kernel_arg arg;

	if (membuf->memparcel_hdl != MEM_BUF_MEMPARCEL_INVALID) {
		if (membuf->gh_rm_trans_type != GH_RM_TRANS_TYPE_DONATE) {
			ret = mem_buf_unmap_mem_s2(membuf->memparcel_hdl);
			if (ret)
				return;
		}

		return __mem_buf_relinquish_mem(membuf->obj_id,
						membuf->memparcel_hdl);
	}

	sgt = dup_gh_sgl_desc_to_sgt(membuf->sgl_desc);
	if (IS_ERR(sgt))
		return;

	arg.nr_acl_entries = 1;
	arg.vmids = vmids;
	arg.perms = perms;
	arg.flags = GH_RM_MEM_DONATE_SANITIZE;
	arg.label = 0;

	ret = mem_buf_assign_mem(GH_RM_TRANS_TYPE_DONATE, sgt, &arg);
	if (ret)
		goto err_free_sgt;

	membuf->memparcel_hdl = arg.memparcel_hdl;
	__mem_buf_relinquish_mem(membuf->obj_id, membuf->memparcel_hdl);
err_free_sgt:
	sg_free_table(sgt);
	kfree(sgt);
}

static int get_mem_buf(void *membuf_desc);
static int mem_buf_add_dmaheap_mem(struct mem_buf_desc *membuf, struct sg_table *sgt,
				void *dst_data)
{
	char *heap_name = dst_data;

	return carveout_heap_add_memory(heap_name, sgt, (void *)membuf,
					get_mem_buf, mem_buf_put);
}

static int mem_buf_add_mem_type(struct mem_buf_desc *membuf, enum mem_buf_mem_type type,
				void *dst_data, struct sg_table *sgt)
{
	if (type == MEM_BUF_DMAHEAP_MEM_TYPE)
		return mem_buf_add_dmaheap_mem(membuf, sgt, dst_data);

	return -EINVAL;
}

static int mem_buf_add_mem(struct mem_buf_desc *membuf)
{
	int i, ret, nr_sgl_entries;
	struct sg_table sgt;
	struct scatterlist *sgl;
	u64 base, size;

	pr_debug("%s: adding memory to destination\n", __func__);
	nr_sgl_entries = membuf->sgl_desc->n_sgl_entries;
	ret = sg_alloc_table(&sgt, nr_sgl_entries, GFP_KERNEL);
	if (ret)
		return ret;

	for_each_sg(sgt.sgl, sgl, nr_sgl_entries, i) {
		base = membuf->sgl_desc->sgl_entries[i].ipa_base;
		size = membuf->sgl_desc->sgl_entries[i].size;
		sg_set_page(sgl, phys_to_page(base), size, 0);
	}

	ret = mem_buf_add_mem_type(membuf, membuf->dst_mem_type, membuf->dst_data,
				   &sgt);
	if (ret)
		pr_err("%s failed to add memory to destination rc: %d\n",
		       __func__, ret);
	else
		pr_debug("%s: memory added to destination\n", __func__);

	sg_free_table(&sgt);
	return ret;
}

static int mem_buf_remove_dmaheap_mem(struct sg_table *sgt, void *dst_data)
{
	char *heap_name = dst_data;

	return carveout_heap_remove_memory(heap_name, sgt);
}

static int mem_buf_remove_mem_type(enum mem_buf_mem_type type, void *dst_data,
				   struct sg_table *sgt)
{
	if (type == MEM_BUF_DMAHEAP_MEM_TYPE)
		return mem_buf_remove_dmaheap_mem(sgt, dst_data);

	return -EINVAL;
}

static int mem_buf_remove_mem(struct mem_buf_desc *membuf)
{
	int i, ret, nr_sgl_entries;
	struct sg_table sgt;
	struct scatterlist *sgl;
	u64 base, size;

	pr_debug("%s: removing memory from destination\n", __func__);
	nr_sgl_entries = membuf->sgl_desc->n_sgl_entries;
	ret = sg_alloc_table(&sgt, nr_sgl_entries, GFP_KERNEL);
	if (ret)
		return ret;

	for_each_sg(sgt.sgl, sgl, nr_sgl_entries, i) {
		base = membuf->sgl_desc->sgl_entries[i].ipa_base;
		size = membuf->sgl_desc->sgl_entries[i].size;
		sg_set_page(sgl, phys_to_page(base), size, 0);
	}

	ret = mem_buf_remove_mem_type(membuf->dst_mem_type, membuf->dst_data,
				      &sgt);
	if (ret)
		pr_err("%s failed to remove memory from destination rc: %d\n",
		       __func__, ret);
	else
		pr_debug("%s: memory removed from destination\n", __func__);

	sg_free_table(&sgt);
	return ret;
}

static bool is_valid_mem_buf_vmid(u32 mem_buf_vmid)
{
	if ((mem_buf_vmid == MEM_BUF_VMID_PRIMARY_VM) ||
	    (mem_buf_vmid == MEM_BUF_VMID_TRUSTED_VM))
		return true;

	pr_err_ratelimited("%s: Invalid mem-buf VMID detected\n", __func__);
	return false;
}

static bool is_valid_mem_buf_perms(u32 mem_buf_perms)
{
	if (mem_buf_perms & ~MEM_BUF_PERM_VALID_FLAGS) {
		pr_err_ratelimited("%s: Invalid mem-buf permissions detected\n",
				   __func__);
		return false;
	}
	return true;
}

static int mem_buf_vmid_to_vmid(u32 mem_buf_vmid)
{
	int ret;
	gh_vmid_t vmid;
	enum gh_vm_names vm_name;

	if (!is_valid_mem_buf_vmid(mem_buf_vmid))
		return -EINVAL;

	if (mem_buf_vmid == MEM_BUF_VMID_PRIMARY_VM)
		vm_name = GH_PRIMARY_VM;
	else if (mem_buf_vmid == MEM_BUF_VMID_TRUSTED_VM)
		vm_name = GH_TRUSTED_VM;
	else
		return -EINVAL;

	ret = gh_rm_get_vmid(vm_name, &vmid);
	if (!ret)
		return vmid;
	return ret;
}

static int mem_buf_perms_to_perms(u32 mem_buf_perms)
{
	int perms = 0;

	if (!is_valid_mem_buf_perms(mem_buf_perms))
		return -EINVAL;

	if (mem_buf_perms & MEM_BUF_PERM_FLAG_READ)
		perms |= PERM_READ;
	if (mem_buf_perms & MEM_BUF_PERM_FLAG_WRITE)
		perms |= PERM_WRITE;
	if (mem_buf_perms & MEM_BUF_PERM_FLAG_EXEC)
		perms |= PERM_EXEC;

	return perms;
}

static void *mem_buf_retrieve_dmaheap_mem_type_data_user(
				struct mem_buf_dmaheap_data __user *udata)
{
	char *buf;
	int ret;
	struct mem_buf_dmaheap_data data;

	ret = copy_struct_from_user(&data, sizeof(data),
				    udata,
				    sizeof(data));
	if (ret)
		return ERR_PTR(-EINVAL);

	buf = kcalloc(MEM_BUF_MAX_DMAHEAP_NAME_LEN, sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	ret = strncpy_from_user(buf, (const void __user *)data.heap_name,
			MEM_BUF_MAX_DMAHEAP_NAME_LEN);
	if (ret < 0 || ret == MEM_BUF_MAX_DMAHEAP_NAME_LEN) {
		kfree(buf);
		return ERR_PTR(-EINVAL);
	}
	return buf;
}

static void *mem_buf_retrieve_mem_type_data_user(enum mem_buf_mem_type mem_type,
						 void __user *mem_type_data)
{
	void *data = ERR_PTR(-EINVAL);

	if (mem_type == MEM_BUF_DMAHEAP_MEM_TYPE)
		data = mem_buf_retrieve_dmaheap_mem_type_data_user(mem_type_data);

	return data;
}

static void *mem_buf_retrieve_dmaheap_mem_type_data(char *dmaheap_name)
{
	return kstrdup(dmaheap_name, GFP_KERNEL);
}

static void *mem_buf_retrieve_mem_type_data(enum mem_buf_mem_type mem_type,
					    void *mem_type_data)
{
	void *data = ERR_PTR(-EINVAL);

	if (mem_type == MEM_BUF_DMAHEAP_MEM_TYPE)
		data = mem_buf_retrieve_dmaheap_mem_type_data(mem_type_data);

	return data;
}

static void mem_buf_free_dmaheap_mem_type_data(char *dmaheap_name)
{
	kfree(dmaheap_name);
}

static void mem_buf_free_mem_type_data(enum mem_buf_mem_type mem_type,
				       void *mem_type_data)
{
	if (mem_type == MEM_BUF_DMAHEAP_MEM_TYPE)
		mem_buf_free_dmaheap_mem_type_data(mem_type_data);
}

static int mem_buf_buffer_release(struct inode *inode, struct file *filp)
{
	struct mem_buf_desc *membuf = filp->private_data;
	int ret;

	mutex_lock(&mem_buf_list_lock);
	list_del(&membuf->entry);
	mutex_unlock(&mem_buf_list_lock);

	pr_debug("%s: Destroying tui carveout\n", __func__);
	if (membuf->add_mem_complete) {
		ret = mem_buf_remove_mem(membuf);
		if (ret < 0)
			goto out_free_mem;
	}

	ret = mem_buf_unmap_mem_s1(membuf->sgl_desc);
	if (ret < 0)
		goto out_free_mem;

	mem_buf_relinquish_mem(membuf);

out_free_mem:
	mem_buf_free_mem_type_data(membuf->dst_mem_type, membuf->dst_data);
	mem_buf_free_mem_type_data(membuf->src_mem_type, membuf->src_data);
	kfree(membuf->sgl_desc);
	kfree(membuf->acl_desc);
	kfree(membuf);
	return ret;
}

static const struct file_operations mem_buf_fops = {
	.release = mem_buf_buffer_release,
};

static bool is_valid_mem_type(enum mem_buf_mem_type mem_type)
{
	return mem_type == MEM_BUF_DMAHEAP_MEM_TYPE;
}

static void *mem_buf_alloc(struct mem_buf_allocation_data *alloc_data)
{
	int ret;
	struct file *filp;
	struct mem_buf_desc *membuf;
	struct gh_sgl_desc *sgl_desc;
	int op;
	int perms = PERM_READ | PERM_WRITE | PERM_EXEC;

	if (!(mem_buf_capability & MEM_BUF_CAP_CONSUMER))
		return ERR_PTR(-EOPNOTSUPP);

	if (!alloc_data || !alloc_data->size || alloc_data->nr_acl_entries != 1 ||
	    !alloc_data->vmids || !alloc_data->perms ||
	    !is_valid_mem_type(alloc_data->src_mem_type) ||
	    !is_valid_mem_type(alloc_data->dst_mem_type) ||
	    mem_buf_check_vmids(alloc_data->vmids, alloc_data->nr_acl_entries))
		return ERR_PTR(-EINVAL);

	membuf = kzalloc(sizeof(*membuf), GFP_KERNEL);
	if (!membuf)
		return ERR_PTR(-ENOMEM);

	pr_debug("%s: mem buf alloc begin\n", __func__);
	membuf->size = ALIGN(alloc_data->size, MEM_BUF_MHP_ALIGNMENT);
	membuf->acl_desc = mem_buf_vmid_perm_list_to_gh_acl(
				alloc_data->vmids, &perms,
				alloc_data->nr_acl_entries);
	if (IS_ERR(membuf->acl_desc)) {
		ret = PTR_ERR(membuf->acl_desc);
		goto err_alloc_acl_list;
	}
	membuf->src_mem_type = alloc_data->src_mem_type;
	membuf->dst_mem_type = alloc_data->dst_mem_type;

	membuf->src_data =
		mem_buf_retrieve_mem_type_data(alloc_data->src_mem_type,
					       alloc_data->src_data);
	if (IS_ERR(membuf->src_data)) {
		ret = PTR_ERR(membuf->src_data);
		goto err_alloc_src_data;
	}

	membuf->dst_data =
		mem_buf_retrieve_mem_type_data(alloc_data->dst_mem_type,
					       alloc_data->dst_data);
	if (IS_ERR(membuf->dst_data)) {
		ret = PTR_ERR(membuf->dst_data);
		goto err_alloc_dst_data;
	}

	trace_mem_buf_alloc_info(membuf->size, membuf->src_mem_type,
				 membuf->dst_mem_type, membuf->acl_desc);
	ret = mem_buf_request_mem(membuf);
	if (ret)
		goto err_mem_req;

	op = membuf->gh_rm_trans_type;
	sgl_desc = mem_buf_map_mem_s2(op, &membuf->memparcel_hdl, membuf->acl_desc, VMID_HLOS);
	if (IS_ERR(sgl_desc)) {
		ret = PTR_ERR(sgl_desc);
		goto err_map_mem_s2;
	}
	membuf->sgl_desc = sgl_desc;

	ret = mem_buf_map_mem_s1(membuf->sgl_desc);
	if (ret)
		goto err_map_mem_s1;

	filp = anon_inode_getfile("membuf", &mem_buf_fops, membuf, O_RDWR);
	if (IS_ERR(filp)) {
		ret = PTR_ERR(filp);
		goto err_get_file;
	}
	membuf->filp = filp;

	mutex_lock(&mem_buf_list_lock);
	list_add_tail(&membuf->entry, &mem_buf_list);
	mutex_unlock(&mem_buf_list_lock);

	ret = mem_buf_add_mem(membuf);
	if (ret)
		goto err_add_mem;

	membuf->add_mem_complete = true;
	pr_debug("%s: mem buf alloc success\n", __func__);
	return membuf;

err_add_mem:
	fput(filp);
	return ERR_PTR(ret);
err_get_file:
	if (mem_buf_unmap_mem_s1(membuf->sgl_desc) < 0) {
		kfree(membuf->sgl_desc);
		goto err_mem_req;
	}
err_map_mem_s1:
err_map_mem_s2:
	mem_buf_relinquish_mem(membuf);
	kfree(membuf->sgl_desc);
err_mem_req:
	mem_buf_free_mem_type_data(membuf->dst_mem_type, membuf->dst_data);
err_alloc_dst_data:
	mem_buf_free_mem_type_data(membuf->src_mem_type, membuf->src_data);
err_alloc_src_data:
	kfree(membuf->acl_desc);
err_alloc_acl_list:
	kfree(membuf);
	return ERR_PTR(ret);
}

static int _mem_buf_get_fd(struct file *filp)
{
	int fd;

	if (!filp)
		return -EINVAL;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0)
		return fd;

	fd_install(fd, filp);
	return fd;
}

int mem_buf_get_fd(void *membuf_desc)
{
	struct mem_buf_desc *membuf = membuf_desc;

	if (!membuf_desc)
		return -EINVAL;

	return _mem_buf_get_fd(membuf->filp);
}
EXPORT_SYMBOL(mem_buf_get_fd);

static void _mem_buf_put(struct file *filp)
{
	fput(filp);
}

void mem_buf_put(void *membuf_desc)
{
	struct mem_buf_desc *membuf = membuf_desc;

	if (membuf && membuf->filp)
		_mem_buf_put(membuf->filp);
}
EXPORT_SYMBOL(mem_buf_put);

static bool is_mem_buf_file(struct file *filp)
{
	return filp->f_op == &mem_buf_fops;
}

void *mem_buf_get(int fd)
{
	struct file *filp;

	filp = fget(fd);

	if (!filp)
		return ERR_PTR(-EBADF);

	if (!is_mem_buf_file(filp)) {
		fput(filp);
		return ERR_PTR(-EINVAL);
	}

	return filp->private_data;
}
EXPORT_SYMBOL(mem_buf_get);

static int get_mem_buf(void *membuf_desc)
{
	struct mem_buf_desc *membuf = membuf_desc;

	/*
	 * get_file_rcu used for atomic_long_inc_unless_zero.
	 * It returns 0 if file count is already zero.
	 */
	if (!get_file_rcu(membuf->filp))
		return -EINVAL;

	return 0;
}

static void mem_buf_retrieve_release(struct qcom_sg_buffer *buffer)
{
	sg_free_table(&buffer->sg_table);
	kfree(buffer);
}

struct dma_buf *mem_buf_retrieve(struct mem_buf_retrieve_kernel_arg *arg)
{
	int ret, op;
	struct qcom_sg_buffer *buffer;
	struct gh_acl_desc *acl_desc;
	struct gh_sgl_desc *sgl_desc;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;
	struct sg_table *sgt;

	if (arg->fd_flags & ~MEM_BUF_VALID_FD_FLAGS)
		return ERR_PTR(-EINVAL);

	if (!arg->nr_acl_entries || !arg->vmids || !arg->perms)
		return ERR_PTR(-EINVAL);

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	acl_desc = mem_buf_vmid_perm_list_to_gh_acl(arg->vmids, arg->perms,
				arg->nr_acl_entries);
	if (IS_ERR(acl_desc)) {
		ret = PTR_ERR(acl_desc);
		goto err_gh_acl;
	}

	op = mem_buf_get_mem_xfer_type_gh(acl_desc, arg->sender_vmid);
	sgl_desc = mem_buf_map_mem_s2(op, &arg->memparcel_hdl, acl_desc,
					arg->sender_vmid);
	if (IS_ERR(sgl_desc)) {
		ret = PTR_ERR(sgl_desc);
		goto err_map_s2;
	}

	ret = mem_buf_map_mem_s1(sgl_desc);
	if (ret < 0)
		goto err_map_mem_s1;

	sgt = dup_gh_sgl_desc_to_sgt(sgl_desc);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		goto err_dup_sgt;
	}
	buffer->sg_table = *sgt;
	kfree(sgt);

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->heap = NULL;
	buffer->len = mem_buf_get_sgl_buf_size(sgl_desc);
	buffer->uncached = false;
	buffer->free = mem_buf_retrieve_release;
	buffer->vmperm = mem_buf_vmperm_alloc_accept(&buffer->sg_table,
						     arg->memparcel_hdl);

	exp_info.size = buffer->len;
	exp_info.flags = arg->fd_flags;
	exp_info.priv = buffer;

	dmabuf = mem_buf_dma_buf_export(&exp_info, &qcom_sg_buf_ops);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto err_export_dma_buf;
	}

	/* sgt & qcom_sg_buffer will be freed by mem_buf_retrieve_release */
	kfree(sgl_desc);
	kfree(acl_desc);
	return dmabuf;

err_export_dma_buf:
	sg_free_table(&buffer->sg_table);
err_dup_sgt:
	mem_buf_unmap_mem_s1(sgl_desc);
err_map_mem_s1:
	kfree(sgl_desc);
	mem_buf_unmap_mem_s2(arg->memparcel_hdl);
err_map_s2:
	kfree(acl_desc);
err_gh_acl:
	kfree(buffer);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(mem_buf_retrieve);

/* Userspace machinery */
static int mem_buf_prep_alloc_data(struct mem_buf_allocation_data *alloc_data,
				struct mem_buf_alloc_ioctl_arg *allocation_args)
{
	unsigned int nr_acl_entries = allocation_args->nr_acl_entries;
	int ret;

	alloc_data->size = allocation_args->size;
	alloc_data->nr_acl_entries = nr_acl_entries;

	ret = mem_buf_acl_to_vmid_perms_list(nr_acl_entries,
				(const void __user *)allocation_args->acl_list,
				&alloc_data->vmids, &alloc_data->perms, true);
	if (ret)
		goto err_acl;

	alloc_data->src_mem_type = allocation_args->src_mem_type;
	alloc_data->dst_mem_type = allocation_args->dst_mem_type;

	alloc_data->src_data =
		mem_buf_retrieve_mem_type_data_user(
						allocation_args->src_mem_type,
				(void __user *)allocation_args->src_data);
	if (IS_ERR(alloc_data->src_data)) {
		ret = PTR_ERR(alloc_data->src_data);
		goto err_alloc_src_data;
	}

	alloc_data->dst_data =
		mem_buf_retrieve_mem_type_data_user(
						allocation_args->dst_mem_type,
				(void __user *)allocation_args->dst_data);
	if (IS_ERR(alloc_data->dst_data)) {
		ret = PTR_ERR(alloc_data->dst_data);
		goto err_alloc_dst_data;
	}

	return 0;

err_alloc_dst_data:
	mem_buf_free_mem_type_data(alloc_data->src_mem_type,
				   alloc_data->src_data);
err_alloc_src_data:
	kfree(alloc_data->vmids);
	kfree(alloc_data->perms);
err_acl:
	return ret;
}

static void mem_buf_free_alloc_data(struct mem_buf_allocation_data *alloc_data)
{
	mem_buf_free_mem_type_data(alloc_data->dst_mem_type,
				   alloc_data->dst_data);
	mem_buf_free_mem_type_data(alloc_data->src_mem_type,
				   alloc_data->src_data);
	kfree(alloc_data->vmids);
	kfree(alloc_data->perms);
}

static int mem_buf_alloc_fd(struct mem_buf_alloc_ioctl_arg *allocation_args)
{
	struct mem_buf_desc *membuf;
	struct mem_buf_allocation_data alloc_data;
	int ret;

	if (!allocation_args->size || !allocation_args->nr_acl_entries ||
	    !allocation_args->acl_list ||
	    (allocation_args->nr_acl_entries > MEM_BUF_MAX_NR_ACL_ENTS) ||
	    !is_valid_mem_type(allocation_args->src_mem_type) ||
	    !is_valid_mem_type(allocation_args->dst_mem_type) ||
	    allocation_args->reserved0 || allocation_args->reserved1 ||
	    allocation_args->reserved2)
		return -EINVAL;

	ret = mem_buf_prep_alloc_data(&alloc_data, allocation_args);
	if (ret < 0)
		return ret;

	membuf = mem_buf_alloc(&alloc_data);
	if (IS_ERR(membuf)) {
		ret = PTR_ERR(membuf);
		goto out;
	}

	ret = mem_buf_get_fd(membuf);
	if (ret < 0)
		mem_buf_put(membuf);

out:
	mem_buf_free_alloc_data(&alloc_data);
	return ret;
}

union mem_buf_ioctl_arg {
	struct mem_buf_alloc_ioctl_arg allocation;
	struct mem_buf_lend_ioctl_arg lend;
	struct mem_buf_retrieve_ioctl_arg retrieve;
	struct mem_buf_reclaim_ioctl_arg reclaim;
	struct mem_buf_share_ioctl_arg share;
	struct mem_buf_exclusive_owner_ioctl_arg get_ownership;
};

static int mem_buf_acl_to_vmid_perms_list(unsigned int nr_acl_entries,
					  const void __user *acl_entries,
					  int **dst_vmids, int **dst_perms,
					  bool lookup_fd)
{
	int ret, i, *vmids, *perms;
	struct acl_entry entry;

	if (!nr_acl_entries || !acl_entries)
		return -EINVAL;

	vmids = kmalloc_array(nr_acl_entries, sizeof(*vmids), GFP_KERNEL);
	if (!vmids)
		return -ENOMEM;

	perms = kmalloc_array(nr_acl_entries, sizeof(*perms), GFP_KERNEL);
	if (!perms) {
		kfree(vmids);
		return -ENOMEM;
	}

	for (i = 0; i < nr_acl_entries; i++) {
		ret = copy_struct_from_user(&entry, sizeof(entry),
					    acl_entries + (sizeof(entry) * i),
					    sizeof(entry));
		if (ret < 0)
			goto out;

		if (lookup_fd)
			vmids[i] = mem_buf_fd_to_vmid(entry.vmid);
		else
			vmids[i] = mem_buf_vmid_to_vmid(entry.vmid);
		perms[i] = mem_buf_perms_to_perms(entry.perms);
		if (vmids[i] < 0 || perms[i] < 0) {
			ret = -EINVAL;
			goto out;
		}
	}

	*dst_vmids = vmids;
	*dst_perms = perms;
	return ret;

out:
	kfree(perms);
	kfree(vmids);
	return ret;
}

static size_t mem_buf_get_sgl_buf_size(struct gh_sgl_desc *sgl_desc)
{
	size_t size = 0;
	unsigned int i;

	for (i = 0; i < sgl_desc->n_sgl_entries; i++)
		size += sgl_desc->sgl_entries[i].size;

	return size;
}

static int mem_buf_lend_user(struct mem_buf_lend_ioctl_arg *uarg, bool is_lend)
{
	int *vmids, *perms;
	int ret;
	struct dma_buf *dmabuf;
	struct mem_buf_lend_kernel_arg karg = {0};

	if (!uarg->nr_acl_entries || !uarg->acl_list ||
	    uarg->nr_acl_entries > MEM_BUF_MAX_NR_ACL_ENTS ||
	    uarg->reserved0 || uarg->reserved1 || uarg->reserved2)
		return -EINVAL;

	dmabuf = dma_buf_get(uarg->dma_buf_fd);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	ret = mem_buf_acl_to_vmid_perms_list(uarg->nr_acl_entries,
			(void *)uarg->acl_list, &vmids, &perms, true);
	if (ret)
		goto err_acl;

	karg.nr_acl_entries = uarg->nr_acl_entries;
	karg.vmids = vmids;
	karg.perms = perms;

	if (is_lend) {
		ret = mem_buf_lend(dmabuf, &karg);
		if (ret)
			goto err_lend;
	} else {
		ret = mem_buf_share(dmabuf, &karg);
		if (ret)
			goto err_lend;
	}

	uarg->memparcel_hdl = karg.memparcel_hdl;
err_lend:
	kfree(perms);
	kfree(vmids);
err_acl:
	dma_buf_put(dmabuf);
	return ret;
}

static int mem_buf_retrieve_user(struct mem_buf_retrieve_ioctl_arg *uarg)
{
	int ret, fd;
	int *vmids, *perms;
	struct dma_buf *dmabuf;
	struct mem_buf_retrieve_kernel_arg karg = {0};

	if (!uarg->nr_acl_entries || !uarg->acl_list ||
	    uarg->nr_acl_entries > MEM_BUF_MAX_NR_ACL_ENTS ||
	    uarg->reserved0 || uarg->reserved1 ||
	    uarg->reserved2 ||
	    uarg->fd_flags & ~MEM_BUF_VALID_FD_FLAGS)
		return -EINVAL;

	ret = mem_buf_acl_to_vmid_perms_list(uarg->nr_acl_entries,
			(void *)uarg->acl_list, &vmids, &perms, true);
	if (ret)
		return ret;

	karg.sender_vmid = mem_buf_fd_to_vmid(uarg->sender_vm_fd);
	if (karg.sender_vmid < 0) {
		pr_err_ratelimited("%s: Invalid sender_vmid %d\n", __func__,
					uarg->sender_vm_fd);
		goto err_sender_vmid;
	}

	karg.nr_acl_entries = uarg->nr_acl_entries;
	karg.vmids = vmids;
	karg.perms = perms;
	karg.memparcel_hdl = uarg->memparcel_hdl;
	karg.fd_flags = uarg->fd_flags;
	dmabuf = mem_buf_retrieve(&karg);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto err_retrieve;
	}

	fd = dma_buf_fd(dmabuf, karg.fd_flags);
	if (fd < 0) {
		ret = fd;
		goto err_fd;
	}

	uarg->dma_buf_import_fd = fd;
	kfree(vmids);
	kfree(perms);
	return 0;
err_fd:
	dma_buf_put(dmabuf);
err_sender_vmid:
err_retrieve:
	kfree(vmids);
	kfree(perms);
	return ret;
}

static int mem_buf_reclaim_user(struct mem_buf_reclaim_ioctl_arg *uarg)
{
	struct dma_buf *dmabuf;
	int ret;

	if (uarg->reserved0 || uarg->reserved1 || uarg->reserved2)
		return -EINVAL;

	dmabuf = dma_buf_get(uarg->dma_buf_fd);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	ret = mem_buf_reclaim(dmabuf);
	dma_buf_put(dmabuf);
	return ret;
}

static int mem_buf_get_exclusive_ownership(struct mem_buf_exclusive_owner_ioctl_arg *uarg)
{
	struct dma_buf *dmabuf;
	int ret = 0;

	dmabuf = dma_buf_get(uarg->dma_buf_fd);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	if (IS_ERR(to_mem_buf_vmperm(dmabuf))) {
		ret = -EINVAL;
		goto put_dma_buf;
	}

	uarg->is_exclusive_owner = mem_buf_dma_buf_exclusive_owner(dmabuf);

put_dma_buf:
	dma_buf_put(dmabuf);

	return ret;
}

static long mem_buf_dev_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	int fd;
	unsigned int dir = _IOC_DIR(cmd);
	union mem_buf_ioctl_arg ioctl_arg;

	if (_IOC_SIZE(cmd) > sizeof(ioctl_arg))
		return -EINVAL;

	if (copy_from_user(&ioctl_arg, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	if (!(dir & _IOC_WRITE))
		memset(&ioctl_arg, 0, sizeof(ioctl_arg));

	switch (cmd) {
	case MEM_BUF_IOC_ALLOC:
	{
		struct mem_buf_alloc_ioctl_arg *allocation =
			&ioctl_arg.allocation;

		if (!(mem_buf_capability & MEM_BUF_CAP_CONSUMER))
			return -EOPNOTSUPP;

		fd = mem_buf_alloc_fd(allocation);

		if (fd < 0)
			return fd;

		allocation->mem_buf_fd = fd;
		break;
	}
	case MEM_BUF_IOC_LEND:
	{
		struct mem_buf_lend_ioctl_arg *lend = &ioctl_arg.lend;
		int ret;

		ret = mem_buf_lend_user(lend, true);
		if (ret)
			return ret;

		break;
	}
	case MEM_BUF_IOC_RETRIEVE:
	{
		struct mem_buf_retrieve_ioctl_arg *retrieve =
			&ioctl_arg.retrieve;
		int ret;

		ret = mem_buf_retrieve_user(retrieve);
		if (ret)
			return ret;
		break;
	}
	case MEM_BUF_IOC_RECLAIM:
	{
		struct mem_buf_reclaim_ioctl_arg *reclaim =
			&ioctl_arg.reclaim;
		int ret;

		ret = mem_buf_reclaim_user(reclaim);
		if (ret)
			return ret;
		break;
	}
	case MEM_BUF_IOC_SHARE:
	{
		struct mem_buf_share_ioctl_arg *share = &ioctl_arg.share;
		int ret;

		/* The two formats are currently identical */
		ret = mem_buf_lend_user((struct mem_buf_lend_ioctl_arg *)share,
					 false);
		if (ret)
			return ret;

		break;
	}
	case MEM_BUF_IOC_EXCLUSIVE_OWNER:
	{
		struct mem_buf_exclusive_owner_ioctl_arg *get_ownership = &ioctl_arg.get_ownership;
		int ret;

		ret = mem_buf_get_exclusive_ownership(get_ownership);
		if (ret)
			return ret;

		break;
	}

	default:
		return -ENOTTY;
	}

	if (dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &ioctl_arg,
				 _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	return 0;
}

static const struct file_operations mem_buf_dev_fops = {
	.unlocked_ioctl = mem_buf_dev_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};

/*
 * The msgq needs to live in the same module as the ioctl handling code because it
 * directly calls into mem_buf_process_alloc_resp without using a function
 * pointer. Ideally msgq would support a client registration API which would
 * associated a 'struct mem_buf_msg_hdr->msg_type' with a handler callback.
 */
static int mem_buf_msgq_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device *class_dev;

	if (!mem_buf_dev)
		return -EPROBE_DEFER;

	mem_buf_wq = alloc_workqueue("mem_buf_wq", WQ_HIGHPRI | WQ_UNBOUND, 0);
	if (!mem_buf_wq) {
		dev_err(dev, "Unable to initialize workqueue\n");
		return -EINVAL;
	}

	mem_buf_msgq_recv_thr = kthread_create(mem_buf_msgq_recv_fn, NULL,
					       "mem_buf_rcvr");
	if (IS_ERR(mem_buf_msgq_recv_thr)) {
		dev_err(dev, "Failed to create msgq receiver thread rc: %d\n",
			PTR_ERR(mem_buf_msgq_recv_thr));
		ret = PTR_ERR(mem_buf_msgq_recv_thr);
		goto err_kthread_create;
	}

	mem_buf_gh_msgq_hdl = gh_msgq_register(GH_MSGQ_LABEL_MEMBUF);
	if (IS_ERR(mem_buf_gh_msgq_hdl)) {
		ret = PTR_ERR(mem_buf_gh_msgq_hdl);
		if (ret != -EPROBE_DEFER)
			dev_err(dev,
				"Message queue registration failed: rc: %d\n",
				ret);
		goto err_msgq_register;
	}

	cdev_init(&mem_buf_char_dev, &mem_buf_dev_fops);
	ret = cdev_add(&mem_buf_char_dev, mem_buf_dev_no, MEM_BUF_MAX_DEVS);
	if (ret < 0)
		goto err_cdev_add;

	class_dev = device_create(mem_buf_class, NULL, mem_buf_dev_no, NULL,
				  "membuf");
	if (IS_ERR(class_dev)) {
		ret = PTR_ERR(class_dev);
		goto err_dev_create;
	}

	wake_up_process(mem_buf_msgq_recv_thr);
	return 0;

err_dev_create:
	cdev_del(&mem_buf_char_dev);
err_cdev_add:
	gh_msgq_unregister(mem_buf_gh_msgq_hdl);
	mem_buf_gh_msgq_hdl = NULL;
err_msgq_register:
	kthread_stop(mem_buf_msgq_recv_thr);
	mem_buf_msgq_recv_thr = NULL;
err_kthread_create:
	destroy_workqueue(mem_buf_wq);
	mem_buf_wq = NULL;
	return ret;
}

static int mem_buf_msgq_remove(struct platform_device *pdev)
{
	mutex_lock(&mem_buf_list_lock);
	if (!list_empty(&mem_buf_list))
		dev_err(mem_buf_dev,
			"Removing mem-buf driver while there are membufs\n");
	mutex_unlock(&mem_buf_list_lock);

	mutex_lock(&mem_buf_xfer_mem_list_lock);
	if (!list_empty(&mem_buf_xfer_mem_list))
		dev_err(mem_buf_dev,
			"Removing mem-buf driver while memory is still lent\n");
	mutex_unlock(&mem_buf_xfer_mem_list_lock);

	device_destroy(mem_buf_class, mem_buf_dev_no);
	cdev_del(&mem_buf_char_dev);
	gh_msgq_unregister(mem_buf_gh_msgq_hdl);
	mem_buf_gh_msgq_hdl = NULL;
	kthread_stop(mem_buf_msgq_recv_thr);
	mem_buf_msgq_recv_thr = NULL;
	destroy_workqueue(mem_buf_wq);
	mem_buf_wq = NULL;
	return 0;
}

static const struct of_device_id mem_buf_msgq_match_tbl[] = {
	 {.compatible = "qcom,mem-buf-msgq"},
	 {},
};

static struct platform_driver mem_buf_msgq_driver = {
	.probe = mem_buf_msgq_probe,
	.remove = mem_buf_msgq_remove,
	.driver = {
		.name = "mem-buf-msgq",
		.of_match_table = of_match_ptr(mem_buf_msgq_match_tbl),
	},
};

static int __init mem_buf_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&mem_buf_dev_no, 0, MEM_BUF_MAX_DEVS,
				  "membuf");
	if (ret < 0)
		goto err_chrdev_region;

	mem_buf_class = class_create(THIS_MODULE, "membuf");
	if (IS_ERR(mem_buf_class)) {
		ret = PTR_ERR(mem_buf_class);
		goto err_class_create;
	}

	ret = platform_driver_register(&mem_buf_msgq_driver);
	if (ret < 0)
		goto err_platform_drvr_register;

	return 0;

err_platform_drvr_register:
	class_destroy(mem_buf_class);
err_class_create:
	unregister_chrdev_region(mem_buf_dev_no, MEM_BUF_MAX_DEVS);
err_chrdev_region:
	return ret;
}
module_init(mem_buf_init);

static void __exit mem_buf_exit(void)
{
	platform_driver_unregister(&mem_buf_msgq_driver);
	class_destroy(mem_buf_class);
	unregister_chrdev_region(mem_buf_dev_no, MEM_BUF_MAX_DEVS);
}
module_exit(mem_buf_exit);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Memory Buffer Sharing driver");
MODULE_LICENSE("GPL v2");
