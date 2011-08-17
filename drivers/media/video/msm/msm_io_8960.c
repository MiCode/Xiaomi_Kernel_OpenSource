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
#include "msm_ispif.h"

#define DBG_CSID 0
#define DBG_CSIPHY 0

/* MIPI	CSI	PHY registers */
#define MIPI_CSIPHY_LNn_CFG1_ADDR                0x0
#define MIPI_CSIPHY_LNn_CFG2_ADDR                0x4
#define MIPI_CSIPHY_LNn_CFG3_ADDR                0x8
#define MIPI_CSIPHY_LNn_CFG4_ADDR                0xC
#define MIPI_CSIPHY_LNn_CFG5_ADDR                0x10
#define MIPI_CSIPHY_LNCK_CFG1_ADDR               0x100
#define MIPI_CSIPHY_LNCK_CFG2_ADDR               0x104
#define MIPI_CSIPHY_LNCK_CFG3_ADDR               0x108
#define MIPI_CSIPHY_LNCK_CFG4_ADDR               0x10C
#define MIPI_CSIPHY_LNCK_CFG5_ADDR               0x110
#define MIPI_CSIPHY_LNCK_MISC1_ADDR              0x128
#define MIPI_CSIPHY_GLBL_T_INIT_CFG0_ADDR        0x1E0
#define MIPI_CSIPHY_T_WAKEUP_CFG0_ADDR           0x1E8
#define MIPI_CSIPHY_GLBL_PWR_CFG_ADDR           0x0144
#define MIPI_CSIPHY_INTERRUPT_STATUS0_ADDR      0x0180
#define MIPI_CSIPHY_INTERRUPT_STATUS1_ADDR      0x0184
#define MIPI_CSIPHY_INTERRUPT_STATUS2_ADDR      0x0188
#define MIPI_CSIPHY_INTERRUPT_STATUS3_ADDR      0x018C
#define MIPI_CSIPHY_INTERRUPT_STATUS4_ADDR      0x0190
#define MIPI_CSIPHY_INTERRUPT_MASK0_ADDR        0x01A0
#define MIPI_CSIPHY_INTERRUPT_MASK1_ADDR        0x01A4
#define MIPI_CSIPHY_INTERRUPT_MASK2_ADDR        0x01A8
#define MIPI_CSIPHY_INTERRUPT_MASK3_ADDR        0x01AC
#define MIPI_CSIPHY_INTERRUPT_MASK4_ADDR        0x01B0
#define MIPI_CSIPHY_INTERRUPT_CLEAR0_ADDR       0x01C0
#define MIPI_CSIPHY_INTERRUPT_CLEAR1_ADDR       0x01C4
#define MIPI_CSIPHY_INTERRUPT_CLEAR2_ADDR       0x01C8
#define MIPI_CSIPHY_INTERRUPT_CLEAR3_ADDR       0x01CC
#define MIPI_CSIPHY_INTERRUPT_CLEAR4_ADDR       0x01D0

/* MIPI	CSID registers */
#define CSID_CORE_CTRL_ADDR                     0x4
#define CSID_RST_CMD_ADDR                       0x8
#define CSID_CID_LUT_VC_0_ADDR                  0xc
#define CSID_CID_LUT_VC_1_ADDR                  0x10
#define CSID_CID_LUT_VC_2_ADDR                  0x14
#define CSID_CID_LUT_VC_3_ADDR                  0x18
#define CSID_CID_n_CFG_ADDR                     0x1C
#define CSID_IRQ_CLEAR_CMD_ADDR                 0x5c
#define CSID_IRQ_MASK_ADDR                      0x60
#define CSID_IRQ_STATUS_ADDR                    0x64
#define CSID_CAPTURED_UNMAPPED_LONG_PKT_HDR_ADDR    0x68
#define CSID_CAPTURED_MMAPPED_LONG_PKT_HDR_ADDR     0x6c
#define CSID_CAPTURED_SHORT_PKT_ADDR                0x70
#define CSID_CAPTURED_LONG_PKT_HDR_ADDR             0x74
#define CSID_CAPTURED_LONG_PKT_FTR_ADDR             0x78
#define CSID_PIF_MISR_DL0_ADDR                      0x7C
#define CSID_PIF_MISR_DL1_ADDR                      0x80
#define CSID_PIF_MISR_DL2_ADDR                      0x84
#define CSID_PIF_MISR_DL3_ADDR                      0x88
#define CSID_STATS_TOTAL_PKTS_RCVD_ADDR             0x8C
#define CSID_STATS_ECC_ADDR                         0x90
#define CSID_STATS_CRC_ADDR                         0x94
#define CSID_TG_CTRL_ADDR                           0x9C
#define CSID_TG_VC_CFG_ADDR                         0xA0
#define CSID_TG_DT_n_CFG_0_ADDR                     0xA8
#define CSID_TG_DT_n_CFG_1_ADDR                     0xAC
#define CSID_TG_DT_n_CFG_2_ADDR                     0xB0
#define CSID_TG_DT_n_CFG_3_ADDR                     0xD8

/* Regulator Voltage and Current */

#define CAM_VAF_MINUV                 2800000
#define CAM_VAF_MAXUV                 2800000
#define CAM_VDIG_MINUV                    1200000
#define CAM_VDIG_MAXUV                    1200000
#define CAM_VANA_MINUV                    2800000
#define CAM_VANA_MAXUV                    2850000
#define CAM_CSI_VDD_MINUV                  1200000
#define CAM_CSI_VDD_MAXUV                  1200000

#define CAM_VAF_LOAD_UA               300000
#define CAM_VDIG_LOAD_UA                  105000
#define CAM_VANA_LOAD_UA                  85600
#define CAM_CSI_LOAD_UA                    20000

static struct clk *camio_cam_clk;
static struct clk *camio_vfe_clk;
static struct clk *camio_csi_src_clk;
static struct clk *camio_csi1_src_clk;
static struct clk *camio_csi0_vfe_clk;
static struct clk *camio_csi0_clk;
static struct clk *camio_csi0_pclk;
static struct clk *camio_csi_pix_clk;
static struct clk *camio_csi_rdi_clk;
static struct clk *camio_csiphy0_timer_clk;
static struct clk *camio_csiphy1_timer_clk;
static struct clk *camio_vfe_axi_clk;
static struct clk *camio_vfe_pclk;
static struct clk *camio_csi0_phy_clk;
static struct clk *camio_csiphy_timer_src_clk;

/*static struct clk *camio_vfe_pclk;*/
static struct clk *camio_jpeg_clk;
static struct clk *camio_jpeg_pclk;
static struct clk *camio_vpe_clk;
static struct clk *camio_vpe_pclk;
static struct regulator *fs_vfe;
static struct regulator *fs_ijpeg;
static struct regulator *fs_vpe;
static struct regulator *cam_vana;
static struct regulator *cam_vio;
static struct regulator *cam_vdig;
static struct regulator *cam_vaf;
static struct regulator *mipi_csi_vdd;

static struct msm_camera_io_clk camio_clk;
static struct platform_device *camio_dev;
static struct resource *csidio, *csiphyio;
static struct resource *csid_mem, *csiphy_mem;
static struct resource *csid_irq, *csiphy_irq;
void __iomem *csidbase, *csiphybase;

static struct msm_bus_vectors cam_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_preview_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1521190000,
		.ib  = 1521190000,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_video_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1521190000,
		.ib  = 1521190000,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1521190000,
		.ib  = 1521190000,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_snapshot_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1521190000,
		.ib  = 1521190000,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1521190000,
		.ib  = 1521190000,
	},
};

static struct msm_bus_paths cam_bus_client_config[] = {
	{
		ARRAY_SIZE(cam_init_vectors),
		cam_init_vectors,
	},
	{
		ARRAY_SIZE(cam_preview_vectors),
		cam_preview_vectors,
	},
	{
		ARRAY_SIZE(cam_video_vectors),
		cam_video_vectors,
	},
	{
		ARRAY_SIZE(cam_snapshot_vectors),
		cam_snapshot_vectors,
	},
};

static struct msm_bus_scale_pdata cam_bus_client_pdata = {
		cam_bus_client_config,
		ARRAY_SIZE(cam_bus_client_config),
		.name = "msm_camera",
};


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
	char line_str[128], *p_str;
	int i;
	u32 *p = (u32 *) addr;
	u32 data;
	CDBG("%s: %p %d\n", __func__, addr, size);
	line_str[0] = '\0';
	p_str = line_str;
	for (i = 0; i < size/4; i++) {
		if (i % 4 == 0) {
			sprintf(p_str, "%08x: ", (u32) p);
			p_str += 10;
		}
		data = readl_relaxed(p++);
		sprintf(p_str, "%08x ", data);
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

static int msm_camera_vreg_enable(struct platform_device *pdev)
{
	if (mipi_csi_vdd == NULL) {
		mipi_csi_vdd = regulator_get(&pdev->dev, "mipi_csi_vdd");
		if (IS_ERR(mipi_csi_vdd)) {
			CDBG("%s: VREG MIPI CSI VDD get failed\n", __func__);
			mipi_csi_vdd = NULL;
			return -ENODEV;
		}
		if (regulator_set_voltage(mipi_csi_vdd, CAM_CSI_VDD_MINUV,
			CAM_CSI_VDD_MAXUV)) {
			CDBG("%s: VREG MIPI CSI VDD set voltage failed\n",
				__func__);
			goto mipi_csi_vdd_put;
		}
		if (regulator_set_optimum_mode(mipi_csi_vdd,
			CAM_CSI_LOAD_UA) < 0) {
			CDBG("%s: VREG MIPI CSI set optimum mode failed\n",
				__func__);
			goto mipi_csi_vdd_release;
		}
		if (regulator_enable(mipi_csi_vdd)) {
			CDBG("%s: VREG MIPI CSI VDD enable failed\n",
				__func__);
			goto mipi_csi_vdd_disable;
		}
	}
	if (cam_vana == NULL) {
		cam_vana = regulator_get(&pdev->dev, "cam_vana");
		if (IS_ERR(cam_vana)) {
			CDBG("%s: VREG CAM VANA get failed\n", __func__);
			cam_vana = NULL;
			goto mipi_csi_vdd_disable;
		}
		if (regulator_set_voltage(cam_vana, CAM_VANA_MINUV,
			CAM_VANA_MAXUV)) {
			CDBG("%s: VREG CAM VANA set voltage failed\n",
				__func__);
			goto cam_vana_put;
		}
		if (regulator_set_optimum_mode(cam_vana,
			CAM_VANA_LOAD_UA) < 0) {
			CDBG("%s: VREG CAM VANA set optimum mode failed\n",
				__func__);
			goto cam_vana_release;
		}
		if (regulator_enable(cam_vana)) {
			CDBG("%s: VREG CAM VANA enable failed\n", __func__);
			goto cam_vana_disable;
		}
	}
	if (cam_vio == NULL) {
		cam_vio = regulator_get(&pdev->dev, "cam_vio");
		if (IS_ERR(cam_vio)) {
			CDBG("%s: VREG VIO get failed\n", __func__);
			cam_vio = NULL;
			goto cam_vana_disable;
		}
		if (regulator_enable(cam_vio)) {
			CDBG("%s: VREG VIO enable failed\n", __func__);
			goto cam_vio_put;
		}
	}
	if (cam_vdig == NULL) {
		cam_vdig = regulator_get(&pdev->dev, "cam_vdig");
		if (IS_ERR(cam_vdig)) {
			CDBG("%s: VREG CAM VDIG get failed\n", __func__);
			cam_vdig = NULL;
			goto cam_vio_disable;
		}
		if (regulator_set_voltage(cam_vdig, CAM_VDIG_MINUV,
			CAM_VDIG_MAXUV)) {
			CDBG("%s: VREG CAM VDIG set voltage failed\n",
				__func__);
			goto cam_vdig_put;
		}
		if (regulator_set_optimum_mode(cam_vdig,
			CAM_VDIG_LOAD_UA) < 0) {
			CDBG("%s: VREG CAM VDIG set optimum mode failed\n",
				__func__);
			goto cam_vdig_release;
		}
		if (regulator_enable(cam_vdig)) {
			CDBG("%s: VREG CAM VDIG enable failed\n", __func__);
			goto cam_vdig_disable;
		}
	}
	if (cam_vaf == NULL) {
		cam_vaf = regulator_get(&pdev->dev, "cam_vaf");
		if (IS_ERR(cam_vaf)) {
			CDBG("%s: VREG CAM VAF get failed\n", __func__);
			cam_vaf = NULL;
			goto cam_vdig_disable;
		}
		if (regulator_set_voltage(cam_vaf, CAM_VAF_MINUV,
			CAM_VAF_MAXUV)) {
			CDBG("%s: VREG CAM VAF set voltage failed\n",
				__func__);
			goto cam_vaf_put;
		}
		if (regulator_set_optimum_mode(cam_vaf,
			CAM_VAF_LOAD_UA) < 0) {
			CDBG("%s: VREG CAM VAF set optimum mode failed\n",
				__func__);
			goto cam_vaf_release;
		}
		if (regulator_enable(cam_vaf)) {
			CDBG("%s: VREG CAM VAF enable failed\n", __func__);
			goto cam_vaf_disable;
		}
	}
	if (fs_vfe == NULL) {
		fs_vfe = regulator_get(&pdev->dev, "fs_vfe");
		if (IS_ERR(fs_vfe)) {
			CDBG("%s: Regulator FS_VFE get failed %ld\n", __func__,
				PTR_ERR(fs_vfe));
			fs_vfe = NULL;
		} else if (regulator_enable(fs_vfe)) {
			CDBG("%s: Regulator FS_VFE enable failed\n", __func__);
			regulator_put(fs_vfe);
		}
	}
	return 0;

cam_vaf_disable:
	regulator_set_optimum_mode(cam_vaf, 0);
cam_vaf_release:
	regulator_set_voltage(cam_vaf, 0, CAM_VAF_MAXUV);
	regulator_disable(cam_vaf);
cam_vaf_put:
	regulator_put(cam_vaf);
	cam_vaf = NULL;
cam_vdig_disable:
	regulator_set_optimum_mode(cam_vdig, 0);
cam_vdig_release:
	regulator_set_voltage(cam_vdig, 0, CAM_VDIG_MAXUV);
	regulator_disable(cam_vdig);
cam_vdig_put:
	regulator_put(cam_vdig);
	cam_vdig = NULL;
cam_vio_disable:
	regulator_disable(cam_vio);
cam_vio_put:
	regulator_put(cam_vio);
	cam_vio = NULL;
cam_vana_disable:
	regulator_set_optimum_mode(cam_vana, 0);
cam_vana_release:
	regulator_set_voltage(cam_vana, 0, CAM_VANA_MAXUV);
	regulator_disable(cam_vana);
cam_vana_put:
	regulator_put(cam_vana);
	cam_vana = NULL;
mipi_csi_vdd_disable:
	regulator_set_optimum_mode(mipi_csi_vdd, 0);
mipi_csi_vdd_release:
	regulator_set_voltage(mipi_csi_vdd, 0, CAM_CSI_VDD_MAXUV);
	regulator_disable(mipi_csi_vdd);

mipi_csi_vdd_put:
	regulator_put(mipi_csi_vdd);
	mipi_csi_vdd = NULL;
	return -ENODEV;
}

static void msm_camera_vreg_disable(void)
{
	if (mipi_csi_vdd) {
		regulator_set_voltage(mipi_csi_vdd, 0, CAM_CSI_VDD_MAXUV);
		regulator_set_optimum_mode(mipi_csi_vdd, 0);
		regulator_disable(mipi_csi_vdd);
		regulator_put(mipi_csi_vdd);
		mipi_csi_vdd = NULL;
	}

	if (cam_vana) {
		regulator_set_voltage(cam_vana, 0, CAM_VANA_MAXUV);
		regulator_set_optimum_mode(cam_vana, 0);
		regulator_disable(cam_vana);
		regulator_put(cam_vana);
		cam_vana = NULL;
	}

	if (cam_vio) {
		regulator_disable(cam_vio);
		regulator_put(cam_vio);
		cam_vio = NULL;
	}

	if (cam_vdig) {
		regulator_set_voltage(cam_vdig, 0, CAM_VDIG_MAXUV);
		regulator_set_optimum_mode(cam_vdig, 0);
		regulator_disable(cam_vdig);
		regulator_put(cam_vdig);
		cam_vdig = NULL;
	}

	if (cam_vaf) {
		regulator_set_voltage(cam_vaf, 0, CAM_VAF_MAXUV);
		regulator_set_optimum_mode(cam_vaf, 0);
		regulator_disable(cam_vaf);
		regulator_put(cam_vaf);
		cam_vaf = NULL;
	}

	if (fs_vfe) {
		regulator_disable(fs_vfe);
		regulator_put(fs_vfe);
		fs_vfe = NULL;
	}
}

int msm_camio_clk_enable(enum msm_camio_clk_type clktype)
{
	int rc = 0;
	struct clk *clk = NULL;
	struct msm_camera_sensor_info *sinfo = camio_dev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	uint8_t csid_core = camdev->csid_core;

	switch (clktype) {
	case CAMIO_CAM_MCLK_CLK:
		camio_cam_clk =
		clk = clk_get(&camio_dev->dev, "cam_clk");
		msm_camio_clk_rate_set_2(clk, camio_clk.mclk_clk_rate);
		break;

	case CAMIO_VFE_CLK:
		camio_vfe_clk =
		clk = clk_get(NULL, "vfe_clk");
		msm_camio_clk_rate_set_2(clk, camio_clk.vfe_clk_rate);
		break;

	case CAMIO_VFE_AXI_CLK:
		camio_vfe_axi_clk =
		clk = clk_get(NULL, "vfe_axi_clk");
		break;

	case CAMIO_VFE_PCLK:
		camio_vfe_pclk =
		clk = clk_get(NULL, "vfe_pclk");
		break;

	case CAMIO_CSI0_VFE_CLK:
		camio_csi0_vfe_clk =
		clk = clk_get(NULL, "csi_vfe_clk");
		break;
/*
	case CAMIO_CSI1_VFE_CLK:
		camio_csi1_vfe_clk =
		clk = clk_get(&camio_dev->dev, "csi_vfe_clk");
		break;
*/
	case CAMIO_CSI_SRC_CLK:
		camio_csi_src_clk =
		clk = clk_get(NULL, "csi_src_clk");
		msm_camio_clk_rate_set_2(clk, 177780000);
		break;

	case CAMIO_CSI1_SRC_CLK:
		camio_csi1_src_clk =
		clk = clk_get(&camio_dev->dev, "csi_src_clk");
		msm_camio_clk_rate_set_2(clk, 177780000);
		break;

	case CAMIO_CSI0_CLK:
		camio_csi0_clk =
		clk = clk_get(&camio_dev->dev, "csi_clk");
		break;

	case CAMIO_CSI0_PHY_CLK:
		camio_csi0_phy_clk =
		clk = clk_get(&camio_dev->dev, "csi_phy_clk");
		break;

	case CAMIO_CSI_PIX_CLK:
		camio_csi_pix_clk =
		clk = clk_get(NULL, "csi_pix_clk");
		/* mux to select between csid0 and csid1 */
		msm_camio_clk_rate_set_2(clk, csid_core);
		break;

	case CAMIO_CSI_RDI_CLK:
		camio_csi_rdi_clk =
		clk = clk_get(NULL, "csi_rdi_clk");
		/* mux to select between csid0 and csid1 */
		msm_camio_clk_rate_set_2(clk, csid_core);
		break;

	case CAMIO_CSIPHY0_TIMER_CLK:
		camio_csiphy0_timer_clk =
		clk = clk_get(NULL, "csi0phy_timer_clk");
		break;

	case CAMIO_CSIPHY1_TIMER_CLK:
		camio_csiphy1_timer_clk =
		clk = clk_get(NULL, "csi1phy_timer_clk");
		break;

	case CAMIO_CSIPHY_TIMER_SRC_CLK:
		camio_csiphy_timer_src_clk =
		clk = clk_get(NULL, "csiphy_timer_src_clk");
		msm_camio_clk_rate_set_2(clk, 177780000);
		break;

	case CAMIO_CSI0_PCLK:
		camio_csi0_pclk =
		clk = clk_get(NULL, "csi_pclk");
		break;

	case CAMIO_JPEG_CLK:
		camio_jpeg_clk =
		clk = clk_get(NULL, "ijpeg_clk");
		clk_set_min_rate(clk, 144000000);
		break;

	case CAMIO_JPEG_PCLK:
		camio_jpeg_pclk =
		clk = clk_get(NULL, "ijpeg_pclk");
		break;

	case CAMIO_VPE_CLK:
		camio_vpe_clk =
		clk = clk_get(NULL, "vpe_clk");
		msm_camio_clk_set_min_rate(clk, 150000000);
		break;

	case CAMIO_VPE_PCLK:
		camio_vpe_pclk =
		clk = clk_get(NULL, "vpe_pclk");
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
	case CAMIO_CAM_MCLK_CLK:
		clk = camio_cam_clk;
		break;

	case CAMIO_VFE_CLK:
		clk = camio_vfe_clk;
		break;

	case CAMIO_VFE_AXI_CLK:
		clk = camio_vfe_axi_clk;
		break;

	case CAMIO_VFE_PCLK:
		clk = camio_vfe_pclk;
		break;

	case CAMIO_CSI0_VFE_CLK:
		clk = camio_csi0_vfe_clk;
		break;

	case CAMIO_CSI_SRC_CLK:
		clk = camio_csi_src_clk;
		break;

	case CAMIO_CSI0_CLK:
		clk = camio_csi0_clk;
		break;

	case CAMIO_CSI0_PHY_CLK:
		clk = camio_csi0_phy_clk;
		break;

	case CAMIO_CSI_PIX_CLK:
		clk = camio_csi_pix_clk;
		break;

	case CAMIO_CSI_RDI_CLK:
		clk = camio_csi_rdi_clk;
		break;

	case CAMIO_CSIPHY0_TIMER_CLK:
		clk = camio_csiphy0_timer_clk;
		break;

	case CAMIO_CSIPHY_TIMER_SRC_CLK:
		clk = camio_csiphy_timer_src_clk;
		break;

	case CAMIO_CSI0_PCLK:
		clk = camio_csi0_pclk;
		break;

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
		clk_put(clk);
	} else
		rc = PTR_ERR(clk);

	if (rc < 0)
		pr_err("%s(%d) failed %d\n", __func__, clktype, rc);

	return rc;
}

void msm_camio_vfe_clk_rate_set(int rate)
{
	struct clk *clk = camio_vfe_clk;
	if (rate > clk_get_rate(clk))
		clk_set_rate(clk, rate);
}

void msm_camio_clk_rate_set(int rate)
{
	struct clk *clk = camio_cam_clk;
	clk_set_rate(clk, rate);
}

void msm_camio_clk_rate_set_2(struct clk *clk, int rate)
{
	clk_set_rate(clk, rate);
}

void msm_camio_clk_set_min_rate(struct clk *clk, int rate)
{
	clk_set_min_rate(clk, rate);
}

#if DBG_CSID
static irqreturn_t msm_io_csi_irq(int irq_num, void *data)
{
	uint32_t irq;
	irq = msm_io_r(csidbase + CSID_IRQ_STATUS_ADDR);
	CDBG("%s CSID_IRQ_STATUS_ADDR = 0x%x\n", __func__, irq);
	msm_io_w(irq, csidbase + CSID_IRQ_CLEAR_CMD_ADDR);
	irq = msm_io_r(csidbase + 0x7C);
	CDBG("%s CSID_PIF_MISR_DL0 = 0x%x\n", __func__, irq);
	irq = msm_io_r(csidbase + 0x80);
	CDBG("%s CSID_PIF_MISR_DL1 = 0x%x\n", __func__, irq);
	irq = msm_io_r(csidbase + 0x84);
	CDBG("%s CSID_PIF_MISR_DL2 = 0x%x\n", __func__, irq);
	irq = msm_io_r(csidbase + 0x88);
	CDBG("%s CSID_PIF_MISR_DL3 = 0x%x\n", __func__, irq);
	irq = msm_io_r(csidbase + 0x8C);
	CDBG("%s PACKET Count = %d\n", __func__, irq);
	return IRQ_HANDLED;
}
#endif
/*
void msm_io_read_interrupt(void)
{
	uint32_t irq;
	irq = msm_io_r(csiphybase + MIPI_CSIPHY_INTERRUPT_STATUS0_ADDR);
	CDBG("%s MIPI_CSIPHY_INTERRUPT_STATUS0 = 0x%x\n", __func__, irq);
	irq = msm_io_r(csiphybase + MIPI_CSIPHY_INTERRUPT_STATUS0_ADDR);
	CDBG("%s MIPI_CSIPHY_INTERRUPT_STATUS0 = 0x%x\n", __func__, irq);
	irq = msm_io_r(csiphybase + MIPI_CSIPHY_INTERRUPT_STATUS1_ADDR);
	CDBG("%s MIPI_CSIPHY_INTERRUPT_STATUS1 = 0x%x\n", __func__, irq);
	irq = msm_io_r(csiphybase + MIPI_CSIPHY_INTERRUPT_STATUS2_ADDR);
	CDBG("%s MIPI_CSIPHY_INTERRUPT_STATUS2 = 0x%x\n", __func__, irq);
	irq = msm_io_r(csiphybase + MIPI_CSIPHY_INTERRUPT_STATUS3_ADDR);
	CDBG("%s MIPI_CSIPHY_INTERRUPT_STATUS3 = 0x%x\n", __func__, irq);
	irq = msm_io_r(csiphybase + MIPI_CSIPHY_INTERRUPT_STATUS4_ADDR);
	CDBG("%s MIPI_CSIPHY_INTERRUPT_STATUS4 = 0x%x\n", __func__, irq);
	msm_io_w(irq, csiphybase + MIPI_CSIPHY_INTERRUPT_CLEAR0_ADDR);
	msm_io_w(irq, csiphybase + MIPI_CSIPHY_INTERRUPT_CLEAR1_ADDR);
	msm_io_w(irq, csiphybase + MIPI_CSIPHY_INTERRUPT_CLEAR2_ADDR);
	msm_io_w(irq, csiphybase + MIPI_CSIPHY_INTERRUPT_CLEAR3_ADDR);
	msm_io_w(irq, csiphybase + MIPI_CSIPHY_INTERRUPT_CLEAR4_ADDR);
	msm_io_w(0x1, csiphybase + 0x164);
	msm_io_w(0x0, csiphybase + 0x164);
	return;
}
*/
#if DBG_CSIPHY
static irqreturn_t msm_io_csiphy_irq(int irq_num, void *data)
{
	uint32_t irq;
	irq = msm_io_r(csiphybase + MIPI_CSIPHY_INTERRUPT_STATUS0_ADDR);
	msm_io_w(irq, csiphybase + MIPI_CSIPHY_INTERRUPT_CLEAR0_ADDR);
	CDBG("%s MIPI_CSIPHY_INTERRUPT_STATUS0 = 0x%x\n", __func__, irq);
	irq = msm_io_r(csiphybase + MIPI_CSIPHY_INTERRUPT_STATUS1_ADDR);
	msm_io_w(irq, csiphybase + MIPI_CSIPHY_INTERRUPT_CLEAR1_ADDR);
	CDBG("%s MIPI_CSIPHY_INTERRUPT_STATUS1 = 0x%x\n", __func__, irq);
	irq = msm_io_r(csiphybase + MIPI_CSIPHY_INTERRUPT_STATUS2_ADDR);
	msm_io_w(irq, csiphybase + MIPI_CSIPHY_INTERRUPT_CLEAR2_ADDR);
	CDBG("%s MIPI_CSIPHY_INTERRUPT_STATUS2 = 0x%x\n", __func__, irq);
	irq = msm_io_r(csiphybase + MIPI_CSIPHY_INTERRUPT_STATUS3_ADDR);
	msm_io_w(irq, csiphybase + MIPI_CSIPHY_INTERRUPT_CLEAR3_ADDR);
	CDBG("%s MIPI_CSIPHY_INTERRUPT_STATUS3 = 0x%x\n", __func__, irq);
	irq = msm_io_r(csiphybase + MIPI_CSIPHY_INTERRUPT_STATUS4_ADDR);
	msm_io_w(irq, csiphybase + MIPI_CSIPHY_INTERRUPT_CLEAR4_ADDR);
	CDBG("%s MIPI_CSIPHY_INTERRUPT_STATUS4 = 0x%x\n", __func__, irq);
	msm_io_w(0x1, csiphybase + 0x164);
	msm_io_w(0x0, csiphybase + 0x164);
	return IRQ_HANDLED;
}
#endif
static int msm_camio_enable_all_clks(uint8_t csid_core)
{
	int rc = 0;

	rc = msm_camio_clk_enable(CAMIO_CSI_SRC_CLK);
	if (rc < 0)
		goto csi_src_fail;
	if (csid_core == 1) {
		rc = msm_camio_clk_enable(CAMIO_CSI1_SRC_CLK);
		if (rc < 0)
			goto csi1_src_fail;
	}
	rc = msm_camio_clk_enable(CAMIO_CSI0_CLK);
	if (rc < 0)
		goto csi0_fail;
	rc = msm_camio_clk_enable(CAMIO_CSI0_PHY_CLK);
	if (rc < 0)
		goto csi0_phy_fail;
	rc = msm_camio_clk_enable(CAMIO_CSIPHY_TIMER_SRC_CLK);
	if (rc < 0)
		goto csiphy_timer_src_fail;
	if (csid_core == 0) {
		rc = msm_camio_clk_enable(CAMIO_CSIPHY0_TIMER_CLK);
		if (rc < 0)
			goto csiphy0_timer_fail;
	} else if (csid_core == 1) {
		rc = msm_camio_clk_enable(CAMIO_CSIPHY1_TIMER_CLK);
		if (rc < 0)
			goto csiphy1_timer_fail;
	}
	rc = msm_camio_clk_enable(CAMIO_CSI0_PCLK);
	if (rc < 0)
		goto csi0p_fail;

	rc = msm_camio_clk_enable(CAMIO_VFE_CLK);
	if (rc < 0)
		goto vfe_fail;
	rc = msm_camio_clk_enable(CAMIO_VFE_AXI_CLK);
	if (rc < 0)
		goto axi_fail;
	rc = msm_camio_clk_enable(CAMIO_VFE_PCLK);
	if (rc < 0)
		goto vfep_fail;

	rc = msm_camio_clk_enable(CAMIO_CSI0_VFE_CLK);
	if (rc < 0)
		goto csi0_vfe_fail;
	rc = msm_camio_clk_enable(CAMIO_CSI_PIX_CLK);
	if (rc < 0)
		goto csi_pix_fail;
	rc = msm_camio_clk_enable(CAMIO_CSI_RDI_CLK);
	if (rc < 0)
		goto csi_rdi_fail;
	return rc;

csi_rdi_fail:
	msm_camio_clk_disable(CAMIO_CSI_PIX_CLK);
csi_pix_fail:
	msm_camio_clk_disable(CAMIO_CSI0_VFE_CLK);
csi0_vfe_fail:
	msm_camio_clk_disable(CAMIO_VFE_PCLK);
vfep_fail:
	msm_camio_clk_disable(CAMIO_VFE_AXI_CLK);
axi_fail:
	msm_camio_clk_disable(CAMIO_VFE_CLK);
vfe_fail:
	msm_camio_clk_disable(CAMIO_CSI0_PCLK);
csi0p_fail:
	msm_camio_clk_disable(CAMIO_CSIPHY0_TIMER_CLK);
csiphy1_timer_fail:
	msm_camio_clk_disable(CAMIO_CSIPHY1_TIMER_CLK);
csiphy0_timer_fail:
	msm_camio_clk_disable(CAMIO_CSIPHY_TIMER_SRC_CLK);
csiphy_timer_src_fail:
	msm_camio_clk_disable(CAMIO_CSI0_PHY_CLK);
csi0_phy_fail:
	msm_camio_clk_disable(CAMIO_CSI0_CLK);
csi0_fail:
	msm_camio_clk_disable(CAMIO_CSI1_SRC_CLK);
csi1_src_fail:
	msm_camio_clk_disable(CAMIO_CSI_SRC_CLK);
csi_src_fail:
	return rc;
}

static void msm_camio_disable_all_clks(uint8_t csid_core)
{
	msm_camio_clk_disable(CAMIO_CSI_RDI_CLK);
	msm_camio_clk_disable(CAMIO_CSI_PIX_CLK);
	msm_camio_clk_disable(CAMIO_CSI0_VFE_CLK);
	msm_camio_clk_disable(CAMIO_VFE_PCLK);
	msm_camio_clk_disable(CAMIO_VFE_AXI_CLK);
	msm_camio_clk_disable(CAMIO_VFE_CLK);
	msm_camio_clk_disable(CAMIO_CSI0_PCLK);
	if (csid_core == 0)
		msm_camio_clk_disable(CAMIO_CSIPHY0_TIMER_CLK);
	else if (csid_core == 1)
		msm_camio_clk_disable(CAMIO_CSIPHY1_TIMER_CLK);
	msm_camio_clk_disable(CAMIO_CSIPHY_TIMER_SRC_CLK);
	msm_camio_clk_disable(CAMIO_CSI0_PHY_CLK);
	msm_camio_clk_disable(CAMIO_CSI0_CLK);
	if (csid_core == 1)
		msm_camio_clk_disable(CAMIO_CSI1_SRC_CLK);
	msm_camio_clk_disable(CAMIO_CSI_SRC_CLK);
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
	(void)clk_rate;
	fs_vpe = regulator_get(NULL, "fs_vpe");
	if (IS_ERR(fs_vpe)) {
		CDBG("%s: Regulator FS_VPE get failed %ld\n", __func__,
			PTR_ERR(fs_vpe));
		fs_vpe = NULL;
	} else if (regulator_enable(fs_vpe)) {
		CDBG("%s: Regulator FS_VPE enable failed\n", __func__);
		regulator_put(fs_vpe);
	}

	rc = msm_camio_clk_enable(CAMIO_VPE_CLK);
	if (rc < 0)
		return rc;
	rc = msm_camio_clk_enable(CAMIO_VPE_PCLK);
	return rc;
}

int msm_camio_enable(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	uint8_t csid_core = camdev->csid_core;
	char csid[] = "csid0";
	char csiphy[] = "csiphy0";
	if (csid_core < 0 || csid_core > 2)
		return -ENODEV;

	csid[4] = '0' + csid_core;
	csiphy[6] = '0' + csid_core;

	camio_dev = pdev;
	camio_clk = camdev->ioclk;

	rc = msm_camio_enable_all_clks(csid_core);
	if (rc < 0)
		return rc;

	csid_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, csid);
	if (!csid_mem) {
		pr_err("%s: no mem resource?\n", __func__);
		return -ENODEV;
	}
	csid_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ, csid);
	if (!csid_irq) {
		pr_err("%s: no irq resource?\n", __func__);
		return -ENODEV;
	}
	csiphy_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, csiphy);
	if (!csiphy_mem) {
		pr_err("%s: no mem resource?\n", __func__);
		return -ENODEV;
	}
	csiphy_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ, csiphy);
	if (!csiphy_irq) {
		pr_err("%s: no irq resource?\n", __func__);
		return -ENODEV;
	}

	csidio = request_mem_region(csid_mem->start,
		resource_size(csid_mem), pdev->name);
	if (!csidio) {
		rc = -EBUSY;
		goto common_fail;
	}
	csidbase = ioremap(csid_mem->start,
		resource_size(csid_mem));
	if (!csidbase) {
		rc = -ENOMEM;
		goto csi_busy;
	}
#if DBG_CSID
	rc = request_irq(csid_irq->start, msm_io_csi_irq,
		IRQF_TRIGGER_RISING, "csid", 0);
	if (rc < 0)
		goto csi_irq_fail;
#endif
	csiphyio = request_mem_region(csiphy_mem->start,
		resource_size(csiphy_mem), pdev->name);
	if (!csidio) {
		rc = -EBUSY;
		goto csi_irq_fail;
	}
	csiphybase = ioremap(csiphy_mem->start,
		resource_size(csiphy_mem));
	if (!csiphybase) {
		rc = -ENOMEM;
		goto csiphy_busy;
	}
#if DBG_CSIPHY
	rc = request_irq(csiphy_irq->start, msm_io_csiphy_irq,
		IRQF_TRIGGER_RISING, "csiphy", 0);
	if (rc < 0)
		goto csiphy_irq_fail;
#endif
	rc = msm_ispif_init(pdev);
	if (rc < 0)
		goto csiphy_irq_fail;
	CDBG("camio enable done\n");
	return 0;
csiphy_irq_fail:
	iounmap(csiphybase);
csiphy_busy:
	release_mem_region(csiphy_mem->start, resource_size(csiphy_mem));
csi_irq_fail:
	iounmap(csidbase);
csi_busy:
	release_mem_region(csid_mem->start, resource_size(csid_mem));
common_fail:
	msm_camio_disable_all_clks(csid_core);
	msm_camera_vreg_disable();
	camdev->camera_gpio_off();
	return rc;
}

void msm_camio_disable(struct platform_device *pdev)
{
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	uint8_t csid_core = camdev->csid_core;
#if DBG_CSIPHY
	free_irq(csiphy_irq->start, 0);
#endif
	iounmap(csiphybase);
	release_mem_region(csiphy_mem->start, resource_size(csiphy_mem));

#if DBG_CSID
	free_irq(csid_irq, 0);
#endif
	iounmap(csidbase);
	release_mem_region(csid_mem->start, resource_size(csid_mem));

	msm_camio_disable_all_clks(csid_core);
	msm_ispif_release(pdev);
}

int msm_camio_sensor_clk_on(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	camio_dev = pdev;
	camio_clk = camdev->ioclk;

	msm_camera_vreg_enable(pdev);
	msleep(20);
	rc = camdev->camera_gpio_on();
	if (rc < 0)
		return rc;
	return msm_camio_clk_enable(CAMIO_CAM_MCLK_CLK);
}

int msm_camio_sensor_clk_off(struct platform_device *pdev)
{
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	msm_camera_vreg_disable();
	camdev->camera_gpio_off();
	return msm_camio_clk_disable(CAMIO_CAM_MCLK_CLK);
}

void msm_camio_vfe_blk_reset(void)
{
	return;
}

int msm_camio_probe_on(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	camio_dev = pdev;
	camio_clk = camdev->ioclk;

	rc = camdev->camera_gpio_on();
	if (rc < 0)
		return rc;
	msm_camera_vreg_enable(pdev);
	return msm_camio_clk_enable(CAMIO_CAM_MCLK_CLK);
}

int msm_camio_probe_off(struct platform_device *pdev)
{
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	msm_camera_vreg_disable();
	camdev->camera_gpio_off();
	return msm_camio_clk_disable(CAMIO_CAM_MCLK_CLK);
}

int msm_camio_csid_cid_lut(struct msm_camera_csid_lut_params *csid_lut_params)
{
	int rc = 0, i = 0;
	uint32_t val = 0;

	for (i = 0; i < csid_lut_params->num_cid && i < 4; i++)	{
		if (csid_lut_params->vc_cfg[i].dt < 0x12 ||
			csid_lut_params->vc_cfg[i].dt > 0x37) {
			CDBG("%s: unsupported data type 0x%x\n",
				 __func__, csid_lut_params->vc_cfg[i].dt);
			return rc;
		}
		val = msm_io_r(csidbase + CSID_CID_LUT_VC_0_ADDR +
		(csid_lut_params->vc_cfg[i].cid >> 2) * 4)
		& ~(0xFF << csid_lut_params->vc_cfg[i].cid * 8);
		val |= csid_lut_params->vc_cfg[i].dt <<
			csid_lut_params->vc_cfg[i].cid * 8;
		msm_io_w(val, csidbase + CSID_CID_LUT_VC_0_ADDR +
			(csid_lut_params->vc_cfg[i].cid >> 2) * 4);
		val = csid_lut_params->vc_cfg[i].decode_format << 4 | 0x3;
		msm_io_w(val, csidbase + CSID_CID_n_CFG_ADDR +
			(csid_lut_params->vc_cfg[i].cid * 4));
	}
	return rc;
}

int msm_camio_csid_config(struct msm_camera_csid_params *csid_params)
{
	int rc = 0;
	uint32_t val = 0;
	val = csid_params->lane_cnt - 1;
	val |= csid_params->lane_assign << 2;
	val |= 0x1 << 10;
	val |= 0x1 << 11;
	val |= 0x1 << 12;
	val |= 0x1 << 28;
	msm_io_w(val, csidbase + CSID_CORE_CTRL_ADDR);

	rc = msm_camio_csid_cid_lut(&csid_params->lut_params);
	if (rc < 0)
		return rc;

	msm_io_w(0xFFFFFFFF, csidbase + CSID_IRQ_MASK_ADDR);
	msm_io_w(0xFFFFFFFF, csidbase + CSID_IRQ_CLEAR_CMD_ADDR);

	msleep(20);
	return rc;
}

int msm_camio_csiphy_config(struct msm_camera_csiphy_params *csiphy_params)
{
	int rc = 0;
	int i = 0;
	uint32_t val = 0;
	if (csiphy_params->lane_cnt < 1 || csiphy_params->lane_cnt > 4) {
		CDBG("%s: unsupported lane cnt %d\n",
			__func__, csiphy_params->lane_cnt);
		return rc;
	}

	val = 0x3;
	msm_io_w((((1 << csiphy_params->lane_cnt) - 1) << 2) | val,
			 csiphybase + MIPI_CSIPHY_GLBL_PWR_CFG_ADDR);
	msm_io_w(0x1, csiphybase + MIPI_CSIPHY_GLBL_T_INIT_CFG0_ADDR);
	msm_io_w(0x1, csiphybase + MIPI_CSIPHY_T_WAKEUP_CFG0_ADDR);

	for (i = 0; i < csiphy_params->lane_cnt; i++) {
		msm_io_w(0x10, csiphybase + MIPI_CSIPHY_LNn_CFG1_ADDR + 0x40*i);
		msm_io_w(0x5F, csiphybase + MIPI_CSIPHY_LNn_CFG2_ADDR + 0x40*i);
		msm_io_w(csiphy_params->settle_cnt,
			csiphybase + MIPI_CSIPHY_LNn_CFG3_ADDR + 0x40*i);
		msm_io_w(0x00000052,
			csiphybase + MIPI_CSIPHY_LNn_CFG5_ADDR + 0x40*i);
	}

	msm_io_w(0x00000000, csiphybase + MIPI_CSIPHY_LNCK_CFG1_ADDR);
	msm_io_w(0x5F, csiphybase + MIPI_CSIPHY_LNCK_CFG2_ADDR);
	msm_io_w(csiphy_params->settle_cnt,
			 csiphybase + MIPI_CSIPHY_LNCK_CFG3_ADDR);
	msm_io_w(0x5, csiphybase + MIPI_CSIPHY_LNCK_CFG4_ADDR);
	msm_io_w(0x2, csiphybase + MIPI_CSIPHY_LNCK_CFG5_ADDR);
	msm_io_w(0x0, csiphybase + 0x128);

	for (i = 0; i <= csiphy_params->lane_cnt; i++) {
		msm_io_w(0xFFFFFFFF,
			csiphybase + MIPI_CSIPHY_INTERRUPT_MASK0_ADDR + 0x4*i);
		msm_io_w(0xFFFFFFFF,
			csiphybase + MIPI_CSIPHY_INTERRUPT_CLEAR0_ADDR + 0x4*i);
	}
	return rc;
}

void msm_camio_set_perf_lvl(enum msm_bus_perf_setting perf_setting)
{
	static uint32_t bus_perf_client;
	int rc = 0;
	switch (perf_setting) {
	case S_INIT:
		bus_perf_client =
			msm_bus_scale_register_client(&cam_bus_client_pdata);
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
	case S_DEFAULT:
		break;
	default:
		pr_warning("%s: INVALID CASE\n", __func__);
	}
}
