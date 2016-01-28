/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/usb/audio.h>
#include <linux/uaccess.h>
#include <sound/pcm.h>
#include <sound/core.h>
#include <sound/asound.h>
#include <linux/usb.h>
#include <linux/qmi_encdec.h>
#include <soc/qcom/msm_qmi_interface.h>

#include "usbaudio.h"
#include "card.h"
#include "helper.h"
#include "pcm.h"
#include "usb_audio_qmi_v01.h"

#define SND_PCM_CARD_NUM_MASK 0xffff0000
#define SND_PCM_DEV_NUM_MASK 0xff00
#define SND_PCM_STREAM_DIRECTION 0xff

struct uaudio_dev {
	atomic_t in_use;
	struct kref kref;
	wait_queue_head_t disconnect_wq;
};

static struct uaudio_dev uadev[SNDRV_CARDS];

struct uaudio_qmi_svc {
	struct qmi_handle *uaudio_svc_hdl;
	void *curr_conn;
	struct work_struct recv_msg_work;
	struct workqueue_struct *uaudio_wq;
	ktime_t t_request_recvd;
	ktime_t t_resp_sent;
};

static struct uaudio_qmi_svc *uaudio_svc;

static struct msg_desc uaudio_stream_req_desc = {
	.max_msg_len = QMI_UAUDIO_STREAM_REQ_MSG_V01_MAX_MSG_LEN,
	.msg_id = QMI_UAUDIO_STREAM_REQ_V01,
	.ei_array = qmi_uaudio_stream_req_msg_v01_ei,
};

static struct msg_desc uaudio_stream_resp_desc = {
	.max_msg_len = QMI_UAUDIO_STREAM_RESP_MSG_V01_MAX_MSG_LEN,
	.msg_id = QMI_UAUDIO_STREAM_RESP_V01,
	.ei_array = qmi_uaudio_stream_resp_msg_v01_ei,
};

static int prepare_qmi_response(struct snd_usb_substream *subs,
		struct qmi_uaudio_stream_resp_msg_v01 *resp, int card_num)
{
	struct usb_interface *iface;
	struct usb_host_interface *alts;
	struct usb_interface_descriptor *altsd;
	struct usb_host_endpoint *ep;
	struct uac_format_type_i_continuous_descriptor *fmt;
	struct uac_format_type_i_discrete_descriptor *fmt_v1;
	struct uac_format_type_i_ext_descriptor *fmt_v2;
	struct uac1_as_header_descriptor *as;
	struct uac1_ac_header_descriptor *ac;
	int protocol;

	iface = usb_ifnum_to_if(subs->dev, subs->interface);
	if (!iface) {
		pr_err("%s: interface # %d does not exist\n", __func__,
			subs->interface);
		goto err;
	}

	alts = &iface->altsetting[subs->altset_idx];
	altsd = get_iface_desc(alts);
	protocol = altsd->bInterfaceProtocol;

	/* get format type */
	fmt = snd_usb_find_csint_desc(alts->extra, alts->extralen, NULL,
			UAC_FORMAT_TYPE);
	if (!fmt) {
		pr_err("%s: %u:%d : no UAC_FORMAT_TYPE desc\n", __func__,
			subs->interface, subs->altset_idx);
		goto err;
	}

	if (protocol == UAC_VERSION_1) {
		as = snd_usb_find_csint_desc(alts->extra, alts->extralen, NULL,
			UAC_AS_GENERAL);
		if (!as) {
			pr_err("%s: %u:%d : no UAC_AS_GENERAL desc\n", __func__,
				subs->interface, subs->altset_idx);
			goto err;
		}
		resp->bDelay = as->bDelay;
		fmt_v1 = (struct uac_format_type_i_discrete_descriptor *)fmt;
		resp->bSubslotSize = fmt_v1->bSubframeSize;
	} else if (protocol == UAC_VERSION_2) {
		fmt_v2 = (struct uac_format_type_i_ext_descriptor *)fmt;
		resp->bSubslotSize = fmt_v2->bSubslotSize;
	} else {
		pr_err("%s: unknown protocol version %x\n", __func__, protocol);
		goto err;
	}

	ac = snd_usb_find_csint_desc(alts->extra,
						 alts->extralen,
						 NULL, UAC_HEADER);
	if (!ac) {
		pr_err("%s: %u:%d : no UAC_HEADER desc\n", __func__,
			subs->interface, subs->altset_idx);
		goto err;
	}
	resp->bcdADC = ac->bcdADC;

	resp->slot_id = subs->dev->slot_id;

	memcpy(&resp->std_as_opr_intf_desc, &alts->desc, sizeof(alts->desc));

	ep = usb_pipe_endpoint(subs->dev, subs->data_endpoint->pipe);
	if (!ep) {
		pr_err("%s: data ep # %d context is null\n", __func__,
			subs->data_endpoint->ep_num);
		goto err;
	}
	memcpy(&resp->std_as_data_ep_desc, &ep->desc, sizeof(ep->desc));

	if (subs->sync_endpoint) {
		ep = usb_pipe_endpoint(subs->dev, subs->sync_endpoint->pipe);
		if (!ep) {
			pr_err("%s: sync ep # %d context is null\n", __func__,
				subs->sync_endpoint->ep_num);
			goto err;
		}
		memcpy(&resp->std_as_sync_ep_desc, &ep->desc, sizeof(ep->desc));
	}

	if (!atomic_read(&uadev[card_num].in_use)) {
		kref_init(&uadev[card_num].kref);
		init_waitqueue_head(&uadev[card_num].disconnect_wq);
		atomic_set(&uadev[card_num].in_use, 1);
	} else {
		kref_get(&uadev[card_num].kref);
	}

	return 0;
err:
	return -ENODEV;
}

void uaudio_disconnect_cb(struct snd_usb_audio *chip)
{
	int ret;
	struct uaudio_dev *dev;
	int card_num = chip->card_num;

	mutex_lock(&chip->dev_lock);
	if (card_num >=  SNDRV_CARDS) {
		pr_err("%s: invalid card number\n", __func__);
		goto done;
	}

	dev = &uadev[card_num];
	if (atomic_read(&dev->in_use)) {
		ret = wait_event_interruptible(dev->disconnect_wq,
				!atomic_read(&dev->in_use));
		if (ret < 0) {
			pr_debug("%s: failed with ret %d\n", __func__, ret);
			goto done;
		}
	}
done:
	mutex_unlock(&chip->dev_lock);
}

static void uaudio_dev_release(struct kref *kref)
{
	struct uaudio_dev *dev = container_of(kref, struct uaudio_dev, kref);

	atomic_set(&dev->in_use, 0);
	wake_up(&dev->disconnect_wq);
}

static int handle_uaudio_stream_req(void *req_h, void *req)
{
	struct qmi_uaudio_stream_req_msg_v01 *req_msg;
	struct qmi_uaudio_stream_resp_msg_v01 resp = {0};
	struct snd_usb_substream *subs;
	struct snd_usb_audio *chip = NULL;
	struct uaudio_qmi_svc *svc = uaudio_svc;
	u8 pcm_card_num, pcm_dev_num, direction;
	int ret = 0;

	req_msg = (struct qmi_uaudio_stream_req_msg_v01 *)req;

	direction = req_msg->usb_token & SND_PCM_STREAM_DIRECTION;
	pcm_dev_num = (req_msg->usb_token & SND_PCM_DEV_NUM_MASK) >> 8;
	pcm_card_num = (req_msg->usb_token & SND_PCM_CARD_NUM_MASK) >> 16;

	pr_debug("%s:card#:%d dev#:%d dir:%d en:%d fmt:%d rate:%d #ch:%d\n",
		__func__, pcm_card_num, pcm_dev_num, direction, req_msg->enable,
		req_msg->audio_format, req_msg->bit_rate,
		req_msg->number_of_ch);

	if (pcm_card_num >= SNDRV_CARDS) {
		pr_err("%s: invalid card # %u", __func__, pcm_card_num);
		ret = -EINVAL;
		goto response;
	}

	subs = find_snd_usb_substream(pcm_card_num, pcm_dev_num, direction,
					&chip, uaudio_disconnect_cb);
	if (!subs || !chip || atomic_read(&chip->shutdown)) {
		pr_err("%s: can't find substream for card# %u, dev# %u dir%u\n",
			__func__, pcm_card_num, pcm_dev_num, direction);
		ret = -ENODEV;
		goto response;
	}

	mutex_lock(&chip->dev_lock);
	if (atomic_read(&chip->shutdown) || !subs->stream || !subs->stream->pcm
			|| !subs->stream->chip) {
		ret = -ENODEV;
		mutex_unlock(&chip->dev_lock);
		goto response;
	}

	subs->pcm_format = req_msg->audio_format;
	subs->channels = req_msg->number_of_ch;
	subs->cur_rate = req_msg->bit_rate;

	ret = snd_usb_enable_audio_stream(subs, req_msg->enable);

	if (!ret && req_msg->enable)
		ret = prepare_qmi_response(subs, &resp, pcm_card_num);

	mutex_unlock(&chip->dev_lock);

response:
	if (!req_msg->enable && ret != -EINVAL) {
		if (atomic_read(&uadev[pcm_card_num].in_use))
			kref_put(&uadev[pcm_card_num].kref,
					uaudio_dev_release);
	}

	resp.status = ret;
	ret = qmi_send_resp_from_cb(svc->uaudio_svc_hdl, svc->curr_conn, req_h,
			&uaudio_stream_resp_desc, &resp, sizeof(resp));

	svc->t_resp_sent = ktime_get();

	pr_debug("%s: t_resp sent - t_req recvd (in ms) %lld\n", __func__,
		ktime_to_ms(ktime_sub(svc->t_resp_sent, svc->t_request_recvd)));

	return ret;
}

static int uaudio_qmi_svc_connect_cb(struct qmi_handle *handle,
			       void *conn_h)
{
	struct uaudio_qmi_svc *svc = uaudio_svc;

	if (svc->uaudio_svc_hdl != handle || !conn_h) {
		pr_err("%s: handle mismatch\n", __func__);
		return -EINVAL;
	}
	if (svc->curr_conn) {
		pr_err("%s: Service is busy\n", __func__);
		return -ECONNREFUSED;
	}
	svc->curr_conn = conn_h;
	return 0;
}

static int uaudio_qmi_svc_disconnect_cb(struct qmi_handle *handle,
				  void *conn_h)
{
	struct uaudio_qmi_svc *svc = uaudio_svc;

	if (svc->uaudio_svc_hdl != handle || svc->curr_conn != conn_h) {
		pr_err("%s: handle mismatch\n", __func__);
		return -EINVAL;
	}

	svc->curr_conn = NULL;
	return 0;
}

static int uaudio_qmi_svc_req_cb(struct qmi_handle *handle, void *conn_h,
			void *req_h, unsigned int msg_id, void *req)
{
	int ret;
	struct uaudio_qmi_svc *svc = uaudio_svc;

	if (svc->uaudio_svc_hdl != handle || svc->curr_conn != conn_h) {
		pr_err("%s: handle mismatch\n", __func__);
		return -EINVAL;
	}

	switch (msg_id) {
	case QMI_UAUDIO_STREAM_REQ_V01:
		ret = handle_uaudio_stream_req(req_h, req);
		break;

	default:
		ret = -ENOTSUPP;
		break;
	}
	return ret;
}

static int uaudio_qmi_svc_req_desc_cb(unsigned int msg_id,
	struct msg_desc **req_desc)
{
	int ret;

	pr_debug("%s: msg_id %d\n", __func__, msg_id);

	switch (msg_id) {
	case QMI_UAUDIO_STREAM_REQ_V01:
		*req_desc = &uaudio_stream_req_desc;
		ret = sizeof(struct qmi_uaudio_stream_req_msg_v01);
		break;

	default:
		ret = -ENOTSUPP;
		break;
	}
	return ret;
}

static void uaudio_qmi_svc_recv_msg(struct work_struct *w)
{
	int ret;
	struct uaudio_qmi_svc *svc = container_of(w, struct uaudio_qmi_svc,
		recv_msg_work);

	do {
		pr_debug("%s: Notified about a Receive Event", __func__);
	} while ((ret = qmi_recv_msg(svc->uaudio_svc_hdl)) == 0);

	if (ret != -ENOMSG)
		pr_err("%s: Error receiving message\n", __func__);
}

static void uaudio_qmi_svc_ntfy(struct qmi_handle *handle,
		enum qmi_event_type event, void *priv)
{
	struct uaudio_qmi_svc *svc = uaudio_svc;

	pr_debug("%s: event %d", __func__, event);

	svc->t_request_recvd = ktime_get();

	switch (event) {
	case QMI_RECV_MSG:
		queue_work(svc->uaudio_wq, &svc->recv_msg_work);
		break;
	default:
		break;
	}
}

static struct qmi_svc_ops_options uaudio_svc_ops_options = {
	.version = 1,
	.service_id = UAUDIO_STREAM_SERVICE_ID_V01,
	.service_vers = UAUDIO_STREAM_SERVICE_VERS_V01,
	.connect_cb = uaudio_qmi_svc_connect_cb,
	.disconnect_cb = uaudio_qmi_svc_disconnect_cb,
	.req_desc_cb = uaudio_qmi_svc_req_desc_cb,
	.req_cb = uaudio_qmi_svc_req_cb,
};

static int __init uaudio_qmi_svc_init(void)
{
	int ret;
	struct uaudio_qmi_svc *svc;

	svc = kzalloc(sizeof(struct uaudio_qmi_svc), GFP_KERNEL);
	if (!svc)
		return -ENOMEM;

	svc->uaudio_wq = create_singlethread_workqueue("uaudio_svc");
	if (!svc->uaudio_wq) {
		ret = -ENOMEM;
		goto free_svc;
	}

	svc->uaudio_svc_hdl = qmi_handle_create(uaudio_qmi_svc_ntfy, NULL);
	if (!svc->uaudio_svc_hdl) {
		pr_err("%s: Error creating svc_hdl\n", __func__);
		ret = -EFAULT;
		goto destroy_uaudio_wq;
	}

	ret = qmi_svc_register(svc->uaudio_svc_hdl, &uaudio_svc_ops_options);
	if (ret < 0) {
		pr_err("%s:Error registering uaudio svc %d\n", __func__, ret);
		goto destroy_svc_handle;
	}

	INIT_WORK(&svc->recv_msg_work, uaudio_qmi_svc_recv_msg);

	uaudio_svc = svc;

	return 0;

destroy_svc_handle:
	qmi_handle_destroy(svc->uaudio_svc_hdl);
destroy_uaudio_wq:
	destroy_workqueue(svc->uaudio_wq);
free_svc:
	kfree(svc);
	return ret;
}

static void __exit uaudio_qmi_svc_exit(void)
{
	struct uaudio_qmi_svc *svc = uaudio_svc;

	qmi_svc_unregister(svc->uaudio_svc_hdl);
	flush_workqueue(svc->uaudio_wq);
	qmi_handle_destroy(svc->uaudio_svc_hdl);
	destroy_workqueue(svc->uaudio_wq);
	kfree(svc);
	uaudio_svc = NULL;
}

module_init(uaudio_qmi_svc_init);
module_exit(uaudio_qmi_svc_exit);

MODULE_DESCRIPTION("USB AUDIO QMI Service Driver");
MODULE_LICENSE("GPL v2");
