// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#define pr_fmt(__fmt) "[drm][%20s] "__fmt, __func__

#include <linux/device.h>
#include <linux/libfdt.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "disp_lib.h"

struct bmp_header {
	u16 magic;
	u32 size;
	u32 unused;
	u32 start;
} __attribute__((__packed__));

struct dib_header {
	u32 size;
	u32 width;
	u32 height;
	u16 planes;
	u16 bpp;
	u32 compression;
	u32 data_size;
	u32 h_res;
	u32 v_res;
	u32 colours;
	u32 important_colours;
	u32 red_mask;
	u32 green_mask;
	u32 blue_mask;
	u32 alpha_mask;
	u32 colour_space;
	u32 unused[12];
} __attribute__((__packed__));

int str_to_u32_array(const char *p, u32 base, u32 array[], u8 size)
{
	const char *start = p;
	char str[12];
	int length = 0;
	int i, ret;

	pr_info("input: %s", p);

	for (i = 0 ; i < size; i++) {
		while (*p == ' ')
			p++;
		if (*p == '\0')
			break;
		start = p;

		while ((*p != ' ') && (*p != '\0'))
			p++;

		if ((p - start) >= sizeof(str))
			break;

		memset(str, 0, sizeof(str));
		memcpy(str, start, p - start);

		ret = kstrtou32(str, base, &array[i]);
		if (ret) {
			DRM_ERROR("input format error\n");
			break;
		}

		length++;
	}

	return length;
}

int str_to_u8_array(const char *p, u32 base, u8 array[], u8 size)
{
	const char *start = p;
	char str[12];
	int length = 0;
	int i, ret;

	pr_info("input: %s", p);

	for (i = 0 ; i < size; i++) {
		while (*p == ' ')
			p++;
		if (*p == '\0')
			break;
		start = p;

		while ((*p != ' ') && (*p != '\0'))
			p++;

		if ((p - start) >= sizeof(str))
			break;

		memset(str, 0, sizeof(str));
		memcpy(str, start, p - start);

		ret = kstrtou8(str, base, &array[i]);
		if (ret) {
			DRM_ERROR("input format error\n");
			break;
		}

		length++;
	}

	return length;
}

#ifdef CONFIG_DRM_SPRD_WB_DEBUG
int dump_bmp32(const char *p, u32 width, u32 height,
		bool noflip, const char *filename)
{
	struct file *fp;
	loff_t pos = 0;
	struct dib_header dib_header = {
		.size = sizeof(dib_header),
		.width = width,
		.height = noflip ? -height : height,
		.planes = 1,
		.bpp = 32,
		.compression = 3,
		.data_size = 4 * width * height,
		.h_res = 0xB13,
		.v_res = 0xB13,
		.colours = 0,
		.important_colours = 0,
		.red_mask = 0x000000FF,
		.green_mask = 0x0000FF00,
		.blue_mask = 0x00FF0000,
		.alpha_mask = 0xFF000000,
		.colour_space = 0x57696E20,
	};
	struct bmp_header bmp_header = {
		.magic = 0x4d42,
		.size = (width * height * 4) +
		sizeof(bmp_header) + sizeof(dib_header),
		.start = sizeof(bmp_header) + sizeof(dib_header),
	};

	fp = filp_open(filename, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		DRM_ERROR("failed to open %s: %ld\n", filename, PTR_ERR(fp));
		return PTR_ERR(fp);
	}

	kernel_write(fp, (const char *)&bmp_header, sizeof(bmp_header), &pos);
	kernel_write(fp, (const char *)&dib_header, sizeof(dib_header), &pos);
	kernel_write(fp, p, width * height * 4, &pos);

	filp_close(fp, NULL);

	return 0;
}
#endif

struct device *sprd_disp_pipe_get_by_port(struct device *dev, int port)
{
	struct device_node *np = dev->of_node;
	struct device_node *endpoint;
	struct device_node *remote_node;
	struct platform_device *remote_pdev;

	endpoint = of_graph_get_endpoint_by_regs(np, port, 0);
	if (!endpoint) {
		DRM_ERROR("%s/port%d/endpoint0 was not found\n",
			  np->full_name, port);
		return NULL;
	}

	remote_node = of_graph_get_remote_port_parent(endpoint);
	if (!remote_node) {
		DRM_ERROR("device node was not found by endpoint0\n");
		return NULL;
	}

	remote_pdev = of_find_device_by_node(remote_node);
	if (remote_pdev == NULL) {
		DRM_ERROR("find %s platform device failed\n",
			  remote_node->full_name);
		return NULL;
	}

	return &remote_pdev->dev;
}

struct device *sprd_disp_pipe_get_input(struct device *dev)
{
	return sprd_disp_pipe_get_by_port(dev, 1);
}

struct device *sprd_disp_pipe_get_output(struct device *dev)
{
	return sprd_disp_pipe_get_by_port(dev, 0);
}

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("Display Common API Library");
MODULE_LICENSE("GPL v2");
