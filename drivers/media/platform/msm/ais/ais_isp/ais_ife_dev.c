/* Copyright (c) 2017-2018, 2020, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/ion.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>

#include <uapi/media/ais_isp.h>
#include <uapi/media/cam_req_mgr.h>
#include "ais_ife_dev.h"
#include "ais_vfe_hw_intf.h"
#include "ais_ife_csid_hw_intf.h"
#include "cam_node.h"
#include "cam_debug_util.h"
#include "cam_smmu_api.h"

#define AIS_IFE_SUBDEVICE_EVENT_MAX 30

static int ais_ife_driver_cmd(struct ais_ife_dev *p_ife_dev, void *arg);
static int ais_ife_init_subdev_params(struct ais_ife_dev *p_ife_dev);

static int ais_ife_subdev_subscribe_event(struct v4l2_subdev *sd,
	struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	return v4l2_event_subscribe(fh, sub, AIS_IFE_SUBDEVICE_EVENT_MAX, NULL);
}

static int ais_ife_subdev_unsubscribe_event(struct v4l2_subdev *sd,
	struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	return v4l2_event_unsubscribe(fh, sub);
}

static long ais_ife_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int rc = 0;

	struct ais_ife_dev *p_ife_dev = v4l2_get_subdevdata(sd);


	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = ais_ife_driver_cmd(p_ife_dev, arg);
		break;
	default:
		CAM_ERR(CAM_ISP, "Invalid ioctl cmd: %d", cmd);
		rc = -EINVAL;
		break;
	}
	return rc;
}

#ifdef CONFIG_COMPAT
static long ais_ife_subdev_ioctl_compat(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	struct cam_control cmd_data;
	int32_t rc = 0;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_SENSOR, "Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = ais_ife_subdev_ioctl(sd, cmd, &cmd_data);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "cam_sensor_subdev_ioctl failed");
			break;
	default:
		CAM_ERR(CAM_SENSOR, "Invalid compat ioctl cmd_type: %d", cmd);
		rc = -EINVAL;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_SENSOR,
				"Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}

	return rc;
}
#endif

static void ais_ife_dev_get_hw_caps(struct ais_ife_dev *p_ife_dev,
		struct cam_isp_query_cap_cmd *query_isp)
{
	int i;

	query_isp->device_iommu.non_secure = p_ife_dev->iommu_hdl;
	query_isp->device_iommu.secure = p_ife_dev->iommu_hdl_secure;
	query_isp->reserved = p_ife_dev->hw_idx;
	query_isp->num_dev = 1;
	for (i = 0; i < query_isp->num_dev; i++) {
		query_isp->dev_caps[i].hw_type = CAM_ISP_HW_IFE;
		query_isp->dev_caps[i].hw_version.major = 1;
		query_isp->dev_caps[i].hw_version.minor = 7;
		query_isp->dev_caps[i].hw_version.incr = 0;
		query_isp->dev_caps[i].hw_version.reserved = 0;
	}
}

static int ais_ife_cmd_reserve(struct ais_ife_dev *p_ife_dev,
	struct ais_ife_rdi_init_args *rdi_init,
	uint32_t cmd_size)
{
	int rc;
	struct cam_hw_intf *csid_drv;
	struct cam_hw_intf *vfe_drv;
	struct ais_ife_rdi_deinit_args rdi_deinit;

	csid_drv = p_ife_dev->p_csid_drv;
	vfe_drv = p_ife_dev->p_vfe_drv;
	rdi_deinit.path = rdi_init->path;

	rc = csid_drv->hw_ops.init(
		csid_drv->hw_priv, rdi_init, cmd_size);
	if (rc)
		goto fail_csid_init;

	rc = vfe_drv->hw_ops.init(
			vfe_drv->hw_priv, rdi_init, cmd_size);
	if (rc)
		goto fail_vfe_init;

	rc = csid_drv->hw_ops.reserve(
			csid_drv->hw_priv, rdi_init, cmd_size);
	if (rc)
		goto fail_csid_reserve;

	rc = vfe_drv->hw_ops.reserve(
			vfe_drv->hw_priv, rdi_init, cmd_size);
	if (rc)
		goto fail_vfe_reserve;

	return rc;

fail_vfe_reserve:
	csid_drv->hw_ops.release(
		csid_drv->hw_priv, &rdi_deinit, sizeof(rdi_deinit));
fail_csid_reserve:
	vfe_drv->hw_ops.deinit(
		vfe_drv->hw_priv, &rdi_deinit, sizeof(rdi_deinit));
fail_vfe_init:
	csid_drv->hw_ops.deinit(
		csid_drv->hw_priv, &rdi_deinit, sizeof(rdi_deinit));
fail_csid_init:
	return rc;
}

static int ais_ife_cmd_release(struct ais_ife_dev *p_ife_dev,
	struct ais_ife_rdi_deinit_args *rdi_deinit,
	uint32_t cmd_size)
{
	int rc;
	int tmp;
	struct cam_hw_intf *csid_drv;
	struct cam_hw_intf *vfe_drv;

	csid_drv = p_ife_dev->p_csid_drv;
	vfe_drv = p_ife_dev->p_vfe_drv;

	rc = csid_drv->hw_ops.release(
		csid_drv->hw_priv, rdi_deinit, cmd_size);

	tmp = vfe_drv->hw_ops.release(
		vfe_drv->hw_priv, rdi_deinit, cmd_size);
	if (!rc)
		rc = tmp;

	tmp = csid_drv->hw_ops.deinit(
		csid_drv->hw_priv, rdi_deinit, cmd_size);
	if (!rc)
		rc = tmp;

	tmp = vfe_drv->hw_ops.deinit(
		vfe_drv->hw_priv, rdi_deinit, cmd_size);
	if (!rc)
		rc = tmp;

	return rc;
}

static int ais_ife_driver_cmd(struct ais_ife_dev *p_ife_dev, void *arg)
{
	int rc = 0;
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_hw_intf *csid_drv;
	struct cam_hw_intf *vfe_drv;

	if (!p_ife_dev || !arg) {
		CAM_ERR(CAM_SENSOR, "s_ctrl is NULL");
		rc = -EINVAL;
		goto EXIT;
	}

	if (cmd->handle_type != AIS_ISP_CMD_TYPE) {
		CAM_ERR(CAM_SENSOR, "Invalid handle type 0x%x",
			cmd->handle_type);
		rc = -EINVAL;
		goto EXIT;
	}

	csid_drv = p_ife_dev->p_csid_drv;
	vfe_drv = p_ife_dev->p_vfe_drv;

	CAM_DBG(CAM_ISP, "CMD %d", cmd->op_code);

	mutex_lock(&p_ife_dev->mutex);
	switch (cmd->op_code) {
	case AIS_IFE_QUERY_CAPS: {
		struct cam_isp_query_cap_cmd query_isp;

		if (cmd->size != sizeof(query_isp)) {
			rc = -EINVAL;
		} else if (copy_from_user(&query_isp,
				u64_to_user_ptr(cmd->handle),
				cmd->size)) {
			rc = -EFAULT;
		} else {
			ais_ife_dev_get_hw_caps(p_ife_dev, &query_isp);

			if (copy_to_user(u64_to_user_ptr(cmd->handle),
				&query_isp,
				cmd->size))
				rc = -EFAULT;
		}
	}
		break;
	case AIS_IFE_POWER_UP: {
	}
		break;
	case AIS_IFE_POWER_DOWN: {
	}
		break;
	case AIS_IFE_RESET: {
		int tmp;

		rc = p_ife_dev->p_csid_drv->hw_ops.reset(
				p_ife_dev->p_csid_drv->hw_priv, NULL, 0);
		tmp = p_ife_dev->p_vfe_drv->hw_ops.reset(
				p_ife_dev->p_vfe_drv->hw_priv, NULL, 0);
		if (!rc)
			rc = tmp;
	}
		break;
	case AIS_IFE_RESERVE: {
		struct ais_ife_rdi_init_args rdi_init;

		if (cmd->size != sizeof(rdi_init)) {
			CAM_ERR(CAM_ISP, "Invalid cmd size");
			rc = -EINVAL;
		} else if (copy_from_user(&rdi_init,
				u64_to_user_ptr(cmd->handle),
				cmd->size)) {
			rc = -EFAULT;
		} else {
			rc = ais_ife_cmd_reserve(p_ife_dev,
				&rdi_init, cmd->size);
		}
	}
		break;
	case AIS_IFE_RELEASE: {
		struct ais_ife_rdi_deinit_args rdi_deinit;

		if (cmd->size != sizeof(rdi_deinit)) {
			CAM_ERR(CAM_ISP, "Invalid cmd size");
			rc = -EINVAL;
		} else if (copy_from_user(&rdi_deinit,
				u64_to_user_ptr(cmd->handle),
				cmd->size)) {
			rc = -EFAULT;
		} else {
			rc = ais_ife_cmd_release(p_ife_dev,
					&rdi_deinit, cmd->size);
		}
	}
		break;
	case AIS_IFE_START: {
		struct ais_ife_rdi_start_args rdi_start;

		if (cmd->size != sizeof(rdi_start)) {
			CAM_ERR(CAM_ISP, "Invalid cmd size");
			rc = -EINVAL;
		} else if (copy_from_user(&rdi_start,
				u64_to_user_ptr(cmd->handle),
				cmd->size)) {
			rc = -EFAULT;
		} else {
			rc = vfe_drv->hw_ops.start(vfe_drv->hw_priv,
				&rdi_start, cmd->size);
			if (!rc) {
				rc = csid_drv->hw_ops.start(
					csid_drv->hw_priv, &rdi_start,
					cmd->size);
				if (rc) {
					struct ais_ife_rdi_stop_args rdi_stop;

					rdi_stop.path = rdi_start.path;
					vfe_drv->hw_ops.stop(vfe_drv->hw_priv,
						&rdi_stop, sizeof(rdi_stop));
				}
			}
		}
	}
		break;
	case AIS_IFE_STOP: {
		struct ais_ife_rdi_stop_args rdi_stop;

		if (cmd->size != sizeof(rdi_stop)) {
			CAM_ERR(CAM_ISP, "Invalid cmd size");
			rc = -EINVAL;
		} else if (copy_from_user(&rdi_stop,
				u64_to_user_ptr(cmd->handle),
				cmd->size)) {
			rc = -EFAULT;
		} else {
			int tmp;

			rc = csid_drv->hw_ops.stop(
				csid_drv->hw_priv, &rdi_stop, cmd->size);
			tmp = vfe_drv->hw_ops.stop(
				vfe_drv->hw_priv, &rdi_stop, cmd->size);
			if (!rc)
				rc = tmp;
		}
	}
		break;
	case AIS_IFE_PAUSE: {
		struct ais_ife_rdi_stop_args rdi_stop;

		if (cmd->size != sizeof(rdi_stop)) {
			CAM_ERR(CAM_ISP, "Invalid cmd size");
			rc = -EINVAL;
		} else if (copy_from_user(&rdi_stop,
				u64_to_user_ptr(cmd->handle),
				cmd->size)) {
			rc = -EFAULT;
		} else {
			rc = vfe_drv->hw_ops.stop(
				vfe_drv->hw_priv, &rdi_stop, cmd->size);
		}
	}
		break;
	case AIS_IFE_RESUME: {
		struct ais_ife_rdi_start_args rdi_start;

		if (cmd->size != sizeof(rdi_start)) {
			CAM_ERR(CAM_ISP, "Invalid cmd size");
			rc = -EINVAL;
		} else if (copy_from_user(&rdi_start,
				u64_to_user_ptr(cmd->handle),
				cmd->size)) {
			rc = -EFAULT;
		} else {
			rc = vfe_drv->hw_ops.start(
				vfe_drv->hw_priv, &rdi_start, cmd->size);
		}
	}
		break;
	case AIS_IFE_BUFFER_ENQ: {
		struct ais_ife_enqueue_buffer_args enq_buf;

		if (cmd->size != sizeof(enq_buf)) {
			CAM_ERR(CAM_ISP, "Invalid cmd size");
			rc = -EINVAL;
		} else if (copy_from_user(&enq_buf,
				u64_to_user_ptr(cmd->handle),
				cmd->size)) {
			rc = -EFAULT;
		} else {
			rc = vfe_drv->hw_ops.process_cmd(vfe_drv->hw_priv,
					AIS_VFE_CMD_ENQ_BUFFER, &enq_buf,
					cmd->size);
		}
	}
		break;
	default:
		rc = -EINVAL;
		break;
	}

	mutex_unlock(&p_ife_dev->mutex);
EXIT:
	return rc;
}

static int ais_ife_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct ais_ife_dev *p_ife_dev = v4l2_get_subdevdata(sd);

	mutex_lock(&p_ife_dev->mutex);
	p_ife_dev->open_cnt++;
	mutex_unlock(&p_ife_dev->mutex);

	return 0;
}

static int ais_ife_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct ais_ife_dev *p_ife_dev = v4l2_get_subdevdata(sd);

	CAM_INFO(CAM_ISP, "IFE%d close", p_ife_dev->hw_idx);

	mutex_lock(&p_ife_dev->mutex);
	if (p_ife_dev->open_cnt <= 0) {
		CAM_ERR(CAM_ISP, "IFE device is already closed");
		rc = -EINVAL;
		goto end;
	}

	p_ife_dev->open_cnt--;

	/*reset to shutdown vfe and csid*/
	if (p_ife_dev->open_cnt == 0) {
		CAM_DBG(CAM_ISP, "IFE%d shutdown", p_ife_dev->hw_idx);
		p_ife_dev->p_csid_drv->hw_ops.reset(
			p_ife_dev->p_csid_drv->hw_priv, NULL, 0);
		p_ife_dev->p_vfe_drv->hw_ops.reset(
			p_ife_dev->p_vfe_drv->hw_priv, NULL, 0);
		CAM_INFO(CAM_ISP, "IFE%d shutdown complete", p_ife_dev->hw_idx);
	}

end:
	mutex_unlock(&p_ife_dev->mutex);
	return rc;
}

static int ais_ife_dev_cb(void *priv, struct ais_ife_event_data *evt_data)
{
	struct ais_ife_dev  *p_ife_dev;
	struct v4l2_event event = {};

	p_ife_dev = (struct ais_ife_dev *)priv;

	if (!evt_data) {
		CAM_ERR(CAM_ISP, "IFE%d callback with NULL event",
			p_ife_dev->hw_idx);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "IFE%d CALLBACK %d",
		p_ife_dev->hw_idx, evt_data->type);


	if (sizeof(*evt_data) > sizeof(event.u.data)) {
		CAM_ERR(CAM_ISP, "IFE Callback struct too large (%d)!",
			sizeof(*evt_data));
		return -EINVAL;
	}

	/* Queue the event */
	memcpy(event.u.data, (void *)evt_data, sizeof(*evt_data));
	event.id = V4L_EVENT_ID_AIS_IFE;
	event.type = V4L_EVENT_TYPE_AIS_IFE;
	v4l2_event_queue(p_ife_dev->cam_sd.sd.devnode, &event);

	return 0;
}

static void ais_ife_dev_iommu_fault_handler(
	struct iommu_domain *domain, struct device *dev, unsigned long iova,
	int flags, void *token, uint32_t buf_info)
{
	struct ais_ife_dev *p_ife_dev = NULL;

	if (!token) {
		CAM_ERR(CAM_ISP, "invalid token in page handler cb");
		return;
	}

	p_ife_dev = (struct ais_ife_dev *)token;

	CAM_ERR(CAM_ISP, "IFE%d Pagefault at iova 0x%x %s",
		p_ife_dev->hw_idx, iova, domain->name);
}

static int ais_ife_dev_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct ais_ife_dev  *p_ife_dev;

	p_ife_dev = platform_get_drvdata(pdev);
	if (!p_ife_dev) {
		CAM_ERR(CAM_ISP, "IFE device is NULL");
		return 0;
	}

	/* clean up resources */
	cam_unregister_subdev(&(p_ife_dev->cam_sd));
	platform_set_drvdata(pdev, NULL);
	v4l2_set_subdevdata(&(p_ife_dev->cam_sd.sd), NULL);
	devm_kfree(&pdev->dev, p_ife_dev);

	return rc;
}

static int ais_ife_dev_probe(struct platform_device *pdev)
{
	int rc = -1;
	struct ais_ife_dev *p_ife_dev = NULL;
	struct ais_isp_hw_init_args hw_init = {};

	/* Create IFE control structure */
	p_ife_dev = devm_kzalloc(&pdev->dev,
		sizeof(struct ais_ife_dev), GFP_KERNEL);
	if (!p_ife_dev)
		return -ENOMEM;

	rc = of_property_read_u32(pdev->dev.of_node,
		"cell-index", &p_ife_dev->hw_idx);
	if (rc) {
		CAM_ERR(CAM_ISP, "IFE failed to read cell-index");
		return rc;
	}

	/* Initialze the v4l2 subdevice and register with cam_node */
	rc = ais_ife_init_subdev_params(p_ife_dev);
	if (rc) {
		CAM_ERR(CAM_ISP, "IFE%d init subdev failed!",
			p_ife_dev->hw_idx);
		goto err;
	}

	mutex_init(&p_ife_dev->mutex);

	/*
	 *  for now, we only support one iommu handle. later
	 *  we will need to setup more iommu handle for other
	 *  use cases.
	 *  Also, we have to release them once we have the
	 *  deinit support
	 */
	cam_smmu_get_handle("ife", &p_ife_dev->iommu_hdl);
	if (rc) {
		CAM_ERR(CAM_ISP, "Can not get iommu handle");
		rc = -EINVAL;
		goto unregister;
	}

	rc = cam_smmu_ops(p_ife_dev->iommu_hdl, CAM_SMMU_ATTACH);
	if (rc && rc != -EALREADY) {
		CAM_ERR(CAM_ISP, "Attach iommu handle failed %d", rc);
		goto attach_fail;
	}

	rc = cam_smmu_get_handle("ife-cp",
		&p_ife_dev->iommu_hdl_secure);
	if (rc) {
		CAM_ERR(CAM_ISP, "Failed to get secure iommu handle %d", rc);
		goto secure_fail;
	}

	rc = cam_smmu_ops(p_ife_dev->iommu_hdl_secure, CAM_SMMU_ATTACH);
	if (rc && rc != -EALREADY) {
		CAM_ERR(CAM_ISP, "Attach secure iommu handle failed %d", rc);
		goto secure_fail;
	}

	CAM_DBG(CAM_ISP, "iommu_handles: non-secure[0x%x], secure[0x%x]",
		p_ife_dev->iommu_hdl,
		p_ife_dev->iommu_hdl_secure);

	cam_smmu_set_client_page_fault_handler(p_ife_dev->iommu_hdl,
			ais_ife_dev_iommu_fault_handler, p_ife_dev);

	cam_smmu_set_client_page_fault_handler(p_ife_dev->iommu_hdl_secure,
			ais_ife_dev_iommu_fault_handler, p_ife_dev);

	hw_init.hw_idx = p_ife_dev->hw_idx;
	hw_init.iommu_hdl = p_ife_dev->iommu_hdl;
	hw_init.iommu_hdl_secure = p_ife_dev->iommu_hdl_secure;
	hw_init.event_cb = &ais_ife_dev_cb;
	hw_init.event_cb_priv = p_ife_dev;

	rc = ais_ife_csid_hw_init(&p_ife_dev->p_csid_drv, &hw_init);
	if (rc) {
		CAM_ERR(CAM_ISP, "IFE%d no CSID dev", p_ife_dev->hw_idx, rc);
		goto secure_attach_fail;
	}

	rc = ais_vfe_hw_init(&p_ife_dev->p_vfe_drv, &hw_init,
			p_ife_dev->p_csid_drv);
	if (rc) {
		CAM_ERR(CAM_ISP, "IFE%d no VFE dev", p_ife_dev->hw_idx, rc);
		goto secure_attach_fail;
	}

	CAM_INFO(CAM_ISP, "IFE%d probe complete", p_ife_dev->hw_idx);

	platform_set_drvdata(pdev, p_ife_dev);

	return 0;

secure_attach_fail:
	cam_smmu_ops(p_ife_dev->iommu_hdl_secure,
		CAM_SMMU_DETACH);
secure_fail:
	cam_smmu_ops(p_ife_dev->iommu_hdl,
		CAM_SMMU_DETACH);
attach_fail:
	cam_smmu_destroy_handle(p_ife_dev->iommu_hdl);
	p_ife_dev->iommu_hdl = -1;
unregister:
	cam_unregister_subdev(&(p_ife_dev->cam_sd));
err:
	return rc;
}

static struct v4l2_subdev_core_ops ais_ife_subdev_core_ops = {
	.ioctl = ais_ife_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = ais_ife_subdev_ioctl_compat,
#endif
	.subscribe_event = ais_ife_subdev_subscribe_event,
	.unsubscribe_event = ais_ife_subdev_unsubscribe_event,
};

static struct v4l2_subdev_ops ais_ife_subdev_ops = {
	.core = &ais_ife_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops ais_ife_internal_ops = {
	.open = ais_ife_subdev_open,
	.close = ais_ife_subdev_close,
};

static int ais_ife_init_subdev_params(struct ais_ife_dev *p_ife_dev)
{
	int rc = 0;

	p_ife_dev->cam_sd.internal_ops =
		&ais_ife_internal_ops;
	p_ife_dev->cam_sd.ops =
		&ais_ife_subdev_ops;
	strlcpy(p_ife_dev->device_name, AIS_IFE_DEV_NAME,
		sizeof(p_ife_dev->device_name));
	p_ife_dev->cam_sd.name =
		p_ife_dev->device_name;
	p_ife_dev->cam_sd.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	p_ife_dev->cam_sd.ent_function =
			AIS_IFE_DEVICE_TYPE;
	p_ife_dev->cam_sd.token = p_ife_dev;

	rc = cam_register_subdev(&(p_ife_dev->cam_sd));
	if (rc)
		CAM_ERR(CAM_ISP, "Fail with cam_register_subdev rc: %d", rc);

	return rc;
}

static const struct of_device_id ais_ife_dt_match[] = {
	{
		.compatible = "qcom,ais-ife"
	},
	{}
};

static struct platform_driver ife_driver = {
	.probe = ais_ife_dev_probe,
	.remove = ais_ife_dev_remove,
	.driver = {
		.name = "ais_ife",
		.owner = THIS_MODULE,
		.of_match_table = ais_ife_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init ais_ife_dev_init_module(void)
{
	return platform_driver_register(&ife_driver);
}

static void __exit ais_ife_dev_exit_module(void)
{
	platform_driver_unregister(&ife_driver);
}

module_init(ais_ife_dev_init_module);
module_exit(ais_ife_dev_exit_module);
MODULE_DESCRIPTION("AIS IFE driver");
MODULE_LICENSE("GPL v2");
