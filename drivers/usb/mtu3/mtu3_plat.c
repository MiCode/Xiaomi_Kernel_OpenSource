// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 */

#include <linux/arm-smccc.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeirq.h>
#include <linux/regmap.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/mfd/syscon.h>

#include "mtu3.h"
#include "mtu3_dr.h"
#include "mtu3_debug.h"

#define PHY_MODE_DPPULLUP_SET 5
#define PHY_MODE_DPPULLUP_CLR 6

#define VS_VOTER_EN_LO 0x0
#define VS_VOTER_EN_LO_SET 0x1
#define VS_VOTER_EN_LO_CLR 0x2

enum ssusb_smc_request {
	SSUSB_SMC_HWRECS_REQUEST = 0,
	SSUSB_SMC_HWRECS_RELEASE,
	SSUSB_SMC_HWRECS_RESUME,
	SSUSB_SMC_HWRECS_SUSPEND,
	SSUSB_SMC_HWRECS_NUM,
};

enum ssusb_hwrscs_vers {
	SSUSB_HWRECS_V1 = 1,
};

static struct ssusb_offload *usb_offload;
static DEFINE_MUTEX(offload_lock);

/* protect vs voter state */
static DEFINE_MUTEX(vsv_mutex);
static unsigned int vsv_use_count;

static void ssusb_hwrscs_req(struct ssusb_mtk *ssusb,
	enum mtu3_power_state state)
{
	struct arm_smccc_res res;
	void __iomem *ibase = ssusb->ippc_base;
	u32 spm_ctrl, value;
	u32 smc_req = -1;
	int ret;


	dev_info(ssusb->dev, "%s state = %d\n", __func__, state);

	spm_ctrl = mtu3_readl(ibase, U3D_SSUSB_SPM_CTRL);

	switch (state) {
	case MTU3_STATE_POWER_OFF:
		spm_ctrl &= ~SSUSB_SPM_REQ_MSK;
		break;
	case MTU3_STATE_POWER_ON:
		spm_ctrl |= SSUSB_SPM_REQ_MSK;
		break;
	case MTU3_STATE_OFFLOAD:
		spm_ctrl &= ~SSUSB_SPM_REQ_MSK;
		spm_ctrl |= (SSUSB_SPM_SRCCLKENA | SSUSB_SPM_INFRE_REQ);
		break;
	case MTU3_STATE_RESUME:
		spm_ctrl |= SSUSB_SPM_REQ_MSK;
		smc_req = SSUSB_SMC_HWRECS_RESUME;
		break;
	case MTU3_STATE_SUSPEND:
		spm_ctrl &= ~SSUSB_SPM_REQ_MSK;
		smc_req = SSUSB_SMC_HWRECS_SUSPEND;
		break;
	default:
		return;
	}

	/* write spm_ctrl */
	mtu3_writel(ibase, U3D_SSUSB_SPM_CTRL, spm_ctrl);

	ret = readl_poll_timeout_atomic(ibase + U3D_SSUSB_SPM_CTRL_ACK,
		value, (spm_ctrl == (value & SSUSB_SPM_REQ_MSK)), 100, 20000);
	if (ret)
		dev_info(ssusb->dev, "%s spm ctrl timeout\n", __func__);

	/* wait 2ms */
	mdelay(2);

	/* send smc request */
	if (smc_req != -1)
		arm_smccc_smc(MTK_SIP_KERNEL_USB_CONTROL,
			smc_req, 0, 0, 0, 0, 0, 0, &res);
}

void ssusb_set_power_state(struct ssusb_mtk *ssusb,
	enum mtu3_power_state state)
{
	if (ssusb->plat_type == PLAT_FPGA || !ssusb->clk_mgr)
		return;

	if (ssusb->hwrscs_vers == SSUSB_HWRECS_V1)
		ssusb_hwrscs_req(ssusb, state);
}

void ssusb_set_txdeemph(struct ssusb_mtk *ssusb)
{
	u32 txdeemph;

	if (!ssusb->gen1_txdeemph)
		return;

	txdeemph = mtu3_readl(ssusb->mac_base, U3D_TXDEEMPH);
	txdeemph &= ~PIPE_TXDEEMPH_MASK;
	txdeemph |= PIPE_TXDEEMPH(0x1);
	mtu3_writel(ssusb->mac_base, U3D_TXDEEMPH, txdeemph);

	txdeemph = mtu3_readl(ssusb->mac_base, U3D_CP5_CP7_TXDEEMPH);
	txdeemph &= ~PIPE_CP5_CP7_TXDEEMPH_MASK;
	txdeemph |= PIPE_CP5_CP7_TXDEEMPH(0x1);
	mtu3_writel(ssusb->mac_base, U3D_CP5_CP7_TXDEEMPH, txdeemph);
}

void ssusb_set_noise_still_tr(struct ssusb_mtk *ssusb)
{
	/* set noise still transfer */
	if (ssusb->noise_still_tr) {
		mtu3_setbits(ssusb->mac_base, U3D_USB_BUS_PERFORMANCE,
			NOISE_STILL_TRANSFER);
		mtu3_setbits(ssusb->ippc_base, U3D_SSUSB_IP_SPARE0,
			SSUSB_SOF_KEEP);
	}
}

void ssusb_vsvoter_set(struct ssusb_mtk *ssusb)
{
	u32 reg, msk, val;
	unsigned int count = vsv_use_count;

	if (ssusb->plat_type == PLAT_FPGA)
		return;

	if (IS_ERR_OR_NULL(ssusb->vsv))
		return;

	mutex_lock(&vsv_mutex);
	if (++vsv_use_count > 1)
		goto out;

	/* write 1 to set and clr, update reg address */
	reg = ssusb->vsv_reg + VS_VOTER_EN_LO_SET;
	msk = ssusb->vsv_mask;
	val = ssusb->vsv_mask;

	regmap_update_bits(ssusb->vsv, reg, msk, val);
out:
	mutex_unlock(&vsv_mutex);

	dev_info(ssusb->dev, "%s count %d to %d\n",
		__func__, count, vsv_use_count);
}

void ssusb_vsvoter_clr(struct ssusb_mtk *ssusb)
{
	u32 reg, msk, val;
	unsigned int count = vsv_use_count;

	if (ssusb->plat_type == PLAT_FPGA)
		return;

	if (IS_ERR_OR_NULL(ssusb->vsv))
		return;

	mutex_lock(&vsv_mutex);
	if (--vsv_use_count > 0)
		goto out;

	/* write 1 to set and clr, update reg address */
	reg = ssusb->vsv_reg + VS_VOTER_EN_LO_CLR;
	msk = ssusb->vsv_mask;
	val = ssusb->vsv_mask;

	regmap_update_bits(ssusb->vsv, reg, msk, val);
out:
	mutex_unlock(&vsv_mutex);

	dev_info(ssusb->dev, "%s count %d to %d\n",
		__func__, count, vsv_use_count);
}

static int ssusb_vsvoter_of_property_parse(struct ssusb_mtk *ssusb,
				struct device_node *dn)
{
	struct of_phandle_args args;
	struct platform_device *pdev;
	int ret;

	/* vs vote function is optional */
	if (!of_property_read_bool(dn, "mediatek,vs-voter"))
		return 0;

	ret = of_parse_phandle_with_fixed_args(dn,
		"mediatek,vs-voter", 3, 0, &args);
	if (ret)
		return ret;

	pdev = of_find_device_by_node(args.np->child);
	if (!pdev)
		return -ENODEV;

	ssusb->vsv = dev_get_regmap(pdev->dev.parent, NULL);
	if (!ssusb->vsv)
		return -ENODEV;

	ssusb->vsv_reg = args.args[0];
	ssusb->vsv_mask = args.args[1];
	ssusb->vsv_vers = args.args[2];
	dev_info(ssusb->dev, "vsv - reg:0x%x, mask:0x%x, version:%d\n",
			ssusb->vsv_reg, ssusb->vsv_mask, ssusb->vsv_vers);

	return PTR_ERR_OR_ZERO(ssusb->vsv);
}

int get_dp_switch_status(struct ssusb_mtk *ssusb)
{
	u32 val;
	int dp_switch_bit;

	if (IS_ERR_OR_NULL(ssusb->dp_switch))
		return 0;

	regmap_read(ssusb->dp_switch, 0, &val);

	if (val & DP_SWITCH_MSK << ssusb->dp_switch_oft)
		dp_switch_bit = 1;
	else
		dp_switch_bit = 0;

	dev_info(ssusb->dev, "%s dp_switch bit is %d\n",
		__func__, dp_switch_bit);

	return dp_switch_bit;
}

static int ssusb_dp_switch_of_property_parse(struct ssusb_mtk *ssusb,
			struct device_node *dn)
{
	struct of_phandle_args args;
	struct platform_device *pdev;
	int ret;

	/* dp switch is optional */
	if (!of_property_read_bool(dn, "mediatek,uds"))
		return 0;

	ret = of_parse_phandle_with_fixed_args(dn,
		"mediatek,uds", 1, 0, &args);

	if (ret)
		return ret;

	pdev = of_find_device_by_node(args.np);
	if (!pdev)
		return -ENODEV;

	ssusb->dp_switch = device_node_to_regmap(args.np);

	if (!ssusb->dp_switch)
		return -ENODEV;

	ssusb->dp_switch_oft = args.args[0];

	dev_info(ssusb->dev, "dp switch - oft:%d\n",
			ssusb->dp_switch_oft);

	return PTR_ERR_OR_ZERO(ssusb->dp_switch);
}

static int ssusb_offload_get_mode(void)
{
	if (usb_offload && usb_offload->get_mode)
		return usb_offload->get_mode(usb_offload->dev);
	else
		return SSUSB_OFFLOAD_MODE_NONE;
}

int ssusb_offload_register(struct ssusb_offload *offload)
{
	int ret = 0;

	mutex_lock(&offload_lock);

	if (IS_ERR_OR_NULL(offload) || IS_ERR_OR_NULL(offload->dev)) {
		ret = -EINVAL;
		goto out;
	}

	if (usb_offload) {
		ret = -EEXIST;
		goto out;
	}

	usb_offload = kzalloc(sizeof(*usb_offload), GFP_KERNEL);
	if (!usb_offload) {
		ret = -ENOMEM;
		goto out;
	}

	usb_offload->dev = offload->dev;
	usb_offload->get_mode = offload->get_mode;
out:
	mutex_unlock(&offload_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(ssusb_offload_register);

int ssusb_offload_unregister(struct device *dev)
{
	int ret = 0;

	mutex_lock(&offload_lock);

	if (usb_offload->dev != dev) {
		ret = -EINVAL;
		goto out;
	}

	kfree(usb_offload);
	usb_offload = NULL;
out:
	mutex_unlock(&offload_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(ssusb_offload_unregister);

void ssusb_set_force_vbus(struct ssusb_mtk *ssusb, bool vbus_on)
{
	u32 u2ctl;
	u32 misc;

	if (!ssusb->force_vbus)
		return;

	u2ctl = mtu3_readl(ssusb->ippc_base, SSUSB_U2_CTRL(0));
	misc = mtu3_readl(ssusb->mac_base, U3D_MISC_CTRL);
	if (vbus_on) {
		u2ctl &= ~SSUSB_U2_PORT_OTG_SEL;
		misc |= VBUS_FRC_EN | VBUS_ON;
	} else {
		u2ctl |= SSUSB_U2_PORT_OTG_SEL;
		misc &= ~(VBUS_FRC_EN | VBUS_ON);
	}
	mtu3_writel(ssusb->ippc_base, SSUSB_U2_CTRL(0), u2ctl);
	mtu3_writel(ssusb->mac_base, U3D_MISC_CTRL, misc);
}

/* u2-port0 should be powered on and enabled; */
int ssusb_check_clocks(struct ssusb_mtk *ssusb, u32 ex_clks)
{
	void __iomem *ibase = ssusb->ippc_base;
	u32 value, check_val;
	int ret;

	check_val = ex_clks | SSUSB_SYS125_RST_B_STS | SSUSB_SYSPLL_STABLE |
			SSUSB_REF_RST_B_STS;

	ret = readl_poll_timeout(ibase + U3D_SSUSB_IP_PW_STS1, value,
			(check_val == (value & check_val)), 100, 20000);
	if (ret) {
		dev_err(ssusb->dev, "clks of sts1 are not stable!\n");
		return ret;
	}

	ret = readl_poll_timeout(ibase + U3D_SSUSB_IP_PW_STS2, value,
			(value & SSUSB_U2_MAC_SYS_RST_B_STS), 100, 10000);
	if (ret) {
		dev_err(ssusb->dev, "mac2 clock is not stable\n");
		return ret;
	}

	return 0;
}

static int wait_for_ip_sleep(struct ssusb_mtk *ssusb)
{
	bool sleep_check = true;
	u32 value;
	int ret;

	if (!ssusb->is_host)
		sleep_check = ssusb_gadget_ip_sleep_check(ssusb);

	if (!sleep_check)
		return 0;

	/* wait for ip enter sleep mode */
	ret = readl_poll_timeout(ssusb->ippc_base + U3D_SSUSB_IP_PW_STS1, value,
				 (value & SSUSB_IP_SLEEP_STS), 100, 100000);
	if (ret) {
		dev_err(ssusb->dev, "ip sleep failed!!!\n");
		ret = -EBUSY;
	} else {
		/* workaround: avoid wrong wakeup signal latch for some soc */
		usleep_range(100, 200);
	}

	return ret;
}

static void ssusb_dp_pullup_work(struct work_struct *w)
{
	struct ssusb_mtk *ssusb = container_of(w, struct ssusb_mtk, dp_work);

	phy_set_mode_ext(ssusb->phys[0], PHY_MODE_USB_DEVICE,
		PHY_MODE_DPPULLUP_SET);
	mdelay(50);
	phy_set_mode_ext(ssusb->phys[0], PHY_MODE_USB_DEVICE,
		PHY_MODE_DPPULLUP_CLR);
}

void ssusb_phy_dp_pullup(struct ssusb_mtk *ssusb)
{
	dev_info(ssusb->dev, "d+ pull high\n");
	queue_work(system_power_efficient_wq, &ssusb->dp_work);
}

static int ssusb_phy_init(struct ssusb_mtk *ssusb)
{
	int i;
	int ret;

	for (i = 0; i < ssusb->num_phys; i++) {
		ret = phy_init(ssusb->phys[i]);
		if (ret)
			goto exit_phy;
	}
	return 0;

exit_phy:
	for (; i > 0; i--)
		phy_exit(ssusb->phys[i - 1]);

	return ret;
}

static int ssusb_phy_exit(struct ssusb_mtk *ssusb)
{
	int i;

	for (i = 0; i < ssusb->num_phys; i++)
		phy_exit(ssusb->phys[i]);

	return 0;
}

int ssusb_phy_power_on(struct ssusb_mtk *ssusb)
{
	int i;
	int ret;

	for (i = 0; i < ssusb->num_phys; i++) {
		ret = phy_power_on(ssusb->phys[i]);
		if (ret)
			goto power_off_phy;
	}
	return 0;

power_off_phy:
	for (; i > 0; i--)
		phy_power_off(ssusb->phys[i - 1]);

	return ret;
}

void ssusb_phy_power_off(struct ssusb_mtk *ssusb)
{
	unsigned int i;

	for (i = 0; i < ssusb->num_phys; i++)
		phy_power_off(ssusb->phys[i]);
}

void ssusb_phy_set_mode(struct ssusb_mtk *ssusb, enum phy_mode mode)
{
	unsigned int i;

	for (i = 0; i < ssusb->num_phys; i++)
		phy_set_mode(ssusb->phys[i], mode);
}

int ssusb_clks_enable(struct ssusb_mtk *ssusb)
{
	int ret;

	ret = clk_bulk_prepare_enable(BULK_CLKS_CNT, ssusb->clks);
	if (ret)
		dev_err(ssusb->dev, "failed to enable clk\n");

	return ret;
}

void ssusb_clks_disable(struct ssusb_mtk *ssusb)
{
	clk_bulk_disable_unprepare(BULK_CLKS_CNT, ssusb->clks);
}

static int ssusb_rscs_init(struct ssusb_mtk *ssusb)
{
	int ret = 0;

	if (ssusb->plat_type == PLAT_FPGA)
		goto phy_init;

	ret = regulator_enable(ssusb->vusb33);
	if (ret) {
		dev_err(ssusb->dev, "failed to enable vusb33\n");
		goto vusb33_err;
	}

	ssusb_vsvoter_set(ssusb);

	ret = clk_bulk_prepare_enable(BULK_CLKS_CNT, ssusb->clks);
	if (ret)
		goto clks_err;

phy_init:
	ret = ssusb_phy_init(ssusb);
	if (ret) {
		dev_err(ssusb->dev, "failed to init phy\n");
		goto phy_init_err;
	}

	ret = ssusb_phy_power_on(ssusb);
	if (ret) {
		dev_err(ssusb->dev, "failed to power on phy\n");
		goto phy_err;
	}

	return 0;

phy_err:
	ssusb_phy_exit(ssusb);
phy_init_err:
	if (ssusb->plat_type == PLAT_FPGA)
		return ret;

	clk_bulk_disable_unprepare(BULK_CLKS_CNT, ssusb->clks);
clks_err:
	ssusb_vsvoter_clr(ssusb);
	regulator_disable(ssusb->vusb33);
vusb33_err:
	return ret;
}

static void ssusb_rscs_exit(struct ssusb_mtk *ssusb)
{

	clk_bulk_disable_unprepare(BULK_CLKS_CNT, ssusb->clks);
	regulator_disable(ssusb->vusb33);
	ssusb_phy_power_off(ssusb);
	ssusb_phy_exit(ssusb);
	ssusb_vsvoter_clr(ssusb);
}

void ssusb_ip_sw_reset(struct ssusb_mtk *ssusb)
{
	/* reset whole ip (xhci & u3d) */
	mtu3_setbits(ssusb->ippc_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
	udelay(1);
	mtu3_clrbits(ssusb->ippc_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);

	/*
	 * device ip may be powered on in firmware/BROM stage before entering
	 * kernel stage;
	 * power down device ip, otherwise ip-sleep will fail when working as
	 * host only mode
	 */
	if (ssusb->dr_mode == USB_DR_MODE_HOST)
		mtu3_setbits(ssusb->ippc_base, U3D_SSUSB_IP_PW_CTRL2,
				SSUSB_IP_DEV_PDN);
}

static int get_ssusb_rscs(struct platform_device *pdev, struct ssusb_mtk *ssusb)
{
	struct device_node *node = pdev->dev.of_node;
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;
	struct clk_bulk_data *clks = ssusb->clks;
	struct device *dev = &pdev->dev;
	int i;
	int ret;

	ret = of_property_read_u32(node, "plat_type", &ssusb->plat_type);
	if (!ret && ssusb->plat_type == PLAT_FPGA) {
		dev_info(ssusb->dev, "platform is fpga\n");

		of_property_read_u32(node, "fpga_phy",
				&ssusb->fpga_phy);

		dev_info(ssusb->dev, "fpga phy is %d\n", ssusb->fpga_phy);
		goto get_phy;
	}

	ssusb->vusb33 = devm_regulator_get(dev, "vusb33");
	if (IS_ERR(ssusb->vusb33)) {
		dev_err(dev, "failed to get vusb33\n");
		return PTR_ERR(ssusb->vusb33);
	}

	clks[0].id = "sys_ck";
	clks[1].id = "ref_ck";
	clks[2].id = "mcu_ck";
	clks[3].id = "dma_ck";
	clks[4].id = "host_ck";
	clks[5].id = "frmcnt_ck";
	ret = devm_clk_bulk_get_optional(dev, BULK_CLKS_CNT, clks);
	if (ret)
		return ret;

get_phy:
	ssusb->num_phys = of_count_phandle_with_args(node,
			"phys", "#phy-cells");
	if (ssusb->num_phys > 0) {
		ssusb->phys = devm_kcalloc(dev, ssusb->num_phys,
					sizeof(*ssusb->phys), GFP_KERNEL);
		if (!ssusb->phys)
			return -ENOMEM;
	} else {
		ssusb->num_phys = 0;
	}

	for (i = 0; i < ssusb->num_phys; i++) {
		ssusb->phys[i] = devm_of_phy_get_by_index(dev, node, i);
		if (IS_ERR(ssusb->phys[i])) {
			dev_err(dev, "failed to get phy-%d\n", i);
			return PTR_ERR(ssusb->phys[i]);
		}
	}

	ssusb->ippc_base = devm_platform_ioremap_resource_byname(pdev, "ippc");
	if (IS_ERR(ssusb->ippc_base))
		return PTR_ERR(ssusb->ippc_base);

	ssusb->force_vbus = of_property_read_bool(node, "mediatek,force-vbus");
	ssusb->clk_mgr = of_property_read_bool(node, "mediatek,clk-mgr");
	ssusb->noise_still_tr =
		of_property_read_bool(node, "mediatek,noise-still-tr");
	ssusb->gen1_txdeemph =
		of_property_read_bool(node, "mediatek,gen1-txdeemph");
	if (of_property_read_u32(node, "mediatek,hwrscs-vers",
			     &ssusb->hwrscs_vers)) {
		/* compatible to devie tree setting */
		if (of_property_read_bool(node, "mediatek,hw-req-ctrl"))
			ssusb->hwrscs_vers = SSUSB_HWRECS_V1;
	}

	ret = ssusb_vsvoter_of_property_parse(ssusb, node);
	if (ret)
		dev_info(dev, "failed to parse vsv property\n");

	ret = ssusb_dp_switch_of_property_parse(ssusb, node);
	if (ret)
		dev_info(dev, "failed to parse dp_switch property\n");

	ssusb->dr_mode = usb_get_dr_mode(dev);
	if (ssusb->dr_mode == USB_DR_MODE_UNKNOWN)
		ssusb->dr_mode = USB_DR_MODE_OTG;

	if (ssusb->dr_mode == USB_DR_MODE_PERIPHERAL)
		goto out;

	/* if host role is supported */
	ret = ssusb_wakeup_of_property_parse(ssusb, node);
	if (ret) {
		dev_err(dev, "failed to parse uwk property\n");
		return ret;
	}

	/* optional property, ignore the error if it does not exist */
	of_property_read_u32(node, "mediatek,u3p-dis-msk",
			     &ssusb->u3p_dis_msk);
	of_property_read_u32(node, "mediatek,u2p-dis-msk",
			     &ssusb->u2p_dis_msk);

	otg_sx->vbus = devm_regulator_get(dev, "vbus");
	if (IS_ERR(otg_sx->vbus)) {
		dev_err(dev, "failed to get vbus\n");
		return PTR_ERR(otg_sx->vbus);
	}

	if (ssusb->dr_mode == USB_DR_MODE_HOST)
		goto out;

	/* if dual-role mode is supported */
	otg_sx->is_u3_drd = of_property_read_bool(node, "mediatek,usb3-drd");
	otg_sx->manual_drd_enabled =
		of_property_read_bool(node, "enable-manual-drd");
	otg_sx->role_sw_used = of_property_read_bool(node, "usb-role-switch");

	/* can't disable port0 when use dual-role mode */
	ssusb->u2p_dis_msk &= ~0x1;

	if (otg_sx->role_sw_used || otg_sx->manual_drd_enabled)
		goto out;

	if (of_property_read_bool(node, "extcon")) {
		otg_sx->edev = extcon_get_edev_by_phandle(ssusb->dev, 0);
		if (IS_ERR(otg_sx->edev)) {
			return dev_err_probe(dev, PTR_ERR(otg_sx->edev),
					     "couldn't get extcon device\n");
		}
	}

out:
	dev_info(dev, "dr_mode: %d, is_u3_dr: %d, drd: %s\n",
		 ssusb->dr_mode, otg_sx->is_u3_drd,
		otg_sx->manual_drd_enabled ? "manual" : "auto");
	dev_info(dev, "u2p_dis_msk: %x, u3p_dis_msk: %x\n",
		 ssusb->u2p_dis_msk, ssusb->u3p_dis_msk);

	return 0;
}

static int mtu3_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct ssusb_mtk *ssusb;
	int ret = -ENOMEM;

	/* all elements are set to ZERO as default value */
	ssusb = devm_kzalloc(dev, sizeof(*ssusb), GFP_KERNEL);
	if (!ssusb)
		return -ENOMEM;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(dev, "No suitable DMA config available\n");
		return -ENOTSUPP;
	}

	platform_set_drvdata(pdev, ssusb);
	ssusb->dev = dev;

	ret = get_ssusb_rscs(pdev, ssusb);
	if (ret)
		return ret;

	ssusb_debugfs_create_root(ssusb);

	/* enable power domain */
	pm_runtime_set_active(dev);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_autosuspend_delay(dev, 4000);
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	ret = ssusb_rscs_init(ssusb);
	if (ret)
		goto comm_init_err;

	if (IS_ENABLED(CONFIG_USB_MTU3_HOST))
		ssusb->dr_mode = USB_DR_MODE_HOST;
	else if (IS_ENABLED(CONFIG_USB_MTU3_GADGET))
		ssusb->dr_mode = USB_DR_MODE_PERIPHERAL;

	ssusb_ip_sw_reset(ssusb);

	ssusb_set_power_state(ssusb, MTU3_STATE_POWER_ON);

	/* default as host */
	ssusb->is_host = !(ssusb->dr_mode == USB_DR_MODE_PERIPHERAL);

	switch (ssusb->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		ret = ssusb_gadget_init(ssusb);
		if (ret) {
			dev_err(dev, "failed to initialize gadget\n");
			goto comm_exit;
		}
		break;
	case USB_DR_MODE_HOST:
		ret = ssusb_host_init(ssusb, node);
		if (ret) {
			dev_err(dev, "failed to initialize host\n");
			goto comm_exit;
		}
		break;
	case USB_DR_MODE_OTG:
		ret = ssusb_gadget_init(ssusb);
		if (ret) {
			dev_err(dev, "failed to initialize gadget\n");
			goto comm_exit;
		}

		ret = ssusb_host_init(ssusb, node);
		if (ret) {
			dev_err(dev, "failed to initialize host\n");
			goto gadget_exit;
		}

		ret = ssusb_otg_switch_init(ssusb);
		if (ret) {
			dev_err(dev, "failed to initialize switch\n");
			goto host_exit;
		}
		break;
	default:
		dev_err(dev, "unsupported mode: %d\n", ssusb->dr_mode);
		ret = -EINVAL;
		goto comm_exit;
	}

	INIT_WORK(&ssusb->dp_work, ssusb_dp_pullup_work);

	device_enable_async_suspend(dev);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);
	pm_runtime_forbid(dev);

	return 0;

host_exit:
	ssusb_host_exit(ssusb);
gadget_exit:
	ssusb_gadget_exit(ssusb);
comm_exit:
	ssusb_set_power_state(ssusb, MTU3_STATE_POWER_OFF);
	ssusb_rscs_exit(ssusb);
comm_init_err:
	pm_runtime_put_noidle(dev);
	pm_runtime_disable(dev);
	ssusb_debugfs_remove_root(ssusb);

	return ret;
}

static int mtu3_remove(struct platform_device *pdev)
{
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);

	pm_runtime_get_sync(&pdev->dev);

	ssusb_set_power_state(ssusb, MTU3_STATE_POWER_OFF);

	switch (ssusb->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		ssusb_gadget_exit(ssusb);
		break;
	case USB_DR_MODE_HOST:
		ssusb_host_exit(ssusb);
		break;
	case USB_DR_MODE_OTG:
		ssusb_otg_switch_exit(ssusb);
		ssusb_gadget_exit(ssusb);
		ssusb_host_exit(ssusb);
		break;
	default:
		return -EINVAL;
	}

	ssusb_rscs_exit(ssusb);
	ssusb_debugfs_remove_root(ssusb);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);

	return 0;
}

static void mtu3_shutdown(struct platform_device *pdev)
{
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	dev_info(ssusb->dev, "%s role %d\n", __func__, otg_sx->current_role);

	if (ssusb->clk_mgr && otg_sx->current_role == USB_ROLE_DEVICE)
		mtu3_stop(ssusb->u3d);
}

static int resume_ip_and_ports(struct ssusb_mtk *ssusb, pm_message_t msg)
{
	switch (ssusb->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		ssusb_gadget_resume(ssusb, msg);
		break;
	case USB_DR_MODE_HOST:
		ssusb_host_resume(ssusb, false);
		break;
	case USB_DR_MODE_OTG:
		ssusb_host_resume(ssusb, !ssusb->is_host);
		if (!ssusb->is_host)
			ssusb_gadget_resume(ssusb, msg);

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mtu3_suspend_common(struct device *dev, pm_message_t msg)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	int ret = 0;

	dev_info(ssusb->dev, "%s event %d\n", __func__, msg.event);

	/* workaround for pm runtime*/
	if (ssusb->clk_mgr && !ssusb->is_host)
		return 0;

	ssusb->offload_mode = ssusb_offload_get_mode();

	dev_info(ssusb->dev, "%s offload_mode %d\n", __func__, ssusb->offload_mode);

	if (ssusb->offload_mode == SSUSB_OFFLOAD_MODE_S) {
		ssusb_set_power_state(ssusb, MTU3_STATE_OFFLOAD);
		return 0;
	} else if (ssusb->offload_mode == SSUSB_OFFLOAD_MODE_D) {
		return 0;
	}

	ssusb_set_power_state(ssusb, MTU3_STATE_SUSPEND);

	switch (ssusb->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		ret = ssusb_gadget_suspend(ssusb, msg);
		if (ret)
			goto err;

		break;
	case USB_DR_MODE_HOST:
		ssusb_host_suspend(ssusb);
		break;
	case USB_DR_MODE_OTG:
		if (!ssusb->is_host) {
			ret = ssusb_gadget_suspend(ssusb, msg);
			if (ret)
				goto err;
		}
		ssusb_host_suspend(ssusb);
		break;
	default:
		return -EINVAL;
	}

	ret = wait_for_ip_sleep(ssusb);
	if (ret)
		goto sleep_err;

	ssusb_phy_power_off(ssusb);
	clk_bulk_disable_unprepare(BULK_CLKS_CNT, ssusb->clks);
	ssusb_wakeup_set(ssusb, true);
	return 0;

sleep_err:
	resume_ip_and_ports(ssusb, msg);
err:
	return ret;
}

static int mtu3_resume_common(struct device *dev, pm_message_t msg)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	int ret;

	dev_info(ssusb->dev, "%s event %d\n", __func__, msg.event);

	/* workaround for pm runtime*/
	if (ssusb->clk_mgr && !ssusb->is_host)
		return 0;

	dev_info(ssusb->dev, "%s offload_mode %d\n",
		__func__, ssusb->offload_mode);

	if (ssusb->offload_mode == SSUSB_OFFLOAD_MODE_S) {
		ssusb_set_power_state(ssusb, MTU3_STATE_POWER_ON);
		return 0;
	} else if (ssusb->offload_mode == SSUSB_OFFLOAD_MODE_D) {
		return 0;
	}

	ssusb_wakeup_set(ssusb, false);
	ret = clk_bulk_prepare_enable(BULK_CLKS_CNT, ssusb->clks);
	if (ret)
		goto clks_err;

	ret = ssusb_phy_power_on(ssusb);
	if (ret)
		goto phy_err;

	ret = resume_ip_and_ports(ssusb, msg);

	ssusb_set_power_state(ssusb, MTU3_STATE_RESUME);

	return ret;
phy_err:
	clk_bulk_disable_unprepare(BULK_CLKS_CNT, ssusb->clks);
clks_err:
	return ret;
}

static int __maybe_unused mtu3_suspend(struct device *dev)
{
	return mtu3_suspend_common(dev, PMSG_SUSPEND);
}

static int __maybe_unused mtu3_resume(struct device *dev)
{
	return mtu3_resume_common(dev, PMSG_SUSPEND);
}

static int __maybe_unused mtu3_runtime_suspend(struct device *dev)
{
	if (!device_may_wakeup(dev))
		return 0;

	return mtu3_suspend_common(dev, PMSG_AUTO_SUSPEND);
}

static int __maybe_unused mtu3_runtime_resume(struct device *dev)
{
	if (!device_may_wakeup(dev))
		return 0;

	return mtu3_resume_common(dev, PMSG_AUTO_SUSPEND);
}

static const struct dev_pm_ops mtu3_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtu3_suspend, mtu3_resume)
	SET_RUNTIME_PM_OPS(mtu3_runtime_suspend,
			   mtu3_runtime_resume, NULL)
};

#define DEV_PM_OPS (IS_ENABLED(CONFIG_PM) ? &mtu3_pm_ops : NULL)

static const struct of_device_id mtu3_of_match[] = {
	{.compatible = "mediatek,mt8173-mtu3",},
	{.compatible = "mediatek,mtu3",},
	{},
};
MODULE_DEVICE_TABLE(of, mtu3_of_match);

static struct platform_driver mtu3_driver = {
	.probe = mtu3_probe,
	.remove = mtu3_remove,
	.shutdown = mtu3_shutdown,
	.driver = {
		.name = MTU3_DRIVER_NAME,
		.pm = DEV_PM_OPS,
		.of_match_table = mtu3_of_match,
	},
};
module_platform_driver(mtu3_driver);

MODULE_AUTHOR("Chunfeng Yun <chunfeng.yun@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek USB3 DRD Controller Driver");
