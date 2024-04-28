/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */
#define DEBUG

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/of_platform.h>
#include <linux/workqueue.h>
#include <linux/genalloc.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/component.h>
#include <linux/mfd/ispv4_defs.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/ispv4_defs.h>
#include <linux/mfd/core.h>
#include "ispv4_pcie_pm.h"
#include "ispv4_regops.h"
#include "../ipc/rproc/ispv4_rproc.h"
#include "ispv4_ctrl.h"

#define HAS_ISP_NODE

#define ISPV4_PM_LINKST 0xCC00020
#define ISPV4_PM_LINKST_L0s (1 << 18)
#define ISPV4_PM_LINKST_L1 (1 << 17)
#define ISPV4_PM_LINKST_L1ss (1 << 16)
#define ISPV4_PM_LINKST_L2 (1 << 15)

#define CHECK_COMP_AVALID(comp)                                                \
	do {                                                                   \
		if (!priv->comp.avalid) {                                      \
			ret = -ENODEV;                                         \
			goto err;                                              \
		}                                                              \
	} while (0)

extern struct dentry *ispv4_debugfs;

static struct mfd_cell ispv4_qcomcam_cell = {
	.name = "ispv4-cam",
	.num_resources = 0,
};

static int ispv4_v4l2_open(struct file *file)
{
	int ret;
	struct ispv4_v4l2_dev *priv = video_drvdata(file);
	ret = atomic_cmpxchg(&priv->opened, 0, 1);
	if (ret != 0) {
		return -EBUSY;
	}

	ret = v4l2_fh_open(file);
	if (ret)
		goto open_err;

	return 0;

open_err:
	atomic_set(&priv->opened, 0);
	return ret;
}

static int ispv4_v4l2_close(struct file *file)
{
	struct ispv4_v4l2_dev *priv = video_drvdata(file);
	v4l2_fh_release(file);
	atomic_set(&priv->opened, 0);
	return 0;
}

static int asst_setloglevel(struct ispv4_v4l2_dev *priv, int level)
{
	int msg_len = sizeof(struct xm_ispv4_rpmsg_pkg) + 4;
	struct xm_ispv4_rpmsg_pkg *send_buf;
	int ret;

	if (!priv->v4l2_rpmsg.avalid)
		return -ENODEV;

	send_buf = kzalloc(msg_len, GFP_KERNEL);
	if (send_buf == NULL)
		return -ENOMEM;

	send_buf->func = ICC_REQUEST_DEBUG_LOG_SET_LEVEL;
	send_buf->data[0] = level;

	ret = priv->v4l2_rpmsg.send(priv->v4l2_rpmsg.rp,
				    XM_ISPV4_IPC_EPT_RPMSG_ASST,
				    MIPC_MSGHEADER_COMMON, msg_len, send_buf,
				    false, NULL);

	kfree(send_buf);

	return ret;
}

static int asst_logenable(struct ispv4_v4l2_dev *priv, bool enable)
{
	int msg_len = sizeof(struct xm_ispv4_rpmsg_pkg) + 4;
	struct xm_ispv4_rpmsg_pkg *send_buf;
	int ret;

	if (!priv->v4l2_rpmsg.avalid)
		return -ENODEV;

	send_buf = kzalloc(msg_len, GFP_KERNEL);
	if (send_buf == NULL)
		return -ENOMEM;

	send_buf->func = ICC_REQUEST_DEBUG_LOG_ENABLE;
	send_buf->data[0] = enable ? priv->v4l2_rproc.rp->ramlog_dma_da : 0;

	ret = priv->v4l2_rpmsg.send(priv->v4l2_rpmsg.rp,
				    XM_ISPV4_IPC_EPT_RPMSG_ASST,
				    MIPC_MSGHEADER_COMMON, msg_len, send_buf,
				    false, NULL);

	kfree(send_buf);

	return ret;
}

static long ispv4_v4l2_isp_ioctl(struct v4l2_subdev *sd, unsigned int cmd,
				 void *arg)
{
	int ret;
	struct ispv4_v4l2_dev *priv = v4l2_get_subdevdata(sd);
	struct ispv4_cam_control *cmd_data;

	cmd_data = arg;
	switch (cmd) {
	case ISPV4_POWERON:
		dev_info(priv->dev, "power on\n");
		break;
	case ISPV4_RPROC_BOOT:
		CHECK_COMP_AVALID(v4l2_rproc);
		priv->v4l2_rproc.register_crash_cb(priv->v4l2_rproc.rp, NULL,
						   NULL);
		ret = priv->v4l2_rproc.boot(priv->v4l2_rproc.rp, NULL);
		if (ret != 0)
			goto err;
		dev_info(priv->dev, "rproc boot\n");
		break;
	case ISPV4_IONMAP:
		CHECK_COMP_AVALID(v4l2_ionmap);
		ret = priv->v4l2_ionmap.mapfd(
			priv->v4l2_ionmap.dev,
			cmd_data->params[ISPV4_CTRLPARAM_IONMAP_FD],
			cmd_data->params[ISPV4_CTRLPARAM_IONMAP_REGION], NULL);
		if (ret != 0)
			goto err;
		dev_info(priv->dev, "map region %d\n",
			 cmd_data->params[ISPV4_CTRLPARAM_IONMAP_REGION]);
		break;
	case ISPV4_IONUNMAP:
		CHECK_COMP_AVALID(v4l2_ionmap);
		ret = priv->v4l2_ionmap.unmap(
			priv->v4l2_ionmap.dev,
			cmd_data->params[ISPV4_CTRLPARAM_IONMAP_REGION]);
		if (ret != 0)
			goto err;
		dev_info(priv->dev, "unmap region %d\n",
			 cmd_data->params[ISPV4_CTRLPARAM_IONMAP_REGION]);
		break;
	case ISPV4_POWEROFF:
		dev_info(priv->dev, "power off\n");
		break;
	case ISPV4_RPROC_SHUTDOWN:
		CHECK_COMP_AVALID(v4l2_rproc);
		ret = priv->v4l2_rproc.shutdown(priv->v4l2_rproc.rp);
		if (ret != 0)
			goto err;
		dev_info(priv->dev, "rproc shutdown\n");
		break;
	default:
		dev_err(priv->dev, "%s Unknown/Empty cmd %d.\n", __FUNCTION__,
			cmd);
	}

	return 0;
err:
	dev_err(priv->dev, "ioctl failed %d\n", ret);
	return ret;
}

static long ispv4_v4l2_ass_ioctl(struct v4l2_subdev *sd, unsigned int cmd,
				 void *arg)
{
	int ret = 0;
	struct ispv4_v4l2_dev *priv = v4l2_get_subdevdata(sd);
	struct ispv4_cam_control *cmd_data;
	struct ispv4_pmic_regu regu_param;

	cmd_data = arg;
	switch (cmd) {
	case ISPV4_PCIE_PM_CTL: {
		CHECK_COMP_AVALID(v4l2_pci);
		ret = ispv4_pci_linksta_ctl(priv->v4l2_pci.pcidev,
					    cmd_data->params[0]);
		if (ret) {
			dev_err(priv->dev, "PM Ctl set fail\n");
			goto err;
		}
		dev_info(priv->dev, "PM Ctl set\n");
	} break;
	case ISPV4_PCIE_GET_ASPM_STATE: {
		uint32_t val = 0;
		ret = ispv4_regops_read(ISPV4_PM_LINKST, &val);
		if (ret) {
			dev_err(priv->dev, "spi read fail\n");
			goto err;
		}
		dev_info(priv->dev, "ISPV4_PM_LINKST 0x%x\n", val);

		if (val & ISPV4_PM_LINKST_L0s)
			dev_info(priv->dev, "ISPV4_PM_LINKST in L0s\n");
		else if (val & ISPV4_PM_LINKST_L1)
			dev_info(priv->dev, "ISPV4_PM_LINKST in L1\n");
		else if (val & ISPV4_PM_LINKST_L1ss)
			dev_info(priv->dev, "ISPV4_PM_LINKST in L1ss\n");
		else if (val & ISPV4_PM_LINKST_L2)
			dev_info(priv->dev, "ISPV4_PM_LINKST in L2\n");
		else
			dev_info(priv->dev, "ISPV4_PM_LINKST in L0\n");

		cmd_data->params[0] = val;
	} break;
	case ISPV4_PCIE_SET_LINK_SPEED: {
		CHECK_COMP_AVALID(v4l2_pci);
		ret = ispv4_set_linkspeed(priv->v4l2_pci.pcidev,
					  cmd_data->params[0]);
		if (ret) {
			dev_err(priv->dev, "set linkspeed err\n");
			goto err;
		}
		dev_info(priv->dev, "set linkspeed to %d\n",
			 cmd_data->params[0]);
	} break;
	case ISPV4_PCIE_GET_LINK_SPEED: {
		uint16_t link_speed;
		CHECK_COMP_AVALID(v4l2_pci);
		link_speed = ispv4_get_linkspeed(priv->v4l2_pci.pcidev);
		cmd_data->params[0] = link_speed;
		dev_info(priv->dev, "get linkspeed to %d\n", link_speed);
	} break;
	case ISPV4_CAM_ASST_READ: {
		uint32_t read_val;
		ret = ispv4_regops_read(cmd_data->params[0], &read_val);
		if (ret) {
			dev_err(priv->dev, "spi read fail\n");
			goto err;
		}
		cmd_data->params[0] = read_val;
	}
		break;
	case ISPV4_CAM_ASST_WRITE: {
		ret = ispv4_regops_write(cmd_data->params[0], cmd_data->params[1]);
		if (ret) {
			dev_err(priv->dev, "spi read fail\n");
			goto err;
		}
	}
		break;
	case ISPV4_CAM_ASST_SET: {
		ret = ispv4_regops_set(cmd_data->params[0], cmd_data->params[1],
				       cmd_data->params[2]);
		if (ret) {
			dev_err(priv->dev, "spi read fail\n");
			goto err;
		}
	}
		break;

	case ISPV4_ASST_CMD_SETLOGLEVEL:
		CHECK_COMP_AVALID(v4l2_rpmsg);
		ret = asst_setloglevel(
			priv, cmd_data->params[ISPV4_ASST_LOG_PARAM_P]);
		if (ret != 0)
			goto err;
		dev_info(priv->dev, "v4log set level finish\n");
		break;

	case ISPV4_ASST_CMD_LOGENABLE:
		CHECK_COMP_AVALID(v4l2_rpmsg);
		ret = asst_logenable(priv,
				     cmd_data->params[ISPV4_ASST_LOG_PARAM_P]);
		if (ret != 0)
			goto err;
		dev_info(priv->dev, "v4log enable finish\n");
		break;

	case ISPV4_ASST_CPU_PLL_INIT: {
		CHECK_COMP_AVALID(v4l2_ctrl);
		if (cmd_data->params[0] == PLL_POWER_ON) {
			ret = ispv4_power_on_cpu(priv->v4l2_ctrl.pdev);
			if (ret != 0)
				goto err;
			ret = ispv4_pll_disenable();
			ret |= ispv4_regops_write(CPUPLL_CON1, CPUPLL_CON1_CONFIG);
			pr_info("ispv4 pll_cpu set 0x%lx = 0x%lx\n", CPUPLL_CON1, CPUPLL_CON1_CONFIG);
			ret |= ispv4_regops_write(CPUPLL_CON2, CPUPLL_CON2_CONFIG);
			pr_info("ispv4 pll_cpu set 0x%lx = 0x%lx\n", CPUPLL_CON2, CPUPLL_CON2_CONFIG);
			ret |= ispv4_regops_write(CPUPLL_CON4, CPUPLL_CON4_CONFIG);
			pr_info("ispv4 pll_cpu set 0x%lx = 0x%lx\n", CPUPLL_CON4, CPUPLL_CON4_CONFIG);
			if (ret != 0)
				goto err;
		} else if (cmd_data->params[0] == PLL_POWER_OFF) {
			ispv4_power_off_cpu(priv->v4l2_ctrl.pdev);
		} else {
			goto err;
		}
	}
		break;

	case ISPV4_ASST_CPU_PLL_TEST: {
		uint64_t time_us_old , time_us_new ;
		ret = ispv4_pll_en();
		if (ret != 0) {
			pr_info("ispv4 pll_cpu enable failed\n");
			goto err;
		}
		time_us_old = (ktime_get_ns() / 1000);
		usleep_range(13, 14);

		time_us_new = ispv4_plltest_status(CPUPLL_CON4);
		if (time_us_new != 0) {
			pr_info("ispv4 pll_cpu waitlock failed, using time %ld us \n",
				time_us_new - time_us_old);
		}

		ret = ispv4_pll_disenable();
		if (ret != 0) {
			pr_info("ispv4 pll_cpu disable failed\n");
			goto err;
		}
	}
		break;

	case ISPV4_ASST_DDR_PLL_INIT: {
		CHECK_COMP_AVALID(v4l2_ctrl);
		if (cmd_data->params[0] == PLL_POWER_ON) {
			ret = ispv4_power_on_cpu(priv->v4l2_ctrl.pdev);
			if (ret != 0)
				goto err;
			ispv4_ddrpll_disable();
			if (cmd_data->params[1] == DPLL_SETRATE_2133M) {
				ret = ispv4_ddrpll_2133m();
			} else if (cmd_data->params[1] == DPLL_SETRATE_1600M) {
				ret = ispv4_ddrpll_1600m();
			} else {
				goto err;
			}
			if (ret != 0)
				goto err;
			ispv4_regops_write(DDRPLL_CON4, DPLL_CON4_CONFIG);
			pr_info("ispv4 pll_cpu set 0x%lx = 0x%lx\n", DDRPLL_CON4, DPLL_CON4_CONFIG);

			ret = ispv4_config_dpll_gpio(priv->v4l2_ctrl.pdev);
			if (ret != 0)
				goto err;
		} else if (cmd_data->params[0] == PLL_POWER_OFF) {
			ispv4_power_off_cpu(priv->v4l2_ctrl.pdev);
		} else {
			goto err;
		}
	}
		break;

	case ISPV4_ASST_DDR_PLL_TEST: {
		uint64_t time_us_old , time_us_new ;
		ret = ispv4_ddrpll_enable();
		if (ret != 0) {
			pr_info("ispv4 pll_ddr enable failed\n");
			goto err;
		}
		time_us_old = (ktime_get_ns() / 1000);
		usleep_range(13,14);
		time_us_new = ispv4_plltest_status(DDRPLL_CON4);
		if (time_us_new != 0) {
			pr_info("ispv4 pll_ddr waitlock failed, using time %ld us \n",
				time_us_new - time_us_old);
		}
		ret = ispv4_ddrpll_disable();
		if (ret != 0) {
			pr_info("ispv4 pll_ddr disable failed\n");
			goto err;
		}
	}
		break;

	case ISPV4_PMIC_REGU_CONFIG: {
		CHECK_COMP_AVALID(v4l2_pmic);
		if (copy_from_user(&regu_param,u64_to_user_ptr(cmd_data->priv),
		    sizeof(regu_param)))
			return -EFAULT;
		ret = priv->v4l2_pmic.regulator(priv->v4l2_pmic.data,
						regu_param.ops,
						regu_param.id,
						&regu_param.en,
						&regu_param.voltage);
		if (ret != 0)
			goto err;
		if (copy_to_user(u64_to_user_ptr(cmd_data->priv), &regu_param,
		    sizeof(regu_param))) {
			dev_err(priv->dev, "Failed Copy to User");
			return -EFAULT;
		}
	}
			break;
	case ISPv4_ASST_PVT_SENSOR_GET: {
		int i;
		uint32_t databuf[20];
		uint32_t *recv_buf = NULL;
		int msg_len = sizeof(struct xm_ispv4_rpmsg_pkg) + 4;
		struct xm_ispv4_rpmsg_pkg *send_buf = kzalloc(msg_len + 120, GFP_KERNEL);
		if (send_buf == NULL) {
			dev_err(priv->dev, "send_buf kzalloc failed\n");
			goto err;
		}

		if (!priv->v4l2_rpmsg.avalid) {
			kfree(send_buf);
			goto err;
		}

		send_buf->func = ICC_REQUEST_THERMAL_VALUE;
		send_buf->data[0] = 2; //get PROCESS and VOLTAGE sensor

		ret = priv->v4l2_rpmsg.send(priv->v4l2_rpmsg.rp, XM_ISPV4_IPC_EPT_RPMSG_ASST,
					    MIPC_MSGHEADER_CMD, msg_len +120, send_buf, false,
					    NULL);
		if (ret) {
			kfree(send_buf);
			dev_err(priv->dev, "can not receive v400 PVT sensors value, ret=%d\n", ret);
			goto err;
		}
		recv_buf = (void *)send_buf;
		recv_buf += 3;

		for (i = 0; i < 20; i++) {
			databuf[i] = recv_buf[i + 1];
			printk("ispv4 PVT send_buf %d  =  %d", i, recv_buf[i + 1]);
		}

		if (copy_to_user(u64_to_user_ptr(cmd_data->priv), &databuf, sizeof(databuf))) {
			kfree(send_buf);
			dev_err(priv->dev, "Failed Copy to User");
			return -EFAULT;
		}
		kfree(send_buf);
		dev_info(priv->dev, "PVT sensors receive finish\n");
	}
		break;
	default:
		dev_err(priv->dev, "err para\n");
		ret = -EINVAL;
		goto err;
	}

	return 0;
err:
	dev_err(priv->dev, "ioctl failed %d\n", ret);

	return ret;
}

static long ispv4_v4l2_ioctl(struct v4l2_subdev *sd, unsigned int cmd,
			     void *arg)
{
	struct ispv4_cam_control *cmd_data;
	int32_t ret = 0;
	struct ispv4_v4l2_dev *priv = v4l2_get_subdevdata(sd);

	cmd_data = arg;
	switch (cmd) {
	case ISPV4_CAM_CTL_ISP:
		ret = ispv4_v4l2_isp_ioctl(sd, cmd_data->cmd, arg);
		break;
	case ISPV4_CAM_CTL_ASST:
		ret = ispv4_v4l2_ass_ioctl(sd, cmd_data->cmd, arg);
		break;
	default:
		dev_err(priv->dev, "Only support ISPV4_CAM_CTL_ASST %d\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

__maybe_unused static void
ispv4_v4l2_notify_asst_msg(struct ispv4_v4l2_dev *priv, uint32_t id,
			   uint32_t type)
{
	struct v4l2_event event;

	event.id = id;
	event.type = type;
	v4l2_event_queue(priv->v4l2_s_asst->devnode, &event);
}

static struct v4l2_file_operations ispv4_v4l2_fops = {
	.owner = THIS_MODULE,
	.open = ispv4_v4l2_open,
	.release = ispv4_v4l2_close,
	.unlocked_ioctl = video_ioctl2,
};

static int ispv4_vidioc_querycap(struct file *file, void *fh,
				 struct v4l2_capability *cap)
{
	struct v4l2_fh *vfh = file->private_data;

	strscpy(cap->driver, "ispv4cam", sizeof(cap->driver));
	strscpy(cap->card, vfh->vdev->name, sizeof(cap->card));
	return 0;
}

const struct v4l2_ioctl_ops ispv4_ioctl_ops = {
	.vidioc_querycap = ispv4_vidioc_querycap,
};

static int ispv4_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
				 struct v4l2_event_subscription *sub)
{
	return v4l2_event_subscribe(fh, sub, 0, NULL);
}

static int ispv4_unsubscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
				   struct v4l2_event_subscription *sub)
{
	return v4l2_event_unsubscribe(fh, sub);
}

static struct v4l2_subdev_core_ops ispv4_subdev_core_ops = {
	.ioctl = ispv4_v4l2_ioctl,
	.subscribe_event = ispv4_subscribe_event,
	.unsubscribe_event = ispv4_unsubscribe_event,
};

static struct v4l2_subdev_ops ispv4_subdev_ops = {
	.core = &ispv4_subdev_core_ops,
};

void ispv4_video_release(struct video_device *vdev)
{
	return;
}

static int ispv4_v4l2_device_setup(struct ispv4_v4l2_dev *priv)
{
	int ret = 0;

	priv->v4l2_dev =
		devm_kzalloc(priv->dev, sizeof(*priv->v4l2_dev), GFP_KERNEL);
	if (!priv->v4l2_dev)
		return -ENOMEM;

	ret = v4l2_device_register(priv->dev, priv->v4l2_dev);
	if (ret)
		goto out;

	priv->v4l2_dev->mdev = devm_kzalloc(
		priv->dev, sizeof(*priv->v4l2_dev->mdev), GFP_KERNEL);
	if (!priv->v4l2_dev->mdev) {
		ret = -ENOMEM;
		goto v4l2_fail;
	}

	media_device_init(priv->v4l2_dev->mdev);
	priv->v4l2_dev->mdev->dev = priv->dev;
	strlcpy(priv->v4l2_dev->mdev->model, ISPV4_VNODE_NAME,
		sizeof(priv->v4l2_dev->mdev->model));

	ret = media_device_register(priv->v4l2_dev->mdev);
	if (ret)
		goto media_fail;

	priv->video.v4l2_dev = priv->v4l2_dev;

	strlcpy(priv->video.name, "ispv4", sizeof(priv->video.name));
	priv->video.release = ispv4_video_release;
	priv->video.fops = &ispv4_v4l2_fops;
	priv->video.ioctl_ops = &ispv4_ioctl_ops;
	priv->video.minor = -1;
	priv->video.vfl_type = VFL_TYPE_VIDEO;
	priv->video.device_caps = V4L2_CAP_VIDEO_CAPTURE;
	ret = video_register_device(&priv->video, VFL_TYPE_VIDEO, -1);
	if (ret)
		goto video_fail;

	video_set_drvdata(&priv->video, priv);

	ret = media_entity_pads_init(&priv->video.entity, 0, NULL);
	if (ret)
		goto entity_fail;

	priv->video.entity.function = ISPV4_VIDEO_DEVICE_TYPE;
	priv->video.entity.name = video_device_node_name(&priv->video);

	priv->v4l2_s_isp = devm_kzalloc(
		priv->dev, sizeof(*priv->v4l2_s_isp) * 2, GFP_KERNEL);
	if (priv->v4l2_s_isp == NULL) {
		ret = -ENOMEM;
		goto sd_alloc_fail;
	}
	priv->v4l2_s_asst = priv->v4l2_s_isp + 1;

	v4l2_subdev_init(priv->v4l2_s_isp, &ispv4_subdev_ops);
	priv->v4l2_s_isp->internal_ops = NULL;
	snprintf(priv->v4l2_s_isp->name, V4L2_SUBDEV_NAME_SIZE, "%s", "isp");
	v4l2_set_subdevdata(priv->v4l2_s_isp, priv);

	v4l2_subdev_init(priv->v4l2_s_asst, &ispv4_subdev_ops);
	priv->v4l2_s_asst->internal_ops = NULL;
	snprintf(priv->v4l2_s_asst->name, V4L2_SUBDEV_NAME_SIZE, "%s", "asst");
	v4l2_set_subdevdata(priv->v4l2_s_asst, priv);

	priv->v4l2_s_isp->flags =
		V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	priv->v4l2_s_isp->entity.num_pads = 0;
	priv->v4l2_s_isp->entity.pads = NULL;
	priv->v4l2_s_isp->entity.function = ISPV4_ISP_DEVICE_TYPE;
	priv->v4l2_s_asst->flags =
		V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	priv->v4l2_s_asst->entity.num_pads = 0;
	priv->v4l2_s_asst->entity.pads = NULL;
	priv->v4l2_s_asst->entity.function = ISPV4_ASST_DEVICE_TYPE;

#ifdef HAS_ISP_NODE
	ret = v4l2_device_register_subdev(priv->v4l2_dev, priv->v4l2_s_isp);
	if (ret) {
		dev_err(priv->dev, "register subdev isp failed\n");
		goto reg_isp_fail;
	}
#endif

	ret = v4l2_device_register_subdev(priv->v4l2_dev, priv->v4l2_s_asst);
	if (ret) {
		dev_err(priv->dev, "register subdev asst failed\n");
		goto reg_asst_fail;
	}
	ret = v4l2_device_register_subdev_nodes(priv->v4l2_dev);
	if (ret) {
		dev_err(priv->dev, "Failed to register subdev node\n");
		goto reg_node_fail;
	}
	priv->v4l2_s_asst->entity.name =
		video_device_node_name(priv->v4l2_s_asst->devnode);
	priv->v4l2_s_isp->entity.name =
		video_device_node_name(priv->v4l2_s_isp->devnode);

	dev_info(priv->dev, "v4l2 setup finish\n");
	return 0;

reg_node_fail:
	v4l2_device_unregister_subdev(priv->v4l2_s_asst);
reg_asst_fail:
	v4l2_device_unregister_subdev(priv->v4l2_s_isp);
#ifdef HAS_ISP_NODE
reg_isp_fail:
#endif
sd_alloc_fail:
entity_fail:
	video_unregister_device(&priv->video);
video_fail:
	media_device_unregister(priv->v4l2_dev->mdev);
media_fail:
v4l2_fail:
	v4l2_device_unregister(priv->v4l2_dev);
out:
	return ret;
}

struct ispv4_v4l2_bind {
	const char *name;
	void *data;
	bool req_pci;
};

static struct ispv4_v4l2_bind submodule_first[] = {
	// { .name = "xm-ispv4-regops", .data = NULL },
	/* For config & boot pcie */
	{ .name = "ispv4_boot", .data = NULL, .req_pci = false },
	{ .name = "ispv4-ctrl", .data = NULL, .req_pci = false },
	{ .name = "ispv4_pci", .data = NULL, .req_pci = true },
};

static struct ispv4_v4l2_bind submodule_early[] = {
	{ .name = "xm-ispv4-rproc", .data = NULL, .req_pci = false },
	{ .name = "ispv4-ionmap", .data = NULL, .req_pci = true },
	{ .name = "ispv4-pmic", .data = NULL, .req_pci = false },
	// { .name = "ispv4-time_align", .data = NULL, .req_pci = true },
	{ .name = "ispv4-memdump", .data = NULL, .req_pci = false },
	/* For sof/eof irq */
	// { .name = "xiaomi_ispv4", .data = NULL }
};

static struct ispv4_v4l2_bind submodule_late[] = {
	{ .name = "ispv4-rpmsg-isp", .data = NULL, .req_pci = true },
	{ .name = "ispv4-rpmsg-asst", .data = NULL, .req_pci = true }
};

static int _compare_of(struct device *dev, void *data)
{
	struct ispv4_v4l2_bind *bd = data;
	int ret = 0;

	if (strcmp(bd->name, dev_name(dev)) == 0)
		ret = 1;

	return ret;
}

static void _release_of(struct device *dev, void *data)
{
}

static int ispv4_try_power_off(struct ispv4_v4l2_dev *priv)
{
	int ret;

	dev_info(priv->dev, "Into %s", __FUNCTION__);
	if (!priv->first_bound || !priv->early_bound) {
		dev_info(priv->dev, "Not bound finish.");
		return 0;
	}

	if (!priv->v4l2_pci.avalid) {
		dev_err(priv->dev, "v4l2_pci is not valid");
		return -EINVAL;
	}

	ret = priv->v4l2_pci.suspend_pci(priv->v4l2_pci.pcidev);
	if (ret) {
		dev_err(priv->dev, "suspend pci fail");
		priv->v4l2_pci.linkup = false;
		return -EINVAL;
	} else {
		dev_err(priv->dev, "suspend pci success");
		priv->v4l2_pci.linkup = false;
	}

	if (!priv->v4l2_ctrl.avalid)
		return -ENODEV;

	priv->v4l2_ctrl.ispv4_power_off_seq(priv->v4l2_ctrl.pdev);

	return 0;
}

static int ispv4_comp_mfirst_bind(struct device *master)
{
	struct ispv4_v4l2_dev *priv =
		container_of(master, struct ispv4_v4l2_dev, first_master);
	pr_info("ispv4 cam: %s\n", __FUNCTION__);
	component_bind_all(master, priv);
	/* bind has mutex, so it is right to lockless */
	priv->first_bound = true;
	WARN_ON(ispv4_try_power_off(priv) != 0);
	return 0;
}

static int ispv4_comp_mearly_bind(struct device *master)
{
	struct ispv4_v4l2_dev *priv =
		container_of(master, struct ispv4_v4l2_dev, early_master);
	pr_info("ispv4 cam: %s\n", __FUNCTION__);
	component_bind_all(master, priv);
	/* bind has mutex, so it is right to lockless */
	priv->early_bound = true;
	WARN_ON(ispv4_try_power_off(priv) != 0);
	return 0;
}

static int ispv4_comp_mlate_bind(struct device *master)
{
	struct ispv4_v4l2_dev *priv =
		container_of(master, struct ispv4_v4l2_dev, late_master);
	pr_info("ispv4 cam: %s\n", __FUNCTION__);
	component_bind_all(master, priv);
	return 0;

	pr_info("ispv4 cam: %s\n", __FUNCTION__);
	return 0;
}

static void ispv4_comp_mfirst_unbind(struct device *master)
{
	struct ispv4_v4l2_dev *priv =
		container_of(master, struct ispv4_v4l2_dev, first_master);
	component_unbind_all(master, priv);
	pr_info("ispv4 cam: %s\n", __FUNCTION__);
}

static void ispv4_comp_mearly_unbind(struct device *master)
{
	struct ispv4_v4l2_dev *priv =
		container_of(master, struct ispv4_v4l2_dev, early_master);
	component_unbind_all(master, priv);
	pr_info("ispv4 cam: %s\n", __FUNCTION__);
}

static void ispv4_comp_mlate_unbind(struct device *master)
{
	struct ispv4_v4l2_dev *priv =
		container_of(master, struct ispv4_v4l2_dev, late_master);
	component_unbind_all(master, priv);
	pr_info("ispv4 cam: %s\n", __FUNCTION__);
}

static const struct component_master_ops master_first_ops = {
	.bind = ispv4_comp_mfirst_bind,
	.unbind = ispv4_comp_mfirst_unbind,
};

static const struct component_master_ops master_early_ops = {
	.bind = ispv4_comp_mearly_bind,
	.unbind = ispv4_comp_mearly_unbind,
};

static const struct component_master_ops master_late_ops = {
	.bind = ispv4_comp_mlate_bind,
	.unbind = ispv4_comp_mlate_unbind,
};

static bool with_pci(struct platform_device *pdev)
{
	static bool init = false;
	static bool is_pci = false;
	if (!init) {
		is_pci = !strcmp(dev_name(&pdev->dev), "ispv4-manager-pci");
		if (!is_pci)
			dev_crit(&pdev->dev, "manager init without pci.");
		init = true;
	}
	return is_pci;
}

static int ispv4_v4l2_init_comp(struct platform_device *pdev,
				struct device *master, char *name,
				struct ispv4_v4l2_bind *bind_array, int num,
				const struct component_master_ops *ops)
{
	struct component_match *match = NULL;
	int i, ret;
	struct ispv4_v4l2_dev *priv = platform_get_drvdata(pdev);

	device_initialize(master);
	dev_set_name(master, name);

	for (i = 0; i < num; i++) {
		if (!with_pci(pdev) && bind_array[i].req_pci)
			continue;
		bind_array[i].data = priv;
		component_match_add_release(master, &match, _release_of,
					    _compare_of,
					    (void *)&bind_array[i]);
	}
	if (match) {
		ret = component_master_add_with_match(master, ops, match);
		if (ret != 0) {
			goto add_err;
		}
	}
	return 0;

add_err:
	return ret;
}

static int asst_get_loglevel(void *data, u64 *val)
{
	struct ispv4_v4l2_dev *dev = data;
	*val = dev->asst.log_level;
	return 0;
}

static int asst_set_loglevel(void *data, u64 val)
{
	struct ispv4_v4l2_dev *dev = data;
	dev->asst.log_level = val;
	asst_setloglevel(dev, val);
	return 0;
}

static int asst_set_logenable(void *data, u64 val)
{
	struct ispv4_v4l2_dev *dev = data;
	asst_logenable(dev, !!val);
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(asst_loglevel, asst_get_loglevel, asst_set_loglevel,
			 "%llu\n");

DEFINE_DEBUGFS_ATTRIBUTE(asst_logen, NULL, asst_set_logenable,
			 "%llu\n");

static void ispv4_asst_debug(struct ispv4_v4l2_dev *dev)
{
	debugfs_create_file("loglevel", 0666, dev->debugfs, dev,
			    &asst_loglevel);
	debugfs_create_file("logenable", 0666, dev->debugfs, dev,
			    &asst_logen);
}

static int ispv4_v4l2_probe(struct platform_device *pdev)
{
	struct ispv4_v4l2_dev *priv, **privp;
	int ret = -EIO;

	pr_info("entry %s\n", __FUNCTION__);

#ifndef HAS_ISP_NODE
	pr_warn("ispv4 cam dev mask v4l2-isp!!!");
#endif

	priv = devm_kzalloc(&pdev->dev, sizeof(struct ispv4_v4l2_dev),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->debugfs = debugfs_create_dir("ispv4_asst", ispv4_debugfs);
	if (IS_ERR(priv->debugfs))
		return PTR_ERR(priv->debugfs);

	ispv4_asst_debug(priv);

	priv->dev = &pdev->dev;
	platform_set_drvdata(pdev, priv);

	ret = ispv4_v4l2_device_setup(priv);
	if (ret) {
		dev_err(&pdev->dev, "ispv4-v4l2 setup failed!! %d\n", ret);
		return ret;
	}

	ret = ispv4_v4l2_init_comp(pdev, &priv->first_master,
				   "ispv4-v4l2-first", submodule_first,
				   ARRAY_SIZE(submodule_first),
				   &master_first_ops);
	if (ret) {
		dev_err(&pdev->dev, "ispv4-v4l2 init comp first failed!! %d\n",
			ret);
	}

	ret = ispv4_v4l2_init_comp(pdev, &priv->early_master,
				   "ispv4-v4l2-early", submodule_early,
				   ARRAY_SIZE(submodule_early),
				   &master_early_ops);
	if (ret) {
		dev_err(&pdev->dev, "ispv4-v4l2 init comp early failed!! %d\n",
			ret);
	}

	ret = ispv4_v4l2_init_comp(pdev, &priv->late_master, "ispv4-v4l2-late",
				   submodule_late, ARRAY_SIZE(submodule_late),
				   &master_late_ops);
	if (ret) {
		dev_err(&pdev->dev, "ispv4-v4l2 init comp early failed!! %d\n",
			ret);
	}

	privp = kzalloc(sizeof(struct ispv4_v4l2_dev *), GFP_KERNEL);
	if (privp == NULL) {
		dev_err(&pdev->dev, "ispv4-v4l2 alloc mem failed!!\n");
		return -ENOMEM;
	}
	*privp = priv;
	ispv4_qcomcam_cell.platform_data = privp;
	ispv4_qcomcam_cell.pdata_size = sizeof(struct ispv4_v4l2_dev *);
	(void)mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
			      &ispv4_qcomcam_cell, 1, NULL, 0, NULL);

	dev_info(&pdev->dev, "ispv4-v4l2 probe finish!!\n");
	return 0;
}

static int ispv4_v4l2_remove(struct platform_device *pdev)
{
	struct ispv4_v4l2_dev *priv;
	priv = platform_get_drvdata(pdev);

	debugfs_remove(priv->debugfs);
	mfd_remove_devices(&pdev->dev);
	video_unregister_device(&priv->video);
	media_device_unregister(priv->v4l2_dev->mdev);
	v4l2_device_unregister(priv->v4l2_dev);

	component_master_del(&priv->first_master, NULL);
	component_master_del(&priv->early_master, NULL);
	component_master_del(&priv->late_master, NULL);

	dev_info(&pdev->dev, "ispv4-v4l2 remove finish!!\n");

	return 0;
}

static const struct platform_device_id plat_match_table[] = {
	{ .name = "ispv4-manager-pci" },
	{ .name = "ispv4-manager-spi" },
	{},
};

static struct platform_driver ispv4_v4l2_driver = {
	.probe = ispv4_v4l2_probe,
	.remove = ispv4_v4l2_remove,
	.driver = {
		.name = "ispv4-manager",
		.owner = THIS_MODULE,
	},
	.id_table = plat_match_table,
};

module_platform_driver(ispv4_v4l2_driver);
MODULE_LICENSE("GPL v2");
