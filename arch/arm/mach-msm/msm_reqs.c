/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <mach/msm_reqs.h>
#include "npa_remote.h"
#include <linux/slab.h>

struct request {
	atomic_t creation_pending;
	struct npa_client *client;
	char *client_name;
	char *resource_name;
};

/**
 * npa_res_available_cb - NPA resource-available callback.
 * @u_data:	Pointer to request struct.
 * @t,d,n:	Unused
 */
static void npa_res_available_cb(void *u_data, unsigned t, void *d, unsigned n)
{
	struct request *request = u_data;

	/* Make sure client is still needed. */
	if (!atomic_dec_and_test(&request->creation_pending)) {
		kfree(request);
		return;
	}

	/* Create NPA 'required' client. */
	request->client = npa_create_sync_client(request->resource_name,
				request->client_name, NPA_CLIENT_REQUIRED);
	if (IS_ERR(request->client)) {
		pr_crit("npa_req: Failed to create NPA client '%s' "
			"for resource '%s'. (Error %ld)\n",
			request->client_name, request->resource_name,
			PTR_ERR(request->client));
		BUG();
	}

	return;
}

/**
 * msm_req_add - Creates an NPA request and returns a handle. Non-blocking.
 * @req_name:	Name of the request
 * @res_name:	Name of the NPA resource the request is for
 */
void *msm_req_add(char *res_name, char *req_name)
{
	struct request *request;

	request = kmalloc(sizeof(*request), GFP_KERNEL);
	if (!request)
		return ERR_PTR(-ENOMEM);

	/* Populate request data. */
	request->client_name = req_name;
	request->resource_name = res_name;

	/* Mark client creation as pending. */
	atomic_set(&request->creation_pending, 1);

	/* Create NPA client when the resource becomes available. */
	npa_resource_available(res_name, npa_res_available_cb, request);

	return request;
}

/**
 * msm_req_update - Updates an existing NPA request. May block.
 * @req:	Request handle
 * @value:	Request value
 */
int msm_req_update(void *req, s32 value)
{
	struct request *request = req;
	int rc = 0;

	if (atomic_read(&request->creation_pending)) {
		pr_err("%s: Error: No client '%s' for resource '%s'.\n",
			__func__, request->client_name, request->resource_name);
		return -ENXIO;
	}

	if (value == MSM_REQ_DEFAULT_VALUE)
		npa_complete_request(request->client);
	else
		rc = npa_issue_required_request(request->client, value);

	return rc;
}

/**
 * msm_req_remove - Removes an existing NPA request. May block.
 * @req:	Request handle
 */
int msm_req_remove(void *req)
{
	struct request *request = req;

	/* Remove client if it's been created. */
	if (!atomic_dec_and_test(&request->creation_pending)) {
		npa_cancel_request(request->client);
		npa_destroy_client(request->client);
		kfree(request);
	}

	return 0;
}

