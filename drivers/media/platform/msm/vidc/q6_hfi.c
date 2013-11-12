/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/qdsp6v2/apr.h>
#include <mach/subsystem_restart.h>
#include "hfi_packetization.h"
#include "msm_vidc_debug.h"
#include "q6_hfi.h"
#include "vidc_hfi_api.h"

static struct hal_device_data hal_ctxt;

static int write_queue(void *info, u8 *packet)
{
	u32 packet_size_in_words, new_write_idx;
	struct q6_iface_q_info *qinfo;
	u32 empty_space, read_idx;
	u32 *write_ptr;

	if (!info || !packet) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}

	qinfo = (struct q6_iface_q_info *) info;

	packet_size_in_words = (*(u32 *)packet) >> 2;

	if (packet_size_in_words == 0) {
		dprintk(VIDC_ERR, "Zero packet size");
		return -ENODATA;
	}

	read_idx = qinfo->read_idx;

	empty_space = (qinfo->write_idx >=  read_idx) ?
		(qinfo->q_size - (qinfo->write_idx -  read_idx)) :
		(read_idx - qinfo->write_idx);
	if (empty_space <= packet_size_in_words) {
		dprintk(VIDC_ERR, "Insufficient size (%d) to write (%d)",
					  empty_space, packet_size_in_words);
		return -ENOTEMPTY;
	}

	new_write_idx = (qinfo->write_idx + packet_size_in_words);
	write_ptr = (u32 *)(qinfo->buffer + (qinfo->write_idx << 2));
	if (new_write_idx < qinfo->q_size) {
		memcpy(write_ptr, packet, packet_size_in_words << 2);
	} else {
		new_write_idx -= qinfo->q_size;
		memcpy(write_ptr, packet, (packet_size_in_words -
			new_write_idx) << 2);
		memcpy((void *)qinfo->buffer,
			packet + ((packet_size_in_words - new_write_idx) << 2),
			new_write_idx  << 2);
	}
	qinfo->write_idx = new_write_idx;
	return 0;
}

static int read_queue(void *info, u8 *packet)
{
	u32 packet_size_in_words, new_read_idx;
	u32 *read_ptr;
	struct q6_iface_q_info *qinfo;

	if (!info || !packet) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}

	qinfo = (struct q6_iface_q_info *) info;

	if (qinfo->read_idx == qinfo->write_idx)
		return -EPERM;

	read_ptr = (u32 *)(qinfo->buffer + (qinfo->read_idx << 2));
	packet_size_in_words = (*read_ptr) >> 2;
	if (packet_size_in_words == 0) {
		dprintk(VIDC_ERR, "Zero packet size");
		return -ENODATA;
	}

	new_read_idx = qinfo->read_idx + packet_size_in_words;
	if (new_read_idx < qinfo->q_size) {
		memcpy(packet, read_ptr,
			packet_size_in_words << 2);
	} else {
		new_read_idx -= qinfo->q_size;
		memcpy(packet, read_ptr,
			(packet_size_in_words - new_read_idx) << 2);
		memcpy(packet + ((packet_size_in_words -
			new_read_idx) << 2),
			(u8 *)qinfo->buffer,
			new_read_idx << 2);
	}

	qinfo->read_idx = new_read_idx;
	return 0;
}

static int q6_hfi_iface_eventq_write(struct q6_hfi_device *device, void *pkt)
{
	struct q6_iface_q_info *q_info;
	int rc = 0;
	unsigned long flags = 0;

	if (!device || !pkt) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}

	q_info = &device->event_queue;
	if (!q_info->buffer) {
		dprintk(VIDC_ERR, "cannot write to shared Q");
		rc = -ENODATA;
		goto err_q_write;
	}

	spin_lock_irqsave(&q_info->lock, flags);
	rc = write_queue(q_info, (u8 *)pkt);
	if (rc)
		dprintk(VIDC_ERR, "q6_hfi_iface_eventq_write: queue_full");

	spin_unlock_irqrestore(&q_info->lock, flags);
err_q_write:
	return rc;
}

static int q6_hfi_iface_eventq_read(struct q6_hfi_device *device, void *pkt)
{
	int rc = 0;
	struct q6_iface_q_info *q_info;
	unsigned long flags = 0;

	if (!pkt) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}

	q_info = &device->event_queue;

	if (!q_info->buffer) {
		dprintk(VIDC_ERR, "cannot read from shared Q");
		rc = -ENODATA;
		goto read_error;
	}

	spin_lock_irqsave(&q_info->lock, flags);
	rc = read_queue(q_info, (u8 *)pkt);
	if (rc) {
		dprintk(VIDC_INFO, "q6_hfi_iface_eventq_read:queue_empty");
		rc = -ENODATA;
	}
	spin_unlock_irqrestore(&q_info->lock, flags);

read_error:
	return rc;
}

static void q6_hfi_core_work_handler(struct work_struct *work)
{
	int rc = 0;
	struct q6_hfi_device *device = container_of(
		work, struct q6_hfi_device, vidc_worker);
	u8 packet[VIDC_IFACEQ_MED_PKT_SIZE];

	/* need to consume all the messages from the firmware */
	do {
		rc = q6_hfi_iface_eventq_read(device, packet);
		if (!rc)
			hfi_process_msg_packet(device->callback,
				device->device_id,
				(struct vidc_hal_msg_pkt_hdr *) packet,
				&device->sess_head, &device->session_lock);
	} while (!rc);

	if (rc != -ENODATA)
		dprintk(VIDC_ERR, "Failed to read from event queue");
}

static int q6_hfi_register_iommu_domains(struct q6_hfi_device *device)
{
	struct iommu_domain *domain;
	int rc = 0, i = 0;
	struct iommu_set *iommu_group_set;
	struct iommu_info *iommu_map;

	if (!device || !device->res) {
		dprintk(VIDC_ERR, "Invalid parameter: %p", device);
		return -EINVAL;
	}

	iommu_group_set = &device->res->iommu_group_set;

	for (i = 0; i < iommu_group_set->count; i++) {
		iommu_map = &iommu_group_set->iommu_maps[i];
		iommu_map->group = iommu_group_find(iommu_map->name);
		if (!iommu_map->group) {
			dprintk(VIDC_ERR, "Failed to find group :%s\n",
					iommu_map->name);
			goto fail_group;
		}
		domain = iommu_group_get_iommudata(iommu_map->group);
		if (IS_ERR_OR_NULL(domain)) {
			dprintk(VIDC_ERR,
					"Failed to get domain data for group %p",
					iommu_map->group);
			goto fail_group;
		}
		iommu_map->domain = msm_find_domain_no(domain);
		if (iommu_map->domain < 0) {
			dprintk(VIDC_ERR,
					"Failed to get domain index for domain %p",
					domain);
			goto fail_group;
		}
	}
	return rc;

fail_group:
	for (--i; i >= 0; i--) {
		iommu_map = &iommu_group_set->iommu_maps[i];
		if (iommu_map->group)
			iommu_group_put(iommu_map->group);
		iommu_map->group = NULL;
		iommu_map->domain = -1;
	}
	return -EINVAL;
}

static void q6_hfi_deregister_iommu_domains(struct q6_hfi_device *device)
{
	struct iommu_set *iommu_group_set;
	struct iommu_info *iommu_map;
	int i = 0;

	if (!device || !device->res) {
		dprintk(VIDC_ERR, "Invalid parameter: %p", device);
		return;
	}

	iommu_group_set = &device->res->iommu_group_set;
	for (i = 0; i < iommu_group_set->count; i++) {
		iommu_map = &iommu_group_set->iommu_maps[i];
		if (iommu_map->group)
			iommu_group_put(iommu_map->group);
		iommu_map->group = NULL;
		iommu_map->domain = -1;
	}
}

static int q6_hfi_init_resources(struct q6_hfi_device *device,
		struct msm_vidc_platform_resources *res)
{
	int rc = 0;

	if (!device || !res) {
		dprintk(VIDC_ERR, "Invalid device or resources");
		return -EINVAL;
	}

	device->res = res;
	rc = q6_hfi_register_iommu_domains(device);
	if (rc)
		dprintk(VIDC_ERR, "Failed to register iommu domains: %d\n", rc);

	return rc;
}

static void q6_hfi_deinit_resources(struct q6_hfi_device *device)
{
	q6_hfi_deregister_iommu_domains(device);
}

static void *q6_hfi_add_device(u32 device_id,
			hfi_cmd_response_callback callback)
{
	struct q6_hfi_device *hdevice = NULL;

	if (!callback) {
		dprintk(VIDC_ERR, "Invalid Paramters");
		return NULL;
	}

	hdevice = (struct q6_hfi_device *)
			kzalloc(sizeof(struct q6_hfi_device), GFP_KERNEL);
	if (!hdevice) {
		dprintk(VIDC_ERR, "failed to allocate new device");
		goto err_alloc;
	}

	hdevice->device_id = device_id;
	hdevice->callback = callback;

	dprintk(VIDC_DBG, "q6_hfi_add_device device_id %d\n", device_id);

	INIT_WORK(&hdevice->vidc_worker, q6_hfi_core_work_handler);
	hdevice->vidc_workq = create_singlethread_workqueue(
		"msm_vidc_workerq_q6");
	if (!hdevice->vidc_workq) {
		dprintk(VIDC_ERR, ": create workq failed\n");
		goto error_createq;
	}

	if (hal_ctxt.dev_count == 0)
		INIT_LIST_HEAD(&hal_ctxt.dev_head);

	INIT_LIST_HEAD(&hdevice->list);
	list_add_tail(&hdevice->list, &hal_ctxt.dev_head);
	hal_ctxt.dev_count++;

	return (void *) hdevice;
error_createq:
	kfree(hdevice);
err_alloc:
	return NULL;
}

static void *q6_hfi_get_device(u32 device_id,
				struct msm_vidc_platform_resources *res,
				hfi_cmd_response_callback callback)
{
	struct q6_hfi_device *device;
	int rc = 0;

	if (!callback) {
		dprintk(VIDC_ERR, "%s Invalid params:  %p\n",
			__func__, callback);
		return NULL;
	}

	device = q6_hfi_add_device(device_id, &handle_cmd_response);
	if (!device) {
		dprintk(VIDC_ERR, "Failed to create HFI device\n");
		return NULL;
	}

	rc = q6_hfi_init_resources(device, res);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to init resources: %d\n", rc);
		goto err_fail_init_res;
	}
	return device;

err_fail_init_res:
	q6_hfi_delete_device(device);
	return NULL;
}

void q6_hfi_delete_device(void *device)
{
	struct q6_hfi_device *close, *tmp, *dev;

	if (device) {
		q6_hfi_deinit_resources(device);
		dev = (struct q6_hfi_device *) device;
		list_for_each_entry_safe(close, tmp, &hal_ctxt.dev_head, list) {
			if (close->device_id == dev->device_id) {
				hal_ctxt.dev_count--;
				list_del(&close->list);
				destroy_workqueue(close->vidc_workq);
				kfree(close);
				break;
			}
		}

	}
}

static inline void q6_hfi_add_apr_hdr(struct q6_hfi_device *dev,
			struct apr_hdr *hdr, u32 pkt_size)
{
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(sizeof(struct apr_hdr)),
				APR_PKT_VER);

	hdr->src_svc = ((struct apr_svc *)dev->apr)->id;
	hdr->src_domain = APR_DOMAIN_APPS;
	hdr->dest_svc = APR_SVC_VIDC;
	hdr->src_port = 0;
	hdr->dest_port = 0;
	hdr->pkt_size  = pkt_size;
	hdr->token = 0;
	hdr->opcode = VIDEO_HFI_CMD_ID;
}

static int q6_hfi_apr_callback(struct apr_client_data *data, void *priv)
{
	struct q6_hfi_device *device = priv;
	struct hfi_msg_event_notify_packet pkt = {0};
	void *payload = NULL;
	int rc = 0;

	if (!data || !device) {
		dprintk(VIDC_ERR, "%s - Invalid arguments", __func__);
		return -EINVAL;
	}

	dprintk(VIDC_DBG, "%s opcode = %u payload size = %u", __func__,
				data->opcode, data->payload_size);

	if (data->opcode == RESET_EVENTS) {
		dprintk(VIDC_ERR, "%s Received subsystem reset event: %d",
				__func__, data->reset_event);
		pkt.packet_type = HFI_MSG_EVENT_NOTIFY;
		pkt.size = sizeof(pkt);
		pkt.event_id = HFI_EVENT_SYS_ERROR;
		pkt.event_data1 = data->opcode;
		pkt.event_data2 = data->reset_event;
		payload = &pkt;
	} else if (data->payload_size > 0) {
		payload = data->payload;
	} else {
		dprintk(VIDC_ERR, "%s - Invalid payload size", __func__);
		return -EINVAL;
	}

	rc = q6_hfi_iface_eventq_write(device, payload);
	if (rc) {
		dprintk(VIDC_ERR, "%s failed to write to event queue",
				__func__);
		return rc;
	}
	queue_work(device->vidc_workq, &device->vidc_worker);
	return 0;
}

static void q6_release_event_queue(struct q6_hfi_device *device)
{
	kfree(device->event_queue.buffer);
	device->event_queue.buffer = NULL;
	device->event_queue.q_size = 0;
	device->event_queue.read_idx = 0;
	device->event_queue.write_idx = 0;
}

static int q6_init_event_queue(struct q6_hfi_device *dev)
{
	struct q6_iface_q_info *iface_q;

	if (!dev) {
		dprintk(VIDC_ERR, "Invalid device");
		return -EINVAL;
	}

	iface_q = &dev->event_queue;
	iface_q->buffer = kzalloc(Q6_IFACEQ_QUEUE_SIZE, GFP_KERNEL);
	if (!iface_q->buffer) {
		dprintk(VIDC_ERR, "iface_q alloc failed");
		q6_release_event_queue(dev);
		return -ENOMEM;
	} else {
		iface_q->q_size = Q6_IFACEQ_QUEUE_SIZE / 4;
		iface_q->read_idx = 0;
		iface_q->write_idx = 0;
		spin_lock_init(&iface_q->lock);
	}
	return 0;
}

static int q6_hfi_core_init(void *device)
{
	struct q6_apr_cmd_sys_init_packet apr;
	int rc = 0;
	struct q6_hfi_device *dev = device;

	if (!dev) {
		dprintk(VIDC_ERR, "%s: invalid argument\n", __func__);
		return -ENODEV;
	}

	INIT_LIST_HEAD(&dev->sess_head);
	mutex_init(&dev->session_lock);

	if (!dev->event_queue.buffer) {
		rc = q6_init_event_queue(dev);
		if (rc) {
			dprintk(VIDC_ERR, "q6_init_event_queue failed");
			goto err_core_init;
		}
	} else {
		dprintk(VIDC_ERR, "queue buffer exists");
		rc = -EEXIST;
		goto err_core_init;
	}

	q6_hfi_add_apr_hdr(dev, &apr.hdr, sizeof(apr));

	rc = create_pkt_cmd_sys_init(&apr.pkt, HFI_VIDEO_ARCH_OX);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to create sys init pkt");
		goto err_core_init;
	}

	rc = apr_send_pkt(dev->apr, (uint32_t *)&apr);
	if (rc != apr.hdr.pkt_size) {
		dprintk(VIDC_ERR, "%s: apr_send_pkt failed rc: %d",
				__func__, rc);
		rc = -EBADE;
	} else
		rc = 0;
err_core_init:
	return rc;
}

static int q6_hfi_core_release(void *device)
{
	struct q6_hfi_device *dev = device;

	if (!dev) {
		dprintk(VIDC_ERR, "%s: invalid argument\n", __func__);
		return -ENODEV;
	}
	q6_release_event_queue(dev);

	dprintk(VIDC_DBG, "HAL exited\n");
	return 0;
}

static void *q6_hfi_session_init(void *device, u32 session_id,
	enum hal_domain session_type, enum hal_video_codec codec_type)
{
	struct q6_apr_cmd_sys_session_init_packet apr;
	struct hal_session *new_session;
	struct q6_hfi_device *dev = device;
	int rc = 0;

	if (!dev) {
		dprintk(VIDC_ERR, "%s: invalid argument\n", __func__);
		return NULL;
	}

	new_session = (struct hal_session *)
		kzalloc(sizeof(struct hal_session), GFP_KERNEL);
	new_session->session_id = (u32) session_id;
	if (session_type == 1)
		new_session->is_decoder = 0;
	else if (session_type == 2)
		new_session->is_decoder = 1;
	new_session->device = dev;

	q6_hfi_add_apr_hdr(dev, &apr.hdr, sizeof(apr));

	if (create_pkt_cmd_sys_session_init(&apr.pkt, (u32)new_session,
					session_type, codec_type)) {
		dprintk(VIDC_ERR, "session_init: failed to create packet");
		goto err_session_init;
	}
	/*
	 * Add session id to the list entry and then send the apr pkt.
	 * This will avoid scenarios where apr_send_pkt is taking more
	 * time and Q6 is returning an ack even before the session id
	 * gets added to the session list.
	 */
	mutex_lock(&dev->session_lock);
	list_add_tail(&new_session->list, &dev->sess_head);
	mutex_unlock(&dev->session_lock);

	rc = apr_send_pkt(dev->apr, (uint32_t *)&apr);
	if (rc != apr.hdr.pkt_size) {
		dprintk(VIDC_ERR, "%s: apr_send_pkt failed rc: %d",
				__func__, rc);
		/* Delete the session id as the send pkt is not successful */
		mutex_lock(&dev->session_lock);
		list_del(&new_session->list);
		mutex_unlock(&dev->session_lock);
		rc = -EBADE;
		goto err_session_init;
	}

	return new_session;

err_session_init:
	kfree(new_session);
	return NULL;
}

static int q6_hal_send_session_cmd(void *sess,
	 int pkt_type)
{
	struct q6_apr_session_cmd_pkt apr;
	int rc = 0;
	struct hal_session *session = sess;
	struct q6_hfi_device *dev;

	if (!session || !session->device) {
		dprintk(VIDC_ERR, "%s: invalid arguments\n", __func__);
		return -EINVAL;
	}
	dev = session->device;

	q6_hfi_add_apr_hdr(dev, &apr.hdr, sizeof(apr));

	rc = create_pkt_cmd_session_cmd(&apr.pkt, pkt_type, (u32)session);
	if (rc) {
		dprintk(VIDC_ERR, "send session cmd: create pkt failed");
		goto err_create_pkt;
	}

	rc = apr_send_pkt(dev->apr, (uint32_t *)&apr);
	if (rc != apr.hdr.pkt_size) {
		dprintk(VIDC_ERR, "%s: apr_send_pkt failed rc: %d",
				__func__, rc);
		rc = -EBADE;
	} else
		rc = 0;
err_create_pkt:
	return rc;
}

static int q6_hfi_session_end(void *session)
{
	return q6_hal_send_session_cmd(session,
		HFI_CMD_SYS_SESSION_END);
}

static int q6_hfi_session_abort(void *session)
{
	return q6_hal_send_session_cmd(session,
		HFI_CMD_SYS_SESSION_ABORT);
}

static int q6_hfi_session_clean(void *session)
{
	struct hal_session *sess_close;
	if (!session) {
		dprintk(VIDC_ERR, "Invalid Params %s", __func__);
		return -EINVAL;
	}
	sess_close = session;
	dprintk(VIDC_DBG, "deleted the session: 0x%x",
			sess_close->session_id);
	mutex_lock(&((struct q6_hfi_device *)
			sess_close->device)->session_lock);
	list_del(&sess_close->list);
	mutex_unlock(&((struct q6_hfi_device *)
			sess_close->device)->session_lock);
	kfree(sess_close);
	return 0;
}

static int q6_hfi_session_set_buffers(void *sess,
	struct vidc_buffer_addr_info *buffer_info)
{
	struct q6_apr_cmd_session_set_buffers_packet *apr;
	u8 packet[VIDC_IFACEQ_VAR_LARGE_PKT_SIZE];
	int rc = 0;
	struct hal_session *session = sess;
	struct q6_hfi_device *dev;

	if (!session || !buffer_info || !session->device) {
		dprintk(VIDC_ERR, "%s: invalid arguments\n", __func__);
		return -EINVAL;
	}
	dev = session->device;

	if (buffer_info->buffer_type == HAL_BUFFER_INPUT)
		return 0;
	apr = (struct q6_apr_cmd_session_set_buffers_packet *)packet;

	q6_hfi_add_apr_hdr(dev, &apr->hdr, VIDC_IFACEQ_VAR_LARGE_PKT_SIZE);

	rc = create_pkt_cmd_session_set_buffers(&apr->pkt,
			(u32)session, buffer_info);
	if (rc) {
		dprintk(VIDC_ERR, "set buffers: failed to create packet");
		goto err_create_pkt;
	}

	dprintk(VIDC_INFO, "set buffers: 0x%x", buffer_info->buffer_type);
	rc = apr_send_pkt(dev->apr, (uint32_t *)apr);
	if (rc != apr->hdr.pkt_size) {
		dprintk(VIDC_ERR, "%s: apr_send_pkt failed rc: %d",
				__func__, rc);
		rc = -EBADE;
	} else
		rc = 0;
err_create_pkt:
	return rc;
}

static int q6_hfi_session_release_buffers(void *sess,
	struct vidc_buffer_addr_info *buffer_info)
{
	struct q6_apr_cmd_session_release_buffer_packet *apr;
	u8 packet[VIDC_IFACEQ_VAR_LARGE_PKT_SIZE];
	int rc = 0;
	struct hal_session *session = sess;
	struct q6_hfi_device *dev;

	if (!session || !buffer_info || !session->device) {
		dprintk(VIDC_ERR, "%s: invalid arguments\n", __func__);
		return -EINVAL;
	}

	dev = session->device;

	if (buffer_info->buffer_type == HAL_BUFFER_INPUT)
		return 0;

	apr = (struct q6_apr_cmd_session_release_buffer_packet *) packet;

	q6_hfi_add_apr_hdr(dev, &apr->hdr, VIDC_IFACEQ_VAR_LARGE_PKT_SIZE);

	rc = create_pkt_cmd_session_release_buffers(&apr->pkt,
					(u32)session, buffer_info);
	if (rc) {
		dprintk(VIDC_ERR, "release buffers: failed to create packet");
		goto err_create_pkt;
	}

	dprintk(VIDC_INFO, "Release buffers: 0x%x", buffer_info->buffer_type);
	rc = apr_send_pkt(dev->apr, (uint32_t *)apr);

	if (rc != apr->hdr.pkt_size) {
		dprintk(VIDC_ERR, "%s: apr_send_pkt failed rc: %d",
				__func__, rc);
		rc = -EBADE;
	} else
		rc = 0;
err_create_pkt:
	return rc;
}

static int q6_hfi_session_load_res(void *sess)
{
	return q6_hal_send_session_cmd(sess,
		HFI_CMD_SESSION_LOAD_RESOURCES);
}

static int q6_hfi_session_release_res(void *sess)
{
	return q6_hal_send_session_cmd(sess,
		HFI_CMD_SESSION_RELEASE_RESOURCES);
}

static int q6_hfi_session_start(void *sess)
{
	return q6_hal_send_session_cmd(sess,
		HFI_CMD_SESSION_START);
}

static int q6_hfi_session_stop(void *sess)
{
	return q6_hal_send_session_cmd(sess,
		HFI_CMD_SESSION_STOP);
}

static int q6_hfi_session_suspend(void *sess)
{
	return q6_hal_send_session_cmd(sess,
		HFI_CMD_SESSION_SUSPEND);
}

static int q6_hfi_session_resume(void *sess)
{
	return q6_hal_send_session_cmd(sess,
		HFI_CMD_SESSION_RESUME);
}

static int q6_hfi_session_etb(void *sess,
			struct vidc_frame_data *input_frame)
{
	int rc = 0;
	struct hal_session *session = sess;
	struct q6_hfi_device *dev;

	if (!session || !input_frame || !session->device) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}

	dev = session->device;

	if (session->is_decoder) {
		struct q6_apr_cmd_session_empty_buffer_compressed_packet apr;
		q6_hfi_add_apr_hdr(dev, &apr.hdr, sizeof(apr));

		rc = create_pkt_cmd_session_etb_decoder(&apr.pkt,
					(u32)session, input_frame);
		if (rc) {
			dprintk(VIDC_ERR,
				"Session etb decoder: failed to create pkt");
			goto err_create_pkt;
		}
		dprintk(VIDC_DBG, "Q DECODER INPUT BUFFER");
		dprintk(VIDC_DBG, "addr = 0x%x ts = %lld",
			input_frame->device_addr, input_frame->timestamp);
		rc = apr_send_pkt(dev->apr, (uint32_t *)&apr);
		if (rc != apr.hdr.pkt_size) {
			dprintk(VIDC_ERR, "%s: apr_send_pkt failed rc: %d",
					__func__, rc);
			rc = -EBADE;
		} else
			rc = 0;
	} else {
		struct
		q6_apr_cmd_session_empty_buffer_uncompressed_plane0_packet apr;
		q6_hfi_add_apr_hdr(dev, &apr.hdr, sizeof(apr));

		rc =  create_pkt_cmd_session_etb_encoder(&apr.pkt,
					(u32)session, input_frame);
		if (rc) {
			dprintk(VIDC_ERR,
				"Session etb encoder: failed to create pkt");
			goto err_create_pkt;
		}
		dprintk(VIDC_DBG, "Q ENCODER INPUT BUFFER");
		rc = apr_send_pkt(dev->apr, (uint32_t *)&apr);
		if (rc != apr.hdr.pkt_size) {
			dprintk(VIDC_ERR, "%s: apr_send_pkt failed rc: %d",
					__func__, rc);
			rc = -EBADE;
		} else
			rc = 0;
	}
err_create_pkt:
	return rc;
}

static int q6_hfi_session_ftb(void *sess,
	struct vidc_frame_data *output_frame)
{
	struct q6_apr_cmd_session_fill_buffer_packet apr;
	int rc = 0;
	struct hal_session *session = sess;
	struct q6_hfi_device *dev;

	if (!session || !output_frame || !session->device) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	dev = session->device;

	q6_hfi_add_apr_hdr(dev, &apr.hdr, sizeof(apr));

	rc = create_pkt_cmd_session_ftb(&apr.pkt, (u32)session, output_frame);
	if (rc) {
		dprintk(VIDC_ERR, "Session ftb: failed to create pkt");
		goto err_create_pkt;
	}

	dprintk(VIDC_INFO, "Q OUTPUT BUFFER");
	rc = apr_send_pkt(dev->apr, (uint32_t *)&apr);
	if (rc != apr.hdr.pkt_size) {
		dprintk(VIDC_ERR, "%s: apr_send_pkt failed rc: %d",
				__func__, rc);
		rc = -EBADE;
	} else
		rc = 0;
err_create_pkt:
	return rc;
}

static int q6_hfi_session_parse_seq_hdr(void *sess,
	struct vidc_seq_hdr *seq_hdr)
{
	struct q6_apr_cmd_session_parse_sequence_header_packet *apr;
	int rc = 0;
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	struct hal_session *session = sess;
	struct q6_hfi_device *dev;

	if (!session || !seq_hdr || !session->device) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	dev = session->device;

	apr = (struct q6_apr_cmd_session_parse_sequence_header_packet *) packet;

	q6_hfi_add_apr_hdr(dev, &apr->hdr, VIDC_IFACEQ_VAR_SMALL_PKT_SIZE);

	rc = create_pkt_cmd_session_parse_seq_header(&apr->pkt,
					(u32)session, seq_hdr);
	if (rc) {
		dprintk(VIDC_ERR,
			"Session parse seq hdr: failed to create pkt");
		goto err_create_pkt;
	}

	rc = apr_send_pkt(dev->apr, (uint32_t *)apr);
	if (rc != apr->hdr.pkt_size) {
		dprintk(VIDC_ERR, "%s: apr_send_pkt failed rc: %d",
				__func__, rc);
		rc = -EBADE;
	} else
		rc = 0;
err_create_pkt:
	return rc;
}

static int q6_hfi_session_get_seq_hdr(void *sess,
	struct vidc_seq_hdr *seq_hdr)
{
	struct q6_apr_cmd_session_get_sequence_header_packet *apr;
	int rc = 0;
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	struct hal_session *session = sess;
	struct q6_hfi_device *dev;

	if (!session || !seq_hdr || !session->device) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	dev = session->device;

	apr = (struct q6_apr_cmd_session_get_sequence_header_packet *) packet;

	q6_hfi_add_apr_hdr(dev, &apr->hdr, VIDC_IFACEQ_VAR_SMALL_PKT_SIZE);

	rc = create_pkt_cmd_session_get_seq_hdr(&apr->pkt, (u32)session,
						seq_hdr);
	if (rc) {
		dprintk(VIDC_ERR, "Session get seq hdr: failed to create pkt");
		goto err_create_pkt;
	}

	rc = apr_send_pkt(dev->apr, (uint32_t *)apr);
	if (rc != apr->hdr.pkt_size) {
		dprintk(VIDC_ERR, "%s: apr_send_pkt failed rc: %d",
				__func__, rc);
		rc = -EBADE;
	} else
		rc = 0;
err_create_pkt:
	return rc;
}

static int q6_hfi_session_get_buf_req(void *sess)
{
	struct q6_apr_cmd_session_get_property_packet apr;
	int rc = 0;
	struct hal_session *session = sess;

	struct q6_hfi_device *dev;

	if (!session || !session->device) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	dev = session->device;

	q6_hfi_add_apr_hdr(dev, &apr.hdr, sizeof(apr));

	rc = create_pkt_cmd_session_get_buf_req(&apr.pkt, (u32)session);
	if (rc) {
		dprintk(VIDC_ERR, "Session get buf req: failed to create pkt");
		goto err_create_pkt;
	}

	rc = apr_send_pkt(dev->apr, (uint32_t *)&apr);
	if (rc != apr.hdr.pkt_size) {
		dprintk(VIDC_ERR, "%s: apr_send_pkt failed rc: %d",
				__func__, rc);
		rc = -EBADE;
	} else
		rc = 0;
err_create_pkt:
	return rc;
}

static int q6_hfi_session_flush(void *sess, enum hal_flush flush_mode)
{
	struct q6_apr_cmd_session_flush_packet apr;
	int rc = 0;
	struct hal_session *session = sess;
	struct q6_hfi_device *dev;

	if (!session || !session->device) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	dev = session->device;

	q6_hfi_add_apr_hdr(dev, &apr.hdr, sizeof(apr));

	rc = create_pkt_cmd_session_flush(&apr.pkt, (u32)session, flush_mode);
	if (rc) {
		dprintk(VIDC_ERR, "Session flush: failed to create pkt");
		goto err_create_pkt;
	}

	rc = apr_send_pkt(dev->apr, (uint32_t *)&apr);
	if (rc != apr.hdr.pkt_size) {
		dprintk(VIDC_ERR, "%s: apr_send_pkt failed rc: %d",
				__func__, rc);
		rc = -EBADE;
	} else
		rc = 0;

err_create_pkt:
	return rc;
}

static int q6_hfi_session_set_property(void *sess,
	enum hal_property ptype, void *pdata)
{
	u8 packet[VIDC_IFACEQ_VAR_LARGE_PKT_SIZE];
	struct q6_apr_cmd_session_set_property_packet *apr =
		(struct q6_apr_cmd_session_set_property_packet *) &packet;
	struct hal_session *session = sess;
	int rc = 0;
	struct q6_hfi_device *dev;

	if (!session || !pdata || !session->device) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	dev = session->device;
	dprintk(VIDC_DBG, "in set_prop,with prop id: 0x%x", ptype);

	q6_hfi_add_apr_hdr(dev, &apr->hdr, VIDC_IFACEQ_VAR_LARGE_PKT_SIZE);

	rc = create_pkt_cmd_session_set_property(&apr->pkt,
				(u32)session, ptype, pdata);
	if (rc) {
		dprintk(VIDC_ERR, "set property: failed to create packet");
		goto err_create_pkt;
	}

	rc = apr_send_pkt(dev->apr, (uint32_t *)apr);
	if (rc != apr->hdr.pkt_size) {
		dprintk(VIDC_ERR, "%s: apr_send_pkt failed rc: %d",
				__func__, rc);
		rc = -EBADE;
	} else
		rc = 0;

err_create_pkt:
	return rc;
}

static int q6_hfi_session_get_property(void *sess,
	enum hal_property ptype, void *pdata)
{
	struct hal_session *session = sess;
	struct q6_hfi_device *dev;

	if (!session || !pdata || !session->device) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	dev = session->device;

	dprintk(VIDC_DBG, "IN func: , with property id: %d", ptype);

	switch (ptype) {
	case HAL_CONFIG_FRAME_RATE:
		break;
	case HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT:
		break;
	case HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_CONSTRAINTS_INFO:
		break;
	case HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_INFO:
		break;
	case HAL_PARAM_EXTRA_DATA_HEADER_CONFIG:
		break;
	case HAL_PARAM_FRAME_SIZE:
		break;
	case HAL_CONFIG_REALTIME:
		break;
	case HAL_PARAM_BUFFER_COUNT_ACTUAL:
		break;
	case HAL_PARAM_NAL_STREAM_FORMAT_SELECT:
		break;
	case HAL_PARAM_VDEC_OUTPUT_ORDER:
		break;
	case HAL_PARAM_VDEC_PICTURE_TYPE_DECODE:
		break;
	case HAL_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO:
		break;
	case HAL_CONFIG_VDEC_POST_LOOP_DEBLOCKER:
		break;
	case HAL_PARAM_VDEC_MULTI_STREAM:
		break;
	case HAL_PARAM_VDEC_DISPLAY_PICTURE_BUFFER_COUNT:
		break;
	case HAL_PARAM_DIVX_FORMAT:
		break;
	case HAL_CONFIG_VDEC_MB_ERROR_MAP_REPORTING:
		break;
	case HAL_PARAM_VDEC_CONTINUE_DATA_TRANSFER:
		break;
	case HAL_CONFIG_VDEC_MB_ERROR_MAP:
		break;
	case HAL_CONFIG_VENC_REQUEST_IFRAME:
		break;
	case HAL_PARAM_VENC_MPEG4_SHORT_HEADER:
		break;
	case HAL_PARAM_VENC_MPEG4_AC_PREDICTION:
		break;
	case HAL_CONFIG_VENC_TARGET_BITRATE:
		break;
	case HAL_PARAM_PROFILE_LEVEL_CURRENT:
		break;
	case HAL_PARAM_VENC_H264_ENTROPY_CONTROL:
		break;
	case HAL_PARAM_VENC_RATE_CONTROL:
		break;
	case HAL_PARAM_VENC_MPEG4_TIME_RESOLUTION:
		break;
	case HAL_PARAM_VENC_MPEG4_HEADER_EXTENSION:
		break;
	case HAL_PARAM_VENC_H264_DEBLOCK_CONTROL:
		break;
	case HAL_PARAM_VENC_SESSION_QP:
		break;
	case HAL_CONFIG_VENC_INTRA_PERIOD:
		break;
	case HAL_CONFIG_VENC_IDR_PERIOD:
		break;
	case HAL_CONFIG_VPE_OPERATIONS:
		break;
	case HAL_PARAM_VENC_INTRA_REFRESH:
		break;
	case HAL_PARAM_VENC_MULTI_SLICE_CONTROL:
		break;
	case HAL_CONFIG_VPE_DEINTERLACE:
		break;
	case HAL_SYS_DEBUG_CONFIG:
		break;
	/*FOLLOWING PROPERTIES ARE NOT IMPLEMENTED IN CORE YET*/
	case HAL_CONFIG_BUFFER_REQUIREMENTS:
	case HAL_CONFIG_PRIORITY:
	case HAL_CONFIG_BATCH_INFO:
	case HAL_PARAM_METADATA_PASS_THROUGH:
	case HAL_SYS_IDLE_INDICATOR:
	case HAL_PARAM_UNCOMPRESSED_FORMAT_SUPPORTED:
	case HAL_PARAM_INTERLACE_FORMAT_SUPPORTED:
	case HAL_PARAM_CHROMA_SITE:
	case HAL_PARAM_PROPERTIES_SUPPORTED:
	case HAL_PARAM_PROFILE_LEVEL_SUPPORTED:
	case HAL_PARAM_CAPABILITY_SUPPORTED:
	case HAL_PARAM_NAL_STREAM_FORMAT_SUPPORTED:
	case HAL_PARAM_MULTI_VIEW_FORMAT:
	case HAL_PARAM_MAX_SEQUENCE_HEADER_SIZE:
	case HAL_PARAM_CODEC_SUPPORTED:
	case HAL_PARAM_VDEC_MULTI_VIEW_SELECT:
	case HAL_PARAM_VDEC_MB_QUANTIZATION:
	case HAL_PARAM_VDEC_NUM_CONCEALED_MB:
	case HAL_PARAM_VDEC_H264_ENTROPY_SWITCHING:
	case HAL_PARAM_VENC_SLICE_DELIVERY_MODE:
	case HAL_PARAM_VENC_MPEG4_DATA_PARTITIONING:

	case HAL_CONFIG_BUFFER_COUNT_ACTUAL:
	case HAL_CONFIG_VDEC_MULTI_STREAM:
	case HAL_PARAM_VENC_MULTI_SLICE_INFO:
	case HAL_CONFIG_VENC_TIMESTAMP_SCALE:
	case HAL_PARAM_VENC_LOW_LATENCY:
	default:
		dprintk(VIDC_INFO, "DEFAULT: Calling 0x%x", ptype);
		break;
	}
	return 0;
}

static int q6_hfi_unset_ocmem(void *dev)
{
	(void)dev;

	/* Q6 does not support ocmem */
	return -EINVAL;
}

static int q6_hfi_iommu_get_domain_partition(void *dev, u32 flags,
	u32 buffer_type, int *domain, int *partition)
{
	(void)dev;

	dprintk(VIDC_ERR, "Not implemented: %s", __func__);

	return -ENOTSUPP;
}

static int q6_hfi_iommu_attach(struct q6_hfi_device *device)
{
	int rc = 0;
	struct iommu_domain *domain;
	int i;
	struct iommu_set *iommu_group_set;
	struct iommu_group *group;
	struct iommu_info *iommu_map;

	if (!device || !device->res) {
		dprintk(VIDC_ERR, "Invalid parameter: %p", device);
		return -EINVAL;
	}

	iommu_group_set = &device->res->iommu_group_set;
	for (i = 0; i < iommu_group_set->count; i++) {
		iommu_map = &iommu_group_set->iommu_maps[i];
		group = iommu_map->group;
		domain = msm_get_iommu_domain(iommu_map->domain);
		if (IS_ERR_OR_NULL(domain)) {
			dprintk(VIDC_ERR, "Failed to get domain: %s",
					iommu_map->name);
			rc = IS_ERR(domain) ? PTR_ERR(domain) : -EINVAL;
			break;
		}
		dprintk(VIDC_DBG, "Attaching domain(id:%d) %p to group %p",
				iommu_map->domain, domain, group);
		rc = iommu_attach_group(domain, group);
		if (rc) {
			dprintk(VIDC_ERR, "IOMMU attach failed: %s",
					iommu_map->name);
			break;
		}
	}
	if (i < iommu_group_set->count) {
		i--;
		for (; i >= 0; i--) {
			iommu_map = &iommu_group_set->iommu_maps[i];
			group = iommu_map->group;
			domain = msm_get_iommu_domain(iommu_map->domain);
			if (group && domain)
				iommu_detach_group(domain, group);
		}
	}
	return rc;
}

static void q6_hfi_iommu_detach(struct q6_hfi_device *device)
{
	struct iommu_group *group;
	struct iommu_domain *domain;
	struct iommu_set *iommu_group_set;
	struct iommu_info *iommu_map;
	int i;

	if (!device || !device->res) {
		dprintk(VIDC_ERR, "Invalid parameter: %p", device);
		return;
	}

	iommu_group_set = &device->res->iommu_group_set;
	for (i = 0; i < iommu_group_set->count; i++) {
		iommu_map = &iommu_group_set->iommu_maps[i];
		group = iommu_map->group;
		domain = msm_get_iommu_domain(iommu_map->domain);
		if (group && domain)
			iommu_detach_group(domain, group);
	}
}

static int q6_hfi_load_fw(void *dev)
{
	int rc = 0;
	struct q6_hfi_device *device = dev;

	if (!device)
		return -EINVAL;

	if (!device->resources.fw.cookie)
		device->resources.fw.cookie = subsystem_get("adsp");

	if (IS_ERR_OR_NULL(device->resources.fw.cookie)) {
		dprintk(VIDC_ERR, "Failed to download firmware\n");
		rc = -ENOMEM;
		goto fail_subsystem_get;
	}

	/*Set Q6 to loaded state*/
	apr_set_q6_state(APR_SUBSYS_LOADED);

	device->apr = apr_register("ADSP", "VIDC",
				(apr_fn)q6_hfi_apr_callback,
				0xFFFFFFFF,
				device);

	if (device->apr == NULL) {
		dprintk(VIDC_ERR, "Failed to register with QDSP6");
		rc = -EINVAL;
		goto fail_apr_register;
	}

	rc = q6_hfi_iommu_attach(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to attach iommu");
		goto fail_iommu_attach;
	}

	return rc;

fail_iommu_attach:
	apr_deregister(device->apr);
	device->apr = NULL;
fail_apr_register:
	subsystem_put(device->resources.fw.cookie);
	device->resources.fw.cookie = NULL;
fail_subsystem_get:
	return rc;
}

static void q6_hfi_unload_fw(void *hfi_device_data)
{
	struct q6_hfi_device *device = hfi_device_data;

	if (!device)
		return;

	if (device->resources.fw.cookie) {
		q6_hfi_iommu_detach(device);
		subsystem_put(device->resources.fw.cookie);
		device->resources.fw.cookie = NULL;
	}

	if (device->apr) {
		if (apr_deregister(device->apr))
			dprintk(VIDC_ERR, "Failed to deregister APR");
		device->apr = NULL;
	}
}

static int q6_hfi_get_stride_scanline(int color_fmt,
	int width, int height, int *stride, int *scanlines) {
	*stride = VENUS_Y_STRIDE(color_fmt, width);
	*scanlines = VENUS_Y_SCANLINES(color_fmt, height);
	return 0;
}

static void q6_init_hfi_callbacks(struct hfi_device *hdev)
{
	hdev->core_init = q6_hfi_core_init;
	hdev->core_release = q6_hfi_core_release;
	hdev->session_init = q6_hfi_session_init;
	hdev->session_end = q6_hfi_session_end;
	hdev->session_abort = q6_hfi_session_abort;
	hdev->session_clean = q6_hfi_session_clean;
	hdev->session_set_buffers = q6_hfi_session_set_buffers;
	hdev->session_release_buffers = q6_hfi_session_release_buffers;
	hdev->session_load_res = q6_hfi_session_load_res;
	hdev->session_release_res = q6_hfi_session_release_res;
	hdev->session_start = q6_hfi_session_start;
	hdev->session_stop = q6_hfi_session_stop;
	hdev->session_suspend = q6_hfi_session_suspend;
	hdev->session_resume = q6_hfi_session_resume;
	hdev->session_etb = q6_hfi_session_etb;
	hdev->session_ftb = q6_hfi_session_ftb;
	hdev->session_parse_seq_hdr = q6_hfi_session_parse_seq_hdr;
	hdev->session_get_seq_hdr = q6_hfi_session_get_seq_hdr;
	hdev->session_get_buf_req = q6_hfi_session_get_buf_req;
	hdev->session_flush = q6_hfi_session_flush;
	hdev->session_set_property = q6_hfi_session_set_property;
	hdev->session_get_property = q6_hfi_session_get_property;
	hdev->unset_ocmem = q6_hfi_unset_ocmem;
	hdev->iommu_get_domain_partition = q6_hfi_iommu_get_domain_partition;
	hdev->load_fw = q6_hfi_load_fw;
	hdev->unload_fw = q6_hfi_unload_fw;
	hdev->get_stride_scanline = q6_hfi_get_stride_scanline;
}


int q6_hfi_initialize(struct hfi_device *hdev, u32 device_id,
		struct msm_vidc_platform_resources *res,
		hfi_cmd_response_callback callback)
{
	int rc = 0;

	if (!hdev || !res || !callback) {
		dprintk(VIDC_ERR, "Invalid params: %p %p %p",
				hdev, res, callback);
		rc = -EINVAL;
		goto err_hfi_init;
	}
	hdev->hfi_device_data = q6_hfi_get_device(device_id, res, callback);

	q6_init_hfi_callbacks(hdev);

err_hfi_init:
	return rc;
}

