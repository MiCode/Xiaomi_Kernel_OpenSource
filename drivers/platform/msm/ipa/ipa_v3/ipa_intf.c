/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include "ipa_i.h"
#include <linux/msm_ipa.h>

struct ipa3_intf {
	char name[IPA_RESOURCE_NAME_MAX];
	struct list_head link;
	u32 num_tx_props;
	u32 num_rx_props;
	u32 num_ext_props;
	struct ipa_ioc_tx_intf_prop *tx;
	struct ipa_ioc_rx_intf_prop *rx;
	struct ipa_ioc_ext_intf_prop *ext;
	enum ipa_client_type excp_pipe;
};

struct ipa3_push_msg {
	struct ipa_msg_meta meta;
	ipa_msg_free_fn callback;
	void *buff;
	struct list_head link;
};

struct ipa3_pull_msg {
	struct ipa_msg_meta meta;
	ipa_msg_pull_fn callback;
	struct list_head link;
};

/**
 * ipa3_register_intf() - register "logical" interface
 * @name: [in] interface name
 * @tx:	[in] TX properties of the interface
 * @rx:	[in] RX properties of the interface
 *
 * Register an interface and its tx and rx properties, this allows
 * configuration of rules from user-space
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_register_intf(const char *name, const struct ipa_tx_intf *tx,
		       const struct ipa_rx_intf *rx)
{
	return ipa3_register_intf_ext(name, tx, rx, NULL);
}

/**
 * ipa3_register_intf_ext() - register "logical" interface which has only
 * extended properties
 * @name: [in] interface name
 * @tx:	[in] TX properties of the interface
 * @rx:	[in] RX properties of the interface
 * @ext: [in] EXT properties of the interface
 *
 * Register an interface and its tx, rx and ext properties, this allows
 * configuration of rules from user-space
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_register_intf_ext(const char *name, const struct ipa_tx_intf *tx,
		       const struct ipa_rx_intf *rx,
		       const struct ipa_ext_intf *ext)
{
	struct ipa3_intf *intf;
	u32 len;

	if (name == NULL || (tx == NULL && rx == NULL && ext == NULL)) {
		IPAERR("invalid params name=%pK tx=%pK rx=%pK ext=%pK\n", name,
				tx, rx, ext);
		return -EINVAL;
	}

	if (tx && tx->num_props > IPA_NUM_PROPS_MAX) {
		IPAERR("invalid tx num_props=%d max=%d\n", tx->num_props,
				IPA_NUM_PROPS_MAX);
		return -EINVAL;
	}

	if (rx && rx->num_props > IPA_NUM_PROPS_MAX) {
		IPAERR("invalid rx num_props=%d max=%d\n", rx->num_props,
				IPA_NUM_PROPS_MAX);
		return -EINVAL;
	}

	if (ext && ext->num_props > IPA_NUM_PROPS_MAX) {
		IPAERR("invalid ext num_props=%d max=%d\n", ext->num_props,
				IPA_NUM_PROPS_MAX);
		return -EINVAL;
	}

	len = sizeof(struct ipa3_intf);
	intf = kzalloc(len, GFP_KERNEL);
	if (intf == NULL)
		return -ENOMEM;

	strlcpy(intf->name, name, IPA_RESOURCE_NAME_MAX);

	if (tx) {
		intf->num_tx_props = tx->num_props;
		len = tx->num_props * sizeof(struct ipa_ioc_tx_intf_prop);
		intf->tx = kzalloc(len, GFP_KERNEL);
		if (intf->tx == NULL) {
			kfree(intf);
			return -ENOMEM;
		}
		memcpy(intf->tx, tx->prop, len);
	}

	if (rx) {
		intf->num_rx_props = rx->num_props;
		len = rx->num_props * sizeof(struct ipa_ioc_rx_intf_prop);
		intf->rx = kzalloc(len, GFP_KERNEL);
		if (intf->rx == NULL) {
			kfree(intf->tx);
			kfree(intf);
			return -ENOMEM;
		}
		memcpy(intf->rx, rx->prop, len);
	}

	if (ext) {
		intf->num_ext_props = ext->num_props;
		len = ext->num_props * sizeof(struct ipa_ioc_ext_intf_prop);
		intf->ext = kzalloc(len, GFP_KERNEL);
		if (intf->ext == NULL) {
			kfree(intf->rx);
			kfree(intf->tx);
			kfree(intf);
			return -ENOMEM;
		}
		memcpy(intf->ext, ext->prop, len);
	}

	if (ext && ext->excp_pipe_valid)
		intf->excp_pipe = ext->excp_pipe;
	else
		intf->excp_pipe = IPA_CLIENT_APPS_LAN_CONS;

	mutex_lock(&ipa3_ctx->lock);
	list_add_tail(&intf->link, &ipa3_ctx->intf_list);
	mutex_unlock(&ipa3_ctx->lock);

	return 0;
}

/**
 * ipa3_deregister_intf() - de-register previously registered logical interface
 * @name: [in] interface name
 *
 * De-register a previously registered interface
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_deregister_intf(const char *name)
{
	struct ipa3_intf *entry;
	struct ipa3_intf *next;
	int result = -EINVAL;

	if ((name == NULL) ||
	    (strnlen(name, IPA_RESOURCE_NAME_MAX) == IPA_RESOURCE_NAME_MAX)) {
		IPAERR("invalid param name=%s\n", name);
		return result;
	}

	mutex_lock(&ipa3_ctx->lock);
	list_for_each_entry_safe(entry, next, &ipa3_ctx->intf_list, link) {
		if (!strcmp(entry->name, name)) {
			list_del(&entry->link);
			kfree(entry->ext);
			kfree(entry->rx);
			kfree(entry->tx);
			kfree(entry);
			result = 0;
			break;
		}
	}
	mutex_unlock(&ipa3_ctx->lock);

	return result;
}

/**
 * ipa3_query_intf() - query logical interface properties
 * @lookup:	[inout] interface name and number of properties
 *
 * Obtain the handle and number of tx and rx properties for the named
 * interface, used as part of querying the tx and rx properties for
 * configuration of various rules from user-space
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_query_intf(struct ipa_ioc_query_intf *lookup)
{
	struct ipa3_intf *entry;
	int result = -EINVAL;

	if (lookup == NULL) {
		IPAERR_RL("invalid param lookup=%pK\n", lookup);
		return result;
	}

	lookup->name[IPA_RESOURCE_NAME_MAX-1] = '\0';
	if (strnlen(lookup->name, IPA_RESOURCE_NAME_MAX) ==
			IPA_RESOURCE_NAME_MAX) {
		IPAERR_RL("Interface name too long. (%s)\n", lookup->name);
		return result;
	}

	mutex_lock(&ipa3_ctx->lock);
	list_for_each_entry(entry, &ipa3_ctx->intf_list, link) {
		if (!strcmp(entry->name, lookup->name)) {
			lookup->num_tx_props = entry->num_tx_props;
			lookup->num_rx_props = entry->num_rx_props;
			lookup->num_ext_props = entry->num_ext_props;
			lookup->excp_pipe = entry->excp_pipe;
			result = 0;
			break;
		}
	}
	mutex_unlock(&ipa3_ctx->lock);

	return result;
}

/**
 * ipa3_query_intf_tx_props() - qeury TX props of an interface
 * @tx:  [inout] interface tx attributes
 *
 * Obtain the tx properties for the specified interface
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_query_intf_tx_props(struct ipa_ioc_query_intf_tx_props *tx)
{
	struct ipa3_intf *entry;
	int result = -EINVAL;

	if (tx == NULL) {
		IPAERR("null args: tx\n");
		return result;
	}

	tx->name[IPA_RESOURCE_NAME_MAX-1] = '\0';
	if (strnlen(tx->name, IPA_RESOURCE_NAME_MAX) == IPA_RESOURCE_NAME_MAX) {
		IPAERR_RL("Interface name too long. (%s)\n", tx->name);
		return result;
	}

	mutex_lock(&ipa3_ctx->lock);
	list_for_each_entry(entry, &ipa3_ctx->intf_list, link) {
		if (!strcmp(entry->name, tx->name)) {
			/* add the entry check */
			if (entry->num_tx_props != tx->num_tx_props) {
				IPAERR("invalid entry number(%u %u)\n",
					entry->num_tx_props,
						tx->num_tx_props);
				mutex_unlock(&ipa3_ctx->lock);
				return result;
			}
			memcpy(tx->tx, entry->tx, entry->num_tx_props *
			       sizeof(struct ipa_ioc_tx_intf_prop));
			result = 0;
			break;
		}
	}
	mutex_unlock(&ipa3_ctx->lock);

	return result;
}

/**
 * ipa3_query_intf_rx_props() - qeury RX props of an interface
 * @rx:  [inout] interface rx attributes
 *
 * Obtain the rx properties for the specified interface
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_query_intf_rx_props(struct ipa_ioc_query_intf_rx_props *rx)
{
	struct ipa3_intf *entry;
	int result = -EINVAL;

	if (rx == NULL) {
		IPAERR("null args: rx\n");
		return result;
	}

	rx->name[IPA_RESOURCE_NAME_MAX-1] = '\0';
	if (strnlen(rx->name, IPA_RESOURCE_NAME_MAX) == IPA_RESOURCE_NAME_MAX) {
		IPAERR_RL("Interface name too long. (%s)\n", rx->name);
		return result;
	}

	mutex_lock(&ipa3_ctx->lock);
	list_for_each_entry(entry, &ipa3_ctx->intf_list, link) {
		if (!strcmp(entry->name, rx->name)) {
			/* add the entry check */
			if (entry->num_rx_props != rx->num_rx_props) {
				IPAERR("invalid entry number(%u %u)\n",
					entry->num_rx_props,
						rx->num_rx_props);
				mutex_unlock(&ipa3_ctx->lock);
				return result;
			}
			memcpy(rx->rx, entry->rx, entry->num_rx_props *
					sizeof(struct ipa_ioc_rx_intf_prop));
			result = 0;
			break;
		}
	}
	mutex_unlock(&ipa3_ctx->lock);

	return result;
}

/**
 * ipa3_query_intf_ext_props() - qeury EXT props of an interface
 * @ext:  [inout] interface ext attributes
 *
 * Obtain the ext properties for the specified interface
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_query_intf_ext_props(struct ipa_ioc_query_intf_ext_props *ext)
{
	struct ipa3_intf *entry;
	int result = -EINVAL;

	if (ext == NULL) {
		IPAERR("invalid param ext=%pK\n", ext);
		return result;
	}

	mutex_lock(&ipa3_ctx->lock);
	list_for_each_entry(entry, &ipa3_ctx->intf_list, link) {
		if (!strcmp(entry->name, ext->name)) {
			/* add the entry check */
			if (entry->num_ext_props != ext->num_ext_props) {
				IPAERR("invalid entry number(%u %u)\n",
					entry->num_ext_props,
						ext->num_ext_props);
				mutex_unlock(&ipa3_ctx->lock);
				return result;
			}
			memcpy(ext->ext, entry->ext, entry->num_ext_props *
					sizeof(struct ipa_ioc_ext_intf_prop));
			result = 0;
			break;
		}
	}
	mutex_unlock(&ipa3_ctx->lock);
	return result;
}

static void ipa3_send_msg_free(void *buff, u32 len, u32 type)
{
	kfree(buff);
}

static int wlan_msg_process(struct ipa_msg_meta *meta, void *buff)
{
	struct ipa3_push_msg *msg_dup;
	struct ipa_wlan_msg_ex *event_ex_cur_con = NULL;
	struct ipa_wlan_msg_ex *event_ex_list = NULL;
	struct ipa_wlan_msg *event_ex_cur_discon = NULL;
	void *data_dup = NULL;
	struct ipa3_push_msg *entry;
	struct ipa3_push_msg *next;
	int cnt = 0, total = 0, max = 0;
	uint8_t mac[IPA_MAC_ADDR_SIZE];
	uint8_t mac2[IPA_MAC_ADDR_SIZE];

	if (meta->msg_type == WLAN_CLIENT_CONNECT_EX) {
		/* debug print */
		event_ex_cur_con = buff;
		for (cnt = 0; cnt < event_ex_cur_con->num_of_attribs; cnt++) {
			if (event_ex_cur_con->attribs[cnt].attrib_type ==
				WLAN_HDR_ATTRIB_MAC_ADDR) {
				IPADBG("%02x:%02x:%02x:%02x:%02x:%02x,(%d)\n",
				event_ex_cur_con->attribs[cnt].u.mac_addr[0],
				event_ex_cur_con->attribs[cnt].u.mac_addr[1],
				event_ex_cur_con->attribs[cnt].u.mac_addr[2],
				event_ex_cur_con->attribs[cnt].u.mac_addr[3],
				event_ex_cur_con->attribs[cnt].u.mac_addr[4],
				event_ex_cur_con->attribs[cnt].u.mac_addr[5],
				meta->msg_type);
			}
		}

		mutex_lock(&ipa3_ctx->msg_wlan_client_lock);
		msg_dup = kzalloc(sizeof(*msg_dup), GFP_KERNEL);
		if (msg_dup == NULL) {
			mutex_unlock(&ipa3_ctx->msg_wlan_client_lock);
			return -ENOMEM;
		}
		msg_dup->meta = *meta;
		if (meta->msg_len > 0 && buff) {
			data_dup = kmalloc(meta->msg_len, GFP_KERNEL);
			if (data_dup == NULL) {
				kfree(msg_dup);
				mutex_unlock(&ipa3_ctx->msg_wlan_client_lock);
				return -ENOMEM;
			}
			memcpy(data_dup, buff, meta->msg_len);
			msg_dup->buff = data_dup;
			msg_dup->callback = ipa3_send_msg_free;
		} else {
			IPAERR("msg_len %d\n", meta->msg_len);
			kfree(msg_dup);
			mutex_unlock(&ipa3_ctx->msg_wlan_client_lock);
			return -ENOMEM;
		}
		list_add_tail(&msg_dup->link, &ipa3_ctx->msg_wlan_client_list);
		mutex_unlock(&ipa3_ctx->msg_wlan_client_lock);
	}

	/* remove the cache */
	if (meta->msg_type == WLAN_CLIENT_DISCONNECT) {
		/* debug print */
		event_ex_cur_discon = buff;
		IPADBG("Mac %pM, msg %d\n",
		event_ex_cur_discon->mac_addr,
		meta->msg_type);
		memcpy(mac2,
			event_ex_cur_discon->mac_addr,
			sizeof(mac2));

		mutex_lock(&ipa3_ctx->msg_wlan_client_lock);
		list_for_each_entry_safe(entry, next,
				&ipa3_ctx->msg_wlan_client_list,
				link) {
			event_ex_list = entry->buff;
			max = event_ex_list->num_of_attribs;
			for (cnt = 0; cnt < max; cnt++) {
				memcpy(mac,
					event_ex_list->attribs[cnt].u.mac_addr,
					sizeof(mac));
				if (event_ex_list->attribs[cnt].attrib_type ==
					WLAN_HDR_ATTRIB_MAC_ADDR) {
					pr_debug("%pM\n", mac);

					/* compare to delete one*/
					if (memcmp(mac2, mac,
						sizeof(mac)) == 0) {
						IPADBG("clean %d\n", total);
						list_del(&entry->link);
						kfree(entry);
						break;
					}
				}
			}
			total++;
		}
		mutex_unlock(&ipa3_ctx->msg_wlan_client_lock);
	}
	return 0;
}

/**
 * ipa3_send_msg() - Send "message" from kernel client to IPA driver
 * @meta: [in] message meta-data
 * @buff: [in] the payload for message
 * @callback: [in] free callback
 *
 * Client supplies the message meta-data and payload which IPA driver buffers
 * till read by user-space. After read from user space IPA driver invokes the
 * callback supplied to free the message payload. Client must not touch/free
 * the message payload after calling this API.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_send_msg(struct ipa_msg_meta *meta, void *buff,
		  ipa_msg_free_fn callback)
{
	struct ipa3_push_msg *msg;
	void *data = NULL;

	if (meta == NULL || (buff == NULL && callback != NULL) ||
	    (buff != NULL && callback == NULL)) {
		IPAERR_RL("invalid param meta=%pK buff=%pK, callback=%pK\n",
		       meta, buff, callback);
		return -EINVAL;
	}

	if (meta->msg_type >= IPA_EVENT_MAX_NUM) {
		IPAERR_RL("unsupported message type %d\n", meta->msg_type);
		return -EINVAL;
	}

	msg = kzalloc(sizeof(struct ipa3_push_msg), GFP_KERNEL);
	if (msg == NULL)
		return -ENOMEM;

	msg->meta = *meta;
	if (meta->msg_len > 0 && buff) {
		data = kmalloc(meta->msg_len, GFP_KERNEL);
		if (data == NULL) {
			kfree(msg);
			return -ENOMEM;
		}
		memcpy(data, buff, meta->msg_len);
		msg->buff = data;
		msg->callback = ipa3_send_msg_free;
	}

	mutex_lock(&ipa3_ctx->msg_lock);
	list_add_tail(&msg->link, &ipa3_ctx->msg_list);
	/* support for softap client event cache */
	if (wlan_msg_process(meta, buff))
		IPAERR("wlan_msg_process failed\n");

	/* unlock only after process */
	mutex_unlock(&ipa3_ctx->msg_lock);
	IPA_STATS_INC_CNT(ipa3_ctx->stats.msg_w[meta->msg_type]);

	wake_up(&ipa3_ctx->msg_waitq);
	if (buff)
		callback(buff, meta->msg_len, meta->msg_type);

	return 0;
}

/**
 * ipa3_resend_wlan_msg() - Resend cached "message" to IPACM
 *
 * resend wlan client connect events to user-space
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_resend_wlan_msg(void)
{
	struct ipa_wlan_msg_ex *event_ex_list = NULL;
	struct ipa3_push_msg *entry;
	struct ipa3_push_msg *next;
	int cnt = 0, total = 0;
	struct ipa3_push_msg *msg;
	void *data = NULL;

	IPADBG("\n");

	mutex_lock(&ipa3_ctx->msg_wlan_client_lock);
	list_for_each_entry_safe(entry, next, &ipa3_ctx->msg_wlan_client_list,
			link) {

		event_ex_list = entry->buff;
		for (cnt = 0; cnt < event_ex_list->num_of_attribs; cnt++) {
			if (event_ex_list->attribs[cnt].attrib_type ==
				WLAN_HDR_ATTRIB_MAC_ADDR) {
				IPADBG("%d-Mac %pM\n", total,
				event_ex_list->attribs[cnt].u.mac_addr);
			}
		}

		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		if (msg == NULL) {
			mutex_unlock(&ipa3_ctx->msg_wlan_client_lock);
			return -ENOMEM;
		}
		msg->meta = entry->meta;
		data = kmalloc(entry->meta.msg_len, GFP_KERNEL);
		if (data == NULL) {
			kfree(msg);
			mutex_unlock(&ipa3_ctx->msg_wlan_client_lock);
			return -ENOMEM;
		}
		memcpy(data, entry->buff, entry->meta.msg_len);
		msg->buff = data;
		msg->callback = ipa3_send_msg_free;
		mutex_lock(&ipa3_ctx->msg_lock);
		list_add_tail(&msg->link, &ipa3_ctx->msg_list);
		mutex_unlock(&ipa3_ctx->msg_lock);
		wake_up(&ipa3_ctx->msg_waitq);

		total++;
	}
	mutex_unlock(&ipa3_ctx->msg_wlan_client_lock);
	return 0;
}

/**
 * ipa3_register_pull_msg() - register pull message type
 * @meta: [in] message meta-data
 * @callback: [in] pull callback
 *
 * Register message callback by kernel client with IPA driver for IPA driver to
 * pull message on-demand.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_register_pull_msg(struct ipa_msg_meta *meta, ipa_msg_pull_fn callback)
{
	struct ipa3_pull_msg *msg;

	if (meta == NULL || callback == NULL) {
		IPAERR("invalid param meta=%pK callback=%pK\n", meta, callback);
		return -EINVAL;
	}

	msg = kzalloc(sizeof(struct ipa3_pull_msg), GFP_KERNEL);
	if (msg == NULL)
		return -ENOMEM;

	msg->meta = *meta;
	msg->callback = callback;

	mutex_lock(&ipa3_ctx->msg_lock);
	list_add_tail(&msg->link, &ipa3_ctx->pull_msg_list);
	mutex_unlock(&ipa3_ctx->msg_lock);

	return 0;
}

/**
 * ipa3_deregister_pull_msg() - De-register pull message type
 * @meta: [in] message meta-data
 *
 * De-register "message" by kernel client from IPA driver
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_deregister_pull_msg(struct ipa_msg_meta *meta)
{
	struct ipa3_pull_msg *entry;
	struct ipa3_pull_msg *next;
	int result = -EINVAL;

	if (meta == NULL) {
		IPAERR("null arg: meta\n");
		return result;
	}

	mutex_lock(&ipa3_ctx->msg_lock);
	list_for_each_entry_safe(entry, next, &ipa3_ctx->pull_msg_list, link) {
		if (entry->meta.msg_len == meta->msg_len &&
		    entry->meta.msg_type == meta->msg_type) {
			list_del(&entry->link);
			kfree(entry);
			result = 0;
			break;
		}
	}
	mutex_unlock(&ipa3_ctx->msg_lock);
	return result;
}

/**
 * ipa3_read() - read message from IPA device
 * @filp:	[in] file pointer
 * @buf:	[out] buffer to read into
 * @count:	[in] size of above buffer
 * @f_pos:	[inout] file position
 *
 * Uer-space should continually read from /dev/ipa, read wll block when there
 * are no messages to read. Upon return, user-space should read the ipa_msg_meta
 * from the start of the buffer to know what type of message was read and its
 * length in the remainder of the buffer. Buffer supplied must be big enough to
 * hold the message meta-data and the largest defined message type
 *
 * Returns:	how many bytes copied to buffer
 *
 * Note:	Should not be called from atomic context
 */
ssize_t ipa3_read(struct file *filp, char __user *buf, size_t count,
		  loff_t *f_pos)
{
	char __user *start;
	struct ipa3_push_msg *msg = NULL;
	int ret;
	DEFINE_WAIT_FUNC(wait, woken_wake_function);
	int locked;

	start = buf;

	add_wait_queue(&ipa3_ctx->msg_waitq, &wait);
	while (1) {
		mutex_lock(&ipa3_ctx->msg_lock);
		locked = 1;

		if (!list_empty(&ipa3_ctx->msg_list)) {
			msg = list_first_entry(&ipa3_ctx->msg_list,
					struct ipa3_push_msg, link);
			list_del(&msg->link);
		}

		IPADBG_LOW("msg=%pK\n", msg);

		if (msg) {
			locked = 0;
			mutex_unlock(&ipa3_ctx->msg_lock);
			if (copy_to_user(buf, &msg->meta,
					  sizeof(struct ipa_msg_meta))) {
				ret = -EFAULT;
				kfree(msg);
				msg = NULL;
				break;
			}
			buf += sizeof(struct ipa_msg_meta);
			count -= sizeof(struct ipa_msg_meta);
			if (msg->buff) {
				if (copy_to_user(buf, msg->buff,
						  msg->meta.msg_len)) {
					ret = -EFAULT;
					kfree(msg);
					msg = NULL;
					break;
				}
				buf += msg->meta.msg_len;
				count -= msg->meta.msg_len;
				msg->callback(msg->buff, msg->meta.msg_len,
					       msg->meta.msg_type);
			}
			IPA_STATS_INC_CNT(
				ipa3_ctx->stats.msg_r[msg->meta.msg_type]);
			kfree(msg);
		}

		ret = -EAGAIN;
		if (filp->f_flags & O_NONBLOCK)
			break;

		ret = -EINTR;
		if (signal_pending(current))
			break;

		if (start != buf)
			break;

		locked = 0;
		mutex_unlock(&ipa3_ctx->msg_lock);
		wait_woken(&wait, TASK_INTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
	}

	remove_wait_queue(&ipa3_ctx->msg_waitq, &wait);
	if (start != buf && ret != -EFAULT)
		ret = buf - start;

	if (locked)
		mutex_unlock(&ipa3_ctx->msg_lock);

	return ret;
}

/**
 * ipa3_pull_msg() - pull the specified message from client
 * @meta: [in] message meta-data
 * @buf:  [out] buffer to read into
 * @count: [in] size of above buffer
 *
 * Populate the supplied buffer with the pull message which is fetched
 * from client, the message must have previously been registered with
 * the IPA driver
 *
 * Returns:	how many bytes copied to buffer
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_pull_msg(struct ipa_msg_meta *meta, char *buff, size_t count)
{
	struct ipa3_pull_msg *entry;
	int result = -EINVAL;

	if (meta == NULL || buff == NULL || !count) {
		IPAERR_RL("invalid param name=%pK buff=%pK count=%zu\n",
				meta, buff, count);
		return result;
	}

	mutex_lock(&ipa3_ctx->msg_lock);
	list_for_each_entry(entry, &ipa3_ctx->pull_msg_list, link) {
		if (entry->meta.msg_len == meta->msg_len &&
		    entry->meta.msg_type == meta->msg_type) {
			result = entry->callback(buff, count, meta->msg_type);
			break;
		}
	}
	mutex_unlock(&ipa3_ctx->msg_lock);
	return result;
}
