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

/*
 * SMD RPCROUTER XDR module.
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include <mach/msm_rpcrouter.h>

int xdr_send_uint32(struct msm_rpc_xdr *xdr, const uint32_t *value)
{
	if ((xdr->out_index + sizeof(uint32_t)) > xdr->out_size) {
		pr_err("%s: xdr out buffer full\n", __func__);
		return -1;
	}

	*(uint32_t *)(xdr->out_buf + xdr->out_index) = cpu_to_be32(*value);
	xdr->out_index += sizeof(uint32_t);
	return 0;
}

int xdr_send_int8(struct msm_rpc_xdr *xdr, const int8_t *value)
{
	return xdr_send_uint32(xdr, (uint32_t *)value);
}

int xdr_send_uint8(struct msm_rpc_xdr *xdr, const uint8_t *value)
{
	return xdr_send_uint32(xdr, (uint32_t *)value);
}

int xdr_send_int16(struct msm_rpc_xdr *xdr, const int16_t *value)
{
	return xdr_send_uint32(xdr, (uint32_t *)value);
}

int xdr_send_uint16(struct msm_rpc_xdr *xdr, const uint16_t *value)
{
	return xdr_send_uint32(xdr, (uint32_t *)value);
}

int xdr_send_int32(struct msm_rpc_xdr *xdr, const int32_t *value)
{
	return xdr_send_uint32(xdr, (uint32_t *)value);
}

int xdr_send_bytes(struct msm_rpc_xdr *xdr, const void **data,
		   uint32_t *size)
{
	void *buf = xdr->out_buf + xdr->out_index;
	uint32_t temp;

	if (!size || !data || !*data)
		return -1;

	temp = *size;
	if (temp & 0x3)
		temp += 4 - (temp & 0x3);

	temp += sizeof(uint32_t);
	if ((xdr->out_index + temp) > xdr->out_size) {
		pr_err("%s: xdr out buffer full\n", __func__);
		return -1;
	}

	*((uint32_t *)buf) = cpu_to_be32(*size);
	buf += sizeof(uint32_t);
	memcpy(buf, *data, *size);
	buf += *size;
	if (*size & 0x3) {
		memset(buf, 0, 4 - (*size & 0x3));
		buf += 4 - (*size & 0x3);
	}

	xdr->out_index = buf - xdr->out_buf;
	return 0;
}

int xdr_recv_uint32(struct msm_rpc_xdr *xdr, uint32_t *value)
{
	if ((xdr->in_index + sizeof(uint32_t)) > xdr->in_size) {
		pr_err("%s: xdr in buffer full\n", __func__);
		return -1;
	}

	*value = be32_to_cpu(*(uint32_t *)(xdr->in_buf + xdr->in_index));
	xdr->in_index += sizeof(uint32_t);
	return 0;
}

int xdr_recv_int8(struct msm_rpc_xdr *xdr, int8_t *value)
{
	return xdr_recv_uint32(xdr, (uint32_t *)value);
}

int xdr_recv_uint8(struct msm_rpc_xdr *xdr, uint8_t *value)
{
	return xdr_recv_uint32(xdr, (uint32_t *)value);
}

int xdr_recv_int16(struct msm_rpc_xdr *xdr, int16_t *value)
{
	return xdr_recv_uint32(xdr, (uint32_t *)value);
}

int xdr_recv_uint16(struct msm_rpc_xdr *xdr, uint16_t *value)
{
	return xdr_recv_uint32(xdr, (uint32_t *)value);
}

int xdr_recv_int32(struct msm_rpc_xdr *xdr, int32_t *value)
{
	return xdr_recv_uint32(xdr, (uint32_t *)value);
}

int xdr_recv_bytes(struct msm_rpc_xdr *xdr, void **data,
		   uint32_t *size)
{
	void *buf = xdr->in_buf + xdr->in_index;
	uint32_t temp;

	if (!size || !data)
		return -1;

	*size = be32_to_cpu(*(uint32_t *)buf);
	buf += sizeof(uint32_t);

	temp = *size;
	if (temp & 0x3)
		temp += 4 - (temp & 0x3);

	temp += sizeof(uint32_t);
	if ((xdr->in_index + temp) > xdr->in_size) {
		pr_err("%s: xdr in buffer full\n", __func__);
		return -1;
	}

	if (*size) {
		*data = kmalloc(*size, GFP_KERNEL);
		if (!*data)
			return -1;

		memcpy(*data, buf, *size);

		buf += *size;
		if (*size & 0x3)
			buf += 4 - (*size & 0x3);
	} else
		*data = NULL;

	xdr->in_index = buf - xdr->in_buf;
	return 0;
}

int xdr_send_pointer(struct msm_rpc_xdr *xdr, void **obj,
		     uint32_t obj_size, void *xdr_op)
{
	uint32_t ptr_valid, rc;

	ptr_valid = (*obj != NULL);

	rc = xdr_send_uint32(xdr, &ptr_valid);
	if (rc)
		return rc;

	if (!ptr_valid)
		return 0;

	return ((int (*) (struct msm_rpc_xdr *, void *))xdr_op)(xdr, *obj);
}

int xdr_recv_pointer(struct msm_rpc_xdr *xdr, void **obj,
		     uint32_t obj_size, void *xdr_op)
{
	uint32_t rc, ptr_valid = 0;

	rc = xdr_recv_uint32(xdr, &ptr_valid);
	if (rc)
		return rc;

	if (!ptr_valid) {
		*obj = NULL;
		return 0;
	}

	*obj = kmalloc(obj_size, GFP_KERNEL);
	if (!*obj)
		return -1;

	rc = ((int (*) (struct msm_rpc_xdr *, void *))xdr_op)(xdr, *obj);
	if (rc)
		kfree(*obj);

	return rc;
}

int xdr_send_array(struct msm_rpc_xdr *xdr, void **addr, uint32_t *size,
		   uint32_t maxsize, uint32_t elm_size, void *xdr_op)
{
	int i, rc;
	void *tmp_addr = *addr;

	if (!size || !tmp_addr || (*size > maxsize) || !xdr_op)
		return -1;

	rc = xdr_send_uint32(xdr, size);
	if (rc)
		return rc;

	for (i = 0; i < *size; i++) {
		rc = ((int (*) (struct msm_rpc_xdr *, void *))xdr_op)
			(xdr, tmp_addr);
		if (rc)
			return rc;

		tmp_addr += elm_size;
	}

	return 0;
}

int xdr_recv_array(struct msm_rpc_xdr *xdr, void **addr, uint32_t *size,
		   uint32_t maxsize, uint32_t elm_size, void *xdr_op)
{
	int i, rc;
	void *tmp_addr;

	if (!size || !xdr_op)
		return -1;

	rc = xdr_recv_uint32(xdr, size);
	if (rc)
		return rc;

	if (*size > maxsize)
		return -1;

	tmp_addr = kmalloc((*size * elm_size), GFP_KERNEL);
	if (!tmp_addr)
		return -1;

	*addr = tmp_addr;
	for (i = 0; i < *size; i++) {
		rc = ((int (*) (struct msm_rpc_xdr *, void *))xdr_op)
			(xdr, tmp_addr);
		if (rc) {
			kfree(*addr);
			*addr = NULL;
			return rc;
		}

		tmp_addr += elm_size;
	}

	return 0;
}

int xdr_recv_req(struct msm_rpc_xdr *xdr, struct rpc_request_hdr *req)
{
	int rc = 0;
	if (!req)
		return -1;

	rc |= xdr_recv_uint32(xdr, &req->xid);           /* xid */
	rc |= xdr_recv_uint32(xdr, &req->type);          /* type */
	rc |= xdr_recv_uint32(xdr, &req->rpc_vers);      /* rpc_vers */
	rc |= xdr_recv_uint32(xdr, &req->prog);          /* prog */
	rc |= xdr_recv_uint32(xdr, &req->vers);          /* vers */
	rc |= xdr_recv_uint32(xdr, &req->procedure);     /* procedure */
	rc |= xdr_recv_uint32(xdr, &req->cred_flavor);   /* cred_flavor */
	rc |= xdr_recv_uint32(xdr, &req->cred_length);   /* cred_length */
	rc |= xdr_recv_uint32(xdr, &req->verf_flavor);   /* verf_flavor */
	rc |= xdr_recv_uint32(xdr, &req->verf_length);   /* verf_length */

	return rc;
}

int xdr_recv_reply(struct msm_rpc_xdr *xdr, struct rpc_reply_hdr *reply)
{
	int rc = 0;

	if (!reply)
		return -1;

	rc |= xdr_recv_uint32(xdr, &reply->xid);           /* xid */
	rc |= xdr_recv_uint32(xdr, &reply->type);          /* type */
	rc |= xdr_recv_uint32(xdr, &reply->reply_stat);    /* reply_stat */

	/* acc_hdr */
	if (reply->reply_stat == RPCMSG_REPLYSTAT_ACCEPTED) {
		rc |= xdr_recv_uint32(xdr, &reply->data.acc_hdr.verf_flavor);
		rc |= xdr_recv_uint32(xdr, &reply->data.acc_hdr.verf_length);
		rc |= xdr_recv_uint32(xdr, &reply->data.acc_hdr.accept_stat);
	}

	return rc;
}

int xdr_start_request(struct msm_rpc_xdr *xdr, uint32_t prog,
		      uint32_t ver, uint32_t proc)
{
	mutex_lock(&xdr->out_lock);

	/* TODO: replace below function with its implementation */
	msm_rpc_setup_req((struct rpc_request_hdr *)xdr->out_buf,
			  prog, ver, proc);

	xdr->out_index = sizeof(struct rpc_request_hdr);
	return 0;
}

int xdr_start_accepted_reply(struct msm_rpc_xdr *xdr, uint32_t accept_status)
{
	struct rpc_reply_hdr *reply;

	mutex_lock(&xdr->out_lock);

	/* TODO: err if xdr is not cb xdr */
	reply = (struct rpc_reply_hdr *)xdr->out_buf;

	/* TODO: use xdr functions instead */
	reply->xid = ((struct rpc_request_hdr *)(xdr->in_buf))->xid;
	reply->type = cpu_to_be32(1); /* reply */
	reply->reply_stat = cpu_to_be32(RPCMSG_REPLYSTAT_ACCEPTED);

	reply->data.acc_hdr.accept_stat = cpu_to_be32(accept_status);
	reply->data.acc_hdr.verf_flavor = 0;
	reply->data.acc_hdr.verf_length = 0;

	xdr->out_index = sizeof(*reply);
	return 0;
}

int xdr_send_msg(struct msm_rpc_xdr *xdr)
{
	int rc = 0;

	rc = msm_rpc_write(xdr->ept, xdr->out_buf,
			   xdr->out_index);
	if (rc > 0)
		rc = 0;

	mutex_unlock(&xdr->out_lock);
	return rc;
}

void xdr_init(struct msm_rpc_xdr *xdr)
{
	mutex_init(&xdr->out_lock);
	init_waitqueue_head(&xdr->in_buf_wait_q);

	xdr->in_buf = NULL;
	xdr->in_size = 0;
	xdr->in_index = 0;

	xdr->out_buf = NULL;
	xdr->out_size = 0;
	xdr->out_index = 0;
}

void xdr_init_input(struct msm_rpc_xdr *xdr, void *buf, uint32_t size)
{
	wait_event(xdr->in_buf_wait_q, !(xdr->in_buf));

	xdr->in_buf = buf;
	xdr->in_size = size;
	xdr->in_index = 0;
}

void xdr_init_output(struct msm_rpc_xdr *xdr, void *buf, uint32_t size)
{
	xdr->out_buf = buf;
	xdr->out_size = size;
	xdr->out_index = 0;
}

void xdr_clean_input(struct msm_rpc_xdr *xdr)
{
	kfree(xdr->in_buf);
	xdr->in_size = 0;
	xdr->in_index = 0;
	xdr->in_buf = NULL;

	wake_up(&xdr->in_buf_wait_q);
}

void xdr_clean_output(struct msm_rpc_xdr *xdr)
{
	kfree(xdr->out_buf);
	xdr->out_buf = NULL;
	xdr->out_size = 0;
	xdr->out_index = 0;
}

uint32_t xdr_read_avail(struct msm_rpc_xdr *xdr)
{
	return xdr->in_size;
}
