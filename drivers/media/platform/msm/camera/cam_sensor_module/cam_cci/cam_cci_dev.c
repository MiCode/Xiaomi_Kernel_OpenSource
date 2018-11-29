/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#include "cam_cci_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_cci_soc.h"
#include "cam_cci_core.h"

#define CCI_MAX_DELAY 1000000

static struct v4l2_subdev *g_cci_subdev;

struct v4l2_subdev *cam_cci_get_subdev(void)
{
	return g_cci_subdev;
}

static long cam_cci_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int32_t rc = 0;

	if (arg == NULL) {
		CAM_ERR(CAM_CCI, "Invalid Args");
		return rc;
	}

	switch (cmd) {
	case VIDIOC_MSM_CCI_CFG:
		rc = cam_cci_core_cfg(sd, arg);
		break;
	case VIDIOC_CAM_CONTROL:
		break;
	default:
		CAM_ERR(CAM_CCI, "Invalid ioctl cmd: %d", cmd);
		rc = -ENOIOCTLCMD;
	}

	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_cci_subdev_compat_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	return cam_cci_subdev_ioctl(sd, cmd, NULL);
}
#endif

irqreturn_t cam_cci_irq(int irq_num, void *data)
{
	uint32_t irq_status0 = 0;
	uint32_t irq_status1 = 0;
	struct cci_device *cci_dev = data;
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;
	void __iomem *base = soc_info->reg_map[0].mem_base;
	unsigned long flags;
	bool rd_done_th_assert = false;

	irq_status0 = cam_io_r_mb(base + CCI_IRQ_STATUS_0_ADDR);
	irq_status1 = cam_io_r_mb(base + CCI_IRQ_STATUS_1_ADDR);
	CAM_DBG(CAM_CCI, "irq0:%x irq1:%x", irq_status0, irq_status1);

	if (irq_status0 & CCI_IRQ_STATUS_0_RST_DONE_ACK_BMSK) {
		if (cci_dev->cci_master_info[MASTER_0].reset_pending == TRUE) {
			cci_dev->cci_master_info[MASTER_0].reset_pending =
				FALSE;
			complete(
			&cci_dev->cci_master_info[MASTER_0].reset_complete);
		}
		if (cci_dev->cci_master_info[MASTER_1].reset_pending == TRUE) {
			cci_dev->cci_master_info[MASTER_1].reset_pending =
				FALSE;
			complete(
			&cci_dev->cci_master_info[MASTER_1].reset_complete);
		}
	}

	if ((irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK) &&
		(irq_status1 & CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD)) {
		cci_dev->cci_master_info[MASTER_0].status = 0;
		rd_done_th_assert = true;
		complete(&cci_dev->cci_master_info[MASTER_0].th_complete);
		complete(&cci_dev->cci_master_info[MASTER_0].reset_complete);
	}
	if ((irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK) &&
		(!rd_done_th_assert)) {
		cci_dev->cci_master_info[MASTER_0].status = 0;
		rd_done_th_assert = true;
		if (cci_dev->is_burst_read)
			complete(
			&cci_dev->cci_master_info[MASTER_0].th_complete);
		complete(&cci_dev->cci_master_info[MASTER_0].reset_complete);
	}
	if ((irq_status1 & CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD) &&
		(!rd_done_th_assert)) {
		cci_dev->cci_master_info[MASTER_0].status = 0;
		complete(&cci_dev->cci_master_info[MASTER_0].th_complete);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_Q0_REPORT_BMSK) {
		struct cam_cci_master_info *cci_master_info;

		cci_master_info = &cci_dev->cci_master_info[MASTER_0];
		spin_lock_irqsave(
			&cci_dev->cci_master_info[MASTER_0].lock_q[QUEUE_0],
			flags);
		atomic_set(&cci_master_info->q_free[QUEUE_0], 0);
		cci_master_info->status = 0;
		if (atomic_read(&cci_master_info->done_pending[QUEUE_0]) == 1) {
			complete(&cci_master_info->report_q[QUEUE_0]);
			atomic_set(&cci_master_info->done_pending[QUEUE_0], 0);
		}
		spin_unlock_irqrestore(
			&cci_dev->cci_master_info[MASTER_0].lock_q[QUEUE_0],
			flags);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_Q1_REPORT_BMSK) {
		struct cam_cci_master_info *cci_master_info;

		cci_master_info = &cci_dev->cci_master_info[MASTER_0];
		spin_lock_irqsave(
			&cci_dev->cci_master_info[MASTER_0].lock_q[QUEUE_1],
			flags);
		atomic_set(&cci_master_info->q_free[QUEUE_1], 0);
		cci_master_info->status = 0;
		if (atomic_read(&cci_master_info->done_pending[QUEUE_1]) == 1) {
			complete(&cci_master_info->report_q[QUEUE_1]);
			atomic_set(&cci_master_info->done_pending[QUEUE_1], 0);
		}
		spin_unlock_irqrestore(
			&cci_dev->cci_master_info[MASTER_0].lock_q[QUEUE_1],
			flags);
	}
	if ((irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK) &&
		(irq_status1 & CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD)) {
		cci_dev->cci_master_info[MASTER_1].status = 0;
		rd_done_th_assert = true;
		complete(&cci_dev->cci_master_info[MASTER_1].th_complete);
		complete(&cci_dev->cci_master_info[MASTER_1].reset_complete);
	}
	if ((irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK) &&
		(!rd_done_th_assert)) {
		cci_dev->cci_master_info[MASTER_1].status = 0;
		rd_done_th_assert = true;
		if (cci_dev->is_burst_read)
			complete(
			&cci_dev->cci_master_info[MASTER_1].th_complete);
		complete(&cci_dev->cci_master_info[MASTER_1].reset_complete);
	}
	if ((irq_status1 & CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD) &&
		(!rd_done_th_assert)) {
		cci_dev->cci_master_info[MASTER_1].status = 0;
		complete(&cci_dev->cci_master_info[MASTER_1].th_complete);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_Q0_REPORT_BMSK) {
		struct cam_cci_master_info *cci_master_info;

		cci_master_info = &cci_dev->cci_master_info[MASTER_1];
		spin_lock_irqsave(
			&cci_dev->cci_master_info[MASTER_1].lock_q[QUEUE_0],
			flags);
		atomic_set(&cci_master_info->q_free[QUEUE_0], 0);
		cci_master_info->status = 0;
		if (atomic_read(&cci_master_info->done_pending[QUEUE_0]) == 1) {
			complete(&cci_master_info->report_q[QUEUE_0]);
			atomic_set(&cci_master_info->done_pending[QUEUE_0], 0);
		}
		spin_unlock_irqrestore(
			&cci_dev->cci_master_info[MASTER_1].lock_q[QUEUE_0],
			flags);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_Q1_REPORT_BMSK) {
		struct cam_cci_master_info *cci_master_info;

		cci_master_info = &cci_dev->cci_master_info[MASTER_1];
		spin_lock_irqsave(
			&cci_dev->cci_master_info[MASTER_1].lock_q[QUEUE_1],
			flags);
		atomic_set(&cci_master_info->q_free[QUEUE_1], 0);
		cci_master_info->status = 0;
		if (atomic_read(&cci_master_info->done_pending[QUEUE_1]) == 1) {
			complete(&cci_master_info->report_q[QUEUE_1]);
			atomic_set(&cci_master_info->done_pending[QUEUE_1], 0);
		}
		spin_unlock_irqrestore(
			&cci_dev->cci_master_info[MASTER_1].lock_q[QUEUE_1],
			flags);
	}
	if (irq_status1 & CCI_IRQ_STATUS_1_I2C_M0_RD_PAUSE)
		CAM_DBG(CAM_CCI, "RD_PAUSE ON MASTER_0");

	if (irq_status1 & CCI_IRQ_STATUS_1_I2C_M1_RD_PAUSE)
		CAM_DBG(CAM_CCI, "RD_PAUSE ON MASTER_1");

	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_Q0Q1_HALT_ACK_BMSK) {
		cci_dev->cci_master_info[MASTER_0].reset_pending = TRUE;
		cam_io_w_mb(CCI_M0_RESET_RMSK,
			base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_Q0Q1_HALT_ACK_BMSK) {
		cci_dev->cci_master_info[MASTER_1].reset_pending = TRUE;
		cam_io_w_mb(CCI_M1_RESET_RMSK,
			base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_ERROR_BMSK) {
		cci_dev->cci_master_info[MASTER_0].status = -EINVAL;
		cam_io_w_mb(CCI_M0_HALT_REQ_RMSK,
			base + CCI_HALT_REQ_ADDR);
		CAM_DBG(CAM_CCI, "MASTER_0 error 0x%x", irq_status0);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_ERROR_BMSK) {
		cci_dev->cci_master_info[MASTER_1].status = -EINVAL;
		cam_io_w_mb(CCI_M1_HALT_REQ_RMSK,
			base + CCI_HALT_REQ_ADDR);
		CAM_DBG(CAM_CCI, "MASTER_1 error 0x%x", irq_status0);
	}

	if ((rd_done_th_assert) || (!cci_dev->is_burst_read)) {
		cam_io_w_mb(irq_status1, base + CCI_IRQ_CLEAR_1_ADDR);
		CAM_DBG(CAM_CCI, "clear irq_status0:%x irq_status1:%x",
			irq_status0, irq_status1);
	} else {
		spin_lock_irqsave(&cci_dev->lock_status, flags);
		cci_dev->irq_status1 |= irq_status1;
		spin_unlock_irqrestore(&cci_dev->lock_status, flags);
	}

	cam_io_w_mb(irq_status0, base + CCI_IRQ_CLEAR_0_ADDR);
	cam_io_w_mb(0x1, base + CCI_IRQ_GLOBAL_CLEAR_CMD_ADDR);
	return IRQ_HANDLED;
}

static int cam_cci_irq_routine(struct v4l2_subdev *sd, u32 status,
	bool *handled)
{
	struct cci_device *cci_dev = v4l2_get_subdevdata(sd);
	irqreturn_t ret;
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;

	ret = cam_cci_irq(soc_info->irq_line->start, cci_dev);
	*handled = TRUE;
	return 0;
}

static struct v4l2_subdev_core_ops cci_subdev_core_ops = {
	.ioctl = cam_cci_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_cci_subdev_compat_ioctl,
#endif
	.interrupt_service_routine = cam_cci_irq_routine,
};

static const struct v4l2_subdev_ops cci_subdev_ops = {
	.core = &cci_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops cci_subdev_intern_ops;

static struct v4l2_file_operations cci_v4l2_subdev_fops;

static long cam_cci_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);

	return cam_cci_subdev_ioctl(sd, cmd, NULL);
}

static long cam_cci_subdev_fops_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, cam_cci_subdev_do_ioctl);
}

#ifdef CONFIG_COMPAT
static long cam_cci_subdev_fops_compat_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);

	return v4l2_subdev_call(sd, core, ioctl, cmd, NULL);
}
#endif

static int cam_cci_platform_probe(struct platform_device *pdev)
{
	struct cam_cpas_register_params cpas_parms;
	struct cci_device *new_cci_dev;
	struct cam_hw_soc_info *soc_info = NULL;
	int rc = 0;

	new_cci_dev = kzalloc(sizeof(struct cci_device),
		GFP_KERNEL);
	if (!new_cci_dev)
		return -ENOMEM;

	soc_info = &new_cci_dev->soc_info;

	new_cci_dev->v4l2_dev_str.pdev = pdev;

	soc_info->pdev = pdev;
	soc_info->dev = &pdev->dev;
	soc_info->dev_name = pdev->name;

	rc = cam_cci_parse_dt_info(pdev, new_cci_dev);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "Resource get Failed: %d", rc);
		goto cci_no_resource;
	}

	new_cci_dev->v4l2_dev_str.internal_ops =
		&cci_subdev_intern_ops;
	new_cci_dev->v4l2_dev_str.ops =
		&cci_subdev_ops;
	strlcpy(new_cci_dev->device_name, CAMX_CCI_DEV_NAME,
		sizeof(new_cci_dev->device_name));
	new_cci_dev->v4l2_dev_str.name =
		new_cci_dev->device_name;
	new_cci_dev->v4l2_dev_str.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	new_cci_dev->v4l2_dev_str.ent_function =
		CAM_CCI_DEVICE_TYPE;
	new_cci_dev->v4l2_dev_str.token =
		new_cci_dev;

	rc = cam_register_subdev(&(new_cci_dev->v4l2_dev_str));
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "Fail with cam_register_subdev");
		goto cci_no_resource;
	}

	platform_set_drvdata(pdev, &(new_cci_dev->v4l2_dev_str.sd));
	v4l2_set_subdevdata(&new_cci_dev->v4l2_dev_str.sd, new_cci_dev);
	g_cci_subdev = &new_cci_dev->v4l2_dev_str.sd;

	cam_register_subdev_fops(&cci_v4l2_subdev_fops);
	cci_v4l2_subdev_fops.unlocked_ioctl = cam_cci_subdev_fops_ioctl;
#ifdef CONFIG_COMPAT
	cci_v4l2_subdev_fops.compat_ioctl32 =
		cam_cci_subdev_fops_compat_ioctl;
#endif

	cpas_parms.cam_cpas_client_cb = NULL;
	cpas_parms.cell_index = 0;
	cpas_parms.dev = &pdev->dev;
	cpas_parms.userdata = new_cci_dev;
	strlcpy(cpas_parms.identifier, "cci", CAM_HW_IDENTIFIER_LENGTH);
	rc = cam_cpas_register_client(&cpas_parms);
	if (rc) {
		CAM_ERR(CAM_CCI, "CPAS registration failed");
		goto cci_no_resource;
	}
	CAM_DBG(CAM_CCI, "CPAS registration successful handle=%d",
		cpas_parms.client_handle);
	new_cci_dev->cpas_handle = cpas_parms.client_handle;

	return rc;
cci_no_resource:
	kfree(new_cci_dev);
	return rc;
}

static int cam_cci_device_remove(struct platform_device *pdev)
{
	struct v4l2_subdev *subdev = platform_get_drvdata(pdev);
	struct cci_device *cci_dev =
		v4l2_get_subdevdata(subdev);

	cam_cpas_unregister_client(cci_dev->cpas_handle);
	cam_cci_soc_remove(pdev, cci_dev);
	devm_kfree(&pdev->dev, cci_dev);
	return 0;
}

static const struct of_device_id cam_cci_dt_match[] = {
	{.compatible = "qcom,cci"},
	{}
};

MODULE_DEVICE_TABLE(of, cam_cci_dt_match);

static struct platform_driver cci_driver = {
	.probe = cam_cci_platform_probe,
	.remove = cam_cci_device_remove,
	.driver = {
		.name = CAMX_CCI_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_cci_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int cam_cci_assign_fops(void)
{
	struct v4l2_subdev *sd;

	sd = g_cci_subdev;
	if (!sd || !(sd->devnode)) {
		CAM_ERR(CAM_CRM,
			"Invalid args sd node: %pK", sd);
		return -EINVAL;
	}
	sd->devnode->fops = &cci_v4l2_subdev_fops;

	return 0;
}

static int __init cam_cci_late_init(void)
{
	return cam_cci_assign_fops();
}

static int __init cam_cci_init_module(void)
{
	return platform_driver_register(&cci_driver);
}

static void __exit cam_cci_exit_module(void)
{
	platform_driver_unregister(&cci_driver);
}

module_init(cam_cci_init_module);
late_initcall(cam_cci_late_init);
module_exit(cam_cci_exit_module);
MODULE_DESCRIPTION("MSM CCI driver");
MODULE_LICENSE("GPL v2");
