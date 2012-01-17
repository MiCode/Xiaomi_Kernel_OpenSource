/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <mach/vreg.h>
#include <mach/camera.h>
#include <mach/clk.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>

#define BUFF_SIZE_128 128

static struct clk *camio_jpeg_clk;
static struct clk *camio_jpeg_pclk;
static struct regulator *fs_ijpeg;

static struct platform_device *camio_dev;
static struct resource *s3drw_io, *s3dctl_io;
static struct resource *s3drw_mem, *s3dctl_mem;
void __iomem *s3d_rw, *s3d_ctl;

void msm_io_w(u32 data, void __iomem *addr)
{
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	writel_relaxed((data), (addr));
}

void msm_io_w_mb(u32 data, void __iomem *addr)
{
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	wmb();
	writel_relaxed((data), (addr));
	wmb();
}

u32 msm_io_r(void __iomem *addr)
{
	uint32_t data = readl_relaxed(addr);
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	return data;
}

u32 msm_io_r_mb(void __iomem *addr)
{
	uint32_t data;
	rmb();
	data = readl_relaxed(addr);
	rmb();
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	return data;
}

void msm_io_memcpy_toio(void __iomem *dest_addr,
	void __iomem *src_addr, u32 len)
{
	int i;
	u32 *d = (u32 *) dest_addr;
	u32 *s = (u32 *) src_addr;
	/* memcpy_toio does not work. Use writel_relaxed for now */
	for (i = 0; i < len; i++)
		writel_relaxed(*s++, d++);
}

void msm_io_dump(void __iomem *addr, int size)
{
	char line_str[BUFF_SIZE_128], *p_str;
	int i;
	u32 *p = (u32 *) addr;
	u32 data;
	CDBG("%s: %p %d\n", __func__, addr, size);
	line_str[0] = '\0';
	p_str = line_str;
	for (i = 0; i < size/4; i++) {
		if (i % 4 == 0) {
			snprintf(p_str, 12, "%08x: ", (u32) p);
			p_str += 10;
		}
		data = readl_relaxed(p++);
		snprintf(p_str, 12, "%08x ", data);
		p_str += 9;
		if ((i + 1) % 4 == 0) {
			CDBG("%s\n", line_str);
			line_str[0] = '\0';
			p_str = line_str;
		}
	}
	if (line_str[0] != '\0')
		CDBG("%s\n", line_str);
}

void msm_io_memcpy(void __iomem *dest_addr, void __iomem *src_addr, u32 len)
{
	CDBG("%s: %p %p %d\n", __func__, dest_addr, src_addr, len);
	msm_io_memcpy_toio(dest_addr, src_addr, len / 4);
	msm_io_dump(dest_addr, len);
}

int msm_camio_clk_enable(enum msm_camio_clk_type clktype)
{
	int rc = 0;
	struct clk *clk = NULL;

	switch (clktype) {
	case CAMIO_JPEG_CLK:
		camio_jpeg_clk =
		clk = clk_get(NULL, "ijpeg_clk");
		clk_set_rate(clk, 153600000);
		break;

	case CAMIO_JPEG_PCLK:
		camio_jpeg_pclk =
		clk = clk_get(NULL, "ijpeg_pclk");
		break;

	default:
		break;
	}

	if (!IS_ERR(clk))
		rc = clk_enable(clk);
	else
		rc = PTR_ERR(clk);

	if (rc < 0)
		pr_err("%s(%d) failed %d\n", __func__, clktype, rc);

	return rc;
}

int msm_camio_clk_disable(enum msm_camio_clk_type clktype)
{
	int rc = 0;
	struct clk *clk = NULL;

	switch (clktype) {
	case CAMIO_JPEG_CLK:
		clk = camio_jpeg_clk;
		break;

	case CAMIO_JPEG_PCLK:
		clk = camio_jpeg_pclk;
		break;

	default:
		break;
	}

	if (!IS_ERR(clk)) {
		clk_disable(clk);
		clk_put(clk);
	} else
		rc = PTR_ERR(clk);

	if (rc < 0)
		pr_err("%s(%d) failed %d\n", __func__, clktype, rc);

	return rc;
}

void msm_camio_clk_rate_set_2(struct clk *clk, int rate)
{
	clk_set_rate(clk, rate);
}

int msm_camio_jpeg_clk_disable(void)
{
	int rc = 0;
	rc = msm_camio_clk_disable(CAMIO_JPEG_PCLK);
	if (rc < 0)
		return rc;
	rc = msm_camio_clk_disable(CAMIO_JPEG_CLK);

	if (fs_ijpeg) {
		rc = regulator_disable(fs_ijpeg);
		if (rc < 0) {
			pr_err("%s: Regulator disable failed %d\n",
						__func__, rc);
			return rc;
		}
		regulator_put(fs_ijpeg);
	}
	CDBG("%s: exit %d\n", __func__, rc);
	return rc;
}

int msm_camio_jpeg_clk_enable(void)
{
	int rc = 0;
	fs_ijpeg = regulator_get(NULL, "fs_ijpeg");
	if (IS_ERR(fs_ijpeg)) {
		pr_err("%s: Regulator FS_IJPEG get failed %ld\n",
			__func__, PTR_ERR(fs_ijpeg));
		fs_ijpeg = NULL;
	} else if (regulator_enable(fs_ijpeg)) {
		pr_err("%s: Regulator FS_IJPEG enable failed\n", __func__);
		regulator_put(fs_ijpeg);
	}

	rc = msm_camio_clk_enable(CAMIO_JPEG_CLK);
	if (rc < 0)
		return rc;
	rc = msm_camio_clk_enable(CAMIO_JPEG_PCLK);
	if (rc < 0)
		return rc;

	CDBG("%s: exit %d\n", __func__, rc);
	return rc;
}

int32_t msm_camio_3d_enable(const struct msm_camera_sensor_info *s_info)
{
	int32_t val = 0, rc = 0;
	char s3drw[] = "s3d_rw";
	char s3dctl[] = "s3d_ctl";
	struct platform_device *pdev = camio_dev;
	pdev->resource = s_info->resource;
	pdev->num_resources = s_info->num_resources;

	s3drw_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, s3drw);
	if (!s3drw_mem) {
		pr_err("%s: no mem resource?\n", __func__);
		return -ENODEV;
	}
	s3dctl_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, s3dctl);
	if (!s3dctl_mem) {
		pr_err("%s: no mem resource?\n", __func__);
		return -ENODEV;
	}
	s3drw_io = request_mem_region(s3drw_mem->start,
		resource_size(s3drw_mem), pdev->name);
	if (!s3drw_io)
		return -EBUSY;

	s3d_rw = ioremap(s3drw_mem->start,
		resource_size(s3drw_mem));
	if (!s3d_rw) {
		rc = -ENOMEM;
		goto s3drw_nomem;
	}
	s3dctl_io = request_mem_region(s3dctl_mem->start,
		resource_size(s3dctl_mem), pdev->name);
	if (!s3dctl_io) {
		rc = -EBUSY;
		goto s3dctl_busy;
	}
	s3d_ctl = ioremap(s3dctl_mem->start,
		resource_size(s3dctl_mem));
	if (!s3d_ctl) {
		rc = -ENOMEM;
		goto s3dctl_nomem;
	}

	val = msm_io_r(s3d_rw);
	msm_io_w((val | 0x200), s3d_rw);
	return rc;

s3dctl_nomem:
	release_mem_region(s3dctl_mem->start, resource_size(s3dctl_mem));
s3dctl_busy:
	iounmap(s3d_rw);
s3drw_nomem:
	release_mem_region(s3drw_mem->start, resource_size(s3drw_mem));
return rc;
}

void msm_camio_3d_disable(void)
{
	int32_t val = 0;
	msm_io_w((val & ~0x200), s3d_rw);
	iounmap(s3d_ctl);
	release_mem_region(s3dctl_mem->start, resource_size(s3dctl_mem));
	iounmap(s3d_rw);
	release_mem_region(s3drw_mem->start, resource_size(s3drw_mem));
}

void msm_camio_mode_config(enum msm_cam_mode mode)
{
	uint32_t val;
	val = msm_io_r(s3d_ctl);
	if (mode == MODE_DUAL) {
		msm_io_w(val | 0x3, s3d_ctl);
	} else if (mode == MODE_L) {
		msm_io_w(((val | 0x2) & ~(0x1)), s3d_ctl);
		val = msm_io_r(s3d_ctl);
		CDBG("the camio mode config left value is %d\n", val);
	} else {
		msm_io_w(((val | 0x1) & ~(0x2)), s3d_ctl);
		val = msm_io_r(s3d_ctl);
		CDBG("the camio mode config right value is %d\n", val);
	}
}

void msm_camio_bus_scale_cfg(struct msm_bus_scale_pdata *cam_bus_scale_table,
		enum msm_bus_perf_setting perf_setting)
{
	static uint32_t bus_perf_client;
	int rc = 0;
	switch (perf_setting) {
	case S_INIT:
		bus_perf_client =
			msm_bus_scale_register_client(cam_bus_scale_table);
		if (!bus_perf_client) {
			CDBG("%s: Registration Failed!!!\n", __func__);
			bus_perf_client = 0;
			return;
		}
		CDBG("%s: S_INIT rc = %u\n", __func__, bus_perf_client);
		break;
	case S_EXIT:
		if (bus_perf_client) {
			CDBG("%s: S_EXIT\n", __func__);
			msm_bus_scale_unregister_client(bus_perf_client);
		} else
			CDBG("%s: Bus Client NOT Registered!!!\n", __func__);
		break;
	case S_PREVIEW:
		if (bus_perf_client) {
			rc = msm_bus_scale_client_update_request(
				bus_perf_client, 1);
			CDBG("%s: S_PREVIEW rc = %d\n", __func__, rc);
		} else
			CDBG("%s: Bus Client NOT Registered!!!\n", __func__);
		break;
	case S_VIDEO:
		if (bus_perf_client) {
			rc = msm_bus_scale_client_update_request(
				bus_perf_client, 2);
			CDBG("%s: S_VIDEO rc = %d\n", __func__, rc);
		} else
			CDBG("%s: Bus Client NOT Registered!!!\n", __func__);
		break;
	case S_CAPTURE:
		if (bus_perf_client) {
			rc = msm_bus_scale_client_update_request(
				bus_perf_client, 3);
			CDBG("%s: S_CAPTURE rc = %d\n", __func__, rc);
		} else
			CDBG("%s: Bus Client NOT Registered!!!\n", __func__);
		break;
	case S_ZSL:
		if (bus_perf_client) {
			rc = msm_bus_scale_client_update_request(
				bus_perf_client, 4);
			CDBG("%s: S_ZSL rc = %d\n", __func__, rc);
		} else
			CDBG("%s: Bus Client NOT Registered!!!\n", __func__);
		break;
	case S_DEFAULT:
		break;
	default:
		pr_warning("%s: INVALID CASE\n", __func__);
	}
}
