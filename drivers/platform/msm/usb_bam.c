/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/usb/msm_hsusb.h>
#include <mach/usb_bam.h>
#include <mach/sps.h>
#include <linux/workqueue.h>

#define USB_SUMMING_THRESHOLD 512
#define CONNECTIONS_NUM	4

static struct sps_bam_props usb_props;
static struct sps_pipe *sps_pipes[CONNECTIONS_NUM][2];
static struct sps_connect sps_connections[CONNECTIONS_NUM][2];
static struct sps_mem_buffer data_mem_buf[CONNECTIONS_NUM][2];
static struct sps_mem_buffer desc_mem_buf[CONNECTIONS_NUM][2];
static struct platform_device *usb_bam_pdev;
static struct workqueue_struct *usb_bam_wq;

struct usb_bam_wake_event_info {
	struct sps_register_event event;
	int (*callback)(void *);
	void *param;
	struct work_struct wake_w;
};

struct usb_bam_connect_info {
	u8 idx;
	u32 *src_pipe;
	u32 *dst_pipe;
	struct usb_bam_wake_event_info peer_event;
	bool enabled;
};

static struct usb_bam_connect_info usb_bam_connections[CONNECTIONS_NUM];
static struct usb_bam_pipe_connect ***msm_usb_bam_connections_info;
static struct usb_bam_pipe_connect *bam_connection_arr;

static bool device_tree_enabled;

static inline int bam_offset(struct msm_usb_bam_platform_data *pdata)
{
	return pdata->usb_active_bam * CONNECTIONS_NUM * 2;
}

static int connect_pipe(u8 conn_idx, enum usb_bam_pipe_dir pipe_dir,
						u32 *usb_pipe_idx)
{
	int ret;
	struct sps_pipe **pipe = &sps_pipes[conn_idx][pipe_dir];
	struct sps_connect *connection =
		&sps_connections[conn_idx][pipe_dir];
	struct msm_usb_bam_platform_data *pdata =
		usb_bam_pdev->dev.platform_data;
	struct usb_bam_pipe_connect *pipe_connection =
		(struct usb_bam_pipe_connect *)(pdata->connections +
			 bam_offset(pdata) + (2*conn_idx+pipe_dir));

	*pipe = sps_alloc_endpoint();
	if (*pipe == NULL) {
		pr_err("%s: sps_alloc_endpoint failed\n", __func__);
		return -ENOMEM;
	}

	ret = sps_get_config(*pipe, connection);
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

	if (!device_tree_enabled) {
		ret = sps_setup_bam2bam_fifo(
				&data_mem_buf[conn_idx][pipe_dir],
				pipe_connection->data_fifo_base_offset,
				pipe_connection->data_fifo_size, 1);
		if (ret) {
			pr_err("%s: data fifo setup failure %d\n", __func__,
				ret);
			goto fifo_setup_error;
		}

		ret = sps_setup_bam2bam_fifo(
				&desc_mem_buf[conn_idx][pipe_dir],
				pipe_connection->desc_fifo_base_offset,
				pipe_connection->desc_fifo_size, 1);
		if (ret) {
			pr_err("%s: desc. fifo setup failure %d\n", __func__,
				ret);
			goto fifo_setup_error;
		}
	} else {
		data_mem_buf[conn_idx][pipe_dir].phys_base =
			pipe_connection->data_fifo_base_offset +
				pdata->usb_base_address;
		data_mem_buf[conn_idx][pipe_dir].size =
			pipe_connection->data_fifo_size;
		data_mem_buf[conn_idx][pipe_dir].base =
			ioremap(data_mem_buf[conn_idx][pipe_dir].phys_base,
				data_mem_buf[conn_idx][pipe_dir].size);
		memset(data_mem_buf[conn_idx][pipe_dir].base, 0,
			data_mem_buf[conn_idx][pipe_dir].size);

		desc_mem_buf[conn_idx][pipe_dir].phys_base =
			pipe_connection->desc_fifo_base_offset +
				pdata->usb_base_address;
		desc_mem_buf[conn_idx][pipe_dir].size =
			pipe_connection->desc_fifo_size;
		desc_mem_buf[conn_idx][pipe_dir].base =
			ioremap(desc_mem_buf[conn_idx][pipe_dir].phys_base,
				desc_mem_buf[conn_idx][pipe_dir].size);
		memset(desc_mem_buf[conn_idx][pipe_dir].base, 0,
			desc_mem_buf[conn_idx][pipe_dir].size);
	}

	connection->data = data_mem_buf[conn_idx][pipe_dir];
	connection->desc = desc_mem_buf[conn_idx][pipe_dir];
	connection->event_thresh = 16;
	connection->options = SPS_O_AUTO_ENABLE;

	ret = sps_connect(*pipe, connection);
	if (ret < 0) {
		pr_err("%s: tx connect error %d\n", __func__, ret);
		goto error;
	}
	return 0;

error:
	sps_disconnect(*pipe);
fifo_setup_error:
get_config_failed:
	sps_free_endpoint(*pipe);
	return ret;
}


static int disconnect_pipe(u8 connection_idx, enum usb_bam_pipe_dir pipe_dir,
						u32 *usb_pipe_idx)
{
	struct sps_pipe *pipe = sps_pipes[connection_idx][pipe_dir];
	struct sps_connect *connection =
		&sps_connections[connection_idx][pipe_dir];

	sps_disconnect(pipe);
	sps_free_endpoint(pipe);

	connection->options &= ~SPS_O_AUTO_ENABLE;
	return 0;
}

int usb_bam_connect(u8 idx, u32 *src_pipe_idx, u32 *dst_pipe_idx)
{
	struct usb_bam_connect_info *connection = &usb_bam_connections[idx];
	int ret;

	if (idx >= CONNECTIONS_NUM) {
		pr_err("%s: Invalid connection index\n",
			__func__);
		return -EINVAL;
	}

	if (connection->enabled) {
		pr_debug("%s: connection %d was already established\n",
			__func__, idx);
		return 0;
	}
	connection->src_pipe = src_pipe_idx;
	connection->dst_pipe = dst_pipe_idx;
	connection->idx = idx;

	if (src_pipe_idx) {
		/* open USB -> Peripheral pipe */
		ret = connect_pipe(connection->idx, USB_TO_PEER_PERIPHERAL,
			connection->src_pipe);
		if (ret) {
			pr_err("%s: src pipe connection failure\n", __func__);
			return ret;
		}
	}
	if (dst_pipe_idx) {
		/* open Peripheral -> USB pipe */
		ret = connect_pipe(connection->idx, PEER_PERIPHERAL_TO_USB,
			connection->dst_pipe);
		if (ret) {
			pr_err("%s: dst pipe connection failure\n", __func__);
			return ret;
		}
	}
	connection->enabled = 1;

	return 0;
}

static void usb_bam_wake_work(struct work_struct *w)
{
	struct usb_bam_wake_event_info *wake_event_info =
		container_of(w, struct usb_bam_wake_event_info, wake_w);

	wake_event_info->callback(wake_event_info->param);
}

static void usb_bam_wake_cb(struct sps_event_notify *notify)
{
	struct usb_bam_wake_event_info *wake_event_info =
		(struct usb_bam_wake_event_info *)notify->user;

	queue_work(usb_bam_wq, &wake_event_info->wake_w);
}

int usb_bam_register_wake_cb(u8 idx,
	int (*callback)(void *user), void* param)
{
	struct sps_pipe *pipe = sps_pipes[idx][PEER_PERIPHERAL_TO_USB];
	struct sps_connect *sps_connection =
		&sps_connections[idx][PEER_PERIPHERAL_TO_USB];
	struct usb_bam_connect_info *connection = &usb_bam_connections[idx];
	struct usb_bam_wake_event_info *wake_event_info =
		&connection->peer_event;
	int ret;

	wake_event_info->param = param;
	wake_event_info->callback = callback;
	wake_event_info->event.mode = SPS_TRIGGER_CALLBACK;
	wake_event_info->event.xfer_done = NULL;
	wake_event_info->event.callback = callback ? usb_bam_wake_cb : NULL;
	wake_event_info->event.user = wake_event_info;
	wake_event_info->event.options = SPS_O_WAKEUP;
	ret = sps_register_event(pipe, &wake_event_info->event);
	if (ret) {
		pr_err("%s: sps_register_event() failed %d\n", __func__, ret);
		return ret;
	}

	sps_connection->options = callback ?
		(SPS_O_AUTO_ENABLE | SPS_O_WAKEUP | SPS_O_WAKEUP_IS_ONESHOT) :
			SPS_O_AUTO_ENABLE;
	ret = sps_set_config(pipe, sps_connection);
	if (ret) {
		pr_err("%s: sps_set_config() failed %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

int usb_bam_disconnect_pipe(u8 idx)
{
	struct usb_bam_connect_info *connection = &usb_bam_connections[idx];
	int ret;

	if (idx >= CONNECTIONS_NUM) {
		pr_err("%s: Invalid connection index\n",
			__func__);
		return -EINVAL;
	}

	if (!connection->enabled) {
		pr_debug("%s: connection %d isn't enabled\n",
			__func__, idx);
		return 0;
	}

	if (connection->src_pipe) {
		/* close USB -> Peripheral pipe */
		ret = disconnect_pipe(connection->idx, USB_TO_PEER_PERIPHERAL,
						   connection->src_pipe);
		if (ret) {
			pr_err("%s: src pipe connection failure\n", __func__);
			return ret;
		}

	}
	if (connection->dst_pipe) {
		/* close Peripheral -> USB pipe */
		ret = disconnect_pipe(connection->idx, PEER_PERIPHERAL_TO_USB,
			connection->dst_pipe);
		if (ret) {
			pr_err("%s: dst pipe connection failure\n", __func__);
			return ret;
		}
	}

	connection->src_pipe = 0;
	connection->dst_pipe = 0;
	connection->enabled = 0;

	return 0;
}

static int update_connections_info(struct device_node *node, int bam,
	int conn_num, int dir)
{
	u32 rc;
	char *key = NULL;
	uint32_t val = 0;

	struct usb_bam_pipe_connect *pipe_connection;

	pipe_connection = &msm_usb_bam_connections_info[bam][conn_num][dir];

	key = "qcom,src-bam-physical-address";
	rc = of_property_read_u32(node, key, &val);
	if (rc)
		goto err;
	pipe_connection->src_phy_addr = val;

	key = "qcom,src-bam-pipe-index";
	rc = of_property_read_u32(node, key, &val);
	if (rc)
		goto err;
	pipe_connection->src_pipe_index = val;

	key = "qcom,dst-bam-physical-address";
	rc = of_property_read_u32(node, key, &val);
	if (rc)
		goto err;
	pipe_connection->dst_phy_addr = val;

	key = "qcom,dst-bam-pipe-index";
	rc = of_property_read_u32(node, key, &val);
	if (rc)
		goto err;
	pipe_connection->dst_pipe_index = val;

	key = "qcom,data-fifo-offset";
	rc = of_property_read_u32(node, key, &val);
	if (rc)
		goto err;
	pipe_connection->data_fifo_base_offset = val;

	key = "qcom,data-fifo-size";
	rc = of_property_read_u32(node, key, &val);
	if (rc)
		goto err;
	pipe_connection->data_fifo_size = val;

	key = "qcom,descriptor-fifo-offset";
	rc = of_property_read_u32(node, key, &val);
	if (rc)
		goto err;
	pipe_connection->desc_fifo_base_offset = val;

	key = "qcom,descriptor-fifo-size";
	rc = of_property_read_u32(node, key, &val);
	if (rc)
		goto err;
	pipe_connection->desc_fifo_size = val;

	return 0;

err:
	pr_err("%s: Error in name %s key %s\n", __func__,
		node->full_name, key);
	return -EFAULT;
}

static struct msm_usb_bam_platform_data *usb_bam_dt_to_pdata(
	struct platform_device *pdev)
{
	struct msm_usb_bam_platform_data *pdata;
	struct device_node *node = pdev->dev.of_node;
	u32 i, j;
	int conn_num, bam;
	u8 dir;
	u8 ncolumns = 2;
	int bam_amount, rc = 0;
	u32 pipe_entry = 0;
	char *key = NULL;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("unable to allocate platform data\n");
		return NULL;
	}

	rc = of_property_read_u32(node, "qcom,usb-active-bam",
		&pdata->usb_active_bam);
	if (rc) {
		pr_err("Invalid usb active bam property\n");
		return NULL;
	}

	rc = of_property_read_u32(node, "qcom,usb-total-bam-num",
		&pdata->total_bam_num);
	if (rc) {
		pr_err("Invalid usb total bam num property\n");
		return NULL;
	}

	rc = of_property_read_u32(node, "qcom,usb-bam-num-pipes",
		&pdata->usb_bam_num_pipes);
	if (rc) {
		pr_err("Invalid usb bam num pipes property\n");
		return NULL;
	}

	rc = of_property_read_u32(node, "qcom,usb-base-address",
		&pdata->usb_base_address);
	if (rc) {
		pr_err("Invalid usb base address property\n");
		return NULL;
	}

	for_each_child_of_node(pdev->dev.of_node, node)
		pipe_entry++;

	/*
	 * we need to know the number of connection, so we will know
	 * how much memory to allocate
	 */
	conn_num = pipe_entry / 2;
	bam_amount = pdata->total_bam_num;

	if (conn_num > 0 && conn_num < pdata->usb_bam_num_pipes) {
		/* alloc msm_usb_bam_connections_info */
		bam_connection_arr = devm_kzalloc(&pdev->dev, bam_amount *
			conn_num * ncolumns *
			sizeof(struct usb_bam_pipe_connect), GFP_KERNEL);

		if (!bam_connection_arr)
			goto err;

		msm_usb_bam_connections_info = devm_kzalloc(&pdev->dev,
			bam_amount * sizeof(struct usb_bam_pipe_connect **),
			GFP_KERNEL);

		if (!msm_usb_bam_connections_info)
			goto err;

		for (j = 0; j < bam_amount; j++) {
			msm_usb_bam_connections_info[j] =
				devm_kzalloc(&pdev->dev, conn_num *
				sizeof(struct usb_bam_pipe_connect *),
				GFP_KERNEL);
			for (i = 0; i < conn_num; i++)
				msm_usb_bam_connections_info[j][i] =
					bam_connection_arr +
					(j * conn_num * ncolumns) +
					(i * ncolumns);
		}

		/* retrieve device tree parameters */
		for_each_child_of_node(pdev->dev.of_node, node) {
			const char *str;

			key = "qcom,usb-bam-type";
			rc = of_property_read_u32(node, key, &bam);
			if (rc)
				goto err;

			rc = of_property_read_string(node, "label", &str);
			if (rc) {
				pr_err("Cannot read string\n");
				goto err;
			}

			if (strstr(str, "usb-to-peri"))
				dir = USB_TO_PEER_PERIPHERAL;
			else if (strstr(str, "peri-to-usb"))
				dir = PEER_PERIPHERAL_TO_USB;
			else
				goto err;

			if (!strcmp(str, "usb-to-peri-qdss-dwc3") ||
				!strcmp(str, "peri-to-usb-qdss-dwc3"))
					conn_num = 0;
			else
				goto err;

			rc = update_connections_info(node, bam, conn_num, dir);
			if (rc)
				goto err;
		}

		pdata->connections = &msm_usb_bam_connections_info[0][0][0];

	} else {
		goto err;
	}

	return pdata;
err:
	pr_err("%s: failed\n", __func__);
	return NULL;
}

static int usb_bam_init(void)
{
	u32 h_usb;
	int ret;
	void *usb_virt_addr;
	struct msm_usb_bam_platform_data *pdata =
		usb_bam_pdev->dev.platform_data;
	struct resource *res;
	int irq;

	res = platform_get_resource(usb_bam_pdev, IORESOURCE_MEM,
		pdata->usb_active_bam);
	if (!res) {
		dev_err(&usb_bam_pdev->dev, "Unable to get memory resource\n");
		return -ENODEV;
	}

	irq = platform_get_irq(usb_bam_pdev, pdata->usb_active_bam);
	if (irq < 0) {
		dev_err(&usb_bam_pdev->dev, "Unable to get IRQ resource\n");
		return irq;
	}

	usb_virt_addr = ioremap(res->start, resource_size(res));
	if (!usb_virt_addr) {
		pr_err("%s: ioremap failed\n", __func__);
		return -ENOMEM;
	}
	usb_props.phys_addr = res->start;
	usb_props.virt_addr = usb_virt_addr;
	usb_props.virt_size = resource_size(res);
	usb_props.irq = irq;
	usb_props.summing_threshold = USB_SUMMING_THRESHOLD;
	usb_props.event_threshold = 512;
	usb_props.num_pipes = pdata->usb_bam_num_pipes;

	ret = sps_register_bam_device(&usb_props, &h_usb);
	if (ret < 0) {
		pr_err("%s: register bam error %d\n", __func__, ret);
		return -EFAULT;
	}

	return 0;
}

static char *bam_enable_strings[2] = {
	[HSUSB_BAM] = "hsusb",
	[HSIC_BAM]  = "hsic",
};

static ssize_t
usb_bam_show_enable(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct platform_device *pdev =
		container_of(dev, struct platform_device, dev);
	struct msm_usb_bam_platform_data *pdata =
		usb_bam_pdev->dev.platform_data;

	if (!pdev || !pdata)
		return 0;
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 bam_enable_strings[pdata->usb_active_bam]);
}

static ssize_t usb_bam_store_enable(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
		struct platform_device, dev);
	struct msm_usb_bam_platform_data *pdata =
		usb_bam_pdev->dev.platform_data;
	char str[10], *pstr;
	int ret, i;

	strlcpy(str, buf, sizeof(str));
	pstr = strim(str);

	for (i = 0; i < ARRAY_SIZE(bam_enable_strings); i++) {
		if (!strncmp(pstr, bam_enable_strings[i], sizeof(str)))
			pdata->usb_active_bam = i;
	}

	dev_dbg(&pdev->dev, "active_bam=%s\n",
		bam_enable_strings[pdata->usb_active_bam]);

	ret = usb_bam_init();
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize usb bam\n");
		return ret;
	}

	return count;
}

static DEVICE_ATTR(enable, S_IWUSR | S_IRUSR, usb_bam_show_enable,
		   usb_bam_store_enable);

static int usb_bam_probe(struct platform_device *pdev)
{
	int ret, i;
	struct msm_usb_bam_platform_data *pdata;

	dev_dbg(&pdev->dev, "usb_bam_probe\n");

	for (i = 0; i < CONNECTIONS_NUM; i++) {
		usb_bam_connections[i].enabled = 0;
		INIT_WORK(&usb_bam_connections[i].peer_event.wake_w,
			usb_bam_wake_work);
	}

	if (pdev->dev.of_node) {
		dev_dbg(&pdev->dev, "device tree enabled\n");
		device_tree_enabled = 1;
		pdata = usb_bam_dt_to_pdata(pdev);
		if (!pdata)
			return -ENOMEM;
		pdev->dev.platform_data = pdata;
	} else if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "missing platform_data\n");
		return -ENODEV;
	} else {
		pdata = pdev->dev.platform_data;
		device_tree_enabled = 0;
	}
	usb_bam_pdev = pdev;

	ret = device_create_file(&pdev->dev, &dev_attr_enable);
	if (ret)
		dev_err(&pdev->dev, "failed to create device file\n");

	usb_bam_wq = alloc_workqueue("usb_bam_wq",
		WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!usb_bam_wq) {
		pr_err("unable to create workqueue usb_bam_wq\n");
		return -ENOMEM;
	}

	return ret;
}

void get_bam2bam_connection_info(u8 conn_idx, enum usb_bam_pipe_dir pipe_dir,
	u32 *usb_bam_handle, u32 *usb_bam_pipe_idx, u32 *peer_pipe_idx,
	struct sps_mem_buffer *desc_fifo, struct sps_mem_buffer *data_fifo)
{
	struct sps_connect *connection =
		&sps_connections[conn_idx][pipe_dir];


	if (pipe_dir == USB_TO_PEER_PERIPHERAL) {
		*usb_bam_handle = connection->source;
		*usb_bam_pipe_idx = connection->src_pipe_index;
		*peer_pipe_idx = connection->dest_pipe_index;
	} else {
		*usb_bam_handle = connection->destination;
		*usb_bam_pipe_idx = connection->dest_pipe_index;
		*peer_pipe_idx = connection->src_pipe_index;
	}
	if (data_fifo)
		memcpy(data_fifo, &data_mem_buf[conn_idx][pipe_dir],
			sizeof(struct sps_mem_buffer));
	if (desc_fifo)
		memcpy(desc_fifo, &desc_mem_buf[conn_idx][pipe_dir],
			sizeof(struct sps_mem_buffer));
}
EXPORT_SYMBOL(get_bam2bam_connection_info);

static int usb_bam_remove(struct platform_device *pdev)
{
	destroy_workqueue(usb_bam_wq);

	return 0;
}

static const struct of_device_id usb_bam_dt_match[] = {
	{ .compatible = "qcom,usb-bam-msm",
	},
	{}
};
MODULE_DEVICE_TABLE(of, usb_bam_dt_match);

static struct platform_driver usb_bam_driver = {
	.probe = usb_bam_probe,
	.remove = usb_bam_remove,
	.driver		= {
		.name	= "usb_bam",
		.of_match_table = usb_bam_dt_match,
	},
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
