/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include "cam_req_mgr_interface.h"
#include "cam_req_mgr_util.h"
#include "cam_req_mgr_core.h"
#include "cam_req_mgr_workq.h"

/* Forward declarations */
static int cam_req_mgr_cb_notify_sof(
	struct cam_req_mgr_sof_notify *sof_data);


static struct cam_req_mgr_core_device *g_crm_core_dev;

static struct cam_req_mgr_crm_cb cam_req_mgr_ops = {
	.notify_sof = cam_req_mgr_cb_notify_sof,
	.notify_err = NULL,
	.add_req = NULL,
};

/**
 * cam_req_mgr_pvt_find_link()
 *
 * @brief: Finds link matching with handle within session
 * @session: session indetifier
 * @link_hdl: link handle
 *
 * Returns pointer to link matching handle
 */
static struct cam_req_mgr_core_link *cam_req_mgr_pvt_find_link(
	struct cam_req_mgr_core_session *session, int32_t link_hdl)
{
	int32_t i;
	struct cam_req_mgr_core_link *link = NULL;

	if (!session) {
		CRM_ERR("NULL session ptr");
		return NULL;
	}

	spin_lock(&session->lock);
	for (i = 0; i < MAX_LINKS_PER_SESSION; i++) {
		link = &session->links[i];
		spin_lock(&link->lock);
		if (link->link_hdl == link_hdl) {
			CRM_DBG("Link found p_delay %d",
				 link->max_pipeline_delay);
			spin_unlock(&link->lock);
			break;
		}
		spin_unlock(&link->lock);
	}
	if (i >= MAX_LINKS_PER_SESSION)
		link = NULL;
	spin_unlock(&session->lock);

	return link;
}

/**
 * cam_req_mgr_process_sof()
 *
 * @brief: This runs in workque thread context. Call core funcs to check
 * which peding requests can be processed.
 * @data:contains information about frame_id, link etc.
 *
 * Returns 0 on success.
 */
static int cam_req_mgr_process_sof(void *priv, void *data)
{
	int ret = 0, i = 0;
	struct cam_req_mgr_sof_notify *sof_data = NULL;
	struct cam_req_mgr_core_link *link = NULL;
	struct cam_req_mgr_connected_device *device = NULL;
	struct cam_req_mgr_apply_request apply_req;

	if (!data || !priv) {
		CRM_ERR("input args NULL %pK %pK", data, priv);
		ret = -EINVAL;
		goto end;
	}
	link = (struct cam_req_mgr_core_link *)priv;
	sof_data = (struct cam_req_mgr_sof_notify *)data;

	CRM_DBG("link_hdl %x frame_id %lld",
		sof_data->link_hdl,
		sof_data->frame_id);

	apply_req.link_hdl = sof_data->link_hdl;
	/* @TODO: go through request table and issue
	 * request id based on dev status
	 */
	apply_req.request_id = sof_data->frame_id;
	apply_req.report_if_bubble = 0;

	CRM_DBG("link %pK l_dev %pK num_dev %d",
		link, link->l_devices, link->num_connections);
	for (i = 0; i < link->num_connections; i++) {
		device = &link->l_devices[i];
		if (device != NULL) {
			CRM_DBG("dev_id %d dev_hdl %x ops %pK p_delay %d",
				device->dev_info.dev_id, device->dev_hdl,
				device->ops, device->dev_info.p_delay);
			apply_req.dev_hdl = device->dev_hdl;
			if (device->ops && device->ops->apply_req) {
				ret = device->ops->apply_req(&apply_req);
				/* Error handling for this failure is pending */
				if (ret < 0)
					CRM_ERR("Failure:%d dev=%d", ret,
						device->dev_info.dev_id);
			}

		}
	}

end:
	return ret;
}

/**
 * cam_req_mgr_notify_sof()
 *
 * @brief: SOF received from device, sends trigger through workqueue
 * @sof_data: contains information about frame_id, link etc.
 *
 * Returns 0 on success
 */
static int cam_req_mgr_cb_notify_sof(struct cam_req_mgr_sof_notify *sof_data)
{
	int                           ret = 0;
	struct crm_workq_task        *task = NULL;
	struct cam_req_mgr_core_link *link = NULL;

	if (!sof_data) {
		CRM_ERR("sof_data is NULL");
		ret = -EINVAL;
		goto end;
	}

	CRM_DBG("link_hdl %x frame_id %lld",
		sof_data->link_hdl,
		sof_data->frame_id);

	link = (struct cam_req_mgr_core_link *)
		cam_get_device_priv(sof_data->link_hdl);
	if (!link) {
		CRM_DBG("link ptr NULL %x", sof_data->link_hdl);
		ret = -EINVAL;
		goto end;

	}

	task = cam_req_mgr_workq_get_task(link->workq);
	if (!task) {
		CRM_ERR("no empty task frame %lld", sof_data->frame_id);
		ret = -EBUSY;
		goto end;
	}
	task->type = CRM_WORKQ_TASK_NOTIFY_SOF;
	task->u.notify_sof.frame_id = sof_data->frame_id;
	task->u.notify_sof.link_hdl = sof_data->link_hdl;
	task->u.notify_sof.dev_hdl = sof_data->dev_hdl;
	task->process_cb = &cam_req_mgr_process_sof;
	task->priv = link;
	cam_req_mgr_workq_enqueue_task(task);

end:
	return ret;
}

/**
 * cam_req_mgr_pvt_reserve_link()
 *
 * @brief: Reserves one link data struct within session
 * @session: session identifier
 *
 * Returns pointer to link reserved
 */
static struct cam_req_mgr_core_link *cam_req_mgr_pvt_reserve_link(
	struct cam_req_mgr_core_session *session)
{
	int32_t i;
	struct cam_req_mgr_core_link *link;

	if (!session) {
		CRM_ERR("NULL session ptr");
		return NULL;
	}

	spin_lock(&session->lock);
	for (i = 0; i < MAX_LINKS_PER_SESSION; i++) {
		link = &session->links[i];
		spin_lock(&link->lock);
		if (link->link_state == CAM_CRM_LINK_STATE_AVAILABLE) {
			link->num_connections = 0;
			link->max_pipeline_delay = 0;
			memset(link->req_table, 0,
				sizeof(struct cam_req_mgr_request_table));
			link->link_state = CAM_CRM_LINK_STATE_IDLE;
			spin_unlock(&link->lock);
			break;
		}
		spin_unlock(&link->lock);
	}
	CRM_DBG("Link available (total %d)", session->num_active_links);
	spin_unlock(&session->lock);

	if (i >= MAX_LINKS_PER_SESSION)
		link = NULL;

	return link;
}

/**
 * cam_req_mgr_pvt_create_subdevs()
 *
 * @brief: Create new crm  subdev to link with realtime devices
 * @l_devices: list of subdevs internal to crm
 * @num_dev: num of subdevs to be created for link
 *
 * Returns pointer to allocated list of devices
 */
static struct cam_req_mgr_connected_device *
	cam_req_mgr_pvt_create_subdevs(int32_t num_dev)
{
	struct cam_req_mgr_connected_device *l_devices;

	l_devices = (struct cam_req_mgr_connected_device *)
		kzalloc(sizeof(struct cam_req_mgr_connected_device) * num_dev,
		GFP_KERNEL);
	if (!l_devices)
		CRM_DBG("Insufficient memory %lu",
			sizeof(struct cam_req_mgr_connected_device) * num_dev);

	return l_devices;
}

/**
 * cam_req_mgr_pvt_destroy_subdev()
 *
 * @brief: Cleans up the subdevs allocated by crm for link
 * @l_device: pointer to list of subdevs crm created
 *
 * Returns 0 for success
 */
static int cam_req_mgr_pvt_destroy_subdev(
	struct cam_req_mgr_connected_device **l_device)
{
	int ret = 0;

	if (!(*l_device))
		ret = -EINVAL;
	else {
		kfree(*l_device);
		*l_device = NULL;
	}

	return ret;
}

int cam_req_mgr_create_session(
	struct cam_req_mgr_session_info *ses_info)
{
	int ret = 0;
	int32_t i;
	int32_t session_hdl;
	struct cam_req_mgr_core_session *cam_session;

	if (!ses_info) {
		CRM_ERR("NULL session info pointer");
		return -EINVAL;
	}
	mutex_lock(&g_crm_core_dev->crm_lock);
	cam_session = (struct cam_req_mgr_core_session *)
		kzalloc(sizeof(*cam_session), GFP_KERNEL);
	if (!cam_session) {
		ret = -ENOMEM;
		goto end;
	}

	session_hdl = cam_create_session_hdl((void *)cam_session);
	if (session_hdl < 0) {
		CRM_ERR("unable to create session_hdl = %x", session_hdl);
		ret = session_hdl;
		goto session_hdl_failed;
	}
	ses_info->session_hdl = session_hdl;
	cam_session->session_hdl = session_hdl;

	spin_lock_init(&cam_session->lock);
	cam_session->num_active_links = 0;

	for (i = 0; i < MAX_LINKS_PER_SESSION; i++) {
		spin_lock_init(&cam_session->links[i].lock);
		cam_session->links[i].link_state = CAM_CRM_LINK_STATE_AVAILABLE;
		INIT_LIST_HEAD(&cam_session->links[i].link_head);
		cam_session->links[i].workq = NULL;
	}
	list_add(&cam_session->entry, &g_crm_core_dev->session_head);

	mutex_unlock(&g_crm_core_dev->crm_lock);
	return ret;

session_hdl_failed:
	kfree(cam_session);
end:
	mutex_unlock(&g_crm_core_dev->crm_lock);
	return ret;
}

int cam_req_mgr_destroy_session(
		struct cam_req_mgr_session_info *ses_info)
{
	int ret;
	int32_t i;
	struct cam_req_mgr_core_session *cam_session;
	struct cam_req_mgr_core_link *link = NULL;

	if (!ses_info) {
		CRM_ERR("NULL session info pointer");
		return -EINVAL;
	}

	mutex_lock(&g_crm_core_dev->crm_lock);
	cam_session = (struct cam_req_mgr_core_session *)
		cam_get_device_priv(ses_info->session_hdl);
	if (cam_session == NULL) {
		CRM_ERR("failed to get session priv");
		ret = -ENOENT;
		goto end;

	}
	spin_lock(&cam_session->lock);
	for (i = 0; i < cam_session->num_active_links; i++) {
		link = &cam_session->links[i];
		CRM_ERR("session %x active_links %d hdl %x connections %d",
			ses_info->session_hdl,
			cam_session->num_active_links,
			link->link_hdl, link->num_connections);
	}
	list_del(&cam_session->entry);
	spin_unlock(&cam_session->lock);
	kfree(cam_session);

	ret = cam_destroy_session_hdl(ses_info->session_hdl);
	if (ret)
		CRM_ERR("unable to destroy session_hdl = %x ret %d",
			ses_info->session_hdl, ret);

end:
	mutex_unlock(&g_crm_core_dev->crm_lock);
	return ret;

}

int cam_req_mgr_link(struct cam_req_mgr_link_info *link_info)
{
	int ret = 0;
	int32_t i, link_hdl;
	char buf[128];
	struct cam_create_dev_hdl root_dev;
	struct cam_req_mgr_core_session *cam_session;
	struct cam_req_mgr_core_link *link;
	struct cam_req_mgr_core_dev_link_setup link_data;
	struct cam_req_mgr_connected_device *l_devices;
	enum cam_pipeline_delay max_delay = CAM_PIPELINE_DELAY_0;

	if (!link_info) {
		CRM_ERR("NULL pointer");
		return -EINVAL;
	}

	if (link_info->num_devices > CAM_REQ_MGR_MAX_HANDLES) {
		CRM_ERR("Invalid num devices %d", link_info->num_devices);
		return -EINVAL;
	}

	cam_session = (struct cam_req_mgr_core_session *)
		cam_get_device_priv(link_info->session_hdl);
	if (!cam_session) {
		CRM_ERR("NULL session pointer");
		return -EINVAL;
	}

	link = cam_req_mgr_pvt_reserve_link(cam_session);
	if (!link) {
		CRM_ERR("NULL link pointer");
		return -EINVAL;
	}

	memset(&root_dev, 0, sizeof(struct cam_create_dev_hdl));
	root_dev.session_hdl = link_info->session_hdl;
	root_dev.priv = (void *)link;

	link_hdl = cam_create_device_hdl(&root_dev);
	if (link_hdl < 0) {
		CRM_ERR("Insufficient memory to create new device handle");
		ret = link_hdl;
		goto link_hdl_fail;
	}

	l_devices = cam_req_mgr_pvt_create_subdevs(link_info->num_devices);
	if (!l_devices) {
		ret = -ENOMEM;
		goto create_subdev_failed;
	}

	for (i = 0; i < link_info->num_devices; i++) {
		l_devices[i].dev_hdl = link_info->dev_hdls[i];
		l_devices[i].parent = (void *)link;
		l_devices[i].ops = (struct cam_req_mgr_kmd_ops *)
			cam_get_device_ops(link_info->dev_hdls[i]);
		link_data.dev_hdl = l_devices[i].dev_hdl;
		l_devices[i].dev_info.dev_hdl = l_devices[i].dev_hdl;
		if (l_devices[i].ops) {
			if (l_devices[i].ops->get_dev_info) {
				ret = l_devices[i].ops->get_dev_info(
					&l_devices[i].dev_info);
				if (ret < 0 ||
					l_devices[i].dev_info.p_delay >=
					CAM_PIPELINE_DELAY_MAX ||
					l_devices[i].dev_info.p_delay <
					CAM_PIPELINE_DELAY_0) {
					CRM_ERR("get device info failed");
					goto error;
				} else {
					CRM_DBG("%x: connected: %s, delay %d",
						link_info->session_hdl,
						l_devices[i].dev_info.name,
						l_devices[i].dev_info.p_delay);
					if (l_devices[i].dev_info.p_delay >
						max_delay)
					max_delay =
						l_devices[i].dev_info.p_delay;
				}
			}
		} else {
			CRM_ERR("FATAL: device ops NULL");
			ret = -ENXIO;
			goto error;
		}
	}

	link_data.link_enable = true;
	link_data.link_hdl = link_hdl;
	link_data.crm_cb = &cam_req_mgr_ops;
	link_data.max_delay = max_delay;

	/* After getting info about all devices, establish link */
	for (i = 0; i < link_info->num_devices; i++) {
		l_devices[i].dev_hdl = link_info->dev_hdls[i];
		l_devices[i].parent = (void *)link;
		l_devices[i].ops = (struct cam_req_mgr_kmd_ops *)
			cam_get_device_ops(link_info->dev_hdls[i]);
		link_data.dev_hdl = l_devices[i].dev_hdl;
		l_devices[i].dev_info.dev_hdl = l_devices[i].dev_hdl;
		if (l_devices[i].ops) {
			if (l_devices[i].ops->link_setup) {
				ret = l_devices[i].ops->link_setup(&link_data);
				if (ret < 0) {
					/* TODO check handlng of this failure */
					CRM_ERR("link setup failed");
					goto error;
				}
			}
		}
		list_add_tail(&l_devices[i].entry, &link->link_head);
	}

	/* Create worker for current link */
	snprintf(buf, sizeof(buf), "%x-%x", link_info->session_hdl, link_hdl);
	ret = cam_req_mgr_workq_create(buf, &link->workq);
	if (ret < 0) {
		CRM_ERR("FATAL: unable to create worker");
		goto error;
	}

	link_info->link_hdl = link_hdl;
	spin_lock(&link->lock);
	link->l_devices = l_devices;
	link->link_hdl = link_hdl;
	link->parent = (void *)cam_session;
	link->num_connections = link_info->num_devices;
	link->link_state = CAM_CRM_LINK_STATE_READY;
	spin_unlock(&link->lock);

	spin_lock(&cam_session->lock);
	cam_session->num_active_links++;
	spin_unlock(&cam_session->lock);

	return ret;

error:
	cam_req_mgr_pvt_destroy_subdev(&l_devices);
create_subdev_failed:
	cam_destroy_device_hdl(link_hdl);
link_hdl_fail:
	spin_lock(&link->lock);
	link->link_state = CAM_CRM_LINK_STATE_AVAILABLE;
	spin_unlock(&link->lock);

	return ret;
}

int cam_req_mgr_unlink(struct cam_req_mgr_unlink_info *unlink_info)
{
	int ret = 0;
	int32_t i = 0;
	struct cam_req_mgr_core_session *cam_session;
	struct cam_req_mgr_core_link *link;
	struct cam_req_mgr_connected_device *device;
	struct cam_req_mgr_core_dev_link_setup link_data;

	if (!unlink_info) {
		CRM_ERR("NULL pointer");
		return -EINVAL;
	}
	cam_session = (struct cam_req_mgr_core_session *)
	cam_get_device_priv(unlink_info->session_hdl);
	if (!cam_session) {
		CRM_ERR("NULL pointer");
		return -EINVAL;
	}

	link = cam_req_mgr_pvt_find_link(cam_session,
		unlink_info->link_hdl);
	if (!link) {
		CRM_ERR("NULL pointer");
		return -EINVAL;
	}

	ret = cam_destroy_device_hdl(link->link_hdl);
	if (ret < 0) {
		CRM_ERR("error in destroying dev handle %d %x",
			ret, link->link_hdl);
		ret = -EINVAL;
	}
	link_data.link_enable = false;
	link_data.link_hdl = link->link_hdl;
	link_data.crm_cb = NULL;
	for (i = 0; i < link->num_connections; i++) {
		device = &link->l_devices[i];
		link_data.dev_hdl = device->dev_hdl;
		if (device->ops && device->ops->link_setup)
			device->ops->link_setup(&link_data);
		device->dev_hdl = 0;
		device->parent = NULL;
		device->ops = NULL;
		list_del(&device->entry);
	}
	/* Destroy worker of link */
	cam_req_mgr_workq_destroy(link->workq);
	spin_lock(&link->lock);
	link->link_state = CAM_CRM_LINK_STATE_AVAILABLE;
	link->parent = NULL;
	link->num_connections = 0;
	link->link_hdl = 0;
	link->workq = NULL;
	spin_unlock(&link->lock);

	spin_lock(&cam_session->lock);
	cam_session->num_active_links--;
	spin_unlock(&cam_session->lock);

	ret = cam_req_mgr_pvt_destroy_subdev(&link->l_devices);
	if (ret < 0) {
		CRM_ERR("error while destroying subdev link %x",
			link_data.link_hdl);
		ret = -EINVAL;
	}

	return ret;
}

int cam_req_mgr_schedule_request(
			struct cam_req_mgr_sched_request *sched_req)
{
	if (!sched_req) {
		CRM_ERR("NULL pointer");
		return -EINVAL;
	}

	/* This function handles ioctl, implementation pending */
	return 0;
}

int cam_req_mgr_sync_mode(
			struct cam_req_mgr_sync_mode *sync_links)
{
	if (!sync_links) {
		CRM_ERR("NULL pointer");
		return -EINVAL;
	}

	/* This function handles ioctl, implementation pending */
	return 0;
}

int cam_req_mgr_flush_requests(
			struct cam_req_mgr_flush_info *flush_info)
{
	if (!flush_info) {
		CRM_ERR("NULL pointer");
		return -EINVAL;
	}

	/* This function handles ioctl, implementation pending */
	return 0;
}


int cam_req_mgr_core_device_init(void)
{
	CRM_DBG("Enter g_crm_core_dev %pK", g_crm_core_dev);

	if (g_crm_core_dev) {
		CRM_WARN("core device is already initialized");
		return 0;
	}
	g_crm_core_dev = (struct cam_req_mgr_core_device *)
		kzalloc(sizeof(*g_crm_core_dev), GFP_KERNEL);
	if (!g_crm_core_dev)
		return -ENOMEM;

	CRM_DBG("g_crm_core_dev %pK", g_crm_core_dev);
	INIT_LIST_HEAD(&g_crm_core_dev->session_head);
	mutex_init(&g_crm_core_dev->crm_lock);

	return 0;
}

int cam_req_mgr_core_device_deinit(void)
{
	if (!g_crm_core_dev) {
		CRM_ERR("NULL pointer");
		return -EINVAL;
	}

	CRM_DBG("g_crm_core_dev %pK", g_crm_core_dev);
	mutex_destroy(&g_crm_core_dev->crm_lock);
	kfree(g_crm_core_dev);
	g_crm_core_dev = NULL;

	return 0;
}
