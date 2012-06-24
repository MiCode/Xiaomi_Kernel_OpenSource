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
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <mach/vreg.h>
#include <mach/clk.h>

#define CAMIF_CFG_RMSK             0x1fffff
#define CAM_SEL_BMSK               0x2
#define CAM_PCLK_SRC_SEL_BMSK      0x60000
#define CAM_PCLK_INVERT_BMSK       0x80000
#define CAM_PAD_REG_SW_RESET_BMSK  0x100000

#define EXT_CAM_HSYNC_POL_SEL_BMSK 0x10000
#define EXT_CAM_VSYNC_POL_SEL_BMSK 0x8000
#define MDDI_CLK_CHICKEN_BIT_BMSK  0x80

#define CAM_SEL_SHFT               0x1
#define CAM_PCLK_SRC_SEL_SHFT      0x11
#define CAM_PCLK_INVERT_SHFT       0x13
#define CAM_PAD_REG_SW_RESET_SHFT  0x14

#define EXT_CAM_HSYNC_POL_SEL_SHFT 0x10
#define EXT_CAM_VSYNC_POL_SEL_SHFT 0xF
#define MDDI_CLK_CHICKEN_BIT_SHFT  0x7

/* MIPI	CSI	controller registers */
#define	MIPI_PHY_CONTROL			0x00000000
#define	MIPI_PROTOCOL_CONTROL		0x00000004
#define	MIPI_INTERRUPT_STATUS		0x00000008
#define	MIPI_INTERRUPT_MASK			0x0000000C
#define	MIPI_CAMERA_CNTL			0x00000024
#define	MIPI_CALIBRATION_CONTROL	0x00000018
#define	MIPI_PHY_D0_CONTROL2		0x00000038
#define	MIPI_PHY_D1_CONTROL2		0x0000003C
#define	MIPI_PHY_D2_CONTROL2		0x00000040
#define	MIPI_PHY_D3_CONTROL2		0x00000044
#define	MIPI_PHY_CL_CONTROL			0x00000048
#define	MIPI_PHY_D0_CONTROL			0x00000034
#define	MIPI_PHY_D1_CONTROL			0x00000020
#define	MIPI_PHY_D2_CONTROL			0x0000002C
#define	MIPI_PHY_D3_CONTROL			0x00000030
#define	MIPI_PROTOCOL_CONTROL_SW_RST_BMSK			0x8000000
#define	MIPI_PROTOCOL_CONTROL_LONG_PACKET_HEADER_CAPTURE_BMSK	0x200000
#define	MIPI_PROTOCOL_CONTROL_DATA_FORMAT_BMSK			0x180000
#define	MIPI_PROTOCOL_CONTROL_DECODE_ID_BMSK			0x40000
#define	MIPI_PROTOCOL_CONTROL_ECC_EN_BMSK			0x20000
#define	MIPI_CALIBRATION_CONTROL_SWCAL_CAL_EN_SHFT		0x16
#define	MIPI_CALIBRATION_CONTROL_SWCAL_STRENGTH_OVERRIDE_EN_SHFT	0x15
#define	MIPI_CALIBRATION_CONTROL_CAL_SW_HW_MODE_SHFT		0x14
#define	MIPI_CALIBRATION_CONTROL_MANUAL_OVERRIDE_EN_SHFT	0x7
#define	MIPI_PROTOCOL_CONTROL_DATA_FORMAT_SHFT			0x13
#define	MIPI_PROTOCOL_CONTROL_DPCM_SCHEME_SHFT			0x1e
#define	MIPI_PHY_D0_CONTROL2_SETTLE_COUNT_SHFT			0x18
#define	MIPI_PHY_D0_CONTROL2_HS_TERM_IMP_SHFT			0x10
#define	MIPI_PHY_D0_CONTROL2_LP_REC_EN_SHFT				0x4
#define	MIPI_PHY_D0_CONTROL2_ERR_SOT_HS_EN_SHFT			0x3
#define	MIPI_PHY_D1_CONTROL2_SETTLE_COUNT_SHFT			0x18
#define	MIPI_PHY_D1_CONTROL2_HS_TERM_IMP_SHFT			0x10
#define	MIPI_PHY_D1_CONTROL2_LP_REC_EN_SHFT				0x4
#define	MIPI_PHY_D1_CONTROL2_ERR_SOT_HS_EN_SHFT			0x3
#define	MIPI_PHY_D2_CONTROL2_SETTLE_COUNT_SHFT			0x18
#define	MIPI_PHY_D2_CONTROL2_HS_TERM_IMP_SHFT			0x10
#define	MIPI_PHY_D2_CONTROL2_LP_REC_EN_SHFT				0x4
#define	MIPI_PHY_D2_CONTROL2_ERR_SOT_HS_EN_SHFT			0x3
#define	MIPI_PHY_D3_CONTROL2_SETTLE_COUNT_SHFT			0x18
#define	MIPI_PHY_D3_CONTROL2_HS_TERM_IMP_SHFT			0x10
#define	MIPI_PHY_D3_CONTROL2_LP_REC_EN_SHFT				0x4
#define	MIPI_PHY_D3_CONTROL2_ERR_SOT_HS_EN_SHFT			0x3
#define	MIPI_PHY_CL_CONTROL_HS_TERM_IMP_SHFT			0x18
#define	MIPI_PHY_CL_CONTROL_LP_REC_EN_SHFT				0x2
#define	MIPI_PHY_D0_CONTROL_HS_REC_EQ_SHFT				0x1c
#define	MIPI_PHY_D1_CONTROL_MIPI_CLK_PHY_SHUTDOWNB_SHFT		0x9
#define	MIPI_PHY_D1_CONTROL_MIPI_DATA_PHY_SHUTDOWNB_SHFT	0x8

#define	CAMIO_VFE_CLK_SNAP			122880000
#define	CAMIO_VFE_CLK_PREV			122880000

/* AXI rates in KHz */
#define MSM_AXI_QOS_PREVIEW     192000
#define MSM_AXI_QOS_SNAPSHOT    192000
#define MSM_AXI_QOS_RECORDING   192000

static struct clk *camio_vfe_mdc_clk;
static struct clk *camio_mdc_clk;
static struct clk *camio_vfe_clk;
static struct clk *camio_vfe_camif_clk;
static struct clk *camio_vfe_pbdg_clk;
static struct clk *camio_cam_m_clk;
static struct clk *camio_camif_pad_pbdg_clk;
static struct clk *camio_csi_clk;
static struct clk *camio_csi_pclk;
static struct clk *camio_csi_vfe_clk;
static struct clk *camio_vpe_clk;
static struct regulator *fs_vpe;
static struct msm_camera_io_ext camio_ext;
static struct msm_camera_io_clk camio_clk;
static struct resource *camifpadio, *csiio;
void __iomem *camifpadbase, *csibase;
static uint32_t vpe_clk_rate;

static struct regulator_bulk_data regs[] = {
	{ .supply = "gp2",  .min_uV = 2600000, .max_uV = 2600000 },
	{ .supply = "lvsw1" },
	{ .supply = "fs_vfe" },
	/* sn12m0pz regulators */
	{ .supply = "gp6",  .min_uV = 3050000, .max_uV = 3100000 },
	{ .supply = "gp16", .min_uV = 1200000, .max_uV = 1200000 },
};

static int reg_count;

static void msm_camera_vreg_enable(struct platform_device *pdev)
{
	int count, rc;

	struct device *dev = &pdev->dev;

	/* Use gp6 and gp16 if and only if dev name matches. */
	if (!strncmp(pdev->name, "msm_camera_sn12m0pz", 20))
		count = ARRAY_SIZE(regs);
	else
		count = ARRAY_SIZE(regs) - 2;

	rc = regulator_bulk_get(dev, count, regs);

	if (rc) {
		dev_err(dev, "%s: could not get regulators: %d\n",
				__func__, rc);
		return;
	}

	rc = regulator_bulk_set_voltage(count, regs);

	if (rc) {
		dev_err(dev, "%s: could not set voltages: %d\n",
				__func__, rc);
		goto reg_free;
	}

	rc = regulator_bulk_enable(count, regs);

	if (rc) {
		dev_err(dev, "%s: could not enable regulators: %d\n",
				__func__, rc);
		goto reg_free;
	}

	reg_count = count;
	return;

reg_free:
	regulator_bulk_free(count, regs);
	return;
}


static void msm_camera_vreg_disable(void)
{
	regulator_bulk_disable(reg_count, regs);
	regulator_bulk_free(reg_count, regs);
	reg_count = 0;
}

int msm_camio_clk_enable(enum msm_camio_clk_type clktype)
{
	int rc = 0;
	struct clk *clk = NULL;

	switch (clktype) {
	case CAMIO_VFE_MDC_CLK:
		camio_vfe_mdc_clk =
		clk = clk_get(NULL, "vfe_mdc_clk");
		break;

	case CAMIO_MDC_CLK:
		camio_mdc_clk =
		clk = clk_get(NULL, "mdc_clk");
		break;

	case CAMIO_VFE_CLK:
		camio_vfe_clk =
		clk = clk_get(NULL, "vfe_clk");
		msm_camio_clk_rate_set_2(clk, camio_clk.vfe_clk_rate);
		break;

	case CAMIO_VFE_CAMIF_CLK:
		camio_vfe_camif_clk =
		clk = clk_get(NULL, "vfe_camif_clk");
		break;

	case CAMIO_VFE_PBDG_CLK:
		camio_vfe_pbdg_clk =
		clk = clk_get(NULL, "vfe_pclk");
		break;

	case CAMIO_CAM_MCLK_CLK:
		camio_cam_m_clk =
		clk = clk_get(NULL, "cam_m_clk");
		msm_camio_clk_rate_set_2(clk, camio_clk.mclk_clk_rate);
		break;

	case CAMIO_CAMIF_PAD_PBDG_CLK:
		camio_camif_pad_pbdg_clk =
		clk = clk_get(NULL, "camif_pad_pclk");
		break;

	case CAMIO_CSI0_CLK:
		camio_csi_clk =
		clk = clk_get(NULL, "csi_clk");
		msm_camio_clk_rate_set_2(clk, 153600000);
		break;
	case CAMIO_CSI0_VFE_CLK:
		camio_csi_vfe_clk =
		clk = clk_get(NULL, "csi_vfe_clk");
		break;
	case CAMIO_CSI0_PCLK:
		camio_csi_pclk =
		clk = clk_get(NULL, "csi_pclk");
		break;

	case CAMIO_VPE_CLK:
		camio_vpe_clk =
		clk = clk_get(NULL, "vpe_clk");
		vpe_clk_rate = clk_round_rate(clk, vpe_clk_rate);
		clk_set_rate(clk, vpe_clk_rate);
		break;
	default:
		break;
	}

	if (!IS_ERR(clk))
		clk_prepare_enable(clk);
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

	case CAMIO_VFE_CAMIF_CLK:
		clk = camio_vfe_camif_clk;
		break;

	case CAMIO_VFE_PBDG_CLK:
		clk = camio_vfe_pbdg_clk;
		break;

	case CAMIO_CAM_MCLK_CLK:
		clk = camio_cam_m_clk;
		break;

	case CAMIO_CAMIF_PAD_PBDG_CLK:
		clk = camio_camif_pad_pbdg_clk;
		break;
	case CAMIO_CSI0_CLK:
		clk = camio_csi_clk;
		break;
	case CAMIO_CSI0_VFE_CLK:
		clk = camio_csi_vfe_clk;
		break;
	case CAMIO_CSI0_PCLK:
		clk = camio_csi_pclk;
		break;
	case CAMIO_VPE_CLK:
		clk = camio_vpe_clk;
		break;
	default:
		break;
	}

	if (!IS_ERR(clk)) {
		clk_disable_unprepare(clk);
		clk_put(clk);
	} else {
		rc = -1;
	}

	return rc;
}

void msm_camio_clk_rate_set(int rate)
{
	struct clk *clk = camio_cam_m_clk;
	clk_set_rate(clk, rate);
}

int msm_camio_vfe_clk_rate_set(int rate)
{
	struct clk *clk = camio_vfe_clk;
	return clk_set_rate(clk, rate);
}

void msm_camio_clk_rate_set_2(struct clk *clk, int rate)
{
	clk_set_rate(clk, rate);
}

static irqreturn_t msm_io_csi_irq(int irq_num, void *data)
{
	uint32_t irq;
	irq = msm_camera_io_r(csibase + MIPI_INTERRUPT_STATUS);
	CDBG("%s MIPI_INTERRUPT_STATUS = 0x%x\n", __func__, irq);
	msm_camera_io_w(irq, csibase + MIPI_INTERRUPT_STATUS);
	return IRQ_HANDLED;
}

int msm_camio_vpe_clk_disable(void)
{
	msm_camio_clk_disable(CAMIO_VPE_CLK);

	if (fs_vpe) {
		regulator_disable(fs_vpe);
		regulator_put(fs_vpe);
	}

	return 0;
}

int msm_camio_vpe_clk_enable(uint32_t clk_rate)
{
	fs_vpe = regulator_get(NULL, "fs_vpe");
	if (IS_ERR(fs_vpe)) {
		pr_err("%s: Regulator FS_VPE get failed %ld\n", __func__,
			PTR_ERR(fs_vpe));
		fs_vpe = NULL;
	} else if (regulator_enable(fs_vpe)) {
		pr_err("%s: Regulator FS_VPE enable failed\n", __func__);
		regulator_put(fs_vpe);
	}

	vpe_clk_rate = clk_rate;
	msm_camio_clk_enable(CAMIO_VPE_CLK);
	return 0;
}

int msm_camio_enable(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	msm_camio_clk_enable(CAMIO_VFE_PBDG_CLK);
	if (!sinfo->csi_if)
		msm_camio_clk_enable(CAMIO_VFE_CAMIF_CLK);
	else {
		msm_camio_clk_enable(CAMIO_VFE_CLK);
		csiio = request_mem_region(camio_ext.csiphy,
			camio_ext.csisz, pdev->name);
		if (!csiio) {
			rc = -EBUSY;
			goto common_fail;
		}
		csibase = ioremap(camio_ext.csiphy,
			camio_ext.csisz);
		if (!csibase) {
			rc = -ENOMEM;
			goto csi_busy;
		}
		rc = request_irq(camio_ext.csiirq, msm_io_csi_irq,
			IRQF_TRIGGER_RISING, "csi", 0);
		if (rc < 0)
			goto csi_irq_fail;
		/* enable required clocks for CSI */
		msm_camio_clk_enable(CAMIO_CSI0_PCLK);
		msm_camio_clk_enable(CAMIO_CSI0_VFE_CLK);
		msm_camio_clk_enable(CAMIO_CSI0_CLK);
	}
	return 0;
csi_irq_fail:
	iounmap(csibase);
csi_busy:
	release_mem_region(camio_ext.csiphy, camio_ext.csisz);
common_fail:
	msm_camio_clk_disable(CAMIO_VFE_PBDG_CLK);
	msm_camio_clk_disable(CAMIO_VFE_CLK);
	return rc;
}

static void msm_camio_csi_disable(void)
{
	uint32_t val;
	val = 0x0;
	CDBG("%s MIPI_PHY_D0_CONTROL2 val=0x%x\n", __func__, val);
	msm_camera_io_w(val, csibase + MIPI_PHY_D0_CONTROL2);
	msm_camera_io_w(val, csibase + MIPI_PHY_D1_CONTROL2);
	msm_camera_io_w(val, csibase + MIPI_PHY_D2_CONTROL2);
	msm_camera_io_w(val, csibase + MIPI_PHY_D3_CONTROL2);

	CDBG("%s MIPI_PHY_CL_CONTROL val=0x%x\n", __func__, val);
	msm_camera_io_w(val, csibase + MIPI_PHY_CL_CONTROL);
	usleep_range(9000, 10000);
	free_irq(camio_ext.csiirq, 0);
	iounmap(csibase);
	release_mem_region(camio_ext.csiphy, camio_ext.csisz);
}

void msm_camio_disable(struct platform_device *pdev)
{
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	if (!sinfo->csi_if) {
		msm_camio_clk_disable(CAMIO_VFE_CAMIF_CLK);
	} else {
		CDBG("disable mipi\n");
		msm_camio_csi_disable();
		CDBG("disable clocks\n");
		msm_camio_clk_disable(CAMIO_CSI0_PCLK);
		msm_camio_clk_disable(CAMIO_CSI0_VFE_CLK);
		msm_camio_clk_disable(CAMIO_CSI0_CLK);
		msm_camio_clk_disable(CAMIO_VFE_CLK);
	}
	msm_camio_clk_disable(CAMIO_VFE_PBDG_CLK);
}

void msm_camio_camif_pad_reg_reset(void)
{
	uint32_t reg;

	msm_camio_clk_sel(MSM_CAMIO_CLK_SRC_INTERNAL);
	usleep_range(10000, 11000);

	reg = (msm_camera_io_r(camifpadbase)) & CAMIF_CFG_RMSK;
	reg |= 0x3;
	msm_camera_io_w(reg, camifpadbase);
	usleep_range(10000, 11000);

	reg = (msm_camera_io_r(camifpadbase)) & CAMIF_CFG_RMSK;
	reg |= 0x10;
	msm_camera_io_w(reg, camifpadbase);
	usleep_range(10000, 11000);

	reg = (msm_camera_io_r(camifpadbase)) & CAMIF_CFG_RMSK;
	/* Need to be uninverted*/
	reg &= 0x03;
	msm_camera_io_w(reg, camifpadbase);
	usleep_range(10000, 11000);
}

void msm_camio_vfe_blk_reset(void)
{
	return;


}

void msm_camio_camif_pad_reg_reset_2(void)
{
	uint32_t reg;
	uint32_t mask, value;
	reg = (msm_camera_io_r(camifpadbase)) & CAMIF_CFG_RMSK;
	mask = CAM_PAD_REG_SW_RESET_BMSK;
	value = 1 << CAM_PAD_REG_SW_RESET_SHFT;
	msm_camera_io_w((reg & (~mask)) | (value & mask), camifpadbase);
	usleep_range(10000, 11000);
	reg = (msm_camera_io_r(camifpadbase)) & CAMIF_CFG_RMSK;
	mask = CAM_PAD_REG_SW_RESET_BMSK;
	value = 0 << CAM_PAD_REG_SW_RESET_SHFT;
	msm_camera_io_w((reg & (~mask)) | (value & mask), camifpadbase);
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
int msm_camio_probe_on(struct platform_device *pdev)
{
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	camio_clk = camdev->ioclk;
	camio_ext = camdev->ioext;
	camdev->camera_gpio_on();
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

int msm_camio_sensor_clk_on(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	camio_clk = camdev->ioclk;
	camio_ext = camdev->ioext;
	camdev->camera_gpio_on();
	msm_camera_vreg_enable(pdev);
	msm_camio_clk_enable(CAMIO_CAM_MCLK_CLK);
	msm_camio_clk_enable(CAMIO_CAMIF_PAD_PBDG_CLK);
	if (!sinfo->csi_if) {
		camifpadio = request_mem_region(camio_ext.camifpadphy,
			camio_ext.camifpadsz, pdev->name);
		msm_camio_clk_enable(CAMIO_VFE_CLK);
		if (!camifpadio) {
			rc = -EBUSY;
			goto common_fail;
		}
		camifpadbase = ioremap(camio_ext.camifpadphy,
			camio_ext.camifpadsz);
		if (!camifpadbase) {
			CDBG("msm_camio_sensor_clk_on fail\n");
			rc = -ENOMEM;
			goto parallel_busy;
		}
	}
	return rc;
parallel_busy:
	release_mem_region(camio_ext.camifpadphy, camio_ext.camifpadsz);
	goto common_fail;
common_fail:
	msm_camio_clk_disable(CAMIO_CAM_MCLK_CLK);
	msm_camio_clk_disable(CAMIO_VFE_CLK);
	msm_camio_clk_disable(CAMIO_CAMIF_PAD_PBDG_CLK);
	msm_camera_vreg_disable();
	camdev->camera_gpio_off();
	return rc;
}

int msm_camio_sensor_clk_off(struct platform_device *pdev)
{
	uint32_t rc = 0;
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	camdev->camera_gpio_off();
	msm_camera_vreg_disable();
	rc = msm_camio_clk_disable(CAMIO_CAM_MCLK_CLK);
	rc = msm_camio_clk_disable(CAMIO_CAMIF_PAD_PBDG_CLK);
	if (!sinfo->csi_if) {
		iounmap(camifpadbase);
		release_mem_region(camio_ext.camifpadphy, camio_ext.camifpadsz);
		rc = msm_camio_clk_disable(CAMIO_VFE_CLK);
	}
	return rc;
}

int msm_camio_csi_config(struct msm_camera_csi_params *csi_params)
{
	int rc = 0;
	uint32_t val = 0;
	int i;

	CDBG("msm_camio_csi_config\n");

	/* SOT_ECC_EN enable error correction for SYNC (data-lane) */
	msm_camera_io_w(0x4, csibase + MIPI_PHY_CONTROL);

	/* SW_RST to the CSI core */
	msm_camera_io_w(MIPI_PROTOCOL_CONTROL_SW_RST_BMSK,
		csibase + MIPI_PROTOCOL_CONTROL);

	/* PROTOCOL CONTROL */
	val = MIPI_PROTOCOL_CONTROL_LONG_PACKET_HEADER_CAPTURE_BMSK |
		MIPI_PROTOCOL_CONTROL_DECODE_ID_BMSK |
		MIPI_PROTOCOL_CONTROL_ECC_EN_BMSK;
	val |= (uint32_t)(csi_params->data_format) <<
		MIPI_PROTOCOL_CONTROL_DATA_FORMAT_SHFT;
	val |= csi_params->dpcm_scheme <<
		MIPI_PROTOCOL_CONTROL_DPCM_SCHEME_SHFT;
	CDBG("%s MIPI_PROTOCOL_CONTROL val=0x%x\n", __func__, val);
	msm_camera_io_w(val, csibase + MIPI_PROTOCOL_CONTROL);

	/* SW CAL EN */
	val = (0x1 << MIPI_CALIBRATION_CONTROL_SWCAL_CAL_EN_SHFT) |
		(0x1 <<
		MIPI_CALIBRATION_CONTROL_SWCAL_STRENGTH_OVERRIDE_EN_SHFT) |
		(0x1 << MIPI_CALIBRATION_CONTROL_CAL_SW_HW_MODE_SHFT) |
		(0x1 << MIPI_CALIBRATION_CONTROL_MANUAL_OVERRIDE_EN_SHFT);
	CDBG("%s MIPI_CALIBRATION_CONTROL val=0x%x\n", __func__, val);
	msm_camera_io_w(val, csibase + MIPI_CALIBRATION_CONTROL);

	/* settle_cnt is very sensitive to speed!
	increase this value to run at higher speeds */
	val = (csi_params->settle_cnt <<
			MIPI_PHY_D0_CONTROL2_SETTLE_COUNT_SHFT) |
		(0x0F << MIPI_PHY_D0_CONTROL2_HS_TERM_IMP_SHFT) |
		(0x1 << MIPI_PHY_D0_CONTROL2_LP_REC_EN_SHFT) |
		(0x1 << MIPI_PHY_D0_CONTROL2_ERR_SOT_HS_EN_SHFT);
	CDBG("%s MIPI_PHY_D0_CONTROL2 val=0x%x\n", __func__, val);
	for (i = 0; i < csi_params->lane_cnt; i++)
		msm_camera_io_w(val, csibase + MIPI_PHY_D0_CONTROL2 + i * 4);

	val = (0x0F << MIPI_PHY_CL_CONTROL_HS_TERM_IMP_SHFT) |
		(0x1 << MIPI_PHY_CL_CONTROL_LP_REC_EN_SHFT);
	CDBG("%s MIPI_PHY_CL_CONTROL val=0x%x\n", __func__, val);
	msm_camera_io_w(val, csibase + MIPI_PHY_CL_CONTROL);

	val = 0 << MIPI_PHY_D0_CONTROL_HS_REC_EQ_SHFT;
	msm_camera_io_w(val, csibase + MIPI_PHY_D0_CONTROL);

	val = (0x1 << MIPI_PHY_D1_CONTROL_MIPI_CLK_PHY_SHUTDOWNB_SHFT) |
		(0x1 << MIPI_PHY_D1_CONTROL_MIPI_DATA_PHY_SHUTDOWNB_SHFT);
	CDBG("%s MIPI_PHY_D1_CONTROL val=0x%x\n", __func__, val);
	msm_camera_io_w(val, csibase + MIPI_PHY_D1_CONTROL);

	msm_camera_io_w(0x00000000, csibase + MIPI_PHY_D2_CONTROL);
	msm_camera_io_w(0x00000000, csibase + MIPI_PHY_D3_CONTROL);

	/* halcyon only supports 1 or 2 lane */
	switch (csi_params->lane_cnt) {
	case 1:
		msm_camera_io_w(csi_params->lane_assign << 8 | 0x4,
			csibase + MIPI_CAMERA_CNTL);
		break;
	case 2:
		msm_camera_io_w(csi_params->lane_assign << 8 | 0x5,
			csibase + MIPI_CAMERA_CNTL);
		break;
	case 3:
		msm_camera_io_w(csi_params->lane_assign << 8 | 0x6,
			csibase + MIPI_CAMERA_CNTL);
		break;
	case 4:
		msm_camera_io_w(csi_params->lane_assign << 8 | 0x7,
			csibase + MIPI_CAMERA_CNTL);
		break;
	}

	/* mask out ID_ERROR[19], DATA_CMM_ERR[11]
	and CLK_CMM_ERR[10] - de-featured */
	msm_camera_io_w(0xFFF7F3FF, csibase + MIPI_INTERRUPT_MASK);
	/*clear IRQ bits*/
	msm_camera_io_w(0xFFF7F3FF, csibase + MIPI_INTERRUPT_STATUS);

	return rc;
}
void msm_camio_set_perf_lvl(enum msm_bus_perf_setting perf_setting)
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

int msm_cam_core_reset(void)
{
	struct clk *clk1;
	int rc = 0;

	clk1 = clk_get(NULL, "csi_vfe_clk");
	if (IS_ERR(clk1)) {
		pr_err("%s: did not get csi_vfe_clk\n", __func__);
		return PTR_ERR(clk1);
	}

	rc = clk_reset(clk1, CLK_RESET_ASSERT);
	if (rc) {
		pr_err("%s:csi_vfe_clk assert failed\n", __func__);
		clk_put(clk1);
		return rc;
	}
	usleep_range(1000, 1200);
	rc = clk_reset(clk1, CLK_RESET_DEASSERT);
	if (rc) {
		pr_err("%s:csi_vfe_clk deassert failed\n", __func__);
		clk_put(clk1);
		return rc;
	}
	clk_put(clk1);

	clk1 = clk_get(NULL, "csi_clk");
	if (IS_ERR(clk1)) {
		pr_err("%s: did not get csi_clk\n", __func__);
		return PTR_ERR(clk1);
	}

	rc = clk_reset(clk1, CLK_RESET_ASSERT);
	if (rc) {
		pr_err("%s:csi_clk assert failed\n", __func__);
		clk_put(clk1);
		return rc;
	}
	usleep_range(1000, 1200);
	rc = clk_reset(clk1, CLK_RESET_DEASSERT);
	if (rc) {
		pr_err("%s:csi_clk deassert failed\n", __func__);
		clk_put(clk1);
		return rc;
	}
	clk_put(clk1);

	clk1 = clk_get(NULL, "csi_pclk");
	if (IS_ERR(clk1)) {
		pr_err("%s: did not get csi_pclk\n", __func__);
		return PTR_ERR(clk1);
	}

	rc = clk_reset(clk1, CLK_RESET_ASSERT);
	if (rc) {
		pr_err("%s:csi_pclk assert failed\n", __func__);
		clk_put(clk1);
		return rc;
	}
	usleep_range(1000, 1200);
	rc = clk_reset(clk1, CLK_RESET_DEASSERT);
	if (rc) {
		pr_err("%s:csi_pclk deassert failed\n", __func__);
		clk_put(clk1);
		return rc;
	}
	clk_put(clk1);

	return rc;
}
