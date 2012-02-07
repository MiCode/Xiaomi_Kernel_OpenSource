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
#define	DBG_CSI	0

static struct clk *camio_cam_clk;
static struct clk *camio_vfe_clk;
static struct clk *camio_csi_src_clk;
static struct clk *camio_csi0_vfe_clk;
static struct clk *camio_csi1_vfe_clk;
static struct clk *camio_csi0_clk;
static struct clk *camio_csi1_clk;
static struct clk *camio_csi0_pclk;
static struct clk *camio_csi1_pclk;
static struct clk *camio_vfe_pclk;
static struct clk *camio_jpeg_clk;
static struct clk *camio_jpeg_pclk;
static struct clk *camio_vpe_clk;
static struct clk *camio_vpe_pclk;
static struct regulator *fs_vfe;
static struct regulator *fs_ijpeg;
static struct regulator *fs_vpe;
static struct regulator *ldo15;
static struct regulator *lvs0;
static struct regulator *ldo25;

static struct msm_camera_io_ext camio_ext;
static struct msm_camera_io_clk camio_clk;
static struct platform_device *camio_dev;
static struct resource *csiio;
void __iomem *csibase;
static int vpe_clk_rate;
struct msm_bus_scale_pdata *cam_bus_scale_table;

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

static void msm_camera_vreg_enable(void)
{
	ldo15 = regulator_get(NULL, "8058_l15");
	if (IS_ERR(ldo15)) {
		pr_err("%s: VREG LDO15 get failed\n", __func__);
		ldo15 = NULL;
		return;
	}
	if (regulator_set_voltage(ldo15, 2850000, 2850000)) {
		pr_err("%s: VREG LDO15 set voltage failed\n",  __func__);
		goto ldo15_disable;
	}
	if (regulator_enable(ldo15)) {
		pr_err("%s: VREG LDO15 enable failed\n", __func__);
		goto ldo15_put;
	}

	lvs0 = regulator_get(NULL, "8058_lvs0");
	if (IS_ERR(lvs0)) {
		pr_err("%s: VREG LVS0 get failed\n", __func__);
		lvs0 = NULL;
		goto ldo15_disable;
	}
	if (regulator_enable(lvs0)) {
		pr_err("%s: VREG LVS0 enable failed\n", __func__);
		goto lvs0_put;
	}

	ldo25 = regulator_get(NULL, "8058_l25");
	if (IS_ERR(ldo25)) {
		pr_err("%s: VREG LDO25 get failed\n", __func__);
		ldo25 = NULL;
		goto lvs0_disable;
	}
	if (regulator_set_voltage(ldo25, 1200000, 1200000)) {
		pr_err("%s: VREG LDO25 set voltage failed\n",  __func__);
		goto ldo25_disable;
	}
	if (regulator_enable(ldo25)) {
		pr_err("%s: VREG LDO25 enable failed\n", __func__);
		goto ldo25_put;
	}

	fs_vfe = regulator_get(NULL, "fs_vfe");
	if (IS_ERR(fs_vfe)) {
		CDBG("%s: Regulator FS_VFE get failed %ld\n", __func__,
			PTR_ERR(fs_vfe));
		fs_vfe = NULL;
	} else if (regulator_enable(fs_vfe)) {
		CDBG("%s: Regulator FS_VFE enable failed\n", __func__);
		regulator_put(fs_vfe);
	}
	return;

ldo25_disable:
	regulator_disable(ldo25);
ldo25_put:
	regulator_put(ldo25);
lvs0_disable:
	regulator_disable(lvs0);
lvs0_put:
	regulator_put(lvs0);
ldo15_disable:
	regulator_disable(ldo15);
ldo15_put:
	regulator_put(ldo15);
}

static void msm_camera_vreg_disable(void)
{
	if (ldo15) {
		regulator_disable(ldo15);
		regulator_put(ldo15);
	}

	if (lvs0) {
		regulator_disable(lvs0);
		regulator_put(lvs0);
	}

	if (ldo25) {
		regulator_disable(ldo25);
		regulator_put(ldo25);
	}

	if (fs_vfe) {
		regulator_disable(fs_vfe);
		regulator_put(fs_vfe);
	}
}

int msm_camio_clk_enable(enum msm_camio_clk_type clktype)
{
	int rc = 0;
	struct clk *clk = NULL;

	switch (clktype) {
	case CAMIO_CAM_MCLK_CLK:
		camio_cam_clk =
		clk = clk_get(NULL, "cam_clk");
		msm_camio_clk_rate_set_2(clk, camio_clk.mclk_clk_rate);
		break;

	case CAMIO_VFE_CLK:
		camio_vfe_clk =
		clk = clk_get(NULL, "vfe_clk");
		msm_camio_clk_rate_set_2(clk, camio_clk.vfe_clk_rate);
		break;

	case CAMIO_CSI0_VFE_CLK:
		camio_csi0_vfe_clk =
		clk = clk_get(NULL, "csi_vfe_clk");
		break;

	case CAMIO_CSI1_VFE_CLK:
		camio_csi1_vfe_clk =
		clk = clk_get(&camio_dev->dev, "csi_vfe_clk");
		break;

	case CAMIO_CSI_SRC_CLK:
		camio_csi_src_clk =
		clk = clk_get(NULL, "csi_src_clk");
		msm_camio_clk_rate_set_2(clk, 384000000);
		break;

	case CAMIO_CSI0_CLK:
		camio_csi0_clk =
		clk = clk_get(NULL, "csi_clk");
		break;

	case CAMIO_CSI1_CLK:
		camio_csi1_clk =
		clk = clk_get(&camio_dev->dev, "csi_clk");
		break;

	case CAMIO_VFE_PCLK:
		camio_vfe_pclk =
		clk = clk_get(NULL, "vfe_pclk");
		break;

	case CAMIO_CSI0_PCLK:
		camio_csi0_pclk =
		clk = clk_get(NULL, "csi_pclk");
		break;

	case CAMIO_CSI1_PCLK:
		camio_csi1_pclk =
		clk = clk_get(&camio_dev->dev, "csi_pclk");
		break;

	case CAMIO_JPEG_CLK:
		camio_jpeg_clk =
		clk = clk_get(NULL, "ijpeg_clk");
		msm_camio_clk_rate_set_2(clk, 228571000);
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
	case CAMIO_CAM_MCLK_CLK:
		clk = camio_cam_clk;
		break;

	case CAMIO_VFE_CLK:
		clk = camio_vfe_clk;
		break;

	case CAMIO_CSI_SRC_CLK:
		clk = camio_csi_src_clk;
		break;

	case CAMIO_CSI0_VFE_CLK:
		clk = camio_csi0_vfe_clk;
		break;

	case CAMIO_CSI1_VFE_CLK:
		clk = camio_csi1_vfe_clk;
		break;

	case CAMIO_CSI0_CLK:
		clk = camio_csi0_clk;
		break;

	case CAMIO_CSI1_CLK:
		clk = camio_csi1_clk;
		break;

	case CAMIO_VFE_PCLK:
		clk = camio_vfe_pclk;
		break;

	case CAMIO_CSI0_PCLK:
		clk = camio_csi0_pclk;
		break;

	case CAMIO_CSI1_PCLK:
		clk = camio_csi1_pclk;
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
		rc = -1;
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

void msm_camio_clk_rate_set(int rate)
{
	struct clk *clk = camio_cam_clk;
	clk_set_rate(clk, rate);
}

void msm_camio_clk_rate_set_2(struct clk *clk, int rate)
{
	clk_set_rate(clk, rate);
}

static irqreturn_t msm_io_csi_irq(int irq_num, void *data)
{
	uint32_t irq = 0;
	if (csibase != NULL)
		irq = msm_io_r(csibase + MIPI_INTERRUPT_STATUS);
	CDBG("%s MIPI_INTERRUPT_STATUS = 0x%x\n", __func__, irq);
	if (csibase != NULL)
		msm_io_w(irq, csibase + MIPI_INTERRUPT_STATUS);
	return IRQ_HANDLED;
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

#ifdef DBG_CSI
static int csi_request_irq(void)
{
	return request_irq(camio_ext.csiirq, msm_io_csi_irq,
		IRQF_TRIGGER_HIGH, "csi", 0);
}
#else
static int csi_request_irq(void) { return 0; }
#endif

#ifdef DBG_CSI
static void csi_free_irq(void)
{
	free_irq(camio_ext.csiirq, 0);
}
#else
static void csi_free_irq(void) { return 0; }
#endif

int msm_camio_enable(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	camio_dev = pdev;
	camio_ext = camdev->ioext;
	camio_clk = camdev->ioclk;
	cam_bus_scale_table = camdev->cam_bus_scale_table;

	msm_camio_clk_enable(CAMIO_VFE_CLK);
	msm_camio_clk_enable(CAMIO_CSI0_VFE_CLK);
	msm_camio_clk_enable(CAMIO_CSI1_VFE_CLK);
	msm_camio_clk_enable(CAMIO_CSI_SRC_CLK);
	msm_camio_clk_enable(CAMIO_CSI0_CLK);
	msm_camio_clk_enable(CAMIO_CSI1_CLK);
	msm_camio_clk_enable(CAMIO_VFE_PCLK);
	msm_camio_clk_enable(CAMIO_CSI0_PCLK);
	msm_camio_clk_enable(CAMIO_CSI1_PCLK);

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
	rc = csi_request_irq();
	if (rc < 0)
		goto csi_irq_fail;

	return 0;

csi_irq_fail:
	iounmap(csibase);
	csibase = NULL;
csi_busy:
	release_mem_region(camio_ext.csiphy, camio_ext.csisz);
	csibase = NULL;
common_fail:
	msm_camio_clk_disable(CAMIO_CAM_MCLK_CLK);
	msm_camio_clk_disable(CAMIO_CSI0_VFE_CLK);
	msm_camio_clk_disable(CAMIO_CSI0_CLK);
	msm_camio_clk_disable(CAMIO_CSI1_VFE_CLK);
	msm_camio_clk_disable(CAMIO_CSI1_CLK);
	msm_camio_clk_disable(CAMIO_VFE_PCLK);
	msm_camio_clk_disable(CAMIO_CSI0_PCLK);
	msm_camio_clk_disable(CAMIO_CSI1_PCLK);
	msm_camera_vreg_disable();
	camdev->camera_gpio_off();
	return rc;
}

static void msm_camio_csi_disable(void)
{
	uint32_t val;

	val = 0x0;
	if (csibase != NULL) {
		CDBG("%s MIPI_PHY_D0_CONTROL2 val=0x%x\n", __func__, val);
		msm_io_w(val, csibase + MIPI_PHY_D0_CONTROL2);
		msm_io_w(val, csibase + MIPI_PHY_D1_CONTROL2);
		msm_io_w(val, csibase + MIPI_PHY_D2_CONTROL2);
		msm_io_w(val, csibase + MIPI_PHY_D3_CONTROL2);
		CDBG("%s MIPI_PHY_CL_CONTROL val=0x%x\n", __func__, val);
		msm_io_w(val, csibase + MIPI_PHY_CL_CONTROL);
		msleep(20);
		val = msm_io_r(csibase + MIPI_PHY_D1_CONTROL);
		val &=
		~((0x1 << MIPI_PHY_D1_CONTROL_MIPI_CLK_PHY_SHUTDOWNB_SHFT)
		|(0x1 << MIPI_PHY_D1_CONTROL_MIPI_DATA_PHY_SHUTDOWNB_SHFT));
		CDBG("%s MIPI_PHY_D1_CONTROL val=0x%x\n", __func__, val);
		msm_io_w(val, csibase + MIPI_PHY_D1_CONTROL);
		usleep_range(5000, 6000);
		msm_io_w(0x0, csibase + MIPI_INTERRUPT_MASK);
		msm_io_w(0x0, csibase + MIPI_INTERRUPT_STATUS);
		csi_free_irq();
		iounmap(csibase);
		csibase = NULL;
		release_mem_region(camio_ext.csiphy, camio_ext.csisz);
	}
}
void msm_camio_disable(struct platform_device *pdev)
{
	CDBG("disable mipi\n");
	msm_camio_csi_disable();
	CDBG("disable clocks\n");
	msm_camio_clk_disable(CAMIO_CSI0_VFE_CLK);
	msm_camio_clk_disable(CAMIO_CSI0_CLK);
	msm_camio_clk_disable(CAMIO_CSI1_VFE_CLK);
	msm_camio_clk_disable(CAMIO_CSI1_CLK);
	msm_camio_clk_disable(CAMIO_VFE_PCLK);
	msm_camio_clk_disable(CAMIO_CSI0_PCLK);
	msm_camio_clk_disable(CAMIO_CSI1_PCLK);
	msm_camio_clk_disable(CAMIO_CSI_SRC_CLK);
	msm_camio_clk_disable(CAMIO_VFE_CLK);
}

int msm_camio_sensor_clk_on(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	camio_dev = pdev;
	camio_ext = camdev->ioext;
	camio_clk = camdev->ioclk;

	msm_camera_vreg_enable();
	msleep(10);
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
	camio_ext = camdev->ioext;
	camio_clk = camdev->ioclk;

	rc = camdev->camera_gpio_on();
	if (rc < 0)
		return rc;
	msm_camera_vreg_enable();
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

int msm_camio_csi_config(struct msm_camera_csi_params *csi_params)
{
	int rc = 0;
	uint32_t val = 0;
	int i;

	CDBG("msm_camio_csi_config\n");
	if (csibase != NULL) {
		/* SOT_ECC_EN enable error correction for SYNC (data-lane) */
		msm_io_w(0x4, csibase + MIPI_PHY_CONTROL);

		/* SW_RST to the CSI core */
		msm_io_w(MIPI_PROTOCOL_CONTROL_SW_RST_BMSK,
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
		msm_io_w(val, csibase + MIPI_PROTOCOL_CONTROL);

		/* settle_cnt is very sensitive to speed!
		increase this value to run at higher speeds */
		val = (csi_params->settle_cnt <<
			MIPI_PHY_D0_CONTROL2_SETTLE_COUNT_SHFT) |
			(0x0F << MIPI_PHY_D0_CONTROL2_HS_TERM_IMP_SHFT) |
			(0x1 << MIPI_PHY_D0_CONTROL2_LP_REC_EN_SHFT) |
			(0x1 << MIPI_PHY_D0_CONTROL2_ERR_SOT_HS_EN_SHFT);
		CDBG("%s MIPI_PHY_D0_CONTROL2 val=0x%x\n", __func__, val);
		for (i = 0; i < csi_params->lane_cnt; i++)
			msm_io_w(val, csibase + MIPI_PHY_D0_CONTROL2 + i * 4);

		val = (0x0F << MIPI_PHY_CL_CONTROL_HS_TERM_IMP_SHFT) |
			(0x1 << MIPI_PHY_CL_CONTROL_LP_REC_EN_SHFT);
		CDBG("%s MIPI_PHY_CL_CONTROL val=0x%x\n", __func__, val);
		msm_io_w(val, csibase + MIPI_PHY_CL_CONTROL);

		val = 0 << MIPI_PHY_D0_CONTROL_HS_REC_EQ_SHFT;
		msm_io_w(val, csibase + MIPI_PHY_D0_CONTROL);

		val =
		(0x1 << MIPI_PHY_D1_CONTROL_MIPI_CLK_PHY_SHUTDOWNB_SHFT)
		|(0x1 << MIPI_PHY_D1_CONTROL_MIPI_DATA_PHY_SHUTDOWNB_SHFT);
		CDBG("%s MIPI_PHY_D1_CONTROL val=0x%x\n", __func__, val);
		msm_io_w(val, csibase + MIPI_PHY_D1_CONTROL);

		msm_io_w(0x00000000, csibase + MIPI_PHY_D2_CONTROL);
		msm_io_w(0x00000000, csibase + MIPI_PHY_D3_CONTROL);

		/* halcyon only supports 1 or 2 lane */
		switch (csi_params->lane_cnt) {
		case 1:
			msm_io_w(csi_params->lane_assign << 8 | 0x4,
				csibase + MIPI_CAMERA_CNTL);
			break;
		case 2:
			msm_io_w(csi_params->lane_assign << 8 | 0x5,
				csibase + MIPI_CAMERA_CNTL);
			break;
		case 3:
			msm_io_w(csi_params->lane_assign << 8 | 0x6,
				csibase + MIPI_CAMERA_CNTL);
			break;
		case 4:
			msm_io_w(csi_params->lane_assign << 8 | 0x7,
				csibase + MIPI_CAMERA_CNTL);
			break;
		}

		/* mask out ID_ERROR[19], DATA_CMM_ERR[11]
		and CLK_CMM_ERR[10] - de-featured */
		msm_io_w(0xF017F3C0, csibase + MIPI_INTERRUPT_MASK);
		/*clear IRQ bits*/
		msm_io_w(0xF017F3C0, csibase + MIPI_INTERRUPT_STATUS);
	} else {
		pr_info("CSIBASE is NULL");
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

int msm_cam_core_reset(void)
{
	struct clk *clk1;
	int rc = 0;
	clk1 = clk_get(&camio_dev->dev, "csi_vfe_clk");
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

	clk1 = clk_get(&camio_dev->dev, "csi_clk");
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

	clk1 = clk_get(&camio_dev->dev, "csi_pclk");
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
