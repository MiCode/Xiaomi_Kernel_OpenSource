// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/anon_inodes.h>
#include <linux/cdev.h>
#include <linux/dma-buf.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/haven/hh_rm_drv.h>
#include <linux/haven/hh_msgq.h>
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

#include <soc/qcom/secure_buffer.h>
#include <uapi/linux/mem-buf.h>

#define MEM_BUF_MAX_DEVS 1
#define MEM_BUF_TIMEOUT_MS 2000
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
static void *mem_buf_hh_msgq_hdl;

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
 * @acl_desc: A HH ACL descriptor that describes the VMIDs that will be
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
	struct hh_acl_desc acl_desc;
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

/* Data structures for maintaining memory shared to other VMs */
static struct device *mem_buf_dev;

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
 * @entry: List entry for maintaining a list of memory buffers that are lent
 * out.
 * @acl_desc: A HH ACL descriptor that describes the VMIDs that have access to
 * the memory, as well as the permissions each VMID has.
 */
struct mem_buf_xfer_mem {
	size_t size;
	enum mem_buf_mem_type mem_type;
	void *mem_type_data;
	struct sg_table *mem_sgt;
	bool secure_alloc;
	hh_memparcel_handle_t hdl;
	struct list_head entry;
	struct hh_acl_desc acl_desc;
};

/**
 * struct mem_buf_desc - Internal data structure, which contains information
 * about a particular memory buffer.
 * @size: The size of the memory buffer
 * @acl_desc: A HH ACL descriptor that describes the VMIDs that have access to
 * the memory, as well as the permissions each VMID has.
 * @sgl_desc: An HH SG-List descriptor that describes the IPAs of the memory
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
 */
struct mem_buf_desc {
	size_t size;
	struct hh_acl_desc *acl_desc;
	struct hh_sgl_desc *sgl_desc;
	hh_memparcel_handle_t memparcel_hdl;
	enum mem_buf_mem_type src_mem_type;
	void *src_data;
	enum mem_buf_mem_type dst_mem_type;
	void *dst_data;
	struct file *filp;
	struct list_head entry;
};

/**
 * struct mem_buf_xfer_ion_mem: Data specific to memory buffers allocated
 * through ION and have been transferred to another VM.
 * @heap_id: ID of the heap from which the allocation was performed from
 * @dmabuf: dmabuf associated with the memory buffer
 * @attachment: dmabuf attachment associated with the memory buffer
 */
struct mem_buf_xfer_ion_mem {
	u32 heap_id;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
};

static int mem_buf_init_txn(struct mem_buf_txn *txn, void *resp_buf)
{
	int ret;

	mutex_lock(&mem_buf_idr_mutex);
	ret = idr_alloc_cyclic(&mem_buf_txn_idr, txn, 0, U16_MAX, GFP_KERNEL);
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

	ret = hh_msgq_send(mem_buf_hh_msgq_hdl, msg, msg_size, 0);
	if (ret < 0)
		pr_err("%s: failed to send allocation request rc: %d\n",
		       __func__, ret);

	return ret;
}

static int mem_buf_txn_wait(struct mem_buf_txn *txn)
{
	int ret;

	ret = wait_for_completion_timeout(&txn->txn_done,
					  msecs_to_jiffies(MEM_BUF_TIMEOUT_MS));
	if (ret == 0)
		return -ETIMEDOUT;

	return txn->txn_ret;
}

static void mem_buf_destroy_txn(struct mem_buf_txn *txn)
{
	mutex_lock(&mem_buf_idr_mutex);
	idr_remove(&mem_buf_txn_idr, txn->txn_id);
	mutex_unlock(&mem_buf_idr_mutex);
}

/* Functions invoked when treating allocation requests from other VMs. */
static int mem_buf_rmt_alloc_ion_mem(struct mem_buf_xfer_mem *xfer_mem)
{
	unsigned int i;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *mem_sgt;
	struct mem_buf_xfer_ion_mem *ion_mem_data = xfer_mem->mem_type_data;
	int ion_flags = 0, flag;
	int heap_id = ion_mem_data->heap_id;
	u32 nr_acl_entries = xfer_mem->acl_desc.n_acl_entries;
	struct hh_acl_entry *acl_entries = xfer_mem->acl_desc.acl_entries;

	if (msm_ion_heap_is_secure(heap_id)) {
		xfer_mem->secure_alloc = true;
		ion_flags |= ION_FLAG_SECURE;
		for (i = 0; i < nr_acl_entries; i++) {
			flag = get_ion_flags(acl_entries[i].vmid);
			if (flag < 0)
				return flag;
			ion_flags |= flag;
		}
	} else {
		xfer_mem->secure_alloc = false;
	}

	dmabuf = ion_alloc(xfer_mem->size, heap_id, ion_flags);
	if (IS_ERR(dmabuf)) {
		pr_err("%s ion_alloc failure sz: 0x%x heap_id: %d flags: 0x%x rc: %d\n",
		       __func__, xfer_mem->size, heap_id, ion_flags,
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

	ion_mem_data->dmabuf = dmabuf;
	ion_mem_data->attachment = attachment;
	xfer_mem->mem_sgt = mem_sgt;

	return 0;
}

static int mem_buf_rmt_alloc_mem(struct mem_buf_xfer_mem *xfer_mem)
{
	int ret = -EINVAL;

	if (xfer_mem->mem_type == MEM_BUF_ION_MEM_TYPE)
		ret = mem_buf_rmt_alloc_ion_mem(xfer_mem);

	return ret;
}

static void mem_buf_rmt_free_ion_mem(struct mem_buf_xfer_mem *xfer_mem)
{
	struct mem_buf_xfer_ion_mem *ion_mem_data = xfer_mem->mem_type_data;
	struct dma_buf *dmabuf = ion_mem_data->dmabuf;
	struct dma_buf_attachment *attachment = ion_mem_data->attachment;
	struct sg_table *mem_sgt = xfer_mem->mem_sgt;

	dma_buf_unmap_attachment(attachment, mem_sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attachment);
	dma_buf_put(ion_mem_data->dmabuf);
}

static void mem_buf_rmt_free_mem(struct mem_buf_xfer_mem *xfer_mem)
{
	if (xfer_mem->mem_type == MEM_BUF_ION_MEM_TYPE)
		mem_buf_rmt_free_ion_mem(xfer_mem);
}

static int mem_buf_hh_acl_desc_to_vmid_perm_list(struct hh_acl_desc *acl_desc,
						 int **vmids, int **perms)
{
	int *vmids_arr = NULL, *perms_arr = NULL;
	u32 nr_acl_entries = acl_desc->n_acl_entries;
	unsigned int i;

	if (!vmids && !perms)
		return -EINVAL;

	if (vmids) {
		vmids_arr = kmalloc_array(nr_acl_entries, sizeof(*vmids_arr),
					  GFP_KERNEL);
		if (!vmids_arr)
			return -ENOMEM;

		*vmids = vmids_arr;
	}

	if (perms) {
		perms_arr = kmalloc_array(nr_acl_entries, sizeof(*perms_arr),
					  GFP_KERNEL);
		if (!perms_arr) {
			kfree(vmids_arr);
			return -ENOMEM;
		}

		*perms = perms_arr;
	}

	for (i = 0; i < nr_acl_entries; i++) {
		if (vmids_arr)
			vmids_arr[i] = acl_desc->acl_entries[i].vmid;
		if (perms_arr)
			perms_arr[i] = acl_desc->acl_entries[i].perms;
	}

	return 0;
}

static int mem_buf_assign_mem(struct mem_buf_xfer_mem *xfer_mem)
{
	int *dst_vmids, *dst_perms;
	u32 src_vmid = VMID_HLOS;
	int ret;

	if (!xfer_mem)
		return -EINVAL;

	ret = mem_buf_hh_acl_desc_to_vmid_perm_list(&xfer_mem->acl_desc,
						    &dst_vmids, &dst_perms);
	if (ret < 0)
		return ret;

	ret = hyp_assign_table(xfer_mem->mem_sgt, &src_vmid, 1, dst_vmids,
			       dst_perms, xfer_mem->acl_desc.n_acl_entries);
	if (ret < 0)
		pr_err("%s: failed to assign memory for rmt allocation rc:%d\n",
		       __func__, ret);
	kfree(dst_perms);
	kfree(dst_vmids);
	return ret;
}

static int mem_buf_unassign_mem(struct mem_buf_xfer_mem *xfer_mem)
{
	int dst_vmid = VMID_HLOS;
	int dst_perms = PERM_READ | PERM_WRITE | PERM_EXEC;
	int *src_vmids;
	int ret;

	if (!xfer_mem)
		return -EINVAL;

	ret = mem_buf_hh_acl_desc_to_vmid_perm_list(&xfer_mem->acl_desc,
						    &src_vmids, NULL);
	if (ret < 0)
		return ret;

	ret = hyp_assign_table(xfer_mem->mem_sgt, src_vmids,
			       xfer_mem->acl_desc.n_acl_entries, &dst_vmid,
			       &dst_perms, 1);
	if (ret < 0)
		pr_err("%s: failed to assign memory from rmt allocation rc: %d\n",
		       __func__, ret);
	kfree(src_vmids);
	return ret;
}

static int mem_buf_retrieve_memparcel_hdl(struct mem_buf_xfer_mem *xfer_mem)
{
	struct hh_sgl_desc *sgl_desc;
	unsigned int i, nr_sg_entries = xfer_mem->mem_sgt->nents;
	struct scatterlist *sg;
	int ret;
	size_t sgl_desc_size;

	sgl_desc_size = offsetof(struct hh_sgl_desc,
				 sgl_entries[nr_sg_entries]);
	sgl_desc = kzalloc(sgl_desc_size, GFP_KERNEL);
	if (!sgl_desc)
		return -ENOMEM;

	sgl_desc->n_sgl_entries = nr_sg_entries;
	for_each_sg(xfer_mem->mem_sgt->sgl, sg, nr_sg_entries, i) {
		sgl_desc->sgl_entries[i].ipa_base = page_to_phys(sg_page(sg));
		sgl_desc->sgl_entries[i].size = sg->length;
	}

	ret = hh_rm_mem_qcom_lookup_sgl(HH_RM_MEM_TYPE_NORMAL, 0,
					&xfer_mem->acl_desc, sgl_desc, NULL,
					&xfer_mem->hdl);
	if (ret < 0)
		pr_err("%s: hh_rm_mem_qcom_lookup_sgl failure rc: %d\n", ret);

	kfree(sgl_desc);
	return ret;
}

static
struct mem_buf_xfer_ion_mem *mem_buf_alloc_ion_xfer_mem_type_data(
								void *rmt_data)
{
	struct mem_buf_xfer_ion_mem *xfer_ion_mem;
	struct mem_buf_ion_alloc_data *ion_data = rmt_data;

	xfer_ion_mem = kmalloc(sizeof(*xfer_ion_mem), GFP_KERNEL);
	if (!xfer_ion_mem)
		return ERR_PTR(-ENOMEM);

	xfer_ion_mem->heap_id = ion_data->heap_id;
	return xfer_ion_mem;
}

static void *mem_buf_alloc_xfer_mem_type_data(enum mem_buf_mem_type type,
					      void *rmt_data)
{
	void *data = ERR_PTR(-EINVAL);

	if (type == MEM_BUF_ION_MEM_TYPE)
		data = mem_buf_alloc_ion_xfer_mem_type_data(rmt_data);

	return data;
}

static
void mem_buf_free_ion_xfer_mem_type_data(struct mem_buf_xfer_ion_mem *mem)
{
	kfree(mem);
}

static void mem_buf_free_xfer_mem_type_data(enum mem_buf_mem_type type,
					    void *data)
{
	if (type == MEM_BUF_ION_MEM_TYPE)
		mem_buf_free_ion_xfer_mem_type_data(data);
}

static
struct mem_buf_xfer_mem *mem_buf_prep_xfer_mem(void *req_msg)
{
	struct mem_buf_xfer_mem *xfer_mem;
	struct mem_buf_alloc_req *req = req_msg;
	u32 nr_acl_entries = req->acl_desc.n_acl_entries;
	size_t xfer_mem_size = offsetof(struct mem_buf_xfer_mem,
					acl_desc.acl_entries[nr_acl_entries]);
	size_t acl_size = offsetof(struct hh_acl_desc,
				   acl_entries[nr_acl_entries]);
	size_t alloc_req_msg_size = offsetof(struct mem_buf_alloc_req,
					acl_desc.acl_entries[nr_acl_entries]);
	void *arb_payload = req_msg + alloc_req_msg_size;
	void *mem_type_data;

	xfer_mem = kzalloc(xfer_mem_size, GFP_KERNEL);
	if (!xfer_mem)
		return ERR_PTR(-ENOMEM);

	xfer_mem->size = req->size;
	xfer_mem->mem_type = req->src_mem_type;
	mem_type_data = mem_buf_alloc_xfer_mem_type_data(req->src_mem_type,
							 arb_payload);
	if (IS_ERR(mem_type_data)) {
		pr_err("%s: failed to allocate mem type specific data: %d\n",
		       __func__, PTR_ERR(mem_type_data));
		kfree(xfer_mem);
		return ERR_CAST(mem_type_data);
	}
	xfer_mem->mem_type_data = mem_type_data;
	INIT_LIST_HEAD(&xfer_mem->entry);
	memcpy(&xfer_mem->acl_desc, &req->acl_desc, acl_size);
	return xfer_mem;
}

static void mem_buf_free_xfer_mem(struct mem_buf_xfer_mem *xfer_mem)
{
	mem_buf_free_xfer_mem_type_data(xfer_mem->mem_type,
					xfer_mem->mem_type_data);
	kfree(xfer_mem);
}

static struct mem_buf_xfer_mem *mem_buf_process_alloc_req(void *req)
{
	int ret;
	struct mem_buf_alloc_req *req_msg = req;
	struct mem_buf_xfer_mem *xfer_mem;

	xfer_mem = mem_buf_prep_xfer_mem(req_msg);
	if (IS_ERR(xfer_mem))
		return xfer_mem;

	ret = mem_buf_rmt_alloc_mem(xfer_mem);
	if (ret < 0)
		goto err_rmt_alloc;

	if (!xfer_mem->secure_alloc) {
		ret = mem_buf_assign_mem(xfer_mem);
		if (ret < 0)
			goto err_assign_mem;
	}

	ret = mem_buf_retrieve_memparcel_hdl(xfer_mem);
	if (ret < 0)
		goto err_retrieve_hdl;

	mutex_lock(&mem_buf_xfer_mem_list_lock);
	list_add(&xfer_mem->entry, &mem_buf_xfer_mem_list);
	mutex_unlock(&mem_buf_xfer_mem_list_lock);

	return xfer_mem;

err_retrieve_hdl:
	if (!xfer_mem->secure_alloc && (mem_buf_unassign_mem(xfer_mem) < 0))
		return ERR_PTR(ret);
err_assign_mem:
	mem_buf_rmt_free_mem(xfer_mem);
err_rmt_alloc:
	mem_buf_free_xfer_mem(xfer_mem);
	return ERR_PTR(ret);
}

static void mem_buf_cleanup_alloc_req(struct mem_buf_xfer_mem *xfer_mem)
{
	if (!xfer_mem->secure_alloc && (mem_buf_unassign_mem(xfer_mem) < 0))
		return;
	mem_buf_rmt_free_mem(xfer_mem);
	mem_buf_free_xfer_mem(xfer_mem);
}

static int mem_buf_get_mem_xfer_type(struct hh_acl_desc *acl_desc)
{
	u32 i, nr_acl_entries = acl_desc->n_acl_entries;

	for (i = 0; i < nr_acl_entries; i++)
		if (acl_desc->acl_entries[i].vmid == VMID_HLOS &&
		    acl_desc->acl_entries[i].perms != 0)
			return HH_RM_TRANS_TYPE_SHARE;

	return HH_RM_TRANS_TYPE_LEND;
}

static void mem_buf_alloc_req_work(struct work_struct *work)
{
	struct mem_buf_rmt_msg *rmt_msg = to_rmt_msg(work);
	struct mem_buf_alloc_req *req_msg = rmt_msg->msg;
	struct mem_buf_alloc_resp *resp_msg;
	struct mem_buf_xfer_mem *xfer_mem;
	int ret = 0;

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
	} else {
		resp_msg->hdl = xfer_mem->hdl;
	}

	resp_msg->ret = ret;
	ret = hh_msgq_send(mem_buf_hh_msgq_hdl, resp_msg, sizeof(*resp_msg), 0);

	/*
	 * Free the buffer regardless of the return value as the hypervisor
	 * would have consumed the data in the case of a success.
	 */
	kfree(resp_msg);

	if (ret < 0) {
		pr_err("%s: failed to send memory allocation response rc: %d\n",
		       __func__, ret);
		mutex_lock(&mem_buf_xfer_mem_list_lock);
		list_del(&xfer_mem->entry);
		mutex_unlock(&mem_buf_xfer_mem_list_lock);
		mem_buf_cleanup_alloc_req(xfer_mem);
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
	hh_memparcel_handle_t hdl = relinquish_msg->hdl;

	mutex_lock(&mem_buf_xfer_mem_list_lock);
	list_for_each_entry_safe(xfer_mem_iter, tmp, &mem_buf_xfer_mem_list,
				 entry)
		if (xfer_mem_iter->hdl == hdl) {
			xfer_mem = xfer_mem_iter;
			list_del(&xfer_mem->entry);
			break;
		}
	mutex_unlock(&mem_buf_xfer_mem_list_lock);

	if (xfer_mem)
		mem_buf_cleanup_alloc_req(xfer_mem);
	else
		pr_err("%s: transferred memory with handle 0x%x not found\n",
		       __func__, hdl);

	kfree(rmt_msg->msg);
	kfree(rmt_msg);
}

static int mem_buf_decode_alloc_resp(struct mem_buf_txn *txn, void *buf,
				      size_t size)
{
	struct mem_buf_alloc_resp *alloc_resp = buf;
	hh_memparcel_handle_t *hdl = txn->resp_buf;

	if (size != sizeof(*alloc_resp)) {
		pr_err("%s response received is not of correct size\n",
		       __func__);
		return -EINVAL;
	}

	if (alloc_resp->ret < 0)
		pr_err("%s remote allocation failed rc: %d\n", __func__,
		       alloc_resp->ret);
	else
		*hdl = alloc_resp->hdl;

	return alloc_resp->ret;
}

static void mem_buf_process_msg(void *buf, size_t size)
{
	struct mem_buf_msg_hdr *hdr = buf;
	struct mem_buf_txn *txn;
	struct mem_buf_rmt_msg *rmt_msg;
	work_func_t work_fn;

	if (size < sizeof(*hdr)) {
		pr_err("%s: message received is not of a proper size: 0x%lx\n",
		       __func__, size);
		kfree(buf);
		return;
	}

	if (hdr->msg_type == MEM_BUF_ALLOC_RESP) {
		mutex_lock(&mem_buf_idr_mutex);
		txn = idr_find(&mem_buf_txn_idr, hdr->txn_id);
		if (!txn) {
			pr_err("%s no txn associated with id: %d\n", __func__,
			       hdr->txn_id);
		} else {
			txn->txn_ret = mem_buf_decode_alloc_resp(txn, buf,
								 size);
			complete(&txn->txn_done);
		}
		mutex_unlock(&mem_buf_idr_mutex);
		kfree(buf);
	} else if (hdr->msg_type == MEM_BUF_ALLOC_REQ ||
		   hdr->msg_type == MEM_BUF_ALLOC_RELINQUISH) {
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
		schedule_work(&rmt_msg->work);
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
		buf = kzalloc(HH_MSGQ_MAX_MSG_SIZE_BYTES, GFP_KERNEL);
		if (!buf)
			continue;

		ret = hh_msgq_recv(mem_buf_hh_msgq_hdl, buf,
					HH_MSGQ_MAX_MSG_SIZE_BYTES, &size, 0);
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
	if (type == MEM_BUF_ION_MEM_TYPE)
		return sizeof(struct mem_buf_ion_alloc_data);

	return 0;
}

static void mem_buf_populate_alloc_req_arb_payload(void *dst, void *src,
						   enum mem_buf_mem_type type)
{
	struct mem_buf_ion_alloc_data *alloc_req_data;
	struct mem_buf_ion_data *src_ion_data;

	if (type == MEM_BUF_ION_MEM_TYPE) {
		alloc_req_data = dst;
		src_ion_data = src;
		alloc_req_data->heap_id = src_ion_data->heap_id;
	}
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
	acl_desc_size = offsetof(struct hh_acl_desc,
				 acl_entries[nr_acl_entries]);
	memcpy(&req->acl_desc, membuf->acl_desc, acl_desc_size);

	arb_payload = req_buf + alloc_req_size;
	mem_buf_populate_alloc_req_arb_payload(arb_payload, membuf->src_data,
					       membuf->src_mem_type);

	return req_buf;
}

static int mem_buf_request_mem(struct mem_buf_desc *membuf)
{
	struct mem_buf_txn txn;
	void *alloc_req_msg;
	size_t msg_size;
	hh_memparcel_handle_t resp_hdl;
	int ret;

	ret = mem_buf_init_txn(&txn, &resp_hdl);
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

	membuf->memparcel_hdl = resp_hdl;

out:
	mem_buf_destroy_txn(&txn);
	return ret;
}

static void mem_buf_relinquish_mem(struct mem_buf_desc *membuf)
{
	struct mem_buf_alloc_relinquish *msg;
	int ret;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg)
		return;

	msg->hdr.msg_type = MEM_BUF_ALLOC_RELINQUISH;
	msg->hdl = membuf->memparcel_hdl;

	ret = hh_msgq_send(mem_buf_hh_msgq_hdl, msg, sizeof(*msg), 0);

	/*
	 * Free the buffer regardless of the return value as the hypervisor
	 * would have consumed the data in the case of a success.
	 */
	kfree(msg);

	if (ret < 0)
		pr_err("%s failed to send memory relinquish message rc: %d\n",
		       __func__, ret);
}

static int mem_buf_map_mem_s2(struct mem_buf_desc *membuf)
{
	int ret = 0;
	struct hh_sgl_desc *sgl_desc;
	u32 xfer_type = mem_buf_get_mem_xfer_type(membuf->acl_desc);

	sgl_desc = hh_rm_mem_accept(membuf->memparcel_hdl,
				    HH_RM_MEM_TYPE_NORMAL, xfer_type,
				    HH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS |
				    HH_RM_MEM_ACCEPT_DONE, 0, membuf->acl_desc,
				    NULL, NULL, 0);
	if (IS_ERR(sgl_desc)) {
		ret = PTR_ERR(sgl_desc);
		pr_err("%s failed to map memory in stage 2 rc: %d\n", __func__,
		       PTR_ERR(sgl_desc));
	} else {
		membuf->sgl_desc = sgl_desc;
	}

	return ret;
}

static void mem_buf_unmap_mem_s2(struct mem_buf_desc *membuf)
{
	hh_rm_mem_release(membuf->memparcel_hdl, HH_RM_MEM_RELEASE_CLEAR);
	kfree(membuf->sgl_desc);
}

static int mem_buf_map_mem_s1(struct mem_buf_desc *membuf)
{
	int i, ret;
	unsigned int nid;
	u64 base, size;
	struct mhp_restrictions restrictions = {};

	mem_hotplug_begin();

	for (i = 0; i < membuf->sgl_desc->n_sgl_entries; i++) {
		base = membuf->sgl_desc->sgl_entries[i].ipa_base;
		size = membuf->sgl_desc->sgl_entries[i].size;
		nid = memory_add_physaddr_to_nid(base);
		memblock_add_node(base, size, nid);
		ret = arch_add_memory(nid, base, size, &restrictions);
		if (ret) {
			pr_err("%s failed to map memory in stage 1 rc: %d\n",
			       __func__, ret);
			goto err_add_mem;
		}
	}

	mem_hotplug_done();
	return 0;

err_add_mem:
	for (i = i - 1; i >= 0; i--) {
		base = membuf->sgl_desc->sgl_entries[i].ipa_base;
		size = membuf->sgl_desc->sgl_entries[i].size;
		nid = memory_add_physaddr_to_nid(base);
		arch_remove_memory(nid, base, size, NULL);
		memblock_remove(base, size);
	}

	mem_hotplug_done();
	return ret;
}

static void mem_buf_unmap_mem_s1(struct mem_buf_desc *membuf)
{
	unsigned int i, nid;
	u64 base, size;

	mem_hotplug_begin();

	for (i = 0; i < membuf->sgl_desc->n_sgl_entries; i++) {
		base = membuf->sgl_desc->sgl_entries[i].ipa_base;
		size = membuf->sgl_desc->sgl_entries[i].size;
		nid = memory_add_physaddr_to_nid(base);
		arch_remove_memory(nid, base, size, NULL);
		memblock_remove(base, size);
	}

	mem_hotplug_done();
}

static int mem_buf_add_ion_mem(struct sg_table *sgt, void *dst_data)
{
	struct mem_buf_ion_data *dst_ion_data = dst_data;

	return msm_ion_heap_add_memory(dst_ion_data->heap_id, sgt);
}

static int mem_buf_add_mem_type(enum mem_buf_mem_type type, void *dst_data,
				struct sg_table *sgt)
{
	if (type == MEM_BUF_ION_MEM_TYPE)
		return mem_buf_add_ion_mem(sgt, dst_data);

	return -EINVAL;
}

static int mem_buf_add_mem(struct mem_buf_desc *membuf)
{
	int i, ret, nr_sgl_entries;
	struct sg_table sgt;
	struct scatterlist *sgl;
	u64 base, size;

	nr_sgl_entries = membuf->sgl_desc->n_sgl_entries;
	ret = sg_alloc_table(&sgt, nr_sgl_entries, GFP_KERNEL);
	if (ret)
		return ret;

	for_each_sg(sgt.sgl, sgl, nr_sgl_entries, i) {
		base = membuf->sgl_desc->sgl_entries[i].ipa_base;
		size = membuf->sgl_desc->sgl_entries[i].size;
		sg_set_page(sgl, phys_to_page(base), size, 0);
	}

	ret = mem_buf_add_mem_type(membuf->dst_mem_type, membuf->dst_data,
				   &sgt);
	if (ret)
		pr_err("%s failed to add memory to destination rc: %d\n",
		       __func__, ret);

	sg_free_table(&sgt);
	return ret;
}

static int mem_buf_remove_ion_mem(struct sg_table *sgt, void *dst_data)
{
	struct mem_buf_ion_data *dst_ion_data = dst_data;

	return msm_ion_heap_remove_memory(dst_ion_data->heap_id, sgt);
}

static int mem_buf_remove_mem_type(enum mem_buf_mem_type type, void *dst_data,
				   struct sg_table *sgt)
{
	if (type == MEM_BUF_ION_MEM_TYPE)
		return mem_buf_remove_ion_mem(sgt, dst_data);

	return -EINVAL;
}

static int mem_buf_remove_mem(struct mem_buf_desc *membuf)
{
	int i, ret, nr_sgl_entries;
	struct sg_table sgt;
	struct scatterlist *sgl;
	u64 base, size;

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

	sg_free_table(&sgt);
	return ret;
}

static bool is_valid_mem_buf_vmid(u32 mem_buf_vmid)
{
	if ((mem_buf_vmid == MEM_BUF_VMID_PRIMARY_VM) ||
	    (mem_buf_vmid == MEM_BUF_VMID_TRUSTED_VM))
		return true;

	return false;
}

static bool is_valid_mem_buf_perms(u32 mem_buf_perms)
{
	return !(mem_buf_perms & ~MEM_BUF_PERM_VALID_FLAGS);
}

static int mem_buf_vmid_to_vmid(u32 mem_buf_vmid)
{
	int ret;
	hh_vmid_t vmid;
	enum hh_vm_names vm_name;

	if (mem_buf_vmid == MEM_BUF_VMID_PRIMARY_VM)
		vm_name = HH_PRIMARY_VM;
	else if (mem_buf_vmid == MEM_BUF_VMID_TRUSTED_VM)
		vm_name = HH_TRUSTED_VM;
	else
		return -EINVAL;

	ret = hh_rm_get_vmid(vm_name, &vmid);
	if (!ret)
		return vmid;
	return ret;
}

static int mem_buf_perms_to_perms(u32 mem_buf_perms)
{
	int perms = 0;

	if (mem_buf_perms & MEM_BUF_PERM_FLAG_READ)
		perms |= PERM_READ;
	if (mem_buf_perms & MEM_BUF_PERM_FLAG_WRITE)
		perms |= PERM_WRITE;
	if (mem_buf_perms & MEM_BUF_PERM_FLAG_EXEC)
		perms |= PERM_EXEC;

	return perms;
}

static struct hh_acl_desc *mem_buf_acl_to_hh_acl(unsigned int nr_acl_entries,
						 struct acl_entry *entries)
{
	unsigned int i;
	int ret;
	u32 mem_buf_vmid, mem_buf_perms;
	int vmid, perms;
	struct hh_acl_desc *acl_desc = kzalloc(offsetof(struct hh_acl_desc,
						acl_entries[nr_acl_entries]),
						GFP_KERNEL);

	if (!acl_desc)
		return ERR_PTR(-ENOMEM);

	acl_desc->n_acl_entries = nr_acl_entries;
	for (i = 0; i < nr_acl_entries; i++) {
		mem_buf_vmid = entries[i].vmid;
		mem_buf_perms = entries[i].perms;
		if (!is_valid_mem_buf_vmid(mem_buf_vmid) ||
		    !is_valid_mem_buf_perms(mem_buf_perms)) {
			ret = -EINVAL;
			goto err_inv_vmid_perms;
		}
		vmid = mem_buf_vmid_to_vmid(mem_buf_vmid);
		perms = mem_buf_perms_to_perms(mem_buf_perms);
		acl_desc->acl_entries[i].vmid = vmid;
		acl_desc->acl_entries[i].perms = perms;
	}

	return acl_desc;

err_inv_vmid_perms:
	kfree(acl_desc);
	return ERR_PTR(ret);
}

static void *mem_buf_retrieve_ion_mem_type_data_user(
				struct mem_buf_ion_data __user *mem_type_data)
{
	return memdup_user((const void __user *)mem_type_data,
			   sizeof(*mem_type_data));
}

static void *mem_buf_retrieve_mem_type_data_user(enum mem_buf_mem_type mem_type,
						 void __user *mem_type_data)
{
	void *data = ERR_PTR(-EINVAL);

	if (mem_type == MEM_BUF_ION_MEM_TYPE)
		data = mem_buf_retrieve_ion_mem_type_data_user(mem_type_data);

	return data;
}


static void *mem_buf_retrieve_ion_mem_type_data(
					struct mem_buf_ion_data *mem_type_data)
{
	return kmemdup(mem_type_data, sizeof(*mem_type_data), GFP_KERNEL);
}

static void *mem_buf_retrieve_mem_type_data(enum mem_buf_mem_type mem_type,
					    void *mem_type_data)
{
	void *data = ERR_PTR(-EINVAL);

	if (mem_type == MEM_BUF_ION_MEM_TYPE)
		data = mem_buf_retrieve_ion_mem_type_data(mem_type_data);

	return data;
}

static void mem_buf_free_ion_mem_type_data(struct mem_buf_ion_data *ion_data)
{
	kfree(ion_data);
}

static void mem_buf_free_mem_type_data(enum mem_buf_mem_type mem_type,
				       void *mem_type_data)
{
	if (mem_type == MEM_BUF_ION_MEM_TYPE)
		mem_buf_free_ion_mem_type_data(mem_type_data);
}

static int mem_buf_buffer_release(struct inode *inode, struct file *filp)
{
	struct mem_buf_desc *membuf = filp->private_data;
	int ret;

	mutex_lock(&mem_buf_list_lock);
	list_del(&membuf->entry);
	mutex_unlock(&mem_buf_list_lock);

	ret = mem_buf_remove_mem(membuf);
	if (ret < 0)
		return ret;

	mem_buf_unmap_mem_s1(membuf);
	mem_buf_unmap_mem_s2(membuf);
	mem_buf_relinquish_mem(membuf);
	mem_buf_free_mem_type_data(membuf->dst_mem_type, membuf->dst_data);
	mem_buf_free_mem_type_data(membuf->src_mem_type, membuf->src_data);
	kfree(membuf->acl_desc);
	kfree(membuf);
	return 0;
}

static const struct file_operations mem_buf_fops = {
	.release = mem_buf_buffer_release,
};

void *mem_buf_alloc(struct mem_buf_allocation_data *alloc_data)
{
	int ret;
	struct file *filp;
	struct mem_buf_desc *membuf;

	if (!alloc_data)
		return ERR_PTR(-EINVAL);

	membuf = kzalloc(sizeof(*membuf), GFP_KERNEL);
	if (!membuf)
		return ERR_PTR(-ENOMEM);

	membuf->size = alloc_data->size;
	membuf->acl_desc = mem_buf_acl_to_hh_acl(alloc_data->nr_acl_entries,
						 alloc_data->acl_list);
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

	ret = mem_buf_request_mem(membuf);
	if (ret)
		goto err_mem_req;

	ret = mem_buf_map_mem_s2(membuf);
	if (ret)
		goto err_map_mem_s2;

	ret = mem_buf_map_mem_s1(membuf);
	if (ret)
		goto err_map_mem_s1;

	ret = mem_buf_add_mem(membuf);
	if (ret)
		goto err_add_mem;

	filp = anon_inode_getfile("membuf", &mem_buf_fops, membuf, O_RDWR);
	if (IS_ERR(filp)) {
		ret = PTR_ERR(filp);
		goto err_get_file;
	}
	membuf->filp = filp;

	mutex_lock(&mem_buf_list_lock);
	list_add_tail(&membuf->entry, &mem_buf_list);
	mutex_unlock(&mem_buf_list_lock);

	return membuf;

err_get_file:
	if (mem_buf_remove_mem(membuf) < 0)
		return ERR_PTR(ret);
err_add_mem:
	mem_buf_unmap_mem_s1(membuf);
err_map_mem_s1:
	mem_buf_unmap_mem_s2(membuf);
err_map_mem_s2:
	mem_buf_relinquish_mem(membuf);
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
EXPORT_SYMBOL(mem_buf_alloc);

int mem_buf_get_fd(void *membuf_desc)
{
	int fd;
	struct mem_buf_desc *membuf = membuf_desc;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0)
		return fd;

	fd_install(fd, membuf->filp);
	return fd;
}
EXPORT_SYMBOL(mem_buf_get_fd);

void mem_buf_put(void *membuf_desc)
{
	struct mem_buf_desc *membuf = membuf_desc;

	if (membuf && membuf->filp)
		fput(membuf->filp);
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

/* Userspace machinery */
static int mem_buf_prep_alloc_data(struct mem_buf_allocation_data *alloc_data,
				struct mem_buf_alloc_ioctl_arg *allocation_args)
{
	struct acl_entry *acl_list;
	unsigned int nr_acl_entries = allocation_args->nr_acl_entries;
	int ret;

	alloc_data->size = allocation_args->size;
	alloc_data->nr_acl_entries = nr_acl_entries;

	alloc_data->acl_list =
		memdup_user((const void __user *)allocation_args->acl_list,
			    sizeof(*acl_list) * nr_acl_entries);
	if (IS_ERR(alloc_data->acl_list))
		return PTR_ERR(alloc_data->acl_list);

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
	kfree(alloc_data->acl_list);
	return ret;
}

static void mem_buf_free_alloc_data(struct mem_buf_allocation_data *alloc_data)
{
	mem_buf_free_mem_type_data(alloc_data->dst_mem_type,
				   alloc_data->dst_data);
	mem_buf_free_mem_type_data(alloc_data->src_mem_type,
				   alloc_data->src_data);
	kfree(alloc_data->acl_list);
}

static int mem_buf_alloc_fd(struct mem_buf_alloc_ioctl_arg *allocation_args)
{
	struct mem_buf_desc *membuf;
	struct mem_buf_allocation_data alloc_data;
	int ret;

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

static bool is_valid_mem_type(enum mem_buf_mem_type mem_type)
{
	return mem_type >= MEM_BUF_ION_MEM_TYPE &&
		mem_type < MEM_BUF_MAX_MEM_TYPE;
}

static int validate_ioctl_arg(struct mem_buf_alloc_ioctl_arg *allocation)
{
	if (!allocation->size || !allocation->nr_acl_entries ||
	    !allocation->acl_list ||
	    !is_valid_mem_type(allocation->src_mem_type) ||
	    !is_valid_mem_type(allocation->dst_mem_type) ||
	    allocation->reserved0 || allocation->reserved1 ||
	    allocation->reserved2)
		return -EINVAL;

	return 0;
}

static long mem_buf_dev_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	int fd;
	unsigned int dir = _IOC_DIR(cmd);
	struct mem_buf_alloc_ioctl_arg allocation;

	if (_IOC_SIZE(cmd) > sizeof(allocation))
		return -EINVAL;

	if (copy_from_user(&allocation, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	if (validate_ioctl_arg(&allocation) < 0)
		return -EINVAL;

	if (!(dir & _IOC_WRITE))
		memset(&allocation, 0, sizeof(allocation));

	switch (cmd) {
	case MEM_BUF_IOC_ALLOC:
	{
		fd = mem_buf_alloc_fd(&allocation);

		if (fd < 0)
			return fd;

		allocation.mem_buf_fd = fd;
		break;
	}
	default:
		return -ENOTTY;
	}

	if (dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &allocation,
				 _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	return 0;
}

static const struct file_operations mem_buf_dev_fops = {
	.unlocked_ioctl = mem_buf_dev_ioctl,
};

static int mem_buf_probe(struct platform_device *pdev)
{
	int ret;
	struct device *class_dev;

	mem_buf_msgq_recv_thr = kthread_create(mem_buf_msgq_recv_fn, NULL,
					       "mem_buf_rcvr");
	if (IS_ERR(mem_buf_msgq_recv_thr)) {
		dev_err(&pdev->dev,
			"Failed to create msgq receiver thread rc: %d\n",
			PTR_ERR(mem_buf_msgq_recv_thr));
		return PTR_ERR(mem_buf_msgq_recv_thr);
	}

	mem_buf_hh_msgq_hdl = hh_msgq_register(HH_MSGQ_LABEL_MEMBUF);
	if (IS_ERR(mem_buf_hh_msgq_hdl)) {
		ret = PTR_ERR(mem_buf_hh_msgq_hdl);
		if (ret != EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Message queue registration failed: rc: %d\n",
				ret);
		goto err_msgq_register;
	}

	cdev_init(&mem_buf_char_dev, &mem_buf_dev_fops);
	ret = cdev_add(&mem_buf_char_dev, mem_buf_dev_no, MEM_BUF_MAX_DEVS);
	if (ret < 0)
		goto err_cdev_add;

	mem_buf_dev = &pdev->dev;
	class_dev = device_create(mem_buf_class, NULL, mem_buf_dev_no, NULL,
				  "membuf");
	if (IS_ERR(class_dev)) {
		ret = PTR_ERR(class_dev);
		goto err_dev_create;
	}

	wake_up_process(mem_buf_msgq_recv_thr);
	return 0;

err_dev_create:
	mem_buf_dev = NULL;
	cdev_del(&mem_buf_char_dev);
err_cdev_add:
	hh_msgq_unregister(mem_buf_hh_msgq_hdl);
	mem_buf_hh_msgq_hdl = NULL;
err_msgq_register:
	kthread_stop(mem_buf_msgq_recv_thr);
	mem_buf_msgq_recv_thr = NULL;
	return ret;
}

static int mem_buf_remove(struct platform_device *pdev)
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
	mem_buf_dev = NULL;
	cdev_del(&mem_buf_char_dev);
	hh_msgq_unregister(mem_buf_hh_msgq_hdl);
	mem_buf_hh_msgq_hdl = NULL;
	kthread_stop(mem_buf_msgq_recv_thr);
	mem_buf_msgq_recv_thr = NULL;
	return 0;
}

static const struct of_device_id mem_buf_match_tbl[] = {
	 {.compatible = "qcom,mem-buf"},
	 {},
};

static struct platform_driver mem_buf_driver = {
	.probe = mem_buf_probe,
	.remove = mem_buf_remove,
	.driver = {
		.name = "mem-buf",
		.of_match_table = of_match_ptr(mem_buf_match_tbl),
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

	ret = platform_driver_register(&mem_buf_driver);
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
	platform_driver_unregister(&mem_buf_driver);
	class_destroy(mem_buf_class);
	unregister_chrdev_region(mem_buf_dev_no, MEM_BUF_MAX_DEVS);
}
module_exit(mem_buf_exit);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Memory Buffer Sharing driver");
MODULE_LICENSE("GPL v2");
