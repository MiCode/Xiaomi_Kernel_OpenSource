// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include "cam_cci_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_cci_soc.h"
#include "cam_cci_core.h"
#include "camera_main.h"

#define CCI_MAX_DELAY 1000000

static struct v4l2_subdev *g_cci_subdev[MAX_CCI];
static struct dentry *debugfs_root;

struct v4l2_subdev *cam_cci_get_subdev(int cci_dev_index)
{
	struct v4l2_subdev *sub_device = NULL;

	if (cci_dev_index < MAX_CCI)
		sub_device = g_cci_subdev[cci_dev_index];
	else
		CAM_WARN(CAM_CCI, "Index: %u is beyond max num CCI allowed: %u",
			cci_dev_index,
			MAX_CCI);

	return sub_device;
}

static long cam_cci_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int32_t rc = 0;

	if (arg == NULL) {
		CAM_ERR(CAM_CCI, "Args is Null");
		return -EINVAL;
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
	uint32_t irq_status0, irq_status1, reg_bmsk;
	uint32_t irq_update_rd_done = 0;
	struct cci_device *cci_dev = data;
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;
	void __iomem *base = soc_info->reg_map[0].mem_base;
	unsigned long flags;
	bool rd_done_th_assert = false;

	irq_status0 = cam_io_r_mb(base + CCI_IRQ_STATUS_0_ADDR);
	irq_status1 = cam_io_r_mb(base + CCI_IRQ_STATUS_1_ADDR);
	CAM_DBG(CAM_CCI,
		"BASE: %pK, irq0:%x irq1:%x",
		base, irq_status0, irq_status1);

	cam_io_w_mb(irq_status0, base + CCI_IRQ_CLEAR_0_ADDR);
	cam_io_w_mb(irq_status1, base + CCI_IRQ_CLEAR_1_ADDR);

	reg_bmsk = CCI_IRQ_MASK_1_RMSK;
	if ((irq_status1 & CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD) &&
	!(irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK)) {
		reg_bmsk &= ~CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD;
		spin_lock_irqsave(&cci_dev->lock_status, flags);
		cci_dev->irqs_disabled |=
			CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD;
		spin_unlock_irqrestore(&cci_dev->lock_status, flags);
	}

	if ((irq_status1 & CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD) &&
	!(irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK)) {
		reg_bmsk &= ~CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD;
		spin_lock_irqsave(&cci_dev->lock_status, flags);
		cci_dev->irqs_disabled |=
			CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD;
		spin_unlock_irqrestore(&cci_dev->lock_status, flags);
	}

	if (reg_bmsk != CCI_IRQ_MASK_1_RMSK) {
		cam_io_w_mb(reg_bmsk, base + CCI_IRQ_MASK_1_ADDR);
		CAM_DBG(CAM_CCI, "Updating the reg mask for irq1: 0x%x",
			reg_bmsk);
	} else if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK ||
		irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK) {
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK) {
			spin_lock_irqsave(&cci_dev->lock_status, flags);
			if (cci_dev->irqs_disabled &
				CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD) {
				irq_update_rd_done |=
					CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD;
				cci_dev->irqs_disabled &=
					~CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD;
			}
			spin_unlock_irqrestore(&cci_dev->lock_status, flags);
		}
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK) {
			spin_lock_irqsave(&cci_dev->lock_status, flags);
			if (cci_dev->irqs_disabled &
				CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD) {
				irq_update_rd_done |=
					CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD;
				cci_dev->irqs_disabled &=
					~CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD;
			}
			spin_unlock_irqrestore(&cci_dev->lock_status, flags);
		}
	}

	if (irq_update_rd_done != 0) {
		irq_update_rd_done |= cam_io_r_mb(base + CCI_IRQ_MASK_1_ADDR);
		cam_io_w_mb(irq_update_rd_done, base + CCI_IRQ_MASK_1_ADDR);
	}

	cam_io_w_mb(0x1, base + CCI_IRQ_GLOBAL_CLEAR_CMD_ADDR);

	if (irq_status0 & CCI_IRQ_STATUS_0_RST_DONE_ACK_BMSK) {
		struct cam_cci_master_info *cci_master_info;
		if (cci_dev->cci_master_info[MASTER_0].reset_pending == true) {
			cci_master_info = &cci_dev->cci_master_info[MASTER_0];
			cci_dev->cci_master_info[MASTER_0].reset_pending =
				false;
			if (!cci_master_info->status)
				complete(&cci_master_info->reset_complete);

			complete_all(&cci_master_info->rd_done);
			complete_all(&cci_master_info->th_complete);
		}
		if (cci_dev->cci_master_info[MASTER_1].reset_pending == true) {
			cci_master_info = &cci_dev->cci_master_info[MASTER_1];
			cci_dev->cci_master_info[MASTER_1].reset_pending =
				false;
			if (!cci_master_info->status)
				complete(&cci_master_info->reset_complete);

			complete_all(&cci_master_info->rd_done);
			complete_all(&cci_master_info->th_complete);
		}
	}

	if ((irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK) &&
		(irq_status1 & CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD)) {
		cci_dev->cci_master_info[MASTER_0].status = 0;
		rd_done_th_assert = true;
		complete(&cci_dev->cci_master_info[MASTER_0].th_complete);
		complete(&cci_dev->cci_master_info[MASTER_0].rd_done);
	}
	if ((irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK) &&
		(!rd_done_th_assert)) {
		cci_dev->cci_master_info[MASTER_0].status = 0;
		rd_done_th_assert = true;
		if (cci_dev->is_burst_read[MASTER_0])
			complete(
			&cci_dev->cci_master_info[MASTER_0].th_complete);
		complete(&cci_dev->cci_master_info[MASTER_0].rd_done);
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
	rd_done_th_assert = false;
	if ((irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK) &&
		(irq_status1 & CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD)) {
		cci_dev->cci_master_info[MASTER_1].status = 0;
		rd_done_th_assert = true;
		complete(&cci_dev->cci_master_info[MASTER_1].th_complete);
		complete(&cci_dev->cci_master_info[MASTER_1].rd_done);
	}
	if ((irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK) &&
		(!rd_done_th_assert)) {
		cci_dev->cci_master_info[MASTER_1].status = 0;
		rd_done_th_assert = true;
		if (cci_dev->is_burst_read[MASTER_1])
			complete(
			&cci_dev->cci_master_info[MASTER_1].th_complete);
		complete(&cci_dev->cci_master_info[MASTER_1].rd_done);
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
		cci_dev->cci_master_info[MASTER_0].reset_pending = true;
		cam_io_w_mb(CCI_M0_RESET_RMSK,
			base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_Q0Q1_HALT_ACK_BMSK) {
		cci_dev->cci_master_info[MASTER_1].reset_pending = true;
		cam_io_w_mb(CCI_M1_RESET_RMSK,
			base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_ERROR_BMSK) {
		cci_dev->cci_master_info[MASTER_0].status = -EINVAL;
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_Q0_NACK_ERROR_BMSK) {
			CAM_ERR(CAM_CCI,
				"Base:%pK,cci: %d, M0_Q0 NACK ERROR: 0x%x",
				base, cci_dev->soc_info.index, irq_status0);
			cam_cci_dump_registers(cci_dev, MASTER_0,
					QUEUE_0);
			complete_all(&cci_dev->cci_master_info[MASTER_0]
				.report_q[QUEUE_0]);
		}
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_Q1_NACK_ERROR_BMSK) {
			CAM_ERR(CAM_CCI,
				"Base:%pK,cci: %d, M0_Q1 NACK ERROR: 0x%x",
				base, cci_dev->soc_info.index, irq_status0);
			cam_cci_dump_registers(cci_dev, MASTER_0,
					QUEUE_1);
			complete_all(&cci_dev->cci_master_info[MASTER_0]
			.report_q[QUEUE_1]);
		}
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_Q0Q1_ERROR_BMSK)
			CAM_ERR(CAM_CCI,
			"Base:%pK, cci: %d, M0 QUEUE_OVER/UNDER_FLOW OR CMD ERR: 0x%x",
				base, cci_dev->soc_info.index, irq_status0);
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_ERROR_BMSK)
			CAM_ERR(CAM_CCI,
				"Base: %pK, M0 RD_OVER/UNDER_FLOW ERROR: 0x%x",
				base, irq_status0);

		cci_dev->cci_master_info[MASTER_0].reset_pending = true;
		cam_io_w_mb(CCI_M0_RESET_RMSK, base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_ERROR_BMSK) {
		cci_dev->cci_master_info[MASTER_1].status = -EINVAL;
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_Q0_NACK_ERROR_BMSK) {
			CAM_ERR(CAM_CCI,
				"Base:%pK, cci: %d, M1_Q0 NACK ERROR: 0x%x",
				base, cci_dev->soc_info.index, irq_status0);
			cam_cci_dump_registers(cci_dev, MASTER_1,
					QUEUE_0);
			complete_all(&cci_dev->cci_master_info[MASTER_1]
			.report_q[QUEUE_0]);
		}
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_Q1_NACK_ERROR_BMSK) {
			CAM_ERR(CAM_CCI,
				"Base:%pK, cci: %d, M1_Q1 NACK ERROR: 0x%x",
				base, cci_dev->soc_info.index, irq_status0);
			cam_cci_dump_registers(cci_dev, MASTER_1,
				QUEUE_1);
			complete_all(&cci_dev->cci_master_info[MASTER_1]
			.report_q[QUEUE_1]);
		}
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_Q0Q1_ERROR_BMSK)
			CAM_ERR(CAM_CCI,
			"Base:%pK, cci: %d, M1 QUEUE_OVER_UNDER_FLOW OR CMD ERROR:0x%x",
				base, cci_dev->soc_info.index, irq_status0);
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_ERROR_BMSK)
			CAM_ERR(CAM_CCI,
				"Base:%pK, cci: %d, M1 RD_OVER/UNDER_FLOW ERROR: 0x%x",
				base, cci_dev->soc_info.index, irq_status0);

		cci_dev->cci_master_info[MASTER_1].reset_pending = true;
		cam_io_w_mb(CCI_M1_RESET_RMSK, base + CCI_RESET_CMD_ADDR);
	}

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
	*handled = true;
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

static int cam_cci_get_debug(void *data, u64 *val)
{
	struct cci_device *cci_dev = (struct cci_device *)data;

	*val = cci_dev->dump_en;

	return 0;
}

static int cam_cci_set_debug(void *data, u64 val)
{
	struct cci_device *cci_dev = (struct cci_device *)data;

	cci_dev->dump_en = val;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(cam_cci_debug,
	cam_cci_get_debug,
	cam_cci_set_debug, "%16llu\n");

static int cam_cci_create_debugfs_entry(struct cci_device *cci_dev)
{
	int rc = 0;
	struct dentry *dbgfileptr = NULL;

	if (!debugfs_root) {
		dbgfileptr = debugfs_create_dir("cam_cci", NULL);
		if (!dbgfileptr) {
			CAM_ERR(CAM_CCI, "debugfs directory creation fail");
			rc = -ENOENT;
			goto end;
		}
		debugfs_root = dbgfileptr;
	}

	if (cci_dev->soc_info.index == 0) {
		dbgfileptr = debugfs_create_file("en_dump_cci0", 0644,
			debugfs_root, cci_dev, &cam_cci_debug);
		if (IS_ERR(dbgfileptr)) {
			if (PTR_ERR(dbgfileptr) == -ENODEV)
				CAM_WARN(CAM_CCI, "DebugFS not enabled");
			else {
				rc = PTR_ERR(dbgfileptr);
				goto end;
			}
		}
	} else {
		dbgfileptr = debugfs_create_file("en_dump_cci1", 0644,
			debugfs_root, cci_dev, &cam_cci_debug);
		if (IS_ERR(dbgfileptr)) {
			if (PTR_ERR(dbgfileptr) == -ENODEV)
				CAM_WARN(CAM_CCI, "DebugFS not enabled");
			else {
				rc = PTR_ERR(dbgfileptr);
				goto end;
			}
		}
	}
end:
	return rc;
}

static int cam_cci_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_cpas_register_params cpas_parms;
	struct cci_device *new_cci_dev;
	struct cam_hw_soc_info *soc_info = NULL;
	int rc = 0;
	struct platform_device *pdev = to_platform_device(dev);

	new_cci_dev = devm_kzalloc(&pdev->dev, sizeof(struct cci_device),
		GFP_KERNEL);
	if (!new_cci_dev) {
		CAM_ERR(CAM_CCI, "Memory allocation failed for cci_dev");
		return -ENOMEM;
	}
	soc_info = &new_cci_dev->soc_info;

	new_cci_dev->v4l2_dev_str.pdev = pdev;

	soc_info->pdev = pdev;
	soc_info->dev = &pdev->dev;
	soc_info->dev_name = pdev->name;

	rc = cam_cci_parse_dt_info(pdev, new_cci_dev);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "Resource get Failed rc:%d", rc);
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
	new_cci_dev->v4l2_dev_str.sd_flags = V4L2_SUBDEV_FL_HAS_EVENTS;
	new_cci_dev->v4l2_dev_str.ent_function =
		CAM_CCI_DEVICE_TYPE;
	new_cci_dev->v4l2_dev_str.token =
		new_cci_dev;

	rc = cam_register_subdev(&(new_cci_dev->v4l2_dev_str));
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "Fail with cam_register_subdev rc: %d", rc);
		goto cci_no_resource;
	}

	platform_set_drvdata(pdev, &(new_cci_dev->v4l2_dev_str.sd));
	v4l2_set_subdevdata(&new_cci_dev->v4l2_dev_str.sd, new_cci_dev);
	if (soc_info->index >= MAX_CCI) {
		CAM_ERR(CAM_CCI, "Invalid index: %d max supported:%d",
			soc_info->index, MAX_CCI-1);
		goto cci_no_resource;
	}

	g_cci_subdev[soc_info->index] = &new_cci_dev->v4l2_dev_str.sd;
	mutex_init(&(new_cci_dev->init_mutex));
	CAM_DBG(CAM_CCI, "Device Type :%d", soc_info->index);

	cpas_parms.cam_cpas_client_cb = NULL;
	cpas_parms.cell_index = soc_info->index;
	cpas_parms.dev = &pdev->dev;
	cpas_parms.userdata = new_cci_dev;
	strlcpy(cpas_parms.identifier, "cci", CAM_HW_IDENTIFIER_LENGTH);
	rc = cam_cpas_register_client(&cpas_parms);
	if (rc) {
		CAM_ERR(CAM_CCI, "CPAS registration failed rc:%d", rc);
		goto cci_unregister_subdev;
	}

	CAM_DBG(CAM_CCI, "CPAS registration successful handle=%d",
		cpas_parms.client_handle);
	new_cci_dev->cpas_handle = cpas_parms.client_handle;

	rc = cam_cci_create_debugfs_entry(new_cci_dev);
	if (rc) {
		CAM_WARN(CAM_CCI, "debugfs creation failed");
		rc = 0;
	}
	CAM_DBG(CAM_CCI, "Component bound successfully");
	return rc;

cci_unregister_subdev:
	cam_unregister_subdev(&(new_cci_dev->v4l2_dev_str));
cci_no_resource:
	devm_kfree(&pdev->dev, new_cci_dev);
	return rc;
}

static void cam_cci_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc = 0;
	struct platform_device *pdev = to_platform_device(dev);

	struct v4l2_subdev *subdev = platform_get_drvdata(pdev);
	struct cci_device *cci_dev =
		v4l2_get_subdevdata(subdev);

	cam_cpas_unregister_client(cci_dev->cpas_handle);
	debugfs_remove_recursive(debugfs_root);
	cam_cci_soc_remove(pdev, cci_dev);
	rc = cam_unregister_subdev(&(cci_dev->v4l2_dev_str));
	if (rc < 0)
		CAM_ERR(CAM_CCI, "Fail with cam_unregister_subdev. rc:%d", rc);

	devm_kfree(&pdev->dev, cci_dev);
}

const static struct component_ops cam_cci_component_ops = {
	.bind = cam_cci_component_bind,
	.unbind = cam_cci_component_unbind,
};

static int cam_cci_platform_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_CCI, "Adding CCI component");
	rc = component_add(&pdev->dev, &cam_cci_component_ops);
	if (rc)
		CAM_ERR(CAM_CCI, "failed to add component rc: %d", rc);

	return rc;
}

static int cam_cci_device_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_cci_component_ops);
	return 0;
}

static const struct of_device_id cam_cci_dt_match[] = {
	{.compatible = "qcom,cci"},
	{}
};

MODULE_DEVICE_TABLE(of, cam_cci_dt_match);

struct platform_driver cci_driver = {
	.probe = cam_cci_platform_probe,
	.remove = cam_cci_device_remove,
	.driver = {
		.name = CAMX_CCI_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_cci_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_cci_init_module(void)
{
	return platform_driver_register(&cci_driver);
}

void cam_cci_exit_module(void)
{
	platform_driver_unregister(&cci_driver);
}

MODULE_DESCRIPTION("MSM CCI driver");
MODULE_LICENSE("GPL v2");
