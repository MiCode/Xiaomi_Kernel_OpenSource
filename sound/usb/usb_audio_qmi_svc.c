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
#include <linux/usb/audio-v2.h>
#include <linux/uaccess.h>
#include <sound/pcm.h>
#include <sound/core.h>
#include <sound/asound.h>
#include <linux/usb.h>
#include <linux/qmi_encdec.h>
#include <linux/of.h>
#include <soc/qcom/msm_qmi_interface.h>
#include <linux/platform_device.h>

#include "usbaudio.h"
#include "card.h"
#include "helper.h"
#include "pcm.h"
#include "usb_audio_qmi_v01.h"

#define SND_PCM_CARD_NUM_MASK 0xffff0000
#define SND_PCM_DEV_NUM_MASK 0xff00
#define SND_PCM_STREAM_DIRECTION 0xff

#define MAX_XFER_BUFF_LEN (24 * PAGE_SIZE)

struct intf_info {
	size_t xfer_buf_size;
	phys_addr_t xfer_buf_pa;
	u8 *xfer_buf;
	u8 pcm_card_num;
	u8 pcm_dev_num;
	u8 direction;
	bool in_use;
};

struct uaudio_dev {
	struct usb_device *udev;
	/* audio control interface */
	struct usb_host_interface *ctrl_intf;
	unsigned int card_num;
	atomic_t in_use;
	struct kref kref;
	wait_queue_head_t disconnect_wq;

	/* interface specific */
	int num_intf;
	struct intf_info *info;
};

static struct uaudio_dev uadev[SNDRV_CARDS];

struct uaudio_qmi_dev {
	struct device *dev;
	u32 intr_num;

	/* bit fields representing pcm card enabled */
	unsigned long card_slot;
	/* cache event ring phys addr */
	u64 er_phys_addr;
};

static struct uaudio_qmi_dev *uaudio_qdev;

struct uaudio_qmi_svc {
	struct qmi_handle *uaudio_svc_hdl;
	void *curr_conn;
	struct work_struct recv_msg_work;
	struct work_struct qmi_disconnect_work;
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

static struct msg_desc uaudio_stream_ind_desc = {
	.max_msg_len = QMI_UAUDIO_STREAM_IND_MSG_V01_MAX_MSG_LEN,
	.msg_id = QMI_UADUIO_STREAM_IND_V01,
	.ei_array = qmi_uaudio_stream_ind_msg_v01_ei,
};

enum usb_qmi_audio_format {
	USB_QMI_PCM_FORMAT_S8 = 0,
	USB_QMI_PCM_FORMAT_U8,
	USB_QMI_PCM_FORMAT_S16_LE,
	USB_QMI_PCM_FORMAT_S16_BE,
	USB_QMI_PCM_FORMAT_U16_LE,
	USB_QMI_PCM_FORMAT_U16_BE,
	USB_QMI_PCM_FORMAT_S24_LE,
	USB_QMI_PCM_FORMAT_S24_BE,
	USB_QMI_PCM_FORMAT_U24_LE,
	USB_QMI_PCM_FORMAT_U24_BE,
	USB_QMI_PCM_FORMAT_S24_3LE,
	USB_QMI_PCM_FORMAT_S24_3BE,
	USB_QMI_PCM_FORMAT_U24_3LE,
	USB_QMI_PCM_FORMAT_U24_3BE,
	USB_QMI_PCM_FORMAT_S32_LE,
	USB_QMI_PCM_FORMAT_S32_BE,
	USB_QMI_PCM_FORMAT_U32_LE,
	USB_QMI_PCM_FORMAT_U32_BE,
};

static int prepare_qmi_response(struct snd_usb_substream *subs,
		struct qmi_uaudio_stream_resp_msg_v01 *resp, u32 xfer_buf_len,
		int card_num, int pcm_dev_num)
{
	int ret = -ENODEV;
	struct usb_interface *iface;
	struct usb_host_interface *alts;
	struct usb_interface_descriptor *altsd;
	struct usb_host_endpoint *ep;
	struct uac_format_type_i_continuous_descriptor *fmt;
	struct uac_format_type_i_discrete_descriptor *fmt_v1;
	struct uac_format_type_i_ext_descriptor *fmt_v2;
	struct uac1_as_header_descriptor *as;
	int protocol;
	void *hdr_ptr;
	u8 *xfer_buf;
	u32 len, mult, remainder;
	phys_addr_t xhci_pa, xfer_buf_pa;

	iface = usb_ifnum_to_if(subs->dev, subs->interface);
	if (!iface) {
		pr_err("%s: interface # %d does not exist\n", __func__,
			subs->interface);
		goto err;
	}

	if (uadev[card_num].info &&
			uadev[card_num].info[subs->interface].in_use) {
		pr_err("%s interface# %d already in use card# %d\n", __func__,
			subs->interface, card_num);
		ret = -EBUSY;
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

	if (!uadev[card_num].ctrl_intf) {
		pr_err("%s: audio ctrl intf info not cached\n", __func__);
		goto err;
	}

	hdr_ptr = snd_usb_find_csint_desc(uadev[card_num].ctrl_intf->extra,
					uadev[card_num].ctrl_intf->extralen,
					NULL, UAC_HEADER);
	if (!hdr_ptr) {
		pr_err("%s: no UAC_HEADER desc\n", __func__);
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
		resp->data_path_delay = as->bDelay;
		resp->data_path_delay_valid = 1;
		fmt_v1 = (struct uac_format_type_i_discrete_descriptor *)fmt;
		resp->usb_audio_subslot_size = fmt_v1->bSubframeSize;
		resp->usb_audio_subslot_size_valid = 1;

		resp->usb_audio_spec_revision =
			((struct uac1_ac_header_descriptor *)hdr_ptr)->bcdADC;
		resp->usb_audio_spec_revision_valid = 1;
	} else if (protocol == UAC_VERSION_2) {
		fmt_v2 = (struct uac_format_type_i_ext_descriptor *)fmt;
		resp->usb_audio_subslot_size = fmt_v2->bSubslotSize;
		resp->usb_audio_subslot_size_valid = 1;

		resp->usb_audio_spec_revision =
			((struct uac2_ac_header_descriptor *)hdr_ptr)->bcdADC;
		resp->usb_audio_spec_revision_valid = 1;
	} else {
		pr_err("%s: unknown protocol version %x\n", __func__, protocol);
		goto err;
	}

	resp->slot_id = subs->dev->slot_id;
	resp->slot_id_valid = 1;

	memcpy(&resp->std_as_opr_intf_desc, &alts->desc, sizeof(alts->desc));
	resp->std_as_opr_intf_desc_valid = 1;

	ep = usb_pipe_endpoint(subs->dev, subs->data_endpoint->pipe);
	if (!ep) {
		pr_err("%s: data ep # %d context is null\n", __func__,
			subs->data_endpoint->ep_num);
		goto err;
	}
	memcpy(&resp->std_as_data_ep_desc, &ep->desc, sizeof(ep->desc));
	resp->std_as_data_ep_desc_valid = 1;

	xhci_pa = usb_get_xfer_ring_dma_addr(subs->dev, ep);
	if (!xhci_pa) {
		pr_err("%s:failed to get data ep ring dma address\n", __func__);
		goto err;
	}

	resp->xhci_mem_info.tr_data.pa = xhci_pa;

	if (subs->sync_endpoint) {
		ep = usb_pipe_endpoint(subs->dev, subs->sync_endpoint->pipe);
		if (!ep) {
			pr_debug("%s: implicit fb on data ep\n", __func__);
			goto skip_sync_ep;
		}
		memcpy(&resp->std_as_sync_ep_desc, &ep->desc, sizeof(ep->desc));
		resp->std_as_sync_ep_desc_valid = 1;

		xhci_pa = usb_get_xfer_ring_dma_addr(subs->dev, ep);
		if (!xhci_pa) {
			pr_err("%s:failed to get sync ep ring dma address\n",
				__func__);
			goto err;
		}
		resp->xhci_mem_info.tr_sync.pa = xhci_pa;
	}

skip_sync_ep:
	resp->interrupter_num = uaudio_qdev->intr_num;
	resp->interrupter_num_valid = 1;

	/*  map xhci data structures PA memory to iova */

	/* event ring */
	ret = usb_sec_event_ring_setup(subs->dev, resp->interrupter_num);
	if (ret) {
		pr_err("%s: failed to setup sec event ring ret %d\n", __func__,
			ret);
		goto err;
	}
	xhci_pa = usb_get_sec_event_ring_dma_addr(subs->dev,
			resp->interrupter_num);
	if (!xhci_pa) {
		pr_err("%s: failed to get sec event ring dma address\n",
		__func__);
		goto err;
	}
	resp->xhci_mem_info.evt_ring.va = xhci_pa;
	resp->xhci_mem_info.evt_ring.pa = xhci_pa;
	resp->xhci_mem_info.evt_ring.size = PAGE_SIZE;
	uaudio_qdev->er_phys_addr = xhci_pa;

	/* dcba */
	xhci_pa = usb_get_dcba_dma_addr(subs->dev);
	if (!xhci_pa) {
		pr_err("%s:failed to get dcba dma address\n", __func__);
		goto err;
	}
	resp->xhci_mem_info.dcba.va = xhci_pa;
	resp->xhci_mem_info.dcba.pa = xhci_pa;
	resp->xhci_mem_info.dcba.size = PAGE_SIZE;

	/* data transfer ring */
	xhci_pa = resp->xhci_mem_info.tr_data.pa;
	resp->xhci_mem_info.tr_data.va = xhci_pa;
	resp->xhci_mem_info.tr_data.size = PAGE_SIZE;

	/* sync transfer ring */
	if (!resp->xhci_mem_info.tr_sync.pa)
		goto skip_sync;

	xhci_pa = resp->xhci_mem_info.tr_sync.pa;
	resp->xhci_mem_info.tr_sync.va = xhci_pa;
	resp->xhci_mem_info.tr_sync.size = PAGE_SIZE;

skip_sync:
	/* xfer buffer, multiple of 4K only */
	if (!xfer_buf_len)
		xfer_buf_len = PAGE_SIZE;

	mult = xfer_buf_len / PAGE_SIZE;
	remainder = xfer_buf_len % PAGE_SIZE;
	len = mult * PAGE_SIZE;
	len += remainder ? PAGE_SIZE : 0;

	if (len > MAX_XFER_BUFF_LEN) {
		pr_err("%s: req buf len %d > max buf len %lu, setting %lu\n",
		__func__, len, MAX_XFER_BUFF_LEN, MAX_XFER_BUFF_LEN);
		len = MAX_XFER_BUFF_LEN;
	}

	xfer_buf = usb_alloc_coherent(subs->dev, len, GFP_KERNEL, &xfer_buf_pa);
	if (!xfer_buf)
		goto free_xfer_buf;

	resp->xhci_mem_info.xfer_buff.pa = xfer_buf_pa;
	resp->xhci_mem_info.xfer_buff.size = len;
	resp->xhci_mem_info.xfer_buff.va = xfer_buf_pa;
	resp->xhci_mem_info_valid = 1;

	if (!atomic_read(&uadev[card_num].in_use)) {
		kref_init(&uadev[card_num].kref);
		init_waitqueue_head(&uadev[card_num].disconnect_wq);
		uadev[card_num].num_intf =
			subs->dev->config->desc.bNumInterfaces;
		uadev[card_num].info =
			kzalloc(sizeof(struct intf_info) *
			uadev[card_num].num_intf, GFP_KERNEL);
		if (!uadev[card_num].info) {
			ret = -ENOMEM;
			goto free_xfer_buf;
		}
		uadev[card_num].udev = subs->dev;
		atomic_set(&uadev[card_num].in_use, 1);
	} else {
		kref_get(&uadev[card_num].kref);
	}

	uadev[card_num].card_num = card_num;

	/* cache intf specific info to use it for free xfer buf */
	uadev[card_num].info[subs->interface].xfer_buf_pa = xfer_buf_pa;
	uadev[card_num].info[subs->interface].xfer_buf_size = len;
	uadev[card_num].info[subs->interface].xfer_buf = xfer_buf;
	uadev[card_num].info[subs->interface].pcm_card_num = card_num;
	uadev[card_num].info[subs->interface].pcm_dev_num = pcm_dev_num;
	uadev[card_num].info[subs->interface].direction = subs->direction;
	uadev[card_num].info[subs->interface].in_use = true;

	set_bit(card_num, &uaudio_qdev->card_slot);

	return 0;

free_xfer_buf:
	usb_free_coherent(subs->dev, len, xfer_buf, xfer_buf_pa);
err:
	return ret;
}

static void uaudio_dev_intf_cleanup(struct usb_device *udev,
	struct intf_info *info)
{
	usb_free_coherent(udev, info->xfer_buf_size,
		info->xfer_buf, info->xfer_buf_pa);
	info->xfer_buf_size = 0;
	info->xfer_buf = NULL;
	info->xfer_buf_pa = 0;

	info->in_use = false;
}

static void uaudio_dev_cleanup(struct uaudio_dev *dev)
{
	int if_idx;

	/* free xfer buffer */
	for (if_idx = 0; if_idx < dev->num_intf; if_idx++) {
		if (!dev->info[if_idx].in_use)
			continue;
		uaudio_dev_intf_cleanup(dev->udev, &dev->info[if_idx]);
		pr_debug("%s: release resources: intf# %d card# %d\n", __func__,
			if_idx, dev->card_num);
	}
	dev->num_intf = 0;

	/* free interface info */
	kfree(dev->info);
	dev->info = NULL;

	clear_bit(dev->card_num, &uaudio_qdev->card_slot);

	/* all audio devices are disconnected */
	if (!uaudio_qdev->card_slot) {
		usb_sec_event_ring_cleanup(dev->udev, uaudio_qdev->intr_num);
		pr_debug("%s: all audio devices disconnected\n", __func__);
	}

	dev->udev = NULL;
}

void uaudio_disconnect_cb(struct snd_usb_audio *chip)
{
	int ret;
	struct uaudio_dev *dev;
	int card_num = chip->card_num;
	struct uaudio_qmi_svc *svc = uaudio_svc;
	struct qmi_uaudio_stream_ind_msg_v01 disconnect_ind = {0};

	pr_debug("%s: for card# %d\n", __func__, card_num);

	if (card_num >=  SNDRV_CARDS) {
		pr_err("%s: invalid card number\n", __func__);
		return;
	}

	mutex_lock(&chip->dev_lock);
	dev = &uadev[card_num];

	/* clean up */
	if (!dev->udev) {
		pr_debug("%s: no clean up required\n", __func__);
		goto done;
	}

	if (atomic_read(&dev->in_use)) {
		mutex_unlock(&chip->dev_lock);

		pr_debug("%s: sending qmi indication disconnect\n", __func__);
		disconnect_ind.dev_event = USB_AUDIO_DEV_DISCONNECT_V01;
		disconnect_ind.slot_id = dev->udev->slot_id;
		ret = qmi_send_ind(svc->uaudio_svc_hdl, svc->curr_conn,
				&uaudio_stream_ind_desc, &disconnect_ind,
				sizeof(disconnect_ind));
		if (ret < 0) {
			pr_err("%s: qmi send failed wiht err: %d\n",
					__func__, ret);
			return;
		}

		ret = wait_event_interruptible(dev->disconnect_wq,
				!atomic_read(&dev->in_use));
		if (ret < 0) {
			pr_debug("%s: failed with ret %d\n", __func__, ret);
			return;
		}
		mutex_lock(&chip->dev_lock);
	}

	uaudio_dev_cleanup(dev);
done:
	mutex_unlock(&chip->dev_lock);
}

static void uaudio_dev_release(struct kref *kref)
{
	struct uaudio_dev *dev = container_of(kref, struct uaudio_dev, kref);

	pr_debug("%s for dev %pK\n", __func__, dev);

	atomic_set(&dev->in_use, 0);

	clear_bit(dev->card_num, &uaudio_qdev->card_slot);

	/* all audio devices are disconnected */
	if (!uaudio_qdev->card_slot) {
		usb_sec_event_ring_cleanup(dev->udev, uaudio_qdev->intr_num);
		pr_debug("%s: all audio devices disconnected\n", __func__);
	}

	wake_up(&dev->disconnect_wq);
}

/* maps audio format received over QMI to asound.h based pcm format */
static int map_pcm_format(unsigned int fmt_received)
{
	switch (fmt_received) {
	case USB_QMI_PCM_FORMAT_S8:
		return SNDRV_PCM_FORMAT_S8;
	case USB_QMI_PCM_FORMAT_U8:
		return SNDRV_PCM_FORMAT_U8;
	case USB_QMI_PCM_FORMAT_S16_LE:
		return SNDRV_PCM_FORMAT_S16_LE;
	case USB_QMI_PCM_FORMAT_S16_BE:
		return SNDRV_PCM_FORMAT_S16_BE;
	case USB_QMI_PCM_FORMAT_U16_LE:
		return SNDRV_PCM_FORMAT_U16_LE;
	case USB_QMI_PCM_FORMAT_U16_BE:
		return SNDRV_PCM_FORMAT_U16_BE;
	case USB_QMI_PCM_FORMAT_S24_LE:
		return SNDRV_PCM_FORMAT_S24_LE;
	case USB_QMI_PCM_FORMAT_S24_BE:
		return SNDRV_PCM_FORMAT_S24_BE;
	case USB_QMI_PCM_FORMAT_U24_LE:
		return SNDRV_PCM_FORMAT_U24_LE;
	case USB_QMI_PCM_FORMAT_U24_BE:
		return SNDRV_PCM_FORMAT_U24_BE;
	case USB_QMI_PCM_FORMAT_S24_3LE:
		return SNDRV_PCM_FORMAT_S24_3LE;
	case USB_QMI_PCM_FORMAT_S24_3BE:
		return SNDRV_PCM_FORMAT_S24_3BE;
	case USB_QMI_PCM_FORMAT_U24_3LE:
		return SNDRV_PCM_FORMAT_U24_3LE;
	case USB_QMI_PCM_FORMAT_U24_3BE:
		return SNDRV_PCM_FORMAT_U24_3BE;
	case USB_QMI_PCM_FORMAT_S32_LE:
		return SNDRV_PCM_FORMAT_S32_LE;
	case USB_QMI_PCM_FORMAT_S32_BE:
		return SNDRV_PCM_FORMAT_S32_BE;
	case USB_QMI_PCM_FORMAT_U32_LE:
		return SNDRV_PCM_FORMAT_U32_LE;
	case USB_QMI_PCM_FORMAT_U32_BE:
		return SNDRV_PCM_FORMAT_U32_BE;
	default:
		return -EINVAL;
	}
}

static int handle_uaudio_stream_req(void *req_h, void *req)
{
	struct qmi_uaudio_stream_req_msg_v01 *req_msg;
	struct qmi_uaudio_stream_resp_msg_v01 resp = {{0}, 0};
	struct snd_usb_substream *subs;
	struct snd_usb_audio *chip = NULL;
	struct uaudio_qmi_svc *svc = uaudio_svc;
	struct intf_info *info;
	int pcm_format;
	u8 pcm_card_num, pcm_dev_num, direction;
	int intf_num = -1, ret = 0;

	req_msg = (struct qmi_uaudio_stream_req_msg_v01 *)req;

	if (!req_msg->audio_format_valid || !req_msg->bit_rate_valid ||
	!req_msg->number_of_ch_valid || !req_msg->xfer_buff_size_valid) {
		pr_err("%s: invalid request msg\n", __func__);
		ret = -EINVAL;
		goto response;
	}

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

	pcm_format = map_pcm_format(req_msg->audio_format);
	if (pcm_format == -EINVAL) {
		pr_err("%s: unsupported pcm format received %d\n",
		__func__, req_msg->audio_format);
		ret = -EINVAL;
		goto response;
	}

	subs = find_snd_usb_substream(pcm_card_num, pcm_dev_num, direction,
					&chip, uaudio_disconnect_cb);
	if (!subs || !chip || chip->shutdown) {
		pr_err("%s: can't find substream for card# %u, dev# %u dir%u\n",
			__func__, pcm_card_num, pcm_dev_num, direction);
		ret = -ENODEV;
		goto response;
	}

	mutex_lock(&chip->dev_lock);
	intf_num = subs->interface;
	if (chip->shutdown || !subs->stream || !subs->stream->pcm
			|| !subs->stream->chip) {
		ret = -ENODEV;
		mutex_unlock(&chip->dev_lock);
		goto response;
	}

	subs->pcm_format = pcm_format;
	subs->channels = req_msg->number_of_ch;
	subs->cur_rate = req_msg->bit_rate;
	uadev[pcm_card_num].ctrl_intf = chip->ctrl_intf;

	ret = snd_usb_enable_audio_stream(subs, req_msg->enable);

	if (!ret && req_msg->enable)
		ret = prepare_qmi_response(subs, &resp, req_msg->xfer_buff_size,
			pcm_card_num, pcm_dev_num);

	mutex_unlock(&chip->dev_lock);

response:
	if (!req_msg->enable && ret != -EINVAL) {
		if (intf_num >= 0) {
			mutex_lock(&chip->dev_lock);
			info = &uadev[pcm_card_num].info[intf_num];
			uaudio_dev_intf_cleanup(uadev[pcm_card_num].udev, info);
			pr_debug("%s:release resources: intf# %d card# %d\n",
				__func__, intf_num, pcm_card_num);
			mutex_unlock(&chip->dev_lock);
		}
		if (atomic_read(&uadev[pcm_card_num].in_use))
			kref_put(&uadev[pcm_card_num].kref,
					uaudio_dev_release);
	}

	resp.usb_token = req_msg->usb_token;
	resp.usb_token_valid = 1;
	resp.internal_status = ret;
	resp.internal_status_valid = 1;
	resp.status = ret ? USB_AUDIO_STREAM_REQ_FAILURE_V01 : ret;
	resp.status_valid = 1;
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

static void uaudio_qmi_disconnect_work(struct work_struct *w)
{
	struct intf_info *info;
	int idx, if_idx;
	struct snd_usb_substream *subs;
	struct snd_usb_audio *chip = NULL;

	/* find all active intf for set alt 0 and cleanup usb audio dev */
	for (idx = 0; idx < SNDRV_CARDS; idx++) {
		if (!atomic_read(&uadev[idx].in_use))
			continue;

		for (if_idx = 0; if_idx < uadev[idx].num_intf; if_idx++) {
			if (!uadev[idx].info || !uadev[idx].info[if_idx].in_use)
				continue;
			info = &uadev[idx].info[if_idx];
			subs = find_snd_usb_substream(info->pcm_card_num,
							info->pcm_dev_num,
							info->direction,
							&chip,
							uaudio_disconnect_cb);
			if (!subs || !chip || chip->shutdown) {
				pr_debug("%s:no subs for c#%u, dev#%u dir%u\n",
					__func__, info->pcm_card_num,
					info->pcm_dev_num,
					info->direction);
				continue;
			}
			snd_usb_enable_audio_stream(subs, 0);
		}
		atomic_set(&uadev[idx].in_use, 0);
		mutex_lock(&chip->dev_lock);
		uaudio_dev_cleanup(&uadev[idx]);
		mutex_unlock(&chip->dev_lock);
	}
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
	queue_work(svc->uaudio_wq, &svc->qmi_disconnect_work);

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

static int uaudio_qmi_plat_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node = pdev->dev.of_node;

	uaudio_qdev = devm_kzalloc(&pdev->dev, sizeof(struct uaudio_qmi_dev),
		GFP_KERNEL);
	if (!uaudio_qdev)
		return -ENOMEM;

	uaudio_qdev->dev = &pdev->dev;

	ret = of_property_read_u32(node, "qcom,usb-audio-intr-num",
				&uaudio_qdev->intr_num);
	if (ret) {
		dev_err(&pdev->dev, "failed to read intr num.\n");
		return -ENODEV;
	}
	return 0;
}

static int uaudio_qmi_plat_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id of_uaudio_matach[] = {
	{
		.compatible = "qcom,usb-audio-qmi-dev",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_uaudio_matach);

static struct platform_driver uaudio_qmi_driver = {
	.probe		= uaudio_qmi_plat_probe,
	.remove		= uaudio_qmi_plat_remove,
	.driver		= {
		.name	= "uaudio-qmi",
		.of_match_table	= of_uaudio_matach,
	},
};

static int uaudio_qmi_svc_init(void)
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
	INIT_WORK(&svc->qmi_disconnect_work, uaudio_qmi_disconnect_work);

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

static void uaudio_qmi_svc_exit(void)
{
	struct uaudio_qmi_svc *svc = uaudio_svc;

	qmi_svc_unregister(svc->uaudio_svc_hdl);
	flush_workqueue(svc->uaudio_wq);
	qmi_handle_destroy(svc->uaudio_svc_hdl);
	destroy_workqueue(svc->uaudio_wq);
	kfree(svc);
	uaudio_svc = NULL;
}

static int __init uaudio_qmi_plat_init(void)
{
	int ret;

	ret = platform_driver_register(&uaudio_qmi_driver);
	if (ret)
		return ret;

	return uaudio_qmi_svc_init();
}

static void __exit uaudio_qmi_plat_exit(void)
{
	uaudio_qmi_svc_exit();
	platform_driver_unregister(&uaudio_qmi_driver);
}

module_init(uaudio_qmi_plat_init);
module_exit(uaudio_qmi_plat_exit);

MODULE_DESCRIPTION("USB AUDIO QMI Service Driver");
MODULE_LICENSE("GPL v2");
