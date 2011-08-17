/*
   Copyright (c) 2010-2011 Code Aurora Forum.  All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 and
   only version 2 as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>

#include <linux/skbuff.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/timer.h>

#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/err.h>
#include <crypto/hash.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <net/bluetooth/l2cap.h>
#include <net/bluetooth/amp.h>

static struct workqueue_struct *amp_workqueue;

LIST_HEAD(amp_mgr_list);
DEFINE_RWLOCK(amp_mgr_list_lock);

static int send_a2mp(struct socket *sock, u8 *data, int len);

static void ctx_timeout(unsigned long data);

static void launch_ctx(struct amp_mgr *mgr);
static int execute_ctx(struct amp_ctx *ctx, u8 evt_type, void *data);
static int kill_ctx(struct amp_ctx *ctx);
static int cancel_ctx(struct amp_ctx *ctx);

static struct socket *open_fixed_channel(bdaddr_t *src, bdaddr_t *dst);

static void remove_amp_mgr(struct amp_mgr *mgr)
{
	BT_DBG("mgr %p", mgr);

	write_lock_bh(&amp_mgr_list_lock);
	list_del(&mgr->list);
	write_unlock_bh(&amp_mgr_list_lock);

	read_lock_bh(&mgr->ctx_list_lock);
	while (!list_empty(&mgr->ctx_list)) {
		struct amp_ctx *ctx;
		ctx = list_first_entry(&mgr->ctx_list, struct amp_ctx, list);
		read_unlock_bh(&mgr->ctx_list_lock);
		BT_DBG("kill ctx %p", ctx);
		kill_ctx(ctx);
		read_lock_bh(&mgr->ctx_list_lock);
	}
	read_unlock_bh(&mgr->ctx_list_lock);

	kfree(mgr->ctrls);

	kfree(mgr);
}

static struct amp_mgr *get_amp_mgr_sk(struct sock *sk)
{
	struct amp_mgr *mgr;
	struct amp_mgr *found = NULL;

	read_lock_bh(&amp_mgr_list_lock);
	list_for_each_entry(mgr, &amp_mgr_list, list) {
		if ((mgr->a2mp_sock) && (mgr->a2mp_sock->sk == sk)) {
			found = mgr;
			break;
		}
	}
	read_unlock_bh(&amp_mgr_list_lock);
	return found;
}

static struct amp_mgr *get_create_amp_mgr(struct l2cap_conn *conn,
						struct sk_buff *skb)
{
	struct amp_mgr *mgr;

	write_lock_bh(&amp_mgr_list_lock);
	list_for_each_entry(mgr, &amp_mgr_list, list) {
		if (mgr->l2cap_conn == conn) {
			BT_DBG("conn %p found %p", conn, mgr);
			goto gc_finished;
		}
	}

	mgr = kzalloc(sizeof(*mgr), GFP_ATOMIC);
	if (!mgr)
		goto gc_finished;

	mgr->l2cap_conn = conn;
	mgr->next_ident = 1;
	INIT_LIST_HEAD(&mgr->ctx_list);
	rwlock_init(&mgr->ctx_list_lock);
	mgr->skb = skb;
	BT_DBG("conn %p mgr %p", conn, mgr);
	mgr->a2mp_sock = open_fixed_channel(conn->src, conn->dst);
	if (!mgr->a2mp_sock) {
		kfree(mgr);
		goto gc_finished;
	}
	list_add(&(mgr->list), &amp_mgr_list);

gc_finished:
	write_unlock_bh(&amp_mgr_list_lock);
	return mgr;
}

static struct amp_ctrl *get_ctrl(struct amp_mgr *mgr, u8 remote_id)
{
	if ((mgr->ctrls) && (mgr->ctrls->id == remote_id))
		return mgr->ctrls;
	else
		return NULL;
}

static struct amp_ctrl *get_create_ctrl(struct amp_mgr *mgr, u8 id)
{
	struct amp_ctrl *ctrl;

	BT_DBG("mgr %p, id %d", mgr, id);
	if ((mgr->ctrls) && (mgr->ctrls->id == id))
		ctrl = mgr->ctrls;
	else {
		kfree(mgr->ctrls);
		ctrl = kzalloc(sizeof(struct amp_ctrl), GFP_ATOMIC);
		if (ctrl) {
			ctrl->mgr = mgr;
			ctrl->id = id;
		}
		mgr->ctrls = ctrl;
	}

	return ctrl;
}

static struct amp_ctx *create_ctx(u8 type, u8 state)
{
	struct amp_ctx *ctx = NULL;

	ctx = kzalloc(sizeof(*ctx), GFP_ATOMIC);
	if (ctx) {
		ctx->type = type;
		ctx->state = state;
		init_timer(&(ctx->timer));
		ctx->timer.function = ctx_timeout;
		ctx->timer.data = (unsigned long) ctx;
	}
	BT_DBG("ctx %p, type %d", ctx, type);
	return ctx;
}

static inline void start_ctx(struct amp_mgr *mgr, struct amp_ctx *ctx)
{
	BT_DBG("ctx %p", ctx);
	write_lock_bh(&mgr->ctx_list_lock);
	list_add(&ctx->list, &mgr->ctx_list);
	write_unlock_bh(&mgr->ctx_list_lock);
	ctx->mgr = mgr;
	execute_ctx(ctx, AMP_INIT, 0);
}

static void destroy_ctx(struct amp_ctx *ctx)
{
	struct amp_mgr *mgr = ctx->mgr;

	BT_DBG("ctx %p deferred %p", ctx, ctx->deferred);
	del_timer(&ctx->timer);
	write_lock_bh(&mgr->ctx_list_lock);
	list_del(&ctx->list);
	write_unlock_bh(&mgr->ctx_list_lock);
	if (ctx->deferred)
		execute_ctx(ctx->deferred, AMP_INIT, 0);
	kfree(ctx);
}

static struct amp_ctx *get_ctx_mgr(struct amp_mgr *mgr, u8 type)
{
	struct amp_ctx *fnd = NULL;
	struct amp_ctx *ctx;

	read_lock_bh(&mgr->ctx_list_lock);
	list_for_each_entry(ctx, &mgr->ctx_list, list) {
		if (ctx->type == type) {
			fnd = ctx;
			break;
		}
	}
	read_unlock_bh(&mgr->ctx_list_lock);
	return fnd;
}

static struct amp_ctx *get_ctx_type(struct amp_ctx *cur, u8 type)
{
	struct amp_mgr *mgr = cur->mgr;
	struct amp_ctx *fnd = NULL;
	struct amp_ctx *ctx;

	read_lock_bh(&mgr->ctx_list_lock);
	list_for_each_entry(ctx, &mgr->ctx_list, list) {
		if ((ctx->type == type) && (ctx != cur)) {
			fnd = ctx;
			break;
		}
	}
	read_unlock_bh(&mgr->ctx_list_lock);
	return fnd;
}

static struct amp_ctx *get_ctx_a2mp(struct amp_mgr *mgr, u8 ident)
{
	struct amp_ctx *fnd = NULL;
	struct amp_ctx *ctx;

	read_lock_bh(&mgr->ctx_list_lock);
	list_for_each_entry(ctx, &mgr->ctx_list, list) {
		if ((ctx->evt_type & AMP_A2MP_RSP) &&
				(ctx->rsp_ident == ident)) {
			fnd = ctx;
			break;
		}
	}
	read_unlock_bh(&mgr->ctx_list_lock);
	return fnd;
}

static struct amp_ctx *get_ctx_hdev(struct hci_dev *hdev, u8 evt_type,
					u16 evt_value)
{
	struct amp_mgr *mgr;
	struct amp_ctx *fnd = NULL;

	read_lock_bh(&amp_mgr_list_lock);
	list_for_each_entry(mgr, &amp_mgr_list, list) {
		struct amp_ctx *ctx;
		read_lock_bh(&mgr->ctx_list_lock);
		list_for_each_entry(ctx, &mgr->ctx_list, list) {
			struct hci_dev *ctx_hdev;
			ctx_hdev = hci_dev_get(A2MP_HCI_ID(ctx->id));
			if ((ctx_hdev == hdev) && (ctx->evt_type & evt_type)) {
				switch (evt_type) {
				case AMP_HCI_CMD_STATUS:
				case AMP_HCI_CMD_CMPLT:
					if (ctx->opcode == evt_value)
						fnd = ctx;
					break;
				case AMP_HCI_EVENT:
					if (ctx->evt_code == (u8) evt_value)
						fnd = ctx;
					break;
				}
			}
			if (ctx_hdev)
				hci_dev_put(ctx_hdev);

			if (fnd)
				break;
		}
		read_unlock_bh(&mgr->ctx_list_lock);
	}
	read_unlock_bh(&amp_mgr_list_lock);
	return fnd;
}

static inline u8 next_ident(struct amp_mgr *mgr)
{
	if (++mgr->next_ident == 0)
		mgr->next_ident = 1;
	return mgr->next_ident;
}

static inline void send_a2mp_cmd2(struct amp_mgr *mgr, u8 ident, u8 code,
				u16 len, void *data, u16 len2, void *data2)
{
	struct a2mp_cmd_hdr *hdr;
	int plen;
	u8 *p, *cmd;

	BT_DBG("ident %d code 0x%02x", ident, code);
	if (!mgr->a2mp_sock)
		return;
	plen = sizeof(*hdr) + len + len2;
	cmd = kzalloc(plen, GFP_ATOMIC);
	if (!cmd)
		return;
	hdr = (struct a2mp_cmd_hdr *) cmd;
	hdr->code  = code;
	hdr->ident = ident;
	hdr->len   = cpu_to_le16(len+len2);
	p = cmd + sizeof(*hdr);
	memcpy(p, data, len);
	p += len;
	memcpy(p, data2, len2);
	send_a2mp(mgr->a2mp_sock, cmd, plen);
	kfree(cmd);
}

static inline void send_a2mp_cmd(struct amp_mgr *mgr, u8 ident,
				u8 code, u16 len, void *data)
{
	send_a2mp_cmd2(mgr, ident, code, len, data, 0, NULL);
}

static inline int command_rej(struct amp_mgr *mgr, struct sk_buff *skb)
{
	struct a2mp_cmd_hdr *hdr = (struct a2mp_cmd_hdr *) skb->data;
	struct a2mp_cmd_rej *rej;
	struct amp_ctx *ctx;

	BT_DBG("ident %d code %d", hdr->ident, hdr->code);
	rej = (struct a2mp_cmd_rej *) skb_pull(skb, sizeof(*hdr));
	if (skb->len < sizeof(*rej))
		return -EINVAL;
	BT_DBG("reason %d", le16_to_cpu(rej->reason));
	ctx = get_ctx_a2mp(mgr, hdr->ident);
	if (ctx)
		kill_ctx(ctx);
	skb_pull(skb, sizeof(*rej));
	return 0;
}

static int send_a2mp_cl(struct amp_mgr *mgr, u8 ident, u8 code, u16 len,
			void *msg)
{
	struct a2mp_cl clist[16];
	struct a2mp_cl *cl;
	struct hci_dev *hdev;
	int num_ctrls = 1, id;

	cl = clist;
	cl->id  = 0;
	cl->type = 0;
	cl->status = 1;

	for (id = 0; id < 16; ++id) {
		hdev = hci_dev_get(id);
		if (hdev) {
			if ((hdev->amp_type != HCI_BREDR) &&
			test_bit(HCI_UP, &hdev->flags)) {
				(cl + num_ctrls)->id  = HCI_A2MP_ID(hdev->id);
				(cl + num_ctrls)->type = hdev->amp_type;
				(cl + num_ctrls)->status = hdev->amp_status;
				++num_ctrls;
			}
			hci_dev_put(hdev);
		}
	}
	send_a2mp_cmd2(mgr, ident, code, len, msg,
						num_ctrls*sizeof(*cl), clist);

	return 0;
}

static void send_a2mp_change_notify(void)
{
	struct amp_mgr *mgr;

	read_lock_bh(&amp_mgr_list_lock);
	list_for_each_entry(mgr, &amp_mgr_list, list) {
		if (mgr->discovered)
			send_a2mp_cl(mgr, next_ident(mgr),
					A2MP_CHANGE_NOTIFY, 0, NULL);
	}
	read_unlock_bh(&amp_mgr_list_lock);
}

static inline int discover_req(struct amp_mgr *mgr, struct sk_buff *skb)
{
	struct a2mp_cmd_hdr *hdr = (struct a2mp_cmd_hdr *) skb->data;
	struct a2mp_discover_req *req;
	u16 *efm;
	struct a2mp_discover_rsp rsp;

	req = (struct a2mp_discover_req *) skb_pull(skb, sizeof(*hdr));
	if (skb->len < sizeof(*req))
		return -EINVAL;
	efm = (u16 *) skb_pull(skb, sizeof(*req));

	BT_DBG("mtu %d efm 0x%4.4x", le16_to_cpu(req->mtu),
		le16_to_cpu(req->ext_feat));

	while (le16_to_cpu(req->ext_feat) & 0x8000) {
		if (skb->len < sizeof(*efm))
			return -EINVAL;
		req->ext_feat = *efm;
		BT_DBG("efm 0x%4.4x", le16_to_cpu(req->ext_feat));
		efm = (u16 *) skb_pull(skb, sizeof(*efm));
	}

	rsp.mtu = cpu_to_le16(L2CAP_A2MP_DEFAULT_MTU);
	rsp.ext_feat = 0;

	mgr->discovered = 1;

	return send_a2mp_cl(mgr, hdr->ident, A2MP_DISCOVER_RSP,
				sizeof(rsp), &rsp);
}

static inline int change_notify(struct amp_mgr *mgr, struct sk_buff *skb)
{
	struct a2mp_cmd_hdr *hdr = (struct a2mp_cmd_hdr *) skb->data;
	struct a2mp_cl *cl;

	cl = (struct a2mp_cl *) skb_pull(skb, sizeof(*hdr));
	while (skb->len >= sizeof(*cl)) {
		struct amp_ctrl *ctrl;
		if (cl->id != 0) {
			ctrl = get_create_ctrl(mgr, cl->id);
			if (ctrl != NULL) {
				ctrl->type = cl->type;
				ctrl->status = cl->status;
			}
		}
		cl = (struct a2mp_cl *) skb_pull(skb, sizeof(*cl));
	}

	/* TODO find controllers in manager that were not on received */
	/*      controller list and destroy them */
	send_a2mp_cmd(mgr, hdr->ident, A2MP_CHANGE_RSP, 0, NULL);

	return 0;
}

static inline int getinfo_req(struct amp_mgr *mgr, struct sk_buff *skb)
{
	struct a2mp_cmd_hdr *hdr = (struct a2mp_cmd_hdr *) skb->data;
	u8 *data;
	int id;
	struct hci_dev *hdev;
	struct a2mp_getinfo_rsp rsp;

	data = (u8 *) skb_pull(skb, sizeof(*hdr));
	if (le16_to_cpu(hdr->len) < sizeof(*data))
		return -EINVAL;
	if (skb->len < sizeof(*data))
		return -EINVAL;
	id = *data;
	skb_pull(skb, sizeof(*data));
	rsp.id = id;
	rsp.status = 1;

	BT_DBG("id %d", id);
	hdev = hci_dev_get(A2MP_HCI_ID(id));

	if (hdev && hdev->amp_type != HCI_BREDR) {
		rsp.status = 0;
		rsp.total_bw = cpu_to_le32(hdev->amp_total_bw);
		rsp.max_bw = cpu_to_le32(hdev->amp_max_bw);
		rsp.min_latency = cpu_to_le32(hdev->amp_min_latency);
		rsp.pal_cap = cpu_to_le16(hdev->amp_pal_cap);
		rsp.assoc_size = cpu_to_le16(hdev->amp_assoc_size);
	}

	send_a2mp_cmd(mgr, hdr->ident, A2MP_GETINFO_RSP, sizeof(rsp), &rsp);

	if (hdev)
		hci_dev_put(hdev);

	return 0;
}

static void create_physical(struct l2cap_conn *conn, struct sock *sk)
{
	struct amp_mgr *mgr;
	struct amp_ctx *ctx = NULL;

	BT_DBG("conn %p", conn);
	mgr = get_create_amp_mgr(conn, NULL);
	if (!mgr)
		goto cp_finished;
	BT_DBG("mgr %p", mgr);
	ctx = create_ctx(AMP_CREATEPHYSLINK, AMP_CPL_INIT);
	if (!ctx)
		goto cp_finished;
	ctx->sk = sk;
	sock_hold(sk);
	start_ctx(mgr, ctx);
	return;

cp_finished:
	l2cap_amp_physical_complete(-ENOMEM, 0, 0, sk);
}

static void accept_physical(struct l2cap_conn *lcon, u8 id, struct sock *sk)
{
	struct amp_mgr *mgr;
	struct hci_dev *hdev;
	struct hci_conn *conn;
	struct amp_ctx *aplctx = NULL;
	u8 remote_id = 0;
	int result = -EINVAL;

	BT_DBG("lcon %p", lcon);
	mgr = get_create_amp_mgr(lcon, NULL);
	if (!mgr)
		goto ap_finished;
	BT_DBG("mgr %p", mgr);
	hdev = hci_dev_get(A2MP_HCI_ID(id));
	if (!hdev)
		goto ap_finished;
	BT_DBG("hdev %p", hdev);
	conn = hci_conn_hash_lookup_ba(hdev, ACL_LINK,
					&mgr->l2cap_conn->hcon->dst);
	if (conn) {
		BT_DBG("conn %p", hdev);
		result = 0;
		remote_id = conn->dst_id;
		goto ap_finished;
	}
	aplctx = get_ctx_mgr(mgr, AMP_ACCEPTPHYSLINK);
	if (!aplctx)
		goto ap_finished;
	aplctx->sk = sk;
	sock_hold(sk);
	return;

ap_finished:
	l2cap_amp_physical_complete(result, id, remote_id, sk);
}

static int getampassoc_req(struct amp_mgr *mgr, struct sk_buff *skb)
{
	struct a2mp_cmd_hdr *hdr = (struct a2mp_cmd_hdr *) skb->data;
	struct amp_ctx *ctx;
	struct a2mp_getampassoc_req *req;

	if (hdr->len < sizeof(*req))
		return -EINVAL;
	req = (struct a2mp_getampassoc_req *) skb_pull(skb, sizeof(*hdr));
	skb_pull(skb, sizeof(*req));

	ctx = create_ctx(AMP_GETAMPASSOC, AMP_GAA_INIT);
	if (!ctx)
		return -ENOMEM;
	ctx->id = req->id;
	ctx->d.gaa.req_ident = hdr->ident;
	ctx->hdev = hci_dev_get(A2MP_HCI_ID(ctx->id));
	if (ctx->hdev)
		ctx->d.gaa.assoc = kmalloc(ctx->hdev->amp_assoc_size,
						GFP_ATOMIC);
	start_ctx(mgr, ctx);
	return 0;
}

static u8 getampassoc_handler(struct amp_ctx *ctx, u8 evt_type, void *data)
{
	struct sk_buff *skb = (struct sk_buff *) data;
	struct hci_cp_read_local_amp_assoc cp;
	struct hci_rp_read_local_amp_assoc *rp;
	struct a2mp_getampassoc_rsp rsp;
	u16 rem_len;
	u16 frag_len;

	rsp.status = 1;
	if ((evt_type == AMP_KILLED) || (!ctx->hdev) || (!ctx->d.gaa.assoc))
		goto gaa_finished;

	switch (ctx->state) {
	case AMP_GAA_INIT:
		ctx->state = AMP_GAA_RLAA_COMPLETE;
		ctx->evt_type = AMP_HCI_CMD_CMPLT;
		ctx->opcode = HCI_OP_READ_LOCAL_AMP_ASSOC;
		ctx->d.gaa.len_so_far = 0;
		cp.phy_handle = 0;
		cp.len_so_far = 0;
		cp.max_len = ctx->hdev->amp_assoc_size;
		hci_send_cmd(ctx->hdev, ctx->opcode, sizeof(cp), &cp);
		break;

	case AMP_GAA_RLAA_COMPLETE:
		if (skb->len < 4)
			goto gaa_finished;
		rp = (struct hci_rp_read_local_amp_assoc *) skb->data;
		if (rp->status)
			goto gaa_finished;
		rem_len = le16_to_cpu(rp->rem_len);
		skb_pull(skb, 4);
		frag_len = skb->len;

		if (ctx->d.gaa.len_so_far + rem_len <=
				ctx->hdev->amp_assoc_size) {
			struct hci_cp_read_local_amp_assoc cp;
			u8 *assoc = ctx->d.gaa.assoc + ctx->d.gaa.len_so_far;
			memcpy(assoc, rp->frag, frag_len);
			ctx->d.gaa.len_so_far += rem_len;
			rem_len -= frag_len;
			if (rem_len == 0) {
				rsp.status = 0;
				goto gaa_finished;
			}
			/* more assoc data to read */
			cp.phy_handle = 0;
			cp.len_so_far = ctx->d.gaa.len_so_far;
			cp.max_len = ctx->hdev->amp_assoc_size;
			hci_send_cmd(ctx->hdev, ctx->opcode, sizeof(cp), &cp);
		}
		break;

	default:
		goto gaa_finished;
		break;
	}
	return 0;

gaa_finished:
	rsp.id = ctx->id;
	send_a2mp_cmd2(ctx->mgr, ctx->d.gaa.req_ident, A2MP_GETAMPASSOC_RSP,
			sizeof(rsp), &rsp,
			ctx->d.gaa.len_so_far, ctx->d.gaa.assoc);
	kfree(ctx->d.gaa.assoc);
	if (ctx->hdev)
		hci_dev_put(ctx->hdev);
	return 1;
}

struct hmac_sha256_result {
	struct completion completion;
	int err;
};

static void hmac_sha256_final(struct crypto_async_request *req, int err)
{
	struct hmac_sha256_result *r = req->data;
	if (err == -EINPROGRESS)
		return;
	r->err = err;
	complete(&r->completion);
}

int hmac_sha256(u8 *key, u8 ksize, char *plaintext, u8 psize,
		u8 *output, u8 outlen)
{
	int ret = 0;
	struct crypto_ahash *tfm;
	struct scatterlist sg;
	struct ahash_request *req;
	struct hmac_sha256_result tresult;
	void *hash_buff = NULL;

	unsigned char hash_result[64];
	int i;

	memset(output, 0, outlen);

	init_completion(&tresult.completion);

	tfm = crypto_alloc_ahash("hmac(sha256)", CRYPTO_ALG_TYPE_AHASH,
				CRYPTO_ALG_TYPE_AHASH_MASK);
	if (IS_ERR(tfm)) {
		BT_DBG("crypto_alloc_ahash failed");
		ret = PTR_ERR(tfm);
		goto err_tfm;
	}

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		BT_DBG("failed to allocate request for hmac(sha256)");
		ret = -ENOMEM;
		goto err_req;
	}

	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
					hmac_sha256_final, &tresult);

	hash_buff = kzalloc(psize, GFP_KERNEL);
	if (!hash_buff) {
		BT_DBG("failed to kzalloc hash_buff");
		ret = -ENOMEM;
		goto err_hash_buf;
	}

	memset(hash_result, 0, 64);
	memcpy(hash_buff, plaintext, psize);
	sg_init_one(&sg, hash_buff, psize);

	if (ksize) {
		crypto_ahash_clear_flags(tfm, ~0);
		ret = crypto_ahash_setkey(tfm, key, ksize);

		if (ret) {
			BT_DBG("crypto_ahash_setkey failed");
			goto err_setkey;
		}
	}

	ahash_request_set_crypt(req, &sg, hash_result, psize);
	ret = crypto_ahash_digest(req);

	BT_DBG("ret 0x%x", ret);

	switch (ret) {
	case 0:
		for (i = 0; i < outlen; i++)
			output[i] = hash_result[i];
		break;
	case -EINPROGRESS:
	case -EBUSY:
		ret = wait_for_completion_interruptible(&tresult.completion);
		if (!ret && !tresult.err) {
			INIT_COMPLETION(tresult.completion);
			break;
		} else {
			BT_DBG("wait_for_completion_interruptible failed");
			if (!ret)
				ret = tresult.err;
			goto out;
		}
	default:
		goto out;
	}

out:
err_setkey:
	kfree(hash_buff);
err_hash_buf:
	ahash_request_free(req);
err_req:
	crypto_free_ahash(tfm);
err_tfm:
	return ret;
}

static void show_key(u8 *k)
{
	int i = 0;
	for (i = 0; i < 32; i += 8)
		BT_DBG("    %02x %02x %02x %02x %02x %02x %02x %02x",
				*(k+i+0), *(k+i+1), *(k+i+2), *(k+i+3),
				*(k+i+4), *(k+i+5), *(k+i+6), *(k+i+7));
}

static int physlink_security(struct hci_conn *conn, u8 *data, u8 *len, u8 *type)
{
	u8 bt2_key[32];
	u8 gamp_key[32];
	u8 b802_key[32];
	int result;

	if (!hci_conn_check_link_mode(conn))
		return -EACCES;

	BT_DBG("key_type %d", conn->key_type);
	if (conn->key_type < 3)
		return -EACCES;

	*type = conn->key_type;
	*len = 32;
	memcpy(&bt2_key[0], conn->link_key, 16);
	memcpy(&bt2_key[16], conn->link_key, 16);
	result = hmac_sha256(bt2_key, 32, "gamp", 4, gamp_key, 32);
	if (result)
		goto ps_finished;

	if (conn->key_type == 3) {
		BT_DBG("gamp_key");
		show_key(gamp_key);
		memcpy(data, gamp_key, 32);
		goto ps_finished;
	}

	result = hmac_sha256(gamp_key, 32, "802b", 4, b802_key, 32);
	if (result)
		goto ps_finished;

	BT_DBG("802b_key");
	show_key(b802_key);
	memcpy(data, b802_key, 32);

ps_finished:
	return result;
}

static u8 amp_next_handle;
static inline u8 physlink_handle(struct hci_dev *hdev)
{
	/* TODO amp_next_handle should be part of hci_dev */
	if (amp_next_handle == 0)
		amp_next_handle = 1;
	return amp_next_handle++;
}

/* Start an Accept Physical Link sequence */
static int createphyslink_req(struct amp_mgr *mgr, struct sk_buff *skb)
{
	struct a2mp_cmd_hdr *hdr = (struct a2mp_cmd_hdr *) skb->data;
	struct amp_ctx *ctx = NULL;
	struct a2mp_createphyslink_req *req;

	if (hdr->len < sizeof(*req))
		return -EINVAL;
	req = (struct a2mp_createphyslink_req *) skb_pull(skb, sizeof(*hdr));
	skb_pull(skb, sizeof(*req));
	BT_DBG("local_id %d, remote_id %d", req->local_id, req->remote_id);

	/* initialize the context */
	ctx = create_ctx(AMP_ACCEPTPHYSLINK, AMP_APL_INIT);
	if (!ctx)
		return -ENOMEM;
	ctx->d.apl.req_ident = hdr->ident;
	ctx->d.apl.remote_id = req->local_id;
	ctx->id = req->remote_id;

	/* add the supplied remote assoc to the context */
	ctx->d.apl.remote_assoc = kmalloc(skb->len, GFP_ATOMIC);
	if (ctx->d.apl.remote_assoc)
		memcpy(ctx->d.apl.remote_assoc, skb->data, skb->len);
	ctx->d.apl.len_so_far = 0;
	ctx->d.apl.rem_len = skb->len;
	skb_pull(skb, skb->len);
	ctx->hdev = hci_dev_get(A2MP_HCI_ID(ctx->id));
	start_ctx(mgr, ctx);
	return 0;
}

static u8 acceptphyslink_handler(struct amp_ctx *ctx, u8 evt_type, void *data)
{
	struct sk_buff *skb = data;
	struct hci_cp_accept_phys_link acp;
	struct hci_cp_write_remote_amp_assoc wcp;
	struct hci_rp_write_remote_amp_assoc *wrp;
	struct hci_ev_cmd_status *cs = data;
	struct hci_ev_phys_link_complete *ev;
	struct a2mp_createphyslink_rsp rsp;
	struct amp_ctx *cplctx;
	struct amp_ctx *aplctx;
	u16 frag_len;
	struct hci_conn *conn;
	int result;

	BT_DBG("state %d", ctx->state);
	result = -EINVAL;
	rsp.status = 1;        /* Invalid Controller ID */
	if (!ctx->hdev || !test_bit(HCI_UP, &ctx->hdev->flags))
		goto apl_finished;
	if (evt_type == AMP_KILLED) {
		result = -EAGAIN;
		rsp.status = 4;        /* Disconnect request received */
		goto apl_finished;
	}
	if (!ctx->d.apl.remote_assoc) {
		result = -ENOMEM;
		rsp.status = 2;        /* Unable to Start */
		goto apl_finished;
	}

	switch (ctx->state) {
	case AMP_APL_INIT:
		BT_DBG("local_id %d, remote_id %d",
			ctx->id, ctx->d.apl.remote_id);
		conn = hci_conn_hash_lookup_id(ctx->hdev,
					&ctx->mgr->l2cap_conn->hcon->dst,
					ctx->d.apl.remote_id);
		if (conn) {
			result = -EEXIST;
			rsp.status = 5;   /* Already Exists */
			goto apl_finished;
		}

		aplctx = get_ctx_type(ctx, AMP_ACCEPTPHYSLINK);
		if ((aplctx) &&
			(aplctx->d.cpl.remote_id == ctx->d.apl.remote_id)) {
			BT_DBG("deferred to %p", aplctx);
			aplctx->deferred = ctx;
			break;
		}

		cplctx = get_ctx_type(ctx, AMP_CREATEPHYSLINK);
		if ((cplctx) &&
			(cplctx->d.cpl.remote_id == ctx->d.apl.remote_id)) {
			struct hci_conn *bcon = ctx->mgr->l2cap_conn->hcon;
			BT_DBG("local %s remote %s",
				batostr(&bcon->hdev->bdaddr),
				batostr(&bcon->dst));
			if ((cplctx->state < AMP_CPL_PL_COMPLETE) ||
				(bacmp(&bcon->hdev->bdaddr, &bcon->dst) < 0)) {
				BT_DBG("COLLISION LOSER");
				cplctx->deferred = ctx;
				cancel_ctx(cplctx);
				break;
			} else {
				BT_DBG("COLLISION WINNER");
				result = -EISCONN;
				rsp.status = 3;    /* Collision */
				goto apl_finished;
			}
		}

		result = physlink_security(ctx->mgr->l2cap_conn->hcon, acp.data,
						&acp.key_len, &acp.type);
		if (result) {
			BT_DBG("SECURITY");
			rsp.status = 6;    /* Security Violation */
			goto apl_finished;
		}

		ctx->d.apl.phy_handle = physlink_handle(ctx->hdev);
		ctx->state = AMP_APL_APL_STATUS;
		ctx->evt_type = AMP_HCI_CMD_STATUS;
		ctx->opcode = HCI_OP_ACCEPT_PHYS_LINK;
		acp.phy_handle = ctx->d.apl.phy_handle;
		hci_send_cmd(ctx->hdev, ctx->opcode, sizeof(acp), &acp);
		break;

	case AMP_APL_APL_STATUS:
		if (cs->status != 0)
			goto apl_finished;
		/* PAL will accept link, send a2mp response */
		rsp.local_id = ctx->id;
		rsp.remote_id = ctx->d.apl.remote_id;
		rsp.status = 0;
		send_a2mp_cmd(ctx->mgr, ctx->d.apl.req_ident,
				A2MP_CREATEPHYSLINK_RSP, sizeof(rsp), &rsp);

		/* send the first assoc fragment */
		wcp.phy_handle = ctx->d.apl.phy_handle;
		wcp.len_so_far = cpu_to_le16(ctx->d.apl.len_so_far);
		wcp.rem_len = cpu_to_le16(ctx->d.apl.rem_len);
		frag_len = min_t(u16, 248, ctx->d.apl.rem_len);
		memcpy(wcp.frag, ctx->d.apl.remote_assoc, frag_len);
		ctx->state = AMP_APL_WRA_COMPLETE;
		ctx->evt_type = AMP_HCI_CMD_CMPLT;
		ctx->opcode = HCI_OP_WRITE_REMOTE_AMP_ASSOC;
		hci_send_cmd(ctx->hdev, ctx->opcode, 5+frag_len, &wcp);
		break;

	case AMP_APL_WRA_COMPLETE:
		/* received write remote amp assoc command complete event */
		wrp = (struct hci_rp_write_remote_amp_assoc *) skb->data;
		if (wrp->status != 0)
			goto apl_finished;
		if (wrp->phy_handle != ctx->d.apl.phy_handle)
			goto apl_finished;
		/* update progress */
		frag_len = min_t(u16, 248, ctx->d.apl.rem_len);
		ctx->d.apl.len_so_far += frag_len;
		ctx->d.apl.rem_len -= frag_len;
		if (ctx->d.apl.rem_len > 0) {
			u8 *assoc;
			/* another assoc fragment to send */
			wcp.phy_handle = ctx->d.apl.phy_handle;
			wcp.len_so_far = cpu_to_le16(ctx->d.apl.len_so_far);
			wcp.rem_len = cpu_to_le16(ctx->d.apl.rem_len);
			frag_len = min_t(u16, 248, ctx->d.apl.rem_len);
			assoc = ctx->d.apl.remote_assoc + ctx->d.apl.len_so_far;
			memcpy(wcp.frag, assoc, frag_len);
			hci_send_cmd(ctx->hdev, ctx->opcode, 5+frag_len, &wcp);
			break;
		}
		/* wait for physical link complete event */
		ctx->state = AMP_APL_PL_COMPLETE;
		ctx->evt_type = AMP_HCI_EVENT;
		ctx->evt_code = HCI_EV_PHYS_LINK_COMPLETE;
		break;

	case AMP_APL_PL_COMPLETE:
		/* physical link complete event received */
		if (skb->len < sizeof(*ev))
			goto apl_finished;
		ev = (struct hci_ev_phys_link_complete *) skb->data;
		if (ev->phy_handle != ctx->d.apl.phy_handle)
			break;
		if (ev->status != 0)
			goto apl_finished;
		conn = hci_conn_hash_lookup_handle(ctx->hdev, ev->phy_handle);
		if (!conn)
			goto apl_finished;
		result = 0;
		BT_DBG("PL_COMPLETE phy_handle %x", ev->phy_handle);
		conn->dst_id = ctx->d.apl.remote_id;
		bacpy(&conn->dst, &ctx->mgr->l2cap_conn->hcon->dst);
		goto apl_finished;
		break;

	default:
		goto apl_finished;
		break;
	}
	return 0;

apl_finished:
	if (ctx->sk)
		l2cap_amp_physical_complete(result, ctx->id,
					ctx->d.apl.remote_id, ctx->sk);
	if ((result) && (ctx->state < AMP_APL_PL_COMPLETE)) {
		rsp.local_id = ctx->id;
		rsp.remote_id = ctx->d.apl.remote_id;
		send_a2mp_cmd(ctx->mgr, ctx->d.apl.req_ident,
				A2MP_CREATEPHYSLINK_RSP, sizeof(rsp), &rsp);
	}
	kfree(ctx->d.apl.remote_assoc);
	if (ctx->sk)
		sock_put(ctx->sk);
	if (ctx->hdev)
		hci_dev_put(ctx->hdev);
	return 1;
}

static void cancel_cpl_ctx(struct amp_ctx *ctx, u8 reason)
{
	struct hci_cp_disconn_phys_link dcp;

	ctx->state = AMP_CPL_PL_CANCEL;
	ctx->evt_type = AMP_HCI_EVENT;
	ctx->evt_code = HCI_EV_DISCONN_PHYS_LINK_COMPLETE;
	dcp.phy_handle = ctx->d.cpl.phy_handle;
	dcp.reason = reason;
	hci_send_cmd(ctx->hdev, HCI_OP_DISCONN_PHYS_LINK, sizeof(dcp), &dcp);
}

static u8 createphyslink_handler(struct amp_ctx *ctx, u8 evt_type, void *data)
{
	struct amp_ctrl *ctrl;
	struct sk_buff *skb = data;
	struct a2mp_cmd_hdr *hdr;
	struct hci_ev_cmd_status *cs = data;
	struct amp_ctx *cplctx;
	struct a2mp_discover_req dreq;
	struct a2mp_discover_rsp *drsp;
	u16 *efm;
	struct a2mp_getinfo_req greq;
	struct a2mp_getinfo_rsp *grsp;
	struct a2mp_cl *cl;
	struct a2mp_getampassoc_req areq;
	struct a2mp_getampassoc_rsp *arsp;
	struct hci_cp_create_phys_link cp;
	struct hci_cp_write_remote_amp_assoc wcp;
	struct hci_rp_write_remote_amp_assoc *wrp;
	struct hci_ev_channel_selected *cev;
	struct hci_cp_read_local_amp_assoc rcp;
	struct hci_rp_read_local_amp_assoc *rrp;
	struct a2mp_createphyslink_req creq;
	struct a2mp_createphyslink_rsp *crsp;
	struct hci_ev_phys_link_complete *pev;
	struct hci_ev_disconn_phys_link_complete *dev;
	u8 *assoc, *rassoc, *lassoc;
	u16 frag_len;
	u16 rem_len;
	int result = -EAGAIN;
	struct hci_conn *conn;

	BT_DBG("state %d", ctx->state);
	if (evt_type == AMP_KILLED)
		goto cpl_finished;

	if (evt_type == AMP_CANCEL) {
		if ((ctx->state < AMP_CPL_CPL_STATUS) ||
			((ctx->state == AMP_CPL_PL_COMPLETE) &&
			!(ctx->evt_type & AMP_HCI_EVENT)))
			goto cpl_finished;

		cancel_cpl_ctx(ctx, 0x16);
		return 0;
	}

	switch (ctx->state) {
	case AMP_CPL_INIT:
		cplctx = get_ctx_type(ctx, AMP_CREATEPHYSLINK);
		if (cplctx) {
			BT_DBG("deferred to %p", cplctx);
			cplctx->deferred = ctx;
			break;
		}
		ctx->state = AMP_CPL_DISC_RSP;
		ctx->evt_type = AMP_A2MP_RSP;
		ctx->rsp_ident = next_ident(ctx->mgr);
		dreq.mtu = cpu_to_le16(L2CAP_A2MP_DEFAULT_MTU);
		dreq.ext_feat = 0;
		send_a2mp_cmd(ctx->mgr, ctx->rsp_ident, A2MP_DISCOVER_REQ,
							sizeof(dreq), &dreq);
		break;

	case AMP_CPL_DISC_RSP:
		drsp = (struct a2mp_discover_rsp *) skb_pull(skb, sizeof(*hdr));
		if (skb->len < (sizeof(*drsp))) {
			result = -EINVAL;
			goto cpl_finished;
		}

		efm = (u16 *) skb_pull(skb, sizeof(*drsp));
		BT_DBG("mtu %d efm 0x%4.4x", le16_to_cpu(drsp->mtu),
						le16_to_cpu(drsp->ext_feat));

		while (le16_to_cpu(drsp->ext_feat) & 0x8000) {
			if (skb->len < sizeof(*efm)) {
				result = -EINVAL;
				goto cpl_finished;
			}
			drsp->ext_feat = *efm;
			BT_DBG("efm 0x%4.4x", le16_to_cpu(drsp->ext_feat));
			efm = (u16 *) skb_pull(skb, sizeof(*efm));
		}
		cl = (struct a2mp_cl *) efm;

		/* find the first remote and local controller with the
		 * same type
		 */
		greq.id = 0;
		result = -ENODEV;
		while (skb->len >= sizeof(*cl)) {
			if ((cl->id != 0) && (greq.id == 0)) {
				struct hci_dev *hdev;
				hdev = hci_dev_get_type(cl->type);
				if (hdev) {
					struct hci_conn *conn;
					ctx->hdev = hdev;
					ctx->id = HCI_A2MP_ID(hdev->id);
					ctx->d.cpl.remote_id = cl->id;
					conn = hci_conn_hash_lookup_ba(hdev,
					    ACL_LINK,
					    &ctx->mgr->l2cap_conn->hcon->dst);
					if (conn) {
						BT_DBG("PL_COMPLETE exists %x",
							(int) conn->handle);
						result = 0;
					}
					ctrl = get_create_ctrl(ctx->mgr,
								cl->id);
					if (ctrl) {
						ctrl->type = cl->type;
						ctrl->status = cl->status;
					}
					greq.id = cl->id;
				}
			}
			cl = (struct a2mp_cl *) skb_pull(skb, sizeof(*cl));
		}
		if ((!greq.id) || (!result))
			goto cpl_finished;
		ctx->state = AMP_CPL_GETINFO_RSP;
		ctx->evt_type = AMP_A2MP_RSP;
		ctx->rsp_ident = next_ident(ctx->mgr);
		send_a2mp_cmd(ctx->mgr, ctx->rsp_ident, A2MP_GETINFO_REQ,
							sizeof(greq), &greq);
		break;

	case AMP_CPL_GETINFO_RSP:
		if (skb->len < sizeof(*grsp))
			goto cpl_finished;
		grsp = (struct a2mp_getinfo_rsp *) skb_pull(skb, sizeof(*hdr));
		if (grsp->status)
			goto cpl_finished;
		if (grsp->id != ctx->d.cpl.remote_id)
			goto cpl_finished;
		ctrl = get_ctrl(ctx->mgr, grsp->id);
		if (!ctrl)
			goto cpl_finished;
		ctrl->status = grsp->status;
		ctrl->total_bw = le32_to_cpu(grsp->total_bw);
		ctrl->max_bw = le32_to_cpu(grsp->max_bw);
		ctrl->min_latency = le32_to_cpu(grsp->min_latency);
		ctrl->pal_cap = le16_to_cpu(grsp->pal_cap);
		ctrl->max_assoc_size = le16_to_cpu(grsp->assoc_size);
		skb_pull(skb, sizeof(*grsp));

		ctx->d.cpl.max_len = ctrl->max_assoc_size;

		/* setup up GAA request */
		areq.id = ctx->d.cpl.remote_id;

		/* advance context state */
		ctx->state = AMP_CPL_GAA_RSP;
		ctx->evt_type = AMP_A2MP_RSP;
		ctx->rsp_ident = next_ident(ctx->mgr);
		send_a2mp_cmd(ctx->mgr, ctx->rsp_ident, A2MP_GETAMPASSOC_REQ,
							sizeof(areq), &areq);
		break;

	case AMP_CPL_GAA_RSP:
		if (skb->len < sizeof(*arsp))
			goto cpl_finished;
		hdr = (void *) skb->data;
		arsp = (void *) skb_pull(skb, sizeof(*hdr));
		if (arsp->id != ctx->d.cpl.remote_id)
			goto cpl_finished;
		if (arsp->status != 0)
			goto cpl_finished;

		/* store away remote assoc */
		assoc = (u8 *) skb_pull(skb, sizeof(*arsp));
		ctx->d.cpl.len_so_far = 0;
		ctx->d.cpl.rem_len = hdr->len - sizeof(*arsp);
		rassoc = kmalloc(ctx->d.cpl.rem_len, GFP_ATOMIC);
		if (!rassoc)
			goto cpl_finished;
		memcpy(rassoc, assoc, ctx->d.cpl.rem_len);
		ctx->d.cpl.remote_assoc = rassoc;
		skb_pull(skb, ctx->d.cpl.rem_len);

		/* set up CPL command */
		ctx->d.cpl.phy_handle = physlink_handle(ctx->hdev);
		cp.phy_handle = ctx->d.cpl.phy_handle;
		if (physlink_security(ctx->mgr->l2cap_conn->hcon, cp.data,
					&cp.key_len, &cp.type)) {
			result = -EPERM;
			goto cpl_finished;
		}

		/* advance context state */
		ctx->state = AMP_CPL_CPL_STATUS;
		ctx->evt_type = AMP_HCI_CMD_STATUS;
		ctx->opcode = HCI_OP_CREATE_PHYS_LINK;
		hci_send_cmd(ctx->hdev, ctx->opcode, sizeof(cp), &cp);
		break;

	case AMP_CPL_CPL_STATUS:
		/* received create physical link command status */
		if (cs->status != 0)
			goto cpl_finished;
		/* send the first assoc fragment */
		wcp.phy_handle = ctx->d.cpl.phy_handle;
		wcp.len_so_far = ctx->d.cpl.len_so_far;
		wcp.rem_len = cpu_to_le16(ctx->d.cpl.rem_len);
		frag_len = min_t(u16, 248, ctx->d.cpl.rem_len);
		memcpy(wcp.frag, ctx->d.cpl.remote_assoc, frag_len);
		ctx->state = AMP_CPL_WRA_COMPLETE;
		ctx->evt_type = AMP_HCI_CMD_CMPLT;
		ctx->opcode = HCI_OP_WRITE_REMOTE_AMP_ASSOC;
		hci_send_cmd(ctx->hdev, ctx->opcode, 5+frag_len, &wcp);
		break;

	case AMP_CPL_WRA_COMPLETE:
		/* received write remote amp assoc command complete event */
		if (skb->len < sizeof(*wrp))
			goto cpl_finished;
		wrp = (struct hci_rp_write_remote_amp_assoc *) skb->data;
		if (wrp->status != 0)
			goto cpl_finished;
		if (wrp->phy_handle != ctx->d.cpl.phy_handle)
			goto cpl_finished;

		/* update progress */
		frag_len = min_t(u16, 248, ctx->d.cpl.rem_len);
		ctx->d.cpl.len_so_far += frag_len;
		ctx->d.cpl.rem_len -= frag_len;
		if (ctx->d.cpl.rem_len > 0) {
			/* another assoc fragment to send */
			wcp.phy_handle = ctx->d.cpl.phy_handle;
			wcp.len_so_far = cpu_to_le16(ctx->d.cpl.len_so_far);
			wcp.rem_len = cpu_to_le16(ctx->d.cpl.rem_len);
			frag_len = min_t(u16, 248, ctx->d.cpl.rem_len);
			memcpy(wcp.frag,
				ctx->d.cpl.remote_assoc + ctx->d.cpl.len_so_far,
				frag_len);
			hci_send_cmd(ctx->hdev, ctx->opcode, 5+frag_len, &wcp);
			break;
		}
		/* now wait for channel selected event */
		ctx->state = AMP_CPL_CHANNEL_SELECT;
		ctx->evt_type = AMP_HCI_EVENT;
		ctx->evt_code = HCI_EV_CHANNEL_SELECTED;
		break;

	case AMP_CPL_CHANNEL_SELECT:
		/* received channel selection event */
		if (skb->len < sizeof(*cev))
			goto cpl_finished;
		cev = (void *) skb->data;
/* TODO - PK This check is valid but Libra PAL returns 0 for handle during
			Create Physical Link collision scenario
		if (cev->phy_handle != ctx->d.cpl.phy_handle)
			goto cpl_finished;
*/

		/* request the first local assoc fragment */
		rcp.phy_handle = ctx->d.cpl.phy_handle;
		rcp.len_so_far = 0;
		rcp.max_len = ctx->d.cpl.max_len;
		lassoc = kmalloc(ctx->d.cpl.max_len, GFP_ATOMIC);
		if (!lassoc)
			goto cpl_finished;
		ctx->d.cpl.local_assoc = lassoc;
		ctx->d.cpl.len_so_far = 0;
		ctx->state = AMP_CPL_RLA_COMPLETE;
		ctx->evt_type = AMP_HCI_CMD_CMPLT;
		ctx->opcode = HCI_OP_READ_LOCAL_AMP_ASSOC;
		hci_send_cmd(ctx->hdev, ctx->opcode, sizeof(rcp), &rcp);
		break;

	case AMP_CPL_RLA_COMPLETE:
		/* received read local amp assoc command complete event */
		if (skb->len < 4)
			goto cpl_finished;
		rrp = (struct hci_rp_read_local_amp_assoc *) skb->data;
		if (rrp->status)
			goto cpl_finished;
		if (rrp->phy_handle != ctx->d.cpl.phy_handle)
			goto cpl_finished;
		rem_len = le16_to_cpu(rrp->rem_len);
		skb_pull(skb, 4);
		frag_len = skb->len;

		if (ctx->d.cpl.len_so_far + rem_len > ctx->d.cpl.max_len)
			goto cpl_finished;

		/* save this fragment in context */
		lassoc = ctx->d.cpl.local_assoc + ctx->d.cpl.len_so_far;
		memcpy(lassoc, rrp->frag, frag_len);
		ctx->d.cpl.len_so_far += frag_len;
		rem_len -= frag_len;
		if (rem_len > 0) {
			/* request another local assoc fragment */
			rcp.phy_handle = ctx->d.cpl.phy_handle;
			rcp.len_so_far = ctx->d.cpl.len_so_far;
			rcp.max_len = ctx->d.cpl.max_len;
			hci_send_cmd(ctx->hdev, ctx->opcode, sizeof(rcp), &rcp);
		} else {
			creq.local_id = ctx->id;
			creq.remote_id = ctx->d.cpl.remote_id;
			/* wait for A2MP rsp AND phys link complete event */
			ctx->state = AMP_CPL_PL_COMPLETE;
			ctx->evt_type = AMP_A2MP_RSP | AMP_HCI_EVENT;
			ctx->rsp_ident = next_ident(ctx->mgr);
			ctx->evt_code = HCI_EV_PHYS_LINK_COMPLETE;
			send_a2mp_cmd2(ctx->mgr, ctx->rsp_ident,
				A2MP_CREATEPHYSLINK_REQ, sizeof(creq), &creq,
				ctx->d.cpl.len_so_far, ctx->d.cpl.local_assoc);
		}
		break;

	case AMP_CPL_PL_COMPLETE:
		if (evt_type == AMP_A2MP_RSP) {
			/* create physical link response received */
			ctx->evt_type &= ~AMP_A2MP_RSP;
			if (skb->len < sizeof(*crsp))
				goto cpl_finished;
			crsp = (void *) skb_pull(skb, sizeof(*hdr));
			if ((crsp->local_id != ctx->d.cpl.remote_id) ||
				(crsp->remote_id != ctx->id) ||
				(crsp->status != 0)) {
				cancel_cpl_ctx(ctx, 0x13);
				break;
			}

			/* notify Qualcomm PAL */
			if (ctx->hdev->manufacturer == 0x001d)
				hci_send_cmd(ctx->hdev,
					hci_opcode_pack(0x3f, 0x00), 0, NULL);
		}
		if (evt_type == AMP_HCI_EVENT) {
			ctx->evt_type &= ~AMP_HCI_EVENT;
			/* physical link complete event received */
			if (skb->len < sizeof(*pev))
				goto cpl_finished;
			pev = (void *) skb->data;
			if (pev->phy_handle != ctx->d.cpl.phy_handle)
				break;
			if (pev->status != 0)
				goto cpl_finished;
		}
		if (ctx->evt_type)
			break;
		conn = hci_conn_hash_lookup_handle(ctx->hdev,
							ctx->d.cpl.phy_handle);
		if (!conn)
			goto cpl_finished;
		result = 0;
		BT_DBG("PL_COMPLETE phy_handle %x", ctx->d.cpl.phy_handle);
		bacpy(&conn->dst, &ctx->mgr->l2cap_conn->hcon->dst);
		conn->dst_id = ctx->d.cpl.remote_id;
		conn->out = 1;
		goto cpl_finished;
		break;

	case AMP_CPL_PL_CANCEL:
		dev = (void *) skb->data;
		BT_DBG("PL_COMPLETE cancelled %x", dev->phy_handle);
		result = -EISCONN;
		goto cpl_finished;
		break;

	default:
		goto cpl_finished;
		break;
	}
	return 0;

cpl_finished:
	l2cap_amp_physical_complete(result, ctx->id, ctx->d.cpl.remote_id,
					ctx->sk);
	if (ctx->sk)
		sock_put(ctx->sk);
	if (ctx->hdev)
		hci_dev_put(ctx->hdev);
	kfree(ctx->d.cpl.remote_assoc);
	kfree(ctx->d.cpl.local_assoc);
	return 1;
}

static int disconnphyslink_req(struct amp_mgr *mgr, struct sk_buff *skb)
{
	struct a2mp_cmd_hdr *hdr = (void *) skb->data;
	struct a2mp_disconnphyslink_req *req;
	struct a2mp_disconnphyslink_rsp rsp;
	struct hci_dev *hdev;
	struct hci_conn *conn;
	struct amp_ctx *aplctx;

	BT_DBG("mgr %p skb %p", mgr, skb);
	if (hdr->len < sizeof(*req))
		return -EINVAL;
	req = (void *) skb_pull(skb, sizeof(*hdr));
	skb_pull(skb, sizeof(*req));

	rsp.local_id = req->remote_id;
	rsp.remote_id = req->local_id;
	rsp.status = 0;
	BT_DBG("local_id %d remote_id %d",
		(int) rsp.local_id, (int) rsp.remote_id);
	hdev = hci_dev_get(A2MP_HCI_ID(rsp.local_id));
	if (!hdev) {
		rsp.status = 1; /* Invalid Controller ID */
		goto dpl_finished;
	}
	BT_DBG("hdev %p", hdev);
	conn = hci_conn_hash_lookup_ba(hdev, ACL_LINK,
					&mgr->l2cap_conn->hcon->dst);
	if (!conn) {
		aplctx = get_ctx_mgr(mgr, AMP_ACCEPTPHYSLINK);
		if (aplctx) {
			kill_ctx(aplctx);
			rsp.status = 0;
			goto dpl_finished;
		}
		rsp.status = 2;  /* No Physical Link exists */
		goto dpl_finished;
	}
	BT_DBG("conn %p", conn);
	hci_disconnect(conn, 0x13);

dpl_finished:
	send_a2mp_cmd(mgr, hdr->ident,
				A2MP_DISCONNPHYSLINK_RSP, sizeof(rsp), &rsp);
	if (hdev)
		hci_dev_put(hdev);
	return 0;
}

static int execute_ctx(struct amp_ctx *ctx, u8 evt_type, void *data)
{
	struct amp_mgr *mgr = ctx->mgr;
	u8 finished = 0;

	if (!mgr->connected)
		return 0;

	switch (ctx->type) {
	case AMP_GETAMPASSOC:
		finished = getampassoc_handler(ctx, evt_type, data);
		break;
	case AMP_CREATEPHYSLINK:
		finished = createphyslink_handler(ctx, evt_type, data);
		break;
	case AMP_ACCEPTPHYSLINK:
		finished = acceptphyslink_handler(ctx, evt_type, data);
		break;
	}

	if (!finished)
		mod_timer(&(ctx->timer), jiffies +
			msecs_to_jiffies(A2MP_RSP_TIMEOUT));
	else
		destroy_ctx(ctx);
	return finished;
}

static int cancel_ctx(struct amp_ctx *ctx)
{
	return execute_ctx(ctx, AMP_CANCEL, 0);
}

static int kill_ctx(struct amp_ctx *ctx)
{
	return execute_ctx(ctx, AMP_KILLED, 0);
}

static void ctx_timeout_worker(struct work_struct *w)
{
	struct amp_work_ctx_timeout *work = (struct amp_work_ctx_timeout *) w;
	struct amp_ctx *ctx = work->ctx;
	kill_ctx(ctx);
	kfree(work);
}

static void ctx_timeout(unsigned long data)
{
	struct amp_ctx *ctx = (struct amp_ctx *) data;
	struct amp_work_ctx_timeout *work;

	BT_DBG("ctx %p", ctx);
	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (work) {
		INIT_WORK((struct work_struct *) work, ctx_timeout_worker);
		work->ctx = ctx;
		if (queue_work(amp_workqueue, (struct work_struct *) work) == 0)
			kfree(work);
	}
}

static void launch_ctx(struct amp_mgr *mgr)
{
	struct amp_ctx *ctx = NULL;

	BT_DBG("mgr %p", mgr);
	read_lock_bh(&mgr->ctx_list_lock);
	if (!list_empty(&mgr->ctx_list))
		ctx = list_first_entry(&mgr->ctx_list, struct amp_ctx, list);
	read_unlock_bh(&mgr->ctx_list_lock);
	BT_DBG("ctx %p", ctx);
	if (ctx)
		execute_ctx(ctx, AMP_INIT, NULL);
}

static inline int a2mp_rsp(struct amp_mgr *mgr, struct sk_buff *skb)
{
	struct amp_ctx *ctx;
	struct a2mp_cmd_hdr *hdr = (struct a2mp_cmd_hdr *) skb->data;
	u16 hdr_len = le16_to_cpu(hdr->len);

	/* find context waiting for A2MP rsp with this rsp's identifier */
	BT_DBG("ident %d code %d", hdr->ident, hdr->code);
	ctx = get_ctx_a2mp(mgr, hdr->ident);
	if (ctx) {
		execute_ctx(ctx, AMP_A2MP_RSP, skb);
	} else {
		BT_DBG("context not found");
		skb_pull(skb, sizeof(*hdr));
		if (hdr_len > skb->len)
			hdr_len = skb->len;
		skb_pull(skb, hdr_len);
	}
	return 0;
}

/* L2CAP-A2MP interface */

void a2mp_receive(struct sock *sk, struct sk_buff *skb)
{
	struct a2mp_cmd_hdr *hdr = (struct a2mp_cmd_hdr *) skb->data;
	int len;
	int err = 0;
	struct amp_mgr *mgr;

	mgr = get_amp_mgr_sk(sk);
	if (!mgr)
		goto a2mp_finished;

	len = skb->len;
	while (len >= sizeof(*hdr)) {
		struct a2mp_cmd_hdr *hdr = (struct a2mp_cmd_hdr *) skb->data;
		u16 clen = le16_to_cpu(hdr->len);

		BT_DBG("code 0x%02x id %d len %d", hdr->code, hdr->ident, clen);
		if (clen > len || !hdr->ident) {
			err = -EINVAL;
			break;
		}
		switch (hdr->code) {
		case A2MP_COMMAND_REJ:
			command_rej(mgr, skb);
			break;
		case A2MP_DISCOVER_REQ:
			err = discover_req(mgr, skb);
			break;
		case A2MP_CHANGE_NOTIFY:
			err = change_notify(mgr, skb);
			break;
		case A2MP_GETINFO_REQ:
			err = getinfo_req(mgr, skb);
			break;
		case A2MP_GETAMPASSOC_REQ:
			err = getampassoc_req(mgr, skb);
			break;
		case A2MP_CREATEPHYSLINK_REQ:
			err = createphyslink_req(mgr, skb);
			break;
		case A2MP_DISCONNPHYSLINK_REQ:
			err = disconnphyslink_req(mgr, skb);
			break;
		case A2MP_CHANGE_RSP:
		case A2MP_DISCOVER_RSP:
		case A2MP_GETINFO_RSP:
		case A2MP_GETAMPASSOC_RSP:
		case A2MP_CREATEPHYSLINK_RSP:
		case A2MP_DISCONNPHYSLINK_RSP:
			err = a2mp_rsp(mgr, skb);
			break;
		default:
			BT_ERR("Unknown A2MP signaling command 0x%2.2x",
				hdr->code);
			skb_pull(skb, sizeof(*hdr));
			err = -EINVAL;
			break;
		}
		len = skb->len;
	}

a2mp_finished:
	if (err && mgr) {
		struct a2mp_cmd_rej rej;
		rej.reason = cpu_to_le16(0);
		send_a2mp_cmd(mgr, hdr->ident, A2MP_COMMAND_REJ,
							sizeof(rej), &rej);
	}
}

/* L2CAP-A2MP interface */

static int send_a2mp(struct socket *sock, u8 *data, int len)
{
	struct kvec iv = { data, len };
	struct msghdr msg;

	memset(&msg, 0, sizeof(msg));

	return kernel_sendmsg(sock, &msg, &iv, 1, len);
}

static void data_ready_worker(struct work_struct *w)
{
	struct amp_work_data_ready *work = (struct amp_work_data_ready *) w;
	struct sock *sk = work->sk;
	struct sk_buff *skb;

	/* skb_dequeue() is thread-safe */
	while ((skb = skb_dequeue(&sk->sk_receive_queue))) {
		a2mp_receive(sk, skb);
		kfree_skb(skb);
	}
	sock_put(work->sk);
	kfree(work);
}

static void data_ready(struct sock *sk, int bytes)
{
	struct amp_work_data_ready *work;
	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (work) {
		INIT_WORK((struct work_struct *) work, data_ready_worker);
		sock_hold(sk);
		work->sk = sk;
		work->bytes = bytes;
		if (!queue_work(amp_workqueue, (struct work_struct *) work)) {
			kfree(work);
			sock_put(sk);
		}
	}
}

static void state_change_worker(struct work_struct *w)
{
	struct amp_work_state_change *work = (struct amp_work_state_change *) w;
	struct amp_mgr *mgr;
	switch (work->sk->sk_state) {
	case BT_CONNECTED:
		/* socket is up */
		BT_DBG("CONNECTED");
		mgr = get_amp_mgr_sk(work->sk);
		if (mgr) {
			mgr->connected = 1;
			if (mgr->skb) {
				l2cap_recv_deferred_frame(work->sk, mgr->skb);
				mgr->skb = NULL;
			}
			launch_ctx(mgr);
		}
		break;

	case BT_CLOSED:
		/* connection is gone */
		BT_DBG("CLOSED");
		mgr = get_amp_mgr_sk(work->sk);
		if (mgr) {
			if (!sock_flag(work->sk, SOCK_DEAD))
				sock_release(mgr->a2mp_sock);
			mgr->a2mp_sock = NULL;
			remove_amp_mgr(mgr);
		}
		break;

	default:
		/* something else happened */
		break;
	}
	sock_put(work->sk);
	kfree(work);
}

static void state_change(struct sock *sk)
{
	struct amp_work_state_change *work;
	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (work) {
		INIT_WORK((struct work_struct *) work, state_change_worker);
		sock_hold(sk);
		work->sk = sk;
		if (!queue_work(amp_workqueue, (struct work_struct *) work)) {
			kfree(work);
			sock_put(sk);
		}
	}
}

static struct socket *open_fixed_channel(bdaddr_t *src, bdaddr_t *dst)
{
	int err;
	struct socket *sock;
	struct sockaddr_l2 addr;
	struct sock *sk;
	struct l2cap_options opts = {L2CAP_A2MP_DEFAULT_MTU,
			L2CAP_A2MP_DEFAULT_MTU, L2CAP_DEFAULT_FLUSH_TO,
			L2CAP_MODE_ERTM, 1, 0xFF, 1};


	err = sock_create_kern(PF_BLUETOOTH, SOCK_SEQPACKET,
					BTPROTO_L2CAP, &sock);

	if (err) {
		BT_ERR("sock_create_kern failed %d", err);
		return NULL;
	}

	sk = sock->sk;
	sk->sk_data_ready = data_ready;
	sk->sk_state_change = state_change;

	memset(&addr, 0, sizeof(addr));
	bacpy(&addr.l2_bdaddr, src);
	addr.l2_family = AF_BLUETOOTH;
	addr.l2_cid = L2CAP_CID_A2MP;
	err = kernel_bind(sock, (struct sockaddr *) &addr, sizeof(addr));
	if (err) {
		BT_ERR("kernel_bind failed %d", err);
		sock_release(sock);
		return NULL;
	}

	l2cap_fixed_channel_config(sk, &opts);

	memset(&addr, 0, sizeof(addr));
	bacpy(&addr.l2_bdaddr, dst);
	addr.l2_family = AF_BLUETOOTH;
	addr.l2_cid = L2CAP_CID_A2MP;
	err = kernel_connect(sock, (struct sockaddr *) &addr, sizeof(addr),
							O_NONBLOCK);
	if ((err == 0) || (err == -EINPROGRESS))
		return sock;
	else {
		BT_ERR("kernel_connect failed %d", err);
		sock_release(sock);
		return NULL;
	}
}

static void conn_ind_worker(struct work_struct *w)
{
	struct amp_work_conn_ind *work = (struct amp_work_conn_ind *) w;
	struct l2cap_conn *conn = work->conn;
	struct sk_buff *skb = work->skb;
	struct amp_mgr *mgr;

	mgr = get_create_amp_mgr(conn, skb);
	BT_DBG("mgr %p", mgr);
	kfree(work);
}

static void create_physical_worker(struct work_struct *w)
{
	struct amp_work_create_physical *work =
		(struct amp_work_create_physical *) w;

	create_physical(work->conn, work->sk);
	sock_put(work->sk);
	kfree(work);
}

static void accept_physical_worker(struct work_struct *w)
{
	struct amp_work_accept_physical *work =
		(struct amp_work_accept_physical *) w;

	accept_physical(work->conn, work->id, work->sk);
	sock_put(work->sk);
	kfree(work);
}

/* L2CAP Fixed Channel interface */

void amp_conn_ind(struct l2cap_conn *conn, struct sk_buff *skb)
{
	struct amp_work_conn_ind *work;
	BT_DBG("conn %p, skb %p", conn, skb);
	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (work) {
		INIT_WORK((struct work_struct *) work, conn_ind_worker);
		work->conn = conn;
		work->skb = skb;
		if (queue_work(amp_workqueue, (struct work_struct *) work) == 0)
			kfree(work);
	}
}

/* L2CAP Physical Link interface */

void amp_create_physical(struct l2cap_conn *conn, struct sock *sk)
{
	struct amp_work_create_physical *work;
	BT_DBG("conn %p", conn);
	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (work) {
		INIT_WORK((struct work_struct *) work, create_physical_worker);
		work->conn = conn;
		work->sk = sk;
		sock_hold(sk);
		if (!queue_work(amp_workqueue, (struct work_struct *) work)) {
			sock_put(sk);
			kfree(work);
		}
	}
}

void amp_accept_physical(struct l2cap_conn *conn, u8 id, struct sock *sk)
{
	struct amp_work_accept_physical *work;
	BT_DBG("conn %p", conn);

	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (work) {
		INIT_WORK((struct work_struct *) work, accept_physical_worker);
		work->conn = conn;
		work->sk = sk;
		work->id = id;
		sock_hold(sk);
		if (!queue_work(amp_workqueue, (struct work_struct *) work)) {
			sock_put(sk);
			kfree(work);
		}
	}
}

/* HCI interface */

static void amp_cmd_cmplt_worker(struct work_struct *w)
{
	struct amp_work_cmd_cmplt *work = (struct amp_work_cmd_cmplt *) w;
	struct hci_dev *hdev = work->hdev;
	u16 opcode = work->opcode;
	struct sk_buff *skb = work->skb;
	struct amp_ctx *ctx;

	ctx = get_ctx_hdev(hdev, AMP_HCI_CMD_CMPLT, opcode);
	if (ctx)
		execute_ctx(ctx, AMP_HCI_CMD_CMPLT, skb);
	kfree_skb(skb);
	kfree(w);
}

static void amp_cmd_cmplt_evt(struct hci_dev *hdev, u16 opcode,
				struct sk_buff *skb)
{
	struct amp_work_cmd_cmplt *work;
	struct sk_buff *skbc;
	BT_DBG("hdev %p opcode 0x%x skb %p len %d",
		hdev, opcode, skb, skb->len);
	skbc = skb_clone(skb, GFP_ATOMIC);
	if (!skbc)
		return;
	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (work) {
		INIT_WORK((struct work_struct *) work, amp_cmd_cmplt_worker);
		work->hdev = hdev;
		work->opcode = opcode;
		work->skb = skbc;
		if (queue_work(amp_workqueue, (struct work_struct *) work) == 0)
			kfree(work);
	}
}

static void amp_cmd_status_worker(struct work_struct *w)
{
	struct amp_work_cmd_status *work = (struct amp_work_cmd_status *) w;
	struct hci_dev *hdev = work->hdev;
	u16 opcode = work->opcode;
	u8 status = work->status;
	struct amp_ctx *ctx;

	ctx = get_ctx_hdev(hdev, AMP_HCI_CMD_STATUS, opcode);
	if (ctx)
		execute_ctx(ctx, AMP_HCI_CMD_STATUS, &status);
	kfree(w);
}

static void amp_cmd_status_evt(struct hci_dev *hdev, u16 opcode, u8 status)
{
	struct amp_work_cmd_status *work;
	BT_DBG("hdev %p opcode 0x%x status %d", hdev, opcode, status);
	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (work) {
		INIT_WORK((struct work_struct *) work, amp_cmd_status_worker);
		work->hdev = hdev;
		work->opcode = opcode;
		work->status = status;
		if (queue_work(amp_workqueue, (struct work_struct *) work) == 0)
			kfree(work);
	}
}

static void amp_event_worker(struct work_struct *w)
{
	struct amp_work_event *work = (struct amp_work_event *) w;
	struct hci_dev *hdev = work->hdev;
	u8 event = work->event;
	struct sk_buff *skb = work->skb;
	struct amp_ctx *ctx;

	if (event == HCI_EV_AMP_STATUS_CHANGE) {
		struct hci_ev_amp_status_change *ev;
		if (skb->len < sizeof(*ev))
			goto amp_event_finished;
		ev = (void *) skb->data;
		if (ev->status != 0)
			goto amp_event_finished;
		if (ev->amp_status == hdev->amp_status)
			goto amp_event_finished;
		hdev->amp_status = ev->amp_status;
		send_a2mp_change_notify();
		goto amp_event_finished;
	}
	ctx = get_ctx_hdev(hdev, AMP_HCI_EVENT, (u16) event);
	if (ctx)
		execute_ctx(ctx, AMP_HCI_EVENT, skb);

amp_event_finished:
	kfree_skb(skb);
	kfree(w);
}

static void amp_evt(struct hci_dev *hdev, u8 event, struct sk_buff *skb)
{
	struct amp_work_event *work;
	struct sk_buff *skbc;
	BT_DBG("hdev %p event 0x%x skb %p", hdev, event, skb);
	skbc = skb_clone(skb, GFP_ATOMIC);
	if (!skbc)
		return;
	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (work) {
		INIT_WORK((struct work_struct *) work, amp_event_worker);
		work->hdev = hdev;
		work->event = event;
		work->skb = skbc;
		if (queue_work(amp_workqueue, (struct work_struct *) work) == 0)
			kfree(work);
	}
}

static void amp_dev_event_worker(struct work_struct *w)
{
	send_a2mp_change_notify();
	kfree(w);
}

static int amp_dev_event(struct notifier_block *this, unsigned long event,
			void *ptr)
{
	struct hci_dev *hdev = (struct hci_dev *) ptr;
	struct amp_work_event *work;

	if (hdev->amp_type == HCI_BREDR)
		return NOTIFY_DONE;

	switch (event) {
	case HCI_DEV_UNREG:
	case HCI_DEV_REG:
	case HCI_DEV_UP:
	case HCI_DEV_DOWN:
		BT_DBG("hdev %p event %ld", hdev, event);
		work = kmalloc(sizeof(*work), GFP_ATOMIC);
		if (work) {
			INIT_WORK((struct work_struct *) work,
				amp_dev_event_worker);
			if (queue_work(amp_workqueue,
				(struct work_struct *) work) == 0)
				kfree(work);
		}
	}
	return NOTIFY_DONE;
}


/* L2CAP module init continued */

static struct notifier_block amp_notifier = {
	.notifier_call = amp_dev_event
};

static struct amp_mgr_cb hci_amp = {
	.amp_cmd_complete_event = amp_cmd_cmplt_evt,
	.amp_cmd_status_event = amp_cmd_status_evt,
	.amp_event = amp_evt
};

int amp_init(void)
{
	hci_register_amp(&hci_amp);
	hci_register_notifier(&amp_notifier);
	amp_next_handle = 1;
	amp_workqueue = create_singlethread_workqueue("a2mp");
	if (!amp_workqueue)
		return -EPERM;
	return 0;
}

void amp_exit(void)
{
	hci_unregister_amp(&hci_amp);
	hci_unregister_notifier(&amp_notifier);
	flush_workqueue(amp_workqueue);
	destroy_workqueue(amp_workqueue);
}
