/*
   BlueZ - Bluetooth protocol stack for Linux
   Copyright (C) 2010  Nokia Corporation
   Copyright (c) 2011-2012 Code Aurora Forum.  All rights reserved.

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

/* Bluetooth HCI Management interface */

#include <linux/uaccess.h>
#include <asm/unaligned.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <net/bluetooth/l2cap.h>
#include <net/bluetooth/mgmt.h>
#include <net/bluetooth/smp.h>

#define MGMT_VERSION	0
#define MGMT_REVISION	1

#define SCAN_IDLE	0x00
#define SCAN_LE		0x01
#define SCAN_BR		0x02

struct pending_cmd {
	struct list_head list;
	__u16 opcode;
	int index;
	void *param;
	struct sock *sk;
	void *user_data;
};

struct mgmt_pending_free_work {
	struct work_struct work;
	struct sock *sk;
};

LIST_HEAD(cmd_list);

static int cmd_status(struct sock *sk, u16 index, u16 cmd, u8 status)
{
	struct sk_buff *skb;
	struct mgmt_hdr *hdr;
	struct mgmt_ev_cmd_status *ev;

	BT_DBG("sock %p, index %u, cmd %u, status %u", sk, index, cmd, status);

	skb = alloc_skb(sizeof(*hdr) + sizeof(*ev), GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	hdr = (void *) skb_put(skb, sizeof(*hdr));

	hdr->opcode = cpu_to_le16(MGMT_EV_CMD_STATUS);
	hdr->index = cpu_to_le16(index);
	hdr->len = cpu_to_le16(sizeof(*ev));

	ev = (void *) skb_put(skb, sizeof(*ev));
	ev->status = status;
	put_unaligned_le16(cmd, &ev->opcode);

	if (sock_queue_rcv_skb(sk, skb) < 0)
		kfree_skb(skb);

	return 0;
}

static int cmd_complete(struct sock *sk, u16 index, u16 cmd, void *rp,
								size_t rp_len)
{
	struct sk_buff *skb;
	struct mgmt_hdr *hdr;
	struct mgmt_ev_cmd_complete *ev;

	BT_DBG("sock %p", sk);

	skb = alloc_skb(sizeof(*hdr) + sizeof(*ev) + rp_len, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	hdr = (void *) skb_put(skb, sizeof(*hdr));

	hdr->opcode = cpu_to_le16(MGMT_EV_CMD_COMPLETE);
	hdr->index = cpu_to_le16(index);
	hdr->len = cpu_to_le16(sizeof(*ev) + rp_len);

	ev = (void *) skb_put(skb, sizeof(*ev) + rp_len);
	put_unaligned_le16(cmd, &ev->opcode);

	if (rp)
		memcpy(ev->data, rp, rp_len);

	if (sock_queue_rcv_skb(sk, skb) < 0)
		kfree_skb(skb);

	return 0;
}

static int read_version(struct sock *sk)
{
	struct mgmt_rp_read_version rp;

	BT_DBG("sock %p", sk);

	rp.version = MGMT_VERSION;
	put_unaligned_le16(MGMT_REVISION, &rp.revision);

	return cmd_complete(sk, MGMT_INDEX_NONE, MGMT_OP_READ_VERSION, &rp,
								sizeof(rp));
}

static int read_index_list(struct sock *sk)
{
	struct mgmt_rp_read_index_list *rp;
	struct list_head *p;
	size_t rp_len;
	u16 count;
	int i, err;

	BT_DBG("sock %p", sk);

	read_lock(&hci_dev_list_lock);

	count = 0;
	list_for_each(p, &hci_dev_list) {
		struct hci_dev *d = list_entry(p, struct hci_dev, list);
		if (d->dev_type != HCI_BREDR)
			continue;
		count++;
	}

	rp_len = sizeof(*rp) + (2 * count);
	rp = kmalloc(rp_len, GFP_ATOMIC);
	if (!rp) {
		read_unlock(&hci_dev_list_lock);
		return -ENOMEM;
	}

	put_unaligned_le16(0, &rp->num_controllers);

	i = 0;
	list_for_each(p, &hci_dev_list) {
		struct hci_dev *d = list_entry(p, struct hci_dev, list);

		hci_del_off_timer(d);

		if (d->dev_type != HCI_BREDR)
			continue;

		set_bit(HCI_MGMT, &d->flags);

		if (test_bit(HCI_SETUP, &d->flags))
			continue;

		put_unaligned_le16(d->id, &rp->index[i++]);
		put_unaligned_le16((u16)i, &rp->num_controllers);
		BT_DBG("Added hci%u", d->id);
	}

	read_unlock(&hci_dev_list_lock);

	err = cmd_complete(sk, MGMT_INDEX_NONE, MGMT_OP_READ_INDEX_LIST, rp,
									rp_len);

	kfree(rp);

	return err;
}

static int read_controller_info(struct sock *sk, u16 index)
{
	struct mgmt_rp_read_info rp;
	struct hci_dev *hdev;

	BT_DBG("sock %p hci%u", sk, index);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_READ_INFO, ENODEV);

	hci_del_off_timer(hdev);

	hci_dev_lock_bh(hdev);

	set_bit(HCI_MGMT, &hdev->flags);

	memset(&rp, 0, sizeof(rp));

	rp.type = hdev->dev_type;

	rp.powered = test_bit(HCI_UP, &hdev->flags);
	rp.connectable = test_bit(HCI_PSCAN, &hdev->flags);
	rp.discoverable = test_bit(HCI_ISCAN, &hdev->flags);
	rp.pairable = test_bit(HCI_PSCAN, &hdev->flags);

	if (test_bit(HCI_AUTH, &hdev->flags))
		rp.sec_mode = 3;
	else if (hdev->ssp_mode > 0)
		rp.sec_mode = 4;
	else
		rp.sec_mode = 2;

	bacpy(&rp.bdaddr, &hdev->bdaddr);
	memcpy(rp.features, hdev->features, 8);
	memcpy(rp.dev_class, hdev->dev_class, 3);
	put_unaligned_le16(hdev->manufacturer, &rp.manufacturer);
	rp.hci_ver = hdev->hci_ver;
	put_unaligned_le16(hdev->hci_rev, &rp.hci_rev);

	memcpy(rp.name, hdev->dev_name, sizeof(hdev->dev_name));

	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return cmd_complete(sk, index, MGMT_OP_READ_INFO, &rp, sizeof(rp));
}

static void mgmt_pending_free_worker(struct work_struct *work)
{
	struct mgmt_pending_free_work *free_work =
		container_of(work, struct mgmt_pending_free_work, work);

	BT_DBG("sk %p", free_work->sk);

	sock_put(free_work->sk);
	kfree(free_work);
}

static void mgmt_pending_free(struct pending_cmd *cmd)
{
	struct mgmt_pending_free_work *free_work;
	struct sock *sk = cmd->sk;

	BT_DBG("opcode %d, sk %p", cmd->opcode, sk);

	kfree(cmd->param);
	kfree(cmd);

	free_work = kzalloc(sizeof(*free_work), GFP_ATOMIC);
	if (free_work) {
		INIT_WORK(&free_work->work, mgmt_pending_free_worker);
		free_work->sk = sk;

		if (!schedule_work(&free_work->work))
			kfree(free_work);
	}
}

static struct pending_cmd *mgmt_pending_add(struct sock *sk, u16 opcode,
						u16 index, void *data, u16 len)
{
	struct pending_cmd *cmd;

	BT_DBG("%d", opcode);

	cmd = kmalloc(sizeof(*cmd), GFP_ATOMIC);
	if (!cmd)
		return NULL;

	cmd->opcode = opcode;
	cmd->index = index;

	cmd->param = kmalloc(len, GFP_ATOMIC);
	if (!cmd->param) {
		kfree(cmd);
		return NULL;
	}

	if (data)
		memcpy(cmd->param, data, len);

	cmd->sk = sk;
	sock_hold(sk);

	list_add(&cmd->list, &cmd_list);

	return cmd;
}

static void mgmt_pending_foreach(u16 opcode, int index,
				void (*cb)(struct pending_cmd *cmd, void *data),
				void *data)
{
	struct list_head *p, *n;

	BT_DBG(" %d", opcode);

	list_for_each_safe(p, n, &cmd_list) {
		struct pending_cmd *cmd;

		cmd = list_entry(p, struct pending_cmd, list);

		if (cmd->opcode != opcode)
			continue;

		if (index >= 0 && cmd->index != index)
			continue;

		cb(cmd, data);
	}
}

static struct pending_cmd *mgmt_pending_find(u16 opcode, int index)
{
	struct list_head *p;

	BT_DBG(" %d", opcode);

	list_for_each(p, &cmd_list) {
		struct pending_cmd *cmd;

		cmd = list_entry(p, struct pending_cmd, list);

		if (cmd->opcode != opcode)
			continue;

		if (index >= 0 && cmd->index != index)
			continue;

		return cmd;
	}

	return NULL;
}

static void mgmt_pending_remove(struct pending_cmd *cmd)
{
	BT_DBG(" %d", cmd->opcode);

	list_del(&cmd->list);
	mgmt_pending_free(cmd);
}

static int set_powered(struct sock *sk, u16 index, unsigned char *data, u16 len)
{
	struct mgmt_mode *cp;
	struct hci_dev *hdev;
	struct pending_cmd *cmd;
	int err, up;

	cp = (void *) data;

	BT_DBG("request for hci%u", index);

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_SET_POWERED, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_SET_POWERED, ENODEV);

	hci_dev_lock_bh(hdev);

	up = test_bit(HCI_UP, &hdev->flags);
	if ((cp->val && up) || (!cp->val && !up)) {
		err = cmd_status(sk, index, MGMT_OP_SET_POWERED, EALREADY);
		goto failed;
	}

	if (mgmt_pending_find(MGMT_OP_SET_POWERED, index)) {
		err = cmd_status(sk, index, MGMT_OP_SET_POWERED, EBUSY);
		goto failed;
	}

	cmd = mgmt_pending_add(sk, MGMT_OP_SET_POWERED, index, data, len);
	if (!cmd) {
		err = -ENOMEM;
		goto failed;
	}

	if (cp->val)
		queue_work(hdev->workqueue, &hdev->power_on);
	else
		queue_work(hdev->workqueue, &hdev->power_off);

	err = 0;

failed:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);
	return err;
}

static u8 get_service_classes(struct hci_dev *hdev)
{
	struct list_head *p;
	u8 val = 0;

	list_for_each(p, &hdev->uuids) {
		struct bt_uuid *uuid = list_entry(p, struct bt_uuid, list);

		val |= uuid->svc_hint;
	}

	return val;
}

static int update_class(struct hci_dev *hdev)
{
	u8 cod[3];

	BT_DBG("%s", hdev->name);

	if (test_bit(HCI_SERVICE_CACHE, &hdev->flags))
		return 0;

	cod[0] = hdev->minor_class;
	cod[1] = hdev->major_class;
	cod[2] = get_service_classes(hdev);

	if (memcmp(cod, hdev->dev_class, 3) == 0)
		return 0;

	return hci_send_cmd(hdev, HCI_OP_WRITE_CLASS_OF_DEV, sizeof(cod), cod);
}

static int set_limited_discoverable(struct sock *sk, u16 index,
						unsigned char *data, u16 len)
{
	struct mgmt_mode *cp;
	struct hci_dev *hdev;
	struct pending_cmd *cmd;
	struct hci_cp_write_current_iac_lap dcp;
	int update_cod;
	int err = 0;
	/* General Inquiry LAP: 0x9E8B33, Limited Inquiry LAP: 0x9E8B00 */
	u8 lap[] = { 0x33, 0x8b, 0x9e, 0x00, 0x8b, 0x9e };

	cp = (void *) data;

	BT_DBG("hci%u discoverable: %d", index, cp->val);

	if (!cp || len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_SET_LIMIT_DISCOVERABLE,
									EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_SET_LIMIT_DISCOVERABLE,
									ENODEV);

	hci_dev_lock_bh(hdev);

	if (!test_bit(HCI_UP, &hdev->flags)) {
		err = cmd_status(sk, index, MGMT_OP_SET_LIMIT_DISCOVERABLE,
								ENETDOWN);
		goto failed;
	}

	if (mgmt_pending_find(MGMT_OP_SET_LIMIT_DISCOVERABLE, index)) {
		err = cmd_status(sk, index, MGMT_OP_SET_LIMIT_DISCOVERABLE,
									EBUSY);
		goto failed;
	}

	if (cp->val == test_bit(HCI_ISCAN, &hdev->flags) &&
					test_bit(HCI_PSCAN, &hdev->flags)) {
		err = cmd_status(sk, index, MGMT_OP_SET_LIMIT_DISCOVERABLE,
								EALREADY);
		goto failed;
	}

	cmd = mgmt_pending_add(sk, MGMT_OP_SET_LIMIT_DISCOVERABLE, index, data,
									len);
	if (!cmd) {
		err = -ENOMEM;
		goto failed;
	}

	memset(&dcp, 0, sizeof(dcp));
	dcp.num_current_iac = cp->val ? 2 : 1;
	memcpy(&dcp.lap, lap, dcp.num_current_iac * 3);
	update_cod = 1;

	if (cp->val) {
		if (hdev->major_class & MGMT_MAJOR_CLASS_LIMITED)
			update_cod = 0;
		hdev->major_class |= MGMT_MAJOR_CLASS_LIMITED;
	} else {
		if (!(hdev->major_class & MGMT_MAJOR_CLASS_LIMITED))
			update_cod = 0;
		hdev->major_class &= ~MGMT_MAJOR_CLASS_LIMITED;
	}

	if (update_cod)
		err = update_class(hdev);

	if (err >= 0)
		err = hci_send_cmd(hdev, HCI_OP_WRITE_CURRENT_IAC_LAP,
							sizeof(dcp), &dcp);

	if (err < 0)
		mgmt_pending_remove(cmd);

failed:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int set_discoverable(struct sock *sk, u16 index, unsigned char *data,
									u16 len)
{
	struct mgmt_mode *cp;
	struct hci_dev *hdev;
	struct pending_cmd *cmd;
	u8 scan;
	int err;

	cp = (void *) data;

	BT_DBG("request for hci%u", index);

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_SET_DISCOVERABLE, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_SET_DISCOVERABLE, ENODEV);

	hci_dev_lock_bh(hdev);

	if (!test_bit(HCI_UP, &hdev->flags)) {
		err = cmd_status(sk, index, MGMT_OP_SET_DISCOVERABLE, ENETDOWN);
		goto failed;
	}

	if (mgmt_pending_find(MGMT_OP_SET_DISCOVERABLE, index) ||
			mgmt_pending_find(MGMT_OP_SET_CONNECTABLE, index)) {
		err = cmd_status(sk, index, MGMT_OP_SET_DISCOVERABLE, EBUSY);
		goto failed;
	}

	if (cp->val == test_bit(HCI_ISCAN, &hdev->flags) &&
					test_bit(HCI_PSCAN, &hdev->flags)) {
		err = cmd_status(sk, index, MGMT_OP_SET_DISCOVERABLE, EALREADY);
		goto failed;
	}

	cmd = mgmt_pending_add(sk, MGMT_OP_SET_DISCOVERABLE, index, data, len);
	if (!cmd) {
		err = -ENOMEM;
		goto failed;
	}

	scan = SCAN_PAGE;

	if (cp->val)
		scan |= SCAN_INQUIRY;

	err = hci_send_cmd(hdev, HCI_OP_WRITE_SCAN_ENABLE, 1, &scan);
	if (err < 0)
		mgmt_pending_remove(cmd);

failed:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int set_connectable(struct sock *sk, u16 index, unsigned char *data,
									u16 len)
{
	struct mgmt_mode *cp;
	struct hci_dev *hdev;
	struct pending_cmd *cmd;
	u8 scan;
	int err;

	cp = (void *) data;

	BT_DBG("request for hci%u", index);

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_SET_CONNECTABLE, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_SET_CONNECTABLE, ENODEV);

	hci_dev_lock_bh(hdev);

	if (!test_bit(HCI_UP, &hdev->flags)) {
		err = cmd_status(sk, index, MGMT_OP_SET_CONNECTABLE, ENETDOWN);
		goto failed;
	}

	if (mgmt_pending_find(MGMT_OP_SET_DISCOVERABLE, index) ||
			mgmt_pending_find(MGMT_OP_SET_CONNECTABLE, index)) {
		err = cmd_status(sk, index, MGMT_OP_SET_CONNECTABLE, EBUSY);
		goto failed;
	}

	if (cp->val == test_bit(HCI_PSCAN, &hdev->flags)) {
		err = cmd_status(sk, index, MGMT_OP_SET_CONNECTABLE, EALREADY);
		goto failed;
	}

	cmd = mgmt_pending_add(sk, MGMT_OP_SET_CONNECTABLE, index, data, len);
	if (!cmd) {
		err = -ENOMEM;
		goto failed;
	}

	if (cp->val)
		scan = SCAN_PAGE;
	else
		scan = 0;

	err = hci_send_cmd(hdev, HCI_OP_WRITE_SCAN_ENABLE, 1, &scan);
	if (err < 0)
		mgmt_pending_remove(cmd);

failed:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int mgmt_event(u16 event, u16 index, void *data, u16 data_len,
							struct sock *skip_sk)
{
	struct sk_buff *skb;
	struct mgmt_hdr *hdr;

	BT_DBG("hci%d %d", index, event);

	skb = alloc_skb(sizeof(*hdr) + data_len, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	bt_cb(skb)->channel = HCI_CHANNEL_CONTROL;

	hdr = (void *) skb_put(skb, sizeof(*hdr));
	hdr->opcode = cpu_to_le16(event);
	hdr->index = cpu_to_le16(index);
	hdr->len = cpu_to_le16(data_len);

	if (data)
		memcpy(skb_put(skb, data_len), data, data_len);

	hci_send_to_sock(NULL, skb, skip_sk);
	kfree_skb(skb);

	return 0;
}

static int send_mode_rsp(struct sock *sk, u16 opcode, u16 index, u8 val)
{
	struct mgmt_mode rp;

	rp.val = val;

	return cmd_complete(sk, index, opcode, &rp, sizeof(rp));
}

static int set_pairable(struct sock *sk, u16 index, unsigned char *data,
									u16 len)
{
	struct mgmt_mode *cp, ev;
	struct hci_dev *hdev;
	int err;

	cp = (void *) data;

	BT_DBG("request for hci%u", index);

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_SET_PAIRABLE, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_SET_PAIRABLE, ENODEV);

	hci_dev_lock_bh(hdev);

	if (cp->val)
		set_bit(HCI_PAIRABLE, &hdev->flags);
	else
		clear_bit(HCI_PAIRABLE, &hdev->flags);

	err = send_mode_rsp(sk, MGMT_OP_SET_PAIRABLE, index, cp->val);
	if (err < 0)
		goto failed;

	ev.val = cp->val;

	err = mgmt_event(MGMT_EV_PAIRABLE, index, &ev, sizeof(ev), sk);

failed:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

#define EIR_FLAGS		0x01 /* flags */
#define EIR_UUID16_SOME		0x02 /* 16-bit UUID, more available */
#define EIR_UUID16_ALL		0x03 /* 16-bit UUID, all listed */
#define EIR_UUID32_SOME		0x04 /* 32-bit UUID, more available */
#define EIR_UUID32_ALL		0x05 /* 32-bit UUID, all listed */
#define EIR_UUID128_SOME	0x06 /* 128-bit UUID, more available */
#define EIR_UUID128_ALL		0x07 /* 128-bit UUID, all listed */
#define EIR_NAME_SHORT		0x08 /* shortened local name */
#define EIR_NAME_COMPLETE	0x09 /* complete local name */
#define EIR_TX_POWER		0x0A /* transmit power level */
#define EIR_DEVICE_ID		0x10 /* device ID */

#define PNP_INFO_SVCLASS_ID		0x1200

static u8 bluetooth_base_uuid[] = {
			0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
			0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u16 get_uuid16(u8 *uuid128)
{
	u32 val;
	int i;

	for (i = 0; i < 12; i++) {
		if (bluetooth_base_uuid[i] != uuid128[i])
			return 0;
	}

	memcpy(&val, &uuid128[12], 4);

	val = le32_to_cpu(val);
	if (val > 0xffff)
		return 0;

	return (u16) val;
}

static void create_eir(struct hci_dev *hdev, u8 *data)
{
	u8 *ptr = data;
	u16 eir_len = 0;
	u16 uuid16_list[HCI_MAX_EIR_LENGTH / sizeof(u16)];
	int i, truncated = 0;
	struct list_head *p;
	size_t name_len;

	name_len = strnlen(hdev->dev_name, HCI_MAX_EIR_LENGTH);

	if (name_len > 0) {
		/* EIR Data type */
		if (name_len > 48) {
			name_len = 48;
			ptr[1] = EIR_NAME_SHORT;
		} else
			ptr[1] = EIR_NAME_COMPLETE;

		/* EIR Data length */
		ptr[0] = name_len + 1;

		memcpy(ptr + 2, hdev->dev_name, name_len);

		eir_len += (name_len + 2);
		ptr += (name_len + 2);
	}

	memset(uuid16_list, 0, sizeof(uuid16_list));

	/* Group all UUID16 types */
	list_for_each(p, &hdev->uuids) {
		struct bt_uuid *uuid = list_entry(p, struct bt_uuid, list);
		u16 uuid16;

		uuid16 = get_uuid16(uuid->uuid);
		if (uuid16 == 0)
			return;

		if (uuid16 < 0x1100)
			continue;

		if (uuid16 == PNP_INFO_SVCLASS_ID)
			continue;

		/* Stop if not enough space to put next UUID */
		if (eir_len + 2 + sizeof(u16) > HCI_MAX_EIR_LENGTH) {
			truncated = 1;
			break;
		}

		/* Check for duplicates */
		for (i = 0; uuid16_list[i] != 0; i++)
			if (uuid16_list[i] == uuid16)
				break;

		if (uuid16_list[i] == 0) {
			uuid16_list[i] = uuid16;
			eir_len += sizeof(u16);
		}
	}

	if (uuid16_list[0] != 0) {
		u8 *length = ptr;

		/* EIR Data type */
		ptr[1] = truncated ? EIR_UUID16_SOME : EIR_UUID16_ALL;

		ptr += 2;
		eir_len += 2;

		for (i = 0; uuid16_list[i] != 0; i++) {
			*ptr++ = (uuid16_list[i] & 0x00ff);
			*ptr++ = (uuid16_list[i] & 0xff00) >> 8;
		}

		/* EIR Data length */
		*length = (i * sizeof(u16)) + 1;
	}
}

static int update_eir(struct hci_dev *hdev)
{
	struct hci_cp_write_eir cp;

	if (!(hdev->features[6] & LMP_EXT_INQ))
		return 0;

	if (hdev->ssp_mode == 0)
		return 0;

	if (test_bit(HCI_SERVICE_CACHE, &hdev->flags))
		return 0;

	memset(&cp, 0, sizeof(cp));

	create_eir(hdev, cp.data);

	if (memcmp(cp.data, hdev->eir, sizeof(cp.data)) == 0)
		return 0;

	memcpy(hdev->eir, cp.data, sizeof(cp.data));

	return hci_send_cmd(hdev, HCI_OP_WRITE_EIR, sizeof(cp), &cp);
}

static int add_uuid(struct sock *sk, u16 index, unsigned char *data, u16 len)
{
	struct mgmt_cp_add_uuid *cp;
	struct hci_dev *hdev;
	struct bt_uuid *uuid;
	int err;

	cp = (void *) data;

	BT_DBG("request for hci%u", index);

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_ADD_UUID, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_ADD_UUID, ENODEV);

	hci_dev_lock_bh(hdev);

	uuid = kmalloc(sizeof(*uuid), GFP_ATOMIC);
	if (!uuid) {
		err = -ENOMEM;
		goto failed;
	}

	memcpy(uuid->uuid, cp->uuid, 16);
	uuid->svc_hint = cp->svc_hint;

	list_add(&uuid->list, &hdev->uuids);

	if (test_bit(HCI_UP, &hdev->flags)) {

		err = update_class(hdev);
		if (err < 0)
			goto failed;

		err = update_eir(hdev);
		if (err < 0)
			goto failed;
	} else
		err = 0;

	err = cmd_complete(sk, index, MGMT_OP_ADD_UUID, NULL, 0);

failed:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int remove_uuid(struct sock *sk, u16 index, unsigned char *data, u16 len)
{
	struct list_head *p, *n;
	struct mgmt_cp_remove_uuid *cp;
	struct hci_dev *hdev;
	u8 bt_uuid_any[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int err, found;

	cp = (void *) data;

	BT_DBG("request for hci%u", index);

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_REMOVE_UUID, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_REMOVE_UUID, ENODEV);

	hci_dev_lock_bh(hdev);

	if (memcmp(cp->uuid, bt_uuid_any, 16) == 0) {
		err = hci_uuids_clear(hdev);
		goto unlock;
	}

	found = 0;

	list_for_each_safe(p, n, &hdev->uuids) {
		struct bt_uuid *match = list_entry(p, struct bt_uuid, list);

		if (memcmp(match->uuid, cp->uuid, 16) != 0)
			continue;

		list_del(&match->list);
		found++;
	}

	if (found == 0) {
		err = cmd_status(sk, index, MGMT_OP_REMOVE_UUID, ENOENT);
		goto unlock;
	}

	if (test_bit(HCI_UP, &hdev->flags)) {
		err = update_class(hdev);
		if (err < 0)
			goto unlock;

		err = update_eir(hdev);
		if (err < 0)
			goto unlock;
	} else
		err = 0;

	err = cmd_complete(sk, index, MGMT_OP_REMOVE_UUID, NULL, 0);

unlock:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int set_dev_class(struct sock *sk, u16 index, unsigned char *data,
									u16 len)
{
	struct hci_dev *hdev;
	struct mgmt_cp_set_dev_class *cp;
	int err;

	cp = (void *) data;

	BT_DBG("request for hci%u", index);

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_SET_DEV_CLASS, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_SET_DEV_CLASS, ENODEV);

	hci_dev_lock_bh(hdev);

	hdev->major_class &= ~MGMT_MAJOR_CLASS_MASK;
	hdev->major_class |= cp->major & MGMT_MAJOR_CLASS_MASK;
	hdev->minor_class = cp->minor;

	if (test_bit(HCI_UP, &hdev->flags))
		err = update_class(hdev);
	else
		err = 0;

	if (err == 0)
		err = cmd_complete(sk, index, MGMT_OP_SET_DEV_CLASS, NULL, 0);

	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int set_service_cache(struct sock *sk, u16 index,  unsigned char *data,
									u16 len)
{
	struct hci_dev *hdev;
	struct mgmt_cp_set_service_cache *cp;
	int err;

	cp = (void *) data;

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_SET_SERVICE_CACHE, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_SET_SERVICE_CACHE, ENODEV);

	hci_dev_lock_bh(hdev);

	BT_DBG("hci%u enable %d", index, cp->enable);

	if (cp->enable) {
		set_bit(HCI_SERVICE_CACHE, &hdev->flags);
		err = 0;
	} else {
		clear_bit(HCI_SERVICE_CACHE, &hdev->flags);
		if (test_bit(HCI_UP, &hdev->flags)) {
			err = update_class(hdev);
			if (err == 0)
				err = update_eir(hdev);
		} else
			err = 0;
	}

	if (err == 0)
		err = cmd_complete(sk, index, MGMT_OP_SET_SERVICE_CACHE, NULL,
									0);

	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int load_keys(struct sock *sk, u16 index, unsigned char *data, u16 len)
{
	struct hci_dev *hdev;
	struct mgmt_cp_load_keys *cp;
	u16 key_count, expected_len;
	int i, err;

	cp = (void *) data;

	if (len < sizeof(*cp))
		return -EINVAL;

	key_count = get_unaligned_le16(&cp->key_count);

	expected_len = sizeof(*cp) + key_count * sizeof(struct mgmt_key_info);
	if (expected_len > len) {
		BT_ERR("load_keys: expected at least %u bytes, got %u bytes",
							expected_len, len);
		return -EINVAL;
	}

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_LOAD_KEYS, ENODEV);

	BT_DBG("hci%u debug_keys %u key_count %u", index, cp->debug_keys,
								key_count);

	hci_dev_lock_bh(hdev);

	hci_link_keys_clear(hdev);

	set_bit(HCI_LINK_KEYS, &hdev->flags);

	if (cp->debug_keys)
		set_bit(HCI_DEBUG_KEYS, &hdev->flags);
	else
		clear_bit(HCI_DEBUG_KEYS, &hdev->flags);

	len -= sizeof(*cp);
	i = 0;

	while (i < len) {
		struct mgmt_key_info *key = (void *) cp->keys + i;

		i += sizeof(*key);

		if (key->key_type == KEY_TYPE_LTK) {
			struct key_master_id *id = (void *) key->data;

			if (key->dlen != sizeof(struct key_master_id))
				continue;

			hci_add_ltk(hdev, 0, &key->bdaddr, key->addr_type,
					key->pin_len, key->auth, id->ediv,
					id->rand, key->val);

			continue;
		}

		hci_add_link_key(hdev, 0, &key->bdaddr, key->val, key->key_type,
								key->pin_len);
	}

	err = cmd_complete(sk, index, MGMT_OP_LOAD_KEYS, NULL, 0);

	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int remove_key(struct sock *sk, u16 index, unsigned char *data, u16 len)
{
	struct hci_dev *hdev;
	struct mgmt_cp_remove_key *cp;
	struct hci_conn *conn;
	int err;

	cp = (void *) data;

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_REMOVE_KEY, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_REMOVE_KEY, ENODEV);

	hci_dev_lock_bh(hdev);

	err = hci_remove_link_key(hdev, &cp->bdaddr);
	if (err < 0) {
		err = cmd_status(sk, index, MGMT_OP_REMOVE_KEY, -err);
		goto unlock;
	}

	err = 0;

	if (!test_bit(HCI_UP, &hdev->flags) || !cp->disconnect)
		goto unlock;

	conn = hci_conn_hash_lookup_ba(hdev, ACL_LINK, &cp->bdaddr);
	if (conn) {
		struct hci_cp_disconnect dc;

		put_unaligned_le16(conn->handle, &dc.handle);
		dc.reason = 0x13; /* Remote User Terminated Connection */
		err = hci_send_cmd(hdev, HCI_OP_DISCONNECT, 0, NULL);
	}

unlock:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int disconnect(struct sock *sk, u16 index, unsigned char *data, u16 len)
{
	struct hci_dev *hdev;
	struct mgmt_cp_disconnect *cp;
	struct hci_cp_disconnect dc;
	struct pending_cmd *cmd;
	struct hci_conn *conn;
	int err;

	BT_DBG("");

	cp = (void *) data;

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_DISCONNECT, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_DISCONNECT, ENODEV);

	hci_dev_lock_bh(hdev);

	if (!test_bit(HCI_UP, &hdev->flags)) {
		err = cmd_status(sk, index, MGMT_OP_DISCONNECT, ENETDOWN);
		goto failed;
	}

	if (mgmt_pending_find(MGMT_OP_DISCONNECT, index)) {
		err = cmd_status(sk, index, MGMT_OP_DISCONNECT, EBUSY);
		goto failed;
	}

	conn = hci_conn_hash_lookup_ba(hdev, ACL_LINK, &cp->bdaddr);
	if (!conn) {
		conn = hci_conn_hash_lookup_ba(hdev, LE_LINK, &cp->bdaddr);
		if (!conn) {
			err = cmd_status(sk, index, MGMT_OP_DISCONNECT,
							ENOTCONN);
			goto failed;
		}
	}

	cmd = mgmt_pending_add(sk, MGMT_OP_DISCONNECT, index, data, len);
	if (!cmd) {
		err = -ENOMEM;
		goto failed;
	}

	put_unaligned_le16(conn->handle, &dc.handle);
	dc.reason = 0x13; /* Remote User Terminated Connection */

	err = hci_send_cmd(hdev, HCI_OP_DISCONNECT, sizeof(dc), &dc);
	if (err < 0)
		mgmt_pending_remove(cmd);

failed:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int get_connections(struct sock *sk, u16 index)
{
	struct mgmt_rp_get_connections *rp;
	struct hci_dev *hdev;
	struct list_head *p;
	size_t rp_len;
	u16 count;
	int i, err;

	BT_DBG("");

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_GET_CONNECTIONS, ENODEV);

	hci_dev_lock_bh(hdev);

	count = 0;
	list_for_each(p, &hdev->conn_hash.list) {
		count++;
	}

	rp_len = sizeof(*rp) + (count * sizeof(bdaddr_t));
	rp = kmalloc(rp_len, GFP_ATOMIC);
	if (!rp) {
		err = -ENOMEM;
		goto unlock;
	}

	put_unaligned_le16(count, &rp->conn_count);

	read_lock(&hci_dev_list_lock);

	i = 0;
	list_for_each(p, &hdev->conn_hash.list) {
		struct hci_conn *c = list_entry(p, struct hci_conn, list);

		bacpy(&rp->conn[i++], &c->dst);
	}

	read_unlock(&hci_dev_list_lock);

	err = cmd_complete(sk, index, MGMT_OP_GET_CONNECTIONS, rp, rp_len);

unlock:
	kfree(rp);
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);
	return err;
}

static int pin_code_reply(struct sock *sk, u16 index, unsigned char *data,
									u16 len)
{
	struct hci_dev *hdev;
	struct mgmt_cp_pin_code_reply *cp;
	struct hci_cp_pin_code_reply reply;
	struct pending_cmd *cmd;
	int err;

	BT_DBG("");

	cp = (void *) data;

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_PIN_CODE_REPLY, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_PIN_CODE_REPLY, ENODEV);

	hci_dev_lock_bh(hdev);

	if (!test_bit(HCI_UP, &hdev->flags)) {
		err = cmd_status(sk, index, MGMT_OP_PIN_CODE_REPLY, ENETDOWN);
		goto failed;
	}

	cmd = mgmt_pending_add(sk, MGMT_OP_PIN_CODE_REPLY, index, data, len);
	if (!cmd) {
		err = -ENOMEM;
		goto failed;
	}

	bacpy(&reply.bdaddr, &cp->bdaddr);
	reply.pin_len = cp->pin_len;
	memcpy(reply.pin_code, cp->pin_code, 16);

	err = hci_send_cmd(hdev, HCI_OP_PIN_CODE_REPLY, sizeof(reply), &reply);
	if (err < 0)
		mgmt_pending_remove(cmd);

failed:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int encrypt_link(struct sock *sk, u16 index, unsigned char *data,
									u16 len)
{
	struct hci_dev *hdev;
	struct mgmt_cp_encrypt_link *cp;
	struct hci_cp_set_conn_encrypt enc;
	struct hci_conn *conn;
	int err = 0;

	BT_DBG("");

	cp = (void *) data;

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_ENCRYPT_LINK, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_ENCRYPT_LINK, ENODEV);

	hci_dev_lock(hdev);

	if (!test_bit(HCI_UP, &hdev->flags)) {
		err = cmd_status(sk, index, MGMT_OP_ENCRYPT_LINK, ENETDOWN);
		goto failed;
	}

	conn = hci_conn_hash_lookup_ba(hdev, ACL_LINK,
					&cp->bdaddr);
	if (!conn)
		return cmd_status(sk, index, MGMT_OP_ENCRYPT_LINK, ENOTCONN);

	if (test_and_set_bit(HCI_CONN_ENCRYPT_PEND, &conn->pend))
		return cmd_status(sk, index, MGMT_OP_ENCRYPT_LINK, EINPROGRESS);

	if (conn->link_mode & HCI_LM_AUTH) {
		enc.handle = cpu_to_le16(conn->handle);
		enc.encrypt = cp->enable;
		err = hci_send_cmd(hdev,
				HCI_OP_SET_CONN_ENCRYPT, sizeof(enc), &enc);
	} else {
		conn->auth_initiator = 1;
		if (!test_and_set_bit(HCI_CONN_AUTH_PEND, &conn->pend)) {
			struct hci_cp_auth_requested cp;
			cp.handle = cpu_to_le16(conn->handle);
			err = hci_send_cmd(conn->hdev,
				HCI_OP_AUTH_REQUESTED, sizeof(cp), &cp);
		}
	}

failed:
	hci_dev_unlock(hdev);
	hci_dev_put(hdev);

	return err;
}


static int pin_code_neg_reply(struct sock *sk, u16 index, unsigned char *data,
									u16 len)
{
	struct hci_dev *hdev;
	struct mgmt_cp_pin_code_neg_reply *cp;
	struct pending_cmd *cmd;
	int err;

	BT_DBG("");

	cp = (void *) data;

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_PIN_CODE_NEG_REPLY,
									EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_PIN_CODE_NEG_REPLY,
									ENODEV);

	hci_dev_lock_bh(hdev);

	if (!test_bit(HCI_UP, &hdev->flags)) {
		err = cmd_status(sk, index, MGMT_OP_PIN_CODE_NEG_REPLY,
								ENETDOWN);
		goto failed;
	}

	cmd = mgmt_pending_add(sk, MGMT_OP_PIN_CODE_NEG_REPLY, index,
								data, len);
	if (!cmd) {
		err = -ENOMEM;
		goto failed;
	}

	err = hci_send_cmd(hdev, HCI_OP_PIN_CODE_NEG_REPLY, sizeof(cp->bdaddr),
								&cp->bdaddr);
	if (err < 0)
		mgmt_pending_remove(cmd);

failed:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int set_io_capability(struct sock *sk, u16 index, unsigned char *data,
									u16 len)
{
	struct hci_dev *hdev;
	struct mgmt_cp_set_io_capability *cp;

	BT_DBG("");

	cp = (void *) data;

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_SET_IO_CAPABILITY, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_SET_IO_CAPABILITY, ENODEV);

	hci_dev_lock_bh(hdev);

	hdev->io_capability = cp->io_capability;

	BT_DBG("%s IO capability set to 0x%02x", hdev->name,
							hdev->io_capability);

	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return cmd_complete(sk, index, MGMT_OP_SET_IO_CAPABILITY, NULL, 0);
}

static inline struct pending_cmd *find_pairing(struct hci_conn *conn)
{
	struct hci_dev *hdev = conn->hdev;
	struct list_head *p;

	list_for_each(p, &cmd_list) {
		struct pending_cmd *cmd;

		cmd = list_entry(p, struct pending_cmd, list);

		if (cmd->opcode != MGMT_OP_PAIR_DEVICE)
			continue;

		if (cmd->index != hdev->id)
			continue;

		if (cmd->user_data != conn)
			continue;

		return cmd;
	}

	return NULL;
}

static void pairing_complete(struct pending_cmd *cmd, u8 status)
{
	struct mgmt_rp_pair_device rp;
	struct hci_conn *conn = cmd->user_data;

	BT_DBG(" %u", status);

	bacpy(&rp.bdaddr, &conn->dst);
	rp.status = status;

	cmd_complete(cmd->sk, cmd->index, MGMT_OP_PAIR_DEVICE, &rp, sizeof(rp));

	/* So we don't get further callbacks for this connection */
	conn->connect_cfm_cb = NULL;
	conn->security_cfm_cb = NULL;
	conn->disconn_cfm_cb = NULL;

	mgmt_pending_remove(cmd);
}

static void pairing_complete_cb(struct hci_conn *conn, u8 status)
{
	struct pending_cmd *cmd;

	BT_DBG(" %u", status);

	cmd = find_pairing(conn);
	if (!cmd) {
		BT_DBG("Unable to find a pending command");
		return;
	}

	pairing_complete(cmd, status);
	hci_conn_put(conn);
}

static void pairing_security_complete_cb(struct hci_conn *conn, u8 status)
{
	struct pending_cmd *cmd;

	BT_DBG(" %u", status);

	cmd = find_pairing(conn);
	if (!cmd) {
		BT_DBG("Unable to find a pending command");
		return;
	}

	if (conn->type == LE_LINK)
		smp_link_encrypt_cmplt(conn->l2cap_data, status,
				status ? 0 : 1);
	else
		pairing_complete(cmd, status);
}

static void pairing_connect_complete_cb(struct hci_conn *conn, u8 status)
{
	struct pending_cmd *cmd;

	BT_DBG("conn: %p %u", conn, status);

	cmd = find_pairing(conn);
	if (!cmd) {
		BT_DBG("Unable to find a pending command");
		return;
	}

	if (status)
		pairing_complete(cmd, status);

	hci_conn_put(conn);
}

static void discovery_terminated(struct pending_cmd *cmd, void *data)
{
	struct hci_dev *hdev;
	struct mgmt_mode ev = {0};

	BT_DBG("");
	hdev = hci_dev_get(cmd->index);
	if (!hdev)
		goto not_found;

	del_timer(&hdev->disco_le_timer);
	del_timer(&hdev->disco_timer);
	hci_dev_put(hdev);

not_found:
	mgmt_event(MGMT_EV_DISCOVERING, cmd->index, &ev, sizeof(ev), NULL);

	list_del(&cmd->list);

	mgmt_pending_free(cmd);
}

static int pair_device(struct sock *sk, u16 index, unsigned char *data, u16 len)
{
	struct hci_dev *hdev;
	struct mgmt_cp_pair_device *cp;
	struct pending_cmd *cmd;
	u8 sec_level, auth_type, io_cap;
	struct hci_conn *conn;
	struct adv_entry *entry;
	int err;

	BT_DBG("");

	cp = (void *) data;

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_PAIR_DEVICE, EINVAL);

	hdev = hci_dev_get(index);

	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_PAIR_DEVICE, ENODEV);

	hci_dev_lock_bh(hdev);

	io_cap = cp->io_cap;
	if (io_cap == 0x03) {
		sec_level = BT_SECURITY_MEDIUM;
		auth_type = HCI_AT_DEDICATED_BONDING;
	} else {
		sec_level = BT_SECURITY_HIGH;
		auth_type = HCI_AT_DEDICATED_BONDING_MITM;
	}

	entry = hci_find_adv_entry(hdev, &cp->bdaddr);
	if (entry && entry->flags & 0x04) {
		conn = hci_connect(hdev, LE_LINK, 0, &cp->bdaddr, sec_level,
								auth_type);
	} else {
		/* ACL-SSP does not support io_cap 0x04 (KeyboadDisplay) */
		if (io_cap == 0x04)
			io_cap = 0x01;
		conn = hci_connect(hdev, ACL_LINK, 0, &cp->bdaddr, sec_level,
								auth_type);
		conn->auth_initiator = 1;
	}

	if (IS_ERR(conn)) {
		err = PTR_ERR(conn);
		goto unlock;
	}

	if (conn->connect_cfm_cb) {
		hci_conn_put(conn);
		err = cmd_status(sk, index, MGMT_OP_PAIR_DEVICE, EBUSY);
		goto unlock;
	}

	cmd = mgmt_pending_add(sk, MGMT_OP_PAIR_DEVICE, index, data, len);
	if (!cmd) {
		err = -ENOMEM;
		hci_conn_put(conn);
		goto unlock;
	}

	conn->connect_cfm_cb = pairing_connect_complete_cb;
	conn->security_cfm_cb = pairing_security_complete_cb;
	conn->disconn_cfm_cb = pairing_complete_cb;
	conn->io_capability = io_cap;
	cmd->user_data = conn;

	if (conn->state == BT_CONNECTED &&
				hci_conn_security(conn, sec_level, auth_type))
		pairing_complete(cmd, 0);

	err = 0;

unlock:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int user_confirm_reply(struct sock *sk, u16 index, unsigned char *data,
							u16 len, u16 opcode)
{
	struct mgmt_cp_user_confirm_reply *cp = (void *) data;
	u16 mgmt_op = opcode, hci_op;
	struct pending_cmd *cmd;
	struct hci_dev *hdev;
	struct hci_conn *le_conn;
	int err;

	BT_DBG("%d", mgmt_op);

	if (mgmt_op == MGMT_OP_USER_CONFIRM_NEG_REPLY)
		hci_op = HCI_OP_USER_CONFIRM_NEG_REPLY;
	else
		hci_op = HCI_OP_USER_CONFIRM_REPLY;

	if (len < sizeof(*cp))
		return cmd_status(sk, index, mgmt_op, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, mgmt_op, ENODEV);

	hci_dev_lock_bh(hdev);

	if (!test_bit(HCI_UP, &hdev->flags)) {
		err = cmd_status(sk, index, mgmt_op, ENETDOWN);
		goto done;
	}

	le_conn = hci_conn_hash_lookup_ba(hdev, LE_LINK, &cp->bdaddr);
	if (le_conn) {
		err = le_user_confirm_reply(le_conn, mgmt_op, (void *) cp);
		goto done;
	}
	BT_DBG("BR/EDR: %s", mgmt_op == MGMT_OP_USER_CONFIRM_NEG_REPLY ?
							"Reject" : "Accept");

	cmd = mgmt_pending_add(sk, mgmt_op, index, data, len);
	if (!cmd) {
		err = -ENOMEM;
		goto done;
	}

	err = hci_send_cmd(hdev, hci_op, sizeof(cp->bdaddr), &cp->bdaddr);
	if (err < 0)
		mgmt_pending_remove(cmd);

done:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int resolve_name(struct sock *sk, u16 index, unsigned char *data,
								u16 len)
{
	struct mgmt_cp_resolve_name *mgmt_cp = (void *) data;
	struct hci_cp_remote_name_req hci_cp;
	struct hci_dev *hdev;
	struct pending_cmd *cmd;
	int err;

	BT_DBG("");

	if (len != sizeof(*mgmt_cp))
		return cmd_status(sk, index, MGMT_OP_RESOLVE_NAME, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_RESOLVE_NAME, ENODEV);

	hci_dev_lock_bh(hdev);

	cmd = mgmt_pending_add(sk, MGMT_OP_RESOLVE_NAME, index, data, len);
	if (!cmd) {
		err = -ENOMEM;
		goto failed;
	}

	memset(&hci_cp, 0, sizeof(hci_cp));
	bacpy(&hci_cp.bdaddr, &mgmt_cp->bdaddr);
	err = hci_send_cmd(hdev, HCI_OP_REMOTE_NAME_REQ, sizeof(hci_cp),
								&hci_cp);
	if (err < 0)
		mgmt_pending_remove(cmd);

failed:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int set_connection_params(struct sock *sk, u16 index,
				unsigned char *data, u16 len)
{
	struct mgmt_cp_set_connection_params *cp = (void *) data;
	struct hci_dev *hdev;
	struct hci_conn *conn;
	int err;

	BT_DBG("");

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_SET_CONNECTION_PARAMS,
									EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_SET_CONNECTION_PARAMS,
									ENODEV);

	hci_dev_lock_bh(hdev);

	conn = hci_conn_hash_lookup_ba(hdev, LE_LINK, &cp->bdaddr);
	if (!conn) {
		err = cmd_status(sk, index, MGMT_OP_SET_CONNECTION_PARAMS,
								ENOTCONN);
		goto failed;
	}

	hci_le_conn_update(conn, le16_to_cpu(cp->interval_min),
				le16_to_cpu(cp->interval_max),
				le16_to_cpu(cp->slave_latency),
				le16_to_cpu(cp->timeout_multiplier));

	err = cmd_status(sk, index, MGMT_OP_SET_CONNECTION_PARAMS, 0);

failed:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int set_rssi_reporter(struct sock *sk, u16 index,
				unsigned char *data, u16 len)
{
	struct mgmt_cp_set_rssi_reporter *cp = (void *) data;
	struct hci_dev *hdev;
	struct hci_conn *conn;
	int err = 0;

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_SET_RSSI_REPORTER,
								EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_SET_RSSI_REPORTER,
							ENODEV);

	hci_dev_lock(hdev);

	conn = hci_conn_hash_lookup_ba(hdev, LE_LINK, &cp->bdaddr);

	if (!conn) {
		err = cmd_status(sk, index, MGMT_OP_SET_RSSI_REPORTER,
						ENOTCONN);
		goto failed;
	}

	BT_DBG("updateOnThreshExceed %d ", cp->updateOnThreshExceed);
	hci_conn_set_rssi_reporter(conn, cp->rssi_threshold,
			__le16_to_cpu(cp->interval), cp->updateOnThreshExceed);

failed:
	hci_dev_unlock(hdev);
	hci_dev_put(hdev);

	return err;
}

static int unset_rssi_reporter(struct sock *sk, u16 index,
			unsigned char *data, u16 len)
{
	struct mgmt_cp_unset_rssi_reporter *cp = (void *) data;
	struct hci_dev *hdev;
	struct hci_conn *conn;
	int err = 0;

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_UNSET_RSSI_REPORTER,
					EINVAL);

	hdev = hci_dev_get(index);

	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_UNSET_RSSI_REPORTER,
					ENODEV);

	hci_dev_lock(hdev);

	conn = hci_conn_hash_lookup_ba(hdev, LE_LINK, &cp->bdaddr);

	if (!conn) {
		err = cmd_status(sk, index, MGMT_OP_UNSET_RSSI_REPORTER,
					ENOTCONN);
		goto failed;
	}

	hci_conn_unset_rssi_reporter(conn);

failed:
	hci_dev_unlock(hdev);
	hci_dev_put(hdev);

	return err;
}

static int set_local_name(struct sock *sk, u16 index, unsigned char *data,
								u16 len)
{
	struct mgmt_cp_set_local_name *mgmt_cp = (void *) data;
	struct hci_cp_write_local_name hci_cp;
	struct hci_dev *hdev;
	struct pending_cmd *cmd;
	int err;

	BT_DBG("");

	if (len != sizeof(*mgmt_cp))
		return cmd_status(sk, index, MGMT_OP_SET_LOCAL_NAME, EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_SET_LOCAL_NAME, ENODEV);

	hci_dev_lock_bh(hdev);

	cmd = mgmt_pending_add(sk, MGMT_OP_SET_LOCAL_NAME, index, data, len);
	if (!cmd) {
		err = -ENOMEM;
		goto failed;
	}

	memcpy(hci_cp.name, mgmt_cp->name, sizeof(hci_cp.name));
	err = hci_send_cmd(hdev, HCI_OP_WRITE_LOCAL_NAME, sizeof(hci_cp),
								&hci_cp);
	if (err < 0)
		mgmt_pending_remove(cmd);

failed:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static void discovery_rsp(struct pending_cmd *cmd, void *data)
{
	struct mgmt_mode ev;

	BT_DBG("");
	if (cmd->opcode == MGMT_OP_START_DISCOVERY) {
		ev.val = 1;
		cmd_status(cmd->sk, cmd->index, MGMT_OP_START_DISCOVERY, 0);
	} else {
		ev.val = 0;
		cmd_complete(cmd->sk, cmd->index, MGMT_OP_STOP_DISCOVERY,
								NULL, 0);
		if (cmd->opcode == MGMT_OP_STOP_DISCOVERY) {
			struct hci_dev *hdev = hci_dev_get(cmd->index);
			if (hdev) {
				del_timer(&hdev->disco_le_timer);
				del_timer(&hdev->disco_timer);
				hci_dev_put(hdev);
			}
		}
	}

	mgmt_event(MGMT_EV_DISCOVERING, cmd->index, &ev, sizeof(ev), NULL);

	list_del(&cmd->list);

	mgmt_pending_free(cmd);
}

void mgmt_inquiry_started(u16 index)
{
	BT_DBG("");
	mgmt_pending_foreach(MGMT_OP_START_DISCOVERY, index,
						discovery_rsp, NULL);
}

void mgmt_inquiry_complete_evt(u16 index, u8 status)
{
	struct hci_dev *hdev;
	struct hci_cp_le_set_scan_enable le_cp = {1, 0};
	struct mgmt_mode cp = {0};
	int err = -1;

	BT_DBG("");

	hdev = hci_dev_get(index);

	if (!hdev || !lmp_le_capable(hdev)) {

		mgmt_pending_foreach(MGMT_OP_STOP_DISCOVERY, index,
						discovery_terminated, NULL);

		mgmt_event(MGMT_EV_DISCOVERING, index, &cp, sizeof(cp), NULL);

		if (hdev)
			goto done;
		else
			return;
	}

	if (hdev->disco_state != SCAN_IDLE) {
		err = hci_send_cmd(hdev, HCI_OP_LE_SET_SCAN_ENABLE,
						sizeof(le_cp), &le_cp);
		if (err >= 0) {
			mod_timer(&hdev->disco_le_timer, jiffies +
				msecs_to_jiffies(hdev->disco_int_phase * 1000));
			hdev->disco_state = SCAN_LE;
		} else
			hdev->disco_state = SCAN_IDLE;
	}

	if (hdev->disco_state == SCAN_IDLE)
		mgmt_event(MGMT_EV_DISCOVERING, index, &cp, sizeof(cp), NULL);

	if (err < 0)
		mgmt_pending_foreach(MGMT_OP_STOP_DISCOVERY, index,
						discovery_terminated, NULL);

done:
	hci_dev_put(hdev);
}

void mgmt_disco_timeout(unsigned long data)
{
	struct hci_dev *hdev = (void *) data;
	struct pending_cmd *cmd;
	struct mgmt_mode cp = {0};

	BT_DBG("hci%d", hdev->id);

	hdev = hci_dev_get(hdev->id);

	if (!hdev)
		return;

	hci_dev_lock_bh(hdev);
	del_timer(&hdev->disco_le_timer);

	if (hdev->disco_state != SCAN_IDLE) {
		struct hci_cp_le_set_scan_enable le_cp = {0, 0};

		if (test_bit(HCI_UP, &hdev->flags)) {
			if (hdev->disco_state == SCAN_LE)
				hci_send_cmd(hdev, HCI_OP_LE_SET_SCAN_ENABLE,
							sizeof(le_cp), &le_cp);
			else
				hci_send_cmd(hdev, HCI_OP_INQUIRY_CANCEL, 0,
									 NULL);
		}
		hdev->disco_state = SCAN_IDLE;
	}

	mgmt_event(MGMT_EV_DISCOVERING, hdev->id, &cp, sizeof(cp), NULL);

	cmd = mgmt_pending_find(MGMT_OP_STOP_DISCOVERY, hdev->id);
	if (cmd)
		mgmt_pending_remove(cmd);

	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);
}

void mgmt_disco_le_timeout(unsigned long data)
{
	struct hci_dev *hdev = (void *)data;
	struct hci_cp_le_set_scan_enable le_cp = {0, 0};

	BT_DBG("hci%d", hdev->id);

	hdev = hci_dev_get(hdev->id);

	if (!hdev)
		return;

	hci_dev_lock_bh(hdev);

	if (test_bit(HCI_UP, &hdev->flags)) {
		if (hdev->disco_state == SCAN_LE)
			hci_send_cmd(hdev, HCI_OP_LE_SET_SCAN_ENABLE,
					sizeof(le_cp), &le_cp);

	/* re-start BR scan */
		if (hdev->disco_state != SCAN_IDLE) {
			struct hci_cp_inquiry cp = {{0x33, 0x8b, 0x9e}, 4, 0};
			hdev->disco_int_phase *= 2;
			hdev->disco_int_count = 0;
			cp.num_rsp = (u8) hdev->disco_int_phase;
			hci_send_cmd(hdev, HCI_OP_INQUIRY, sizeof(cp), &cp);
			hdev->disco_state = SCAN_BR;
		}
	}

	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);
}

static int start_discovery(struct sock *sk, u16 index)
{
	struct hci_cp_inquiry cp = {{0x33, 0x8b, 0x9e}, 8, 0};
	struct hci_dev *hdev;
	struct pending_cmd *cmd;
	int err;

	BT_DBG("");

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_START_DISCOVERY, ENODEV);

	hci_dev_lock_bh(hdev);

	if (hdev->disco_state && timer_pending(&hdev->disco_timer)) {
		err = -EBUSY;
		goto failed;
	}

	cmd = mgmt_pending_add(sk, MGMT_OP_START_DISCOVERY, index, NULL, 0);
	if (!cmd) {
		err = -ENOMEM;
		goto failed;
	}

	/* If LE Capable, we will alternate between BR/EDR and LE */
	if (lmp_le_capable(hdev)) {
		struct hci_cp_le_set_scan_parameters le_cp;

		/* Shorten BR scan params */
		cp.num_rsp = 1;
		cp.length /= 2;

		/* Setup LE scan params */
		memset(&le_cp, 0, sizeof(le_cp));
		le_cp.type = 0x01;		/* Active scanning */
		/* The recommended value for scan interval and window is
		 * 11.25 msec. It is calculated by: time = n * 0.625 msec */
		le_cp.interval = cpu_to_le16(0x0012);
		le_cp.window = cpu_to_le16(0x0012);
		le_cp.own_bdaddr_type = 0;	/* Public address */
		le_cp.filter = 0;		/* Accept all adv packets */

		hci_send_cmd(hdev, HCI_OP_LE_SET_SCAN_PARAMETERS,
						sizeof(le_cp), &le_cp);
	}

	err = hci_send_cmd(hdev, HCI_OP_INQUIRY, sizeof(cp), &cp);

	if (err < 0)
		mgmt_pending_remove(cmd);
	else if (lmp_le_capable(hdev)) {
		cmd = mgmt_pending_find(MGMT_OP_STOP_DISCOVERY, index);
		if (!cmd)
			mgmt_pending_add(sk, MGMT_OP_STOP_DISCOVERY, index,
								NULL, 0);
		hdev->disco_int_phase = 1;
		hdev->disco_int_count = 0;
		hdev->disco_state = SCAN_BR;
		del_timer(&hdev->disco_le_timer);
		del_timer(&hdev->disco_timer);
		mod_timer(&hdev->disco_timer,
				jiffies + msecs_to_jiffies(20000));
	}

failed:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	if (err < 0)
		return cmd_status(sk, index, MGMT_OP_START_DISCOVERY, -err);

	return err;
}

static int stop_discovery(struct sock *sk, u16 index)
{
	struct hci_cp_le_set_scan_enable le_cp = {0, 0};
	struct mgmt_mode mode_cp = {0};
	struct hci_dev *hdev;
	struct pending_cmd *cmd = NULL;
	int err = -EPERM;
	u8 state;

	BT_DBG("");

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_STOP_DISCOVERY, ENODEV);

	hci_dev_lock_bh(hdev);

	state = hdev->disco_state;
	hdev->disco_state = SCAN_IDLE;
	del_timer(&hdev->disco_le_timer);
	del_timer(&hdev->disco_timer);

	if (state == SCAN_LE) {
		err = hci_send_cmd(hdev, HCI_OP_LE_SET_SCAN_ENABLE,
							sizeof(le_cp), &le_cp);
		if (err >= 0) {
			mgmt_pending_foreach(MGMT_OP_STOP_DISCOVERY, index,
						discovery_terminated, NULL);

			err = cmd_complete(sk, index, MGMT_OP_STOP_DISCOVERY,
								NULL, 0);
		}
	}

	if (err < 0)
		err = hci_send_cmd(hdev, HCI_OP_INQUIRY_CANCEL, 0, NULL);

	cmd = mgmt_pending_find(MGMT_OP_STOP_DISCOVERY, index);
	if (err < 0 && cmd)
		mgmt_pending_remove(cmd);

	mgmt_event(MGMT_EV_DISCOVERING, index, &mode_cp, sizeof(mode_cp), NULL);

	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	if (err < 0)
		return cmd_status(sk, index, MGMT_OP_STOP_DISCOVERY, -err);
	else
		return err;
}

static int read_local_oob_data(struct sock *sk, u16 index)
{
	struct hci_dev *hdev;
	struct pending_cmd *cmd;
	int err;

	BT_DBG("hci%u", index);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_READ_LOCAL_OOB_DATA,
									ENODEV);

	hci_dev_lock_bh(hdev);

	if (!test_bit(HCI_UP, &hdev->flags)) {
		err = cmd_status(sk, index, MGMT_OP_READ_LOCAL_OOB_DATA,
								ENETDOWN);
		goto unlock;
	}

	if (!(hdev->features[6] & LMP_SIMPLE_PAIR)) {
		err = cmd_status(sk, index, MGMT_OP_READ_LOCAL_OOB_DATA,
								EOPNOTSUPP);
		goto unlock;
	}

	if (mgmt_pending_find(MGMT_OP_READ_LOCAL_OOB_DATA, index)) {
		err = cmd_status(sk, index, MGMT_OP_READ_LOCAL_OOB_DATA, EBUSY);
		goto unlock;
	}

	cmd = mgmt_pending_add(sk, MGMT_OP_READ_LOCAL_OOB_DATA, index, NULL, 0);
	if (!cmd) {
		err = -ENOMEM;
		goto unlock;
	}

	err = hci_send_cmd(hdev, HCI_OP_READ_LOCAL_OOB_DATA, 0, NULL);
	if (err < 0)
		mgmt_pending_remove(cmd);

unlock:
	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int add_remote_oob_data(struct sock *sk, u16 index, unsigned char *data,
									u16 len)
{
	struct hci_dev *hdev;
	struct mgmt_cp_add_remote_oob_data *cp = (void *) data;
	int err;

	BT_DBG("hci%u ", index);

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_ADD_REMOTE_OOB_DATA,
									EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_ADD_REMOTE_OOB_DATA,
									ENODEV);

	hci_dev_lock_bh(hdev);

	err = hci_add_remote_oob_data(hdev, &cp->bdaddr, cp->hash,
								cp->randomizer);
	if (err < 0)
		err = cmd_status(sk, index, MGMT_OP_ADD_REMOTE_OOB_DATA, -err);
	else
		err = cmd_complete(sk, index, MGMT_OP_ADD_REMOTE_OOB_DATA, NULL,
									0);

	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

static int remove_remote_oob_data(struct sock *sk, u16 index,
						unsigned char *data, u16 len)
{
	struct hci_dev *hdev;
	struct mgmt_cp_remove_remote_oob_data *cp = (void *) data;
	int err;

	BT_DBG("hci%u ", index);

	if (len != sizeof(*cp))
		return cmd_status(sk, index, MGMT_OP_REMOVE_REMOTE_OOB_DATA,
									EINVAL);

	hdev = hci_dev_get(index);
	if (!hdev)
		return cmd_status(sk, index, MGMT_OP_REMOVE_REMOTE_OOB_DATA,
									ENODEV);

	hci_dev_lock_bh(hdev);

	err = hci_remove_remote_oob_data(hdev, &cp->bdaddr);
	if (err < 0)
		err = cmd_status(sk, index, MGMT_OP_REMOVE_REMOTE_OOB_DATA,
									-err);
	else
		err = cmd_complete(sk, index, MGMT_OP_REMOVE_REMOTE_OOB_DATA,
								NULL, 0);

	hci_dev_unlock_bh(hdev);
	hci_dev_put(hdev);

	return err;
}

int mgmt_control(struct sock *sk, struct msghdr *msg, size_t msglen)
{
	unsigned char *buf;
	struct mgmt_hdr *hdr;
	u16 opcode, index, len;
	int err;

	BT_DBG("got %zu bytes", msglen);

	if (msglen < sizeof(*hdr))
		return -EINVAL;

	buf = kmalloc(msglen, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (memcpy_fromiovec(buf, msg->msg_iov, msglen)) {
		err = -EFAULT;
		goto done;
	}

	hdr = (struct mgmt_hdr *) buf;
	opcode = get_unaligned_le16(&hdr->opcode);
	index = get_unaligned_le16(&hdr->index);
	len = get_unaligned_le16(&hdr->len);

	if (len != msglen - sizeof(*hdr)) {
		err = -EINVAL;
		goto done;
	}

	BT_DBG("got opcode %x", opcode);
	switch (opcode) {
	case MGMT_OP_READ_VERSION:
		err = read_version(sk);
		break;
	case MGMT_OP_READ_INDEX_LIST:
		err = read_index_list(sk);
		break;
	case MGMT_OP_READ_INFO:
		err = read_controller_info(sk, index);
		break;
	case MGMT_OP_SET_POWERED:
		err = set_powered(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_SET_DISCOVERABLE:
		err = set_discoverable(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_SET_LIMIT_DISCOVERABLE:
		err = set_limited_discoverable(sk, index, buf + sizeof(*hdr),
									len);
		break;
	case MGMT_OP_SET_CONNECTABLE:
		err = set_connectable(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_SET_PAIRABLE:
		err = set_pairable(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_ADD_UUID:
		err = add_uuid(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_REMOVE_UUID:
		err = remove_uuid(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_SET_DEV_CLASS:
		err = set_dev_class(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_SET_SERVICE_CACHE:
		err = set_service_cache(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_LOAD_KEYS:
		err = load_keys(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_REMOVE_KEY:
		err = remove_key(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_DISCONNECT:
		err = disconnect(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_GET_CONNECTIONS:
		err = get_connections(sk, index);
		break;
	case MGMT_OP_PIN_CODE_REPLY:
		err = pin_code_reply(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_PIN_CODE_NEG_REPLY:
		err = pin_code_neg_reply(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_SET_IO_CAPABILITY:
		err = set_io_capability(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_PAIR_DEVICE:
		err = pair_device(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_USER_CONFIRM_REPLY:
	case MGMT_OP_USER_PASSKEY_REPLY:
	case MGMT_OP_USER_CONFIRM_NEG_REPLY:
		err = user_confirm_reply(sk, index, buf + sizeof(*hdr),
								len, opcode);
		break;
	case MGMT_OP_SET_LOCAL_NAME:
		err = set_local_name(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_START_DISCOVERY:
		err = start_discovery(sk, index);
		break;
	case MGMT_OP_STOP_DISCOVERY:
		err = stop_discovery(sk, index);
		break;
	case MGMT_OP_RESOLVE_NAME:
		err = resolve_name(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_SET_CONNECTION_PARAMS:
		err = set_connection_params(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_SET_RSSI_REPORTER:
		err = set_rssi_reporter(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_UNSET_RSSI_REPORTER:
		err = unset_rssi_reporter(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_READ_LOCAL_OOB_DATA:
		err = read_local_oob_data(sk, index);
		break;
	case MGMT_OP_ADD_REMOTE_OOB_DATA:
		err = add_remote_oob_data(sk, index, buf + sizeof(*hdr), len);
		break;
	case MGMT_OP_REMOVE_REMOTE_OOB_DATA:
		err = remove_remote_oob_data(sk, index, buf + sizeof(*hdr),
									len);
		break;
	case MGMT_OP_ENCRYPT_LINK:
		err = encrypt_link(sk, index, buf + sizeof(*hdr), len);
		break;

	default:
		BT_DBG("Unknown op %u", opcode);
		err = cmd_status(sk, index, opcode, 0x01);
		break;
	}

	if (err < 0)
		goto done;

	err = msglen;

done:
	kfree(buf);
	return err;
}

int mgmt_index_added(u16 index)
{
	BT_DBG("%d", index);
	return mgmt_event(MGMT_EV_INDEX_ADDED, index, NULL, 0, NULL);
}

int mgmt_index_removed(u16 index)
{
	BT_DBG("%d", index);
	return mgmt_event(MGMT_EV_INDEX_REMOVED, index, NULL, 0, NULL);
}

struct cmd_lookup {
	u8 val;
	struct sock *sk;
};

static void mode_rsp(struct pending_cmd *cmd, void *data)
{
	struct mgmt_mode *cp = cmd->param;
	struct cmd_lookup *match = data;

	if (cp->val != match->val)
		return;

	send_mode_rsp(cmd->sk, cmd->opcode, cmd->index, cp->val);

	list_del(&cmd->list);

	if (match->sk == NULL) {
		match->sk = cmd->sk;
		sock_hold(match->sk);
	}

	mgmt_pending_free(cmd);
}

int mgmt_powered(u16 index, u8 powered)
{
	struct mgmt_mode ev;
	struct cmd_lookup match = { powered, NULL };
	int ret;

	BT_DBG("hci%u %d", index, powered);

	mgmt_pending_foreach(MGMT_OP_SET_POWERED, index, mode_rsp, &match);

	ev.val = powered;

	ret = mgmt_event(MGMT_EV_POWERED, index, &ev, sizeof(ev), match.sk);

	if (match.sk)
		sock_put(match.sk);

	return ret;
}

int mgmt_discoverable(u16 index, u8 discoverable)
{
	struct mgmt_mode ev;
	struct cmd_lookup match = { discoverable, NULL };
	int ret;

	mgmt_pending_foreach(MGMT_OP_SET_DISCOVERABLE, index, mode_rsp, &match);

	ev.val = discoverable;

	ret = mgmt_event(MGMT_EV_DISCOVERABLE, index, &ev, sizeof(ev),
								match.sk);

	if (match.sk)
		sock_put(match.sk);

	return ret;
}

int mgmt_connectable(u16 index, u8 connectable)
{
	struct mgmt_mode ev;
	struct cmd_lookup match = { connectable, NULL };
	int ret;

	mgmt_pending_foreach(MGMT_OP_SET_CONNECTABLE, index, mode_rsp, &match);

	ev.val = connectable;

	ret = mgmt_event(MGMT_EV_CONNECTABLE, index, &ev, sizeof(ev), match.sk);

	if (match.sk)
		sock_put(match.sk);

	return ret;
}

int mgmt_new_key(u16 index, struct link_key *key, u8 bonded)
{
	struct mgmt_ev_new_key *ev;
	int err, total;

	total = sizeof(struct mgmt_ev_new_key) + key->dlen;
	ev = kzalloc(total, GFP_ATOMIC);
	if (!ev)
		return -ENOMEM;

	bacpy(&ev->key.bdaddr, &key->bdaddr);
	ev->key.addr_type = key->addr_type;
	ev->key.key_type = key->key_type;
	memcpy(ev->key.val, key->val, 16);
	ev->key.pin_len = key->pin_len;
	ev->key.auth = key->auth;
	ev->store_hint = bonded;
	ev->key.dlen = key->dlen;

	memcpy(ev->key.data, key->data, key->dlen);

	err = mgmt_event(MGMT_EV_NEW_KEY, index, ev, total, NULL);

	kfree(ev);

	return err;
}

int mgmt_connected(u16 index, bdaddr_t *bdaddr, u8 le)
{
	struct mgmt_ev_connected ev;

	bacpy(&ev.bdaddr, bdaddr);
	ev.le = le;

	return mgmt_event(MGMT_EV_CONNECTED, index, &ev, sizeof(ev), NULL);
}

static void disconnect_rsp(struct pending_cmd *cmd, void *data)
{
	struct mgmt_cp_disconnect *cp = cmd->param;
	struct sock **sk = data;
	struct mgmt_rp_disconnect rp;

	bacpy(&rp.bdaddr, &cp->bdaddr);

	cmd_complete(cmd->sk, cmd->index, MGMT_OP_DISCONNECT, &rp, sizeof(rp));

	*sk = cmd->sk;
	sock_hold(*sk);

	mgmt_pending_remove(cmd);
}

int mgmt_disconnected(u16 index, bdaddr_t *bdaddr)
{
	struct mgmt_ev_disconnected ev;
	struct sock *sk = NULL;
	int err;

	mgmt_pending_foreach(MGMT_OP_DISCONNECT, index, disconnect_rsp, &sk);

	bacpy(&ev.bdaddr, bdaddr);

	err = mgmt_event(MGMT_EV_DISCONNECTED, index, &ev, sizeof(ev), sk);

	if (sk)
		sock_put(sk);

	return err;
}

int mgmt_disconnect_failed(u16 index)
{
	struct pending_cmd *cmd;
	int err;

	cmd = mgmt_pending_find(MGMT_OP_DISCONNECT, index);
	if (!cmd)
		return -ENOENT;

	err = cmd_status(cmd->sk, index, MGMT_OP_DISCONNECT, EIO);

	mgmt_pending_remove(cmd);

	return err;
}

int mgmt_connect_failed(u16 index, bdaddr_t *bdaddr, u8 status)
{
	struct mgmt_ev_connect_failed ev;

	bacpy(&ev.bdaddr, bdaddr);
	ev.status = status;

	return mgmt_event(MGMT_EV_CONNECT_FAILED, index, &ev, sizeof(ev), NULL);
}

int mgmt_pin_code_request(u16 index, bdaddr_t *bdaddr)
{
	struct mgmt_ev_pin_code_request ev;

	BT_DBG("hci%u", index);

	bacpy(&ev.bdaddr, bdaddr);
	ev.secure = 0;

	return mgmt_event(MGMT_EV_PIN_CODE_REQUEST, index, &ev, sizeof(ev),
									NULL);
}

int mgmt_pin_code_reply_complete(u16 index, bdaddr_t *bdaddr, u8 status)
{
	struct pending_cmd *cmd;
	struct mgmt_rp_pin_code_reply rp;
	int err;

	cmd = mgmt_pending_find(MGMT_OP_PIN_CODE_REPLY, index);
	if (!cmd)
		return -ENOENT;

	bacpy(&rp.bdaddr, bdaddr);
	rp.status = status;

	err = cmd_complete(cmd->sk, index, MGMT_OP_PIN_CODE_REPLY, &rp,
								sizeof(rp));

	mgmt_pending_remove(cmd);

	return err;
}

int mgmt_pin_code_neg_reply_complete(u16 index, bdaddr_t *bdaddr, u8 status)
{
	struct pending_cmd *cmd;
	struct mgmt_rp_pin_code_reply rp;
	int err;

	cmd = mgmt_pending_find(MGMT_OP_PIN_CODE_NEG_REPLY, index);
	if (!cmd)
		return -ENOENT;

	bacpy(&rp.bdaddr, bdaddr);
	rp.status = status;

	err = cmd_complete(cmd->sk, index, MGMT_OP_PIN_CODE_NEG_REPLY, &rp,
								sizeof(rp));

	mgmt_pending_remove(cmd);

	return err;
}

int mgmt_user_confirm_request(u16 index, u8 event,
					bdaddr_t *bdaddr, __le32 value)
{
	struct mgmt_ev_user_confirm_request ev;
	struct hci_conn *conn = NULL;
	struct hci_dev *hdev;
	u8 loc_cap, rem_cap, loc_mitm, rem_mitm;

	BT_DBG("hci%u", index);

	hdev = hci_dev_get(index);

	if (!hdev)
		return -ENODEV;

	conn = hci_conn_hash_lookup_ba(hdev, ACL_LINK, bdaddr);

	ev.auto_confirm = 0;

	if (!conn || event != HCI_EV_USER_CONFIRM_REQUEST)
		goto no_auto_confirm;

	loc_cap = (conn->io_capability == 0x04) ? 0x01 : conn->io_capability;
	rem_cap = conn->remote_cap;
	loc_mitm = conn->auth_type & 0x01;
	rem_mitm = conn->remote_auth & 0x01;

	if ((conn->auth_type & HCI_AT_DEDICATED_BONDING) &&
			conn->auth_initiator && rem_cap == 0x03)
		ev.auto_confirm = 1;
	else if (loc_cap == 0x01 && (rem_cap == 0x00 || rem_cap == 0x03)) {
		if (!loc_mitm && !rem_mitm)
			value = 0;
		goto no_auto_confirm;
	}


	if ((!loc_mitm || rem_cap == 0x03) && (!rem_mitm || loc_cap == 0x03))
		ev.auto_confirm = 1;

no_auto_confirm:
	bacpy(&ev.bdaddr, bdaddr);
	ev.event = event;
	put_unaligned_le32(value, &ev.value);

	hci_dev_put(hdev);

	return mgmt_event(MGMT_EV_USER_CONFIRM_REQUEST, index, &ev, sizeof(ev),
									NULL);
}

int mgmt_user_passkey_request(u16 index, bdaddr_t *bdaddr)
{
	struct mgmt_ev_user_passkey_request ev;

	BT_DBG("hci%u", index);

	bacpy(&ev.bdaddr, bdaddr);

	return mgmt_event(MGMT_EV_USER_PASSKEY_REQUEST, index, &ev, sizeof(ev),
									NULL);
}

static int confirm_reply_complete(u16 index, bdaddr_t *bdaddr, u8 status,
								u8 opcode)
{
	struct pending_cmd *cmd;
	struct mgmt_rp_user_confirm_reply rp;
	int err;

	cmd = mgmt_pending_find(opcode, index);
	if (!cmd)
		return -ENOENT;

	bacpy(&rp.bdaddr, bdaddr);
	rp.status = status;
	err = cmd_complete(cmd->sk, index, opcode, &rp, sizeof(rp));

	mgmt_pending_remove(cmd);

	return err;
}

int mgmt_user_confirm_reply_complete(u16 index, bdaddr_t *bdaddr, u8 status)
{
	return confirm_reply_complete(index, bdaddr, status,
						MGMT_OP_USER_CONFIRM_REPLY);
}

int mgmt_user_confirm_neg_reply_complete(u16 index, bdaddr_t *bdaddr, u8 status)
{
	return confirm_reply_complete(index, bdaddr, status,
					MGMT_OP_USER_CONFIRM_NEG_REPLY);
}

int mgmt_auth_failed(u16 index, bdaddr_t *bdaddr, u8 status)
{
	struct mgmt_ev_auth_failed ev;

	bacpy(&ev.bdaddr, bdaddr);
	ev.status = status;

	return mgmt_event(MGMT_EV_AUTH_FAILED, index, &ev, sizeof(ev), NULL);
}

int mgmt_set_local_name_complete(u16 index, u8 *name, u8 status)
{
	struct pending_cmd *cmd;
	struct hci_dev *hdev;
	struct mgmt_cp_set_local_name ev;
	int err;

	memset(&ev, 0, sizeof(ev));
	memcpy(ev.name, name, HCI_MAX_NAME_LENGTH);

	cmd = mgmt_pending_find(MGMT_OP_SET_LOCAL_NAME, index);
	if (!cmd)
		goto send_event;

	if (status) {
		err = cmd_status(cmd->sk, index, MGMT_OP_SET_LOCAL_NAME, EIO);
		goto failed;
	}

	hdev = hci_dev_get(index);
	if (hdev) {
		update_eir(hdev);
		hci_dev_put(hdev);
	}

	err = cmd_complete(cmd->sk, index, MGMT_OP_SET_LOCAL_NAME, &ev,
								sizeof(ev));
	if (err < 0)
		goto failed;

send_event:
	err = mgmt_event(MGMT_EV_LOCAL_NAME_CHANGED, index, &ev, sizeof(ev),
							cmd ? cmd->sk : NULL);

failed:
	if (cmd)
		mgmt_pending_remove(cmd);
	return err;
}

int mgmt_read_local_oob_data_reply_complete(u16 index, u8 *hash, u8 *randomizer,
								u8 status)
{
	struct pending_cmd *cmd;
	int err;

	BT_DBG("hci%u status %u", index, status);

	cmd = mgmt_pending_find(MGMT_OP_READ_LOCAL_OOB_DATA, index);
	if (!cmd)
		return -ENOENT;

	if (status) {
		err = cmd_status(cmd->sk, index, MGMT_OP_READ_LOCAL_OOB_DATA,
									EIO);
	} else {
		struct mgmt_rp_read_local_oob_data rp;

		memcpy(rp.hash, hash, sizeof(rp.hash));
		memcpy(rp.randomizer, randomizer, sizeof(rp.randomizer));

		err = cmd_complete(cmd->sk, index, MGMT_OP_READ_LOCAL_OOB_DATA,
							&rp, sizeof(rp));
	}

	mgmt_pending_remove(cmd);

	return err;
}

void mgmt_read_rssi_complete(u16 index, s8 rssi, bdaddr_t *bdaddr,
		u16 handle, u8 status)
{
	struct mgmt_ev_rssi_update ev;
	struct hci_conn *conn;
	struct hci_dev *hdev;

	if (status)
		return;

	hdev = hci_dev_get(index);
	conn = hci_conn_hash_lookup_handle(hdev, handle);

	if (!conn)
		return;

	BT_DBG("rssi_update_thresh_exceed : %d ",
		   conn->rssi_update_thresh_exceed);
	BT_DBG("RSSI Threshold : %d , recvd RSSI : %d ",
			conn->rssi_threshold, rssi);

	if (conn->rssi_update_thresh_exceed == 1) {
		BT_DBG("rssi_update_thresh_exceed == 1");
		if (rssi >= conn->rssi_threshold) {
			memset(&ev, 0, sizeof(ev));
			bacpy(&ev.bdaddr, bdaddr);
			ev.rssi = rssi;
			mgmt_event(MGMT_EV_RSSI_UPDATE, index, &ev,
				sizeof(ev), NULL);
		} else {
			hci_conn_set_rssi_reporter(conn, conn->rssi_threshold,
				conn->rssi_update_interval,
				conn->rssi_update_thresh_exceed);
		}
	} else {
		BT_DBG("rssi_update_thresh_exceed == 0");
		if (rssi <= conn->rssi_threshold) {
			memset(&ev, 0, sizeof(ev));
			bacpy(&ev.bdaddr, bdaddr);
			ev.rssi = rssi;
			mgmt_event(MGMT_EV_RSSI_UPDATE, index, &ev,
				sizeof(ev), NULL);
		} else {
			hci_conn_set_rssi_reporter(conn, conn->rssi_threshold,
				conn->rssi_update_interval,
				conn->rssi_update_thresh_exceed);
		}
	}
}


int mgmt_device_found(u16 index, bdaddr_t *bdaddr, u8 type, u8 le,
			u8 *dev_class, s8 rssi, u8 eir_len, u8 *eir)
{
	struct mgmt_ev_device_found ev;
	struct hci_dev *hdev;
	int err;

	BT_DBG("le: %d", le);

	memset(&ev, 0, sizeof(ev));

	bacpy(&ev.bdaddr, bdaddr);
	ev.rssi = rssi;
	ev.type = type;
	ev.le = le;

	if (dev_class)
		memcpy(ev.dev_class, dev_class, sizeof(ev.dev_class));

	if (eir && eir_len)
		memcpy(ev.eir, eir, eir_len);

	err = mgmt_event(MGMT_EV_DEVICE_FOUND, index, &ev, sizeof(ev), NULL);

	if (err < 0)
		return err;

	hdev = hci_dev_get(index);

	if (!hdev)
		return 0;

	if (hdev->disco_state == SCAN_IDLE)
		goto done;

	hdev->disco_int_count++;

	if (hdev->disco_int_count >= hdev->disco_int_phase) {
		/* Inquiry scan for General Discovery LAP */
		struct hci_cp_inquiry cp = {{0x33, 0x8b, 0x9e}, 4, 0};
		struct hci_cp_le_set_scan_enable le_cp = {0, 0};

		hdev->disco_int_phase *= 2;
		hdev->disco_int_count = 0;
		if (hdev->disco_state == SCAN_LE) {
			/* cancel LE scan */
			hci_send_cmd(hdev, HCI_OP_LE_SET_SCAN_ENABLE,
					sizeof(le_cp), &le_cp);
			/* start BR scan */
			cp.num_rsp = (u8) hdev->disco_int_phase;
			hci_send_cmd(hdev, HCI_OP_INQUIRY,
					sizeof(cp), &cp);
			hdev->disco_state = SCAN_BR;
			del_timer_sync(&hdev->disco_le_timer);
		}
	}

done:
	hci_dev_put(hdev);
	return 0;
}


int mgmt_remote_name(u16 index, bdaddr_t *bdaddr, u8 status, u8 *name)
{
	struct mgmt_ev_remote_name ev;

	memset(&ev, 0, sizeof(ev));

	bacpy(&ev.bdaddr, bdaddr);
	ev.status = status;
	memcpy(ev.name, name, HCI_MAX_NAME_LENGTH);

	return mgmt_event(MGMT_EV_REMOTE_NAME, index, &ev, sizeof(ev), NULL);
}

int mgmt_encrypt_change(u16 index, bdaddr_t *bdaddr, u8 status)
{
	struct mgmt_ev_encrypt_change ev;

	BT_DBG("hci%u", index);

	bacpy(&ev.bdaddr, bdaddr);
	ev.status = status;

	return mgmt_event(MGMT_EV_ENCRYPT_CHANGE, index, &ev, sizeof(ev),
									NULL);
}

int mgmt_remote_class(u16 index, bdaddr_t *bdaddr, u8 dev_class[3])
{
	struct mgmt_ev_remote_class ev;

	memset(&ev, 0, sizeof(ev));

	bacpy(&ev.bdaddr, bdaddr);
	memcpy(ev.dev_class, dev_class, 3);

	return mgmt_event(MGMT_EV_REMOTE_CLASS, index, &ev, sizeof(ev), NULL);
}

int mgmt_remote_version(u16 index, bdaddr_t *bdaddr, u8 ver, u16 mnf,
							u16 sub_ver)
{
	struct mgmt_ev_remote_version ev;

	memset(&ev, 0, sizeof(ev));

	bacpy(&ev.bdaddr, bdaddr);
	ev.lmp_ver = ver;
	ev.manufacturer = mnf;
	ev.lmp_subver = sub_ver;

	return mgmt_event(MGMT_EV_REMOTE_VERSION, index, &ev, sizeof(ev), NULL);
}

int mgmt_remote_features(u16 index, bdaddr_t *bdaddr, u8 features[8])
{
	struct mgmt_ev_remote_features ev;

	memset(&ev, 0, sizeof(ev));

	bacpy(&ev.bdaddr, bdaddr);
	memcpy(ev.features, features, sizeof(ev.features));

	return mgmt_event(MGMT_EV_REMOTE_FEATURES, index, &ev, sizeof(ev),
									NULL);
}
