/* Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
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
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <mach/clk.h>

#define CAMIF_CFG_RMSK 0x1fffff
#define CAM_SEL_BMSK 0x2
#define CAM_PCLK_SRC_SEL_BMSK 0x60000
#define CAM_PCLK_INVERT_BMSK 0x80000
#define CAM_PAD_REG_SW_RESET_BMSK 0x100000

#define EXT_CAM_HSYNC_POL_SEL_BMSK 0x10000
#define EXT_CAM_VSYNC_POL_SEL_BMSK 0x8000
#define MDDI_CLK_CHICKEN_BIT_BMSK  0x80

#define CAM_SEL_SHFT 0x1
#define CAM_PCLK_SRC_SEL_SHFT 0x11
#define CAM_PCLK_INVERT_SHFT 0x13
#define CAM_PAD_REG_SW_RESET_SHFT 0x14

#define EXT_CAM_HSYNC_POL_SEL_SHFT 0x10
#define EXT_CAM_VSYNC_POL_SEL_SHFT 0xF
#define MDDI_CLK_CHICKEN_BIT_SHFT  0x7
#define APPS_RESET_OFFSET 0x00000214

static struct clk *camio_vfe_mdc_clk;
static struct clk *camio_mdc_clk;
static struct clk *camio_vfe_clk;
static struct clk *camio_vfe_axi_clk;
static struct msm_camera_io_ext camio_ext;
static struct resource *appio, *mdcio;

void __iomem *appbase, *mdcbase;


int msm_camio_clk_enable(enum msm_camio_clk_type clktype)
{
	int rc = 0;
	struct clk *clk = NULL;

	switch (clktype) {
	case CAMIO_VFE_MDC_CLK:
		camio_vfe_mdc_clk = clk = clk_get(NULL, "vfe_mdc_clk");
		break;

	case CAMIO_MDC_CLK:
		camio_mdc_clk = clk = clk_get(NULL, "mdc_clk");
		break;

	case CAMIO_VFE_CLK:
		camio_vfe_clk = clk = clk_get(NULL, "vfe_clk");
		break;

	case CAMIO_VFE_AXI_CLK:
		camio_vfe_axi_clk = clk = clk_get(NULL, "vfe_axi_clk");
		break;

	default:
		break;
	}

	if (!IS_ERR(clk))
		clk_enable(clk);
	else
		rc = -1;

	return rc;
}

int msm_camio_clk_disable(enum msm_camio_clk_type clktype)
{
	int rc = 0;
	struct clk *clk = NULL;

	switch (clktype) {
	case CAMIO_VFE_MDC_CLK:
		clk = camio_vfe_mdc_clk;
		break;

	case CAMIO_MDC_CLK:
		clk = camio_mdc_clk;
		break;

	case CAMIO_VFE_CLK:
		clk = camio_vfe_clk;
		break;

	case CAMIO_VFE_AXI_CLK:
		clk = camio_vfe_axi_clk;
		break;

	default:
		break;
	}

	if (!IS_ERR(clk)) {
		clk_disable(clk);
		clk_put(clk);
	} else
		rc = -1;

	return rc;
}

void msm_camio_clk_rate_set(int rate)
{
	struct clk *clk = camio_vfe_mdc_clk;

	/* TODO: check return */
	clk_set_rate(clk, rate);
}

int msm_camio_enable(struct platform_device *pdev)
{
	int rc = 0;

	appio = request_mem_region(camio_ext.appphy,
		camio_ext.appsz, pdev->name);
	if (!appio) {
		rc = -EBUSY;
		goto enable_fail;
	}

	appbase = ioremap(camio_ext.appphy, camio_ext.appsz);
	if (!appbase) {
		rc = -ENOMEM;
		goto apps_no_mem;
	}
	msm_camio_clk_enable(CAMIO_MDC_CLK);
	msm_camio_clk_enable(CAMIO_VFE_AXI_CLK);
	return 0;

apps_no_mem:
	release_mem_region(camio_ext.appphy, camio_ext.appsz);
enable_fail:
	return rc;
}

void msm_camio_disable(struct platform_device *pdev)
{
	iounmap(appbase);
	release_mem_region(camio_ext.appphy, camio_ext.appsz);
	msm_camio_clk_disable(CAMIO_MDC_CLK);
	msm_camio_clk_disable(CAMIO_VFE_AXI_CLK);
}

int msm_camio_sensor_clk_on(struct platform_device *pdev)
{

	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	int32_t rc = 0;
	camio_ext = camdev->ioext;

	mdcio = request_mem_region(camio_ext.mdcphy,
		camio_ext.mdcsz, pdev->name);
	if (!mdcio)
		rc = -EBUSY;
	mdcbase = ioremap(camio_ext.mdcphy,
		camio_ext.mdcsz);
	if (!mdcbase)
		goto mdc_no_mem;
	camdev->camera_gpio_on();

	msm_camio_clk_enable(CAMIO_VFE_CLK);
	msm_camio_clk_enable(CAMIO_VFE_MDC_CLK);
	return rc;


mdc_no_mem:
	release_mem_region(camio_ext.mdcphy, camio_ext.mdcsz);
	return -EINVAL;
}

int msm_camio_sensor_clk_off(struct platform_device *pdev)
{
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	camdev->camera_gpio_off();
	iounmap(mdcbase);
	release_mem_region(camio_ext.mdcphy, camio_ext.mdcsz);
	msm_camio_clk_disable(CAMIO_VFE_CLK);
	return msm_camio_clk_disable(CAMIO_VFE_MDC_CLK);

}

void msm_disable_io_gpio_clk(struct platform_device *pdev)
{
	return;
}

void msm_camio_camif_pad_reg_reset(void)
{
	uint32_t reg;
	uint32_t mask, value;

	/* select CLKRGM_VFE_SRC_CAM_VFE_SRC:  internal source */
	msm_camio_clk_sel(MSM_CAMIO_CLK_SRC_INTERNAL);

	reg = (msm_camera_io_r_mb(mdcbase)) & CAMIF_CFG_RMSK;

	mask = CAM_SEL_BMSK |
		CAM_PCLK_SRC_SEL_BMSK |
		CAM_PCLK_INVERT_BMSK |
		EXT_CAM_HSYNC_POL_SEL_BMSK |
	    EXT_CAM_VSYNC_POL_SEL_BMSK | MDDI_CLK_CHICKEN_BIT_BMSK;

	value = 1 << CAM_SEL_SHFT |
		3 << CAM_PCLK_SRC_SEL_SHFT |
		0 << CAM_PCLK_INVERT_SHFT |
		0 << EXT_CAM_HSYNC_POL_SEL_SHFT |
	    0 << EXT_CAM_VSYNC_POL_SEL_SHFT | 0 << MDDI_CLK_CHICKEN_BIT_SHFT;
	msm_camera_io_w_mb((reg & (~mask)) | (value & mask), mdcbase);
	usleep_range(10000, 11000);

	reg = (msm_camera_io_r_mb(mdcbase)) & CAMIF_CFG_RMSK;
	mask = CAM_PAD_REG_SW_RESET_BMSK;
	value = 1 << CAM_PAD_REG_SW_RESET_SHFT;
	msm_camera_io_w_mb((reg & (~mask)) | (value & mask), mdcbase);
	usleep_range(10000, 11000);

	reg = (msm_camera_io_r_mb(mdcbase)) & CAMIF_CFG_RMSK;
	mask = CAM_PAD_REG_SW_RESET_BMSK;
	value = 0 << CAM_PAD_REG_SW_RESET_SHFT;
	msm_camera_io_w_mb((reg & (~mask)) | (value & mask), mdcbase);
	usleep_range(10000, 11000);

	msm_camio_clk_sel(MSM_CAMIO_CLK_SRC_EXTERNAL);

	usleep_range(10000, 11000);

	/* todo: check return */
	if (camio_vfe_clk)
		clk_set_rate(camio_vfe_clk, 96000000);
}

void msm_camio_vfe_blk_reset(void)
{
	uint32_t val;

	val = msm_camera_io_r_mb(appbase + APPS_RESET_OFFSET);
	val |= 0x1;
	msm_camera_io_w_mb(val, appbase + APPS_RESET_OFFSET);
	usleep_range(10000, 11000);

	val = msm_camera_io_r_mb(appbase + APPS_RESET_OFFSET);
	val &= ~0x1;
	msm_camera_io_w_mb(val, appbase + APPS_RESET_OFFSET);
	usleep_range(10000, 11000);
}

void msm_camio_camif_pad_reg_reset_2(void)
{
	uint32_t reg;
	uint32_t mask, value;

	reg = (msm_camera_io_r_mb(mdcbase)) & CAMIF_CFG_RMSK;
	mask = CAM_PAD_REG_SW_RESET_BMSK;
	value = 1 << CAM_PAD_REG_SW_RESET_SHFT;
	msm_camera_io_w_mb((reg & (~mask)) | (value & mask), mdcbase);
	usleep_range(10000, 11000);

	reg = (msm_camera_io_r_mb(mdcbase)) & CAMIF_CFG_RMSK;
	mask = CAM_PAD_REG_SW_RESET_BMSK;
	value = 0 << CAM_PAD_REG_SW_RESET_SHFT;
	msm_camera_io_w_mb((reg & (~mask)) | (value & mask), mdcbase);
	usleep_range(10000, 11000);
}

void msm_camio_clk_sel(enum msm_camio_clk_src_type srctype)
{
	struct clk *clk = NULL;

	clk = camio_vfe_clk;

	if (clk != NULL) {
		switch (srctype) {
		case MSM_CAMIO_CLK_SRC_INTERNAL:
			clk_set_flags(clk, 0x00000100 << 1);
			break;

		case MSM_CAMIO_CLK_SRC_EXTERNAL:
			clk_set_flags(clk, 0x00000100);
			break;

		default:
			break;
		}
	}
}

void msm_camio_clk_axi_rate_set(int rate)
{
	struct clk *clk = camio_vfe_axi_clk;
	/* todo: check return */
	clk_set_rate(clk, rate);
}

int msm_camio_probe_on(struct platform_device *pdev)
{
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;

	camdev->camera_gpio_on();
	return msm_camio_clk_enable(CAMIO_VFE_MDC_CLK);
}

int msm_camio_probe_off(struct platform_device *pdev)
{
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;

	camdev->camera_gpio_off();
	return msm_camio_clk_disable(CAMIO_VFE_MDC_CLK);
}
