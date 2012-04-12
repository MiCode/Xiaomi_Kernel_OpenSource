/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <mach/vreg.h>
#include <mach/camera.h>
#include <mach/clk.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>

#include <linux/pm_qos_params.h>

/* AXI rates in KHz */
#define MSM_AXI_QOS_PREVIEW     192000
#define MSM_AXI_QOS_SNAPSHOT    192000
#define MSM_AXI_QOS_RECORDING   192000

#define BUFF_SIZE_128 128

static struct clk *camio_jpeg_clk;
static struct clk *camio_vfe_clk;
static struct clk *camio_jpeg_pclk;
static struct clk *camio_vpe_clk;
static struct clk *camio_vpe_pclk;
static struct regulator *fs_ijpeg;
static struct regulator *fs_vpe;

static int vpe_clk_rate;
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
	/* memcpy_toio does not work. Use writel for now */
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
		rc = clk_set_rate(clk, 228571000);
		if (rc < 0)
			rc = clk_set_rate(clk, 153600000);
		break;

	case CAMIO_JPEG_PCLK:
		camio_jpeg_pclk =
		clk = clk_get(NULL, "ijpeg_pclk");
		break;

	case CAMIO_VPE_CLK:
		camio_vpe_clk =
		clk = clk_get(NULL, "vpe_clk");
		vpe_clk_rate = clk_round_rate(camio_vpe_clk, vpe_clk_rate);
		clk_set_rate(camio_vpe_clk, vpe_clk_rate);
		break;

	case CAMIO_VPE_PCLK:
		camio_vpe_pclk =
		clk = clk_get(NULL, "vpe_pclk");
		break;

	default:
		break;
	}

	if (!IS_ERR(clk)) {
		clk_prepare(clk);
		clk_enable(clk);
	} else {
		rc = -1;
	}
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

	case CAMIO_VPE_CLK:
		clk = camio_vpe_clk;
		break;

	case CAMIO_VPE_PCLK:
		clk = camio_vpe_pclk;
		break;

	default:
		break;
	}

	if (!IS_ERR(clk)) {
		clk_disable(clk);
		clk_unprepare(clk);
		clk_put(clk);
	} else {
		rc = -1;
	}

	return rc;
}

int msm_camio_vfe_clk_rate_set(int rate)
{
	int rc = 0;
	struct clk *clk = camio_vfe_clk;
	if (rate > clk_get_rate(clk))
		rc = clk_set_rate(clk, rate);
	return rc;
}

void msm_camio_clk_rate_set_2(struct clk *clk, int rate)
{
	clk_set_rate(clk, rate);
}

int msm_camio_jpeg_clk_disable(void)
{
	int rc = 0;
	if (fs_ijpeg) {
		rc = regulator_disable(fs_ijpeg);
		if (rc < 0) {
			CDBG("%s: Regulator disable failed %d\n", __func__, rc);
			return rc;
		}
		regulator_put(fs_ijpeg);
	}
	rc = msm_camio_clk_disable(CAMIO_JPEG_PCLK);
	if (rc < 0)
		return rc;
	rc = msm_camio_clk_disable(CAMIO_JPEG_CLK);
	CDBG("%s: exit %d\n", __func__, rc);
	return rc;
}

int msm_camio_jpeg_clk_enable(void)
{
	int rc = 0;
	rc = msm_camio_clk_enable(CAMIO_JPEG_CLK);
	if (rc < 0)
		return rc;
	rc = msm_camio_clk_enable(CAMIO_JPEG_PCLK);
	if (rc < 0)
		return rc;
	fs_ijpeg = regulator_get(NULL, "fs_ijpeg");
	if (IS_ERR(fs_ijpeg)) {
		CDBG("%s: Regulator FS_IJPEG get failed %ld\n", __func__,
			PTR_ERR(fs_ijpeg));
		fs_ijpeg = NULL;
	} else if (regulator_enable(fs_ijpeg)) {
		CDBG("%s: Regulator FS_IJPEG enable failed\n", __func__);
		regulator_put(fs_ijpeg);
	}
	CDBG("%s: exit %d\n", __func__, rc);
	return rc;
}

int msm_camio_vpe_clk_disable(void)
{
	int rc = 0;
	if (fs_vpe) {
		regulator_disable(fs_vpe);
		regulator_put(fs_vpe);
	}

	rc = msm_camio_clk_disable(CAMIO_VPE_CLK);
	if (rc < 0)
		return rc;
	rc = msm_camio_clk_disable(CAMIO_VPE_PCLK);
	return rc;
}

int msm_camio_vpe_clk_enable(uint32_t clk_rate)
{
	int rc = 0;
	fs_vpe = regulator_get(NULL, "fs_vpe");
	if (IS_ERR(fs_vpe)) {
		CDBG("%s: Regulator FS_VPE get failed %ld\n", __func__,
			PTR_ERR(fs_vpe));
		fs_vpe = NULL;
	} else if (regulator_enable(fs_vpe)) {
		CDBG("%s: Regulator FS_VPE enable failed\n", __func__);
		regulator_put(fs_vpe);
	}

	vpe_clk_rate = clk_rate;
	rc = msm_camio_clk_enable(CAMIO_VPE_CLK);
	if (rc < 0)
		return rc;

	rc = msm_camio_clk_enable(CAMIO_VPE_PCLK);
	return rc;
}

void msm_camio_vfe_blk_reset(void)
{
	return;
}

static void msm_camio_axi_cfg(enum msm_bus_perf_setting perf_setting)
{
	switch (perf_setting) {
	case S_INIT:
		add_axi_qos();
		break;
	case S_PREVIEW:
		update_axi_qos(MSM_AXI_QOS_PREVIEW);
		break;
	case S_VIDEO:
		update_axi_qos(MSM_AXI_QOS_RECORDING);
		break;
	case S_CAPTURE:
		update_axi_qos(MSM_AXI_QOS_SNAPSHOT);
		break;
	case S_DEFAULT:
		update_axi_qos(PM_QOS_DEFAULT_VALUE);
		break;
	case S_EXIT:
		release_axi_qos();
		break;
	default:
		CDBG("%s: INVALID CASE\n", __func__);
	}
}

void msm_camio_bus_scale_cfg(struct msm_bus_scale_pdata *cam_bus_scale_table,
		enum msm_bus_perf_setting perf_setting)
{
	static uint32_t bus_perf_client;
	int rc = 0;
	if (cam_bus_scale_table == NULL) {
		msm_camio_axi_cfg(perf_setting);
		return;
	}

	switch (perf_setting) {
	case S_INIT:
		bus_perf_client =
			msm_bus_scale_register_client(cam_bus_scale_table);
		if (!bus_perf_client) {
			pr_err("%s: Registration Failed!!!\n", __func__);
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
			pr_err("%s: Bus Client NOT Registered!!!\n", __func__);
		break;
	case S_PREVIEW:
		if (bus_perf_client) {
			rc = msm_bus_scale_client_update_request(
				bus_perf_client, 1);
			CDBG("%s: S_PREVIEW rc = %d\n", __func__, rc);
		} else
			pr_err("%s: Bus Client NOT Registered!!!\n", __func__);
		break;
	case S_VIDEO:
		if (bus_perf_client) {
			rc = msm_bus_scale_client_update_request(
				bus_perf_client, 2);
			CDBG("%s: S_VIDEO rc = %d\n", __func__, rc);
		} else
			pr_err("%s: Bus Client NOT Registered!!!\n", __func__);
		break;
	case S_CAPTURE:
		if (bus_perf_client) {
			rc = msm_bus_scale_client_update_request(
				bus_perf_client, 3);
			CDBG("%s: S_CAPTURE rc = %d\n", __func__, rc);
		} else
			pr_err("%s: Bus Client NOT Registered!!!\n", __func__);
		break;

	case S_ZSL:
		if (bus_perf_client) {
			rc = msm_bus_scale_client_update_request(
				bus_perf_client, 4);
			CDBG("%s: S_ZSL rc = %d\n", __func__, rc);
		} else
			pr_err("%s: Bus Client NOT Registered!!!\n", __func__);
		break;
	case S_STEREO_VIDEO:
		if (bus_perf_client) {
			rc = msm_bus_scale_client_update_request(
				bus_perf_client, 5);
			CDBG("%s: S_STEREO_VIDEO rc = %d\n", __func__, rc);
		} else
			pr_err("%s: Bus Client NOT Registered!!!\n", __func__);
		break;
	case S_STEREO_CAPTURE:
		if (bus_perf_client) {
			rc = msm_bus_scale_client_update_request(
				bus_perf_client, 6);
			CDBG("%s: S_STEREO_VIDEO rc = %d\n", __func__, rc);
		} else
			pr_err("%s: Bus Client NOT Registered!!!\n", __func__);
		break;
	case S_DEFAULT:
		break;
	default:
		pr_warning("%s: INVALID CASE\n", __func__);
	}
}

