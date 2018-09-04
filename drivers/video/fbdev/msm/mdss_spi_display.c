/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/leds.h>
#include <linux/pwm.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>

#include "mdss_panel.h"
#include "mdss_spi_panel.h"
#include "mdss_spi_client.h"
#include "mdss_mdp.h"

static int mdss_spi_get_img(struct spi_panel_data *ctrl_pdata,
			struct mdp_layer_commit_v1 *commit, struct device *dev)
{
	struct msmfb_data image;
	struct dma_buf *dmabuf;
	void *vaddr;

	memset(&image, 0, sizeof(image));
	image.memory_id = commit->input_layers[0].buffer.planes[0].fd;
	image.offset = commit->input_layers[0].buffer.planes[0].offset;

	dmabuf = dma_buf_get(image.memory_id);
	if (IS_ERR(dmabuf)) {
		pr_err("%s : error on dma_buf_get\n", __func__);
		return PTR_ERR(dmabuf);
	}
	ctrl_pdata->image_data.srcp_attachment =
				dma_buf_attach(dmabuf, dev);
	if (IS_ERR(ctrl_pdata->image_data.srcp_attachment))
		goto err_put;

	ctrl_pdata->image_data.srcp_table =
		dma_buf_map_attachment(ctrl_pdata->image_data.srcp_attachment,
		DMA_TO_DEVICE);
	if (IS_ERR(ctrl_pdata->image_data.srcp_table))
		goto err_detach;

	dma_buf_begin_cpu_access(dmabuf, DMA_TO_DEVICE);

	vaddr  = dma_buf_kmap(dmabuf, 0);
	if (!vaddr) {
		pr_err("%s:ion memory mapping failed\n", __func__);
		goto err_unmap;
	};

	ctrl_pdata->image_data.addr = vaddr;
	ctrl_pdata->image_data.len = dmabuf->size;
	ctrl_pdata->image_data.mapped = true;
	ctrl_pdata->image_data.srcp_dma_buf = dmabuf;

	return 0;
err_unmap:
	dma_buf_unmap_attachment(ctrl_pdata->image_data.srcp_attachment,
		ctrl_pdata->image_data.srcp_table, DMA_BIDIRECTIONAL);
err_detach:
	dma_buf_detach(ctrl_pdata->image_data.srcp_dma_buf,
			ctrl_pdata->image_data.srcp_attachment);
err_put:
	dma_buf_put(ctrl_pdata->image_data.srcp_dma_buf);
	return -EINVAL;
}

static void mdss_spi_put_img(struct spi_panel_data *ctrl_pdata)
{
	if (!ctrl_pdata->image_data.mapped)
		return;
	dma_buf_kunmap(ctrl_pdata->image_data.srcp_dma_buf, 0,
				ctrl_pdata->image_data.addr);
	dma_buf_end_cpu_access(ctrl_pdata->image_data.srcp_dma_buf,
				DMA_BIDIRECTIONAL);
	dma_buf_unmap_attachment(ctrl_pdata->image_data.srcp_attachment,
			ctrl_pdata->image_data.srcp_table, DMA_TO_DEVICE);
	dma_buf_detach(ctrl_pdata->image_data.srcp_dma_buf,
				ctrl_pdata->image_data.srcp_attachment);
	dma_buf_put(ctrl_pdata->image_data.srcp_dma_buf);

	ctrl_pdata->image_data.srcp_dma_buf = NULL;
	ctrl_pdata->image_data.addr = NULL;
	ctrl_pdata->image_data.len = 0;
	ctrl_pdata->image_data.mapped = false;
}

int mdss_spi_display_pre_commit(struct msm_fb_data_type *mfd,
	struct file *file, struct mdp_layer_commit_v1 *commit)
{
	char *temp_buf;
	int rc = 0, scan_count = 0;
	int panel_yres, panel_xres;
	int padding_length, byte_per_pixel;
	int dma_stride, actual_stride;
	struct mdss_panel_data *pdata;
	struct spi_panel_data *ctrl_pdata = NULL;

	if (commit->input_layer_cnt == 0) {
		pr_err("SPI display doesn't support NULL commit\n");
		return 0;
	}

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl_pdata = container_of(pdata, struct spi_panel_data, panel_data);

	rc = mdss_spi_get_img(ctrl_pdata, commit, &mfd->pdev->dev);
	if (rc) {
		pr_err("mdss_spi_get_img failed\n");
		return rc;
	}

	panel_xres = ctrl_pdata->panel_data.panel_info.xres;
	panel_yres = ctrl_pdata->panel_data.panel_info.yres;
	dma_stride = mfd->fbi->fix.line_length;
	byte_per_pixel = ctrl_pdata->panel_data.panel_info.bpp / 8;
	actual_stride = panel_xres * byte_per_pixel;
	padding_length = dma_stride - actual_stride;

	/* remove padding and copy to continuous buffer */
	while (scan_count < panel_yres) {
		memcpy((ctrl_pdata->back_buf + scan_count * actual_stride),
			(ctrl_pdata->image_data.addr + scan_count *
			(actual_stride + padding_length)), actual_stride);
		scan_count++;
	}

	mdss_spi_put_img(ctrl_pdata);

	/* wait for SPI transfer done */
	rc = mdss_spi_wait_tx_done(ctrl_pdata);
	if (!rc) {
		pr_err("SPI transfer timeout\n");
		return -EINVAL;
	}

	/* swap buffer */
	temp_buf = ctrl_pdata->front_buf;
	ctrl_pdata->front_buf = ctrl_pdata->back_buf;
	ctrl_pdata->back_buf = temp_buf;

	return 0;
}

int mdss_spi_display_atomic_validate(struct msm_fb_data_type *mfd,
	struct file *file, struct mdp_layer_commit_v1 *commit)
{
	struct mdss_panel_data *pdata;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	if ((commit->input_layers->dst_rect.w != pdata->panel_info.xres) &&
		(commit->input_layers->dst_rect.h != pdata->panel_info.yres) &&
		(commit->input_layer_cnt > 1)) {
		WARN_ONCE(1, "%s:Only support GPU composition layer_cnt %d\n",
				__func__, commit->input_layer_cnt);
		return -EINVAL;
	}

	if (commit->input_layers[0].buffer.format != MDP_RGB_565) {
		WARN_ONCE(1, "%s:SPI display only support RGB565 format %d\n",
			__func__, commit->input_layers[0].buffer.format);
		return -EINVAL;
	}

	return 0;
}

int mdss_spi_panel_kickoff(struct msm_fb_data_type *mfd,
			struct mdp_display_commit *data)
{
	struct spi_panel_data *ctrl_pdata = NULL;
	struct mdss_panel_data *pdata;
	int rc = 0;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (WARN_ON(!pdata))
		return -EINVAL;

	ctrl_pdata = container_of(pdata, struct spi_panel_data, panel_data);

	enable_spi_panel_te_irq(ctrl_pdata, true);
	mutex_lock(&ctrl_pdata->spi_tx_mutex);
	reinit_completion(&ctrl_pdata->spi_panel_te);
	atomic_inc(&ctrl_pdata->koff_cnt);

	rc = wait_for_completion_timeout(&ctrl_pdata->spi_panel_te,
				msecs_to_jiffies(SPI_PANEL_TE_TIMEOUT));
	if (rc == 0) {
		pr_err("wait panel TE time out\n");
		mutex_unlock(&ctrl_pdata->spi_tx_mutex);
		return rc;
	}

	rc = mdss_spi_tx_pixel(ctrl_pdata->front_buf,
				ctrl_pdata->byte_per_frame,
				mdss_spi_tx_fb_complete, ctrl_pdata);

	mutex_unlock(&ctrl_pdata->spi_tx_mutex);
	enable_spi_panel_te_irq(ctrl_pdata, false);

	return rc;
}

static int spi_display_get_metadata(struct msm_fb_data_type *mfd,
				struct msmfb_metadata *metadata)
{
	int ret = 0;

	switch (metadata->op) {
	case metadata_op_frame_rate:
		metadata->data.panel_frame_rate =
			mfd->panel_info->spi.frame_rate;
		break;
	case metadata_op_get_caps:
		metadata->data.caps.mdp_rev = 5;
		metadata->data.caps.rgb_pipes = 0;
		metadata->data.caps.vig_pipes = 0;
		metadata->data.caps.dma_pipes = 1;
		break;

	default:
		pr_warn("Unsupported request to GET META IOCTL %d\n",
			metadata->op);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int spi_display_ioctl_handler(struct msm_fb_data_type *mfd,
					  u32 cmd, void __user *argp)
{
	int val, ret = 0;
	struct mdss_panel_data *pdata;
	struct msmfb_metadata metadata;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	switch (cmd) {
	case MSMFB_OVERLAY_VSYNC_CTRL:
		if (!copy_from_user(&val, argp, sizeof(val))) {
			mdss_spi_vsync_enable(pdata, val);
		} else {
			pr_err("overlay vsync ctrl copy from user failed\n");
			ret = -EFAULT;
		}
		break;
	case MSMFB_METADATA_GET:
		ret = copy_from_user(&metadata, argp, sizeof(metadata));
		if (ret) {
			pr_err("get metadata from user failed (%d)\n", ret);
			break;
		}
		ret = spi_display_get_metadata(mfd, &metadata);
		if (ret) {
			pr_err("spi_display_get_metadata failed (%d)\n", ret);
			break;
		}
		ret = copy_to_user(argp, &metadata, sizeof(metadata));
		if (ret)
			pr_err("copy to user failed (%d)\n", ret);
		break;
	default:
		break;
	}

	return ret;
}

static int mdss_spi_display_off(struct msm_fb_data_type *mfd)
{
	int rc = 0;
	struct mdss_panel_data *pdata;
	struct spi_panel_data *ctrl_pdata = NULL;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	ctrl_pdata = container_of(pdata, struct spi_panel_data, panel_data);

	ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_ACTIVE;

	if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
		rc = mdss_spi_panel_off(&ctrl_pdata->panel_data);
		if (rc) {
			pr_err("%s: Panel off failed\n", __func__);
			return rc;
		}
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
	}
	rc = mdss_spi_panel_power_ctrl(pdata, MDSS_PANEL_POWER_OFF);

	return rc;
}

static int mdss_spi_display_on(struct msm_fb_data_type *mfd)
{
	int rc = 0;
	struct mdss_panel_data *pdata;
	struct spi_panel_data *ctrl_pdata = NULL;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	ctrl_pdata = container_of(pdata, struct spi_panel_data, panel_data);

	rc = mdss_spi_panel_power_ctrl(pdata, MDSS_PANEL_POWER_ON);
	if (rc) {
		pr_err("%s:Panel power on failed. rc=%d\n",
			__func__, rc);
		return rc;
	}

	mdss_spi_panel_pinctrl_set_state(ctrl_pdata, true);
	mdss_spi_panel_reset(pdata, 1);
	ctrl_pdata->ctrl_state |= CTRL_STATE_PANEL_ACTIVE;
	rc = mdss_spi_panel_on(&ctrl_pdata->panel_data);
	return rc;
}

u32 mdss_spi_display_fb_stride(u32 fb_index, u32 xres, int bpp)
{
	/*
	 * The adreno GPU hardware requires that the pitch be aligned to
	 * 32 pixels for color buffers, so for the cases where the GPU
	 * is writing directly to fb0, the framebuffer pitch
	 * also needs to be 32 pixels aligned
	 */

	if (fb_index == 0)
		return ALIGN(xres, 32) * bpp;
	else
		return xres * bpp;
}

ssize_t mdss_spi_show_capabilities(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t len = PAGE_SIZE;
	int cnt = 0;

	cnt += scnprintf(buf + cnt, len - cnt, "mdp_version=5\n");
	cnt += scnprintf(buf + cnt, len - cnt, "hw_rev=%d\n", 5);
	cnt += scnprintf(buf + cnt, len - cnt, "pipe_count:%d\n", 1);
	cnt += scnprintf(buf + cnt, len - cnt,
		"pipe_num:3 pipe_type:rgb pipe_ndx:8 rects:1 pipe_is_handoff:0"
		);
	cnt += scnprintf(buf + cnt, len - cnt,
		"display_id:0 fmts_supported:51,224,0,22,0,191,248,255,1,");
	cnt += scnprintf(buf + cnt, len - cnt,
		"0,0,0,0,0,,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\n");
	cnt += scnprintf(buf + cnt, len - cnt, "rgb_pipes=%d\n", 0);
	cnt += scnprintf(buf + cnt, len - cnt, "vig_pipes=%d\n", 0);
	cnt += scnprintf(buf + cnt, len - cnt, "dma_pipes=%d\n", 1);
	cnt += scnprintf(buf + cnt, len - cnt, "blending_stages=%d\n", 2);
	cnt += scnprintf(buf + cnt, len - cnt, "cursor_pipes=%d\n", 0);
	cnt += scnprintf(buf + cnt, len - cnt, "max_cursor_size=%d\n", 0);
	cnt += scnprintf(buf + cnt, len - cnt, "smp_count=%d\n", 0);
	cnt += scnprintf(buf + cnt, len - cnt, "smp_size=%d\n", 0);
	cnt += scnprintf(buf + cnt, len - cnt, "smp_mb_per_pipe=%d\n", 0);
	cnt += scnprintf(buf + cnt, len - cnt, "max_bandwidth_low=3100000\n");
	cnt += scnprintf(buf + cnt, len - cnt, "max_bandwidth_high=3100000\n");
	cnt += scnprintf(buf + cnt, len - cnt, "max_pipe_width=2048\n");
	cnt += scnprintf(buf + cnt, len - cnt, "max_mixer_width=2048\n");
	cnt += scnprintf(buf + cnt, len - cnt, "max_bandwidth_low=3100000\n");
	cnt += scnprintf(buf + cnt, len - cnt, "max_pipe_bw=2300000\n");
	cnt += scnprintf(buf + cnt, len - cnt, "max_mdp_clk=320000000\n");
	cnt += scnprintf(buf + cnt, len - cnt, "rot_dwnscale_min=1\n");
	cnt += scnprintf(buf + cnt, len - cnt, "rot_dwnscale_max=1\n");
	cnt += scnprintf(buf + cnt, len - cnt, "max_downscale_ratio=1\n");
	cnt += scnprintf(buf + cnt, len - cnt, "max_upscale_ratio=1\n");

	return cnt;
}

static ssize_t mdss_spi_vsync_show_event(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata;
	struct spi_panel_data *ctrl_pdata = NULL;
	int rc = 0;
	u64 vsync_ticks;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl_pdata = container_of(pdata, struct spi_panel_data, panel_data);

	if (!(ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_ACTIVE))
		return -EAGAIN;

	vsync_ticks = ktime_to_ns(ctrl_pdata->vsync_time);
	pr_debug("fb%d vsync=%llu\n", mfd->index, vsync_ticks);
	rc = scnprintf(buf, PAGE_SIZE, "VSYNC=%llu\n", vsync_ticks);

	return rc;
}

static DEVICE_ATTR(vsync_event, 0444, mdss_spi_vsync_show_event, NULL);
static DEVICE_ATTR(caps, 0444, mdss_spi_show_capabilities, NULL);

static struct attribute *mdp_spi_sysfs_attrs[] = {
	&dev_attr_caps.attr,
	NULL,
};

static struct attribute *spi_vsync_fs_attr_group[] = {
	&dev_attr_vsync_event.attr,
	NULL,
};

static struct attribute_group mdp_spi_sysfs_group = {
	.attrs = mdp_spi_sysfs_attrs,
};

static struct attribute_group spi_vsync_sysfs_group = {
	.attrs = spi_vsync_fs_attr_group,
};

int mdss_spi_overlay_init(struct msm_fb_data_type *mfd)
{
	struct msm_mdp_interface *spi_display_interface = &mfd->mdp;
	struct device *dev = mfd->fbi->dev;
	struct mdss_data_type *spi_mdata;
	struct mdss_panel_data *pdata;
	struct spi_panel_data *ctrl_pdata = NULL;
	int rc = 0;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl_pdata = container_of(pdata, struct spi_panel_data, panel_data);

	spi_mdata = dev_get_drvdata(mfd->pdev->dev.parent);
	if (!spi_mdata) {
		pr_err("unable to initialize spi mdata for fb%d\n", mfd->index);
		return -ENODEV;
	}

	spi_display_interface->on_fnc = mdss_spi_display_on;
	spi_display_interface->off_fnc = mdss_spi_display_off;
	spi_display_interface->do_histogram = NULL;
	spi_display_interface->cursor_update = NULL;

	spi_display_interface->ioctl_handler = spi_display_ioctl_handler;
	spi_display_interface->kickoff_fnc = mdss_spi_panel_kickoff;
	spi_display_interface->pre_commit = mdss_spi_display_pre_commit;
	spi_display_interface->atomic_validate =
				mdss_spi_display_atomic_validate;
	spi_display_interface->fb_mem_get_iommu_domain = NULL;
	spi_display_interface->fb_stride = mdss_spi_display_fb_stride;
	spi_display_interface->fb_mem_alloc_fnc = NULL;
	spi_display_interface->check_dsi_status = NULL;

	rc = sysfs_create_group(&dev->kobj, &spi_vsync_sysfs_group);
	if (rc)
		pr_err("spi vsync sysfs group creation failed, ret=%d\n", rc);

	rc = sysfs_create_link_nowarn(&dev->kobj,
			&spi_mdata->pdev->dev.kobj, "mdp");

	ctrl_pdata->vsync_event_sd = sysfs_get_dirent(dev->kobj.sd,
			"vsync_event");
	if (!ctrl_pdata->vsync_event_sd)
		pr_err("spi vsync_event sysfs lookup failed\n");

	return rc;
}

static int mdss_spi_display_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct mdss_data_type *mdata;
	static struct msm_mdp_interface spi_display_interface = {
		.init_fnc = mdss_spi_overlay_init,
		.fb_stride = mdss_spi_display_fb_stride,
	};
	struct device *dev = &pdev->dev;

	if (!pdev->dev.of_node) {
		pr_err("spi display driver only supports device tree probe\n");
		return -ENOTSUPP;
	}

	mdata = devm_kzalloc(&pdev->dev, sizeof(*mdata), GFP_KERNEL);
	if (mdata == NULL)
		return -ENOMEM;

	pdev->id = 0;
	mdata->pdev = pdev;
	platform_set_drvdata(pdev, mdata);

	rc = mdss_fb_register_mdp_instance(&spi_display_interface);
	if (rc) {
		pr_err("unable to register SPI display instance\n");
		return rc;
	}

	rc = sysfs_create_group(&dev->kobj, &mdp_spi_sysfs_group);
	if (rc) {
		pr_err("spi vsync sysfs group creation failed, ret=%d\n", rc);
		return rc;
	}

	return 0;
}

static const struct of_device_id mdss_spi_display_match[] = {
	{ .compatible = "qcom,mdss-spi-display" },
	{},
};

static struct platform_driver this_driver = {
	.probe = mdss_spi_display_probe,
	.driver = {
		.name = "spi_display",
		.owner  = THIS_MODULE,
		.of_match_table = mdss_spi_display_match,
	},
};

static int __init mdss_spi_display_init(void)
{
	int ret;

	ret = platform_driver_register(&this_driver);
	return ret;
}

module_init(mdss_spi_display_init);
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, mdss_spi_display_match);
