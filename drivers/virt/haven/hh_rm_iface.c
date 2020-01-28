// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/slab.h>
#include <linux/limits.h>
#include <linux/module.h>

#include <linux/haven/hh_msgq.h>
#include <linux/haven/hh_common.h>

#include "hh_rm_drv_private.h"

static struct hh_vm_property hh_vm_table[HH_VM_MAX];

int hh_update_vm_prop_table(enum hh_vm_names vm_name,
			struct hh_vm_property *vm_prop)
{
	if (vm_prop->vmid <= 0)
		return -EINVAL;
	hh_vm_table[vm_name].vmid = vm_prop->vmid;

	return 0;
}

/**
 * hh_rm_get_vmid: Translate VM name to vmid
 * @vm_name: VM name to lookup
 * @vmid: out pointer to store found vmid if VM is ofund
 *
 * If no VM is known to RM with the supplied name, -EINVAL is returned.
 * 0 otherwise.
 */
int hh_rm_get_vmid(enum hh_vm_names vm_name, hh_vmid_t *vmid)
{
	hh_vmid_t _vmid = hh_vm_table[vm_name].vmid;

	if (!_vmid) {
		pr_err("%s: No vmid associated with the vm\n", __func__);
		return -EINVAL;
	}

	if (vmid)
		*vmid = _vmid;

	return 0;
}
EXPORT_SYMBOL(hh_rm_get_vmid);

/**
 * hh_rm_get_vm_name: Translate vmid to vm name
 * @vmid: vmid to lookup
 * @vm_name: out pointer to store found VM name if vmid is found
 *
 * If no VM is known to RM with the supplied VMID, -EINVAL is returned.
 * 0 otherwise.
 */
int hh_rm_get_vm_name(hh_vmid_t vmid, enum hh_vm_names *vm_name)
{
	enum hh_vm_names i;

	for (i = 0; i < HH_VM_MAX; i++)
		if (hh_vm_table[i].vmid == vmid) {
			if (vm_name)
				*vm_name = i;
			return 0;
		}

	return -EINVAL;
}
EXPORT_SYMBOL(hh_rm_get_vm_name);

/**
 * hh_rm_vm_get_hyp_res: Get info about a series of resources for this VM
 * @vmid: vmid whose info is needed. Pass 0 for self
 * @n_entries: The number of the resource entries that's returned to the caller
 *
 * The function returns an array of type 'struct hh_vm_get_hyp_res_resp_entry',
 * in which each entry specifies info about a particular resource. The number
 * of entries in the array is returned by 'n_entries'. The caller must kfree
 * the returned pointer when done.
 *
 * The function encodes the error codes via ERR_PTR. Hence, the caller is
 * responsible to check it with IS_ERR_OR_NULL().
 */
struct hh_vm_get_hyp_res_resp_entry *
hh_rm_vm_get_hyp_res(hh_vmid_t vmid, u32 *n_entries)
{
	struct hh_vm_get_hyp_res_resp_payload *resp_payload;
	struct hh_vm_get_hyp_res_req_payload req_payload = {
		.vmid = vmid
	};
	struct hh_vm_get_hyp_res_resp_entry *resp_entries;
	size_t resp_payload_size, resp_entries_size;
	int err, reply_err_code;

	if (!n_entries)
		return ERR_PTR(-EINVAL);

	resp_payload = hh_rm_call(HH_RM_RPC_MSG_ID_CALL_VM_GET_HYP_RESOURCES,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		err = PTR_ERR(resp_payload);
		pr_err("%s: GET_HYP_RESOURCES failed with err: %d\n",
			__func__, err);
		return ERR_PTR(err);
	}

	/* The response payload should contain all the resource entries */
	if (resp_payload_size < sizeof(*n_entries) ||
		resp_payload_size != sizeof(*n_entries) +
		(resp_payload->n_resource_entries * sizeof(*resp_entries))) {
		pr_err("%s: Invalid size received for GET_HYP_RESOURCES: %u\n",
			__func__, resp_payload_size);
		resp_entries = ERR_PTR(-EINVAL);
		goto out;
	}

	resp_entries_size = sizeof(*resp_entries) *
				resp_payload->n_resource_entries;
	resp_entries = kmemdup(resp_payload->resp_entries, resp_entries_size,
			       GFP_KERNEL);
	if (!resp_entries) {
		resp_entries = ERR_PTR(-ENOMEM);
		goto out;
	}

	*n_entries = resp_payload->n_resource_entries;

out:
	kfree(resp_payload);
	return resp_entries;
}

/**
 * hh_rm_vm_irq_accept: Bind the virq number to the supplied virq_handle
 * @virq_handle: The virtual IRQ handle (for example, obtained via
 *               call to hh_rm_get_hyp_resources())
 * @virq: The virtual IRQ number to bind to. Note that this is the virtual
 *        GIC IRQ number and not the linux IRQ number. Pass -1 here if the
 *        caller wants the Resource Manager VM to allocate a number
 *
 * If provided -1 for virq, the function returns the new IRQ number, else
 * the one that was already provided.
 *
 * The function encodes the error codes via ERR_PTR. Hence, the caller is
 * responsible to check it with IS_ERR_OR_NULL().
 */
int hh_rm_vm_irq_accept(hh_virq_handle_t virq_handle, int virq)
{
	struct hh_vm_irq_accept_resp_payload *resp_payload;
	struct hh_vm_irq_accept_req_payload req_payload;
	size_t resp_payload_size;
	int ret, reply_err_code;

	/* -1 is valid for virq if requesting for a new number */
	if (virq < -1)
		return -EINVAL;

	req_payload.virq_handle = virq_handle;
	req_payload.virq = virq;

	resp_payload = hh_rm_call(HH_RM_RPC_MSG_ID_CALL_VM_IRQ_ACCEPT,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		ret = PTR_ERR(resp_payload);
		pr_err("%s: VM_IRQ_ACCEPT failed with err: %d\n",
			__func__, ret);
		return ret;
	}

	if (virq == -1 && resp_payload_size != sizeof(*resp_payload)) {
		pr_err("%s: Invalid size received for VM_IRQ_ACCEPT: %u\n",
			__func__, resp_payload_size);
		ret = -EINVAL;
		goto out;
	}

	ret = virq == -1 ? resp_payload->virq : virq;
out:
	kfree(resp_payload);
	return ret;
}

/**
 * hh_rm_vm_irq_lend: Lend an IRQ to another VM
 * @vmid: VM to lend the interrupt to
 * @virq: Virtual IRQ number to lend
 * @label: Label to give to VM so it may know how to associate the interrupt
 * @virq_handle: Response handle which RM will accept from the other VM to take
 *		 the lent interrupt
 */
static int hh_rm_vm_irq_lend(hh_vmid_t vmid, int virq, int label,
			     hh_virq_handle_t *virq_handle)
{
	struct hh_vm_irq_lend_resp_payload *resp_payload;
	struct hh_vm_irq_lend_req_payload req_payload;
	size_t resp_payload_size;
	int ret = 0, reply_err_code;

	req_payload.vmid = vmid;
	req_payload.virq = virq;
	req_payload.label = label;

	resp_payload = hh_rm_call(HH_RM_RPC_MSG_ID_CALL_VM_IRQ_LEND,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		ret = PTR_ERR(resp_payload);
		pr_err("%s: VM_IRQ_LEND failed with err: %d\n",
			__func__, ret);
		return ret;
	}

	if (resp_payload_size != sizeof(*resp_payload)) {
		pr_err("%s: Invalid size received for VM_IRQ_LEND: %u\n",
			__func__, resp_payload_size);
		ret = -EINVAL;
		goto out;
	}

	if (virq_handle)
		*virq_handle = resp_payload->virq;
out:
	kfree(resp_payload);
	return ret;
}

/**
 * hh_rm_vm_irq_notify: Notify an IRQ to another VM
 * @vmids: VMs to notify the handle about
 * @num_vmids: number of VMs to notify the handle about
 * @flags: notification reason
 * @virq_handle: Response handle which RM will accept from the other VM to take
 *		 the lent interrupt
 */
static int hh_rm_vm_irq_notify(const hh_vmid_t *vmids, unsigned int num_vmids,
			       u16 flags, hh_virq_handle_t virq_handle)
{
	struct hh_vm_irq_notify_resp_payload *resp_payload;
	struct hh_vm_irq_notify_req_payload *req_payload;
	size_t resp_payload_size, req_payload_size;
	int ret = 0, reply_err_code;
	unsigned int i;


	if (!(flags & HH_VM_IRQ_NOTIFY_FLAGS_LENT) && num_vmids)
		return -EINVAL;

	if (num_vmids > U32_MAX)
		return -EINVAL;

	req_payload_size = sizeof(*req_payload);
	if (flags & HH_VM_IRQ_NOTIFY_FLAGS_LENT)
		req_payload_size += sizeof(req_payload->optional) +
			(sizeof(req_payload->optional->vmids[0]) * num_vmids);
	req_payload = kzalloc(req_payload_size, GFP_KERNEL);

	if (!req_payload)
		return -ENOMEM;

	req_payload->virq = virq_handle;
	req_payload->flags = flags;
	if (flags & HH_VM_IRQ_NOTIFY_FLAGS_LENT) {
		req_payload->optional[0].num_vmids = num_vmids;
		for (i = 0; i < num_vmids; i++)
			req_payload->optional[0].vmids[i] = vmids[i];
	}


	resp_payload = hh_rm_call(HH_RM_RPC_MSG_ID_CALL_VM_IRQ_NOTIFY,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	kfree(req_payload);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		ret = PTR_ERR(resp_payload);
		pr_err("%s: VM_IRQ_NOTIFY failed with err: %d\n",
			__func__, ret);
		return ret;
	}

	if (resp_payload_size != sizeof(*resp_payload)) {
		pr_err("%s: Invalid size received for VM_IRQ_NOTIFY: %u\n",
			__func__, resp_payload_size);
		ret = -EINVAL;
	}

	kfree(resp_payload);
	return ret;
}

/**
 * hh_rm_vm_irq_lend_notify: Lend an IRQ to a VM and notify the VM about it
 * @vmid: VM to lend interrupt to
 * @virq: Virtual IRQ number to lend
 * @label: Label to give to VM so it may know how to associate the interrupt
 *
 * This function performs interrupt sharing flow for "HLOS" described in
 * Resource Manager High Level Design Sec. 3.3.3.
 */
int hh_rm_vm_irq_lend_notify(hh_vmid_t vmid, int virq, int label)
{
	hh_virq_handle_t virq_handle;
	int ret;

	ret = hh_rm_vm_irq_lend(vmid, virq, label, &virq_handle);
	if (ret)
		return ret;

	return hh_rm_vm_irq_notify(&vmid, 1, HH_VM_IRQ_NOTIFY_FLAGS_LENT,
				   virq_handle);
}

/**
 * hh_rm_vm_alloc_vmid: Return a vmid associated with the vm loaded into
 *			memory. This call should be called only during
			initialization.
 * @vm_name: The enum value of the vm that has been loaded.
 *
 * The function encodes the error codes via ERR_PTR. Hence, the caller is
 * responsible to check it with IS_ERR_OR_NULL().
 */
int hh_rm_vm_alloc_vmid(enum hh_vm_names vm_name)
{
	struct hh_vm_allocate_resp_payload *resp_payload;
	struct hh_vm_allocate_req_payload req_payload;
	size_t resp_payload_size;
	struct hh_vm_property vm_prop;
	int err, reply_err_code;

	/* Look up for the vm_name<->vmid pair if already present.
	 * If so, return.
	 */
	if (!hh_vm_table[vm_name].vmid) {
		pr_err("%s: VM_ALLOCATE already called for this VM\n",
			__func__);
		return -EINVAL;
	}

	req_payload.vmid = 0;

	resp_payload = hh_rm_call(HH_RM_RPC_MSG_ID_CALL_VM_ALLOCATE,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		err = PTR_ERR(resp_payload);
		pr_err("%s: VM_ALLOCATE failed with err: %d\n",
			__func__, err);
		return err;
	}

	if (resp_payload_size != sizeof(*resp_payload)) {
		pr_err("%s: Invalid size received for VM_ALLOCATE: %u\n",
			__func__, resp_payload_size);
		kfree(resp_payload);
		return -EINVAL;
	}

	vm_prop.vmid = resp_payload->vmid;
	err = hh_update_vm_prop_table(vm_name, &vm_prop);

	if (err) {
		pr_err("%s: Invalid vmid sent for updating table: %u\n",
			__func__, resp_payload_size);
		return -EINVAL;
	}
	return resp_payload->vmid;
}
EXPORT_SYMBOL(hh_rm_vm_alloc_vmid);

/**
 * hh_rm_vm_start: Send a request to Resource Manager VM to start a VM.
 * @vmid: The vmid of the vm to be started.
 *
 * The function encodes the error codes via ERR_PTR. Hence, the caller is
 * responsible to check it with IS_ERR_OR_NULL().
 */
int hh_rm_vm_start(int vmid)
{
	struct hh_vm_start_resp_payload *resp_payload;
	struct hh_vm_start_req_payload req_payload;
	size_t resp_payload_size;
	int err, reply_err_code;

	req_payload.vmid = (hh_vmid_t) vmid;

	resp_payload = hh_rm_call(HH_RM_RPC_MSG_ID_CALL_VM_START,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		err = PTR_ERR(resp_payload);
		pr_err("%s: VM_ALLOCATE failed with err: %d\n",
			__func__, err);
		return err;
	}

	if (resp_payload_size != sizeof(*resp_payload)) {
		pr_err("%s: Invalid size received for VM_IRQ_ACCEPT: %u\n",
			__func__, resp_payload_size);
		kfree(resp_payload);
		return -EINVAL;
	}

	return resp_payload->response;
}
EXPORT_SYMBOL(hh_rm_vm_start);

/**
 * hh_rm_console_open: Open a console with a VM
 * @vmid: The vmid of the vm to be started.
 */
int hh_rm_console_open(hh_vmid_t vmid)
{
	struct hh_vm_console_common_resp_payload *resp_payload;
	struct hh_vm_console_common_req_payload req_payload = {0};
	size_t resp_payload_size;
	int err = 0, reply_err_code = 0;

	req_payload.vmid = vmid;

	resp_payload = hh_rm_call(HH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_OPEN,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		err = PTR_ERR(resp_payload);
		pr_err("%s: CONSOLE_OPEN failed with err: %d\n",
			__func__, err);
		goto out;
	}

	if (resp_payload_size != sizeof(*resp_payload)) {
		pr_err("%s: Invalid size received for CONSOLE_OPEN: %u\n",
			__func__, resp_payload_size);
		err = -EINVAL;
	}

	err = err ? : resp_payload->response;
	kfree(resp_payload);
out:
	return err;
}
EXPORT_SYMBOL(hh_rm_console_open);

/**
 * hh_rm_console_close: Close a console with a VM
 * @vmid: The vmid of the vm whose console to close.
 */
int hh_rm_console_close(hh_vmid_t vmid)
{
	struct hh_vm_console_common_resp_payload *resp_payload;
	struct hh_vm_console_common_req_payload req_payload = {0};
	size_t resp_payload_size;
	int err = 0, reply_err_code = 0;

	req_payload.vmid = vmid;

	resp_payload = hh_rm_call(HH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_CLOSE,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		err = PTR_ERR(resp_payload);
		pr_err("%s: CONSOLE_CLOSE failed with err: %d\n",
			__func__, err);
		goto out;
	}

	if (resp_payload_size != sizeof(*resp_payload)) {
		pr_err("%s: Invalid size received for CONSOLE_CLOSE: %u\n",
			__func__, resp_payload_size);
		err = -EINVAL;
	}

	err = err ? : resp_payload->response;
	kfree(resp_payload);
out:
	return err;
}
EXPORT_SYMBOL(hh_rm_console_close);

/**
 * hh_rm_console_write: Write to a VM's console
 * @vmid: The vmid of the vm whose console to write to.
 * @buf: Buffer to write to the VM's console
 * @size: Size of the buffer
 */
int hh_rm_console_write(hh_vmid_t vmid, const char *buf, size_t size)
{
	struct hh_vm_console_write_resp_payload *resp_payload;
	struct hh_vm_console_write_req_payload *req_payload;
	size_t resp_payload_size;
	int err = 0, reply_err_code = 0;
	size_t req_payload_size = sizeof(*req_payload) + size;

	if (size < 1 || size > U32_MAX)
		return -EINVAL;

	req_payload = kzalloc(req_payload_size, GFP_KERNEL);

	if (!req_payload)
		return -ENOMEM;

	req_payload->vmid = vmid;
	req_payload->num_bytes = size;
	memcpy(req_payload->data, buf, size);

	resp_payload = hh_rm_call(HH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_WRITE,
				req_payload, req_payload_size,
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		err = PTR_ERR(resp_payload);
		pr_err("%s: CONSOLE_WRITE failed with err: %d\n",
			__func__, err);
		goto out;
	}

	if (resp_payload_size != sizeof(*resp_payload)) {
		pr_err("%s: Invalid size received for CONSOLE_WRITE: %u\n",
			__func__, resp_payload_size);
		err = -EINVAL;
	}

	err = err ? : resp_payload->response;
	kfree(resp_payload);
out:
	kfree(req_payload);
	return err;
}
EXPORT_SYMBOL(hh_rm_console_write);

/**
 * hh_rm_console_flush: Flush a console with a VM
 * @vmid: The vmid of the vm whose console to flush
 */
int hh_rm_console_flush(hh_vmid_t vmid)
{
	struct hh_vm_console_common_resp_payload *resp_payload;
	struct hh_vm_console_common_req_payload req_payload = {0};
	size_t resp_payload_size;
	int err = 0, reply_err_code = 0;

	req_payload.vmid = vmid;

	resp_payload = hh_rm_call(HH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_FLUSH,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		err = PTR_ERR(resp_payload);
		pr_err("%s: CONSOLE_FLUSH failed with err: %d\n",
			__func__, err);
		goto out;
	}

	if (resp_payload_size != sizeof(*resp_payload)) {
		pr_err("%s: Invalid size received for CONSOLE_FLUSH: %u\n",
			__func__, resp_payload_size);
		err = -EINVAL;
	}

	err = err ? : resp_payload->response;
	kfree(resp_payload);
out:
	return err;
}
EXPORT_SYMBOL(hh_rm_console_flush);
