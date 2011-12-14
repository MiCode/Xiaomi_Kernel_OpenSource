/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/usb/msm_hsusb.h>
#include <mach/usb_bam.h>
#include <mach/sps.h>

#define USB_SUMMING_THRESHOLD 512
#define CONNECTIONS_NUM		4

static struct sps_bam_props usb_props;
static struct sps_pipe *sps_pipes[CONNECTIONS_NUM][2];
static struct sps_connect sps_connections[CONNECTIONS_NUM][2];
static struct sps_mem_buffer data_mem_buf[CONNECTIONS_NUM][2];
static struct sps_mem_buffer desc_mem_buf[CONNECTIONS_NUM][2];
static struct platform_device *usb_bam_pdev;

struct usb_bam_connect_info {
	u8 idx;
	u8 *src_pipe;
	u8 *dst_pipe;
	bool enabled;
};

static struct usb_bam_connect_info usb_bam_connections[CONNECTIONS_NUM];

static int connect_pipe(u8 connection_idx, enum usb_bam_pipe_dir pipe_dir,
						u8 *usb_pipe_idx)
{
	int ret;
	struct sps_pipe *pipe = sps_pipes[connection_idx][pipe_dir];
	struct sps_connect *connection =
		&sps_connections[connection_idx][pipe_dir];
	struct msm_usb_bam_platform_data *pdata =
		(struct msm_usb_bam_platform_data *)
			(usb_bam_pdev->dev.platform_data);
	struct usb_bam_pipe_connect *pipe_connection =
			(struct usb_bam_pipe_connect *)(pdata->connections +
						(2*connection_idx+pipe_dir));

	pipe = sps_alloc_endpoint();
	if (pipe == NULL) {
		pr_err("%s: sps_alloc_endpoint failed\n", __func__);
		return -ENOMEM;
	}

	ret = sps_get_config(pipe, connection);
	if (ret) {
		pr_err("%s: tx get config failed %d\n", __func__, ret);
		goto get_config_failed;
	}

	ret = sps_phy2h(pipe_connection->src_phy_addr, &(connection->source));
	if (ret) {
		pr_err("%s: sps_phy2h failed (src BAM) %d\n", __func__, ret);
		goto get_config_failed;
	}

	connection->src_pipe_index = pipe_connection->src_pipe_index;
	ret = sps_phy2h(pipe_connection->dst_phy_addr,
					&(connection->destination));
	if (ret) {
		pr_err("%s: sps_phy2h failed (dst BAM) %d\n", __func__, ret);
		goto get_config_failed;
	}
	connection->dest_pipe_index = pipe_connection->dst_pipe_index;

	if (pipe_dir == USB_TO_PEER_PERIPHERAL) {
		connection->mode = SPS_MODE_SRC;
		*usb_pipe_idx = connection->src_pipe_index;
	} else {
		connection->mode = SPS_MODE_DEST;
		*usb_pipe_idx = connection->dest_pipe_index;
	}

	ret = sps_setup_bam2bam_fifo(
				&data_mem_buf[connection_idx][pipe_dir],
				pipe_connection->data_fifo_base_offset,
				pipe_connection->data_fifo_size, 1);
	if (ret) {
		pr_err("%s: data fifo setup failure %d\n", __func__, ret);
		goto fifo_setup_error;
	}
	connection->data = data_mem_buf[connection_idx][pipe_dir];

	ret = sps_setup_bam2bam_fifo(
				&desc_mem_buf[connection_idx][pipe_dir],
				pipe_connection->desc_fifo_base_offset,
				pipe_connection->desc_fifo_size, 1);
	if (ret) {
		pr_err("%s: desc. fifo setup failure %d\n", __func__, ret);
		goto fifo_setup_error;
	}
	connection->desc = desc_mem_buf[connection_idx][pipe_dir];
	connection->event_thresh = 512;

	ret = sps_connect(pipe, connection);
	if (ret < 0) {
		pr_err("%s: tx connect error %d\n", __func__, ret);
		goto error;
	}
	return 0;

error:
	sps_disconnect(pipe);
fifo_setup_error:
get_config_failed:
	sps_free_endpoint(pipe);
	return ret;
}

int usb_bam_connect(u8 idx, u8 *src_pipe_idx, u8 *dst_pipe_idx)
{
	struct usb_bam_connect_info *connection = &usb_bam_connections[idx];
	int ret;

	if (idx >= CONNECTIONS_NUM) {
		pr_err("%s: Invalid connection index\n",
			__func__);
		return -EINVAL;
	}

	if (connection->enabled) {
		pr_info("%s: connection %d was already established\n",
			__func__, idx);
		return 0;
	}
	connection->src_pipe = src_pipe_idx;
	connection->dst_pipe = dst_pipe_idx;
	connection->idx = idx;

	/* open USB -> Peripheral pipe */
	ret = connect_pipe(connection->idx, USB_TO_PEER_PERIPHERAL,
					   connection->src_pipe);
	if (ret) {
		pr_err("%s: src pipe connection failure\n", __func__);
		return ret;
	}
	/* open Peripheral -> USB pipe */
	ret = connect_pipe(connection->idx, PEER_PERIPHERAL_TO_USB,
				 connection->dst_pipe);
	if (ret) {
		pr_err("%s: dst pipe connection failure\n", __func__);
		return ret;
	}
	connection->enabled = 1;

	return 0;
}
static int usb_bam_init(void)
{
	u32 h_usb;
	int ret;
	void *usb_virt_addr;
	struct msm_usb_bam_platform_data *pdata =
		(struct msm_usb_bam_platform_data *)
			(usb_bam_pdev->dev.platform_data);

	usb_virt_addr = ioremap_nocache(
						pdata->usb_bam_phy_base,
						pdata->usb_bam_phy_size);
	if (!usb_virt_addr) {
		pr_err("%s: ioremap failed\n", __func__);
		return -ENOMEM;
	}
	usb_props.phys_addr = pdata->usb_bam_phy_base;
	usb_props.virt_addr = usb_virt_addr;
	usb_props.virt_size = pdata->usb_bam_phy_size;
	usb_props.irq = USB1_HS_BAM_IRQ;
	usb_props.num_pipes = pdata->usb_bam_num_pipes;
	usb_props.summing_threshold = USB_SUMMING_THRESHOLD;
	ret = sps_register_bam_device(&usb_props, &h_usb);
	if (ret < 0) {
		pr_err("%s: register bam error %d\n", __func__, ret);
		return -EFAULT;
	}

	return 0;
}

static int usb_bam_probe(struct platform_device *pdev)
{
	int ret, i;

	dev_dbg(&pdev->dev, "usb_bam_probe\n");

	for (i = 0; i < CONNECTIONS_NUM; i++)
		usb_bam_connections[i].enabled = 0;

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "missing platform_data\n");
		return -ENODEV;
	}
	usb_bam_pdev = pdev;

	ret = usb_bam_init();
	if (ret) {
		dev_err(&pdev->dev, "failed to get platform resource mem\n");
		return ret;
	}

	return 0;
}

static struct platform_driver usb_bam_driver = {
	.probe = usb_bam_probe,
	.driver = { .name = "usb_bam", },
};

static int __init init(void)
{
	return platform_driver_register(&usb_bam_driver);
}
module_init(init);

static void __exit cleanup(void)
{
	platform_driver_unregister(&usb_bam_driver);
}
module_exit(cleanup);

MODULE_DESCRIPTION("MSM USB BAM DRIVER");
MODULE_LICENSE("GPL v2");
