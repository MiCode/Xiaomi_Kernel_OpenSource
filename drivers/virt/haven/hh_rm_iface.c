// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/slab.h>
#include <linux/module.h>

#include <linux/haven/hh_msgq.h>
#include <linux/haven/hh_common.h>

#include "hh_rm_drv_private.h"

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
