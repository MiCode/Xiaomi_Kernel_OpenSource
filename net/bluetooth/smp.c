/*
   BlueZ - Bluetooth protocol stack for Linux
   Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation;

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
   IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
   CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS,
   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS
   SOFTWARE IS DISCLAIMED.
*/

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <net/bluetooth/l2cap.h>
#include <net/bluetooth/mgmt.h>
#include <net/bluetooth/smp.h>
#include <linux/crypto.h>
#include <crypto/b128ops.h>
#include <asm/unaligned.h>

#define SMP_TIMEOUT 30000 /* 30 seconds */

#define SMP_MIN_CONN_INTERVAL	40	/* 50ms (40 * 1.25ms) */
#define SMP_MAX_CONN_INTERVAL	56	/* 70ms (56 * 1.25ms) */
#define SMP_MAX_CONN_LATENCY	0	/* 0ms (0 * 1.25ms) */
#define SMP_SUPERVISION_TIMEOUT	500	/* 5 seconds (500 * 10ms) */

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif

static int smp_distribute_keys(struct l2cap_conn *conn, __u8 force);

static inline void swap128(u8 src[16], u8 dst[16])
{
	int i;
	for (i = 0; i < 16; i++)
		dst[15 - i] = src[i];
}

static inline void swap56(u8 src[7], u8 dst[7])
{
	int i;
	for (i = 0; i < 7; i++)
		dst[6 - i] = src[i];
}

static int smp_e(struct crypto_blkcipher *tfm, const u8 *k, u8 *r)
{
	struct blkcipher_desc desc;
	struct scatterlist sg;
	int err, iv_len;
	unsigned char iv[128];

	if (tfm == NULL) {
		BT_ERR("tfm %p", tfm);
		return -EINVAL;
	}

	desc.tfm = tfm;
	desc.flags = 0;

	err = crypto_blkcipher_setkey(tfm, k, 16);
	if (err) {
		BT_ERR("cipher setkey failed: %d", err);
		return err;
	}

	sg_init_one(&sg, r, 16);

	iv_len = crypto_blkcipher_ivsize(tfm);
	if (iv_len) {
		memset(&iv, 0xff, iv_len);
		crypto_blkcipher_set_iv(tfm, iv, iv_len);
	}

	err = crypto_blkcipher_encrypt(&desc, &sg, &sg, 16);
	if (err)
		BT_ERR("Encrypt data error %d", err);

	return err;
}

static int smp_c1(struct crypto_blkcipher *tfm, u8 k[16], u8 r[16],
		u8 preq[7], u8 pres[7], u8 _iat, bdaddr_t *ia,
		u8 _rat, bdaddr_t *ra, u8 res[16])
{
	u8 p1[16], p2[16];
	int err;

	memset(p1, 0, 16);

	/* p1 = pres || preq || _rat || _iat */
	swap56(pres, p1);
	swap56(preq, p1 + 7);
	p1[14] = _rat;
	p1[15] = _iat;

	memset(p2, 0, 16);

	/* p2 = padding || ia || ra */
	baswap((bdaddr_t *) (p2 + 4), ia);
	baswap((bdaddr_t *) (p2 + 10), ra);

	/* res = r XOR p1 */
	u128_xor((u128 *) res, (u128 *) r, (u128 *) p1);

	/* res = e(k, res) */
	err = smp_e(tfm, k, res);
	if (err) {
		BT_ERR("Encrypt data error");
		return err;
	}

	/* res = res XOR p2 */
	u128_xor((u128 *) res, (u128 *) res, (u128 *) p2);

	/* res = e(k, res) */
	err = smp_e(tfm, k, res);
	if (err)
		BT_ERR("Encrypt data error");

	return err;
}

static int smp_s1(struct crypto_blkcipher *tfm, u8 k[16],
			u8 r1[16], u8 r2[16], u8 _r[16])
{
	int err;

	/* Just least significant octets from r1 and r2 are considered */
	memcpy(_r, r1 + 8, 8);
	memcpy(_r + 8, r2 + 8, 8);

	err = smp_e(tfm, k, _r);
	if (err)
		BT_ERR("Encrypt data error");

	return err;
}

static int smp_rand(u8 *buf)
{
	get_random_bytes(buf, 16);

	return 0;
}

static struct sk_buff *smp_build_cmd(struct l2cap_conn *conn, u8 code,
		u16 dlen, void *data)
{
	struct sk_buff *skb;
	struct l2cap_hdr *lh;
	int len;

	len = L2CAP_HDR_SIZE + sizeof(code) + dlen;

	if (len > conn->mtu)
		return NULL;

	skb = bt_skb_alloc(len, GFP_ATOMIC);
	if (!skb)
		return NULL;

	lh = (struct l2cap_hdr *) skb_put(skb, L2CAP_HDR_SIZE);
	lh->len = cpu_to_le16(sizeof(code) + dlen);
	lh->cid = cpu_to_le16(L2CAP_CID_SMP);

	memcpy(skb_put(skb, sizeof(code)), &code, sizeof(code));

	memcpy(skb_put(skb, dlen), data, dlen);

	return skb;
}

static void smp_send_cmd(struct l2cap_conn *conn, u8 code, u16 len, void *data)
{
	struct sk_buff *skb = smp_build_cmd(conn, code, len, data);

	BT_DBG("code 0x%2.2x", code);

	if (!skb)
		return;

	hci_send_acl(conn->hcon, NULL, skb, 0);
}

static __u8 authreq_to_seclevel(__u8 authreq)
{
	if (authreq & SMP_AUTH_MITM)
		return BT_SECURITY_HIGH;
	else if (authreq & SMP_AUTH_BONDING)
		return BT_SECURITY_MEDIUM;
	else
		return BT_SECURITY_LOW;
}

static __u8 seclevel_to_authreq(__u8 level)
{
	switch (level) {
	case BT_SECURITY_HIGH:
		return SMP_AUTH_MITM | SMP_AUTH_BONDING;

	default:
		return SMP_AUTH_NONE;
	}
}

static void build_pairing_cmd(struct l2cap_conn *conn,
				struct smp_cmd_pairing *req,
				struct smp_cmd_pairing *rsp,
				__u8 authreq)
{
	struct hci_conn *hcon = conn->hcon;
	u8 all_keys = 0;
	u8 dist_keys = 0;

	dist_keys = SMP_DIST_ENC_KEY;
	authreq |= SMP_AUTH_BONDING;

	BT_DBG("conn->hcon->io_capability:%d", conn->hcon->io_capability);

	if (rsp == NULL) {
		req->io_capability = conn->hcon->io_capability;
		req->oob_flag = hcon->oob ? SMP_OOB_PRESENT :
							SMP_OOB_NOT_PRESENT;
		req->max_key_size = SMP_MAX_ENC_KEY_SIZE;
		req->init_key_dist = all_keys;
		req->resp_key_dist = dist_keys;
		req->auth_req = authreq;
		BT_DBG("SMP_CMD_PAIRING_REQ %d %d %d %d %2.2x %2.2x",
				req->io_capability, req->oob_flag,
				req->auth_req, req->max_key_size,
				req->init_key_dist, req->resp_key_dist);
		return;
	}

	/* Only request OOB if remote AND we support it */
	if (req->oob_flag)
		rsp->oob_flag = hcon->oob ? SMP_OOB_PRESENT :
						SMP_OOB_NOT_PRESENT;
	else
		rsp->oob_flag = SMP_OOB_NOT_PRESENT;

	rsp->io_capability = conn->hcon->io_capability;
	rsp->max_key_size = SMP_MAX_ENC_KEY_SIZE;
	rsp->init_key_dist = req->init_key_dist & all_keys;
	rsp->resp_key_dist = req->resp_key_dist & dist_keys;
	rsp->auth_req = authreq;
	BT_DBG("SMP_CMD_PAIRING_RSP %d %d %d %d %2.2x %2.2x",
			req->io_capability, req->oob_flag, req->auth_req,
			req->max_key_size, req->init_key_dist,
			req->resp_key_dist);
}

static u8 check_enc_key_size(struct l2cap_conn *conn, __u8 max_key_size)
{
	struct hci_conn *hcon = conn->hcon;

	if ((max_key_size > SMP_MAX_ENC_KEY_SIZE) ||
			(max_key_size < SMP_MIN_ENC_KEY_SIZE))
		return SMP_ENC_KEY_SIZE;

	hcon->smp_key_size = max_key_size;

	return 0;
}

#define JUST_WORKS	SMP_JUST_WORKS
#define REQ_PASSKEY	SMP_REQ_PASSKEY
#define CFM_PASSKEY	SMP_CFM_PASSKEY
#define JUST_CFM	SMP_JUST_CFM
#define OVERLAP		SMP_OVERLAP
static const u8	gen_method[5][5] = {
	{JUST_WORKS,  JUST_CFM,    REQ_PASSKEY, JUST_WORKS, REQ_PASSKEY},
	{JUST_WORKS,  JUST_CFM,    REQ_PASSKEY, JUST_WORKS, REQ_PASSKEY},
	{CFM_PASSKEY, CFM_PASSKEY, REQ_PASSKEY, JUST_WORKS, CFM_PASSKEY},
	{JUST_WORKS,  JUST_CFM,    JUST_WORKS,  JUST_WORKS, JUST_CFM},
	{CFM_PASSKEY, CFM_PASSKEY, REQ_PASSKEY, JUST_WORKS, OVERLAP}
};

static int tk_request(struct l2cap_conn *conn, u8 remote_oob, u8 auth,
						u8 local_io, u8 remote_io)
{
	struct hci_conn *hcon = conn->hcon;
	u8 method;
	u32 passkey = 0;
	int ret = 0;

	/* Initialize key to JUST WORKS */
	memset(hcon->tk, 0, sizeof(hcon->tk));
	hcon->tk_valid = FALSE;
	hcon->auth = auth;

	/* By definition, OOB data will be used if both sides have it available
	 */
	if (remote_oob && hcon->oob) {
		method = SMP_REQ_OOB;
		goto agent_request;
	}

	BT_DBG("tk_request: auth:%d lcl:%d rem:%d", auth, local_io, remote_io);

	/* If neither side wants MITM, use JUST WORKS */
	/* If either side has unknown io_caps, use JUST_WORKS */
	if (!(auth & SMP_AUTH_MITM) ||
			local_io > SMP_IO_KEYBOARD_DISPLAY ||
			remote_io > SMP_IO_KEYBOARD_DISPLAY) {
		hcon->auth &= ~SMP_AUTH_MITM;
		hcon->tk_valid = TRUE;
		return 0;
	}

	/* MITM is now officially requested, but not required */
	/* Determine what we need (if anything) from the agent */
	method = gen_method[local_io][remote_io];

	BT_DBG("tk_method: %d", method);

	if (method == SMP_JUST_WORKS || method == SMP_JUST_CFM)
		hcon->auth &= ~SMP_AUTH_MITM;

	/* Don't bother confirming unbonded JUST_WORKS */
	if (!(auth & SMP_AUTH_BONDING) && method == SMP_JUST_CFM) {
		hcon->tk_valid = TRUE;
		return 0;
	} else if (method == SMP_JUST_WORKS) {
		hcon->tk_valid = TRUE;
		return 0;
	} else if (method == SMP_OVERLAP) {
		if (hcon->link_mode & HCI_LM_MASTER)
			method = SMP_CFM_PASSKEY;
		else
			method = SMP_REQ_PASSKEY;
	}

	BT_DBG("tk_method-2: %d", method);

	if (method == SMP_CFM_PASSKEY) {
		u8 key[16];
		/* Generate a passkey for display. It is not valid until
		 * confirmed.
		 */
		memset(key, 0, sizeof(key));
		get_random_bytes(&passkey, sizeof(passkey));
		passkey %= 1000000;
		put_unaligned_le32(passkey, key);
		swap128(key, hcon->tk);
		BT_DBG("PassKey: %d", passkey);
	}

agent_request:
	hci_dev_lock(hcon->hdev);

	switch (method) {
	case SMP_REQ_PASSKEY:
		ret = mgmt_user_confirm_request(hcon->hdev->id,
				HCI_EV_USER_PASSKEY_REQUEST, conn->dst, 0);
		break;
	case SMP_CFM_PASSKEY:
	default:
		ret = mgmt_user_confirm_request(hcon->hdev->id,
			HCI_EV_USER_CONFIRM_REQUEST, conn->dst, passkey);
		break;
	}

	hci_dev_unlock(hcon->hdev);

	return ret;
}

static int send_pairing_confirm(struct l2cap_conn *conn)
{
	struct hci_conn *hcon = conn->hcon;
	struct crypto_blkcipher *tfm = hcon->hdev->tfm;
	struct smp_cmd_pairing_confirm cp;
	int ret;
	u8 res[16];

	if (conn->hcon->out)
		ret = smp_c1(tfm, hcon->tk, hcon->prnd, hcon->preq, hcon->prsp,
				0, conn->src, hcon->dst_type, conn->dst, res);
	else
		ret = smp_c1(tfm, hcon->tk, hcon->prnd, hcon->preq, hcon->prsp,
				hcon->dst_type, conn->dst, 0, conn->src, res);

	if (ret)
		return SMP_CONFIRM_FAILED;

	swap128(res, cp.confirm_val);

	hcon->cfm_pending = FALSE;

	smp_send_cmd(conn, SMP_CMD_PAIRING_CONFIRM, sizeof(cp), &cp);

	return 0;
}

int le_user_confirm_reply(struct hci_conn *hcon, u16 mgmt_op, void *cp)
{
	struct mgmt_cp_user_passkey_reply *psk_reply = cp;
	struct l2cap_conn *conn = hcon->smp_conn;
	u8 key[16];
	u8 reason = 0;
	int ret = 0;

	BT_DBG("");

	hcon->tk_valid = TRUE;

	switch (mgmt_op) {
	case MGMT_OP_USER_CONFIRM_NEG_REPLY:
		reason = SMP_CONFIRM_FAILED;
		break;
	case MGMT_OP_USER_CONFIRM_REPLY:
		break;
	case MGMT_OP_USER_PASSKEY_REPLY:
		memset(key, 0, sizeof(key));
		BT_DBG("PassKey: %d", psk_reply->passkey);
		put_unaligned_le32(psk_reply->passkey, key);
		swap128(key, hcon->tk);
		break;
	default:
		reason = SMP_CONFIRM_FAILED;
		ret = -EOPNOTSUPP;
		break;
	}

	if (reason) {
		BT_DBG("smp_send_cmd: SMP_CMD_PAIRING_FAIL");
		smp_send_cmd(conn, SMP_CMD_PAIRING_FAIL, sizeof(reason),
								&reason);
		del_timer(&hcon->smp_timer);
		clear_bit(HCI_CONN_ENCRYPT_PEND, &hcon->pend);
		mgmt_auth_failed(hcon->hdev->id, conn->dst, reason);
		hci_conn_put(hcon);
	} else if (hcon->cfm_pending) {
		BT_DBG("send_pairing_confirm");
		ret = send_pairing_confirm(conn);
	}

	return ret;
}

static u8 smp_cmd_pairing_req(struct l2cap_conn *conn, struct sk_buff *skb)
{
	struct hci_conn *hcon = conn->hcon;
	struct smp_cmd_pairing rsp, *req = (void *) skb->data;
	u8 key_size;
	u8 auth = SMP_AUTH_NONE;
	int ret;

	BT_DBG("conn %p", conn);

	hcon->preq[0] = SMP_CMD_PAIRING_REQ;
	memcpy(&hcon->preq[1], req, sizeof(*req));
	skb_pull(skb, sizeof(*req));

	if (req->oob_flag && hcon->oob) {
		/* By definition, OOB data pairing will have MITM protection */
		auth = req->auth_req | SMP_AUTH_MITM;
	} else if (req->auth_req & SMP_AUTH_BONDING) {
		/* We will attempt MITM for all Bonding attempts */
		auth = SMP_AUTH_BONDING | SMP_AUTH_MITM;
	}

	/* We didn't start the pairing, so no requirements */
	build_pairing_cmd(conn, req, &rsp, auth);

	key_size = min(req->max_key_size, rsp.max_key_size);
	if (check_enc_key_size(conn, key_size))
		return SMP_ENC_KEY_SIZE;

	ret = smp_rand(hcon->prnd);
	if (ret)
		return SMP_UNSPECIFIED;

	/* Request setup of TK */
	ret = tk_request(conn, req->oob_flag, auth, rsp.io_capability,
							req->io_capability);
	if (ret)
		return SMP_UNSPECIFIED;

	hcon->prsp[0] = SMP_CMD_PAIRING_RSP;
	memcpy(&hcon->prsp[1], &rsp, sizeof(rsp));

	smp_send_cmd(conn, SMP_CMD_PAIRING_RSP, sizeof(rsp), &rsp);

	mod_timer(&hcon->smp_timer, jiffies + msecs_to_jiffies(SMP_TIMEOUT));

	return 0;
}

static u8 smp_cmd_pairing_rsp(struct l2cap_conn *conn, struct sk_buff *skb)
{
	struct hci_conn *hcon = conn->hcon;
	struct smp_cmd_pairing *req, *rsp = (void *) skb->data;
	u8 key_size, auth = SMP_AUTH_NONE;
	int ret;

	BT_DBG("conn %p", conn);

	skb_pull(skb, sizeof(*rsp));

	req = (void *) &hcon->preq[1];

	key_size = min(req->max_key_size, rsp->max_key_size);
	if (check_enc_key_size(conn, key_size))
		return SMP_ENC_KEY_SIZE;

	hcon->prsp[0] = SMP_CMD_PAIRING_RSP;
	memcpy(&hcon->prsp[1], rsp, sizeof(*rsp));

	ret = smp_rand(hcon->prnd);
	if (ret)
		return SMP_UNSPECIFIED;

	if ((req->auth_req & SMP_AUTH_BONDING) &&
			(rsp->auth_req & SMP_AUTH_BONDING))
		auth = SMP_AUTH_BONDING;

	auth |= (req->auth_req | rsp->auth_req) & SMP_AUTH_MITM;

	ret = tk_request(conn, req->oob_flag, auth, rsp->io_capability,
							req->io_capability);
	if (ret)
		return SMP_UNSPECIFIED;

	hcon->cfm_pending = TRUE;

	/* Can't compose response until we have been confirmed */
	if (!hcon->tk_valid)
		return 0;

	ret = send_pairing_confirm(conn);
	if (ret)
		return SMP_CONFIRM_FAILED;

	return 0;
}

static u8 smp_cmd_pairing_confirm(struct l2cap_conn *conn, struct sk_buff *skb)
{
	struct hci_conn *hcon = conn->hcon;
	int ret;

	BT_DBG("conn %p %s", conn, conn->hcon->out ? "master" : "slave");

	memcpy(hcon->pcnf, skb->data, sizeof(hcon->pcnf));
	skb_pull(skb, sizeof(hcon->pcnf));

	if (conn->hcon->out) {
		u8 random[16];

		swap128(hcon->prnd, random);
		smp_send_cmd(conn, SMP_CMD_PAIRING_RANDOM, sizeof(random),
								random);
	} else if (hcon->tk_valid) {
		ret = send_pairing_confirm(conn);

		if (ret)
			return SMP_CONFIRM_FAILED;
	} else
		hcon->cfm_pending = TRUE;


	mod_timer(&hcon->smp_timer, jiffies + msecs_to_jiffies(SMP_TIMEOUT));

	return 0;
}

static u8 smp_cmd_pairing_random(struct l2cap_conn *conn, struct sk_buff *skb)
{
	struct hci_conn *hcon = conn->hcon;
	struct crypto_blkcipher *tfm = hcon->hdev->tfm;
	int ret;
	u8 key[16], res[16], random[16], confirm[16];

	swap128(skb->data, random);
	skb_pull(skb, sizeof(random));

	if (conn->hcon->out)
		ret = smp_c1(tfm, hcon->tk, random, hcon->preq, hcon->prsp, 0,
				conn->src, hcon->dst_type, conn->dst,
				res);
	else
		ret = smp_c1(tfm, hcon->tk, random, hcon->preq, hcon->prsp,
				hcon->dst_type, conn->dst, 0, conn->src,
				res);
	if (ret)
		return SMP_UNSPECIFIED;

	BT_DBG("conn %p %s", conn, conn->hcon->out ? "master" : "slave");

	swap128(res, confirm);

	if (memcmp(hcon->pcnf, confirm, sizeof(hcon->pcnf)) != 0) {
		BT_ERR("Pairing failed (confirmation values mismatch)");
		return SMP_CONFIRM_FAILED;
	}

	if (conn->hcon->out) {
		u8 stk[16], rand[8];
		__le16 ediv;

		memset(rand, 0, sizeof(rand));
		ediv = 0;

		smp_s1(tfm, hcon->tk, random, hcon->prnd, key);
		swap128(key, stk);

		memset(stk + hcon->smp_key_size, 0,
				SMP_MAX_ENC_KEY_SIZE - hcon->smp_key_size);

		hci_le_start_enc(hcon, ediv, rand, stk);
		hcon->enc_key_size = hcon->smp_key_size;
	} else {
		u8 stk[16], r[16], rand[8];
		__le16 ediv;

		memset(rand, 0, sizeof(rand));
		ediv = 0;

		swap128(hcon->prnd, r);
		smp_send_cmd(conn, SMP_CMD_PAIRING_RANDOM, sizeof(r), r);

		smp_s1(tfm, hcon->tk, hcon->prnd, random, key);
		swap128(key, stk);

		memset(stk + hcon->smp_key_size, 0,
				SMP_MAX_ENC_KEY_SIZE - hcon->smp_key_size);

		hci_add_ltk(conn->hcon->hdev, 0, conn->dst, hcon->dst_type,
			hcon->smp_key_size, hcon->auth, ediv, rand, stk);
	}

	return 0;
}

static int smp_encrypt_link(struct hci_conn *hcon, struct link_key *key)
{
	struct key_master_id *master;
	u8 sec_level;
	u8 zerobuf[8];

	if (!hcon || !key || !key->data)
		return -EINVAL;

	memset(zerobuf, 0, sizeof(zerobuf));

	master = (void *) key->data;

	if (!master->ediv && !memcmp(master->rand, zerobuf, sizeof(zerobuf)))
		return -EINVAL;

	hcon->enc_key_size = key->pin_len;
	hcon->sec_req = TRUE;
	sec_level = authreq_to_seclevel(key->auth);

	BT_DBG("cur %d, req: %d", hcon->sec_level, sec_level);

	if (sec_level > hcon->sec_level)
		hcon->pending_sec_level = sec_level;


	if (!(hcon->link_mode & HCI_LM_ENCRYPT))
		hci_conn_hold(hcon);

	hci_le_start_enc(hcon, master->ediv, master->rand, key->val);

	return 0;
}

static u8 smp_cmd_security_req(struct l2cap_conn *conn, struct sk_buff *skb)
{
	struct hci_conn *hcon = conn->hcon;
	struct smp_cmd_security_req *rp = (void *) skb->data;
	struct smp_cmd_pairing cp;
	struct link_key *key;

	BT_DBG("conn %p", conn);

	if (test_bit(HCI_CONN_ENCRYPT_PEND, &hcon->pend))
		return 0;

	key = hci_find_link_key_type(hcon->hdev, conn->dst, KEY_TYPE_LTK);
	if (key && ((key->auth & SMP_AUTH_MITM) ||
					!(rp->auth_req & SMP_AUTH_MITM))) {

		if (smp_encrypt_link(hcon, key) < 0)
			goto invalid_key;

		return 0;
	}

invalid_key:
	hcon->sec_req = FALSE;

	/* Switch to Pairing Connection Parameters */
	hci_le_conn_update(hcon, SMP_MIN_CONN_INTERVAL, SMP_MAX_CONN_INTERVAL,
			SMP_MAX_CONN_LATENCY, SMP_SUPERVISION_TIMEOUT);

	skb_pull(skb, sizeof(*rp));

	memset(&cp, 0, sizeof(cp));
	build_pairing_cmd(conn, &cp, NULL, rp->auth_req);

	hcon->preq[0] = SMP_CMD_PAIRING_REQ;
	memcpy(&hcon->preq[1], &cp, sizeof(cp));

	smp_send_cmd(conn, SMP_CMD_PAIRING_REQ, sizeof(cp), &cp);

	mod_timer(&hcon->smp_timer, jiffies + msecs_to_jiffies(SMP_TIMEOUT));

	set_bit(HCI_CONN_ENCRYPT_PEND, &hcon->pend);

	hci_conn_hold(hcon);

	return 0;
}

int smp_conn_security(struct l2cap_conn *conn, __u8 sec_level)
{
	struct hci_conn *hcon = conn->hcon;
	__u8 authreq;

	BT_DBG("conn %p hcon %p %d req: %d",
			conn, hcon, hcon->sec_level, sec_level);

	if (IS_ERR(hcon->hdev->tfm))
		return 1;

	if (test_bit(HCI_CONN_ENCRYPT_PEND, &hcon->pend))
		return -EINPROGRESS;

	if (sec_level == BT_SECURITY_LOW)
		return 1;


	if (hcon->sec_level >= sec_level)
		return 1;

	authreq = seclevel_to_authreq(sec_level);

	hcon->smp_conn = conn;
	hcon->pending_sec_level = sec_level;

	if ((hcon->link_mode & HCI_LM_MASTER) && !hcon->sec_req) {
		struct link_key *key;

		key = hci_find_link_key_type(hcon->hdev, conn->dst,
							KEY_TYPE_LTK);

		if (smp_encrypt_link(hcon, key) == 0)
			goto done;
	}

	hcon->sec_req = FALSE;

	if (hcon->link_mode & HCI_LM_MASTER) {
		struct smp_cmd_pairing cp;

		/* Switch to Pairing Connection Parameters */
		hci_le_conn_update(hcon, SMP_MIN_CONN_INTERVAL,
				SMP_MAX_CONN_INTERVAL, SMP_MAX_CONN_LATENCY,
				SMP_SUPERVISION_TIMEOUT);

		build_pairing_cmd(conn, &cp, NULL, authreq);
		hcon->preq[0] = SMP_CMD_PAIRING_REQ;
		memcpy(&hcon->preq[1], &cp, sizeof(cp));

		mod_timer(&hcon->smp_timer, jiffies +
					msecs_to_jiffies(SMP_TIMEOUT));

		smp_send_cmd(conn, SMP_CMD_PAIRING_REQ, sizeof(cp), &cp);
		hci_conn_hold(hcon);
	} else {
		struct smp_cmd_security_req cp;
		cp.auth_req = authreq;
		smp_send_cmd(conn, SMP_CMD_SECURITY_REQ, sizeof(cp), &cp);
	}

done:
	set_bit(HCI_CONN_ENCRYPT_PEND, &hcon->pend);

	return 0;
}

static int smp_cmd_encrypt_info(struct l2cap_conn *conn, struct sk_buff *skb)
{
	struct hci_conn *hcon = conn->hcon;
	struct smp_cmd_encrypt_info *rp = (void *) skb->data;
	u8 rand[8];
	int err;

	skb_pull(skb, sizeof(*rp));

	BT_DBG("conn %p", conn);

	memset(rand, 0, sizeof(rand));

	err = hci_add_ltk(hcon->hdev, 0, conn->dst, hcon->dst_type,
						0, 0, 0, rand, rp->ltk);
	if (err)
		return SMP_UNSPECIFIED;

	return 0;
}

static int smp_cmd_master_ident(struct l2cap_conn *conn, struct sk_buff *skb)
{
	struct hci_conn *hcon = conn->hcon;
	struct smp_cmd_master_ident *rp = (void *) skb->data;
	struct smp_cmd_pairing *paircmd = (void *) &hcon->prsp[1];
	struct link_key *key;
	u8 *keydist;

	skb_pull(skb, sizeof(*rp));

	key = hci_find_link_key_type(hcon->hdev, conn->dst, KEY_TYPE_LTK);
	if (key == NULL)
		return SMP_UNSPECIFIED;

	if (hcon->out)
		keydist = &paircmd->resp_key_dist;
	else
		keydist = &paircmd->init_key_dist;

	BT_DBG("keydist 0x%x", *keydist);

	hci_add_ltk(hcon->hdev, 1, conn->dst, hcon->dst_type,
			hcon->smp_key_size, hcon->auth, rp->ediv,
			rp->rand, key->val);

	*keydist &= ~SMP_DIST_ENC_KEY;
	if (hcon->out) {
		if (!(*keydist))
			smp_distribute_keys(conn, 1);
	}

	return 0;
}

int smp_sig_channel(struct l2cap_conn *conn, struct sk_buff *skb)
{
	struct hci_conn *hcon = conn->hcon;
	__u8 code = skb->data[0];
	__u8 reason;
	int err = 0;

	if (IS_ERR(hcon->hdev->tfm)) {
		err = PTR_ERR(hcon->hdev->tfm);
		reason = SMP_PAIRING_NOTSUPP;
		BT_ERR("SMP_PAIRING_NOTSUPP %p", hcon->hdev->tfm);
		goto done;
	}

	hcon->smp_conn = conn;
	skb_pull(skb, sizeof(code));

	switch (code) {
	case SMP_CMD_PAIRING_REQ:
		reason = smp_cmd_pairing_req(conn, skb);
		break;

	case SMP_CMD_PAIRING_FAIL:
		reason = 0;
		err = -EPERM;
		del_timer(&hcon->smp_timer);
		clear_bit(HCI_CONN_ENCRYPT_PEND, &hcon->pend);
		mgmt_auth_failed(hcon->hdev->id, conn->dst, skb->data[0]);
		hci_conn_put(hcon);
		break;

	case SMP_CMD_PAIRING_RSP:
		reason = smp_cmd_pairing_rsp(conn, skb);
		break;

	case SMP_CMD_SECURITY_REQ:
		reason = smp_cmd_security_req(conn, skb);
		break;

	case SMP_CMD_PAIRING_CONFIRM:
		reason = smp_cmd_pairing_confirm(conn, skb);
		break;

	case SMP_CMD_PAIRING_RANDOM:
		reason = smp_cmd_pairing_random(conn, skb);
		break;

	case SMP_CMD_ENCRYPT_INFO:
		reason = smp_cmd_encrypt_info(conn, skb);
		break;

	case SMP_CMD_MASTER_IDENT:
		reason = smp_cmd_master_ident(conn, skb);
		break;

	case SMP_CMD_IDENT_INFO:
	case SMP_CMD_IDENT_ADDR_INFO:
	case SMP_CMD_SIGN_INFO:
		/* Just ignored */
		reason = 0;
		break;

	default:
		BT_DBG("Unknown command code 0x%2.2x", code);

		reason = SMP_CMD_NOTSUPP;
		err = -EOPNOTSUPP;
		goto done;
	}

done:
	if (reason) {
		BT_ERR("SMP_CMD_PAIRING_FAIL: %d", reason);
		smp_send_cmd(conn, SMP_CMD_PAIRING_FAIL, sizeof(reason),
								&reason);
		del_timer(&hcon->smp_timer);
		clear_bit(HCI_CONN_ENCRYPT_PEND, &hcon->pend);
		mgmt_auth_failed(hcon->hdev->id, conn->dst, reason);
		hci_conn_put(hcon);
	}

	kfree_skb(skb);
	return err;
}

static int smp_distribute_keys(struct l2cap_conn *conn, __u8 force)
{
	struct hci_conn *hcon = conn->hcon;
	struct smp_cmd_pairing *req, *rsp;
	__u8 *keydist;

	BT_DBG("conn %p force %d", conn, force);

	if (IS_ERR(hcon->hdev->tfm))
		return PTR_ERR(hcon->hdev->tfm);

	rsp = (void *) &hcon->prsp[1];

	/* The responder sends its keys first */
	if (!force && hcon->out && (rsp->resp_key_dist & 0x07))
		return 0;

	req = (void *) &hcon->preq[1];

	if (hcon->out) {
		keydist = &rsp->init_key_dist;
		*keydist &= req->init_key_dist;
	} else {
		keydist = &rsp->resp_key_dist;
		*keydist &= req->resp_key_dist;
	}


	BT_DBG("keydist 0x%x", *keydist);

	if (*keydist & SMP_DIST_ENC_KEY) {
		struct smp_cmd_encrypt_info enc;
		struct smp_cmd_master_ident ident;
		__le16 ediv;

		get_random_bytes(enc.ltk, sizeof(enc.ltk));
		get_random_bytes(&ediv, sizeof(ediv));
		get_random_bytes(ident.rand, sizeof(ident.rand));

		smp_send_cmd(conn, SMP_CMD_ENCRYPT_INFO, sizeof(enc), &enc);

		hci_add_ltk(hcon->hdev, 1, conn->dst, hcon->dst_type,
				hcon->smp_key_size, hcon->auth, ediv,
				ident.rand, enc.ltk);

		ident.ediv = cpu_to_le16(ediv);

		smp_send_cmd(conn, SMP_CMD_MASTER_IDENT, sizeof(ident), &ident);

		*keydist &= ~SMP_DIST_ENC_KEY;
	}

	if (*keydist & SMP_DIST_ID_KEY) {
		struct smp_cmd_ident_addr_info addrinfo;
		struct smp_cmd_ident_info idinfo;

		/* Send a dummy key */
		get_random_bytes(idinfo.irk, sizeof(idinfo.irk));

		smp_send_cmd(conn, SMP_CMD_IDENT_INFO, sizeof(idinfo), &idinfo);

		/* Just public address */
		memset(&addrinfo, 0, sizeof(addrinfo));
		bacpy(&addrinfo.bdaddr, conn->src);

		smp_send_cmd(conn, SMP_CMD_IDENT_ADDR_INFO, sizeof(addrinfo),
								&addrinfo);

		*keydist &= ~SMP_DIST_ID_KEY;
	}

	if (*keydist & SMP_DIST_SIGN) {
		struct smp_cmd_sign_info sign;

		/* Send a dummy key */
		get_random_bytes(sign.csrk, sizeof(sign.csrk));

		smp_send_cmd(conn, SMP_CMD_SIGN_INFO, sizeof(sign), &sign);

		*keydist &= ~SMP_DIST_SIGN;
	}

	if (hcon->out) {
		if (hcon->disconn_cfm_cb)
			hcon->disconn_cfm_cb(hcon, 0);
		del_timer(&hcon->smp_timer);
		clear_bit(HCI_CONN_ENCRYPT_PEND, &hcon->pend);
		hci_conn_put(hcon);
	} else if (rsp->resp_key_dist) {
		if (hcon->disconn_cfm_cb)
			hcon->disconn_cfm_cb(hcon, SMP_UNSPECIFIED);
		clear_bit(HCI_CONN_ENCRYPT_PEND, &hcon->pend);
		mgmt_auth_failed(hcon->hdev->id, conn->dst, SMP_UNSPECIFIED);
		hci_conn_put(hcon);
	}

	return 0;
}

int smp_link_encrypt_cmplt(struct l2cap_conn *conn, u8 status, u8 encrypt)
{
	struct hci_conn *hcon = conn->hcon;

	BT_DBG("smp: %d %d %d", status, encrypt, hcon->sec_req);

	clear_bit(HCI_CONN_ENCRYPT_PEND, &hcon->pend);

	if (!status && encrypt && hcon->sec_level < hcon->pending_sec_level)
		hcon->sec_level = hcon->pending_sec_level;

	if (!status && encrypt && !hcon->sec_req)
		return smp_distribute_keys(conn, 0);

	/* Fall back to Pairing request if failed a Link Security request */
	else if (hcon->sec_req  && (status || !encrypt))
		smp_conn_security(conn, hcon->pending_sec_level);

	hci_conn_put(hcon);

	return 0;
}

void smp_timeout(unsigned long arg)
{
	struct l2cap_conn *conn = (void *) arg;
	u8 reason = SMP_UNSPECIFIED;

	BT_DBG("%p", conn);

	smp_send_cmd(conn, SMP_CMD_PAIRING_FAIL, sizeof(reason), &reason);
	clear_bit(HCI_CONN_ENCRYPT_PEND, &conn->hcon->pend);
	mgmt_auth_failed(conn->hcon->hdev->id, conn->dst, SMP_UNSPECIFIED);
	hci_conn_put(conn->hcon);
}
